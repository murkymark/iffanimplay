//part of generic graphical user interface, only depends on SDL
//this file contains the core
/*
todo:
 single and multithread support
 in most GUI libs
   main opens and builds GUI
     the the GUI run method/function is called -> returns when GUI/Window is closed
     main returns -> program exit
   the GUI runs in a single main thread
   when the GUI handles events it calls previously bound methods


typedef union
{
  Uint8 type;
  SDL_ActiveEvent active;
  SDL_KeyboardEvent key;
  SDL_MouseMotionEvent motion;
  SDL_MouseButtonEvent button;
  SDL_JoyAxisEvent jaxis;
  SDL_JoyBallEvent jball;
  SDL_JoyHatEvent jhat;
  SDL_JoyButtonEvent jbutton;
  SDL_ResizeEvent resize;
  SDL_ExposeEvent expose;
  SDL_QuitEvent quit;
  SDL_UserEvent user;
  SDL_SysWMEvent syswm;
}
SDL_Event;
  
  
  
add "bind()" method for all events (builds up a list for all events it shall check for)
 bind(Uint8 type, method/function pointer, void *userdata)  //type is the SDL type
 bind(Uint8 type, method/function pointer, int usernumber)
 
 bind_widget() //bind call to widget event
 
 
 set_poll_delay_ms(int) //passed directly to sdl_delay(), wait time after polling all events from the event queue
 set_poll_delay(float) //in seconds (converted to ms, which may be rounded down to 0, if too small)
*/


#ifndef _gui_H_
#define _gui_H_


#include <iostream>
#include <algorithm>
#include <queue>
#include <list>
#include <math.h>

#include <SDL/SDL.h>

using namespace std;

#include "gui_widgets.hpp"
#include "gui_event.hpp"

#define SDLGUI_DEBUG
#ifdef SDLGUI_DEBUG
 #define DEBUG(EXPR) (EXPR) //include expression
#else
 #define DEBUG(EXPR) () //exclude expression
#endif


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


//event filter, handling window resize events (needed for proper resize behavior in Windows)
int eventFilterResize( const SDL_Event *e );
//thread starter, helper function
int runAppThread(void *objectRef);

//prototypes
class AnimPlayer;
class Button;
int runGuiThread(void *objectRef);

class SDLGui;
extern SDLGui *guiPtr; //to access gui class members




#define COLOR_PLAIN()         (SDL_MapRGB(&(guiPtr->format), 0xcc, 0xdd, 0x88)) //button color
#define COLOR_BORDER_LIGHT()  (SDL_MapRGB(&(guiPtr->format), 0xff, 0xff, 0xff))
#define COLOR_BORDER_SHADOW() (SDL_MapRGB(&(guiPtr->format), 0x66, 0x66, 0x66))

#define COLOR_BLUE_LIGHT()    (SDL_MapRGB(&(guiPtr->format), 0x00, 0xff, 0xff))
#define COLOR_BLACK()         (SDL_MapRGB(&(guiPtr->format), 0x00, 0x00, 0x00))

//i is scale factor for color change (gradient)
#define BG_COLOR(I)  (SDL_MapRGB(&format, 0x58 - (int)((float)I * 30), 0x58 - (int)((float)I * 30), 0x70 - (int)((float)I * 30) )) 
#define BG_COLOR_0   {0x58, 0x58, 0x70}
#define BG_COLOR_1   {0x58-30, 0x58-30, 0x70-30}




//bitmap font, up to 256 different glyphs (characters)
// takes allocated surface and frees it by itself upon destruction
class SDLGuiFont {
	public:
	SDL_Surface *glyphSurf;  //surface with all glyphs
	SDL_Rect glyphRect[256]; //rectangle to cut out glyph from the surface, must be setup manually
	string name;
	int const id;		//unique reference id number
	static int id_count;

	//constructor
	// add allocated surface, is then managed by the object
	// set the rectangles for the glyphs manually
	SDLGuiFont(SDL_Surface *s, const char *name_ = NULL): glyphSurf(s), id(id_count){
		if(name_ == NULL){
			name = "Noname";
			name += id_count;
		}
		id_count++;
		memset(glyphRect, 0, 256 * sizeof(SDL_Rect));
        DEBUG(cout << "Font: " << name << " created" << endl);
	};
	
	~SDLGuiFont(){
		if(glyphSurf != NULL) SDL_FreeSurface(glyphSurf);
        DEBUG(cout << "Font" << name << " freed" << endl);
	}
};





//only one instace allowed
class SDLGui
{
 public:

	string exepath;  //executeable path, relative to CWD, important for loading ressource data relative to exe (else the CWD incorrectly is the directory, files are opened from)

	SDL_Surface *screen;  //may change if resized, that's why ptr of ptr

