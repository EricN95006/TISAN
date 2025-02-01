/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task DBSUBSET
*
*  Task to take a subset of a data base by time and value.
*  For time labeled files, points that fall out of range are deleted.
*  For time series files, points are flagged as bad since their position is important, with the exception of CODE 0 which can just trim the ends off the file.
*
*  value (b), the Y-intercept, must be updated if leading points are removed in a time series file
*
* CODE 0: Delete values outside time range, exclusive
* CODE 1: Delete values inside time range, inclusive
* CODE 2: Delete values outside Amp range, exclusive 
* CODE 3: Delete values inside Amp range, inclusive
* CODE 4: Delete values outside both time range and amplitude range (this is an AND condition), exclusive
* CODE 5: Delete values inside both time range and amplitude range (this is an AND condition), inclusive
* CODE 6: Remove duplicate time stamps within the specified time range in time labeled files (ALL copies are removed)
*           if TRANGE[0] == TRANGE[1] then look at the entire data set
*           if TRANGE[0] < TRANGE[1] then only look inside that range (inclusive)
*           if TRANGE[0] > TRANGE[1] then only look outside that range (inclusive)
* CODE 7: Remove duplicate time stamps within the specified time range in sorted time labeled files (first copy is retained)
*           if TRANGE[0] == TRANGE[1] then look at the entire data set
*           if TRANGE[0] < TRANGE[1] then only look inside that range (inclusive)
*           if TRANGE[0] > TRANGE[1] then only look outside that range (inclusive)
* CODE 8: Starting at record P (zero indexed) save N records. P = TRANGE[1] and N = TRANGE[2]
*
* The duplicate removal using CODE 6 is done brute force and runs in N^2 time, so it can take a while for even modest sized files.
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

BOOL  deleteTimeSeriesOutsideTimeRange(double TIME);
short deleteTimeSeriesInsideTimeRange(double TIME, short flag);
short deleteTimeSeriesOutsideAmpRange(double TIME, short flag);
short deleteTimeSeriesInsideAmpRange(double TIME, short flag);
short deleteTimeSeriesOutsideTimeAndAmpRange(double TIME, double VALUE, short flag);
short deleteTimeSeriesInsideTimeAndAmpRange(double TIME, double VALUE, short flag);

void deleteOutsideTimeRange(double TIME, short dataType);
void deleteInsideTimeRange(double TIME, short dataType);
void deleteOutsideAmpRange(double VALUE, short dataType);
void deleteInsideAmpRange(double VALUE, short dataType);
void deleteOutsideTimeAndAmpRange(double TIME, double VALUE, short dataType);
void deleteInsideTimeAndAmpRange(double TIME, double VALUE, short dataType);

void deleteDuplicates(double TIME, short dataType);
void deleteSortedDuplicates(double TIME, short dataType);

void saveRecordsStartingatP(short dataType);
BOOL saveTimeSeriesRecordsStartingatP(void);

char TMPFILE[_MAX_PATH];  // Must be visible to BREAKREQ

FILE *INSTR  = (FILE *)NIL;
FILE *OUTSTR = (FILE *)NIL;
FILE *INSTR2  = (FILE *)NIL; // Used for removing duplicate time labels value (need to rescan file for every data point)

double WCNT=0., DCNT=0., TCNT=0., FCNT=0., TB;
struct FILEHDR FileHeader;
double TIME2, TCNT2;

char BUFFER[sizeof(struct TXData)]; // allocate for largest data type

const char szTask[]="DBSUBSET";

int main(int argc, char *argv[])
   {
   char INFILE[_MAX_PATH], OUTFILE[_MAX_PATH];
   double TIME, VALUE;
   short  FLAG;
   struct RData  *RDataPntr;
//   struct TRData *TRDataPntr;
   struct XData  *XDataPntr;
//   struct TXData *TXDataPntr;
   struct complex Zval;
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;
   
   signal(SIGINT,BREAKREQ);  /* Setup ^C Interrupt */
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);

   RDataPntr  = (struct  RData *)BUFFER;
