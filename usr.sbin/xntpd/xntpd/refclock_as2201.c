/*
 * refclock_gps - clock driver for the Austron 2201A GPS Timing Receiver
 */
#if defined(REFCLOCK) && (defined(AS2201) || defined(AS2201CLK) || defined(AS2201PPS))

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
#if defined(AS2201CLK)
#include <sys/clkdefs.h>
#endif /* AS2201CLK */
#endif /* STREAM */

#if defined (AS2201PPS)
#include <sys/ppsclock.h>
#endif /* AS2201PPS */

#include "ntp_stdlib.h"

/*
 * This driver supports the Austron 2200A/2201A GPS Receiver with
 * Buffered RS-232-C Interface Module. Note that the original 2200/2201
 * receivers will not work reliably with this driver, since the older
 * design cannot accept input commands at any reasonable data rate.
 *
 * The program sends a "*toc\r" to the radio and expects a response of
 * the form "yy:ddd:hh:mm:ss.mmm\r" where yy = year of century, ddd =
 * day of year, hh:mm:ss = second of day and mmm = millisecond of
 * second. Then, it sends statistics commands to the radio and expects
 * a multi-line reply showing the corresponding statistics or other
 * selected data. Statistics commands are sent in order as determined by
 * a vector of commands; these might have to be changed with different
 * radio options.
 *
 * In order for this code to work, the radio must be placed in non-
 * interactive mode using the "off" command and with a single <cr>
 * resonse using the "term cr" command. The setting of the "echo"
 * and "df" commands does not matter. The radio should select UTC
 * timescale using the "ts utc" command.
 *
 * There are two modes of operation for this driver. The first with
 * undefined AS2201PPS is used with stock kernels and serial-line drivers
 * and works with almost any machine. In this mode the driver assumes
 * the radio captures a timestamp upon receipt of the "*" that begins
 * the driver query. Accuracies in this mode are in the order of a
 * millisecond or two and the receiver can be connected to only one
 * host. The second with AS2201PPS defined can be used for SunOS kernels
 * that have been modified with the ppsclock streams module included in
 * this distribution. In this mode a precise timestamp is available
 * using a gadget box and 1-pps signal from the receiver; however, the
 * sample rate is limited to the polling rate, normally about one poll
 * every 16 seconds. This improves the accuracy to the order of a few
 * tens of microseconds. In addition, the serial output and 1-pps signal
 * can be bussed to additional receivers. For the utmost accuracy, the
 * sample rate can be increased to one per second using the PPSCD
 * define. This improves the accuracy to the order of a few
 * microseconds. 
 */

/*
 * Definitions
 */
#define	MAXUNITS	4	/* max number of GPS units */
#define	GPS232	"/dev/gps%d"	/* name of radio device */
#define	SPEED232	B9600	/* uart speed (9600 baud) */

/*
 * Radio interface parameters
 */
#define	GPSPRECISION	(-15)	/* precision assumed (about 30 us) */
#define	GPSREFID	"GPS"	/* reference id */
#define	GPSDESCRIPTION	"Austron 2201A GPS Receiver" /* who we are */
#define	GPSHSREFID	0x7f7f040a /* 127.127.4.10 refid hi strata */
#define GMT		0	/* hour offset from Greenwich */
#define	NCODES		3	/* stages of median filter */
#define	LENTOC		19	/* yy:ddd:hh:mm:ss.mmm datecode length */
#define BMAX		100	/* timecode buffer length */
#define SMAX		200	/* statistics buffer length */
#define	CODEDIFF	0x20000000	/* 0.125 seconds as an l_fp fraction */

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
 * GPS unit control structure.
 */
