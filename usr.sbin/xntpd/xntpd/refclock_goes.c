/*
 * refclock_goes - clock driver for the Kinimetrics Truetime GOES receiver
 *    Version 2.0
 */

#if defined(REFCLOCK) && (defined(GOES) || defined(GOESCLK) || defined(GOESPPS))

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

#if defined(HAVE_TERMIOS)
#include <termios.h>
#endif
#if defined(STREAM)
#include <stropts.h>
#if defined(GOESCLK)
#include <clkdefs.h>
#endif /* GOESCLK */
#endif /* STREAM */

#if defined (GOESPPS)
#include <sys/ppsclock.h>
#endif /* GOESPPS */

#include "ntp_stdlib.h"

/*
 * Support for Kinemetrics Truetime 468-DC GOES Receiver
 *
 * Most of this code is copied from refclock_goes.c with thanks.
 *
 * the time code looks like follows;  Send the clock a R or C and once per
 * second a timestamp will appear that looks like this:
 * ADDD:HH:MM:SSQCL
 * A - control A
 * Q Quality indication: indicates possible error of
 *     ?     +/- 500 milliseconds            #     +/- 50 milliseconds
 *     *     +/- 5 milliseconds              .     +/- 1 millisecond
 *   space   less than 1 millisecond
 * C - Carriage return
 * L - Line feed
 * The carriage return start bit begins on 0 seconds and extends to 1 bit time.
 *
 * Unless you live on 125 degrees west longitude, you can't set your clock
 * propagation delay settings correctly and still use automatic mode.
 * The manual says to use a compromise when setting the switches.  This
 * results in significant errors.  The solution; use fudge time1 and time2
 * to incorporate corrections.  If your clock is set for 50 and it should
 * be 58 for using the west and 46 for using the east, use the line
 * fudge 127.127.5.0 time1 +0.008 time2 -0.004
 * This corrects the 4 milliseconds advance and 5 milliseconds retard needed.
 * The software will ask the clock which satellite it sees.
 *
 * Flag1 set to 1 will silence the clock side of xntpd, just reading the
 * clock without trying to write to it.  This is usefull if several
 * xntpds listen to the same clock.  This has not been tested yet...
 */

/*
 * Definitions
 */
#define	MAXUNITS	4	/* max number of GOES units */
#define	GOES232	"/dev/goes%d"
#define	SPEED232	B9600	/* 9600 baud */

/*
 * Radio interface parameters
 */
#define	GOESMAXDISPERSE	(FP_SECOND>>1) /* max error for synchronized clock (0.5 s as an u_fp) */
#define	GOESSKEWFACTOR	17	/* skew factor (for about 32 ppm) */
#define	GOESPRECISION	(-10)	/* precision assumed (about 1 ms) */
#define	GOESREFID	"GOES"	/* reference id */
#define	GOESDESCRIPTION	"Kinemetrics GOES Receiver" /* who we are */
#define	GOESHSREFID	0x7f7f050a /* 127.127.5.10 refid hi strata */
#define GMT		0	/* hour offset from Greenwich */
#define	NCODES		3	/* stages of median filter */
#define	LENGOES0	13	/* format 0 timecode length */
#define	LENGOES2	21	/* format 2 timecode length */
#define FMTGOESU	0	/* unknown format timecode id */
#define FMTGOES0	1	/* format 0 timecode id */
#define FMTGOES2	2	/* format 2 timecode id */
#define	DEFFUDGETIME	0	/* default fudge time (ms) */
#define BMAX		50	/* timecode buffer length */
#define	CODEDIFF	0x20000000	/* 0.125 seconds as an l_fp fraction */

/*
 * Tag which satellite we see
 */
#define GOES_SAT_NONE   0
#define GOES_SAT_WEST   1
#define GOES_SAT_EAST   2
#define GOES_SAT_STAND  3

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
 * GOES unit control structure
 */
