#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <SDL/SDL.h>

#include "main.h"
#include "tetris.h"
#include "sdl_draw/SDL_draw.h"

enum { ZOOM = 14 };

typedef struct {
	int nr;
	int state;
	int buttons[8];
	char name[32];
} Player;

static Player			players[6];
static int				input_map[6];
static unsigned char	display[DISPLAY_HEIGHT][DISPLAY_WIDTH];
static int				rerender = 1;


static void set_button(int input_nr, int button, int state) {

	Player* p = &players[input_nr];
	if(!p->state) {
		p->nr = add_player();	// TODO: player nick etc.
		if(p->nr >= 0) {
			p->state = 1;
			input_map[p->nr] = input_nr;
		}
	}
	p->buttons[button] = state;
}

int button_down(unsigned int nr, unsigned int button) {
	Player* p = &players[input_map[nr]];
	if(p->state) {
		return p->buttons[button];
	}
	return 0;
}


static int map_key(unsigned int sdl_key) {
	switch(sdl_key) {
	case SDLK_RIGHT:	return BUTTON_RIGHT;
	case SDLK_LEFT:		return BUTTON_LEFT;
	case SDLK_UP:		return BUTTON_UP;
	case SDLK_DOWN:		return BUTTON_DOWN;
	case SDLK_x:		return BUTTON_A;
	case SDLK_c:		return BUTTON_B;
	case SDLK_RETURN:	return BUTTON_START;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:	return BUTTON_SELECT;
	default:			return -1;
	}
}



void pixel(int x, int y, unsigned char color) {
	assert(x < DISPLAY_WIDTH);
	assert(y < DISPLAY_HEIGHT);
	assert(color < 16);
	if(display[y][x] != color) {
		rerender=1;
		display[y][x] = color;
	}
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

	int input_nr = 5;
	int key;
	int fast = 0;

	int running = 1;
	while(running) {

	for(int i = 0;i < 5;i++)
	{
		Player* p = &players[i];
		if(!p->state) {
			p->nr = add_player();	// TODO: player nick etc.
			p->state = 1;
		}
	}



		SDL_Event ev;
		while(SDL_PollEvent(&ev)) {

			switch(ev.type) {
			case SDL_QUIT:
				running = 0;
				break;
			case SDL_KEYUP:
			case SDL_KEYDOWN:

				key = map_key(ev.key.keysym.sym);
				if(key > 0) {
					set_button(input_nr, key, ev.type == SDL_KEYDOWN);
					break;
				}
				if(ev.key.keysym.sym == SDLK_SPACE) {
					fast ^= ev.type == SDL_KEYDOWN;
					break;
				}

				if(ev.type == SDL_KEYUP) break;

				switch(ev.key.keysym.sym) {
				case SDLK_ESCAPE:
					running = 0;
					break;

				case SDLK_1:
					input_nr = 0;
					break;

				case SDLK_2:
					input_nr = 1;
					break;

				case SDLK_3:
					input_nr = 2;
					break;

				case SDLK_4:
					input_nr = 3;
					break;

				case SDLK_5:
					input_nr = 4;
					break;

				case SDLK_6:
					input_nr = 5;
					break;


				default: break;
				}
			default: break;
			}
		}

		tetris_update();

		if(rerender) {
			rerender = 0;
			for(int x = 0; x < DISPLAY_WIDTH; x++)
				for(int y = 0; y < DISPLAY_HEIGHT; y++)
					Draw_FillCircle(screen, ZOOM * x + ZOOM / 2,
						ZOOM * y + ZOOM / 2, ZOOM * 0.45, COLORS[display[y][x]]);
			SDL_Flip(screen);
		}
		if(!fast)
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

