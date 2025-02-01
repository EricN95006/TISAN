/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task DBSORT
*
* Task to do a quick sort on time labeled data files
*
* By default this task overwrites the original input file.
*
* The entire file must fit into memory in order to be sorted.
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

int qsortCompare(const void *v1, const void *v2);

BYTE *pDataBuffer = (BYTE *)NIL;    // Pointer to all the data
struct FILEHDR FileHeader;

char tempfileName[_MAX_PATH];

FILE *infileStream  = (FILE *)NIL;
FILE *outfileStream = (FILE *)NIL;

const char szTask[]="DBSORT";

int main(int argc, char *argv[])
   {
//   char buffer[sizeof(struct TXData)];
//   struct TRData *TRDataPntr;
//   struct TXData *TXDataPntr;
   char infileName[_MAX_PATH], outfileName[_MAX_PATH];
   long lbyteCount, lDataCount;
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;
   
   signal(SIGINT,BREAKREQ);
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);

//   TRDataPntr = (struct TRData *)buffer;
//   TXDataPntr = (struct TXData *)buffer;

   zBuildFileName(M_inname,infileName);
   CatList = ZCatFiles(infileName);   // Find Matching Files for the input file specification
   if (CatList->N == 0) Zexit(1);     // Quit if there are none

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
      
      if (!Zgethead(infileStream,&FileHeader)) BombOff(1);

      if ((FileHeader.type != TR_Data) && (FileHeader.type != TX_Data))
         {
         zTaskMessage(5,"Only time labeled files can be sorted.\n");
         BombOff(1);
         }

      zTaskMessage(2,"Opening Scratch File '%s'\n",tempfileName);

      if ((outfileStream = zOpen(tempfileName,O_writeb)) == NULL) Zexit(1);

   /*
   ** Read the input file data into memory
   */
      lbyteCount = filesize(infileStream) - sizeof(struct FILEHDR); // Number of data bytes to read

      if (ferror(infileStream))
         {
         zTaskMessage(10,"Error getting file length.\n");
         zError();
         BombOff(1);
         }

      pDataBuffer = malloc(lbyteCount);

      if (!pDataBuffer)
         {
         zTaskMessage(10,"Memory allocation of %ld bytes failed.\n", lbyteCount);
         BombOff(1);
         }
      
      fread(pDataBuffer, 1, lbyteCount, infileStream);

      if (ferror(infileStream))
         {
         zTaskMessage(10,"Error reading file data.\n");
         zError();
         BombOff(1);
         }

      Zclose(infileStream);
      infileStream = (FILE *)NIL;
   /*
   ** Quick sort based on time
   */
         switch (FileHeader.type)
            {
            case TR_Data:
               lDataCount = lbyteCount / sizeof(struct TRData);
               zTaskMessage(1,"Sorting %ld values.\n", lDataCount);
               qsort(pDataBuffer, lDataCount, sizeof(struct TRData), qsortCompare);
               break;
            case TX_Data:
               lDataCount = lbyteCount / sizeof(struct TXData);
               zTaskMessage(1,"Sorting %ld values.\n", lDataCount);
               qsort(pDataBuffer, lDataCount, sizeof(struct TXData), qsortCompare);
               break;
            }

   /*
   ** Write data to the output file
   */
      if (Zputhead(outfileStream,&FileHeader)) BombOff(1);     // Ouput file is the same type as the input file, just a sorted version of it
      fwrite(pDataBuffer, 1, lbyteCount, outfileStream);

      if (ferror(outfileStream))
         {
         zTaskMessage(10,"Error writing file data.\n");
         zError();
         BombOff(1);
         }

      fcloseall();
      
      ERRFLAG = zNameOutputFile(outfileName,tempfileName);
      } // for (iwc = 0, pWild = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

/************************************************
**
** qsort comparison routine to sort based on time
*/
int qsortCompare(const void *v1, const void *v2)
   {
   double time1, time2;
   int returnValue;
   
   switch (FileHeader.type)
      {
      case TR_Data:
         time1 = ((struct TRData *)v1)->t;
         time2 = ((struct TRData *)v2)->t;
         break;
      case TX_Data:
         time1 = ((struct TXData *)v1)->t;
         time2 = ((struct TXData *)v2)->t;
         break;
      default:
         time1 = time2 = 0.;                          // needed to quite a compiler warning
         zTaskMessage(10, "Unknown data type.\n");
         BombOff(1);
      }

   if (time1 == time2)
      returnValue = 0;
   else if (time1 > time2)
      returnValue = 1;
   else
      returnValue = -1;
      
   return(returnValue);
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

   if (pDataBuffer) free(pDataBuffer);
   pDataBuffer = (BYTE *)NIL;
   
   return;
   }
