/*
 * refclock_wwvb - clock driver for Spectracom WWVB receivers
 */
#if defined(REFCLOCK) && defined(WWVB)

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

/*
 * This driver supports the Spectracom Model 8170 and Netclock/2 WWVB
 * Synchronized Clock. This clock has proven a reliable source of time,
 * except in some cases of high ambient conductive RF interference. The
 * claimed accuracy of the clock is 100 usec relative to the broadcast
 * signal; however, in most cases the actual accuracy is limited by the
 * precision of the timecode and the latencies of the serial interface
 * and operating system.
 *
 * The DIPswitches on this clock should be set to 24-hour display, AUTO
 * DST off, time zone 0 (UTC), data format 0 or 2 (see below) and baud
 * rate 9600. If this clock is to used as the source for the IRIG Audio
 * Decoder (refclock_irig.c in this distribution), set the DIPswitches
 * for AM IRIG output and IRIG format 1 (IRIG B with signature control).
 *
 * There are two timecode formats used by these clocks. Format 0, which
 * is available with both the Netclock/2 and 8170, and format 2, which
 * is available only with the Netclock/2 and specially modified 8170.
 *
 * Format 0 (22 ASCII printing characters):
 *
 * <cr><lf>i  ddd hh:mm:ss  TZ=zz<cr><lf>
 *
 *	on-time = first <cr> *	hh:mm:ss = hours, minutes, seconds
 *	i = synchronization flag (' ' = in synch, '?' = out of synch)
 *
 * The alarm condition is indicated by other than ' ' at A, which occurs
 * during initial synchronization and when received signal is lost for
 * about ten hours.
 *
 * Format 2 (24 ASCII printing characters):
 *
 * <cr><lf>iqyy ddd hh:mm:ss.fff ld
 *
 *	on-time = <cr>
 *	i = synchronization flag (' ' = in synch, '?' = out of synch)
 *	q = quality indicator (' ' = locked, 'A'...'D' = unlocked)
 *	yy = year (as broadcast)
 *	ddd = day of year
 *	hh:mm:ss.fff = hours, minutes, seconds, milliseconds
 *
 * The alarm condition is indicated by other than ' ' at A, which occurs
 * during initial synchronization and when received signal is lost for
 * about ten hours. The unlock condition is indicated by other than ' '
 * at Q.
 *
 * The Q is normally ' ' when the time error is less than 1 ms and a
 * character in the set 'A'...'D' when the time error is less than 10,
 * 100, 500 and greater than 500 ms respectively. The L is normally ' ',
 * but is set to 'L' early in the month of an upcoming UTC leap second
 * and reset to ' ' on the first day of the following month. The D is
 * set to 'S' for standard time 'I' on the day preceding a switch to
 * daylight time, 'D' for daylight time and 'O' on the day preceding a
 * switch to standard time. The start bit of the first <cr> is
 * synchronized to the indicated time as returned.
 *
 * This driver does not need to be told which format is in use - it
 * figures out which one from the length of the message. A three-stage
 * median filter is used to reduce jitter and provide a dispersion
 * measure. The driver makes no attempt to correct for the intrinsic
 * jitter of the radio itself, which is a known problem with the older
 * radios.
 *
 * Fudge Factors
 *
 * This driver can retrieve a table of quality data maintained
 * internally by the Netclock/2 receiver. If flag4 of the fudge
 * configuration command is set to 1, the driver will retrieve this
 * table and write it to the clockstats file on when the first timecode
 * message of a new day is received.
 */

/*
 * Interface definitions
 */
#define	DEVICE		"/dev/wwvb%d" /* device name and unit */
#define	SPEED232	B9600	/* uart speed (9600 baud) */
#define	PRECISION	(-10)	/* precision assumed (about 1 ms) */
#define	REFID		"WWVB"	/* reference ID */
#define	DESCRIPTION	"Spectracom WWVB Receiver" /* WRU */

#define	NSAMPLES	3	/* stages of median filter */
#define	LENWWVB0	22	/* format 0 timecode length */
#define	LENWWVB2	24	/* format 2 timecode length */
#define MONLIN		15	/* number of monitoring lines */

/*
 * Imported from ntp_timer module
 */
extern	u_long	current_time;	/* current time (s) */

/*
 * Imported from ntpd module
 */
extern	int	debug;		/* global debug flag */

/*
 * WWVB unit control structure
 */
struct wwvbunit {
	int	pollcnt;	/* poll message counter */

	u_char	tcswitch;	/* timecode switch */
	l_fp	laststamp;	/* last receive timestamp */
	u_char	lasthour;	/* last hour (for monitor) */
	u_char	linect;		/* count ignored lines (for monitor */
};

/*
 * Function prototypes
 */
static	int	wwvb_start	P((int, struct peer *));
static	void	wwvb_shutdown	P((int, struct peer *));
static	void	wwvb_receive	P((struct recvbuf *));
static	void	wwvb_poll	P((int, struct peer *));

/*
 * Transfer vector
 */
struct	refclock refclock_wwvb = {
	wwvb_start,		/* start up driver */
	wwvb_shutdown,		/* shut down driver */
	wwvb_poll,		/* transmit poll message */
	noentry,		/* not used (old wwvb_control) */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used (old wwvb_buginfo) */
	NOFLAGS			/* not used */
};


