/*
 This file contains some ANSI C conversion functions related to old AMIGA graphic formats
 see *.c file for further function descriptions
*/

#ifndef _conv_H_
#define _conv_H_

#include <string.h>
#include <stdint.h>   //for exact integer width


#ifdef __cplusplus
extern "C" {
#endif

//prototypes
int convertHamTo24bpp(void *dst_, void *src_,  void *cmap_, int w, int h, int hambits, int dst_pitch, int rgba);
int bitPlanarToChunky(void *dst_, void *src_,  int w, int h, int bitssrc, int bitsdst, int dst_pitch);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif


#endif
