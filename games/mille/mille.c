/*
 * Copyright (c) 1982, 1993
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
"@(#) Copyright (c) 1982, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)mille.c	8.1 (Berkeley) 5/31/93";
#else
static const char rcsid[] =
  "$FreeBSD$";
#endif
#endif /* not lint */

# include	"mille.h"
# include	<signal.h>
# ifdef attron
#	include	<term.h>
# endif	attron

/*
 * @(#)mille.c	1.3 (Berkeley) 5/10/83
 */

static void usage(void);

int
main(ac, av)
	int	ac;
	char	*av[]; {

	bool	restore;

	/* revoke */
	setgid(getgid());

	if (strcmp(av[0], "a.out") == 0) {
		outf = fopen("q", "w");
		setbuf(outf, (char *)NULL);
		Debug = TRUE;
	}
	restore = FALSE;
	switch (ac) {
	  case 2:
		rest_f(av[1]);
		restore = TRUE;
	  case 1:
		break;
	  default:
		usage();
		/* NOTREACHED */
	}
	Play = PLAYER;
	initscr();
	delwin(stdscr);
	stdscr = Board = newwin(BOARD_Y, BOARD_X, 0, 0);
	Score = newwin(SCORE_Y, SCORE_X, 0, 40);
	Miles = newwin(MILES_Y, MILES_X, 17, 0);
#ifdef attron
	idlok(Board, TRUE);
	idlok(Score, TRUE);
	idlok(Miles, TRUE);
#endif
	leaveok(Score, TRUE);
	leaveok(Miles, TRUE);
	clearok(curscr, TRUE);
	srandomdev();
	crmode();
	noecho();
	signal(SIGINT, rub);
	for (;;) {
		if (!restore || (Player[PLAYER].total >= 5000
		    || Player[COMP].total >= 5000)) {
			if (Player[COMP].total < Player[PLAYER].total)
				Player[PLAYER].games++;
			else if (Player[COMP].total > Player[PLAYER].total)
				Player[COMP].games++;
			Player[COMP].total = 0;
			Player[PLAYER].total = 0;
		}
		do {
			if (!restore)
				Handstart = Play = other(Handstart);
			if (!restore || On_exit) {
				shuffle();
				init();
			}
			newboard();
			if (restore)
				mvwaddstr(Score, ERR_Y, ERR_X, Initstr);
			prboard();
			do {
				domove();
				if (Finished)
					newscore();
				prboard();
			} while (!Finished);
			check_more();
			restore = On_exit = FALSE;
		} while (Player[COMP].total < 5000
		    && Player[PLAYER].total < 5000);
	}
}

static void
usage()
{
	fprintf(stderr, "usage: mille [restore_file]\n");
	exit(1);
}

/*
 *	Routine to trap rubouts, and make sure they really want to
 * quit.
 */
void
rub(sig)
int sig; {

	(void)signal(SIGINT, SIG_IGN);
	if (getyn(REALLYPROMPT))
		die(0);
	(void)signal(SIGINT, rub);
}

/*
 *	Time to go beddy-by
 */
void
die(code)
int code; {

	(void)signal(SIGINT, SIG_IGN);
	if (outf)
		fflush(outf);
	mvcur(0, COLS - 1, LINES - 1, 0);
	endwin();
	exit(code);
}
