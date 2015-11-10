// Store and call function pointer to method of any class
//
// Method pointers are complex, they contain 2 numbers:
//  a pointer to the class instance in memory
//  an offset (or v-table index) to the method within the class
//  -> When calling a method (pointer) it gets an additional implicite (hidden) parameter
//
// In comparison to normal function pointers of non class members
// you cannot simply call methods of arbitrary classes the same way.
// Method pointers can only store method references of one specific class (without evil back and forth casting).
//


// Using template generated method pointers sharing the same base class

//todo: try to store non class function pointers!!!!


