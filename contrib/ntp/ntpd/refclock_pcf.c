/*
 * refclock_pcf - clock driver for the Conrad parallel port radio clock
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_PCF)

#include <time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"

/*
 * This driver supports the parallel port radio clocks sold by Conrad
 * Electronic under order numbers 967602 and 642002.
 *
 * It requires that the local timezone be CET/CEST and that the pcfclock
 * device driver be installed.  A device driver for Linux 2.2 is available
 * at http://home.pages.de/~voegele/pcf.html.
 */

/*
 * Interface definitions
 */
#define	DEVICE		"/dev/pcfclock%d"
#define	PRECISION	(-1)	/* precision assumed (about 0.5 s) */
#define REFID		"PCF"
#define DESCRIPTION	"Conrad parallel port radio clock"

#define LENPCF		18	/* timecode length */

/*
 * Function prototypes
 */
static	int 	pcf_start 		P((int, struct peer *));
static	void	pcf_shutdown		P((int, struct peer *));
static	void	pcf_poll		P((int, struct peer *));

/*
 * Transfer vector
 */
struct  refclock refclock_pcf = {
	pcf_start,              /* start up driver */
	pcf_shutdown,           /* shut down driver */
	pcf_poll,               /* transmit poll message */
	noentry,                /* not used */
	noentry,                /* initialize driver (not used) */
	noentry,                /* not used */
	NOFLAGS                 /* not used */
};


/*
 * pcf_start - open the device and initialize data for processing
 */
static int
pcf_start(
     	int unit,
	struct peer *peer
	)
{
	struct refclockproc *pp;
	int fd;
	char device[20];

	/*
	 * Open device file for reading.
	 */
	(void)sprintf(device, DEVICE, unit);
#ifdef DEBUG
	if (debug)
		printf ("starting PCF with device %s\n",device);
#endif
	if ((fd = open(device, O_RDONLY)) == -1) {
		return (0);
	}
	
	pp = peer->procptr;
	pp->io.clock_recv = noentry;
	pp->io.srcclock = (caddr_t)peer;
	pp->io.datalen = 0;
	pp->io.fd = fd;
	
	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = PRECISION;
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);

	return (1);
}


/*
 * pcf_shutdown - shut down the clock
 */
static void
pcf_shutdown(
	int unit,
	struct peer *peer
	)
{
	struct refclockproc *pp;
	
	pp = peer->procptr;
	(void)close(pp->io.fd);
}


/*
 * pcf_poll - called by the transmit procedure
 */
static void
pcf_poll(
	int unit,
	struct peer *peer
	)
{
	struct refclockproc *pp;
	char buf[LENPCF];
	struct tm tm, *tp;
	time_t t;
	
	pp = peer->procptr;

	buf[0] = 0;
	if (read(pp->io.fd, buf, sizeof(buf)) < sizeof(buf) || buf[0] != 9) {
		refclock_report(peer, CEVNT_FAULT);
		return;
	}

	tm.tm_mday = buf[11] * 10 + buf[10];
	tm.tm_mon = buf[13] * 10 + buf[12] - 1;
	tm.tm_year = buf[15] * 10 + buf[14];
	tm.tm_hour = buf[7] * 10 + buf[6];
	tm.tm_min = buf[5] * 10 + buf[4];
	tm.tm_sec = buf[3] * 10 + buf[2];
	tm.tm_isdst = -1;

	/*
	 * Y2K convert the 2-digit year
	 */
	if (tm.tm_year < 99)
		tm.tm_year += 100;
	
	t = mktime(&tm);
	if (t == (time_t) -1) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}

#if defined(__GLIBC__) && defined(_BSD_SOURCE)
	if ((tm.tm_isdst > 0 && tm.tm_gmtoff != 7200)
	    || (tm.tm_isdst == 0 && tm.tm_gmtoff != 3600)
	    || tm.tm_isdst < 0) {
#ifdef DEBUG
		if (debug)
			printf ("local time zone not set to CET/CEST\n");
#endif
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}
#endif

	pp->lencode = strftime(pp->a_lastcode, BMAX, "%Y %m %d %H %M %S", &tm);

#if defined(_REENTRANT) || defined(_THREAD_SAFE)
	tp = gmtime_r(&t, &tm);
#else
	tp = gmtime(&t);
#endif
	if (!tp) {
		refclock_report(peer, CEVNT_FAULT);
		return;
	}

	get_systime(&pp->lastrec);
	pp->polls++;
	pp->year = tp->tm_year + 1900;
	pp->day = tp->tm_yday + 1;
	pp->hour = tp->tm_hour;
	pp->minute = tp->tm_min;
	pp->second = tp->tm_sec;
	pp->usec = buf[16] * 31250;
	if (buf[17] & 1)
		pp->usec += 500000;

#ifdef DEBUG
	if (debug)
		printf ("pcf%d: time is %04d/%02d/%02d %02d:%02d:%02d UTC\n",
			unit, pp->year, tp->tm_mon + 1, tp->tm_mday, pp->hour,
			pp->minute, pp->second);
#endif

	if (!refclock_process(pp)) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}
	record_clock_stats(&peer->srcadr, pp->a_lastcode);
	if (buf[1] & 1)
		pp->leap = LEAP_NOTINSYNC;
	else
		pp->leap = LEAP_NOWARNING;
	refclock_receive(peer);
}
#else
int refclock_pcf_bs;
#endif /* REFCLOCK */
