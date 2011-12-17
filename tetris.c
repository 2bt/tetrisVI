#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "grid.h"
#include "tetris.h"

enum {
	MAX_PLAYERS = 6
};

static Grid grids[MAX_PLAYERS];
static int player_count;


void tetris_load() {
	int i;
	for(i = 0; i < MAX_PLAYERS; i++) init_grid(&grids[i]);
	
	player_count = 1;
}

void tetris_update() {

	int i;
	for(i = 0; i < player_count; i++) update_grid(&grids[i]);

	for(i = 0; i < player_count; i++) draw_grid(&grids[i], i * 12 + 1);
}


