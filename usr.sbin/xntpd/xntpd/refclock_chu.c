/*
 * refclock_chu - clock driver for the CHU time code
 */
#if defined(REFCLOCK) && (defined(CHU) || defined(CHUCLK) || defined(CHUPPS))

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
#endif /* STREAM */

#if defined (CHUPPS)
#include <sys/ppsclock.h>
#endif /* CHUPPS */

#include <sys/chudefs.h>

#include "ntp_stdlib.h"

/*
 * The CHU time signal includes a time code which is modulated at the
 * standard Bell 103 frequencies (i.e. mark=2225Hz, space=2025Hz).
 * and formatted into 8 bit characters with one start bit and two
 * stop bits. The time code is composed of 10 8-bit characters.
 * The second 5 bytes of the timecode are a redundancy check, and
 * are a copy of the first 5 bytes.
 *
 * It is assumed that you have built or modified a Bell 103 standard
 * modem, attached the input to the output of a radio and cabled the
 * output to a serial port on your computer, i.e. what you are receiving
 * is essentially the output of your radio.  It is also assumed you have
 * installed a special CHU line discipline to condition the output from
 * the terminal driver and take accurate time stamps.
 *
 * There are two types of timecodes. One is sent in the 32nd
 * through 39th second of the minute.
 *
 *     6dddhhmmss6dddhhmmss
 *
 * where ddd is the day of the year, hh is the hour (in UTC), mm is
 * the minute and ss the second.  The 6 is a constant.  Note that
 * the code is sent twice.
 *
 * The second sort of timecode is sent only during the 31st second
 * past the minute.
 *
 *     xdyyyyttabXDYYYYTTAB
 *
 * In this case, the second part of the code is the one's complement
 * of the code. This differentiates it from the other timecode
 * format.
 *
 * d is the absolute value of DUT (in tenths of a second). yyyy
 * is the year. tt is the difference between UTC and TAI. a is
 * a canadian daylight time flag and b is a serial number.
 * x is a bitwise field. The least significant bit of x is
 * one if DUT is negative. The 2nd bit is set if a leap second
 * will be added at the next opportunity. The 3rd bit is set if
 * a leap second will be deleted at the next opportunity.
 * The 4th bit is an even parity bit for the other three bits
 * in this nibble.
 *
 * The start bit in each character has a precise relationship to
 * the on-time second.  Most often UART's synchronize themselves to the
 * start bit and will post an interrupt at the center of the first stop
 * bit.  Thus each character's interrupt should occur at a fixed offset
 * from the on-time second.  This means that a timestamp taken at the
 * arrival of each character in the code will provide an independent
 * estimate of the offset.  Since there are 10 characters in the time
 * code and the code is sent 9 times per minute, this means you potentially
 * get 90 offset samples per minute.  Much of the code in here is dedicated
 * to producing a single offset estimate from these samples.
 *
 * A note about the line discipline.  It is possible to receive the
 * CHU time code in raw mode, but this has disadvantages.  In particular,
 * this puts a lot of code between the interrupt and the time you freeze
 * a time stamp, decreasing precision.  It is also expensive in terms of
 * context switches, and made even more expensive by the way I do I/O.
 * Worse, since you are listening directly to the output of your radio,
 * CHU is noisy and will make you spend a lot of time receiving noise.
 *
 * The line discipline fixes a lot of this.  It knows that the CHU time
 * code consists of 10 bytes which arrive with an intercharacter
 * spacing of about 37 ms, and that the data is BCD, and filters on this
 * basis.  It delivers block of ten characters plus their associated time
 * stamps all at once.  The time stamps are hence about as accurate as
 * a Unix machine can get them, and much of the noise disappears in the
 * kernel with no context switching cost.
 *
 * The kernel module also will insure that the packets that are
 * delivered have the correct redundancy bytes, and will return
 * a flag in chutype to differentiate one sort of packet from
 * the other.
 */

/*
 * Definitions
 */
#define	MAXUNITS	4	/* maximum number of CHU units permitted */
#define	CHUDEV	"/dev/chu%d"	/* device we open.  %d is unit number */
#define SPEED232 B300		/* uart speed (300 baud) */
#define	NCHUCODES	8	/* expect 8 CHU codes per minute */
#define CHULDISC	10	/* XXX temp CHU line discipline */

/*
 * To compute a quality for the estimate (a pseudo dispersion) we add a
 * fixed 10 ms for each missing code in the minute and add to this
 * the sum of the differences between the remaining offsets and the
 * estimated sample offset.
 */
#define	CHUDELAYPENALTY	0x0000028f

/*
 * Other constant stuff
 */
