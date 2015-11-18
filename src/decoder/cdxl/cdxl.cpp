#include "cdxl.h"
#include "../amiga_conv.h"

/******************************************************************************/
int CDXL::GetInfo(int* w, int* h, int* bpp_out, int* nframes_)
{
 if(w != NULL) *w = infoCur.w;
 if(h != NULL) *h = infoCur.h;
 if(bpp_out != NULL) {
   if(infoFirst.ven == CDXL_HAM)
     *bpp_out = 24;
   else if(infoFirst.ven == CDXL_RGB  ||  infoFirst.ven == CDXL_YUV)
     *bpp_out = (infoFirst.bitplanes + 7) / 8 * 8; //full bytes per pixel only
   else 
     *bpp_out = 0;    //this should make the player abort -> for all unsupported encodings
 }
 if(nframes_ != NULL) *nframes_ = this->nframes;
}

/******************************************************************************/
bool CDXL::GetAudioFormat(int* channels, int* bits)
{
 if(bits != NULL) *bits = 8;
 if(channels != NULL) *channels = infoFirst.nch;

 if(infoFirst.audiosize == 0)
   return false;
 return true;
}


/******************************************************************************/
bool CDXL::SeekFrameAbs(int frame)
{
 if(frame < 0  ||  frame >= nframes)
   return false;

 stream.seekg(frame * chunksize, ios::beg);
 NextFrame();  //read frame
}

/******************************************************************************/
bool CDXL::SeekFrameRel(int rframes)
{
 return SeekFrameAbs(curFrame + rframes);
}


/******************************************************************************/
CDXL::CDXL()
{
 sStartPos = 0;
 chunk.data = NULL;
 InitVars();
}


/******************************************************************************/
CDXL::~CDXL()
{
 Close();
}

/******************************************************************************/
void CDXL::InitVars()
{
 curFrame = 0;
 ready = false;
 fchange = 0;
}



/******************************************************************************/
//Open and init CDXL data using an open stream
int CDXL::Open(ifstream* s)
{
 if(!s->is_open()) {
   if(sStartPos != (streampos)-1)
     printf("Error, stream not opened");
   return -1;
 }

 streamPtr = s;
 if(sStartPos != (streampos)-1)
   sStartPos = s->tellg();  //memorize stream start position 
 
 //read first chunk to buffer completly with header
 if( ReadChunk( &infoFirst ) == false )
   return -2;
 curFrame = 0;

 //copy struct, init
 infoCur = infoFirst;
 infoPrev = infoFirst;  //to make it valid


 //try to determine the filesize
 streamsize curpos = s->tellg();
 s->seekg(0, ios::end);          //seek end
 streamsize fsize = s->tellg();  //64 bit
 s->seekg(curpos, ios::beg);     //seeking back

 if(chunksize != 0)
   nframes = (int)(fsize / (streamsize)chunksize);  //get number of frames
 else
   nframes = 0;


 //create format info string
 strcpy(infostring, GetChunkInfo( &infoFirst ));

 ready = true;
 return 0;
}


/******************************************************************************/
//Open file stream by file name
int CDXL::Open(const char* fname)
{
 stream.open(fname, ios::in | ios::binary);
 if(stream.is_open() == false) {
   printf("Can't open file \"%s\"", fname);
   return -1;
 } 

 sStartPos = -1;
 return Open(&stream);
}

/******************************************************************************/
void CDXL::Parse()
{
 if(ready == false) {
   cout << "cannot parse CDXL stream, stream is not ready" << endl;
   return;  
 }

 //start parsing at the current chunk position

 //get data of current chunk
 streamsize nf = 1;                  //number of frames
 streamsize ad = infoCur.audiosize;  //bytes of audio data
 streamsize bd = infoCur.bitmapsize; //bytes of bitmap data
 streamsize cd = infoCur.cmapsize;   //bytes of color map data
 int start_fchange = this->fchange;
 
 bool rel = false;
 
 if(stream.tellg() != (streampos)infoFirst.csizeCur) {
   cout << "Parsing started not at the beginning of the file\n";
   rel = true;
 }

 while(NextFrame()) {
   nf++;
   ad += infoCur.audiosize;
   bd += infoCur.bitmapsize;
   cd += infoCur.cmapsize;
 }
 
 cout << "Frames: " << nf << "\n";
 cout << "Audio data: " << ad << " bytes\n";
 cout << "Bitmap data: " << bd << " bytes\n";
 cout << "Color map data: " << cd << " bytes\n";
 cout << "Format changes detected: " << fchange - start_fchange << "\n";
 
 if(rel)
   cout << "(From start of parsing position)" << endl;
   
 cout << endl;
   
}


