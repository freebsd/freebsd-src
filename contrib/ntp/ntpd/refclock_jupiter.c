/*
 * Copyright (c) 1997, 1998
 *	The Regents of the University of California.  All rights reserved.
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if defined(REFCLOCK) && defined(CLOCK_JUPITER) && defined(PPS)

#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"
#include "ntp_stdlib.h"
#include "ntp_calendar.h"

#include "jupiter.h"

#include <sys/ppsclock.h>

#ifdef XNTP_BIG_ENDIAN
#define getshort(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))
#define putshort(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))
#else
#define getshort(s) (s)
#define putshort(s) (s)
#endif

/* XXX */
#ifdef sun
char *strerror(int);
#endif

/*
 * This driver supports the Rockwell Jupiter GPS Receiver board
 * adapted to precision timing applications.  It requires the
 * ppsclock line discipline or streams module described in the
 * Line Disciplines and Streams Drivers page. It also requires a
 * gadget box and 1-PPS level converter, such as described in the
 * Pulse-per-second (PPS) Signal Interfacing page.
 *
 * It may work (with minor modifications) with other Rockwell GPS
 * receivers such as the CityTracker.
 */

/*
 * GPS Definitions
 */
#define	DEVICE		"/dev/gps%d"	/* device name and unit */
#define	SPEED232	B9600		/* baud */

/*
 * The number of raw samples which we acquire to derive a single estimate.
 * NSAMPLES ideally should not exceed the default poll interval 64.
 * NKEEP must be a power of 2 to simplify the averaging process.
 */
#define NSAMPLES	64
#define NKEEP		8
#define REFCLOCKMAXDISPERSE .25	/* max sample dispersion */

/*
 * Radio interface parameters
 */
#define	PRECISION	(-18)	/* precision assumed (about 4 us) */
#define	REFID	"GPS\0"		/* reference id */
#define	DESCRIPTION	"Rockwell Jupiter GPS Receiver" /* who we are */
#define	DEFFUDGETIME	0	/* default fudge time (ms) */

/* Unix timestamp for the GPS epoch: January 6, 1980 */
#define GPS_EPOCH 315964800

/* Double short to unsigned int */
#define DS2UI(p) ((getshort((p)[1]) << 16) | getshort((p)[0]))

/* Double short to signed int */
#define DS2I(p) ((getshort((p)[1]) << 16) | getshort((p)[0]))

/* One week's worth of seconds */
#define WEEKSECS (7 * 24 * 60 * 60)

/*
 * Jupiter unit control structure.
 */
struct jupiterunit {
	u_int  pollcnt;			/* poll message counter */
	u_int  polled;			/* Hand in a time sample? */
	u_int  lastserial;		/* last pps serial number */
	struct ppsclockev ppsev;	/* PPS control structure */
	u_int gweek;			/* current GPS week number */
	u_int32 lastsweek;		/* last seconds into GPS week */
	u_int32 timecode;		/* current ntp timecode */
	u_int32 stime;			/* used to detect firmware bug */
	int wantid;			/* don't reconfig on channel id msg */
	u_int  moving;			/* mobile platform? */
	u_long sloppyclockflag;		/* fudge flags */
	u_int  known;			/* position known yet? */
	int    coderecv;		/* total received samples */
	int    nkeep;			/* number of samples to preserve */
	int    rshift;			/* number of rshifts for division */
	l_fp   filter[NSAMPLES];	/* offset filter */
	l_fp   lastref;			/* last reference timestamp */
	u_short sbuf[512];		/* local input buffer */
	int ssize;			/* space used in sbuf */
};

/*
 * Function prototypes
 */
static	void	jupiter_canmsg	P((struct peer *, u_int));
static	u_short	jupiter_cksum	P((u_short *, u_int));
#ifdef QSORT_USES_VOID_P
	int	jupiter_cmpl_fp	P((const void *, const void *));
#else
	int	jupiter_cmpl_fp	P((const l_fp *, const l_fp *));
#endif /* not QSORT_USES_VOID_P */
static	void	jupiter_config	P((struct peer *));
static	void	jupiter_debug	P((struct peer *, char *, ...))
    __attribute__ ((format (printf, 2, 3)));
static	char *	jupiter_offset	P((struct peer *));
static	char *	jupiter_parse_t	P((struct peer *, u_short *));
static	void	jupiter_platform	P((struct peer *, u_int));
static	void	jupiter_poll	P((int, struct peer *));
static	int	jupiter_pps	P((struct peer *));
static	char *	jupiter_process	P((struct peer *));
static	int	jupiter_recv	P((struct peer *));
static	void	jupiter_receive P((register struct recvbuf *rbufp));
static	void	jupiter_reqmsg	P((struct peer *, u_int, u_int));
static	void	jupiter_reqonemsg	P((struct peer *, u_int));
static	char *	jupiter_send	P((struct peer *, struct jheader *));
static	void	jupiter_shutdown	P((int, struct peer *));
static	int	jupiter_start	P((int, struct peer *));
static	int	jupiter_ttyinit	P((struct peer *, int));

