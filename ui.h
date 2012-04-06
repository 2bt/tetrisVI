#ifndef UI_H_
#define UI_H_


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

struct Packet;

// functions to be implemented by the UI for the grid to work
int button_down(unsigned int nr, unsigned int button);
int is_occupied(unsigned int nr);
void push_lines(unsigned int nr, unsigned int lines);
unsigned int rand_int(unsigned int limit);
void set_frame_buffer(int x, int y, unsigned char color);
void player_gameover(unsigned int nr);
void process_packet(struct Packet *packet);

#endif
