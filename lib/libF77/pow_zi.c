#include "f2c.h"

#ifdef KR_headers
VOID pow_zi(p, a, b) 	/* p = a**b  */
 doublecomplex *p, *a; integer *b;
#else
extern void z_div(doublecomplex*, doublecomplex*, doublecomplex*);
void pow_zi(doublecomplex *p, doublecomplex *a, integer *b) 	/* p = a**b  */
#endif
{
integer n;
unsigned long u;
double t;
doublecomplex x;
static doublecomplex one = {1.0, 0.0};

n = *b;
p->r = 1;
p->i = 0;

if(n == 0)
	return;
if(n < 0)
	{
	n = -n;
	z_div(&x, &one, a);
	}
else
	{
	x.r = a->r;
	x.i = a->i;
	}

for(u = n; ; )
	{
	if(u & 01)
		{
		t = p->r * x.r - p->i * x.i;
		p->i = p->r * x.i + p->i * x.r;
		p->r = t;
		}
	if(u >>= 1)
		{
		t = x.r * x.r - x.i * x.i;
		x.i = 2 * x.r * x.i;
		x.r = t;
		}
	else
		break;
	}
}
