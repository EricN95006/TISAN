/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task DBCON
*
* Link with tisanlib, dos
*
* Task to convert from one data format to another.
*
* Default text file output format is csv. If TFORMAT, YFORMAT or ZFORMT are used then
* the delimiter must be included in the format statement
*
* CODE  1 -> CONVERT ASCII TIME SERIES TO STANDARD FORMAT
*             1 -> REAL TIME SERIES
*            11 -> TIME LABELED FILE
*            21 -> COMPLEX TIME SERIES
*            31 -> COMPLEX TIME LABELED FILE
*       2 -> CONVERT 2 BYTE BINARY INTEGER TO STANDARD FORMAT
*             2 -> REAL TIME SERIES
*            12 -> TIME LABELED FILE
*            22 -> COMPLEX TIME SERIES
*            32 -> COMPLEX TIME LABELED FILE
*       3 -> CONVERT LabVIEW RM-60 Geiger Counter Data to Standard Format
*       4 -> CONVERT STANDARD FORMAT TO ASCII TIME SERIES
*             4 -> CURRENT FILE TYPE
*            14 -> TIME LABELED FILE OF CURRENT TYPE
*       5 -> CONVERT STANDARD FORMAT TO 2 BYTE BINARY INTEGER
*
* For time series files TRANGE is used to store the slope and intercept values for determining the time stamp. Default is 1, 0 if both are zero.
*
* The CODE of 3 (or 13) takes in the data from my FileGeiger application. It is a {T Y} pair where T is a double and Y is a U16.
* LabVIEW uses the big-endian format for its values, so you need to byte swap them to get the proper interpretation here.
*
* There are a number of conversion functions here that are not currently used. Feel free to implement them...
*
* This program accepts wild cards for the input file name
*
*/
#include <unistd.h>
#include <signal.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include "tisan.h"

short ATOS(void);    // ASCII to standard
short ITOS(void);    // Integer to standard
short GTOS(void);    // Geiger Data to Standard
short STOA(short);   // Standard to ASCII
short STOI(void);    // Standard to Integer

short READASCII(double *);     /* Read in an ASCII value         */
short READINT(double *);       /* Read in a binary integer value */
short READWORD(double *YP);    // Red in a unsigned 2-byte integer
short READWORDM(double *YP);   // Red in a unsigned 2-byte integer big-endian
short READDOUBLE(double *YP);  // Read in a double value (8 bytes) 
short READDOUBLEM(double *YP); // Read in a double value (8 bytes) big-endian

void FPEERROR(int a);

char BUFFER[2048];                  /* General I/O Buffer */
char CONBUF[sizeof(struct TXData)]; /* Largest Data Type */
char C, TMPFILE[_MAX_PATH];
char INFILE[_MAX_PATH], OUTFILE[_MAX_PATH], *CPNTR;
double TOTAL;
struct FILEHDR FileHeader;
short I;
struct RData  *RDataPntr;
struct TRData *TRDataPntr;
struct XData  *XDataPntr;
struct TXData *TXDataPntr;

FILE *INSTR = (FILE*)NIL;
FILE *OUTSTR = (FILE*)NIL;

const char szTask[]="DBCON";

int main(int argc, char *argv[])
   {
   short ERRFLAG = 0;
   short WhatType, WhichConversion;
   struct CATSTRUCT *CatList;
   int I;
   char *pChar;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];

   signal(SIGINT,BREAKREQ);  /* Setup ^C Interrupt */
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);  /* Initialize task */

   RDataPntr  =  (struct RData *)CONBUF;   /* Data Structure Pointers */
   TRDataPntr = (struct TRData *)CONBUF;
   XDataPntr  =  (struct XData *)CONBUF;
   TXDataPntr = (struct TXData *)CONBUF;

   zBuildFileName(M_inname,INFILE);    /* Build Primary File Name */

   CatList = ZCatFiles(INFILE);   // Find matching files for the input name
   if (CatList->N == 0) Zexit(1); // Quit if there are none
