#
/*
 * MAKETIME		derive 32-bit time value from TM structure.
 *
 * Usage:
 *	int zone;	Minutes west of GMT, or
 *			48*60 for localtime
 *	time_t t;
 *	struct tm *tp;	Pointer to TM structure from <time.h>
 *	t = maketime(tp,zone);
 *
 * Returns:
 *	-1 if failure; parameter out of range or nonsensical.
 *	else time-value.
 * Notes:
 *	This code is quasi-public; it may be used freely in like software.
 *	It is not to be sold, nor used in licensed software without
 *	permission of the author.
 *	For everyone's benefit, please report bugs and improvements!
 * 	Copyright 1981 by Ken Harrenstien, SRI International.
 *	(ARPANET: KLH @ SRI)
 */
/* $Log: maketime.c,v $
 * Revision 5.3  1991/08/19  03:13:55  eggert
 * Add setfiledate, str2time, TZ_must_be_set.
 *
 * Revision 5.2  1990/11/01  05:03:30  eggert
 * Remove lint.
 *
 * Revision 5.1  1990/10/04  06:30:13  eggert
 * Calculate the GMT offset of 'xxx LT' as of xxx, not as of now.
 * Don't assume time_t is 32 bits.  Fix bugs near epoch and near end of time.
 *
 * Revision 5.0  1990/08/22  08:12:38  eggert
 * Switch to GMT and fix the bugs exposed thereby.
 * Permit dates past 1999/12/31.  Ansify and Posixate.
 *
 * Revision 1.8  88/11/08  13:54:53  narten
 * allow negative timezones (-24h <= x <= 24h)
 * 
 * Revision 1.7  88/08/28  14:47:52  eggert
 * Allow cc -R.  Remove unportable "#endif XXX"s.
 * 
 * Revision 1.6  87/12/18  17:05:58  narten
 * include rcsparam.h
 * 
 * Revision 1.5  87/12/18  11:35:51  narten
 * maketime.c: fixed USG code - you have tgo call "tzset" in order to have
 * "timezone" set. ("localtime" calls it, but it's probably better not to 
 * count on "localtime" having been called.)
 * 
 * Revision 1.4  87/10/18  10:26:57  narten
 * Updating version numbers. Changes relative to 1.0 are actually 
 * relative to 1.2
 * 
 * Revision 1.3  87/09/24  13:58:45  narten
 * Sources now pass through lint (if you ignore printf/sprintf/fprintf 
 * warnings)
 * 
 * Revision 1.2  87/03/27  14:21:48  jenkins
 * Port to suns
 * 
 * Revision 1.2  83/12/05  10:12:56  wft
 * added cond. compilation for USG Unix; long timezone;
 * 
 * Revision 1.1  82/05/06  11:38:00  wft
 * Initial revision
 * 
 */


#include "rcsbase.h"

libId(maketId, "$Id: maketime.c,v 5.3 1991/08/19 03:13:55 eggert Exp $")

static struct tm const *time2tm P((time_t));

#define given(v) (0 <= (v)) /* Negative values are unspecified. */

static int const daytb[] = {
	/* # days in year thus far, indexed by month (0-12!!) */
	0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365
};

	static time_t
