/*
 * ntp_calgps.c - calendar for GPS/GNSS based clocks
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 *
 * --------------------------------------------------------------------
 *
 * This module implements stuff often used with GPS/GNSS receivers
 */

#include <config.h>
#include <sys/types.h>

#include "ntp_types.h"
#include "ntp_calendar.h"
#include "ntp_calgps.h"
#include "ntp_stdlib.h"
#include "ntp_unixtime.h"

#include "ntp_fp.h"
#include "ntpd.h"
#include "vint64ops.h"

/* ====================================================================
 * misc. helpers -- might go elsewhere sometime?
 * ====================================================================
 */

l_fp
ntpfp_with_fudge(
	l_fp	lfp,
	double	ofs
	)
{
	l_fp	fpo;
	/* calculate 'lfp - ofs' as '(l_fp)(-ofs) + lfp': negating a
	 * double is cheap, as it only flips one bit...
	 */
	ofs = -ofs;
	DTOLFP(ofs, &fpo);
	L_ADD(&fpo, &lfp);
	return fpo;
}


/* ====================================================================
 * GPS calendar functions
 * ====================================================================
 */

/* --------------------------------------------------------------------
 * normalization functions for day/time and week/time representations.
 * Since we only use moderate offsets (leap second corrections and
 * alike) it does not really pay off to do a floor-corrected division
 * here.  We use compare/decrement/increment loops instead.
 * --------------------------------------------------------------------
 */
static void
_norm_ntp_datum(
	TNtpDatum *	datum
	)
{
	static const int32_t limit = SECSPERDAY;

	if (datum->secs >= limit) {
		do
			++datum->days;
		while ((datum->secs -= limit) >= limit);
	} else if (datum->secs < 0) {
		do
			--datum->days;
		while ((datum->secs += limit) < 0);
	}
}

static void
_norm_gps_datum(
	TGpsDatum *	datum
	)
{
	static const int32_t limit = 7 * SECSPERDAY;

	if (datum->wsecs >= limit) {
		do
			++datum->weeks;
		while ((datum->wsecs -= limit) >= limit);
	} else if (datum->wsecs < 0) {
		do
			--datum->weeks;
		while ((datum->wsecs += limit) < 0);
	}
}

/* --------------------------------------------------------------------
 * Add an offset to a day/time and week/time representation.
 *
 * !!Attention!! the offset should be small, compared to the time period
 * (either a day or a week).
 * --------------------------------------------------------------------
 */
void
gpsntp_add_offset(
	TNtpDatum *	datum,
	l_fp		offset
	)
{
	/* fraction can be added easily */
	datum->frac += offset.l_uf;
	datum->secs += (datum->frac < offset.l_uf);

	/* avoid integer overflow on the seconds */
	if (offset.l_ui >= INT32_MAX)
		datum->secs -= (int32_t)~offset.l_ui + 1;
	else
		datum->secs += (int32_t)offset.l_ui;
	_norm_ntp_datum(datum);
}

void
gpscal_add_offset(
	TGpsDatum *	datum,
	l_fp		offset
	)
{
	/* fraction can be added easily */
	datum->frac  += offset.l_uf;
	datum->wsecs += (datum->frac < offset.l_uf);


	/* avoid integer overflow on the seconds */
	if (offset.l_ui >= INT32_MAX)
		datum->wsecs -= (int32_t)~offset.l_ui + 1;
	else
		datum->wsecs += (int32_t)offset.l_ui;
	_norm_gps_datum(datum);
}

/* -------------------------------------------------------------------
 *	API functions civil calendar and NTP datum
 * -------------------------------------------------------------------
 */

