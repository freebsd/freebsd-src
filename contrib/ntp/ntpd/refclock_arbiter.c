/*
 * refclock_arbiter - clock driver for Arbiter 1088A/B Satellite
 *	Controlled Clock
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_ARBITER)

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

#include <stdio.h>
#include <ctype.h>

/*
 * This driver supports the Arbiter 1088A/B Satellite Controlled Clock.
 * The claimed accuracy of this clock is 100 ns relative to the PPS
 * output when receiving four or more satellites.
 *
 * The receiver should be configured before starting the NTP daemon, in
 * order to establish reliable position and operating conditions. It
 * does not initiate surveying or hold mode. For use with NTP, the
 * daylight savings time feature should be disables (D0 command) and the
 * broadcast mode set to operate in UTC (BU command).
 *
 * The timecode format supported by this driver is selected by the poll
 * sequence "B5", which initiates a line in the following format to be
 * repeated once per second until turned off by the "B0" poll sequence.
 *
 * Format B5 (24 ASCII printing characters):
 *
 * <cr><lf>i yy ddd hh:mm:ss.000bbb  
 *
 *	on-time = <cr>
 *	i = synchronization flag (' ' = locked, '?' = unlocked)
 *	yy = year of century
 *	ddd = day of year
 *	hh:mm:ss = hours, minutes, seconds
 *	.000 = fraction of second (not used)
 *	bbb = tailing spaces for fill
 *
 * The alarm condition is indicated by a '?' at i, which indicates the
 * receiver is not synchronized. In normal operation, a line consisting
 * of the timecode followed by the time quality character (TQ) followed
 * by the receiver status string (SR) is written to the clockstats file.
 * The time quality character is encoded in IEEE P1344 standard:
 *
 * Format TQ (IEEE P1344 estimated worst-case time quality)
 *
 *	0	clock locked, maximum accuracy
 *	F	clock failure, time not reliable
 *	4	clock unlocked, accuracy < 1 us
 *	5	clock unlocked, accuracy < 10 us
 *	6	clock unlocked, accuracy < 100 us
 *	7	clock unlocked, accuracy < 1 ms
 *	8	clock unlocked, accuracy < 10 ms
 *	9	clock unlocked, accuracy < 100 ms
 *	A	clock unlocked, accuracy < 1 s
 *	B	clock unlocked, accuracy < 10 s
 *
 * The status string is encoded as follows:
 *
 * Format SR (25 ASCII printing characters)
 *
 *	V=vv S=ss T=t P=pdop E=ee
 *
 *	vv = satellites visible
 *	ss = relative signal strength
 *	t = satellites tracked
 *	pdop = position dilution of precision (meters)
 *	ee = hardware errors
 *
 * If flag4 is set, an additional line consisting of the receiver
 * latitude (LA), longitude (LO) and elevation (LH) (meters) is written
 * to this file. If channel B is enabled for deviation mode and connected
 * to a 1-PPS signal, the last two numbers on the line are the deviation
 * and standard deviation averaged over the last 15 seconds.
 */

/*
 * Interface definitions
 */
#define	DEVICE		"/dev/gps%d" /* device name and unit */
#define	SPEED232	B9600	/* uart speed (9600 baud) */
#define	PRECISION	(-20)	/* precision assumed (about 1 us) */
#define	REFID		"GPS " /* reference ID */
#define	DESCRIPTION	"Arbiter 1088A/B GPS Receiver" /* WRU */

#define	LENARB		24	/* format B5 timecode length */
#define MAXSTA		30	/* max length of status string */
#define MAXPOS		70	/* max length of position string */

/*
 * ARB unit control structure
 */
struct arbunit {
	l_fp	laststamp;	/* last receive timestamp */
	int	tcswitch;	/* timecode switch/counter */
	char	qualchar;	/* IEEE P1344 quality (TQ command) */
	char	status[MAXSTA];	/* receiver status (SR command) */
	char	latlon[MAXPOS];	/* receiver position (lat/lon/alt) */
};

/*
 * Function prototypes
 */
static	int	arb_start	P((int, struct peer *));
static	void	arb_shutdown	P((int, struct peer *));
static	void	arb_receive	P((struct recvbuf *));
static	void	arb_poll	P((int, struct peer *));

