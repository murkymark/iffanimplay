#include "player_gui.h"
#include "system_specific.h"

void AnimPlayerGui::threadAppEntry(){

	//open window in default dimensions
	//thread that opens the window is the only one getting events via  SDL_PumpEvents()/SDL_PollEvent() 
	screen = SDL_SetVideoMode(320, 200, 0, SDL_SWSURFACE | SDL_RESIZABLE);
	if(screen == NULL) {
		cerr << "unable to open SDL video surface: " << SDL_GetError() << endl;
	}
	
	//init for drag&drop support
	system_window_init();

//getchar();
cout << "returning from AnimPlayerGui::threadAppEntry" << endl;
//return;
	player->run();
}



void AnimPlayerGui::resizeWindow_callback(){
	int h_disp = screen->h;
	if(showGui)
		h_disp -= height;
	player->Resize(screen->w, h_disp);
}



void AnimPlayerGui::myResize(int w_disp, int h_disp){
	if(showGui)
		h_disp += height; //+ GUI bar height
	resizeWindow(w_disp, h_disp);
}



void AnimPlayerGui::setupFont(){
	string path = string(exepath) + FONT_FILE;
	
	SDLGuiFont *f;
	f = new SDLGuiFont(SDL_LoadBMP(path.c_str()), "Bitstream Vera Sans Mono");
	if (f->glyphSurf == NULL) {
		cerr << "Error loading font image: " << path << endl << SDL_GetError() << endl;
		return;
	}
	SDL_SetColorKey(f->glyphSurf, SDL_SRCCOLORKEY, 0x0); //transparent color black

	//prepare glyphs
	for(int i = 0; i < 256; i++) {
		SDL_Rect r;
		r.x = i * GLYPH_W_BMP;
		r.y = 0;
		r.w = GLYPH_W;
		r.h = GLYPH_H;
		f->glyphRect[i] = r;
	}
	
	addFont(f);
}





 //doing event handling polling in the GUI class only, it would be more clean to have a method in the player class for each key event
 //  -> to avoid calling methods of the parent by the child
 //
 //window input (mouse,keyboard) must be handled by GUI I/O system first, which all are considered to be GUI events
 //the non GUI parent class has to bind methods/functions to GUI events
 //
 //suggestion:
 // to avoid lots of separate methods for each event in the parent class,
 // define a list of specific events, each entry with a specific number -> from the GUI event handler call a basic parent method with that number
 //  where the parent method handles all events via "switch(number){}"

//void AnimPlayerGui::BindEvent(Uint8 type, void *evtHPtr){
 //...
//}



void AnimPlayerGui::eventPoll()
{
	//cout << "event method called" << endl;


 //poll events, empty event queue
 SDL_Event event;
 while(SDL_PollEvent(&event) && !this->signal_appThreadEnd)
 {
   player->eventHandler(&event);
 } //end while
 
 //for multithreading use, delay event polling loops
 
 //SDL_Delay(eventPollingDelayMs);

}



AnimPlayerGui::AnimPlayerGui(SDL_Surface *screen_, string exepath_, AnimPlayer *p) : SDLGui(screen_, exepath_)  {
 //at this point, after the parent constructor is called, screen is open and != NULL, if no error occured

 player = p;

 height = 50;  //GUI bar
 showGui = true;
 useGL = false;
 
 sliderPos = new Slider;
 sliderPos->setImages("slider0.bmp", "slider1.bmp");
 sliderPos->setMinMax(0, player->numframes - 1);

 buttonPlay = new Button();
 buttonPlay->setImages("control0.bmp", "control1.bmp");
 buttonPause = new Button();
 buttonPause->setImages("control-pause0.bmp", "control-pause1.bmp");
 buttonStop = new Button();
 
 buttonLoop = new Button();
 buttonLoop->setImages("arrow-circle-315-left0.bmp", "arrow-circle-315-left1.bmp");
}



void AnimPlayerGui::render(){
	//for(int i = 0; i < height; i++)
	//  lineH(*screen, 0, i, (*screen)->w, BG_COLOR( ((float)i / (height-1)) ));
	SDL_Rect r = {0, 0, screen->w, this->height};
	SDL_Color bg0 = BG_COLOR_0;
	SDL_Color bg1 = BG_COLOR_1;
	//fillRectGradient(screen, &r, 89, bg0, bg1); //not working yet
	
	buttonPlay->draw(screen, 100, 10);
    buttonPause->draw(screen, 130, 10);
    buttonLoop->draw(screen, 160, 10);
	
	sliderPos->setPos(player->GetFrameIndex());
    sliderPos->draw(screen, 200, 20);
	
  // writeText(screen, 0, 2,0, "File: \"abcd/anim\"", 0xff,0xff,0xff);
  //  writeText(screen, 0, 2,11, "00:00:00.000 / 12:35:57.678", 0xff,0xff,0xff);
}

