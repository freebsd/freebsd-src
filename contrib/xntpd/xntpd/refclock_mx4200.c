/*
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66.
 *
 * Copyright (c) 1992 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 * 4. The name of the University may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(REFCLOCK) && (defined(MX4200) || defined(MX4200CLK) || defined(MX4200PPS))

#if	!defined(lint) && !defined(__GNUC__)
static char rcsid[] =
    "@(#) /src/master/xntp-930612/xntpd/refclock_mx4200.c,v 1.5 1993/06/18 21:19:54 jbj Exp (LBL) ";
#endif

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_calendar.h"
#include "ntp_unixtime.h"

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
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
#if defined(MX4200CLK)
#include <sys/clkdefs.h>
#endif /* MX4200CLK */
#endif /* STREAM */

#include <sys/ppsclock.h>

#include "mx4200.h"
#include "ntp_stdlib.h"

/*
 * This driver supports the Magnavox Model MX4200 GPS Receiver.
 */

/*
 * Definitions
 */
#define	MAXUNITS	2		/* max number of mx4200 units */
#define	MX4200232	"/dev/gps%d"
#define	SPEED232	B4800		/* baud */

/*
 * The number of raw samples which we acquire to derive a single estimate.
 */
#define	NSTAMPS	64

/*
 * Radio interface parameters
 */
#define	MX4200PRECISION	(-18)	/* precision assumed (about 4 us) */
#define	MX4200REFID	"GPS"	/* reference id */
#define	MX4200DESCRIPTION	"Magnavox MX4200 GPS Receiver" /* who we are */
#define	MX4200HSREFID	0x7f7f0a0a /* 127.127.10.10 refid for hi strata */
#define	DEFFUDGETIME	0	/* default fudge time (ms) */

/* Leap stuff */
extern U_LONG leap_hoursfromleap;
extern U_LONG leap_happened;
static int leap_debug;

/*
 * mx4200_reset - reset the count back to zero
 */
#define	mx4200_reset(mx4200) \
	do { \
		(mx4200)->nsamples = 0; \
	} while (0)

/*
 * mx4200_event - record and report an event
 */
#define	mx4200_event(mx4200, evcode) \
	do { \
		if ((mx4200)->status != (u_char)(evcode)) \
			mx4200_report_event((mx4200), (evcode)); \
	} while (0)

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
 * MX4200 unit control structure.
 */
struct mx4200unit {
	struct peer *peer;		/* associated peer structure */
	struct refclockio io;		/* given to the I/O handler */
	U_LONG gpssamples[NSTAMPS];	/* the GPS time samples */
	l_fp unixsamples[NSTAMPS];	/* the UNIX time samples */


	l_fp lastsampletime;		/* time of last estimate */
	u_int lastserial;		/* last pps serial number */
#ifdef notdef
	l_fp lastrec;			/* last receive time */
	l_fp lastref;			/* last timecode time */
#endif
	char lastcode[RX_BUFF_SIZE];	/* last timecode received */
	U_LONG lasttime;		/* last time clock heard from */
	u_char nsamples;		/* number of samples we've collected */
	u_char unit;			/* unit number for this guy */
	u_char status;			/* clock status */
	u_char lastevent;		/* last clock event */
	u_char reason;			/* reason for last abort */
	u_char lencode;			/* length of last timecode */
	u_char year;			/* year of eternity */
	u_short monthday;		/* day of month */
	u_char hour;			/* hour of day */
	u_char minute;			/* minute of hour */
	u_char second;			/* seconds of minute */
	u_char leap;			/* leap indicators */
	/*
	 * Status tallies
	 */
#ifdef notdef
	U_LONG polls;			/* polls sent */
	U_LONG noresponse;		/* number of nonresponses */
#endif
	U_LONG badformat;		/* bad format */
	U_LONG baddata;			/* bad data */
	U_LONG timestarted;		/* time we started this */
};

/*
 * We demand that consecutive PPS samples are more than 0.995 seconds
 * and less than 1.005 seconds apart.
 */
#define	PPSLODIFF_UI	0		/* 0.900 as an l_fp */
#define	PPSLODIFF_UF	0xe6666610

#define	PPSHIDIFF_UI	1		/* 1.100 as an l_fp */
#define	PPSHIDIFF_UF	0x19999990

/*
 * reason codes
 */
#define	PPSREASON	20
#define	CODEREASON	40
#define	PROCREASON	60

/*
 * Data space for the unit structures.  Note that we allocate these on
 * the fly, but never give them back.
 */
static struct mx4200unit *mx4200units[MAXUNITS];
static u_char unitinuse[MAXUNITS];

/*
 * Keep the fudge factors separately so they can be set even
 * when no clock is configured.
 */
static	l_fp fudgefactor[MAXUNITS];
static	u_char stratumtouse[MAXUNITS];
static	u_char sloppyclockflag[MAXUNITS];

static const char pmvxg[] = "PMVXG";

/*
 * Function prototypes
 */
