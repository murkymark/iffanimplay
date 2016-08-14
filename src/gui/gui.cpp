#include "gui.hpp"


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




SDLGui *guiPtr;


//int minw = 200;
//int minh = 200;

//to allow resizing instantly while dragging (under Windows, main drawing thread would stop)
//SDL_VIDEOEXPOSE events are created when resizing
//a forced resize via SDL_SetVideoMode
//may run in a separate thread on some systems, not in MS Windows
int eventFilterResize( const SDL_Event *e )
{
    if( e->type == SDL_VIDEORESIZE )
    {
		int newh = e->resize.h;
		if(newh <= 0) newh = 1; //minimal 1, else leads to crash
		guiPtr->resizeWindow(e->resize.w, e->resize.h);
		return 0; //don't add to queue because we handled it already
    }
    return 1; // return 1 -> add to queue
}



int ef(const SDL_Event *event){
	cout << "event filter called" << endl;
}



//thread starter
int runAppThread(void *objectRef){
	
	SDLGui *o = (SDLGui*)objectRef;  //cast to object
	
	//load and set application icon
	SDL_Surface *icon = o->loadImageFile("icon.bmp", false);
	if(icon != NULL){
		bool ok = true;
		#ifdef _WIN32
			//MS Windows needs exactly 32x32 icon or it shows garbage
			if(icon->w != 32  ||  icon->h != 32) {
				cerr << "Can't use icon with " << icon->w << "x" << icon->h << ", MS Windows needs icon dimensions of exactly 32x32" << endl;
				ok = false;
			}
		#endif
		if(ok){
			//make sure exe icon has no blank alpha channel -> convert to 24 bit
			if(icon->format->BitsPerPixel == 32) {
				SDL_PixelFormat f = *(icon->format);
				f.BitsPerPixel = 24;
				f.BytesPerPixel = 3;
				SDL_Surface *icon1 = SDL_ConvertSurface(icon, &f, 0);
				SDL_FreeSurface(icon);
				icon = icon1;
			}
			//set icon before SDL_SetVideoMode()!
			SDL_WM_SetIcon(icon, NULL);
		}
		SDL_FreeSurface(icon);
	}
	
	
	
	//SDL_Surface *screen = SDL_SetVideoMode(320, 200, 0, SDL_SWSURFACE | SDL_RESIZABLE);
	//if(screen == NULL) {
	//	cerr << "unable to open SDL video surface: " << SDL_GetError() << endl;
	//}

// SDL_SetEventFilter(ef);
//SDL_Delay(20);

//SDL_Event event;
// while(1)
// {	
//	 SDL_PollEvent(&event);
//	switch(event.type)
//	{
//		case SDL_KEYDOWN:      //key event
//			break;
//	}
//	SDL_PollEvent(&event);
//	cout << "event occurence thread 2 " << (int)event.type << endl;
//	SDL_Delay(10);
//
//
//}


	o->threadAppEntry();
cout << "returning from thread" << endl;
	return 0;
}



void SDLGui::resizeWindow(int w, int h){
	bool err = false;
	bool skip = false;

	//make sure dimensions are valid
	if(w <= 0 || h <= 0) {
		err = true;
		cerr << "Error! Video surface resize failed, invalid dimensions: " << w << " x " << h << endl;
		return;
	}

	SDL_mutexP(mutex_screen);
	
		//at this point no other thread is allowed to access the old surface pointer anymore -> screen access only safe in a mutex
		if (screen == NULL)
			err = true;
		else if(w == screen->w  &&  h == screen->h)
			skip = true; //no need to resize
		else {
			screen = SDL_SetVideoMode(w, h, screen->format->BitsPerPixel, screen->flags);
			if(screen == NULL) {
				cerr << "Error: Resize video surface failed - " << SDL_GetError() << endl;
				err = true;
			}
		}
		
	if(err){
		cerr << "Retrying with save parameters ..." << endl;
		//lets try to open a save window mode, maybe an unsupported fullscreen mode was requested before
		screen = SDL_SetVideoMode(320, 240, 0, SDL_SWSURFACE | SDL_RESIZABLE);
		if(screen == NULL) {
			cerr << "Error: Resize video surface failed again - " << SDL_GetError() << endl;
			cerr << "Program shall quit now" << endl;
		}
	}
	
	if(!skip)
		resizeWindow_callback();
	
	SDL_mutexV(mutex_screen);
}

SDLGui::SDLGui(SDL_Surface *screen_, const string& exepath_){
		screen = screen_;
		exepath = exepath_;
		appThread = NULL;
		eventPollingDelayMs = 3; //set default value, 0 would poll at full CPU usage
		guiPtr = this;
		
		//if screen is NULL we open it now with default values
		if(screen == NULL){
			screen = SDL_SetVideoMode(320, 200, 0, SDL_SWSURFACE | SDL_RESIZABLE); //desktop bits per pixel
			if(screen == NULL) {
				cerr << "unable to open SDL video surface: " << SDL_GetError() << endl;
				return;
			}
		}
		
		//Setup for 32 bit graphics: RGBA
		// generate a format struct -> internal format of all GUI graphics
		SDL_Surface *s = SDL_CreateRGBSurface(SDL_SWSURFACE, 1, 1, 32, RMASK, GMASK, BMASK, AMASK);
		format = *(s->format);
		SDL_FreeSurface(s);
		
		mutex_screen = SDL_CreateMutex();
		SDL_SetEventFilter( &eventFilterResize );
}

SDLGui::~SDLGui(){
		//disable event filter
		SDL_SetEventFilter(NULL);
		threadAppEnd();
		SDL_DestroyMutex(mutex_screen);
		
		//at this point no other thread must run anymore, which accesses SDLGui or members
		font.clear();
		panel.clear();
}