/*
 * Transfer vector
 */
struct	refclock refclock_arbiter = {
	arb_start,		/* start up driver */
	arb_shutdown,		/* shut down driver */
	arb_poll,		/* transmit poll message */
	noentry,		/* not used (old arb_control) */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used (old arb_buginfo) */
	NOFLAGS			/* not used */
};


/*
 * arb_start - open the devices and initialize data for processing
 */
static int
arb_start(
	int unit,
	struct peer *peer
	)
{
	register struct arbunit *up;
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
	if (!(up = (struct arbunit *)emalloc(sizeof(struct arbunit)))) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct arbunit));
	pp = peer->procptr;
	pp->io.clock_recv = arb_receive;
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
	write(pp->io.fd, "B0", 2);
	return (1);
}


/*
 * arb_shutdown - shut down the clock
 */
static void
arb_shutdown(
	int unit,
	struct peer *peer
	)
{
	register struct arbunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct arbunit *)pp->unitptr;
	io_closeclock(&pp->io);
	free(up);
}


/*
 * arb_receive - receive data from the serial interface
 */
static void
arb_receive(
	struct recvbuf *rbufp
	)
{
	register struct arbunit *up;
	struct refclockproc *pp;
	struct peer *peer;
	l_fp trtmp;
	int temp;
	u_char	syncchar;	/* synchronization indicator */

	/*
	 * Initialize pointers and read the timecode and timestamp
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct arbunit *)pp->unitptr;
	temp = refclock_gtlin(rbufp, pp->a_lastcode, BMAX, &trtmp);

	/*
	 * Note we get a buffer and timestamp for both a <cr> and <lf>,
	 * but only the <cr> timestamp is retained. The program first
	 * sends a TQ and expects the echo followed by the time quality
	 * character. It then sends a B5 starting the timecode broadcast
	 * and expects the echo followed some time later by the on-time
	 * character <cr> and then the <lf> beginning the timecode
	 * itself. Finally, at the <cr> beginning the next timecode at
	 * the next second, the program sends a B0 shutting down the
	 * timecode broadcast.
	 *
	 * If flag4 is set, the program snatches the latitude, longitude
	 * and elevation and writes it to the clockstats file.
	 */
	if (temp == 0)
		return;
	pp->lastrec = up->laststamp;
	up->laststamp = trtmp;
	if (temp < 3)
		return;
	if (up->tcswitch == 0) {

		/*
		 * Collect statistics. If nothing is recogized, just
		 * ignore; sometimes the clock doesn't stop spewing
		 * timecodes for awhile after the B0 commant.
		 */
		if (!strncmp(pp->a_lastcode, "TQ", 2)) {
			up->qualchar = pp->a_lastcode[2];
			write(pp->io.fd, "SR", 2);
		} else if (!strncmp(pp->a_lastcode, "SR", 2)) {
			strcpy(up->status, pp->a_lastcode + 2);
			if (pp->sloppyclockflag & CLK_FLAG4)
				write(pp->io.fd, "LA", 2);
			else {
				write(pp->io.fd, "B5", 2);
				up->tcswitch++;
			}
		} else if (!strncmp(pp->a_lastcode, "LA", 2)) {
			strcpy(up->latlon, pp->a_lastcode + 2);
			write(pp->io.fd, "LO", 2);
		} else if (!strncmp(pp->a_lastcode, "LO", 2)) {
			strcat(up->latlon, " ");
			strcat(up->latlon, pp->a_lastcode + 2);
			write(pp->io.fd, "LH", 2);
		} else if (!strncmp(pp->a_lastcode, "LH", 2)) {
			strcat(up->latlon, " ");
			strcat(up->latlon, pp->a_lastcode + 2);
			write(pp->io.fd, "DB", 2);
		} else if (!strncmp(pp->a_lastcode, "DB", 2)) {
			strcat(up->latlon, " ");
			strcat(up->latlon, pp->a_lastcode + 2);
			record_clock_stats(&peer->srcadr, up->latlon);
			write(pp->io.fd, "B5", 2);
			up->tcswitch++;
		}
		return;
	}
	pp->lencode = temp;

	/*
	 * We get down to business, check the timecode format and decode
	 * its contents. If the timecode has valid length, but not in
	 * proper format, we declare bad format and exit. If the
	 * timecode has invalid length, which sometimes occurs when the
	 * B0 amputates the broadcast, we just quietly steal away. Note
	 * that the time quality character and receiver status string is
	 * tacked on the end for clockstats display. 
	 */
	if (pp->lencode == LENARB) {
		/*
		 * Timecode format B5: "i yy ddd hh:mm:ss.000   "
		 */
		pp->a_lastcode[LENARB - 2] = up->qualchar;
		strcat(pp->a_lastcode, up->status);
		syncchar = ' ';
		if (sscanf(pp->a_lastcode, "%c%2d %3d %2d:%2d:%2d",
		    &syncchar, &pp->year, &pp->day, &pp->hour,
		    &pp->minute, &pp->second) != 6) {
			refclock_report(peer, CEVNT_BADREPLY);
			write(pp->io.fd, "B0", 2);
			return;
		}
	} else  {
		write(pp->io.fd, "B0", 2);
		return;
	}
	up->tcswitch++;

	/*
	 * We decode the clock dispersion from the time quality
	 * character.
	 */
	switch (up->qualchar) {

	    case '0':		/* locked, max accuracy */
		pp->disp = 1e-7;
		break;

	    case '4':		/* unlock accuracy < 1 us */
		pp->disp = 1e-6;
		break;

	    case '5':		/* unlock accuracy < 10 us */
		pp->disp = 1e-5;
		break;

	    case '6':		/* unlock accuracy < 100 us */
		pp->disp = 1e-4;
		break;

	    case '7':		/* unlock accuracy < 1 ms */
		pp->disp = .001;
		break;

	    case '8':		/* unlock accuracy < 10 ms */
		pp->disp = .01;
		break;

	    case '9':		/* unlock accuracy < 100 ms */
		pp->disp = .1;
		break;

	    case 'A':		/* unlock accuracy < 1 s */
		pp->disp = 1;
		break;

	    case 'B':		/* unlock accuracy < 10 s */
		pp->disp = 10;
		break;

	    case 'F':		/* clock failure */
		pp->disp = MAXDISPERSE;
		refclock_report(peer, CEVNT_FAULT);
		write(pp->io.fd, "B0", 2);
		return;

	    default:
		pp->disp = MAXDISPERSE;
		refclock_report(peer, CEVNT_BADREPLY);
		write(pp->io.fd, "B0", 2);
		return;
	}
	if (syncchar != ' ')
		pp->leap = LEAP_NOTINSYNC;
	else
		pp->leap = LEAP_NOWARNING;
#ifdef DEBUG
	if (debug)
		printf("arbiter: timecode %d %s\n", pp->lencode,
		    pp->a_lastcode);
#endif
	if (up->tcswitch >= NSTAGE)
		write(pp->io.fd, "B0", 2);

	/*
	 * Process the new sample in the median filter and determine the
	 * timecode timestamp.
	 */
	if (!refclock_process(pp))
		refclock_report(peer, CEVNT_BADTIME);
}


/*
 * arb_poll - called by the transmit procedure
 */
static void
arb_poll(
	int unit,
	struct peer *peer
	)
{
	register struct arbunit *up;
	struct refclockproc *pp;

	/*
	 * Time to poll the clock. The Arbiter clock responds to a "B5"
	 * by returning a timecode in the format specified above.
	 * Transmission occurs once per second, unless turned off by a
	 * "B0". Note there is no checking on state, since this may not
	 * be the only customer reading the clock. Only one customer
	 * need poll the clock; all others just listen in. If nothing is
	 * heard from the clock for two polls, declare a timeout and
	 * keep going.
	 */
	pp = peer->procptr;
	up = (struct arbunit *)pp->unitptr;
	up->tcswitch = 0;
	if (write(pp->io.fd, "TQ", 2) != 2) {
		refclock_report(peer, CEVNT_FAULT);
	} else
		pp->polls++;
	if (pp->coderecv == pp->codeproc) {
		refclock_report(peer, CEVNT_TIMEOUT);
		return;
	}
	pp->lastref = pp->lastrec;
	refclock_receive(peer);
	record_clock_stats(&peer->srcadr, pp->a_lastcode);
}

#else
int refclock_arbiter_bs;
#endif /* REFCLOCK */
