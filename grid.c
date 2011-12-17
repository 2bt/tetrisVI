#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grid.h"
#include "main.h"
#include "libs/text.h"

static const char STONES[7][16] = {
	{ 0, 0, 0, 0, 0,15,15, 0, 0,15,15, 0, 0, 0, 0, 0 },
	{ 0, 0,10, 0, 5, 5,15, 5, 0, 0,10, 0, 0, 0,10, 0 },
	{ 0, 0, 5, 0, 0, 0,15,15, 0,10,10, 5, 0, 0, 0, 0 },
	{ 0, 0, 0, 5, 0,10,15, 5, 0, 0,15,10, 0, 0, 0, 0 },
	{ 0, 4, 5, 8, 0,10,15,10, 0, 2, 5, 1, 0, 0, 0, 0 },
	{ 0, 8, 5, 1, 0,10,15,10, 0, 4, 5, 2, 0, 0, 0, 0 },
	{ 0, 0,11, 0, 0,13,15, 7, 0, 0,14, 0, 0, 0, 0, 0 }
};

static const char PALETTE[8] = {
	1, 15, 9, 14, 10, 13, 11, 12 
};


enum {
	ANIMATION_SHIFT,
	ANIMATION_VANISH,
	ANIMATION_COUNT
};


static void new_stone(Grid* grid) {
	grid->tick = 0;
	grid->x = GRID_WIDTH / 2 - 2;
	grid->y = -3;
	grid->rot = grid->next_rot;
	grid->stone = grid->next_stone;
	grid->next_rot = 1 << (rand() & 3);
	grid->next_stone = rand() % 7;
}


static int collision(Grid* grid, int top_also) {
	int x, y;
	for(y = 0; y < 4; y++) {
		for(x = 0; x < 4; x++) {
			if(STONES[grid->stone][x * 4 + y] & grid->rot) {
				if(top_also) {
					if(	x + grid->x < 0 || x + grid->x >= GRID_WIDTH ||
						y + grid->y < 0 || y + grid->y >= GRID_HEIGHT ||
						grid->matrix[y + grid->y][x + grid->x])
						return 1;
				}
				else {
					if(	x + grid->x < 0 || x + grid->x >= GRID_WIDTH ||
						y + grid->y >= GRID_HEIGHT || (
						y + grid->y >= 0 &&
						grid->matrix[y + grid->y][x + grid->x]))
						return 1;
				}
			}
		}
	}
	return 0;
}




static void update_grid_normal(Grid* grid) {
	int i, j, x, y;

	// rotation
	j = button_down(BUTTON_A) - button_down(BUTTON_B);
	if(j) {
		i = grid->rot;
		grid->rot = (j > 0)
			? grid->rot * 2 % 15
			: (grid->rot / 2 | grid->rot * 8) & 15;
		if(collision(grid, 0)) grid->rot = i;
	}


	// horizontal movement
	i = grid->x;
	grid->x += button_down(BUTTON_RIGHT) - button_down(BUTTON_LEFT);
	if(i != grid->x && collision(grid, 0)) grid->x = i;


	// vertical movement
	grid->tick++;
	if(button_down(BUTTON_DOWN) || grid->tick >= grid->ticks_per_drop) {
		grid->tick = 0;
		grid->y++;
		if(collision(grid, 0)) {
			grid->y--;

			// check for game over
			if(collision(grid, 1)) {
				grid->state = STATE_GAMEOVER;
				return;
			}

			// copy stone to grid
			for(y = 0; y < 4; y++)
				for(x = 0; x < 4; x++)
					if(STONES[grid->stone][x * 4 + y] & grid->rot)
						grid->matrix[y + grid->y][x + grid->x] = grid->stone + 1;

			// get a new stone
			new_stone(grid);

			// check for complete lines
			int lines = 0;
			for(y = 0; y < GRID_HEIGHT; y++) {

				for(x = 0; x < GRID_WIDTH; x++)
					if(!grid->matrix[y][x]) break;

				lines += (x == GRID_WIDTH);
				grid->highlight[y] = (x == GRID_WIDTH);
			}
			grid->state = lines ? STATE_CLEARLINES : STATE_WAIT;
			grid->state_delay = 0;
			if(++grid->animation == ANIMATION_COUNT) grid->animation = 0;
		}
	}
}


