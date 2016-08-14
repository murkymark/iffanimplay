#include "player.hpp"
#include "scale.h"
#include "system_specific.h"


//for debugging -> comment out
//fstream ftest;


AnimPlayer *playerPtr = NULL; //reference to player instance







/******************************************************************************/
void AnimPlayer::audio_callback(void* userdata, Uint8* stream, int len)
{
 AnimPlayer* player = (AnimPlayer*)userdata;  //to access player attributes

 int valid = player->QueueGetAudio(player->audiodata, player->audiopos, len);  //complete buffer is filled; buffer is filled with silence automatically

 SDL_MixAudio(stream, (Uint8*)(player->audiodata), len, SDL_MIX_MAXVOLUME);
 player->audiopos += valid;
}









/******************************************************************************/
AnimPlayer::AnimPlayer()
{
 dispimg = NULL;
 gui = NULL;
 init_done = false;
}

/******************************************************************************/
AnimPlayer::~AnimPlayer()
{
 clear();
  
 if(gui != NULL)
   delete gui;
 
 SDL_Quit(); //call after gui is destroyed
}


/******************************************************************************/
int AnimPlayer::GetFrameIndex()
{
	return queue[qh].frameno;
}

/******************************************************************************/
int AnimPlayer::PrintHelp()
{
 cout << "IFF-Anim and CDXL player | " << IFFANIMPLAY_VERSION << " | "<< IFFANIMPLAY_AUTHOR << endl;
 cout << "Usage: iffanimplay file [options]" << endl;
 cout << "Options:" << endl;
 cout << " -fixtime...     Use a fix delay between frames, delay in ms\n"
         "                  (example: -fixtime1000)" << endl;
 cout << " -extract <dir>  All frames are extracted to single .bmp files" << endl;
 cout << " -loop           Animation loops" << endl;
 cout << " -loopanim       Doesn't play the first 2 frames when looping (for animations\n"
         "                  where the 2 first and the 2 last frames are the same)" << endl;
 cout << " -w...           Scale to requested width (example: -w300) for display" << endl;
 cout << " -h...           Scale to requested height" << endl;
 cout << " -nogui          makes GUI unavailable" << endl;
 cout << " -help           Print this help" << endl;
 cout << " -seek           seek frame when opening video" << endl;
 cout << "obsolete!!!" << endl;
}

/******************************************************************************/
int AnimPlayer::PrintInfo()
{
 cout << "IFF-Anim and CDXL player | " << IFFANIMPLAY_VERSION << " | "<< IFFANIMPLAY_AUTHOR << endl;
 cout << "Use option --h or -help to print help (iffanimplay "" -h)" << endl;
}


/******************************************************************************/
//-create a file name from a value with a specified number of digits, a "prefix" and a extension string
//-returns NULL if value exceeds number of digits
//-"fname": resultin string is stored to, must be large enough for: prefix + ndigits + extension
int AnimPlayer::mkfname(int val, int ndigits, char* fname, const char* prefix, const char* ext)
{
 char strbuf[256]; 
 int len;
 int prefixlen;

 //number to string conversion
 sprintf(strbuf, "%d", val);
 len = strlen(strbuf);

 //handle a too big value
 if(len > ndigits) {
   cerr << "file naming error: number too large" << endl;
   return -1;
 }
 //add prefix
 prefixlen = strlen(prefix);
 strcpy(fname, prefix);

 //-fill fname with '0's
 memset(&fname[prefixlen], '0', ndigits-len);

 //-merge "0"s, number and extension
 strcpy(&fname[ prefixlen + ndigits - len ], strbuf);
 strcat(fname, ext);
 return 0;
}



/******************************************************************************/
bool AnimPlayer::ToggleLoop(){
	if(loop){
		loop = false;
		return loop;
	}
	else{
		loop = true;
		if(ended)
			ended = false;
		return loop;
	}
}


/******************************************************************************/
void AnimPlayer::Scale(SDL_Surface* src, SDL_Surface* dst)
{
 if(src == NULL ||  dst == NULL) {
   cerr << "Scale error: Invalid surface pointer" << endl;
   return;
 }

 SDL_LockSurface(src);
 SDL_LockSurface(dst);
 
 if(bpp == 8){
   ScaleBitmap8((char*)dst->pixels, dst->w, dst->h, dst->pitch, (char*)src->pixels, src->w, src->h, src->pitch);
   SDL_SetColors(dst, src->format->palette->colors, 0, 256);       //copy palette (can change each frame)
 }
 else if(bpp == 24)
   ScaleBitmap24((char*)dst->pixels, dst->w, dst->h, dst->pitch, (char*)src->pixels, src->w, src->h, src->pitch);
 else
   cerr << "Scale error: No scaling function for " << bpp << " bit format" << endl;

 SDL_UnlockSurface(src);
 SDL_UnlockSurface(dst);
}

