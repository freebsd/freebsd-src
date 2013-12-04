/*
 * buftvtots - pull a Unix-format (struct timeval) time stamp out of
 *	       an octet stream and convert it to a l_fp time stamp.
 *	       This is useful when using the clock line discipline.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include "ntp_fp.h"
#include "ntp_string.h"
#include "ntp_unixtime.h"

#ifndef SYS_WINNT
int
buftvtots(
	const char *bufp,
	l_fp *ts
	)
{
	struct timeval tv;

	/*
	 * copy to adhere to alignment restrictions
	 */
	memcpy(&tv, bufp, sizeof(tv));

	/*
	 * and use it
	 */
	ts->l_ui = tv.tv_sec + (u_long)JAN_1970;
	if (tv.tv_usec > 999999)
	    return 0;
	TVUTOTSF(tv.tv_usec, ts->l_uf);
	return 1;
}
#else	/* SYS_WINNT */
/*
 * Windows doesn't have the tty_clock line discipline, so
 * don't look for a timestamp where there is none.
 */
int
buftvtots(
	const char *bufp,
	l_fp *ts
	)
{
	UNUSED_ARG(bufp);
	UNUSED_ARG(ts);

	return 0;
}
#endif	/* SYS_WINNT */