static TNtpDatum
_gpsntp_fix_gps_era(
	TcNtpDatum * in
	)
{
	/* force result in basedate era
	 *
	 * When calculating this directly in days, we have to execute a
	 * real modulus calculation, since we're obviously not doing a
	 * modulus by a power of 2. Executing this as true floor mod
	 * needs some care and is done under explicit usage of one's
	 * complement and masking to get mostly branchless code.
	 */
	static uint32_t const	clen = 7*1024;

	uint32_t	base, days, sign;
	TNtpDatum	out = *in;

	/* Get base in NTP day scale. No overflows here. */
	base = (basedate_get_gpsweek() + GPSNTP_WSHIFT) * 7
	     - GPSNTP_DSHIFT;
	days = out.days;

	sign = (uint32_t)-(days < base);
	days = sign ^ (days - base);
	days %= clen;
	days = base + (sign & clen) + (sign ^ days);

	out.days = days;
	return out;
}

TNtpDatum
gpsntp_fix_gps_era(
	TcNtpDatum * in
	)
{
	TNtpDatum out = *in;
	_norm_ntp_datum(&out);
	return _gpsntp_fix_gps_era(&out);
}

/* ----------------------------------------------------------------- */
static TNtpDatum
_gpsntp_from_daytime(
	TcCivilDate *	jd,
	l_fp		fofs,
	TcNtpDatum *	pivot,
	int		warp
	)
{
	static const int32_t shift = SECSPERDAY / 2;

	TNtpDatum	retv;

	/* set result based on pivot -- ops order is important here */
	ZERO(retv);
	retv.secs = ntpcal_date_to_daysec(jd);
	gpsntp_add_offset(&retv, fofs);	/* result is normalized */
	retv.days = pivot->days;

	/* Manual periodic extension without division: */
	if (pivot->secs < shift) {
		int32_t lim = pivot->secs + shift;
		retv.days -= (retv.secs > lim ||
			      (retv.secs == lim && retv.frac >= pivot->frac));
	} else {
		int32_t lim = pivot->secs - shift;
		retv.days += (retv.secs < lim ||
			      (retv.secs == lim && retv.frac < pivot->frac));
	}
	return warp ? _gpsntp_fix_gps_era(&retv) : retv;
}

/* -----------------------------------------------------------------
 * Given the time-of-day part of a civil datum and an additional
 * (fractional) offset, calculate a full time stamp around a given pivot
 * time so that the difference between the pivot and the resulting time
 * stamp is less or equal to 12 hours absolute.
 */
TNtpDatum
gpsntp_from_daytime2_ex(
	TcCivilDate *	jd,
	l_fp		fofs,
	TcNtpDatum *	pivot,
	int/*BOOL*/	warp
	)
{
	TNtpDatum	dpiv = *pivot;
	_norm_ntp_datum(&dpiv);
	return _gpsntp_from_daytime(jd, fofs, &dpiv, warp);
}

/* -----------------------------------------------------------------
 * This works similar to 'gpsntp_from_daytime1()' and actually even uses
 * it, but the pivot is calculated from the pivot given as 'l_fp' in NTP
 * time scale. This is in turn expanded around the current system time,
 * and the resulting absolute pivot is then used to calculate the full
 * NTP time stamp.
 */
TNtpDatum
gpsntp_from_daytime1_ex(
	TcCivilDate *	jd,
	l_fp		fofs,
	l_fp		pivot,
	int/*BOOL*/	warp
	)
{
	vint64		pvi64;
	TNtpDatum	dpiv;
	ntpcal_split	split;

	pvi64 = ntpcal_ntp_to_ntp(pivot.l_ui, NULL);
	split = ntpcal_daysplit(&pvi64);
	dpiv.days = split.hi;
	dpiv.secs = split.lo;
	dpiv.frac = pivot.l_uf;
	return _gpsntp_from_daytime(jd, fofs, &dpiv, warp);
}

/* -----------------------------------------------------------------
 * Given a calendar date, zap it into a GPS time format and then convert
 * that one into the NTP time scale.
 */
