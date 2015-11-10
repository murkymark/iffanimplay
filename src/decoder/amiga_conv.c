#include "amiga_conv.h"


//macros for optimized copy operations
// s = source pointer, d = destination ptr
#define COPY24BIT(s,d)    { ((uint16_t*)(d))[0] = ((uint16_t*)(s))[0]; ((uint8_t*)(d))[2] = ((uint8_t*)(s))[2]; }
#define COPY32BIT(s,d)    { ((uint32_t*)(d))[0] = ((uint32_t*)(s))[0]; }



/***
 HAM lookup tables for mapping component values of lower bit precision to values of higher bit precision
 "http://de.wikipedia.org/wiki/Hold-And-Modify_Modus" says it should be mapped (instead using a simple shift)
 The value 255 cannot be reached with simple shifts.

 Table generation:
   min energy: 0.0 =>   0 (8bit)
   max energy: 1.0 => 255 (8bit)
   other values are linear interpolated

   LowBitsMaxVal == "2^4 -1" (for 4 bit => HAM6) , "2^6 -1" (for 6 bit => HAM8)
   HigBitsMaxVal == "2^8 -1" (8 bit per component,RGB)
   for(int i = 0; i <= LowBitsMaxVal; i++)
     lut[i] = (HigBitsMaxVal * i / LowBitsMaxVal);
***/

unsigned char ham6_lut[16] = {    //4 bit per direct component value mapped to 8 bit
     0, 17, 34, 51, 68, 85,102,119,136,153,170,187,204,221,238,255 };
unsigned char ham8_lut[64] = {    //6 bit per direct component value mapped to 8 bit
     0,  4,  8, 12, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60,
    64, 68, 72, 76, 80, 85, 89, 93, 97,101,105,109,113,117,121,125,
   129,133,137,141,145,149,153,157,161,165,170,174,178,182,186,190,
   194,198,202,206,210,214,218,222,226,230,234,238,242,246,250,255};



//******************************************************************************
// Convert HAM6 or HAM8 to 24 bit chunky
// -color map is needed 
// -we use 32 bit operations to copy 24 bits!
//  => make sure the 24 bit destination buffer has an extra byte at the end when "rbga=0", a single padding byte is enough => byte can be ignored for further processing
//
// -returns "-1" on failure, 0 on success
// -"src_" must be in original bit planar format (pitch is mutliple of 16 bit), destination is chunky
// -"dst_" must point to a large enough destination buffer
// -"w" and "h" are width and height in pixels
// -"cmap_" pointer must point to a valid color map (R,G,B triplets, 8 bit per component, 24 bit per color entry)
// -"hambits" must be 6 or 8
// -"dst_pitch" is the number of bytes each scanline takes (including padding bytes at the end), set to "0" to use the minimal pitch (without padding)
// -"rgba" if 0 => RGB; if 1 => RGBA (32bit -> buffer must be large enough), the alpha byte is set to 255
int convertHamTo24bpp(void *dst_, void *src_,  void *cmap_, int w, int h, int hambits, int dst_pitch, int rgba)
{
 unsigned char colbuf[4];      //color component buffer: R,G,B(,A)
 if(rgba)
   colbuf[3] = 255;   //constant alpha value for RGBA ("255" means "no transparency")
 else
   colbuf[3] = 0;   
 int cbytes = 3;      //use 3 bytes of color buffer, also used for offset calculation in destination
 if(rgba) cbytes = 4; //use 4 bytes of color buffer (RGBA)
    
    
 if((hambits != 8) && (hambits != 6))  //only HAM6 or HAM8 allowed
   return -1;
 if(rgba != 0  && rgba != 1)
   return -1;
 if(dst_pitch <= 0)
   dst_pitch = w * cbytes;
 if(dst_pitch < (w * cbytes))  //must be large enough
   return -1;

 unsigned char *clut;
 if(hambits == 6)
   clut = ham6_lut;
 else  //8 hambits
   clut = ham8_lut;

 int i,j,k;         //loop counter
 int data, mode;    //parts of a HAM value
 char* cmap = (char*)cmap_;
 
 unsigned char *dstL = (unsigned char*)dst_; //start of current line in destination
 unsigned char *srcL = (unsigned char*)src_; //start of current line in source
 unsigned char *src,*dst;                    //working pointers             

 int bitplane_pitch = (w + 15) / 16 * 2;     //pitch of a bitplane multiple of 16 bit, in bytes
 int src_pitch = bitplane_pitch * hambits;   //pitch of line of all bitplanes
 
 int modeshift = hambits - 2;           //to shift mode bits into lowest position
 int datamask =  (1 << modeshift) - 1;  //to mask mode bits
 int datashift = 10 - hambits;          //to shift the data to upper bits so it can be used as 8 bit color component value (not needed when using the lookup tables)


 for(j = 0; j < h; j++)  //for every line
 {
   dst = dstL;
   COPY24BIT(cmap,colbuf);   //set color buffer to border color (index 0 => first 3 bytes)
   for(i = 0; i < w; i++)    //for every pixel of line
   {

     data = 0;
     int bitpos = i;               //bit position in a plane of the soure data
     src = srcL + (bitpos / 8);    //position of the first byte to read
     bitpos = 7 - (bitpos & 0x7);  //first bit in a byte is the most left bit
     for(k = 0; k < hambits; k++)  //collect all bits from the different planes for a complete pixel
     {
       data |= ((*src >> bitpos) & 0x1) << k;  //shift to bit position 0 to mask, then to it's right position
       src += bitplane_pitch;
     }

     mode = data >> modeshift;   //upper 2 bits
     data = data & datamask;     //lower 6 or 4 bits

     //change color component
     switch(mode) {
       case 0x0: COPY24BIT(cmap +(data*3), colbuf); break; //RGB values from color map to color buffer
       case 0x1: colbuf[2] = clut[data]; break;    //change blue upper bits (color value is shifted to full 8 bits so it can be set directly)
       case 0x2: colbuf[0] = clut[data]; break;    //change red upper bits
       case 0x3: colbuf[1] = clut[data]; break;    //change green upper bits
     }
     COPY24BIT(colbuf,dst);  //write color buffer to dst (we don't use a 32 bit operaton or the dst array would have to have +1 byte in size for RGB => an access violation can occure easily)
     dst += cbytes;

   }
   dstL += dst_pitch;
   srcL += src_pitch;
 } 
 return 0;
}




