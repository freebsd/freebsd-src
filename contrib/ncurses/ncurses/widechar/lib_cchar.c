/****************************************************************************
 * Copyright (c) 2001,2002 Free Software Foundation, Inc.                   *
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
**	lib_cchar.c
**
**	The routines setcchar() and getcchar().
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_cchar.c,v 1.5 2002/04/27 22:35:46 tom Exp $")

/* 
 * The SuSv2 description leaves some room for interpretation.  We'll assume wch
 * is L'\0' terminated, contains at most one character with strictly positive
 * width, which must be the first, and contains no characters of negative
 * width.
 */
NCURSES_EXPORT(int)
setcchar(cchar_t * wcval, const wchar_t * wch, const attr_t attrs,
	 short color_pair, const void *opts)
{
    int i;
    int len;
    int code = OK;

    TR(TRACE_CCALLS, (T_CALLED("setcchar(%p,%s,%ld,%d,%p)"),
		      wcval, _nc_viswbuf(wch), attrs, color_pair, opts));

    if (opts != NULL || (len = wcslen(wch)) > CCHARW_MAX
	|| (len > 0 && wcwidth(wch[0]) < 0)) {
	code = ERR;
    } else {

	for (i = 1; i < len; ++i) {
	    if (wcwidth(wch[i]) != 0) {
		code = ERR;
		break;
	    }
	}

	if (code != ERR) {
	    memset(wcval, 0, sizeof(*wcval));

	    if (len != 0) {
		SetAttr(*wcval, attrs | color_pair);
		memcpy(&wcval->chars, wch, len * sizeof(wchar_t));
		TR(TRACE_CCALLS, ("copy %d wchars, first is %s", len,
				  _tracecchar_t(wcval)));
	    }
	}
    }

    TR(TRACE_CCALLS, (T_RETURN("%d"), code));
    return (code);
}

NCURSES_EXPORT(int)
getcchar(const cchar_t * wcval, wchar_t * wch, attr_t * attrs,
	 short *color_pair, void *opts)
{
    wchar_t *wp;
    int len;
    int code = ERR;

    TR(TRACE_CCALLS, (T_CALLED("getcchar(%p,%p,%p,%p,%p)"),
		      wcval, wch, attrs, color_pair, opts));

    if (opts == NULL) {
	len = (wp = wmemchr(wcval->chars, L'\0', CCHARW_MAX))
	    ? wp - wcval->chars
	    : CCHARW_MAX;

	if (wch == NULL) {
	    code = len;
	} else if (len >= 0) {
	    *attrs = AttrOf(*wcval) & A_ATTRIBUTES;
	    *color_pair = AttrOf(*wcval) & A_COLOR;
	    wmemcpy(wch, wcval->chars, (unsigned) len);
	    wch[len] = L'\0';
	    code = OK;
	}
    }

    TR(TRACE_CCALLS, (T_RETURN("%d"), code));
    return (code);
}
