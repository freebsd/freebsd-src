
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_addch.c
**
**	The routine waddch().
**
*/

#include "curses.priv.h"
#include "unctrl.h"

static int
wladdch(WINDOW *win, chtype c, bool literal)
{
int	x, y;
int	newx;
chtype	ch = c;

	x = win->_curx;
	y = win->_cury;

	if (y > win->_maxy  ||  x > win->_maxx  ||  y < 0  ||  x < 0)
	    return(ERR);

	/* ugly, but necessary --- and, bizarrely enough, even portable! */
	if (literal)
	    goto noctrl;

	switch (ch&A_CHARTEXT) {
    	case '\t':
		for (newx = x + (8 - (x & 07)); x < newx; x++)
	    		if (waddch(win, ' ') == ERR)
				return(ERR);
		return(OK);
    	case '\n':
		wclrtoeol(win);
		x = 0;
		goto newline;
    	case '\r':
		x = 0;
		break;
    	case '\b':
		if (--x < 0)
		    	x = 0;
		break;
    	default:
		if (ch < ' ')
		    	return(waddstr(win, unctrl(ch)));

		/* FALL THROUGH */
        noctrl:
        	T(("win attr = %x", win->_attrs));
		ch |= win->_attrs;

		if ((win->_line[y][x]&A_CHARTEXT) == ' ')
			ch |= win->_bkgd;
		else
			ch |= (win->_bkgd&A_ATTRIBUTES);
		T(("bkg = %x -> ch = %x", win->_bkgd, ch));

		if (win->_line[y][x] != ch) {
		    	if (win->_firstchar[y] == _NOCHANGE)
				win->_firstchar[y] = win->_lastchar[y] = x;
		    	else if (x < win->_firstchar[y])
				win->_firstchar[y] = x;
		    	else if (x > win->_lastchar[y])
				win->_lastchar[y] = x;

		}

		T(("char %d of line %d is %x", x, y, ch));
		win->_line[y][x++] = ch;
		if (x > win->_maxx) {
		    	x = 0;
newline:
		    	y++;
		    	if (y > win->_regbottom) {
				y--;
				if (win->_scroll)
				    	scroll(win);
		    	}
		}
		break;
	}

	win->_curx = x;
	win->_cury = y;

	return(OK);
}

int waddch(WINDOW *win, chtype ch)
{
	TR(TRACE_CHARPUT, ("waddch(%x,%c (%x)) called", win, ch&A_CHARTEXT, ch));
	return wladdch(win, ch, FALSE);
}

int wechochar(WINDOW *win, chtype ch)
{
	T(("wechochar(%x,%c (%x)) called", win, ch&A_CHARTEXT, ch));

	return wladdch(win, ch, TRUE);
}