struct gpsunit {
	struct peer *peer;	/* associated peer structure */
	struct refclockio io;	/* given to the I/O handler */
	l_fp lastrec;		/* last data receive time */
	l_fp lastref;		/* last timecode time */
	l_fp offset[NCODES];	/* recent sample offsets */
	char lastcode[BMAX];	/* last timecode received */
	char *lastptr;		/* statistics buffer pointer */
	char stats[SMAX];	/* statistics buffer */
	u_char lencode;		/* length of last received ASCII string */
	U_LONG lasttime;	/* last time clock heard from */
#ifdef AS2201PPS
	U_LONG lastev;		/* last ppsclock second */
#endif /* AS2201PPS */
	u_char unit;		/* unit number for this guy */
	u_char status;		/* clock status */
	u_char lastevent;	/* last clock event */
	u_char reason;		/* reason for last abort */
	u_char year;		/* year of eternity */
	u_short day;		/* day of year */
	u_char hour;		/* hour of day */
	u_char minute;		/* minute of hour */
	u_char second;		/* seconds of minute */
	u_short msec;		/* milliseconds of second */
	u_char leap;		/* leap indicators */
	U_LONG yearstart;	/* start of current year */
	int linect;		/* count of lines remaining */
	int index;		/* current statistics command */
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
 * Radio commands to extract statitistics
 *
 * A command consists of an ASCII string terminated by a <cr> (\r). The
 * command list consist of a sequence of commands terminated by a null
 * string ("\0"). One command from the list is sent immediately
 * following each received timecode (*toc\r command) and the ASCII
 * strings received from the radio are saved along with the timecode in
 * the clockstats file. Subsequent commands are sent at each timecode,
 * with the last one in the list followed by the first one. The data
 * received from the radio consist of ASCII strings, each terminated by
 * a <cr> (\r) character. The number of strings for each command is
 * specified as the first line of output as an ASCII-encode number. Note
 * that the ETF command requires the Input Buffer Module and the LORAN
 * commands require the LORAN Assist Module. However, if these modules
 * are not installed, the radio and this driver will continue to operate
 * successfuly, but no data will be captured for these commands.
 */
static char stat_command[][30] = {
	"ITF\r",		/* internal time/frequency */
	"ETF\r",		/* external time/frequency */
	"LORAN ENSEMBLE\r",	/* GPS/LORAN ensemble statistics */
	"LORAN TDATA\r",	/* LORAN signal data */
	"ID;OPT;VER\r",		/* model; options; software version */

	"ITF\r",		/* internal time/frequency */
	"ETF\r",		/* external time/frequency */
	"LORAN ENSEMBLE\r",	/* GPS/LORAN ensemble statistics */
	"TRSTAT\r",		/* satellite tracking status */
	"POS;PPS;PPSOFF\r",	/* position, pps source, offsets */

	"ITF\r",		/* internal time/frequency */
	"ETF\r",		/* external time/frequency */
	"LORAN ENSEMBLE\r",	/* GPS/LORAN ensemble statistics */
	"LORAN TDATA\r",	/* LORAN signal data */
	"UTC\r",			/* UTC leap info */

	"ITF\r",		/* internal time/frequency */
	"ETF\r",		/* external time/frequency */
	"LORAN ENSEMBLE\r",	/* GPS/LORAN ensemble statistics */
	"TRSTAT\r",		/* satellite tracking status */
	"OSC;ET;TEMP\r",	/* osc type; tune volts; oven temp */
	"\0"			/* end of table */
};

/*
 * Data space for the unit structures.  Note that we allocate these on
 * the fly, but never give them back.
 */
static struct gpsunit *gpsunits[MAXUNITS];
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
static	void	as2201_init	P(());
static	int	as2201_start	P((u_int, struct peer *));
static	void	as2201_shutdown	P((int));
static	void	as2201_report_event	P((struct gpsunit *, int));
static	void	as2201_receive	P((struct recvbuf *));
static	char	as2201_process	P((struct gpsunit *, l_fp *, u_fp *));
static	void	as2201_poll	P((int unit, struct peer *));
static	void	as2201_control	P((u_int, struct refclockstat *, struct refclockstat *));
static	void	as2201_buginfo	P((int, struct refclockbug *));

/*
 * Transfer vector
 */
struct  refclock refclock_as2201 = {
	as2201_start, as2201_shutdown, as2201_poll,
	as2201_control, as2201_init, as2201_buginfo, NOFLAGS
};

/*
 * as2201_init - initialize internal gps driver data
 */
static void
as2201_init()
{
	register int i;
	/*
	 * Just zero the data arrays
	 */
	bzero((char *)gpsunits, sizeof gpsunits);
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
 * as2201_start - open the GPS devices and initialize data for processing
 */
static int
as2201_start(unit, peer)
	u_int unit;
	struct peer *peer;
{
	register struct gpsunit *gps;
	register int i;
	int fd232;
	char as2201dev[20];
#ifdef AS2201PPS
	struct ppsclockev ev;
#endif /* AS2201PPS */

	/*
	 * Check configuration info
	 */
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "gps_start: unit %d invalid", unit);
		return (0);
	}
	if (unitinuse[unit]) {
		syslog(LOG_ERR, "gps_start: unit %d in use", unit);
		return (0);
	}

