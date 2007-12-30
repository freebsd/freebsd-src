/****************************************************************************
 * Copyright (c) 1998-2006,2007 Free Software Foundation, Inc.              *
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
 *     and: Thomas E. Dickey                        1996-on                 *
 ****************************************************************************/

/*
**	lib_getch.c
**
**	The routine getch().
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_getch.c,v 1.80 2007/09/29 20:39:34 tom Exp $")

#include <fifo_defs.h>

#if USE_REENTRANT
NCURSES_EXPORT(int)
NCURSES_PUBLIC_VAR(ESCDELAY) (void)
{
    return SP ? SP->_ESCDELAY : 1000;
}
#else
NCURSES_EXPORT_VAR(int)
ESCDELAY = 1000;		/* max interval betw. chars in funkeys, in millisecs */
#endif

#ifdef NCURSES_WGETCH_EVENTS
#define TWAIT_MASK 7
#else
#define TWAIT_MASK 3
#endif

/*
 * Check for mouse activity, returning nonzero if we find any.
 */
static int
check_mouse_activity(int delay EVENTLIST_2nd(_nc_eventlist * evl))
{
    int rc;

#if USE_SYSMOUSE
    if ((SP->_mouse_type == M_SYSMOUSE)
	&& (SP->_sysmouse_head < SP->_sysmouse_tail)) {
	return 2;
    }
#endif
    rc = _nc_timed_wait(TWAIT_MASK, delay, (int *) 0 EVENTLIST_2nd(evl));
#if USE_SYSMOUSE
    if ((SP->_mouse_type == M_SYSMOUSE)
	&& (SP->_sysmouse_head < SP->_sysmouse_tail)
	&& (rc == 0)
	&& (errno == EINTR)) {
	rc |= 2;
    }
#endif
    return rc;
}

static NCURSES_INLINE int
fifo_peek(void)
{
    int ch = SP->_fifo[peek];
    TR(TRACE_IEVENT, ("peeking at %d", peek));

    p_inc();
    return ch;
}

static NCURSES_INLINE int
fifo_pull(void)
{
    int ch;
    ch = SP->_fifo[head];
    TR(TRACE_IEVENT, ("pulling %s from %d", _tracechar(ch), head));

    if (peek == head) {
	h_inc();
	peek = head;
    } else
	h_inc();

#ifdef TRACE
    if (USE_TRACEF(TRACE_IEVENT)) {
	_nc_fifo_dump();
	_nc_unlock_global(tracef);
    }
#endif
    return ch;
}

static NCURSES_INLINE int
fifo_push(EVENTLIST_0th(_nc_eventlist * evl))
{
    int n;
    int ch = 0;
    int mask = 0;

    (void) mask;
    if (tail == -1)
	return ERR;

#ifdef HIDE_EINTR
  again:
    errno = 0;
#endif

#ifdef NCURSES_WGETCH_EVENTS
    if (evl
#if USE_GPM_SUPPORT || USE_EMX_MOUSE || USE_SYSMOUSE
	|| (SP->_mouse_fd >= 0)
#endif
	) {
	mask = check_mouse_activity(-1 EVENTLIST_2nd(evl));
    } else
	mask = 0;

    if (mask & 4) {
	T(("fifo_push: ungetch KEY_EVENT"));
	ungetch(KEY_EVENT);
	return KEY_EVENT;
    }
#elif USE_GPM_SUPPORT || USE_EMX_MOUSE || USE_SYSMOUSE
    if (SP->_mouse_fd >= 0) {
	mask = check_mouse_activity(-1 EVENTLIST_2nd(evl));
    }
#endif

#if USE_GPM_SUPPORT || USE_EMX_MOUSE
    if ((SP->_mouse_fd >= 0) && (mask & 2)) {
	SP->_mouse_event(SP);
	ch = KEY_MOUSE;
	n = 1;
    } else
#endif
#if USE_SYSMOUSE
	if ((SP->_mouse_type == M_SYSMOUSE)
	    && (SP->_sysmouse_head < SP->_sysmouse_tail)) {
	SP->_mouse_event(SP);
	ch = KEY_MOUSE;
	n = 1;
    } else if ((SP->_mouse_type == M_SYSMOUSE)
	       && (mask <= 0) && errno == EINTR) {
	SP->_mouse_event(SP);
	ch = KEY_MOUSE;
	n = 1;
    } else
#endif
    {				/* Can block... */
	unsigned char c2 = 0;
	n = read(SP->_ifd, &c2, 1);
	ch = c2;
    }

#ifdef HIDE_EINTR
    /*
     * Under System V curses with non-restarting signals, getch() returns
     * with value ERR when a handled signal keeps it from completing.
     * If signals restart system calls, OTOH, the signal is invisible
     * except to its handler.
     *
     * We don't want this difference to show.  This piece of code
     * tries to make it look like we always have restarting signals.
     */
    if (n <= 0 && errno == EINTR)
	goto again;
#endif

    if ((n == -1) || (n == 0)) {
	TR(TRACE_IEVENT, ("read(%d,&ch,1)=%d, errno=%d", SP->_ifd, n, errno));
	ch = ERR;
    }
    TR(TRACE_IEVENT, ("read %d characters", n));

    SP->_fifo[tail] = ch;
    SP->_fifohold = 0;
    if (head == -1)
	head = peek = tail;
    t_inc();
    TR(TRACE_IEVENT, ("pushed %s at %d", _tracechar(ch), tail));
#ifdef TRACE
    if (USE_TRACEF(TRACE_IEVENT)) {
	_nc_fifo_dump();
	_nc_unlock_global(tracef);
    }
#endif
    return ch;
}

