#include "f2c.h"

/* compare two strings */

#ifdef KR_headers
integer s_cmp(a, b, la, lb) register char *a, *b; ftnlen la, lb;
#else
integer s_cmp(register char *a, register char *b, ftnlen la, ftnlen lb)
#endif
{
register char *aend, *bend;
aend = a + la;
bend = b + lb;

if(la <= lb)
	{
	while(a < aend)
		if(*a != *b)
			return( *a - *b );
		else
			{ ++a; ++b; }

	while(b < bend)
		if(*b != ' ')
			return( ' ' - *b );
		else	++b;
	}

else
	{
	while(b < bend)
		if(*a == *b)
			{ ++a; ++b; }
		else
			return( *a - *b );
	while(a < aend)
		if(*a != ' ')
			return(*a - ' ');
		else	++a;
	}
return(0);
}
