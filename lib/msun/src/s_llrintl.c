#define type		long double
#define	roundit		rintl
#define dtype		long long
#define	fn		llrintl

#include <float.h>
#include "s_lrint.c"

#if LDBL_MANT_DIG == 113
__weak_reference(llrintl, llrintf128);
#endif
