/*
 * refclock_wwvb - clock driver for the Spectracom WWVB receivers
 */
#if defined(REFCLOCK) && (defined(WWVB) || defined(WWVBCLK) || defined(WWVBPPS))

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
#if defined(WWVBCLK)
#include <sys/clkdefs.h>
#endif /* WWVBCLK */
#endif /* STREAM */

#if defined (WWVBPPS)
#include <sys/ppsclock.h>
#endif /* WWVBPPS */

#include "ntp_stdlib.h"

/*
 * This driver supports the Spectracom Model 8170 and Netclock/2 WWVB
 * Synchronized Clock under Unix and on a Gizmo board. There are two
 * formats used by these clocks. Format 0 (zero), which is available
 * with both the Netclock/2 and 8170, is in the following format:
 *
 * <cr><lf>I<sp><sp>ddd<sp>hh:mm:ss<sp><sp>TZ=nn<cr><lf>
 *
 * The ddd, hh, mm and ss fields show the day of year, hours, minutes
 * and seconds, respectively. The nn field shows the local hour offset
 * relative to UTC and should always be set to 00. The I is normally
 * <sp> when the clock is synchronized and '?' when it isn't (it could
 * also be a '*' if we set the time manually, but this is forbidden.
 *
 * Format 2 (two), which is available only with the Netclock/2 and
 * specially modified 8170, is in the following format:
 *
 * <cr><lf>IQyy<sp>ddd<sp>hh:mm:ss.mmm<sp>LD
 *
 * The ddd, hh and ss fields and I are as in format 0. The yy field
 * shows the year and mmm the milliseconds, respectively. The Q is
 * normally <sp> when the time error is less than 1 ms and and a
 * character in the set 'A'...'D' when the time error is less than 10,
 * 100, 500 and greater than 500 ms respectively. The L is normally
 * <sp>, but is set to 'L' early in the month of an upcoming UTC
 * leap second and reset to <sp> on the first day of the following
 * month. The D is set to 'S' for standard time 'I' on the day
 * preceding a switch to daylight time, 'D' for daylight time and 'O'
 * on the day preceding a switch to standard time. The start bit of the
 * first <cr> is supposed to be synchronized to the on-time second.
 *
 * This driver does not need to be told which format is in use - it
 * figures out which one from the length of the message. A three-stage
 * median filter is used to reduce jitter and provide a dispersion
 * measure. The driver makes no attempt to correct for the intrinsic
 * jitter of the radio itself, which is a known problem with the older
 * radios.
 *
 * This driver supports the 1-pps signal provided by the radio and
 * connected via a level converted described in the gadget directory.
 * The signal is captured using a separate, dedicated, serial port and
 * the tty_clk line discipline/streams modules described in the kernel
 * directory. For the highest precision, the signal is captured using
 * the carrier-detect line of the same serial port using the ppsclock
 * streams module described in the ppsclock directory.
 *
 * Bugs:
 *
 * The year indication so carefully provided in format 2 is not used.
 */

/*
 * Definitions
 */
#define	MAXUNITS	4	/* max number of WWVB units */
#define	WWVB232	"/dev/wwvb%d"	/* name of radio device */
#define	SPEED232	B9600	/* uart speed (9600 baud) */

/*
 * Radio interface parameters
 */
#define	WWVBPRECISION	(-13)	/* precision assumed (about 100 us) */
#define	WWVBREFID	"WWVB"	/* reference id */
#define	WWVBDESCRIPTION	"Spectracom WWVB Receiver" /* who we are */
#define	WWVBHSREFID	0x7f7f040a /* 127.127.4.10 refid hi strata */
#define GMT		0	/* hour offset from Greenwich */
#define	NCODES		3	/* stages of median filter */
#define	LENWWVB0	22	/* format 0 timecode length */
#define	LENWWVB2	24	/* format 2 timecode length */
#define FMTWWVBU	0	/* unknown format timecode id */
#define FMTWWVB0	1	/* format 0 timecode id */
#define FMTWWVB2	2	/* format 2 timecode id */
#define BMAX		80	/* timecode buffer length */
#define	CODEDIFF	0x20000000 /* 0.125 seconds as an l_fp fraction */
#define MONLIN		15	/* number of monitoring lines */
/*
 * Hack to avoid excercising the multiplier.  I have no pride.
 */
#define	MULBY10(x)	(((x)<<3) + ((x)<<1))

/*
 * Imported from ntp_timer module
 */
extern U_LONG current_time;	/* current time (s) */

