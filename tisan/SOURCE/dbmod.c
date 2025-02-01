/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task DBMOD
*
* Task to modify the values in a data base.
* Time information is not affected.
*
* C does not allow for negative values to be raised to ANY power.
* This deficiency will be compensated for at a later date.
*
* If the file is complex, then ZFACTOR is the complex factor value.
*
*      -1 -> Custom
* CODE  0 -> ADD FACTOR TO DATA
*       1 -> MULTIPLY FACTOR INTO DATA
*       2 -> DIVIDE FACTOR INTO DATA
*       3 -> DIVIDE DATA INTO FACTOR
*       4 -> RAISE DATA TO FACTOR POWER
*       5 -> LOG OF DATA IN BASE FACTOR
*       6 -> ANTI-LOG OF DATA IN BASE FACTOR
*       7 -> Unbiased Round
*       8 -> Subtract Mean
*
*  The infile of this task accepts wild cards
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

BOOL invalidCombination(short type, struct complex Zfactor);

void addFactor(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval);
void multiplyFactor(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval);
void dividebyFactor(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval);
void divideintoFactor(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval);
void raisetoFactorPower(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval);
void logBaseFactor(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval);
void antilogBaseFactor(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval);
void roundData(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval);
void subtractFactor(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval);

void calculateMean(struct FILEHDR *fileHeader, double *factor, struct complex *Zfactor);

void customOperation(short type, double factor, struct complex Zfactor, double time, double *Rval, struct complex *Zval);


double TWOPI;

char *DVZP = "%-8s: Divide By Zero, Returned 0.\n";

char szTemporaryFile[_MAX_PATH];  /* Must be global so BREAKREQ can see it */

FILE *inputFile  = (FILE *)NIL;
FILE *outputFile = (FILE *)NIL;

const char szTask[]="DBMOD";