	//format of all GUI graphics (other formats are converted to this)
	// create new gui surfaces only via methods provided in this class
	SDL_PixelFormat format;

	list<SDLGuiFont*> font; //all glyphs on one surface
	list<Panel*> panel; //list of panels, the basic GUI element (root of a widget tree)

	queue<SDL_Event*> eventQ;  //queue of gui events, must be filled from extern thread


	int eventPollingDelayMs;  //delay in the event polling loop

	SDL_Thread *appThread;    //application thread, NULL if thread not running
	bool signal_appThreadEnd; //signals end to application thread
	
	SDL_mutex *mutex_screen;



	//already opened screen can be used as drawing surface, if set to 0, a default screen surface is opened (window 320x200, desktop format)
	SDLGui(SDL_Surface *screen_, const string& exepath_ = "");
	
	~SDLGui();
	
	
	
	//user code, called during a window resize inside mutex (also in case of window resize event)
	virtual void resizeWindow_callback(){};
	
	//resize the Window
	void resizeWindow(int w, int h);
	


	

	
	//start GUI thread
	void threadAppStart(){
		if(appThread == NULL){
			signal_appThreadEnd = false;
			//appThread = SDL_CreateThread( &runAppThread, this); //start via plain C helper functioin
			runAppThread(this);
			DEBUG(cout << "App thread (" << SDL_GetThreadID(appThread) << ") started" << endl);
		}
	}
	
	//user code, this member function is called in a new thread (application thread)
	virtual void threadAppEntry() { cout << "GUI parent app entry - default virtaul method called. Implement in the derived child class!" << endl;};
	
	void threadAppEnd(){
		if(appThread != NULL){
			//signal end to thread and wait until it ends
			//TODO: wait to make sure the thread is at a safe point
			signal_appThreadEnd = true; //App thread only reads , so no mutex here
			DEBUG(cout << "App thread (" << SDL_GetThreadID(appThread) << "), wait for ending" << endl);
			SDL_WaitThread(appThread, NULL);
			appThread = NULL;
			DEBUG(cout << "App thread ended" << endl);
		}
	}
	


	//Change event polling delay (at the beginning it has a default value)
	// 0: no delay, the polling loop runs at fast as possible -> high CPU usage, not recommended
	// 1..10 ms recommended
    void SetEventPollingDelay(int d){
		eventPollingDelayMs = d;
	}

	// Add a font to the font list, which is managed by SDLGui
	//  font must already be allocate and setup manually, before passed
	//  don't deallocate added font, it will be freed automatically
	//  the font can be referenced with it's reference number
	void addFont(SDLGuiFont *f){
		font.push_back(f);
	}

 //render gui
 void render();


 void writeText(SDL_Surface *s, int i, int x, int y, const char *str, Uint8 r, Uint8 g, Uint8 b){
	 //get font from list
	SDLGuiFont *f;
	for (list<SDLGuiFont*>::const_iterator ci = font.begin(); ci != font.end(); ++ci)
		f = *ci;
	 
//	font->format->colors  =  r,g,b //map color
	int len = strlen(str);
	int posx = x;
	int posy = y;
	for(int i = 0; i < len; i++){
		 unsigned char c = (unsigned char)str[i];
		 SDL_Rect rd;
		 rd.x = posx;
		 rd.y = posy + 1;
		 rd.w = f->glyphRect[c].w;
		 rd.h = f->glyphRect[c].h - 1;
		 if(c == '\n') {  //newline
			posy += f->glyphRect[c].h;
			posx = x;
		 }
		 else {
			SDL_BlitSurface(f->glyphSurf, &(f->glyphRect[c]), s, &rd);
			posx += f->glyphRect[c].w;
		 }
	 }
 }
 

 void blit(SDL_Surface *src, SDL_Surface *dst, int x, int y){
	SDL_Rect r;
	r.x = x;
	r.y = y;
	r.w = src->w;
	r.h = src->h;
	SDL_BlitSurface(src, NULL, dst, &r);
 }

 void lineH(SDL_Surface *s, int x, int y, int w, Uint32 col){
	SDL_Rect r;
	r.x = x;
	r.y = y;
	r.w = w;
	r.h = 1;
	SDL_FillRect(s, &r, col);
 }
 
 void lineV(SDL_Surface *s, int x, int y, int h, Uint32 col){
	SDL_Rect r;
	r.x = x;
	r.y = y;
	r.w = 1;
	r.h = h;
	SDL_FillRect(s, &r, col);
 }

