/*
 * refclock_trak.c - clock driver for the TRAK 8810 GPS STATION CLOCK
 *		Tsuruoka Tomoaki Oct 30, 1993 
 *
 */
#if defined(REFCLOCK) && (defined(TRAK) || defined(TRAKCLK) || defined(TRAKPPS))

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
#if defined(TRAKCLK)
#include <sys/clkdefs.h>
#endif /* TRAKCLK */
#endif /* STREAM */

#if defined (TRAKPPS)
#include <sys/ppsclock.h>
#endif /* TRAKPPS */

#include "ntp_stdlib.h"

/*
 * This driver supports the TRAK 8810 GPS Receiver with
 * Buffered RS-232-C Interface Module. 
 *
 * Most of codes are copied from refclock_as2201.c, Thanks a lot.
 *
 * The program expects the radio responses once per seccond
 * ( by "rqts,u" command or panel control )
 * of the form "*RQTS U,ddd:hh:mm:ss.0,Q\r\n for UTC" where 
 * ddd= day of year
 * hh=  hours
 * mm=  minutes
 * ss=  seconds
 * Q=   Quality byte. Q=0 Phase error > 20 us
 *		      Q=6 Pahse error < 20 us
 *				      > 10 us
 *		      Q=5 Pahse error < 10 us
 *				      > 1 us
 *		      Q=4 Pahse error < 1 us
 *				      > 100 ns
 *		      Q=3 Pahse error < 100 ns
 *				      > 10 ns
 *		      Q=2 Pahse error < 10 ns
 *  (note that my clock almost stable at 1 us per 10 hours)
 *
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
#define	GPSPRECISION	(-20)	/* precision assumed (about 1 us) */
#define	GPSREFID	"GPS"	/* reference id */
#define	GPSDESCRIPTION	"TRAK 8810 GPS station clock" /* who we are */
#define	GPSHSREFID	0x7f7f110a /* 127.127.17.10 refid hi strata */
#define GMT		0	/* hour offset from Greenwich */
#define	NCODES		3	/* stages of median filter */
#define	LENTOC		25	/* *RQTS U,ddd:hh:mm:ss.0,Q datecode length */
#define BMAX		100	/* timecode buffer length */
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
	u_short	polled;		/* when polled, means a last sample */
	u_char lencode;		/* length of last received ASCII string */
	U_LONG lasttime;	/* last time clock heard from */
#ifdef TRAKPPS
	U_LONG lastev;		/* last ppsclock second */
#endif /* TRAKPPS */
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
static	void	trak_init	P(());
static	int	trak_start	P((u_int, struct peer *));
static	void	trak_shutdown	P((int));
static	void	trak_report_event	P((struct gpsunit *, int));
static	void	trak_receive	P((struct recvbuf *));
static	char	trak_process	P((struct gpsunit *, l_fp *, u_fp *));
static	void	trak_poll	P((int unit, struct peer *));
static	void	trak_control	P((u_int, struct refclockstat *, struct refclockstat *));
static	void	trak_buginfo	P((int, struct refclockbug *));

/*
 * Transfer vector
 */
struct  refclock refclock_trak = {
	trak_start, trak_shutdown, trak_poll,
	trak_control, trak_init, trak_buginfo, NOFLAGS
};

/*
 * trak_init - initialize internal gps driver data
 */
static void
trak_init()
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
 * trak_start - open the GPS devices and initialize data for processing
 */
static int
trak_start(unit, peer)
	u_int unit;
	struct peer *peer;
{
	register struct gpsunit *gps;
	register int i;
	int fd232;
	char trakdev[20];
#ifdef TRAKPPS
	struct ppsclockev ev;
#endif /* TRAKPPS */

