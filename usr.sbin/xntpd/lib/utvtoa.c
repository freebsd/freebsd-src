/*
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

	(void) sprintf(buf, "%lu.%06lu", (u_long)tv->tv_sec,
	    (u_long)tv->tv_usec);
	return buf;
}
