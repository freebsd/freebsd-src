/* alloc.c - version 1.0.2 */
/* $FreeBSD: src/games/hack/alloc.c,v 1.4 1999/11/16 02:57:01 billf Exp $ */

#include <stdlib.h>

#ifdef LINT

/*
   a ridiculous definition, suppressing
	"possible pointer alignment problem" for (long *) malloc()
	"enlarg defined but never used"
	"ftell defined (in <stdio.h>) but never used"
   from lint
*/
#include <stdio.h>
long *
alloc(n) unsigned n; {
long dummy = ftell(stderr);
	if(n) dummy = 0;	/* make sure arg is used */
	return(&dummy);
}

#else

long *
alloc(lth)
unsigned lth;
{
	char *ptr;

	if(!(ptr = malloc(lth)))
		panic("Cannot get %d bytes", lth);
	return((long *) ptr);
}

long *
enlarge(ptr,lth)
char *ptr;
unsigned lth;
{
	char *nptr;

	if(!(nptr = realloc(ptr,lth)))
		panic("Cannot reallocate %d bytes", lth);
	return((long *) nptr);
}

#endif LINT
