/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task CANDY
*
* This template is for processing files directly from disk.
* Data values are read in one at a time, processed,
* and then written out. Files of any size can be managed since
* little memory is required.
*
* The default outclass is cdy
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

double processRealData(double timeVal, double Rval);
struct complex processComplexData(double timeVal, struct complex Zval);
void processCodeZero(short type, double timeVal, double *Rval, struct complex *Zval);

char tempfileName[_MAX_PATH];

FILE *infileStream  = (FILE *)NIL;
FILE *outfileStream = (FILE *)NIL;

const char szTask[]="CANDY";

int main(int argc, char *argv[])
   {
   struct FILEHDR FileHeader;
   char buffer[sizeof(struct TXData)];
//   struct RData *RDataPntr;
//   struct TRData *TRDataPntr;
//   struct XData *XDataPntr;
//   struct TXData *TXDataPntr;
   double Rval;
   struct complex Zval;
   short flag=0;
   char infileName[_MAX_PATH], outfileName[_MAX_PATH];
   double timeVal, timeIndex=0.;
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;
   
   signal(SIGINT,BREAKREQ);
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);

//   RDataPntr =  (struct  RData *)buffer; // Different interpretations of the buffer if we need them
//   TRDataPntr = (struct TRData *)buffer;
//   XDataPntr =  (struct  XData *)buffer;
//   TXDataPntr = (struct TXData *)buffer;

   if (!OUTCLASS[0]) strcpy(OUTCLASS,"cdy"); // Set default values

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

      zTaskMessage(2,"Opening Scratch File '%s'\n",tempfileName);

      if ((outfileStream = zOpen(tempfileName,O_writeb)) == NULL) Zexit(1);

      if (Zputhead(outfileStream,&FileHeader)) BombOff(1);

      while (Zread(infileStream,buffer,FileHeader.type))
         {
         if (extractValues(buffer, &FileHeader, timeIndex, &timeVal, &Rval, &Zval, &flag))
            {
            zTaskMessage(10,"Invalid Data Type.\n");
            BombOff(1);
            }

         if (!flag)     // Only process good data
            {
            switch (CODE)  // Assume CODE controls what gets done.
               {
               case 0:
                  processCodeZero(FileHeader.type, timeVal, &Rval, &Zval);
                  break;
               default:
                  zTaskMessage(10,"Invalide CODE Value.\n");
                  BombOff(1);
                  break;
               } /* switch (CODE) */
            } /* if (!flag) */

         insertValues(buffer, &FileHeader, timeVal, Rval, Zval, flag);

         if (Zwrite(outfileStream,buffer,FileHeader.type)) BombOff(1);

         ++timeIndex;           // Must update after all the processing...
         } /* end while */

      if (ferror(infileStream)) BombOff(1);

      zTaskMessage(2,"%ld points processed.\n",(long)timeIndex);

      fcloseall();

      ERRFLAG = zNameOutputFile(outfileName,tempfileName);
      } // for (iwc = 0, pWild = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

void processCodeZero(short type, double timeVal, double *Rval, struct complex *Zval)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = processRealData(timeVal, *Rval);
         break;
      case X_Data:
      case TX_Data:
         *Zval = processComplexData(timeVal, *Zval);
         break;
      }
   return;
   }

double processRealData(double timeVal, double Rval)
   {
   return(Rval);
   }

struct complex processComplexData(double timeVal, struct complex Zval)
   {
   return(Zval);
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
