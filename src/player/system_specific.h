// system specific declarations and code

#ifndef _system_specific_H_
#define _system_specific_H_

#include <cstdio>
#include <iostream>

#include <SDL/SDL.h>
#include <SDL/SDL_syswm.h>

#include "player.hpp"

using namespace std;



// OS to compile for
// define TARGET_OS as compiler parameter manually if needed
// if UNKNOWN, autodetect
#ifndef TARGET_OS
	#define TARGET_OS UNKNOWN
#endif


#if TARGET_OS == UNKNOWN
	#undef TARGET_OS

	#ifdef __linux__	//check for Linux
		#define TARGET_OS LINUX

	#elif defined _WIN32	//check for windows 32/64
		#define TARGET_OS WIN

	#elif defined __MACH__	//check for MacOS
		#define TARGET_OS MAC


	#else
		#define TARGET_OS UNKNOWN

	#endif
#endif




//system dependent init
void system_init();

//system dependent init
//call only after window is opened, may crash otherwise
void system_window_init();

//process system events (drag and drop)
void system_event_proc(SDL_SysWMmsg*, AnimPlayer*);

#endif
