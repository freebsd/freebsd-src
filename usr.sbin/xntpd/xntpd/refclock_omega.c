/*
 * refclock_omega - clock driver for the Kinemetrics Truetime OM-DC OMEGA
 *		    receiver.
 *
 * Version 1.0	11-Dec-92	Steve Clift (clift@ml.csiro.au)
 *	Initial version, mostly lifted from refclock_goes.c.
 *
 *	1.1  03-May-93  Steve Clift
 *	Tarted up the sample filtering mechanism to give improved
 *	one-off measurements.  Improved measurement dispersion code
 *	to account for accumulated drift when the clock loses lock.
 *		
 */

#if defined(REFCLOCK) && (defined(OMEGA) || defined(OMEGACLK) || defined(OMEGAPPS))

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"

#if defined(HAVE_BSD_TTYS)
#include <sgtty.h>
#endif /* HAVE_BSD_TTYS */

#if defined(HAVE_SYSV_TTYS)
#include <termio.h>
#endif /* HAVE_SYSV_TTYS */

#if defined(STREAM)
#include <termios.h>
#include <stropts.h>
#if defined(OMEGACLK)
#include <sys/clkdefs.h>
#endif /* OMEGACLK */
#endif /* STREAM */

#if defined (OMEGAPPS)
#include <sys/ppsclock.h>
#endif /* OMEGAPPS */

#include "ntp_stdlib.h"

/*
 * Support for Kinemetrics Truetime OM-DC OMEGA Receiver
 *
 * Most of this code is copied from refclock_goes.c with thanks.
 *
 * the time code looks like follows;  Send the clock a R or C and once per
 * second a timestamp will appear that looks like this:
 * ADDD:HH:MM:SSQCL
 * A - control A
 * Q Quality indication: indicates possible error of
 *     >     >+- 5 seconds
 *     ?     >+/- 500 milliseconds            #     >+/- 50 milliseconds
 *     *     >+/- 5 milliseconds              .     >+/- 1 millisecond
 *    A-H    less than 1 millisecond.  Character indicates which station
 *           is being received as follows:
 *           A = Norway, B = Liberia, C = Hawaii, D = North Dakota,
 *           E = La Reunion, F = Argentina, G = Australia, H = Japan.
 * C - Carriage return
 * L - Line feed
 * The carriage return start bit begins on 0 seconds and extends to 1 bit time.
 */

/*
 * Definitions
 */
#define	MAXUNITS	4	/* max number of OMEGA units */
#define	OMEGA232	"/dev/omega%d"
#define	SPEED232	B9600	/* 9600 baud */

/*
 * Radio interface parameters
 */
#define	OMEGADESCRIPTION "Kinemetrics OM-DC OMEGA Receiver" /* who we are */
#define	OMEGAMAXDISPERSE (FP_SECOND/32) /* max allowed sample dispersion */
#define	OMEGAPRECISION	(-10)	/* precision assumed (about 1 ms) */
#define	OMEGAREFID	"VLF\0"	/* reference id */
#define	OMEGAHSREFID	0x7f7f0b0a /* 127.127.11.10 refid hi strata */
#define LENOMEGA	13	/* length of standard response */
#define GMT		0	/* hour offset from Greenwich */
#define	NSTAMPS		9	/* samples collected when polled */
#define NSKEEP		5	/* samples to keep after discards */
#define BMAX		50	/* timecode buffer length */

/*
 * The OM-DC puts out the start bit of the <CR> on the second, but
 * we see the result after the <LF> is received, about 2ms later at
 * 9600 baud.  Use this as the default fudge time, and let the user
 * fiddle it to account for driver latency etc.
 */
#define	DEFFUDGETIME	0x00830000 /* default fudge time (~2ms) */

/*
 * Clock drift errors as u_fp values.
 */
#define U_FP5000MS	(5*FP_SECOND)	/* 5 seconds */
#define U_FP500MS	(FP_SECOND/2)	/* 500 msec */
#define U_FP50MS	(FP_SECOND/20)	/* 50 msec */
#define U_FP5MS		(FP_SECOND/200)	/* 5 msec */

/*
 * Station codes
 */