	/*
	 * Check configuration info
	 */
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "trak_start: unit %d invalid", unit);
		return (0);
	}
	if (unitinuse[unit]) {
		syslog(LOG_ERR, "trak_start: unit %d in use", unit);
		return (0);
	}

	/*
	 * Open serial port
	 */
	(void) sprintf(trakdev, GPS232, unit);
	fd232 = open(trakdev, O_RDWR, 0777);
	if (fd232 == -1) {
		syslog(LOG_ERR, "trak_start: open of %s: %m", trakdev);
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
		    "trak_start: ioctl(%s, TCGETA): %m", trakdev);
                goto screwed;
        }
        ttyb.c_iflag = IGNBRK|IGNPAR|ICRNL;
        ttyb.c_oflag = 0;
        ttyb.c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyb.c_lflag = ICANON;
	ttyb.c_cc[VERASE] = ttyb.c_cc[VKILL] = '\0';
        if (ioctl(fd232, TCSETA, &ttyb) < 0) {
                syslog(LOG_ERR,
		    "trak_start: ioctl(%s, TCSETA): %m", trakdev);
                goto screwed;
        }
    }
#endif /* HAVE_SYSV_TTYS */
#if defined(STREAM)
	/*
	 * POSIX/STREAMS serial line parameters (termios interface)
	 *
	 * The TRAKCLK option provides timestamping at the driver level. 
	 * It requires the tty_clk streams module.
	 *
	 * The TRAKPPS option provides timestamping at the driver level.
	 * It uses a 1-pps signal and level converter (gadget box) and
	 * requires the ppsclock streams module and SunOS 4.1.1 or
	 * later.
	 */
    {	struct termios ttyb, *ttyp;

	ttyp = &ttyb;
	if (tcgetattr(fd232, ttyp) < 0) {
                syslog(LOG_ERR,
		    "trak_start: tcgetattr(%s): %m", trakdev);
                goto screwed;
        }
        ttyp->c_iflag = IGNBRK|IGNPAR;
        ttyp->c_oflag = 0;
        ttyp->c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyp->c_lflag = ICANON;
	ttyp->c_cc[VERASE] = ttyp->c_cc[VKILL] = '\0';
        if (tcsetattr(fd232, TCSANOW, ttyp) < 0) {
                syslog(LOG_ERR,
		    "trak_start: tcsetattr(%s): %m", trakdev);
                goto screwed;
        }
        if (tcflush(fd232, TCIOFLUSH) < 0) {
                syslog(LOG_ERR,
		    "trak_start: tcflush(%s): %m", trakdev);
                goto screwed;
        }
#if defined(TRAKCLK)
	if (ioctl(fd232, I_PUSH, "clk") < 0)
		syslog(LOG_ERR,
		    "trak_start: ioctl(%s, I_PUSH, clk): %m", trakdev);
	if (ioctl(fd232, CLK_SETSTR, "*") < 0)
		syslog(LOG_ERR,
		    "trak_start: ioctl(%s, CLK_SETSTR): %m", trakdev);
#endif /* TRAKCLK */
#if defined(TRAKPPS)
	if (ioctl(fd232, I_PUSH, "ppsclock") < 0)
		syslog(LOG_ERR,
		    "trak_start: ioctl(%s, I_PUSH, ppsclock): %m", trakdev);
	else
		fdpps = fd232;
#endif /* TRAKPPS */
    }
