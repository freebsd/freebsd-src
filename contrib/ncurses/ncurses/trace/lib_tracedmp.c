/****************************************************************************
 * Copyright (c) 1998,2000 Free Software Foundation, Inc.                   *
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
 *	lib_tracedmp.c - Tracing/Debugging routines
 */

#include <curses.priv.h>

MODULE_ID("$Id: lib_tracedmp.c,v 1.16 2000/12/10 03:02:45 tom Exp $")

#ifdef TRACE
NCURSES_EXPORT(void)
_tracedump(const char *name, WINDOW *win)
{
    int i, j, n, width;

    /* compute narrowest possible display width */
    for (width = i = 0; i <= win->_maxy; i++) {
	n = 0;
	for (j = 0; j <= win->_maxx; j++)
	    if (win->_line[i].text[j] != ' ')
		n = j;

	if (n > width)
	    width = n;
    }
    if (width < win->_maxx)
	++width;

    for (n = 0; n <= win->_maxy; n++) {
	char buf[BUFSIZ], *ep;
	bool haveattrs, havecolors;

	/* dump A_CHARTEXT part */
	(void) sprintf(buf, "%s[%2d] %3d%3d ='",
		       name, n,
		       win->_line[n].firstchar,
		       win->_line[n].lastchar);
	ep = buf + strlen(buf);
	for (j = 0; j <= width; j++) {
	    ep[j] = TextOf(win->_line[n].text[j]);
	    if (ep[j] == 0)
		ep[j] = '.';
	}
	ep[j] = '\'';
	ep[j + 1] = '\0';
	_tracef("%s", buf);

	/* dump A_COLOR part, will screw up if there are more than 96 */
	havecolors = FALSE;
	for (j = 0; j <= width; j++)
	    if (win->_line[n].text[j] & A_COLOR) {
		havecolors = TRUE;
		break;
	    }
	if (havecolors) {
	    (void) sprintf(buf, "%*s[%2d]%*s='", (int) strlen(name),
			   "colors", n, 8, " ");
	    ep = buf + strlen(buf);
	    for (j = 0; j <= width; j++)
		ep[j] = CharOf(win->_line[n].text[j] >> 8) + ' ';
	    ep[j] = '\'';
	    ep[j + 1] = '\0';
	    _tracef("%s", buf);
	}

	for (i = 0; i < 4; i++) {
	    const char *hex = " 123456789ABCDEF";
	    chtype mask = (0xf << ((i + 4) * 4));

	    haveattrs = FALSE;
	    for (j = 0; j <= width; j++)
		if (win->_line[n].text[j] & mask) {
		    haveattrs = TRUE;
		    break;
		}
	    if (haveattrs) {
		(void) sprintf(buf, "%*s%d[%2d]%*s='", (int) strlen(name) -
			       1, "attrs", i, n, 8, " ");
		ep = buf + strlen(buf);
		for (j = 0; j <= width; j++)
		    ep[j] = hex[(win->_line[n].text[j] & mask) >> ((i + 4) * 4)];
		ep[j] = '\'';
		ep[j + 1] = '\0';
		_tracef("%s", buf);
	    }
	}
    }
}
#else
extern
NCURSES_EXPORT(void)
_nc_lib_tracedmp(void);
NCURSES_EXPORT(void)
_nc_lib_tracedmp(void)
{
}
#endif /* TRACE */