static	void	mx4200_init	P((void));
static	int	mx4200_start	P((u_int, struct peer *));
static	void	mx4200_shutdown	P((int));
static	void	mx4200_receive	P((struct recvbuf *));
static	void	mx4200_process	P((struct mx4200unit *));
static	void	mx4200_report_event	P((struct mx4200unit *, int));
static	void	mx4200_poll	P((int, struct peer *));
static	void	mx4200_control	P((u_int, struct refclockstat *, struct refclockstat *));
static	void	mx4200_buginfo	P((int, struct refclockbug *));

static	char *	mx4200_parse	P((char *, struct calendar *, int *, int *));
static	int	mx4200_needconf	P((char *));
static	void	mx4200_config	P((struct mx4200unit *));
static	void	mx4200_send	P((int, const char *, ...));
static	int	mx4200_cmpl_fp	P((void *, void *));
static	u_char	cksum		P((char *, u_int));

#ifdef	DEBUG
static	void	opendfile	P((int));
static	void	checkdfile	P((void));
#endif	/* DEBUG */

/*
 * Transfer vector
 */
struct	refclock refclock_mx4200 = {
	mx4200_start, mx4200_shutdown, mx4200_poll,
	mx4200_control, mx4200_init, mx4200_buginfo, NOFLAGS
};

/*
 * mx4200_init - initialize internal mx4200 driver data
 */
static void
mx4200_init()
{
	register int i;
	/*
	 * Just zero the data arrays
	 */
	memset((char *)mx4200units, 0, sizeof mx4200units);
	memset((char *)unitinuse, 0, sizeof unitinuse);

	/*
	 * Initialize fudge factors to default.
	 */
	for (i = 0; i < MAXUNITS; i++) {
		fudgefactor[i].l_ui = 0;
		fudgefactor[i].l_uf = DEFFUDGETIME;
		stratumtouse[i] = 0;
		sloppyclockflag[i] = 0;
	}
}

#ifdef DEBUG
static char dfile[] = "/var/tmp/MX4200.debug";
static FILE *df = NULL;

static void
opendfile(create)
	int create;
{
	if (!create && access(dfile, F_OK) < 0) {
		syslog(LOG_ERR, "mx4200: open %s: %m", dfile);
		return;
	}
	df = fopen(dfile, "a");
	if (df == NULL)
		syslog(LOG_ERR, "mx4200: open %s: %m", dfile);
	else if (setvbuf(df, NULL, _IOLBF, 0) < 0)
		syslog(LOG_ERR, "mx4200: setvbuf %s: %m", dfile);
}

static void
checkdfile()
{

	if (df == NULL)
		return;

	if (access(dfile, F_OK) < 0) {
		fclose(df);
		opendfile(1);
	}
}

#endif


/*
 * mx4200_start - open the MX4200 devices and initialize data for processing
 */
static int
mx4200_start(unit, peer)
	u_int unit;
	struct peer *peer;
{
	register struct mx4200unit *mx4200;
	register int i;
	int fd232;
	char mx4200dev[20];

	/*
	 * Check configuration info
	 */
	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "mx4200_start: unit %d invalid", unit);
		return (0);
	}
	if (unitinuse[unit]) {
		syslog(LOG_ERR, "mx4200_start: unit %d in use", unit);
		return (0);
	}

	/*
	 * Open serial port
	 */
	(void) sprintf(mx4200dev, MX4200232, unit);
	fd232 = open(mx4200dev, O_RDWR, 0777);
	if (fd232 == -1) {
		syslog(LOG_ERR,
		    "mx4200_start: open of %s: %m", mx4200dev);
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
		    "mx4200_start: ioctl(%s, TCGETA): %m", mx4200dev);
                goto screwed;
        }
        ttyb.c_iflag = IGNBRK|IGNPAR|ICRNL;
        ttyb.c_oflag = 0;
        ttyb.c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyb.c_lflag = ICANON;
	ttyb.c_cc[VERASE] = ttyb.c_cc[VKILL] = '\0';
        if (ioctl(fd232, TCSETA, &ttyb) < 0) {
                syslog(LOG_ERR,
		    "mx4200_start: ioctl(%s, TCSETA): %m", mx4200dev);
                goto screwed;
        }
    }
