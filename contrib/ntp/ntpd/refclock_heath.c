/*
 * refclock_heath - clock driver for Heath GC-1000 Most Accurate Clock
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_HEATH)

#include <stdio.h>
#include <ctype.h>
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef TM_IN_SYS_TIME
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif /* not HAVE_SYS_IOCTL_H */

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

/*
 * This driver supports the Heath GC-1000 Most Accurate Clock, with
 * RS232C Output Accessory. This is a WWV/WWVH receiver somewhat less
 * robust than other supported receivers. Its claimed accuracy is 100 ms
 * when actually synchronized to the broadcast signal, but this doesn't
 * happen even most of the time, due to propagation conditions, ambient
 * noise sources, etc. When not synchronized, the accuracy is at the
 * whim of the internal clock oscillator, which can wander into the
 * sunset without warning. Since the indicated precision is 100 ms,
 * expect a host synchronized only to this thing to wander to and fro,
 * occasionally being rudely stepped when the offset exceeds the default
 * clock_max of 128 ms. 
 *
 * The internal DIPswitches should be set to operate at 1200 baud in
 * MANUAL mode and the current year. The external DIPswitches should be
 * set to GMT and 24-hour format, or to the host local time zone (with
 * DST) and 12-hour format. It is very important that the year be
 * set correctly in the DIPswitches. Otherwise, the day of year will be
 * incorrect after 28 April[?] of a normal or leap year.  In 12-hour mode
 * with DST selected the clock will be incorrect by an hour for an
 * indeterminate amount of time between 0000Z and 0200 on the day DST
 * changes.
 *
 * In MANUAL mode the clock responds to a rising edge of the request to
 * send (RTS) modem control line by sending the timecode. Therefore, it
 * is necessary that the operating system implement the TIOCMBIC and
 * TIOCMBIS ioctl system calls and TIOCM_RTS control bit. Present
 * restrictions require the use of a POSIX-compatible programming
 * interface, although other interfaces may work as well.
 *
 * A simple hardware modification to the clock can be made which
 * prevents the clock hearing the request to send (RTS) if the HI SPEC
 * lamp is out. Route the HISPEC signal to the tone decoder board pin
 * 19, from the display, pin 19. Isolate pin 19 of the decoder board
 * first, but maintain connection with pin 10. Also isolate pin 38 of
 * the CPU on the tone board, and use half an added 7400 to gate the
 * original signal to pin 38 with that from pin 19.
 *
 * The clock message consists of 23 ASCII printing characters in the
 * following format:
 *
 * hh:mm:ss.f AM  dd/mm/yr<cr>
 *
 *	hh:mm:ss.f = hours, minutes, seconds
 *	f = deciseconds ('?' when out of spec)
 *	AM/PM/bb = blank in 24-hour mode
 *	dd/mm/yr = day, month, year
 *
 * The alarm condition is indicated by '?', rather than a digit, at f.
 * Note that 0?:??:??.? is displayed before synchronization is first
 * established and hh:mm:ss.? once synchronization is established and
 * then lost again for about a day.
 *
 * Fudge Factors
 *
 * A fudge time1 value of .04 s appears to center the clock offset
 * residuals. The fudge time2 parameter is the local time offset east of
 * Greenwich, which depends on DST. Sorry about that, but the clock
 * gives no hint on what the DIPswitches say.
 */

/*
 * Interface definitions
 */
#define	DEVICE		"/dev/heath%d" /* device name and unit */
#define	SPEED232	B1200	/* uart speed (1200 baud) */
#define	PRECISION	(-4)	/* precision assumed (about 100 ms) */
#define	REFID		"WWV\0"	/* reference ID */
#define	DESCRIPTION	"Heath GC-1000 Most Accurate Clock" /* WRU */

#define LENHEATH	23	/* min timecode length */

/*
 * Tables to compute the ddd of year form icky dd/mm timecode. Viva la
 * leap.
 */
