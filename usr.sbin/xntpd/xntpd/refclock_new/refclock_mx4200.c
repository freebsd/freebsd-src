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

#if defined(REFCLOCK) && defined(MX4200)

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
#define	MX4200FD	"/dev/gps%d"
#define	SPEED232	B4800		/* baud */

/*
 * The number of raw samples which we acquire to derive a single estimate.
 */
#define	NSTMPS	64

/*
 * Radio interface parameters
 */
#define	MX4200PRECISION	(-18)	/* precision assumed (about 4 us) */
#define	MX4200REFID	"GPS"	/* reference id */
#define	MX4200DESCRIPTION	"Magnavox MX4200 GPS Receiver" /* WRU */
#define	DEFFUDGETIME	0	/* default fudge time (ms) */

/* Leap stuff */
extern u_long leap_hoursfromleap;
extern u_long leap_happened;
static int leap_debug;

/*
 * mx4200_reset - reset the count back to zero
 */
#define	mx4200_reset(up) \
	do { \
		(up)->nsamples = 0; \
	} while (0)

/*
 * Imported from the timer module
 */
extern u_long current_time;
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
	u_long gpssamples[NSTMPS];	/* the GPS time samples */
	l_fp unixsamples[NSTMPS];	/* the UNIX time samples */


	l_fp lastsampletime;		/* time of last estimate */
	u_int lastserial;		/* last pps serial number */
#ifdef notdef
	l_fp lastrec;			/* last receive time */
	l_fp lastref;			/* last timecode time */
#endif
	char lastcode[RX_BUFF_SIZE];	/* last timecode received */
	u_long lasttime;		/* last time clock heard from */
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
	u_long polls;			/* polls sent */
	u_long noresponse;		/* number of nonresponses */
#endif
	u_long badformat;		/* bad format */
	u_long baddata;			/* bad data */
	u_long timestarted;		/* time we started this */
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

static const char pmvxg[] = "PMVXG";

/*
 * Function prototypes
 */
static	int	mx4200_start	P((int, struct peer *));
static	void	mx4200_shutdown	P((int, struct peer *));
static	void	mx4200_receive	P((struct recvbuf *));
static	void	mx4200_process	P((struct mx4200unit *));
static	void	mx4200_poll	P((int, struct peer *));

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
	mx4200_start,		/* start up driver */
	mx4200_shutdown,		/* shut down driver */
	mx4200_poll,		/* transmit poll message */
	noentry,		/* not used (old mx4200_control) */
	noentry,		/* initialize driver (not used) */
	noentry,		/* not used (old mx4200_buginfo) */
	NOFLAGS			/* not used */
};

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
 * mx4200_start - open the devices and initialize data for processing
 */
static int
mx4200_start(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct mx4200unit *up;
	struct refclockproc *pp;
	int fd;
	char device[20];

	/*
	 * Open serial port
	 */
	(void)sprintf(device, MX4200FD, unit);
	if (!(fd = refclock_open(device, SPEED232, 0)))
		return (0);

	/*
	 * Allocate and initialize unit structure
	 */
	if (!(up = (struct mx4200unit *)
	    emalloc(sizeof(struct mx4200unit)))) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct mx4200unit));
	up->io.clock_recv = mx4200_receive;
	up->io.srcclock = (caddr_t)up;
	up->io.datalen = 0;
	up->io.fd = fd;
	if (!io_addclock(&up->io)) {
		(void) close(fd);
		free(up);
		return (0);
	}
	up->peer = peer;
	pp = peer->procptr;
	pp->unitptr = (caddr_t)up;

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = MX4200PRECISION;
	pp->clockdesc = MX4200DESCRIPTION;
	memcpy((char *)&pp->refid, MX4200REFID, 4);

	/* Insure the receiver is properly configured */
	mx4200_config(up);

#ifdef DEBUG
	opendfile(0);
#endif
	return (1);
}


/*
 * mx4200_shutdown - shut down the clock
 */
static void
mx4200_shutdown(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct mx4200unit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct mx4200unit *)pp->unitptr;
	io_closeclock(&up->io);
	free(up);
}