struct goesunit {
	struct peer *peer;		/* associated peer structure */
	struct refclockio io;		/* given to the I/O handler */
	l_fp lastrec;			/* last receive time */
	l_fp lastref;			/* last timecode time */
	l_fp offset[NCODES];		/* recent sample offsets */
	char lastcode[BMAX];		/* last timecode received */
	u_short satellite;		/* which satellite we saw */
	u_short polled;			/* Hand in a time sample? */
	u_char format;			/* timecode format */
	u_char lencode;			/* length of last timecode */
	U_LONG lasttime;		/* last time clock heard from */
	u_char unit;			/* unit number for this guy */
	u_char status;			/* clock status */
	u_char lastevent;		/* last clock event */
	u_char reason;			/* reason for last abort */
	u_char year;			/* year of eternity */
	u_short day;			/* day of year */
	u_char hour;			/* hour of day */
	u_char minute;			/* minute of hour */
	u_char second;			/* seconds of minute */
	u_char leap;			/* leap indicators */
	u_short msec;			/* millisecond of second */
	u_char quality;			/* quality char from format 2 */
	U_LONG yearstart;		/* start of current year */
	/*
	 * Status tallies
 	 */
	U_LONG polls;			/* polls sent */
	U_LONG noreply;			/* no replies to polls */
	U_LONG coderecv;		/* timecodes received */
	U_LONG badformat;		/* bad format */
	U_LONG baddata;			/* bad data */
	U_LONG timestarted;		/* time we started this */
};

/*
 * Data space for the unit structures.  Note that we allocate these on
 * the fly, but never give them back.
 */
static struct goesunit *goesunits[MAXUNITS];
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
static	void	goes_init	P((void));
static	int	goes_start	P((u_int, struct peer *));
static	void	goes_shutdown	P((int));
static	void	goes_report_event	P((struct goesunit *, int));
static	void	goes_receive	P((struct recvbuf *));
static	char	goes_process	P((struct goesunit *, l_fp *, u_fp *));
static	void	goes_poll	P((int, struct peer *));
static	void	goes_control	P((u_int, struct refclockstat *, struct refclockstat *));
static	void	goes_buginfo	P((int, struct refclockbug *));
static	void	goes_send	P((struct goesunit *, char *));

struct	refclock refclock_goes = {
	goes_start, goes_shutdown, goes_poll,
	goes_control, goes_init, goes_buginfo, NOFLAGS
};

/*
 * goes_init - initialize internal goes driver data
 */
static void
goes_init()
{
	register int i;
	/*
	 * Just zero the data arrays
	 */
	memset((char *)goesunits, 0, sizeof goesunits);
	memset((char *)unitinuse, 0, sizeof unitinuse);

	/*
	 * Initialize fudge factors to default.
	 */
	for (i = 0; i < MAXUNITS; i++) {
		fudgefactor1[i].l_ui = 0;
		fudgefactor1[i].l_uf = DEFFUDGETIME;
		fudgefactor2[i].l_ui = 0;
		fudgefactor2[i].l_uf = DEFFUDGETIME;
		stratumtouse[i] = 0;
		readonlyclockflag[i] = 0;
	}
}


/*
 * goes_start - open the GOES devices and initialize data for processing
 */
static int
goes_start(unit, peer)
	u_int unit;
	struct peer *peer;
{
	register struct goesunit *goes;
	register int i;
	int fd232;
	char goesdev[20];

	/*
	 * Check configuration info
	 */
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "goes_start: unit %d invalid", unit);
		return 0;
	}
	if (unitinuse[unit]) {
		syslog(LOG_ERR, "goes_start: unit %d in use", unit);
		return 0;
	}

	/*
	 * Open serial port
	 */
	(void) sprintf(goesdev, GOES232, unit);
	fd232 = open(goesdev, O_RDWR, 0777);
	if (fd232 == -1) {
		syslog(LOG_ERR, "goes_start: open of %s: %m", goesdev);
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
		    "goes_start: ioctl(%s, TCGETA): %m", goesdev);
                goto screwed;
        }
        ttyb.c_iflag = IGNBRK|IGNPAR|ICRNL;
        ttyb.c_oflag = 0;
        ttyb.c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyb.c_lflag = ICANON;
	ttyb.c_cc[VERASE] = ttyb.c_cc[VKILL] = '\0';
        if (ioctl(fd232, TCSETA, &ttyb) < 0) {
                syslog(LOG_ERR,
		    "goes_start: ioctl(%s, TCSETA): %m", goesdev);
                goto screwed;
        }
    }
