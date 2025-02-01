/*
**
** Dr. Eric R. Nelson
** 12/28/2016
**
** All BOOL functions return FALSE if OK or TRUE on an error
**
** List of Library Functions in this file:
**
** void printPercentComplete(long currentCount, long totalCounts, short reportInterval)
** BOOL getConfigString(PSTR key, int N, PSTR szValue)
** long filesize(FILE *pFile)
** void getIntFrac(double val, double *pIntval, double *pFracVal)
** BOOL isodd(double val)
** BOOL iseven(double val)
** double unbiasedRound(double val)
** long countDataRecords(FILE *stream)
** BOOL insertValues(char *BUFFER, struct FILEHDR *fileHeader, double TIME, double Rval, struct ** complex Zval, short FLAG)
** BOOL extractValues(char *BUFFER, struct FILEHDR *fileHeader, double TCNT, double *TIME, double *Rval, struct complex *Zval, short *FLAG)
** 
** FILE *zOpen(PSTR fileName, short type)
** short Zclose(FILE *PNTR)
** PSTR parseString(FILE* pFile, int N, char szStr[N])
** PSTR parseAdverb(FILE* pFile, int N, char szAdverb[N])
** BOOL zGetAdverbs(PSTR pTaskName)
** BOOL zPutAdverbs(PSTR pTaskName)
** short Zputhead(FILE *stream, struct FILEHDR *pHeader)
** struct FILEHDR *Zgethead(FILE *INSTR, struct FILEHDR *HeadStruct)
** BOOL isTisanHeader(struct FILEHDR *pHeader)
** void setHeaderText(struct FILEHDR *pHeader)
** void getHeaderText(struct FILEHDR *pHeader)
** BOOL zTaskInit(PSTR Argv0)
** short zNameOutputFile(char *OUTFILE, char *TMPFILE)
** void zError()
** long zGetData(long nRecords, FILE *INSTR, char *BUFFER, short dataType)
** long zPutData(long N, FILE *OUTSTR, char *BUFFER, short dataType)
** short Zwrite(FILE *OUTSTR, char *DATA, short dataType)
** char *Zread(FILE *INSTR, char *DATA, short dataType)
** short Zsize(short dataType)
** void BEEP()
** void Zexit(int N)

** double c_abs(struct complex z)
** struct complex c_add(struct complex z1, struct complex z2)
** struct complex c_sub(struct complex z1, struct complex z2)
** struct complex c_mul(struct complex z1, struct complex z2)
** struct complex c_div(struct complex z1, struct complex z2)
** struct complex c_sqrt(struct complex z)
** struct complex c_ln(struct complex z)
** struct complex c_exp(struct complex z)
** struct complex c_sin(struct complex z)
** struct complex c_cos(struct complex z)
** struct complex c_tan(struct complex z)
** struct complex c_sec(struct complex z)
** struct complex c_csc(struct complex z)
** struct complex c_cot(struct complex z)
** struct complex c_asin(struct complex z)
** struct complex c_acos(struct complex z)
** struct complex c_atan(struct complex z)
** struct complex c_acot(struct complex z)
** struct complex c_acsc(struct complex z)
** struct complex c_asec(struct complex z)
** struct complex c_sinh(struct complex z)
** struct complex c_cosh(struct complex z)
** struct complex c_tanh(struct complex z)
** struct complex c_sech(struct complex z)
** struct complex c_csch(struct complex z)
** struct complex c_coth(struct complex z)
** struct complex c_asinh(struct complex z)
** struct complex c_acosh(struct complex z)
** struct complex c_atanh(struct complex z)
** struct complex c_acsch(struct complex z)
** struct complex c_asech(struct complex z)
** struct complex c_acoth(struct complex z)
** struct complex c_log10(struct complex z)
** struct complex c_pow(struct complex z1, struct complex z2)
** double asinh(double x)
** double acosh(double x)
** double atanh(double x)
** struct complex cmplx(double r, double i)
** 
** short displayText(FILE *IndexTableStream)
** short WriteString(char *StringPointer, FILE *IndexTableStream)
** void displayInputs(unsigned char InputsAdverbToken, FILE *IndexTableStream)
** BOOL displayTaskInputs(char *cPointer)
** struct CATSTRUCT *ZCatFiles(char* pPath)
** PSTR zBuildFileName(short TYPE, PSTR cPointer)
** short zMessage(short level, const char *format, ...)
** short zTaskMessage(short level, const char *format, ...)
** void initializeAdverbArrays()
*/
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "tisan.h"

char const szTisanSignature[] = "TISAN\0\r\n";    // Must be 8 bytes plus the nul
char const szEndBytes[] = "\0\r\n";               // Must be 3 bytes plus the nul

/*
** Print %pass Complete in "reportInterval" percent intervals
**
** You should call it outside of the processing loop with reportInterval equal to 0 to ensure
** it gives the correct percentages when processing multiple files, then call it
** at the bottom of the processing loop with reset FALSE, and then again just outside
** the processing loop after the loop has completed (to print 100% complete).
** 
*/
void printPercentComplete(long currentCount, long totalCounts, short reportInterval)
   {
   static int pass; // needs to be saved for when we come back
   int v;

   if (reportInterval < 1)
      pass = 0;
   else
      {
      v = (int)(100. * (double)currentCount / (double)totalCounts);

      if (v == pass)
         {
         zTaskMessage(3,"%d%% Complete\n", pass);
         pass += (int)reportInterval;
         }
      }

   return;
   }

/*
** Function to get a configuration string in TISAN.CFG based on the key passed to it
**
** The TISAN.CFG file has the same structure as the inputs files
** key=value
**
** So we can use the same functions for passing adverbs from the inputs files.
*/
BOOL getConfigString(PSTR key, int N, PSTR szValue)
   {
   char path[_MAX_PATH];
   char szKeyEntry[256];
   FILE *pFile;
   BOOL bFoundMatch = FALSE;
   
   makePath(path, TisanDrive, TisanDir, "TISAN", "CFG"); // TISABN.CFG needs to be in the same folder as the tisan executable

   pFile = fopen(path,"rt");
   
   if (pFile)
      {
      do {
         parseAdverb(pFile, sizeof(szKeyEntry), szKeyEntry);
         parseString(pFile, N, szValue);
         if (!strcmp(key, szKeyEntry)) bFoundMatch = TRUE;
         }
      while (!bFoundMatch && !feof(pFile) && !ferror(pFile));
      
      fclose(pFile);

      if (!bFoundMatch)
         {
         szValue[0] = NUL; // No match so set the return string to empty
         zTaskMessage(8, "Key '%s' not found in TISAN.CFG.\n", key);
         }
      }
   else
      {
      zTaskMessage(10,"Configuration File '%s' not found.\n", path);
      }

   return(bFoundMatch);
   }

long filesize(FILE *pFile)
   {
   long lpos, length;
   
   lpos = ftell(pFile);
   fseek(pFile,0L,SEEK_END);
   length = ftell(pFile);
   fseek(pFile,lpos,SEEK_SET);

   return(length);
   }

void getIntFrac(double val, double *pIntval, double *pFracVal)
   {
   *pIntval  = (double)((long)val);      // get the integer part
   *pFracVal = val - *pIntval;           // get the fractional part
   
   return;
   }

BOOL isodd(double val)
   {
   double fracVal, intVal;
   BOOL isOdd = FALSE;

   val /= 2.;   

   getIntFrac(val, &intVal, &fracVal);

   if (fracVal) isOdd = TRUE;

   return(isOdd);
   }

BOOL iseven(double val)
   {
   return(!isodd(val));
   }

/*
** Unbiased round to nearest integer value.
** Round up if fraction >0.5, round down if fraction < 0.5, round to even value if fraction = 0.5
*/
double unbiasedRound(double val)
   {
   double fracVal, intVal;
   
   getIntFrac(val, &intVal, &fracVal);

   if ((fracVal > 0.5) || ((fracVal == 0.5) && isodd(intVal))) ++intVal;

   return(intVal);
   }

/*
** Count the number of data records in a TISAN file
** Restore the file pointer when done.
*/
long countDataRecords(FILE *stream)
   {
   struct FILEHDR localFileHeader;
   char bitbucket[sizeof(struct TRData)]; // Largest data typew
   long lCount = 0L;
   long lpos;
   
   lpos = ftell(stream);
   
   Zgethead(stream,&localFileHeader);
   
   while (Zread(stream,bitbucket,localFileHeader.type)) ++lCount;

   fseek(stream,lpos,SEEK_SET);
   
   return(lCount);
   }

