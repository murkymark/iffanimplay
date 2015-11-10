#include "iffanim.h"
#include "../amiga_conv.h"
#include "../safemem.hpp"

#include <stdlib.h>


//ANHD bitmasks
#define ANHD_BITS_INTERLACE  ((uint32_t)0x40)





/******************************************************************************/
IffAnim::IffAnim()
{
 InitAttributes();
}


/******************************************************************************/
IffAnim::~IffAnim()
{
 Close();
}


/******************************************************************************/
//free buffers
void IffAnim::Close()
{
 if(!(file_loaded))
   return;
    
 if(ansq != NULL) {
   delete[] ansq;
 }

 //delete curframe and prevframe
 if(curframe != NULL)
   delete[] (char*)curframe;
 if(prevframe != NULL)
   delete[] (char*)prevframe;

 //delete every frame in the list
 for(int i = 0; i < nframes; i++)
 {
   if(frame[i].data != NULL)
     delete[] (char*)frame[i].data;
   if(frame[i].cmap != NULL)
     delete[] (char*)frame[i].cmap;
 }
 //delete frame list
 if(frame != NULL)
   delete[] frame;

 //delete audio data and lists
 if(audio.dataoffset != NULL)
   delete[] audio.dataoffset;
 if(audio.data != NULL) {
   delete[] audio.data;
 }

 InitAttributes();    //reset (init) buffer related attributes
}

/******************************************************************************/
void IffAnim::ClearError()
{
 errorstring = "";
}

/******************************************************************************/
//open and read anim file to memory, init to first frame
int IffAnim::Open(const char* fname)
{
 fstream file;

 //make sure no file is opened and buffers are deleted
 Close();

 //set default attributes
 SetLoopAnim(false);
 SetLoop(false);

 interlace_detected = false;

 //open file
 file.open(fname, ios::in | ios::binary);
 if(file.is_open() == false) {        
   errorstring.append("Can't open file\n");
   return -1;
 }

 //count number of frames, verify file structure
 nframes = GetNumFrames(&file);
 if(nframes <= 0) {
   errorstring.append("No frame found\n");
   return -1;
 }

 //load frames to memory (fills frame list), set format specs
 if(ReadFrames(&file) == -1) {
   errorstring.append("Couldn't load animation\n");
   Close();
   return -1;
 }

 //allocate buffer for delta decoded frame (a scanline is a multiple of 16 bit in ILBM BODY data)
 int pitch_16 = (w + 1) / 2 * 2;
 curframe =  new char[pitch_16 * bpp * h];
 prevframe = new char[pitch_16 * bpp * h]; 
 
 if((curframe == NULL) || (prevframe == NULL))
   return -1;


 //determine bits per pixel for converted output format
 if(bpp <= 8  &&  ham == false)
   disp_bpp = 8;
 else
   disp_bpp = 24;

 PrintInfo();  //create info string

 file_loaded = true;

 Reset();  //init to first frame
 return 0;
}


/******************************************************************************/
bool IffAnim::Loaded()
{
 return file_loaded;
}


/******************************************************************************/
string IffAnim::GetError()
{
 return errorstring;
}


/******************************************************************************/
//init member variables before anim file is loaded, a file must not be loaded at this moment
//called by Close()
void IffAnim::InitAttributes()
{
 file_loaded = false;
 ilbm = false;
 sprintf(formatinfo,"");   //clears information text string
 ClearError();
 curframe = NULL;
 curcmap = NULL;
 prevframe = NULL;
 prevcmap = NULL;
 frame = NULL;
 
 audio.data = NULL;
 audio.dataoffset = NULL;
 audio.datasize = 0;
 audio.freq = 0;
 audio.nch = 0;

 ansq = NULL;
 ansq_size = 0;

 memset(disp_cmap, 0, 256*3);  //set all color components to 0
}




/*
//method 3 (from XAnim)
xaULONG
IFF_Delta_3(image,delta,dsize,dec_info)
xaUBYTE *image;         //* Image Buffer.
xaUBYTE *delta;         //* delta data.
xaULONG dsize;          //* delta size
XA_DEC_INFO *dec_info;  //* Decoder Info Header 
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG imaged = dec_info->imaged;
 register xaLONG i,depth,dmask;
 xaULONG poff;
 register xaSHORT  offset;
 register xaUSHORT s,data;
 register xaUBYTE  *i_ptr,*dptr;

 dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
 dmask = 1;
 for(depth=0;depth<imaged;depth++)
 {
  i_ptr = image;

  //*poff = planeoff[depth]; // offset into delt chunk 

  poff  = (xaULONG)(delta[ 4 * depth    ]) << 24;
  poff |= (xaULONG)(delta[ 4 * depth + 1]) << 16;
  poff |= (xaULONG)(delta[ 4 * depth + 2]) <<  8;
  poff |= (xaULONG)(delta[ 4 * depth + 3]);

  if (poff)
  {
   dptr = (xaUBYTE *)(delta + poff);
   while( (dptr[0] != 0xff) || (dptr[1] != 0xff) )
   {
     offset = (*dptr++)<<8; offset |= (*dptr++);
     if (offset >= 0)
     {
      data = (*dptr++)<<8; data |= (*dptr++);
      i_ptr += 16 * (xaULONG)(offset);
      IFF_Short_Mod(i_ptr,data,dmask,0);
     } // end of pos 
     else
     {
      i_ptr += 16 * (xaULONG)(-(offset+2));
      s = (*dptr++)<<8; s |= (*dptr++); // size of next 
      for(i=0; i < (xaULONG)s; i++)
      {
       data = (*dptr++)<<8; data |= (*dptr++);
       i_ptr += 16;
       IFF_Short_Mod(i_ptr,data,dmask,0);
      }
    }  // end of neg 
   } // end of delta for this plane 
  } // plane has changed data 
  dmask <<= 1;
 } // end of d 
 return(ACT_DLTA_NORM); 
}
*/



// "Decode IFF type l anims" (method 108)
// code ported from XAnim source
// I've never seen a file in that format, also I did not find any document mentioning it, just mentioned here:
//   "http://lclevy.free.fr/amiga/formats.html" -> "ANIM-l (Eric Graham's compression)"
//   Can it be the predecessor of ANIM-J and there is an error so it should be actually "ANIM-I"???
/*
xaULONG IFF_Delta_l(image,delta,dsize,dec_info)
xaUBYTE *image;         // Image Buffer.
xaUBYTE *delta;         // delta data.
xaULONG dsize;          // delta size 
XA_DEC_INFO *dec_info;  // Decoder Info Header 
{ xaULONG imagex = dec_info->imagex;    xaULONG imagey = dec_info->imagey;
  xaULONG imaged = dec_info->imaged;
  xaULONG vertflag = (xaULONG)(dec_info->extra);
 register xaLONG i,depth,dmask,width;
 xaULONG poff0,poff1;
 register xaUBYTE *i_ptr;
 register xaUBYTE *optr,*dptr;
 register xaSHORT cnt;
 register xaUSHORT offset,data;

 dec_info->xs = dec_info->ys = 0; dec_info->xe = imagex; dec_info->ye = imagey;
 i_ptr = image;
 if (vertflag) width = imagex;
 else width = 16;
 dmask = 1;
 for(depth = 0; depth<imaged; depth++)
 {
   i_ptr = image;
   //poff = planeoff[depth]; // offset into delt chunk
   poff0  = (xaULONG)(delta[ 4 * depth    ]) << 24;
   poff0 |= (xaULONG)(delta[ 4 * depth + 1]) << 16;
   poff0 |= (xaULONG)(delta[ 4 * depth + 2]) <<  8;
   poff0 |= (xaULONG)(delta[ 4 * depth + 3]);

   if (poff0)
   {
     poff1  = (xaULONG)(delta[ 4 * (depth+8)    ]) << 24;
     poff1 |= (xaULONG)(delta[ 4 * (depth+8) + 1]) << 16;
     poff1 |= (xaULONG)(delta[ 4 * (depth+8) + 2]) <<  8;
     poff1 |= (xaULONG)(delta[ 4 * (depth+8) + 3]);

     dptr = (xaUBYTE *)(delta + 2 * poff0); 
     optr = (xaUBYTE *)(delta + 2 * poff1); 

     // while short *optr != -1
     while( (optr[0] != 0xff) || (optr[1] != 0xff) )
     {
       offset = (*optr++) << 8; offset |= (*optr++);
       cnt    = (*optr++) << 8; cnt    |= (*optr++);
 
       if (cnt < 0)  // cnt negative
       {
         i_ptr = image + 16 * (xaULONG)(offset);
         cnt = -cnt;
         data = (*dptr++) << 8; data |= (*dptr++);
         for(i=0; i < (xaULONG)cnt; i++)
         {
           IFF_Short_Mod(i_ptr,data,dmask,0);
           i_ptr += width;
         }
       }  // end of neg
       else // cnt pos then
       {
         i_ptr = image + 16 * (xaULONG)(offset);
         for(i=0; i < (xaULONG)cnt; i++)
         {
           data = (*dptr++) << 8; data |= (*dptr++);
           IFF_Short_Mod(i_ptr,data,dmask,0);
           i_ptr += width;
         }
       } // end of pos
     } // end of delta for this plane
   } // plane has changed data
   dmask <<= 1;
 } // end of d
 return(ACT_DLTA_NORM);
}
*/



