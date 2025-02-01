/*********************************************************************
**
** Dr. Eric R. Nelson
** 1/13/2025
*
*  Task DBBUILD
*
* This task makes TISAN data files for testing purposes. You can select
* from a list of functions and specify the file type (real or complex,
* labeled of series), time interval, number of points and function parameters.
*
* The default outclass is tsn
*
* There is no input file for this task, just an output file.
*
* CODE function selection
*    0 - Sine       : A sine(ω t + φ),                         PARMS = A, ω, φ
*    1 - sin(x)/x   : A sinc(ω t + φ),                         PARMS = A, ω, φ
*    2 - Exponential: A exp(B t) + C,                          PARMS = A, B, C
*    3 - Gaussian   : A exp(-((t - B) * C)^2) + D,             PARMS = A, B, C, D
*    4 - Random     : (maximum - minimum)*rand(0,1] + minimum, PARMS = minimum, maximum
*
* ITYPE
*    0 - Real time series
*    1 - Real Time Labeled
*    2 - Real time series
*    3 - Complex time labeled
*
* FACTOR number of data points in the output file
* TRANGE start/stop time range
*
* If you change the function list be sure the update the following...
*    The CODE Function selection comment above
*    The DBBUILD.TXT files in the SUPPORT subdirectory
*    The documentation for DBBUILD in the help.txt file
*
*/
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>

#include "tisan.h"

void setValue(short type, double y, double *Rval, struct complex *Zval);

void sineFunction(short type, double time, double *Rval, struct complex *Zval);
void sincFunction(short type, double time, double *Rval, struct complex *Zval);
void exponentialFunction(short type, double time, double *Rval, struct complex *Zval);
void gaussianFunction(short type, double time, double *Rval, struct complex *Zval);
void randomFunction(short type, double time, double *Rval, struct complex *Zval);

double ramdomNumber(double rangeMin, double rangeMax);

char tempfileName[_MAX_PATH];

FILE *outfileStream = (FILE *)NIL;

const char szTask[]="DBBUILD";

int main(int argc, char *argv[])
   {
   struct FILEHDR FileHeader;
   char buffer[sizeof(struct TXData)];
   double Rval;
   struct complex Zval;
   short flag=0, type;
   char outfileName[_MAX_PATH];
   double timeVal, timeIndex=0.;
   short ERRFLAG = 0;
   long numberOfPoints, i;
   time_t t;

   signal(SIGINT,BREAKREQ);
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);

   if (!OUTCLASS[0]) strcpy(OUTCLASS,"tsn"); // Set default values
   FileHeader.m = 1.0;
   FileHeader.b = 0.0;

   if ((ITYPE < 0) || (ITYPE > 3))
      {
      zTaskMessage(10,"Invalid Data Type: ITYPE = %d\n",ITYPE);
      BombOff(1);
      }

   numberOfPoints = (long)FACTOR;

   if (numberOfPoints < (long)3)
      {
      zTaskMessage(10,"Invalid Number of Data Points: FACTOR = %g\n",FACTOR);
      BombOff(1);
      }

   if (TRANGE[0] >= TRANGE[1])
      {
      zTaskMessage(10,"Invalid Time Range: TRANGE = %g, %g\n",TRANGE[0], TRANGE[1]);
      BombOff(1);
      }

   switch (CODE)
      {
      case 0:  // sine
         zTaskMessage(2,"Sine Function: %g sin(%g t + %g)\n", PARMS[0], PARMS[1], PARMS[2]);
         break;

      case 1:  // sinc
         zTaskMessage(2,"Sinc Function: %g sinc(%g t + %g)\n", PARMS[0], PARMS[1], PARMS[2]);
         break;

      case 2:  // exponential
         zTaskMessage(2,"Exponential Function: %g exp(%g t) + %g\n", PARMS[0], PARMS[1], PARMS[2]);
         break;

      case 3:  // Gaussian
         zTaskMessage(2,"Gaussian Function: %g exp(((t - %g) * %g)²) + %g\n", PARMS[0], PARMS[1], PARMS[2], PARMS[3]);
         break;

      case 4:  // random
         zTaskMessage(2,"Random Function:(%g - %g)*rand(0,1] + %g\n", PARMS[1], PARMS[0], PARMS[0]);
         time(&t);
         srand((unsigned)t); // Initialize the random number generator
         rand();
         break;

      default: // invalid choice
         zTaskMessage(10,"Invalid Function Selection: CODE = %d\n",CODE);
         BombOff(1);
      }

   strcpy(FileHeader.szTitle,  TITLE);
   strcpy(FileHeader.szTlabel, TLABEL);
   strcpy(FileHeader.szYlabel, YLABEL);

   FileHeader.type = ITYPE;

   FileHeader.m = (TRANGE[1] - TRANGE[0])/FACTOR;
   FileHeader.b = TRANGE[0];

   zBuildFileName(M_outname,outfileName);
   zBuildFileName(M_tmpname,tempfileName);

   zTaskMessage(2,"Opening Scratch File '%s'\n",tempfileName);

   if ((outfileStream = zOpen(tempfileName,O_writeb)) == NULL) BombOff(1);

   if (Zputhead(outfileStream,&FileHeader)) BombOff(1);

   type = (short)ITYPE;

   for (i = 0; i < numberOfPoints; ++i)
      {
      timeVal = FileHeader.b + FileHeader.m * timeIndex;

      switch (CODE)
         {
         case 0:
            sineFunction(type, timeVal, &Rval, &Zval); // sine
            break;
         case 1:
            sincFunction(type, timeVal, &Rval, &Zval); // sinc
            break;
         case 2:
            exponentialFunction(type, timeVal, &Rval, &Zval); // exponential
            break;
         case 3:
            gaussianFunction(type, timeVal, &Rval, &Zval); // Gaussian
            break;
         case 4:
            randomFunction(type, timeVal, &Rval, &Zval); // random
            break;
         }

      insertValues(buffer, &FileHeader, timeVal, Rval, Zval, flag);

      if (Zwrite(outfileStream,buffer,FileHeader.type)) BombOff(1);

      ++timeIndex;           // Must update after all the processing...
      }
   

   zTaskMessage(2,"%ld points written.\n", numberOfPoints);

   fcloseall();

   ERRFLAG = zNameOutputFile(outfileName,tempfileName);

   Zexit(ERRFLAG);
   }

