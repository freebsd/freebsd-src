/****************************************************************************
 * Copyright (c) 1998,1999,2000 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

/*
 * This module is intended to encapsulate ncurses's interface to pointing
 * devices.
 *
 * The first method used is xterm's internal mouse-tracking facility.
 * The second is Alessandro Rubini's GPM server.
 *
 * Notes for implementors of new mouse-interface methods:
 *
 * The code is logically split into a lower level that accepts event reports
 * in a device-dependent format and an upper level that parses mouse gestures
 * and filters events.  The mediating data structure is a circular queue of
 * MEVENT structures.
 *
 * Functionally, the lower level's job is to pick up primitive events and
 * put them on the circular queue.  This can happen in one of two ways:
 * either (a) _nc_mouse_event() detects a series of incoming mouse reports
 * and queues them, or (b) code in lib_getch.c detects the kmous prefix in
 * the keyboard input stream and calls _nc_mouse_inline to queue up a series
 * of adjacent mouse reports.
 *
 * In either case, _nc_mouse_parse() should be called after the series is
 * accepted to parse the digested mouse reports (low-level MEVENTs) into
 * a gesture (a high-level or composite MEVENT).
 *
 * Don't be too shy about adding new event types or modifiers, if you can find
 * room for them in the 32-bit mask.  The API is written so that users get
 * feedback on which theoretical event types they won't see when they call
 * mousemask. There's one bit per button (the RESERVED_EVENT bit) not being
 * used yet, and a couple of bits open at the high end.
 */

#ifdef __EMX__
#  include <io.h>
#  define  INCL_DOS
#  define  INCL_VIO
#  define  INCL_KBD
#  define  INCL_MOU
#  define  INCL_DOSPROCESS
#  include <os2.h>		/* Need to include before the others */
#endif

#include <curses.priv.h>
#include <term.h>

#if USE_GPM_SUPPORT
#ifndef LINT			/* don't need this for llib-lncurses */
#undef buttons			/* term.h defines this, and gpm uses it! */
#include <gpm.h>
#include <linux/keyboard.h>	/* defines KG_* macros */
#endif
#endif

MODULE_ID("$Id: lib_mouse.c,v 1.55 2000/10/10 00:07:28 Ilya.Zakharevich Exp $")

#define MY_TRACE TRACE_ICALLS|TRACE_IEVENT

#define INVALID_EVENT	-1

static int mousetype;
#define M_XTERM		-1	/* use xterm's mouse tracking? */
#define M_NONE		0	/* no mouse device */
#define M_GPM		1	/* use GPM */
#define M_QNX		2	/* QNX mouse on console */
#define M_QNX_TERM	3	/* QNX mouse on pterm/xterm (using qansi-m) */

#if USE_GPM_SUPPORT
#ifndef LINT
static Gpm_Connect gpm_connect;
#endif
#endif

static mmask_t eventmask;	/* current event mask */

static bool _nc_mouse_parse(int);
static void _nc_mouse_resume(SCREEN *);
static void _nc_mouse_wrap(SCREEN *);

/* maintain a circular list of mouse events */

/* The definition of the circular list size (EV_MAX), is in curses.priv.h, so
 * wgetch() may refer to the size and call _nc_mouse_parse() before circular
 * list overflow.
 */
static MEVENT events[EV_MAX];	/* hold the last mouse event seen */
static MEVENT *eventp = events;	/* next free slot in event queue */
#define NEXT(ep)	((ep == events + EV_MAX - 1) ? events : ep + 1)
#define PREV(ep)	((ep == events) ? events + EV_MAX - 1 : ep - 1)

#ifdef TRACE
static void
_trace_slot(const char *tag)
{
    MEVENT *ep;

    _tracef(tag);

    for (ep = events; ep < events + EV_MAX; ep++)
	_tracef("mouse event queue slot %ld = %s",
		(long) (ep - events),
		_tracemouse(ep));
}
#endif

#ifdef USE_EMX_MOUSE

#  define TOP_ROW          0
#  define LEFT_COL         0

static int mouse_wfd;
static int mouse_thread;
static int mouse_activated;
static char mouse_buttons[] =
{0, 1, 3, 2};

#  define M_FD(sp) sp->_mouse_fd

