/*
 * refclock_trak - clock driver for the TRAK 8820 GPS Station Clock
 *
 * Thanks to Tomoaki TSURUOKA <tsuruoka@nc.fukuoka-u.ac.jp> for the
 * previous version from which this one was developed.
 */
#if defined(REFCLOCK) && defined(TRAK)

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

/*
 * This driver supports the TRAK 8820 GPS Station Clock. The claimed
 * accuracy at the 1-pps output is 200-300 ns relative to the broadcast
 * signal; however, in most cases the actual accuracy is limited by the
 * precision of the timecode and the latencies of the serial interface
 * and operating system.
 *
 * For best accuracy, this radio requires the LDISC_ACTS line
 * discipline, which captures a timestamp at the '*' on-time character
 * of the timecode. Using this discipline the jitter is in the order of
 * 1 ms and systematic error about 0.5 ms. If unavailable, the buffer
 * timestamp is used, which is captured at the \r ending the timecode
 * message. This introduces a systematic error of 23 character times, or
 * about 24 ms at 9600 bps, together with a jitter well over 8 ms on Sun
 * IPC-class machines.
 *
 * Using the memus, the radio should be set for 9600 bps, one stop bit
 * and no parity. It should be set to operate in computer (no echo)
 * mode. The timecode format includes neither the year nor leap-second
 * warning. No provisions are included in this preliminary version of
 * the driver to read and record detailed internal radio status.
 *
 * In operation, this driver sends a RQTS\r request to the radio at
 * initialization in order to put it in continuous time output mode. The
 * radio then sends the following message once each second:
 *
 *	*RQTS U,ddd:hh:mm:ss.0,q<cr><lf>
 *
 *	on-time = '*' *	ddd = day of year
 *	hh:mm:ss = hours, minutes, seconds
 *	q = quality indicator (phase error), 0-6:
 * 		0 > 20 us
 *		6 > 10 us
 *		5 > 1 us
 *		4 > 100 ns
 *		3 > 10 ns
 *		2 < 10 ns
 *
 * The alarm condition is indicated by '0' at Q, which means the radio
 * has a phase error than 20 usec relative to the broadcast time. The
 * absence of year, DST and leap-second warning in this format is also
 * alarming.
 *
 * The continuous time mode is disabled using the RQTX<cr> request,
 * following which the radio sends a RQTX DONE<cr><lf> response. In the
 * normal mode, other control and status requests are effective,
 * including the leap-second status request RQLS<cr>. The radio responds
 * wtih RQLS yy,mm,dd<cr><lf>, where yy,mm,dd are the year, month and
 * day. Presumably, this gives the epoch of the next leap second,
 * RQLS 00,00,00 if none is specified in the GPS message. Specified in
 * this form, the information is generally useless and is ignored by
 * the driver.
 *
 * Fudge Factors
 *
 * There are no special fudge factors other than the generic.
 */

/*
 * Interface definitions
 */
#define	DEVICE		"/dev/trak%d" /* device name and unit */
#define	SPEED232	B9600	/* uart speed (9600 baud) */
#define	PRECISION	(-10)	/* precision assumed (about 1 ms) */
#define	REFID		"TRAK"	/* reference ID */
#define	DESCRIPTION	"TRACK 8810/8820 Station Clock" /* WRU */

#define	NSAMPLES	3	/* stages of median filter */
#define	LENTRAK		24	/* timecode length */
#define C_CTO		"RQTS\r" /* start continuous time output */

/*
 * Imported from ntp_timer module
 */
extern	u_long	current_time;	/* current time (s) */

/*
 * Imported from ntpd module
 */
extern	int	debug;		/* global debug flag */

/*
 * Unit control structure
 */
struct wwvbunit {
	int	pollcnt;	/* poll message counter */

	u_char	tcswitch;	/* timecode switch */
	char	qualchar;	/* quality indicator */
};

/*
 * Function prototypes
 */
static	int	trak_start	P((int, struct peer *));
static	void	trak_shutdown	P((int, struct peer *));
static	void	trak_receive	P((struct recvbuf *));
static	void	trak_poll	P((int, struct peer *));

/*
 * Transfer vector
 */
struct	refclock refclock_trak = {
	trak_start,		/* start up driver */
	trak_shutdown,		/* shut down driver */
	trak_poll,		/* transmit poll message */
	noentry,		/* not used (old trak_control) */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used (old trak_buginfo) */
	NOFLAGS			/* not used */
};


