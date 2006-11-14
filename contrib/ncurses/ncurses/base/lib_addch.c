/****************************************************************************
 * Copyright (c) 1998-2001,2002 Free Software Foundation, Inc.              *
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

/*
**	lib_addch.c
**
**	The routine waddch().
**
*/

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("$Id: lib_addch.c,v 1.64 2002/05/11 18:16:43 tom Exp $")

/*
 * Ugly microtweaking alert.  Everything from here to end of module is
 * likely to be speed-critical -- profiling data sure says it is!
 * Most of the important screen-painting functions are shells around
 * waddch().  So we make every effort to reduce function-call overhead
 * by inlining stuff, even at the cost of making wrapped copies for
 * export.  Also we supply some internal versions that don't call the
 * window sync hook, for use by string-put functions.
 */

/* Return bit mask for clearing color pair number if given ch has color */
#define COLOR_MASK(ch) (~(attr_t)((ch)&A_COLOR?A_COLOR:0))

static inline NCURSES_CH_T
render_char(WINDOW *win, NCURSES_CH_T ch)
/* compute a rendition of the given char correct for the current context */
{
    attr_t a = win->_attrs;

    if (ISBLANK(ch) && AttrOf(ch) == A_NORMAL) {
	/* color in attrs has precedence over bkgrnd */
	ch = win->_nc_bkgd;
	SetAttr(ch, a | (AttrOf(win->_nc_bkgd) & COLOR_MASK(a)));
    } else {
	/* color in attrs has precedence over bkgrnd */
	a |= (AttrOf(win->_nc_bkgd) & A_ATTRIBUTES) & COLOR_MASK(a);
	/* color in ch has precedence */
	AddAttr(ch, (a & COLOR_MASK(AttrOf(ch))));
    }

    TR(TRACE_VIRTPUT, ("bkg = %s, attrs = %s -> ch = %s",
		       _tracech_t2(1, CHREF(win->_nc_bkgd)),
		       _traceattr(win->_attrs),
		       _tracech_t2(3, CHREF(ch))));

    return (ch);
}

NCURSES_EXPORT(NCURSES_CH_T)
_nc_render(WINDOW *win, NCURSES_CH_T ch)
/* make render_char() visible while still allowing us to inline it below */
{
    return render_char(win, ch);
}

/* check if position is legal; if not, return error */
#ifndef NDEBUG			/* treat this like an assertion */
#define CHECK_POSITION(win, x, y) \
	if (y > win->_maxy \
	 || x > win->_maxx \
	 || y < 0 \
	 || x < 0) { \
		TR(TRACE_VIRTPUT, ("Alert! Win=%p _curx = %d, _cury = %d " \
				   "(_maxx = %d, _maxy = %d)", win, x, y, \
				   win->_maxx, win->_maxy)); \
		return(ERR); \
	}
#else
#define CHECK_POSITION(win, x, y)	/* nothing */
#endif

static inline int
waddch_literal(WINDOW *win, NCURSES_CH_T ch)
{
    int x;
    struct ldat *line;

    x = win->_curx;

    CHECK_POSITION(win, x, win->_cury);

    /*
     * If we're trying to add a character at the lower-right corner more
     * than once, fail.  (Moving the cursor will clear the flag).
     */
#if 0				/* Solaris 2.6 allows updating the corner more than once */
    if (win->_flags & _WRAPPED) {
	if (x >= win->_maxx)
	    return (ERR);
	win->_flags &= ~_WRAPPED;
    }
#endif

    ch = render_char(win, ch);
    TR(TRACE_VIRTPUT, ("win attr = %s", _traceattr(win->_attrs)));

    line = win->_line + win->_cury;

    CHANGED_CELL(line, x);

    line->text[x++] = ch;
    if_WIDEC({
	if (wcwidth(CharOf(ch)) > 1)
	    AddAttr(line->text[x++], WA_NAC);
    });

    TR(TRACE_VIRTPUT, ("(%d, %d) = %s", win->_cury, x, _tracech_t(CHREF(ch))));
    if (x > win->_maxx) {
	/*
	 * The _WRAPPED flag is useful only for telling an application that
	 * we've just wrapped the cursor.  We don't do anything with this flag
	 * except set it when wrapping, and clear it whenever we move the
	 * cursor.  If we try to wrap at the lower-right corner of a window, we
	 * cannot move the cursor (since that wouldn't be legal).  So we return
	 * an error (which is what SVr4 does).  Unlike SVr4, we can
	 * successfully add a character to the lower-right corner (Solaris 2.6
	 * does this also, however).
	 */
	win->_flags |= _WRAPPED;
	if (++win->_cury > win->_regbottom) {
	    win->_cury = win->_regbottom;
	    win->_curx = win->_maxx;
	    if (!win->_scroll)
		return (ERR);
	    scroll(win);
	}
	win->_curx = 0;
	return (OK);
    }
    win->_curx = x;
    return OK;
}

