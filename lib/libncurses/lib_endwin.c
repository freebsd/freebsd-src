
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_endwin.c
**
**	The routine endwin().
**
*/

#include <nterm.h>
#include "curses.priv.h"

int _isendwin;

int isendwin()
{
	return _isendwin;
}

int
endwin()
{
	T(("endwin() called"));

	_isendwin = 1;

	mvcur(-1, -1, lines - 1, 0);

	if (exit_ca_mode)
	    tputs(exit_ca_mode, 1, _outc);

	if (_coloron == 1)
		tputs(orig_pair, 1, _outc);

	if (curscr  &&  (curscr->_attrs != A_NORMAL)) 
	    vidattr(curscr->_attrs = A_NORMAL);

	fflush(SP->_ofp);

	return(reset_shell_mode());
}
