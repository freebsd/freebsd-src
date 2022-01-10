/*-
 * Copyright (c) 2020-2021 The FreeBSD Foundation
 * Copyright (c) 2020-2022 Bjoern A. Zeeb
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Public functions are called linuxkpi_*().
 * Internal (static) functions are called lkpi_*().
 *
 * The internal structures holding metadata over public structures are also
 * called lkpi_xxx (usually with a member at the end called xxx).
 * Note: we do not replicate the structure names but the general variable names
 * for these (e.g., struct hw -> struct lkpi_hw, struct sta -> struct lkpi_sta).
 * There are macros to access one from the other.
 * We call the internal versions lxxx (e.g., hw -> lhw, sta -> lsta).
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_proto.h>
#include <net80211/ieee80211_ratectl.h>
#include <net80211/ieee80211_radiotap.h>

#define	LINUXKPI_NET80211
#include <net/mac80211.h>

#include <linux/workqueue.h>
#include "linux_80211.h"

static MALLOC_DEFINE(M_LKPI80211, "lkpi80211", "Linux KPI 80211 compat");

/* -------------------------------------------------------------------------- */
/* These are unrelated to 802.11 sysctl bug debugging during 802.11 work so   *
 * keep them here rather than in a more general file.                         */

int debug_skb;
SYSCTL_INT(_compat_linuxkpi, OID_AUTO, debug_skb, CTLFLAG_RWTUN,
    &debug_skb, 0, "SKB debug level");

/* -------------------------------------------------------------------------- */

int debug_80211;
SYSCTL_INT(_compat_linuxkpi, OID_AUTO, debug_80211, CTLFLAG_RWTUN,
    &debug_80211, 0, "80211 debug Level");

#define LINUXKPI_DEBUG_80211
#ifdef LINUXKPI_DEBUG_80211
#ifndef	D80211_TODO
#define	D80211_TODO		0x1
#endif
#ifndef D80211_IMPROVE
#define	D80211_IMPROVE		0x2
#endif
#define	D80211_TRACE		0x10
#define	D80211_TRACEOK		0x20
#define	D80211_TRACE_TX		0x100
#define	D80211_TRACE_TX_DUMP	0x200
#define	D80211_TRACE_RX		0x1000
#define	D80211_TRACE_RX_DUMP	0x2000
#define	D80211_TRACE_RX_BEACONS	0x4000
#define	D80211_TRACEX		(D80211_TRACE_TX|D80211_TRACE_RX)
#define	D80211_TRACEX_DUMP	(D80211_TRACE_TX_DUMP|D80211_TRACE_RX_DUMP)
#define	UNIMPLEMENTED		if (debug_80211 & D80211_TODO)		\
    printf("XXX-TODO %s:%d: UNIMPLEMENTED\n", __func__, __LINE__)
#define	TRACEOK()		if (debug_80211 & D80211_TRACEOK)	\
    printf("XXX-TODO %s:%d: TRACEPOINT\n", __func__, __LINE__)
#else
#define	UNIMPLEMENTED		do { } while (0)
#define	TRACEOK()		do { } while (0)
#endif

/* #define	PREP_TX_INFO_DURATION	(IEEE80211_TRANS_WAIT * 1000) */
#ifndef PREP_TX_INFO_DURATION
#define	PREP_TX_INFO_DURATION	0 /* Let the driver do its thing. */
#endif

/* This is DSAP | SSAP | CTRL | ProtoID/OrgCode{3}. */
const uint8_t rfc1042_header[6] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };

const struct cfg80211_ops linuxkpi_mac80211cfgops = {
	/*
	 * XXX TODO need a "glue layer" to link cfg80211 ops to
	 * mac80211 and to the driver or net80211.
	 * Can we pass some on 1:1? Need to compare the (*f)().
	 */
};

static struct lkpi_sta *lkpi_find_lsta_by_ni(struct lkpi_vif *,
    struct ieee80211_node *);
static void lkpi_80211_txq_task(void *, int);
static void lkpi_ieee80211_free_skb_mbuf(void *);

static enum nl80211_band
lkpi_net80211_chan_to_nl80211_band(struct ieee80211_channel *c)
{

	if (IEEE80211_IS_CHAN_2GHZ(c))
		return (NL80211_BAND_2GHZ);
	else if (IEEE80211_IS_CHAN_5GHZ(c))
		return (NL80211_BAND_5GHZ);
#ifdef __notyet__
	else if ()
		return (NL80211_BAND_6GHZ);
	else if ()
		return (NL80211_BAND_60GHZ);
	else if (IEEE80211_IS_CHAN_GSM(c))
		return (NL80211_BAND_XXX);
#endif
	else
		panic("%s: unsupported band. c %p flags %#x\n",
		    __func__, c, c->ic_flags);
}

static uint32_t
lkpi_nl80211_band_to_net80211_band(enum nl80211_band band)
{

	/* XXX-BZ this is just silly; net80211 is too convoluted. */
	/* IEEE80211_CHAN_A / _G / .. doesn't really work either. */
	switch (band) {
	case NL80211_BAND_2GHZ:
		return (IEEE80211_CHAN_2GHZ);
		break;
	case NL80211_BAND_5GHZ:
		return (IEEE80211_CHAN_5GHZ);
		break;
	case NL80211_BAND_60GHZ:
		break;
	case NL80211_BAND_6GHZ:
		break;
	default:
		panic("%s: unsupported band %u\n", __func__, band);
		break;
	}

	IMPROVE();
	return (0x00);
}

static enum ieee80211_ac_numbers
lkpi_ac_net_to_l80211(int ac)
{

	switch (ac) {
	case WME_AC_VO:
		return (IEEE80211_AC_VO);
	case WME_AC_VI:
		return (IEEE80211_AC_VI);
	case WME_AC_BE:
		return (IEEE80211_AC_BE);
	case WME_AC_BK:
		return (IEEE80211_AC_BK);
	default:
		printf("%s: invalid WME_AC_* input: ac = %d\n", __func__, ac);
		return (IEEE80211_AC_BE);
	}
}

static enum nl80211_iftype
lkpi_opmode_to_vif_type(enum ieee80211_opmode opmode)
{

	switch (opmode) {
	case IEEE80211_M_IBSS:
		return (NL80211_IFTYPE_ADHOC);
		break;
	case IEEE80211_M_STA:
		return (NL80211_IFTYPE_STATION);
		break;
	case IEEE80211_M_WDS:
		return (NL80211_IFTYPE_WDS);
		break;
	case IEEE80211_M_HOSTAP:
		return (NL80211_IFTYPE_AP);
		break;
	case IEEE80211_M_MONITOR:
		return (NL80211_IFTYPE_MONITOR);
		break;
	case IEEE80211_M_MBSS:
		return (NL80211_IFTYPE_MESH_POINT);
		break;
	case IEEE80211_M_AHDEMO:
		/* FALLTHROUGH */
	default:
		printf("ERROR: %s: unsupported opmode %d\n", __func__, opmode);
		/* FALLTHROUGH */
	}
	return (NL80211_IFTYPE_UNSPECIFIED);
}

#ifdef __notyet__
static uint32_t
lkpi_l80211_to_net80211_cyphers(uint32_t wlan_cipher_suite)
{

	switch (wlan_cipher_suite) {
	case WLAN_CIPHER_SUITE_WEP40:
		return (IEEE80211_CRYPTO_WEP);
	case WLAN_CIPHER_SUITE_TKIP:
		return (IEEE80211_CRYPTO_TKIP);
	case WLAN_CIPHER_SUITE_CCMP:
		return (IEEE80211_CIPHER_AES_CCM);
	case WLAN_CIPHER_SUITE_WEP104:
		return (IEEE80211_CRYPTO_WEP);
	case WLAN_CIPHER_SUITE_AES_CMAC:
	case WLAN_CIPHER_SUITE_GCMP:
	case WLAN_CIPHER_SUITE_GCMP_256:
	case WLAN_CIPHER_SUITE_CCMP_256:
	case WLAN_CIPHER_SUITE_BIP_GMAC_128:
	case WLAN_CIPHER_SUITE_BIP_GMAC_256:
	case WLAN_CIPHER_SUITE_BIP_CMAC_256:
		printf("%s: unsupported WLAN Cipher Suite %#08x | %u\n", __func__,
		    wlan_cipher_suite >> 8, wlan_cipher_suite & 0xff);
		break;
	default:
		printf("%s: unknown WLAN Cipher Suite %#08x | %u\n", __func__,
		    wlan_cipher_suite >> 8, wlan_cipher_suite & 0xff);
	}

	return (0);
}
#endif

#ifdef TRY_HW_CRYPTO
static uint32_t
lkpi_net80211_to_l80211_cipher_suite(uint32_t cipher, uint8_t keylen)
{

	switch (cipher) {
	case IEEE80211_CIPHER_TKIP:
		return (WLAN_CIPHER_SUITE_TKIP);
	case IEEE80211_CIPHER_AES_CCM:
		return (WLAN_CIPHER_SUITE_CCMP);
	case IEEE80211_CIPHER_WEP:
		if (keylen < 8)
			return (WLAN_CIPHER_SUITE_WEP40);
		else
			return (WLAN_CIPHER_SUITE_WEP104);
		break;
	case IEEE80211_CIPHER_AES_OCB:
	case IEEE80211_CIPHER_TKIPMIC:
	case IEEE80211_CIPHER_CKIP:
	case IEEE80211_CIPHER_NONE:
		printf("%s: unsupported cipher %#010x\n", __func__, cipher);
		break;
	default:
		printf("%s: unknown cipher %#010x\n", __func__, cipher);
	};
	return (0);
}
#endif

#ifdef __notyet__
static enum ieee80211_sta_state
lkpi_net80211_state_to_sta_state(enum ieee80211_state state)
{

	/*
	 * XXX-BZ The net80211 states are "try to ..", the lkpi8011 states are
	 * "done".  Also ASSOC/AUTHORIZED are both "RUN" then?
	 */
	switch (state) {
	case IEEE80211_S_INIT:
		return (IEEE80211_STA_NOTEXIST);
	case IEEE80211_S_SCAN:
		return (IEEE80211_STA_NONE);
	case IEEE80211_S_AUTH:
		return (IEEE80211_STA_AUTH);
	case IEEE80211_S_ASSOC:
		return (IEEE80211_STA_ASSOC);
	case IEEE80211_S_RUN:
		return (IEEE80211_STA_AUTHORIZED);
	case IEEE80211_S_CAC:
	case IEEE80211_S_CSA:
	case IEEE80211_S_SLEEP:
	default:
		UNIMPLEMENTED;
	};

	return (IEEE80211_STA_NOTEXIST);
}
#endif

static struct linuxkpi_ieee80211_channel *
lkpi_find_lkpi80211_chan(struct lkpi_hw *lhw,
    struct ieee80211_channel *c)
{
	struct ieee80211_hw *hw;
	struct linuxkpi_ieee80211_channel *channels;
	enum nl80211_band band;
	int i, nchans;

	hw = LHW_TO_HW(lhw);
	band = lkpi_net80211_chan_to_nl80211_band(c);
	if (hw->wiphy->bands[band] == NULL)
		return (NULL);

	nchans = hw->wiphy->bands[band]->n_channels;
	if (nchans <= 0)
		return (NULL);

	channels = hw->wiphy->bands[band]->channels;
	for (i = 0; i < nchans; i++) {
		if (channels[i].hw_value == c->ic_ieee)
			return (&channels[i]);
	}

	return (NULL);
}

static struct linuxkpi_ieee80211_channel *
lkpi_get_lkpi80211_chan(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct linuxkpi_ieee80211_channel *chan;
	struct ieee80211_channel *c;
	struct lkpi_hw *lhw;

	chan = NULL;
	if (ni != NULL && ni->ni_chan != IEEE80211_CHAN_ANYC)
		c = ni->ni_chan;
	else if (ic->ic_bsschan != IEEE80211_CHAN_ANYC)
		c = ic->ic_bsschan;
	else if (ic->ic_curchan != IEEE80211_CHAN_ANYC)
		c = ic->ic_curchan;
	else
		c = NULL;

	if (c != NULL && c != IEEE80211_CHAN_ANYC) {
		lhw = ic->ic_softc;
		chan = lkpi_find_lkpi80211_chan(lhw, c);
	}

	return (chan);
}

#ifdef TRY_HW_CRYPTO
static int
_lkpi_iv_key_set_delete(struct ieee80211vap *vap, const struct ieee80211_key *k,
    enum set_key_cmd cmd)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct ieee80211_sta *sta;
	struct ieee80211_node *ni;
	struct ieee80211_key_conf *kc;
	int error;

	/* XXX TODO Check (k->wk_flags & IEEE80211_KEY_SWENCRYPT) and don't upload to driver/hw? */

	ic = vap->iv_ic;
	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	memset(&kc, 0, sizeof(kc));
	kc = malloc(sizeof(*kc) + k->wk_keylen, M_LKPI80211, M_WAITOK | M_ZERO);
	kc->cipher = lkpi_net80211_to_l80211_cipher_suite(
	    k->wk_cipher->ic_cipher, k->wk_keylen);
	kc->keyidx = k->wk_keyix;
#if 0
	kc->hw_key_idx = /* set by hw and needs to be passed for TX */;
#endif
	atomic64_set(&kc->tx_pn, k->wk_keytsc);
	kc->keylen = k->wk_keylen;
	memcpy(kc->key, k->wk_key, k->wk_keylen);

	switch (kc->cipher) {
	case WLAN_CIPHER_SUITE_CCMP:
		kc->iv_len = k->wk_cipher->ic_header;
		kc->icv_len = k->wk_cipher->ic_trailer;
		break;
	case WLAN_CIPHER_SUITE_TKIP:
	default:
		IMPROVE();
		return (0);
	};

	ni = vap->iv_bss;
	sta = ieee80211_find_sta(vif, ni->ni_bssid);
	if (sta != NULL) {
		struct lkpi_sta *lsta;

		lsta = STA_TO_LSTA(sta);
		lsta->kc = kc;
	}

	error = lkpi_80211_mo_set_key(hw, cmd, vif, sta, kc);
	if (error != 0) {
		/* XXX-BZ leaking kc currently */
		ic_printf(ic, "%s: set_key failed: %d\n", __func__, error);
		return (0);
	} else {
		ic_printf(ic, "%s: set_key succeeded: keyidx %u hw_key_idx %u "
		    "flags %#10x\n", __func__,
		    kc->keyidx, kc->hw_key_idx, kc->flags);
		return (1);
	}
}

static int
lkpi_iv_key_delete(struct ieee80211vap *vap, const struct ieee80211_key *k)
{

	/* XXX-BZ one day we should replace this iterating over VIFs, or node list? */
	return (_lkpi_iv_key_set_delete(vap, k, DISABLE_KEY));
}
static  int
lkpi_iv_key_set(struct ieee80211vap *vap, const struct ieee80211_key *k)
{

	return (_lkpi_iv_key_set_delete(vap, k, SET_KEY));
}
#endif

