
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_endwin.c
**
**	The routine endwin().
**
*/

#include "terminfo.h"
#include "curses.priv.h"

int isendwin()
{
	if (SP == NULL)
		return FALSE;
	return SP->_endwin;
}

int
endwin()
{
	T(("endwin() called"));

	SP->_endwin = TRUE;

	if (change_scroll_region)
		putp(tparm(change_scroll_region, 0, lines - 1));

	mvcur(-1, -1, lines - 1, 0);

	if (exit_ca_mode)
	    putp(exit_ca_mode);

	if (SP->_coloron == TRUE)
		putp(orig_pair);

	if (curscr  &&  (curscr->_attrs != A_NORMAL)) 
	    vidattr(curscr->_attrs = A_NORMAL);

	if (SP->_cursor != 1)
	    putp(cursor_normal);

	fflush(SP->_ofp);

	return(reset_shell_mode());
}
