/*
 * calyearstart - determine the NTP time at midnight of January 1 in
 *		  the year of the given date.
 */
#include <sys/types.h>

#include "ntp_types.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"

/*
 * calyeartab - year start offsets from the beginning of a cycle
 */
u_long calyeartab[YEARSPERCYCLE] = {
	(SECSPERLEAPYEAR-JANFEBLEAP),
	(SECSPERLEAPYEAR-JANFEBLEAP) + SECSPERYEAR,
	(SECSPERLEAPYEAR-JANFEBLEAP) + 2*SECSPERYEAR,
	(SECSPERLEAPYEAR-JANFEBLEAP) + 3*SECSPERYEAR
};

u_long
calyearstart(dateinyear)
	register u_long dateinyear;
{
	register u_long cyclestart;
	register u_long nextyear, lastyear;
	register int i;

	/*
	 * Find the start of the cycle this is in.
	 */
	if (dateinyear >= MAR1988)
		cyclestart = MAR1988;
	else
		cyclestart = MAR1900;
	while ((cyclestart + SECSPERCYCLE) <= dateinyear)
		cyclestart += SECSPERCYCLE;
	
	/*
	 * If we're in the first year of the cycle, January 1 is
	 * two months back from the cyclestart and the year is
	 * a leap year.
	 */
	lastyear = cyclestart + calyeartab[0];
	if (dateinyear < lastyear)
		return (cyclestart - JANFEBLEAP);

	/*
	 * Look for an intermediate year
	 */
	for (i = 1; i < YEARSPERCYCLE; i++) {
		nextyear = cyclestart + calyeartab[i];
		if (dateinyear < nextyear)
			return lastyear;
		lastyear = nextyear;
	}

	/*
	 * Not found, must be in last two months of cycle
	 */
	return nextyear;
}
