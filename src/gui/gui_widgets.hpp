//part of generic graphical user interface
//this file contains the widgets

#ifndef _gui_widgets_H_
#define _gui_widgets_H_

class Panel;

#include "gui.hpp"


//default colors
// Col_bg (0xd0,0xd0,0xd0) //default panel color
// Col_Text
// Col_TextBG
// Col_TextBG_Disabled
// Col_borderRaisedTopLeft
// Col_borderRaisedRightBottom
// Col_borderSunkenTopLeft
// Col_borderSunkenRightBottom
// Col_Border (0,0,0)

// All widgets use 32 bit color resolution, neccessary for alpha channel information (-> better make individual, can save memory)
//  a panel groups different widgets, all other widgets should be added to the panel (and shouldn't be blit directly to the screen surface?)
//  when a panel is drawn a temporary surface is created, which can be blitted efficently to a lower color resolution surface (screen)
//  all widgets of a panel are on one layer and should not overlap

// - widget has anchor to stuck to it's side, if 2 opposit anchors are set, it resizes
//   - containers are anchord to all sides by default, other widgets are anchored to left by default

//typical widget tree
//..

/*
 widgets:

 layoutContainer //wxwidget calls it just sizer, Tkinter calls it geometry manager
   table     //table based
   panel     //pin to specified x,y position, if

 text based  //have font reference
   label
   textbox
 
 other:
 //VideoContext - bound to window size //not needed -> single cell tab seems best as primary widget -> draw onto SDL screen surface
 Panel - rectangle, used as background, holds no more than 1 sub
 Box - like a panel, but draws a box and can have a left aligned headline headline (QT group Box)
 canvas - a surface for drawing pixel data (resize clears invisible content if resizeable)
 Label - text
 Button - can contain image (surface) or label
 ProgressBar - left to right progress
 LineH  - used for separation
 LineV
 subwindow - can be dragged, not resized by parent, useful as message box
 spacer //invisible rectangle used for margin/padding of other widgets
 MenuBar - text based drop down menu, tree of submenus (may contain little images)
 tabs - a panel container (only horizontal, left aligned)
 list
 table - text lines multiple columns, sortable
 checkbox
 radiobutton
 slider
*/

#include <string>

//std::u32string abc;

//holds one or more widgets
//cannot contain itself
class SDLGuiWContainer {
	public:
	
	//add widget (use container type specialized add() to give parameters)
	//cannot add itself, traverses through sub container tree to be sure
	//return true on success, else false
	//bool add(widget w)
	
	//removes all sub widgets, deletes all that are marked to be deleted
	//clear();
	
	//search for 
	//bool remove(widget *f)
};


class SDLGuiWContainerTable : public SDLGuiWContainer {
	public:
	
	//insert in cell, expands table if needed
	//
	bool insert(int x, int y);
	//addLeft() //useful for single row
	//addBottom() //useful for single column
	
	//set number of cells, horizontal and vertical
	//default is 1,1 (single cell), coordinates 0,0
	void SetSize(int nx, int ny);
	
	//reduces cell dimension as much as possible, if cells are empty
	void MinimizeSize();
	
	//span all cells in rectangle -> appears like a single panel, contained cells' coordinates all point to it
	//rectangle is created, so specified cells are joined
	bool SpanCells(int x0, int y0, int x1, int y1);
	
	//separate cells contained in rectangle (automatically unspans more cells to ensure remaining span is a rectangle)
	//contained widget is moved to most top left cell,
	//bool UnspanCells(int x0, int y0, int x1, int y1);
};


//widget base
class Widget
{
	public:
	Widget *parent; //pointer to parent, needed for event propagation (child to parent)
	
	//rect dimensions and position
	int w,h,x,y;  //size and pin pixel coords (if used)
	int minw, minh; //minimum width and height, limits resize
	bool pinned; //indicates if widget is pinned to x,y coord
	bool disabled; //if set doesn't react to events, greyed out
	
	//tooltip
	//appears left aligned under mouse pointer and travels with it
	//tooltip box tries to be fully visible and may change default position
	string tooltip_str;   //tooltip text, no tooltip if empty
	float tooltip_delay; //time needed to hover over element before tooltip is shown
	
	bool redraw; //indicates, if widget was modified so it has to be redrawn
	
	//order: left, right, top, bottom
	uint16_t padding[4];  //space in pixels to child content
	uint16_t margin[4];   //space in pixels to parent rectangle
	bool anchor[4];      //used to stick widget to 
	uint8_t border[4];    //border type for each side
	
	//SetPadding(int left, int right, int top, int bottom) //use -1 to keep old
	//SetMargin(int left, int right, int top, int bottom) //use -1 to keep old
	
	//set if widget shall center in parent rectangle
	//default is false
	//auto removes conflicting anchors, disables resizing
	//CenterH(bool)
	//CenterV(bool)
	
	//return pointer to child widget
	//@param recursive  if set, 
	//GetWidgetAt(int x, int y, bool recursive = true);
	Widget(): redraw(true) {
		x = y = w = h = 0;
	}
	
	bool intersectRect(const SDL_Rect *r2, SDL_Rect *intersection);   //get intersection rectangle of widget rectangle with another
	
	//draw to destination surface
	virtual void draw(SDL_Surface *s){};

	virtual void resize(int wnew, int hnew){
		w = wnew; h = hnew;
	}

	void setXY(int xnew, int ynew){
		x = xnew; y = ynew;
	}
	
	void setTooltip(string s){
		tooltip_str = s;
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
