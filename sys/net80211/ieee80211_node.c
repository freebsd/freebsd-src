/*-
 * Copyright (c) 2001 Atsushi Onoe
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

#include "opt_wlan.h"

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
#include <net80211/ieee80211_input.h>
#include <net80211/ieee80211_wds.h>

#include <net/bpf.h>

/*
 * Association id's are managed with a bit vector.
 */
#define	IEEE80211_AID_SET(_vap, b) \
	((_vap)->iv_aid_bitmap[IEEE80211_AID(b) / 32] |= \
		(1 << (IEEE80211_AID(b) % 32)))
#define	IEEE80211_AID_CLR(_vap, b) \
	((_vap)->iv_aid_bitmap[IEEE80211_AID(b) / 32] &= \
		~(1 << (IEEE80211_AID(b) % 32)))
#define	IEEE80211_AID_ISSET(_vap, b) \
	((_vap)->iv_aid_bitmap[IEEE80211_AID(b) / 32] & (1 << (IEEE80211_AID(b) % 32)))

#ifdef IEEE80211_DEBUG_REFCNT
#define REFCNT_LOC "%s (%s:%u) %p<%s> refcnt %d\n", __func__, func, line
#else
#define REFCNT_LOC "%s %p<%s> refcnt %d\n", __func__
#endif

static int ieee80211_sta_join1(struct ieee80211_node *);

static struct ieee80211_node *node_alloc(struct ieee80211vap *,
	const uint8_t [IEEE80211_ADDR_LEN]);
static void node_cleanup(struct ieee80211_node *);
static void node_free(struct ieee80211_node *);
static void node_age(struct ieee80211_node *);
static int8_t node_getrssi(const struct ieee80211_node *);
static void node_getsignal(const struct ieee80211_node *, int8_t *, int8_t *);
static void node_getmimoinfo(const struct ieee80211_node *,
	struct ieee80211_mimo_info *);

static void _ieee80211_free_node(struct ieee80211_node *);

static void ieee80211_node_table_init(struct ieee80211com *ic,
	struct ieee80211_node_table *nt, const char *name,
	int inact, int keymaxix);
static void ieee80211_node_table_reset(struct ieee80211_node_table *,
	struct ieee80211vap *);
static void ieee80211_node_reclaim(struct ieee80211_node *);
static void ieee80211_node_table_cleanup(struct ieee80211_node_table *nt);
static void ieee80211_erp_timeout(struct ieee80211com *);

MALLOC_DEFINE(M_80211_NODE, "80211node", "802.11 node state");
MALLOC_DEFINE(M_80211_NODE_IE, "80211nodeie", "802.11 node ie");

void
ieee80211_node_attach(struct ieee80211com *ic)
{
	ieee80211_node_table_init(ic, &ic->ic_sta, "station",
		IEEE80211_INACT_INIT, ic->ic_max_keyix);
	callout_init(&ic->ic_inact, CALLOUT_MPSAFE);
	callout_reset(&ic->ic_inact, IEEE80211_INACT_WAIT*hz,
		ieee80211_node_timeout, ic);

	ic->ic_node_alloc = node_alloc;
	ic->ic_node_free = node_free;
	ic->ic_node_cleanup = node_cleanup;
	ic->ic_node_age = node_age;
	ic->ic_node_drain = node_age;		/* NB: same as age */
	ic->ic_node_getrssi = node_getrssi;
	ic->ic_node_getsignal = node_getsignal;
	ic->ic_node_getmimoinfo = node_getmimoinfo;

	/*
	 * Set flags to be propagated to all vap's;
	 * these define default behaviour/configuration.
	 */
	ic->ic_flags_ext |= IEEE80211_FEXT_INACT; /* inactivity processing */
}

void
ieee80211_node_detach(struct ieee80211com *ic)
{

	callout_drain(&ic->ic_inact);
	ieee80211_node_table_cleanup(&ic->ic_sta);
}

void
ieee80211_node_vattach(struct ieee80211vap *vap)
{
	/* NB: driver can override */
	vap->iv_max_aid = IEEE80211_AID_DEF;

	/* default station inactivity timer setings */
	vap->iv_inact_init = IEEE80211_INACT_INIT;
	vap->iv_inact_auth = IEEE80211_INACT_AUTH;
	vap->iv_inact_run = IEEE80211_INACT_RUN;
	vap->iv_inact_probe = IEEE80211_INACT_PROBE;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_INACT,
	    "%s: init %u auth %u run %u probe %u\n", __func__,
	    vap->iv_inact_init, vap->iv_inact_auth,
	    vap->iv_inact_run, vap->iv_inact_probe);
}

void
ieee80211_node_latevattach(struct ieee80211vap *vap)
{
	if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
		/* XXX should we allow max aid to be zero? */
		if (vap->iv_max_aid < IEEE80211_AID_MIN) {
			vap->iv_max_aid = IEEE80211_AID_MIN;
			if_printf(vap->iv_ifp,
			    "WARNING: max aid too small, changed to %d\n",
			    vap->iv_max_aid);
		}
		MALLOC(vap->iv_aid_bitmap, uint32_t *,
			howmany(vap->iv_max_aid, 32) * sizeof(uint32_t),
			M_80211_NODE, M_NOWAIT | M_ZERO);
		if (vap->iv_aid_bitmap == NULL) {
			/* XXX no way to recover */
			printf("%s: no memory for AID bitmap, max aid %d!\n",
			    __func__, vap->iv_max_aid);
			vap->iv_max_aid = 0;
		}
	}

	ieee80211_reset_bss(vap);

	vap->iv_auth = ieee80211_authenticator_get(vap->iv_bss->ni_authmode);
}

void
ieee80211_node_vdetach(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;

	ieee80211_node_table_reset(&ic->ic_sta, vap);
	if (vap->iv_bss != NULL) {
		ieee80211_free_node(vap->iv_bss);
		vap->iv_bss = NULL;
	}
	if (vap->iv_aid_bitmap != NULL) {
		FREE(vap->iv_aid_bitmap, M_80211_NODE);
		vap->iv_aid_bitmap = NULL;
	}
}

/* 
 * Port authorize/unauthorize interfaces for use by an authenticator.
 */

void
ieee80211_node_authorize(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;

	ni->ni_flags |= IEEE80211_NODE_AUTH;
	ni->ni_inact_reload = vap->iv_inact_run;
	ni->ni_inact = ni->ni_inact_reload;

	IEEE80211_NOTE(vap, IEEE80211_MSG_INACT, ni,
	    "%s: inact_reload %u", __func__, ni->ni_inact_reload);
}

void
ieee80211_node_unauthorize(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;

	ni->ni_flags &= ~IEEE80211_NODE_AUTH;
	ni->ni_inact_reload = vap->iv_inact_auth;
	if (ni->ni_inact > ni->ni_inact_reload)
		ni->ni_inact = ni->ni_inact_reload;

	IEEE80211_NOTE(vap, IEEE80211_MSG_INACT, ni,
	    "%s: inact_reload %u inact %u", __func__,
	    ni->ni_inact_reload, ni->ni_inact);
}

/*
 * Fix tx parameters for a node according to ``association state''.
 */
static void
node_setuptxparms(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;

	if (ni->ni_flags & IEEE80211_NODE_HT) {
		if (IEEE80211_IS_CHAN_5GHZ(ni->ni_chan))
			ni->ni_txparms = &vap->iv_txparms[IEEE80211_MODE_11NA];
		else
			ni->ni_txparms = &vap->iv_txparms[IEEE80211_MODE_11NG];
	} else {				/* legacy rate handling */
		if (IEEE80211_IS_CHAN_A(ni->ni_chan))
			ni->ni_txparms = &vap->iv_txparms[IEEE80211_MODE_11A];
		else if (ni->ni_flags & IEEE80211_NODE_ERP)
			ni->ni_txparms = &vap->iv_txparms[IEEE80211_MODE_11G];
		else
			ni->ni_txparms = &vap->iv_txparms[IEEE80211_MODE_11B];
	}
}

/*
 * Set/change the channel.  The rate set is also updated as
 * to insure a consistent view by drivers.
 * XXX should be private but hostap needs it to deal with CSA
 */
void
ieee80211_node_set_chan(struct ieee80211_node *ni,
	struct ieee80211_channel *chan)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211vap *vap = ni->ni_vap;
	enum ieee80211_phymode mode;

	KASSERT(chan != IEEE80211_CHAN_ANYC, ("no channel"));

	ni->ni_chan = chan;
	mode = ieee80211_chan2mode(chan);
	if (IEEE80211_IS_CHAN_HT(chan)) {
		/*
		 * XXX Gotta be careful here; the rate set returned by
		 * ieee80211_get_suprates is actually any HT rate
		 * set so blindly copying it will be bad.  We must
		 * install the legacy rate est in ni_rates and the
		 * HT rate set in ni_htrates.
		 */
		ni->ni_htrates = *ieee80211_get_suphtrates(ic, chan);
		/*
		 * Setup bss tx parameters based on operating mode.  We
		 * use legacy rates when operating in a mixed HT+non-HT bss
		 * and non-ERP rates in 11g for mixed ERP+non-ERP bss.
		 */
		if (mode == IEEE80211_MODE_11NA &&
		    (vap->iv_flags_ext & IEEE80211_FEXT_PUREN) == 0)
			mode = IEEE80211_MODE_11A;
		else if (mode == IEEE80211_MODE_11NG &&
		    (vap->iv_flags_ext & IEEE80211_FEXT_PUREN) == 0)
			mode = IEEE80211_MODE_11G;
		if (mode == IEEE80211_MODE_11G &&
		    (vap->iv_flags & IEEE80211_F_PUREG) == 0)
			mode = IEEE80211_MODE_11B;
	}
	ni->ni_txparms = &vap->iv_txparms[mode];
	ni->ni_rates = *ieee80211_get_suprates(ic, chan);
}

