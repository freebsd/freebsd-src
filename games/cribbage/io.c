/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)io.c	5.8 (Berkeley) 2/28/91";
#endif /* not lint */

# include	<curses.h>
# include	<ctype.h>
# include	<signal.h>
# include	<stdarg.h>
# include	"deck.h"
# include	"cribbage.h"
# include	"cribcur.h"

# define	LINESIZE		128

# ifdef CTRL
# undef CTRL
# endif
# define	CTRL(X)			('X' - 'A' + 1)

# ifdef	notdef				/* defined in curses.h */
#	define	erasechar()	_tty.sg_erase
#	define	killchar()	_tty.sg_kill
# endif

char		linebuf[ LINESIZE ];

char		*rankname[ RANKS ]	= { "ACE", "TWO", "THREE", "FOUR",
					    "FIVE", "SIX", "SEVEN", "EIGHT",
					    "NINE", "TEN", "JACK", "QUEEN",
					    "KING" };

char            *rankchar[ RANKS ]      = { "A", "2", "3", "4", "5", "6", "7",
					    "8", "9", "T", "J", "Q", "K" };

char            *suitname[ SUITS ]      = { "SPADES", "HEARTS", "DIAMONDS",
					    "CLUBS" };

char            *suitchar[ SUITS ]      = { "S", "H", "D", "C" };



/*
 * msgcard:
 *	Call msgcrd in one of two forms
 */
msgcard(c, brief)
CARD		c;
BOOLEAN		brief;
{
	if (brief)
		return msgcrd(c, TRUE, (char *) NULL, TRUE);
	else
		return msgcrd(c, FALSE, " of ", FALSE);
}



/*
 * msgcrd:
 *	Print the value of a card in ascii
 */
msgcrd(c, brfrank, mid, brfsuit)
CARD		c;
char		*mid;
BOOLEAN		brfrank,  brfsuit;
{
	if (c.rank == EMPTY || c.suit == EMPTY)
	    return FALSE;
	if (brfrank)
	    addmsg("%1.1s", rankchar[c.rank]);
	else
	    addmsg(rankname[c.rank]);
	if (mid != NULL)
	    addmsg(mid);
	if (brfsuit)
	    addmsg("%1.1s", suitchar[c.suit]);
	else
	    addmsg(suitname[c.suit]);
	return TRUE;
}

/*
 * printcard:
 *	Print out a card.
 */
printcard(win, cardno, c, blank)
WINDOW		*win;
int		cardno;
CARD		c;
BOOLEAN		blank;
{
	prcard(win, cardno * 2, cardno, c, blank);
}

/*
 * prcard:
 *	Print out a card on the window at the specified location
 */
prcard(win, y, x, c, blank)
WINDOW		*win;
int		y, x;
CARD		c;
BOOLEAN		blank;
{
	if (c.rank == EMPTY)
	    return;
	mvwaddstr(win, y + 0, x, "+-----+");
	mvwaddstr(win, y + 1, x, "|     |");
	mvwaddstr(win, y + 2, x, "|     |");
	mvwaddstr(win, y + 3, x, "|     |");
	mvwaddstr(win, y + 4, x, "+-----+");
	if (!blank) {
		mvwaddch(win, y + 1, x + 1, rankchar[c.rank][0]);
		waddch(win, suitchar[c.suit][0]);
		mvwaddch(win, y + 3, x + 4, rankchar[c.rank][0]);
		waddch(win, suitchar[c.suit][0]);
	}
}

/*
 * prhand:
 *	Print a hand of n cards
 */
prhand(h, n, win, blank)
CARD		h[];
int		n;
WINDOW		*win;
BOOLEAN		blank;
{
	register int	i;

	werase(win);
	for (i = 0; i < n; i++)
	    printcard(win, i, *h++, blank);
	wrefresh(win);
}



/*
 * infrom:
 *	reads a card, supposedly in hand, accepting unambigous brief
 *	input, returns the index of the card found...
 */
infrom(hand, n, prompt)
CARD		hand[];
int		n;
char		*prompt;
{
	register int           i, j;
	CARD                    crd;

	if (n < 1) {
	    printf("\nINFROM: %d = n < 1!!\n", n);
	    exit(74);
	}
	for (;;) {
	    msg(prompt);
	    if (incard(&crd)) {			/* if card is full card */
		if (!isone(crd, hand, n))
		    msg("That's not in your hand");
		else {
		    for (i = 0; i < n; i++)
			if (hand[i].rank == crd.rank &&
			    hand[i].suit == crd.suit)
				break;
		    if (i >= n) {
			printf("\nINFROM: isone or something messed up\n");
			exit(77);
		    }
		    return i;
		}
	    }
	    else				/* if not full card... */
		if (crd.rank != EMPTY) {
		    for (i = 0; i < n; i++)
			if (hand[i].rank == crd.rank)
				break;
		    if (i >= n)
			msg("No such rank in your hand");
		    else {
			for (j = i + 1; j < n; j++)
			    if (hand[j].rank == crd.rank)
				break;
			if (j < n)
			    msg("Ambiguous rank");
			else
			    return i;
		    }
		}
		else
		    msg("Sorry, I missed that");
	}
	/* NOTREACHED */
}



