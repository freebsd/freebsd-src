/*
 * These are the wrappers around malloc for detailed tracing and leak
 * detection.  Allocation routines call RECORD_FILE_AND_LINE to record the
 * filename/line number keyed on the block address in the splay tree,
 * de-allocation functions call DELETE_RECORD to delete the specified block
 * address and its associated file/line from the splay tree.
 */
/*  Author: Mark Moraes <moraes@csri.toronto.edu> */

/*LINTLIBRARY*/

#include "defs.h"
#include "globals.h"
#include "trace.h"

RCSID("$Id: _malloc.c,v 1.1 1994/03/06 22:59:20 nate Exp $")

univptr_t
__malloc(nbytes, fname, linenum)
size_t nbytes;
const char *fname;
int linenum;
{
	univptr_t cp;
	
	PRTRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
	cp = malloc(nbytes);
	RECORD_FILE_AND_LINE(cp, fname, linenum);
	return(cp);
}

/* ARGSUSED if TRACE is not defined */
void
__free(cp, fname, linenum)
univptr_t cp;
const char *fname;
int linenum;
{
	PRTRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
	DELETE_RECORD(cp);
	free(cp);
}

univptr_t
__realloc(cp, nbytes, fname, linenum)
univptr_t cp;
size_t nbytes;
const char *fname;
int linenum;
{
	univptr_t old;
	
	PRTRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
	old = cp;
	cp = realloc(cp, nbytes);
	if (old != cp) {
		DELETE_RECORD(old);
		RECORD_FILE_AND_LINE(cp, fname, linenum);
	}
	return(cp);
}

univptr_t
__calloc(nelem, elsize, fname, linenum)
size_t nelem, elsize;
const char *fname;
int linenum;
{
	univptr_t cp;
	
	PRTRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
	cp = calloc(nelem, elsize);
	RECORD_FILE_AND_LINE(cp, fname, linenum);
	return(cp);
}

/* ARGSUSED if TRACE is not defined */
void
__cfree(cp, fname, linenum)
univptr_t cp;
const char *fname;
int linenum;
{
	PRTRACE(sprintf(_malloc_statsbuf, "%s:%d:", fname, linenum));
	DELETE_RECORD(cp);
	/* No point calling cfree() - it just calls free() */
	free(cp);
}
