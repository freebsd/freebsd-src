
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
int     line, i, touched = 0;
chtype	*ptr, *temp;
chtype  **saved, **newsaved = NULL, **cursaved = NULL;
chtype	blank = ' ';

	T(("wscrl(%x,%d) called", win, n));

    if (! win->_scroll)
		return ERR;

	if (n == 0)
		return OK;

    /* test for scrolling region == entire screen */

	/* as an optimization, if the scrolling region is the entire screen
	   scroll the physical screen */
	/* should we extend this to include smaller scrolling ranges by using
	   change_scroll_region? */

    if (   win->_begx == 0 && win->_maxx == columns - 1
	&& !memory_above && !memory_below
	&& ((win->_regtop == 0 && win->_regbottom == lines - 1
	     && (   (n < 0 && (parm_rindex || scroll_reverse))
		 || (n > 0 && (parm_index || scroll_forward))
		)
	    ) || (win->_idlok && (parm_insert_line || insert_line)
		  && (parm_delete_line || delete_line)
		 )
	   )
       ) {
		wrefresh(win);
		if (n < 0) {
			if (   win->_regtop == 0 && win->_regbottom == lines - 1
			    && (parm_rindex || scroll_reverse)
			   ) {
				i = abs(n);
				mvcur(-1, -1, win->_regtop, 0);
				if (parm_rindex) {
					putp(tparm(parm_rindex, i));
				} else if (scroll_reverse) {
					while (i--)
						putp(scroll_reverse);
				}
			} else {
				i = abs(n);
				if (win->_regbottom < lines - 1) {
					mvcur(-1, -1, win->_regbottom, 0);
					if (parm_delete_line) {
						putp(tparm(parm_delete_line, i));
					} else if (delete_line) {
						while (i--)
							putp(delete_line);
						i = abs(n);
					}
				}
				mvcur(-1, -1, win->_regtop, 0);
				if (parm_insert_line) {
					putp(tparm(parm_insert_line, i));
				} else if (insert_line) {
					while (i--)
						putp(insert_line);
				}
			}
		} else {
			if (   win->_regtop == 0 && win->_regbottom == lines - 1
			    && (parm_index || scroll_forward)
			   ) {
				mvcur(-1, -1, win->_regbottom, 0);
				if (parm_index) {
					putp(tparm(parm_index, n));
				} else if (scroll_forward) {
					i = n;
					while (i--)
						putp(scroll_forward);
				}
			} else {
				mvcur(-1, -1, win->_regtop, 0);
				if (parm_delete_line) {
					putp(tparm(parm_delete_line, n));
				} else if (delete_line) {
					i = n;
					while (i--)
						putp(delete_line);
				}
				if (win->_regbottom < lines - 1) {
					mvcur(win->_regtop, 0, win->_regbottom, 0);
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

		mvcur(-1, -1, win->_cury, win->_curx);
		touched = 1;
	}

    saved = (chtype **)malloc(sizeof(chtype *) * abs(n));
    if (touched) {
	newsaved = (chtype **)malloc(sizeof(chtype *) * abs(n));
	cursaved = (chtype **)malloc(sizeof(chtype *) * abs(n));
    }

    if (n < 0) {
	/* save overwritten lines */

	for (i = 0; i < -n; i++) {
	    saved[i] = win->_line[win->_regbottom-i];
	    if (touched) {
		newsaved[i] = newscr->_line[win->_begy+win->_regbottom-i];
		cursaved[i] = curscr->_line[win->_begy+win->_regbottom-i];
	    }
	}

	/* shift n lines */

	for (line = win->_regbottom; line >= win->_regtop - n; line--) {
	    win->_line[line] = win->_line[line+n];
	    if (touched) {
		newscr->_line[win->_begy+line] = newscr->_line[win->_begy+line+n];
		curscr->_line[win->_begy+line] = curscr->_line[win->_begy+line+n];
	    }
	}

	/* restore saved lines and blank them */

	for (i = 0, line = win->_regtop; line < win->_regtop-n; line++, i++) {
	    win->_line[line] = saved[i];
	    if (touched) {
		newscr->_line[win->_begy+line] = newsaved[i];
		curscr->_line[win->_begy+line] = cursaved[i];
	    }
	    temp = win->_line[line];
	    for (ptr = temp; ptr - temp <= win->_maxx; ptr++)
		*ptr = blank;
	    if (touched) {
		temp = newscr->_line[win->_begy+line];
		for (ptr = temp; ptr - temp <= newscr->_maxx; ptr++)
		    *ptr = blank;
		temp = curscr->_line[win->_begy+line];
		for (ptr = temp; ptr - temp <= curscr->_maxx; ptr++)
		    *ptr = blank;
	    }
	}
    } else {
	/* save overwritten lines */

	for (i = 0; i < n; i++) {
	    saved[i] = win->_line[win->_regtop+i];
	    if (touched) {
		newsaved[i] = newscr->_line[win->_begy+win->_regtop+i];
		cursaved[i] = curscr->_line[win->_begy+win->_regtop+i];
	    }
	}

	/* shift n lines */

	for (line = win->_regtop; line <= win->_regbottom - n; line++) {
	    win->_line[line] = win->_line[line+n];
	    if (touched) {
		newscr->_line[win->_begy+line] = newscr->_line[win->_begy+line+n];
		curscr->_line[win->_begy+line] = curscr->_line[win->_begy+line+n];
	    }
	}

	/* restore saved lines and blank them */

	for (i = 0, line = win->_regbottom; line > win->_regbottom - n; line--, i++) {
	    win->_line[line] = saved[i];
	    if (touched) {
		newscr->_line[win->_begy+line] = newsaved[i];
		curscr->_line[win->_begy+line] = cursaved[i];
	    }
	    temp = win->_line[line];
	    for (ptr = temp; ptr - temp <= win->_maxx; ptr++)
		*ptr = blank;
	    if (touched) {
		temp = newscr->_line[win->_begy+line];
		for (ptr = temp; ptr - temp <= newscr->_maxx; ptr++)
		    *ptr = blank;
		temp = curscr->_line[win->_begy+line];
		for (ptr = temp; ptr - temp <= curscr->_maxx; ptr++)
		    *ptr = blank;
	    }
	}
    }
	
    free(saved);
    if (touched) {
	free(newsaved);
	free(cursaved);
    }

    if (!touched)
	touchline(win, win->_regtop, win->_regbottom - win->_regtop + 1);

    return OK;
}
