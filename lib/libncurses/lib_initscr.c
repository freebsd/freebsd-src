
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

WINDOW *initscr()
{
static	bool initialized = FALSE;
char *name;
#ifdef TRACE
	_init_trace();

	T(("initscr() called"));
#endif

	/* Portable applications must not call initscr() more than once */
	if (!initialized) {
		initialized = TRUE;

		if ((name = getenv("TERM")) == 0)
			name = "unknown";
		if (newterm(name, stdout, stdin) == 0) {
			fprintf(stderr, "Error opening terminal: %s.\n", name);
			exit(1);
		}
		/* def_shell_mode - done in newterm */
		def_prog_mode();
	}
	return(stdscr);
}
