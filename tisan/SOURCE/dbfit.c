/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/15/2017
*
*  Task DBFIT
*
* Task to do a least squares fit using steepest descent
* The entire file must fit into memory in order to be processed.
* This is a nonstandard task in that it reads the data records into a "point" array.
* The point data type is structurally the same as the TRData structure, but the
* least squares routine was written independently of the TISAN framework and was
* ported in with a minimum of modifications and it uses the point data type.
*
* PARMS[] - The initial parameters for the fit (q[] values)
* ITYPE   - The type of fit (add more functions as needed, see setFunctionPointer)
* CODE    - What the task does on completing the fit
* FACTOR  - Order for polynomial fits
* TRANGE and YRANGE are the data window in which to perform the fit (default is all the data)
*
* If an output file is specified that is different from the input file, and CODE is set to create
* an output file, then a file is generated using the domain of the input file and the functional
* fit for the range values. The output file is of type TRData (x-y data pairs).
*
* CODE
*   1 -> Update PARMS[] to the new q[] values and save to the INPUTS file
*   2 -> Make an output file if the name is different from the input file
*   3 -> Do both CODE 1 and CODE 2
*
*   Default -> Just report the results.
*
*  POINT   - LOOPMAX,        MINERROR
*  ZFACTOR - LAMBDASTART,    MINLAMBDA
*  TMAJOR -  LAMBDADECREASE, LAMBDAINCREASE
*  YMAJOR -  DQVALUE
*
* ITYPE:
* Function Selection
*    0 -> Polynomial of order FACTOR
*    1 -> Gaussian
*    2 -> Double Gaussian
*    3 -> Exponential
*    4 -> Sine Function
*
* Special case for a linear fit which has a closed form solution:
* deviation (D) = n ∑(x^2) - (∑x)^2
* slope (m) = (n * ∑(x y) - ∑(x) ∑(y)) / D
* intercept (b) = (∑(x^2) ∑(y) - ∑(x) ∑(x y)) / D
*
* Special case for a quadratic fit (a x^2 + b x + c) which has a closed form solution: (added 01/03/2025)
* D = -n ∑(x^2) ∑(x^4) + n (∑x^3)^2 + ∑(x^2) ∑(x^4) - 2 ∑(x) ∑(x^2) ∑(x^3) + (∑x^2)^3
* a = (-n ∑(x^2) ∑(x^2 y) + n ∑(x^3) ∑(x y) + (∑(x))^2 ∑(x^2 y) - ∑(x) ∑(x^2) ∑(x y) - ∑(x) ∑(x^3) ∑(y) + (∑(x^2))^2 ∑(y)) / D
* b = (n ∑(x^2 y) ∑(x^3) - n ∑(x^4) ∑(x y) - ∑(x) ∑(x^2) ∑(x^2 y) + ∑(x) ∑(x^4) ∑(y) + (∑(x^2))^2 ∑(x y) - ∑(x^2) ∑(x^3) ∑(y)) / D
* c = (-∑(x) ∑(x^2 y) ∑(x^3) + ∑(x) ∑(x^4) ∑(x y) + (∑(x^2))^2 ∑(x^2 y) - ∑(x^2) ∑(x^3) ∑(x y) - ∑(x^2) ∑(x^4) ∑(y) + (∑(x^3))^2 ∑(y)) / D
*
*
* In general the fit is performed through steepest descent:
* E = ½∑(Y[t] - f(q[];t))²
* ∆q[i] = -λ ∂E/∂q[i]
* ∂E/∂q[i] = -∑(Y[t] - f(q[];t)) ∂f(q[];t)/∂q[i]
* ∂f(q[];t)/∂q[i] = (f(q[], q[i] + δ q[i];t) - f(q[], q[i] - δ q[i];t))/ (2 δ q[i])
*
* If the dynamic range of the values in the fit is too large, it will not converge. In that case you will need to perform some scaling.
*
* The infile of this task accepts wild cards.
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

#define LOOPMAXDEFAULT        100000L   // Maximum number of iterations before we give up trying
#define MINERRORDEFAULT       0.0       // Stop if the error function is less than or equal to this value
#define LAMBDASTARTDEFAULT    0.001     // Need to start somewhere
#define MINLAMBDADEFAULT      0.0       // Stop if the learning factor is less than or equal to this value

