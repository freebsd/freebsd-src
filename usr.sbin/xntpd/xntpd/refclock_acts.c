/*
 * refclock_acts - clock driver for the NIST Automated Computer Time
 *	Service aka Amalgamated Containerized Trash Service (ACTS)
 */
#if defined(REFCLOCK) && defined(ACTS)

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_unixtime.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

/*
 * This driver supports the NIST Automated Computer Time Service (ACTS).
 * It periodically dials a prespecified telephone number, receives the
 * NIST timecode data and calculates the local clock correction. It is
 * designed primarily for use as a backup when neither a radio clock nor
 * connectivity to Internet time servers is available. For the best
 * accuracy, the individual telephone line/modem delay needs to be
 * calibrated using outside sources.
 *
 * The ACTS is located at NIST Boulder, CO, telephone 303 494 4774. A
 * toll call from a residence telephone in Newark, DE, costs between 14
 * and 27 cents, depending on time of day, and from a campus telephone
 * between 3 and 4 cents, although it is not clear what carrier and time
 * of day discounts apply in this case. The modem dial string will
 * differ depending on local telephone configuration, etc., and is
 * specified by the phone command in the configuration file. The
 * argument to this command is an AT command for a Hayes compatible
 * modem.
 *
 * The accuracy produced by this driver should be in the range of a
 * millisecond or two, but may need correction due to the delay
 * characteristics of the individual modem involved. For undetermined
 * reasons, some modems work with the ACTS echo-delay measurement scheme
 * and some don't. This driver tries to do the best it can with what it
 * gets. Initial experiments with a Practical Peripherals 9600SA modem
 * here in Delaware suggest an accuracy of a millisecond or two can be
 * achieved without the scheme by using a fudge time1 value of 65.0 ms.
 * In either case, the dispersion for a single call involving ten
 * samples is about 1.3 ms.
 *
 * The driver can operate in either of three modes, as determined by
 * the mode parameter in the server configuration command. In mode 0
 * (automatic) the driver operates continuously at intervals depending
 * on the prediction error, as measured by the driver, usually in the
 * order of several hours. In mode 1 (backup) the driver is enabled in
 * automatic mode only when no other source of synchronization is
 * available and when more than MAXOUTAGE (3600 s) have elapsed since
 * last synchronized by other sources. In mode 2 (manual) the driver
 * operates only when enabled using a fudge flags switch, as described
 * below.
 *
 * For reliable call management, this driver requires a 1200-bps modem
 * with a Hayes-compatible command set and control over the modem data
 * terminal ready (DTR) control line. Present restrictions require the
 * use of a POSIX-compatible programming interface, although other
 * interfaces may work as well. The modem setup string is hard-coded in
 * the driver and may require changes for nonstandard modems or special
 * circumstances.
 *
 * Further information can be found in the README.refclock file in the
 * xntp3 distribution.
 *
 * Fudge Factors
 *
 * Ordinarily, the propagation time correction is computed automatically
 * by ACTS and the driver. When this is not possible or erratic due to
 * individual modem characteristics, the fudge flag2 switch should be
 * set to disable the ACTS echo-delay scheme. In any case, the fudge
 * time1 parameter can be used to adjust the propagation delay as
 * required.
 *
 * The ACTS call interval is determined in one of three ways. In manual
 * mode a call is initiated by setting fudge flag1 using xntpdc, either
 * manually or via a cron job. In AUTO mode this flag is set by the peer
 * timer, which is controlled by the sys_poll variable in response to
 * measured errors. In backup mode the driver is ordinarily asleep, but
 * awakes (in auto mode) if all other synchronization sources are lost.
 * In either auto or backup modes, the call interval increases as long
 * as the measured errors do not exceed the value of the fudge time2
 * parameter.
 *
 * When the fudge flag1 is set, the ACTS calling program is activated.
 * This program dials each number listed in the phones command of the
 * configuration file in turn. If a call attempt fails, the next number
 * in the list is dialed. The fudge flag1 and counter are reset and the
 * calling program terminated if (a) a valid clock update has been
 * determined, (b) no more numbers remain in the list, (c) a device
 * fault or timeout occurs or (d) fudge flag1 is reset manually using
 * xntpdc.
 *
 * In automatic and backup modes, the driver determines the call
 * interval using a procedure depending on the measured prediction
 * error and the fudge time2 parameter. If the error exceeds time2 for a
 * number of times depending on the current interval, the interval is
 * decreased, but not less than about 1000 s. If the error is less than
 * time2 for some number of times, the interval is increased, but not
 * more than about 18 h. With the default value of zero for fudge time2,
 * the interval will increase from 1000 s to the 4000-8000-s range, in
 * which the expected accuracy should be in the 1-2-ms range. Setting
 * fudge time2 to a large value, like 0.1 s, may result in errors of
 * that order, but increase the call interval to the maximum. The exact
 * value for each configuration will depend on the modem and operating
 * system involved, so some experimentation may be necessary.
 */

