
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_window.c
**
**
*/

#include <string.h>
#include "curses.priv.h"

int mvder(WINDOW *win, int y, int x)
{
    return(ERR);
}

void wsyncup(WINDOW *win)
{

}

int syncok(WINDOW *win, bool bf)
{
    return(ERR);
}

void wcursyncup(WINDOW *win)
{

}

void wsyncdown(WINDOW *win)
{

}

WINDOW *dupwin(WINDOW *win)
{
WINDOW *nwin;
int linesize, i;

	T(("dupwin(%x) called", win));

	if ((nwin = newwin(win->_maxy + 1, win->_maxx + 1, win->_begy, win->_begx)) == NULL)
		return NULL;

	nwin->_curx       = win->_curx;
	nwin->_cury       = win->_cury;
	nwin->_maxy       = win->_maxy;
	nwin->_maxx       = win->_maxx;       
	nwin->_begy       = win->_begy;
	nwin->_begx       = win->_begx;

	nwin->_flags      = win->_flags;
	nwin->_attrs      = win->_attrs;
	nwin->_bkgd	  = win->_bkgd; 

	nwin->_clear      = win->_clear; 
	nwin->_scroll     = win->_scroll;
	nwin->_leave      = win->_leave;
	nwin->_use_keypad = win->_use_keypad;
	nwin->_use_meta   = win->_use_meta;
	nwin->_delay   	  = win->_delay;
	nwin->_immed	  = win->_immed;
	nwin->_sync	  = win->_sync;
	nwin->_parx	  = win->_parx;
	nwin->_pary	  = win->_pary;
	nwin->_parent	  = win->_parent; 

	nwin->_regtop     = win->_regtop;
	nwin->_regbottom  = win->_regbottom;

	linesize = (win->_maxx + 1) * sizeof(chtype);
	for (i = 0; i <= nwin->_maxy; i++) {
		memcpy(nwin->_line[i], win->_line[i], linesize);
		nwin->_firstchar[i]  = win->_firstchar[i];
		nwin->_lastchar[i] = win->_lastchar[i];
	}

	return nwin;
}

