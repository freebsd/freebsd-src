/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"
#include "globals.h"
#include "trace.h"

RCSID("$Id: _strdup.c,v 1.1 1994/03/06 22:59:21 nate Exp $")

char *
__strdup(s, fname, linenum)
char *s;
const char *fname;
int linenum;
{
	char *cp;
	
	PRTRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
	cp = strdup(s);
	RECORD_FILE_AND_LINE((univptr_t) cp, fname, linenum);
	return(cp);
}