/*
** For Each Matching File, Perform the Conversion.
** For this simple task it is not necessary to reload the
** adverbs for each iteration since we don't change anything critical.
*/
   if (CODE == 3) CODE = 13; // The Geiger counter data is real time labeled, so force it

   if ((TRANGE[0] == 0.) && (TRANGE[1] == 0.)) TRANGE[0] = 1.0; // TRANGE is only meaningful for time series files. Set slope to 1 by default

   WhatType =  CODE / 10;
   if ((WhatType > 3) || (WhatType < 0)) WhatType = 0;
   WhichConversion = CODE % 10;

   for (I = 0, pChar = CatList->pList;
        (I < CatList->N) && !ERRFLAG;
        ++I, pChar = strchr(pChar,'\0') + 1)
      {
      TOTAL = 0.;
      splitPath(pChar,Drive,Dir,Fname,Ext);
      strcpy(INNAME,Fname);
      strcpy(INCLASS,Ext);
      zBuildFileName(M_inname,INFILE);    /* Build file names */
      zBuildFileName(M_outname,OUTFILE);
      zBuildFileName(M_tmpname,TMPFILE);
/*
** Open both the input and output files.  If an error occurs then
** the task dies.
*/
      zTaskMessage(2,"Opening Input File '%s'\n",INFILE);
      if ((INSTR = zOpen(INFILE,O_readb)) == NULL) Zexit(1);

      zTaskMessage(2,"Opening Scratch File '%s'\n",TMPFILE);
      if ((OUTSTR = zOpen(TMPFILE,O_writeb)) == NULL) Zexit(1);
     
      switch (WhatType)
         {
         case 0:
            FileHeader.type = R_Data;
            break;
         case 1:
            FileHeader.type = TR_Data;
            TRANGE[0] = 1.0;
            TRANGE[1] = 0.0;
            break;
         case 2:
            FileHeader.type = X_Data;
            break;
         case 3:
            FileHeader.type = TX_Data;
            TRANGE[0] = 1.0;
            TRANGE[1] = 0.0;
            break;
         default:
            zTaskMessage(10,"This Error Should Not Occur!\n");
            Zexit(1);
         }

      FileHeader.m = TRANGE[0];  /* Initialize time slope value     */
      FileHeader.b = TRANGE[1];  /* Initialize time intercept value */

      setHeaderText(&FileHeader);

      switch (WhichConversion)
         {
         case 1: /* ASCII to Standard */
            ERRFLAG = ATOS();
            break;
         case 2: /* 2 Byte Binary Integer to Standard */
            ERRFLAG = ITOS();
            break;
         case 3: /* LabVIEW Geiger counter data to standard */
            ERRFLAG = GTOS();
            break;
         case 4: /* Standard to ASCII */
            ERRFLAG = STOA(WhatType);
            break;
         case 5: /* Standard to 2 Byte Binary Integer */
            ERRFLAG = STOI();
            break;
         default:
            zTaskMessage(10,"Unknown Action Code '%d'\n",CODE);
            ERRFLAG = 1;
         }

      if (ERRFLAG) BombOff(1);
      
      zTaskMessage(2,"%ld Data Points Processed\n",(long)TOTAL);

      fcloseall();

      ERRFLAG = zNameOutputFile(OUTFILE,TMPFILE);
      }

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

/*********************************************************************
**
** ASCII to Standard Format
*/
short ATOS()
   {
   double Y, X, T;
   short DFLAG=1;

   if (Zputhead(OUTSTR,&FileHeader)) return(1);

   while (DFLAG)  /* Use DFLAG to signal end of input file */
      {
      switch (FileHeader.type) /* Select data interpretation */
         {
         case R_Data:  /* Real time series */
            if ((DFLAG = READASCII(&Y)) > 0)
               {
               RDataPntr->y = Y;
               RDataPntr->f = 0;
               }
            break;
         case TR_Data:  /* Real time labeled */
            if (((DFLAG = READASCII(&T)) > 0) &&
                ((DFLAG = READASCII(&Y)) > 0))
               {
               TRDataPntr->t = T;
               TRDataPntr->y = Y;
               }
            break;
         case X_Data:  /* Complex time series */
            if (((DFLAG = READASCII(&X)) > 0) &&
                ((DFLAG = READASCII(&Y)) > 0))
               {
               XDataPntr->z.x = X;
               XDataPntr->z.y = Y;
               XDataPntr->f = 0;
               }
            break;
         case TX_Data:  /* Complex time labeled */
            if (((DFLAG = READASCII(&T)) > 0) &&
                ((DFLAG = READASCII(&X)) > 0) &&
                ((DFLAG = READASCII(&Y)) > 0))
               {
               TXDataPntr->t = T;
               TXDataPntr->z.x = X;
               TXDataPntr->z.y = Y;
               }
            break;
         }

      if (DFLAG < 0) return(1);

      if (DFLAG)
         {
         if (Zwrite(OUTSTR,CONBUF,FileHeader.type)) return(1);
         ++TOTAL;
         }
      }

   return(0);
   }

