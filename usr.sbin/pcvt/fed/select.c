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
 * 	select.c, 3.00, last edit-date: [Sun Jan  2 20:09:36 1994]
 */

/*---------------------------------------------------------------------------
 *
 *	select.c		font editor select character
 *	----------------------------------------------------
 *
 *	written by Hellmuth Michaelis, hm@hcshh.hcs.de
 *
 *	-hm	first public release
 *	-hm	debugging
 *
 *---------------------------------------------------------------------------*/

#include "fed.h"

int sc, sr, scurchar;

int edit();

void sel_mode(void)
{
	mvwprintw(cmd_win,1,1,"(E)dit      ");
	mvwprintw(cmd_win,2,1,"(M)ove      ");
	mvwprintw(cmd_win,3,1,"exchan(G)e  ");
	mvwprintw(cmd_win,4,1,"(Q)uit/Save ");
	mvwprintw(cmd_win,5,1,"e(X)it/Undo ");
	mvwprintw(cmd_win,6,1,"            ");
	mvwprintw(cmd_win,7,1,"            ");
	mvwprintw(cmd_win,8,1,"            ");

	mvwprintw(cmd_win,9 ,1,"            ");
	mvwprintw(cmd_win,10,1,"            ");
	mvwprintw(cmd_win,11,1,"            ");
	mvwprintw(cmd_win,12,1,"            ");
	mvwprintw(cmd_win,13,1,"(^P)rev Line");
	mvwprintw(cmd_win,14,1,"(^N)ext Line");
	mvwprintw(cmd_win,15,1,"(^F)orwd Col");
	mvwprintw(cmd_win,16,1,"(^B)ack  Col");
	wrefresh(cmd_win);
}

int selectc()
{
	int c, r;
	int ret;
	char h, l;
	unsigned int k_ch;

	c = (curchar / 16);
	r = (curchar % 16);

	for(;;)
	{
		dis_cmd("   Select Character");

		sel_mode();

		curchar = r + (c*16);

		display(curchar);

		h = ((mvwinch(set_win,(r+1),((c*2)+1))) & A_CHARTEXT);
		l = ((mvwinch(set_win,(r+1),((c*2)+2))) & A_CHARTEXT);
		wattron(set_win,A_REVERSE);
		mvwprintw(set_win,(r+1),((c*2)+1),"%c%c",h,l);
		wattroff(set_win,A_REVERSE);
		wmove(set_win,(r+1),((c*2)+1));
		wrefresh(set_win);

		k_ch = wgetch(set_win);

		switch(k_ch)
		{
			case K_LEFT:
			case KEY_LEFT:
				if(c > 0)
				{
					normal_set(r,c);
					c--;
				}
				break;

			case K_DOWN:
			case KEY_DOWN:
				if(r < 15)
				{
					normal_set(r,c);
					r++;
				}
				break;

			case K_UP:
			case KEY_UP:
				if(r > 0)
				{
					normal_set(r,c);
					r--;
				}
				break;

			case K_RIGHT:
			case KEY_RIGHT:
				if(c < 15)
				{
					normal_set(r,c);
					c++;
				}
				break;

			case 'e':
			case 'E':
				edit_mode();
				dis_cmd("   Edit Character");
				display(curchar);
				ret = edit();
				if(ret == 1)
					save_ch();
				break;

			case 'g':
			case 'G':
				dis_cmd("   Exchange: select Destination, then press RETURN or any other Key to ABORT");
				sr = r;
				sc = c;
				scurchar = curchar;
				if((curchar = sel_dest()) == -1)
				{ /* failsafe */
					r = sr;
					c = sc;
					curchar = scurchar;
				}
				else
				{ /* valid return */
					normal_set(r,c);
					c = (curchar / 16);
					r = (curchar % 16);
					xchg_ch(scurchar,curchar);
				}
				break;

			case 'm':
			case 'M':
				dis_cmd("   Move: select Destination, then press RETURN or any other Key to ABORT");
				sr = r;
				sc = c;
				scurchar = curchar;
				if((curchar = sel_dest()) == -1)
				{ /* failsafe */
					r = sr;
					c = sc;
					curchar = scurchar;
				}
				else
				{ /* valid return */
					normal_set(r,c);
					c = (curchar / 16);
					r = (curchar % 16);
					move_ch(scurchar,curchar);
				}
				break;

			case 'q':
			case 'Q':
				normal_set(r,c);
				wrefresh(set_win);
				return(1);
				break;

			case 'x':
			case 'X':
				normal_set(r,c);
				wrefresh(set_win);
				return(0);
				break;

			case 0x0c:
				wrefresh(curscr);
				break;

			default:
				beep();
				break;

		}
	}
}

void normal_set(int r, int c)
{
	char h, l;

	h = ((mvwinch(set_win,(r+1),((c*2)+1))) & A_CHARTEXT);
	l = ((mvwinch(set_win,(r+1),((c*2)+2))) & A_CHARTEXT);
	wattroff(set_win,A_REVERSE);
	mvwprintw(set_win,(r+1),((c*2)+1),"%c%c",h,l);
	wmove(set_win,(r+1),((c*2)+1));
}

int sel_dest(void)
{
	int c, r;
	char h, l;
	unsigned int k_ch;

	c = (curchar / 16);
	r = (curchar % 16);

	for(;;)
	{

		curchar = r + (c*16);

		display(curchar);

		h = ((mvwinch(set_win,(r+1),((c*2)+1))) & A_CHARTEXT);
		l = ((mvwinch(set_win,(r+1),((c*2)+2))) & A_CHARTEXT);
		wattron(set_win,A_UNDERLINE);
		mvwprintw(set_win,(r+1),((c*2)+1),"%c%c",h,l);
		wattroff(set_win,A_UNDERLINE);
		wmove(set_win,(r+1),((c*2)+1));
		wrefresh(set_win);

		k_ch = wgetch(set_win);

		switch(k_ch)
		{
			case K_LEFT:
			case KEY_LEFT:
				if(c > 0)
				{
					normal_uset(r,c);
					c--;
				}
				break;

			case K_DOWN:
			case KEY_DOWN:
				if(r < 15)
				{
					normal_uset(r,c);
					r++;
				}
				break;

			case K_UP:
			case KEY_UP:
				if(r > 0)
				{
					normal_uset(r,c);
					r--;
				}
				break;

			case K_RIGHT:
			case KEY_RIGHT:
				if(c < 15)
				{
					normal_uset(r,c);
					c++;
				}
				break;

			case '\r':
			case '\n':
				normal_uset(r,c);
				return(r + (c*16));

			case 0x0c:
				wrefresh(curscr);
				break;

			default:
				normal_uset(r,c);
				return(-1);
		}
	}
}

void normal_uset(int r, int c)
{
	char h, l;

	h = ((mvwinch(set_win,(r+1),((c*2)+1))) & A_CHARTEXT);
	l = ((mvwinch(set_win,(r+1),((c*2)+2))) & A_CHARTEXT);

	wattroff(set_win,A_UNDERLINE);
	mvwprintw(set_win,(r+1),((c*2)+1),"%c%c",h,l);
	wmove(set_win,(r+1),((c*2)+1));

	if((r==sr) && (c==sc))
	{
		wattron(set_win,A_REVERSE);
		mvwprintw(set_win,(r+1),((c*2)+1),"%c%c",h,l);
		wattroff(set_win,A_REVERSE);
		wmove(set_win,(r+1),((c*2)+1));
	}
}



/*---------------------------------- E O F ----------------------------------*/
