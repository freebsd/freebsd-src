/*-
 * Copyright (c) 1988 The Regents of the University of California.
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
static char sccsid[] = "@(#)termout.c	4.3 (Berkeley) 4/26/91";
#endif /* not lint */

#if defined(unix)
#include <signal.h>
#include <sgtty.h>
#endif
#include <stdio.h>
#include <curses.h>
#if	defined(ultrix)
/* Some version of this OS has a bad definition for nonl() */
#undef	nl
#undef	nonl

#define nl()	 (_tty.sg_flags |= CRMOD,_pfast = _rawmode,stty(_tty_ch, &_tty))
#define nonl()	 (_tty.sg_flags &= ~CRMOD, _pfast = TRUE, stty(_tty_ch, &_tty))
#endif	/* defined(ultrix) */

#include "../general/general.h"

#include "terminal.h"

#include "../api/disp_asc.h"

#include "../ctlr/hostctlr.h"
#include "../ctlr/externs.h"
#include "../ctlr/declare.h"
#include "../ctlr/oia.h"
#include "../ctlr/screen.h"
#include "../ctlr/scrnctlr.h"

#include "../general/globals.h"

#include "../telextrn.h"

#define CorrectTerminalCursor() ((TransparentClock == OutputClock)? \
		CursorAddress:UnLocked? CursorAddress: HighestScreen())


static int terminalCursorAddress;	/* where the cursor is on term */
static int screenInitd; 		/* the screen has been initialized */
static int screenStopped;		/* the screen has been stopped */
static int max_changes_before_poll;	/* how many characters before looking */
					/* at terminal and net again */

static int needToRing;			/* need to ring terinal bell */
static char *bellSequence = "\07";	/* bell sequence (may be replaced by
					 * VB during initialization)
					 */
static WINDOW *bellwin = 0;		/* The window the bell message is in */
int	bellwinup = 0;			/* Are we up with it or not */

#if	defined(unix)
static char *myKS, *myKE;
#endif	/* defined(unix) */


static int inHighlightMode = 0;
ScreenImage Terminal[MAXSCREENSIZE];

/* Variables for transparent mode */
#if	defined(unix)
static int tcflag = -1;			/* transparent mode command flag */
static int savefd[2];			/* for storing fds during transcom */
extern int	tin, tout;		/* file descriptors */
#endif	/* defined(unix) */


/*
 * init_screen()
 *
 * Initialize variables used by screen.
 */

void
init_screen()
{
    bellwinup = 0;
    inHighlightMode = 0;
    ClearArray(Terminal);
}


/* OurExitString - designed to keep us from going through infinite recursion */

static void
OurExitString(string, value)
char	*string;
int	value;
{
    static int recursion = 0;

    if (!recursion) {
	recursion = 1;
	ExitString(string, value);
    }
}


/* DoARefresh */

static void
DoARefresh()
{
    if (ERR == refresh()) {
	OurExitString("ERR from refresh\n", 1);
    }
}

static void
GoAway(from, where)
char *from;		/* routine that gave error */
int	where;		/* cursor address */
{
	char foo[100];

	sprintf(foo, "ERR from %s at %d (%d, %d)\n",
		from, where, ScreenLine(where), ScreenLineOffset(where));
	OurExitString(foo, 1);
	/* NOTREACHED */
}

/* What is the screen address of the attribute byte for the terminal */

static int
WhereTermAttrByte(p)
register int	p;
{
    register int i;

    i = p;

    do {
	if (TermIsStartField(i)) {
	    return(i);
	}
	i = ScreenDec(i);
    } while (i != p);

    return(LowestScreen());	/* unformatted screen... */
}

