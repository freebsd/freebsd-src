/*
 * refclock_gpstm - clock driver for the Kinimetrics Truetime GPSTM/TMD rcvr
 *    Version 1.0 (from Version 2.0 of the GOES driver, as of 03Jan94)
 */

#if defined(REFCLOCK) && (defined(GPSTM) || defined(GPSTMCLK) \
			  || defined(GPSTMPPS))

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"

#ifdef SYS_BSDI
#undef HAVE_BSD_TTYS
#include <sys/ioctl.h>
#endif

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
#if defined(GPSTMCLK)
#include <clkdefs.h>
#endif /* GPSTMCLK */
#endif /* STREAM */

#if defined(GPSTMPPS)
#include <sys/ppsclock.h>
#endif /* GPSTMPPS */

#include "ntp_stdlib.h"

/*
 * Support for Kinemetrics Truetime GPS-TM/TMD Receiver
 *
 * Most of this code is copied from refclock_goes.c with thanks.
 *
 * the time code looks like follows:
 *
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
 * Flag1 set to 1 will silence the clock side of xntpd, just reading the
 * clock without trying to write to it.  This is usefull if several
 * xntpds listen to the same clock.  This has not been tested yet...
 */

/*
 * Definitions
 */
#define	MAXUNITS	4	/* max number of GPSTM units */
#define	GPSTM232	"/dev/gpstm%d"
#define	SPEED232	B9600	/* 9600 baud */

/*
 * Radio interface parameters
 */
#define	MAXDISPERSE	(FP_SECOND>>1) /* max error for synchronized clock (0.5 s as an u_fp) */
#define	PRECISION	(-20)	/* precision assumed (about 1 ms) */
#define	REFID		"GPS\0"	/* reference id */
#define	DESCRIPTION	"Kinemetrics GPS-TM/TMD Receiver" /* who we are */
#define	HSREFID		0x7f7f0f0a /* 127.127.15.10 refid hi strata */
#define GMT		0	/* hour offset from Greenwich */
#define	NCODES		3	/* stages of median filter */
#define BMAX		99	/* timecode buffer length */
#define	CODEDIFF	0x20000000	/* 0.125 seconds as an l_fp fraction */
#define	TIMEOUT		180	/* ping the clock if it's silent this long */

/*
 * used by the state machine
 */
enum gpstm_event {e_Init, e_F18, e_F50, e_F51, e_TS};
static enum {Base, Start, F18, F50, F51, F08} State[MAXUNITS];
static time_t Last[MAXUNITS];
static void gpstm_doevent P((int, enum gpstm_event));
static void gpstm_initstate P((int));

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
 * GPSTM unit control structure
 */
struct gpstm_unit {
	struct peer *peer;		/* associated peer structure */
	struct refclockio io;		/* given to the I/O handler */
	l_fp lastrec;			/* last receive time */
	l_fp lastref;			/* last timecode time */
	l_fp offset[NCODES];		/* recent sample offsets */
	char lastcode[BMAX];		/* last timecode received */
	u_short polled;			/* Hand in a time sample? */
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
	u_char quality;			/* quality character */
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
static struct gpstm_unit *gpstm_units[MAXUNITS];
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
static	void	gpstm_init	P((void));
static	int	gpstm_start	P((u_int, struct peer *));
static	void	gpstm_shutdown	P((int));
static	void	gpstm_rep_event	P((struct gpstm_unit *, int));
static	void	gpstm_receive	P((struct recvbuf *));
static	char	gpstm_process	P((struct gpstm_unit *, l_fp *, u_fp *));
static	void	gpstm_poll	P((int, struct peer *));
static	void	gpstm_control	P((u_int, struct refclockstat *,
				   struct refclockstat *));
static	void	gpstm_buginfo	P((int, struct refclockbug *));
static	void	gpstm_send	P((struct gpstm_unit *, char *));

struct	refclock refclock_gpstm = {
	gpstm_start, gpstm_shutdown, gpstm_poll,
	gpstm_control, gpstm_init, gpstm_buginfo, NOFLAGS
};

/*
 * gpstm_init - initialize internal driver data
 */
static void
gpstm_init()
{
	register int i;
	/*
	 * Just zero the data arrays
	 */
	memset((char *)gpstm_units, 0, sizeof gpstm_units);
	memset((char *)unitinuse, 0, sizeof unitinuse);

	/*
	 * Initialize fudge factors to default.
	 */
	for (i = 0; i < MAXUNITS; i++) {
		fudgefactor1[i].l_ui = 0;
		fudgefactor1[i].l_uf = 0;
		fudgefactor2[i].l_ui = 0;
		fudgefactor2[i].l_uf = 0;
		stratumtouse[i] = 0;
		readonlyclockflag[i] = 0;
	}
}


/*
 * gpstm_start - open the device and initialize data for processing
 */
static int
gpstm_start(unit, peer)
	u_int unit;
	struct peer *peer;
{
	register struct gpstm_unit *gpstm;
	register int i;
	int fd232;
	char dev[20];

