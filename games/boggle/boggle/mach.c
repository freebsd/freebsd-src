/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Barry Brachman.
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
static char sccsid[] = "@(#)mach.c	8.1 (Berkeley) 6/11/93";
#endif /* not lint */

/*
 * Terminal interface
 *
 * Input is raw and unechoed
 */
#include <ctype.h>
#include <curses.h>
#include <fcntl.h>
#include <sgtty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "bog.h"
#include "extern.h"

static int ccol, crow, maxw;
static int colstarts[MAXCOLS], ncolstarts;
static int lastline;
int ncols, nlines;

extern char *pword[], *mword[];
extern int ngames, nmwords, npwords, tnmwords, tnpwords;

static void	cont_catcher __P((int));
static int	prwidth __P((char *[], int));
static void	prword __P((char *[], int));
static void	stop_catcher __P((int));
static void	tty_cleanup __P((void));
static int	tty_setup __P((void));
static void	tty_showboard __P((char *));
static void	winch_catcher __P((int));

/*
 * Do system dependent initialization
 * This is called once, when the program starts
 */
int
setup(sflag, seed)
	int sflag;
	long seed;
{
	extern int debug;

	if (tty_setup() < 0)
		return(-1);

	if (!sflag)
		time(&seed);
	srandom(seed);
	if (debug)
		(void) printf("seed = %ld\n", seed);
	return(0);
}

/*
 * Do system dependent clean up
 * This is called once, just before the program terminates
 */
void
cleanup()
{
	tty_cleanup();
}

/*
 * Display the player's word list, the list of words not found, and the running
 * stats
 */
void
results()
{
	int col, row;
	int denom1, denom2;

	move(LIST_LINE, LIST_COL);
	clrtobot();
	printw("Words you found (%d):", npwords);
	refresh();
	move(LIST_LINE + 1, LIST_COL);
	prtable(pword, npwords, 0, ncols, prword, prwidth);

	getyx(stdscr, row, col);
	move(row + 1, col);
	printw("Words you missed (%d):", nmwords);
	refresh();
	move(row + 2, col);
	prtable(mword, nmwords, 0, ncols, prword, prwidth);

	denom1 = npwords + nmwords;
	denom2 = tnpwords + tnmwords;
 
	move(SCORE_LINE, SCORE_COL);
	printw("Percentage: %0.2f%% (%0.2f%% over %d game%s)\n",
        denom1 ? (100.0 * npwords) / (double) (npwords + nmwords) : 0.0,
        denom2 ? (100.0 * tnpwords) / (double) (tnpwords + tnmwords) : 0.0,
        ngames, ngames > 1 ? "s" : "");
}

static void
prword(base, indx)
	char *base[];
	int indx;
{
	printw("%s", base[indx]);
}

static int
prwidth(base, indx)
	char *base[];
	int indx;
{
	return (strlen(base[indx]));
}

/*
 * Main input routine
 *
 * - doesn't accept words longer than MAXWORDLEN or containing caps
 */
char *
getline(q)
	char *q;
{
	register int ch, done;
	register char *p;
	int row, col;

	p = q;
	done = 0;
	while (!done) {
		ch = timerch();
		switch (ch) {
		case '\n':
		case '\r':
		case ' ':
			done = 1;
			break;
		case '\033':
			findword();
			break;
		case '\177':			/* <del> */
		case '\010':			/* <bs> */
			if (p == q)
				break;
			p--;
			getyx(stdscr, row, col);
			move(row, col - 1);
			clrtoeol();
			refresh();
			break;
		case '\025':			/* <^u> */
		case '\027':			/* <^w> */
			if (p == q)
				break;
			getyx(stdscr, row, col);
			move(row, col - (int) (p - q));
			p = q;
			clrtoeol();
			refresh();
			break;
#ifdef SIGTSTP
		case '\032':			/* <^z> */
			stop_catcher(0);
			break;
#endif
		case '\023':			/* <^s> */
			stoptime();
			printw("<PAUSE>");
			refresh();
			while ((ch = inputch()) != '\021' && ch != '\023')
				;
			move(crow, ccol);
			clrtoeol();
			refresh();
			starttime();
			break;
		case '\003':			/* <^c> */
			cleanup();
			exit(0);
			/*NOTREACHED*/
		case '\004':			/* <^d> */
			done = 1;
			ch = EOF;
			break;
		case '\014':			/* <^l> */
		case '\022':			/* <^r> */
			redraw();
			break;
		case '?':
			stoptime();
			if (help() < 0)
				showstr("Can't open help file", 1);
			starttime();
			break;
		default:
			if (!islower(ch))
				break;
			if ((int) (p - q) == MAXWORDLEN) {
				p = q;
				badword();
				break;
			}
			*p++ = ch;
			addch(ch);
			refresh();
			break;
		}
	}
	*p = '\0';
	if (ch == EOF)
		return((char *) NULL);
	return(q);
}