#define	CHUPRECISION	(-9)	/* what the heck */
#define	CHUREFID	"CHU\0"
#define	CHUDESCRIPTION	"Direct synchronized to CHU timecode"
#define	CHUHSREFID	0x7f7f070a /* 127.127.7.10 refid for hi strata */

/*
 * Default fudge factors
 */
#define	DEFPROPDELAY	0x00624dd3	/* 0.0015 seconds, 1.5 ms */
#define	DEFFILTFUDGE	0x000d1b71	/* 0.0002 seconds, 200 us */

/*
 * Hacks to avoid excercising the multiplier.  I have no pride.
 */
#define	MULBY10(x)	(((x)<<3) + ((x)<<1))
#define	MULBY60(x)	(((x)<<6) - ((x)<<2))	/* watch overflow */
#define	MULBY24(x)	(((x)<<4) + ((x)<<3))

/*
 * Constants for use when multiplying by 0.1.  ZEROPTONE is 0.1
 * as an l_fp fraction, NZPOBITS is the number of significant bits
 * in ZEROPTONE.
 */
#define	ZEROPTONE	0x1999999a
#define	NZPOBITS	29

static char hexstring[]="0123456789abcdef";

/*
 * CHU unit control structure.
 */
struct chuunit {
	struct peer *peer;		/* associated peer structure */
	struct event chutimer;		/* timeout timer structure */
	struct refclockio chuio;	/* given to the I/O handler */
	l_fp offsets[NCHUCODES];	/* offsets computed from each code */
	l_fp rectimes[NCHUCODES];	/* times we received this stuff */
	U_LONG reftimes[NCHUCODES];	/* time of last code received */
	u_char lastcode[NCHUCHARS*4];	/* last code we received */
        u_char asciicode[NCHUCHARS*4+1]; /* last code translated to ascii */
	u_char expect;			/* the next offset expected */
	u_char unit;			/* unit number for this guy */
	u_short haveoffset;		/* flag word indicating valid offsets */
	u_short flags;			/* operational flags */
	u_char status;			/* clock status */
	u_char lastevent;		/* last clock event */
	u_char unused[2];
	U_LONG lastupdate;		/* last time data received */
	U_LONG responses;		/* number of responses */
	U_LONG badformat;		/* number of bad format responses */
	U_LONG baddata;			/* number of invalid time codes */
	U_LONG timestarted;		/* time we started this */
	u_char leap;			/* leap status */
};

#define	CHUTIMERSET	0x1		/* timer is set to fire */


/*
 * The CHU table.  This gives the expected time of arrival of each
 * character after the on-time second and is computed as follows:
 * The CHU time code is sent at 300 bps.  Your average UART will
 * synchronize at the edge of the start bit and will consider the
 * character complete at the middle of the first stop bit, i.e.
 * 0.031667 ms later (some UARTS may complete the character at the
 * end of the stop bit instead of the middle, but you can fudge this).
 * Thus the expected time of each interrupt is the start bit time plus
 * 0.031667 seconds.  These times are in chutable[].  To this we add
 * such things as propagation delay and delay fudge factor.
 */
#define	CHARDELAY	0x081b4e82

static U_LONG chutable[NCHUCHARS] = {
	0x22222222 + CHARDELAY,		/* 0.1333333333 */
	0x2b851eb8 + CHARDELAY,		/* 0.170 (exactly) */
	0x34e81b4e + CHARDELAY,		/* 0.2066666667 */
	0x3f92c5f9 + CHARDELAY,		/* 0.2483333333 */
	0x47ae147b + CHARDELAY,		/* 0.280 (exactly) */
	0x51111111 + CHARDELAY,		/* 0.3166666667 */
	0x5a740da7 + CHARDELAY,		/* 0.3533333333 */
	0x63d70a3d + CHARDELAY,		/* 0.390 (exactly) */
	0x6d3a06d4 + CHARDELAY,		/* 0.4266666667 */
	0x769d0370 + CHARDELAY,		/* 0.4633333333 */
};

/*
 * Data space for the unit structures.  Note that we allocate these on
 * the fly, but never give them back.
 */
static struct chuunit *chuunits[MAXUNITS];
static u_char unitinuse[MAXUNITS];

/*
 * Keep the fudge factors separately so they can be set even
 * when no clock is configured.
 */
static l_fp propagation_delay[MAXUNITS];
static l_fp fudgefactor[MAXUNITS];
static l_fp offset_fudge[MAXUNITS];
static u_char stratumtouse[MAXUNITS];
static u_char sloppyclockflag[MAXUNITS];

