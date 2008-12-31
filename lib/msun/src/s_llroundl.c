#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llroundl.c,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $");

#define type		long double
#define	roundit		roundl
#define dtype		long long
#define	DTYPE_MIN	LLONG_MIN
#define	DTYPE_MAX	LLONG_MAX
#define	fn		llroundl

#include "s_lround.c"