//******************************************************************************
// Convert bit planar frame to chunky
// -expands also bits per pixel => low to high only, make sure: "bitssrc <= bitsdst"
//  => bits in chunky mode are stuffed one after another by the function => byte borders are ignored except for start of a new scanline
// -7/6/5/3 bits per pixels are not suitable for chunky modes!
//
// -"dst_" destination pointer
// -"src_" source pointer
// -"w" and "h" width and height in pixels
// -"bitssrc" bit planes in source
// -"bitsdst" bits per chunk in destination
// -"dst_pitch" is the number of bytes each scanline takes (including padding bytes at the end), set to "0" to use the minimal pitch (without padding)
int bitPlanarToChunky(void *dst_, void *src_,  int w, int h, int bitssrc, int bitsdst, int dst_pitch)
{
 unsigned char *dst = (unsigned char *)dst_;
 unsigned char *src = (unsigned char *)src_;

 if(bitssrc > bitsdst) return -1;
 if(dst_pitch == 0)
   dst_pitch = ((w * bitsdst + 7) / 8);        //integer number of bytes for each scanline
 else if(dst_pitch < ((w * bitsdst + 7) / 8))  //must be large enough (else better leave because buffer probably too small)
   return -1;

 int i,j,k;       //loop counters
 int bitval;      //for storing a bit
 int srcBitOffs;  //offset from beginning of line of source in bits
 int dstBitOffs;  //offset from beginning of line of destination in bits

 int BitPlaneRowLen = ((w + 15) >> 4) << 4;   //lenth of a line in a single bit plane, in bits
 
 int LineLenSrc = (BitPlaneRowLen >> 3) * bitssrc; //in bytes
 int LineLenDst = dst_pitch;      //in bytes
 
 int padbits = bitsdst - bitssrc; //number of padding bits per pixel (for bpp conversion)


 for(k = 0; k < h; k++)  //for all lines
 {
     memset(dst, 0, LineLenDst);   //clear line
     dstBitOffs = 0;
     for(j = 0; j < w; j++)  //for all pixels of a line
     {

        srcBitOffs = j;
        int downshift = 7 - (srcBitOffs & 0x7);   //bit order in a bit plane byte: 0x1 masks bit 7, 0x80 masks bit 0 of color index
        for(i = 0; i < bitssrc; i++)  //for all bits of a pixel
        {
            bitval = (src[srcBitOffs >> 3] >> downshift)  &  0x1;   //get bit from bitplane, shift to lowest position and mask
            dst[dstBitOffs >> 3] |= bitval << i;                    //write bit to chunky buffer
            dstBitOffs++; 
            srcBitOffs += BitPlaneRowLen;
        }
        dstBitOffs += padbits;
     }

     src += LineLenSrc;
     dst += LineLenDst;
 }
 
 return 0;
}