static void
write_event(int down, int button, int x, int y)
{
    char buf[6];
    unsigned long ignore;

    strncpy(buf, key_mouse, 3);	/* should be "\033[M" */
    buf[3] = ' ' + (button - 1) + (down ? 0 : 0x40);
    buf[4] = ' ' + x - LEFT_COL + 1;
    buf[5] = ' ' + y - TOP_ROW + 1;
    DosWrite(mouse_wfd, buf, 6, &ignore);
}

static void
mouse_server(unsigned long ignored GCC_UNUSED)
{
    unsigned short fWait = MOU_WAIT;
    /* NOPTRRECT mourt = { 0,0,24,79 }; */
    MOUEVENTINFO mouev;
    HMOU hmou;
    unsigned short mask = MOUSE_BN1_DOWN | MOUSE_BN2_DOWN | MOUSE_BN3_DOWN;
    int nbuttons = 3;
    int oldstate = 0;
    char err[80];
    unsigned long rc;

    /* open the handle for the mouse */
    if (MouOpen(NULL, &hmou) == 0) {
	rc = MouSetEventMask(&mask, hmou);
	if (rc) {		/* retry with 2 buttons */
	    mask = MOUSE_BN1_DOWN | MOUSE_BN2_DOWN;
	    rc = MouSetEventMask(&mask, hmou);
	    nbuttons = 2;
	}
	if (rc == 0 && MouDrawPtr(hmou) == 0) {
	    for (;;) {
		/* sit and wait on the event queue */
		rc = MouReadEventQue(&mouev, &fWait, hmou);
		if (rc) {
		    sprintf(err, "Error reading mouse queue, rc=%lu.\r\n", rc);
		    break;
		}
		if (!mouse_activated)
		    goto finish;

		/*
		 * OS/2 numbers a 3-button mouse inconsistently from other
		 * platforms:
		 *      1 = left
		 *      2 = right
		 *      3 = middle.
		 */
		if ((mouev.fs ^ oldstate) & MOUSE_BN1_DOWN)
		    write_event(mouev.fs & MOUSE_BN1_DOWN,
				mouse_buttons[1], mouev.col, mouev.row);
		if ((mouev.fs ^ oldstate) & MOUSE_BN2_DOWN)
		    write_event(mouev.fs & MOUSE_BN2_DOWN,
				mouse_buttons[3], mouev.col, mouev.row);
		if ((mouev.fs ^ oldstate) & MOUSE_BN3_DOWN)
		    write_event(mouev.fs & MOUSE_BN3_DOWN,
				mouse_buttons[2], mouev.col, mouev.row);

	      finish:
		oldstate = mouev.fs;
	    }
	} else
	    sprintf(err, "Error setting event mask, buttons=%d, rc=%lu.\r\n",
		    nbuttons, rc);

	DosWrite(2, err, strlen(err), &rc);
	MouClose(hmou);
    }
    DosExit(EXIT_THREAD, 0L);
}

static void
server_state(const int state)
{				/* It would be nice to implement pointer-off and stop looping... */
    mouse_activated = state;
}

#endif

static int initialized;

