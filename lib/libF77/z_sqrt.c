#include "f2c.h"

#ifdef KR_headers
double sqrt(), f__cabs();
VOID z_sqrt(r, z) doublecomplex *r, *z;
#else
#undef abs
#include "math.h"
extern double f__cabs(double, double);
void z_sqrt(doublecomplex *r, doublecomplex *z)
#endif
{
double mag;

if( (mag = f__cabs(z->r, z->i)) == 0.)
	r->r = r->i = 0.;
else if(z->r > 0)
	{
	r->r = sqrt(0.5 * (mag + z->r) );
	r->i = z->i / r->r / 2;
	}
else
	{
	r->i = sqrt(0.5 * (mag - z->r) );
	if(z->i < 0)
		r->i = - r->i;
	r->r = z->i / r->i / 2;
	}
}
