/*
 * Copyright (c) 1997, 1998, 2003
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

/* This clock *REQUIRES* the PPS API to be available */
#if defined(REFCLOCK) && defined(CLOCK_JUPITER) && defined(HAVE_PPSAPI)

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_refclock.h"
#include "ntp_unixtime.h"
#include "ntp_stdlib.h"
#include "ntp_calendar.h"
#include "ntp_calgps.h"
#include "timespecops.h"

#include <stdio.h>
#include <ctype.h>

#include "jupiter.h"
#include "ppsapi_timepps.h"

#ifdef WORDS_BIGENDIAN
#define getshort(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))
#define putshort(s) ((((s) & 0xff) << 8) | (((s) >> 8) & 0xff))
#else
#define getshort(s) ((u_short)(s))
#define putshort(s) ((u_short)(s))
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
 * Radio interface parameters
 */
#define	PRECISION	(-18)	/* precision assumed (about 4 us) */
#define	REFID	"GPS\0"		/* reference id */
#define	DESCRIPTION	"Rockwell Jupiter GPS Receiver" /* who we are */
#define	DEFFUDGETIME	0	/* default fudge time (ms) */

/* Unix timestamp for the GPS epoch: January 6, 1980 */
#define GPS_EPOCH 315964800

/* Rata Die Number of first day of GPS epoch. This is the number of days
 * since 0000-12-31 to 1980-01-06 in the proleptic Gregorian Calendar.
 */
#define RDN_GPS_EPOCH (4*146097 + 138431 + 1)

/* Double short to unsigned int */
#define DS2UI(p) ((getshort((p)[1]) << 16) | getshort((p)[0]))

/* Double short to signed int */
#define DS2I(p) ((getshort((p)[1]) << 16) | getshort((p)[0]))

/* One week's worth of seconds */
#define WEEKSECS (7 * 24 * 60 * 60)

/*
 * Jupiter unit control structure.
 */
struct instance {
	struct peer *peer;		/* peer */

	pps_params_t pps_params;	/* pps parameters */
	pps_info_t pps_info;		/* last pps data */
	pps_handle_t pps_handle;	/* pps handle */
	u_int	assert;			/* pps edge to use */
	u_int	hardpps;		/* enable kernel mode */
	l_fp 		rcv_pps;	/* last pps timestamp */
	l_fp	        rcv_next;	/* rcv time of next reftime */
	TGpsDatum	ref_next;	/* next GPS time stamp to use with PPS */
	TGpsDatum	piv_next;	/* pivot for week date unfolding */
	uint16_t	piv_hold;	/* TTL for pivot value */
	uint16_t	rcvtout;	/* receive timeout ticker */
	int wantid;			/* don't reconfig on channel id msg */
	u_int  moving;			/* mobile platform? */
	u_char sloppyclockflag;		/* fudge flags */
	u_short sbuf[512];		/* local input buffer */
	int ssize;			/* space used in sbuf */
};

/*
 * Function prototypes
 */
static	void	jupiter_canmsg	(struct instance * const, u_int);
static	u_short	jupiter_cksum	(u_short *, u_int);
static	int	jupiter_config	(struct instance * const);
static	void	jupiter_debug	(struct peer *, const char *,
				 const char *, ...) NTP_PRINTF(3, 4);
static	const char *	jupiter_parse_t	(struct instance * const, u_short *, l_fp);
static	const char *	jupiter_parse_gpos(struct instance * const, u_short *);
static	void	jupiter_platform(struct instance * const, u_int);
static	void	jupiter_poll	(int, struct peer *);
static	void	jupiter_control	(int, const struct refclockstat *,
				 struct refclockstat *, struct peer *);
static	int	jupiter_ppsapi	(struct instance * const);
static	int	jupiter_pps	(struct instance * const);
static	int	jupiter_recv	(struct instance * const);
static	void	jupiter_receive (struct recvbuf * const rbufp);
static	void	jupiter_reqmsg	(struct instance * const, u_int, u_int);
static	void	jupiter_reqonemsg(struct instance * const, u_int);
static	char *	jupiter_send	(struct instance * const, struct jheader *);
static	void	jupiter_shutdown(int, struct peer *);
static	int	jupiter_start	(int, struct peer *);
static	void	jupiter_ticker	(int, struct peer *);