	/*
	 * Check configuration info
	 */
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "gpstm_start: unit %d invalid", unit);
		return 0;
	}
	if (unitinuse[unit]) {
		syslog(LOG_ERR, "gpstm_start: unit %d in use", unit);
		return 0;
	}

	/*
	 * Open serial port
	 */
	(void) sprintf(dev, GPSTM232, unit);
	fd232 = open(dev, O_RDWR, 0777);
	if (fd232 == -1) {
		syslog(LOG_ERR, "gpstm_start: open of %s: %m", dev);
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
		    "gpstm_start: ioctl(%s, TCGETA): %m", dev);
                goto screwed;
        }
        ttyb.c_iflag = IGNBRK|IGNPAR|ICRNL;
        ttyb.c_oflag = 0;
        ttyb.c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyb.c_lflag = ICANON;
	ttyb.c_cc[VERASE] = ttyb.c_cc[VKILL] = '\0';
        if (ioctl(fd232, TCSETA, &ttyb) < 0) {
                syslog(LOG_ERR,
		    "gpstm_start: ioctl(%s, TCSETA): %m", dev);
                goto screwed;
        }
    }
#endif /* HAVE_SYSV_TTYS */
#if defined(HAVE_TERMIOS)
	/*
	 * POSIX serial line parameters (termios interface)
	 *
	 * The GPSTMCLK option provides timestamping at the driver level. 
	 * It requires the tty_clk streams module.
	 *
	 * The GPSTMPPS option provides timestamping at the driver level.
	 * It uses a 1-pps signal and level converter (gadget box) and
	 * requires the ppsclock streams module and SunOS 4.1.1 or
	 * later.
	 */
    {	struct termios ttyb, *ttyp;
	ttyp = &ttyb;

	if (tcgetattr(fd232, ttyp) < 0) {
                syslog(LOG_ERR,
		    "gpstm_start: tcgetattr(%s): %m", dev);
                goto screwed;
        }
        ttyp->c_iflag = IGNBRK|IGNPAR|ICRNL;
        ttyp->c_oflag = 0;
        ttyp->c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyp->c_lflag = ICANON;
	ttyp->c_cc[VERASE] = ttyp->c_cc[VKILL] = '\0';
        if (tcsetattr(fd232, TCSANOW, ttyp) < 0) {
                syslog(LOG_ERR,
		    "gpstm_start: tcsetattr(%s): %m", dev);
                goto screwed;
        }
        if (tcflush(fd232, TCIOFLUSH) < 0) {
                syslog(LOG_ERR,
		    "gpstm_start: tcflush(%s): %m", dev);
                goto screwed;
        }
#if defined(STREAM)
#if defined(GPSTMCLK)
	if (ioctl(fd232, I_PUSH, "clk") < 0)
		syslog(LOG_ERR,
		    "gpstm_start: ioctl(%s, I_PUSH, clk): %m", dev);
	if (ioctl(fd232, CLK_SETSTR, "\n") < 0)
		syslog(LOG_ERR,
		    "gpstm_start: ioctl(%s, CLK_SETSTR): %m", dev);
#endif /* GPSTMCLK */
#if defined(GPSTMPPS)
	if (ioctl(fd232, I_PUSH, "ppsclock") < 0)
		syslog(LOG_ERR,
		    "gpstm_start: ioctl(%s, I_PUSH, ppsclock): %m", dev);
	else
		fdpps = fd232;
#endif /* GPSTMPPS */
#endif /* STREAM */
    }
