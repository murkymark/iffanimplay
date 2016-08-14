/*
 This file contains all code for software and hardware surface support
 
 http://www.libsdl.org/release/SDL-1.2.15/docs/html/guidevideoopengl.html
 
 SDL_GL_SwapBuffers( );
*/


#include <SDL/SDL.h>
#include <GL/gl.h>
#include <GL/glu.h>



//can contain software surface and hardware texture reference
//it is possible to use only a small rect of a bigger surface
class GUI_Surface {
	public:
	SDL_Surface *sw; //software surface pointer
	
	GLuint hw; //hardware texture ID
//	SDL_Rect hw_rect;

	int w, h;

	//ownership - determines if surface must or must not be freed by this instance
	bool sw_own;
	bool hw_own;



	GUI_Surface(){
		init();
	}


	~GUI_Surface(){
		if(sw_own)
			freesw();
		if(hw_own)(){
			freehw();
		}
	}




	void SetRect(SDL_Rect r){
		
	}
	
	//set references to an existing image, without ownership
	SetSW(SDL_Surface *s, SDL_Rect r){
	}
	
	//create a new SW surface with ownership from another one
	CreateSWFrom(SDL_Surface *s, SDL_Displayformat, SDL_Rect)


	//create texture from software surface
	//activates ownership for HW
	void sw2hw(){
		if(hw != 0)
			freeHW();
		
		if(sw == NULL)
			return;
		
		glGenTextures(1, &hw);
		glBindTexture(GL_TEXTURE_2D, TextureID);

		//get mode and byte order from SW
		int Mode = GL_RGB;

		if(Surface->format->BytesPerPixel == 4) {
			Mode = GL_RGBA;
		}

		glTexImage2D(GL_TEXTURE_2D, 0, Mode, Surface->w, Surface->h, 0, Mode, GL_UNSIGNED_BYTE, Surface->pixels);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		
		hw_own = true;
	}


	void SetOwnershipSW(bool os){
		sw_own = os;
	}
	
	void SetOwnershipHW(bool os){
		hw_own = os;
	}


	void freeSW(){
		if(sw != NULL)
			SDL_FreeSurface(sw);
		sw = NULL;
	}
	
	
	void freeHW(){
		if(hw != 0)
			glDeleteTextures(1, &hw);
		hw = 0;
	}
	
	
	
	protected:
	
	
	
	Init(){
		sw = NULL;
		hw = 0
		
		sw_own = false;
		hw_own = false;
	}
};





/* Information about the current video settings. */
const SDL_VideoInfo* info = NULL;

    /* Dimensions of our window. */
    int width = 0;
    int height = 0;
    /* Color depth in bits of our window. */
    int bpp = 0;
    /* Flags we will pass into SDL_SetVideoMode. */
    int flags = 0;

    /* First, initialize SDL's video subsystem. */
    if( SDL_Init( SDL_INIT_VIDEO ) < 0 ) {
        /* Failed, exit. */
        fprintf( stderr, "Video initialization failed: %s\n",
             SDL_GetError( ) );
        quit_tutorial( 1 );
    }

    /* Let's get some video information. */
    info = SDL_GetVideoInfo( );

    if( !info ) {
        /* This should probably never happen. */
        fprintf( stderr, "Video query failed: %s\n",
             SDL_GetError( ) );
        quit_tutorial( 1 );
    }

    /*
     * Set our width/height to 640/480 (you would
     * of course let the user decide this in a normal
     * app). We get the bpp we will request from
     * the display. On X11, VidMode can't change
     * resolution, so this is probably being overly
     * safe. Under Win32, ChangeDisplaySettings
     * can change the bpp.
     */
    width = 640;
    height = 480;
    bpp = info->vfmt->BitsPerPixel;

    /*
     *
     * The last thing we do is request a double
     * buffered window. '1' turns on double
     * buffering, '0' turns it off.
     *
     * Note that we do not use SDL_DOUBLEBUF in
     * the flags to SDL_SetVideoMode. That does
     * not affect the GL attribute state, only
     * the standard 2D blitting setup.
     */
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, bpp );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

    /*
     * We want to request that SDL provide us
     * with an OpenGL window, in a fullscreen
     * video mode.
     *
     * EXERCISE:
     * Make starting windowed an option, and
     * handle the resize events properly with
     * glViewport.
     */
    flags = SDL_OPENGL;

    /*
     * Set the video mode
     */
    if( SDL_SetVideoMode( width, height, bpp, flags ) == 0 ) {
        /* 
         * This could happen for a variety of reasons,
         * including DISPLAY not being set, the specified
         * resolution not being available, etc.
         */
        fprintf( stderr, "Video mode set failed: %s\n",
             SDL_GetError( ) );
        quit_tutorial( 1 );
    }
    
    
    