/*
 * Transfer vector
 */
struct	refclock refclock_jupiter = {
	jupiter_start,		/* start up driver */
	jupiter_shutdown,	/* shut down driver */
	jupiter_poll,		/* transmit poll message */
	jupiter_control,	/* (clock control) */
	noentry,		/* (clock init) */
	noentry,		/* (clock buginfo) */
	jupiter_ticker		/* 1HZ ticker */
};

/*
 * jupiter_start - open the devices and initialize data for processing
 */
static int
jupiter_start(
	int unit,
	struct peer *peer
	)
{
	struct refclockproc * const pp = peer->procptr;
	struct instance * up;
	int fd;
	char gpsdev[20];

	/*
	 * Open serial port
	 */
	snprintf(gpsdev, sizeof(gpsdev), DEVICE, unit);
	fd = refclock_open(&peer->srcadr, gpsdev, SPEED232, LDISC_RAW);
	if (fd <= 0) {
		jupiter_debug(peer, "jupiter_start", "open %s: %m",
			      gpsdev);
		return (0);
	}

	/* Allocate unit structure */
	up = emalloc_zero(sizeof(*up));
	up->peer = peer;
	pp->io.clock_recv = jupiter_receive;
	pp->io.srcclock = peer;
	pp->io.datalen = 0;
	pp->io.fd = fd;
	if (!io_addclock(&pp->io)) {
		close(fd);
		pp->io.fd = -1;
		free(up);
		return (0);
	}
	pp->unitptr = up;

	/*
	 * Initialize miscellaneous variables
	 */
	peer->precision = PRECISION;
	pp->clockdesc = DESCRIPTION;
	memcpy((char *)&pp->refid, REFID, 4);

	up->assert = 1;
	up->hardpps = 0;
	/*
	 * Start the PPSAPI interface if it is there. Default to use
	 * the assert edge and do not enable the kernel hardpps.
	 */
	if (time_pps_create(fd, &up->pps_handle) < 0) {
		up->pps_handle = 0;
		msyslog(LOG_ERR,
			"refclock_jupiter: time_pps_create failed: %m");
	}
	else if (!jupiter_ppsapi(up))
		goto clean_up;
	
	/* Ensure the receiver is properly configured */
	if (!jupiter_config(up))
		goto clean_up;

	jupiter_pps(up);	/* get current PPS state */
	return (1);

clean_up:
	jupiter_shutdown(unit, peer);
	pp->unitptr = 0;
	return (0);
}

/*
 * jupiter_shutdown - shut down the clock
 */
static void
jupiter_shutdown(int unit, struct peer *peer)
{
	struct refclockproc * const pp = peer->procptr;
	struct instance *     const up = pp->unitptr;
	
	if (!up)
		return;

	if (up->pps_handle) {
		time_pps_destroy(up->pps_handle);
		up->pps_handle = 0;
	}

	if (pp->io.fd != -1)
		io_closeclock(&pp->io);
	free(up);
}

/*
 * jupiter_config - Configure the receiver
 */
static int
jupiter_config(struct instance * const up)
{
	jupiter_debug(up->peer, __func__, "init receiver");

	/*
	 * Initialize the unit variables
	 */
	up->sloppyclockflag = up->peer->procptr->sloppyclockflag;
	up->moving = !!(up->sloppyclockflag & CLK_FLAG2);
	if (up->moving)
		jupiter_debug(up->peer, __func__, "mobile platform");

	ZERO(up->rcv_next);
	ZERO(up->ref_next);
	ZERO(up->piv_next);
	up->ssize = 0;

	/* Stop outputting all messages */
	jupiter_canmsg(up, JUPITER_ALL);

	/* Request the receiver id so we can syslog the firmware version */
	jupiter_reqonemsg(up, JUPITER_O_ID);

	/* Flag that this the id was requested (so we don't get called again) */
	up->wantid = 1;

	/* Request perodic time mark pulse messages */
	jupiter_reqmsg(up, JUPITER_O_PULSE, 1);

	/* Request perodic geodetic position status */
	jupiter_reqmsg(up, JUPITER_O_GPOS, 1);

	/* Set application platform type */
	if (up->moving)
		jupiter_platform(up, JUPITER_I_PLAT_MED);
	else
		jupiter_platform(up, JUPITER_I_PLAT_LOW);

	return (1);
}

