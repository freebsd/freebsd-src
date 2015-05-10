/*-
 * Copyright (c) 2002-2008 Sam Leffler, Errno Consulting
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * IEEE 802.11 scanning support.
 */
#include "opt_wlan.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/condvar.h>
 
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>

#include <net80211/ieee80211_scan_sw.h>

#include <net/bpf.h>

struct scan_state {
	struct ieee80211_scan_state base;	/* public state */

	u_int		ss_iflags;		/* flags used internally */
#define	ISCAN_MINDWELL 	0x0001		/* min dwell time reached */
#define	ISCAN_DISCARD	0x0002		/* discard rx'd frames */
#define	ISCAN_CANCEL	0x0004		/* cancel current scan */
#define	ISCAN_ABORT	0x0008		/* end the scan immediately */
	unsigned long	ss_chanmindwell;	/* min dwell on curchan */
	unsigned long	ss_scanend;		/* time scan must stop */
	u_int		ss_duration;		/* duration for next scan */
	struct task	ss_scan_task;		/* scan execution */
	struct cv	ss_scan_cv;		/* scan signal */
	struct callout	ss_scan_timer;		/* scan timer */
};
#define	SCAN_PRIVATE(ss)	((struct scan_state *) ss)

/*
 * Amount of time to go off-channel during a background
 * scan.  This value should be large enough to catch most
 * ap's but short enough that we can return on-channel
 * before our listen interval expires.
 *
 * XXX tunable
 * XXX check against configured listen interval
 */
#define	IEEE80211_SCAN_OFFCHANNEL	msecs_to_ticks(150)

/*
 * Roaming-related defaults.  RSSI thresholds are as returned by the
 * driver (.5dBm).  Transmit rate thresholds are IEEE rate codes (i.e
 * .5M units) or MCS.
 */
/* rssi thresholds */
#define	ROAM_RSSI_11A_DEFAULT		14	/* 11a bss */
#define	ROAM_RSSI_11B_DEFAULT		14	/* 11b bss */
#define	ROAM_RSSI_11BONLY_DEFAULT	14	/* 11b-only bss */
/* transmit rate thresholds */
#define	ROAM_RATE_11A_DEFAULT		2*12	/* 11a bss */
#define	ROAM_RATE_11B_DEFAULT		2*5	/* 11b bss */
#define	ROAM_RATE_11BONLY_DEFAULT	2*1	/* 11b-only bss */
#define	ROAM_RATE_HALF_DEFAULT		2*6	/* half-width 11a/g bss */
#define	ROAM_RATE_QUARTER_DEFAULT	2*3	/* quarter-width 11a/g bss */
#define	ROAM_MCS_11N_DEFAULT		(1 | IEEE80211_RATE_MCS) /* 11n bss */

static	void scan_curchan(struct ieee80211_scan_state *, unsigned long);
static	void scan_mindwell(struct ieee80211_scan_state *);
static	void scan_signal(void *);
static	void scan_task(void *, int);

MALLOC_DEFINE(M_80211_SCAN, "80211scan", "802.11 scan state");

void
ieee80211_swscan_attach(struct ieee80211com *ic)
{
	struct scan_state *ss;

	ss = (struct scan_state *) malloc(sizeof(struct scan_state),
		M_80211_SCAN, M_NOWAIT | M_ZERO);
	if (ss == NULL) {
		ic->ic_scan = NULL;
		return;
	}
	callout_init_mtx(&ss->ss_scan_timer, IEEE80211_LOCK_OBJ(ic), 0);
	cv_init(&ss->ss_scan_cv, "scan");
	TASK_INIT(&ss->ss_scan_task, 0, scan_task, ss);

	ic->ic_scan = &ss->base;
	ss->base.ss_ic = ic;

	ic->ic_scan_curchan = scan_curchan;
	ic->ic_scan_mindwell = scan_mindwell;

	/*
	 * TODO: all of the non-vap scan calls should be methods!
	 */
}

