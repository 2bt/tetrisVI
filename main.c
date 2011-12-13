#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <SDL/SDL.h>

#include "main.h"

enum { ZOOM = 10 };

static unsigned int display[DISPLAY_HEIGHT][DISPLAY_WIDTH];
static int button_state[8];

void pixel(int x, int y, unsigned int color) {
	assert(x < DISPLAY_WIDTH);
	assert(y < DISPLAY_HEIGHT);
	display[y][x] = color;
}

int button_down(unsigned int button) {
	assert(button < 8);
//	return button_state[button];
	return button_state[button]-->0; // hacky
}


int main(int argc, char *argv[]) {
	srand(SDL_GetTicks());
	load();

	SDL_Surface* screen = SDL_SetVideoMode(
		DISPLAY_WIDTH * ZOOM,
		DISPLAY_HEIGHT * ZOOM,
		32, SDL_SWSURFACE | SDL_DOUBLEBUF);


	SDL_EnableKeyRepeat(100, 30);	// must be deleted


	int x, y;
	int running = 1;
	while(running) {
		SDL_Event ev;
		while(SDL_PollEvent(&ev)) {

			switch(ev.type) {
			case SDL_KEYUP:
			case SDL_KEYDOWN:

				switch(ev.key.keysym.sym) {
				case SDLK_ESCAPE:
					running = 0;
					break;

				case SDLK_RIGHT:
					button_state[BUTTON_RIGHT] = (ev.type == SDL_KEYDOWN);
					break;

				case SDLK_LEFT:
					button_state[BUTTON_LEFT] = (ev.type == SDL_KEYDOWN);
					break;

				case SDLK_UP:
					button_state[BUTTON_UP] = (ev.type == SDL_KEYDOWN);
					break;

				case SDLK_DOWN:
					button_state[BUTTON_DOWN] = (ev.type == SDL_KEYDOWN);
					break;

				case SDLK_x:
					button_state[BUTTON_A] = (ev.type == SDL_KEYDOWN);
					break;

				case SDLK_c:
					button_state[BUTTON_B] = (ev.type == SDL_KEYDOWN);
					break;

				case SDLK_RETURN:
					button_state[BUTTON_START] = (ev.type == SDL_KEYDOWN);
					break;

				case SDLK_LSHIFT:
				case SDLK_RSHIFT:
					button_state[BUTTON_SELECT] = (ev.type == SDL_KEYDOWN);
					break;

				default:
					break;
				}

			default:
				break;
			}
		}

		update();

		SDL_Rect rect = { 0, 0, ZOOM, ZOOM };
		for(x = rect.x = 0; x < DISPLAY_WIDTH; rect.x += ZOOM, x++)
			for(y = rect.y = 0; y < DISPLAY_HEIGHT; rect.y += ZOOM, y++)
				SDL_FillRect(screen, &rect, display[y][x]);

		SDL_Flip(screen);
		SDL_Delay(20);
	}

	SDL_Quit();
	return 0;
}