/*
 * DESCRIPTION OF THE AUTOMATED COMPUTER TELEPHONE SERVICE (ACTS)
 * (reformatted from ACTS on-line computer help information)
 *
 * The following is transmitted (at 1200 baud) following completion of
 * the telephone connection.
 *
 * National Institute of Standards and Technology
 * Telephone Time Service, Generator 3B
 * Enter question mark "?" for HELP
 *                         D  L D
 *  MJD  YR MO DA H  M  S  ST S UT1 msADV        <OTM>
 * 47999 90-04-18 21:39:15 50 0 +.1 045.0 UTC(NIST) *
 * 47999 90-04-18 21:39:16 50 0 +.1 045.0 UTC(NIST) *
 * 47999 90-04-18 21:39:17 50 0 +.1 045.0 UTC(NIST) *
 * 47999 90-04-18 21:39:18 50 0 +.1 045.0 UTC(NIST) *
 * 47999 90-04-18 21:39:19 50 0 +.1 037.6 UTC(NIST) #
 * 47999 90-04-18 21:39:20 50 0 +.1 037.6 UTC(NIST) #
 * etc..etc...etc.......
 *
 * UTC = Universal Time Coordinated, the official world time referred to
 * the zero meridian.
 *
 * DST	Daylight savings time characters, valid for the continental
 *	U.S., are set as follows:
 *
 *	00	We are on standard time (ST).
 *	01-49	Now on DST, go to ST when your local time is 2:00 am and
 *		the count is 01. The count is decremented daily at 00
 *		(UTC).
 *	50	We are on DST.
 *	51-99	Now on ST, go to DST when your local time is 2:00 am and
 *		the count is 51. The count is decremented daily at 00
 *		(UTC).
 *
 *	The two DST characters provide up to 48 days advance notice of a
 *	change in time. The count remains at 00 or 50 at other times.
 *
 * LS	Leap second flag is set to "1" to indicate that a leap second is
 *	to be added as 23:59:60 (UTC) on the last day of the current UTC
 *	month. The LS flag will be reset to "0" starting with 23:59:60
 *	(UTC). The flag will remain on for the entire month before the
 *	second is added. Leap seconds are added as needed at the end of
 *	any month. Usually June and/or December are chosen.
 *
 *	The leap second flag will be set to a "2" to indicate that a
 *	leap second is to be deleted at 23:59:58--00:00:00 on the last
 *	day of the current month. (This latter provision is included per
 *	international recommendation, however it is not likely to be
 *	required in the near future.)
 *
 * DUT1	Approximate difference between earth rotation time (UT1) and
 *	UTC, in steps of 0.1 second: DUT1 = UT1 - UTC.
 *
 * MJD	Modified Julian Date, often used to tag certain scientific data.
 *
 * The full time format is sent at 1200 baud, 8 bit, 1 stop, no parity.
 * The format at 300 Baud is also 8 bit, 1 stop, no parity. At 300 Baud
 * the MJD and DUT1 values are deleted and the time is transmitted only
 * on even seconds.
 *
 * Maximum on line time will be 56 seconds. If all lines are busy at any
 * time, the oldest call will be terminated if it has been on line more
 * than 28 seconds, otherwise, the call that first reaches 28 seconds
 * will be terminated.
 *
 * Current time is valid at the "on-time" marker (OTM), either "*" or
 * "#". The nominal on-time marker (*) will be transmitted 45 ms early
 * to account for the 8 ms required to send 1 character at 1200 Baud,
 * plus an additional 7 ms for delay from NIST to the user, and
 * approximately 30 ms "scrambler" delay inherent in 1200 Baud modems.
 * If the caller echoes all characters, NIST will measure the round trip
 * delay and advance the on-time marker so that the midpoint of the stop
 * bit arrives at the user on time. The amount of msADV will reflect the
 * actual required advance in milliseconds and the OTM will be a "#".
 *
 * (The NIST system requires 4 or 5 consecutive delay measurements which
 * are consistent before switching from "*" to "#". If the user has a
 * 1200 Baud modem with the same internal delay as that used by NIST,
 * then the "#" OTM should arrive at the user within +-2 ms of the
 * correct time. 
 *
 * However, NIST has studied different brands of 1200 Baud modems and
 * found internal delays from 24 ms to 40 ms and offsets of the "#" OTM
 * of +-10 ms. For many computer users, +-10 ms accuracy should be more
 * than adequate since many computer internal clocks can only be set
 * with granularity of 20 to 50 ms. In any case, the repeatability of
 * the offset for the "#" OTM should be within +-2 ms, if the dial-up
 * path is reciprocal and the user doesn't change the brand or model of
 * modem used. 
 *
 * This should be true even if the dial-up path on one day is a land-
 * line of less than 40 ms (one way) and on the next day is a satellite
 * link of 260 to 300 ms. In the rare event that the path is one way by
 * satellite and the other way by land line with a round trip
 * measurement in the range of 90 to 260 ms, the OTM will remain a "*"
 * indicating 45 ms advance.
 *
 * For user comments write:
 * NIST-ACTS
 * Time and Frequency Division
 * Mail Stop 847
 * 325 Broadway
 * Boulder, CO 80303
 *
 * Software for setting (PC)DOS compatable machines is available on a
 * 360-kbyte diskette for $35.00 from: NIST Office of Standard Reference
 * Materials B311-Chemistry Bldg, NIST, Gaithersburg, MD, 20899, (301)
 * 975-6776
 */