#endif /* HAVE_SYSV_TTYS */
#if defined(HAVE_TERMIOS)
	/*
	 * POSIX serial line parameters (termios interface)
	 *
	 * The MX4200CLK option provides timestamping at the driver level. 
	 * It requires the tty_clk streams module.
	 *
	 * The MX4200PPS option provides timestamping at the driver level.
	 * It uses a 1-pps signal and level converter (gadget box) and
	 * requires the ppsclock streams module and SunOS 4.1.1 or
	 * later.
	 */
    {	struct termios ttyb, *ttyp;

	ttyp = &ttyb;
	if (tcgetattr(fd232, ttyp) < 0) {
                syslog(LOG_ERR,
		    "mx4200_start: tcgetattr(%s): %m", mx4200dev);
                goto screwed;
        }
        ttyp->c_iflag = IGNBRK|IGNPAR|ICRNL;
        ttyp->c_oflag = 0;
        ttyp->c_cflag = SPEED232|CS8|CLOCAL|CREAD;
        ttyp->c_lflag = ICANON;
	ttyp->c_cc[VERASE] = ttyp->c_cc[VKILL] = '\0';
        if (tcsetattr(fd232, TCSANOW, ttyp) < 0) {
                syslog(LOG_ERR,
		    "mx4200_start: tcsetattr(%s): %m", mx4200dev);
                goto screwed;
        }
        if (tcflush(fd232, TCIOFLUSH) < 0) {
                syslog(LOG_ERR,
		    "mx4200_start: tcflush(%s): %m", mx4200dev);
                goto screwed;
        }
    }
#endif /* HAVE_TERMIOS */
#ifdef STREAM
#if defined(MX4200CLK)
    if (ioctl(fd232, I_PUSH, "clk") < 0)
	    syslog(LOG_ERR,
		"mx4200_start: ioctl(%s, I_PUSH, clk): %m", mx4200dev);
    if (ioctl(fd232, CLK_SETSTR, "\n") < 0)
	    syslog(LOG_ERR,
		"mx4200_start: ioctl(%s, CLK_SETSTR): %m", mx4200dev);
#endif /* MX4200CLK */
#if defined(MX4200PPS)
    if (ioctl(fd232, I_PUSH, "ppsclock") < 0)
	    syslog(LOG_ERR,
		"mx4200_start: ioctl(%s, I_PUSH, ppsclock): %m", mx4200dev);
    else
	    fdpps = fd232;
#endif /* MX4200PPS */
#endif /* STREAM */
#if defined(HAVE_BSD_TTYS)
	/*
	 * 4.3bsd serial line parameters (sgttyb interface)
	 *
	 * The MX4200CLK option provides timestamping at the driver level. 
	 * It requires the tty_clk line discipline and 4.3bsd or later.
	 */
    {	struct sgttyb ttyb;
#if defined(MX4200CLK)
	int ldisc = CLKLDISC;
#endif /* MX4200CLK */

	if (ioctl(fd232, TIOCGETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "mx4200_start: ioctl(%s, TIOCGETP): %m", mx4200dev);
		goto screwed;
	}
	ttyb.sg_ispeed = ttyb.sg_ospeed = SPEED232;
#if defined(MX4200CLK)
	ttyb.sg_erase = ttyb.sg_kill = '\r';
	ttyb.sg_flags = RAW;
#else
	ttyb.sg_erase = ttyb.sg_kill = '\0';
	ttyb.sg_flags = EVENP|ODDP|CRMOD;
#endif /* MX4200CLK */
	if (ioctl(fd232, TIOCSETP, &ttyb) < 0) {
		syslog(LOG_ERR,
		    "mx4200_start: ioctl(%s, TIOCSETP): %m", mx4200dev);
		goto screwed;
	}
#if defined(MX4200CLK)
	if (ioctl(fd232, TIOCSETD, &ldisc) < 0) {
		syslog(LOG_ERR,
		    "mx4200_start: ioctl(%s, TIOCSETD): %m",mx4200dev);
		goto screwed;
	}
#endif /* MX4200CLK */
    }
#endif /* HAVE_BSD_TTYS */

	/*
	 * Allocate unit structure
	 */
	if (mx4200units[unit] != 0) {
		mx4200 = mx4200units[unit];	/* The one we want is okay */
	} else {
		for (i = 0; i < MAXUNITS; i++) {
			if (!unitinuse[i] && mx4200units[i] != 0)
				break;
		}
		if (i < MAXUNITS) {
			/*
			 * Reclaim this one
			 */
			mx4200 = mx4200units[i];
			mx4200units[i] = 0;
		} else {
			mx4200 = (struct mx4200unit *)
			    emalloc(sizeof(struct mx4200unit));
		}
	}

	memset((char *)mx4200, 0, sizeof(struct mx4200unit));
	mx4200units[unit] = mx4200;

	/*
	 * Set up the structures
	 */
	mx4200->peer = peer;
	mx4200->unit = (u_char)unit;
	mx4200->timestarted = current_time;

	mx4200->io.clock_recv = mx4200_receive;
	mx4200->io.srcclock = (caddr_t)mx4200;
	mx4200->io.datalen = 0;
	mx4200->io.fd = fd232;
	if (!io_addclock(&mx4200->io))
		goto screwed;

	/*
	 * All done.  Initialize a few random peer variables, then
	 * return success.
	 */
	peer->precision = MX4200PRECISION;
	peer->rootdelay = 0;
	peer->rootdispersion = 0;
	peer->stratum = stratumtouse[unit];
	if (stratumtouse[unit] <= 1)
		memmove((char *)&peer->refid, MX4200REFID, 4);
	else
		peer->refid = htonl(MX4200HSREFID);
	unitinuse[unit] = 1;

	/* Insure the receiver is properly configured */
	mx4200_config(mx4200);