#endif /* HAVE_TERMIOS */
#if defined(HAVE_BSD_TTYS)
	/*
	 * 4.3bsd serial line parameters (sgttyb interface)
	 *
	 * The GPSTMCLK option provides timestamping at the driver level. 
	 * It requires the tty_clk line discipline and 4.3bsd or later.
	 */
    {	struct sgttyb ttyb;
#if defined(GPSTMCLK)
	int ldisc = CLKLDISC;
#endif /* GPSTMCLK */

	if (ioctl(fd232, TIOCGETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "gpstm_start: ioctl(%s, TIOCGETP): %m", dev);
		goto screwed;
	}
	ttyb.sg_ispeed = ttyb.sg_ospeed = SPEED232;
#if defined(GPSTMCLK)
	ttyb.sg_erase = ttyb.sg_kill = '\r';
	ttyb.sg_flags = RAW;
#else
	ttyb.sg_erase = ttyb.sg_kill = '\0';
	ttyb.sg_flags = EVENP|ODDP|CRMOD;
#endif /* GPSTMCLK */
	if (ioctl(fd232, TIOCSETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "gpstm_start: ioctl(%s, TIOCSETP): %m", dev);
		goto screwed;
	}
#if defined(GPSTMCLK)
	if (ioctl(fd232, TIOCSETD, &ldisc) < 0) {
		syslog(LOG_ERR,
		    "gpstm_start: ioctl(%s, TIOCSETD): %m", dev);
		goto screwed;
	}
#endif /* GPSTMCLK */
    }
#endif /* HAVE_BSD_TTYS */

	/*
	 * Allocate unit structure
	 */
	if (gpstm_units[unit] != 0) {
		gpstm = gpstm_units[unit];	/* The one we want is okay */
	} else {
		for (i = 0; i < MAXUNITS; i++) {
			if (!unitinuse[i] && gpstm_units[i] != 0)
				break;
		}
		if (i < MAXUNITS) {
			/*
			 * Reclaim this one
			 */
			gpstm = gpstm_units[i];
			gpstm_units[i] = 0;
		} else {
			gpstm = (struct gpstm_unit *)
			    emalloc(sizeof(struct gpstm_unit));
		}
	}
	memset((char *)gpstm, 0, sizeof(struct gpstm_unit));
	gpstm_units[unit] = gpstm;

	/*
	 * Set up the structures
	 */
	gpstm->peer = peer;
	gpstm->unit = (u_char)unit;
	gpstm->timestarted = current_time;

	gpstm->io.clock_recv = gpstm_receive;
	gpstm->io.srcclock = (caddr_t)gpstm;
	gpstm->io.datalen = 0;
	gpstm->io.fd = fd232;
	if (!io_addclock(&gpstm->io)) {
		goto screwed;
	}

	/*
	 * All done.  Initialize a few random peer variables, then
	 * return success.
	 */
	peer->precision = PRECISION;
	peer->rootdelay = 0;
	peer->rootdispersion = 0;
	peer->stratum = stratumtouse[unit];
	if (stratumtouse[unit] <= 1)
		memmove((char *)&peer->refid, REFID, 4);
	else
		peer->refid = htonl(HSREFID);
	unitinuse[unit] = 1;
	gpstm_initstate(unit);
	return 1;

	/*
	 * Something broke; abandon ship
	 */
screwed:
	(void) close(fd232);
	return 0;
}

/*
 * gpstm_shutdown - shut down a clock
 */