/*
 * Imported from ntp_loopfilter module
 */
extern int fdpps;		/* pps file descriptor */

/*
 * Imported from ntpd module
 */
extern int debug;		/* global debug flag */

/*
 * WWVB unit control structure
 */
struct wwvbunit {
	struct peer *peer;	/* associated peer structure */
	struct refclockio io;	/* given to the I/O handler */
	l_fp lastrec;		/* last receive time */
	l_fp lastref;		/* last timecode time */
	l_fp offset[NCODES];	/* recent sample offsets */
	char lastcode[BMAX];	/* last timecode received */
	u_char format;		/* timecode format */
	u_char tcswitch;	/* timecode switch */
	u_char pollcnt;		/* poll message counter */
	u_char lencode;		/* length of last timecode */
	U_LONG lasttime;	/* last time clock heard from */
	u_char unit;		/* unit number for this guy */
	u_char status;		/* clock status */
	u_char lastevent;	/* last clock event */
	u_char reason;		/* reason for last abort */
	u_char year;		/* year of eternity */
	u_short day;		/* day of year */
	u_char hour;		/* hour of day */
	u_char minute;		/* minute of hour */
	u_char second;		/* seconds of minute */
	u_char leap;		/* leap indicators */
	u_short msec;		/* millisecond of second */
	u_char quality;		/* quality char from format 2 */
	U_LONG yearstart;	/* start of current year */
	u_char lasthour;	/* last hour (for monitor) */
	u_char linect;		/* count of ignored lines (for monitor */

	/*
	 * Status tallies
 	 */
	U_LONG polls;		/* polls sent */
	U_LONG noreply;		/* no replies to polls */
	U_LONG coderecv;	/* timecodes received */
	U_LONG badformat;	/* bad format */
	U_LONG baddata;		/* bad data */
	U_LONG timestarted;	/* time we started this */
};

/*
 * Data space for the unit structures.  Note that we allocate these on
 * the fly, but never give them back.
 */
static struct wwvbunit *wwvbunits[MAXUNITS];
static u_char unitinuse[MAXUNITS];

/*
 * Keep the fudge factors separately so they can be set even
 * when no clock is configured.
 */
static l_fp fudgefactor[MAXUNITS];
static u_char stratumtouse[MAXUNITS];
static u_char sloppyclockflag[MAXUNITS];

/*
 * Function prototypes
 */
static	void	wwvb_init	P((void));
static	int	wwvb_start	P((u_int, struct peer *));
static	void	wwvb_shutdown	P((int));
static	void	wwvb_report_event	P((struct wwvbunit *, int));
static	void	wwvb_receive	P((struct recvbuf *));
static	char	wwvb_process	P((struct wwvbunit *, l_fp *, u_fp *));
static	void	wwvb_poll	P((int, struct peer *));
static	void	wwvb_control	P((u_int, struct refclockstat *, struct refclockstat *));
static	void	wwvb_buginfo	P((int, struct refclockbug *));

/*
 * Transfer vector
 */
struct	refclock refclock_wwvb = {
	wwvb_start, wwvb_shutdown, wwvb_poll,
	wwvb_control, wwvb_init, wwvb_buginfo, NOFLAGS
};

/*
 * wwvb_init - initialize internal wwvb driver data
 */
static void
wwvb_init()
{
	register int i;
	/*
	 * Just zero the data arrays
	 */
	bzero((char *)wwvbunits, sizeof wwvbunits);
	bzero((char *)unitinuse, sizeof unitinuse);

	/*
	 * Initialize fudge factors to default.
	 */
	for (i = 0; i < MAXUNITS; i++) {
		fudgefactor[i].l_ui = 0;
		fudgefactor[i].l_uf = 0;
		stratumtouse[i] = 0;
		sloppyclockflag[i] = 0;
	}
}


/*
 * wwvb_start - open the WWVB devices and initialize data for processing
 */
static int
wwvb_start(unit, peer)
	u_int unit;
	struct peer *peer;
{
	register struct wwvbunit *wwvb;
	register int i;
	int fd232;
	char wwvbdev[20];

	/*
	 * Check configuration info
	 */
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "wwvb_start: unit %d invalid", unit);
		return (0);
	}
	if (unitinuse[unit]) {
		syslog(LOG_ERR, "wwvb_start: unit %d in use", unit);
		return (0);
	}

	/*
	 * Open serial port
	 */
	(void) sprintf(wwvbdev, WWVB232, unit);
	fd232 = open(wwvbdev, O_RDWR, 0777);
	if (fd232 == -1) {
		syslog(LOG_ERR, "wwvb_start: open of %s: %m", wwvbdev);
		return (0);
	}

