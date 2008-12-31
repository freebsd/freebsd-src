#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llrint.c,v 1.1.20.1 2008/11/25 02:59:29 kensmith Exp $");

#define type		double
#define	roundit		rint
#define dtype		long long
#define	fn		llrint

#include "s_lrint.c"