/******************************************************************************/
//close before opening a new file
void CDXL::Close()
{
 if(stream.is_open())
 {
   if(stream.is_open()) stream.close();
   //close buffers
   if(chunk.data != NULL) {
     delete[] chunk.data;
     chunk.data = NULL;
   }
 }
 InitVars();
 sStartPos = 0;
}

/******************************************************************************/
bool CDXL::Rewind()
{
 streamPtr->clear();   //clear stream errors
 if(sStartPos == (streampos)-1)   //if file was opened directly
   streamPtr->seekg(0, ios::beg);
 else
   streamPtr->seekg(sStartPos, ios::beg);
     
 InitVars();
 
 if( Open(streamPtr) == 0 )
   return true;
 else
   return false;
}

/******************************************************************************/
bool CDXL::IsReady()
{
  return ready;
}




/******************************************************************************/
char* CDXL::GetFrame(char* dst, int pitch)
{
 int err = 0;

 if(infoCur.ven == CDXL_HAM  &&  infoCur.por == CDXL_BIT_PLANAR)  //HAM conversion
   err = convertHamTo24bpp(dst, chunk.bitmap, cmap24, infoCur.w, infoCur.h, infoCur.bitplanes, pitch, 0);
 else if(infoCur.ven == CDXL_RGB  &&  infoCur.por == CDXL_BIT_PLANAR)  //planar to chunky 8 bit
   err = bitPlanarToChunky(dst, chunk.bitmap, infoCur.w, infoCur.h, infoCur.bitplanes, ((infoCur.bitplanes + 7) / 8 * 8), pitch);
 else if(infoCur.ven == CDXL_RGB  &&  infoCur.por == CDXL_CHUNKY  &&  (infoCur.bitplanes % 8) == 0)  //simple copy
   memcpy(dst, chunk.bitmap, infoCur.w * (infoCur.bitplanes / 8) * infoCur.h);
 else {
   printf("Don't know how to decode the frame %d\n", curFrame);
   err = -1;
 }
   
 if(err != 0) {
   printf("CDXL frame decoding failed\n");
   return NULL;
 }
 
 return dst;
}

/******************************************************************************/
char* CDXL::GetCmap()
{
 if(infoCur.cmapsize <= 0)
   return NULL;
 return cmap24;
}


/******************************************************************************/
char* CDXL::GetFrameRaw(int* datasize)
{
 if(stream.is_open() == false  ||  infoCur.bitmapsize == 0)
 {
  if(datasize != NULL)
    *datasize = 0;
  return NULL;
 }

 if(datasize != NULL) *datasize = infoCur.bitmapsize;

 return chunk.bitmap;
}



