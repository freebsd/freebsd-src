/*-
 * Copyright (c) 2002-2007 Sam Leffler, Errno Consulting
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
__FBSDID("$FreeBSD: src/sys/net80211/ieee80211_scan.c,v 1.3.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * IEEE 802.11 scanning support.
 */
#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/kernel.h>
 
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
#define	ISCAN_START	0x0008		/* 1st time through next_scan */
	unsigned long	ss_chanmindwell;	/* min dwell on curchan */
	unsigned long	ss_scanend;		/* time scan must stop */
	u_int		ss_duration;		/* duration for next scan */
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
 * driver (dBm).  Transmit rate thresholds are IEEE rate codes (i.e
 * .5M units).
 */
#define	ROAM_RSSI_11A_DEFAULT		14	/* rssi threshold for 11a bss */
#define	ROAM_RSSI_11B_DEFAULT		14	/* rssi threshold for 11b bss */
#define	ROAM_RSSI_11BONLY_DEFAULT	14	/* rssi threshold for 11b-only bss */
#define	ROAM_RATE_11A_DEFAULT		2*12	/* tx rate thresh for 11a bss */
#define	ROAM_RATE_11B_DEFAULT		2*5	/* tx rate thresh for 11b bss */
#define	ROAM_RATE_11BONLY_DEFAULT	2*1	/* tx rate thresh for 11b-only bss */

static	void scan_restart_pwrsav(void *);
static	void scan_curchan(struct ieee80211com *, unsigned long);
static	void scan_mindwell(struct ieee80211com *);
static	void scan_next(void *);

MALLOC_DEFINE(M_80211_SCAN, "80211scan", "802.11 scan state");

void
ieee80211_scan_attach(struct ieee80211com *ic)
{
	struct scan_state *ss;

	ic->ic_roaming = IEEE80211_ROAMING_AUTO;

	MALLOC(ss, struct scan_state *, sizeof(struct scan_state),
		M_80211_SCAN, M_NOWAIT | M_ZERO);
	if (ss == NULL) {
		ic->ic_scan = NULL;
		return;
	}
	callout_init(&ss->ss_scan_timer, CALLOUT_MPSAFE);
	ic->ic_scan = &ss->base;

	ic->ic_scan_curchan = scan_curchan;
	ic->ic_scan_mindwell = scan_mindwell;

	ic->ic_bgscanidle = (IEEE80211_BGSCAN_IDLE_DEFAULT*1000)/hz;
	ic->ic_bgscanintvl = IEEE80211_BGSCAN_INTVAL_DEFAULT*hz;
	ic->ic_scanvalid = IEEE80211_SCAN_VALID_DEFAULT*hz;
	ic->ic_roam.rssi11a = ROAM_RSSI_11A_DEFAULT;
	ic->ic_roam.rssi11b = ROAM_RSSI_11B_DEFAULT;
	ic->ic_roam.rssi11bOnly = ROAM_RSSI_11BONLY_DEFAULT;
	ic->ic_roam.rate11a = ROAM_RATE_11A_DEFAULT;
	ic->ic_roam.rate11b = ROAM_RATE_11B_DEFAULT;
	ic->ic_roam.rate11bOnly = ROAM_RATE_11BONLY_DEFAULT;
}

void
ieee80211_scan_detach(struct ieee80211com *ic)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	if (ss != NULL) {
		callout_drain(&SCAN_PRIVATE(ss)->ss_scan_timer);
		if (ss->ss_ops != NULL) {
			ss->ss_ops->scan_detach(ss);
			ss->ss_ops = NULL;
		}
		ic->ic_flags &= ~IEEE80211_F_SCAN;
		ic->ic_scan = NULL;
		FREE(SCAN_PRIVATE(ss), M_80211_SCAN);
	}
}

/*
 * Simple-minded scanner module support.
 */
#define	IEEE80211_SCANNER_MAX	(IEEE80211_M_MONITOR+1)

