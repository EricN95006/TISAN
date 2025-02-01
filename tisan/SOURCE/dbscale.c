/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task DBSCALE
*
* CODE 0 then
*  TRANGE is the new time range
*  YRANGE is the new amplitude range
*
* CODE 1 then
*  TRANGE[0] add to time base
*  YRANGE[0] add to real amplitude
*  ZRANGE    add to complex values
*
* CODE 2 then
*  TRANGE[0] multiply into time base
*  YRANGE[0] multiply into real amplitude
*  ZRANGE    multiply into complex values
*
* CODE 3 then
*  TRANGE[0] divide into time base
*  YRANGE[0] divide into real amplitude
*  ZRANGE    divide into complex values
*
* CODE 4 then
*  TRANGE[0] divide by time base
*  YRANGE[0] divide by real amplitude
*  ZRANGE    divide by complex values
*
* CODE 5 then
*   Set the time base to start at zero.
*
* The infile of this task accepts wild cards
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

double getDataScales(struct FILEHDR *pHeader, FILE *infileStream, double *pTmin, double *pTmax, double *pRmin, double *pRmax);
void   offsetValues(struct FILEHDR *pHeader, FILE *infileStream, FILE *outfileStream);
void   scaleValues(struct FILEHDR *pHeader, FILE *infileStream, FILE *outfileStream, double Tmin, double Tmax, double Rmin, double Rmax);
void   multiplyValues(struct FILEHDR *pHeader, FILE *infileStream, FILE *outfileStream);
void   divideValues(struct FILEHDR *pHeader, FILE *infileStream, FILE *outfileStream);
void   divideintoValues(struct FILEHDR *pHeader, FILE *infileStream, FILE *outfileStream);
void   zeroStartTime(struct FILEHDR *pHeader, FILE *infileStream, FILE *outfileStream, double Tmin);

char TMPFILE[_MAX_PATH];  /* Must be visible to BREAKREQ */

FILE *INSTR  = (FILE*)NIL;
FILE *OUTSTR = (FILE*)NIL;

const char szTask[]="DBSCALE";

