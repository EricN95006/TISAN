/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task DBCMB
*
* I combine two data bases in a multitude of fashions.
* When combining files, the timing information is ignored and the time stamp
* from the secondary is kept while that of the primary file is lost.
*
* Note that for complex data bases, ZFACTOR is used as the complex FACTOR
*
* Currently only files of the same data type can be combined.
*
* CODE  0 -> DB1 + FACTOR * DB2
*       1 -> DB1 * DB2
*       2 -> DB1 / DB2
*       3 -> DB1 ^ DB2
*       4 -> LOG(DB1) BASE DB2
*       5 -> APPEND DB2 TO DB1
*       6 -> INTERLEAVE DB1 WITH DB2
*       7 -> CONVOLVE DB1 and DB2
*
* The in2file accepts wild cards. To use this feature, make the outfile the same as the infile and give infile a name that does not match the secondary files.
* The code will then apply the new secondary file to the output of each previous run.
*
* The secondary filename in this task accepts wild cards.
*
*/
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <memory.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>

#include "tisan.h"

void appendFiles();
void interleaveFiles();
void convolveFiles();
void addFiles();
void multiplyFiles();
void divideFiles();
void powerFiles();
void logFiles();
void getFileDataElements();
void putFileDataElements();

char TMPFILE[_MAX_PATH];           /* Must be visible to BREAKREQ */

FILE *INSTR  = (FILE *)NIL;
FILE *OUTSTR = (FILE *)NIL;
FILE *IN2STR = (FILE *)NIL;

char BUFF1[sizeof(struct TXData)];
char BUFF2[sizeof(struct TXData)];
char BUFF[sizeof(struct TXData)];

struct FILEHDR F1Header, F2Header, FHeader;

struct TRData *TRDataPntr, *TRDataPntr1, *TRDataPntr2;
struct TXData *TXDataPntr, *TXDataPntr1, *TXDataPntr2;
struct XData  *XDataPntr,  *XDataPntr1,  *XDataPntr2;
struct RData  *RDataPntr,  *RDataPntr1,  *RDataPntr2;

short FLAG1=0, FLAG2=0, FLAG;

double Rval1, Rval2, Rval;
struct complex Zval1, Zval2, Zval, Zfactor;

char *DVZP = "%-8s: Division by Zero, Returned ZERO.\n";

const char szTask[]="DBCMB";

