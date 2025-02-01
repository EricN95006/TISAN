/*
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*   Pseudoverb Functions for the TISAN Interpreter
*/
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <errno.h>

#include "tisan.h"

#define Pvstrnm 18

#define NEWLINE   ('\n')

struct DEVICES HARDWARE;

int decodeHelpText(PSTR inFileName, PSTR matchString);
int displayHelpText(PSTR inFileName, PSTR outFileName, BOOL bFindExactMatch);


short PUTADV(char *);
char *FNAME(short);

short Cls(void);

short PROCESS(void);
void BEEP(void);
void PVINIT(void);
short  PVERBCHK(char *);
short  PVERB(short);

short  SystemCall(PSTR pCommand, PSTR pArgs);

short  Setwin(void);
short  Go(void);
short  Put(void);
short  Get(void);
short  Rename(void);
short  Delete(void);
short  Help(void);
short  Copy(void);
short  Catalog(void);
short  Run(void);
short  System(void);
short  Ascii(void);
short  Gethead(void);
short  Puthead(void);
short  Rem(void);
short  Wait(void);
short  Setpoint(void);

short  PUTMSG(FILE *,short);
short  PUTSTR(char *, FILE *);
void PRINTADV(char,FILE *);
short  NEWPOS(char,short *,short *,short *,short,
              short,short *,short *,short *,short *,short*);
short Inputs(char *);
void Encode(char *,...);

extern const char *MSP[];
extern char *JPNTR;
extern BOOL InterruptFlag;
extern short MATCH;
extern char ILINE[256];
extern short ECHO;
extern BOOL bExactMatch;

char *PVERBSTR[Pvstrnm];

BOOL bStopRun;

/******************************************
** Arrays for the Full Path for the TISAN
** Program on startup so wew know where to
** find the INPUTS and RUN files.
*/
extern char TisanDrive[], TisanDir[];

/*
** Character strings used throughout the program
extern char *NullString;
extern char *Space;
extern char *CrLf;
*/

/*********************************************************************
*
* Initialize PSERDOVERBS
*
*/
void PVINIT()
   {
   PVERBSTR[0]  = "GO";
   PVERBSTR[1]  = "INPUTS";
   PVERBSTR[2]  = "DELETE";
   PVERBSTR[3]  = "RENAME";
   PVERBSTR[4]  = "CATALOG";
   PVERBSTR[5]  = "COPY";
   PVERBSTR[6]  = "GET";
   PVERBSTR[7]  = "PUT";
   PVERBSTR[8]  = "HELP";
   PVERBSTR[9]  = "RUN";
   PVERBSTR[10] = "SYSTEM";
   PVERBSTR[11] = "SETWIN";
   PVERBSTR[12] = "GETHEAD";
   PVERBSTR[13] = "PUTHEAD";
   PVERBSTR[14] = "REM";
   PVERBSTR[15] = "WAIT";
   PVERBSTR[16] = "SETPOINT";
   PVERBSTR[17] = "ASCII";
   return;
   }

/*********************************************************************
*
* Do the requested system call with arguments
*
*/
short SystemCall(PSTR pCommand, PSTR pArgs)
   {
   char *pSystemCommand;
   
   pSystemCommand = (char *)malloc(strlen(pCommand) + strlen(pArgs) + 2);

   if (pSystemCommand)
      {
      strcpy(pSystemCommand, pCommand);
      strcat(pSystemCommand, " ");
      strcat(pSystemCommand, pArgs);
      system(pSystemCommand);
      free(pSystemCommand);
      }
   else
      printf("%sMemory Allocaiton Failure in function SystemCall.\n",MSP[1]);
      
   return(0);
   }

/*********************************************************************
* Pseudo Verb Go
*
* Spawn a task from disk
*
*/
short Go()
   {
   short ERRFLAG=0;
   char path[_MAX_PATH];
   
   if (!*JPNTR) JPNTR = TASKNAME;

   *(JPNTR+8) = (char)0;

   strupr(JPNTR);

   if (PUTADV(JPNTR)) return(1);

   makePath(path,TisanDrive,TisanDir,JPNTR,(PSTR)NIL);

   ERRFLAG = system(path);

   if (ERRFLAG < 0)
      {
      printf("%s",MSP[1]);
      switch (errno)
         {
         case E2BIG:
            printf("%s",MSP[5]);
            break;
         case EINVAL:
            printf("%s",MSP[12]);
            break;
         case ENOENT:
            printf("Task '%s' Not Found\n",JPNTR);
            break;
         case ENOEXEC:
            printf("%s",MSP[14]);
            break;
         case ENOMEM:
            printf(MSP[10],"Task");
         }
      BEEP();
      }

   return(ERRFLAG);
   }

