#include "font.hpp"

//#include "gui.hpp"

#include "font_gohufont.h"


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



int SDLFont_Base::id_count = 0; //init static var




//load default glyph surface from array
SDL_Surface *load_default_font_surf(){
	SDL_RWops *file = SDL_RWFromMem(gohufont11bmp, sizeof(gohufont11bmp));
	SDL_Surface *s = SDL_LoadBMP_RW(file, 1);
	if(s == NULL) {
		cerr << "Error loading bitmap from address " << gohufont11bmp << endl << SDL_GetError() << endl;
		return NULL;
	}
	//BMP lower than 8 bit is converted to 8 bit with kept indices, while palette is not loaded/copied
	//cout << "bpp surface: " << (int)s->format->BitsPerPixel << endl;
	//SDL_Color *cols = s->format->palette->colors;
	//cout << "color0: " << (int)cols[0].r << "," << (int)cols[0].g << "," << (int)cols[0].b << endl;
	//cout << "color1: " << (int)cols[1].r << "," << (int)cols[1].g << "," << (int)cols[1].b << endl;
	
	//set palette to black & white, libSDL doesn't load the palette from 1 bit images properly
	SDL_Color c[2];
	c[0].r = 0;
	c[0].g = 0;
	c[0].b = 0;
	c[1].r = 255;
	c[1].g = 255;
	c[1].b = 255;
	SDL_SetColors(s, c, 0, 2);
	return s;
}


//create 1 bpp surface
//SDL_LoadBMP() doesn't return surfaces with lower than 8 bpp, also for lower bpp BMPs it doesn't load the palette correctly and returns them converted to 8 bpp surfaces
//we can manually correct the palette colors
//SDL_CreateRGBSurface() can create 1 bpp surfaces
//  most left pixel is MSB of byte
//  we cannot blit to it and SDL_SaveBMP cannot store them
//  we can blit from it, but for SDL_Rect 1 in x or w counts as 8 pixels
///@param c0 - can be NULL, then color becomes black
SDL_Surface *CreateSurface1BPP(int w, int h, SDL_Color *c0, SDL_Color *c1){
	SDL_Surface *s = SDL_CreateRGBSurface(0, w, h, 1, RMASK, GMASK, BMASK, 0);
	if(s == NULL)
		return NULL;
	
	//set palette
	SDL_Color c[2] = {{0,0,0,0},{0,0,0,0}};
	if(c0 != NULL)
		c[0] = *c0;
	if(c1 != NULL)
		c[1] = *c1;
	//SDL_SetPalette(s, SDL_LOGPAL | SDL_PHYSPAL, c, 0, 2);
	SDL_SetColors(s, c, 0, 2);
	return s;
}


///create 1 bpp surface from input surface
///copy from 8 bit to 1 bit surface
///@param rin  rectangle to copy from
SDL_Surface *CreateSurface1BPPCopy(SDL_Color *c0, SDL_Color *c1, SDL_Surface *sin, SDL_Rect *rin){
	if(sin == NULL)
		return NULL;
	SDL_Rect r = {0,0,sin->w,sin->h};
	if(rin != NULL)
		r = *rin;
	
	SDL_Surface *sout = CreateSurface1BPP(r.w, r.h, c0, c1);
	if(sout == NULL)
		return NULL;
	
	//make r safe for pixel access
	if(r.x < 0){
		r.w += r.x; //reduce
		r.x = 0;
	}
	if(r.y < 0){
		r.h += r.y; //reduce
		r.y = 0;
	}
	if(r.x + r.w > sin->w)
		r.w -= sin->w - (r.x + r.w);
	if(r.y + r.h > sin->h)
		r.h -= sin->h - (r.y + r.h);
	if(r.w < 0  ||  r.h < 0){
		r.w = 0;
		r.h = 0;
	}
	
	
	SDL_LockSurface(sout);
	SDL_LockSurface(sin);
	
	//pixels conversion
	//we map by color brightness (simple average)
	SDL_Color *dc = sout->format->palette->colors; //ptr to dst colors
	int da[2] = {dc[0].r + dc[0].g + dc[0].b, dc[1].r + dc[1].g + dc[1].b}; //dst color average
	if(sin->format->BitsPerPixel == 8){
		uint8_t lut[256];    //lookup table to map src indices
		
		SDL_Color *sc = sin->format->palette->colors; //ptr to dst colors
		
		//setup LUT
		for(int i = 0; i < sin->format->palette->ncolors; i++){
			int sa = sc[i].r + sc[i].g + sc[i].b;
			if(abs(sa-da[0]) < abs(sa-da[1]))
				lut[i] = 0;
			else
				lut[i] = 1;
		}
		//blit 8bpp to 1bpp
		int wbytes = (r.w + 7) / 8;
		for(int y = 0; y < r.h; y++){
			uint8_t *pd = (uint8_t *)sout->pixels + (y * sout->pitch);
			uint8_t *ps = (uint8_t *)sin->pixels + r.x + ((y + r.y) * sin->pitch);
			
			for(int x = 0; x < wbytes; x++){
				*pd =
					(lut[ps[0]] << 7) |
					(lut[ps[1]] << 6) |
					(lut[ps[2]] << 5) |
					(lut[ps[3]] << 4) |
					(lut[ps[4]] << 3) |
					(lut[ps[5]] << 2) |
					(lut[ps[6]] << 1) |
					(lut[ps[7]]);
				pd += 1;
				ps += 8;
			}
		}
	}
	//else if (sin->format->BitsPerPixel > 8){
	//	//loop through pixel values, map by color brightness: r+g+b
	//}
	else {
		//todo
		cerr << "Error, bpp !=8 to 1 conversion not implemented"<< endl;
	}
	
	SDL_UnlockSurface(sout);
	SDL_UnlockSurface(sin);
	
	return sout;
}