//   TRDataPntr = (struct TRData *)BUFFER;
   XDataPntr  = (struct  XData *)BUFFER;
//   TXDataPntr = (struct TXData *)BUFFER;

   if ((CODE < 0) || (CODE > 8))
      {
      zTaskMessage(10,"CODE Value Out of Range.\n");
      Zexit(1);
      }

   if (((CODE == 0) || (CODE == 1) || (CODE == 4) || (CODE == 5)) && (TRANGE[0] >= TRANGE[1]))
      {
      zTaskMessage(10,"Invalid Time Range for CODE %d.\n", CODE);
      Zexit(1);
      }

   if (((CODE == 2) || (CODE == 3) || (CODE == 4) || (CODE == 5)) && (YRANGE[0] >= YRANGE[1]))
      {
      zTaskMessage(10,"Invalid Amplitude Range for CODE %d.\n", CODE);
      Zexit(1);
      }

   if ((CODE == 8) && ((TRANGE[0] < 0.) || (TRANGE[1] < 1.)))
      {
      zTaskMessage(10,"CODE = 8 Request to save %d records starting at record %d makes no sense\n", TRANGE[1], TRANGE[0]);
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

      zBuildFileName(M_inname,INFILE);    /* Build file names */
      zBuildFileName(M_outname,OUTFILE);
      zBuildFileName(M_tmpname,TMPFILE);

      zTaskMessage(2,"Opening Input File '%s'\n",INFILE);

      if ((INSTR = zOpen(INFILE,O_readb)) == NULL) Zexit(1);

      if  (!Zgethead(INSTR,&FileHeader)) BombOff(1);

      if ((CODE == 6) || (CODE == 7))
         {
         if ((FileHeader.type == R_Data) || (FileHeader.type == X_Data))
            {
            zTaskMessage(10,"Duplicate data points do not exist in time series files. There is nothing to do.\n");
            BombOff(1);
            }

         if (CODE == 6)
            {
            if ((INSTR2 = zOpen(INFILE,O_readb)) == NULL) BombOff(1);
            }
         }
         
      zTaskMessage(2,"Opening Scratch File '%s'\n",TMPFILE);
      if (((OUTSTR = zOpen(TMPFILE,O_writeb)) == NULL) ||
           Zputhead(OUTSTR,&FileHeader))
         BombOff(1);

      TB = FileHeader.b;  /* Save time intercept in case we change it */

      FCNT  = 0.;
      WCNT  = 0.;
      DCNT  = 0.;
      TCNT  = 0.;
      TCNT2 = 0.;

      printPercentComplete(0L, 0L, 0);

      while (Zread(INSTR,BUFFER,FileHeader.type))
         {
         if (extractValues(BUFFER, &FileHeader, TCNT, &TIME, &VALUE, &Zval, &FLAG))
            {
            zTaskMessage(10,"Unknown Data Type.\n");
            BombOff(1);
            }
         
         switch (FileHeader.type)
            {
            case R_Data:   /* Real time series */
               switch (CODE)
                  {
                  case 0:  /* Delete values outside time range */
                     if (deleteTimeSeriesOutsideTimeRange(TIME) && Zwrite(OUTSTR,BUFFER,R_Data)) BombOff(1); // relies on short circuit of &&
                     break;
                  case 1:  /* Delete values inside time range */
                     RDataPntr->f = deleteTimeSeriesInsideTimeRange(TIME, RDataPntr->f);
                     break;
                  case 2:  /* Delete values outside Amp range */
                     RDataPntr->f = deleteTimeSeriesOutsideAmpRange(VALUE, RDataPntr->f);
                     break;
                  case 3:  /* Delete values inside Amp range */
                     RDataPntr->f = deleteTimeSeriesInsideAmpRange(VALUE, RDataPntr->f);
                     break;
                  case 4:  /* Delete values outside both time and Amp range */
                     RDataPntr->f = deleteTimeSeriesOutsideTimeAndAmpRange(TIME, VALUE, RDataPntr->f);
                     break;
                  case 5:  /* Delete values inside both time and Amp range */
                     RDataPntr->f = deleteTimeSeriesInsideTimeAndAmpRange(TIME, VALUE, RDataPntr->f);
                     break;
                  case 8:
                     if (saveTimeSeriesRecordsStartingatP() && Zwrite(OUTSTR,BUFFER,R_Data)) BombOff(1); // relies on short circuit of &&
                     break;
                  }
               if (((CODE > 0) && (CODE < 6)) && Zwrite(OUTSTR,BUFFER,R_Data)) BombOff(1); // Short circuits, so does not try to write in that case
               break;

            case TR_Data:
               switch (CODE)
                  {
                  case 0:  /* Delete values outside time range */
                     deleteOutsideTimeRange(TIME, TR_Data);
                     break;
                  case 1:  /* Delete values inside time range */
                     deleteInsideTimeRange(TIME, TR_Data);
                     break;
                  case 2:  /* Delete values outside Amp range */
                     deleteOutsideAmpRange(VALUE, TR_Data);
                     break;
                  case 3:  /* Delete values inside Amp range */
                     deleteInsideAmpRange(VALUE, TR_Data);
                     break;
                  case 4:  /* Delete values outside time and Amp range */
                     deleteOutsideTimeAndAmpRange(TIME, VALUE, TR_Data);
                     break;
                  case 5:  /* Delete values inside time and Amp range */
                     deleteInsideTimeAndAmpRange(TIME, VALUE, TR_Data);
                     break;
                  case 6:  /* Delete duplicate values inside or outside (if T0 > T1) time range, look at all points if TRANGE[0] == TRANGE[1] */
                     deleteDuplicates(TIME, TR_Data);
                     break;
                  case 7:  /* Delete duplicate values (sorted) inside or outside (if T0 > T1) time range, look at all points if TRANGE[0] == TRANGE[1] */
                     deleteSortedDuplicates(TIME, TR_Data);
                     break;
                  case 8:
                     saveRecordsStartingatP(TR_Data);
                     break;
                  }
               break;

            case X_Data:
               switch (CODE)
                  {
                  case 0:  /* Delete values outside time range. */
                     if (deleteTimeSeriesOutsideTimeRange(TIME) && Zwrite(OUTSTR,BUFFER,X_Data)) BombOff(1); // relies on short circuit of &&
                     break;
                  case 1:  /* Delete values inside time range */
                     XDataPntr->f = deleteTimeSeriesInsideTimeRange(TIME, XDataPntr->f);
                     break;
                  case 2:  /* Delete values outside Amp range */
                     XDataPntr->f = deleteTimeSeriesOutsideAmpRange(VALUE, XDataPntr->f);
                     break;
                  case 3:  /* Delete values inside Amp range */
                     XDataPntr->f = deleteTimeSeriesInsideAmpRange(VALUE, XDataPntr->f);
                     break;
                  case 4:  /* Delete values outside time and Amp range */
                     XDataPntr->f = deleteTimeSeriesOutsideTimeAndAmpRange(TIME, VALUE, XDataPntr->f);
                     break;
                  case 5:  /* Delete values inside time and Amp range */
                     XDataPntr->f = deleteTimeSeriesInsideTimeAndAmpRange(TIME, VALUE, XDataPntr->f);
                     break;
                  case 8:
                     if (saveTimeSeriesRecordsStartingatP() && Zwrite(OUTSTR,BUFFER,X_Data)) BombOff(1); // relies on short circuit of &&
                     break;
                  }
               if (((CODE > 0) && (CODE < 6)) && Zwrite(OUTSTR,BUFFER,X_Data)) BombOff(1); // Short circuits, so does not try to write in that case
               break;

            case TX_Data:
               switch (CODE)
                  {
                  case 0:  /* Delete values outside time range */
                     deleteOutsideTimeRange(TIME, TX_Data);
                     break;
                  case 1:  /* Delete values inside time range */
                     deleteInsideTimeRange(TIME, TX_Data);
                     break;
                  case 2:  /* Delete values outside Amp range */
                     deleteOutsideAmpRange(VALUE, TX_Data);
                     break;
                  case 3:  /* Delete values inside Amp range */
                     deleteInsideAmpRange(VALUE, TX_Data);
                     break;
                  case 4:  /* Delete values outside time and Amp range */
                     deleteOutsideTimeAndAmpRange(TIME, VALUE, TX_Data);
                     break;
                  case 5:  /* Delete values inside time and Amp range */
                     deleteInsideTimeAndAmpRange(TIME, VALUE, TX_Data);
                     break;
                  case 6:  /* Delete duplicate values inside or outside (if T0 > T1) time range, look at all points if TRANGE[0] == TRANGE[1] */
                     deleteDuplicates(TIME, TX_Data);
                     break;
                  case 7:  /* Delete duplicate values (sorted) inside or outside (if T0 > T1) time range, look at all points if TRANGE[0] == TRANGE[1] */
                     deleteSortedDuplicates(TIME, TX_Data);
                     break;
                  case 8:
                     saveRecordsStartingatP(TX_Data);
                     break;
                  }
               break;
            }

         ++TCNT;                                /* Count data read. Must be updated after all the edits! */
         } /* End WHILE */

      FileHeader.b = TB;                       /* Update intercept */

      if ((Zputhead(OUTSTR,&FileHeader)) || (ferror(INSTR)))
         BombOff(1);
      else
         {
         zTaskMessage(2,"%ld Data Points Processed\n", (long)TCNT);
         zTaskMessage(3,"%ld Data Points Written\n",   (long)WCNT);
         switch (FileHeader.type)
            {
            case R_Data:  /* Only time series can have flagged points */
            case X_Data:
               zTaskMessage(3,"%ld Data Points Flagged\n", (long)FCNT);
            case TR_Data:
            case TX_Data:
               zTaskMessage(3,"%ld Data Points Deleted\n", (long)DCNT);
               break;
            }
         }

      fcloseall();

      ERRFLAG = zNameOutputFile(OUTFILE,TMPFILE);
      } // for (iwc = 0, pWild = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

