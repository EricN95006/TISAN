/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task DBTRANS
*
* Apply transcendental functions (Euclidean and hyperbolic trig functions)
*
*
*CODE   0 -> SINE
*       1 -> COSINE
*       2 -> TANGENT
*       3 -> INVERSE SINE
*       4 -> INVERSE COSINE
*       5 -> INVERSE TANGENT
*       6 -> Hyperbolic SINE
*       7 -> Hyperbolic COSINE
*       8 -> Hyperbolic TANGENT
*       9 -> Inverse Hyperbolic SINE
*       10-> Inverse Hyperbolic COSINE
*       11-> Inverse Hyperbolic TANGENT
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

void getSine(double *Rval, struct complex *Zval, short type);
void getCosine(double *Rval, struct complex *Zval, short type);
void getTangent(double *Rval, struct complex *Zval, short type);
void getInverseSine(double *Rval, struct complex *Zval, short type);
void getInverseCosine(double *Rval, struct complex *Zval, short type);
void getInverseTangent(double *Rval, struct complex *Zval, short type);
void getHyperbolicSine(double *Rval, struct complex *Zval, short type);
void getHyperbolicCosine(double *Rval, struct complex *Zval, short type);
void getHyperbolicTangent(double *Rval, struct complex *Zval, short type);
void getInverseHyperbolicSine(double *Rval, struct complex *Zval, short type);
void getInverseHyperbolicCosine(double *Rval, struct complex *Zval, short type);
void getInverseHyperbolicTangent(double *Rval, struct complex *Zval, short type);

char TMPFILE[_MAX_PATH];

FILE *INSTR  = (FILE *)NIL;
FILE *OUTSTR = (FILE *)NIL;

const char szTask[]="DBTRANS";

int main(int argc, char *argv[])
   {
   struct FILEHDR FileHeader;
   char BUFFER[sizeof(struct TXData)];
//   struct RData *RDataPntr;
//   struct TRData *TRDataPntr;
//   struct XData *XDataPntr;
//   struct TXData *TXDataPntr;
   double Rval;
   struct complex Zval;
   short  FLAG=0;
   char INFILE[_MAX_PATH], OUTFILE[_MAX_PATH];
   double TIME, TCNT=0.;
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;
   
   signal(SIGINT,BREAKREQ);
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);

//   RDataPntr  = (struct  RData *)BUFFER;
//   TRDataPntr = (struct TRData *)BUFFER;
//   XDataPntr  = (struct  XData *)BUFFER;
//   TXDataPntr = (struct TXData *)BUFFER;

   if ((CODE <0) || (CODE > 11))
      {
      zTaskMessage(10,"CODE Out of Range\n");
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
         case X_Data:
         case TX_Data:
            break;
         default:
            zTaskMessage(10,"Invalid File Type.\n");
            BombOff(1);
         }

      zTaskMessage(2,"Opening Scratch File '%s'\n",TMPFILE);

      if ((OUTSTR = zOpen(TMPFILE,O_writeb)) == NULL) Zexit(1);

      if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);

      FLAG=0;
      TCNT=0.;

      while (Zread(INSTR,BUFFER,FileHeader.type))
         {
         if (extractValues(BUFFER, &FileHeader, TCNT, &TIME, &Rval, &Zval, &FLAG))
            {
            zTaskMessage(10,"Invalid Data Type.\n");
            BombOff(1);
            }

         if (!FLAG)     // Only process good data
            {
            switch (CODE)
               {
               case 0: /* SINE of Data */
                  getSine(&Rval, &Zval, FileHeader.type);
                  break;
               case 1: /* COSINE of Data */
                  getCosine(&Rval, &Zval, FileHeader.type);
                  break;
               case 2: /* TANGENT of Data */
                  getTangent(&Rval, &Zval, FileHeader.type);
                  break;
               case 3: /* INVERSE SINE of Data */
                  getInverseSine(&Rval, &Zval, FileHeader.type);
                  break;
               case 4: /* INVERSE COSINE of Data */
                  getInverseCosine(&Rval, &Zval, FileHeader.type);
                  break;
               case 5: /* INVERSE TANGENT of Data */
                  getInverseTangent(&Rval, &Zval, FileHeader.type);
                  break;
               case 6: /* Hyperbolic SINE of Data */
                  getHyperbolicSine(&Rval, &Zval, FileHeader.type);
                  break;
               case 7: /* Hyperbolic COSINE */
                  getHyperbolicCosine(&Rval, &Zval, FileHeader.type);
                  break;
               case 8: /* Hyperbolic TANGENT of Data */
                  getHyperbolicTangent(&Rval, &Zval, FileHeader.type);
                  break;
               case 9: /* INVERSE Hyperbolic SINE of Data */
                  getInverseHyperbolicSine(&Rval, &Zval, FileHeader.type);
                  break;
               case 10: /* INVERSE Hyperbolic COSINE of Data */
                  getInverseHyperbolicCosine(&Rval, &Zval, FileHeader.type);
                  break;
               case 11: /* INVERSE Hyperbolic TANGENT of Data */
                  getInverseHyperbolicTangent(&Rval, &Zval, FileHeader.type);
                  break;
               } /* end switch */
            } /* end if FLAG */

         insertValues(BUFFER, &FileHeader, TIME, Rval, Zval, FLAG);

         if (Zwrite(OUTSTR,BUFFER,FileHeader.type)) BombOff(1);

         ++TCNT;           // Must update after all the processing...
         } /* end while */

      if (ferror(INSTR)) BombOff(1);

      fcloseall();

      ERRFLAG = zNameOutputFile(OUTFILE,TMPFILE);
      } // for (iwc = 0, pWild = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