/******************************************************************************/
//read complete chunk into chunk buffer, if buffer is too small it is resized 
// return false if data couldn't be read (EOF)
//   ->  if a read array occures the class attributes may contain wrong information and the file position pointer has to be reset, the only way to fix them is to "rewind()"
// "info" info struct to fill the data to
bool CDXL::ReadChunk(cdxl_chunkinfo* info )
{
 static unsigned char header[CDXL_CHUNKHEADSIZE];  //buffer for a chunk header

 //read and interpret chunk header (BIG ENDIAN), error check afterwards
 stream.read((char*)header, CDXL_CHUNKHEADSIZE);

 if(stream.fail()) {
   printf("Error, reading failed");
   if(stream.eof())
     printf(", EOF reached");
   printf("\n");
   ready = false;
   return false;
 }

 info->type = header[0];        //byte 0, bit 0..7
 info->ven = header[1] & 0x03;  //byte 1, bit 0..1
 info->por = header[1] & 0xe0;  //byte 1, bit 5..7
 info->nch = ((header[1] & 0x10) >> 4 ) + 1;    //byte 1, bit 4  (audio channels)
 info->csizeCur   = (header[2]  << 24) | (header[3]  << 16) | (header[4]  << 8) | header[5];
 info->csizePrev  = (header[6]  << 24) | (header[7]  << 16) | (header[8]  << 8) | header[9];
 info->fnumber = (header[10] << 24) | (header[11] << 16) | (header[12] << 8) | header[13];
 info->w = (header[14] << 8) | header[15];
 info->h = (header[16] << 8) | header[17];
 info->bitplanes = header[19];
 info->cmapsize  = (header[20] << 8) | header[21];
 info->audiosize = (header[22] << 8) | header[23];
 
 //calc size of the bitmap in bytes, whose byte size is not given directly in the header
 {
   //size of a single bitplane line is assumed to be a multiple of 16 bit always (Amiga hardware related)
   if(info->por == CDXL_BIT_PLANAR) {
     int planesize = (info->w + 15) / 16 * 2;  //in bytes
     info->bitmapsize = planesize * info->bitplanes * info->h;  // pitch * h
   }
   else { //pitch of other "pixel orientations" unkown yet, for now we assume the same as for bit planar
     int planesize = (info->w + 15) / 16 * 2;
     info->bitmapsize = planesize * info->bitplanes * info->h;
   }

   //compare the calculated bitmap size with the chunk size
   int bitmapsize = info->csizeCur - CDXL_CHUNKHEADSIZE - info->cmapsize - info->audiosize;
   if(bitmapsize != info->bitmapsize) {
     info->bitmapsize = bitmapsize;    //the value indirectly given by the size of the other chunk sections should be more correct (maybe there are padding bytes)
     printf("Warning, size of bitmap data larger than needed or incorrect chunk size value in header\n");
   }
 }
 
 //verify information, if data is invalid cdxl stream should be closed
 switch(info->type) {
   case CDXL_CUSTOM:
   case CDXL_STANDART:
   case CDXL_SPECIAL:
     break;
   default:
     printf("Unknown CDXL type \"0x%x\"\n", info->type);
     return false;
     break;
 }

 switch(info->ven) {
   case CDXL_RGB:
   case CDXL_HAM:
   case CDXL_YUV:
   case CDXL_AVM_DCTV:
     break;
   default:
     printf("Unknown video encoding \"0x%x\"\n", info->ven);
     return false;
     break;
 }

 switch(info->por) {
   case CDXL_BIT_PLANAR:
   case CDXL_BYTE_PLANAR:
   case CDXL_CHUNKY:
   case CDXL_BIT_LINE:
   case CDXL_BYTE_LINE:
     break;
   default:
     printf("Unknown pixel orientation \"0x%x\"\n", info->por);
     return false;
     break;
 }
 
 
 
 //check for illeagal combinations; optimum would be, if this covers every illeagal combination
 if(info->ven == CDXL_HAM) {  //only bit planar HAM6 or HAM8
   if(info->por != CDXL_BIT_PLANAR  ||  (info->bitplanes != 6  &&  info->bitplanes != 8)) {
     printf("Illeagal attributes for HAM mode type\n");
     return false;
   }
   
   if(info->bitplanes == 6){
     if(info->cmapsize < 16*2) {printf("Color map too small for HAM6\n"); return false;}
     if(info->cmapsize > 16*2) printf("Color map too large for HAM6\n");
   }
   else if(info->bitplanes == 8){
     if(info->cmapsize < 64*2) {printf("Color map too small for HAM8\n"); return false;}
     if(info->cmapsize > 64*2) printf("Color map too large for HAM8\n");
   }
 }
 else {
  if(info->cmapsize > 256*2)
    printf("Warning, color map is too large\n");
  if(info->cmapsize % 1)
    printf("Warning, color map size is not a multiple of 2 bytes\n");
          
  if( (info->ven != CDXL_AVM_DCTV)  &&  ((info->cmapsize / 2) != (1 << (info->bitplanes))) )
    printf("Warning, size of color map does not fit to number of bitplanes\n");

  if(info->ven == CDXL_RGB) {
   if(info->bitplanes > 8  &&  info->cmapsize > 0)
     printf("Warning, color map available which is not used for more than 8 bits per pixel\n");
  }
 }
 

 //check values for uncommon ranges indicating a corrupted header   
 if((info->w > 2000)  ||  (info->w <= 0)  ||  (info->h > 2000)  ||  (info->h <= 0))  
   printf("Dimension values seems corrupted\n");

 if(((info->nch % 2) != 1)  &&  (info->audiosize % 1))
   printf("Warning, size of audio data is not a multiple of 2 byte for an even number of channels\n");



 //format warnigs
 //print a message if the reserved bytes are not 0x00 each => maybe we find a new undocumented format information in the reserved fields
 char resOffs[] = {18,24,25,26,27,28,29,30,31};  //offset of reserved bytes in header 
 int rbytes = sizeof(resOffs);;  //number of reserved bytes (=> sizeof behaves differerenty for static arrays)
 for(int i = 0; i < rbytes; i++) {
   if(header[resOffs[i]] != 0x00) {
     printf("Warning, reserved bytes of header not 0x00 as expected:\n ");
     for(int j = 0; j < rbytes; j++)   //now print all reserved bytes
        printf("%d:0x%02x, ", resOffs[j], header[resOffs[j]]);
     printf("\n");
     break;
   }
 }
 
  //print warnings for all formats that aren't supported (unknown to the programmer) yet
 if(info->ven == CDXL_AVM_DCTV)
   printf("Warning, AVM & DCTV is not supported\n");
 if(info->por == CDXL_BIT_LINE)
   printf("Warning, BIT_LINE is not supported\n");
 if(info->por == CDXL_BYTE_LINE)
   printf("Warning, BYTE_LINE is not supported\n");
 //YUV is also not supported but might show something
   
   
 
 //chunk seems to be good, so allocate chunk buffer:  header + cmap + bitmap + sound
 chunksize = info->csizeCur;
 int chunksize_req = CDXL_CHUNKHEADSIZE + info->cmapsize + info->bitmapsize + info->audiosize; //buffer needed for the complete chunk 
 if(chunksize_req != chunksize)    //if chunksize has changed
 {
   printf("Warning chunk size has changed: %d -> %d\n", chunksize, chunksize_req);
   if(chunk.data != NULL)
     delete[] chunk.data;
   chunk.data = new char[chunksize_req];
   chunksize = chunksize_req;      //variable chunksize possibly never occur, may cause problems
 }
 else if(chunk.data == NULL)
   chunk.data = new char[chunksize];

 chunk.header = chunk.data;
 chunk.cmap = chunk.header + CDXL_CHUNKHEADSIZE;
 chunk.bitmap = chunk.cmap + info->cmapsize;
 chunk.audio = chunk.bitmap + info->bitmapsize;

 memcpy(chunk.header, header, CDXL_CHUNKHEADSIZE); //copy header, although not needed anymore
 stream.read(chunk.data + CDXL_CHUNKHEADSIZE, chunksize - CDXL_CHUNKHEADSIZE);  //read rest of the current chunk into buffer

 if(stream.fail()) {
   printf("Reading failed");
   if(stream.eof())
     printf(", EOF reached");
   else
     printf(", unspecified error");
   printf("\n");
   ready = false;
   return false;
 }


 //convert the color table: 16 (4 bit per component) to 24 bits (8 bit per component)
 if(infoCur.cmapsize > 0)
 {
   unsigned char* cmap16 = (unsigned char*)chunk.cmap;  //cmap starts right at the start of the chunk buffer
   int colors = infoCur.cmapsize / 2;
   if(colors > 256) colors = 256;
   for(int i = 0; i < colors; i++)
   {
     cmap24[i*3+0] = (cmap16[i*2+0] & 0x0f) << 4;  //R
     cmap24[i*3+1] = (cmap16[i*2+1] & 0xf0);       //G
     cmap24[i*3+2] = (cmap16[i*2+1] & 0x0f) << 4;  //B
   }
   memset(cmap24 + colors * 3, 0, (256 - colors) * 3);  //set rest to 0 (black)
 }

 return true;
}