/*
** Given a pointer to the data buffer and the file type, take the time, real value, complex value and flag and
** then put them into the appropriate structure elements for the selected file type.
*/
BOOL insertValues(char *BUFFER, struct FILEHDR *fileHeader, double TIME, double Rval, struct complex Zval, short FLAG)
   {
   struct RData  *RDataPntr;
   struct TRData *TRDataPntr;
   struct XData  *XDataPntr;
   struct TXData *TXDataPntr;
   BOOL bError = FALSE;

   RDataPntr =  (struct RData *)BUFFER;
   TRDataPntr = (struct TRData *)BUFFER;
   XDataPntr =  (struct XData *)BUFFER;
   TXDataPntr = (struct TXData *)BUFFER;

   switch (fileHeader->type)  /* Fill DATA, TIME and FLAG values */
      {
      case R_Data:    /* Real Time Series */
         RDataPntr->y = Rval;
         RDataPntr->f = FLAG;
         break;
      case TR_Data:  /* Real Time Labeled */
         TRDataPntr->y = Rval;
         TRDataPntr->t = TIME;
         break;
      case X_Data:   /* Complex Time Series */
         XDataPntr->z = Zval;
         XDataPntr->f = FLAG;
         break;
      case TX_Data:   /* Complex Time Labeled */
         TXDataPntr->z = Zval;
         TXDataPntr->t = TIME;
         break;
      default: // Error
         bError = TRUE;
      }

   return(bError);
   }

/*
** Given one data value read in from disk into BUFFER, extract the time, value, zvalue, and flag
** Return TRUE if an unknown data type is encountered.
*/
BOOL extractValues(char *BUFFER, struct FILEHDR *fileHeader, double TCNT, double *TIME, double *Rval, struct complex *Zval, short *FLAG)
   {
   struct RData  *RDataPntr;
   struct TRData *TRDataPntr;
   struct XData  *XDataPntr;
   struct TXData *TXDataPntr;
   BOOL bError = FALSE;

   RDataPntr =  (struct RData *)BUFFER;
   TRDataPntr = (struct TRData *)BUFFER;
   XDataPntr =  (struct XData *)BUFFER;
   TXDataPntr = (struct TXData *)BUFFER;

   switch (fileHeader->type)  /* Fill DATA, TIME and FLAG values */
      {
      case R_Data:    /* Real Time Series */
         *Rval = RDataPntr->y;
         *TIME = TCNT*fileHeader->m + fileHeader->b;
         *FLAG = RDataPntr->f;
         break;
      case TR_Data:  /* Real Time Labeled */
         *Rval = TRDataPntr->y;
         *TIME = TRDataPntr->t;
         *FLAG = 0.;
         break;
      case X_Data:   /* Complex Time Series */
         *Zval = XDataPntr->z;
         *Rval = c_abs(XDataPntr->z);
         *TIME = TCNT*fileHeader->m + fileHeader->b;
         *FLAG = XDataPntr->f;
         break;
      case TX_Data:   /* Complex Time Labeled */
         *Zval = TXDataPntr->z;
         *Rval = c_abs(XDataPntr->z);
         *TIME = TXDataPntr->t;
         *FLAG = 0.;
         break;
      default: // Error
         bError = TRUE;
      }

   return(bError);
   }


/*
** These I/O routines are called in the same way as their
** WINDOWS counterparts.  The initial definitions are needed
** to maintain WINDOWS portability by the calling functions.
**
** I/O errors can be detected by examining the iEoF structure
** element.  A Zero value indicates no problems, 1 means end of file
** and -1 means that an error has been detected.
*/

/*********************************************************************
*
* Open a file specified by the pointer to fileName of the
* type specified by type.  type must be one of the following
* 
* O_readb   :  Open for binary read
* O_writeb  :  Open for binary write
* O_appendb :  Open for binary append
* O_readt   :  Open for text read
* O_writet  :  Open for text write
* O_appendt :  Open for text append
*
* If the file is successfully opened, then a file pointer is returned
* otherwise an error message is printed and NULL is returned.
*
** The file is opened as a FILE stream.
*/
FILE *zOpen(PSTR fileName, short type)
   {
   char mode[8];
   FILE *pFile = (FILE*)NIL;
   BOOL bError = FALSE;
   extern const char szTask[];
   
   switch (type)
      {
      case O_readb: /* Open for binary read */
         strcpy(mode,"r+b");
         break;
      case O_writeb: /* Open for binary write */
        strcpy(mode,"w+b");
        break;
      case O_appendb: /* Open for binary append */
         strcpy(mode,"a+b");
         break;
      case O_readt: /* Open for text read */
         strcpy(mode,"r+t");
         break;
      case O_writet: /* Open for text write */
         strcpy(mode,"w+t");
         break;
      case O_appendt: /* Open for text append */
         strcpy(mode,"a+t");
         break;
      default:
         zMessage(10,"%-8s: Unknown type specified in zOpen %hd\n",szTask, type);
         bError = TRUE;
      }

   if (!bError)
      {
      pFile = fopen(fileName,mode);

      if (!pFile)
         {
         zError();
         BEEP();
         }
      }

   return(pFile);
   }

/*********************************************************************
*
* Close a file specified by the file pointer PNTR.
* Return 0 if no errors.
* If an error occurs it is printed and 1 is returned.
*
*/
short Zclose(FILE *PNTR)
   {
   short ERRFLAG=0;
   extern const char szTask[];

   if (fclose(PNTR))
      {
      zMessage(10,"%-8s: Error Closing File\n",szTask);
      ERRFLAG=1;
      }

   return(ERRFLAG);
   }

/*
** The %s option for fscanf does not handle spaces well, so just handle strings ourselves.
*/
PSTR parseString(FILE* pFile, int N, char szStr[N])
   {
   BOOL done = FALSE;
   char c;
   int index = 0;

   --N; // Make room for the terminating null byte

   do {
      c = fgetc(pFile);

      if ((c == '\n') || (feof(pFile)))
         done = TRUE;
      else
         {
         if (index < N) szStr[index] = c;
         ++index;
         }
      }
   while(!done);

   szStr[index] = NUL; // Add terminating null byte to make it a string.

   return(szStr);
   }

/*****************************************************
**
** Function to parse in the adverb in a INP file.
** The file format is ADVERB=values
** There can be no extra spaces before or after the ADVERB label
** The code scans the file one character at a time looking for the '='.
** As long as there is space in the receiving buffer, characters
** are placed into it until the '=' is reached. It also stops in the
** event of an EOF or NUL byte (which should never happen).
*/
PSTR parseAdverb(FILE* pFile, int N, char szAdverb[N])
   {
   BOOL done = FALSE;
   char c;
   int index = 0;

   --N; // Make room for the terminating null byte

   do {
      c = fgetc(pFile);

      if ((c == '=') || (feof(pFile)))
         done = TRUE;
      else
         {
         if (index < N) szAdverb[index] = c;
         ++index;
         }
      }
   while(!done);

   szAdverb[index] = NUL; // Add terminating null byte to make it a string.

   return(szAdverb);
   }


