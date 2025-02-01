/*
**                       Time Series Analyzer
**
** This is thew TISAN CLI user interface
**
** The tisan app accepts a run file on the command line on start up. For example:
**
** ./tisan sample
**
** will run the RUN file sample at start up.
**
** Dr. Eric R. Nelson
** Lats major update was 12/28/2016
*
* Link with tisanlib.o dos.o verbs.o pverbs.o
*
*/
#include <unistd.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <signal.h>

#include "tisan.h"

#define ENDOFLINE       ((char)10)       // Could be 13 on some systems
#define ESCAPE          ((char)27)
#define BACKSPACE       ((char)8)
#define DELETE          ((char)127)
#define SINGLEQUOTE     ((char)39)
#define SEMICOLON       (';')
#define TAB             ('\t')
#define SPACE           (' ')

/*
** Define Functions
*/
void   PVINIT(void);
void   VERBINIT(void);
void   HANDELER(int a);
short  PROCESS(void);
short  ADVCHK(char *);
short  VERBCHK(char *);
short  PVERBCHK(char *);
char  *SEVAL(char *);
short  ADVERB(short);

short  SIMPLE(short,short,short);
double GETVAL(char *,short,short);
void   PUTVAL(char *,short,short,double);

/*
** in verbs.c
*/
short  Config(void);
short  Defaults(void);
short  VERB(short);

/*
** in pverbs.c
*/
short  PVERB(short);


/*
** Define Globals that are not adverbs
*/
extern char *VERBSTR[], *PVERBSTR[];
double TM, TB;
char ILINE[256],*IPNTR, JLINE[256],*JPNTR, KLINE[256],*KPNTR;
char  CPA[256];
short MATCH, INDEX;
BOOL bExactMatch;
short ERRTYPE=0, ECHO=1;
short GF1, GF2, GF3, CS1, CS2;
BOOL InterruptFlag;
short EVLFLG, TASKFLG;

/*
** Commonly Displayed Messages
*/
const char *MSP[] =
{"TISAN",
"TISAN   : ",
"\nTISAN   : ",
"TISAN   ",
"TISAN   : Ambiguous Entry '%s'\n",
"Environment Too Big\n",
"%sError Opening File '%s'\n",
"Error Opening ",
"Error Reading ",
"File '%s' not found\n",
"Insufficient Memory to Execute %s\n",
"TISAN   : Invalid File Specification\n",
"Invalid Mode\n",
"TISAN   : Sorry, No Graphics Display Available\n",
"Not an Executable File\n",
"TISAN   : Sorry, Inputs File '%s' is not Available.\n"
};

/******************************************
** Populate the strings for the Full Path for the TISAN
** Program on startup so we know where to
** find the INPUTS and RUN files.
*/

const char szTask[]="TISAN"; // All tasks must define the szTask array to use the TISAN messaging system 

/*
** Character strings used throughout the program
*/
int main(int argc, char *argv[])
   {
   short I;
   char C;
   extern BOOL bStopRun;

   signal(SIGINT,HANDELER);

   splitPath(argv[0],TisanDrive, TisanDir,(PSTR)NIL,(PSTR)NIL);

   printf("'%s'\n",argv[0]);

   printf("\nWelcome to the Time Series Analyzer Version 5.0\n");
   
/*
** Build ADVSTR, VERBSTR, PVERBSTR and ADVPNTR, ADVSIZE
*/
   PVINIT();
   VERBINIT();

   initializeAdverbArrays();

   bStopRun = FALSE;

   if (!zGetAdverbs((char *)MSP[0]))
      {
      printf("%sPevious Environment Restored\n",MSP[1]);
      zTaskMessage(1,"Current Task is %s.\n", TASKNAME);
      }
   else
      {
      printf("%sNo Previous Environment.\n",MSP[1]);
      printf("%sTASKNAME is undefined.\n",MSP[1]);
      Config();
      Defaults();
      }

/*
** Process the command line. Argument can be the name of a run file...
*/
   if (argc > 1)
      {
      strcpy(ILINE, "run ");
      strcat(ILINE, argv[1]);
      PROCESS();
      }

   while (TRUE) // Process forever until exit() is called elsewhere
      {
      I = 0;
      memset(ILINE,0,sizeof(ILINE));
      printf("%s>",MSP[0]);
      do {
         if (InterruptFlag)
            {
            I=0;
            InterruptFlag=FALSE;
            }
         C=getch();

         if (!C) getch();

         if (C == DELETE) C = BACKSPACE;

         if (C == TAB) C = SPACE;
         
         if (C == ESCAPE)  /* Escape Pressed */
            {
            I=0;
            printf("\n");
            C=ENDOFLINE;
            memset(ILINE,0,sizeof(ILINE));
            }
         else if ((C == BACKSPACE) && (I > 0))
            {
            ILINE[--I] = 0;
            putchar(C);
            putchar(SPACE);
            putchar(C);
            }
         else if ((I < sizeof(ILINE)) && (C != 0) && (C != BACKSPACE))
            {
            ILINE[I] = C;
            ++I;
            putchar(C);
            }
         }
      while (C != ENDOFLINE);
      
      if (C != ESCAPE)
         {
         if (I>0) ILINE[--I] = 0;
         PROCESS();
         }
      } // while (TRUE)
   } // int main(int argc, char *argv[])

