CC = gcc
CPP = g++
AR = ar
RANLIB = ranlib

CCW32 = i686-w64-mingw32-gcc-win32
CPPW32 = i686-w64-mingw32-g++-win32
ARW32 = i686-w64-mingw32-gcc-ar-win32
RANLIBW32 = i686-w64-mingw32-gcc-ranlib-win32

CCW64 = x86_64-w64-mingw32-gcc-win32
CPPW64 = x86_64-w64-mingw32-g++-win32
ARW64 = x86_64-w64-mingw32-gcc-ar-win32
RANLIBW64 = x86_64-w64-mingw32-gcc-ranlib-win32

SRC = src
OBJ = obj
LIB = lib
LIBW32 = lib/mingw32
LIBW64 = lib/mingw64

CX = $(CC)
CFLAG = -Wall -Os -I./include
target = vhid

all:	lib lib_mingw32 lib_mingw64
lib:	$(OBJ)/$(target).lo
	mkdir -p $(LIB)
	$(AR) rc $(LIB)/lib$(target).a $(OBJ)/$(target).lo
	$(RANLIB) $(LIB)/lib$(target).a
lib_mingw32:	$(OBJ)/$(target).wo32
	mkdir -p $(LIBW32)
	$(ARW32) rc $(LIBW32)/lib$(target).a $(OBJ)/$(target).wo32
	$(RANLIBW32) $(LIBW32)/lib$(target).a
lib_mingw64:	$(OBJ)/$(target).wo64
	mkdir -p $(LIBW64)
	$(ARW64) rc $(LIBW64)/lib$(target).a $(OBJ)/$(target).wo64
	$(RANLIBW64) $(LIBW64)/lib$(target).a
	
clean:
	rm -rf $(OBJ)
cleanall:	clean
	rm -f $(LIB)/lib$(target).a
	rm -f $(LIBW32)/lib$(target).a
	rm -f $(LIBW64)/lib$(target).a
	
$(OBJ)/%.lo:	$(SRC)/%.c
	mkdir -p $(OBJ)
	$(CC) -c $(CFLAG) $< -o $@
$(OBJ)/%.lo:	$(SRC)/%.cpp
	mkdir -p $(OBJ)
	$(CPP) -c $(CFLAG) $< -o $@
$(OBJ)/%.wo32:	$(SRC)/%.c
	mkdir -p $(OBJ)
	$(CCW32) -c $(CFLAG) $< -o $@
$(OBJ)/%.wo32:	$(SRC)/%.cpp
	mkdir -p $(OBJ)
	$(CPPW32) -c $(CFLAG) $< -o $@
$(OBJ)/%.wo64:	$(SRC)/%.c
	mkdir -p $(OBJ)
	$(CCW64) -c $(CFLAG) $< -o $@
$(OBJ)/%.wo64:	$(SRC)/%.cpp
	mkdir -p $(OBJ)
	$(CPPW64) -c $(CFLAG) $< -o $@
