#include "f2c.h"

#ifdef KR_headers
double sin(), cos(), sinh(), cosh();
VOID z_cos(r, z) doublecomplex *r, *z;
#else
#undef abs
#include "math.h"
void z_cos(doublecomplex *r, doublecomplex *z)
#endif
{
r->r = cos(z->r) * cosh(z->i);
r->i = - sin(z->r) * sinh(z->i);
}