#endif /* STREAM */
#if defined(HAVE_BSD_TTYS)
	/*
	 * 4.3bsd serial line parameters (sgttyb interface)
	 *
	 * The TRAKCLK option provides timestamping at the driver level. 
	 * It requires the tty_clk line discipline and 4.3bsd or later.
	 */
    {	struct sgttyb ttyb;
#if defined(TRAKCLK)
	int ldisc = CLKLDISC;
#endif /* TRAKCLK */

	if (ioctl(fd232, TIOCGETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "trak_start: ioctl(%s, TIOCGETP): %m", trakdev);
		goto screwed;
	}
	ttyb.sg_ispeed = ttyb.sg_ospeed = SPEED232;
#if defined(TRAKCLK)
	ttyb.sg_erase = ttyb.sg_kill = '\r';
	ttyb.sg_flags = RAW;
#else
	ttyb.sg_erase = ttyb.sg_kill = '\0';
	ttyb.sg_flags = EVENP|ODDP|CRMOD;
#endif /* TRAKCLK */
	if (ioctl(fd232, TIOCSETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "trak_start: ioctl(%s, TIOCSETP): %m", trakdev);
		goto screwed;
	}
#if defined(TRAKCLK)
	if (ioctl(fd232, TIOCSETD, &ldisc) < 0) {
		syslog(LOG_ERR,
		    "trak_start: ioctl(%s, TIOCSETD): %m",trakdev);
		goto screwed;
	}
#endif /* TRAKCLK */
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

	gps->io.clock_recv = trak_receive;
	gps->io.srcclock = (caddr_t)gps;
	gps->io.datalen = 0;
	gps->io.fd = fd232;
#ifdef TRAKPPS
	if (ioctl(fd232, CIOGETEV, (caddr_t)&ev) < 0) {
		syslog(LOG_ERR,
		    "trak_start: ioctl(%s, CIOGETEV): %m", trakdev);
		goto screwed;
	} else
		gps->lastev = ev.tv.tv_sec;
#endif /* TRAKPPS */
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
	/*
	 *	request to give time code
	 */
	{
		void gps_send();
		gps_send(gps,"\rRQTS,U\r");
		gps_send(gps,"SEL 00\r");
	}

	return (1);

	/*
	 * Something broke; abandon ship.
	 */
screwed:
	(void) close(fd232);
	return (0);
}

/*
 * trak_shutdown - shut down a GPS clock
 */
static void
trak_shutdown(unit)
	int unit;
{
	register struct gpsunit *gps;
	void gps_send();

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "trak_shutdown: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "trak_shutdown: unit %d not in use", unit);
		return;
	}
	gps = gpsunits[unit];
	/*
	 *	request not to give time code any more
	 */
	gps_send(gps,"RQTX\r");
	/*
	 * Tell the I/O module to turn us off.  We're history.
	 */
	io_closeclock(&gps->io);

	unitinuse[unit] = 0;
}


/*
 * trak_report_event - note the occurance of an event
 *
 * This routine presently just remembers the report and logs it, but
 * does nothing heroic for the trap handler.
 */
static void
trak_report_event(gps, code)
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
 * trak_receive - receive data from the serial interface
 */
static void
trak_receive(rbufp)
	struct recvbuf *rbufp;
{
	register int i,cmdtype;
	register struct gpsunit *gps;

#if defined(TRAKPPS)
	struct ppsclockev ev;
	l_fp trtmp;
#endif /* TRAKPPS */
	register u_char *dpt;
	register u_char *cp;
	register u_char *dpend;
	l_fp tstmp;
	u_fp dispersion;

	/*
	 * Get the clock this applies to and pointers to the data.
	 * Edit the timecode to remove control chars and trashbits.
	 */
	gps = (struct gpsunit *)rbufp->recv_srcclock;
	dpt = (u_char *)&rbufp->recv_space;
	dpend = dpt + rbufp->recv_length;
	cp = (u_char *)gps->lastcode;

	while (dpt < dpend) {
#ifdef TRAKCLK	/* prior to TRAKPPS due to timestamp */
		if ((*cp = 0x7f & *dpt++) != '*' ) cp++;
		else if (*cp == '*' ) { /* caught magic character */
			if ( dpend - dpt < 8) {
				/* short timestamp */
				if(debug) puts("gps: short timestamp.");
				return;
			}
			if (!buftvtots(dpt,&gps->lastrec)) {
				/* screwy timestamp */
				if(debug) puts("gps: screwy timestamp.");
				return;
			}
			dpt += 8;
		}
#else
#ifdef TRAKPPS
		if ((*cp = 0x7f & *dpt++) >= ' ') cp++;
#else
	/* both are not specified */
#endif /* TRAKPPS */
#endif /* TRAKCLK */
	}
	*cp = '\0';
	gps->lencode = cp - (u_char *)gps->lastcode;
	if (gps->lencode == 0) return;

#ifdef DEBUG
	if (debug)
        	printf("gps: timecode %d %s\n",
		    gps->lencode, gps->lastcode);
#endif

