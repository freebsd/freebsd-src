#include "f2c.h"

#ifdef KR_headers
float log10f();
double r_lg10(x) real *x;
#else
#undef abs
#include "math.h"
double r_lg10(real *x)
#endif
{
return( log10f(*x) );
}