/******************************************************************************/
// -set windows title information
void AnimPlayer::UpdateCaption()
{
 char strbuf[1000];
 static const char window_prefix[] = "IffAnimPlay";
 const char* state;

 //if no file loaded
 if(ftype == IFFANIMPLAY_FT_NONE) {
   SDL_WM_SetCaption(window_prefix, NULL);
   return;
 }

 //determine state
 if(ended  &&  qframes == 1)
   state = "ended";
 else {
   if (playing) state = "playing";
   else         state = "paused";
 }

 // compose window string
 sprintf(strbuf, "%s - [%dx%d / %dx%d][%d / %d (%d sec)][%s] lag:%dms",
   window_prefix,
   w_disp, h_disp,
   w_org, h_org,
   GetFrameIndex() + 1,
   numframes,
   lentime / 1000,
   state, 
   (int)lag);   
   
 SDL_WM_SetCaption(strbuf, NULL);  //update
}


/******************************************************************************/
//must return false after the last frame when not looping (for extraction)
int AnimPlayer::Extract(char* outpath)
{
 static char bmp_path[2000];  //directory
 static char file_name[1000]; //filename
 string path;
 int i;
 
 strcpy(bmp_path, outpath);
 if( (strlen(bmp_path) > 0) && bmp_path[ strlen(bmp_path) - 1 ] != '/')  //make sure there is the '/' at the end
   strcat(bmp_path, "/");
 cout << "Extracting data to \"" << bmp_path << "\" ..." << endl;

 //open file where the timings are written in ms
 ofstream timingfile;
 path = string(bmp_path) + "timing.txt";
 timingfile.open(path.c_str(), ios::out);

 if(!(timingfile.is_open())) {
   cerr << "Cant create file \"" << path << "\"" << endl;
   return -1;
 }
 timingfile << "#All timing values in milliseconds:" << endl;


 //audio file
 ofstream audiofile;
 if(playAudio)
 {
   path = string(bmp_path) + "audio.raw";
   audiofile.open(path.c_str(), ios::binary | ios::out);

   if(!(audiofile.is_open())) {
     cerr << "Cant create file \"" << path << "\"" << endl;
     return -1;
   }
 }

 //get length of decimal number, for file numbering
 sprintf(file_name, "%d", numframes);
 int numlen = strlen( file_name );


 //extract each frame to file
 //number of frames might be estimated, so we get as many frames as available
 i = 0;
 while( ended == false )
 {
   QueueAddFrame();  //buffer only a single frame
   UpdateCaption();

   //generate file name
   mkfname(i, numlen, file_name, "frame", ".bmp");
   path = string(bmp_path) + file_name;

   if(SDL_SaveBMP(queue[qh].frame, path.c_str()) != 0) {
     cerr << "Error saving .bmp file: " << SDL_GetError() << endl;
     break;
   }

   if(playAudio)
     audiofile.write(queue[qh].audio, queue[qh].asize);

   timingfile << "FN=\"" << file_name << "\" T=" << (queue[qh].delay * 1000) << endl;

   QueueDeleteFrame();
   i++;

   //event handling (so window doesn't freeze)
   SDL_Event event;
   while(SDL_PollEvent(&event)) {
     if((event.type == SDL_KEYDOWN  &&  SDLK_ESCAPE)  ||  (event.type == SDL_QUIT)) {
       cout << "Extraction aborted" << endl;
       ended = true;
     }
   }
 }
 if(audiofile.is_open()) audiofile.close();

 cout << i << " frames of estimated " << numframes << " extracted" << endl;
 if(playAudio)
   cout << "Extracted audio data: " << (audiocnt * a_frameSize) << " bytes" << endl;
 return 0;
}


/******************************************************************************/
//open media file
bool AnimPlayer::openFile(int argc, const char *argv[]){
 //open file, first argument must be the file to open
 if(Open(argv[1]) == IFFANIMPLAY_FT_NONE)
   return false;  //fail to open file

 file_name = argv[1];

 //get frame format
 bool err = false;
 GetFrameFormat(&bpp, &w_org, &h_org, &pitch);
 
 //verify
 switch(bpp) {
   case  8:
   case 16:
   case 24:
   case 32:
     break;
   default:
     err = true;
     cerr << "The bits per pixel the decoder serves is not useable by the player: " << bpp << endl;
     break;
 }
 
 if(w_org < 0  ||  h_org < 0) {
   err = true;
   cerr << "The decoder reports invalid dimensions: " << w_org << " x " << h_org << endl;
   w_org = 0;
   h_org = 0;
   pitch = 0;
 }
 else if(((bpp * w_org + 7) / 8) > pitch) {
   err = true;
   cerr << "Invalid line pitch value: " << pitch << endl;
 }
 
 if(err)
   return false;
 
 
 float lsec;
 GetLength(&numframes, NULL, &lsec);
 lentime = (int)(lsec * 1000);
 this->w_disp = w_org;
 this->h_disp = h_org;

 //process args after opening file to overwrite input format
 if( ProcessArgs(argc - 2, (argv + 2)) == false)
   return false;


 //it is probably the best, to init audio always
 // -> for streaming formats we don't know if audio appears later
 
 if(playAudio)
 {
   if( GetAudioFormat(&a_bps, &a_sign, &a_nch, &a_srate, &a_bigEndian) ) {   //essential for initializing audio playback
     a_frameSize = a_bps * a_nch / 8;
     if(a_frameSize <= 0) a_frameSize = 1;     //to prevent exceptions in case <0
   }
   else
     playAudio = false;
 }

 //set video dim
 gui->myResize(w_disp, h_disp);

 cout << "Player window dimension: " << w_disp << " x " << h_disp << endl;
 cout << "Player bits per pixel of decoded frame: " << bpp << endl;
 if(fps != 0)
   cout << "FPS: " << fps << endl;
 if((playAudio == true)  &&  (a_srate != 0))
   cout << "Sample rate: " << a_srate << endl;
 cout << "Length: " << (float)lentime / 1000 << "sec" << endl;
 cout << endl;
 
 if (seekatopen > 0) {
   cout << "seeking frame: " << seekatopen << endl;
   Seek(seekatopen);
 }
 return true;
}