static void
jupiter_checkpps(
	struct refclockproc * const pp,
	struct instance *     const up
	)
{
	l_fp		tstamp, delta;
	struct calendar	cd;
	
	if (jupiter_pps(up) || !up->piv_next.weeks)
		return;

	/* check delay between pulse message and pulse. */
	delta = up->rcv_pps;		/* set by jupiter_pps() */
	L_SUB(&delta, &up->rcv_next);	/* recv time pulse message */
	if (delta.l_ui != 0 || delta.l_uf >= 0xC0000000) {
		up->ref_next.weeks = 0;	/* consider as consumed... */
		return;
	}
	
	pp->lastrec = up->rcv_pps;
	tstamp = ntpfp_from_gpsdatum(&up->ref_next);
	refclock_process_offset(pp, tstamp, up->rcv_pps, pp->fudgetime1);
	up->rcvtout = 2;

	gpscal_to_calendar(&cd, &up->ref_next);
	refclock_save_lcode(pp, ntpcal_iso8601std(NULL, 0, &cd),
			    (size_t)-1);
	up->ref_next.weeks = 0;	/* consumed... */
}

/*
 * jupiter_ticker - process periodic checks
 */
static void
jupiter_ticker(int unit, struct peer *peer)
{
	struct refclockproc * const pp = peer->procptr;
	struct instance *     const up = pp->unitptr;

	if (!up)
		return;

	/* check if we can add another sample now */
	jupiter_checkpps(pp, up);

	/* check the pivot update cycle */
	if (up->piv_hold && !--up->piv_hold)
		ZERO(up->piv_next);

	if (up->rcvtout)
		--up->rcvtout;
	else if (pp->coderecv != pp->codeproc)
		refclock_samples_expire(pp, 1);
}

/*
 * Initialize PPSAPI
 */
int
jupiter_ppsapi(
	struct instance * const up	/* unit structure pointer */
	)
{
	int capability;

	if (time_pps_getcap(up->pps_handle, &capability) < 0) {
		msyslog(LOG_ERR,
		    "refclock_jupiter: time_pps_getcap failed: %m");
		return (0);
	}
	memset(&up->pps_params, 0, sizeof(pps_params_t));
	if (!up->assert)
		up->pps_params.mode = capability & PPS_CAPTURECLEAR;
	else
		up->pps_params.mode = capability & PPS_CAPTUREASSERT;
	if (!(up->pps_params.mode & (PPS_CAPTUREASSERT | PPS_CAPTURECLEAR))) {
		msyslog(LOG_ERR,
		    "refclock_jupiter: invalid capture edge %d",
		    up->assert);
		return (0);
	}
	up->pps_params.mode |= PPS_TSFMT_TSPEC;
	if (time_pps_setparams(up->pps_handle, &up->pps_params) < 0) {
		msyslog(LOG_ERR,
		    "refclock_jupiter: time_pps_setparams failed: %m");
		return (0);
	}
	if (up->hardpps) {
		if (time_pps_kcbind(up->pps_handle, PPS_KC_HARDPPS,
				    up->pps_params.mode & ~PPS_TSFMT_TSPEC,
				    PPS_TSFMT_TSPEC) < 0) {
			msyslog(LOG_ERR,
			    "refclock_jupiter: time_pps_kcbind failed: %m");
			return (0);
		}
		hardpps_enable = 1;
	}
/*	up->peer->precision = PPS_PRECISION; */

#if DEBUG
	if (debug) {
		time_pps_getparams(up->pps_handle, &up->pps_params);
		jupiter_debug(up->peer, __func__,
			"pps capability 0x%x version %d mode 0x%x kern %d",
			capability, up->pps_params.api_version,
			up->pps_params.mode, up->hardpps);
	}
#endif

	return (1);
}

