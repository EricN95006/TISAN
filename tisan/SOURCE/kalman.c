/*********************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task KALMAN
*
* Task to evenly distribute time record data (2) which are not necessarily evenly spaced in to file type (1) which are evenly spaced,
* although type (1) data can be processed as well.
*
* This task will redistribute data in real data files and create an evenly spaced time series file using the bidirectional Kalman filter technique
* as defined by Jurkevich, I., William, W.W. and Petty, A.F. 1976, Astrophysics and Space Science, 44, p. 63.  Note that there are some errors in
* the derivations in this paper. The corrections were published by Nelson,E.R. 1985 in Cool Stars, Stellar Systems, and the Sun, 254, p. 287.
*
*
* FACTOR = Number of Points to Generate (Default = N)
* If CODE = 0 then POINT[0] is % error and POINT[1] is not used
* If CODE = 1 then POINT[0] is RMS error and POINT[1] is not used
* If CODE = 2 then POINT[0] is r and POINT[1] is q
*
* See the help file for more details on the above parameters
*
* Default output extension is kal
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

struct KALMANSTRUCT {double t;
                     double y;
                     int f;};

struct PREDICTSTRUCT {double t;
                      double x;
                      double p;
                      int f;};

int FINIT(void);
int BUILD(void);
int STRIP(void);
int PREDICT(void);

double Q, R=0., TB, TM;
double N=0.;
short ErrorFlag=0;
char InputFileName[_MAX_PATH], OutputFileName[_MAX_PATH], TempFileName[_MAX_PATH];
struct FILEHDR FileHeader;

FILE *InputStream  = (FILE *)NIL;
FILE *OutputStream = (FILE *)NIL;;

const char szTask[]="KALMAN";

int main(int argc, char *argv[])
   {
// declarations need to manage wild cards
   struct CATSTRUCT *CatList;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   short ERRFLAG = 0;
   int iwc;
   char *pWild;

   signal(SIGINT,BREAKREQ);
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1); /* Initialize Task */

   if ((CODE < 0) || (CODE > 2))
      {
      zTaskMessage(10,"CODE adverb out of range.\n");
      Zexit(1);
      }

   if ((CODE < 2) && (POINT[0] <= 0.))
      {
      zTaskMessage(10,"Error Factor POINT[1] Must be Specified.\n");
      Zexit(1);
      }

   if ((CODE == 2) && ((POINT[0] <= 0.) || (POINT[1] <= 0.)))
      {
      zTaskMessage(10,"Invalid r or q specification(s).\n");
      Zexit(1);
      }

   if (!OUTCLASS[0]) strcpy(OUTCLASS,"kal");

   zBuildFileName(M_inname,InputFileName);
   CatList = ZCatFiles(InputFileName);    // Find Matching Files for the input file specification
   if (CatList->N == 0) Zexit(1);         // Quit if there are none

   for (iwc = 0, pWild = CatList->pList;
        (iwc < CatList->N) && !ERRFLAG;
        ++iwc, pWild = strchr(pWild,'\0') + 1)
      {
      splitPath(pWild,Drive,Dir,Fname,Ext);
      strcpy(INNAME,Fname);
      strcpy(INCLASS,Ext);

      zBuildFileName(M_outname,OutputFileName);

      FINIT();

      if (!(ErrorFlag=BUILD()) &&
          !(ErrorFlag=STRIP())) (ErrorFlag=PREDICT());

      if (ErrorFlag >= 1) BombOff(1);

      fcloseall();

      ERRFLAG = zNameOutputFile(OutputFileName,TempFileName);
      } // for (iwc = 0, pWild = CatList->pList;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

