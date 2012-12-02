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
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>

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
ieee80211_scan_attach(struct ieee80211com *ic)
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
}

void
ieee80211_scan_detach(struct ieee80211com *ic)
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
		if (ss->ss_ops != NULL) {
			ss->ss_ops->scan_detach(ss);
			ss->ss_ops = NULL;
		}
		ic->ic_scan = NULL;
		free(SCAN_PRIVATE(ss), M_80211_SCAN);
	}
}

static const struct ieee80211_roamparam defroam[IEEE80211_MODE_MAX] = {
	[IEEE80211_MODE_11A]	= { .rssi = ROAM_RSSI_11A_DEFAULT,
				    .rate = ROAM_RATE_11A_DEFAULT },
	[IEEE80211_MODE_11G]	= { .rssi = ROAM_RSSI_11B_DEFAULT,
				    .rate = ROAM_RATE_11B_DEFAULT },
	[IEEE80211_MODE_11B]	= { .rssi = ROAM_RSSI_11BONLY_DEFAULT,
				    .rate = ROAM_RATE_11BONLY_DEFAULT },
	[IEEE80211_MODE_TURBO_A]= { .rssi = ROAM_RSSI_11A_DEFAULT,
				    .rate = ROAM_RATE_11A_DEFAULT },
	[IEEE80211_MODE_TURBO_G]= { .rssi = ROAM_RSSI_11A_DEFAULT,
				    .rate = ROAM_RATE_11A_DEFAULT },
	[IEEE80211_MODE_STURBO_A]={ .rssi = ROAM_RSSI_11A_DEFAULT,
				    .rate = ROAM_RATE_11A_DEFAULT },
	[IEEE80211_MODE_HALF]	= { .rssi = ROAM_RSSI_11A_DEFAULT,
				    .rate = ROAM_RATE_HALF_DEFAULT },
	[IEEE80211_MODE_QUARTER]= { .rssi = ROAM_RSSI_11A_DEFAULT,
				    .rate = ROAM_RATE_QUARTER_DEFAULT },
	[IEEE80211_MODE_11NA]	= { .rssi = ROAM_RSSI_11A_DEFAULT,
				    .rate = ROAM_MCS_11N_DEFAULT },
	[IEEE80211_MODE_11NG]	= { .rssi = ROAM_RSSI_11B_DEFAULT,
				    .rate = ROAM_MCS_11N_DEFAULT },
};

void
ieee80211_scan_vattach(struct ieee80211vap *vap)
{
	vap->iv_bgscanidle = (IEEE80211_BGSCAN_IDLE_DEFAULT*1000)/hz;
	vap->iv_bgscanintvl = IEEE80211_BGSCAN_INTVAL_DEFAULT*hz;
	vap->iv_scanvalid = IEEE80211_SCAN_VALID_DEFAULT*hz;

	vap->iv_roaming = IEEE80211_ROAMING_AUTO;
	memcpy(vap->iv_roamparms, defroam, sizeof(defroam));
}

void
ieee80211_scan_vdetach(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss;

	IEEE80211_LOCK(ic);
	ss = ic->ic_scan;
	if (ss != NULL && ss->ss_vap == vap) {
		if (ic->ic_flags & IEEE80211_F_SCAN) {
			SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_ABORT;
			scan_signal(ss);
		}
		if (ss->ss_ops != NULL) {
			ss->ss_ops->scan_detach(ss);
			ss->ss_ops = NULL;
		}
		ss->ss_vap = NULL;
	}
	IEEE80211_UNLOCK(ic);
}

/*
 * Simple-minded scanner module support.
 */
static const char *scan_modnames[IEEE80211_OPMODE_MAX] = {
	"wlan_scan_sta",	/* IEEE80211_M_IBSS */
	"wlan_scan_sta",	/* IEEE80211_M_STA */
	"wlan_scan_wds",	/* IEEE80211_M_WDS */
	"wlan_scan_sta",	/* IEEE80211_M_AHDEMO */
	"wlan_scan_ap",		/* IEEE80211_M_HOSTAP */
	"wlan_scan_monitor",	/* IEEE80211_M_MONITOR */
	"wlan_scan_sta",	/* IEEE80211_M_MBSS */
};
static const struct ieee80211_scanner *scanners[IEEE80211_OPMODE_MAX];

const struct ieee80211_scanner *
ieee80211_scanner_get(enum ieee80211_opmode mode)
{
	if (mode >= IEEE80211_OPMODE_MAX)
		return NULL;
	if (scanners[mode] == NULL)
		ieee80211_load_module(scan_modnames[mode]);
	return scanners[mode];
}