/*
 *	There are two algorithms for updating the screen.
 *  The first, SlowScreen() optimizes the line between the
 *  computer and the screen (say a 9600 baud line).  To do
 *  this, we break out of the loop every so often to look
 *  at any pending input from the network (so that successive
 *  screens will only partially print until the final screen,
 *  the one the user possibly wants to see, is displayed
 *  in its entirety).
 *
 *	The second algorithm tries to optimize CPU time (by
 *  being simpler) at the cost of the bandwidth to the
 *  screen.
 *
 *	Of course, curses(3X) gets in here also.
 */


#if	defined(NOT43)
static int
#else	/* defined(NOT43) */
static void
#endif	/* defined(NOT43) */
SlowScreen()
{
    register int is, shouldbe, isattr, shouldattr;
    register int pointer;
    register int fieldattr, termattr;
    register int columnsleft;

#define	NORMAL		0		
#define	HIGHLIGHT	1		/* Mask bits */
#define	NONDISPLAY	4		/* Mask bits */
#define	UNDETERMINED	8		/* Mask bits */

#define	DoAttributes(x) \
	    switch (x&ATTR_DSPD_MASK) { \
	    case ATTR_DSPD_NONDISPLAY: \
		x = NONDISPLAY; \
		break; \
	    case ATTR_DSPD_HIGH: \
		x = HIGHLIGHT; \
		break; \
	    default: \
		x = 0; \
		break; \
	    }

#   define  SetHighlightMode(x) \
	    { \
		if ((x)&HIGHLIGHT) { \
		    if (!inHighlightMode) { \
			inHighlightMode = HIGHLIGHT; \
			standout(); \
		    } \
		} else { \
		    if (inHighlightMode) { \
			inHighlightMode = 0; \
			standend(); \
		    } \
		} \
	    }

#   define  DoCharacterAt(c,p) { \
		if (p != HighestScreen()) { \
		    c = disp_asc[c&0xff]; \
		    if (terminalCursorAddress != p) { \
			if (ERR == mvaddch(ScreenLine(p), \
						ScreenLineOffset(p), c)) {\
			    GoAway("mvaddch", p); \
			} \
		    } else { \
			if (ERR == addch(c)) {\
			    GoAway("addch", p); \
			} \
		    } \
		    terminalCursorAddress = ScreenInc(p); \
		} \
	    }


    /* run through screen, printing out non-null lines */

    /* There are two separate reasons for wanting to terminate this
     * loop early.  One is to respond to new input (either from
     * the terminal or from the network [host]).  For this reason,
     * we expect to see 'HaveInput' come true when new input comes in.
     *
     * The second reason is a bit more difficult (for me) to understand.
     * Basically, we don't want to get too far ahead of the characters that
     * appear on the screen.  Ideally, we would type out a few characters,
     * wait until they appeared on the screen, then type out a few more.
     * The reason for this is that the user, on seeing some characters
     * appear on the screen may then start to type something.  We would
     * like to look at what the user types at about the same 'time'
     * (measured by characters being sent to the terminal) that the
     * user types them.  For this reason, what we would like to do
     * is update a bit, then call curses to do a refresh, flush the
     * output to the terminal, then wait until the terminal data
     * has been sent.
     *
     * Note that curses is useful for, among other things, deciding whether
     * or not to send :ce: (clear to end of line), so we should call curses
     * at end of lines (beginning of next lines).
     *
     * The problems here are the following:  If we do lots of write(2)s,
     * we will be doing lots of context switches, thus lots of overhead
     * (which we have already).  Second, if we do a select to wait for
     * the output to drain, we have to contend with the fact that NOW
     * we are scheduled to run, but who knows what the scheduler will
     * decide when the output has caught up.
     */

    if (Highest >= HighestScreen()) {	/* Could be > if screen shrunk... */
	Highest = ScreenDec(Highest);	/* else, while loop will never end */
    }
    if (Lowest < LowestScreen()) {
	Lowest = LowestScreen();	/* could be -1 in some cases with
					 * unformatted screens.
					 */
    }
    if (Highest >= (pointer = Lowest)) {
		/* if there is anything to do, do it.  We won't terminate
		 * the loop until we've gone at least to Highest.
		 */
	while ((pointer <= Highest) && !HaveInput) {

		/* point at the next place of disagreement */
	    pointer += (bunequal(Host+pointer, Terminal+pointer,
			(Highest-pointer+1)*sizeof Host[0])/sizeof Host[0]);

		/*
		 * How many characters to change until the end of the
		 * current line
		 */
	    columnsleft = NumberColumns - ScreenLineOffset(pointer);
		/*
		 * Make sure we are where we think we are.
		 */
	    move(ScreenLine(pointer), ScreenLineOffset(pointer));

		/* what is the field attribute of the current position */
	    if (FormattedScreen()) {
		fieldattr = FieldAttributes(pointer);
		DoAttributes(fieldattr);
	    } else {
		fieldattr = NORMAL;
	    }
	    if (TerminalFormattedScreen()) {
		termattr = TermAttributes(pointer);
		DoAttributes(termattr);
	    } else {
		termattr = NORMAL;
	    }

	    SetHighlightMode(fieldattr);
	    /*
	     * The following will terminate at least when we get back
	     * to the original 'pointer' location (since we force
	     * things to be equal).
	     */
	    for (;;) {
		if (IsStartField(pointer)) {
		    shouldbe = DISP_BLANK;
		    shouldattr = 0;
		    fieldattr = GetHost(pointer);
		    DoAttributes(fieldattr);
		} else {
		    if (fieldattr&NONDISPLAY) {
			shouldbe = DISP_BLANK;
		    } else {
			shouldbe = GetHost(pointer);
		    }
		    shouldattr = fieldattr;
		}
		if (TermIsStartField(pointer)) {
		    is = DISP_BLANK;
		    isattr = 0;
		    termattr = UNDETERMINED; /* Need to find out AFTER update */
		} else {
		    if (termattr&NONDISPLAY) {
			is = DISP_BLANK;
		    } else {
			is = GetTerminal(pointer);
		    }
		    isattr = termattr;
		}
		if ((shouldbe == is) && (shouldattr == isattr)
			&& (GetHost(pointer) == GetTerminal(pointer))
			&& (GetHost(ScreenInc(pointer))
					== GetTerminal(ScreenInc(pointer)))) {
		    break;
		}

		if (shouldattr^inHighlightMode) {
		    SetHighlightMode(shouldattr);
		}

		DoCharacterAt(shouldbe, pointer);
		if (IsStartField(pointer)) {
		    TermNewField(pointer, FieldAttributes(pointer));
		    termattr = GetTerminal(pointer);
		    DoAttributes(termattr);
		} else {
		    SetTerminal(pointer, GetHost(pointer));
		    /*
		     * If this USED to be a start field location,
		     * recompute the terminal attributes.
		     */
		    if (termattr == UNDETERMINED) {
			termattr = WhereTermAttrByte(pointer);
			if ((termattr != 0) || TermIsStartField(0)) {
			    termattr = GetTerminal(termattr);
			    DoAttributes(termattr);
			} else {	/* Unformatted screen */
			    termattr = NORMAL;
			}
		    }
		}
		pointer = ScreenInc(pointer);
		if (!(--columnsleft)) {
		    DoARefresh();
		    EmptyTerminal();
		    if (HaveInput) {	/* if input came in, take it */
			int c, j;

			/*
			 * We need to start a new terminal field
			 * at this location iff the terminal attributes
			 * of this location are not what we have had
			 * them as (ie: we've overwritten the terminal
			 * start field, a the previous field had different
			 * display characteristics).
			 */

			isattr = TermAttributes(pointer);
			DoAttributes(isattr);
			if ((!TermIsStartField(pointer)) &&
					(isattr != termattr)) {
			    /*
			     * Since we are going to leave a new field
			     * at this terminal position, we
			     * need to make sure that we get an actual
			     * non-highlighted blank on the screen.
			     */
			    if ((is != DISP_BLANK) || (termattr&HIGHLIGHT)) {
				SetHighlightMode(0);	/* Turn off highlight */
				c = ScreenInc(pointer);
				j = DISP_BLANK;
				DoCharacterAt(j, c);
			    }
			    if (termattr&HIGHLIGHT) {
				termattr = ATTR_DSPD_HIGH;
			    } else if (termattr&NONDISPLAY) {
				termattr = ATTR_DSPD_NONDISPLAY;
			    } else {
				termattr = 0;
			    }
			    TermNewField(pointer, termattr);
			}
			break;
		    }
		    move(ScreenLine(pointer), 0);
		    columnsleft = NumberColumns;
		}
	    }	/* end of for (;;) */
	} /* end of while (...) */
    }
    DoARefresh();
    Lowest = pointer;
    if (Lowest > Highest) {		/* if we finished input... */
	Lowest = HighestScreen()+1;
	Highest = LowestScreen()-1;
	terminalCursorAddress = CorrectTerminalCursor();
	if (ERR == move(ScreenLine(terminalCursorAddress),
			ScreenLineOffset(terminalCursorAddress))) {
	    GoAway("move", terminalCursorAddress);
	}
	DoARefresh();
	if (needToRing) {
	    StringToTerminal(bellSequence);
	    needToRing = 0;
	}
    }
    EmptyTerminal();			/* move data along */
    return;
}

