
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
			unsigned char flags;
			unsigned char dummy[18];
		} PACK ack;

		struct {
			unsigned char state;
			unsigned char dummy[18];
		} PACK button;

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
};