/******************************************************************************/
//initialize
//call only once at the start
int AnimPlayer::init(int argc, const char** argv)
{
 //prevent double init
 if(init_done)
   return 0;

 ::system_init();   //system specific init
 playerPtr = this;

 if(argc <= 1) {
   PrintInfo();
 }

 //init SDL
 if(SDL_Init(SDL_INIT_VIDEO) < 0) {
    cout << "Unable to initialize SDL: " << SDL_GetError() << endl;
    return -1; //better quit here
 }
 
 //init vars
 audiocnt = 0;
 audiodata = NULL;
 audiopos = 0;
 
 extract = false;
  
 playing = true;
 ended = false;
 loop = false;
 ftype = IFFANIMPLAY_FT_NONE;
 speed = 1.0;
 a_srate = 0;
 fps = 0;
 playAudio = true;
 seekatopen = 0;

 //setup queue
 qframes = 0;  //frames currently in queue
 QueueFree();  //init

 //iff anim related settings
 loopanim = false;
 fixtime = false;

 const SDL_VideoInfo* info = SDL_GetVideoInfo();   //before any SDL screen is opened this gives us the the desktop screen format
 if(info != NULL) cout << "Player bits per pixel (desktop): " << (int)(info->vfmt->BitsPerPixel) << endl;


 //open window
 //the 8 bit mode is much faster (although for a 32 bit desktop it must be converted by SDL)
 //for 8 bit mode the color map must be copied to the "screen" surface before blitting the frame to it, else the default color map may result in wrong colors
 //(maybe 8 bit mode not a good idea when using a GUI, therefor true color mode needed, else widgets have wrong colors)


/*
 SDL_Surface *screen = SDL_SetVideoMode(320, 240, 0, SDL_SWSURFACE | SDL_RESIZABLE);
 if(screen == NULL) {
   cerr << "Unable to open SDL video surface: " << SDL_GetError() << endl;
   return -1;
 }
*/




 //Todo: open gui in std. window size before loading anim file (big files may delay GUI otherwise)


// SDL_Surface s_dummy;
 
 if(gui == NULL){
   //executeable path needed to load GUI gfx if not in CWD
   string exepath = argv[0];
   size_t pos;
   //convert all '\\' to '/'
   while( (pos = exepath.find('\\')) != string::npos )
     exepath.replace(pos, 1, "/");
   pos = exepath.find_last_of('/');
   if(pos == string::npos)
     exepath = "./";
   else
     exepath = exepath.substr(0, pos+1);
   
   gui = new AnimPlayerGui(NULL, exepath, this);
   gui->setupFont();
   gui->showGui = false;
 }

 //useful when stepping through frames
 SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

 init_done = true;

 if(argc > 1  &&  strlen(argv[1]) != 0) {
   //open videofile
   openFile(argc, argv);
 }
 
 //needed for opening gui when starting with closed file
 w_disp = gui->screen->w;
 h_disp = gui->screen->h;
 
cout << "starting app thread" << endl;
 
 //start "run" thread
 gui->threadAppStart();
// SDL_Delay(4000);
// gui->threadAppEnd();

 
 //quit normally
 return 0;
}

/******************************************************************************/
//this one should run in an own thread
void AnimPlayer::run(){
	
cout << "player run() method called" << endl;


 
//for debugging
//ftest.open("acbaudio.raw",ios::out | ios::binary);

 int ret = IFFANIMPLAY_NONE;
 while(1){
   close_file = false;
 
   if(extract)
     Extract(extr_path);  //extract video data without playing
   else if (ftype != IFFANIMPLAY_FT_NONE){ //if decoder open -> file open
     ret = Play();
     if(ret == IFFANIMPLAY_QUIT)
       break;
     //close file but keep player open
     cout << "Closing file" << endl;
     clear();
     UpdateCaption();
   }
   
   if(!file_to_open.empty()){
     const char *args[] = {"bla0","bla1"};
     args[1] = file_to_open.c_str();
     openFile(2, args);
     file_to_open.clear();
   }
   else {
     //idle, no file opened
     ret = Wait(8);
     if(ret == IFFANIMPLAY_QUIT)
       break;
   }
 }

//for debugging
//ftest.close();
}