/*********************************************************************
* Pseudo Verb Put
*
* Put Adverbs to disk
*
*/
short Put()
   {
   if (!*JPNTR) JPNTR = TASKNAME;
   return(PUTADV(JPNTR));
   }

/*********************************************************************
* Pseudo Verb Get
*
* Get Adverbs from disk
*
*/
short Get()
   {
   char STRING[sizeof(TASKNAME)];
   short  ERRVAL = 0;

   strcpy(STRING,TASKNAME);
   if (!*JPNTR)
      {
      if (zGetAdverbs(TASKNAME)) ERRVAL=1;
      strcpy(TASKNAME,STRING);
      }
   else
      {
      if (zGetAdverbs(JPNTR)) ERRVAL=1;
      }

   return(ERRVAL);
   }

/*********************************************************************
* Pseudo Verb Rename
*
* Rename a disk file
*
*/
short Rename()
   {
   char *FROMFILE, *TOFILE;
   short ERRFLAG;

   FROMFILE = strtok(JPNTR," ,");
   TOFILE = strtok(NULL," ,");
   if ((!FROMFILE) || (!TOFILE))
      {
      printf("%s",MSP[11]);
      return(1);
      }
   ERRFLAG = rename(TOFILE,FROMFILE);
   if (ERRFLAG)
      {
      printf("%s",MSP[1]);
      switch (errno)
         {
         case EACCES:
            printf("File '%s' could not be created\n",TOFILE);
            break;
         case ENOENT:
            printf(MSP[9],FROMFILE);
            break;
         case EXDEV:
            printf("Unable to RENAME across devices\n");
         }
      }
   return(ERRFLAG);
   }

/*********************************************************************
* Pseudo Verb Delete
*
* Delete a disk file
* Always returns zero unless no argument is given
*
*/
short Delete()
   {
   if (!*JPNTR)
      {
      printf("%s",MSP[11]);
      return(1);
      }

  if (unlink(JPNTR))
     {
     printf("%s",MSP[1]);
     switch (errno)
        {
        case EACCES:
           printf("File '%s' is read only\n",JPNTR);
           break;
        case ENOENT:
           printf(MSP[9],JPNTR);
        }
     }
  return(0);
  }

/*********************************************************************
* Pseudo Verb Help
*
* Print some help
*
*/
short Help()
   {
   unsigned short LEN;
   char path[_MAX_PATH];

   LEN = strlen(JPNTR);
   if (!LEN)
      {
      JPNTR = "HELP";
      LEN = 4;
      }

   makePath(path,TisanDrive,TisanDir,"TISAN",".HLP");

   decodeHelpText(path, JPNTR);

   return(0);
   }

/*********************************************************************
* Pseudo Verb Catalog
*
* Display a disk directory
*
*/
short Catalog()
   {
   struct CATSTRUCT *pCatList;
   int i;
   char *pArg;
   char *pList;
   
   if (isNullString(JPNTR) || isEmptyString(JPNTR))
      pArg = "^.*";
   else
      pArg = JPNTR;
   
   pCatList = ZCatFiles(pArg);
   
   if (pCatList)
      {
      printf(CrLf);
      pList = pCatList->pList;
      for (i = 0; i < pCatList->N; ++i)
         {
         printf("%s\n",pList);
         pList = strchr(pList,NUL) + 1;
         }
      printf(CrLf);
      }

   ZCatFiles((char *)NIL); //  free any allocated memory

   return(0);
   }

/*********************************************************************
* Pseudo Verb Copy
*
* Copy a disk file
*
*/
short Copy()
   {
   return(SystemCall("cp", JPNTR));
   }

