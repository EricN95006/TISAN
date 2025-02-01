/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task PGRAM
*
* This task is used to calculate the periodogram of a data base as described by Horne J.H. and Baliunas S.L., 1986,
* Astrophysical Journal, 302, p. 757 based on the paper by Scargle,J.D. 1982, Astrophys. Jour., 263, 835.
*
* Task to Calculate the Periodogram of the Data within the
* frequency range TRANGE with FACTOR output points.
* Factor must be greater than 3
*
* CODE = 1 -> Scale by the variance
* CODE = 2 -> Change to % Probability Level
*
* TRANGE - Frequency range (not angular frequency)
*
* FACTOR - Default is number of iput points
*
* OUTCLASS - default is "pgm"
*
* Defaults are 2pi/T to (2pi/min(t))/2 and N points
*
* The program allocates the memory needed using malloc.
* If the input file is too large for a single allocation, then
* it is processed from disk one data point at a time.
*
* The infile of this task accepts wild cards.
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

double tau(double);
void pxw(double,double);
void variance(void);

char *dataBuffer = (char *)NIL;

char INFILE[_MAX_PATH], OUTFILE[_MAX_PATH], TMPFILE[_MAX_PATH];
double N0=0.0, SIGMA2, MEAN;
struct FILEHDR FileHeader;
long   NUMDAT, NDAT;
short  FLAG=0;
double DATA, TIME;
BOOL bMemoryFile = TRUE;
long NUMPNT;                                          // Number of points in the periodogram
struct RData *pgramDataBuffer = (struct RData *)NIL;  // Data array for the periodogram
long numDataRecords = 0L;

double TWOPI;

FILE *INSTR = (FILE *)NIL;
FILE *OUTSTR = (FILE *)NIL;

const char szTask[]="PGRAM";

int main(int argc, char *argv[])
   {
   long I;
   short  FFLAG=0;
   double FIRST, OMEGA, SLOPE;
   double OSTART, OSTOP, TCNT=0.;
   double SUM=0., SUM2=0., DELT=0., FT, DT;
   struct RData  *RDataPntr;
   struct TRData *TRDataPntr;
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;
   
   TWOPI = 2.0 * acos(-1.0);

   signal(SIGINT,BREAKREQ);      /* Process ^C Interrupt */
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);     /* Initialize Task */

   if (!OUTCLASS[0]) strcpy(OUTCLASS,"pgm");

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
      if ((INSTR = zOpen(INFILE,O_readb)) == NULL) Zexit(1);

      if (!Zgethead(INSTR,&FileHeader)) BombOff(1);
      if ((FileHeader.type != R_Data) &&
          (FileHeader.type != TR_Data))
         {
         zTaskMessage(10,"Cannot process complex data files.\n");
         BombOff(1);
         }