static u_int
lkpi_ic_update_mcast_copy(void *arg, struct sockaddr_dl *sdl, u_int cnt)
{
	struct netdev_hw_addr_list *mc_list;
	struct netdev_hw_addr *addr;

	KASSERT(arg != NULL && sdl != NULL, ("%s: arg %p sdl %p cnt %u\n",
	    __func__, arg, sdl, cnt));

	mc_list = arg;
	/* If it is on the list already skip it. */
	netdev_hw_addr_list_for_each(addr, mc_list) {
		if (!memcmp(addr->addr, LLADDR(sdl), sdl->sdl_alen))
			return (0);
	}

	addr = malloc(sizeof(*addr), M_LKPI80211, M_NOWAIT | M_ZERO);
	if (addr == NULL)
		return (0);

	INIT_LIST_HEAD(&addr->addr_list);
	memcpy(addr->addr, LLADDR(sdl), sdl->sdl_alen);
	/* XXX this should be a netdev function? */
	list_add(&addr->addr_list, &mc_list->addr_list);
	mc_list->count++;

	if (debug_80211 & D80211_TRACE)
		printf("%s:%d: mc_list count %d: added %6D\n",
		    __func__, __LINE__, mc_list->count, addr->addr, ":");

	return (1);
}

static void
lkpi_update_mcast_filter(struct ieee80211com *ic, bool force)
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct netdev_hw_addr_list mc_list;
	struct list_head *le, *next;
	struct netdev_hw_addr *addr;
	struct ieee80211vap *vap;
	u64 mc;
	unsigned int changed_flags, total_flags;

	lhw = ic->ic_softc;

	if (lhw->ops->prepare_multicast == NULL ||
	    lhw->ops->configure_filter == NULL)
		return;

	if (!lhw->update_mc && !force)
		return;

	changed_flags = total_flags = 0;
	mc_list.count = 0;
	INIT_LIST_HEAD(&mc_list.addr_list);
	if (ic->ic_allmulti == 0) {
		TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next)
			if_foreach_llmaddr(vap->iv_ifp,
			    lkpi_ic_update_mcast_copy, &mc_list);
	} else {
		changed_flags |= FIF_ALLMULTI;
	}

	hw = LHW_TO_HW(lhw);
	mc = lkpi_80211_mo_prepare_multicast(hw, &mc_list);
	/*
	 * XXX-BZ make sure to get this sorted what is a change,
	 * what gets all set; what was already set?
	 */
	total_flags = changed_flags;
	lkpi_80211_mo_configure_filter(hw, changed_flags, &total_flags, mc);

	if (debug_80211 & D80211_TRACE)
		printf("%s: changed_flags %#06x count %d total_flags %#010x\n",
		    __func__, changed_flags, mc_list.count, total_flags);

	if (mc_list.count != 0) {
		list_for_each_safe(le, next, &mc_list.addr_list) {
			addr = list_entry(le, struct netdev_hw_addr, addr_list);
			free(addr, M_LKPI80211);
			mc_list.count--;
		}
	}
	KASSERT(mc_list.count == 0, ("%s: mc_list %p count %d != 0\n",
	    __func__, &mc_list, mc_list.count));
}

const uint8_t tid_to_mac80211_ac[] = {
	IEEE80211_AC_BE,
	IEEE80211_AC_BK,
	IEEE80211_AC_BK,
	IEEE80211_AC_BE,
	IEEE80211_AC_VI,
	IEEE80211_AC_VI,
	IEEE80211_AC_VO,
	IEEE80211_AC_VO,
#if 0
	IEEE80211_AC_VO, /* We treat MGMT as TID 8, which is set as AC_VO */
#endif
};

static void
lkpi_stop_hw_scan(struct lkpi_hw *lhw, struct ieee80211_vif *vif)
{
	struct ieee80211_hw *hw;
	int error;

	if ((lhw->scan_flags & LKPI_SCAN_RUNNING) == 0)
		return;

	hw = LHW_TO_HW(lhw);

	IEEE80211_UNLOCK(lhw->ic);
	LKPI_80211_LHW_LOCK(lhw);
	/* Need to cancel the scan. */
	lkpi_80211_mo_cancel_hw_scan(hw, vif);

	/* Need to make sure we see ieee80211_scan_completed. */
	error = msleep(lhw, &lhw->mtx, 0, "lhwscanstop", hz/2);
	LKPI_80211_LHW_UNLOCK(lhw);
	IEEE80211_LOCK(lhw->ic);

	if ((lhw->scan_flags & LKPI_SCAN_RUNNING) != 0)
		ic_printf(lhw->ic, "%s: failed to cancel scan: %d (%p, %p)\n",
		    __func__, error, lhw, vif);
}

static void
lkpi_hw_conf_idle(struct ieee80211_hw *hw, bool new)
{
	struct lkpi_hw *lhw;
	int error;
	bool old;

	old = hw->conf.flags & IEEE80211_CONF_IDLE;
	if (old == new)
		return;

	hw->conf.flags ^= IEEE80211_CONF_IDLE;
	error = lkpi_80211_mo_config(hw, IEEE80211_CONF_CHANGE_IDLE);
	if (error != 0 && error != EOPNOTSUPP) {
		lhw = HW_TO_LHW(hw);
		ic_printf(lhw->ic, "ERROR: %s: config %#0x returned %d\n",
		    __func__, IEEE80211_CONF_CHANGE_IDLE, error);
	}
}

static void
lkpi_disassoc(struct ieee80211_sta *sta, struct ieee80211_vif *vif,
    struct lkpi_hw *lhw)
{
	sta->aid = 0;
	if (vif->bss_conf.assoc) {
		struct ieee80211_hw *hw;
		enum ieee80211_bss_changed changed;

		lhw->update_mc = true;
		lkpi_update_mcast_filter(lhw->ic, true);

		changed = 0;
		vif->bss_conf.assoc = false;
		vif->bss_conf.aid = 0;
		changed |= BSS_CHANGED_ASSOC;
		IMPROVE();
		hw = LHW_TO_HW(lhw);
		lkpi_80211_mo_bss_info_changed(hw, vif, &vif->bss_conf,
		    changed);

		lkpi_hw_conf_idle(hw, true);
	}
}

static void
lkpi_wake_tx_queues(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
    bool dequeue_seen, bool no_emptyq)
{
	struct lkpi_txq *ltxq;
	int tid;

	/* Wake up all queues to know they are allocated in the driver. */
	for (tid = 0; tid < nitems(sta->txq); tid++) {

			if (tid == IEEE80211_NUM_TIDS) {
				IMPROVE("station specific?");
				if (!ieee80211_hw_check(hw, STA_MMPDU_TXQ))
					continue;
			} else if (tid >= hw->queues)
				continue;

			if (sta->txq[tid] == NULL)
				continue;

			ltxq = TXQ_TO_LTXQ(sta->txq[tid]);
			if (dequeue_seen && !ltxq->seen_dequeue)
				continue;

			if (no_emptyq && skb_queue_empty(&ltxq->skbq))
				continue;

			lkpi_80211_mo_wake_tx_queue(hw, sta->txq[tid]);
	}
}

/* -------------------------------------------------------------------------- */

static int
lkpi_sta_state_do_nada(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{

	return (0);
}

/* lkpi_iv_newstate() handles the stop scan case generally. */
#define	lkpi_sta_scan_to_init(_v, _n, _a)	lkpi_sta_state_do_nada(_v, _n, _a)

static int
lkpi_sta_scan_to_auth(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct linuxkpi_ieee80211_channel *chan;
	struct ieee80211_chanctx_conf *conf;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct ieee80211_node *ni;
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;
	enum ieee80211_bss_changed bss_changed;
	struct ieee80211_prep_tx_info prep_tx_info;
	uint32_t changed;
	int error;

	chan = lkpi_get_lkpi80211_chan(vap->iv_ic, vap->iv_bss);
	if (chan == NULL) {
		ic_printf(vap->iv_ic, "%s: failed to get channel\n", __func__);
		return (ESRCH);
	}

	lhw = vap->iv_ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	IEEE80211_UNLOCK(vap->iv_ic);

	/* Add chanctx (or if exists, change it). */
	if (vif->chanctx_conf != NULL) {
		conf = vif->chanctx_conf;
		IMPROVE("diff changes for changed, working on live copy, rcu");
	} else {
		/* Keep separate alloc as in Linux this is rcu managed? */
		conf = malloc(sizeof(*conf) + hw->chanctx_data_size,
		    M_LKPI80211, M_WAITOK | M_ZERO);
	}

	conf->rx_chains_dynamic = 1;
	conf->rx_chains_static = 1;
	conf->radar_enabled =
	    (chan->flags & IEEE80211_CHAN_RADAR) ? true : false;
	conf->def.chan = chan;
	conf->def.width = NL80211_CHAN_WIDTH_20_NOHT;
	conf->def.center_freq1 = chan->center_freq;
	conf->def.center_freq2 = 0;
	/* Responder ... */
	conf->min_def.chan = chan;
	conf->min_def.width = NL80211_CHAN_WIDTH_20_NOHT;
	conf->min_def.center_freq1 = chan->center_freq;
	conf->min_def.center_freq2 = 0;
	IMPROVE("currently 20_NOHT only");

	ni = NULL;
	error = 0;
	if (vif->chanctx_conf != NULL) {
		changed = IEEE80211_CHANCTX_CHANGE_MIN_WIDTH;
		changed |= IEEE80211_CHANCTX_CHANGE_RADAR;
		changed |= IEEE80211_CHANCTX_CHANGE_RX_CHAINS;
		changed |= IEEE80211_CHANCTX_CHANGE_WIDTH;
		lkpi_80211_mo_change_chanctx(hw, conf, changed);
	} else {
		error = lkpi_80211_mo_add_chanctx(hw, conf);
		if (error == 0 || error == EOPNOTSUPP) {
			vif->bss_conf.chandef.chan = conf->def.chan;
			vif->bss_conf.chandef.width = conf->def.width;
			vif->bss_conf.chandef.center_freq1 =
			    conf->def.center_freq1;
			vif->bss_conf.chandef.center_freq2 =
			    conf->def.center_freq2;
		} else {
			goto out;
		}
		/* Assign vif chanctx. */
		if (error == 0)
			error = lkpi_80211_mo_assign_vif_chanctx(hw, vif, conf);
		if (error == EOPNOTSUPP)
			error = 0;
		if (error != 0) {
			lkpi_80211_mo_remove_chanctx(hw, conf);
			free(conf, M_LKPI80211);
			goto out;
		}
	}
	IMPROVE("update radiotap chan fields too");

	ni = ieee80211_ref_node(vap->iv_bss);

	/* Set bss info (bss_info_changed). */
	bss_changed = 0;
	IEEE80211_ADDR_COPY(vif->bss_conf.bssid, ni->ni_bssid);
	bss_changed |= BSS_CHANGED_BSSID;
	vif->bss_conf.txpower = ni->ni_txpower;
	bss_changed |= BSS_CHANGED_TXPOWER;
	vif->bss_conf.idle = false;
	bss_changed |= BSS_CHANGED_IDLE;
	vif->bss_conf.beacon_int = ni->ni_intval;
	/* iwlwifi FW bug workaround; iwl_mvm_mac_sta_state. */
	if (vif->bss_conf.beacon_int < 16)
		vif->bss_conf.beacon_int = 16;
	bss_changed |= BSS_CHANGED_BEACON_INT;
	/* Should almost assert it is this. */
	vif->bss_conf.assoc = false;
	vif->bss_conf.aid = 0;
	/* RATES */
	IMPROVE("bss info: not all needs to come now and rates are missing");
	lkpi_80211_mo_bss_info_changed(hw, vif, &vif->bss_conf, bss_changed);

	/* Add (or adjust) sta and change state (from NOTEXIST) to NONE. */
	lsta = ni->ni_drv_data;
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_NOTEXIST, ("%s: lsta %p state not "
	    "NOTEXIST: %#x\n", __func__, lsta, lsta->state));
	sta = LSTA_TO_STA(lsta);
	error = lkpi_80211_mo_sta_state(hw, vif, sta, IEEE80211_STA_NONE);
	if (error != 0) {
		IMPROVE("do we need to undo the chan ctx?");
		goto out;
	}
#if 0
	lsta->added_to_drv = true;	/* mo manages. */
#endif

	/*
	 * Wakeup all queues now that sta is there so we have as much time to
	 * possibly prepare the queue in the driver to be ready for the 1st
	 * packet;  lkpi_80211_txq_tx_one() still has a workaround as there
	 * is no guarantee or way to check.
	 */
	lkpi_wake_tx_queues(hw, sta, false, false);

	{
		int i, count;

		for (i = 3 * (hw->queues + 1); i > 0; i--) {
			struct lkpi_txq *ltxq;
			int tid;

			count = 0;
			/* Wake up all queues to know they are allocated in the driver. */
			for (tid = 0; tid < nitems(sta->txq); tid++) {

				if (tid == IEEE80211_NUM_TIDS) {
					IMPROVE("station specific?");
					if (!ieee80211_hw_check(hw, STA_MMPDU_TXQ))
						continue;
				} else if (tid >= hw->queues)
					continue;

				if (sta->txq[tid] == NULL)
					continue;

				ltxq = TXQ_TO_LTXQ(sta->txq[tid]);
				if (!ltxq->seen_dequeue)
					count++;
			}
			if (count == 0)
				break;
#ifdef LINUXKPI_DEBUG_80211
			if (count > 0)
				ic_printf(vap->iv_ic, "%s: waiting for %d quuees "
				    "to be allocated by driver\n", __func__, count);
#endif
			pause("lkpi80211txq", hz/10);
		}
#ifdef LINUXKPI_DEBUG_80211
		if (count > 0)
			ic_printf(vap->iv_ic, "%s: %d quuees still not "
			    "allocated by driver\n", __func__, count);
#endif
	}

	/* Start mgd_prepare_tx. */
	memset(&prep_tx_info, 0, sizeof(prep_tx_info));
	prep_tx_info.duration = PREP_TX_INFO_DURATION;
	lkpi_80211_mo_mgd_prepare_tx(hw, vif, &prep_tx_info);
	lsta->in_mgd = true;

	/*
	 * What is going to happen next:
	 * - <twiddle> .. we should end up in "auth_to_assoc"
	 * - event_callback
	 * - update sta_state (NONE to AUTH)
	 * - mgd_complete_tx
	 * (ideally we'd do that on a callback for something else ...)
	 */

out:
	IEEE80211_LOCK(vap->iv_ic);
	if (ni != NULL)
		ieee80211_free_node(ni);
	return (error);
}

static int
lkpi_sta_auth_to_scan(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct ieee80211_node *ni;
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;
	struct ieee80211_prep_tx_info prep_tx_info;
	int error;

	lhw = vap->iv_ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	/* Keep ni around. */
	ni = ieee80211_ref_node(vap->iv_bss);

	IEEE80211_UNLOCK(vap->iv_ic);
	lsta = ni->ni_drv_data;
	sta = LSTA_TO_STA(lsta);

	/* flush, drop. */
	lkpi_80211_mo_flush(hw, vif,  nitems(sta->txq), true);

	IEEE80211_LOCK(vap->iv_ic);

	/* Call iv_newstate first so we get potential deauth packet out. */
	error = lvif->iv_newstate(vap, nstate, arg);
	if (error != 0)
		goto outni;

	IEEE80211_UNLOCK(vap->iv_ic);

	/* Wake tx queues to get packet(s) out. */
	lkpi_wake_tx_queues(hw, sta, true, true);

	/* flush, no drop */
	lkpi_80211_mo_flush(hw, vif,  nitems(sta->txq), false);

	/* Take the station and chan ctx down again. */

	IMPROVE("event callback with failure?");

	/* End mgd_complete_tx. */
	if (lsta->in_mgd) {
		memset(&prep_tx_info, 0, sizeof(prep_tx_info));
		prep_tx_info.success = false;
		lkpi_80211_mo_mgd_complete_tx(hw, vif, &prep_tx_info);
		lsta->in_mgd = false;
	}

#ifdef __not_yet__
	/* sync_rx_queues */
	lkpi_80211_mo_sync_rx_queues(hw);

	/* sta_pre_rcu_remove */
        lkpi_80211_mo_sta_pre_rcu_remove(hw, vif, sta);
#endif

	/* Adjust sta and change state (from NONE) to NOTEXIST. */
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_NONE, ("%s: lsta %p state not "
	    "NONE: %#x\n", __func__, lsta, lsta->state));
	error = lkpi_80211_mo_sta_state(hw, vif, sta, IEEE80211_STA_NOTEXIST);
	if (error != 0) {
		IMPROVE("do we need to undo the chan ctx?");
		goto out;
	}