#endif /* HAVE_SYSV_TTYS */
#if defined(HAVE_TERMIOS)
	/*
	 * POSIX serial line parameters (termios interface)
	 *
	 * The GOESCLK option provides timestamping at the driver level. 
	 * It requires the tty_clk streams module.
	 *
	 * The GOESPPS option provides timestamping at the driver level.
	 * It uses a 1-pps signal and level converter (gadget box) and
	 * requires the ppsclock streams module and SunOS 4.1.1 or
	 * later.
	 */
    {	struct termios ttyb, *ttyp;
	ttyp = &ttyb;

	if (tcgetattr(fd232, ttyp) < 0) {
                syslog(LOG_ERR,
		    "goes_start: tcgetattr(%s): %m", goesdev);
                goto screwed;
        }
        ttyp->c_iflag = IGNBRK|IGNPAR|ICRNL;
        ttyp->c_oflag = 0;
        ttyp->c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyp->c_lflag = ICANON;
	ttyp->c_cc[VERASE] = ttyp->c_cc[VKILL] = '\0';
        if (tcsetattr(fd232, TCSANOW, ttyp) < 0) {
                syslog(LOG_ERR,
		    "goes_start: tcsetattr(%s): %m", goesdev);
                goto screwed;
        }
        if (tcflush(fd232, TCIOFLUSH) < 0) {
                syslog(LOG_ERR,
		    "goes_start: tcflush(%s): %m", goesdev);
                goto screwed;
        }
    }
#endif /* HAVE_TERMIOS */
#ifdef STREAM
#if defined(GOESCLK)
    if (ioctl(fd232, I_PUSH, "clk") < 0)
	    syslog(LOG_ERR,
		"goes_start: ioctl(%s, I_PUSH, clk): %m", goesdev);
    if (ioctl(fd232, CLK_SETSTR, "\n") < 0)
	    syslog(LOG_ERR,
		"goes_start: ioctl(%s, CLK_SETSTR): %m", goesdev);
#endif /* GOESCLK */
#if defined(GOESPPS)
    if (ioctl(fd232, I_PUSH, "ppsclock") < 0)
	    syslog(LOG_ERR,
		"goes_start: ioctl(%s, I_PUSH, ppsclock): %m", goesdev);
    else
	    fdpps = fd232;
#endif /* GOESPPS */
#endif /* STREAM */
#if defined(HAVE_BSD_TTYS)
	/*
	 * 4.3bsd serial line parameters (sgttyb interface)
	 *
	 * The GOESCLK option provides timestamping at the driver level. 
	 * It requires the tty_clk line discipline and 4.3bsd or later.
	 */
    {	struct sgttyb ttyb;
#if defined(GOESCLK)
	int ldisc = CLKLDISC;
#endif /* GOESCLK */

	if (ioctl(fd232, TIOCGETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "goes_start: ioctl(%s, TIOCGETP): %m", goesdev);
		goto screwed;
	}
	ttyb.sg_ispeed = ttyb.sg_ospeed = SPEED232;
#if defined(GOESCLK)
	ttyb.sg_erase = ttyb.sg_kill = '\r';
	ttyb.sg_flags = RAW;
#else
	ttyb.sg_erase = ttyb.sg_kill = '\0';
	ttyb.sg_flags = EVENP|ODDP|CRMOD;
#endif /* GOESCLK */
	if (ioctl(fd232, TIOCSETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "goes_start: ioctl(%s, TIOCSETP): %m", goesdev);
		goto screwed;
	}
#if defined(GOESCLK)
	if (ioctl(fd232, TIOCSETD, &ldisc) < 0) {
		syslog(LOG_ERR,
		    "goes_start: ioctl(%s, TIOCSETD): %m",goesdev);
		goto screwed;
	}
#endif /* GOESCLK */
    }