/*
 * Interface definitions
 */
#define	DEVICE		"/dev/acts%d" /* device name and unit */
#define	SPEED232	B1200	/* uart speed (1200 cowardly baud) */
#define	PRECISION	(-10)	/* precision assumed (about 1 ms) */
#define	REFID		"ACTS"	/* reference ID */
#define	DESCRIPTION	"NIST Automated Computer Time Service" /* WRU */

#define MODE_AUTO	0	/* automatic mode */
#define MODE_BACKUP	1	/* backup mode */
#define MODE_MANUAL	2	/* manual mode */

#define	NSAMPLES	3	/* stages of median filter */
#define MSGCNT		10	/* we need this many ACTS messages */
#define SMAX		80	/* max token string length */
#define LENCODE		50	/* length of valid timecode string */
#define ACTS_MINPOLL	10	/* log2 min poll interval (1024 s) */
#define ACTS_MAXPOLL	14	/* log2 max poll interval (16384 s) */
#define MAXOUTAGE	3600	/* max before ACTS kicks in (s) */

/*
 * Modem control strings. These may have to be changed for some modems.
 *
 * AT	command prefix
 * B1	initiate call negotiation using Bell 212A
 * &C1	enable carrier detect
 * &D2	hang up and return to command mode on DTR transition
 * E0	modem command echo disabled
 * l1	set modem speaker volume to low level
 * M1	speaker enabled untill carrier detect
 * Q0	return result codes
 * V1	return result codes as English words
 */
