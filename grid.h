#ifndef GRID_H_
#define GRID_H_

enum {
	GRID_WIDTH = 10,
	GRID_HEIGHT = 20,
};

enum {
	STATE_FREE,
	STATE_NORMAL,
	STATE_WAIT,
	STATE_CLEARLINES,
	STATE_GAMEOVER
};

typedef struct Grid {
	int nr;
	int x;
	int y;
	int rot;
	int stone;
	int next_rot;
	int next_stone;
	int stone_count;
	int tick;
	int ticks_per_drop;
	int level_progress;
	int lines;
	int state;
	int state_delay;
	int animation;
	char matrix[GRID_HEIGHT][GRID_WIDTH];
	char highlight[GRID_HEIGHT];
	int input_mov;
	int input_rep;
	int input_rot;
} Grid;

void init_grid(Grid* grid, int nr);
int activate_grid(Grid* grid);
void update_grid(Grid* grid);
void draw_grid(Grid* grid);

#endif
