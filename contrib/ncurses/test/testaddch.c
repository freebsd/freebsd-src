/*
 * This is an example written by Alexander V. Lukyanov <lav@yars.free.net>,
 * to demonstrate an inconsistency between ncurses and SVr4 curses.
 *
 * $Id: testaddch.c,v 1.3 1997/10/18 21:35:15 tom Exp $
 */
#include <test.priv.h>

static void attr_addstr(const char *s, chtype a)
{
	while(*s)
		addch(((unsigned char)(*s++))|a);
}

int
main(
	int argc GCC_UNUSED,
	char *argv[] GCC_UNUSED)
{
	unsigned i;
	chtype back,set,attr;
	
	initscr();
	start_color();
	init_pair(1,COLOR_WHITE,COLOR_BLUE);
	init_pair(2,COLOR_WHITE,COLOR_RED);
	init_pair(3,COLOR_BLACK,COLOR_MAGENTA);
	init_pair(4,COLOR_BLACK,COLOR_GREEN);
	init_pair(5,COLOR_BLACK,COLOR_CYAN);
	init_pair(6,COLOR_BLACK,COLOR_YELLOW);
	init_pair(7,COLOR_BLACK,COLOR_WHITE);

	for(i=0; i<8; i++)
	{
		back = (i&1) ? A_BOLD|'B' : ' ';
		set = (i&2) ? A_REVERSE : 0;
		attr = (i&4) ? COLOR_PAIR(4) : 0;

		bkgdset(back);
		attrset(set);
		
		attr_addstr("Test string with spaces ->   <-\n",attr);
	}
	addch('\n');
	for(i=0; i<8; i++)
	{
		back = (i&1) ? A_BOLD|'B'|COLOR_PAIR(1) : ' ';
		set = (i&2) ? A_REVERSE|COLOR_PAIR(2) : 0;
		attr = (i&4) ? COLOR_PAIR(4) : 0;

		bkgdset(back);
		attrset(set);
		
		attr_addstr("Test string with spaces ->   <-\n",attr);
	}

	getch();
	endwin();
	return EXIT_SUCCESS;
}
