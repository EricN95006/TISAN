/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task FIT
*
* Fourier Integral Transform
* CODE = 0 ->  FIT
* CODE = 1 -> IFIT
*
* The default behavior is
* F(ν) = 2/N ∫(f(t) exp(-i 2π ν t) dt
* summed from 0 to N - 1 (Fundamental to 2*Nyquist)
*
* The Inverse transform i -> -i and 2/N is changed to 1/2

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

void FINIT(void);
void MEMREDUCE(void);
short DSKREDUCE(void);

double Omega, Nu, SLOPE, II, TIME, RDATA, IDATA=0., TCNT, HalfN;
double N=0., a1H0H1ON;
double TOTAL=0.;
long NUMDAT, NDAT;
char INFILE[_MAX_PATH], OUTFILE[_MAX_PATH], TMPFILE[_MAX_PATH];

struct FILEHDR FileHeader;
struct RData  *RDataPntr;
struct TRData *TRDataPntr;
struct XData  *XDataPntr;
struct TXData *TXDataPntr;
struct XData  XDataOut;
char *dataBuffer = (char *)NIL;
BOOL bMemoryFile = TRUE;

double TWOPI;

FILE *INSTR  = (FILE *)NIL;
FILE *OUTSTR = (FILE *)NIL;

const char szTask[]="FIT";

int main(int argc, char *argv[])
   {
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;

   TWOPI = 2.0 * acos(-1.0);
   
   signal(SIGINT,BREAKREQ);  /* Handle ^C Interrupt */
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1); /* Initialize Task */

   RDataPntr  = (struct RData *)dataBuffer;
   TRDataPntr = (struct TRData *)dataBuffer;
   XDataPntr  = (struct XData *)dataBuffer;
   TXDataPntr = (struct TXData *)dataBuffer;

   if ((CODE<0) || (CODE>1))
      {
      zTaskMessage(10,"Invalid Code Specification\n");
      Zexit(1);
      }

   if (!OUTCLASS[0]) strcpy(OUTCLASS,"fit");

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
      if (!INSTR) Zexit(1);
      
      if (!Zgethead(INSTR,&FileHeader)) BombOff(1);

      switch (FileHeader.type)
         {
         case X_Data:
         case TX_Data:
         case R_Data:
         case TR_Data:
            break;
         default:
            zTaskMessage(10,"Unknown File Type.\n");
            BombOff(1);
         }

      FINIT();          /* Do some initialization */

      zTaskMessage(2,"Opening Scratch File '%s'\n",TMPFILE);
      if ((OUTSTR = zOpen(TMPFILE,O_writeb)) == NULL) BombOff(1);

      if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);

      FACTOR = floor(FACTOR);

      if (FACTOR < 3.0) FACTOR = (double)N;

      SLOPE = (TRANGE[1] - TRANGE[0])/(FACTOR - 1.0);
      
      XDataOut.f = 0;

      for (II = 0.0; II < FACTOR; ++II)
         {
         XDataOut.z.x = XDataOut.z.y = 0.;

         Omega = TWOPI * (II * SLOPE + TRANGE[0]);  // Calculate next Omega which is 2 pi * frequency

         if (bMemoryFile)
            MEMREDUCE();      /* Data Fit into Memory */
         else
            {
            if (DSKREDUCE()) BombOff(1);  /* Disk Processing */
            }

         Zwrite(OUTSTR,(char *)&XDataOut,X_Data);

         printPercentComplete((long)II, (long)FACTOR, PROGRESS);
         } /* End FOR II */

      printPercentComplete((long)II, (long)FACTOR, PROGRESS); // 100% Complete

      FileHeader.type = X_Data;        /* Output File is Complex */
      FileHeader.m = SLOPE;
      FileHeader.b = TRANGE[0];

      Zputhead(OUTSTR,&FileHeader);

      fcloseall();

      ERRFLAG = zNameOutputFile(OUTFILE,TMPFILE);
      } // for (iwc = 0, pWild = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