/******************************************************************************/
//decode RLE ("byterun" aka "packer", aka "PackBits" on Macintosh) compressed line
//normally only the first frame is packed with it (except delta method 0 or 1)
//mask plane is ignored (if available)
int IffAnim::DecodeByteRun(void* dst_, void* data_, int datasize, int w, int h, int bpp, int mask)
{
 int i,j;

 char* dst = (char*)dst_;   //destination (uncompressed)
 char* src = (char*)data_;  //byte code (compressed)
 
 int planepitch;     //pitch of a bitplane
 int linepitch;      //pitch of a scanline with mask plane

 int n,val;
 int posdst;         //write position in dst
 int possrc;         //read position in src
 
 
 planepitch = (w + 15) / 16 * 2;   //pitch of a plane
 linepitch = planepitch * bpp;     //pitch of a line without mask
 if(mask == 1)
   linepitch += planepitch;        //add mask plane pitch to line pitch
 
 if((dst == NULL) || (src == NULL))
   return -1;

 possrc = 0;   //position in src buffer

 //for each line decode to dst buffer
 for(i = 0; i < h; i++)
 {
   posdst = 0;

   j = planepitch * bpp;  //number of bytes for the scanline to decode (without mask)

   //while scanline data is not decoded
   while(j > 0)
   {
     n = src[possrc++];   //get type
     if(n >= 0) {         //copy number of bytes
       n = n + 1;
       if(n > j)          //overflow protection
         memcpy(dst + posdst, src + possrc, j);
       else
         memcpy(dst + posdst, src + possrc, n);
       possrc += n;
     }
     else if(n != -128) { //multiple times the same byte value
       n = -n + 1;
       val = src[possrc++];
       if(n > j)         //overflow protection
         memset(dst + posdst, val, j);
       else
         memset(dst + posdst, val, n);
     }
     posdst += n;
     j -= n;
   }
   dst += planepitch * bpp;                 //set pointer to beginning of next line
 }

 return 0;
}


/******************************************************************************/
// Decode Byte Vertical Delta compression: compression 5
int IffAnim::DecodeByteVerticalDelta(char* dst, void* data_, int w, int bpp)
{
 unsigned char* data = (unsigned char*)data_;
 int i,j,k;       //loop counter
 int ofsdst;      //offset in destination buffer
 int ofssrc;      //offset in compressed data (delta chunk)
 int op, val;     //holds opcode , data value

 //width of a plane within a line in bytes (number of columns in a plane)
 int ncolumns = ((w + 15) / 16) * 2;
 
 //total len of a line in destination buffer, in bytes
 int dstpitch = ncolumns * bpp;


 //for every plane
 for(k = 0; k < bpp; k++)
 {
    //get offset (pointer) to compressed opcodes and data of current plane
    ofssrc = k * 4;
    ofssrc = (data[ofssrc] << 24) | (data[ofssrc + 1] << 16) | (data[ofssrc + 2] << 8) | data[ofssrc + 3];
    
    if(ofssrc)   //no change in plane if pointer index is 0
    {
       //for each column of a plane (column: a byte from every line -> vertically)
       for(j = 0; j < ncolumns; j++) 
       {
          ofsdst = j + k * ncolumns;   //set dst offset for current column, a column starts in the first scanline
           
          //get number of ops for the column and interpret
          for(i = data[ofssrc++]; i > 0; i--)
          {           
             op = data[ofssrc++];      //get opcode

             //if SAME_OP, opcode 0
             if(op == 0) {
                op =  data[ofssrc++];  //number of same bytes
                val = data[ofssrc++];
                while(op) {
                   dst[ofsdst] = val;
                   ofsdst += dstpitch;
                   op--;
                }
             }
             //if SKIP_OP, high bit is 0
             else if(op < 0x80)
                ofsdst += op * dstpitch;

             //if UNIQ_OP, high bit is set
             else {
                op &= 0x7f;  //set high bit to 0
                while(op) {
                   dst[ofsdst] = data[ofssrc++];
                   ofsdst += dstpitch;
                   op--;
                }
             }  
             
          } //end for all ops
       }  //end for all columns
    }
 } // end for all planes
 
 return 0;
}




/******************************************************************************/
// Decode General Delta compression: compression 4   (probably not working, untested!!!)
int IffAnim::DecodeGeneralDelta(char* dst, void* data_, int w, int bpp){

//short, vertical, set version
// others are probably set by flags in ANHD

//flags needed  short/long, set/xor, horizontal/vertical

//	struct BitMap *bm;
	int16_t  *deltaword = (int16_t*)data_;

	int i;
	uint32_t *deltadata;
	int16_t *ptr,*planeptr;
	int s, size, nw;
	int16_t *data,*dest;

	int dstplanepitch = ((w + 15) / 16) * 2;
	int dstpitch = dstplanepitch * bpp;

	deltadata = (uint32_t*)deltaword;
	nw = dstpitch / 2;

	//for each plane
	for (i=0; i < bpp; i++) {
	
	
			planeptr = (int16_t*)((int8_t*)dst + (dstplanepitch * i)); //dst
			data = deltaword + deltadata[i];
			ptr  = deltaword + deltadata[i+8];
			while (*ptr != 0xFFFF) {
				dest = planeptr + *ptr++;
				size = *ptr++;
				if (size < 0) {
						for (s = size; s < 0; s++) {
							*dest = *data;
							dest += nw;
						}
						data++;
				 }
				 else {
						for (s = 0; s < size; s++) {
							*dest = *data++;
							dest += nw;
						}
				 }
			}
	}
	return 0;

}




#define DEBUG_ON 0
#if DEBUG_ON != 0
 #define DEBUG_MSG(x) x
#else
 #define DEBUG_MSG(x)
#endif



