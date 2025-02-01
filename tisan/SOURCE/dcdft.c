/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task DCDFT
*
* Date Compenstaed Discrete Fourier Transform
*
* This task performs a Data Compensated Discrete Fourier Transform as described by Ferraz-Mello, 1981, Astronomical Journal, 86, p. 619. 
* 
* CODE = 0 -> DCDFT
* CODE = 1 -> Inverse DCDFT
* CODE = 2 -> FILTER
* CODE = 3 -> FILTER information only
*
* OUTCLASS -> DEFAULT dcd
*
* TRANGE -> Frequency Range
* FACTOR -> Number of Integration Points
*
* The infile of this task accepts wild cards.
*
*/
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>

#include "tisan.h"

// Maximum number of points that can be loaded into memory at one time.
void FINIT(void);
void MEMREDUCE(void);
short DSKREDUCE(void);

double Omega, Nu, SLOPE, II, TIME, RDATA, IDATA=0., TCNT;
double N=0., a1H0H1ON;
double H0H1, H0H2, H1H1, H2H2, H1H2, h1H2, a1, a2, h1, h2;
double d1, d2;
double NSQ, H0H1SQ, H0H2SQ, H1, H2, TOTAL=0., SQNO2, SQNO8;
short  FLAG=0;
long   NUMDAT, NDAT;
BOOL   bMemoryFile = TRUE;
char   INFILE[_MAX_PATH], OUTFILE[_MAX_PATH], TMPFILE[_MAX_PATH];

struct FILEHDR FileHeader;
struct RData  *RDataPntr;
struct TRData *TRDataPntr;
struct XData  *XDataPntr;
struct TXData *TXDataPntr;
struct XData  XDataOut;

double TWOPI;

char *dataBuffer = (char *)NIL;

FILE *INSTR=(FILE *)NIL;
FILE *OUTSTR=(FILE *)NIL;

const char szTask[]="DCDFT";

int main(int argc, char *argv[])
   {
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;

   TWOPI = 2.0 * acos(-1.0);

   signal(SIGINT,BREAKREQ);  /* Handle ^C Interrupt             */
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);     /* Initialize Task */

   RDataPntr =  (struct RData *)dataBuffer;
   TRDataPntr = (struct TRData *)dataBuffer;
   XDataPntr =  (struct XData *)dataBuffer;
   TXDataPntr = (struct TXData *)dataBuffer;

   if ((CODE<0) || (CODE>3))
      {
      zTaskMessage(10,"Invalid Code Specification\n");
      Zexit(1);
      }

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
      if (!OUTCLASS[0]) strcpy(OUTCLASS,"dcd");
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
            if (CODE > 1)
               {
               zTaskMessage(10,"Can't Filter Complex Files.\n");
               BombOff(1);
               }
         case R_Data:
         case TR_Data:
            break;
         default:
            zTaskMessage(10,"Invalid File Type.\n");
            BombOff(1);
         }

      FINIT();          /* Do some initialization */

      if (CODE != 3)    /* CODE=3 does not create a file, all others do */
         {
         zTaskMessage(2,"Opening Scratch File '%s'\n",TMPFILE);
         if ((OUTSTR = zOpen(TMPFILE,O_writeb)) == NULL) BombOff(1);
         if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);
         }

      if (CODE > 1)
         {
         TRANGE[0] = Nu;
         SLOPE=0.;
         FACTOR=1.;
         }
      else
         {
         if (FACTOR<3.) FACTOR = (double)N;
         FACTOR = floor(FACTOR);
         SLOPE = (TRANGE[1]-TRANGE[0])/(FACTOR-1.);
         }
      
      XDataOut.f = 0;

      printPercentComplete(0L, 0L, 0);

      for (II=0.; II<FACTOR; ++II)
         {
         H0H1=H0H2=H1H1=H2H2=H1H2=h1H2=0.;   /* Initialize Iterative Values */
         XDataOut.z.x = XDataOut.z.y = 0.;
         Omega = TWOPI*(II*SLOPE+TRANGE[0]);  /* Calculate next Omega */

         if (bMemoryFile)
            MEMREDUCE();      /* Data Fit into Memory */
         else
            if (DSKREDUCE()) BombOff(1);  /* Disk Processing */

         if (CODE > 1)   /* Either Filter or just display results */
            {
            d2 = a2*XDataOut.z.y;
            d1 = a1*XDataOut.z.x + d2*Square(a1)*(H0H1*H0H2/N - H1H2);

            zTaskMessage(5,"Amplitude = %lG\n",sqrt(d1*d1+d2*d2));

            if (d1 || d2)
               zTaskMessage(5,"Phase = %lG\n",atan2(d2,d1));
            else
               zTaskMessage(5,"Phase is undefined.\n");
            }
         else
            {
            if (!CODE)
               {
               XDataOut.z.y /= SQNO2;
               XDataOut.z.x /= SQNO2;
               }
            else
               {
               XDataOut.z.y *= SQNO8;
               XDataOut.z.x *= SQNO8;
               }

            Zwrite(OUTSTR,(char *)&XDataOut,X_Data);
            }

         printPercentComplete((long)II, (long)FACTOR, PROGRESS);
         } /* End FOR II */

      printPercentComplete((long)II, (long)FACTOR, PROGRESS); // 100% Complete

      if (CODE < 2) /* File is Complex DCDFT so update header information */
         {
         FileHeader.type = X_Data;        /* Output File is Complex */
         FileHeader.m = SLOPE;
         FileHeader.b = TRANGE[0];
         Zputhead(OUTSTR,&FileHeader);
         }

      fcloseall();
      
      if (CODE != 3) ERRFLAG = zNameOutputFile(OUTFILE,TMPFILE);

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
   short FFLAG=0;
   long numDataRecords;