static void
gpstm_shutdown(unit)
	int unit;
{
	register struct gpstm_unit *gpstm;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "gpstm_shutdown: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "gpstm_shutdown: unit %d not in use", unit);
		return;
	}

	/*
	 * Tell the I/O module to turn us off.  We're history.
	 */
	gpstm = gpstm_units[unit];
	io_closeclock(&gpstm->io);
	unitinuse[unit] = 0;
}


/*
 * gpstm_rep_event - note the occurance of an event
 */
static void
gpstm_rep_event(gpstm, code)
	struct gpstm_unit *gpstm;
	int code;
{
	struct peer *peer;

	peer = gpstm->peer;
	if (gpstm->status != (u_char)code) {
		gpstm->status = (u_char)code;
		if (code != CEVNT_NOMINAL)
			gpstm->lastevent = (u_char)code;
		syslog(LOG_INFO,
		    "clock %s event %x\n", ntoa(&peer->srcadr), code);
#ifdef DEBUG
		if (debug) {
			printf("gpstm_rep_event(gpstm%d, code %d)\n",
			       gpstm->unit, code);
		}
#endif
	}
	if (code == CEVNT_BADREPLY)
		gpstm_initstate(gpstm->unit);
}


/*
 * gpstm_receive - receive data from the serial interface on a clock
 */
static void
gpstm_receive(rbufp)
	struct recvbuf *rbufp;
{
	register int i;
	register struct gpstm_unit *gpstm;
	register u_char *dpt;
	register char *cp;
	register u_char *dpend;
	l_fp tstmp;
	u_fp dispersion;

	/*
	 * Get the clock this applies to and a pointers to the data
	 */
	gpstm = (struct gpstm_unit *)rbufp->recv_srcclock;
	dpt = (u_char *)&rbufp->recv_space;

	/*
	 * Edit timecode to remove control chars
	 */
	dpend = dpt + rbufp->recv_length;
	cp = gpstm->lastcode;
	while (dpt < dpend) {
		if ((*cp = 0x7f & *dpt++) >= ' ') cp++; 
#ifdef GPSTMCLK
		else if (*cp == '\r') {
			if (dpend - dpt < 8) {
				/* short timestamp */
				return;
			}
			if (!buftvtots(dpt,&gpstm->lastrec)) {
				/* screwy timestamp */
				return;
			}
			dpt += 8;
		}
#endif
	}
	*cp = '\0';
	gpstm->lencode = cp - gpstm->lastcode;
	if (gpstm->lencode == 0)
		return;
#ifndef GPSTMCLK
	gpstm->lastrec = rbufp->recv_time;
#endif /* GPSTMCLK */
#if !defined(GPSTMCLK) && !defined(GPSTMPPS) && defined(TIOCMODT)
	do {
		auto	struct timeval	cur, now;
		register long		usec;

		if (ioctl(gpstm->io.fd, TIOCMODT, &cur) < 0) {
			syslog(LOG_ERR, "TIOCMODT: %m");
#ifdef DEBUG
			if (debug) perror("TIOCMODT");
			break;
#endif
		}
		if (cur.tv_sec == 0) {
			/* no timestamps yet */
			if (debug) printf("MODT tv_sec == 0\n");
			break;
		}

		gettimeofday(&now, NULL);
		usec = 1000000 * (now.tv_sec - cur.tv_sec)
			+ (now.tv_usec - cur.tv_usec);
#ifdef DEBUG
		if (debug) printf("lastmodem: delay=%d us\n", usec);
#endif
		if (usec < 0 || usec > 10000) {
			/* time warp or stale timestamp */
			break;
		}
		if (!buftvtots((char *)&cur, &gpstm->lastrec)) {
			/* screwy timestamp */
			break;
		}
	} while (0);
#endif /*TIOCMODT*/

#ifdef DEBUG
	if (debug)
        	printf("gpstm: timecode %d %s\n",
		       gpstm->lencode, gpstm->lastcode);
#endif