#define MODEM_SETUP	"ATB1&C1&D2E0L1M1Q0V1" /* modem setup */
#define MODEM_HANGUP	"ATH"	/* modem disconnect */

/*
 * Timeouts
 */
#define IDLE		60	/* idle timeout (s) */
#define WAIT		2	/* wait timeout (s) */
#define ANSWER		30	/* answer timeout (s) */
#define CONNECT		10	/* connect timeout (s) */
#define TIMECODE	15	/* timecode timeout (s) */

/*
 * Imported from ntp_timer module
 */
extern	u_long	current_time;	/* current time (s) */
extern	u_long	last_time;	/* last clock update time (s) */
extern	struct event timerqueue[]; /* inner space */

/*
 * Imported from ntpd module
 */
extern	int	debug;		/* global debug flag */

/*
 * Imported from ntp_config module
 */
extern	char	sys_phone[][MAXDIAL]; /* modem dial strings */

/*
 * Imported from ntp_proto module
 */
extern	struct peer *sys_peer;	/* who is running the show */
extern	u_char sys_poll;	/* log2 of system poll interval */
extern	struct peer *sys_peer;	/* system peer structure pointer */

/*
 * Tables to compute the ddd of year form icky dd/mm timecode. Viva la
 * leap.
 */
static day1tab[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
static day2tab[] = {31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/*
 * Unit control structure
 */
struct actsunit {
	struct	event timer;	/* timeout timer */
	int	pollcnt;	/* poll message counter */

	int	state;		/* the first one was Delaware */
	int	run;		/* call program run switch */
	int	msgcnt;		/* count of ACTS messages received */
	long	redial;		/* interval to next automatic call */
	double	msADV;		/* millisecond advance of last message */
};

/*
 * Function prototypes
 */
static	int	acts_start	P((int, struct peer *));
static	void	acts_shutdown	P((int, struct peer *));
static	void	acts_receive	P((struct recvbuf *));
static	void	acts_poll	P((int, struct peer *));
static	void	acts_timeout	P((struct peer *));
static	void	acts_disc	P((struct peer *));
static	int	acts_write	P((struct peer *, char *));

/*
 * Transfer vector
 */
struct	refclock refclock_acts = {
	acts_start,		/* start up driver */
	acts_shutdown,		/* shut down driver */
	acts_poll,		/* transmit poll message */
	noentry,		/* not used (old acts_control) */
	noentry,		/* not used (old acts_init) */
	noentry,		/* not used (old acts_buginfo) */
	NOFLAGS			/* not used */
};


/*
 * acts_start - open the devices and initialize data for processing
 */
static int
acts_start(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct actsunit *up;
	struct refclockproc *pp;
	int fd;
	char device[20];
	int dtr = TIOCM_DTR;

	/*
	 * Open serial port. Use ACTS line discipline, if available. It
	 * pumps a timestamp into the data stream at every on-time
	 * character '*' found. Note: the port must have modem control
	 * or deep pockets for the phone bill. HP-UX 9.03 users should
	 * have very deep pockets.
	 */
	(void)sprintf(device, DEVICE, unit);
	if (!(fd = refclock_open(device, SPEED232, LDISC_ACTS)))
		return (0);
	if (ioctl(fd, TIOCMBIC, (char *)&dtr) < 0) {
		syslog(LOG_ERR, "clock %s ACTS no modem control",
		    ntoa(&peer->srcadr));
		return (0);
	}

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct actsunit *)
	    emalloc(sizeof(struct actsunit)))) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct actsunit));
	pp = peer->procptr;
	pp->io.clock_recv = acts_receive;
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
	peer->minpoll = ACTS_MINPOLL;
	peer->maxpoll = ACTS_MAXPOLL;

	/*
	 * Initialize modem and kill DTR. We skedaddle if this comes
	 * bum.
	 */
	if (!acts_write(peer, MODEM_SETUP)) {
		(void) close(fd);
		free(up);
		return (0);
	}

	/*
	 * Set up the driver timeout
	 */
	up->timer.peer = (struct peer *)peer;
	up->timer.event_handler = acts_timeout;
	up->timer.event_time = current_time + WAIT;
	TIMER_INSERT(timerqueue, &up->timer);
	return (1);
}


