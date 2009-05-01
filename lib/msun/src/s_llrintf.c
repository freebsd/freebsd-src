#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llrintf.c,v 1.1.22.1 2009/04/15 03:14:26 kensmith Exp $");

#define type		float
#define	roundit		rintf
#define dtype		long long
#define	fn		llrintf

#include "s_lrint.c"