#ifdef DEBUG
	opendfile(0);
#endif
	return (1);

	/*
	 * Something broke; abandon ship
	 */
screwed:
	(void) close(fd232);
	return (0);
}

/*
 * mx4200_shutdown - shut down a MX4200 clock
 */
static void
mx4200_shutdown(unit)
	int unit;
{
	register struct mx4200unit *mx4200;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "mx4200_shutdown: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "mx4200_shutdown: unit %d not in use", unit);
		return;
	}

	/*
	 * Tell the I/O module to turn us off.  We're history.
	 */
	mx4200 = mx4200units[unit];
	io_closeclock(&mx4200->io);
	unitinuse[unit] = 0;
}

static void
mx4200_config(mx4200)
	register struct mx4200unit *mx4200;
{
	register int fd = mx4200->io.fd;

syslog(LOG_DEBUG, "mx4200_config");

	/* Zero the output list (do it twice to flush possible junk) */
	mx4200_send(fd, "%s,%03d,,%d,,,,,,", pmvxg, PMVXG_S_PORTCONF, 1);
	mx4200_send(fd, "%s,%03d,,%d,,,,,,", pmvxg, PMVXG_S_PORTCONF, 1);

	/* Switch to 2d mode */
	mx4200_send(fd, "%s,%03d,%d,,%.1f,%.1f,,%d,%d,%c,%d",
	    pmvxg, PMVXG_S_INITMODEB,
	    2,		/* 2d mode */
	    0.1,	/* hor accel fact as per Steve */
	    0.1,	/* ver accel fact as per Steve */
	    10,		/* hdop limit as per Steve */
	    5,		/* elevation limit as per Steve */
	    'U',	/* time output mode */
	    0);		/* local time offset from gmt */

	/* Configure time recovery */
	mx4200_send(fd, "%s,%03d,%c,%c,%c,%d,%d,%d,",
	    pmvxg, PMVXG_S_TRECOVCONF,
#ifdef notdef
	    'K',	/* known position */
	    'D',	/* dynamic position */
#else
	    'S',	/* static position */
#endif
	    'U',	/* steer clock to gps time */
	    'A',	/* always output time pulse */
	    500,	/* max time error in ns */
	    0,		/* user bias in ns */
	    1);		/* output to control port */
}

/*
 * mx4200_report_event - note the occurrence of an event
 */
static void
mx4200_report_event(mx4200, code)
	struct mx4200unit *mx4200;
	int code;
{
	struct peer *peer;

	peer = mx4200->peer;
	if (mx4200->status != (u_char)code) {
		mx4200->status = (u_char)code;
		if (code != CEVNT_NOMINAL)
			mx4200->lastevent = (u_char)code;
		syslog(LOG_INFO,
		    "mx4200 clock %s event %x", ntoa(&peer->srcadr), code);
	}
}

/*
 * mx4200_poll - mx4200 watchdog routine
 */
static void
mx4200_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct mx4200unit *mx4200;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "mx4200_poll: unit %d invalid", unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "mx4200_poll: unit %d not used", unit);
		return;
	}

	mx4200 = mx4200units[unit];
	if ((current_time - mx4200->lasttime) > 150) {
		mx4200_event(mx4200, CEVNT_FAULT);

		/* Request a status message which should trigger a reconfig */
		mx4200_send(mx4200->io.fd, "%s,%03d", "CDGPQ", PMVXG_D_STATUS);
		syslog(LOG_DEBUG, "mx4200_poll: request status");
	}
}

static const char char2hex[] = "0123456789ABCDEF";

/*
 * mx4200_receive - receive gps data
 */
static void
mx4200_receive(rbufp)
	struct recvbuf *rbufp;
{
	register struct mx4200unit *mx4200;
	register char *dpt, *cp;
	register U_LONG tmp_ui;
	register U_LONG tmp_uf;
	register U_LONG gpstime;
	struct ppsclockev ev;
	register struct calendar *jt;
	struct calendar sjt;
	register int n;
	int valid, leapsec;
	register u_char ck;

	mx4200 = (struct mx4200unit *)rbufp->recv_srcclock;

#ifdef DEBUG
	if (debug > 3)
		printf("mx4200_receive: nsamples = %d\n", mx4200->nsamples);
#endif

	/* Record the time of this event */
	mx4200->lasttime = current_time;

	/* Get the pps value */
	if (ioctl(mx4200->io.fd, CIOGETEV, (char *)&ev) < 0) {
		/* XXX Actually, if this fails, we're pretty much screwed */
#ifdef DEBUG
		if (debug) {
			fprintf(stderr, "mx4200_receive: ");
			perror("CIOGETEV");
		}
#endif
		mx4200->reason = PPSREASON + 1;
		mx4200_event(mx4200, CEVNT_FAULT);
		mx4200_reset(mx4200);
		return;
	}
	tmp_ui = ev.tv.tv_sec + (U_LONG)JAN_1970;
	TVUTOTSF(ev.tv.tv_usec, tmp_uf);

