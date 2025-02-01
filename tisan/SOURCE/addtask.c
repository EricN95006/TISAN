/*
**
** Dr. Eric R. Nelson
** 12/28/2016
**
** Updated 12/2019 to account for changes in the library files that use szTask
**
** Note that addtask expects to be run from the tisan directory and is passed the file to process as ./SUPPORT/TASKNAME.TXT on the command line,
** that way the TISAN.IDX file is created in the tisan folder where it is needed.
**
** The program reads in the contents of the existing TISAN.IDX file (if it exists) and then adds the new index information to it.
**
** You will need to "make index" if addtask is recompiled.
**
** Updated default inputs 1/6/2021
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

#define Number_of_Adverbs ADVCOUNT
/*
** gcc addtask.c -o addtask dos.o tisanlib.o
**
** Program to add a task in the TISAN.IDX file.
** 
** The INDEX file is in the format
**
** TASKNAME1234TASKNAME1234NULL0TextTTextTTextTText0
**
** The text file TASK.TXT must be in the following format:
**
Taskname: Description Line for the Task
ADVERB\
ADVERB:
Text Description Lines\
*/
void WriteUserMessage(BYTE);
void WriteAdverbInfo(BYTE);
int  TokenizeAdverb(void);

FILE *IndexStream, *TextStream, *OutStream;
char FNAME[_MAX_PATH], ADVERB[16];
char *ADVSTR[Number_of_Adverbs];
char *MessagePointer[Number_of_Adverbs];
LONG longP, CurPos;
int I;

const char szTask[] = "ADDTASK"; // This name is used by all the error handlers to identify the module throwing the error.

int main(int argc, char *argv[])
   {
   BYTE C;

   initializeAdverbArrays();

   MessagePointer[0]  = "";
   MessagePointer[1]  = "Input File Name";
   MessagePointer[2]  = "Input File Class";
   MessagePointer[3]  = "Input File Path";
   MessagePointer[4]  = "Secondary File Name  (INNAME)";
   MessagePointer[5]  = "Secondary File Class (INCLASS)";
   MessagePointer[6]  = "Secondary File Path  (INPATH)";
   MessagePointer[7]  = "Tertiary File Name";
   MessagePointer[8]  = "Tertiary File Class";
   MessagePointer[9]  = "Tertiary File Path";
   MessagePointer[10]  = "Output Name  (INNAME)";
   MessagePointer[11]  = "Output Class (INCLASS)";
   MessagePointer[12]  = "Output Path  (INPATH)";
   MessagePointer[13]  = "Execution Code";
   MessagePointer[14]  = "Type Code";
   MessagePointer[15]  = "Real Factor Value";
   MessagePointer[16]  = "Time Range";
   MessagePointer[17]  = "Amplitude Range";
   MessagePointer[18]  = "Z-Axis Range";
   MessagePointer[19]  = "Plotting Window";
   MessagePointer[20]  = "T Text Format";
   MessagePointer[21]  = "Y Text Format";
   MessagePointer[22]  = "Z Text Format";
   MessagePointer[23]  = "I/O Device";
   MessagePointer[24]  = "Baud Rate";
   MessagePointer[25]  = "Stop Bits";
   MessagePointer[26]  = "";                           // PROGRESS (was DATABITS)
   MessagePointer[27]  = "Parity";
   MessagePointer[28]  = "Draw Graphics Frame";
   MessagePointer[29]  = "Draw Graphics Border";
   MessagePointer[30]  = "Color";
   MessagePointer[31]  = "T-Axis Minor Tic-Marks";
   MessagePointer[32]  = "Y-Axis Minor Tic-Marks";
   MessagePointer[33]  = "Z-Axis Minor Tic-Marks";
   MessagePointer[34]  = "T-Axis Major Tic-Marks";
   MessagePointer[35]  = "Y-Axis Major Tic-Marks";
   MessagePointer[36]  = "Z-Axis Major Tic-Marks";
   MessagePointer[37]  = "Time Text Label";
   MessagePointer[38]  = "Y Text Label";
   MessagePointer[39]  = "Z Text Label";
   MessagePointer[40]  = "Title Text Label";
   MessagePointer[41]  = "Control Parameters";
   MessagePointer[42]  = "";				// QUIET
   MessagePointer[43]  = "X,Y Data Pair";
   MessagePointer[44]  = "Complex Factor Value";

   memset(TASKNAME,'\0',sizeof(TASKNAME));
   printf("\n'%s'\n",argv[1]);

   TextStream = fopen(strcat(strcpy(FNAME,argv[1]),".TXT"),"rt");

   if (TextStream == (FILE*)NIL)
      {
      printf("Can't Open '%s':",FNAME);
      perror(NULL);
      abort();
      }

   do {
      C = fgetc(TextStream);
      }
   while (isspace(C));

   I = 0;

   do {
      if (!isspace(C))
         {
         TASKNAME[I] = C;
         ++I;
         }
      C = fgetc(TextStream);
      }
   while ((C != ':') && (!feof(TextStream)));

   if (feof(TextStream))
      {
      printf("Missing Colon in Header Line\n");
      abort();
      }

   if (!(OutStream = tmpfile()))
      {
      perror ("Can't Open Scratch File\n");
      abort();
      }

   IndexStream = fopen("TISAN.IDX","rb");
   if (IndexStream != (FILE*)NIL)
      {
      do {
         fread(FNAME,1,8,IndexStream);
         fread((char *)&longP,sizeof(LONG),1,IndexStream);
         if (longP)
            {
            longP += 12;
            fwrite(FNAME,1,8,OutStream);
            fwrite((char *)&longP,sizeof(LONG),1,OutStream);
            }
         }
      while (longP);
      
      CurPos = ftell(IndexStream);
      fseek(IndexStream,0L,SEEK_END);
      longP = (LONG)ftell(IndexStream) + 12;
      fseek(IndexStream,CurPos,SEEK_SET);
      fwrite(TASKNAME,1,8,OutStream);
      fwrite((char *)&longP,sizeof(LONG),1,OutStream);
      memset(TASKNAME,'\0',sizeof(TASKNAME));
      fwrite(TASKNAME,1,8,OutStream);
      longP = 0;
      fwrite((char *)&longP,sizeof(LONG),1,OutStream);

      do {
         C = fgetc(IndexStream);
         if (!feof(IndexStream)) fputc(C,OutStream);
         }
      while (!feof(IndexStream));
      
      do {
         C = fgetc(TextStream);
         if (C == '\n') C = '\0';
         fputc(C,OutStream);
         }
      while (C);
      
      WriteAdverbInfo(C);
      }
   else
      {
      printf("Creating File TISAN.IDX\n");
      longP = 24;
      fwrite(TASKNAME,1,8,OutStream);
      printf("%8s: ",TASKNAME);
      fwrite((char *)&longP,sizeof(LONG),1,OutStream);
      memset(TASKNAME,'\0',9);
      fwrite(TASKNAME,1,8,OutStream);
      longP = 0;
      fwrite((char *)&longP,sizeof(LONG),1,OutStream);
      do {
         C = fgetc(TextStream);
         printf("%c",C);
         if (C == '\n') C = '\0';
         fputc(C,OutStream);
         }
      while (C);
      WriteAdverbInfo(C);
      }
   fclose(IndexStream);
   IndexStream = fopen("TISAN.IDX","wb");
   rewind(OutStream);
   while (!feof(OutStream))
      {
      C = fgetc(OutStream);
      fputc(C,IndexStream);
      }

   fclose(IndexStream);
   fclose(OutStream);

   exit(0);
   }