int
inputch()
{
	return (getch() & 0177);
}

void
redraw()
{
	clearok(stdscr, 1);
	refresh();
}

void
flushin(fp)
	FILE *fp;
{
	int arg;

	arg = FREAD;
	(void)ioctl(fileno(fp), TIOCFLUSH, &arg);
}

static int gone;

/*
 * Stop the game timer
 */
void
stoptime()
{
	extern long start_t;
	long t;

	(void)time(&t);
	gone = (int) (t - start_t);
}

/*
 * Restart the game timer
 */
void
starttime()
{
	extern long start_t;
	long t;

	(void)time(&t);
	start_t = t - (long) gone;
}

/*
 * Initialize for the display of the player's words as they are typed
 * This display starts at (LIST_LINE, LIST_COL) and goes "down" until the last
 * line.  After the last line a new column is started at LIST_LINE
 * Keep track of each column position for showword()
 * There is no check for exceeding COLS
 */
void
startwords()
{
	crow = LIST_LINE;
	ccol = LIST_COL;
	maxw = 0;
	ncolstarts = 1;
	colstarts[0] = LIST_COL;
	move(LIST_LINE, LIST_COL);
	refresh();
}

/*
 * Add a word to the list and start a new column if necessary
 * The maximum width of the current column is maintained so we know where
 * to start the next column
 */
void
addword(w)
	char *w;
{
	int n;

	if (crow == lastline) {
		crow = LIST_LINE;
		ccol += (maxw + 5);
		colstarts[ncolstarts++] = ccol;
		maxw = 0;
		move(crow, ccol);
	}
	else {
		move(++crow, ccol);
		if ((n = strlen(w)) > maxw)
			maxw = n;
	}
	refresh();
}

/*
 * The current word is unacceptable so erase it
 */
void
badword()
{

	move(crow, ccol);
	clrtoeol();
	refresh();
}

/*
 * Highlight the nth word in the list (starting with word 0)
 * No check for wild arg
 */
void
showword(n)
	int n;
{
	int col, row;

	row = LIST_LINE + n % (lastline - LIST_LINE + 1);
	col = colstarts[n / (lastline - LIST_LINE + 1)];
	move(row, col);
	standout();
	printw("%s", pword[n]);
	standend();
	move(crow, ccol);
	refresh();
	delay(15);
	move(row, col);
	printw("%s", pword[n]);
	move(crow, ccol);
	refresh();
}

/*
 * Get a word from the user and check if it is in either of the two
 * word lists
 * If it's found, show the word on the board for a short time and then
 * erase the word
 *
 * Note: this function knows about the format of the board
 */
void
findword()
{
	int c, col, found, i, r, row;
	char buf[MAXWORDLEN + 1];
	extern char board[];
	extern int usedbits, wordpath[];
	extern char *mword[], *pword[];
	extern int nmwords, npwords;

	getyx(stdscr, r, c);
	getword(buf);
	found = 0;
	for (i = 0; i < npwords; i++) {
		if (strcmp(buf, pword[i]) == 0) {
			found = 1;
			break;
		}
	}
	if (!found) {
		for (i = 0; i < nmwords; i++) {
			if (strcmp(buf, mword[i]) == 0) {
				found = 1;
				break;
			}
		}
	}
	for (i = 0; i < MAXWORDLEN; i++)
		wordpath[i] = -1;
	usedbits = 0;
	if (!found || checkword(buf, -1, wordpath) == -1) {
		move(r, c);
		clrtoeol();
		addstr("[???]");
		refresh();
		delay(10);
		move(r, c);
		clrtoeol();
		refresh();
		return;
	}

	standout();
	for (i = 0; wordpath[i] != -1; i++) {
		row = BOARD_LINE + (wordpath[i] / 4) * 2 + 1;
		col = BOARD_COL + (wordpath[i] % 4) * 4 + 2;
		move(row, col);
		if (board[wordpath[i]] == 'q')
			printw("Qu");
		else
			printw("%c", toupper(board[wordpath[i]]));
		move(r, c);
		refresh();
		delay(5);
	}

	standend();

	for (i = 0; wordpath[i] != -1; i++) {
		row = BOARD_LINE + (wordpath[i] / 4) * 2 + 1;
		col = BOARD_COL + (wordpath[i] % 4) * 4 + 2;
		move(row, col);
		if (board[wordpath[i]] == 'q')
			printw("Qu");
		else
			printw("%c", toupper(board[wordpath[i]]));
	}
	move(r, c);
	clrtoeol();
	refresh();
}