static void update_grid_clearlines(Grid* grid) {
	int i, x, y;

	// animations
	switch(grid->animation) {

	case ANIMATION_SHIFT:
		if(grid->state_delay & 1) {
			for(y = 0; y < GRID_HEIGHT; y++) {
				if(!grid->highlight[y]) continue;
				if(y & 1) {
					for(x = 1; x < GRID_WIDTH; x++)
						grid->matrix[y][x - 1] = grid->matrix[y][x];
					grid->matrix[y][GRID_WIDTH - 1] = 0;
				}
				else {
					for(x = GRID_WIDTH - 1; x > 0; x--)
						grid->matrix[y][x] = grid->matrix[y][x - 1];
					grid->matrix[y][0] = 0;
				}
			}
		}
		break;

	case ANIMATION_VANISH:
		if(grid->state_delay & 1) {
			for(y = 0; y < GRID_HEIGHT; y++) {
				if(!grid->highlight[y]) continue;

				x = rand() % GRID_WIDTH;
				for(i = 0; i < GRID_WIDTH; i++) {
					if(grid->matrix[y][x]) {
						grid->matrix[y][x] = 0;
						break;
					}
					if(++x == GRID_WIDTH) x = 0;
				}

			}
		}
		break;

	default: break;
	}

	// erase lines
	if(++grid->state_delay >= 24) {
		for(y = 0; y < GRID_HEIGHT; y++) {
			if(!grid->highlight[y]) continue;
			grid->highlight[y] = 0;

			grid->lines++;
			grid->level_progress++;
			if(grid->level_progress == 10) {
				grid->level_progress = 0;
				grid->ticks_per_drop--;
				if(grid->ticks_per_drop == 0) grid->ticks_per_drop = 1;
			}

			for(i = y; i > 0; i--)
				for(x = 0; x < GRID_WIDTH; x++)
					grid->matrix[i][x] = grid->matrix[i - 1][x];
		}
		grid->state = STATE_NORMAL;
	}
}

static void update_grid_wait(Grid* grid) {
	if(++grid->state_delay > 15) grid->state = STATE_NORMAL;
}



void init_grid(Grid* grid) {
	grid->ticks_per_drop = 20;
	grid->level_progress = 0;
	grid->lines = 0;
	grid->animation = 0;
	grid->state = STATE_NORMAL;
	memset(grid->matrix, 0, sizeof(grid->matrix));
	memset(grid->highlight, 0, sizeof(grid->highlight));
	new_stone(grid);
	new_stone(grid);


//	print_5x3_at(0,0,"Text Test",15);
}

void update_grid(Grid* grid) {

	switch(grid->state) {
	case STATE_NORMAL:
		update_grid_normal(grid);
		break;

	case STATE_WAIT:
		update_grid_wait(grid);
		break;

	case STATE_CLEARLINES:
		update_grid_clearlines(grid);
		break;


	default: break;

	}
}


void draw_grid(Grid* grid, int x_offset) {
	int x, y;

	for(y = 0; y < 4; y++) {
		for(x = 0; x < 4; x++) {
			unsigned char color = 0;
			if(STONES[grid->next_stone][x * 4 + y] & grid->next_rot) {
				color = PALETTE[grid->next_stone + 1];
			}
			pixel(x_offset + x + 7, y + 6, color);
		}
	}

	for(y = 0; y < GRID_HEIGHT; y++) {
		for(x = 0; x < GRID_WIDTH; x++) {
			unsigned char color = PALETTE[grid->matrix[y][x]];
			if(	grid->state == STATE_NORMAL &&
				x >= grid->x && x < grid->x + 4 &&
				y >= grid->y && y < grid->y + 4 &&
				STONES[grid->stone][(x - grid->x) * 4 + y - grid->y] & grid->rot) {
				color = PALETTE[grid->stone + 1];
			}
			pixel(x + x_offset, y + 11, color);
		}
	}
	print_unsigned_5x3_at(x_offset + 1, 0, grid->lines, 3, ' ', 8);

}


