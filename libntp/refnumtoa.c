/*
 * refnumtoa - return asciized refclock addresses stored in local array space
 */
#include <stdio.h>

#include "ntp_net.h"
#include "lib_strbuf.h"
#include "ntp_stdlib.h"

char *
refnumtoa(
	sockaddr_u *num
	)
{
	register u_int32 netnum;
	register char *buf;
	register const char *rclock;

	LIB_GETBUF(buf);

	if (ISREFCLOCKADR(num)) {
		netnum = SRCADR(num);
		rclock = clockname((int)((u_long)netnum >> 8) & 0xff);

		if (rclock != NULL)
			snprintf(buf, LIB_BUFLENGTH, "%s(%lu)",
				 rclock, (u_long)netnum & 0xff);
		else
			snprintf(buf, LIB_BUFLENGTH, "REFCLK(%lu,%lu)",
				 ((u_long)netnum >> 8) & 0xff,
				 (u_long)netnum & 0xff);

	}

	return buf;
}