#define STATION_NONE		0
#define STATION_NORWAY		1
#define STATION_LIBERIA		2
#define STATION_HAWAII		3
#define STATION_N_DAKOTA	4
#define STATION_LA_REUNION	5
#define STATION_ARGENTINA	6
#define STATION_AUSTRALIA	7
#define STATION_JAPAN		8

/*
 * Hack to avoid excercising the multiplier.  I have no pride.
 */
#define	MULBY10(x)	(((x)<<3) + ((x)<<1))

/*
 * Imported from the timer module
 */
extern U_LONG current_time;
extern struct event timerqueue[];

/*
 * Imported from ntp_loopfilter module
 */
extern int fdpps;		/* pps file descriptor */

/*
 * Imported from ntpd module
 */
extern int debug;		/* global debug flag */

/*
 * OMEGA unit control structure
 */
struct omegaunit {
	struct peer *peer;		/* associated peer structure */
	struct refclockio io;		/* given to the I/O handler */
	l_fp lastrec;			/* last receive time */
	l_fp lastref;			/* last timecode time */
	l_fp offset[NSTAMPS];		/* recent sample offsets */
	char lastcode[BMAX];		/* last timecode received */
	u_short station;		/* which station we're locked to */
	u_short polled;			/* Hand in a time sample? */
	U_LONG coderecv;		/* timecodes received */
	u_char lencode;			/* length of last timecode */
	U_LONG lasttime;		/* last time clock heard from */
	u_char unit;			/* unit number for this guy */
	u_char status;			/* clock status */
	u_char lastevent;		/* last clock event */
	u_char reason;			/* reason for last failure */
	u_char year;			/* year of eternity */
	u_short day;			/* day of year */
	u_char hour;			/* hour of day */
	u_char minute;			/* minute of hour */
	u_char second;			/* seconds of minute */
	u_char leap;			/* leap indicators */
	u_short msec;			/* millisecond of second */
	u_char quality;			/* quality char from last timecode */
	U_LONG yearstart;		/* start of current year */
	/*
	 * Status tallies
 	 */
	U_LONG polls;			/* polls sent */
	U_LONG noreply;			/* no replies to polls */
	U_LONG badformat;		/* bad format */
	U_LONG baddata;			/* bad data */
	U_LONG timestarted;		/* time we started this */
};

/*
 * Data space for the unit structures.  Note that we allocate these on
 * the fly, but never give them back.
 */
static struct omegaunit *omegaunits[MAXUNITS];
static u_char unitinuse[MAXUNITS];

/*
 * Keep the fudge factors separately so they can be set even
 * when no clock is configured.
 */
static l_fp fudgefactor1[MAXUNITS];
static l_fp fudgefactor2[MAXUNITS];
static u_char stratumtouse[MAXUNITS];
static u_char readonlyclockflag[MAXUNITS];

/*
 * Function prototypes
 */
static	void	omega_init	P((void));
static	int	omega_start	P((u_int, struct peer *));
static	void	omega_shutdown	P((int));
static	void	omega_report_event	P((struct omegaunit *, int));
static	void	omega_receive	P((struct recvbuf *));
static	char	omega_process	P((struct omegaunit *, l_fp *, u_fp *));
static	void	omega_poll	P((int, struct peer *));
static	void	omega_control	P((u_int, struct refclockstat *, struct refclockstat *));
static	void	omega_buginfo	P((int, struct refclockbug *));
static	void	omega_send	P((struct omegaunit *, char *));

/*
 * Transfer vector
 */
struct	refclock refclock_omega = {
	omega_start, omega_shutdown, omega_poll,
	omega_control, omega_init, omega_buginfo, NOFLAGS
};

/*
 * omega_init - initialize internal omega driver data
 */
static void
omega_init()
{
	register int i;
	/*
	 * Just zero the data arrays
	 */
	bzero((char *)omegaunits, sizeof omegaunits);
	bzero((char *)unitinuse, sizeof unitinuse);

	/*
	 * Initialize fudge factors to default.
	 */
	for (i = 0; i < MAXUNITS; i++) {
		fudgefactor1[i].l_ui = 0;
		fudgefactor1[i].l_uf = DEFFUDGETIME;
		fudgefactor2[i].l_ui = 0;
		fudgefactor2[i].l_uf = 0;
		stratumtouse[i] = 0;
		readonlyclockflag[i] = 0;
	}
}