/*
** Write TRANGE[1] records starting at record number TRANGE[0] (zero indexed)
*/
void saveRecordsStartingatP(short dataType)
   {
   if ((TCNT >= TRANGE[0]) && (TCNT < TRANGE[0]+TRANGE[1]))
      {
      ++WCNT;       // Count points written
      if (Zwrite(OUTSTR,BUFFER,dataType)) BombOff(1);
      }
   else
      ++DCNT;                       // Count deletes

   return;
   }

/*
** Save TRANGE[1] records in a time series starting at record TRANGE[0]
** We could do this by just indexing into the file, but that would require 
** a lot of restructuring in main(), and I'm not that ambitious.
*/
BOOL saveTimeSeriesRecordsStartingatP()
   {
   BOOL writeRecord = FALSE;

   if ((TCNT >= TRANGE[0]) && (TCNT < TRANGE[0]+TRANGE[1]))
      {
      if (!WCNT) TB = TRANGE[0]*FileHeader.m + FileHeader.b;  // only need to do this once
      ++WCNT;       // Count points written
      writeRecord = TRUE;
      }
else
      ++DCNT;                                                                // Count deleted points

   return(writeRecord);
   }

/*
** Removes points in a time series outside of a specified time range.
** Time series can easily be trimmed. Points outside the window are simply
** not written out. If the point is on the right side, then the Y-intercept
** value for the time base needs to be updated. Remember, the file is
** traversed from left to right, so any point on the left that is removed
** can be thought of as the "first" point. The function returns TRUE if
** the data value needs to be written out to the file.
*/
BOOL deleteTimeSeriesOutsideTimeRange(double TIME)
   {
   BOOL writeFlag = TRUE;

   if ((TIME < TRANGE[0]) || (TIME > TRANGE[1]))
      {
      writeFlag = FALSE;
      ++DCNT;                                                                // Count deleted points
      if (TIME < TRANGE[0]) TB = (TCNT + 1.)*FileHeader.m + FileHeader.b;    // Update the Y-intercept if needed
      }
   else
      {
      ++WCNT;       // Count points written
      }

   return(writeFlag);
   }

