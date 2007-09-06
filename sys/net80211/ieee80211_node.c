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

static int ieee80211_sta_join1(struct ieee80211_node *);

static struct ieee80211_node *node_alloc(struct ieee80211_node_table *);
static void node_cleanup(struct ieee80211_node *);
static void node_free(struct ieee80211_node *);
static int8_t node_getrssi(const struct ieee80211_node *);
static void node_getsignal(const struct ieee80211_node *, int8_t *, int8_t *);

static void ieee80211_setup_node(struct ieee80211_node_table *,
		struct ieee80211_node *, const uint8_t *);
static void _ieee80211_free_node(struct ieee80211_node *);

static void ieee80211_node_table_init(struct ieee80211com *ic,
	struct ieee80211_node_table *nt, const char *name,
	int inact, int keymaxix);
static void ieee80211_node_table_reset(struct ieee80211_node_table *);
static void ieee80211_node_table_cleanup(struct ieee80211_node_table *nt);

MALLOC_DEFINE(M_80211_NODE, "80211node", "802.11 node state");

void
ieee80211_node_attach(struct ieee80211com *ic)
{

	ic->ic_node_alloc = node_alloc;
	ic->ic_node_free = node_free;
	ic->ic_node_cleanup = node_cleanup;
	ic->ic_node_getrssi = node_getrssi;
	ic->ic_node_getsignal = node_getsignal;

	/* default station inactivity timer setings */
	ic->ic_inact_init = IEEE80211_INACT_INIT;
	ic->ic_inact_auth = IEEE80211_INACT_AUTH;
	ic->ic_inact_run = IEEE80211_INACT_RUN;
	ic->ic_inact_probe = IEEE80211_INACT_PROBE;

	callout_init(&ic->ic_inact, CALLOUT_MPSAFE);

	/* NB: driver should override */
	ic->ic_max_aid = IEEE80211_AID_DEF;

	ic->ic_flags_ext |= IEEE80211_FEXT_INACT; /* inactivity processing */
}