/*
 * We keep track of the start of the year, watching for changes.
 * We also keep track of whether the year is a leap year or not.
 * All because stupid CHU doesn't include the year in the time code.
 */
static U_LONG yearstart;

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
 * Event reporting.  This optimizes things a little.
 */
#define	chu_event(chu, evcode) \
	do { \
		if ((chu)->status != (u_char)(evcode)) \
			chu_report_event((chu), (evcode)); \
	} while (0)

static	void	chu_init	P((void));
static	int	chu_start	P((u_int, struct peer *));
static	void	chu_shutdown	P((int));
static	void	chu_report_event	P((struct chuunit *, int));
static	void	chu_receive	P((struct recvbuf *));
static	void	chu_process	P((struct chuunit *));
static	void	chu_poll	P((int, struct peer *));
static	void	chu_control	P((u_int, struct refclockstat *, struct refclockstat *));
static	void	chu_timeout	P((struct peer *));

/*
 * Transfer vector
 */
struct	refclock refclock_chu = {
	chu_start, chu_shutdown, chu_poll,
	chu_control, chu_init, noentry, NOFLAGS
};

/*
 * chu_init - initialize internal chu driver data
 */
static void
chu_init()
{
	register int i;
	/*
	 * Just zero the data arrays
	 */
	bzero((char *)chuunits, sizeof chuunits);
	bzero((char *)unitinuse, sizeof unitinuse);

	/*
	 * Initialize fudge factors to default.
	 */
	for (i = 0; i < MAXUNITS; i++) {
		propagation_delay[i].l_ui = 0;
		propagation_delay[i].l_uf = DEFPROPDELAY;
		fudgefactor[i].l_ui = 0;
		fudgefactor[i].l_uf = DEFFILTFUDGE;
		offset_fudge[i] = propagation_delay[i];
		L_ADD(&offset_fudge[i], &fudgefactor[i]);
		stratumtouse[i] = 0;
		sloppyclockflag[i] = 0;
	}
}


/*
 * chu_start - open the CHU device and initialize data for processing
 */
static int
chu_start(unit, peer)
	u_int unit;
	struct peer *peer;
{
	register struct chuunit *chu;
	register int i;
	int fd232;
	char chudev[20];
	l_fp ts;

	/*
	 * Check configuration info
	 */
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "chu_start: unit %d invalid", unit);
		return 0;
	}
	if (unitinuse[unit]) {
		syslog(LOG_ERR, "chu_start: unit %d in use", unit);
		return 0;
	}

	/*
	 * Open serial port
	 */
	(void) sprintf(chudev, CHUDEV, unit);
	fd232 = open(chudev, O_RDONLY, 0777);
	if (fd232 == -1) {
		syslog(LOG_ERR, "chu_start: open of %s: %m", chudev);
		return 0;
	}

#if defined(HAVE_SYSV_TTYS)
	/*
	 * System V serial line parameters (termio interface)
	 */
	CHU SUPPORT NOT AVAILABLE IN TERMIO INTERFACE
#endif /* HAVE_SYSV_TTYS */
#if defined(STREAM)
	/*
	 * POSIX/STREAMS serial line parameters (termios interface)
	 *
	 * The CHUCLK support uses a 300-baud modem and level converter
	 * (gadget box). It requires the chu_clk streams module and
	 * SunOS 4.1.1 or later.
	 *
	 * The CHUPPS option provides timestamping at the driver level.
	 * It uses a 1-pps signal and level converter (gadget box) and
	 * requires the ppsclock streams module and SunOS 4.1.1 or
	 * later.
	 */
    {	struct termios ttyb, *ttyp;

	ttyp = &ttyb;
	if (tcgetattr(fd232, ttyp) < 0) {
                syslog(LOG_ERR,
		    "chu_start: tcgetattr(%s): %m", chudev);
                goto screwed;
        }
        ttyp->c_iflag = IGNBRK|IGNPAR;
        ttyp->c_oflag = 0;
        ttyp->c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyp->c_lflag = 0;
	ttyp->c_cc[VERASE] = ttyp->c_cc[VKILL] = '\0';
	ttyp->c_cc[VMIN] = 1;
	ttyp->c_cc[VTIME] = 0;
        if (tcsetattr(fd232, TCSANOW, ttyp) < 0) {
                syslog(LOG_ERR,
		    "chu_start: tcsetattr(%s): %m", chudev);
                goto screwed;
        }
        if (tcflush(fd232, TCIOFLUSH) < 0) {
                syslog(LOG_ERR,
		    "chu_start: tcflush(%s): %m", chudev);
                goto screwed;
        }
	while (ioctl(fd232, I_POP, 0 ) >= 0) ;
	if (ioctl(fd232, I_PUSH, "chu" ) < 0) {
		syslog(LOG_ERR,
		    "chu_start: ioctl(%s, I_PUSH, chu): %m", chudev);
		goto screwed;
	}
#if defined(CHUPPS)
	if (ioctl(fd232, I_PUSH, "ppsclock") < 0)
		syslog(LOG_ERR,
		    "chu_start: ioctl(%s, I_PUSH, ppsclock): %m", chudev);
	else
		fdpps = fd232;
#endif /* CHUPPS */
    }
