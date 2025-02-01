/*
**
** Dr. Eric R. Nelson
** 12/28/2016
**
*   Verb Functions for the TISAN Interpreter
*/
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "tisan.h"

#define Vrbstrnm 9 // Longest verb string +1 for the NUL at the end

void VERBINIT(void);
short  VERBCHK(char *);
short  VERB(short);
short  Cls(void);
short  Config(void);
short  Defaults(void);
short  Stop(void);

extern char *MSP[];
extern short ECHO;
extern short MATCH;
extern BOOL bExactMatch;

char *VERBSTR[Vrbstrnm];

/*********************************************************************
*
* Initialize VERBS
*
*/
void VERBINIT()
   {
   VERBSTR[0]  = "EXIT";
   VERBSTR[1]  = "QUIT";
   VERBSTR[2]  = "CLS";
   VERBSTR[3]  = "CONFIG";
   VERBSTR[4]  = "DEFAULTS";
   VERBSTR[5]  = "END";
   VERBSTR[6]  = "ECHO";
   VERBSTR[7]  = "NOECHO";
   VERBSTR[8]  = "STOP";
   return;
   }

/*********************************************************************
* Verb Cls
*
* Clear Screen
*
*/
short Cls()
   {
   return(system("clear"));
   }

/*********************************************************************
* Verb STOP
*
* Stop all run files.
*
*/
short Stop()
   {
   extern BOOL bStopRun;
   bStopRun = TRUE;
   return(-1);
   }

/*********************************************************************
*
* Set up Screen Configuration (use for debugging) NOT IMPLEMENTED
*
*/
short Config()
   {
/*
   char szpath[256];
   char drive[256], dir[256], fname[256], ext[256];
   struct CATSTRUCT *CatList;
   char INFILE[_MAX_PATH];
   int I;
   PSTR pChar;
   char oldinname[256], oldinclass[256];

   strcpy(oldinname,INNAME);
   strcpy(oldinclass,INCLASS);

   zBuildFileName(M_inname,INFILE);

   CatList = ZCatFiles(INFILE);

   for (I = 0, pChar = CatList->pList;
        (I < CatList->N);
        ++I, pChar = strchr(pChar,'\0') + 1)
      {
      splitPath(pChar,drive,dir,fname,ext);
      strcpy(INNAME,fname);
      strcpy(INCLASS,ext);
      zBuildFileName(M_inname,INFILE);
      printf("INFILE='%s'\n", INFILE);
      }

   strcpy(INNAME,oldinname);
   strcpy(INCLASS,oldinclass);
*/
   return(0);
   }

/*********************************************************************
*
* Verb DEFAULTS Set default values
*
*/
short Defaults()
   {
   short I;

   for (I=0;I<2;++I)
      {
      TRANGE[I]=0.;
      YRANGE[I]=0.;
      ZRANGE[I]=0.;
      TMAJOR[I]=-1;
      YMAJOR[I]=-1;
      ZMAJOR[I]=-1;
      POINT[I]=0;
      }
   for (I=0;I<4;++I) WINDOW[I]=0;
   for (I=0;I<PARMSCOUNT;++I) PARMS[I]=0;
   memset(INNAME,0,sizeof(INNAME));
   memset(INCLASS,0,sizeof(INCLASS));
   memset(INPATH,0,sizeof(INPATH));
   memset(IN2NAME,0,sizeof(IN2NAME));
   memset(IN2CLASS,0,sizeof(IN2CLASS));
   memset(IN2PATH,0,sizeof(IN2PATH));
   memset(IN3NAME,0,sizeof(IN3NAME));
   memset(IN3CLASS,0,sizeof(IN3CLASS));
   memset(IN3PATH,0,sizeof(IN3PATH));
   memset(OUTNAME,0,sizeof(OUTNAME));
   memset(OUTCLASS,0,sizeof(OUTCLASS));
   memset(OUTPATH,0,sizeof(OUTPATH));
   memset(TFORMAT,0,sizeof(TFORMAT));
   memset(YFORMAT,0,sizeof(YFORMAT));
   memset(ZFORMAT,0,sizeof(ZFORMAT));
   memset(PARITY,0,sizeof(PARITY));
   memset(DEVICE,0,sizeof(DEVICE));
   memset(TLABEL,0,sizeof(TLABEL));
   memset(YLABEL,0,sizeof(YLABEL));
   memset(ZLABEL,0,sizeof(ZLABEL));
   memset(TITLE,0,sizeof(TITLE));
   FACTOR = 0;
   CODE = 0;
   ITYPE = 0;
   BAUD = 0;
   STOPBITS = 0;
   PROGRESS = 10;
   TMINOR = 3;
   YMINOR = 3;
   ZMINOR = 3;
   FRAME = 0;
   BORDER = 1;
   COLOR = 0L;
   QUIET = 0;
   return(0);
   }

/*********************************************************************
*
* Search for a VERB return 0 if no match
*
*/
short VERBCHK(char *PNTR)
   {
   unsigned short I, LEN;
   short VRB=0;

   if ((LEN = strlen(strupr(PNTR))) == 0) return(0);

   if (MATCH < 0)
      {
      for (I=0;I<Vrbstrnm;++I)
         {
         if (!strncmp(VERBSTR[I],PNTR,LEN))
            printf("%sCould be %-.8s\n",MSP[1],VERBSTR[I]);
         }
      return(0);
      }

   for (I=0;I<Vrbstrnm;++I)
      {
      if (!strncmp(VERBSTR[I],PNTR,LEN))
         {
         VRB = I+1;
         ++MATCH;
         if (LEN == strlen(VERBSTR[I])) /* Exact Match */
            {
            bExactMatch = TRUE;
            MATCH = 1;
            break;
            }
         }
      }

   return(VRB);
   }

/*********************************************************************
*
* Process a VERB
*
*/
short VERB(short N)
   {
   switch (N)
      {
      case 1: /* EXIT */
         zPutAdverbs(MSP[0]);
      case 2: /* QUIT */
         exit(0);
         break;
      case 3: /* Cls */
         Cls();
         break;
      case 4: /* Config */
         Config();
         break;
      case 5: /* Defaults */
         Defaults();
         break;
      case 6: /* END */
         return(-1);
         break;
      case 7: /* ECHO */
         ECHO = 1;
         break;
      case 8: /* NOECHO */
         ECHO = 0;
         break;
      case 9: /* Stop */
         return(Stop());
         break;
      }
   return(0);
   }