/*
 * Transfer vector
 */
struct	refclock refclock_jupiter = {
	jupiter_start,		/* start up driver */
	jupiter_shutdown,	/* shut down driver */
	jupiter_poll,		/* transmit poll message */
	noentry,		/* (clock control) */
	noentry,		/* (clock init) */
	noentry,		/* (clock buginfo) */
	NOFLAGS			/* not used */
};

/*
 * jupiter_start - open the devices and initialize data for processing
 */
static int
jupiter_start(
	register int unit,
	register struct peer *peer
	)
{
	struct refclockproc *pp;
	register struct jupiterunit *up;
	register int fd;
	char gpsdev[20];

	/*
	 * Open serial port
	 */
	(void)sprintf(gpsdev, DEVICE, unit);
	fd = open(gpsdev, O_RDWR
#ifdef O_NONBLOCK
	    | O_NONBLOCK
#endif
	    , 0);
	if (fd < 0) {
		jupiter_debug(peer, "jupiter_start: open %s: %s\n",
		    gpsdev, strerror(errno));
		return (0);
	}
	if (!jupiter_ttyinit(peer, fd))
		return (0);

	/* Allocate unit structure */
	if ((up = (struct jupiterunit *)
	    emalloc(sizeof(struct jupiterunit))) == NULL) {
		(void) close(fd);
		return (0);
	}
	memset((char *)up, 0, sizeof(struct jupiterunit));
	pp = peer->procptr;
	pp->io.clock_recv = jupiter_receive;
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


	/* Ensure the receiver is properly configured */
	jupiter_config(peer);

	/* Turn on pulse gathering by requesting the first sample */
	if (ioctl(fd, CIOGETEV, (caddr_t)&up->ppsev) < 0) {
		jupiter_debug(peer, "jupiter_ttyinit: CIOGETEV: %s\n",
		    strerror(errno));
		(void) close(fd);
		free(up);
		return (0);
	}
	up->lastserial = up->ppsev.serial;
	memset(&up->ppsev, 0, sizeof(up->ppsev));
	return (1);
}

/*
 * jupiter_shutdown - shut down the clock
 */
static void
jupiter_shutdown(register int unit, register struct peer *peer)
{
	register struct jupiterunit *up;
	struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct jupiterunit *)pp->unitptr;
	io_closeclock(&pp->io);
	free(up);
}

/*
 * jupiter_config - Configure the receiver
 */
static void
jupiter_config(register struct peer *peer)
{
	register int i;
	register struct jupiterunit *up;
	register struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct jupiterunit *)pp->unitptr;

	/*
	 * Initialize the unit variables
	 *
	 * STRANGE BEHAVIOUR WARNING: The fudge flags are not available
	 * at the time jupiter_start is called.  These are set later,
	 * and so the code must be prepared to handle changing flags.
	 */
	up->sloppyclockflag = pp->sloppyclockflag;
	if (pp->sloppyclockflag & CLK_FLAG2) {
		up->moving = 1;		/* Receiver on mobile platform */
		msyslog(LOG_DEBUG, "jupiter_config: mobile platform");
	} else {
		up->moving = 0;		/* Static Installation */
	}

	/* XXX fludge flags don't make the trip from the config to here... */
#ifdef notdef
	/* Configure for trailing edge triggers */
#ifdef CIOSETTET
	i = ((pp->sloppyclockflag & CLK_FLAG3) != 0);
	jupiter_debug(peer, "jupiter_configure: (sloppyclockflag 0x%lx)\n",
	    pp->sloppyclockflag);
	if (ioctl(pp->io.fd, CIOSETTET, (char *)&i) < 0)
		msyslog(LOG_DEBUG, "jupiter_configure: CIOSETTET %d: %m", i);
#else
	if (pp->sloppyclockflag & CLK_FLAG3)
		msyslog(LOG_DEBUG, "jupiter_configure: \
No kernel support for trailing edge trigger");
#endif
#endif

	up->pollcnt     = 2;
	up->polled      = 0;
	up->known       = 0;
	up->gweek = 0;
	up->lastsweek = 2 * WEEKSECS;
	up->timecode = 0;
	up->stime = 0;
	up->ssize = 0;
	up->coderecv    = 0;
	up->nkeep       = NKEEP;
	if (up->nkeep > NSAMPLES)
		up->nkeep = NSAMPLES;
	if (up->nkeep >= 1)
		up->rshift = 0;
	if (up->nkeep >= 2)
		up->rshift = 1;
	if (up->nkeep >= 4)
		up->rshift = 2;
	if (up->nkeep >= 8)
		up->rshift = 3;
	if (up->nkeep >= 16)
		up->rshift = 4;
	if (up->nkeep >= 32)
		up->rshift = 5;
	if (up->nkeep >= 64)
		up->rshift = 6;
	up->nkeep = 1;
	i = up->rshift;
	while (i > 0) {
		up->nkeep *= 2;
		i--;
	}

	/* Stop outputting all messages */
	jupiter_canmsg(peer, JUPITER_ALL);

	/* Request the receiver id so we can syslog the firmware version */
	jupiter_reqonemsg(peer, JUPITER_O_ID);

	/* Flag that this the id was requested (so we don't get called again) */
	up->wantid = 1;

	/* Request perodic time mark pulse messages */
	jupiter_reqmsg(peer, JUPITER_O_PULSE, 1);

	/* Set application platform type */
	if (up->moving)
		jupiter_platform(peer, JUPITER_I_PLAT_MED);
	else
		jupiter_platform(peer, JUPITER_I_PLAT_LOW);
}

