/*
 * ymd2yd - compute the date in the year from y/m/d
 */

#include "ntp_fp.h"
#include "ntp_unixtime.h"
#include "ntp_stdlib.h"

/*
 * Tables to compute the day of year from yyyymmdd timecode.
 * Viva la leap.
 */
static int day1tab[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static int day2tab[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

int
ymd2yd(
	int y,
	int m,
	int d
	)
{
	int i, *t;

	if (m < 1 || m > 12 || d < 1)
		return (-1);

	if (((y%4 == 0) && (y%100 != 0)) || (y%400 == 0))
		t = day2tab;	/* leap year */
	else
		t = day1tab;	/* not a leap year */
	if (d > t[m - 1])
		return (-1);
	for (i = 0; i < m - 1; i++)
		d += t[i];
	return d;
}
