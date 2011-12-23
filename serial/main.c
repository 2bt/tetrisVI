#include <pthread.h>
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

const unsigned char announce_addr[5] = { 'R', 'E', 'M', '0', 'T' };
const unsigned char game_addr[5] = { 0x55, 0xC3, 0xD2, 0x28, 0xC3 };
const unsigned char game_id[2] = { 0xC3, 0xD2 };


static int serial;

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


void send_cmd(int cmd, const unsigned char* buffer, int len) {

	printf("cmd %c len %d\n", cmd, len);

	static unsigned char magic[4] = { CMD_ESC, 0, CMD_ESC, CMD_END };

	magic[1] = cmd;
	tcdrain(serial);
	assert(write(serial, magic, 2) == 2);
	int i;
	for(i = 0; i < len; i++) {
		tcdrain(serial);
		assert(write(serial, &buffer[i], 1) == 1);
		if(buffer[i] == 92) {
			tcdrain(serial);
			assert(write(serial, &buffer[i], 1) == 1);
		}
		printf("write %3d at %3d\n", buffer[i], i);
	}
	tcdrain(serial);
	assert(write(serial, magic + 2, 2) == 2);
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
	announce_packet.announce.game_flags = 0;

	announce_packet.announce.interval = 5;
	announce_packet.announce.jitter = 16;
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
	memcpy(ack_packet.announce.game_mac, game_addr, 5);
}


typedef struct {
	int occupied;
	unsigned char id[4];
	unsigned char counter[4];
} Joiner;

typedef struct {
	int occupied;
	unsigned char id[4];
	unsigned char counter[4];
	// also other stuff here ...

} Player;

enum {
	MAX_JOINER = 32,
	MAX_PLAYER = 6,
};

Joiner joiners[MAX_JOINER];
Player players[MAX_PLAYER];

void join(int nr) {
	int i;
	memcpy(ack_packet.id, joiners[nr].id, 4);
	memcpy(ack_packet.counter, joiners[nr].counter, 4);

	for(i = 0; i < MAX_PLAYER; i++) {
		if(players[i].occupied) {
			if(memcmp(players[i].id, joiners[nr].id, 4) == 0)
			ack_packet.ack.flags = 1;
			break;
		}
	}
	if(i == MAX_PLAYER)	{

		for(i = 0; i < MAX_PLAYER; i++) {
			if(!players[i].occupied) {
				players[i].occupied = 1;
				memcpy(players[i].id, joiners[nr].id, 4);
				memcpy(players[i].counter, joiners[nr].counter, 4);

				ack_packet.ack.flags = 1;
				break;
			}
		}
		if(i == MAX_PLAYER) ack_packet.ack.flags = 0;
	}

	send_packet(&ack_packet);
}


int cmd_block = 0;

void process_cmd(unsigned char cmd, Packet* packet, unsigned char len) {

	printf("received command %c len %d\n", cmd, len);
	int i;

	switch(cmd) {
	case CMD_OK:
		cmd_block = 0;
		puts("<<<< ok >>>>");
		return;

	case CMD_PACKET:
		puts("<<<< packet >>>>");

		{
			int i;
			for(i = 0; i < len; i++) {
				unsigned char c = ((unsigned char*)packet)[i];
				printf("> %2x at %2d\n", c, i);
			}
		}
		if(len != packet->len) {
			puts("len error");
			return;
		}


		switch(packet->command) {
			case 'J':
				puts("join");

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
				printf("button      %x\n", packet->button.state);
				
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


int main() {
	puts("main");
	init_serial();
	puts("initilaized");

	prepare_announce();
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

	unsigned int time = get_time();
	unsigned int announce_time = time;
	int state = STATE_INIT_PACKETLEN;

	for(;;) {

		// read input
		while(read(serial, &byte, 1) == 1) {
			printf("read %3d %c pos %d\n", byte, byte, pos);
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


		unsigned int new_time = get_time();
		// TODO: call game loop


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

			// check whether anybody wans to join
			for(i = 0; i < MAX_JOINER; i++) {
				if(joiners[i].occupied) {
					printf(">>>>>>>>>>>>>-- %d\n", i);
					state = STATE_JOIN_ACK_TX_MAC;
					break;
				}
			}
			if(i != MAX_JOINER) break;

			if(0){
				int p = 0;
				int j = 0;
				for(i = 0; i < MAX_JOINER; i++) {
					j += joiners[i].occupied;
				}

				for(i = 0; i < MAX_PLAYER; i++) {
					p += players[i].occupied;
				}

				printf("players %d joiners %d\n", p, j);
			}
			break;


		case STATE_JOIN_ACK_TX_MAC:
			send_cmd(CMD_TX_MAC, join_ack_addr, 5);
			cmd_block = 1;
			state = STATE_JOIN_ACK_ACK;
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
			printf(">>>>>>>>>>>>>>> %d\n", i);
			assert(i < MAX_JOINER);
			state = STATE_JOIN_ACK_RESTORE_TX_MAC;
			break;


		case STATE_JOIN_ACK_RESTORE_TX_MAC:
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


