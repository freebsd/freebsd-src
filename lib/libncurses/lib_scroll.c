
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
#include "terminfo.h"

void _nc_soft_scroll_window(WINDOW *win, int const n, short const top, short const bottom, chtype blank)
{
int	line, i;
chtype	*ptr, *temp;
chtype  **saved;

	saved = (chtype **)calloc(abs(n), sizeof(chtype *));

    	if (n < 0) {
		/* save overwritten lines */

		for (i = 0; i < -n && bottom-i >= 0; i++)
			saved[i] = win->_line[bottom-i];

		/* shift n lines */

		for (line = bottom; line >= top-n && line >= 0; line--)
			win->_line[line] = win->_line[line+n];

		/* restore saved lines and blank them */

		for (i = 0, line = top; line < top-n && line <= win->_maxy; line++, i++) {
			if (saved[i])
				win->_line[line] = saved[i];
			temp = win->_line[line];
		    	for (ptr = temp; ptr - temp <= win->_maxx; ptr++)
				*ptr = blank;
		}
	} else {
		/* save overwritten lines */

		for (i = 0; i < n && top+i <= win->_maxy; i++)
			saved[i] = win->_line[top+i];

		/* shift n lines */

		for (line = top; line <= bottom-n && line+n <= win->_maxy; line++)
			win->_line[line] = win->_line[line+n];

		/* restore saved lines and blank them */

		for (i = 0, line = bottom; line > bottom - n && line >= 0; line--, i++) {
			if (saved[i])
				win->_line[line] = saved[i];
			temp = win->_line[line];
		    	for (ptr = temp; ptr - temp <= win->_maxx; ptr++)
				*ptr = blank;
		}
	}
	/* touchline(win, top, bottom-top+1); */        /* not yet */
	free(saved);
}

void _nc_scroll_window(WINDOW *win, int n, short const top, short const bottom, chtype blank)
{
int physical = FALSE;
int i;

	if (top == bottom) {
		int sy, sx;

		getyx(win, sy, sx);
		win->_curx = 0;
		win->_cury = top;
		wclrtoeol(win);
		win->_curx = sx;
		win->_cury = sy;
		return;
	}

	if (n > lines)
		n = lines;
	else if (-n > lines)
		n = -lines;

	/* as an optimization, if the scrolling region is the entire screen
	   scroll the physical screen */

	if (   win->_begx == 0 && win->_maxx == columns - 1
	    && !memory_above && !memory_below
	    && ((((win->_begy+top == 0 && win->_begy+bottom == lines - 1)
		  || change_scroll_region)
		 && (   (n < 0 && (parm_rindex || scroll_reverse))
		     || (n > 0 && (parm_index || scroll_forward))
		    )
		) || (win->_idlok && (parm_insert_line || insert_line)
		      && (parm_delete_line || delete_line)
		     )
	       )
	   )
		physical = TRUE;

	if (physical == TRUE) {
		wrefresh(win);
		_nc_soft_scroll_window(curscr, n, win->_begy+top, win->_begy+bottom, blank);
		_nc_soft_scroll_window(newscr, n, win->_begy+top, win->_begy+bottom, blank);
	}
	_nc_soft_scroll_window(win, n, top, bottom, blank);

	if (physical == TRUE) {
		if (n < 0) {
			if (   ((   win->_begy+top == 0
				 && win->_begy+bottom == lines - 1)
				|| change_scroll_region)
			    && (parm_rindex || scroll_reverse)
			   ) {
				if (change_scroll_region &&
				    (win->_begy+top != 0 || win->_begy+bottom != lines - 1)
				   )
					putp(tparm(change_scroll_region, win->_begy+top, win->_begy+bottom));
				i = abs(n);
				mvcur(-1, -1, win->_begy+top, 0);
				if (parm_rindex) {
					putp(tparm(parm_rindex, i));
				} else if (scroll_reverse) {
					while (i--)
						putp(scroll_reverse);
				}
				if (change_scroll_region &&
				    (win->_begy+top != 0 || win->_begy+bottom != lines - 1)
				   )
					putp(tparm(change_scroll_region, 0, lines-1));
			} else {
				i = abs(n);
				if (win->_begy+bottom < lines - 1) {
					mvcur(-1, -1, win->_begy+bottom, 0);
					if (parm_delete_line) {
						putp(tparm(parm_delete_line, i));
					} else if (delete_line) {
						while (i--)
							putp(delete_line);
						i = abs(n);
					}
				}
				mvcur(-1, -1, win->_begy+top, 0);
				if (parm_insert_line) {
					putp(tparm(parm_insert_line, i));
				} else if (insert_line) {
					while (i--)
						putp(insert_line);
				}
			}
		} else {
			if (   ((   win->_begy+top == 0
				 && win->_begy+bottom == lines - 1)
				|| change_scroll_region)
			    && (parm_index || scroll_forward)
			   ) {
				if (change_scroll_region &&
				    (win->_begy+top != 0 || win->_begy+bottom != lines - 1)
				   )
					putp(tparm(change_scroll_region, win->_begy+top, win->_begy+bottom));
				mvcur(-1, -1, win->_begy+bottom, 0);
				if (parm_index) {
					putp(tparm(parm_index, n));
				} else if (scroll_forward) {
					i = n;
					while (i--)
						putp(scroll_forward);
				}
				if (change_scroll_region &&
				    (win->_begy+top != 0 || win->_begy+bottom != lines - 1)
				   )
					putp(tparm(change_scroll_region, 0, lines-1));
			} else {
				mvcur(-1, -1, win->_begy+top, 0);
				if (parm_delete_line) {
					putp(tparm(parm_delete_line, n));
				} else if (delete_line) {
					i = n;
					while (i--)
						putp(delete_line);
				}
				if (win->_begy+bottom < lines - 1) {
					mvcur(win->_begy+top, 0, win->_begy+bottom, 0);
					if (parm_insert_line) {
						putp(tparm(parm_insert_line, n));
					} else if (insert_line) {
						i = n;
						while (i--)
							putp(insert_line);
					}
				}
			}
		}

		mvcur(-1, -1, win->_begy+win->_cury, win->_begx+win->_curx);
	} else
		touchline(win, top, bottom - top + 1);
}


int
wscrl(WINDOW *win, int n)
{
	T(("wscrl(%x,%d) called", win, n));

	if (!win || !win->_scroll)
		return ERR;

	if (n == 0)
		return OK;

	if ((n > (win->_regbottom - win->_regtop)) || 
	    (-n > (win->_regbottom - win->_regtop)))
	    return ERR;

	_nc_scroll_window(win, n, win->_regtop, win->_regbottom, _nc_background(win));

    	return OK;
}
