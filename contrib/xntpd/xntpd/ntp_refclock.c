/* ntp_refclock.c,v 3.1 1993/07/06 01:11:25 jbj Exp
 * ntp_refclock - processing support for reference clocks
 */
#include <stdio.h>
#include <sys/types.h>

#include "ntpd.h"
#include "ntp_refclock.h"
#include "ntp_stdlib.h"

#ifdef REFCLOCK
/*
 * Reference clock support is provided here by maintaining the
 * fiction that the clock is actually a peer.  As no packets are
 * exchanged with a reference clock, however, we replace the
 * transmit, receive and packet procedures with separate code
 * to simulate them.  Refclock_transmit and refclock_receive
 * maintain the peer variables in a state analogous to an
 * actual peer and pass reference clock data on through the
 * filters.  Refclock_peer and refclock_unpeer are called to
 * initialize and terminate reference clock associations.
 */

/*
 * The refclock configuration table.  Imported from refclock_conf.c
 */
extern struct refclock *refclock_conf[];
extern u_char num_refclock_conf;

/*
 * Imported from the I/O module
 */
extern struct interface *any_interface;
extern struct interface *loopback_interface;

/*
 * Imported from the timer module
 */
extern U_LONG current_time;
extern struct event timerqueue[];

/*
 * Imported from the main and peer modules.  We use the same
 * algorithm for spacing out timers at configuration time that
 * the peer module does.
 */
extern U_LONG init_peer_starttime;
extern int initializing;
extern int debug;

static	void	refclock_transmit	P((struct peer *));

/*
 * refclock_newpeer - initialize and start a reference clock
 */
int
refclock_newpeer(peer)
	struct peer *peer;
{
	u_char clktype;
	int unit;

	/*
	 * Sanity...
	 */
	if (!ISREFCLOCKADR(&peer->srcadr)) {
		syslog(LOG_ERR,
	"Internal error: attempting to initialize %s as reference clock",
		    ntoa(&peer->srcadr));
		return 0;
	}

	clktype = REFCLOCKTYPE(&peer->srcadr);
	unit = REFCLOCKUNIT(&peer->srcadr);

	/*
	 * If clktype is invalid, return
	 */
	if (clktype >= num_refclock_conf
	    || refclock_conf[clktype]->clock_start == noentry) {
		syslog(LOG_ERR,
		    "Can't initialize %s, no support for clock type %d\n",
		    ntoa(&peer->srcadr), clktype);
		return 0;
	}

	/*
	 * Complete initialization of peer structure.
	 */
	peer->refclktype = clktype;
	peer->refclkunit = unit;
	peer->flags |= FLAG_REFCLOCK;
	peer->stratum = STRATUM_REFCLOCK;
	peer->ppoll = peer->minpoll;
	peer->hpoll = peer->minpoll;
	peer->maxpoll = peer->minpoll;
	
	/*
	 * Check the flags.  If the peer is configured in client mode
	 * but prefers the broadcast client filter algorithm, change
	 * him over.
	 */
	if (peer->hmode == MODE_CLIENT
	    && refclock_conf[clktype]->clock_flags & REF_FLAG_BCLIENT)
		peer->hmode = MODE_BCLIENT;

	peer->event_timer.peer = peer;
	peer->event_timer.event_handler = refclock_transmit;

	/*
	 * Do driver dependent initialization
	 */
	if (!((refclock_conf[clktype]->clock_start)(unit, peer))) {
		syslog(LOG_ERR, "Clock dependent initialization of %s failed",
		    ntoa(&peer->srcadr));
		return 0;
	}

	/*
	 * Set up the timeout for reachability determination.
	 */
	if (initializing) {
		init_peer_starttime += (1 << EVENT_TIMEOUT);
		if (init_peer_starttime >= (1 << peer->minpoll))
			init_peer_starttime = (1 << EVENT_TIMEOUT);
		peer->event_timer.event_time = init_peer_starttime;
	} else {
		peer->event_timer.event_time = current_time + (1 << peer->hpoll);
	}
	TIMER_ENQUEUE(timerqueue, &peer->event_timer);
	return 1;
}


