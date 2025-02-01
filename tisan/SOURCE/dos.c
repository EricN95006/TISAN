/*
**
** Dr. Eric R. Nelson
** 12/28/2016
**
** Replacements for the old DOS splitpath and makepath functions, but without the bugs when passed NIL pointers
** Also has string upper/lower as well as a replacement for the old getche() function (get a character from the terminal and echo it)
** and a number of other old, and very useful, functions that were in the MS-DOS version of C.
**
** 1/13/2017 Replaced ? with . in getting the file directory
*/
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <dirent.h>
#include <regex.h>

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include <termios.h>

#include "dos.h"

static char emptyPath = NUL;  // Use to return an empty string when NIL was passed

/*
** Return the first character in a string
*/
char firstChar(PSTR string)
   {
   return(*string);
   }

/*
** Return the last character in a string
*/
char lastChar(PSTR string)
   {
   return(*(string + (strlen(string) - 1)));
   }


BOOL isNullString(const PSTR string)
   {
   return(string == (PSTR)NIL ? TRUE : FALSE);
   }

BOOL isNotNullString(const PSTR string)
   {
   return(!isNullString(string));
   }

BOOL isEmptyString(const PSTR string)
   {
   return(*string == NUL ? TRUE : FALSE);
   }

BOOL isNotEmptyString(const PSTR string)
   {
   return(!isEmptyString(string));
   }

/*************************************************************
** Breaks a path into its component parts
** Any NIL pointers passed as arguments are simply not used
** so you can use this function to get just the pieces you want.
**
** A fully qualified path is
** drive:/dir/fname.ext
**
** The function keeps the colon in the drive specifier.
** It does NOT keep the period in the extension.
**
** Properly handles . and .. as well as *.*
*/

PSTR splitPath(const PSTR path, PSTR drive, PSTR dir, PSTR fname, PSTR ext)
   {
   PSTR pCopy, pmem;   // pmem an original pointer needed to free memory, pCopy is the one we play with.
   PSTR pStr;
   PSTR pReturnPath;
   long n;
   
// Make empty strings out of all the non-null pointers
   if (isNotNullString(drive)) *drive = NUL;
   if (isNotNullString(dir))   *dir   = NUL;
   if (isNotNullString(fname)) *fname = NUL;
   if (isNotNullString(ext))   *ext   = NUL;

   if (isNullString(path))       // in the event that path is null, return a pointer to a safe empty string (which is a global in this module)
      pReturnPath = &emptyPath;
   else
      {
      pReturnPath = path;
      
      pmem = malloc(strlen(path) + 1); // we are going to modify the path string, so mangle a copy.

      if (pmem != (PSTR)NIL)
         {
         pCopy = pmem;                 // We are going to change the pCopy pointer, pmem is needed to free the memory later
         strcpy(pCopy, path);

// drive
         pStr = strchr(pCopy,':');     // Points to the colon if there is one
         if (isNotNullString(pStr))
            {
            n = pStr - pCopy + 1;
            if (isNotNullString(drive)) strncpy(drive, pCopy, n);
            *(drive + n) = NUL;
            pCopy = pStr + 1; // Move past the drive specifier, now points at either the directory or filename
            }
// ext
         pStr = strrchr(pCopy,'.');    // Points to the last period if there is one.
         if (isNotNullString(pStr))
            {
            if ((pStr != pCopy) && (*pCopy != '.'))       // In case of . and .. don't treat them as the extension delimiter
               {
               if (isNotNullString(ext)) strcpy(ext, pStr+1);  // copy starting past the period
               *pStr = NUL; // Truncate path at the extension
               }
            }
// dir
         pStr = strrchr(pCopy,'/');                         // Points to the last slash if there is one

         if (isNotNullString(pStr))
            {
            n = pStr - pCopy + 1;
            if (isNotNullString(dir)) strncpy(dir, pCopy, n);
            *(dir + n) = NUL;
            pCopy = pStr+1; // Should be the filename just past either the slash or the period
            }

// fname
         if (isNotNullString(fname)) strcpy(fname, pCopy);

         free(pmem);
         }
      }

   return(pReturnPath);
   }