/******************************************************************************/
bool CDXL::CompareInfoDiff(cdxl_chunkinfo* a, cdxl_chunkinfo* b)
{
 if((a->nch != b->nch) ||
    (a->w != b->w) ||
    (a->h != a->h))
   return true;

 //first prepare a string with all detected changes, then print the result
 ostringstream s;

 if(a->type != b->type)
   s << "type: " << a->type << " -> " << b->type << endl;
 if(a->ven != b->ven)
   s << "video encoding: " << a->ven << " -> " << b->ven << endl;
 if(a->por != b->por)
   s << "pixel orientation: " << a->por << " -> " << b->por << endl;
 if(a->nch != b->nch)
   s << "number of audio channels: " << a->nch << " -> " << b->nch << endl;
 if(a->w != b->w)
   s << "width: " << a->w << " -> " << b->w << endl;
 if(a->h != b->h)
   s << "height: " << a->h << " -> " << b->h << endl;
 if(a->bitplanes != b->bitplanes)
    s << "bitplanes: " << a->bitplanes << " -> " << b->bitplanes << endl;
 if(a->cmapsize != b->cmapsize)
    s << "color map size: " << a->cmapsize << " -> " << b->cmapsize << endl;
 if(a->audiosize != b->audiosize)
   s << "audio data size: " << a->audiosize << " -> " << b->audiosize << endl;
 if(a->bitmapsize != b->bitmapsize)
   s << "bitmap size: " << a->bitmapsize << " -> " << b->bitmapsize << endl;

 if(s.str().size() > 0) {
   fchange++;
   cout << "change in chunk header " <<  a->fnumber << " -> " << b->fnumber << " detected" << endl;
   cout << s.str();
 }

 
 return false;
}


