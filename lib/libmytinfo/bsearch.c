/*
 * bsearch.c
 *
 * This is something I found on watmath. I've made some minor changes for use
 * in this package.
 *
 * 92/06/04 11:35:15
 */

#if 0
#ifndef lint
static char *RCSid = "$OHeader: /usr/mfcf/src/accounts/libuw/RCS/bsearch.c,v 1.1 88/06/11 20:41:48 root Exp $";
#endif
#endif

#include "defs.h"

#ifdef USE_MYBSEARCH

#ifdef USE_SCCS_IDS
static char const SCCSid[] = "@(#) mytinfo bsearch.c 3.4 92/06/04 public domain, By Ross Ridge";
#endif

#ifdef USE_SHORT_BSEARCH
#define fast_int short
#else
#define fast_int mysize_t
#endif

/*
 * bsearch - find an element of a sorted vector
 *
 *	found = bsearch(key, array, dimension, width, compare)
 *		returns a pointer to the specified element in the array,
 *		or (char*)0 if the element can't be found.
 *	key
 *		pointer to the element to be searched for in the array
 *	array
 *		address of an array of elements
 *	dimension
 *		number of elements in the array
 *	width
 *		sizeof(type) of each element
 *	compare
 *		pointer to a function taking (char *) pointers to two elements
 *		and returning <0, 0, or >0 as the first element comes before,
 *		at, or after the second element.  A compare function is provided
 *		for comparing strings.
*/
#if 0
/*
 * $OLog:	bsearch.c,v $
 * Revision 1.1  88/06/11  20:41:48  root
 * Initial revision
 *
*/
#endif

	static anyptr
bsearch(key, array, dimension, iwidth, compare)
	anyptr key;
	anyptr array;
	int dimension;
	mysize_t iwidth;
	compar_fn compare;
{
	register fast_int start;   /* offset to start of current interval */
	register fast_int end;     /* offset to end+1 of current interval */
	register fast_int middle;  /* offset to middle of current interval */
	auto int status;
	register fast_int width;

	width = iwidth / sizeof(char);

	start = 0;
	middle = 0;
	end = dimension;

	while (start < end) {

		middle = (start + end) / 2;

		status = (*compare)(key, ((char *)array + middle*width));

		if (status < 0)
			end = middle;

		else if (status > 0)
			start = middle + 1;

		else return (anyptr)(((char *)array) + middle*width);
	}

	return  0;
}

#endif /* USE_MYBSEARCH */