#if defined(HAVE_SYSV_TTYS)
	/*
	 * System V serial line parameters (termio interface)
	 *
	 */
    {	struct termio ttyb;
	if (ioctl(fd232, TCGETA, &ttyb) < 0) {
                syslog(LOG_ERR,
		    "wwvb_start: ioctl(%s, TCGETA): %m", wwvbdev);
                goto screwed;
        }
        ttyb.c_iflag = IGNBRK|IGNPAR|ICRNL;
        ttyb.c_oflag = 0;
        ttyb.c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyb.c_lflag = ICANON;
	ttyb.c_cc[VERASE] = ttyb.c_cc[VKILL] = '\0';
        if (ioctl(fd232, TCSETA, &ttyb) < 0) {
                syslog(LOG_ERR,
		    "wwvb_start: ioctl(%s, TCSETA): %m", wwvbdev);
                goto screwed;
        }
    }
#endif /* HAVE_SYSV_TTYS */
#if defined(STREAM)
	/*
	 * POSIX/STREAMS serial line parameters (termios interface)
	 *
	 * The WWVBCLK option provides timestamping at the driver level. 
	 * It requires the tty_clk streams module.
	 *
	 * The WWVBPPS option provides timestamping at the driver level.
	 * It uses a 1-pps signal and level converter (gadget box) and
	 * requires the ppsclock streams module and SunOS 4.1.1 or
	 * later.
	 */
    {	struct termios ttyb, *ttyp;

	ttyp = &ttyb;
	if (tcgetattr(fd232, ttyp) < 0) {
                syslog(LOG_ERR,
		    "wwvb_start: tcgetattr(%s): %m", wwvbdev);
                goto screwed;
        }
        ttyp->c_iflag = IGNBRK|IGNPAR|ICRNL;
        ttyp->c_oflag = 0;
        ttyp->c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyp->c_lflag = ICANON;
	ttyp->c_cc[VERASE] = ttyp->c_cc[VKILL] = '\0';
        if (tcsetattr(fd232, TCSANOW, ttyp) < 0) {
                syslog(LOG_ERR,
		    "wwvb_start: tcsetattr(%s): %m", wwvbdev);
                goto screwed;
        }
        if (tcflush(fd232, TCIOFLUSH) < 0) {
                syslog(LOG_ERR,
		    "wwvb_start: tcflush(%s): %m", wwvbdev);
                goto screwed;
        }
#if defined(WWVBCLK)
	if (ioctl(fd232, I_PUSH, "clk") < 0)
		syslog(LOG_ERR,
		    "wwvb_start: ioctl(%s, I_PUSH, clk): %m", wwvbdev);
	if (ioctl(fd232, CLK_SETSTR, "\n") < 0)
		syslog(LOG_ERR,
		    "wwvb_start: ioctl(%s, CLK_SETSTR): %m", wwvbdev);
#endif /* WWVBCLK */
#if defined(WWVBPPS)
	if (ioctl(fd232, I_PUSH, "ppsclock") < 0)
		syslog(LOG_ERR,
		    "wwvb_start: ioctl(%s, I_PUSH, ppsclock): %m", wwvbdev);
	else
		fdpps = fd232;
#endif /* WWVBPPS */
    }
#endif /* STREAM */
#if defined(HAVE_BSD_TTYS)
	/*
	 * 4.3bsd serial line parameters (sgttyb interface)
	 *
	 * The WWVBCLK option provides timestamping at the driver level. 
	 * It requires the tty_clk line discipline and 4.3bsd or later.
	 */
    {	struct sgttyb ttyb;
#if defined(WWVBCLK)
	int ldisc = CLKLDISC;
#endif /* WWVBCLK */

	if (ioctl(fd232, TIOCGETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "wwvb_start: ioctl(%s, TIOCGETP): %m", wwvbdev);
		goto screwed;
	}
	ttyb.sg_ispeed = ttyb.sg_ospeed = SPEED232;
#if defined(WWVBCLK)
	ttyb.sg_erase = ttyb.sg_kill = '\r';
	ttyb.sg_flags = RAW;
#else
	ttyb.sg_erase = ttyb.sg_kill = '\0';
	ttyb.sg_flags = EVENP|ODDP|CRMOD;
#endif /* WWVBCLK */
	if (ioctl(fd232, TIOCSETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "wwvb_start: ioctl(%s, TIOCSETP): %m", wwvbdev);
		goto screwed;
	}
#if defined(WWVBCLK)
	if (ioctl(fd232, TIOCSETD, &ldisc) < 0) {
		syslog(LOG_ERR,
		    "wwvb_start: ioctl(%s, TIOCSETD): %m",wwvbdev);
		goto screwed;
	}
#endif /* WWVBCLK */
    }