/******************************************************************************/
void AnimPlayer::clear(){
 if(dispimg != NULL) {
   SDL_FreeSurface(dispimg);
   dispimg = NULL;
 }
 ended = false;
 DelDecoder();
 QueueFree();
 if(gui != NULL)
   gui->fillRect(NULL, 0, 0, 0); //blank
 UpdateCaption();
 //close any audio
 SDL_LockAudio(); //make sure callback is not running
 SDL_PauseAudio(1);
 if(audiodata != NULL){
   delete[] audiodata;
   audiodata = NULL;
 }
 SDL_UnlockAudio();
 file_name = "";
}

/******************************************************************************/
void AnimPlayer::ResetTimer()
{
 timer = 0;
 timerThres = 0;
 sdlStartTicks = SDL_GetTicks();
 lag = 0;
}

/******************************************************************************/
void AnimPlayer::SetTimeDelay(double delaysec)  
{
 timerThres += delaysec * 1000;
}

/******************************************************************************/
Uint32 AnimPlayer::GetDelayTimeMs()  
{
 timer = (double)(SDL_GetTicks() - sdlStartTicks);  //update timer here, to get exact remaining delay
 int towait = (int)(timerThres - timer);
 return (towait < 0 ?  0 : (Uint32)towait);
}

/******************************************************************************/
void AnimPlayer::SetLagTime()  
{
  this->lag = (SDL_GetTicks() - sdlStartTicks) - timerThres;
}






/******************************************************************************/
void AnimPlayer::QueueFree()
{
 while(qframes > 0) QueueDeleteFrame();  //delete all queue entries
 qh = 0;
 qt = 0;
 qsamples = 0;
 framecnt = 0;
 audiocnt = 0;
 q_init = false;
}

/******************************************************************************/
int AnimPlayer::QueueFill(int minFrames, int minSamples)
{
 int nframes = qframes;  //remember number of queued frames
 while((minFrames > qframes  ||  (minSamples > qsamples  &&  playAudio == true))  &&  QueueAddFrame());
 return (qframes - nframes);  //added frames
}

/******************************************************************************/
//must be thread save, interruption by requesting audio must be valid
bool AnimPlayer::QueueAddFrame()
{
 q_init = true;
 if(qframes >= IFFANIMPLAY_QUEUE_MAX) {
   cerr << "Can't load frame, queue is full!\n";
   return false;
 }
 if(ended)  //rewind required to start buffering again
   return false;
 
 //qt always points to the last valid entry, except when the queue is empty (wasn't filled once)
 int qt_temp = qt;
 if(qframes > 0) {
   qt_temp++;
   if(qt_temp >= IFFANIMPLAY_QUEUE_MAX) qt_temp -= IFFANIMPLAY_QUEUE_MAX;  //handle overflow -> ring buffer
 }
 APFrame* q = &(queue[qt_temp]);

 //create frame surface in original dimensions, pitch must be setable so we must allocate the pixel array manually (=> software surface only)
 //component masks byte order must match to data format written by "GetFrame()" => R,G,B
 void* pixels = (void*)(new char[pitch * h_org]);
 q->frame = SDL_CreateRGBSurfaceFrom(pixels, w_org, h_org, bpp, pitch, RMASK, GMASK, BMASK, 0);  //creates also palette for 8 bit mode
 if(q->frame == NULL) {
   cerr << "Couldn't create surface: " << SDL_GetError() << endl;
   delete[] (char*)q->frame->pixels;
   return false;
 }
 if(GetFrame(q->frame) == NULL) {   //if no more frames available (possibly EOF)
   delete[] (char*)q->frame->pixels;
   SDL_FreeSurface(q->frame);
   return false;
 }
 
 q->delay = GetFrameDelay();
 qframes++;
 q->frameno = framecnt;
 framecnt++;



 if(playAudio){
   q->asize = GetFrameAudioSize();     //get size of audio
   q->audio = new char[q->asize];
   GetFrameAudio(q->audio, q->asize);  //get audio data (written to the pointed buffer, in a valid format)
   qsamples += q->asize / a_frameSize;
   q->apos = audiocnt;
   audiocnt += q->asize / a_frameSize;
 }
 else {
   q->asize = 0;
   q->audio = NULL;
   q->apos = 0;
 }  

//cout << "queue debug " << q->frameno << "  " << q->asize << "  " <<  q->delay << endl;

 if( NextFrame() == false )  //let the decoder load the next frame, detect EOF
  ended = true;

 qt = qt_temp;  //first here -> thread safety

 return true;
}

