/* ntp_unixtime.h,v 3.1 1993/07/06 01:07:02 jbj Exp
 * ntp_unixtime.h - contains constants and macros for converting between
 *		    NTP time stamps (l_fp) and Unix times (struct timeval)
 */

#include "ntp_types.h"
#include <sys/time.h>

/* gettimeofday() takes two args in BSD and only one in SYSV */
#ifdef SYSV_TIMEOFDAY
#  define GETTIMEOFDAY(a, b) (gettimeofday(a))
#  define SETTIMEOFDAY(a, b) (settimeofday(a))
#else /* ! SYSV_TIMEOFDAY */
#  define GETTIMEOFDAY(a, b) (gettimeofday(a, b))
#  define SETTIMEOFDAY(a, b) (settimeofday(a, b))
#endif /* SYSV_TIMEOFDAY */

/*
 * Time of day conversion constant.  Ntp's time scale starts in 1900,
 * Unix in 1970.
 */
#define	JAN_1970	0x83aa7e80	/* 2208988800 1970 - 1900 in seconds */

/*
 * These constants are used to round the time stamps computed from
 * a struct timeval to the microsecond (more or less).  This keeps
 * things neat.
 */
#define	TS_MASK		0xfffff000	/* mask to usec, for time stamps */
#define	TS_ROUNDBIT	0x00000800	/* round at this bit */


/*
 * Convert usec to a time stamp fraction.  If you use this the program
 * must include the following declarations:
 */
extern U_LONG ustotslo[];
extern U_LONG ustotsmid[];
extern U_LONG ustotshi[];

#define	TVUTOTSF(tvu, tsf) \
	(tsf) = ustotslo[(tvu) & 0xff] \
	    + ustotsmid[((tvu) >> 8) & 0xff] \
	    + ustotshi[((tvu) >> 16) & 0xf]

/*
 * Convert a struct timeval to a time stamp.
 */
#define TVTOTS(tv, ts) \
	do { \
		(ts)->l_ui = (unsigned LONG)(tv)->tv_sec; \
		TVUTOTSF((tv)->tv_usec, (ts)->l_uf); \
	} while(0)

#define sTVTOTS(tv, ts) \
	do { \
		int isneg = 0; \
		LONG usec; \
		(ts)->l_ui = (tv)->tv_sec; \
		usec = (tv)->tv_usec; \
		if (((tv)->tv_sec < 0) || ((tv)->tv_usec < 0)) { \
			usec = -usec; \
			(ts)->l_ui = -(ts)->l_ui; \
			isneg = 1; \
		} \
		TVUTOTSF(usec, (ts)->l_uf); \
		if (isneg) { \
			L_NEG((ts)); \
		} \
	} while(0)

/*
 * TV_SHIFT is used to turn the table result into a usec value.  To round,
 * add in TV_ROUNDBIT before shifting
 */
#define	TV_SHIFT	3
#define	TV_ROUNDBIT	0x4


/*
 * Convert a time stamp fraction to microseconds.  The time stamp
 * fraction is assumed to be unsigned.  To use this in a program, declare:
 */
extern LONG tstouslo[];
extern LONG tstousmid[];
extern LONG tstoushi[];

#define	TSFTOTVU(tsf, tvu) \
	(tvu) = (tstoushi[((tsf) >> 24) & 0xff] \
	    + tstousmid[((tsf) >> 16) & 0xff] \
	    + tstouslo[((tsf) >> 9) & 0x7f] \
	    + TV_ROUNDBIT) >> TV_SHIFT
/*
 * Convert a time stamp to a struct timeval.  The time stamp
 * has to be positive.
 */
#define	TSTOTV(ts, tv) \
	do { \
		(tv)->tv_sec = (ts)->l_ui; \
		TSFTOTVU((ts)->l_uf, (tv)->tv_usec); \
		if ((tv)->tv_usec == 1000000) { \
			(tv)->tv_sec++; \
			(tv)->tv_usec = 0; \
		} \
	} while (0)

/*
 * Convert milliseconds to a time stamp fraction.  This shouldn't be
 * here, but it is convenient since the guys who use the definition will
 * often be including this file anyway.
 */
extern U_LONG msutotsflo[];
extern U_LONG msutotsfhi[];

#define	MSUTOTSF(msu, tsf) \
	(tsf) = msutotsfhi[((msu) >> 5) & 0x1f] + msutotsflo[(msu) & 0x1f]

extern	char *	tvtoa		P((const struct timeval *));
extern	char *	utvtoa		P((const struct timeval *));