#endif /* HAVE_BSD_TTYS */

	/*
	 * Allocate unit structure
	 */
	if (wwvbunits[unit] != 0) {
		wwvb = wwvbunits[unit];	/* The one we want is okay */
	} else {
		for (i = 0; i < MAXUNITS; i++) {
			if (!unitinuse[i] && wwvbunits[i] != 0)
				break;
		}
		if (i < MAXUNITS) {
			/*
			 * Reclaim this one
			 */
			wwvb = wwvbunits[i];
			wwvbunits[i] = 0;
		} else {
			wwvb = (struct wwvbunit *)
			    emalloc(sizeof(struct wwvbunit));
		}
	}
	bzero((char *)wwvb, sizeof(struct wwvbunit));
	wwvbunits[unit] = wwvb;

	/*
	 * Set up the structures
	 */
	wwvb->peer = peer;
	wwvb->unit = (u_char)unit;
	wwvb->timestarted = current_time;
	wwvb->pollcnt = 2;

	wwvb->io.clock_recv = wwvb_receive;
	wwvb->io.srcclock = (caddr_t)wwvb;
	wwvb->io.datalen = 0;
	wwvb->io.fd = fd232;
	if (!io_addclock(&wwvb->io))
		goto screwed;

	/*
	 * All done.  Initialize a few random peer variables, then
	 * return success. Note that root delay and root dispersion are
	 * always zero for this clock.
	 */
	peer->precision = WWVBPRECISION;
	peer->rootdelay = 0;
	peer->rootdispersion = 0;
	peer->stratum = stratumtouse[unit];
	if (stratumtouse[unit] <= 1)
	    bcopy(WWVBREFID, (char *)&peer->refid, 4);
	else
	    peer->refid = htonl(WWVBHSREFID);
	unitinuse[unit] = 1;
	return (1);

	/*
	 * Something broke; abandon ship.
	 */
screwed:
	(void) close(fd232);
	return (0);
}

/*
 * wwvb_shutdown - shut down a WWVB clock
 */
static void
wwvb_shutdown(unit)
	int unit;
{
	register struct wwvbunit *wwvb;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "wwvb_shutdown: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "wwvb_shutdown: unit %d not in use", unit);
		return;
	}

	/*
	 * Tell the I/O module to turn us off.  We're history.
	 */
	wwvb = wwvbunits[unit];
	io_closeclock(&wwvb->io);
	unitinuse[unit] = 0;
}


/*
 * wwvb_report_event - note the occurance of an event
 *
 * This routine presently just remembers the report and logs it, but
 * does nothing heroic for the trap handler.
 */
static void
wwvb_report_event(wwvb, code)
	struct wwvbunit *wwvb;
	int code;
{
	struct peer *peer;

	peer = wwvb->peer;
	if (wwvb->status != (u_char)code) {
		wwvb->status = (u_char)code;
		if (code != CEVNT_NOMINAL)
			wwvb->lastevent = (u_char)code;
		syslog(LOG_INFO,
		    "clock %s event %x", ntoa(&peer->srcadr), code);
	}
}


/*
 * wwvb_receive - receive data from the serial interface on a Spectracom
 * clock
 */
static void
wwvb_receive(rbufp)
	struct recvbuf *rbufp;
{
	register int i;
	register struct wwvbunit *wwvb;
	register char *dpt, *cp, *dp;
	int dpend;
	l_fp tstmp, trtmp;
	u_fp dispersion;