/******************************************************************************/
//audio should always be ahead of frames -> not possible to delete yet unplayed data
void AnimPlayer::QueueDeleteFrame()
{
 if(qframes <= 0) return;
 
 //delete data
 APFrame* q = &(queue[qh]);
 if(q->frame != NULL) {
   delete[] (char*)q->frame->pixels;
   SDL_FreeSurface(q->frame);
 }
 if(playAudio) {
   qsamples -= q->asize / a_frameSize;
   if(q->audio != NULL) delete[] q->audio;
 }
 
 q->frame = NULL;
 q->audio = NULL;
 q->frameno = -1;
  qframes--;

 if(qh != qt) {   //head and tail points to same if only one entry in queue -> head isn't allowed to outstrip tail
   qh++;
   if(qh >= IFFANIMPLAY_QUEUE_MAX) qh -= IFFANIMPLAY_QUEUE_MAX;
 }
}






/******************************************************************************/
int AnimPlayer::QueueGetAudio(void* data, streamsize apos, int size)
{
#if IFFANIMPLAY_DEBUG
/*
 cout << ">>audio request: " << apos << " .. " << (apos+size) << endl;   //for debugging
 if(qframes > 0  &&  playAudio) {
   cout << "  audio buffered: " << queue[qh].apos << " .. " << (queue[qt].apos + queue[qt].asize) << " [" << queue[qh].frameno << ".." << queue[qt].frameno << "]" << endl;
   cout << "    buffered ahead: " << ((float)(((queue[qt].apos + queue[qt].asize) - apos - size) / a_frameSize) / a_srate) << "sec" << endl;
 }
 else
   cout << "  audio buffered: " << "0" << endl;
*/
#endif
   
 if(qframes <= 0) {    //no data buffered fill with silence
   memset(data, 0, size);
   return 0;
 }
 
 /* 5 cases must be handled: 
                ----
    ------------
    ----
       ----
            ----
     |________|      <- queued range "qh" to "qt", "---" above represents the possible requested range
     
     requested range parts outside the queued range are filled with "0"s
 */
 
 int copy_n;         //available data to copy from queue in bytes
 int copied_n;       //available data copied already (useful audio data)
 int pre_silence;    //silence to write at beginning of buffer
 int post_silence;   //silence to write at the end of buffer
 
 if(apos  <  queue[qh].apos) {  //requested start before queued range start
   pre_silence = queue[qh].apos - apos;
   if(pre_silence > size)  pre_silence = size;  
   memset(data, 0, pre_silence);
   data = (char*)data + pre_silence;   //so we can ignore "pre_silence" hereinafter
   size -= pre_silence;
 }  
 else
   pre_silence = 0;



 if((apos + size)  >  (queue[qt].apos + queue[qt].asize)) {  //requested end beyond queued range end
   post_silence = (apos + size) - (queue[qt].apos + queue[qt].asize);
   if(post_silence > size) post_silence = size;
   memset((char*)data + size - post_silence, 0, post_silence);
 }
 else
   post_silence = 0;

 copy_n = size - post_silence;  //valid data that can be copied
 copied_n = 0;     //of "size"

 
 //at this point all the silence is set, and we know how much valid data can be copied from the buffer
 //we only have to locate and copy the valid data from the single queue entries and put it into the "data" buffer
 
 int i = 0;
 while((copy_n > 0)  &&  (i < qframes))   //find "apos" in the queue and copy data
 {
   struct APFrame* qe = &queue[(qh+i) % IFFANIMPLAY_QUEUE_MAX];  //get queue entry ptr for simpler access (current audio should match the current frame as optimum)
   int offset;   //offset from the start of the frame audio
   int tocopy;

   if(apos < (qe->apos + qe->asize)) //if requested audio data is in this frame (or starts in a frame before -> normally never)
   {
     if(copied_n == 0)  //if no data copied yet -> locate offset
       offset = apos - qe->apos;    
     else    //else we can start at byte 0 of the frame
       offset = 0;

     tocopy = qe->asize - offset;  //rest of the frame audio
     if(tocopy > copy_n)  //limit
       tocopy = copy_n;

     memcpy((char*)data + copied_n, qe->audio + offset, tocopy);
     
//ftest.write(qe->audio + offset, tocopy);

     copy_n -= tocopy;
     copied_n += tocopy;
     apos += tocopy;
   }

   i++;
 }
 
 //print message if requested data is not completly in the queue
 //"post_silence" is ok at the end of the video, "pre_silence" is not
 if(copied_n == 0) {
   cout << "None of the requested audio data in buffer: pos " << apos  << " to " << apos + size << endl;  //buffer, or timing problem
 } 
 else if(pre_silence > 0)
   cout << "Audio data requested that is not in buffer anymore. Audio playback lags.\n";

#if IFFANIMPLAY_DEBUG
/*
 if(copied_n == 0)
   cout << " Either video lags or buffer is too small" << endl;
 cout << ">>valid: " << copied_n << "  presilence: " << pre_silence  <<  "  postsilence: " << post_silence << "    at frame: " << queue[qh].frameno << "  SDL_TICKS: " << SDL_GetTicks() << endl;  //for debugging
*/
#endif
 
 

 
 return copied_n;
}