maketime(atm,zone)
	struct tm const *atm;
	int zone;
{
    register struct tm const *tp;
    register int i;
    int year, yday, mon, day, hour, min, sec, leap, localzone;
    int attempts;
    time_t t, tres;

    attempts = 2;
    localzone = zone==48*60;
    tres = -1;
    year = mon = day = 0;  /* Keep lint happy.  */

    do {

	if (localzone || !given(atm->tm_year)) {
		if (tres == -1)
			if ((tres = time((time_t*)0))  ==  -1)
				return -1;
		tp = time2tm(tres);
		/* Get breakdowns of default time, adjusting to zone. */
		year = tp->tm_year;		/* Use to set up defaults */
		yday = tp->tm_yday;
		mon = tp->tm_mon;
		day = tp->tm_mday;
		hour = tp->tm_hour;
		min = tp->tm_min;
		if (localzone) {
		    tp = localtime(&tres);
		    zone =
			min - tp->tm_min + 60*(
				hour - tp->tm_hour + 24*(
					/* If years differ, it's by one day. */
						year - tp->tm_year
					?	year - tp->tm_year
					:	yday - tp->tm_yday));
		}
		/* Adjust the default day, month and year according to zone.  */
		if ((min -= zone) < 0) {
		    if (hour-(59-min)/60 < 0  &&  --day <= 0) {
			if (--mon < 0) {
				--year;
				mon = 11;
			}
			day  =  daytb[mon+1] - daytb[mon] + (mon==1&&!(year&3));
		    }
		} else
		    if (
		      24 <= hour+min/60  &&
		      daytb[mon+1] - daytb[mon] + (mon==1&&!(year&3))  <  ++day
		    ) {
			    if (11 < ++mon) {
				    ++year;
				    mon = 0;
			    }
			    day = 1;
		    }
	}
	if (zone < -24*60  ||  24*60 < zone)
		return -1;


#ifdef DEBUG
printf("first YMD: %d %d %d\n",year,mon,day);
#endif
	tp = atm;

	/* First must find date, using specified year, month, day.
	 * If one of these is unspecified, it defaults either to the
	 * current date (if no more global spec was given) or to the
	 * zero-value for that spec (i.e. a more global spec was seen).
	 * Reject times that do not fit in time_t,
	 * without assuming that time_t is 32 bits or is signed.
	 */
	if (given(tp->tm_year))
	  {
		year = tp->tm_year;
		mon = 0;		/* Since year was given, default */
		day = 1;		/* for remaining specs is zero */
	  }
	if (year < 69)			/* 1969/12/31 OK in some timezones.  */
		return -1;		/* ERR: year out of range */
	leap   =   !(year&3)  &&  (year%100 || !((year+300)%400));
	year -= 70;			/* UNIX time starts at 1970 */

	/*
	 * Find day of year.
	 */
	{
		if (given(tp->tm_mon))
		  {	mon = tp->tm_mon;	/* Month was specified */
			day = 1;		/* so set remaining default */
		  }
		if (11 < (unsigned)mon)
			return -1;		/* ERR: bad month */
		if (given(tp->tm_mday)) day = tp->tm_mday;
		if(day < 1
		 || (((daytb[mon+1]-daytb[mon]) < day)
			&& (day!=29 || mon!=1 || !leap) ))
				return -1;	/* ERR: bad day */
		yday = daytb[mon]	/* Add # of days in months so far */
		  + ((leap		/* Leap year, and past Feb?  If */
		      && mon>1)? 1:0)	/* so, add leap day for this year */
		  + day-1;		/* And finally add # days this mon */

	}
	if (leap+365 <= (unsigned)yday)
		return -1;		/* ERR: bad YDAY */

	if (year < 0) {
	    if (yday != 364)
		return -1;		/* ERR: too early */
	    t = -1;
	} else {
	    tres = year*365;		/* Get # days of years so far */
	    if (tres/365 != year)
		    return -1;		/* ERR: overflow */
	    t = tres
		+ ((year+1)>>2)		/* plus # of leap days since 1970 */
		+ yday;			/* and finally add # days this year */
	    if (t+4 < tres)
		    return -1;		/* ERR: overflow */
	}
	tres = t;

	if (given(i = tp->tm_wday)) /* Check WDAY if present */
		if (i != (tres+4)%7)	/* 1970/01/01 was Thu = 4 */
			return -1;	/* ERR: bad WDAY */

#ifdef DEBUG
printf("YMD: %d %d %d, T=%ld\n",year,mon,day,tres);
#endif
	/*
	 * Now determine time.  If not given, default to zeros
	 * (since time is always the least global spec)
	 */
	tres *= 86400L;			/* Get # seconds (24*60*60) */
	if (tres/86400L != t)
		return -1;		/* ERR: overflow */
	hour = min = sec = 0;
	if (given(tp->tm_hour)) hour = tp->tm_hour;
	if (given(tp->tm_min )) min  = tp->tm_min;
	if (given(tp->tm_sec )) sec  = tp->tm_sec;
	if (60 <= (unsigned)min  ||  60 < (unsigned)sec)
		return -1;		/* ERR: MS out of range */
	if (24 <= (unsigned)hour)
		if(hour != 24 || (min+sec) !=0)	/* Allow 24:00 */
			return -1;	/* ERR: H out of range */

	t = tres;
	tres += sec + 60L*(zone + min + 60*hour);

#ifdef DEBUG
printf("HMS: %d %d %d T=%ld\n",hour,min,sec,tres);
#endif

	if (!localzone)			/* check for overflow */
	    return (year<0 ? (tres<0||86400L<=tres) : tres<t)  ?  -1  :  tres;

	/* Check results; LT may have had a different GMT offset back then.  */
	tp = localtime(&tres);
	if (given(atm->tm_sec)  &&  atm->tm_sec != tp->tm_sec)
		return -1; /* If seconds don't match, we're in trouble.  */
	if (!(
	    given(atm->tm_min)  &&  atm->tm_min != tp->tm_min  ||
	    given(atm->tm_hour)  &&  atm->tm_hour != tp->tm_hour  ||
	    given(atm->tm_mday)  &&  atm->tm_mday != tp->tm_mday  ||
	    given(atm->tm_mon)  &&  atm->tm_mon != tp->tm_mon  ||
	    given(atm->tm_year)  &&  atm->tm_year != tp->tm_year
	))
		return tres; /* Everything matches.  */

    } while (--attempts);

    return -1;
}