/*
 * acts_shutdown - shut down the clock
 */
static void
acts_shutdown(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct actsunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct actsunit *)pp->unitptr;
	TIMER_DEQUEUE(&up->timer);
	io_closeclock(&pp->io);
	free(up);
}


/*
 * acts_receive - receive data from the serial interface
 */
static void
acts_receive(rbufp)
	struct recvbuf *rbufp;
{
	register struct actsunit *up;
	struct refclockproc *pp;
	struct peer *peer;
	char str[SMAX];
	int i;
	l_fp tstmp;
	u_fp disp;
	char hangup = '%';	/* ACTS hangup */
	int day;		/* day of the month */
	int month;		/* month of the year */
	u_long mjd;		/* Modified Julian Day */
	u_int dst;		/* daylight/standard time indicator */
	u_int leap;		/* leap-second indicator */
	double dut1;		/* DUT adjustment */
	double msADV;		/* ACTS transmit advance (ms) */
	char utc[10];		/* this is NIST and you're not */
	char flag;		/* calibration flag */

	/*
	 * Initialize pointers and read the timecode and timestamp. If
	 * the OK modem status code, leave it where folks can find it.
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct actsunit *)pp->unitptr;
	pp->lencode = refclock_gtlin(rbufp, pp->lastcode, BMAX,
	    &pp->lastrec);
	if (pp->lencode == 0) {
		if (strcmp(pp->lastcode, "OK") == 0)
			pp->lencode = 2;
		return;
	}
#ifdef DEBUG
	if (debug)
        	printf("acts: timecode %d %s\n", pp->lencode,
		    pp->lastcode);
#endif

	switch (up->state) {

		case 0:

		/*
		 * State 0. We are not expecting anything. Probably
		 * modem disconnect noise. Go back to sleep.
		 */
		return;

		case 1:

		/*
		 * State 1. We are waiting for the call to be answered.
		 * All we care about here is CONNECT as the first token
		 * in the string. If the modem signals BUSY, ERROR, NO
		 * ANSWER, NO CARRIER or NO DIALTONE, we immediately
		 * hang up the phone. If CONNECT doesn't happen after
		 * ANSWER seconds, hang up the phone. If everything is
		 * okay, start the connect timeout and slide into state
		 * 2.
		 */
		(void)strncpy(str, strtok(pp->lastcode, " "), SMAX);
		if (strcmp(str, "BUSY") == 0 || strcmp(str, "ERROR") ==
		     0 || strcmp(str, "NO") == 0) {
			TIMER_DEQUEUE(&up->timer);
			syslog(LOG_NOTICE,
			    "clock %s ACTS modem status %s",
			    ntoa(&peer->srcadr), pp->lastcode);
			acts_disc(peer);
		} else if (strcmp(str, "CONNECT") == 0) {
			TIMER_DEQUEUE(&up->timer);
			up->timer.event_time = current_time + CONNECT;
			TIMER_INSERT(timerqueue, &up->timer);
			up->msgcnt = 0;
			up->state++;
		}
		return;

		case 2:

