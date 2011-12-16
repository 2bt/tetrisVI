#ifndef GRID_H_
#define GRID_H_

enum {
	GRID_WIDTH = 10,
	GRID_HEIGHT = 20,
};

enum {
	STATE_NORMAL,
	STATE_WAIT,
	STATE_CLEARLINES,
	STATE_GAMEOVER
};

typedef struct Grid {
	int x;
	int y;
	int rot;
	int stone;
	int next_rot;
	int next_stone;
	int tick;
	int ticks_per_drop;
	int level_progress;
	int lines;
	int state;
	int state_delay;
	int animation;
	unsigned char matrix[GRID_HEIGHT][GRID_WIDTH];
	unsigned char highlight[GRID_HEIGHT];
} Grid;

void init_grid(Grid* grid);
void update_grid(Grid* grid);
void draw_grid(Grid* grid, int x_offset);

#endif