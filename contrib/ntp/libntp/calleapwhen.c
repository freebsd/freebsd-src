/*
 * calleapwhen - determine the number of seconds to the next possible
 *		 leap occurance.
 */
#include <sys/types.h>

#include "ntp_types.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"

/*
 * calleaptab - leaps occur at the end of December and June
 */
long calleaptab[10] = {
	-(JAN+FEBLEAP)*SECSPERDAY,	/* leap previous to cycle */
	(MAR+APR+MAY+JUN)*SECSPERDAY,	/* end of June */
	(MAR+APR+MAY+JUN+JUL+AUG+SEP+OCT+NOV+DEC)*SECSPERDAY, /* end of Dec */
	(MAR+APR+MAY+JUN)*SECSPERDAY + SECSPERYEAR,
	(MAR+APR+MAY+JUN+JUL+AUG+SEP+OCT+NOV+DEC)*SECSPERDAY + SECSPERYEAR,
	(MAR+APR+MAY+JUN)*SECSPERDAY + 2*SECSPERYEAR,
	(MAR+APR+MAY+JUN+JUL+AUG+SEP+OCT+NOV+DEC)*SECSPERDAY + 2*SECSPERYEAR,
	(MAR+APR+MAY+JUN)*SECSPERDAY + 3*SECSPERYEAR,
	(MAR+APR+MAY+JUN+JUL+AUG+SEP+OCT+NOV+DEC)*SECSPERDAY + 3*SECSPERYEAR,
	(MAR+APR+MAY+JUN+JUL+AUG+SEP+OCT+NOV+DEC+JAN+FEBLEAP+MAR+APR+MAY+JUN)
	*SECSPERDAY + 3*SECSPERYEAR,	/* next after current cycle */
};

u_long
calleapwhen(
	u_long ntpdate
	)
{
	register u_long dateincycle;
	register int i;

	/*
	 * Find the offset from the start of the cycle
	 */
	dateincycle = ntpdate;
	if (dateincycle >= MAR1988)
	    dateincycle -= MAR1988;
	else
	    dateincycle -= MAR1900;

	while (dateincycle >= SECSPERCYCLE)
	    dateincycle -= SECSPERCYCLE;

	/*
	 * Find where we are with respect to the leap events.
	 */
	for (i = 1; i < 9; i++)
	    if (dateincycle < (u_long)calleaptab[i])
		break;
	
	/*
	 * i points at the next leap.  Compute the last and the next.
	 */
	return (u_long)(calleaptab[i] - (long)dateincycle);
}
