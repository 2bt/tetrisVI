#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../pixelfonts/5x3/font.h"
#include "../main.h"
#include "text.h"

void print_5x3_at (int x,int y, char *text, unsigned char brightness)
{
	while (*text)
	{
		putc_5x3_at(x,y,*text,brightness);
		x+=4;
		text++;
	}

}

void putc_5x3_at (int x,int y, char text, unsigned char brightness)
{
	text -= 32;

	int i;
	for (i = 0; i < 3; i++)
	{
		char ch = font5x3[(int)text][i];

		int j;
		for (j = 0; j < 5; j++)
		{
			if(ch & (1<<j))
			{
				set_frame_buffer(x+i,y-j+4,brightness);
			}
			else
			{
				set_frame_buffer(x+i,y-j+4,0);
			}
		}
	}
}

void print_unsigned_5x3_at(int x, int y, unsigned int number, int length, char pad, unsigned char brightness) {
	x += length * 4 - 1;
	int i;
	for(i = 0; i < length; i++) {
		int d = number % 10;
		putc_5x3_at(
			x -= 4, y,
			!i || number > 0 ? '0' + d : pad,
			brightness);
		number /= 10;
	}
}


void print_num_5x3_at (int x, int y, int number, int length, int pad, unsigned char brightness)
{

	char s[10];
	sprintf(s, "%i", number);
	int len = strlen(s);

	if (length < len) {
		int i;
		for (i = 0; i < length; i++) {
			putc_5x3_at (x += 4, y, '*', brightness);
		}
		return;
	}
	int i;
	for (i = 0; i < length - len; i++) {
		putc_5x3_at (x += 4, y, pad, brightness);
	}
	print_5x3_at(x, y, (char*)s, brightness);

}

