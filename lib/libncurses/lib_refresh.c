
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
 *	lib_refresh.c
 *
 *	The routines wredrawln(), wrefresh() and wnoutrefresh().
 *
 */

#include "curses.priv.h"

int wredrawln(WINDOW *win, int beg, int num)
{
	T(("wredrawln(%x,%d,%d) called", win, beg, num));
	touchline(win, beg, num);
	wrefresh(win);
	return OK;
}

int wrefresh(WINDOW *win)
{
	T(("wrefresh(%x) called", win));

	if (win == curscr)
	    	curscr->_clear = TRUE;
	else
	    	wnoutrefresh(win);
	return(doupdate());
}

int wnoutrefresh(WINDOW *win)
{
int	i, j;
int	begx = win->_begx;
int	begy = win->_begy;
int	m, n;

	T(("wnoutrefresh(%x) called", win));

	/*
	 * This function will break badly if we try to refresh a pad.
	 */
	if ((win == 0)
	 || (win->_flags & _ISPAD))
		return(ERR);

	/*
	 * If 'newscr' has a different background than the window that we're
	 * trying to refresh, we'll have to copy the whole thing.
	 */
	if (win->_bkgd != newscr->_bkgd) {
		touchwin(win);
		newscr->_bkgd = win->_bkgd;
	}
	newscr->_attrs = win->_attrs;

	win->_flags &= ~_HASMOVED;
	for (i = 0, m = begy; i <= win->_maxy; i++, m++) {
		if (win->_firstchar[i] != _NOCHANGE) {
			j = win->_firstchar[i];
			n = j + begx;
			for (; j <= win->_lastchar[i]; j++, n++) {
		    	if (win->_line[i][j] != newscr->_line[m][n]) {
					newscr->_line[m][n] = win->_line[i][j];

					if (newscr->_firstchar[m] == _NOCHANGE)
			   			newscr->_firstchar[m] = newscr->_lastchar[m] = n;
					else if (n < newscr->_firstchar[m])
			   			newscr->_firstchar[m] = n;
					else if (n > newscr->_lastchar[m])
			   			newscr->_lastchar[m] = n;
		    	}
			}
		}
		win->_firstchar[i] = win->_lastchar[i] = _NOCHANGE;
	}

	if (win->_clear) {
	   	win->_clear = FALSE;
	   	newscr->_clear = TRUE;
	}

	if (! win->_leave) {
	   	newscr->_cury = win->_cury + win->_begy;
	   	newscr->_curx = win->_curx + win->_begx;
	}
	return(OK);
}