/******************************************************************************/
//decode Scala compression 100 (ANIM32) and 101 (ANIM16), interlace possible
//todo: must the last line be handled specifically, how does it affect the dst offset from the DLTA data?
//      since there is no indication if offset points to last column, it seem the "virtual" offsets assume 32 bit padded buffer
//      => check with video sample
//interlace : if true DLTA is only half frame
//odd : odd frame number if true, else even frame number -> related to interlace
int IffAnim::DecodeScalaAnim(char* dst, void* data_, int datasize, int w, int h, int bpp, bool long_data, bool interlace, bool odd){

	unsigned char* data = (unsigned char*)data_;  //source buffer
	int i, k;

	int32_t p_da;      //offset to data in source

	int ofsdst;        //offset in destination buffer in bytes
	int opcnt;
	int op;            //holds opcode

	int ncolumns;      //number of columns per bitplane (total number of columns for "long" data), each with size "wordsize"
	int wordsize;      //bytes for one data word: 32 (long) or 16 bit (short)
	int dstpitch;      //length of a scanline containing all bitplanes in destination buffer, in bytes (padded to multiple of 16 bit always)
	int dstplanepitch; //length of a scanline of a single bitplane in destination buffer in bytes (padded to multiple of 16 bit always)
	bool care_boundary = false; //last column may have only 16 instead of 32 bits (only relevant for long data)


	dstplanepitch = ((w + 15) / 16) * 2;

	if(long_data) {
		wordsize = 4;
		ncolumns = (w + 31) / 32; //rounded up
		if(dstplanepitch != ncolumns * wordsize)   //relating to the width of a video frame, possibly for the last column of each plane one must copy only 16 bit words to ensure 16 bit padding
			care_boundary = true;
	}
	else {
		wordsize = 2;
		ncolumns = (w + 15) / 16; //rounded up
	}


	//size of a pixel line containing all planes, 
	dstpitch = dstplanepitch * bpp;

	//for safe memory access
	SafeMemory dlta(data, datasize);
	SafeMemory fbuf(dst, dstpitch * h);

	//todo add interlace support
	//...
	//printf("32bit columns per plane: %d\n", dstpitch/wordsize/bpp);
	//printf("last column only 16 bit: %d\n", care_boundary);
	//printf("framesize: %d\n", dstpitch * h);

	//for each plane
	for(i = 0; i < bpp; i++)
	{
		//get offset to bitplane date
		//offset must be "> 4*bpp", otherwise invalid
		p_da = dlta.readUInt32BE(i * 4);  // = data[i*4] << 24 | data[i*4+1] << 16 | data[i*4+2] << 8 | data[i*4+3]
		
		//if offset is -1 => no further bitplane to decode
		if(p_da == -1)
			break;
		//if offset 0 => no change in bitplane, skip
		if(p_da == 0)
			continue;
		
		//get number of ops for current bitplane (32bit)
		int offs = p_da;
		if(long_data)
			opcnt = dlta.readUInt32BE(offs);
		else
			opcnt = dlta.readUInt16BE(offs);
		
		//offset in DLTA data where the first op starts
		offs += wordsize;

		
DEBUG_MSG(printf("ptr data: %d\n",p_da);)
DEBUG_MSG(printf("opcnt: %d\n",opcnt);)
		
		
		
		//apply operations to destination frame buffer
		while(opcnt)
		{
DEBUG_MSG(printf("-----\n");)
			//set dst byte start offset for current column, a column starts in the first scanline
//     ofsdst = (j + i * ncolumns) * wordsize;
				
				
			//get opcode (data unit count), can be negative
			if(long_data)
				op = dlta.readInt32BE(offs);
			else
				op = dlta.readInt16BE(offs);

//cout << "op "<< op << endl;
				
			offs += wordsize;
			
			int dstoffs = dlta.readUInt32BE(offs);
			offs += 4;

			//which line
			int y = dstoffs / (ncolumns * wordsize);
			//byte in current bitplane line
			int x = dstoffs % (ncolumns * wordsize); 
			//which column of bitplane
			int c = x / wordsize; 
			//since the offset is in bytes, it must be always dividable by wordsize without rest,
			// otherwise the written word would overstep the column particially -> indicates data corruption
			if(x % wordsize  !=  0)
				printf("Invalid offset: %d\n", dstoffs);
			
			//if(interlace && !odd)
			//	y += 1;
			
			//calc offset in interleaved bitplane frame buffer
			//<line offset> + <previous bitplanes of line> + <column offset>
			dstoffs = (y * dstpitch) + (dstpitch / bpp * i) + (c * wordsize);

//cout << dstoffs << " " << op << endl;

			//the following only writes to a single row
		
			//if positive, write single data unit to n supsequent lines starting at <dstoffs>
			if(op > 0) {

				//because of 16 bit padded bitplanes we can only write 16 bits in the last column if "0 < (width % 32) <= 16"
				if(long_data && !(care_boundary  &&  c == (ncolumns - 1))){ //32 bit data units
					uint32_t d; //data buffer
					dlta.copyFrom32(&d, offs);

					for(; op > 0; op--){
						fbuf.copyTo32(dstoffs, &d);  //*((int32_t*)(dst + dstoffs)) = *((int32_t*)(data + offs))
						dstoffs += dstpitch;
						if(interlace)
							dstoffs += dstpitch;
					}
				}
				else { //16 bit data units
					uint16_t d;
					dlta.copyFrom16(&d, offs);

					for(; op > 0; op--){
						fbuf.copyTo16(dstoffs, &d);
						dstoffs += dstpitch;
						if(interlace)
							dstoffs += dstpitch;
					}
				}
				offs += wordsize; //increase one unit
			}
			//if negative => copy number of units
			else if(op < 0){
				op = abs(op);

				if(long_data && !(care_boundary  &&  c == (ncolumns - 1))) {
					for(k = 0; k < op; k++){
						dlta.copyFrom32(fbuf, dstoffs, offs);
						offs += wordsize;
						dstoffs += dstpitch; //offset to place next unit to (next line in dst)
						if(interlace)
							dstoffs += dstpitch;
					}
				}
				else {
					for(k = 0; k < op; k++){
						dlta.copyFrom16(fbuf, dstoffs, offs);
						offs += wordsize;
						dstoffs += dstpitch;
						if(interlace)
							dstoffs += dstpitch;
					}
				}
				
				
			
			}
			//op == 0 shall not appear, undefined operation -> corrupt data
			else if(op == 0) {
				printf("%s: Operation %d is illegal -> corrupt DLTA data!\n", __FUNCTION__, op);
				//stop operations of current bitplane here and continue with the next bitplane
				break; //leave while(opcnt)
			}
		
		opcnt--;
		} //end while(opcnt)
	} //end for-each-bitplane
	
	
	//TODO: for interlace half frame do line doubling (bobbing)
	
	return 0;
}





/******************************************************************************/
//decode delta 7 long or short
int IffAnim::DecodeLSVerticalDelta7(char* dst, void* data_, int w, int bpp, bool long_data)
{
 unsigned char* data = (unsigned char*)data_;  //source buffer
 int i,j;

 uint32_t p_da;     //offset to data in source
 uint32_t p_op;     //offset to opcode in source buffer
 
 int ofsdst;        //offset in destination buffer in bytes
 int op;            //holds opcode
 int opcnt;         //op counter
 int16_t val16;     //holds 16 bit data
 int32_t val32;     //holds 32 bit data
 
 int t;             //help variable

 int ncolumns;      //number of columns per bitplane (total number of columns for "long" data), each with size "wordsize"
 int wordsize;      //bytes for one data word: 32 (long) or 16 bit (short)
 int dstpitch;      //length of a scanline in destination buffer in bytes
 bool care_boundary = false; //last column maybe only with 16 instead of 32 bits


 if(long_data) {
   wordsize = 4;              
   ncolumns = (w + 31) / 32;
   if(((w + 15) / 16 * 2) != ((w + 31) / 32 * 4))   //relating to the width of a video frame, possibly for the last column of each plane one must copy only 16 bit words to ensure 16 bit padding
     care_boundary = true;
 }
 else {
   wordsize = 2;
   ncolumns = (w + 15) / 16;
 }

 dstpitch = ((w + 15) / 16 * 2) * bpp;


 //for every plane
 for(i = 0; i < bpp; i++)
 {

   //get 32 bit offsets (pointers) to data and opcodes for the current plane, stored as Big Endian
   t = i * 4;
   p_op = (data[t] << 24) | (data[t + 1] << 16) | (data[t + 2] << 8) | data[t + 3];
   p_da = (data[t + 32] << 24) | (data[t + 33] << 16) | (data[t + 34] << 8) | data[t + 35];

   if(p_op)  //if opcode pointer index not 0 => plane is modified
   {
                
     //for each column
     for(j = 0; j < ncolumns; j++)
     {
        //set dst byte start offset for current column, a column starts in the first scanline
        ofsdst = (j + i * ncolumns) * wordsize;

        //correct if last column has only 16 bit
        if(care_boundary) ofsdst -= (2 * i);

        //get number of ops for the column
        opcnt = data[p_op++];

        //interpret all ops of the column
        while(opcnt)
        {
          op = data[p_op++];   //fetch opcode

          if((wordsize == 2) || (care_boundary && ((j + 1) == ncolumns)))    //2 bytes per data word, or 16 bit of last column with 32 bit byte wordsize
          {
             //SAME_OP, opcode is 0
             if(op == 0) {
               op = data[p_op++];   //number of same words
               val16 = *((int16_t*)(data + p_da));  //get data word
               p_da += wordsize;
               while(op) {
                 *((int16_t*)(dst + ofsdst)) = val16;
                 ofsdst += dstpitch;
                 op--;
               }
             }

             //SKIP_OP, high bit is not set
             else if(op < 128)
               ofsdst += dstpitch * op;


             //UNIQ_OP, high bit is set 
             else {
               op &= 0x7f;  //mask out high bit and use as counter
               while(op) {
                  *((int16_t*)(dst + ofsdst)) = *((int16_t*)(data + p_da));
                  p_da += wordsize;
                  ofsdst += dstpitch;
                  op--;
               }
             }
            
          }
          else    //4 bytes per data word
          {
             //SAME_OP, opcode is 0
             if(op == 0) {
               op = data[p_op++];   //number of same words
               val32 = *((int32_t*)(data + p_da));  //get data word
               p_da += 4;
               while(op) {
                 *((int32_t*)(dst + ofsdst)) = val32;
                 ofsdst += dstpitch;
                 op--;
               }
             }

             //SKIP_OP, high bit is not set
             else if(op < 128)
               ofsdst += dstpitch * op;

             //UNIQ_OP, high bit is set 
             else {
               op &= 0x7f;
               while(op) {
                  *((int32_t*)(dst + ofsdst)) = *((int32_t*)(data + p_da));
                  p_da += 4;
                  ofsdst += dstpitch;
                  op--;
               }
             }
 
          } //end else

          opcnt--;
        } //end for number of ops
     }
   }
    
 }

 return 0;
}