#endif /* HAVE_BSD_TTYS */

	/*
	 * Allocate unit structure
	 */
	if (goesunits[unit] != 0) {
		goes = goesunits[unit];	/* The one we want is okay */
	} else {
		for (i = 0; i < MAXUNITS; i++) {
			if (!unitinuse[i] && goesunits[i] != 0)
				break;
		}
		if (i < MAXUNITS) {
			/*
			 * Reclaim this one
			 */
			goes = goesunits[i];
			goesunits[i] = 0;
		} else {
			goes = (struct goesunit *)
			    emalloc(sizeof(struct goesunit));
		}
	}
	memset((char *)goes, 0, sizeof(struct goesunit));
	goesunits[unit] = goes;

	/*
	 * Set up the structures
	 */
	goes->peer = peer;
	goes->unit = (u_char)unit;
	goes->timestarted = current_time;
	goes->satellite = GOES_SAT_NONE;

	goes->io.clock_recv = goes_receive;
	goes->io.srcclock = (caddr_t)goes;
	goes->io.datalen = 0;
	goes->io.fd = fd232;
	if (!io_addclock(&goes->io)) {
		goto screwed;
	}

	/*
	 * All done.  Initialize a few random peer variables, then
	 * return success.
	 */
	peer->precision = GOESPRECISION;
	peer->rootdelay = 0;
	peer->rootdispersion = 0;
	peer->stratum = stratumtouse[unit];
	if (stratumtouse[unit] <= 1)
		memmove((char *)&peer->refid, GOESREFID, 4);
	else
		peer->refid = htonl(GOESHSREFID);
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
 * goes_shutdown - shut down a GOES clock
 */
static void
goes_shutdown(unit)
	int unit;
{
	register struct goesunit *goes;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "goes_shutdown: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "goes_shutdown: unit %d not in use", unit);
		return;
	}

	/*
	 * Tell the I/O module to turn us off.  We're history.
	 */
	goes = goesunits[unit];
	io_closeclock(&goes->io);
	unitinuse[unit] = 0;
}


/*
 * goes_report_event - note the occurance of an event
 */
static void
goes_report_event(goes, code)
	struct goesunit *goes;
	int code;
{
	struct peer *peer;

	peer = goes->peer;
	if (goes->status != (u_char)code) {
		goes->status = (u_char)code;
		if (code != CEVNT_NOMINAL)
			goes->lastevent = (u_char)code;
		syslog(LOG_INFO,
		    "clock %s event %x\n", ntoa(&peer->srcadr), code);
#ifdef DEBUG
		if (debug) {
			printf("goes_report_event(goes%d, code %d)\n",
				goes->unit, code);
		}
#endif
	}
}


/*
 * goes_receive - receive data from the serial interface on a Kinimetrics
 * clock
 */
static void
goes_receive(rbufp)
	struct recvbuf *rbufp;
{
	register int i;
	register struct goesunit *goes;
	register u_char *dpt;
	register char *cp;
	register u_char *dpend;
	l_fp tstmp;
	u_fp dispersion;

	/*
	 * Get the clock this applies to and a pointers to the data
	 */
	goes = (struct goesunit *)rbufp->recv_srcclock;
	dpt = (u_char *)&rbufp->recv_space;

	/*
	 * Edit timecode to remove control chars
	 */
	dpend = dpt + rbufp->recv_length;
	cp = goes->lastcode;
	while (dpt < dpend) {
		if ((*cp = 0x7f & *dpt++) >= ' ') cp++; 
#ifdef GOESCLK
		else if (*cp == '\r') {
			if (dpend - dpt < 8) {
				/* short timestamp */
				return;
			}
			if (!buftvtots(dpt,&goes->lastrec)) {
				/* screwy timestamp */
				return;
			}
			dpt += 8;
		}
#endif
	}
	*cp = '\0';
	goes->lencode = cp - goes->lastcode;
	if (goes->lencode == 0) return;
#ifndef GOESCLK
	goes->lastrec = rbufp->recv_time;
#endif /* GOESCLK */
	tstmp = goes->lastrec;

#ifdef DEBUG
	if (debug)
        	printf("goes: timecode %d %s\n",
		    goes->lencode, goes->lastcode);
#endif

