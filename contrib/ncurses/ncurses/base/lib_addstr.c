/****************************************************************************
 * Copyright (c) 1998,1999,2000,2001 Free Software Foundation, Inc.         *
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
**	lib_addstr.c
*
**	The routines waddnstr(), waddchnstr().
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_addstr.c,v 1.31 2001/12/19 01:05:52 tom Exp $")

#if USE_WIDEC_SUPPORT
#define CONV_DATA   mbstate_t state; wchar_t cached; int clen = 0
#define CONV_INIT   memset (&state, '\0', sizeof (state)); cached = (wchar_t)WEOF
#define NEXT_CHAR(s,ch, n)						\
    {									\
	int len, i = 0;							\
	memset(&ch, 0, sizeof(cchar_t));				\
	if (cached != (wchar_t) WEOF) {					\
	    ch.chars[i++] = cached;					\
	    cached = (wchar_t) WEOF;					\
	    n -= clen;							\
	    s += clen;							\
	}								\
	for (; i < CCHARW_MAX && n > 0; ++i) {				\
	    if ((len = mbrtowc(&ch.chars[i], s, n, &state)) < 0) {	\
		code = ERR;						\
		break;							\
	    }								\
	    if (i == 0 || wcwidth(ch.chars[i]) == 0) {			\
		n -= len;						\
		s += len;						\
	    } else {							\
		cached = ch.chars[i];					\
		clen = len;						\
		ch.chars[i] = L'\0';					\
		break;							\
	    }								\
	}								\
	if (code == ERR)						\
	    break;							\
    }
#else
#define CONV_DATA
#define CONV_INIT
#define NEXT_CHAR(s,ch, n)						\
    ch = *s++;								\
    --n
#endif

NCURSES_EXPORT(int)
waddnstr(WINDOW *win, const char *const astr, int n)
{
    unsigned const char *str = (unsigned const char *) astr;
    int code = ERR;
    CONV_DATA;

    T((T_CALLED("waddnstr(%p,%s,%d)"), win, _nc_visbuf(astr), n));

    if (win && (str != 0)) {
	TR(TRACE_VIRTPUT | TRACE_ATTRS, ("... current %s", _traceattr(win->_attrs)));
	code = OK;
	if (n < 0)
	    n = (int) strlen(astr);

	TR(TRACE_VIRTPUT, ("str is not null, length = %d", n));
	CONV_INIT;
	while ((n > 0) && (*str != '\0')) {
	    NCURSES_CH_T ch;
	    TR(TRACE_VIRTPUT, ("*str = %#x", *str));
	    NEXT_CHAR(str, ch, n);
	    if (_nc_waddch_nosync(win, ch) == ERR) {
		code = ERR;
		break;
	    }
	}
	_nc_synchook(win);
    }
    TR(TRACE_VIRTPUT, ("waddnstr returns %d", code));
    returnCode(code);
}

NCURSES_EXPORT(int)
waddchnstr(WINDOW *win, const chtype * const astr, int n)
{
    NCURSES_SIZE_T y = win->_cury;
    NCURSES_SIZE_T x = win->_curx;
    int code = OK;
    struct ldat *line;

    T((T_CALLED("waddchnstr(%p,%p,%d)"), win, astr, n));

    if (!win)
	returnCode(ERR);

    if (n < 0) {
	const chtype *str;
	n = 0;
	for (str = (const chtype *) astr; *str != 0; str++)
	    n++;
    }
    if (n > win->_maxx - x + 1)
	n = win->_maxx - x + 1;
    if (n == 0)
	returnCode(code);

    line = &(win->_line[y]);
#if USE_WIDEC_SUPPORT
    {
	int i;
	for (i = 0; i < n; ++i)
	    SetChar(line->text[i + x], ChCharOf(astr[i]), ChAttrOf(astr[i]));
    }
#else
    memcpy(line->text + x, astr, n * sizeof(*astr));
#endif
    CHANGED_RANGE(line, x, x + n - 1);

    _nc_synchook(win);
    returnCode(code);
}

#if USE_WIDEC_SUPPORT

int
_nc_wchstrlen(const cchar_t * s)
{
    int result = 0;
    while (CharOf(s[result]) != L'\0') {
	result++;
    }
    return result;
}

NCURSES_EXPORT(int)
wadd_wchnstr(WINDOW *win, const cchar_t * const astr, int n)
{
    NCURSES_SIZE_T y = win->_cury;
    NCURSES_SIZE_T x = win->_curx;
    int code = OK;
    struct ldat *line;
    int i, start, end;

    T((T_CALLED("wadd_wchnstr(%p,%s,%d)"), win, _nc_viscbuf(astr, n), n));

    if (!win)
	returnCode(ERR);

    if (n < 0) {
	n = _nc_wchstrlen(astr);
    }
    if (n > win->_maxx - x + 1)
	n = win->_maxx - x + 1;
    if (n == 0)
	returnCode(code);

    line = &(win->_line[y]);
    start = x;
    end = x + n - 1;
    if (isnac(line->text[x])) {
	line->text[x - 1] = win->_nc_bkgd;
	--start;
    }
    for (i = 0; i < n && x <= win->_maxx; ++i) {
	line->text[x++] = astr[i];
	if (wcwidth(CharOf(astr[i])) > 1) {
	    if (x <= win->_maxx)
		AddAttr(line->text[x++], WA_NAC);
	    else
		line->text[x - 1] = win->_nc_bkgd;
	}
    }
    if (x <= win->_maxx && isnac(line->text[x])) {
	line->text[x] = win->_nc_bkgd;
	++end;
    }
    CHANGED_RANGE(line, start, end);

    _nc_synchook(win);
    returnCode(code);
}

NCURSES_EXPORT(int)
waddnwstr(WINDOW *win, const wchar_t * str, int n)
{
    int code = ERR;
    int i;

    T((T_CALLED("waddnwstr(%p,%s,%d)"), win, _nc_viswbuf(str), n));

    if (win && (str != 0)) {
	TR(TRACE_VIRTPUT | TRACE_ATTRS, ("... current %s", _traceattr(win->_attrs)));
	code = OK;
	if (n < 0)
	    n = (int) wcslen(str);

	TR(TRACE_VIRTPUT, ("str is not null, length = %d", n));
	while ((n-- > 0) && (*str != L('\0'))) {
	    NCURSES_CH_T ch;
	    TR(TRACE_VIRTPUT, ("*str[0] = %#lx", *str));
	    SetChar(ch, *str++, A_NORMAL);
	    i = 1;
	    while (i < CCHARW_MAX && n > 0 && (*str != L('\0'))
		   && wcwidth(*str) == 0) {
		TR(TRACE_VIRTPUT, ("*str[%d] = %#lx", i, *str));
		ch.chars[i++] = *str++;
		--n;
	    }
	    if (_nc_waddch_nosync(win, ch) == ERR) {
		code = ERR;
		break;
	    }
	}
	_nc_synchook(win);
    }
    TR(TRACE_VIRTPUT, ("waddnwstr returns %d", code));
    returnCode(code);
}

#endif
