
/* This work is copyrighted. See COPYRIGHT.OLD & COPYRIGHT.NEW for   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/*
**	lib_longname.c
**
**	The routine longname().
**
*/

#include <string.h>
#include "curses.priv.h"
#include "terminfo.h"

char *
longname()
{
#ifndef MYTINFO
char	*ptr;
#endif

	T(("longname() called"));

#ifdef MYTINFO
	return cur_term->name_long;
#else
	for (ptr = ttytype + strlen(ttytype); ptr > ttytype; ptr--)
	   	if (*ptr == '|')
			return(ptr + 1);

    return(ttytype);
#endif
}