void
ieee80211_scanner_register(enum ieee80211_opmode mode,
	const struct ieee80211_scanner *scan)
{
	if (mode >= IEEE80211_OPMODE_MAX)
		return;
	scanners[mode] = scan;
}

void
ieee80211_scanner_unregister(enum ieee80211_opmode mode,
	const struct ieee80211_scanner *scan)
{
	if (mode >= IEEE80211_OPMODE_MAX)
		return;
	if (scanners[mode] == scan)
		scanners[mode] = NULL;
}

void
ieee80211_scanner_unregister_all(const struct ieee80211_scanner *scan)
{
	int m;

	for (m = 0; m < IEEE80211_OPMODE_MAX; m++)
		if (scanners[m] == scan)
			scanners[m] = NULL;
}

/*
 * Update common scanner state to reflect the current
 * operating mode.  This is called when the state machine
 * is transitioned to RUN state w/o scanning--e.g. when
 * operating in monitor mode.  The purpose of this is to
 * ensure later callbacks find ss_ops set to properly
 * reflect current operating mode.
 */
static void
scan_update_locked(struct ieee80211vap *vap,
	const struct ieee80211_scanner *scan)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	IEEE80211_LOCK_ASSERT(ic);

#ifdef IEEE80211_DEBUG
	if (ss->ss_vap != vap || ss->ss_ops != scan) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: current scanner is <%s:%s>, switch to <%s:%s>\n",
		    __func__,
		    ss->ss_vap != NULL ?
			ss->ss_vap->iv_ifp->if_xname : "none",
		    ss->ss_vap != NULL ?
			ieee80211_opmode_name[ss->ss_vap->iv_opmode] : "none",
		    vap->iv_ifp->if_xname,
		    ieee80211_opmode_name[vap->iv_opmode]);
	}
#endif
	ss->ss_vap = vap;
	if (ss->ss_ops != scan) {
		/*
		 * Switch scanners; detach old, attach new.  Special
		 * case where a single scan module implements multiple
		 * policies by using different scan ops but a common
		 * core.  We assume if the old and new attach methods
		 * are identical then it's ok to just change ss_ops
		 * and not flush the internal state of the module.
		 */
		if (scan == NULL || ss->ss_ops == NULL ||
		    ss->ss_ops->scan_attach != scan->scan_attach) {
			if (ss->ss_ops != NULL)
				ss->ss_ops->scan_detach(ss);
			if (scan != NULL && !scan->scan_attach(ss)) {
				/* XXX attach failure */
				/* XXX stat+msg */
				scan = NULL;
			}
		}
		ss->ss_ops = scan;
	}
}

static char
channel_type(const struct ieee80211_channel *c)
{
	if (IEEE80211_IS_CHAN_ST(c))
		return 'S';
	if (IEEE80211_IS_CHAN_108A(c))
		return 'T';
	if (IEEE80211_IS_CHAN_108G(c))
		return 'G';
	if (IEEE80211_IS_CHAN_HT(c))
		return 'n';
	if (IEEE80211_IS_CHAN_A(c))
		return 'a';
	if (IEEE80211_IS_CHAN_ANYG(c))
		return 'g';
	if (IEEE80211_IS_CHAN_B(c))
		return 'b';
	return 'f';
}

void
ieee80211_scan_dump_channels(const struct ieee80211_scan_state *ss)
{
	struct ieee80211com *ic = ss->ss_ic;
	const char *sep;
	int i;

	sep = "";
	for (i = ss->ss_next; i < ss->ss_last; i++) {
		const struct ieee80211_channel *c = ss->ss_chans[i];

		printf("%s%u%c", sep, ieee80211_chan2ieee(ic, c),
			channel_type(c));
		sep = ", ";
	}
}

#ifdef IEEE80211_DEBUG
static void
scan_dump(struct ieee80211_scan_state *ss)
{
	struct ieee80211vap *vap = ss->ss_vap;

	if_printf(vap->iv_ifp, "scan set ");
	ieee80211_scan_dump_channels(ss);
	printf(" dwell min %lums max %lums\n",
	    ticks_to_msecs(ss->ss_mindwell), ticks_to_msecs(ss->ss_maxdwell));
}
#endif /* IEEE80211_DEBUG */

static void
copy_ssid(struct ieee80211vap *vap, struct ieee80211_scan_state *ss,
	int nssid, const struct ieee80211_scan_ssid ssids[])
{
	if (nssid > IEEE80211_SCAN_MAX_SSID) {
		/* XXX printf */
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: too many ssid %d, ignoring all of them\n",
		    __func__, nssid);
		return;
	}
	memcpy(ss->ss_ssid, ssids, nssid * sizeof(ssids[0]));
	ss->ss_nssid = nssid;
}

