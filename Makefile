UNAME := $(shell uname)

SRC = $(wildcard *.c)

FLAGS = -Wall -O2

ifeq ($(UNAME), Darwin)
	FLAGS +=  -I/Library/Frameworks/SDL.framework/Headers SDLmain.m -framework SDL -framework Cocoa
endif

ifeq ($(UNAME), Linux)
	FLAGS +=  -lSDL
endif
    

all:
	gcc -o tetris $(SRC) $(FLAGS)

clean:
	rm -f tetris