/*
** For time series files we set a flag to "delete" a point. All points get written out, just some are flagged as bad.
*/
short deleteTimeSeriesInsideTimeRange(double TIME, short flag)
   {
   if ((TIME >= TRANGE[0]) && (TIME <= TRANGE[1]))
      {
      flag = 1;
      ++FCNT;     // Count flagged points
      }

   ++WCNT;       // Count points written

   return(flag);
   }

short deleteTimeSeriesOutsideAmpRange(double VALUE, short flag)
   {
   if ((VALUE < YRANGE[0]) || (VALUE > YRANGE[1]))
      {
      flag = 1;
      ++FCNT;     // Count flagged points
      }

   ++WCNT;       // Count points written

   return(flag);
   }

short deleteTimeSeriesInsideAmpRange(double VALUE, short flag)
   {
   if ((VALUE >= YRANGE[0]) && (VALUE <= YRANGE[1]))
      {
      flag = 1;
      ++FCNT;     // Count flagged points
      }

   ++WCNT;       // Count points written

   return(flag);
   }

short deleteTimeSeriesOutsideTimeAndAmpRange(double TIME, double VALUE, short flag)
   {
   if (((TIME  < TRANGE[0]) || (TIME  > TRANGE[1])) &&
       ((VALUE < YRANGE[0]) || (VALUE > YRANGE[1])))
      {
      flag = 1;
      ++FCNT;     // Count flagged points
      }

   ++WCNT;       // Count points written

   return(flag);
   }