/*********************************************************************
*
* Get Adverbs from the file name pointed to by pFile.
* FALSE is returned if no errors.
* TRUE is returned otherwise and an error message is printed.
* Input file names are of the form inputs\filename.INP
* All the inputs files are stored in the inputs folder which must
* be in the same folder as the TISAN executable.
*
*/
BOOL zGetAdverbs(PSTR pTaskName)
   {
   char dir[_MAX_DIR];
   char path[_MAX_PATH];
   FILE *pFile;
   int i;
   char szAdverb[16];
   double* pDouble;
   short* pShort;
   int indexAdverb;
   BOOL bError=FALSE;
   extern const char szTask[];

   strcpy(dir,TisanDir);
   strcat(dir,INPUTSDIR);
   makePath(path,TisanDrive,dir,pTaskName,INPUTSEXT);

   pFile = fopen(path,"rt");
   
   if (pFile)
      {
      while (!feof(pFile))
         {
         parseAdverb(pFile, sizeof(szAdverb), szAdverb);

         if (szAdverb[0] != NUL)
            {
            indexAdverb = -1;
            for (i = 0; i < ADVCOUNT; ++i)
               {
               if (strcmp(szAdverb, ADVSTR[i]) == 0) indexAdverb = i;
               }

            switch (indexAdverb)
               {
// Strings
               case 0: // TASKNAME
               case 1: // INNAME
               case 2: // INCLASS
               case 3: // INPATH
               case 4: // IN2NAME
               case 5: // IN2CLASS
               case 6: // IN2PATH
               case 7: // IN3NAME
               case 8: // IN3CLASS
               case 9: // IN3PATH
               case 10: // OUTNAME
               case 11: // OUTCLASS
               case 12: // OUTPATH
               case 20: // TFORMAT
               case 21: // YFORMAT
               case 22: // ZFORMAT
               case 23: // DEVICE
               case 27: // PARITY
               case 37: // TLABEL
               case 38: // YLABEL
               case 39: // ZLABEL
               case 40: // TITLE
                  parseString(pFile, ADVSIZE[indexAdverb], ADVPNTR[indexAdverb]);
                  break;
// short scalars
               case 13: // CODE
               case 14: // ITYPE
               case 24: // BAUD
               case 25: // STOPBITS
               case 26: // PROGRESS
               case 28: // FRAME
               case 29: // BORDER
               case 31: // TMINOR
               case 32: // YMINOR
               case 33: // ZMINOR
               case 42: // QUIET
                  fscanf(pFile, "%hd\n", (short*)ADVPNTR[indexAdverb]);
                  break;
// long scalars
               case 30: // COLOR
                  fscanf(pFile, "%d\n", (LONG*)ADVPNTR[indexAdverb]);
                  break;
// double scalars
               case 15: // FACTOR
                  fscanf(pFile, "%lg\n", (double*)ADVPNTR[indexAdverb]);
                  break;
// double vectors [2]
               case 16: // TRANGE
               case 17: // YRANGE
               case 18: // ZRANGE
               case 34: // TMAJOR
               case 35: // YMAJOR
               case 36: // ZMAJOR
               case 43: // POINT
               case 44: // ZFACTOR
                  pDouble = (double*)ADVPNTR[indexAdverb];
                  fscanf(pFile, "%lg,%lg\n", pDouble, pDouble+1);
                  break;
// short Arrays [4]
               case 19: // WINDOW
                  pShort = (short*)ADVPNTR[indexAdverb];
                  fscanf(pFile, "%hd,%hd,%hd,%hd\n", pShort, pShort+1, pShort+2, pShort+3);
                  break;
// double Arrays [10]
               case 41: // PARMS
                  pDouble = (double*)ADVPNTR[indexAdverb];
                  fscanf(pFile, "%lg,%lg,%lg,%lg,%lg,%lg,%lg,%lg,%lg,%lg\n", pDouble, pDouble+1, pDouble+2, pDouble+3, pDouble+4, pDouble+5, pDouble+6, pDouble+7, pDouble+8, pDouble+9);
                  break;
             
               default:
                  printf("Unknown Adverb '%s' in INP File for task '%s' in function zGetAdverbs\n", szAdverb, TASKNAME);
                  bError=TRUE;
               } // switch (indexAdverb)
            } // if (szAdverb[0] != NUL)
         } // while (!feof(pFile))

      if (ferror(pFile) != 0)
         {
         zMessage(10,"%-8s: Error Reading Inputs File '%s' - %s\n",szTask,path,strerror(errno));
         bError=TRUE;
         }

      fclose(pFile);
      } // if (pFile)
   else
      {
      bError=TRUE;
      zMessage(10,"%-8s: Sorry, Inputs File '%s' is not available.\n",
                   szTask,                   path);
     }

   return(bError);
   }

/*********************************************************************
*
* Put Adverbs to the file name pointed to by pTaskName.
* FALSE is returned if no errors.
* TRUE is returned otherwise and an error message is printed.
* Input file names are of the form inputs\filename.inp
*
*/
BOOL zPutAdverbs(PSTR pTaskName)
   {
   char dir[_MAX_DIR];
   char path[_MAX_PATH];
   FILE *pFile;
   double* pDouble;
   short* pShort;
   int indexAdverb;
   BOOL bError=FALSE;
   extern const char szTask[];
   
   strcpy(dir,TisanDir);
   strcat(dir,INPUTSDIR);
   makePath(path,TisanDrive,dir,pTaskName,INPUTSEXT);

   pFile = fopen(path,"wt");
   
   if (pFile)
      {
      for (indexAdverb = 0; indexAdverb < ADVCOUNT; ++indexAdverb)
         {
         switch (indexAdverb)
            {
// Strings
            case 0: // TASKNAME
            case 1: // INNAME
            case 2: // INCLASS
            case 3: // INPATH
            case 4: // IN2NAME
            case 5: // IN2CLASS
            case 6: // IN2PATH
            case 7: // IN3NAME
            case 8: // IN3CLASS
            case 9: // IN3PATH
            case 10: // OUTNAME
            case 11: // OUTCLASS
            case 12: // OUTPATH
            case 20: // TFORMAT
            case 21: // YFORMAT
            case 22: // ZFORMAT
            case 23: // DEVICE
            case 27: // PARITY
            case 37: // TLABEL
            case 38: // YLABEL
            case 39: // ZLABEL
            case 40: // TITLE
               fprintf(pFile, "%s=%s\n", ADVSTR[indexAdverb], ADVPNTR[indexAdverb]);
               break;
// short scalars
            case 13: // CODE
            case 14: // ITYPE
            case 24: // BAUD
            case 25: // STOPBITS
            case 26: // PROGRESS
            case 28: // FRAME
            case 29: // BORDER
            case 31: // TMINOR
            case 32: // YMINOR
            case 33: // ZMINOR
            case 42: // QUIET
               fprintf(pFile, "%s=%hd\n", ADVSTR[indexAdverb], *(short*)ADVPNTR[indexAdverb]);
               break;
// long scalars
            case 30: // COLOR
               fprintf(pFile, "%s=%d\n", ADVSTR[indexAdverb], *(LONG*)ADVPNTR[indexAdverb]);
               break;
// double scalars
            case 15: // FACTOR
               fprintf(pFile, "%s=%.12lg\n", ADVSTR[indexAdverb], *(double*)ADVPNTR[indexAdverb]);
               break;
// double vectors [2]
            case 16: // TRANGE
            case 17: // YRANGE
            case 18: // ZRANGE
            case 34: // TMAJOR
            case 35: // YMAJOR
            case 36: // ZMAJOR
            case 43: // POINT
            case 44: // ZFACTOR
               pDouble = (double*)ADVPNTR[indexAdverb];
               fprintf(pFile, "%s=%.12lg,%.12lg\n", ADVSTR[indexAdverb], *pDouble, *(pDouble+1));
               break;
// short Arrays [4]
            case 19: // WINDOW
               pShort = (short*)ADVPNTR[indexAdverb];
               fprintf(pFile, "%s=%hd,%hd,%hd,%hd\n", ADVSTR[indexAdverb], *pShort, *(pShort+1), *(pShort+2), *(pShort+3));
               break;
// double Arrays [10]
            case 41: // PARMS
               pDouble = (double*)ADVPNTR[indexAdverb];
               fprintf(pFile, "%s=%.12lg,%.12lg,%.12lg,%.12lg,%.12lg,%.12lg,%.12lg,%.12lg,%.12lg,%.12lg\n", ADVSTR[indexAdverb],
                             *pDouble, *(pDouble+1), *(pDouble+2), *(pDouble+3), *(pDouble+4), *(pDouble+5), *(pDouble+6), *(pDouble+7), *(pDouble+8), *(pDouble+9));
               break;
             
            default:
               printf("Error Should Not Occur %d! Writing INP File for task '%s' in function Zputadv\n", indexAdverb, TASKNAME);
               bError=TRUE;
            } // switch (indexAdverb)
         } // for (indexAdverb = 0; i < ADVCOUNT; ++indexAdverb)

      if (ferror(pFile))
         {
         zMessage(10,"%-8s: Error Writing Inputs File '%s'\n",
                  szTask,path);
         zError();
         bError=TRUE;
         }

      fclose(pFile);
      } // if (pFile)
   else
      {
      bError=TRUE;
      zMessage(10,"%-8s: Error Opening Inputs File '%s'\n",
              szTask,path);
      }

   return(bError);
   }

/*********************************************************************
*
*  Put a file header to disk.
*  The signature and identifying text is added to the header.
*  The text fields end in \0\r\n so that they can be easily seen in an editor on any system
*
*  Returns 0 if no errors.
*  Returns 1 on error and prints a message.
*
*/
short Zputhead(FILE *stream, struct FILEHDR *pHeader)
     {
     memcpy(pHeader->szTISAN, szTisanSignature, 8);   // Always set the signature

     rewind(stream);
     fwrite(pHeader,sizeof(struct FILEHDR),1,stream);

     if (ferror(stream))
        {
        zError();
        return(1);
        }

     return(0);
     }

