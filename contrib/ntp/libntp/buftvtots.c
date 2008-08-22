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
