#include "f2c.h"

#ifdef KR_headers
extern double log(), f__cabs(), atan2();
VOID c_log(r, z) complex *r, *z;
#else
#undef abs
#include "math.h"
extern double f__cabs(double, double);

void c_log(complex *r, complex *z)
#endif
{
r->i = atan2(z->i, z->r);
r->r = log( f__cabs(z->r, z->i) );
}
