#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <SDL/SDL.h>

#include "time.h"
#include "grid.h"
#include "config.h"
#include "ui.h"
#include "serial.h"
#include "sdl_draw/SDL_draw.h"


enum {
	DISPLAY_WIDTH = 72,
	DISPLAY_HEIGHT = 32
};

enum {
	MAX_PLAYERS = 6
};


static int              rerender = 1;
static unsigned char    display[DISPLAY_HEIGHT][DISPLAY_WIDTH];
static Grid grids[MAX_PLAYERS];
static unsigned char game_send_addr[5];


typedef struct {
	int occupied;
	unsigned int id;
	unsigned int counter;
} Joiner;

typedef struct {
	int occupied;
	// stuff to be sent to player. priority: text, lines, state
	int needs_title; // "Player <n>"
	int needs_lines; // will never actually be set at the same time as needs_text (text is sent just once at the beginning)
	int needs_gamestate;
	// player state
	int button_state;
	long long last_active;
	unsigned int id;
	//unsigned char counter[4];
	//unsigned char nick[18];
	// also other stuff here ...

} Player;

enum {
	MAX_JOINER = 32,
	MAX_PLAYER = 6,
};

enum {
	DATA_X = 0,
	DATA_Y,
	DATA_STONE,
	DATA_ROT,
	DATA_STONE_ID,
	DATA_GAME_ID,
	DATA_LAST_X,
	DATA_LAST_Y,
	DATA_LAST_STONE,
	DATA_LAST_ROT,
	DATA_NEXT_STONE,
	DATA_NEXT_ROT
};

Joiner joiners[MAX_JOINER];
Player players[MAX_PLAYER];

int button_down(unsigned int nr, unsigned int button) {
	return ((players[nr].button_state) & (1<<button)) != 0;
}
int is_occupied(unsigned int nr) {
	return players[nr].occupied;
}
void player_gameover(unsigned int nr) {}

void push_lines(unsigned int nr, unsigned int lines) {
	players[nr].needs_lines = 1;
}


void announce() {
	struct Packet *announce_packet = new_packet('A');
	memcpy(announce_packet->announce.game_mac, game_receive_addr, 5);
	announce_packet->announce.game_chan = GAME_CHANNEL;
	announce_packet->announce.game_id[0] = game_id[0];
	announce_packet->announce.game_id[1] = game_id[1];
	announce_packet->announce.game_flags = 4;
	announce_packet->announce.interval = 1;
	announce_packet->announce.jitter = 6;
	memcpy(announce_packet->announce.game_name, "tetrisVI", 8);
	queue_packet(announce_packet, ANNOUNCE_CHANNEL, announce_addr);
}


void join(int nr) {
	int i;
	struct Packet *ack_packet = new_packet('a');
	ack_packet->id = joiners[nr].id;
	ack_packet->counter = joiners[nr].counter;

	for(i = 0; i < MAX_PLAYER; i++) {
		if(players[i].occupied) {
			if(players[i].id == joiners[nr].id)
			{
				ack_packet->ack.flags = 1;
				printf("player %i again\n",i);
				break;
			}
		}
	}
	if(i == MAX_PLAYER)	{ // not already joined

		for(i = 0; i < MAX_PLAYER; i++) {
			if(!players[i].occupied) {
				printf("player %i added\n",i);
				players[i].occupied = 1;
				players[i].needs_title = 1; 
				players[i].needs_lines = 0;
				players[i].needs_gamestate = 0;
				players[i].last_active = get_time();
				players[i].id = joiners[nr].id;

				ack_packet->ack.flags = 1;

				init_grid(&grids[i], i);

				break;
			}
		}
		if(i == MAX_PLAYER) ack_packet->ack.flags = 0; // no free slot
	}

	queue_packet(ack_packet, GAME_CHANNEL, game_send_addr);
}

void nickrequest(int nr) {
	struct Packet *nickrequest_packet = new_packet('N');
	nickrequest_packet->id = players[nr].id;

	queue_packet(nickrequest_packet, GAME_CHANNEL, game_send_addr);
}

void sendtitle(int nr) {
	struct Packet *text_packet = new_packet('T');
	text_packet->id = players[nr].id;

	text_packet->text.x=1;
	text_packet->text.y=1;
	text_packet->text.flags=1; // clear screen before printing

	memcpy(text_packet->text.text, "Player ", 7);
	text_packet->text.text[7] = '0' + nr; // add the actual number

	queue_packet(text_packet, GAME_CHANNEL, game_send_addr);
}

void sendlines(int nr) {
	struct Packet *text_packet = new_packet('T');
	text_packet->id = players[nr].id;

	text_packet->text.x=1;
	text_packet->text.y=15;
	text_packet->text.flags=0;

	sprintf((char*)text_packet->text.text, "Lines %i   ", grids[nr].lines);

	queue_packet(text_packet, GAME_CHANNEL, game_send_addr);
}

