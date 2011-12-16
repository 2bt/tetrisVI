#ifndef MAIN_H_
#define MAIN_H_

enum {
	DISPLAY_WIDTH = 72,
	DISPLAY_HEIGHT = 32
};

enum {
	BUTTON_UP,
	BUTTON_DOWN,
	BUTTON_LEFT,
	BUTTON_RIGHT,
	BUTTON_A,
	BUTTON_B,
	BUTTON_START,
	BUTTON_SELECT,
};

int button_down(unsigned int button);

void pixel(int x, int y, unsigned char color);

#endif
