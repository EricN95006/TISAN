/*******************************************************************
**
** Dr. Eric R. Nelson
** 12/28/2016
*
*  Task DBPLOT
*
* Plot data, in all sorts of interesting ways, to a BMP memory image and save it to a file.
* 
* Characters in the range 0 to 255 are supported (although not all are implemented). Anything outside the range is just not displayed.
* Characters are in FONT[] which are defined in tisanfnt.h
*
* POINT  Canvas size. Default is 640x640 (x,y)
*
* PARMS[0]: LABEL LOCATIONS <0 NO LABELS
*              100's TITLE,  0-> TOP
*              10's  XLABEL, 0-> BOTTOM
*              1's   YLABEL, 0-> LEFT SIDE
*
* PARMS[1]: X TICS <0 NO TICS
*              100's=0 Inside, 100's >0 Outside
*              10's=0 Double Marks, 10's>0 Single Marks
*              1's=0 BOTTOM, 1's>0 TOP
*
* PARMS[2]: Y TICS <0 NO TICS
*              100's=0 Inside, 100's >0 Outside
*              10's=0 Double Marks, 10's>0 Single Marks
*              1's=0 LEFT, 1's>0 RIGHT
*
* PARMS[3]: <= 0 ->PLOT LINES else PLOT POINTS abs(PARMS[3]) is SYMBOL
* PARMS[4]: LINETYPE TO USE(<= 0 -> SOLID)
* These controls are incremented by 1, if set, for each file plotted.
*
* PARMS[5]: T LOG BASE
* PARMS[6]: Y LOG BASE
* 
* PARMS[7]: T-AXIS END POINT LABELING (1's LOWER, 10's HIGHER)
* PARMS[8]: Y-AXIS END POINT LABELING (1's LOWER, 10's HIGHER)
*
* PARMS[9]: NUMERIC LABEL CONTROL (1's  Y AXIS VERTICAL NUMBERS DEFAULT)
*                                 (10's T AXIS HORIZONTAL NUMBERS DEFAULT)]
*                                 (100's INVERT Y AXIS IF SET)
*                                 (1000's INVERT T AXIS IF SET)
*                                 
* COLOR: 10's SET BACKGROUND (0-15)
*        1's  SET POINT COLOR (1-6)
*
* FACTOR: Data skip interval (2 = every other point, ect.)
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
#include "tisanfnt.h"

#define Tpnt(x) (IITA ? (W0 + W2 - (x)) : (x))    /* Reverse T Axis */
#define Ypnt(y) (IIYA ? (W1 + W3 - (y)) : (y))    /* Reverse Y Axis */

#define SIZEOFBFIH      14 // size of the BITMAPFILEHEADER content (unfortunately not the same size as the structure)
#define SIZEOFBIH       40 // Size of BITMAPINFOHEADER for true color images
#define DEFAULTDIM      640
#define IMAGEBYTES  (canvasHeight * (canvasWidth * 4))
#define BMPBYTES    (IMAGEBYTES + SIZEOFBIH)
#define BI_RGB     0
#define TRUECOLOR 32

#define BACKSLASH ((char)92) // the '\' character

/*
 * Bitmaps on disk have the following basic structure
 *  BITMAPFILEHEADER (may be missing if file is not saved properly by the creating application)
 *  BITMAPINFO -
 *        BITMAPINFOHEADER
 *        RGBQUAD - Color Table Array (not present for true color images)
 *  Bitmap Bits in one of many coded formats
 *
 *  The BMP image is stored from bottom to top, meaning that the first scan line in the file is the last scan line in the image.
 *
 *  For ALL images types, each scan line is padded to an even 4-byte boundary.
 *  
 *  For images where there are multiple pels per byte, the left side is the low order element and the right is the
 *  high order element (Intel integer format also known as little-endian)
*  
*  BITMAPFILEHEADER
*  
*  bfType
*      Specifies the file type. It must be set to the signature word BM (0x4D42) to indicate bitmap.
*  bfSize
*      Specifies the size, in bytes, of the bitmap file.
*  bfReserved1
*      Reserved; set to zero
*  bfReserved2
*      Reserved; set to zero
*  bfOffBits
*      Specifies the offset, in bytes, from the BITMAPFILEHEADER structure to the bitmap bits
*/
struct BITMAPFILEHEADER {WORD  bfType;
                         DWORD bfSize;
                         WORD  bfReserved1;
                         WORD  bfReserved2;
                         DWORD bfOffBits;};        // 14 bytes. Maps into 16 bytes on 64 bit systems (rats, cannot just save it as is)

/*
*  BITMAPINFOHEADER
*  
*  biSize
*      Specifies the size of the structure, in bytes.
*      This size does not include the color table or the masks mentioned in the biClrUsed member.
*      See the Remarks section for more information.
*  biWidth
*      Specifies the width of the bitmap, in pixels.
*  biHeight
*      Specifies the height of the bitmap, in pixels.
*      If biHeight is positive, the bitmap is a bottom-up DIB and its origin is the lower left corner.
*      If biHeight is negative, the bitmap is a top-down DIB and its origin is the upper left corner.
*      If biHeight is negative, indicating a top-down DIB, biCompression must be either BI_RGB or BI_BITFIELDS. Top-down DIBs cannot be compressed.
*  biPlanes
*      Specifies the number of planes for the target device.
*      This value must be set to 1.
*  biBitCount
*      Specifies the number of bits per pixel.
*      The biBitCount member of the BITMAPINFOHEADER structure determines the number of bits that define each pixel and the maximum number of colors in the bitmap.
*  
*      The bmiColors color table is used for optimizing colors used on palette-based devices, and must contain the number of entries specified by the biClrUsed member of the BITMAPINFOHEADER.
*  
*      This member must be one of the following values.
*      Value     Description
*      1       The bitmap is monochrome, and the bmiColors member contains two entries.
*              Each bit in the bitmap array represents a pixel. The most significant bit is to the left in the image. 
*              If the bit is clear, the pixel is displayed with the color of the first entry in the bmiColors table.
*              If the bit is set, the pixel has the color of the second entry in the table.
*      2       The bitmap has four possible color values.  The most significant half-nibble is to the left in the image.
*      4       The bitmap has a maximum of 16 colors, and the bmiColors member contains up to 16 entries.
*              Each pixel in the bitmap is represented by a 4-bit index into the color table. The most significant nibble is to the left in the image.
*              For example, if the first byte in the bitmap is 0x1F, the byte represents two pixels. The first pixel contains the color in the second table entry,
*              and the second pixel contains the color in the sixteenth table entry.
*      8       The bitmap has a maximum of 256 colors, and the bmiColors member contains up to 256 entries. In this case, each byte in the array represents a single pixel.
*      16      The bitmap has a maximum of 2^16 colors.
*              If the biCompression member of the BITMAPINFOHEADER is BI_RGB, the bmiColors member is NULL.
*              Each WORD in the bitmap array represents a single pixel. The relative intensities of red, green, and blue are represented with 5 bits for each color component.
*              The value for blue is in the least significant 5 bits, followed by 5 bits each for green and red.
*              The most significant bit is not used. The bmiColors color table is used for optimizing colors used on palette-based devices,
*              and must contain the number of entries specified by the biClrUsed member of the BITMAPINFOHEADER.
*      24      The bitmap has a maximum of 2^24 colors, and the bmiColors member is NULL.
*              Each 3-byte triplet in the bitmap array represents the relative intensities of blue, green, and red, respectively, for a pixel.
*      32      The bitmap has a maximum of 2^32 colors. If the biCompression member of the BITMAPINFOHEADER is BI_RGB, the bmiColors member is NULL.
*              Each DWORD in the bitmap array represents the relative intensities of blue, green, and red, respectively, for a pixel. The high byte in each DWORD is not used. The bmiColors color table is
*              used for optimizing colors used on palette-based devices, and must contain the number of entries specified by the biClrUsed member of the BITMAPINFOHEADER.
*              If the biCompression member of the BITMAPINFOHEADER is BI_BITFIELDS, the bmiColors member contains three DWORD color masks that specify the red, green, and blue components,
*              respectively, of each pixel.
*              Each DWORD in the bitmap array represents a single pixel.
*  biCompression
*      Specifies the type of compression for a compressed bottom-up bitmap (top-down DIBs cannot be compressed). This member can be one of the following values.
*      Value               Description
*      BI_RGB              An uncompressed format.
*      BI_BITFIELDS        Specifies that the bitmap is not compressed and that the color table consists of three DWORD color masks that specify the red, green, and blue components of each pixel.
*                          This is valid when used with 16- and 32-bpp bitmaps.
*                          This value is valid in Windows Embedded CE versions 2.0 and later.
*      BI_ALPHABITFIELDS   Specifies that the bitmap is not compressed and that the color table consists of four DWORD color masks that specify the red, green, blue, and alpha components of each pixel.
*                          This is valid when used with 16- and 32-bpp bitmaps.
*                          This value is valid in Windows CE .NET 4.0 and later.
*                          You can OR any of the values in the above table with BI_SRCPREROTATE to specify that the source DIB section has the same rotation angle as the destination.
*  biSizeImage
*      Specifies the size, in bytes, of the image. This value will be the number of bytes in each scan line which must be padded to
*      insure the line is a multiple of 4 bytes (it must align on a DWORD boundary) times the number of rows.
*      This value may be set to zero for BI_RGB bitmaps (so you cannot be sure it will be set).
*  biXPelsPerMeter
*      Specifies the horizontal resolution, in pixels per meter, of the target device for the bitmap.
*      An application can use this value to select a bitmap from a resource group that best matches the characteristics of the current device.
*  biYPelsPerMeter
*      Specifies the vertical resolution, in pixels per meter, of the target device for the bitmap
*  biClrUsed
*      Specifies the number of color indexes in the color table that are actually used by the bitmap.
*      If this value is zero, the bitmap uses the maximum number of colors corresponding to the value of the biBitCount member for the compression mode specified by biCompression.
*      If biClrUsed is nonzero and the biBitCount member is less than 16, the biClrUsed member specifies the actual number of colors the graphics engine or device driver accesses.
*      If biBitCount is 16 or greater, the biClrUsed member specifies the size of the color table used to optimize performance of the system color palettes.
*      If biBitCount equals 16 or 32, the optimal color palette starts immediately following the three DWORD masks.
*      If the bitmap is a packed bitmap (a bitmap in which the bitmap array immediately follows the BITMAPINFO header and is referenced by a single pointer),
*      the biClrUsed member must be either zero or the actual size of the color table.
*  biClrImportant
*      Specifies the number of color indexes required for displaying the bitmap.
*      If this value is zero, all colors are required.
*  Remarks
*  
*  The BITMAPINFO structure combines the BITMAPINFOHEADER structure and a color table to provide a complete definition of the dimensions and colors of a DIB.
*  An application should use the information stored in the biSize member to locate the color table in a BITMAPINFO structure, as follows.
*  
*  pColor = ((LPSTR)pBitmapInfo + (WORD)(pBitmapInfo->bmiHeader.biSize));
*/

