/*-
 * Copyright (c) 2001 Atsushi Onoe
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

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/mbuf.h>   
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <sys/socket.h>
 
#include <net/if.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>

#include <net/bpf.h>

/*
 * Association id's are managed with a bit vector.
 */
#define	IEEE80211_AID_SET(b, w) \
	((w)[IEEE80211_AID(b) / 32] |= (1 << (IEEE80211_AID(b) % 32)))
#define	IEEE80211_AID_CLR(b, w) \
	((w)[IEEE80211_AID(b) / 32] &= ~(1 << (IEEE80211_AID(b) % 32)))
#define	IEEE80211_AID_ISSET(b, w) \
	((w)[IEEE80211_AID(b) / 32] & (1 << (IEEE80211_AID(b) % 32)))

#ifdef IEEE80211_DEBUG_REFCNT
#define REFCNT_LOC "%s (%s:%u) %p<%s> refcnt %d\n", __func__, func, line
#else
#define REFCNT_LOC "%s %p<%s> refcnt %d\n", __func__
#endif

static struct ieee80211_node *node_alloc(struct ieee80211_node_table *);
static void node_cleanup(struct ieee80211_node *);
static void node_free(struct ieee80211_node *);
static u_int8_t node_getrssi(const struct ieee80211_node *);

static void ieee80211_setup_node(struct ieee80211_node_table *,
		struct ieee80211_node *, const u_int8_t *);
static void _ieee80211_free_node(struct ieee80211_node *);
static void ieee80211_free_allnodes(struct ieee80211_node_table *);

static void ieee80211_timeout_scan_candidates(struct ieee80211_node_table *);
static void ieee80211_timeout_stations(struct ieee80211_node_table *);

static void ieee80211_set_tim(struct ieee80211_node *, int set);

static void ieee80211_node_table_init(struct ieee80211com *ic,
	struct ieee80211_node_table *nt, const char *name,
	int inact, int keyixmax,
	void (*timeout)(struct ieee80211_node_table *));
static void ieee80211_node_table_cleanup(struct ieee80211_node_table *nt);

MALLOC_DEFINE(M_80211_NODE, "80211node", "802.11 node state");

void
ieee80211_node_attach(struct ieee80211com *ic)
{

	ic->ic_node_alloc = node_alloc;
	ic->ic_node_free = node_free;
	ic->ic_node_cleanup = node_cleanup;
	ic->ic_node_getrssi = node_getrssi;

	/* default station inactivity timer setings */
	ic->ic_inact_init = IEEE80211_INACT_INIT;
	ic->ic_inact_auth = IEEE80211_INACT_AUTH;
	ic->ic_inact_run = IEEE80211_INACT_RUN;
	ic->ic_inact_probe = IEEE80211_INACT_PROBE;

	/* NB: driver should override */
	ic->ic_max_aid = IEEE80211_AID_DEF;
	ic->ic_set_tim = ieee80211_set_tim;
}

void
ieee80211_node_lateattach(struct ieee80211com *ic)
{
	struct ieee80211_rsnparms *rsn;

	if (ic->ic_max_aid > IEEE80211_AID_MAX)
		ic->ic_max_aid = IEEE80211_AID_MAX;
	MALLOC(ic->ic_aid_bitmap, u_int32_t *,
		howmany(ic->ic_max_aid, 32) * sizeof(u_int32_t),
		M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ic->ic_aid_bitmap == NULL) {
		/* XXX no way to recover */
		printf("%s: no memory for AID bitmap!\n", __func__);
		ic->ic_max_aid = 0;
	}

	/* XXX defer until using hostap/ibss mode */
	ic->ic_tim_len = howmany(ic->ic_max_aid, 8) * sizeof(u_int8_t);
	MALLOC(ic->ic_tim_bitmap, u_int8_t *, ic->ic_tim_len,
		M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ic->ic_tim_bitmap == NULL) {
		/* XXX no way to recover */
		printf("%s: no memory for TIM bitmap!\n", __func__);
	}

	ieee80211_node_table_init(ic, &ic->ic_sta, "station",
		IEEE80211_INACT_INIT, ic->ic_crypto.cs_max_keyix,
		ieee80211_timeout_stations);
	ieee80211_node_table_init(ic, &ic->ic_scan, "scan",
		IEEE80211_INACT_SCAN, 0,
		ieee80211_timeout_scan_candidates);

	ieee80211_reset_bss(ic);
	/*
	 * Setup "global settings" in the bss node so that
	 * each new station automatically inherits them.
	 */
	rsn = &ic->ic_bss->ni_rsn;
	/* WEP, TKIP, and AES-CCM are always supported */
	rsn->rsn_ucastcipherset |= 1<<IEEE80211_CIPHER_WEP;
	rsn->rsn_ucastcipherset |= 1<<IEEE80211_CIPHER_TKIP;
	rsn->rsn_ucastcipherset |= 1<<IEEE80211_CIPHER_AES_CCM;
	if (ic->ic_caps & IEEE80211_C_AES)
		rsn->rsn_ucastcipherset |= 1<<IEEE80211_CIPHER_AES_OCB;
	if (ic->ic_caps & IEEE80211_C_CKIP)
		rsn->rsn_ucastcipherset |= 1<<IEEE80211_CIPHER_CKIP;
	/*
	 * Default unicast cipher to WEP for 802.1x use.  If
	 * WPA is enabled the management code will set these
	 * values to reflect.
	 */
	rsn->rsn_ucastcipher = IEEE80211_CIPHER_WEP;
	rsn->rsn_ucastkeylen = 104 / NBBY;
	/*
	 * WPA says the multicast cipher is the lowest unicast
	 * cipher supported.  But we skip WEP which would
	 * otherwise be used based on this criteria.
	 */
	rsn->rsn_mcastcipher = IEEE80211_CIPHER_TKIP;
	rsn->rsn_mcastkeylen = 128 / NBBY;

	/*
	 * We support both WPA-PSK and 802.1x; the one used
	 * is determined by the authentication mode and the
	 * setting of the PSK state.
	 */
	rsn->rsn_keymgmtset = WPA_ASE_8021X_UNSPEC | WPA_ASE_8021X_PSK;
	rsn->rsn_keymgmt = WPA_ASE_8021X_PSK;

	ic->ic_auth = ieee80211_authenticator_get(ic->ic_bss->ni_authmode);
}

void
ieee80211_node_detach(struct ieee80211com *ic)
{

	if (ic->ic_bss != NULL) {
		ieee80211_free_node(ic->ic_bss);
		ic->ic_bss = NULL;
	}
	ieee80211_node_table_cleanup(&ic->ic_scan);
	ieee80211_node_table_cleanup(&ic->ic_sta);
	if (ic->ic_aid_bitmap != NULL) {
		FREE(ic->ic_aid_bitmap, M_DEVBUF);
		ic->ic_aid_bitmap = NULL;
	}
	if (ic->ic_tim_bitmap != NULL) {
		FREE(ic->ic_tim_bitmap, M_DEVBUF);
		ic->ic_tim_bitmap = NULL;
	}
}

/* 
 * Port authorize/unauthorize interfaces for use by an authenticator.
 */

void
ieee80211_node_authorize(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;

	ni->ni_flags |= IEEE80211_NODE_AUTH;
	ni->ni_inact_reload = ic->ic_inact_run;
}

void
ieee80211_node_unauthorize(struct ieee80211_node *ni)
{
	ni->ni_flags &= ~IEEE80211_NODE_AUTH;
}

/*
 * Set/change the channel.  The rate set is also updated as
 * to insure a consistent view by drivers.
 */
static void
ieee80211_set_chan(struct ieee80211com *ic,
	struct ieee80211_node *ni, struct ieee80211_channel *chan)
{
	if (chan == IEEE80211_CHAN_ANYC)	/* XXX while scanning */
		chan = ic->ic_curchan;
	ni->ni_chan = chan;
	ni->ni_rates = *ieee80211_get_suprates(ic, chan);
}

/*
 * AP scanning support.
 */

#ifdef IEEE80211_DEBUG
static void
dump_chanlist(const u_char chans[])
{
	const char *sep;
	int i;

	sep = " ";
	for (i = 0; i < IEEE80211_CHAN_MAX; i++)
		if (isset(chans, i)) {
			printf("%s%u", sep, i);
			sep = ", ";
		}
}
#endif /* IEEE80211_DEBUG */

/*
 * Initialize the channel set to scan based on the
 * of available channels and the current PHY mode.
 */
static void
ieee80211_reset_scan(struct ieee80211com *ic)
{

	/* XXX ic_des_chan should be handled with ic_chan_active */
	if (ic->ic_des_chan != IEEE80211_CHAN_ANYC) {
		memset(ic->ic_chan_scan, 0, sizeof(ic->ic_chan_scan));
		setbit(ic->ic_chan_scan,
			ieee80211_chan2ieee(ic, ic->ic_des_chan));
	} else
		memcpy(ic->ic_chan_scan, ic->ic_chan_active,
			sizeof(ic->ic_chan_active));
#ifdef IEEE80211_DEBUG
	if (ieee80211_msg_scan(ic)) {
		printf("%s: scan set:", __func__);
		dump_chanlist(ic->ic_chan_scan);
		printf(" start chan %u\n",
			ieee80211_chan2ieee(ic, ic->ic_curchan));
	}
#endif /* IEEE80211_DEBUG */
}

/*
 * Begin an active scan.
 */
void
ieee80211_begin_scan(struct ieee80211com *ic, int reset)
{

	ic->ic_scan.nt_scangen++;
	/*
	 * In all but hostap mode scanning starts off in
	 * an active mode before switching to passive.
	 */
	if (ic->ic_opmode != IEEE80211_M_HOSTAP) {
		ic->ic_flags |= IEEE80211_F_ASCAN;
		ic->ic_stats.is_scan_active++;
	} else
		ic->ic_stats.is_scan_passive++;
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
		"begin %s scan in %s mode, scangen %u\n",
		(ic->ic_flags & IEEE80211_F_ASCAN) ?  "active" : "passive",
		ieee80211_phymode_name[ic->ic_curmode], ic->ic_scan.nt_scangen);
	/*
	 * Clear scan state and flush any previously seen AP's.
	 */
	ieee80211_reset_scan(ic);
	if (reset)
		ieee80211_free_allnodes(&ic->ic_scan);

	ic->ic_flags |= IEEE80211_F_SCAN;

	/* Scan the next channel. */
	ieee80211_next_scan(ic);
}

/*
 * Switch to the next channel marked for scanning.
 */