/******************************************************************************/
// decompress delta mode 8 long or short
int IffAnim::DecodeLSVerticalDelta8(char* dst, void* data_, int w, int bpp, bool long_data)
{
 unsigned char* data = (unsigned char*)data_;  //source buffer
 int i,j;

 uint32_t p_op;       //offset to opcode in source buffer
 
 int ofsdst;          //offset in destination buffer in bytes
 uint32_t op;         //holds opcode
 unsigned int opcnt;  //op counter
 int16_t val16;       //holds 16 bit data
 int32_t val32;       //holds 32 bit data
 
 int t;               //help variable

 int ncolumns;        //number of columns per bitplane (total number of columns for "long" data), each with size "wordsize"
 int wordsize;        //bytes for one data word: 32 (long) or 16 bit (short)
 int dstpitch;        //length of a scanline in destination buffer in bytes
 bool care_boundary = false;   //last column maybe only with 16 instead of 32 bits


 if(long_data) {
   wordsize = 4;              
   ncolumns = (w + 31) / 32;
   if(((w + 15) / 16 * 2) != ((w + 31) / 32 * 4))   //relating to the width of a video frame, possibly for the last column of each plane one must copy only 16 bit words, 
     care_boundary = true;
 }
 else {
   wordsize = 2;
   ncolumns = (w + 15) / 16;
 }

 dstpitch = ((w + 15) / 16 * 2) * bpp;


 //for every plane
 for(i = 0; i < bpp; i++)
 {

   //get 32 bit offset (pointer) to opcodes for the current plane, stored as Big Endian
   t = i * 4;
   p_op = (data[t] << 24) | (data[t + 1] << 16) | (data[t + 2] << 8) | data[t + 3];

   if(p_op)  //if opcode pointer index not 0 => plane is modified
   {
                
     //for each column
     for(j = 0; j < ncolumns; j++)
     {
        //set dst byte start offset for current column, a column starts in the first scanline
        ofsdst = (j + i * ncolumns) * wordsize;


        if(wordsize == 2)
          opcnt = (data[p_op] << 8) | data[p_op + 1]; //get number of ops for the column
        else {
          if(care_boundary) ofsdst -= (2 * i);                 //correct if last column has only 16 bit
          opcnt = (data[p_op] << 24) | (data[p_op + 1] << 16) | (data[p_op + 2] << 8) | data[p_op + 3];
        }

        p_op += wordsize;

        //interpret all ops of the column
        while(opcnt)
        {

          //fetch opcode
          if(wordsize == 2)
            op = (data[p_op] << 8) | data[p_op + 1];   
          else
            op = (data[p_op] << 24) | (data[p_op + 1] << 16) | (data[p_op + 2] << 8) | data[p_op + 3];
            
          p_op += wordsize;

          if((wordsize == 2) || (care_boundary && ((j + 1) == ncolumns)))    //2 bytes per data word, or 16 bit of last column with 32 bit byte wordsize
          {
             //SAME_OP, opcode is 0
             if(op == 0) {
               op = (data[p_op] << 8) | data[p_op + 1]; //number of same words
               p_op += 2;
               val16 = *((int16_t*)(data + p_op));      //get data word
               p_op += 2;
               while(op) {
                 *((int16_t*)(dst + ofsdst)) = val16;
                 ofsdst += dstpitch;
                 op--;
               }
             }

             //SKIP_OP, high bit is not set
             else if(op < 0x8000)
               ofsdst += dstpitch * op;


             //UNIQ_OP, high bit is set 
             else {
               op &= 0x7fff;   //mask out high bit and use as counter
               while(op) {
                  *((int16_t*)(dst + ofsdst)) = *((int16_t*)(data + p_op));
                  p_op += 2;
                  ofsdst += dstpitch;
                  op--;
               }
             }
            
          }
          else    //4 bytes per data word
          {
             //SAME_OP, opcode is 0
             if(op == 0) {
               op = (data[p_op] << 24) | (data[p_op + 1] << 16) | (data[p_op + 2] << 8) | data[p_op + 3];   //number of same words
               p_op += 4;
               val32 = *((int32_t*)(data + p_op));  //get data word
               p_op += 4;
               while(op) {
                 *((int32_t*)(dst + ofsdst)) = val32;
                 ofsdst += dstpitch;
                 op--;
               }
             }

             //SKIP_OP, high bit is not set
             else if(op < 0x80000000)
               ofsdst += dstpitch * op;

             //UNIQ_OP, high bit is set 
             else {
               op &= 0x7fffffff;
               while(op) {
                  *((int32_t*)(dst + ofsdst)) = *((int32_t*)(data + p_op));
                  p_op += 4;
                  ofsdst += dstpitch;
                  op--;
               }
             }
 
          } //end else

          opcnt--;
        } //end for number of ops
     }
   }
    
 }
 return 0;
}






/******************************************************************************/
/*
The following function is an interpretation of the
"Delta_J" code from " "xanim2801.tar.gz" (XAnim Revision 2.80.1).
It is modified to draw to a bitplanar frame buffer (BODY data format) instead of
a chunky one, thus it fits better into a decoding pipeline.

 char *dst,     //Image Buffer pointer (old frame data, BODY format)
 void *delta_,  //delta data
 int  w,        //width in pixels
 int  h,        //height in pixels
 int  bpp       //bits per pixel (depth, number of bitplanes)
*/
int IffAnim::DecodeDeltaJ(char* dst, void* delta_, int w, int h, int bpp)
{
 unsigned char* image = (unsigned char*)dst;
 unsigned char* delta = (unsigned char*)delta_;

 int32_t   pitch;     //scanline width in bytes
 uint8_t*  i_ptr;     //used as destination pointer into the frame buffer
 uint32_t  type, r_flag, b_cnt, g_cnt, r_cnt; 
 int       b, g, r, d;    //loop counters
 uint32_t  offset;    //byte offset

 int planepitch_byte = (w + 7) / 8;      //plane pitch as multiple of 8 bits, needed to calc the right offset
 int planepitch = ((w + 15) / 16) * 2;   //width of a line of a single bitplane in bytes (multiple of 16 bit)
 pitch = planepitch * bpp;               //size of a scanline in bytes (bitplanar BODY buffer)

 //for pixel width < 320 we need the horizontal byte offset in a bitplane on a 320 pixel wide screen
 int kludge_j;
 if (w < 320)
   kludge_j = (320 - w) / 8 / 2;  //byte offset
 else 
   kludge_j = 0;



 //loop until block type 0 appears (or any unsupported type with unknown byte structure)
 int exitflag = 0;
 while(!exitflag)
 {
   //read compression type and reversible_flag ("reversible" means XOR operation) 
   type   = ((delta[0]) << 8) | (delta[1]);
   delta += 2;

   //switch on compression type
   switch(type)
   {
     case 0: exitflag = 1; break;  // end of list, delta frame complete -> leave
     case 1:
       //read reversible_flag
       r_flag = (*delta++) << 8; r_flag |= (*delta++);

       // Get byte count and group count 
       b_cnt = (*delta++) << 8; b_cnt |= (*delta++);
       g_cnt = (*delta++) << 8; g_cnt |= (*delta++);

       // Loop through groups
       for(g = 0; g < g_cnt; g++)
       {
         offset = (*delta++) << 8; offset |= (*delta++);

         //get real byte offset in IFF BODY data
         if (kludge_j)
           offset = ((offset/(320 / 8)) * pitch) + (offset % (320/ 8)) - kludge_j;
         else
           offset = ((offset/planepitch_byte) * pitch) + (offset % planepitch_byte);
 
         i_ptr = image + offset;  //BODY data pointer

         //read and apply "byte count" * "bpp" bytes (+ optional pad byte for even number of bytes)
         //1 byte for each plane -> modifies up to 8 bits
         // byte count represents number of rows

         // Loop thru byte count
         for(b = 0; b < b_cnt; b++)  //number of vertical steps
         {
           for(d = 0; d < bpp; d++)  //loop thru planes, a delta byte for each plane
           {
             if (r_flag) *i_ptr ^= *delta++;
             else        *i_ptr  = *delta++;

             i_ptr += planepitch;    //go to next plane
           } // end of depth loop 

         } // end of byte loop
         if((b_cnt * bpp) & 0x1) delta++;  //read pad byte (group contains even number of bytes)

       } //end of group loop 
       break;

     case 2:
       //read reversible_flag
       r_flag = (*delta++) << 8; r_flag |= (*delta++);

       // Read row count, byte count and group count
       r_cnt = (*delta++) << 8; r_cnt |= (*delta++);
       b_cnt = (*delta++) << 8; b_cnt |= (*delta++);
       g_cnt = (*delta++) << 8; g_cnt |= (*delta++);
 
       // Loop through groups
       for(g = 0; g < g_cnt; g++)
       {
         offset  = (*delta++) << 8; offset |= (*delta++);

         //get real byte offset in IFF BODY data
         if (kludge_j)
           offset = ((offset/(320 / 8)) * pitch) + (offset % (320/ 8)) - kludge_j;
         else
           offset = ((offset/planepitch_byte) * pitch) + (offset % planepitch_byte);


         // Loop through rows
         for(r = 0; r < r_cnt; r++)
         {
           for(d = 0; d < bpp; d++) // loop thru planes
           {
             i_ptr = image + offset + (r * pitch) + d * planepitch;
             
             for(b = 0; b < b_cnt; b++) // loop through byte count
             {
               if (r_flag) *i_ptr ^= *delta++;
               else        *i_ptr  = *delta++;           
               i_ptr++;      // data is horizontal
             } // end of byte loop
           } // end of depth loop 
         } // end of row loop
         if ((r_cnt * b_cnt * bpp) & 0x01) delta++; // pad to even number of bytes
       } // end of group loop
       break;

     default: //unknown type
       fprintf(stderr,"DeltaJ decoder: Unknown J-type %x\n", type);
       exitflag = 1;
       break;
   }  // end of type switch
 }  // end of while loop

 return 0;
} // end of DeltaJ routine