#endif /* STREAM */
#if defined(HAVE_BSD_TTYS)
	/*
	 * 4.3bsd serial line parameters (sgttyb interface)
	 *
	 * The CHUCLK support uses a 300-baud modem and level converter
	 * (gadget box). It requires the chu_clk streams module and
	 * 4.3bsd or later.
	 */
    {	struct sgttyb ttyb;
	int ldisc = CHULDISC;

	if (ioctl(fd232, TIOCGETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "chu_start: ioctl(%s, TIOCGETP): %m", chudev);
		goto screwed;
	}
	ttyb.sg_ispeed = ttyb.sg_ospeed = SPEED232;
	ttyb.sg_erase = ttyb.sg_kill = '\r';
	ttyb.sg_flags = RAW;
	if (ioctl(fd232, TIOCSETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "chu_start: ioctl(%s, TIOCSETP): %m", chudev);
		goto screwed;
	}
	if (ioctl(fd232, TIOCSETD, &ldisc) < 0) {
		syslog(LOG_ERR,
		    "chu_start: ioctl(%s, TIOCSETD): %m",chudev);
		goto screwed;
	}
    }
#endif /* HAVE_BSD_TTYS */

	/*
	 * Allocate unit structure
	 */
	if (chuunits[unit] != 0) {
		chu = chuunits[unit];	/* The one we want is okay */
	} else {
		for (i = 0; i < MAXUNITS; i++) {
			if (!unitinuse[i] && chuunits[i] != 0)
				break;
		}
		if (i < MAXUNITS) {
			/*
			 * Reclaim this one
			 */
			chu = chuunits[i];
			chuunits[i] = 0;
		} else {
			chu = (struct chuunit *)emalloc(sizeof(struct chuunit));
		}
	}
	bzero((char *)chu, sizeof(struct chuunit));
	chuunits[unit] = chu;

	/*
	 * Set up the structure
	 */
	chu->peer = peer;
	chu->unit = (u_char)unit;
	chu->timestarted = current_time;

	chu->chutimer.peer = (struct peer *)chu;
	chu->chutimer.event_handler = chu_timeout;

	chu->chuio.clock_recv = chu_receive;
	chu->chuio.srcclock = (caddr_t)chu;
	chu->chuio.datalen = sizeof(struct chucode);
	chu->chuio.fd = fd232;

	/*
	 * Initialize the year from the system time in case this is the
	 * first open.
	 */
	get_systime(&ts);
	yearstart = calyearstart(ts.l_ui);
	if (!io_addclock(&chu->chuio)) {
		goto screwed;
	}

	/*
	 * All done.  Initialize a few random peer variables, then
	 * return success.
	 */
	peer->precision = CHUPRECISION;
	peer->rootdelay = 0;
	peer->rootdispersion = 0;
	peer->stratum = stratumtouse[unit];
	if (stratumtouse[unit] <= 1)
		bcopy(CHUREFID, (char *)&peer->refid, 4);
	else
		peer->refid = htonl(CHUHSREFID);
	unitinuse[unit] = 1;
	return 1;

	/*
	 * Something broke; abandon ship.
	 */
screwed:
	(void) close(fd232);
	return (0);
}


/*
 * chu_shutdown - shut down a CHU clock
 */
static void
chu_shutdown(unit)
	int unit;
{
	register struct chuunit *chu;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "chu_shutdown: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "chu_shutdown: unit %d not in use", unit);
		return;
	}

	/*
	 * Tell the I/O module to turn us off, and dequeue timer
	 * if any.  We're history.
	 */
	chu = chuunits[unit];
	if (chu->flags & CHUTIMERSET)
		TIMER_DEQUEUE(&chu->chutimer);
	io_closeclock(&chu->chuio);
	unitinuse[unit] = 0;
}

/*
 * chu_report_event - record an event and report it
 */
static void
chu_report_event(chu, code)
	struct chuunit *chu;
	int code;
{
	/*
	 * Trap support isn't up to handling this, so just
	 * record it.
	 */
	if (chu->status != (u_char)code) {
		chu->status = (u_char)code;
		if (code != CEVNT_NOMINAL)
			chu->lastevent = (u_char)code;
	}
}