TNtpDatum
gpsntp_from_calendar_ex(
	TcCivilDate *	jd,
	l_fp		fofs,
	int/*BOOL*/	warp
	)
{
	TGpsDatum	gps;
	gps = gpscal_from_calendar_ex(jd, fofs, warp);
	return gpsntp_from_gpscal_ex(&gps, FALSE);
}

/* -----------------------------------------------------------------
 * create a civil calendar datum from a NTP date representation
 */
void
gpsntp_to_calendar(
	TCivilDate * cd,
	TcNtpDatum * nd
	)
{
	memset(cd, 0, sizeof(*cd));
	ntpcal_rd_to_date(
		cd,
		nd->days + DAY_NTP_STARTS + ntpcal_daysec_to_date(
			cd, nd->secs));
}

/* -----------------------------------------------------------------
 * get day/tod representation from week/tow datum
 */
TNtpDatum
gpsntp_from_gpscal_ex(
	TcGpsDatum *	gd,
    	int/*BOOL*/	warp
	)
{
	TNtpDatum	retv;
	vint64		ts64;
	ntpcal_split	split;
	TGpsDatum	date = *gd;

	if (warp) {
		uint32_t base = basedate_get_gpsweek() + GPSNTP_WSHIFT;
		_norm_gps_datum(&date);
		date.weeks = ((date.weeks - base) & 1023u) + base;
	}

	ts64  = ntpcal_weekjoin(date.weeks, date.wsecs);
	ts64  = subv64u32(&ts64, (GPSNTP_DSHIFT * SECSPERDAY));
	split = ntpcal_daysplit(&ts64);

	retv.frac = gd->frac;
	retv.secs = split.lo;
	retv.days = split.hi;
	return retv;
}

/* -----------------------------------------------------------------
 * get LFP from ntp datum
 */
l_fp
ntpfp_from_ntpdatum(
	TcNtpDatum *	nd
	)
{
	l_fp retv;

	retv.l_uf = nd->frac;
	retv.l_ui = nd->days * (uint32_t)SECSPERDAY
	          + nd->secs;
	return retv;
}

/* -------------------------------------------------------------------
 *	API functions GPS week calendar
 *
 * Here we use a calendar base of 1899-12-31, so the NTP epoch has
 * { 0, 86400.0 } in this representation.
 * -------------------------------------------------------------------
 */

static TGpsDatum
_gpscal_fix_gps_era(
	TcGpsDatum * in
	)
{
	/* force result in basedate era
	 *
	 * This is based on calculating the modulus to a power of two,
	 * so signed integer overflow does not affect the result. Which
	 * in turn makes for a very compact calculation...
	 */
	uint32_t	base, week;
	TGpsDatum	out = *in;

	week = out.weeks;
	base = basedate_get_gpsweek() + GPSNTP_WSHIFT;
	week = base + ((week - base) & (GPSWEEKS - 1));
	out.weeks = week;
	return out;
}

TGpsDatum
gpscal_fix_gps_era(
	TcGpsDatum * in
	)
{
	TGpsDatum out = *in;
	_norm_gps_datum(&out);
	return _gpscal_fix_gps_era(&out);
}

/* -----------------------------------------------------------------
 * Given a calendar date, zap it into a GPS time format and the do a
 * proper era mapping in the GPS time scale, based on the GPS base date,
 * if so requested.
 *
 * This function also augments the century if just a 2-digit year
 * (0..99) is provided on input.
 *
 * This is a fail-safe against GPS receivers with an unknown starting
 * point for their internal calendar calculation and therefore
 * unpredictable (but reproducible!) rollover behavior. While there
 * *are* receivers that create a full date in the proper way, many
 * others just don't.  The overall damage is minimized by simply not
 * trusting the era mapping of the receiver and doing the era assignment
 * with a configurable base date *inside* ntpd.
 */