void
ieee80211_node_lateattach(struct ieee80211com *ic)
{
	struct ieee80211_rsnparms *rsn;

	if (ic->ic_max_aid > IEEE80211_AID_MAX)
		ic->ic_max_aid = IEEE80211_AID_MAX;
	MALLOC(ic->ic_aid_bitmap, uint32_t *,
		howmany(ic->ic_max_aid, 32) * sizeof(uint32_t),
		M_80211_NODE, M_NOWAIT | M_ZERO);
	if (ic->ic_aid_bitmap == NULL) {
		/* XXX no way to recover */
		printf("%s: no memory for AID bitmap!\n", __func__);
		ic->ic_max_aid = 0;
	}

	ieee80211_node_table_init(ic, &ic->ic_sta, "station",
		IEEE80211_INACT_INIT, ic->ic_crypto.cs_max_keyix);

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
	ieee80211_node_table_cleanup(&ic->ic_sta);
	if (ic->ic_aid_bitmap != NULL) {
		FREE(ic->ic_aid_bitmap, M_80211_NODE);
		ic->ic_aid_bitmap = NULL;
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
	ni->ni_inact = ni->ni_inact_reload;
}

void
ieee80211_node_unauthorize(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;

	ni->ni_flags &= ~IEEE80211_NODE_AUTH;
	ni->ni_inact_reload = ic->ic_inact_auth;
	if (ni->ni_inact > ni->ni_inact_reload)
		ni->ni_inact = ni->ni_inact_reload;
}

/*
 * Set/change the channel.  The rate set is also updated as
 * to insure a consistent view by drivers.
 */
static void
ieee80211_node_set_chan(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct ieee80211_channel *chan = ic->ic_bsschan;

#if 0
	KASSERT(chan != IEEE80211_CHAN_ANYC, ("bss channel not setup"));
#else
	if (chan == IEEE80211_CHAN_ANYC)	/* XXX while scanning */
		chan = ic->ic_curchan;
#endif
	ni->ni_chan = chan;
	if (IEEE80211_IS_CHAN_HT(chan)) {
		/*
		 * XXX Gotta be careful here; the rate set returned by
		 * ieee80211_get_suprates is actually any HT rate
		 * set so blindly copying it will be bad.  We must
		 * install the legacy rate est in ni_rates and the
		 * HT rate set in ni_htrates.
		 */
		ni->ni_htrates = *ieee80211_get_suphtrates(ic, chan);
	}
	ni->ni_rates = *ieee80211_get_suprates(ic, chan);
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
			ic->ic_des_ssid[0].ssid, ic->ic_des_ssid[0].len,
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
	ni->ni_esslen = ic->ic_des_ssid[0].len;
	memcpy(ni->ni_essid, ic->ic_des_ssid[0].ssid, ni->ni_esslen);
	if (ic->ic_bss != NULL)
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
	ic->ic_bsschan = chan;
	ieee80211_node_set_chan(ic, ni);
	ic->ic_curmode = ieee80211_chan2mode(chan);
	/*
	 * Do mode-specific rate setup.
	 */
	if (IEEE80211_IS_CHAN_FULL(chan)) {
		if (IEEE80211_IS_CHAN_ANYG(chan)) {
			/*
			 * Use a mixed 11b/11g rate set.
			 */
			ieee80211_set11gbasicrates(&ni->ni_rates,
				IEEE80211_MODE_11G);
		} else if (IEEE80211_IS_CHAN_B(chan)) {
			/*
			 * Force pure 11b rate set.
			 */
			ieee80211_set11gbasicrates(&ni->ni_rates,
				IEEE80211_MODE_11B);
		}
	}

	(void) ieee80211_sta_join1(ieee80211_ref_node(ni));
}

/*
 * Reset bss state on transition to the INIT state.
 * Clear any stations from the table (they have been
 * deauth'd) and reset the bss node (clears key, rate
 * etc. state).
 */
void
ieee80211_reset_bss(struct ieee80211com *ic)
{
	struct ieee80211_node *ni, *obss;

	callout_drain(&ic->ic_inact);
	ieee80211_node_table_reset(&ic->ic_sta);
	ieee80211_reset_erp(ic);

	ni = ieee80211_alloc_node(&ic->ic_sta, ic->ic_myaddr);
	KASSERT(ni != NULL, ("unable to setup inital BSS node"));
	obss = ic->ic_bss;
	ic->ic_bss = ieee80211_ref_node(ni);
	if (obss != NULL) {
		copy_bss(ni, obss);
		ni->ni_intval = ic->ic_bintval;
		ieee80211_free_node(obss);
	}
}

static int
match_ssid(const struct ieee80211_node *ni,
	int nssid, const struct ieee80211_scan_ssid ssids[])
{
	int i;

	for (i = 0; i < nssid; i++) {
		if (ni->ni_esslen == ssids[i].len &&
		     memcmp(ni->ni_essid, ssids[i].ssid, ni->ni_esslen) == 0)
			return 1;
	}
	return 0;
}

/*
 * Test a node for suitability/compatibility.
 */
static int
check_bss(struct ieee80211com *ic, struct ieee80211_node *ni)
{
        uint8_t rate;

	if (isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic, ni->ni_chan)))
		return 0;
	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) == 0)
			return 0;
	} else {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_ESS) == 0)
			return 0;
	}
	if (ic->ic_flags & IEEE80211_F_PRIVACY) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) == 0)
			return 0;
	} else {
		/* XXX does this mean privacy is supported or required? */
		if (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)
			return 0;
	}
	rate = ieee80211_fix_rate(ni, &ni->ni_rates,
	    IEEE80211_F_JOIN | IEEE80211_F_DONEGO | IEEE80211_F_DOFRATE);
	if (rate & IEEE80211_RATE_BASIC)
		return 0;
	if (ic->ic_des_nssid != 0 &&
	    !match_ssid(ni, ic->ic_des_nssid, ic->ic_des_ssid))
		return 0;
	if ((ic->ic_flags & IEEE80211_F_DESBSSID) &&
	    !IEEE80211_ADDR_EQ(ic->ic_des_bssid, ni->ni_bssid))
		return 0;
	return 1;
}