SDLFontGlyph::SDLFontGlyph(){
	r.x = 0;
	r.y = 0;
	r.w = 0;
	r.h = 0;
	s = NULL;
	delsurf = true;
	baseline = 0;
	advance_h = 0;
	advance_v = 0;
	lsb = 0;
}

SDLFontGlyph::~SDLFontGlyph(){
	if(delsurf  &&  s != NULL)
		delete s;
}

void SDLFontGlyph::Blit(SDL_Surface* dst, SDL_Rect* dstrect, int x, int y){
	if(s == NULL || dst == NULL)
		return;
		
	//dst clipping rectangle
	SDL_Rect rd; 
	if(dstrect != NULL)
		rd = *dstrect;
	else {
		rd.x = 0;
		rd.y = 0;
		rd.w = dst->w;
		rd.h = dst->h;
	}
	//rect for SDL_BlitSurface() to blit glyph in dst
	SDL_Rect rdg = {rd.x + x + lsb, rd.y + y - baseline, s->w, s->h};
	
	//clip
	if(
		(rdg.x + rdg.w <= rd.x) ||
		(rdg.x >= rd.x + rd.w)  ||
		(rdg.y + rdg.h <= rd.y) ||
		(rdg.y >= rd.y + rd.h)
		)
		return;
	
	SDL_Rect rb; //backup clipping rectrect
	SDL_GetClipRect(dst, &rb);
	SDL_SetClipRect(dst, &rd);
	//SDL only blits from full bytes, so if 1 bit surface shall be blitted to x<0, SDL doesn't do
	// -> as workaround we blit fully to a temporary 8 bit surface
	if(s->format->BitsPerPixel < 8  &&  rdg.x < rd.x) {
		SDL_Surface* stemp = SDL_CreateRGBSurface(0, s->w, s->h, dst->format->BitsPerPixel, dst->format->Rmask, dst->format->Gmask, dst->format->Bmask, 0);
		if(stemp == NULL)
			return;
		//copy palette
		if(stemp->format->BitsPerPixel <= 8)
			SDL_SetColors(stemp, s->format->palette->colors, 0, 2);
		SDL_BlitSurface(s, NULL, stemp, NULL); //blit 1bpp to temp 8bpp
		SDL_SetColorKey(stemp, SDL_SRCCOLORKEY, SDL_MapRGB(stemp->format, 0, 0, 0));
		SDL_BlitSurface(stemp, &r, dst, &rdg); //blit temp to dst
		SDL_FreeSurface(stemp);
	}
	else
		SDL_BlitSurface(s, &r, dst, &rdg);
	SDL_SetClipRect(dst, &rb); //restore previous clipping rect
}

void SDLFontGlyph::SetColor(SDL_Color *c){
	if(c != NULL  &&  s != NULL){
		SDL_SetColors(s, c, 1, 1);
	}
	//for surfaces with more than 2 colors we need to create a temp surface and multiply (shade) colors
	//if color key is active, this color must be left untouched
	//-> counts for true color and 8 bit images with more than 1 non colorkey color
	//-> we could parse pixel data at "this->set surface"
}

void SDLFontGlyph::SetSurface(SDL_Surface *sin, SDL_Rect *rin, bool delsurf_){
	if(sin != NULL) {
		if(delsurf && s != NULL)
			SDL_FreeSurface(s);
		s = sin;
		delsurf = delsurf_;
	}
	if(rin != NULL)
		r = *rin;
}




SDLFont_Base::SDLFont_Base(string name_) : id(id_count){
	SetName(name_);
	if(name == ""){
		name = "Unnamed_";
		name += this->id_count;
	}
	
	id_count++; //for the next instance
	col[0] = 0xff;
	col[1] = 0xff;
	col[2] = 0xff;
	vax = 1;
	vay = 0;
};

void SDLFont_Base::SetColor(uint8_t r, uint8_t g, uint8_t b){
	col[0] = r;
	col[1] = g;
	col[2] = b;
}

void SDLFont_Base::SetColorF(float r, float g, float b){
	float c[3] = {r,g,b};
	for(int i = 0; i < 3; i++){
		if(c[i] < 0)
			c[i] = 0;
		else if(c[i] > 1)
			c[i] = 1;
		this->col[i] = (uint8_t)(c[i] * 255);
	}
}

