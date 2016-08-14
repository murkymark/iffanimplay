/*
IFF Anim loader
---------------
Handles decoding of IFF animation files known from the Amiga

License: GNU LGPL v3 (http://www.gnu.org/licenses/lgpl.html)
Author:  Markus Wolf (http://murkymind.de)
Version: see preprocessor definition below
Date:    see preprocessor definition below
*/

/*
This C++ class provides neccessary methods for decompressing IFF-ANIM animations.

Internal frame Management:
-----------------
 The Animation data is read to memory completely, into a frame list:
   -> frames are decoded faster (delta compression is kept in memory, RLE not)
   -> verification of data before playing, no file reading errors during play (except compression errors)
   - I haven't seen an iff animation with more than 10MB, so this shouldn't be a problem.

 internal frame data:
   There is the "current frame" and the "previous frame" in bitplanar format representing a kind of double buffer
   When a frame is decoded, the delta frame information modifies/updates the previous frame always resulting in the
   new current frame.
   The "current frame" can be obtained as chunky converted, suitable for modern hardware.
   When opening a file, the first bit planar frame is decoded (frame 0).
   A call to "NextFrame" decodes the next frame (frame 1). The chunky conversion is made on request only.


Output formats
--------------
 - 1..8 bit are converted to 8 bit, with a "r,g,b,0" palette (byte order exactly as mentioned)
 - HAM and 24 bit frames are converted to 24 bit "r,g,b" images (byte order as mentioned)
 - pitch for a scanline of the converted frame as requested (should never be lesser than the relevant pixel data of a scanline)
 - the raw bit planar frame data can be returned via a function additionally


Player timing:
---------------------
 While waiting the delay of a frame, it is useful to decode the next frame already.
 Add the passed time for the decoding to your already waited time.
 A timer with a resolution of at least 1/60 seconds is needed (floating point variable recommended).



File format support:
--------------------

ANIM file here is currently only supported in it's standart structure:
(I haven't seen any other structure so far)

 FORM ANIM
  FORM ILBM        first frame (frame 0)
    BMHD             normal type IFF data
    ANHD             optional animation header (chunk for timing of 1st frame)
    ...            
    CMAP             (cmap optional for frames > 0)
    BODY             normal iff pixel data
  FORM ILBM        frame 1
    ANHD             animation header chunk
    DLTA             delta mode data
  FORM ILBM        frame 2
    ANHD
    DLTA
    ....

known delta compression modes (current decoding support is marked with '*'):
  - ANIM-0   * ILBM BODY (no delta compression, almost always RLE ("cmpByteRun1") compressed)
  - ANIM-1     XOR (full frames)
  - ANIM-2     Long Delta mode
  - ANIM-3     Short Delta mode
  - ANIM-4     General Delta mode
  - ANIM-5   * Byte Vertical Delta mode
  - ANIM-6     Stereo Byte Delta mode
  - ANIM-7   * like Anim-5 compression using LONG/WORD data
  - ANIM-8   * like Anim-5 compression using LONG/WORD data
  - ANIM-74  * Eric Graham's compression format (known as compression 'J' or just "Movie", used by DILBM/PILBM, "Sculpt 3D" / "Sculpt 4D" by "Byte by Byte")
  - ANIM-100 * ANIM32: Long Vertical Mode (used by Scala Multimedia and InfoChannel)
  - ANIM-101 * ANIM16: Short Vertical Mode (used by Scala Multimedia and InfoChannel)
 * short: 16bit, long: 32bit units
 * "dctv" animations aren't supported yet, due to lacking format information. (information wanted)
     -> appears as ANIM-5 compression method (how to auto detect?), but colours are wrong 
     - there is old software for conversion with a proprietary library (http://amiga.resource.cx/exp/dctv)
 
 => sample files and descriptions of compression/decoding algorithms are helpful to add missing format support


TODO:
 SafeMemory for all decompressors?

History:
--------
 * 14-Aug-2016
   - Issue with multiple color maps in ANIM file fixed
     (reported by Piotr Bandurski)

 * 20-Jan-2015
   - Support for compression mode 100 (ANIM16) and 101 (ANIM32) added (used by Scala Multimedia/MM400 and InfoChannel/IC500)
     (requested & format info by Dimitris Panokostas)
   - SafeMemory class added to prevent memory access violation for corrupt DLTA data
     -> may slow down a little; simplifies decompression code with (multi)byte access methods

 * 22-Apr-2008
   - serveral changes (internal decoding buffer removed for simplification)
   - "GetPrevFrame()" removed -> not very useful

 * 30-mar-2007:
   - "GetPrevFrame()" added

 * 20-nov-2006:
   - Initial release
*/