#ifdef IEEE80211_DEBUG
/*
 * Display node suitability/compatibility.
 */
static void
check_bss_debug(struct ieee80211com *ic, struct ieee80211_node *ni)
{
        uint8_t rate;
        int fail;

	fail = 0;
	if (isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic, ni->ni_chan)))
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
	if (ic->ic_des_nssid != 0 &&
	    !match_ssid(ni, ic->ic_des_nssid, ic->ic_des_ssid))
		fail |= 0x10;
	if ((ic->ic_flags & IEEE80211_F_DESBSSID) &&
	    !IEEE80211_ADDR_EQ(ic->ic_des_bssid, ni->ni_bssid))
		fail |= 0x20;

	printf(" %c %s", fail ? '-' : '+', ether_sprintf(ni->ni_macaddr));
	printf(" %s%c", ether_sprintf(ni->ni_bssid), fail & 0x20 ? '!' : ' ');
	printf(" %3d%c",
	    ieee80211_chan2ieee(ic, ni->ni_chan), fail & 0x01 ? '!' : ' ');
	printf(" %+4d", ni->ni_rssi);
	printf(" %2dM%c", (rate & IEEE80211_RATE_VAL) / 2,
	    fail & 0x08 ? '!' : ' ');
	printf(" %4s%c",
	    (ni->ni_capinfo & IEEE80211_CAPINFO_ESS) ? "ess" :
	    (ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) ? "ibss" :
	    "????",
	    fail & 0x02 ? '!' : ' ');
	printf(" %3s%c ",
	    (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) ?  "wep" : "no",
	    fail & 0x04 ? '!' : ' ');
	ieee80211_print_essid(ni->ni_essid, ni->ni_esslen);
	printf("%s\n", fail & 0x10 ? "!" : "");
}
#endif /* IEEE80211_DEBUG */
 
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
	if (!check_bss(ic, ni)) {
		/* capabilities mismatch */
		IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC,
		    "%s: merge failed, capabilities mismatch\n", __func__);
#ifdef IEEE80211_DEBUG
		if (ieee80211_msg_assoc(ic))
			check_bss_debug(ic, ni);
#endif
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
	return ieee80211_sta_join1(ieee80211_ref_node(ni));
}

/*
 * Join the specified IBSS/BSS network.  The node is assumed to
 * be passed in with a held reference.
 */
static int
ieee80211_sta_join1(struct ieee80211_node *selbs)
{
	struct ieee80211com *ic = selbs->ni_ic;
	struct ieee80211_node *obss;
	int canreassoc;

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
	/*
	 * Check if old+new node have the same address in which
	 * case we can reassociate when operating in sta mode.
	 */
	canreassoc = (obss != NULL &&
		ic->ic_state == IEEE80211_S_RUN &&
		IEEE80211_ADDR_EQ(obss->ni_macaddr, selbs->ni_macaddr));
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

	ic->ic_bsschan = selbs->ni_chan;
	ic->ic_curchan = ic->ic_bsschan;
	ic->ic_curmode = ieee80211_chan2mode(ic->ic_curchan);
	ic->ic_set_channel(ic);
	/*
	 * Set the erp state (mostly the slot time) to deal with
	 * the auto-select case; this should be redundant if the
	 * mode is locked.
	 */ 
	ieee80211_reset_erp(ic);
	ieee80211_wme_initparams(ic);

	if (ic->ic_opmode == IEEE80211_M_STA) {
		if (canreassoc) {
			/* Reassociate */
			ieee80211_new_state(ic, IEEE80211_S_ASSOC, 1);
		} else {
			/*
			 * Act as if we received a DEAUTH frame in case we
			 * are invoked from the RUN state.  This will cause
			 * us to try to re-authenticate if we are operating
			 * as a station.
			 */
			ieee80211_new_state(ic, IEEE80211_S_AUTH,
				IEEE80211_FC0_SUBTYPE_DEAUTH);
		}
	} else
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	return 1;
}

