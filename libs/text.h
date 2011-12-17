#ifndef TEXT_H_
#define TEXT_H_

void print_5x3_at (int x, int y, char *text, char brightness);
void putc_5x3_at (int x, int y, char text, char brightness);
void print_num_5x3_at (int x, int y, int number, int length, int pad, char brightness);

void print_unsigned_5x3_at (int x, int y, unsigned int number, int length, char pad, char brightness);


#endif
