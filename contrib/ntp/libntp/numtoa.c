/*
 * numtoa - return asciized network numbers store in local array space
 */
#include <stdio.h>

#include "ntp_fp.h"
#include "lib_strbuf.h"
#include "ntp_stdlib.h"

char *
numtoa(
	u_int32 num
	)
{
	register u_int32 netnum;
	register char *buf;

	netnum = ntohl(num);
	LIB_GETBUF(buf);
	(void) sprintf(buf, "%lu.%lu.%lu.%lu", ((u_long)netnum >> 24) & 0xff,
		       ((u_long)netnum >> 16) & 0xff, ((u_long)netnum >> 8) & 0xff,
		       (u_long)netnum & 0xff);
	return buf;
}
