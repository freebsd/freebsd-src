
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_initscr.c
**
**	The routine initscr().
**
*/

#include <stdlib.h>
#include "curses.priv.h"

extern char _ncurses_copyright[];

WINDOW *initscr()
{
	char *use_it;
#ifdef TRACE
	_init_trace();

	if (_tracing)
	    _tracef("initscr() called");
#endif
	use_it = _ncurses_copyright;
  	if (newterm(getenv("TERM"), stdout, stdin) == NULL)
		return NULL;
	else {
		def_shell_mode();
		def_prog_mode();
		return(stdscr);
	}
}
