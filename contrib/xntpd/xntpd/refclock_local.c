/*
 * refclock_local - local pseudo-clock driver
 */
#if	defined(REFCLOCK) && defined(LOCAL_CLOCK)
#include <stdio.h>
#include <ctype.h>
#include <sys/time.h>

#include "ntpd.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

static	void	local_init	P((void));
static	int	local_start	P((u_int, struct peer *));
static	void	local_shutdown	P((int));
static	void	local_poll	P((int, struct peer *));
static	void	local_control	P((u_int, struct refclockstat *, struct refclockstat *));
#define	local_buginfo	noentry

struct	refclock refclock_local = {
	local_start, local_shutdown, local_poll,
	local_control, local_init, local_buginfo, NOFLAGS
};

/*
 * This is a hack to allow a machine to use its own system clock as
 * a "reference clock", i.e. to free run against its own clock at
 * a non-infinity stratum.  This is certainly useful if you want to
 * use NTP in an isolated environment with no radio clock (not that
 * this is a good idea) to synchronize the machines together.  Pick
 * a machine that you figure has a good clock and configure it with
 * a local reference clock running at stratum 0 (i.e. 127.127.1.0).
 * Then point all the other machines at the one you're using as the
 * reference.
 *
 * The other thing this is good for is if you want to use a particular
 * server's clock as the last resort, when all radio time has gone
 * away.  This is especially good if that server has an ovenized
 * oscillator or something which will keep the time stable for extended
 * periods, since then all the other machines can benefit from this.
 * For this you would configure a local clock at a higher stratum (say
 * 3 or 4) to prevent the server's stratum from falling below here.
 */

/*
 * Definitions
 */
#define	NUMUNITS	16	/* 127.127.1.[0-15] */

/*
 * Some constant values we stick in the peer structure
 */
#define	LCLDISPERSION	(FP_SECOND/5)	/* 200 ms dispersion */
#define	LCLROOTDISPERSION (FP_SECOND/5)	/* 200 ms root dispersion */
#define	LCLPRECISION	(-5)		/* what the heck */
#define	LCLREFID	"LCL\0"
#define	LCLREFOFFSET	20		/* reftime is 20s behind */
#define	LCLHSREFID	0x7f7f0101	/* 127.127.1.1 refid for hi stratum */

/*
 * Description of clock
 */
#define	LCLDESCRIPTION	"Free running against local system clock"

/*
 * Local clock unit control structure.
 */
struct lclunit {
	struct peer *peer;		/* associated peer structure */
	u_char status;			/* clock status */
	u_char lastevent;		/* last clock event */
	u_char unit;			/* unit number */
	u_char unused;
	U_LONG lastupdate;		/* last time data received */
	U_LONG polls;			/* number of polls */
	U_LONG timestarted;		/* time we started this */
};


/*
 * Data space for the unit structures.  Note that we allocate these on
 * the fly, but never give them back.
 */
static struct lclunit *lclunits[NUMUNITS];
static u_char unitinuse[NUMUNITS];

/*
 * Imported from the timer module
 */
extern U_LONG current_time;

extern l_fp sys_clock_offset;

/*
 * local_init - initialize internal local clock driver data
 */
static void
local_init()
{
	/*
	 * Just zero the data arrays
	 */
	memset((char *)lclunits, 0, sizeof lclunits);
	memset((char *)unitinuse, 0, sizeof unitinuse);
}


/*
 * local_start - start up a local reference clock
 */
static int
local_start(unit, peer)
	u_int unit;
	struct peer *peer;
{
	register int i;
	register struct lclunit *lcl;

	if (unit >= NUMUNITS) {
		syslog(LOG_ERR, "local clock: unit number %d invalid (max 15)",
		    unit);
		return 0;
	}
	if (unitinuse[unit]) {
		syslog(LOG_ERR, "local clock: unit number %d in use", unit);
		return 0;
	}

	/*
	 * Looks like this might succeed.  Find memory for the structure.
	 * Look to see if there are any unused ones, if not we malloc()
	 * one.
	 */
	if (lclunits[unit] != 0) {
		lcl = lclunits[unit];	/* The one we want is okay */
	} else {
		for (i = 0; i < NUMUNITS; i++) {
			if (!unitinuse[i] && lclunits[i] != 0)
				break;
		}
		if (i < NUMUNITS) {
			/*
			 * Reclaim this one
			 */
			lcl = lclunits[i];
			lclunits[i] = 0;
		} else {
			lcl = (struct lclunit *)emalloc(sizeof(struct lclunit));
		}
	}
	memset((char *)lcl, 0, sizeof(struct lclunit));
	lclunits[unit] = lcl;

