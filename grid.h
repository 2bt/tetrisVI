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
	char game_id; // game-over detection for KIs (the value is unpredictable, but will only be re-used after 256 games)
	// information about the last stone that was fixed (for KIs). only reliable if stone_count > 0
	int last_x, last_y, last_stone, last_rot;
} Grid;

void init_grid(Grid* grid, int nr);
int activate_grid(Grid* grid);
int update_grid(Grid* grid); // return: 1 if something changed
void draw_grid(Grid* grid);

#endif
