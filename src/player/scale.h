/// Scale functions, writing scaled source image to destination
/// See *.c file for function descriptions
///
/// Nearest neighbour:
///   fastest software scaling uses fixed point integer as
///     * increments: pixel distance of 1 in the scaled image equals the scale factor in the source image
///     * and accumulator/sum: represents coordinate in source surface
///   only addition and bitshift needed per pixel in destination
///   looks best with exact multiples of original dimension
///   upscaled: some lines and and rows are multiplied 1 more than others
///   downscaled: rows and lines of picles are skipped
///


#ifndef _scale_H_
#define _scale_H_

#include <string.h>
#include <stdint.h>   //for exact integer width



#ifdef __cplusplus
extern "C" {
#endif

void ScaleBitmap8(char *dst, int dstw, int dsth, int dstpitch, char *src, int srcw, int srch, int srcpitch);
void ScaleBitmap24(char *dst, int dstw, int dsth, int dstpitch, char *src, int srcw, int srch, int srcpitch);

#ifdef __cplusplus
} /* closing brace for extern "C" */
#endif


#endif
