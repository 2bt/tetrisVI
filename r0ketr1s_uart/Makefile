UNAME := $(shell uname)

SRC = $(wildcard *.c)
SRC+= pixelfonts/5x3/font.c
SRC+= $(wildcard libs/*.c)

FLAGS = -Wall -O2 

all:
	gcc -o r0ketr1s $(SRC) $(FLAGS)

clean:
	rm -f r0ketr1s