struct BITMAPINFOHEADER {DWORD biSize;
                         LONG  biWidth;
                         LONG  biHeight;
                         WORD  biPlanes;
                         WORD  biBitCount;
                         DWORD biCompression;
                         DWORD biSizeImage;
                         LONG  biXPelsPerMeter;
                         LONG  biYPelsPerMeter;
                         DWORD biClrUsed;
                         DWORD biClrImportant;};      // 40 bytes

struct BITMAP {struct BITMAPINFOHEADER bmpinfoHeader;
               LONG                    imageArray[1];};  // 44 bytes

void MAININIT(void);
void PLOT1(FILE *);
void PLOT2(FILE *,FILE *);
short IMLOAD(FILE *);
void FRAMEIT(void);
void PUTLAB(void);
void PUTTICS(void);
void PLTALPHA(char *,short,short,short);
void PLTSYM(double,double);
void ROTATE(short *,short *,double);
short GETRNG(double *,double *,double *,double *,FILE *,struct FILEHDR *,int);
short GTEXT(short *,short *);
short XMOVE(short *,short *);
double PLOG(double,short);
void PTXTICS(short,short);
void PTYTICS(short,short);
void PTSETUP(short *,short *);
void LINE(short,short,short,short);
void VECTOR(double,double,double,double,short);

void PSET(short,short);     /* Pixel Setting Functions (hardware level)*/

BOOL isBitmapImage(FILE *inputStream);
void readBitmapFileHeader(FILE *inputStream, struct BITMAPFILEHEADER *fileHeader);
void saveBitmapFileHeader(DWORD bmpInfoHeader_biSizeImage);
void saveBitmapImage(void);
BOOL createBitmapImage(void);

PSTR addEscapes(PSTR OUTFILE);

short CWIDTH=8, CHEIGHT=8, XTICBASE=6, YTICBASE=8;

short canvasWidth, canvasHeight; // Canvas dimensions

short SCLIPXL,SCLIPYL,SCLIPXH,SCLIPYH,NOCLIP=0;
short LT1=0, LT2=0, SLT1=0, SLT2=0, IYV, ITV, IIYA, IITA;
short W0, W1, W2, W3, T2FLAG=0;
short GF1, GF2, GF3, CS1, CS2;
char BUFFER[256];
char far *SCREEN;
char SBUFF[1280], TMPFILE[_MAX_PATH];
double TSLOPE, TINTER, YSLOPE, YINTER, T0, T1, TCNT, TIME;
double TPL1, TPL2, YPL1, YPL2, XROT, YROT, YSLOPE, YINTER, YDELTA;
double TDELTA, YVT, TVT;

/*
** The Logical Units (LU) Table is used to intelligently place scaled tic marks on a graph.
** Unfortunately, I don't really remember why there are duplicate entries here, so that's
** one for the "insufficient documentation" pile. Remember, I wrote most of this code in 1986,
** and ANSI C was not even standardized until three years later.
*/
short LUTABLE[] = {1,2,4,5,8,10,10,15,15,20,25};

struct FILEHDR FileHeader, F1Header, F2Header;
char BUFFER1[sizeof(struct TXData)];
char BUFFER2[sizeof(struct TXData)];
struct RData   *RDataPntr1,  *RDataPntr2;
struct TRData *TRDataPntr1, *TRDataPntr2;
struct TXData *TXDataPntr1, *TXDataPntr2;
struct XData   *XDataPntr1,  *XDataPntr2;
long LFPS1, LFPE1;  /* Long File Pointer Start and End */
double TCNTS, TCNTE;
double TIME, DATA;
short FLAG=0;
double TOTAL=0.;
short FirstPointFlag;
long lScaleCount = 0L;


struct BITMAPFILEHEADER bmpFileHeader;
struct BITMAP *pBitMap = (struct BITMAP *)NIL;

FILE *INSTREAM  = (FILE *)NIL;
FILE *IN2STREAM = (FILE *)NIL;
FILE *OUTSTREAM = (FILE *)NIL;

const char szTask[]="DBPLOT";

