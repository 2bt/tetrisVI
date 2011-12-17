
#include "../pixelfonts/5x3/font.h"
#include "../main.h"
#include "text.h"

void print_5x3_at (int x,int y, char *text, char brightness)
{
	while (*text)
	{
		putc_5x3_at(x,y,*text,brightness);
		x+=4;
		text++;
	}

}

void putc_5x3_at (int x,int y, char text, char brightness)
{
	text -= 32;

	for (int i = 0; i < 3; i++)
	{
		char ch = font5x3[(int)text][i];

		for (int j = 0; j < 5; j++)
		{
			if(ch & (1<<j))
			{
				pixel(x+i,y-j+4,brightness);
			}
			else
			{
				pixel(x+i,y-j+4,0);
			}
		}
	}
}
