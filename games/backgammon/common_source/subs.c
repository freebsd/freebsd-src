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
#if 0
static char sccsid[] = "@(#)subs.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD: src/games/backgammon/common_source/subs.c,v 1.12 1999/11/30 03:48:27 billf Exp $";
#endif /* not lint */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "back.h"

int	buffnum;
char	outbuff[BUFSIZ];

static const char	plred[] = "Player is red, computer is white.";
static const char	plwhite[] = "Player is white, computer is red.";
static const char	nocomp[] = "(No computer play.)";

const char  *const descr[] = {
	"Usage:  backgammon [-h n r w b pr pw pb tterm sfile]\n",
	"\t-h\tgets this list\n\t-n\tdon't ask for rules or instructions",
	"\t-r\tplayer is red (implies -n)\n\t-w\tplayer is white (implies -n)",
	"\t-b\ttwo players, red and white (implies -n)",
	"\t-pr\tprint the board before red's turn",
	"\t-pw\tprint the board before white's turn",
	"\t-pb\tprint the board before both player's turn",
	"\t-tterm\tterminal is a term",
	"\t-sfile\trecover saved game from file",
	0
};

errexit (s)
char	*s;
{
	write (2,"\n",1);
	perror (s);
	getout();
}

int addbuf (c)
int	c;

{
	buffnum++;
	if (buffnum == BUFSIZ)  {
		if (write(1,outbuff,BUFSIZ) != BUFSIZ)
			errexit ("addbuf (write):");
		buffnum = 0;
	}
	outbuff[buffnum] = c;
	return (0);
}

buflush ()  {
	if (buffnum < 0)
		return;
	buffnum++;
	if (write (1,outbuff,buffnum) != buffnum)
		errexit ("buflush (write):");
	buffnum = -1;
}

readc () {
	char	c;

	if (tflag)  {
		cline();
		newpos();
	}
	buflush();
	if (read(0,&c,1) != 1)
		errexit ("readc");
#ifdef WHY_IS_THIS_HARDWIRED_IN_HERE
	if (c == '\177')
		getout();
#endif
	if (c == '\033' || c == '\015')
		return ('\n');
	if (cflag)
		return (c);
	if (c == '\014')
		return ('R');
	if (c >= 'a' && c <= 'z')
		return (c & 0137);
	return (c);
}

writec (c)
char	c;
{
	if (tflag)
		fancyc (c);
	else
		addbuf (c);
}

void
writel (l)
const char	*l;
{
#ifdef DEBUG
	const char	*s;

	if (trace == NULL)
		trace = fopen ("bgtrace","w");

	fprintf (trace,"writel: \"");
	for (s = l; *s; s++) {
		if (*s < ' ' || *s == '\177')
			fprintf (trace,"^%c",(*s)^0100);
		else
			putc (*s,trace);
	}
	fprintf (trace,"\"\n");
	fflush (trace);
#endif

	while (*l)
		writec (*l++);
}

proll ()   {
	if (d0)
		swap;
	if (cturn == 1)
		writel ("Red's roll:  ");
	else
		writel ("White's roll:  ");
	writec (D0+'0');
	writec ('\040');
	writec (D1+'0');
	if (tflag)
		cline();
}

wrint (n)
int	n;
{
	int	i, j, t;

	for (i = 4; i > 0; i--)  {
		t = 1;
		for (j = 0; j<i; j++)
			t *= 10;
		if (n > t-1)
			writec ((n/t)%10+'0');
	}
	writec (n%10+'0');
}

gwrite()  {
	int	r, c;

	if (tflag)  {
		r = curr;
		c = curc;
		curmove (16,0);
	}

	if (gvalue > 1)  {
		writel ("Game value:  ");
		wrint (gvalue);
		writel (".  ");
		if (dlast == -1)
			writel (color[0]);
		else
			writel (color[1]);
		writel (" doubled last.");
	} else  {
		switch (pnum)  {
		case -1:			    /* player is red */
			writel (plred);
			break;
		case 0:				    /* player is both colors */
			writel (nocomp);
			break;
		case 1:				    /* player is white */
			writel (plwhite);
		}
	}

	if (rscore || wscore)  {
		writel ("  ");
		wrscore();
	}

	if (tflag)  {
		cline();
		curmove (r,c);
	}
}

quit ()  {
	int	i;

	if (tflag)  {
		curmove (20,0);
		clend();
	} else
		writec ('\n');
	writel ("Are you sure you want to quit?");
	if (yorn (0))  {
		if (rfl)  {
			writel ("Would you like to save this game?");
			if (yorn(0))
				save(0);
		}
		cturn = 0;
		return (1);
	}
	return (0);
}

