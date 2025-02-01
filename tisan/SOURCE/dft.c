/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task DFT
*
* Discrete Fourier Transform
* See Ronald N. Bracewell "The Fourier Transform and its Applications"
*
* CODE 0 -> DFT
* CODE 1 -> IDFT
*
* F(ν) = 2/N ∑(f(t(n)) exp(-i 2 π t(n) (ν/N))
* summed from n=0 to N - 1 (Fundamental to 2*Nyquist)
*
* The Inverse transform i -> -i and 2/N is changed to 1/2
*
* Default outclass = dft
*
* The infile of this task accepts wild cards.
*
*/
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <signal.h>
#include <string.h>

#include "tisan.h"

double twoPi;
double    Pi;

void InitializeDFT(void);
void MEMREDUCE(double Nu);
short DSKREDUCE(double Nu);

char *dataBuffer = (char *)NIL;
char INFILE[_MAX_PATH], OUTFILE[_MAX_PATH];
char TMPFILE[_MAX_PATH];
double RVAL, IVAL=0., TIME, DATA;
double N=0., Imean=0., Rmean=0.;
struct FILEHDR FileHeader;
short  FLAG=0;
long NUMDAT, NDAT;
struct RData  *RDataPntr;
struct XData  *XDataPntr;
struct XData  XDataOut;
double Nyquist, Fundamental, HalfN;
BOOL   bMemoryFile = TRUE;

FILE *INSTR  = (FILE *)NIL;
FILE *OUTSTR = (FILE *)NIL;

const char szTask[]="DFT";

int main(int argc, char *argv[])
   {
   double Nu;
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;

   Pi = acos(-1.0);
   twoPi = 2.0 * Pi;

   signal(SIGINT,BREAKREQ);
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1); /* Initialize Task */

   RDataPntr = (struct RData *)dataBuffer;
   XDataPntr = (struct XData *)dataBuffer;

   if ((CODE < 0) || (CODE > 1))
      {
      zTaskMessage(10,"Invalid CODE Specification\n");
      Zexit(1);
      }

   if (!OUTCLASS[0]) strcpy(OUTCLASS,"dft");

   zBuildFileName(M_inname,INFILE);
   CatList = ZCatFiles(INFILE);    // Find Matching Files for the input file specification
   if (CatList->N == 0) Zexit(1);  // Quit if there are none

   for (iwc = 0, pWild = CatList->pList;
        (iwc < CatList->N) && !ERRFLAG;
        ++iwc, pWild = strchr(pWild,'\0') + 1)
      {
      splitPath(pWild,Drive,Dir,Fname,Ext);
      strcpy(INNAME,Fname);
      strcpy(INCLASS,Ext);

      zBuildFileName(M_inname,INFILE);
      zBuildFileName(M_outname,OUTFILE);
      zBuildFileName(M_tmpname,TMPFILE);

      zTaskMessage(2,"Opening Input File '%s'\n",INFILE);
      INSTR = zOpen(INFILE,O_readb);
      if (INSTR == NULL) Zexit(1);

      if (!Zgethead(INSTR,&FileHeader)) BombOff(1);

      InitializeDFT();

      zTaskMessage(2,"Opening Scratch File '%s'\n",TMPFILE);

      if ((OUTSTR = zOpen(TMPFILE,O_writeb)) == NULL) BombOff(1);

      if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);

      XDataOut.f = 0;

      printPercentComplete(0L, 0L, PROGRESS);

      for (Nu=0.; Nu < N; ++Nu)
         {
         XDataOut.z.x = XDataOut.z.y = 0.;

         if (bMemoryFile)
            MEMREDUCE(Nu);      /* Data Fit into Memory */
         else
            {
            if (DSKREDUCE(Nu)) BombOff(1);
            }
               
         if (Zwrite(OUTSTR,(char *)&XDataOut,X_Data)) BombOff(1);

         printPercentComplete((long)Nu, (long)N, PROGRESS);
         } /* End FOR */

      printPercentComplete((long)Nu, (long)N, PROGRESS); // 100% Complete

      FileHeader.type = X_Data;
      FileHeader.b = 0.;
      FileHeader.m = (2. * Nyquist - Fundamental) / (N - 1.);

      if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);

      fcloseall();

      ERRFLAG = zNameOutputFile(OUTFILE,TMPFILE);
      } // for (iwc = 0, pWild = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

/***************************************************************
**
** Process "small" data files
*/
void MEMREDUCE(double Nu)
   {
   long I;
   double Tau;
   double h1, h2;
/*
** Determine the output values
*/
   RDataPntr =  (struct RData *)dataBuffer;
   XDataPntr =  (struct XData *)dataBuffer;
   for (I=0; I<NUMDAT; ++I)
      {
      switch (FileHeader.type)
         {
         case R_Data:
            RVAL = RDataPntr->y;
            FLAG = RDataPntr->f;
            ++RDataPntr;
            break;
         case X_Data:
            RVAL = XDataPntr->z.x;
            IVAL = XDataPntr->z.y;
            FLAG = XDataPntr->f;
            ++XDataPntr;
            break;
         }

      if (!FLAG)
         {
         Tau = twoPi * (double)I * Nu / N;
         h1 = cos(Tau);
         h2 = sin(Tau);

         if (!CODE)
            {
            XDataOut.z.x += (RVAL * h1 + IVAL * h2);
            XDataOut.z.y += (IVAL * h1 - RVAL * h2);
            }
         else
            {
            XDataOut.z.x += (RVAL * h1 - IVAL * h2);
            XDataOut.z.y += (IVAL * h1 + RVAL * h2);
            }
         }
      }

   if (!CODE)
      {
      XDataOut.z.y /= HalfN;
      XDataOut.z.x /= HalfN;
      }
   else
      {
      XDataOut.z.y /= 2.;
      XDataOut.z.x /= 2.;
      }

   return;
   }

