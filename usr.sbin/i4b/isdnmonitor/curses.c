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
 *	i4b daemon - curses fullscreen output
 *	-------------------------------------
 *
 *	$Id: curses.c,v 1.10 1999/12/13 21:25:25 hm Exp $ 
 *
 * $FreeBSD: src/usr.sbin/i4b/isdnmonitor/curses.c,v 1.1 1999/12/14 21:07:41 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 21:51:47 1999]
 *
 *---------------------------------------------------------------------------*/

#include "monprivate.h"

#ifndef WIN32

static void display_bell(void);
static void display_chans(void);

/*---------------------------------------------------------------------------*
 *	program exit
 *---------------------------------------------------------------------------*/
void
do_exit(int exitval)
{
	if(curses_ready)
		endwin();
	exit(exitval);
}

/*---------------------------------------------------------------------------*
 *	init curses fullscreen display
 *---------------------------------------------------------------------------*/
void
init_screen(void)
{
	char buffer[512];
	int uheight, lheight;
	int i, j;
	
	initscr();			/* curses init */
	
	if((COLS < 80) || (LINES < 24))
	{
		endwin();
		fprintf(stderr, "ERROR, minimal screensize must be 80x24, is %dx%d, terminating!",COLS, LINES);
		exit(1);
	}		

	noecho();
	raw();

	uheight = nctrl * 2; /* cards * b-channels */
	lheight = LINES - uheight - 6 + 1; /* rest of display */
	
	if((upper_w = newwin(uheight, COLS, UPPER_B, 0)) == NULL)
	{
		endwin();
		fprintf(stderr, "ERROR, curses init upper window, terminating!");
		exit(1);
	}

	if((mid_w = newwin(1, COLS, UPPER_B+uheight+1, 0)) == NULL)
	{
		endwin();
		fprintf(stderr, "ERROR, curses init mid window, terminating!");
		exit(1);
	}

	if((lower_w = newwin(lheight, COLS, UPPER_B+uheight+3, 0)) == NULL)
	{
		endwin();
		fprintf(stderr, "ERROR, curses init lower window, LINES = %d, lheight = %d, uheight = %d, terminating!", LINES, lheight, uheight);
		exit(1);
	}
	
	scrollok(lower_w, 1);

	sprintf(buffer, "----- isdn controller channel state ------------- isdnmonitor %02d.%02d.%d -", VERSION, REL, STEP);

	while(strlen(buffer) < COLS)
		strcat(buffer, "-");	

	move(0, 0);
	standout();
	addstr(buffer);
	standend();
	
	move(1, 0);
	/*      01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
	addstr("c tei b remote                 iface  dir outbytes   obps inbytes    ibps  units");
	
	if(hostname)
		sprintf(buffer, "----- isdn userland interface state ------------- %s:%d -", hostname, portno);
	else
		sprintf(buffer, "----- isdn userland interface state ------------- %s -", sockpath);
		
	while(strlen(buffer) < COLS)
		strcat(buffer, "-");	

	move(uheight+2, 0);
	standout();
	addstr(buffer);
	standend();

	sprintf(buffer, "----- isdnd logfile display --------------------------------------------------");
	while(strlen(buffer) < COLS)
		strcat(buffer, "-");	

	move(uheight+4, 0);
	standout();
	addstr(buffer);
	standend();
	
	refresh();

	for(i=0, j=0; i <= nctrl; i++, j+=2)
	{
		mvwprintw(upper_w, j,   H_CNTL, "%d --- 1 ", i);  /*TEI*/
		mvwprintw(upper_w, j+1, H_CNTL, "  L12 2 ");
	}
	wrefresh(upper_w);

#ifdef NOTDEF
	for(i=0, j=0; i < nentries; i++)	/* walk thru all entries */
	{
		p = &cfg_entry_tab[i];		/* get ptr to enry */

		mvwprintw(mid_w, 0, j, "%s%d ", bdrivername(p->usrdevicename), p->usrdeviceunit);

		p->fs_position = j;

		j += ((strlen(bdrivername(p->usrdevicename)) + (p->usrdeviceunit > 9 ? 2 : 1) + 1));
	}
#else
	mvwprintw(mid_w, 0, 0, "%s", devbuf);
#endif
	wrefresh(mid_w);

	wmove(lower_w, 0, 0);
	wrefresh(lower_w);

	curses_ready = 1;
}

/*---------------------------------------------------------------------------*
 *	display the charge in units
 *---------------------------------------------------------------------------*/