/*
 * jupiter_poll - jupiter watchdog routine
 */
static void
jupiter_poll(register int unit, register struct peer *peer)
{
	register struct jupiterunit *up;
	register struct refclockproc *pp;

	pp = peer->procptr;
	up = (struct jupiterunit *)pp->unitptr;

	/*
	 * You don't need to poll this clock.  It puts out timecodes
	 * once per second.  If asked for a timestamp, take note.
	 * The next time a timecode comes in, it will be fed back.
	 */

	/*
	 * If we haven't had a response in a while, reset the receiver.
	 */
	if (up->pollcnt > 0) {
		up->pollcnt--;
	} else {
		refclock_report(peer, CEVNT_TIMEOUT);

		/* Request the receiver id to trigger a reconfig */
		jupiter_reqonemsg(peer, JUPITER_O_ID);
		up->wantid = 0;
	}

	/*
	 * polled every 64 seconds. Ask jupiter_receive to hand in
	 * a timestamp.
	 */
	up->polled = 1;
	pp->polls++;
}

/*
 * jupiter_receive - receive gps data
 * Gag me!
 */
static void
jupiter_receive(register struct recvbuf *rbufp)
{
	register int bpcnt, cc, size, ppsret;
	register u_int32 last_timecode, laststime;
	register char *cp;
	register u_char *bp;
	register u_short *sp;
	register u_long sloppyclockflag;
	register struct jupiterunit *up;
	register struct jid *ip;
	register struct jheader *hp;
	register struct refclockproc *pp;
	register struct peer *peer;

	/* Initialize pointers and read the timecode and timestamp */
	peer = (struct peer *)rbufp->recv_srcclock;
	pp = peer->procptr;
	up = (struct jupiterunit *)pp->unitptr;

	/*
	 * If operating mode has been changed, then reinitialize the receiver
	 * before doing anything else.
	 */
/* XXX Sloppy clock flags are broken!! */
	sloppyclockflag = up->sloppyclockflag;
	up->sloppyclockflag = pp->sloppyclockflag;
	if ((pp->sloppyclockflag & CLK_FLAG2) !=
	    (sloppyclockflag & CLK_FLAG2)) {
		jupiter_debug(peer,
		    "jupiter_receive: mode switch: reset receiver\n");
		jupiter_config(peer);
		return;
	}

	up->pollcnt = 2;

	bp = (u_char *)rbufp->recv_buffer;
	bpcnt = rbufp->recv_length;

	/* This shouldn't happen */
	if (bpcnt > sizeof(up->sbuf) - up->ssize)
		bpcnt = sizeof(up->sbuf) - up->ssize;

	/* Append to input buffer */
	memcpy((u_char *)up->sbuf + up->ssize, bp, bpcnt);
	up->ssize += bpcnt;

	/* While there's at least a header and we parse a intact message */
	while (up->ssize > sizeof(*hp) && (cc = jupiter_recv(peer)) > 0) {
		hp = (struct jheader *)up->sbuf;
		sp = (u_short *)(hp + 1);
		size = cc - sizeof(*hp);
		switch (getshort(hp->id)) {

		case JUPITER_O_PULSE:
			if (size != sizeof(struct jpulse)) {
				jupiter_debug(peer,
				    "jupiter_receive: pulse: len %d != %u\n",
				    size, (int)sizeof(struct jpulse));
				refclock_report(peer, CEVNT_BADREPLY);
				break;
			}

			/*
			 * There appears to be a firmware bug related
			 * to the pulse message; in addition to the one
			 * per second messages, we get an extra pulse
			 * message once an hour (on the anniversary of
			 * the cold start). It seems to come 200 ms
			 * after the one requested. So if we've seen a
			 * pulse message in the last 210 ms, we skip
			 * this one.
			 */
			laststime = up->stime;
			up->stime = DS2UI(((struct jpulse *)sp)->stime);
			if (laststime != 0 && up->stime - laststime <= 21) {
				jupiter_debug(peer, "jupiter_receive: \
avoided firmware bug (stime %.2f, laststime %.2f)\n",
    (double)up->stime * 0.01, (double)laststime * 0.01);
				break;
			}

			/* Retrieve pps timestamp */
			ppsret = jupiter_pps(peer);

			/* Parse timecode (even when there's no pps) */
			last_timecode = up->timecode;
			if ((cp = jupiter_parse_t(peer, sp)) != NULL) {
				jupiter_debug(peer,
				    "jupiter_receive: pulse: %s\n", cp);
				break;
			}

			/* Bail if we didn't get a pps timestamp */
			if (ppsret)
				break;

			/* Bail if we don't have the last timecode yet */
			if (last_timecode == 0)
				break;

			/* Add the new sample to a median filter */
			if ((cp = jupiter_offset(peer)) != NULL) {
				jupiter_debug(peer,
				    "jupiter_receive: offset: %s\n", cp);
				refclock_report(peer, CEVNT_BADTIME);
				break;
			}

			/*
			 * The clock will blurt a timecode every second
			 * but we only want one when polled.  If we
			 * havn't been polled, bail out.
			 */
			if (!up->polled)
				break;

			/*
			 * It's a live one!  Remember this time.
			 */
			pp->lasttime = current_time;

			/*
			 * Determine the reference clock offset and
			 * dispersion. NKEEP of NSAMPLE offsets are
			 * passed through a median filter.
			 * Save the (filtered) offset and dispersion in
			 * pp->offset and pp->disp.
			 */
			if ((cp = jupiter_process(peer)) != NULL) {
				jupiter_debug(peer,
				    "jupiter_receive: process: %s\n", cp);
				refclock_report(peer, CEVNT_BADTIME);
				break;
			}
			/*
			 * Return offset and dispersion to control
			 * module. We use lastrec as both the reference
			 * time and receive time in order to avoid
			 * being cute, like setting the reference time
			 * later than the receive time, which may cause
			 * a paranoid protocol module to chuck out the
			 * data.
			 */
			jupiter_debug(peer,
			    "jupiter_receive: process time: \
%4d-%03d %02d:%02d:%02d at %s, %s\n",
			    pp->year, pp->day,
			    pp->hour, pp->minute, pp->second,
			    prettydate(&pp->lastrec), lfptoa(&pp->offset, 6));

			refclock_receive(peer);

			/*
			 * We have succeeded in answering the poll.
			 * Turn off the flag and return
			 */
			up->polled = 0;
			break;

		case JUPITER_O_ID:
			if (size != sizeof(struct jid)) {
				jupiter_debug(peer,
				    "jupiter_receive: id: len %d != %u\n",
				    size, (int)sizeof(struct jid));
				refclock_report(peer, CEVNT_BADREPLY);
				break;
			}
			/*
			 * If we got this message because the Jupiter
			 * just powered up, it needs to be reconfigured.
			 */
			ip = (struct jid *)sp;
			jupiter_debug(peer,
			    "jupiter_receive: >> %s chan ver %s, %s (%s)\n",
			    ip->chans, ip->vers, ip->date, ip->opts);
			msyslog(LOG_DEBUG,
			    "jupiter_receive: %s chan ver %s, %s (%s)\n",
			    ip->chans, ip->vers, ip->date, ip->opts);
			if (up->wantid)
				up->wantid = 0;
			else {
				jupiter_debug(peer,
				    "jupiter_receive: reset receiver\n");
				jupiter_config(peer);
				/* Rese since jupiter_config() just zeroed it */
				up->ssize = cc;
			}
			break;

		default:
			jupiter_debug(peer,
			    "jupiter_receive: >> unknown message id %d\n",
			    getshort(hp->id));
			break;
		}
		up->ssize -= cc;
		if (up->ssize < 0) {
			fprintf(stderr, "jupiter_recv: negative ssize!\n");
			abort();
		} else if (up->ssize > 0)
			memcpy(up->sbuf, (u_char *)up->sbuf + cc, up->ssize);
	}
	record_clock_stats(&peer->srcadr, "<timecode is binary>");
}