	/*
	 * Open serial port
	 */
	(void) sprintf(as2201dev, GPS232, unit);
	fd232 = open(as2201dev, O_RDWR, 0777);
	if (fd232 == -1) {
		syslog(LOG_ERR, "gps_start: open of %s: %m", as2201dev);
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
		    "as2201_start: ioctl(%s, TCGETA): %m", as2201dev);
                goto screwed;
        }
        ttyb.c_iflag = IGNBRK|IGNPAR|ICRNL;
        ttyb.c_oflag = 0;
        ttyb.c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyb.c_lflag = ICANON;
	ttyb.c_cc[VERASE] = ttyb.c_cc[VKILL] = '\0';
        if (ioctl(fd232, TCSETA, &ttyb) < 0) {
                syslog(LOG_ERR,
		    "as2201_start: ioctl(%s, TCSETA): %m", as2201dev);
                goto screwed;
        }
    }
#endif /* HAVE_SYSV_TTYS */
#if defined(STREAM)
	/*
	 * POSIX/STREAMS serial line parameters (termios interface)
	 *
	 * The AS2201CLK option provides timestamping at the driver level. 
	 * It requires the tty_clk streams module.
	 *
	 * The AS2201PPS option provides timestamping at the driver level.
	 * It uses a 1-pps signal and level converter (gadget box) and
	 * requires the ppsclock streams module and SunOS 4.1.1 or
	 * later.
	 */
    {	struct termios ttyb, *ttyp;

	ttyp = &ttyb;
	if (tcgetattr(fd232, ttyp) < 0) {
                syslog(LOG_ERR,
		    "as2201_start: tcgetattr(%s): %m", as2201dev);
                goto screwed;
        }
        ttyp->c_iflag = IGNBRK|IGNPAR|ICRNL;
        ttyp->c_oflag = 0;
        ttyp->c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyp->c_lflag = ICANON;
	ttyp->c_cc[VERASE] = ttyp->c_cc[VKILL] = '\0';
        if (tcsetattr(fd232, TCSANOW, ttyp) < 0) {
                syslog(LOG_ERR,
		    "as2201_start: tcsetattr(%s): %m", as2201dev);
                goto screwed;
        }
        if (tcflush(fd232, TCIOFLUSH) < 0) {
                syslog(LOG_ERR,
		    "as2201_start: tcflush(%s): %m", as2201dev);
                goto screwed;
        }
#if defined(AS2201CLK)
	if (ioctl(fd232, I_PUSH, "clk") < 0)
		syslog(LOG_ERR,
		    "as2201_start: ioctl(%s, I_PUSH, clk): %m", as2201dev);
	if (ioctl(fd232, CLK_SETSTR, "\n") < 0)
		syslog(LOG_ERR,
		    "as2201_start: ioctl(%s, CLK_SETSTR): %m", as2201dev);
#endif /* AS2201CLK */
#if defined(AS2201PPS)
	if (ioctl(fd232, I_PUSH, "ppsclock") < 0)
		syslog(LOG_ERR,
		    "as2201_start: ioctl(%s, I_PUSH, ppsclock): %m", as2201dev);
	else
		fdpps = fd232;
#endif /* AS2201PPS */
    }
#endif /* STREAM */
#if defined(HAVE_BSD_TTYS)
	/*
	 * 4.3bsd serial line parameters (sgttyb interface)
	 *
	 * The AS2201CLK option provides timestamping at the driver level. 
	 * It requires the tty_clk line discipline and 4.3bsd or later.
	 */
    {	struct sgttyb ttyb;
#if defined(AS2201CLK)
	int ldisc = CLKLDISC;
#endif /* AS2201CLK */

	if (ioctl(fd232, TIOCGETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "as2201_start: ioctl(%s, TIOCGETP): %m", as2201dev);
		goto screwed;
	}
	ttyb.sg_ispeed = ttyb.sg_ospeed = SPEED232;
#if defined(AS2201CLK)
	ttyb.sg_erase = ttyb.sg_kill = '\r';
	ttyb.sg_flags = RAW;
#else
	ttyb.sg_erase = ttyb.sg_kill = '\0';
	ttyb.sg_flags = EVENP|ODDP|CRMOD;
#endif /* AS2201CLK */
	if (ioctl(fd232, TIOCSETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "as2201_start: ioctl(%s, TIOCSETP): %m", as2201dev);
		goto screwed;
	}
#if defined(AS2201CLK)
	if (ioctl(fd232, TIOCSETD, &ldisc) < 0) {
		syslog(LOG_ERR,
		    "as2201_start: ioctl(%s, TIOCSETD): %m",as2201dev);
		goto screwed;
	}
#endif /* AS2201CLK */
    }
