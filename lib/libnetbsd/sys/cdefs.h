/* $FreeBSD$ */

#include_next <sys/cdefs.h>

#ifdef __dead2
#define __dead __dead2
#else
#define __dead
#endif

/*
 * Return the number of elements in a statically-allocated array,
 * __x.
 */
#define	__arraycount(__x)	(sizeof(__x) / sizeof(__x[0]))

