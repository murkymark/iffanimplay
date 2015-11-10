/*
 separate part of "player.cpp" containing all the decoder related methods (wrapping decoder member functions)
 Generic code should be put to "player.cpp" if possible

*/


#include "player.hpp"



/******************************************************************************/
bool AnimPlayer::Seek(int frame)  //should be done in a separate thread, so controls don't freeze
{
 if( ftype == IFFANIMPLAY_FT_IFFANIM )  //seeking backward would require to decode all frames from the start (XOR mode may improve speed)
   return false;
   
 else if( ftype == IFFANIMPLAY_FT_CDXL )
   if(dec_cdxl->SeekFrameAbs(frame)) {
      //make the seeked frame the current (clear buffered frames)
     framecnt = frame;
     QueueAddFrame();
     while(qframes > 1) QueueDeleteFrame();
     return true;
   }
 
 return false;
}





/******************************************************************************/
//try to open with all decoders one after another, until one succeeds
enum_fileType AnimPlayer::Open(const char *path)
{
 if(ftype != IFFANIMPLAY_FT_NONE){
   DelDecoder();
 }

 cout << "Opening file \"" << path << "\" ... \n";
 //try to open in read mode to check if file can be accessed
 fstream file;
 file.open(path, ios::in);
 if(file.is_open() == false) {
   cout << "Error: Can't open file. Check the path!\n";
   return IFFANIMPLAY_FT_NONE;
 }
 else
   file.close();


 bool ok = false;  //indicates success
 dec_anim = NULL;
 dec_cdxl = NULL;
 
 //try to open with different decoders
 
 if(ok == false) cout << "Trying IFFANIM decoder:" << endl;
 dec_anim = new IffAnim();

 if(ok == false  &&  (dec_anim->Open(path) == 0)) {
   ftype = IFFANIMPLAY_FT_IFFANIM;
   if (dec_anim->IsILBM())
     cout << "IFF ILBM opened\n";
   else
     cout << "IFF ANIM opened\n";
     
   ok = true;
   dec_anim->SetLoop(false);  //important for detection of ending
   
 }
 cout << dec_anim->GetError();  //successful opening also generates an error string (maybe there are warnings)
 if(ftype == IFFANIMPLAY_FT_IFFANIM)
   cout << "\n" << dec_anim->GetInfoString();  //print file information
 else if(ok == false){
   delete dec_anim;
   dec_anim = NULL;
 }
 

 if(ok == false){
   cout << endl << "Trying CDXL decoder:" << endl;
   dec_cdxl = new CDXL();
 
   if(dec_cdxl->Open(path) == 0) {
     ftype = IFFANIMPLAY_FT_CDXL;
     fps = 12.0;  //default value, may later be overwritten when processing command line parameters
     cout << "CDXL file opened\n";
     cout << "Proposed FPS: " << fps << " (not stored in file)" <<endl;
     cout << "sample rate: " << a_srate << endl;
     cout << "Data rate: " << dec_cdxl->GetDataRateByFps(fps) / 1024 << "kb" << endl;
     cout << endl;
     cout << dec_cdxl->GetInfoString(); 
     ok = true;
   
     //get info
   }
   else {
     delete dec_cdxl;
     dec_cdxl = NULL;
   }
 }

 cout << endl;


 if(ok == false)
   cout << "None of the available decoders could open this file\n";

 return ftype;
}


/******************************************************************************/
void AnimPlayer::DelDecoder()
{
 if(ftype != IFFANIMPLAY_FT_NONE) {
 
   if(ftype == IFFANIMPLAY_FT_IFFANIM) {
     if(dec_anim != NULL)
       delete dec_anim;
     dec_anim = NULL;
   }
   else if(ftype == IFFANIMPLAY_FT_CDXL) {
     if(dec_cdxl != NULL)
       delete dec_cdxl;
     dec_cdxl = NULL;
   }
   else
     cerr << "Error in " << __FUNCTION__ << ": Decoder not deleted!" << endl;
 }
 
 ftype = IFFANIMPLAY_FT_NONE;
}


