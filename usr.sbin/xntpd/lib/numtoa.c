/*
 * numtoa - return asciized network numbers store in local array space
 */
#include <stdio.h>

#include "ntp_fp.h"
#include "lib_strbuf.h"
#include "ntp_stdlib.h"

char *
numtoa(num)
	u_int32_t num;
{
	register u_int32_t netnum;
	register char *buf;

	netnum = ntohl(num);
	LIB_GETBUF(buf);
	(void) sprintf(buf, "%lu.%lu.%lu.%lu", (netnum >> 24) & 0xff,
	    (netnum >> 16) & 0xff, (netnum >> 8) & 0xff, netnum & 0xff);
	return buf;
}
