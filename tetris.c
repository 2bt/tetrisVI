#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grid.h"
#include "tetris.h"

enum {
	MAX_PLAYERS = 1
//	MAX_PLAYERS = 6
};

static Grid grids[MAX_PLAYERS];


void tetris_load() {
	int i;
	for(i = 0; i < MAX_PLAYERS; i++) init_grid(&grids[i], i);
}

void tetris_update() {
	int i;
	for(i = 0; i < MAX_PLAYERS; i++) {
		update_grid(&grids[i]);
		draw_grid(&grids[i]);
	}
}

int add_player() {
	int i;
	for(i = 0; i < MAX_PLAYERS; i++) {
		if(activate_grid(&grids[i])) return i;
	}

	return -1;
}

void remove_player(int nr) {
	init_grid(&grids[nr], nr);
}

