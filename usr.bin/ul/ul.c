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
static char sccsid[] = "@(#)ul.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id: ul.c,v 1.2.6.2 1997/08/26 06:18:16 charnier Exp $";
#endif /* not lint */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termcap.h>
#include <unistd.h>

#define	IESC	'\033'
#define	SO	'\016'
#define	SI	'\017'
#define	HFWD	'9'
#define	HREV	'8'
#define	FREV	'7'
#define	MAXBUF	512

#define	NORMAL	000
#define	ALTSET	001	/* Reverse */
#define	SUPERSC	002	/* Dim */
#define	SUBSC	004	/* Dim | Ul */
#define	UNDERL	010	/* Ul */
#define	BOLD	020	/* Bold */

int	must_use_uc, must_overstrike;
char	*CURS_UP, *CURS_RIGHT, *CURS_LEFT,
	*ENTER_STANDOUT, *EXIT_STANDOUT, *ENTER_UNDERLINE, *EXIT_UNDERLINE,
	*ENTER_DIM, *ENTER_BOLD, *ENTER_REVERSE, *UNDER_CHAR, *EXIT_ATTRIBUTES;

struct	CHAR	{
	char	c_mode;
	char	c_char;
} ;

struct	CHAR	obuf[MAXBUF];
int	col, maxcol;
int	mode;
int	halfpos;
int	upln;
int	iflag;

static void usage __P((void));
void setnewmode __P((int));
void initcap __P((void));
void reverse __P((void));
int outchar __P((int));
void fwd __P((void));
void initbuf __P((void));
void iattr __P((void));
void overstrike __P((void));
void flushln __P((void));
void filter __P((FILE *));
void outc __P((int));

#define	PRINT(s)	if (s == NULL) /* void */; else tputs(s, 1, outchar)

int
main(argc, argv)
	int argc;
	char **argv;
{
	int c;
	char *termtype;
	FILE *f;
	char termcap[1024];

	termtype = getenv("TERM");
	if (termtype == NULL || (argv[0][0] == 'c' && !isatty(1)))
		termtype = "lpr";
	while ((c=getopt(argc, argv, "it:T:")) !=  -1)
		switch(c) {

		case 't':
		case 'T': /* for nroff compatibility */
				termtype = optarg;
			break;
		case 'i':
			iflag = 1;
			break;
		default:
			usage();
		}

	switch(tgetent(termcap, termtype)) {

	case 1:
		break;

	default:
		warnx("trouble reading termcap");
		/* fall through to ... */

	case 0:
		/* No such terminal type - assume dumb */
		(void)strcpy(termcap, "dumb:os:col#80:cr=^M:sf=^J:am:");
		break;
	}
	initcap();
	if (    (tgetflag("os") && ENTER_BOLD==NULL ) ||
		(tgetflag("ul") && ENTER_UNDERLINE==NULL && UNDER_CHAR==NULL))
			must_overstrike = 1;
	initbuf();
	if (optind == argc)
		filter(stdin);
	else for (; optind<argc; optind++) {
		f = fopen(argv[optind],"r");
		if (f == NULL)
			err(1, "%s", argv[optind]);
		else
			filter(f);
	}
	exit(0);
}

static void
usage()
{
	fprintf(stderr, "usage: ul [-i] [-t terminal] file...\n");
	exit(1);
}

