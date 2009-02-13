/*
 * refclock_acts - clock driver for the NIST/USNO/PTB/NPL Computer Time
 *	Services
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if defined(REFCLOCK) && (defined(CLOCK_ACTS) || defined(CLOCK_PTBACTS))

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"
#include "ntp_control.h"

#include <stdio.h>
#include <ctype.h>
#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif /* HAVE_SYS_IOCTL_H */

/*
 * This driver supports the US (NIST, USNO) and European (PTB, NPL,
 * etc.) modem time services, as well as Spectracom GPS and WWVB
 * receivers connected via a modem. The driver periodically dials a
 * number from a telephone list, receives the timecode data and
 * calculates the local clock correction. It is designed primarily for
 * use as backup when neither a radio clock nor connectivity to Internet
 * time servers is available.
 *
 * This driver requires a modem with a Hayes-compatible command set and
 * control over the modem data terminal ready (DTR) control line. The
 * modem setup string is hard-coded in the driver and may require
 * changes for nonstandard modems or special circumstances. For reasons
 * unrelated to this driver, the data set ready (DSR) control line
 * should not be set when this driver is first started.
 *
 * The calling program is initiated by setting fudge flag1, either
 * manually or automatically. When flag1 is set, the calling program
 * dials the first number in the phone command of the configuration
 * file. If that call fails, the calling program dials the second number
 * and so on. The number is specified by the Hayes ATDT prefix followed
 * by the number itself, including the prefix and long-distance digits
 * and delay code, if necessary. The flag1 is reset and the calling
 * program terminated if (a) a valid clock update has been determined,
 * (b) no more numbers remain in the list, (c) a device fault or timeout
 * occurs or (d) fudge flag1 is reset manually.
 *
 * The driver is transparent to each of the modem time services and
 * Spectracom radios. It selects the parsing algorithm depending on the
 * message length. There is some hazard should the message be corrupted.
 * However, the data format is checked carefully and only if all checks
 * succeed is the message accepted. Corrupted lines are discarded
 * without complaint.
 *
 * Fudge controls
 *
 * flag1	force a call in manual mode
 * flag2	enable port locking (not verified)
 * flag3	no modem; port is directly connected to device
 * flag4	not used
 *
 * time1	offset adjustment (s)
 *
 * Ordinarily, the serial port is connected to a modem; however, it can
 * be connected directly to a device or another computer for testing and
 * calibration. In this case set fudge flag3 and the driver will send a
 * single character 'T' at each poll event. In principle, fudge flag2
 * enables port locking, allowing the modem to be shared when not in use
 * by this driver. At least on Solaris with the current NTP I/O
 * routines, this results only in lots of ugly error messages.
 */
/*
 * National Institute of Science and Technology (NIST)
 *
 * Phone: (303) 494-4774 (Boulder, CO); (808) 335-4721 (Hawaii)
 *
 * Data Format
 *
 * National Institute of Standards and Technology
 * Telephone Time Service, Generator 3B
 * Enter question mark "?" for HELP
 *                         D  L D
 *  MJD  YR MO DA H  M  S  ST S UT1 msADV        <OTM>
 * 47999 90-04-18 21:39:15 50 0 +.1 045.0 UTC(NIST) *<CR><LF>
 * ...
 *
 * MJD, DST, DUT1 and UTC are not used by this driver. The "*" or "#" is
 * the on-time markers echoed by the driver and used by NIST to measure
 * and correct for the propagation delay.
 *
 * US Naval Observatory (USNO)
 *
 * Phone: (202) 762-1594 (Washington, DC); (719) 567-6742 (Boulder, CO)
 *
 * Data Format (two lines, repeating at one-second intervals)
 *
 * jjjjj nnn hhmmss UTC<CR><LF>
 * *<CR><LF>
 *
 * jjjjj	modified Julian day number (not used)
 * nnn		day of year
 * hhmmss	second of day
 * *		on-time marker for previous timecode
 * ...
 *
 * USNO does not correct for the propagation delay. A fudge time1 of
 * about .06 s is advisable.
 *
 * European Services (PTB, NPL, etc.)
 *
 * PTB: +49 531 512038 (Germany)
 * NPL: 0906 851 6333 (UK only)
 *
 * Data format (see the documentation for phone numbers and formats.)
 *
 * 1995-01-23 20:58:51 MEZ  10402303260219950123195849740+40000500<CR><LF>
 *
 * Spectracom GPS and WWVB Receivers
 *
 * If a modem is connected to a Spectracom receiver, this driver will
 * call it up and retrieve the time in one of two formats. As this
 * driver does not send anything, the radio will have to either be
 * configured in continuous mode or be polled by another local driver.
 */