/*
 * chu_receive - receive data from a CHU clock, do format checks and compute
 *		 an estimate from the sample data
 */
static void
chu_receive(rbufp)
	struct recvbuf *rbufp;
{
	register int i;
	register U_LONG date_ui;
	register U_LONG tmp;
	register u_char *code;
	register struct chuunit *chu;
	register struct chucode *chuc;
	int isneg;
	U_LONG reftime;
	l_fp off[NCHUCHARS];
	int day, hour, minute, second;

	/*
	 * Do a length check on the data.  Should be what we asked for.
	 */
	if (rbufp->recv_length != sizeof(struct chucode)) {
		syslog(LOG_ERR, "chu_receive: received %d bytes, expected %d",
		    rbufp->recv_length, sizeof(struct chucode));
		return;
	}

	/*
	 * Get the clock this applies to and a pointer to the data
	 */
	chu = (struct chuunit *)rbufp->recv_srcclock;
	chuc = (struct chucode *)&rbufp->recv_space;
	chu->responses++;
	chu->lastupdate = current_time;

	/*
	 * At this point we're assured that both halves of the
	 * data match because of what the kernel has done.
	 * But there's more than one data format. We need to
	 * check chutype to see what to do now. If it's a
	 * year packet, then we fiddle with it specially.
	 */

	if (chuc->chutype == CHU_YEAR)
	{
		u_char leapbits,parity;

		/*
	 	 * Break out the code into the BCD nibbles.
		 * Put it in the half of lastcode.
		 */
		code = chu->lastcode;
		code += 2*NCHUCHARS;
		for (i = 0; i < NCHUCHARS; i++) {
			*code++ = chuc->codechars[i] & 0xf;
			*code++ = (chuc->codechars[i] >> 4) & 0xf;
		}

		leapbits = chuc->codechars[0]&0xf;

		/*
		 * Now make sure that the leap nibble
		 * is even parity.
		 */

		parity = (leapbits ^ (leapbits >> 2))&0x3;
		parity = (parity ^ (parity>>1))&0x1;
		if (parity)
		{
			chu->badformat++;
			chu_event(chu, CEVNT_BADREPLY);
			return;
		}

		/*
		 * This just happens to work. :-)
		 */

		chu->leap = (leapbits >> 1) & 0x3;

		return;
	}

	if (chuc->chutype != CHU_TIME)
	{
		chu->badformat++;
		chu_event(chu, CEVNT_BADREPLY);
		return;
	}

	/*
	 * Break out the code into the BCD nibbles.  Only need to fiddle
	 * with the first half since both are identical.  Note the first
	 * BCD character is the low order nibble, the second the high order.
	 */
	code = chu->lastcode;
	for (i = 0; i < NCHUCHARS; i++) {
		*code++ = chuc->codechars[i] & 0xf;
		*code++ = (chuc->codechars[i] >> 4) & 0xf;
	}

	/*
	 * Format check.  Make sure the two halves match.
	 * There's really no need for this, but it can't hurt.
	 */
	for (i = 0; i < NCHUCHARS/2; i++)
		if (chuc->codechars[i] != chuc->codechars[i+(NCHUCHARS/2)]) {
			chu->badformat++;
			chu_event(chu, CEVNT_BADREPLY);
			return;
		}

	/*
	 * If the first nibble isn't a 6, we're up the creek
	 */
	code = chu->lastcode;
	if (*code++ != 6) {
		chu->badformat++;
		chu_event(chu, CEVNT_BADREPLY);
		return;
	}

	/*
	 * Collect the day, the hour, the minute and the second.
	 */
	day = *code++;
	day = MULBY10(day) + *code++;
	day = MULBY10(day) + *code++;
	hour = *code++;
	hour = MULBY10(hour) + *code++;
	minute = *code++;
	minute = MULBY10(minute) + *code++;
	second = *code++;
	second = MULBY10(second) + *code++;

	/*
	 * Sanity check the day and time.  Note that this
	 * only occurs on the 32st through the 39th second
	 * of the minute.
	 */
	if (day < 1 || day > 366
	    || hour > 23 || minute > 59
	    || second < 32 || second > 39) {
		chu->baddata++;
		if (day < 1 || day > 366) {
			chu_event(chu, CEVNT_BADDATE);
		} else {
			chu_event(chu, CEVNT_BADTIME);
		}
		return;
	}

	/*
	 * Compute the NTP date from the input data and the
	 * receive timestamp.  If this doesn't work, mark the
	 * date as bad and forget it.
	 */
	if (!clocktime(day, hour, minute, second, 0,
	    rbufp->recv_time.l_ui, &yearstart, &reftime)) {
		chu_event(chu, CEVNT_BADDATE);
		return;
	}
	date_ui = reftime;;