/************************************************************
**
** Initialize file stuff
*/
void FINIT()
   {
   double FIRST, FT, DELT=0., RMEAN=0., IMEAN=0.;
   long I;
   short FFLAG=0, FLAG=0;
   long numDataRecords;

// Need to initialize these globals in case there are wildcards in the file name and thus we would make multiple passes
   IDATA = 0.;
   N = 0.;
   TOTAL = 0.;
   
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
      zTaskMessage(2,"Data file too large for available memopry. Processing from file (which might take a while).\n");

   TCNT = 0.;

   while ((NDAT = zGetData(numDataRecords, INSTR, dataBuffer, FileHeader.type)))
      {
      NUMDAT = NDAT;                                  // should be the same as numDataRecords (we would hope)
      
      RDataPntr =  (struct RData *)dataBuffer;       /* Setup some data pointers */
      TRDataPntr = (struct TRData *)dataBuffer;
      XDataPntr =  (struct XData *)dataBuffer;
      TXDataPntr = (struct TXData *)dataBuffer;

      for (I = 0; I < NUMDAT; ++I)           /* Iterate over data buffer */
         {
         switch (FileHeader.type)
            {
            case R_Data:
               TIME = TCNT * FileHeader.m + FileHeader.b;
               ++TCNT;
               RDATA = RDataPntr->y;
               FLAG = RDataPntr->f;
               ++RDataPntr;             /* Advance to next value */
               break;
            case TR_Data:
               TIME = TRDataPntr->t;
               RDATA = TRDataPntr->y;
               ++TRDataPntr;            /* Advance to next value */
               break;
            case X_Data:
               TIME = TCNT * FileHeader.m + FileHeader.b;
               ++TCNT;
               RDATA = XDataPntr->z.x;
               IDATA = XDataPntr->z.y;
               FLAG = XDataPntr->f;
               ++XDataPntr;             /* Advance to next vale */
               break;
            case TX_Data:
               TIME = TXDataPntr->t;
               RDATA = TXDataPntr->z.x;
               IDATA = TXDataPntr->z.y;
               ++TXDataPntr;            /* Advance to next value */
               break;
            }

         if (!FLAG)                 /* Only use good data */
            {
            ++N;
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
                  DELT = Min(DELT,TIME - FT);
               FT = TIME;
               }

            RMEAN += RDATA;
            IMEAN += IDATA;
            } /* End IF */
         } /* End For */
      } /* End WHILE */

   if (ferror(INSTR)) BombOff(1);

   if (N < 3L)
      {
      zTaskMessage(10,"** ERROR ** File Contains Only %ld Point(s)\n", (long)N);
      BombOff(1);
      }

   RMEAN /= N;
   IMEAN /= N;
   HalfN = N / 2.;

   zTaskMessage(3,"File Contains %ld Points\n", (long)N);
   zTaskMessage(3,"Mean Value = %lG + i(%lG)\n",RMEAN, IMEAN);

   if (TRANGE[0] >= TRANGE[1])
      {
      TRANGE[0] = 1.0/(TIME - FIRST);
      TRANGE[1] = 1.0/DELT;          // Twice Nyqyist
      }

   zTaskMessage(3,"Fundamental Frequency = %lG\n",1.0/(TIME - FIRST));
   zTaskMessage(3,"Nyquist Frequency = %lG\n",1.0/(2.0 * DELT));

   return;
   }

