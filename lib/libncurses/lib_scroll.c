
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_scroll.c
**
**	The routine wscrl(win, n).
**  positive n scroll the window up (ie. move lines down)
**  negative n scroll the window down (ie. move lines up)
**
*/

#include <stdlib.h>
#include "curses.priv.h"
#include <nterm.h>

int
wscrl(WINDOW *win, int n)
{
int	line, i;
chtype	*ptr, *temp;
chtype  **saved;
chtype	blank = ' ';

	T(("wscrl(%x,%d) called", win, n));

    if (! win->_scroll)
		return ERR;

	if (n == 0)
		return OK;

    /* test for scrolling region == entire screen */

    saved = (chtype **)malloc(sizeof(chtype *) * abs(n));

    if (n < 0) {
		/* save overwritten lines */
		
		for (i = 0; i < -n; i++)
		    saved[i] = win->_line[win->_regbottom-i];

		/* shift n lines */
		
		for (line = win->_regbottom; line > win->_regtop+n; line--)
		    win->_line[line] = win->_line[line+n];

		/* restore saved lines and blank them */

		for (i = 0, line = win->_regtop; line < win->_regtop-n; line++, i++) {
		    win->_line[line] = saved[i]; 
		    temp = win->_line[line];
		    for (ptr = temp; ptr - temp <= win->_maxx; ptr++)
				*ptr = blank;
		}
    }

    if (n > 0) {
		/* save overwritten lines */
		
		for (i = 0; i < n; i++)
		    saved[i] = win->_line[win->_regtop+i];

		/* shift n lines */
		
		for (line = win->_regtop; line < win->_regbottom; line++)
		    win->_line[line] = win->_line[line+n];

		/* restore saved lines and blank them */

		for (i = 0, line = win->_regbottom; line > win->_regbottom - n; line--, i++) {
		    win->_line[line] = saved[i];
		    temp = win->_line[line];
		    for (ptr = temp; ptr - temp <= win->_maxx; ptr++)
			*ptr = blank;
		}
	}
	
	free(saved);

	/* as an optimization, if the scrolling region is the entire screen
	   scroll the physical screen */
	/* should we extend this to include smaller scrolling ranges by using
	   change_scroll_region? */

    if (win->_maxx == columns && win->_regtop == 0 && win->_regbottom == lines) {

		wrefresh(win);
		if (back_color_erase) {
			T(("back_color_erase, turning attributes off"));
			vidattr(curscr->_attrs = A_NORMAL);
		}
		/* at the moment this relies on scroll_reverse and scroll_forward
		   or parm_rindex and parm_index.
		   we should add idl support as an alternative */

		if (n > 0) {
			mvcur(-1, -1, win->_regtop, 0);
			if (parm_rindex) {
				putp(tparm(parm_rindex, n));
			} else if (scroll_reverse) {
				while (n--)
					putp(scroll_reverse);
			}
		}

		if (n < 0) {
			mvcur(-1, -1, win->_regbottom, columns);
			n = abs(n);
			if (parm_index) {
				putp(tparm(parm_index, n));
		    } else if (scroll_forward) {
		    	while (n--)
					putp(scroll_forward);
			}
		}

		mvcur(-1, -1, win->_cury, win->_curx);
	} else 
	    touchline(win, win->_regtop, win->_regbottom - win->_regtop + 1);

    return OK;
}
