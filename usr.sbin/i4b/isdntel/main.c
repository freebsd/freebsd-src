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
 *      $Id: main.c,v 1.12 1999/12/13 21:25:26 hm Exp $ 
 *
 * $FreeBSD: src/usr.sbin/i4b/isdntel/main.c,v 1.8 1999/12/14 21:07:44 hm Exp $
 *
 *      last edit-date: [Mon Dec 13 21:54:26 1999]
 *
 *----------------------------------------------------------------------------*/

#define MAIN
#include <locale.h>
#include "defs.h"
#include "alias.h"

static void usage( void );

static int top_dis = 0;
static int bot_dis = 0;
static int cur_pos_scr = 0;

static void makecurrent(int cur_pos, struct onefile *cur_file, int cold);

/*---------------------------------------------------------------------------*
 *      program entry
 *---------------------------------------------------------------------------*/
int
main(int argc, char **argv)
{
        int i;
	int kchar;
	
	char *spooldir = SPOOLDIR;
	char *playstring = PLAYCMD;
	char *aliasfile = ALIASFILE;
	int rrtimeout = REREADTIMEOUT;
	
	extern char *optarg;	

	setlocale( LC_ALL, "");
	
	while ((i = getopt(argc, argv, "a:d:p:t:")) != -1)
	{
		switch (i)
		{
			case 'a':
				aliasfile = optarg;
				break;

			case 'd':
				spooldir = optarg;
				break;

			case 'p':
				playstring = optarg;
				break;

			case 't':
				if(isdigit(*optarg))
				{
					rrtimeout = strtoul(optarg, NULL, 10);
				}
				else
				{
					usage();
				}
				break;

			case '?':
			default:
				usage();
				break;
		}
	}

	if(rrtimeout < 10)
		rrtimeout = 10;

	if((chdir(spooldir)) != 0)
		fatal("cannot change directory to spooldir %s!", spooldir);

	init_alias(aliasfile);
	
	init_screen();

	init_files(0);
	
	/* go into loop */

	for (;;)
	{
		fd_set set;
		struct timeval timeout;

		FD_ZERO(&set);
		FD_SET(STDIN_FILENO, &set);
		timeout.tv_sec = rrtimeout;
		timeout.tv_usec = 0;

		/* if no char is available within timeout, reread spool */
		
		if((select(STDIN_FILENO + 1, &set, NULL, NULL, &timeout)) <= 0)
		{
			reread();
			continue;
		}

		kchar = wgetch(main_w);		/* get char */

		switch (kchar)
		{
			case CR:
			case LF:
#ifdef KEY_ENTER
			case KEY_ENTER:
#endif
				do_menu();
				break;

			case KEY_UP:	/* up-move cursor */
				if(cur_file && cur_file->prev)
				{
					cur_file = cur_file->prev;
					cur_pos--;
				}
				break;


			case TAB:
			case KEY_DOWN:	/* down-move cursor */
				if(cur_file && cur_file->next)
				{
					cur_file = cur_file->next;
					cur_pos++;
				}
				break;

			case KEY_HOME:	/* move cursor to first dir */
				break;

			case KEY_LL:	/* move cursor to last file */
				break;

			case CNTRL_D:
				do_quit(0);
				break;

			case CNTRL_L:	/* refresh */
				touchwin(curscr);
				wrefresh(curscr);
				break;

		}
		makecurrent(cur_pos, cur_file, 0);
	}

	do_quit(0);
	
	return(0);
}

/*---------------------------------------------------------------------------*
 *      handle horizontal selection bar movement
 *---------------------------------------------------------------------------*/