/*
 * omega_start - open the OMEGA devices and initialize data for processing
 */
static int
omega_start(unit, peer)
	u_int unit;
	struct peer *peer;
{
	register struct omegaunit *omega;
	register int i;
	int fd232;
	char omegadev[20];

	/*
	 * Check configuration info
	 */
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR,"omega_start: unit %d invalid", unit);
		return 0;
	}
	if (unitinuse[unit]) {
		syslog(LOG_ERR, "omega_start: unit %d in use", unit);
		return 0;
	}

	/*
	 * Open serial port
	 */
	(void) sprintf(omegadev, OMEGA232, unit);
	fd232 = open(omegadev, O_RDWR, 0777);
	if (fd232 == -1) {
		syslog(LOG_ERR, "omega_start: open of %s: %m", omegadev);
		return 0;
	}

#if defined(HAVE_SYSV_TTYS)
	/*
	 * System V serial line parameters (termio interface)
	 *
	 */
    {	struct termio ttyb;
	if (ioctl(fd232, TCGETA, &ttyb) < 0) {
                syslog(LOG_ERR,
		    "omega_start: ioctl(%s, TCGETA): %m", omegadev);
                goto screwed;
        }
        ttyb.c_iflag = IGNBRK|IGNPAR|ICRNL;
        ttyb.c_oflag = 0;
        ttyb.c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyb.c_lflag = ICANON;
	ttyb.c_cc[VERASE] = ttyb.c_cc[VKILL] = '\0';
        if (ioctl(fd232, TCSETA, &ttyb) < 0) {
                syslog(LOG_ERR,
		    "omega_start: ioctl(%s, TCSETA): %m", omegadev);
                goto screwed;
        }
    }
#endif /* HAVE_SYSV_TTYS */
#if defined(STREAM)
	/*
	 * POSIX/STREAMS serial line parameters (termios interface)
	 *
	 * The OMEGACLK option provides timestamping at the driver level. 
	 * It requires the tty_clk streams module.
	 *
	 * The OMEGAPPS option provides timestamping at the driver level.
	 * It uses a 1-pps signal and level converter (gadget box) and
	 * requires the ppsclock streams module and SunOS 4.1.1 or
	 * later.
	 */
    {	struct termios ttyb, *ttyp;

	ttyp = &ttyb;
	if (tcgetattr(fd232, ttyp) < 0) {
                syslog(LOG_ERR,
		    "omega_start: tcgetattr(%s): %m", omegadev);
                goto screwed;
        }
        ttyp->c_iflag = IGNBRK|IGNPAR|ICRNL;
        ttyp->c_oflag = 0;
        ttyp->c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyp->c_lflag = ICANON;
	ttyp->c_cc[VERASE] = ttyp->c_cc[VKILL] = '\0';
        if (tcsetattr(fd232, TCSANOW, ttyp) < 0) {
                syslog(LOG_ERR,
		    "omega_start: tcsetattr(%s): %m", omegadev);
                goto screwed;
        }
        if (tcflush(fd232, TCIOFLUSH) < 0) {
                syslog(LOG_ERR,
		    "omega_start: tcflush(%s): %m", omegadev);
                goto screwed;
        }
#if defined(OMEGACLK)
	if (ioctl(fd232, I_PUSH, "clk") < 0)
		syslog(LOG_ERR,
		    "omega_start: ioctl(%s, I_PUSH, clk): %m", omegadev);
	if (ioctl(fd232, CLK_SETSTR, "\n") < 0)
		syslog(LOG_ERR,
		    "omega_start: ioctl(%s, CLK_SETSTR): %m", omegadev);
#endif /* OMEGACLK */
#if defined(OMEGAPPS)
	if (ioctl(fd232, I_PUSH, "ppsclock") < 0)
		syslog(LOG_ERR,
		    "omega_start: ioctl(%s, I_PUSH, ppsclock): %m", omegadev);
	else
		fdpps = fd232;
#endif /* OMEGAPPS */
    }
