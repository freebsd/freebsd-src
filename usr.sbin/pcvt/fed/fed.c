/*
 * Copyright (c) 1992, 1993, 1994 by Hellmuth Michaelis
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Hellmuth Michaelis.
 * 4. The name of the developer may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * 	fed.c, 3.00, last edit-date: [Sun Jan  2 20:08:45 1994]
 */

/*---------------------------------------------------------------------------
 *
 *	fed.c		font editor main file
 *	-------------------------------------
 *
 *	written by Hellmuth Michaelis, hm@hcshh.hcs.de
 *
 *	-hm	first public release
 *	-hm	activating font save
 *
 *---------------------------------------------------------------------------*/

#define FED

#include "fed.h"

void main(int argc, char *argv[])
{
	int i;
	int row, col;
	int ret;

	if(argc != 2)
	{
		fprintf(stderr,"EGA/VGA Fonteditor, Rel 1.00\n");
		fprintf(stderr,"usage: %s <fontfilename>\n",argv[0]);
		exit(1);
	}

 	readfont(argv[1]);		/* read fontfile into memory */

	initscr();
	cbreak();
	noecho();
	nonl();
	keypad(stdscr,TRUE);
	idlok(stdscr, TRUE);

	move(0,0);
	standout();
	addstr("      Interactive EGA/VGA Fonteditor - (c) 1993, 1994 Hellmuth Michaelis        ");
	standend();

/* character horizontal ruler */

	move(WINROW-1, CHCOL + ((WIDTH16 - ch_width)/2) + 1);
	if(ch_width == WIDTH16)
		addstr("1234567890123456");
	else
		addstr("12345678");

/* charcater vertical ruler */

	for(i=1; i < ch_height+1; i++)
		mvprintw((WINROW+i), (CHCOL + ((WIDTH16 - ch_width)/2) - 2), "%2d", i);


/* select horizontal ruler */

	move(WINROW-1,SETCOL+2);
	addstr("0 1 2 3 4 5 6 7 8 9 A B C D E F ");

/* select vertical ruler */

	for(i=0; i<10; i++)
		mvaddch((WINROW+i+1),(SETCOL-1),(i+'0'));
	for(i=0; i<6; i++)
		mvaddch((WINROW+10+i+1),(SETCOL-1),(i+'A'));

/* label available commands window */

	move(WINROW-1,CMDCOL+1);
	addstr("Commands");

	refresh();

/* command window */

	cmd_win = newwin(((WSIZE)+(2*WBORDER)),(CMDSIZE+(2*WBORDER)),
				WINROW,CMDCOL);
	keypad(cmd_win,TRUE);
	idlok(cmd_win, TRUE);
	box(cmd_win,'|','-');

	sel_mode();

/* character font window */

	ch_win = newwin((ch_height+(2*WBORDER)),(ch_width+(2*WBORDER)),
				WINROW, CHCOL+((WIDTH16 - ch_width)/2));
	keypad(ch_win,TRUE);
	idlok(ch_win, TRUE);

	box(ch_win,'|','-');
	wrefresh(ch_win);

/* character select window */

	set_win = newwin((WSIZE+(2*WBORDER)),((WSIZE*2)+(2*WBORDER)),
				WINROW,SETCOL); /* whole character set */
	keypad(set_win,TRUE);
	idlok(set_win, TRUE);

	box(set_win,'|','-');

	row = 0;
	col = 0;

	for(i=0; i<256; i++)
	{
		mvwprintw(set_win,row+1,col+1,"%02.2X",i);
		if(++row > 15)
		{
			row = 0;
			col += 2;
		}
	}
	wmove(set_win,1,1);
	wrefresh(set_win);

/* start */

	clr_cmd();

	curchar = 0;

	if((ret = selectc()) == 1)
	{
		writefont();
	}
	endwin();
}

/*---------------------------------- E O F ----------------------------------*/
