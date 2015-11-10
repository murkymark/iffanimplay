

#include <vector>


// IFF 8SVX
// Needed for ANIM+SLA (static loaded audio)
// Octaves or envelopes are ignored. It is unlikely to be used with ANIM.
// Volume is also ignored, given in SCTL


#define sCmpNone       0    // not compressed
#define sCmpFibDelta   1    // Fibonacci-delta encoding

typedef struct
{
	uint32_t oneShotHiSamples;  // # samples in the high octave 1-shot part
	uint32_t repeatHiSamples;   // # samples in the high octave repeat part
	uint32_t samplesPerHiCycle; // # samples/cycle in high octave, else 0
	uint16_t samplesPerSec;     // data sampling rate
	uint8_t  ctOctave;          // # octaves of waveforms
	uint8_t  sCompression;      // data compression technique used
	int32_t  volume;            // fixed point 16.16 value
} Voice8Header; //8SVX


struct SoundControl {
   UBYTE  sc_Command, // What to do, see below
          sc_Volume;  // Volume 0..64
   USHORT sc_Sound,   // Sound number (starting with 1, order of storage)
          sc_Repeats, // Number of times to play the sound (0 is endless)
          sc_Channel, // Channel(s) to use for playing (bit mask)
          sc_Frequency, // If non-zero, overrides the VHDR value
          sc_Flags;   // Flags, see below
   UBYTE  pad[4];     // For future use
};





//one IFF 8SVX sound
class Iffanim_sla_sound {
	void *data;
	int size;
	int compression;
	
	load(void *body, int bodysize, void *vhdr, int vhdrsize);
	//Iffanim_sla_sound
	//~Iffanim_sla_sound
};


class Iffanim_sla {
 std::vector<Iffanim_sla_sound>bank;

 //copy data to destination buffer from audio index
 //return number of written bytes
 int getData(void *dest, int index, int offset, int size)
};