void
ieee80211_swscan_detach(struct ieee80211com *ic)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	if (ss != NULL) {
		IEEE80211_LOCK(ic);
		SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_ABORT;
		scan_signal(ss);
		IEEE80211_UNLOCK(ic);
		ieee80211_draintask(ic, &SCAN_PRIVATE(ss)->ss_scan_task);
		callout_drain(&SCAN_PRIVATE(ss)->ss_scan_timer);
		KASSERT((ic->ic_flags & IEEE80211_F_SCAN) == 0,
		    ("scan still running"));

		/*
		 * For now, do the ss_ops detach here rather
		 * than ieee80211_scan_detach().
		 *
		 * I'll figure out how to cleanly split things up
		 * at a later date.
		 */
		if (ss->ss_ops != NULL) {
			ss->ss_ops->scan_detach(ss);
			ss->ss_ops = NULL;
		}
		ic->ic_scan = NULL;
		free(SCAN_PRIVATE(ss), M_80211_SCAN);
	}
}

void
ieee80211_swscan_vattach(struct ieee80211vap *vap)
{
	/* nothing to do for now */
	/*
	 * TODO: all of the vap scan calls should be methods!
	 */

}

void
ieee80211_swscan_vdetach(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss;

	IEEE80211_LOCK_ASSERT(ic);
	ss = ic->ic_scan;
	if (ss != NULL && ss->ss_vap == vap) {
		if (ic->ic_flags & IEEE80211_F_SCAN) {
			SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_ABORT;
			scan_signal(ss);
		}
	}
}

void
ieee80211_swscan_set_scan_duration(struct ieee80211vap *vap, u_int duration)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	IEEE80211_LOCK_ASSERT(ic);

	/* NB: flush frames rx'd before 1st channel change */
	SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_DISCARD;
	SCAN_PRIVATE(ss)->ss_duration = duration;
}

void
ieee80211_swscan_run_scan_task(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	IEEE80211_LOCK_ASSERT(ic);

	ieee80211_runtask(ic, &SCAN_PRIVATE(ss)->ss_scan_task);
}

/*
 * Start a scan unless one is already going.
 */
static int
ieee80211_swscan_start_scan_locked(const struct ieee80211_scanner *scan,
	struct ieee80211vap *vap, int flags, u_int duration,
	u_int mindwell, u_int maxdwell,
	u_int nssid, const struct ieee80211_scan_ssid ssids[])
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	IEEE80211_LOCK_ASSERT(ic);

	if (ic->ic_flags & IEEE80211_F_CSAPENDING) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: scan inhibited by pending channel change\n", __func__);
	} else if ((ic->ic_flags & IEEE80211_F_SCAN) == 0) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: %s scan, duration %u mindwell %u maxdwell %u, desired mode %s, %s%s%s%s%s%s\n"
		    , __func__
		    , flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive"
		    , duration, mindwell, maxdwell
		    , ieee80211_phymode_name[vap->iv_des_mode]
		    , flags & IEEE80211_SCAN_FLUSH ? "flush" : "append"
		    , flags & IEEE80211_SCAN_NOPICK ? ", nopick" : ""
		    , flags & IEEE80211_SCAN_NOJOIN ? ", nojoin" : ""
		    , flags & IEEE80211_SCAN_NOBCAST ? ", nobcast" : ""
		    , flags & IEEE80211_SCAN_PICK1ST ? ", pick1st" : ""
		    , flags & IEEE80211_SCAN_ONCE ? ", once" : ""
		);

		ieee80211_scan_update_locked(vap, scan);
		if (ss->ss_ops != NULL) {
			if ((flags & IEEE80211_SCAN_NOSSID) == 0)
				ieee80211_scan_copy_ssid(vap, ss, nssid, ssids);

			/* NB: top 4 bits for internal use */
			ss->ss_flags = flags & 0xfff;
			if (ss->ss_flags & IEEE80211_SCAN_ACTIVE)
				vap->iv_stats.is_scan_active++;
			else
				vap->iv_stats.is_scan_passive++;
			if (flags & IEEE80211_SCAN_FLUSH)
				ss->ss_ops->scan_flush(ss);
			if (flags & IEEE80211_SCAN_BGSCAN)
				ic->ic_flags_ext |= IEEE80211_FEXT_BGSCAN;

			/* Set duration for this particular scan */
			ieee80211_swscan_set_scan_duration(vap, duration);

			ss->ss_next = 0;
			ss->ss_mindwell = mindwell;
			ss->ss_maxdwell = maxdwell;
			/* NB: scan_start must be before the scan runtask */
			ss->ss_ops->scan_start(ss, vap);
#ifdef IEEE80211_DEBUG
			if (ieee80211_msg_scan(vap))
				ieee80211_scan_dump(ss);
#endif /* IEEE80211_DEBUG */
			ic->ic_flags |= IEEE80211_F_SCAN;

			/* Start scan task */
			ieee80211_swscan_run_scan_task(vap);
		}
		return 1;
	} else {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: %s scan already in progress\n", __func__,
		    ss->ss_flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive");
	}
	return 0;
}