/*********************************************************************
**
** 2 Byte Binary Integer (little-endian/Intel) to Standard Format
*/
short ITOS()
   {
   short DFLAG=1;
   double X, Y, T;

   if (Zputhead(OUTSTR,&FileHeader)) return(1);

   while (DFLAG) /* Use DFLAG to signal end of input file */
      {
      switch (FileHeader.type) /* Select data interpretation */
         {
         case R_Data:   /* Real time series */
            if ((DFLAG = READINT(&Y)))
               {
               RDataPntr->y = Y;
               RDataPntr->f = 0;
               }
            break;
         case TR_Data:   /* Real time labeled */
            if ((DFLAG = READINT(&T)) &&
                (DFLAG = READINT(&Y)))
               {
               TRDataPntr->t = T;
               TRDataPntr->y = Y;
               }
            break;
         case X_Data:  /* Complex time series */
            if ((DFLAG = READINT(&X)) &&
                (DFLAG = READINT(&Y)))
               {
               XDataPntr->z.x = X;
               XDataPntr->z.y = Y;
               XDataPntr->f = 0;
               }
            break;
         case TX_Data:   /* Complex time labeled */
            if ((DFLAG = READINT(&T)) &&
                (DFLAG = READINT(&X)) &&
                (DFLAG = READINT(&Y)))
               {
               TXDataPntr->t = T;
               TXDataPntr->z.x = X;
               TXDataPntr->z.y = Y;
               }
            break;
         }
      if (ferror(INSTR))
         {
         zError();
         return(1);
         }
      if (DFLAG > 0)
         {
         if (Zwrite(OUTSTR,CONBUF,FileHeader.type)) return(1);
         ++TOTAL;
         }
      }
   return(0);
   }

/*********************************************************************
**
** LabView Geiger counter data to standard format
**
** The Geiger counter data are in a double, u16 structure
** The data are collected by the LabView FileGeiger program
** that collects data from the RM60 Geiger counter.
**
*/
short GTOS()
   {
   short DFLAG=1;
   double Y, T;

   if (Zputhead(OUTSTR,&FileHeader)) return(1);

   while (DFLAG) /* Use DFLAG to signal end of input file */
      {
      if ((DFLAG = READDOUBLEM(&T)) &&
          (DFLAG = READWORDM(&Y)))
         {
         TRDataPntr->t = T;
         TRDataPntr->y = Y;
         }

      if (ferror(INSTR))
         {
         zError();
         return(1);
         }

      if (DFLAG > 0)
         {
         if (Zwrite(OUTSTR,CONBUF,FileHeader.type)) return(1);
         ++TOTAL;
         }
      }

   return(0);
   }

/*********************************************************************
**
** Standard Format to ASCII (default is csv y or y,z or t,y or t,y,z)
*/
short STOA(short WhatType)
   {
   double T, Y, Z;
   struct FILEHDR FH;
   struct complex Zval;
   short flag;

   if (!strlen(TFORMAT)) strcpy(TFORMAT,"%lG,");  /* Setup default csv output */
   if (!strlen(YFORMAT)) strcpy(YFORMAT,"%lG");   /* formats if necessary     */
   if (!strlen(ZFORMAT)) strcpy(ZFORMAT,",%lG");

   if (!Zgethead(INSTR,&FH)) return(1);
   
   while (Zread(INSTR,CONBUF,FH.type))  /* Read 1 value */
      {
      if (extractValues(CONBUF, &FH, TOTAL, &T, &Y, &Zval, &flag))
         {
         zTaskMessage(10,"Unknown File Type.\n");
         return(1);
         }

      switch (FH.type)
         {
         case R_Data:
            if (WhatType) fprintf(OUTSTR,TFORMAT,T);
            fprintf(OUTSTR,YFORMAT,Y);
            break;
         case TR_Data:
            fprintf(OUTSTR,TFORMAT,T);
            fprintf(OUTSTR,YFORMAT,Y);
            break;
         case X_Data:
            T = TOTAL * FH.m + FH.b;
            Y = Zval.x;
            Z = Zval.y;
            if (WhatType) fprintf(OUTSTR,TFORMAT,T);
            fprintf(OUTSTR,YFORMAT,Y);
            fprintf(OUTSTR,ZFORMAT,Z);
            break;
         case TX_Data:
            Y = Zval.x;
            Z = Zval.y;
            fprintf(OUTSTR,TFORMAT,T);
            fprintf(OUTSTR,YFORMAT,Y);
            fprintf(OUTSTR,ZFORMAT,Z);
            break;
         }
      ++TOTAL;
      fprintf(OUTSTR,"\n");
      if (ferror(OUTSTR)) return(1);
      }
   return(0);
   }

