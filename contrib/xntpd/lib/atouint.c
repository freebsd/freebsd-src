/* atouint.c,v 3.1 1993/07/06 01:07:42 jbj Exp
 * atouint - convert an ascii string to an unsigned long, with error checking
 */
#include <sys/types.h>
#include <ctype.h>

#include "ntp_types.h"

int
atouint(str, uval)
	const char *str;
	U_LONG *uval;
{
	register U_LONG u;
	register const char *cp;

	cp = str;
	if (*cp == '\0')
		return 0;

	u = 0;
	while (*cp != '\0') {
		if (!isdigit(*cp))
			return 0;
		if (u > 429496729 || (u == 429496729 && *cp >= '6'))
			return 0;	/* overflow */
		u = (u << 3) + (u << 1);
		u += *cp++ - '0';	/* ascii dependent */
	}

	*uval = u;
	return 1;
}
