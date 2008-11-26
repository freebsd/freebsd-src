#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llrint.c,v 1.1.18.1 2008/10/02 02:57:24 kensmith Exp $");

#define type		double
#define	roundit		rint
#define dtype		long long
#define	fn		llrint

#include "s_lrint.c"