#ifndef _iffanim_H_
#define _iffanim_H_

#define IFFANIM_VERSION "2016-08-14"


#include <iostream>
#include <fstream>
#include <stdint.h>    //integer type definitions

using namespace std;




#define IFFANIM_ERRORSTRING_BUFSIZE 3000  //string buffer size
#define IFFANIM_FORMATINFO_BUFSIZE  2000  //size of string buffer for returning information about ANIM file


//IFF ANSQ chunk ("animation sequence"?)
//contains an array of DLTA frame references
//found at the very end of all ANIM-J files
//DLTA frames shall be decoded in that order or grahics can become glitched (see ANIM "BoingThrows" which uses inside ping pong sequence)
//DLTA encoder should have taken the ANSQ order into account upon creation
//A DLTA can appear multiple times within one play while the previous frame differs
//used to play animations in loops and ping pong
// -> no official specification found, just the source code of "XAnim" and "cvtmovie" (with notes on "https://home.comcast.net/~erniew/cghist/cvtmovie.html")
// "reltime" of "ANHD" has to be ignored
//chunk probably used to reduce file size by encoding repeated frames only once
typedef struct
{
  uint16_t dindex;  // index into the delta array
  uint16_t jiffies; // duration in jiffies (1/60 sec)
} IFF_ANSQ;


//struct for a single ANIM frame in memory
// contains needed anim header information for decompression
// some fields are not used currently
class iffanim_frame
{
 public:
 
 int   delta_compression;  //determines compression type/method

 int   mask;       //for XOR mode only (compression 1)
 int   w;          //XOR mode only
 int   h;          //XOR mode only
 int   x;          //XOR mode only
 int   y;          //XOR mode only

 int     reltime;    //relative display time in 1/60 sec (Jiffies)
 int     interleave; //indicates how many frames back the data is to modify
 uint32_t bits;       //option flags for some compressions (already in system byte order)
 
 char *cmap;          //original cmap (if exists), else NULL, number of color entries depends on bits per pixel resolution
 char *data;          //original pixel data from file (maybe compressed)
 int   datasize;      //size of data in bytes
 

 streamsize audiopos; //absolute position starting with this frame
 int   audiosize;     //size of audiodata
 char *audiodata;     //audio data of this frame



 //print bit flags (useful for debugging)
 //print as bit string (most significant left), and listing of meaning
 //some flags are unused for specific compressions (shall be 0 => not meaning anything)
 //flags stored as 32 bit Big Endian int in file, here already converted to system byte order
 void printBits(){
   printf("Flags of frame (32 bit int): ");
   
   //print bits as bitstring
   int nbits = sizeof(this->bits) * 8; //32
   for(int i = 0; i < nbits; i++) { //for 4 bytes
     printf("%d", (bits >> (nbits-1 - i)) & 0x1);
     if((i+1) % 8 == 0)
         printf(".");
   }
 
   printf("\n   ===  0  /  1  ==\n");
   printf(" 0 short/long data: %d\n", (bits >> 0) & 0x1);
   printf(" 1 set/XOR: %d\n", (bits >> 1) & 0x1);
   printf(" 2 separate info for each plane / one info list for all planes: %d\n", (bits >> 2) & 0x1);
   printf(" 3 not RLC / RLC (run length coded): %d\n", (bits >> 3) & 0x1);
   printf(" 4 horizontal/vertical: %d\n", (bits >> 4) & 0x1);
   printf(" 5 short/long info offsets: %d\n", (bits >> 5) & 0x1);
   printf(" 6 interlace: %d\n", (bits >> 6) & 0x1);  //used by compression 100/101 only
 }
 
};




//struct for embedded audio created with Amiga software "Wave Tracer DS"
struct iffanim_audio
{
 int   freq;      //playback sample frequency 
 int   nch;       //number of channels: 0 no audio, 1 mono, 2 stereo (left, right interleaved), other values aren't supported
 int   bps;       //bits per sample point
 float volume;    //volume: 0..1

 int   n;           //equal to nframes (the last frame data may contain 2 SBDY chunks, or somewhere else for joined animations)
 char *data;        //audio buffer (Big Endian byte order, signed)
 int   datasize;    //total audio data size in bytes
 int  *dataoffset;  //list of audio sample start positions, for each frame in bytes
};



