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
static char sccsid[] = "@(#)main.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD$";
#endif /* not lint */

#include <stdio.h>
#include <termcap.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include "back.h"

#define MVPAUSE	5				/* time to sleep when stuck */
#define MAXUSERS 35				/* maximum number of users */

extern const char	*const instr[];		/* text of instructions */
extern const char	*const message[];	/* update message */

const char	*helpm[] = {			/* help message */
	"Enter a space or newline to roll, or",
	"     R   to reprint the board\tD   to double",
	"     S   to save the game\tQ   to quit",
	0
};

const char	*contin[] = {			/* pause message */
	"(Type a newline to continue.)",
	"",
	0
};

static const char	rules[] = "\nDo you want the rules of the game?";
static const char	noteach[] = "Teachgammon not available!\n\007";
static const char	need[] = "Do you need instructions for this program?";
static const char	askcol[] =
	"Enter 'r' to play red, 'w' to play white, 'b' to play both:";
static const char	rollr[] = "Red rolls a ";
static const char	rollw[] = ".  White rolls a ";
static const char	rstart[] = ".  Red starts.\n";
static const char	wstart[] = ".  White starts.\n";
static const char	toobad1[] = "Too bad, ";
static const char	unable[] = " is unable to use that roll.\n";
static const char	toobad2[] = ".  Too bad, ";
static const char	cantmv[] = " can't move.\n";
static const char	bgammon[] = "Backgammon!  ";
static const char	gammon[] = "Gammon!  ";
static const char	again[] = ".\nWould you like to play again?";
static const char	svpromt[] = "Would you like to save this game?";

static const char	password[] = "losfurng";
static char	pbuf[10];

main (argc,argv)
int	argc;
char	**argv;

