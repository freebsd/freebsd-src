/*
 * buftvtots - pull a Unix-format (struct timeval) time stamp out of
 *	       an octet stream and convert it to a l_fp time stamp.
 *	       This is useful when using the clock line discipline.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "ntp_fp.h"
#include "ntp_unixtime.h"

int
buftvtots(
	const char *bufp,
	l_fp *ts
	)
{
	register const u_char *bp;
	register u_long sec;
	register u_long usec;
	struct timeval tv;

#ifdef WORDS_BIGENDIAN
	bp = (const u_char *)bufp;

	sec = (u_long)*bp++ & 0xff;
	sec <<= 8;
	sec += (u_long)*bp++ & 0xff;
	sec <<= 8;
	sec += (u_long)*bp++ & 0xff;
	sec <<= 8;
	sec += (u_long)*bp++ & 0xff;

	if (sizeof(tv.tv_sec) == 8) {
		sec += (u_long)*bp++ & 0xff;
		sec <<= 8;
		sec += (u_long)*bp++ & 0xff;
		sec <<= 8;
		sec += (u_long)*bp++ & 0xff;
		sec <<= 8;
		sec += (u_long)*bp++ & 0xff;
	}

	usec = (u_long)*bp++ & 0xff;
	usec <<= 8;
	usec += (u_long)*bp++ & 0xff;
	usec <<= 8;
	usec += (u_long)*bp++ & 0xff;
	usec <<= 8;
	usec += (u_long)*bp++ & 0xff;

	if (sizeof(tv.tv_usec) == 8) {
		usec += (u_long)*bp++ & 0xff;
		usec <<= 8;
		usec += (u_long)*bp++ & 0xff;
		usec <<= 8;
		usec += (u_long)*bp++ & 0xff;
		usec <<= 8;
		usec += (u_long)*bp & 0xff;
	}
#else
	bp = (const u_char *)bufp + 7;

	usec = (u_long)*bp-- & 0xff;
	usec <<= 8;
	usec += (u_long)*bp-- & 0xff;
	usec <<= 8;
	usec += (u_long)*bp-- & 0xff;
	usec <<= 8;
	usec += (u_long)*bp-- & 0xff;

	if (sizeof(tv.tv_usec) == 8) {
		usec += (u_long)*bp-- & 0xff;
		usec <<= 8;
		usec += (u_long)*bp-- & 0xff;
		usec <<= 8;
		usec += (u_long)*bp-- & 0xff;
		usec <<= 8;
		usec += (u_long)*bp-- & 0xff;
	}

	sec = (u_long)*bp-- & 0xff;
	sec <<= 8;
	sec += (u_long)*bp-- & 0xff;
	sec <<= 8;
	sec += (u_long)*bp-- & 0xff;
	sec <<= 8;
	sec += (u_long)*bp-- & 0xff;

	if (sizeof (tv.tv_sec) == 8) {
		sec += (u_long)*bp-- & 0xff;
		sec <<= 8;
		sec += (u_long)*bp-- & 0xff;
		sec <<= 8;
		sec += (u_long)*bp-- & 0xff;
		sec <<= 8;
		sec += (u_long)*bp & 0xff;
	}
#endif
	ts->l_ui = sec + (u_long)JAN_1970;
	if (usec > 999999)
	    return 0;
	TVUTOTSF(usec, ts->l_uf);
	return 1;
}