/************************************************************
**
** Initialize Q, R
*/
int FINIT()
   {
   double PreviousData, PreviousTime;
   int FirstFlag = 0;
   char InputBuffer[sizeof(struct TRData)];
   struct TRData *pInputTRData;
   struct RData  *pInputRData;
   double Data, Time, TimeCount=0.;
   int Flag=0;
   double SumSqr=0.;


// Initialize these globals in case we have wildcards in the file name
   R = 0.;
   N = 0.;
   ErrorFlag = 0;

   pInputTRData = (struct TRData *)InputBuffer;
   pInputRData  = (struct  RData *)InputBuffer;

   zBuildFileName(M_inname,InputFileName);
   zTaskMessage(2,"Opening Input File '%s'\n",InputFileName);

   if (((InputStream = zOpen(InputFileName,O_readb)) == NULL) ||
        (!Zgethead(InputStream,&FileHeader))) BombOff(1);

   switch (FileHeader.type)
      {
      case TR_Data:
      case R_Data:
         break;
      default:
         zTaskMessage(10,"Not a Real Data File.\n");
         BombOff(1);
      }

   zBuildFileName(M_tmpname,TempFileName);
   zTaskMessage(2,"Opening Scratch File '%s'\n",TempFileName);

   if (!(OutputStream = zOpen(TempFileName,O_writeb))) BombOff(1);

   while (Zread(InputStream,InputBuffer,FileHeader.type))
      {
      if (Zwrite(OutputStream,InputBuffer,FileHeader.type)) return(1);
      switch (FileHeader.type)
         {
         case R_Data:
            Flag = pInputRData->f;
            Data = pInputRData->y;
            Time = FileHeader.m*TimeCount + FileHeader.b;
            ++TimeCount;
            break;
         case TR_Data:
            Time = pInputTRData->t;
            Data = pInputTRData->y;
            break;
         }

      if (!Flag)
         {
         SumSqr += (Data * Data);
         ++N;
         if (!FirstFlag)
            {
            TB = PreviousTime = Time;
            PreviousData = Data;
            Q = 0.;
            FirstFlag = 1;
            }
         else
            {
            if (Time != PreviousTime)
               Q = Max(Square(Data-PreviousData)/(Time-PreviousTime),Q);
            PreviousTime = Time;
            PreviousData = Data;
            }
         }
      }

   if (N < 2.)
      {
      zTaskMessage(10,"File Contains Only %ld Valid Point(s)!\n", (long)N);
      BombOff(1);
      }

   if (FACTOR < 2.) FACTOR = N;

   TM = (Time - TB)/(FACTOR - 1.);

   if (CODE < 2)
      R = (CODE) ? Square(POINT[0]) : Square(POINT[0]/100.) * SumSqr;
   else
      {
      R = POINT[0];
      Q = POINT[1];
      }

   zTaskMessage(3,"File Contains %ld Valid Points\n",(long)N);
   zTaskMessage(3,"r=%lG, q=%lG\n",R,Q);
   fcloseall();

   return(0);
   }

/************************************************************
**
** Build Prediction Data Times
*/
int BUILD()
   {
   double II;
   struct KALMANSTRUCT Predicted, InputData;
   char InputBuffer[sizeof(struct TRData)];
   struct TRData *pInputTRData;
   struct RData  *pInputRData;
   double TimeCount=0.;
   int Flag;

   pInputTRData = (struct TRData *)InputBuffer;
   pInputRData  = (struct  RData *)InputBuffer;

   Predicted.f = 0;
   Predicted.y = 0.;

   InputData.f = 1;

   zTaskMessage(3,"Building Prediction Times\n",N);

/*
** From this point on, the input file is now a temporary file
*/
   strcpy(InputFileName,TempFileName);
   zTaskMessage(2,"Opening Input File '%s'\n",InputFileName);
   if (!(InputStream = zOpen(InputFileName,O_readb))) return(2);

   zBuildFileName(M_tmpname,TempFileName);
   zTaskMessage(2,"Opening Scratch File '%s'\n",TempFileName);
   if (!(OutputStream = zOpen(TempFileName,O_writeb))) return(2);

/*
** Read Past any initial bad data for setting prediction times
*/
   do {
      if (!Zread(InputStream,InputBuffer,FileHeader.type)) return(2);
      Flag =  (FileHeader.type == R_Data) ? pInputRData->f : 0;
      ++TimeCount;
      }
   while (Flag);
   --TimeCount;
   switch (FileHeader.type)
      {
      case R_Data:
         InputData.y = pInputRData->y;
         InputData.t = FileHeader.m*TimeCount + FileHeader.b;
         break;
      case TR_Data:
         InputData.y = pInputTRData->y;
         InputData.t = pInputTRData->t;
         break;
      }

   for (II = 0.; II < FACTOR; ++II)
      {
      Predicted.t = II * TM + TB;
      while ((InputData.t <= Predicted.t) && !feof(InputStream))
         {
         fwrite(&InputData,sizeof(struct KALMANSTRUCT),1,OutputStream);
         do {
            if (!Zread(InputStream,InputBuffer,FileHeader.type)) break;
            Flag =  (FileHeader.type == R_Data) ? pInputRData->f : 0;
            ++TimeCount;
            }
         while (Flag);
         switch (FileHeader.type)
            {
            case R_Data:
               InputData.y = pInputRData->y;
               InputData.t = FileHeader.m*TimeCount + FileHeader.b;
               break;
            case TR_Data:
               InputData.y = pInputTRData->y;
               InputData.t = pInputTRData->t;
               break;
            }
         }
      fwrite(&Predicted,sizeof(struct KALMANSTRUCT),1,OutputStream);
      }

   fcloseall();
   unlink(InputFileName);
   return(0);
   }