int main(int argc, char *argv[])
   {
   char INFILE[_MAX_PATH], OUTFILE[_MAX_PATH];
   struct FILEHDR FileHeader;
   double TMAX, TMIN, YMAX, YMIN;
   double dataCount;
   BOOL bNewTimeRange;
   BOOL bNewDataRange;
   BOOL bNewComplexRange;
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;
   
   signal(SIGINT,BREAKREQ);
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);

   if ((CODE < 0) || (CODE > 5))
      {
      zTaskMessage(10,"Invalid CODE value.\n");
      Zexit(1);
      }

   zBuildFileName(M_inname,INFILE);
   CatList = ZCatFiles(INFILE);     // Find Matching Files for the input file specification
   if (CatList->N == 0) Zexit(1);   // Quit if there are none

   for (iwc = 0, pWild = CatList->pList;
        (iwc < CatList->N) && !ERRFLAG;
        ++iwc, pWild = strchr(pWild,'\0') + 1)
      {
      splitPath(pWild,Drive,Dir,Fname,Ext);
      strcpy(INNAME,Fname);
      strcpy(INCLASS,Ext);

      zBuildFileName(M_inname,INFILE);
      zBuildFileName(M_outname,OUTFILE);
      zBuildFileName(M_tmpname,TMPFILE);

      zTaskMessage(2,"Opening Input File '%s'\n",INFILE);
      INSTR = zOpen(INFILE,O_readb);
      
      if (!INSTR) Zexit(1);
      
      if (!Zgethead(INSTR,&FileHeader)) BombOff(1);

      zTaskMessage(2,"Opening Scratch File '%s'\n",TMPFILE);
      if ((OUTSTR = zOpen(TMPFILE,O_writeb)) == NULL) BombOff(1);

      if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);   // Needed to hold space for the actual header values

   /*
   ** Find existing min/max
   */
      dataCount = getDataScales(&FileHeader, INSTR, &TMIN, &TMAX, &YMIN, &YMAX);
      zTaskMessage(2,"File Contains %ld Points\n", (long)dataCount);
      zTaskMessage(3,"Current TRANGE = %lG, %lG\n", TMIN, TMAX);
      zTaskMessage(3,"Current YRANGE = %lG, %lG\n", YMIN, YMAX);

   /*
   ** Scale by value or with a simple offset
   */
      switch (CODE)
         {
         case 0:  // Linear scaling
            scaleValues(&FileHeader, INSTR, OUTSTR, TMIN, TMAX, YMIN, YMAX);
            if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);
            getDataScales(&FileHeader, OUTSTR, &TMIN, &TMAX, &YMIN, &YMAX);

            if (TRANGE[0] < TRANGE[1]) zTaskMessage(3,"New TRANGE = %lG, %lG\n",TMIN,TMAX);
            if (YRANGE[0] < YRANGE[1]) zTaskMessage(3,"New YRANGE = %lG, %lG\n",YMIN,YMAX);
            break;
         case 1:  // Simple offsets
            offsetValues(&FileHeader, INSTR, OUTSTR);
            if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);
            getDataScales(&FileHeader, OUTSTR, &TMIN, &TMAX, &YMIN, &YMAX);
            
            if (TRANGE[0]) zTaskMessage(3,"New TRANGE = %lG, %lG\n",TMIN,TMAX);

            if ((YRANGE[0] && ((FileHeader.type == R_Data) || (FileHeader.type== TR_Data))) ||
                ((ZRANGE[0] || ZRANGE[1]) && ((FileHeader.type == X_Data) || (FileHeader.type== TX_Data))))
               zTaskMessage(3,"New YRANGE = %lG, %lG\n",YMIN,YMAX);
            break;
         case 2: // multiply by...
            if (!TRANGE[0])
               {
               bNewTimeRange = FALSE;
               TRANGE[0] = 1.0;
               }
            else
               bNewTimeRange = TRUE;
               
            if (!YRANGE[0])
               {
               bNewDataRange = FALSE;
               YRANGE[0] = 1.0;
               }
            else
               bNewDataRange = TRUE;
               
            if (!ZRANGE[0] && !ZRANGE[1])
               {
               bNewComplexRange = FALSE;
               ZRANGE[0] = 1.0;
               }
            else
               bNewComplexRange = TRUE;

            multiplyValues(&FileHeader, INSTR, OUTSTR);
            if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);
            getDataScales(&FileHeader, OUTSTR, &TMIN, &TMAX, &YMIN, &YMAX);

            if (bNewTimeRange) zTaskMessage(3,"New TRANGE = %lG, %lG\n",TMIN,TMAX);

            if ((bNewDataRange    && ((FileHeader.type == R_Data) || (FileHeader.type== TR_Data))) ||
                (bNewComplexRange && ((FileHeader.type == X_Data) || (FileHeader.type== TX_Data))))
               zTaskMessage(3,"New YRANGE = %lG, %lG\n",YMIN,YMAX);
            break;
          case 3: // divide by...
            if (!TRANGE[0])
               {
               bNewTimeRange = FALSE;
               TRANGE[0] = 1.0;
               }
            else
               bNewTimeRange = TRUE;
               
            if (!YRANGE[0])
               {
               bNewDataRange = FALSE;
               YRANGE[0] = 1.0;
               }
            else
               bNewDataRange = TRUE;
               
            if (!ZRANGE[0] && !ZRANGE[1])
               {
               bNewComplexRange = FALSE;
               ZRANGE[0] = 1.0;
               }
            else
               bNewComplexRange = TRUE;

            divideValues(&FileHeader, INSTR, OUTSTR);
            if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);
            getDataScales(&FileHeader, OUTSTR, &TMIN, &TMAX, &YMIN, &YMAX);

            if (bNewTimeRange) zTaskMessage(3,"New TRANGE = %lG, %lG\n",TMIN,TMAX);

            if ((bNewDataRange    && ((FileHeader.type == R_Data) || (FileHeader.type== TR_Data))) ||
                (bNewComplexRange && ((FileHeader.type == X_Data) || (FileHeader.type== TX_Data))))
               zTaskMessage(3,"New YRANGE = %lG, %lG\n",YMIN,YMAX);
            break;
         case 4: // divide into...
            if (!TRANGE[0])
               bNewTimeRange = FALSE;
            else
               bNewTimeRange = TRUE;
               
            if (!YRANGE[0])
               bNewDataRange = FALSE;
            else
               bNewDataRange = TRUE;
               
            if (!ZRANGE[0] && !ZRANGE[1])
               bNewComplexRange = FALSE;
            else
               bNewComplexRange = TRUE;

            if (!TMIN || !TMAX)
               {
               zTaskMessage(10,"Invalid min/max for this operation. Cannot divide by zero min=%lG, max=%lG\n",TMIN,TMAX);
               BombOff(1);
               }
            else
               {
               divideintoValues(&FileHeader, INSTR, OUTSTR);
               if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);
               getDataScales(&FileHeader, OUTSTR, &TMIN, &TMAX, &YMIN, &YMAX);

               if (TRANGE[0]) zTaskMessage(3,"New TRANGE = %lG, %lG\n",TMIN,TMAX);

               if ((YRANGE[0] && ((FileHeader.type == R_Data) || (FileHeader.type== TR_Data))) ||
                   ((ZRANGE[0] || ZRANGE[1]) && ((FileHeader.type == X_Data) || (FileHeader.type== TX_Data))))
                  zTaskMessage(3,"New YRANGE = %lG, %lG\n",YMIN,YMAX);
               }
            break;
         case 5: // Set start time to zero
               zeroStartTime(&FileHeader, INSTR, OUTSTR, TMIN);
               if (Zputhead(OUTSTR,&FileHeader)) BombOff(1);
               getDataScales(&FileHeader, OUTSTR, &TMIN, &TMAX, &YMIN, &YMAX);
               zTaskMessage(3,"New TRANGE = %lG, %lG\n",TMIN,TMAX);
            break;
         }

      if (ferror(INSTR)) BombOff(1);

      fcloseall();

      ERRFLAG = zNameOutputFile(OUTFILE,TMPFILE);
      } // for (iwc = 0, pWild = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

