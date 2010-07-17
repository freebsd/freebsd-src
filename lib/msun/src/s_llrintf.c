#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llrintf.c,v 1.1.24.1.4.1 2010/06/14 02:09:06 kensmith Exp $");

#define type		float
#define	roundit		rintf
#define dtype		long long
#define	fn		llrintf

#include "s_lrint.c"
