/*
 * refclock_wwvb - clock driver for Spectracom WWVB receivers
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_SPECTRACOM)

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_calendar.h"
#include "ntp_stdlib.h"

#include <stdio.h>
#include <ctype.h>

/*
 * This driver supports the Spectracom Model 8170 and Netclock/2 WWVB
 * Synchronized Clocks and the Netclock/GPS Master Clock. Both the WWVB
 * and GPS clocks have proven reliable sources of time; however, the
 * WWVB clocks have proven vulnerable to high ambient conductive RF
 * interference. The claimed accuracy of the WWVB clocks is 100 us
 * relative to the broadcast signal, while the claimed accuracy of the
 * GPS clock is 50 ns; however, in most cases the actual accuracy is
 * limited by the resolution of the timecode and the latencies of the
 * serial interface and operating system.
 *
 * The WWVB and GPS clocks should be configured for 24-hour display,
 * AUTO DST off, time zone 0 (UTC), data format 0 or 2 (see below) and
 * baud rate 9600. If the clock is to used as the source for the IRIG
 * Audio Decoder (refclock_irig.c in this distribution), it should be
 * configured for AM IRIG output and IRIG format 1 (IRIG B with
 * signature control). The GPS clock can be configured either to respond
 * to a 'T' poll character or left running continuously. 
 *
 * There are two timecode formats used by these clocks. Format 0, which
 * is available with both the Netclock/2 and 8170, and format 2, which
 * is available only with the Netclock/2, specially modified 8170 and
 * GPS.
 *
 * Format 0 (22 ASCII printing characters):
 *
 * <cr><lf>i  ddd hh:mm:ss  TZ=zz<cr><lf>
 *
 *	on-time = first <cr>
 *	hh:mm:ss = hours, minutes, seconds
 *	i = synchronization flag (' ' = in synch, '?' = out of synch)
 *
 * The alarm condition is indicated by other than ' ' at a, which occurs
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
 * The alarm condition is indicated by other than ' ' at a, which occurs
 * during initial synchronization and when received signal is lost for
 * about ten hours. The unlock condition is indicated by other than ' '
 * at q.
 *
 * The q is normally ' ' when the time error is less than 1 ms and a
 * character in the set 'A'...'D' when the time error is less than 10,
 * 100, 500 and greater than 500 ms respectively. The l is normally ' ',
 * but is set to 'L' early in the month of an upcoming UTC leap second
 * and reset to ' ' on the first day of the following month. The d is
 * set to 'S' for standard time 'I' on the day preceding a switch to
 * daylight time, 'D' for daylight time and 'O' on the day preceding a
 * switch to standard time. The start bit of the first <cr> is
 * synchronized to the indicated time as returned.
 *
 * This driver does not need to be told which format is in use - it
 * figures out which one from the length of the message.The driver makes
 * no attempt to correct for the intrinsic jitter of the radio itself,
 * which is a known problem with the older radios.
 *
 * Fudge Factors
 *
 * This driver can retrieve a table of quality data maintained
 * internally by the Netclock/2 clock. If flag4 of the fudge
 * configuration command is set to 1, the driver will retrieve this
 * table and write it to the clockstats file on when the first timecode
 * message of a new day is received.
 */

/*
 * Interface definitions
 */
#define	DEVICE		"/dev/wwvb%d" /* device name and unit */
#define	SPEED232	B9600	/* uart speed (9600 baud) */
#define	PRECISION	(-13)	/* precision assumed (about 100 us) */
#define	REFID		"WWVB"	/* reference ID */
#define	DESCRIPTION	"Spectracom WWVB/GPS Receivers" /* WRU */

#define	LENWWVB0	22	/* format 0 timecode length */
#define	LENWWVB2	24	/* format 2 timecode length */
#define LENWWVB3        29      /* format 3 timecode length */
#define MONLIN		15	/* number of monitoring lines */

/*
 * WWVB unit control structure
 */
struct wwvbunit {
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
wwvb_start(
	int unit,
	struct peer *peer
	)
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
	pp->unitptr = (caddr_t)up;
	pp->io.clock_recv = wwvb_receive;
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
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);
	peer->burst = NSTAGE;
	return (1);
}


/*
 * wwvb_shutdown - shut down the clock
 */