int main(int argc, char *argv[])
   {
   char INFILE[_MAX_PATH], OUTFILE[_MAX_PATH], IN2FILE[_MAX_PATH];
   char *N1, *N2;
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int I;
   char *pChar;

   signal(SIGINT,BREAKREQ);             /* Setup ^C Processing */
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);      /* Initialize task */

   RDataPntr   = (struct  RData *)BUFF;
   RDataPntr1  = (struct  RData *)BUFF1;
   RDataPntr2  = (struct  RData *)BUFF2;
   TRDataPntr  = (struct TRData *)BUFF;
   TRDataPntr1 = (struct TRData *)BUFF1;
   TRDataPntr2 = (struct TRData *)BUFF2;
   XDataPntr   = (struct  XData *)BUFF;
   XDataPntr1  = (struct  XData *)BUFF1;
   XDataPntr2  = (struct  XData *)BUFF2;
   TXDataPntr  = (struct TXData *)BUFF;
   TXDataPntr1 = (struct TXData *)BUFF1;
   TXDataPntr2 = (struct TXData *)BUFF2;

   if ((CODE < 0) || (CODE > 7))
      {
      zTaskMessage(10,"CODE Out of Range\n");
      Zexit(1);
      }

   zBuildFileName(M_inname,INFILE);   // Build file names
   zBuildFileName(M_in2name,IN2FILE); // This could have wild cards....
   zBuildFileName(M_outname,OUTFILE);
   zBuildFileName(M_tmpname,TMPFILE);

   CatList = ZCatFiles(IN2FILE);      // Find Matching Files for the secondary input file specification
   if (CatList->N == 0) Zexit(1);     // Quit if there are none

   for (I = 0, pChar = CatList->pList;
        (I < CatList->N) && !ERRFLAG;
        ++I, pChar = strchr(pChar,'\0') + 1)
      {
      splitPath(pChar,Drive,Dir,Fname,Ext);
      strcpy(IN2NAME,Fname);
      strcpy(IN2CLASS,Ext);

      zBuildFileName(M_in2name,IN2FILE);
/*
** Open input file and verify that we know what it is
*/
      zTaskMessage(2,"Opening Input File '%s'\n", INFILE);

      INSTR = zOpen(INFILE,O_readb);

      if (!INSTR) Zexit(1);
   
      if (!Zgethead(INSTR,&F1Header)) BombOff(1);

      switch (F1Header.type)
         {
         case R_Data:
         case TR_Data:
         case X_Data:
         case TX_Data:
            break;
         default:
            zTaskMessage(10,"Unknown file type.\n");
            BombOff(1);
         }
/*
** Open second input file and verify that we know what it is
*/
      zTaskMessage(2,"Opening Secondary Input File '%s'\n", IN2FILE);

      if (((IN2STR = zOpen(IN2FILE,O_readb)) == NULL) ||
          (!Zgethead(IN2STR,&F2Header))) BombOff(1);

      switch (F2Header.type)
         {
         case R_Data:
         case TR_Data:
         case X_Data:
         case TX_Data:
            break;
         default:
            zTaskMessage(10,"Invalid file type.\n");
            BombOff(1);
         }

      if (F1Header.type != F2Header.type)
         {
         zTaskMessage(10,"Cannot combine files of different types.\n");
         BombOff(1);
         }

      if ((CODE == 7) && (F1Header.type != R_Data) && (F1Header.type != X_Data)) // Cannot convolve time labeled files since the time intervals MUST be the same between points.
         {
         zTaskMessage(10,"Cannot convolve time labeled files.\n");
         BombOff(1);
         }

      if ((F1Header.m != F2Header.m) || (F1Header.m != F2Header.m))
         zTaskMessage(8,"** WARNING ** Different Time Scales!\n");
/*
** The file takes on the type of file #2
**
*/
      FHeader = F2Header;
      Zfactor.x = ZFACTOR[0];
      Zfactor.y = ZFACTOR[1];
      zTaskMessage(2,"Opening Scratch File '%s'\n",TMPFILE);

      if (((OUTSTR = zOpen(TMPFILE,O_writeb)) == NULL) || Zputhead(OUTSTR,&FHeader)) BombOff(1);

      FLAG1=0;
      FLAG2=0;

      if ((CODE == 5) || (CODE == 6) || (CODE ==7))
         {
         switch (CODE)           /* Select Action */
            {
            case 5:  // Append DB2 to DB1
               appendFiles();
               break;

            case 6:  // Interleave (aka shuffle)
               interleaveFiles();
               break;
            
            case 7:  // Convolve
               convolveFiles();
            } /* End SWITCH over 5, 6 and 7*/
         }
      else
         {
/*
** Note:  The data processing program structure is written
**        in such a way that the modifications needed for
**        combining un-like data files can be more easily
**        implemented.
*/
         while ((N1 = Zread(INSTR,BUFF1,F1Header.type)) &&
                (N2 = Zread(IN2STR,BUFF2,F2Header.type)))
            {
            getFileDataElements();

            FLAG = Max(FLAG1, FLAG2);

            if (!FLAG)   /* Only process valid data points */
               {
               switch (CODE) // Element by element combinations
                  {
                  case 0: // DB1 + FACTOR * DB2
                     addFiles();
                     break;
                  case 1: // DB1 * DB2
                     multiplyFiles();
                     break;
                  case 2: // DB1 / DB2
                     divideFiles();
                     break;
                  case 3: // POW (DB1,DB2)
                     powerFiles() ;
                     break;
                  case 4: /* LOG(DB1) BASE DB2 */
                     logFiles();
                     break;
                  }
               }

            putFileDataElements();

            if (Zwrite(OUTSTR,BUFF,F1Header.type)) BombOff(1);
            } /* End WHILE */

         if (ferror(INSTR)) BombOff(1);

   /* If both N1 and N2 are not NULL then message */
         if (!N1) N2 = Zread(IN2STR,BUFF2,F2Header.type);
         if (N1 != N2) zTaskMessage(8,"WARNING!!! Files are of Different Lengths\n");
         } // if ((CODE == 5) || (CODE == 6) || (CODE ==7))

      fcloseall();
      ERRFLAG = zNameOutputFile(OUTFILE,TMPFILE);
      }  // for (I = 0, pChar = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

/*****
** Helper functions mostly to improve readability
*/
void appendFiles() // Append DB2 to DB1
   {
   while (Zread(INSTR,BUFF1,F1Header.type))
      {
      if (Zwrite(OUTSTR,BUFF1,F1Header.type)) BombOff(1);
      }

   if (ferror(INSTR)) BombOff(1);

   while (Zread(IN2STR,BUFF2,F2Header.type))
      {
      if (Zwrite(OUTSTR,BUFF2,F2Header.type)) BombOff(1);
      }

   if (ferror(IN2STR)) BombOff(1);

   return;
   }

