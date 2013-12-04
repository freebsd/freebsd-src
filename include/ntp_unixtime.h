/*
 * ntp_unixtime.h - contains constants and macros for converting between
 *		    NTP time stamps (l_fp) and Unix times (struct timeval)
 */

#include "ntp_types.h"

#ifdef SIM
#include "ntpsim.h"
#endif

#ifdef SIM
#   define GETTIMEOFDAY(a, b) (node_gettime(&ntp_node, a))
#   define SETTIMEOFDAY(a, b) (node_settime(&ntp_node, a))
#   define ADJTIMEOFDAY(a, b) (node_adjtime(&ntp_node, a, b))
#else
#   define ADJTIMEOFDAY(a, b) (adjtime(a, b))
/* gettimeofday() takes two args in BSD and only one in SYSV */
# if defined(HAVE_SYS_TIMERS_H) && defined(HAVE_GETCLOCK)
#  include <sys/timers.h>
int getclock (int clock_type, struct timespec *tp);
/* Don't #define GETTIMEOFDAY because we shouldn't be using it in this case. */
#   define SETTIMEOFDAY(a, b) (settimeofday(a, b))
# else /* not (HAVE_SYS_TIMERS_H && HAVE_GETCLOCK) */
#  ifdef SYSV_TIMEOFDAY
#   define GETTIMEOFDAY(a, b) (gettimeofday(a))
#   define SETTIMEOFDAY(a, b) (settimeofday(a))
#  else /* ! SYSV_TIMEOFDAY */
#if defined SYS_CYGWIN32
#   define GETTIMEOFDAY(a, b) (gettimeofday(a, b))
#   define SETTIMEOFDAY(a, b) (settimeofday_NT(a))
#else
#   define GETTIMEOFDAY(a, b) (gettimeofday(a, b))
#   define SETTIMEOFDAY(a, b) (settimeofday(a, b))
#endif
#  endif /* SYSV_TIMEOFDAY */
# endif /* not (HAVE_SYS_TIMERS_H && HAVE_GETCLOCK) */
#endif /* SIM */

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
extern u_long ustotslo[];
extern u_long ustotsmid[];
extern u_long ustotshi[];

#define	TVUTOTSF(tvu, tsf) \
	(tsf) = ustotslo[(tvu) & 0xff] \
	    + ustotsmid[((tvu) >> 8) & 0xff] \
	    + ustotshi[((tvu) >> 16) & 0xf]

/*
 * Convert a struct timeval to a time stamp.
 */
#define TVTOTS(tv, ts) \
	do { \
		(ts)->l_ui = (u_long)(tv)->tv_sec; \
		TVUTOTSF((tv)->tv_usec, (ts)->l_uf); \
	} while(0)

#define sTVTOTS(tv, ts) \
	do { \
		int isneg = 0; \
		long usec; \
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
extern long tstouslo[];
extern long tstousmid[];
extern long tstoushi[];

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
extern u_long msutotsflo[];
extern u_long msutotsfhi[];

#define	MSUTOTSF(msu, tsf) \
	(tsf) = msutotsfhi[((msu) >> 5) & 0x1f] + msutotsflo[(msu) & 0x1f]