/*
 * Display a string at the current cursor position for the given number of secs
 */
void
showstr(str, delaysecs)
	char *str;
	int delaysecs;
{
	addstr(str);
	refresh();
	delay(delaysecs * 10);
	move(crow, ccol);
	clrtoeol();
	refresh();
}

void
putstr(s)
	char *s;
{
	addstr(s);
}

/*
 * Get a valid word and put it in the buffer
 */
void
getword(q)
	char *q;
{
	int ch, col, done, i, row;
	char *p;

	done = 0;
	i = 0;
	p = q;
	addch('[');
	refresh();
	while (!done && i < MAXWORDLEN - 1) {
		ch = getch() & 0177;
		switch (ch) {
		case '\177':			/* <del> */
		case '\010':			/* <bs> */
			if (p == q)
				break;
			p--;
			getyx(stdscr, row, col);
			move(row, col - 1);
			clrtoeol();
			break;
		case '\025':			/* <^u> */
		case '\027':			/* <^w> */
			if (p == q)
				break;
			getyx(stdscr, row, col);
			move(row, col - (int) (p - q));
			p = q;
			clrtoeol();
			break;
		case ' ':
		case '\n':
		case '\r':
			done = 1;
			break;
		case '\014':			/* <^l> */
		case '\022':			/* <^r> */
			clearok(stdscr, 1);
			refresh();
			break;
		default:
			if (islower(ch)) {
				*p++ = ch;
				addch(ch);
				i++;
			}
			break;
		}
		refresh();
	}
	*p = '\0';
	addch(']');
	refresh();
}

void
showboard(b)
	char *b;
{
	tty_showboard(b);
}

void
prompt(mesg)
	char *mesg;
{
	move(PROMPT_LINE, PROMPT_COL);
	printw("%s", mesg);
	move(PROMPT_LINE + 1, PROMPT_COL);
	refresh();
}

static int
tty_setup()
{
	initscr();
	raw();
	noecho();

	/*
	 * Does curses look at the winsize structure?
	 * Should handle SIGWINCH ...
	 */
	nlines = LINES;
	lastline = nlines - 1;
	ncols = COLS;

	(void) signal(SIGTSTP, stop_catcher);
	(void) signal(SIGCONT, cont_catcher);
	(void) signal(SIGWINCH, winch_catcher);
	return(0);
}

static void
stop_catcher(signo)
	int signo;
{
	stoptime();
	noraw();
	echo();
	move(nlines - 1, 0);
	refresh();

	(void) signal(SIGTSTP, SIG_DFL);
	(void) sigsetmask(sigblock(0) & ~(1 << (SIGTSTP-1)));
	(void) kill(0, SIGTSTP);
	(void) signal(SIGTSTP, stop_catcher);
}
 
static void
cont_catcher(signo)
	int signo;
{
	(void) signal(SIGCONT, cont_catcher);
	noecho();
	raw();
	clearok(stdscr, 1);
	move(crow, ccol);
	refresh();
	starttime();
}
 
/*
 * The signal is caught but nothing is done about it...
 * It would mean reformatting the entire display
 */
static void
winch_catcher(signo)
	int signo;
{
	struct winsize win;

	(void) signal(SIGWINCH, winch_catcher);
	(void) ioctl(fileno(stdout), TIOCGWINSZ, &win);
	/*
	LINES = win.ws_row;
	COLS = win.ws_col;
	*/
}

static void
tty_cleanup()
{
	move(nlines - 1, 0);
	refresh();
	noraw();
	echo();
	endwin();
}

static void
tty_showboard(b)
	char *b;
{
	register int i;
	int line;

	clear();
	move(BOARD_LINE, BOARD_COL);
	line = BOARD_LINE;
	printw("+---+---+---+---+");
	move(++line, BOARD_COL);
	for (i = 0; i < 16; i++) {
		if (b[i] == 'q')
			printw("| Qu");
		else
			printw("| %c ", toupper(b[i]));
		if ((i + 1) % 4 == 0) {
			printw("|");
			move(++line, BOARD_COL);
			printw("+---+---+---+---+");
			move(++line, BOARD_COL);
		}
	}
	move(SCORE_LINE, SCORE_COL);
	printw("Type '?' for help");
	refresh();
}
