#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llroundf.c,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $");

#define type		float
#define	roundit		roundf
#define dtype		long long
#define	DTYPE_MIN	LLONG_MIN
#define	DTYPE_MAX	LLONG_MAX
#define	fn		llroundf

#include "s_lround.c"