/*
 * Get PPSAPI timestamps.
 *
 * Return 0 on failure and 1 on success.
 */
static int
jupiter_pps(struct instance * const up)
{
	pps_info_t pps_info;
	struct timespec timeout, ts;
	l_fp tstmp;

	/*
	 * Convert the timespec nanoseconds field to ntp l_fp units.
	 */ 
	if (up->pps_handle == 0)
		return 1;
	timeout.tv_sec = 0;
	timeout.tv_nsec = 0;
	memcpy(&pps_info, &up->pps_info, sizeof(pps_info_t));
	if (time_pps_fetch(up->pps_handle, PPS_TSFMT_TSPEC, &up->pps_info,
	    &timeout) < 0)
		return 1;
	if (up->pps_params.mode & PPS_CAPTUREASSERT) {
		if (pps_info.assert_sequence ==
		    up->pps_info.assert_sequence)
			return 1;
		ts = up->pps_info.assert_timestamp;
	} else if (up->pps_params.mode & PPS_CAPTURECLEAR) {
		if (pps_info.clear_sequence ==
		    up->pps_info.clear_sequence)
			return 1;
		ts = up->pps_info.clear_timestamp;
	} else {
		return 1;
	}

	tstmp = tspec_stamp_to_lfp(ts);		
	if (L_ISEQU(&tstmp, &up->rcv_pps))
		return 1;

	up->rcv_pps = tstmp;
	return 0;
}

/*
 * jupiter_poll - jupiter watchdog routine
 */
static void
jupiter_poll(int unit, struct peer *peer)
{
	struct refclockproc * const pp = peer->procptr;
	struct instance *     const up = pp->unitptr;

	pp->polls++;

	/*
	 * If we have new samples since last poll, everything is fine.
	 * if not, blarb loudly.
	 */
	if (pp->coderecv != pp->codeproc) {
		refclock_receive(peer);
		refclock_report(peer, CEVNT_NOMINAL);
	} else {
		refclock_report(peer, CEVNT_TIMEOUT);

		/* Request the receiver id to trigger a reconfig */
		jupiter_reqonemsg(up, JUPITER_O_ID);
		up->wantid = 0;
	}
}

/*
 * jupiter_control - fudge control
 */
static void
jupiter_control(
	int unit,		/* unit (not used) */
	const struct refclockstat *in, /* input parameters (not used) */
	struct refclockstat *out, /* output parameters (not used) */
	struct peer *peer	/* peer structure pointer */
	)
{
	struct refclockproc * const pp = peer->procptr;
	struct instance *     const up = pp->unitptr;
	
	u_char sloppyclockflag;

	up->assert = !(pp->sloppyclockflag & CLK_FLAG3);
	jupiter_ppsapi(up);

	sloppyclockflag = up->sloppyclockflag;
	up->sloppyclockflag = pp->sloppyclockflag;
	if ((up->sloppyclockflag & CLK_FLAG2) !=
	    (sloppyclockflag & CLK_FLAG2)) {
		jupiter_debug(peer, __func__,
		    "mode switch: reset receiver");
		jupiter_config(up);
		return;
	}
}

/*
 * jupiter_receive - receive gps data
 * Gag me!
 */
