/*********************************************************************
*
* Dr. Eric R. Nelson
* Ported to unix 12/28/2016
* Fixed 1-indexing memory error 12/10/2022
*
*  Task FFT
*
* Fast Fourier Transform
* See Ronald N. Bracewell "The Fourier Transform and its Applications"
*
* This code was ported from FORTRAN and so the data arras are one indexed.
*
* CODE 0 -> FFT
* CODE 1 -> IFFT
*
* Default outclass = fft
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

void InitializeFFT(void);
void FOUR1(int);

char BUFFER[sizeof(struct TRData)];
char INFILE[_MAX_PATH], OUTFILE[_MAX_PATH];
char TMPFILE[_MAX_PATH];
long N=0L;
double Imean=0., Rmean=0.;
struct FILEHDR FileHeader;
long  NUMDAT, NDAT;
struct RData  *RDataPntr;
struct XData  *XDataPntr;
struct XData  XDataOut;
double Nyquist, Fundamental;

double *dataBuffer = (double *)NIL;  // Holds X iY in adjacent cells

FILE *INSTR  = (FILE *)NIL;
FILE *OUTSTR = (FILE *)NIL;

double twoPI;

const char szTask[]="FFT";

int main(int argc, char *argv[])
   {
   unsigned int wI;
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;

   signal(SIGINT,BREAKREQ);
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1); /* Initialize Task */

   if ((CODE < 0) || (CODE > 1))
      {
      zTaskMessage(10,"Invalid CODE Specification\n");
      Zexit(1);
      }

   if (!OUTCLASS[0]) strcpy(OUTCLASS,"fft");

   twoPI = 2.0 * acos(-1.0);

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
      if (!INSTR) Zexit(1);;
      
      if (!Zgethead(INSTR,&FileHeader)) BombOff(1);

      InitializeFFT();

      FOUR1(CODE ? -1 : 1);

      zTaskMessage(2,"Opening Scratch File '%s'\n",TMPFILE);
      if ((OUTSTR = zOpen(TMPFILE,O_writeb)) == NULL) BombOff(1);

      if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);

      XDataOut.f = 0;
      for (wI = 1; wI <= (unsigned int)(2L * N); wI += 2) // write out the result starting at index 1.
         {
         XDataOut.z.x = dataBuffer[wI];
         XDataOut.z.y = dataBuffer[wI+1];

         if (!CODE)
            {
            XDataOut.z.y /= (double)(N / 2L);
            XDataOut.z.x /= (double)(N / 2L);
            }
         else
            {
            XDataOut.z.y /= 2.;
            XDataOut.z.x /= 2.;
            }

         if (Zwrite(OUTSTR,(char *)&XDataOut,X_Data)) BombOff(1);
         }

      FileHeader.type = X_Data;
      FileHeader.b = 0.;
      FileHeader.m = (2. * Nyquist - Fundamental) / (double)(N - 1L);

      if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);

      fcloseall();

      ERRFLAG = zNameOutputFile(OUTFILE,TMPFILE);
      } // for (iwc = 0, pWild = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

