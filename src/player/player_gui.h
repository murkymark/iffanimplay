//from the generic GUI, this files derives a user gui for the ANIM player 

#ifndef _player_gui_H_
#define _player_gui_H_

class AnimPlayerGui;

#include "../gui/gui.hpp"
#include "player.hpp"

//font related
#define FONT_FILE "gfx/font.bmp"
#define GLYPH_W  6
#define GLYPH_H  12
#define GLYPH_W_BMP 8  //glyph width in bitmap file



class AnimPlayerGui : public SDLGui
{
	public:
	
	//reference to player
	AnimPlayer *player; 
	
	Button *buttonPlay;
	Button *buttonPause;
	Button *buttonStop;
	Button *buttonLoop;
	
	Slider *sliderPos;
	
	int height;		//height of rectangle holding GUI widgets
	
	bool useGL;		//indicates if OpenGL should be used (speedup for scaling and blitting)
	bool showGui;	//show gui indicator
	
	int fontIndex;
	
	//constructor, forward parameters to parent class
	//SDL_Surface *screen: already opened screen can be used as drawing surface
	//string exepath: path to program, to open ressource data which has a relative path to it, otherwise problem if CWD and exe path don't match
	//AnimPlayer *p: pointer to player instance
	AnimPlayerGui(SDL_Surface *screen_, string exepath_, AnimPlayer *p);
	
    void setupFont();
	void render();
	
	//resize, incorporate w and h of GUI (input is only w and h from the video display)
	void myResize(int w_disp, int h_disp);
	
	//play/pause
	void togglePlay(){
	}
	
	virtual void threadAppEntry();
	virtual void resizeWindow_callback();
	virtual void eventPoll();
};

#endif
