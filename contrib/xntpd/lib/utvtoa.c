/* utvtoa.c,v 3.1 1993/07/06 01:08:55 jbj Exp
 * utvtoa - return an asciized representation of an unsigned struct timeval
 */
#include <stdio.h>
#include <sys/time.h>

#include "lib_strbuf.h"
#include "ntp_stdlib.h"

char *
utvtoa(tv)
	struct timeval *tv;
{
	register char *buf;

	LIB_GETBUF(buf);
	
	(void) sprintf(buf, "%lu.%06lu", (U_LONG)tv->tv_sec,
	    (U_LONG)tv->tv_usec);
	return buf;
}
