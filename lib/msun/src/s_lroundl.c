#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_lroundl.c,v 1.1.20.1 2009/04/15 03:14:26 kensmith Exp $");

#define type		long double
#define	roundit		roundl
#define dtype		long
#define	DTYPE_MIN	LONG_MIN
#define	DTYPE_MAX	LONG_MAX
#define	fn		lroundl

#include "s_lround.c"