/******************************************************************************/
//return current (logical) frame
int IffAnim::CurrentFrameIndex()
{
 if(loopanim && (frameno >= (nframes - 2)))
   return frameno - (nframes - 2);
 else
   return frameno;
}


/******************************************************************************/
//find the chunk with the requested 4 byte ID of "idreq" within a range from the current file position
//searches only in one level, file pointer must point to a chunk id inside this level
// example: to search data of a parent chunk get chunksize (len to search) and set file pos to byte 0 of chunk data
//returns start position in file of requested chunk and positions the file pointer to it's id
//not requested chunks are skipped
//returns -1 if chunk not found within range
int IffAnim::FindChunk(fstream* file, const char* idreq, int len)
{
 char id[4];
 int  chunksize;
 int  pos;


 pos = file->tellg();
 len += pos;
  
 while(pos < len){
    file->read(id, 4);
    if(memcmp(id, idreq, 4) == 0) //break if found
       break;
    chunksize = (file->get() << 24) | (file->get() << 16) | (file->get() << 8) | file->get();
    //note: every chunk is padded to full 16 bit words => even number of bytes
    chunksize = ((chunksize + 1) >> 1) << 1;
    pos += chunksize + 8;
    file->seekg(chunksize, ios::cur);
 }
 if(pos >= len)
   return -1;
   
 file->seekg(pos, ios::beg);
 return pos;
}




/******************************************************************************/
//verify chunk structure
//count and return number of frames in the file
int IffAnim::GetNumFrames(fstream* file)
{
 char idbuf[4];
 int chunksize;
 int numframes;

 //init
 numframes = 0;
 file->seekg(0, ios::beg);

 //check for FORM ID
 file->read(idbuf, 4);
 if(memcmp(idbuf, "FORM", 4) != 0) {
   errorstring.append("FORM ID not found at beginning, no IFF file\n");
   return -1;
 }
 file->seekg(4, ios::cur);

 //check for FORM type ID: ANIM
 file->read(idbuf, 4);
 if( memcmp(idbuf, "ANIM", 4) != 0) {
   //check if it is an ILBM (single image)
   if( memcmp(idbuf, "ILBM", 4) != 0){
     this->ilbm = true;
     file->seekg(0, ios::beg); //reset file pos to point to "FORM....ILBM" -> will detect single frame
   }
   else {
     errorstring.append("IFF FORM type ID is not ANIM, no supported ANIM file\n");
     return -1;
   }
 }

 //count number of FORM...ILBM chunks (frames) within file
 do {
     //check for FORM ID
     file->read(idbuf, 4);
     if(memcmp(idbuf, "FORM", 4) != 0)
       break;
     chunksize = (file->get()<<24) + (file->get()<<16) + (file->get()<<8) + file->get();
     chunksize = ((chunksize + 1) >> 1) << 1;   //must be multiple of 2 bytes (round up)
     //check for FORM type ID: ILBM
     file->read(idbuf, 4);
     if(memcmp(idbuf, "ILBM", 4) != 0)
       break;
     //skip ILBM
     file->seekg(chunksize-4, ios::cur);
     //frame found -> increase "nframes"
     numframes++;
 }while (1);
 
 //clear possible errors (when reading beyond EOF)
 file->clear();

 if(numframes == 0)
   return -1;

 return numframes;
}



/******************************************************************************/
//read anim header chunk info into mem (frame entry of the frame list)
void IffAnim::read_ANHD(fstream* file, iffanim_frame* frame)
{
 file->seekg(8, ios::cur);   //file pointer points to first byte of chunk before, so we jump to the chunk content

 frame->delta_compression = file->get(); //"operation"
 frame->mask = file->get();
 frame->w = (file->get()<< 8) | file->get();
 frame->h = (file->get()<< 8) | file->get();
 frame->x = (file->get()<< 8) | file->get();
 frame->y = (file->get()<< 8) | file->get();

 file->seekg(4, ios::cur);
 frame->reltime = (file->get() << 24) | (file->get() << 16) | (file->get() << 8) | file->get();
 frame->interleave = file->get();

 file->seekg(1,ios::cur);
 frame->bits = (file->get() << 24) | (file->get() << 16) | (file->get() << 8) | file->get();
 
 //when the first interlace frame is detected
 if(!interlace_detected  &&  (frame->bits & ANHD_BITS_INTERLACE)) {
   interlace_detected = true;
 }
}



/******************************************************************************/
//read CMAP chunk into mem (frame entry)
void IffAnim::read_CMAP(fstream* file, iffanim_frame* frame)
{
 int j;
 int ncolors;
 int palsize;

 file->seekg(8, ios::cur); //skip ID and chunk size
 //allocate mem for cmap
 ncolors = 1 << bpp;
 palsize = ncolors * 3;   //RGB entries
 frame->cmap = new char[palsize];
 
 //read cmap, handle EHB mode (second half are darker versions of the previous colors)
 if(ehb) {
    palsize = palsize / 2;
    file->read(frame->cmap, palsize);
    for(j = 0; j < palsize; j++)
       frame->cmap[palsize + j] = frame->cmap[j] / 2;  // every color value divided by 2 (half brightness)
 }
 else
    file->read(frame->cmap, palsize);
}

/******************************************************************************/
/* - make audio interleaved -> reordering
  current sample point order:    0L,1L,2L,3L,... | 0R,1R,2R,3R,...
  wanted sample point order:  0L,0R,1L,1R,2L,2R,3L,3R,...

 - structure of bit depth other than 8 ist unknown, although 1..32 should be supported as the format spec. says
 -> well, let's support only 8 and 16 bit
*/
int IffAnim::InterleaveStereo(char* data, int datasize, int bps)
{
 int i;
 
 if(data == NULL)
   return -1;

 int nframes = datasize / 2 / ((bps + 7) / 8);  //number of sample frames in "data"

 char* newdata = new char[datasize];

 if(newdata == NULL)
   return -1;

 //reorder
 if(bps <= 8) //8 bit per point
 {
   int8_t* sl8 = (int8_t*)data;
   int8_t* sr8 = (int8_t*)(data + (datasize / 2));
   int8_t* dst8 = (int8_t*)newdata;
   for(i = 0; i < nframes; i++) {
     *dst8 = sl8[i];
     dst8[1] = sr8[i];
     dst8 += 2;
   }
 }
 else        //16 bit per point
 {
   int16_t* sl16  = (int16_t*)data;
   int16_t* sr16  = (int16_t*)(data + (datasize / 2));
   int16_t* dst16 = (int16_t*)newdata;
   for(i = 0; i < nframes; i++) {
     *dst16 = sl16[i];
     dst16[1] = sr16[i];
     dst16 += 2;
   }
 }

 //copy reordered points to old buffer
 memcpy(data, newdata, datasize);
 delete[] newdata;

 return 0;
}

/******************************************************************************/
//read SBDY chunk into mem (frame entry)
//stereo data is not interleaved in the file!
int IffAnim::read_SBDY(fstream* file, int searchlen, char** audiobuf, int* audiobufsize)
{
 if((audiobuf == NULL) || (audiobufsize == NULL))
   return -1;

 char* tptr;    //help pointer
 int   chunksize;
 int   startpos = file->tellg();

 //file pointer should point to first SBDY chunk
 file->seekg(4, ios::cur);
 chunksize = (file->get()<<24) + (file->get()<<16) + (file->get()<<8) + file->get();

 *audiobuf = new char[chunksize];
 if(*audiobuf == NULL) {
   errorstring.append("Can't allocate memory\n");
   return -1;
 }
 *audiobufsize = chunksize;
 file->read(*audiobuf, chunksize);    

 //interleave stereo channels
 if((audio.nch == 2) && (audio.bps != 0))
   InterleaveStereo(*audiobuf, chunksize, audio.bps);



 //in case there is a second SBDY chunk, join the data
 if(FindChunk(file, "SBDY", searchlen - ((int)file->tellg() - startpos)) != -1)
 {
   file->seekg(4, ios::cur);
   chunksize = (file->get()<<24) + (file->get()<<16) + (file->get()<<8) + file->get();
   tptr = new char[*audiobufsize + chunksize];
   if(tptr == NULL) {
     errorstring.append("Can't allocate memory\n");
     return -1;
   }
   memcpy(tptr, *audiobuf,  *audiobufsize);      //copy data of first SBDY
   file->read(tptr + *audiobufsize, chunksize);  //read first SBDY data to mem
   delete[] *audiobuf;  //delete old, too small buffer
   *audiobuf = tptr;    //set pointer to new buffer

   //interleave stereo channels
   if((audio.nch == 2) && (audio.bps != 0))
     InterleaveStereo(*audiobuf + *audiobufsize, chunksize, audio.bps);

   *audiobufsize += chunksize; 
 }
 
 audio.datasize += *audiobufsize;

 return 0;
}