	/* Get buffer and length; sock away last timecode */
	n = rbufp->recv_length;
	dpt = rbufp->recv_buffer;
	if (n <= 1)
		return;
	mx4200->lencode = n;
	memmove(mx4200->lastcode, dpt, n);

	/*
	 * We expect to see something like:
	 *
	 *    $PMVXG,830,T,1992,07,09,04:18:34,U,S,-02154,00019,000000,00*1D\n
	 *
	 * Reject if any important landmarks are missing.
	 */
	cp = dpt + n - 4;
	if (cp < dpt || *dpt != '$' || cp[0] != '*' || cp[3] != '\n') {
#ifdef DEBUG
		if (debug)
			printf("mx4200_receive: bad format\n");
#endif
		mx4200->badformat++;
		mx4200->reason = PPSREASON + 2;
		mx4200_event(mx4200, CEVNT_BADREPLY);
		mx4200_reset(mx4200);
		return;
	}

	/* Check checksum */
	ck = cksum(&dpt[1], n - 5);
	if (char2hex[ck >> 4] != cp[1] || char2hex[ck & 0xf] != cp[2]) {
#ifdef DEBUG
		if (debug)
			printf("mx4200_receive: bad checksum\n");
#endif
		mx4200->badformat++;
		mx4200->reason = PPSREASON + 3;
		mx4200_event(mx4200, CEVNT_BADREPLY);
		mx4200_reset(mx4200);
		return;
	}

	/* Truncate checksum (and the buffer for that matter) */
	*cp = '\0';

	/* Leap second debugging stuff */
	if ((leap_hoursfromleap && !leap_happened) || leap_debug > 0) {
		/* generate reports for awhile after leap */
		if (leap_hoursfromleap && !leap_happened)
			leap_debug = 3600;
		else
			--leap_debug;
		syslog(LOG_INFO, "mx4200 leap: %s \"%s\"",
		    umfptoa(tmp_ui, tmp_uf, 6), dpt);
	}

	/* Parse time recovery message */
	jt = &sjt;
	if ((cp = mx4200_parse(dpt, jt, &valid, &leapsec)) != NULL) {
		/* Configure the receiver if necessary */
		if (mx4200_needconf(dpt))
			mx4200_config(mx4200);
#ifdef DEBUG
		if (debug)
			printf("mx4200_receive: mx4200_parse: %s\n", cp);
#endif
		mx4200->badformat++;
		mx4200->reason = PPSREASON + 5;
		mx4200_event(mx4200, CEVNT_BADREPLY);
		mx4200_reset(mx4200);
		return;
	}

	/* Setup leap second indicator */
	if (leapsec == 0)
		mx4200->leap = LEAP_NOWARNING;
	else if (leapsec == 1)
		mx4200->leap = LEAP_ADDSECOND;
	else if (leapsec == -1)
		mx4200->leap = LEAP_DELSECOND;
	else
		mx4200->leap = LEAP_NOTINSYNC;		/* shouldn't happen */

	/* Check parsed time (allow for possible leap seconds) */
	if (jt->second >= 61 || jt->minute >= 60 || jt->hour >= 24) {
#ifdef DEBUG
		if (debug) {
			printf("mx4200_receive: bad time %d:%02d:%02d",
			    jt->hour, jt->minute, jt->second);
			if (leapsec != 0)
				printf(" (leap %+d)", leapsec);
			putchar('\n');
		}
#endif
		mx4200->baddata++;
		mx4200->reason = PPSREASON + 6;
		mx4200_event(mx4200, CEVNT_BADTIME);
		mx4200_reset(mx4200);
		/* Eat the next pulse which the clock claims will be bad */
		mx4200->nsamples = -1;
		return;
	}

	/* Check parsed date */
	if (jt->monthday > 31 || jt->month > 12 || jt->year < 1900) {
#ifdef DEBUG
		if (debug)
			printf("mx4200_receive: bad date (%d/%d/%d)\n",
			    jt->monthday, jt->month, jt->year);
#endif
		mx4200->baddata++;
		mx4200->reason = PPSREASON + 7;
		mx4200_event(mx4200, CEVNT_BADDATE);
		mx4200_reset(mx4200);
		return;
	}

	/* Convert to ntp time */
	gpstime = caltontp(jt);

	/* The gps message describes the *next* pulse; pretend it's this one */
	--gpstime;

	/* Debugging */
#ifdef DEBUG
	checkdfile();
	if (df != NULL) {
		l_fp t;

		t.l_ui = gpstime;
		t.l_uf = 0;
		M_SUB(t.l_ui, t.l_uf, tmp_ui, tmp_uf);
		fprintf(df, "%s\t%s",
		    umfptoa(tmp_ui, tmp_uf, 6), mfptoa(t.l_ui, t.l_uf, 6));
		if (debug > 3)
			fprintf(df, "\t(gps: %lu)", gpstime);
		if (leapsec != 0)
			fprintf(df, "\t(leap sec %+d)", leapsec);
		if (!valid)
			fprintf(df, "\t(pulse not valid)");
		fputc('\n', df);
	}
#endif

