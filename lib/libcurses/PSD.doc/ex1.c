.\" Copyright (c) 1992, 1993
.\"	 The Regents of the University of California.  All rights reserved.
.\"
.\" Redistribution and use in source and binary forms, with or without
.\" modification, are permitted provided that the following conditions
.\" are met:
.\" 1. Redistributions of source code must retain the above copyright
.\"    notice, this list of conditions and the following disclaimer.
.\" 2. Redistributions in binary form must reproduce the above copyright
.\"    notice, this list of conditions and the following disclaimer in the
.\"    documentation and/or other materials provided with the distribution.
.\" 3. All advertising materials mentioning features or use of this software
.\"    must display the following acknowledgement:
.\"	This product includes software developed by the University of
.\"	California, Berkeley and its contributors.
.\" 4. Neither the name of the University nor the names of its contributors
.\"    may be used to endorse or promote products derived from this software
.\"    without specific prior written permission.
.\"
.\" THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
.\" ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
.\" IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
.\" ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
.\" FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
.\" DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
.\" OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
.\" HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
.\" LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
.\" OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
.\" SUCH DAMAGE.
.\"
.\"     @(#)ex1.c	8.1 (Berkeley) 6/8/93
.\"
#include <sys/types.h>
#include <curses.h>
#include <stdio.h>
#include <signal.h>


#define YSIZE 10 
#define XSIZE 20

int quit();

main()
{
	int i, j, c;
	size_t len;
	char id[100];
	FILE *fp;
	char *s;

	initscr();			/* Always call initscr() first */
	signal(SIGINT, quit);		/* Make sure wou have a 'cleanup' fn */
	crmode();			/* We want cbreak mode */
	noecho();			/* We want to have control of chars */
	delwin(stdscr);			/* Create our own stdscr */
	stdscr = newwin(YSIZE, XSIZE, 10, 35); 
	flushok(stdscr, TRUE);		/* Enable flushing of stdout */
	scrollok(stdscr, TRUE);		/* Enable scrolling */
	erase();			/* Initially, clear the screen */

	standout();
	move(0,0);
	while (1) {
		c = getchar();
		switch(c) {
		case 'q':		/* Quit on 'q' */
			quit();
			break;
		case 's':		/* Go into standout mode on 's' */
			standout();
			break;
		case 'e':		/* Exit standout mode on 'e' */
			standend();
			break;
		case 'r':		/* Force a refresh on 'r' */
			wrefresh(curscr);
			break;
		default:		/* By default output the character */
			addch(c);
			refresh();
		}
	}
}


int
quit()
{
	erase();		/* Terminate by erasing the screen */
	refresh();
	endwin();		/* Always end with endwin() */
	delwin(curscr);		/* Return storage */
	delwin(stdscr);
	putchar('\n');
	exit(0);
}

				
	
	