/*
* Convert Unix time to struct tm format.
* Use Coordinated Universal Time (UTC) if version 5 or newer;
* use local time otherwise.
*/
	static struct tm const *
time2tm(unixtime)
	time_t unixtime;
{
	struct tm const *tm;
#	if TZ_must_be_set
		static char const *TZ;
		if (!TZ  &&  !(TZ = getenv("TZ")))
			faterror("TZ is not set");
#	endif
	if (!(tm  =  (RCSversion<VERSION(5) ? localtime : gmtime)(&unixtime)))
		faterror("UTC is not available; perhaps TZ is not set?");
	return tm;
}

/*
* Convert Unix time to RCS format.
* For compatibility with older versions of RCS,
* dates before AD 2000 are stored without the leading "19".
*/
	void
time2date(unixtime,date)
	time_t unixtime;
	char date[datesize];
{
	register struct tm const *tm = time2tm(unixtime);
	VOID sprintf(date, DATEFORM,
		tm->tm_year  +  (tm->tm_year<100 ? 0 : 1900),
		tm->tm_mon+1, tm->tm_mday,
		tm->tm_hour, tm->tm_min, tm->tm_sec
	);
}



	static time_t
str2time(source)
	char const *source;
/* Parse a free-format date in SOURCE, yielding a Unix format time.  */
{
	int zone;
	time_t unixtime;
	struct tm parseddate;

	if (!partime(source, &parseddate, &zone))
	    faterror("can't parse date/time: %s", source);
	if ((unixtime = maketime(&parseddate, zone))  ==  -1)
	    faterror("bad date/time: %s", source);
	return unixtime;
}

	void
str2date(source, target)
	char const *source;
	char target[datesize];
/* Parse a free-format date in SOURCE, convert it
 * into RCS internal format, and store the result into TARGET.
 */
{
	time2date(str2time(source), target);
}

	int
setfiledate(file, date)
	char const *file, date[datesize];
/* Set the access and modification time of FILE to DATE.  */
{
	static struct utimbuf times; /* static so unused fields are zero */
	char datebuf[datesize];

	if (!date)
		return 0;
	times.actime = times.modtime = str2time(date2str(date, datebuf));
	return utime(file, &times);
}
