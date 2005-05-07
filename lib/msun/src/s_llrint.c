#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llrint.c,v 1.1.2.1 2005/01/15 22:57:17 das Exp $");

#define type		double
#define	roundit		rint
#define dtype		long long
#define	fn		llrint

#include "s_lrint.c"