	/* Check pps serial number against last one */
	if (mx4200->lastserial + 1 != ev.serial && mx4200->lastserial != 0) {
#ifdef DEBUG
		if (debug) {
			if (ev.serial == mx4200->lastserial)
				printf("mx4200_receive: no new pps event\n");
			else
				printf("mx4200_receive: missed %d pps events\n",
				    ev.serial - mx4200->lastserial - 1);
		}
#endif
		mx4200->reason = PPSREASON + 8;
		mx4200_event(mx4200, CEVNT_FAULT);
		mx4200_reset(mx4200);
		/* fall through and this one collect as first sample */
	}
	mx4200->lastserial = ev.serial;

/*
 * XXX
 * Since this message is for the next pulse, it's really the next pulse
 * that the clock might be telling us will be invalid.
 */
	/* Toss if not designated "valid" by the gps */
	if (!valid) {
#ifdef DEBUG
		if (debug)
			printf("mx4200_receive: pps not valid\n");
#endif
		mx4200->reason = PPSREASON + 9;
		mx4200_event(mx4200, CEVNT_BADTIME);
		mx4200_reset(mx4200);
		return;
	}

	/* Copy time into mx4200unit struct */
	/* XXX (why?) */
	mx4200->year = jt->year;
	mx4200->monthday = jt->monthday;
	mx4200->hour = jt->hour;
	mx4200->minute = jt->minute;
	mx4200->second = jt->second;

	/* Sock away the GPS and UNIX timesamples */
	n = mx4200->nsamples++;
	if (n < 0)
		return;			/* oops, this pulse is bad */
	mx4200->gpssamples[n] = gpstime;
	mx4200->unixsamples[n].l_ui = mx4200->lastsampletime.l_ui = tmp_ui;
	mx4200->unixsamples[n].l_uf = mx4200->lastsampletime.l_uf = tmp_uf;
	if (mx4200->nsamples >= NSTAMPS) {
		/*
		 * Here we've managed to complete an entire NSTAMPS
		 * second cycle without major mishap. Process what has
		 * been received.
		 */
		mx4200_process(mx4200);
		mx4200_reset(mx4200);
	}
}

/* Compare two l_fp's, used with qsort() */
static int
mx4200_cmpl_fp(p1, p2)
	register void *p1, *p2;
{

	if (!L_ISGEQ((l_fp *)p1, (l_fp *)p2))
		return (-1);
	if (L_ISEQU((l_fp *)p1, (l_fp *)p2))
		return (0);
	return (1);
}

/*
 * mx4200_process - process a pile of samples from the clock
 */
static void
mx4200_process(mx4200)
	struct mx4200unit *mx4200;
{
	register int i, n;
	register l_fp *fp, *op;
	register U_LONG *lp;
	l_fp off[NSTAMPS];
	register U_LONG tmp_ui, tmp_uf;
	register U_LONG date_ui, date_uf;
	u_fp dispersion;

	/* Compute offsets from the raw data. */
	fp = mx4200->unixsamples;
	op = off;
	lp = mx4200->gpssamples;
	for (i = 0; i < NSTAMPS; ++i, ++lp, ++op, ++fp) {
		op->l_ui = *lp;
		op->l_uf = 0;
		L_SUB(op, fp);
	}

	/* Sort offsets into ascending order. */
	qsort((char *)off, NSTAMPS, sizeof(l_fp), mx4200_cmpl_fp);

	/*
	 * Reject the furthest from the median until 8 samples left
	 */
	i = 0;
	n = NSTAMPS;
	while ((n - i) > 8) {
		tmp_ui = off[n-1].l_ui;
		tmp_uf = off[n-1].l_uf;
		date_ui = off[(n+i)/2].l_ui;
		date_uf = off[(n+i)/2].l_uf;
		M_SUB(tmp_ui, tmp_uf, date_ui, date_uf);
		M_SUB(date_ui, date_uf, off[i].l_ui, off[i].l_uf);
		if (M_ISHIS(date_ui, date_uf, tmp_ui, tmp_uf)) {
			/*
			 * reject low end
			 */
			i++;
		} else {
			/*
			 * reject high end
			 */
			n--;
		}
	}

	/*
	 * Compute the dispersion based on the difference between the
	 * extremes of the remaining offsets.
	 */
	tmp_ui = off[n-1].l_ui;
	tmp_uf = off[n-1].l_uf;
	M_SUB(tmp_ui, tmp_uf, off[i].l_ui, off[i].l_uf);
	dispersion = MFPTOFP(tmp_ui, tmp_uf);

