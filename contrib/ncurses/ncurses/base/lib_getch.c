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
**	lib_getch.c
**
**	The routine getch().
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_getch.c,v 1.54 2000/12/10 02:43:27 tom Exp $")

#include <fifo_defs.h>

NCURSES_EXPORT_VAR(int)
ESCDELAY = 1000;		/* max interval betw. chars in funkeys, in millisecs */

     static inline int
       fifo_peek(void)
{
    int ch = SP->_fifo[peek];
    TR(TRACE_IEVENT, ("peeking at %d", peek));

    p_inc();
    return ch;
}

static inline int
fifo_pull(void)
{
    int ch;
    ch = SP->_fifo[head];
    TR(TRACE_IEVENT, ("pulling %d from %d", ch, head));

    if (peek == head) {
	h_inc();
	peek = head;
    } else
	h_inc();

#ifdef TRACE
    if (_nc_tracing & TRACE_IEVENT)
	_nc_fifo_dump();
#endif
    return ch;
}

static inline int
fifo_push(void)
{
    int n;
    int ch;

    if (tail == -1)
	return ERR;

#ifdef HIDE_EINTR
  again:
    errno = 0;
#endif

#if USE_GPM_SUPPORT || defined(USE_EMX_MOUSE)
    if ((SP->_mouse_fd >= 0)
	&& (_nc_timed_wait(3, -1, (int *) 0) & 2)) {
	SP->_mouse_event(SP);
	ch = KEY_MOUSE;
	n = 1;
    } else
#endif
    {
	unsigned char c2 = 0;
	n = read(SP->_ifd, &c2, 1);
	ch = CharOf(c2);
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
    TR(TRACE_IEVENT, ("pushed %#x at %d", ch, tail));
#ifdef TRACE
    if (_nc_tracing & TRACE_IEVENT)
	_nc_fifo_dump();
#endif
    return ch;
}

static inline void
fifo_clear(void)
{
    int i;
    for (i = 0; i < FIFO_SIZE; i++)
	SP->_fifo[i] = 0;
    head = -1;
    tail = peek = 0;
}

static int kgetch(WINDOW *);

#define wgetch_should_refresh(win) (\
	(is_wintouched(win) || (win->_flags & _HASMOVED)) \
	&& !(win->_flags & _ISPAD))

NCURSES_EXPORT(int)
wgetch(WINDOW *win)
{
    int ch;

    T((T_CALLED("wgetch(%p)"), win));

    if (!win)
	returnCode(ERR);

    if (cooked_key_in_fifo()) {
	if (wgetch_should_refresh(win))
	    wrefresh(win);

	ch = fifo_pull();
	T(("wgetch returning (pre-cooked): %#x = %s", ch, _trace_key(ch)));
	returnCode(ch);
    }

    /*
     * Handle cooked mode.  Grab a string from the screen,
     * stuff its contents in the FIFO queue, and pop off
     * the first character to return it.
     */
    if (head == -1 && !SP->_raw && !SP->_cbreak) {
	char buf[MAXCOLUMNS], *sp;

	TR(TRACE_IEVENT, ("filling queue in cooked mode"));

	wgetnstr(win, buf, MAXCOLUMNS);

	/* ungetch in reverse order */
	ungetch('\n');
	for (sp = buf + strlen(buf); sp > buf; sp--)
	    ungetch(sp[-1]);

	returnCode(fifo_pull());
    }

    if (wgetch_should_refresh(win))
	wrefresh(win);

    if (!win->_notimeout && (win->_delay >= 0 || SP->_cbreak > 1)) {
	int delay;

	TR(TRACE_IEVENT, ("timed delay in wgetch()"));
	if (SP->_cbreak > 1)
	    delay = (SP->_cbreak - 1) * 100;
	else
	    delay = win->_delay;

	TR(TRACE_IEVENT, ("delay is %d milliseconds", delay));

	if (head == -1)		/* fifo is empty */
	    if (!_nc_timed_wait(3, delay, (int *) 0))
		returnCode(ERR);
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

	do {
	    ch = kgetch(win);
	    if (ch == KEY_MOUSE) {
		++runcount;
		if (SP->_mouse_inline(SP))
		    break;
	    }
	} while
	    (ch == KEY_MOUSE
	     && (_nc_timed_wait(3, SP->_maxclick, (int *) 0)
		 || !SP->_mouse_parse(runcount)));
	if (runcount > 0 && ch != KEY_MOUSE) {
	    /* mouse event sequence ended by keystroke, push it */
	    ungetch(ch);
	    ch = KEY_MOUSE;
	}
    } else {
	if (head == -1)
	    fifo_push();
	ch = fifo_pull();
    }

    if (ch == ERR) {
#if USE_SIZECHANGE
	if (SP->_sig_winch) {
	    _nc_update_screensize();
	    /* resizeterm can push KEY_RESIZE */
	    if (cooked_key_in_fifo()) {
		ch = fifo_pull();
		T(("wgetch returning (pre-cooked): %#x = %s", ch, _trace_key(ch)));
		returnCode(ch);
	    }
	}
#endif
	T(("wgetch returning ERR"));
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
    if ((ch < KEY_MIN) && (ch & 0x80))
	if (!SP->_use_meta)
	    ch &= 0x7f;

    T(("wgetch returning : %#x = %s", ch, _trace_key(ch)));

    returnCode(ch);
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
**	This function must be called when there is no cooked keys in queue.
**	(that is head==-1 || peek==head)
**
*/

static int
kgetch(WINDOW *win GCC_UNUSED)
{
    struct tries *ptr;
    int ch = 0;
    int timeleft = ESCDELAY;

    TR(TRACE_IEVENT, ("kgetch(%p) called", win));

    ptr = SP->_keytry;

    for (;;) {
	if (!raw_key_in_fifo()) {
	    if (fifo_push() == ERR) {
		peek = head;	/* the keys stay uninterpreted */
		return ERR;
	    }
	}
	ch = fifo_peek();
	if (ch >= KEY_MIN) {
	    peek = head;
	    /* assume the key is the last in fifo */
	    t_dec();		/* remove the key */
	    return ch;
	}

	TR(TRACE_IEVENT, ("ch: %s", _trace_key((unsigned char) ch)));
	while ((ptr != NULL) && (ptr->ch != (unsigned char) ch))
	    ptr = ptr->sibling;
#ifdef TRACE
	if (ptr == NULL) {
	    TR(TRACE_IEVENT, ("ptr is null"));
	} else
	    TR(TRACE_IEVENT, ("ptr=%p, ch=%d, value=%d",
			      ptr, ptr->ch, ptr->value));
#endif /* TRACE */

	if (ptr == NULL)
	    break;

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
	    TR(TRACE_IEVENT, ("waiting for rest of sequence"));
	    if (!_nc_timed_wait(3, timeleft, &timeleft)) {
		TR(TRACE_IEVENT, ("ran out of time"));
		break;
	    }
	}
    }
    ch = fifo_pull();
    peek = head;
    return ch;
}