short deleteTimeSeriesInsideTimeAndAmpRange(double TIME, double VALUE, short flag)
   {
   if (((TIME  >= TRANGE[0]) && (TIME  <= TRANGE[1])) &&
       ((VALUE >= YRANGE[0]) && (VALUE <= YRANGE[1])))
      {
      flag = 1;
      ++FCNT;     // Count flagged points
      }

   ++WCNT;       // Count points written

   return(flag);
   }


void deleteOutsideTimeRange(double TIME, short dataType)
   {
   if ((TIME < TRANGE[0]) || (TIME > TRANGE[1]))
      {
      ++DCNT;                       // Count deletes
      }
   else
      {
      if (Zwrite(OUTSTR,BUFFER,dataType)) BombOff(1);
      ++WCNT;
      }

   return;
   }

void deleteInsideTimeRange(double TIME, short dataType)
   {
   if ((TIME >= TRANGE[0]) && (TIME <= TRANGE[1]))
      {
      ++DCNT;                       // Count deletes
      }
   else
      {
      if (Zwrite(OUTSTR,BUFFER,dataType)) BombOff(1);
      ++WCNT;
      }

   return;
   }

void deleteOutsideAmpRange(double VALUE, short dataType)
   {
   if ((VALUE < YRANGE[0]) || (VALUE > YRANGE[1]))
      {
      ++DCNT;                       // Count deletes
      }
   else
      {
      if (Zwrite(OUTSTR,BUFFER,dataType)) BombOff(1);
      ++WCNT;
      }

   return;
   }

void deleteInsideAmpRange(double VALUE, short dataType)
   {
   if ((VALUE >= YRANGE[0]) && (VALUE <= YRANGE[1]))
      {
      ++DCNT;                       // Count deletes
      }
   else
      {
      if (Zwrite(OUTSTR,BUFFER,dataType)) BombOff(1);
      ++WCNT;
      }

   return;
   }

void deleteOutsideTimeAndAmpRange(double TIME, double VALUE, short dataType)
   {
   if (((TIME  < TRANGE[0] ) || (TIME  > TRANGE[1] )) &&
       ((VALUE < YRANGE[0])  || (VALUE > YRANGE[1])))
      {
      ++DCNT;                       // Count deletes
      }
   else
      {
      if (Zwrite(OUTSTR,BUFFER,dataType)) BombOff(1);
      ++WCNT;
      }

   return;
   }

void deleteInsideTimeAndAmpRange(double TIME, double VALUE, short dataType)
   {
   if (((TIME  >= TRANGE[0] ) && (TIME  <= TRANGE[1] )) &&
       ((VALUE >= YRANGE[0])  && (VALUE <= YRANGE[1])))
      {
      ++DCNT;                       // Count deletes
      }
   else
      {
      if (Zwrite(OUTSTR,BUFFER,dataType)) BombOff(1);
      ++WCNT;
      }

   return;
   }