int
ieee80211_next_scan(struct ieee80211com *ic)
{
	struct ieee80211_channel *chan;

	/*
	 * Insure any previous mgt frame timeouts don't fire.
	 * This assumes the driver does the right thing in
	 * flushing anything queued in the driver and below.
	 */
	ic->ic_mgt_timer = 0;
	ic->ic_flags_ext &= ~IEEE80211_FEXT_PROBECHAN;

	chan = ic->ic_curchan;
	do {
		if (++chan > &ic->ic_channels[IEEE80211_CHAN_MAX])
			chan = &ic->ic_channels[0];
		if (isset(ic->ic_chan_scan, ieee80211_chan2ieee(ic, chan))) {
			clrbit(ic->ic_chan_scan, ieee80211_chan2ieee(ic, chan));
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
			    "%s: chan %d->%d\n", __func__,
			    ieee80211_chan2ieee(ic, ic->ic_curchan),
			    ieee80211_chan2ieee(ic, chan));
			ic->ic_curchan = chan;
			/*
			 * XXX drivers should do this as needed,
			 * XXX for now maintain compatibility
			 */
			ic->ic_bss->ni_rates = *ieee80211_get_suprates(ic, chan);
			ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
			return 1;
		}
	} while (chan != ic->ic_curchan);
	ieee80211_end_scan(ic);
	return 0;
}

/*
 * Probe the curent channel, if allowed, while scanning.
 * If the channel is not marked passive-only then send
 * a probe request immediately.  Otherwise mark state and
 * listen for beacons on the channel; if we receive something
 * then we'll transmit a probe request.
 */
void
ieee80211_probe_curchan(struct ieee80211com *ic, int force)
{
	struct ifnet *ifp = ic->ic_ifp;

	if ((ic->ic_curchan->ic_flags & IEEE80211_CHAN_PASSIVE) == 0 || force) {
		/*
		 * XXX send both broadcast+directed probe request
		 */
		ieee80211_send_probereq(ic->ic_bss,
			ic->ic_myaddr, ifp->if_broadcastaddr,
			ifp->if_broadcastaddr,
			ic->ic_des_essid, ic->ic_des_esslen,
			ic->ic_opt_ie, ic->ic_opt_ie_len);
	} else
		ic->ic_flags_ext |= IEEE80211_FEXT_PROBECHAN;
}

static __inline void
copy_bss(struct ieee80211_node *nbss, const struct ieee80211_node *obss)
{
	/* propagate useful state */
	nbss->ni_authmode = obss->ni_authmode;
	nbss->ni_txpower = obss->ni_txpower;
	nbss->ni_vlan = obss->ni_vlan;
	nbss->ni_rsn = obss->ni_rsn;
	/* XXX statistics? */
}

void
ieee80211_create_ibss(struct ieee80211com* ic, struct ieee80211_channel *chan)
{
	struct ieee80211_node_table *nt;
	struct ieee80211_node *ni;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
		"%s: creating ibss\n", __func__);

	/*
	 * Create the station/neighbor table.  Note that for adhoc
	 * mode we make the initial inactivity timer longer since
	 * we create nodes only through discovery and they typically
	 * are long-lived associations.
	 */
	nt = &ic->ic_sta;
	IEEE80211_NODE_LOCK(nt);
	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		nt->nt_name = "station";
		nt->nt_inact_init = ic->ic_inact_init;
	} else {
		nt->nt_name = "neighbor";
		nt->nt_inact_init = ic->ic_inact_run;
	}
	IEEE80211_NODE_UNLOCK(nt);

	ni = ieee80211_alloc_node(&ic->ic_sta, ic->ic_myaddr);
	if (ni == NULL) {
		/* XXX recovery? */
		return;
	}
	IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_myaddr);
	ni->ni_esslen = ic->ic_des_esslen;
	memcpy(ni->ni_essid, ic->ic_des_essid, ni->ni_esslen);
	copy_bss(ni, ic->ic_bss);
	ni->ni_intval = ic->ic_bintval;
	if (ic->ic_flags & IEEE80211_F_PRIVACY)
		ni->ni_capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if (ic->ic_phytype == IEEE80211_T_FH) {
		ni->ni_fhdwell = 200;	/* XXX */
		ni->ni_fhindex = 1;
	}
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		ic->ic_flags |= IEEE80211_F_SIBSS;
		ni->ni_capinfo |= IEEE80211_CAPINFO_IBSS;	/* XXX */
		if (ic->ic_flags & IEEE80211_F_DESBSSID)
			IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_des_bssid);
		else {
			get_random_bytes(ni->ni_bssid, IEEE80211_ADDR_LEN);
			/* clear group bit, add local bit */
			ni->ni_bssid[0] = (ni->ni_bssid[0] &~ 0x01) | 0x02;
		}
	} else if (ic->ic_opmode == IEEE80211_M_AHDEMO) {
		if (ic->ic_flags & IEEE80211_F_DESBSSID)
			IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_des_bssid);
		else
			memset(ni->ni_bssid, 0, IEEE80211_ADDR_LEN);
	}
	/* 
	 * Fix the channel and related attributes.
	 */
	ieee80211_set_chan(ic, ni, chan);
	ic->ic_curchan = chan;
	ic->ic_curmode = ieee80211_chan2mode(ic, chan);
	/*
	 * Do mode-specific rate setup.
	 */
	if (IEEE80211_IS_CHAN_FULL(chan) &&
	    (ic->ic_curmode == IEEE80211_MODE_11G ||
	     ic->ic_curmode == IEEE80211_MODE_11B))
		ieee80211_set11gbasicrates(&ni->ni_rates, ic->ic_curmode);

	(void) ieee80211_sta_join(ic, ieee80211_ref_node(ni));
}

void
ieee80211_reset_bss(struct ieee80211com *ic)
{
	struct ieee80211_node *ni, *obss;

	ieee80211_node_table_reset(&ic->ic_scan);
	ieee80211_node_table_reset(&ic->ic_sta);

	ni = ieee80211_alloc_node(&ic->ic_scan, ic->ic_myaddr);
	KASSERT(ni != NULL, ("unable to setup inital BSS node"));
	obss = ic->ic_bss;
	ic->ic_bss = ieee80211_ref_node(ni);
	if (obss != NULL) {
		copy_bss(ni, obss);
		ni->ni_intval = ic->ic_bintval;
		ieee80211_free_node(obss);
	}
}

/* XXX tunable */
#define	STA_FAILS_MAX	2		/* assoc failures before ignored */

static int
ieee80211_match_bss(struct ieee80211com *ic, struct ieee80211_node *ni)
{
        u_int8_t rate;
        int fail;

	fail = 0;
	if (isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic, ni->ni_chan)))
		fail |= 0x01;
	if (ic->ic_des_chan != IEEE80211_CHAN_ANYC &&
	    ni->ni_chan != ic->ic_des_chan)
		fail |= 0x01;
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) == 0)
			fail |= 0x02;
	} else {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_ESS) == 0)
			fail |= 0x02;
	}
	if (ic->ic_flags & IEEE80211_F_PRIVACY) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0)
			fail |= 0x04;
	} else {
		/* XXX does this mean privacy is supported or required? */
		if (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)
			fail |= 0x04;
	}
	rate = ieee80211_fix_rate(ni, &ni->ni_rates,
	     IEEE80211_F_JOIN | IEEE80211_F_DONEGO | IEEE80211_F_DOFRATE);
	if (rate & IEEE80211_RATE_BASIC)
		fail |= 0x08;
	if (ic->ic_des_esslen != 0 &&
	    (ni->ni_esslen != ic->ic_des_esslen ||
	     memcmp(ni->ni_essid, ic->ic_des_essid, ic->ic_des_esslen) != 0))
		fail |= 0x10;
	if ((ic->ic_flags & IEEE80211_F_DESBSSID) &&
	    !IEEE80211_ADDR_EQ(ic->ic_des_bssid, ni->ni_bssid))
		fail |= 0x20;
	if (ni->ni_fails >= STA_FAILS_MAX)
		fail |= 0x40;
#ifdef IEEE80211_DEBUG
	if (ieee80211_msg_scan(ic)) {
		printf(" %c %s",
		    fail & 0x40 ? '=' : fail & 0x80 ? '^' : fail ? '-' : '+',
		    ether_sprintf(ni->ni_macaddr));
		printf(" %s%c", ether_sprintf(ni->ni_bssid),
		    fail & 0x20 ? '!' : ' ');
		printf(" %3d%c", ieee80211_chan2ieee(ic, ni->ni_chan),
			fail & 0x01 ? '!' : ' ');
		printf(" %+4d", ni->ni_rssi);
		printf(" %2dM%c", (rate & IEEE80211_RATE_VAL) / 2,
		    fail & 0x08 ? '!' : ' ');
		printf(" %4s%c",
		    (ni->ni_capinfo & IEEE80211_CAPINFO_ESS) ? "ess" :
		    (ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) ? "ibss" :
		    "????",
		    fail & 0x02 ? '!' : ' ');
		printf(" %3s%c ",
		    (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) ?
		    "wep" : "no",
		    fail & 0x04 ? '!' : ' ');
		ieee80211_print_essid(ni->ni_essid, ni->ni_esslen);
		printf("%s\n", fail & 0x10 ? "!" : "");
	}
#endif
	return fail;
}

static __inline u_int8_t
maxrate(const struct ieee80211_node *ni)
{
	const struct ieee80211_rateset *rs = &ni->ni_rates;
	/* NB: assumes rate set is sorted (happens on frame receive) */
	return rs->rs_rates[rs->rs_nrates-1] & IEEE80211_RATE_VAL;
}

/*
 * Compare the capabilities of two nodes and decide which is
 * more desirable (return >0 if a is considered better).  Note
 * that we assume compatibility/usability has already been checked
 * so we don't need to (e.g. validate whether privacy is supported).
 * Used to select the best scan candidate for association in a BSS.
 */