#endif /* STREAM */
#if defined(HAVE_BSD_TTYS)
	/*
	 * 4.3bsd serial line parameters (sgttyb interface)
	 *
	 * The OMEGACLK option provides timestamping at the driver level. 
	 * It requires the tty_clk line discipline and 4.3bsd or later.
	 */
    {	struct sgttyb ttyb;
#if defined(OMEGACLK)
	int ldisc = CLKLDISC;
#endif /* OMEGACLK */

	if (ioctl(fd232, TIOCGETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "omega_start: ioctl(%s, TIOCGETP): %m", omegadev);
		goto screwed;
	}
	ttyb.sg_ispeed = ttyb.sg_ospeed = SPEED232;
#if defined(OMEGACLK)
	ttyb.sg_erase = ttyb.sg_kill = '\r';
	ttyb.sg_flags = RAW;
#else
	ttyb.sg_erase = ttyb.sg_kill = '\0';
	ttyb.sg_flags = EVENP|ODDP|CRMOD;
#endif /* OMEGACLK */
	if (ioctl(fd232, TIOCSETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "omega_start: ioctl(%s, TIOCSETP): %m", omegadev);
		goto screwed;
	}
#if defined(OMEGACLK)
	if (ioctl(fd232, TIOCSETD, &ldisc) < 0) {
		syslog(LOG_ERR,
		    "omega_start: ioctl(%s, TIOCSETD): %m",omegadev);
		goto screwed;
	}
#endif /* OMEGACLK */
    }
#endif /* HAVE_BSD_TTYS */

	/*
	 * Allocate unit structure
	 */
	if (omegaunits[unit] != 0) {
		omega = omegaunits[unit];	/* The one we want is okay */
	} else {
		for (i = 0; i < MAXUNITS; i++) {
			if (!unitinuse[i] && omegaunits[i] != 0)
				break;
		}
		if (i < MAXUNITS) {
			/*
			 * Reclaim this one
			 */
			omega = omegaunits[i];
			omegaunits[i] = 0;
		} else {
			omega = (struct omegaunit *)
			    emalloc(sizeof(struct omegaunit));
		}
	}
	bzero((char *)omega, sizeof(struct omegaunit));
	omegaunits[unit] = omega;

	/*
	 * Set up the structures
	 */
	omega->peer = peer;
	omega->unit = (u_char)unit;
	omega->timestarted = current_time;
	omega->station = STATION_NONE;

	omega->io.clock_recv = omega_receive;
	omega->io.srcclock = (caddr_t)omega;
	omega->io.datalen = 0;
	omega->io.fd = fd232;
	if (!io_addclock(&omega->io)) {
		goto screwed;
	}

	/*
	 * All done.  Initialize a few random peer variables, then
	 * return success.
	 */
	peer->precision = OMEGAPRECISION;
	peer->rootdelay = 0;
	peer->rootdispersion = 0;
	peer->stratum = stratumtouse[unit];
	if (stratumtouse[unit] <= 1)
		bcopy(OMEGAREFID, (char *)&peer->refid, 4);
	else
		peer->refid = htonl(OMEGAHSREFID);
	unitinuse[unit] = 1;
	return 1;

	/*
	 * Something broke; abandon ship
	 */
screwed:
	(void) close(fd232);
	return 0;
}


/*
 * omega_shutdown - shut down a OMEGA clock
 */
static void
omega_shutdown(unit)
	int unit;
{
	register struct omegaunit *omega;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "omega_shutdown: unit %d invalid",
		    unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "omega_shutdown: unit %d not in use", unit);
		return;
	}

	/*
	 * Tell the I/O module to turn us off.  We're history.
	 */
	omega = omegaunits[unit];
	io_closeclock(&omega->io);
	unitinuse[unit] = 0;
}


/*
 * omega_report_event - note the occurance of an event
 */
static void
omega_report_event(omega, code)
	struct omegaunit *omega;
	int code;
{
	struct peer *peer;

	peer = omega->peer;
	if (omega->status != (u_char)code) {
		omega->status = (u_char)code;
		if (code != CEVNT_NOMINAL)
			omega->lastevent = (u_char)code;
		syslog(LOG_INFO,
		    "omega clock %s event %x\n", ntoa(&peer->srcadr), code);
	}
}