/************************************************************
**
** Initialize file stuff
** When Done, We know if the entire file can be processed
** in memory and how many data points there are (N).
*/
void InitializeFFT()
   {
   double RVAL, IVAL=0.;
   long L, lFLAGGED=0L;
   int FLAG;
   long numDataRecords;

// Need to initialize these globals in case we have wild cards in the file name and thus make multiple passes
   N = 0L;
   Imean = 0.;
   Rmean = 0.;
   
   numDataRecords = countDataRecords(INSTR);

   if (numDataRecords < 2L)
      {
      zTaskMessage(10,"File Contains Only %ld Point(s)\n",numDataRecords);
      BombOff(1);
      }

   zTaskMessage(3,"File Contains %ld Total Points\n",numDataRecords);

   L = 1L;
   while (L < numDataRecords) L *= 2L; // FFT must have a power of 2 number of records

   if (L != numDataRecords)
      {
      numDataRecords = L;
      zTaskMessage(9,"** WARNING ** Padding to %ld\n",L);
      }

/*
** This FFT was originally written in FORTRAN and ported to C. FORTRAN arrays are 1 indexed, and the
** indexing was left alone to avoid introducing bugs. As a result we need space for one more complex value.
** The 2L is needed since we are storing complex numbers (real and imaginary parts for each record).
*/
   dataBuffer = (double *)calloc((numDataRecords + 1L) * 2L, sizeof(double));

   if (!dataBuffer)
      {
      zTaskMessage(10,"Unable to allocate memory for %ld complex records.\n", numDataRecords);
      BombOff(1);
      }

   RDataPntr =  (struct RData *)BUFFER;
   XDataPntr =  (struct XData *)BUFFER;

   L = 1L;
   while (Zread(INSTR,BUFFER,FileHeader.type))
      {
      ++N;
      switch (FileHeader.type)
         {
         case R_Data:
            RVAL = RDataPntr->y;
            FLAG = RDataPntr->f;
            break;
         case X_Data:
            RVAL = XDataPntr->z.x;
            IVAL = XDataPntr->z.y;
            FLAG = RDataPntr->f;
            break;
         case TR_Data:
         case TX_Data:
            zTaskMessage(10,"Cannot perform an FFT on time labeled data files.\n");
            BombOff(1);
            break;
         default:
            zTaskMessage(10,"Unknown File Type.\n");
            BombOff(1);
         }

      if (FLAG)
         {
         RVAL = IVAL = 0.0;
         ++lFLAGGED;
         }

      dataBuffer[L]   = RVAL; // Set the real and imaginary values
      dataBuffer[L+1] = IVAL;
      L += 2L;                // Advances the the next complex value

      Imean += IVAL;
      Rmean += RVAL;
      } /* End WHILE */

   if (ferror(INSTR)) BombOff(1);

   zTaskMessage(3,"File Contains %ld Flagged Points\n",lFLAGGED);

   N = numDataRecords;  // N is used throughout the code because it is easier to read.
   
   Rmean /= (double)N;
   Imean /= (double)N;

   Nyquist     = 1./(2. * FileHeader.m);
   Fundamental = 1./((double)(N - 1L) * FileHeader.m);

   zTaskMessage(3,"Mean Value =  %lG + i(%lG)\n",Rmean,Imean);
   zTaskMessage(3,"Fundamental Frequency = %lG\n",Fundamental );
   zTaskMessage(3,"Nyquist Frequency = %lG\n",Nyquist);

   Zclose(INSTR);
   INSTR = (FILE *)NIL;

   return;
   }

void FOUR1(int ISIGN)
   {
   double WR,WI,WPR,WPI,WTEMP,THETA, TEMPR, TEMPI;
   long wN, J, I, M, MMAX, ISTEP;

   wN = 2L * N; // N is a global and is the same as numDataRecords and is used strictly for readability
   J = 1L;
   for (I=1L; I <= wN; I += 2L)
      {
      if (J > I)
         {
         TEMPR = dataBuffer[J];
         TEMPI = dataBuffer[J+1];
         dataBuffer[J]   = dataBuffer[I];
         dataBuffer[J+1] = dataBuffer[I+1];
         dataBuffer[I]   = TEMPR;
         dataBuffer[I+1] = TEMPI;
         }
      M=wN/2;
      while ((M >= 2L ) && (J > M))
         {
         J = J - M;
         M /= 2L;
         }
      J += M;
      }

   MMAX = 2L;

   while (wN > MMAX)
      {
      ISTEP = 2L * MMAX;
      THETA = twoPI / ((double)ISIGN * (double)MMAX);
      WPR = sin(THETA / 2.0);
      WPR *= (-2.0 * WPR);
      WPI = sin(THETA);
      WR = 1.0;
      WI = 0.0;

      for (M=1; M <= MMAX; M += 2L)
         {
         for (I=M; I<=wN; I+=ISTEP)
            {
            J = I + MMAX;

            TEMPR = dataBuffer[J]   * WR - dataBuffer[J+1] * WI;
            TEMPI = dataBuffer[J+1] * WR + dataBuffer[J]   * WI;

            dataBuffer[J]   = dataBuffer[I]   - TEMPR;
            dataBuffer[J+1] = dataBuffer[I+1] - TEMPI;
            dataBuffer[I]   = dataBuffer[I]   + TEMPR;
            dataBuffer[I+1] = dataBuffer[I+1] + TEMPI;
            }
         WTEMP = WR;
         WR = WR * WPR - WI    * WPI + WR;
         WI = WI * WPR + WTEMP * WPI + WI;
         }
       MMAX=ISTEP;
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

void fcloseall()
   {
   if (INSTR) Zclose(INSTR);
   INSTR = (FILE *)NIL;

   if (OUTSTR) Zclose(OUTSTR);
   OUTSTR = (FILE *)NIL;

   if (dataBuffer) free(dataBuffer);
   dataBuffer = (double *)NIL;

   return;
   }
