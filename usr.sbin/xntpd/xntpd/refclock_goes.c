/*
 * refclock_goes - clock driver for the Kinemetrics Truetime GOES
 *	Receiver Version 3.0C - tested plain, with CLKLDISC
 *	Developement work being done:
 * 	- Properly handle varying satellite positions (more acurately)
 *	- Integrate GPSTM and/or OMEGA and/or TRAK and/or ??? drivers
 */

#if defined(REFCLOCK) && defined(GOES)

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"
#include "ntp_stdlib.h"

/*
 * Support for Kinemetrics Truetime 468-DC GOES Receiver
 *   OM-DC OMEGA and GPS-TM/TMD support in progress...
 *
 * Most of this code is originally from refclock_wwvb.c with thanks.
 * It has been so mangled that wwvb is not a recognizable ancestor.
 *
 * Timcode format: ADDD:HH:MM:SSQCL
 *	A - control A
 *	Q Quality indication: indicates possible error of
 *	C - Carriage return
 *	L - Line feed
 *
 * Quality codes indicate possible error of
 *   468-DC GOES Receiver:
 *   GPS-TM/TMD Receiver:
 *       ?     +/- 500 milliseconds	#     +/- 50 milliseconds
 *       *     +/- 5 milliseconds	.     +/- 1 millisecond
 *     space   less than 1 millisecond
 *   OM-DC OMEGA Receiver:
 *       >     >+- 5 seconds
 *       ?     >+/- 500 milliseconds    #     >+/- 50 milliseconds
 *       *     >+/- 5 milliseconds      .     >+/- 1 millisecond
 *      A-H    less than 1 millisecond.  Character indicates which station
 *             is being received as follows:
 *             A = Norway, B = Liberia, C = Hawaii, D = North Dakota,
 *             E = La Reunion, F = Argentina, G = Australia, H = Japan.
 *
 * The carriage return start bit begins on 0 seconds and extends to 1
 * bit time
 *
 * Notes on 468-DC and OMEGA receiver:
 *
 * Send the clock a 'R' or 'C' and once per second a timestamp will
 * appear.  Send a 'P' to get the satellite position once.
 *
 * Notes on the 468-DC receiver:
 *
 * Unless you live on 125 degrees west longitude, you can't
 * set your clock propagation delay settings correctly and still use
 * automatic mode. The manual says to use a compromise when setting the
 * switches. This results in significant errors. The solution; use fudge
 * time1 and time2 to incorporate corrections. If your clock is set for
 * 50 and it should be 58 for using the west and 46 for using the east,
 * use the line
 *
 * fudge 127.127.5.0 time1 +0.008 time2 -0.004
 *
 * This corrects the 4 milliseconds advance and 8 milliseconds retard
 * needed. The software will ask the clock which satellite it sees.
 *
 * Ntp.conf parameters:
 * time1 - offset applied to samples when reading WEST satellite (default = 0)
 * time2 - offset applied to samples when reading EAST satellite (default = 0)
 * val1  - stratum to assign to this clock (default = 0)
 * val2  - refid assigned to this clock (default = "GOES", see below)
 * flag1 - will silence the clock side of xntpd, just reading the clock
 *         without trying to write to it.  (default = 0)
 * flag2 - not assigned
 * flag3 - enable ppsclock streams module
 * flag4 - not assigned
 *
 */

/*
 * Definitions
 */
#define	DEVICE	"/dev/goes%d"
#define	SPEED232	B9600	/* 9600 baud */

/*
 * Radio interface parameters
 */
#define	MAXDISPERSE	(FP_SECOND>>1) /* max error for synchronized clock (0.5 s as an u_fp) */
#define	PRECISION	(-10)	/* precision assumed (about 1 ms) */
#define	REFID		"GOES"	/* reference id */
#define	DESCRIPTION	"TrueTime GPS/GOES Receivers" /* WRU */
#define	NSAMPLES	3	/* stages of median filter */

/*
 * Tags which station (satellite) we see
 */
#define GOES_WEST	0	/* Default to WEST satellite and apply time1 */
#define GOES_EAST	1	/* until you discover otherwise */

/*
 * used by the state machine
 */
enum goes_event {e_Init, e_F18, e_F50, e_F51, e_TS};

/*
 * Imported from the timer module
 */
extern u_long current_time;

/*
 * Imported from ntpd module
 */
extern int debug;		/* global debug flag */

/*
 * unit control structure
 */
