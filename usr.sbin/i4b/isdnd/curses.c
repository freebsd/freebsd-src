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
 *	i4b daemon - curses fullscreen output
 *	-------------------------------------
 *
 *	$Id: curses.c,v 1.29 1999/12/13 21:25:24 hm Exp $ 
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:45:43 1999]
 *
 *---------------------------------------------------------------------------*/

#ifdef USE_CURSES

#include "isdnd.h"

#define CHPOS(cfgp) (((cfgp)->isdncontrollerused*2) + (cfgp)->isdnchannelused)

/*---------------------------------------------------------------------------*
 *	init curses fullscreen display
 *---------------------------------------------------------------------------*/
void
init_screen(void)
{
	char buffer[512];
	int uheight, lheight;
	int i, j;
	cfg_entry_t *p;
	
	initscr();			/* curses init */
	
	if((COLS < 80) || (LINES < 24))
	{
		log(LL_ERR, "ERROR, minimal screensize must be 80x24, is %dx%d, terminating!",COLS, LINES);
		do_exit(1);
	}		

	noecho();
	raw();

	uheight = ncontroller * 2; /* cards * b-channels */
	lheight = LINES - uheight - 6 + 1; /* rest of display */
	
	if((upper_w = newwin(uheight, COLS, UPPER_B, 0)) == NULL)
	{
		log(LL_ERR, "ERROR, curses init upper window, terminating!");
		exit(1);
	}

	if((mid_w = newwin(1, COLS, UPPER_B+uheight+1, 0)) == NULL)
	{
		log(LL_ERR, "ERROR, curses init mid window, terminating!");
		exit(1);
	}

	if((lower_w = newwin(lheight, COLS, UPPER_B+uheight+3, 0)) == NULL)
	{
		log(LL_ERR, "ERROR, curses init lower window, LINES = %d, lheight = %d, uheight = %d, terminating!", LINES, lheight, uheight);
		exit(1);
	}
	
	scrollok(lower_w, 1);

	snprintf(buffer, sizeof(buffer), "----- isdn controller channel state ------------- isdnd %02d.%02d.%d [pid %d] -", VERSION, REL, STEP, (int)getpid());	

	while(strlen(buffer) < COLS && strlen(buffer) < sizeof(buffer) - 1)
		strcat(buffer, "-");	

	move(0, 0);
	standout();
	addstr(buffer);
	standend();
	
	move(1, 0);
	/*      01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
	addstr("c tei b remote                 iface  dir outbytes   obps inbytes    ibps  units");
	
	snprintf(buffer, sizeof(buffer), "----- isdn userland interface state ------------------------------------------");	
	while(strlen(buffer) < COLS && strlen(buffer) < sizeof(buffer) - 1)
		strcat(buffer, "-");	

	move(uheight+2, 0);
	standout();
	addstr(buffer);
	standend();

	snprintf(buffer, sizeof(buffer), "----- isdnd logfile display --------------------------------------------------");
	while(strlen(buffer) < COLS && strlen(buffer) < sizeof(buffer) - 1)
		strcat(buffer, "-");	

	move(uheight+4, 0);
	standout();
	addstr(buffer);
	standend();
	
	move(uheight+5, 0);
	addstr("Date     Time     Typ Information   Description");	

	refresh();

	for(i=0, j=0; i <= ncontroller; i++, j+=2)
	{
		if(isdn_ctrl_tab[i].tei == -1)
			mvwprintw(upper_w, j,   H_CNTL, "%d --- 1 ", i);
		else
			mvwprintw(upper_w, j,   H_CNTL, "%d %3d 1 ", i, isdn_ctrl_tab[i].tei);
		mvwprintw(upper_w, j+1, H_CNTL, "  L12 2 ");
	}
	wrefresh(upper_w);

	for(i=0, j=0; i < nentries; i++)	/* walk thru all entries */
	{
		p = &cfg_entry_tab[i];		/* get ptr to enry */

		mvwprintw(mid_w, 0, j, "%s%d ", bdrivername(p->usrdevicename), p->usrdeviceunit);

		p->fs_position = j;

		j += ((strlen(bdrivername(p->usrdevicename)) + (p->usrdeviceunit > 9 ? 2 : 1) + 1));
	}
	wrefresh(mid_w);

	wmove(lower_w, 0, 0);
	wrefresh(lower_w);

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
		log(LL_WRN, "ERROR, curses init menu window!");
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
				rereadconfig(42);
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
						rereadconfig(42);
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
 *	display the charge in units
 *---------------------------------------------------------------------------*/
void
display_charge(cfg_entry_t *cep)
{
	mvwprintw(upper_w, CHPOS(cep), H_UNITS, "%d", cep->charge);
	wclrtoeol(upper_w);	
	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	display the calculated charge in units
 *---------------------------------------------------------------------------*/
void
display_ccharge(cfg_entry_t *cep, int units)
{
	mvwprintw(upper_w, CHPOS(cep), H_UNITS, "(%d)", units);
	wclrtoeol(upper_w);	
	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	display accounting information
 *---------------------------------------------------------------------------*/
void
display_acct(cfg_entry_t *cep)
{
	mvwprintw(upper_w, CHPOS(cep), H_OUT,    "%-10d", cep->outbytes);
	mvwprintw(upper_w, CHPOS(cep), H_OUTBPS, "%-4d", cep->outbps);
	mvwprintw(upper_w, CHPOS(cep), H_IN,     "%-10d", cep->inbytes);
	mvwprintw(upper_w, CHPOS(cep), H_INBPS,  "%-4d", cep->inbps);
	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	display connect information
 *---------------------------------------------------------------------------*/
void
display_connect(cfg_entry_t *cep)
{
	char buffer[256];

	/* remote telephone number */

	if(aliasing)
	{
		if(cep->direction == DIR_IN)
			snprintf(buffer, sizeof(buffer), "%s", get_alias(cep->real_phone_incoming));
		else
			snprintf(buffer, sizeof(buffer), "%s", get_alias(cep->remote_phone_dialout));
	}
	else
	{
		if(cep->direction == DIR_IN)
			snprintf(buffer, sizeof(buffer), "%s/%s", cep->name, cep->real_phone_incoming);
		else
			snprintf(buffer, sizeof(buffer), "%s/%s", cep->name, cep->remote_phone_dialout);	
	}
		
	buffer[H_IFN - H_TELN - 1] = '\0';

	mvwprintw(upper_w, CHPOS(cep), H_TELN, "%s", buffer);

	/* interface */
	
	mvwprintw(upper_w, CHPOS(cep), H_IFN, "%s%d ",
			bdrivername(cep->usrdevicename), cep->usrdeviceunit);
	
	mvwprintw(upper_w, CHPOS(cep), H_IO,
		cep->direction == DIR_OUT ? "out" : "in");

	mvwprintw(upper_w, CHPOS(cep), H_OUT,    "-");
	mvwprintw(upper_w, CHPOS(cep), H_OUTBPS, "-");
	mvwprintw(upper_w, CHPOS(cep), H_IN,     "-");
	mvwprintw(upper_w, CHPOS(cep), H_INBPS,  "-");

	if(do_bell)
		display_bell();
	
	wrefresh(upper_w);
}

/*---------------------------------------------------------------------------*
 *	erase line at disconnect time
 *---------------------------------------------------------------------------*/
void
display_disconnect(cfg_entry_t *cep)
{
	wmove(upper_w, CHPOS(cep),
		 H_TELN);
	wclrtoeol(upper_w);
	wrefresh(upper_w);

	if(do_bell)
		display_bell();
	
}

/*---------------------------------------------------------------------------*
 *	display interface up/down information
 *---------------------------------------------------------------------------*/
void
display_updown(cfg_entry_t *cep, int updown)
{
	if(updown)
		wstandend(mid_w);
	else
		wstandout(mid_w);

	mvwprintw(mid_w, 0, cep->fs_position, "%s%d ",
			bdrivername(cep->usrdevicename), cep->usrdeviceunit);

	wstandend(mid_w);
	wrefresh(mid_w);
}

/*---------------------------------------------------------------------------*
 *	display interface up/down information
 *---------------------------------------------------------------------------*/
void
display_l12stat(int controller, int layer, int state)
{
	if(controller > ncontroller)
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
	if(controller > ncontroller)
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
void
display_bell(void)
{
	static char bell[1] = { 0x07 };
	write(STDOUT_FILENO, &bell[0], 1);
}

/*---------------------------------------------------------------------------*
 *	display channel information for shutdown
 *---------------------------------------------------------------------------*/
void
display_chans(void)
{
	char buffer[80];
	int i;
	int cnt = 0;
	WINDOW *chan_w;
	int nlines, ncols, pos_x, pos_y;
	fd_set set;
	struct timeval timeout;
	cfg_entry_t *cep = NULL;

	/* need this later to close the connection */
	struct ctlr_chan {
		int cntl;
		int chn;
	} *cc = NULL;

	for (i = 0; i < ncontroller; i++)
	{
		if((get_controller_state(i)) != CTRL_UP)
			continue;
		if((ret_channel_state(i, CHAN_B1)) == CHAN_RUN)
			cnt++;
		if((ret_channel_state(i, CHAN_B2)) == CHAN_RUN)
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
		log(LL_WRN, "ERROR, curses init channel window!");
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

	for (i = 0; i < ncontroller; i++)
	{
		if((get_controller_state(i)) != CTRL_UP)
			continue;

		if((ret_channel_state(i, CHAN_B1)) == CHAN_RUN)
		{
			snprintf(buffer, sizeof(buffer), "%d - Controller %d channel %s", ncols, i, "B1");
			mvwaddstr(chan_w, nlines, 2, buffer);
			cc[ncols - 1].cntl = i;
			cc[ncols - 1].chn = CHAN_B1;
			nlines++;
			ncols++;
		}
		if((ret_channel_state(i, CHAN_B2)) == CHAN_RUN)
		{
			snprintf(buffer, sizeof(buffer), "%d - Controller %d channel %s", ncols, i, "B2");
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

		if((cep = get_cep_by_cc(cc[nlines-1].cntl, cc[nlines-1].chn))
			!= NULL)
		{
			log(LL_CHD, "%05d %s manual disconnect (fullscreen menu)", cep->cdid, cep->name);
			cep->hangup = 1;
			break;
		}
	}

	free(cc);

	/* delete the channels window */

	delwin(chan_w);
}

#endif

/* EOF */
