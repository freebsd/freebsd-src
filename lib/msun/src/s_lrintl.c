#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/msun/src/s_lrintl.c,v 1.1.2.1.4.1 2010/06/14 02:09:06 kensmith Exp $");

#define type		long double
#define	roundit		rintl
#define dtype		long
#define	fn		lrintl

#include "s_lrint.c"