#define LAMBDADECREASEDEFAULT 1.001     // Decrease the learning factor if we go up hill
#define LAMBDAINCREASEDEFAULT 1.0001    // Increase when we go down hill
#define DQVALUEDEFAULT        1.0e-6    // How big is delta-q for taking the derivatives

#define MAXPOLY 9 // maximum polynomial fit power

struct configOptions {long   loopMax;         // A set of configuration options used by least squares
                      double minError;
                      double minLambda;
                      double lambdaDecrease;  // must be >= 1.0
                      double lambdaIncrease;  // must be >= 1.0
                      double lambdaStart;
                      double dqValue;};

typedef struct pointStruct {double x,y;} point;    // Same as TRData structure, but this type is compatible with the fit routines.

double (*function)(double, double*);    // Global Pointer to the function that will be used for the fit.

void reportResults(int qCount, int iEndCode);
void writeOutputFile(struct TRData* data, long lRecordCount);
long readDataPoints(FILE* infileStream, point* pDataBuffer, long lDataCount);
point* getXYdata(char* data, long index, short type);
int setFunctionPointer(struct configOptions* options);
int findLinearFit(point data[], long dataCount);
int findQuadFit(point data[], long dataCount);
int findFit(point data[], long dataCount, int qCount, struct configOptions *options);
double errorFunction(point data[], long dataCount, double q[]);
double rmsError(double error, long dataCount);
double partialDerivative(double x, int qIndex, double q[], double dq);
BOOL calculateNewParameters(point data[], long dataCount, double q[], double lambda, int qCount, double dq, struct configOptions *options);

double polynomial(double x, double q[]);
double gaussian(double x, double q[]);
double doubleGaussian(double x, double q[]);
double exponential(double x, double q[]);
double sineFunction(double x, double q[]);

int iFactor;   // Order of a polynomial fit

point* pDataBuffer = (point *)NIL;     // Global data buffer. Needs to be global in case we have to bail and free it.
struct FILEHDR FileHeader;

char tempfileName[_MAX_PATH];

FILE *infileStream  = (FILE *)NIL;
FILE *outfileStream = (FILE *)NIL;

const char szTask[]="DBFIT";

int main(int argc, char *argv[])
   {
   char infileName[_MAX_PATH], outfileName[_MAX_PATH];
   long lDataCount, lValidRecords;
   int qCount, iEndCode;
   struct configOptions options;
   BOOL bCreateOutputFile;
   // declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;
   
   signal(SIGINT,BREAKREQ);
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);

   if (isEmptyString(OUTCLASS)) strcpy(OUTCLASS,"dbfit");

   zBuildFileName(M_inname,infileName);
   CatList = ZCatFiles(infileName);   // Find Matching Files for the input file specification
   if (CatList->N == 0) Zexit(1);     // Quit if there are none

/*
** Fit as function using steepest descent and set up the options for the fit
*/
   qCount = setFunctionPointer(&options);   // Also populates the options structure