	cp = gpstm->lastcode;
	gpstm->leap = 0;
	if ((cp[0] == 'F' && isdigit(cp[1]) && isdigit(cp[2]))
	    || (cp[0] == ' ' && cp[1] == 'T' && cp[2] == 'R')) {
		enum gpstm_event event;

		syslog(LOG_NOTICE, "gpstm%d: \"%s\"", gpstm->unit, cp);
		if (cp[1] == '5' && cp[2] == '0')
			event = e_F50;
		else if (cp[1] == '5' && cp[2] == '1')
			event = e_F51;
		else if (!strncmp(" TRUETIME Mk III", cp, 16))
			event = e_F18;
		else {
			gpstm_rep_event(gpstm, CEVNT_BADREPLY);
			return;
		}
		gpstm_doevent(gpstm->unit, event);
		return;
	} else if (gpstm->lencode == 13) {
		/*
	 	 * Check timecode format 0
	 	 */
		if (!isdigit(cp[0])	/* day of year */
		    || !isdigit(cp[1])
		    || !isdigit(cp[2])
		    || cp[3] != ':'	/* : separator */
		    || !isdigit(cp[4])	/* hours */
		    || !isdigit(cp[5])
		    || cp[6] != ':'	/* : separator */
		    || !isdigit(cp[7])	/* minutes */
		    || !isdigit(cp[8])
		    || cp[9] != ':'	/* : separator */
		    || !isdigit(cp[10])	/* seconds */
		    || !isdigit(cp[11]))
		{
			gpstm->badformat++;
			gpstm_rep_event(gpstm, CEVNT_BADREPLY);
			return;
		}

		/*
		 * Convert format 0 and check values 
		 */
		gpstm->year = 0;		/* fake */
		gpstm->day = cp[0] - '0';
		gpstm->day = MULBY10(gpstm->day) + cp[1] - '0';
		gpstm->day = MULBY10(gpstm->day) + cp[2] - '0';
		gpstm->hour = MULBY10(cp[4] - '0') + cp[5] - '0';
		gpstm->minute = MULBY10(cp[7] - '0') + cp[8] -  '0';
		gpstm->second = MULBY10(cp[10] - '0') + cp[11] - '0';
		gpstm->msec = 0;

		if (cp[12] != ' ' && cp[12] != '.' && cp[12] != '*')
			gpstm->leap = LEAP_NOTINSYNC;
		else
			gpstm->lasttime = current_time;

		if (gpstm->day < 1 || gpstm->day > 366) {
			gpstm->baddata++;
			gpstm_rep_event(gpstm, CEVNT_BADDATE);
			return;
		}
		if (gpstm->hour > 23 || gpstm->minute > 59
		    || gpstm->second > 59) {
			gpstm->baddata++;
			gpstm_rep_event(gpstm, CEVNT_BADTIME);
			return;
		}
		gpstm_doevent(gpstm->unit, e_TS);
	} else {
		gpstm_rep_event(gpstm, CEVNT_BADREPLY);
		return;
	}

	/*
	 * The clock will blurt a timecode every second but we only
	 * want one when polled.  If we havn't been polled, bail out.
	 */
	if (!gpstm->polled)
		return;

	/*
	 * Now, compute the reference time value. Use the heavy
	 * machinery for the seconds and the millisecond field for the
	 * fraction when present.
         *
	 * this code does not yet know how to do the years
	 */
	tstmp = gpstm->lastrec;
	if (!clocktime(gpstm->day, gpstm->hour, gpstm->minute,
		       gpstm->second, GMT, tstmp.l_ui,
		       &gpstm->yearstart, &gpstm->lastref.l_ui))
	{
		gpstm->baddata++;
		gpstm_rep_event(gpstm, CEVNT_BADTIME);
		return;
	}
	MSUTOTSF(gpstm->msec, gpstm->lastref.l_uf);