static int
ieee80211_node_compare(struct ieee80211com *ic,
		       const struct ieee80211_node *a,
		       const struct ieee80211_node *b)
{
	u_int8_t maxa, maxb;
	u_int8_t rssia, rssib;
	int weight;

	/* privacy support preferred */
	if ((a->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) &&
	    (b->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0)
		return 1;
	if ((a->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0 &&
	    (b->ni_capinfo & IEEE80211_CAPINFO_PRIVACY))
		return -1;

	/* compare count of previous failures */
	weight = b->ni_fails - a->ni_fails;
	if (abs(weight) > 1)
		return weight;

	rssia = ic->ic_node_getrssi(a);
	rssib = ic->ic_node_getrssi(b);
	if (abs(rssib - rssia) < 5) {
		/* best/max rate preferred if signal level close enough XXX */
		maxa = maxrate(a);
		maxb = maxrate(b);
		if (maxa != maxb)
			return maxa - maxb;
		/* XXX use freq for channel preference */
		/* for now just prefer 5Ghz band to all other bands */
		if (IEEE80211_IS_CHAN_5GHZ(a->ni_chan) &&
		   !IEEE80211_IS_CHAN_5GHZ(b->ni_chan))
			return 1;
		if (!IEEE80211_IS_CHAN_5GHZ(a->ni_chan) &&
		     IEEE80211_IS_CHAN_5GHZ(b->ni_chan))
			return -1;
	}
	/* all things being equal, use signal level */
	return rssia - rssib;
}

/*
 * Mark an ongoing scan stopped.
 */
void
ieee80211_cancel_scan(struct ieee80211com *ic)
{

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN, "%s: end %s scan\n",
		__func__,
		(ic->ic_flags & IEEE80211_F_ASCAN) ?  "active" : "passive");

	ic->ic_flags &= ~(IEEE80211_F_SCAN | IEEE80211_F_ASCAN);
	ic->ic_flags_ext &= ~IEEE80211_FEXT_PROBECHAN;
}

/*
 * Complete a scan of potential channels.
 */
void
ieee80211_end_scan(struct ieee80211com *ic)
{
	struct ieee80211_node_table *nt = &ic->ic_scan;
	struct ieee80211_node *ni, *selbs;

	ieee80211_cancel_scan(ic);
	ieee80211_notify_scan_done(ic);

	if (ic->ic_opmode == IEEE80211_M_HOSTAP) {
		u_int8_t maxrssi[IEEE80211_CHAN_MAX];	/* XXX off stack? */
		int i, bestchan;
		u_int8_t rssi;

		/*
		 * The passive scan to look for existing AP's completed,
		 * select a channel to camp on.  Identify the channels
		 * that already have one or more AP's and try to locate
		 * an unoccupied one.  If that fails, pick a channel that
		 * looks to be quietest.
		 */
		memset(maxrssi, 0, sizeof(maxrssi));
		IEEE80211_NODE_LOCK(nt);
		TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
			rssi = ic->ic_node_getrssi(ni);
			i = ieee80211_chan2ieee(ic, ni->ni_chan);
			if (rssi > maxrssi[i])
				maxrssi[i] = rssi;
		}
		IEEE80211_NODE_UNLOCK(nt);
		/* XXX select channel more intelligently */
		bestchan = -1;
		for (i = 0; i < IEEE80211_CHAN_MAX; i++)
			if (isset(ic->ic_chan_active, i)) {
				/*
				 * If the channel is unoccupied the max rssi
				 * should be zero; just take it.  Otherwise
				 * track the channel with the lowest rssi and
				 * use that when all channels appear occupied.
				 */
				if (maxrssi[i] == 0) {
					bestchan = i;
					break;
				}
				if (bestchan == -1 ||
				    maxrssi[i] < maxrssi[bestchan])
					bestchan = i;
			}
		if (bestchan != -1) {
			ieee80211_create_ibss(ic, &ic->ic_channels[bestchan]);
			return;
		}
		/* no suitable channel, should not happen */
	}

	/*
	 * When manually sequencing the state machine; scan just once
	 * regardless of whether we have a candidate or not.  The
	 * controlling application is expected to setup state and
	 * initiate an association.
	 */
	if (ic->ic_roaming == IEEE80211_ROAMING_MANUAL)
		return;
	/*
	 * Automatic sequencing; look for a candidate and
	 * if found join the network.
	 */
	/* NB: unlocked read should be ok */
	if (TAILQ_FIRST(&nt->nt_node) == NULL) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN,
			"%s: no scan candidate\n", __func__);
  notfound:
		if (ic->ic_opmode == IEEE80211_M_IBSS &&
		    (ic->ic_flags & IEEE80211_F_IBSSON) &&
		    ic->ic_des_esslen != 0) {
			ieee80211_create_ibss(ic, ic->ic_ibss_chan);
			return;
		}
		/*
		 * Decrement the failure counts so entries will be
		 * reconsidered the next time around.  We really want
		 * to do this only for sta's where we've previously
		 * had some success.
		 */
		IEEE80211_NODE_LOCK(nt);
		TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
			if (ni->ni_fails)
				ni->ni_fails--;
		IEEE80211_NODE_UNLOCK(nt);
		/*
		 * Reset the list of channels to scan and start again.
		 */
		ieee80211_reset_scan(ic);
		ic->ic_flags |= IEEE80211_F_SCAN;
		ieee80211_next_scan(ic);
		return;
	}
	selbs = NULL;
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_SCAN, "\t%s\n",
	    "macaddr          bssid         chan  rssi rate flag  wep  essid");
	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (ieee80211_match_bss(ic, ni) == 0) {
			if (selbs == NULL)
				selbs = ni;
			else if (ieee80211_node_compare(ic, ni, selbs) > 0)
				selbs = ni;
		}
	}
	if (selbs != NULL)		/* NB: grab ref while dropping lock */
		(void) ieee80211_ref_node(selbs);
	IEEE80211_NODE_UNLOCK(nt);
	if (selbs == NULL)
		goto notfound;
	if (!ieee80211_sta_join(ic, selbs)) {
		ieee80211_free_node(selbs);
		goto notfound;
	}
}
 
/*
 * Handle 802.11 ad hoc network merge.  The
 * convention, set by the Wireless Ethernet Compatibility Alliance
 * (WECA), is that an 802.11 station will change its BSSID to match
 * the "oldest" 802.11 ad hoc network, on the same channel, that
 * has the station's desired SSID.  The "oldest" 802.11 network
 * sends beacons with the greatest TSF timestamp.
 *
 * The caller is assumed to validate TSF's before attempting a merge.
 *
 * Return !0 if the BSSID changed, 0 otherwise.
 */
int
ieee80211_ibss_merge(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;

	if (ni == ic->ic_bss ||
	    IEEE80211_ADDR_EQ(ni->ni_bssid, ic->ic_bss->ni_bssid)) {
		/* unchanged, nothing to do */
		return 0;
	}
	if (ieee80211_match_bss(ic, ni) != 0) {	/* capabilities mismatch */
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		    "%s: merge failed, capabilities mismatch\n", __func__);
		ic->ic_stats.is_ibss_capmismatch++;
		return 0;
	}
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		"%s: new bssid %s: %s preamble, %s slot time%s\n", __func__,
		ether_sprintf(ni->ni_bssid),
		ic->ic_flags&IEEE80211_F_SHPREAMBLE ? "short" : "long",
		ic->ic_flags&IEEE80211_F_SHSLOT ? "short" : "long",
		ic->ic_flags&IEEE80211_F_USEPROT ? ", protection" : ""
	);
	return ieee80211_sta_join(ic, ieee80211_ref_node(ni));
}

/*
 * Join the specified IBSS/BSS network.  The node is assumed to
 * be passed in with a held reference.
 */
int
ieee80211_sta_join(struct ieee80211com *ic, struct ieee80211_node *selbs)
{
	struct ieee80211_node *obss;

	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		struct ieee80211_node_table *nt;
		/*
		 * Fillin the neighbor table; it will already
		 * exist if we are simply switching mastership.
		 * XXX ic_sta always setup so this is unnecessary?
		 */
		nt = &ic->ic_sta;
		IEEE80211_NODE_LOCK(nt);
		nt->nt_name = "neighbor";
		nt->nt_inact_init = ic->ic_inact_run;
		IEEE80211_NODE_UNLOCK(nt);
	}

	/*
	 * Committed to selbs, setup state.
	 */
	obss = ic->ic_bss;
	ic->ic_bss = selbs;		/* NB: caller assumed to bump refcnt */
	if (obss != NULL) {
		copy_bss(selbs, obss);
		ieee80211_free_node(obss);
	}

	/*
	 * Delete unusable rates; we've already checked
	 * that the negotiated rate set is acceptable.
	 */
	ieee80211_fix_rate(ic->ic_bss, &ic->ic_bss->ni_rates,
		IEEE80211_F_DODEL | IEEE80211_F_JOIN);

	/*
	 * Set the erp state (mostly the slot time) to deal with
	 * the auto-select case; this should be redundant if the
	 * mode is locked.
	 */ 
	ic->ic_curmode = ieee80211_chan2mode(ic, selbs->ni_chan);
	ic->ic_curchan = selbs->ni_chan;
	ieee80211_reset_erp(ic);
	ieee80211_wme_initparams(ic);

	if (ic->ic_opmode == IEEE80211_M_STA)
		ieee80211_new_state(ic, IEEE80211_S_AUTH, -1);
	else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	return 1;
}

/*
 * Leave the specified IBSS/BSS network.  The node is assumed to
 * be passed in with a held reference.
 */
void
ieee80211_sta_leave(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	ic->ic_node_cleanup(ni);
	ieee80211_notify_node_leave(ic, ni);
}

static struct ieee80211_node *
node_alloc(struct ieee80211_node_table *nt)
{
	struct ieee80211_node *ni;

	MALLOC(ni, struct ieee80211_node *, sizeof(struct ieee80211_node),
		M_80211_NODE, M_NOWAIT | M_ZERO);
	return ni;
}

/*
 * Reclaim any resources in a node and reset any critical
 * state.  Typically nodes are free'd immediately after,
 * but in some cases the storage may be reused so we need
 * to insure consistent state (should probably fix that).
 */
static void
node_cleanup(struct ieee80211_node *ni)
{
#define	N(a)	(sizeof(a)/sizeof(a[0]))
	struct ieee80211com *ic = ni->ni_ic;
	int i, qlen;

	/* NB: preserve ni_table */
	if (ni->ni_flags & IEEE80211_NODE_PWR_MGT) {
		ic->ic_ps_sta--;
		ni->ni_flags &= ~IEEE80211_NODE_PWR_MGT;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER,
		    "[%s] power save mode off, %u sta's in ps mode\n",
		    ether_sprintf(ni->ni_macaddr), ic->ic_ps_sta);
	}
	/*
	 * Clear AREF flag that marks the authorization refcnt bump
	 * has happened.  This is probably not needed as the node
	 * should always be removed from the table so not found but
	 * do it just in case.
	 */
	ni->ni_flags &= ~IEEE80211_NODE_AREF;

	/*
	 * Drain power save queue and, if needed, clear TIM.
	 */
	IEEE80211_NODE_SAVEQ_DRAIN(ni, qlen);
	if (qlen != 0 && ic->ic_set_tim != NULL)
		ic->ic_set_tim(ni, 0);

	ni->ni_associd = 0;
	if (ni->ni_challenge != NULL) {
		FREE(ni->ni_challenge, M_DEVBUF);
		ni->ni_challenge = NULL;
	}
	/*
	 * Preserve SSID, WPA, and WME ie's so the bss node is
	 * reusable during a re-auth/re-assoc state transition.
	 * If we remove these data they will not be recreated
	 * because they come from a probe-response or beacon frame
	 * which cannot be expected prior to the association-response.
	 * This should not be an issue when operating in other modes
	 * as stations leaving always go through a full state transition
	 * which will rebuild this state.
	 *
	 * XXX does this leave us open to inheriting old state?
	 */
	for (i = 0; i < N(ni->ni_rxfrag); i++)
		if (ni->ni_rxfrag[i] != NULL) {
			m_freem(ni->ni_rxfrag[i]);
			ni->ni_rxfrag[i] = NULL;
		}
	/*
	 * Must be careful here to remove any key map entry w/o a LOR.
	 */
	ieee80211_node_delucastkey(ni);