#endif /* HAVE_BSD_TTYS */

	/*
	 * Allocate unit structure
	 */
	if (gpsunits[unit] != 0) {
		gps = gpsunits[unit];	/* The one we want is okay */
	} else {
		for (i = 0; i < MAXUNITS; i++) {
			if (!unitinuse[i] && gpsunits[i] != 0)
				break;
		}
		if (i < MAXUNITS) {
			/*
			 * Reclaim this one
			 */
			gps = gpsunits[i];
			gpsunits[i] = 0;
		} else {
			gps = (struct gpsunit *)
			    emalloc(sizeof(struct gpsunit));
		}
	}
	bzero((char *)gps, sizeof(struct gpsunit));
	gpsunits[unit] = gps;

	/*
	 * Set up the structures
	 */
	gps->peer = peer;
	gps->unit = (u_char)unit;
	gps->timestarted = current_time;
	gps->lastptr = gps->stats;
	gps->index = 0;

	gps->io.clock_recv = as2201_receive;
	gps->io.srcclock = (caddr_t)gps;
	gps->io.datalen = 0;
	gps->io.fd = fd232;
#ifdef AS2201PPS
	if (ioctl(fd232, CIOGETEV, (caddr_t)&ev) < 0) {
		syslog(LOG_ERR,
		    "gps_start: ioctl(%s, CIOGETEV): %m", as2201dev);
		goto screwed;
	} else
		gps->lastev = ev.tv.tv_sec;
#endif /* AS2201PPS */
	if (!io_addclock(&gps->io)) {
		goto screwed;
	}

	/*
	 * All done.  Initialize a few random peer variables, then
	 * return success. Note that root delay and root dispersion are
	 * always zero for this clock.
	 */
	peer->precision = GPSPRECISION;
	peer->rootdelay = 0;
	peer->rootdispersion = 0;
	peer->stratum = stratumtouse[unit];
	if (stratumtouse[unit] <= 1)
	    bcopy(GPSREFID, (char *)&peer->refid, 4);
	else
	    peer->refid = htonl(GPSHSREFID);
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
 * as2201_shutdown - shut down a GPS clock
 */
static void
as2201_shutdown(unit)
	int unit;
{
	register struct gpsunit *gps;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "gps_shutdown: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "gps_shutdown: unit %d not in use", unit);
		return;
	}

	/*
	 * Tell the I/O module to turn us off.  We're history.
	 */
	gps = gpsunits[unit];
	io_closeclock(&gps->io);
	unitinuse[unit] = 0;
}


/*
 * as2201_report_event - note the occurance of an event
 *
 * This routine presently just remembers the report and logs it, but
 * does nothing heroic for the trap handler.
 */
static void
as2201_report_event(gps, code)
	struct gpsunit *gps;
	int code;
{
	struct peer *peer;

	peer = gps->peer;
	if (gps->status != (u_char)code) {
		gps->status = (u_char)code;
		if (code != CEVNT_NOMINAL)
			gps->lastevent = (u_char)code;
		syslog(LOG_INFO,
		    "clock %s event %x\n", ntoa(&peer->srcadr), code);
	}
}


/*
 * as2201_receive - receive data from the serial interface
 */
static void
as2201_receive(rbufp)
	struct recvbuf *rbufp;
{
	register int i;
	register struct gpsunit *gps;

#if defined(AS2201PPS)
	struct ppsclockev ev;
	l_fp trtmp;
#endif /* AS2201PPS */
	register u_char *dpt;
	register char *cp, *dp;
	int dpend;
	l_fp tstmp;
	u_fp dispersion;