/*********************************************************************
*
*  Get file header and return pointer to FILEHDR structure.
*  Returns NULL on error and prints a message.
*
* If HeadStruct is a null pointer then just advanced past the file
* header and return a non-null value. In this case the return value is
* NOT a valid pointer.
*
*/
struct FILEHDR *Zgethead(FILE *INSTR, struct FILEHDR *HeadStruct)
   {
   rewind(INSTR);

   if (!HeadStruct)
      {
      fseek(INSTR, (long)sizeof(struct FILEHDR), SEEK_SET);
      HeadStruct = (struct FILEHDR *)TRUE;                     // NOT A VALID POINTER! Used ONLY to indicate there were no errors
      }
   else
      {
      fread(HeadStruct,sizeof(struct FILEHDR),1,INSTR);
      if (!ferror(INSTR) && !isTisanHeader(HeadStruct))
         {
         zTaskMessage(10,"Not a TISAN Data File.\n");
         HeadStruct = NULL;
         }
      }

   if (ferror(INSTR))
      {
      zError();
      HeadStruct = (struct FILEHDR *)NIL;
      }
   
     return(HeadStruct);
     }

/*******************************************
*
* Tests if the current header is a TISAN file
** by looking for the signature
*/
BOOL isTisanHeader(struct FILEHDR *pHeader)
   {
   BOOL bTisanHeader;
   union {char szTisan[8]; DWORD dwTisan;} expected, readin;
   
   memcpy(readin.szTisan,   pHeader->szTISAN, 8);
   memcpy(expected.szTisan, szTisanSignature, 8);
   
   if (readin.dwTisan == expected.dwTisan)
      bTisanHeader = TRUE;
   else
      bTisanHeader = FALSE;

   return(bTisanHeader);
   }

/*******************************************
*
* sets the header text fields based on
* TITLE, TLABEL, YLABEL
* and sets the last 3 bytes to \0\r\n
*/
void setHeaderText(struct FILEHDR *pHeader)
   {
   strncpy(pHeader->szTitle,  TITLE,  LABELSIZE);
   memcpy(&pHeader->szTitle[LABELSIZE - 4], szEndBytes, 3);

   strncpy(pHeader->szTlabel, TLABEL, LABELSIZE);
   memcpy(&pHeader->szTlabel[LABELSIZE - 4], szEndBytes, 3);

   strncpy(pHeader->szYlabel, YLABEL, LABELSIZE);
   memcpy(&pHeader->szYlabel[LABELSIZE - 4], szEndBytes, 3);

   return;
   }

/*******************************************
*
* gets the header text fields into
* TITLE, TLABEL, YLABEL
*/
void getHeaderText(struct FILEHDR *pHeader)
   {
   strcpy(TITLE,  pHeader->szTitle);
   strcpy(TLABEL,  pHeader->szTlabel);
   strcpy(YLABEL,  pHeader->szYlabel);
   return;
   }

/*********************************************************************
*
*  Initialize Task.
*  Gets the adverbs from the inputs file, prints a message
*  and displays the inputs.
*  Returns FALSE if no errors.
*  Returns TRUE on error.
*/
BOOL zTaskInit(PSTR Argv0)
   {
   long LTIME;
   char fname[_MAX_FNAME], ext[_MAX_EXT];
   BOOL bError;
   extern const char szTask[];
   
   initializeAdverbArrays();

   splitPath(Argv0,TisanDrive,TisanDir,fname,ext);

   zTaskMessage(0,"Task %s Begins\n",szTask);

   bError = zGetAdverbs((PSTR)szTask);

   strcpy(TASKNAME,szTask);
   
   if (!bError)
      {
      time(&LTIME);
      zTaskMessage(2,"Got Inputs on %s",ctime(&LTIME));
      bError = displayTaskInputs(TASKNAME);
      }

   return(bError);
   }

/*********************************************************************
*
*  Name the Final Output File.
*  Deletes the file OUTFILE and names the file TMPFILE to OUTFILE.
*  Returns 0 if no errors.
*  Returns 1 on error and prints a message.
*
*/
short zNameOutputFile(char *OUTFILE, char *TMPFILE)
   {
   extern const char szTask[];
   short ERRFLAG = 0;

   zTaskMessage(2,"Naming File '%s'\n",OUTFILE);
   unlink(OUTFILE);

   if (rename(TMPFILE,OUTFILE))
      {
      zError();
      ERRFLAG = 1;
      }

   return(ERRFLAG);
   }

/*********************************************************************
*
*  Use zMessage to Print a File Error.
*
*/
void zError()
   {
   extern const char szTask[];

   zMessage(10,"%-8s",szTask);
   zMessage(10,": %s\n",strerror(errno));
   return;
   }

/*********************************************************************
*
*  Get Data from INSTR into BUFFER and return number of bytes read
*  dataType is type of data;
*/
long zGetData(long nRecords, FILE *INSTR, char *BUFFER, short dataType)
   {
   long N;

   N = fread(BUFFER, Zsize(dataType), nRecords, INSTR);
   if (ferror(INSTR))
      {
      zError();
      N = 0L;
      }
   return(N);
   }

/*********************************************************************
*
*  Write out data buffer
* Returns the number of bytes written
*
*/
long zPutData(long N, FILE *OUTSTR, char *BUFFER, short dataType)
   {
   long bytesWritten = 0L;

   if (N >= 1L)
      {
      bytesWritten = fwrite(BUFFER, (size_t)Zsize(dataType), (size_t)N, OUTSTR);

      if (ferror(OUTSTR)) zError();
      }

   return(bytesWritten);
   }

/*********************************************************************
*
*  Write out one data value
*  Returns 0 if no errors.
*  Returns 1 on error and prints a message.
*
*/
short Zwrite(FILE *OUTSTR, char *DATA, short dataType)
   {
   fwrite(DATA,Zsize(dataType),1,OUTSTR);

   if (ferror(OUTSTR))
      {
      zError();
      return(1);
      }

   return(0);
   }

/*********************************************************************
*
*  Read in one data value, return NULL if EOF or ERROR
*
*/
char *Zread(FILE *INSTR, char *DATA, short dataType)
   {
   fread(DATA,Zsize(dataType),1,INSTR);

   if (ferror(INSTR) || feof(INSTR))
      {
      if (ferror(INSTR)) zError();
      return((char*)NIL);
      }

   return(DATA);
   }

/*********************************************************************
*
*  Return the size of dataType
*
*/
short Zsize(short dataType)
   {
   short M=0;

   switch (dataType)
      {
      case R_Data:
         M = sizeof(struct RData);
         break;
      case TR_Data:
         M = sizeof(struct TRData);
         break;
      case X_Data:
         M = sizeof(struct XData);
         break;
      case TX_Data:
         M = sizeof(struct TXData);
         break;
      }

   return(M);
   }

/*********************************************************************
**
** Command to sound bell
*/
void BEEP()
   {
   printf("\a");
   return;
   }

/*********************************************************************
**
** Exit a task
*/
void Zexit(int N)
   {
   extern const char szTask[];
   long LTIME;
   
   ZCatFiles(NULL);  /* Possibly Deallocate CatFiles Memory */
   
   time(&LTIME);

   if (N == 256)
      printf("%-8s: Task %s ABORTS on %s", szTask, szTask, ctime(&LTIME));
   else if (N == 255)
      printf("%-8s: Task %s Dies of a Floating Point Exception on %s",szTask, szTask, ctime(&LTIME));
   else if (N)
      {
      printf("%-8s: Task %s Dies Unexpectedly on %s",szTask, szTask, ctime(&LTIME));
      }
    else
      {
      printf("%-8s: Task %s Successfully Completes on %s",szTask, szTask, ctime(&LTIME));      }

   exit(N);
   }
   
/*
** This library defines the complex equivalents to:
**
**    addition
**    subtraction
**    multiplication
**    division
**    square root
**    natural log (ln)
**    exponentiation (exp)
**    sin
**    cos
**    tan
**    sec
**    csc
**    cot
**    arcsin
**    arccos
**    arctan
**    arcsec
**    arccsc
**    arccot
**    sinh
**    cosh
**    tanh
**    sech
**    csch
**    coth
**    arcsinh
**    arccosh
**    arctanh
**    arcsech
**    arccsch
**    arccoth
**    log10
**    pow
*/

#define LN10 2.302585092994046

double c_abs(struct complex z)
   {
   return(sqrt(Square(z.x)+Square(z.y)));
   }

struct complex c_add(struct complex z1, struct complex z2)
   {
   struct complex zz;

   zz.x = z1.x + z2.x;
   zz.y = z1.y + z2.y;
   return(zz);
   }