/*
 * Start a scan unless one is already going.
 */
static int
start_scan_locked(const struct ieee80211_scanner *scan,
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

		scan_update_locked(vap, scan);
		if (ss->ss_ops != NULL) {
			if ((flags & IEEE80211_SCAN_NOSSID) == 0)
				copy_ssid(vap, ss, nssid, ssids);

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

			/* NB: flush frames rx'd before 1st channel change */
			SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_DISCARD;
			SCAN_PRIVATE(ss)->ss_duration = duration;
			ss->ss_next = 0;
			ss->ss_mindwell = mindwell;
			ss->ss_maxdwell = maxdwell;
			/* NB: scan_start must be before the scan runtask */
			ss->ss_ops->scan_start(ss, vap);
#ifdef IEEE80211_DEBUG
			if (ieee80211_msg_scan(vap))
				scan_dump(ss);
#endif /* IEEE80211_DEBUG */
			ic->ic_flags |= IEEE80211_F_SCAN;
			ieee80211_runtask(ic, &SCAN_PRIVATE(ss)->ss_scan_task);
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
 */
int
ieee80211_start_scan(struct ieee80211vap *vap, int flags,
	u_int duration, u_int mindwell, u_int maxdwell,
	u_int nssid, const struct ieee80211_scan_ssid ssids[])
{
	struct ieee80211com *ic = vap->iv_ic;
	const struct ieee80211_scanner *scan;
	int result;

	scan = ieee80211_scanner_get(vap->iv_opmode);
	if (scan == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: no scanner support for %s mode\n",
		    __func__, ieee80211_opmode_name[vap->iv_opmode]);
		/* XXX stat */
		return 0;
	}

	IEEE80211_LOCK(ic);
	result = start_scan_locked(scan, vap, flags, duration,
	    mindwell, maxdwell, nssid, ssids);
	IEEE80211_UNLOCK(ic);

	return result;
}

/*
 * Check the scan cache for an ap/channel to use; if that
 * fails then kick off a new scan.
 */
int
ieee80211_check_scan(struct ieee80211vap *vap, int flags,
	u_int duration, u_int mindwell, u_int maxdwell,
	u_int nssid, const struct ieee80211_scan_ssid ssids[])
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;
	const struct ieee80211_scanner *scan;
	int result;

	scan = ieee80211_scanner_get(vap->iv_opmode);
	if (scan == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: no scanner support for %s mode\n",
		    __func__, vap->iv_opmode);
		/* XXX stat */
		return 0;
	}

	/*
	 * Check if there's a list of scan candidates already.
	 * XXX want more than the ap we're currently associated with
	 */

	IEEE80211_LOCK(ic);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
	    "%s: %s scan, %s%s%s%s%s\n"
	    , __func__
	    , flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive"
	    , flags & IEEE80211_SCAN_FLUSH ? "flush" : "append"
	    , flags & IEEE80211_SCAN_NOPICK ? ", nopick" : ""
	    , flags & IEEE80211_SCAN_NOJOIN ? ", nojoin" : ""
	    , flags & IEEE80211_SCAN_PICK1ST ? ", pick1st" : ""
	    , flags & IEEE80211_SCAN_ONCE ? ", once" : ""
	);

	if (ss->ss_ops != scan) {
		/* XXX re-use cache contents? e.g. adhoc<->sta */
		flags |= IEEE80211_SCAN_FLUSH;
	}
	scan_update_locked(vap, scan);
	if (ss->ss_ops != NULL) {
		/* XXX verify ss_ops matches vap->iv_opmode */
		if ((flags & IEEE80211_SCAN_NOSSID) == 0) {
			/*
			 * Update the ssid list and mark flags so if
			 * we call start_scan it doesn't duplicate work.
			 */
			copy_ssid(vap, ss, nssid, ssids);
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
				IEEE80211_UNLOCK(ic);
				return 1;
			}
		}
	}
	result = start_scan_locked(scan, vap, flags, duration,
	    mindwell, maxdwell, nssid, ssids);
	IEEE80211_UNLOCK(ic);

	return result;
}

/*
 * Check the scan cache for an ap/channel to use; if that fails
 * then kick off a scan using the current settings.
 */