	/*
	 * Get the clock this applies to and a pointers to the data.
	 * Check for the presence of a timestamp left by the tty_clock
	 * line discipline/streams module and, if present, use that
	 * instead of the timestamp captured by the i/o routines.
	 */
	wwvb = (struct wwvbunit *)rbufp->recv_srcclock;
	dpt = (char *)&rbufp->recv_space;
	dpend = rbufp->recv_length;
	if (dpend > BMAX - 1)
		dpend = BMAX - 1;
	wwvb-> pollcnt = 2;
	trtmp = rbufp->recv_time;
	if (dpend >= 9) {
		dp = dpt + dpend - 9;
		if (*dp == '\n' || *dp == '\r') {
			dpend -= 8;
			if (!buftvtots(dp + 1, &tstmp)) {
#ifdef DEBUG
				if (debug)
					printf("wwvb_receive: invalid timestamp");
#endif
			} else {
#ifdef DEBUG
				if (debug) {
					L_SUB(&trtmp, &tstmp);
					printf("wwvb: delta %s",
					    lfptoa(&trtmp, 6));
					gettstamp(&trtmp);
				        L_SUB(&trtmp, &tstmp);
				        printf(" SIGIO %s\n",
                                            lfptoa(&trtmp, 6));
				}
#endif
				trtmp = tstmp;
			}
		}
	}
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
	if (dpend == 1) {
		if (wwvb->tcswitch == 0) {
			wwvb->tcswitch = 1;
			wwvb->lastrec = trtmp;
		} else
			wwvb->tcswitch = 0;
		return;
	}
	tstmp = wwvb->lastrec;
	wwvb->lastrec = trtmp;
	wwvb->tcswitch = 1;

	/*
	 * Edit timecode to remove control chars. Note the receive
	 * timestamp is determined at the first <cr>; however, we don't
	 * get the timecode for that timestamp until the next <cr>. We
	 * assume that, if we happen to come up during a timestamp, or
	 * other awkward time, the format and data checks will cause the
	 * driver to resynchronize after maybe a few false starts.
	 */
	if (dpend <= 0)
		return;
	cp = dp = wwvb->lastcode;
	for (i = 0; i < dpend; i++)
		if ((*dp = 0x7f & *dpt++) >= ' ') dp++; 
	*dp = '\0';
	wwvb->lencode = dp - cp;
	record_clock_stats(&(wwvb->peer->srcadr), wwvb->lastcode);
#ifdef DEBUG
	if (debug)
        	printf("wwvb: timecode %d %d %s\n",
		   wwvb->linect,  wwvb->lencode, wwvb->lastcode);
#endif