	/*
	 * We get down to business, check the timecode format and decode
	 * its contents. This code checks for and decodes both format 0
	 * and format 2 and need not be told which in advance.
	 */
	cp = goes->lastcode;
	goes->leap = 0;
	goes->format = FMTGOESU;
	if (goes->lencode == LENGOES0) {

		/*
	 	 * Check timecode format 0
	 	 */
		if (!isdigit(cp[0]) ||	/* day of year */
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
				goes->badformat++;
				goes_report_event(goes, CEVNT_BADREPLY);
				return;
			}
		else goes->format = FMTGOES0;

		/*
		 * Convert format 0 and check values 
		 */
		goes->year = 0;		/* fake */
		goes->day = cp[0] - '0';
		goes->day = MULBY10(goes->day) + cp[1] - '0';
		goes->day = MULBY10(goes->day) + cp[2] - '0';
		goes->hour = MULBY10(cp[4] - '0') + cp[5] - '0';
		goes->minute = MULBY10(cp[7] - '0') + cp[8] -  '0';
		goes->second = MULBY10(cp[10] - '0') + cp[11] - '0';
		goes->msec = 0;

		if (cp[12] != ' ' && cp[12] != '.' && cp[12] != '*')
			goes->leap = LEAP_NOTINSYNC;
		else
			goes->lasttime = current_time;

		if (goes->day < 1 || goes->day > 366) {
			goes->baddata++;
			goes_report_event(goes, CEVNT_BADDATE);
			return;
		}
		if (goes->hour > 23 || goes->minute > 59
		    || goes->second > 59) {
			goes->baddata++;
			goes_report_event(goes, CEVNT_BADTIME);
			return;
		}

	} else if (goes->lencode == LENGOES2) {

		/*
		 * Extended precision satelite location info
		 */
		if (!isdigit(cp[0]) ||		/* longitude */
			!isdigit(cp[1]) ||
			!isdigit(cp[2]) ||
			cp[3] != '.' ||
			!isdigit(cp[4]) ||
			!isdigit(cp[5]) ||
			!isdigit(cp[6]) ||
			!isdigit(cp[7]) ||
			(cp[8] != '+' && cp[8] != '-') ||
			!isdigit(cp[9]) ||	/*latitude */
			cp[10] != '.' ||
			!isdigit(cp[11]) ||
			!isdigit(cp[12]) ||
			!isdigit(cp[13]) ||
			!isdigit(cp[14]) ||
			(cp[15] != '+' && cp[15] != '-') ||
			!isdigit(cp[16]) ||	/* height */
			!isdigit(cp[17]) ||
			!isdigit(cp[18]) ||
			cp[19] != '.' ||
			!isdigit(cp[20])) {
				goes->badformat++;
				goes_report_event(goes, CEVNT_BADREPLY);
				return;
			}
		else goes->format = FMTGOES2;

		/*
		 * Figure out which satellite this is.
		 * This allows +-5 degrees from nominal.
		 */
		if (cp[0] == '1' && (cp[1] == '3' || cp[1] == '2'))
			goes->satellite = GOES_SAT_WEST;
		else if (cp[0] == '1' && cp[1] == '0')
			goes->satellite = GOES_SAT_STAND;
		else if (cp[0] == '0' && cp[1] == '7')
			goes->satellite = GOES_SAT_EAST;
		else
			goes->satellite = GOES_SAT_NONE;

#ifdef DEBUG
		if (debug)
			printf("goes_receive: select satellite %d\n",
				goes->satellite);
#endif

		/*
		 * Switch back to on-second time codes.
		 */
		goes_send(goes,"C");

		/*
		 * Since this is not a time code, just return...
		 */
		return;
	} else {
		goes_report_event(goes, CEVNT_BADREPLY);
		return;
	}

	/*
	 * The clock will blurt a timecode every second but we only
	 * want one when polled.  If we havn't been polled, bail out.
	 */
	if (!goes->polled)
		return;

	/*
	 * Now, compute the reference time value. Use the heavy
	 * machinery for the seconds and the millisecond field for the
	 * fraction when present.
         *
	 * this code does not yet know how to do the years
	 */
	tstmp = goes->lastrec;
	if (!clocktime(goes->day, goes->hour, goes->minute,
	    goes->second, GMT, tstmp.l_ui,
	    &goes->yearstart, &goes->lastref.l_ui)) {
		goes->baddata++;
		goes_report_event(goes, CEVNT_BADTIME);
		return;
	}
	MSUTOTSF(goes->msec, goes->lastref.l_uf);


	/*
	 * Slop the read value by fudgefactor1 or fudgefactor2 depending
	 * on which satellite we are viewing last time we checked.
	 */

#ifdef DEBUG
	if (debug)
		printf("GOES_RECEIVE: Slopping for satellite %d\n",
			goes->satellite);
#endif
	if (goes->satellite == GOES_SAT_WEST)
		L_ADD(&goes->lastref, &fudgefactor1[goes->unit]);
	else if (goes->satellite == GOES_SAT_EAST)
		L_ADD(&goes->lastref, &fudgefactor2[goes->unit]);
/*	else if (goes->satellite == GOES_SAT_STAND)
		L_ADD(&goes->lastref, &((fudgefactor1[goes->unit] +
			fudgefactor2[goes->unit]) / 2)); */