{
	int	i;		/* non-descript index */
	int	l;		/* non-descript index */
	char	c;		/* non-descript character storage */

	/* revoke privs */
	setgid(getgid());

	/* initialization */
	bflag = 2;					/* default no board */
	acnt = 1;                                       /* Nuber of args */
	signal (SIGINT,getout);				/* trap interrupts */
	if (gtty (0,&tty) == -1)			/* get old tty mode */
		errexit ("backgammon(gtty)");
	old = tty.sg_flags;
#ifdef V7
	raw = ((noech = old & ~ECHO) | CBREAK);		/* set up modes */
#else
	raw = ((noech = old & ~ECHO) | RAW);		/* set up modes */
#endif
	ospeed = tty.sg_ospeed;				/* for termlib */

							/* get terminal
							 * capabilities, and
					   		 * decide if it can
							 * cursor address */
	tflag = getcaps (getenv ("TERM"));
							/* use whole screen
							 * for text */
	if (tflag)
		begscr = 0;
	srandomdev();

	getarg (argc, argv);
	args[acnt] = NULL;
	if (tflag)  {					/* clear screen */
		noech &= ~(CRMOD|XTABS);
		raw &= ~(CRMOD|XTABS);
		clear();
	}
	fixtty (raw);					/* go into raw mode */

							/* check if restored
							 * game and save flag
							 * for later */
	if (rfl = rflag)  {
		text (message);				/* print message */
		text (contin);
		wrboard();				/* print board */
							/* if new game, pretend
							 * to be a non-restored
							 * game */
		if (cturn == 0)
			rflag = 0;
	} else  {
		rscore = wscore = 0;			/* zero score */
		text (message);				/* update message
							 * without pausing */

		if (aflag)  {				/* print rules */
			writel (rules);
			if (yorn(0))  {

				fixtty (old);		/* restore tty */
				args[0] = strdup ("teachgammon");
				execv (TEACH,args);

				tflag = 0;		/* error! */
				writel (noteach);
				exit(1);
			} else  {			/* if not rules, then
							 * instructions */
				writel (need);
				if (yorn(0))  {		/* print instructions */
					clear();
					text (instr);
				}
			}
		}

		for (i = 0; i < acnt; i++)
			free (args[i]);

		init();					/* initialize board */

		if (pnum == 2)  {			/* ask for color(s) */
			writec ('\n');
			writel (askcol);
			while (pnum == 2)  {
				c = readc();
				switch (c)  {

				case 'R':		/* red */
					pnum = -1;
					break;

				case 'W':		/* white */
					pnum = 1;
					break;

				case 'B':		/* both */
					pnum = 0;
					break;

				case 'P':
					if (iroll)
						break;
					if (tflag)
						curmove (curr,0);
					else
						writec ('\n');
					writel ("Password:");
					signal (SIGALRM,getout);
					cflag = 1;
					alarm (10);
					for (i = 0; i < 10; i++)  {
						pbuf[i] = readc();
						if (pbuf[i] == '\n')
							break;
					}
					if (i == 10)
						while (readc() != '\n');
					alarm (0);
					cflag = 0;
					if (i < 10)
						pbuf[i] = '\0';
					for (i = 0; i < 9; i++)
						if (pbuf[i] != password[i])
							getout();
					iroll = 1;
					if (tflag)
						curmove (curr,0);
					else
						writec ('\n');
					writel (askcol);
					break;

				default:		/* error */
					writec ('\007');
				}
			}
		} else  if (!aflag)
							/* pause to read
							 * message */
			text (contin);

		wrboard();				/* print board */

		if (tflag)
			curmove (18,0);
		else
			writec ('\n');
	}
							/* limit text to bottom
							 * of screen */
	if (tflag)
		begscr = 17;

	for (;;)  {					/* begin game! */
							/* initial roll if
							 * needed */
		if ((! rflag) || raflag)
			roll();

							/* perform ritual of
							 * first roll */
		if (! rflag)  {
			if (tflag)
				curmove (17,0);
			while (D0 == D1)		/* no doubles */
				roll();

							/* print rolls */
			writel (rollr);
			writec (D0+'0');
			writel (rollw);
			writec (D1+'0');

							/* winner goes first */
			if (D0 > D1)  {
				writel (rstart);
				cturn = 1;
			} else  {
				writel (wstart);
				cturn = -1;
			}
		}

							/* initalize variables
							 * according to whose
							 * turn it is */

		if (cturn == 1)  {			    /* red */
			home = 25;
			bar = 0;
			inptr = &in[1];
			inopp = &in[0];
			offptr = &off[1];
			offopp = &off[0];
			Colorptr = &color[1];
			colorptr = &color[3];
			colen = 3;
		} else  {				    /* white */
			home = 0;
			bar = 25;
			inptr = &in[0];
			inopp = &in[1];
			offptr = &off[0];
			offopp = &off[1];
			Colorptr = &color[0];
			colorptr = &color[2];
			colen = 5;
		}

							/* do first move
							 * (special case) */
		if (! (rflag && raflag))  {
			if (cturn == pnum)		/* computer's move */
				move (0);
			else  {				/* player's move */
				mvlim = movallow();
							/* reprint roll */
				if (tflag)
					curmove (cturn == -1? 18: 19,0);
				proll();
				getmove();		/* get player's move */
			}
		}
		if (tflag)  {
			curmove (17,0);
			cline();
			begscr = 18;
		}

							/* no longer any diff-
							 * erence between normal
							 * game and recovered
							 * game. */
		rflag = 0;

							/* move as long as it's
							 * someone's turn */
		while (cturn == 1 || cturn == -1)  {

							/* board maintainence */
			if (tflag)
				refresh();		/* fix board */
			else
							/* redo board if -p */
				if (cturn == bflag || bflag == 0)
					wrboard();

							/* do computer's move */
			if (cturn == pnum)  {
				move (1);

							/* see if double
							 * refused */
				if (cturn == -2 || cturn == 2)
					break;

							/* check for winning
							 * move */
				if (*offopp == 15)  {
					cturn *= -2;
					break;
				}
				continue;

			}

							/* (player's move) */

							/* clean screen if
							 * safe */
			if (tflag && hflag)  {
				curmove (20,0);
				clend ();
				hflag = 1;
			}

							/* if allowed, give him
							 * a chance to double */
			if (dlast != cturn && gvalue < 64)  {
				if (tflag)
					curmove (cturn == -1? 18: 19,0);
				writel (*Colorptr);
				c = readc();

							/* character cases */
				switch (c)  {

							/* reprint board */
				case 'R':
					wrboard();
					break;

							/* save game */
				case 'S':
					raflag = 1;
					save (1);
					break;

							/* quit */
				case 'Q':
					quit();
					break;

							/* double */
				case 'D':
					dble();
					break;

							/* roll */
				case ' ':
				case '\n':
					roll();
					writel (" rolls ");
					writec (D0+'0');
					writec (' ');
					writec (D1+'0');
					writel (".  ");

							/* see if he can move */
					if ( (mvlim = movallow()) == 0)  {

							/* can't move */
						writel (toobad1);
						writel (*colorptr);
						writel (unable);
						if (tflag)  {
							if (pnum)  {
								buflush();
								sleep (MVPAUSE);
							}
						}
						nexturn();
						break;
					}

							/* get move */
					getmove();

							/* okay to clean
							 * screen */
					hflag = 1;
					break;

							/* invalid character */
				default:

							/* print help message */
					if (tflag)
						curmove (20,0);
					else
						writec ('\n');
					text (helpm);
					if (tflag)
						curmove (cturn == -1? 18: 19,0);
					else
						writec ('\n');

							/* don't erase */
					hflag = 0;
				}
			} else  {			/* couldn't double */

							/* print roll */
				roll();
				if (tflag)
					curmove (cturn == -1? 18: 19,0);
				proll ();

							/* can he move? */
				if ((mvlim = movallow()) == 0)  {

							/* he can't */
					writel (toobad2);
					writel (*colorptr);
					writel (cantmv);
					buflush();
					sleep (MVPAUSE);
					nexturn();
					continue;
				}

							/* get move */
				getmove();
			}
		}

							/* don't worry about who
							 * won if quit */
		if (cturn == 0)
			break;

							/* fix cturn = winner */
		cturn /= -2;

							/* final board pos. */
		if (tflag)
			refresh();

							/* backgammon? */
		mflag = 0;
		l = bar+7*cturn;
		for (i = bar; i != l; i += cturn)
			if (board[i]*cturn)  mflag++;

							/* compute game value */
		if (tflag)
			curmove (20,0);
		if (*offopp == 15)  {
			if (mflag)  {
				writel (bgammon);
				gvalue *= 3;
			}
			else  if (*offptr <= 0)  {
				writel (gammon);
				gvalue *= 2;
			}
		}

							/* report situation */
		if (cturn == -1)  {
			writel ("Red wins ");
			rscore += gvalue;
		} else {
			writel ("White wins ");
			wscore += gvalue;
		}
		wrint (gvalue);
		writel (" point");
		if (gvalue > 1)
			writec ('s');
		writel (".\n");

							/* write score */
		wrscore();

							/* see if he wants
							 * another game */
		writel (again);
		if ((i = yorn ('S')) == 0)
			break;

		init();
		if (i == 2)  {
			writel ("  Save.\n");
			cturn = 0;
			save (0);
		}

							/* yes, reset game */
		wrboard();
	}

	/* give him a chance to save if game was recovered */
	if (rfl && cturn)  {
		writel (svpromt);
		if (yorn (0))  {
							/* re-initialize for
							 * recovery */
			init();
			cturn = 0;
			save(0);
		}
	}

							/* leave peacefully */
	getout ();
}
