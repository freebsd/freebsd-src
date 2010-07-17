#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llround.c,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $");

#define type		double
#define	roundit		round
#define dtype		long long
#define	DTYPE_MIN	LLONG_MIN
#define	DTYPE_MAX	LLONG_MAX
#define	fn		llround

#include "s_lround.c"
