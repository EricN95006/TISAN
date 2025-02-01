/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task DBSMOOTH
*
*
* CODE  0 -> KALMAN FILTER SMOOTH
*       1 -> RUNNING AVERAGE SMOOTH
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

// #define SMLimit 500

char TMPFILE[_MAX_PATH]; /* Must be visible to BREAKREQ */

double *DBUFF;

FILE *INSTR  = (FILE *)NIL;
FILE *OUTSTR = (FILE *)NIL;

const char szTask[]="DBSMOOTH";

int main(int argc, char *argv[])
   {
   struct FILEHDR FileHeader;
   char BUFFER[sizeof(struct TRData)]; /* Largest structure porcessed */
   struct RData *RDataPntr;
   struct TRData *TRDataPntr;
   double IDATA, ODATA;
   double SUM=0., LASTVAL, YINMAX, YINMIN, YOUTMAX, YOUTMIN;
   double COUNT=0.,TOTAL=0., TCNT=0., TIME;
   long J=0L, nBOX, K;
   short IFLAG=1, OFLAG=1, FFLAG=1, FLAG=0;
   char INFILE[_MAX_PATH], OUTFILE[_MAX_PATH];
   struct complex Zval;
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;
   
   signal(SIGINT,BREAKREQ);
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);

   RDataPntr =  (struct RData *)BUFFER;
   TRDataPntr = (struct TRData *)BUFFER;

   if ((CODE < 0) || (CODE > 1))
      {
      zTaskMessage(10,"CODE Out of Range\n");
      Zexit(1);
      }

   if ((CODE == 0) && ((FACTOR >= 1.0) || (FACTOR <= 0.0)))
      {
      zTaskMessage(10,"Invalid FACTOR for Kalman Filter\n");
      Zexit(1);
      }

   if ((CODE == 1) && ((FACTOR <= 2.)))
      {
      zTaskMessage(10,"Invalid FACTOR for Running Average Smooth\n");
      Zexit(1);
      }

   if (CODE == 1) // Boxcar
      {
      nBOX = (long)FACTOR;
      FACTOR = (double)nBOX;
      
      DBUFF = (double *)malloc((nBOX + 1L) * sizeof(double));
   
      if (!DBUFF)
         {
         zTaskMessage(10,"Failed to allocate memory for %ld point boxcar.\n", nBOX);
         BombOff(1);
         }
      }

   zBuildFileName(M_inname,INFILE);
   CatList = ZCatFiles(INFILE);   // Find Matching Files for the input file specification
   if (CatList->N == 0) Zexit(1);     // Quit if there are none

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
      if ((INSTR = zOpen(INFILE,O_readb)) == NULL) Zexit(1);

      if (!Zgethead(INSTR,&FileHeader)) BombOff(1);

      switch (FileHeader.type)
         {
         case R_Data:
         case TR_Data:
            break;
         default:
            zTaskMessage(10,"Cannot process complex data files.\n");
            BombOff(1);
         }

      zTaskMessage(2,"Opening Scratch File '%s'\n",TMPFILE);
      if (((OUTSTR = zOpen(TMPFILE,O_writeb)) == NULL) ||
          (Zputhead(OUTSTR,&FileHeader))) BombOff(1);

      SUM = 0.;
      COUNT = 0.;
      TOTAL = 0.;
      TCNT = 0.;
      J = 0L;
      IFLAG = 1;
      OFLAG = 1;
      FFLAG = 1;
      FLAG = 0;

      while (Zread(INSTR,BUFFER,FileHeader.type))
         {
         if (extractValues(BUFFER, &FileHeader, TCNT, &TIME, &IDATA, &Zval, &FLAG))
            {
            zTaskMessage(10,"Invalid Data Type.\n");
            BombOff(1);
            }
         ++TCNT;
         
         if (!FLAG)
            {
            if (IFLAG)
               {
               ODATA = YINMIN = YINMAX = IDATA;
               IFLAG = 0;
               }
            else
               {
               YINMAX = Max(YINMAX,IDATA);
               YINMIN = Min(YINMIN,IDATA);
               }

            switch (CODE)
               {
               case 0: /* Kalman Filter */
                  if (FFLAG)
                     {
                     LASTVAL = IDATA;
                     FFLAG = 0;
                     }
                  else
                     {
                     ODATA = (1.-FACTOR)*LASTVAL + FACTOR*IDATA;
                     LASTVAL = IDATA;
                     }
                  break;
               case 1: /* Running Average */
                  if (++COUNT < FACTOR)
                     {
                     SUM += (DBUFF[J] = IDATA);
                     ODATA = SUM/((double)(++J));
                     }
                   else
                     {
                     DBUFF[J] = IDATA;
                     K = J - nBOX;
                     if (K < 0L) K += (nBOX + 1L);
                     SUM += (DBUFF[J] - DBUFF[K]);
                     if (++J > nBOX) J = 0L;
                     ODATA = SUM/FACTOR;
                     }
                   break;
               } /* End switch CODE */

            switch (FileHeader.type)
               {
               case R_Data:
                  RDataPntr->y = ODATA;
                  break;
               case TR_Data:
                  TRDataPntr->y = ODATA;
                  break;
               }

            if (OFLAG)
               {
               YOUTMIN = YOUTMAX = ODATA;
               OFLAG = 0;
               }
            else
               {
               YOUTMAX = Max(YOUTMAX,ODATA);
               YOUTMIN = Min(YOUTMIN,ODATA);
               }
            ++TOTAL;
            } /* End if FLAG */

         if (Zwrite(OUTSTR,BUFFER,FileHeader.type)) BombOff(1);
         } /* End WHILE */

      if (ferror(INSTR)) BombOff(1);

      zTaskMessage(2,"%ld Data Points Processed\n",(long)TOTAL);
      zTaskMessage(3,"Old Amplitude Range %lG,%lG\n",YINMIN,YINMAX);
      zTaskMessage(3,"New Amplitude Range %lG,%lG\n",YOUTMIN,YOUTMAX);

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

   if (DBUFF) free(DBUFF);
   DBUFF = (double *)NIL;
   
   return;
   }