int main(int argc, char *argv[])
   {
   double YMAX=0., YMIN=0., TMIN=0.,TMAX=0.;
   double TR1, TR2, YR1, YR2;
   short I;
   short ERRFLAG=0;
   struct CATSTRUCT *CatList;
   char *pChar;
   char Drive[_MAX_DRIVE], Dir[_MAX_DIR], Fname[_MAX_FNAME], Ext[_MAX_EXT];
   char INFILE[_MAX_PATH], IN2FILE[_MAX_PATH], OUTFILE[_MAX_PATH];
   int AutoAmpScale = 0, AutoTimeScale = 0, SecondFile = 0, OutPutFile = 1;
   BOOL isBitmap;
   LONG plotColor;
   char szOpenCommand[512];
   char szBMPviewer[256];

   if (sizeof(struct BITMAP) != 44L)
      {
      zTaskMessage(10, "BITMAP structure is %ld bytes long. Expected 44. Change the definition of LONG in atcs.h.\n", sizeof(struct BITMAP));
      Zexit(1);
      }
   
   signal(SIGINT,BREAKREQ);
   signal(SIGFPE,FPEERROR);  /* Setup Floating Point Error Trap */

   if (zTaskInit(argv[0])) Zexit(1);

   T0 = TRANGE[0];
   T1 = TRANGE[1];

   MAININIT();

   if (YRANGE[0] >= YRANGE[1]) AutoAmpScale = 1;
   if (TRANGE[0] >= TRANGE[1]) AutoTimeScale = 1;

   if (isEmptyString(OUTCLASS)) strcpy(OUTCLASS,"bmp");

   zBuildFileName(M_inname,INFILE);
   zBuildFileName(M_in2name,IN2FILE);
   zBuildFileName(M_tmpname,TMPFILE);
   zBuildFileName(M_outname,OUTFILE);

   if (strcmp(INFILE,IN2FILE)) SecondFile = 1;

   if (!strcmp(INFILE,OUTFILE))
      {
      OutPutFile = 0;  // Never overwrite the original data file!
      zTaskMessage(10,"Bitmap cannot overwrite input file. Change the output file specification.\n");
      Zexit(1);
      }

   if (!(CatList = ZCatFiles(INFILE))) Zexit(1); /* Find Matching Files */

   if (AutoAmpScale || AutoTimeScale)
      {
      zTaskMessage(3,"Determining Primary File Scale(s)\n");

      pChar = CatList->pList;

      for (I = 0; I < CatList->N; ++I)
         {
         splitPath(pChar,Drive,Dir,Fname,Ext);
         strcpy(INNAME,Fname);
         strcpy(INCLASS,Ext);
         zBuildFileName(M_inname,INFILE);
         zTaskMessage(2,"Opening Input File '%s' for Scaling\n",INFILE);

         INSTREAM = zOpen(INFILE,O_readb);
         if (INSTREAM == NULL) Zexit(1);
         
         if (!Zgethead(INSTREAM,&F1Header)) BombOff(1);

         switch (F1Header.type)
            {
            case R_Data:
            case TR_Data:
            case X_Data:
            case TX_Data:
               break;
            default:
               zTaskMessage(10,"Invalid File Type.\n");
               Zexit(1);
            }

         if (GETRNG(&TMIN,&TMAX,&YMIN,&YMAX,INSTREAM,&F1Header,I)) BombOff(1);

         Zclose(INSTREAM);
         INSTREAM = (FILE *)NIL;
         
         pChar = strchr(pChar,NUL) + 1; // go to the next file name in the list
         }

/*
** When autoscaling we could be processing multiple files, so we need to get the results from all of them combined.
** We then need to see if we have invalid auto ranges, but only if that axis is to be scaled automatically.
*/
      if (AutoAmpScale || AutoTimeScale) // If we are auto scaling we need to check for invalid conditions such as no data in the selected ranges
         {
         if (lScaleCount == 0L)
            {
            zTaskMessage(10, "No data in the selected ranges with which to autoscale.\n");
            BombOff(1);
            }
/*
** We want to trap auto ranging errors for both axis at the same time so we can print both errors at once (if there are two errors)
**
** There is the possibility of an odd bug if a secondary file is being use for the time base and autoscaling for time is requested since
** checking for the second file happens later down in the logic. The trap that catches an invalid time range could erroneously happen here under just the right conditions.
*/
         if (AutoTimeScale && (TMIN == TMAX)) zTaskMessage(10,"Time range is invalid for autoscaling: %lG, %lG\n", TMIN, TMAX);
         if (AutoAmpScale  && (YMIN == YMAX)) zTaskMessage(10,"Amplitude range is invalid for autoscaling: %lG, %lG\n", YMIN, YMAX);

         if ((AutoTimeScale && (TMIN == TMAX)) || (AutoAmpScale  && (YMIN == YMAX))) BombOff(1); // One of the ranges is bad, so give up
         }

      if (AutoAmpScale)
         {
         YRANGE[0] = YMIN;
         YRANGE[1] = YMAX;
         }

      if (AutoTimeScale)
         {
         TRANGE[0] = TMIN;
         TRANGE[1] = TMAX;
         }
      }

   if (OutPutFile)
      {
      if (strchr(OUTFILE,'*') || strchr(OUTFILE,'?'))
         {
         zTaskMessage(10,"Invalid Output File Name '%s'\n",OUTFILE);
         BombOff(1);
         }

      zTaskMessage(2,"Opening Scratch File '%s'\n",TMPFILE);
      if ((OUTSTREAM = zOpen(TMPFILE,O_writeb)) == NULL) BombOff(1);
      }

   if (SecondFile) // The second file can be a BMP to overlay or a TSN file to use as the time base thus allowing you to plot one file against anbother
      {
      if (strchr(IN2FILE,'*') || strchr(IN2FILE,'?'))
         {
         zTaskMessage(10,"Invalid Secondary File Name '%s'\n",IN2FILE);
         BombOff(1);
         }

      zTaskMessage(2,"Opening Secondary Input File '%s'\n",IN2FILE);
      IN2STREAM = zOpen(IN2FILE,O_readb);

      if (IN2STREAM == (FILE *)NIL) BombOff(1);

      isBitmap = isBitmapImage(IN2STREAM);

      if (!isBitmap)
         {
         if (!Zgethead(IN2STREAM,&F2Header)) BombOff(1);

         switch (F2Header.type)     // Need to identify BMP files from TISAN files here at some point in the future
            {
            case R_Data:
            case TR_Data:
            case X_Data:
            case TX_Data:
               T2FLAG=1;
               if (AutoTimeScale) /* The data in the secondary file are used as the time base for the plot, so the data secondary file y-range becomes the plot t-range */
                  {
                  YMIN = YMAX = 0.0; // Need to reinitialize these in case the requested ranges don't include any data points (oops)
                  zTaskMessage(3,"Determining Secondary File Scale\n");
                  if (GETRNG(&TMIN,&TMAX,&YMIN,&YMAX,IN2STREAM,&F2Header,0)) BombOff(1);

                  if (YMIN == YMAX) zTaskMessage(10,"Time range from secondary file is invalid for autoscaling: %lG, %lG\n", YMIN, YMAX);

                  TRANGE[0] = YMIN;
                  TRANGE[1] = YMAX;
                  }
               break;
            default:
               zTaskMessage(10,"Invalid File Type.\n");
               BombOff(1);
               break;
            }
         }
      else
         {
         if (IMLOAD(IN2STREAM)) BombOff(1);
         }
      }

   if (!pBitMap && createBitmapImage()) BombOff(1); // No image loaded so we need to try and create the space for the BMP

   if (TRANGE[0] == TRANGE[1]) // This should not happen, but just in case...
      {
      ++TRANGE[1];
      --TRANGE[0];
      }

   if (YRANGE[0] == YRANGE[1])  // This should not happen, but just in case...
      {
      --YRANGE[0];
      ++YRANGE[1];
      }

   WINDOW[1] = canvasHeight - WINDOW[1];     // Bitmaps are upside down, so flip the window Y values to accommodate it
   WINDOW[3] = canvasHeight - WINDOW[3];

   plotColor = COLOR;
   COLOR = 0;              // Labels will always be in black


   if (FRAME) FRAMEIT();

   W0 = WINDOW[0];
   W1 = WINDOW[1];
   W2 = WINDOW[2];
   W3 = WINDOW[3];

   PUTLAB();
   PUTTICS();

   SCLIPXL = WINDOW[0] = W0;
   SCLIPXH = WINDOW[2] = W2;
   SCLIPYL = WINDOW[3] = W3;
   SCLIPYH = WINDOW[1] = W1;

   if (BORDER) FRAMEIT();

   COLOR = plotColor;

   if (PARMS[5])
      {
      TR1 = PLOG(TRANGE[0],5);
      TR2 = PLOG(TRANGE[1],5);
      }
   else
      {
      TR1 = TRANGE[0];
      TR2 = TRANGE[1];
      }
   if (PARMS[6])
      {
      YR1 = PLOG(YRANGE[0],6);
      YR2 = PLOG(YRANGE[1],6);
      }
   else
      {
      YR1 = YRANGE[0];
      YR2 = YRANGE[1];
      }

   TSLOPE = ((double)(WINDOW[2] - WINDOW[0]))/(TR2 - TR1);
   TINTER = (double)WINDOW[0] - TSLOPE*TR1;
   YSLOPE = ((double)(WINDOW[3] - WINDOW[1]))/(YR2 - YR1);
   YINTER = (double)WINDOW[1] - YSLOPE*YR1;

   SLT2 = LT2 = PARMS[4]/10;
   SLT1 = LT1 = (short)PARMS[4]%10;
   YPL1 = YRANGE[0];
   YPL2 = YRANGE[1];
   TPL1 = TRANGE[0];
   TPL2 = TRANGE[1];

   pChar = CatList->pList;

   for (I = 0; I < CatList->N; ++I)
      {
      FirstPointFlag = 0;
      TCNT = 0.;
      LFPS1 = LFPE1 = 0L;  // Long File Pointer Start and End

      splitPath(pChar, Drive, Dir, Fname, Ext);

      strcpy(INNAME,  Fname);
      strcpy(INCLASS, Ext);

      zBuildFileName(M_inname,INFILE);

      zTaskMessage(1,"Opening Input File '%s' for Plotting\n",INFILE);

      INSTREAM = zOpen(INFILE,O_readb);

      if (!INSTREAM || !Zgethead(INSTREAM,&F1Header))
         {
         zTaskMessage(10,"Error Reading Input File '%s'\n",INFILE);
         BombOff(1);
         }


      if (IN2STREAM) // Rewind the secondary file
         {
         zTaskMessage(1,"Rewinding Secondary Input File '%s'\n",IN2FILE);

         if (!isBitmap)
            {
            if (!Zgethead(IN2STREAM,&F2Header))
               {
               zTaskMessage(10,"Error Reading the Header from the Secondary Input File '%s'\n",IN2FILE);
               BombOff(1);
               }
            }
         else
            rewind(IN2STREAM);
         }

      switch (F1Header.type)
         {
         case R_Data:
         case TR_Data:
         case X_Data:
         case TX_Data:
            break;
         default:
            zTaskMessage(10,"Invalid File Type.\n");
            BombOff(1);
         }

      if (!T2FLAG)
         {
         while (Zread(INSTREAM,BUFFER1,F1Header.type)) PLOT1(INSTREAM);

         if (ferror(INSTREAM)) BombOff(1);
         }
      else /* Plot 2 Files Against One Another */
         {
         while (Zread(INSTREAM,BUFFER1,F1Header.type) &&
                Zread(IN2STREAM,BUFFER2,F2Header.type)) PLOT2(INSTREAM,IN2STREAM);

         if (ferror(INSTREAM) || ferror(IN2STREAM)) BombOff(1);
         }
      Zclose(INSTREAM);
      INSTREAM = (FILE *)NIL;

      if (PARMS[3] < 0)
         --PARMS[3];
      else if (PARMS[3] > 0)
         ++PARMS[3];
         
      if (PARMS[4] > 0) ++PARMS[4];

      pChar = strchr(pChar,NUL) + 1;
      }

   NOCLIP=1;
/*
** Plot is Now Complete.
** If OUTSTREAM then save the image
*/
   if (OUTSTREAM) saveBitmapImage();

   zTaskMessage(3,"%ld Data Points Processed\n",(long)TOTAL);
   zTaskMessage(3,"T Range Displayed %lG, %lG\n",TPL1,TPL2);
   zTaskMessage(3,"Y Range Displayed %lG, %lG\n",YPL1,YPL2);

   if (IN2STREAM) Zclose(IN2STREAM);
   IN2STREAM = (FILE *)NIL;

   if (OUTSTREAM)
      {
      Zclose(OUTSTREAM);
      OUTSTREAM = (FILE *)NIL;
      ERRFLAG = zNameOutputFile(OUTFILE,TMPFILE);

/*
** Use the TISAN.CFG file to spawn the BMP file viewer using the BMPVIEWER keyword
*/
      if (!ERRFLAG)
         {
         if (getConfigString("BMPVIEWER", sizeof(szBMPviewer), szBMPviewer)) // Get the system command template for viewing BMP files from the TISANB.CFG file: 'open -a preview %s&' in MacOS
            {
            addEscapes(OUTFILE);                                             // OUTFILE could have spaces in it, so we need to prefix them with a '\' character before we pass the filename to the system shell
            sprintf(szOpenCommand, szBMPviewer, OUTFILE);                    // Create the system command to launch the bmp view
            system(szOpenCommand);                                           // And launch the viewer passing the BMP file name as an argument (we hope)
            }
         }
      }

   if (pBitMap) free(pBitMap);
   pBitMap = (struct BITMAP *)NIL;

   ZCatFiles((char*)NIL); // free catalog memory

   Zexit(ERRFLAG);
   }