	/*
	 * Now compute the offset estimate.  If the sloppy clock
	 * flag is set, average the remainder, otherwise pick the
	 * median.
	 */
	if (sloppyclockflag[mx4200->unit]) {
		tmp_ui = tmp_uf = 0;
		while (i < n) {
			M_ADD(tmp_ui, tmp_uf, off[i].l_ui, off[i].l_uf);
			i++;
		}
		M_RSHIFT(tmp_ui, tmp_uf);
		M_RSHIFT(tmp_ui, tmp_uf);
		M_RSHIFT(tmp_ui, tmp_uf);
		i = 0;
		off[0].l_ui = tmp_ui;
		off[0].l_uf = tmp_uf;
	} else {
		i = (n + i) / 2;
	}

	/*
	 * Add the default MX4200 QT delay into this.
	 */
#ifdef notdef
	L_ADDUF(&off[i], MX4200QTFUDGE);
#endif

	/*
	 * Done.  Use lastref as the reference time and lastrec
	 * as the receive time. ** note this can result in tossing
	 * out the peer in the protocol module if lastref > lastrec,
	 * so last rec is used for both values - dlm ***
	 */
	refclock_receive(mx4200->peer, &off[i],
	    (s_fp)0,				/* delay */
	    dispersion,
	    &mx4200->unixsamples[NSTAMPS-1],	/* reftime */
	    &mx4200->unixsamples[NSTAMPS-1],	/* rectime */
	    mx4200->leap);

	mx4200_event(mx4200, CEVNT_NOMINAL);
}

/*
 * mx4200_control - set fudge factors, return statistics
 */
static void
mx4200_control(unit, in, out)
	u_int unit;
	struct refclockstat *in;
	struct refclockstat *out;
{
	register struct mx4200unit *mx4200;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "mx4200_control: unit %d invalid", unit);
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
				mx4200 = mx4200units[unit];
				peer = mx4200->peer;
				peer->stratum = stratumtouse[unit];
				if (stratumtouse[unit] <= 1)
					memmove((char *)&peer->refid,
						MX4200REFID, 4);
				else
					peer->refid = htonl(MX4200HSREFID);
			}
		}
		if (in->haveflags & CLK_HAVEFLAG1) {
			sloppyclockflag[unit] = in->flags & CLK_FLAG1;
		}
	}

	if (out != 0) {
		out->type = REFCLK_GPS_MX4200;
		out->haveflags
		    = CLK_HAVETIME1|CLK_HAVEVAL1|CLK_HAVEVAL2|CLK_HAVEFLAG1;
		out->clockdesc = MX4200DESCRIPTION;
		out->fudgetime1 = fudgefactor[unit];
		out->fudgetime2.l_ui = 0;
		out->fudgetime2.l_uf = 0;
		out->fudgeval1 = (LONG)stratumtouse[unit];
		out->fudgeval2 = 0;
		out->flags = sloppyclockflag[unit];
		if (unitinuse[unit]) {
			mx4200 = mx4200units[unit];
			out->lencode = mx4200->lencode;
			out->lastcode = mx4200->lastcode;
			out->lastevent = mx4200->lastevent;
			out->currentstatus = mx4200->status;

			out->polls = 0; /* mx4200->polls; */
			out->noresponse = 0; /* mx4200->noresponse; */
			out->badformat = mx4200->badformat;
			out->baddata = mx4200->baddata;
			out->timereset = current_time - mx4200->timestarted;
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
 * mx4200_buginfo - return clock dependent debugging info
 */
static void
mx4200_buginfo(unit, bug)
	int unit;
	register struct refclockbug *bug;
{
	register struct mx4200unit *mx4200;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "mx4200_buginfo: unit %d invalid", unit);
		return;
	}

	if (!unitinuse[unit])
		return;
	mx4200 = mx4200units[unit];

	memset((char *)bug, 0, sizeof(*bug));
	bug->nvalues = 10;
	bug->ntimes = 2;
	if (mx4200->lasttime != 0)
		bug->values[0] = current_time - mx4200->lasttime;
	else
		bug->values[0] = 0;
	bug->values[1] = (U_LONG)mx4200->reason;
	bug->values[2] = (U_LONG)mx4200->year;
	bug->values[3] = (U_LONG)mx4200->monthday;
	bug->values[4] = (U_LONG)mx4200->hour;
	bug->values[5] = (U_LONG)mx4200->minute;
	bug->values[6] = (U_LONG)mx4200->second;
#ifdef notdef
	bug->values[7] = mx4200->msec;
	bug->values[8] = mx4200->noreply;
	bug->values[9] = mx4200->yearstart;
#endif
	bug->stimes = 0x1c;
#ifdef notdef
	bug->times[0] = mx4200->lastref;
	bug->times[1] = mx4200->lastrec;
#endif
}

/*
 * Returns true if the this is a status message. We use this as
 * an indication that the receiver needs to be initialized.
 */
