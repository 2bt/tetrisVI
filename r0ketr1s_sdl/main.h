#ifndef MAIN_H_
#define MAIN_H_



#define PACK __attribute__((packed))

enum {
	CMD_ESC			= 92,
	CMD_END			= 48,//  '0'
	CMD_PACKET		= 49,//  '1'
	CMD_OK			= 50,//  '2'
	CMD_TX_MAC		= 51,//  '3'
	CMD_RX_MAC		= 52,//  '4'
	CMD_CHAN		= 53,//  '5'
	CMD_PACKETLEN	= 54,//  '6'
};

typedef struct {
	// 11 common bytes
	unsigned char len;			// 32
	unsigned char protocol;		// 'G'
	unsigned char command;		// 'A'nounce 'J'oin 'a'ck 'B'utton ... 'b'lob
	unsigned char id[4];
	unsigned char counter[4];	// network byte order

	union {
		struct {
			unsigned char game_mac[5];
			unsigned char game_chan;
			unsigned char game_id[2];

			// bit 1: mass game
			// bit 2: short packet
			// bit 3: long receive
			unsigned char game_flags;

			unsigned char interval;
			unsigned char jitter;
			unsigned char game_name[8];
		} PACK announce;

		struct {
			unsigned char game_id[2];
			unsigned char dummy[17];
		} PACK join;

		struct {
			// 0 = go way
			// 1 = accepted
			unsigned char flags;
			unsigned char dummy[18];
		} PACK ack;

		struct {
			unsigned char state;
			unsigned char dummy[18];
		} PACK button;

		struct {
			unsigned char dummy[19];
		} PACK nickrequest;

		struct {
			// flags currently unused 
			unsigned char flags;
			unsigned char nick[18];
		} PACK nick;

		struct {
			unsigned char x;
			unsigned char y;
			// bit 1: clear screen
			unsigned char flags; 
			unsigned char text[16];
		} PACK text;

		struct {
			unsigned char data[19];
		} PACK blob;

	};

	unsigned char crc[2];

} PACK Packet;

enum {
	// initialize
	STATE_INIT_PACKETLEN,
	STATE_INIT_RX_MAC,
	// default state
	STATE_RESTORE_CHAN, // game channel
	STATE_RESTORE_MAC, // game mac
	STATE_IDLE,
	// do ann announcement
	STATE_ANNOUNCE_SET_CHAN,
	STATE_ANNOUNCE_SET_MAC,
	STATE_ANNOUNCE_SEND,
	// join
// 	STATE_JOIN_ACK_SEND,
	// send some stuff
// 	STATE_DATA_SEND,
};




enum {
    DISPLAY_WIDTH = 72,
    DISPLAY_HEIGHT = 32
};

enum {
    BUTTON_A,
    BUTTON_DOWN,
    BUTTON_LEFT,
    BUTTON_RIGHT,
    BUTTON_B,
    BUTTON_UP,
    BUTTON_START,
    BUTTON_SELECT,
};

enum {
    MAX_PLAYERS = 6
};
    

int button_down(unsigned int nr, unsigned int button);
int is_occupied(unsigned int nr);
void push_lines(unsigned int nr, unsigned int lines);
unsigned int rand_int(unsigned int limit);
void pixel(int x, int y, unsigned char color);


#endif