#include "serial.h"
#include "ui.h"
//#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>

// enable/disable debugging on command level
//#define CMD_DEBUG
// enable/disable debugging on packet lavel
#define PACKET_DEBUG

/*
 * This implements the lower levels of package sending, and it hides all the actual
 * serial communication.
 * It does not know package types and stuff, nor does it care about acks and counters.
 */

// current state
static int serial = 0;
static int cmd_block; // we just sent work to the bridge, block doing commands till we get an "CMD_OK"
static unsigned char cur_chan, default_chan; // the currently selected channel (0: none set yet), and the channel to go back to when being idle
static unsigned char rx_addr[5], tx_addr[5]; // the currently used MAC (initial byte 0: none set yet)

// stuff we want to do
static struct Packet *next_packet = NULL;
static unsigned char next_chan;
static const unsigned char *next_addr;

// state of the sender
enum {
	// initialize
	STATE_INIT_PACKETLEN,
	STATE_INIT_RX_MAC,
	// sending
	STATE_SET_CHAN,
	STATE_SET_MAC,
	STATE_SEND,
	// idling around
	STATE_RESTORE_CHAN,
	STATE_IDLE,
} state;

static void print_time() {
	struct timeval tv;
	char tmptime[32];
	struct tm * tptr;
	
	gettimeofday(&tv,NULL);
	tptr=gmtime(&tv.tv_sec);
	strftime(tmptime,30,"%H:%M:%S",tptr);
	printf("%s.%06i  ", tmptime, (unsigned int)tv.tv_usec);
}

//// Initialization
void init_serial(const unsigned char *new_rx_addr, unsigned char default_channel) {
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

	default_chan = default_channel;
	cmd_block = 0;
	cur_chan = 0;
	memset(tx_addr, 0, 5);
	memcpy(rx_addr, new_rx_addr, 5);
	state = STATE_INIT_PACKETLEN;
}


//// Sending
static void send_cmd(int cmd, const unsigned char* buffer, int len) {
	assert(!cmd_block);
#ifdef CMD_DEBUG
	print_time();
	printf("<= send cmd %c\n", cmd);
#endif

	static unsigned char magic[4] = { CMD_ESC, 0, CMD_ESC, CMD_END };

	magic[1] = cmd;
	assert(write(serial, magic, 2) == 2);
	usleep(250);
	int i;
	for(i = 0; i < len; i++) {
		assert(write(serial, &buffer[i], 1) == 1);
		usleep(150);
		if(buffer[i] == CMD_ESC) {
			assert(write(serial, &buffer[i], 1) == 1);
			usleep(150);
		}
	}
	assert(write(serial, magic + 2, 2) == 2);
	usleep(300);
	cmd_block = 1;
}


static unsigned short do_crc(void* load, int len) {
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

static void send_packet(struct Packet* p) {
	unsigned short crc = do_crc(p, 30);
	p->crc[0] = (crc >> 8) & 0xff;
	p->crc[1] = crc & 0xff;


#ifdef PACKET_DEBUG
	print_time();
	printf("<= send packet %c\n", p->command);
#endif
	send_cmd(CMD_PACKET, (unsigned char*)p, sizeof(struct Packet));
}

struct Packet* new_packet(unsigned char command)
{
	struct Packet *p = (struct Packet *)calloc(1, sizeof(struct Packet));
	p->len = 32;
	p->protocol = 'G';
	p->command = command;
	return p;
}


void queue_packet(struct Packet* p, unsigned char chan, const unsigned char* addr) {
	assert(p != NULL && chan != 0 && addr != 0 && addr[0] != 0);
	assert(state == STATE_IDLE && next_packet == 0);
#ifdef PACKET_DEBUG
	print_time();
	printf("<= queue packet %c\n", p->command);
#endif
	next_packet = p;
	next_chan = chan;
	next_addr = addr;
	state = STATE_SET_CHAN;
}


//// State machine and incoming package handling
static void process_cmd(unsigned char cmd, unsigned char* data, unsigned char len)
{
#ifdef CMD_DEBUG
	print_time();
	printf("=> recv cmd %c\n", cmd);
#endif
	struct Packet *packet = (struct Packet *)data;
	switch(cmd) {
		case CMD_OK:
			cmd_block = 0;
			return;
		case CMD_PACKET:
			if(len != packet->len) {
				fprintf(stderr, "len error\n");
				return;
			}
#ifdef PACKET_DEBUG
			print_time();
			printf("=> recv packet %c\n", packet->command);
#endif
			process_packet(packet);
			return;
		default:
			fprintf(stderr, "Unknown incoming command %d\n", cmd);
			abort();
	}
}

void seriel_receive()
{
	unsigned char data[256];
	unsigned char pos = 0;
	unsigned char byte;
	unsigned char cmd = CMD_OK;
	int esc = 0;
	
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
				process_cmd(cmd, data, pos);
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
}


int serial_do_work() {
	if (cmd_block) return 0;
	//printf("doing work, state %d\n", state);
	unsigned char len;
	switch(state) {
		// init
		case STATE_INIT_PACKETLEN:
			len = 32;
			send_cmd(CMD_PACKETLEN, &len, 1);
			state = STATE_INIT_RX_MAC;
			return 0;
		case STATE_INIT_RX_MAC:
			send_cmd(CMD_RX_MAC, rx_addr, 5);
			state = STATE_RESTORE_CHAN;
			return 0;
		
		// send something
		case STATE_SET_CHAN:
			if (cur_chan != next_chan) {
				send_cmd(CMD_CHAN, &next_chan, 1);
				cur_chan = next_chan;
				state = STATE_SET_MAC;
				return 0;
			}
			// correct channel already set: fall-through on purpose (state will be set below)
		case STATE_SET_MAC:
			if (memcmp(next_addr, tx_addr, 5) != 0) {
				send_cmd(CMD_TX_MAC, next_addr, 5);
				memcpy(tx_addr, next_addr, 5);
				state = STATE_SEND;
				return 0;
			}
			// correct addr already set: fall-through on purpose (state will be set below)
		case STATE_SEND:
			send_packet(next_packet);
			free(next_packet);
			next_packet = NULL;
			state = STATE_RESTORE_CHAN;
			return 0;
		
		// default: idle
		case STATE_RESTORE_CHAN:
			if (cur_chan != default_chan) {
				send_cmd(CMD_CHAN, &default_chan, 1);
				cur_chan = default_chan;
				state = STATE_IDLE;
				return 0;
			}
			// correct channel already set: fall-through on purpose
			state = STATE_IDLE;
		case STATE_IDLE:
			assert(next_packet == NULL);
			return 1;

		// must not happen
		default:
			fprintf(stderr, "Unknown state %d\n", state);
			abort();
	}
}