/************************************************************
**
** Strip File of Redundant Points
*/
int STRIP()
   {
   struct KALMANSTRUCT InputData1, InputData2;
   int DoneFlag = 0;

   zTaskMessage(3,"Stripping Redundant Points\n",N);

   strcpy(InputFileName,TempFileName);
   zTaskMessage(2,"Opening Input File '%s'\n",InputFileName);
   if (!(InputStream = zOpen(InputFileName,O_readb))) return(2);

   zBuildFileName(M_tmpname,TempFileName);
   zTaskMessage(2,"Opening Scratch File '%s'\n",TempFileName);
   if (!(OutputStream = zOpen(TempFileName,O_writeb))) return(2);

   fread(&InputData1,sizeof(struct KALMANSTRUCT),1,InputStream);
   while (!feof(InputStream))
      {
      fread(&InputData2,sizeof(struct KALMANSTRUCT),1,InputStream);
      if (!feof(InputStream))
         {
         if (InputData1.t == InputData2.t)
            {
            if (InputData1.f)
               {
               InputData1.f = -1;
               fwrite(&InputData1,sizeof(struct KALMANSTRUCT),1,OutputStream);
               }
            else
               {
               InputData2.f = -1;
               fwrite(&InputData2,sizeof(struct KALMANSTRUCT),1,OutputStream);
               }
            fread(&InputData1,sizeof(struct KALMANSTRUCT),1,InputStream);
            if (feof(InputStream)) DoneFlag = 1;
            }
         else
            {
            fwrite(&InputData1,sizeof(struct KALMANSTRUCT),1,OutputStream);
            InputData1 = InputData2;
            }
         }
      }

   if (!DoneFlag)
      fwrite(&InputData1,sizeof(struct KALMANSTRUCT),1,OutputStream);

   fcloseall();
   unlink(InputFileName);
   return(0);
   }

