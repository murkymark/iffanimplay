#include "scale.h"


//macros: s = source pointer, d = destination ptr
#define COPY24BIT(s,d)    { ((uint16_t*)(d))[0] = ((uint16_t*)(s))[0]; ((uint8_t*)(d))[2] = ((uint8_t*)(s))[2]; }
#define COPY32BIT(s,d)    { ((uint32_t*)(d))[0] = ((uint32_t*)(s))[0]; }


/******************************************************************************/
// - scale 8 bit bitmap to requested size, pitch conversion
// - use precalced step -> fixedpoint 16.16
//
// dst: destination to scale to
// src: source
// w: width
// h: height
// pitch: length of a scanline
void ScaleBitmap8(char *dst, int dstw, int dsth, int dstpitch, char *src, int srcw, int srch, int srcpitch)
{
 int i, j;

 if(dstw <= 0 || dsth <= 0 || srcw <= 0 || srch <= 0 || dst == NULL || src == NULL)
   return;

 //if simple copy can be done
 if((srcpitch == dstpitch) && (srcw == dstw) && (srch == dsth)) {
   memcpy(dst, src, srcpitch * srch);
   return;
 }
   
 //fixed point 16.16 precision
 uint32_t xratio = (srcw << 16) / dstw;  //step in src for step of 1 in dst
 uint32_t yratio = (srch << 16) / dsth;
 
 uint32_t xofs;    //x position in source
 uint32_t yofs;    //y position in source

 //holds address of line start in src
 char *srcT = src;

 yofs = 0;
 for(j = 0; j < dsth; j++) {
   src = srcT + ((yofs >> 16) * srcpitch);
   xofs = 0;
   for(i = 0; i < dstw; i++) {  //for every pixel of a line in dst, get the corresponding pixel in src
     dst[i] = src[xofs >> 16];
     xofs += xratio;
   }
   yofs += yratio;
   dst += dstpitch;
 }
}


/******************************************************************************/
// - scale 24 bit bitmap to requested size, pitch conversion
// - use precalced step: fixedpoint 16.16
//
// dst: destination to scale to
// src: source
// w: width
// h: height
// pitch: length of a scanline
void ScaleBitmap24(char *dst, int dstw, int dsth, int dstpitch, char *src, int srcw, int srch, int srcpitch)
{    
 int i, j;
 
 if(dstw <= 0 || dsth <= 0 || srcw <= 0 || srch <= 0 || dst == NULL || src == NULL)
   return;
   
 //if simple copy can be done
 if((srcpitch == dstpitch) && (srcw == dstw) && (srch == dsth)) {
   memcpy(dst, src, srcpitch * srch);
   return;
 }
 
 //fixed point vars 16.16
 uint32_t xratio = (srcw << 16) / dstw;
 uint32_t yratio = (srch << 16) / dsth;
 
 uint32_t xofs;     //x position in source
 uint32_t yofs;     //y position in source

 char *srcT = src;  //save adress of first line start

 int src_lineoffs;
 dstw *= 3; //pixels -> bytes

 yofs = 0;
 for(j = 0; j < dsth; j++) {       //for every line in dst
   src = srcT + ((yofs >> 16) * srcpitch);  //set line start of line
   xofs = 0;
   for(i = 0; i < dstw; i += 3) {  //for every pixel of a line in dst, get the corresponding pixel in src
     src_lineoffs = (xofs >> 16) * 3;
     COPY24BIT((src + src_lineoffs), (dst + i));  //optimized
     xofs += xratio;
   }
   yofs += yratio;
   dst += dstpitch;
 }
}
