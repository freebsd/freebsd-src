#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_llrintl.c,v 1.1.2.1.2.1 2009/10/25 01:10:29 kensmith Exp $");

#define type		long double
#define	roundit		rintl
#define dtype		long long
#define	fn		llrintl

#include "s_lrint.c"
