#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llrint.c,v 1.1.24.1.4.1 2010/06/14 02:09:06 kensmith Exp $");

#define type		double
#define	roundit		rint
#define dtype		long long
#define	fn		llrint

#include "s_lrint.c"