static __inline void
copy_bss(struct ieee80211_node *nbss, const struct ieee80211_node *obss)
{
	/* propagate useful state */
	nbss->ni_authmode = obss->ni_authmode;
	nbss->ni_txpower = obss->ni_txpower;
	nbss->ni_vlan = obss->ni_vlan;
	/* XXX statistics? */
	/* XXX legacy WDS bssid? */
}

void
ieee80211_create_ibss(struct ieee80211vap* vap, struct ieee80211_channel *chan)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_SCAN,
		"%s: creating ibss on channel %u\n", __func__,
		ieee80211_chan2ieee(ic, chan));

	ni = ieee80211_alloc_node(&ic->ic_sta, vap, vap->iv_myaddr);
	if (ni == NULL) {
		/* XXX recovery? */
		return;
	}
	IEEE80211_ADDR_COPY(ni->ni_bssid, vap->iv_myaddr);
	ni->ni_esslen = vap->iv_des_ssid[0].len;
	memcpy(ni->ni_essid, vap->iv_des_ssid[0].ssid, ni->ni_esslen);
	if (vap->iv_bss != NULL)
		copy_bss(ni, vap->iv_bss);
	ni->ni_intval = ic->ic_bintval;
	if (vap->iv_flags & IEEE80211_F_PRIVACY)
		ni->ni_capinfo |= IEEE80211_CAPINFO_PRIVACY;
	if (ic->ic_phytype == IEEE80211_T_FH) {
		ni->ni_fhdwell = 200;	/* XXX */
		ni->ni_fhindex = 1;
	}
	if (vap->iv_opmode == IEEE80211_M_IBSS) {
		vap->iv_flags |= IEEE80211_F_SIBSS;
		ni->ni_capinfo |= IEEE80211_CAPINFO_IBSS;	/* XXX */
		if (vap->iv_flags & IEEE80211_F_DESBSSID)
			IEEE80211_ADDR_COPY(ni->ni_bssid, vap->iv_des_bssid);
		else {
			get_random_bytes(ni->ni_bssid, IEEE80211_ADDR_LEN);
			/* clear group bit, add local bit */
			ni->ni_bssid[0] = (ni->ni_bssid[0] &~ 0x01) | 0x02;
		}
	} else if (vap->iv_opmode == IEEE80211_M_AHDEMO) {
		if (vap->iv_flags & IEEE80211_F_DESBSSID)
			IEEE80211_ADDR_COPY(ni->ni_bssid, vap->iv_des_bssid);
		else
			memset(ni->ni_bssid, 0, IEEE80211_ADDR_LEN);
	}
	/* 
	 * Fix the channel and related attributes.
	 */
	/* clear DFS CAC state on previous channel */
	if (ic->ic_bsschan != IEEE80211_CHAN_ANYC &&
	    ic->ic_bsschan->ic_freq != chan->ic_freq &&
	    IEEE80211_IS_CHAN_CACDONE(ic->ic_bsschan))
		ieee80211_dfs_cac_clear(ic, ic->ic_bsschan);
	ic->ic_bsschan = chan;
	ieee80211_node_set_chan(ni, chan);
	ic->ic_curmode = ieee80211_chan2mode(chan);
	/*
	 * Do mode-specific setup.
	 */
	if (IEEE80211_IS_CHAN_FULL(chan)) {
		if (IEEE80211_IS_CHAN_ANYG(chan)) {
			/*
			 * Use a mixed 11b/11g basic rate set.
			 */
			ieee80211_setbasicrates(&ni->ni_rates,
			    IEEE80211_MODE_11G);
			if (vap->iv_flags & IEEE80211_F_PUREG) {
				/*
				 * Also mark OFDM rates basic so 11b
				 * stations do not join (WiFi compliance).
				 */
				ieee80211_addbasicrates(&ni->ni_rates,
				    IEEE80211_MODE_11A);
			}
		} else if (IEEE80211_IS_CHAN_B(chan)) {
			/*
			 * Force pure 11b rate set.
			 */
			ieee80211_setbasicrates(&ni->ni_rates,
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
ieee80211_reset_bss(struct ieee80211vap *vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni, *obss;

	ieee80211_node_table_reset(&ic->ic_sta, vap);
	/* XXX multi-bss: wrong */
	ieee80211_reset_erp(ic);

	ni = ieee80211_alloc_node(&ic->ic_sta, vap, vap->iv_myaddr);
	KASSERT(ni != NULL, ("unable to setup inital BSS node"));
	obss = vap->iv_bss;
	vap->iv_bss = ieee80211_ref_node(ni);
	if (obss != NULL) {
		copy_bss(ni, obss);
		ni->ni_intval = ic->ic_bintval;
		ieee80211_free_node(obss);
	} else
		IEEE80211_ADDR_COPY(ni->ni_bssid, vap->iv_myaddr);
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
check_bss(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
        uint8_t rate;

	if (isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic, ni->ni_chan)))
		return 0;
	if (vap->iv_opmode == IEEE80211_M_IBSS) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) == 0)
			return 0;
	} else {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_ESS) == 0)
			return 0;
	}
	if (vap->iv_flags & IEEE80211_F_PRIVACY) {
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
	if (vap->iv_des_nssid != 0 &&
	    !match_ssid(ni, vap->iv_des_nssid, vap->iv_des_ssid))
		return 0;
	if ((vap->iv_flags & IEEE80211_F_DESBSSID) &&
	    !IEEE80211_ADDR_EQ(vap->iv_des_bssid, ni->ni_bssid))
		return 0;
	return 1;
}

#ifdef IEEE80211_DEBUG
/*
 * Display node suitability/compatibility.
 */
static void
check_bss_debug(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
        uint8_t rate;
        int fail;

	fail = 0;
	if (isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic, ni->ni_chan)))
		fail |= 0x01;
	if (vap->iv_opmode == IEEE80211_M_IBSS) {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) == 0)
			fail |= 0x02;
	} else {
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_ESS) == 0)
			fail |= 0x02;
	}
	if (vap->iv_flags & IEEE80211_F_PRIVACY) {
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
	if (vap->iv_des_nssid != 0 &&
	    !match_ssid(ni, vap->iv_des_nssid, vap->iv_des_ssid))
		fail |= 0x10;
	if ((vap->iv_flags & IEEE80211_F_DESBSSID) &&
	    !IEEE80211_ADDR_EQ(vap->iv_des_bssid, ni->ni_bssid))
		fail |= 0x20;

	printf(" %c %s", fail ? '-' : '+', ether_sprintf(ni->ni_macaddr));
	printf(" %s%c", ether_sprintf(ni->ni_bssid), fail & 0x20 ? '!' : ' ');
	printf(" %3d%c",
	    ieee80211_chan2ieee(ic, ni->ni_chan), fail & 0x01 ? '!' : ' ');
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
	struct ieee80211vap *vap = ni->ni_vap;
#ifdef IEEE80211_DEBUG
	struct ieee80211com *ic = ni->ni_ic;
#endif

	if (ni == vap->iv_bss ||
	    IEEE80211_ADDR_EQ(ni->ni_bssid, vap->iv_bss->ni_bssid)) {
		/* unchanged, nothing to do */
		return 0;
	}
	if (!check_bss(vap, ni)) {
		/* capabilities mismatch */
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
		    "%s: merge failed, capabilities mismatch\n", __func__);
#ifdef IEEE80211_DEBUG
		if (ieee80211_msg_assoc(vap))
			check_bss_debug(vap, ni);
#endif
		vap->iv_stats.is_ibss_capmismatch++;
		return 0;
	}
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
		"%s: new bssid %s: %s preamble, %s slot time%s\n", __func__,
		ether_sprintf(ni->ni_bssid),
		ic->ic_flags&IEEE80211_F_SHPREAMBLE ? "short" : "long",
		ic->ic_flags&IEEE80211_F_SHSLOT ? "short" : "long",
		ic->ic_flags&IEEE80211_F_USEPROT ? ", protection" : ""
	);
	return ieee80211_sta_join1(ieee80211_ref_node(ni));
}

/*
 * Calculate HT channel promotion flags for all vaps.
 * This assumes ni_chan have been setup for each vap.
 */
static int
gethtadjustflags(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;
	int flags;

	flags = 0;
	/* XXX locking */
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
		if (vap->iv_state < IEEE80211_S_RUN)
			continue;
		switch (vap->iv_opmode) {
		case IEEE80211_M_WDS:
		case IEEE80211_M_STA:
		case IEEE80211_M_AHDEMO:
		case IEEE80211_M_HOSTAP:
		case IEEE80211_M_IBSS:
			flags |= ieee80211_htchanflags(vap->iv_bss->ni_chan);
			break;
		default:
			break;
		}
	}
	return flags;
}

