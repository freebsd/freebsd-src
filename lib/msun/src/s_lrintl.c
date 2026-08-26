#define type		long double
#define	roundit		rintl
#define dtype		long
#define	fn		lrintl

#include <float.h>
#include "s_lrint.c"

#if LDBL_MANT_DIG == 113
__weak_reference(lrintl, lrintf128);
#endif