/*
 * omega_receive - receive data from the serial interface on a
 * Kinemetrics OM-DC OMEGA clock.
 */
static void
omega_receive(rbufp)
	struct recvbuf *rbufp;
{
	register int i;
	register struct omegaunit *omega;
	register u_char *dpt;
	register char *cp, *cpend;
	register u_char *dpend;
	l_fp tstmp;
	u_fp dispersion, drift;

	/*
	 * Get the clock this applies to and a pointers to the data
	 */
	omega = (struct omegaunit *)rbufp->recv_srcclock;
	dpt = (u_char *)&rbufp->recv_space;

#ifndef PEDANTIC
	/*
	 * The OM-DC outputs a timecode every second, but we only want
	 * a set of NSTAMPS timecodes when polled (every 64 seconds).
	 * Setting PEDANTIC causes a sanity check on every timecode.
	 */
	if (!omega->polled)
		return;
#endif

	/*
	 * Edit timecode to remove control chars
	 */
	dpend = dpt + rbufp->recv_length;
	cp = omega->lastcode;
	cpend = omega->lastcode + BMAX - 1;
	while (dpt < dpend && cp < cpend) {
		if ((*cp = 0x7f & *dpt++) >= ' ') cp++;
#ifdef OMEGACLK
		else if (*cp == '\r') {
			if (dpend - dpt < 8) {
				/* short timestamp */
				return;
			}
			if (!buftvtots(dpt,&omega->lastrec)) {
				/* screwy timestamp */
				return;
			}
			dpt += 8;
		}
#endif
	}
	*cp = '\0';
	omega->lencode = cp - omega->lastcode;

	if (omega->lencode == 0)
		return;
	else if (omega->lencode != LENOMEGA) {
		omega->badformat++;
		/* Sometimes get a lot of these, filling the log with noise */
		/* omega_report_event(omega, CEVNT_BADREPLY); */
		return;
	}

#ifndef OMEGACLK
	omega->lastrec = rbufp->recv_time;
#endif

#ifdef DEBUG
	if (debug)
        	printf("omega: timecode %d %s\n",
		    omega->lencode, omega->lastcode);
#endif

	/*
	 * We get down to business, check the timecode format
	 * and decode its contents.
	 */
	cp = omega->lastcode;
	omega->leap = 0;
	/*
	 * Check timecode format.
	 */
	if (!isdigit(cp[0]) ||		/* day of year */
		!isdigit(cp[1]) ||
		!isdigit(cp[2]) ||
		cp[3] != ':' ||		/* <sp> */
		!isdigit(cp[4]) ||	/* hours */
		!isdigit(cp[5]) ||
		cp[6] != ':' ||		/* : separator */
		!isdigit(cp[7]) ||	/* minutes */
		!isdigit(cp[8]) ||
		cp[9] != ':' ||		/* : separator */
		!isdigit(cp[10]) ||	/* seconds */
		!isdigit(cp[11])) {
			omega->badformat++;
			omega_report_event(omega, CEVNT_BADREPLY);
			return;
	}

	/*
	 * Convert and check values.
	 */
	omega->year = 0;		/* fake */
	omega->day = cp[0] - '0';
	omega->day = MULBY10(omega->day) + cp[1] - '0';
	omega->day = MULBY10(omega->day) + cp[2] - '0';
	omega->hour = MULBY10(cp[4] - '0') + cp[5] - '0';
	omega->minute = MULBY10(cp[7] - '0') + cp[8] -  '0';
	omega->second = MULBY10(cp[10] - '0') + cp[11] - '0';
	omega->msec = 0;

	if (omega->day < 1 || omega->day > 366) {
		omega->baddata++;
		omega_report_event(omega, CEVNT_BADDATE);
		return;
	}
	if (omega->hour > 23 || omega->minute > 59 || omega->second > 59) {
		omega->baddata++;
		omega_report_event(omega, CEVNT_BADTIME);
		return;
	}