int main(int argc, char *argv[])
   {
   struct FILEHDR FileHeader;
   double TCNT=0., TIME, Rval;
   struct complex Zval, Zfactor;
   short FLAG=0;
   char szInputFile[_MAX_PATH], szOutputFile[_MAX_PATH];
   char BUFFER[sizeof(struct TXData)];
   struct RData  *RDataPntr;
   struct TRData *TRDataPntr;
   struct XData  *XDataPntr;
   struct TXData *TXDataPntr;
   // declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;
   
   TWOPI = 2.0 * acos(-1.0);

   signal(SIGINT,BREAKREQ);
   signal(SIGFPE,FPEERROR);

   if (zTaskInit(argv[0])) Zexit(1);

   RDataPntr =  (struct RData *)BUFFER;
   TRDataPntr = (struct TRData *)BUFFER;
   XDataPntr =  (struct XData *)BUFFER;
   TXDataPntr = (struct TXData *)BUFFER;

   if ((CODE < -1) || (CODE > 8))       // Change this line for new codes!
      {
      zTaskMessage(10,"CODE Out of Range\n");
      Zexit(1);
      }

   zBuildFileName(M_inname,szInputFile);
   CatList = ZCatFiles(szInputFile);   // Find Matching Files for the input file specification
   if (CatList->N == 0) Zexit(1);     // Quit if there are none

   for (iwc = 0, pWild = CatList->pList;
        (iwc < CatList->N) && !ERRFLAG;
        ++iwc, pWild = strchr(pWild,'\0') + 1)
      {
      splitPath(pWild,Drive,Dir,Fname,Ext);
      strcpy(INNAME,Fname);
      strcpy(INCLASS,Ext);

      zBuildFileName(M_inname,szInputFile);   /* Build file names */
      zBuildFileName(M_outname,szOutputFile);
      zBuildFileName(M_tmpname,szTemporaryFile);

      zTaskMessage(2,"Opening Input File '%s'\n",szInputFile);
      inputFile = zOpen(szInputFile, O_readb);
      if (inputFile == (FILE *)NIL) Zexit(1);

      if (!Zgethead(inputFile,&FileHeader)) BombOff(1);

      zTaskMessage(2,"Opening Scratch File '%s'\n",szTemporaryFile);
      outputFile = zOpen(szTemporaryFile, O_writeb);
      if (outputFile == (FILE *)NIL) BombOff(1);

      if (Zputhead(outputFile, &FileHeader)) BombOff(1);

      Zfactor = cmplx(ZFACTOR[0],ZFACTOR[1]);
   /*
   ** Now trap invalid combinations
   */
      if (invalidCombination(FileHeader.type, Zfactor)) BombOff(1);
      
   /*
   ** In the event we are subtracting off the mean, just
   ** get the mean value, set FACTOR (or Zfactor) to the negative of the mean
   ** and continue.
   */
      if (CODE == 8) calculateMean(&FileHeader, &FACTOR, &Zfactor);

      TCNT=0.;

      while (Zread(inputFile, BUFFER, FileHeader.type))     // Read until EOF or error. Prints a message in the event of an error.
         {
         if (extractValues(BUFFER, &FileHeader, TCNT, &TIME, &Rval, &Zval, &FLAG))
            {
            zTaskMessage(10,"Invalid Data Type.\n");
            BombOff(1);
            }

         if (!FLAG)   /* Only apply calculations to valid data points */
            {
            switch (CODE)
               {
               case -1: /* Custom Operation */
                  customOperation(FileHeader.type, FACTOR, Zfactor, TIME, &Rval, &Zval);
                  break;
               case 0: /* Add FACTOR    */
                  addFactor(FileHeader.type, FACTOR, Zfactor, &Rval, &Zval);
                  break;
               case 1: /* Multiply by FACTOR */
                  multiplyFactor(FileHeader.type, FACTOR, Zfactor, &Rval, &Zval);
                  break;
               case 2: /* Divide by FACTOR */
                  dividebyFactor(FileHeader.type, FACTOR, Zfactor, &Rval, &Zval);
                  break;
               case 3: /* Divide data into FACTOR */
                  divideintoFactor(FileHeader.type, FACTOR, Zfactor, &Rval, &Zval);
                  break;
               case 4: /* Raise data to FACTOR power */
                  raisetoFactorPower(FileHeader.type, FACTOR, Zfactor, &Rval, &Zval);
                  break;
               case 5: /* Log of data in base FACTOR */
                  logBaseFactor(FileHeader.type, FACTOR, Zfactor, &Rval, &Zval);
                  break;
               case 6: /* Anti-log of data in base factor */
                  antilogBaseFactor(FileHeader.type, FACTOR, Zfactor, &Rval, &Zval);
                  break;
               case 7: /* Round */
                  roundData(FileHeader.type, FACTOR, Zfactor, &Rval, &Zval);
                  break;
               case 8: /* Subtract mean */
                  subtractFactor(FileHeader.type, FACTOR, Zfactor, &Rval, &Zval);
                  break;

               } /* End Switch */

            switch (FileHeader.type)  /* Fill data value buffer */
               {
               case R_Data:    /* Real Time Series */
                  RDataPntr->y  = Rval;
                  break;
               case TR_Data:  /* Real Time Labeled */
                  TRDataPntr->y = Rval;
                  break;
               case X_Data:   /* Complex Time Series */
                  XDataPntr->z = Zval;
                  break;
               case TX_Data:   /* Complex Time Labeled */
                  TXDataPntr->z = Zval;
                  break;
               }
            } /* End IF */

         if (Zwrite(outputFile, BUFFER, FileHeader.type)) BombOff(1);

         ++TCNT;  /* Count number of writes */
         } /* End WHILE */

      if (ferror(inputFile)) BombOff(1);

      zTaskMessage(2,"%ld Data Points Processed\n",(long)TCNT);

      fcloseall();

      ERRFLAG = zNameOutputFile(szOutputFile,szTemporaryFile);
      } // for (iwc = 0, pWild = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

/*************************
** Helper functions
*/
BOOL invalidCombination(short type, struct complex Zfactor)
   {
   BOOL bError = FALSE;
   
   switch (type)
      {
      case R_Data:
      case TR_Data:
         switch (CODE)
            {
            case 2:
               if (!FACTOR)
                  {
                  zTaskMessage(10,"FACTOR = 0 with CODE = 2\n");
                  bError = TRUE;
                  }
               break;
            case 5:
            case 6:
               if (FACTOR<=0)
                  {
                  zTaskMessage(10,"FACTOR <= 0 as Log Base\n");
                  bError = TRUE;
                  }
               break;
            }
         break;
      case X_Data:
      case TX_Data:
         switch (CODE)
            {
            case 2:
               if (!c_abs(Zfactor))
                  {
                  zTaskMessage(10,"|ZFACTOR| = 0 with CODE = 2\n");
                  bError = TRUE;
                  }
               break;
            case 5:
            case 6:
               if (!c_abs(Zfactor))
                  {
                  zTaskMessage(10,"|ZFACTOR| = 0 as Complex Log Base\n");
                  bError = TRUE;
                  }
               break;
            }
         break;
      default:  /* File type is not known */
         zTaskMessage(10, "Invalid File Type.\n");
         bError = TRUE;
      }

   return(bError);
   }


void addFactor(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval += factor;
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_add(*Zval,Zfactor);
         break;
      }
   return;
   }