	/*
	 * We check the timecode format and decode its contents. The
	 * timecode has format *........RQTS U,ddd:hh:mm:ss.0,Q\r\n).
         *                                     012345678901234567890123
	 */
	cp = (u_char *)gps->lastcode;
	gps->leap = 0;
	cmdtype=0;
	if (strncmp(cp,"RQTS",4)==0) {
		cmdtype=1;
		cp += 7;
		}
	else if(strncmp(cp,"*RQTS",5)==0) {
		cmdtype=2;
		cp += 8;
		}
	else return;
	switch( cmdtype ) {
	case 1:
	case 2:
		/*
		 *	Check time code format of TRAK 8810
		 */
		if(	!isdigit(cp[0]) ||
			!isdigit(cp[1]) ||
			!isdigit(cp[2]) ||
			cp[3] != ':'	||
			!isdigit(cp[4]) ||
			!isdigit(cp[5]) ||
			cp[6] != ':'	||
			!isdigit(cp[7]) ||
			!isdigit(cp[8]) ||
			cp[9] != ':'	||
			!isdigit(cp[10])||
			!isdigit(cp[11])) {
				gps->badformat++;
				trak_report_event(gps, CEVNT_BADREPLY);
				return;
			}
		break;
	default:
		return;

	}

	/*
	 * Convert date and check values.
	 */
	gps->day = cp[0] - '0';
	gps->day = MULBY10(gps->day) + cp[1] - '0';
	gps->day = MULBY10(gps->day) + cp[2] - '0';
	if (gps->day < 1 || gps->day > 366) {
		gps->baddata++;
		trak_report_event(gps, CEVNT_BADDATE);
		return;
	}
	/*
	 * Convert time and check values.
	 */
	gps->hour = MULBY10(cp[4] - '0') + cp[5] - '0';
	gps->minute = MULBY10(cp[7] - '0') + cp[8] -  '0';
	gps->second = MULBY10(cp[10] - '0') + cp[11] - '0';
	gps->msec = 0; 
	if (gps->hour > 23 || gps->minute > 59 || gps->second > 59) {
		gps->baddata++;
		trak_report_event(gps, CEVNT_BADTIME);
		return;
	}

	/*
	 * Test for synchronization
	 */
/*
	switch( cp[15] ) {
	case '0':
		if(gps->peer->stratum == stratumtouse[gps->unit]) {
			gps->peer->stratum = 10 ;
			bzero(&gps->peer->refid,4);
		}
		break;
	default:
		if(gps->peer->stratum != stratumtouse[gps->unit]) {
			gps->peer->stratum = stratumtouse[gps->unit]   ;
			bcopy(GPSREFID,&gps->peer->refid,4);
		}
		break;
	}
*/
	gps->lasttime = current_time;

	if (!gps->polled) return;

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

#if defined(TRAKPPS)

	/*
	 *  timestamp must be greater than previous one.
	 */
	if (ioctl(fdpps, CIOGETEV, (caddr_t)&ev) >= 0) {
		ev.tv.tv_sec += (U_LONG)JAN_1970;
		TVTOTS(&ev.tv,&gps->lastrec);
		if (gps->lastev < ev.tv.tv_sec) {
			gps->lastev = ev.tv.tv_sec;
		} else {	/* in case of 1-pps missing */
			gps->lastev = ev.tv.tv_sec;
			return;
		}
	}
	else
		return;	/* failed to get timestamp */
#endif /* TRAKPPS */