/*
 * wwvb_start - open the devices and initialize data for processing
 */
static int
wwvb_start(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct wwvbunit *up;
	struct refclockproc *pp;
	int fd;
	char device[20];

	/*
	 * Open serial port. Use CLK line discipline, if available.
	 */
	(void)sprintf(device, DEVICE, unit);
	if (!(fd = refclock_open(device, SPEED232, LDISC_CLK)))
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
	pp->io.clock_recv = wwvb_receive;
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
	return (1);
}


/*
 * wwvb_shutdown - shut down the clock
 */
static void
wwvb_shutdown(unit, peer)
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
 * wwvb_receive - receive data from the serial interface
 */
static void
wwvb_receive(rbufp)
	struct recvbuf *rbufp;
{
	register struct wwvbunit *up;
	struct refclockproc *pp;
	struct peer *peer;
	l_fp trtmp;
	u_long ltemp;
	int temp;
	char	syncchar;	/* synchronization indicator */
	char	qualchar;	/* quality indicator */
	char	leapchar;	/* leap indicator */

	/*
	 * Initialize pointers and read the timecode and timestamp
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct wwvbunit *)pp->unitptr;
	temp = refclock_gtlin(rbufp, pp->lastcode, BMAX, &trtmp);

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
	up->pollcnt = 2;
	record_clock_stats(&peer->srcadr, pp->lastcode);
#ifdef DEBUG
	if (debug)
        	printf("wwvb: timecode %d %s\n", pp->lencode,
		    pp->lastcode);
#endif

	/*
	 * We get down to business, check the timecode format and decode
	 * its contents. This code uses the timecode length to determine
	 * whether format 0 or format 2. If the timecode has invalid
	 * length or is not in proper format, we declare bad format and
	 * exit.
	 */
	switch (pp->lencode) {

		case LENWWVB0:

		/*
	 	 * Timecode format 0: "I  ddd hh:mm:ss  TZ=nn"
	 	 */
		qualchar = leapchar = ' ';
		if (sscanf(pp->lastcode, "%c %3d %2d:%2d:%2d",
		    &syncchar, &pp->day, &pp->hour, &pp->minute,
		    &pp->second) == 5)
			break;

		case LENWWVB2:

		/*
	 	 * Timecode format 2: "IQyy ddd hh:mm:ss.mmm LD"
	 	 */
		if (sscanf(pp->lastcode, "%c%c %2d %3d %2d:%2d:%2d.%3d %c",
		    &syncchar, &qualchar, &pp->year, &pp->day,
		    &pp->hour, &pp->minute, &pp->second, &pp->msec,
		    &leapchar) == 9)
			break;

		default:

		if (up->linect > 0)
			up->linect--;
		else
			refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

	/*
	 * Decode synchronization, quality and leap characters. If
	 * unsynchronized, set the leap bits accordingly and exit.
	 * Otherwise, set the leap bits according to the leap character.
	 * Once synchronized, the dispersion depends only on when the
	 * clock was last heard. The first time the clock is heard, the
	 * time last heard is faked based on the quality indicator. The
	 * magic numbers (in seconds) are from the clock specifications.
	 */
	switch (qualchar) {

		case ' ':
		ltemp = 0;
		break;

		case 'A':
		ltemp = 800;
		break;

		case 'B':
		ltemp = 5300;
		break;

		case 'C':
		ltemp = 25300;
		break;

		case 'D':
		ltemp = NTP_MAXAGE;
		break;

		default:
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}
	if (syncchar != ' ')
		pp->leap = LEAP_NOTINSYNC;
	else {
		if (leapchar == 'L')
			pp->leap = LEAP_ADDSECOND;
		else
			pp->leap = 0;
		pp->lasttime = current_time - ltemp;
	}

	/*
	 * If the monitor flag is set (flag4), we dump the internal
	 * quality table at the first timecode beginning the day.
	 */
        if (pp->sloppyclockflag & CLK_FLAG4 && pp->hour <
	    up->lasthour)
		up->linect = MONLIN;
	up->lasthour = pp->hour;

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
	trtmp = pp->lastrec;
	trtmp.l_ui -= ltemp;
	refclock_receive(peer, &pp->offset, 0, pp->dispersion,
	    &trtmp, &pp->lastrec, pp->leap);
}


/*
 * wwvb_poll - called by the transmit procedure
 */
static void
wwvb_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct wwvbunit *up;
	struct refclockproc *pp;
	char poll;

	/*
	 * Time to poll the clock. The Spectracom clock responds to a
	 * 'T' by returning a timecode in the format(s) specified above.
	 * Note there is no checking on state, since this may not be the
	 * only customer reading the clock. Only one customer need poll
	 * the clock; all others just listen in. If nothing is heard
	 * from the clock for two polls, declare a timeout and keep
	 * going.
	 */
	pp = peer->procptr;
	up = (struct wwvbunit *)pp->unitptr;
	if (up->pollcnt == 0)
		refclock_report(peer, CEVNT_TIMEOUT);
	else
		up->pollcnt--;
	if (up->linect > 0)
		poll = 'R';
	else
		poll = 'T';
	if (write(pp->io.fd, &poll, 1) != 1) {
		refclock_report(peer, CEVNT_FAULT);
	} else
		pp->polls++;
}

#endif