/*
 * Start a scan unless one is already going.
 *
 * Called without the comlock held; grab the comlock as appropriate.
 */
int
ieee80211_swscan_start_scan(const struct ieee80211_scanner *scan,
    struct ieee80211vap *vap, int flags,
    u_int duration, u_int mindwell, u_int maxdwell,
    u_int nssid, const struct ieee80211_scan_ssid ssids[])
{
	struct ieee80211com *ic = vap->iv_ic;
	int result;

	IEEE80211_UNLOCK_ASSERT(ic);

	IEEE80211_LOCK(ic);
	result = ieee80211_swscan_start_scan_locked(scan, vap, flags, duration,
	    mindwell, maxdwell, nssid, ssids);
	IEEE80211_UNLOCK(ic);

	return result;
}

/*
 * Check the scan cache for an ap/channel to use; if that
 * fails then kick off a new scan.
 *
 * Called with the comlock held.
 *
 * XXX TODO: split out!
 */
int
ieee80211_swscan_check_scan(const struct ieee80211_scanner *scan,
    struct ieee80211vap *vap, int flags,
    u_int duration, u_int mindwell, u_int maxdwell,
    u_int nssid, const struct ieee80211_scan_ssid ssids[])
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;
	int result;

	IEEE80211_LOCK_ASSERT(ic);

	if (ss->ss_ops != NULL) {
		/* XXX verify ss_ops matches vap->iv_opmode */
		if ((flags & IEEE80211_SCAN_NOSSID) == 0) {
			/*
			 * Update the ssid list and mark flags so if
			 * we call start_scan it doesn't duplicate work.
			 */
			ieee80211_scan_copy_ssid(vap, ss, nssid, ssids);
			flags |= IEEE80211_SCAN_NOSSID;
		}
		if ((ic->ic_flags & IEEE80211_F_SCAN) == 0 &&
		    (flags & IEEE80211_SCAN_FLUSH) == 0 &&
		    time_before(ticks, ic->ic_lastscan + vap->iv_scanvalid)) {
			/*
			 * We're not currently scanning and the cache is
			 * deemed hot enough to consult.  Lock out others
			 * by marking IEEE80211_F_SCAN while we decide if
			 * something is already in the scan cache we can
			 * use.  Also discard any frames that might come
			 * in while temporarily marked as scanning.
			 */
			SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_DISCARD;
			ic->ic_flags |= IEEE80211_F_SCAN;

			/* NB: need to use supplied flags in check */
			ss->ss_flags = flags & 0xff;
			result = ss->ss_ops->scan_end(ss, vap);

			ic->ic_flags &= ~IEEE80211_F_SCAN;
			SCAN_PRIVATE(ss)->ss_iflags &= ~ISCAN_DISCARD;
			if (result) {
				ieee80211_notify_scan_done(vap);
				return 1;
			}
		}
	}
	result = ieee80211_swscan_start_scan_locked(scan, vap, flags, duration,
	    mindwell, maxdwell, nssid, ssids);

	return result;
}

/*
 * Restart a previous scan.  If the previous scan completed
 * then we start again using the existing channel list.
 */