static void
makecurrent(int cur_pos, struct onefile *cur_file, int cold)
{
	static int lastpos;
	static struct onefile *lastfile;
	char buffer[256];

	/* un-higlight current horizontal bar */

	if(!cold && lastfile && cur_file)
	{
		sprintf(buffer, "%s %s %-16s %-16s %-20s %-6s%*s",
			lastfile->date, lastfile->time,
			lastfile->dstnumber, lastfile->srcnumber,
			lastfile->alias == NULL ? "-/-" : lastfile->alias,
			lastfile->seconds,
			COLS - LAST_POS - 2, "");
			
		wattroff(main_w, A_REVERSE);
		mvwprintw(main_w, lastpos, 0, "%s", buffer);
		wattroff(main_w, A_REVERSE);
	}

	if(cur_file == NULL)
	{
		lastpos = cur_pos_scr;
		lastfile = cur_file;
		return;
	}
		
	/* have to scroll up or down ? */

	if(cur_pos >= bot_dis)		
	{
		/* scroll up */

	    	wscrl(main_w, 1);

	    	bot_dis++;
	    	top_dis++;
	    	cur_pos_scr = LINES-START_O-3;
	}
	else if(cur_pos < top_dis)
	{
		/* scroll down */

	    	wscrl(main_w, -1);

	    	bot_dis--;
	    	top_dis--;
	    	cur_pos_scr = 0;	    	
	}
	else
	{
		cur_pos_scr = cur_pos - top_dis;
	}		

	sprintf(buffer, "%s %s %-16s %-16s %-20s %-6s%*s",
			cur_file->date, cur_file->time,
			cur_file->dstnumber, cur_file->srcnumber,
			cur_file->alias == NULL ? "-/-" : cur_file->alias,
			cur_file->seconds,
			COLS - LAST_POS - 2, "");
			
	wattron(main_w, A_REVERSE);
	mvwprintw(main_w, cur_pos_scr, 0, "%s", buffer);
	wattroff(main_w, A_REVERSE);

	lastpos = cur_pos_scr;
	lastfile = cur_file;

	wrefresh(main_w);	
}

/*---------------------------------------------------------------------------*
 *	exit program
 *---------------------------------------------------------------------------*/
void
do_quit(int exitval)
{
	move(LINES-1, 0);
	clrtoeol();
	refresh();
	endwin();
	exit(exitval);
}

/*---------------------------------------------------------------------------*
 *	usage display and exit
 *---------------------------------------------------------------------------*/
static void
usage(void)
{
	fprintf(stderr, "\n");
	fprintf(stderr, "isdntel - isdn telephone answering management support utility (version %d.%d.%d)\n", VERSION, REL, STEP);
	fprintf(stderr, "    usage: isdntel -a <filename> -d <directory> -p <command> -t <timeout>\n");
	fprintf(stderr, "           -a <filename>   use filename as alias file\n");
	fprintf(stderr, "           -d <directory>  use directory as spool directory\n");
	fprintf(stderr, "           -p <command>    specify commandline for play command\n");
	fprintf(stderr, "           -t <timeout>    spool directory reread timeout in seconds\n");	
	fprintf(stderr, "\n");
	exit(1);
}

/*---------------------------------------------------------------------------*
 *	fatal error exit
 *---------------------------------------------------------------------------*/
void
fatal(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	if(curses_ready)
	{	
		move(LINES-1, 0);
		clrtoeol();
		refresh();
		endwin();
	}

	fprintf(stderr, "\nFatal error: ");
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n\n");
		
	va_end(ap);

	exit(1);
}

/*---------------------------------------------------------------------------*
 *	error printing
 *---------------------------------------------------------------------------*/
void
error(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	if(curses_ready)
	{
		wprintw(main_w, "ERROR: ");
		vwprintw(main_w, fmt, ap);
		wprintw(main_w, "\n");
		wrefresh(main_w);
	}
	else
	{
		fprintf(stderr, "ERROR: ");
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
	}
		
	va_end(ap);
}

/*---------------------------------------------------------------------------*
 *	read files and fill display
 *---------------------------------------------------------------------------*/
void
init_files(int inipos)
{
        int i;

	nofiles = fill_list();
		
	top_dis = 0;
	bot_dis = 0;

	cur_file = first;

	cur_pos = 0;
	cur_pos_scr = 0;	

	if(nofiles == 0)
		return;
	
	for(i=0; (i < nofiles) && (i < (LINES-START_O-2)); i++)
	{
		mvwprintw(main_w, i, 0, "%s %s", cur_file->date, cur_file->time);
		mvwprintw(main_w, i, DST_POS, "%s", cur_file->dstnumber);
		mvwprintw(main_w, i, SRC_POS, "%s", cur_file->srcnumber);
		mvwprintw(main_w, i, ALI_POS,"%s", cur_file->alias == NULL ? "-/-" : cur_file->alias);
		mvwprintw(main_w, i, SEC_POS,"%s", cur_file->seconds);

		bot_dis++;

		if((cur_file = cur_file->next) == NULL)
			break;		
	}
	
	cur_file = first;

	if(inipos)
	{
		for(i=0; i < inipos; i++)
		{
			if(cur_file->next != NULL)
				cur_file = cur_file->next;
			else
				break;
		}
	}
	makecurrent(cur_pos, cur_file, 1);
}

/* EOF */