/*
 * Check if the current channel needs to change based on whether
 * any vap's are using HT20/HT40.  This is used sync the state of
 * ic_curchan after a channel width change on a running vap.
 */
void
ieee80211_sync_curchan(struct ieee80211com *ic)
{
	struct ieee80211_channel *c;

	c = ieee80211_ht_adjust_channel(ic, ic->ic_curchan, gethtadjustflags(ic));
	if (c != ic->ic_curchan) {
		ic->ic_curchan = c;
		ic->ic_curmode = ieee80211_chan2mode(ic->ic_curchan);
		ic->ic_set_channel(ic);
	}
}

/*
 * Change the current channel.  The request channel may be
 * promoted if other vap's are operating with HT20/HT40.
 */
void
ieee80211_setcurchan(struct ieee80211com *ic, struct ieee80211_channel *c)
{
	if (ic->ic_htcaps & IEEE80211_HTC_HT) {
		int flags = gethtadjustflags(ic);
		/*
		 * Check for channel promotion required to support the
		 * set of running vap's.  This assumes we are called
		 * after ni_chan is setup for each vap.
		 */
		/* NB: this assumes IEEE80211_FEXT_USEHT40 > IEEE80211_FEXT_HT */
		if (flags > ieee80211_htchanflags(c))
			c = ieee80211_ht_adjust_channel(ic, c, flags);
	}
	ic->ic_bsschan = ic->ic_curchan = c;
	ic->ic_curmode = ieee80211_chan2mode(ic->ic_curchan);
	ic->ic_set_channel(ic);
}

/*
 * Join the specified IBSS/BSS network.  The node is assumed to
 * be passed in with a held reference.
 */
static int
ieee80211_sta_join1(struct ieee80211_node *selbs)
{
	struct ieee80211vap *vap = selbs->ni_vap;
	struct ieee80211com *ic = selbs->ni_ic;
	struct ieee80211_node *obss;
	int canreassoc;

	/*
	 * Committed to selbs, setup state.
	 */
	obss = vap->iv_bss;
	/*
	 * Check if old+new node have the same address in which
	 * case we can reassociate when operating in sta mode.
	 */
	canreassoc = (obss != NULL &&
		vap->iv_state == IEEE80211_S_RUN &&
		IEEE80211_ADDR_EQ(obss->ni_macaddr, selbs->ni_macaddr));
	vap->iv_bss = selbs;		/* NB: caller assumed to bump refcnt */
	if (obss != NULL) {
		copy_bss(selbs, obss);
		ieee80211_node_reclaim(obss);
		obss = NULL;		/* NB: guard against later use */
	}

	/*
	 * Delete unusable rates; we've already checked
	 * that the negotiated rate set is acceptable.
	 */
	ieee80211_fix_rate(vap->iv_bss, &vap->iv_bss->ni_rates,
		IEEE80211_F_DODEL | IEEE80211_F_JOIN);

	ieee80211_setcurchan(ic, selbs->ni_chan);
	/*
	 * Set the erp state (mostly the slot time) to deal with
	 * the auto-select case; this should be redundant if the
	 * mode is locked.
	 */ 
	ieee80211_reset_erp(ic);
	ieee80211_wme_initparams(vap);

	if (vap->iv_opmode == IEEE80211_M_STA) {
		if (canreassoc) {
			/* Reassociate */
			ieee80211_new_state(vap, IEEE80211_S_ASSOC, 1);
		} else {
			/*
			 * Act as if we received a DEAUTH frame in case we
			 * are invoked from the RUN state.  This will cause
			 * us to try to re-authenticate if we are operating
			 * as a station.
			 */
			ieee80211_new_state(vap, IEEE80211_S_AUTH,
				IEEE80211_FC0_SUBTYPE_DEAUTH);
		}
	} else
		ieee80211_new_state(vap, IEEE80211_S_RUN, -1);
	return 1;
}

int
ieee80211_sta_join(struct ieee80211vap *vap, struct ieee80211_channel *chan,
	const struct ieee80211_scan_entry *se)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;

	ni = ieee80211_alloc_node(&ic->ic_sta, vap, se->se_macaddr);
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
	ni->ni_chan = chan;
	ni->ni_timoff = se->se_timoff;
	ni->ni_fhdwell = se->se_fhdwell;
	ni->ni_fhindex = se->se_fhindex;
	ni->ni_erp = se->se_erp;
	IEEE80211_RSSI_LPF(ni->ni_avgrssi, se->se_rssi);
	ni->ni_noise = se->se_noise;

	if (ieee80211_ies_init(&ni->ni_ies, se->se_ies.data, se->se_ies.len)) {
		ieee80211_ies_expand(&ni->ni_ies);
		if (ni->ni_ies.ath_ie != NULL)
			ieee80211_parse_ath(ni, ni->ni_ies.ath_ie);
		if (ni->ni_ies.htcap_ie != NULL)
			ieee80211_parse_htcap(ni, ni->ni_ies.htcap_ie);
		if (ni->ni_ies.htinfo_ie != NULL)
			ieee80211_parse_htinfo(ni, ni->ni_ies.htinfo_ie);
	}

	vap->iv_dtim_period = se->se_dtimperiod;
	vap->iv_dtim_count = 0;

	/* NB: must be after ni_chan is setup */
	ieee80211_setup_rates(ni, se->se_rates, se->se_xrates,
		IEEE80211_F_DOSORT);

	return ieee80211_sta_join1(ieee80211_ref_node(ni));
}

/*
 * Leave the specified IBSS/BSS network.  The node is assumed to
 * be passed in with a held reference.
 */
void
ieee80211_sta_leave(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;

	ic->ic_node_cleanup(ni);
	ieee80211_notify_node_leave(ni);
}

/*
 * Send a deauthenticate frame and drop the station.
 */
void
ieee80211_node_deauth(struct ieee80211_node *ni, int reason)
{
	/* NB: bump the refcnt to be sure temporay nodes are not reclaimed */
	ieee80211_ref_node(ni);
	if (ni->ni_associd != 0)
		IEEE80211_SEND_MGMT(ni, IEEE80211_FC0_SUBTYPE_DEAUTH, reason);
	ieee80211_node_leave(ni);
	ieee80211_free_node(ni);
}

static struct ieee80211_node *
node_alloc(struct ieee80211vap *vap, const uint8_t macaddr[IEEE80211_ADDR_LEN])
{
	struct ieee80211_node *ni;

	MALLOC(ni, struct ieee80211_node *, sizeof(struct ieee80211_node),
		M_80211_NODE, M_NOWAIT | M_ZERO);
	return ni;
}

/*
 * Initialize an ie blob with the specified data.  If previous
 * data exists re-use the data block.  As a side effect we clear
 * all references to specific ie's; the caller is required to
 * recalculate them.
 */
int
ieee80211_ies_init(struct ieee80211_ies *ies, const uint8_t *data, int len)
{
	/* NB: assumes data+len are the last fields */
	memset(ies, 0, offsetof(struct ieee80211_ies, data));
	if (ies->data != NULL && ies->len != len) {
		/* data size changed */
		FREE(ies->data, M_80211_NODE_IE);
		ies->data = NULL;
	}
	if (ies->data == NULL) {
		MALLOC(ies->data, uint8_t *, len, M_80211_NODE_IE, M_NOWAIT);
		if (ies->data == NULL) {
			ies->len = 0;
			/* NB: pointers have already been zero'd above */
			return 0;
		}
	}
	memcpy(ies->data, data, len);
	ies->len = len;
	return 1;
}

/*
 * Reclaim storage for an ie blob.
 */
void
ieee80211_ies_cleanup(struct ieee80211_ies *ies)
{
	if (ies->data != NULL)
		FREE(ies->data, M_80211_NODE_IE);
}

/*
 * Expand an ie blob data contents and to fillin individual
 * ie pointers.  The data blob is assumed to be well-formed;
 * we don't do any validity checking of ie lengths.
 */
void
ieee80211_ies_expand(struct ieee80211_ies *ies)
{
	uint8_t *ie;
	int ielen;

	ie = ies->data;
	ielen = ies->len;
	while (ielen > 0) {
		switch (ie[0]) {
		case IEEE80211_ELEMID_VENDOR:
			if (iswpaoui(ie))
				ies->wpa_ie = ie;
			else if (iswmeoui(ie))
				ies->wme_ie = ie;
			else if (isatherosoui(ie))
				ies->ath_ie = ie;
			break;
		case IEEE80211_ELEMID_RSN:
			ies->rsn_ie = ie;
			break;
		case IEEE80211_ELEMID_HTCAP:
			ies->htcap_ie = ie;
			break;
		}
		ielen -= 2 + ie[1];
		ie += 2 + ie[1];
	}
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
	struct ieee80211vap *vap = ni->ni_vap;
	int i;

	/* NB: preserve ni_table */
	if (ni->ni_flags & IEEE80211_NODE_PWR_MGT) {
		if (vap->iv_opmode != IEEE80211_M_STA)
			vap->iv_ps_sta--;
		ni->ni_flags &= ~IEEE80211_NODE_PWR_MGT;
		IEEE80211_NOTE(vap, IEEE80211_MSG_POWER, ni,
		    "power save mode off, %u sta's in ps mode", vap->iv_ps_sta);
	}
	/*
	 * Cleanup any HT-related state.
	 */
	if (ni->ni_flags & IEEE80211_NODE_HT)
		ieee80211_ht_node_cleanup(ni);
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
	if (ieee80211_node_saveq_drain(ni) != 0 && vap->iv_set_tim != NULL)
		vap->iv_set_tim(ni, 0);

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
	ieee80211_ies_cleanup(&ni->ni_ies);
	IEEE80211_NODE_SAVEQ_DESTROY(ni);
	IEEE80211_NODE_WDSQ_DESTROY(ni);
	FREE(ni, M_80211_NODE);
}