void SDLFont_Base::TextWrite(SDL_Surface *dst, SDL_Rect *r, int x, int y, string s){
	if(dst == NULL)
		return;
		
	int len = s.length();
	std::map<uint32_t,SDLFontGlyph>::iterator it;
	SDLFontGlyph *g;
	
	for(int i = 0; i < len; i++){
		it = glyphs.find((uint32_t)s[i]);
		if(it == glyphs.end()) //skip undefined glyphs (replace?)
			continue;
		g = &(it->second);
		//set to current color
		if(g->s != NULL){
			SDL_Color c = {col[0], col[1], col[2], 0};
			g->SetColor(&c);
		}
		g->Blit(dst, r, x, y);
		x += g->advance_h * this->vax;
		y += g->advance_v * this->vay;
	}
}

void SDLFont_Base::SetName(string s){
	name = s;
}



SDLFontSingleSurface::SDLFontSingleSurface(string name) : SDLFont_Base(name){
	s = NULL;
}

SDLFontSingleSurface::~SDLFontSingleSurface(){
	if(s != NULL)
		SDL_FreeSurface(s);
}

void SDLFontSingleSurface::SetSurface(SDL_Surface *snew){
	if(s != NULL)
		SDL_FreeSurface(s);
	s = snew;
}

bool SDLFontSingleSurface::SetGlyph(uint32_t code_point, SDL_Rect *r, int baseline){
	if(r == NULL)
		return false;
	SDLFontGlyph g;
	g.s = s;
	g.delsurf = false;
	g.r = *r;
	g.advance_h = r->w;
	g.baseline = baseline;
	return SetGlyph(code_point, &g);
}

bool SDLFontSingleSurface::SetGlyph(uint32_t code_point, SDLFontGlyph *g){
	if(g == NULL)
		return false;
	glyphs[code_point] = *g;
	return true;
}

void SDLFontSingleSurface::DefaultFont(){
	glyphs.clear();
	
	SDL_Surface *sin = load_default_font_surf();
	if(sin == NULL)
		return;
	SetSurface(s);
	SetName("Gohufont 11");
	//surface should have 8 bpp
	
	SDLFontGlyph g;
	g.s = sin; //will be freed by destructor when leaving local scope
	g.delsurf = false;
	g.advance_h = GOHUFONT11_W;
	g.advance_v = sin->h;
	g.baseline = GOHUFONT11_B;
	
	for(int i = 0; i < sin->w / GOHUFONT11_W; i++) {
		g.r.x = i * GOHUFONT11_W;
		g.r.y = 0;
		g.r.w = GOHUFONT11_W;
		g.r.h = GOHUFONT11_H;
		SetGlyph(i, &g);
	}
}





SDLFont::SDLFont(string name) : SDLFont_Base(name){
}



bool SDLFont::SetGlyph(uint32_t code_point, SDLFontGlyph *g){
	if(g == NULL || g->s == NULL)
		return false;
	
	//Todo: optimize g->r (check foreground and alpha channel to minimize rect and stored surface dim)
	
	SDL_Color c[2] = {{0,0,0,0},{255,255,255,0}};  //black & white palette
	SDL_Surface *s = CreateSurface1BPPCopy(&c[0], &c[1], g->s, &(g->r));
	
	//SDL_Surface *s = SDL_CreateRGBSurface(0, g->s->w, g->s->h, 1, RMASK, GMASK, BMASK, AMASK);
	if(s == NULL) {
		cerr << "CreateRGBSurface failed: " << SDL_GetError() << endl;
		return false;
	}

	SDL_SetColorKey(s, SDL_SRCCOLORKEY, SDL_MapRGB(s->format, 0, 0, 0));
	glyphs.erase(code_point);  //remove old
	SDLFontGlyph *gnew = &(glyphs[code_point]); //create new
	
	gnew->baseline = g->baseline;
	gnew->advance_h = g->advance_h;
	gnew->advance_v = g->advance_v;
	gnew->lsb = g->lsb;
	
	gnew->r.w = g->r.w;
	gnew->r.h = g->r.h;
	gnew->s = s;
	
	return true;
}

void SDLFont::DefaultFont(){
	glyphs.clear();
	
	SDL_Surface *sin = load_default_font_surf();
	if(sin == NULL)
		return;
	SetName("Gohufont 11");
	//surface should have 8 bpp
	
	SDLFontGlyph g;
	g.s = sin; //will be freed by destructor when leaving local scope
	g.advance_h = GOHUFONT11_W;
	g.advance_v = sin->h;
	g.baseline = GOHUFONT11_B;
	
	for(int i = 0; i < sin->w / GOHUFONT11_W; i++) {
		g.r.x = i * GOHUFONT11_W;
		g.r.y = 0;
		g.r.w = GOHUFONT11_W;
		g.r.h = GOHUFONT11_H;
		SetGlyph(i, &g);
	}
}