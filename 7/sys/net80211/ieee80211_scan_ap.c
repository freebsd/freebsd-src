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
__FBSDID("$FreeBSD$");

/*
 * IEEE 802.11 ap scanning support.
 */
#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/kernel.h>
#include <sys/module.h>
 
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>

#include <net/bpf.h>

struct ap_state {
	int	as_maxrssi[IEEE80211_CHAN_MAX];
};

static int ap_flush(struct ieee80211_scan_state *);

/* number of references from net80211 layer */
static	int nrefs = 0;

/*
 * Attach prior to any scanning work.
 */
static int
ap_attach(struct ieee80211_scan_state *ss)
{
	struct ap_state *as;

	MALLOC(as, struct ap_state *, sizeof(struct ap_state),
		M_80211_SCAN, M_NOWAIT);
	ss->ss_priv = as;
	ap_flush(ss);
	nrefs++;			/* NB: we assume caller locking */
	return 1;
}

/*
 * Cleanup any private state.
 */
static int
ap_detach(struct ieee80211_scan_state *ss)
{
	struct ap_state *as = ss->ss_priv;

	if (as != NULL) {
		KASSERT(nrefs > 0, ("imbalanced attach/detach"));
		nrefs--;		/* NB: we assume caller locking */
		FREE(as, M_80211_SCAN);
	}
	return 1;
}

/*
 * Flush all per-scan state.
 */
static int
ap_flush(struct ieee80211_scan_state *ss)
{
	struct ap_state *as = ss->ss_priv;

	memset(as->as_maxrssi, 0, sizeof(as->as_maxrssi));
	ss->ss_last = 0;		/* insure no channel will be picked */
	return 0;
}

static int
find11gchannel(struct ieee80211com *ic, int i, int freq)
{
	const struct ieee80211_channel *c;
	int j;

	/*
	 * The normal ordering in the channel list is b channel
	 * immediately followed by g so optimize the search for
	 * this.  We'll still do a full search just in case.
	 */
	for (j = i+1; j < ic->ic_nchans; j++) {
		c = &ic->ic_channels[j];
		if (c->ic_freq == freq && IEEE80211_IS_CHAN_ANYG(c))
			return 1;
	}
	for (j = 0; j < i; j++) {
		c = &ic->ic_channels[j];
		if (c->ic_freq == freq && IEEE80211_IS_CHAN_ANYG(c))
			return 1;
	}
	return 0;
}

/*
 * Start an ap scan by populating the channel list.
 */