static void
initialize_mousetype(void)
{
    static const char *xterm_kmous = "\033[M";

    /* Try gpm first, because gpm may be configured to run in xterm */
#if USE_GPM_SUPPORT
    /* GPM: initialize connection to gpm server */
    gpm_connect.eventMask = GPM_DOWN | GPM_UP;
    gpm_connect.defaultMask = ~(gpm_connect.eventMask | GPM_HARD);
    gpm_connect.minMod = 0;
    gpm_connect.maxMod = ~((1 << KG_SHIFT) | (1 << KG_SHIFTL) | (1 << KG_SHIFTR));
    if (Gpm_Open(&gpm_connect, 0) >= 0) {	/* returns the file-descriptor */
	mousetype = M_GPM;
	SP->_mouse_fd = gpm_fd;
	return;
    }
#endif

    /* OS/2 VIO */
#ifdef USE_EMX_MOUSE
    if (!mouse_thread
	&& strstr(cur_term->type.term_names, "xterm") == 0
	&& key_mouse) {
	int handles[2];

	if (pipe(handles) < 0) {
	    perror("mouse pipe error");
	    return;
	} else {
	    int rc;

	    if (!mouse_buttons[0]) {
		char *s = getenv("MOUSE_BUTTONS_123");

		mouse_buttons[0] = 1;
		if (s && strlen(s) >= 3) {
		    mouse_buttons[1] = s[0] - '0';
		    mouse_buttons[2] = s[1] - '0';
		    mouse_buttons[3] = s[2] - '0';
		}
	    }
	    mouse_wfd = handles[1];
	    M_FD(SP) = handles[0];
	    /* Needed? */
	    setmode(handles[0], O_BINARY);
	    setmode(handles[1], O_BINARY);
	    /* Do not use CRT functions, we may single-threaded. */
	    rc = DosCreateThread((unsigned long *) &mouse_thread,
				 mouse_server, 0, 0, 8192);
	    if (rc) {
		printf("mouse thread error %d=%#x", rc, rc);
		return;
	    } else {
		mousetype = M_XTERM;
		return;
	    }
	}
    }
#endif

    /* we know how to recognize mouse events under "xterm" */
    if (key_mouse != 0) {
	if (!strcmp(key_mouse, xterm_kmous)) {
	    mousetype = M_XTERM;
	    return;
	}
    } else if (strstr(cur_term->type.term_names, "xterm") != 0) {
	(void) _nc_add_to_try(&(SP->_keytry), xterm_kmous, KEY_MOUSE);
	mousetype = M_XTERM;
	return;
    }
}

static void
_nc_mouse_init(void)
/* initialize the mouse */
{
    int i;

    if (!initialized) {
	initialized = TRUE;

	TR(MY_TRACE, ("_nc_mouse_init() called"));

	for (i = 0; i < EV_MAX; i++)
	    events[i].id = INVALID_EVENT;

	initialize_mousetype();

	T(("_nc_mouse_init() set mousetype to %d", mousetype));
    }
}

static bool
_nc_mouse_event(SCREEN * sp GCC_UNUSED)
/* query to see if there is a pending mouse event */
{
#if USE_GPM_SUPPORT
    /* GPM: query server for event, return TRUE if we find one */
    Gpm_Event ev;

    if (gpm_fd >= 0
	&& (_nc_timed_wait(3, 0, (int *) 0) & 2) != 0
	&& Gpm_GetEvent(&ev) == 1) {
	eventp->id = 0;		/* there's only one mouse... */

	eventp->bstate = 0;
	switch (ev.type & 0x0f) {
	case (GPM_DOWN):
	    if (ev.buttons & GPM_B_LEFT)
		eventp->bstate |= BUTTON1_PRESSED;
	    if (ev.buttons & GPM_B_MIDDLE)
		eventp->bstate |= BUTTON2_PRESSED;
	    if (ev.buttons & GPM_B_RIGHT)
		eventp->bstate |= BUTTON3_PRESSED;
	    break;
	case (GPM_UP):
	    if (ev.buttons & GPM_B_LEFT)
		eventp->bstate |= BUTTON1_RELEASED;
	    if (ev.buttons & GPM_B_MIDDLE)
		eventp->bstate |= BUTTON2_RELEASED;
	    if (ev.buttons & GPM_B_RIGHT)
		eventp->bstate |= BUTTON3_RELEASED;
	    break;
	default:
	    break;
	}

	eventp->x = ev.x - 1;
	eventp->y = ev.y - 1;
	eventp->z = 0;

	/* bump the next-free pointer into the circular list */
	eventp = NEXT(eventp);
	return (TRUE);
    }
#endif

#ifdef USE_EMX_MOUSE
    if (SP->_mouse_fd >= 0
	&& (_nc_timed_wait(3, 0, (int *) 0) & 2) != 0) {
	char kbuf[3];

	int i, res = read(M_FD(sp), &kbuf, 3);	/* Eat the prefix */
	if (res != 3)
	    printf("Got %d chars instead of 3 for prefix.\n", res);
	for (i = 0; i < res; i++) {
	    if (kbuf[i] != key_mouse[i])
		printf("Got char %d instead of %d for prefix.\n",
		       (int) kbuf[i], (int) key_mouse[i]);
	}
	return TRUE;
    }
#endif /* USE_EMX_MOUSE */

    /* xterm: never have to query, mouse events are in the keyboard stream */
    return (FALSE);		/* no event waiting */
}

