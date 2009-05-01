#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_lrintf.c,v 1.1.22.1 2009/04/15 03:14:26 kensmith Exp $");

#define type		float
#define	roundit		rintf
#define dtype		long
#define	fn		lrintf

#include "s_lrint.c"
