/* octtoint.c,v 3.1 1993/07/06 01:08:41 jbj Exp
 * octtoint - convert an ascii string in octal to an unsigned
 *	      long, with error checking
 */
#include <stdio.h>
#include <ctype.h>

#include "ntp_stdlib.h"

int
octtoint(str, ival)
	const char *str;
	U_LONG *ival;
{
	register U_LONG u;
	register const char *cp;

	cp = str;

	if (*cp == '\0')
		return 0;

	u = 0;
	while (*cp != '\0') {
		if (!isdigit(*cp) || *cp == '8' || *cp == '9')
			return 0;
		if (u >= 0x20000000)
			return 0;	/* overflow */
		u <<= 3;
		u += *cp++ - '0';	/* ascii dependent */
	}
	*ival = u;
	return 1;
}
