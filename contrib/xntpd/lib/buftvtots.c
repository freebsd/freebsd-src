/* buftvtots.c,v 3.1 1993/07/06 01:07:59 jbj Exp
 * buftvtots - pull a Unix-format (struct timeval) time stamp out of
 *	       an octet stream and convert it to a l_fp time stamp.
 *	       This is useful when using the clock line discipline.
 */
#include "ntp_fp.h"
#include "ntp_unixtime.h"

int
buftvtots(bufp, ts)
	const char *bufp;
	l_fp *ts;
{
	register const u_char *bp;
	register U_LONG sec;
	register U_LONG usec;

#ifdef XNTP_BIG_ENDIAN
	bp = (u_char *)bufp;

	sec = (U_LONG)*bp++ & 0xff;
	sec <<= 8;
	sec += (U_LONG)*bp++ & 0xff;
	sec <<= 8;
	sec += (U_LONG)*bp++ & 0xff;
	sec <<= 8;
	sec += (U_LONG)*bp++ & 0xff;

	usec = (U_LONG)*bp++ & 0xff;
	usec <<= 8;
	usec += (U_LONG)*bp++ & 0xff;
	usec <<= 8;
	usec += (U_LONG)*bp++ & 0xff;
	usec <<= 8;
	usec += (U_LONG)*bp & 0xff;
#else
	bp = (u_char *)bufp + 7;

	usec = (U_LONG)*bp-- & 0xff;
	usec <<= 8;
	usec += (U_LONG)*bp-- & 0xff;
	usec <<= 8;
	usec += (U_LONG)*bp-- & 0xff;
	usec <<= 8;
	usec += (U_LONG)*bp-- & 0xff;

	sec = (U_LONG)*bp-- & 0xff;
	sec <<= 8;
	sec += (U_LONG)*bp-- & 0xff;
	sec <<= 8;
	sec += (U_LONG)*bp-- & 0xff;
	sec <<= 8;
	sec += (U_LONG)*bp & 0xff;
#endif
	if (usec > 999999)
		return 0;

	ts->l_ui = sec + (U_LONG)JAN_1970;
	TVUTOTSF(usec, ts->l_uf);
	return 1;
}