int
ieee80211_swscan_bg_scan(const struct ieee80211_scanner *scan,
    struct ieee80211vap *vap, int flags)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	/* XXX assert unlocked? */
	// IEEE80211_UNLOCK_ASSERT(ic);

	IEEE80211_LOCK(ic);
	if ((ic->ic_flags & IEEE80211_F_SCAN) == 0) {
		u_int duration;
		/*
		 * Go off-channel for a fixed interval that is large
		 * enough to catch most ap's but short enough that
		 * we can return on-channel before our listen interval
		 * expires.
		 */
		duration = IEEE80211_SCAN_OFFCHANNEL;

		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: %s scan, ticks %u duration %lu\n", __func__,
		    ss->ss_flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive",
		    ticks, duration);

		ieee80211_scan_update_locked(vap, scan);
		if (ss->ss_ops != NULL) {
			ss->ss_vap = vap;
			/*
			 * A background scan does not select a new sta; it
			 * just refreshes the scan cache.  Also, indicate
			 * the scan logic should follow the beacon schedule:
			 * we go off-channel and scan for a while, then
			 * return to the bss channel to receive a beacon,
			 * then go off-channel again.  All during this time
			 * we notify the ap we're in power save mode.  When
			 * the scan is complete we leave power save mode.
			 * If any beacon indicates there are frames pending
			 * for us then we drop out of power save mode
			 * (and background scan) automatically by way of the
			 * usual sta power save logic.
			 */
			ss->ss_flags |= IEEE80211_SCAN_NOPICK
				     |  IEEE80211_SCAN_BGSCAN
				     |  flags
				     ;
			/* if previous scan completed, restart */
			if (ss->ss_next >= ss->ss_last) {
				if (ss->ss_flags & IEEE80211_SCAN_ACTIVE)
					vap->iv_stats.is_scan_active++;
				else
					vap->iv_stats.is_scan_passive++;
				/*
				 * NB: beware of the scan cache being flushed;
				 *     if the channel list is empty use the
				 *     scan_start method to populate it.
				 */
				ss->ss_next = 0;
				if (ss->ss_last != 0)
					ss->ss_ops->scan_restart(ss, vap);
				else {
					ss->ss_ops->scan_start(ss, vap);
#ifdef IEEE80211_DEBUG
					if (ieee80211_msg_scan(vap))
						ieee80211_scan_dump(ss);
#endif /* IEEE80211_DEBUG */
				}
			}
			ieee80211_swscan_set_scan_duration(vap, duration);
			ss->ss_maxdwell = duration;
			ic->ic_flags |= IEEE80211_F_SCAN;
			ic->ic_flags_ext |= IEEE80211_FEXT_BGSCAN;
			ieee80211_runtask(ic, &SCAN_PRIVATE(ss)->ss_scan_task);
		} else {
			/* XXX msg+stat */
		}
	} else {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: %s scan already in progress\n", __func__,
		    ss->ss_flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive");
	}
	IEEE80211_UNLOCK(ic);

	/* NB: racey, does it matter? */
	return (ic->ic_flags & IEEE80211_F_SCAN);
}

/*
 * Cancel any scan currently going on for the specified vap.
 */
void
ieee80211_swscan_cancel_scan(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	IEEE80211_LOCK(ic);
	if ((ic->ic_flags & IEEE80211_F_SCAN) &&
	    ss->ss_vap == vap &&
	    (SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL) == 0) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: cancel %s scan\n", __func__,
		    ss->ss_flags & IEEE80211_SCAN_ACTIVE ?
			"active" : "passive");

		/* clear bg scan NOPICK and mark cancel request */
		ss->ss_flags &= ~IEEE80211_SCAN_NOPICK;
		SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_CANCEL;
		/* wake up the scan task */
		scan_signal(ss);
	} else {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: called; F_SCAN=%d, vap=%s, CANCEL=%d\n",
		        __func__,
			!! (ic->ic_flags & IEEE80211_F_SCAN),
			(ss->ss_vap == vap ? "match" : "nomatch"),
			!! (SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL));
	}
	IEEE80211_UNLOCK(ic);
}