	/*
	 * Check quality/station-id flag.  The OM-DC should normally stay
	 * permanently locked to a station, and its time error should be less
	 * than 1 msec.  If it loses lock for any reason, it makes a worst
	 * case drift estimate based on the internally stored stability figure
	 * for its reference oscillator.  The stability figure can be adjusted
	 * by the user based on experience.  The default value is 1E05, which
	 * is pretty bad - 2E07 is about right for the unit I have.
	 *
	 * The following is arbitrary, change it if you're offended:
	 * For errors less than 50 msec, just clear the station indicator.
	 * For errors greater than 50 msec, flag loss of sync and report a
	 * propagation problem.  If the error is greater than 500 msec,
	 * something is dreadfully wrong - report a clock fault.
	 *
	 * In each case, we set a drift estimate which is used below as an
	 * estimate of measurement accuracy.
	 */
	omega->quality = cp[12];
	if (cp[12] == '>' || cp[12] == '?') {
		/* Error 500 to 5000 msec */
		omega_report_event(omega, CEVNT_FAULT);
		omega->leap = LEAP_NOTINSYNC;
		omega->station = STATION_NONE;
		drift = U_FP5000MS;
	} else if (cp[12] == '#') {
		/* Error 50 to 500 msec */
		omega_report_event(omega, CEVNT_PROP);
		omega->leap = LEAP_NOTINSYNC;
		omega->station = STATION_NONE;
		drift = U_FP500MS;
	} else if (cp[12] == '*') {
		/* Error 5 to 50 msec */
		omega->lasttime = current_time;
		omega->station = STATION_NONE;
		drift = U_FP50MS;
	} else if (cp[12] == '.') {
		/* Error 1 to 5 msec */
		omega->lasttime = current_time;
		omega->station = STATION_NONE;
		drift = U_FP5MS;
	} else if ('A' <= cp[12] && cp[12] <= 'H') {
		/* Error less than 1 msec */
		omega->lasttime = current_time;
		omega->station = cp[12] - 'A' + 1;
		drift = 0;
	} else {
		omega->badformat++;
		omega_report_event(omega, CEVNT_BADREPLY);
		return;
	}

#ifdef PEDANTIC
	/* If we haven't been polled, bail out. */
	if (!omega->polled)
		return;
#endif

	/*
	 * Now, compute the reference time value. Use the heavy
	 * machinery for the seconds and the millisecond field for the
	 * fraction when present.
         *
	 * this code does not yet know how to do the years
	 */
	tstmp = omega->lastrec;
	if (!clocktime(omega->day, omega->hour, omega->minute,
	    omega->second, GMT, tstmp.l_ui,
	    &omega->yearstart, &omega->lastref.l_ui)) {
		omega->baddata++;
		omega_report_event(omega, CEVNT_BADTIME);
		return;
	}
	MSUTOTSF(omega->msec, omega->lastref.l_uf);

	/*
	 * Adjust the read value by fudgefactor1 to correct RS232 delays.
	 */
	L_ADD(&omega->lastref, &fudgefactor1[omega->unit]);

	/* Carousel of NSTAMPS offsets. */
	i = omega->coderecv % NSTAMPS;
	omega->offset[i] = omega->lastref;
	L_SUB(&omega->offset[i], &tstmp);
	omega->coderecv++;

	/* If we don't yet have a full set, return. */
	if (omega->coderecv < NSTAMPS)
		return;

	/*
	 * Filter the samples, add the fudge factor and pass the
	 * offset and dispersion along. We use lastrec as both the
	 * reference time and receive time in order to avoid being cute,
	 * like setting the reference time later than the receive time,
	 * which may cause a paranoid protocol module to chuck out the
	 * data.  If the sample filter chokes because of excessive
	 * dispersion or whatever, get a new sample (omega->coderecv
	 * is still >= NSTAMPS) and try again.
 	 */
	if (!omega_process(omega, &tstmp, &dispersion)) {
		omega->baddata++;
		omega_report_event(omega, CEVNT_BADTIME);
		return;
	}

	/*
	 * Add accumulated clock drift to the dispersion to get
	 * a (hopefully) meaningful measurement accuracy estimate.
	 */
	dispersion += drift;
	refclock_receive(omega->peer, &tstmp, GMT, dispersion,
	    &omega->lastrec, &omega->lastrec, omega->leap);