#undef N
}

static void
node_free(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;

	ic->ic_node_cleanup(ni);
	if (ni->ni_wpa_ie != NULL)
		FREE(ni->ni_wpa_ie, M_DEVBUF);
	if (ni->ni_wme_ie != NULL)
		FREE(ni->ni_wme_ie, M_DEVBUF);
	IEEE80211_NODE_SAVEQ_DESTROY(ni);
	FREE(ni, M_80211_NODE);
}

static u_int8_t
node_getrssi(const struct ieee80211_node *ni)
{
	return ni->ni_rssi;
}

static void
ieee80211_setup_node(struct ieee80211_node_table *nt,
	struct ieee80211_node *ni, const u_int8_t *macaddr)
{
	struct ieee80211com *ic = nt->nt_ic;
	int hash;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,
		"%s %p<%s> in %s table\n", __func__, ni,
		ether_sprintf(macaddr), nt->nt_name);

	IEEE80211_ADDR_COPY(ni->ni_macaddr, macaddr);
	hash = IEEE80211_NODE_HASH(macaddr);
	ieee80211_node_initref(ni);		/* mark referenced */
	ni->ni_chan = IEEE80211_CHAN_ANYC;
	ni->ni_authmode = IEEE80211_AUTH_OPEN;
	ni->ni_txpower = ic->ic_txpowlimit;	/* max power */
	ieee80211_crypto_resetkey(ic, &ni->ni_ucastkey, IEEE80211_KEYIX_NONE);
	ni->ni_inact_reload = nt->nt_inact_init;
	ni->ni_inact = ni->ni_inact_reload;
	IEEE80211_NODE_SAVEQ_INIT(ni, "unknown");

	IEEE80211_NODE_LOCK(nt);
	TAILQ_INSERT_TAIL(&nt->nt_node, ni, ni_list);
	LIST_INSERT_HEAD(&nt->nt_hash[hash], ni, ni_hash);
	ni->ni_table = nt;
	ni->ni_ic = ic;
	IEEE80211_NODE_UNLOCK(nt);
}

struct ieee80211_node *
ieee80211_alloc_node(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni;

	ni = ic->ic_node_alloc(nt);
	if (ni != NULL)
		ieee80211_setup_node(nt, ni, macaddr);
	else
		ic->ic_stats.is_rx_nodealloc++;
	return ni;
}

/*
 * Craft a temporary node suitable for sending a management frame
 * to the specified station.  We craft only as much state as we
 * need to do the work since the node will be immediately reclaimed
 * once the send completes.
 */
struct ieee80211_node *
ieee80211_tmp_node(struct ieee80211com *ic, const u_int8_t *macaddr)
{
	struct ieee80211_node *ni;

	ni = ic->ic_node_alloc(&ic->ic_sta);
	if (ni != NULL) {
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,
			"%s %p<%s>\n", __func__, ni, ether_sprintf(macaddr));

		IEEE80211_ADDR_COPY(ni->ni_macaddr, macaddr);
		IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_bss->ni_bssid);
		ieee80211_node_initref(ni);		/* mark referenced */
		ni->ni_txpower = ic->ic_bss->ni_txpower;
		/* NB: required by ieee80211_fix_rate */
		ieee80211_set_chan(ic, ni, ic->ic_bss->ni_chan);
		ieee80211_crypto_resetkey(ic, &ni->ni_ucastkey,
			IEEE80211_KEYIX_NONE);
		/* XXX optimize away */
		IEEE80211_NODE_SAVEQ_INIT(ni, "unknown");

		ni->ni_table = NULL;		/* NB: pedantic */
		ni->ni_ic = ic;
	} else {
		/* XXX msg */
		ic->ic_stats.is_rx_nodealloc++;
	}
	return ni;
}

struct ieee80211_node *
ieee80211_dup_bss(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni;

	ni = ic->ic_node_alloc(nt);
	if (ni != NULL) {
		ieee80211_setup_node(nt, ni, macaddr);
		/*
		 * Inherit from ic_bss.
		 */
		ni->ni_authmode = ic->ic_bss->ni_authmode;
		ni->ni_txpower = ic->ic_bss->ni_txpower;
		ni->ni_vlan = ic->ic_bss->ni_vlan;	/* XXX?? */
		IEEE80211_ADDR_COPY(ni->ni_bssid, ic->ic_bss->ni_bssid);
		ieee80211_set_chan(ic, ni, ic->ic_bss->ni_chan);
		ni->ni_rsn = ic->ic_bss->ni_rsn;
	} else
		ic->ic_stats.is_rx_nodealloc++;
	return ni;
}

static struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
_ieee80211_find_node_debug(struct ieee80211_node_table *nt,
	const u_int8_t *macaddr, const char *func, int line)
#else
_ieee80211_find_node(struct ieee80211_node_table *nt,
	const u_int8_t *macaddr)
#endif
{
	struct ieee80211_node *ni;
	int hash;

	IEEE80211_NODE_LOCK_ASSERT(nt);

	hash = IEEE80211_NODE_HASH(macaddr);
	LIST_FOREACH(ni, &nt->nt_hash[hash], ni_hash) {
		if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr)) {
			ieee80211_ref_node(ni);	/* mark referenced */
#ifdef IEEE80211_DEBUG_REFCNT
			IEEE80211_DPRINTF(nt->nt_ic, IEEE80211_MSG_NODE,
			    "%s (%s:%u) %p<%s> refcnt %d\n", __func__,
			    func, line,
			    ni, ether_sprintf(ni->ni_macaddr),
			    ieee80211_node_refcnt(ni));
#endif
			return ni;
		}
	}
	return NULL;
}
#ifdef IEEE80211_DEBUG_REFCNT
#define	_ieee80211_find_node(nt, mac) \
	_ieee80211_find_node_debug(nt, mac, func, line)
#endif

struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_node_debug(struct ieee80211_node_table *nt,
	const u_int8_t *macaddr, const char *func, int line)
#else
ieee80211_find_node(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
#endif
{
	struct ieee80211_node *ni;

	IEEE80211_NODE_LOCK(nt);
	ni = _ieee80211_find_node(nt, macaddr);
	IEEE80211_NODE_UNLOCK(nt);
	return ni;
}

/*
 * Fake up a node; this handles node discovery in adhoc mode.
 * Note that for the driver's benefit we we treat this like
 * an association so the driver has an opportunity to setup
 * it's private state.
 */
struct ieee80211_node *
ieee80211_fakeup_adhoc_node(struct ieee80211_node_table *nt,
	const u_int8_t macaddr[IEEE80211_ADDR_LEN])
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni;

	IEEE80211_DPRINTF(nt->nt_ic, IEEE80211_MSG_NODE,
	    "%s: mac<%s>\n", __func__, ether_sprintf(macaddr));
	ni = ieee80211_dup_bss(nt, macaddr);
	if (ni != NULL) {
		/* XXX no rate negotiation; just dup */
		ni->ni_rates = ic->ic_bss->ni_rates;
		if (ic->ic_newassoc != NULL)
			ic->ic_newassoc(ni, 1);
		/* XXX not right for 802.1x/WPA */
		ieee80211_node_authorize(ni);
		if (ic->ic_opmode == IEEE80211_M_AHDEMO) {
			/*
			 * Blindly propagate capabilities based on the
			 * local configuration.  In particular this permits
			 * us to use QoS to disable ACK's.
			 */
			if (ic->ic_flags & IEEE80211_F_WME)
				ni->ni_flags |= IEEE80211_NODE_QOS;
		}
	}
	return ni;
}