static int day1tab[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static int day2tab[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/*
 * Unit control structure
 */
struct heathunit {
	int	pollcnt;	/* poll message counter */
	l_fp	tstamp;		/* timestamp of last poll */
};

/*
 * Function prototypes
 */
static	int	heath_start	P((int, struct peer *));
static	void	heath_shutdown	P((int, struct peer *));
static	void	heath_receive	P((struct recvbuf *));
static	void	heath_poll	P((int, struct peer *));

/*
 * Transfer vector
 */
struct	refclock refclock_heath = {
	heath_start,		/* start up driver */
	heath_shutdown,		/* shut down driver */
	heath_poll,		/* transmit poll message */
	noentry,		/* not used (old heath_control) */
	noentry,		/* initialize driver */
	noentry,		/* not used (old heath_buginfo) */
	NOFLAGS			/* not used */
};

#if 0
/*
 * Gee, Unix so thoughfully omitted code to convert from a struct tm to
 * a long, so I'll just have to ferret out the inverse myself, the hard way.
 * (Newton's method.)
 */
#define timelocal(x) invert(x, localtime)
/*
 * comparetm compares two tm structures and returns -1 if the first
 * is less than the second, 0 if they are equal, and +1 if the first
 * is greater than the second.  Only the year, month, day, hour, minute
 * and second are compared.  The yearday (Julian), day of week, and isdst
 * are not compared.
 */

static int
comparetm(
	struct tm *a,
	struct tm *b
	)
{
	if (a->tm_year < b->tm_year ) return -1;
	if (a->tm_year > b->tm_year ) return 1;
	if (a->tm_mon < b->tm_mon ) return -1;
	if (a->tm_mon > b->tm_mon ) return 1;
	if (a->tm_mday < b->tm_mday ) return -1;
	if (a->tm_mday > b->tm_mday ) return 1;
	if (a->tm_hour < b->tm_hour ) return -1;
	if (a->tm_hour > b->tm_hour ) return 1;
	if (a->tm_min < b->tm_min ) return -1;
	if (a->tm_min > b->tm_min ) return 1;
	if (a->tm_sec < b->tm_sec ) return -1;
	if (a->tm_sec > b->tm_sec ) return 1;
	return 0;
}

static long
invert (
       struct tm *x,
       struct tm *(*func)()
       )
{
	struct tm *y;
	int result;
	long trial;
	long lower=0L;
	long upper=(long)((unsigned long)(~lower) >> 1);

	do {
		trial = (upper + lower) / 2L;
		y = (*func)(&trial);
		result = comparetm(x, y);
		if (result < 0) upper = trial;
		if (result > 0) lower = trial;
	} while (result != 0);
	return trial;
}
#endif /* 0 */


/*
 * heath_start - open the devices and initialize data for processing
 */
static int
heath_start(
	int unit,
	struct peer *peer
	)
{
	register struct heathunit *up;
	struct refclockproc *pp;
	int fd;
	char device[20];

	/*
	 * Open serial port
	 */
	(void)sprintf(device, DEVICE, unit);
	if (!(fd = refclock_open(device, SPEED232, 0)))
	    return (0);

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct heathunit *)
	      emalloc(sizeof(struct heathunit)))) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct heathunit));
	pp = peer->procptr;
	pp->io.clock_recv = heath_receive;
	pp->io.srcclock = (caddr_t)peer;
	pp->io.datalen = 0;
	pp->io.fd = fd;
	if (!io_addclock(&pp->io)) {
		(void) close(fd);
		free(up);
		return (0);
	}
	pp->unitptr = (caddr_t)up;

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = PRECISION;
	peer->burst = NSTAGE;
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);
	up->pollcnt = 2;
	return (1);
}


/*
 * heath_shutdown - shut down the clock
 */
static void
heath_shutdown(
	int unit,
	struct peer *peer
	)
{
	register struct heathunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct heathunit *)pp->unitptr;
	io_closeclock(&pp->io);
	free(up);
}


/*
 * heath_receive - receive data from the serial interface
 */