int
ieee80211_sta_join(struct ieee80211com *ic,
	const struct ieee80211_scan_entry *se)
{
	struct ieee80211_node *ni;

	ni = ieee80211_alloc_node(&ic->ic_sta, se->se_macaddr);
	if (ni == NULL) {
		/* XXX msg */
		return 0;
	}
	/*
	 * Expand scan state into node's format.
	 * XXX may not need all this stuff
	 */
	IEEE80211_ADDR_COPY(ni->ni_bssid, se->se_bssid);
	ni->ni_esslen = se->se_ssid[1];
	memcpy(ni->ni_essid, se->se_ssid+2, ni->ni_esslen);
	ni->ni_rstamp = se->se_rstamp;
	ni->ni_tstamp.tsf = se->se_tstamp.tsf;
	ni->ni_intval = se->se_intval;
	ni->ni_capinfo = se->se_capinfo;
	/* XXX shift to 11n channel if htinfo present */
	ni->ni_chan = se->se_chan;
	ni->ni_timoff = se->se_timoff;
	ni->ni_fhdwell = se->se_fhdwell;
	ni->ni_fhindex = se->se_fhindex;
	ni->ni_erp = se->se_erp;
	ni->ni_rssi = se->se_rssi;
	ni->ni_noise = se->se_noise;
	if (se->se_htcap_ie != NULL)
		ieee80211_ht_node_init(ni, se->se_htcap_ie);
	if (se->se_htinfo_ie != NULL)
		ieee80211_parse_htinfo(ni, se->se_htinfo_ie);
	if (se->se_wpa_ie != NULL)
		ieee80211_saveie(&ni->ni_wpa_ie, se->se_wpa_ie);
	if (se->se_rsn_ie != NULL)
		ieee80211_saveie(&ni->ni_rsn_ie, se->se_rsn_ie);
	if (se->se_wme_ie != NULL)
		ieee80211_saveie(&ni->ni_wme_ie, se->se_wme_ie);
	if (se->se_ath_ie != NULL)
		ieee80211_saveath(ni, se->se_ath_ie);

	ic->ic_dtim_period = se->se_dtimperiod;
	ic->ic_dtim_count = 0;

	/* NB: must be after ni_chan is setup */
	ieee80211_setup_rates(ni, se->se_rates, se->se_xrates,
		IEEE80211_F_DOSORT);
	if (se->se_htcap_ie != NULL)
		ieee80211_setup_htrates(ni, se->se_htcap_ie, IEEE80211_F_JOIN);
	if (se->se_htinfo_ie != NULL)
		ieee80211_setup_basic_htrates(ni, se->se_htinfo_ie);

	return ieee80211_sta_join1(ieee80211_ref_node(ni));
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
	int i;

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
	if (ieee80211_node_saveq_drain(ni) != 0 && ic->ic_set_tim != NULL)
		ic->ic_set_tim(ni, 0);

	ni->ni_associd = 0;
	if (ni->ni_challenge != NULL) {
		FREE(ni->ni_challenge, M_80211_NODE);
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
		FREE(ni->ni_wpa_ie, M_80211_NODE);
	if (ni->ni_rsn_ie != NULL)
		FREE(ni->ni_rsn_ie, M_80211_NODE);
	if (ni->ni_wme_ie != NULL)
		FREE(ni->ni_wme_ie, M_80211_NODE);
	if (ni->ni_ath_ie != NULL)
		FREE(ni->ni_ath_ie, M_80211_NODE);
	IEEE80211_NODE_SAVEQ_DESTROY(ni);
	FREE(ni, M_80211_NODE);
}

static int8_t
node_getrssi(const struct ieee80211_node *ni)
{
	return ni->ni_rssi;
}

static void
node_getsignal(const struct ieee80211_node *ni, int8_t *rssi, int8_t *noise)
{
	*rssi = ni->ni_rssi;
	*noise = ni->ni_noise;
}

static void
ieee80211_setup_node(struct ieee80211_node_table *nt,
	struct ieee80211_node *ni, const uint8_t *macaddr)
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
	ni->ni_ath_defkeyix = 0x7fff;
	IEEE80211_NODE_SAVEQ_INIT(ni, "unknown");

	IEEE80211_NODE_LOCK(nt);
	TAILQ_INSERT_TAIL(&nt->nt_node, ni, ni_list);
	LIST_INSERT_HEAD(&nt->nt_hash[hash], ni, ni_hash);
	ni->ni_table = nt;
	ni->ni_ic = ic;
	IEEE80211_NODE_UNLOCK(nt);
}

