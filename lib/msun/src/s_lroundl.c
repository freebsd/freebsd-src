#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_lroundl.c,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $");

#define type		long double
#define	roundit		roundl
#define dtype		long
#define	DTYPE_MIN	LONG_MIN
#define	DTYPE_MAX	LONG_MAX
#define	fn		lroundl

#include "s_lround.c"
