#include "f2c.h"

#ifdef KR_headers
double log(), f__cabs(), atan2();
VOID z_log(r, z) doublecomplex *r, *z;
#else
#undef abs
#include "math.h"
extern double f__cabs(double, double);
void z_log(doublecomplex *r, doublecomplex *z)
#endif
{

r->i = atan2(z->i, z->r);
r->r = log( f__cabs( z->r, z->i ) );
}