void
display_charge(int pos, int charge)
{
	mvwprintw(upper_w, pos, H_UNITS, "%d", charge);
	wclrtoeol(upper_w);	
	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	display the calculated charge in units
 *---------------------------------------------------------------------------*/
void
display_ccharge(int pos, int units)
{
	mvwprintw(upper_w, pos, H_UNITS, "(%d)", units);
	wclrtoeol(upper_w);	
	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	display accounting information
 *---------------------------------------------------------------------------*/
void
display_acct(int pos, int obyte, int obps, int ibyte, int ibps)
{
	mvwprintw(upper_w, pos, H_OUT,    "%-10d", obyte);
	mvwprintw(upper_w, pos, H_OUTBPS, "%-4d",  obps);
	mvwprintw(upper_w, pos, H_IN,     "%-10d", ibyte);
	mvwprintw(upper_w, pos, H_INBPS,  "%-4d",  ibps);
	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	erase line at disconnect time
 *---------------------------------------------------------------------------*/
void
display_disconnect(int pos)
{
	wmove(upper_w, pos, H_TELN);
	wclrtoeol(upper_w);
	wrefresh(upper_w);

	if(do_bell)
		display_bell();
}

/*---------------------------------------------------------------------------*
 *	display interface up/down information
 *---------------------------------------------------------------------------*/
void
display_updown(int pos, int updown, char *device)
{
	if(updown)
		wstandend(mid_w);
	else
		wstandout(mid_w);

	mvwprintw(mid_w, 0, pos, "%s ", device);

	wstandend(mid_w);
	wrefresh(mid_w);
}

/*---------------------------------------------------------------------------*
 *	display interface up/down information
 *---------------------------------------------------------------------------*/
void
display_l12stat(int controller, int layer, int state)
{
	if(controller > nctrl)
		return;
		
	if(!(layer == 1 || layer == 2))
		return;

	if(state)
		wstandout(upper_w);
	else
		wstandend(upper_w);

	if(layer == 1)
	{
		mvwprintw(upper_w, (controller*2)+1, H_TEI+1, "1");

		if(!state)
			mvwprintw(upper_w, (controller*2)+1, H_TEI+2, "2");
	}
	else if(layer == 2)
	{
		mvwprintw(upper_w, (controller*2)+1, H_TEI+2, "2");
		if(state)
			mvwprintw(upper_w, (controller*2)+1, H_TEI+1, "1");
	}

	wstandend(upper_w);
	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	display TEI
 *---------------------------------------------------------------------------*/
void
display_tei(int controller, int tei)
{
	if(controller > nctrl)
		return;

	if(tei == -1)
		mvwprintw(upper_w, controller*2, H_TEI, "---");
	else
		mvwprintw(upper_w, controller*2, H_TEI, "%3d", tei);

	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	display bell :-)
 *---------------------------------------------------------------------------*/
static void
display_bell(void)
{
	static char bell[1] = { 0x07 };
	write(STDOUT_FILENO, &bell[0], 1);
}

/*---------------------------------------------------------------------------*
 *	curses menu for fullscreen command mode
 *---------------------------------------------------------------------------*/
void
do_menu(void)
{
	static char *menu[WMITEMS] =
	{
		"1 - (D)isplay refresh",
		"2 - (H)angup (choose a channel)",
		"3 - (R)eread config file",		
		"4 - (Q)uit the program",		
	};

	WINDOW *menu_w;
	int c;
	int mpos;
	fd_set set;
	struct timeval timeout;

	/* create a new window in the lower screen area */
	
	if((menu_w = newwin(WMENU_HGT, WMENU_LEN, WMENU_POSLN, WMENU_POSCO )) == NULL)
	{
		return;
	}

	/* create a border around the window */
	
	box(menu_w, '|', '-');

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

		FD_ZERO(&set);
		FD_SET(STDIN_FILENO, &set);
		timeout.tv_sec = WMTIMEOUT;
		timeout.tv_usec = 0;

		/* if no char is available within timeout, exit menu*/
		
		if((select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout)) <= 0)
			goto mexit;
		
		c = wgetch(menu_w);

		switch(c)
		{
			case ' ':
			case '\t':	/* hilite next option */
				mvwaddstr(menu_w, mpos + 2, 2, menu[mpos]);
				mpos++;
				if(mpos >= WMITEMS)
					mpos = 0;
				wstandout(menu_w);
				mvwaddstr(menu_w, mpos + 2, 2, menu[mpos]);
				wstandend(menu_w);
				break;

			case ('0'+WREFRESH+1):	/* display refresh */
			case 'D':
			case 'd':
				wrefresh(curscr);
				goto mexit;

			case ('0'+WQUIT+1):	/* quit program */
			case 'Q':
			case 'q':
				do_exit(0);
				goto mexit;


			case ('0'+WHANGUP+1):	/* hangup connection */
			case 'H':
			case 'h':
				display_chans();
				goto mexit;

			case ('0'+WREREAD+1):	/* reread config file */
			case 'R':
			case 'r':
				reread();
				goto mexit;

			case '\n':
			case '\r':	/* exec highlighted option */
				switch(mpos)
				{
					case WREFRESH:
						wrefresh(curscr);
						break;

					case WQUIT:
						do_exit(0);
						break;

					case WHANGUP:
						display_chans();
						break;

					case WREREAD:
						reread();
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
	
	touchwin(lower_w);
	wrefresh(lower_w);
}

/*---------------------------------------------------------------------------*
 *	display connect information
 *---------------------------------------------------------------------------*/
void
display_connect(int pos, int dir, char *name, char *remtel, char *dev)
{
	char buffer[256];

	/* remote telephone number */

	sprintf(buffer, "%s/%s", name, remtel);
		
	buffer[H_IFN - H_TELN - 1] = '\0';

	mvwprintw(upper_w, pos, H_TELN, "%s", buffer);

	/* interface */
	
	mvwprintw(upper_w, pos, H_IFN, "%s ", dev);
	
	mvwprintw(upper_w, pos, H_IO, dir ? "out" : "in");

	mvwprintw(upper_w, pos, H_OUT,    "-");
	mvwprintw(upper_w, pos, H_OUTBPS, "-");
	mvwprintw(upper_w, pos, H_IN,     "-");
	mvwprintw(upper_w, pos, H_INBPS,  "-");

	if(do_bell)
		display_bell();
	
	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	display channel information for shutdown
 *---------------------------------------------------------------------------*/
static void
display_chans(void)
{
	char buffer[80];
	int i;
	int cnt = 0;
	WINDOW *chan_w;
	int nlines, ncols, pos_x, pos_y;
	fd_set set;
	struct timeval timeout;

	/* need this later to close the connection */
	struct ctlr_chan {
		int cntl;
		int chn;
	} *cc = NULL;

        for(i = 0; i < nctrl; i++)
        {
		if(remstate[i].ch1state)
	                cnt++;
		if(remstate[i].ch2state)
                        cnt++;
        }

	if(cnt > 0)
	{
		if ((cc = (struct ctlr_chan *)malloc (cnt *
			sizeof (struct ctlr_chan))) == NULL)
		{
			return;
		}
		nlines = cnt + 4;
		ncols = 60;
	}
	else
	{
		nlines = 5;
		ncols = 22;		
	}

	pos_y = WMENU_POSLN + 4;
	pos_x = WMENU_POSCO + 10;

	/* create a new window in the lower screen area */
	
	if((chan_w = newwin(nlines, ncols, pos_y, pos_x )) == NULL)
	{
		if (cnt > 0)
			free(cc);
		return;
	}

	/* create a border around the window */
	
	box(chan_w, '|', '-');

	/* add a title */
	
	wstandout(chan_w);
	mvwaddstr(chan_w, 0, (ncols / 2) - (strlen("Channels") / 2), "Channels");
	wstandend(chan_w);	

	/* no active channels */
	if (cnt == 0)
	{
		mvwaddstr(chan_w, 2, 2, "No active channels");
		wrefresh(chan_w);
		sleep(1);

		/* delete the channels window */

		delwin(chan_w);
		return;
	}

	nlines = 2;
	ncols = 1;

	for (i = 0; i < nctrl; i++)
	{
		if(remstate[i].ch1state)
		{
			sprintf(buffer, "%d - Controller %d channel %s", ncols, i, "B1");
			mvwaddstr(chan_w, nlines, 2, buffer);
			cc[ncols - 1].cntl = i;
			cc[ncols - 1].chn = CHAN_B1;
			nlines++;
			ncols++;
		}
		if(remstate[i].ch2state)		
		{
			sprintf(buffer, "%d - Controller %d channel %s", ncols, i, "B2");
			mvwaddstr(chan_w, nlines, 2, buffer);
			cc[ncols - 1].cntl = i;
			cc[ncols - 1].chn = CHAN_B2;
			nlines++;
			ncols++;
		}
	}

	for(;;)
	{
		wrefresh(chan_w);

		FD_ZERO(&set);
		FD_SET(STDIN_FILENO, &set);
		timeout.tv_sec = WMTIMEOUT;
		timeout.tv_usec = 0;

		/* if no char is available within timeout, exit menu*/
		
		if((select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout)) <= 0)
			break;
		
		ncols = wgetch(chan_w);

		if (!(isdigit(ncols)))
		{
			display_bell();
			continue;
		}

		nlines = ncols - '0';

		if ((nlines == 0) || (nlines > cnt))
		{
			display_bell();
			continue;
		}

		hangup(cc[nlines-1].cntl, cc[nlines-1].chn);
		break;
	}

	free(cc);

	/* delete the channels window */

	delwin(chan_w);
}

#endif /* !WIN32*/

/* EOF */