#if 0
	lsta->added_to_drv = false;	/* mo manages. */
#endif

	IMPROVE("Any bss_info changes to announce?");

	if (vif->chanctx_conf != NULL) {
		struct ieee80211_chanctx_conf *conf;

		conf = vif->chanctx_conf;
		/* Remove vif context. */
		lkpi_80211_mo_unassign_vif_chanctx(hw, vif, &vif->chanctx_conf);
		/* NB: vif->chanctx_conf is NULL now. */

		/* Remove chan ctx. */
		lkpi_80211_mo_remove_chanctx(hw, conf);
		free(conf, M_LKPI80211);
	}

	/* No need to start a scan; ic_scan_start should do. */

	error = EALREADY;
out:
	IEEE80211_LOCK(vap->iv_ic);
outni:
	if (ni != NULL)
		ieee80211_free_node(ni);
	return (error);
}

static int
lkpi_sta_auth_to_init(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	int error;

	error = lkpi_sta_auth_to_scan(vap, nstate, arg);
	if (error == 0)
		error = lkpi_sta_scan_to_init(vap, nstate, arg);
	return (error);
}

static int
lkpi_sta_auth_to_assoc(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct ieee80211_node *ni;
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;
	struct ieee80211_prep_tx_info prep_tx_info;
	int error;

	lhw = vap->iv_ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	IEEE80211_UNLOCK(vap->iv_ic);
	ni = NULL;

	/* Finish auth. */
	IMPROVE("event callback");

	/* Update sta_state (NONE to AUTH). */
	ni = ieee80211_ref_node(vap->iv_bss);
	lsta = ni->ni_drv_data;
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_NONE, ("%s: lsta %p state not "
	    "NONE: %#x\n", __func__, lsta, lsta->state));
	sta = LSTA_TO_STA(lsta);
	error = lkpi_80211_mo_sta_state(hw, vif, sta, IEEE80211_STA_AUTH);
	if (error != 0)
		goto out;

	/* End mgd_complete_tx. */
	if (lsta->in_mgd) {
		memset(&prep_tx_info, 0, sizeof(prep_tx_info));
		prep_tx_info.success = true;
		lkpi_80211_mo_mgd_complete_tx(hw, vif, &prep_tx_info);
		lsta->in_mgd = false;
	}

	/* Now start assoc. */

	/* Start mgd_prepare_tx. */
	if (!lsta->in_mgd) {
		memset(&prep_tx_info, 0, sizeof(prep_tx_info));
		prep_tx_info.duration = PREP_TX_INFO_DURATION;
		lkpi_80211_mo_mgd_prepare_tx(hw, vif, &prep_tx_info);
		lsta->in_mgd = true;
	}

	/* Wake tx queue to get packet out. */
	lkpi_wake_tx_queues(hw, sta, true, true);

	/*
	 * <twiddle> .. we end up in "assoc_to_run"
	 * - update sta_state (AUTH to ASSOC)
	 * - conf_tx [all]
	 * - bss_info_changed (assoc, aid, ssid, ..)
	 * - change_chanctx (if needed)
	 * - event_callback
	 * - mgd_complete_tx
	 */

out:
	IEEE80211_LOCK(vap->iv_ic);
	if (ni != NULL)
		ieee80211_free_node(ni);
	return (error);
}

/* auth_to_auth, assoc_to_assoc. */
static int
lkpi_sta_a_to_a(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct ieee80211_node *ni;
	struct lkpi_sta *lsta;
	struct ieee80211_prep_tx_info prep_tx_info;

	lhw = vap->iv_ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	ni = ieee80211_ref_node(vap->iv_bss);

	IEEE80211_UNLOCK(vap->iv_ic);
	lsta = ni->ni_drv_data;

	IMPROVE("event callback?");

	/* End mgd_complete_tx. */
	if (lsta->in_mgd) {
		memset(&prep_tx_info, 0, sizeof(prep_tx_info));
		prep_tx_info.success = false;
		lkpi_80211_mo_mgd_complete_tx(hw, vif, &prep_tx_info);
		lsta->in_mgd = false;
	}

	/* Now start assoc. */

	/* Start mgd_prepare_tx. */
	if (!lsta->in_mgd) {
		memset(&prep_tx_info, 0, sizeof(prep_tx_info));
		prep_tx_info.duration = PREP_TX_INFO_DURATION;
		lkpi_80211_mo_mgd_prepare_tx(hw, vif, &prep_tx_info);
		lsta->in_mgd = true;
	}

	IEEE80211_LOCK(vap->iv_ic);
	if (ni != NULL)
		ieee80211_free_node(ni);

	return (0);
}

static int
lkpi_sta_assoc_to_auth(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct ieee80211_node *ni;
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;
	struct ieee80211_prep_tx_info prep_tx_info;
	int error;

	lhw = vap->iv_ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	/* Keep ni around. */
	ni = ieee80211_ref_node(vap->iv_bss);

	IEEE80211_UNLOCK(vap->iv_ic);
	lsta = ni->ni_drv_data;
	sta = LSTA_TO_STA(lsta);

	/* End mgd_complete_tx. */
	if (lsta->in_mgd) {
		memset(&prep_tx_info, 0, sizeof(prep_tx_info));
		prep_tx_info.success = false;
		lkpi_80211_mo_mgd_complete_tx(hw, vif, &prep_tx_info);
		lsta->in_mgd = false;
	}

	/* Update sta and change state (from AUTH) to NONE. */
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_AUTH, ("%s: lsta %p state not "
	    "AUTH: %#x\n", __func__, lsta, lsta->state));
	error = lkpi_80211_mo_sta_state(hw, vif, sta, IEEE80211_STA_NONE);
	if (error != 0)
		goto out;

	IMPROVE("anything else?");

out:
	IEEE80211_LOCK(vap->iv_ic);
	if (ni != NULL)
		ieee80211_free_node(ni);
	return (error);
}

static int
lkpi_sta_assoc_to_scan(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	int error;

	error = lkpi_sta_assoc_to_auth(vap, nstate, arg);
	if (error == 0)
		error = lkpi_sta_auth_to_scan(vap, nstate, arg);
	return (error);
}

static int
lkpi_sta_assoc_to_init(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	int error;

	error = lkpi_sta_assoc_to_scan(vap, nstate, arg);
	if (error == 0)
		error = lkpi_sta_scan_to_init(vap, nstate, arg);
	return (error);
}

static int
lkpi_sta_assoc_to_run(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct ieee80211_node *ni;
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;
	struct ieee80211_prep_tx_info prep_tx_info;
	enum ieee80211_bss_changed bss_changed;
	int error;

	lhw = vap->iv_ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	IEEE80211_UNLOCK(vap->iv_ic);
	ni = NULL;

	IMPROVE("ponder some of this moved to ic_newassoc, scan_assoc_success, "
	    "and to lesser extend ieee80211_notify_node_join");

	/* Finish assoc. */
	/* Update sta_state (AUTH to ASSOC) and set aid. */
	ni = ieee80211_ref_node(vap->iv_bss);
	lsta = ni->ni_drv_data;
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_AUTH, ("%s: lsta %p state not "
	    "AUTH: %#x\n", __func__, lsta, lsta->state));
	sta = LSTA_TO_STA(lsta);
	sta->aid = IEEE80211_NODE_AID(ni);
	error = lkpi_80211_mo_sta_state(hw, vif, sta, IEEE80211_STA_ASSOC);
	if (error != 0)
		goto out;

	IMPROVE("wme / conf_tx [all]");

	/* Update bss info (bss_info_changed) (assoc, aid, ..). */
	bss_changed = 0;
	if (!vif->bss_conf.assoc || vif->bss_conf.aid != IEEE80211_NODE_AID(ni)) {
		vif->bss_conf.assoc = true;
		vif->bss_conf.aid = IEEE80211_NODE_AID(ni);
		bss_changed |= BSS_CHANGED_ASSOC;
	}
	/* We set SSID but this is not BSSID! */
	vif->bss_conf.ssid_len = ni->ni_esslen;
	memcpy(vif->bss_conf.ssid, ni->ni_essid, ni->ni_esslen);
	if ((vap->iv_flags & IEEE80211_F_SHPREAMBLE) !=
	    vif->bss_conf.use_short_preamble) {
		vif->bss_conf.use_short_preamble ^= 1;
		/* bss_changed |= BSS_CHANGED_??? */
	}
	if ((vap->iv_flags & IEEE80211_F_SHSLOT) !=
	    vif->bss_conf.use_short_slot) {
		vif->bss_conf.use_short_slot ^= 1;
		/* bss_changed |= BSS_CHANGED_??? */
	}
	if ((ni->ni_flags & IEEE80211_NODE_QOS) !=
	    vif->bss_conf.qos) {
		vif->bss_conf.qos ^= 1;
		bss_changed |= BSS_CHANGED_QOS;
	}
	if (vif->bss_conf.beacon_int != ni->ni_intval) {
		vif->bss_conf.beacon_int = ni->ni_intval;
		/* iwlwifi FW bug workaround; iwl_mvm_mac_sta_state. */
		if (vif->bss_conf.beacon_int < 16)
			vif->bss_conf.beacon_int = 16;
		bss_changed |= BSS_CHANGED_BEACON_INT;
	}
	if (vif->bss_conf.dtim_period != vap->iv_dtim_period &&
	    vap->iv_dtim_period > 0) {
		vif->bss_conf.dtim_period = vap->iv_dtim_period;
		bss_changed |= BSS_CHANGED_BEACON_INFO;
	}
	lkpi_80211_mo_bss_info_changed(hw, vif, &vif->bss_conf, bss_changed);

	/* - change_chanctx (if needed)
	 * - event_callback
	 */

	/* End mgd_complete_tx. */
	if (lsta->in_mgd) {
		memset(&prep_tx_info, 0, sizeof(prep_tx_info));
		prep_tx_info.success = true;
		lkpi_80211_mo_mgd_complete_tx(hw, vif, &prep_tx_info);
		lsta->in_mgd = false;
	}

	lkpi_hw_conf_idle(hw, false);

	/*
	 * And then:
	 * - (more packets)?
	 * - set_key
	 * - set_default_unicast_key
	 * - set_key (?)
	 * - ipv6_addr_change (?)
	 */
	/* Prepare_multicast && configure_filter. */
	lhw->update_mc = true;
	lkpi_update_mcast_filter(vap->iv_ic, true);

	if (!ieee80211_node_is_authorized(ni)) {
		IMPROVE("net80211 does not consider node authorized");
	}

	/* Update sta_state (ASSOC to AUTHORIZED). */
	ni = ieee80211_ref_node(vap->iv_bss);
	lsta = ni->ni_drv_data;
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_ASSOC, ("%s: lsta %p state not "
	    "ASSOC: %#x\n", __func__, lsta, lsta->state));
	sta = LSTA_TO_STA(lsta);
	error = lkpi_80211_mo_sta_state(hw, vif, sta, IEEE80211_STA_AUTHORIZED);
	if (error != 0) {
		IMPROVE("undo some changes?");
		goto out;
	}

	/* - drv_config (?)
	 * - bss_info_changed
	 * - set_rekey_data (?)
	 *
	 * And now we should be passing packets.
	 */
	IMPROVE("Need that bssid setting, and the keys");

out:
	IEEE80211_LOCK(vap->iv_ic);
	if (ni != NULL)
		ieee80211_free_node(ni);
	return (error);
}

static int
lkpi_sta_auth_to_run(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	int error;

	error = lkpi_sta_auth_to_assoc(vap, nstate, arg);
	if (error == 0)
		error = lkpi_sta_assoc_to_run(vap, nstate, arg);
	return (error);
}

static int
lkpi_sta_run_to_assoc(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct ieee80211_node *ni;
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;
	int error;

	lhw = vap->iv_ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	/* Keep ni around. */
	ni = ieee80211_ref_node(vap->iv_bss);

	IEEE80211_UNLOCK(vap->iv_ic);
	lsta = ni->ni_drv_data;
	sta = LSTA_TO_STA(lsta);

	/* Adjust sta and change state (from AUTHORIZED) to ASSOC. */
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_AUTHORIZED, ("%s: lsta %p state not "
	    "AUTHORIZED: %#x\n", __func__, lsta, lsta->state));
	error = lkpi_80211_mo_sta_state(hw, vif, sta, IEEE80211_STA_ASSOC);
	if (error != 0)
		goto out;

	/* Update bss info (bss_info_changed) (assoc, aid, ..). */
	lkpi_disassoc(sta, vif, lhw);

	/* Update sta_state (ASSOC to AUTH). */
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_ASSOC, ("%s: lsta %p state not "
	    "ASSOC: %#x\n", __func__, lsta, lsta->state));
	sta = LSTA_TO_STA(lsta);
	sta->aid = 0;
	error = lkpi_80211_mo_sta_state(hw, vif, sta, IEEE80211_STA_AUTH);
	if (error != 0)
		goto out;

	IMPROVE("if ASSOC is final state, prep_tx_info?");

out:
	IEEE80211_LOCK(vap->iv_ic);
	if (ni != NULL)
		ieee80211_free_node(ni);
	return (error);
}

static int
lkpi_sta_run_to_auth(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	int error;

	error = lkpi_sta_run_to_assoc(vap, nstate, arg);
	if (error == 0)
		error = lkpi_sta_assoc_to_auth(vap, nstate, arg);
	return (error);
}

static int
lkpi_sta_run_to_scan(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	int error;

	error = lkpi_sta_run_to_auth(vap, nstate, arg);
	if (error == 0)
		error = lkpi_sta_auth_to_scan(vap, nstate, arg);
	return (error);
}

static int
lkpi_sta_run_to_init(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	int error;

	error = lkpi_sta_run_to_scan(vap, nstate, arg);
	if (error == 0)
		error = lkpi_sta_scan_to_init(vap, nstate, arg);
	return (error);
}

/*
 * The matches the documented state changes in net80211::sta_newstate().
 * XXX (1) without CSA and SLEEP yet, * XXX (2) not all unhandled cases
 * there are "invalid" (so there is a room for failure here).
 */