 //fill rectangle with linear interpolated color gradient
 //angle: degrees in which we c0 fades to c1 (in reference to a circle this is the fading angle/direction, where c0 represents the center)
 //  fast degrees - c0 to c1 - single colored lines are drawn:
 //     0 - left to right,
 //     180 - right to left (like 0 but with c0 swapped with c1),
 //     90 - bottom to top
 //     180 - top to bottom
 //  other angles uses slower per pixel interpolation
 //  for diagonal angles c0 and c1 start in opposing corners and the rectangle represents a clipped section of a parallelogram
 void fillRectGradient(SDL_Surface *dest, SDL_Rect *destRect, float angle, SDL_Color c0, SDL_Color c1){
	 
	//can be used as increment and as counter/accumulator
	class ColorStep{
		 public:
		 int r,g,b;  //fixed point 8.16, integer part always in range 255..-255
		 
		ColorStep()
		{}
		ColorStep(SDL_Color c)
		{
			set(c);
		}
		ColorStep(SDL_Color c0, SDL_Color c1, int nsteps){
			diff(c0, c1);
			div(nsteps);
		}

		void set(SDL_Color c)
		{
			r = (c.r << 16);
			g = (c.g << 16);
			b = (c.b << 16);
		}
		void diff(SDL_Color c0, SDL_Color c1){
			r = (c1.r - c0.r) << 16;
			g = (c1.g - c0.g) << 16;
			b = (c1.b - c0.b) << 16;
		}
		void div(int nsteps){  //divide in n steps
			if(nsteps == 0) nsteps = 1;  //prevent /0
			r /= nsteps;
			g /= nsteps;
			b /= nsteps;
		}
		void step(ColorStep cs){
			r += cs.r;
			g += cs.g;
			b += cs.b;
		}
		Uint32 mapRGB(SDL_Surface *s){
			return SDL_MapRGB(s->format, r >> 16, g >> 16, b >> 16);
		}
	};
	 
	class Vector2D{
		 public:
		 float x,y;
		 int u,v;  //only available after toScreenCoordFixP(), fix point 16.16

		 Vector2D(float x_, float y_) : x(x_), y(y_)
		 {}
		 void rotate(float degAng){  //rotate around 0,0
			float radAng = degAng * M_PI / 180;
			float sinVal = sin(radAng);
			float cosVal = cos(radAng);
			float newx = x * cosVal - y * sinVal;
			float newy = y * cosVal + x * sinVal;
		 }
		 void normalize(){
			 float len = sqrt(x*x + y*y);
			 if(len == 0) len = 1;
			 x /= len;
			 y /= len;
		 }
		 void toScreenCoordFixP(){
			 u = x * 0x10000;
			 v = -y * 0x10000;  //flip y sign for carthesic to screen coords
		 }
		 void mul(float f){
			x *= f;
			y *= f;
		 }
	 };



	 SDL_Rect destRectAlt = {0,0, dest->w, dest->h};  //alternative
	 if(destRect == NULL)
		destRect = &destRectAlt;

	 angle = fmod(angle, 360);  //makes angle always positive
/*
	 if(angle == 0  ||  angle == 90  ||  angle == 180  ||  angle == 270) {
		SDL_Color ct;
		switch ((int)angle){
			case 180:
				ct = c0; //swap
				c0 = c1;
				c1 = ct;

			case 0:
			{
				ColorStep cstep(c0,c1, destRect->w - 1);
				ColorStep cacc(c0);
				for(int i = 0; i < destRect->w; i++) {
					lineV(dest, i + destRect->x, destRect->y, destRect->h, cacc.mapRGB(dest));
					cacc.step(cstep);
				}
				break;
			}


			case 90:
				ct = c0; //swap
				c0 = c1;
				c1 = ct;
			case 270:
			{
				ColorStep cstep(c0,c1, destRect->h - 1);
				ColorStep cacc(c0);
				for(int i = 0; i < destRect->h; i++) {
					lineH(dest, destRect->x, i + destRect->y, destRect->w, cacc.mapRGB(dest));
					cacc.step(cstep);
				}
				break;
			}
		}
	 }
*/
if(0);
	 else {
		 //todo
		 
		 //get gradient direction
		 Vector2D vdirH(destRect->w - 1, 0); //(0,0) -> (w-1,0)  carthesic coordinates, gradient in this direction
		 vdirH.rotate(angle);		 
		 vdirH.normalize();
		 Vector2D vdirV(vdirH.y, - vdirH.x); //(0,0) -> (0,h-1) screen coordinates, previous vector rotated clockwise -90 deg, no gradient in this direction
		 
		 //rectangle is filled with a rotated "texture", the color gradient
		 //the rectangle to fill is the inner rectangle of a rotated texture rectangle
		 
		 //we need the dimenions of the texture rectangle -> so we temporary rotate the target rectangle, which dimensions is known, with the negative angle and look at the rotated corner points
		 //or we can use angle function to caculate
		 
		 //texture size of "-angle" same as with "angle"
		 //since we have only a positive angle here we only need to regard 0..90
		 angle = fmod(angle,180); //directions reverted but lengths are the same
		 
		 bool do_swap = false;
		 if(angle >= 90){  //length and height must be swapped for 90..180
			angle -= 90;
			do_swap = true;
		 }
		 
		 //deg to rad
		 angle = angle * M_PI / 180;
		 float sin_a = sin(angle);
		 float cos_a = cos(angle);
		 
		 float a1 = cos_a * destRect->w;
		 float a2 = sin_a * destRect->h;
		 float a = a1 + a2;
		 
		 float b1 = sin_a * destRect->w;
		 float b2 = cos_a * destRect->h;
		 float b = b1 + b2;
		 
		 if(do_swap) {
			 swap(a,b);
			 swap(a1,b1);
			 swap(a2,b2);
		 }
		 
		 ColorStep c_tl, c_tr; //color at top left corner, top right corner
		 ColorStep c_bl, c_br;
		 

		 ColorStep c_diff;  //color differenc (range) between c0 and c1
		 c_diff.diff(c0, c1);




		 //now a is length in gradient direction
		 vdirH.mul(a);
		 vdirV.mul(b);
		 


		 c_tl = c_diff;
		 c_tl.div(a / a1);
         c_tl.step(c0);
		
		 c_tr.set(c1);
		 c_tr.div(a / a1);
         c_tr.step(c0);
		 /*	 
		 c_tl.mul(a1);

		 cHd  //horizontal color delta -> increment or decrement
		 cVd  //vertical color delta
		 */
		 
		 cout << "debug gradient " << a << " " << b << endl;
		 cout << "debug gradient2 " << a2 << " = " << sin_a << " * " << destRect->w << endl;
	 }
 }