void interleaveFiles() // shuffle files together
   {
   char *N1, *N2;

   if ((F1Header.type != R_Data) && (F1Header.type != X_Data))
      {
      zTaskMessage(10,"Can only Interleave time series files, not time-labeled.\n");
      BombOff(1);
      }
   if (Zputhead(OUTSTR,&F1Header)) BombOff(1);

   while ((N1 = Zread(INSTR,BUFF1,F1Header.type)) &&
          (N2 = Zread(IN2STR,BUFF2,F1Header.type)))
      {
      if (Zwrite(OUTSTR,BUFF1,F1Header.type)) BombOff(1);
      if (Zwrite(OUTSTR,BUFF2,F1Header.type)) BombOff(1);
      }

   if (ferror(INSTR) || ferror(IN2STR)) BombOff(1);

   if (!N1) N2 = Zread(IN2STR,BUFF2,F2Header.type);

   if (N1 != N2)     // If both N1 and N2 are not NULL then message
      zTaskMessage(8,"WARNING!!! Files are of Different Lengths\n");

   return;
   }

/*
** Brute force convolution (no Fourier transforms)
** The data must be evenly spaced, so it cannot process time labeled files.
** The time information for the final output (m and b in the header) will come from file #2.
**
** The convolution integral is given by:
**
** h(y) = ∫ f(x) g(y-x) dx
**
** Which approximates to
**
** h[y] = ∑f[x]g[y-x] ∆x
**
** where x and y are now integer array index values and ∆x is the time interval.
** The time interval ∆x is inside the sum to account for flagged data points in a time series (we might skip over a few time stamps)
**
** The value of [y] goes from 0 to N-1 and [x] thus must be limited to sum from [0] to [y]
**
** The number if points in each file need not be the same, but the sums will be limited to the number of points in the
** smaller file.
**
*/ 
void convolveFiles()
   {
   long N, N1, N2, x, y;
   double Rval1, Rval2, Rsum, delta_t, baseDelta_t;
   struct complex Zval1, Zval2, Zval, Zsum;

/*
** Determined the value of N based on which file has fewer points $$$$
*/
   N1 = countDataRecords(INSTR);
   N2 = countDataRecords(IN2STR);
   N = Min(N1, N2);

   zTaskMessage(4,"Number of data Points in File #1: %ld\n", N1);
   zTaskMessage(4,"Number of data Points in File #2: %ld\n", N2);
   if (N1 != N2) zTaskMessage(8,"WARNING!!! Files are of Different Lengths\n");

/*
** Now perform the loops (outer loop is the second file)
*/
   printPercentComplete(0L, 0L, 0);

   for (y=0; y < N; ++y)
      {
      Rsum = Zsum.x = Zsum.y = 0.;

      Zgethead(INSTR,&F1Header); // Rewind the first data file
      baseDelta_t = F1Header.m;   // Get the base time interval
      delta_t = baseDelta_t;      // This is the time interval used in the calculations

      Zread(IN2STR, BUFF2, F2Header.type);
      switch (F2Header.type) // Get data from input file #2
         {
         case R_Data:
            Rval2 = RDataPntr2->y;
            FLAG2 = RDataPntr2->f;
            break;
         case X_Data:
            Zval2 = XDataPntr2->z;
            FLAG2 = XDataPntr2->f;
            break;
         }

      for (x=0; x <= y; ++x)
         {
         Zread(INSTR,  BUFF1, F1Header.type); // Data from File #1

         switch (F1Header.type)
            {
            case R_Data:
               Rval1 = RDataPntr1->y;
               FLAG1 = RDataPntr1->f;
               break;
            case X_Data:
               Zval1 = XDataPntr1->z;
               FLAG1 = XDataPntr1->f;
               break;
            }

         FLAG = FLAG1 | FLAG2;

         if (FLAG)
            delta_t += baseDelta_t; // Bad data point, so need to make the time interval larger for the next calculation
         else
            {
            switch (FHeader.type)
               {
               case R_Data:
                  Rsum += (Rval1 * Rval2) * delta_t;
                  break;
               case X_Data:
                  Zval = c_mul(Zval1, Zval2);
                  Zval.x *= delta_t;
                  Zval.y *= delta_t;
                  Zsum = c_add(Zsum, Zval);
                  break;
               }

            delta_t = baseDelta_t; // Reset the time interval for the next calculation
            }
         } // for (x=0; x <= y; ++x)

      switch (FHeader.type)  /* Output Data */
         {
         case R_Data:
            RDataPntr->y = Rsum;
            RDataPntr->f = 0;
            break;
         case X_Data:
            XDataPntr->z = Zsum;
            XDataPntr->f = 0;
            break;
         }

      if (Zwrite(OUTSTR,BUFF,FHeader.type)) BombOff(1);

      printPercentComplete(y, N, PROGRESS);

      } // for (y=0; y < N; ++y)

   printPercentComplete(y, N, PROGRESS); // 100% Complete

   return;
   }

