/*
**
** Replacements for the old DOS splitpath and makepath functions, but without the bugs when passed NIL pointers
**
** Dr. Eric R. Nelson
** 12/28/2016
*/
#ifndef dos_h
#define dos_h

#include "atcs.h"

#define stdprn stdout

#define _A_NORMAL 0x0

#define _MAX_PATH  512
#define _MAX_DRIVE 32
#define _MAX_DIR   256
#define _MAX_FNAME 128
#define _MAX_EXT   32

struct CATSTRUCT {int N;
                  PSTR pList;};


int endoffile; // needed to mimic the old eof(HANDLE) function.

PSTR splitPath(const PSTR path, PSTR drive, PSTR dir, PSTR fname, PSTR ext);
PSTR makePath(PSTR path, const PSTR drive, const PSTR dir, const PSTR fname, const PSTR ext);

char firstChar(PSTR string);
char lastChar(PSTR string);

BOOL isNullString(PSTR string);
BOOL isNotNullString(PSTR string);

BOOL isEmptyString(PSTR string);
BOOL isNotEmptyString(PSTR string);

PSTR strupr(PSTR p);
PSTR strlwr(PSTR p);

long filelength(HANDLE hFile);
char* itoa(int value, PSTR str, int base);
int eof(HANDLE hFile);
int kbhit(void);
struct CATSTRUCT* fileDirectory(PSTR pPath, struct CATSTRUCT* pCatList);

void fcloseall(void); // body must be in the task code if used

char getch(void);
char getche(void);

#endif
