//part of generic graphical user interface
//this file contains the widgets

#ifndef _gui_widgets_H_
#define _gui_widgets_H_

class Panel;

#include "gui.hpp"


// All widgets use 32 bit color resolution, neccessary for alpha channel information
//  a panel groups different widgets, all other widgets should be added to the panel (and shouldn't be blit directly to the screen)
//  when a panel is drawn a temporary surface is created, which can be blitted efficently to a lower color resolution surface (screen)
//  all widgets of a panel are on one layer and should not overlap

class Widget
{
	public:
	//rect dimensions and position
	SDL_Rect r;  //screen location and mouse area
	string tooltipStr;
	bool redraw; //indicates, if widget was modified so it has to be redrawn
	
	Widget(): redraw(true) {
		memset(&r, 0, sizeof(SDL_Rect));
	}
	
	bool intersectRect(const SDL_Rect *r2, SDL_Rect *intersection);   //get intersection rectangle of widget rectangle with another
	
	//draw to destination surface
	virtual void draw(SDL_Surface *s){};

	virtual void resize(int wnew, int hnew){
		r.w = wnew; r.h = hnew;
	}

	void setXY(int xnew, int ynew){
		r.x = xnew; r.y = ynew;
	}
	
	void setTooltip(string s){
		tooltipStr = s;
	}
	
	//events
	
	//hoover
	//unhover
	//press
	//release
};


//a rectangular area grouping other widgets
class Panel : Widget
{
	public:
	
	SDL_Surface *surf;	//allocated panel surface (plain color or image)
	SDL_Surface *surfMerged; 	//panel with all widgets from the list drawn to it, for quick redraw
	
	list<Widget> wlist;    //list of widgets
	
	Panel() : surf(NULL), surfMerged(NULL) {};
	
	~Panel(){
		if(surf != NULL) SDL_FreeSurface(surf);
		if(surfMerged != NULL) SDL_FreeSurface(surfMerged);
	}
	//add widget to list
	void Add(Widget &w){
		w.redraw = true;
		redraw = true;
		wlist.push_back(w);
	}
	
	void prepareSurface();
	
	//draw panel and all widgets in the list
	void draw(SDL_Surface *dst);

};

class Button : Widget
{
 public:
	
	SDL_Surface *buttonSurf[2];  //unpressed, pressed
	bool pressed;


	Button() :
		pressed(false) {
	}
	
	//compose button images, unpressed and pressed
	void setImages(const char *path0, const char *path1);
 
	void draw(SDL_Surface *dst);
	void draw(SDL_Surface *dst, int x, int y); //for debugging only
};


class Slider : Widget
{
	public:
	
	SDL_Surface *thumb;
	SDL_Surface *thumbGhost; //outline when dragging to new position
	SDL_Surface *bar;  //background bar, rendered again when resized
	
	int min, max, pos;
	bool dragged; //indicates if thumb is currently dragged (button pressed)
	
	void setImages(const char *path0, const char *path1);
	//set value for left and right end, steps are interpolated
	void setMinMax(int left, int right){
		min = left; max = right;
	}
	void setPos(int p){
		pos = p;
		if(p > max) max = p;  //extend
	}
	
	void draw(SDL_Surface *dst, int x, int y);
};


#endif
