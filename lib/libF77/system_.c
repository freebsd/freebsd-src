/* f77 interface to system routine */

#include "f2c.h"

#ifdef KR_headers
system_(s, n) register char *s; ftnlen n;
#else
#undef abs
#include "stdlib.h"
system_(register char *s, ftnlen n)
#endif
{
char buff[1000];
register char *bp, *blast;

blast = buff + (n < 1000 ? n : 1000);

for(bp = buff ; bp<blast && *s!='\0' ; )
	*bp++ = *s++;
*bp = '\0';
return system(buff);
}