	/*
	 * We've now got the integral seconds part of the time code (we hope).
	 * The fractional part comes from the table.  We next compute
	 * the offsets for each character.
	 */
	for (i = 0; i < NCHUCHARS; i++) {
		register U_LONG tmp2;

		off[i].l_ui = date_ui;
		off[i].l_uf = chutable[i];
		tmp = chuc->codetimes[i].tv_sec + JAN_1970;
		TVUTOTSF(chuc->codetimes[i].tv_usec, tmp2);
		M_SUB(off[i].l_ui, off[i].l_uf, tmp, tmp2);
	}

	if (!sloppyclockflag[chu->unit]) {
		u_short ord[NCHUCHARS];
		/*
		 * In here we assume the clock has adequate bits
		 * to take timestamps with reasonable accuracy.
		 * Note that the time stamps may contain errors
		 * for a couple of reasons.  Timing is actually
		 * referenced to the start bit in each character
		 * in the time code.  If this is obscured by static
		 * you can still get a valid character but have the
		 * timestamp offset by +-1.5 ms.  Also, we may suffer
		 * from interrupt delays if the interrupt is being
		 * held off when the character arrives.  Note the
		 * latter error is always in the form of a delay.
		 *
		 * After fiddling I arrived at the following scheme.
		 * We sort the times into order by offset.  We then
		 * drop the most positive 2 offset values (which may
		 * correspond to a character arriving early due to
		 * static) and the most negative 4 (which may correspond
		 * to delayed characters, either from static or from
		 * interrupt latency).  We then take the mean of the
		 * remaining 4 offsets as our estimate.
		 */
		
		/*
		 * Set up the order array.
		 */
		for (i = 0; i < NCHUCHARS; i++)
			ord[i] = (u_short)i;
		
		/*
		 * Sort them into order.  Reuse variables with abandon.
		 */
		for (tmp = 0; tmp < (NCHUCHARS-1); tmp++) {
			for (i = (int)tmp+1; i < NCHUCHARS; i++) {
				if (!L_ISGEQ(&off[ord[i]], &off[ord[tmp]])) {
					date_ui = (U_LONG)ord[i];
					ord[i] = ord[tmp];
					ord[tmp] = (u_short)date_ui;
				}
			}
		}

		/*
		 * Done the sort.  We drop 0, 1, 2 and 3 at the negative
		 * end, and 8 and 9 at the positive.  Take the sum of
		 * 4, 5, 6 and 7.
		 */
		date_ui = off[ord[4]].l_ui;
		tmp = off[ord[4]].l_uf;
		for (i = 5; i <= 7; i++)
			M_ADD(date_ui, tmp, off[ord[i]].l_ui, off[ord[i]].l_uf);
		
		/*
		 * Round properly, then right shift two bits for the
		 * divide by four.
		 */
		if (tmp & 0x2)
			M_ADDUF(date_ui, tmp, 0x4);
		M_RSHIFT(date_ui, tmp);
		M_RSHIFT(date_ui, tmp);
	} else {
		/*
		 * Here is a *big* problem.  On a machine where the
		 * low order bit in the clock is on the order of half
		 * a millisecond or more we don't really have enough
		 * precision to make intelligent choices about which
		 * samples might be in error and which aren't.  More
		 * than this, in the case of error free data we can
		 * pick up a few bits of precision by taking the mean
		 * of the whole bunch.  This is what we do.  The problem
		 * comes when it comes time to divide the 64 bit sum of
		 * the 10 samples by 10, a procedure which really sucks.
		 * Oh, well, grin and bear it.  Compute the sum first.
		 */
		date_ui = 0;
		tmp = 0;
		for (i = 0; i < NCHUCHARS; i++)
			M_ADD(date_ui, tmp, off[i].l_ui, off[i].l_uf);
		if (M_ISNEG(date_ui, tmp))
			isneg = 1;
		else
			isneg = 0;

		/*
		 * Here is a multiply-by-0.1 optimization that should apply
		 * just about everywhere.  If the magnitude of the sum
		 * is less than 9 we don't have to worry about overflow
		 * out of a 64 bit product, even after rounding.
		 */
		if (date_ui < 9 || date_ui > 0xfffffff7) {
			register U_LONG prod_ui;
			register U_LONG prod_uf;
	
			prod_ui = prod_uf = 0;
			/*
			 * This code knows the low order bit in 0.1 is zero
			 */
			for (i = 1; i < NZPOBITS; i++) {
				M_LSHIFT(date_ui, tmp);
				if (ZEROPTONE & (1<<i))
					M_ADD(prod_ui, prod_uf, date_ui, tmp);
			}

			/*
			 * Done, round it correctly.  Prod_ui contains the
			 * fraction.
			 */
			if (prod_uf & 0x80000000)
				prod_ui++;
			if (isneg)
				date_ui = 0xffffffff;
			else
				date_ui = 0;
			tmp = prod_ui;
			/*
			 * date_ui is integral part, tmp is fraction.
			 */
		} else {
			register U_LONG prod_ovr;
			register U_LONG prod_ui;
			register U_LONG prod_uf;
			register U_LONG highbits;

			prod_ovr = prod_ui = prod_uf = 0;
			if (isneg)
				highbits = 0xffffffff;	/* sign extend */
			else
				highbits = 0;
			/*
			 * This code knows the low order bit in 0.1 is zero
			 */
			for (i = 1; i < NZPOBITS; i++) {
				M_LSHIFT3(highbits, date_ui, tmp);
				if (ZEROPTONE & (1<<i))
					M_ADD3(prod_ovr, prod_uf, prod_ui,
					    highbits, date_ui, tmp);
			}

			if (prod_uf & 0x80000000)
				M_ADDUF(prod_ovr, prod_ui, (U_LONG)1);
			date_ui = prod_ovr;
			tmp = prod_ui;
		}
	}
	
