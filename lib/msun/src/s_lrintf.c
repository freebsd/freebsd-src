#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_lrintf.c,v 1.1.24.1.4.1 2010/06/14 02:09:06 kensmith Exp $");

#define type		float
#define	roundit		rintf
#define dtype		long
#define	fn		lrintf

#include "s_lrint.c"