#if	defined(NOT43)
static int
#else	/* defined(NOT43) */
static void
#endif	/* defined(NOT43) */
FastScreen()
{
#if	defined(MSDOS)
#define	SaveCorner	0
#else	/* defined(MSDOS) */
#define	SaveCorner	1
#endif	/* defined(MSDOS) */

#define	DoAttribute(a) 	    if (IsHighlightedAttr(a)) { \
				standout(); \
			    } else { \
				standend(); \
			    } \
			    if (IsNonDisplayAttr(a)) { \
				a = 0; 	/* zero == don't display */ \
			    } \
			    if (!FormattedScreen()) { \
				a = 1;	/* one ==> do display on unformatted */\
			    }
    ScreenImage *p, *upper;
    int fieldattr;		/* spends most of its time == 0 or 1 */

/* OK.  We want to do this a quickly as possible.  So, we assume we
 * only need to go from Lowest to Highest.  However, if we find a
 * field in the middle, we do the whole screen.
 *
 * In particular, we separate out the two cases from the beginning.
 */
    if ((Highest != HighestScreen()) || (Lowest != LowestScreen())) {
	register int columnsleft;

	move(ScreenLine(Lowest), ScreenLineOffset(Lowest));
	p = &Host[Lowest];
#if	!defined(MSDOS)
	if (Highest == HighestScreen()) {
	    Highest = ScreenDec(Highest);
	}
#endif	/* !defined(MSDOS) */
	upper = &Host[Highest];
	fieldattr = FieldAttributes(Lowest);
	DoAttribute(fieldattr);	/* Set standout, non-display status */
	columnsleft = NumberColumns-ScreenLineOffset(p-Host);

	while (p <= upper) {
	    if (IsStartFieldPointer(p)) {	/* New field? */
		Highest = HighestScreen();
		Lowest = LowestScreen();
		FastScreen();		/* Recurse */
		return;
	    } else if (fieldattr) {	/* Should we display? */
			    /* Display translated data */
		addch((char)disp_asc[GetTerminalPointer(p)]);
	    } else {
		addch(' ');			/* Display a blank */
	    }
			/* If the physical screen is larger than what we
			 * are using, we need to make sure that each line
			 * starts at the beginning of the line.  Otherwise,
			 * we will just string all the lines together.
			 */
	    p++;
	    if (--columnsleft == 0) {
		int i = p-Host;

		move(ScreenLine(i), 0);
		columnsleft = NumberColumns;
	    }
	}
    } else {		/* Going from Lowest to Highest */
	unsigned char tmpbuf[MAXNUMBERCOLUMNS+1];
	ScreenImage *End = &Host[ScreenSize]-1-SaveCorner;
	register unsigned char *tmp = tmpbuf, *tmpend = tmpbuf+NumberColumns;

	*tmpend = 0;		/* terminate from the beginning */
	move(0,0);
	p = Host;
	fieldattr = FieldAttributes(LowestScreen());
	DoAttribute(fieldattr);	/* Set standout, non-display status */

	while (p <= End) {
	    if (IsStartFieldPointer(p)) {	/* New field? */
		if (tmp != tmpbuf) {
		    *tmp++ = 0;			/* close out */
		    addstr((char *)tmpbuf);
		    tmp = tmpbuf;
		    tmpend = tmpbuf+NumberColumns-ScreenLineOffset(p-Host)-1;
		}
		standend();
		addch(' ');
		fieldattr = FieldAttributesPointer(p);	/* Get attributes */
		DoAttribute(fieldattr);	/* Set standout, non-display */
	    } else {
		if (fieldattr) {	/* Should we display? */
				/* Display translated data */
		    *tmp++ = disp_asc[GetTerminalPointer(p)];
		} else {
		    *tmp++ = ' ';
		}
	    }
			/* If the physical screen is larger than what we
			 * are using, we need to make sure that each line
			 * starts at the beginning of the line.  Otherwise,
			 * we will just string all the lines together.
			 */
	    p++;
	    if (tmp == tmpend) {
		int i = p-Host;		/* Be sure the "p++" happened first! */

		*tmp++ = 0;
		addstr((char *)tmpbuf);
		tmp = tmpbuf;
		move(ScreenLine(i), 0);
		tmpend = tmpbuf + NumberColumns;
	    }
	}
	if (tmp != tmpbuf) {
	    *tmp++ = 0;
	    addstr((char *)tmpbuf);
	    tmp = tmpbuf;
	}
    }
    Lowest = HighestScreen()+1;
    Highest = LowestScreen()-1;
    terminalCursorAddress = CorrectTerminalCursor();
    if (ERR == move(ScreenLine(terminalCursorAddress),
		    ScreenLineOffset(terminalCursorAddress))) {
	GoAway("move", terminalCursorAddress);
    }
    DoARefresh();
    if (needToRing) {
	StringToTerminal(bellSequence);
	needToRing = 0;
    }
    EmptyTerminal();			/* move data along */
    return;
}