/*
** Just a little feedback of we are using steepest descent to find the fit
*/
   if ((ITYPE != 0) || ((iFactor != 1) && (iFactor != 2))) // don't need this output for linear and quadratic fits
      {
      zTaskMessage(4,"Maximum number of iterations: %ld\n",options.loopMax);
      zTaskMessage(4,"Minimum error for fitting: %lg\n",options.minError);
      zTaskMessage(4,"Minimum learning factor: %lg\n",options.minLambda);
      zTaskMessage(4,"Initial learning factor: %lg\n",options.lambdaStart);
      zTaskMessage(4,"Learning factor decreased by: %lg\n",options.lambdaDecrease);
      zTaskMessage(4,"Learning factor increased by: %lg\n",options.lambdaIncrease);
      zTaskMessage(4,"Differential: %lg\n",options.dqValue);
      zTaskMessage(4,"\n");
      }

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
      if ((infileStream = zOpen(infileName,O_readb)) == NULL) Zexit(1);
      
      if (!Zgethead(infileStream,&FileHeader)) BombOff(1);

      if ((FileHeader.type != R_Data) && (FileHeader.type != TR_Data))
         {
         zTaskMessage(5,"Only real data files can be fit at this time.\n");
         BombOff(1);
         }

      if ((CODE == 2) || (CODE == 3))  // Try and create an output file
         {
         if (!strcmp(infileName,outfileName))  // However don't overwrite the input file
            {
            bCreateOutputFile = FALSE;
            zTaskMessage(5,"Output filename the same as input file. No output file will be created.\n");
            }
         else
            {
            zTaskMessage(2,"Opening Scratch File '%s'\n",tempfileName);
            if ((outfileStream = zOpen(tempfileName,O_writeb)) == NULL) BombOff(1);
            bCreateOutputFile = TRUE;
            }
         }
      else
         bCreateOutputFile = FALSE;

   /*
   ** Read the input file data into memory
   */
      lDataCount = countDataRecords(infileStream);

      if (ferror(infileStream))
         {
         zTaskMessage(10,"Error getting number of data records.\n");
         zError();
         BombOff(1);
         }

      pDataBuffer = malloc(lDataCount * sizeof(point));   // Allocate enough space for all the possible X-Y data pairs

      if (!pDataBuffer)
         {
         zTaskMessage(10,"Memory allocation of %ld bytes failed.\n", lDataCount * sizeof (point));
         BombOff(1);
         }
      
      lValidRecords = readDataPoints(infileStream, pDataBuffer, lDataCount);      // Read TISAN records into the X-Y data point array buffer

      if (ferror(infileStream))
         {
         zTaskMessage(10,"Error reading file data.\n");
         zError();
         BombOff(1);
         }

      zTaskMessage(3,"Fitting to %ld Records\n",lValidRecords);

      Zclose(infileStream);
      infileStream = (FILE *)NIL;

      if ((ITYPE == 0) && (iFactor == 1))      // Special processing for a simple linear fit
         iEndCode = findLinearFit(pDataBuffer, lValidRecords);
      else if ((ITYPE == 0) && (iFactor == 2)) // Special processing for a not-as-simple quadratic fit
         iEndCode = findQuadFit(pDataBuffer, lValidRecords);
      else
         iEndCode = findFit(pDataBuffer, lValidRecords, qCount, &options);

      reportResults(qCount, iEndCode);
      
      if (bCreateOutputFile)
         {
         writeOutputFile((struct TRData*)pDataBuffer, lDataCount); // The point and TRData structures are actually the same
         ERRFLAG = zNameOutputFile(outfileName,tempfileName);
         }
         
      fcloseall();
      } // for (iwc = 0, pWild = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

/******************************************************************
**
** Report the results of the run: Why it stopped and the q[] values
*/
void reportResults(int qCount, int iEndCode)
   {
   int i;
   
   if ((ITYPE != 0) || ((iFactor != 1) && (iFactor != 2))) // don't need this output for linear and quadratic fits
      {
      switch (iEndCode)
         {
         case 1:
            zTaskMessage(3,"Maximum number of iterations reached.\n");
            break;
         case 2:
            zTaskMessage(3,"Learning factor below minimum value.\n");
            break;
         case 3:
            zTaskMessage(3,"Error function below minimum value.\n");
            break;
         }
      } // if ((ITYPE != 0) || ((iFactor != 1) && (iFactor != 2)))

   zTaskMessage(3,"\n");

   for (i = 0; i < qCount; ++i)
      {
      zTaskMessage(5, "q[%d] = %lG\n", i, PARMS[i]);
      }
   zTaskMessage(5, "\n");

   if ((CODE == 1) || (CODE == 3))
      {
      if (zPutAdverbs(TASKNAME)) zTaskMessage(9, "Unable to update inputs file for task '%s'\n", TASKNAME);
      }

   return;
   } // main

/*****************************************************
** Write data to the output file
**
** We are going to replace the input data set in the point array with
** a calculated data set and then write it out. This function thus
** destroys the original data in memory (oh well).
*/
void writeOutputFile(struct TRData* data, long lRecordCount)
   {
   long i;
   
   FileHeader.type = TR_Data;
   if (Zputhead(outfileStream,&FileHeader)) BombOff(1);

   for (i = 0L; i < lRecordCount; ++i) data[i].y = function(data[i].t, PARMS);   // Replace the input data with the fit. "function" is a pointer to the chosen fit function.

   fwrite(data, sizeof(struct TRData), lRecordCount, outfileStream); // Write out the fit

   if (ferror(outfileStream))
      {
      zTaskMessage(10,"Error writing file data.\n");
      zError();
      BombOff(1);
      }

   return;
   }