/*
 * Interface definitions
 */
#define	DEVICE		"/dev/acts%d" /* device name and unit */
#define	SPEED232	B9600	/* uart speed (9600 baud) */
#define	PRECISION	(-10)	/* precision assumed (about 1 ms) */
#define LOCKFILE	"/var/spool/locks/LCK..cua%d"
#define DESCRIPTION	"Automated Computer Time Service" /* WRU */
#define REFID		"NONE"	/* default reference ID */
#define MSGCNT		20	/* max message count */
#define SMAX		256	/* max clockstats line length */

/*
 * Calling program modes
 */
#define MODE_AUTO	0	/* automatic mode */
#define MODE_BACKUP	1	/* backup mode */
#define MODE_MANUAL	2	/* manual mode */

/*
 * Service identifiers.
 */
#define REFACTS		"NIST"	/* NIST reference ID */
#define LENACTS		50	/* NIST format */
#define REFUSNO		"USNO"	/* USNO reference ID */
#define LENUSNO		20	/* USNO */
#define REFPTB		"PTB\0"	/* PTB/NPL reference ID */
#define LENPTB		78	/* PTB/NPL format */
#define REFWWVB		"WWVB"	/* WWVB reference ID */
#define	LENWWVB0	22	/* WWVB format 0 */
#define	LENWWVB2	24	/* WWVB format 2 */
#define LF		0x0a	/* ASCII LF */

/*
 * Modem setup strings. These may have to be changed for some modems.
 *
 * AT	command prefix
 * B1	US answer tone
 * &C0	disable carrier detect
 * &D2	hang up and return to command mode on DTR transition
 * E0	modem command echo disabled
 * l1	set modem speaker volume to low level
 * M1	speaker enabled until carrier detect
 * Q0	return result codes
 * V1	return result codes as English words
 */
#define MODEM_SETUP	"ATB1&C0&D2E0L1M1Q0V1\r" /* modem setup */
#define MODEM_HANGUP	"ATH\r"	/* modem disconnect */

/*
 * Timeouts (all in seconds)
 */
#define SETUP		3	/* setup timeout */
#define	DTR		1	/* DTR timeout */
#define ANSWER		60	/* answer timeout */
#define CONNECT		20	/* first valid message timeout */
#define TIMECODE	30	/* all valid messages timeout */

/*
 * State machine codes
 */
#define S_IDLE		0	/* wait for poll */
#define S_OK		1	/* wait for modem setup */
#define S_DTR		2	/* wait for modem DTR */
#define S_CONNECT	3	/* wait for answer*/
#define S_FIRST		4	/* wait for first valid message */
#define S_MSG		5	/* wait for all messages */
#define S_CLOSE		6	/* wait after sending disconnect */

/*
 * Unit control structure
 */
struct actsunit {
	int	unit;		/* unit number */
	int	state;		/* the first one was Delaware */
	int	timer;		/* timeout counter */
	int	retry;		/* retry index */
	int	msgcnt;		/* count of messages received */
	l_fp	tstamp;		/* on-time timestamp */
	char	*bufptr;	/* buffer pointer */
};

/*
 * Function prototypes
 */
static	int	acts_start	P((int, struct peer *));
static	void	acts_shutdown	P((int, struct peer *));
static	void	acts_receive	P((struct recvbuf *));
static	void	acts_message	P((struct peer *));
static	void	acts_timecode	P((struct peer *, char *));
static	void	acts_poll	P((int, struct peer *));
static	void	acts_timeout	P((struct peer *));
static	void	acts_disc	P((struct peer *));
static	void	acts_timer	P((int, struct peer *));

/*
 * Transfer vector (conditional structure name)
 */
