/*
 * refclock_trak - clock driver for the TRAK 8810 GPS Station Clock
 *
 * Tomoaki TSURUOKA <tsuruoka@nc.fukuoka-u.ac.jp> 
 *	original version  Dec, 1993
 *	revised  version  Sep, 1994 for ntp3.4e or later
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_TRAK)

#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"
#include "ntp_unixtime.h"

#ifdef HAVE_SYS_TERMIOS_H
# include <sys/termios.h>
#endif
#ifdef HAVE_SYS_PPSCLOCK_H
# include <sys/ppsclock.h>
#endif

/*
 * This driver supports the TRAK 8810/8820 GPS Station Clock. The claimed
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
#define	PRECISION	(-20)	/* precision assumed (about 1 us) */
#define	REFID		"GPS\0"	/* reference ID */
#define	DESCRIPTION	"TRACK 8810/8820 Station Clock" /* WRU */

#define	LENTRAK		24	/* timecode length */
#define C_CTO		"RQTS\r" /* start continuous time output */

/*
 * Unit control structure
 */
struct trakunit {
	int	polled;		/* poll message flag */
	l_fp    tstamp;         /* timestamp of last poll */
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
trak_start(
	int unit,
	struct peer *peer
	)
{
	register struct trakunit *up;
	struct refclockproc *pp;
	int fd;
	char device[20];

	/*
	 * Open serial port. The LDISC_ACTS line discipline inserts a
	 * timestamp following the "*" on-time character of the
	 * timecode.
	 */
	(void)sprintf(device, DEVICE, unit);
	if (
#ifdef PPS
		!(fd = refclock_open(device, SPEED232, LDISC_CLK))
#else
		!(fd = refclock_open(device, SPEED232, 0))
#endif /* PPS */
		)
	    return (0);

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct trakunit *)
	      emalloc(sizeof(struct trakunit)))) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct trakunit));
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
	up->polled = 0;

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
trak_shutdown(
	int unit,
	struct peer *peer
	)
{
	register struct trakunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct trakunit *)pp->unitptr;
	io_closeclock(&pp->io);
	free(up);
}


/*
 * trak_receive - receive data from the serial interface
 */
static void
trak_receive(
	struct recvbuf *rbufp
	)
{
	register struct trakunit *up;
	struct refclockproc *pp;
	struct peer *peer;
	l_fp trtmp;
	char *dpt, *dpend;
	char qchar;
#ifdef PPS
	struct ppsclockev ppsev;
	int request;
#ifdef HAVE_CIOGETEV
        request = CIOGETEV;
#endif
#ifdef HAVE_TIOCGPPSEV
        request = TIOCGPPSEV;
#endif
#endif /* PPS */

	/*
	 * Initialize pointers and read the timecode and timestamp. We
	 * then chuck out everything, including runts, except one
	 * message each poll interval.
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct trakunit *)pp->unitptr;
	pp->lencode = refclock_gtlin(rbufp, pp->a_lastcode, BMAX,
				     &pp->lastrec);

	/*
	 * We get a buffer and timestamp following the '*' on-time
	 * character. If a valid timestamp, we use that in place of the
	 * buffer timestamp and edit out the timestamp for prettyprint
	 * billboards.
	 */
	dpt = pp->a_lastcode;
	dpend = dpt + pp->lencode;
	if (*dpt == '*' && buftvtots(dpt + 1, &trtmp)) {
		if (trtmp.l_i == pp->lastrec.l_i || trtmp.l_i ==
		    pp->lastrec.l_i + 1) {
			pp->lastrec = trtmp;
			dpt += 9;
			while (dpt < dpend) {
				*(dpt - 8) = *dpt;
				++dpt;
			}
		}
	}
	if (up->polled == 0) return;
	up->polled = 0;
#ifndef PPS
	get_systime(&up->tstamp);
#endif
	record_clock_stats(&peer->srcadr, pp->a_lastcode);
#ifdef DEBUG
	if (debug)
	    printf("trak: timecode %d %s\n", pp->lencode,
		   pp->a_lastcode);
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
	if (sscanf(pp->a_lastcode, "*RQTS U,%3d:%2d:%2d:%2d.0,%c",
		   &pp->day, &pp->hour, &pp->minute, &pp->second, &qchar) != 5) {
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

	/*
	 * Decode quality and leap characters. If unsynchronized, set
	 * the leap bits accordingly and exit.
	 */
	if (qchar == '0') {
		pp->leap = LEAP_NOTINSYNC;
		return;
	}
#ifdef PPS
	if(ioctl(fdpps,request,(caddr_t) &ppsev) >=0) {
		ppsev.tv.tv_sec += (u_int32) JAN_1970;
		TVTOTS(&ppsev.tv,&up->tstamp);
	}
#endif /* PPS */
	/* record the last ppsclock event time stamp */
	pp->lastrec = up->tstamp;
	if (!refclock_process(pp)) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
        }
	refclock_receive(peer);
}


/*
 * trak_poll - called by the transmit procedure
 */
static void
trak_poll(
	int unit,
	struct peer *peer
	)
{
	register struct trakunit *up;
	struct refclockproc *pp;

	/*
	 * We don't really do anything here, except arm the receiving
	 * side to capture a sample and check for timeouts.
	 */
	pp = peer->procptr;
	up = (struct trakunit *)pp->unitptr;
	if (up->polled)
	    refclock_report(peer, CEVNT_TIMEOUT);
	pp->polls++;
	up->polled = 1;
}

#else
int refclock_trak_bs;
#endif /* REFCLOCK */