/*********************************************************************
*
* Process the Input Line
*
*/
short PROCESS()
   {
   short I, F1=0, F2=0, F3;
   unsigned short LEN;

   IPNTR = ILINE;

PSTART:
   JPNTR=memset(JLINE,0,sizeof(JLINE));
   KPNTR=memset(KLINE,0,sizeof(KLINE));

/* Strip leading spaces from ILINE */
   while (isspace(*IPNTR)) ++IPNTR;

/* Find semicolon or single quote (also handle smart single quotes), whichever is first */
   if ((LEN = strlen(IPNTR)) == 0) return(0);

   for (F1=F2=I=0;I<LEN;++I)
      {
      if ((*(IPNTR + I) == ';') && (!F1))  /* Semicolon */
         F1 = I;
      else if ((*(IPNTR + I) == SINGLEQUOTE) && (!F2)) /* Single Quote */
         F2 = I;
      if (F1 && F2) break;
      }

   if ((F1 <= F2) || (!F2))  /* Semicolon was first or none at all*/
      {
      if (!F1) F1 = LEN;
      strncpy(JLINE,IPNTR,F1);   /* extract command line */
      JLINE[F1] = (char)0;
      IPNTR += (++F1);           /* advance pointer to new CL */
      }
   else            /* Single Quote was first, ; is in a string? */
      {
      ++F2;
      for (I=F2;I<LEN;++I)   /* Find end of string */
         if (*(IPNTR + I) == SINGLEQUOTE) break;
      F2 = ++I;   /* Now find semicolon */
      for (I=F2;I<LEN;++I)
         if (*(IPNTR + I) == SEMICOLON) break;      /* Semicolon */
      strncpy(JLINE,IPNTR,F2);   /* extract command line */
      JLINE[F2] = (char)0;
      IPNTR += (++I);              /* advance pointer to new CL */
      }
/*
*  At this point JLINE has the current CL and IPNTR points
*  to the next CL in ILINE.
*  Now separate the possible two parts of the CL.
*/
   while (isspace(*JPNTR)) ++JPNTR;     /* Remove Leading Spaces */
   LEN = strlen(JPNTR);
   for (I=0;I<LEN;++I)
      {
      if (isspace(*(JPNTR + I)) || (*(JPNTR + I) == '=')) break;
      }
      
   strncpy(KLINE,JPNTR,I);   /* extract first word of CL */
   KLINE[I] = (char)0;
   JPNTR += (++I);              /* advance pointer to expression */
   while (isspace(*JPNTR)) ++JPNTR;     /* Remove Leading Spaces */
/*
*  At this point KPNTR points to the stripped first word
*  and JPNTR points to the stripped remaining expression in JLINE
*  so now process KPNTR
*/
   bExactMatch = FALSE;
   MATCH = 0;            /* First Check for an Ambiguous Match */
   ERRTYPE = 0;

   F1 = ADVCHK(KPNTR);       /* While getting command index        */
   if (!bExactMatch)
      {
      F2 = VERBCHK(KPNTR);
      if (!bExactMatch)
         {
         F3 = PVERBCHK(KPNTR);

         if (bExactMatch)
            {
            F1 = -1;
            F2 = 0;
            }
         }
      else
         F1 = -1;
      }

   if (MATCH > 1)  /* Ambiguous Entry */
      {
      MATCH = -1;   /* Set MATCH is -1 as flag */
      ADVCHK(KPNTR);
      VERBCHK(KPNTR);
      PVERBCHK(KPNTR);
      printf(MSP[4],KPNTR);
      return(1);
      }

   if (F1 >= 0)      /* ADVERB */
      {
      I = ADVERB(F1);
      if (I) return(I); /* Place Value into Adverb */
      }
   else if (F2) /* VERB */
      {
      if (strlen(JPNTR))
         {
         printf("%sArgument following a VERB\n",MSP[1]);
         return(1);
         }
      I = VERB(F2);
      if (I) return(I);
      }
   else if (F3)        /* PSEUDOVERB */
      {
      I = PVERB(F3);
      if (I) return(I);
      }
   else if (!ERRTYPE)
      {
      printf("%sUnknown command '%s'\n",MSP[1],KPNTR);
      return(1);
      }

   goto PSTART;
   }