static const char *scan_modnames[IEEE80211_SCANNER_MAX] = {
	"wlan_scan_sta",	/* IEEE80211_M_IBSS */
	"wlan_scan_sta",	/* IEEE80211_M_STA */
	"wlan_scan_wds",	/* IEEE80211_M_WDS */
	"wlan_scan_sta",	/* IEEE80211_M_AHDEMO */
	"wlan_scan_4",		/* n/a */
	"wlan_scan_5",		/* n/a */
	"wlan_scan_ap",		/* IEEE80211_M_HOSTAP */
	"wlan_scan_7",		/* n/a */
	"wlan_scan_monitor",	/* IEEE80211_M_MONITOR */
};
static const struct ieee80211_scanner *scanners[IEEE80211_SCANNER_MAX];

const struct ieee80211_scanner *
ieee80211_scanner_get(enum ieee80211_opmode mode)
{
	if (mode >= IEEE80211_SCANNER_MAX)
		return NULL;
	/* NB: avoid monitor mode; there is no scan support */
	if (mode != IEEE80211_M_MONITOR && scanners[mode] == NULL)
		ieee80211_load_module(scan_modnames[mode]);
	return scanners[mode];
}

void
ieee80211_scanner_register(enum ieee80211_opmode mode,
	const struct ieee80211_scanner *scan)
{
	if (mode >= IEEE80211_SCANNER_MAX)
		return;
	scanners[mode] = scan;
}

void
ieee80211_scanner_unregister(enum ieee80211_opmode mode,
	const struct ieee80211_scanner *scan)
{
	if (mode >= IEEE80211_SCANNER_MAX)
		return;
	if (scanners[mode] == scan)
		scanners[mode] = NULL;
}

void
ieee80211_scanner_unregister_all(const struct ieee80211_scanner *scan)
{
	int m;

	for (m = 0; m < IEEE80211_SCANNER_MAX; m++)
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
int
ieee80211_scan_update(struct ieee80211com *ic)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;
	const struct ieee80211_scanner *scan;

	scan = ieee80211_scanner_get(ic->ic_opmode);
	IEEE80211_LOCK(ic);
	if (scan == NULL) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
		    "%s: no scanner support for mode %u\n",
		    __func__, ic->ic_opmode);
		/* XXX stat */
	}
	ss->ss_ic = ic;
	if (ss->ss_ops != scan) {
		/* switch scanners; detach old, attach new */
		if (ss->ss_ops != NULL)
			ss->ss_ops->scan_detach(ss);
		if (scan != NULL && !scan->scan_attach(ss)) {
			/* XXX attach failure */
			/* XXX stat+msg */
			ss->ss_ops = NULL;
		} else
			ss->ss_ops = scan;
	}
	IEEE80211_UNLOCK(ic);

	return (scan != NULL);
}

static void
change_channel(struct ieee80211com *ic,
	struct ieee80211_channel *chan)
{
	ic->ic_curchan = chan;
	ic->ic_set_channel(ic);
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

/*
 * Enable station power save mode and start/restart the scanning thread.
 */
static void
scan_restart_pwrsav(void *arg)
{
	struct scan_state *ss = (struct scan_state *) arg;
	struct ieee80211com *ic = ss->base.ss_ic;
	int delay;

	ieee80211_sta_pwrsave(ic, 1);
	/*
	 * Use an initial 1ms delay to insure the null
	 * data frame has a chance to go out.
	 * XXX 1ms is a lot, better to trigger scan
	 * on tx complete.
	 */
	delay = hz/1000;
	if (delay < 1)
		delay = 1;
	ic->ic_scan_start(ic);			/* notify driver */
	ss->ss_scanend = ticks + delay + ss->ss_duration;
	ss->ss_iflags |= ISCAN_START;
	callout_reset(&ss->ss_scan_timer, delay, scan_next, ss);
}

/*
 * Start/restart scanning.  If we're operating in station mode
 * and associated notify the ap we're going into power save mode
 * and schedule a callback to initiate the work (where there's a
 * better context for doing the work).  Otherwise, start the scan
 * directly.
 */
static int
scan_restart(struct scan_state *ss, u_int duration)
{
	struct ieee80211com *ic = ss->base.ss_ic;
	int defer = 0;

	if (ss->base.ss_next == ss->base.ss_last) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
			"%s: no channels to scan\n", __func__);
		return 0;
	}
	if (ic->ic_opmode == IEEE80211_M_STA &&
	    ic->ic_state == IEEE80211_S_RUN) {
		if ((ic->ic_bss->ni_flags & IEEE80211_NODE_PWR_MGT) == 0) {
			/*
			 * Initiate power save before going off-channel.
			 * Note that we cannot do this directly because
			 * of locking issues; instead we defer it to a
			 * tasklet.
			 */
			ss->ss_duration = duration;
			defer = 1;
		}
	}

	if (!defer) {
		ic->ic_scan_start(ic);		/* notify driver */
		ss->ss_scanend = ticks + duration;
		ss->ss_iflags |= ISCAN_START;
		callout_reset(&ss->ss_scan_timer, 0, scan_next, ss);
	} else
		scan_restart_pwrsav(ss);
	return 1;
}