	/*
	 * At this point we have the mean offset, with the integral
	 * part in date_ui and the fractional part in tmp.  Store
	 * it in the structure.
	 */
	i = second - 32;	/* gives a value 0 through 8 */
	if (i < (int)chu->expect) {
		/*
		 * This shouldn't actually happen, but might if a single
		 * bit error occurred in the code which fooled us.
		 * Throw away all previous data.
		 */
		chu->expect = 0;
		chu->haveoffset = 0;
		if (chu->flags & CHUTIMERSET) {
			TIMER_DEQUEUE(&chu->chutimer);
			chu->flags &= ~CHUTIMERSET;
		}
	}

	/*
	 * Add in fudge factor.
	 */
	M_ADD(date_ui, tmp, offset_fudge[chu->unit].l_ui,
	    offset_fudge[chu->unit].l_uf);

	chu->offsets[i].l_ui = date_ui;
	chu->offsets[i].l_uf = tmp;
	chu->rectimes[i] = rbufp->recv_time;
	chu->reftimes[i] = reftime;

	chu->expect = i + 1;
	chu->haveoffset |= (1<<i);

	if (chu->expect >= NCHUCODES) {
		/*
		 * Got a full second's worth.  Dequeue timer and
		 * process this.
		 */
		if (chu->flags & CHUTIMERSET) {
			TIMER_DEQUEUE(&chu->chutimer);
			chu->flags &= ~CHUTIMERSET;
		}
		chu_process(chu);
	} else if (!(chu->flags & CHUTIMERSET)) {
		/*
		 * Try to take an interrupt sometime after the
		 * 42 second mark (leaves an extra 2 seconds for
		 * slop).  Round it up to an even multiple of
		 * 4 seconds.
		 */
		chu->chutimer.event_time =
		    current_time + (U_LONG)(10 - i) + (1<<EVENT_TIMEOUT);
		chu->chutimer.event_time &= ~((1<<EVENT_TIMEOUT) - 1);
		TIMER_INSERT(timerqueue, &chu->chutimer);
		chu->flags |= CHUTIMERSET;
	}
}


/*
 * chu_timeout - process a timeout event
 */
static void
chu_timeout(fakepeer)
	struct peer *fakepeer;
{
	/*
	 * If we got here it means we received some time codes
	 * but didn't get the one which should have arrived on
	 * the 39th second.  Process what we have.
	 */
	((struct chuunit *)fakepeer)->flags &= ~CHUTIMERSET;
	chu_process((struct chuunit *)fakepeer);
}


/*
 * chu_process - process the raw offset estimates we have and pass
 *		 the results on to the NTP clock filters.
 */