/*
 * jupiter_offset - Calculate the offset, and add to the rolling filter.
 */
static char *
jupiter_offset(register struct peer *peer)
{
	register struct jupiterunit *up;
	register struct refclockproc *pp;
	register int i;
	l_fp offset;

	pp = peer->procptr;
	up = (struct jupiterunit *)pp->unitptr;

	/*
	 * Calculate the offset
	 */
	if (!clocktime(pp->day, pp->hour, pp->minute, pp->second, GMT,
		pp->lastrec.l_ui, &pp->yearstart, &offset.l_ui)) {
		return ("jupiter_process: clocktime failed");
	}
	if (pp->usec) {
		TVUTOTSF(pp->usec, offset.l_uf);
	} else {
		MSUTOTSF(pp->msec, offset.l_uf);
	}
	L_ADD(&offset, &pp->fudgetime1);
	up->lastref = offset;   /* save last reference time */
	L_SUB(&offset, &pp->lastrec); /* form true offset */

	/*
	 * A rolling filter.  Initialize first time around.
	 */
	i = ((up->coderecv)) % NSAMPLES;

	up->filter[i] = offset;
	if (up->coderecv == 0)
		for (i = 1; (u_int) i < NSAMPLES; i++)
			up->filter[i] = up->filter[0];
	up->coderecv++;

	return (NULL);
}

