/*
IFFAnimPlayer (Amiga animation player)
-------------
player class using "libiffanim","libcdxl", and "SDL" (Simple DirectMedia Layer) to play "CDXL video" and "IFF animations"

License: GNU LGPL v3 (http://www.gnu.org/licenses/lgpl.html)
Author:  Markus Wolf (http://murkymind.de)
Version: see preprocessor definition below
Date:    see preprocessor definition below

 when compiling under Windows/MinGW without "-mwindows" the console stays open for information

*/


/*
 A preload queue is used to get enough data for an audio block to play.
 Frame images are also preloaded, this would provide a more stable playback
 if multiple threads are used (maybe in the future) for playing and for loading.
 Frame audio and image are connected, so when queueing both, the decoder needs
 only to provide audio and graphics data of one frame.
*/



#ifndef _player_H_
#define _player_H_

#define IFFANIMPLAY_VERSION "2015-011-18"
#define IFFANIMPLAY_AUTHOR  "Markus Wolf (http://murkymind.de)"

#include <iostream>


using namespace std;





//loaders
#include <iffanim.hpp>   
#include <cdxl.h>

//GUI
#include "player_gui.hpp"



//RGBA order for software surfaces
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  #define RMASK  0xff000000
  #define GMASK  0x00ff0000
  #define BMASK  0x0000ff00
  #define AMASK  0x000000ff
#else
  #define RMASK  0x000000ff
  #define GMASK  0x0000ff00
  #define BMASK  0x00ff0000
  #define AMASK  0xff000000
#endif




#define IFFANIMPLAY_GUISUPPORT 1    //0 = player won't provide a GUI; 1 ) player provides a GUI
#define IFFANIMPLAY_GUI_GFXPATH "res/gfx/"  //path to data dir for GUI graphics, only used for development version - final release includes graphics in executeable


//to distinguish the file formats
enum enum_fileType {
 IFFANIMPLAY_FT_NONE,   //initial, if no file is open
 IFFANIMPLAY_FT_CDXL,
 IFFANIMPLAY_FT_IFFANIM
};


#define IFFANIMPLAY_DEBUG 1    //set to 1 to print some messages for debugging


//events - return codes
#define IFFANIMPLAY_NONE      0  //does nothing
#define IFFANIMPLAY_QUIT      1  //quit event
#define IFFANIMPLAY_CLOSE     2  //close file


#define IFFANIMPLAY_SMPBUFSIZE   1024*2    //sample buffer in samples (sampleframes), 512 very accurate -> buffer underun on some systems
#define IFFANIMPLAY_BUFFER_TIME  3.00      //max number of seconds of buffered data


#define IFFANIMPLAY_QUEUE_MAX 1000  //maximum size of frame queue


class AnimPlayer;
extern AnimPlayer *playerPtr; //reference to player instance




struct APFrame
{
 SDL_Surface* frame; //a single frame
 char* audio;        //audio data of the frame
 
 int asize;       //size of audio data in bytes
 streamsize apos; //audio sample position of the first byte in the data in relation to the whole video
 int frameno;     //frame number of the video stream
 double delay;    //delay of frame in seconds
};




class AnimPlayer
{
 public:
   
   string file_to_open;   //name of video file to open/opened
   string file_name;      //currently opened file

   SDL_Surface* dispimg;    //scaled frame (needed because we don't have a scaling function for every possible screen mode; and we automatically convert the data to screen mode by SDL blitting)

   bool init_done; //to prevent multiple init()

   int  numframes;   // number of total frames


   bool   playAudio;  //"true" if audio data is available; else "false"
   streamsize audiopos;   // current byte position in audio data
   char*  audiodata;  // audio buffer, SDL gets audio from within the callback

//   bool   audioAsync; // depends on decoder, if audio can only be retrieved frame synchron it is set to "false", else "true" -> asynchron audio is used when possible to prevent gaps

   float minAudioBuf;   //minimum buffered audio data
   int   minFrameBuf;   //minimum buffered frames

   double a_srate;      //sample rate (connected to "speed" factor)
   int    a_nch;        //number of audio channels
   int    a_bps;        //bits per sample point
   bool   a_sign;       //true if signed
   bool   a_bigEndian;  //endianess
   int    a_frameSize; //size of a sample frame in bytes (sample point of each channel)

   bool  playing;    // "true" if playing, "false" if paused
   bool  ended;      // "false" if end not reached, else "true" (the last frame in the buffer is the end, not automatically the currently displayed frame)
   bool  loop;       // loops animation if set to "true"
   bool  fixtime;    // indicates usage of fixed time delay for every frame
   int   fixtimeval; // fixed frame delay in ms
   float speed;      // speed factor influences playback speed
   int seekatopen;   // seek frame when opening file (only used if video is opened via command line)

   int  w_disp;      // display width in pixels
   int  h_disp;      // display height
   int  w_org;       // original width
   int  h_org;       // original height
   int  pitch;       // length of a scanline of the decoded/converted image in bytes
   int  bpp;         // bits per pixel of the decoded/convertd image (8/24)
   int  lentime;     // length of animation in ms 



   bool  extract;    // indicates "extraction" request
   char* extr_path;  // path (dir) to extract frames to

//   char* snap_path;  // path (dir) to save snapshot to

   float fps;        //fps (only usable for fps based animation formats)

 public:
   AnimPlayerGui *gui;
 
 //>>>>>>>> decoders
 union{ //as anonymous union
   class IffAnim *dec_anim;  // animation class (libiffanim)
   class CDXL    *dec_cdxl;  // cdxl class (libcdxl)
 };

