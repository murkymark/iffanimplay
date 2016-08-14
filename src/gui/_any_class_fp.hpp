// Store and call function pointer to method of any class
//
// Method pointers are complex, they contain 2 numbers:
//  a pointer to the class instance in memory
//  an offset (or v-table index) to the method within the class
//  -> When calling a method (pointer) it gets an additional implicit (hidden) parameter
//
// In comparison to normal function pointers of non class members
// you cannot simply call methods of arbitrary classes the same way.
// Method pointers can only store method references of one specific class (without evil back and forth casting).
//


// Using template generated method pointers sharing the same base class

//todo: try to store non class function pointers!!!!


// ==> see "gui_event"




//callback test


#include <iostream>

using namespace std;



/****************************************************/

//function pointer encapsulation class
class MyCallback_Base {
	public:
	//void *user;
	virtual void callfp() = 0;
};

//for static fptr
class MyCallback_Static : public MyCallback_Base {
	public:
	void (*fp)(); //fp to C or static member function
	
	virtual void callfp(){
		(*fp)();
	}
	MyCallback_Static(void (*fptr)()){
		fp = fptr;
	}
};

//for member fptr
template<typename T>
class MyCallback_Member : public MyCallback_Base {
	public:
	T *o; //object ptr
	void (T::*pt2member)(); //ptr to member function
	
	//member function pointer
	virtual void callfp(){
		(o->*pt2member)();
	}
	MyCallback_Member(T *o_, void (T::*pt2member_)()){
		o = o_;
		pt2member = pt2member_;
	}
};



/****************************************************/

//the class used to bind function pointers
class MyCallback {
	public:
	MyCallback_Base *p;

	MyCallback() : p(NULL){
	}


	~MyCallback(){
		if(p!= NULL)
			delete p;
	}

	void bind(void (*foo)()){
		if(p != NULL)
			delete p;
		p = new MyCallback_Static(foo);
	}

	template<typename T>
	void bind(T *o, void (T::*pt2member)()){  //example: bind(&obj, &memberfunc)   or from within a method   bind(this, &memberfunc)
		if(p != NULL)
			delete p;
		p = new MyCallback_Member<T>(o, pt2member);
	}

	void unbind(){
		if(p != NULL)
			delete p;
		p = NULL;
	}


template<typename T>
void b(){
}

	void callfp(){
		if(p != NULL)
			p->callfp();
	}
};


/****************************************************/
//test functions to take pointers from

class A {
	public:
	
	void test(){
		cout << "A::test() called" << endl;
	}
};

void abc(){
	cout << "abc() called" << endl;
}


/****************************************************/
//test it all
void example(){
	A a;
	MyCallback m;

	m.bind(&abc); //formal way
	m.callfp();
	m.bind(abc);  //works also
	m.callfp();
	
	//for member function pointers we have to take care that the function pointer is not called anymore after the object instance is destroyed
	m.bind(&a, &A::test); //formal way to specify template type implicit
	m.callfp();
	m.bind(&a, a.test); //simpler way to specify template type implicit
	m.callfp();
	
	cout << sizeof(MyCallback_Static) << endl;
	cout << sizeof(MyCallback_Member<MyCallback>) << endl;
	cout << sizeof(MyCallback) << endl;
}
