#include "f2c.h"

#ifdef KR_headers
extern double sqrt(), f__cabs();

VOID c_sqrt(r, z) complex *r, *z;
#else
#undef abs
#include "math.h"
extern double f__cabs(double, double);

void c_sqrt(complex *r, complex *z)
#endif
{
double mag, t;

if( (mag = f__cabs(z->r, z->i)) == 0.)
	r->r = r->i = 0.;
else if(z->r > 0)
	{
	r->r = t = sqrt(0.5 * (mag + z->r) );
	t = z->i / t;
	r->i = 0.5 * t;
	}
else
	{
	t = sqrt(0.5 * (mag - z->r) );
	if(z->i < 0)
		t = -t;
	r->i = t;
	t = z->i / t;
	r->r = 0.5 * t;
	}
}