void sendstate(int nr) {
	struct Packet *blob_packet = new_packet('b');
	blob_packet->id = players[nr].id;

	blob_packet->blob.data[DATA_X]=grids[nr].x;
	blob_packet->blob.data[DATA_Y]=grids[nr].y;
	blob_packet->blob.data[DATA_STONE]=grids[nr].stone;
	blob_packet->blob.data[DATA_ROT]=grids[nr].rot;
	blob_packet->blob.data[DATA_STONE_ID]=grids[nr].stone_count;
	blob_packet->blob.data[DATA_GAME_ID]=grids[nr].game_id;
	blob_packet->blob.data[DATA_LAST_X]=grids[nr].last_x;
	blob_packet->blob.data[DATA_LAST_Y]=grids[nr].last_y;
	blob_packet->blob.data[DATA_LAST_STONE]=grids[nr].last_stone;
	blob_packet->blob.data[DATA_LAST_ROT]=grids[nr].last_rot;
	blob_packet->blob.data[DATA_NEXT_STONE]=grids[nr].next_stone;
	blob_packet->blob.data[DATA_NEXT_ROT]=grids[nr].next_rot;
	
	
	queue_packet(blob_packet, GAME_CHANNEL, game_send_addr);
}


void process_packet(struct Packet* packet) {

	int i;
	switch(packet->command) {
		case 'J':
			// ignore packets with wrong game id
			if(	packet->join.game_id[0] != game_id[0] ||
				packet->join.game_id[1] != game_id[1])
				break;

			// look for free joiner
			for(i = 0; i < MAX_JOINER; i++) {
				if(!joiners[i].occupied) {
					joiners[i].occupied = 1;
					joiners[i].id = packet->id;
					joiners[i].counter = packet->counter;
					break;
				}
			}
			break;


		case 'B':
			for(i = 0; i < MAX_PLAYER; i++) {
				if(players[i].occupied && players[i].id == packet->id) {
					players[i].last_active = get_time();
					players[i].button_state = packet->button.state;
					
					//paket indicator
					if(display[10][1+(i*12)]==0)
					{
						display[10][1+(i*12)]=15;
					}
					else
					{
						display[10][1+(i*12)]=0;
					}
					rerender=1;
					
					break;
				}
			}
			
			break;

		case 'a':
			for(i = 0; i < MAX_PLAYER; i++) {
				if(players[i].occupied && players[i].id == packet->id) { // we got an ack for this player and assume all is fine
					if (players[i].needs_title)
						players[i].needs_title = 0; 
					else
						players[i].needs_lines = 0;
					break;
				}
			}
			
			break;

		default:
			printf("error: unexpected command %d", packet->command);
			break;
	}
}

int main(int argc, char *argv[]) {
	srand(SDL_GetTicks());
	memset(players, 0, sizeof(players));
	

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

	init_serial("/dev/ttyACM0", game_receive_addr, GAME_CHANNEL);
	puts("Serial device successfully initilaized");

	memcpy(game_send_addr, game_receive_addr, 5);
	game_send_addr[4]++;

	
	int i;

	unsigned long long time = get_time();
	unsigned long long announce_time = time;
	unsigned long long tetris_time = time;

	for(i = 0; i < MAX_PLAYERS; i++) {
		init_grid(&grids[i], i);
	}
	
	int running = 1;
	while(running) {

		// read SDL input
		SDL_Event ev;
		while(SDL_PollEvent(&ev)) {

			switch(ev.type) {
			case SDL_QUIT:
				running = 0;
				break;
			case SDL_KEYUP:
			case SDL_KEYDOWN:

				switch(ev.key.keysym.sym) {
				case SDLK_ESCAPE:
					running = 0;
					break;
				default:break;
				}
			}
		}

		// read serial input
		seriel_receive();

		// update screen
		unsigned long long new_time = get_time();
		if(new_time - tetris_time > 20) {

			tetris_time = new_time;

			for(i = 0; i < MAX_PLAYERS; i++) {
				int updt = update_grid(&grids[i]);
				draw_grid(&grids[i]);
				if (updt && players[i].occupied) {
					players[i].needs_gamestate = 1;
				}
			}

			if(rerender) {
				rerender = 0;
				int x,y;
				for(x = 0; x < DISPLAY_WIDTH; x++)
					for(y = 0; y < DISPLAY_HEIGHT; y++)
						Draw_FillCircle(screen, ZOOM * x + ZOOM / 2,
							ZOOM * y + ZOOM / 2, ZOOM * 0.45, COLORS[display[y][x]]);
				SDL_Flip(screen);
			}
		}

		if (serial_do_work()) {
			// we are idle
			
			// announce the game
			if(new_time - announce_time > 1000) {
				announce_time = new_time;
				announce();
				continue;
			}

			// check for inactive players
			for(i = 0; i < MAX_PLAYER; i++) {
				if(players[i].occupied) {
					if((get_time() - players[i].last_active) > 10000) {
						players[i].occupied=0;
						init_grid(&grids[i], i);
					}
				}
			}

			// check whether anybody wants to join
			for(i = 0; i < MAX_JOINER; i++) {
				if(joiners[i].occupied) {
					joiners[i].occupied = 0;
					join(i);
					break;
				}
			}
			if(i != MAX_JOINER) continue;

			// check whether anybody should get text
			for(i = 0; i < MAX_PLAYER; i++) {
				if (players[i].occupied) {
					if (players[i].needs_title) {
						sendtitle(i);
						// we expect an ack
						break;
					}
					else if (players[i].needs_lines) {
						sendlines(i);
						// we expect an ack
						break;
					}
					else if (players[i].needs_gamestate) {
						sendstate(i);
						players[i].needs_gamestate = 0;
						break;
					}
				}
			}
			if(i != MAX_PLAYER) continue;

		}
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


void set_frame_buffer(int x, int y, unsigned char color) {
	assert(x < DISPLAY_WIDTH);
	assert(y < DISPLAY_HEIGHT);
	assert(color < 16);
	if(display[y][x] != color) {
		rerender = 1;
		display[y][x] = color;
	}
}