/*********************************************************************
**
** Standard Format to 2 Byte Binary Integer (little-endian/Intel)
*/
short STOI()
   {
   short X, Y, T, F;
   struct FILEHDR FH;
   struct complex Zval;
   double time, rval;
   double TOTAL = 0.0;
   
   if (!Zgethead(INSTR,&FH)) return(1);

   while (Zread(INSTR,CONBUF,FH.type))
      {
      if (extractValues(CONBUF, &FH, TOTAL, &time, &rval, &Zval, &F))
         {
         zTaskMessage(10,"Unknown File Type.\n");
         return(1);
         }

      T = (short)time;

      switch (FH.type)
         {
         case R_Data:
            Y = (short)rval;
            fwrite(&Y,2,1,OUTSTR);
            fwrite(&F,2,1,OUTSTR);
            break;
         case TR_Data:
            Y = (short)rval;
            fwrite(&T,2,1,OUTSTR);
            fwrite(&Y,2,1,OUTSTR);
            break;
         case X_Data:
            X = (short)Zval.x;
            Y = (short)Zval.y;
            fwrite(&X,2,1,OUTSTR);
            fwrite(&Y,2,1,OUTSTR);
            fwrite(&F,2,1,OUTSTR);
            break;
         case TX_Data:
            X = (short)Zval.x;
            Y = (short)Zval.y;
            fwrite(&T,2,1,OUTSTR);
            fwrite(&X,2,1,OUTSTR);
            fwrite(&Y,2,1,OUTSTR);
            break;
         default:
            zTaskMessage(10,"Invalid File Type.\n");
            return(1);
         }
      if (ferror(OUTSTR)) return(1);

      ++TOTAL;
      }
   return(0);
   }

/***********************************************************************
**
** Read an ASCII value.
** Anything other than a white space, digit, comma, period, e, E, - or +
** is considered and error and kills the task.
** ANY white space or comma is considered a delimiter.
** An EOF returns 0, ERROR -1, else 1 is returned.  The calling
** program must determine if a NULL return is an ERROR or EOF.
*/
short READASCII(double *YP)
   {
   char *CPNTR;

   CPNTR = BUFFER;
   do *CPNTR = fgetc(INSTR);
   while (((*CPNTR<(char)33) || (*CPNTR == ',')) && (!feof(INSTR)));
   if (feof(INSTR) || ferror(INSTR)) return(0);
   do {
      *(++CPNTR) = fgetc(INSTR);
      if (ferror(INSTR))
         {
         zError();
         return(-1);
         }
      if ((!feof(INSTR))       &&
          (!isspace(*CPNTR))   &&
          (!isdigit(*CPNTR))   &&
          (*CPNTR != ',')      &&
          (*CPNTR != '.')      &&
          (*CPNTR != 'e')      &&
          (*CPNTR != 'E')      &&
          (*CPNTR != '-')      &&
          (*CPNTR != '+'))
            {
            zTaskMessage(10,"Not an ASCII File\n");
            return(-1);
            }
      }
   while ((!feof(INSTR)) && (*CPNTR != (char)13) && (*CPNTR != ',')
       && (!isspace(*CPNTR)));
   *CPNTR = 0;
   *YP = atof(BUFFER);
   return(1);
   }