	/*
	 * Set up the structure
	 */
	lcl->peer = peer;
	lcl->unit = (u_char)unit;
	lcl->timestarted = lcl->lastupdate = current_time;

	/*
	 * That was easy.  Diddle the peer variables and return success.
	 */
	peer->precision = LCLPRECISION;
	peer->rootdelay = 0;
	peer->rootdispersion = LCLROOTDISPERSION;
	peer->stratum = (u_char)unit;
	if (unit <= 1)
		memmove((char *)&peer->refid, LCLREFID, 4);
	else
		peer->refid = htonl(LCLHSREFID);
	unitinuse[unit] = 1;
	return 1;
}


/*
 * local_shutdown - shut down a local clock
 */
static void
local_shutdown(unit)
	int unit;
{
	if (unit >= NUMUNITS) {
		syslog(LOG_ERR,
		"local clock: INTERNAL ERROR, unit number %d invalid (max 15)",
		    unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR,
		"local clock: INTERNAL ERROR, unit number %d not in use", unit);
		return;
	}

	unitinuse[unit] = 0;
}


/*
 * local_poll - called by the transmit procedure
 */
static void
local_poll(unit, peer)
	int unit;
	struct peer *peer;
{
	l_fp off;
	l_fp ts;

	if (unit >= NUMUNITS) {
		syslog(LOG_ERR, "local clock poll: INTERNAL: unit %d invalid",
		    unit);
		return;
	}
	if (!unitinuse[unit]) {
		syslog(LOG_ERR, "local clock poll: INTERNAL: unit %d unused",
		    unit);
		return;
	}
	if (peer != lclunits[unit]->peer) {
		syslog(LOG_ERR,
		    "local clock poll: INTERNAL: peer incorrect for unit %d",
		    unit);
		return;
	}

	/*
	 * Update clock stat counters
	 */
	lclunits[unit]->polls++;
	lclunits[unit]->lastupdate = current_time;

	/*
	 * This is pretty easy.  Give the reference clock support
	 * a zero offset and our fixed dispersion.  Use peer->xmt for
	 * our receive time.  Use peer->xmt - 20 seconds for our
	 * reference time.
	 */
	off.l_ui = off.l_uf = 0;
	ts = peer->xmt;
	ts.l_ui -= LCLREFOFFSET;
	refclock_receive(peer, &off, 0, LCLDISPERSION,
	    &ts, &peer->xmt, 0);
}



/*
 * local_control - set fudge factors, return statistics
 */
static void
local_control(unit, in, out)
	u_int unit;
	struct refclockstat *in;
	struct refclockstat *out;
{
	extern s_fp drift_comp;

	if (unit >= NUMUNITS) {
		syslog(LOG_ERR, "local clock: unit %d invalid (max %d)",
		    unit, NUMUNITS-1);
		return;
	}

	/*
	 * The time1 fudge factor is the drift compensation register.
	 * The time2 fudge factor is the offset of the system clock from
	 * what the protocol has set it to be. Most useful when SLEWALWAYS
	 * is defined.
	 */
	if (in != 0) {
		if (in->haveflags & CLK_HAVETIME1)
			drift_comp = LFPTOFP(&in->fudgetime1);
		if (in->haveflags & CLK_HAVETIME2) {
			sys_clock_offset.l_ui = in->fudgetime2.l_ui;
			sys_clock_offset.l_uf = in->fudgetime2.l_uf;
		}
	}
	if (out != 0) {
		out->type = REFCLK_LOCALCLOCK;
		out->flags = 0;
		out->haveflags = CLK_HAVETIME1;
		out->clockdesc = LCLDESCRIPTION;
		FPTOLFP(drift_comp, &out->fudgetime1);
		out->fudgetime2.l_ui = sys_clock_offset.l_ui;
		out->fudgetime2.l_uf = sys_clock_offset.l_uf;
		out->fudgeval1 = out->fudgeval2 = 0;
		out->lencode = 0;
		out->lastcode = "";
		out->badformat = 0;
		out->baddata = 0;
		out->noresponse = 0;
		if (unitinuse[unit]) {
			out->polls = lclunits[unit]->polls;
			out->timereset =
			    current_time - lclunits[unit]->timestarted;
			out->lastevent = lclunits[unit]->lastevent;
			out->currentstatus = lclunits[unit]->status;
		} else {
			out->polls = 0;
			out->timereset = 0;
			out->currentstatus = out->lastevent = CEVNT_NOMINAL;
		}
	}
}
#endif	/* REFCLOCK */