	/*
	 * Get the clock this applies to and pointers to the data.
	 * Edit the timecode to remove control chars and trashbits.
	 */
	gps = (struct gpsunit *)rbufp->recv_srcclock;
	dpt = (u_char *)&rbufp->recv_space;
	dpend = rbufp->recv_length;
	if (dpend > BMAX - 1)
		dpend = BMAX - 1;
	cp = dp = gps->lastcode;
	for (i = 0; i < dpend; i++)
		if ((*dp = 0x7f & *dpt++) >= ' ') dp++;
	*dp = '\0';
	gps->lencode = dp - cp;
#ifdef DEBUG
	if (debug)
        	printf("gps: timecode %d %d %s\n",
		    gps->linect, gps->lencode, gps->lastcode);
#endif
	if (gps->lencode == 0)
		return;

	/*
	 * If linect is greater than zero, we must be in the middle of a
	 * statistics operation, so simply tack the received data at the
	 * end of the statistics string. If not, we could either have
	 * just received the timecode itself or a decimal number
	 * indicating the number of following lines of the statistics
	 * reply. In the former case, write the accumulated statistics
	 * data to the clockstats file and continue onward to process
	 * the timecode; in the later case, save the number of lines and
	 * quietly return.
	 */
	if (gps->linect > 0) {
		gps->linect--;
		if ((int)(gps->lastptr - gps->stats + gps->lencode) > SMAX - 2)
			return;
		*gps->lastptr++ = ' ';
		(void)strcpy(gps->lastptr, gps->lastcode);
		gps->lastptr += gps->lencode;
		return;
	} else {
		if (gps->lencode == 1) {
			gps->linect = atoi(gps->lastcode);
			return;
		} else {
			record_clock_stats(&(gps->peer->srcadr), gps->stats);
#ifdef DEBUG
			if (debug)
				printf("gps: stat %s\n", gps->stats);
#endif
		}
	}
	gps->lastptr = gps->stats;
	*gps->lastptr = '\0';

	/*
	 * We check the timecode format and decode its contents. The
	 * timecode has format yy:ddd:hh:mm:ss.mmm). If it has invalid
	 * length or is not in proper format, the driver declares bad
	 * format and exits. If the converted decimal values are out of
	 * range, the driver declares bad data and exits.
	 */
	if (gps->lencode != LENTOC || !isdigit(cp[0]) ||
	    !isdigit(cp[1]) || !isdigit(cp[3]) || !isdigit(cp[4]) ||
	    !isdigit(cp[5]) || !isdigit(cp[7]) || !isdigit(cp[8]) ||
	    !isdigit(cp[10]) || !isdigit(cp[11]) || !isdigit(cp[13]) ||
	    !isdigit(cp[14]) || !isdigit(cp[16]) || !isdigit(cp[17]) ||
	    !isdigit(cp[18])) {
		gps->badformat++;
		as2201_report_event(gps, CEVNT_BADREPLY);
		return;
	}

	/*
	 * Convert date and check values.
	 */
	gps->day = cp[3] - '0';
	gps->day = MULBY10(gps->day) + cp[4] - '0';
	gps->day = MULBY10(gps->day) + cp[5] - '0';
	if (gps->day < 1 || gps->day > 366) {
		gps->baddata++;
		as2201_report_event(gps, CEVNT_BADDATE);
		return;
	}

	/*
	 * Convert time and check values.
	 */
	gps->hour = MULBY10(cp[7] - '0') + cp[8] - '0';
	gps->minute = MULBY10(cp[10] - '0') + cp[11] -  '0';
	gps->second = MULBY10(cp[13] - '0') + cp[14] - '0';
	gps->msec = MULBY10(MULBY10(cp[16] - '0') + cp[17] - '0')
	    + cp[18] - '0';
	if (gps->hour > 23 || gps->minute > 59 || gps->second > 59) {
		gps->baddata++;
		as2201_report_event(gps, CEVNT_BADTIME);
		return;
	}

	/*
	 * Test for synchronization (this is a temporary crock).
	 */
	if (cp[2] != ':')
		gps->leap = LEAP_NOTINSYNC;
	else
		gps->lasttime = current_time;

	/*
	 * Now, compute the reference time value. Use the heavy
	 * machinery for the second, which presumably is the one which
	 * occured at the last pps pulse and which was captured by the
	 * loop_filter module. All we have to do here is present a
	 * reasonable facsimile of the time at that pulse so the clock-
	 * filter and selection machinery declares us truechimer. The
	 * precision offset within the second is really tuned by the
	 * loop_filter module. Note that this code does not yet know how
	 * to do the years and relies on the clock-calendar chip for
	 * sanity.
	 */
	if (!clocktime(gps->day, gps->hour, gps->minute,
	    gps->second, GMT, gps->lastrec.l_ui,
	    &gps->yearstart, &gps->lastref.l_ui)) {
		gps->baddata++;
		as2201_report_event(gps, CEVNT_BADTIME);
		
	printf("gps: bad data\n");

		return;
	}
	MSUTOTSF(gps->msec, gps->lastref.l_uf);
#if defined(AS2201PPS)

