#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_lroundl.c,v 1.1.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $");

#define type		long double
#define	roundit		roundl
#define dtype		long
#define	DTYPE_MIN	LONG_MIN
#define	DTYPE_MAX	LONG_MAX
#define	fn		lroundl

#include "s_lround.c"
