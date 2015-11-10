// Put system specific code to this file

#include "system_specific.h"

#if TARGET_OS == WIN
 #include <shellapi.h>
#endif


//call at program start
void system_init(){

	#if TARGET_OS == WIN
		// Under Windows with the default libSDL apps have no console. This pipes streams to the console again:
		freopen( "CON", "w", stdout );
		freopen( "CON", "w", stderr );
		/*
		an alternative would be the following before the main function
		#if defined _WIN32 || defined WIN32  //defined for both 32-bit and 64-bit environments
			#ifdef main
				#undef main
			#endif
		#endif
		*/
			#endif

}




//drag and drop support, call after window was initialized
void system_window_init(){

	//drag and drop
	#if TARGET_OS == WIN
		SDL_SysWMinfo wmInfo;

		//set SDL version to WMinfo struct
		//-> maybe problems when not calling this macro before
		SDL_VERSION(&wmInfo.version);  

		if(SDL_GetWMInfo(&wmInfo) != SDL_TRUE) {
			cerr << "Error on getting WMInfo" << endl;
			return;
		}

		SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
		DragAcceptFiles(wmInfo.window, true); //afxwin.h
	#endif
	
}


//system event callback
/*
if (e.type == SDL_SYSWMEVENT)
  system_event_proc(SDL_SysWMmsg *m, *player)
*/
void system_event_proc(SDL_SysWMmsg *m, AnimPlayer *p){
	//process drag and drop event
	#if TARGET_OS == WIN

		if(m->msg == WM_DROPFILES) {

			TCHAR lpszFile[1000];	//buffer for file path
			HDROP hDrop = (HDROP)(m->wParam);

			DragQueryFile(hDrop, 0, lpszFile, 1000);
			DragFinish(hDrop);
			//char *file = WIN_StringToUTF8(buffer);
			//CStringA cstrText(lpszFile);
			//cout << "File dragged: " << lpszFile << endl;
			
			cout << "File dragged!" << endl;
			const char *args[] = {"bla0","bla1"};
			args[1] = (const char*)(lpszFile);
			p->clear();
			p->openFile(2, args);
		}

	#elif TARGET_OS == LINUX
	if(m->subsystem == SDL_SYSWM_X11) {
		//todo
	}
	#endif
}