/************************************************
**
** Read the data file in as X-Y pairs
** Returns the number of data points actually read into the buffer.
**
*/
long readDataPoints(FILE* infileStream, point* pDataBuffer, long lDataCount)
   {
   long i, index = 0L;
   char BUFFER[sizeof(struct TXData)]; // Largest data size (just in case) for a single data record
   point* pp;
   
   for (i = 0L; i < lDataCount; ++i)                           // Loop through all the records in the TISAN file
      {
      zGetData(1L, infileStream, BUFFER, FileHeader.type);     // Read in a single record

      pp = getXYdata(BUFFER, i, FileHeader.type);              // Extract X-Y data pair from the TISAN record
      
      if (pp)        // Pointer is valid so we might keep this point
         {
         if ((TRANGE[0] >= TRANGE[1]) || ((pp->x >= TRANGE[0] && (pp->x <= TRANGE[1])))) // Data are within the time domain
            {
            if ((YRANGE[0] >= YRANGE[1]) || ((pp->y >= YRANGE[0] && (pp->y <= YRANGE[1])))) // Data are within the Y range
               {
               pDataBuffer[index].x = pp->x;
               pDataBuffer[index].y = pp->y;
               ++index;
               }
            }
         }
      }
      
   return(index);
   }

/************************************************
**
** Function to get an X-Y data point from the
** the data buffer given the index of the requested data value.
** Return a pointer to a point structure. If the pointer is null then
** the requested index points to a data value flagged as bad and should not be used.
*/
point* getXYdata(char* data, long index, short type)
   {
   static point p;               // Need static so the return pointer will remain valid after we exit this function
   point* pPointer = (point*)NIL;
   struct RData *RDataPntr;
   struct TRData *TRDataPntr;
 
   switch (type)
      {
      case TR_Data:
         TRDataPntr  = (struct TRData *)data;
         p.x = TRDataPntr->t;
         p.y = TRDataPntr->y;
         pPointer = &p;
         break;
      case R_Data:
         RDataPntr = (struct RData *)data;
         if (!RDataPntr->f)
            {
            p.x = (double)index * FileHeader.m + FileHeader.b;
            p.y = RDataPntr->y;
            pPointer = &p;
            }
         break;
      default:
         zTaskMessage(10,"Unexpected data type in function getXYdata.\n");
         BombOff(1);
         break;
      }

   return(pPointer);
   }