/*
** Sets the values to y for either real or complex data types
*/
void setValue(short type, double y, double *Rval, struct complex *Zval)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = y;
         break;
      case X_Data:
      case TX_Data:
         Zval->x = y;
         Zval->y = y;
         break;
      }
   return;
   }

/*
** Sine : A sine(ω t + φ), PARMS = A, ω, φ
*/
void sineFunction(short type, double timeVal, double *Rval, struct complex *Zval)
   {
   double y;

   y = PARMS[0] * sin(PARMS[1] * timeVal + PARMS[2]);

   setValue(type, y, Rval, Zval);

   return;
   }

/*
** sin(x)/x : A sinc(ω t + φ)
*/
void sincFunction(short type, double timeVal, double *Rval, struct complex *Zval)
   {
   double x, y;

   x = PARMS[1] * timeVal + PARMS[2];

   if (x)
      y = PARMS[0] * sin(x) / x;
   else
      y = PARMS[0];

   setValue(type, y, Rval, Zval);

   return;
   }

/*
** Exponential : A exp(B t) + D, PARMS = A, B, C
*/
void exponentialFunction(short type, double timeVal, double *Rval, struct complex *Zval)
   {
   double y;

   y = PARMS[0] * exp(PARMS[1] * timeVal) + PARMS[2];

   setValue(type, y, Rval, Zval);

   return;
   }

/*
** Gaussian : A exp(-((t - B) * C)^2) + D, PARMS = A, B, C, D
*/
void gaussianFunction(short type, double timeVal, double *Rval, struct complex *Zval)
   {
   double x, y;

   x = (timeVal - PARMS[1]) * PARMS[2];
   y = PARMS[0] * exp(-x*x) + PARMS[3];

   setValue(type, y, Rval, Zval);

   return;
   }

/*
** Random : (maximum - minimum)*rand(0,1] + minimum, PARMS = minimum, maximum
*/
void randomFunction(short type, double timeVal, double *Rval, struct complex *Zval)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = ramdomNumber(PARMS[0], PARMS[1]);
         break;
      case X_Data:
      case TX_Data:
         Zval->x = ramdomNumber(PARMS[0], PARMS[1]);
         Zval->y = ramdomNumber(PARMS[0], PARMS[1]);
         break;
      }
   return;
   }

/*
** Generate random values in the specified range rangeMin <= x <= rangeMax
*/
double ramdomNumber(double rangeMin, double rangeMax)
   {
   double val;

   val = ((double)rand())/(double)RAND_MAX; // val is now between [0 and 1)

   return(val * (rangeMax - rangeMin) + rangeMin);
   }

/***************************************************************
**
** Process ^C Interrupt
*/
void BREAKREQ(int a)
   {
   BombOff(256);
   }

/*
** Trap Floating Point Error
*/
void FPEERROR(int a)
   {
   BombOff(255);
   }

void BombOff(int a)
   {
   fcloseall();
   unlink(tempfileName);
   Zexit(a);
   }

void fcloseall()
   {
   if (outfileStream) Zclose(outfileStream);
   outfileStream = (FILE *)NIL;
   
   return;
   }
