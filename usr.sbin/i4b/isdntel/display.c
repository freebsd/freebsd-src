/*
 * Copyright (c) 1997, 1999 Hellmuth Michaelis. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *---------------------------------------------------------------------------
 *
 *	isdntel - isdn4bsd telephone answering machine support
 *      ======================================================
 *
 *	$Id: display.c,v 1.7 1999/12/13 21:25:26 hm Exp $ 
 *
 * $FreeBSD: src/usr.sbin/i4b/isdntel/display.c,v 1.6 1999/12/14 21:07:44 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 21:53:57 1999]
 *
 *----------------------------------------------------------------------------*/

#include "defs.h"

static char *helpstr = "Enter Control-D to exit program or RETURN for command window";

/*---------------------------------------------------------------------------*
 *	init curses fullscreen display
 *---------------------------------------------------------------------------*/
void
init_screen(void)
{
	char buffer[512];
	
	initscr();			/* curses init */
	
	if((COLS < 80) || (LINES < 24))
		fatal(0, "ERROR, minimal screensize must be 80x24, is %dx%d, terminating!", COLS, LINES);

	
	if((main_w = newwin(LINES-START_O-2, COLS, START_O, 0)) == NULL)
		fatal("ERROR, curses init main window, terminating!");

	raw();					/* raw input */
	noecho();				/* do not echo input */
	keypad(stdscr, TRUE);			/* use special keys */
	keypad(main_w, TRUE);			/* use special keys */
	scrollok(main_w, TRUE);

	sprintf(buffer, " isdntel %d.%d.%d ", VERSION, REL, STEP);

	move(0, 0);
	standout();
	hline(ACS_HLINE, 5);
	move(0, 5);
	addstr(buffer);
	move(0, 5 + strlen(buffer));
	hline(ACS_HLINE, 256);
	standend();
	
	move(1, 0);
	addstr("Date     Time     Called Party     Calling Party    Alias                Length");	
           /*   31.12.96 16:45:12 1234567890123456 1234567890123456 12345678901234567890 123456 */

	move(2, 0);
	hline(ACS_HLINE, 256);
           
	move(LINES-2, 0);
	hline(ACS_HLINE, 256);

	mvaddstr(LINES-1, (COLS / 2) - (strlen(helpstr) / 2), helpstr);

	refresh();

	wrefresh(main_w);

	curses_ready = 1;
}

/*---------------------------------------------------------------------------*
 *	curses menu for fullscreen command mode
 *---------------------------------------------------------------------------*/
void
do_menu(void)
{
	static char *menu[WMITEMS] =
	{
		"Play   File",
#define PLAY	0		
		"Delete File",
#define DELETE	1
		"Re-Read Spool",
#define REREAD	2
		"Refresh Screen",
#define REFRESH 3
		"Exit Program",
#define EXIT	4
	};

	WINDOW *menu_w;
	int c;
	int mpos;

	/* create a new window in the lower screen area */
	
	if((menu_w = newwin(WMENU_HGT, WMENU_LEN, WMENU_POSLN, WMENU_POSCO )) == NULL)
		return;

	keypad(menu_w, TRUE);			/* use special keys */
		
	/* draw border around the window */
	
	box(menu_w, 0, 0);

	/* add a title */
	
	wstandout(menu_w);
	mvwaddstr(menu_w, 0, (WMENU_LEN / 2) - (strlen(WMENU_TITLE) / 2), WMENU_TITLE);
	wstandend(menu_w);	

	/* fill the window with the menu options */
	
	for(mpos=0; mpos <= (WMITEMS-1); mpos++)
		mvwaddstr(menu_w, mpos + 2, 2, menu[mpos]);

	/* highlight the first menu option */
	
	mpos = 0;
	wstandout(menu_w);
	mvwaddstr(menu_w, mpos + 2, 2, menu[mpos]);
	wstandend(menu_w);

	/* input loop */
	
	for(;;)
	{
		wrefresh(menu_w);

		c = wgetch(menu_w);

		switch(c)
		{
			case TAB:
			case KEY_DOWN:	/* down-move cursor */
			case ' ':
				mvwaddstr(menu_w, mpos + 2, 2, menu[mpos]);
				mpos++;
				if(mpos >= WMITEMS)
					mpos = 0;
				wstandout(menu_w);
				mvwaddstr(menu_w, mpos + 2, 2, menu[mpos]);
				wstandend(menu_w);
				break;

			case KEY_UP:	/* up-move cursor */
				mvwaddstr(menu_w, mpos + 2, 2, menu[mpos]);
				if(mpos)
					mpos--;
				else
					mpos = WMITEMS-1;
				wstandout(menu_w);
				mvwaddstr(menu_w, mpos + 2, 2, menu[mpos]);
				wstandend(menu_w);
				break;

			case 'R':
			case 'r':
				wrefresh(curscr);
				goto mexit;

			case 'E':
			case 'e':
			case 'Q':
			case 'q':
			case 'X':
			case 'x':
				do_quit(0);
				goto mexit;
				break;

			case 'P':
			case 'p':
				play(cur_file);
				goto mexit;
				break;

			case 'D':
			case 'd':
				delete(cur_file);
				goto mexit;				
				break;
				
			case CR:
			case LF:	/* exec highlighted option */
#ifdef KEY_ENTER
			case KEY_ENTER:
#endif
				switch(mpos)
				{
					case PLAY:
						play(cur_file);
						goto mexit;
						break;
					case DELETE:
						delete(cur_file);
						goto mexit;
						break;
					case REREAD:
						reread();
						goto mexit;
						break;
					case REFRESH:
						wrefresh(curscr);
						break;
					case EXIT:
						do_quit(0);
						break;
				}
				goto mexit;
				break;
		
			default:
				goto mexit;
				break;
		}
	}

mexit:
	/* delete the menu window */

	delwin(menu_w);

	/* re-display the original lower window contents */
	
	touchwin(main_w);
	wrefresh(main_w);
}

/* EOF */