void getSine(double *Rval, struct complex *Zval, short type)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = sin(*Rval);
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_sin(*Zval);
         break;
      }
   return;
   }

void getCosine(double *Rval, struct complex *Zval, short type)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = cos(*Rval);
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_cos(*Zval);
         break;
      }
   return;
   }

void getTangent(double *Rval, struct complex *Zval, short type)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = tan(*Rval);
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_tan(*Zval);
         break;
      }
   return;
   }

void getInverseSine(double *Rval, struct complex *Zval, short type)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = asin(*Rval);
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_asin(*Zval);
         break;
      }
   return;
   }

void getInverseCosine(double *Rval, struct complex *Zval, short type)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = acos(*Rval);
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_acos(*Zval);
         break;
      }
   return;
   }

void getInverseTangent(double *Rval, struct complex *Zval, short type)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = atan(*Rval);
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_atan(*Zval);
         break;
      }
   return;
   }

void getHyperbolicSine(double *Rval, struct complex *Zval, short type)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = sinh(*Rval);
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_sinh(*Zval);
         break;
      }
   return;
   }

void getHyperbolicCosine(double *Rval, struct complex *Zval, short type)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = cosh(*Rval);
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_cosh(*Zval);
         break;
      }
   return;
   }

void getHyperbolicTangent(double *Rval, struct complex *Zval, short type)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = tanh(*Rval);
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_tanh(*Zval);
         break;
      }
   return;
   }

void getInverseHyperbolicSine(double *Rval, struct complex *Zval, short type)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         *Rval = asinh(*Rval);
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_asinh(*Zval);
         break;
      }
   return;
   }

void getInverseHyperbolicCosine(double *Rval, struct complex *Zval, short type)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         if (*Rval >= 1.)
            *Rval = acosh(*Rval);
         else
            {
            zTaskMessage(9,"ArcCosh(%lG) Range Error. Value set to zero.\n",*Rval);
            *Rval = 0.;
            }
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_acosh(*Zval);
         break;
      }
   return;
   }

void getInverseHyperbolicTangent(double *Rval, struct complex *Zval, short type)
   {
   switch (type)
      {
      case R_Data:
      case TR_Data:
         if (Square(*Rval) < 1.0)
            *Rval = atanh(*Rval);
         else
            {
            zTaskMessage(9,"ArcTanh(%lG) Range Error. Value set to zero.\n",*Rval);
            *Rval = 0.;
            }
         break;
      case X_Data:
      case TX_Data:
         *Zval = c_atanh(*Zval);
         break;
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
   
   return;
   }
