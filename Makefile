SRC = $(wildcard *.c)

FLAGS = -Wall -O2 -s -lSDL

all:
	gcc -o tetris $(SRC) $(FLAGS)

clean:
	rm -f tetris

