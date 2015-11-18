//Event binding list:
//Events are handled via callback functions/methods
//Function pointers of callback functions are dynamically bound and stored in an internal event binding list
//Each GUI element derived from EventHandler has such a list
//
// 
//Callbacks:
//They run in the same thread as the GUI.
//Shall return as fast as possible because they block the GUI and delay following events.
//If something time consuming is to be started by a callback, better do it in another thread.
//  -> use signal event to trigger callback in GUI thread right before thread end.
//     The callback should wait then until the thread has really ended before freeing it's resources


#ifndef _gui_event_H_
#define _gui_event_H_



#include <iostream>
#include <string>
#include <map>

#include <SDL/SDL.h>

//#include "any_class_fp.hpp"



#define DEBUG_GUI_EVENT 1  //set to 0 for disabled RTTI or if not needed

#if DEBUG_GUI_EVENT == 1
 #include <typeinfo>
#endif



//access with scope operator
namespace SDLGui_
{

//event type IDs (uint8_t): standart SDL events, no need to redefine
/*
SDL_ACTIVEEVENT
SDL_KEYDOWN
SDL_KEYUP
SDL_MOUSEMOTION
SDL_MOUSEBUTTONDOWN
SDL_MOUSEBUTTONUP
SDL_JOYAXISMOTION
SDL_JOYBALLMOTION
SDL_JOYHATMOTION
SDL_JOYBUTTONDOWN
SDL_JOYBUTTONUP
SDL_QUIT
SDL_SYSWMEVENT
SDL_VIDEORESIZE
SDL_VIDEOEXPOSE
SDL_USEREVENT
*/

//additional GUI events, mapped to the SDL event type space in the range: "SDL_USEREVENT to SDL_NUMEVENTS-1"
// we ignore the limited SDL_NUMEVENTS (defined as "SDL_NUMEVENTS = 32") and consider the highest value as 0xFF
enum EventType {
	EV_BUTTON_DOWN = (SDL_USEREVENT+1), //if a button (GUI element) is pressed down once
	EV_BUTTON_UP,
	EV_BUTTON_BUTTON_CLICK, //if a button (GUI element) is clicked: pressed with mouse focus and released with mouse focus (mouse can leave focus in between, but not when releasing)
	EV_POINTER_ENTER,       //pointer (mouse) enters element focus (rectangle area)
	EV_POINTER_LEAVE,       //pointer (mouse) leaves element focus (rectangle area)
	EV_TIMER,               //callback after specified time has passed
	EV_SIGNAL               //signal event can be pushed onto event stack by another thread, the event just consists of an int representing an ID (use preprocessor to define your unique IDs)
};


/*
//event struct for each type
typedef struct {} EventButtonClick;
typedef struct {} EventTimer;
typedef struct {} EventSignal;

//put to union
Event
*/







//##############################################################################
//base class for event callback entries
//callback functions are stored as baseclass
class CallbackBase {
	public:

	//call method of derived class
	virtual void Callback() = 0; 
	virtual ~CallbackBase(){}; //virtual destructor, so that derived class' destructor is also called by base destructor

	//create and return new class instance
	//param:
	//static factory(...);

	virtual void Print() = 0;

#if DEBUG_GUI_EVENT == 1
	//print class type of function pointer
	virtual void PrintType() = 0;
#endif
};


//##############################################################################
//class template for different classes
template<typename CLASS>
class CallbackTemplate : CallbackBase  {
	public:
	
	//callback function pointer
	void (CLASS::*pt2Member)(SDL_Event &e, char, void *userdata);
	void *userdata; //passed to callback function


	
	void CallbackSpecific() {
		//pt2Member = NULL;
	}
	
	virtual void Callback() {
		this->CallbackSpecific();
	}
	
	virtual ~CallbackTemplate(){};
	
	virtual void Print(){
		std::cout << pt2Member << std::endl;
	}
	
	
#if DEBUG_GUI_EVENT == 1
	//print class type of function pointer
	virtual void PrintType(){
		std::cout << typeid(CLASS).name() << std::endl;
	}
#endif
};










//##############################################################################
//event class
//passed to callback functions
//before accessing the struct make sure it is the expected type, distinguish between SDL events and GUI event
class Event {
	union {
	 SDL_Event sdl;
	 //SDLGui_Event gui;
	};

	int GetEventType ();

	//return user data pointer, specified when event was bound
	void *GetUserData();
};


//##############################################################################

//event binding list:

//
/*
class EventBindingList {
	public:
	

	

	

};
*/

//##############################################################################
//stores event binding list with callbacks
class EventHandler {
	public:

	//event binding list
	//if another binding for that event already exists it is replaced
	//
	//we use a map of maps for fast access (actual a map of maps: map< type, map<binding_id, callback> >)
	// binding_id : highest byte is type, 3 lower bytes are generated by counter
	// we could also use a lookup table but that would use up at least: 256*sizeof(void*)
	// map as pointer to save memory (because this class is a base class of all widgets)
	std::map< uint8_t, std::map<uint32_t, CallbackBase*> > *bindmap;


	EventHandler() {
		bindmap = NULL;
	}
	
	~EventHandler() {
		if (bindmap != NULL)
			delete bindmap;
	}

	//create and add callback entry to list of event callback functions
	//template argument deduction -> no need to call with <type>
	//pass method:   gui_element.Bind(SDLGUI_BUTTON_CLICK, &MyObject.)
	//pass function: gui_element.Bind(int )
	template<typename T>
	void BindEvent(int event_type, T f, void* userdata = NULL){
		if (bindmap == NULL)
			bindmap = new std::map< uint8_t, std::map<uint32_t, CallbackBase*> >;
		//get type specific map
		std::map<uint8_t,std::map<uint32_t, CallbackBase*> >::iterator it = bindmap->find(event_type);
		//insert callback
		((*it).second).insert( std::pair<uint32_t, CallbackBase*>(event_type, (CallbackBase*)100) );
		
		CallbackTemplate<T> testobj;
		std::cout << typeid(T).name() << std::endl;
	}

	//unbind function from event binding list
	//Unbind()
	//UnbindPropagated() //unbind also in child elements

	//remove all callback methods of this object (instance, not class type) from the event binding list
	// useful before deleting an object with potentially still bound callback methods (you can call this from the objects'destructor)
	void UnbindObject(void *optr){
	}

	//search
	bool IsBound(){
		//...
		return false;
	}
	
	

	//check if binding exists
	bool BindingExists(uint32_t key);
	
	//return new line separated list with all set bindings (for debugging)
	std::string Bindings() {
		if (bindmap == NULL)
			return "";
		
		std::map<uint8_t,std::map<uint32_t, CallbackBase*> >::iterator it = bindmap->begin();
		std::map<uint32_t, CallbackBase*>::iterator it_inner;
		
		while (it != bindmap->end()) {
			it_inner = ((*it).second).begin();  //get start of event type specific map
			std::cout << "Type: " << ((*it).first) << std::endl;
			while (it_inner != ((*it).second).end()) {
				//std::cout << " " << ((*it_inner).second)->Print() << std::endl;
				((*it_inner).second)->Print();
				it_inner++;
			}
			it++;
		}
		
		return "";
	}
	

};






//##############################################################################
class GuiTest {
	public:

   void Dummy(){
   }
};


}

#endif