/*********************************************************************
* Pseudo Verb Run
*
* Execute a RUN file
*
*/
short Run()
   {
   FILE *FPNTR;
   long CURPOS;
   short I;
   char drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
   char path[_MAX_PATH];

   bStopRun = FALSE;

   splitPath(JPNTR,drive,dir,fname,ext);

   if (!*dir && !*drive)
      {
      strcpy(dir,TisanDir);
      strcat(dir,RUNDIREXT);
      strcpy(drive,TisanDrive);
      }

   makePath(path,drive,dir,fname,RUNDIREXT);

   if ((FPNTR = fopen(path,"rb")) == NULL)
      {
      printf(MSP[6],MSP[1],path);
      return(1);
      }

   while (!feof(FPNTR) && !InterruptFlag && !bStopRun)
      {
      memset(ILINE,0,256);
      I = 0;
      do ILINE[0] = fgetc(FPNTR);
      while ((!feof(FPNTR)) && (ILINE[0]<33));
      if (feof(FPNTR)) break;
      while ((!feof(FPNTR)) && (ILINE[I] > (char)31))
         ILINE[++I] = fgetc(FPNTR);
      ILINE[I] = 0;

      if (ECHO) printf("%s>%s\n",MSP[0],ILINE);

      if (ferror(FPNTR))
         {
         printf("%s%sFile '%s'\n",MSP[1],MSP[8],path);
         perror(MSP[3]);
         BEEP();
         bStopRun = TRUE;
         fclose(FPNTR);
         return(1);
         }

      CURPOS = ftell(FPNTR);
      I = PROCESS();
      fseek(FPNTR,CURPOS,0);

      if (bStopRun)
         {
         fclose(FPNTR);
         return(1);
         }

      if (I<0)
         break; /* End Command */
      else if (I>0)
         {
         fclose(FPNTR);
         return(1);
         }
      }
      
   fclose(FPNTR);
   memset(ILINE,(char)0,256);
   return(0);
   }

/*********************************************************************
* Pseudo Verb System
*
* Execute a System command
*
*/
short System()
   {
   return(SystemCall(JPNTR, ""));
   }

/*********************************************************************
* Pseudo Verb ASCII
*
* Print the ASCII value of the first chracter passed. Unicode-16 characgters will be negative
*
*/
short Ascii()
   {
   printf("%s%d\n",MSP[1],(int)(*JPNTR));
   return(0);
   }

/*********************************************************************
*
* Pseudo Verb SETWIN to set the plotting window
*
*/
short Setwin()
   {
   printf("%sSETWIN is not implemented.\n",MSP[1]);
   return(0);
   }

/*********************************************************************
*
* Update Cursor position
*
*/
short NEWPOS(char C, short *XLOC, short *YLOC, short *WHATSIDE, short SXMAX, short SYMAX, short *W0,
short *W1, short *W2, short *W3, short *BFLAG)
   {
   printf("%sFunction NEWPOS is not implemented.\n",MSP[1]);
   return(0);
   }

/*********************************************************************
* Pseudoverb Gethead
*
* Get header and put into TRANGE
*
*/
short Gethead()
   {
   FILE *INSTR;
   short ERRFLAG = 0;
   struct FILEHDR FileHeader;

   if (!*JPNTR) JPNTR = FNAME(0);                   // Default is INPATH/INNAME.INCLASS

   zTaskMessage(255,"Reading File '%s'\n",JPNTR);
   
   if ((INSTR = fopen(JPNTR,"rb")) == NULL)
      {
      printf(MSP[6],MSP[1],JPNTR);
      return(1);
      }

   fread(&FileHeader,sizeof(struct FILEHDR),1,INSTR);
   
   if (isTisanHeader(&FileHeader))
      {
      switch (FileHeader.type)
         {
         case R_Data:
            printf("%sREAL Time Series File\n",MSP[1]);
            break;
         case TR_Data:
            printf("%sREAL Time Stamped Data File\n",MSP[1]);
            break;
         case X_Data:
            printf("%sCOMPLEX Time Series File\n",MSP[1]);
            break;
         case TX_Data:
            printf("%sCOMPLEX Time Stamped Data File\n",MSP[1]);
            break;
         default:
            printf("%sUnknown File Type\n",MSP[1]);
            ERRFLAG=1;
         }

      if (ferror(INSTR))
         {
         perror(MSP[3]);
         ERRFLAG=1;
         }

      if (!ERRFLAG)
         {
         TRANGE[0] = FileHeader.m;
         TRANGE[1] = FileHeader.b;
         zTaskMessage(255,"TRANGE = %lG, %lG\n",TRANGE[0],TRANGE[1]);

         getHeaderText(&FileHeader);
         zTaskMessage(255,"TITLE = '%s'\n", TITLE);
         zTaskMessage(255,"TLABEL = '%s'\n", TLABEL);
         zTaskMessage(255,"YLABEL = '%s'\n", YLABEL);
         }
      }
   else
      {
      zTaskMessage(255,"Not a TISAN data file.\n");
      ERRFLAG = 1;
      }

   fclose(INSTR);

   return(ERRFLAG);
   }