// Need to initialize the globals in case we have wild card file names and make multiple passes
   IDATA = 0.;
   N = 0.;
   TOTAL = 0.;
   FLAG = 0;
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
      zTaskMessage(2,"Data file too large for available memopry. Processing from file (which might take a while).\n");

   TCNT = 0.;

   while ((NDAT = zGetData(numDataRecords, INSTR, dataBuffer, FileHeader.type)))
      {
      NUMDAT = NDAT;                                 // Should be the same as numDataRecords, but better safe than sorry
      
      RDataPntr =  (struct RData *)dataBuffer;       /* Setup some data pointers */
      TRDataPntr = (struct TRData *)dataBuffer;
      XDataPntr =  (struct XData *)dataBuffer;
      TXDataPntr = (struct TXData *)dataBuffer;

      for (I=0; I<NUMDAT; ++I)           /* Iterate over data buffer */
         {
         switch (FileHeader.type)
            {
            case R_Data:
               TIME = TCNT * FileHeader.m + FileHeader.b;
               ++TCNT;
               RDATA = RDataPntr->y;
               FLAG = RDataPntr->f;
               ++RDataPntr;             /* Advance to next vale */
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

   if (N < 2)
      {
      zTaskMessage(10,"** ERROR ** File Contains only %lG Point(s)\n",N);
      BombOff(1);
      }
/*
** If the data were read in 1 pass then the entire data base can
** be processed in memory (i.e. it is SMALL), otherwise the
** file must be processed from disc, which takes a great deal
** more time.
*/

   RMEAN /= N;
   IMEAN /= N;
   SQNO2 = sqrt(N/2.);
   SQNO8 = sqrt(N/8.);
   zTaskMessage(3,"File Contains %lG Points\n",N);
   zTaskMessage(3,"Mean Value = %lG + i(%lG)\n",RMEAN, IMEAN);

   if (CODE > 1) Nu = TRANGE[0];

   if (TRANGE[0] >= TRANGE[1])
      {
      TRANGE[0] = 1./(TIME-FIRST);
      TRANGE[1] = 1./DELT;
      }
      
   zTaskMessage(3,"Fundamental Frequency = %lG\n",1./(TIME-FIRST));
   zTaskMessage(3,"Nyquist Frequency = %lG\n",1./(2. * DELT));

   return;
   }

/***************************************************************
**
** Process "small" data files
*/
void MEMREDUCE()
   {
   long I;
/*
** First we must determine the inner product sets
** H0H1, H0H2, a1 and a2
*/
   TRDataPntr = (struct TRData *)dataBuffer;
   TXDataPntr = (struct TXData *)dataBuffer;

   TCNT = 0.;

   for (I=0L; I<NUMDAT; ++I)
      {
      switch (FileHeader.type)
         {
         case R_Data:
            TIME = TCNT * FileHeader.m + FileHeader.b;
            ++TCNT;
            break;
         case TR_Data:
            TIME = TRDataPntr->t;
            ++TRDataPntr;
            break;
         case X_Data:
            TIME = TCNT * FileHeader.m + FileHeader.b;
            ++TCNT;
            break;
         case TX_Data:
            TIME = TXDataPntr->t;
            ++TXDataPntr;
            break;
         }

      TIME *= Omega;
      H1 = cos(TIME);
      H2 = sin(TIME);

      H0H1 += H1;
      H0H2 += H2;
      H1H1 += (H1*H1);
      H2H2 += (H2*H2);
      H1H2 += (H1*H2);
      }

   H0H1SQ = Square(H0H1);
   H0H2SQ = Square(H0H2);

   a1 = H1H1 - H0H1SQ/N;
   if (a1 > 0.)
      a1 = 1./sqrt(a1);
   else
      a1 = 0.;

   a2 = H2H2 - H0H2SQ/N - Square(a1)*(Square(H1H2) +
               H0H1SQ*H0H2SQ/(N*N) - 2*H0H1*H0H2*H1H2/N);

   if (a2 > 0.)
      a2 = 1./sqrt(a2);
   else
      a2 = 0.;

   a1H0H1ON = a1 * H0H1/N;
/*
** Now we must determine the inner product set h1H2
*/
   TRDataPntr = (struct TRData *)dataBuffer;
   TXDataPntr = (struct TXData *)dataBuffer;

   TCNT = 0.;

   for (I=0L; I<NUMDAT; ++I)
      {
      switch (FileHeader.type)
         {
         case R_Data:
            TIME = TCNT * FileHeader.m + FileHeader.b;
            ++TCNT;
            break;
         case TR_Data:
            TIME = TRDataPntr->t;
            ++TRDataPntr;
            break;
         case X_Data:
            TIME = TCNT * FileHeader.m + FileHeader.b;
            ++TCNT;
            break;
         case TX_Data:
            TIME = TXDataPntr->t;
            ++TXDataPntr;
            break;
         }
      TIME *= Omega;
      h1H2 += (a1 * cos(TIME) - a1H0H1ON) * sin(TIME);
      }
/*
** Now determine the output values
*/
   RDataPntr =  (struct RData *)dataBuffer;
   TRDataPntr = (struct TRData *)dataBuffer;
   XDataPntr =  (struct XData *)dataBuffer;
   TXDataPntr = (struct TXData *)dataBuffer;
   TCNT = 0.;
   for (I=0;I<NUMDAT;++I)
      {
      switch (FileHeader.type)
         {
         case R_Data:
            TIME = TCNT * FileHeader.m + FileHeader.b;
            RDATA = RDataPntr->y;
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

      TIME *= Omega;
      h1 = a1 * (cos(TIME) - H0H1/N);
      h2 = a2*(sin(TIME) - H0H2/N - h1*h1H2);

      switch (CODE)
         {
         case 0: /* DCDFT */
            XDataOut.z.x += ((RDATA * h1) + (IDATA * h2));
            XDataOut.z.y += ((IDATA * h1) - (RDATA * h2));
            break;
         case 1: /* IDCDFT */
            XDataOut.z.x += ((RDATA * h1) - (IDATA * h2));
            XDataOut.z.y += ((IDATA * h1) + (RDATA * h2));
            break;
         default: /* Filter */
            XDataOut.z.x += (RDATA * h1);
            XDataOut.z.y += (RDATA * h2);
         }
      }

   if (CODE==2)    /* Filter Data */
      {
      RDataPntr =  (struct RData *)dataBuffer;
      TRDataPntr = (struct TRData *)dataBuffer;

      TCNT = 0.;

      for (I=0L; I<NUMDAT; ++I)
         {
         switch (FileHeader.type)
            {
            case R_Data:
               TIME = TCNT * FileHeader.m + FileHeader.b;
               ++TCNT;
               break;
            case TR_Data:
               TIME = TRDataPntr->t;
               break;
            }

         TIME *= Omega;
         h1 = a1 * (cos(TIME) - H0H1/N);
         h2 = a2*(sin(TIME) - H0H2/N - h1*h1H2);

         switch (FileHeader.type)
            {
            case R_Data:
               RDataPntr->y -= (XDataOut.z.x*h1 + XDataOut.z.y*h2);
               ++RDataPntr;
               break;
            case TR_Data:
               TRDataPntr->y -= (XDataOut.z.x*h1 + XDataOut.z.y*h2);
               ++TRDataPntr;
               break;
            }
         } /* END for I */

      zPutData(NUMDAT, OUTSTR, (char *)dataBuffer, FileHeader.type);
      }
   return;
   }

/***************************************************************
**
** Process "large" data files
*/
short DSKREDUCE()
   {
/*
** First we must determine the inner product sets
** H0H1, H0H2, a1 and a2
*/
   TCNT = 0.;

   if (!Zgethead(INSTR,(struct FILEHDR *)NULL)) return(1);

   while (Zread(INSTR,dataBuffer,FileHeader.type))
      {
      switch (FileHeader.type)
         {
         case R_Data:
            TIME = TCNT * FileHeader.m + FileHeader.b;
            ++TCNT;
            break;
         case TR_Data:
            TIME = TRDataPntr->t;
            break;
         case X_Data:
            TIME = TCNT * FileHeader.m + FileHeader.b;
            ++TCNT;
            break;
         case TX_Data:
            TIME = TXDataPntr->t;
            break;
         }

      TIME *= Omega;
      H1 = cos(TIME);
      H2 = sin(TIME);
      H0H1 += H1;
      H0H2 += H2;
      H1H1 += (H1*H1);
      H2H2 += (H2*H2);
      H1H2 += (H1*H2);
      }

   H0H1SQ = Square(H0H1);
   H0H2SQ = Square(H0H2);

   a1 = H1H1 - H0H1SQ/N;
   if (a1 > 0.)
      a1 = 1./sqrt(a1);
   else
      a1 = 0.;
   a2 = H2H2 - H0H2SQ/N - Square(a1)*(Square(H1H2) +
               H0H1SQ*H0H2SQ/(N*N) - 2*H0H1*H0H2*H1H2/N);
   if (a2 > 0.)
      a2 = 1./sqrt(a2);
   else
      a2 = 0.;

   a1H0H1ON = a1 * H0H1/N;
/*
** Now we must determine the inner product set h1H2
*/
   TCNT = 0.;

   if (!Zgethead(INSTR,(struct FILEHDR *)NULL)) return(1);

   while (Zread(INSTR,dataBuffer,FileHeader.type))
      {
      switch (FileHeader.type)
         {
         case R_Data:
            TIME = TCNT * FileHeader.m + FileHeader.b;
            ++TCNT;
            break;
         case TR_Data:
            TIME = TRDataPntr->t;
            break;
         case X_Data:
            TIME = TCNT * FileHeader.m + FileHeader.b;
            ++TCNT;
            break;
         case TX_Data:
            TIME = TXDataPntr->t;
            break;
         }

      TIME *= Omega;
      h1H2 += (a1 * cos(TIME) - a1H0H1ON) * sin(TIME);
      }
/*
** Now determine the output values
*/
   TCNT = 0.;

   if (!Zgethead(INSTR,(struct FILEHDR *)NULL)) return(1);

   while (Zread(INSTR,dataBuffer,FileHeader.type))
      {
      switch (FileHeader.type)
         {
         case R_Data:
            TIME = TCNT * FileHeader.m + FileHeader.b;
            RDATA = RDataPntr->y;
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
            ++TCNT;
            break;
         case TX_Data:
            TIME = TXDataPntr->t;
            RDATA = TXDataPntr->z.x;
            IDATA = TXDataPntr->z.y;
            break;
         }

      TIME *= Omega;
      h1 = a1 * (cos(TIME) - H0H1/N);
      h2 = a2*(sin(TIME) - H0H2/N - h1*h1H2);

      switch (CODE)
         {
         case 0: /* DCDFT */
            XDataOut.z.x += ((RDATA * h1) + (IDATA * h2));
            XDataOut.z.y += ((IDATA * h1) - (RDATA * h2));
            break;
         case 1: /* IDCDFT */
            XDataOut.z.x += ((RDATA * h1) - (IDATA * h2));
            XDataOut.z.y += ((IDATA * h1) + (RDATA * h2));
            break;
         default: /* Filter */
            XDataOut.z.x += (RDATA * h1);
            XDataOut.z.y += (RDATA * h2);
         }
      } /* End WHILE */

   if (CODE==2)    /* Filter Data */
      {
      if (!Zgethead(INSTR,(struct FILEHDR *)NULL)) return(1);

      TCNT = 0.;

      while (Zread(INSTR,dataBuffer,FileHeader.type))
         {
         switch (FileHeader.type)
            {
            case R_Data:
               TIME = TCNT * FileHeader.m + FileHeader.b;
               ++TCNT;
               break;
            case TR_Data:
               TIME = TRDataPntr->t;
               break;
            }
         TIME *= Omega;
         h1 = a1 * (cos(TIME) - H0H1/N);
         h2 = a2*(sin(TIME) - H0H2/N - h1*h1H2);
         switch (FileHeader.type)
            {
            case R_Data:
               RDataPntr->y -= (XDataOut.z.x*h1 + XDataOut.z.y*h2);
               break;
            case TR_Data:
               TRDataPntr->y -= (XDataOut.z.x*h1 + XDataOut.z.y*h2);
               break;
            }
         Zwrite(OUTSTR,(char *)dataBuffer,FileHeader.type);
         } /* END while */
      }

   return(0);
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