/* TryToSend - send data out to user's terminal */

#if	defined(NOT43)
int
#else	/* defined(NOT43) */
void
#endif	/* defined(NOT43) */
	(*TryToSend)() = FastScreen;

/*ARGSUSED*/
void
ScreenOIA(oia)
OIA *oia;
{
}


/* InitTerminal - called to initialize the screen, etc. */

void
InitTerminal()
{
#if defined(unix)
    struct sgttyb ourttyb;
    static int speeds[] = { 0, 50, 75, 110, 134, 150, 200, 300, 600, 1200, 1800,
		2400, 4800, 9600 };
#endif
    extern void InitMapping();
    
    InitMapping();		/* Go do mapping file (MAP3270) first */
    if (!screenInitd) { 	/* not initialized */
#if	defined(unix)
	char KSEbuffer[2050];
	char *lotsofspace = KSEbuffer;
	extern int abort();
	extern char *tgetstr();
#endif	/* defined(unix) */

	if (initscr() == ERR) {	/* Initialize curses to get line size */
	    ExitString("InitTerminal:  Error initializing curses", 1);
	    /*NOTREACHED*/
	}
	MaxNumberLines = LINES;
	MaxNumberColumns = COLS;
	ClearArray(Terminal);
	terminalCursorAddress = SetBufferAddress(0,0);
#if defined(unix)
	signal(SIGHUP, abort);
#endif

	TryToSend = FastScreen;
#if defined(unix)
	ioctl(1, TIOCGETP, (char *) &ourttyb);
	if ((ourttyb.sg_ospeed < 0) || (ourttyb.sg_ospeed > B9600)) {
	    max_changes_before_poll = 1920;
	} else {
	    max_changes_before_poll = speeds[ourttyb.sg_ospeed]/10;
	    if (max_changes_before_poll < 40) {
		max_changes_before_poll = 40;
	    }
	    TryToSend = SlowScreen;
	    HaveInput = 1;		/* get signals going */
	}
#endif	/* defined(unix) */
	setcommandmode();
	/*
	 * By now, initscr() (in curses) has been called (from telnet.c),
	 * and the screen has been initialized.
	 */
#if defined(unix)
	nonl();
			/* the problem is that curses catches SIGTSTP to
			 * be nice, but it messes us up.
			 */
	signal(SIGTSTP, SIG_DFL);
	if ((myKS = tgetstr("ks", &lotsofspace)) != 0) {
	    myKS = strsave(myKS);
	    StringToTerminal(myKS);
	}
	if ((myKE = tgetstr("ke", &lotsofspace)) != 0) {
	    myKE = strsave(myKE);
	}
	if (tgetstr("md", &lotsofspace) && tgetstr("me", &lotsofspace)) {
	   SO = strsave(tgetstr("md", &lotsofspace));
	   SE = strsave(tgetstr("me", &lotsofspace));
	}
#endif
	DoARefresh();
	setconnmode();
	if (VB && *VB) {
	    bellSequence = VB;		/* use visual bell */
	}
	screenInitd = 1;
	screenStopped = 0;		/* Not stopped */
    }
}