/******************************************************************************/
void IffAnim::parseIFF(fstream* file){
 //TODO for debug: print chunk list -> separate method
 file->seekg(0, ios::beg);
 //files must start with "FORM", "LIST" or "CAT "
 //"PROP"
 
 //detect ILBM image
 //printf(ILBM image, not ANIM)
 
 //printf("tellg: %d\n", file->tellg());
}


/******************************************************************************/
//check file for valid frames, read to mem, get lentime
int IffAnim::ReadFrames(fstream* file)
{
 char** tabuf;    //temporary audio data buffer list, an allocated block for each frame
 int*   tabufsize;  //list of size of a block in bytes (for each frame)
    
 int  i,k;
 char idbuf[8];
 int  chunksize;
 int  ILBMsize;   //size of ILBM chunk
 int  filepos;    //marks position in file 
 int  pos;        //for temporary use
 int  ncolors;
 
 char tptr;       //help pointer
 int  t;          //help variable

 Close();  //close open buffers
 lentime = 0;
 memset(dcompressions, 0, 256/8);

 //allocate / init frame list
 frame = new struct iffanim_frame[nframes];
 for(i = 0; i < nframes; i++) {
   frame[i].cmap = NULL;
   frame[i].data = NULL;
 }
 
 
 
 //debug
 parseIFF(file);




 if(ilbm) {
   file->seekg(0, ios::beg);
 }
 else {
   //set get pointer of file to first frame
   file->seekg(8, ios::beg); //skip "FORM " + chunk size, already checked
   pos = FindChunk(file, "ANIM", 100); //should already point at ANIM chunk, but doesn't hurt
   if (pos < 0)
     return -1;
   file->seekg(4, ios::cur); //skip "ANIM" ID
 }



 //identify IFF ANIM+SLA (IFF ANIM with Statically-Loaded Audio)
 //before the first ILBM chunk in the ANIM chunk there can be a number of 8SVX chunks, multiple sounds
 //FindChunk(file, "SXHD", ILBMsize)
 //printf("ANIM+SLA detected\n");
 //...

 //for all frames (ILBM chunks)
 for(i = 0; i < nframes; i++)
 {
   //pointer should point now to "FORM....ILBM"
   
   file->seekg(4, ios::cur);    

   //get size of ILBM chunk
   ILBMsize = (file->get() << 24) + (file->get() << 16) + (file->get() << 8) + file->get();
   ILBMsize -= 4;
   //save ILBM start position 
   file->seekg(4, ios::cur);
   filepos = file->tellg();

   //the following is only for frame 0
   if(i == 0)
   {
       //search for SXHD (audio header)
       //init audio struct
       if(FindChunk(file, "SXHD", ILBMsize) != -1)
       {
         audio.n = nframes;

         tabuf = new char* [audio.n];        //temporary audio data buffer list, an allocated block for each frame
         tabufsize = new int [audio.n];      //list of sizes of each block in bytes (for each frame)
         for(int j = 0; j < audio.n; j++) {  //init dynamically allocated lists
           tabuf[j] = NULL;
           tabufsize[j] = 0;  
         }

         file->seekg(8, ios::cur);
         audio.bps = (unsigned char)file->get();
         audio.volume = (float)file->get() / 64;
         file->seekg(13, ios::cur);   //skip "length" (4 bytes) and "playrate" (4 bytes) "CompressionMethod" (4 bytes), "UsedChannels" (1 byte)
         audio.nch = file->get();     //only "1" or "2" supported
         audio.freq = (file->get()<<24) + (file->get()<<16) + (file->get()<<8) + file->get();

       }
       file->seekg(filepos, ios::beg);  //back to ilbm chunk start


       //search for BMHD
       pos = FindChunk(file, "BMHD", ILBMsize);
       if(pos == -1) {
         errorstring.append("BMHD chunk not found for first frame\n");
         return -1;
       }
       file->seekg(8, ios::cur);
       //read relevant format info
       w = (file->get() << 8) + file->get();
       h = (file->get() << 8) + file->get();
       file->seekg(4, ios::cur);
       bpp = file->get();      // bitplanes, equal to bits per pixel
       mask = file->get();
       compressed = file->get();
       framesize = ((w + 15) / 16 * 2) * bpp * h; //multiple of 16 bit per plane line

       //check compression
       if(compressed > 1) {
         errorstring.append("Unknown frame compression (0 or 1 expected)\n");
         return -1;
       }
       file->seekg(filepos, ios::beg);  //back to ilbm chunk start

       //search for CAMG chunk (for identifying HAM mode)
       ham = false;
       ehb = false;
       pos = FindChunk(file, "CAMG", ILBMsize);
       if(pos != -1) {
         file->seekg(pos + 10, ios::beg);
         //check if HAM or EHB mode is set
         if(file->get() & 0x8)  ham = true;
         if(file->get() & 0x80) ehb = true;
       }
       file->seekg(filepos, ios::beg);  //back to ILBM chunk start

       //search & read CMAP
       if(bpp <= 8) {
         pos = FindChunk(file, "CMAP", ILBMsize);
         if(pos == -1) {
            errorstring.append("No CMAP chunk found in first frame (color map required)\n");
            return -1;
         }
         read_CMAP(file, &(frame[i]));
         file->seekg(filepos, ios::beg);  //back to ILBM chunk start
       }
   }
   else
   {
       //search for new CMAP
       pos = FindChunk(file, "CMAP", ILBMsize);
       if(pos != -1) {
         read_CMAP(file, &(frame[i]));
       }
       file->seekg(filepos, ios::beg);  //back to ILBM chunk start
   }
   
   
   //search for ANSQ (animation sequence) - shall appear only once at the end or not at all
   //(we still continue looking for more frames)
   //ANSQ data is an array of:
   //  uint16_t frameindex
   //  uint16_t reltime (makes the reltime in ANHD obsolete)
   //last entry has a reltime of 0xFFFF (-1)
   //???? is index 0 the ilbm body and 1 the first DLTA frame?
   //???? what to do with the last frame (it always seem to end on "00 01 ff ff")
   if(FindChunk(file, "ANSQ", ILBMsize) != -1) {
     printf("ANSQ chunk found: ");
     file->seekg(4, ios::cur); //skip chunk ID
     int chunksize = (file->get()<<24) + (file->get()<<16) + (file->get()<<8) + file->get();
     ansq_size = chunksize / (sizeof(uint16_t) * 2);
     ansq = new IFF_ANSQ[ansq_size];
     for(int i; i < ansq_size; i++) {
       ansq[i].dindex = (file->get()<<8) + file->get();
       ansq[i].jiffies = (file->get()<<8) + file->get();
     }
     printf(" lists %d frames (ignored yet)\n", ansq_size);
   }
   file->seekg(filepos, ios::beg);  //back to ILBM chunk start


   //search and read SBDY (audio data chunk) -> one chunk for each frame containing the audio to that frame
   if((audio.nch > 0) && (audio.n > i) && (tabuf != NULL) && (tabufsize != NULL))
   {
     //read first SBDY
     if(FindChunk(file, "SBDY", ILBMsize) != -1)
       if(read_SBDY(file, ILBMsize, &(tabuf[i]), &(tabufsize[i])) == -1) {   //if error occurs
         errorstring.append("Error when reading SBDY chunk!\n");
         break;
       }
     file->seekg(filepos, ios::beg);  //back to ILBM chunk start
   }

   //search & read ANHD (animation header)
   pos = FindChunk(file, "ANHD", ILBMsize);
   if(pos != -1)
     read_ANHD(file, &(frame[i]));
   else {
     frame[i].reltime = 0;
     frame[i].delta_compression = 0;
   }
   lentime += frame[i].reltime;
   file->seekg(filepos, ios::beg);  //back to ilbm chunk start

   //check DLTA compression
   if((frame[i].delta_compression != 0) &&
      (frame[i].delta_compression != 5) &&
      (frame[i].delta_compression != 7) &&
      (frame[i].delta_compression != 8) &&
      (frame[i].delta_compression != 100) &&
      (frame[i].delta_compression != 101) &&
      (frame[i].delta_compression != 74))
   {
     sprintf(errorstring_tmp, "DLTA compression method %d is not supported but used in frame %d\n", frame[i].delta_compression, i);
     errorstring.append(errorstring_tmp);
     break;   //-> at least show the already loaded frames with supported compression
   }

   //search & read BODY or DLTA chunk
   bool body;
   pos = FindChunk(file, "BODY", ILBMsize);
   if(pos == -1) {
     file->seekg(filepos, ios::beg);
     pos = FindChunk(file, "DLTA", ILBMsize);
     if(pos == -1) {
        sprintf(errorstring_tmp,"no BODY or DLTA chunk found for frame %d\n", i);
        errorstring.append(errorstring_tmp);
        break;    //we stop reading frames here, maybe some frames are already read
     }
     else
       body = false;
   }
   else
     body = true;
   file->seekg(4, ios::cur);
  
   //get chunksize, allocate data buffer, read data
   chunksize = (file->get() << 24) + (file->get() << 16) + (file->get() << 8) + file->get();
   frame[i].data = new char[chunksize];
   if(frame[i].data == NULL) {
     errorstring.append("Can't allocate memory\n");
     break;
   }
   frame[i].datasize = chunksize;
   file->read(frame[i].data, chunksize);

   //decompress data from body chunks if RLE compressed, only "body" chunk data can be RLE compressed
   if(body && compressed)
   {
     framesize = (w + 15) / 16 * 2 * bpp * h;
     char* framemem = new char[framesize];  //memory for the RLE decompression is needed
     if(framemem == NULL) {
       errorstring.append("Can't allocate memory\n");
       break;
     }
     DecodeByteRun(framemem, frame[i].data, frame[i].datasize, w, h, bpp, mask); 
     delete[] frame[i].data;        //delete compressed data
     frame[i].data = framemem;      //insert decompressed data into framelist
     frame[i].datasize = framesize; //update size of decompressed data

     if((frame[i].delta_compression != 0)  &&  (frame[i].delta_compression != 1)) //in some files the delta compression is not set correctly for the first BODY frame, only 0 (uncompressed) or 1 (XOR map) is allowed (which can be RLE compression although)
       frame[i].delta_compression = 0;       //assume 0 is the correct value
   }
   else //uncompressed "BODY" or "DLTA"
     dcompressions[frame[i].delta_compression / 8]  |=  1 << (frame[i].delta_compression % 8);     //set bit in compression mode list (body chunks aren't delta compressed -> compression "0"); 

   //set file pointer to next frame
   file->seekg(filepos + ILBMsize, ios::beg);
 }


 //correct number of frames (if there are any corrupt frames)
 if(i != nframes)
   sprintf(errorstring_tmp, "Error occured when opening animation - %d of %d frames loaded\n", i, nframes);
 else
   sprintf(errorstring_tmp, "Animation successfully opened, %d of %d frames loaded\n", i, nframes);
 errorstring.append(errorstring_tmp);
 
 nframes = i;


 //copy audio to single buffer
 if(audio.datasize > 0)
 {
   audio.data = new char [audio.datasize];
   char* ptr = audio.data;
   audio.dataoffset = new int [audio.n];
   int sync = 0;
 
   for(int i = 0; i < audio.n; i++)
   {
     audio.dataoffset[i] = sync;          //set audio start byte for current video frame
     memcpy(ptr, tabuf[i], tabufsize[i]); //copy to single data buffer
     delete[] tabuf[i];
     ptr += tabufsize[i];
     sync += tabufsize[i];
   }
   delete[] tabuf;
   delete[] tabufsize;
 }

 if(i == 0)
   return -1;
 return 0;
}