struct complex c_sub(struct complex z1, struct complex z2)
   {
   struct complex zz;

   zz.x = z1.x - z2.x;
   zz.y = z1.y - z2.y;
   return(zz);
   }

struct complex c_mul(struct complex z1, struct complex z2)
   {
   struct complex zz;

   zz.x = (z1.x * z2.x) - (z1.y * z2.y);
   zz.y = (z1.x * z2.y) + (z1.y * z2.x);
   return(zz);
   }

struct complex c_div(struct complex z1, struct complex z2)
   {
   struct complex zz;
   double mag;

   zz.x = ((z1.x * z2.x) + (z1.y * z2.y))/
          (mag = z2.x*z2.x + z2.y*z2.y);
   zz.y = ((z2.x * z1.y) - (z1.x * z2.y))/mag;
   return(zz);
   }

struct complex c_sqrt(struct complex z)
   {
   struct complex zz;
   double sqrt_r, half_theta;

   zz.x = (sqrt_r = sqrt(c_abs(z))) *
          cos(half_theta = atan2(z.y,z.x)/2.);
   zz.y = sqrt_r * sin(half_theta);
   return(zz);
   }

struct complex c_ln(struct complex z)
   {
   struct complex zz;

   zz.x = log(c_abs(z));
   zz.y = atan2(z.y,z.x);
   return(zz);
   }

struct complex c_exp(struct complex z)
   {
   struct complex zz;
   double ex;

   zz.x = (ex = exp(z.x)) * cos(z.y);
   zz.y = ex * sin(z.y);
   return(zz);
   }

struct complex c_sin(struct complex z)
   {
   double v1, v2;
   struct complex zz;

   zz.x =  ((v2 = exp(-z.y)) + (v1 = exp(z.y)))*sin(z.x)/2.;
   zz.y = -(v2 - v1)*cos(z.x)/2.;
   return(zz);
   }

struct complex c_cos(struct complex z)
   {
   double v1, v2;
   struct complex zz;

   zz.x =  ((v2 = exp(-z.y)) + (v1 = exp(z.y)))*cos(z.x)/2.;
   zz.y =  (v2 - v1)*sin(z.x)/2.;
   return(zz);
   }

struct complex c_tan(struct complex z)
   {
   return(c_div(c_sin(z),c_cos(z)));
   }

struct complex c_sec(struct complex z)
   {
   struct complex zz;

   zz.x = 1.;
   zz.y = 0.;
   return(c_div(zz,c_cos(z)));
   }

struct complex c_csc(struct complex z)
   {
   struct complex zz;

   zz.x = 1.;
   zz.y = 0.;
   return(c_div(zz,c_sin(z)));
   }

struct complex c_cot(struct complex z)
   {
   return(c_div(c_cos(z),c_sin(z)));
   }

struct complex c_asin(struct complex z)   /* -i*ln(iz + sqrt(1 - z^2)) */
   {
   struct complex zz;
   double v;

   zz.x = 1. - (z.x*z.x - z.y*z.y);
   zz.y = -2. * z.x*z.y;
   zz = c_sqrt(zz);
   zz.x -= z.y;
   zz.y += z.x;
   zz = c_ln(zz);
   v = zz.x;
   zz.x = zz.y;
   zz.y = -v;
   return(zz);
   }

struct complex c_acos(struct complex z)     /* -i*ln(iz + sqrt(z^2 - 1)) */
   {
   struct complex zz;
   double v;

   zz.x = (z.x*z.x - z.y*z.y) - 1;
   zz.y = 2. * z.x*z.y;
   zz = c_sqrt(zz);
   zz.x += z.x;
   zz.y += z.y;
   zz = c_ln(zz);
   v = zz.x;
   zz.x = zz.y;
   zz.y = -v;
   return(zz);
   }

struct complex c_atan(struct complex z)
   {
   struct complex zz1, zz2, zz;
   double v;

   zz1.x = 1. - z.y;
   zz1.y =  z.x;
   zz2.x = 1. + z.y;
   zz2.y = -z.x;
   zz = c_ln(c_div(zz1,zz2));
   v = zz.x/2.;
   zz.x = zz.y/2.;
   zz.y = -v;
   return(zz);
   }

struct complex c_acot(struct complex z)
   {
   struct complex zz1, zz2, zz;
   double v;

   zz1.x = z.x;
   zz1.y = z.y + 1.;
   zz2.x = z.x;
   zz2.y = z.y - 1.;
   zz = c_ln(c_div(zz1,zz2));
   v = zz.x/2.;
   zz.x = zz.y/2.;
   zz.y = -v;
   return(zz);
   }

struct complex c_acsc(struct complex z)
   {
   struct complex zz;
   double v;

   zz.x = (z.x*z.x - z.y*z.y) - 1;
   zz.y = 2. * z.x*z.y;
   zz = c_sqrt(zz);
   zz.y += 1.;
   zz = c_ln(c_div(zz,z));
   v = zz.x;
   zz.x = zz.y;
   zz.y = -v;
   return(zz);
   }

struct complex c_asec(struct complex z)
   {
   struct complex zz;
   double v;

   zz.x = 1. - (z.x*z.x - z.y*z.y);
   zz.y = -2. * z.x*z.y;
   zz = c_sqrt(zz);
   zz.x += 1.;
   zz = c_ln(c_div(zz,z));
   v = zz.x;
   zz.x = zz.y;
   zz.y = -v;
   return(zz);
   }

struct complex c_sinh(struct complex z)
   {
   double v1, v2;
   struct complex zz;

   zz.x =  ((v1 = exp(z.x)) - (v2 = exp(-z.x)))*cos(z.y)/2.;
   zz.y =  (v1 + v2)*sin(z.y)/2.;
   return(zz);
   }

struct complex c_cosh(struct complex z)
   {
   double v1, v2;
   struct complex zz;

   zz.x = ((v1 = exp(z.x)) + (v2 = exp(-z.x)))*cos(z.y)/2.;
   zz.y = (v1 - v2)*sin(z.y)/2.;
   return(zz);
   }

struct complex c_tanh(struct complex z)
   {
   return(c_div(c_sinh(z),c_cosh(z)));
   }

struct complex c_sech(struct complex z)
   {
   struct complex zz;

   zz.x = 1.;
   zz.y = 0.;
   return(c_div(zz,c_cosh(z)));
   }

struct complex c_csch(struct complex z)
   {
   struct complex zz;

   zz.x = 1.;
   zz.y = 0.;
   return(c_div(zz,c_sinh(z)));
   }

struct complex c_coth(struct complex z)
   {
   return(c_div(c_cosh(z),c_sinh(z)));
   }

struct complex c_asinh(struct complex z)
   {
   struct complex zz;

   zz.x = 1. - (z.x*z.x - z.y*z.y);
   zz.y = -2. * z.x*z.y;
   zz = c_sqrt(zz);
   zz.x += z.x;
   zz.y += z.y;
   zz = c_ln(zz);
   return(zz);
   }

struct complex c_acosh(struct complex z)
   {
   struct complex zz;

   zz.x = (z.x*z.x - z.y*z.y) - 1;
   zz.y = 2. * z.x*z.y;
   zz = c_sqrt(zz);
   zz.x += z.x;
   zz.y += z.y;
   zz = c_ln(zz);
   return(zz);
   }

struct complex c_atanh(struct complex z)
   {
   struct complex zz1, zz2, zz;

   zz1.x = 1. + z.x;
   zz1.y =  z.y;
   zz2.x = 1. - z.x;
   zz2.y =  z.y;
   zz = c_ln(c_div(zz1,zz2));
   zz.x /= 2.;
   zz.y /= 2.;
   return(zz);
   }

struct complex c_acsch(struct complex z)
   {
   struct complex zz;

   zz.x = (z.x*z.x - z.y*z.y) + 1;
   zz.y = 2. * z.x*z.y;
   zz = c_sqrt(zz);
   zz.x += 1.;
   zz = c_ln(c_div(zz,z));
   return(zz);
   }

struct complex c_asech(struct complex z)
   {
   struct complex zz;

   zz.x = 1. - (z.x*z.x - z.y*z.y);
   zz.y = -2. * z.x*z.y;
   zz = c_sqrt(zz);
   zz.x += 1.;
   zz = c_ln(c_div(zz,z));
   return(zz);
   }

struct complex c_acoth(struct complex z)
   {
   struct complex zz1, zz2, zz;

   zz1.x = z.x + 1.;
   zz1.y = z.y;
   zz2.x = z.x - 1.;
   zz2.y = z.y;
   zz = c_ln(c_div(zz1,zz2));
   zz.x /= 2.;
   zz.y /= 2.;
   return(zz);
   }