	if (!clocktime(gps->day, gps->hour, gps->minute,
	    gps->second, GMT, gps->lastrec.l_ui,
	    &gps->yearstart, &gps->lastref.l_ui)) {
		gps->baddata++;
		trak_report_event(gps, CEVNT_BADTIME);
#ifdef DEBUG		
		if(debug) printf("gps: bad date \n");
#endif
		return;
	}
	MSUTOTSF(gps->msec, gps->lastref.l_uf);
	tstmp = gps->lastref;

	L_SUB(&tstmp, &gps->lastrec);
	L_ADD(&tstmp, &(fudgefactor[gps->unit]));
	i = ((int)(gps->coderecv)) % NCODES;
	gps->offset[i] = tstmp;
	gps->coderecv++;
#if DEBUG
	if (debug)
		printf("gps: times %s %s %s\n",
		    ulfptoa(&gps->lastref, 6), ulfptoa(&gps->lastrec, 6),
		    lfptoa(&tstmp, 6));
#endif
/*	if( tstmp.l_ui != 0 ) return;  something wrong */

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
	if (!trak_process(gps, &tstmp, &dispersion)) {
		gps->baddata++;
		trak_report_event(gps, CEVNT_BADTIME);
		return;
	}
	refclock_receive(gps->peer, &tstmp, GMT, dispersion,
	    &gps->lastrec, &gps->lastrec, gps->leap);
	/*
	 *	after all, clear polled flag
	*/
	gps->polled = 0;
}

/*
 * ==================================================================
 *	gps_send(gps,cmd)  Sends a command to the GPS receiver.
 *	 as	gps_send(gps,"rqts,u\r");
 * ==================================================================
 */
static void
gps_send(gps,cmd)
	struct gpsunit *gps;
	char *cmd;
{
	if (write(gps->io.fd, cmd, strlen(cmd)) == -1) {
		syslog(LOG_ERR, "gps_send: unit %d: %m", gps->unit);
		trak_report_event(gps,CEVNT_FAULT);
	} else {
		gps->polls++;
	}
}

/*
 * trak_process - process a pile of samples from the clock
 *
 * This routine uses a three-stage median filter to calculate offset and
 * dispersion and reduce jitter. The dispersion is calculated as the
 * span of the filter (max - min).
 */
static char
trak_process(gps, offset, dispersion)
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
 * trak_poll - called by the transmit procedure
 *
 * We go to great pains to avoid changing state here, since there may be
 * more than one eavesdropper receiving the same timecode.
 */
static void
trak_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	struct gpsunit *gps;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "trak_poll: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "trak_poll: unit %d not in use", unit);
		return;
	}
	gps = gpsunits[unit];
	if ((current_time - gps->lasttime) > 150)
		trak_report_event(gpsunits[unit], CEVNT_TIMEOUT);
	/*
	 * usually trak_receive can get a timestamp every second
	 */
#ifndef TRAKPPS && TRAKCLK
	gettstamp(&gps->lastrec);
#endif
	gps->polls++;
	/*
	 *	may be polled every 64 seconds
	 */
	gps->polled = 1;
}

/*
 * trak_control - set fudge factors, return statistics
 */
static void
trak_control(unit, in, out)
	u_int unit;
	struct refclockstat *in;
	struct refclockstat *out;
{
	register struct gpsunit *gps;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "trak_control: unit %d invalid", unit);
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
	}

	if (out != 0) {
		out->type = REFCLK_GPS_TRAK;
		out->haveflags
		    = CLK_HAVETIME1|CLK_HAVEVAL1|CLK_HAVEVAL2;
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
 * trak_buginfo - return clock dependent debugging info
 */
static void
trak_buginfo(unit, bug)
	int unit;
	register struct refclockbug *bug;
{
	register struct gpsunit *gps;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "trak_buginfo: unit %d invalid", unit);
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