// Initialize in case we have wildcards in the file name
      N0 = 0.0;
      FLAG = 0;
      bMemoryFile = TRUE;
      FFLAG = 0;
      TCNT = 0.;
      SUM = 0.;
      SUM2 = 0.;
      DELT = 0.;

   /*
   ** We would like to be able to load the entire file into memory at once, however that might not be possible,
   ** so allocate the full space and keep asking for less if the allocation fails. We will either get some
   ** memory or we will error out with no bytes left to request.
   */
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
         
      OSTART = TRANGE[0] * TWOPI;     /* Omega Start and Stop Ranges */
      OSTOP  = TRANGE[1] * TWOPI;
      
   /*
   ** Find Size of File, MEAN and SIGMA2.
   ** File is loaded into dataBuffer and NUMDAT is set.
   ** If the file is small, (i.e. it fit in memory on 1 read)
   ** then dataBuffer and NUMDAT are not changed ever again.
   **
   */
      printPercentComplete(0L, 0L, 0);

      while ((NDAT = zGetData(numDataRecords, INSTR, dataBuffer, FileHeader.type)))    //  Read in as much as possible at a time
         {
         NUMDAT = NDAT;                               // NUMDAT should be the same as numDataRecords, but better safe than sorry.
         RDataPntr =  (struct RData *)dataBuffer;
         TRDataPntr = (struct TRData *)dataBuffer;

         for (I=0;I<NUMDAT;++I)           /* Iterate over data buffer */
            {
            switch (FileHeader.type)
               {
               case R_Data:
                  TIME = TCNT * FileHeader.m + FileHeader.b;
                  ++TCNT;
                  DATA = RDataPntr->y;
                  FLAG = RDataPntr->f;
                  ++RDataPntr;             /* Advance to next value */
                  break;
               case TR_Data:
                  TIME = TRDataPntr->t;
                  DATA = TRDataPntr->y;
                  ++TRDataPntr;            /* Advance to next value */
                  break;
               }

            if (!FLAG)                 /* Only use good data */
               {
               ++N0;
               if (!FFLAG)
                  {
                  FIRST = FT = TIME;
                  FFLAG = 1;
                  }
               else
                  {
                  if (!DELT)
                     DELT = TIME - FT;
                  else
                     {
                     DT = TIME - FT;
                     if (DT) DELT = Min(DELT,DT);
                     }
                  FT = TIME;
                  }
               SUM += DATA;
               SUM2 += Square(DATA);
               }
            }
         } /* End WHILE */

      if (ferror(INSTR)) BombOff(1);

      if (N0 < 2.)
         {
         zTaskMessage(10,"Insufficient Data\n", szTask);
         BombOff(1);
         }

      SIGMA2 = ((N0 * SUM2) - Square(SUM))/(N0*(N0-1.));
      MEAN = SUM/N0;
      DELT = 1./(2.*DELT);

   // Default angular frequency ranges if zeros are passed in TRANGE
      if (OSTART >= OSTOP) OSTART = OSTOP = 0.;
      if (!OSTOP) OSTOP  = TWOPI*DELT;
      if (!OSTART) OSTART = TWOPI/(TIME-FIRST);

      zTaskMessage(3,"File Contains %ld Valid Points\n", (long)N0);
      zTaskMessage(3,"Variance = %lG\n", SIGMA2);
      zTaskMessage(3,"Mean = %lG\n", MEAN);
      zTaskMessage(3,"Fundamental Frequency = %lG\n", 1./(TIME-FIRST));
      zTaskMessage(3,"Nyquist Frequency = %lG\n", DELT);

      if (FACTOR < 3.)           /* Set Defaults for funny FACTOR values */
         NUMPNT = (long)N0;
      else
         NUMPNT = (long)FACTOR;

      pgramDataBuffer = (struct RData *)malloc(sizeof(struct RData) * NUMPNT);
      
      if (pgramDataBuffer == (struct RData *)NIL)
         {
         zTaskMessage(10, "Unable to allocate %ld bytes for periodogram. Set FACTOR to a smaller value.\n", sizeof(struct RData) * NUMPNT);
         BombOff(1);
         }

      SLOPE = (OSTOP-OSTART)/(double)(NUMPNT - 1L);   /* Omega value slope */

      for (I=0L; I<NUMPNT; ++I)
         {
         printPercentComplete(I, NUMPNT, PROGRESS);

         OMEGA = OSTART + (double)I * SLOPE;
         pxw(tau(OMEGA),OMEGA);
         } /* End For */

      printPercentComplete(I, NUMPNT, PROGRESS); // 100% Complete

      zTaskMessage(2,"Scaling periodogram...\n");

      variance();

      zTaskMessage(2,"Opening Scratch File '%s'\n",TMPFILE);
      if ((OUTSTR = zOpen(TMPFILE,O_writeb)) == NULL) BombOff(1);

      FileHeader.b = OSTART/TWOPI;     /* Build Output header */
      FileHeader.m = SLOPE/TWOPI;
      FileHeader.type = R_Data;

      if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);

      if (zPutData(NUMPNT,OUTSTR,(char *)pgramDataBuffer,R_Data) <= 0L) BombOff(1);

      fcloseall();

      ERRFLAG = zNameOutputFile(OUTFILE,TMPFILE);
      } // for (iwc = 0, pWild = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

/*********************************************************************
*
* Function to Calculate Tau's
*
*/
double tau(double OMEGA)
   {
   double ARG, V1=0.,V2=0.,TCNT=0.;
   long J;
   struct RData  *RDataPntr;
   struct TRData *TRDataPntr;

   RDataPntr =  (struct RData *)dataBuffer;
   TRDataPntr = (struct TRData *)dataBuffer;
   OMEGA *= 2.;

   if (bMemoryFile)        /* Data is memory resident */
      {
      for (J=0L; J<NUMDAT; ++J)
         {
         switch (FileHeader.type)
            {
            case R_Data:
               TIME = TCNT * FileHeader.m + FileHeader.b;
               ++TCNT;
               FLAG = RDataPntr->f;
               break;
            case TR_Data:
               TIME = TRDataPntr->t;
               ++TRDataPntr;
               break;
            }
         if (!FLAG)
            {
            ARG = OMEGA*TIME;
            V1 += sin(ARG);
            V2 += cos(ARG);
            }
         }  /* End For */
      }  /* End IF */
   else                    /* Read data from disk */
      {
      if (!Zgethead(INSTR,(struct FILEHDR *)NULL)) BombOff(1);
      while (Zread(INSTR,dataBuffer,FileHeader.type))
         {
         switch (FileHeader.type)
            {
            case R_Data:
               TIME = TCNT * FileHeader.m + FileHeader.b;
               ++TCNT;
               FLAG = RDataPntr->f;
               break;
            case TR_Data:
               TIME = TRDataPntr->t;
               break;
            }
         if (!FLAG)
            {
            ARG = OMEGA*TIME;
            V1 += sin(ARG);
            V2 += cos(ARG);
            }
         } /* End WHILE */
      if (ferror(INSTR))
         {
         zError();
         BombOff(1);
         }
      }    /* End ELSE */
   return(atan2(V1,V2)/OMEGA);
   }