/* StopScreen - called when we are going away... */

void
StopScreen(doNewLine)
int doNewLine;
{
    if (screenInitd && !screenStopped) {
	move(NumberLines-1, 1);
	standend();
	inHighlightMode = 0;
	DoARefresh();
	setcommandmode();
	endwin();
	setconnmode();
#if	defined(unix)
	if (myKE) {
	    StringToTerminal(myKE);
	}
#endif	/* defined(unix) */
	if (doNewLine) {
	    StringToTerminal("\r\n");
	}
	EmptyTerminal();
	screenStopped = 1;		/* This is stopped */
    }
}


/* RefreshScreen - called to cause the screen to be refreshed */

void
RefreshScreen()
{
    clearok(curscr, TRUE);
    (*TryToSend)();
}


/* ConnectScreen - called to reconnect to the screen */

void
ConnectScreen()
{
    if (screenInitd) {
#if	defined(unix)
	if (myKS) {
	    StringToTerminal(myKS);
	}
#endif	/* defined(unix) */
	RefreshScreen();
	(*TryToSend)();
	screenStopped = 0;
    }
}

/* LocalClearScreen() - clear the whole ball of wax, cheaply */

void
LocalClearScreen()
{
    extern void Clear3270();

    outputPurge();		/* flush all data to terminal */
    clear();			/* clear in curses */
    ClearArray(Terminal);
    Clear3270();
    Lowest = HighestScreen()+1; /* everything in sync... */
    Highest = LowestScreen()+1;
}