static bool
_nc_mouse_inline(SCREEN * sp)
/* mouse report received in the keyboard stream -- parse its info */
{
    TR(MY_TRACE, ("_nc_mouse_inline() called"));

    if (mousetype == M_XTERM) {
	unsigned char kbuf[4];
	MEVENT *prev;
	size_t grabbed;
	int res;

	/* This code requires that your xterm entry contain the kmous
	 * capability and that it be set to the \E[M documented in the
	 * Xterm Control Sequences reference.  This is how we
	 * arrange for mouse events to be reported via a KEY_MOUSE
	 * return value from wgetch().  After this value is received,
	 * _nc_mouse_inline() gets called and is immediately
	 * responsible for parsing the mouse status information
	 * following the prefix.
	 *
	 * The following quotes from the ctrlseqs.ms document in the
	 * X distribution, describing the X mouse tracking feature:
	 *
	 * Parameters for all mouse tracking escape sequences
	 * generated by xterm encode numeric parameters in a single
	 * character as value+040.  For example, !  is 1.
	 *
	 * On button press or release, xterm sends ESC [ M CbCxCy.
	 * The low two bits of Cb encode button information: 0=MB1
	 * pressed, 1=MB2 pressed, 2=MB3 pressed, 3=release.  The
	 * upper bits encode what modifiers were down when the
	 * button was pressed and are added together.  4=Shift,
	 * 8=Meta, 16=Control.  Cx and Cy are the x and y coordinates
	 * of the mouse event.  The upper left corner is (1,1).
	 *
	 * (End quote)  By the time we get here, we've eaten the
	 * key prefix.  FYI, the loop below is necessary because
	 * mouse click info isn't guaranteed to present as a
	 * single clist item.  It always does under Linux but often
	 * fails to under Solaris.
	 */
	for (grabbed = 0; grabbed < 3; grabbed += res) {

	    /* For VIO mouse we add extra bit 64 to disambiguate button-up. */
#ifdef USE_EMX_MOUSE
	    res = read(M_FD(sp) >= 0 ? M_FD(sp) : sp->_ifd, &kbuf, 3);
#else
	    res = read(sp->_ifd, kbuf + grabbed, 3 - grabbed);
#endif
	    if (res == -1)
		break;
	}
	kbuf[3] = '\0';

	TR(TRACE_IEVENT,
	   ("_nc_mouse_inline sees the following xterm data: '%s'", kbuf));

	eventp->id = 0;		/* there's only one mouse... */

	/* processing code goes here */
	eventp->bstate = 0;
	switch (kbuf[0] & 0x3) {
	case 0x0:
	    eventp->bstate = BUTTON1_PRESSED;
#ifdef USE_EMX_MOUSE
	    if (kbuf[0] & 0x40)
		eventp->bstate = BUTTON1_RELEASED;
#endif
	    break;

	case 0x1:
	    eventp->bstate = BUTTON2_PRESSED;
#ifdef USE_EMX_MOUSE
	    if (kbuf[0] & 0x40)
		eventp->bstate = BUTTON2_RELEASED;
#endif
	    break;

	case 0x2:
	    eventp->bstate = BUTTON3_PRESSED;
#ifdef USE_EMX_MOUSE
	    if (kbuf[0] & 0x40)
		eventp->bstate = BUTTON3_RELEASED;
#endif
	    break;

	case 0x3:
	    /*
	     * Release events aren't reported for individual buttons,
	     * just for the button set as a whole...
	     */
	    eventp->bstate =
		(BUTTON1_RELEASED |
		 BUTTON2_RELEASED |
		 BUTTON3_RELEASED);
	    /*
	     * ...however, because there are no kinds of mouse events under
	     * xterm that can intervene between press and release, we can
	     * deduce which buttons were actually released by looking at the
	     * previous event.
	     */
	    prev = PREV(eventp);
	    if (!(prev->bstate & BUTTON1_PRESSED))
		eventp->bstate &= ~BUTTON1_RELEASED;
	    if (!(prev->bstate & BUTTON2_PRESSED))
		eventp->bstate &= ~BUTTON2_RELEASED;
	    if (!(prev->bstate & BUTTON3_PRESSED))
		eventp->bstate &= ~BUTTON3_RELEASED;
	    break;
	}

	if (kbuf[0] & 4) {
	    eventp->bstate |= BUTTON_SHIFT;
	}
	if (kbuf[0] & 8) {
	    eventp->bstate |= BUTTON_ALT;
	}
	if (kbuf[0] & 16) {
	    eventp->bstate |= BUTTON_CTRL;
	}

	eventp->x = (kbuf[1] - ' ') - 1;
	eventp->y = (kbuf[2] - ' ') - 1;
	TR(MY_TRACE,
	   ("_nc_mouse_inline: primitive mouse-event %s has slot %ld",
	    _tracemouse(eventp),
	    (long) (eventp - events)));

	/* bump the next-free pointer into the circular list */
	eventp = NEXT(eventp);
#if 0				/* this return would be needed for QNX's mods to lib_getch.c */
	return (TRUE);
#endif
    }

    return (FALSE);
}

