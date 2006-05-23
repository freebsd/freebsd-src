#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_lrintf.c,v 1.1.2.1 2005/01/15 22:57:17 das Exp $");

#define type		float
#define	roundit		rintf
#define dtype		long
#define	fn		lrintf

#include "s_lrint.c"