/*
 * jupiter_process - process the sample from the clock,
 * passing it through a median filter and optionally averaging
 * the samples.  Returns offset and dispersion in "up" structure.
 */
static char *
jupiter_process(register struct peer *peer)
{
	register struct jupiterunit *up;
	register struct refclockproc *pp;
	register int i, n;
	register int j, k;
	l_fp offset, median, lftmp;
	u_fp disp;
	l_fp off[NSAMPLES];

	pp = peer->procptr;
	up = (struct jupiterunit *)pp->unitptr;

	/*
	 * Copy the raw offsets and sort into ascending order
	 */
	for (i = 0; i < NSAMPLES; i++)
		off[i] = up->filter[i];
	qsort((char *)off, NSAMPLES, sizeof(l_fp), jupiter_cmpl_fp);

	/*
	 * Reject the furthest from the median of NSAMPLES samples until
	 * NKEEP samples remain.
	 */
	i = 0;
	n = NSAMPLES;
	while ((n - i) > up->nkeep) {
		lftmp = off[n - 1];
		median = off[(n + i) / 2];
		L_SUB(&lftmp, &median);
		L_SUB(&median, &off[i]);
		if (L_ISHIS(&median, &lftmp)) {
			/* reject low end */
			i++;
		} else {
			/* reject high end */
			n--;
		}
	}

	/*
	 * Copy key values to the billboard to measure performance.
	 */
	pp->lastref = up->lastref;
	pp->coderecv = up->coderecv;
	pp->filter[0] = off[0];			/* smallest offset */
	pp->filter[1] = off[NSAMPLES-1];	/* largest offset */
	for (j = 2, k = i; k < n; j++, k++)
		pp->filter[j] = off[k];		/* offsets actually examined */

	/*
	 * Compute the dispersion based on the difference between the
	 * extremes of the remaining offsets. Add to this the time since
	 * the last clock update, which represents the dispersion
	 * increase with time. We know that NTP_MAXSKEW is 16. If the
	 * sum is greater than the allowed sample dispersion, bail out.
	 * If the loop is unlocked, return the most recent offset;
	 * otherwise, return the median offset.
	 */
	lftmp = off[n - 1];
	L_SUB(&lftmp, &off[i]);
	disp = LFPTOFP(&lftmp);
	if (disp > REFCLOCKMAXDISPERSE)
		return ("Maximum dispersion exceeded");

	/*
	 * Now compute the offset estimate.  If fudge flag 1
	 * is set, average the remainder, otherwise pick the
	 * median.
	 */
	if (pp->sloppyclockflag & CLK_FLAG1) {
		L_CLR(&lftmp);
		while (i < n) {
			L_ADD(&lftmp, &off[i]);
			i++;
		}
		i = up->rshift;
		while (i > 0) {
			L_RSHIFT(&lftmp);
			i--;
		}
		offset = lftmp;
	} else {
		i = (n + i) / 2;
		offset = off[i];
	}

	/*
	 * The payload: filtered offset and dispersion.
	 */

	pp->offset = offset;
	pp->disp = disp;

	return (NULL);

}

/* Compare two l_fp's, used with qsort() */
#ifdef QSORT_USES_VOID_P
int
jupiter_cmpl_fp(register const void *p1, register const void *p2)
#else
int
jupiter_cmpl_fp(register const l_fp *fp1, register const l_fp *fp2)
#endif
{
#ifdef QSORT_USES_VOID_P
	register const l_fp *fp1 = (const l_fp *)p1;
	register const l_fp *fp2 = (const l_fp *)p2;
#endif

	if (!L_ISGEQ(fp1, fp2))
		return (-1);
	if (L_ISEQU(fp1, fp2))
		return (0);
	return (1);
}