static void
copy_ssid(struct ieee80211com *ic, struct ieee80211_scan_state *ss,
	int nssid, const struct ieee80211_scan_ssid ssids[])
{
	if (nssid > IEEE80211_SCAN_MAX_SSID) {
		/* XXX printf */
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
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
int
ieee80211_start_scan(struct ieee80211com *ic, int flags, u_int duration,
	u_int nssid, const struct ieee80211_scan_ssid ssids[])
{
	const struct ieee80211_scanner *scan;
	struct ieee80211_scan_state *ss = ic->ic_scan;

	scan = ieee80211_scanner_get(ic->ic_opmode);
	if (scan == NULL) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
		    "%s: no scanner support for mode %u\n",
		    __func__, ic->ic_opmode);
		/* XXX stat */
		return 0;
	}

	IEEE80211_LOCK(ic);
	if ((ic->ic_flags & IEEE80211_F_SCAN) == 0) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
		    "%s: %s scan, duration %u, desired mode %s, %s%s%s%s\n"
		    , __func__
		    , flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive"
		    , duration
		    , ieee80211_phymode_name[ic->ic_des_mode]
		    , flags & IEEE80211_SCAN_FLUSH ? "flush" : "append"
		    , flags & IEEE80211_SCAN_NOPICK ? ", nopick" : ""
		    , flags & IEEE80211_SCAN_PICK1ST ? ", pick1st" : ""
		    , flags & IEEE80211_SCAN_ONCE ? ", once" : ""
		);

		ss->ss_ic = ic;
		if (ss->ss_ops != scan) {
			/* switch scanners; detach old, attach new */
			if (ss->ss_ops != NULL)
				ss->ss_ops->scan_detach(ss);
			if (!scan->scan_attach(ss)) {
				/* XXX attach failure */
				/* XXX stat+msg */
				ss->ss_ops = NULL;
			} else
				ss->ss_ops = scan;
		}
		if (ss->ss_ops != NULL) {
			if ((flags & IEEE80211_SCAN_NOSSID) == 0)
				copy_ssid(ic, ss, nssid, ssids);

			/* NB: top 4 bits for internal use */
			ss->ss_flags = flags & 0xfff;
			if (ss->ss_flags & IEEE80211_SCAN_ACTIVE)
				ic->ic_stats.is_scan_active++;
			else
				ic->ic_stats.is_scan_passive++;
			if (flags & IEEE80211_SCAN_FLUSH)
				ss->ss_ops->scan_flush(ss);

			/* NB: flush frames rx'd before 1st channel change */
			SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_DISCARD;
			ss->ss_ops->scan_start(ss, ic);
			if (scan_restart(SCAN_PRIVATE(ss), duration))
				ic->ic_flags |= IEEE80211_F_SCAN;
		}
	} else {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
		    "%s: %s scan already in progress\n", __func__,
		    ss->ss_flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive");
	}
	IEEE80211_UNLOCK(ic);

	/* NB: racey, does it matter? */
	return (ic->ic_flags & IEEE80211_F_SCAN);
}

/*
 * Check the scan cache for an ap/channel to use; if that
 * fails then kick off a new scan.
 */