/************************************************************
*
* INITIALIZATION
*
*/
void MAININIT()
   {
   if ((PARMS[4] <0.) || (PARMS[4] > 99.)) PARMS[4] = 0.;
   if (PARMS[5]) TMINOR = Max(TMINOR,2);
   if (PARMS[6]) YMINOR = Max(YMINOR,2);
   if ((PARMS[5] <=0.) || (PARMS[5] == 1.))
      PARMS[5] = 0.;
   else
      PARMS[5] = log(PARMS[5]);
   if ((PARMS[6] <=0.) || (PARMS[6] == 1.))
      PARMS[6] = 0.;
   else
      PARMS[6] = log(PARMS[6]);
   if (PARMS[7] <0.) PARMS[7] = 0.;
   if (PARMS[8] <0.) PARMS[8] = 0.;

   if ((POINT[0] < 320) || (POINT[1] < 240))
      {
      POINT[0] = POINT[1] = DEFAULTDIM;
      }

   SCLIPXH = canvasWidth  = POINT[0];
   SCLIPYH = canvasHeight = POINT[1];
 
    if (!WINDOW[0] && !WINDOW[1] && !WINDOW[2] && !WINDOW[3]) // Have the default window give a little space around the border
      {
      WINDOW[0] = WINDOW[1] = 1;
      WINDOW[2] = canvasWidth - 1;
      WINDOW[3] = canvasHeight - 1;
      }

   WINDOW[0] = Max(WINDOW[0],0);
   WINDOW[2] = Max(WINDOW[2],0);
   WINDOW[1] = Min(WINDOW[1],canvasWidth - 1);
   WINDOW[3] = Min(WINDOW[3],canvasHeight - 1);

   if (WINDOW[0] >= WINDOW[2])
      {
      WINDOW[0] = 0;
      WINDOW[2] = canvasWidth;
      }
   if (WINDOW[1] >= WINDOW[3])
      {
      WINDOW[1] = 0;
      WINDOW[3] = canvasHeight;
      }

   if (!strlen(TFORMAT)) strcpy(TFORMAT,"%lG");
   if (!strlen(YFORMAT)) strcpy(YFORMAT,"%lG");

   RDataPntr1 =  (struct RData *)BUFFER1;     /* Initialize pointers */
   RDataPntr2 =  (struct RData *)BUFFER2;
   TRDataPntr1 = (struct TRData *)BUFFER1;
   TRDataPntr2 = (struct TRData *)BUFFER2;
   XDataPntr1 =  (struct XData *)BUFFER1;
   XDataPntr2 =  (struct XData *)BUFFER2;
   TXDataPntr1 = (struct TXData *)BUFFER1;
   TXDataPntr2 = (struct TXData *)BUFFER2;

   return;
   }

/************************************************************
*
* PLOT1
*     Plot a single file.
*
*/
void PLOT1(FILE *INSTREAM)
   {
   double XLOC, YLOC;

   ++TOTAL;
   switch (F1Header.type)
      {
      case R_Data:
         TIME = TCNT * F1Header.m + F1Header.b;
         DATA = RDataPntr1->y;
         FLAG = RDataPntr1->f;
         break;
      case TR_Data:
         TIME = TRDataPntr1->t;
         DATA = TRDataPntr1->y;
         break;
      case X_Data:
         TIME = TCNT * F1Header.m + F1Header.b;
         DATA = c_abs(XDataPntr1->z);
         FLAG = XDataPntr1->f;
         break;
      case TX_Data:
         TIME = TXDataPntr1->t;
         DATA = c_abs(TXDataPntr1->z);
         break;
      }

   if (!LFPS1 && ((TIME>=TRANGE[0]) || (TRANGE[1]<=TRANGE[0])))
      {
      LFPS1 = ftell(INSTREAM); /* First Value Plotted */
      TCNTS = TCNT;
      }

   if ((TRANGE[1]<=TRANGE[0]) || (TIME<=TRANGE[1]))
      {
      LFPE1 = ftell(INSTREAM); /* Last Value in File */
      TCNTE = TCNT;
      }
   ++TCNT;

   if (!FLAG)           /* Only plot good data */
      {
      if (PARMS[5]) TIME = PLOG(TIME,5);
      if (PARMS[6]) DATA = PLOG(DATA,6);
      XLOC = Tpnt(TSLOPE*TIME + TINTER);
      YLOC = Ypnt(YSLOPE*DATA + YINTER);

      if (PARMS[3]>0)
         PLTSYM(XLOC,YLOC);
      else if (!FirstPointFlag)               /* First point */
         {
         if (PARMS[3]) PLTSYM(XLOC,YLOC);
         VECTOR(XLOC,YLOC,XLOC,YLOC,0);
         FirstPointFlag = 1;
         }
      else                                    /* Rest of points */
         {
         VECTOR(0.,0.,XLOC,YLOC,1);
         if (PARMS[3]) PLTSYM(XLOC,YLOC);
         }
      }
   return;
   }

/************************************************************
*
* PLOT2
*
*/
void PLOT2(FILE *INSTREAM, FILE *IN2STREAM)
   {
   double XLOC, YLOC;

   ++TOTAL;
   switch (F1Header.type)    /* Data are Y Axis */
      {
      case R_Data:
         DATA = RDataPntr1->y;
         FLAG = RDataPntr1->f;
         break;
      case TR_Data:
         DATA = TRDataPntr1->y;
         break;
      case X_Data:
         DATA = c_abs(XDataPntr1->z);
         FLAG = XDataPntr1->f;
         break;
      case TX_Data:
         DATA = c_abs(TXDataPntr1->z);
         break;
      }

   switch (F2Header.type)    /* Data are TIME Axis */
      {
      case R_Data:
         TIME = RDataPntr2->y;
         FLAG += RDataPntr2->f;
         break;
      case TR_Data:
         TIME = TRDataPntr2->y;
         break;
      case X_Data:
         TIME = c_abs(XDataPntr2->z);
         FLAG += XDataPntr2->f;
         break;
      case TX_Data:
         TIME = c_abs(TXDataPntr2->z);
         break;
      }

   if (!FLAG)
      {
      if (PARMS[5]) TIME = PLOG(TIME,5);
      if (PARMS[6]) DATA = PLOG(DATA,6);
      XLOC = Tpnt(TSLOPE*TIME + TINTER);
      YLOC = Ypnt(YSLOPE*DATA + YINTER);
      if (PARMS[3]>0)
         PLTSYM(XLOC,YLOC);
      else if (!FirstPointFlag)
         {
         if (PARMS[3]) PLTSYM(XLOC,YLOC);
         VECTOR(XLOC,YLOC,XLOC,YLOC,0);
         FirstPointFlag = 1;
         }
      else
         {
         VECTOR(0.,0.,XLOC,YLOC,1);
         if (PARMS[3]) PLTSYM(XLOC,YLOC);
         }
      }
     return;
     }

/************************************************************
*
* Function to load an image for plot overlay
* This file will override the image size settings
* so that the old and new plots have the same dimensions
*
*/
short IMLOAD(FILE *IN2STREAM)
   {
   short ERRFLAG = 0;
   BYTE bitbucket[16];
   long byteCount;

   byteCount = filesize(IN2STREAM) - SIZEOFBFIH; // Don't load the file header into memory
   
   rewind(IN2STREAM);
   pBitMap = (struct BITMAP *)malloc(byteCount);
   
   if (!pBitMap)
      {
      zTaskMessage(10,"Mempry allocaiton failure in funciton IMLOAD.\n");
      ERRFLAG = 1;
      }
   else
      {
      fread(bitbucket,1,SIZEOFBFIH, IN2STREAM);   // read past the file info header since it does not align well in 64 bit memory
      fread(pBitMap,  1, byteCount, IN2STREAM);

      canvasWidth = pBitMap->bmpinfoHeader.biWidth;      // Current image must be same size as this one
      canvasHeight = pBitMap->bmpinfoHeader.biHeight;
      }

   if (ferror(IN2STREAM)) ERRFLAG = 1;

   return(ERRFLAG);
   }