static void
node_age(struct ieee80211_node *ni)
{
	struct ieee80211vap *vap = ni->ni_vap;
#if 0
	IEEE80211_NODE_LOCK_ASSERT(&ic->ic_sta);
#endif
	/*
	 * Age frames on the power save queue.
	 */
	if (ieee80211_node_saveq_age(ni) != 0 &&
	    IEEE80211_NODE_SAVEQ_QLEN(ni) == 0 &&
	    vap->iv_set_tim != NULL)
		vap->iv_set_tim(ni, 0);
	/*
	 * Age frames on the wds pending queue.
	 */
	if (IEEE80211_NODE_WDSQ_QLEN(ni) != 0)
		ieee80211_node_wdsq_age(ni);
	/*
	 * Age out HT resources (e.g. frames on the
	 * A-MPDU reorder queues).
	 */
	if (ni->ni_associd != 0 && (ni->ni_flags & IEEE80211_NODE_HT))
		ieee80211_ht_node_age(ni);
}

static int8_t
node_getrssi(const struct ieee80211_node *ni)
{
	uint32_t avgrssi = ni->ni_avgrssi;
	int32_t rssi;

	if (avgrssi == IEEE80211_RSSI_DUMMY_MARKER)
		return 0;
	rssi = IEEE80211_RSSI_GET(avgrssi);
	return rssi < 0 ? 0 : rssi > 127 ? 127 : rssi;
}

static void
node_getsignal(const struct ieee80211_node *ni, int8_t *rssi, int8_t *noise)
{
	*rssi = node_getrssi(ni);
	*noise = ni->ni_noise;
}

static void
node_getmimoinfo(const struct ieee80211_node *ni,
	struct ieee80211_mimo_info *info)
{
	/* XXX zero data? */
}

struct ieee80211_node *
ieee80211_alloc_node(struct ieee80211_node_table *nt,
	struct ieee80211vap *vap, const uint8_t macaddr[IEEE80211_ADDR_LEN])
{
	struct ieee80211com *ic = nt->nt_ic;
	struct ieee80211_node *ni;
	int hash;

	ni = ic->ic_node_alloc(vap, macaddr);
	if (ni == NULL) {
		vap->iv_stats.is_rx_nodealloc++;
		return NULL;
	}

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
		"%s %p<%s> in %s table\n", __func__, ni,
		ether_sprintf(macaddr), nt->nt_name);

	IEEE80211_ADDR_COPY(ni->ni_macaddr, macaddr);
	hash = IEEE80211_NODE_HASH(macaddr);
	ieee80211_node_initref(ni);		/* mark referenced */
	ni->ni_chan = IEEE80211_CHAN_ANYC;
	ni->ni_authmode = IEEE80211_AUTH_OPEN;
	ni->ni_txpower = ic->ic_txpowlimit;	/* max power */
	ni->ni_txparms = &vap->iv_txparms[ieee80211_chan2mode(ic->ic_curchan)];
	ieee80211_crypto_resetkey(vap, &ni->ni_ucastkey, IEEE80211_KEYIX_NONE);
	ni->ni_avgrssi = IEEE80211_RSSI_DUMMY_MARKER;
	ni->ni_inact_reload = nt->nt_inact_init;
	ni->ni_inact = ni->ni_inact_reload;
	ni->ni_ath_defkeyix = 0x7fff;
	IEEE80211_NODE_SAVEQ_INIT(ni, "unknown");
	IEEE80211_NODE_WDSQ_INIT(ni, "unknown");

	IEEE80211_NODE_LOCK(nt);
	TAILQ_INSERT_TAIL(&nt->nt_node, ni, ni_list);
	LIST_INSERT_HEAD(&nt->nt_hash[hash], ni, ni_hash);
	ni->ni_table = nt;
	ni->ni_vap = vap;
	ni->ni_ic = ic;
	IEEE80211_NODE_UNLOCK(nt);

	IEEE80211_NOTE(vap, IEEE80211_MSG_INACT, ni,
	    "%s: inact_reload %u", __func__, ni->ni_inact_reload);

	return ni;
}

/*
 * Craft a temporary node suitable for sending a management frame
 * to the specified station.  We craft only as much state as we
 * need to do the work since the node will be immediately reclaimed
 * once the send completes.
 */
struct ieee80211_node *
ieee80211_tmp_node(struct ieee80211vap *vap,
	const uint8_t macaddr[IEEE80211_ADDR_LEN])
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;

	ni = ic->ic_node_alloc(vap, macaddr);
	if (ni != NULL) {
		struct ieee80211_node *bss = vap->iv_bss;

		IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
			"%s %p<%s>\n", __func__, ni, ether_sprintf(macaddr));

		ni->ni_table = NULL;		/* NB: pedantic */
		ni->ni_ic = ic;			/* NB: needed to set channel */
		ni->ni_vap = vap;

		IEEE80211_ADDR_COPY(ni->ni_macaddr, macaddr);
		IEEE80211_ADDR_COPY(ni->ni_bssid, bss->ni_bssid);
		ieee80211_node_initref(ni);		/* mark referenced */
		/* NB: required by ieee80211_fix_rate */
		ieee80211_node_set_chan(ni, bss->ni_chan);
		ieee80211_crypto_resetkey(vap, &ni->ni_ucastkey,
			IEEE80211_KEYIX_NONE);
		ni->ni_txpower = bss->ni_txpower;
		/* XXX optimize away */
		IEEE80211_NODE_SAVEQ_INIT(ni, "unknown");
		IEEE80211_NODE_WDSQ_INIT(ni, "unknown");
	} else {
		/* XXX msg */
		vap->iv_stats.is_rx_nodealloc++;
	}
	return ni;
}

struct ieee80211_node *
ieee80211_dup_bss(struct ieee80211vap *vap,
	const uint8_t macaddr[IEEE80211_ADDR_LEN])
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;

	ni = ieee80211_alloc_node(&ic->ic_sta, vap, macaddr);
	if (ni != NULL) {
		struct ieee80211_node *bss = vap->iv_bss;
		/*
		 * Inherit from iv_bss.
		 */
		copy_bss(ni, bss);
		IEEE80211_ADDR_COPY(ni->ni_bssid, bss->ni_bssid);
		ieee80211_node_set_chan(ni, bss->ni_chan);
	}
	return ni;
}

/*
 * Create a bss node for a legacy WDS vap.  The far end does
 * not associate so we just create create a new node and
 * simulate an association.  The caller is responsible for
 * installing the node as the bss node and handling any further
 * setup work like authorizing the port.
 */
struct ieee80211_node *
ieee80211_node_create_wds(struct ieee80211vap *vap,
	const uint8_t bssid[IEEE80211_ADDR_LEN], struct ieee80211_channel *chan)
{
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;

	/* XXX check if node already in sta table? */
	ni = ieee80211_alloc_node(&ic->ic_sta, vap, bssid);
	if (ni != NULL) {
		ni->ni_wdsvap = vap;
		IEEE80211_ADDR_COPY(ni->ni_bssid, bssid);
		/*
		 * Inherit any manually configured settings.
		 */
		copy_bss(ni, vap->iv_bss);
		ieee80211_node_set_chan(ni, chan);
		/* NB: propagate ssid so available to WPA supplicant */
		ni->ni_esslen = vap->iv_des_ssid[0].len;
		memcpy(ni->ni_essid, vap->iv_des_ssid[0].ssid, ni->ni_esslen);
		/* NB: no associd for peer */
		/*
		 * There are no management frames to use to
		 * discover neighbor capabilities, so blindly
		 * propagate the local configuration.
		 */
		if (vap->iv_flags & IEEE80211_F_WME)
			ni->ni_flags |= IEEE80211_NODE_QOS;
		if (vap->iv_flags & IEEE80211_F_FF)
			ni->ni_flags |= IEEE80211_NODE_FF;
		if ((ic->ic_htcaps & IEEE80211_HTC_HT) &&
		    (vap->iv_flags_ext & IEEE80211_FEXT_HT)) {
			/*
			 * Device is HT-capable and HT is enabled for
			 * the vap; setup HT operation.  On return
			 * ni_chan will be adjusted to an HT channel.
			 */
			ieee80211_ht_wds_init(ni);
		} else {
			struct ieee80211_channel *c = ni->ni_chan;
			/*
			 * Force a legacy channel to be used.
			 */
			c = ieee80211_find_channel(ic,
			    c->ic_freq, c->ic_flags &~ IEEE80211_CHAN_HT);
			KASSERT(c != NULL, ("no legacy channel, %u/%x",
			    ni->ni_chan->ic_freq, ni->ni_chan->ic_flags));
			ni->ni_chan = c;
		}
	}
	return ni;
}

struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_node_locked_debug(struct ieee80211_node_table *nt,
	const uint8_t macaddr[IEEE80211_ADDR_LEN], const char *func, int line)
#else
ieee80211_find_node_locked(struct ieee80211_node_table *nt,
	const uint8_t macaddr[IEEE80211_ADDR_LEN])
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
			IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_NODE,
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

struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_node_debug(struct ieee80211_node_table *nt,
	const uint8_t macaddr[IEEE80211_ADDR_LEN], const char *func, int line)
#else
ieee80211_find_node(struct ieee80211_node_table *nt,
	const uint8_t macaddr[IEEE80211_ADDR_LEN])
#endif
{
	struct ieee80211_node *ni;

	IEEE80211_NODE_LOCK(nt);
	ni = ieee80211_find_node_locked(nt, macaddr);
	IEEE80211_NODE_UNLOCK(nt);
	return ni;
}

struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_vap_node_locked_debug(struct ieee80211_node_table *nt,
	const struct ieee80211vap *vap,
	const uint8_t macaddr[IEEE80211_ADDR_LEN], const char *func, int line)
#else
ieee80211_find_vap_node_locked(struct ieee80211_node_table *nt,
	const struct ieee80211vap *vap,
	const uint8_t macaddr[IEEE80211_ADDR_LEN])