/*
 * trak_start - open the devices and initialize data for processing
 */
static int
trak_start(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct wwvbunit *up;
	struct refclockproc *pp;
	int fd;
	char device[20];

	/*
	 * Open serial port. The LDISC_ACTS line discipline inserts a
	 * timestamp following the "*" on-time character of the
	 * timecode.
	 */
	(void)sprintf(device, DEVICE, unit);
	if (!(fd = refclock_open(device, SPEED232, LDISC_ACTS)))
		return (0);

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct wwvbunit *)
	    emalloc(sizeof(struct wwvbunit)))) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct wwvbunit));
	pp = peer->procptr;
	pp->io.clock_recv = trak_receive;
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
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);
	up->pollcnt = 2;

	/*
	 * Start continuous time output. If something breaks, fold the
	 * tent and go home.
	 */
	if (write(pp->io.fd, C_CTO, sizeof(C_CTO)) != sizeof(C_CTO)) {
		refclock_report(peer, CEVNT_FAULT);
		(void) close(fd);
		free(up);
		return (0);
	}
	return (1);
}


/*
 * trak_shutdown - shut down the clock
 */
static void
trak_shutdown(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct wwvbunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct wwvbunit *)pp->unitptr;
	io_closeclock(&pp->io);
	free(up);
}


/*
 * trak_receive - receive data from the serial interface
 */
static void
trak_receive(rbufp)
	struct recvbuf *rbufp;
{
	register struct wwvbunit *up;
	struct refclockproc *pp;
	struct peer *peer;
	l_fp trtmp;
	char *dpt, *dpend;
	char qchar;

	/*
	 * Initialize pointers and read the timecode and timestamp. We
	 * then chuck out everything, including runts, except one
	 * message each poll interval.
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct wwvbunit *)pp->unitptr;
	pp->lencode = refclock_gtlin(rbufp, pp->lastcode, BMAX,
	    &pp->lastrec);
	if (up->tcswitch || pp->lencode < 9)
		return;
	up->tcswitch = 1;

	/*
	 * We get a buffer and timestamp following the '*' on-time
	 * character. If a valid timestamp, we use that in place of the
	 * buffer timestamp and edit out the timestamp for prettyprint
	 * billboards.
	 */
	dpt = pp->lastcode;
	dpend = dpt + pp->lencode;
	if (*dpt == '*' && buftvtots(dpt + 1, &trtmp)) {
		if (trtmp.l_i == pp->lastrec.l_i || trtmp.l_i ==
		    pp->lastrec.l_i + 1) {
			pp->lastrec = trtmp;
			dpt += 9;
			while (dpt < dpend)
				*(dpt - 8) = *dpt++;
		}
	}
	up->pollcnt = 2;
	record_clock_stats(&peer->srcadr, pp->lastcode);
#ifdef DEBUG
	if (debug)
        	printf("trak: timecode %d %s\n", pp->lencode,
		    pp->lastcode);
#endif

	/*
	 * We get down to business, check the timecode format and decode
	 * its contents. If the timecode has invalid length or is not in
	 * proper format, we declare bad format and exit.
	 */
	if (pp->lencode < LENTRAK) {
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

	/*
	 * Timecode format: "*RQTS U,ddd:hh:mm:ss.0,q"
 	 */
	if (sscanf(pp->lastcode, "*RQTS U,%3d:%2d:%2d:%2d.0,%c",
	    &pp->day, &pp->hour, &pp->minute, &pp->second, &qchar) != 5) {
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

	/*
	 * Decode quality and leap characters. If unsynchronized, set
	 * the leap bits accordingly and exit.
	 */
	if (qchar == '0')
		pp->leap = LEAP_NOTINSYNC;
	else {
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


/*
 * trak_poll - called by the transmit procedure
 */
static void
trak_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct wwvbunit *up;
	struct refclockproc *pp;

	/*
	 * We don't really do anything here, except arm the receiving
	 * side to capture a sample and check for timeouts.
	 */
	pp = peer->procptr;
	up = (struct wwvbunit *)pp->unitptr;
	if (up->pollcnt == 0)
		refclock_report(peer, CEVNT_TIMEOUT);
	else
		up->pollcnt--;
	up->tcswitch = 0;
	pp->polls++;
}

#endif