	/*
	 * We have succeeded in answering the poll.  If the clock
	 * is locked, we're nominal.
	 */
	omega->polled = 0;
	omega->coderecv = 0;
	if (omega->leap != LEAP_NOTINSYNC)
		omega_report_event(omega, CEVNT_NOMINAL);
}


/*
 * omega_send - time to send the clock a signal to cough up a time sample
 */
static void
omega_send(omega,cmd)
	struct omegaunit *omega;
	char *cmd;
{
	if (!readonlyclockflag[omega->unit]) {
	/*
	 * Send a command to the clock.
	 */
	    if (write(omega->io.fd, cmd, 1) != 1) {
	    	syslog(LOG_ERR, "omega_send: unit %d: %m", omega->unit);
	    	omega_report_event(omega, CEVNT_FAULT);
	    }
	}
}


/*
 * Compare two l_fp's, used with qsort()
 */
static int
omega_cmpl_fp(p1, p2)
	register void *p1, *p2;
{

	if (!L_ISGEQ((l_fp *)p1, (l_fp *)p2))
		return (-1);
	if (L_ISEQU((l_fp *)p1, (l_fp *)p2))
		return (0);
	return (1);
}


/*
 * omega_process - process a pile of samples from the clock
 */
static char
omega_process(omega, offset, dispersion)
	struct omegaunit *omega;
	l_fp *offset;
	u_fp *dispersion;
{
	register int i, n;
	register U_LONG med_ui, med_uf, tmp_ui, tmp_uf;
	l_fp off[NSTAMPS];
	u_fp disp;

	/* Copy in offsets and sort into ascending order */
	for (i = 0; i < NSTAMPS; i++)
		off[i] = omega->offset[i];
	qsort((char *)off, NSTAMPS, sizeof(l_fp), omega_cmpl_fp);
	/*
	 * Reject the furthest from the median until NSKEEP samples remain
	 */
	i = 0;
	n = NSTAMPS;
	while ((n - i) > NSKEEP) {
		tmp_ui = off[n-1].l_ui;
		tmp_uf = off[n-1].l_uf;
		med_ui = off[(n+i)/2].l_ui;
		med_uf = off[(n+i)/2].l_uf;
		M_SUB(tmp_ui, tmp_uf, med_ui, med_uf);
		M_SUB(med_ui, med_uf, off[i].l_ui, off[i].l_uf);
		if (M_ISHIS(med_ui, med_uf, tmp_ui, tmp_uf)) {
			/* reject low end */
			i++;
		} else {
			/* reject high end */
			n--;
		}
	}

	/*
	 * Compute the dispersion based on the difference between the
	 * extremes of the remaining offsets.  If this is greater than
	 * the allowed sample set dispersion, bail out.  Otherwise,
	 * return the median offset and the dispersion.
	 */
	tmp_ui = off[n-1].l_ui;
	tmp_uf = off[n-1].l_uf;
	M_SUB(tmp_ui, tmp_uf, off[i].l_ui, off[i].l_uf);
	disp = MFPTOFP(tmp_ui, tmp_uf);
	if (disp > OMEGAMAXDISPERSE)
		return 0;
	*offset = off[(n+1)/2];
	*dispersion = disp;
	return 1;
}


/*
 * omega_poll - called by the transmit procedure
 */
static void
omega_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	struct omegaunit *omega;

	/*
	 * You don't need to poll this clock.  It puts out timecodes
	 * once per second.  If asked for a timestamp, take note.
	 * The next time a timecode comes in, it will be fed back.
	 */
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "omega_poll: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "omega_poll: unit %d not in use", unit);
		return;
	}
	omega = omegaunits[unit];
	if ((current_time - omega->lasttime) > 150) {
		omega->noreply++;
		omega_report_event(omegaunits[unit], CEVNT_TIMEOUT);
	}

	/*
	 * polled every 64 seconds.  Ask OMEGA_RECEIVE to hand in a timestamp.
	 */
	omega->polled = 1;
	omega->polls++;
	/*
	 * Ensure the clock is running in the correct mode - on-second
	 * timestamps.
	 */
	omega_send(omega,"C");
}


