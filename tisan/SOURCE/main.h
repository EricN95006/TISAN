/*
**
** Dr. Eric R. Nelson
** 12/28/2016
*/
#ifndef main_h
#define main_h

#define far

#include "atcs.h"

#define LABELSIZE 128
/*
** Character strings used throughout the program
*/
#define NullString ""
#define Space " "
#define CrLf "\n"

#define OF_READWRITE O_RDWR
#define OF_WRITE     O_WRONLY
#define OF_READ      O_RDONLY
#define OF_CREATE    O_CREAT | O_TRUNC

#define OPENERROR ((HANDLE)-1)
#define IOERROR   ((WORD)-1)

/*
** macros to get to specific bits on the original IBM CGA graphics screen
** and the Hercules high resolution graphics cards. These macros are no
** longer used but remain here for historical interest. The pos(X,Y) macro
** locates the specific byte of memory that has the bit at coordinates X,Y.
** The pe(X) macros gets the specific bit in the byte located using the pos(X,Y)
** macro.
*/
#define CGApos(X,Y) (((Y>>1)<<6)+((Y>>1)<<4)+((Y&1)<<13)+(X>>GF1))
#define CGApel(X) (1<<((GF2 - (X - ((X>>GF1)<<GF1)))<<GF3))
#define HERCpos(X,Y) (0x2000*(Y%4)) + (90*(Y>>2)) + (X>>3)
#define HERCpel(X) (1<<(7 - (X%8)))

#define Min(a,b) ((a) < (b) ? (a) : (b))
#define Max(a,b) ((a) > (b) ? (a) : (b))
#define Square(n) ((n)*(n))

#define Maxbuff 2048

#define M_inname  (char)0
#define M_in2name (char)01
#define M_outname (char)02
#define M_tmpname (char)03

#define O_readb   (char)00
#define O_writeb  (char)01
#define O_appendb (char)02
#define O_readt   (char)03
#define O_writet  (char)04
#define O_appendt (char)05

#define R_Data  0
#define TR_Data 1
#define X_Data  2
#define TX_Data 3

/*
** Define Structures
*/
struct complex {double x;
                double y;};

struct RData {double y;
              short  f;};

struct TRData {double t;
               double y;};

struct XData {struct complex z;
              short f;};

struct TXData {double t;
               struct complex z;};

struct FILEHDR {char szTISAN[8];          // TISAN\0\r\n
                char szTitle[LABELSIZE];  // text...\0\r\n
                char szTlabel[LABELSIZE]; // text...\0\r\n
                char szYlabel[LABELSIZE]; // text...\0\r\n
                short type;
                double m;
                double b;};

struct DEVICES {char scrn;
                char pntr;
                char pltr;};
#endif