/*
 * Cancel any scan currently going on.
 */
void
ieee80211_swscan_cancel_anyscan(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	IEEE80211_LOCK(ic);
	if ((ic->ic_flags & IEEE80211_F_SCAN) &&
	    (SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL) == 0) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: cancel %s scan\n", __func__,
		    ss->ss_flags & IEEE80211_SCAN_ACTIVE ?
			"active" : "passive");

		/* clear bg scan NOPICK and mark cancel request */
		ss->ss_flags &= ~IEEE80211_SCAN_NOPICK;
		SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_CANCEL;
		/* wake up the scan task */
		scan_signal(ss);
	} else {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: called; F_SCAN=%d, vap=%s, CANCEL=%d\n",
		        __func__,
			!! (ic->ic_flags & IEEE80211_F_SCAN),
			(ss->ss_vap == vap ? "match" : "nomatch"),
			!! (SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL));
	}
	IEEE80211_UNLOCK(ic);
}

/*
 * Public access to scan_next for drivers that manage
 * scanning themselves (e.g. for firmware-based devices).
 */
void
ieee80211_swscan_scan_next(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN, "%s: called\n", __func__);

	/* wake up the scan task */
	IEEE80211_LOCK(ic);
	scan_signal(ss);
	IEEE80211_UNLOCK(ic);
}

/*
 * Public access to scan_next for drivers that are not able to scan single
 * channels (e.g. for firmware-based devices).
 */
void
ieee80211_swscan_scan_done(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss;

	IEEE80211_LOCK_ASSERT(ic);

	ss = ic->ic_scan;
	scan_signal(ss);
}

/*
 * Probe the curent channel, if allowed, while scanning.
 * If the channel is not marked passive-only then send
 * a probe request immediately.  Otherwise mark state and
 * listen for beacons on the channel; if we receive something
 * then we'll transmit a probe request.
 */
void
ieee80211_swscan_probe_curchan(struct ieee80211vap *vap, int force)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;
	struct ifnet *ifp = vap->iv_ifp;
	int i;

	/*
	 * Send directed probe requests followed by any
	 * broadcast probe request.
	 * XXX remove dependence on ic/vap->iv_bss
	 */
	for (i = 0; i < ss->ss_nssid; i++)
		ieee80211_send_probereq(vap->iv_bss,
			vap->iv_myaddr, ifp->if_broadcastaddr,
			ifp->if_broadcastaddr,
			ss->ss_ssid[i].ssid, ss->ss_ssid[i].len);
	if ((ss->ss_flags & IEEE80211_SCAN_NOBCAST) == 0)
		ieee80211_send_probereq(vap->iv_bss,
			vap->iv_myaddr, ifp->if_broadcastaddr,
			ifp->if_broadcastaddr,
			"", 0);
}

/*
 * Scan curchan.  If this is an active scan and the channel
 * is not marked passive then send probe request frame(s).
 * Arrange for the channel change after maxdwell ticks.
 */
static void
scan_curchan(struct ieee80211_scan_state *ss, unsigned long maxdwell)
{
	struct ieee80211vap *vap  = ss->ss_vap;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
	    "%s: calling; maxdwell=%lu\n",
	    __func__,
	    maxdwell);
	IEEE80211_LOCK(vap->iv_ic);
	if (ss->ss_flags & IEEE80211_SCAN_ACTIVE)
		ieee80211_probe_curchan(vap, 0);
	callout_reset(&SCAN_PRIVATE(ss)->ss_scan_timer,
	    maxdwell, scan_signal, ss);
	IEEE80211_UNLOCK(vap->iv_ic);
}

static void
scan_signal(void *arg)
{
	struct ieee80211_scan_state *ss = (struct ieee80211_scan_state *) arg;

	IEEE80211_LOCK_ASSERT(ss->ss_ic);
	cv_signal(&SCAN_PRIVATE(ss)->ss_scan_cv);
}

/*
 * Handle mindwell requirements completed; initiate a channel
 * change to the next channel asap.
 */
