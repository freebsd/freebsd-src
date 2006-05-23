#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llroundf.c,v 1.1.2.1 2005/01/15 22:57:17 das Exp $");

#define type		float
#define	roundit		roundf
#define dtype		long long
#define	DTYPE_MIN	LLONG_MIN
#define	DTYPE_MAX	LLONG_MAX
#define	fn		llroundf

#include "s_lrint.c"