/***************************************************************
**
** Process "large" data files
*/
short DSKREDUCE(double Nu)
   {
   double Tau, II=0.;
   double h1, h2;
/*
** Determine the output values
*/
   if (!Zgethead(INSTR,(struct FILEHDR *)NULL)) return(1);
   while (Zread(INSTR,dataBuffer,FileHeader.type))
      {
      ++II;
      switch (FileHeader.type)
         {
         case R_Data:
            RVAL = RDataPntr->y;
            FLAG = RDataPntr->f;
            break;
         case X_Data:
            RVAL = XDataPntr->z.x;
            IVAL = XDataPntr->z.y;
            FLAG = XDataPntr->f;
            break;
         }

      if (!FLAG)
         {
         Tau = twoPi * II * Nu / N;
         h1 = cos(Tau);
         h2 = sin(Tau);

         if (!CODE)
            {
            XDataOut.z.x += (RVAL * h1 + IVAL * h2);
            XDataOut.z.y += (IVAL * h1 - RVAL * h2);
            }
         else
            {
            XDataOut.z.x += (RVAL * h1 - IVAL * h2);
            XDataOut.z.y += (IVAL * h1 + RVAL * h2);
            }
         }
      }

   if (ferror(INSTR)) return(1);

   if (!CODE)
      {
      XDataOut.z.y /= HalfN;
      XDataOut.z.x /= HalfN;
      }
   else
      {
      XDataOut.z.y /= 2.;
      XDataOut.z.x /= 2.;
      }

   return(0);
   }

/************************************************************
**
** Initialize file stuff
** When Done, We know if the entire file can be processed
** in memory and how many data points there are (N).
*/
void InitializeDFT()
   {
   long I;
   double GoodN = 0.;
   long numDataRecords;

// Need to initialize these globals in case we are using wild cards in the filename and thus make multiple passes
   IVAL = 0.;
   N = 0.;
   Imean = 0.;
   Rmean = 0.;
   bMemoryFile = TRUE;
   
   numDataRecords = countDataRecords(INSTR);
   
   do {
      dataBuffer = (char *)malloc(numDataRecords * (long)Zsize(FileHeader.type));

      if (!dataBuffer)
         {
         numDataRecords /= 2L;
         bMemoryFile = FALSE;
         }
      }
   while (!dataBuffer && numDataRecords);

   if (!numDataRecords)
      {
      zTaskMessage(10,"Unable to allocate memory for input file.\n");
      BombOff(1);
      }
   
   if (bMemoryFile)
      zTaskMessage(2,"Processing data file in memory.\n");
   else
      zTaskMessage(2,"Data file too large for available memory. Processing from file (which might take a while).\n");

   while ((NDAT = zGetData(numDataRecords, INSTR, dataBuffer, FileHeader.type)))
      {
      NUMDAT = NDAT;
      N += (double)NDAT;
      RDataPntr =  (struct RData *)dataBuffer;
      XDataPntr =  (struct XData *)dataBuffer;

      for (I=0; I<NUMDAT; ++I)          /* Iterate over data buffer */
         {
         switch (FileHeader.type)
            {
            case R_Data:
               RVAL = RDataPntr->y;
               FLAG = RDataPntr->f;
               ++RDataPntr;             /* Advance to next value */
               break;
            case X_Data:
               RVAL = XDataPntr->z.x;
               IVAL = XDataPntr->z.y;
               FLAG = XDataPntr->f;
               ++XDataPntr;             /* Advance to next value */
               break;
            case TR_Data:
            case TX_Data:
               zTaskMessage(10,"Cannot perform a DFT on time labeled data files.\n");
               BombOff(1);
               break;
            default:
               zTaskMessage(10,"Unknown File Type.\n");
               BombOff(1);
            }

         if (!FLAG)                 /* Only use good data */
            {
            Imean += IVAL;
            Rmean += RVAL;
            ++GoodN;
            } /* End IF */
         } /* End For */
      } /* End WHILE */

   if (ferror(INSTR)) BombOff(1);

   if (N < 2.)
      {
      zTaskMessage(10,"** ERROR ** File Contains Only %ld Point(s)\n",(long)N);
      BombOff(1);
      }

   Rmean /= GoodN;
   Imean /= GoodN;

   Nyquist     = 1./(2. * FileHeader.m);
   Fundamental = 1./((N - 1.) * FileHeader.m);
   HalfN = N / 2.;

   zTaskMessage(3,"File Contains %ld Total Points\n", (long)N);
   zTaskMessage(3,"And %ld Valid Points\n", (long)GoodN);
   zTaskMessage(3,"Mean Value =  %lG + i(%lG)\n",Rmean,Imean);
   zTaskMessage(3,"Fundamental Frequency = %lG\n",Fundamental );
   zTaskMessage(3,"Nyquist Frequency = %lG\n",Nyquist);

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
   unlink(TMPFILE);
   ZCatFiles((char*)NIL); // free catalog memory
   Zexit(a);
   }

void fcloseall()
   {
   if (INSTR) Zclose(INSTR);
   INSTR = (FILE *)NIL;

   if (OUTSTR) Zclose(OUTSTR);
   OUTSTR = (FILE *)NIL;

   if (dataBuffer) free(dataBuffer);
   dataBuffer = (char *)NIL;

   return;
   }