/************************************************
**
** Set the function pointer based on the ITYPE value
** Add new functions by expanding the case statement.
** Be sure to update the help text if you add
** new functions.
**
*/
int setFunctionPointer(struct configOptions* options)
   {
   int qCount = 0;

/*
* The options control how the least squares routine runs and ends. These are the default values.
*/

   options->loopMax        = LOOPMAXDEFAULT;            // Maximum number of iterations before we give up trying
   options->minError       = MINERRORDEFAULT;           // Stop if the error function is less than or equal to this value
   options->lambdaStart    = LAMBDASTARTDEFAULT;        // Need to start somewhere
   options->minLambda      = MINLAMBDADEFAULT;          // Stop if the learning factor is less than or equal to this value

   options->lambdaDecrease = LAMBDADECREASEDEFAULT;     // Decrease the learning factor by two if we go up hill
   options->lambdaIncrease = LAMBDAINCREASEDEFAULT;     // Increase by 1.5 wqhen we go down hill
   options->dqValue        = DQVALUEDEFAULT;            // How big is delta-q for taking the derivatives
   
/*
**  POINT   - LOOPMAX, MINERROR
**  ZFACTOR - LAMBDASTART, MINLAMBDA
**  TMAJOR -  LAMBDADECREASE, LAMBDAINCREASE
**  YMAJOR -  DQVALUE
*/
   if (POINT[0] > 0.0) options->loopMax  = POINT[0];
   if (POINT[1] > 0.0) options->minError = POINT[1];

   if (ZFACTOR[0] > 0.0) options->lambdaStart = ZFACTOR[0];
   if (ZFACTOR[1] > 0.0) options->minLambda   = ZFACTOR[1];

   if (TMAJOR[0] >= 1.0) options->lambdaDecrease = TMAJOR[0];
   if (TMAJOR[1] >= 1.0) options->lambdaIncrease = TMAJOR[1];

   if (YMAJOR[0] > 0.0) options->dqValue = YMAJOR[0];

   switch (ITYPE)
      {
      case 0: // Polynomial of order FACTOR
         iFactor = (int)FACTOR;                  // Order of the polynomial (Global)
         if ((iFactor < 1) || (iFactor > MAXPOLY))
            {
            zTaskMessage(10,"Invalid integer FACTOR value of %d.\n", iFactor);
            BombOff(1);
            }
         qCount = iFactor + 1;                              // Number of parameters for the polynomial is one more than the order
         function = polynomial;
         zTaskMessage(3,"Fitting Polynomial of Order %d\n",iFactor);
         break;
      case 1: // Gaussian
         qCount = 4;
         function = gaussian;
         zTaskMessage(3,"Fitting Gaussian with %d Parameters\n",qCount);
         break;
      case 2: // Double Gaussian
         qCount = 7;
         function = doubleGaussian;
         zTaskMessage(3,"Fitting Double Gaussian with %d Parameters\n",qCount);
         break;
      case 3: // Exponential
         qCount = 3;
         function = exponential;
         zTaskMessage(3,"Fitting Exponential with %d Parameters\n",qCount);
         break;
      case 4: // A Sin(w t + phi) + B
         qCount = 4;
         function = sineFunction;
         zTaskMessage(3,"Fitting Sine Function with %d Parameters\n",qCount);
         break;
      default:
         zTaskMessage(10,"Invalid ITYPE value of %d.\n", ITYPE);
         BombOff(1);
         break;
      }

   return(qCount);
   }

/***********************************************
* Do a closed form solution for a line
* y = m x + b where m = q[0] and b = q[1]
*
* deviation (D) = N sum(x^2) - sum(x)^2
* slope (m) = (N * sum(x * y) - sum(x) * sum(y)) / D
* intercept (b) = (sum(x^2) * sum(y) - sum(x) * sum(x * y)) / D
*
*/
int findLinearFit(point data[], long dataCount)
   {
   double sumx = 0.0, sumy = 0.0, sumxx =0.0, sumxy = 0.0;
   double N, D, m, b, error;
   long i;

   error = errorFunction(data, dataCount, PARMS);
   zTaskMessage(3, "Initial RMS error = %lG\n", rmsError(error, dataCount));
   
   N = (double)dataCount;
   
   for (i = 0L; i < dataCount; ++i)
      {
      sumx  += data[i].x;
      sumy  += data[i].y;
      sumxy += data[i].x * data[i].y;
      sumxx += data[i].x * data[i].x;
      }
      
   D = N * sumxx - sumx * sumx;
   
   if (D != 0.0)
      {
      m = (N * sumxy - sumx * sumy) / D;
      b = (sumxx * sumy - sumx * sumxy) / D;
      
      PARMS[0] = m;
      PARMS[1] = b;

      error = errorFunction(data, dataCount, PARMS);
      zTaskMessage(3, "Final RMS error = %lG\n", rmsError(error, dataCount));
      }
   else
      {
      zTaskMessage(10, "Deviation is zero! No linear fit is possible.\n");
      BombOff(1);
      }
   
   return(0);
   }

