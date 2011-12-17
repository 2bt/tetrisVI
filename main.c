#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <SDL/SDL.h>

#include "main.h"
#include "tetris.h"
#include "sdl_draw/SDL_draw.h"

enum { ZOOM = 14 };

static unsigned char display[DISPLAY_HEIGHT][DISPLAY_WIDTH];
static int button_state[6][8];

void pixel(int x, int y, unsigned char color) {
	assert(x < DISPLAY_WIDTH);
	assert(y < DISPLAY_HEIGHT);
	assert(color < 16);
	display[y][x] = color;
}

int button_down(unsigned int nr, unsigned int button) {
	assert(button < 8);
	return button_state[nr][button];
}


int main(int argc, char *argv[]) {
	srand(SDL_GetTicks());
	tetris_load();

	SDL_Surface* screen = SDL_SetVideoMode(
		DISPLAY_WIDTH * ZOOM,
		DISPLAY_HEIGHT * ZOOM,
		32, SDL_SWSURFACE | SDL_DOUBLEBUF);

	const unsigned int COLORS[] = {
		SDL_MapRGB(screen->format, 0x00,0x10,0x00),
		SDL_MapRGB(screen->format, 0x00,0x20,0x00),
		SDL_MapRGB(screen->format, 0x00,0x30,0x00),
		SDL_MapRGB(screen->format, 0x00,0x40,0x00),
		SDL_MapRGB(screen->format, 0x00,0x50,0x00),
		SDL_MapRGB(screen->format, 0x00,0x60,0x00),
		SDL_MapRGB(screen->format, 0x00,0x70,0x00),
		SDL_MapRGB(screen->format, 0x00,0x80,0x00),
		SDL_MapRGB(screen->format, 0x00,0x90,0x00),
		SDL_MapRGB(screen->format, 0x00,0xa0,0x00),
		SDL_MapRGB(screen->format, 0x00,0xb0,0x00),
		SDL_MapRGB(screen->format, 0x00,0xc0,0x00),
		SDL_MapRGB(screen->format, 0x00,0xd0,0x00),
		SDL_MapRGB(screen->format, 0x00,0xe0,0x00),
		SDL_MapRGB(screen->format, 0x00,0xf0,0x00),
		SDL_MapRGB(screen->format, 0x00,0xff,0x00)
	};

	int running = 1;
	while(running) {
		SDL_Event ev;
		while(SDL_PollEvent(&ev)) {

			switch(ev.type) {
			case SDL_QUIT:
				running = 0;
				break;
			case SDL_KEYUP:
			case SDL_KEYDOWN:

				switch(ev.key.keysym.sym) {
				case SDLK_ESCAPE:
					running = 0;
					break;

				case SDLK_RIGHT:
					button_state[0][BUTTON_RIGHT] = (ev.type == SDL_KEYDOWN);
					break;

				case SDLK_LEFT:
					button_state[0][BUTTON_LEFT] = (ev.type == SDL_KEYDOWN);
					break;

				case SDLK_UP:
					button_state[0][BUTTON_UP] = (ev.type == SDL_KEYDOWN);
					break;

				case SDLK_DOWN:
					button_state[0][BUTTON_DOWN] = (ev.type == SDL_KEYDOWN);
					break;

				case SDLK_x:
					button_state[0][BUTTON_A] = (ev.type == SDL_KEYDOWN);
					break;

				case SDLK_c:
					button_state[0][BUTTON_B] = (ev.type == SDL_KEYDOWN);
					break;

				case SDLK_RETURN:
					button_state[0][BUTTON_START] = (ev.type == SDL_KEYDOWN);
					break;

				case SDLK_LSHIFT:
				case SDLK_RSHIFT:
					button_state[0][BUTTON_SELECT] = (ev.type == SDL_KEYDOWN);
					break;

				default:
					break;
				}

			default:
				break;
			}
		}

		tetris_update();

		for(int x = 0; x < DISPLAY_WIDTH; x++)
			for(int y = 0; y < DISPLAY_HEIGHT; y++)
				Draw_FillCircle(screen, ZOOM*x + ZOOM/2, ZOOM*y + ZOOM/2,
					ZOOM*0.45, COLORS[display[y][x]]);

		SDL_Flip(screen);
		SDL_Delay(20);
	}

	SDL_Quit();
	return 0;
}

// found it in the internet...
static unsigned int my_rand(void) {
	static unsigned int z1 = 12345, z2 = 12345, z3 = 12345, z4 = 12345;
	unsigned int b;
	b  = ((z1 << 6) ^ z1) >> 13;
	z1 = ((z1 & 4294967294U) << 18) ^ b;
	b  = ((z2 << 2) ^ z2) >> 27; 
	z2 = ((z2 & 4294967288U) << 2) ^ b;
	b  = ((z3 << 13) ^ z3) >> 21;
	z3 = ((z3 & 4294967280U) << 7) ^ b;
	b  = ((z4 << 3) ^ z4) >> 12;
	z4 = ((z4 & 4294967168U) << 13) ^ b;
	return (z1 ^ z2 ^ z3 ^ z4);
}

unsigned int rand_int(unsigned int limit) {
	return my_rand() % limit;
}