static int
ap_start(struct ieee80211_scan_state *ss, struct ieee80211com *ic)
{
	struct ieee80211_channel *c;
	int i;

	ss->ss_last = 0;
	if (ic->ic_des_mode == IEEE80211_MODE_AUTO) {
		for (i = 0; i < ic->ic_nchans; i++) {
			c = &ic->ic_channels[i];
			if (IEEE80211_IS_CHAN_TURBO(c)) {
#ifdef IEEE80211_F_XR
				/* XR is not supported on turbo channels */
				if (ic->ic_flags & IEEE80211_F_XR)
					continue;
#endif
				/* dynamic channels are scanned in base mode */
				if (!IEEE80211_IS_CHAN_ST(c))
					continue;
			} else if (IEEE80211_IS_CHAN_HT(c)) {
				/* HT channels are scanned in legacy */
				continue;
			} else {
				/*
				 * Use any 11g channel instead of 11b one.
				 */
				if (IEEE80211_IS_CHAN_B(c) &&
				    find11gchannel(ic, i, c->ic_freq))
					continue;
			}
			if (ss->ss_last >= IEEE80211_SCAN_MAX)
				break;
			ss->ss_chans[ss->ss_last++] = c;
		}
	} else {
		static const u_int chanflags[IEEE80211_MODE_MAX] = {
			0,			/* IEEE80211_MODE_AUTO */
			IEEE80211_CHAN_A,	/* IEEE80211_MODE_11A */
			IEEE80211_CHAN_B,	/* IEEE80211_MODE_11B */
			IEEE80211_CHAN_G,	/* IEEE80211_MODE_11G */
			IEEE80211_CHAN_FHSS,	/* IEEE80211_MODE_FH */
			IEEE80211_CHAN_108A,	/* IEEE80211_MODE_TURBO_A */
			IEEE80211_CHAN_108G,	/* IEEE80211_MODE_TURBO_G */
			IEEE80211_CHAN_ST,	/* IEEE80211_MODE_STURBO_A */
			IEEE80211_CHAN_A,	/* IEEE80211_MODE_11NA */
			IEEE80211_CHAN_G,	/* IEEE80211_MODE_11NG */
		};
		u_int modeflags;

		modeflags = chanflags[ic->ic_des_mode];
		if ((ic->ic_flags & IEEE80211_F_TURBOP) &&
		    modeflags != IEEE80211_CHAN_ST) {
			if (ic->ic_des_mode == IEEE80211_MODE_11G)
				modeflags = IEEE80211_CHAN_108G;
			else
				modeflags = IEEE80211_CHAN_108A;
		}
		for (i = 0; i < ic->ic_nchans; i++) {
			c = &ic->ic_channels[i];
			if ((c->ic_flags & modeflags) != modeflags)
				continue;
#ifdef IEEE80211_F_XR
			/* XR is not supported on turbo channels */
			if (IEEE80211_IS_CHAN_TURBO(c) &&
			    (ic->ic_flags & IEEE80211_F_XR))
				continue;
#endif
			if (ss->ss_last >= IEEE80211_SCAN_MAX)
				break;
			/* 
			 * Do not select static turbo channels if
			 * the mode is not static turbo.
			 */
			if (IEEE80211_IS_CHAN_STURBO(c) &&
			    ic->ic_des_mode != IEEE80211_MODE_STURBO_A)
				continue;
			ss->ss_chans[ss->ss_last++] = c;
		}
	}
	ss->ss_next = 0;
	/* XXX tunables */
	ss->ss_mindwell = msecs_to_ticks(200);		/* 200ms */
	ss->ss_maxdwell = msecs_to_ticks(300);		/* 300ms */

#ifdef IEEE80211_DEBUG
	if (ieee80211_msg_scan(ic)) {
		if_printf(ic->ic_ifp, "scan set ");
		ieee80211_scan_dump_channels(ss);
		printf(" dwell min %ld max %ld\n",
			ss->ss_mindwell, ss->ss_maxdwell);
	}
#endif /* IEEE80211_DEBUG */

	return 0;
}

/*
 * Restart a bg scan.
 */
static int
ap_restart(struct ieee80211_scan_state *ss, struct ieee80211com *ic)
{
	return 0;
}

/*
 * Cancel an ongoing scan.
 */
static int
ap_cancel(struct ieee80211_scan_state *ss, struct ieee80211com *ic)
{
	return 0;
}

/*
 * Record max rssi on channel.
 */
static int
ap_add(struct ieee80211_scan_state *ss,
	const struct ieee80211_scanparams *sp,
	const struct ieee80211_frame *wh,
	int subtype, int rssi, int noise, int rstamp)
{
	struct ap_state *as = ss->ss_priv;
	struct ieee80211com *ic = ss->ss_ic;
	int chan;

	chan = ieee80211_chan2ieee(ic, ic->ic_curchan);
	/* XXX better quantification of channel use? */
	/* XXX count bss's? */
	if (rssi > as->as_maxrssi[chan])
		as->as_maxrssi[chan] = rssi;
	/* XXX interference, turbo requirements */
	return 1;
}

/*
 * Pick a quiet channel to use for ap operation.
 */
