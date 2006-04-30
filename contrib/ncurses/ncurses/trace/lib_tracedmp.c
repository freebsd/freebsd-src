/****************************************************************************
 * Copyright (c) 1998,2000,2001 Free Software Foundation, Inc.              *
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
 *	lib_tracedmp.c - Tracing/Debugging routines
 */

#include <curses.priv.h>
#include <ctype.h>

MODULE_ID("$Id: lib_tracedmp.c,v 1.22 2001/11/03 15:45:35 tom Exp $")

#ifdef TRACE
NCURSES_EXPORT(void)
_tracedump(const char *name, WINDOW *win)
{
    static char *buf = 0;
    static size_t used = 0;

    int i, j, n, width;

    /* compute narrowest possible display width */
    for (width = i = 0; i <= win->_maxy; ++i) {
	n = 0;
	for (j = 0; j <= win->_maxx; ++j)
	    if (CharOf(win->_line[i].text[j]) != L(' '))
		n = j;

	if (n > width)
	    width = n;
    }
    if (width < win->_maxx)
	++width;
    if (++width + 1 > (int) used) {
	used = 2 * (width + 1);
	buf = _nc_doalloc(buf, used);
    }

    for (n = 0; n <= win->_maxy; ++n) {
	char *ep = buf;
	bool haveattrs, havecolors;

	/*
	 * Dump A_CHARTEXT part.  It is more important to make the grid line up
	 * in the trace file than to represent control- and wide-characters, so
	 * we map those to '.' and '?' respectively.
	 */
	for (j = 0; j < width; ++j) {
	    chtype test = CharOf(win->_line[n].text[j]);
	    ep[j] = (UChar(test) == test
#if USE_WIDEC_SUPPORT
		     && (win->_line[n].text[j].chars[1] == 0)
#endif
		)
		? (iscntrl(UChar(test))
		   ? '.'
		   : UChar(test))
		: '?';
	}
	ep[j] = '\0';
	_tracef("%s[%2d] %3d%3d ='%s'",
		name, n,
		win->_line[n].firstchar,
		win->_line[n].lastchar,
		ep);

	/* dump A_COLOR part, will screw up if there are more than 96 */
	havecolors = FALSE;
	for (j = 0; j < width; ++j)
	    if (AttrOf(win->_line[n].text[j]) & A_COLOR) {
		havecolors = TRUE;
		break;
	    }
	if (havecolors) {
	    ep = buf;
	    for (j = 0; j < width; ++j)
		ep[j] = UChar(CharOf(win->_line[n].text[j]) >>
			      NCURSES_ATTR_SHIFT) + ' ';
	    ep[j] = '\0';
	    _tracef("%*s[%2d]%*s='%s'", (int) strlen(name),
		    "colors", n, 8, " ", buf);
	}

	for (i = 0; i < 4; ++i) {
	    const char *hex = " 123456789ABCDEF";
	    attr_t mask = (0xf << ((i + 4) * 4));

	    haveattrs = FALSE;
	    for (j = 0; j < width; ++j)
		if (AttrOf(win->_line[n].text[j]) & mask) {
		    haveattrs = TRUE;
		    break;
		}
	    if (haveattrs) {
		ep = buf;
		for (j = 0; j < width; ++j)
		    ep[j] = hex[(AttrOf(win->_line[n].text[j]) & mask) >>
				((i + 4) * 4)];
		ep[j] = '\0';
		_tracef("%*s%d[%2d]%*s='%s'", (int) strlen(name) -
			1, "attrs", i, n, 8, " ", buf);
	    }
	}
    }
#if NO_LEAKS
    free(buf);
    used = 0;
#endif
}

#else
empty_module(_nc_lib_tracedmp)
#endif /* TRACE */