struct ieee80211_node *
ieee80211_alloc_node(struct ieee80211_node_table *nt, const uint8_t *macaddr)
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
ieee80211_tmp_node(struct ieee80211com *ic, const uint8_t *macaddr)
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
		ieee80211_node_set_chan(ic, ni);
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
ieee80211_dup_bss(struct ieee80211_node_table *nt, const uint8_t *macaddr)
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
		ieee80211_node_set_chan(ic, ni);
		ni->ni_rsn = ic->ic_bss->ni_rsn;
	} else
		ic->ic_stats.is_rx_nodealloc++;
	return ni;
}

static struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
_ieee80211_find_node_debug(struct ieee80211_node_table *nt,
	const uint8_t *macaddr, const char *func, int line)
#else
_ieee80211_find_node(struct ieee80211_node_table *nt,
	const uint8_t *macaddr)
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
	const uint8_t *macaddr, const char *func, int line)
#else
ieee80211_find_node(struct ieee80211_node_table *nt, const uint8_t *macaddr)
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
	const uint8_t macaddr[IEEE80211_ADDR_LEN])
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
		if (ic->ic_opmode == IEEE80211_M_AHDEMO) {
			/*
			 * In adhoc demo mode there are no management
			 * frames to use to discover neighbor capabilities,
			 * so blindly propagate the local configuration 
			 * so we can do interesting things (e.g. use
			 * WME to disable ACK's).
			 */
			if (ic->ic_flags & IEEE80211_F_WME)
				ni->ni_flags |= IEEE80211_NODE_QOS;
			if (ic->ic_flags & IEEE80211_F_FF)
				ni->ni_flags |= IEEE80211_NODE_FF;
		}
		/* XXX not right for 802.1x/WPA */
		ieee80211_node_authorize(ni);
	}
	return ni;
}

void
ieee80211_init_neighbor(struct ieee80211_node *ni,
	const struct ieee80211_frame *wh,
	const struct ieee80211_scanparams *sp)
{
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
	if (sp->rsn != NULL)
		ieee80211_saveie(&ni->ni_rsn_ie, sp->rsn);
	if (sp->ath != NULL)
		ieee80211_saveath(ni, sp->ath);

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
#define	IS_BAR(wh) \
	((wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) == IEEE80211_FC0_SUBTYPE_BAR)

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

	/* XXX check ic_bss first in station mode */
	/* XXX 4-address frames? */
	nt = &ic->ic_sta;
	IEEE80211_NODE_LOCK(nt);
	if (IS_CTL(wh) && !IS_PSPOLL(wh) && !IS_BAR(wh) /*&& !IS_RTS(ah)*/)
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

	nt = &ic->ic_sta;
	IEEE80211_NODE_LOCK(nt);
	if (nt->nt_keyixmap != NULL && keyix < nt->nt_keyixmax)
		ni = nt->nt_keyixmap[keyix];
	else
		ni = NULL;
	if (ni == NULL) {
		if (IS_CTL(wh) && !IS_PSPOLL(wh) && !IS_BAR(wh) /*&& !IS_RTS(ah)*/)
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
	} else
		ieee80211_ref_node(ni);
	IEEE80211_NODE_UNLOCK(nt);

	return ni;
}
#undef IS_BAR
#undef IS_PSPOLL
#undef IS_CTL

/*
 * Return a reference to the appropriate node for sending
 * a data frame.  This handles node discovery in adhoc networks.
 */
struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_txnode_debug(struct ieee80211com *ic, const uint8_t *macaddr,
	const char *func, int line)
#else
ieee80211_find_txnode(struct ieee80211com *ic, const uint8_t *macaddr)
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
 * Like find but search based on the ssid too.
 */
struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_node_with_ssid_debug(struct ieee80211_node_table *nt,
	const uint8_t *macaddr, u_int ssidlen, const uint8_t *ssid,
	const char *func, int line)
#else
ieee80211_find_node_with_ssid(struct ieee80211_node_table *nt,
	const uint8_t *macaddr, u_int ssidlen, const uint8_t *ssid)
#endif
{
#define	MATCH_SSID(ni, ssid, ssidlen) \
	(ni->ni_esslen == ssidlen && memcmp(ni->ni_essid, ssid, ssidlen) == 0)
	static const uint8_t zeromac[IEEE80211_ADDR_LEN];
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
		IEEE80211_DPRINTF(nt->nt_ic, IEEE80211_MSG_NODE,
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
		if ((ic->ic_opmode == IEEE80211_M_HOSTAP ||
		     ic->ic_opmode == IEEE80211_M_STA) &&
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
		if (ni->ni_inact > 0)
			ni->ni_inact--;
		/*
		 * Special case ourself; we may be idle for extended periods
		 * of time and regardless reclaiming our state is wrong.
		 */
		if (ni == ic->ic_bss)
			continue;
		if (ni->ni_associd != 0 || isadhoc) {
			/*
			 * Age frames on the power save queue.
			 */
			if (ieee80211_node_saveq_age(ni) != 0 &&
			    IEEE80211_NODE_SAVEQ_QLEN(ni) == 0 &&
			    ic->ic_set_tim != NULL)
				ic->ic_set_tim(ni, 0);
			/*
			 * Probe the station before time it out.  We
			 * send a null data frame which may not be
			 * universally supported by drivers (need it
			 * for ps-poll support so it should be...).
			 *
			 * XXX don't probe the station unless we've
			 *     received a frame from them (and have
			 *     some idea of the rates they are capable
			 *     of); this will get fixed more properly
			 *     soon with better handling of the rate set.
			 */
			if ((ic->ic_flags_ext & IEEE80211_FEXT_INACT) &&
			    (0 < ni->ni_inact &&
			     ni->ni_inact <= ic->ic_inact_probe) &&
			    ni->ni_rates.rs_nrates != 0) {
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
		if ((ic->ic_flags_ext & IEEE80211_FEXT_INACT) &&
		    ni->ni_inact <= 0) {
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
			 * in case the driver takes a lock, as this can result
			 * in a LOR between the node lock and the driver lock.
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
}

void
ieee80211_node_timeout(void *arg)
{
	struct ieee80211com *ic = arg;

	ieee80211_scan_timeout(ic);
	ieee80211_timeout_stations(&ic->ic_sta);

	callout_reset(&ic->ic_inact, IEEE80211_INACT_WAIT*hz,
		ieee80211_node_timeout, ic);
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
	printf("\trstamp %u rssi %d noise %d intval %u capinfo 0x%x\n",
		ni->ni_rstamp, ni->ni_rssi, ni->ni_noise,
		ni->ni_intval, ni->ni_capinfo);
	printf("\tbssid %s essid \"%.*s\" channel %u:0x%x\n",
		ether_sprintf(ni->ni_bssid),
		ni->ni_esslen, ni->ni_essid,
		ni->ni_chan->ic_freq, ni->ni_chan->ic_flags);
	printf("\tfails %u inact %u txrate %u\n",
		ni->ni_fails, ni->ni_inact, ni->ni_txrate);
	printf("\thtcap %x htparam %x htctlchan %u ht2ndchan %u\n",
		ni->ni_htcap, ni->ni_htparam,
		ni->ni_htctlchan, ni->ni_ht2ndchan);
	printf("\thtopmode %x htstbc %x chw %u\n",
		ni->ni_htopmode, ni->ni_htstbc, ni->ni_chw);
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
		if (!IEEE80211_IS_CHAN_108G(ic->ic_bsschan)) {
			/*
			 * Don't force slot time when switched to turbo
			 * mode as non-ERP stations won't be present; this
			 * need only be done when on the normal G channel.
			 */
			ieee80211_set_shortslottime(ic, 0);
		}
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
		uint16_t aid;

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
		if (IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan) &&
		    IEEE80211_IS_CHAN_FULL(ic->ic_bsschan))
			ieee80211_node_join_11g(ic, ni);
	} else
		newassoc = 0;

	IEEE80211_DPRINTF(ic, IEEE80211_MSG_ASSOC | IEEE80211_MSG_DEBUG,
	    "[%s] station %sassociated at aid %d: %s preamble, %s slot time%s%s%s%s%s\n",
	    ether_sprintf(ni->ni_macaddr), newassoc ? "" : "re",
	    IEEE80211_NODE_AID(ni),
	    ic->ic_flags & IEEE80211_F_SHPREAMBLE ? "short" : "long",
	    ic->ic_flags & IEEE80211_F_SHSLOT ? "short" : "long",
	    ic->ic_flags & IEEE80211_F_USEPROT ? ", protection" : "",
	    ni->ni_flags & IEEE80211_NODE_QOS ? ", QoS" : "",
	    ni->ni_flags & IEEE80211_NODE_HT ?
		(ni->ni_chw == 20 ? ", HT20" : ", HT40") : "",
	    IEEE80211_ATH_CAP(ic, ni, IEEE80211_NODE_FF) ?
		", fast-frames" : "",
	    IEEE80211_ATH_CAP(ic, ni, IEEE80211_NODE_TURBOP) ?
		", turbo" : ""
	);

	/* give driver a chance to setup state like ni_txrate */
	if (ic->ic_newassoc != NULL)
		ic->ic_newassoc(ni, newassoc);
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

	KASSERT(IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan),
	     ("not in 11g, bss %u:0x%x, curmode %u", ic->ic_bsschan->ic_freq,
	      ic->ic_bsschan->ic_flags, ic->ic_curmode));

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

	KASSERT(ic->ic_opmode != IEEE80211_M_STA,
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

	if (IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan) &&
	    IEEE80211_IS_CHAN_FULL(ic->ic_bsschan))
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

int8_t
ieee80211_getrssi(struct ieee80211com *ic)
{
#define	NZ(x)	((x) == 0 ? 1 : (x))
	struct ieee80211_node_table *nt = &ic->ic_sta;
	int rssi_samples;
	int32_t rssi_total;
	struct ieee80211_node *ni;

	rssi_total = 0;
	rssi_samples = 0;
	switch (ic->ic_opmode) {
	case IEEE80211_M_IBSS:		/* average of all ibss neighbors */
	case IEEE80211_M_AHDEMO:	/* average of all neighbors */
	case IEEE80211_M_HOSTAP:	/* average of all associated stations */
		/* XXX locking */
		TAILQ_FOREACH(ni, &nt->nt_node, ni_list)
			if (ic->ic_opmode == IEEE80211_M_HOSTAP ||
			    (ni->ni_capinfo & IEEE80211_CAPINFO_IBSS)) {
				int8_t rssi = ic->ic_node_getrssi(ni);
				if (rssi != 0) {
					rssi_samples++;
					rssi_total += rssi;
				}
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

void
ieee80211_getsignal(struct ieee80211com *ic, int8_t *rssi, int8_t *noise)
{

	if (ic->ic_bss == NULL)		/* NB: shouldn't happen */
		return;
	ic->ic_node_getsignal(ic->ic_bss, rssi, noise);
	/* for non-station mode return avg'd rssi accounting */
	if (ic->ic_opmode != IEEE80211_M_STA)
		*rssi = ieee80211_getrssi(ic);
}

/*
 * Node table support.
 */

static void
ieee80211_node_table_init(struct ieee80211com *ic,
	struct ieee80211_node_table *nt,
	const char *name, int inact, int keyixmax)
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

static void
ieee80211_node_table_reset(struct ieee80211_node_table *nt)
{

	IEEE80211_DPRINTF(nt->nt_ic, IEEE80211_MSG_NODE,
		"%s %s table\n", __func__, nt->nt_name);

	IEEE80211_NODE_LOCK(nt);
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