		/*
		 * State 2. The call has been answered and we are
		 * waiting for the first ACTS message. If this doesn't
		 * happen within the timecode timeout, hang up the
		 * phone. We probably got a wrong number or ACTS is
		 * down.
		 */
		TIMER_DEQUEUE(&up->timer);
		up->timer.event_time = current_time + TIMECODE;
		TIMER_INSERT(timerqueue, &up->timer);
		up->state++;
	}

	/*
	 * Real yucky things here. Ignore everything except timecode
	 * messages, as determined by the message length. We told the
	 * terminal routines to end the line with '*' and the line
	 * discipline to strike a timestamp on that character. However,
	 * when the ACTS echo-delay scheme works, the '*' eventually
	 * becomes a '#'. In this case the message is ended by the <CR>
	 * that comes about 200 ms after the '#' and the '#' cannot be
	 * echoed at the proper time. But, this may not be a lose, since
	 * we already have good data from prior messages and only need
	 * the millisecond advance calculated by ACTS. So, if the
	 * message is long enough and has an on-time character at the
	 * right place, we consider the message (but not neccesarily the
	 * timestmap) to be valid.
	 */
	if (pp->lencode != LENCODE)
		return;

	/*
	 * We apparently have a valid timecode message, so dismember it
	 * with sscan(). This routine does a good job in spotting syntax
	 * errors without becoming overly pedantic.
	 *
	 *                         D  L D
	 *  MJD  YR MO DA H  M  S  ST S UT1 msADV         OTM
	 * 47222 88-03-02 21:39:15 83 0 +.3 045.0 UTC(NBS) *
	 */
	if (sscanf(pp->lastcode,
	    "%5ld %2d-%2d-%2d %2d:%2d:%2d %2d %1d %3lf %5lf %s %c",
	    &mjd, &pp->year, &month, &day, &pp->hour, &pp->minute,
	    &pp->second, &dst, &leap, &dut1, &msADV, utc, &flag) != 13) {
		refclock_report(peer, CEVNT_BADREPLY);
		return;
	}

	/*
	 * Some modems can't be trusted (the Practical Peripherals
	 * 9600SA comes to mind) and, even if they manage to unstick
	 * ACTS, the millisecond advance is wrong, so we use CLK_FLAG2
	 * to disable echoes, if neccessary.
	 */
	if ((flag == '*' || flag == '#') && !(pp->sloppyclockflag &
	    CLK_FLAG2))
		(void)write(pp->io.fd, &flag, 1);

	/*
	 * Yes, I know this code incorrectly thinks that 2000 is a leap
	 * year. The ACTS timecode format croaks then anyway. Life is
	 * short. Would only the timecode mavens resist the urge to
	 * express months of the year and days of the month in favor of
	 * days of the year.
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
	if (leap == 1)
		pp->leap = LEAP_ADDSECOND;
	else if (pp->leap == 2)
		pp->leap = LEAP_DELSECOND;
	else
		pp->leap = 0;
	pp->lasttime = current_time;

	/*
	 * Colossal hack here. We process each sample in a trimmed-mean
	 * filter and determine the reference clock offset and
	 * dispersion. The fudge time1 value is added to each sample as
	 * received. If we collect MSGCNT samples before the '#' on-time
	 * character, we use the results of the filter as is. If the '#'
	 * is found before that, the adjusted msADV is used to correct
	 * the propagation delay.
	 */
	up->msgcnt++;
	if (flag == '#') {
		L_CLR(&tstmp);
		TVUTOTSF((long)((msADV - up->msADV) * 1000.),
		    tstmp.l_uf);
		L_ADD(&pp->offset, &tstmp);
	} else {
		up->msADV = msADV;
		if (!refclock_process(pp, up->msgcnt, up->msgcnt -
		    up->msgcnt / 3)) {
			refclock_report(peer, CEVNT_BADTIME);
			return;
		} else if (up->msgcnt < MSGCNT)
			return;
	}

	/*
	 * We have a filtered sample offset ready for peer processing.
	 * We use lastrec as both the reference time and receive time in
	 * order to avoid being cute, like setting the reference time
	 * later than the receive time, which may cause a paranoid
	 * protocol module to chuck out the data. Finaly, we unhook the
	 * timeout, arm for the next call, fold the tent and go home.
	 * The little dance with the '%' character is an undocumented
	 * ACTS feature that hangs up the phone real quick without
	 * waiting for carrier loss or long-space disconnect, but we do
	 * these clumsy things anyway.
	 */
	disp = LFPTOFP(&pp->fudgetime2);
	record_clock_stats(&peer->srcadr, pp->lastcode);
	refclock_receive(peer, &pp->offset, 0, pp->dispersion +
	    (u_fp)disp, &pp->lastrec, &pp->lastrec, pp->leap);
	pp->sloppyclockflag &= ~CLK_FLAG1;
	up->pollcnt = 0;
	TIMER_DEQUEUE(&up->timer);
	(void)write(pp->io.fd, &hangup, 1);
	up->state = 0;
	acts_disc(peer);
}