#ifdef IEEE80211_DEBUG
static void
dump_probe_beacon(u_int8_t subtype, int isnew,
	const u_int8_t mac[IEEE80211_ADDR_LEN],
	const struct ieee80211_scanparams *sp)
{

	printf("[%s] %s%s on chan %u (bss chan %u) ",
	    ether_sprintf(mac), isnew ? "new " : "",
	    ieee80211_mgt_subtype_name[subtype >> IEEE80211_FC0_SUBTYPE_SHIFT],
	    sp->chan, sp->bchan);
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

static void
saveie(u_int8_t **iep, const u_int8_t *ie)
{

	if (ie == NULL)
		*iep = NULL;
	else
		ieee80211_saveie(iep, ie);
}

/*
 * Process a beacon or probe response frame.
 */
void
ieee80211_add_scan(struct ieee80211com *ic,
	const struct ieee80211_scanparams *sp,
	const struct ieee80211_frame *wh,
	int subtype, int rssi, int rstamp)
{
#define	ISPROBE(_st)	((_st) == IEEE80211_FC0_SUBTYPE_PROBE_RESP)
	struct ieee80211_node_table *nt = &ic->ic_scan;
	struct ieee80211_node *ni;
	int newnode = 0;

	ni = ieee80211_find_node(nt, wh->i_addr2);
	if (ni == NULL) {
		/*
		 * Create a new entry.
		 */
		ni = ic->ic_node_alloc(nt);
		if (ni == NULL) {
			ic->ic_stats.is_rx_nodealloc++;
			return;
		}
		ieee80211_setup_node(nt, ni, wh->i_addr2);
		/*
		 * XXX inherit from ic_bss.
		 */
		ni->ni_authmode = ic->ic_bss->ni_authmode;
		ni->ni_txpower = ic->ic_bss->ni_txpower;
		ni->ni_vlan = ic->ic_bss->ni_vlan;	/* XXX?? */
		ieee80211_set_chan(ic, ni, ic->ic_curchan);
		ni->ni_rsn = ic->ic_bss->ni_rsn;
		newnode = 1;
	}
#ifdef IEEE80211_DEBUG
	if (ieee80211_msg_scan(ic) && (ic->ic_flags & IEEE80211_F_SCAN))
		dump_probe_beacon(subtype, newnode, wh->i_addr2, sp);
#endif
	/* XXX ap beaconing multiple ssid w/ same bssid */
	if (sp->ssid[1] != 0 &&
	    (ISPROBE(subtype) || ni->ni_esslen == 0)) {
		ni->ni_esslen = sp->ssid[1];
		memset(ni->ni_essid, 0, sizeof(ni->ni_essid));
		memcpy(ni->ni_essid, sp->ssid + 2, sp->ssid[1]);
	}
	ni->ni_scangen = ic->ic_scan.nt_scangen;
	IEEE80211_ADDR_COPY(ni->ni_bssid, wh->i_addr3);
	ni->ni_rssi = rssi;
	ni->ni_rstamp = rstamp;
	memcpy(ni->ni_tstamp.data, sp->tstamp, sizeof(ni->ni_tstamp));
	ni->ni_intval = sp->bintval;
	ni->ni_capinfo = sp->capinfo;
	ni->ni_chan = &ic->ic_channels[sp->chan];
	ni->ni_fhdwell = sp->fhdwell;
	ni->ni_fhindex = sp->fhindex;
	ni->ni_erp = sp->erp;
	if (sp->tim != NULL) {
		struct ieee80211_tim_ie *ie =
		    (struct ieee80211_tim_ie *) sp->tim;

		ni->ni_dtim_count = ie->tim_count;
		ni->ni_dtim_period = ie->tim_period;
	}
	/*
	 * Record the byte offset from the mac header to
	 * the start of the TIM information element for
	 * use by hardware and/or to speedup software
	 * processing of beacon frames.
	 */
	ni->ni_timoff = sp->timoff;
	/*
	 * Record optional information elements that might be
	 * used by applications or drivers.
	 */
	saveie(&ni->ni_wme_ie, sp->wme);
	saveie(&ni->ni_wpa_ie, sp->wpa);

	/* NB: must be after ni_chan is setup */
	ieee80211_setup_rates(ni, sp->rates, sp->xrates, IEEE80211_F_DOSORT);

	if (!newnode)
		ieee80211_free_node(ni);
#undef ISPROBE
}

void
ieee80211_init_neighbor(struct ieee80211_node *ni,
	const struct ieee80211_frame *wh,
	const struct ieee80211_scanparams *sp)
{

	IEEE80211_DPRINTF(ni->ni_ic, IEEE80211_MSG_NODE,
	    "%s: %p<%s>\n", __func__, ni, ether_sprintf(ni->ni_macaddr));
	ni->ni_esslen = sp->ssid[1];
	memcpy(ni->ni_essid, sp->ssid + 2, sp->ssid[1]);
	IEEE80211_ADDR_COPY(ni->ni_bssid, wh->i_addr3);
	memcpy(ni->ni_tstamp.data, sp->tstamp, sizeof(ni->ni_tstamp));
	ni->ni_intval = sp->bintval;
	ni->ni_capinfo = sp->capinfo;
	ni->ni_chan = ni->ni_ic->ic_curchan;
	ni->ni_fhdwell = sp->fhdwell;
	ni->ni_fhindex = sp->fhindex;
	ni->ni_erp = sp->erp;
	ni->ni_timoff = sp->timoff;
	if (sp->wme != NULL)
		ieee80211_saveie(&ni->ni_wme_ie, sp->wme);
	if (sp->wpa != NULL)
		ieee80211_saveie(&ni->ni_wpa_ie, sp->wpa);

	/* NB: must be after ni_chan is setup */
	ieee80211_setup_rates(ni, sp->rates, sp->xrates,
		IEEE80211_F_DOSORT | IEEE80211_F_DOFRATE |
		IEEE80211_F_DONEGO | IEEE80211_F_DODEL);
}

/*
 * Do node discovery in adhoc mode on receipt of a beacon
 * or probe response frame.  Note that for the driver's
 * benefit we we treat this like an association so the
 * driver has an opportunity to setup it's private state.
 */
struct ieee80211_node *
ieee80211_add_neighbor(struct ieee80211com *ic,
	const struct ieee80211_frame *wh,
	const struct ieee80211_scanparams *sp)
{
	struct ieee80211_node *ni;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,
	    "%s: mac<%s>\n", __func__, ether_sprintf(wh->i_addr2));
	ni = ieee80211_dup_bss(&ic->ic_sta, wh->i_addr2);/* XXX alloc_node? */
	if (ni != NULL) {
		ieee80211_init_neighbor(ni, wh, sp);
		if (ic->ic_newassoc != NULL)
			ic->ic_newassoc(ni, 1);
		/* XXX not right for 802.1x/WPA */
		ieee80211_node_authorize(ni);
	}
	return ni;
}

#define	IS_CTL(wh) \
	((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL)
#define	IS_PSPOLL(wh) \
	((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_PS_POLL)
/*
 * Locate the node for sender, track state, and then pass the
 * (referenced) node up to the 802.11 layer for its use.  We
 * are required to pass some node so we fall back to ic_bss
 * when this frame is from an unknown sender.  The 802.11 layer
 * knows this means the sender wasn't in the node table and
 * acts accordingly. 
 */
struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_rxnode_debug(struct ieee80211com *ic,
	const struct ieee80211_frame_min *wh, const char *func, int line)
#else
ieee80211_find_rxnode(struct ieee80211com *ic,
	const struct ieee80211_frame_min *wh)
#endif
{
	struct ieee80211_node_table *nt;
	struct ieee80211_node *ni;

	/* XXX may want scanned nodes in the neighbor table for adhoc */
	if (ic->ic_opmode == IEEE80211_M_STA ||
	    ic->ic_opmode == IEEE80211_M_MONITOR ||
	    (ic->ic_flags & IEEE80211_F_SCAN))
		nt = &ic->ic_scan;
	else
		nt = &ic->ic_sta;
	/* XXX check ic_bss first in station mode */
	/* XXX 4-address frames? */
	IEEE80211_NODE_LOCK(nt);
	if (IS_CTL(wh) && !IS_PSPOLL(wh) /*&& !IS_RTS(ah)*/)
		ni = _ieee80211_find_node(nt, wh->i_addr1);
	else
		ni = _ieee80211_find_node(nt, wh->i_addr2);
	if (ni == NULL)
		ni = ieee80211_ref_node(ic->ic_bss);
	IEEE80211_NODE_UNLOCK(nt);

	return ni;
}

/*
 * Like ieee80211_find_rxnode but use the supplied h/w
 * key index as a hint to locate the node in the key
 * mapping table.  If an entry is present at the key
 * index we return it; otherwise do a normal lookup and
 * update the mapping table if the station has a unicast
 * key assigned to it.
 */
struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_rxnode_withkey_debug(struct ieee80211com *ic,
	const struct ieee80211_frame_min *wh, ieee80211_keyix keyix,
	const char *func, int line)
#else
ieee80211_find_rxnode_withkey(struct ieee80211com *ic,
	const struct ieee80211_frame_min *wh, ieee80211_keyix keyix)
#endif
{
	struct ieee80211_node_table *nt;
	struct ieee80211_node *ni;

	if (ic->ic_opmode == IEEE80211_M_STA ||
	    ic->ic_opmode == IEEE80211_M_MONITOR ||
	    (ic->ic_flags & IEEE80211_F_SCAN))
		nt = &ic->ic_scan;
	else
		nt = &ic->ic_sta;
	IEEE80211_NODE_LOCK(nt);
	if (nt->nt_keyixmap != NULL && keyix < nt->nt_keyixmax)
		ni = nt->nt_keyixmap[keyix];
	else
		ni = NULL;
	if (ni == NULL) {
		if (IS_CTL(wh) && !IS_PSPOLL(wh) /*&& !IS_RTS(ah)*/)
			ni = _ieee80211_find_node(nt, wh->i_addr1);
		else
			ni = _ieee80211_find_node(nt, wh->i_addr2);
		if (ni == NULL)
			ni = ieee80211_ref_node(ic->ic_bss);
		if (nt->nt_keyixmap != NULL) {
			/*
			 * If the station has a unicast key cache slot
			 * assigned update the key->node mapping table.
			 */
			keyix = ni->ni_ucastkey.wk_rxkeyix;
			/* XXX can keyixmap[keyix] != NULL? */
			if (keyix < nt->nt_keyixmax &&
			    nt->nt_keyixmap[keyix] == NULL) {
				IEEE80211_DPRINTF(ni->ni_ic, IEEE80211_MSG_NODE,
				    "%s: add key map entry %p<%s> refcnt %d\n",
				    __func__, ni, ether_sprintf(ni->ni_macaddr),
				    ieee80211_node_refcnt(ni)+1);
				nt->nt_keyixmap[keyix] = ieee80211_ref_node(ni);
			}
		}
	} else {
		ieee80211_ref_node(ni);
	}
	IEEE80211_NODE_UNLOCK(nt);

	return ni;
}
#undef IS_PSPOLL
#undef IS_CTL

/*
 * Return a reference to the appropriate node for sending
 * a data frame.  This handles node discovery in adhoc networks.
 */
struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_txnode_debug(struct ieee80211com *ic, const u_int8_t *macaddr,
	const char *func, int line)
#else
ieee80211_find_txnode(struct ieee80211com *ic, const u_int8_t *macaddr)
#endif
{
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211_node *ni;

	/*
	 * The destination address should be in the node table
	 * unless this is a multicast/broadcast frame.  We can
	 * also optimize station mode operation, all frames go
	 * to the bss node.
	 */
	/* XXX can't hold lock across dup_bss 'cuz of recursive locking */
	IEEE80211_NODE_LOCK(nt);
	if (ic->ic_opmode == IEEE80211_M_STA || IEEE80211_IS_MULTICAST(macaddr))
		ni = ieee80211_ref_node(ic->ic_bss);
	else {
		ni = _ieee80211_find_node(nt, macaddr);
		if (ic->ic_opmode == IEEE80211_M_HOSTAP && 
		    (ni != NULL && ni->ni_associd == 0)) {
			/*
			 * Station is not associated; don't permit the
			 * data frame to be sent by returning NULL.  This
			 * is kinda a kludge but the least intrusive way
			 * to add this check into all drivers.
			 */
			ieee80211_unref_node(&ni);	/* NB: null's ni */
		}
	}
	IEEE80211_NODE_UNLOCK(nt);

	if (ni == NULL) {
		if (ic->ic_opmode == IEEE80211_M_IBSS ||
		    ic->ic_opmode == IEEE80211_M_AHDEMO) {
			/*
			 * In adhoc mode cons up a node for the destination.
			 * Note that we need an additional reference for the
			 * caller to be consistent with _ieee80211_find_node.
			 */
			ni = ieee80211_fakeup_adhoc_node(nt, macaddr);
			if (ni != NULL)
				(void) ieee80211_ref_node(ni);
		} else {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_OUTPUT,
				"[%s] no node, discard frame (%s)\n",
				ether_sprintf(macaddr), __func__);
			ic->ic_stats.is_tx_nonode++;
		}
	}
	return ni;
}

/*
 * Like find but search based on the channel too.
 */
struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_node_with_channel_debug(struct ieee80211_node_table *nt,
	const u_int8_t *macaddr, struct ieee80211_channel *chan,
	const char *func, int line)
#else
ieee80211_find_node_with_channel(struct ieee80211_node_table *nt,
	const u_int8_t *macaddr, struct ieee80211_channel *chan)
#endif
{
	struct ieee80211_node *ni;
	int hash;

	hash = IEEE80211_NODE_HASH(macaddr);
	IEEE80211_NODE_LOCK(nt);
	LIST_FOREACH(ni, &nt->nt_hash[hash], ni_hash) {
		if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr) &&
		    ni->ni_chan == chan) {
			ieee80211_ref_node(ni);		/* mark referenced */
			IEEE80211_DPRINTF(nt->nt_ic, IEEE80211_MSG_NODE,
			    REFCNT_LOC, ni, ether_sprintf(ni->ni_macaddr),
			    ieee80211_node_refcnt(ni));
			break;
		}
	}
	IEEE80211_NODE_UNLOCK(nt);
	return ni;
}