static NCURSES_INLINE void
fifo_clear(void)
{
    memset(SP->_fifo, 0, sizeof(SP->_fifo));
    head = -1;
    tail = peek = 0;
}

static int kgetch(EVENTLIST_0th(_nc_eventlist * evl));

#define wgetch_should_refresh(win) (\
	(is_wintouched(win) || (win->_flags & _HASMOVED)) \
	&& !(win->_flags & _ISPAD))

NCURSES_EXPORT(int)
_nc_wgetch(WINDOW *win,
	   unsigned long *result,
	   int use_meta
	   EVENTLIST_2nd(_nc_eventlist * evl))
{
    int ch;
#ifdef NCURSES_WGETCH_EVENTS
    long event_delay = -1;
#endif

    T((T_CALLED("_nc_wgetch(%p)"), win));

    *result = 0;
    if (win == 0 || SP == 0) {
	returnCode(ERR);
    }

    if (cooked_key_in_fifo()) {
	if (wgetch_should_refresh(win))
	    wrefresh(win);

	*result = fifo_pull();
	returnCode(*result >= KEY_MIN ? KEY_CODE_YES : OK);
    }
#ifdef NCURSES_WGETCH_EVENTS
    if (evl && (evl->count == 0))
	evl = NULL;
    event_delay = _nc_eventlist_timeout(evl);
#endif

    /*
     * Handle cooked mode.  Grab a string from the screen,
     * stuff its contents in the FIFO queue, and pop off
     * the first character to return it.
     */
    if (head == -1 &&
	!SP->_notty &&
	!SP->_raw &&
	!SP->_cbreak &&
	!SP->_called_wgetch) {
	char buf[MAXCOLUMNS], *sp;
	int rc;

	TR(TRACE_IEVENT, ("filling queue in cooked mode"));

	SP->_called_wgetch = TRUE;
	rc = wgetnstr(win, buf, MAXCOLUMNS);
	SP->_called_wgetch = FALSE;

	/* ungetch in reverse order */
#ifdef NCURSES_WGETCH_EVENTS
	if (rc != KEY_EVENT)
#endif
	    ungetch('\n');
	for (sp = buf + strlen(buf); sp > buf; sp--)
	    ungetch(sp[-1]);

#ifdef NCURSES_WGETCH_EVENTS
	/* Return it first */
	if (rc == KEY_EVENT) {
	    *result = rc;
	} else
#endif
	    *result = fifo_pull();
	returnCode(*result >= KEY_MIN ? KEY_CODE_YES : OK);
    }

    if (win->_use_keypad != SP->_keypad_on)
	_nc_keypad(win->_use_keypad);

    if (wgetch_should_refresh(win))
	wrefresh(win);

    if (!win->_notimeout && (win->_delay >= 0 || SP->_cbreak > 1)) {
	if (head == -1) {	/* fifo is empty */
	    int delay;
	    int rc;

	    TR(TRACE_IEVENT, ("timed delay in wgetch()"));
	    if (SP->_cbreak > 1)
		delay = (SP->_cbreak - 1) * 100;
	    else
		delay = win->_delay;

#ifdef NCURSES_WGETCH_EVENTS
	    if (event_delay >= 0 && delay > event_delay)
		delay = event_delay;
#endif

	    TR(TRACE_IEVENT, ("delay is %d milliseconds", delay));

	    rc = check_mouse_activity(delay EVENTLIST_2nd(evl));

#ifdef NCURSES_WGETCH_EVENTS
	    if (rc & 4) {
		*result = KEY_EVENT;
		returnCode(KEY_CODE_YES);
	    }
#endif
	    if (!rc)
		returnCode(ERR);
	}
	/* else go on to read data available */
    }

    if (win->_use_keypad) {
	/*
	 * This is tricky.  We only want to get special-key
	 * events one at a time.  But we want to accumulate
	 * mouse events until either (a) the mouse logic tells
	 * us it's picked up a complete gesture, or (b)
	 * there's a detectable time lapse after one.
	 *
	 * Note: if the mouse code starts failing to compose
	 * press/release events into clicks, you should probably
	 * increase the wait with mouseinterval().
	 */
	int runcount = 0;
	int rc;

	do {
	    ch = kgetch(EVENTLIST_1st(evl));
	    if (ch == KEY_MOUSE) {
		++runcount;
		if (SP->_mouse_inline(SP))
		    break;
	    }
	    if (SP->_maxclick < 0)
		break;
	} while
	    (ch == KEY_MOUSE
	     && (((rc = check_mouse_activity(SP->_maxclick
					     EVENTLIST_2nd(evl))) != 0
		  && !(rc & 4))
		 || !SP->_mouse_parse(runcount)));
#ifdef NCURSES_WGETCH_EVENTS
	if ((rc & 4) && !ch == KEY_EVENT) {
	    ungetch(ch);
	    ch = KEY_EVENT;
	}
#endif
	if (runcount > 0 && ch != KEY_MOUSE) {
#ifdef NCURSES_WGETCH_EVENTS
	    /* mouse event sequence ended by an event, report event */
	    if (ch == KEY_EVENT) {
		ungetch(KEY_MOUSE);	/* FIXME This interrupts a gesture... */
	    } else
#endif
	    {
		/* mouse event sequence ended by keystroke, store keystroke */
		ungetch(ch);
		ch = KEY_MOUSE;
	    }
	}
    } else {
	if (head == -1)
	    fifo_push(EVENTLIST_1st(evl));
	ch = fifo_pull();
    }

    if (ch == ERR) {
#if USE_SIZECHANGE
	if (_nc_handle_sigwinch(FALSE)) {
	    _nc_update_screensize();
	    /* resizeterm can push KEY_RESIZE */
	    if (cooked_key_in_fifo()) {
		*result = fifo_pull();
		returnCode(*result >= KEY_MIN ? KEY_CODE_YES : OK);
	    }
	}
#endif
	returnCode(ERR);
    }

    /*
     * If echo() is in effect, display the printable version of the
     * key on the screen.  Carriage return and backspace are treated
     * specially by Solaris curses:
     *
     * If carriage return is defined as a function key in the
     * terminfo, e.g., kent, then Solaris may return either ^J (or ^M
     * if nonl() is set) or KEY_ENTER depending on the echo() mode. 
     * We echo before translating carriage return based on nonl(),
     * since the visual result simply moves the cursor to column 0.
     *
     * Backspace is a different matter.  Solaris curses does not
     * translate it to KEY_BACKSPACE if kbs=^H.  This does not depend
     * on the stty modes, but appears to be a hardcoded special case.
     * This is a difference from ncurses, which uses the terminfo entry.
     * However, we provide the same visual result as Solaris, moving the
     * cursor to the left.
     */
    if (SP->_echo && !(win->_flags & _ISPAD)) {
	chtype backup = (ch == KEY_BACKSPACE) ? '\b' : ch;
	if (backup < KEY_MIN)
	    wechochar(win, backup);
    }

    /*
     * Simulate ICRNL mode
     */
    if ((ch == '\r') && SP->_nl)
	ch = '\n';

    /* Strip 8th-bit if so desired.  We do this only for characters that
     * are in the range 128-255, to provide compatibility with terminals
     * that display only 7-bit characters.  Note that 'ch' may be a
     * function key at this point, so we mustn't strip _those_.
     */
    if (!use_meta)
	if ((ch < KEY_MIN) && (ch & 0x80))
	    ch &= 0x7f;

    T(("wgetch returning : %s", _tracechar(ch)));

    *result = ch;
    returnCode(ch >= KEY_MIN ? KEY_CODE_YES : OK);
}

