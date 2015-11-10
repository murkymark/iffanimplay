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



int SDLGuiFont::id_count = 0; //init static var
SDLGui *guiPtr;


//int minw = 200;
//int minh = 200;

//to allow resizing instantly while dragging (under Windows, main drawing thread would stop)
//SDL_VIDEOEXPOSE events are created when resizing
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
	
	//set icon before SDL_SetVideoMode()
	SDL_WM_SetIcon(SDL_LoadBMP("gfx/icons/icon.bmp"), NULL);
	
	
	SDL_Surface *screen = SDL_SetVideoMode(320, 200, 0, SDL_SWSURFACE | SDL_RESIZABLE);
	if(screen == NULL) {
		cerr << "unable to open SDL video surface: " << SDL_GetError() << endl;
	}

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




	SDLGui *o = (SDLGui*)objectRef;  //cast to object
	o->threadAppEntry();
cout << "returning from thread" << endl;
	return 0;
}



void SDLGui::resizeWindow(int w, int h){
	bool err = false;
cout << "SDLGui::resizeWindow called: "<< w << " " << h << endl;

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
		else {
			screen = SDL_SetVideoMode(w, h, 0, screen->flags);
			if (screen == NULL)
				err = true;
		}
	resizeWindow_callback();
	SDL_mutexV(mutex_screen);

	if(err){
		cerr << "Error: Resize video surface failed" << SDL_GetError() << endl;
		//exit(1);
	}
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