/******************************************************************************/
int AnimPlayer::Play()
{

 //at this point video file is opened
 
 if(dispimg != NULL)
   SDL_FreeSurface(dispimg);
 
 //create display surface with format of animation (8 or 24 bits) => surface for scaled frame
 this->dispimg = SDL_CreateRGBSurface(SDL_SWSURFACE, w_disp, h_disp, bpp, RMASK, GMASK, BMASK, 0);
 if(dispimg == NULL) {
   cerr << "Couldn't create surface: " << SDL_GetError() << endl;
   return IFFANIMPLAY_QUIT;
 }
 
 //extend screen size (for GUI)
 //gui->myResize(w_disp, h_disp); already called after opening

 //init SDL audio if audio data available
 if(playAudio)
 {
   if((a_nch > 2) || (a_nch <= 0) || ((a_bps != 8) && (a_bps != 16))  ||  (a_srate <= 0))   //check for invalid vars (depends on SDL_AUDIO)
     cerr << "Invalid audio format, audio not playable" << endl;
   else
   {
     SDL_AudioSpec desired;
     
     desired.freq = (int)a_srate;
     desired.channels = a_nch;
     desired.samples = IFFANIMPLAY_SMPBUFSIZE;
     desired.callback = AnimPlayer::audio_callback;
     if(a_bps == 8) {
       if(a_sign) desired.format = AUDIO_S8;
       else     desired.format = AUDIO_U8;
     }
     else if(a_bigEndian) {
       if(a_sign) desired.format = AUDIO_S16MSB;
       else     desired.format = AUDIO_U16MSB;
     }  
     else {
       if(a_sign) desired.format = AUDIO_S16LSB;
       else     desired.format = AUDIO_U16LSB;
     }
     desired.userdata = (void*)this;              //pointer to the player instance
     //Open the audio device, forcing the desired format
     if ( SDL_OpenAudio(&desired, NULL) < 0 ) {
       cerr << "Couldn't open SDL audio: " << SDL_GetError() << endl;
       playAudio = false;
     }
     else {        //start playing
       cout << "SDL audio initialized" << endl;
       audiodata = new char[IFFANIMPLAY_SMPBUFSIZE * a_frameSize];  //buffer for callback
     }
   }
 }

 //we need to calculate a suitable buffer size
 // - so that audio doesn't stutter (callback is is equivalent to a thread)
 // - so that enough video data is available (only needed if loading is put into separate thread)
 {
   minAudioBuf = IFFANIMPLAY_BUFFER_TIME * a_srate * a_frameSize;
   minFrameBuf = (int)(fps * IFFANIMPLAY_BUFFER_TIME);
   
   if(fps <= 0)        //if not a frame based video format, set to minimum
     minFrameBuf = 2;  //minimum is always 2! (current and next frame)
 }



 //playing loop
 QueueFill( minFrameBuf, (int)minAudioBuf );   //prebuffer frames and audio
 if(playAudio) SDL_PauseAudio(0);              //start playing of audio (after prebuffering)
 ResetTimer();  
 int ret = IFFANIMPLAY_NONE;

 while(1)
 {
    UpdateCaption();               //set window title info
    
    //now we need an own copy of the original sized frame, which must be kept extra for a possile window resize, (to resize before the following frame is shown on the screen)
    //this surface is than scaled to a larger surface with window dimensions
    //the scaled surface is then blitted to the screen and automatically converted to the screen specs (screen might be 16 bit or "0BGR" order but we don't have to care this way)
    //=> in OGL mode only a texture is made from the original sized frame
    //before sending this thread to sleep for the delay we already make the following frame ready => double buffer (without resizing to save some memory)

    Scale(queue[qh].frame, dispimg);  //scale bitmap (8 or 24 bit), don't blit to screen directly so SDL can convert properly: e.g. color channel order

    //blit to screen
    if(gui->screen->format->BitsPerPixel <= 8)
      SDL_SetColors(gui->screen, dispimg->format->palette->colors, 0, 256);    //for 8 bit surfaces: we have 8 bit screen so copy palette before blitting
    DrawFrame();

    SetLagTime();      //detect lag right after the new frame is displayed => always a bit positive because of scaling and blitting (normally the frame must flip right after the waiting loop => avoid by using a proper double buffer)
    
    SetTimeDelay(queue[qh].delay); //set time delay of frame

    //take delay into account when filling buffer
    QueueFill( minFrameBuf, (int)minAudioBuf + (int)(a_srate * a_frameSize * queue[qh].delay));        //make sure we have enough audio data; at least 2 frames are needed => the old + the new (which might not be available anymore)

    ret = Wait((int)(queue[qh].delay * 1000));     //waits automatically when the last frame is reached and loop is deactivated
    if(ret != IFFANIMPLAY_NONE  ||  !file_to_open.empty())
      break;
      
    if(qframes > 1)
      QueueDeleteFrame();  //queue must contain at least 1 frame; increases "qh"

    //if last frame is reached, loop if requested
    if(loop && (queue[qh].frameno == (numframes - 1))) {
      Rewind();
    }

 } // end for nframes



 SDL_PauseAudio(1);  //stop audio (callback thread), else the program crashes when "anim" is destructed
 return ret;
}