/*
 * refclock_unpeer - shut down a clock
 */
void
refclock_unpeer(peer)
	struct peer *peer;
{
	/*
	 * Do sanity checks.  I know who programmed the calling routine!
	 */
	if (peer->refclktype >= num_refclock_conf
	    || refclock_conf[peer->refclktype]->clock_shutdown == noentry) {
		syslog(LOG_ERR, "Attempting to shutdown %s: no such clock!",
		    ntoa(&peer->srcadr));
		return;
	}

	/*
	 * Tell the driver we're finished.
	 */
	(refclock_conf[peer->refclktype]->clock_shutdown)(peer->refclkunit);
}


/*
 * refclock_transmit - replacement transmit procedure for reference clocks
 */
static void
refclock_transmit(peer)
	struct peer *peer;
{
	u_char opeer_reach;
	int clktype;
	int unit;

	clktype = peer->refclktype;
	unit = peer->refclkunit;
	peer->sent++;

	/*
	 * The transmit procedure is supposed to freeze a time stamp.
	 * Get one just for fun, and to tell when we last were here.
	 */
	get_systime(&peer->xmt);

	/*
	 * Fiddle reachability.
	 */
	opeer_reach = peer->reach;
	peer->reach <<= 1;
	if (peer->reach == 0) {
		/*
		 * Clear this one out.  No need to redo
		 * selection since this fellow will definitely
		 * be suffering from dispersion madness.
		 */
		if (opeer_reach != 0) {
			peer_clear(peer);
			peer->timereachable = current_time;
			report_event(EVNT_UNREACH, peer);
		}

	/*
	 * Update reachability and poll variables
	 */
	} else if ((opeer_reach & 3) == 0) {

		l_fp off;

		if (peer->valid > 0) peer->valid--;
		if (peer->hpoll > peer->minpoll) peer->hpoll--;
		off.l_ui = off.l_uf = 0;
		clock_filter(peer, &off, 0, NTP_MAXDISPERSE);
		if (peer->flags & FLAG_SYSPEER)
		    clock_select();
	} else {
		if (peer->valid < NTP_SHIFT) {
			peer->valid++;
		} else {
			if (peer->hpoll < peer->maxpoll)
			    peer->hpoll++;
		}
	}

	/*
	 * If he wants to be polled, do it.
	 */
	if (refclock_conf[clktype]->clock_poll != noentry)
		(refclock_conf[clktype]->clock_poll)(unit, peer);
	
	/*
	 * Finally, reset the timer
	 */
	peer->event_timer.event_time += (1 << peer->hpoll);
	TIMER_ENQUEUE(timerqueue, &peer->event_timer);
}


/*
 * refclock_receive - simulate the receive and packet procedures for clocks
 */
