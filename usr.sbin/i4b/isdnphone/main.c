/*
 * Copyright (c) 1999, 2001 Hellmuth Michaelis. All rights reserved.
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
 *	isdnphone - main module
 *	=======================
 *
 *	$Id: main.c,v 1.12 1999/12/13 21:25:26 hm Exp $
 *
 * $FreeBSD$
 *
 *      last edit-date: [Mon Dec 13 21:53:25 1999]
 *
 *---------------------------------------------------------------------------*/

#define MAIN
#include "defs.h"

static void kbd_hdlr(void);

/*---------------------------------------------------------------------------*
 *	usage display and exit
 *---------------------------------------------------------------------------*/
static void
usage(void)
{
	fprintf(stderr, "\n");
	fprintf(stderr, "isdnphone - i4b phone program, version %d.%d.%d, compiled %s %s\n",VERSION, REL, STEP, __DATE__, __TIME__);
	fprintf(stderr, "usage: isdnphone -d -h -k <string> -n <number> -u <unit>\n");
	fprintf(stderr, "       -d            debug\n");
	fprintf(stderr, "       -h            hangup\n");
	fprintf(stderr, "       -k string     keypad string\n");
	fprintf(stderr, "       -n number     dial number\n");
	fprintf(stderr, "       -u unit       set unit number\n");
	fprintf(stderr, "\n");
	exit(1);
}

/*---------------------------------------------------------------------------*
 *	program entry
 *---------------------------------------------------------------------------*/
int
main(int argc, char **argv)
{
	int c;
	char namebuffer[128];
	int bschar;
	int ret;
	int opt_n = 0;
	int opt_h = 0;
	int opt_k = 0;
	char *number = "";
	
	numberbuffer[0] = '\0';	

	while ((c = getopt(argc, argv, "dhk:n:u:")) != -1)
	{
		switch(c)
		{
			case 'd':
				opt_d = 1;
				break;
				
			case 'h':
				opt_h = 1;
				break;
				
			case 'k':
				number = optarg;
				opt_k = 1;
				break;
				
			case 'n':
				number = optarg;
				opt_n = 1;
				break;
				
			case 'u':
				opt_unit = atoi(optarg);
				if(opt_unit < 0 || opt_unit > 9)
					usage();
				break;

			case '?':
			default:
				usage();
				break;
		}
	}

	sprintf(namebuffer,"%s%d", I4BTELDDEVICE, opt_unit);
	
	if((dialerfd = init_dial(namebuffer)) == -1)
		exit(1);

	if(opt_n || opt_h || opt_k)
	{
		char commandbuffer[80];
		
		/* commandline operation goes here */
		
		if(opt_n)
		{
			sprintf(commandbuffer, "D%s", number);
	
		}
		else if(opt_k)
		{
			sprintf(commandbuffer, "K%s", number);
	
		}
		else if(opt_h)
		{
			sprintf(commandbuffer, "H");
		}
	
		if((ret = write(dialerfd, commandbuffer, strlen(commandbuffer))) < 0)
		{
			fprintf(stderr, "write commandbuffer failed: %s", strerror(errno));
			exit(1);
		}
	
		close(dialerfd);
		
		exit(0);
	}

	if((audiofd = init_audio(AUDIODEVICE)) == -1)
		exit(1);
	
	/* fullscreen operation here */	

	init_mainw();

	bschar = erasechar();
	curx = 0;

	wmove(main_w, MW_NUMY, MW_NUX + curx);
	
	/* go into loop */

	for (;;)
	{
		int maxfd = 0;
		fd_set set;
		struct timeval timeout;

		FD_ZERO(&set);
		
		FD_SET(STDIN_FILENO, &set);
		if(STDIN_FILENO > maxfd)
			maxfd = STDIN_FILENO;
		
		FD_SET(dialerfd, &set);
		if(dialerfd > maxfd)
			maxfd = dialerfd;
		
		if(state == ST_ACTIVE)
		{
			if(audiofd != -1)
			{
				FD_SET(audiofd, &set);
				if(audiofd > maxfd)
					maxfd = audiofd;
			}
			
			if(telfd != -1)
			{
				FD_SET(telfd, &set);
				if(telfd > maxfd)
					maxfd = telfd;
			}
		}
		
		timeout.tv_sec = 2;
		timeout.tv_usec = 0;

		wrefresh(main_w);
		
		/* if no char is available within timeout, do something */
		
#ifdef NOTDEF
		ret = select(maxfd+1, &set, NULL, NULL, &timeout);
#else
		ret = select(maxfd+1, &set, NULL, NULL, NULL);
#endif

		if(ret > 0)
		{
			if((telfd != -1) && (FD_ISSET(telfd, &set)))
			{
				message("select from ISDN");
				tel_hdlr();
			}
			if((audiofd != -1) && (FD_ISSET(audiofd, &set)))
			{
				message("select from audio");
				audio_hdlr();
			}
			if(FD_ISSET(dialerfd, &set))
			{
				message("select from tel");
				dial_hdlr();
			}
			if(FD_ISSET(STDIN_FILENO, &set))
			{
				message("select from kbd");
				kbd_hdlr();
			}
		}
	}
	do_quit(0);
	
	return(0);
}