/***************************************************************
**
** Process "small" data files
*/
void MEMREDUCE()
   {
   long I;
   short Pass1 = 1;
   double h1, h2, LastTime, LastX, LastY, DeltaT, ThisX, ThisY;
   double avgDeltaT = 0.0;
   short FLAG = 0;
   
   RDataPntr  = (struct RData *)dataBuffer;
   TRDataPntr = (struct TRData *)dataBuffer;
   XDataPntr  = (struct XData *)dataBuffer;
   TXDataPntr = (struct TXData *)dataBuffer;

   TCNT = 0.0;

   for (I = 0; I < NUMDAT; ++I)
      {
      switch (FileHeader.type)
         {
         case R_Data:
            TIME = TCNT * FileHeader.m + FileHeader.b;
            RDATA = RDataPntr->y;
            FLAG = RDataPntr->f;
            ++TCNT;
            ++RDataPntr;
            break;
         case TR_Data:
            TIME = TRDataPntr->t;
            RDATA = TRDataPntr->y;
            ++TRDataPntr;
            break;
         case X_Data:
            TIME = TCNT * FileHeader.m + FileHeader.b;
            RDATA = XDataPntr->z.x;
            IDATA = XDataPntr->z.y;
            FLAG = XDataPntr->f;
            ++TCNT;
            ++XDataPntr;
            break;
         case TX_Data:
            TIME = TXDataPntr->t;
            RDATA = TXDataPntr->z.x;
            IDATA = TXDataPntr->z.y;
            ++TXDataPntr;
            break;

         }

      if (!FLAG)
         {
         if (Pass1)
            {
            Pass1 = 0;

            LastTime = TIME;

            TIME *= Omega;
            h1 = cos(TIME);
            h2 = sin(TIME);

            switch (CODE)
               {
               case 0: /* FIT */
                  LastX = ((RDATA * h1) + (IDATA * h2));
                  LastY = ((IDATA * h1) - (RDATA * h2));
                  break;
               case 1: /* IFIT */
                  LastX = ((RDATA * h1) - (IDATA * h2));
                  LastY = ((IDATA * h1) + (RDATA * h2));
                  break;
               }
            }
         else
            {
            DeltaT = TIME - LastTime;
            avgDeltaT += DeltaT; 

            LastTime = TIME;

            TIME *= Omega;
            h1 = cos(TIME);
            h2 = sin(TIME);

            switch (CODE)
               {
               case 0: /* FIT using simple rectangular integration */
                  ThisX = (RDATA * h1) + (IDATA * h2);
                  ThisY = (IDATA * h1) - (RDATA * h2);
                  XDataOut.z.x += (DeltaT * (ThisX + LastX) / 2.);
                  XDataOut.z.y += (DeltaT * (ThisY + LastY) / 2.);
                  break;
               case 1: /* IFIT */
                  ThisX = (RDATA * h1) - (IDATA * h2);
                  ThisY = (IDATA * h1) + (RDATA * h2);
                  XDataOut.z.x += (DeltaT * (ThisX + LastX) / 2.);
                  XDataOut.z.y += (DeltaT * (ThisY + LastY) / 2.);
                  break;
               }

            LastX = ThisX;
            LastY = ThisY;
            } // if (Pass1) ... else
         } // if (!FLAG)
      } // for (I = 0; I < NUMDAT; ++I)

   avgDeltaT /= (2. * HalfN);

   if (!CODE)
      {
      XDataOut.z.y /= HalfN;
      XDataOut.z.x /= HalfN;

      XDataOut.z.y /= avgDeltaT; // Apply scaling so sin(t) gives an amplitude of 1
      XDataOut.z.x /= avgDeltaT;
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
short DSKREDUCE()
   {
   short Pass1 = 1;
   double h1, h2, LastTime, LastX, LastY, DeltaT, ThisX, ThisY;
   double avgDeltaT = 0.0;
   short FLAG = 0;
   
   TCNT = 0.;

   if (!Zgethead(INSTR,(struct FILEHDR *)NULL)) return(1);

   while (Zread(INSTR,dataBuffer,FileHeader.type))
      {
      switch (FileHeader.type)
         {
         case R_Data:
            TIME = TCNT * FileHeader.m + FileHeader.b;
            RDATA = RDataPntr->y;
            FLAG = RDataPntr->f;
            ++TCNT;
            break;
         case TR_Data:
            TIME = TRDataPntr->t;
            RDATA = TRDataPntr->y;
            break;
         case X_Data:
            TIME = TCNT * FileHeader.m + FileHeader.b;
            RDATA = XDataPntr->z.x;
            IDATA = XDataPntr->z.y;
            FLAG = XDataPntr->f;
            ++TCNT;
            break;
         case TX_Data:
            TIME = TXDataPntr->t;
            RDATA = TXDataPntr->z.x;
            IDATA = TXDataPntr->z.y;
            break;
         }

      if (!FLAG)
         {
         if (Pass1)
            {
            Pass1 = 0;

            LastTime = TIME;

            TIME *= Omega;
            h1 = cos(TIME);
            h2 = sin(TIME);

            switch (CODE)
               {
               case 0: /* FIT */
                  LastX = ((RDATA * h1) + (IDATA * h2));
                  LastY = ((IDATA * h1) - (RDATA * h2));
                  break;
               case 1: /* IFIT */
                  LastX = ((RDATA * h1) - (IDATA * h2));
                  LastY = ((IDATA * h1) + (RDATA * h2));
                  break;
               }
            }
         else
            {
            DeltaT = TIME - LastTime;
            avgDeltaT += DeltaT; 

            LastTime = TIME;

            TIME *= Omega;
            h1 = cos(TIME);
            h2 = sin(TIME);

            switch (CODE)
               {
               case 0: /* FIT */
                  ThisX = (RDATA * h1) + (IDATA * h2);
                  ThisY = (IDATA * h1) - (RDATA * h2);
                  XDataOut.z.x += (DeltaT * (ThisX + LastX) / 2.);
                  XDataOut.z.y += (DeltaT * (ThisY + LastY) / 2.);
                  break;
               case 1: /* IFIT */
                  ThisX = (RDATA * h1) - (IDATA * h2);
                  ThisY = (IDATA * h1) + (RDATA * h2);
                  XDataOut.z.x += (DeltaT * (ThisX + LastX) / 2.);
                  XDataOut.z.y += (DeltaT * (ThisY + LastY) / 2.);
                  break;
               }
            LastX = ThisX;
            LastY = ThisY;
            }
         }
      } /* End WHILE */

   avgDeltaT /= (2. * HalfN);

   if (!CODE)
      {
      XDataOut.z.y /= HalfN;
      XDataOut.z.x /= HalfN;

      XDataOut.z.y /= avgDeltaT; // Apply scaling so sin(t) gives an amplitude of 1
      XDataOut.z.x /= avgDeltaT;
      }
   else
      {
      XDataOut.z.y /= 2.;
      XDataOut.z.x /= 2.;
      }

   return(ferror(INSTR));
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