 //create rectangle surface filled with color, 32 always
 SDL_Surface *createRectSurf(int w, int h, Uint32 col){
	 SDL_Surface *s = SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 32, format.Rmask, format.Gmask, format.Bmask, format.Amask);
	 SDL_FillRect(s, NULL, col);
	 return s;
 }
 
 //draw raised or sunken 3d border
 //rectangle specifies where border is drawn in surface, if NULL at the edges
 void draw3DBorder(SDL_Surface *s, bool raised, SDL_Rect *rec){
	Uint32 light = COLOR_BORDER_LIGHT();
	Uint32 shadow = COLOR_BORDER_SHADOW();
	if(raised)
		swap(light, shadow);
	 
	 SDL_Rect r;
	 if(rec == NULL){
		 r.x = 0;
		 r.y = 0;
		 r.w = s->w;
		 r.h = s->h;
		 rec = &r;
	 }
	 
	 lineV(s, rec->x,   rec->y,   rec->h,   light);
	 lineV(s, rec->x + rec->w-1, rec->y+1, rec->h-1, shadow);
	 lineH(s, rec->x,   rec->y,   rec->w,   light);
	 lineH(s, rec->x+1, rec->y + rec->h-1, rec->w-1, shadow);
 }



 SDL_Surface *loadIcon(const char *path){
	string p = string(exepath) + "gfx/icons/" + path;
	SDL_Surface *icon = SDL_LoadBMP(p.c_str());
	if (icon == NULL) {
		cerr << "Error loading bitmap: " << p << endl << SDL_GetError() << endl;
		//create empty bitmap as dummy
		icon = SDL_CreateRGBSurface(SDL_SWSURFACE, 8, 8, format.BitsPerPixel, format.Rmask, format.Gmask, format.Bmask, format.Amask);
		if (icon  == NULL) {
			cerr << "Error: " << SDL_GetError() << endl;
			return NULL;
		}
	}
	//SDL deactivates alpha channel in BMP surface -> just add the alpha channel mask and it works
	#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		icon->format->Amask = 0x000000ff;
	#else
		icon->format->Amask = 0xff000000;
	#endif
	
	SDL_Surface *icon1 = SDL_DisplayFormatAlpha(icon); //convert to display format for speedup
	if(icon1 == NULL) {
		cerr << "GUI image loader: Image format conversion failed!" << endl;
		return icon;
	}
	
	//success
	SDL_FreeSurface(icon);
	return icon1;
 }



 Uint32 buttonColor(){
	 return COLOR_PLAIN();
 }
 

 
 //runs in own thread, polling loop
 virtual void eventPoll() = 0;

};



/*


buttonPlay //play button
buttonStop //stop/rewind button
buttonLoop //ch
sliderPos


class UIManager
{
 public:
	//returned
	struct widgetLoc{
		void *ptr; //pointer and id, to identify the widget it should be compared with a list of widget adresses
		int x; //x offset of widget start (0,0) from top left
		int y; //y offset
	}
	list<Widget> wlist; //list of widgets
	press
	release
	mousemove
	
	//with given panel coordinates it returns
	widgetLoc location(int x, int y){

	}
}

*/


#endif