struct fsm_state {
	/* INIT, SCAN, AUTH, ASSOC, CAC, RUN, CSA, SLEEP */
	enum ieee80211_state ostate;
	enum ieee80211_state nstate;
	int (*handler)(struct ieee80211vap *, enum ieee80211_state, int);
} sta_state_fsm[] = {
	{ IEEE80211_S_INIT,	IEEE80211_S_INIT, lkpi_sta_state_do_nada },
	{ IEEE80211_S_SCAN,	IEEE80211_S_INIT, lkpi_sta_state_do_nada },	/* scan_to_init */
	{ IEEE80211_S_AUTH,	IEEE80211_S_INIT, lkpi_sta_auth_to_init },	/* not explicitly in sta_newstate() */
	{ IEEE80211_S_ASSOC,	IEEE80211_S_INIT, lkpi_sta_assoc_to_init },
	{ IEEE80211_S_RUN,	IEEE80211_S_INIT, lkpi_sta_run_to_init },

	{ IEEE80211_S_INIT,	IEEE80211_S_SCAN, lkpi_sta_state_do_nada },
	{ IEEE80211_S_SCAN,	IEEE80211_S_SCAN, lkpi_sta_state_do_nada },
	{ IEEE80211_S_AUTH,	IEEE80211_S_SCAN, lkpi_sta_auth_to_scan },
	{ IEEE80211_S_ASSOC,	IEEE80211_S_SCAN, lkpi_sta_assoc_to_scan },
	{ IEEE80211_S_RUN,	IEEE80211_S_SCAN, lkpi_sta_run_to_scan },

	{ IEEE80211_S_INIT,	IEEE80211_S_AUTH, lkpi_sta_scan_to_auth },
	{ IEEE80211_S_SCAN,	IEEE80211_S_AUTH, lkpi_sta_scan_to_auth },
	{ IEEE80211_S_AUTH,	IEEE80211_S_AUTH, lkpi_sta_a_to_a },
	{ IEEE80211_S_ASSOC,	IEEE80211_S_AUTH, lkpi_sta_assoc_to_auth },
	{ IEEE80211_S_RUN,	IEEE80211_S_AUTH, lkpi_sta_run_to_auth },

	{ IEEE80211_S_AUTH,	IEEE80211_S_ASSOC, lkpi_sta_auth_to_assoc },
	{ IEEE80211_S_ASSOC,	IEEE80211_S_ASSOC, lkpi_sta_a_to_a },
	{ IEEE80211_S_RUN,	IEEE80211_S_ASSOC, lkpi_sta_run_to_assoc },

	{ IEEE80211_S_AUTH,	IEEE80211_S_RUN, lkpi_sta_auth_to_run },
	{ IEEE80211_S_ASSOC,	IEEE80211_S_RUN, lkpi_sta_assoc_to_run },
	{ IEEE80211_S_RUN,	IEEE80211_S_RUN, lkpi_sta_state_do_nada },

	/* Dummy at the end without handler. */
	{ IEEE80211_S_INIT,	IEEE80211_S_INIT, NULL },
};

static int
lkpi_iv_newstate(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct fsm_state *s;
	enum ieee80211_state ostate;
	int error;

	ic = vap->iv_ic;
	IEEE80211_LOCK_ASSERT(ic);
	ostate = vap->iv_state;

	if (debug_80211 & D80211_TRACE)
		ic_printf(vap->iv_ic, "%s:%d: vap %p nstate %#x arg %#x\n",
		    __func__, __LINE__, vap, nstate, arg);

	if (vap->iv_opmode == IEEE80211_M_STA) {

		lhw = ic->ic_softc;
		lvif = VAP_TO_LVIF(vap);
		vif = LVIF_TO_VIF(lvif);

		/* No need to replicate this in most state handlers. */
		if (ostate == IEEE80211_S_SCAN && nstate != IEEE80211_S_SCAN)
			lkpi_stop_hw_scan(lhw, vif);

		s = sta_state_fsm;

	} else {
		ic_printf(vap->iv_ic, "%s: only station mode currently supported: "
		    "cap %p iv_opmode %d\n", __func__, vap, vap->iv_opmode);
		return (ENOSYS);
	}

	error = 0;
	for (; s->handler != NULL; s++) {
		if (ostate == s->ostate && nstate == s->nstate) {
			error = s->handler(vap, nstate, arg);
			break;
		}
	}
	IEEE80211_LOCK_ASSERT(vap->iv_ic);

	if (s->handler == NULL) {
		IMPROVE("thurn this into a KASSERT\n");
		ic_printf(vap->iv_ic, "%s: unsupported state transition "
		    "%d (%s) -> %d (%s)\n", __func__,
		    ostate, ieee80211_state_name[ostate],
		    nstate, ieee80211_state_name[nstate]);
		return (ENOSYS);
	}

	if (error == EALREADY) {
		IMPROVE("make this a debug log later");
		ic_printf(vap->iv_ic, "%s: error %d during state transition "
		    "%d (%s) -> %d (%s): iv_newstate already handled.\n",
		    __func__, error,
		    ostate, ieee80211_state_name[ostate],
		    nstate, ieee80211_state_name[nstate]);
		return (0);
	}

	if (error != 0) {
		/* XXX-BZ currently expected so ignore. */
		ic_printf(vap->iv_ic, "%s: error %d during state transition "
		    "%d (%s) -> %d (%s)\n", __func__, error,
		    ostate, ieee80211_state_name[ostate],
		    nstate, ieee80211_state_name[nstate]);
		/* return (error); */
	}

	if (debug_80211 & D80211_TRACE)
		ic_printf(vap->iv_ic, "%s:%d: vap %p nstate %#x arg %#x calling net80211 parent\n",
		    __func__, __LINE__, vap, nstate, arg);

	return (lvif->iv_newstate(vap, nstate, arg));
}

/* -------------------------------------------------------------------------- */

static int
lkpi_ic_wme_update(struct ieee80211com *ic)
{
	/* This needs queuing and go at the right moment. */
#ifdef WITH_WME_UPDATE
	struct ieee80211vap *vap;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct chanAccParams chp;
	struct wmeParams wmeparr[WME_NUM_AC];
	struct ieee80211_tx_queue_params txqp;
	enum ieee80211_bss_changed changed;
	int error;
	uint16_t ac;
#endif

	IMPROVE();
	KASSERT(WME_NUM_AC == IEEE80211_NUM_ACS, ("%s: WME_NUM_AC %d != "
	    "IEEE80211_NUM_ACS %d\n", __func__, WME_NUM_AC, IEEE80211_NUM_ACS));

#ifdef WITH_WME_UPDATE
	vap = TAILQ_FIRST(&ic->ic_vaps);
	if (vap == NULL)
		return (0);

	/* We should factor this out into per-vap (*wme_update). */
	lhw = ic->ic_softc;
	if (lhw->ops->conf_tx == NULL)
		return (0);

	/* XXX-BZ check amount of hw queues */
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	ieee80211_wme_ic_getparams(ic, &chp);
	IEEE80211_LOCK(ic);
	for (ac = 0; ac < WME_NUM_AC; ac++)
		wmeparr[ac] = chp.cap_wmeParams[ac];
	IEEE80211_UNLOCK(ic);

	/* Configure tx queues (conf_tx) & send BSS_CHANGED_QOS. */
	LKPI_80211_LHW_LOCK(lhw);
	for (ac = 0; ac < IEEE80211_NUM_ACS; ac++) {
		struct wmeParams *wmep;

		/* XXX-BZ should keep this in lvif? */
		wmep = &wmeparr[ac];
		bzero(&txqp, sizeof(txqp));
		txqp.cw_min = wmep->wmep_logcwmin;
		txqp.cw_max = wmep->wmep_logcwmax;
		txqp.txop = wmep->wmep_txopLimit;
		txqp.aifs = wmep->wmep_aifsn;
		error = lkpi_80211_mo_conf_tx(hw, vif, ac, &txqp);
		if (error != 0)
			printf("%s: conf_tx ac %u failed %d\n",
			    __func__, ac, error);
	}
	LKPI_80211_LHW_UNLOCK(lhw);
	changed = BSS_CHANGED_QOS;
	lkpi_80211_mo_bss_info_changed(hw, vif, &vif->bss_conf, changed);
#endif

	return (0);
}

static struct ieee80211vap *
lkpi_ic_vap_create(struct ieee80211com *ic, const char name[IFNAMSIZ],
    int unit, enum ieee80211_opmode opmode, int flags,
    const uint8_t bssid[IEEE80211_ADDR_LEN],
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211vap *vap;
	struct ieee80211_vif *vif;
	enum ieee80211_bss_changed changed;
	size_t len;
	int error;

	if (!TAILQ_EMPTY(&ic->ic_vaps))	/* 1 so far. Add <n> once this works. */
		return (NULL);

	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);

	len = sizeof(*lvif);
	len += hw->vif_data_size;	/* vif->drv_priv */

	lvif = malloc(len, M_80211_VAP, M_WAITOK | M_ZERO);
	mtx_init(&lvif->mtx, "lvif", NULL, MTX_DEF);
	TAILQ_INIT(&lvif->lsta_head);
	vap = LVIF_TO_VAP(lvif);

	vif = LVIF_TO_VIF(lvif);
	memcpy(vif->addr, mac, IEEE80211_ADDR_LEN);
	vif->p2p = false;
	vif->probe_req_reg = false;
	vif->type = lkpi_opmode_to_vif_type(opmode);
	lvif->wdev.iftype = vif->type;
	/* Need to fill in other fields as well. */
	IMPROVE();

	/* XXX-BZ hardcoded for now! */
#if 1
	vif->chanctx_conf = NULL;
	vif->bss_conf.idle = true;
	vif->bss_conf.ps = false;
	vif->bss_conf.chandef.width = NL80211_CHAN_WIDTH_20_NOHT;
	vif->bss_conf.use_short_preamble = false;	/* vap->iv_flags IEEE80211_F_SHPREAMBLE */
	vif->bss_conf.use_short_slot = false;		/* vap->iv_flags IEEE80211_F_SHSLOT */
	vif->bss_conf.qos = false;
	vif->bss_conf.use_cts_prot = false;		/* vap->iv_protmode */
	vif->bss_conf.ht_operation_mode = IEEE80211_HT_OP_MODE_PROTECTION_NONE;
	vif->bss_conf.assoc = false;
	vif->bss_conf.aid = 0;
#endif
#if 0
	vif->bss_conf.dtim_period = 0; /* IEEE80211_DTIM_DEFAULT ; must stay 0. */
	IEEE80211_ADDR_COPY(vif->bss_conf.bssid, bssid);
	vif->bss_conf.beacon_int = ic->ic_bintval;
	/* iwlwifi bug. */
	if (vif->bss_conf.beacon_int < 16)
		vif->bss_conf.beacon_int = 16;
#endif
	IMPROVE();

	error = lkpi_80211_mo_start(hw);
	if (error != 0) {
		printf("%s: failed to start hw: %d\n", __func__, error);
		mtx_destroy(&lvif->mtx);
		free(lvif, M_80211_VAP);
		return (NULL);
	}

	error = lkpi_80211_mo_add_interface(hw, vif);
	if (error != 0) {
		IMPROVE();	/* XXX-BZ mo_stop()? */
		printf("%s: failed to add interface: %d\n", __func__, error);
		mtx_destroy(&lvif->mtx);
		free(lvif, M_80211_VAP);
		return (NULL);
	}

	LKPI_80211_LHW_LOCK(lhw);
	TAILQ_INSERT_TAIL(&lhw->lvif_head, lvif, lvif_entry);
	LKPI_80211_LHW_UNLOCK(lhw);

	/* Set bss_info. */
	changed = 0;
	lkpi_80211_mo_bss_info_changed(hw, vif, &vif->bss_conf, changed);

	/* conf_tx setup; default WME? */

	/* Force MC init. */
	lkpi_update_mcast_filter(ic, true);

	IMPROVE();

	ieee80211_vap_setup(ic, vap, name, unit, opmode, flags, bssid);

	/* Override with LinuxKPI method so we can drive mac80211/cfg80211. */
	lvif->iv_newstate = vap->iv_newstate;
	vap->iv_newstate = lkpi_iv_newstate;

	/* Key management. */
	if (lhw->ops->set_key != NULL) {
#ifdef TRY_HW_CRYPTO
		vap->iv_key_set = lkpi_iv_key_set;
		vap->iv_key_delete = lkpi_iv_key_delete;
#endif
	}

	ieee80211_ratectl_init(vap);

	/* Complete setup. */
	ieee80211_vap_attach(vap, ieee80211_media_change,
	    ieee80211_media_status, mac);

	if (hw->max_listen_interval == 0)
		hw->max_listen_interval = 7 * (ic->ic_lintval / ic->ic_bintval);
	hw->conf.listen_interval = hw->max_listen_interval;
	ic->ic_set_channel(ic);

	/* XXX-BZ do we need to be able to update these? */
	hw->wiphy->frag_threshold =  vap->iv_fragthreshold;
	lkpi_80211_mo_set_frag_threshold(hw, vap->iv_fragthreshold);
	hw->wiphy->rts_threshold =  vap->iv_rtsthreshold;
	lkpi_80211_mo_set_rts_threshold(hw, vap->iv_rtsthreshold);
	/* any others? */
	IMPROVE();

	return (vap);
}

static void
lkpi_ic_vap_delete(struct ieee80211vap *vap)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;

	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);
	ic = vap->iv_ic;
	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);

	LKPI_80211_LHW_LOCK(lhw);
	TAILQ_REMOVE(&lhw->lvif_head, lvif, lvif_entry);
	LKPI_80211_LHW_UNLOCK(lhw);
	lkpi_80211_mo_remove_interface(hw, vif);

	ieee80211_ratectl_deinit(vap);
	ieee80211_vap_detach(vap);
	mtx_destroy(&lvif->mtx);
	free(lvif, M_80211_VAP);
}

static void
lkpi_ic_update_mcast(struct ieee80211com *ic)
{

	lkpi_update_mcast_filter(ic, false);
	TRACEOK();
}

static void
lkpi_ic_update_promisc(struct ieee80211com *ic)
{

	UNIMPLEMENTED;
}

static void
lkpi_ic_update_chw(struct ieee80211com *ic)
{

	UNIMPLEMENTED;
}

/* Start / stop device. */
static void
lkpi_ic_parent(struct ieee80211com *ic)
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	int error;
	bool start_all;

	IMPROVE();

	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	start_all = false;

	if (ic->ic_nrunning > 0) {
		error = lkpi_80211_mo_start(hw);
		if (error == 0)
			start_all = true;
	} else {
		lkpi_80211_mo_stop(hw);
	}

	if (start_all)
		ieee80211_start_all(ic);
}

bool
linuxkpi_ieee80211_is_ie_id_in_ie_buf(const u8 ie, const u8 *ie_ids,
    size_t ie_ids_len)
{
	int i;

	for (i = 0; i < ie_ids_len; i++) {
		if (ie == *ie_ids)
			return (true);
	}

	return (false);
}

/* Return true if skipped; false if error. */
bool
linuxkpi_ieee80211_ie_advance(size_t *xp, const u8 *ies, size_t ies_len)
{
	size_t x;
	uint8_t l;

	x = *xp;

	KASSERT(x < ies_len, ("%s: x %zu ies_len %zu ies %p\n",
	    __func__, x, ies_len, ies));
	l = ies[x + 1];
	x += 2 + l;

	if (x > ies_len)
		return (false);

	*xp = x;
	return (true);
}

static int
lkpi_ieee80211_probereq_ie_alloc(struct ieee80211vap *vap,
    struct ieee80211com *ic, struct ieee80211_scan_ies *scan_ies,
    const uint8_t *ssid, size_t ssidlen)
{

	return (ieee80211_probereq_ie(vap, ic,
	    &scan_ies->common_ies, &scan_ies->common_ie_len,
	    ssid, ssidlen, true));
}