/*********************************************************************
* Pseudoverb Puthead
*
* Put header from TRANGE
*
*/
short Puthead()
   {
   FILE *OUTSTR;
   short ERRFLAG = 0;
   struct FILEHDR FileHeader;

   if (!*JPNTR) JPNTR = FNAME(1);            // Default is OUTPATH/OUTNAME.OUTCLASS

   if ((OUTSTR = fopen(JPNTR,"r+b")) == NULL)
      {
      printf(MSP[6],MSP[1],JPNTR);
      return(1);
      }

   fread(&FileHeader,sizeof(struct FILEHDR),1,OUTSTR); /* Read in data type */

   switch (FileHeader.type)
      {
      case R_Data:
      case X_Data:
      case TR_Data:
      case TX_Data:
         break;
      default:
         zTaskMessage(255,"Unknown File Type\n");
         ERRFLAG=1;
      }

   if (!ERRFLAG)
      {
      rewind(OUTSTR);
      FileHeader.m = TRANGE[0];
      FileHeader.b = TRANGE[1];

      setHeaderText(&FileHeader);

      fwrite(&FileHeader,sizeof(struct FILEHDR),1,OUTSTR);

      if (ferror(OUTSTR))
         {
         perror(MSP[3]);
         ERRFLAG=1;
         }
      else
         {
         zTaskMessage(255,"Header values for file '%s' updated to:\n",JPNTR);
         zTaskMessage(255,"   m = %lG, b = %lG\n",TRANGE[0],TRANGE[1]);
         zTaskMessage(255,"   Title  = '%s'\n", TITLE);
         zTaskMessage(255,"   Tlabel = '%s'\n", TLABEL);
         zTaskMessage(255,"   Ylabel = '%s'\n", YLABEL);
         }
      }
      
   fclose(OUTSTR);

   return(ERRFLAG);
   }

/*********************************************************************
* Pseudo Verb Rem
*
* Remark statment to ignore everyting else on the line
*
*/
short Rem()
   {
   memset(ILINE,(char)0,sizeof(ILINE));
   return(0);
   }

/*********************************************************************
* Pseudo Verb Wait
*
* Print JLINE and wait for a key press
*
*/
short Wait()
   {
   BEEP();
   while (kbhit()) getch();       /* Clear Buffer of Pending Keys */
   printf("%s%s\n",MSP[1],JPNTR);
   printf("%sPress Any Key to Continue...\n",MSP[1]);
   getch();
   return(0);
   }

/*********************************************************************
*
* Pseudo Verb SETPOINT to set POINT[]
*
*/
short Setpoint()
   {
   printf("%sSETPOINT is not yet implemented.\n",MSP[1]);
   return(0);
   }

/*********************************************************************
*
* Search for a PSEUDOVERB
*
*/
short PVERBCHK(char *PNTR)
   {
   unsigned short I, LEN;
   short PVRB=0;

   if ((LEN = strlen(strupr(PNTR))) == 0) return(0);

   if (MATCH < 0)
      {
      for (I=0;I<Pvstrnm;++I)
         {
         if (!strncmp(PVERBSTR[I],PNTR,LEN))
            printf("%sCould be %-.8s\n",MSP[1],PVERBSTR[I]);
         }
      return(0);
      }

   if (bExactMatch) return(0);

   for (I=0;I<Pvstrnm;++I)
      {
      if (!strncmp(PVERBSTR[I],PNTR,LEN))
         {
         PVRB = I+1;
         ++MATCH;
         if (LEN == strlen(PVERBSTR[I])) /* Exact Match */
            {
            bExactMatch = TRUE;
            MATCH = 1;
            break;
            }
         }
      }

   return(PVRB);
   }