/******************************************************************************/
int CDXL::GetCurFrameNum()
{
 if(stream.is_open() == false)
   return -1;
 return curFrame;    
}

/******************************************************************************/
int CDXL::GetChunkSize()
{
  return chunksize;
}





/******************************************************************************/
float CDXL::GetFpsBySampleRate(int samplerate)
{
 float fps;
 
 if(infoCur.nch == 0)
   return 0;
 float spf = (float)infoCur.audiosize / (float)infoCur.nch;  //number of sample frames per audio block from a chunk

 if(spf == 0)
   return 0; 
 return samplerate / spf;
}

/******************************************************************************/
float CDXL::GetFpsByDataRate(int bytesPerSec)
{
 if(chunksize == 0) return 0;
 return bytesPerSec / (float)chunksize;
}

/******************************************************************************/
float CDXL::GetSampleRateByFps(float fps)
{
 float sps = fps * (float)infoFirst.audiosize;  //samples per second  (fps == chunks per second)
 return sps / infoFirst.nch;    //mind number of channels
}

/******************************************************************************/
float CDXL::GetDataRateByFps(float fps)
{
 return fps * infoFirst.csizeCur; //chunks per second
}



/******************************************************************************/
char* CDXL::GetAudio(int* datasize)
{
 if(stream.is_open() == false  ||  infoCur.audiosize == 0)
 {
   if(datasize != NULL)
     *datasize = 0;
   return NULL;
 }
  
 if(datasize != NULL)
   *datasize = infoCur.audiosize;
   
 return chunk.audio;
}


/******************************************************************************/
bool CDXL::NextFrame()
{
 infoPrev = infoCur;
 bool ret = ReadChunk( &infoCur );
 if(ret == true) {
   curFrame++;
   if(CompareInfoDiff(&infoPrev, &infoCur)){
     printf("Format change detected: frame %d -> %d\n", curFrame-1, curFrame);
   }
 }
 else
   printf("Error loading next chunk\n");

 return ret;
}


/******************************************************************************/
bool CDXL::PreviousFrame()
{
 if(infoCur.csizePrev <= 0  ||  curFrame == 0) { //only the case for the first frame
   printf("Previous frame cannot be located");
   return false;
 }

 //try to seek the previous chunk
 streamsize pos = stream.tellg();
 int reloffs = infoCur.csizeCur + infoCur.csizePrev;  //jump to start of previous chunk
 if(pos < reloffs)  //if illeagal offset
   return false;

 stream.seekg(reloffs, ios_base::cur);
 curFrame--;

 infoPrev = infoCur;  //copy old info struct
 bool ret = ReadChunk( &infoCur );
 if(CompareInfoDiff(&infoPrev, &infoCur)) {
   printf("Format change detected: frame %d -> %d\n", curFrame+1, curFrame);
 }

 return ret;
}




/******************************************************************************/
char* CDXL::GetInfoString()
{
 return infostring;
}

/******************************************************************************/
char* CDXL::GetChunkInfo(cdxl_chunkinfo *info)
{
 static char infostring[CDXL_INFOSTRING_BUFFER];
 
  //string lists
 static const char* ltype[3] = {"Custom", "Standart", "Special"};
 static const char* lvenc[4] = {"RGB", "HAM", "YUV", "AVM & DCTV"};
 static const char* lpori[5] = {"Bit Planar", "Byte Planar", "Chunky", "Bit Line", "Byte Line"};
 static const char* lncha[2] = {"Mono", "Stereo"};

 //build info string
 sprintf(infostring,
    "Number of frames: %d (estimated)\n"
    "CDXL type: %d (%s)\n"
    "Video encoding: %d (%s)\n"
    "Pixel orientation: %d (%s)\n"
    "Bitmap width: %d\n"
    "Bitmap height: %d\n"
    "Number of bitplanes: %d\n"
    "Audio channels: %d (%s)\n"
    "Bits per sample: 8\n"         //fix
    "Current chunksize: %d\n"
    "Previous chunksize: %d\n"
    "Current frame number: %d\n"
    "Colormap size: %d\n"
    "Raw audio size: %d\n"
    "Bitmap size: %d\n",
    nframes,
    (int)info->type, ltype[info->type],
    (int)info->ven, lvenc[info->ven],
    (int)info->por, lpori[info->por],
    info->w,
    info->h,
    info->bitplanes,
    (int)info->nch, lncha[info->nch - 1],
    info->csizeCur,
    info->csizePrev, 
    info->fnumber, 
    info->cmapsize,
    info->audiosize,
    info->bitmapsize);

 return infostring;
}


