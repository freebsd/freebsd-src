/*
 * refclock_ulink - clock driver for Ultralink Model 320 WWVB receivers
 * By Dave Strout <dstrout@linuxfoundary.com>
 *
 * Latest version is always on www.linuxfoundary.com
 *
 * Based on the Spectracom driver
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_ULINK)

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>
#include <time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"

/*
 * This driver supports the Ultralink Model 320 WWVB receiver.  The Model 320 is
 * an RS-232 powered unit which consists of two parts:  a DB-25 shell that contains
 * a microprocessor, and an approx 2"x4" plastic box that contains the antenna.
 * The two are connected by a 6-wire RJ-25 cable of length up to 1000'.  The 
 * microprocessor steals power from the RS-232 port, which means that the port must 
 * be kept open all of the time.  The unit also has an internal clock for loss of signal
 * periods.  Claimed accuracy is 0.1 sec.
 * 
 * The timecode format is:
 *
 *  <cr><lf>SQRYYYYDDD+HH:MM:SS.mmLT<cr>
 *
 * where:
 *
 * S = 'S' -- sync'd in last hour, '0'-'9' - hours x 10 since last update, else '?'
 * Q = Number of correlating time-frames, from 0 to 5
 * R = 'R' -- reception in progress, 'N' -- Noisy reception, ' ' -- standby mode
 * YYYY = year from 1990 to 2089
 * DDD = current day from 1 to 366
 * + = '+' if current year is a leap year, else ' '
 * HH = UTC hour 0 to 23
 * MM = Minutes of current hour from 0 to 59
 * SS = Seconds of current minute from 0 to 59
 * mm = 10's milliseconds of the current second from 00 to 99
 * L  = Leap second pending at end of month -- 'I' = inset, 'D'=delete
 * T  = DST <-> STD transition indicators
 *
 * Note that this driver does not do anything with the L or T flags.
 *
 * The M320 also has a 'U' command which returns UT1 correction information.  It
 * is not used in this driver.
 *
 */

/*
 * Interface definitions
 */
#define	DEVICE		"/dev/ulink%d" /* device name and unit */
#define	SPEED232	B9600	/* uart speed (9600 baud) */
#define	PRECISION	(-13)	/* precision assumed (about 100 us) */
#define	REFID		"M320"	/* reference ID */
#define	DESCRIPTION	"Ultralink WWVB Receiver" /* WRU */

#define	LENWWVB0	28	/* format 0 timecode length */
#define	LENWWVB2	24	/* format 2 timecode length */
#define LENWWVB3        29      /* format 3 timecode length */

#define MONLIN		15	/* number of monitoring lines */

/*
 * ULINK unit control structure
 */
struct ulinkunit {
	u_char	tcswitch;	/* timecode switch */
	l_fp	laststamp;	/* last receive timestamp */
	u_char	lasthour;	/* last hour (for monitor) */
	u_char	linect;		/* count ignored lines (for monitor */
};

/*
 * Function prototypes
 */
static	int	ulink_start	P((int, struct peer *));
static	void	ulink_shutdown	P((int, struct peer *));
static	void	ulink_receive	P((struct recvbuf *));
static	void	ulink_poll	P((int, struct peer *));
static  int     fd; /* We need to keep the serial port open to power the ULM320 */

/*
 * Transfer vector
 */
struct	refclock refclock_ulink = {
	ulink_start,		/* start up driver */
	ulink_shutdown,		/* shut down driver */
	ulink_poll,		/* transmit poll message */
	noentry,		/* not used (old wwvb_control) */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used (old wwvb_buginfo) */
	NOFLAGS			/* not used */
};


/*
 * ulink_start - open the devices and initialize data for processing
 */
static int
ulink_start(
	int unit,
	struct peer *peer
	)
{
	register struct ulinkunit *up;
	struct refclockproc *pp;
	char device[20];
	fprintf(stderr, "Starting Ulink driver\n");
	/*
	 * Open serial port. Use CLK line discipline, if available.
	 */
	(void)sprintf(device, DEVICE, unit);
	if (!(fd = refclock_open(device, SPEED232, LDISC_CLK)))
		return (0);

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct ulinkunit *)
	      emalloc(sizeof(struct ulinkunit)))) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct ulinkunit));
	pp = peer->procptr;
	pp->unitptr = (caddr_t)up;
	pp->io.clock_recv = ulink_receive;
	pp->io.srcclock = (caddr_t)peer;
	pp->io.datalen = 0;
	pp->io.fd = fd;
	if (!io_addclock(&pp->io)) {
		(void) close(fd);
		free(up);
		return (0);
	}

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = PRECISION;
	peer->flags |= FLAG_BURST;
	peer->burst = NSTAGE;
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);
	return (1);
}