double getDataScales(struct FILEHDR *pHeader, FILE *stream, double *pTmin, double *pTmax, double *pRmin, double *pRmax)
   {
   char buffer[sizeof(struct TXData)]; /* Largest Data Type Processed */
   double timeIndex = 0.0;
   double time;
   double Rval;
   struct complex Zval;
   short flag;
   BOOL bFirstPass = TRUE;
   double TMAX, TMIN, YMAX, YMIN;

   if (!Zgethead(stream,(struct FILEHDR *)NIL)) BombOff(1); /* back to the start of the file */

   while (Zread(stream,buffer,pHeader->type))
      {
      if (extractValues(buffer, pHeader, timeIndex, &time, &Rval, &Zval, &flag))
         {
         zTaskMessage(10,"Invalid File Type.\n");
         BombOff(1);
         }
      
      if (!flag)
         {
         if (bFirstPass)
            {
            bFirstPass = FALSE;
            
            YMIN = YMAX = Rval;
            TMIN = TMAX = time;
            }
         else
            {
            YMAX = Max(YMAX,Rval);
            YMIN = Min(YMIN,Rval);
            TMAX = Max(TMAX,time);
            TMIN = Min(TMIN,time);
            }
         }

      ++timeIndex;
      } /* End WHILE */

   if (!Zgethead(stream,(struct FILEHDR *)NULL)) BombOff(1); /* back to the start of the file */

   *pTmin = TMIN;
   *pTmax = TMAX;
   *pRmin = YMIN;
   *pRmax = YMAX;

   return(timeIndex);
   }