/******************************************************************************/
const char *IffAnim::GetCompressionName(int comp){
	switch(comp) {
		case 0:
			return "ILBM BODY - no delta compression";
			break;
		case 1:
			return "ANIM-1 - XOR";
			break;
		case 2:
			return "ANIM-2 - Long Delta";
			break;
		case 3:
			return "ANIM-3 - Short Delta";
			break;
		case 4:
			return "ANIM-4 - General Delta";
			break;
		case 5:
			return "ANIM-5 - Byte Vertical Delta";
			break;
		case 6:
			return "ANIM-6 - ANIM-5 with quad buffer for stereo glasses (Cryogenic Software)";
			break;
		case 7:
			return "ANIM-7 - Long/Short Vertical Delta";
			break;
		case 8:
			return "ANIM-8 - Long/Short Vertical Delta";
			break;
		case 'J':
			return "ANIM-J - Eric Graham's compression";
			break;
		case 108:
			return "ANIM-l - Eric Graham's compression";  //Really?? I've never seen files or specs!
			break;
		case 100:
			return "ANIM32 - Long Vertical Delta (Scala)";
			break;
		case 101:
			return "ANIM16 - Short Vertical Delta (Scala)";
			break;
			
		default:
			return "undefined";
			break;
	}
}


/******************************************************************************/
//print format information to text buffer
void IffAnim::PrintInfo()
{
 //HAM info
 char str0[30]; //string segment to be used below
 sprintf(str0,"");
 if(ham)
   sprintf(str0," (HAM%d)", bpp);
 
 
 int ncol = 1 << bpp; //number of colors;
  
 //HAM6: 2^12 = 4096 (4 bit for each RGB component)
 //HAM8: 2^24 = 16.8M
 //  - all possible combination of 2 least significant bits of RGB palette entries (ignoring 6 MSBs) => 2^6, exactly matches Palette size of HAM8
 //  - 6 MSB of a component can be set via "modify" operation
 //  => with right palette entries all 2^24 colours (8 bit per RGB component) can appear in an image, if the image dimensions is large enough
 if(ham){
   switch(bpp){
     case(6):
       ncol = 1 << 12;
       break;
     case(8):
       ncol = 1 << 24;
       break;
   }
 }
 
 //if all frames have a time of 0, we handle them as 1
 char str1[30];
 sprintf(str1,"");
 char str2[30];
 sprintf(str2,"");
 
 int lentime_corrected = lentime;
 if(lentime == 0  &&  nframes > 1) {
   lentime_corrected = nframes;
   sprintf(str1, " (handled as 1 per frame)");
 }
 float lentime_sec = (float)lentime_corrected  / 60;
 sprintf(str2," => %.1f s", lentime_sec);
 
 const char *interlace_info = " (doubling lines of half frames)"; //note about built in deinterlacing
 
 sprintf(formatinfo,
   "Number of frames: %d\n"
   "Width: %d\n"
   "Height: %d\n"
   "Bits per pixel (bitplanar): %d\n"
   "Interlace: %s%s\n"
   "Mask: %d\n"
   "HAM: %s\n"
   "EHB: %s\n"
   "Max. colours per frame: %d%s\n"
   "Total time in 1/60 sec: %d%s%s\n",
   nframes, w, h, bpp,
   interlace_detected? "yes":"no", interlace_detected? interlace_info : "",
   mask,
   ham? "yes":"no",
   ehb? "yes":"no",
   ncol, str0,
   lentime, str1, str2);


 //info about compressions
 strcat(formatinfo, "Compression methods:\n");
 if(compressed)
   strcat(formatinfo, " ByteRun RLE (ILBM BODY)\n");

 int n = 0; //count number of compressions
 for(int i = 0; i < 256; i++)  //list all delta compression modes
 {
   if((dcompressions[i / 8] >> (i % 8)) & 0x1) //check bit of 32*8 bit array
   {
     n++;
     //if(n == 1)
     //  sprintf(formatinfo,"%s %d",formatinfo, i);
     //else
     sprintf(formatinfo,"%s %d (%s)\n",formatinfo, i, GetCompressionName(i));
   }    
 }
 //strcat(formatinfo, "\n");

 if(GetAudioFormat(NULL,NULL,NULL) != -1)
 {
   strcat(formatinfo, "Audio format:\n");
   sprintf(formatinfo,
     "%s"
     " Channels: %d\n"
     " Bits per sample: %d\n"
     " Sample rate: %d\n",
     formatinfo, audio.nch, audio.bps, audio.freq);
   int t;
   if((audio.nch != 0) && (audio.bps != 0))  //prevent division with 0
     t = audio.datasize / audio.nch * 8 / audio.bps;
   else
     t = 0;
   sprintf(formatinfo, "%s Number of sample frames: %d\n", formatinfo, t);
 }
 
}




/******************************************************************************/
// specify, that animation is a special one, determined for looping
bool IffAnim::SetLoopAnim(bool state)
{
 if((state == true) && (nframes >= 4))
   loopanim = true;
 else
   loopanim = false;
 
 return loopanim;
}




/******************************************************************************/
// return info string
char* IffAnim::GetInfoString()
{
 return formatinfo;
}





