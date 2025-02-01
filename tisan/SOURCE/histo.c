/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task HISTO
*
* Make a histogram of the input file.
*
* Multiply the amplitude by FACTOR, round and then bin. Output file is a real time series.
* Time base intercept is the low integer value of the input data. Time base slope is 1/FACTOR.
*
* If FACTOR=0 it is set to 1. No other restrictions apply.
*
* Default outclass is 'hst'
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

void findMinMax(FILE *stream, struct FILEHDR *pHeader, double *minVal, double *maxVal);
void populateHistogram(FILE *stream, struct FILEHDR *pHeader, long minVal, long maxVal, struct RData *Histogram);

struct RData *pHisto = (struct RData *)NIL;

char tempfileName[_MAX_PATH];

FILE *infileStream  = (FILE *)NIL;
FILE *outfileStream = (FILE *)NIL;

const char szTask[]="HISTO";

int main(int argc, char *argv[])
   {
   struct FILEHDR FileHeader;
   struct FILEHDR histogramFileHeader;
   char infileName[_MAX_PATH], outfileName[_MAX_PATH];
   double minVal, maxVal;
   long numEntries;
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;
   
   signal(SIGINT,BREAKREQ);
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);

   if (!OUTCLASS[0]) strcpy(OUTCLASS,"hst"); // Set default values

   zBuildFileName(M_inname,infileName);
   CatList = ZCatFiles(infileName);    // Find Matching Files for the input file specification
   if (CatList->N == 0) Zexit(1);      // Quit if there are none

   for (iwc = 0, pWild = CatList->pList;
        (iwc < CatList->N) && !ERRFLAG;
        ++iwc, pWild = strchr(pWild,'\0') + 1)
      {
      splitPath(pWild,Drive,Dir,Fname,Ext);
      strcpy(INNAME,Fname);
      strcpy(INCLASS,Ext);

      zBuildFileName(M_inname,infileName);
      zBuildFileName(M_outname,outfileName);
      zBuildFileName(M_tmpname,tempfileName);

      zTaskMessage(2,"Opening Input File '%s'\n",infileName);
      if ((infileStream = zOpen(infileName,O_readb)) == NULL) Zexit(1);
      
      zTaskMessage(2,"Opening Scratch File '%s'\n",tempfileName);
      if ((outfileStream = zOpen(tempfileName,O_writeb)) == NULL) BombOff(1);
      
      if (!Zgethead(infileStream,&FileHeader)) BombOff(1);

      findMinMax(infileStream, &FileHeader, &minVal, &maxVal);

      if (ferror(infileStream)) BombOff(1);

      if (FACTOR == 0.0) FACTOR = 1.0;
      
      zTaskMessage(3,"File amplitude range = %lG, %lG\n", minVal, maxVal);
      
      minVal *= FACTOR;
      maxVal *= FACTOR;
      
      minVal = unbiasedRound(minVal);
      maxVal = unbiasedRound(maxVal);

      numEntries = (long)(maxVal - minVal) + 1L;
      
      if (numEntries == 1L)
         {
         zTaskMessage(4, "Scaled integer maximum equals minimum!\n", numEntries);
         BombOff(1);
         }

      zTaskMessage(4, "Allocaiting memory for %ld entries.\n", numEntries);

      pHisto =  (struct RData *)malloc(numEntries * (long)Zsize(R_Data));     // Data will be a real time series
      
      if (!pHisto)
         {
         zTaskMessage(10, "Memory allocation failure.\n");
         BombOff(1);
         }

      populateHistogram(infileStream, &FileHeader, (long)minVal, (long)maxVal, pHisto);

      histogramFileHeader.type = R_Data;
      histogramFileHeader.m = 1.0/FACTOR;
      histogramFileHeader.b = minVal;

      if (Zputhead(outfileStream,&histogramFileHeader)) BombOff(1);

      zPutData(numEntries, outfileStream, (char *)pHisto, R_Data);

      if (ferror(outfileStream)) BombOff(1);

      fcloseall();

      ERRFLAG = zNameOutputFile(outfileName,tempfileName);
      } // for (iwc = 0, pWild = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

/*
** Build the histogram by counting up how many times each value occurs.
** We are going to write out this data buffer "as is" which is why it is of type struct RData
*/
void populateHistogram(FILE *stream, struct FILEHDR *pHeader, long minVal, long maxVal, struct RData *Histogram)
   {
   char buffer[sizeof(struct TXData)];
   double Rval;
   struct complex Zval;
   double time, timeIndex=0.;
   short flag=0;
   long i, longVal;
   
   for (i = minVal; i <= maxVal; ++i)
      {
      Histogram[i - minVal].y = 0.0;
      Histogram[i - minVal].f = 0;
      }

   while (Zread(stream,buffer,pHeader->type))
      {
      if (extractValues(buffer, pHeader, timeIndex, &time, &Rval, &Zval, &flag))
         {
         zTaskMessage(10,"Invalid Data Type.\n");
         BombOff(1);
         }

      Rval *= FACTOR;
      longVal = (long)unbiasedRound(Rval);

      ++Histogram[longVal - minVal].y;

      ++timeIndex;           // Must update after all the processing... (it's not used here, but we want to be consistent)
      } /* end while */

   return;
   }

void findMinMax(FILE *stream, struct FILEHDR *pHeader, double *minVal, double *maxVal)
   {
   char buffer[sizeof(struct TXData)];
   double Rval;
   struct complex Zval;
   double time, timeIndex=0.;
   short flag=0;
   BOOL bFirstPass = TRUE;

   while (Zread(stream,buffer,pHeader->type))
      {
      if (extractValues(buffer, pHeader, timeIndex, &time, &Rval, &Zval, &flag))
         {
         zTaskMessage(10,"Invalid Data Type.\n");
         BombOff(1);
         }

      if (bFirstPass)
         {
         bFirstPass = FALSE;
         *minVal = *maxVal = Rval;
         }
      else
         {
         *minVal = Min(*minVal, Rval);
         *maxVal = Max(*maxVal, Rval);
         }

      ++timeIndex;           // Must update after all the processing...  (it's not used here, but we want to be consistent)
      } /* end while */


   if (!Zgethead(stream,pHeader)) BombOff(1);     // Return back to start of the data

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
   unlink(tempfileName);
   ZCatFiles((char*)NIL); // free catalog memory
   Zexit(a);
   }

void fcloseall()
   {
   if (infileStream) Zclose(infileStream);
   infileStream = (FILE *)NIL;
   
   if (outfileStream) Zclose(outfileStream);
   outfileStream = (FILE *)NIL;

   if (pHisto) free(pHisto);
   pHisto = (struct RData *)NIL;
   
   return;
   }
