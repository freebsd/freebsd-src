/****************************************************************************
 * Copyright (c) 2001 Free Software Foundation, Inc.                        *
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
 *  Author: Thomas E. Dickey 1996-2001                                      *
 *     and: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

/*
 *	visbuf.c - Tracing/Debugging support routines
 */

#include <curses.priv.h>

#include <tic.h>
#include <ctype.h>

MODULE_ID("$Id: visbuf.c,v 1.3 2001/11/10 23:47:51 tom Exp $")

static char *
_nc_vischar(char *tp, unsigned c)
{
    if (c == '"' || c == '\\') {
	*tp++ = '\\';
	*tp++ = c;
    } else if (is7bits(c) && (isgraph(c) || c == ' ')) {
	*tp++ = c;
    } else if (c == '\n') {
	*tp++ = '\\';
	*tp++ = 'n';
    } else if (c == '\r') {
	*tp++ = '\\';
	*tp++ = 'r';
    } else if (c == '\b') {
	*tp++ = '\\';
	*tp++ = 'b';
    } else if (c == '\033') {
	*tp++ = '\\';
	*tp++ = 'e';
    } else if (is7bits(c) && iscntrl(UChar(c))) {
	*tp++ = '\\';
	*tp++ = '^';
	*tp++ = '@' + c;
    } else {
	sprintf(tp, "\\%03lo", ChCharOf(c));
	tp += strlen(tp);
    }
    return tp;
}

NCURSES_EXPORT(const char *)
_nc_visbuf2(int bufnum, const char *buf)
{
    char *vbuf;
    char *tp;
    int c;

    if (buf == 0)
	return ("(null)");
    if (buf == CANCELLED_STRING)
	return ("(cancelled)");

#ifdef TRACE
    tp = vbuf = _nc_trace_buf(bufnum, (strlen(buf) * 4) + 5);
#else
    {
	static char *mybuf[2];
	mybuf[bufnum] = _nc_doalloc(mybuf[bufnum], (strlen(buf) * 4) + 5);
	tp = vbuf = mybuf[bufnum];
    }
#endif
    *tp++ = D_QUOTE;
    while ((c = *buf++) != '\0') {
	tp = _nc_vischar(tp, UChar(c));
    }
    *tp++ = D_QUOTE;
    *tp++ = '\0';
    return (vbuf);
}

NCURSES_EXPORT(const char *)
_nc_visbuf(const char *buf)
{
    return _nc_visbuf2(0, buf);
}

#if USE_WIDEC_SUPPORT
#ifdef TRACE
NCURSES_EXPORT(const char *)
_nc_viswbuf2(int bufnum, const wchar_t * buf)
{
    char *vbuf;
    char *tp;
    int c;

    if (buf == 0)
	return ("(null)");

#ifdef TRACE
    tp = vbuf = _nc_trace_buf(bufnum, (wcslen(buf) * 4) + 5);
#else
    {
	static char *mybuf[2];
	mybuf[bufnum] = _nc_doalloc(mybuf[bufnum], (wcslen(buf) * 4) + 5);
	tp = vbuf = mybuf[bufnum];
    }
#endif
    *tp++ = D_QUOTE;
    while ((c = *buf++) != '\0') {
	tp = _nc_vischar(tp, ChCharOf(c));
    }
    *tp++ = D_QUOTE;
    *tp++ = '\0';
    return (vbuf);
}

NCURSES_EXPORT(const char *)
_nc_viswbuf(const wchar_t * buf)
{
    return _nc_viswbuf2(0, buf);
}

NCURSES_EXPORT(const char *)
_nc_viscbuf2(int bufnum, const cchar_t * buf, int len)
{
    size_t have = BUFSIZ;
    char *result = _nc_trace_buf(bufnum, have);
    char *tp = result;
    int n;
    bool same = TRUE;
    attr_t attr = A_NORMAL;
    const char *found;

    if (len < 0)
	len = _nc_wchstrlen(buf);

    for (n = 1; n < len; n++) {
	if (AttrOf(buf[n]) != AttrOf(buf[0])) {
	    same = FALSE;
	    break;
	}
    }

    /*
     * If the rendition is the same for the whole string, display it as a
     * quoted string, followed by the rendition.  Otherwise, use the more
     * detailed trace function that displays each character separately.
     */
    if (same) {
	*tp++ = D_QUOTE;
	while (len-- > 0) {
	    if ((found = _nc_altcharset_name(attr, CharOfD(buf))) != 0) {
		(void) strcpy(tp, found);
		tp += strlen(tp);
		attr &= ~A_ALTCHARSET;
	    } else if (!isnac(CHDEREF(buf))) {
		PUTC_DATA;

		memset(&PUT_st, '\0', sizeof(PUT_st));
		PUTC_i = 0;
		do {
		    PUTC_ch = PUTC_i < CCHARW_MAX ? buf->chars[PUTC_i] : L'\0';
		    PUTC_n = wcrtomb(PUTC_buf, buf->chars[PUTC_i], &PUT_st);
		    if (PUTC_ch == L'\0')
			--PUTC_n;
		    if (PUTC_n <= 0)
			break;
		    for (n = 0; n < PUTC_n; n++) {
			tp = _nc_vischar(tp, UChar(PUTC_buf[n]));
		    }
		    ++PUTC_i;
		} while (PUTC_ch != L'\0');
	    }
	    buf++;
	}
	*tp++ = D_QUOTE;
	*tp++ = '\0';
	if (attr != A_NORMAL)
	    (void) sprintf(tp, " | %s",
			   _traceattr2(bufnum + 20, attr));
    } else {
	*tp++ = L_BRACE;
	while (len-- > 0) {
	    char *temp = _tracecchar_t2(bufnum + 20, buf++);
	    size_t used = (tp - result);
	    size_t want = strlen(temp) + 5 + used;
	    if (want > have) {
		result = _nc_trace_buf(bufnum, have = want);
		tp = result + used;
	    }
	    (void) strcpy(tp, temp);
	    tp += strlen(tp);
	}
	*tp++ = R_BRACE;
	*tp++ = '\0';
    }
    return result;
}

NCURSES_EXPORT(const char *)
_nc_viscbuf(const cchar_t * buf, int len)
{
    return _nc_viscbuf2(0, buf, len);
}
#endif /* TRACE */
#endif /* USE_WIDEC_SUPPORT */
