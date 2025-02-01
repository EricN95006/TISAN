/*
**
** Dr. Eric R. Nelson
** 12/28/2016
** usage
** To compile the help file: ./help
** To read the help file ./help keyword
**
** This program creates a help file which has usefull text for each task.
** The input file has a simple format
**
** `KEYWORD: short description
**    text body
** `
** `KEYWORD: short description
**    text body
** `
**
** The output file is a simple linked list of the form
** Offset from start of file to the next offset value (4 bytes) or zero for last entry
** keyword as a null terminated string (16 bytes max)
** Short one line description (null terminated)
** Text body (null terminated)
** 
** Next offset form the start of the file or zero for the last entry (4 bytes)
**
** To read the compiled help file do the following:
** Read in the 4 byte file offset to the next key word. If it is zero then we are at the end of the file and are all done (stop).
** Read in the NUL terminated string (16 bytes max)
** If the search key does not match the string, fseek to the offset and repeat this process
** Read in text and print it until a NUL is reached.
**
*/

#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "atcs.h"
#include "dos.h"

#define DELIMITER ('`') // Back tick is seldom used in text
#define COLON     (':')
#define NEWLINE   ('\n')
#define COLUMNLEN  70

int decodeHelpText(PSTR inFileName, PSTR matchString);
int displayHelpText(PSTR inFileName, PSTR outFileName, BOOL bFindExactMatch);
int encodeHelpText(PSTR inFileName, PSTR outFileName);

char *MSP[]={"","TISAN   : "};

char szTask[] = "HELP";

int decodeHelpText(PSTR inFileName, PSTR matchString)
   {
   if (!displayHelpText(inFileName, matchString, TRUE))
      {
      displayHelpText(inFileName, matchString, FALSE);
      }
   return(0);
   }

/*
** Find ALL help text matching the matchString
*/
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
      printf("%sMemory allocaiton failure in decodeHelpText.\n",MSP[1]);
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
            C = fgetc(inputHelpFile);     // Don't check for EOF here since it is an error and is trapped shortly
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
               iColumn = 0;               // Use to wrap words around column 80 to hep with readability.

               do {
                  C = fgetc(inputHelpFile);

                  if (feof(inputHelpFile))
                     allDone = TRUE;
                  else if (C != NUL)
                     {
                     if (C == NEWLINE)
                        iColumn = -1;
                     else if (iColumn > COLUMNLEN) // Time to do line wrapping at a word boundry
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

int encodeHelpText(PSTR inFileName, PSTR outFileName)
   {
   FILE *inputTextFile;
   FILE *outputHelpFile;
   LONG offsetToNextEntry, offsetWriteLocation;
   char C, keyWord[16];
   int keywordIndex;
   BOOL allDone = FALSE;

   if ((inputTextFile = fopen(inFileName,"rb")) == NULL)
      {
      printf("Error: Unable to open input file '%s'\n", inFileName);
      return(1);
      }

   if ((outputHelpFile = fopen(outFileName,"wb")) == NULL)
      {
      printf("Error: Unable to create output file '%s'\n", outFileName);
      fclose(inputTextFile);
      return(1);
      }

/*
** The logic...
**
** offsetWriteLocation is where the pointer to the next help block is to be written.
** it is initially zero.
** Write out 4 bytes as a place holder
**
** Find the DELIMITER. Read the task name up to the colon and make it a null terminator string
** Write that string to the HLP file.
** Read in the rest of the line until the \n and write each byte out to the HLP file.
** Write a terminating NULL.
** Read in the remaining text until we reach the terminating DELIMITER and write out the bytes sans DELIMITER.
** offsetToNextEntry is now the current file pointer.
** Seek to the offsetWriteLocation
** Write out offsetToNextEntry
** Seek to offsetToNextEntry
** Zero out offsetToNextEntry
**
** Repeat the process until we hit the EOF
**
*/
   offsetToNextEntry = 0;
   offsetWriteLocation = 0;

   do {
      fwrite(&offsetToNextEntry, 4, 1, outputHelpFile); // Place holder for the file seek pointer

      do {                             // Find the starting DELIMITER
         C = fgetc(inputTextFile);
         }
      while ((C != DELIMITER) && (!feof(inputTextFile)));

      if (feof(inputTextFile))
         allDone =TRUE;
      else                             // Found the starting DELIMITER, read in the keyword up to the colon
         {
         keywordIndex = 0;
         
         do {                          // Read in the key word one bye at a time until we hit a colon, being sure to not overrun the keyWord buffer.
            C = fgetc(inputTextFile);
            keyWord[keywordIndex] = C;
            if (C != COLON) ++keywordIndex;
            if (keywordIndex == sizeof(keyWord)) --keywordIndex;  // Don't advance past the end of the buffer space.
            }
         while ((C != COLON) && (!feof(inputTextFile)));

         keyWord[keywordIndex] = NUL;

         if (feof(inputTextFile)) // Now read in the text and print it one character at a time until the next DELIMITER or EOF
            {
            printf("ERROR: Unexpeted EOF reading in key word (missing colon) '%s'\n", keyWord);
            allDone =TRUE;
            }
         else
            {
            strupr(keyWord);
            fwrite(keyWord, 1, strlen(keyWord)+1, outputHelpFile); // NUL terminated keyword string (uppercase)

            printf("\n%s: ", keyWord);

            do {                          // Read in text bytes and print/save all but the DELIMITER. Quit if we read EOF.
               C = fgetc(inputTextFile);

               if (feof(inputTextFile))
                  allDone = TRUE;
               else if (C != DELIMITER)
                  {
                  fputc(C, outputHelpFile);
                  }
               }
            while ((C != DELIMITER) && (!feof(inputTextFile)));

            fputc(NUL, outputHelpFile);

            offsetToNextEntry = ftell(outputHelpFile);
            fseek(outputHelpFile, offsetWriteLocation, SEEK_SET);
            fwrite(&offsetToNextEntry, 4, 1, outputHelpFile); // Place holder for the file seek pointer
            fseek(outputHelpFile, 0L, SEEK_END);

            offsetWriteLocation = offsetToNextEntry;          // Start of next entry at what is now the end of the file
            offsetToNextEntry = 0;
            }

         } // if (feof(inputTextFile)) else

      }
   while (!allDone);

   fwrite(&offsetToNextEntry, 4, 1, outputHelpFile); // Write out four zero bytes to indicate the end of the file.

   fclose(inputTextFile);
   fclose(outputHelpFile);

   return(0);
   }

int main(int argc, char *argv[])
   {
   if (sizeof(LONG) != 4)
      {
      printf("The sizeof LONG needs to be 4 bytes.\n Try redefining in atcs.h");
      exit(1);
      }

   if (argc == 1) // No command line arguments, so build the help file from help.txt
      {
      encodeHelpText("help.txt", "TISAN.HLP");
      printf("\n");
      }
   else  // Run the help engine on the argument passed
      {
      decodeHelpText("TISAN.HLP", argv[1]);
      }
   
   exit(0);
   }