static char *
jupiter_parse_t(register struct peer *peer, register u_short *sp)
{
	register struct refclockproc *pp;
	register struct jupiterunit *up;
	register struct tm *tm;
	register char *cp;
	register struct jpulse *jp;
	register struct calendar *jt;
	register u_int32 sweek;
	register u_int32 last_timecode;
	register u_short flags;
	time_t t;
	struct calendar cal;

	pp = peer->procptr;
	up = (struct jupiterunit *)pp->unitptr;
	jp = (struct jpulse *)sp;

	/* The timecode is presented as seconds into the current GPS week */
	sweek = DS2UI(jp->sweek);

	/*
	 * If we don't know the current GPS week, calculate it from the
	 * current time. (It's too bad they didn't include this
	 * important value in the pulse message). We'd like to pick it
	 * up from one of the other messages like gpos or chan but they
	 * don't appear to be synchronous with time keeping and changes
	 * too soon (something like 10 seconds before the new GPS
	 * week).
	 *
	 * If we already know the current GPS week, increment it when
	 * we wrap into a new week.
	 */
	if (up->gweek == 0)
		up->gweek = (time(NULL) - GPS_EPOCH) / WEEKSECS;
	else if (sweek == 0 && up->lastsweek == WEEKSECS - 1) {
		++up->gweek;
		jupiter_debug(peer,
		    "jupiter_parse_t: NEW gps week %u\n", up->gweek);
	}

	/*
	 * See if the sweek stayed the same (this happens when there is
	 * no pps pulse).
	 *
	 * Otherwise, look for time warps:
	 *
	 *   - we have stored at least one lastsweek and
	 *   - the sweek didn't increase by one and
	 *   - we didn't wrap to a new GPS week
	 *
	 * Then we warped.
	 */
	if (up->lastsweek == sweek)
		jupiter_debug(peer,
		    "jupiter_parse_t: gps sweek not incrementing (%d)\n",
		    sweek);
	else if (up->lastsweek != 2 * WEEKSECS &&
	    up->lastsweek + 1 != sweek &&
	    !(sweek == 0 && up->lastsweek == WEEKSECS - 1))
		jupiter_debug(peer,
		    "jupiter_parse_t: gps sweek jumped (was %d, now %d)\n",
		    up->lastsweek, sweek);
	up->lastsweek = sweek;

	/* This timecode describes next pulse */
	last_timecode = up->timecode;
	up->timecode = (u_int32)JAN_1970 +
	    GPS_EPOCH + (up->gweek * WEEKSECS) + sweek;

	if (last_timecode == 0)
		/* XXX debugging */
		jupiter_debug(peer,
		    "jupiter_parse_t: UTC <none> (gweek/sweek %u/%u)\n",
		    up->gweek, sweek);
	else {
		/* XXX debugging */
		t = last_timecode - (u_int32)JAN_1970;
		tm = gmtime(&t);
		cp = asctime(tm);

		jupiter_debug(peer,
		    "jupiter_parse_t: UTC %.24s (gweek/sweek %u/%u)\n",
		    cp, up->gweek, sweek);

		/* Billboard last_timecode (which is now the current time) */
		jt = &cal;
		caljulian(last_timecode, jt);
		pp = peer->procptr;
		pp->year = jt->year;
		pp->day = jt->yearday;
		pp->hour = jt->hour;
		pp->minute = jt->minute;
		pp->second = jt->second;
		pp->msec = 0;
		pp->usec = 0;
	}

	/* XXX debugging */
	tm = gmtime(&up->ppsev.tv.tv_sec);
	cp = asctime(tm);
	flags = getshort(jp->flags);
	jupiter_debug(peer,
	    "jupiter_parse_t: PPS %.19s.%06lu %.4s (serial %u)%s\n",
	    cp, up->ppsev.tv.tv_usec, cp + 20, up->ppsev.serial,
	    (flags & JUPITER_O_PULSE_VALID) == 0 ?
	    " NOT VALID" : "");

	/* Toss if not designated "valid" by the gps */
	if ((flags & JUPITER_O_PULSE_VALID) == 0) {
		refclock_report(peer, CEVNT_BADTIME);
		return ("time mark not valid");
	}

	/* We better be sync'ed to UTC... */
	if ((flags & JUPITER_O_PULSE_UTC) == 0) {
		refclock_report(peer, CEVNT_BADTIME);
		return ("time mark not sync'ed to UTC");
	}

	return (NULL);
}

/*
 * Process a PPS signal, returning a timestamp.
 */
static int
jupiter_pps(register struct peer *peer)
{
	register struct refclockproc *pp;
	register struct jupiterunit *up;
	register int firsttime;
	struct timeval ntp_tv;

	pp = peer->procptr;
	up = (struct jupiterunit *)pp->unitptr;

	/*
	 * Grab the timestamp of the PPS signal.
	 */
	firsttime = (up->ppsev.tv.tv_sec == 0);
	if (ioctl(pp->io.fd, CIOGETEV, (caddr_t)&up->ppsev) < 0) {
		/* XXX Actually, if this fails, we're pretty much screwed */
		jupiter_debug(peer, "jupiter_pps: CIOGETEV: %s\n",
		    strerror(errno));
		refclock_report(peer, CEVNT_FAULT);
		return (1);
	}

	/*
	 * Check pps serial number against last one
	 */
	if (!firsttime && up->lastserial + 1 != up->ppsev.serial) {
		if (up->ppsev.serial == up->lastserial)
			jupiter_debug(peer, "jupiter_pps: no new pps event\n");
		else
			jupiter_debug(peer,
			    "jupiter_pps: missed %d pps events\n",
				up->ppsev.serial - up->lastserial - 1);
		up->lastserial = up->ppsev.serial;
		refclock_report(peer, CEVNT_FAULT);
		return (1);
	}
	up->lastserial = up->ppsev.serial;

	/*
	 * Return the timestamp in pp->lastrec
	 */
	ntp_tv = up->ppsev.tv;
	ntp_tv.tv_sec += (u_int32)JAN_1970;
	TVTOTS(&ntp_tv, &pp->lastrec);

	return (0);
}