/*************************************************************
** Crates a complete path from the arguments while making sure
** such things as colons, slashes, and periods are where
** they belong. A full path looks like
**
** drive:/dir/fname.ext
**
*/
PSTR makePath(PSTR path, const PSTR drive, const PSTR dir, const PSTR fname, const PSTR ext)
   {
   PSTR pDirStr;
   
   if (isNullString(path))
      path = &emptyPath;
   else
      {
      *path = NUL; // initialize the path string so we can concatenate
/*
** drive should be of the form "Identifier:"
**
** Copy the drive string to the path and then add a ":" if it needs one
*/
      if (isNotNullString(drive) && isNotEmptyString(drive))
         {
         strcat(path, drive);

         if (lastChar(drive) != ':') strcat(path, ":"); // is last character a colon?
         }
/*
** If the drive is specified then we must have a leading slash since the path will be fully qualified
*/
      if (lastChar(path) == ':') strcat(path, "/");
/*
** If the first character of pDirStr is a slash and the path already ends in one, then don’t add it again.
** The directory should end in a '/', so use the same logic as above.
*/
      if (isNotNullString(dir) && isNotEmptyString(dir))
         {
         pDirStr = dir;

         if ((lastChar(path) == '/') && (firstChar(pDirStr) == '/')) ++pDirStr; // Skip over duplicate slash

         strcat(path, pDirStr);

         if (lastChar(pDirStr) != '/') strcat(path, "/"); // is last character a slash? If not then it needs one.
         }
/*
** The filename needs no special treatment. Use it as is.
*/
      if (isNotNullString(fname)) strcat(path, fname);

/*
** The extension should start with a period, so put it in first if needed
*/
      if (isNotNullString(ext) && isNotEmptyString(ext))
         {
         if (firstChar(ext) != '.') strcat(path, "."); // is first character a period?

         strcat(path, ext);
         }
      }
      
   return(path);
   }

/*
** The strupr and strlwr functions can cause a fault if the string is 
** declared const by the calling routine. Be aware!
*/
PSTR strupr(PSTR p)
   {
   PSTR pp;

   if (p)
      {
      pp = p;

      while (*pp)
         {
         *pp = toupper(*pp);
         ++pp;
         }
      }
      
   return(p);
   }

PSTR strlwr(PSTR p)
   {
   PSTR pp;

   if (p)
      {
      pp = p;

      while (*pp)
         {
         *pp = tolower(*pp);
         ++pp;
         }
      }

   return(p);
   }

PSTR itoa(int value, PSTR str, int base)   // itoa routine, the base argument is not used.
   {
   sprintf(str, "%d", value);
   return(str);
   }

long filelength(HANDLE hFile)
   {
   long length, lpos;
   
   lpos = lseek(hFile, 0L, SEEK_CUR);
   length = lseek(hFile, 0L, SEEK_END);
   lseek(hFile, lpos, SEEK_SET);
   
   return(length);
   }

int eof(HANDLE hFile) // Mimics the old eof() function. The Handle is not used here. Need to set end-of-file when doing i/o to use this test.
   {
   return(endoffile != EOF ? 0 : EOF);
   }

/*
** Found on a message board. No attribution.
*/
int kbhit(void)
   {
   struct termios oldt, newt;
   int ch;
   int oldf;
 
   tcgetattr(STDIN_FILENO, &oldt);
   newt = oldt;
   newt.c_lflag &= ~(ICANON | ECHO);
   tcsetattr(STDIN_FILENO, TCSANOW, &newt);
   oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
   fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
 
   ch = getchar();
 
   tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
   fcntl(STDIN_FILENO, F_SETFL, oldf);
 
   if(ch != EOF)
      {
      ungetc(ch, stdin);
      return(1);
      }
 
   return(0);
   }