/*********************************************************************
*
* Search for an ADVERB
* INDEX is used to determine a specific array element.
* if INDEX=-1 then the array name was given without an index.
* returns adverb number if found, returns -1 if no match, -2 if error.
* NOTE: a return value of zero is TASKNAME
*       also -1 means that no argument was in JPNTR
*
*/
short ADVCHK(char *PNTR)
   {
   unsigned short I, LEN;
   short ADV=-1, F1=0, F2=0;

   if (!strlen(strupr(PNTR))) return(-1);

/*
*  First see if there are []
*/
   INDEX = -1;
   LEN = strlen(PNTR);
   for (I=0;I<LEN;++I)
      {
      if (*(PNTR+I) == '[') F1 = I;
      else if (*(PNTR+I) == ']') F2 = I;
      if (F1 && F2) break;
      }
   if ((F1 && !F2) || (F1 > F2))
      {
      printf("%sMissing ']'\n",MSP[1]);
      ERRTYPE = 1;
      return(-2);
      }
   else if (!F1 && F2)
      {
      printf("%sMissing '['\n",MSP[1]);
      ERRTYPE = 1;
      return(-2);
      }

   if (F1)
      {
      if (*(PNTR + F2 + 1)) return(-1); /*Stuff Following ] */
      INDEX = atoi((PNTR+F1 + 1)) - 1;
      *(PNTR + F1) = (char)0;     /* Mask off [ */
      }

   LEN = strlen(PNTR);
   if (MATCH < 0)
      {
      for (I=0;I<ADVCOUNT ;++I)
         {
         if (!strncmp(ADVSTR[I],PNTR,LEN))
            printf("%sCould be %-.8s\n",MSP[1],ADVSTR[I]);
         }
      return(-1);
      }

   for (I=0;I<ADVCOUNT;++I)
      {
      if (!strncmp(ADVSTR[I],PNTR,LEN))
         {
         ADV = I;
         ++MATCH;
         if (LEN == strlen(ADVSTR[I])) /* Exact Match */
            {
            bExactMatch = TRUE;
            MATCH = 1;
            break;
            }
         }
      }
   if (!strlen(JPNTR)) EVLFLG=1; else EVLFLG=0;
   if (!ADV) TASKFLG = 1; else TASKFLG = 0;
   return(ADV);
   }

/*********************************************************************
*
* Evaluate PNTR and return a string literal
*/
char *SEVAL(char *cPointer)
   {
   char *AdverbPointer;
   short I=0, N;

   if ((*cPointer == (char)39) || TASKFLG)  /* ' is First Character */
      {
      if (*cPointer == (char)39) ++cPointer;
      while ((*(cPointer + I) != (char)39) && (*(cPointer + I) != (char)0))
         {
         CPA[I] = *(cPointer + I);
         ++I;
         }
      CPA[I] = (char)0;
      return(CPA);
      }

   MATCH=0;
   N = ADVCHK(cPointer);
   if (MATCH > 1)
      {
      MATCH = -1;
      ADVCHK(cPointer);
      printf(MSP[4],cPointer);
      return(NULL);
      }
   
   if (N < 0) return(NULL);    /* Error */

   if (INDEX < 0) INDEX = 0;
   if (INDEX > ADVSIZE[N]) INDEX=ADVSIZE[N];

   switch (N)
      {
      case 0:   /* String Adverbs */
         AdverbPointer = TASKNAME;
         break;
      case 1:
         AdverbPointer = INNAME;
         break;
      case 2:
         AdverbPointer = INCLASS;
         break;
      case 3:
         AdverbPointer = INPATH;
         break;
      case 4:
         AdverbPointer = IN2NAME;
         break;
      case 5:
         AdverbPointer = IN2CLASS;
         break;
      case 6:
         AdverbPointer = IN2PATH;
         break;
      case 7:
         AdverbPointer = IN3NAME;
         break;
      case 8:
         AdverbPointer = IN3CLASS;
         break;
      case 9:
         AdverbPointer = IN3PATH;
         break;
      case 10:
         AdverbPointer = OUTNAME;
         break;
      case 11:
         AdverbPointer = OUTCLASS;
         break;
      case 12:
         AdverbPointer = OUTPATH;
         break;
      case 20:
         AdverbPointer = TFORMAT;
         break;
      case 21:
         AdverbPointer = YFORMAT;
         break;
      case 22:
         AdverbPointer = ZFORMAT;
         break;
      case 23:
         AdverbPointer = DEVICE;
         break;
      case 37:
         AdverbPointer = TLABEL;
         break;
      case 38:
         AdverbPointer = YLABEL;
         break;
      case 39:
         AdverbPointer = ZLABEL;
         break;
      case 40:
         AdverbPointer = TITLE;
         break;
      case 13: /* Integer Numeric Adverbs */
      case 14:
      case 19:
      case 24:
      case 25:
      case 26:
      case 28:
      case 29:
      case 30:
      case 31:
      case 32:
      case 33:
      case 42:
      case 15: /* Double Numeric Adverbs */
      case 16:
      case 17:
      case 18:
      case 34:
      case 35:
      case 36:
      case 41:
      case 43:
      case 44:
         printf("%sNumeric Adverb Unexpected '%s'\n",MSP[1],cPointer);
      default:
         return(NULL);
      }

   strcpy(CPA+I,AdverbPointer + INDEX);

   return(CPA);
   }

