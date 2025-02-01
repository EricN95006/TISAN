/*********************************************************************
*
*  Task DBBLOCK
*
* Task to accumulate data into blocks based on count or time interval
*
* Dr. Eric R. Nelson
* 12/28/2016
*
* CODE 0: Accumulate FACTOR count values. Data will be summed in runs
*         of FACTOR points. The time for the new accumulated point
*         will be the time of the first point in the run. For time series
*         files the header slope value will be multiplied by factor.
*         If the total number of points is not divisible by FACTOR,
*         then the incomplete run is discarded
*
* CODE != 0: Accumulate runs within a time interval. For example, if
*            FACTOR is 60 and the times units are seconds then runs
*            will be accumulated for 60 seconds. If a run falls short by
*            more than PARMS[0] time increments, then that run is discarded.
*
* The logic is simple enough where the code does the calculations for
* both the real and complex file types, regardless of what the file is.
* Granted, it does twice the number of computations needed, and half of
* those will be zero, but it makes the logic real easy to follow
* and implement. Since speed is not an issue, I went for readability.
*
* The infile of this task accepts wild cards
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

char tempfileName[_MAX_PATH];

FILE *infileStream  = (FILE *)NIL;
FILE *outfileStream = (FILE *)NIL;

const char szTask[]="DBBLOCK";

int main(int argc, char *argv[])
   {
   struct FILEHDR FileHeader;
   char buffer[sizeof(struct TXData)];
   double Rval;
   struct complex Zval;
   short flag=0;
   char infileName[_MAX_PATH], outfileName[_MAX_PATH];
   double time, timeIndex=0.;
   
   double blockRval, blockTime, blockTimeIndex = 0.0;
   struct complex blockZval;
   double blockCount = 0.0;
   
   double deltaTime = 0.0;
   long droppedBlocks = 0L;
   BOOL bSetBlockTime = TRUE;;

// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int I;
   char *pChar;

   signal(SIGINT,BREAKREQ);
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);

   FACTOR = (double)((long)FACTOR);    // Truncate
   if (FACTOR < 2.0)
      {
      zTaskMessage(10, "Invalid FACTOR.\n");
      BombOff(1);
      }
      
   if (!OUTCLASS[0]) strcpy(OUTCLASS,"blk"); // Set default values


   zBuildFileName(M_inname,infileName);
   CatList = ZCatFiles(infileName);   // Find Matching Files for the input file specification
   if (CatList->N == 0) Zexit(1);     // Quit if there are none

   for (I = 0, pChar = CatList->pList;
        (I < CatList->N) && !ERRFLAG;
        ++I, pChar = strchr(pChar,'\0') + 1)
      {
      splitPath(pChar,Drive,Dir,Fname,Ext);
      strcpy(INNAME,Fname);
      strcpy(INCLASS,Ext);

      zBuildFileName(M_inname,infileName);
      zBuildFileName(M_outname,outfileName);
      zBuildFileName(M_tmpname,tempfileName);

      zTaskMessage(2,"Opening Input File '%s'\n",infileName);
      if ((infileStream = zOpen(infileName,O_readb)) == NULL) Zexit(1);
   
      if (!Zgethead(infileStream,&FileHeader)) BombOff(1);

      zTaskMessage(2,"Opening Scratch File '%s'\n",tempfileName);

      if ((outfileStream = zOpen(tempfileName,O_writeb)) == NULL) Zexit(1);

      if (Zputhead(outfileStream,&FileHeader)) BombOff(1);


      flag = 0;
      timeIndex = 0.;
      blockTimeIndex = 0.0;
      blockCount = 0.0;
   
      deltaTime = 0.0;
      droppedBlocks = 0L;
      bSetBlockTime = TRUE;;

      while (Zread(infileStream,buffer,FileHeader.type))
         {
         if (extractValues(buffer, &FileHeader, timeIndex, &time, &Rval, &Zval, &flag))
            {
            zTaskMessage(10,"Invalid Data Type.\n");
            BombOff(1);
            }

         if (CODE) // FACTOR represent the size of a time window per block
            {
            if (bSetBlockTime)         // No base time has been set (default state), so set it now
               {
               bSetBlockTime = FALSE;
               blockTime = time;
               }
/*
** (time - blockTime) is the current size of the time difference between where we started this block
** and where we are now in reading the file. Remove FACTOR (the size of the desired time window)
** and we get a value, called deltaTime, that will be zero when the time window is reached. However
** We need to account for time jitter, so allow for going over or under by PARMS[0] time increments. In other words
** deltaTime = -2, -1, 0, 1, or 2 are all treated as deltaTime = 0 if PARMS[0] = 2.
*/
            deltaTime = (time - blockTime) - FACTOR;
         
            if ((deltaTime >= -PARMS[0]) && (deltaTime <= PARMS[0])) // We have a block! Current value is in NEXT block
               {
               insertValues(buffer, &FileHeader, blockTime, blockRval, blockZval, FALSE);
               if (Zwrite(outfileStream,buffer,FileHeader.type)) BombOff(1);

               ++blockTimeIndex;

               blockTime = time;

               if (!flag)
                  {
                  blockRval = Rval;
                  blockZval = Zval;
                  }
               else
                  {
                  blockRval  = 0.0;
                  blockZval  = cmplx(0.0, 0.0);
                  }
               }
            else if (deltaTime > PARMS[0])  // We have a large jump, so drop the current block & start from here
               {
               ++droppedBlocks;
               blockTime = time;

               if (!flag)
                  {
                  blockRval = Rval;
                  blockZval = Zval;
                  }
               else
                  {
                  blockRval  = 0.0;
                  blockZval  = cmplx(0.0, 0.0);
                  }
               }
            else  // Add this point to the sum
               {
               if (!flag)
                  {
                  blockRval += Rval;
                  blockZval = c_add(blockZval, Zval);
                  }
               }
            }
         else  // FACTOR is the number of data values to add for each block
            {
            if (blockCount == 0.) blockTime = time;

            if (!flag)     // Only add in good data, but all points count for the total.
               {
               blockRval += Rval;
               blockZval = c_add(blockZval, Zval);
               }
               
            ++blockCount;        // Number of values accumuated so far in this block
            
            if (blockCount == FACTOR)
               {
               insertValues(buffer, &FileHeader, blockTime, blockRval, blockZval, FALSE);
               if (Zwrite(outfileStream,buffer,FileHeader.type)) BombOff(1);
               ++blockTimeIndex;
               blockCount = 0.0;
               blockRval  = 0.0;
               blockZval  = cmplx(0.0, 0.0);
               }
            }

         ++timeIndex;           // Must update after all the processing...
         } /* end while */

      zTaskMessage(3,"Wrote out %ld values.\n", (long)blockTimeIndex);

      if (CODE)
         {
         if (droppedBlocks) zTaskMessage(3,"Dropped %ld block(s).\n", droppedBlocks);
         FileHeader.m = FACTOR;        // Time window size is new slope
         }
      else
         {
         if (blockCount) zTaskMessage(3,"%ld data value(s) dropped.\n", (long)blockCount);
         FileHeader.m *= FACTOR;       // Number of points times current slope
         }

      if (Zputhead(outfileStream,&FileHeader)) BombOff(1);

      if (ferror(infileStream)) BombOff(1);

      fcloseall();

      ERRFLAG = zNameOutputFile(outfileName,tempfileName);
      } // for (I = 0, pChar = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
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
   
   return;
   }