void addFiles() // DB1 + FACTOR * DB2
   {
   switch(FHeader.type)
      {
      case X_Data:
      case TX_Data:
         Zval = c_add(Zval1,c_mul(Zval2,Zfactor));
         break;
      case R_Data:
      case TR_Data:
         Rval = Rval1 + FACTOR*Rval2;
         break;
      }
   return;
   }

void multiplyFiles() // DB1 * DB2 element by element
   {
   switch(FHeader.type)
      {
      case X_Data:
      case TX_Data:
         Zval = c_mul(Zval1,Zval2);
         break;
      case R_Data:
      case TR_Data:
         Rval = Rval1 * Rval2;
         break;
      }
   return;
   }

void divideFiles() // DB1 / DB2
   {
   switch(FHeader.type)
      {
      case X_Data:
      case TX_Data:
         if (!c_abs(Zval2))
            {
            zTaskMessage(8,DVZP);
            Zval.x = Zval.y = 0.;
            }
         else
            Zval = c_div(Zval1,Zval2);
         break;
      case R_Data:
      case TR_Data:
         if (!Rval2)
            {
            zTaskMessage(8,DVZP);
            Rval = 0.;
            }
         else
            Rval = Rval1 / Rval2;
         break;
      }
   return;
   }

void powerFiles() // DB1 ^ DB2
   {
   switch(FHeader.type)
      {
      case X_Data:
      case TX_Data:
         Zval = c_pow(Zval1,Zval2);
         break;
      case R_Data:
      case TR_Data:
         Rval = pow(Rval1,Rval2);
         break;
      }
   return;
   }

void logFiles() // Log(DB1) in base DB2
   {
   switch(FHeader.type)
      {
      case X_Data:
      case TX_Data:
         Zval = c_ln(Zval2);
         if  (!c_abs(Zval))
            {
            zTaskMessage(8,DVZP);
            Zval.x = Zval.y = 0.;
            }
         else
            Zval = c_div(c_ln(Zval1),Zval);
         break;
      case R_Data:
      case TR_Data:
         Rval = log(Rval2);
         if (!Rval)
            {
            zTaskMessage(8,DVZP);
            Rval = 0.;
            }
         else
            Rval = log(Rval1)/Rval;
         break;
      }
   return;
   }

void getFileDataElements()
   {
   switch (F1Header.type) /* Get data from input file #1 */
      {
      case R_Data:
         Rval1 = RDataPntr1->y;
         FLAG1 = RDataPntr1->f;
         break;
      case TR_Data:
         Rval1 = TRDataPntr1->y;
         break;
      case X_Data:
         Zval1 = XDataPntr1->z;
         FLAG1 = XDataPntr1->f;
         break;
      case TX_Data:
         Zval1 = TXDataPntr1->z;
         break;
      }

   switch (F2Header.type) /* Get data from input file #2 */
      {
      case R_Data:
         Rval2 = RDataPntr2->y;
         FLAG2 = RDataPntr2->f;
         break;
      case TR_Data:
         Rval2 = TRDataPntr2->y;
         break;
      case X_Data:
         Zval2 = XDataPntr2->z;
         FLAG2 = XDataPntr2->f;
         break;
      case TX_Data:
         Zval2 = TXDataPntr2->z;
         break;
      }

   return;
   }

/*
** Set the values in the data buffer based on the output data type.
** The time stamp for time labeled data will come from the secondary input file.
*/
void putFileDataElements()
   {
   switch (F1Header.type)  /* Output Data */
      {
      case R_Data:
         RDataPntr->y = Rval;
         RDataPntr->f = FLAG;
         break;
      case TR_Data:
         TRDataPntr->t = TRDataPntr2->t;
         TRDataPntr->y = Rval;
         break;
      case X_Data:
         XDataPntr->z = Zval;
         XDataPntr->f = FLAG;
         break;
      case TX_Data:
         TXDataPntr->t = TXDataPntr2->t;
         TXDataPntr->z = Zval;
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
   
   if (IN2STR) Zclose(IN2STR);
   IN2STR = (FILE *)NIL;
   
   if (OUTSTR) Zclose(OUTSTR);
   OUTSTR = (FILE *)NIL;

   return;
   }