int
ieee80211_check_scan_current(struct ieee80211vap *vap)
{
	return ieee80211_check_scan(vap,
	    IEEE80211_SCAN_ACTIVE,
	    IEEE80211_SCAN_FOREVER, 0, 0,
	    vap->iv_des_nssid, vap->iv_des_ssid);
}

/*
 * Restart a previous scan.  If the previous scan completed
 * then we start again using the existing channel list.
 */
int
ieee80211_bg_scan(struct ieee80211vap *vap, int flags)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;
	const struct ieee80211_scanner *scan;

	scan = ieee80211_scanner_get(vap->iv_opmode);
	if (scan == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		    "%s: no scanner support for %s mode\n",
		    __func__, vap->iv_opmode);
		/* XXX stat */
		return 0;
	}

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

		scan_update_locked(vap, scan);
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
						scan_dump(ss);
#endif /* IEEE80211_DEBUG */
				}
			}
			/* NB: flush frames rx'd before 1st channel change */
			SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_DISCARD;
			SCAN_PRIVATE(ss)->ss_duration = duration;
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
ieee80211_cancel_scan(struct ieee80211vap *vap)
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
	}
	IEEE80211_UNLOCK(ic);
}

/*
 * Cancel any scan currently going on.
 */
void
ieee80211_cancel_anyscan(struct ieee80211vap *vap)
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
	}
	IEEE80211_UNLOCK(ic);
}

/*
 * Public access to scan_next for drivers that manage
 * scanning themselves (e.g. for firmware-based devices).
 */
void
ieee80211_scan_next(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;

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
ieee80211_scan_done(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss;

	IEEE80211_LOCK(ic);
	ss = ic->ic_scan;
	ss->ss_next = ss->ss_last; /* all channels are complete */
	scan_signal(ss);
	IEEE80211_UNLOCK(ic);
}

/*
 * Probe the curent channel, if allowed, while scanning.
 * If the channel is not marked passive-only then send
 * a probe request immediately.  Otherwise mark state and
 * listen for beacons on the channel; if we receive something
 * then we'll transmit a probe request.
 */
void
ieee80211_probe_curchan(struct ieee80211vap *vap, int force)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_scan_state *ss = ic->ic_scan;
	struct ifnet *ifp = vap->iv_ifp;
	int i;

	if ((ic->ic_curchan->ic_flags & IEEE80211_CHAN_PASSIVE) && !force) {
		ic->ic_flags_ext |= IEEE80211_FEXT_PROBECHAN;
		return;
	}
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
	IEEE80211_UNLOCK(ic);
	ic->ic_scan_start(ic);		/* notify driver */
	IEEE80211_LOCK(ic);

	for (;;) {
		scandone = (ss->ss_next >= ss->ss_last) ||
		    (SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL) != 0;
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
		        channel_type(ic->ic_curchan),
		    ieee80211_chan2ieee(ic, chan), channel_type(chan),
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

		SCAN_PRIVATE(ss)->ss_chanmindwell = ticks + ss->ss_mindwell;
		/* clear mindwell lock and initial channel change flush */
		SCAN_PRIVATE(ss)->ss_iflags &= ~ISCAN_REP;

		if ((SCAN_PRIVATE(ss)->ss_iflags & (ISCAN_CANCEL|ISCAN_ABORT)))
			continue;

		/* Wait to be signalled to scan the next channel */
		cv_wait(&SCAN_PRIVATE(ss)->ss_scan_cv, IEEE80211_LOCK_OBJ(ic));
	}
	if (SCAN_PRIVATE(ss)->ss_iflags & ISCAN_ABORT)
		goto done;

	IEEE80211_UNLOCK(ic);
	ic->ic_scan_end(ic);		/* notify driver */
	IEEE80211_LOCK(ic);

	/*
	 * Since a cancellation may have occured during one of the
	 * driver calls (whilst unlocked), update scandone.
	 */
	if (scandone == 0 &&
	    ((SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL) != 0)) {
		/* XXX printf? */
		if_printf(vap->iv_ifp,
		    "%s: OOPS! scan cancelled during driver call!\n",
		    __func__);
	}
	scandone |= ((SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL) != 0);

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

#ifdef IEEE80211_DEBUG
static void
dump_country(const uint8_t *ie)
{
	const struct ieee80211_country_ie *cie =
	   (const struct ieee80211_country_ie *) ie;
	int i, nbands, schan, nchan;

	if (cie->len < 3) {
		printf(" <bogus country ie, len %d>", cie->len);
		return;
	}
	printf(" country [%c%c%c", cie->cc[0], cie->cc[1], cie->cc[2]);
	nbands = (cie->len - 3) / sizeof(cie->band[0]);
	for (i = 0; i < nbands; i++) {
		schan = cie->band[i].schan;
		nchan = cie->band[i].nchan;
		if (nchan != 1)
			printf(" %u-%u,%u", schan, schan + nchan-1,
			    cie->band[i].maxtxpwr);
		else
			printf(" %u,%u", schan, cie->band[i].maxtxpwr);
	}
	printf("]");
}

static void
dump_probe_beacon(uint8_t subtype, int isnew,
	const uint8_t mac[IEEE80211_ADDR_LEN],
	const struct ieee80211_scanparams *sp, int rssi)
{

	printf("[%s] %s%s on chan %u (bss chan %u) ",
	    ether_sprintf(mac), isnew ? "new " : "",
	    ieee80211_mgt_subtype_name[subtype >> IEEE80211_FC0_SUBTYPE_SHIFT],
	    sp->chan, sp->bchan);
	ieee80211_print_essid(sp->ssid + 2, sp->ssid[1]);
	printf(" rssi %d\n", rssi);

	if (isnew) {
		printf("[%s] caps 0x%x bintval %u erp 0x%x", 
			ether_sprintf(mac), sp->capinfo, sp->bintval, sp->erp);
		if (sp->country != NULL)
			dump_country(sp->country);
		printf("\n");
	}
}
#endif /* IEEE80211_DEBUG */

/*
 * Process a beacon or probe response frame.
 */
void
ieee80211_add_scan(struct ieee80211vap *vap,
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
		dump_probe_beacon(subtype, 1, wh->i_addr2, sp, rssi);
#endif
	if (ss->ss_ops != NULL &&
	    ss->ss_ops->scan_add(ss, sp, wh, subtype, rssi, noise)) {
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
				channel_type(ic->ic_curchan),
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

/*
 * Timeout/age scan cache entries; called from sta timeout
 * timer (XXX should be self-contained).
 */
void
ieee80211_scan_timeout(struct ieee80211com *ic)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	if (ss->ss_ops != NULL)
		ss->ss_ops->scan_age(ss);
}

/*
 * Mark a scan cache entry after a successful associate.
 */
void
ieee80211_scan_assoc_success(struct ieee80211vap *vap, const uint8_t mac[])
{
	struct ieee80211_scan_state *ss = vap->iv_ic->ic_scan;

	if (ss->ss_ops != NULL) {
		IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_SCAN,
			mac, "%s",  __func__);
		ss->ss_ops->scan_assoc_success(ss, mac);
	}
}

/*
 * Demerit a scan cache entry after failing to associate.
 */
void
ieee80211_scan_assoc_fail(struct ieee80211vap *vap,
	const uint8_t mac[], int reason)
{
	struct ieee80211_scan_state *ss = vap->iv_ic->ic_scan;

	if (ss->ss_ops != NULL) {
		IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_SCAN, mac,
			"%s: reason %u", __func__, reason);
		ss->ss_ops->scan_assoc_fail(ss, mac, reason);
	}
}