struct goesunit {
	int	pollcnt;	/* poll message counter */
	u_short station;	/* which station we are on */
	u_short polled;		/* Hand in a time sample? */
	enum {Base, Start, F18, F50, F51, F08}
		State;		/* State machine */
};

/*
 * Function prototypes
 */
static	int	goes_start	P((int, struct peer *));
static	void	goes_shutdown	P((int, struct peer *));
static	void	goes_receive	P((struct recvbuf *));
static	void	goes_poll	P((int, struct peer *));
static	void	goes_send	P((struct peer *, char *));
static	void	goes_initstate	P((struct peer *));
static	void	goes_doevent	P((struct peer *, enum goes_event));

/*
 * Transfer vector
 */
struct	refclock refclock_goes = {
	goes_start,		/* start up driver */
	goes_shutdown,		/* shut down driver */
	goes_poll,		/* transmit poll message */
	noentry,		/* not used (old goes_control) */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used (old goes_buginfo) */
	NOFLAGS			/* not used */
};


/*
 * goes_start - open the devices and initialize data for processing
 */
static int
goes_start(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct goesunit *up;
	struct refclockproc *pp;
	int fd;
	char device[20];

	/*
	 * Open serial port
	 */
	(void)sprintf(device, DEVICE, unit);
	if (!(fd = refclock_open(device, SPEED232, LDISC_CLK)))
		return (0);

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct goesunit *)
	    emalloc(sizeof(struct goesunit)))) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct goesunit));
	pp = peer->procptr;
	pp->io.clock_recv = goes_receive;
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
/*	goes_initstate(peer);*/
	return (1);
}


/*
 * goes_shutdown - shut down the clock
 */
static void
goes_shutdown(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct goesunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct goesunit *)pp->unitptr;
	io_closeclock(&pp->io);
	free(up);
}


/*
 * goes_receive - receive data from the serial interface on a
 * Kinimetrics clock
 */
static void
goes_receive(rbufp)
	struct recvbuf *rbufp;
{
	register struct goesunit *up;
	struct refclockproc *pp;
	struct peer *peer;
	l_fp tmp_l_fp;
	u_short new_station;
	char sync, c1, c2;
	int i;
	int lat, lon, off;	/* GOES Satellite position */

	/*
	 * Get the clock this applies to and pointers to the data
	 */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct goesunit *)pp->unitptr;

	/*
	 * Read clock output.  Automatically handles STREAMS, CLKLDISC
	 */
	pp->lencode = refclock_gtlin(rbufp, pp->lastcode, BMAX,
		&pp->lastrec);

	/*
	 * There is a case where <cr><lf> generates 2 timestamps
	 */
	if (pp->lencode == 0)
		return;

	up->pollcnt = 2;
	record_clock_stats(&peer->srcadr, pp->lastcode);

	/*
	 * We get down to business, check the timecode format and decode
	 * its contents. This code decodes a multitude of different
	 * clock messages. Timecodes are processed if needed. All replies
	 * will be run through the state machine to tweak driver options
	 * and program the clock.
	 */

	/*
	 * Timecode: "nnnnn+nnn-nnn"
	 */
	if (sscanf(pp->lastcode, "%5d%c%3d%c%3d",
		&lon, &c1, &lat, &c2, &off) == 5 &&
		(c1 == '+' || c1 == '-') &&
		(c2 == '+' || c2 == '-')) {

		/*
		 * This is less than perfect.  Call the (satellite)
		 * either EAST or WEST and adjust slop accodingly
		 * Perfectionists would recalcuted the exact delay
		 * and adjust accordingly...
		 */
		if (lon > 7000 && lon < 14000) {
			if (lon < 10000)
				new_station = GOES_EAST;
			else
				new_station = GOES_WEST;
#ifdef DEBUG
			if (debug) {
				if (new_station == GOES_EAST)
					printf("goes: station EAST\n");
				if (new_station == GOES_WEST)
					printf("goes: station WEST\n");
			}
#endif
			if (new_station != up->station) {
				tmp_l_fp = pp->fudgetime1;
				pp->fudgetime1 = pp->fudgetime2;
				pp->fudgetime2 = tmp_l_fp;
				up->station = new_station;
			}
		}
		else {
			refclock_report(peer, CEVNT_BADREPLY);
#ifdef DEBUG
			if (debug)
				printf("goes: station UNKNONW\n");
#endif
		}
		/*
		 * Switch back to on-second time codes and return.
		 */
		goes_send(peer, "C");

		return;
	}

	/*
	 * Timecode: "Fnn"
	 */
	if (sscanf(pp->lastcode, "F%2d", &i) == 1 &&
		i > 0 && i < 80) {
		enum goes_event event = 0;

		if (i == 50) event = e_F50;
		if (i == 51) event = e_F51;
		if (i == 50 || i == 51) {
			goes_doevent(peer, event);
			return;
		}
	}

	/*
	 * Timecode:" TRUETIME Mk III"
	 */
	if (strcmp(pp->lastcode, " TRUETIME Mk III") == 0) {
		enum goes_event event;

		event = e_F18;
		goes_doevent(peer, event);
		return;
	}

	/*
 	 * Timecode: "ddd:hh:mm:ssQ"
 	 */
	if (sscanf(pp->lastcode, "%3d:%2d:%2d:%2d%c",
	    &pp->day, &pp->hour, &pp->minute,
	    &pp->second, &sync) == 5) {

		/*
		 * Adjust the synchronize indicator according to timecode
		 */
		if (sync !=' ' && sync !='.' && sync !='*')
			pp->leap = LEAP_NOTINSYNC;
		else {
			pp->leap = 0;
			pp->lasttime = current_time;
		}

		/* goes_doevent(peer, e_TS); */

		/*
		 * The clock will blurt a timecode every second but we only
		 * want one when polled.  If we havn't been polled, bail out.
		 */
		if (!up->polled)
			return;

		/*
		 * After each poll, check the station (satellite)
		 */
		goes_send(peer, "P");

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

		/*
		 * We have succedded in answering the poll.
		 * Turn off the flag and return
		 */
		up->polled = 0;

		return;
	}

	/*
	 * No match to known timecodes, report failure and return
	 */
	refclock_report(peer, CEVNT_BADREPLY);
	return;
}