static void
lkpi_ic_scan_start(struct ieee80211com *ic)
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct ieee80211_scan_state *ss;
	struct ieee80211vap *vap;
	int error;

	lhw = ic->ic_softc;
	if ((lhw->scan_flags & LKPI_SCAN_RUNNING) != 0) {
		/* A scan is still running. */
		return;
	}

	ss = ic->ic_scan;
	vap = ss->ss_vap;
	if (vap->iv_state != IEEE80211_S_SCAN) {
		/* Do not start a scan for now. */
		return;
	}

	hw = LHW_TO_HW(lhw);
	if ((ic->ic_flags_ext & IEEE80211_FEXT_SCAN_OFFLOAD) == 0) {

		lvif = VAP_TO_LVIF(vap);
		vif = LVIF_TO_VIF(lvif);

		if (vap->iv_state == IEEE80211_S_SCAN)
			lkpi_hw_conf_idle(hw, false);

		lkpi_80211_mo_sw_scan_start(hw, vif, vif->addr);
		/* net80211::scan_start() handled PS for us. */
		IMPROVE();
		/* XXX Also means it is too late to flush queues?
		 * need to check iv_sta_ps or overload? */
		/* XXX want to adjust ss end time/ maxdwell? */

	} else {
		struct ieee80211_channel *c;
		struct ieee80211_scan_request *hw_req;
		struct linuxkpi_ieee80211_channel *lc, **cpp;
		struct cfg80211_ssid *ssids;
		struct cfg80211_scan_6ghz_params *s6gp;
		size_t chan_len, nchan, ssids_len, s6ghzlen;
		int i;

		ssids_len = ss->ss_nssid * sizeof(*ssids);;
		s6ghzlen = 0 * (sizeof(*s6gp));			/* XXX-BZ */

		nchan = 0;
		for (i = ss->ss_next; i < ss->ss_last; i++)
			nchan++;
		chan_len = nchan * (sizeof(lc) + sizeof(*lc));

		KASSERT(lhw->hw_req == NULL, ("%s: ic %p lhw %p hw_req %p "
		    "!= NULL\n", __func__, ic, lhw, lhw->hw_req));
		lhw->hw_req = hw_req = malloc(sizeof(*hw_req) + ssids_len +
		    s6ghzlen + chan_len, M_LKPI80211, M_WAITOK | M_ZERO);

		error = lkpi_ieee80211_probereq_ie_alloc(vap, ic,
		    &hw_req->ies, NULL, -1);
		if (error != 0)
			ic_printf(ic, "ERROR: %s: probereq_ie returned %d\n",
			    __func__, error);

		hw_req->req.flags = 0;			/* XXX ??? */
		/* hw_req->req.wdev */
		hw_req->req.wiphy = hw->wiphy;
		hw_req->req.no_cck = false;		/* XXX */
#if 0
		/* This seems to pessimise default scanning behaviour. */
		hw_req->req.duration_mandatory = TICKS_2_USEC(ss->ss_mindwell);
		hw_req->req.duration = TICKS_2_USEC(ss->ss_maxdwell);
#endif
#ifdef __notyet__
		hw_req->req.flags |= NL80211_SCAN_FLAG_RANDOM_ADDR;
		memcpy(hw_req->req.mac_addr, xxx, IEEE80211_ADDR_LEN);
		memset(hw_req->req.mac_addr_mask, 0xxx, IEEE80211_ADDR_LEN);
#endif
#if 0
		hw_req->req.ie_len = ;
		hw_req->req.ie = ;
#endif

		hw_req->req.n_channels = nchan;
		cpp = (struct linuxkpi_ieee80211_channel **)(hw_req + 1);
		lc = (struct linuxkpi_ieee80211_channel *)(cpp + nchan);
		for (i = 0; i < nchan; i++) {
			*(cpp + i) =
			    (struct linuxkpi_ieee80211_channel *)(lc + i);
		}
		for (i = 0; i < nchan; i++) {
			c = ss->ss_chans[ss->ss_next + i];

			lc->hw_value = c->ic_ieee;
			lc->center_freq = c->ic_freq;
			/* lc->flags */
			lc->band = lkpi_net80211_chan_to_nl80211_band(c);
			lc->max_power = c->ic_maxpower;
			/* lc-> ... */
			lc++;
		}

		hw_req->req.n_ssids = ss->ss_nssid;
		if (hw_req->req.n_ssids > 0) {
			ssids = (struct cfg80211_ssid *)lc;
			hw_req->req.ssids = ssids;
			for (i = 0; i < ss->ss_nssid; i++) {
				ssids->ssid_len = ss->ss_ssid[i].len;
				memcpy(ssids->ssid, ss->ss_ssid[i].ssid,
				    ss->ss_ssid[i].len);
				ssids++;
			}
			s6gp = (struct cfg80211_scan_6ghz_params *)ssids;
		} else {
			s6gp = (struct cfg80211_scan_6ghz_params *)lc;
		}

		/* 6GHz one day. */
		hw_req->req.n_6ghz_params = 0;
		hw_req->req.scan_6ghz_params = NULL;
		hw_req->req.scan_6ghz = false;	/* Weird boolean; not what you think. */
		/* s6gp->... */

		lvif = VAP_TO_LVIF(vap);
		vif = LVIF_TO_VIF(lvif);
		error = lkpi_80211_mo_hw_scan(hw, vif, hw_req);
		if (error != 0) {
			ic_printf(ic, "ERROR: %s: hw_scan returned %d\n",
			    __func__, error);
			ieee80211_cancel_scan(vap);
			free(hw_req->ies.common_ies, M_80211_VAP);
			free(hw_req, M_LKPI80211);
			lhw->hw_req = NULL;
		}
	}
}

static void
lkpi_ic_scan_end(struct ieee80211com *ic)
{
	struct lkpi_hw *lhw;

	lhw = ic->ic_softc;
	if ((lhw->scan_flags & LKPI_SCAN_RUNNING) == 0) {
		return;
	}

	if (ic->ic_flags_ext & IEEE80211_FEXT_SCAN_OFFLOAD) {
		/* Nothing to do. */
	} else {
		struct ieee80211_hw *hw;
		struct lkpi_vif *lvif;
		struct ieee80211_vif *vif;
		struct ieee80211_scan_state *ss;
		struct ieee80211vap *vap;

		hw = LHW_TO_HW(lhw);
		ss = ic->ic_scan;
		vap = ss->ss_vap;
		lvif = VAP_TO_LVIF(vap);
		vif = LVIF_TO_VIF(lvif);
		lkpi_80211_mo_sw_scan_complete(hw, vif);

		/* Send PS to stop buffering if n80211 does not for us? */

		if (vap->iv_state == IEEE80211_S_SCAN)
			lkpi_hw_conf_idle(hw, true);
	}
}

static void
lkpi_ic_scan_curchan_nada(struct ieee80211_scan_state *ss __unused,
    unsigned long maxdwell __unused)
{
}

static void
lkpi_ic_set_channel(struct ieee80211com *ic)
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	int error;

	lhw = ic->ic_softc;
#ifdef __no_longer__
	/* For now only be concerned if scanning. */
	if ((lhw->scan_flags & LKPI_SCAN_RUNNING) == 0) {
		IMPROVE();
		return;
	}
#endif

	if (ic->ic_flags_ext & IEEE80211_FEXT_SCAN_OFFLOAD) {
		/*
		 * AP scanning is taken care of by firmware, so only switch
		 * channels in monitor mode (maybe, maybe not; to be
		 * investigated at the right time).
		 */
		if (ic->ic_opmode == IEEE80211_M_MONITOR) {
			UNIMPLEMENTED;
		}
	} else {
		struct ieee80211_channel *c = ic->ic_curchan;
		struct linuxkpi_ieee80211_channel *chan;
		struct cfg80211_chan_def chandef;

		if (c == NULL || c == IEEE80211_CHAN_ANYC ||
		    lhw->ops->config == NULL) {
			ic_printf(ic, "%s: c %p ops->config %p\n", __func__,
			    c, lhw->ops->config);
			return;
		}

		chan = lkpi_find_lkpi80211_chan(lhw, c);
		if (chan == NULL) {
			ic_printf(ic, "%s: c %p chan %p\n", __func__,
			    c, chan);
			return;
		}

		memset(&chandef, 0, sizeof(chandef));
		chandef.chan = chan;
		chandef.width = NL80211_CHAN_WIDTH_20_NOHT;
		chandef.center_freq1 = chandef.chan->center_freq;

		/* XXX max power for scanning? */
		IMPROVE();

		hw = LHW_TO_HW(lhw);
		hw->conf.chandef = chandef;

		error = lkpi_80211_mo_config(hw, IEEE80211_CONF_CHANGE_CHANNEL);
		if (error != 0 && error != EOPNOTSUPP) {
			ic_printf(ic, "ERROR: %s: config %#0x returned %d\n",
			    __func__, IEEE80211_CONF_CHANGE_CHANNEL, error);
			/* XXX should we unroll to the previous chandef? */
			IMPROVE();
		} else {
			/* Update radiotap channels as well. */
			lhw->rtap_tx.wt_chan_freq = htole16(c->ic_freq);
			lhw->rtap_tx.wt_chan_flags = htole16(c->ic_flags);
			lhw->rtap_rx.wr_chan_freq = htole16(c->ic_freq);
			lhw->rtap_rx.wr_chan_flags = htole16(c->ic_flags);
		}

		/* Currently PS is hard coded off! Not sure it belongs here. */
		IMPROVE();
		if (ieee80211_hw_check(hw, SUPPORTS_PS) &&
		    (hw->conf.flags & IEEE80211_CONF_PS) != 0) {
			hw->conf.flags &= ~IEEE80211_CONF_PS;
			error = lkpi_80211_mo_config(hw, IEEE80211_CONF_CHANGE_PS);
			if (error != 0 && error != EOPNOTSUPP)
				ic_printf(ic, "ERROR: %s: config %#0x returned "
				    "%d\n", __func__, IEEE80211_CONF_CHANGE_PS,
				    error);
		}
	}
}

static struct ieee80211_node *
lkpi_ic_node_alloc(struct ieee80211vap *vap,
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct ieee80211_node *ni;
	int tid;

	ic = vap->iv_ic;
	lhw = ic->ic_softc;

	/* We keep allocations de-coupled so we can deal with the two worlds. */
	if (lhw->ic_node_alloc != NULL) {
		ni = lhw->ic_node_alloc(vap, mac);
		if (ni == NULL)
			return (NULL);
	}

	hw = LHW_TO_HW(lhw);
	lsta = malloc(sizeof(*lsta) + hw->sta_data_size, M_LKPI80211,
	    M_NOWAIT | M_ZERO);
	if (lsta == NULL) {
		if (lhw->ic_node_free != NULL)
			lhw->ic_node_free(ni);
		return (NULL);
	}

	lsta->added_to_drv = false;
	lsta->state = IEEE80211_STA_NOTEXIST;
#if 0
	/*
	 * This needs to be done in node_init() as ieee80211_alloc_node()
	 * will initialise the refcount after us.
	 */
	lsta->ni = ieee80211_ref_node(ni);
#endif
	/* The back-pointer "drv_data" to net80211_node let's us get lsta. */
	ni->ni_drv_data = lsta;

	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);
	sta = LSTA_TO_STA(lsta);

	IEEE80211_ADDR_COPY(sta->addr, mac);
	for (tid = 0; tid < nitems(sta->txq); tid++) {
		struct lkpi_txq *ltxq;

		/*
		 * We are neither limiting ourselves to hw.queues here,
		 * nor do we check if driver wants IEEE80211_NUM_TIDS queue.
		 */

		ltxq = malloc(sizeof(*ltxq) + hw->txq_data_size,
		    M_LKPI80211, M_NOWAIT | M_ZERO);
		if (ltxq == NULL)
			goto cleanup;
		ltxq->seen_dequeue = false;
		skb_queue_head_init(&ltxq->skbq);
		/* iwlwifi//mvm/sta.c::tid_to_mac80211_ac[] */
		if (tid == IEEE80211_NUM_TIDS) {
			IMPROVE();
			ltxq->txq.ac = IEEE80211_AC_VO;
		} else {
			ltxq->txq.ac = tid_to_mac80211_ac[tid & 7];
		}
		ltxq->txq.tid = tid;
		ltxq->txq.sta = sta;
		ltxq->txq.vif = vif;
		sta->txq[tid] = &ltxq->txq;
	}

	/* Deferred TX path. */
	mtx_init(&lsta->txq_mtx, "lsta_txq", NULL, MTX_DEF);
	TASK_INIT(&lsta->txq_task, 0, lkpi_80211_txq_task, lsta);
	mbufq_init(&lsta->txq, IFQ_MAXLEN);

	return (ni);

cleanup:
	for (; tid >= 0; tid--)
		free(sta->txq[tid], M_LKPI80211);
	free(lsta, M_LKPI80211);
	if (lhw->ic_node_free != NULL)
		lhw->ic_node_free(ni);
	return (NULL);
}

static int
lkpi_ic_node_init(struct ieee80211_node *ni)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct lkpi_sta *lsta;
	struct lkpi_vif *lvif;
	int error;

	ic = ni->ni_ic;
	lhw = ic->ic_softc;

	if (lhw->ic_node_init != NULL) {
		error = lhw->ic_node_init(ni);
		if (error != 0)
			return (error);
	}

	lvif = VAP_TO_LVIF(ni->ni_vap);

	lsta = ni->ni_drv_data;

	/* Now take the reference before linking it to the table. */
	lsta->ni = ieee80211_ref_node(ni);

	LKPI_80211_LVIF_LOCK(lvif);
	TAILQ_INSERT_TAIL(&lvif->lsta_head, lsta, lsta_entry);
	LKPI_80211_LVIF_UNLOCK(lvif);

	/* XXX-BZ Sync other state over. */
	IMPROVE();

	return (0);
}

static void
lkpi_ic_node_cleanup(struct ieee80211_node *ni)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;

	ic = ni->ni_ic;
	lhw = ic->ic_softc;

	/* XXX-BZ remove from driver, ... */
	IMPROVE();

	if (lhw->ic_node_cleanup != NULL)
		lhw->ic_node_cleanup(ni);
}

static void
lkpi_ic_node_free(struct ieee80211_node *ni)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct lkpi_sta *lsta;

	ic = ni->ni_ic;
	lhw = ic->ic_softc;
	lsta = ni->ni_drv_data;

	/* XXX-BZ free resources, ... */
	IMPROVE();

	/* Flush mbufq (make sure to release ni refs!). */
#ifdef __notyet__
	KASSERT(mbufq_len(&lsta->txq) == 0, ("%s: lsta %p has txq len %d != 0\n",
	    __func__, lsta, mbufq_len(&lsta->txq)));
#endif
	/* Drain taskq. */

	/* Drain sta->txq[] */
	mtx_destroy(&lsta->txq_mtx);

	/* Remove lsta if added_to_drv. */
	/* Remove lsta from vif */

	/* remove ref from lsta node... */

	if (lhw->ic_node_free != NULL)
		lhw->ic_node_free(ni);

	/* Free lsta. */
}

static int
lkpi_ic_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
        const struct ieee80211_bpf_params *params __unused)
{
	struct lkpi_sta *lsta;

	lsta = ni->ni_drv_data;

	/* Queue the packet and enqueue the task to handle it. */
	LKPI_80211_LSTA_LOCK(lsta);
	mbufq_enqueue(&lsta->txq, m);
	LKPI_80211_LSTA_UNLOCK(lsta);

	if (debug_80211 & D80211_TRACE_TX)
		printf("%s:%d lsta %p ni %p %6D mbuf_qlen %d\n",
		    __func__, __LINE__, lsta, ni, ni->ni_macaddr, ":",
		    mbufq_len(&lsta->txq));

	taskqueue_enqueue(taskqueue_thread, &lsta->txq_task);
	return (0);
}