	i = ((int)(gpstm->coderecv)) % NCODES;
	gpstm->offset[i] = gpstm->lastref;
	L_SUB(&gpstm->offset[i], &tstmp);
	if (gpstm->coderecv == 0)
		for (i = 1; i < NCODES; i++)
			gpstm->offset[i] = gpstm->offset[0];

	gpstm->coderecv++;

	/*
	 * Process the median filter, and pass the
	 * offset and dispersion along. We use lastrec as both the
	 * reference time and receive time in order to avoid being cute,
	 * like setting the reference time later than the receive time,
	 * which may cause a paranoid protocol module to chuck out the
	 * data.
 	 */
	if (!gpstm_process(gpstm, &tstmp, &dispersion)) {
		gpstm->baddata++;
		gpstm_rep_event(gpstm, CEVNT_BADTIME);
		return;
	}
	refclock_receive(gpstm->peer, &tstmp, GMT, dispersion,
			 &gpstm->lastrec, &gpstm->lastrec, gpstm->leap);

	/*
	 * We have succedded in answering the poll.  Turn off the flag
	 */
	gpstm->polled = 0;
}

/*
 * gpstm_send - time to send the clock a signal to cough up a time sample
 */
static void
gpstm_send(gpstm, cmd)
	struct gpstm_unit *gpstm;
	char *cmd;
{
#ifdef DEBUG
	if (debug) {
		printf("gpstm_send(gpstm%d): %s\n", gpstm->unit, cmd);
	}
#endif
	if (!readonlyclockflag[gpstm->unit]) {
		register int len = strlen(cmd);

		if (write(gpstm->io.fd, cmd, len) != len) {
			syslog(LOG_ERR, "gpstm_send: unit %d: %m",
			       gpstm->unit);
			gpstm_rep_event(gpstm, CEVNT_FAULT);
		}
	}
}

/*
 * state machine for initializing the clock
 */

static void
gpstm_doevent(unit, event)
	int unit;
	enum gpstm_event event;
{
	struct gpstm_unit *gpstm = gpstm_units[unit];

#ifdef DEBUG
	if (debug) {
		printf("gpstm_doevent(gpstm%d, %d)\n", unit, (int)event);
	}
#endif
	if (event == e_TS && State[unit] != F51 && State[unit] != F08) {
		gpstm_send(gpstm, "\03\r");
	}

	switch (event) {
	case e_Init:
		gpstm_send(gpstm, "F18\r");
		State[unit] = Start;
		break;
	case e_F18:
		gpstm_send(gpstm, "F50\r");
		State[unit] = F18;
		break;
	case e_F50:
		gpstm_send(gpstm, "F51\r");
		State[unit] = F50;
		break;
	case e_F51:
		gpstm_send(gpstm, "F08\r");
		State[unit] = F51;
		break;
	case e_TS:
		/* nothing to send - we like this mode */
		State[unit] = F08;
		break;
	}
}

static void
gpstm_initstate(unit) {
	State[unit] = Base;		/* just in case */
	gpstm_doevent(unit, e_Init);
}

/*
 * gpstm_process - process a pile of samples from the clock
 */
static char
gpstm_process(gpstm, offset, dispersion)
	struct gpstm_unit *gpstm;
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
			tmp_ui = gpstm->offset[i].l_ui;
			tmp_uf = gpstm->offset[i].l_uf;
			M_SUB(tmp_ui, tmp_uf, gpstm->offset[j].l_ui,
				gpstm->offset[j].l_uf);
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
	if (gpstm->lasttime == 0 || disp_tmp2 > MAXDISPERSE)
	    disp_tmp2 = MAXDISPERSE;
	if (not_median1 == 0) {
		if (not_median2 == 1)
		    median = 2;
		else
		    median = 1;
        } else {
		median = 0;
        }
	*offset = gpstm->offset[median];
	*dispersion = disp_tmp2;
	return 1;
}

/*
 * gpstm_poll - called by the transmit procedure
 */