void multiplyFactor(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval *= factor;
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_mul(*Zval,Zfactor);
         break;
      }
   return;
   }

void dividebyFactor(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval /= factor;
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_div(*Zval,Zfactor);
         break;
      }
   return;
   }

void divideintoFactor(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         if (!Rval)
            {
            zTaskMessage(8,DVZP);
            *Rval = 0.;
            }
         else
            *Rval = FACTOR / (*Rval);
         break;
      case X_Data:
      case TX_Data:
         if (!c_abs(*Zval))
            {
            zTaskMessage(8,DVZP);
            *Zval = cmplx(0.,0.);
            }
         else
            *Zval = c_div(Zfactor,*Zval);
         break;
      }
      
   return;
   }

void raisetoFactorPower(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = pow(*Rval,factor);
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_pow(*Zval,Zfactor);
         break;
      }
   return;
   }

void logBaseFactor(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = log(*Rval) / log(factor);
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_div(c_ln(*Zval),c_ln(Zfactor));
         break;
      }
   return;
   }

void antilogBaseFactor(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = pow(factor, *Rval);
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_pow(Zfactor, *Zval);
         break;
      }
   return;
   }

void roundData(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = unbiasedRound(*Rval);
         break;
      case X_Data:
      case TX_Data:
         *Zval = cmplx(unbiasedRound(Zval->x), unbiasedRound(Zval->y));
         break;
      }
   return;
   }

void subtractFactor(short type, double factor, struct complex Zfactor, double *Rval, struct complex *Zval)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval -= factor;
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_sub(*Zval,Zfactor);
         break;
      }
   return;
   }

/*
** Assume we are just past the header and ready to read in values DEBUG
*/
void calculateMean(struct FILEHDR *fileHeader, double *factor, struct complex *Zfactor)
   {
   fpos_t lCurrent;
   char BUFFER[sizeof(struct TXData)];
   double TCNT=0., time, Rval;
   struct complex Zval;
   short FLAG;
   double count = 0;;
   
   fgetpos(inputFile, &lCurrent); // Needed to restore file pointer for the next read cycle

   *factor = 0.0;                 // We will be accumulating values in these variables
   *Zfactor = cmplx(0.0, 0.0);
   
   while (Zread(inputFile, BUFFER, fileHeader->type))     // Read until EOF or error. Prints a message in the event of an error.
      {
      extractValues(BUFFER, fileHeader, TCNT, &time, &Rval, &Zval, &FLAG);

      if (!FLAG) // Use only good data
         {
         ++count; // Count the number of good values used
         
         switch (fileHeader->type)
            {
            case R_Data:
            case TR_Data:
               *factor += Rval;
               break;
            case X_Data:
            case TX_Data:
               *Zfactor = c_add(*Zfactor, Zval);
               break;
            }

         } //if (!FLAG)
      ++TCNT;                                      // Number of actual reads needed for the time in time series files
      } //while (Zread(inputFile, BUFFER, type))

   fseek(inputFile, lCurrent, SEEK_SET);           // Restore the file pointer

   if (count)
      {
      *factor /= count;
      *Zfactor = cmplx(Zfactor->x/count, Zfactor->y/count);

      switch (fileHeader->type)
         {
         case R_Data:
         case TR_Data:
            zTaskMessage(5,"Subtracting Mean Value of %lG\n",*factor);
            break;
         case X_Data:
         case TX_Data:
            zTaskMessage(5,"Subtracting Complex Mean Value of %lG + i(%lG)\n",Zfactor->x, Zfactor->y);
            break;
         }
      }
      
   return;
   }

void customOperation(short type, double factor, struct complex Zfactor, double time, double *Rval, struct complex *Zval)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval += factor * sin((time * (TWOPI/86400.)) + 0.4);
         *Rval = unbiasedRound(*Rval);
         break;
      }
   return;
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
   unlink(szTemporaryFile);
   ZCatFiles((char*)NIL); // free catalog memory
   Zexit(a);
   }

void fcloseall()           // Declared in tisan.h
   {
   if (inputFile) Zclose(inputFile);
   inputFile = (FILE *)NIL;
   
   if (outputFile) Zclose(outputFile);
   outputFile = (FILE *)NIL;
   
   return;
   }