/***********************************************
* Do a closed form solution for a quadratic
* y =  a x^2 + b x + c where a = q[0], b = q[1] c = q[2]
*
* D = -n ∑(x^2) ∑(x^4) + n (∑x^3)^2 + ∑(x^2) ∑(x^4) - 2 ∑(x) ∑(x^2) ∑(x^3) + (∑x^2)^3
* a = (-n ∑(x^2) ∑(x^2 y) + n ∑(x^3) ∑(x y) + (∑(x))^2 ∑(x^2 y) - ∑(x) ∑(x^2) ∑(x y) - ∑(x) ∑(x^3) ∑(y) + (∑(x^2))^2 ∑(y)) / D
* b = (n ∑(x^2 y) ∑(x^3) - n ∑(x^4) ∑(x y) - ∑(x) ∑(x^2) ∑(x^2 y) + ∑(x) ∑(x^4) ∑(y) + (∑(x^2))^2 ∑(x y) - ∑(x^2) ∑(x^3) ∑(y)) / D
* c = (-∑(x) ∑(x^2 y) ∑(x^3) + ∑(x) ∑(x^4) ∑(x y) + (∑(x^2))^2 ∑(x^2 y) - ∑(x^2) ∑(x^3) ∑(x y) - ∑(x^2) ∑(x^4) ∑(y) + (∑(x^3))^2 ∑(y)) / D
*
*/
int findQuadFit(point data[], long dataCount)
   {
   double sum_x4 = 0.0, sum_x3 = 0.0, sum_x2 = 0.0, sum_x = 0.0, sum_x2y = 0.0, sum_xy = 0.0, sum_y = 0.0;
   double N, D, a, b, c, error, v;
   long i;

   error = errorFunction(data, dataCount, PARMS);
   zTaskMessage(3, "Initial RMS error for Quadratic Fit = %lG\n", rmsError(error, dataCount));
   
   N = (double)dataCount;
   
   for (i = 0L; i < dataCount; ++i)
      {
      v = data[i].x;
      sum_x  += v;
      sum_xy += v * data[i].y;

      v *= data[i].x; // v is now x^2
      sum_x2  += v;
      sum_x2y += v * data[i].y;

      v *= data[i].x; // v is now x^3
      sum_x3 += v;

      v *= data[i].x; // v is now x^4
      sum_x4 += v;

      sum_y   += data[i].y;
      }

   D  = - N * sum_x2 * sum_x4
        + N * (sum_x3 * sum_x3)
        + (sum_x * sum_x) * sum_x4
        - 2 * sum_x * sum_x2 * sum_x3
        + (sum_x2 * sum_x2 * sum_x2);

   if (D != 0.0)
      {
      a = -N * sum_x2 * sum_x2y + N * sum_x3 * sum_xy + sum_x * sum_x * sum_x2y - sum_x * sum_x2 * sum_xy - sum_x * sum_x3 * sum_y + sum_x2 * sum_x2 * sum_y;
      a /= D;

      b = N * sum_x2y * sum_x3 - N * sum_x4 * sum_xy - sum_x * sum_x2 * sum_x2y + sum_x * sum_x4 * sum_y + sum_x2 * sum_x2 * sum_xy - sum_x2 * sum_x3 * sum_y;
      b /= D;

      c = -sum_x * sum_x2y * sum_x3 + sum_x * sum_x4 * sum_xy + sum_x2 * sum_x2 * sum_x2y - sum_x2 * sum_x3 * sum_xy - sum_x2 * sum_x4 * sum_y + sum_x3 * sum_x3 * sum_y;
      c /= D;
      
      PARMS[0] = a;
      PARMS[1] = b;
      PARMS[2] = c;

      error = errorFunction(data, dataCount, PARMS);
      zTaskMessage(3, "Final RMS error = %lG\n", rmsError(error, dataCount));
      }
   else
      {
      zTaskMessage(10, "Deviation is zero! No quadradic fit is possible.\n");
      BombOff(1);
      }
   
   return(0);
   }

/************************************************
**
** Function to find the fit. Return value depends on how the function ended
** 1 -> Reached the max loop count
** 2 -> Learning factor got too small
** 3 -> Error function got too small
**
*/
int findFit(point data[], long dataCount, int qCount, struct configOptions *options)
   {
   int returnCode = 0;
   int i = 0;
   BOOL worse;
   double q[PARMSCOUNT];
   double error, lambda;
   
   lambda = options->lambdaStart;
   
   for (i = 0; i < PARMSCOUNT; ++i) q[i] = PARMS[i];  // Need a local copy of the parameters since we will be tweaking them

   error = errorFunction(data, dataCount, q);
   zTaskMessage(3, "Initial RMS error = %lG\n", rmsError(error, dataCount));

   do {
      worse = calculateNewParameters(data, dataCount, q, lambda, qCount, options->dqValue, options);

      if ((options->lambdaDecrease != options->lambdaIncrease) || (options->lambdaDecrease != 1.0)) // If the factors are both 1.0 then ignore them
         {
         if (worse)
            lambda /= options->lambdaDecrease; // The error went up, so the q values have not been changed, reduce the learning factor
         else
            lambda *= options->lambdaIncrease; // Error was lower, we have new q values, so take bigger steps.
         }

      error = errorFunction(data, dataCount, q);
      
      if (++i >= options->loopMax)                              returnCode = 1; // We hit the max number of iterations
      else if (lambda <= options->minLambda)                    returnCode = 2; // The learning factor is too small to continue
      else if (rmsError(error, dataCount) <= options->minError) returnCode = 3; // Success! The RMS error is below the stated minimum
      }
   while (!returnCode);

   zTaskMessage(3, "Stopping after %ld iterations\n", i);
   zTaskMessage(3, "Final learning factor = %lG\n", lambda);
   zTaskMessage(3, "Final RMS error = %lG\n", rmsError(error, dataCount));

   for (i = 0; i < PARMSCOUNT; ++i) PARMS[i] = q[i];     // Need to make the results visible to the rest of the TASK

   return(returnCode);
   }