/*
 * jupiter_debug - print debug messages
 */
#if defined(__STDC__)
static void
jupiter_debug(struct peer *peer, char *fmt, ...)
#else
static void
jupiter_debug(peer, fmt, va_alist)
	struct peer *peer;
	char *fmt;
#endif /* __STDC__ */
{
	va_list ap;

	if (debug) {

#if defined(__STDC__)
		va_start(ap, fmt);
#else
		va_start(ap);
#endif /* __STDC__ */
		/*
		 * Print debug message to stdout
		 * In the future, we may want to get get more creative...
		 */
		vfprintf(stderr, fmt, ap);

		va_end(ap);
	}
}

/* Checksum and transmit a message to the Jupiter */
static char *
jupiter_send(register struct peer *peer, register struct jheader *hp)
{
	register u_int len, size;
	register int cc;
	register u_short *sp;
	static char errstr[132];

	size = sizeof(*hp);
	hp->hsum = putshort(jupiter_cksum((u_short *)hp,
	    (size / sizeof(u_short)) - 1));
	len = getshort(hp->len);
	if (len > 0) {
		sp = (u_short *)(hp + 1);
		sp[len] = putshort(jupiter_cksum(sp, len));
		size += (len + 1) * sizeof(u_short);
	}

	if ((cc = write(peer->procptr->io.fd, (char *)hp, size)) < 0) {
		(void)sprintf(errstr, "write: %s", strerror(errno));
		return (errstr);
	} else if (cc != size) {
		(void)sprintf(errstr, "short write (%d != %d)", cc, size);
		return (errstr);
	}
	return (NULL);
}

/* Request periodic message output */
static struct {
	struct jheader jheader;
	struct jrequest jrequest;
} reqmsg = {
	{ putshort(JUPITER_SYNC), 0,
	    putshort((sizeof(struct jrequest) / sizeof(u_short)) - 1),
	    0, putshort(JUPITER_FLAG_REQUEST | JUPITER_FLAG_NAK |
	    JUPITER_FLAG_CONN | JUPITER_FLAG_LOG), 0 },
	{ 0, 0, 0, 0 }
};

/* An interval of zero means to output on trigger */
static void
jupiter_reqmsg(register struct peer *peer, register u_int id,
    register u_int interval)
{
	register struct jheader *hp;
	register struct jrequest *rp;
	register char *cp;

	hp = &reqmsg.jheader;
	hp->id = putshort(id);
	rp = &reqmsg.jrequest;
	rp->trigger = putshort(interval == 0);
	rp->interval = putshort(interval);
	if ((cp = jupiter_send(peer, hp)) != NULL)
		jupiter_debug(peer, "jupiter_reqmsg: %u: %s\n", id, cp);
}

/* Cancel periodic message output */
static struct jheader canmsg = {
	putshort(JUPITER_SYNC), 0, 0, 0,
	putshort(JUPITER_FLAG_REQUEST | JUPITER_FLAG_NAK | JUPITER_FLAG_DISC),
	0
};

static void
jupiter_canmsg(register struct peer *peer, register u_int id)
{
	register struct jheader *hp;
	register char *cp;

	hp = &canmsg;
	hp->id = putshort(id);
	if ((cp = jupiter_send(peer, hp)) != NULL)
		jupiter_debug(peer, "jupiter_canmsg: %u: %s\n", id, cp);
}

/* Request a single message output */
static struct jheader reqonemsg = {
	putshort(JUPITER_SYNC), 0, 0, 0,
	putshort(JUPITER_FLAG_REQUEST | JUPITER_FLAG_NAK | JUPITER_FLAG_QUERY),
	0
};

static void
jupiter_reqonemsg(register struct peer *peer, register u_int id)
{
	register struct jheader *hp;
	register char *cp;

	hp = &reqonemsg;
	hp->id = putshort(id);
	if ((cp = jupiter_send(peer, hp)) != NULL)
		jupiter_debug(peer, "jupiter_reqonemsg: %u: %s\n", id, cp);
}

/* Set the platform dynamics */
static struct {
	struct jheader jheader;
	struct jplat jplat;
} platmsg = {
	{ putshort(JUPITER_SYNC), putshort(JUPITER_I_PLAT),
	    putshort((sizeof(struct jplat) / sizeof(u_short)) - 1), 0,
	    putshort(JUPITER_FLAG_REQUEST | JUPITER_FLAG_NAK), 0 },
	{ 0, 0, 0 }
};

static void
jupiter_platform(register struct peer *peer, register u_int platform)
{
	register struct jheader *hp;
	register struct jplat *pp;
	register char *cp;

	hp = &platmsg.jheader;
	pp = &platmsg.jplat;
	pp->platform = putshort(platform);
	if ((cp = jupiter_send(peer, hp)) != NULL)
		jupiter_debug(peer, "jupiter_platform: %u: %s\n", platform, cp);
}