/***************************************************************
**
** Process ^C Interrupt
*/
void HANDELER(int a)
   {
   extern BOOL bStopRun;

   signal(SIGINT,HANDELER);
   InterruptFlag = TRUE;
   memset(ILINE,0,sizeof(ILINE));
   memset(JLINE,0,sizeof(JLINE));
   memset(KLINE,0,sizeof(KLINE));
   IPNTR = ILINE;
   JPNTR = JLINE;
   KPNTR = KLINE;
   bStopRun = TRUE;
   return;
   }

/***************************************************************
**
** Simple Adverb Equate
*/
short SIMPLE(short N, short F1, short IDX)
   {
   short M, I;

   switch (F1)
      {
      case 0:   /* String Adverbs */
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
      case 9:
      case 10:
      case 11:
      case 12:
      case 20:
      case 21:
      case 22:
      case 23:
      case 37:
      case 38:
      case 39:
      case 40:
         printf("%sString Adverb Unexpected '%s'\n",MSP[1],KPNTR);
         return(1);
         break;
      default: // All numeric adverbs are treated the same way
         if ((IDX <= 0) && (INDEX <= 0))     /* ARRAY1   = ARRAY2   */
            {
            M = Min(ADVSIZE[N],ADVSIZE[F1]);
            for (I=0;I<M;++I)
               PUTVAL(ADVPNTR[N],I,N,GETVAL(ADVPNTR[F1],I,F1));
            return(0);
            }
         else if ((IDX > 0) && (INDEX <= 0)) /* ARRAY1[] = ARRAY2   */
            {
            if (IDX>ADVSIZE[N]) IDX = ADVSIZE[N];
            M = Min(ADVSIZE[N]-IDX,ADVSIZE[F1]);
            for (I=0;I<M;++I)
               PUTVAL(ADVPNTR[N],I+IDX,N,GETVAL(ADVPNTR[F1],I,F1));
            return(0);
            }
         else if ((IDX <= 0) && (INDEX > 0)) /* ARRAY1   = ARRAY2[] */
            {
            if (INDEX>ADVSIZE[F1]) INDEX = ADVSIZE[F1];
            M = ADVSIZE[N];
            for (I=0;I<M;++I) 
               PUTVAL(ADVPNTR[N],I,N,GETVAL(ADVPNTR[F1],INDEX,F1));
            return(0);
            }
         else                                /* ARRAY1[] = ARRAY2[] */
            {
            if (INDEX>ADVSIZE[F1]) INDEX = ADVSIZE[F1]-1;
            if (IDX>ADVSIZE[N]) IDX = ADVSIZE[N]-1;
            PUTVAL(ADVPNTR[N],IDX,N,GETVAL(ADVPNTR[F1],INDEX,F1));
            }
         break;
      }
   return(0);
   }
   
/***************************************************************
**
** Get a numeric adverb value and return double
*/
double GETVAL(char *PNTR, short I, short N)
   {
   double D;

   switch (N)
      {
      case 13: // Short Numeric Adverbs
      case 14:
      case 19:
      case 24:
      case 25:
      case 26:
      case 28:
      case 29:
      case 31:
      case 32:
      case 33:
      case 42:
         D = (double) *( (short *)PNTR + I);
         break;
      case 30: // LONG Numeric Adverbs
         D = (double) *( (LONG *)PNTR + I);
         break;
      case 15: /* Double Numeric Adverbs */
      case 16:
      case 17:
      case 18:
      case 34:
      case 35:
      case 36:
      case 41:
      case 43:
      case 44:
         D = *( (double *)PNTR + I );
      }
   return(D);
   }