static void
lkpi_80211_txq_tx_one(struct lkpi_sta *lsta, struct mbuf *m)
{
	struct ieee80211_node *ni;
	struct ieee80211_frame *wh;
	struct ieee80211_key *k;
	struct sk_buff *skb;
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct ieee80211_channel *c;
	struct ieee80211_tx_control control;
	struct ieee80211_tx_info *info;
	struct ieee80211_sta *sta;
	void *buf;
	int ac;

	M_ASSERTPKTHDR(m);
#ifdef LINUXKPI_DEBUG_80211
	if (debug_80211 & D80211_TRACE_TX_DUMP)
		hexdump(mtod(m, const void *), m->m_len, "RAW TX (plain) ", 0);
#endif

	ni = lsta->ni;
#ifndef TRY_HW_CRYPTO
	/* Encrypt the frame if need be; XXX-BZ info->control.hw_key. */
	wh = mtod(m, struct ieee80211_frame *);
	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {
		/* Retrieve key for TX && do software encryption. */
		k = ieee80211_crypto_encap(ni, m);
		if (k == NULL) {
			ieee80211_free_node(ni);
			m_freem(m);
			return;
		}
	}
#endif

	ic = ni->ni_ic;
	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	c = ni->ni_chan;

	if (ieee80211_radiotap_active_vap(ni->ni_vap)) {
		struct lkpi_radiotap_tx_hdr *rtap;

		rtap = &lhw->rtap_tx;
		rtap->wt_flags = 0;
		if (k != NULL)
			rtap->wt_flags |= IEEE80211_RADIOTAP_F_WEP;
		if (m->m_flags & M_FRAG)
			rtap->wt_flags |= IEEE80211_RADIOTAP_F_FRAG;
		IMPROVE();
		rtap->wt_rate = 0;
		if (c != NULL && c != IEEE80211_CHAN_ANYC) {
			rtap->wt_chan_freq = htole16(c->ic_freq);
			rtap->wt_chan_flags = htole16(c->ic_flags);
		}

		ieee80211_radiotap_tx(ni->ni_vap, m);
	}

	/*
	 * net80211 should handle hw->extra_tx_headroom.
	 * Though for as long as we are copying we don't mind.
	 */
	skb = dev_alloc_skb(hw->extra_tx_headroom + m->m_pkthdr.len);
	if (skb == NULL) {
		printf("XXX ERROR %s: skb alloc failed\n", __func__);
		ieee80211_free_node(ni);
		m_freem(m);
		return;
	}
	skb_reserve(skb, hw->extra_tx_headroom);

	/* XXX-BZ we need a SKB version understanding mbuf. */
	/* Save the mbuf for ieee80211_tx_complete(). */
	skb->m_free_func = lkpi_ieee80211_free_skb_mbuf;
	skb->m = m;
#if 0
	skb_put_data(skb, m->m_data, m->m_pkthdr.len);
#else
	buf = skb_put(skb, m->m_pkthdr.len);
	m_copydata(m, 0, m->m_pkthdr.len, buf);
#endif
	/* Save the ni. */
	m->m_pkthdr.PH_loc.ptr = ni;

	lvif = VAP_TO_LVIF(ni->ni_vap);
	vif = LVIF_TO_VIF(lvif);

	/* XXX-BZ review this at some point [4] vs. [8] vs. [16](TID). */
	ac = M_WME_GETAC(m);
	skb->priority = WME_AC_TO_TID(ac);
	ac = lkpi_ac_net_to_l80211(ac);
	skb_set_queue_mapping(skb, ac);

	info = IEEE80211_SKB_CB(skb);
	info->flags |= IEEE80211_TX_CTL_REQ_TX_STATUS;
	/* Slight delay; probably only happens on scanning so fine? */
	if (c == NULL || c == IEEE80211_CHAN_ANYC)
		c = ic->ic_curchan;
	info->band = lkpi_net80211_chan_to_nl80211_band(c);
	info->hw_queue = ac;		/* XXX-BZ is this correct? */
	if (m->m_flags & M_EAPOL)
		info->control.flags |= IEEE80211_TX_CTRL_PORT_CTRL_PROTO;
	info->control.vif = vif;
	/* XXX-BZ info->control.rates */

	lsta = lkpi_find_lsta_by_ni(lvif, ni);
	if (lsta != NULL) {
		sta = LSTA_TO_STA(lsta);
#ifdef TRY_HW_CRYPTO
		info->control.hw_key = lsta->kc;
#endif
	} else {
		sta = NULL;
	}

	IMPROVE();

	if (sta != NULL) {
		struct lkpi_txq *ltxq;

		ltxq = TXQ_TO_LTXQ(sta->txq[ac]);	/* XXX-BZ re-check */
		/*
		 * We currently do not use queues but do direct TX.
		 * The exception to the rule is initial packets, as we cannot
		 * TX until queues are allocated (at least for iwlwifi).
		 * So we wake_tx_queue in newstate and register any dequeue
		 * calls.  In the time until then we queue packets and
		 * let the driver deal with them.
		 */
		if (!ltxq->seen_dequeue) {

			/* Prevent an ordering problem, likely other issues. */
			while (!skb_queue_empty(&ltxq->skbq)) {
				struct sk_buff *skb2;

				skb2 = skb_dequeue(&ltxq->skbq);
				if (skb2 != NULL) {
					memset(&control, 0, sizeof(control));
					control.sta = sta;
					lkpi_80211_mo_tx(hw, &control, skb2);
				}
			}
			goto ops_tx;
		}
		if (0 && ltxq->seen_dequeue && skb_queue_empty(&ltxq->skbq))
			goto ops_tx;

		skb_queue_tail(&ltxq->skbq, skb);
		if (debug_80211 & D80211_TRACE_TX)
			printf("%s:%d lsta %p sta %p ni %p %6D skb %p lxtq %p "
			    "qlen %u WAKE_TX_Q ac %d prio %u qmap %u\n",
			    __func__, __LINE__, lsta, sta, ni,
			    ni->ni_macaddr, ":", skb, ltxq,
			    skb_queue_len(&ltxq->skbq), ac,
			    skb->priority, skb->qmap);
		lkpi_80211_mo_wake_tx_queue(hw, sta->txq[ac]);	/* XXX-BZ */
		return;
	}

ops_tx:
	if (debug_80211 & D80211_TRACE_TX)
		printf("%s:%d lsta %p sta %p ni %p %6D skb %p TX ac %d prio %u qmap %u\n",
		    __func__, __LINE__, lsta, sta, ni, ni->ni_macaddr, ":",
		    skb, ac, skb->priority, skb->qmap);
	memset(&control, 0, sizeof(control));
	control.sta = sta;

	lkpi_80211_mo_tx(hw, &control, skb);
	return;
}

static void
lkpi_80211_txq_task(void *ctx, int pending)
{
	struct lkpi_sta *lsta;
	struct ieee80211_node *ni;
	struct mbufq mq;
	struct mbuf *m;

	lsta = ctx;
	ni = lsta->ni;

	if (debug_80211 & D80211_TRACE_TX)
		printf("%s:%d lsta %p ni %p %6D pending %d mbuf_qlen %d\n",
		    __func__, __LINE__, lsta, ni, ni->ni_macaddr, ":",
		    pending, mbufq_len(&lsta->txq));

	mbufq_init(&mq, IFQ_MAXLEN);

	LKPI_80211_LSTA_LOCK(lsta);
	mbufq_concat(&mq, &lsta->txq);
	LKPI_80211_LSTA_UNLOCK(lsta);

	m = mbufq_dequeue(&mq);
	while (m != NULL) {
		lkpi_80211_txq_tx_one(lsta, m);
		m = mbufq_dequeue(&mq);
	}
}

static int
lkpi_ic_transmit(struct ieee80211com *ic, struct mbuf *m)
{

	/* XXX TODO */
	IMPROVE();

	/* Quick and dirty cheating hack. */
	struct ieee80211_node *ni;

	ni = (struct ieee80211_node *)m->m_pkthdr.rcvif;
	return (lkpi_ic_raw_xmit(ni, m, NULL));
}

static void
lkpi_ic_getradiocaps(struct ieee80211com *ic, int maxchan,
    int *n, struct ieee80211_channel *c)
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct linuxkpi_ieee80211_channel *channels;
	uint8_t bands[IEEE80211_MODE_BYTES];
	int chan_flags, error, i, nchans;

	/* Channels */
	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);

	/* NL80211_BAND_2GHZ */
	nchans = 0;
	if (hw->wiphy->bands[NL80211_BAND_2GHZ] != NULL)
		nchans = hw->wiphy->bands[NL80211_BAND_2GHZ]->n_channels;
	if (nchans > 0) {
		memset(bands, 0, sizeof(bands));
		chan_flags = 0;
		setbit(bands, IEEE80211_MODE_11B);
		/* XXX-BZ unclear how to check for 11g. */
		setbit(bands, IEEE80211_MODE_11G);
#ifdef __notyet__
		if (hw->wiphy->bands[NL80211_BAND_2GHZ]->ht_cap.ht_supported) {
			setbit(bands, IEEE80211_MODE_11NG);
			chan_flags |= NET80211_CBW_FLAG_HT40;
		}
#endif

		channels = hw->wiphy->bands[NL80211_BAND_2GHZ]->channels;
		for (i = 0; i < nchans; i++) {
			uint32_t nflags = 0;
			int cflags = chan_flags;

			if (channels[i].flags & IEEE80211_CHAN_DISABLED) {
				printf("%s: %s: Skipping disabled chan "
				    "[%u/%u/%#x]\n", ic->ic_name, __func__,
				    channels[i].hw_value,
				    channels[i].center_freq, channels[i].flags);
				continue;
			}
			if (channels[i].flags & IEEE80211_CHAN_NO_IR)
				nflags |= (IEEE80211_CHAN_NOADHOC|IEEE80211_CHAN_PASSIVE);
			if (channels[i].flags & IEEE80211_CHAN_RADAR)
				nflags |= IEEE80211_CHAN_DFS;
			if (channels[i].flags & IEEE80211_CHAN_NO_160MHZ)
				cflags &= ~(NET80211_CBW_FLAG_VHT160|NET80211_CBW_FLAG_VHT80P80);
			if (channels[i].flags & IEEE80211_CHAN_NO_80MHZ)
				cflags &= ~NET80211_CBW_FLAG_VHT80;
			/* XXX how to map the remaining enum ieee80211_channel_flags? */
			if (channels[i].flags & IEEE80211_CHAN_NO_HT40)
				cflags &= ~NET80211_CBW_FLAG_HT40;

			error = ieee80211_add_channel_cbw(c, maxchan, n,
			    channels[i].hw_value, channels[i].center_freq,
			    channels[i].max_power,
			    nflags, bands, chan_flags);
			if (error != 0) {
				printf("%s: %s: Adding chan %u/%u/%#x/%#x/%#x/%#x "
				    "returned error %d\n", ic->ic_name,
				    __func__, channels[i].hw_value,
				    channels[i].center_freq, channels[i].flags,
				    nflags, chan_flags, cflags, error);
				break;
			}
		}
	}

	/* NL80211_BAND_5GHZ */
	nchans = 0;
	if (hw->wiphy->bands[NL80211_BAND_5GHZ] != NULL)
		nchans = hw->wiphy->bands[NL80211_BAND_5GHZ]->n_channels;
	if (nchans > 0) {
		memset(bands, 0, sizeof(bands));
		chan_flags = 0;
		setbit(bands, IEEE80211_MODE_11A);
#ifdef __not_yet__
		if (hw->wiphy->bands[NL80211_BAND_5GHZ]->ht_cap.ht_supported) {
			setbit(bands, IEEE80211_MODE_11NA);
			chan_flags |= NET80211_CBW_FLAG_HT40;
		}
		if (hw->wiphy->bands[NL80211_BAND_5GHZ]->vht_cap.vht_supported){

			ic->ic_flags_ext |= IEEE80211_FEXT_VHT;
			ic->ic_vhtcaps =
			    hw->wiphy->bands[NL80211_BAND_5GHZ]->vht_cap.cap;

			setbit(bands, IEEE80211_MODE_VHT_5GHZ);
			chan_flags |= NET80211_CBW_FLAG_VHT80;
			if (IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_IS_160MHZ(
			    ic->ic_vhtcaps))
				chan_flags |= NET80211_CBW_FLAG_VHT160;
			if (IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_IS_160_80P80MHZ(
			    ic->ic_vhtcaps))
				chan_flags |= NET80211_CBW_FLAG_VHT80P80;
		}
#endif

		channels = hw->wiphy->bands[NL80211_BAND_5GHZ]->channels;
		for (i = 0; i < nchans; i++) {
			uint32_t nflags = 0;
			int cflags = chan_flags;

			if (channels[i].flags & IEEE80211_CHAN_DISABLED) {
				printf("%s: %s: Skipping disabled chan "
				    "[%u/%u/%#x]\n", ic->ic_name, __func__,
				    channels[i].hw_value,
				    channels[i].center_freq, channels[i].flags);
				continue;
			}
			if (channels[i].flags & IEEE80211_CHAN_NO_IR)
				nflags |= (IEEE80211_CHAN_NOADHOC|IEEE80211_CHAN_PASSIVE);
			if (channels[i].flags & IEEE80211_CHAN_RADAR)
				nflags |= IEEE80211_CHAN_DFS;
			if (channels[i].flags & IEEE80211_CHAN_NO_160MHZ)
				cflags &= ~(NET80211_CBW_FLAG_VHT160|NET80211_CBW_FLAG_VHT80P80);
			if (channels[i].flags & IEEE80211_CHAN_NO_80MHZ)
				cflags &= ~NET80211_CBW_FLAG_VHT80;
			/* XXX hwo to map the remaining enum ieee80211_channel_flags? */
			if (channels[i].flags & IEEE80211_CHAN_NO_HT40)
				cflags &= ~NET80211_CBW_FLAG_HT40;

			error = ieee80211_add_channel_cbw(c, maxchan, n,
			    channels[i].hw_value, channels[i].center_freq,
			    channels[i].max_power,
			    nflags, bands, chan_flags);
			if (error != 0) {
				printf("%s: %s: Adding chan %u/%u/%#x/%#x/%#x/%#x "
				    "returned error %d\n", ic->ic_name,
				    __func__, channels[i].hw_value,
				    channels[i].center_freq, channels[i].flags,
				    nflags, chan_flags, cflags, error);
				break;
			}
		}
	}
}

static void *
lkpi_ieee80211_ifalloc(void)
{
	struct ieee80211com *ic;

	ic = malloc(sizeof(*ic), M_LKPI80211, M_WAITOK | M_ZERO);
	if (ic == NULL)
		return (NULL);

	/* Setting these happens later when we have device information. */
	ic->ic_softc = NULL;
	ic->ic_name = "linuxkpi";

	return (ic);
}

struct ieee80211_hw *
linuxkpi_ieee80211_alloc_hw(size_t priv_len, const struct ieee80211_ops *ops)
{
	struct ieee80211_hw *hw;
	struct lkpi_hw *lhw;
	struct wiphy *wiphy;

	/* Get us and the driver data also allocated. */
	wiphy = wiphy_new(&linuxkpi_mac80211cfgops, sizeof(*lhw) + priv_len);
	if (wiphy == NULL)
		return (NULL);

	lhw = wiphy_priv(wiphy);
	lhw->ops = ops;
	lhw->workq = alloc_ordered_workqueue(wiphy_name(wiphy), 0);
	if (lhw->workq == NULL) {
		wiphy_free(wiphy);
		return (NULL);
	}
	mtx_init(&lhw->mtx, "lhw", NULL, MTX_DEF | MTX_RECURSE);
	TAILQ_INIT(&lhw->lvif_head);

	/*
	 * XXX-BZ TODO make sure there is a "_null" function to all ops
	 * not initialized.
	 */
	hw = LHW_TO_HW(lhw);
	hw->wiphy = wiphy;
	hw->conf.flags |= IEEE80211_CONF_IDLE;
	hw->priv = (void *)(lhw + 1);

	/* BSD Specific. */
	lhw->ic = lkpi_ieee80211_ifalloc();
	if (lhw->ic == NULL) {
		ieee80211_free_hw(hw);
		return (NULL);
	}

	IMPROVE();

	return (hw);
}