void
filter(f)
	FILE *f;
{
	register c;

	while ((c = getc(f)) != EOF) switch(c) {

	case '\b':
		if (col > 0)
			col--;
		continue;

	case '\t':
		col = (col+8) & ~07;
		if (col > maxcol)
			maxcol = col;
		continue;

	case '\r':
		col = 0;
		continue;

	case SO:
		mode |= ALTSET;
		continue;

	case SI:
		mode &= ~ALTSET;
		continue;

	case IESC:
		switch (c = getc(f)) {

		case HREV:
			if (halfpos == 0) {
				mode |= SUPERSC;
				halfpos--;
			} else if (halfpos > 0) {
				mode &= ~SUBSC;
				halfpos--;
			} else {
				halfpos = 0;
				reverse();
			}
			continue;

		case HFWD:
			if (halfpos == 0) {
				mode |= SUBSC;
				halfpos++;
			} else if (halfpos < 0) {
				mode &= ~SUPERSC;
				halfpos++;
			} else {
				halfpos = 0;
				fwd();
			}
			continue;

		case FREV:
			reverse();
			continue;

		default:
			errx(1, "unknown escape sequence in input: %o, %o", IESC, c);
		}
		continue;

	case '_':
		if (obuf[col].c_char)
			obuf[col].c_mode |= UNDERL | mode;
		else
			obuf[col].c_char = '_';
	case ' ':
		col++;
		if (col > maxcol)
			maxcol = col;
		continue;

	case '\n':
		flushln();
		continue;

	case '\f':
		flushln();
		putchar('\f');
		continue;

	default:
		if (c < ' ')	/* non printing */
			continue;
		if (obuf[col].c_char == '\0') {
			obuf[col].c_char = c;
			obuf[col].c_mode = mode;
		} else if (obuf[col].c_char == '_') {
			obuf[col].c_char = c;
			obuf[col].c_mode |= UNDERL|mode;
		} else if (obuf[col].c_char == c)
			obuf[col].c_mode |= BOLD|mode;
		else
			obuf[col].c_mode = mode;
		col++;
		if (col > maxcol)
			maxcol = col;
		continue;
	}
	if (maxcol)
		flushln();
}

void
flushln()
{
	register lastmode;
	register i;
	int hadmodes = 0;

	lastmode = NORMAL;
	for (i=0; i<maxcol; i++) {
		if (obuf[i].c_mode != lastmode) {
			hadmodes++;
			setnewmode(obuf[i].c_mode);
			lastmode = obuf[i].c_mode;
		}
		if (obuf[i].c_char == '\0') {
			if (upln)
				PRINT(CURS_RIGHT);
			else
				outc(' ');
		} else
			outc(obuf[i].c_char);
	}
	if (lastmode != NORMAL) {
		setnewmode(0);
	}
	if (must_overstrike && hadmodes)
		overstrike();
	putchar('\n');
	if (iflag && hadmodes)
		iattr();
	(void)fflush(stdout);
	if (upln)
		upln--;
	initbuf();
}

/*
 * For terminals that can overstrike, overstrike underlines and bolds.
 * We don't do anything with halfline ups and downs, or Greek.
 */
void
overstrike()
{
	register int i;
	char lbuf[256];
	register char *cp = lbuf;
	int hadbold=0;

	/* Set up overstrike buffer */
	for (i=0; i<maxcol; i++)
		switch (obuf[i].c_mode) {
		case NORMAL:
		default:
			*cp++ = ' ';
			break;
		case UNDERL:
			*cp++ = '_';
			break;
		case BOLD:
			*cp++ = obuf[i].c_char;
			hadbold=1;
			break;
		}
	putchar('\r');
	for (*cp=' '; *cp==' '; cp--)
		*cp = 0;
	for (cp=lbuf; *cp; cp++)
		putchar(*cp);
	if (hadbold) {
		putchar('\r');
		for (cp=lbuf; *cp; cp++)
			putchar(*cp=='_' ? ' ' : *cp);
		putchar('\r');
		for (cp=lbuf; *cp; cp++)
			putchar(*cp=='_' ? ' ' : *cp);
	}
}

void
iattr()
{
	register int i;
	char lbuf[256];
	register char *cp = lbuf;

	for (i=0; i<maxcol; i++)
		switch (obuf[i].c_mode) {
		case NORMAL:	*cp++ = ' '; break;
		case ALTSET:	*cp++ = 'g'; break;
		case SUPERSC:	*cp++ = '^'; break;
		case SUBSC:	*cp++ = 'v'; break;
		case UNDERL:	*cp++ = '_'; break;
		case BOLD:	*cp++ = '!'; break;
		default:	*cp++ = 'X'; break;
		}
	for (*cp=' '; *cp==' '; cp--)
		*cp = 0;
	for (cp=lbuf; *cp; cp++)
		putchar(*cp);
	putchar('\n');
}

void
initbuf()
{

	bzero((char *)obuf, sizeof (obuf));	/* depends on NORMAL == 0 */
	col = 0;
	maxcol = 0;
	mode &= ALTSET;
}

void
fwd()
{
	register oldcol, oldmax;

	oldcol = col;
	oldmax = maxcol;
	flushln();
	col = oldcol;
	maxcol = oldmax;
}