/*
 * goes_send - time to send the clock a signal to cough up a time sample
 */
static void
goes_send(peer, cmd)
	struct peer *peer;
	char *cmd;
{
	struct refclockproc *pp;
	register int len = strlen(cmd);

	pp = peer->procptr;
	if (!pp->sloppyclockflag & CLK_FLAG1) {
#ifdef DEBUG
		if (debug)
			printf("goes: Send '%s'\n", cmd);
#endif
		if (write(pp->io.fd, cmd, len) != len) {
			refclock_report(peer, CEVNT_FAULT);
		} else {
			pp->polls++;
		}
	}
}


/*
 * state machine for initializing the clock
 */
static void
goes_doevent(peer, event)
	struct peer *peer;
	enum goes_event event;
{
	struct goesunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct goesunit *)pp->unitptr;

#ifdef DEBUG
	if (debug) {
		printf("goes_doevent: %d\n", (int)event);
	}
#endif
	if (event == e_TS && up->State != F51 && up->State != F08) {
		goes_send(peer, "\03\r");
	}

	switch (event) {
	case e_Init:
		goes_send(peer, "F18\r");
		up->State = Start;
		break;
	case e_F18:
		goes_send(peer, "F50\r");
		up->State = F18;
		break;
	case e_F50:
		goes_send(peer, "F51\r");
		up->State = F50;
		break;
	case e_F51:
		goes_send(peer, "F08\r");
		up->State = F51;
		break;
	case e_TS:
		/* nothing to send - we like this mode */
		up->State = F08;
		break;
	}
}

static void
goes_initstate(peer)
	struct peer *peer;
{
	struct goesunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct goesunit *)pp->unitptr;
	up->State = Base;		/* just in case */
	goes_doevent(peer, e_Init);
}


/*
 * goes_poll - called by the transmit procedure
 */
static void
goes_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	struct goesunit *up;
	struct refclockproc *pp;

	/*
	 * You don't need to poll this clock.  It puts out timecodes
	 * once per second.  If asked for a timestamp, take note.
	 * The next time a timecode comes in, it will be fed back.
	 */
	pp = peer->procptr;
	up = (struct goesunit *)pp->unitptr;
	if (up->pollcnt == 0) {
		refclock_report(peer, CEVNT_TIMEOUT);
		goes_send(peer, "C");
	}
	else
		up->pollcnt--;

	/*
	 * polled every 64 seconds. Ask goes_receive to hand in a
	 * timestamp.
	 */
	up->polled = 1;
	pp->polls++;
}

#endif