/* Checksum "len" shorts */
static u_short
jupiter_cksum(register u_short *sp, register u_int len)
{
	register u_short sum, x;

	sum = 0;
	while (len-- > 0) {
		x = *sp++;
		sum += getshort(x);
	}
	return (~sum + 1);
}

/* Return the size of the next message (or zero if we don't have it all yet) */
static int
jupiter_recv(register struct peer *peer)
{
	register int n, len, size, cc;
	register struct refclockproc *pp;
	register struct jupiterunit *up;
	register struct jheader *hp;
	register u_char *bp;
	register u_short *sp;

	pp = peer->procptr;
	up = (struct jupiterunit *)pp->unitptr;

	/* Must have at least a header's worth */
	cc = sizeof(*hp);
	size = up->ssize;
	if (size < cc)
		return (0);

	/* Search for the sync short if missing */
	sp = up->sbuf;
	hp = (struct jheader *)sp;
	if (getshort(hp->sync) != JUPITER_SYNC) {
		/* Wasn't at the front, sync up */
		jupiter_debug(peer, "syncing");
		bp = (u_char *)sp;
		n = size;
		while (n >= 2) {
			if (bp[0] != (JUPITER_SYNC & 0xff)) {
				jupiter_debug(peer, "{0x%x}", bp[0]);
				++bp;
				--n;
				continue;
			}
			if (bp[1] == ((JUPITER_SYNC >> 8) & 0xff))
				break;
			jupiter_debug(peer, "{0x%x 0x%x}", bp[0], bp[1]);
			bp += 2;
			n -= 2;
		}
		jupiter_debug(peer, "\n");
		/* Shuffle data to front of input buffer */
		if (n > 0)
			memcpy(sp, bp, n);
		size = n;
		up->ssize = size;
		if (size < cc || hp->sync != JUPITER_SYNC)
			return (0);
	}

	if (jupiter_cksum(sp, (cc / sizeof(u_short) - 1)) !=
	    getshort(hp->hsum)) {
	    jupiter_debug(peer, "jupiter_recv: bad header checksum!\n");
		/* This is drastic but checksum errors should be rare */
		up->ssize = 0;
		return (0);
	}

	/* Check for a payload */
	len = getshort(hp->len);
	if (len > 0) {
		n = (len + 1) * sizeof(u_short);
		/* Not enough data yet */
		if (size < cc + n)
			return (0);

		/* Check payload checksum */
		sp = (u_short *)(hp + 1);
		if (jupiter_cksum(sp, len) != getshort(sp[len])) {
			jupiter_debug(peer,
			    "jupiter_recv: bad payload checksum!\n");
			/* This is drastic but checksum errors should be rare */
			up->ssize = 0;
			return (0);
		}
		cc += n;
	}
	return (cc);
}

static int
jupiter_ttyinit(register struct peer *peer, register int fd)
{
	struct termios termios;

	memset((char *)&termios, 0, sizeof(termios));
	if (cfsetispeed(&termios, B9600) < 0 ||
	    cfsetospeed(&termios, B9600) < 0) {
		jupiter_debug(peer,
		    "jupiter_ttyinit: cfsetispeed/cfgetospeed: %s\n",
		    strerror(errno));
		return (0);
	}
#ifdef HAVE_CFMAKERAW
	cfmakeraw(&termios);
#else
	termios.c_iflag &= ~(IMAXBEL | IXOFF | INPCK | BRKINT | PARMRK |
	    ISTRIP | INLCR | IGNCR | ICRNL | IXON | IGNPAR);
	termios.c_iflag |= IGNBRK;
	termios.c_oflag &= ~OPOST;
	termios.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ICANON | ISIG |
	    IEXTEN | NOFLSH | TOSTOP | PENDIN);
	termios.c_cflag &= ~(CSIZE | PARENB);
	termios.c_cflag |= CS8 | CREAD;
	termios.c_cc[VMIN] = 1;
#endif
	termios.c_cflag |= CLOCAL;
	if (tcsetattr(fd, TCSANOW, &termios) < 0) {
		jupiter_debug(peer, "jupiter_ttyinit: tcsetattr: %s\n",
		    strerror(errno));
		return (0);
	}

#ifdef TIOCSPPS
	if (ioctl(fd, TIOCSPPS, (char *)&fdpps) < 0) {
		jupiter_debug(peer, "jupiter_ttyinit: TIOCSPPS: %s\n",
		    strerror(errno));
		return (0);
	}
#endif
#ifdef I_PUSH
	if (ioctl(fd, I_PUSH, "ppsclock") < 0) {
		jupiter_debug(peer, "jupiter_ttyinit: push ppsclock: %s\n",
		    strerror(errno));
		return (0);
	}
#endif

	return (1);
}

#else /* not (REFCLOCK && CLOCK_JUPITER && PPS) */
int refclock_jupiter_bs;
#endif /* not (REFCLOCK && CLOCK_JUPITER && PPS) */
