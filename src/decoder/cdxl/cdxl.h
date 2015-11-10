/*
 CDXL loader
 -----------
 Purpose: handles decoding of CDXL animation files known from the Amiga

 License: GNU LGPL v3 (http://www.gnu.org/licenses/lgpl.html)
 Author:  Markus Wolf (http://murkymind.de)
 Version: see preprocessor definition below
 Date:    see preprocessor definition below
*/
/*
 information about CDXL: http://en.wikipedia.org/wiki/CDXL

 CDXL consists of a simple list of chunks, each containing a frame and the relating
 sound data. Normally the format is constant within a file (AFAIK format changes aren't allowed within a file).
 1..8 bits per pixel are always returned as 8 bit (with cmap), or when >8 as 24 bit RGB.
 Variable chunk sizes might be possible but will cause a lot of problems (quick seeking fails)

 Player requirement:
  - Playing backward (audio data has to be reversed by the player).
  - the CDXL class implements only the simplest way to read and interpret the data
      There is no special buffering implemented, this is up to the player  
  - the playing speed (frames per second or audio sample rate) is not defined in the data, so
    only a assumed value can be returned, speed should be configurable in the player
    => common fps rate is 12
  - fps and sample rate must match exactly or result in asynchronous playback
  - signed 8 bit audio always:
      If the first chunk has no audio the bitrate cannot be calculated and results in "0" => no audio.
      Normally each chunk should have the same amount of audio data.
      To make sure a CDXL has no audio at all, all frame chunks would have need to be parsed. => a special method is provided
      

to implement:
- error message if frames are not numbered properly
- warning if format change detected in a chunk
- if the player can't display the frames fast enough, an audio buffer underun should be avoided by feeding "silence" => causes unpleasant gaps
*/


#ifndef _cdxl_H_
#define _cdxl_H_

#define CDXL_VERSION "1.00, 22-Apr-2008"


#include <iostream>
#include <fstream>
#include <string.h>
#include <sstream>
#include <stdint.h>    //Exact-width integer types

using namespace std;







#define CDXL_CHUNKHEADSIZE   32

#define CDXL_INFOSTRING_BUFFER  512



enum enum_type {  //byte 0 of a chunk
 CDXL_CUSTOM   = 0x0,
 CDXL_STANDART = 0x1,
 CDXL_SPECIAL  = 0x2
};


enum enum_venc {  //bit 0..1 of info byte, video encoding
 CDXL_RGB      = 0x00, //is chunky and planar in RGB possible (one at a time) ?
 CDXL_HAM      = 0x01, // bitplanes can only be 6 or 8 here
 CDXL_YUV      = 0x02, //standart YUV as in http://en.wikipedia.org/wiki/YUV ?
 CDXL_AVM_DCTV = 0x03  //???
};

enum enum_aud { //bit 4 of info byte, audio mode
 CDXL_MONO   = 0x00,
 CDXL_STEREO = 0x10
};

enum enum_por {  //bit 5..7 of info byte, pixel orientation
 CDXL_BIT_PLANAR  = 0x00,
 CDXL_BYTE_PLANAR = 0x20,  //pitch calculation unknown
 CDXL_CHUNKY      = 0x40,  //pitch calculation unknown
 CDXL_BIT_LINE    = 0x80,  //???
 CDXL_BYTE_LINE   = 0xC0   //???
};






//this struct holds all te info from the chunk header + additional
// - info which is marked with "*" must be also handled by the player
struct cdxl_chunkinfo
{
 uint8_t type;            //  custom / standart / special (????)

 uint8_t ven;             //  video encoding
 uint8_t por;             //  pixel orientation
 uint8_t nch;             //* number of audio channels

 size_t  csizeCur;        //  current chunksize
 size_t  csizePrev;       //  previous chunksize (helpful for playing backward)
 size_t  fnumber;         //  framenumber as stored in the chunk
 int     w;               //* frame width
 int     h;               //* frame height
 uint8_t bitplanes;       //  number of bitplanes
 size_t  cmapsize;        //  size of colormap in the chunk in bytes
 int     audiosize;       //* size of audio data in the chunk in bytes

 size_t  bitmapsize;      //  size of bitmap data (chunksize - audio - header - cmap) => not part of the chunk
};



//chunkbuffer (with related pointers pointing to specific buffer sections)
//byte sizes are given in the corresponding (first) "cdxl_chunkinfo" struct
struct cdxl_chunk
{
 char* data;   //complete chunk with header (allocated buffer)

 char* header;  // == data
 char* cmap;    // == data + headersize
 char* bitmap;  // == cmap + cmapsize
 char* audio;   // == bitmap + bitmapsize
};




