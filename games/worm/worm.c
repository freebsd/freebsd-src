/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)worm.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD: src/games/worm/worm.c,v 1.9 1999/12/07 02:01:27 billf Exp $";
#endif /* not lint */

/*
 * Worm.  Written by Michael Toy
 * UCSC
 */

#include <ctype.h>
#include <curses.h>
#include <signal.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#define newlink() (struct body *) malloc(sizeof (struct body));
#define HEAD '@'
#define BODY 'o'
#define LENGTH 7
#define RUNLEN 8
#define CNTRL(p) (p-'A'+1)
#if 0
#ifndef baudrate
# define	baudrate()	_tty.sg_ospeed
#endif
#endif

WINDOW *tv;
WINDOW *stw;
struct body {
	int x;
	int y;
	struct body *prev;
	struct body *next;
} *head, *tail, goody;
int growing = 0;
int running = 0;
int slow = 0;
int score = 0;
int start_len = LENGTH;
char lastch;
char outbuf[BUFSIZ];
 
void crash __P((void));
void display __P((struct body *, char));
void leave __P((int));
void life __P((void));
void newpos __P((struct body *));
void prize __P((void));
void process __P((char));
long rnd __P((int));
void setup __P((void));
void suspend __P((int));
void wake __P((int));

int
main(argc, argv)
	int argc;
	char **argv;
{
	char ch;

	/* revoke */
	setgid(getgid());

	if (argc == 2)
		start_len = atoi(argv[1]);
	if ((start_len <= 0) || (start_len > 500))
		start_len = LENGTH;
	setbuf(stdout, outbuf);
	srandomdev();
	signal(SIGALRM, wake);
	signal(SIGINT, leave);
	signal(SIGQUIT, leave);
	signal(SIGTSTP, suspend);	/* process control signal */
	initscr();
	crmode();
	noecho();
	slow = (baudrate() <= B1200);
	clear();
	stw = newwin(1, COLS-1, 0, 0);
	tv = newwin(LINES-1, COLS-1, 1, 0);
	box(tv, '*', '*');
	scrollok(tv, FALSE);
	scrollok(stw, FALSE);
	wmove(stw, 0, 0);
	wprintw(stw, " Worm");
	refresh();
	wrefresh(stw);
	wrefresh(tv);
	life();			/* Create the worm */
	prize();		/* Put up a goal */
	while(1)
	{
		if (running)
		{
			running--;
			process(lastch);
		}
		else
		{
		    fflush(stdout);
		    if (read(0, &ch, 1) >= 0)
			process(ch);
		}
	}
}

void
life()
{
	struct body *bp, *np;
	int i;

	np = NULL;
	head = newlink();
	head->x = start_len+2;
	head->y = 12;
	head->next = NULL;
	display(head, HEAD);
	for (i = 0, bp = head; i < start_len; i++, bp = np) {
		np = newlink();
		np->next = bp;
		bp->prev = np;
		np->x = bp->x - 1;
		np->y = bp->y;
		display(np, BODY);
	}
	tail = np;
	tail->prev = NULL;
}

void
display(pos, chr)
struct body *pos;
char chr;
{
	wmove(tv, pos->y, pos->x);
	waddch(tv, chr);
}

void
leave(sig)
int sig;
{
	endwin();
	exit(0);
}

void
wake(sig)
int sig;
{
	signal(SIGALRM, wake);
	fflush(stdout);
	process(lastch);
}

long
rnd(range)
{
	return random() % range;
}

void
newpos(bp)
struct body * bp;
{
	do {
		bp->y = rnd(LINES-3)+ 2;
		bp->x = rnd(COLS-3) + 1;
		wmove(tv, bp->y, bp->x);
	} while(winch(tv) != ' ');
}

void
prize()
{
	int value;

	value = rnd(9) + 1;
	newpos(&goody);
	waddch(tv, value+'0');
	wrefresh(tv);
}

void
process(ch)
char ch;
{
	int x,y;
	struct body *nh;

	alarm(0);
	x = head->x;
	y = head->y;
	switch(ch)
	{
		case 'h': x--; break;
		case 'j': y++; break;
		case 'k': y--; break;
		case 'l': x++; break;
		case 'H': x--; running = RUNLEN; ch = tolower(ch); break;
		case 'J': y++; running = RUNLEN/2; ch = tolower(ch); break;
		case 'K': y--; running = RUNLEN/2; ch = tolower(ch); break;
		case 'L': x++; running = RUNLEN; ch = tolower(ch); break;
		case '\f': setup(); return;
		case CNTRL('Z'): suspend(0); return;
		case CNTRL('C'): crash(); return;
		case CNTRL('D'): crash(); return;
		default: if (! running) alarm(1);
			   return;
	}
	lastch = ch;
	if (growing == 0)
	{
		display(tail, ' ');
		tail->next->prev = NULL;
		nh = tail->next;
		free(tail);
		tail = nh;
	}
	else growing--;
	display(head, BODY);
	wmove(tv, y, x);
	if (isdigit(ch = winch(tv)))
	{
		growing += ch-'0';
		prize();
		score += growing;
		running = 0;
		wmove(stw, 0, 68);
		wprintw(stw, "Score: %3d", score);
		wrefresh(stw);
	}
	else if(ch != ' ') crash();
	nh = newlink();
	nh->next = NULL;
	nh->prev = head;
	head->next = nh;
	nh->y = y;
	nh->x = x;
	display(nh, HEAD);
	head = nh;
	if (!(slow && running))
		wrefresh(tv);
	if (!running)
		alarm(1);
}

void
crash()
{
	sleep(2);
	clear();
	move(23, 0);
	refresh();
	printf("Well, you ran into something and the game is over.\n");
	printf("Your final score was %d\n", score);
	leave(0);
}

void
suspend(sig)
int sig;
{
	move(LINES-1, 0);
	refresh();
	endwin();
	fflush(stdout);
	kill(getpid(), SIGTSTP);
	signal(SIGTSTP, suspend);
	crmode();
	noecho();
	setup();
}

void
setup()
{
	clear();
	refresh();
	touchwin(stw);
	wrefresh(stw);
	touchwin(tv);
	wrefresh(tv);
	alarm(1);
}