class IffAnim
{
 //>>>>>>>> attributes
 protected:
  //animation attributes
  int  w,h,bpp;         //width, height, bits per pixel of anim (original format)
  int  mask;            //indicates mask type defined in BMHD chunk (0,1,2,3)
  bool ham;             //"true" if ham mode
  bool ehb;             //"true" if extra half bright palette
  int  compressed;      //indicates compression type of first frame: 0 or 1 (ByteRun) (other frames have delta compression usually)
  unsigned char dcompressions[32]; //bit array indicates which delta compression methods are used in the anim (there may be mixed compression modes)
 
  int    nframes;       //number of frames
  int    lentime;       //overall length of animation in 1/60 seconds (original format)
  struct iffanim_frame *frame;  //list containing all frames (original format, still delta compressed)

  int   disp_bpp;         //display (output) format (constant for all frames) (1..8 bit => 8, HAM or more bits => 24 bpp)
  char  disp_cmap[256*3]; //color map buffer, holds color map for current 8 bit converted frame

  //frame attributes
  char *prevframe;   //frame before current (internal/original frame format)
  char *prevcmap;    //pointer to cmap of previous frame
  char *curframe;    //buffer for decoded frame, in original format (multiplanar)
  char *curcmap;     //ptr to cmap in frame list, can be redefined for a frame
  int   framesize;   //size of a decoded frame in bytes (each scanline has a multiple of 16 bit -> number of bytes is even)
  int   frameno;     //current frame (as number starting with 0)

  iffanim_audio audio;  //contains audio data, if supported

//  char *xor_buffer;     //used for XOR delta compression (mode 1) only, can hold a RLE decompressed XOR map

  bool loopanim;        //if set, when looping, the next frame of the last frame is frame 2, else frame 0
  bool loop;            //if unset, an error is returned when trying to load the next frame of the last frame, else the animation continues with the first frame
  bool file_loaded;     //indicates if a file is currently loaded
  bool ilbm;            //indicates if file is IFF ILBM, not an ANIM file

  bool interlace_detected; //currently only for ANIM-100/101

  bool follow_ansq;        //if requested by player parameter, else stored DLTA order is frame order
  IFF_ANSQ *ansq; //ANSQ array
  int ansq_size; //size (elements) of ANSQ array


  char formatinfo[IFFANIM_FORMATINFO_BUFSIZE];       //buffer for returning format information
  char errorstring_tmp[IFFANIM_ERRORSTRING_BUFSIZE]; //buffer for temporary string formatting
  string errorstring;   //error information



  //>>>>>>>> internal methods
 protected:
  //init attributes to default values (after closing, object creation)  
   void InitAttributes();

  //decode compressed data ("static" to make sure functions contain portable code only)
   static int DecodeByteRun(void* dst, void* data, int datasize, int w, int h, int bpp, int mask);  //ILBM byte run (RLE), normally used for the first frame (key frame) only
   //add compression 2 here
   //add compression 3 here
   static int DecodeGeneralDelta(char* dst, void* data_, int w, int bpp);                           //delta compression method 4 (untested!)
   static int DecodeByteVerticalDelta(char* dst, void* data, int w, int bpp);                       //delta compression method 5 (byte vertical delta with skip)
   //add compression 6 here
   static int DecodeLSVerticalDelta7(char* dst, void* data_, int w, int bpp,  bool long_data);      //delta compression method 7 (long/short vertical delta)(with 16 or 32 bit words)
   static int DecodeLSVerticalDelta8(char* dst, void* data_, int w, int bpp,  bool long_data);      //delta compression method 8 (long/short vertical delta)(with 16 or 32 bit words)
   static int DecodeDeltaJ(char* dst, void* data_, int  w, int  h, int  bpp);                       //delta compression method 'J' (74) by Eric Graham
   static int DecodeScalaAnim(char* dst, void* data_, int datasize, int  w, int  h, int bpp, bool long_data, bool interlace, bool odd);  //delta compression method 100/101 (Scala ANIM32/ANIM16)
   
   
   int  DecodeFrame(char* dstframe, int index);           //decode frame from frame list to dstframe (bitplanar, multiple of 16 bit per plane), ignores possible mask plane
   int  FindChunk(fstream* file, const char* idreq, int len);   //find a requested chunk in iff file
   int  GetNumFrames(fstream* file);   //get number of frames (verify file struct)
   int  ReadFrames(fstream* file);     //read anim frames and info to buffer (calculate animation length)
   void PrintInfo();                   //print file format information to info string
   