	/*
	 * We get down to business, check the timecode format and decode
	 * its contents. This code uses the timecode length to determine
	 * whether format 0 or format 2. If the timecode has invalid
	 * length or is not in proper format, we declare bad format and
	 * exit; if the converted decimal values are out of range, we
	 * declare bad data and exit.
	 */
	cp = wwvb->lastcode;
	wwvb->leap = 0;
	wwvb->format = FMTWWVBU;
	if ((cp[0] == ' ' || cp[0] == '?') && wwvb->lencode == LENWWVB0) {

		/*
	 	 * Check timecode format 0
	 	 */
		if (cp[1] != ' ' ||		/* <sp> separator */
			cp[2] != ' ' ||		/* <sp> separator */
			!isdigit(cp[3]) ||	/* day of year */
			!isdigit(cp[4]) ||
			!isdigit(cp[5]) ||
			cp[6] != ' ' ||		/* <sp> separator */
			!isdigit(cp[7]) ||	/* hours */
			!isdigit(cp[8]) ||
			cp[9] != ':' ||		/* : separator */
			!isdigit(cp[10]) ||	/* minutes */
			!isdigit(cp[11]) ||
			cp[12] != ':' ||	/* : separator */
			!isdigit(cp[13]) ||	/* seconds */
			!isdigit(cp[14])) {
				wwvb->badformat++;
				wwvb_report_event(wwvb, CEVNT_BADREPLY);
				return;
			}
		else
			wwvb->format = FMTWWVB0;

		/*
		 * Convert format 0 and check values 
		 */
		wwvb->year = 0;		/* fake */
		wwvb->day = cp[3] - '0';
		wwvb->day = MULBY10(wwvb->day) + cp[4] - '0';
		wwvb->day = MULBY10(wwvb->day) + cp[5] - '0';
		wwvb->hour = MULBY10(cp[7] - '0') + cp[8] - '0';
		wwvb->minute = MULBY10(cp[10] - '0') + cp[11] -  '0';
		wwvb->second = MULBY10(cp[13] - '0') + cp[14] - '0';
		wwvb->msec = 0;
		if (cp[0] != ' ')
			wwvb->leap = LEAP_NOTINSYNC;
		else
			wwvb->lasttime = current_time;
		if (wwvb->day < 1 || wwvb->day > 366) {
			wwvb->baddata++;
			wwvb_report_event(wwvb, CEVNT_BADDATE);
			return;
		}
		if (wwvb->hour > 23 || wwvb->minute > 59
		    || wwvb->second > 59) {
			wwvb->baddata++;
			wwvb_report_event(wwvb, CEVNT_BADTIME);
			return;
		}
	} else if ((cp[0] == ' ' || cp[0] == '?') && wwvb->lencode == LENWWVB2) {

		/*
	 	 * Check timecode format 2
	 	 */
		if (!isdigit(cp[2]) ||		/* year of century */
			!isdigit(cp[3]) ||
			cp[4] != ' ' ||		/* <sp> separator */
			!isdigit(cp[5]) ||	/* day of year */
			!isdigit(cp[6]) ||
			!isdigit(cp[7]) ||
			cp[8] != ' ' ||		/* <sp> separator */
			!isdigit(cp[9]) ||	/* hour */
			!isdigit(cp[10]) ||
			cp[11] != ':' ||	/* : separator */
			!isdigit(cp[12]) ||	/* minute */
			!isdigit(cp[13]) ||
			cp[14] != ':' ||	/* : separator */
			!isdigit(cp[15]) ||	/* second */
			!isdigit(cp[16]) ||
			cp[17] != '.' ||	/* . separator */
			!isdigit(cp[18]) ||	/* millisecond */
			!isdigit(cp[19]) ||
			!isdigit(cp[20]) ||
			cp[21] != ' ') {	/* <sp> separator */
				wwvb->badformat++;
				wwvb_report_event(wwvb, CEVNT_BADREPLY);
				return;
			}
		else
			wwvb->format = FMTWWVB2;

		/*
		 * Convert format 2 and check values 
		 */
		wwvb->year = MULBY10(cp[2] - '0') + cp[3] - '0';
		wwvb->day = cp[5] - '0';
		wwvb->day = MULBY10(wwvb->day) + cp[6] - '0';
		wwvb->day = MULBY10(wwvb->day) + cp[7] - '0';
		wwvb->hour = MULBY10(cp[9] - '0') + cp[10] - '0';
		wwvb->minute = MULBY10(cp[12] - '0') + cp[13] -  '0';
		wwvb->second = MULBY10(cp[15] - '0') + cp[16] - '0';
		wwvb->msec = cp[18] - '0';
		wwvb->msec = MULBY10(wwvb->msec) + cp[19] - '0';
		wwvb->msec = MULBY10(wwvb->msec) + cp[20] - '0';
		wwvb->quality = cp[1];
		if (cp[0] != ' ')
			wwvb->leap = LEAP_NOTINSYNC;

		/*
		 * This nonsense adjusts the last time the clock was
		 * heard from depending on the quality indicator. Once
		 * the clock has been heard, the dispersion depends only
		 * on when the clock was last heard. The first time the
		 * clock is heard, the time last heard is faked based on
		 * the quality indicator. The magic numbers (in seconds)
		 * are from the clock specifications.
		 */
		if (wwvb->lasttime != 0) {
			if (cp[1] == ' ')
	 			wwvb->lasttime = current_time;
		} else {
			switch (cp[1]) {
			case ' ':
				wwvb->lasttime = current_time;
				break;
			case 'A':
				wwvb->lasttime = current_time - 800;
				break;
			case 'B':
				wwvb->lasttime = current_time - 5300;
				break;
			case 'C':
				wwvb->lasttime = current_time - 25300;
				break;
			/* Don't believe anything else */
			}
		}
		if (cp[22] == 'L')
		    wwvb->leap = LEAP_ADDSECOND;
		if (wwvb->day < 1 || wwvb->day > 366) {
			wwvb->baddata++;
			wwvb_report_event(wwvb, CEVNT_BADDATE);
			return;
		}
		if (wwvb->hour > 23 || wwvb->minute > 59
		    || wwvb->second > 59) {
			wwvb->baddata++;
			wwvb_report_event(wwvb, CEVNT_BADTIME);
			return;
		}
	} else {
		if (wwvb->linect > 0)
			wwvb->linect--;
		else {
			wwvb->badformat++;
			wwvb_report_event(wwvb, CEVNT_BADREPLY);
		}
		return;
	}
        if (sloppyclockflag[wwvb->unit] & CLK_FLAG4 &&
	    wwvb->hour < wwvb->lasthour)
		wwvb->linect = MONLIN;
	wwvb->lasthour = wwvb->hour;