	i = ((int)(goes->coderecv)) % NCODES;
	goes->offset[i] = goes->lastref;
	L_SUB(&goes->offset[i], &tstmp);
	if (goes->coderecv == 0)
		for (i = 1; i < NCODES; i++)
			goes->offset[i] = goes->offset[0];

	goes->coderecv++;

	/*
	 * Check the satellite position
	 */
	goes_send(goes,"E");

	/*
	 * Process the median filter, add the fudge factor and pass the
	 * offset and dispersion along. We use lastrec as both the
	 * reference time and receive time in order to avoid being cute,
	 * like setting the reference time later than the receive time,
	 * which may cause a paranoid protocol module to chuck out the
	 * data.
 	 */
	if (!goes_process(goes, &tstmp, &dispersion)) {
		goes->baddata++;
		goes_report_event(goes, CEVNT_BADTIME);
		return;
	}
	refclock_receive(goes->peer, &tstmp, GMT, dispersion,
	    &goes->lastrec, &goes->lastrec, goes->leap);

	/*
	 * We have succedded in answering the poll.  Turn off the flag
	 */
	goes->polled = 0;
}


/*
 * goes_send - time to send the clock a signal to cough up a time sample
 */
static void
goes_send(goes,cmd)
	struct goesunit *goes;
	char *cmd;
{
	if (!readonlyclockflag[goes->unit]) {
	/*
	 * Send a command to the clock.  C for on-second timecodes.
	 * E for extended resolution satelite postion information.
	 */
	    if (write(goes->io.fd, cmd, 1) != 1) {
	    	syslog(LOG_ERR, "goes_send: unit %d: %m", goes->unit);
	    	goes_report_event(goes, CEVNT_FAULT);
	    } else {
	    	goes->polls++;
	    }
	}
}

/*
 * goes_process - process a pile of samples from the clock
 */
static char
goes_process(goes, offset, dispersion)
	struct goesunit *goes;
	l_fp *offset;
	u_fp *dispersion;
{
	register int i, j;
	register U_LONG tmp_ui, tmp_uf;
	int not_median1 = -1;	/* XXX correct? */
	int not_median2 = -1;	/* XXX correct? */
	int median;
	u_fp disp_tmp, disp_tmp2;

	/*
	 * This code implements a three-stage median filter. First, we
         * check if the samples are within 125 ms of each other. If not,
	 * dump the sample set. We take the median of the three offsets
	 * and use that as the sample offset. We take the maximum
	 * difference and use that as the sample dispersion. There
	 * probably is not much to be gained by a longer filter, since
	 * the clock filter in ntp_proto should do its thing.
	 */
	disp_tmp2 = 0;
	for (i = 0; i < NCODES-1; i++) {
		for (j = i+1; j < NCODES; j++) {
			tmp_ui = goes->offset[i].l_ui;
			tmp_uf = goes->offset[i].l_uf;
			M_SUB(tmp_ui, tmp_uf, goes->offset[j].l_ui,
				goes->offset[j].l_uf);
			if (M_ISNEG(tmp_ui, tmp_uf)) {
				M_NEG(tmp_ui, tmp_uf);
			}
			if (tmp_ui != 0 || tmp_uf > CODEDIFF) {
				return 0;
			}
			disp_tmp = MFPTOFP(0, tmp_uf);
			if (disp_tmp > disp_tmp2) {
				disp_tmp2 = disp_tmp;
				not_median1 = i;
				not_median2 = j;
			}
		}
	}

	/*
	 * It seems as if all are within 125 ms of each other.
	 * Now to determine the median of the three. Whlie the
	 * 125 ms check was going on, we also subtly catch the
	 * dispersion and set-up for a very easy median calculation.
	 * The largest difference between any two samples constitutes
	 * the dispersion. The sample not involve in the dispersion is
	 * the median sample. EASY!
	 */
	if (goes->lasttime == 0 || disp_tmp2 > GOESMAXDISPERSE)
	    disp_tmp2 = GOESMAXDISPERSE;
	if (not_median1 == 0) {
		if (not_median2 == 1)
		    median = 2;
		else
		    median = 1;
        } else {
		median = 0;
        }
	*offset = goes->offset[median];
	*dispersion = disp_tmp2;
	return 1;
}