void
linuxkpi_ieee80211_iffree(struct ieee80211_hw *hw)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);
	free(lhw->ic, M_LKPI80211);
	lhw->ic = NULL;

	/* Cleanup more of lhw here or in wiphy_free()? */
	mtx_destroy(&lhw->mtx);
	IMPROVE();
}

void
linuxkpi_set_ieee80211_dev(struct ieee80211_hw *hw, char *name)
{
	struct lkpi_hw *lhw;
	struct ieee80211com *ic;

	lhw = HW_TO_LHW(hw);
	ic = lhw->ic;

	/* Now set a proper name before ieee80211_ifattach(). */
	ic->ic_softc = lhw;
	ic->ic_name = name;

	/* XXX-BZ do we also need to set wiphy name? */
}

struct ieee80211_hw *
linuxkpi_wiphy_to_ieee80211_hw(struct wiphy *wiphy)
{
	struct lkpi_hw *lhw;

	lhw = wiphy_priv(wiphy);
	return (LHW_TO_HW(lhw));
}

static void
lkpi_radiotap_attach(struct lkpi_hw *lhw)
{
	struct ieee80211com *ic;

	ic = lhw->ic;
	ieee80211_radiotap_attach(ic,
	    &lhw->rtap_tx.wt_ihdr, sizeof(lhw->rtap_tx),
	    LKPI_RTAP_TX_FLAGS_PRESENT,
	    &lhw->rtap_rx.wr_ihdr, sizeof(lhw->rtap_rx),
	    LKPI_RTAP_RX_FLAGS_PRESENT);
}

void
linuxkpi_ieee80211_ifattach(struct ieee80211_hw *hw)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
#ifdef TRY_HW_CRYPTO
	int i;
#endif

	lhw = HW_TO_LHW(hw);
	ic = lhw->ic;

	/* XXX-BZ figure this out how they count his... */
	if (!is_zero_ether_addr(hw->wiphy->perm_addr)) {
		IEEE80211_ADDR_COPY(ic->ic_macaddr,
		    hw->wiphy->perm_addr);
	} else if (hw->wiphy->n_addresses > 0) {
		/* We take the first one. */
		IEEE80211_ADDR_COPY(ic->ic_macaddr,
		    hw->wiphy->addresses[0].addr);
	} else {
		ic_printf(ic, "%s: warning, no hardware address!\n", __func__);
	}

	ic->ic_headroom = hw->extra_tx_headroom;

	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;

	/* Set device capabilities. */
	/* XXX-BZ we need to get these from linux80211/drivers and convert. */
	ic->ic_caps =
	    IEEE80211_C_STA |
	    IEEE80211_C_MONITOR |
	    IEEE80211_C_WPA |		/* WPA/RSN */
	    IEEE80211_C_WME |
#if 0
	    IEEE80211_C_PMGT |
#endif
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_SHPREAMBLE	/* short preamble supported */
	    ;
#if 0
	/* Scanning is a different kind of beast to re-work. */
	ic->ic_caps |= IEEE80211_C_BGSCAN;
#endif
	if (lhw->ops->hw_scan &&
	    ieee80211_hw_check(hw, SINGLE_SCAN_ON_ALL_BANDS)) {
		/* Advertise full-offload scanning */
		ic->ic_flags_ext |= IEEE80211_FEXT_SCAN_OFFLOAD;
	}

#ifdef __notyet__
	ic->ic_htcaps = IEEE80211_HTC_HT        /* HT operation */
		    | IEEE80211_HTC_AMPDU       /* A-MPDU tx/rx */
		    | IEEE80211_HTC_AMSDU       /* A-MSDU tx/rx */
		    | IEEE80211_HTCAP_MAXAMSDU_3839
						/* max A-MSDU length */
		    | IEEE80211_HTCAP_SMPS_OFF; /* SM power save off */
	ic->ic_htcaps |= IEEE80211_HTCAP_SHORTGI20;
	ic->ic_htcaps |= IEEE80211_HTCAP_CHWIDTH40 | IEEE80211_HTCAP_SHORTGI40;
	ic->ic_htcaps |= IEEE80211_HTCAP_TXSTBC;
#endif

	ic->ic_cryptocaps = 0;
#ifdef TRY_HW_CRYPTO
	if (hw->wiphy->n_cipher_suites > 0) {
		for (i = 0; i < hw->wiphy->n_cipher_suites; i++)
			ic->ic_cryptocaps |= lkpi_l80211_to_net80211_cyphers(
			    hw->wiphy->cipher_suites[i]);
	}
#endif

	lkpi_ic_getradiocaps(ic, IEEE80211_CHAN_MAX, &ic->ic_nchans,
	    ic->ic_channels);

	ieee80211_ifattach(ic);

	ic->ic_update_mcast = lkpi_ic_update_mcast;
	ic->ic_update_promisc = lkpi_ic_update_promisc;
	ic->ic_update_chw = lkpi_ic_update_chw;
	ic->ic_parent = lkpi_ic_parent;
	ic->ic_scan_start = lkpi_ic_scan_start;
	ic->ic_scan_end = lkpi_ic_scan_end;
	if (lhw->ops->hw_scan &&
	    ieee80211_hw_check(hw, SINGLE_SCAN_ON_ALL_BANDS))
		ic->ic_scan_curchan = lkpi_ic_scan_curchan_nada;
	ic->ic_set_channel = lkpi_ic_set_channel;
	ic->ic_transmit = lkpi_ic_transmit;
	ic->ic_raw_xmit = lkpi_ic_raw_xmit;
	ic->ic_vap_create = lkpi_ic_vap_create;
	ic->ic_vap_delete = lkpi_ic_vap_delete;
	ic->ic_getradiocaps = lkpi_ic_getradiocaps;
	ic->ic_wme.wme_update = lkpi_ic_wme_update;

	lhw->ic_node_alloc = ic->ic_node_alloc;
	ic->ic_node_alloc = lkpi_ic_node_alloc;
	lhw->ic_node_init = ic->ic_node_init;
	ic->ic_node_init = lkpi_ic_node_init;
	lhw->ic_node_cleanup = ic->ic_node_cleanup;
	ic->ic_node_cleanup = lkpi_ic_node_cleanup;
	lhw->ic_node_free = ic->ic_node_free;
	ic->ic_node_free = lkpi_ic_node_free;

	lkpi_radiotap_attach(lhw);

	if (bootverbose)
		ieee80211_announce(ic);
}

void
linuxkpi_ieee80211_ifdetach(struct ieee80211_hw *hw)
{
	struct lkpi_hw *lhw;
	struct ieee80211com *ic;

	lhw = HW_TO_LHW(hw);
	ic = lhw->ic;
	ieee80211_ifdetach(ic);
}

void
linuxkpi_ieee80211_iterate_interfaces(struct ieee80211_hw *hw,
    enum ieee80211_iface_iter flags,
    void(*iterfunc)(void *, uint8_t *, struct ieee80211_vif *),
    void *arg)
{
	struct lkpi_hw *lhw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	bool active, atomic;

	lhw = HW_TO_LHW(hw);

	if (flags & ~(IEEE80211_IFACE_ITER_NORMAL|
	    IEEE80211_IFACE_ITER_RESUME_ALL|
	    IEEE80211_IFACE_ITER__ACTIVE|IEEE80211_IFACE_ITER__ATOMIC)) {
		ic_printf(lhw->ic, "XXX TODO %s flags(%#x) not yet supported.\n",
		    __func__, flags);
	}

	active = (flags & IEEE80211_IFACE_ITER__ACTIVE) != 0;
	atomic = (flags & IEEE80211_IFACE_ITER__ATOMIC) != 0;

	if (atomic)
		LKPI_80211_LHW_LOCK(lhw);
	TAILQ_FOREACH(lvif, &lhw->lvif_head, lvif_entry) {
		struct ieee80211vap *vap;

		vif = LVIF_TO_VIF(lvif);

		/*
		 * If we want "active" interfaces, we need to distinguish on
		 * whether the driver knows about them or not to be able to
		 * handle the "resume" case correctly.  Skip the ones the
		 * driver does not know about.
		 */
		if (active && !lvif->added_to_drv &&
		    (flags & IEEE80211_IFACE_ITER_RESUME_ALL) != 0)
			continue;

		/*
		 * Run the iterator function if we are either not asking
		 * asking for active only or if the VAP is "running".
		 */
		/* XXX-BZ probably should have state in the lvif as well. */
		vap = LVIF_TO_VAP(lvif);
		if (!active || (vap->iv_state != IEEE80211_S_INIT))
			iterfunc(arg, vif->addr, vif);
	}
	if (atomic)
		LKPI_80211_LHW_UNLOCK(lhw);
}

void
linuxkpi_ieee80211_iterate_keys(struct ieee80211_hw *hw,
    struct ieee80211_vif *vif,
    void(*iterfunc)(struct ieee80211_hw *, struct ieee80211_vif *,
        struct ieee80211_sta *, struct ieee80211_key_conf *, void *),
    void *arg)
{

	UNIMPLEMENTED;
}

void
linuxkpi_ieee80211_iterate_chan_contexts(struct ieee80211_hw *hw,
    void(*iterfunc)(struct ieee80211_hw *, struct ieee80211_chanctx_conf *,
	void *),
    void *arg)
{

	UNIMPLEMENTED;
}

void
linuxkpi_ieee80211_iterate_stations_atomic(struct ieee80211_hw *hw,
   void (*iterfunc)(void *, struct ieee80211_sta *), void *arg)
{
	struct lkpi_hw *lhw;
	struct lkpi_vif *lvif;
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;

	KASSERT(hw != NULL && iterfunc != NULL,
	    ("%s: hw %p iterfunc %p arg %p\n", __func__, hw, iterfunc, arg));

	lhw = HW_TO_LHW(hw);

	LKPI_80211_LHW_LOCK(lhw);
	TAILQ_FOREACH(lvif, &lhw->lvif_head, lvif_entry) {

		LKPI_80211_LVIF_LOCK(lvif);
		TAILQ_FOREACH(lsta, &lvif->lsta_head, lsta_entry) {
			if (!lsta->added_to_drv)
				continue;
			sta = LSTA_TO_STA(lsta);
			iterfunc(arg, sta);
		}
		LKPI_80211_LVIF_UNLOCK(lvif);
	}
	LKPI_80211_LHW_UNLOCK(lhw);
}

int
linuxkpi_regulatory_set_wiphy_regd_sync(struct wiphy *wiphy,
    struct linuxkpi_ieee80211_regdomain *regd)
{
	struct lkpi_hw *lhw;
	struct ieee80211com *ic;
	struct ieee80211_regdomain *rd;

	lhw = wiphy_priv(wiphy);
	ic = lhw->ic;

	rd = &ic->ic_regdomain;
	if (rd->isocc[0] == '\0') {
		rd->isocc[0] = regd->alpha2[0];
		rd->isocc[1] = regd->alpha2[1];
	}

	TODO();
	/* XXX-BZ finish the rest. */

	return (0);
}

void
linuxkpi_ieee80211_scan_completed(struct ieee80211_hw *hw,
    struct cfg80211_scan_info *info)
{
	struct lkpi_hw *lhw;
	struct ieee80211com *ic;
	struct ieee80211_scan_state *ss;

	lhw = wiphy_priv(hw->wiphy);
	ic = lhw->ic;
	ss = ic->ic_scan;

	ieee80211_scan_done(ss->ss_vap);

	LKPI_80211_LHW_LOCK(lhw);
	free(lhw->hw_req->ies.common_ies, M_80211_VAP);
	free(lhw->hw_req, M_LKPI80211);
	lhw->hw_req = NULL;
	lhw->scan_flags &= ~LKPI_SCAN_RUNNING;
	wakeup(lhw);
	LKPI_80211_LHW_UNLOCK(lhw);

	return;
}

void
linuxkpi_ieee80211_rx(struct ieee80211_hw *hw, struct sk_buff *skb,
    struct ieee80211_sta *sta, struct napi_struct *napi __unused)
{
	struct epoch_tracker et;
	struct lkpi_hw *lhw;
	struct ieee80211com *ic;
	struct mbuf *m;
	struct skb_shared_info *shinfo;
	struct ieee80211_rx_status *rx_status;
	struct ieee80211_rx_stats rx_stats;
	struct ieee80211_node *ni;
	struct ieee80211vap *vap;
	struct ieee80211_frame_min *wh;
	struct ieee80211_hdr *hdr;
	struct lkpi_sta *lsta;
	int i, offset, ok, type;

	if (skb->len < 2) {
		/* Need 80211 stats here. */
		IMPROVE();
		goto err;
	}