/******************************************************************************/
//todo put this to player.cpp
bool AnimPlayer::ProcessArgs(int argc, const char **argv)
{
 char *argv1[1000];  //not generic args
 int  argc1 = 0;

 char *argv2[1000];  //unknown args
 int  argc2 = 0;

 //generic args
 int tmp;
 for(int i = 0; i < argc; i++)
 {
    //print help text and quit
    if((strcmp(argv[i],"-help") == 0) || (strcmp(argv[i],"--h") == 0)){
       PrintHelp();
       return false;
    }
    //try to use OpenGL
    if( strcmp(argv[i],"-ogl") == 0) {
       gui->useGL = true;
    }
    //window width
    else if((strlen(argv[i]) >= 2)  &&  memcmp(argv[i],"-w",2) == 0) {
       w_disp = atoi( argv[i]+2 );
       if(w_disp <= 0)
          w_disp = w_org;
    }
    //window height
    else if((strlen(argv[i]) >= 2)  &&  memcmp(argv[i],"-h",2) == 0) {
       h_disp = atoi( argv[i]+2 );
       if(h_disp <= 0)
          h_disp = h_org;
    }
    //extract data
    else if(strcmp(argv[i],"-extract") == 0)
    { 
      extract = true;
      w_disp = 320;  //during extraction frames aren't shown so use small window dimension
      h_disp = 200;
      if(i+1 < argc) {
        extr_path = (char*)argv[i+1];
      }
      else {  
        cerr << "Error, no output path specified for extraction" << endl;
        return false;
      }
    }
    //set loop
    else if(strcmp(argv[i],"-loop") == 0)
       loop = true;
    //set start mode
    else if(strcmp(argv[i],"-pause") == 0)
       playing = false;
    //ignore any audio
    else if(strcmp(argv[i],"-noaudio") == 0) {
       playAudio = false;
       cout << "Audio disabled" << endl;
    }
    //set a fixed time delay for each frame
    else if((strlen(argv[i]) > 8)  &&  memcmp(argv[i],"-fixtime",8) == 0) {
       fixtime = true;
       fixtimeval = atoi(argv[i] + 8);
    }

    //seek frame when loading
    else if((strlen(argv[i]) >= 5)  &&  memcmp(argv[i],"-seek",5) == 0) {
      seekatopen = atoi( argv[i]+2 );
      if(seekatopen < 0)
         seekatopen = 0;
    }

    //put unknown arg to a separate list, maybe it is decoder specific
    else {
       argv1[argc1] = (char*)argv[i];
       argc1++;
    }
 }

 //some options are ignored when "extract" option is set
 if(extract == true)  
   loop = false;

 
 
 //iff anim related args
 if(ftype == IFFANIMPLAY_FT_IFFANIM)
 {
    for(int i = 0; i < argc1; i++)
    {
      //tell that 2 first and 2 last frames are equal, iff anim format specific
      if( strcmp(argv1[i],"-loopanim") == 0) {
         loopanim = true;
         dec_anim->SetLoopAnim(true);

         float lensec;
         GetLength(&numframes, NULL, &lensec);   // update info
         lentime = (int)(lensec * 1000);
      }

      //put unknown arg to a separate list
      else {
         argv2[argc2] = argv1[i];
         argc2++;
      }
    }
 }

 //cdxl anim args
 else if(ftype == IFFANIMPLAY_FT_CDXL)
 {
    for(int i = 0; i < argc1; i++)
    {
      if((strlen(argv1[i]) > 4)  &&  memcmp("-fps", argv1[i],4) == 0) {
        fps = atof(argv1[i] + 4);
        a_srate = dec_cdxl->GetSampleRateByFps(fps);
      }
      //put unknown arg to a separate list
      else {
         argv2[argc2] = argv1[i];
         argc2++;
      }
    }
 }

 //print all unprocessed args
 if(argc2 > 0) {
   cout << "Can't process unknown arguments:\n";
   for(int i = 0; i < argc2; i++) {
     cout << argv2[i] << endl;
   }
 }

 return true;
}



/******************************************************************************/
bool AnimPlayer::NextFrame()
{
 if(ftype == IFFANIMPLAY_FT_IFFANIM)
   return dec_anim->NextFrame();
 else if(ftype == IFFANIMPLAY_FT_CDXL)
   return dec_cdxl->NextFrame();
 else
   return false;
}

/******************************************************************************/
void AnimPlayer::Rewind()
{
 ended = false;
 audiopos = 0;         // reset audio position to restart playing
 framecnt = 0;         // turns to 0 next frame loop iteration start 
 
 if(ftype == IFFANIMPLAY_FT_IFFANIM)
   dec_anim->Reset();
 else if(ftype == IFFANIMPLAY_FT_CDXL)
   dec_cdxl->Rewind();
}

/******************************************************************************/
SDL_Surface *AnimPlayer::GetFrame(SDL_Surface *frameSurf)
{
 //get chunky frame data and color map
 SDL_LockSurface(frameSurf);

 if(ftype == IFFANIMPLAY_FT_IFFANIM) 
 {
   dec_anim->GetFrameChunky(frameSurf->pixels, frameSurf->pitch);
   if(bpp <= 8) {
     char *cmap = (char*)dec_anim->GetCmap();
     if(cmap == NULL)
       cout << "No color map available\n";
     else 
     {
       SDL_Color *c = frameSurf->format->palette->colors;
       for(int i = 0; i < 256; i++) {  //setup palette of surface
         c[i].r = cmap[0];
         c[i].g = cmap[1];
         c[i].b = cmap[2];
         cmap += 3;
       }
     }
   }
 }
 
 else if(ftype == IFFANIMPLAY_FT_CDXL)
 { 
   dec_cdxl->GetFrame((char*)frameSurf->pixels, frameSurf->pitch);  //write data directly to surface mem (no palette, returns 24 bits always)
   if(bpp <= 8) {
     char *cmap = (char*)dec_cdxl->GetCmap();
     if(cmap == NULL)
       cout << "No color map available\n";
     else 
     {
       SDL_Color *c = frameSurf->format->palette->colors;
       for(int i = 0; i < 256; i++) {  //setup palette of surface
         c[i].r = cmap[0];
         c[i].g = cmap[1];
         c[i].b = cmap[2];
         cmap += 3;
       }
     }
   }
 }


 SDL_UnlockSurface(frameSurf);
 return frameSurf;
}