/*
 * Like find but search based on the ssid too.
 */
struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_node_with_ssid_debug(struct ieee80211_node_table *nt,
	const u_int8_t *macaddr, u_int ssidlen, const u_int8_t *ssid,
	const char *func, int line)
#else
ieee80211_find_node_with_ssid(struct ieee80211_node_table *nt,
	const u_int8_t *macaddr, u_int ssidlen, const u_int8_t *ssid)
#endif
{
#define	MATCH_SSID(ni, ssid, ssidlen) \
	(ni->ni_esslen == ssidlen && memcmp(ni->ni_essid, ssid, ssidlen) == 0)
	static const u_int8_t zeromac[IEEE80211_ADDR_LEN];
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni;
	int hash;

	IEEE80211_NODE_LOCK(nt);
	/*
	 * A mac address that is all zero means match only the ssid;
	 * otherwise we must match both.
	 */
	if (IEEE80211_ADDR_EQ(macaddr, zeromac)) {
		TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
			if (MATCH_SSID(ni, ssid, ssidlen))
				break;
		}
	} else {
		hash = IEEE80211_NODE_HASH(macaddr);
		LIST_FOREACH(ni, &nt->nt_hash[hash], ni_hash) {
			if (IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr) &&
			    MATCH_SSID(ni, ssid, ssidlen))
				break;
		}
	}
	if (ni != NULL) {
		ieee80211_ref_node(ni);	/* mark referenced */
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,
		     REFCNT_LOC, ni, ether_sprintf(ni->ni_macaddr),
		     ieee80211_node_refcnt(ni));
	}
	IEEE80211_NODE_UNLOCK(nt);
	return ni;
#undef MATCH_SSID
}

static void
_ieee80211_free_node(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_node_table *nt = ni->ni_table;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,
		"%s %p<%s> in %s table\n", __func__, ni,
		ether_sprintf(ni->ni_macaddr),
		nt != NULL ? nt->nt_name : "<gone>");

	IEEE80211_AID_CLR(ni->ni_associd, ic->ic_aid_bitmap);
	if (nt != NULL) {
		TAILQ_REMOVE(&nt->nt_node, ni, ni_list);
		LIST_REMOVE(ni, ni_hash);
	}
	ic->ic_node_free(ni);
}

void
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_free_node_debug(struct ieee80211_node *ni, const char *func, int line)
#else
ieee80211_free_node(struct ieee80211_node *ni)
#endif
{
	struct ieee80211_node_table *nt = ni->ni_table;

#ifdef IEEE80211_DEBUG_REFCNT
	IEEE80211_DPRINTF(ni->ni_ic, IEEE80211_MSG_NODE,
		"%s (%s:%u) %p<%s> refcnt %d\n", __func__, func, line, ni,
		 ether_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni)-1);
#endif
	if (nt != NULL) {
		IEEE80211_NODE_LOCK(nt);
		if (ieee80211_node_dectestref(ni)) {
			/*
			 * Last reference, reclaim state.
			 */
			_ieee80211_free_node(ni);
		} else if (ieee80211_node_refcnt(ni) == 1 &&
		    nt->nt_keyixmap != NULL) {
			ieee80211_keyix keyix;
			/*
			 * Check for a last reference in the key mapping table.
			 */
			keyix = ni->ni_ucastkey.wk_rxkeyix;
			if (keyix < nt->nt_keyixmax &&
			    nt->nt_keyixmap[keyix] == ni) {
				IEEE80211_DPRINTF(ni->ni_ic, IEEE80211_MSG_NODE,
				    "%s: %p<%s> clear key map entry", __func__,
				    ni, ether_sprintf(ni->ni_macaddr));
				nt->nt_keyixmap[keyix] = NULL;
				ieee80211_node_decref(ni); /* XXX needed? */
				_ieee80211_free_node(ni);
			}
		}
		IEEE80211_NODE_UNLOCK(nt);
	} else {
		if (ieee80211_node_dectestref(ni))
			_ieee80211_free_node(ni);
	}
}

/*
 * Reclaim a unicast key and clear any key cache state.
 */
int
ieee80211_node_delucastkey(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211_node *nikey;
	ieee80211_keyix keyix;
	int isowned, status;

	/*
	 * NB: We must beware of LOR here; deleting the key
	 * can cause the crypto layer to block traffic updates
	 * which can generate a LOR against the node table lock;
	 * grab it here and stash the key index for our use below.
	 *
	 * Must also beware of recursion on the node table lock.
	 * When called from node_cleanup we may already have
	 * the node table lock held.  Unfortunately there's no
	 * way to separate out this path so we must do this
	 * conditionally.
	 */
	isowned = IEEE80211_NODE_IS_LOCKED(nt);
	if (!isowned)
		IEEE80211_NODE_LOCK(nt);
	keyix = ni->ni_ucastkey.wk_rxkeyix;
	status = ieee80211_crypto_delkey(ic, &ni->ni_ucastkey);
	if (nt->nt_keyixmap != NULL && keyix < nt->nt_keyixmax) {
		nikey = nt->nt_keyixmap[keyix];
		nt->nt_keyixmap[keyix] = NULL;;
	} else
		nikey = NULL;
	if (!isowned)
		IEEE80211_NODE_UNLOCK(&ic->ic_sta);

	if (nikey != NULL) {
		KASSERT(nikey == ni,
			("key map out of sync, ni %p nikey %p", ni, nikey));
		IEEE80211_DPRINTF(ni->ni_ic, IEEE80211_MSG_NODE,
			"%s: delete key map entry %p<%s> refcnt %d\n",
			__func__, ni, ether_sprintf(ni->ni_macaddr),
			ieee80211_node_refcnt(ni)-1);
		ieee80211_free_node(ni);
	}
	return status;
}

/*
 * Reclaim a node.  If this is the last reference count then
 * do the normal free work.  Otherwise remove it from the node
 * table and mark it gone by clearing the back-reference.
 */
static void
node_reclaim(struct ieee80211_node_table *nt, struct ieee80211_node *ni)
{
	ieee80211_keyix keyix;

	IEEE80211_NODE_LOCK_ASSERT(nt);

	IEEE80211_DPRINTF(ni->ni_ic, IEEE80211_MSG_NODE,
		"%s: remove %p<%s> from %s table, refcnt %d\n",
		__func__, ni, ether_sprintf(ni->ni_macaddr),
		nt->nt_name, ieee80211_node_refcnt(ni)-1);
	/*
	 * Clear any entry in the unicast key mapping table.
	 * We need to do it here so rx lookups don't find it
	 * in the mapping table even if it's not in the hash
	 * table.  We cannot depend on the mapping table entry
	 * being cleared because the node may not be free'd.
	 */
	keyix = ni->ni_ucastkey.wk_rxkeyix;
	if (nt->nt_keyixmap != NULL && keyix < nt->nt_keyixmax &&
	    nt->nt_keyixmap[keyix] == ni) {
		IEEE80211_DPRINTF(ni->ni_ic, IEEE80211_MSG_NODE,
			"%s: %p<%s> clear key map entry\n",
			__func__, ni, ether_sprintf(ni->ni_macaddr));
		nt->nt_keyixmap[keyix] = NULL;
		ieee80211_node_decref(ni);	/* NB: don't need free */
	}
	if (!ieee80211_node_dectestref(ni)) {
		/*
		 * Other references are present, just remove the
		 * node from the table so it cannot be found.  When
		 * the references are dropped storage will be
		 * reclaimed.
		 */
		TAILQ_REMOVE(&nt->nt_node, ni, ni_list);
		LIST_REMOVE(ni, ni_hash);
		ni->ni_table = NULL;		/* clear reference */
	} else
		_ieee80211_free_node(ni);
}

static void
ieee80211_free_allnodes_locked(struct ieee80211_node_table *nt)
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,
		"%s: free all nodes in %s table\n", __func__, nt->nt_name);

	while ((ni = TAILQ_FIRST(&nt->nt_node)) != NULL) {
		if (ni->ni_associd != 0) {
			if (ic->ic_auth->ia_node_leave != NULL)
				ic->ic_auth->ia_node_leave(ic, ni);
			IEEE80211_AID_CLR(ni->ni_associd, ic->ic_aid_bitmap);
		}
		node_reclaim(nt, ni);
	}
	ieee80211_reset_erp(ic);
}

static void
ieee80211_free_allnodes(struct ieee80211_node_table *nt)
{

	IEEE80211_NODE_LOCK(nt);
	ieee80211_free_allnodes_locked(nt);
	IEEE80211_NODE_UNLOCK(nt);
}

/*
 * Timeout entries in the scan cache.
 */
static void
ieee80211_timeout_scan_candidates(struct ieee80211_node_table *nt)
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni, *tni;

	IEEE80211_NODE_LOCK(nt);
	ni = ic->ic_bss;
	/* XXX belongs elsewhere */
	if (ni->ni_rxfrag[0] != NULL && ticks > ni->ni_rxfragstamp + hz) {
		m_freem(ni->ni_rxfrag[0]);
		ni->ni_rxfrag[0] = NULL;
	}
	TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, tni) {
		if (ni->ni_inact && --ni->ni_inact == 0) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,
			    "[%s] scan candidate purged from cache "
			    "(refcnt %u)\n", ether_sprintf(ni->ni_macaddr),
			    ieee80211_node_refcnt(ni));
			node_reclaim(nt, ni);
		}
	}
	IEEE80211_NODE_UNLOCK(nt);

	nt->nt_inact_timer = IEEE80211_INACT_WAIT;
}

/*
 * Timeout inactive stations and do related housekeeping.
 * Note that we cannot hold the node lock while sending a
 * frame as this would lead to a LOR.  Instead we use a
 * generation number to mark nodes that we've scanned and
 * drop the lock and restart a scan if we have to time out
 * a node.  Since we are single-threaded by virtue of
 * controlling the inactivity timer we can be sure this will
 * process each node only once.
 */
static void
ieee80211_timeout_stations(struct ieee80211_node_table *nt)
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni;
	u_int gen;
	int isadhoc;

	isadhoc = (ic->ic_opmode == IEEE80211_M_IBSS ||
		   ic->ic_opmode == IEEE80211_M_AHDEMO);
	IEEE80211_SCAN_LOCK(nt);
	gen = ++nt->nt_scangen;
	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,
		"%s: %s scangen %u\n", __func__, nt->nt_name, gen);