/*
 * incard:
 *	Inputs a card in any format.  It reads a line ending with a CR
 *	and then parses it.
 */
incard(crd)
CARD		*crd;
{
	char		*getline();
	register int	i;
	int		rnk, sut;
	char		*line, *p, *p1;
	BOOLEAN		retval;

	retval = FALSE;
	rnk = sut = EMPTY;
	if (!(line = getline()))
		goto gotit;
	p = p1 = line;
	while(  *p1 != ' '  &&  *p1 != NULL  )  ++p1;
	*p1++ = NULL;
	if(  *p == NULL  )  goto  gotit;
			/* IMPORTANT: no real card has 2 char first name */
	if(  strlen(p) == 2  )  {               /* check for short form */
	    rnk = EMPTY;
	    for( i = 0; i < RANKS; i++ )  {
		if(  *p == *rankchar[i]  )  {
		    rnk = i;
		    break;
		}
	    }
	    if(  rnk == EMPTY  )  goto  gotit;     /* it's nothing... */
	    ++p;                                /* advance to next char */
	    sut = EMPTY;
	    for( i = 0; i < SUITS; i++ )  {
		if(  *p == *suitchar[i]  )  {
		    sut = i;
		    break;
		}
	    }
	    if(  sut != EMPTY  )  retval = TRUE;
	    goto  gotit;
	}
	rnk = EMPTY;
	for( i = 0; i < RANKS; i++ )  {
	    if(  !strcmp( p, rankname[i] )  ||  !strcmp( p, rankchar[i] )  )  {
		rnk = i;
		break;
	    }
	}
	if(  rnk == EMPTY  )  goto  gotit;
	p = p1;
	while(  *p1 != ' '  &&  *p1 != NULL  )  ++p1;
	*p1++ = NULL;
	if(  *p == NULL  )  goto  gotit;
	if(  !strcmp( "OF", p )  )  {
	    p = p1;
	    while(  *p1 != ' '  &&  *p1 != NULL  )  ++p1;
	    *p1++ = NULL;
	    if(  *p == NULL  )  goto  gotit;
	}
	sut = EMPTY;
	for( i = 0; i < SUITS; i++ )  {
	    if(  !strcmp( p, suitname[i] )  ||  !strcmp( p, suitchar[i] )  )  {
		sut = i;
		break;
	    }
	}
	if(  sut != EMPTY  )  retval = TRUE;
gotit:
	(*crd).rank = rnk;
	(*crd).suit = sut;
	return( retval );
}



/*
 * getuchar:
 *	Reads and converts to upper case
 */
getuchar()
{
	register int		c;

	c = readchar();
	if (islower(c))
	    c = toupper(c);
	waddch(Msgwin, c);
	return c;
}

/*
 * number:
 *	Reads in a decimal number and makes sure it is between "lo" and
 *	"hi" inclusive.
 */
number(lo, hi, prompt)
int		lo, hi;
char		*prompt;
{
	char			*getline();
	register char		*p;
	register int		sum;

	sum = 0;
	for (;;) {
	    msg(prompt);
	    if(!(p = getline()) || *p == NULL) {
		msg(quiet ? "Not a number" : "That doesn't look like a number");
		continue;
	    }
	    sum = 0;

	    if (!isdigit(*p))
		sum = lo - 1;
	    else
		while (isdigit(*p)) {
		    sum = 10 * sum + (*p - '0');
		    ++p;
		}

	    if (*p != ' ' && *p != '\t' && *p != NULL)
		sum = lo - 1;
	    if (sum >= lo && sum <= hi)
		return sum;
	    if (sum == lo - 1)
		msg("that doesn't look like a number, try again --> ");
	    else
		msg("%d is not between %d and %d inclusive, try again --> ",
								sum, lo, hi);
	}
}

/*
 * msg:
 *	Display a message at the top of the screen.
 */
char		Msgbuf[BUFSIZ] = { '\0' };

int		Mpos = 0;

static int	Newpos = 0;

/* VARARGS1 */
msg(fmt)
	char *fmt;
{
	va_list ap;

	va_start(ap, fmt);
	(void)vsprintf(&Msgbuf[Newpos], fmt, ap);
	va_end(ap);
	endmsg();
}

/*
 * addmsg:
 *	Add things to the current message
 */
/* VARARGS1 */
addmsg(fmt)
	char *fmt;
{
	va_list ap;

	va_start(ap, fmt);
	(void)vsprintf(&Msgbuf[Newpos], fmt, ap);
	va_end(ap);
}

/*
 * endmsg:
 *	Display a new msg.
 */

int	Lineno = 0;

