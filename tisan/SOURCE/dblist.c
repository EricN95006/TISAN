/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task DBLIST
*
* I List the contents of a data base, including flagged values.
*
* CODE 0: Simple Single Column Output
* CODE 1: Multiple Column Output in FACTOR Columns
* CODE 2: Tabular Output with FACTOR columns
*
* FACTOR columns (see CODE)
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

const char szTask[] = "DBLIST";

int main(int argc, char *argv[])
   {
   char DataBuffer[sizeof(struct TXData)];        /* Largest Data Structure */
   char szInputFileName[_MAX_PATH];
   double Rval, DataTime, TimeCount=0.;
   struct complex Zval;
   int iColumnCount, iColBaseCount;

   struct FILEHDR FileHeader;
//   struct RData  *RDataPntr;
//   struct TRData *TRDataPntr;
//   struct TXData *TXDataPntr;
//   struct XData  *XDataPntr;
   short flag;
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;
   
   signal(SIGINT,BREAKREQ);
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);

//   RDataPntr  = (struct RData *)DataBuffer;
//   TRDataPntr = (struct TRData *)DataBuffer;
//   XDataPntr  = (struct XData *)DataBuffer;
//   TXDataPntr = (struct TXData *)DataBuffer;

/*
** Check CODE value if applicable
*/
   if ((CODE < 0) || (CODE > 2)) CODE = 0;
   iColumnCount = Max(FACTOR,1);

   zBuildFileName(M_inname,szInputFileName);
   CatList = ZCatFiles(szInputFileName);   // Find Matching Files for the input file specification
   if (CatList->N == 0) Zexit(1);     // Quit if there are none

   for (iwc = 0, pWild = CatList->pList;
        (iwc < CatList->N) && !ERRFLAG;
        ++iwc, pWild = strchr(pWild,'\0') + 1)
      {
      splitPath(pWild,Drive,Dir,Fname,Ext);
      strcpy(INNAME,Fname);
      strcpy(INCLASS,Ext);

      zBuildFileName(M_inname,szInputFileName);
      zTaskMessage(2,"\n");
      zTaskMessage(2,"Opening Input File '%s'\n",szInputFileName);

      if ((pFile = zOpen(szInputFileName,O_readb)) == (FILE*)NIL) Zexit(1);

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

      if (!strlen(YFORMAT)) strcpy(YFORMAT,"%lG "); /* Default output formats */
      if (!strlen(TFORMAT)) strcpy(TFORMAT,"%lG ");

      if (strlen(TITLE))  zTaskMessage(4,"%s\n",TITLE);
      if (strlen(TLABEL)) zTaskMessage(4,"%s\n",TLABEL);
      if (strlen(YLABEL)) zTaskMessage(4,"%s\n",YLABEL);

      TimeCount = 0.;

      iColumnCount = iColBaseCount = (int)FACTOR;
      zTaskMessage(4,"");

      while (Zread(pFile, DataBuffer, FileHeader.type))
         {
         if (extractValues(DataBuffer, &FileHeader, TimeCount, &DataTime, &Rval, &Zval, &flag))
            {
            zTaskMessage(10,"Unknown Data Type.\n");
            BombOff(1);
            }

         if (!flag && ((((DataTime <= TRANGE[1]) && (DataTime >= TRANGE[0])) || (TRANGE[0] >= TRANGE[1]))))
            {
            switch (CODE)
               {
               case 0: /* Simple 2-Column Format */
                  switch (FileHeader.type)
                     {
                     case R_Data:
                     case TR_Data:
                        zMessage(5,TFORMAT,DataTime);
                        zMessage(5,YFORMAT,Rval);
                        break;
                     case X_Data:
                     case TX_Data:
                        zMessage(5,TFORMAT,DataTime);
                        zMessage(5,YFORMAT,Zval.x);
                        zMessage(5,YFORMAT,Zval.y);
                        break;
                     }
                  zMessage(5,"\n%-8s: ",szTask);
                  break;
               case 1:
                  switch (FileHeader.type)
                     {
                     case R_Data:
                     case TR_Data:
                        zMessage(5,TFORMAT,DataTime);
                        zMessage(5,YFORMAT,Rval);
                        break;
                     case X_Data:
                     case TX_Data:
                        zMessage(5,TFORMAT,DataTime);
                        zMessage(5,YFORMAT,Zval.x);
                        zMessage(5,YFORMAT,Zval.y);
                        break;
                     }
                  if (!(--iColumnCount))
                     {
                     zMessage(5,"\n%-8s: ",szTask);
                     iColumnCount = iColBaseCount;
                     }
                  break;
               case 2:
                  switch (FileHeader.type)
                     {
                     case R_Data:
                     case TR_Data:
                        if (iColumnCount == iColBaseCount)
                           zMessage(5,TFORMAT,DataTime);
                        zMessage(5,YFORMAT,Rval);
                        break;
                     case X_Data:
                     case TX_Data:
                        if (iColumnCount == iColBaseCount)
                           zMessage(5,TFORMAT,DataTime);
                        zMessage(5,YFORMAT,Zval.x);
                        zMessage(5,YFORMAT,Zval.y);
                        break;
                     }
                  if (!(--iColumnCount))
                     {
                     zMessage(5,"\n%-8s: ",szTask);
                     iColumnCount = iColBaseCount;
                     }
                  break;
               }  /* End Switch */
            } /* End IF */

         ++TimeCount;
         } // while (Zread(pFile, DataBuffer, FileHeader.type))

      if (ferror(pFile)) ERRFLAG = 1;

      zMessage(5,"\n%-8s: \n",szTask);

      ERRFLAG = Zclose(pFile) ? 1 : ERRFLAG;
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

void BombOff(int a)
   {
   fcloseall();
   ZCatFiles((char*)NIL); // free catalog memory
   Zexit(a);
   }

void fcloseall()
   {
   if (pFile) Zclose(pFile);
   pFile = (FILE *)NIL;
   return;
   }

