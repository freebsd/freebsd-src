/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"

RCSID("$Id: strsave.c,v 1.1 1994/03/06 22:59:55 nate Exp $")

/* 
 *  makes a copy of a null terminated string in malloc'ed storage. Dies
 *  if enough memory isn't available or there is a malloc error
 */
char *
strsave(s)
const char *s;
{
	if (s)
		return(strcpy(emalloc((size_t) (strlen(s)+1)),s));
	else
		return((char *) NULL);
}