static inline int
waddch_nosync(WINDOW *win, const NCURSES_CH_T ch)
/* the workhorse function -- add a character to the given window */
{
    int x, y;
    chtype t = 0;
    const char *s = 0;

    if ((AttrOf(ch) & A_ALTCHARSET)
	|| ((t = CharOf(ch)) > 127)
	|| ((s = unctrl(t))[1] == 0))
	return waddch_literal(win, ch);

    x = win->_curx;
    y = win->_cury;

    switch (t) {
    case '\t':
	x += (TABSIZE - (x % TABSIZE));

	/*
	 * Space-fill the tab on the bottom line so that we'll get the
	 * "correct" cursor position.
	 */
	if ((!win->_scroll && (y == win->_regbottom))
	    || (x <= win->_maxx)) {
	    NCURSES_CH_T blank = NewChar2(BLANK_TEXT, BLANK_ATTR);
	    AddAttr(blank, AttrOf(ch));
	    while (win->_curx < x) {
		if (waddch_literal(win, blank) == ERR)
		    return (ERR);
	    }
	    break;
	} else {
	    wclrtoeol(win);
	    win->_flags |= _WRAPPED;
	    if (++y > win->_regbottom) {
		x = win->_maxx;
		y--;
		if (win->_scroll) {
		    scroll(win);
		    x = 0;
		}
	    } else {
		x = 0;
	    }
	}
	break;
    case '\n':
	wclrtoeol(win);
	if (++y > win->_regbottom) {
	    y--;
	    if (win->_scroll)
		scroll(win);
	    else
		return (ERR);
	}
	/* FALLTHRU */
    case '\r':
	x = 0;
	win->_flags &= ~_WRAPPED;
	break;
    case '\b':
	if (x == 0)
	    return (OK);
	x--;
	win->_flags &= ~_WRAPPED;
	break;
    default:
	while (*s) {
	    NCURSES_CH_T sch;
	    SetChar(sch, *s++, AttrOf(ch));
	    if (waddch_literal(win, sch) == ERR)
		return ERR;
	}
	return (OK);
    }

    win->_curx = x;
    win->_cury = y;

    return (OK);
}

NCURSES_EXPORT(int)
_nc_waddch_nosync(WINDOW *win, const NCURSES_CH_T c)
/* export copy of waddch_nosync() so the string-put functions can use it */
{
    return (waddch_nosync(win, c));
}

/*
 * The versions below call _nc_synhook().  We wanted to avoid this in the
 * version exported for string puts; they'll call _nc_synchook once at end
 * of run.
 */

/* These are actual entry points */

NCURSES_EXPORT(int)
waddch(WINDOW *win, const chtype ch)
{
    int code = ERR;
    NCURSES_CH_T wch;
    SetChar2(wch, ch);

    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_CALLED("waddch(%p, %s)"), win,
				      _tracechtype(ch)));

    if (win && (waddch_nosync(win, wch) != ERR)) {
	_nc_synchook(win);
	code = OK;
    }

    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_RETURN("%d"), code));
    return (code);
}

NCURSES_EXPORT(int)
wechochar(WINDOW *win, const chtype ch)
{
    int code = ERR;
    NCURSES_CH_T wch;
    SetChar2(wch, ch);

    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_CALLED("wechochar(%p, %s)"), win,
				      _tracechtype(ch)));

    if (win && (waddch_nosync(win, wch) != ERR)) {
	bool save_immed = win->_immed;
	win->_immed = TRUE;
	_nc_synchook(win);
	win->_immed = save_immed;
	code = OK;
    }
    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_RETURN("%d"), code));
    return (code);
}

#if USE_WIDEC_SUPPORT
NCURSES_EXPORT(int)
wadd_wch(WINDOW *win, const cchar_t * wch)
{
    int code = ERR;

    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_CALLED("wadd_wch(%p, %s)"), win,
				      _tracech_t(wch)));

    if (win && (waddch_nosync(win, *wch) != ERR)) {
	_nc_synchook(win);
	code = OK;
    }

    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_RETURN("%d"), code));
    return (code);
}

NCURSES_EXPORT(int)
wecho_wchar(WINDOW *win, const cchar_t * wch)
{
    int code = ERR;

    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_CALLED("wecho_wchar(%p, %s)"), win,
				      _tracech_t(wch)));

    if (win && (waddch_nosync(win, *wch) != ERR)) {
	bool save_immed = win->_immed;
	win->_immed = TRUE;
	_nc_synchook(win);
	win->_immed = save_immed;
	code = OK;
    }
    TR(TRACE_VIRTPUT | TRACE_CCALLS, (T_RETURN("%d"), code));
    return (code);
}
#endif /* USE_WIDEC_SUPPORT */