/************************************************************
*
* Function to Frame the Current Window
*
*/
void FRAMEIT()
   {
   LINE(WINDOW[0],WINDOW[1],WINDOW[0],WINDOW[3]);
   LINE(WINDOW[0],WINDOW[3],WINDOW[2],WINDOW[3]);
   LINE(WINDOW[2],WINDOW[3],WINDOW[2],WINDOW[1]);
   LINE(WINDOW[2],WINDOW[1],WINDOW[0],WINDOW[1]);
   return;
   }

/************************************************************
*
* Function to Title the Plot and update WINDOW
*
*/
void PUTLAB()
   {
   short XLOC, YLOC;
   short VAL, IPARM;

/*
*
* Center the Title at the top or bottom and update WINDOW
*
*/
   if (PARMS[0] < 0) return;

   IPARM = PARMS[0];
   VAL = IPARM/100;
   IPARM %= 100;

   if (strlen(TITLE))
      {
      XLOC =  (W2 + W0 + 1 - strlen(TITLE)*CWIDTH)/2;
      if (VAL)
         {
         YLOC = W1;
         W1 -= (CHEIGHT + CHEIGHT/8);
         if (FRAME)
            {
            --YLOC;
            --W1;
            }
         }
      else
         {
         YLOC = (W3 += CHEIGHT);
         W3 += CHEIGHT/8;
         if (FRAME)
            {
            ++YLOC;
            ++W3;
            }
         }
      PLTALPHA(TITLE,XLOC,YLOC,0);
      }
      
   VAL = IPARM/10;
   IPARM %= 10;

   if (strlen(TLABEL))
      {
      XLOC =  (W2 + W0 + 1 - strlen(TLABEL)*CWIDTH)/2;
      if (VAL)
         {
         YLOC = (W3 += CHEIGHT);
         W3 += CHEIGHT/8;
         if (FRAME)
            {
            ++YLOC;
            ++W3;
            }
         }
      else
         {
         YLOC = W1;
         W1 -= (CHEIGHT + CHEIGHT/8);
         if (FRAME)
            {
            --YLOC;
            --W1;
            }
         }
      PLTALPHA(TLABEL,XLOC,YLOC,0);
      }

   if (strlen(YLABEL))
      {
      YLOC =  (W1 + W3 + 1 + strlen(YLABEL)*CWIDTH)/2;

      if (IPARM)
         {
         XLOC = W2;
         W2 -= (CHEIGHT + CHEIGHT/4);

         if (FRAME)
            {
            --XLOC;
            --W2;
            }
         }
      else
         {
         XLOC = (W0 += CHEIGHT);
         W0 += CHEIGHT/4;

         if (FRAME)
            {
            ++XLOC;
            ++W0;
            }
         }

      PLTALPHA(YLABEL,XLOC,YLOC,90);
      } // if (strlen(YLABEL))
      
   return;
   }

/************************************************************
*
* Function to Plot Tic Marks and Numeric Labels
*
*/
void PUTTICS()
   {
   short XLOC, YLOC;

   PTSETUP(&XLOC,&YLOC);
   PTXTICS(XLOC,YLOC);
   PTYTICS(XLOC,YLOC);

   return;
   }

/************************************************************
*
* Function for setting up values for tic marks
*
*/
void PTSETUP(short *X, short *Y)
   {
   double N, XP, YP;
   short IPARM, DSFLG, TBFLGY=0, TBFLGT=0, IOFLG;
   short XLOC, YLOC, I;
   double YY, V1T, V2T, V1Y=0., V2Y=0., XX;
   short BLENY=0, BLENT=0;


   I = fabs(PARMS[9]);
   IITA = I/1000;
   I %= 1000;
   IIYA = I/100;
   I %= 100;
   ITV = I/10;
   IYV = I%10;

   if (TMAJOR[1] < 0.)
      {
      N = floor(log10(TRANGE[1] - TRANGE[0]));
      N = pow(10.,N);
      I = ((TRANGE[1] - TRANGE[0])/N + .5);
      TMAJOR[1] = (LUTABLE[I] * N)/10.;
      TMAJOR[0] = TRANGE[1] - fabs(fmod(TRANGE[1],TMAJOR[1]));
      if (PARMS[5])
         {
         TMAJOR[1] = 1.;
         TMAJOR[0] = pow(10.,floor(PLOG(TMAJOR[0],5)));
         }
      }

   if (YMAJOR[1] < 0.)
      {
      N = floor(log10(YRANGE[1] - YRANGE[0]));
      N = pow(10.,N);
      I = ((YRANGE[1] - YRANGE[0])/N + .5);
      YMAJOR[1] = LUTABLE[I]*N/10.;
      YMAJOR[0] = YRANGE[1] - fabs(fmod(YRANGE[1],YMAJOR[1]));
      if (PARMS[6])
         {
         YMAJOR[1] = 1.;
         YMAJOR[0] = pow(10.,floor(PLOG(YMAJOR[0],6)));
         }
      }

   if ((TMAJOR[0] < TRANGE[0]) || (TMAJOR[0] > TRANGE[1]))
      TMAJOR[1] = 0.;

   if ((YMAJOR[0] < YRANGE[0]) || (YMAJOR[0] > YRANGE[1]))
      YMAJOR[1] = 0.;

   XROT = 90.;
   if (PARMS[1] >= 0.)
      {
      IPARM = (short)PARMS[1];
//      IGRID = IPARM/1000;           /* Grid Flag */
      IPARM %= 1000;
      IOFLG =  IPARM/100;           /* Outside Tics */
      IPARM %= 100;
      DSFLG = IPARM/10;             /* Double Sided Tics 0 -> Yes */
      IPARM %= 10;
      TBFLGT = IPARM;               /* Top/Bottom Exchange */
      if (TMAJOR[1])
         {
         if (!TBFLGT) /* Set WINDOW According to the Side Chosen */
            {
            YLOC = W1;
            if (!ITV) W1 -= (CHEIGHT + CHEIGHT/8);
            if (IOFLG)
               {
               W1 -= XTICBASE;
               if (!DSFLG) W3 += XTICBASE;
               }
            }
         else
            {
            if (!ITV)
               {
               YLOC = (W3 += CHEIGHT);
               W3 += CHEIGHT/8;
               }
            if (IOFLG)
               {
               W3 += XTICBASE;
               if (!DSFLG) W1 -= XTICBASE;
               }
            }
         }
      
      if (PARMS[5])       /* Log T Axis */
         {
         V1T = PLOG(TRANGE[0],5);
         V2T = PLOG(TRANGE[1],5);
         if (TMAJOR[0] > 0) TVT = PLOG(TMAJOR[0],5);
         /*
          * TMAJOR[1] now represents an exponent which is
          * the seperation of the major tic marks in log space.
          * NOTE: PARMS[5] is now log(P5)
          */
         TDELTA = exp(PARMS[5]*TMAJOR[1])/(double)(TMINOR);
         TMINOR -= 2;
         }
      else
         {
         V1T = TRANGE[0];
         V2T = TRANGE[1];
         TVT = TMAJOR[0];
         TDELTA = TMAJOR[1]/(double)(TMINOR+1);
         }
      
      if (TMAJOR[1] && ITV)
         {
         for (XX=TVT;XX >= V1T;XX-=TMAJOR[1])
            {
            if (PARMS[5]) XP = exp(XX*PARMS[5]); else XP = XX;
            sprintf(BUFFER,TFORMAT,XP);
            BLENT = Max(strlen(BUFFER)*CWIDTH,BLENT);
            }
         for (XX=TVT;XX <= V2T;XX+=TMAJOR[1])
            {
            if (PARMS[5]) XP = exp(XX*PARMS[5]); else XP = XX;
            sprintf(BUFFER,TFORMAT,XP);
            BLENT = Max(strlen(BUFFER)*CWIDTH,BLENT);
            }
         }
      else
         XROT = 0.;
      }

   YROT = 0.;
   if (PARMS[2] >= 0)
      {
      IPARM = PARMS[2];
//      IGRID = IPARM/1000;
      IPARM %= 1000;
      IOFLG = IPARM/100;
      IPARM %= 100;
      DSFLG = IPARM/10;
      IPARM %= 10;
      TBFLGY = IPARM;
      if (YMAJOR[1])
         {
         if (!TBFLGY)
            {
            XLOC = W0;
            if (IYV)
               {
               XLOC = (W0 += CHEIGHT);
               W0 += CHEIGHT/4;
               }
            if (IOFLG)
               {
               W0 += YTICBASE;
               if (!DSFLG) W2 -= YTICBASE;
               }
            }
         else
            {
            XLOC = W2;
            if (IYV) W2 -= (CHEIGHT + CHEIGHT/4);
            if (IOFLG)
               {
               W2 -= YTICBASE;
               if (!DSFLG) W0 += YTICBASE;
               }
            }
         }
      if (PARMS[6])
         {
         V1Y = PLOG(YRANGE[0],6);
         V2Y = PLOG(YRANGE[1],6);
         if (YMAJOR[0] > 0) YVT = PLOG(YMAJOR[0],6);
         YDELTA = exp(PARMS[6]*YMAJOR[1])/(double)(YMINOR);
         YMINOR -= 2;
         }
      else
         {
         V1Y = YRANGE[0];
         V2Y = YRANGE[1];
         YVT = YMAJOR[0];
         YDELTA = YMAJOR[1]/(double)(YMINOR+1);
         }
      
      if (YMAJOR[1] && !IYV)
         {
         for (YY=YVT;YY >= V1Y;YY-=YMAJOR[1])
            {
            if (PARMS[6]) YP = exp(YY*PARMS[6]); else YP = YY;
            sprintf(BUFFER,YFORMAT,YP);
            BLENY = Max(strlen(BUFFER)*CWIDTH,BLENY);
            }
         for (YY=YVT;YY <= V2Y;YY+=YMAJOR[1])
            {
            if (PARMS[6]) YP = exp(YY*PARMS[6]); else YP = YY;
            sprintf(BUFFER,YFORMAT,YP);
            BLENY = Max(strlen(BUFFER)*CWIDTH,BLENY);
            }
         }
      else
         YROT = 90.;
      }

   if (TBFLGT && ITV)
      {
      YLOC = (W3 += BLENT);
      W3 += CHEIGHT/4;
      }
   else
      W1 -= BLENT;
      
   if (TBFLGY && !IYV)
      {
      XLOC = (W2 -= BLENY);
      W2 -= CHEIGHT/4;
      }
   else
      W0 += BLENY;

   TSLOPE = ((double)(W2 - W0))/(V2T - V1T);
   TINTER = (double)W0 - TSLOPE*V1T;

   YSLOPE = ((double)(W3 - W1))/(V2Y - V1Y);
   YINTER = (double)W1 - YSLOPE*V1Y;

   *X = XLOC;
   *Y = YLOC;

   return;
   }

