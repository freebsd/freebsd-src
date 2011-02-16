#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llrintl.c,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $");

#define type		long double
#define	roundit		rintl
#define dtype		long long
#define	fn		llrintl

#include "s_lrint.c"
