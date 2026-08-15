/*
 * This file is in the public domain, so clarified as of
 * 1996-06-05 by Arthur David Olson.
 */

/*
 * The whole implementation of `timesub()` and its dependencies are copied from
 * `contrib/tzcode/localtime.c` as of FreeBSD commit
 * 28f617de7d9b9c708eacb3c2c13e5287e1b7354d.
 */

#include <linux/time.h>

enum {
  SECSPERMIN = 60,
  MINSPERHOUR = 60,
  SECSPERHOUR = SECSPERMIN * MINSPERHOUR,
  HOURSPERDAY = 24,
  DAYSPERWEEK = 7,
  DAYSPERNYEAR = 365,
  DAYSPERLYEAR = DAYSPERNYEAR + 1,
  MONSPERYEAR = 12,
  YEARSPERREPEAT = 400	/* years before a Gregorian repeat */
};

enum {
  TM_SUNDAY,
  TM_MONDAY,
  TM_TUESDAY,
  TM_WEDNESDAY,
  TM_THURSDAY,
  TM_FRIDAY,
  TM_SATURDAY
};

enum {
  TM_JANUARY,
  TM_FEBRUARY,
  TM_MARCH,
  TM_APRIL,
  TM_MAY,
  TM_JUNE,
  TM_JULY,
  TM_AUGUST,
  TM_SEPTEMBER,
  TM_OCTOBER,
  TM_NOVEMBER,
  TM_DECEMBER
};

enum {
  TM_YEAR_BASE = 1900,
  TM_WDAY_BASE = TM_MONDAY,
  EPOCH_YEAR = 1970,
  EPOCH_WDAY = TM_THURSDAY
};

#define SECSPERDAY	(SECSPERHOUR * HOURSPERDAY)
#define DAYSPERREPEAT	(400 * 365 + 100 - 4 + 1)

#define isleap(y) (((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))

#define TYPE_SIGNED(type) (((type) -1) < 0)

static const int	mon_lengths[2][MONSPERYEAR] = {
	{ 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
	{ 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static const int	year_lengths[2] = {
	DAYSPERNYEAR, DAYSPERLYEAR
};

static time_t
leaps_thru_end_of_nonneg(time_t y)
{
  return y / 4 - y / 100 + y / 400;
}

static time_t
leaps_thru_end_of(time_t y)
{
  return (y < 0
	  ? -1 - leaps_thru_end_of_nonneg(-1 - y)
	  : leaps_thru_end_of_nonneg(y));
}

static struct tm *
timesub(const time64_t *timep, int offset, struct tm *tmp)
{
	time_t			tdays;
	const int *		ip;
	int corr;
	int idays, rem, dayoff, dayrem;
	time_t y;

	/* If less than SECSPERMIN, the number of seconds since the
	   most recent positive leap second; otherwise, do not add 1
	   to localtime tm_sec because of leap seconds.  */
	time_t secs_since_posleap = SECSPERMIN;

	corr = 0;

	/* Calculate the year, avoiding integer overflow even if
	   time_t is unsigned.  */
	tdays = *timep / SECSPERDAY;
	rem = *timep % SECSPERDAY;
	rem += offset % SECSPERDAY - corr % SECSPERDAY + 3 * SECSPERDAY;
	dayoff = offset / SECSPERDAY - corr / SECSPERDAY + rem / SECSPERDAY - 3;
	rem %= SECSPERDAY;
	/* y = (EPOCH_YEAR
		+ floor((tdays + dayoff) / DAYSPERREPEAT) * YEARSPERREPEAT),
	   sans overflow.  But calculate against 1570 (EPOCH_YEAR -
	   YEARSPERREPEAT) instead of against 1970 so that things work
	   for localtime values before 1970 when time_t is unsigned.  */
	dayrem = tdays % DAYSPERREPEAT;
	dayrem += dayoff % DAYSPERREPEAT;
	y = (EPOCH_YEAR - YEARSPERREPEAT
	     + ((1 + dayoff / DAYSPERREPEAT + dayrem / DAYSPERREPEAT
		 - ((dayrem % DAYSPERREPEAT) < 0)
		 + tdays / DAYSPERREPEAT)
		* YEARSPERREPEAT));
	/* idays = (tdays + dayoff) mod DAYSPERREPEAT, sans overflow.  */
	idays = tdays % DAYSPERREPEAT;
	idays += dayoff % DAYSPERREPEAT + 2 * DAYSPERREPEAT;
	idays %= DAYSPERREPEAT;
	/* Increase Y and decrease IDAYS until IDAYS is in range for Y.  */
	while (year_lengths[isleap(y)] <= idays) {
		int tdelta = idays / DAYSPERLYEAR;
		int_fast32_t ydelta = tdelta + !tdelta;
		time_t newy = y + ydelta;
		register int	leapdays;
		leapdays = leaps_thru_end_of(newy - 1) -
			leaps_thru_end_of(y - 1);
		idays -= ydelta * DAYSPERNYEAR;
		idays -= leapdays;
		y = newy;
	}

	if (!TYPE_SIGNED(time_t) && y < TM_YEAR_BASE) {
	  int signed_y = y;
	  tmp->tm_year = signed_y - TM_YEAR_BASE;
	} else if ((!TYPE_SIGNED(time_t) || INT_MIN + TM_YEAR_BASE <= y)
		   && y - TM_YEAR_BASE <= INT_MAX)
	  tmp->tm_year = y - TM_YEAR_BASE;
	else {
	  return NULL;
	}
	tmp->tm_yday = idays;
	/*
	** The "extra" mods below avoid overflow problems.
	*/
	tmp->tm_wday = (TM_WDAY_BASE
			+ ((tmp->tm_year % DAYSPERWEEK)
			   * (DAYSPERNYEAR % DAYSPERWEEK))
			+ leaps_thru_end_of(y - 1)
			- leaps_thru_end_of(TM_YEAR_BASE - 1)
			+ idays);
	tmp->tm_wday %= DAYSPERWEEK;
	if (tmp->tm_wday < 0)
		tmp->tm_wday += DAYSPERWEEK;
	tmp->tm_hour = rem / SECSPERHOUR;
	rem %= SECSPERHOUR;
	tmp->tm_min = rem / SECSPERMIN;
	tmp->tm_sec = rem % SECSPERMIN;

	/* Use "... ??:??:60" at the end of the localtime minute containing
	   the second just before the positive leap second.  */
	tmp->tm_sec += secs_since_posleap <= tmp->tm_sec;

	ip = mon_lengths[isleap(y)];
	for (tmp->tm_mon = 0; idays >= ip[tmp->tm_mon]; ++(tmp->tm_mon))
		idays -= ip[tmp->tm_mon];
	tmp->tm_mday = idays + 1;
	tmp->tm_isdst = 0;
	return tmp;
}

void
linuxkpi_time64_to_tm(time64_t totalsecs, int offset, struct tm *result)
{
	timesub(&totalsecs, offset, result);
}
