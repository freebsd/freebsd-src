#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llroundf.c,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $");

#define type		float
#define	roundit		roundf
#define dtype		long long
#define	DTYPE_MIN	LLONG_MIN
#define	DTYPE_MAX	LLONG_MAX
#define	fn		llroundf

#include "s_lround.c"