/*
 * ulink_shutdown - shut down the clock
 */
static void
ulink_shutdown(
	int unit,
	struct peer *peer
	)
{
	register struct ulinkunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct ulinkunit *)pp->unitptr;
	io_closeclock(&pp->io);
	free(up);
	close(fd);
}


/*
 * ulink_receive - receive data from the serial interface
 */
static void
ulink_receive(
	struct recvbuf *rbufp
	)
{
	struct ulinkunit *up;
	struct refclockproc *pp;
	struct peer *peer;

	l_fp	trtmp;		/* arrival timestamp */
	char	syncchar;	/* synchronization indicator */
	char	qualchar;	/* quality indicator */
	char    modechar;       /* Modes: 'R'=rx, 'N'=noise, ' '=standby */
	char	leapchar;	/* leap indicator */
	int	temp;		/* int temp */

	/*
	 * Initialize pointers and read the timecode and timestamp
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct ulinkunit *)pp->unitptr;
	temp = refclock_gtlin(rbufp, pp->a_lastcode, BMAX, &trtmp);

	/*
	 * Note we get a buffer and timestamp for both a <cr> and <lf>,
	 * but only the <cr> timestamp is retained. Note: in format 0 on
	 * a Netclock/2 or upgraded 8170 the start bit is delayed 100
	 * +-50 us relative to the pps; however, on an unmodified 8170
	 * the start bit can be delayed up to 10 ms. In format 2 the
	 * reading precision is only to the millisecond. Thus, unless
	 * you have a pps gadget and don't have to have the year, format
	 * 0 provides the lowest jitter.
	 */
	if (temp == 0) {
		if (up->tcswitch == 0) {
			up->tcswitch = 1;
			up->laststamp = trtmp;
		} else
		    up->tcswitch = 0;
		return;
	}
	pp->lencode = temp;
	pp->lastrec = up->laststamp;
	up->laststamp = trtmp;
	up->tcswitch = 1;
#ifdef DEBUG
	if (debug)
		printf("ulink: timecode %d %s\n", pp->lencode,
		    pp->a_lastcode);
#endif

	/*
	 * We get down to business, check the timecode format and decode
	 * its contents. This code uses the timecode length to determine
	 * whether format 0 or format 2. If the timecode has invalid
	 * length or is not in proper format, we declare bad format and
	 * exit.
	 */
	syncchar = qualchar = leapchar = ' ';
	pp->msec = 0;
	
	/*
	 * Timecode format SQRYYYYDDD+HH:MM:SS.mmLT
	 */
	sscanf(pp->a_lastcode, "%c%c%c%4d%3d%c%2d:%2d:%2d.%2d",
	       &syncchar, &qualchar, &modechar, &pp->year, &pp->day,
	       &leapchar,&pp->hour, &pp->minute, &pp->second,&pp->msec);

	pp->msec *= 10; /* M320 returns 10's of msecs */
	qualchar = ' ';

	/*
	 * Decode synchronization, quality and leap characters. If
	 * unsynchronized, set the leap bits accordingly and exit.
	 * Otherwise, set the leap bits according to the leap character.
	 * Once synchronized, the dispersion depends only on the
	 * quality character.
	 */
	pp->disp = .001;
	pp->leap = LEAP_NOWARNING;

	/*
	 * Process the new sample in the median filter and determine the
	 * timecode timestamp.
	 */
	if (!refclock_process(pp))
		refclock_report(peer, CEVNT_BADTIME);
}


/*
 * ulink_poll - called by the transmit procedure
 */
static void
ulink_poll(
	int unit,
	struct peer *peer
	)
{
	register struct ulinkunit *up;
	struct refclockproc *pp;
	char pollchar;

	pp = peer->procptr;
	up = (struct ulinkunit *)pp->unitptr;
	pollchar = 'T';
	if (write(pp->io.fd, &pollchar, 1) != 1)
		refclock_report(peer, CEVNT_FAULT);
	else
		pp->polls++;
	if (peer->burst > 0)
		return;
	if (pp->coderecv == pp->codeproc) {
		refclock_report(peer, CEVNT_TIMEOUT);
		return;
	}
	record_clock_stats(&peer->srcadr, pp->a_lastcode);
	refclock_receive(peer);
	peer->burst = NSTAGE;

	/*
	 * If the monitor flag is set (flag4), we dump the internal
	 * quality table at the first timecode beginning the day.
	 */
	if (pp->sloppyclockflag & CLK_FLAG4 && pp->hour <
	    (int)up->lasthour)
		up->linect = MONLIN;
	up->lasthour = pp->hour;
}

#else
int refclock_ulink_bs;
#endif /* REFCLOCK */