void
BellOff()
{
    if (bellwinup) {
	delwin(bellwin);
	bellwin = 0;
	bellwinup = 0;
	touchwin(stdscr);
	DoARefresh();
    }
}


void
RingBell(s)
char *s;
{
    needToRing = 1;
    if (s) {
	int len = strlen(s);

	if (len > COLS-2) {
	    len = COLS-2;
	}
	if ((bellwin = newwin(3, len+2, LINES/2, 0)) == NULL) {
	    OurExitString("Error from newwin in RingBell", 1);
	}
	werase(bellwin);
	wstandout(bellwin);
	box(bellwin, '|', '-');
	if (wmove(bellwin, 1, 1) == ERR) {
	    OurExitString("Error from wmove in RingBell", 1);
	}
	while (len--) {
	    if (waddch(bellwin, *s++) == ERR) {
		OurExitString("Error from waddch in RingBell", 1);
	    }
	}
	wstandend(bellwin);
	if (wrefresh(bellwin) == ERR) {
	    OurExitString("Error from wrefresh in RingBell", 1);
	}
	bellwinup = 1;
    }
}


/* returns a 1 if no more output available (so, go ahead and block),
    or a 0 if there is more output available (so, just poll the other
    sources/destinations, don't block).
 */

int
DoTerminalOutput()
{
	/* called just before a select to conserve IO to terminal */
    if (!(screenInitd||screenStopped)) {
	return 1;		/* No output if not initialized */
    }
    if ((Lowest <= Highest) || needToRing ||
			(terminalCursorAddress != CorrectTerminalCursor())) {
	(*TryToSend)();
    }
    if (Lowest > Highest) {
	return 1;		/* no more output now */
    } else {
	return 0;		/* more output for future */
    }
}

