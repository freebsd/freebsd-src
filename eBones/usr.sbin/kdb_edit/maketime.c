/*
 * Copyright 1990 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * Convert a struct tm * to a UNIX time.
 *
 *	from: maketime.c,v 4.2 90/01/09 15:54:51 raeburn Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef	lint
static char rcsid[] =
"$Id: maketime.c,v 1.1 1994/03/21 16:23:54 piero Exp ";
#endif	lint
#endif

#include <sys/time.h>

#define daysinyear(y) (((y) % 4) ? 365 : (((y) % 100) ? 366 : (((y) % 400) ? 365 : 366)))

#define SECSPERDAY 24*60*60
#define SECSPERHOUR 60*60
#define SECSPERMIN 60

static int cumdays[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334,
			     365};

static int leapyear[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static int nonleapyear[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

long
maketime(tp, local)
register struct tm *tp;
int local;
{
    register long retval;
    int foo;
    int *marray;

    if (tp->tm_mon < 0 || tp->tm_mon > 11 ||
	tp->tm_hour < 0 || tp->tm_hour > 23 ||
	tp->tm_min < 0 || tp->tm_min > 59 ||
	tp->tm_sec < 0 || tp->tm_sec > 59) /* out of range */
	return 0;

    retval = 0;
    if (tp->tm_year < 1900)
	foo = tp->tm_year + 1900;
    else
	foo = tp->tm_year;

    if (foo < 1901 || foo > 2038)	/* year is too small/large */
	return 0;

    if (daysinyear(foo) == 366) {
	if (tp->tm_mon > 1)
	    retval+= SECSPERDAY;	/* add leap day */
	marray = leapyear;
    } else
	marray = nonleapyear;

    if (tp->tm_mday < 0 || tp->tm_mday > marray[tp->tm_mon])
	return 0;			/* out of range */

    while (--foo >= 1970)
	retval += daysinyear(foo) * SECSPERDAY;

    retval += cumdays[tp->tm_mon] * SECSPERDAY;
    retval += (tp->tm_mday-1) * SECSPERDAY;
    retval += tp->tm_hour * SECSPERHOUR + tp->tm_min * SECSPERMIN + tp->tm_sec;

    if (local) {
	/* need to use local time, so we retrieve timezone info */
	struct timezone tz;
	struct timeval tv;
	if (gettimeofday(&tv, &tz) < 0) {
	    /* some error--give up? */
	    return(retval);
	}
	retval += tz.tz_minuteswest * SECSPERMIN;
    }
    return(retval);
}