yorn (special)
char	special;			/* special response */
{
	char	c;
	int	i;

	i = 1;
	while ( (c = readc()) != 'Y' && c != 'N')  {
		if (special && c == special)
			return (2);
		if (i)  {
			if (special)  {
				writel ("  (Y, N, or ");
				writec (special);
				writec (')');
			} else
				writel ("  (Y or N)");
			i = 0;
		} else
			writec ('\007');
	}
	if (c == 'Y')
		writel ("  Yes.\n");
	else
		writel ("  No.\n");
	if (tflag)
		buflush();
	return (c == 'Y');
}

wrhit (i)
int	i;
{
	writel ("Blot hit on ");
	wrint (i);
	writec ('.');
	writec ('\n');
}

nexturn ()  {
	int	c;

	cturn = -cturn;
	c = cturn/abs(cturn);
	home = bar;
	bar = 25-bar;
	offptr += c;
	offopp -= c;
	inptr += c;
	inopp -= c;
	Colorptr += c;
	colorptr += c;
}

getarg (argc, argv)
int    argc;
char	**argv;

{
	char	ch;
	extern int optind;
	extern char *optarg;
	int i;

	/* process arguments here.  dashes are ignored, nbrw are ignored
	   if the game is being recovered */

	while ((ch = getopt (argc, argv, "nbrwp:t:s:h")) != -1) {
		switch (ch)  {

		/* don't ask if rules or instructions needed */
		case 'n':
			if (rflag)
				break;
			aflag = 0;
			args[acnt++] = strdup ("-n");
			break;

		/* player is both red and white */
		case 'b':
			if (rflag)
				break;
			pnum = 0;
			aflag = 0;
			args[acnt++] = strdup ("-b");
			break;

		/* player is red */
		case 'r':
			if (rflag)
				break;
			pnum = -1;
			aflag = 0;
			args[acnt++] = strdup ("-r");
			break;

		/* player is white */
		case 'w':
			if (rflag)
				break;
			pnum = 1;
			aflag = 0;
			args[acnt++] = strdup ("-w");
			break;

		/* print board after move according to following character */
		case 'p':
			if (optarg[0] != 'r' && optarg[0] != 'w' && optarg[0] != 'b')
				break;
			args[acnt] = strdup ("-p ");
			args[acnt++][2] = optarg[0];
			if (optarg[0] == 'r')
				bflag = 1;
			if (optarg[0] == 'w')
				bflag = -1;
			if (optarg[0] == 'b')
				bflag = 0;
			break;

		case 't':
		        tflag = getcaps (optarg);
			break;

		case 's':
			/* recover file */
			recover (optarg);
			break;
		case 'h':
			for (i = 0; descr[i] != 0; i++)
				puts (descr[i]);
			getout();
		}
	}
	argc -= optind;
	argv += optind;
	if ( argc && argv[0][0] != '\0' )
		recover(argv[0]);
}

init ()  {
	int	i;
	for (i = 0; i < 26;)
		board[i++] = 0;
	board[1] = 2;
	board[6] = board[13] = -5;
	board[8] = -3;
	board[12] = board[19] = 5;
	board[17] = 3;
	board[24] = -2;
	off[0] = off[1] = -15;
	in[0] = in[1] = 5;
	gvalue = 1;
	dlast = 0;
}

wrscore ()  {
	writel ("Score:  ");
	writel (color[1]);
	writec (' ');
	wrint (rscore);
	writel (", ");
	writel (color[0]);
	writec (' ');
	wrint (wscore);
}

fixtty (mode)
int	mode;
{
	if (tflag)
		newpos();
	buflush();
	tty.sg_flags = mode;
	if (stty (0,&tty) < 0)
		errexit("fixtty");
}

void
getout ()  {
	/* go to bottom of screen */
	if (tflag)  {
		curmove (23,0);
		cline();
	} else
		writec ('\n');

	/* fix terminal status */
	fixtty (old);
	exit(0);
}
roll ()  {
	char	c;
	int	row;
	int	col;

	if (iroll)  {
		if (tflag)  {
			row = curr;
			col = curc;
			curmove (17,0);
		} else
			writec ('\n');
		writel ("ROLL: ");
		c = readc();
		if (c != '\n')  {
			while (c < '1' || c > '6')
				c = readc();
			D0 = c-'0';
			writec (' ');
			writec (c);
			c = readc();
			while (c < '1' || c > '6')
				c = readc();
			D1 = c-'0';
			writec (' ');
			writec (c);
			if (tflag)  {
				curmove (17,0);
				cline();
				curmove (row,col);
			} else
				writec ('\n');
			return;
		}
		if (tflag)  {
			curmove (17,0);
			cline();
			curmove (row,col);
		} else
			writec ('\n');
	}
	D0 = rnum(6)+1;
	D1 = rnum(6)+1;
	d0 = 0;
}
