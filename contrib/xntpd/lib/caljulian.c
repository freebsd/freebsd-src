/* caljulian.c,v 3.1 1993/07/06 01:08:00 jbj Exp
 * caljulian - determine the Julian date from an NTP time.
 */
#include <sys/types.h>

#include "ntp_types.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"

/*
 * calmonthtab - month start offsets from the beginning of a cycle.
 */
static u_short calmonthtab[12] = {
	0,						/* March */
	MAR,						/* April */
	(MAR+APR),					/* May */
	(MAR+APR+MAY),					/* June */
	(MAR+APR+MAY+JUN),				/* July */
	(MAR+APR+MAY+JUN+JUL),				/* August */
	(MAR+APR+MAY+JUN+JUL+AUG),			/* September */
	(MAR+APR+MAY+JUN+JUL+AUG+SEP),			/* October */
	(MAR+APR+MAY+JUN+JUL+AUG+SEP+OCT),		/* November */
	(MAR+APR+MAY+JUN+JUL+AUG+SEP+OCT+NOV),		/* December */
	(MAR+APR+MAY+JUN+JUL+AUG+SEP+OCT+NOV+DEC),	/* January */
	(MAR+APR+MAY+JUN+JUL+AUG+SEP+OCT+NOV+DEC+JAN),	/* February */
};

/*
 * caldaytab - calendar year start day offsets
 */
static u_short caldaytab[YEARSPERCYCLE] = {
	(DAYSPERYEAR - (JAN + FEB)),
	((DAYSPERYEAR * 2) - (JAN + FEB)),
	((DAYSPERYEAR * 3) - (JAN + FEB)),
	((DAYSPERYEAR * 4) - (JAN + FEB)),
};

void
caljulian(ntptime, jt)
	U_LONG ntptime;
	register struct calendar *jt;
{
	register int i;
	register U_LONG nt;
	register u_short snt;
	register int cyear;

	/*
	 * Find the start of the cycle this is in.
	 */
	nt = ntptime;
	if (nt >= MAR1988) {
		cyear = CYCLE22;
		nt -= MAR1988;
	} else {
		cyear = 0;
		nt -= MAR1900;
	}
	while (nt >= SECSPERCYCLE) {
		nt -= SECSPERCYCLE;
		cyear++;
	}
	
	/*
	 * Seconds, minutes and hours are too hard to do without
	 * divides, so we don't.
	 */
	jt->second = nt % SECSPERMIN;
	nt /= SECSPERMIN;		/* nt in minutes */
	jt->minute = nt % MINSPERHR;
	snt = nt / MINSPERHR;		/* snt in hours */
	jt->hour = snt % HRSPERDAY;
	snt /= HRSPERDAY;		/* nt in days */

	/*
	 * snt is now the number of days into the cycle, from 0 to 1460.
	 */
	cyear <<= 2;
	if (snt < caldaytab[0]) {
		jt->yearday = snt + JAN + FEBLEAP + 1;	/* first year is leap */
	} else {
		for (i = 1; i < YEARSPERCYCLE; i++)
			if (snt < caldaytab[i])
				break;
		jt->yearday = snt - caldaytab[i-1] + 1;
		cyear += i;
	}
	jt->year = cyear + 1900;

	/*
	 * One last task, to compute the month and day.  Normalize snt to
	 * a day within a cycle year.
	 */
	while (snt >= DAYSPERYEAR)
		snt -= DAYSPERYEAR;
	for (i = 0; i < 11; i++)
		if (snt < calmonthtab[i+1])
			break;
	
	if (i > 9)
		jt->month = i - 9;	/* January or February */
	else
		jt->month = i + 3;	/* March through December */
	jt->monthday = snt - calmonthtab[i] + 1;
}