TGpsDatum
gpscal_from_calendar_ex(
	TcCivilDate *	jd,
	l_fp		fofs,
	int/*BOOL*/	warp
	)
{
	/*  (-DAY_GPS_STARTS) (mod 7*1024) -- complement of cycle shift */
	static const uint32_t s_compl_shift =
	    (7 * 1024) - DAY_GPS_STARTS % (7 * 1024);

	TGpsDatum	gps;
	TCivilDate	cal;
	int32_t		days, week;

	/* if needed, convert from 2-digit year to full year
	 * !!NOTE!! works only between 1980 and 2079!
	 */
	cal = *jd;
	if (cal.year < 80)
		cal.year += 2000;
	else if (cal.year < 100)
		cal.year += 1900;

	/* get RDN from date, possibly adjusting the century */
again:	if (cal.month && cal.monthday) {	/* use Y/M/D civil date */
		days = ntpcal_date_to_rd(&cal);
	} else {				/* using Y/DoY date */
		days = ntpcal_year_to_ystart(cal.year)
		     + (int32_t)cal.yearday
		     - 1; /* both RDN and yearday start with '1'. */
	}

	/* Rebase to days after the GPS epoch. 'days' is positive here,
	 * but it might be less than the GPS epoch start. Depending on
	 * the input, we have to do different things to get the desired
	 * result. (Since we want to remap the era anyway, we only have
	 * to retain congruential identities....)
	 */

	if (days >= DAY_GPS_STARTS) {
		/* simply shift to days since GPS epoch */
		days -= DAY_GPS_STARTS;
	} else if (jd->year < 100) {
		/* Two-digit year on input: add another century and
		 * retry.  This can happen only if the century expansion
		 * yielded a date between 1980-01-01 and 1980-01-05,
		 * both inclusive. We have at most one retry here.
		 */
		cal.year += 100;
		goto again;
	} else {
		/* A very bad date before the GPS epoch. There's not
		 * much we can do, except to add the complement of
		 * DAY_GPS_STARTS % (7 * 1024) here, that is, use a
		 * congruential identity: Add the complement instead of
		 * subtracting the value gives a value with the same
		 * modulus. But of course, now we MUST to go through a
		 * cycle fix... because the date was obviously wrong!
		 */
		warp  = TRUE;
		days += s_compl_shift;
	}

	/* Splitting to weeks is simple now: */
	week  = days / 7;
	days -= week * 7;

	/* re-base on start of NTP with weeks mapped to 1024 weeks
	 * starting with the GPS base day set in the calendar.
	 */
	gps.weeks = week + GPSNTP_WSHIFT;
	gps.wsecs = days * SECSPERDAY + ntpcal_date_to_daysec(&cal);
	gps.frac  = 0;
	gpscal_add_offset(&gps, fofs);
	return warp ? _gpscal_fix_gps_era(&gps) : gps;
}

/* -----------------------------------------------------------------
 * get civil date from week/tow representation
 */
void
gpscal_to_calendar(
	TCivilDate * cd,
	TcGpsDatum * wd
	)
{
	TNtpDatum nd;

	memset(cd, 0, sizeof(*cd));
	nd = gpsntp_from_gpscal_ex(wd, FALSE);
	gpsntp_to_calendar(cd, &nd);
}

/* -----------------------------------------------------------------
 * Given the week and seconds in week, as well as the fraction/offset
 * (which should/could include the leap seconds offset), unfold the
 * weeks (which are assumed to have just 10 bits) into expanded weeks
 * based on the GPS base date derived from the build date (default) or
 * set by the configuration.
 *
 * !NOTE! This function takes RAW GPS weeks, aligned to the GPS start
 * (1980-01-06) on input. The output weeks will be aligned to NTPD's
 * week calendar start (1899-12-31)!
 */
TGpsDatum
gpscal_from_gpsweek(
	uint16_t	week,
	int32_t		secs,
	l_fp		fofs
	)
{
	TGpsDatum retv;

	retv.frac  = 0;
	retv.wsecs = secs;
	retv.weeks = week + GPSNTP_WSHIFT;
	gpscal_add_offset(&retv, fofs);
	return _gpscal_fix_gps_era(&retv);
}