	/*
	 * If the pps signal is available and the local time is within
	 * +-0.5 second of the timecode, use the pps offset instead.
	 * Note that we believe the ppsclock timestamp only if the ioctl
	 * works and the new timestamp is greater than the previous one.
	 */
	gps->lastrec = rbufp->recv_time;
	tstmp = gps->lastref;
	L_SUB(&tstmp, &gps->lastrec);
	L_ADD(&tstmp, &(fudgefactor[gps->unit]));
	trtmp = tstmp;
	if (L_ISNEG(&trtmp))
		L_NEG(&trtmp);
	if (trtmp.l_i < CLOCK_MAX_I || (trtmp.l_i == CLOCK_MAX_I
	    && (U_LONG)trtmp.l_uf < (U_LONG)CLOCK_MAX_F)) {
		if (ioctl(fdpps, CIOGETEV, (caddr_t)&ev) >= 0) {
			if (gps->lastev < ev.tv.tv_sec) {
				trtmp.l_ui = ev.tv.tv_sec + (U_LONG)JAN_1970;
				TVUTOTSF(ev.tv.tv_usec, trtmp.l_uf);
				L_NEG(&trtmp);
				tstmp.l_i = tstmp.l_f = 0;
				M_ADDF(tstmp.l_i, tstmp.l_f, trtmp.l_f);
			}
			gps->lastev = ev.tv.tv_sec;
		}
	}
#else
	tstmp = gps->lastref;
	L_SUB(&tstmp, &gps->lastrec);
	L_ADD(&tstmp, &(fudgefactor[gps->unit]));
#endif /* AS2201PPS */
	i = ((int)(gps->coderecv)) % NCODES;
	gps->offset[i] = tstmp;
	gps->coderecv++;
#if DEBUG
	if (debug)
		printf("gps: times %s %s %s\n",
		    ulfptoa(&gps->lastref, 6), ulfptoa(&gps->lastrec, 6),
		    lfptoa(&tstmp, 6));
#endif

	/*
	 * If the statistics-record switch (CLK_FLAG4) is set,
	 * initialize the statistics buffer and send the next command.
	 * If not, simply write the timecode to the clockstats file.
	 */
	(void)strcpy(gps->lastptr, gps->lastcode);
	gps->lastptr += gps->lencode;
	if (sloppyclockflag[gps->unit] & CLK_FLAG4) {
		*gps->lastptr++ = ' ';
		(void)strcpy(gps->lastptr, stat_command[gps->index]);
		gps->lastptr += strlen(stat_command[gps->index]);
		gps->lastptr--;
		*gps->lastptr = '\0';
		(void)write(gps->io.fd, stat_command[gps->index],
		    strlen(stat_command[gps->index]));
		gps->index++;
		if (*stat_command[gps->index] == '\0')
			gps->index = 0;
	}

	/*
	 * Process the samples in the median filter, add the fudge
	 * factor and pass the offset and dispersion along. We use
	 * lastref as both the reference time and receive time in order
	 * to avoid being cute, like setting the reference time later
	 * than the receive time, which may cause a paranoid protocol
	 * module to chuck out the data.
 	 */
	if (gps->coderecv < NCODES)
		return;
	if (!as2201_process(gps, &tstmp, &dispersion)) {
		gps->baddata++;
		as2201_report_event(gps, CEVNT_BADTIME);
		return;
	}
	refclock_receive(gps->peer, &tstmp, GMT, dispersion,
	    &gps->lastrec, &gps->lastrec, gps->leap);
}

/*
 * as2201_process - process a pile of samples from the clock
 *
 * This routine uses a three-stage median filter to calculate offset and
 * dispersion and reduce jitter. The dispersion is calculated as the
 * span of the filter (max - min).
 */
