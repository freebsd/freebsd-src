#ifdef	ELIMINATE
/* tstotod.c,v 3.1 1993/07/06 01:08:48 jbj Exp
 * tstotod - compute calendar time given an NTP timestamp
 */
#include <stdio.h>

#include "ntp_fp.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"

void
tstotod(ts, tod)
	l_fp *ts;
	struct calendar *tod;
{
	register U_LONG cyclesecs;

	cyclesecs = ts.l_ui - MAR_1900;		/* bump forward to March 1900 */

}
#endif	/* ELIMINATE */
