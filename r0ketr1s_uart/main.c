#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "time.h"
#include "main.h"
#include "grid.h"
#include "config.h"

static int serial_bridge;
static int serial_g3d2;

static unsigned char    display[DISPLAY_HEIGHT][DISPLAY_WIDTH];

static Grid grids[MAX_PLAYERS];



typedef struct {
	int occupied;
	unsigned char id[4];
	unsigned char counter[4];
} Joiner;

typedef struct {
	int occupied;
	int request_nick;
	int is_ready_for_nick;
	int needs_text;
	int needs_lines;
	int lines;
	int is_ready_for_text;
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

Joiner joiners[MAX_JOINER];
Player players[MAX_PLAYER];


void init_serial() {

	serial_bridge = open("/dev/ttyACM0", O_RDWR | O_NONBLOCK);
//	serial_bridge = open("/dev/cu.usbmodem5d11", O_RDWR | O_NONBLOCK);
	assert(serial_bridge != -1);

	struct termios config;
	memset(&config, 0, sizeof(config));
	tcgetattr(serial_bridge, &config);

	config.c_iflag = 0;
	config.c_oflag = 0;
	config.c_lflag = 0;
	config.c_cc[VMIN] = 1;
	config.c_cc[VTIME] = 5;

	cfsetospeed(&config, B115200);
	cfsetispeed(&config, B115200);
	config.c_cflag = CS8 | CREAD | CLOCAL;
	tcsetattr(serial_bridge, TCSANOW, &config);


	serial_g3d2 = open("/dev/ttyUSB0", O_RDWR | O_NONBLOCK);
	assert(serial_g3d2 != -1);

	struct termios config2;
	memset(&config2, 0, sizeof(config));
	tcgetattr(serial_g3d2, &config2);

	config2.c_iflag = 0;
	config2.c_oflag = 0;
	config2.c_lflag = 0;
	config2.c_cc[VMIN] = 1;
	config2.c_cc[VTIME] = 5;

	cfsetospeed(&config2, B500000);
	cfsetispeed(&config2, B500000);
	config2.c_cflag = CS8 | CREAD | CLOCAL;
	tcsetattr(serial_g3d2, TCSANOW, &config2);

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


void send_cmd(int cmd, const unsigned char* buffer, int len) {

	printf("send cmd %c len %d\n", cmd, len);

	static unsigned char magic[4] = { CMD_ESC, 0, CMD_ESC, CMD_END };

	magic[1] = cmd;
//	tcdrain(serial);
	assert(write(serial_bridge, magic, 2) == 2);
	int i;
	for(i = 0; i < len; i++) {
//		tcdrain(serial);
		assert(write(serial_bridge, &buffer[i], 1) == 1);
		if(buffer[i] == 92) {
//			tcdrain(serial);
			assert(write(serial_bridge, &buffer[i], 1) == 1);
		}
	}
//	tcdrain(serial);
	assert(write(serial_bridge, magic + 2, 2) == 2);
//	tcdrain(serial);
	
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

	send_cmd(CMD_PACKET, (unsigned char*)p, sizeof(Packet));
}


Packet announce_packet;
void prepare_announce() {
	memset(&announce_packet, 0, sizeof(Packet));
	announce_packet.len = 32;
	announce_packet.protocol = 'G';

	announce_packet.command = 'A';
	memcpy(announce_packet.announce.game_mac, game_addr, 5);
	announce_packet.announce.game_chan = GAME_CHANNEL;
	announce_packet.announce.game_id[0] = game_id[0];
	announce_packet.announce.game_id[1] = game_id[1];
	announce_packet.announce.game_flags = 4;

	announce_packet.announce.interval = 1;
	announce_packet.announce.jitter = 6;
	memcpy(announce_packet.announce.game_name, "tetrisVI", 8);
}

void announce() {
	unsigned char* c = (unsigned char*)&announce_packet.counter;
	if(++c[0] || ++c[1] || ++c[2] || ++c[3]) {}
	send_packet(&announce_packet);
}


Packet ack_packet;
void prepare_acknowledge() {
	memset(&ack_packet, 0, sizeof(Packet));
	ack_packet.len = 32;
	ack_packet.protocol = 'G';
	ack_packet.command = 'a';
}

Packet nickrequest_packet;
void prepare_nickrequest() {
	memset(&nickrequest_packet, 0, sizeof(Packet));
	nickrequest_packet.len = 32;
	nickrequest_packet.protocol = 'G';
	nickrequest_packet.command = 'N';
}

Packet text_packet;
void prepare_text() {
	memset(&text_packet, 0, sizeof(Packet));
	text_packet.len = 32;
	text_packet.protocol = 'G';
	text_packet.command = 'T';
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
	if(i == MAX_PLAYER)	{

		for(i = 0; i < MAX_PLAYER; i++) {
			if(!players[i].occupied) {
				printf("player %i added\n",i);
				players[i].occupied = 1;
				players[i].request_nick = 1;
				players[i].is_ready_for_nick = 0; 
				players[i].needs_text = 1; 
				players[i].needs_lines = 0; 
				players[i].last_active = get_time(); 
				players[i].is_ready_for_text = 0; 
				memcpy(players[i].id, joiners[nr].id, 4);
				memcpy(players[i].counter, joiners[nr].counter, 4);

				ack_packet.ack.flags = 1;

		    	init_grid(&grids[i], i);
//    			activate_grid(&grids[i]);

				break;
			}
		}
		if(i == MAX_PLAYER) ack_packet.ack.flags = 0;
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
	if(++c[0] || ++c[1] || ++c[2] || ++c[3]) {}


	if(players[nr].needs_text)
	{
		text_packet.text.x=1;
		text_packet.text.y=1;
		text_packet.text.flags=1;
	
		memcpy(text_packet.text.text, "Player ", 7);
		text_packet.text.text[7] = 49 + nr;
	}

	if(players[nr].needs_lines)
	{
		text_packet.text.x=1;
		text_packet.text.y=15;
		text_packet.text.flags=0;
	
		sprintf(text_packet.text.text, "Lines %i", players[nr].lines);
	}
	
	send_packet(&text_packet);
}


int cmd_block = 0;

void process_cmd(unsigned char cmd, Packet* packet, unsigned char len) {

	int i;

	switch(cmd) {
	case CMD_OK:
		cmd_block = 0;
		return;

	case CMD_PACKET:
		printf("received packet %c\n", packet->command);

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
						if((players[i].needs_text)||(players[i].needs_lines))   players[i].is_ready_for_text = 1;
						if(players[i].request_nick) players[i].is_ready_for_nick = 1;
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
						
						break;
					}
				}
				
				break;

			case 'n':
				for(i = 0; i < MAX_PLAYER; i++) {
					if(!memcmp(players[i].id, packet->id, 4)) {
						memcpy(players[i].nick, packet->nick.nick, 18);
						players[i].request_nick = 0;
						break;
					}
				}
				
				break;

			case 'a':
				for(i = 0; i < MAX_PLAYER; i++) {
					if(!memcmp(players[i].id, packet->id, 4)) {
						players[i].needs_text = 0; 
						players[i].needs_lines = 0; 
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
		puts("what???");
		return;
	}
}

int main(int argc, char *argv[]) {
	int x, y;
	for(x = 0; x < DISPLAY_WIDTH; x++)
		for(y = 0; y < DISPLAY_HEIGHT; y++)
			display[y][x] = 16;

	puts("main");
	init_serial();
	puts("initilaized");

	prepare_announce();
	prepare_text();
	prepare_nickrequest();
	prepare_acknowledge();

	unsigned char join_ack_addr[5];
	memcpy(join_ack_addr, game_addr, 5);
	join_ack_addr[4]++;

	int i;
	unsigned char data[256];
	unsigned char pos = 0;
	unsigned char byte;
	unsigned char cmd;
	int esc = 0;

	unsigned long long time = get_time();
	unsigned long long announce_time = time;
	unsigned long long tetris_time = time;
	int state = STATE_INIT_PACKETLEN;

    for(i = 0; i < MAX_PLAYERS; i++) {
    	init_grid(&grids[i], i);
//    	activate_grid(&grids[i]);
    }
    
    int running = 1;
    while(running) {

		// read input
		while(read(serial_bridge, &byte, 1) == 1) {
			//printf("    read %3d %c pos %d\n", byte, byte, pos);
			if(!esc && byte == CMD_ESC) {
				esc = 1;
				continue;
			}

			if(esc) {
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

		if(new_time - tetris_time > 20) {

			tetris_time = new_time;

			for(i = 0; i < MAX_PLAYERS; i++) {
				update_grid(&grids[i]);
				draw_grid(&grids[i]);
			}

		}

		if(cmd_block) continue;

		switch(state) {
		case STATE_INIT_PACKETLEN:
			set_packetlen(32);
			cmd_block = 1;
			state = STATE_INIT_RX_MAC;
			break;

		case STATE_INIT_RX_MAC:
			send_cmd(CMD_RX_MAC, game_addr, 5);
			cmd_block = 1;
			state = STATE_INIT_TX_MAC;
			break;

		case STATE_INIT_TX_MAC:
			send_cmd(CMD_TX_MAC, announce_addr, 5);
			cmd_block = 1;
			state = STATE_INIT_CHAN;
			break;

		case STATE_INIT_CHAN:
		case STATE_ANNOUNCE_RESTORE_CHAN:
			set_chan(GAME_CHANNEL);
			cmd_block = 1;
			state = STATE_IDLE;
			break;
		
		
		case STATE_IDLE:
			// announce the game
			if(new_time - announce_time > 1000) {
				announce_time = new_time;
				state = STATE_ANNOUNCE_CHAN;
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


			// check whether anybody wans to join
			for(i = 0; i < MAX_JOINER; i++) {
				if(joiners[i].occupied) {
					state = STATE_JOIN_ACK_TX_MAC;
					break;
				}
			}
			if(i != MAX_JOINER) break;

			// check whether anybody should send their nick
			for(i = 0; i < MAX_PLAYER; i++) {
				if((players[i].is_ready_for_nick)&&(players[i].request_nick)) {
					state = STATE_NICKREQUEST_TX_MAC;
					break;
				}
			}
			if(i != MAX_PLAYER) break;

			// check whether anybody should send get text
			for(i = 0; i < MAX_PLAYER; i++) {
				if((players[i].is_ready_for_text)&&((players[i].needs_text)||(players[i].needs_lines))&&(!players[i].request_nick)) {
					state = STATE_TEXT_TX_MAC;
					break;
				}
			}
			if(i != MAX_PLAYER) break;

			break;


		case STATE_JOIN_ACK_TX_MAC:
			send_cmd(CMD_TX_MAC, join_ack_addr, 5);
			cmd_block = 1;
			state = STATE_JOIN_ACK_ACK;
			break;

		case STATE_NICKREQUEST_TX_MAC:
			send_cmd(CMD_TX_MAC, join_ack_addr, 5);
			cmd_block = 1;
			state = STATE_NICKREQUEST_NICKREQUEST;
			break;

		case STATE_TEXT_TX_MAC:
			send_cmd(CMD_TX_MAC, join_ack_addr, 5);
			cmd_block = 1;
			state = STATE_TEXT_TEXT;
			break;

		case STATE_JOIN_ACK_ACK:
			for(i = 0; i < MAX_JOINER; i++) {
				if(joiners[i].occupied) {
					joiners[i].occupied = 0;
					cmd_block = 1;
					join(i);
					break;
				}
			}
			assert(i < MAX_JOINER);
			state = STATE_JOIN_ACK_RESTORE_TX_MAC;
			break;

		case STATE_NICKREQUEST_NICKREQUEST:
			for(i = 0; i < MAX_PLAYER; i++) {
				if((players[i].is_ready_for_nick)&&(players[i].request_nick)) {
					cmd_block = 1;
					players[i].is_ready_for_nick=0;
					players[i].is_ready_for_text=0;
					nickrequest(i);
					break;
				}
			}
			assert(i < MAX_PLAYER);
			state = STATE_NICKREQUEST_RESTORE_TX_MAC;
			break;

		case STATE_TEXT_TEXT:
			for(i = 0; i < MAX_PLAYER; i++) {
				if((players[i].is_ready_for_text)&&((players[i].needs_text)||(players[i].needs_lines))) {
					cmd_block = 1;
					players[i].is_ready_for_nick = 0;
					players[i].is_ready_for_text = 0;
					sendtext(i);
					break;
				}
			}
			assert(i < MAX_PLAYER);
			state = STATE_NICKREQUEST_RESTORE_TX_MAC;
			break;


		case STATE_JOIN_ACK_RESTORE_TX_MAC:
		case STATE_NICKREQUEST_RESTORE_TX_MAC:
		case STATE_TEXT_RESTORE_TX_MAC:
			send_cmd(CMD_TX_MAC, announce_addr, 5);
			cmd_block = 1;
			state = STATE_IDLE;
			break;


		case STATE_ANNOUNCE_CHAN:
			set_chan(ANNOUNCE_CHANNEL);
			cmd_block = 1;
			state = STATE_ANNOUNCE_ANNOUNCE;
			break;

		case STATE_ANNOUNCE_ANNOUNCE:
			announce();
			cmd_block = 1;
			state = STATE_ANNOUNCE_RESTORE_CHAN;
			break;

		default:
			break;
		}
	}

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
        display[y][x] = color;
        
        
			y = 31-y;

			unsigned char c=104;
			write(serial_g3d2,&c,1);
            int x2 = x % 8;
            int y2 = y % 8;
            int mod = (x-x2)/8 + ((y-y2)/8*9);

            c=mod;
            write(serial_g3d2,&c,1);
            c=x2;
            write(serial_g3d2,&c,1);
            c=y2;
            write(serial_g3d2,&c,1);
            c=color;
            write(serial_g3d2,&c,1);
            usleep(200);

        
    }
}