static char
as2201_process(gps, offset, dispersion)
	struct gpsunit *gps;
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
			tmp_ui = gps->offset[i].l_ui;
			tmp_uf = gps->offset[i].l_uf;
			M_SUB(tmp_ui, tmp_uf, gps->offset[j].l_ui,
				gps->offset[j].l_uf);
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
	if (gps->lasttime == 0)
	    disp_tmp2 = NTP_MAXDISPERSE;
	else
	    disp_tmp2 = current_time - gps->lasttime;
	if (not_median1 == 0) {
		if (not_median2 == 1)
		    median = 2;
		else
		    median = 1;
        } else {
		median = 0;
        }
	*offset = gps->offset[median];
	*dispersion = disp_tmp2;
	return (1);
}

/*
 * as2201_poll - called by the transmit procedure
 *
 * We go to great pains to avoid changing state here, since there may be
 * more than one eavesdropper receiving the same timecode.
 */
static void
as2201_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	struct gpsunit *gps;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "gps_poll: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "gps_poll: unit %d not in use", unit);
		return;
	}
	gps = gpsunits[unit];
	if ((current_time - gps->lasttime) > 150)
		as2201_report_event(gpsunits[unit], CEVNT_TIMEOUT);
	gettstamp(&gps->lastrec);
	if (write(gps->io.fd, "\r*toc\r", 6) != 6) {
		syslog(LOG_ERR, "gps_poll: unit %d: %m", gps->unit);
		as2201_report_event(gps, CEVNT_FAULT);
	} else
		gps->polls++;
}

/*
 * as2201_control - set fudge factors, return statistics
 */
static void
as2201_control(unit, in, out)
	u_int unit;
	struct refclockstat *in;
	struct refclockstat *out;
{
	register struct gpsunit *gps;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "gps_control: unit %d invalid", unit);
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
				gps = gpsunits[unit];
				peer = gps->peer;
				peer->stratum = stratumtouse[unit];
				if (stratumtouse[unit] <= 1)
					bcopy(GPSREFID, (char *)&peer->refid,
					    4);
				else
					peer->refid = htonl(GPSHSREFID);
			}
		}
		if (in->haveflags & CLK_HAVEFLAG4) {
			sloppyclockflag[unit] = in->flags & CLK_FLAG4;
		}
	}

	if (out != 0) {
		out->type = REFCLK_GPS_AS2201;
		out->haveflags
		    = CLK_HAVETIME1|CLK_HAVEVAL1|CLK_HAVEVAL2|CLK_HAVEFLAG4;
		out->clockdesc = GPSDESCRIPTION;
		out->fudgetime1 = fudgefactor[unit];
		out->fudgetime2.l_ui = 0;
		out->fudgetime2.l_uf = 0;
		out->fudgeval1 = (LONG)stratumtouse[unit];
		out->fudgeval2 = 0;
		out->flags = sloppyclockflag[unit];
		if (unitinuse[unit]) {
			gps = gpsunits[unit];
			out->lencode = LENTOC;
			out->lastcode = gps->stats;
			out->timereset = current_time - gps->timestarted;
			out->polls = gps->polls;
			out->noresponse = gps->noreply;
			out->badformat = gps->badformat;
			out->baddata = gps->baddata;
			out->lastevent = gps->lastevent;
			out->currentstatus = gps->status;
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
 * as2201_buginfo - return clock dependent debugging info
 */
static void
as2201_buginfo(unit, bug)
	int unit;
	register struct refclockbug *bug;
{
	register struct gpsunit *gps;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "gps_buginfo: unit %d invalid", unit);
		return;
	}

	if (!unitinuse[unit])
		return;
	gps = gpsunits[unit];

	bug->nvalues = 10;
	bug->ntimes = 5;
	if (gps->lasttime != 0)
		bug->values[0] = current_time - gps->lasttime;
        else
		bug->values[0] = 0;
	bug->values[1] = (U_LONG)gps->reason;
	bug->values[2] = (U_LONG)gps->year;
	bug->values[3] = (U_LONG)gps->day;
	bug->values[4] = (U_LONG)gps->hour;
	bug->values[5] = (U_LONG)gps->minute;
	bug->values[6] = (U_LONG)gps->second;
	bug->values[7] = (U_LONG)gps->msec;
	bug->values[8] = gps->noreply;
	bug->values[9] = gps->yearstart;
	bug->stimes = 0x1c;
	bug->times[0] = gps->lastref;
	bug->times[1] = gps->lastrec;
	bug->times[2] = gps->offset[0];
	bug->times[3] = gps->offset[1];
	bug->times[4] = gps->offset[2];
}
#endif