static void
mouse_activate(bool on)
{
    if (!on && !initialized)
	return;

    _nc_mouse_init();

    if (on) {

	switch (mousetype) {
	case M_XTERM:
#if NCURSES_EXT_FUNCS
	    keyok(KEY_MOUSE, on);
#endif
	    TPUTS_TRACE("xterm mouse initialization");
#ifdef USE_EMX_MOUSE
	    server_state(1);
#else
	    putp("\033[?1000h");
#endif
	    break;
#if USE_GPM_SUPPORT
	case M_GPM:
	    SP->_mouse_fd = gpm_fd;
	    break;
#endif
	}
	/* Make runtime binding to cut down on object size of applications that
	 * do not use the mouse (e.g., 'clear').
	 */
	SP->_mouse_event = _nc_mouse_event;
	SP->_mouse_inline = _nc_mouse_inline;
	SP->_mouse_parse = _nc_mouse_parse;
	SP->_mouse_resume = _nc_mouse_resume;
	SP->_mouse_wrap = _nc_mouse_wrap;

    } else {

	switch (mousetype) {
	case M_XTERM:
	    TPUTS_TRACE("xterm mouse deinitialization");
#ifdef USE_EMX_MOUSE
	    server_state(0);
#else
	    putp("\033[?1000l");
#endif
	    break;
#if USE_GPM_SUPPORT
	case M_GPM:
	    break;
#endif
	}
    }
    _nc_flush();
}

/**************************************************************************
 *
 * Device-independent code
 *
 **************************************************************************/