/******************************************************************************************************************************************
**
** Scaling values requires a linear mapping of the old min/max into a new min/max
**
** NEWMIN = OLDMIN * m + b
** NEWMAX = OLDMAX * m + b
**
** Based on the general NEW = OLD * m + b
**
** Therefore m = (NEWMAX - NEWMIN)/(OLDMAX - OLDMIN)
**           b = NEWMIN - OLDMIN * m
**
** If TRANGE[0] >= TRANGE[1] then don't change the time range
** If YRANGE[0] >= YRANGE[1] then don't change the amplitude value
*/
void scaleValues(struct FILEHDR *pHeader, FILE *infileStream, FILE *outfileStream, double Tmin, double Tmax, double Rmin, double Rmax)
   {
   char buffer[sizeof(struct TXData)]; /* Largest Data Type Processed */
   double mTime, bTime, mRval, bRval;
   double timeIndex = 0.0;
   double time, Rval;
   struct complex Zval;
   short flag;
   double newTime, newRval;
   struct complex newZval;
   
/*
** Set time scaling factors
*/
   if (TRANGE[1] <= TRANGE[0])
      {
      mTime = 1.0;
      bTime = Tmin;
      }
   else
      {
      if (Tmin == Tmax)
         {
         zTaskMessage(10,"Invalid time range in data file to perform scaling %lG, %lG.\n", Tmin, Tmax);
         BombOff(1);
         }
         
      mTime = (TRANGE[1] - TRANGE[0])/(Tmax - Tmin);
      bTime = TRANGE[0] - Tmin * mTime;
      }

/*
** Set Y scaling factors
*/
   if (YRANGE[1] <= YRANGE[0])
      {
      mRval = 1.0;
      bRval = Rmin;
      }
   else
      {
      if (Rmin == Rmax)
         {
         zTaskMessage(10,"Invalid amplitude range in data file to perform scaling %lG, %lG.\n", Rmin, Rmax);
         BombOff(1);
         }
         
      mRval = (YRANGE[1] - YRANGE[0])/(Rmax - Rmin);
      bRval = YRANGE[0] - Rmin * mRval;
      }

   while (Zread(infileStream,buffer,pHeader->type))
      {
      if (extractValues(buffer, pHeader, timeIndex, &time, &Rval, &Zval, &flag))
         {
         zTaskMessage(10,"Unknown File Type.\n");
         BombOff(1);
         }
      
      if (!flag)
         {
         newTime = time * mTime + bTime;
         newRval = Rval * mRval + bRval;

         if (Rval)
            {
            newZval.x = Zval.x * (newRval / Rval);
            newZval.y = Zval.y * (newRval / Rval);
            }

         insertValues(buffer, pHeader, newTime, newRval, newZval, flag);
         }

      if (Zwrite(outfileStream, buffer, pHeader->type)) BombOff(1);
         
      ++timeIndex;
      } /* End WHILE */

   if (((pHeader->type == R_Data) || (pHeader->type == X_Data)) &&   // Time scaling for a time series files
       (TRANGE[0] < TRANGE[1]))
      {
      --timeIndex;   // Need a zero indexed count of the number of points

      if (timeIndex == 0.0)
         {
         zTaskMessage(10, "File has too few points to scale the time.\n");
         timeIndex = 1.0;
         }

      pHeader->b = TRANGE[0];
      pHeader->m = (TRANGE[1]-TRANGE[0])/timeIndex;   // timeIndex is now N - 1.
      }

   return;
   }


