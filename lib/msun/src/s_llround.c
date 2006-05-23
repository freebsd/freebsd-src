#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llround.c,v 1.1.2.1 2005/01/15 22:57:17 das Exp $");

#define type		double
#define	roundit		round
#define dtype		long long
#define	DTYPE_MIN	LLONG_MIN
#define	DTYPE_MAX	LLONG_MAX
#define	fn		llround

#include "s_lrint.c"