int
ieee80211_check_scan(struct ieee80211com *ic, int flags, u_int duration,
	u_int nssid, const struct ieee80211_scan_ssid ssids[])
{
	struct ieee80211_scan_state *ss = ic->ic_scan;
	int checkscanlist = 0;

	/*
	 * Check if there's a list of scan candidates already.
	 * XXX want more than the ap we're currently associated with
	 */

	IEEE80211_LOCK(ic);
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
	    "%s: %s scan, duration %u, desired mode %s, %s%s%s%s\n"
	    , __func__
	    , flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive"
	    , duration
	    , ieee80211_phymode_name[ic->ic_des_mode]
	    , flags & IEEE80211_SCAN_FLUSH ? "flush" : "append"
	    , flags & IEEE80211_SCAN_NOPICK ? ", nopick" : ""
	    , flags & IEEE80211_SCAN_PICK1ST ? ", pick1st" : ""
	    , flags & IEEE80211_SCAN_ONCE ? ", once" : ""
	);

	if (ss->ss_ops != NULL) {
		/* XXX verify ss_ops matches ic->ic_opmode */
		if ((flags & IEEE80211_SCAN_NOSSID) == 0) {
			/*
			 * Update the ssid list and mark flags so if
			 * we call start_scan it doesn't duplicate work.
			 */
			copy_ssid(ic, ss, nssid, ssids);
			flags |= IEEE80211_SCAN_NOSSID;
		}
		if ((ic->ic_flags & IEEE80211_F_SCAN) == 0 &&
		     time_before(ticks, ic->ic_lastscan + ic->ic_scanvalid)) {
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
			checkscanlist = 1;
		}
	}
	IEEE80211_UNLOCK(ic);
	if (checkscanlist) {
		const struct ieee80211_scanner *scan;

		scan = ieee80211_scanner_get(ic->ic_opmode);
		if (scan == NULL) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
			    "%s: no scanner support for mode %u\n",
			    __func__, ic->ic_opmode);
			/* XXX stat */
			return 0;
		}
		if (scan == ss->ss_ops && ss->ss_ops->scan_end(ss, ic)) {
			/* found an ap, just clear the flag */
			ic->ic_flags &= ~IEEE80211_F_SCAN;
			return 1;
		}
		/* no ap, clear the flag before starting a scan */
		ic->ic_flags &= ~IEEE80211_F_SCAN;
	}
	return ieee80211_start_scan(ic, flags, duration, nssid, ssids);
}

/*
 * Restart a previous scan.  If the previous scan completed
 * then we start again using the existing channel list.
 */
int
ieee80211_bg_scan(struct ieee80211com *ic)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

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

		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
		    "%s: %s scan, ticks %u duration %lu\n", __func__,
		    ss->ss_flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive",
		    ticks, duration);

		if (ss->ss_ops != NULL) {
			ss->ss_ic = ic;
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
				     |  IEEE80211_SCAN_BGSCAN;
			/* if previous scan completed, restart */
			if (ss->ss_next >= ss->ss_last) {
				ss->ss_next = 0;
				if (ss->ss_flags & IEEE80211_SCAN_ACTIVE)
					ic->ic_stats.is_scan_active++;
				else
					ic->ic_stats.is_scan_passive++;
				ss->ss_ops->scan_restart(ss, ic);
			}
			/* NB: flush frames rx'd before 1st channel change */
			SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_DISCARD;
			ss->ss_maxdwell = duration;
			if (scan_restart(SCAN_PRIVATE(ss), duration)) {
				ic->ic_flags |= IEEE80211_F_SCAN;
				ic->ic_flags_ext |= IEEE80211_FEXT_BGSCAN;
			}
		} else {
			/* XXX msg+stat */
		}
	} else {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
		    "%s: %s scan already in progress\n", __func__,
		    ss->ss_flags & IEEE80211_SCAN_ACTIVE ? "active" : "passive");
	}
	IEEE80211_UNLOCK(ic);

	/* NB: racey, does it matter? */
	return (ic->ic_flags & IEEE80211_F_SCAN);
}

/*
 * Cancel any scan currently going on.
 */
void
ieee80211_cancel_scan(struct ieee80211com *ic)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	IEEE80211_LOCK(ic);
	if ((ic->ic_flags & IEEE80211_F_SCAN) &&
	    (SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL) == 0) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
		    "%s: cancel %s scan\n", __func__,
		    ss->ss_flags & IEEE80211_SCAN_ACTIVE ?
			"active" : "passive");

		/* clear bg scan NOPICK and mark cancel request */
		ss->ss_flags &= ~IEEE80211_SCAN_NOPICK;
		SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_CANCEL;
		/* force it to fire asap */
		callout_reset(&SCAN_PRIVATE(ss)->ss_scan_timer,
			0, scan_next, ss);
	}
	IEEE80211_UNLOCK(ic);
}