 public:
   void ResizeRefit(int w, int h);  //refit after resize
   int  GetFrameIndex();        //return currently displayed frame index

   int Play();                  //playback
   bool JumpRel(int d = 1);    //jump to frame relative from current and draw

   bool ToggleLoop();  //toggle loop, returns true if loop is enabled

//>>>>>>>> internal methods
 protected:
   bool close_file;

   void UpdateCaption();         //update window title with information
   int  Extract(char* outpath);  //save each frame as bmp file (audio as raw, interleaved data); player window stays open to indicate the extraction Process, window is closed when done
   int  Wait(int delay);         //wait (handle events) until time has passed, frees CPU usage

   void Scale(SDL_Surface* src, SDL_Surface* dst);  //scales src surface pixel data to dst surface
   
   void DrawFrame();  //draw frame on screen surface
   
   int  PrintHelp();  //print command line help

   int  PrintInfo();  //print player info


   int  mkfname(int val, int ndigits, char* fname, const char* prefix, const char* ext);   //generate numbered file name (for frame extraction)

   static void audio_callback(void* userdata, Uint8* stream, int len);         //static audio callback function

 //>>>>>>>> timing
   //A timer variable stores the elapsed time since start of playing which must be reset when unpausing from paused mode.
   //The current frame is displayed and the time to display this frame is queried, which is used along with the current timer to calculate the threshold time value indicating the frame swap
   //The new threshold is added to the old threshold, to avoid accumulation of discordance, if possible. (if the previous frame was shown too long the next frame is shown a shorter time to compensate)
   double timer;          //the timer holds ticks (ms) since start of playing (synchronized to the "system clock" each frame)
   double timerThres;     //threshold of frame change
   double lag;            //time we lag behind the targetted threshold when showing the new frame (caused by slow CPU), in ms
   Uint32 sdlStartTicks;  //start of playing => to calc the timer value

   void   ResetTimer();               //reset timer
   void   SetTimeDelay(double delaysec);  //specify how long the current frame has to be displayed (delay); invoked for each frame
   Uint32 GetDelayTimeMs();           //return number of ms we have to wait till the new frame has to be displayed; if threshold is already exceeded return 0
   void   SetLagTime();               //set lag var => lag increases if CPU too weak


 //>>>>>>>> queue buffer
   APFrame queue[IFFANIMPLAY_QUEUE_MAX]; //to make sure we have enough data available
   bool q_init;    //indicates, has buffered frames
   int  qt;        //tail
   int  qh;        //head (current frame)
   int  qframes;   //frames currently in queue
   int  qsamples;  //audio sample frames in queue
   int  framecnt;  //frame counter (starting with 0)
   streamsize audiocnt;  // counts audio samples (less important than frame counter)

   void QueueFree();         //free complete queue
   int  QueueFill(int minFrames, int minSamples);  //add frames to the queue until "minFrames" and "minSamples" are met; return number of loaded frames
   bool QueueAddFrame();     //load next frame from decoder to the queue; return "false" on error else "true"
   void QueueDeleteFrame();  //delete oldest frame from queue head (head is increases)
   int  QueueGetAudio(void* data, streamsize apos, int size);  //Get audio data from queue; returns number of requested bytes present in the queue; "apos" is absolute audio byte position;
                                                               //"size" bytes are always written, bytes outside queued range as "0"; proper timing is essential


 //>>>>>>>> wrapper functions to access the different decoders (virtual methods would be possible, but means also more complexity => additional wrapper classes)
  enum_fileType ftype;  //file format (indicates responsible decoder)
  bool  loopanim;       // handles loop animations with the 2 first and 2 last frames beeing the same -> 2 last frames are skipped (IFF ANIM only)

  enum_fileType Open(const char* path);    //try to open a file with available decoders
  void  DelDecoder();            //remove decoder
  bool  ProcessArgs(int argc, const char** argv);  //process command line parameters (some are decoder specific), player should quit if "false" is returned
  void  GetLength(int* nframes, int* nsamples, float* sec);      //return length information, "-1" as integer means unknown


  bool   NextFrame();        //make the decoder loading the next frame
  void   Rewind();           //make the first frame the current
  bool   Seek(int frame);    //seek the specified frame
  double GetFrameDelay();    //return the delay (time to display the current frame) in seconds
  void   GetFrameFormat(int* bits, int* w, int* h, int* pitch);   //Get the frame format the decoder returns: 32,24,16, or 8bit (indexed) only
  SDL_Surface* GetFrame(SDL_Surface* frameSurf);     //current frame data is written (copied) to the surface (along with color map), which must exist (allocated) in the proper format; returns the surface pointer

  bool GetAudioFormat(int* bits, bool* sign, int* channels, double* rate, bool* bigEndian);   //format of audio data (integer PCM only), returns "false" if no audio is available

  int  GetFrameAudioSize();        //get byte size of audio data matching the current frame
  int  GetFrameAudio(void* data, int size);  //writes all audio data of the current frame to the data buffer; "offs" number of bytes are skipped (use "0" for the whole frame data); returns number of written bytes


 //>>>>>>>>> interface methods
 public:
   bool openFile(int argc, const char *argv[]);
   int  init(int argc, const char** argv);   //init player
   void run(); //player thread entry
   
   void clear(); //close any open file but keep running, reset args
   void eventHandler(SDL_Event *e); //temporary solution; called from GUI when event occurs
   
   AnimPlayer();
   ~AnimPlayer();
   
};






#endif
