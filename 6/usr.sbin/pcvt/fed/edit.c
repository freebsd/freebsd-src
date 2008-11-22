/*
 * Copyright (c) 1992, 2000 Hellmuth Michaelis
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
 */

/*---------------------------------------------------------------------------
 *
 *	edit.c		font editor edit character
 *	------------------------------------------
 *
 * 	edit.c, 3.00, last edit-date: [Mon Mar 27 16:35:47 2000]
 *
 * $FreeBSD$
 *
 *---------------------------------------------------------------------------*/

#include "fed.h"

#define UP	0
#define DOWN	1

static int pen;

/*---------------------------------------------------------------------------*
 *	fill edit mode command window
 *---------------------------------------------------------------------------*/
void edit_mode(void)
{
	mvwprintw(cmd_win,1,1,"(W)hite     ");
	mvwprintw(cmd_win,2,1,"(Black      ");
	mvwprintw(cmd_win,3,1,"(I)nvert    ");
	mvwprintw(cmd_win,4,1,"(R)ow BLACK ");
	mvwprintw(cmd_win,5,1,"(r)ow WHITE ");
	mvwprintw(cmd_win,6,1,"(C)ol BLACK ");
	mvwprintw(cmd_win,7,1,"(c)ol WHITE ");
	mvwprintw(cmd_win,8,1,"(Q)uit/Save ");

	mvwprintw(cmd_win,9 ,1,"e(X)it/undo ");
	mvwprintw(cmd_win,10,1,"Pen (U)p    ");
	mvwprintw(cmd_win,11,1,"Pen (D)own  ");
	mvwprintw(cmd_win,12,1,"            ");
	mvwprintw(cmd_win,13,1,"(^P)rev Line");
	mvwprintw(cmd_win,14,1,"(^N)ext Line");
	mvwprintw(cmd_win,15,1,"(^F)orwd Col");
	mvwprintw(cmd_win,16,1,"(^B)ack  Col");
	wrefresh(cmd_win);
}

/*---------------------------------------------------------------------------*
 *	edit mode command loop
 *---------------------------------------------------------------------------*/
int edit(void)
{
	int c, r;
	char l;
	unsigned int k_ch;

	c = r = 0;

	pen = UP;

	for(;;)
	{
		if(pen == DOWN)
			dis_cmd("   Edit Mode, the Pen is DOWN");
		else
			dis_cmd("   Edit Mode, the Pen is UP");

		l = ((mvwinch(ch_win,(r+1),(c+1))) & A_CHARTEXT);
		wattron(ch_win,A_REVERSE);
		mvwprintw(ch_win,(r+1),(c+1),"%c",l);
		wattroff(ch_win,A_REVERSE);
		wmove(ch_win,(r+1),(c+1));
		wrefresh(ch_win);

		k_ch = wgetch(ch_win);

		switch(k_ch)
		{
			case K_LEFT:
			case KEY_LEFT:
				if(c > 0)
				{
					normal_ch(r,c);
					c--;
				}
				break;

			case K_DOWN:
			case KEY_DOWN:
				if(r < (ch_height-1))
				{
					normal_ch(r,c);
					r++;
				}
				break;

			case K_UP:
			case KEY_UP:
				if(r > 0)
				{
					normal_ch(r,c);
					r--;
				}
				break;

			case K_RIGHT:
			case KEY_RIGHT:
				if(c < (ch_width-1))
				{
					normal_ch(r,c);
					c++;
				}
				break;

			case KEY_HOME:
				normal_ch(r,c);
				c = r = 0;
				break;

			case KEY_LL:
				normal_ch(r,c);
				c = ch_width-1;
				r = ch_height-1;
				break;

			case 0x0c:
				wrefresh(curscr);
				break;

			case '\n':
			case '\r':
			case ' ' :
				chg_pt(r,c);
				break;

			case 'q':
				pen = UP;
				normal_ch(r,c);
				wrefresh(ch_win);
				return(1);
				break;

			case 'x':
				pen = UP;
				normal_ch(r,c);
				wrefresh(ch_win);
				return(0);
				break;

			case 'w':
			case 'W':
				setchr(WHITE);
				break;

			case 'b':
			case 'B':
				setchr(BLACK);
				break;

			case 'i':
			case 'I':
				invert();
				break;

			case 'r':
				setrow(WHITE);
				break;

			case 'R':
				setrow(BLACK);
				break;

			case 'c':
				setcol(WHITE);
				break;

			case 'C':
				setcol(BLACK);
				break;

			case 'u':
			case 'U':
				pen = UP;
				break;

			case 'd':
			case 'D':
				pen = DOWN;
				break;

			default:
				beep();
				break;

		}
	}
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
void normal_ch(int r, int c)
{
	char l = ((mvwinch(ch_win,(r+1),(c+1))) & A_CHARTEXT);
	wattroff(ch_win,A_REVERSE);
	if(pen == DOWN)
		mvwprintw(ch_win,(r+1),(c+1),"*");
	else
		mvwprintw(ch_win,(r+1),(c+1),"%c",l);
	wmove(ch_win,(r+1),(c+1));
}

/*---------------------------------------------------------------------------*
 *
 *---------------------------------------------------------------------------*/
void chg_pt(int r, int c)
{
	char l;
	l = ((mvwinch(ch_win,(r+1),(c+1))) & A_CHARTEXT);
	if(l == WHITE)
		l = BLACK;
	else
		l = WHITE;
	mvwprintw(ch_win,(r+1),(c+1),"%c",l);
	wmove(ch_win,(r+1),(c+1));
}

/*---------------------------------------------------------------------------*
 *	invert current character
 *---------------------------------------------------------------------------*/
void invert(void)
{
	int r,c;

	r = 1;

	while(r <= ch_height)
	{
		c = 1;
		while(c <= ch_width)
		{
			if(WHITE == mvwinch(ch_win, r, c))
				mvwaddch(ch_win, r, c, BLACK);
			else
				mvwaddch(ch_win, r, c, WHITE);
			c++;
		}
		r++;
	}
}

/*---------------------------------------------------------------------------*
 *	fill current character black/white
 *---------------------------------------------------------------------------*/
void setchr(char type)
{
	int r,c;

	r = 1;

	while(r <= ch_height)
	{
		c = 1;
		while(c <= ch_width)
		{
			mvwaddch(ch_win, r, c, type);
			c++;
		}
		r++;
	}
}

/*---------------------------------------------------------------------------*
 *	set current row to black/white
 *---------------------------------------------------------------------------*/
void setrow(char type)
{
	int r,c;

	getyx(ch_win,r,c);

	c = 1;

	while(c <= ch_width)
	{
		mvwaddch(ch_win, r, c, type);
		c++;
	}
}

/*---------------------------------------------------------------------------*
 *	set current column to black/white
 *---------------------------------------------------------------------------*/
void setcol(char type)
{
	int r,c;

	getyx(ch_win,r,c);

	r = 1;

	while(r <= ch_height)
	{
		mvwaddch(ch_win, r, c, type);
		r++;
	}
}

/*---------------------------------- E O F ----------------------------------*/
