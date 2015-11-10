#include "safemem.hpp"

#include <iostream>


//static check() version
void checkMem(void *mem, size_t size, void *from, size_t len, SafeMemoryException *e){
	if(reinterpret_cast<char*>(from) < reinterpret_cast<char*>(mem)  ||  reinterpret_cast<char*>(mem) + size < reinterpret_cast<char*>(from) + len) {
		std::cerr << "Illegal memory access!" << std::endl;
		if(size < 1)
			std::cerr << " legal range:  " << "none" << std::endl;
		else
			std::cerr << " legal range:  " << (unsigned long long)mem << "-" << (unsigned long long)mem + size - 1 << std::endl;
		std::cerr << " tried access: " << (unsigned long long)from << "-" << (unsigned long long)from + len - 1 << std::endl;
		throw *e;
	}
}



//derived class version, not throwing exception
void SafeMemorySkip::check(void *mem, size_t size, void *from, size_t len, SafeMemoryException *e){
	if(reinterpret_cast<char*>(from) < reinterpret_cast<char*>(mem)  ||  reinterpret_cast<char*>(mem) + size < reinterpret_cast<char*>(from) + len) {
		std::cerr << "Illegal memory access!" << std::endl;
		if(size < 1)
			std::cerr << " legal range:  " << "none" << std::endl;
		else
			std::cerr << " legal range:  " << (unsigned long long)mem << "-" << (unsigned long long)mem + size - 1 << std::endl;
		std::cerr << " tried access: " << (unsigned long long)from << "-" << (unsigned long long)from + len - 1 << std::endl;
		//throw *e;
	}
}








//check boundary: is access range in available block?
//mem:  pointer to start of legal memory block
//size: size of memory block 
//from: pointer of first byte to access in memory block
//len:  number of bytes to access
void SafeMemory::check(void *mem, size_t size, void *from, size_t len, SafeMemoryException *e){
	if(reinterpret_cast<char*>(from) < reinterpret_cast<char*>(mem)  ||  reinterpret_cast<char*>(mem) + size < reinterpret_cast<char*>(from) + len) {
		std::cerr << "Illegal memory access!" << std::endl;
		if(size < 1)
			std::cerr << " legal range:  " << "none" << std::endl;
		else
			std::cerr << " legal range:  " << (unsigned long long)mem << "-" << (unsigned long long)mem + size - 1 << std::endl;
		std::cerr << " tried access: " << (unsigned long long)from << "-" << (unsigned long long)from + len - 1 << std::endl;
		throw *e;
	}
}



SafeMemory::SafeMemory(void *mem, size_t s){
	this->memory = mem;
	this->size = s;
	if(mem == NULL)
		this->size = 0;
}

int8_t SafeMemory::readInt8(int off){
	int8_t *from = reinterpret_cast<int8_t*>(memory) + off;
	check(memory, size, from, 1, &e); //throws exception if access will be out of boundary
	return *from;
}

int16_t SafeMemory::readInt16BE(int off){
	uint8_t *from = reinterpret_cast<uint8_t*>(memory) + off;
	check(memory, size, from, 2, &e); //throws exception if access will be out of boundary
	return ((from[0] << 8) | from[1]);
}

int32_t SafeMemory::readInt32BE(int off){
	uint8_t *from = reinterpret_cast<uint8_t*>(memory) + off;
	check(memory, size, from, 4, &e); //throws exception if access will be out of boundary
	return ((from[0] << 24) | (from[1] << 16) | (from[2] << 8) | from[3]);
}



void SafeMemory::copyFrom32(SafeMemory &to, int off_to, int off_from){
	uint8_t *ptrfrom = reinterpret_cast<uint8_t*>(memory) + off_from;
	uint8_t *ptrto = reinterpret_cast<uint8_t*>(to.memory) + off_to;
	
	check(memory, size, ptrfrom, 4, &e);
	check(to.memory, to.size, ptrto, 4, &e);
	
	*(reinterpret_cast<int32_t*>(ptrto)) = *(reinterpret_cast<int32_t*>(ptrfrom));
}

void SafeMemory::copyFrom16(SafeMemory &to, int off_to, int off_from){
	uint8_t *ptrfrom = reinterpret_cast<uint8_t*>(memory) + off_from;
	uint8_t *ptrto = reinterpret_cast<uint8_t*>(to.memory) + off_to;
	
	check(memory, size, ptrfrom, 2, &e);
	check(to.memory, to.size, ptrto, 2, &e);
	
	*(reinterpret_cast<int16_t*>(ptrto)) = *(reinterpret_cast<int16_t*>(ptrfrom));
}



void SafeMemory::copyFrom16(void *to, int off_from){
	uint8_t *ptrfrom = reinterpret_cast<uint8_t*>(memory) + off_from;
	check(memory, size, ptrfrom, 2, &e);
	*(reinterpret_cast<int16_t*>(to)) = *(reinterpret_cast<int16_t*>(ptrfrom));
}

void SafeMemory::copyFrom32(void *to, int off_from){
	uint8_t *ptrfrom = reinterpret_cast<uint8_t*>(memory) + off_from;
	check(memory, size, ptrfrom, 4, &e);
	*(reinterpret_cast<int32_t*>(to)) = *(reinterpret_cast<int32_t*>(ptrfrom));
}




void SafeMemory::copyTo16(int off_to, void *from){
	uint8_t *ptrto = reinterpret_cast<uint8_t*>(memory) + off_to;
	check(memory, size, ptrto, 2, &e);
	*(reinterpret_cast<int16_t*>(ptrto)) = *(reinterpret_cast<int16_t*>(from));
}

void SafeMemory::copyTo32(int off_to, void *from){
	uint8_t *ptrto = reinterpret_cast<uint8_t*>(memory) + off_to;
	check(memory, size, ptrto, 4, &e);
	*(reinterpret_cast<int32_t*>(ptrto)) = *(reinterpret_cast<int32_t*>(from));
}

