#include "f2c.h"

#ifdef KR_headers
extern void sig_die();
VOID z_div(c, a, b) doublecomplex *a, *b, *c;
#else
extern void sig_die(char*, int);
void z_div(doublecomplex *c, doublecomplex *a, doublecomplex *b)
#endif
{
	double ratio, den;
	double abr, abi;
	double ai = a->i, ar = a->r, bi = b->i, br = b->r;

	if( (abr = br) < 0.)
		abr = - abr;
	if( (abi = bi) < 0.)
		abi = - abi;
	if( abr <= abi )
		{
		if(abi == 0)
			sig_die("complex division by zero", 1);
		ratio = br / bi ;
		den = bi * (1 + ratio*ratio);
		c->r = (ar*ratio + ai) / den;
		c->i = (ai*ratio - ar) / den;
		}

	else
		{
		ratio = bi / br ;
		den = br * (1 + ratio*ratio);
		c->r = (ar + ai*ratio) / den;
		c->i = (ai - ar*ratio) / den;
		}
	}