/*
 * omega_control - set fudge factors, return statistics
 */
static void
omega_control(unit, in, out)
	u_int unit;
	struct refclockstat *in;
	struct refclockstat *out;
{
	register struct omegaunit *omega;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "omega_control: unit %d invalid", unit);
		return;
	}

	if (in != 0) {
		if (in->haveflags & CLK_HAVETIME1)
			fudgefactor1[unit] = in->fudgetime1;
		if (in->haveflags & CLK_HAVETIME2)
			fudgefactor2[unit] = in->fudgetime2;
		if (in->haveflags & CLK_HAVEVAL1) {
			stratumtouse[unit] = (u_char)(in->fudgeval1 & 0xf);
			if (unitinuse[unit]) {
				struct peer *peer;

				/*
				 * Should actually reselect clock, but
				 * will wait for the next timecode
				 */
				omega = omegaunits[unit];
				peer = omega->peer;
				peer->stratum = stratumtouse[unit];
				if (stratumtouse[unit] <= 1)
					bcopy(OMEGAREFID, (char *)&peer->refid,
					    4);
				else
					peer->refid = htonl(OMEGAHSREFID);
			}
		}
		if (in->haveflags & CLK_HAVEFLAG1) {
			readonlyclockflag[unit] = in->flags & CLK_FLAG1;
		}
	}

	if (out != 0) {
		out->type = REFCLK_OMEGA_TRUETIME;
		out->haveflags
		    = CLK_HAVETIME1|CLK_HAVETIME2|
			CLK_HAVEVAL1|CLK_HAVEVAL2|
			CLK_HAVEFLAG1|CLK_HAVEFLAG2;
		out->clockdesc = OMEGADESCRIPTION;
		out->fudgetime1 = fudgefactor1[unit];
		out->fudgetime2 = fudgefactor2[unit];
		out->fudgeval1 = (LONG)stratumtouse[unit];
		out->fudgeval2 = 0;
		out->flags = readonlyclockflag[unit];
		if (unitinuse[unit]) {
			omega = omegaunits[unit];
			out->flags |= omega->station << 1;
			out->lencode = omega->lencode;
			out->lastcode = omega->lastcode;
			out->timereset = current_time - omega->timestarted;
			out->polls = omega->polls;
			out->noresponse = omega->noreply;
			out->badformat = omega->badformat;
			out->baddata = omega->baddata;
			out->lastevent = omega->lastevent;
			out->currentstatus = omega->status;
		} else {
			out->lencode = 0;
			out->lastcode = "";
			out->polls = out->noresponse = 0;
			out->badformat = out->baddata = 0;
			out->timereset = 0;
			out->currentstatus = out->lastevent = CEVNT_NOMINAL;
		}
	}
}


/*
 * omega_buginfo - return clock dependent debugging info
 */
static void
omega_buginfo(unit, bug)
	int unit;
	register struct refclockbug *bug;
{
	register struct omegaunit *omega;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "omega_buginfo: unit %d invalid", unit);
		return;
	}

	if (!unitinuse[unit])
		return;
	omega = omegaunits[unit];

	bug->nvalues = 11;
	bug->ntimes = 5;
	if (omega->lasttime != 0)
		bug->values[0] = current_time - omega->lasttime;
	else
		bug->values[0] = 0;
	bug->values[1] = (U_LONG)omega->reason;
	bug->values[2] = (U_LONG)omega->year;
	bug->values[3] = (U_LONG)omega->day;
	bug->values[4] = (U_LONG)omega->hour;
	bug->values[5] = (U_LONG)omega->minute;
	bug->values[6] = (U_LONG)omega->second;
	bug->values[7] = (U_LONG)omega->msec;
	bug->values[8] = omega->noreply;
	bug->values[9] = omega->yearstart;
	bug->values[10] = omega->quality;
	bug->stimes = 0x1c;
	bug->times[0] = omega->lastref;
	bug->times[1] = omega->lastrec;
	bug->times[2] = omega->offset[0];
	bug->times[3] = omega->offset[1];
	bug->times[4] = omega->offset[2];
}
#endif
