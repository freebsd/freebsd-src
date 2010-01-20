/*
 * Copyright (c) 1999 Hellmuth Michaelis. All rights reserved.
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
 *	isdnphone - some display operations
 *      ===================================
 *
 *	$Id: display.c,v 1.4 1999/12/13 21:25:26 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:52:55 1999]
 *
 *----------------------------------------------------------------------------*/

#include "defs.h"

/*---------------------------------------------------------------------------*
 *	init curses fullscreen display
 *---------------------------------------------------------------------------*/
void
init_mainw(void)
{
	char buffer[512];
	
	initscr();			/* curses init */
	
	if((COLS < 80) || (LINES < 24))
		fatal(0, "ERROR, minimal screensize must be 80x24, is %dx%d, terminating!", COLS, LINES);

	
	if((main_w = newwin(MW_HEIGHT, MW_WIDTH, MW_ROW, MW_COL)) == NULL)
		fatal("ERROR, curses init main window, terminating!");

	if(opt_d)
	{
		if((dbg_w = newwin(DB_HGT, DB_WID, DB_ROW, DB_COL)) == NULL)
			fatal("ERROR, curses init debug window, terminating!");
		scrollok(dbg_w, TRUE);
	}

	raw();					/* raw input */
	noecho();				/* do not echo input */
	keypad(stdscr, TRUE);			/* use special keys */
	keypad(main_w, TRUE);			/* use special keys */

	box(main_w, 0, 0);

	sprintf(buffer, "isdnphone %d.%d ", VERSION, REL);

	wstandout(main_w);	
	mvwaddstr(main_w, 0,  (MW_WIDTH / 2) - (strlen(buffer) / 2), buffer);
	wstandend(main_w);	
	
	mvwaddstr(main_w, MW_STATEY, MW_STATEX, "  state: ");
	mvwprintw(main_w, MW_STATEY, MW_STX, "%s", states[state]);	
	wmove(main_w, MW_STATEY+1, 1);
	whline(main_w, 0, MW_WIDTH-2);
	
	mvwaddstr(main_w, MW_NUMY, MW_NUMX, " number: ");
	wmove(main_w, MW_NUMY+1, 1);
	whline(main_w, 0, MW_WIDTH-2);
	
	mvwaddstr(main_w, MW_MSGY, MW_MSGX, "message: ");

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
		"Hangup",
#define HANGUP	0		
		"Dial",
#define DIAL	1
		"Refresh",
#define REFRESH	2
		"Exit",
#define EXIT	3
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

			case 'H':
			case 'h':
				do_hangup();
				goto mexit;
				break;

			case 'D':
			case 'd':
				goto mexit;				
				break;
				
			case CR:
			case LF:	/* exec highlighted option */
#ifdef KEY_ENTER
			case KEY_ENTER:
#endif
				switch(mpos)
				{
					case DIAL:
						goto mexit;
						break;
					case HANGUP:
						do_hangup();
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

	wclear(menu_w);
	wrefresh(menu_w);
	delwin(menu_w);

	/* re-display the original lower window contents */
	
	touchwin(main_w);
	wrefresh(main_w);
}

/* EOF */
