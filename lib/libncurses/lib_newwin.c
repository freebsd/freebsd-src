
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_newwin.c
**
**	The routines newwin(), subwin() and their dependent
**
*/

#include <stdlib.h>
#include "terminfo.h"
#include "curses.priv.h"

WINDOW * newwin(int num_lines, int num_columns, int begy, int begx)
{
WINDOW	*win;
chtype	*ptr;
int	i, j;

	T(("newwin(%d,%d,%d,%d) called", num_lines, num_columns, begy, begx));

	if (num_lines == 0)
	    num_lines = lines - begy;

	if (num_columns == 0)
	    num_columns = columns - begx;

	if (num_columns + begx > columns || num_lines + begy > lines)
		return NULL;

	if ((win = makenew(num_lines, num_columns, begy, begx)) == NULL)
		return NULL;

	for (i = 0; i < num_lines; i++) {
	    if ((win->_line[i] = (chtype *) calloc(num_columns, sizeof(chtype))) == NULL) {
			for (j = 0; j < i; j++)
			    free(win->_line[j]);

			free(win->_firstchar);
			free(win->_lastchar);
			free(win->_line);
			free(win);

			return NULL;
	    }
	    else
		for (ptr = win->_line[i]; ptr < win->_line[i] + num_columns; )
		    *ptr++ = ' ';
	}

	T(("newwin: returned window is %x", win));

	return(win);
}

WINDOW * derwin(WINDOW *orig, int num_lines, int num_columns, int begy, int begx)
{
WINDOW	*win;
int	i;

	T(("derwin(%x, %d,%d,%d,%d) called", orig, num_lines, num_columns, begy, begx));

	/*
	** make sure window fits inside the original one
	*/
	if ( begy < 0 || begx < 0)
		return NULL;
	if ( begy + num_lines > orig->_maxy + 1
		|| begx + num_columns > orig->_maxx + 1)
	    return NULL;

	if (num_lines == 0)
	    num_lines = orig->_maxy - orig->_begy - begy;

	if (num_columns == 0)
	    num_columns = orig->_maxx - orig->_begx - begx;

	if ((win = makenew(num_lines, num_columns, orig->_begy + begy, orig->_begx + begx)) == NULL)
	    return NULL;

	win->_pary = begy;
	win->_parx = begx;
	win->_attrs = orig->_attrs;
	win->_bkgd = orig->_bkgd;

	for (i = 0; i < num_lines; i++)
	    win->_line[i] = &orig->_line[begy++][begx];

	win->_flags = _SUBWIN;
	win->_parent = orig;

	T(("derwin: returned window is %x", win));

	return(win);
}


WINDOW *subwin(WINDOW *w, int l, int c, int y, int x)
{
	T(("subwin(%x, %d, %d, %d, %d) called", w, l, c, y, x));
	T(("parent has begy = %d, begx = %d", w->_begy, w->_begx));

	return derwin(w, l, c, y - w->_begy, x - w->_begx);
}

WINDOW *
makenew(int num_lines, int num_columns, int begy, int begx)
{
int	i;
WINDOW	*win;

	T(("makenew(%d,%d,%d,%d)", num_lines, num_columns, begy, begx));

	if ((win = (WINDOW *) malloc(sizeof(WINDOW))) == NULL)
		return NULL;

	if ((win->_line = (chtype **) calloc(num_lines, sizeof (chtype *))) == NULL) {
		free(win);
		return NULL;
	}

	if ((win->_firstchar = calloc(num_lines, sizeof(short))) == NULL) {
		free(win);
		free(win->_line);
		return NULL;
	}

	if ((win->_lastchar = calloc(num_lines, sizeof(short))) == NULL) {
		free(win);
		free(win->_line);
		free(win->_firstchar);
		return NULL;
	}

	win->_curx       = 0;
	win->_cury       = 0;
	win->_maxy       = num_lines - 1;
	win->_maxx       = num_columns - 1;
	win->_begy       = begy;
	win->_begx       = begx;

	win->_flags      = 0;
	win->_attrs      = A_NORMAL;
	win->_bkgd       = BLANK;

	win->_clear      = (num_lines == lines  &&  num_columns == columns);
	win->_idlok      = FALSE;
	win->_use_idc      = TRUE;
	win->_scroll     = FALSE;
	win->_leave      = FALSE;
	win->_use_keypad = FALSE;
#ifdef TERMIOS
	win->_use_meta   = ((cur_term->Ottyb.c_cflag & CSIZE) == CS8 &&
			    !(cur_term->Ottyb.c_iflag & ISTRIP));
#else
	win->_use_meta   = FALSE;
#endif
	win->_delay    	 = -1;
	win->_immed	 = FALSE;
	win->_sync	 = 0;
	win->_parx	 = 0;
	win->_pary	 = 0;
	win->_parent	 = (WINDOW *)NULL;

	win->_regtop     = 0;
	win->_regbottom  = num_lines - 1;

	for (i = 0; i < num_lines; i++)
	    win->_firstchar[i] = win->_lastchar[i] = _NOCHANGE;

	if (begx + num_columns == columns) {
		win->_flags |= _ENDLINE;

		if (begx == 0  &&  num_lines == lines  &&  begy == 0)
			win->_flags |= _FULLWIN;

		if (begy + num_lines == lines)
			win->_flags |= _SCROLLWIN;
	}

	return(win);
}
