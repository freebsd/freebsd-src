/*
 * hextoint - convert an ascii string in hex to an unsigned
 *	      long, with error checking
 */
#include <ctype.h>

#include "ntp_stdlib.h"

int
hextoint(
	const char *str,
	u_long *ival
	)
{
	register u_long u;
	register const char *cp;

	cp = str;

	if (*cp == '\0')
	    return 0;

	u = 0;
	while (*cp != '\0') {
		if (!isxdigit((int)*cp))
		    return 0;
		if (u >= 0x10000000)
		    return 0;	/* overflow */
		u <<= 4;
		if (*cp <= '9')		/* very ascii dependent */
		    u += *cp++ - '0';
		else if (*cp >= 'a')
		    u += *cp++ - 'a' + 10;
		else
		    u += *cp++ - 'A' + 10;
	}
	*ival = u;
	return 1;
}
