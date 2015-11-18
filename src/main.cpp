using namespace std;

#include "player/player.hpp"



/*
Thread 0 (needed to avoid freezing window under MS Windows)
 init SDLVIDEO / Set vide mode
 set event filter
 start player thread
 while player thread running
   loop pumpevents()
 unset event filter
 sdl_quit

Thread 1 - player (core)
 start GUI thread (since the GUI handles all events and drawing it runs permanently in another thread polling for events)
 load requested video file
 
 send and receive signal from/to GUI (with mutex)
 
 while playing
  //each CPU intensive step should be done parallely in a different thread if possible (assume software rendering)
  Thread A: fetch next-next frame (decode to library supported output format: 8,24,32 bit)
  Thread B: resize next frame (resize before converting to display format to avoid handling of all display supported chunk sizes/formats -> simplyfies resize filters)
  signal gui thread:
    convert to display format current frame by blitting to screen
  // => we need to read ahead 2 frames each step to make use of all threads
*/


/*

main  ->  player(setup gui and call gui::runapp) -> gui(callback upon event)
                                                     |
                                          window and event thread

*/




#include <SDL/SDL.h>
//prevent SDL from output of stdout/stderr to file (Windows only) -> normal console output
#if defined _WIN32 || defined WIN32  //defined for both 32-bit and 64-bit environments
 #ifdef main
  #undef main
 #endif
#endif 







int main(int argc, char *argv[])
{





	 //init SDL
/* if(SDL_Init(SDL_INIT_VIDEO) < 0) {
    cout << "Unable to initialize SDL: " << SDL_GetError() << endl;
    return -1;
 }*/



/*
	SDL_Thread *appThread = SDL_CreateThread( &runAppThread, NULL);

 SDL_Delay(100);

//	SDL_Surface *screen = SDL_SetVideoMode(10, 10, 0, SDL_SWSURFACE | SDL_RESIZABLE);
//	if(screen == NULL) {
//		cerr << "unable to open SDL video surface: " << SDL_GetError() << endl;
//	}



 //poll events, empty event queue
 SDL_Event event;
 while(1)
 {
   if(SDL_PollEvent(&event))
	switch(event.type)
	{

		default:
			cout << "event occurence thread 1 " <<(int)event.type << endl;
			break;
	}
	
	SDL_Delay(4);

}


 SDL_Delay(20000);
return 0;

*/

 //create player instance
 AnimPlayer animPlayer;
 
 
 //cout << "hello" << endl;
 //cout << &animPlayer << endl;
 //cout << &AnimPlayer::run << endl;
 
/*
 int a;
 char b;
 SDLGui_::CallbackTemplate<AnimPlayer> t;
 t.PrintType();
 
 SDLGui_::EventHandler eh;
 eh.BindEvent(123, main);
*/

 //run the player
 return animPlayer.init(argc, (const char**)argv);

 //animPlayer.run();
}