/*********************************************************************
**
** Standard Format to Microsoft BASIC Binary (no longer implemented)
*/
short STOB()
   {
   short FILELEN, X, Y, T, F;
   struct FILEHDR FH;
   double TCNT = 0.;
   struct complex Zval;
   double time, rval;
   
   if (!Zgethead(INSTR,&FH)) return(1);           /* Read Header      */

   while (Zread(INSTR,CONBUF,FH.type)) ++TOTAL; /* Find File Length */

   if (!Zgethead(INSTR,(struct FILEHDR *)NULL)) /* Rewind file  */
      return(1);

   fputc(0xFD,OUTSTR); /* Always FD for a BSAVE file */
   fputc(0xDF,OUTSTR); /* Write out rest of BASIC header info */
   fputc(0x14,OUTSTR);
   fputc(0xA3,OUTSTR);
   fputc(0x72,OUTSTR);

   FILELEN = (short)(TOTAL*2);

   fwrite(&FILELEN,2,1,OUTSTR); /* size of file */

   while (Zread(INSTR,CONBUF,FH.type))
      {
      if (extractValues(CONBUF, &FH, TCNT, &time, &rval, &Zval, &F))
         {
         zTaskMessage(10,"Invalid Data Type.\n");
         return(1);
         }

      ++TCNT;

      switch (FH.type)
         {
         case R_Data:
            Y = (short)rval;
            fwrite(&Y,2,1,OUTSTR);
            fwrite(&F,2,1,OUTSTR);
            break;
         case TR_Data:
            T = (short)time;
            Y = (short)rval;
            fwrite(&T,2,1,OUTSTR);
            fwrite(&Y,2,1,OUTSTR);
            break;
         case X_Data:
            X = (short)Zval.x;
            Y = (short)Zval.y;
            fwrite(&X,2,1,OUTSTR);
            fwrite(&Y,2,1,OUTSTR);
            fwrite(&F,2,1,OUTSTR);
            break;
         case TX_Data:
            X = (short)Zval.x;
            Y = (short)Zval.y;
            fwrite(&T,2,1,OUTSTR);
            fwrite(&X,2,1,OUTSTR);
            fwrite(&Y,2,1,OUTSTR);
            break;
         }
      if (ferror(OUTSTR)) return(1);
      }
   fputc(0x1A,OUTSTR);  /* ^Z signals EOF for BASIC */
   return(0);
   }

/*********************************************************************
**
** Read a 2 Byte integer value. (little-endian/Intel)
** EOF RETURN 1 or ERROR returns -1, else 0 is returned.
*/
short READINT(double *YP)
   {
   short Y;

   fread(&Y,2,1,INSTR);
   if (feof(INSTR)) return(0);
   if (ferror(INSTR))
      {
      zError();
      return(-1);
      }
   *YP = (double)Y;
   return(1);
   }

/*********************************************************************
**
** Read a 2 Byte unsigned integer value (little-endian/Intel)
** EOF RETURN 1 or ERROR returns -1, else 0 is returned.
*/
short READWORD(double *YP)
   {
   WORD Y;

   fread(&Y,2,1,INSTR);
   if (feof(INSTR)) return(0);
   if (ferror(INSTR))
      {
      zError();
      return(-1);
      }
   *YP = (double)Y;
   return(1);
   }

/*********************************************************************
**
** Read a 2 Byte unsigned integer value in big-endian (Motorola) format
** EOF RETURN 1 or ERROR returns -1, else 0 is returned.
*/
short READWORDM(double *YP)
   {
   WORD Y;

   fread(&Y,2,1,INSTR);
   if (feof(INSTR)) return(0);
   if (ferror(INSTR))
      {
      zError();
      return(-1);
      }
   *YP = (double)SWAPWORD(Y);
   return(1);
   }

/*********************************************************************
**
** Read an 8 Byte double value
** EOF RETURN 1 or ERROR returns -1, else 0 is returned.
*/
short READDOUBLE(double *YP)
   {
   double Y;

   fread(&Y,8,1,INSTR);
   if (feof(INSTR)) return(0);
   if (ferror(INSTR))
      {
      zError();
      return(-1);
      }
   *YP = Y;
   return(1);
   }


/*********************************************************************
**
** Read an 8 Byte double value in big-endian (Motorola) format
** EOF RETURN 1 or ERROR returns -1, else 0 is returned.
*/
short READDOUBLEM(double *YP)
   {
   union {double v; BYTE b[8];} Y;
   BYTE bval;
   int i;

   fread(&Y.v,8,1,INSTR);
   if (feof(INSTR)) return(0);
   if (ferror(INSTR))
      {
      zError();
      return(-1);
      }

   for (i = 0; i < 4; ++i)
      {
      bval = Y.b[i];
      Y.b[i] = Y.b[7 - i];
      Y.b[7 - i] = bval;
      }

   *YP = Y.v;

   return(1);
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
