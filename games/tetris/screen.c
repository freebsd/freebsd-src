/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Chris Torek and Darren F. Provine.
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
 *
 *	@(#)screen.c	8.1 (Berkeley) 5/31/93
 */

/*
 * Tetris screen control.
 */

#include <sgtty.h>
#include <sys/ioctl.h>

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef sigmask
#define sigmask(s) (1 << ((s) - 1))
#endif

#include "screen.h"
#include "tetris.h"

/*
 * XXX - need a <termcap.h>
 */
int	tgetent __P((char *, const char *));
int	tgetflag __P((const char *));
int	tgetnum __P((const char *));
int	tputs __P((const char *, int, int (*)(int)));

static cell curscreen[B_SIZE];	/* 1 => standout (or otherwise marked) */
static int curscore;
static int isset;		/* true => terminal is in game mode */
static struct sgttyb oldtt;
static void (*tstp)();

char	*tgetstr(), *tgoto();


/*
 * Capabilities from TERMCAP.
 */
char	PC, *BC, *UP;		/* tgoto requires globals: ugh! */
short	ospeed;

static char
	*bcstr,			/* backspace char */
	*CEstr,			/* clear to end of line */
	*CLstr,			/* clear screen */
	*CMstr,			/* cursor motion string */
#ifdef unneeded
	*CRstr,			/* "\r" equivalent */
#endif
	*HOstr,			/* cursor home */
	*LLstr,			/* last line, first column */
	*pcstr,			/* pad character */
	*TEstr,			/* end cursor motion mode */
	*TIstr;			/* begin cursor motion mode */
char
	*SEstr,			/* end standout mode */
	*SOstr;			/* begin standout mode */
static int
	COnum,			/* co# value */
	LInum,			/* li# value */
	MSflag;			/* can move in standout mode */


struct tcsinfo {	/* termcap string info; some abbrevs above */
	char tcname[3];
	char **tcaddr;
} tcstrings[] = {
	"bc", &bcstr,
	"ce", &CEstr,
	"cl", &CLstr,
	"cm", &CMstr,
#ifdef unneeded
	"cr", &CRstr,
#endif
	"le", &BC,		/* move cursor left one space */
	"pc", &pcstr,
	"se", &SEstr,
	"so", &SOstr,
	"te", &TEstr,
	"ti", &TIstr,
	"up", &UP,		/* cursor up */
	0
};

/* This is where we will actually stuff the information */

static char combuf[1024], tbuf[1024];


/*
 * Routine used by tputs().
 */
int
put(c)
	int c;
{

	return (putchar(c));
}

/*
 * putstr() is for unpadded strings (either as in termcap(5) or
 * simply literal strings); putpad() is for padded strings with
 * count=1.  (See screen.h for putpad().)
 */
#define	putstr(s)	(void)fputs(s, stdout)
#define	moveto(r, c)	putpad(tgoto(CMstr, c, r))

/*
 * Set up from termcap.
 */
void
scr_init()
{
	static int bsflag, xsflag, sgnum;
#ifdef unneeded
	static int ncflag;
#endif
	char *term, *fill;
	static struct tcninfo {	/* termcap numeric and flag info */
		char tcname[3];
		int *tcaddr;
	} tcflags[] = {
		"bs", &bsflag,
		"ms", &MSflag,
#ifdef unneeded
		"nc", &ncflag,
#endif
		"xs", &xsflag,
		0
	}, tcnums[] = {
		"co", &COnum,
		"li", &LInum,
		"sg", &sgnum,
		0
	};
	
	if ((term = getenv("TERM")) == NULL)
		stop("you must set the TERM environment variable");
	if (tgetent(tbuf, term) <= 0)
		stop("cannot find your termcap");
	fill = combuf;
	{
		register struct tcsinfo *p;

		for (p = tcstrings; p->tcaddr; p++)
			*p->tcaddr = tgetstr(p->tcname, &fill);
	}
	{
		register struct tcninfo *p;

		for (p = tcflags; p->tcaddr; p++)
			*p->tcaddr = tgetflag(p->tcname);
		for (p = tcnums; p->tcaddr; p++)
			*p->tcaddr = tgetnum(p->tcname);
	}
	if (bsflag)
		BC = "\b";
	else if (BC == NULL && bcstr != NULL)
		BC = bcstr;
	if (CLstr == NULL)
		stop("cannot clear screen");
	if (CMstr == NULL || UP == NULL || BC == NULL)
		stop("cannot do random cursor positioning via tgoto()");
	PC = pcstr ? *pcstr : 0;
	if (sgnum >= 0 || xsflag)
		SOstr = SEstr = NULL;
#ifdef unneeded
	if (ncflag)
		CRstr = NULL;
	else if (CRstr == NULL)
		CRstr = "\r";
#endif
}

/* this foolery is needed to modify tty state `atomically' */
static jmp_buf scr_onstop;

#define	sigunblock(mask) sigsetmask(sigblock(0) & ~(mask))

static void
stopset(sig)
	int sig;
{
	(void) signal(sig, SIG_DFL);
	(void) kill(getpid(), sig);
	(void) sigunblock(sigmask(sig));
	longjmp(scr_onstop, 1);
}

static void
scr_stop()
{
	scr_end();
	(void) kill(getpid(), SIGTSTP);
	(void) sigunblock(sigmask(SIGTSTP));
	scr_set();
	scr_msg(key_msg, 1);
}

/*
 * Set up screen mode.
 */
void
scr_set()
{
	struct winsize ws;
	struct sgttyb newtt;
	volatile int omask;
	void (*ttou)();

	omask = sigblock(sigmask(SIGTSTP) | sigmask(SIGTTOU));
	if ((tstp = signal(SIGTSTP, stopset)) == SIG_IGN)
		(void) signal(SIGTSTP, SIG_IGN);
	if ((ttou = signal(SIGTSTP, stopset)) == SIG_IGN)
		(void) signal(SIGTSTP, SIG_IGN);
	/*
	 * At last, we are ready to modify the tty state.  If
	 * we stop while at it, stopset() above will longjmp back
	 * to the setjmp here and we will start over.
	 */
	(void) setjmp(scr_onstop);
	(void) sigsetmask(omask);
	Rows = 0, Cols = 0;
	if (ioctl(0, TIOCGWINSZ, &ws) == 0) {
		Rows = ws.ws_row;
		Cols = ws.ws_col;
	}
	if (Rows == 0)
		Rows = LInum;
	if (Cols == 0)
	Cols = COnum;
	if (Rows < MINROWS || Cols < MINCOLS) {
		(void) fprintf(stderr,
		    "the screen is too small: must be at least %d x %d",
		    MINROWS, MINCOLS);
		stop("");	/* stop() supplies \n */
	}
	if (ioctl(0, TIOCGETP, &oldtt))
		stop("ioctl(TIOCGETP) fails");
	newtt = oldtt;
	newtt.sg_flags = (newtt.sg_flags | CBREAK) & ~(CRMOD | ECHO);
	if ((newtt.sg_flags & TBDELAY) == XTABS)
		newtt.sg_flags &= ~TBDELAY;
	if (ioctl(0, TIOCSETN, &newtt))
		stop("ioctl(TIOCSETN) fails");
	ospeed = newtt.sg_ospeed;
	omask = sigblock(sigmask(SIGTSTP) | sigmask(SIGTTOU));

	/*
	 * We made it.  We are now in screen mode, modulo TIstr
	 * (which we will fix immediately).
	 */
	if (TIstr)
		putstr(TIstr);	/* termcap(5) says this is not padded */
	if (tstp != SIG_IGN)
		(void) signal(SIGTSTP, scr_stop);
	(void) signal(SIGTTOU, ttou);

	isset = 1;
	(void) sigsetmask(omask);
	scr_clear();
}

/*
 * End screen mode.
 */
void
scr_end()
{
	int omask = sigblock(sigmask(SIGTSTP) | sigmask(SIGTTOU));

	/* move cursor to last line */
	if (LLstr)
		putstr(LLstr);	/* termcap(5) says this is not padded */
	else
		moveto(Rows - 1, 0);
	/* exit screen mode */
	if (TEstr)
		putstr(TEstr);	/* termcap(5) says this is not padded */
	(void) fflush(stdout);
	(void) ioctl(0, TIOCSETN, &oldtt);
	isset = 0;
	/* restore signals */
	(void) signal(SIGTSTP, tstp);
	(void) sigsetmask(omask);
}

void
stop(why)
	char *why;
{

	if (isset)
		scr_end();
	(void) fprintf(stderr, "aborting: %s\n", why);
	exit(1);
}

/*
 * Clear the screen, forgetting the current contents in the process.
 */
void
scr_clear()
{

	putpad(CLstr);
	curscore = -1;
	bzero((char *)curscreen, sizeof(curscreen));
}

#if vax && !__GNUC__
typedef int regcell;	/* pcc is bad at `register char', etc */
#else
typedef cell regcell;
#endif

/*
 * Update the screen.
 */
void
scr_update()
{
	register cell *bp, *sp;
	register regcell so, cur_so = 0;
	register int i, ccol, j;
	int omask = sigblock(sigmask(SIGTSTP));

	/* always leave cursor after last displayed point */
	curscreen[D_LAST * B_COLS - 1] = -1;

	if (score != curscore) {
		if (HOstr)
			putpad(HOstr);
		else
			moveto(0, 0);
		(void) printf("%d", score);
		curscore = score;
	}

	bp = &board[D_FIRST * B_COLS];
	sp = &curscreen[D_FIRST * B_COLS];
	for (j = D_FIRST; j < D_LAST; j++) {
		ccol = -1;
		for (i = 0; i < B_COLS; bp++, sp++, i++) {
			if (*sp == (so = *bp))
				continue;
			*sp = so;
			if (i != ccol) {
				if (cur_so && MSflag) {
					putpad(SEstr);
					cur_so = 0;
				}
				moveto(RTOD(j), CTOD(i));
			}
			if (SOstr) {
				if (so != cur_so) {
					putpad(so ? SOstr : SEstr);
					cur_so = so;
				}
				putstr("  ");
			} else
				putstr(so ? "XX" : "  ");
			ccol = i + 1;
			/*
			 * Look ahead a bit, to avoid extra motion if
			 * we will be redrawing the cell after the next.
			 * Motion probably takes four or more characters,
			 * so we save even if we rewrite two cells
			 * `unnecessarily'.  Skip it all, though, if
			 * the next cell is a different color.
			 */
#define	STOP (B_COLS - 3)
			if (i > STOP || sp[1] != bp[1] || so != bp[1])
				continue;
			if (sp[2] != bp[2])
				sp[1] = -1;
			else if (i < STOP && so == bp[2] && sp[3] != bp[3]) {
				sp[2] = -1;
				sp[1] = -1;
			}
		}
	}
	if (cur_so)
		putpad(SEstr);
	(void) fflush(stdout);
	(void) sigsetmask(omask);
}

/*
 * Write a message (set!=0), or clear the same message (set==0).
 * (We need its length in case we have to overwrite with blanks.)
 */
void
scr_msg(s, set)
	register char *s;
	int set;
{
	
	if (set || CEstr == NULL) {
		register int l = strlen(s);

		moveto(Rows - 2, ((Cols - l) >> 1) - 1);
		if (set)
			putstr(s);
		else
			while (--l >= 0)
				(void) putchar(' ');
	} else {
		moveto(Rows - 2, 0);
		putpad(CEstr);
	}
}
