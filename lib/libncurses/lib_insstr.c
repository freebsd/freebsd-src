
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_insstr.c
**
**	The routine winsnstr().
**
*/

#include "curses.priv.h"

int winsnstr(WINDOW *win, char *str, int n)
{

	T(("winsstr(%x,'%x',%d) called", win, str, n));

	return OK;
}
