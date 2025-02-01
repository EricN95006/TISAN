/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task IMEAN
*
*  Task to display basic statistics on a data file, with the option
*  to return information in the IMEAN.INP file.
*
*  CODE 0 (default) -> No Value Returned
*  CODE 1 -> FACTOR = -DataMean, ZFACTOR = -ComplexMean
*  CODE 2 -> FACTOR = LOWVAL, ZFACTOR = ComplexMin
*            TRANGE = T +/- 1 Point
*  CODE 3 -> FACTOR = HIGHVAL, ZFACTOR = ComplexMax
*            TRANGE = T +/- 1 Point
*
* 8/12/2017 Fixed improper initialization of the Range Min and Max values. Moved it inside the bRangeFistPass flag test (was outside).
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

FILE *pFile = (FILE *)NIL;

const char szTask[]="IMEAN";

int main(int argc, char *argv[])
   {
   struct FILEHDR FileHeader;
   double TimeCount=0.,DataTime, DataCount=0.;
   double FlaggedCount=0., DataCountInRange=0.;
   double FileDataMin , FileDataMax , RangeDataMin , RangeDataMax , Data;
   double FileStartTime, RangeTimeOfMax, RangeTimeOfMin;
   double RangeCountAtMin, RangeCountAtMax, DataSum=0., SumSquared=0.;
   double RangeMinTime, RangeMaxTime, FileMinTime, FileMaxTime;         // Smallest and largest time values
   double DataMean=0.0, SIGMA=0.0;
   double TLLAST, TLNEXT, THLAST, THNEXT, LastDataTime;
   short FLAG=0;
   BOOL bFileFirstPass = TRUE, bRangeFirstPass = TRUE;
   BOOL bMinJustFound, bMaxJustFound;
   char szFileName[_MAX_PATH];
   char DataBuffer[sizeof(struct TXData)];  /* Dimension to largest data size */
//   struct RData  *RDataPntr;
//   struct TRData *TRDataPntr;
//   struct XData  *XDataPntr;
//   struct TXData *TXDataPntr;
   struct complex ComplexMean, ComplexMin, ComplexMax, ComplexSum, ComplexData;
   BOOL unsorted = FALSE, duplicateAdjacentTiemStamps = FALSE;;
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;

   signal(SIGINT,BREAKREQ);  /* Set ^C Interrupt Vector */
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1); /* Initialize Task IMEAN */