//an instance of this class holds all information to decode the stream
// - CDXL data is directly streamed from the file
// - seeking may not be supported by stream
// - if accurate seeking is supported backward playing is possible
class CDXL
{
 protected:
  static const unsigned char bps = 8;  //bits per audio sample (always 8)

  char infostring[CDXL_INFOSTRING_BUFFER];        //format info string
 
  cdxl_chunkinfo infoFirst;    //info from the first chunk
  cdxl_chunkinfo infoCur;      //info from the current chunk
  cdxl_chunkinfo infoPrev;     //info about previous chunk 

  int fchange;  //number of format changes

  int curFrame;         //current frame number (counted), starts with 0 => the frame number in the chunk may be incorrect
 
  ifstream  stream;     //file
  ifstream* streamPtr;  //stream pointer
  streampos sStartPos;  //stream (file) position when file was opened (needed for rewind), is "-1" if stream was closed

  cdxl_chunk chunk;
  int  chunksize;       //size of each chunk in bytes, should (must?) be constant within a file

  char cmap24[3*256];   //holds rgb (24 bit per entry) color map
  
  int  nframes;         //approximated number of frames (extrapolation)
  
  bool ready;           //indicates health state, if "false" "playback" is not possible (maybe stream is corrupted, etc.)



 
 protected:
  void  InitVars();                            //initialise member variables (stream and buffers should be closed)
  bool  ReadChunk( cdxl_chunkinfo* info  );    //read a chunk from stream into chunk buffer, fills the given info struct (stream pointer must be at the right position), returns false on error, else true
  bool  CompareInfoDiff(cdxl_chunkinfo* a, cdxl_chunkinfo* b);   //compare 2 chunk_info structs and return false if (nearly) equal, else true
  char* GetChunkInfo(cdxl_chunkinfo* info);   //return information string about chunkinfo struct (struct must exist)

 public:
  CDXL();
 ~CDXL();
  int  Open(const char* fname);    //open file, read first chunk to buffer, return:  0 -> success, -1 file open error, -2 first chunk is not valid CDXL chunk
  int  Open(ifstream* s);    //use already opened file stream
  void Close();              //close stream, init all member variables
  bool Rewind();             //reopen the file, returns "true" on success else "false"
  bool IsReady();            //returns true if file is opened and the CDXL instance is in a healthy state else "false" (=> read error,corrupt data,etc.) => must always be checked if frame or audio data is requested 

  //seeking, stream begins with frame 0
  bool SeekFrameAbs(int frame);    //seek frame with absolute frame number, returns true on success (loads new frame), else false (current frame remains)
  bool SeekFrameRel(int rframes);  //seek frame relative to current frame, returns true on success (loads new frame), else false (current frame remains)

  void Parse();   //parse the file right after it is opened, prints information and warnings and a result

  int  GetCurFrameNum();         //returns current frame number, -1 if stream is closed
  int  GetChunkSize();           //return size of current chunk (all chunk should have the same size)

  //Following methods can be used for timing (depends on the currently opened file)
  float GetFpsBySampleRate(int samplerate); //return fps value, calculated by sample rate value (e.g 11050 or 22050) ("frame delay" is = 1 / fps)
  float GetFpsByDataRate(int bytesPerSec);  //return fps value, calculated by data rate (CDXL data read per second for playback)
  float GetSampleRateByFps(float fps);      //returns the playback sample rate for the specified FPS relating to the CDXL file
  float GetDataRateByFps(float fps);        //return data rate in bytes per second for the given "fps"




  char* GetFrame(char* framebuf, int pitch);  //writes frame to "framebuf" with the requested pitch (size of buffer = pitch * h), returns NULL if frame data is not available; output data format is chunky RGB (order: R,G,B)
  char* GetCmap();    //returns NULL if not available, else pointer to color table

  char* GetFrameRaw(int* datasize);   //returns raw frame data (pointer into the chunkbuffer), NULL if not available; "datasize" is set to the raw frame data size in bytes

  int   GetInfo(int* w, int* h, int* bpp_out, int* numframes);    //get format info (of decoded frame); "bpp_out" = bits per pixel of output format!; NULL pointers are ignored       

  bool  GetAudioFormat(int* channels, int* bits);     //return format of audio Data; NULL pointer as method parameters are safely ignored; returns "false" if no audio is available

  char* GetAudio(int* datasize);      //return buffer to audio data relating to the current frame, 8 bit mono or stereo, NULL if not available; datasize is set to audio data size of chunk in bytes (???stereo interleaved??? ==> to check)


  bool  NextFrame();      //loads the next frame (complete chunk) as current frame, returns true on success, false if frame could not be loaded
  bool  PreviousFrame();  //loads previous frame, returns true on success else false (if stream is not seekable)


  char* GetInfoString();  //return info string with info from first chunk (considered as valid for the whole strean)
};


#endif
