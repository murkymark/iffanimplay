#include "gui_widgets.hpp"





void Panel::prepareSurface(){
	if(surf != NULL  &&  (r.w != surf->w  ||  r.h != surf->h)){
		SDL_FreeSurface(surf);
		surf = NULL;
	}
	if(surf == NULL)
		surf = guiPtr->createRectSurf(r.w, r.h, COLOR_PLAIN());

	if(surfMerged != NULL  &&  (r.w != surfMerged->w  ||  r.h != surfMerged->h)){
		SDL_FreeSurface(surfMerged);
		surfMerged = NULL;
	}
	if(surfMerged == NULL)
		surfMerged = guiPtr->createRectSurf(r.w, r.h, 0);
}
	

//todo draw only rectangle
void Panel::draw(SDL_Surface *dst){
	prepareSurface();
	guiPtr->blit(surf, surfMerged, 0, 0);
		
	list<Widget>::iterator i;
	for ( i = wlist.begin(); i != wlist.end(); i++ )
		(*i).draw(surfMerged);
	guiPtr->blit(surfMerged, dst, 0, 0);
}
	
	

bool Widget::intersectRect(const SDL_Rect *r2, SDL_Rect *intersection)
{
	int Amin, Amax, Bmin, Bmax;

	// Horizontal intersection
	Amin = r.x;
	Amax = Amin + r.w;
	Bmin = r2->x;
	Bmax = Bmin + r2->w;
	if(Bmin > Amin)
		Amin = Bmin; //most right of left edges -> left intersection edge
	if(Bmax < Amax)
		Amax = Bmax; //most left of right edges -> right intersection edge
	if(Amax - Amin > 0) {
		if(intersection != NULL) {
			intersection->x = Amin;
			intersection->w = Amax - Amin;
		}

		// Vertical intersection
		Amin = r.y;
		Amax = Amin + r.h;
		Bmin = r2->y;
		Bmax = Bmin + r2->h;
		if(Bmin > Amin)
			Amin = Bmin;
		if(Bmax < Amax)
			Amax = Bmax;
		if(Amax - Amin > 0){
			if(intersection != NULL) {
				intersection->y = Amin;
				intersection->h = Amax - Amin;
			}
			return true;
			
		}
	}
	return false;
}



void Slider::setImages(const char *path0, const char *path1){
	thumb = guiPtr->loadIcon(path0);
	thumbGhost = guiPtr->loadIcon(path1);

//cout << (int)thumb << endl;
//cout << (int)thumbGhost << endl;
	//SDL_SetAlpha(thumb, 0, 0xff);
	//SDL_SetAlpha(thumbGhost, 0, 0xff);
}



void Slider::draw(SDL_Surface *dst, int x, int y){
	int w = 40;
	SDL_Rect r = {x-20, y+11, w, 3};
	guiPtr->draw3DBorder(dst, false, &r);
	
	int lenTotal = w-2;
	int len = max - min;
	if(len == 0) len = 1;
	len = pos * (lenTotal)/ len;
	guiPtr->lineH(dst, r.x+1, r.y+1, len, COLOR_BLUE_LIGHT());
	guiPtr->lineH(dst, r.x+1, r.y+1, lenTotal - len, COLOR_BLACK());
	
	guiPtr->blit(thumb, dst, x, y);
	guiPtr->blit(thumbGhost, dst, x, y+16);
}



void Button::setImages(const char *path0, const char *path1){
	//unpressed and pressed button image
	SDL_Surface *icon = guiPtr->loadIcon(path0);
	buttonSurf[0] = guiPtr->createRectSurf(icon->w, icon->h, guiPtr->buttonColor());
	SDL_BlitSurface(icon, NULL, buttonSurf[0], NULL);
	guiPtr->draw3DBorder(buttonSurf[0], false, NULL);
	SDL_SetAlpha(buttonSurf[0], 0, 0xff);

	if(path1 != NULL) {
		SDL_FreeSurface(icon);
		icon = guiPtr->loadIcon(path1);
	}
	buttonSurf[1] = guiPtr->createRectSurf(icon->w, icon->h, guiPtr->buttonColor());
	SDL_BlitSurface(icon, NULL, buttonSurf[1], NULL);
	guiPtr->draw3DBorder(buttonSurf[1], true, NULL);
	SDL_SetAlpha(buttonSurf[1], 0, 0xff);

	SDL_FreeSurface(icon);
}

void Button::draw(SDL_Surface *dst){
	int i = 0;
	if(pressed) i = 1;
	guiPtr->blit(buttonSurf[i], dst, r.x, r.y);
}

void Button::draw(SDL_Surface *dst, int x, int y){
	guiPtr->blit(buttonSurf[0], dst, x, y);
	guiPtr->blit(buttonSurf[1], dst, x, y+16);
}