/*
 * Public access to scan_next for drivers that manage
 * scanning themselves (e.g. for firmware-based devices).
 */
void
ieee80211_scan_next(struct ieee80211com *ic)
{
	/*
	 * XXX: We might need/want to decouple context here by either:
	 *  callout_reset(&SCAN_PRIVATE(ss)->ss_scan_timer, 0, scan_next, ss);
	 * or using a taskqueue.  Let's see what kind of problems direct
	 * dispatch has for now.
	 */
	scan_next(ic->ic_scan);
}

/*
 * Public access to scan_next for drivers that are not able to scan single
 * channels (e.g. for firmware-based devices).
 */
void
ieee80211_scan_done(struct ieee80211com *ic)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	ss->ss_next = ss->ss_last; /* all channels are complete */
	scan_next(ss);
}

/*
 * Scan curchan.  If this is an active scan and the channel
 * is not marked passive then send probe request frame(s).
 * Arrange for the channel change after maxdwell ticks.
 */
static void
scan_curchan(struct ieee80211com *ic, unsigned long maxdwell)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	if ((ss->ss_flags & IEEE80211_SCAN_ACTIVE) &&
	    (ic->ic_curchan->ic_flags & IEEE80211_CHAN_PASSIVE) == 0) {
		struct ifnet *ifp = ic->ic_ifp;
		int i;

		/*
		 * Send a broadcast probe request followed by
		 * any specified directed probe requests.
		 * XXX suppress broadcast probe req?
		 * XXX remove dependence on ic/ic->ic_bss
		 * XXX move to policy code?
		 */
		ieee80211_send_probereq(ic->ic_bss,
			ic->ic_myaddr, ifp->if_broadcastaddr,
			ifp->if_broadcastaddr,
			"", 0,
			ic->ic_opt_ie, ic->ic_opt_ie_len);
		for (i = 0; i < ss->ss_nssid; i++)
			ieee80211_send_probereq(ic->ic_bss,
				ic->ic_myaddr, ifp->if_broadcastaddr,
				ifp->if_broadcastaddr,
				ss->ss_ssid[i].ssid,
				ss->ss_ssid[i].len,
				ic->ic_opt_ie, ic->ic_opt_ie_len);
	}
	callout_reset(&SCAN_PRIVATE(ss)->ss_scan_timer,
		maxdwell, scan_next, ss);
}

/*
 * Handle mindwell requirements completed; initiate a channel
 * change to the next channel asap.
 */
static void
scan_mindwell(struct ieee80211com *ic)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	callout_reset(&SCAN_PRIVATE(ss)->ss_scan_timer, 0, scan_next, ss);
}

/*
 * Switch to the next channel marked for scanning.
 */