struct complex c_log10(struct complex z)
   {
   struct complex zz;
   
   zz = c_ln(z);
   zz.x /= LN10;
   zz.y /= LN10;
   return(zz);
   }

struct complex c_pow(struct complex z1, struct complex z2)
   {
   return(c_exp(c_mul(z2,c_ln(z1))));
   }


double asinh(double x)
   {
   return(log(x + sqrt((x*x) + 1.)));
   }

double acosh(double x)
   {
   return(log(x + sqrt((x*x) - 1.)));
   }

double atanh(double x)
   {
   return(log((1.+x)/(1.-x))/2.);
   }

struct complex cmplx(double r, double i)  /* R + iI */
   {
   struct complex z;
   z.x = r;
   z.y = i;
   return(z);
   }

/*********************************************************************
*
* Get a Message From the File TISAN.IDX and Print it 
* from column 44 to 80 by however-many lines
*
*/
short displayText(FILE *IndexTableStream)
   {
   short I;
   char C;
   extern const char szTask[];

   if (!IndexTableStream)
      {
      zMessage(0,"\n");
      return(0);
      }

   I=0;
   while (I<36)
      {
      C = fgetc(IndexTableStream);
      if (!C || (C == '\n')) break;
      zMessage(0,"%c",C);
      ++I;
      } /* End While */

   if (I < 36) zMessage(0,CrLf);

   return(C);
   }

/*********************************************************************
*
* Write a string to columns 22 to 41 and the message line after it
*
*/
short WriteString(char *StringPointer, FILE *IndexTableStream)
  {
  short M,N,I;
  short PassFlag=0;
  short C=1;

  do {
     if (!PassFlag)
        {
        N = zMessage(0,"%.19s",StringPointer);  // 19 characters to account for the leading single quote
        PassFlag = TRUE;
        M = N + 1;
        }
     else
        {
        M = N = zMessage(0,"%.20s",StringPointer);
        }

     if (M < 20)
        {
        ++M;
        PassFlag = -1;
        zMessage(0,"'");
        }

     for (I=M;I<21;++I) zMessage(0,Space);      // pad out to the description column

     if (C)
        C = displayText(IndexTableStream);
     else
        displayText(NULL);

     if ((M == 20) && (PassFlag > 0))
        {
        StringPointer += N;
        zTaskMessage(0,"%13s",Space);  // New time with more text so pad out to the display column
        }
     }
  while ((M == 20) && (PassFlag > 0));

  if (C) zMessage(0,"%34s",Space); // $$$

  return(C);
  }

/*********************************************************************
*
* Print an adverb and its value
*/
void displayInputs(unsigned char InputsAdverbToken, FILE *IndexTableStream)
   {
   short C=1;
   char BUFFER[80];
   extern const char szTask[];
   short oldQuiet;
   
   oldQuiet = QUIET;

   if (!strcmp(szTask, "TISAN")) QUIET = 0; // We really don't want to suppress the inputs request from the interactive command line

   switch (InputsAdverbToken)
      {
      case 1:
         zMessage(0,"%-8s: INNAME   ... '",szTask);
         C = WriteString(INNAME,IndexTableStream);
         break;
      case 2:
         zMessage(0,"%-8s: INCLASS  ... '",szTask);
         C = WriteString(INCLASS,IndexTableStream);
         break;
      case 3:
         zMessage(0,"%-8s: INPATH   ... '",szTask);
         C = WriteString(INPATH,IndexTableStream);
         break;
      case 4:
         zMessage(0,"%-8s: IN2NAME  ... '",szTask);
         C = WriteString(IN2NAME,IndexTableStream);
         break;
      case 5:
         zMessage(0,"%-8s: IN2CLASS ... '",szTask);
         C = WriteString(IN2CLASS,IndexTableStream);
         break;
      case 6:
         zMessage(0,"%-8s: IN2PATH  ... '",szTask);
         C = WriteString(IN2PATH,IndexTableStream);
         break;
      case 7:
         zMessage(0,"%-8s: IN3NAME  ... '",szTask);
         C = WriteString(IN3NAME,IndexTableStream);
         break;
      case 8:
         zMessage(0,"%-8s: IN3CLASS ... '",szTask);
         C = WriteString(IN3CLASS,IndexTableStream);
         break;
      case 9:
         zMessage(0,"%-8s: IN3PATH  ... '",szTask);
         C = WriteString(IN3PATH,IndexTableStream);
         break;
      case 10:
         zMessage(0,"%-8s: OUTNAME  ... '",szTask);
         C = WriteString(OUTNAME,IndexTableStream);
         break;
      case 11:
         zMessage(0,"%-8s: OUTCLASS ... '",szTask);
         C = WriteString(OUTCLASS,IndexTableStream);
         break;
      case 12:
         zMessage(0,"%-8s: OUTPATH  ... '",szTask);
         C = WriteString(OUTPATH,IndexTableStream);
         break;
      case 13:
         sprintf(BUFFER,"CODE     ... %d",CODE);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 14:
         sprintf(BUFFER,"ITYPE    ... %d",ITYPE);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 15:
         sprintf(BUFFER,"FACTOR   ... %lG",FACTOR);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 16:
         sprintf(BUFFER,"TRANGE   ... %lG, %lG",TRANGE[0],TRANGE[1]);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 17:
         sprintf(BUFFER,"YRANGE   ... %lG, %lG",YRANGE[0],YRANGE[1]);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 18:
         sprintf(BUFFER,"ZRANGE   ... %lG, %lG",ZRANGE[0],ZRANGE[1]);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 19:
         sprintf(BUFFER,"WINDOW   ... %d, %d,",WINDOW[0],WINDOW[1]);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         C = displayText(IndexTableStream);
         sprintf(BUFFER,"             %d, %d",WINDOW[2],WINDOW[3]);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         if (C) C = displayText(IndexTableStream); else zMessage(0,CrLf);
         break;
      case 20:
         sprintf(BUFFER,"TFORMAT  ... '%s'",TFORMAT);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 21:
         sprintf(BUFFER,"YFORMAT  ... '%s'",YFORMAT);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 22:
         sprintf(BUFFER,"ZFORMAT  ... '%s'",ZFORMAT);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 23:
         sprintf(BUFFER,"DEVICE   ... '%s'",DEVICE);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 24:
         sprintf(BUFFER,"BAUD     ... %d",BAUD);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 25:
         sprintf(BUFFER,"STOPBITS ... %d",STOPBITS);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 26:
         sprintf(BUFFER,"PROGRESS ... %d",PROGRESS);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 27:
         sprintf(BUFFER,"PARITY   ... '%s'",PARITY);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 28:
         sprintf(BUFFER,"FRAME    ... %d",FRAME);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 29:
         sprintf(BUFFER,"BORDER   ... %d",BORDER);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 30:
         sprintf(BUFFER,"COLOR    ... 0x%08X",COLOR); // Display COLOR in HEX 0x00RRGGBB
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 31:
         sprintf(BUFFER,"TMINOR   ... %d",TMINOR);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 32:
         sprintf(BUFFER,"YMINOR   ... %d",YMINOR);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 33:
         sprintf(BUFFER,"ZMINOR   ... %d",ZMINOR);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 34:
         sprintf(BUFFER,"TMAJOR   ... %lG, %lG",TMAJOR[0],TMAJOR[1]);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 35:
         sprintf(BUFFER,"YMAJOR   ... %lG, %lG",YMAJOR[0],YMAJOR[1]);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 36:
         sprintf(BUFFER,"ZMAJOR   ... %lG, %lG",ZMAJOR[0],ZMAJOR[1]);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 37:
         zMessage(0,"%-8s: TLABEL   ... '",szTask);
         C = WriteString(TLABEL,IndexTableStream);
         break;
      case 38:
         zMessage(0,"%-8s: YLABEL   ... '",szTask);
         C = WriteString(YLABEL,IndexTableStream);
         break;
      case 39:
         zMessage(0,"%-8s: ZLABEL   ... '",szTask);
         C = WriteString(ZLABEL,IndexTableStream);
         break;
      case 40:
         zMessage(0,"%-8s: TITLE    ... '",szTask);
         C = WriteString(TITLE,IndexTableStream);
         break;
      case 41:
         sprintf(BUFFER,"PARMS    ... %lG, %lG,",PARMS[0],PARMS[1]);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         C = displayText(IndexTableStream);
         sprintf(BUFFER,"             %lG, %lG,",PARMS[2],PARMS[3]);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         if (C) C = displayText(IndexTableStream); else zMessage(0,CrLf);
         sprintf(BUFFER,"             %lG, %lG,",PARMS[4],PARMS[5]);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         if (C) C = displayText(IndexTableStream); else zMessage(0,CrLf);
         sprintf(BUFFER,"             %lG, %lG,",PARMS[6],PARMS[7]);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         if (C) C = displayText(IndexTableStream); else zMessage(0,CrLf);
         sprintf(BUFFER,"             %lG, %lG",PARMS[8],PARMS[9]);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         if (C) C = displayText(IndexTableStream); else zMessage(0,CrLf);
         break;
      case 42:
         sprintf(BUFFER,"QUIET    ... %d",QUIET);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 43:
         sprintf(BUFFER,"POINT    ... %lG, %lG",POINT[0],POINT[1]);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
      case 44:
         sprintf(BUFFER,"ZFACTOR  ... %lG, %lG",ZFACTOR[0],ZFACTOR[1]);
         zMessage(0,"%-8s: %-34s",szTask,BUFFER);
         break;
       default:
          printf("ERROR   :Error should not occur in function DisplayInputs!\n");
      }

   while (C)
      {
      if ((C = displayText(IndexTableStream)))
         zMessage(0,"%-8s: %34s",szTask,Space);
      }

   QUIET = oldQuiet;

   return;
   }