static void
chu_process(chu)
	register struct chuunit *chu;
{
	register int i;
	register s_fp bestoff;
	register s_fp tmpoff;
	u_fp dispersion;
	int imax;
	l_fp ts;

	/*
	 * The most positive offset.
	 */
	imax = NCHUCODES;
	for (i = 0; i < NCHUCODES; i++)
		if (chu->haveoffset & (1<<i))
			if (i < imax || L_ISGEQ(&chu->offsets[i],
			    &chu->offsets[imax]))
				imax = i;

	/*
	 * The most positive estimate is our best bet.  Go through
	 * the list again computing the dispersion.
	 */
	bestoff = LFPTOFP(&chu->offsets[imax]);
	dispersion = 0;
	for (i = 0; i < NCHUCODES; i++) {
		if (chu->haveoffset & (1<<i)) {
			tmpoff = LFPTOFP(&chu->offsets[i]);
			dispersion += (bestoff - tmpoff);
		} else {
			dispersion += CHUDELAYPENALTY;
		}
	}

	/*
	 * Make up a reference time stamp, then give it to the
	 * reference clock support code for further processing.
	 */
	ts.l_ui = chu->reftimes[imax];
	ts.l_uf = chutable[NCHUCHARS-1];

	for (i = 0; i < NCHUCHARS*4; i++) {
		chu->asciicode[i] = hexstring[chu->lastcode[i]];
	}
	chu->asciicode[i] = '\0';
	record_clock_stats(&(chu->peer->srcadr), chu->asciicode);
	refclock_receive(chu->peer, &chu->offsets[imax], 0,
	    dispersion, &ts, &chu->rectimes[imax], chu->leap);
	
	/*
	 * Zero out unit for next code series
	 */
	chu->haveoffset = 0;
	chu->expect = 0;
	chu_event(chu, CEVNT_NOMINAL);
}


/*
 * chu_poll - called by the transmit procedure
 */
static void
chu_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "chu_poll: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "chu_poll: unit %d not in use", unit);
		return;
	}

	if ((current_time - chuunits[unit]->lastupdate) > 150) {
		chu_event(chuunits[unit], CEVNT_PROP);
	}
}



/*
 * chu_control - set fudge factors, return statistics
 */
static void
chu_control(unit, in, out)
	u_int unit;
	struct refclockstat *in;
	struct refclockstat *out;
{
	register struct chuunit *chu;
	U_LONG npolls;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "chu_control: unit %d invalid", unit);
		return;
	}

	if (in != 0) {
		if (in->haveflags & CLK_HAVETIME1)
			propagation_delay[unit] = in->fudgetime1;
		if (in->haveflags & CLK_HAVETIME2)
			fudgefactor[unit] = in->fudgetime2;
		offset_fudge[unit] = propagation_delay[unit];
		L_ADD(&offset_fudge[unit], &fudgefactor[unit]);
		if (in->haveflags & CLK_HAVEVAL1) {
			stratumtouse[unit] = (u_char)(in->fudgeval1 & 0xf);
			if (unitinuse[unit]) {
				struct peer *peer;

				/*
				 * Should actually reselect clock, but
				 * will wait for the next timecode
				 */
				peer = chuunits[unit]->peer;
				peer->stratum = stratumtouse[unit];
				if (stratumtouse[unit] <= 1)
					bcopy(CHUREFID, (char *)&peer->refid,4);
				else
					peer->refid = htonl(CHUHSREFID);
			}
		}
		if (in->haveflags & CLK_HAVEFLAG1) {
			sloppyclockflag[unit] = in->flags & CLK_FLAG1;
		}
	}

	if (out != 0) {
		out->type = REFCLK_CHU;
		out->flags = 0;
		out->haveflags
		    = CLK_HAVETIME1|CLK_HAVETIME2|CLK_HAVEVAL1|
		      CLK_HAVEVAL2|CLK_HAVEFLAG1;
		out->clockdesc = CHUDESCRIPTION;
		out->fudgetime1 = propagation_delay[unit];
		out->fudgetime2 = fudgefactor[unit];
		out->fudgeval1 = (long)stratumtouse[unit];
		out->flags = sloppyclockflag[unit];
		if (unitinuse[unit]) {
			chu = chuunits[unit];
			out->lencode = NCHUCHARS*4;
			out->fudgeval2 = chu->lastcode[2*NCHUCHARS+1];
			out->fudgeval2 *= (chu->lastcode[2*NCHUCHARS]&1)?-1:1;
			out->lastcode = chu->asciicode;
			out->timereset = current_time - chu->timestarted;
			npolls = out->timereset / 6;	/* **divide** */
			out->polls = npolls;
			out->noresponse = (npolls - chu->responses);
			out->badformat = chu->badformat;
			out->baddata = chu->baddata;
			out->lastevent = chu->lastevent;
			out->currentstatus = chu->status;
		} else {
			out->fudgeval2 = 0;
			out->lencode = 0;
			out->lastcode = "";
			out->polls = out->noresponse = 0;
			out->badformat = out->baddata = 0;
			out->timereset = 0;
			out->currentstatus = out->lastevent = CEVNT_NOMINAL;
		}
	}
}
#endif
