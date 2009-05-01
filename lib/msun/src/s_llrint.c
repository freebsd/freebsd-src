#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llrint.c,v 1.1.22.1 2009/04/15 03:14:26 kensmith Exp $");

#define type		double
#define	roundit		rint
#define dtype		long long
#define	fn		llrint

#include "s_lrint.c"
