/*********************************************************************
**
** Dr. Eric R. Nelson
** 11/30/2017
*
*  Task TAFFY
*
* This code processes data in a single monolithic memory block.
* All data are first loaded into allocated memory.
* Working from memory is faster than processing one record at a time from disk, but it requires
* that everything fit.
*
* The default outclass is tfy
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

void processData(void);
void processCodeZero(short type, double timeVal, double *Rval, struct complex *Zval);
double processRealData(double timeVal, double Rval);
struct complex processComplexData(double timeVal, struct complex Zval);
void readAllDataRecords(void);
void writeAllDataRecords(void);
void allocateMemoryBuffer(void);

char infileName[_MAX_PATH], outfileName[_MAX_PATH], tempfileName[_MAX_PATH];

struct FILEHDR FileHeader;
struct RData  *RDataPntr;
struct TRData *TRDataPntr;
struct XData  *XDataPntr;
struct TXData *TXDataPntr;
struct XData  XDataOut;

FILE *infileStream  = (FILE *)NIL;
FILE *outfileStream = (FILE *)NIL;

const char szTask[]="TAFFY";

char *dataBuffer = (char *)NIL;
long numDataRecords = 0L, numberOfDataRecordsRead;


int main(int argc, char *argv[])
   {
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;

   signal(SIGINT,BREAKREQ);  /* Handle ^C Interrupt */
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1); /* Initialize Task */

   if ((CODE<0) || (CODE>1))
      {
      zTaskMessage(10,"Invalid Code Specification\n");
      Zexit(1);
      }

   if (!OUTCLASS[0]) strcpy(OUTCLASS,"tfy");

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
      infileStream = zOpen(infileName,O_readb);
      if (!infileStream) Zexit(1);
      
      if (!Zgethead(infileStream,&FileHeader)) BombOff(1);

      switch (FileHeader.type)
         {
         case X_Data:
         case TX_Data:
         case R_Data:
         case TR_Data:
            break;
         default:
            zTaskMessage(10,"Unknown File Type.\n");
            BombOff(1);
         }

      zTaskMessage(2,"Opening Scratch File '%s'\n",tempfileName);
      if ((outfileStream = zOpen(tempfileName,O_writeb)) == NULL) BombOff(1);

      allocateMemoryBuffer();
      readAllDataRecords();
      processData();
      writeAllDataRecords();

      fcloseall();

      ERRFLAG = zNameOutputFile(outfileName,tempfileName);
      } // for (iwc = 0, pWild = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }


/*
** Do the heavy lifting here....
*/
void processData()
   {
   long i;
   double Rval;
   struct complex Zval;
   short flag=0;
   double timeVal, timeIndex=0.;
   char *buffer;
   
   buffer = dataBuffer; // Local copy of the pointer so we can index it

   RDataPntr =  (struct RData *)dataBuffer;     // Different interpretations of the data in the buffer, in case we need them
   TRDataPntr = (struct TRData *)dataBuffer;
   XDataPntr =  (struct XData *)dataBuffer;
   TXDataPntr = (struct TXData *)dataBuffer;

   for (i = 0L; i < numberOfDataRecordsRead; ++i)
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
            } // switch (CODE)
         } // if (!flag)

      insertValues(buffer, &FileHeader, timeVal, Rval, Zval, flag);     // Replaces existing data in memory

      ++timeIndex;           // Must update after all the processing...

      buffer += Zsize(FileHeader.type);   // Move to the next record for this data type
      }

   return;
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

/*
** Read in all the data records to the allocated memory (we hope)
*/
void readAllDataRecords()
   {
   size_t recordsRead;

   if (!Zgethead(infileStream,&FileHeader)) BombOff(1); // Rewind the file back to the start of the data

   recordsRead = fread(dataBuffer, (size_t)Zsize(FileHeader.type), (size_t)numDataRecords, infileStream);

   if (recordsRead != (size_t)numDataRecords)
      {
      zTaskMessage(10,"Error reading input file. Expected %ld records but only read %ld\n", numDataRecords, recordsRead);
      BombOff(1);
      }
      
   return;   
   }

/*
** Write out all the data records to the allocated memory (we hope)
*/
void writeAllDataRecords()
   {
   size_t recordsWritten;

   if (Zputhead(outfileStream,&FileHeader)) BombOff(1);  // Output file is the same type as the input file

   recordsWritten = fwrite(dataBuffer, (size_t)Zsize(FileHeader.type), (size_t)numDataRecords, outfileStream);

   if (recordsWritten != (size_t)numDataRecords)
      {
      zTaskMessage(10,"Error writing output file. Expected %ld records but only wrote %ld\n", numDataRecords, recordsWritten);
      BombOff(1);
      }

   return;   
   }

/*
**
** Allocate the memory block
** Memory is freed in fcloseall()
*/
void allocateMemoryBuffer()
   {
   numDataRecords = countDataRecords(infileStream);

   if (!numDataRecords)
      {
      zTaskMessage(10,"There appear to be zero records in this input file.\n");
      BombOff(1);
      }
   else
      {
      dataBuffer = (char *)malloc(numDataRecords * (long)Zsize(FileHeader.type));

      if (!numDataRecords)
         {
         zTaskMessage(10,"Unable to allocate memory for input file.\n");
         BombOff(1);
         }
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

   if (dataBuffer) free(dataBuffer);
   dataBuffer = (char *)NIL;
   
   return;
   }