/*---------------------------------------------------------------------------*
 *	keyboard character available handler
 *---------------------------------------------------------------------------*/
static void
kbd_hdlr(void)
{		
	int kchar;

	kchar = wgetch(main_w);		/* get char */
                                
	switch (kchar)
	{
		case CR:
		case LF:
#ifdef KEY_ENTER
		case KEY_ENTER:
#endif
			if((state == ST_IDLE) &&
			   (numberbuffer[0] != '\0'))
			{
				message("dialing .....");
				do_dial(&numberbuffer[0]);
			}
			else
			{
				do_menu();
			}
			break;

		case CNTRL_D:
			if(state == ST_IDLE)
			{
				do_quit(0);
			}
			else
			{
				message("cannot exit while not idle!");
				beep();
			}
			
			break;

		case CNTRL_L:	/* refresh */
			touchwin(curscr);
			wrefresh(curscr);
			break;

		case KEY_BACKSPACE:
		case KEY_DC:
			if (curx == 0)
				break;

			curx--;
			mvwaddch(main_w, MW_NUMY, MW_NUX + curx, ' ');
			numberbuffer[curx] = '\0';
			wmove(main_w, MW_NUMY, MW_NUX + curx);

			if(curx == 0)
				message(" ");
			
			break;
			
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if(curx > (TELNO_MAX-1))
				break;

			mvwaddch(main_w, MW_NUMY, MW_NUX + curx, kchar);
			
			numberbuffer[curx] = kchar;

			curx++;

			numberbuffer[curx] = '\0';
			
			message("press ENTER to dial number .....");
			break;
	}
}

/*---------------------------------------------------------------------------*
 *	exit program
 *---------------------------------------------------------------------------*/
void
do_quit(int exitval)
{
	close(dialerfd);
	move(LINES-1, 0);
	clrtoeol();
	refresh();
	endwin();
	exit(exitval);
}

/*---------------------------------------------------------------------------*
 *	fatal error exit
 *---------------------------------------------------------------------------*/
void
fatal(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	do_hangup();		/* failsafe */
	
	if(curses_ready)
	{	
		close(dialerfd);
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
 *	message printing
 *---------------------------------------------------------------------------*/
void
message(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	if(curses_ready)
	{
		int i;
		char sbuf[MW_WIDTH];
		
		wmove(main_w, MW_MSGY, MW_MSX);
		vsnprintf(sbuf, MW_WIDTH-MW_MSX-1, fmt, ap);
		waddstr(main_w, sbuf);
		for(i=strlen(sbuf);i < MW_WIDTH-MW_MSX-2; i++)
			waddch(main_w, ' ');
		wmove(main_w, MW_NUMY, MW_NUX + curx);			
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
 *	message printing
 *---------------------------------------------------------------------------*/
void
debug(char *fmt, ...)
{
	va_list ap;

	if(opt_d == 0)
		return;
		
	va_start(ap, fmt);

	vwprintw(dbg_w, fmt, ap);
	wrefresh(dbg_w);
		
	va_end(ap);
}

/*---------------------------------------------------------------------------*
 *	go to new state
 *---------------------------------------------------------------------------*/
void
newstate(int newstate)
{
	int i;
	
	if(newstate < 0 || newstate > ST_MAX)
	{
		message("newstate %d undefined!", newstate);
		return;
	}

	state = newstate;

	if(newstate == ST_ACTIVE)
	{
		char namebuffer[128];
		
		sprintf(namebuffer,"%s%d", I4BTELDEVICE, opt_unit);
		telfd = init_tel(namebuffer);
	}

	if(newstate == ST_IDLE)
	{
		close(telfd);
		telfd = -1;
	}
	
	wmove(main_w, MW_STATEY, MW_STX);
	waddstr(main_w, states[newstate]);

	for(i=strlen(states[newstate]);i < MW_WIDTH-MW_STX-2; i++)
		waddch(main_w, ' ');

	wmove(main_w, MW_NUMY, MW_NUX + curx);			
	wrefresh(main_w);
}

/* EOF */