static void
scan_next(void *arg)
{
#define	ISCAN_REP	(ISCAN_MINDWELL | ISCAN_START | ISCAN_DISCARD)
	struct ieee80211_scan_state *ss = (struct ieee80211_scan_state *) arg;
	struct ieee80211com *ic = ss->ss_ic;
	struct ieee80211_channel *chan;
	unsigned long maxdwell, scanend;
	int scanning, scandone;

	IEEE80211_LOCK(ic);
	scanning = (ic->ic_flags & IEEE80211_F_SCAN) != 0;
	IEEE80211_UNLOCK(ic);
	if (!scanning)			/* canceled */
		return;

again:
	scandone = (ss->ss_next >= ss->ss_last) ||
		(SCAN_PRIVATE(ss)->ss_iflags & ISCAN_CANCEL) != 0;
	scanend = SCAN_PRIVATE(ss)->ss_scanend;
	if (!scandone &&
	    (ss->ss_flags & IEEE80211_SCAN_GOTPICK) == 0 &&
	    ((SCAN_PRIVATE(ss)->ss_iflags & ISCAN_START) ||
	     time_before(ticks + ss->ss_mindwell, scanend))) {
		chan = ss->ss_chans[ss->ss_next++];

		/*
		 * Watch for truncation due to the scan end time.
		 */
		if (time_after(ticks + ss->ss_maxdwell, scanend))
			maxdwell = scanend - ticks;
		else
			maxdwell = ss->ss_maxdwell;

		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
		    "%s: chan %3d%c -> %3d%c [%s, dwell min %lu max %lu]\n",
		    __func__,
		    ieee80211_chan2ieee(ic, ic->ic_curchan),
		        channel_type(ic->ic_curchan),
		    ieee80211_chan2ieee(ic, chan), channel_type(chan),
		    (ss->ss_flags & IEEE80211_SCAN_ACTIVE) &&
			(chan->ic_flags & IEEE80211_CHAN_PASSIVE) == 0 ?
			"active" : "passive",
		    ss->ss_mindwell, maxdwell);

		/*
		 * Potentially change channel and phy mode.
		 */
		change_channel(ic, chan);

		/*
		 * Scan curchan.  Drivers for "intelligent hardware"
		 * override ic_scan_curchan to tell the device to do
		 * the work.  Otherwise we manage the work outselves;
		 * sending a probe request (as needed), and arming the
		 * timeout to switch channels after maxdwell ticks.
		 */
		ic->ic_scan_curchan(ic, maxdwell);

		SCAN_PRIVATE(ss)->ss_chanmindwell = ticks + ss->ss_mindwell;
		/* clear mindwell lock and initial channel change flush */
		SCAN_PRIVATE(ss)->ss_iflags &= ~ISCAN_REP;
	} else {
		ic->ic_scan_end(ic);		/* notify driver */
		/*
		 * Record scan complete time.  Note that we also do
		 * this when canceled so any background scan will
		 * not be restarted for a while.
		 */
		if (scandone)
			ic->ic_lastscan = ticks;
		/* return to the bss channel */
		if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
		    ic->ic_curchan != ic->ic_bsschan)
			change_channel(ic, ic->ic_bsschan);
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
		    !ss->ss_ops->scan_end(ss, ic) &&
		    (ss->ss_flags & IEEE80211_SCAN_ONCE) == 0 &&
		    time_before(ticks + ss->ss_mindwell, scanend)) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
			    "%s: done, restart "
			    "[ticks %u, dwell min %lu scanend %lu]\n",
			    __func__,
			    ticks, ss->ss_mindwell, scanend);
			ss->ss_next = 0;	/* reset to begining */
			if (ss->ss_flags & IEEE80211_SCAN_ACTIVE)
				ic->ic_stats.is_scan_active++;
			else
				ic->ic_stats.is_scan_passive++;

			ic->ic_scan_start(ic);	/* notify driver */
			goto again;
		} else {
			/* past here, scandone is ``true'' if not in bg mode */
			if ((ss->ss_flags & IEEE80211_SCAN_BGSCAN) == 0)
				scandone = 1;

			IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
			    "%s: %s, "
			    "[ticks %u, dwell min %lu scanend %lu]\n",
			    __func__, scandone ? "done" : "stopped",
			    ticks, ss->ss_mindwell, scanend);

			/*
			 * Clear the SCAN bit first in case frames are
			 * pending on the station power save queue.  If
			 * we defer this then the dispatch of the frames
			 * may generate a request to cancel scanning.
			 */
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
				ieee80211_sta_pwrsave(ic, 0);
				if (ss->ss_next >= ss->ss_last) {
					ieee80211_notify_scan_done(ic);
					ic->ic_flags_ext &= ~IEEE80211_FEXT_BGSCAN;
				}
			}
			SCAN_PRIVATE(ss)->ss_iflags &= ~ISCAN_CANCEL;
			ss->ss_flags &=
			    ~(IEEE80211_SCAN_ONCE | IEEE80211_SCAN_PICK1ST);
		}
	}
#undef ISCAN_REP
}

