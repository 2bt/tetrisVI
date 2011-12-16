UNAME := $(shell uname)

SRC = $(wildcard *.c)
SRC+= sdl_draw/SDL_draw.c
SRC+= pixelfonts/5x3/font.c
SRC+= $(wildcard libs/*.c)

FLAGS = -Wall -O2 --std=gnu99

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