//   RDataPntr  = (struct RData *)DataBuffer;
//   TRDataPntr = (struct TRData *)DataBuffer;
//   XDataPntr  = (struct XData *)DataBuffer;
//   TXDataPntr = (struct TXData *)DataBuffer;

   zBuildFileName(M_inname,szFileName);
   CatList = ZCatFiles(szFileName);   // Find Matching Files for the input file specification
   if (CatList->N == 0) Zexit(1);     // Quit if there are none

   for (iwc = 0, pWild = CatList->pList;
        (iwc < CatList->N) && !ERRFLAG;
        ++iwc, pWild = strchr(pWild,'\0') + 1)
      {
      splitPath(pWild,Drive,Dir,Fname,Ext);
      strcpy(INNAME,Fname);
      strcpy(INCLASS,Ext);

      zBuildFileName(M_inname,szFileName);      /* Build an input file name */
   /*
   ** Open the input file and read in the file header.
   ** Task dies if either opertation returns an error.
   */
      zTaskMessage(2,"\n");
      zTaskMessage(2,"Opening Input File '%s'\n", szFileName);

      if ((pFile = zOpen(szFileName,O_readb)) == (FILE*)NIL) Zexit(1);
      if (!Zgethead(pFile,&FileHeader)) BombOff(1);

      switch (FileHeader.type)
         {
         case R_Data:
            zTaskMessage(3,"Real Time Series File\n");
            break;
         case TR_Data:
            zTaskMessage(3,"Real Time Stamped Data File\n");
            break;
         case X_Data:
            zTaskMessage(3,"Complex Time Series File\n");
            break;
         case TX_Data:
            zTaskMessage(3,"Complex Time Stamped Data File\n");
            break;
         default:
            zTaskMessage(10,"Invalid file type\n");
            BombOff(1);
         }

      ComplexSum = cmplx(0., 0.);
      TimeCount = 0.;
      DataCount = 0.;
      FlaggedCount = 0.;
      DataCountInRange = 0.;
      DataSum = 0.;
      SumSquared = 0.;
      DataMean = 0.0;
      SIGMA = 0.0;
      FLAG = 0;
      bFileFirstPass = TRUE;
      bRangeFirstPass = TRUE;
      unsorted = FALSE;
      duplicateAdjacentTiemStamps = FALSE;

      while (Zread(pFile, DataBuffer, FileHeader.type))
         {
         if (extractValues(DataBuffer, &FileHeader, TimeCount, &DataTime, &Data, &ComplexData, &FLAG))
            {
            zTaskMessage(10,"Unknown File Type.\n");
            BombOff(1);
            }

         if (FLAG)            /* is data point valid ?                    */
            ++FlaggedCount;   /* If not, then count # of bad points */
         else                 /* Data point is good so do statistics      */
            {
            ++DataCount;         /* Number of valid points */

            if (bFileFirstPass)  /* First pass for file MIN/MAX scan */
               {
               FileDataMin = FileDataMax = Data;                                       /* Initialize Data Max and Min values */
               FileMinTime = FileMaxTime = LastDataTime = FileStartTime = DataTime;  /* Time range of file (minimum) */
               }
            else
               {
               FileDataMin = Min(FileDataMin,Data);      /* Find minimum value in file */
               FileDataMax = Max(FileDataMax,Data);      /* Find maximum value in file */
               FileMinTime = Min(FileMinTime, DataTime);
               FileMaxTime = Max(FileMaxTime, DataTime);
               }
   /*
   ** Only data within the selected time range are processed.
   ** If TRANGE[0] >= TRANGE[1] then all the data are used
   */
            if ((((DataTime <= TRANGE[1]) && (DataTime >= TRANGE[0])) || (TRANGE[0] >= TRANGE[1])))
               {
               switch (FileHeader.type)
                  {
                  case X_Data:   /* Complex Time Series */
                  case TX_Data:   /* Complex Time Labeled */
                     ComplexSum = c_add(ComplexSum,ComplexData);
                  }

               ++DataCountInRange;
               DataSum += Data;            /* Sum of values   */
               SumSquared += Square(Data);   /* Sums of squares */

               if (bRangeFirstPass)      /* First pass for range MIN/MAX scan */
                  {
                  RangeMinTime = RangeMaxTime = DataTime;
                  RangeDataMin = RangeDataMax = Data;
                  ComplexMin = ComplexMax = ComplexData;
                  TLLAST = TLNEXT = THLAST = THNEXT = RangeTimeOfMax = RangeTimeOfMin = DataTime;
                  RangeCountAtMax = RangeCountAtMin = TimeCount;
                  bMinJustFound = bMaxJustFound =  bRangeFirstPass = FALSE;
                  }
               else
                  {
                  RangeMinTime = Min(RangeMinTime, DataTime);
                  RangeMaxTime = Max(RangeMaxTime, DataTime);

                  if (bMinJustFound)       /* Minimum for selected range was found   */
                     {              /* on the last pass, so store "NEXT Time" */
                     bMinJustFound = FALSE;
                     TLNEXT = DataTime;
                     }

                  if (bMaxJustFound)       /* Maximum for selected range was found   */
                     {               /* on the last pass, so store "NEXT Time" */
                     bMaxJustFound = FALSE;
                     THNEXT = DataTime;
                     }

                  if (RangeDataMax < Data)   /* New Maximum value */
                     {
                     RangeDataMax = Data;    /* Range value max */
                     ComplexMax = ComplexData;
                     RangeTimeOfMax = DataTime;      /* Time of maximum */
                     RangeCountAtMax = TimeCount;      /* File entry number */
                     THLAST = LastDataTime; /* Save Time of Previous Point */
                     bMaxJustFound = TRUE;
                     }

                  if (Data < RangeDataMin)   /* New Minimum values */
                     {
                     RangeDataMin = Data;            /* Range value min */
                     ComplexMin = ComplexData;
                     RangeTimeOfMin = DataTime;      /* Time of minimum */
                     RangeCountAtMin = TimeCount;    /* File entry number */
                     TLLAST = LastDataTime;          /* Save Time of Previous Point */
                     bMinJustFound = TRUE;
                     }
                  }
               }

            if (bFileFirstPass)
               bFileFirstPass = FALSE;
            else
               {
               if (DataTime < LastDataTime) unsorted = TRUE;
               if (DataTime == LastDataTime) duplicateAdjacentTiemStamps = TRUE;
               }
            
            LastDataTime = DataTime;  /* Keep this DataTime for the next iteration */
            }

         ++TimeCount;        /* Count Number of Reads */
         } // while (Zread(pFile, DataBuffer, FileHeader.type))

      if (ferror(pFile))
         ERRFLAG = 1;
      else
         {
         if (unsorted)
            zTaskMessage(3,"** WARNING ** Data set is not sorted by time.\n");
         else
            zTaskMessage(3,"Time sorted data set.\n");

         if (duplicateAdjacentTiemStamps) zTaskMessage(3,"** WARNING ** Data set has duplicate time stamps.\n");

         if (!strlen(TFORMAT)) strcpy(TFORMAT,"%lG");  /* Setup default output */
         if (!strlen(YFORMAT)) strcpy(YFORMAT,"%lG");  /* formats if needed.   */

         zTaskMessage(3,"File Contains %ld Valid Points\n", (long)DataCount);
         if ((FileHeader.type == R_Data) || (FileHeader.type == X_Data)) zTaskMessage(3,"File Contains %ld Flagged Points\n", (long)FlaggedCount);
         zTaskMessage(3,"File Contains %ld Points in Selected Range\n", (long)DataCountInRange);
         zTaskMessage(3,"\n");

         if (DataCountInRange > 1.)
            SIGMA = ((DataCountInRange * SumSquared) - Square(DataSum))/(DataCountInRange*(DataCountInRange-1.));

         SIGMA = sqrt(Max(0.,SIGMA));

         if (DataCountInRange > 0.0)
            {
            DataMean = DataSum/DataCountInRange;
            ComplexMean.x = ComplexSum.x / DataCountInRange;
            ComplexMean.y = ComplexSum.y / DataCountInRange;
            }
   /*
   ** File wide information
   */
         zTaskMessage(3,"First/Last Time Stamp of File ");
         zMessage(3,TFORMAT,FileStartTime);
         zMessage(3,", ");
         zMessage(3,TFORMAT,DataTime);
         zMessage(3,"\n");
         zTaskMessage(3,"\n");

         zTaskMessage(3,"Full Time Range of File ");
         zMessage(3,TFORMAT,FileMinTime);
         zMessage(3,", ");
         zMessage(3,TFORMAT,FileMaxTime);
         zMessage(3,"\n");
         zTaskMessage(3,"\n");

         zTaskMessage(3,"Full Amplitude Range of File ");
         zMessage(3,YFORMAT,FileDataMin);
         zMessage(3,", ");
         zMessage(3,YFORMAT,FileDataMax);
         zMessage(3,"\n");
         zTaskMessage(3,"\n");

   /*
   ** Range specific information
   */
         zTaskMessage(3,"Selected Time Range ");
         zMessage(3,TFORMAT,RangeMinTime);
         zMessage(3,", ");
         zMessage(3,TFORMAT,RangeMaxTime);
         zMessage(3,"\n");
         zTaskMessage(3,"\n");

   // Min
         zTaskMessage(5,"Minimum Value in Selected Range ");
         zMessage(5,YFORMAT,RangeDataMin);
         zMessage(5,"\n");

         if ((FileHeader.type == X_Data) || (FileHeader.type == X_Data))
            zTaskMessage(5,"Z = %lG + i(%lG)\n",ComplexMin.x,ComplexMin.y);

         zTaskMessage(4,"at Entry %ld\n", (long)RangeCountAtMin);
         zTaskMessage(5,"Corresponding to Time ");
         zMessage(5,TFORMAT,RangeTimeOfMin);
         zMessage(5,"\n");
         zTaskMessage(3,"\n");

   // Max
         zTaskMessage(5,"Maximum Value in Selected Range ");
         zMessage(5,YFORMAT,RangeDataMax);
         zMessage(5,"\n");

         if ((FileHeader.type == X_Data) || (FileHeader.type == X_Data))
            zTaskMessage(5,"Z = %lG + i(%lG)\n",ComplexMax.x,ComplexMax.y);

         zTaskMessage(4,"at Entry %ld\n", (long)RangeCountAtMax);
         zTaskMessage(5,"Corresponding to Time ");
         zMessage(5,TFORMAT,RangeTimeOfMax);
         zMessage(5,"\n");
         zTaskMessage(3,"\n");

   // Mean and stdev
         zTaskMessage(6,"Mean Amplitude Value in Range:");
         zMessage(6,YFORMAT,DataMean);
         zMessage(6,"\n");

         if ((FileHeader.type == X_Data) || (FileHeader.type == X_Data))
            zTaskMessage(6,"Z = %lG + i(%lG)\n",ComplexMean.x,ComplexMean.y);

         zTaskMessage(6,"Standard Deviation of ");
         zMessage(6,YFORMAT,SIGMA);
         zMessage(6,"\n");

   // Now update the adverbs based on the code
         if (CODE)
            {
            if (!zGetAdverbs(TASKNAME)) /* Restore adverbs */
               {
               switch (CODE)           /* Return selected value (if any) */
                  {
                  case 1:
                     if ((FileHeader.type == X_Data) || (FileHeader.type == X_Data))
                        {
                        ZFACTOR[0] = -ComplexMean.x;
                        ZFACTOR[1] = -ComplexMean.y;
                        }
                     FACTOR = -DataMean;
                     break;
                  case 2:
                     TRANGE[0] = TLLAST;
                     TRANGE[1] = TLNEXT;
                     FACTOR = RangeDataMin;
                     if ((FileHeader.type == X_Data) || (FileHeader.type == X_Data))
                        {
                        ZFACTOR[0] = ComplexMin.x;
                        ZFACTOR[1] = ComplexMin.y;
                        }
                     break;
                  case 3:
                     TRANGE[0] = THLAST;
                     TRANGE[1] = THNEXT;
                     FACTOR = RangeDataMax;
                     if ((FileHeader.type == X_Data) || (FileHeader.type == X_Data))
                        {
                        ZFACTOR[0] = ComplexMax.x;
                        ZFACTOR[1] = ComplexMax.y;
                        }
                     break;
                  }
               if (zPutAdverbs(TASKNAME)) ERRFLAG = 1;
               } // if (CODE)
            }
         }

      ERRFLAG = Zclose(pFile) ? 1 : ERRFLAG;
      pFile = (FILE *)NIL;

      } // for (iwc = 0, pWild = CatList->pList;

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

void BombOff(int i)
   {
   if (pFile) Zclose(pFile);
   pFile = (FILE *)NIL;
   ZCatFiles((char*)NIL); // free catalog memory
   Zexit(i);
   }