static void
scan_mindwell(struct ieee80211_scan_state *ss)
{
	struct ieee80211com *ic = ss->ss_ic;

	IEEE80211_DPRINTF(ss->ss_vap, IEEE80211_MSG_SCAN, "%s: called\n", __func__);

	IEEE80211_LOCK(ic);
	scan_signal(ss);
	IEEE80211_UNLOCK(ic);
}

static void
scan_task(void *arg, int pending)
{
#define	ISCAN_REP	(ISCAN_MINDWELL | ISCAN_DISCARD)
	struct ieee80211_scan_state *ss = (struct ieee80211_scan_state *) arg;
	struct ieee80211vap *vap = ss->ss_vap;
	struct ieee80211com *ic = ss->ss_ic;
	struct ieee80211_channel *chan;
	unsigned long maxdwell, scanend;
	int scandone = 0;

	IEEE80211_LOCK(ic);
	if (vap == NULL || (ic->ic_flags & IEEE80211_F_SCAN) == 0 ||
	    (SCAN_PRIVATE(ss)->ss_iflags & ISCAN_ABORT)) {
		/* Cancelled before we started */
		goto done;
	}

	if (ss->ss_next == ss->ss_last) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			"%s: no channels to scan\n", __func__);
		scandone = 1;
		goto done;
	}

	if (vap->iv_opmode == IEEE80211_M_STA &&
	    vap->iv_state == IEEE80211_S_RUN) {
		if ((vap->iv_bss->ni_flags & IEEE80211_NODE_PWR_MGT) == 0) {
			/* Enable station power save mode */
			vap->iv_sta_ps(vap, 1);
			/*
			 * Use an 1ms delay so the null data frame has a chance
			 * to go out.
			 * XXX Should use M_TXCB mechanism to eliminate this.
			 */
			cv_timedwait(&SCAN_PRIVATE(ss)->ss_scan_cv,
			    IEEE80211_LOCK_OBJ(ic), hz / 1000);
			if (SCAN_PRIVATE(ss)->ss_iflags & ISCAN_ABORT)
				goto done;
		}
	}

	scanend = ticks + SCAN_PRIVATE(ss)->ss_duration;

	/* XXX scan state can change! Re-validate scan state! */

	IEEE80211_UNLOCK(ic);
	ic->ic_scan_start(ic);		/* notify driver */
	IEEE80211_LOCK(ic);

	for (;;) {

		scandone = (ss->ss_next >= ss->ss_last) ||
		    (SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL) != 0;

		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: loop start; scandone=%d\n",
		    __func__,
		    scandone);

		if (scandone || (ss->ss_flags & IEEE80211_SCAN_GOTPICK) ||
		    (SCAN_PRIVATE(ss)->ss_iflags & ISCAN_ABORT) ||
		     time_after(ticks + ss->ss_mindwell, scanend))
			break;

		chan = ss->ss_chans[ss->ss_next++];

		/*
		 * Watch for truncation due to the scan end time.
		 */
		if (time_after(ticks + ss->ss_maxdwell, scanend))
			maxdwell = scanend - ticks;
		else
			maxdwell = ss->ss_maxdwell;

		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: chan %3d%c -> %3d%c [%s, dwell min %lums max %lums]\n",
		    __func__,
		    ieee80211_chan2ieee(ic, ic->ic_curchan),
		    ieee80211_channel_type_char(ic->ic_curchan),
		    ieee80211_chan2ieee(ic, chan),
		    ieee80211_channel_type_char(chan),
		    (ss->ss_flags & IEEE80211_SCAN_ACTIVE) &&
			(chan->ic_flags & IEEE80211_CHAN_PASSIVE) == 0 ?
			"active" : "passive",
		    ticks_to_msecs(ss->ss_mindwell), ticks_to_msecs(maxdwell));

		/*
		 * Potentially change channel and phy mode.
		 */
		ic->ic_curchan = chan;
		ic->ic_rt = ieee80211_get_ratetable(chan);
		IEEE80211_UNLOCK(ic);
		/*
		 * Perform the channel change and scan unlocked so the driver
		 * may sleep. Once set_channel returns the hardware has
		 * completed the channel change.
		 */
		ic->ic_set_channel(ic);
		ieee80211_radiotap_chan_change(ic);

		/*
		 * Scan curchan.  Drivers for "intelligent hardware"
		 * override ic_scan_curchan to tell the device to do
		 * the work.  Otherwise we manage the work outselves;
		 * sending a probe request (as needed), and arming the
		 * timeout to switch channels after maxdwell ticks.
		 *
		 * scan_curchan should only pause for the time required to
		 * prepare/initiate the hardware for the scan (if at all), the
		 * below condvar is used to sleep for the channels dwell time
		 * and allows it to be signalled for abort.
		 */
		ic->ic_scan_curchan(ss, maxdwell);
		IEEE80211_LOCK(ic);

		/* XXX scan state can change! Re-validate scan state! */

		SCAN_PRIVATE(ss)->ss_chanmindwell = ticks + ss->ss_mindwell;
		/* clear mindwell lock and initial channel change flush */
		SCAN_PRIVATE(ss)->ss_iflags &= ~ISCAN_REP;

		if ((SCAN_PRIVATE(ss)->ss_iflags & (ISCAN_CANCEL|ISCAN_ABORT)))
			continue;

		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN, "%s: waiting\n", __func__);
		/* Wait to be signalled to scan the next channel */
		cv_wait(&SCAN_PRIVATE(ss)->ss_scan_cv, IEEE80211_LOCK_OBJ(ic));
	}

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN, "%s: out\n", __func__);

	if (SCAN_PRIVATE(ss)->ss_iflags & ISCAN_ABORT)
		goto done;

	IEEE80211_UNLOCK(ic);
	ic->ic_scan_end(ic);		/* notify driver */
	IEEE80211_LOCK(ic);
	/* XXX scan state can change! Re-validate scan state! */

	/*
	 * Since a cancellation may have occured during one of the
	 * driver calls (whilst unlocked), update scandone.
	 */
	if (scandone == 0 &&
	    ((SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL) != 0)) {
		/* XXX printf? */
		if_printf(vap->iv_ifp,
		    "%s: OOPS! scan cancelled during driver call (1)!\n",
		    __func__);
		scandone = 1;
	}

	/*
	 * Record scan complete time.  Note that we also do
	 * this when canceled so any background scan will
	 * not be restarted for a while.
	 */
	if (scandone)
		ic->ic_lastscan = ticks;
	/* return to the bss channel */
	if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
	    ic->ic_curchan != ic->ic_bsschan) {
		ieee80211_setupcurchan(ic, ic->ic_bsschan);
		IEEE80211_UNLOCK(ic);
		ic->ic_set_channel(ic);
		ieee80211_radiotap_chan_change(ic);
		IEEE80211_LOCK(ic);
	}
	/* clear internal flags and any indication of a pick */
	SCAN_PRIVATE(ss)->ss_iflags &= ~ISCAN_REP;
	ss->ss_flags &= ~IEEE80211_SCAN_GOTPICK;

	/*
	 * If not canceled and scan completed, do post-processing.
	 * If the callback function returns 0, then it wants to
	 * continue/restart scanning.  Unfortunately we needed to
	 * notify the driver to end the scan above to avoid having
	 * rx frames alter the scan candidate list.
	 */
	if ((SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL) == 0 &&
	    !ss->ss_ops->scan_end(ss, vap) &&
	    (ss->ss_flags & IEEE80211_SCAN_ONCE) == 0 &&
	    time_before(ticks + ss->ss_mindwell, scanend)) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: done, restart "
		    "[ticks %u, dwell min %lu scanend %lu]\n",
		    __func__,
		    ticks, ss->ss_mindwell, scanend);
		ss->ss_next = 0;	/* reset to begining */
		if (ss->ss_flags & IEEE80211_SCAN_ACTIVE)
			vap->iv_stats.is_scan_active++;
		else
			vap->iv_stats.is_scan_passive++;

		ss->ss_ops->scan_restart(ss, vap);	/* XXX? */
		ieee80211_runtask(ic, &SCAN_PRIVATE(ss)->ss_scan_task);
		IEEE80211_UNLOCK(ic);
		return;
	}

	/* past here, scandone is ``true'' if not in bg mode */
	if ((ss->ss_flags & IEEE80211_SCAN_BGSCAN) == 0)
		scandone = 1;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
	    "%s: %s, [ticks %u, dwell min %lu scanend %lu]\n",
	    __func__, scandone ? "done" : "stopped",
	    ticks, ss->ss_mindwell, scanend);

	/*
	 * Since a cancellation may have occured during one of the
	 * driver calls (whilst unlocked), update scandone.
	 */
	if (scandone == 0 &&
	    ((SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL) != 0)) {
		/* XXX printf? */
		if_printf(vap->iv_ifp,
		    "%s: OOPS! scan cancelled during driver call (2)!\n",
		    __func__);
		scandone = 1;
	}

	/*
	 * Clear the SCAN bit first in case frames are
	 * pending on the station power save queue.  If
	 * we defer this then the dispatch of the frames
	 * may generate a request to cancel scanning.
	 */
