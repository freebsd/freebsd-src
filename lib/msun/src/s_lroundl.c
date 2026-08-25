#define type		long double
#define	roundit		roundl
#define dtype		long
#define	DTYPE_MIN	LONG_MIN
#define	DTYPE_MAX	LONG_MAX
#define	fn		lroundl

#include <float.h>
#include "s_lround.c"

#if LDBL_MANT_DIG == 113
__weak_reference(lroundl, lroundf128);
#endif