#ifdef IEEE80211_DEBUG
static void
dump_probe_beacon(uint8_t subtype, int isnew,
	const uint8_t mac[IEEE80211_ADDR_LEN],
	const struct ieee80211_scanparams *sp)
{

	printf("[%s] %s%s on chan %u (bss chan %u) ",
	    ether_sprintf(mac), isnew ? "new " : "",
	    ieee80211_mgt_subtype_name[subtype >> IEEE80211_FC0_SUBTYPE_SHIFT],
	    IEEE80211_CHAN2IEEE(sp->curchan), sp->bchan);
	ieee80211_print_essid(sp->ssid + 2, sp->ssid[1]);
	printf("\n");

	if (isnew) {
		printf("[%s] caps 0x%x bintval %u erp 0x%x", 
			ether_sprintf(mac), sp->capinfo, sp->bintval, sp->erp);
		if (sp->country != NULL) {
#ifdef __FreeBSD__
			printf(" country info %*D",
				sp->country[1], sp->country+2, " ");
#else
			int i;
			printf(" country info");
			for (i = 0; i < sp->country[1]; i++)
				printf(" %02x", sp->country[i+2]);
#endif
		}
		printf("\n");
	}
}
#endif /* IEEE80211_DEBUG */

/*
 * Process a beacon or probe response frame.
 */
void
ieee80211_add_scan(struct ieee80211com *ic,
	const struct ieee80211_scanparams *sp,
	const struct ieee80211_frame *wh,
	int subtype, int rssi, int noise, int rstamp)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	/*
	 * Frames received during startup are discarded to avoid
	 * using scan state setup on the initial entry to the timer
	 * callback.  This can occur because the device may enable
	 * rx prior to our doing the initial channel change in the
	 * timer routine (we defer the channel change to the timer
	 * code to simplify locking on linux).
	 */
	if (SCAN_PRIVATE(ss)->ss_iflags & ISCAN_DISCARD)
		return;
#ifdef IEEE80211_DEBUG
	if (ieee80211_msg_scan(ic) && (ic->ic_flags & IEEE80211_F_SCAN))
		dump_probe_beacon(subtype, 1, wh->i_addr2, sp);
#endif
	if (ss->ss_ops != NULL &&
	    ss->ss_ops->scan_add(ss, sp, wh, subtype, rssi, noise, rstamp)) {
		/*
		 * If we've reached the min dwell time terminate
		 * the timer so we'll switch to the next channel.
		 */
		if ((SCAN_PRIVATE(ss)->ss_iflags & ISCAN_MINDWELL) == 0 &&
		    time_after_eq(ticks, SCAN_PRIVATE(ss)->ss_chanmindwell)) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
			    "%s: chan %3d%c min dwell met (%u > %lu)\n",
			    __func__,
			    ieee80211_chan2ieee(ic, ic->ic_curchan),
				channel_type(ic->ic_curchan),
			    ticks, SCAN_PRIVATE(ss)->ss_chanmindwell);
			SCAN_PRIVATE(ss)->ss_iflags |= ISCAN_MINDWELL;
			/*
			 * NB: trigger at next clock tick or wait for the
			 * hardware
			 */
			ic->ic_scan_mindwell(ic);
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
ieee80211_scan_assoc_success(struct ieee80211com *ic, const uint8_t mac[])
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	if (ss->ss_ops != NULL) {
		IEEE80211_NOTE_MAC(ic, IEEE80211_MSG_SCAN,
			mac, "%s",  __func__);
		ss->ss_ops->scan_assoc_success(ss, mac);
	}
}

/*
 * Demerit a scan cache entry after failing to associate.
 */
void
ieee80211_scan_assoc_fail(struct ieee80211com *ic,
	const uint8_t mac[], int reason)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	if (ss->ss_ops != NULL) {
		IEEE80211_NOTE_MAC(ic, IEEE80211_MSG_SCAN, mac,
			"%s: reason %u", __func__, reason);
		ss->ss_ops->scan_assoc_fail(ss, mac, reason);
	}
}

/*
 * Iterate over the contents of the scan cache.
 */
void
ieee80211_scan_iterate(struct ieee80211com *ic,
	ieee80211_scan_iter_func *f, void *arg)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	if (ss->ss_ops != NULL)
		ss->ss_ops->scan_iterate(ss, f, arg);
}

/*
 * Flush the contents of the scan cache.
 */
void
ieee80211_scan_flush(struct ieee80211com *ic)
{
	struct ieee80211_scan_state *ss = ic->ic_scan;

	if (ss->ss_ops != NULL) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
			"%s\n",  __func__);
		ss->ss_ops->scan_flush(ss);
	}
}
