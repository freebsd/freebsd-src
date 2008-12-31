#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_lrintf.c,v 1.1.20.1 2008/11/25 02:59:29 kensmith Exp $");

#define type		float
#define	roundit		rintf
#define dtype		long
#define	fn		lrintf

#include "s_lrint.c"