#ifdef NCURSES_WGETCH_EVENTS
NCURSES_EXPORT(int)
wgetch_events(WINDOW *win, _nc_eventlist * evl)
{
    int code;
    unsigned long value;

    T((T_CALLED("wgetch_events(%p,%p)"), win, evl));
    code = _nc_wgetch(win,
		      &value,
		      SP->_use_meta
		      EVENTLIST_2nd(evl));
    if (code != ERR)
	code = value;
    returnCode(code);
}
#endif

NCURSES_EXPORT(int)
wgetch(WINDOW *win)
{
    int code;
    unsigned long value;

    T((T_CALLED("wgetch(%p)"), win));
    code = _nc_wgetch(win,
		      &value,
		      (SP ? SP->_use_meta : 0)
		      EVENTLIST_2nd((_nc_eventlist *) 0));
    if (code != ERR)
	code = value;
    returnCode(code);
}

/*
**      int
**      kgetch()
**
**      Get an input character, but take care of keypad sequences, returning
**      an appropriate code when one matches the input.  After each character
**      is received, set an alarm call based on ESCDELAY.  If no more of the
**      sequence is received by the time the alarm goes off, pass through
**      the sequence gotten so far.
**
**	This function must be called when there are no cooked keys in queue.
**	(that is head==-1 || peek==head)
**
*/