static int
mx4200_needconf(buf)
	char *buf;
{
	register LONG v;
	char *cp;

	cp = buf;

	if ((cp = strchr(cp, ',')) == NULL)
		return (0);
	++cp;

	/* Record type */
	v = strtol(cp, &cp, 10);
	if (v != PMVXG_D_STATUS)
		return (0);
	/*
	 * XXX
	 * Since we configure the receiver to not give us status
	 * messages and since the receiver outputs status messages by
	 * default after being reset to factory defaults when sent the
	 * "$PMVXG,018,C\r\n" message, any status message we get
	 * indicates the reciever needs to be initialized; thus, it is
	 * not necessary to decode the status message.
	 */
#ifdef notdef
	++cp;

	/* Receiver status */
	if ((cp = strchr(cp, ',')) == NULL)
		return (0);
	++cp;

	/* Number of satellites which should be visible */
	if ((cp = strchr(cp, ',')) == NULL)
		return (0);
	++cp;

	/* Number of satellites being tracked */
	if ((cp = strchr(cp, ',')) == NULL)
		return (0);
	++cp;

	/* Time since last NAV */
	if ((cp = strchr(cp, ',')) == NULL)
		return (0);
	++cp;

	/* Initialization status */
	v = strtol(cp, &cp, 10);
	if (v == 0)
#endif
		return (1);
}

/* Parse a mx4200 time recovery message. Returns a string if error */
static char *
mx4200_parse(buf, jt, validp, leapsecp)
	register char *buf;
	register struct calendar *jt;
	register int *validp, *leapsecp;
{
	register LONG v;
	char *cp;

	cp = buf;
	memset((char *)jt, 0, sizeof(*jt));

	if ((cp = strchr(cp, ',')) == NULL)
		return ("no rec-type");
	++cp;

	/* Record type */
	v = strtol(cp, &cp, 10);
	if (v != PMVXG_D_TRECOVOUT)
		return ("wrong rec-type");

	/* Pulse valid indicator */
	if (*cp++ != ',')
		return ("no pulse-valid");
	if (*cp == 'T')
		*validp = 1;
	else if (*cp == 'F')
		*validp = 0;
	else
		return ("bad pulse-valid");
	++cp;

	/* Year */
	if (*cp++ != ',')
		return ("no year");
	jt->year = strtol(cp, &cp, 10);

	/* Month of year */
	if (*cp++ != ',')
		return ("no month");
	jt->month = strtol(cp, &cp, 10);

	/* Day of month */
	if (*cp++ != ',')
		return ("no month day");
	jt->monthday = strtol(cp, &cp, 10);

	/* Hour */
	if (*cp++ != ',')
		return ("no hour");
	jt->hour = strtol(cp, &cp, 10);

	/* Minute */
	if (*cp++ != ':')
		return ("no minute");
	jt->minute = strtol(cp, &cp, 10);

	/* Second */
	if (*cp++ != ':')
		return ("no second");
	jt->second = strtol(cp, &cp, 10);

	/* Time indicator */
	if (*cp++ != ',' || *cp++ == '\0')
		return ("no time indicator");

	/* Time recovery mode */
	if (*cp++ != ',' || *cp++ == '\0')
		return ("no time mode");

	/* Oscillator offset */
	if ((cp = strchr(cp, ',')) == NULL)
		return ("no osc off");
	++cp;

	/* Time mark error */
	if ((cp = strchr(cp, ',')) == NULL)
		return ("no time mark err");
	++cp;

	/* User time bias */
	if ((cp = strchr(cp, ',')) == NULL)
		return ("no user bias");
	++cp;

	/* Leap second flag */
	if ((cp = strchr(cp, ',')) == NULL)
		return ("no leap");
	++cp;
	*leapsecp = strtol(cp, &cp, 10);

	return (NULL);
}

/* Calculate the checksum */
static u_char
cksum(cp, n)
	register char *cp;
	register u_int n;
{
	register u_char ck;

	for (ck = 0; n-- > 0; ++cp)
		ck ^= *cp;
	return (ck);
}

static void
#if __STDC__
mx4200_send(register int fd, const char *fmt, ...)
#else
mx4200_send(fd, fmt, va_alist)
	register int fd;
	const char *fmt;
	va_dcl
#endif
{
	register char *cp;
	register int n, m;
	va_list ap;
	char buf[1024];
	u_char ck;

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	cp = buf;
	*cp++ = '$';
#ifdef notdef
	/* BSD is rational */
	n = vsnprintf(cp, sizeof(buf) - 1, fmt, ap);
#else
	/* SunOS sucks */
	(void)vsprintf(cp, fmt, ap);
	n = strlen(cp);
#endif
	ck = cksum(cp, n);
	cp += n;
	++n;
#ifdef notdef
	/* BSD is rational */
	n += snprintf(cp, sizeof(buf) - n - 5, "*%02X\r\n", ck);
#else
	/* SunOS sucks */
	sprintf(cp, "*%02X\r\n", ck);
	n += strlen(cp);
#endif

	m = write(fd, buf, n);
	if (m < 0)
		syslog(LOG_ERR, "mx4200_send: write: %m (%s)", buf);
	else if (m != n)
		syslog(LOG_ERR, "mx4200_send: write: %d != %d (%s)", m, n, buf);
	va_end(ap);
}
#endif
