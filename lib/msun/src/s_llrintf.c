#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llrintf.c,v 1.1.2.1 2005/01/15 22:57:17 das Exp $");

#define type		float
#define	roundit		rintf
#define dtype		long long
#define	fn		llrintf

#include "s_lrint.c"
