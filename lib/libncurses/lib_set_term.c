
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_set_term.c
**
**	The routine set_term().
**
*/

#include "curses.priv.h"
#include <nterm.h>

struct screen *
set_term(screen)
struct screen *screen;
{
struct screen	*oldSP;

#ifdef TRACE
	if (_tracing)
	    _tracef("set_term(%o) called", screen);
#endif

	oldSP = SP;
	SP = screen;

	cur_term = SP->_term;
	curscr   = SP->_curscr;
	newscr   = SP->_newscr;

	return(oldSP);
}
