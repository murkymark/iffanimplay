//font related stuff

#ifndef _gui_font_H_
#define _gui_font_H_


#include <stdint.h> //_t types
#include <map>
#include <string>
#include <iostream>

#include <SDL/SDL.h>

//#include "unistring.hpp"



using namespace std;





//single glyph
//a bitmap of a glyph of a specific size
//typically s is a 1bpp surface, this is memory friendly and makes coloring easy
// - libSDL has issues with 1bpp surfaces
// - on Windows we can work around them, hopefully this works on other systems too
class SDLFontGlyph {
	public:
	SDL_Rect r; //glyph rectangle in surface - w, h is glyph bitmap dimension
	SDL_Surface *s; //not owning, just a reference
	bool delsurf;  //indicates if surface must be deleted upon destruction

	uint16_t baseline;  //baseline in rect in pixels (y offset in Latin, x offset in Chinese)
	uint16_t advance_h; //horizontal advance of "pen" (Latin, ...)
	uint16_t advance_v; //vertical advance of "pen" (Chinese, ...)
	uint16_t lsb;       //left site bearing, bearingX (usually 0, but can be negative) -> origin + lsb => blit x position
	
	SDLFontGlyph();
	~SDLFontGlyph();
	///blit glyph to dst (dst must have bpp >= 8)
	///@param dst      destination surface
	///@param dstrect  only draw within rectangle (no pixel is changed outside)
	///@param x,y      baseline origin pixel coord in dstrect
	void Blit(SDL_Surface* dst, SDL_Rect* dstrect, int x, int y);
	//set color (must be done when drawing text, before each blit)
	void SetColor(SDL_Color *c);
	///set surface and rect (old surface will be deleted if delsurf indicates)
	///@param sin  if NULL old surface will remain
	///@param rin  if NULL old rect will remain
	///@param delsurf  indicates if new surface must be freed, ignored if new surface is NULL
	void SetSurface(SDL_Surface *sin, SDL_Rect *rin, bool delsurf_ = true);
	//set glyph metrics
	//void SetMetrics(...);
};






class SDLFont_Base {
	public:
	
	map<uint32_t,SDLFontGlyph> glyphs;
	std::string name;
	uint8_t col[3]; //rgb color to write next text

	int const id;        //unique reference id number
	static int id_count; //unique id for each created object
	float vax, vay;      //advance vector, default: (1,0)


	SDLFont_Base(string name_ = "");

	///set text color
	void SetColor(uint8_t r, uint8_t g, uint8_t b);
	//params expected to be in range 0..1
	void SetColorF(float r, float g, float b);
	
	void SetName(string s);
	
	//set advance vector, default is (1,0) for Latin like text
	//vector is a factor, advance in pixels depend on glyph attributes
	void SetAdvance(float x, float y);
	
		//set encoding (at the moment only sets the value so you can distinguish later)
	void SetEncoding(int e);
	//get encoding
	int GetEncoding();
	
	///get dimension in pixels the blitted text would need, but doesn't blit
	///w,h  output, can be NULL
	void GetTextDim(string s, int *w, int *h);
	
	//returns only that string part, that fits into the w*h rectangle
	//@return string that fits into dimension
	string GetTextFit(string s, int w, int h);
	
		///blit glyphs to target surface
	///'\n' or '\t' have no formatting effect and count as code point
	///-> text formatting must be done beforehand
	///@param[out] dst  dst surface
	///@param[in]  x, y baseline origin in dst r where first glyph is drawn
	///@param[in]  r    limit drawing to rectangle in dst
	///@param[in]  s    text to write
	void TextWrite(SDL_Surface *dst, SDL_Rect *r, int x, int y, string s);
	
	void TextWrite(SDL_Surface *dst, SDL_Rect *r, int x, int y, string& s);
	
	//void TextWrite(SDL_Surface *dst, SDL_Rect *r, int x, int y, unistring& s);
	
	//write UTF-32 string
	void TextWrite(SDL_Surface *dst, SDL_Rect *r, int x, int y, uint32_t *str, size_t len);
	
	virtual bool SetGlyph(uint32_t code_point, SDLFontGlyph *g) = 0;
	
	
	virtual void DefaultFont() = 0;
	
	/*****************************************/
	
	//Line height
	
	//max glyph height in pixels
	//removing or replacing glyphs can make max height unknown (attributes are set to -1 then)
	// -> call MaxHeightKnown() to check status, and then DetectMaxHeight() if needed
	protected:
	int max_h0; //max height - baseline to top (including baseline)
	int max_h1; //max height - baseline to bottom
	uint32_t maxh0_glyph; //max_h0 related glyph code point
	uint32_t maxh2_glyph; //max_h0 related glyph code point 

	//detect max height if unknown and not forced
	void DetectMaxHeight();

	public:
	//set and force line height, disables auto detection
	void SetLineHeight(int h0, int h1);
	//get forced or auto line height (may call DetectMaxHeight() if unknown)
	void GetLineHeight(int *h0, int h1);
	//set height automatically, adding or removing glyphs can change it (may call DetectMaxHeight() if unknown)
	void SetLineHeightAuto();
	//check if max height is known or forced
	bool isMaxHeightKnown();
	//check if line height is forced
	bool isHeightForced();
	
};





//simple font variant
//- uses exactly one surface for all glyphs, not one for each
//- no 1 bpp support
//- faster, suited for small character sets
class SDLFontSingleSurface : public SDLFont_Base {
	public:
	SDL_Surface *s;
	
	SDLFontSingleSurface(string name_ = "");
	~SDLFontSingleSurface();
	
	void SetSurface(SDL_Surface *s);
	bool SetGlyph(uint32_t code_point, SDL_Rect *r, int baseline);
	virtual bool SetGlyph(uint32_t code_point, SDLFontGlyph *g);
	
	//load default font
	virtual void DefaultFont();
};




//font
//(individual, not only monospace)
//Hint: It's a good idea to render text only once to a surface in display format for better speed
class SDLFont : public SDLFont_Base {
	public:
	
	SDLFont(string name_ = "");
	
	//add or replace single glyph
	//creates internal 1 bpp surface copy
	//important for coloring: white is foreground, black is background color (other colors are mapped to black or white)
	virtual bool SetGlyph(uint32_t code_point, SDLFontGlyph *g);

	//add glyph and store surface exactly as input surface
	//- coloring doesn't work yet (shall work like in OpenGL: only white can become any color)
	bool SetGlyphAnyBPP(uint32_t code_point, SDLFontGlyph *g);
	
	//set callback function, called when glyph is not set
	//to dynamically load glyphs on demand
	void setCallback(void (*cb)(SDLFont*, uint32_t, void*), void *user);
	
	//load default font, all previous loaded glyphs are removed
	virtual void DefaultFont();
};





//controls formatting of text
//newline, tab
//text processing
class SDLFontTextCtrl {
	//left, right-aligned, block (enlarged advance)
	//set box - wrappin box
	//word wrap, inword wrap (needed if word is longer than full line)
	//newline (CRLF (Windows), CR (0D - MacOs old), LF (0A - Unix))
	//line spacing (1 px by default)
	
	public:
	void ProfileDummy(); //single text line (default)
	void ProfileTextBox(); //rectangular text box
	void ProfileTextBoxTB(); //top to bottom advance
};

#endif