struct	refclock refclock_acts = {
	acts_start,		/* start up driver */
	acts_shutdown,		/* shut down driver */
	acts_poll,		/* transmit poll message */
	noentry,		/* not used */
	noentry,		/* not used */
	noentry,		/* not used */
	acts_timer		/* housekeeping timer */
};

struct	refclock refclock_ptb;

/*
 * Initialize data for processing
 */
static int
acts_start (
	int	unit,
	struct peer *peer
	)
{
	struct actsunit *up;
	struct refclockproc *pp;

	/*
	 * Allocate and initialize unit structure
	 */
	up = emalloc(sizeof(struct actsunit));
	if (up == NULL)
		return (0);

	memset(up, 0, sizeof(struct actsunit));
	up->unit = unit;
	pp = peer->procptr;
	pp->unitptr = (caddr_t)up;
	pp->io.clock_recv = acts_receive;
	pp->io.srcclock = (caddr_t)peer;
	pp->io.datalen = 0;

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = PRECISION;
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);
	peer->sstclktype = CTL_SST_TS_TELEPHONE;
	peer->flags &= ~FLAG_FIXPOLL;
	up->bufptr = pp->a_lastcode;
	return (1);
}


/*
 * acts_shutdown - shut down the clock
 */
static void
acts_shutdown (
	int	unit,
	struct peer *peer
	)
{
	struct actsunit *up;
	struct refclockproc *pp;

	/*
	 * Warning: do this only when a call is not in progress.
	 */
	pp = peer->procptr;
	up = (struct actsunit *)pp->unitptr;
	free(up);
}


/*
 * acts_receive - receive data from the serial interface
 */
