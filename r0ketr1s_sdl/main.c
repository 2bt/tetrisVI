#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <SDL/SDL.h>

#include "time.h"
#include "main.h"
#include "grid.h"
#include "config.h"
#include "sdl_draw/SDL_draw.h"

static int serial;

static int              rerender = 1;

static unsigned char    display[DISPLAY_HEIGHT][DISPLAY_WIDTH];

static Grid grids[MAX_PLAYERS];



typedef struct {
	int occupied;
	unsigned char id[4];
	unsigned char counter[4];
} Joiner;

typedef struct {
	int occupied;
	// stuff to be sent to player. priority: text, lines, state
	int needs_text; // "Player <n>"
	int needs_lines; // will never actually be set at the same time as needs_text (text is sent just once at the beginning)
	int needs_gamestate;
	int ready_to_send; // set to 0 after sending data, before we got an ack
	// player state
	int lines;
	int button_state;
	long long last_active;
	unsigned char id[4];
	unsigned char counter[4];
	unsigned char nick[18];
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

void init_serial() {

	serial = open("/dev/ttyACM0", O_RDWR | O_NONBLOCK);
	assert(serial != -1);

	struct termios config;
	memset(&config, 0, sizeof(config));
	tcgetattr(serial, &config);

	config.c_iflag = 0;
	config.c_oflag = 0;
	config.c_lflag = 0;
	config.c_cc[VMIN] = 1;
	config.c_cc[VTIME] = 5;

	cfsetospeed(&config, B115200);
	cfsetispeed(&config, B115200);
	config.c_cflag = CS8 | CREAD | CLOCAL;
	tcsetattr(serial, TCSANOW, &config);

}

int button_down(unsigned int nr, unsigned int button) {
	return (((players[nr].button_state)&(1<<button))==(1<<button));
}
int is_occupied(unsigned int nr) {

	return players[nr].occupied;

}

void push_lines(unsigned int nr, unsigned int lines) {
	players[nr].needs_lines = 1;
	players[nr].lines = lines;
}

int wants_to_send_data(Player *p) {
	return p->occupied && (p->needs_text || p->needs_lines || p->needs_gamestate);
}


void send_cmd(int cmd, const unsigned char* buffer, int len) {

//	printf("<= send cmd %c\n", cmd);

	static unsigned char magic[4] = { CMD_ESC, 0, CMD_ESC, CMD_END };

	magic[1] = cmd;
	assert(write(serial, magic, 2) == 2);
	usleep(250);
	int i;
	for(i = 0; i < len; i++) {
		assert(write(serial, &buffer[i], 1) == 1);
		usleep(200);
		if(buffer[i] == CMD_ESC) {
			assert(write(serial, &buffer[i], 1) == 1);
			usleep(200);
		}
	}
	assert(write(serial, magic + 2, 2) == 2);
	usleep(350);
}

void set_chan(unsigned char chan) {
	send_cmd(CMD_CHAN, &chan, 1);
}

void set_packetlen(unsigned char len) {
	send_cmd(CMD_PACKETLEN, &len, 1);
}


unsigned short do_crc(void* load, int len) {
	unsigned char* buf = (unsigned char*)load;
	unsigned short crc = 0xffff;
	int i;
	for(i = 0; i < len; i++) {
		crc  = (unsigned char)(crc >> 8) | (crc << 8);
		crc ^= buf[i];
		crc ^= (unsigned char)(crc & 0xff) >> 4;
		crc ^= (crc << 8) << 4;
		crc ^= ((crc & 0xff) << 4) << 1;
	}
	return crc;
}

void send_packet(Packet* p) {
	unsigned short crc = do_crc(p, 30);
	p->crc[0] = (crc >> 8) & 0xff;
	p->crc[1] = crc & 0xff;

	
	printf("<= send packet %c\n", p->command);
	send_cmd(CMD_PACKET, (unsigned char*)p, sizeof(Packet));
}


static Packet announce_packet;
void prepare_announce() {
	memset(&announce_packet, 0, sizeof(Packet));
	announce_packet.len = 32;
	announce_packet.protocol = 'G';

	announce_packet.command = 'A';
	memcpy(announce_packet.announce.game_mac, game_receive_addr, 5);
	announce_packet.announce.game_chan = GAME_CHANNEL;
	announce_packet.announce.game_id[0] = game_id[0];
	announce_packet.announce.game_id[1] = game_id[1];
	announce_packet.announce.game_flags = 4;

	announce_packet.announce.interval = 1;
	announce_packet.announce.jitter = 6;
	memcpy(announce_packet.announce.game_name, "tetriBot", 8);
}

void announce() {
	unsigned char* c = (unsigned char*)&announce_packet.counter;
	if(++c[0] || ++c[1] || ++c[2] || ++c[3]) {}
	send_packet(&announce_packet);
}


static Packet ack_packet;
void prepare_acknowledge() {
	memset(&ack_packet, 0, sizeof(Packet));
	ack_packet.len = 32;
	ack_packet.protocol = 'G';
	ack_packet.command = 'a';
}

static Packet nickrequest_packet;
void prepare_nickrequest() {
	memset(&nickrequest_packet, 0, sizeof(Packet));
	nickrequest_packet.len = 32;
	nickrequest_packet.protocol = 'G';
	nickrequest_packet.command = 'N';
}

static Packet text_packet;
void prepare_text() {
	memset(&text_packet, 0, sizeof(Packet));
	text_packet.len = 32;
	text_packet.protocol = 'G';
	text_packet.command = 'T';
}

static Packet blob_packet;
void prepare_blob() {
	memset(&blob_packet, 0, sizeof(Packet));
	blob_packet.len = 32;
	blob_packet.protocol = 'G';
	blob_packet.command = 'b';
}

void join(int nr) {
	int i;
	memcpy(ack_packet.id, joiners[nr].id, 4);
	memcpy(ack_packet.counter, joiners[nr].counter, 4);

	for(i = 0; i < MAX_PLAYER; i++) {
		if(players[i].occupied) {
			if(memcmp(players[i].id, joiners[nr].id, 4) == 0)
			{
				ack_packet.ack.flags = 1;
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
				players[i].needs_text = 1; 
				players[i].needs_lines = 0;
				players[i].needs_gamestate = 0;
				players[i].ready_to_send = 0;
				players[i].last_active = get_time();
				memcpy(players[i].id, joiners[nr].id, 4);
				memcpy(players[i].counter, joiners[nr].counter, 4);

				ack_packet.ack.flags = 1;

		    	init_grid(&grids[i], i);
//    			activate_grid(&grids[i]);

				break;
			}
		}
		if(i == MAX_PLAYER) ack_packet.ack.flags = 0; // no free slot
	}

	send_packet(&ack_packet);
}

void nickrequest(int nr) {
	memcpy(nickrequest_packet.id, players[nr].id, 4);

	send_packet(&nickrequest_packet);
}

void 
sendtext(int nr) {
	memcpy(text_packet.id, players[nr].id, 4);
	unsigned char* c = (unsigned char*)&text_packet.counter;
	if(++c[0] || ++c[1] || ++c[2] || ++c[3]) {} // increment counter


	if(players[nr].needs_text)
	{
		text_packet.text.x=1;
		text_packet.text.y=1;
		text_packet.text.flags=1; // clear screen before printing
	
		memcpy(text_packet.text.text, "Player ", 7);
		text_packet.text.text[7] = 49 + nr;
	}
	
	else if(players[nr].needs_lines)
	{
		text_packet.text.x=1;
		text_packet.text.y=15;
		text_packet.text.flags=0;
	
		sprintf((char*)text_packet.text.text, "Lines %i   ", players[nr].lines);
	}
	
	send_packet(&text_packet);
}

void sendstate(int nr) {
	memcpy(blob_packet.id, players[nr].id, 4);
	unsigned char* c = (unsigned char*)&blob_packet.counter;
	if(++c[0] || ++c[1] || ++c[2] || ++c[3]) {} // increment counter

	blob_packet.blob.data[DATA_X]=grids[nr].x;
	blob_packet.blob.data[DATA_Y]=grids[nr].y;
	blob_packet.blob.data[DATA_STONE]=grids[nr].stone;
	blob_packet.blob.data[DATA_ROT]=grids[nr].rot;
	blob_packet.blob.data[DATA_STONE_ID]=grids[nr].stone_count;
	blob_packet.blob.data[DATA_GAME_ID]=grids[nr].game_id;
	blob_packet.blob.data[DATA_LAST_X]=grids[nr].last_x;
	blob_packet.blob.data[DATA_LAST_Y]=grids[nr].last_y;
	blob_packet.blob.data[DATA_LAST_STONE]=grids[nr].last_stone;
	blob_packet.blob.data[DATA_LAST_ROT]=grids[nr].last_rot;
	blob_packet.blob.data[DATA_NEXT_STONE]=grids[nr].next_stone;
	blob_packet.blob.data[DATA_NEXT_ROT]=grids[nr].next_rot;
	
	
	send_packet(&blob_packet);
}

int cmd_block = 0; // block doing commands till we get an "CMD_OK"

static void process_cmd(unsigned char cmd, Packet* packet, unsigned char len) {

	int i;

	switch(cmd) {
	case CMD_OK:
//		printf("=> received ok cmd\n");
		cmd_block = 0;
		return;

	case CMD_PACKET:
		printf("=> received packet %c\n", packet->command);

		if(len != packet->len) {
			puts("len error");
			return;
		}


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
						memcpy(joiners[i].id, packet->id, 4);
						memcpy(joiners[i].counter, packet->counter, 4);
						break;
					}
				}
				break;


			case 'B':
				for(i = 0; i < MAX_PLAYER; i++) {
					if(!memcmp(players[i].id, packet->id, 4)) {
						players[i].ready_to_send = 1;
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
					if(!memcmp(players[i].id, packet->id, 4)) {
						if (players[i].needs_text)
							players[i].needs_text = 0; 
						else if (players[i].needs_lines)
							players[i].needs_lines = 0;
						else
							players[i].needs_gamestate = 0;
						players[i].ready_to_send = 1;
						break;
					}
				}
				
				break;

			default:
				puts("error");
				break;
		}
		return;

	default:
		printf("Unknown command 0x%02X\n", cmd);
		return;
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

	puts("main");
	init_serial();
	puts("initilaized");

	prepare_announce();
	prepare_text();
	prepare_blob();
	prepare_nickrequest();
	prepare_acknowledge();

	unsigned char game_send_addr[5];
	memcpy(game_send_addr, game_receive_addr, 5);
	game_send_addr[4]++;

	int i;
	unsigned char data[256];
	unsigned char pos = 0;
	unsigned char byte;
	unsigned char cmd = CMD_OK;
	int esc = 0;

	unsigned long long time = get_time();
	unsigned long long announce_time = time;
	unsigned long long tetris_time = time;
	int state = STATE_INIT_PACKETLEN;

    for(i = 0; i < MAX_PLAYERS; i++)
    {
    	init_grid(&grids[i], i);
//    	activate_grid(&grids[i]);
    }
    
    int running = 1;
    while(running) {

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

		// read input
		while(read(serial, &byte, 1) == 1) {
			//printf("    read %3d %c pos %d\n", byte, byte, pos);
			if(!esc && byte == CMD_ESC) {
				esc = 1;
				continue;
			}

			if (pos >= 254) pos = 0; // the next loop run could go out of our array... we get garbage anyway, so just keep reading garbage
			if(esc) { // last byte was ESC
				esc = 0;
				if(byte == CMD_ESC) {
					data[pos++] = CMD_ESC;
				}
				else if(byte == CMD_END) {
					process_cmd(cmd, (Packet*)data, pos);
				}
				else {
					pos = 0;
					cmd = byte;
					memset(data, 0, 256);
				}
				continue;
			}
			data[pos++] = byte;
		}

		unsigned long long new_time = get_time();

		if(new_time - tetris_time > 25) {

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

		if(cmd_block) continue;

		switch(state) {
		// init
		case STATE_INIT_PACKETLEN:
			set_packetlen(32);
			cmd_block = 1;
			state = STATE_INIT_RX_MAC;
			break;

		case STATE_INIT_RX_MAC:
			send_cmd(CMD_RX_MAC, game_receive_addr, 5);
			cmd_block = 1;
			state = STATE_RESTORE_CHAN;
			break;
		
		// default: idle
		case STATE_RESTORE_CHAN:
			set_chan(GAME_CHANNEL);
			cmd_block = 1;
			state = STATE_RESTORE_MAC;
			break;
		
		case STATE_RESTORE_MAC:
			send_cmd(CMD_TX_MAC, game_send_addr, 5);
			cmd_block = 1;
			state = STATE_IDLE;
			break;
		
		case STATE_IDLE:
			// announce the game
			if(new_time - announce_time > 1000) {
				announce_time = new_time;
				state = STATE_ANNOUNCE_SET_CHAN;
				break;
			}


			// check for deactive players
			for(i = 0; i < MAX_PLAYER; i++) {
				if(players[i].occupied) {
					if((get_time() - players[i].last_active) > 10000 )
					{
						players[i].occupied=0;
						players[i].id[0]=0;
						players[i].id[1]=0;
						players[i].id[2]=0;
						players[i].id[3]=0;

				    	init_grid(&grids[i], i);
//    					activate_grid(&grids[i]);
						
					};

				}
			}


			// check whether anybody wants to join
			for(i = 0; i < MAX_JOINER; i++) {
				if(joiners[i].occupied) {
					joiners[i].occupied = 0;
					cmd_block = 1;
					join(i);
					break;
				}
			}
			if(i != MAX_JOINER) break;

			// check whether anybody should get text
			for(i = 0; i < MAX_PLAYER; i++) {
				if(players[i].ready_to_send && wants_to_send_data(&players[i])) {
					cmd_block = 1;
					if (players[i].needs_text || players[i].needs_lines)
						sendtext(i);
					else
						sendstate(i);
					players[i].ready_to_send = 0;
					break;
				}
			}
			if(i != MAX_PLAYER) break;

			break;


		// announce stuff
		case STATE_ANNOUNCE_SET_CHAN:
			set_chan(ANNOUNCE_CHANNEL);
			cmd_block = 1;
			state = STATE_ANNOUNCE_SET_MAC;
			break;
		
		case STATE_ANNOUNCE_SET_MAC:
			send_cmd(CMD_TX_MAC, announce_addr, 5);
			cmd_block = 1;
			state = STATE_ANNOUNCE_SEND;
			break;

		case STATE_ANNOUNCE_SEND:
			announce();
			cmd_block = 1;
			state = STATE_RESTORE_CHAN;
			break;

		default:
			break;
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


void pixel(int x, int y, unsigned char color) {
    assert(x < DISPLAY_WIDTH);
    assert(y < DISPLAY_HEIGHT);
    assert(color < 16);
    if(display[y][x] != color) {
        rerender = 1;
        display[y][x] = color;
    }
}
