/*
 * compar.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/06/04 11:36:24
 *
 */

#include "defs.h"

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo compar.c 3.3 92/06/04 public domain, By Ross Ridge";
#endif

/* compare two elements a sorted list of pointers to strings */
int
_compar(a, b)
#ifdef USE_ANSIC
void const *a;
void const *b; {
#else
anyptr a, b; {
#endif
	register char *aa = **(char ***)a;
	register char *bb = **(char ***)b;

	/* An optimization trick from C News, compare the first
	 * two chars of the string here to avoid a the overhead of a
	 * call to strcmp.
	 */

#ifdef __GNUC__
	return ((*aa - *bb) ? : strcmp(aa, bb));
#else
	if (*aa != *bb)
		return *aa - *bb;
	return strcmp(aa, bb);
#endif
}