/*
** Calculates the error function according to the documentation E = 1/2 ∑ (y[i] - f(x[i],q[]))²
*/
double errorFunction(point data[], long dataCount, double q[])
   {
   long i;
   double error = 0.0;
   double v;
   
   for (i = 0L; i < dataCount; ++i)
      {
//      printf("DEBUG errorFunction:%d\t (%lG, %lG), %lG\n", i, data[i].x, data[i].y, function(data[i].x, q));

      v = data[i].y - function(data[i].x, q);
      error += (v * v);
      
      }
   
   error /= 2.0;
   
   return(error);
   }

/*
** Convert fit error to an RMS error
*/
double rmsError(double error, long dataCount)
   {
   return(sqrt(2.0 * error / (double)dataCount));
   }

/*
** Partial Derivative with respect to a parameter in the q[] array
**
** ∂f/∂q = (f(x, q + ∆q) - f(x, q - ∆q))/(2 ∆q)
**
** x is the current x-value for the function evaluation
** qIndex is the index of the parameter being tested
** q[] if the array of parameters
** dq is a scaling factor (less than 1) for determining the offset on either side of q for the derivative
**
*/
double partialDerivative(double x, int qIndex, double q[], double dq)
   {
   double dfdq;
   double deltaq;
   double originalq;
   
   originalq = q[qIndex];           // Need a copy of the current q value that is being tested
   deltaq = originalq * dq;         // Get the delta q based on the dq argument

/*
** The derivative is taken by getting a delta on either side of the current point,
** but the delta value is scaled based on the value of the current point. If that value is
** zero we have a problem because we divide by 2*deltaq so we force the value to be non-zero.
** We also make sure that deltaq is positive while we are at it.
*/

   if (deltaq == 0.0)
      deltaq = dq * dq;                           // Make deltaq non-zero (this works pretty well)
   else
      deltaq = (deltaq < 0.0) ? -deltaq : deltaq; // ensure deltaq is non-negative
   
   q[qIndex] = originalq + deltaq;  // Bump up the q value by a small amount
   dfdq = function(x,q);            // Get the value of the function

//   printf("DEBUG: partialDerivative-1: %lG,\t", dfdq);

   q[qIndex] = originalq - deltaq;  // Now go down a bit
   dfdq -= function(x,q);           // Get the difference

// printf("%lG,\t", dfdq);
   
   dfdq /= (2.0 * deltaq);          // And now we have a derivative

//printf("%lG,\n", dfdq);
   
   q[qIndex] = originalq;           // Restore the q[] array for the next derivative (if any)

   return(dfdq);
   }