static void
mx4200_config(up)
	register struct mx4200unit *up;
{
	register int fd = up->io.fd;

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
 * mx4200_poll - mx4200 watchdog routine
 */
static void
mx4200_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	register struct mx4200unit *up;

	if (unit >= MAXUNITS) {
		syslog(LOG_ERR, "mx4200_poll: unit %d invalid", unit);
		return;
	}

	up = mx4200units[unit];
	if ((current_time - up->lasttime) > 150) {
		refclock_report(peer, CEVNT_FAULT);

		/* Request a status message which should trigger a reconfig */
		mx4200_send(up->io.fd, "%s,%03d", "CDGPQ", PMVXG_D_STATUS);
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
	register struct mx4200unit *up;
	struct peer *peer;
	register char *dpt, *cp;
	register u_long tmp_ui;
	register u_long tmp_uf;
	register u_long gpstime;
	struct ppsclockev ev;
	register struct calendar *jt;
	struct calendar sjt;
	register int n;
	int valid, leapsec;
	register u_char ck;

	up = (struct mx4200unit *)rbufp->recv_srcclock;
	peer = up->peer;
#ifdef DEBUG
	if (debug > 3)
		printf("mx4200_receive: nsamples = %d\n", up->nsamples);
#endif

	/* Record the time of this event */
	up->lasttime = current_time;

	/* Get the pps value */
	if (ioctl(up->io.fd, CIOGETEV, (char *)&ev) < 0) {
		/* XXX Actually, if this fails, we're pretty much screwed */
#ifdef DEBUG
		if (debug) {
			fprintf(stderr, "mx4200_receive: ");
			perror("CIOGETEV");
		}
#endif
		refclock_report(peer, CEVNT_FAULT);
		mx4200_reset(up);
		return;
	}
	tmp_ui = ev.tv.tv_sec + JAN_1970;
	TVUTOTSF(ev.tv.tv_usec, tmp_uf);

	/* Get buffer and length; sock away last timecode */
	n = rbufp->recv_length;
	dpt = rbufp->recv_buffer;
	if (n <= 1)
		return;
	up->lencode = n;
	memmove(up->lastcode, dpt, n);

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
		refclock_report(peer, CEVNT_BADREPLY);
		mx4200_reset(up);
		return;
	}

	/* Check checksum */
	ck = cksum(&dpt[1], n - 5);
	if (char2hex[ck >> 4] != cp[1] || char2hex[ck & 0xf] != cp[2]) {
#ifdef DEBUG
		if (debug)
			printf("mx4200_receive: bad checksum\n");
#endif
		refclock_report(peer, CEVNT_BADREPLY);
		mx4200_reset(up);
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
			mx4200_config(up);
#ifdef DEBUG
		if (debug)
			printf("mx4200_receive: mx4200_parse: %s\n", cp);
#endif
		refclock_report(peer, CEVNT_BADREPLY);
		mx4200_reset(up);
		return;
	}

	/* Setup leap second indicator */
	if (leapsec == 0)
		up->leap = LEAP_NOWARNING;
	else if (leapsec == 1)
		up->leap = LEAP_ADDSECOND;
	else if (leapsec == -1)
		up->leap = LEAP_DELSECOND;
	else
		up->leap = LEAP_NOTINSYNC;		/* shouldn't happen */

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
		refclock_report(peer, CEVNT_BADTIME);
		mx4200_reset(up);
		/* Eat the next pulse which the clock claims will be bad */
		up->nsamples = -1;
		return;
	}

	/* Check parsed date */
	if (jt->monthday > 31 || jt->month > 12 || jt->year < 1900) {
#ifdef DEBUG
		if (debug)
			printf("mx4200_receive: bad date (%d/%d/%d)\n",
			    jt->monthday, jt->month, jt->year);
#endif
		refclock_report(peer, CEVNT_BADDATE);
		mx4200_reset(up);
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
	if (up->lastserial + 1 != ev.serial && up->lastserial != 0) {
#ifdef DEBUG
		if (debug) {
			if (ev.serial == up->lastserial)
				printf("mx4200_receive: no new pps event\n");
			else
				printf("mx4200_receive: missed %d pps events\n",
				    ev.serial - up->lastserial - 1);
		}
#endif
		refclock_report(peer, CEVNT_FAULT);
		mx4200_reset(up);
		/* fall through and this one collect as first sample */
	}
	up->lastserial = ev.serial;

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
		refclock_report(peer, CEVNT_BADTIME);
		mx4200_reset(up);
		return;
	}

	/* Copy time into mx4200unit struct */
	/* XXX (why?) */
	up->year = jt->year;
	up->monthday = jt->monthday;
	up->hour = jt->hour;
	up->minute = jt->minute;
	up->second = jt->second;

	/* Sock away the GPS and UNIX timesamples */
	n = up->nsamples++;
	if (n < 0)
		return;			/* oops, this pulse is bad */
	up->gpssamples[n] = gpstime;
	up->unixsamples[n].l_ui = up->lastsampletime.l_ui = tmp_ui;
	up->unixsamples[n].l_uf = up->lastsampletime.l_uf = tmp_uf;
	if (up->nsamples >= NSTMPS) {
		/*
		 * Here we've managed to complete an entire NSTMPS
		 * second cycle without major mishap. Process what has
		 * been received.
		 */
		mx4200_process(up);
		mx4200_reset(up);
	}
}

/*
 * Compare two l_fp's, used with qsort()
 */
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
mx4200_process(up)
	struct mx4200unit *up;
{
	struct peer *peer;
	struct refclockproc *pp;
	register int i, n;
	register l_fp *fp, *op;
	register u_long *lp;
	l_fp off[NSTMPS];
	register u_long tmp_ui, tmp_uf;
	register u_long date_ui, date_uf;
	u_fp dispersion;

	/* Compute offsets from the raw data. */
	peer = up->peer;
	pp = peer->procptr; 
	fp = up->unixsamples;
	op = off;
	lp = up->gpssamples;
	for (i = 0; i < NSTMPS; ++i, ++lp, ++op, ++fp) {
		op->l_ui = *lp;
		op->l_uf = 0;
		L_SUB(op, fp);
	}

	/* Sort offsets into ascending order. */
	qsort((char *)off, NSTMPS, sizeof(l_fp), mx4200_cmpl_fp);

	/*
	 * Reject the furthest from the median until 8 samples left
	 */
	i = 0;
	n = NSTMPS;
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
	if (pp->sloppyclockflag) {
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
	refclock_receive(up->peer, &off[i],
	    (s_fp)0,				/* delay */
	    dispersion,
	    &up->unixsamples[NSTMPS-1],	/* reftime */
	    &up->unixsamples[NSTMPS-1],	/* rectime */
	    up->leap);

	refclock_report(peer, CEVNT_NOMINAL);
}


/*
 * Returns true if the this is a status message. We use this as
 * an indication that the receiver needs to be initialized.
 */
static int
mx4200_needconf(buf)
	char *buf;
{
	register long v;
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
	register long v;
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