static void
acts_receive (
	struct recvbuf *rbufp
	)
{
	struct actsunit *up;
	struct refclockproc *pp;
	struct peer *peer;
	char	tbuf[BMAX];
	char	*tptr;

	/*
	 * Initialize pointers and read the timecode and timestamp. Note
	 * we are in raw mode and victim of whatever the terminal
	 * interface kicks up; so, we have to reassemble messages from
	 * arbitrary fragments. Capture the timecode at the beginning of
	 * the message and at the '*' and '#' on-time characters.
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct actsunit *)pp->unitptr;
	pp->lencode = refclock_gtraw(rbufp, tbuf, BMAX - (up->bufptr -
	    pp->a_lastcode), &pp->lastrec);
	for (tptr = tbuf; *tptr != '\0'; tptr++) {
		if (*tptr == LF) {
			if (up->bufptr == pp->a_lastcode) {
				up->tstamp = pp->lastrec;
				continue;

			} else {
				*up->bufptr = '\0';
				acts_message(peer);
				up->bufptr = pp->a_lastcode;
			}
		} else if (!iscntrl(*tptr)) {
			*up->bufptr++ = *tptr;
			if (*tptr == '*' || *tptr == '#') {
				up->tstamp = pp->lastrec;
				write(pp->io.fd, tptr, 1);
			}
		}
	}
}


/*
 * acts_message - process message
 */
void
acts_message(
	struct peer *peer
	)
{
	struct actsunit *up;
	struct refclockproc *pp;
	int	dtr = TIOCM_DTR;
	char	tbuf[SMAX];
#ifdef DEBUG
	u_int	modem;
#endif

	/*
	 * What to do depends on the state and the first token in the
	 * message. A NO token sends the message to the clockstats.
	 */
	pp = peer->procptr;
	up = (struct actsunit *)pp->unitptr;
#ifdef DEBUG
	ioctl(pp->io.fd, TIOCMGET, (char *)&modem);
	sprintf(tbuf, "acts: %04x (%d %d) %lu %s", modem, up->state,
	    up->timer, strlen(pp->a_lastcode), pp->a_lastcode);
	if (debug)
		printf("%s\n", tbuf);
#endif
	strncpy(tbuf, pp->a_lastcode, SMAX);
	strtok(tbuf, " ");
	if (strcmp(tbuf, "NO") == 0)
		record_clock_stats(&peer->srcadr, pp->a_lastcode);
	switch(up->state) {

	/*
	 * We are waiting for the OK response to the modem setup
	 * command. When this happens, raise DTR and dial the number
	 * followed by \r.
	 */
	case S_OK:
		if (strcmp(tbuf, "OK") != 0) {
			msyslog(LOG_ERR, "acts: setup error %s",
			    pp->a_lastcode);
			acts_disc(peer);
			return;
		}
		ioctl(pp->io.fd, TIOCMBIS, (char *)&dtr);
		up->state = S_DTR;
		up->timer = DTR;
		return;

	/*
	 * We are waiting for the call to be answered. All we care about
	 * here is token CONNECT. Send the message to the clockstats.
	 */
	case S_CONNECT:
		record_clock_stats(&peer->srcadr, pp->a_lastcode);
		if (strcmp(tbuf, "CONNECT") != 0) {
			acts_disc(peer);
			return;
		}
		up->state = S_FIRST;
		up->timer = CONNECT;
		return;

	/*
	 * We are waiting for a timecode. Pass it to the parser.
	 */
	case S_FIRST:
	case S_MSG:
		acts_timecode(peer, pp->a_lastcode);
		break;
	}
}

/*
 * acts_timecode - identify the service and parse the timecode message
 */
void
acts_timecode(
	struct peer *peer,	/* peer structure pointer */
	char	*str		/* timecode string */
	)
{
	struct actsunit *up;
	struct refclockproc *pp;
	int	day;		/* day of the month */
	int	month;		/* month of the year */
	u_long	mjd;		/* Modified Julian Day */
	double	dut1;		/* DUT adjustment */

	u_int	dst;		/* ACTS daylight/standard time */
	u_int	leap;		/* ACTS leap indicator */
	double	msADV;		/* ACTS transmit advance (ms) */
	char	utc[10];	/* ACTS timescale */
	char	flag;		/* ACTS on-time character (* or #) */

	char	synchar;	/* WWVB synchronized indicator */
	char	qualchar;	/* WWVB quality indicator */
	char	leapchar;	/* WWVB leap indicator */
	char	dstchar;	/* WWVB daylight/savings indicator */
	int	tz;		/* WWVB timezone */

	u_int	leapmonth;	/* PTB/NPL month of leap */
	char	leapdir;	/* PTB/NPL leap direction */

	/*
	 * The parser selects the modem format based on the message
	 * length. Since the data are checked carefully, occasional
	 * errors due noise are forgivable.
	 */
	pp = peer->procptr;
	up = (struct actsunit *)pp->unitptr;
	pp->nsec = 0;
	switch(strlen(str)) {

	/*
	 * For USNO format on-time character '*', which is on a line by
	 * itself. Be sure a timecode has been received.
	 */
	case 1:
		if (*str == '*' && up->msgcnt > 0) 
			break;

		return;
	
	/*
	 * ACTS format: "jjjjj yy-mm-dd hh:mm:ss ds l uuu aaaaa
	 * UTC(NIST) *"
	 */
	case LENACTS:
		if (sscanf(str,
		    "%5ld %2d-%2d-%2d %2d:%2d:%2d %2d %1d %3lf %5lf %9s %c",
		    &mjd, &pp->year, &month, &day, &pp->hour,
		    &pp->minute, &pp->second, &dst, &leap, &dut1,
		    &msADV, utc, &flag) != 13) {
			refclock_report(peer, CEVNT_BADREPLY);
			return;
		}

		/*
		 * Wait until ACTS has calculated the roundtrip delay.
		 * We don't need to do anything, as ACTS adjusts the
		 * on-time epoch.
		 */
		if (flag != '#')
			return;

		pp->day = ymd2yd(pp->year, month, day);
		pp->leap = LEAP_NOWARNING;
		if (leap == 1)
	    		pp->leap = LEAP_ADDSECOND;
		else if (pp->leap == 2)
	    		pp->leap = LEAP_DELSECOND;
		memcpy(&pp->refid, REFACTS, 4);
		if (up->msgcnt == 0)
			record_clock_stats(&peer->srcadr, str);
		up->msgcnt++;
		break;

	/*
	 * USNO format: "jjjjj nnn hhmmss UTC"
	 */
	case LENUSNO:
		if (sscanf(str, "%5ld %3d %2d%2d%2d %3s",
		    &mjd, &pp->day, &pp->hour, &pp->minute,
		    &pp->second, utc) != 6) {
			refclock_report(peer, CEVNT_BADREPLY);
			return;
		}

		/*
		 * Wait for the on-time character, which follows in a
		 * separate message. There is no provision for leap
		 * warning.
		 */
		pp->leap = LEAP_NOWARNING;
		memcpy(&pp->refid, REFUSNO, 4);
		if (up->msgcnt == 0)
			record_clock_stats(&peer->srcadr, str);
		up->msgcnt++;
		return;

	/*
	 * PTB/NPL format: "yyyy-mm-dd hh:mm:ss MEZ" 
	 */
	case LENPTB:
		if (sscanf(str,
		    "%*4d-%*2d-%*2d %*2d:%*2d:%2d %*5c%*12c%4d%2d%2d%2d%2d%5ld%2lf%c%2d%3lf%*15c%c",
		    &pp->second, &pp->year, &month, &day, &pp->hour,
		    &pp->minute, &mjd, &dut1, &leapdir, &leapmonth,
		    &msADV, &flag) != 12) {
			refclock_report(peer, CEVNT_BADREPLY);
			return;
		}
		pp->leap = LEAP_NOWARNING;
		if (leapmonth == month) {
			if (leapdir == '+')
		    		pp->leap = LEAP_ADDSECOND;
			else if (leapdir == '-')
		    		pp->leap = LEAP_DELSECOND;
		}
		pp->day = ymd2yd(pp->year, month, day);
		memcpy(&pp->refid, REFPTB, 4);
		if (up->msgcnt == 0)
			record_clock_stats(&peer->srcadr, str);
		up->msgcnt++;
		break;


	/*
	 * WWVB format 0: "I  ddd hh:mm:ss DTZ=nn"
	 */
	case LENWWVB0:
		if (sscanf(str, "%c %3d %2d:%2d:%2d %cTZ=%2d",
		    &synchar, &pp->day, &pp->hour, &pp->minute,
		    &pp->second, &dstchar, &tz) != 7) {
			refclock_report(peer, CEVNT_BADREPLY);
			return;
		}
		pp->leap = LEAP_NOWARNING;
		if (synchar != ' ')
			pp->leap = LEAP_NOTINSYNC;
		memcpy(&pp->refid, REFWWVB, 4);
		if (up->msgcnt == 0)
			record_clock_stats(&peer->srcadr, str);
		up->msgcnt++;
		break;

	/*
	 * WWVB format 2: "IQyy ddd hh:mm:ss.mmm LD"
	 */
	case LENWWVB2:
		if (sscanf(str, "%c%c%2d %3d %2d:%2d:%2d.%3ld%c%c%c",
		    &synchar, &qualchar, &pp->year, &pp->day,
		    &pp->hour, &pp->minute, &pp->second, &pp->nsec,
		    &dstchar, &leapchar, &dstchar) != 11) {
			refclock_report(peer, CEVNT_BADREPLY);
			return;
		}
		pp->nsec *= 1000000;
		pp->leap = LEAP_NOWARNING;
		if (synchar != ' ')
			pp->leap = LEAP_NOTINSYNC;
		else if (leapchar == 'L')
			pp->leap = LEAP_ADDSECOND;
		memcpy(&pp->refid, REFWWVB, 4);
		if (up->msgcnt == 0)
			record_clock_stats(&peer->srcadr, str);
		up->msgcnt++;
		break;

	/*
	 * None of the above. Just forget about it and wait for the next
	 * message or timeout.
	 */
	default:
		return;
	}

	/*
	 * We have a valid timecode. The fudge time1 value is added to
	 * each sample by the main line routines. Note that in current
	 * telephone networks the propatation time can be different for
	 * each call and can reach 200 ms for some calls.
	 */
	peer->refid = pp->refid;
	pp->lastrec = up->tstamp;
	if (!refclock_process(pp)) {
		refclock_report(peer, CEVNT_BADTIME);
		return;
			}
	pp->lastref = pp->lastrec;
	if (peer->disp > MAXDISTANCE)
		refclock_receive(peer);
	if (up->state != S_MSG) {
		up->state = S_MSG;
		up->timer = TIMECODE;
	}
}


/*
 * acts_poll - called by the transmit routine
 */
static void
acts_poll (
	int	unit,
	struct peer *peer
	)
{
	struct actsunit *up;
	struct refclockproc *pp;

	/*
	 * This routine is called at every system poll. All it does is
	 * set flag1 under certain conditions. The real work is done by
	 * the timeout routine and state machine.
	 */
	pp = peer->procptr;
	up = (struct actsunit *)pp->unitptr;
	switch (peer->ttl) {

	/*
	 * In manual mode the calling program is activated by the ntpdc
	 * program using the enable flag (fudge flag1), either manually
	 * or by a cron job.
	 */
	case MODE_MANUAL:
		/* fall through */
		break;

	/*
	 * In automatic mode the calling program runs continuously at
	 * intervals determined by the poll event or specified timeout.
	 */
	case MODE_AUTO:
		pp->sloppyclockflag |= CLK_FLAG1;
		break;

	/*
	 * In backup mode the calling program runs continuously as long
	 * as either no peers are available or this peer is selected.
	 */
	case MODE_BACKUP:
		if (sys_peer == NULL || sys_peer == peer)
			pp->sloppyclockflag |= CLK_FLAG1;
		break;
	}
}


/*
 * acts_timer - called at one-second intervals
 */
static void
acts_timer(
	int	unit,
	struct peer *peer
	)
{
	struct actsunit *up;
	struct refclockproc *pp;

	/*
	 * This routine implments a timeout which runs for a programmed
	 * interval. The counter is initialized by the state machine and
	 * counts down to zero. Upon reaching zero, the state machine is
	 * called. If flag1 is set while in S_IDLE state, force a
	 * timeout.
	 */
	pp = peer->procptr;
	up = (struct actsunit *)pp->unitptr;
	if (pp->sloppyclockflag & CLK_FLAG1 && up->state == S_IDLE) {
		acts_timeout(peer);
		return;
	}
	if (up->timer == 0)
		return;

	up->timer--;
	if (up->timer == 0)
		acts_timeout(peer);
}


/*
 * acts_timeout - called on timeout
 */
static void
acts_timeout(
	struct peer *peer
	)
{
	struct actsunit *up;
	struct refclockproc *pp;
	int	fd;
	char	device[20];
	char	lockfile[128], pidbuf[8];
	char	tbuf[BMAX];

	/*
	 * The state machine is driven by messages from the modem, when
	 * first stated and at timeout.
	 */
	pp = peer->procptr;
	up = (struct actsunit *)pp->unitptr;
	pp->sloppyclockflag &= ~CLK_FLAG1;
	if (sys_phone[up->retry] == NULL && !(pp->sloppyclockflag &
	    CLK_FLAG3)) {
		msyslog(LOG_ERR, "acts: no phones");
		return;
	}
	switch(up->state) {

	/*
	 * System poll event. Lock the modem port and open the device.
	 */
	case S_IDLE:

		/*
		 * Lock the modem port. If busy, retry later. Note: if
		 * something fails between here and the close, the lock
		 * file may not be removed.
		 */
		if (pp->sloppyclockflag & CLK_FLAG2) {
			sprintf(lockfile, LOCKFILE, up->unit);
			fd = open(lockfile, O_WRONLY | O_CREAT | O_EXCL,
			    0644);
			if (fd < 0) {
				msyslog(LOG_ERR, "acts: port busy");
				return;
			}
			sprintf(pidbuf, "%d\n", (u_int)getpid());
			write(fd, pidbuf, strlen(pidbuf));
			close(fd);
		}

		/*
		 * Open the device in raw mode and link the I/O.
		 */
		if (!pp->io.fd) {
			sprintf(device, DEVICE, up->unit);
			fd = refclock_open(device, SPEED232,
			    LDISC_ACTS | LDISC_RAW | LDISC_REMOTE);
			if (fd == 0) {
				return;
			}
			pp->io.fd = fd;
			if (!io_addclock(&pp->io)) {
				msyslog(LOG_ERR,
				    "acts: addclock fails");
				close(fd);
				pp->io.fd = 0;
				return;
			}
		}

		/*
		 * If the port is directly connected to the device, skip
		 * the modem business and send 'T' for Spectrabum.
		 */
		if (pp->sloppyclockflag & CLK_FLAG3) {
			if (write(pp->io.fd, "T", 1) < 0) {
				msyslog(LOG_ERR, "acts: write %m");
				return;
			}
			up->state = S_FIRST;
			up->timer = CONNECT;
			return;
		}

		/*
		 * Initialize the modem. This works with Hayes commands.
		 */
#ifdef DEBUG
		if (debug)
			printf("acts: setup %s\n", MODEM_SETUP);
#endif
		if (write(pp->io.fd, MODEM_SETUP, strlen(MODEM_SETUP)) <
		    0) {
			msyslog(LOG_ERR, "acts: write %m");
			return;
		}
		up->state = S_OK;
		up->timer = SETUP;
		return;

	/*
	 * In OK state the modem did not respond to setup.
	 */
	case S_OK:
		msyslog(LOG_ERR, "acts: no modem");
		break;

	/*
	 * In DTR state we are waiting for the modem to settle down
	 * before hammering it with a dial command.
	 */
	case S_DTR:
		sprintf(tbuf, "DIAL #%d %s", up->retry,
		    sys_phone[up->retry]);
		record_clock_stats(&peer->srcadr, tbuf);
#ifdef DEBUG
		if (debug)
			printf("%s\n", tbuf);
#endif
		write(pp->io.fd, sys_phone[up->retry],
		    strlen(sys_phone[up->retry]));
		write(pp->io.fd, "\r", 1);
		up->state = S_CONNECT;
		up->timer = ANSWER;
		return;

	/*
	 * In CONNECT state the call did not complete.
	 */
	case S_CONNECT:
		msyslog(LOG_ERR, "acts: no answer");
		break;

	/*
	 * In FIRST state no messages were received.
	 */
	case S_FIRST:
		msyslog(LOG_ERR, "acts: no messages");
		break;

	/*
	 * In CLOSE state hangup is complete. Close the doors and
	 * windows and get some air.
	 */
	case S_CLOSE:

		/*
		 * Close the device and unlock a shared modem.
		 */
		if (pp->io.fd) {
			io_closeclock(&pp->io);
			close(pp->io.fd);
			if (pp->sloppyclockflag & CLK_FLAG2) {
				sprintf(lockfile, LOCKFILE, up->unit);
				unlink(lockfile);
			}
			pp->io.fd = 0;
		}

		/*
		 * If messages were received, fold the tent and wait for
		 * the next poll. If no messages and there are more
		 * numbers to dial, retry after a short wait.
		 */
		up->bufptr = pp->a_lastcode;
		up->timer = 0;
		up->state = S_IDLE;
		if ( up->msgcnt == 0) {
			up->retry++;
			if (sys_phone[up->retry] == NULL)
				up->retry = 0;
			else
				up->timer = SETUP;
		} else {
			up->retry = 0;
		}
		up->msgcnt = 0;
		return;
	}
	acts_disc(peer);
}


/*
 * acts_disc - disconnect the call and clean the place up.
 */
static void
acts_disc (
	struct peer *peer
	)
{
	struct actsunit *up;
	struct refclockproc *pp;
	int	dtr = TIOCM_DTR;

	/*
	 * We get here if the call terminated successfully or if an
	 * error occured. If the median filter has something in it,feed
	 * the data to the clock filter. If a modem port, drop DTR to
	 * force command mode and send modem hangup.
	 */
	pp = peer->procptr;
	up = (struct actsunit *)pp->unitptr;
	if (up->msgcnt > 0)
		refclock_receive(peer);
	if (!(pp->sloppyclockflag & CLK_FLAG3)) {
		ioctl(pp->io.fd, TIOCMBIC, (char *)&dtr);
		write(pp->io.fd, MODEM_HANGUP, strlen(MODEM_HANGUP));
	}
	up->timer = SETUP;
	up->state = S_CLOSE;
}

#else
int refclock_acts_bs;
#endif /* REFCLOCK */
