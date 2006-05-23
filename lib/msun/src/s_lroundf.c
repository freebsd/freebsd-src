#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_lroundf.c,v 1.1.2.1 2005/01/15 22:57:17 das Exp $");

#define type		float
#define	roundit		roundf
#define dtype		long
#define	DTYPE_MIN	LONG_MIN
#define	DTYPE_MAX	LONG_MAX
#define	fn		lroundf

#include "s_lrint.c"