/******************************************************************************************
**
** Apply simple offsets
**
** This function is very general. It just does everything and lets extractValues and
** insertValues manage the specifics. Yes a lot of unneeded calculations are done
** if all we are doing is a time offset for a time series file.
*/
void offsetValues(struct FILEHDR *pHeader, FILE *infileStream, FILE *outfileStream)
   {
   char buffer[sizeof(struct TXData)]; /* Largest Data Type Processed */
   double timeIndex = 0.0;
   double time;
   double Rval;
   struct complex Zval;
   short flag;
   
   while (Zread(infileStream,buffer,pHeader->type))
      {
      if (extractValues(buffer, pHeader, timeIndex, &time, &Rval, &Zval, &flag))
         {
         zTaskMessage(10,"Invalid File Type.\n");
         BombOff(1);
         }

      if (!flag)
         {
         time += TRANGE[0];

         Rval += YRANGE[0];

         Zval.x += ZRANGE[0];
         Zval.y += ZRANGE[1];

         insertValues(buffer, pHeader, time, Rval, Zval, flag);
         }

      if (Zwrite(outfileStream, buffer, pHeader->type)) BombOff(1);
         
      ++timeIndex;
      } /* End WHILE */

   if ((pHeader->type == R_Data) || (pHeader->type == X_Data)) pHeader->b += TRANGE[0];   // Time offset for a time series file

   return;
   }


/******************************************************************************************
**
** Apply a multiplicative factor
**
** This function is very general just like the offset function.
*/
void multiplyValues(struct FILEHDR *pHeader, FILE *infileStream, FILE *outfileStream)
   {
   char buffer[sizeof(struct TXData)]; /* Largest Data Type Processed */
   double timeIndex = 0.0;
   double time;
   double Rval;
   struct complex Zval;
   short flag;
   struct complex zMultiplier;

   zMultiplier = cmplx(ZRANGE[0], ZRANGE[1]);
   
   while (Zread(infileStream,buffer,pHeader->type))
      {
      if (extractValues(buffer, pHeader, timeIndex, &time, &Rval, &Zval, &flag))
         {
         zTaskMessage(10,"Invalid File Type.\n");
         BombOff(1);
         }
      
      if (!flag)
         {
         time *= TRANGE[0];

         Rval *= YRANGE[0];

         Zval = c_mul(Zval, zMultiplier);

         insertValues(buffer, pHeader, time, Rval, Zval, flag);
         }

      if (Zwrite(outfileStream, buffer, pHeader->type)) BombOff(1);
         
      ++timeIndex;
      } /* End WHILE */

   if (TRANGE[0] && ((pHeader->type == R_Data) || (pHeader->type == X_Data)))
      {
      pHeader->m *= TRANGE[0];   // Time series files
      pHeader->b *= TRANGE[0];   // Time series files
      }

   return;
   }

/******************************************************************************************
**
** Apply a division factor (data or time divided by a constant)
**
** This function is very general just like the offset function.
*/
void divideValues(struct FILEHDR *pHeader, FILE *infileStream, FILE *outfileStream)
   {
   char buffer[sizeof(struct TXData)]; /* Largest Data Type Processed */
   double timeIndex = 0.0;
   double time;
   double Rval;
   struct complex Zval;
   short flag;
   struct complex zDivisor;
   BOOL bNotZero;

   zDivisor = cmplx(ZRANGE[0], ZRANGE[1]);
   bNotZero = c_abs(zDivisor) ? TRUE : FALSE;
   
   while (Zread(infileStream,buffer,pHeader->type))
      {
      if (extractValues(buffer, pHeader, timeIndex, &time, &Rval, &Zval, &flag))
         {
         zTaskMessage(10,"Invalid File Type.\n");
         BombOff(1);
         }
      
      if (!flag)
         {
         if (TRANGE[0]) time /= TRANGE[0];

         if (YRANGE[0]) Rval /= YRANGE[0];

         if (bNotZero) Zval = c_div(Zval, zDivisor);

         insertValues(buffer, pHeader, time, Rval, Zval, flag);
         }

      if (Zwrite(outfileStream, buffer, pHeader->type)) BombOff(1);
         
      ++timeIndex;
      } /* End WHILE */

   if (TRANGE[0] && ((pHeader->type == R_Data) || (pHeader->type == X_Data)))
      {
      pHeader->m /= TRANGE[0];   // Time series files
      pHeader->b /= TRANGE[0];   // Time series files
      }

   return;
   }


