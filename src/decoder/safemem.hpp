#ifndef _safemem_H_
#define _safemem_H_

#include <exception>
#include <stdexcept>

//#include <string>
#include <stdint.h> //int.._t types



/*
class Exception : public exception
{
public:
  Exception(string m="exception!") : msg(m) {}
  ~Exception() throw() {}
  const char* what() const throw() {
   return msg.c_str();
  }

private:
  string msg;
};
*/



// Exception thrown when trying to access memory beyond boundary via class SafeMemory
class SafeMemoryException : public std::runtime_error {
 public:
 SafeMemoryException(std::string m="Memory access violation!") :std::runtime_error(m) {}
};




// class for detecting illegal memory access by checking boundary when using methods
// will print error and throws exception without doing the illegal access
// instead of copying the same data from a safememory instance multiple times from the same offset,
//  better read once to a local buffer => prevents redundant checks
class SafeMemory {
	public:

	void *memory;	//pointer to static or allocated memory
	size_t size;	//size of memory block
	SafeMemoryException e;
	
	
	SafeMemory(void *mem, size_t size);

  //read value from pointer stored in Big Endian byte order, return value in system byte order always (on an LE system it gets converted to LE)
	int8_t  readInt8(int off);
	int16_t readInt16BE(int off);
	int32_t readInt32BE(int off);

	//int16_t readInt16LE(int off);
	//int32_t readInt32LE(int off);

  //read as uint
	inline uint8_t  readUInt8(int off)    { return static_cast<uint8_t >(readInt8(off   )); }
	inline uint16_t readUInt16BE(int off) { return static_cast<uint16_t>(readInt16BE(off)); }
	inline uint32_t readUInt32BE(int off) { return static_cast<uint32_t>(readInt32BE(off)); }


  //copy from pointer to this safe memory, only destination memory is safe
	void copyTo8( int off_to, void *from);
	void copyTo16(int off_to, void *from);
	void copyTo32(int off_to, void *from);
	void copyTo(  int off_to, void *from, int n);
	
	//copy from this safe memory to pointer, only source memory is safe
	void copyFrom8( void *to, int off_from);
	void copyFrom16(void *to, int off_from);
	void copyFrom32(void *to, int off_from);
	void copyfrom(  void *to, int off_from, int n);
	
	//copy from this safe memory to safe memory (auto checking both)
	void copyFrom8( SafeMemory &to, int off_to, int off_from);
	void copyFrom16(SafeMemory &to, int off_to, int off_from);
	void copyFrom32(SafeMemory &to, int off_to, int off_from);
	void copyFrom(  SafeMemory &to, int off_to, int off_from, int n);
	
	//check if memory access is within boundaries
	virtual void check(void *mem, size_t size, void *from, size_t len, SafeMemoryException *e);
};




//here is a static version of the check() function
void checkMem(void *mem, size_t size, void *from, size_t len, SafeMemoryException *e);




//same as base class, but just skips illegal access instead of throwing exception
// -> consider this child class as an example for your own handler
class SafeMemorySkip : SafeMemory {
	public:
	
	inline virtual void check(void *mem, size_t size, void *from, size_t len, SafeMemoryException *e = NULL);
};

#endif