/*********************************************************************
* Pseudo Verb Inputs
*
* Display Adverbs on Screen for a Given task name (cPointer)
*
* Format for TISAN.IDX is
* TASKNAME1234TASKNAME1234....NULL0000TextTtextTtextTtext<NULL>
*
*/
BOOL displayTaskInputs(char *cPointer)
   {
   char C;
   FILE *IndexTableStream;
   BOOL NoMatch;
   LONG longP;
   unsigned char InputsAdverbToken;
   char path[_MAX_PATH], fname[_MAX_FNAME];
   extern const char szTask[];

   if (isNullString(cPointer) || isEmptyString(cPointer)) cPointer = TASKNAME;
   
   strupr(cPointer); // task names must be uppercase 

   makePath(path,TisanDrive,TisanDir,"TISAN",".IDX");

   IndexTableStream = fopen(path,"rb");
   if (!IndexTableStream)
      {
      zMessage(0,"%-8s: Error Opening Index File for Read.\n",szTask);
      return(-1);
      }
/*
** Locate the Current Task in the index file
**
*/
   do {
      fread(fname,1,8,IndexTableStream);
      fread(&longP,sizeof(LONG),1,IndexTableStream);
      fname[8] = '\0';
      if (ferror(IndexTableStream))
         {
         zMessage(0,"%-8s: Error Reading TISAN.IDX\n",szTask);
         return(TRUE);
         }
      if (!longP)
         {
         zMessage(0,"%-8s: Task %s Not Found in TISAN.IDX\n",szTask,cPointer);
         return(TRUE);
         }
      NoMatch = strcmp(fname,cPointer);  /* No match ?*/
      }
   while (NoMatch);

   fseek(IndexTableStream,longP,SEEK_SET);
     
   zMessage(0,"%-8s:\n",szTask);
   zMessage(0,"%-8s: %s - ",szTask,cPointer);
   do {
      if ((C = fgetc(IndexTableStream)))
         zMessage(0,"%c",C);
      else
         zMessage(0,CrLf);
      }
   while (C);

   zMessage(0,"%-8s: ---------------------------------------------------------------------\n",szTask);

   do {
      if (!(InputsAdverbToken = fgetc(IndexTableStream))) break;
      displayInputs(InputsAdverbToken,IndexTableStream);
      }
   while (!INTERRUPT);

   zMessage(0,"%-8s:\n",szTask);
   fclose(IndexTableStream);

   return(FALSE);
   }

/*********************************************************************
*
* ZCatFiles will count the number of files matching szPath and
* allocate memory to hold the list of null terminated file names.
* If memory has been allocated by a previous call, then it is
* first deallocated.  If szPath is NULL then only the memory
* deallocation is performed if needed.
* It is important that floating point errors be trapped if any
* memory has been allocated.
**
** The CatList structure is static, so it will be persistent after the
** function returns.
*/
struct CATSTRUCT *ZCatFiles(char* pPath)
   {
   static struct CATSTRUCT CatList;
   struct CATSTRUCT *pCatList = (struct CATSTRUCT *)NIL;
   extern const char szTask[];
 
   if (CatList.pList) free(CatList.pList);

   CatList.N = 0;
   CatList.pList = (char*)NIL;

   if (pPath)
      {
      if (strchr(pPath,'*') || strchr(pPath,'?') || !strcmp(szTask,"TISAN"))   // Wild cards OR being called from the CLI
         {
         fileDirectory(pPath, &CatList);
         }
      else  // No wild cards so not a regular expression and being called from a task, so just return the filename and let the task figure out if it's there
         {
         CatList.N = 1;
         CatList.pList = malloc(strlen(pPath) + 2); // Space for two NULs

         if (CatList.pList)
            {
            strcpy(CatList.pList, pPath);
            *(CatList.pList + strlen(pPath) + 2) = NUL; // Add the second NUL to mark the end of the list
            }
         else
            {
            zTaskMessage(10,"Memory Allocation Failure in function ZCatFiles\n");
            return((struct CATSTRUCT *)NIL);
            }
         }

      if (CatList.N == 0)
         zTaskMessage(10,"No Files Matching '%s'\n",pPath);
      else
         pCatList = &CatList;
      }

   return(pCatList);
   }

/*********************************************************************
*
*  Make a file name and return a pointer
*  Create one of 4 types of file names
*
* M_inname  : Primary input file
* M_in2name : Secondary input file
* M_outname : Output file
* M_tmpname : Temporary scratch file
*
* The Primary file name is built from INNAME, INCLASS and INPATH directly.
* The Secondary file name is built from IN2NAME, IN2CLASS and IN2PATH if
* they are not null strings, if any one is not defined, then INNAME,
* INCLASS or INPATH is used.
* The Output file name is built from OUTNAME, OUTCLASS and OUTPATH if
* they are not null strings, if any one is not defined, then INNAME,
* INCLASS or INPATH is used.
* The Temporary file name is built from OUTPATH, an 8 character name based
* on the current process ID, the TASKNAME and the number of retries in
* building the name.  The extension .TMP is always used.
*
* If the file name can be created, a pointer is returned, otherwise
* an error message is printed and the TASK dies.
*/
PSTR zBuildFileName(short TYPE, PSTR cPointer)
   {
   char drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
   char *cPathPointer, *cFnamePointer, *cExtPointer;
   char PID[64];

   switch (TYPE)
      {
      case M_inname:        /* Create an INfile name */
         splitPath(INPATH,drive,dir,fname,ext);
         strcat(dir,fname);
         makePath(cPointer,drive,dir,INNAME,INCLASS);
         break;
      case M_in2name:        /* Create an IN2file name */
         if (*IN2PATH)
            cPathPointer = IN2PATH;
         else
            cPathPointer = INPATH;

         splitPath(cPathPointer,drive,dir,fname,ext);
         strcat(dir,fname);

         if (*IN2NAME)
            cFnamePointer = IN2NAME;
         else
            cFnamePointer = INNAME;

         if (*IN2CLASS)
            cExtPointer = IN2CLASS;
         else
            cExtPointer = INCLASS;

         makePath(cPointer,drive,dir,cFnamePointer,cExtPointer);
         break;
      case M_outname:        /* Create an OUTfile name */
         if (*OUTPATH)
            cPathPointer = OUTPATH;
         else
            cPathPointer = INPATH;

         splitPath(cPathPointer,drive,dir,fname,ext);
         strcat(dir,fname);

         if (*OUTNAME)
            cFnamePointer = OUTNAME;
         else
            cFnamePointer = INNAME;

         if (*OUTCLASS)
            cExtPointer = OUTCLASS;
         else
            cExtPointer = INCLASS;

         makePath(cPointer,drive,dir,cFnamePointer,cExtPointer);
         break;
      case M_tmpname:        /* Create a TEMPfile name */
         if (*OUTPATH)
            cPathPointer = OUTPATH;
         else
            cPathPointer = INPATH;

         splitPath(cPathPointer,drive,dir,fname,ext);
         strcat(dir,fname);

         strcpy(fname,"DBA");
         switch (strlen(TASKNAME))  /* Grab part of TASKNAME */
            {
            case 1:
               fname[0] = fname[1] = TASKNAME[0];
               break;
            case 2:
               fname[0] = TASKNAME[0];
               fname[1] = TASKNAME[1];
            case 3:
               fname[0] = TASKNAME[1];
               fname[1] = TASKNAME[2];
               break;
            default:
               fname[0] = TASKNAME[2];
               fname[1] = TASKNAME[3];
            }

         strcat(fname,itoa(getpid(),PID,10));

         do {
            makePath(cPointer,drive,dir,fname,".TMP");
            if (!access(cPointer,0))
               {
               ++fname[2];            // Run up the ASCII table until we hit a left square bracket
               if (fname[2] == '[')
                  {
                  zTaskMessage(255,"Unable to Create Scratch File!\n");
                  exit(1);   /* Kill Task */
                  }
               }
            }
         while (!access(cPointer,0));
         break;
         }
     return(cPointer);
     }