/******************************************************************************************
**
** Divide into a divided factor
**
** This function is very general just like the offset function.
*/
void divideintoValues(struct FILEHDR *pHeader, FILE *infileStream, FILE *outfileStream)
   {
   char buffer[sizeof(struct TXData)]; /* Largest Data Type Processed */
   double timeIndex = 0.0;
   double time;
   double Rval;
   struct complex Zval;
   short flag;
   struct complex zDivided;
   double zDividedMagnitude;
   struct FILEHDR newFileHeader;
   
   memcpy(&newFileHeader, pHeader, sizeof(struct FILEHDR));

/*
** When inverting the time values for a time series, you cannot do so with a simple
** linear transformation of the original times series scaling parameters (m and b)
** so times series files need to be converted into time labeled files in order to manage the
** non-linear mapping of the old times to the new one.
*/
   if (pHeader->type == R_Data)
      newFileHeader.type = TR_Data;
   else if (pHeader->type == X_Data)
      newFileHeader.type = TX_Data;

   if (newFileHeader.type != pHeader->type) zTaskMessage(5,"Converting times series to time labeled file.\n");
   
   zDivided = cmplx(ZRANGE[0], ZRANGE[1]);
   zDividedMagnitude = c_abs(zDivided);
   
   while (Zread(infileStream,buffer,pHeader->type))
      {
      if (extractValues(buffer, pHeader, timeIndex, &time, &Rval, &Zval, &flag))
         {
         zTaskMessage(10,"Invalid File Type.\n");
         BombOff(1);
         }
      
      if (!flag)
         {
         if (TRANGE[0] && time)
            time = TRANGE[0] / time;

         if (YRANGE[0] && Rval)
            Rval = YRANGE[0] / Rval;

         if (zDividedMagnitude && c_abs(Zval)) Zval = c_div(zDivided, Zval);

         insertValues(buffer, &newFileHeader, time, Rval, Zval, flag);
         }

      if (Zwrite(outfileStream, buffer, newFileHeader.type)) BombOff(1);
         
      ++timeIndex;
      } /* End WHILE */

   pHeader->type = newFileHeader.type; // needed by the calling routine when it updates the output file header

   return;
   }

void zeroStartTime(struct FILEHDR *pHeader, FILE *infileStream, FILE *outfileStream, double Tmin)
   {
   char buffer[sizeof(struct TXData)]; /* Largest Data Type Processed */
   double timeIndex = 0.0;
   double time;
   double Rval;
   struct complex Zval;
   short flag;

   if ((pHeader->type == R_Data) || (pHeader->type == X_Data)) // Save some time on this one....
      {
      pHeader->b -= Tmin;
      }
   else
      {
      while (Zread(infileStream,buffer,pHeader->type))
         {
         if (extractValues(buffer, pHeader, timeIndex, &time, &Rval, &Zval, &flag))
            {
            zTaskMessage(10,"Invalid File Type.\n");
            BombOff(1);
            }
         
         time -= Tmin;

         insertValues(buffer, pHeader, time, Rval, Zval, flag);

         if (Zwrite(outfileStream, buffer, pHeader->type)) BombOff(1);
            
         ++timeIndex;
         } /* End WHILE */
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
   unlink(TMPFILE);
   ZCatFiles((char*)NIL); // free catalog memory
   Zexit(a);
   }

void fcloseall(void) // decalred in dos.h
   {
   if (INSTR) Zclose(INSTR);
   INSTR = (FILE *)NIL;
   
   if (OUTSTR) Zclose(OUTSTR);
   OUTSTR = (FILE *)NIL;

   return;
   }