/*
** Brute force removal of duplicate time points. Both points are removed. Runs on order N^2, so it can take a while. Data need not be sorted.
*/
void deleteDuplicates(double TIME, short dataType)
   {
   double VALUE2;
   struct FILEHDR FileHeader2;
   short  FLAG2;
   struct complex Zval2;
   char BUFFER2[sizeof(struct TXData)];
   double duplicateCount;

   if ((TRANGE[0] == TRANGE[1])                                                  || // Look in all the data
       ((TRANGE[0] < TRANGE[1]) && ((TIME >= TRANGE[0]) && (TIME <= TRANGE[1]))) || // look inside T0 to T1 (inclusive)
       ((TRANGE[1] < TRANGE[0]) && ((TIME <= TRANGE[1]) || (TIME >= TRANGE[0]))))   // look outside T0 and T1 (inclusive)
      {
      if (!Zgethead(INSTR2,&FileHeader2)) BombOff(1);   // Rewind the second input stream of the data file
      duplicateCount = -1.;                             // There will be one match if there are no duplicates, so the first count goes to zero

      TCNT2 = 0.0;
      while (Zread(INSTR2,BUFFER2,FileHeader2.type)) // Loop through every record in the input file and look for TIME
         {
         if (extractValues(BUFFER2, &FileHeader2, TCNT2, &TIME2, &VALUE2, &Zval2, &FLAG2))
            {
            zTaskMessage(10,"Unknown Data Type in case 6.\n");
            BombOff(1);
            }
         if (TIME == TIME2) ++duplicateCount;
         ++TCNT2;
         }

      if (duplicateCount != 0.)
         {
         ++DCNT;                       /* Count deletes */
         }
      else
         {
         if (Zwrite(OUTSTR,BUFFER,dataType)) BombOff(1);
         ++WCNT;                       /* Count writes */
         }

      printPercentComplete((long)TCNT, (long)TCNT2, PROGRESS);
      }
   else  // Not in the range where we are looking, so keep it....
      {
      if (Zwrite(OUTSTR,BUFFER,dataType)) BombOff(1);
      ++WCNT;
      }

   return;
   }

/*
** Removal of duplicate time points from a sorted data set. The first data point is retained and all others with the same time stamp are removed.
** Since the data are sorted, this function runs in order N.
*/
void deleteSortedDuplicates(double TIME, short dataType)
   {
   double duplicateCount;

   if ((TRANGE[0] == TRANGE[1])                                                  || // Look in all the data
       ((TRANGE[0] < TRANGE[1]) && ((TIME >= TRANGE[0]) && (TIME <= TRANGE[1]))) || // look inside T0 to T1 (inclusive)
       ((TRANGE[1] < TRANGE[0]) && ((TIME <= TRANGE[1]) || (TIME >= TRANGE[0]))))   // look outside T0 and T1 (inclusive)
      {
      duplicateCount = 0.;

      if (TCNT2)
         {
         if (TIME2 == TIME) // Duplicate
            duplicateCount = 1.;
         else if (TIME2 > TIME) // TIME is the current point, TIME2 is the previous one
            {
            zTaskMessage(10,"Dataset not sorted and CODE=7 has been selected.\n");
            BombOff(1);
            }
         else
            TIME2 = TIME;
         }
      else
         TIME2 = TIME;

      ++TCNT2;

      if (duplicateCount != 0.)
         {
         ++DCNT;                       /* Count deletes */
         }
      else
         {
         if (Zwrite(OUTSTR,BUFFER,dataType)) BombOff(1);
         ++WCNT;
         }
      }
   else  // Not in the range where we are looking, so keep it....
      {
      if (Zwrite(OUTSTR,BUFFER,dataType)) BombOff(1);
      ++WCNT;
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

   if (INSTR2) Zclose(INSTR2);
   INSTR2 = (FILE *)NIL;

   return;
   }