#endif
{
	struct ieee80211_node *ni;
	int hash;

	IEEE80211_NODE_LOCK_ASSERT(nt);

	hash = IEEE80211_NODE_HASH(macaddr);
	LIST_FOREACH(ni, &nt->nt_hash[hash], ni_hash) {
		if (ni->ni_vap == vap &&
		    IEEE80211_ADDR_EQ(ni->ni_macaddr, macaddr)) {
			ieee80211_ref_node(ni);	/* mark referenced */
#ifdef IEEE80211_DEBUG_REFCNT
			IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_NODE,
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

struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_vap_node_debug(struct ieee80211_node_table *nt,
	const struct ieee80211vap *vap,
	const uint8_t macaddr[IEEE80211_ADDR_LEN], const char *func, int line)
#else
ieee80211_find_vap_node(struct ieee80211_node_table *nt,
	const struct ieee80211vap *vap,
	const uint8_t macaddr[IEEE80211_ADDR_LEN])
#endif
{
	struct ieee80211_node *ni;

	IEEE80211_NODE_LOCK(nt);
	ni = ieee80211_find_vap_node_locked(nt, vap, macaddr);
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
ieee80211_fakeup_adhoc_node(struct ieee80211vap *vap,
	const uint8_t macaddr[IEEE80211_ADDR_LEN])
{
	struct ieee80211_node *ni;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
	    "%s: mac<%s>\n", __func__, ether_sprintf(macaddr));
	ni = ieee80211_dup_bss(vap, macaddr);
	if (ni != NULL) {
		struct ieee80211com *ic = vap->iv_ic;

		/* XXX no rate negotiation; just dup */
		ni->ni_rates = vap->iv_bss->ni_rates;
		if (vap->iv_opmode == IEEE80211_M_AHDEMO) {
			/*
			 * In adhoc demo mode there are no management
			 * frames to use to discover neighbor capabilities,
			 * so blindly propagate the local configuration 
			 * so we can do interesting things (e.g. use
			 * WME to disable ACK's).
			 */
			if (vap->iv_flags & IEEE80211_F_WME)
				ni->ni_flags |= IEEE80211_NODE_QOS;
			if (vap->iv_flags & IEEE80211_F_FF)
				ni->ni_flags |= IEEE80211_NODE_FF;
		}
		node_setuptxparms(ni);
		if (ic->ic_newassoc != NULL)
			ic->ic_newassoc(ni, 1);
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

	if (ieee80211_ies_init(&ni->ni_ies, sp->ies, sp->ies_len)) {
		ieee80211_ies_expand(&ni->ni_ies);
		if (ni->ni_ies.ath_ie != NULL)
			ieee80211_parse_ath(ni, ni->ni_ies.ath_ie);
	}

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
ieee80211_add_neighbor(struct ieee80211vap *vap,
	const struct ieee80211_frame *wh,
	const struct ieee80211_scanparams *sp)
{
	struct ieee80211_node *ni;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
	    "%s: mac<%s>\n", __func__, ether_sprintf(wh->i_addr2));
	ni = ieee80211_dup_bss(vap, wh->i_addr2);/* XXX alloc_node? */
	if (ni != NULL) {
		struct ieee80211com *ic = vap->iv_ic;

		ieee80211_init_neighbor(ni, wh, sp);
		node_setuptxparms(ni);
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
#define	IS_PROBEREQ(wh) \
	((wh->i_fc[0] & (IEEE80211_FC0_TYPE_MASK|IEEE80211_FC0_SUBTYPE_MASK)) \
	    == (IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_REQ))
#define	IS_BCAST_PROBEREQ(wh) \
	(IS_PROBEREQ(wh) && IEEE80211_IS_MULTICAST( \
	    ((const struct ieee80211_frame *)(wh))->i_addr3))

static __inline struct ieee80211_node *
_find_rxnode(struct ieee80211_node_table *nt,
    const struct ieee80211_frame_min *wh)
{
	/* XXX 4-address frames? */
	if (IS_CTL(wh) && !IS_PSPOLL(wh) && !IS_BAR(wh) /*&& !IS_RTS(ah)*/)
		return ieee80211_find_node_locked(nt, wh->i_addr1);
	if (IS_BCAST_PROBEREQ(wh))
		return NULL;		/* spam bcast probe req to all vap's */
	return ieee80211_find_node_locked(nt, wh->i_addr2);
}

/*
 * Locate the node for sender, track state, and then pass the
 * (referenced) node up to the 802.11 layer for its use.  Note
 * we can return NULL if the sender is not in the table.
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

	nt = &ic->ic_sta;
	IEEE80211_NODE_LOCK(nt);
	ni = _find_rxnode(nt, wh);
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
		ni = _find_rxnode(nt, wh);
		if (ni != NULL && nt->nt_keyixmap != NULL) {
			/*
			 * If the station has a unicast key cache slot
			 * assigned update the key->node mapping table.
			 */
			keyix = ni->ni_ucastkey.wk_rxkeyix;
			/* XXX can keyixmap[keyix] != NULL? */
			if (keyix < nt->nt_keyixmax &&
			    nt->nt_keyixmap[keyix] == NULL) {
				IEEE80211_DPRINTF(ni->ni_vap,
				    IEEE80211_MSG_NODE,
				    "%s: add key map entry %p<%s> refcnt %d\n",
				    __func__, ni, ether_sprintf(ni->ni_macaddr),
				    ieee80211_node_refcnt(ni)+1);
				nt->nt_keyixmap[keyix] = ieee80211_ref_node(ni);
			}
		}
	} else {
		if (IS_BCAST_PROBEREQ(wh))
			ni = NULL;	/* spam bcast probe req to all vap's */
		else
			ieee80211_ref_node(ni);
	}
	IEEE80211_NODE_UNLOCK(nt);

	return ni;
}
#undef IS_BCAST_PROBEREQ
#undef IS_PROBEREQ
#undef IS_BAR
#undef IS_PSPOLL
#undef IS_CTL

/*
 * Return a reference to the appropriate node for sending
 * a data frame.  This handles node discovery in adhoc networks.
 */
struct ieee80211_node *
#ifdef IEEE80211_DEBUG_REFCNT
ieee80211_find_txnode_debug(struct ieee80211vap *vap,
	const uint8_t macaddr[IEEE80211_ADDR_LEN],
	const char *func, int line)
#else
ieee80211_find_txnode(struct ieee80211vap *vap,
	const uint8_t macaddr[IEEE80211_ADDR_LEN])
#endif
{
	struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
	struct ieee80211_node *ni;

	/*
	 * The destination address should be in the node table
	 * unless this is a multicast/broadcast frame.  We can
	 * also optimize station mode operation, all frames go
	 * to the bss node.
	 */
	/* XXX can't hold lock across dup_bss 'cuz of recursive locking */
	IEEE80211_NODE_LOCK(nt);
	if (vap->iv_opmode == IEEE80211_M_STA ||
	    vap->iv_opmode == IEEE80211_M_WDS ||
	    IEEE80211_IS_MULTICAST(macaddr))
		ni = ieee80211_ref_node(vap->iv_bss);
	else {
		ni = ieee80211_find_node_locked(nt, macaddr);
		if (vap->iv_opmode == IEEE80211_M_HOSTAP && 
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
		if (vap->iv_opmode == IEEE80211_M_IBSS ||
		    vap->iv_opmode == IEEE80211_M_AHDEMO) {
			/*
			 * In adhoc mode cons up a node for the destination.
			 * Note that we need an additional reference for the
			 * caller to be consistent with
			 * ieee80211_find_node_locked.
			 */
			ni = ieee80211_fakeup_adhoc_node(vap, macaddr);
			if (ni != NULL)
				(void) ieee80211_ref_node(ni);
		} else {
			IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_OUTPUT, macaddr,
			    "no node, discard frame (%s)", __func__);
			vap->iv_stats.is_tx_nonode++;
		}
	}
	return ni;
}

static void
_ieee80211_free_node(struct ieee80211_node *ni)
{
	struct ieee80211_node_table *nt = ni->ni_table;

	/*
	 * NB: careful about referencing the vap as it may be
	 * gone if the last reference was held by a driver.
	 * We know the com will always be present so it's safe
	 * to use ni_ic below to reclaim resources.
	 */
#if 0
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_NODE,
		"%s %p<%s> in %s table\n", __func__, ni,
		ether_sprintf(ni->ni_macaddr),
		nt != NULL ? nt->nt_name : "<gone>");
#endif
	if (ni->ni_associd != 0) {
		struct ieee80211vap *vap = ni->ni_vap;
		if (vap->iv_aid_bitmap != NULL)
			IEEE80211_AID_CLR(vap, ni->ni_associd);
	}
	if (nt != NULL) {
		TAILQ_REMOVE(&nt->nt_node, ni, ni_list);
		LIST_REMOVE(ni, ni_hash);
	}
	ni->ni_ic->ic_node_free(ni);
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
	IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_NODE,
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
				IEEE80211_DPRINTF(ni->ni_vap,
				    IEEE80211_MSG_NODE,
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
	nikey = NULL;
	status = 1;		/* NB: success */
	if (!IEEE80211_KEY_UNDEFINED(&ni->ni_ucastkey)) {
		keyix = ni->ni_ucastkey.wk_rxkeyix;
		status = ieee80211_crypto_delkey(ni->ni_vap, &ni->ni_ucastkey);
		if (nt->nt_keyixmap != NULL && keyix < nt->nt_keyixmax) {
			nikey = nt->nt_keyixmap[keyix];
			nt->nt_keyixmap[keyix] = NULL;;
		}
	}
	if (!isowned)
		IEEE80211_NODE_UNLOCK(nt);

	if (nikey != NULL) {
		KASSERT(nikey == ni,
			("key map out of sync, ni %p nikey %p", ni, nikey));
		IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_NODE,
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

	IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_NODE,
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
		IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_NODE,
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

/*
 * Reclaim a (bss) node.  Decrement the refcnt and reclaim
 * the node if the only other reference to it is in the sta
 * table.  This is effectively ieee80211_free_node followed
 * by node_reclaim when the refcnt is 1 (after the free).
 */
static void
ieee80211_node_reclaim(struct ieee80211_node *ni)
{
	struct ieee80211_node_table *nt = ni->ni_table;

	KASSERT(nt != NULL, ("reclaim node not in table"));

#ifdef IEEE80211_DEBUG_REFCNT
	IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_NODE,
		"%s %p<%s> refcnt %d\n", __func__, ni,
		 ether_sprintf(ni->ni_macaddr), ieee80211_node_refcnt(ni)-1);
#endif
	IEEE80211_NODE_LOCK(nt);
	if (ieee80211_node_dectestref(ni)) {
		/*
		 * Last reference, reclaim state.
		 */
		_ieee80211_free_node(ni);
		nt = NULL;
	} else if (ieee80211_node_refcnt(ni) == 1 &&
	    nt->nt_keyixmap != NULL) {
		ieee80211_keyix keyix;
		/*
		 * Check for a last reference in the key mapping table.
		 */
		keyix = ni->ni_ucastkey.wk_rxkeyix;
		if (keyix < nt->nt_keyixmax &&
		    nt->nt_keyixmap[keyix] == ni) {
			IEEE80211_DPRINTF(ni->ni_vap,
			    IEEE80211_MSG_NODE,
			    "%s: %p<%s> clear key map entry", __func__,
			    ni, ether_sprintf(ni->ni_macaddr));
			nt->nt_keyixmap[keyix] = NULL;
			ieee80211_node_decref(ni); /* XXX needed? */
			_ieee80211_free_node(ni);
			nt = NULL;
		}
	}
	if (nt != NULL && ieee80211_node_refcnt(ni) == 1) {
		/*
		 * Last reference is in the sta table; complete
		 * the reclaim.  This handles bss nodes being
		 * recycled: the node has two references, one for
		 * iv_bss and one for the table.  After dropping
		 * the iv_bss ref above we need to reclaim the sta
		 * table reference.
		 */
		ieee80211_node_decref(ni);	/* NB: be pendantic */
		_ieee80211_free_node(ni);
	}
	IEEE80211_NODE_UNLOCK(nt);
}

/*
 * Node table support.
 */

static void
ieee80211_node_table_init(struct ieee80211com *ic,
	struct ieee80211_node_table *nt,
	const char *name, int inact, int keyixmax)
{
	struct ifnet *ifp = ic->ic_ifp;

	nt->nt_ic = ic;
	IEEE80211_NODE_LOCK_INIT(nt, ifp->if_xname);
	IEEE80211_NODE_ITERATE_LOCK_INIT(nt, ifp->if_xname);
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
ieee80211_node_table_reset(struct ieee80211_node_table *nt,
	struct ieee80211vap *match)
{
	struct ieee80211_node *ni, *next;

	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, next) {
		if (match != NULL && ni->ni_vap != match)
			continue;
		/* XXX can this happen?  if so need's work */
		if (ni->ni_associd != 0) {
			struct ieee80211vap *vap = ni->ni_vap;

			if (vap->iv_auth->ia_node_leave != NULL)
				vap->iv_auth->ia_node_leave(ni);
			if (vap->iv_aid_bitmap != NULL)
				IEEE80211_AID_CLR(vap, ni->ni_associd);
		}
		ni->ni_wdsvap = NULL;		/* clear reference */
		node_reclaim(nt, ni);
	}
	if (match != NULL && match->iv_opmode == IEEE80211_M_WDS) {
		/*
		 * Make a separate pass to clear references to this vap
		 * held by DWDS entries.  They will not be matched above
		 * because ni_vap will point to the ap vap but we still
		 * need to clear ni_wdsvap when the WDS vap is destroyed
		 * and/or reset.
		 */
		TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, next)
			if (ni->ni_wdsvap == match)
				ni->ni_wdsvap = NULL;
	}
	IEEE80211_NODE_UNLOCK(nt);
}

static void
ieee80211_node_table_cleanup(struct ieee80211_node_table *nt)
{
	ieee80211_node_table_reset(nt, NULL);
	if (nt->nt_keyixmap != NULL) {
#ifdef DIAGNOSTIC
		/* XXX verify all entries are NULL */
		int i;
		for (i = 0; i < nt->nt_keyixmax; i++)
			if (nt->nt_keyixmap[i] != NULL)
				printf("%s: %s[%u] still active\n", __func__,
					nt->nt_name, i);
#endif
		FREE(nt->nt_keyixmap, M_80211_NODE);
		nt->nt_keyixmap = NULL;
	}
	IEEE80211_NODE_ITERATE_LOCK_DESTROY(nt);
	IEEE80211_NODE_LOCK_DESTROY(nt);
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
ieee80211_timeout_stations(struct ieee80211com *ic)
{
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211vap *vap;
	struct ieee80211_node *ni;
	int gen = 0;

	IEEE80211_NODE_ITERATE_LOCK(nt);
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
		vap = ni->ni_vap;
		/*
		 * Only process stations when in RUN state.  This
		 * insures, for example, that we don't timeout an
		 * inactive station during CAC.  Note that CSA state
		 * is actually handled in ieee80211_node_timeout as
		 * it applies to more than timeout processing.
		 */
		if (vap->iv_state != IEEE80211_S_RUN)
			continue;
		/* XXX can vap be NULL? */
		if ((vap->iv_opmode == IEEE80211_M_HOSTAP ||
		     vap->iv_opmode == IEEE80211_M_STA) &&
		    (ni->ni_flags & IEEE80211_NODE_AREF) == 0)
			continue;
		/*
		 * Free fragment if not needed anymore
		 * (last fragment older than 1s).
		 * XXX doesn't belong here, move to node_age
		 */
		if (ni->ni_rxfrag[0] != NULL &&
		    ticks > ni->ni_rxfragstamp + hz) {
			m_freem(ni->ni_rxfrag[0]);
			ni->ni_rxfrag[0] = NULL;
		}
		if (ni->ni_inact > 0) {
			ni->ni_inact--;
			IEEE80211_NOTE(vap, IEEE80211_MSG_INACT, ni,
			    "%s: inact %u inact_reload %u nrates %u",
			    __func__, ni->ni_inact, ni->ni_inact_reload,
			    ni->ni_rates.rs_nrates);
		}
		/*
		 * Special case ourself; we may be idle for extended periods
		 * of time and regardless reclaiming our state is wrong.
		 * XXX run ic_node_age
		 */
		if (ni == vap->iv_bss)
			continue;
		if (ni->ni_associd != 0 || 
		    (vap->iv_opmode == IEEE80211_M_IBSS ||
		     vap->iv_opmode == IEEE80211_M_AHDEMO)) {
			/*
			 * Age/drain resources held by the station.
			 */
			ic->ic_node_age(ni);
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
			if ((vap->iv_flags_ext & IEEE80211_FEXT_INACT) &&
			    (0 < ni->ni_inact &&
			     ni->ni_inact <= vap->iv_inact_probe) &&
			    ni->ni_rates.rs_nrates != 0) {
				IEEE80211_NOTE(vap,
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
		if ((vap->iv_flags_ext & IEEE80211_FEXT_INACT) &&
		    ni->ni_inact <= 0) {
			IEEE80211_NOTE(vap,
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
			ieee80211_ref_node(ni);
			IEEE80211_NODE_UNLOCK(nt);
			if (ni->ni_associd != 0) {
				IEEE80211_SEND_MGMT(ni,
				    IEEE80211_FC0_SUBTYPE_DEAUTH,
				    IEEE80211_REASON_AUTH_EXPIRE);
			}
			ieee80211_node_leave(ni);
			ieee80211_free_node(ni);
			vap->iv_stats.is_node_timeout++;
			goto restart;
		}
	}
	IEEE80211_NODE_UNLOCK(nt);

	IEEE80211_NODE_ITERATE_UNLOCK(nt);
}

/*
 * Aggressively reclaim resources.  This should be used
 * only in a critical situation to reclaim mbuf resources.
 */
void
ieee80211_drain(struct ieee80211com *ic)
{
	struct ieee80211_node_table *nt = &ic->ic_sta;
	struct ieee80211vap *vap;
	struct ieee80211_node *ni;

	IEEE80211_NODE_LOCK(nt);
	TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
		/*
		 * Ignore entries for which have yet to receive an
		 * authentication frame.  These are transient and
		 * will be reclaimed when the last reference to them
		 * goes away (when frame xmits complete).
		 */
		vap = ni->ni_vap;
		/*
		 * Only process stations when in RUN state.  This
		 * insures, for example, that we don't timeout an
		 * inactive station during CAC.  Note that CSA state
		 * is actually handled in ieee80211_node_timeout as
		 * it applies to more than timeout processing.
		 */
		if (vap->iv_state != IEEE80211_S_RUN)
			continue;
		/* XXX can vap be NULL? */
		if ((vap->iv_opmode == IEEE80211_M_HOSTAP ||
		     vap->iv_opmode == IEEE80211_M_STA) &&
		    (ni->ni_flags & IEEE80211_NODE_AREF) == 0)
			continue;
		/*
		 * Free fragments.
		 * XXX doesn't belong here, move to node_drain
		 */
		if (ni->ni_rxfrag[0] != NULL) {
			m_freem(ni->ni_rxfrag[0]);
			ni->ni_rxfrag[0] = NULL;
		}
		/*
		 * Drain resources held by the station.
		 */
		ic->ic_node_drain(ni);
	}
	IEEE80211_NODE_UNLOCK(nt);
}

/*
 * Per-ieee80211com inactivity timer callback.
 */
void
ieee80211_node_timeout(void *arg)
{
	struct ieee80211com *ic = arg;

	/*
	 * Defer timeout processing if a channel switch is pending.
	 * We typically need to be mute so not doing things that
	 * might generate frames is good to handle in one place.
	 * Supressing the station timeout processing may extend the
	 * lifetime of inactive stations (by not decrementing their
	 * idle counters) but this should be ok unless the CSA is
	 * active for an unusually long time.
	 */
	if ((ic->ic_flags & IEEE80211_F_CSAPENDING) == 0) {
		ieee80211_scan_timeout(ic);
		ieee80211_timeout_stations(ic);

		IEEE80211_LOCK(ic);
		ieee80211_erp_timeout(ic);
		ieee80211_ht_timeout(ic);
		IEEE80211_UNLOCK(ic);
	}
	callout_reset(&ic->ic_inact, IEEE80211_INACT_WAIT*hz,
		ieee80211_node_timeout, ic);
}

void
ieee80211_iterate_nodes(struct ieee80211_node_table *nt,
	ieee80211_iter_func *f, void *arg)
{
	struct ieee80211_node *ni;
	u_int gen;

	IEEE80211_NODE_ITERATE_LOCK(nt);
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

	IEEE80211_NODE_ITERATE_UNLOCK(nt);
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
		ni->ni_rstamp, node_getrssi(ni), ni->ni_noise,
		ni->ni_intval, ni->ni_capinfo);
	printf("\tbssid %s essid \"%.*s\" channel %u:0x%x\n",
		ether_sprintf(ni->ni_bssid),
		ni->ni_esslen, ni->ni_essid,
		ni->ni_chan->ic_freq, ni->ni_chan->ic_flags);
	printf("\tinact %u inact_reload %u txrate %u\n",
		ni->ni_inact, ni->ni_inact_reload, ni->ni_txrate);
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

static void
ieee80211_notify_erp_locked(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;

	IEEE80211_LOCK_ASSERT(ic);

	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next)
		if (vap->iv_opmode == IEEE80211_M_HOSTAP)
			ieee80211_beacon_notify(vap, IEEE80211_BEACON_ERP);
}

void
ieee80211_notify_erp(struct ieee80211com *ic)
{
	IEEE80211_LOCK(ic);
	ieee80211_notify_erp_locked(ic);
	IEEE80211_UNLOCK(ic);
}

/*
 * Handle a station joining an 11g network.
 */
static void
ieee80211_node_join_11g(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;

	IEEE80211_LOCK_ASSERT(ic);

	/*
	 * Station isn't capable of short slot time.  Bump
	 * the count of long slot time stations and disable
	 * use of short slot time.  Note that the actual switch
	 * over to long slot time use may not occur until the
	 * next beacon transmission (per sec. 7.3.1.4 of 11g).
	 */
	if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME) == 0) {
		ic->ic_longslotsta++;
		IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_ASSOC, ni,
		    "station needs long slot time, count %d",
		    ic->ic_longslotsta);
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
	if (!ieee80211_iserp_rateset(&ni->ni_rates)) {
		ic->ic_nonerpsta++;
		IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_ASSOC, ni,
		    "station is !ERP, %d non-ERP stations associated",
		    ic->ic_nonerpsta);
		/*
		 * If station does not support short preamble
		 * then we must enable use of Barker preamble.
		 */
		if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE) == 0) {
			IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_ASSOC, ni,
			    "%s", "station needs long preamble");
			ic->ic_flags |= IEEE80211_F_USEBARKER;
			ic->ic_flags &= ~IEEE80211_F_SHPREAMBLE;
		}
		/*
		 * If protection is configured and this is the first
		 * indication we should use protection, enable it.
		 */
		if (ic->ic_protmode != IEEE80211_PROT_NONE &&
		    ic->ic_nonerpsta == 1 &&
		    (ic->ic_flags_ext & IEEE80211_FEXT_NONERP_PR) == 0) {
			IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_ASSOC,
			    "%s: enable use of protection\n", __func__);
			ic->ic_flags |= IEEE80211_F_USEPROT;
			ieee80211_notify_erp_locked(ic);
		}
	} else
		ni->ni_flags |= IEEE80211_NODE_ERP;
}