/************************************************************
**
** Create Prediction Points
*/
int  PREDICT()
   {
   struct PREDICTSTRUCT PredictOutput, PredictInput;
   struct KALMANSTRUCT KalmanInput, KalmanOutput;
   double Xk, Xkp1, Ykp1, Zkp1, Pk, Pkp1, Tk, Tkp1, Yk;
   double Rinv;
   struct RData OutputRData;
   int Flag;
   long lOffset;

   zTaskMessage(3,"Creating Prediction Values\n",N);

   strcpy(InputFileName,TempFileName);
   zTaskMessage(2,"Opening Input File '%s'\n",InputFileName);
   if (!(InputStream = zOpen(InputFileName,O_readb))) return(2);

   zBuildFileName(M_tmpname,TempFileName);
   zTaskMessage(2,"Opening Scratch File '%s'\n",TempFileName);
   if (!(OutputStream = zOpen(TempFileName,O_writeb))) return(2);

   fread(&KalmanInput,sizeof(struct KALMANSTRUCT),1,InputStream);

   Tk = PredictOutput.t = KalmanInput.t;
   Xk = PredictOutput.x = KalmanInput.y;
   Pk = PredictOutput.p = R;
   Flag = PredictOutput.f = KalmanInput.f;

   fwrite(&PredictOutput,sizeof(struct PREDICTSTRUCT),1,OutputStream);

   while(!feof(InputStream))
      {
      fread(&KalmanInput,sizeof(struct KALMANSTRUCT),1,InputStream);
      Tkp1 = KalmanInput.t;
      Zkp1 = KalmanInput.y;
      Flag = KalmanInput.f;
      if (!feof(InputStream))
         {
         Rinv = fabs((double)Flag)/R;
         Pkp1 = 1./(1./(Pk + Q*(Tkp1 - Tk)) + Rinv);
         Xkp1 = Xk + Pkp1*Rinv*(Zkp1 - Xk);
         Tk = PredictOutput.t = Tkp1;
         Xk = PredictOutput.x = Xkp1;
         Pk = PredictOutput.p = Pkp1;
         PredictOutput.f = Flag;
         fwrite(&PredictOutput,sizeof(struct PREDICTSTRUCT),1,OutputStream);
         }
      }

   fcloseall();
   unlink(InputFileName);

   strcpy(InputFileName,TempFileName);
   zTaskMessage(2,"Opening Input File '%s'\n",InputFileName);
   if (!(InputStream = zOpen(InputFileName,O_readb))) return(2);

   zBuildFileName(M_tmpname,TempFileName);
   zTaskMessage(2,"Opening Scratch File '%s'\n",TempFileName);
   if (!(OutputStream = zOpen(TempFileName,O_writeb))) return(2);
/*
* Write out Y's
*/
   fseek(InputStream,-(long)sizeof(struct PREDICTSTRUCT),SEEK_END);
   lOffset = ftell(InputStream);

   fread(&PredictInput,sizeof(struct PREDICTSTRUCT),1,InputStream);
   Tkp1 = KalmanOutput.t = PredictInput.t;
   Ykp1 = KalmanOutput.y = PredictInput.x;
   Pkp1 = PredictInput.x;
   KalmanOutput.f = PredictInput.f;
   fwrite(&KalmanOutput,sizeof(struct KALMANSTRUCT),1,OutputStream);

   while ((lOffset -= (long)sizeof(struct PREDICTSTRUCT)) >= 0L)
      {
      fseek(InputStream,lOffset,SEEK_SET);
      fread(&PredictInput,sizeof(struct PREDICTSTRUCT),1,InputStream);
      Tk = PredictInput.t;
      Xk = PredictInput.x;
      Pk = PredictInput.p;
      Yk = Xk + Pk*(Ykp1 - Xk)/(Pk + Q*(Tkp1-Tk));
      KalmanOutput.t = Tkp1 = Tk;
      KalmanOutput.y = Ykp1 = Yk;
      KalmanOutput.f = PredictInput.f;
      fwrite(&KalmanOutput,sizeof(struct KALMANSTRUCT),1,OutputStream);
      }

   fcloseall();
   unlink(InputFileName);

   strcpy(InputFileName,TempFileName);
   zTaskMessage(2,"Opening Input File '%s'\n",InputFileName);
   if (!(InputStream = zOpen(InputFileName,O_readb))) return(2);

   zBuildFileName(M_tmpname,TempFileName);
   zTaskMessage(2,"Opening Scratch File '%s'\n",TempFileName);
   if (!(OutputStream = zOpen(TempFileName,O_writeb))) return(2);

   FileHeader.type = R_Data;
   FileHeader.m = TM;
   FileHeader.b = TB;
   OutputRData.f = 0;

   if (Zputhead(OutputStream,&FileHeader)) return(2);
   fseek(InputStream,0L,SEEK_END);
   lOffset = ftell(InputStream);

   while ((lOffset -= (long)sizeof(struct KALMANSTRUCT)) >= 0L)
      {
      fseek(InputStream,lOffset,SEEK_SET);
      fread(&KalmanInput,sizeof(struct KALMANSTRUCT),1,InputStream);
      if (KalmanInput.f < 1)
         {
         OutputRData.y = KalmanInput.y;
         if (Zwrite(OutputStream,(char *)&OutputRData,R_Data)) return(2);
         }
      }

   fcloseall();
   unlink(InputFileName);
   return(0);
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
   unlink(TempFileName);
   if (ErrorFlag > 1) unlink(InputFileName); // Should always be a temporary file at this point.
   ZCatFiles((char*)NIL); // free catalog memory
   Zexit(a);
   }

void fcloseall()
   {
   if (InputStream) Zclose(InputStream);
   InputStream = (FILE *)NIL;

   if (OutputStream) Zclose(OutputStream);
   OutputStream = (FILE *)NIL;

   return;
   }