/*
** Calculate and update new parameters
**
** E = ½∑(Y[t] - f(q[];t))²
**
** ∆q[i] = -λ ∂E/∂q[i]
**
** ∂E/∂q[i] = ∑(Y[t] - f(q[];t)) ∂f(q[];t)/∂q[i]
**
** ∂f(q[];t)/∂q[i] = (f(q[], q[i] + δ q[i];t) - f(q[], q[i] - δ q[i];t))/ (2 δ q[i])
**
** Return TRUE if the new fit is worse than before
** FALSE if the error function increases and do not modify the Q values (unless the lambda scaling factors are both 1)
*/
BOOL calculateNewParameters(point data[], long dataCount, double q[], double lambda, int qCount, double dq, struct configOptions *options)
   {
   double partialE;
   double newq[qCount];
   double oldError, newError;
   double pDer;
   long i;
   int j;
   BOOL worse = TRUE;

/*
** Get the current (old) value of the error function so we can test if things get worse
*/
   oldError = errorFunction(data, dataCount, q);

/*
** Take the partial derivative of the error function and find each delta q
*/
   for (j = 0; j < qCount; ++j)
      {
      partialE = 0.0;
      for (i = 0L; i < dataCount; ++i)
         {
         pDer = partialDerivative(data[i].x, j, q, dq);
         partialE += (data[i].y - function(data[i].x, q)) * pDer;   //  ∂E/∂q[i] = ∑(Y[t] - f(q[];t)) ∂f(q[];t)/∂q[i]

// printf("DEBUG: calculateNewParameters: %d, %d\t, %lG, %lG\n", j, i, pDer, partialE);
         }

      newq[j] = q[j] + lambda * partialE;
      }
/*
** Now get the new value of the error function. If it is larger than before then we are going up hill.
** The calling routine should reduce the size of the lambda factor and try again.
**
** If we went down hill, then update the q[] array with the new values
*/
   newError = errorFunction(data, dataCount, newq);

/*
** The inequality here MUST be <= and not just <
** If the error does not change then we are not going up hill. The space is just really really really flat, so we need to
** increase the learning factor to get out of it, otherwise the learning factor just drops to zero as we don't go anywhere.
*/
   if (newError <= oldError)
      {
      worse = FALSE;

      for (j = 0; j < qCount; ++j) q[j] = newq[j];
      }
   else if ((options->lambdaIncrease == 1.0) && (options->lambdaDecrease == 1.0)) // If the learning factor scales are equal to 1 then ignore the outcome and just go for it
      {
      for (j = 0; j < qCount; ++j) q[j] = newq[j];
      }

   return(worse);
   }

/************************************************
**
** ITYPE = 0
**
** Polynomial of order FACTOR (uses global iFactor)
** y = q[n] + q[n-1]x + q[n-2]x^2 + q[n-3]x^3 ...q[0]x^n
**
** xPower is used to get 1, x, x*x, x*x*x, etc...
*/
double polynomial(double x, double q[])
   {
   double y = 0.0;
   double xPower = 1.0;
   int i;
   
   /*
   ** Build up the polynomial one element at a time
   */
   for (i = 0; i <= iFactor; ++i)
      {
      y += xPower * q[iFactor - i];
      xPower *= x;
      }
   
   return(y);
   }

/************************************************
**
** ITYPE = 1
**
** Gaussian of the form y = q[0] exp((-(x - q[1])^2)*(q[2]^2)) + q[3]
**
** So PARMS[2] is the inverse of the deviation (avoids any division by zero issues)
*/
double gaussian(double x, double q[])
   {
   double y;
   double w;

   w = (x - q[1]) * q[2];

   y = q[0] * exp(-(w * w)) + q[3];

   return(y);
   }

/************************************************
**
** ITYPE = 2
**
** Double Gaussian of the form y = q[0] exp((-(x - q[1])^2)*(q[2]^2)) + q[3] exp((-(x - q[4])^2)*(q[5]^2)) + q[6]
**
** So PARMS[2] and [5] are the inverse of the variance (avoids any division by zero issues)
*/
double doubleGaussian(double x, double q[])
   {
   double y;
   double w;

   w = (x - q[1]) * q[2];
   y = q[0] * exp(-(w * w));
   
   w = (x - q[4]) * q[5];
   y += q[3] * exp(-(w * w));
   
   y += q[6];

   return(y);
   }

/************************************************
**
** ITYPE = 3
**
** Exponential of the form y = q[0] exp(-x * q[1]) + q[2]
**
*/
double exponential(double x, double q[])
   {
   double y;

   y = q[0] * exp(-x * q[1]) + q[2];

   return(y);
   }

/************************************************
**
** ITYPE = 4
**
** sine of the form y = q[0] sin(q[1] * x + q[2]) + q[3]
**
*/
double sineFunction(double x, double q[])
   {
   double y;

   y = q[0] * sin(q[1] * x + q[2]) + q[3];

   return(y);
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

   if (pDataBuffer) free(pDataBuffer);
   pDataBuffer = (point *)NIL;
   
   return;
   }