void
ieee80211_node_join(struct ieee80211_node *ni, int resp)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211vap *vap = ni->ni_vap;
	int newassoc;

	if (ni->ni_associd == 0) {
		uint16_t aid;

		KASSERT(vap->iv_aid_bitmap != NULL, ("no aid bitmap"));
		/*
		 * It would be good to search the bitmap
		 * more efficiently, but this will do for now.
		 */
		for (aid = 1; aid < vap->iv_max_aid; aid++) {
			if (!IEEE80211_AID_ISSET(vap, aid))
				break;
		}
		if (aid >= vap->iv_max_aid) {
			IEEE80211_SEND_MGMT(ni, resp, IEEE80211_STATUS_TOOMANY);
			ieee80211_node_leave(ni);
			return;
		}
		ni->ni_associd = aid | 0xc000;
		ni->ni_jointime = time_uptime;
		IEEE80211_LOCK(ic);
		IEEE80211_AID_SET(vap, ni->ni_associd);
		vap->iv_sta_assoc++;
		ic->ic_sta_assoc++;

		if (IEEE80211_IS_CHAN_HT(ic->ic_bsschan))
			ieee80211_ht_node_join(ni);
		if (IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan) &&
		    IEEE80211_IS_CHAN_FULL(ic->ic_bsschan))
			ieee80211_node_join_11g(ni);
		IEEE80211_UNLOCK(ic);

		newassoc = 1;
	} else
		newassoc = 0;

	IEEE80211_NOTE(vap, IEEE80211_MSG_ASSOC | IEEE80211_MSG_DEBUG, ni,
	    "station associated at aid %d: %s preamble, %s slot time%s%s%s%s%s%s%s%s",
	    IEEE80211_NODE_AID(ni),
	    ic->ic_flags & IEEE80211_F_SHPREAMBLE ? "short" : "long",
	    ic->ic_flags & IEEE80211_F_SHSLOT ? "short" : "long",
	    ic->ic_flags & IEEE80211_F_USEPROT ? ", protection" : "",
	    ni->ni_flags & IEEE80211_NODE_QOS ? ", QoS" : "",
	    ni->ni_flags & IEEE80211_NODE_HT ?
		(ni->ni_chw == 40 ? ", HT40" : ", HT20") : "",
	    ni->ni_flags & IEEE80211_NODE_AMPDU ? " (+AMPDU)" : "",
	    ni->ni_flags & IEEE80211_NODE_MIMO_RTS ? " (+SMPS-DYN)" :
	        ni->ni_flags & IEEE80211_NODE_MIMO_PS ? " (+SMPS)" : "",
	    ni->ni_flags & IEEE80211_NODE_RIFS ? " (+RIFS)" : "",
	    IEEE80211_ATH_CAP(vap, ni, IEEE80211_NODE_FF) ?
		", fast-frames" : "",
	    IEEE80211_ATH_CAP(vap, ni, IEEE80211_NODE_TURBOP) ?
		", turbo" : ""
	);

	node_setuptxparms(ni);
	/* give driver a chance to setup state like ni_txrate */
	if (ic->ic_newassoc != NULL)
		ic->ic_newassoc(ni, newassoc);
	IEEE80211_SEND_MGMT(ni, resp, IEEE80211_STATUS_SUCCESS);
	/* tell the authenticator about new station */
	if (vap->iv_auth->ia_node_join != NULL)
		vap->iv_auth->ia_node_join(ni);
	ieee80211_notify_node_join(ni,
	    resp == IEEE80211_FC0_SUBTYPE_ASSOC_RESP);
}