static void
gpstm_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	struct gpstm_unit *gpstm;

	/*
	 * You don't need to poll this clock.  It puts out timecodes
	 * once per second.  If asked for a timestamp, take note.
	 * The next time a timecode comes in, it will be fed back.
	 */
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "gpstm_poll: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "gpstm_poll: unit %d not in use", unit);
		return;
	}
	gpstm = gpstm_units[unit];
	if ((current_time - gpstm->lasttime) > 150) {
		gpstm->noreply++;
		gpstm_rep_event(gpstm_units[unit], CEVNT_TIMEOUT);
		gpstm_initstate(gpstm->unit);
	}

	/*
	 * polled every 64 seconds.  Ask our receiver to hand in a timestamp.
	 */
	gpstm->polled = 1;
	gpstm->polls++;
}

/*
 * gpstm_control - set fudge factors, return statistics
 */
static void
gpstm_control(unit, in, out)
	u_int unit;
	struct refclockstat *in;
	struct refclockstat *out;
{
	register struct gpstm_unit *gpstm;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "gpstm_control: unit %d invalid", unit);
		return;
	}
	gpstm = gpstm_units[unit];

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
				peer = gpstm->peer;
				peer->stratum = stratumtouse[unit];
				if (stratumtouse[unit] <= 1)
					memmove((char *)&peer->refid,
						REFID, 4);
				else
					peer->refid = htonl(HSREFID);
			}
		}
		if (in->haveflags & CLK_HAVEFLAG1) {
			readonlyclockflag[unit] = in->flags & CLK_FLAG1;
		}
	}

	if (out != 0) {
		out->type = REFCLK_GPSTM_TRUETIME;
		out->haveflags = CLK_HAVETIME1 | CLK_HAVETIME2
				| CLK_HAVEVAL1 | CLK_HAVEVAL2
				| CLK_HAVEFLAG1;
		out->clockdesc = DESCRIPTION;
		out->fudgetime1 = fudgefactor1[unit];
		out->fudgetime2 = fudgefactor2[unit];
		out->fudgeval1 = (LONG)stratumtouse[unit];
		out->fudgeval2 = 0;
		out->flags = readonlyclockflag[unit];
		if (unitinuse[unit]) {
			out->lencode = gpstm->lencode;
			out->lastcode = gpstm->lastcode;
			out->timereset = current_time - gpstm->timestarted;
			out->polls = gpstm->polls;
			out->noresponse = gpstm->noreply;
			out->badformat = gpstm->badformat;
			out->baddata = gpstm->baddata;
			out->lastevent = gpstm->lastevent;
			out->currentstatus = gpstm->status;
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
 * gpstm_buginfo - return clock dependent debugging info
 */
static void
gpstm_buginfo(unit, bug)
	int unit;
	register struct refclockbug *bug;
{
	register struct gpstm_unit *gpstm;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "gpstm_buginfo: unit %d invalid", unit);
		return;
	}

	if (!unitinuse[unit])
		return;
	gpstm = gpstm_units[unit];

	bug->nvalues = 11;
	bug->ntimes = 5;
	if (gpstm->lasttime != 0)
		bug->values[0] = current_time - gpstm->lasttime;
	else
		bug->values[0] = 0;
	bug->values[1] = (U_LONG)gpstm->reason;
	bug->values[2] = (U_LONG)gpstm->year;
	bug->values[3] = (U_LONG)gpstm->day;
	bug->values[4] = (U_LONG)gpstm->hour;
	bug->values[5] = (U_LONG)gpstm->minute;
	bug->values[6] = (U_LONG)gpstm->second;
	bug->values[7] = (U_LONG)gpstm->msec;
	bug->values[8] = gpstm->noreply;
	bug->values[9] = gpstm->yearstart;
	bug->values[10] = gpstm->quality;
	bug->stimes = 0x1c;
	bug->times[0] = gpstm->lastref;
	bug->times[1] = gpstm->lastrec;
	bug->times[2] = gpstm->offset[0];
	bug->times[3] = gpstm->offset[1];
	bug->times[4] = gpstm->offset[2];
}

#endif /*GPSTM et al*/