/***************************************************************
**
** Put a value into a numeric adverb
*/
void PUTVAL(char *PNTR, short I, short N, double D)
   {
   switch (N)
      {
      case 13: // Short Numeric Adverbs
      case 14:
      case 19:
      case 24:
      case 25:
      case 26:
      case 28:
      case 29:
      case 31:
      case 32:
      case 33:
      case 42:
         *( (short *)PNTR + I ) = (short)D;
         break;
      case 30: // LONG Numeric Adverbs
         *( (LONG *)PNTR + I ) = (LONG)D;
         break;
      case 15: /* Double Numeric Adverbs */
      case 16:
      case 17:
      case 18:
      case 34:
      case 35:
      case 36:
      case 41:
      case 43:
      case 44:
         *( (double *)PNTR + I ) = D;
      }
   return;
   }

/*********************************************************************
*
* Process an ADVERB number N with an argument JPNTR
* Place Value of Expression JPNTR into ADVERB(N)
*
*/
short ADVERB(short N)
   {
   double D;
   char *P;
   short IDX, I, M, F1;

   IDX = INDEX;  /* Get current array index */
   switch (N)
      {
      case 0:   /* String Adverbs */
         strupr(JPNTR); // TASKNAME must always be upper case
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:
      case 6:
      case 7:
      case 8:
      case 9:
      case 10:
      case 11:
      case 12:
      case 20:
      case 21:
      case 22:
      case 23:
      case 27:
      case 37:
      case 38:
      case 39:
      case 40:
         if (IDX<0) IDX = 0;
         if (EVLFLG)
            {
            printf("%s'%s'\n",MSP[1],ADVPNTR[N]+IDX);
            return(0);
            }
         P = SEVAL(JPNTR);
         if (!P)
            {
            printf("%sInvalid string expression\n",MSP[1]);
            return(1);
            }
         if (IDX>ADVSIZE[N]) IDX = 0;
         *(P + ADVSIZE[N] - IDX) = 0;
         strcpy(ADVPNTR[N]+IDX,P);
         break;
      case 13: /* Integer (short and LONG) Numeric Adverbs */
      case 14:
      case 19:
      case 24:
      case 25:
      case 26:
      case 28:
      case 29:
      case 30: // COLOR is LONG, not processed as HEX value
      case 31:
      case 32:
      case 33:
      case 42:
      case 15: /* Double Numeric Adverbs */
      case 16:
      case 17:
      case 18:
      case 34:
      case 35:
      case 36:
      case 41:
      case 43:
      case 44:
/*
** Array into Array
** Array into Array[]
** Value into Array
** Value into Array[]
*/
         if (EVLFLG) // Evaluate and print the result
            {
            if (IDX<0)
               {
               IDX=0;
               M=ADVSIZE[N];
               }
            else
               M=IDX;
            printf("%s",MSP[1]); // print "TISAN   : "
            for (I=IDX;I<M;++I)
               {
               printf("%.12lG",GETVAL(ADVPNTR[N],I,N));
               if (I <(M-1)) printf(", ");
               }
            printf("\n");
            return(0);
            }
         MATCH = 0;
         F1 = ADVCHK(JPNTR);   /* Check for ARRAY to ??  */
         if (MATCH > 1)  /* Ambiguous Entry */
            {
            MATCH = -1;   /* Set MATCH is -1 as flag */
            ADVCHK(JPNTR);
            printf(MSP[4],JPNTR);
            return(1);
            }

         if (F1 >=0)       /* Adverb Found so go to work and equate */
            return(SIMPLE(N,F1,IDX));

         if (IDX < 0)        /* ?? into ARRAY              */
            {
            P = strtok(JPNTR," ,");
            for (I=0;I<ADVSIZE[N];++I)
               {
               if (P) D = atof(P);              // treat all numbers a floating point for conversion purposes using atof()
               PUTVAL(ADVPNTR[N],I,N,D);
               P = strtok(NULL," ,");
               }
            }
         else                /* ?? into ARRAY[]            */
            {
            if (IDX>ADVSIZE[N]) IDX=ADVSIZE[N]-1;
            P = strtok(JPNTR," ,");
            do {
               if (P)
                  {
                  D = atof(P);                  // treat all numbers a floating point for conversion purposes using atof()
                  PUTVAL(ADVPNTR[N],IDX,N,D);
                  }
                  P = strtok(NULL," ,");
               }
            while (P && (++IDX < ADVSIZE[N]));
         }
      }
   return(0);
   }
