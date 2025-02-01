/*
**
** Dr. Eric R. Nelson
** 12/28/2016
*/
#ifndef tisan_h
#define tisan_h

#include <fcntl.h>

#include "main.h"
#include "dos.h"

#define ADVCOUNT   45	// Nuymber of ADVERBS supported
#define PARMSCOUNT 10   // Number of elements in the PARMS adverb, which is an array

/******************************************
** Constants for the working path for the TISAN
** Program on startup so we know where to
** find the INPUTS and RUN files along with their extensions.
*/
#define INPUTSDIR "inputs/"
#define INPUTSEXT ".INP"
#define RUNDIREXT "run"

/*
** Define Functions
*/
PSTR parseString(FILE* pFile, int N, char szStr[N]);
PSTR parseAdverb(FILE* pFile, int N, char szAdverb[N]);

void initializeAdverbArrays(void);
BOOL displayTaskInputs(char *); // INPUTS()

BOOL  zTaskInit(PSTR);
BOOL  zGetAdverbs(PSTR);
BOOL  zPutAdverbs(PSTR);
PSTR  zBuildFileName(short, PSTR);
long zGetData(long, FILE *,char *,short);
long zPutData(long,FILE *,char *,short);

short zMessage(short level, const char *format, ...);
short zTaskMessage(short level, const char *format, ...);

FILE  *zOpen(PSTR,short);
void  zError(void);

struct FILEHDR *Zgethead(FILE *,struct FILEHDR *);
short           Zputhead(FILE *,struct FILEHDR *);
short           Zclose(FILE *);
short           zNameOutputFile(char *,char *);
char           *Zread(FILE *,char *,short);
short           Zwrite(FILE *,char *,short);
short           Zsize(short);
struct CATSTRUCT *ZCatFiles(char *);   // Used to support wild cards file names

void  BEEP(void);

void Zexit(int N);

BOOL isTisanHeader(struct FILEHDR *pHeader);
void setHeaderText(struct FILEHDR *pHeader);
void getHeaderText(struct FILEHDR *pHeader);

long countDataRecords(FILE *stream);

BOOL extractValues(char *BUFFER, struct FILEHDR *fileHeader, double TCNT, double *TIME, double *Rval, struct complex *Zval, short *FLAG);
BOOL insertValues(char *BUFFER, struct FILEHDR *fileHeader, double TIME, double Rval, struct complex Zval, short FLAG);

/*
** This framework was created before the C language was standardized. The complex data type did not exist, so I made my own.
** Complex number are needed to deal with Fourier transforms. I had no idea what I was going to need at the time, so I just
** defined everything.
*/
struct complex cmplx(double, double);                 /* R + iI            */

double c_abs(struct complex z);                       /* sqrt(z1^2 + z2^2) */

struct complex c_add(struct complex,struct complex);  /* z1 + z2           */
struct complex c_sub(struct complex,struct complex);  /* z1 - z2           */
struct complex c_mul(struct complex,struct complex);  /* z1 * z2           */
struct complex c_div(struct complex,struct complex);  /* z1 / z2           */
struct complex c_sqrt(struct complex);                /* sqrt(z)           */
struct complex c_ln(struct complex);                  /* ln(z)             */
struct complex c_exp(struct complex);                 /* exp(z)            */
struct complex c_sin(struct complex);                 /* sin(z)            */
struct complex c_cos(struct complex);                 /* cos(z)            */
struct complex c_tan(struct complex);                 /* tan(z)            */
struct complex c_sec(struct complex);                 /* sec(z)            */
struct complex c_csc(struct complex);                 /* csc(z)            */
struct complex c_cot(struct complex);                 /* cot(z)            */
struct complex c_asin(struct complex);                /* asin(z)           */
struct complex c_acos(struct complex);                /* acos(z)           */
struct complex c_atan(struct complex);                /* atan(z)           */
struct complex c_acot(struct complex);                /* acot(z)           */
struct complex c_acsc(struct complex);                /* acsc(z)           */
struct complex c_asec(struct complex);                /* asec(z)           */
struct complex c_sinh(struct complex);                /* sinh(z)           */
struct complex c_cosh(struct complex);                /* cosh(z)           */
struct complex c_tanh(struct complex);                /* tanh(z)           */
struct complex c_sech(struct complex);                /* sech(z)           */
struct complex c_csch(struct complex);                /* csch(z)           */
struct complex c_coth(struct complex);                /* coth(z)           */
struct complex c_asinh(struct complex);               /* asinh(z)          */
struct complex c_acosh(struct complex);               /* acosh(z)          */
struct complex c_atanh(struct complex);               /* atanh(z)          */
struct complex c_acsch(struct complex);               /* acsch(z)          */
struct complex c_asech(struct complex);               /* asech(z)          */
struct complex c_acoth(struct complex);               /* acoth(z)          */
struct complex c_log10(struct complex);               /* log10(z)          */
struct complex c_pow(struct complex,struct complex);  /* pow(z1,z2)        */

double asinh(double);
double acosh(double);
double atanh(double);

void getIntFrac(double val, double *pIntval, double *pFracVal);
BOOL isodd(double val);
BOOL iseven(double val);
double unbiasedRound(double val);

long filesize(FILE *);

BOOL getConfigString(PSTR key, int N, PSTR szValue);

void printPercentComplete(long y, long N, short printPercentComplete);

/*
** Functions used by all tasks to deal with errors and termination requests
*/
void BREAKREQ(int a);
void BombOff(int a);
void FPEERROR(int a);


/*
** Define Adverbs
*/
double TRANGE[2],         YRANGE[2], ZRANGE[2];
double TMAJOR[2],         YMAJOR[2], ZMAJOR[2];
double PARMS[PARMSCOUNT], POINT[2], ZFACTOR[2];
short WINDOW[4];
char TASKNAME[16];
char INNAME[_MAX_FNAME],  INCLASS[_MAX_EXT], INPATH[_MAX_DRIVE+_MAX_DIR];
char IN2NAME[_MAX_FNAME], IN2CLASS[_MAX_EXT], IN2PATH[_MAX_DRIVE+_MAX_DIR];
char IN3NAME[_MAX_FNAME], IN3CLASS[_MAX_EXT], IN3PATH[_MAX_DRIVE+_MAX_DIR];
char OUTNAME[_MAX_FNAME], OUTCLASS[_MAX_EXT], OUTPATH[_MAX_DRIVE+_MAX_DIR];
char TFORMAT[32], YFORMAT[32], ZFORMAT[32];
char PARITY[8], DEVICE[8];
char TLABEL[128], YLABEL[128], ZLABEL[128], TITLE[128];
double FACTOR;
short CODE, BAUD, STOPBITS, PROGRESS, ITYPE;
short TMINOR, YMINOR, ZMINOR, FRAME, BORDER, QUIET;
LONG COLOR;

/*
** Hardware specific definitions that are no longer in use (the DEVICES structure is defined in main.h)
*/
struct DEVICES HARDWARE;
short INTERRUPT;

/*
** Arrays for adverb names
*/
short ADVSIZE[ADVCOUNT];
char *ADVSTR[ADVCOUNT];
char *ADVPNTR[ADVCOUNT];

/*
** Task executables
*/
char TisanDrive[_MAX_DRIVE];
char TisanDir[_MAX_DIR];


#endif