static int
ap_end(struct ieee80211_scan_state *ss, struct ieee80211com *ic)
{
	struct ap_state *as = ss->ss_priv;
	int i, chan, bestchan, bestchanix;

	KASSERT(ic->ic_opmode == IEEE80211_M_HOSTAP,
		("wrong opmode %u", ic->ic_opmode));
	/* XXX select channel more intelligently, e.g. channel spread, power */
	bestchan = -1;
	bestchanix = 0;		/* NB: silence compiler */
	/* NB: use scan list order to preserve channel preference */
	for (i = 0; i < ss->ss_last; i++) {
		/*
		 * If the channel is unoccupied the max rssi
		 * should be zero; just take it.  Otherwise
		 * track the channel with the lowest rssi and
		 * use that when all channels appear occupied.
		 */
		/* XXX channel have interference? */
		chan = ieee80211_chan2ieee(ic, ss->ss_chans[i]);

		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
		    "%s: channel %u rssi %d bestchan %d bestchan rssi %d\n",
		    __func__, chan, as->as_maxrssi[chan],
		    bestchan, bestchan != -1 ? as->as_maxrssi[bestchan] : 0);

		if (as->as_maxrssi[chan] == 0) {
			bestchan = chan;
			bestchanix = i;
			/* XXX use other considerations */
			break;
		}
		if (bestchan == -1 ||
		    as->as_maxrssi[chan] < as->as_maxrssi[bestchan])
			bestchan = chan;
	}
	if (bestchan == -1) {
		/* no suitable channel, should not happen */
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
		    "%s: no suitable channel! (should not happen)\n", __func__);
		/* XXX print something? */
		return 0;			/* restart scan */
	} else {
		struct ieee80211_channel *c;

		/* XXX notify all vap's? */
		/*
		 * If this is a dynamic turbo frequency,
		 * start with normal mode first.
		 */
		c = ss->ss_chans[bestchanix];
		if (IEEE80211_IS_CHAN_TURBO(c) &&
		    !IEEE80211_IS_CHAN_STURBO(c)) { 
			c = ieee80211_find_channel(ic, c->ic_freq,
				c->ic_flags & ~IEEE80211_CHAN_TURBO);
			if (c == NULL) {
				/* should never happen ?? */
				return 0;
			}
		}
		ieee80211_create_ibss(ic,
		    ieee80211_ht_adjust_channel(ic, c, ic->ic_flags_ext));
		return 1;
	}
}

static void
ap_age(struct ieee80211_scan_state *ss)
{
	/* XXX is there anything meaningful to do? */
}

static void
ap_iterate(struct ieee80211_scan_state *ss,
	ieee80211_scan_iter_func *f, void *arg)
{
	/* NB: nothing meaningful we can do */
}

static void
ap_assoc_success(struct ieee80211_scan_state *ss,
	const uint8_t macaddr[IEEE80211_ADDR_LEN])
{
	/* should not be called */
}

static void
ap_assoc_fail(struct ieee80211_scan_state *ss,
	const uint8_t macaddr[IEEE80211_ADDR_LEN], int reason)
{
	/* should not be called */
}

static const struct ieee80211_scanner ap_default = {
	.scan_name		= "default",
	.scan_attach		= ap_attach,
	.scan_detach		= ap_detach,
	.scan_start		= ap_start,
	.scan_restart		= ap_restart,
	.scan_cancel		= ap_cancel,
	.scan_end		= ap_end,
	.scan_flush		= ap_flush,
	.scan_add		= ap_add,
	.scan_age		= ap_age,
	.scan_iterate		= ap_iterate,
	.scan_assoc_success	= ap_assoc_success,
	.scan_assoc_fail	= ap_assoc_fail,
};

/*
 * Module glue.
 */
static int
wlan_modevent(module_t mod, int type, void *unused)
{
	switch (type) {
	case MOD_LOAD:
		ieee80211_scanner_register(IEEE80211_M_HOSTAP, &ap_default);
		return 0;
	case MOD_UNLOAD:
	case MOD_QUIESCE:
		if (nrefs) {
			printf("wlan_scan_ap: still in use (%u dynamic refs)\n",
				nrefs);
			return EBUSY;
		}
		if (type == MOD_UNLOAD)
			ieee80211_scanner_unregister_all(&ap_default);
		return 0;
	}
	return EINVAL;
}

static moduledata_t wlan_mod = {
	"wlan_scan_ap",
	wlan_modevent,
	0
};
DECLARE_MODULE(wlan_scan_ap, wlan_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(wlan_scan_ap, 1);
MODULE_DEPEND(wlan_scan_ap, wlan, 1, 1, 1);