void
refclock_receive(peer, offset, delay, dispersion, reftime, rectime, leap)
	struct peer *peer;
	l_fp *offset;
	s_fp delay;
	u_fp dispersion;
	l_fp *reftime;
	l_fp *rectime;
	int leap;
{
	int restrict;
	int trustable;
	extern u_char leap_indicator;

#ifdef DEBUG
	if (debug)
		printf("refclock_receive: %s %s %s %s)\n",
		    ntoa(&peer->srcadr), lfptoa(offset, 6),
		    fptoa(delay, 5), ufptoa(dispersion, 5));
#endif

	/*
	 * The name of this routine is actually a misnomer since
	 * we mostly simulate the variable setting of the packet
	 * procedure.  We do check the flag values, though, and
	 * set the trust bits based on this.
	 */
	restrict = restrictions(&peer->srcadr);
	if (restrict & (RES_IGNORE|RES_DONTSERVE)) {
		/*
		 * Ours is not to reason why...
		 */
		return;
	}

	peer->received++;
	peer->processed++;
	peer->timereceived = current_time;
	if (restrict & RES_DONTTRUST)
		trustable = 0;
	else
		trustable = 1;

	if (peer->flags & FLAG_AUTHENABLE) {
		if (trustable)
			peer->flags |= FLAG_AUTHENTIC;
		else
			peer->flags &= ~FLAG_AUTHENTIC;
	}
	if (leap == 0)
	    peer->leap = leap_indicator;
	else
	    peer->leap = leap;

	/*
	 * Set the timestamps. rec and org are in local time,
	 * while ref is in timecode time.
	 */
	peer->rec = peer->org = *rectime;
	peer->reftime = *reftime;

	/*
	 * If the interface has been set to any_interface, set it
	 * to the loop back address if we have one.  This is so
	 * that peers which are unreachable are easy to see in
	 * peer display.
	 */
	if (peer->dstadr == any_interface && loopback_interface != 0)
		peer->dstadr = loopback_interface;

	/*
	 * Set peer.pmode based on the hmode.  For appearances only.
	 */
	switch (peer->hmode) {
	case MODE_ACTIVE:
		peer->pmode = MODE_PASSIVE;
		break;
	case MODE_CLIENT:
		peer->pmode = MODE_SERVER;
		break;
	case MODE_BCLIENT:
		peer->pmode = MODE_BROADCAST;
		break;
	default:
		syslog(LOG_ERR, "refclock_receive: internal error, mode = %d",
		    peer->hmode);
	}

	/*
	 * Abandon ship if the radio came bum. We only got this far
	 * in order to make pretty billboards, even if bum.
	 */
	if (leap == LEAP_NOTINSYNC) return;
	/*
	 * If this guy was previously unreachable, report him
	 * reachable.
	 */
	if (peer->reach == 0) report_event(EVNT_REACH, peer);
	peer->reach |= 1;

	/*
	 * Give the data to the clock filter and update the clock.
	 */
	clock_filter(peer, offset, delay, dispersion);
	clock_update(peer);
}


/*
 * refclock_control - set and/or return clock values
 */
void
refclock_control(srcadr, in, out)
	struct sockaddr_in *srcadr;
	struct refclockstat *in;
	struct refclockstat *out;
{
	u_char clktype;
	int unit;

	/*
	 * Sanity...
	 */
	if (!ISREFCLOCKADR(srcadr)) {
		syslog(LOG_ERR,
	"Internal error: refclock_control received %s as reference clock",
		    ntoa(srcadr));
		return;
	}

	clktype = REFCLOCKTYPE(srcadr);
	unit = REFCLOCKUNIT(srcadr);

	/*
	 * If clktype is invalid, return
	 */
	if (clktype >= num_refclock_conf
	    || refclock_conf[clktype]->clock_control == noentry) {
		syslog(LOG_ERR,
		   "Internal error: refclock_control finds %s as not supported",
		    ntoa(srcadr));
		return;
	}

	/*
	 * Give the stuff to the clock.
	 */
	(refclock_conf[clktype]->clock_control)(unit, in, out);
}



/*
 * refclock_buginfo - return debugging info
 */
void
refclock_buginfo(srcadr, bug)
	struct sockaddr_in *srcadr;
	struct refclockbug *bug;
{
	u_char clktype;
	int unit;

	/*
	 * Sanity...
	 */
	if (!ISREFCLOCKADR(srcadr)) {
		syslog(LOG_ERR,
	"Internal error: refclock_buginfo received %s as reference clock",
		    ntoa(srcadr));
		return;
	}

	clktype = REFCLOCKTYPE(srcadr);
	unit = REFCLOCKUNIT(srcadr);

	/*
	 * If clktype is invalid or call is unsupported, return
	 */
	if (clktype >= num_refclock_conf ||
	    refclock_conf[clktype]->clock_buginfo == noentry) {
		return;
	}

	/*
	 * Give the stuff to the clock.
	 */
	(refclock_conf[clktype]->clock_buginfo)(unit, bug);
}

/*
 * init_refclock - initialize the reference clock drivers
 */
void
init_refclock()
{
	register u_char i;

	for (i = 0; i < num_refclock_conf; i++) {
		if (refclock_conf[i]->clock_init != noentry)
			(refclock_conf[i]->clock_init)();
	}
}
#endif