/* -----------------------------------------------------------------
 * internal work horse for time-of-week expansion
 */
static TGpsDatum
_gpscal_from_weektime(
	int32_t		wsecs,
	l_fp    	fofs,
	TcGpsDatum *	pivot
	)
{
	static const int32_t shift = SECSPERWEEK / 2;

	TGpsDatum	retv;

	/* set result based on pivot -- ops order is important here */
	ZERO(retv);
	retv.wsecs = wsecs;
	gpscal_add_offset(&retv, fofs);	/* result is normalized */
	retv.weeks = pivot->weeks;

	/* Manual periodic extension without division: */
	if (pivot->wsecs < shift) {
		int32_t lim = pivot->wsecs + shift;
		retv.weeks -= (retv.wsecs > lim ||
			       (retv.wsecs == lim && retv.frac >= pivot->frac));
	} else {
		int32_t lim = pivot->wsecs - shift;
		retv.weeks += (retv.wsecs < lim ||
			       (retv.wsecs == lim && retv.frac < pivot->frac));
	}
	return _gpscal_fix_gps_era(&retv);
}

/* -----------------------------------------------------------------
 * expand a time-of-week around a pivot given as week datum
 */
TGpsDatum
gpscal_from_weektime2(
	int32_t		wsecs,
	l_fp    	fofs,
	TcGpsDatum *	pivot
	)
{
	TGpsDatum wpiv = * pivot;
	_norm_gps_datum(&wpiv);
	return _gpscal_from_weektime(wsecs, fofs, &wpiv);
}

/* -----------------------------------------------------------------
 * epand a time-of-week around an pivot given as LFP, which in turn
 * is expanded around the current system time and then converted
 * into a week datum.
 */
TGpsDatum
gpscal_from_weektime1(
	int32_t	wsecs,
	l_fp    fofs,
	l_fp    pivot
	)
{
	vint64		pvi64;
	TGpsDatum	wpiv;
	ntpcal_split	split;

	/* get 64-bit pivot in NTP epoch */
	pvi64 = ntpcal_ntp_to_ntp(pivot.l_ui, NULL);

	/* convert to weeks since 1899-12-31 and seconds in week */
	pvi64 = addv64u32(&pvi64, (GPSNTP_DSHIFT * SECSPERDAY));
	split = ntpcal_weeksplit(&pvi64);

	wpiv.weeks = split.hi;
	wpiv.wsecs = split.lo;
	wpiv.frac  = pivot.l_uf;
	return _gpscal_from_weektime(wsecs, fofs, &wpiv);
}

/* -----------------------------------------------------------------
 * get week/tow representation from day/tod datum
 */
TGpsDatum
gpscal_from_gpsntp(
	TcNtpDatum *	gd
	)
{
	TGpsDatum	retv;
	vint64		ts64;
	ntpcal_split	split;

	ts64  = ntpcal_dayjoin(gd->days, gd->secs);
	ts64  = addv64u32(&ts64, (GPSNTP_DSHIFT * SECSPERDAY));
	split = ntpcal_weeksplit(&ts64);

	retv.frac  = gd->frac;
	retv.wsecs = split.lo;
	retv.weeks = split.hi;
	return retv;
}

/* -----------------------------------------------------------------
 * convert week/tow to LFP stamp
 */
l_fp
ntpfp_from_gpsdatum(
	TcGpsDatum *	gd
	)
{
	l_fp retv;

	retv.l_uf = gd->frac;
	retv.l_ui = gd->weeks * (uint32_t)SECSPERWEEK
	          + (uint32_t)gd->wsecs
	          - (uint32_t)SECSPERDAY * GPSNTP_DSHIFT;
	return retv;
}

/* -*-EOF-*- */
