/* uinttoa.c,v 3.1 1993/07/06 01:08:54 jbj Exp
 * uinttoa - return an asciized unsigned integer
 */
#include <stdio.h>

#include "lib_strbuf.h"
#include "ntp_stdlib.h"

char *
uinttoa(uval)
	U_LONG uval;
{
	register char *buf;

	LIB_GETBUF(buf);

	(void) sprintf(buf, "%lu", uval);
	return buf;
}