/*********************************************************************
*
* Process a PSEUDOVERB , JPNTR has the argument
*
*/
short PVERB(short N)
   {
   unsigned short LEN, I, J;
   short RVAL=0, F2=0;

   LEN = strlen(JPNTR); /* Find Number of Arguments 0,1,>1 */
   for (I=0;I<LEN;++I)
      if (isspace(*(JPNTR + I)) || (*(JPNTR+I) == ',')) {break;}

   while(isspace(*(JPNTR + I))) ++I;

   for (J=I;J<LEN;++J)
      if (isspace(*(JPNTR + J)) || (*(JPNTR+J) == ',')) {++F2; break;}

   while(isspace(*(JPNTR + J))) ++J;

   if ((N != 4) && (N != 6) && (N != 11) && (N != 15) && (N != 16))
      {
      if (F2)
         {
         printf("%sToo many arguments for PSEUDOVERB\n",MSP[1]);
         return(1);
         }
      }

   switch (N)
      {
      case 1:
         RVAL = Go();
         break;
      case 2:
         RVAL = Inputs(strupr(JPNTR));
         break;
      case 3:
         RVAL = Delete();
         break;
      case 4:
         RVAL = Rename();
         break;
      case 5:
         RVAL = Catalog();
         break;
      case 6:
         RVAL = Copy();
         break;
      case 7:
         strupr(JPNTR);
         RVAL = Get();
         break;
      case 8:
         strupr(JPNTR);
         RVAL = Put();
         break;
      case 9:
         strupr(JPNTR);
         RVAL = Help();
         break;
      case 10:
         RVAL = Run();
         break;
      case 11:
         RVAL = System();
         break;
      case 12:
         RVAL = Setwin();
         break;
      case 13:
         RVAL = Gethead();
         break;
      case 14:
         RVAL = Puthead();
         break;
      case 15:
         RVAL = Rem();
         break;
      case 16:
         RVAL = Wait();
         break;
      case 17:
         RVAL = Setpoint();
         break;
      case 18:
         RVAL = Ascii();
         break;
      }
   return(RVAL);
   }

/*********************************************************************
*
* Put Adverbs to file
*
*/
short PUTADV(char *NAMEPNTR)
   {
   return((short)zPutAdverbs(NAMEPNTR));
   }

/*********************************************************************
*
*  Make a file name and return a pointer
*
*/
char *FNAME(short TYPE)
   {
   static char path[_MAX_PATH];
   char drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];

   switch (TYPE)
      {
      case 0:        /* Create an INfile name */
         splitPath(INPATH,drive,dir,fname,ext);
         strcat(dir,fname);
         makePath(path,drive,dir,INNAME,INCLASS);
         break;
      case 1:        /* Create an OUTfile name */
         if (*OUTPATH)
            strcpy(path,OUTPATH);
         else
            strcpy(path,INPATH);

         splitPath(INPATH,drive,dir,fname,ext);
         strcat(dir,fname);

         if (*OUTNAME)
            strcpy(fname,OUTNAME);
         else
            strcpy(fname,INNAME);

         if (*OUTCLASS)
            strcpy(ext,OUTCLASS);
         else
            strcpy(ext,INCLASS);

         makePath(path,drive,dir,fname,ext);
         
         break;
      }
   return(path);
   }

void Encode(char *cFormatPntr,...)
   {
   va_list arg_ptr;

   va_start(arg_ptr,cFormatPntr);

   printf("TISAN   : ");
   vprintf(cFormatPntr,arg_ptr);

   va_end(arg_ptr);
   return;
   }


/*********************************************************************
*
* Get a Message From the File TISAN.IDX and Print it 
* from column 44 to 80 by however-many lines
*
*/
short DisplayText(FILE *IndexTableStream)
   {
   short I;
   char C;

   if (!IndexTableStream)
      {
      printf("\nTISAN   : ");
      return(0);
      }

   I=0;
   while (I<36)
      {
      C = fgetc(IndexTableStream);
      if (!C || (C == '\n')) break;
      printf("%c",C);
      ++I;
      } /* End While */
   if (I < 36) printf(CrLf);
   return(C);
   }