static void
heath_receive(
	struct recvbuf *rbufp
	)
{
	register struct heathunit *up;
	struct refclockproc *pp;
	struct peer *peer;
	l_fp trtmp;
	int month, day;
	int i;
	char dsec, a[5];

	/*
	 * Initialize pointers and read the timecode and timestamp
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct heathunit *)pp->unitptr;
	pp->lencode = refclock_gtlin(rbufp, pp->a_lastcode, BMAX, &trtmp);

	/*
	 * We get a buffer and timestamp for each <cr>; however, we use
	 * the timestamp captured at the RTS modem control line toggle
	 * on the assumption that's what the radio bases the timecode
	 * on. Apparently, the radio takes about a second to make up its
	 * mind to send a timecode, so the receive timestamp is
	 * worthless.
	 */
	pp->lastrec = up->tstamp;
	up->pollcnt = 2;
#ifdef DEBUG
	if (debug)
	    printf("heath: timecode %d %s\n", pp->lencode,
		   pp->a_lastcode);
#endif

	/*
	 * We get down to business, check the timecode format and decode
	 * its contents. If the timecode has invalid length or is not in
	 * proper format, we declare bad format and exit.
	 */
	if (pp->lencode < LENHEATH) {
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

	/*
	 * Timecode format: "hh:mm:ss.f AM  mm/dd/yy"
	 */
	if (sscanf(pp->a_lastcode, "%2d:%2d:%2d.%c%5c%2d/%2d/%2d",
		   &pp->hour, &pp->minute, &pp->second, &dsec, a, &month, &day,
		   &pp->year) != 8) {
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

	/*
	 * If AM or PM is received, assume the clock is displaying local
	 * time. First, convert to 24-hour format.
	 */

	switch (a[1]) {
	    case 'P':
		if (12 > pp->hour)
		    pp->hour += 12;
		break;

	    case 'A':
		if (12 == pp->hour)
		    pp->hour -= 12;
		break;
	}

	/*
	 * Now make a struct tm out of it, convert to UTC, and
	 * repopulate pp->
	 */

	if (' ' != a[1]) {
		struct tm t, *q;
		time_t l;

		t.tm_sec = pp->second;
		t.tm_min = pp->minute;
		t.tm_hour = pp->hour;
		t.tm_mday = day; /* not converted to yday yet */
		t.tm_mon = month-1; /* ditto */
		t.tm_year = pp->year;
		if ( t.tm_year < YEAR_PIVOT ) t.tm_year += 100;	/* Y2KFixes */

		t.tm_wday = -1; /* who knows? */
		t.tm_yday = -1; /* who knows? */
		t.tm_isdst = -1; /* who knows? */

		l = mktime(&t);
		if (l == -1) {
			/* HMS: do we want to do this? */
			refclock_report(peer, CEVNT_BADTIME);
			return;
		}
		q = gmtime(&l);

		pp->year = q->tm_year;
		month = q->tm_mon+1;
		day = q->tm_mday; /* still not converted */
		pp->hour = q->tm_hour;
		/* pp->minute = q->tm_min;  GC-1000 cannot adjust timezone */
		/* pp->second = q->tm_sec;  by other than hour increments */
	}

  

	/*
	 * We determine the day of the year from the DIPswitches. This
	 * should be fixed, since somebody might forget to set them.
	 * Someday this hazard will be fixed by a fiendish scheme that
	 * looks at the timecode and year the radio shows, then computes
	 * the residue of the seconds mod the seconds in a leap cycle.
	 * If in the third year of that cycle and the third and later
	 * months of that year, add one to the day. Then, correct the
	 * timecode accordingly. Icky pooh. This bit of nonsense could
	 * be avoided if the engineers had been required to write a
	 * device driver before finalizing the timecode format.
	 *
	 * Yes, I know this code incorrectly thinks that 2000 is a leap
	 * year; but, the latest year that can be set by the DIPswitches
	 * is 1997 anyay. Life is short.
	 *	Hey! Year 2000 IS a leap year!			   Y2KFixes
	 */
	if (month < 1 || month > 12 || day < 1) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}
	if (pp->year % 4) {
		if (day > day1tab[month - 1]) {
			refclock_report(peer, CEVNT_BADTIME);
			return;
		}
		for (i = 0; i < month - 1; i++)
		    day += day1tab[i];
	} else {
		if (day > day2tab[month - 1]) {
			refclock_report(peer, CEVNT_BADTIME);
			return;
		}
		for (i = 0; i < month - 1; i++)
		    day += day2tab[i];
	}
	pp->day = day;

	/*
	 * Determine synchronization and last update
	 */
	if (!isdigit((int)dsec))
		pp->leap = LEAP_NOTINSYNC;
	else {
		pp->msec = (dsec - '0') * 100;
		pp->leap = LEAP_NOWARNING;
	}
	if (!refclock_process(pp))
		refclock_report(peer, CEVNT_BADTIME);
}


/*
 * heath_poll - called by the transmit procedure
 */
static void
heath_poll(
	int unit,
	struct peer *peer
	)
{
	register struct heathunit *up;
	struct refclockproc *pp;
	int bits = TIOCM_RTS;

	/*
	 * At each poll we check for timeout and toggle the RTS modem
	 * control line, then take a timestamp. Presumably, this is the
	 * event the radio captures to generate the timecode.
	 */
	pp = peer->procptr;
	up = (struct heathunit *)pp->unitptr;
	pp->polls++;

	/*
	 * We toggle the RTS modem control lead to kick a timecode loose
	 * from the radio. This code works only for POSIX and SYSV
	 * interfaces. With bsd you are on your own. We take a timestamp
	 * between the up and down edges to lengthen the pulse, which
	 * should be about 50 usec on a Sun IPC. With hotshot CPUs, the
	 * pulse might get too short. Later.
	 */
	if (ioctl(pp->io.fd, TIOCMBIC, (char *)&bits) < 0)
		refclock_report(peer, CEVNT_FAULT);
	get_systime(&up->tstamp);
	ioctl(pp->io.fd, TIOCMBIS, (char *)&bits);
	if (peer->burst > 0)
		return;
	if (pp->coderecv == pp->codeproc) {
		refclock_report(peer, CEVNT_TIMEOUT);
		return;
	}
	record_clock_stats(&peer->srcadr, pp->a_lastcode);
	refclock_receive(peer);
	peer->burst = NSTAGE;
}

#else
int refclock_heath_bs;
#endif /* REFCLOCK */
