#include "f2c.h"

#ifdef KR_headers
double erf();
double erf_(x) real *x;
#else
extern double erf(double);
double erf_(real *x)
#endif
{
return( erf(*x) );
}