/************************************************************
*
* X tic Marks
*
*/
void PTXTICS(short XLOC, short YLOC)
   {
   double X, XP, V;
   short XL, TBFLG, DSFLG, IOFLG, IPARM, I, BLEN;
   short HILAB, LOLAB, IGRID;
   
   if (PARMS[1] >= 0)
      {
      IPARM = PARMS[1];
      IGRID = IPARM/1000;
      IPARM %= 1000;
      IOFLG = IPARM/100;
      IPARM %= 100;
      DSFLG = IPARM/10;
      IPARM %= 10;
      TBFLG = IPARM;
      HILAB = PARMS[7]/10;
      LOLAB = (short)PARMS[7]%10;
      
      if ((DSFLG) && (!TBFLG))
         DSFLG = 1;
      else if ((DSFLG) && (TBFLG))
         DSFLG = -1;
      
      if (!IOFLG)
         IOFLG = XTICBASE;
      else
         IOFLG = -XTICBASE;
      
      if (TMAJOR[1])
         {
         for (X=TVT,XL=X*TSLOPE+TINTER;XL <= W2;
              X+=TMAJOR[1],XL=X*TSLOPE+TINTER)
            {
            if (PARMS[5]) XP = exp(X*PARMS[5]); else XP = X;
            if (DSFLG >= 0)
               LINE(Tpnt(XL),W1,Tpnt(XL),W1-IOFLG);
            if (DSFLG <= 0)
               LINE(Tpnt(XL),W3,Tpnt(XL),W3+IOFLG);
            if (IGRID)
               LINE(Tpnt(XL),W1,Tpnt(XL),W3);
            
            sprintf(BUFFER,TFORMAT,XP);
            
            if (!ITV)
               BLEN = strlen(BUFFER)*CWIDTH/2;
            else
               BLEN = CHEIGHT/2;
            
            if (
                (((abs(XL-W2)<BLEN) && (HILAB))
                 ||   (abs(XL-W2)>BLEN))
                && (((abs(XL-W0)<BLEN) && (LOLAB))
                    ||   (abs(XL-W0)>BLEN))
                )
               {
               PLTALPHA(BUFFER,
                        (ITV ? Tpnt(XL) + BLEN : Tpnt(XL) - BLEN),
                        YLOC,XROT);
               }
            
            for (I=1;I<=TMINOR;++I)
               {
               if (PARMS[5])
                  XL = PLOG(XP*(1+I*TDELTA),5)*TSLOPE+TINTER;
               else XL = (X+I*TDELTA)*TSLOPE+TINTER;
               if (XL > W2) break;
               if (DSFLG >= 0)
                  LINE(Tpnt(XL),W1,Tpnt(XL),W1-IOFLG/2);
               if (DSFLG <= 0)
                  LINE(Tpnt(XL),W3,Tpnt(XL),W3+IOFLG/2);
               if (IGRID)
                  LINE(Tpnt(XL),W1,Tpnt(XL),W3);
               }
            }
         
         for (X=TVT,XL=X*TSLOPE+TINTER;XL >= W0;
              X-=TMAJOR[1],XL=X*TSLOPE+TINTER)
            {
            if (PARMS[5]) XP = exp(X*PARMS[5]); else XP = X;
            if (DSFLG >= 0)
               LINE(Tpnt(XL),W1,Tpnt(XL),W1-IOFLG);
            if (DSFLG <= 0)
               LINE(Tpnt(XL),W3,Tpnt(XL),W3+IOFLG);
            if (IGRID)
               LINE(Tpnt(XL),W1,Tpnt(XL),W3);
            sprintf(BUFFER,TFORMAT,XP);
            if (!ITV)
               BLEN = strlen(BUFFER)*CWIDTH/2;
            else
               BLEN = CHEIGHT/2;
            if (
                (((abs(XL-W2)<BLEN) && (HILAB))
                 ||   (abs(XL-W2)>BLEN))
                && (((abs(XL-W0)<BLEN) && (LOLAB))
                    ||   (abs(XL-W0)>BLEN))
                )
               {
               PLTALPHA(BUFFER,
                        (ITV ? Tpnt(XL) + BLEN : Tpnt(XL) - BLEN),
                        YLOC,XROT);
               }
            
            for (I=1;I<=TMINOR;++I)
               {
               if (PARMS[5])  /* Start at one down from X*/
                  {
                  V = (exp(PARMS[5]*(X-TMAJOR[1])))*(1+I*TDELTA);
                  XL = PLOG(V,5)*TSLOPE+TINTER;
                  }
               else
                  XL = (X-I*TDELTA)*TSLOPE+TINTER;
               
               if (XL > W0) /* Decades come in back door */
                  {
                  if (DSFLG >= 0)
                     LINE(Tpnt(XL),W1,Tpnt(XL),W1-IOFLG/2);
                  if (DSFLG <= 0)
                     LINE(Tpnt(XL),W3,Tpnt(XL),W3+IOFLG/2);
                  if (IGRID)
                     LINE(Tpnt(XL),W1,Tpnt(XL),W3);
                  }
               }
            }
         }
      }
   
   return;
   }

