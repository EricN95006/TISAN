/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task DBX
*
* Task to extract part of a complex data base
*
* CODE 0: Extract Amplitude
* CODE 1: Extract Phase
* CODE 2: Extract Real Part
* CODE 3: Extract Imaginary Part
*
* Datatypes X_Data and TX_Data are allowed
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

char TMPFILE[_MAX_PATH];  /* Must be global to be visible to BREAKREQ */

FILE *INSTR  = (FILE *)NIL;
FILE *OUTSTR = (FILE *)NIL;

const char szTask[]="DBX"; // This name must match the file name

int main(int argc, char *argv[])
   {
   char INFILE[_MAX_PATH], OUTFILE[_MAX_PATH];
   char BUFFER[sizeof(struct TXData)], TMPTYPE;
   double TCNT=0.;
   double VALUE, TIME;
   short  FLAG;
   struct complex Z;
   struct FILEHDR FileHeader;
   struct RData  *RDataPntr;
   struct TRData *TRDataPntr;
//   struct XData  *XDataPntr;
//   struct TXData *TXDataPntr;
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;

   signal(SIGINT,BREAKREQ);   /* Setup ^C Interrupt handler */
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);

   RDataPntr  = (struct RData *)BUFFER;
   TRDataPntr = (struct TRData *)BUFFER;
//   XDataPntr  = (struct XData *)BUFFER;
//   TXDataPntr = (struct TXData *)BUFFER;

   if ((CODE < 0) || (CODE > 3))
      {
      zTaskMessage(10,"CODE adverb out of range (%d)\n",CODE);
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

      zBuildFileName(M_inname,INFILE);     /* Build filenames */
      zBuildFileName(M_outname,OUTFILE);
      zBuildFileName(M_tmpname,TMPFILE);

      zTaskMessage(2,"Opening Input File '%s'\n",INFILE);

      if ((INSTR = zOpen(INFILE,O_readb)) == NULL)  Zexit(1);

      if (!Zgethead(INSTR,&FileHeader)) BombOff(1);

   /*
   ** Select proper data type for new file
   */
      switch (TMPTYPE = FileHeader.type)
         {
         case X_Data:
            FileHeader.type = R_Data;
            break;
         case TX_Data:
            FileHeader.type = TR_Data;
            break;
         default:
            zTaskMessage(10,"Invalid File Type.\n");
            BombOff(1);
         }

      zTaskMessage(2,"Opening Scratch File '%s'\n",TMPFILE);

      if ((OUTSTR = zOpen(TMPFILE,O_writeb)) == NULL) BombOff(1);

      if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);

      FileHeader.type = TMPTYPE; /* Restore currnt type */

      while (Zread(INSTR,BUFFER,FileHeader.type))
         {
         if (extractValues(BUFFER, &FileHeader, TCNT, &TIME, &VALUE, &Z, &FLAG))
            {
            zTaskMessage(10,"Invalid Data Type.\n");
            BombOff(1);
            }

         switch (CODE)    /* Select action */
            {
            case 0:   /* Amplitude, which comes back from extractValues in VALUE */
               break;
            case 1:   /* Phase */
               VALUE = atan2(Z.y,Z.x);
               break;
            case 2:   /* Real Part */
               VALUE = Z.x;
               break;
            case 3:   /* Imaginary Part */
               VALUE = Z.y;
               break;
            }

         switch (FileHeader.type)  /* Write by data type */
            {
            case X_Data:              /* Complex time series maps */
               RDataPntr->y = VALUE;  /* into real time sderies   */
               RDataPntr->f = FLAG;
               Zwrite(OUTSTR,BUFFER,R_Data);
               break;
            case TX_Data:                     /* Complex time labeled maps */
               TRDataPntr->y = VALUE; /* into real time labeled    */
               TRDataPntr->t = TIME;
               Zwrite(OUTSTR,BUFFER,TR_Data);
               break;
            }

         if (ferror(OUTSTR)) BombOff(1);

         ++TCNT;
         } /* End WHILE */

      if (ferror(INSTR)) BombOff(1);

      zTaskMessage(2,"%ld Data Points Processed\n",(long)TCNT);

      fcloseall();

      ERRFLAG = zNameOutputFile(OUTFILE,TMPFILE);
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
   
   return;
   }
