
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_insdel.c
**
**	The routine winsdel(win, n).
**  positive n insert n lines above current line
**  negative n delete n lines starting from current line 
**
*/

#include <stdlib.h>
#include "curses.priv.h"
#include <nterm.h>

int
winsdel(WINDOW *win, int n)
{
int	line, i;
int touched = 0;
chtype	*ptr, *temp;
chtype  **saved;
chtype	blank = ' ';

	T(("winsdel(%x,%d) called", win, n));

	if (n == 0)
		return OK;
	if (n < 0 && win->_cury - n >= win->_maxy)
		/* request to delete too many lines */
		/* should we truncate to an appropriate number? */
		return ERR;


    saved = (chtype **)malloc(sizeof(chtype *) * abs(n));

    if (n < 0) {
		/* save overwritten lines */
		
		for (i = 0; i < -n; i++)
		    saved[i] = win->_line[win->_regbottom-i];

		/* delete n lines */
		
		for (line = win->_regbottom; line >= win->_cury; line--)
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

		/* insert n lines */
		
		for (line = win->_regtop; line < win->_regtop + n; line++)
		    win->_line[line] = win->_line[line+n];

		/* restore saved lines and blank them */

		for (i = 0, line = win->_regtop + n; i < n; line--, i++) {
		    temp = win->_line[line] = saved[i];
		    for (ptr = temp; ptr - temp <= win->_maxx; ptr++)
			*ptr = blank;
		}
	}
	
	free(saved);

	/* as an optimization, if the window is COLS wide we can try
	   using idl assuming _idlok is true */

    if (win->_maxx == columns && win->_idlok == TRUE) {

		wrefresh(win);
		if (back_color_erase) {
			T(("back_color_erase, turning attributes off"));
			vidattr(curscr->_attrs = A_NORMAL);
		}
		if (n > 0) {
			mvcur(-1, -1, win->_cury, 0);
			if (parm_insert_line) {
				putp(tparm(parm_insert_line, n));
				touched = 1;
			} else if (insert_line) {
				while (n--)
					putp(insert_line);
				touched = 1;
			}
		}

		if (n < 0) {
			mvcur(-1, -1, win->_cury, 0);
			n = abs(n);
			if (parm_delete_line) {
				putp(tparm(parm_delete_line, n));
				touched = 1;
		    } else if (delete_line) {
		    	while (n--)
					putp(delete_line);
				touched = 1;
			}
		}

		mvcur(-1, -1, win->_cury, win->_curx);
	} 
	if (touched == 0) 
	    touchline(win, win->_regtop, win->_regbottom - win->_regtop + 1);
	touched = 0;

    return OK;
}