/*
** Build a list of files based on a file pattern specification (a regular expression).
** The structure CATSTRUCT has the number of files and a pointer to the list.
** The memory for the list is allocated here. It must be freed elsewhere.
**
*/
struct CATSTRUCT* fileDirectory(PSTR pPath, struct CATSTRUCT* pCatList)
   {
   extern short zTaskMessage(short level, char *format, ...);
   DIR* dirp;
   struct dirent *dp;
   regex_t regex;
   int reti;
   char szDrive[_MAX_DRIVE], szDir[_MAX_DIR];
   char szName[_MAX_FNAME], szExt[_MAX_EXT];
   char szFolder[_MAX_PATH], szPattern[_MAX_PATH];
   int iBufferSize = 0, iCount = 0;
   PSTR pBuffer = (PSTR)NIL, pNextStr;
   struct CATSTRUCT* pReturn = (struct CATSTRUCT*)NIL;

   pCatList->N = 0;   // Assume there are no matchibng files

/*
** We are going to replace ? with . since we allow for old style directory matching syntax
*/
   splitPath(pPath, szDrive, szDir, szName, szExt);

   if (strcmp(szName, "*") == 0) strcpy(szName, "^.*");   // Need to make a useable regular expression in the event of just an *
   if (strlen(szExt) > 0) strcat(szName, "\\");          // If there is an extension then we need a \. to match the period, so add a \ the name

   while ((pNextStr = strchr(szName, '?'))) *pNextStr = '.';
   while ((pNextStr = strchr(szExt,  '?'))) *pNextStr = '.';

   makePath(szFolder,  szDrive,   szDir,     (PSTR)NIL, (PSTR)NIL);     // Used for the directory
   makePath(szPattern, (PSTR)NIL, (PSTR)NIL, szName,    szExt);         // Used for matching the regular expression

   if (strlen(szFolder) == 0) strcpy(szFolder,".");   // If no directory is specified, default to the current working directory

   zTaskMessage(3,"Matching Pattern = '%s'\n",szPattern);

   reti = regcomp(&regex, szPattern, 0);

   if (reti != 0)
      zTaskMessage(10, "regcomp failed in function fileDirectory in file dos.c\n");
   else
      {
      dirp = opendir(szFolder);
/*
** Step 1 is to find out how much buffer space we need to hold the strings
*/
      if (dirp == (DIR*)NIL)
         zTaskMessage(10, "Directory '%s' not found.\n", szFolder);
      else
         {
         do {
            dp = readdir(dirp);
      
            if (dp != (struct dirent *)NIL)
               {
               reti = regexec(&regex, dp->d_name, 0, NULL, 0);
               if (!reti)
                  {
                  iBufferSize += (strlen(dp->d_name) + 1);
                  ++iCount;
                  }
               }
            }
         while (dp);
   
         if (iCount)
            {
            iBufferSize += 2; // Add space to two extra null bytes
   
            pBuffer = (PSTR)malloc(iBufferSize);
   
            if (pBuffer == (PSTR)NIL)
               printf("memory allocation error in function fileDirectory in file dos.c\n");
            else
               {
               *pBuffer = NUL;   // start with a zero length string
               pNextStr = pBuffer;
   
               rewinddir(dirp);

/*
** Step 2 is to fill the buffer with adjacent string followed by two NUL bytes.
** traverse through the strings by adding strlen(p)+1 to the pointer p.
** When the length is zero you have reached the end.
** Remember to free the memory when you are done.
*/
               do {
                  dp = readdir(dirp);
   
                  if (dp != (struct dirent *)NIL)
                     {
                     reti = regexec(&regex, dp->d_name, 0, NULL, 0);
                     if (!reti)
                        {
                        strcpy(pNextStr, dp->d_name);
                        pNextStr += strlen(dp->d_name) + 1;
                        *pNextStr = NUL;
                        }
                     }
                  }
               while (dp);
      
               ++pNextStr;
               *pNextStr = NUL;
               } // if (pBuffer != (PSTR)NIL)

            pCatList->N = iCount;
            pCatList->pList = pBuffer;
            pReturn = pCatList;
            } // if (iCount)
            
         closedir(dirp);
         } // if (dirp == (DIR*)NIL) else

      regfree(&regex);
      } // if (reti) else
      
   return(pReturn);
   }

/*
** http://stackoverflow.com/questions/7469139/what-is-equivalent-to-getch-getche-in-linux
*/
static struct termios old, new;

/* Initialize new terminal i/o settings */
void initTermios(int echo) 
   {
   tcgetattr(0, &old); /* grab old terminal i/o settings */
   new = old; /* make new settings same as old settings */
   new.c_lflag &= ~ICANON; /* disable buffered i/o */
   new.c_lflag &= echo ? ECHO : ~ECHO; /* set echo mode */
   tcsetattr(0, TCSANOW, &new); /* use these new terminal i/o settings now */
   }

/* Restore old terminal i/o settings */
void resetTermios(void) 
   {
   tcsetattr(0, TCSANOW, &old);
   return;
   }

/* Read 1 character - echo defines echo mode */
char getch_(int echo) 
   {
   char ch;

   initTermios(echo);
   ch = getchar();
   resetTermios();

   return(ch);
   }

/* Read 1 character without echo */
char getch(void) 
   {
   return(getch_(0));
   }

/* Read 1 character with echo */
char getche(void) 
   {
   return(getch_(1));
   }
