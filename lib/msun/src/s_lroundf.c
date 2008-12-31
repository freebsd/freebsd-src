#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_lroundf.c,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $");

#define type		float
#define	roundit		roundf
#define dtype		long
#define	DTYPE_MIN	LONG_MIN
#define	DTYPE_MAX	LONG_MAX
#define	fn		lroundf

#include "s_lround.c"