  //read chunks to mem
   void read_ANHD(fstream* file, iffanim_frame* frame);     //read information from animation header chunk to "frame" (list entry)
   void read_CMAP(fstream* file, iffanim_frame* frame);     //read color map from chunk to to "frame"
   int  read_SBDY(fstream* file, int searchlen, char** audiobuf, int* audiobufsize); //read audio data

   int  InterleaveStereo(char* data, int datasize, int bps);    //interleave audio (to common format) from separate channels (8 or 16 bit only), returns new array or NULL on error

   void SwapFrameBuffers();


 //>>>>>>>>> interface methods
 public:    
       IffAnim();         //constructor
      ~IffAnim();         //destructor
  void Close();           //frees all buffers   
  int  Open(const char* fname); //open and read anim file to memory, returs 0 on success, -1 on error (file is closed after reading data successfully to mem)
  bool Loaded();          //returns true if a animation is loaded

  int  Reset();           //reset state to frame 0, also called once after opening a file
        
  bool NextFrame();       //decompress next frame to internal buffer (get new cmap), incr. frame counter

  float GetDelayTime();          //get the delay time of the current frame in seconds
  int   GetDelayTimeOriginal();  //get delay time in 1/60 seconds (original format stored in the file)

  bool SetLoopAnim(bool state);  //Can only be manually set; is false by default; useful for "loop animations" with the 2 first frames and the 2 last beeing the same
                                 // the animation won't stop at the end but continue with frame 2, the 2 last frames are considered to be the first ones after the end is reached
  void SetLoop(bool state);      //the animation won't loop by itself, if deactivated, but looping can also be controlled by the host programm via "Reset()"
                                 // looping is always activated by default

  char  *GetInfoString();       //return format info string about the anim file
  const char *GetCompressionName(int comp); //return name of compression number
  string GetError();             //return error log;
  void   ClearError();          //empty the internal error log

  char *GetFramePlanar(int* framesize);   //return pointer to decoded bitplanar frame (raw), frame size will be set to the number of bytes the frame takes                              
  void *GetFrameChunky(void* framebuf, int pitch);  //decoded frame data is converted and written in chunky format to "framebuf" with requested pitch (size of scanline in bytes); for bits per pixel refer to "GetInfo"; returns NULL on error else "framebuf";  8 bit (only if original <= 8, no HAM) or 24 bit (order: r,g,b,r,g,b,...)
  void *GetCmap();                        //returns cmap pointer of current frame; returns NULL if no cmap exists; format is: R,G,B,R,... (3 bytes per color entry); ncolors = 1 << bpp (but 256 color entries are always save to access); may change for the next frame

  int   GetInfo(int* w, int* h, int* bpp_out, int* numframes, int* lenms);  //get format info (decodingd format); "bpp_out" = bits per pixel of output format! -> "GetFrame()"; length of animation in ms; NULL pointers are ignored
  int   CurrentFrameIndex();              //return current frame number (starting at 0)

  bool  IsILBM(){ return ilbm; }


  int   GetAudioFormat(int* nch, int* bps, int* freq);  //returns 0 if audio is available, else -1, parameters with NULL are ignored (always 8 bit signed PCM)
  char *GetAudioData(int* size);          //returns pointer to audio data array, NULL if not available, stores size in bytes to the pointer
  int   GetAudioOffset(int index);        //returns byte offset in audio data array to specific frame
  int   GetAudioOffset(int index, int msoffs); //return the audio offset of: the frame relating to the "index" + "msoffs" (in millisec), "msoffs" may be < 0 and also return value, useful to resynchronize audio to video
 
  int   GetFrameAudioSize(int index);     //return audio data size in bytes for specific video frame, size of data which is played during the video frame
  int   GetFrameAudioSize();              //audio data size of current frame
  int   GetFrameAudio(char* abuf, int bufsize, int offs);       //fills the buffer with audio data from the current frame; "bufsize" limits the written data; "offs" is the byte offset from the start of the frame data; returns written bytes


  //parse and print list of chunk IDs (indented, in the order of appearance)
  //for debug
  //file is rewound (when starting and when returning)
  void parseIFF(fstream* file);
};





#endif
