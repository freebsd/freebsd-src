/* ntp_calendar.h,v 3.1 1993/07/06 01:06:48 jbj Exp
 * ntp_calendar.h - definitions for the calendar time-of-day routine
 */

#include "ntp_types.h"

struct calendar {
	u_short year;	/* year (A.D.) */
	u_short yearday;	/* day of year, 1 = January 1 */
	u_char month;	/* month, 1 = January */
	u_char monthday;	/* day of month */
	u_char hour;	/* hour of day, midnight = 0 */
	u_char minute;	/* minute of hour */
	u_char second;	/* second of minute */
};

/*
 * Days in each month.  30 days hath September...
 */
#define	JAN	31
#define	FEB	28
#define	FEBLEAP	29
#define	MAR	31
#define	APR	30
#define	MAY	31
#define	JUN	30
#define	JUL	31
#define	AUG	31
#define	SEP	30
#define	OCT	31
#define	NOV	30
#define	DEC	31

/*
 * We deal in a 4 year cycle starting at March 1, 1900.  We assume
 * we will only want to deal with dates since then, and not to exceed
 * the rollover day in 2036.
 */
#define	SECSPERMIN	(60)			/* seconds per minute */
#define	MINSPERHR	(60)			/* minutes per hour */
#define	HRSPERDAY	(24)			/* hours per day */
#define	DAYSPERYEAR	(365)			/* days per year */

#define	SECSPERDAY	(SECSPERMIN*MINSPERHR*HRSPERDAY)
#define SECSPERYEAR	(365 * SECSPERDAY)	/* regular year */
#define	SECSPERLEAPYEAR	(366 * SECSPERDAY)	/* leap year */

#define	MAR1900		((JAN+FEB) * SECSPERDAY) /* no leap year in 1900 */
#define	DAYSPERCYCLE	(365+365+365+366)	/* 3 normal years plus leap */
#define	SECSPERCYCLE	(DAYSPERCYCLE*SECSPERDAY)
#define	YEARSPERCYCLE	4

/*
 * Gross hacks.  I have illicit knowlege that there won't be overflows
 * here, the compiler often can't tell this.
 */
#define TIMES60(val)	((((val)<<4) - (val))<<2)       /* *(16 - 1) * 4 */
#define	TIMES24(val)	(((val)<<4) + ((val)<<3))	/* *16 + *8 */
#define	TIMES7(val)	(((val)<<3) - (val))            /* *8  - *1 */
#define	TIMESDPERC(val)	(((val)<<10) + ((val)<<8) \
			+ ((val)<<7) + ((val)<<5) \
			+ ((val)<<4) + ((val)<<2) + (val))	/* *big* hack */

/*
 * Another big hack.  Cycle 22 started on March 1, 1988.  This is
 * STARTCYCLE22 seconds after the start of cycle 0.
 */
#define	CYCLE22		(22)
#define	STARTCYCLE22	(U_LONG)(0xa586b500)	/* 2777068800 */
#define	MAR1988		(U_LONG)(STARTCYCLE22 + (U_LONG)MAR1900)

/*
 * The length of January + February in leap and non-leap years.
 */
#define	JANFEBNOLEAP	((JAN+FEB) * SECSPERDAY)
#define	JANFEBLEAP	((JAN+FEBLEAP) * SECSPERDAY)

extern	void	caljulian	P((U_LONG, struct calendar *));
extern	U_LONG	caltontp	P((const struct calendar *));

