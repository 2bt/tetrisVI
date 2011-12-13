#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "grid.h"


enum {
	PLAYER_COUNT = 6
};

static Grid grids[PLAYER_COUNT];

void load() {
	int i;
	for(i = 0; i < PLAYER_COUNT; i++)
		init_grid(&grids[i]);
}

void update() {

	int i;
	for(i = 0; i < PLAYER_COUNT; i++)
		update_grid(&grids[i]);

	for(i = 0; i < PLAYER_COUNT; i++)
		draw_grid(&grids[i], i * 12 + 1);
}


