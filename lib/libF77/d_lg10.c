#include "f2c.h"

#ifdef KR_headers
double log10();
double d_lg10(x) doublereal *x;
#else
#undef abs
#include "math.h"
double d_lg10(doublereal *x)
#endif
{
return( log10(*x) );
}