/*********************************************************************
*
* Function to Calculate Periodogram
*
*/
void pxw(double TAUV, double OMEGA)
   {
   double V1=0., V2=0., V3=0., V4=0., V, ARG, TCNT=0.;
   long J;
   struct RData  *RDataPntr;
   struct TRData *TRDataPntr;
   static long lIndex = 0L;

   RDataPntr =  (struct RData *)dataBuffer;
   TRDataPntr = (struct TRData *)dataBuffer;

   if (bMemoryFile)
      {
      for (J=0L; J<NUMDAT; ++J)
         {
         switch (FileHeader.type)
            {
            case R_Data:
               TIME = TCNT * FileHeader.m + FileHeader.b;
               DATA = RDataPntr->y - MEAN;
               FLAG = RDataPntr->f;
               ++RDataPntr;
               ++TCNT;
               break;
            case TR_Data:
               TIME = TRDataPntr->t;
               DATA = TRDataPntr->y - MEAN;
               ++TRDataPntr;
               break;
            }
         if (!FLAG)
            {
            ARG = OMEGA * (TIME - TAUV);
            V = cos(ARG);
            V1 += DATA * V;
            V3 += Square(V);
            V = sin(ARG);
            V2 += DATA * V;
            V4 += Square(V);
            }
         }  /* End For */
      } /* End IF */
   else
      {
      if (!Zgethead(INSTR,(struct FILEHDR *)NULL)) BombOff(1);
      while (Zread(INSTR,dataBuffer,FileHeader.type))
         {
         switch (FileHeader.type)
            {
            case R_Data:
               TIME = TCNT * FileHeader.m + FileHeader.b;
               DATA = RDataPntr->y - MEAN;
               FLAG = RDataPntr->f;
               ++TCNT;
               break;
            case TR_Data:
               TIME = TRDataPntr->t;
               DATA = TRDataPntr->y - MEAN;
               break;
            }
         if (!FLAG)
            {
            ARG = OMEGA * (TIME - TAUV);
            V = cos(ARG);
            V1 += DATA * V;
            V3 += Square(V);
            V = sin(ARG);
            V2 += DATA * V;
            V4 += Square(V);
            }
         } /* End While */
      if (ferror(INSTR))
         {
         zError();
         BombOff(1);
         }
      } /* End ELSE */

   pgramDataBuffer[lIndex].y = (V1*(V1/V3) + V2*(V2/V4))/2.;

   ++lIndex;

   return;
   }

/*********************************************************************
*
* Function to Scale Pgram by the Variance if CODE=1
* or as probability if CODE=2
*
*/
void variance()
   {
   long I;
   double NI, M;

   M = NI = -6.362 + 1.193*N0 + .00098*N0*N0;

   for (I=0L; I<NUMPNT; ++I)
      {
      if (CODE) pgramDataBuffer[I].y /= SIGMA2;

      if (CODE == 2)
         pgramDataBuffer[I].y = pow(1. - exp(-pgramDataBuffer[I].y),NI);

      pgramDataBuffer[I].f = 0;
      }

   if (CODE)
      {
      DATA = 0.;
      while (M > 1.)
         {
         DATA += (1./M);
         --M;
         }

      if (CODE == 2) DATA = pow(1. - exp(-DATA),NI);

      zTaskMessage(3,"Expected Noise Peak = %lG\n",DATA);
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
   unlink(TMPFILE);
   ZCatFiles((char*)NIL); // free catalog memory
   Zexit(a);
   }

void fcloseall(void)
   {
   if (INSTR) Zclose(INSTR);
   INSTR = (FILE *)NIL;
   
   if (OUTSTR) Zclose(OUTSTR);
   OUTSTR = (FILE *)NIL;

   if (pgramDataBuffer) free(pgramDataBuffer);
   pgramDataBuffer = (struct RData *)NIL;
   
   if (dataBuffer) free(dataBuffer);
   dataBuffer = (char *)NIL;
   
   return;
   }