/******************************************************************************/
// decode frame from frame List, decide which decoding function to call
//  "dstframe" : bitplanar frame buffer, each plane per line padded to multiple of 16 bit
//  "index"    : frame number
int IffAnim::DecodeFrame(char* dstframe, int index)
{
 if(index > nframes)
   return -1;

 //debug - ANHD bits
 //if(index == 1)
 //  frame[index].printBits();

 //decode frame
 switch (frame[index].delta_compression)
 {
   //uncompressed
   case 0 :
     memcpy(dstframe, frame[index].data, frame[index].datasize);
     break;
   //General delta compression
   case 4 :
     DecodeGeneralDelta(dstframe, frame[index].data, w, bpp);
     break;
   //Byte vertical delta compression
   case 5 :
     DecodeByteVerticalDelta(dstframe, frame[index].data, w, bpp);
     break;
   //Short/Long vertical delta method 7
   case 7 :
     DecodeLSVerticalDelta7(dstframe, frame[index].data, w, bpp, (frame[index].bits & 0x1) ? true : false );
     break;
   //Short/Long vertical delta method 8
   case 8 :
     DecodeLSVerticalDelta8(dstframe, frame[index].data, w, bpp, (frame[index].bits & 0x1) ? true : false );
     break;
   //anim 'J'
   case 74:
     DecodeDeltaJ(dstframe, frame[index].data, w, h, bpp); 
     break;
     
   //Scala anim32
   case 100:
     DecodeScalaAnim(dstframe, frame[index].data, frame[index].datasize, w, h, bpp, true, (frame[index].bits & ANHD_BITS_INTERLACE ? true : false), (index & 0x1 ? true :false));
     break;
   //Scala anim32
   case 101:
     DecodeScalaAnim(dstframe, frame[index].data, frame[index].datasize, w, h, bpp, false, (frame[index].bits & ANHD_BITS_INTERLACE ? true : false), (index & 0x1 ? true :false));
     break;

   default: fprintf(stderr, "Frame %d - Unsupported Delta Compression: \"%d\"\n", index, frame[index].delta_compression);
     return -1;
     break;
 }

 return 0;
}




/******************************************************************************/
char* IffAnim::GetFramePlanar(int* framesize_)
{
 if(framesize_ != NULL)
   *framesize_ = this->framesize; 
 
 if(curframe != NULL)
   return curframe;
 else
   return NULL;
}

/******************************************************************************/
//convert planar to chunky, HAM to 24 bit
void* IffAnim::GetFrameChunky(void* framebuf, int pitch)
{
 if(!(file_loaded) || framebuf == NULL)
   return NULL;

 //convert multi to single planar format (including bpp conversion)
 if(ham)
   convertHamTo24bpp(framebuf, curframe, curcmap, w, h, bpp, pitch, 0);
 else  
   bitPlanarToChunky(framebuf, curframe, w, h, bpp, disp_bpp, pitch);  //source can also have more than 8 bits per pixel

 return framebuf;
}

/******************************************************************************/
void* IffAnim::GetCmap()
{
 if(!(file_loaded))
   return NULL;

 if(curcmap == NULL  ||  disp_bpp > 8)
   return NULL;

 //in case current cmap has lesser than 256 entries, a proper palette for the 8 bit frame is useful
 int used = 3 * (1 << bpp);   //number of used bytes for the 256 color map
 memcpy(disp_cmap, curcmap, used);
 return disp_cmap;
}


/******************************************************************************/
//reset to first frame
int IffAnim::Reset()
{
 frameno = -1;   //so next frame is 0

 if(frame[0].delta_compression != 0)  //set to black, if frame 0 has delta compression
    memset(prevframe, 0, framesize);  //the next frame will be decoded to "prevframe"

 NextFrame();    //decompress, increment internal counter (swaps curframe <-> prevframe buffers)

 //make prevframe and curframe the same
 // => frame 1 is delta of frame 0  (special case)
 //    frame 2 is delta of frame 0  (2 frames back from here on)
 //    frame 3 is delta of frame 1
 //    frame 4 is delta of frame 2
 //    ...
 memcpy(prevframe, curframe, framesize);
 prevcmap = curcmap;  //pointers into the frame list only
}





/******************************************************************************/
//- decompress next frame (loop), update counter
//- handle looping 
bool IffAnim::NextFrame()
{
 if(!(file_loaded))  //if no anim file loaded
   return false;
    
 if((frameno + 1) >= nframes) //if last frame
 {   
   if(this->loop == false)    //if looping not set, abort, do nothing -> current frame remains
     return false;     
   else                       //handle looping
   {
     if(loopanim && (nframes >= 4))    //continue at frame 2 (skip the first 2)
       frameno = 1;
     else {
       Reset();    // loads the first frame
       return true;
     }
   }
 }

 frameno++;

 //for interlace compression 100/101 we make each half frame to a full frame by having only one non flipping backbuffer
 // => we don't have to combine 2 DLTA frames into one (Interlace comb filter), as it is normally common for interlace-> progressive conversion
 // => number of Anim frames still correspond to DLTA chunks and we don't need to half the frame rate
 if(interlace_detected)
   SwapFrameBuffers(); //swap back so we only draw to the same buffer

 //decompress to prevframe
 DecodeFrame(prevframe, frameno);
 
 //get cmap pointer
 if((bpp <= 8) && (frame[frameno].cmap != NULL))
    prevcmap = frame[frameno].cmap;

 SwapFrameBuffers();

 return true;
}

/******************************************************************************/
//swap 2 frame buffers (actually only their pointers)
void IffAnim::SwapFrameBuffers(){
 //swap frame buffer pointers
 char* temp;
 temp = curframe;
 curframe = prevframe;
 prevframe = temp;
 //swap cmap
 temp = curcmap;
 curcmap = prevcmap;
 prevcmap = temp;
}



/******************************************************************************/
//returns format information using pointers
int IffAnim::GetInfo(int* w_, int* h_, int* bpp_, int* nframes_, int* mslentime_)
{

 if(w_ != NULL)
   *w_ = w;
 if(h_ != NULL)
   *h_ = h;
 if(bpp_ != NULL)
   *bpp_ = disp_bpp;

 if(nframes_ != NULL) {
   if(loopanim && (nframes >= 4))
     *nframes_ = nframes - 2;
   else
     *nframes_ = nframes;
 }
 
 if(mslentime_!= NULL) {
   if(loopanim && (nframes >= 4))
     *mslentime_ = (lentime - frame[0].reltime - frame[1].reltime) * 1000 / 60;
   else
     *mslentime_ = lentime * 1000 / 60;
 }
 
}


/******************************************************************************/
//return frame delay time in milliseconds
float IffAnim::GetDelayTime()
{
 return ((float)frame[frameno].reltime / 60.0);
}


/******************************************************************************/
// original delay value in 1/60sec
int IffAnim::GetDelayTimeOriginal()
{
 return frame[frameno].reltime;
}


/******************************************************************************/
// activate or deactivate automatic looping
void IffAnim::SetLoop(bool state)
{
 this->loop = state;
}

/******************************************************************************/
//return audio format
int IffAnim::GetAudioFormat(int* nch, int* bps, int* freq)
{
 if(audio.nch <= 0)
   return -1;

 if(nch != NULL) *nch = audio.nch;
 if(bps != NULL) *bps = audio.bps;
 if(freq != NULL) *freq = audio.freq;

 return 0;
}

/******************************************************************************/
//return pointer to audio data and the size in bytes
char* IffAnim::GetAudioData(int* size)
{
 if(size != NULL)
   *size = audio.datasize;
 return audio.data;
}

/******************************************************************************/
//return audio data offset to a specific frame
int IffAnim::GetAudioOffset(int index)
{
 if((audio.dataoffset == NULL) || (index >= audio.n) || (index < 0))
   return 0;
      
 return audio.dataoffset[index];
}

/******************************************************************************/
//return audio data offset to a specific frame with time offset in millisec.
int IffAnim::GetAudioOffset(int index, int msoffs)
{
 int bpsf = audio.bps * audio.nch;   //bytes per sample frame
 return GetAudioOffset(index) + (msoffs * audio.freq * bpsf / 1000); //add "msoffs" in audio data
}

/******************************************************************************/
int IffAnim::GetFrameAudioSize(int index)
{
 if((audio.dataoffset == NULL) || (index >= audio.n))
   return 0;

 if((index + 1) >= audio.n)   //if index is the last frame
   return audio.datasize - audio.dataoffset[index];
 else 
   return audio.dataoffset[index + 1] - audio.dataoffset[index];
}

/******************************************************************************/
int IffAnim::GetFrameAudioSize()
{
 return GetFrameAudioSize(frameno);
}

/******************************************************************************/
int IffAnim::GetFrameAudio(char* abuf, int bufsize, int offs)
{
 int asize = GetFrameAudioSize();
 int n;
 if(bufsize > asize)
   n = asize;
 else
   n = bufsize;

 memcpy(abuf, audio.data + audio.dataoffset[frameno] + offs, n);  //copy audio data
 return n;
}