void WriteAdverbInfo(BYTE C)
   {
   BOOL DefaultFlag;
   int iIndex;

   while (!feof(TextStream))
      {
/*
** Remove any Leading Spaces
*/
      do {
         C = fgetc(TextStream);
         }
      while (isspace(C) && !feof(TextStream));

      if (feof(TextStream)) break;

      I = 0;
      do {
    if (C == '\n')
       {
       printf("Missing '\\' or ':'\n");
       abort();
       }
         if (!isspace(C))
            {
            printf("%c",C);
            switch (C)
               {
               case '\\':
                  DefaultFlag = TRUE;
                  C = '\0';
                  break;
               case ':':
                  DefaultFlag = FALSE;
                  C = '\0';
                  break;
               }
            ADVERB[I] = C;
            ++I;
       if (C)
          C = fgetc(TextStream);
            }
         }
      while (C && !feof(TextStream));

      if (feof(TextStream)) break;

      iIndex = TokenizeAdverb();
      fputc(iIndex,OutStream);
      if (DefaultFlag)
         {
         fwrite(MessagePointer[iIndex],
                strlen(MessagePointer[iIndex])+1,1,OutStream);
         printf("%s\n",MessagePointer[iIndex]);
         }
      else
         WriteUserMessage((BYTE)iIndex);
      }
   fputc('\0',OutStream);
   return;
   }

int TokenizeAdverb()
   {
   int I;

   strupr(ADVERB);
   for (I=0;I<Number_of_Adverbs;++I)
      {
      if (!strcmp(ADVERB,ADVSTR[I]))
         return(I);
      }
   printf("Adverb '%s' Unknown.\n",ADVERB);
   abort();
   }

void WriteUserMessage(BYTE C)
   {
/*
** Remove any Leading Spaces
*/
   do {
      C = fgetc(TextStream);
      }
   while (isspace(C));
   printf("%c",C);
   if (C == '\\') C = '\0';
   fputc(C,OutStream);
   while (C && !feof(TextStream))
      {
      C = fgetc(TextStream);
      printf("%c",C);
      if (C == '\\') C = '\0';
      fputc(C,OutStream);
      }
   printf("\n");
   return;
   }