static bool
_nc_mouse_parse(int runcount)
/* parse a run of atomic mouse events into a gesture */
{
    MEVENT *ep, *runp, *next, *prev = PREV(eventp);
    int n;
    bool merge;

    TR(MY_TRACE, ("_nc_mouse_parse(%d) called", runcount));

    /*
     * When we enter this routine, the event list next-free pointer
     * points just past a run of mouse events that we know were separated
     * in time by less than the critical click interval. The job of this
     * routine is to collaps this run into a single higher-level event
     * or gesture.
     *
     * We accomplish this in two passes.  The first pass merges press/release
     * pairs into click events.  The second merges runs of click events into
     * double or triple-click events.
     *
     * It's possible that the run may not resolve to a single event (for
     * example, if the user quadruple-clicks).  If so, leading events
     * in the run are ignored.
     *
     * Note that this routine is independent of the format of the specific
     * format of the pointing-device's reports.  We can use it to parse
     * gestures on anything that reports press/release events on a per-
     * button basis, as long as the device-dependent mouse code puts stuff
     * on the queue in MEVENT format.
     */
    if (runcount == 1) {
	TR(MY_TRACE,
	   ("_nc_mouse_parse: returning simple mouse event %s at slot %ld",
	    _tracemouse(prev),
	    (long) (prev - events)));
	return (prev->id >= 0)
	    ? ((prev->bstate & eventmask) ? TRUE : FALSE)
	    : FALSE;
    }

    /* find the start of the run */
    runp = eventp;
    for (n = runcount; n > 0; n--) {
	runp = PREV(runp);
    }

#ifdef TRACE
    if (_nc_tracing & TRACE_IEVENT) {
	_trace_slot("before mouse press/release merge:");
	_tracef("_nc_mouse_parse: run starts at %ld, ends at %ld, count %d",
		(long) (runp - events),
		(long) ((eventp - events) + (EV_MAX - 1)) % EV_MAX,
		runcount);
    }
#endif /* TRACE */

    /* first pass; merge press/release pairs */
    do {
	merge = FALSE;
	for (ep = runp; next = NEXT(ep), next != eventp; ep = next) {
	    if (ep->x == next->x && ep->y == next->y
		&& (ep->bstate & (BUTTON1_PRESSED | BUTTON2_PRESSED | BUTTON3_PRESSED))
		&& (!(ep->bstate & BUTTON1_PRESSED)
		    == !(next->bstate & BUTTON1_RELEASED))
		&& (!(ep->bstate & BUTTON2_PRESSED)
		    == !(next->bstate & BUTTON2_RELEASED))
		&& (!(ep->bstate & BUTTON3_PRESSED)
		    == !(next->bstate & BUTTON3_RELEASED))
		) {
		if ((eventmask & BUTTON1_CLICKED)
		    && (ep->bstate & BUTTON1_PRESSED)) {
		    ep->bstate &= ~BUTTON1_PRESSED;
		    ep->bstate |= BUTTON1_CLICKED;
		    merge = TRUE;
		}
		if ((eventmask & BUTTON2_CLICKED)
		    && (ep->bstate & BUTTON2_PRESSED)) {
		    ep->bstate &= ~BUTTON2_PRESSED;
		    ep->bstate |= BUTTON2_CLICKED;
		    merge = TRUE;
		}
		if ((eventmask & BUTTON3_CLICKED)
		    && (ep->bstate & BUTTON3_PRESSED)) {
		    ep->bstate &= ~BUTTON3_PRESSED;
		    ep->bstate |= BUTTON3_CLICKED;
		    merge = TRUE;
		}
		if (merge)
		    next->id = INVALID_EVENT;
	    }
	}
    } while
	(merge);

#ifdef TRACE
    if (_nc_tracing & TRACE_IEVENT) {
	_trace_slot("before mouse click merge:");
	_tracef("_nc_mouse_parse: run starts at %ld, ends at %ld, count %d",
		(long) (runp - events),
		(long) ((eventp - events) + (EV_MAX - 1)) % EV_MAX,
		runcount);
    }
#endif /* TRACE */

    /*
     * Second pass; merge click runs.  At this point, click events are
     * each followed by one invalid event. We merge click events
     * forward in the queue.
     *
     * NOTE: There is a problem with this design!  If the application
     * allows enough click events to pile up in the circular queue so
     * they wrap around, it will cheerfully merge the newest forward
     * into the oldest, creating a bogus doubleclick and confusing
     * the queue-traversal logic rather badly.  Generally this won't
     * happen, because calling getmouse() marks old events invalid and
     * ineligible for merges.  The true solution to this problem would
     * be to timestamp each MEVENT and perform the obvious sanity check,
     * but the timer element would have to have sub-second resolution,
     * which would get us into portability trouble.
     */
    do {
	MEVENT *follower;

	merge = FALSE;
	for (ep = runp; next = NEXT(ep), next != eventp; ep = next)
	    if (ep->id != INVALID_EVENT) {
		if (next->id != INVALID_EVENT)
		    continue;
		follower = NEXT(next);
		if (follower->id == INVALID_EVENT)
		    continue;

		/* merge click events forward */
		if ((ep->bstate &
		     (BUTTON1_CLICKED | BUTTON2_CLICKED | BUTTON3_CLICKED))
		    && (follower->bstate &
			(BUTTON1_CLICKED | BUTTON2_CLICKED | BUTTON3_CLICKED))) {
		    if ((eventmask & BUTTON1_DOUBLE_CLICKED)
			&& (follower->bstate & BUTTON1_CLICKED)) {
			follower->bstate &= ~BUTTON1_CLICKED;
			follower->bstate |= BUTTON1_DOUBLE_CLICKED;
			merge = TRUE;
		    }
		    if ((eventmask & BUTTON2_DOUBLE_CLICKED)
			&& (follower->bstate & BUTTON2_CLICKED)) {
			follower->bstate &= ~BUTTON2_CLICKED;
			follower->bstate |= BUTTON2_DOUBLE_CLICKED;
			merge = TRUE;
		    }
		    if ((eventmask & BUTTON3_DOUBLE_CLICKED)
			&& (follower->bstate & BUTTON3_CLICKED)) {
			follower->bstate &= ~BUTTON3_CLICKED;
			follower->bstate |= BUTTON3_DOUBLE_CLICKED;
			merge = TRUE;
		    }
		    if (merge)
			ep->id = INVALID_EVENT;
		}

		/* merge double-click events forward */
		if ((ep->bstate &
		     (BUTTON1_DOUBLE_CLICKED
		      | BUTTON2_DOUBLE_CLICKED
		      | BUTTON3_DOUBLE_CLICKED))
		    && (follower->bstate &
			(BUTTON1_CLICKED | BUTTON2_CLICKED | BUTTON3_CLICKED))) {
		    if ((eventmask & BUTTON1_TRIPLE_CLICKED)
			&& (follower->bstate & BUTTON1_CLICKED)) {
			follower->bstate &= ~BUTTON1_CLICKED;
			follower->bstate |= BUTTON1_TRIPLE_CLICKED;
			merge = TRUE;
		    }
		    if ((eventmask & BUTTON2_TRIPLE_CLICKED)
			&& (follower->bstate & BUTTON2_CLICKED)) {
			follower->bstate &= ~BUTTON2_CLICKED;
			follower->bstate |= BUTTON2_TRIPLE_CLICKED;
			merge = TRUE;
		    }
		    if ((eventmask & BUTTON3_TRIPLE_CLICKED)
			&& (follower->bstate & BUTTON3_CLICKED)) {
			follower->bstate &= ~BUTTON3_CLICKED;
			follower->bstate |= BUTTON3_TRIPLE_CLICKED;
			merge = TRUE;
		    }
		    if (merge)
			ep->id = INVALID_EVENT;
		}
	    }
    } while
	(merge);

#ifdef TRACE
    if (_nc_tracing & TRACE_IEVENT) {
	_trace_slot("before mouse event queue compaction:");
	_tracef("_nc_mouse_parse: run starts at %ld, ends at %ld, count %d",
		(long) (runp - events),
		(long) ((eventp - events) + (EV_MAX - 1)) % EV_MAX,
		runcount);
    }
#endif /* TRACE */

    /*
     * Now try to throw away trailing events flagged invalid, or that
     * don't match the current event mask.
     */
    for (; runcount; prev = PREV(eventp), runcount--)
	if (prev->id == INVALID_EVENT || !(prev->bstate & eventmask)) {
	    eventp = prev;
	}
#ifdef TRACE
    if (_nc_tracing & TRACE_IEVENT) {
	_trace_slot("after mouse event queue compaction:");
	_tracef("_nc_mouse_parse: run starts at %ld, ends at %ld, count %d",
		(long) (runp - events),
		(long) ((eventp - events) + (EV_MAX - 1)) % EV_MAX,
		runcount);
    }
    for (ep = runp; ep != eventp; ep = NEXT(ep))
	if (ep->id != INVALID_EVENT)
	    TR(MY_TRACE,
	       ("_nc_mouse_parse: returning composite mouse event %s at slot %ld",
		_tracemouse(ep),
		(long) (ep - events)));
#endif /* TRACE */

    /* after all this, do we have a valid event? */
    return (PREV(eventp)->id != INVALID_EVENT);
}