static void
disable_protection(struct ieee80211com *ic)
{
	KASSERT(ic->ic_nonerpsta == 0 &&
	    (ic->ic_flags_ext & IEEE80211_FEXT_NONERP_PR) == 0,
	   ("%d non ERP stations, flags 0x%x", ic->ic_nonerpsta,
	   ic->ic_flags_ext));

	ic->ic_flags &= ~IEEE80211_F_USEPROT;
	/* XXX verify mode? */
	if (ic->ic_caps & IEEE80211_C_SHPREAMBLE) {
		ic->ic_flags |= IEEE80211_F_SHPREAMBLE;
		ic->ic_flags &= ~IEEE80211_F_USEBARKER;
	}
	ieee80211_notify_erp_locked(ic);
}

/*
 * Handle a station leaving an 11g network.
 */
static void
ieee80211_node_leave_11g(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;

	IEEE80211_LOCK_ASSERT(ic);

	KASSERT(IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan),
	     ("not in 11g, bss %u:0x%x", ic->ic_bsschan->ic_freq,
	      ic->ic_bsschan->ic_flags));

	/*
	 * If a long slot station do the slot time bookkeeping.
	 */
	if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME) == 0) {
		KASSERT(ic->ic_longslotsta > 0,
		    ("bogus long slot station count %d", ic->ic_longslotsta));
		ic->ic_longslotsta--;
		IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_ASSOC, ni,
		    "long slot time station leaves, count now %d",
		    ic->ic_longslotsta);
		if (ic->ic_longslotsta == 0) {
			/*
			 * Re-enable use of short slot time if supported
			 * and not operating in IBSS mode (per spec).
			 */
			if ((ic->ic_caps & IEEE80211_C_SHSLOT) &&
			    ic->ic_opmode != IEEE80211_M_IBSS) {
				IEEE80211_DPRINTF(ni->ni_vap,
				    IEEE80211_MSG_ASSOC,
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
		IEEE80211_NOTE(ni->ni_vap, IEEE80211_MSG_ASSOC, ni,
		    "non-ERP station leaves, count now %d%s", ic->ic_nonerpsta,
		    (ic->ic_flags_ext & IEEE80211_FEXT_NONERP_PR) ?
			" (non-ERP sta present)" : "");
		if (ic->ic_nonerpsta == 0 &&
		    (ic->ic_flags_ext & IEEE80211_FEXT_NONERP_PR) == 0) {
			IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_ASSOC,
				"%s: disable use of protection\n", __func__);
			disable_protection(ic);
		}
	}
}

/*
 * Time out presence of an overlapping bss with non-ERP
 * stations.  When operating in hostap mode we listen for
 * beacons from other stations and if we identify a non-ERP
 * station is present we enable protection.  To identify
 * when all non-ERP stations are gone we time out this
 * condition.
 */
static void
ieee80211_erp_timeout(struct ieee80211com *ic)
{

	IEEE80211_LOCK_ASSERT(ic);

	if ((ic->ic_flags_ext & IEEE80211_FEXT_NONERP_PR) &&
	    time_after(ticks, ic->ic_lastnonerp + IEEE80211_NONERP_PRESENT_AGE)) {
#if 0
		IEEE80211_NOTE(vap, IEEE80211_MSG_ASSOC, ni,
		    "%s", "age out non-ERP sta present on channel");
#endif
		ic->ic_flags_ext &= ~IEEE80211_FEXT_NONERP_PR;
		if (ic->ic_nonerpsta == 0)
			disable_protection(ic);
	}
}

/*
 * Handle bookkeeping for station deauthentication/disassociation
 * when operating as an ap.
 */
void
ieee80211_node_leave(struct ieee80211_node *ni)
{
	struct ieee80211com *ic = ni->ni_ic;
	struct ieee80211vap *vap = ni->ni_vap;
	struct ieee80211_node_table *nt = ni->ni_table;

	IEEE80211_NOTE(vap, IEEE80211_MSG_ASSOC | IEEE80211_MSG_DEBUG, ni,
	    "station with aid %d leaves", IEEE80211_NODE_AID(ni));

	KASSERT(vap->iv_opmode != IEEE80211_M_STA,
		("unexpected operating mode %u", vap->iv_opmode));
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
	if (vap->iv_auth->ia_node_leave != NULL)
		vap->iv_auth->ia_node_leave(ni);

	IEEE80211_LOCK(ic);
	IEEE80211_AID_CLR(vap, ni->ni_associd);
	ni->ni_associd = 0;
	vap->iv_sta_assoc--;
	ic->ic_sta_assoc--;

	if (IEEE80211_IS_CHAN_HT(ic->ic_bsschan))
		ieee80211_ht_node_leave(ni);
	if (IEEE80211_IS_CHAN_ANYG(ic->ic_bsschan) &&
	    IEEE80211_IS_CHAN_FULL(ic->ic_bsschan))
		ieee80211_node_leave_11g(ni);
	IEEE80211_UNLOCK(ic);
	/*
	 * Cleanup station state.  In particular clear various
	 * state that might otherwise be reused if the node
	 * is reused before the reference count goes to zero
	 * (and memory is reclaimed).
	 */
	ieee80211_sta_leave(ni);
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

struct rssiinfo {
	struct ieee80211vap *vap;
	int	rssi_samples;
	uint32_t rssi_total;
};

static void
get_hostap_rssi(void *arg, struct ieee80211_node *ni)
{
	struct rssiinfo *info = arg;
	struct ieee80211vap *vap = ni->ni_vap;
	int8_t rssi;

	if (info->vap != vap)
		return;
	/* only associated stations */
	if (ni->ni_associd == 0)
		return;
	rssi = vap->iv_ic->ic_node_getrssi(ni);
	if (rssi != 0) {
		info->rssi_samples++;
		info->rssi_total += rssi;
	}
}

static void
get_adhoc_rssi(void *arg, struct ieee80211_node *ni)
{
	struct rssiinfo *info = arg;
	struct ieee80211vap *vap = ni->ni_vap;
	int8_t rssi;

	if (info->vap != vap)
		return;
	/* only neighbors */
	/* XXX check bssid */
	if ((ni->ni_capinfo & IEEE80211_CAPINFO_IBSS) == 0)
		return;
	rssi = vap->iv_ic->ic_node_getrssi(ni);
	if (rssi != 0) {
		info->rssi_samples++;
		info->rssi_total += rssi;
	}
}

int8_t
ieee80211_getrssi(struct ieee80211vap *vap)
{
#define	NZ(x)	((x) == 0 ? 1 : (x))
	struct ieee80211com *ic = vap->iv_ic;
	struct rssiinfo info;

	info.rssi_total = 0;
	info.rssi_samples = 0;
	info.vap = vap;
	switch (vap->iv_opmode) {
	case IEEE80211_M_IBSS:		/* average of all ibss neighbors */
	case IEEE80211_M_AHDEMO:	/* average of all neighbors */
		ieee80211_iterate_nodes(&ic->ic_sta, get_adhoc_rssi, &info);
		break;
	case IEEE80211_M_HOSTAP:	/* average of all associated stations */
		ieee80211_iterate_nodes(&ic->ic_sta, get_hostap_rssi, &info);
		break;
	case IEEE80211_M_MONITOR:	/* XXX */
	case IEEE80211_M_STA:		/* use stats from associated ap */
	default:
		if (vap->iv_bss != NULL)
			info.rssi_total = ic->ic_node_getrssi(vap->iv_bss);
		info.rssi_samples = 1;
		break;
	}
	return info.rssi_total / NZ(info.rssi_samples);
#undef NZ
}

void
ieee80211_getsignal(struct ieee80211vap *vap, int8_t *rssi, int8_t *noise)
{

	if (vap->iv_bss == NULL)		/* NB: shouldn't happen */
		return;
	vap->iv_ic->ic_node_getsignal(vap->iv_bss, rssi, noise);
	/* for non-station mode return avg'd rssi accounting */
	if (vap->iv_opmode != IEEE80211_M_STA)
		*rssi = ieee80211_getrssi(vap);
}