/*
 * acts_poll - called by the transmit routine
 */
static void
acts_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct actsunit *up;
	struct refclockproc *pp;

	/*
	 * If the driver is running, we set the enable flag (fudge
	 * flag1), which causes the driver timeout routine to initiate a
	 * call to ACTS. If not, the enable flag can be set using
	 * xntpdc. If this is the sustem peer, then follow the system
	 * poll interval.
	 */
	pp = peer->procptr;
	up = (struct actsunit *)pp->unitptr;
	if (up->run) {
		pp->sloppyclockflag |= CLK_FLAG1;
		if (peer == sys_peer)
			peer->hpoll = sys_poll;
		else
			peer->hpoll = peer->minpoll;
	}
}


/*
 * acts_timeout - called by the timer interrupt
 */
static void
acts_timeout(peer)
	struct peer *peer;
{
	register struct actsunit *up;
	struct refclockproc *pp;
	int dtr = TIOCM_DTR;

	/*
	 * If a timeout occurs in other than state 0, the call has
	 * failed. If in state 0, we just see if there is other work to
	 * do.
	 */
	pp = peer->procptr;
	up = (struct actsunit *)pp->unitptr;
	if (up->state) {
		acts_disc(peer);
		return;
	}
	switch (peer->ttl) {

		/*
		 * In manual mode the ACTS calling program is activated
		 * by the xntpdc program using the enable flag (fudge
		 * flag1), either manually or by a cron job.
		 */
		case MODE_MANUAL:
		up->run = 0;
		break;

		/*
		 * In automatic mode the ACTS calling program runs
		 * continuously at intervals determined by the sys_poll
		 * variable.
		 */
		case MODE_AUTO:
		if (!up->run)
			pp->sloppyclockflag |= CLK_FLAG1;
		up->run = 1;
		break;

		/*
		 * In backup mode the ACTS calling program is disabled,
		 * unless no system peer has been selected for MAXOUTAGE
		 * (3600 s). Once enabled, it runs until some other NTP
		 * peer shows up.
		 */
		case MODE_BACKUP:
		if (!up->run && sys_peer == 0) {
			if (current_time - last_time > MAXOUTAGE) {
				up->run = 1;
				peer->hpoll = peer->minpoll;
				syslog(LOG_NOTICE,
				    "clock %s ACTS backup started ",
				    ntoa(&peer->srcadr));
			}
		} else if (up->run && sys_peer->refclktype !=
		    REFCLK_NIST_ACTS) {
			peer->hpoll = peer->minpoll;
			up->run = 0;
			syslog(LOG_NOTICE,
			    "clock %s ACTS backup stopped",
			    ntoa(&peer->srcadr));
		}
		break;

		default:
		syslog(LOG_NOTICE,
		    "clock %s ACTS invalid mode", ntoa(&peer->srcadr));
		
	}