	/*
	 * For now do the data copy; we can later improve things. Might even
	 * have an mbuf backing the skb data then?
	 */
	m = m_get2(skb->len, M_NOWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		goto err;
	m_copyback(m, 0, skb->tail - skb->data, skb->data);

	shinfo = skb_shinfo(skb);
	offset = m->m_len;
	for (i = 0; i < shinfo->nr_frags; i++) {
		m_copyback(m, offset, shinfo->frags[i].size,
		    (uint8_t *)linux_page_address(shinfo->frags[i].page) +
		    shinfo->frags[i].offset);
		offset += shinfo->frags[i].size;
	}

	rx_status = IEEE80211_SKB_RXCB(skb);

#ifdef LINUXKPI_DEBUG_80211
	hdr = (void *)skb->data;
	if ((debug_80211 & D80211_TRACE_RX_BEACONS) == 0 &&
	    ieee80211_is_beacon(hdr->frame_control))
		goto no_trace_beacons;

	if (debug_80211 & D80211_TRACE_RX)
		printf("TRACE-RX: %s: skb %p a/l/d/t-len (%u/%u/%u/%u) "
		    "h %p d %p t %p e %p sh %p (%u) m %p plen %u len %u\n",
		    __func__, skb, skb->_alloc_len, skb->len, skb->data_len,
		    skb->truesize, skb->head, skb->data, skb->tail, skb->end,
		    shinfo, shinfo->nr_frags,
		    m, m->m_pkthdr.len, m->m_len);

	if (debug_80211 & D80211_TRACE_RX_DUMP)
		hexdump(mtod(m, const void *), m->m_len, "RX (raw) ", 0);

	/* Implement a dump_rxcb() !!! */
	if (debug_80211 & D80211_TRACE_RX)
		printf("TRACE %s: RXCB: %u %u %u, %#0x, %u, %#0x, %#0x, "
		    "%u band %u, %u %u %u %u, %u, %#x %#x %#x %#x %u %u %u\n",
			__func__,
			rx_status->boottime_ns,
			rx_status->mactime,
			rx_status->device_timestamp,
			rx_status->flag,
			rx_status->freq,
			rx_status->bw,
			rx_status->encoding,
			rx_status->ampdu_reference,
			rx_status->band,
			rx_status->chains,
			rx_status->chain_signal[0],
			rx_status->chain_signal[1],
			rx_status->chain_signal[2],
			rx_status->signal,
			rx_status->enc_flags,
			rx_status->he_dcm,
			rx_status->he_gi,
			rx_status->he_ru,
			rx_status->zero_length_psdu_type,
			rx_status->nss,
			rx_status->rate_idx);
no_trace_beacons:
#endif

	memset(&rx_stats, 0, sizeof(rx_stats));
	rx_stats.r_flags = IEEE80211_R_NF | IEEE80211_R_RSSI;
	if (ieee80211_hw_check(hw, SIGNAL_DBM) &&
	    !(rx_status->flag & RX_FLAG_NO_SIGNAL_VAL))
		rx_stats.c_rssi = rx_status->signal;
	else
		rx_stats.c_rssi = 0;			/* XXX */
	rx_stats.c_nf = -96;				/* XXX */
	rx_stats.r_flags |= IEEE80211_R_BAND;
	rx_stats.c_band =
	    lkpi_nl80211_band_to_net80211_band(rx_status->band);
	rx_stats.r_flags |= IEEE80211_R_FREQ | IEEE80211_R_IEEE;
	rx_stats.c_freq = rx_status->freq;
	rx_stats.c_ieee = ieee80211_mhz2ieee(rx_stats.c_freq, rx_stats.c_band);

	/* XXX-BZ correct hardcoded rssi and noise floor. */
	/* XXX (*sta_statistics)() to get to some of that? */
	/* XXX-BZ dump the FreeBSD version of rx_stats as well! */

	lhw = HW_TO_LHW(hw);
	ic = lhw->ic;

	ok = ieee80211_add_rx_params(m, &rx_stats);
	if (ok == 0) {
		counter_u64_add(ic->ic_ierrors, 1);
		goto err;
	}

	if (sta != NULL) {
		lsta = STA_TO_LSTA(sta);
		ni = ieee80211_ref_node(lsta->ni);
	} else {
		wh = mtod(m, struct ieee80211_frame_min *);
		ni = ieee80211_find_rxnode(ic, wh);
		if (ni != NULL)
			lsta = ni->ni_drv_data;
	}

	if (ni != NULL)
		vap = ni->ni_vap;
	else
		/*
		 * XXX-BZ can we improve this by looking at the frame hdr
		 * or other meta-data passed up?
		 */
		vap = TAILQ_FIRST(&ic->ic_vaps);

	if (debug_80211 & D80211_TRACE_RX)
		printf("TRACE %s: sta %p lsta %p ni %p vap %p\n", __func__, sta, lsta, ni, vap);

	if (vap != NULL && vap->iv_state > IEEE80211_S_INIT &&
	    ieee80211_radiotap_active_vap(vap)) {
		struct lkpi_radiotap_rx_hdr *rtap;

		rtap = &lhw->rtap_rx;
		rtap->wr_tsft = rx_status->device_timestamp;
		rtap->wr_flags = 0;
		if (rx_status->enc_flags & RX_ENC_FLAG_SHORTPRE)
			rtap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTPRE;
		if (rx_status->enc_flags & RX_ENC_FLAG_SHORT_GI)
			rtap->wr_flags |= IEEE80211_RADIOTAP_F_SHORTGI;
#if 0	/* .. or it does not given we strip it below. */
		if (ieee80211_hw_check(hw, RX_INCLUDES_FCS))
			rtap->wr_flags |= IEEE80211_RADIOTAP_F_FCS;
#endif
		if (rx_status->flag & RX_FLAG_FAILED_FCS_CRC)
			rtap->wr_flags |= IEEE80211_RADIOTAP_F_BADFCS;
		rtap->wr_rate = 0;
		IMPROVE();
		/* XXX TODO status->encoding / rate_index / bw */
		rtap->wr_chan_freq = htole16(rx_stats.c_freq);
		if (ic->ic_curchan->ic_ieee == rx_stats.c_ieee)
			rtap->wr_chan_flags = htole16(ic->ic_curchan->ic_flags);
		rtap->wr_dbm_antsignal = rx_stats.c_rssi;
		rtap->wr_dbm_antnoise = rx_stats.c_nf;
	}

	if (ieee80211_hw_check(hw, RX_INCLUDES_FCS))
		m_adj(m, -IEEE80211_CRC_LEN);

	NET_EPOCH_ENTER(et);
	if (ni != NULL) {
		type = ieee80211_input_mimo(ni, m);
		ieee80211_free_node(ni);
	} else {
		type = ieee80211_input_mimo_all(ic, m);
	}
	NET_EPOCH_EXIT(et);

	if (debug_80211 & D80211_TRACE_RX)
		printf("TRACE %s: handled frame type %#0x\n", __func__, type);

	IMPROVE();

err:
	/* The skb is ours so we can free it :-) */
	kfree_skb(skb);
}

uint8_t
linuxkpi_ieee80211_get_tid(struct ieee80211_hdr *hdr)
{
	const struct ieee80211_frame *wh;

	wh = (const struct ieee80211_frame *)hdr;
	return (ieee80211_gettid(wh));
}

struct wiphy *
linuxkpi_wiphy_new(const struct cfg80211_ops *ops, size_t priv_len)
{
	struct lkpi_wiphy *lwiphy;

	lwiphy = kzalloc(sizeof(*lwiphy) + priv_len, GFP_KERNEL);
	if (lwiphy == NULL)
		return (NULL);
	lwiphy->ops = ops;

	/* XXX TODO */
	return (LWIPHY_TO_WIPHY(lwiphy));
}

void
linuxkpi_wiphy_free(struct wiphy *wiphy)
{
	struct lkpi_wiphy *lwiphy;

	if (wiphy == NULL)
		return;

	lwiphy = WIPHY_TO_LWIPHY(wiphy);
	kfree(lwiphy);
}

uint32_t
linuxkpi_ieee80211_channel_to_frequency(uint32_t channel,
    enum nl80211_band band)
{

	switch (band) {
	case NL80211_BAND_2GHZ:
		return (ieee80211_ieee2mhz(channel, IEEE80211_CHAN_2GHZ));
		break;
	case NL80211_BAND_5GHZ:
		return (ieee80211_ieee2mhz(channel, IEEE80211_CHAN_5GHZ));
		break;
	default:
		/* XXX abort, retry, error, panic? */
		break;
	}

	return (0);
}

uint32_t
linuxkpi_ieee80211_frequency_to_channel(uint32_t freq, uint32_t flags __unused)
{

	return (ieee80211_mhz2ieee(freq, 0));
}

static struct lkpi_sta *
lkpi_find_lsta_by_ni(struct lkpi_vif *lvif, struct ieee80211_node *ni)
{
	struct lkpi_sta *lsta, *temp;

	LKPI_80211_LVIF_LOCK(lvif);
	TAILQ_FOREACH_SAFE(lsta, &lvif->lsta_head, lsta_entry, temp) {
		if (lsta->ni == ni) {
			LKPI_80211_LVIF_UNLOCK(lvif);
			return (lsta);
		}
	}
	LKPI_80211_LVIF_UNLOCK(lvif);

	return (NULL);
}

struct ieee80211_sta *
linuxkpi_ieee80211_find_sta(struct ieee80211_vif *vif, const u8 *peer)
{
	struct lkpi_vif *lvif;
	struct lkpi_sta *lsta, *temp;
	struct ieee80211_sta *sta;

	lvif = VIF_TO_LVIF(vif);

	LKPI_80211_LVIF_LOCK(lvif);
	TAILQ_FOREACH_SAFE(lsta, &lvif->lsta_head, lsta_entry, temp) {
		sta = LSTA_TO_STA(lsta);
		if (IEEE80211_ADDR_EQ(sta->addr, peer)) {
			LKPI_80211_LVIF_UNLOCK(lvif);
			return (sta);
		}
	}
	LKPI_80211_LVIF_UNLOCK(lvif);
	return (NULL);
}

struct ieee80211_sta *
linuxkpi_ieee80211_find_sta_by_ifaddr(struct ieee80211_hw *hw, uint8_t *addr,
    uint8_t *ourvifaddr)
{
	struct lkpi_hw *lhw;
	struct lkpi_vif *lvif;
	struct lkpi_sta *lsta;
	struct ieee80211_vif *vif;
	struct ieee80211_sta *sta;

	lhw = wiphy_priv(hw->wiphy);
	sta = NULL;

	LKPI_80211_LHW_LOCK(lhw);
	TAILQ_FOREACH(lvif, &lhw->lvif_head, lvif_entry) {

		/* XXX-BZ check our address from the vif. */

		vif = LVIF_TO_VIF(lvif);
		if (ourvifaddr != NULL &&
		    !IEEE80211_ADDR_EQ(vif->addr, ourvifaddr))
			continue;
		sta = linuxkpi_ieee80211_find_sta(vif, addr);
		if (sta != NULL)
			break;
	}
	LKPI_80211_LHW_UNLOCK(lhw);

	if (sta != NULL) {
		lsta = STA_TO_LSTA(sta);
		if (!lsta->added_to_drv)
			return (NULL);
	}

	return (sta);
}

struct sk_buff *
linuxkpi_ieee80211_tx_dequeue(struct ieee80211_hw *hw,
    struct ieee80211_txq *txq)
{
	struct lkpi_txq *ltxq;
	struct sk_buff *skb;

	ltxq = TXQ_TO_LTXQ(txq);
	ltxq->seen_dequeue = true;

	skb = skb_dequeue(&ltxq->skbq);

	return (skb);
}

void
linuxkpi_ieee80211_txq_get_depth(struct ieee80211_txq *txq,
    uint64_t *frame_cnt, uint64_t *byte_cnt)
{
	struct lkpi_txq *ltxq;
	struct sk_buff *skb;
	uint64_t fc, bc;

	ltxq = TXQ_TO_LTXQ(txq);

	fc = bc = 0;
	skb_queue_walk(&ltxq->skbq, skb) {
		fc++;
		bc += skb->len;
	}
	if (frame_cnt)
		*frame_cnt = fc;
	if (byte_cnt)
		*byte_cnt = bc;

	/* Validate that this is doing the correct thing. */
	/* Should we keep track on en/dequeue? */
	IMPROVE();
}

/*
 * We are called from ieee80211_free_txskb() or ieee80211_tx_status().
 * The latter tries to derive the success status from the info flags
 * passed back from the driver.  rawx_mit() saves the ni on the m and the
 * m on the skb for us to be able to give feedback to net80211.
 */
void
linuxkpi_ieee80211_free_txskb(struct ieee80211_hw *hw, struct sk_buff *skb,
    int status)
{
	struct ieee80211_node *ni;
	struct mbuf *m;

	m = skb->m;
	skb->m = NULL;

	if (m != NULL) {
		ni = m->m_pkthdr.PH_loc.ptr;
		/* Status: 0 is ok, != 0 is error. */
		ieee80211_tx_complete(ni, m, status);
		/* ni & mbuf were consumed. */
	}

	kfree_skb(skb);
}

/*
 * This is an internal bandaid for the moment for the way we glue
 * skbs and mbufs together for TX.  Once we have skbs backed by
 * mbufs this should go away.
 * This is a public function but kept on the private KPI (lkpi_)
 * and is not exposed by a header file.
 */
static void
lkpi_ieee80211_free_skb_mbuf(void *p)
{
	struct ieee80211_node *ni;
	struct mbuf *m;

	if (p == NULL)
		return;

	m = (struct mbuf *)p;
	M_ASSERTPKTHDR(m);

	ni = m->m_pkthdr.PH_loc.ptr;
	m->m_pkthdr.PH_loc.ptr = NULL;
	if (ni != NULL)
		ieee80211_free_node(ni);
	m_freem(m);
}

void
linuxkpi_ieee80211_queue_delayed_work(struct ieee80211_hw *hw,
    struct delayed_work *w, int delay)
{
	struct lkpi_hw *lhw;

	/* Need to make sure hw is in a stable (non-suspended) state. */
	IMPROVE();

	lhw = HW_TO_LHW(hw);
	queue_delayed_work(lhw->workq, w, delay);
}

void
linuxkpi_ieee80211_queue_work(struct ieee80211_hw *hw,
    struct work_struct *w)
{
	struct lkpi_hw *lhw;

	/* Need to make sure hw is in a stable (non-suspended) state. */
	IMPROVE();

	lhw = HW_TO_LHW(hw);
	queue_work(lhw->workq, w);
}

struct sk_buff *
linuxkpi_ieee80211_pspoll_get(struct ieee80211_hw *hw,
    struct ieee80211_vif *vif)
{
	struct lkpi_vif *lvif;
	struct ieee80211vap *vap;
	struct sk_buff *skb;
	struct ieee80211_frame_pspoll *psp;
	uint16_t v;

	skb = dev_alloc_skb(hw->extra_tx_headroom + sizeof(*psp));
	if (skb == NULL)
		return (NULL);

	skb_reserve(skb, hw->extra_tx_headroom);

	lvif = VIF_TO_LVIF(vif);
	vap = LVIF_TO_VAP(lvif);

	psp = skb_put_zero(skb, sizeof(*psp));
	psp->i_fc[0] = IEEE80211_FC0_VERSION_0;
	psp->i_fc[0] |= IEEE80211_FC0_SUBTYPE_PS_POLL | IEEE80211_FC0_TYPE_CTL;
	v = htole16(vif->bss_conf.aid | 1<<15 | 1<<16);
	memcpy(&psp->i_aid, &v, sizeof(v));
	IEEE80211_ADDR_COPY(psp->i_bssid, vap->iv_bss->ni_macaddr);
	IEEE80211_ADDR_COPY(psp->i_ta, vif->addr);

	return (skb);
}

struct sk_buff *
linuxkpi_ieee80211_nullfunc_get(struct ieee80211_hw *hw,
    struct ieee80211_vif *vif, bool qos)
{
	struct lkpi_vif *lvif;
	struct ieee80211vap *vap;
	struct sk_buff *skb;
	struct ieee80211_frame *nullf;

	skb = dev_alloc_skb(hw->extra_tx_headroom + sizeof(*nullf));
	if (skb == NULL)
		return (NULL);

	skb_reserve(skb, hw->extra_tx_headroom);

	lvif = VIF_TO_LVIF(vif);
	vap = LVIF_TO_VAP(lvif);

	nullf = skb_put_zero(skb, sizeof(*nullf));
	nullf->i_fc[0] = IEEE80211_FC0_VERSION_0;
	nullf->i_fc[0] |= IEEE80211_FC0_SUBTYPE_NODATA | IEEE80211_FC0_TYPE_DATA;
	nullf->i_fc[1] = IEEE80211_FC1_DIR_TODS;

	IEEE80211_ADDR_COPY(nullf->i_addr1, vap->iv_bss->ni_bssid);
	IEEE80211_ADDR_COPY(nullf->i_addr2, vif->addr);
	IEEE80211_ADDR_COPY(nullf->i_addr3, vap->iv_bss->ni_macaddr);

	return (skb);
}

struct wireless_dev *
linuxkpi_ieee80211_vif_to_wdev(struct ieee80211_vif *vif)
{
	struct lkpi_vif *lvif;

	lvif = VIF_TO_LVIF(vif);
	return (&lvif->wdev);
}

void
linuxkpi_ieee80211_connection_loss(struct ieee80211_vif *vif)
{
	struct lkpi_vif *lvif;
	struct ieee80211vap *vap;
	enum ieee80211_state nstate;
	int arg;

	lvif = VIF_TO_LVIF(vif);
	vap = LVIF_TO_VAP(lvif);

	/*
	 * Go to scan; otherwise we need to elaborately check state and
	 * handle accordingly, e.g., if in RUN we could call iv_bmiss.
	 * Let the statemachine handle all neccessary changes.
	 */
	nstate = IEEE80211_S_SCAN;
	arg = 0;

	if (debug_80211 & D80211_TRACE)
		ic_printf(vap->iv_ic, "%s: vif %p\n", __func__, vif);
	ieee80211_new_state(vap, nstate, arg);
}

MODULE_VERSION(linuxkpi_wlan, 1);
MODULE_DEPEND(linuxkpi_wlan, linuxkpi, 1, 1, 1);
MODULE_DEPEND(linuxkpi_wlan, wlan, 1, 1, 1);
