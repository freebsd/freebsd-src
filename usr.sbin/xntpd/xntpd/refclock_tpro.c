/*
 * refclock_tpro - clock driver for the KSI/Odetics TPRO-S IRIG-B reader
 */
#if defined(REFCLOCK) && defined(TPRO) && defined(sun)

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"
#include "sys/tpro.h"
#include "ntp_stdlib.h"

/*
 * This driver supports the KSI/Odetecs TPRO-S IRIG-B reader and TPRO-
 * SAT GPS receiver for the Sun Microsystems SBus. It requires that the
 * tpro.o device driver be installed and loaded.
 */ 

/*
 * TPRO interface definitions
 */
#define	DEVICE		 "/dev/tpro%d" /* device name and unit */
#define	PRECISION	(-20)	/* precision assumed (1 us) */
#define	REFID		"IRIG"	/* reference ID */
#define	DESCRIPTION	"KSI/Odetics TPRO/S IRIG Interface" /* WRU */

#define	NSAMPLES	3	/* stages of median filter */

/*
 * Imported from ntp_timer module
 */
extern u_long current_time;	/* current time (s) */

/*
 * Imported from ntpd module
 */
extern int debug;		/* global debug flag */

/*
 * Unit control structure
 */
struct tprounit {
	struct	tproval tprodata; /* data returned from tpro read */
};

/*
 * Function prototypes
 */
static	int	tpro_start	P((int, struct peer *));
static	void	tpro_shutdown	P((int, struct peer *));
static	void	tpro_poll	P((int unit, struct peer *));

/*
 * Transfer vector
 */
struct	refclock refclock_tpro = {
	tpro_start,		/* start up driver */
	tpro_shutdown,		/* shut down driver */
	tpro_poll,		/* transmit poll message */
	noentry,		/* not used (old tpro_control) */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used (old tpro_buginfo) */
	NOFLAGS			/* not used */
};


/*
 * tpro_start - open the TPRO device and initialize data for processing
 */
static int
tpro_start(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct tprounit *up;
	struct refclockproc *pp;
	char device[20];
	int fd;

	/*
	 * Open TPRO device
	 */
	(void)sprintf(device, DEVICE, unit);
	fd = open(device, O_RDONLY | O_NDELAY, 0777);
	if (fd == -1) {
		syslog(LOG_ERR, "tpro_start: open of %s: %m", device);
		return (0);
	}

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct tprounit *)
	    emalloc(sizeof(struct tprounit)))) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct tprounit));
	pp = peer->procptr;
	pp->io.clock_recv = noentry;
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
	 * Initialize miscellaneous peer variables
	 */
	peer->precision = PRECISION;
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);
	return (1);
}


/*
 * tpro_shutdown - shut down the clock
 */
static void
tpro_shutdown(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct tprounit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct tprounit *)pp->unitptr;
	io_closeclock(&pp->io);
	free(up);
}


/*
 * tpro_poll - called by the transmit procedure
 */
static void
tpro_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct tprounit *up;
	struct refclockproc *pp;
	struct tproval *tp;

	/*
	 * This is the main routine. It snatches the time from the TPRO
	 * board and tacks on a local timestamp.
	 */
	pp = peer->procptr;
	up = (struct tprounit *)pp->unitptr;

	tp = &up->tprodata;
	if (read(pp->io.fd, (char *)tp, sizeof(struct tproval)) < 0) {
		refclock_report(peer, CEVNT_FAULT);
		return;
	}
	gettstamp(&pp->lastrec);
	pp->lasttime = current_time;
	pp->polls++;

	/*
	 * We get down to business, check the timecode format and decode
	 * its contents. If the timecode has invalid length or is not in
	 * proper format, we declare bad format and exit. Note: we
	 * can't use the sec/usec conversion produced by the driver,
	 * since the year may be suspect. All format error checking is
	 * done by the sprintf() and sscanf() routines.
	 */
	if (sprintf(pp->lastcode,
	    "%1x%1x%1x %1x%1x:%1x%1x:%1x%1x.%1x%1x%1x%1x%1x%1x %1x",
	    tp->day100, tp->day10, tp->day1, tp->hour10, tp->hour1,
	    tp->min10, tp->min1, tp->sec10, tp->sec1, tp->ms100,
	    tp->ms10, tp->ms1, tp->usec100, tp->usec10, tp->usec1,
	    tp->status)) {
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

#ifdef DEBUG
	if (debug)
		printf("tpro: time %s timecode %d %s\n",
		    ulfptoa(&pp->lastrec, 6), pp->lencode,
		    pp->lastcode);
#endif
	record_clock_stats(&peer->srcadr, pp->lastcode);
	if (sscanf(pp->lastcode, "%3d %2d:%2d:%2d.%6ld", &pp->day,
	    &pp->hour, &pp->minute, &pp->second, &pp->usec)
	    != 5) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}
	if (tp->status != 0xff) {
		pp->leap = LEAP_NOTINSYNC;
		refclock_report(peer, CEVNT_FAULT);
                return;
	} else {
		pp->leap = 0;
		pp->lasttime = current_time;
	}

	/*
	 * Process the new sample in the median filter and determine the
	 * reference clock offset and dispersion. We use lastrec as both
	 * the reference time and receive time in order to avoid being
	 * cute, like setting the reference time later than the receive
	 * time, which may cause a paranoid protocol module to chuck out
	 * the data.
 	 */
	if (!refclock_process(pp, NSAMPLES, NSAMPLES)) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
	}
	refclock_receive(peer, &pp->offset, 0, pp->dispersion,
	    &pp->lastrec, &pp->lastrec, pp->leap);
}

#endif
