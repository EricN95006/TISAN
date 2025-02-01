/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task DBCALC
*
* ITYPE 0 -> INTEGRATE FACTOR TIMES
*       1 -> DIFFERENTIATE FACTOR TIMES 
*
* if FACTOR <= 0 and ITYPE=0 then DO NOT create an output file.
*
* CODE is the moment of integration
*
* The infile of this task accepts wild cards
*
* The implementation is file based, so there are two temporary files
* that are used as the input/output that get identity swapped in each pass (phase).
* In other words, the first temporary file is initially the output. On the second
* pass that file becomes the input and the second temporary file becomes the output.
* On the third pass, the second temporary file becomes the input and the first
* temporary files becomes the output again. Which files ends up being the final
* results depends on the ending phase.
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

char TMPFILE1[_MAX_PATH], TMPFILE2[_MAX_PATH]; /* Must be visible to BREAKREQ */

FILE *INSTR = (FILE*)NIL;
FILE *OUTSTR = (FILE*)NIL;
FILE *TMPSTR = (FILE*)NIL;

const char szTask[]="DBCALC";

int main(int argc, char *argv[])
   {
   struct FILEHDR FileHeader;
   double TCNT, TIME, DATA, FVAL;
   short FLAG=0, PHASE2=1;
   char BUFFER[sizeof(struct TRData)];  /* Largest Data Type Processed */
   struct RData  *RDataPntr;
   struct TRData *TRDataPntr;
   double LASTTIME, LASTVAL, VALUE=0.;
   long COUNT, II, FFLAG, FWFLAG, makeFileFlag=1, I;
   char InputFile[_MAX_PATH], OutputFile[_MAX_PATH];
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

   RDataPntr  = (struct  RData *)BUFFER;
   TRDataPntr = (struct TRData *)BUFFER;

   if ((ITYPE < 0) || (ITYPE > 1))
      {
      zTaskMessage(10,"ITYPE Out of Range\n");
      Zexit(1);
      }

   if (CODE < 0)
      {
      zTaskMessage(10,"CODE must be non-negative\n");
      Zexit(1);
      }

   if (FACTOR <= 0) makeFileFlag = 0;     // Only make an integration files if factor number of integrations is 1 or more.
   if (ITYPE) makeFileFlag=1;             // Differentiation always creates an output file.

   zBuildFileName(M_inname,InputFile);
   CatList = ZCatFiles(InputFile);    // Find Matching Files for the input file specification
   if (CatList->N == 0) Zexit(1);     // Quit if there are none

   for (iwc = 0, pWild = CatList->pList;
        (iwc < CatList->N) && !ERRFLAG;
        ++iwc, pWild = strchr(pWild,'\0') + 1)
      {
      splitPath(pWild,Drive,Dir,Fname,Ext);
      strcpy(INNAME,Fname);
      strcpy(INCLASS,Ext);

      zBuildFileName(M_inname,InputFile);

      zTaskMessage(2,"Opening Input File '%s'\n", InputFile);

      if (((INSTR = zOpen(InputFile,O_readb)) == NULL) ||
          (!Zgethead(INSTR,&FileHeader))) Zexit(1);

      if ((FileHeader.type != R_Data) &&
          (FileHeader.type != TR_Data))
        {
        zTaskMessage(10,"Not a Real Data File.\n");
        Zexit(1);
        }

      if (makeFileFlag)
         {
         zBuildFileName(M_tmpname,TMPFILE1);
         zTaskMessage(2,"Opening Scratch File '%s'\n", TMPFILE1);
         if ((OUTSTR = zOpen(TMPFILE1,O_writeb)) == NULL) Zexit(1);

         zBuildFileName(M_tmpname,TMPFILE2);
         zTaskMessage(2,"Opening Scratch File '%s'\n", TMPFILE2);
         if ((TMPSTR = zOpen(TMPFILE2,O_writeb)) == NULL) Zexit(1);

         FileHeader.b += FileHeader.m;  /* First point is lost */
         if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);

         FileHeader.b -= FileHeader.m;  /* Undo the patch */
         }

      COUNT = Max(1,(short)FACTOR);  /* Number of integrations or */

      for (II=1L; II<=COUNT; ++II)      /* differentiations.         */
         {
         TCNT = 0.;
         VALUE = 0.;
         FWFLAG = FFLAG = 1;
         PHASE2 = PHASE2 ? 0 : 1;   /* Toggle PHASE flag */
         while (Zread(INSTR,BUFFER,FileHeader.type))
            {
            if (extractValues(BUFFER, &FileHeader, TCNT, &TIME, &DATA, &Zval, &FLAG))
               {
               zTaskMessage(10,"Invalid Data Type.\n");
               BombOff(1);
               }

            ++TCNT;

            if (!FLAG)
               {
               if (!FFLAG)
                  {
                  switch (ITYPE)
                     {
                     case 0: /* INTEGRATE */
                        FVAL = (TIME-LASTTIME)*(DATA+LASTVAL)/2.;

                        for (I=0; I<CODE; ++I)
                           FVAL *= (TIME+LASTTIME)/2.;

                        VALUE += FVAL;
                        break;
                     case 1: /* DIFFERENTIATE */
                        VALUE = (DATA-LASTVAL)/(TIME-LASTTIME);
                        break;
                     }
                  switch (FileHeader.type)
                     {
                     case R_Data:
                        RDataPntr->y = VALUE;
                        break;
                     case TR_Data:
                        TRDataPntr->y = VALUE;
                        break;
                     }
                  }
               LASTTIME = TIME;
               LASTVAL = DATA;
               FFLAG = 0;
               }

            if (!FWFLAG &&
                 makeFileFlag  &&
                 (Zwrite(OUTSTR,BUFFER,FileHeader.type))) BombOff(1);

            FWFLAG = FFLAG;
            } /* End WHILE */

         if (ferror(INSTR)) BombOff(1);

         if (!ITYPE) zTaskMessage(3,"Pass %d Integral = %lG\n", II,VALUE);

         if (II < COUNT)
            {
            INSTR = OUTSTR;
            OUTSTR = TMPSTR;
            TMPSTR = INSTR;

            fseek(OUTSTR, 0L, SEEK_SET);
            ftruncate(fileno(OUTSTR),0L);

            if (!Zgethead(INSTR,(struct FILEHDR *)NULL)) BombOff(1); // Rewinds and advances past the header

            FileHeader.b += FileHeader.m;  /* First point is lost */

            if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);

            FileHeader.b -= FileHeader.m;  /* Undo the patch */
            }  /* End if */
         } /* End FOR */

      fcloseall();
      
      if (makeFileFlag)
         {
         zBuildFileName(M_outname,OutputFile);

         if (PHASE2)
            {
            ERRFLAG = zNameOutputFile(OutputFile,TMPFILE2);
            unlink(TMPFILE1);
            }
         else
            {
            ERRFLAG = zNameOutputFile(OutputFile,TMPFILE1);
            unlink(TMPFILE2);
            }
         }
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
   unlink(TMPFILE1);
   unlink(TMPFILE2);
   ZCatFiles((char*)NIL); // free catalog memory
   Zexit(a);
   }

void fcloseall(void) // declared in dos.h
   {
   if (INSTR)  Zclose(INSTR);
   INSTR = (FILE*)NIL;

   if (OUTSTR) Zclose(OUTSTR);
   OUTSTR = (FILE*)NIL;

   if (TMPSTR) Zclose(TMPSTR);
   TMPSTR = (FILE*)NIL;

   return;
   }