/******************************************************************************/
bool AnimPlayer::JumpRel(int d){
	if(ftype == IFFANIMPLAY_FT_NONE)
		return false;
	if(d == 0){
		//display current frame
		Scale(queue[qh].frame, dispimg);
		if(gui->screen->format->BitsPerPixel <= 8)
			if(dispimg != NULL  &&  dispimg->format->palette != NULL)
				SDL_SetColors(gui->screen, dispimg->format->palette->colors, 0, dispimg->format->palette->ncolors);
		DrawFrame();
		UpdateCaption();
		return true;
	}
	if(d == 1){
		if(qframes <= 1)
			QueueAddFrame();
		if(qframes > 1) { //the next frame must be in the buffer
			QueueDeleteFrame();
			audiopos = queue[qh].apos + queue[qh].asize;  // resync audio with next frame
			Scale(queue[qh].frame, dispimg);  //scale bitmap (8 or 24 bit), don't blit to screen directly so SDL can convert properly: e.g. color channel order

			//blit to screen
			if(gui->screen->format->BitsPerPixel <= 8)
				SDL_SetColors(gui->screen, dispimg->format->palette->colors, 0, 256);    //for 8 bit surfaces: we have 8 bit screen so copy palette before blitting
			DrawFrame();
			UpdateCaption();
			return true;
		}
	}
	
	return false;
}

/******************************************************************************/
void AnimPlayer::DrawFrame()
{
 SDL_Rect dstrect;
 dstrect.x = 0;
 dstrect.y = 0;
 dstrect.w = gui->screen->w;
 dstrect.h = gui->screen->h;

 if(gui->showGui) {
	gui->render();
	dstrect.y = gui->height;
 }

 if (dispimg != NULL)
	SDL_BlitSurface(dispimg, NULL, gui->screen, &dstrect);
	
 SDL_Flip(gui->screen);
}

/******************************************************************************/
//resize drawing surface (screen/window already resized by GUI!)
void AnimPlayer::ResizeRefit(int w, int h)
{
 //w x h is whole screen size with GUI, w_disp x h_disp is the video frame size
 
 if(w <= 0 || h <= 0) {
   w = 1;
   h = 1;
 }
 
 this->w_disp = w;
 this->h_disp = h;
 
 if(gui->showGui){
   h += gui->height;
 }


 //dispimg must be resized, surface content obsolete
 if(dispimg == NULL  ||  w_disp != dispimg->w  ||  h_disp != dispimg->h)
 {
   SDL_Surface* tempsurf;
   tempsurf = SDL_CreateRGBSurface(SDL_SWSURFACE, w_disp, h_disp, bpp, RMASK, GMASK, BMASK, 0);
   if(dispimg != NULL)
     SDL_FreeSurface(dispimg);
   dispimg = tempsurf;
   if(dispimg == NULL) {
     cerr << "Can't allocate surface" << endl;
     return;
     //exit(1);
   }
 }
 
 if(q_init)
   Scale(queue[qh].frame ,dispimg);   //scale current original frame to new dimensions

 //in full screen mode, don't change the screen, just scale the content
 //in window mode, remember the old window dimension, and try to restore, in case the new video mode dim is not supported
 //multiplying the screen size can lead to video mode failure (window not bigger than screen allowed on MorphOS)


 //reinit screen with new dimensions, and same flags

// gui->resizeWindow(w, h);  would be recursive
/*
 this->screen = SDL_SetVideoMode(w, h, 0, screen->flags);

 if(this->screen == NULL) {
   cerr << "Unable to resize (open new) SDL video surface: " << SDL_GetError() << endl;
   exit(1);
 }

 if(bpp <= 8)
   SDL_SetColors(screen, dispimg->format->palette->colors, 0, 256);    //copy palette for 8 bit surfaces to screen palette
*/


 //blit resized frame to screen
 DrawFrame();

}





int delayms;
int delayWaitStart;
int delayThres;
int delayPassed;
bool stop_wait;

/******************************************************************************/
//wait for "delayms" milliseconds (always waits at least 1 ms)
// - handle events (user input)
// - stay in the delay loop when anim has ended or is paused
int AnimPlayer::Wait(int delayms_)
{
 SDL_Event event;

 delayms = delayms_;
 if(delayms < 0)
   delayms = 0;
 delayWaitStart = SDL_GetTicks();       //sdl ticks at start of waiting loop
 delayThres = delayWaitStart + delayms; //determine threshold when delay is elapsed and waiting loop must be left
 delayPassed = 0;                       //stores how long we had stayed in the waiting loop already until "pause" was triggered (since start of function)

 stop_wait = false;

 //check for last frame
 if((ended && qframes == 1)  &&  !(loop)) {
   UpdateCaption();  //to show "ended" status
 }
 else
   UpdateCaption(); //always update
 
 //delay loop
 do
 {
/*
//test
static int i = 0;
SDL_Rect r = {10,10,100,100};
gui->fillRect(&r, i*10, i*10, i*10);
//cout << (int)i << endl;
i++;
*/
 if(playing)
	;//cout << (stop_wait || (ended == true  &&  qframes == 1) || (playing == false) || (delayThres > SDL_GetTicks())) << endl;

    gui->eventPoll();
 
    SDL_Delay(1); //sleep a little to relieve CPU usage; a minimum value of 1 ensures the CPU usage is never used up completely by the player (also if "delayms" parameter is "0")

    if(gui->signal_appThreadEnd)
      return IFFANIMPLAY_QUIT;
    
    if(!file_to_open.empty())
      close_file = true;
    
    if(close_file)
      return IFFANIMPLAY_CLOSE;



    if(delayThres > SDL_GetTicks())
       stop_wait = true;

   
 } while((ended == true  &&  qframes == 1) || (playing == false) || (delayThres > SDL_GetTicks()));

 return IFFANIMPLAY_NONE;
}