endmsg()
{
    register int	len;
    register char	*mp, *omp;
    static int		lastline = 0;

    /*
     * All messages should start with uppercase
     */
    mvaddch(lastline + Y_MSG_START, SCORE_X, ' ');
    if (islower(Msgbuf[0]) && Msgbuf[1] != ')')
	Msgbuf[0] = toupper(Msgbuf[0]);
    mp = Msgbuf;
    len = strlen(mp);
    if (len / MSG_X + Lineno >= MSG_Y) {
	while (Lineno < MSG_Y) {
	    wmove(Msgwin, Lineno++, 0);
	    wclrtoeol(Msgwin);
	}
	Lineno = 0;
    }
    mvaddch(Lineno + Y_MSG_START, SCORE_X, '*');
    lastline = Lineno;
    do {
	mvwaddstr(Msgwin, Lineno, 0, mp);
	if ((len = strlen(mp)) > MSG_X) {
	    omp = mp;
	    for (mp = &mp[MSG_X-1]; *mp != ' '; mp--)
	    	continue;
	    while (*mp == ' ')
		mp--;
	    mp++;
	    wmove(Msgwin, Lineno, mp - omp);
	    wclrtoeol(Msgwin);
	}
	if (++Lineno >= MSG_Y)
	    Lineno = 0;
    } while (len > MSG_X);
    wclrtoeol(Msgwin);
    Mpos = len;
    Newpos = 0;
    wrefresh(Msgwin);
    refresh();
    wrefresh(Msgwin);
}

#ifdef notdef
/*
 * doadd:
 *	Perform an add onto the message buffer
 */
doadd(fmt, args)
char	*fmt;
int	*args;
{
    static FILE	junk;

    /*
     * Do the printf into Msgbuf
     */
    junk._flag = _IOWRT + _IOSTRG;
    junk._ptr = &Msgbuf[Newpos];
    junk._cnt = 32767;
    _doprnt(fmt, args, &junk);
    putc('\0', &junk);
    Newpos = strlen(Msgbuf);
}
#endif

/*
 * do_wait:
 *	Wait for the user to type ' ' before doing anything else
 */
do_wait()
{
    register int line;
    static char prompt[] = { '-', '-', 'M', 'o', 'r', 'e', '-', '-', '\0' };

    if (Mpos + sizeof prompt < MSG_X)
	wmove(Msgwin, Lineno > 0 ? Lineno - 1 : MSG_Y - 1, Mpos);
    else {
	mvwaddch(Msgwin, Lineno, 0, ' ');
	wclrtoeol(Msgwin);
	if (++Lineno >= MSG_Y)
	    Lineno = 0;
    }
    waddstr(Msgwin, prompt);
    wrefresh(Msgwin);
    wait_for(' ');
}

/*
 * wait_for
 *	Sit around until the guy types the right key
 */
wait_for(ch)
register char	ch;
{
    register char	c;

    if (ch == '\n')
	while ((c = readchar()) != '\n')
	    continue;
    else
	while (readchar() != ch)
	    continue;
}

/*
 * readchar:
 *	Reads and returns a character, checking for gross input errors
 */
readchar()
{
    register int	cnt, y, x;
    auto char		c;

over:
    cnt = 0;
    while (read(0, &c, 1) <= 0)
	if (cnt++ > 100) {	/* if we are getting infinite EOFs */
		bye();		/* quit the game */
		exit(1);
	}
    if (c == CTRL(L)) {
	wrefresh(curscr);
	goto over;
    }
    if (c == '\r')
	return '\n';
    else
	return c;
}

/*
 * getline:
 *      Reads the next line up to '\n' or EOF.  Multiple spaces are
 *	compressed to one space; a space is inserted before a ','
 */
char *
getline()
{
    register char	*sp;
    register int	c, oy, ox;
    register WINDOW	*oscr;

    oscr = stdscr;
    stdscr = Msgwin;
    getyx(stdscr, oy, ox);
    refresh();
    /*
     * loop reading in the string, and put it in a temporary buffer
     */
    for (sp = linebuf; (c = readchar()) != '\n'; clrtoeol(), refresh()) {
	if (c == -1)
	    continue;
	else if (c == erasechar()) {	/* process erase character */
	    if (sp > linebuf) {
		register int i;

		sp--;
		for (i = strlen(unctrl(*sp)); i; i--)
		    addch('\b');
	    }
	    continue;
	}
	else if (c == killchar()) {	/* process kill character */
	    sp = linebuf;
	    move(oy, ox);
	    continue;
	}
	else if (sp == linebuf && c == ' ')
	    continue;
	if (sp >= &linebuf[LINESIZE-1] || !(isprint(c) || c == ' '))
	    putchar(CTRL(G));
	else {
	    if (islower(c))
		c = toupper(c);
	    *sp++ = c;
	    addstr(unctrl(c));
	    Mpos++;
	}
    }
    *sp = '\0';
    stdscr = oscr;
    return linebuf;
}

rint()
{
	bye();
	exit(1);
}

/*
 * bye:
 *	Leave the program, cleaning things up as we go.
 */
bye()
{
	signal(SIGINT, SIG_IGN);
	mvcur(0, COLS - 1, LINES - 1, 0);
	fflush(stdout);
	endwin();
	putchar('\n');
}