/*********************************************************************
* Pseudo Verb Inputs
*
* Display Adverbs on Screen for a Given TASKNAME
*
* Format for TISAN.IDX is
* TASKNAME1234TASKNAME1234....NULL0000TextTtextTtextTtext<NULL>
*
*/
short Inputs(char *cPointer)
   {
   return((short)displayTaskInputs(cPointer));
   }

/*
** Find help text matching the matchString
*/
int decodeHelpText(PSTR inFileName, PSTR matchString)
   {
   if (!displayHelpText(inFileName, matchString, TRUE))
      {
      displayHelpText(inFileName, matchString, FALSE);
      }
   return(0);
   }

int displayHelpText(PSTR inFileName, PSTR matchString, BOOL bFindExactMatch)
   {
   FILE *inputHelpFile;
   LONG offsetToNextEntry;
   BOOL allDone = FALSE;
   char keyWord[16];
   int keywordIndex;
   char C;
   int iCount = 0;
   PSTR pmatchStr;
   int iColumn;
   BOOL bMatch;
   
   if ((inputHelpFile = fopen(inFileName,"rb")) == NULL)
      {
      printf("%sError - Unable to open help file '%s'\n",MSP[1],inFileName);
      return(1);
      }

   pmatchStr = (PSTR)malloc(strlen(matchString) + 1);
   
   if (!pmatchStr)
      {
      printf("%sMemory allocation failure in decodeHelpText.\n",MSP[1]);
      fclose(inputHelpFile);
      return(1);
      }

   strcpy(pmatchStr,matchString);   // Need a copy of the string since we plan to upper case it and it could be a constant.

   strupr(pmatchStr);

   while (!allDone)
      {
      fread(&offsetToNextEntry, 4, 1, inputHelpFile); // 4 bytes, 1 entry

      if (offsetToNextEntry)
         {
         keywordIndex = 0;

         do {
            C = fgetc(inputHelpFile);     // Don't chewck for EOF here since it is an eror and is traped shortly
            keyWord[keywordIndex] = C;
            ++keywordIndex;
            if (keywordIndex == sizeof(keyWord)) --keywordIndex;  // Don't advance past the end of the buffer space.
            }
         while ((C != NUL) && !feof(inputHelpFile));

         keyWord[keywordIndex] = NUL;
         
         if (feof(inputHelpFile))
            {
            printf("%sUnexpected EOF reading keyword'%s'\n",MSP[1],keyWord);
            allDone = TRUE;
            }
         else  // Look for a match to the matchString
            {
            if (bFindExactMatch)
               {
               if (!strcmp(keyWord, pmatchStr))
                  bMatch = TRUE;
               else
                  bMatch = FALSE;
               }
            else
               {
               if (strstr(keyWord, pmatchStr))
                  bMatch = TRUE;
               else
                  bMatch = FALSE;
               }

            if (bMatch)
               {
               ++iCount;   // Count number of matches
               
               printf("\n%s: ", keyWord);
               iColumn = 0;               // Use to wrap words around column 80 to hep wiht readability.

               do {
                  C = fgetc(inputHelpFile);

                  if (feof(inputHelpFile))
                     allDone = TRUE;
                  else if (C != NUL)
                     {
                     if (C == NEWLINE)
                        iColumn = -1;
                     else if (iColumn > 70) // Time to do line wrapping at a word boundrh
                        {
                        if (isspace(C))
                           {
                           C = NEWLINE;
                           iColumn = -1;
                           }
                        }

                     printf("%c", C);

                     ++iColumn;
                     }
                  }
               while ((C != NUL) && !allDone);
                              }
            else // No match, jump to next entry
               fseek(inputHelpFile, offsetToNextEntry, SEEK_SET);
            }
         }
      else
         allDone = TRUE;
      }

   if ((!bFindExactMatch) && (iCount == 0))
      printf("%sSorry, no Help text matching '%s'\n", MSP[1],matchString);

   free(pmatchStr);
   
   return(iCount);
   }