/************************************************************
*
* Y tic Marks
*
*/
void PTYTICS(short XLOC, short YLOC)
   {
   extern short W0, W1, W2, W3;
   double Y, YP, V;
   short YL, TBFLG, DSFLG, IOFLG, IPARM, I, BLEN;
   short HILAB, LOLAB, IGRID;

   if (PARMS[2] >= 0)
      {
      IPARM = PARMS[2];
      IGRID = IPARM/1000;
      IPARM %= 1000;
      IOFLG = IPARM/100;
      IPARM %= 100;
      DSFLG = IPARM/10;
      IPARM %= 10;
      TBFLG = IPARM;
      HILAB = PARMS[8]/10;
      LOLAB = (short)PARMS[8]%10;
      if ((DSFLG) && (!TBFLG)) DSFLG = 1;
      else if ((DSFLG) && (TBFLG)) DSFLG = -1;
      if (!IOFLG) IOFLG = YTICBASE; else IOFLG = -YTICBASE;
      
      if (YMAJOR[1])
         {
         for (Y=YVT,YL=Y*YSLOPE+YINTER;YL >= W3;
              Y+=YMAJOR[1],YL=Y*YSLOPE+YINTER)
            {
            if (PARMS[6]) YP = exp(Y*PARMS[6]); else YP = Y;
            if (DSFLG >= 0)
               LINE(W0,Ypnt(YL),W0+IOFLG,Ypnt(YL));
            if (DSFLG <= 0)
               LINE(W2,Ypnt(YL),W2-IOFLG,Ypnt(YL));
            if (IGRID)
               LINE(W0,Ypnt(YL),W2,Ypnt(YL));
            sprintf(BUFFER,YFORMAT,YP);
            if (IYV)
               BLEN = strlen(BUFFER)*CWIDTH/2;
            else
               BLEN = CHEIGHT/2;
            if (
                (((abs(YL-W3)<BLEN) && (HILAB))
                 ||   (abs(YL-W3)>BLEN))
                && (((abs(YL-W1)<BLEN) && (LOLAB))
                    ||   (abs(YL-W1)>BLEN))
                )
               {
               PLTALPHA(BUFFER,XLOC,Ypnt(YL) + BLEN,YROT);
               }

            for (I=1;I<=YMINOR;++I)
               {
               if (PARMS[6])
                  YL = PLOG(YP*(1+I*YDELTA),6)*YSLOPE+YINTER;
               else YL = (Y+I*YDELTA)*YSLOPE+YINTER;
               if (YL < W3) break;
               if (DSFLG >= 0)
                  LINE(W0,Ypnt(YL),W0+IOFLG/2,Ypnt(YL));
               if (DSFLG <= 0)
                  LINE(W2,Ypnt(YL),W2-IOFLG/2,Ypnt(YL));
               if (IGRID)
                  LINE(W0,Ypnt(YL),W2,Ypnt(YL));
               }
            }
            
         for (Y=YVT,YL=Y*YSLOPE+YINTER;YL <= W1;
              Y-=YMAJOR[1],YL=Y*YSLOPE+YINTER)
            {
            if (PARMS[6]) YP = exp(Y*PARMS[6]); else YP = Y;
            if (DSFLG >= 0)
               LINE(W0,Ypnt(YL),W0+IOFLG,Ypnt(YL));
            if (DSFLG <= 0)
               LINE(W2,Ypnt(YL),W2-IOFLG,Ypnt(YL));
            if (IGRID)
               LINE(W0,Ypnt(YL),W2,Ypnt(YL));
            sprintf(BUFFER,YFORMAT,YP);
            if (IYV)
               BLEN = strlen(BUFFER)*CWIDTH/2;
            else
               BLEN = CHEIGHT/2;
            if (
                (((abs(YL-W3)<BLEN) && (HILAB))
                 ||   (abs(YL-W3)>BLEN))
                && (((abs(YL-W1)<BLEN) && (LOLAB))
                    ||   (abs(YL-W1)>BLEN))
                )
               {
               PLTALPHA(BUFFER,XLOC,Ypnt(YL) + BLEN,YROT);
               }
               
            for (I=1;I<=YMINOR;++I)
               {
               if (PARMS[6])
                  {
                  V = (exp(PARMS[6]*(Y-YMAJOR[1])))*(1+I*YDELTA);
                  YL = PLOG(V,6)*YSLOPE+YINTER;
                  }
               else YL = (Y-I*YDELTA)*YSLOPE+YINTER;
               
               if (YL < W1) /* Decades come in back door */
                  {
                  if (DSFLG >= 0)
                     LINE(W0,Ypnt(YL),W0+IOFLG/2,Ypnt(YL));
                  if (DSFLG <= 0)
                     LINE(W2,Ypnt(YL),W2-IOFLG/2,Ypnt(YL));
                  if (IGRID)
                     LINE(W0,Ypnt(YL),W2,Ypnt(YL));
                  }
               }
            }
         }
      }

   return;
   }

/************************************************************
*
* Subroutine to Plot Alpha Numerics
* Characters are in FONT[] which are defined in tisanfnt.h
* format is <X><Y><PEN> where <PEN> is ASCII '1' or ASCII '0'
* if <PEN> = '\0' then end of character definition.
* All X,Y Values are defined for the 1st quad only
*
*/
void PLTALPHA(char *LABEL, short XLOC, short YLOC, short ANGLE)
   {
   short X0, Y0, X1, Y1;
   char *PNTR;
   char PEN;
   int c;

   if (!strlen(LABEL)) return;

   while (*LABEL)
      {
      c = (int)*LABEL;

      if ((c < 0) || (c > 255)) c = 0;  // Can only handle ASCII in the range 0 to 255

      PNTR = FONT[c];
      ++LABEL;
      X0 =  *PNTR & 15;
      Y0 = -(*(++PNTR) & 15);
      PEN =  *(++PNTR);
      ROTATE(&X0,&Y0,ANGLE);

      while (PEN)
         {
         X1 =  *(++PNTR) & 15;
         Y1 = -(*(++PNTR) & 15);
         PEN = *(++PNTR);
         ROTATE(&X1,&Y1,ANGLE);
         switch (PEN)
            {
            case '1':
               LINE(XLOC+X0,YLOC+Y0,XLOC+X1,YLOC+Y1);
            case '0':
            case '\0':
               X0 = X1;
               Y0 = Y1;
            }
         }
      XLOC += X0;
      YLOC += Y0;
      }

   return;
   }

/************************************************************
*
* Rotate X, Y through ANGLE (just 90 degrees CCW at the moment)
*
*/
void ROTATE(short *X, short *Y, double ANGLE)
   {
   double XP, YP;

   if (ANGLE)
      {
      XP = *Y;
      YP = -(*X);
      *X = XP;
      *Y = YP;
      }

   return;
   }

/************************************************************
*
* Find the MAX, MIN of a File
*
*/
short GETRNG(double *TMIN, double *TMAX, double *YMIN, double *YMAX, FILE *INSTREAM, struct FILEHDR *FHP, int NotFirstCall)
   {
   short ERRFLAG=0;

   TCNT = 0.;
   while (Zread(INSTREAM,BUFFER1,FHP->type))
      {
      switch (FHP->type)
         {
         case R_Data:
            TIME = TCNT * FHP->m + FHP->b;
            ++TCNT;
            DATA = RDataPntr1->y;
            FLAG = RDataPntr1->f;
            break;
         case TR_Data:
            TIME = TRDataPntr1->t;
            DATA = TRDataPntr1->y;
            break;
         case X_Data:
            TIME = TCNT * FHP->m + FHP->b;
            ++TCNT;
            DATA = c_abs(XDataPntr1->z);
            FLAG = XDataPntr1->f;
            break;
         case TX_Data:
            TIME = TXDataPntr1->t;
            DATA = c_abs(TXDataPntr1->z);
            break;
         }

      if (((TIME >= TRANGE[0]) && (TIME <= TRANGE[1])) ||
           (TRANGE[0] >= TRANGE[1]))
         {
         ++lScaleCount; // Count the number of points used for scaling

         if (!FLAG)
            {
            if (!NotFirstCall)
               {
               *YMIN = *YMAX = DATA;
               *TMAX = *TMIN = TIME;
               NotFirstCall = 1;
               }
            else
               {
               *YMAX = Max(DATA,*YMAX);
               *YMIN = Min(DATA,*YMIN);
               *TMAX = Max(TIME,*TMAX);
               *TMIN = Min(TIME,*TMIN);
               }
            }
         }
      }

   if (ferror(INSTREAM)) BombOff(1);

   return(ERRFLAG);
   }

/************************************************************
*
* Plot a Symbol
*
*/
void PLTSYM(double FXLOC, double FYLOC)
     {
     char C[2];
     short XLOC, YLOC;

     if ((FXLOC < (double)SCLIPXL) ||
         (FYLOC < (double)SCLIPYL) ||
         (FXLOC > (double)SCLIPXH) ||
         (FYLOC > (double)SCLIPYH)) return;

     XLOC = (short)FXLOC;
     YLOC = (short)FYLOC;
     C[0] = (char)fabs(PARMS[3]);
     C[1] = (char)0;
     if (C[0] != (char)255)
          PLTALPHA(C,XLOC-CWIDTH/2,YLOC+CHEIGHT/2,0);
     else
          LINE(XLOC,YLOC,XLOC,YLOC);
     return;
     }

/***************************************************************
**
** Take LOG base PRAMS[5] or PARMS[6]
*/
double PLOG(double V, short B)
   {
   V = log(V)/PARMS[B];
   return(V);
   }

/***************************************************************
**
** Vector Clipping Routine
*/
void VECTOR(double X1, double Y1, double X2, double Y2, short notFirstCall)
   {
   short VFLAG=0, HFLAG=0, FL=0;
   double M, B, ZXL, ZXH, ZYL, ZYH;
   static double LASTX, LASTY;

   if (notFirstCall)
      {
      X1=LASTX;
      Y1=LASTY;
      }

   LASTX = X2;
   LASTY = Y2;

   if ((X2 != X1) && (Y2 != Y1))
      {
      M = (Y2-Y1)/(X2-X1);
      B = Y1 - M*X1;
      ZXL = M *(double)SCLIPXL + B;
      ZXH = M *(double)SCLIPXH + B;
      ZYL =   ((double)SCLIPYL - B)/M;
      ZYH =   ((double)SCLIPYH - B)/M;
      }

   if (X2 == X1) VFLAG = 1;

   if (Y2 == Y1) HFLAG = 1;

   if (X1 < (double)SCLIPXL)
      {
      if (VFLAG) return;
      X1 = (double)SCLIPXL;
      if (!HFLAG) Y1 = ZXL;
      FL=1;
      }
   else if (X1 > (double)SCLIPXH)
      {
      if (VFLAG) return;
      X1 = (double)SCLIPXH;
      if (!HFLAG) Y1 = ZXH;
      FL=1;
      }
   if (X2 < (double)SCLIPXL)
      {
      X2 = (double)SCLIPXL;
      if (!HFLAG) Y2 = ZXL;
      FL=1;
      }
   else if (X2 > (double)SCLIPXH)
      {
      X2 = (double)SCLIPXH;
      if (!HFLAG) Y2 = ZXH;
      FL=1;
      }

   if (Y1 < (double)SCLIPYL)
      {
      if (HFLAG) return;
      Y1 = (double)SCLIPYL;
      if (!VFLAG) X1 = ZYL;
      FL=1;
      }
   else if (Y1 > (double)SCLIPYH)
      {
      if (HFLAG) return;
      Y1 = (double)SCLIPYH;
      if (!VFLAG) X1 = ZYH;
      FL=1;
      }
   if (Y2 < (double)SCLIPYL)
      {
      Y2 = (double)SCLIPYL;
      if (!VFLAG) X2 = ZYL;
      FL=1;
      }
   else if (Y2 > (double)SCLIPYH)
      {
      Y2 = (double)SCLIPYH;
      if (!VFLAG) X2 = ZYH;
      FL=1;
      }

   if (((X1<(double)SCLIPXL) || (X1>(double)SCLIPXH)  ||
        (X2<(double)SCLIPXL) || (X2>(double)SCLIPXH)  ||
        (Y1<(double)SCLIPYL) || (Y1>(double)SCLIPYH)  ||
        (Y2<(double)SCLIPYL) || (Y2>(double)SCLIPYH)) ||
       ((X1 == X2) && (Y1 == Y2) && (FL == 1))) return;

   LINE((short)X1,(short)Y1,(short)X2,(short)Y2);
   return;
   }