restart:
	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (ni->ni_scangen == gen)	/* previously handled */
			continue;
		ni->ni_scangen = gen;
		/*
		 * Ignore entries for which have yet to receive an
		 * authentication frame.  These are transient and
		 * will be reclaimed when the last reference to them
		 * goes away (when frame xmits complete).
		 */
		if (ic->ic_opmode == IEEE80211_M_HOSTAP &&
		    (ni->ni_flags & IEEE80211_NODE_AREF) == 0)
			continue;
		/*
		 * Free fragment if not needed anymore
		 * (last fragment older than 1s).
		 * XXX doesn't belong here
		 */
		if (ni->ni_rxfrag[0] != NULL &&
		    ticks > ni->ni_rxfragstamp + hz) {
			m_freem(ni->ni_rxfrag[0]);
			ni->ni_rxfrag[0] = NULL;
		}
		/*
		 * Special case ourself; we may be idle for extended periods
		 * of time and regardless reclaiming our state is wrong.
		 */
		if (ni == ic->ic_bss)
			continue;
		ni->ni_inact--;
		if (ni->ni_associd != 0 || isadhoc) {
			/*
			 * Age frames on the power save queue. The
			 * aging interval is 4 times the listen
			 * interval specified by the station.  This
			 * number is factored into the age calculations
			 * when the frame is placed on the queue.  We
			 * store ages as time differences we can check
			 * and/or adjust only the head of the list.
			 */
			if (IEEE80211_NODE_SAVEQ_QLEN(ni) != 0) {
				struct mbuf *m;
				int discard = 0;

				IEEE80211_NODE_SAVEQ_LOCK(ni);
				while (IF_POLL(&ni->ni_savedq, m) != NULL &&
				     M_AGE_GET(m) < IEEE80211_INACT_WAIT) {
IEEE80211_DPRINTF(ic, IEEE80211_MSG_POWER, "[%s] discard frame, age %u\n", ether_sprintf(ni->ni_macaddr), M_AGE_GET(m));/*XXX*/
					_IEEE80211_NODE_SAVEQ_DEQUEUE_HEAD(ni, m);
					m_freem(m);
					discard++;
				}
				if (m != NULL)
					M_AGE_SUB(m, IEEE80211_INACT_WAIT);
				IEEE80211_NODE_SAVEQ_UNLOCK(ni);

				if (discard != 0) {
					IEEE80211_DPRINTF(ic,
					    IEEE80211_MSG_POWER,
					    "[%s] discard %u frames for age\n",
					    ether_sprintf(ni->ni_macaddr),
					    discard);
					IEEE80211_NODE_STAT_ADD(ni,
						ps_discard, discard);
					if (IEEE80211_NODE_SAVEQ_QLEN(ni) == 0)
						ic->ic_set_tim(ni, 0);
				}
			}
			/*
			 * Probe the station before time it out.  We
			 * send a null data frame which may not be
			 * universally supported by drivers (need it
			 * for ps-poll support so it should be...).
			 */
			if (0 < ni->ni_inact &&
			    ni->ni_inact <= ic->ic_inact_probe) {
				IEEE80211_NOTE(ic,
				    IEEE80211_MSG_INACT | IEEE80211_MSG_NODE,
				    ni, "%s",
				    "probe station due to inactivity");
				/*
				 * Grab a reference before unlocking the table
				 * so the node cannot be reclaimed before we
				 * send the frame. ieee80211_send_nulldata
				 * understands we've done this and reclaims the
				 * ref for us as needed.
				 */
				ieee80211_ref_node(ni);
				IEEE80211_NODE_UNLOCK(nt);
				ieee80211_send_nulldata(ni);
				/* XXX stat? */
				goto restart;
			}
		}
		if (ni->ni_inact <= 0) {
			IEEE80211_NOTE(ic,
			    IEEE80211_MSG_INACT | IEEE80211_MSG_NODE, ni,
			    "station timed out due to inactivity "
			    "(refcnt %u)", ieee80211_node_refcnt(ni));
			/*
			 * Send a deauthenticate frame and drop the station.
			 * This is somewhat complicated due to reference counts
			 * and locking.  At this point a station will typically
			 * have a reference count of 1.  ieee80211_node_leave
			 * will do a "free" of the node which will drop the
			 * reference count.  But in the meantime a reference
			 * wil be held by the deauth frame.  The actual reclaim
			 * of the node will happen either after the tx is
			 * completed or by ieee80211_node_leave.
			 *
			 * Separately we must drop the node lock before sending
			 * in case the driver takes a lock, as this will result
			 * in  LOR between the node lock and the driver lock.
			 */
			IEEE80211_NODE_UNLOCK(nt);
			if (ni->ni_associd != 0) {
				IEEE80211_SEND_MGMT(ic, ni,
				    IEEE80211_FC0_SUBTYPE_DEAUTH,
				    IEEE80211_REASON_AUTH_EXPIRE);
			}
			ieee80211_node_leave(ic, ni);
			ic->ic_stats.is_node_timeout++;
			goto restart;
		}
	}
	IEEE80211_NODE_UNLOCK(nt);

	IEEE80211_SCAN_UNLOCK(nt);

	nt->nt_inact_timer = IEEE80211_INACT_WAIT;
}

void
ieee80211_iterate_nodes(struct ieee80211_node_table *nt, ieee80211_iter_func *f, void *arg)
{
	struct ieee80211_node *ni;
	u_int gen;

	IEEE80211_SCAN_LOCK(nt);
	gen = ++nt->nt_scangen;
restart:
	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		if (ni->ni_scangen != gen) {
			ni->ni_scangen = gen;
			(void) ieee80211_ref_node(ni);
			IEEE80211_NODE_UNLOCK(nt);
			(*f)(arg, ni);
			ieee80211_free_node(ni);
			goto restart;
		}
	}
	IEEE80211_NODE_UNLOCK(nt);

	IEEE80211_SCAN_UNLOCK(nt);
}

void
ieee80211_dump_node(struct ieee80211_node_table *nt, struct ieee80211_node *ni)
{
	printf("0x%p: mac %s refcnt %d\n", ni,
		ether_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni));
	printf("\tscangen %u authmode %u flags 0x%x\n",
		ni->ni_scangen, ni->ni_authmode, ni->ni_flags);
	printf("\tassocid 0x%x txpower %u vlan %u\n",
		ni->ni_associd, ni->ni_txpower, ni->ni_vlan);
	printf("\ttxseq %u rxseq %u fragno %u rxfragstamp %u\n",
		ni->ni_txseqs[IEEE80211_NONQOS_TID],
		ni->ni_rxseqs[IEEE80211_NONQOS_TID] >> IEEE80211_SEQ_SEQ_SHIFT,
		ni->ni_rxseqs[IEEE80211_NONQOS_TID] & IEEE80211_SEQ_FRAG_MASK,
		ni->ni_rxfragstamp);
	printf("\trstamp %u rssi %u intval %u capinfo 0x%x\n",
		ni->ni_rstamp, ni->ni_rssi, ni->ni_intval, ni->ni_capinfo);
	printf("\tbssid %s essid \"%.*s\" channel %u:0x%x\n",
		ether_sprintf(ni->ni_bssid),
		ni->ni_esslen, ni->ni_essid,
		ni->ni_chan->ic_freq, ni->ni_chan->ic_flags);
	printf("\tfails %u inact %u txrate %u\n",
		ni->ni_fails, ni->ni_inact, ni->ni_txrate);
}

void
ieee80211_dump_nodes(struct ieee80211_node_table *nt)
{
	ieee80211_iterate_nodes(nt,
		(ieee80211_iter_func *) ieee80211_dump_node, nt);
}

/*
 * Handle a station joining an 11g network.
 */
static void
ieee80211_node_join_11g(struct ieee80211com *ic, struct ieee80211_node *ni)
{

	/*
	 * Station isn't capable of short slot time.  Bump
	 * the count of long slot time stations and disable
	 * use of short slot time.  Note that the actual switch
	 * over to long slot time use may not occur until the
	 * next beacon transmission (per sec. 7.3.1.4 of 11g).
	 */
	if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME) == 0) {
		ic->ic_longslotsta++;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		    "[%s] station needs long slot time, count %d\n",
		    ether_sprintf(ni->ni_macaddr), ic->ic_longslotsta);
		/* XXX vap's w/ conflicting needs won't work */
		ieee80211_set_shortslottime(ic, 0);
	}
	/*
	 * If the new station is not an ERP station
	 * then bump the counter and enable protection
	 * if configured.
	 */
	if (!ieee80211_iserp_rateset(ic, &ni->ni_rates)) {
		ic->ic_nonerpsta++;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		    "[%s] station is !ERP, %d non-ERP stations associated\n",
		    ether_sprintf(ni->ni_macaddr), ic->ic_nonerpsta);
		/*
		 * If protection is configured, enable it.
		 */
		if (ic->ic_protmode != IEEE80211_PROT_NONE) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
			    "%s: enable use of protection\n", __func__);
			ic->ic_flags |= IEEE80211_F_USEPROT;
		}
		/*
		 * If station does not support short preamble
		 * then we must enable use of Barker preamble.
		 */
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE) == 0) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
			    "[%s] station needs long preamble\n",
			    ether_sprintf(ni->ni_macaddr));
			ic->ic_flags |= IEEE80211_F_USEBARKER;
			ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;
		}
		if (ic->ic_nonerpsta == 1)
			ic->ic_flags_ext |= IEEE80211_FEXT_ERPUPDATE;
	} else
		ni->ni_flags |= IEEE80211_NODE_ERP;
}

void
ieee80211_node_join(struct ieee80211com *ic, struct ieee80211_node *ni, int resp)
{
	int newassoc;

	if (ni->ni_associd == 0) {
		u_int16_t aid;

		/*
		 * It would be good to search the bitmap
		 * more efficiently, but this will do for now.
		 */
		for (aid = 1; aid < ic->ic_max_aid; aid++) {
			if (!IEEE80211_AID_ISSET(aid,
			    ic->ic_aid_bitmap))
				break;
		}
		if (aid >= ic->ic_max_aid) {
			IEEE80211_SEND_MGMT(ic, ni, resp,
			    IEEE80211_REASON_ASSOC_TOOMANY);
			ieee80211_node_leave(ic, ni);
			return;
		}
		ni->ni_associd = aid | 0xc000;
		IEEE80211_AID_SET(ni->ni_associd, ic->ic_aid_bitmap);
		ic->ic_sta_assoc++;
		newassoc = 1;
		if (ic->ic_curmode == IEEE80211_MODE_11G &&
		    IEEE80211_IS_CHAN_FULL(ni->ni_chan))
			ieee80211_node_join_11g(ic, ni);
	} else
		newassoc = 0;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC | IEEE80211_MSG_DEBUG,
	    "[%s] station %sassociated at aid %d: %s preamble, %s slot time%s%s\n",
	    ether_sprintf(ni->ni_macaddr), newassoc ? "" : "re",
	    IEEE80211_NODE_AID(ni),
	    ic->ic_flags & IEEE80211_F_SHPREAMBLE ? "short" : "long",
	    ic->ic_flags & IEEE80211_F_SHSLOT ? "short" : "long",
	    ic->ic_flags & IEEE80211_F_USEPROT ? ", protection" : "",
	    ni->ni_flags & IEEE80211_NODE_QOS ? ", QoS" : ""
	);

	/* give driver a chance to setup state like ni_txrate */
	if (ic->ic_newassoc != NULL)
		ic->ic_newassoc(ni, newassoc);
	ni->ni_inact_reload = ic->ic_inact_auth;
	ni->ni_inact = ni->ni_inact_reload;
	IEEE80211_SEND_MGMT(ic, ni, resp, IEEE80211_STATUS_SUCCESS);
	/* tell the authenticator about new station */
	if (ic->ic_auth->ia_node_join != NULL)
		ic->ic_auth->ia_node_join(ic, ni);
	ieee80211_notify_node_join(ic, ni, newassoc);
}

