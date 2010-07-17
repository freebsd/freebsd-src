#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_lroundf.c,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $");

#define type		float
#define	roundit		roundf
#define dtype		long
#define	DTYPE_MIN	LONG_MIN
#define	DTYPE_MAX	LONG_MAX
#define	fn		lroundf

#include "s_lround.c"