void
reverse()
{
	upln++;
	fwd();
	PRINT(CURS_UP);
	PRINT(CURS_UP);
	upln++;
}

void
initcap()
{
	static char tcapbuf[512];
	char *bp = tcapbuf;

	/* This nonsense attempts to work with both old and new termcap */
	CURS_UP =		tgetstr("up", &bp);
	CURS_RIGHT =		tgetstr("ri", &bp);
	if (CURS_RIGHT == NULL)
		CURS_RIGHT =	tgetstr("nd", &bp);
	CURS_LEFT =		tgetstr("le", &bp);
	if (CURS_LEFT == NULL)
		CURS_LEFT =	tgetstr("bc", &bp);
	if (CURS_LEFT == NULL && tgetflag("bs"))
		CURS_LEFT =	"\b";

	ENTER_STANDOUT =	tgetstr("so", &bp);
	EXIT_STANDOUT =		tgetstr("se", &bp);
	ENTER_UNDERLINE =	tgetstr("us", &bp);
	EXIT_UNDERLINE =	tgetstr("ue", &bp);
	ENTER_DIM =		tgetstr("mh", &bp);
	ENTER_BOLD =		tgetstr("md", &bp);
	ENTER_REVERSE =		tgetstr("mr", &bp);
	EXIT_ATTRIBUTES =	tgetstr("me", &bp);

	if (!ENTER_BOLD && ENTER_REVERSE)
		ENTER_BOLD = ENTER_REVERSE;
	if (!ENTER_BOLD && ENTER_STANDOUT)
		ENTER_BOLD = ENTER_STANDOUT;
	if (!ENTER_UNDERLINE && ENTER_STANDOUT) {
		ENTER_UNDERLINE = ENTER_STANDOUT;
		EXIT_UNDERLINE = EXIT_STANDOUT;
	}
	if (!ENTER_DIM && ENTER_STANDOUT)
		ENTER_DIM = ENTER_STANDOUT;
	if (!ENTER_REVERSE && ENTER_STANDOUT)
		ENTER_REVERSE = ENTER_STANDOUT;
	if (!EXIT_ATTRIBUTES && EXIT_STANDOUT)
		EXIT_ATTRIBUTES = EXIT_STANDOUT;

	/*
	 * Note that we use REVERSE for the alternate character set,
	 * not the as/ae capabilities.  This is because we are modelling
	 * the model 37 teletype (since that's what nroff outputs) and
	 * the typical as/ae is more of a graphics set, not the greek
	 * letters the 37 has.
	 */

	UNDER_CHAR =		tgetstr("uc", &bp);
	must_use_uc = (UNDER_CHAR && !ENTER_UNDERLINE);
}

int
outchar(c)
	int c;
{
	return(putchar(c & 0177));
}

static int curmode = 0;

void
outc(c)
	int c;
{
	putchar(c);
	if (must_use_uc && (curmode&UNDERL)) {
		PRINT(CURS_LEFT);
		PRINT(UNDER_CHAR);
	}
}

void
setnewmode(newmode)
	int newmode;
{
	if (!iflag) {
		if (curmode != NORMAL && newmode != NORMAL)
			setnewmode(NORMAL);
		switch (newmode) {
		case NORMAL:
			switch(curmode) {
			case NORMAL:
				break;
			case UNDERL:
				PRINT(EXIT_UNDERLINE);
				break;
			default:
				/* This includes standout */
				PRINT(EXIT_ATTRIBUTES);
				break;
			}
			break;
		case ALTSET:
			PRINT(ENTER_REVERSE);
			break;
		case SUPERSC:
			/*
			 * This only works on a few terminals.
			 * It should be fixed.
			 */
			PRINT(ENTER_UNDERLINE);
			PRINT(ENTER_DIM);
			break;
		case SUBSC:
			PRINT(ENTER_DIM);
			break;
		case UNDERL:
			PRINT(ENTER_UNDERLINE);
			break;
		case BOLD:
			PRINT(ENTER_BOLD);
			break;
		default:
			/*
			 * We should have some provision here for multiple modes
			 * on at once.  This will have to come later.
			 */
			PRINT(ENTER_STANDOUT);
			break;
		}
	}
	curmode = newmode;
}