static void
jupiter_receive(struct recvbuf * const rbufp)
{
	struct peer *         const peer = rbufp->recv_peer;
	struct refclockproc * const pp   = peer->procptr;
	struct instance *     const up   = pp->unitptr;

	size_t bpcnt;
	int cc, size;
	const char *cp;
	u_char *bp;
	u_short *sp;
	struct jid *ip;
	struct jheader *hp;

	/* Initialize pointers and read the timecode and timestamp */
	bp = (u_char *)rbufp->recv_buffer;
	bpcnt = rbufp->recv_length;

	/* This shouldn't happen */
	if (bpcnt > sizeof(up->sbuf) - up->ssize)
		bpcnt = sizeof(up->sbuf) - up->ssize;

	/* Append to input buffer */
	memcpy((u_char *)up->sbuf + up->ssize, bp, bpcnt);
	up->ssize += bpcnt;

	/* While there's at least a header and we parse an intact message */
	while (up->ssize > (int)sizeof(*hp) && (cc = jupiter_recv(up)) > 0) {
		hp = (struct jheader *)up->sbuf;
		sp = (u_short *)(hp + 1);
		size = cc - sizeof(*hp);
		switch (getshort(hp->id)) {

		case JUPITER_O_PULSE:
			/* first see if we can push another sample: */
			jupiter_checkpps(pp, up);

			if (size != sizeof(struct jpulse)) {
				jupiter_debug(peer, __func__,
				    "pulse: len %d != %u",
				    size, (int)sizeof(struct jpulse));
				refclock_report(peer, CEVNT_BADREPLY);
				break;
			}
 			
			/* Parse timecode (even when there's no pps)
			 *
			 * There appears to be a firmware bug related to
			 * the pulse message; in addition to the one per
			 * second messages, we get an extra pulse
			 * message once an hour (on the anniversary of
			 * the cold start). It seems to come 200 ms
			 * after the one requested.
			 *
			 * But since we feed samples only when a new PPS
			 * pulse is found we can simply ignore that and
			 * aggregate/update any existing timing message.
			 */
			if ((cp = jupiter_parse_t(up, sp, rbufp->recv_time)) != NULL) {
				jupiter_debug(peer, __func__,
				    "pulse: %s", cp);
			}
			break;

		case JUPITER_O_GPOS:
			if (size != sizeof(struct jgpos)) {
				jupiter_debug(peer, __func__,
				    "gpos: len %d != %u",
				    size, (int)sizeof(struct jgpos));
				refclock_report(peer, CEVNT_BADREPLY);
				break;
			}

			if ((cp = jupiter_parse_gpos(up, sp)) != NULL) {
				jupiter_debug(peer, __func__,
				    "gpos: %s", cp);
				break;
			}
			break;

		case JUPITER_O_ID:
			if (size != sizeof(struct jid)) {
				jupiter_debug(peer, __func__,
				    "id: len %d != %u",
				    size, (int)sizeof(struct jid));
				refclock_report(peer, CEVNT_BADREPLY);
				break;
			}
			/*
			 * If we got this message because the Jupiter
			 * just powered instance, it needs to be reconfigured.
			 */
			ip = (struct jid *)sp;
			jupiter_debug(peer, __func__,
			    "%s chan ver %s, %s (%s)",
			    ip->chans, ip->vers, ip->date, ip->opts);
			msyslog(LOG_DEBUG,
			    "jupiter_receive: %s chan ver %s, %s (%s)",
			    ip->chans, ip->vers, ip->date, ip->opts);
			if (up->wantid)
				up->wantid = 0;
			else {
				jupiter_debug(peer, __func__, "reset receiver");
				jupiter_config(up);
				/*
				 * Restore since jupiter_config() just
				 * zeroed it
				 */
				up->ssize = cc;
			}
			break;

		default:
			jupiter_debug(peer, __func__, "unknown message id %d",
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
}

static const char *
jupiter_parse_t(
	struct instance * const up,
	u_short *               sp,
	l_fp               rcvtime
	)
{
	struct jpulse *jp;
	u_int32 sweek;
	u_short flags;
	l_fp fofs;
	
	jp = (struct jpulse *)sp;
	flags = getshort(jp->flags);

	/* Toss if not designated "valid" by the gps.
	 * !!NOTE!! do *not* kill data received so far!
	 */
	if ((flags & JUPITER_O_PULSE_VALID) == 0) {
		refclock_report(up->peer, CEVNT_BADTIME);
		return ("time mark not valid");
	}

	up->rcv_next = rcvtime; /* remember when this happened */
	
	/* The timecode is presented as seconds into the current GPS week */
	sweek = DS2UI(jp->sweek) % WEEKSECS;
	/* check if we have to apply the UTC offset ourselves */
	if ((flags & JUPITER_O_PULSE_UTC) == 0) {
		struct timespec tofs;
		tofs.tv_sec  = getshort(jp->offs);
		tofs.tv_nsec = DS2I(jp->offns);
		fofs = tspec_intv_to_lfp(tofs);
		L_NEG(&fofs);
	} else {
		ZERO(fofs);
	}
	
	/*
	 * If we don't know the current GPS week, calculate it from the
	 * current time. (It's too bad they didn't include this
	 * important value in the pulse message).
	 * 
	 * So we pick the pivot value from the other messages like gpos
	 * or chan if we can. Of course, the PULSE message can be in UTC
	 * or GPS time scale, and the other messages are simply always
	 * GPS time.
	 *
	 * But as long as the difference between the time stamps is less
	 * than a half week, the unfolding of a week time is unambigeous
	 * and well suited for the problem we have here. And we won't
	 * see *that* many leap seconds, ever.
	 */
	if (up->piv_next.weeks) {
		up->ref_next = gpscal_from_weektime2(
			sweek, fofs, &up->piv_next);
		up->piv_next = up->ref_next;
	} else {
		up->ref_next = gpscal_from_weektime1(
			sweek, fofs, rcvtime);
	}
			


	return (NULL);
}

static const char *
jupiter_parse_gpos(
	struct instance * const up,
	u_short *               sp
	)
{
	struct jgpos *jg;
	struct calendar	tref;
	char *cp;
	struct timespec tofs;
	uint16_t	raw_week;
	uint32_t	raw_secs;

	jg = (struct jgpos *)sp;

	if (jg->navval != 0) {
		/*
		 * Solution not valid. Use caution and refuse
		 * to determine GPS week from this message.
		 */
		return ("Navigation solution not valid");
	}

	raw_week = getshort(jg->gweek);
	raw_secs = DS2UI(jg->sweek);
	tofs.tv_sec  = 0;
	tofs.tv_nsec = DS2UI(jg->nsweek);
	up->piv_next = gpscal_from_gpsweek(raw_week, raw_secs,
					   tspec_intv_to_lfp(tofs));
	up->piv_hold = 60;

	gpscal_to_calendar(&tref, &up->piv_next);
	cp = ntpcal_iso8601std(NULL, 0, &tref);
	jupiter_debug(up->peer, __func__,
		"GPS %s (gweek/sweek %hu/%u)",
		      cp, (unsigned short)raw_week, (unsigned int)raw_secs);
	return (NULL);
}

/*
 * jupiter_debug - print debug messages
 */
static void
jupiter_debug(
	struct peer *	peer,
	const char *	function,
	const char *	fmt,
	...
	)
{
	char	buffer[200];
	va_list	ap;

	va_start(ap, fmt);
	/*
	 * Print debug message to stdout
	 * In the future, we may want to get get more creative...
	 */
	mvsnprintf(buffer, sizeof(buffer), fmt, ap);
	record_clock_stats(&peer->srcadr, buffer);
#ifdef DEBUG
	if (debug) {
		printf("%s: %s\n", function, buffer);
		fflush(stdout);
	}
#endif

	va_end(ap);
}

/* Checksum and transmit a message to the Jupiter */
static char *
jupiter_send(
	struct instance * const up,
	struct jheader *        hp
	)
{
	u_int len, size;
	ssize_t cc;
	u_short *sp;
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

	if ((cc = write(up->peer->procptr->io.fd, (char *)hp, size)) < 0) {
		msnprintf(errstr, sizeof(errstr), "write: %m");
		return (errstr);
	} else if (cc != (int)size) {
		snprintf(errstr, sizeof(errstr), "short write (%zd != %u)", cc, size);
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
	    0, JUPITER_FLAG_REQUEST | JUPITER_FLAG_NAK |
	    JUPITER_FLAG_CONN | JUPITER_FLAG_LOG, 0 },
	{ 0, 0, 0, 0 }
};

/* An interval of zero means to output on trigger */
static void
jupiter_reqmsg(
	struct instance * const up,
	u_int                   id,
	u_int             interval
	)
{
	struct jheader *hp;
	struct jrequest *rp;
	char *cp;

	hp = &reqmsg.jheader;
	hp->id = putshort(id);
	rp = &reqmsg.jrequest;
	rp->trigger = putshort(interval == 0);
	rp->interval = putshort(interval);
	if ((cp = jupiter_send(up, hp)) != NULL)
		jupiter_debug(up->peer, __func__, "%u: %s", id, cp);
}

/* Cancel periodic message output */
static struct jheader canmsg = {
	putshort(JUPITER_SYNC), 0, 0, 0,
	JUPITER_FLAG_REQUEST | JUPITER_FLAG_NAK | JUPITER_FLAG_DISC,
	0
};

static void
jupiter_canmsg(
	struct instance * const up,
	u_int                   id
	)
{
	struct jheader *hp;
	char *cp;

	hp = &canmsg;
	hp->id = putshort(id);
	if ((cp = jupiter_send(up, hp)) != NULL)
		jupiter_debug(up->peer, __func__, "%u: %s", id, cp);
}

/* Request a single message output */
static struct jheader reqonemsg = {
	putshort(JUPITER_SYNC), 0, 0, 0,
	JUPITER_FLAG_REQUEST | JUPITER_FLAG_NAK | JUPITER_FLAG_QUERY,
	0
};

static void
jupiter_reqonemsg(
	struct instance * const up,
	u_int                   id
	)
{
	struct jheader *hp;
	char *cp;

	hp = &reqonemsg;
	hp->id = putshort(id);
	if ((cp = jupiter_send(up, hp)) != NULL)
		jupiter_debug(up->peer, __func__, "%u: %s", id, cp);
}

/* Set the platform dynamics */
static struct {
	struct jheader jheader;
	struct jplat jplat;
} platmsg = {
	{ putshort(JUPITER_SYNC), putshort(JUPITER_I_PLAT),
	    putshort((sizeof(struct jplat) / sizeof(u_short)) - 1), 0,
	    JUPITER_FLAG_REQUEST | JUPITER_FLAG_NAK, 0 },
	{ 0, 0, 0 }
};

static void
jupiter_platform(
	struct instance * const up,
	u_int             platform
	)
{
	struct jheader *hp;
	struct jplat *pp;
	char *cp;

	hp = &platmsg.jheader;
	pp = &platmsg.jplat;
	pp->platform = putshort(platform);
	if ((cp = jupiter_send(up, hp)) != NULL)
		jupiter_debug(up->peer, __func__, "%u: %s", platform, cp);
}

/* Checksum "len" shorts */
static u_short
jupiter_cksum(u_short *sp, u_int len)
{
	u_short sum, x;

	sum = 0;
	while (len-- > 0) {
		x = *sp++;
		sum += getshort(x);
	}
	return (~sum + 1);
}

/* Return the size of the next message (or zero if we don't have it all yet) */
static int
jupiter_recv(
	struct instance * const up
	)
{
	int n, len, size, cc;
	struct jheader *hp;
	u_char *bp;
	u_short *sp;

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
		jupiter_debug(up->peer, __func__, "syncing");
		bp = (u_char *)sp;
		n = size;
		while (n >= 2) {
			if (bp[0] != (JUPITER_SYNC & 0xff)) {
				/*
				jupiter_debug(up->peer, __func__,
				    "{0x%x}", bp[0]);
				*/
				++bp;
				--n;
				continue;
			}
			if (bp[1] == ((JUPITER_SYNC >> 8) & 0xff))
				break;
			/*
			jupiter_debug(up->peer, __func__,
			    "{0x%x 0x%x}", bp[0], bp[1]);
			*/
			bp += 2;
			n -= 2;
		}
		/*
		jupiter_debug(up->peer, __func__, "\n");
		*/
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
	    jupiter_debug(up->peer, __func__, "bad header checksum!");
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
			jupiter_debug(up->peer,
			    __func__, "bad payload checksum!");
			/* This is drastic but checksum errors should be rare */
			up->ssize = 0;
			return (0);
		}
		cc += n;
	}
	return (cc);
}

#else /* not (REFCLOCK && CLOCK_JUPITER && HAVE_PPSAPI) */
int refclock_jupiter_bs;
#endif /* not (REFCLOCK && CLOCK_JUPITER && HAVE_PPSAPI) */