done:
	ic->ic_flags &= ~IEEE80211_F_SCAN;
	/*
	 * Drop out of power save mode when a scan has
	 * completed.  If this scan was prematurely terminated
	 * because it is a background scan then don't notify
	 * the ap; we'll either return to scanning after we
	 * receive the beacon frame or we'll drop out of power
	 * save mode because the beacon indicates we have frames
	 * waiting for us.
	 */
	if (scandone) {
		vap->iv_sta_ps(vap, 0);
		if (ss->ss_next >= ss->ss_last) {
			ieee80211_notify_scan_done(vap);
			ic->ic_flags_ext &= ~IEEE80211_FEXT_BGSCAN;
		}
	}
	SCAN_PRIVATE(ss)->ss_iflags &= ~(ISCAN_CANCEL|ISCAN_ABORT);
	ss->ss_flags &= ~(IEEE80211_SCAN_ONCE | IEEE80211_SCAN_PICK1ST);
	IEEE80211_UNLOCK(ic);
#undef ISCAN_REP
}

/*
 * Process a beacon or probe response frame.
 */
void
ieee80211_swscan_add_scan(struct ieee80211vap *vap,
	struct ieee80211_channel *curchan,
	const struct ieee80211_scanparams *sp,
	const struct ieee80211_frame *wh,
	int subtype, int rssi, int noise)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	/* XXX locking */
	/*
	 * Frames received during startup are discarded to avoid
	 * using scan state setup on the initial entry to the timer
	 * callback.  This can occur because the device may enable
	 * rx prior to our doing the initial channel change in the
	 * timer routine.
	 */
	if (SCAN_PRIVATE(ss)->ss_iflags & ISCAN_DISCARD)
		return;
#ifdef IEEE80211_DEBUG
	if (ieee80211_msg_scan(vap) && (ic->ic_flags & IEEE80211_F_SCAN))
		ieee80211_scan_dump_probe_beacon(subtype, 1, wh->i_addr2, sp, rssi);
#endif
	if (ss->ss_ops != NULL &&
	    ss->ss_ops->scan_add(ss, curchan, sp, wh, subtype, rssi, noise)) {
		/*
		 * If we've reached the min dwell time terminate
		 * the timer so we'll switch to the next channel.
		 */
		if ((SCAN_PRIVATE(ss)->ss_iflags & ISCAN_MINDWELL) == 0 &&
		    time_after_eq(ticks, SCAN_PRIVATE(ss)->ss_chanmindwell)) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
			    "%s: chan %3d%c min dwell met (%u > %lu)\n",
			    __func__,
			    ieee80211_chan2ieee(ic, ic->ic_curchan),
			    ieee80211_channel_type_char(ic->ic_curchan),
			    ticks, SCAN_PRIVATE(ss)->ss_chanmindwell);
			SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_MINDWELL;
			/*
			 * NB: trigger at next clock tick or wait for the
			 * hardware.
			 */
			ic->ic_scan_mindwell(ss);
		}
	}
}

