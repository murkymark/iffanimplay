#
# gcc Makefile for libiffanim and player
#
# "make" :       creates library and player program (statically linked)
# "make lib" :   creates the library only (no libSDL required)
# "make clean" : deletes compilation results
#
# Compiled files can be found in dir "bin","lib","obj"
#
# - Change this file according to your system configuration!
# - This makefile creates directories: "obj", "bin", "lib"
#   compiled files can be found there afterwards,
#   "make clean" deletes them
# - The header for the compiled library can be taken from the source
# - If a header file was modified, delete the object file whose src file includes it,
#   otherwise the object file won't be rebuilt.
#
# - currently configured for mingw/win32 using MSYS
#   MSYS needed for file system commands (mkdir, rm)
#
# - make sure SDL headers and libs are available and found by gcc to build the player

#compiler
CPP = g++

#archiver for creating static lib
ARCH = ar -rcs

#for making prog smaller
STRIP = strip -s

#programs to create and delete directories
RM = rm -r -f
MKDIR = mkdir -v -p

#0 or 1 specifies if debug symbols are stripped
DEBUG = 1

ifeq ($(DEBUG),1)
	DEBUGOPT = -g
endif



#name of the executeable file
BINFILE = iffanimplay.exe

#name of the library
LIBFILE1 = libiffanim.a
LIBFILE2 = libcdxl.a

#header dir for header to libiffanim
LIBINCDIR = -I"src/decoder/iffanim" -I"src/decoder/cdxl"

#C++ library and include path - should already be set, if compiler is installed properly
#LIBSDIR =  -L"c/mingw/lib"
#INCSDIR =  -I"c/mingw/include" 

#library and include path for SDL
LIBSDL =  -L"/c/MinGW/lib" -lmingw32 -lSDLmain -lSDL.dll
 #LIBSDL =  -L"/c/wxDev-Cpp/lib" -lmingw32 -lSDLmain -lSDL -mwindows
INCSDL =  -I"/c/MinGW/include/SDL"



################################### no system specific adjustments needed beyond this line

#dirs to create for result
OBJDIR = obj
BINDIR = bin
LIBDIR = lib

#binary output
BIN = $(BINDIR)/$(BINFILE)
#BIN = $(BINFILE)

#library
LIB1 = $(LIBDIR)/$(LIBFILE1)
LIB2 = $(LIBDIR)/$(LIBFILE2)


#list of all files to compile (no )
SRCS = \
 src/gui/font_gohufont.c \
 src/gui/font.cpp \
 src/gui/gui_event.cpp \
 src/gui/gui.cpp \
 src/gui/gui_widgets.cpp \
 src/gui/unisurface.cpp\
 src/player/player_gui.cpp \
 src/player/player_decwrapper.cpp \
 src/player/system_specific.cpp \
 src/player/scale.c \
 src/player/player.cpp \
 src/main.cpp

LIBSRCS1 = \
 src/decoder/amiga_conv.c \
 src/decoder/safemem.cpp \
 src/decoder/iffanim/iffanim.cpp

LIBSRCS2 = \
 src/decoder/amiga_conv.c \
 src/decoder/cdxl/cdxl.cpp \



#convert file pathes .cpp/.c <-> .o
#to generate .o file names:
# 1. remove "src/"
# 2. replace "/" with "."
# 3. add prefix "$(OBJDIR)/" and suffix ".o"
# The functions seem a bit complex, but this appears to be the best way to compile source files from different dirs to a single object dir
# It wouldn't work without being able to generate the source file path from the object path.
func_src2o = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(subst /,.,$(subst src/,,$(1)))))
func_o2src = $(foreach f,$(1),$(addsuffix $(suffix $(basename $(f))),$(addprefix src/,$(subst .,/,$(subst $(OBJDIR)/,,$(basename $(basename $(f))))))))


OBJFILES = $(call func_src2o,$(SRCS))

OBJFILESLIB1 = $(call func_src2o,$(LIBSRCS1))
OBJFILESLIB2 = $(call func_src2o,$(LIBSRCS2))

#.d file for each source file, written to obj dir
DEPSFILES = $(addsuffix .d, $(basename $(OBJFILES) $(OBJFILESLIB1) $(OBJFILESLIB2))) 

#for debugging
#$(warning $(call func_src2o,$(SRCS)))
#$(warning $(call func_o2src,$(call func_src2o,$(SRCS))))
#$(warning $(OBJFILESLIB1))
#$(warning $(DEPSFILES))




# Delete the default suffixes (built-in rules)
MAKEFLAGS += --no-builtin-rules
.SUFFIXES: 


.PHONY: all


all $(BIN) : $(OBJDIR) $(DEPSFILES) $(LIB1) $(LIB2) $(OBJFILES)
	$(MKDIR) $(BINDIR)
	$(CPP) $(DEBUGOPT) -Os -static $(OBJFILES) "$(LIB1)" "$(LIB2)" -o $(BIN) $(LIBINCDIR) $(INCSDIR) $(LIBSDIR) $(INCSDL) $(LIBSDL)
ifeq ($(DEBUG),0)
	$(STRIP) $(BIN)
endif


#call always first, once for a make run
$(OBJDIR):
	$(MKDIR) $(OBJDIR)

#make sure dir is created first
$(OBJFILES) $(DEPSFILES): | $(OBJDIR)



#include .d files, included after being created by this same makefile
-include $(OBJFILES:.o=.d)



#build object files
#secondary expansion needed here to get the single current target file from the list of targets as string for the prerequisite (auto variable $@)
.SECONDEXPANSION:
$(sort $(OBJFILES) $(OBJFILESLIB1) $(OBJFILESLIB2)): $$(call func_o2src,$$@)
	$(CPP) $(DEBUGOPT) -Os -c $< -o $@ $(LIBINCDIR) $(INCSDIR) $(LIBSDIR) $(INCSDL) $(LIBSDL)


#create .d files, each with includes as prerequisites in the rule, without them modified .h files are not detected
#a .d file has to be remade if corresponding sources changes (-> .d file also target)
%.d: $$(call func_o2src,$$@)
	$(CPP) $(CPPFLAGS) $(LIBINCDIR) $(INCSDIR) -MM -MT "$@ $*.o" "$<" > "$@"





.PHONY: lib
.PHONY: libs

lib libs $(LIB1) $(LIB2): $(OBJFILESLIB1) $(OBJFILESLIB2)
	$(MKDIR) $(LIBDIR)
#	echo $(OBJFILESLIB1)
	$(ARCH) $(LIB1) $(OBJFILESLIB1)
	$(ARCH) $(LIB2) $(OBJFILESLIB2)





.PHONY : clean
clean :
	$(RM) $(OBJDIR)
	$(RM) $(BINDIR)
	$(RM) $(LIBDIR)