/******************************************************************************/
void AnimPlayer::eventHandler(SDL_Event *e){

	switch(e->type)
	{
		case SDL_QUIT:          //if window is closed
			gui->signal_appThreadEnd = true;
			return;
			break;

		case SDL_VIDEOEXPOSE:
			SDL_Flip(gui->screen);    //redraw screen
			break;

		case SDL_KEYDOWN:      //key event
			switch(e->key.keysym.sym)
			{
				case SDLK_0:
					gui->myResize(w_org / 2, h_org / 2);
					break;
				case SDLK_1:
					gui->myResize(w_org, h_org);
					break;
				case SDLK_2:
					gui->myResize(w_org * 2, h_org * 2);
					break;
				case SDLK_3:
					gui->myResize(w_org * 3, h_org * 3);
					break;

				case SDLK_ESCAPE:
					gui->signal_appThreadEnd = true;
					return;
					break;

				case SDLK_SPACE:          //handle pause/play
					if(playing == true) {   //stop if currently playing
						if(playAudio) SDL_PauseAudio(1);
						playing = false;
						delayPassed += SDL_GetTicks() - delayWaitStart;  //needed to resynchronize audio when unpausing; must accumulate (multiple pause/unpause during a waiting period possible)
					}
					else {                  //start playing if currently stopped
						playing = true;
						delayWaitStart = SDL_GetTicks();
						delayThres = delayWaitStart + (delayms - delayPassed);  //set threshold to remaining delay when leaving pause mode
						ResetTimer();   //to correct "timerThres"
						SetTimeDelay( (double)(delayms - delayPassed) / 1000.0 );

						if(playAudio) {
							audiopos = queue[qh].apos + ( (int)((double)delayPassed / 1000 * a_srate) * a_frameSize );    //restore audio position (resynchronize)
						}
						QueueFill( minFrameBuf, (int)minAudioBuf );  //prebuffer before starting to play
						if(playAudio) SDL_PauseAudio(0);
					}
					UpdateCaption(); 
					break;

				case SDLK_RIGHT:
					if(JumpRel(1))
						cout << "Stepping forward 1 frame" << endl;
					break;

				case SDLK_LEFT:
					if(queue[qh].frameno > 0 )
						if( Seek( queue[qh].frameno - 1 ) ){
//						return IFFANIMPLAY_NONE;
							stop_wait = true;
						}
					break;

				case SDLK_c:
					close_file = true;
					return;
					break;

				case SDLK_l:
					cout << "Loop: " << (ToggleLoop() ? "on" : "off") << endl;
					break;

				case SDLK_BACKSPACE:
				case SDLK_r:               // handle reset to first frame; leave on reset and let the main loop load the first frame
					if(ftype == IFFANIMPLAY_FT_NONE)
						break;
					cout << "Rewinding" << endl;
					SDL_PauseAudio(1);
					playing = false;         // stop after reset (playing must be enabled manually)
					framecnt = 0;
					ended = false;
					Rewind();
					QueueFree();
					QueueFill( 1, (int)(minAudioBuf) );   //setup buffer
					ResetTimer();
					JumpRel(0);
//				return IFFANIMPLAY_NONE;
//					stop_wait = true;
					break;
				
				case SDLK_g:
					if(gui->showGui)
						gui->showGui = false;
					else
						gui->showGui = true;
					gui->myResize(w_disp, h_disp);
					break;
				
				//iffanim specific
				case SDLK_k:
					if(ftype == IFFANIMPLAY_FT_IFFANIM){
						if(loopanim){
							loopanim = false;
							dec_anim->SetLoopAnim(false);
						}
						else {
							loopanim = true;
							dec_anim->SetLoopAnim(true);
						}
						cout << "Loopanim (skip 2 last frames): " << (loopanim ? "on" : "off") << endl;
						float lensec;
						GetLength(&numframes, NULL, &lensec);
						lentime = (int)(lensec * 1000);
					}
					break;
			}

			
			break; //break SDL_KEYDOWN
		
		case SDL_SYSWMEVENT:
			system_event_proc(e->syswm.msg, this);
			break;
			
		default:
			//cerr << "Unknown event received" << endl;
			break;
	
	} //end switch
}