/*
 * Iterate over the contents of the scan cache.
 */
void
ieee80211_scan_iterate(struct ieee80211vap *vap,
	ieee80211_scan_iter_func *f, void *arg)
{
	struct ieee80211_scan_state *ss = vap->iv_ic->ic_scan;

	if (ss->ss_ops != NULL)
		ss->ss_ops->scan_iterate(ss, f, arg);
}

/*
 * Flush the contents of the scan cache.
 */
void
ieee80211_scan_flush(struct ieee80211vap *vap)
{
	struct ieee80211_scan_state *ss = vap->iv_ic->ic_scan;

	if (ss->ss_ops != NULL && ss->ss_vap == vap) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN, "%s\n",  __func__);
		ss->ss_ops->scan_flush(ss);
	}
}

/*
 * Check the scan cache for an ap/channel to use; if that
 * fails then kick off a new scan.
 */
struct ieee80211_channel *
ieee80211_scan_pickchannel(struct ieee80211com *ic, int flags)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	IEEE80211_LOCK_ASSERT(ic);

	if (ss == NULL || ss->ss_ops == NULL || ss->ss_vap == NULL) {
		/* XXX printf? */
		return NULL;
	}
	if (ss->ss_ops->scan_pickchan == NULL) {
		IEEE80211_DPRINTF(ss->ss_vap, IEEE80211_MSG_SCAN,
		    "%s: scan module does not support picking a channel, "
		    "opmode %s\n", __func__, ss->ss_vap->iv_opmode);
		return NULL;
	}
	return ss->ss_ops->scan_pickchan(ss, flags);
}