/******************************************************************************/
//pitch value is needed in case the decoder wants to use its own (to avoid unnecessary conversion)
//bits : bits per pixel
void AnimPlayer::GetFrameFormat(int *bits, int *w, int *h, int *pitch)
{

 if(ftype == IFFANIMPLAY_FT_IFFANIM) {
   dec_anim->GetInfo(w, h, bits, NULL, NULL);
   *pitch = *w * *bits  /  8;
 }
 else if(ftype == IFFANIMPLAY_FT_CDXL) {
   dec_cdxl->GetInfo(w, h, bits, NULL);
   *pitch = *w * *bits / 8;
 }
}

/******************************************************************************/
void AnimPlayer::GetLength(int *nframes_, int *nsamples_, float *sec_)
{
 int nframes = -1;
 int nsamples = -1;
 float sec;

 if(ftype == IFFANIMPLAY_FT_IFFANIM) {
   int msec;
   dec_anim->GetInfo(NULL, NULL, NULL, &nframes, &msec);
   sec = (float)msec / 1000;
   nsamples = -1;
 }
 else if(ftype == IFFANIMPLAY_FT_CDXL) {
   dec_cdxl->GetInfo(NULL,NULL,NULL, &nframes);
   nsamples = -1;   //unknown, may vary from frame to frame
   if(fps != 0)
     sec = (float)nframes / fps;
 }
 
 if(nframes_ != NULL) *nframes_ = nframes;
 if(nsamples_ != NULL) *nsamples_ = nsamples;
 if(sec_ != NULL) *sec_ = sec;
}

/******************************************************************************/
double AnimPlayer::GetFrameDelay()
{
 double d;

 if(ftype == IFFANIMPLAY_FT_IFFANIM){
   if(fixtime) d = fixtimeval;
   else        d = (double)dec_anim->GetDelayTime();
 }
 else if(ftype == IFFANIMPLAY_FT_CDXL) {
   if(fps != 0) d = (double)1 / fps;   //delay of a frame depends on FPS
   else         d = 0;
 }
 
 return  d * speed;
}

/******************************************************************************/
bool AnimPlayer::GetAudioFormat(int *bits, bool *sign, int *channels, double *rate, bool *bigEndian)
{
 int int_dummy;
 bool bool_dummy;
 double double_dummy;
 
 if(bits == NULL) bits = &int_dummy;
 if(sign == NULL) sign = &bool_dummy;
 if(channels == NULL) channels = &int_dummy;
 if(rate == NULL) rate = &double_dummy;
 if(bigEndian == NULL) bigEndian = &bool_dummy;


 if(ftype == IFFANIMPLAY_FT_IFFANIM) {
   int rate_int;
   int ret = dec_anim->GetAudioFormat(channels, bits, &rate_int);
   *rate = (double)rate_int;
   *sign = true;
   *bigEndian = true;
   return (ret == 0) ? true : false;
 }  
 else if(ftype == IFFANIMPLAY_FT_CDXL) {
   bool ret = dec_cdxl->GetAudioFormat(channels, bits);
   *rate = dec_cdxl->GetSampleRateByFps(fps);  //sample rate depends on used FPS
   *sign = true;
   *bigEndian = true;
   return ret;
 }
}


/******************************************************************************/
int  AnimPlayer::GetFrameAudioSize()
{
 if(ftype == IFFANIMPLAY_FT_IFFANIM) {
   return dec_anim->GetFrameAudioSize();
 }
 else if(ftype == IFFANIMPLAY_FT_CDXL) {
   int datasize;
   dec_cdxl->GetAudio(&datasize);
   return datasize;
 }
}

/******************************************************************************/
int  AnimPlayer::GetFrameAudio(void *data, int size)
{
 if(ftype == IFFANIMPLAY_FT_IFFANIM){
   return dec_anim->GetFrameAudio((char*)data, size, 0);
 }  
 else if(ftype == IFFANIMPLAY_FT_CDXL){
   int datasize;
   char *d = dec_cdxl->GetAudio(&datasize);
   memcpy(data, d, datasize);
   return datasize;
 }
}
