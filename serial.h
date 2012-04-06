#ifndef SERIAL_H_
#define SERIAL_H_


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

struct Packet {
	// 11 common bytes
	unsigned char len;			// 32
	unsigned char protocol;		// 'G'
	unsigned char command;		// 'A'nounce 'J'oin 'a'ck 'B'utton ... 'b'lob
	unsigned int id;
	unsigned int counter;	// network byte order

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

} PACK;


void init_serial(const unsigned char *new_rx_addr, unsigned char default_channel);
void seriel_receive();
int serial_do_work(); // return whether we are idle
void queue_packet(struct Packet *p, unsigned char chan, const unsigned char *addr); // set channel and MAC and then send the given packet
struct Packet *new_packet(unsigned char command);


#endif