/*
 * Handle a station leaving an 11g network.
 */
static void
ieee80211_node_leave_11g(struct ieee80211com *ic, struct ieee80211_node *ni)
{

	KASSERT(ic->ic_curmode == IEEE80211_MODE_11G,
	     ("not in 11g, bss %u:0x%x, curmode %u", ni->ni_chan->ic_freq,
	      ni->ni_chan->ic_flags, ic->ic_curmode));

	/*
	 * If a long slot station do the slot time bookkeeping.
	 */
	if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME) == 0) {
		KASSERT(ic->ic_longslotsta > 0,
		    ("bogus long slot station count %d", ic->ic_longslotsta));
		ic->ic_longslotsta--;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		    "[%s] long slot time station leaves, count now %d\n",
		    ether_sprintf(ni->ni_macaddr), ic->ic_longslotsta);
		if (ic->ic_longslotsta == 0) {
			/*
			 * Re-enable use of short slot time if supported
			 * and not operating in IBSS mode (per spec).
			 */
			if ((ic->ic_caps & IEEE80211_C_SHSLOT) &&
			    ic->ic_opmode != IEEE80211_M_IBSS) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
				    "%s: re-enable use of short slot time\n",
				    __func__);
				ieee80211_set_shortslottime(ic, 1);
			}
		}
	}
	/*
	 * If a non-ERP station do the protection-related bookkeeping.
	 */
	if ((ni->ni_flags & IEEE80211_NODE_ERP) == 0) {
		KASSERT(ic->ic_nonerpsta > 0,
		    ("bogus non-ERP station count %d", ic->ic_nonerpsta));
		ic->ic_nonerpsta--;
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		    "[%s] non-ERP station leaves, count now %d\n",
		    ether_sprintf(ni->ni_macaddr), ic->ic_nonerpsta);
		if (ic->ic_nonerpsta == 0) {
			IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
				"%s: disable use of protection\n", __func__);
			ic->ic_flags &= ~IEEE80211_F_USEPROT;
			/* XXX verify mode? */
			if (ic->ic_caps & IEEE80211_C_SHPREAMBLE) {
				IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
				    "%s: re-enable use of short preamble\n",
				    __func__);
				ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
				ic->ic_flags &= ~IEEE80211_F_USEBARKER;
			}
			ic->ic_flags_ext |= IEEE80211_FEXT_ERPUPDATE;
		}
	}
}

/*
 * Handle bookkeeping for station deauthentication/disassociation
 * when operating as an ap.
 */
void
ieee80211_node_leave(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ieee80211_node_table *nt = ni->ni_table;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC | IEEE80211_MSG_DEBUG,
	    "[%s] station with aid %d leaves\n",
	    ether_sprintf(ni->ni_macaddr), IEEE80211_NODE_AID(ni));

	KASSERT(ic->ic_opmode == IEEE80211_M_HOSTAP ||
		ic->ic_opmode == IEEE80211_M_IBSS ||
		ic->ic_opmode == IEEE80211_M_AHDEMO,
		("unexpected operating mode %u", ic->ic_opmode));
	/*
	 * If node wasn't previously associated all
	 * we need to do is reclaim the reference.
	 */
	/* XXX ibss mode bypasses 11g and notification */
	if (ni->ni_associd == 0)
		goto done;
	/*
	 * Tell the authenticator the station is leaving.
	 * Note that we must do this before yanking the
	 * association id as the authenticator uses the
	 * associd to locate it's state block.
	 */
	if (ic->ic_auth->ia_node_leave != NULL)
		ic->ic_auth->ia_node_leave(ic, ni);
	IEEE80211_AID_CLR(ni->ni_associd, ic->ic_aid_bitmap);
	ni->ni_associd = 0;
	ic->ic_sta_assoc--;

	if (ic->ic_curmode == IEEE80211_MODE_11G &&
	    IEEE80211_IS_CHAN_FULL(ni->ni_chan))
		ieee80211_node_leave_11g(ic, ni);
	/*
	 * Cleanup station state.  In particular clear various
	 * state that might otherwise be reused if the node
	 * is reused before the reference count goes to zero
	 * (and memory is reclaimed).
	 */
	ieee80211_sta_leave(ic, ni);
done:
	/*
	 * Remove the node from any table it's recorded in and
	 * drop the caller's reference.  Removal from the table
	 * is important to insure the node is not reprocessed
	 * for inactivity.
	 */
	if (nt != NULL) {
		IEEE80211_NODE_LOCK(nt);
		node_reclaim(nt, ni);
		IEEE80211_NODE_UNLOCK(nt);
	} else
		ieee80211_free_node(ni);
}

u_int8_t
ieee80211_getrssi(struct ieee80211com *ic)
{
#define	NZ(x)	((x) == 0 ? 1 : (x))
	struct ieee80211_node_table *nt = &ic->ic_sta;
	u_int32_t rssi_samples, rssi_total;
	struct ieee80211_node *ni;

	rssi_total = 0;
	rssi_samples = 0;
	switch (ic->ic_opmode) {
	case IEEE80211_M_IBSS:		/* average of all ibss neighbors */
		/* XXX locking */
		TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
			if (ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) {
				rssi_samples++;
				rssi_total += ic->ic_node_getrssi(ni);
			}
		break;
	case IEEE80211_M_AHDEMO:	/* average of all neighbors */
		/* XXX locking */
		TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
			rssi_samples++;
			rssi_total += ic->ic_node_getrssi(ni);
		}
		break;
	case IEEE80211_M_HOSTAP:	/* average of all associated stations */
		/* XXX locking */
		TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
			if (IEEE80211_AID(ni->ni_associd) != 0) {
				rssi_samples++;
				rssi_total += ic->ic_node_getrssi(ni);
			}
		break;
	case IEEE80211_M_MONITOR:	/* XXX */
	case IEEE80211_M_STA:		/* use stats from associated ap */
	default:
		if (ic->ic_bss != NULL)
			rssi_total = ic->ic_node_getrssi(ic->ic_bss);
		rssi_samples = 1;
		break;
	}
	return rssi_total / NZ(rssi_samples);
#undef NZ
}

/*
 * Indicate whether there are frames queued for a station in power-save mode.
 */
static void
ieee80211_set_tim(struct ieee80211_node *ni, int set)
{
	struct ieee80211com *ic = ni->ni_ic;
	u_int16_t aid;

	KASSERT(ic->ic_opmode == IEEE80211_M_HOSTAP ||
		ic->ic_opmode == IEEE80211_M_IBSS,
		("operating mode %u", ic->ic_opmode));

	aid = IEEE80211_AID(ni->ni_associd);
	KASSERT(aid < ic->ic_max_aid,
		("bogus aid %u, max %u", aid, ic->ic_max_aid));

	IEEE80211_BEACON_LOCK(ic);
	if (set != (isset(ic->ic_tim_bitmap, aid) != 0)) {
		if (set) {
			setbit(ic->ic_tim_bitmap, aid);
			ic->ic_ps_pending++;
		} else {
			clrbit(ic->ic_tim_bitmap, aid);
			ic->ic_ps_pending--;
		}
		ic->ic_flags |= IEEE80211_F_TIMUPDATE;
	}
	IEEE80211_BEACON_UNLOCK(ic);
}

/*
 * Node table support.
 */

static void
ieee80211_node_table_init(struct ieee80211com *ic,
	struct ieee80211_node_table *nt,
	const char *name, int inact, int keyixmax,
	void (*timeout)(struct ieee80211_node_table *))
{

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_NODE,
		"%s %s table, inact %u\n", __func__, name, inact);

	nt->nt_ic = ic;
	/* XXX need unit */
	IEEE80211_NODE_LOCK_INIT(nt, ic->ic_ifp->if_xname);
	IEEE80211_SCAN_LOCK_INIT(nt, ic->ic_ifp->if_xname);
	TAILQ_INIT(&nt->nt_node);
	nt->nt_name = name;
	nt->nt_scangen = 1;
	nt->nt_inact_init = inact;
	nt->nt_timeout = timeout;
	nt->nt_keyixmax = keyixmax;
	if (nt->nt_keyixmax > 0) {
		MALLOC(nt->nt_keyixmap, struct ieee80211_node **,
			keyixmax * sizeof(struct ieee80211_node *),
			M_80211_NODE, M_NOWAIT | M_ZERO);
		if (nt->nt_keyixmap == NULL)
			if_printf(ic->ic_ifp,
			    "Cannot allocate key index map with %u entries\n",
			    keyixmax);
	} else
		nt->nt_keyixmap = NULL;
}

void
ieee80211_node_table_reset(struct ieee80211_node_table *nt)
{

	IEEE80211_DPRINTF(nt->nt_ic, IEEE80211_MSG_NODE,
		"%s %s table\n", __func__, nt->nt_name);

	IEEE80211_NODE_LOCK(nt);
	nt->nt_inact_timer = 0;
	ieee80211_free_allnodes_locked(nt);
	IEEE80211_NODE_UNLOCK(nt);
}

static void
ieee80211_node_table_cleanup(struct ieee80211_node_table *nt)
{

	IEEE80211_DPRINTF(nt->nt_ic, IEEE80211_MSG_NODE,
		"%s %s table\n", __func__, nt->nt_name);

	IEEE80211_NODE_LOCK(nt);
	ieee80211_free_allnodes_locked(nt);
	if (nt->nt_keyixmap != NULL) {
		/* XXX verify all entries are NULL */
		int i;
		for (i = 0; i < nt->nt_keyixmax; i++)
			if (nt->nt_keyixmap[i] != NULL)
				printf("%s: %s[%u] still active\n", __func__,
					nt->nt_name, i);
		FREE(nt->nt_keyixmap, M_80211_NODE);
		nt->nt_keyixmap = NULL;
	}
	IEEE80211_SCAN_LOCK_DESTROY(nt);
	IEEE80211_NODE_LOCK_DESTROY(nt);
}