/********************************************
*
* Line drawing Primitive
*/
void LINE(short X1, short Y1, short X2, short Y2)
   {
   short DELX, DELY, DXI, DYI, SXI, SYI, SHRTDIS, LNGDIS;
   short SC, DC, TF;

   DELY = Y2-Y1;
   if (DELY < 0)
      {
      DELY = -DELY;
      DYI = -1;
      }
   else DYI = 1;

   DELX = X2-X1;
   if (DELX < 0)
      {
      DELX = -DELX;
      DXI = -1;
      }
   else DXI = 1;

   if (DELY > DELX)
      {
      SXI = 0;
      SYI = DYI;
      SHRTDIS = DELX;
      LNGDIS = DELY;
      }
   else
      {
      SXI = DXI;
      SYI = 0;
      SHRTDIS = DELY;
      LNGDIS = DELX;
      }

   SC = SHRTDIS<<1;
   TF = SC - LNGDIS;
   DC = TF - LNGDIS;
   ++LNGDIS;

   do {
      PSET(X1,Y1);
      if (TF<0)
         {
         X1 += SXI;
         Y1 += SYI;
         TF += SC;
         }
      else
         {
         X1 += DXI;
         Y1 += DYI;
         TF += DC;
         }
      }
   while (--LNGDIS);

   return;
   }

/********************************************
*
* Pixel Setting Routine
*
*/
void PSET(short X, short Y)
   {
   static short LASTX=-1, LASTY=-1;

   if ((X==LASTX) && (Y==LASTY)) return;

   if ((!NOCLIP &&
       ((X<SCLIPXL) || (Y<SCLIPYL) || (X>SCLIPXH) || (Y>SCLIPYH))) ||
        ((X<0) || (Y<0) || (X>=canvasWidth) || (Y>=canvasHeight))) return;

   LASTX = X;
   LASTY = Y;

   if (!SLT1)              // Line Type
      {
      if (!SLT2)
         {
         SLT1 = LT1;
         SLT2 = LT2;
         }
      else
         {
         --SLT2;
         return;
         }
      }
   else
      --SLT1;

// A color pel is made by (red << 16) | (green << 8) | blue;

   pBitMap->imageArray[(canvasHeight - Y) * canvasWidth + X] = COLOR;

   return;
   }

/***************************************************************************
**
** Test if the file is a 32 bit color bitmap, which is what DBPLOT generates
**
** The magic numbers in here are needed because the values must match the
** BMP specification, which cannot be changed.
*/
BOOL isBitmapImage(FILE *inputStream)
   {
   union {char bm[2]; WORD bfType;} bmVal;
   BOOL isBitmap = FALSE;
   struct BITMAPFILEHEADER fileHeader;
   long lpos;
   DWORD bitmapFileSize;

   lpos = ftell(inputStream);

   bitmapFileSize = (DWORD)(filesize(inputStream) - (long)SIZEOFBFIH - (long)SIZEOFBIH);
   
   rewind(inputStream);
   
   readBitmapFileHeader(inputStream, &fileHeader);

   bmVal.bm[0] = 'B';
   bmVal.bm[1] = 'M';

   if ((fileHeader.bfType      == bmVal.bfType)   && // It must be the signature word BM (0x4D42) to indicate bitmap.
       (fileHeader.bfSize      == bitmapFileSize) && // Specifies the size, in bytes, of the bitmap file.
       (fileHeader.bfReserved1 == 0)              &&
       (fileHeader.bfReserved2 == 0)              &&
       (fileHeader.bfOffBits   == 54))               //Specifies the offset, in bytes, from the BITMAPFILEHEADER structure to the bitmap bits
      isBitmap = TRUE;

   fseek(inputStream, lpos, SEEK_SET);
   
   return(isBitmap);
   }

/*
** We cannot just save the structure on 64 bit systems because the 14 byte structure is padded to 16
*/
void readBitmapFileHeader(FILE *inputStream, struct BITMAPFILEHEADER *fileHeader)
   {
   fread(&fileHeader->bfType,     2,1,inputStream);
   fread(&fileHeader->bfSize,     4,1,inputStream);
   fread(&fileHeader->bfReserved1,2,1,inputStream);
   fread(&fileHeader->bfReserved2,2,1,inputStream);
   fread(&fileHeader->bfOffBits,  4,1,inputStream);
   return;
   }

void saveBitmapFileHeader(DWORD bmpInfoHeader_biSizeImage)
   {
   memcpy(&bmpFileHeader.bfType, "BM", 2);
   bmpFileHeader.bfSize = bmpInfoHeader_biSizeImage + bmpFileHeader.bfOffBits;
   bmpFileHeader.bfReserved1 = 0;
   bmpFileHeader.bfReserved2 = 0;
   bmpFileHeader.bfOffBits = 54;

   fwrite(&bmpFileHeader.bfType,     2,1,OUTSTREAM);
   fwrite(&bmpFileHeader.bfSize,     4,1,OUTSTREAM);
   fwrite(&bmpFileHeader.bfReserved1,2,1,OUTSTREAM);
   fwrite(&bmpFileHeader.bfReserved2,2,1,OUTSTREAM);
   fwrite(&bmpFileHeader.bfOffBits,  4,1,OUTSTREAM);

   return;
   }

void saveBitmapImage()
   {
   long numBytes;
   
   numBytes = (long)SIZEOFBIH + (long)pBitMap->bmpinfoHeader.biSizeImage;
   
   saveBitmapFileHeader(pBitMap->bmpinfoHeader.biSizeImage);
   fwrite(&pBitMap->bmpinfoHeader.biSize, 1, numBytes, OUTSTREAM);

   return;
   }

BOOL createBitmapImage()
   {
   BOOL bError = FALSE;

   pBitMap = (struct BITMAP *)malloc(BMPBYTES);

   if (!pBitMap)
      {
      zTaskMessage(10,"Memory allocation failure in function createBitmapImage.\n");
      bError = TRUE;
      }

   pBitMap->bmpinfoHeader.biSize          = SIZEOFBIH;
   pBitMap->bmpinfoHeader.biWidth         = canvasWidth;
   pBitMap->bmpinfoHeader.biHeight        = canvasHeight;
   pBitMap->bmpinfoHeader.biPlanes        = 1;
   pBitMap->bmpinfoHeader.biBitCount      = TRUECOLOR;
   pBitMap->bmpinfoHeader.biCompression   = BI_RGB;
   pBitMap->bmpinfoHeader.biSizeImage     = IMAGEBYTES; // must be a multiple of 4
   pBitMap->bmpinfoHeader.biXPelsPerMeter = 0;
   pBitMap->bmpinfoHeader.biYPelsPerMeter = 0;
   pBitMap->bmpinfoHeader.biClrUsed       = 0;
   pBitMap->bmpinfoHeader.biClrImportant  = 0;

/*
** Set the background color to white
*/
   memset((char *)pBitMap->imageArray, 0xFF, IMAGEBYTES);

   return(bError);
   }

/*
** This function will add escapes before any white space characters in a string.
** it is used to properly structure a file name so it can be passed to the command shell.
**
** It is assumed that the final path will fit inside OUTFILE (and yes it can be broken with a buffer overflow)
*/
PSTR addEscapes(PSTR OUTFILE)
   {
   int i, j;
   char TEMP[_MAX_PATH];
   
   i = 0;
   j = 0;
   do {
      if (isspace(OUTFILE[i]))
         {
         TEMP[j] = BACKSLASH;
         ++j;
         }
      TEMP[j] = OUTFILE[i];
      ++j;
      ++i;
      }
   while (OUTFILE[i-1] != NUL);

   j = 0;
   do {
      OUTFILE[j] = TEMP[j];
      ++j;
      }
   while (OUTFILE[j-1] != NUL);
   
   return(OUTFILE);
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

void fcloseall()
   {
   if (INSTREAM) Zclose(INSTREAM);
   INSTREAM = (FILE *)NIL;
   
   if (IN2STREAM) Zclose(IN2STREAM);
   IN2STREAM = (FILE *)NIL;
   
   if (OUTSTREAM) Zclose(OUTSTREAM);
   OUTSTREAM = (FILE *)NIL;

   if (pBitMap) free(pBitMap);
   pBitMap = (struct BITMAP *)NIL;

   return;
   }
