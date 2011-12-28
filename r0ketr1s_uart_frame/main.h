#ifndef MAIN_H_
#define MAIN_H_



#define PACK __attribute__((packed))

enum {
	CMD_ESC			= 92,
	CMD_END			= 48,
	CMD_PACKET		= 49,
	CMD_OK			= 50,
	CMD_TX_MAC		= 51,
	CMD_RX_MAC		= 52,
	CMD_CHAN		= 53,
	CMD_PACKETLEN	= 54,
};

typedef struct {
	// 11 common bytes
	unsigned char len;			// 32
	unsigned char protocol;		// 'G'
	unsigned char command;		// 'A'nounce 'J'oin 'a'ck 'B'utton ...
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

	};

	unsigned char crc[2];

} PACK Packet;

enum {
	STATE_INIT_PACKETLEN,
	STATE_INIT_RX_MAC,
	STATE_INIT_TX_MAC,
	STATE_INIT_CHAN,
	STATE_IDLE,
	STATE_ANNOUNCE_CHAN,
	STATE_ANNOUNCE_ANNOUNCE,
	STATE_ANNOUNCE_RESTORE_CHAN,
	STATE_JOIN_ACK_TX_MAC,
	STATE_JOIN_ACK_ACK,
	STATE_JOIN_ACK_RESTORE_TX_MAC,
	STATE_NICKREQUEST_TX_MAC,
	STATE_NICKREQUEST_NICKREQUEST,
	STATE_NICKREQUEST_RESTORE_TX_MAC,
	STATE_TEXT_TX_MAC,
	STATE_TEXT_TEXT,
	STATE_TEXT_RESTORE_TX_MAC,
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
void set_frame_buffer(int x, int y, unsigned char color);
void push_frame_buffer(void);

#endif