static void
_nc_mouse_wrap(SCREEN * sp GCC_UNUSED)
/* release mouse -- called by endwin() before shellout/exit */
{
    TR(MY_TRACE, ("_nc_mouse_wrap() called"));

    switch (mousetype) {
    case M_XTERM:
	if (eventmask)
	    mouse_activate(FALSE);
	break;
#if USE_GPM_SUPPORT
	/* GPM: pass all mouse events to next client */
    case M_GPM:
	break;
#endif
    }
}

static void
_nc_mouse_resume(SCREEN * sp GCC_UNUSED)
/* re-connect to mouse -- called by doupdate() after shellout */
{
    TR(MY_TRACE, ("_nc_mouse_resume() called"));

    /* xterm: re-enable reporting */
    if (mousetype == M_XTERM && eventmask)
	mouse_activate(TRUE);

    /* GPM: reclaim our event set */
}

/**************************************************************************
 *
 * Mouse interface entry points for the API
 *
 **************************************************************************/

int
getmouse(MEVENT * aevent)
/* grab a copy of the current mouse event */
{
    T((T_CALLED("getmouse(%p)"), aevent));

    if (aevent && (mousetype != M_NONE)) {
	/* compute the current-event pointer */
	MEVENT *prev = PREV(eventp);

	/* copy the event we find there */
	*aevent = *prev;

	TR(TRACE_IEVENT, ("getmouse: returning event %s from slot %ld",
			  _tracemouse(prev),
			  (long) (prev - events)));

	prev->id = INVALID_EVENT;	/* so the queue slot becomes free */
	returnCode(OK);
    }
    returnCode(ERR);
}

