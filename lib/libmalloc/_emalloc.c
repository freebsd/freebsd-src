/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"
#include "globals.h"
#include "trace.h"

RCSID("$Id: _emalloc.c,v 1.1 1994/03/06 22:59:19 nate Exp $")

univptr_t
__emalloc(nbytes, fname, linenum)
size_t nbytes;
const char *fname;
int linenum;
{
	univptr_t cp;
	
	PRTRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
	cp = emalloc(nbytes);
	RECORD_FILE_AND_LINE(cp, fname, linenum);
	return(cp);
}


univptr_t
__erealloc(ptr, nbytes, fname, linenum)
univptr_t ptr;
size_t nbytes;
const char *fname;
int linenum;
{
	univptr_t cp;
	
	PRTRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
	cp = erealloc(ptr, nbytes);
	RECORD_FILE_AND_LINE(cp, fname, linenum);
	return(cp);
}

univptr_t
__ecalloc(nelem, sz, fname, linenum)
size_t nelem, sz;
const char *fname;
int linenum;
{
	univptr_t cp;
	
	PRTRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
	cp = ecalloc(nelem, sz);
	RECORD_FILE_AND_LINE(cp, fname, linenum);
	return(cp);
}