static void
wwvb_shutdown(
	int unit,
	struct peer *peer
	)
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
wwvb_receive(
	struct recvbuf *rbufp
	)
{
	struct wwvbunit *up;
	struct refclockproc *pp;
	struct peer *peer;

	l_fp	trtmp;		/* arrival timestamp */
	int	tz;		/* time zone */
	int	day, month;	/* ddd conversion */
	int	temp;		/* int temp */
	char	syncchar;	/* synchronization indicator */
	char	qualchar;	/* quality indicator */
	char	leapchar;	/* leap indicator */
	char	dstchar;	/* daylight/standard indicator */
	char	tmpchar;	/* trashbin */

	/*
	 * Initialize pointers and read the timecode and timestamp
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct wwvbunit *)pp->unitptr;
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
		printf("wwvb: timecode %d %s\n", pp->lencode,
		    pp->a_lastcode);
#endif

	/*
	 * We get down to business, check the timecode format and decode
	 * its contents. This code uses the timecode length to determine
	 * format 0, 2 or 3. If the timecode has invalid length or is
	 * not in proper format, we declare bad format and exit.
	 */
	syncchar = qualchar = leapchar = dstchar = ' ';
	tz = 0;
	pp->msec = 0;
	switch (pp->lencode) {

		case LENWWVB0:

		/*
		 * Timecode format 0: "I  ddd hh:mm:ss DTZ=nn"
		 */
		if (sscanf(pp->a_lastcode,
		    "%c %3d %2d:%2d:%2d%c%cTZ=%2d",
		    &syncchar, &pp->day, &pp->hour, &pp->minute,
		    &pp->second, &tmpchar, &dstchar, &tz) == 8)
			break;

		case LENWWVB2:

		/*
		 * Timecode format 2: "IQyy ddd hh:mm:ss.mmm LD" */
		if (sscanf(pp->a_lastcode,
		    "%c%c %2d %3d %2d:%2d:%2d.%3d %c",
		    &syncchar, &qualchar, &pp->year, &pp->day,
		    &pp->hour, &pp->minute, &pp->second, &pp->msec,
		    &leapchar) == 9)
			break;

		case LENWWVB3:

	   	/*
		 * Timecode format 3: "0003I yyyymmdd hhmmss+0000SL#"
		 */
		if (sscanf(pp->a_lastcode,
		    "0003%c %4d%2d%2d %2d%2d%2d+0000%c%c",
		    &syncchar, &pp->year, &month, &day, &pp->hour,
		    &pp->minute, &pp->second, &dstchar, &leapchar) == 8)
		    {
			pp->day = ymd2yd(pp->year, month, day);
			break;
		}

		default:

		/*
		 * Unknown format: If dumping internal table, record
		 * stats; otherwise, declare bad format.
		 */
		if (up->linect > 0) {
			up->linect--;
			record_clock_stats(&peer->srcadr,
			    pp->a_lastcode);
		} else {
			refclock_report(peer, CEVNT_BADREPLY);
		}
		return;
	}

	/*
	 * Decode synchronization, quality and leap characters. If
	 * unsynchronized, set the leap bits accordingly and exit.
	 * Otherwise, set the leap bits according to the leap character.
	 * Once synchronized, the dispersion depends only on the
	 * quality character.
	 */
	switch (qualchar) {

	    case ' ':
		pp->disp = .001;
		break;

	    case 'A':
		pp->disp = .01;
		break;

	    case 'B':
		pp->disp = .1;
		break;

	    case 'C':
		pp->disp = .5;
		break;

	    case 'D':
		pp->disp = MAXDISPERSE;
		break;

	    default:
		pp->disp = MAXDISPERSE;
		refclock_report(peer, CEVNT_BADREPLY);
		break;
	}
	if (syncchar != ' ')
		pp->leap = LEAP_NOTINSYNC;
	else if (leapchar == 'L')
		pp->leap = LEAP_ADDSECOND;
	else
		pp->leap = LEAP_NOWARNING;

	/*
	 * Process the new sample in the median filter and determine the
	 * timecode timestamp.
	 */
	if (!refclock_process(pp))
		refclock_report(peer, CEVNT_BADTIME);
}


/*
 * wwvb_poll - called by the transmit procedure
 */
static void
wwvb_poll(
	int unit,
	struct peer *peer
	)
{
	register struct wwvbunit *up;
	struct refclockproc *pp;
	char	pollchar;	/* character sent to clock */

	/*
	 * Time to poll the clock. The Spectracom clock responds to a
	 * 'T' by returning a timecode in the format(s) specified above.
	 * Note there is no checking on state, since this may not be the
	 * only customer reading the clock. Only one customer need poll
	 * the clock; all others just listen in. If the clock becomes
	 * unreachable, declare a timeout and keep going.
	 */
	pp = peer->procptr;
	up = (struct wwvbunit *)pp->unitptr;
	if (up->linect > 0)
		pollchar = 'R';
	else
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
int refclock_wwvb_bs;
#endif /* REFCLOCK */