int
ungetmouse(MEVENT * aevent)
/* enqueue a synthesized mouse event to be seen by the next wgetch() */
{
    /* stick the given event in the next-free slot */
    *eventp = *aevent;

    /* bump the next-free pointer into the circular list */
    eventp = NEXT(eventp);

    /* push back the notification event on the keyboard queue */
    return ungetch(KEY_MOUSE);
}

mmask_t
mousemask(mmask_t newmask, mmask_t * oldmask)
/* set the mouse event mask */
{
    mmask_t result = 0;

    T((T_CALLED("mousemask(%#lx,%p)"), newmask, oldmask));

    if (oldmask)
	*oldmask = eventmask;

    if (!newmask && !initialized)
	returnCode(0);

    _nc_mouse_init();
    if (mousetype != M_NONE) {
	eventmask = newmask &
	    (BUTTON_ALT | BUTTON_CTRL | BUTTON_SHIFT
	     | BUTTON1_PRESSED | BUTTON1_RELEASED | BUTTON1_CLICKED
	     | BUTTON1_DOUBLE_CLICKED | BUTTON1_TRIPLE_CLICKED
	     | BUTTON2_PRESSED | BUTTON2_RELEASED | BUTTON2_CLICKED
	     | BUTTON2_DOUBLE_CLICKED | BUTTON2_TRIPLE_CLICKED
	     | BUTTON3_PRESSED | BUTTON3_RELEASED | BUTTON3_CLICKED
	     | BUTTON3_DOUBLE_CLICKED | BUTTON3_TRIPLE_CLICKED);

	mouse_activate(eventmask != 0);

	result = eventmask;
    }

    returnCode(result);
}

bool
wenclose(const WINDOW *win, int y, int x)
/* check to see if given window encloses given screen location */
{
    if (win) {
	y -= win->_yoffset;
	return ((win->_begy <= y &&
		 win->_begx <= x &&
		 (win->_begx + win->_maxx) >= x &&
		 (win->_begy + win->_maxy) >= y) ? TRUE : FALSE);
    }
    return FALSE;
}

int
mouseinterval(int maxclick)
/* set the maximum mouse interval within which to recognize a click */
{
    int oldval;

    if (SP != 0) {
	oldval = SP->_maxclick;
	if (maxclick >= 0)
	    SP->_maxclick = maxclick;
    } else {
	oldval = DEFAULT_MAXCLICK;
    }

    return (oldval);
}

/* This may be used by other routines to ask for the existence of mouse
   support */
int
_nc_has_mouse(void)
{
    return (mousetype == M_NONE ? 0 : 1);
}

bool
wmouse_trafo(const WINDOW *win, int *pY, int *pX, bool to_screen)
{
    bool result = FALSE;

    if (win && pY && pX) {
	int y = *pY;
	int x = *pX;

	if (to_screen) {
	    y += win->_begy + win->_yoffset;
	    x += win->_begx;
	    if (wenclose(win, y, x))
		result = TRUE;
	} else {
	    if (wenclose(win, y, x)) {
		y -= (win->_begy + win->_yoffset);
		x -= win->_begx;
		result = TRUE;
	    }
	}
	if (result) {
	    *pX = x;
	    *pY = y;
	}
    }
    return (result);
}

/* lib_mouse.c ends here */