/*
 * The following are defined to handle transparent data.
 */

void
TransStop()
{
#if	defined(unix)
    if (tcflag == 0) {
       tcflag = -1;
       (void) signal(SIGCHLD, SIG_DFL);
    } else if (tcflag > 0) {
       setcommandmode();
       (void) close(tin);
       (void) close(tout);
       tin = savefd[0];
       tout = savefd[1];
       setconnmode();
       tcflag = -1;
       (void) signal(SIGCHLD, SIG_DFL);
    }
#endif	/* defined(unix) */
    RefreshScreen();
}

void
TransOut(buffer, count, kind, control)
unsigned char	*buffer;
int		count;
int		kind;		/* 0 or 5 */
int		control;	/* To see if we are done */
{
#if	defined(unix)
    extern char *transcom;
    int inpipefd[2], outpipefd[2];
    void aborttc();
#endif	/* defined(unix) */

    while (DoTerminalOutput() == 0) {
#if defined(unix)
	HaveInput = 0;
#endif /* defined(unix) */
    }
#if	defined(unix)
    if (transcom && tcflag == -1) {
       while (1) {			  /* go thru once */
	     if (pipe(outpipefd) < 0) {
		break;
	     }
	     if (pipe(inpipefd) < 0) {
		break;
	     }
	     if ((tcflag = fork()) == 0) {
		(void) close(outpipefd[1]);
		(void) close(0);
		if (dup(outpipefd[0]) < 0) {
		   exit(1);
		}
		(void) close(outpipefd[0]);
		(void) close(inpipefd[0]);
		(void) close(1);
		if (dup(inpipefd[1]) < 0) {
		   exit(1);
		}
		(void) close(inpipefd[1]);
		if (execl("/bin/csh", "csh", "-c", transcom, (char *) 0)) {
		    exit(1);
		}
	     }
	     (void) close(inpipefd[1]);
	     (void) close(outpipefd[0]);
	     savefd[0] = tin;
	     savefd[1] = tout;
	     setcommandmode();
	     tin = inpipefd[0];
	     tout = outpipefd[1];
	     (void) signal(SIGCHLD, (int (*)())aborttc);
	     setconnmode();
	     tcflag = 1;
	     break;
       }
       if (tcflag < 1) {
	  tcflag = 0;
       }
    }
#endif	/* defined(unix) */
    (void) DataToTerminal((char *)buffer, count);
    if (control && (kind == 0)) {		/* Send in AID byte */
	SendToIBM();
    } else {
	extern void TransInput();

	TransInput(1, kind);			/* Go get some data */
    }
}


#if	defined(unix)
static void
aborttc()
{
	setcommandmode();
	(void) close(tin);
	(void) close(tout);
	tin = savefd[0];
	tout = savefd[1];
	setconnmode();
	tcflag = 0;
}
#endif	/* defined(unix) */
