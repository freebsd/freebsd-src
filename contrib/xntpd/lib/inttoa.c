/* inttoa.c,v 3.1 1993/07/06 01:08:25 jbj Exp
 * inttoa - return an asciized signed integer
 */
#include <stdio.h>

#include "lib_strbuf.h"
#include "ntp_stdlib.h"

char *
inttoa(ival)
	LONG ival;
{
	register char *buf;

	LIB_GETBUF(buf);

	(void) sprintf(buf, "%ld", ival);
	return buf;
}
