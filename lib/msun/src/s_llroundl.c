#define type		long double
#define	roundit		roundl
#define dtype		long long
#define	DTYPE_MIN	LLONG_MIN
#define	DTYPE_MAX	LLONG_MAX
#define	fn		llroundl

#include <float.h>
#include "s_lround.c"

#if LDBL_MANT_DIG == 113
__weak_reference(llroundl, llroundf128);
#endif
