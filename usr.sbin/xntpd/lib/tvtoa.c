/* tvtoa.c,v 3.1 1993/07/06 01:08:50 jbj Exp
 * tvtoa - return an asciized representation of a struct timeval
 */
#include <stdio.h>
#include <sys/time.h>

#include "lib_strbuf.h"
#include "ntp_stdlib.h"

char *
tvtoa(tv)
	struct timeval *tv;
{
	register char *buf;
	register U_LONG sec;
	register U_LONG usec;
	register int isneg;

	if (tv->tv_sec < 0 || tv->tv_usec < 0) {
		sec = -tv->tv_sec;
		usec = -tv->tv_usec;
		isneg = 1;
	} else {
		sec = tv->tv_sec;
		usec = tv->tv_usec;
		isneg = 0;
	}

	LIB_GETBUF(buf);

	(void) sprintf(buf, "%s%lu.%06lu", (isneg?"-":""), sec, usec);
	return buf;
}