	/*
	 * Now, compute the reference time value. Use the heavy
	 * machinery for the seconds and the millisecond field for the
	 * fraction when present. If an error in conversion to internal
	 * format is found, the program declares bad data and exits.
	 * Note that this code does not yet know how to do the years and
	 * relies on the clock-calendar chip for sanity.
	 */
	if (!clocktime(wwvb->day, wwvb->hour, wwvb->minute,
	    wwvb->second, GMT, tstmp.l_ui,
	    &wwvb->yearstart, &wwvb->lastref.l_ui)) {
		wwvb->baddata++;
		wwvb_report_event(wwvb, CEVNT_BADTIME);
		return;
	}
	MSUTOTSF(wwvb->msec, wwvb->lastref.l_uf);
	i = ((int)(wwvb->coderecv)) % NCODES;
	wwvb->offset[i] = wwvb->lastref;
	L_SUB(&wwvb->offset[i], &tstmp);
	if (wwvb->coderecv == 0)
		for (i = 1; i < NCODES; i++)
			wwvb->offset[i] = wwvb->offset[0];
	wwvb->coderecv++;

	/*
	 * Process the samples in the median filter, add the fudge
	 * factor and pass the offset and dispersion along. We use
	 * lastrec as both the reference time and receive time in order
	 * to avoid being cute, like setting the reference time later
	 * than the receive time, which may cause a paranoid protocol
	 * module to chuck out the data.
 	 */
	if (!wwvb_process(wwvb, &tstmp, &dispersion)) {
		wwvb->baddata++;
		wwvb_report_event(wwvb, CEVNT_BADTIME);
		return;
	}
	L_ADD(&tstmp, &(fudgefactor[wwvb->unit]));
	refclock_receive(wwvb->peer, &tstmp, GMT, dispersion,
	    &wwvb->lastrec, &wwvb->lastrec, wwvb->leap);
}

/*
 * wwvb_process - process a pile of samples from the clock
 *
 * This routine uses a three-stage median filter to calculate offset and
 * dispersion. reduce jitter. The dispersion is calculated as the span
 * of the filter (max - min), unless the quality character (format 2) is
 * non-blank, in which case the dispersion is calculated on the basis of
 * the inherent tolerance of the internal radio oscillator, which is
 * +-2e-5 according to the radio specifications.
 */
static char
wwvb_process(wwvb, offset, dispersion)
	struct wwvbunit *wwvb;
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
	 * and use that as the sample offset. There probably is not much
	 * to be gained by a longer filter, since the clock filter in
	 * ntp_proto should do its thing.
	 */
	disp_tmp2 = 0;
	for (i = 0; i < NCODES-1; i++) {
		for (j = i+1; j < NCODES; j++) {
			tmp_ui = wwvb->offset[i].l_ui;
			tmp_uf = wwvb->offset[i].l_uf;
			M_SUB(tmp_ui, tmp_uf, wwvb->offset[j].l_ui,
				wwvb->offset[j].l_uf);
			if (M_ISNEG(tmp_ui, tmp_uf)) {
				M_NEG(tmp_ui, tmp_uf);
			}
			if (tmp_ui != 0 || tmp_uf > CODEDIFF) {
				return (0);
			}
			disp_tmp = MFPTOFP(0, tmp_uf);
			if (disp_tmp > disp_tmp2) {
				disp_tmp2 = disp_tmp;
				not_median1 = i;
				not_median2 = j;
			}
		}
	}
	if (wwvb->lasttime == 0)
	    disp_tmp2 = NTP_MAXDISPERSE;
	else if (wwvb->quality != ' ')
	    disp_tmp2 = current_time - wwvb->lasttime;
	if (not_median1 == 0) {
		if (not_median2 == 1)
		    median = 2;
		else
		    median = 1;
        } else {
		median = 0;
        }
	*offset = wwvb->offset[median];
	*dispersion = disp_tmp2;
	return (1);
}

/*
 * wwvb_poll - called by the transmit procedure
 */