/*********************************************************************
*
*  Print a message based on its priority level and QUIET.
*
*/
short zMessage(short level, const char *format, ...)
   {
   short N = 0;
   va_list arg_ptr;

   va_start(arg_ptr, format);

   level = abs(level);

   if ((QUIET<0) || (level >= (abs(QUIET)-1)))
      {
      N = vprintf(format, arg_ptr);
      }

   va_end(arg_ptr);
   
   return(N);
   }

short zTaskMessage(short level, const char *format, ...)
   {
   short N = 0;
   va_list arg_ptr;
   extern const char szTask[];

   va_start(arg_ptr, format);

   level = abs(level);

   if ((QUIET<0) || (level >= (abs(QUIET)-1)))
      {
      printf("%-8s: ",szTask);
      N = vprintf(format, arg_ptr);
      }

   va_end(arg_ptr);
   
   return(N);
   }



/********************************************************************
*
* Initialize ADVERBS
*
*/
void initializeAdverbArrays()
   {
   ADVSTR[0]  = "TASKNAME";
   ADVSTR[1]  = "INNAME";
   ADVSTR[2]  = "INCLASS";
   ADVSTR[3]  = "INPATH";
   ADVSTR[4]  = "IN2NAME";
   ADVSTR[5]  = "IN2CLASS";
   ADVSTR[6]  = "IN2PATH";
   ADVSTR[7]  = "IN3NAME";
   ADVSTR[8]  = "IN3CLASS";
   ADVSTR[9]  = "IN3PATH";
   ADVSTR[10] = "OUTNAME";
   ADVSTR[11] = "OUTCLASS";
   ADVSTR[12] = "OUTPATH";
   ADVSTR[13] = "CODE";
   ADVSTR[14] = "ITYPE";
   ADVSTR[15] = "FACTOR";
   ADVSTR[16] = "TRANGE";
   ADVSTR[17] = "YRANGE";
   ADVSTR[18] = "ZRANGE";
   ADVSTR[19] = "WINDOW";
   ADVSTR[20] = "TFORMAT";
   ADVSTR[21] = "YFORMAT";
   ADVSTR[22] = "ZFORMAT";
   ADVSTR[23] = "DEVICE";
   ADVSTR[24] = "BAUD";
   ADVSTR[25] = "STOPBITS";
   ADVSTR[26] = "PROGRESS"; // was DATABITS
   ADVSTR[27] = "PARITY";
   ADVSTR[28] = "FRAME";
   ADVSTR[29] = "BORDER";
   ADVSTR[30] = "COLOR";
   ADVSTR[31] = "TMINOR";
   ADVSTR[32] = "YMINOR";
   ADVSTR[33] = "ZMINOR";
   ADVSTR[34] = "TMAJOR";
   ADVSTR[35] = "YMAJOR";
   ADVSTR[36] = "ZMAJOR";
   ADVSTR[37] = "TLABEL";
   ADVSTR[38] = "YLABEL";
   ADVSTR[39] = "ZLABEL";
   ADVSTR[40] = "TITLE";
   ADVSTR[41] = "PARMS";
   ADVSTR[42] = "QUIET";
   ADVSTR[43] = "POINT";
   ADVSTR[44] = "ZFACTOR";

   ADVPNTR[0]  = TASKNAME;          ADVSIZE[0]  = sizeof(TASKNAME);
   ADVPNTR[1]  = INNAME;            ADVSIZE[1]  = sizeof(INNAME);
   ADVPNTR[2]  = INCLASS;           ADVSIZE[2]  = sizeof(INCLASS);
   ADVPNTR[3]  = INPATH;            ADVSIZE[3]  = sizeof(INPATH);
   ADVPNTR[4]  = IN2NAME;           ADVSIZE[4]  = sizeof(IN2NAME);
   ADVPNTR[5]  = IN2CLASS;          ADVSIZE[5]  = sizeof(IN2CLASS);
   ADVPNTR[6]  = IN2PATH;           ADVSIZE[6]  = sizeof(IN2PATH);
   ADVPNTR[7]  = IN3NAME;           ADVSIZE[7]  = sizeof(IN3NAME);
   ADVPNTR[8]  = IN3CLASS;          ADVSIZE[8]  = sizeof(IN3CLASS);
   ADVPNTR[9]  = IN3PATH;           ADVSIZE[9]  = sizeof(IN3PATH);
   ADVPNTR[10] = OUTNAME;           ADVSIZE[10] = sizeof(OUTNAME);
   ADVPNTR[11] = OUTCLASS;          ADVSIZE[11] = sizeof(OUTCLASS);
   ADVPNTR[12] = OUTPATH;           ADVSIZE[12] = sizeof(OUTPATH);
   ADVPNTR[13] = (char *)&CODE;     ADVSIZE[13] = 1;
   ADVPNTR[14] = (char *)&ITYPE;    ADVSIZE[14] = 1;
   ADVPNTR[15] = (char *)&FACTOR;   ADVSIZE[15] = 1;
   ADVPNTR[16] = (char *)TRANGE;    ADVSIZE[16] = 2;
   ADVPNTR[17] = (char *)YRANGE;    ADVSIZE[17] = 2;
   ADVPNTR[18] = (char *)ZRANGE;    ADVSIZE[18] = 2;
   ADVPNTR[19] = (char *)WINDOW;    ADVSIZE[19] = 4;
   ADVPNTR[20] = TFORMAT;           ADVSIZE[20] = sizeof(TFORMAT);
   ADVPNTR[21] = YFORMAT;           ADVSIZE[21] = sizeof(YFORMAT);
   ADVPNTR[22] = ZFORMAT;           ADVSIZE[22] = sizeof(ZFORMAT);
   ADVPNTR[23] = DEVICE;            ADVSIZE[23] = sizeof(DEVICE);
   ADVPNTR[24] = (char *)&BAUD;     ADVSIZE[24] = 1;
   ADVPNTR[25] = (char *)&STOPBITS; ADVSIZE[25] = 1;
   ADVPNTR[26] = (char *)&PROGRESS; ADVSIZE[26] = 1;
   ADVPNTR[27] = PARITY;            ADVSIZE[27] = sizeof(PARITY);
   ADVPNTR[28] = (char *)&FRAME;    ADVSIZE[28] = 1;
   ADVPNTR[29] = (char *)&BORDER;   ADVSIZE[29] = 1;
   ADVPNTR[30] = (char *)&COLOR;    ADVSIZE[30] = 1;
   ADVPNTR[31] = (char *)&TMINOR;   ADVSIZE[31] = 1;
   ADVPNTR[32] = (char *)&YMINOR;   ADVSIZE[32] = 1;
   ADVPNTR[33] = (char *)&ZMINOR;   ADVSIZE[33] = 1;
   ADVPNTR[34] = (char *)TMAJOR;    ADVSIZE[34] = 2;
   ADVPNTR[35] = (char *)YMAJOR;    ADVSIZE[35] = 2;
   ADVPNTR[36] = (char *)ZMAJOR;    ADVSIZE[36] = 2;
   ADVPNTR[37] = TLABEL;            ADVSIZE[37] = sizeof(TLABEL);
   ADVPNTR[38] = YLABEL;            ADVSIZE[38] = sizeof(YLABEL);
   ADVPNTR[39] = ZLABEL;            ADVSIZE[39] = sizeof(ZLABEL);
   ADVPNTR[40] = TITLE;             ADVSIZE[40] = sizeof(TITLE);
   ADVPNTR[41] = (char *)PARMS;     ADVSIZE[41] = 10;
   ADVPNTR[42] = (char *)&QUIET;    ADVSIZE[42] = 1;
   ADVPNTR[43] = (char *)POINT;     ADVSIZE[43] = 2;
   ADVPNTR[44] = (char *)ZFACTOR;   ADVSIZE[44] = 2;

   return;
   }