/*
 * goes_poll - called by the transmit procedure
 */
static void
goes_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	struct goesunit *goes;

	/*
	 * You don't need to poll this clock.  It puts out timecodes
	 * once per second.  If asked for a timestamp, take note.
	 * The next time a timecode comes in, it will be fed back.
	 */
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "goes_poll: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "goes_poll: unit %d not in use", unit);
		return;
	}
	goes = goesunits[unit];
	if ((current_time - goes->lasttime) > 150) {
		goes->noreply++;
		goes_report_event(goesunits[unit], CEVNT_TIMEOUT);
	}

	/*
	 * polled every 64 seconds.  Ask GOES_RECEIVE to hand in a timestamp.
	 */
	goes->polled = 1;
	goes->polls++;

	goes_send(goes,"C");
}

/*
 * goes_control - set fudge factors, return statistics
 */
static void
goes_control(unit, in, out)
	u_int unit;
	struct refclockstat *in;
	struct refclockstat *out;
{
	register struct goesunit *goes;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "goes_control: unit %d invalid", unit);
		return;
	}
	goes = goesunits[unit];

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
				peer = goes->peer;
				peer->stratum = stratumtouse[unit];
				if (stratumtouse[unit] <= 1)
					memmove((char *)&peer->refid,
						GOESREFID, 4);
				else
					peer->refid = htonl(GOESHSREFID);
			}
		}
		if (in->haveflags & CLK_HAVEFLAG1) {
			readonlyclockflag[unit] = in->flags & CLK_FLAG1;
		}
	}

	if (out != 0) {
		out->type = REFCLK_GOES_TRUETIME;
		out->haveflags
		    = CLK_HAVETIME1|CLK_HAVETIME2|
			CLK_HAVEVAL1|CLK_HAVEVAL2|
			CLK_HAVEFLAG1|CLK_HAVEFLAG2;
		out->clockdesc = GOESDESCRIPTION;
		out->fudgetime1 = fudgefactor1[unit];
		out->fudgetime2 = fudgefactor2[unit];
		out->fudgeval1 = (LONG)stratumtouse[unit];
		out->fudgeval2 = 0;
		out->flags = readonlyclockflag[unit] |
			(goes->satellite << 1);
		if (unitinuse[unit]) {
			out->lencode = goes->lencode;
			out->lastcode = goes->lastcode;
			out->timereset = current_time - goes->timestarted;
			out->polls = goes->polls;
			out->noresponse = goes->noreply;
			out->badformat = goes->badformat;
			out->baddata = goes->baddata;
			out->lastevent = goes->lastevent;
			out->currentstatus = goes->status;
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
 * goes_buginfo - return clock dependent debugging info
 */
static void
goes_buginfo(unit, bug)
	int unit;
	register struct refclockbug *bug;
{
	register struct goesunit *goes;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "goes_buginfo: unit %d invalid", unit);
		return;
	}

	if (!unitinuse[unit])
		return;
	goes = goesunits[unit];

	bug->nvalues = 11;
	bug->ntimes = 5;
	if (goes->lasttime != 0)
		bug->values[0] = current_time - goes->lasttime;
	else
		bug->values[0] = 0;
	bug->values[1] = (U_LONG)goes->reason;
	bug->values[2] = (U_LONG)goes->year;
	bug->values[3] = (U_LONG)goes->day;
	bug->values[4] = (U_LONG)goes->hour;
	bug->values[5] = (U_LONG)goes->minute;
	bug->values[6] = (U_LONG)goes->second;
	bug->values[7] = (U_LONG)goes->msec;
	bug->values[8] = goes->noreply;
	bug->values[9] = goes->yearstart;
	bug->values[10] = goes->quality;
	bug->stimes = 0x1c;
	bug->times[0] = goes->lastref;
	bug->times[1] = goes->lastrec;
	bug->times[2] = goes->offset[0];
	bug->times[3] = goes->offset[1];
	bug->times[4] = goes->offset[2];
}
#endif