static void
wwvb_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	struct wwvbunit *wwvb;
	char poll;

	/*
	 * Time to request a time code.  The Spectracom clock responds
	 * to a "T" sent to it by returning a time code as stated in the
	 * comments in the header.  Note there is no checking on state,
	 * since this may not be the only customer reading the clock.
	 * Only one customer need poll the clock; all others just listen
	 * in. If nothing is heard from the clock for two polls, declare
	 * a timeout and keep going.
	 */
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "wwvb_poll: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "wwvb_poll: unit %d not in use", unit);
		return;
	}
	wwvb = wwvbunits[unit];
	if (wwvb->pollcnt > 0) {
		wwvb->pollcnt--;
		if (wwvb->pollcnt == 0)
		    wwvb_report_event(wwvbunits[unit], CEVNT_TIMEOUT);
	}
	if (wwvb->pollcnt == 0)
		wwvb->noreply++;
	if (wwvb->linect > 0)
		poll = 'R';
	else
		poll = 'T';
	if (write(wwvb->io.fd, &poll, 1) != 1) {
		syslog(LOG_ERR, "wwvb_poll: unit %d: %m", wwvb->unit);
		wwvb_report_event(wwvb, CEVNT_FAULT);
	} else {
		wwvb->polls++;
	}
}

/*
 * wwvb_control - set fudge factors, return statistics
 */
static void
wwvb_control(unit, in, out)
	u_int unit;
	struct refclockstat *in;
	struct refclockstat *out;
{
	register struct wwvbunit *wwvb;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "wwvb_control: unit %d invalid", unit);
		return;
	}

	if (in != 0) {
		if (in->haveflags & CLK_HAVETIME1)
			fudgefactor[unit] = in->fudgetime1;
		if (in->haveflags & CLK_HAVEVAL1) {
			stratumtouse[unit] = (u_char)(in->fudgeval1 & 0xf);
			if (unitinuse[unit]) {
				struct peer *peer;

				/*
				 * Should actually reselect clock, but
				 * will wait for the next timecode
				 */
				wwvb = wwvbunits[unit];
				peer = wwvb->peer;
				peer->stratum = stratumtouse[unit];
				if (stratumtouse[unit] <= 1)
					bcopy(WWVBREFID, (char *)&peer->refid,
					    4);
				else
					peer->refid = htonl(WWVBHSREFID);
			}
		}
		if (in->haveflags & CLK_HAVEFLAG4) {
			sloppyclockflag[unit] = in->flags & CLK_FLAG4;
		}
	}

	if (out != 0) {
		out->type = REFCLK_WWVB_SPECTRACOM;
		out->haveflags
		    = CLK_HAVETIME1|CLK_HAVEVAL1|CLK_HAVEVAL2|CLK_HAVEFLAG4;
		out->clockdesc = WWVBDESCRIPTION;
		out->fudgetime1 = fudgefactor[unit];
		out->fudgetime2.l_ui = 0;
		out->fudgetime2.l_uf = 0;
		out->fudgeval1 = (LONG)stratumtouse[unit];
		out->fudgeval2 = 0;
		out->flags = sloppyclockflag[unit];
		if (unitinuse[unit]) {
			wwvb = wwvbunits[unit];
			out->lencode = wwvb->lencode;
			out->lastcode = wwvb->lastcode;
			out->timereset = current_time - wwvb->timestarted;
			out->polls = wwvb->polls;
			out->noresponse = wwvb->noreply;
			out->badformat = wwvb->badformat;
			out->baddata = wwvb->baddata;
			out->lastevent = wwvb->lastevent;
			out->currentstatus = wwvb->status;
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
 * wwvb_buginfo - return clock dependent debugging info
 */
static void
wwvb_buginfo(unit, bug)
	int unit;
	register struct refclockbug *bug;
{
	register struct wwvbunit *wwvb;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "wwvb_buginfo: unit %d invalid", unit);
		return;
	}

	if (!unitinuse[unit])
		return;
	wwvb = wwvbunits[unit];

	bug->nvalues = 11;
	bug->ntimes = 5;
	if (wwvb->lasttime != 0)
		bug->values[0] = current_time - wwvb->lasttime;
	else
		bug->values[0] = 0;
	bug->values[1] = (U_LONG)wwvb->reason;
	bug->values[2] = (U_LONG)wwvb->year;
	bug->values[3] = (U_LONG)wwvb->day;
	bug->values[4] = (U_LONG)wwvb->hour;
	bug->values[5] = (U_LONG)wwvb->minute;
	bug->values[6] = (U_LONG)wwvb->second;
	bug->values[7] = (U_LONG)wwvb->msec;
	bug->values[8] = wwvb->noreply;
	bug->values[9] = wwvb->yearstart;
	bug->values[10] = wwvb->quality;
	bug->stimes = 0x1c;
	bug->times[0] = wwvb->lastref;
	bug->times[1] = wwvb->lastrec;
	bug->times[2] = wwvb->offset[0];
	bug->times[3] = wwvb->offset[1];
	bug->times[4] = wwvb->offset[2];
}
#endif