	/*
	 * The fudge flag1 is used as an enable/disable; if set either
	 * by the code or via xntpdc, the ACTS calling program is
	 * started; if reset, the phones stop ringing.
	 */
	if (!(pp->sloppyclockflag & CLK_FLAG1)) {
		up->pollcnt = 0;
		up->timer.event_time = current_time + IDLE;
		TIMER_INSERT(timerqueue, &up->timer);
		return;
	}

	/*
	 * Initiate a call to the ACTS service. If we wind up here in
	 * other than state 0, a successful call could not be completed
	 * within minpoll seconds. We advance to the next modem dial
	 * string. If none are left, we log a notice and clear the
	 * enable flag. For future enhancement: call the site RP and
	 * leave an obscene message in his voicemail.
	 */
	if (sys_phone[up->pollcnt][0] == '\0') {
		refclock_report(peer, CEVNT_TIMEOUT);
		syslog(LOG_NOTICE,
		    "clock %s ACTS calling program terminated",
		    ntoa(&peer->srcadr));
		pp->sloppyclockflag &= ~CLK_FLAG1;
#ifdef DEBUG
		if (debug)
			printf("acts: calling program terminated\n");
#endif
		up->pollcnt = 0;
		up->timer.event_time = current_time + IDLE;
		TIMER_INSERT(timerqueue, &up->timer);
		return;
	}

	/*
	 * Raise DTR, call ACTS and start the answer timeout. We think
	 * it strange if the OK status has not been received from the
	 * modem, but plow ahead anyway.
	 */
	if (strcmp(pp->lastcode, "OK") != 0)
		syslog(LOG_NOTICE, "clock %s ACTS no modem status",
		    ntoa(&peer->srcadr));
	(void)ioctl(pp->io.fd, TIOCMBIS, (char *)&dtr);
	(void)acts_write(peer, sys_phone[up->pollcnt]);
	syslog(LOG_NOTICE, "clock %s ACTS calling %s\n",
	    ntoa(&peer->srcadr), sys_phone[up->pollcnt]);
	up->state = 1;
	up->pollcnt++;
	pp->polls++;
	up->timer.event_time = current_time + ANSWER;
	TIMER_INSERT(timerqueue, &up->timer);
}


/*
 * acts_disc - disconnect the call and wait for the ruckus to cool
 */
static void
acts_disc(peer)
	struct peer *peer;
{
	register struct actsunit *up;
	struct refclockproc *pp;
	int dtr = TIOCM_DTR;

	/*
	 * We should never get here other than in state 0, unless a call
	 * has timed out. We drop DTR, which will reliably get the modem
	 * off the air, even while ACTS is hammering away full tilt.
	 */
	pp = peer->procptr;
	up = (struct actsunit *)pp->unitptr;
	(void)ioctl(pp->io.fd, TIOCMBIC, (char *)&dtr);
	if (up->state > 0) {
		up->state = 0;
		syslog(LOG_NOTICE, "clock %s ACTS call failed %d",
		    ntoa(&peer->srcadr), up->state);
#ifdef DEBUG
		if (debug)
			printf("acts: call failed %d\n", up->state);
#endif
	}
	up->timer.event_time = current_time + WAIT;
	TIMER_INSERT(timerqueue, &up->timer);
}


/*
 * acts_write - write a message to the serial port
 */
static int
acts_write(peer, str)
	struct peer *peer;
	char *str;
{
	register struct actsunit *up;
	struct refclockproc *pp;
	int len;
	int code;
	char cr = '\r';

	/*
	 * Not much to do here, other than send the message, handle
	 * debug and report faults.
	 */
	pp = peer->procptr;
	up = (struct actsunit *)pp->unitptr;
	len = strlen(str);
#ifdef DEBUG
	if (debug)
		printf("acts: state %d send %d %s\n", up->state, len,
		    str);
#endif
	code = write(pp->io.fd, str, len) == len;
	code |= write(pp->io.fd, &cr, 1) == 1;
	if (!code)
		refclock_report(peer, CEVNT_FAULT);
	return (code);
}

#endif