static int
kgetch(EVENTLIST_0th(_nc_eventlist * evl))
{
    TRIES *ptr;
    int ch = 0;
    int timeleft = ESCDELAY;

    TR(TRACE_IEVENT, ("kgetch() called"));

    ptr = SP->_keytry;

    for (;;) {
	if (cooked_key_in_fifo() && SP->_fifo[head] >= KEY_MIN) {
	    break;
	} else if (!raw_key_in_fifo()) {
	    ch = fifo_push(EVENTLIST_1st(evl));
	    if (ch == ERR) {
		peek = head;	/* the keys stay uninterpreted */
		return ERR;
	    }
#ifdef NCURSES_WGETCH_EVENTS
	    else if (ch == KEY_EVENT) {
		peek = head;	/* the keys stay uninterpreted */
		return fifo_pull();	/* Remove KEY_EVENT from the queue */
	    }
#endif
	}

	ch = fifo_peek();
	if (ch >= KEY_MIN) {
	    /* If not first in queue, somebody put this key there on purpose in
	     * emergency.  Consider it higher priority than the unfinished
	     * keysequence we are parsing.
	     */
	    peek = head;
	    /* assume the key is the last in fifo */
	    t_dec();		/* remove the key */
	    return ch;
	}

	TR(TRACE_IEVENT, ("ch: %s", _tracechar((unsigned char) ch)));
	while ((ptr != NULL) && (ptr->ch != (unsigned char) ch))
	    ptr = ptr->sibling;

	if (ptr == NULL) {
	    TR(TRACE_IEVENT, ("ptr is null"));
	    break;
	}
	TR(TRACE_IEVENT, ("ptr=%p, ch=%d, value=%d",
			  ptr, ptr->ch, ptr->value));

	if (ptr->value != 0) {	/* sequence terminated */
	    TR(TRACE_IEVENT, ("end of sequence"));
	    if (peek == tail)
		fifo_clear();
	    else
		head = peek;
	    return (ptr->value);
	}

	ptr = ptr->child;

	if (!raw_key_in_fifo()) {
	    int rc;

	    TR(TRACE_IEVENT, ("waiting for rest of sequence"));
	    rc = check_mouse_activity(timeleft EVENTLIST_2nd(evl));
#ifdef NCURSES_WGETCH_EVENTS
	    if (rc & 4) {
		TR(TRACE_IEVENT, ("interrupted by a user event"));
		/* FIXME Should have preserved remainder timeleft for reuse... */
		peek = head;	/* Restart interpreting later */
		return KEY_EVENT;
	    }
#endif
	    if (!rc) {
		TR(TRACE_IEVENT, ("ran out of time"));
		break;
	    }
	}
    }
    ch = fifo_pull();
    peek = head;
    return ch;
}
