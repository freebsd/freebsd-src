/*-
 * Copyright (c) 2020-2025 The FreeBSD Foundation
 * Copyright (c) 2020-2025 Bjoern A. Zeeb
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

/*
 * TODO:
 * - lots :)
 * - HW_CRYPTO: we need a "keystore" and an ordered list for suspend/resume.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/libkern.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_proto.h>
#include <net80211/ieee80211_ratectl.h>
#include <net80211/ieee80211_radiotap.h>
#include <net80211/ieee80211_vht.h>

#define	LINUXKPI_NET80211
#include <net/mac80211.h>

#include <linux/workqueue.h>
#include <linux/rculist.h>
#include "linux_80211.h"

/* #define	LKPI_80211_USE_SCANLIST */
/* #define	LKPI_80211_BGSCAN */
#define	LKPI_80211_WME
#define	LKPI_80211_HW_CRYPTO
#define	LKPI_80211_HT
#define	LKPI_80211_VHT

#if defined(LKPI_80211_VHT) && !defined(LKPI_80211_HT)
#define	LKPI_80211_HT
#endif
#if defined(LKPI_80211_HT) && !defined(LKPI_80211_HW_CRYPTO)
#define	LKPI_80211_HW_CRYPTO
#endif

static MALLOC_DEFINE(M_LKPI80211, "lkpi80211", "LinuxKPI 80211 compat");

/* XXX-BZ really want this and others in queue.h */
#define	TAILQ_ELEM_INIT(elm, field) do {				\
	(elm)->field.tqe_next = NULL;					\
	(elm)->field.tqe_prev = NULL;					\
} while (0)

/* -------------------------------------------------------------------------- */

SYSCTL_DECL(_compat_linuxkpi);
SYSCTL_NODE(_compat_linuxkpi, OID_AUTO, 80211, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "LinuxKPI 802.11 compatibility layer");

static bool lkpi_order_scanlist = false;
SYSCTL_BOOL(_compat_linuxkpi_80211, OID_AUTO, order_scanlist, CTLFLAG_RW,
    &lkpi_order_scanlist, 0, "Enable LinuxKPI 802.11 scan list shuffeling");

#if defined(LKPI_80211_HW_CRYPTO)
static bool lkpi_hwcrypto = false;
SYSCTL_BOOL(_compat_linuxkpi_80211, OID_AUTO, hw_crypto, CTLFLAG_RDTUN,
    &lkpi_hwcrypto, 0, "Enable LinuxKPI 802.11 hardware crypto offload");

static bool lkpi_hwcrypto_tkip = false;
SYSCTL_BOOL(_compat_linuxkpi_80211, OID_AUTO, tkip, CTLFLAG_RDTUN,
    &lkpi_hwcrypto_tkip, 0, "Enable LinuxKPI 802.11 TKIP crypto offload");
#endif

/* Keep public for as long as header files are using it too. */
int linuxkpi_debug_80211;

#ifdef LINUXKPI_DEBUG_80211
SYSCTL_INT(_compat_linuxkpi_80211, OID_AUTO, debug, CTLFLAG_RWTUN,
    &linuxkpi_debug_80211, 0, "LinuxKPI 802.11 debug level");

#define	UNIMPLEMENTED		if (linuxkpi_debug_80211 & D80211_TODO)		\
    printf("XXX-TODO %s:%d: UNIMPLEMENTED\n", __func__, __LINE__)
#define	TRACEOK()		if (linuxkpi_debug_80211 & D80211_TRACEOK)	\
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

/* IEEE 802.11-05/0257r1 */
const uint8_t bridge_tunnel_header[6] = { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0xf8 };

/* IEEE 802.11e Table 20i-UP-to-AC mappings. */
static const uint8_t ieee80211e_up_to_ac[] = {
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

const struct cfg80211_ops linuxkpi_mac80211cfgops = {
	/*
	 * XXX TODO need a "glue layer" to link cfg80211 ops to
	 * mac80211 and to the driver or net80211.
	 * Can we pass some on 1:1? Need to compare the (*f)().
	 */
};

#if 0
static struct lkpi_sta *lkpi_find_lsta_by_ni(struct lkpi_vif *,
    struct ieee80211_node *);
#endif
static void lkpi_sw_scan_task(void *, int);
static void lkpi_80211_txq_tx_one(struct lkpi_sta *, struct mbuf *);
static void lkpi_80211_txq_task(void *, int);
static void lkpi_80211_lhw_rxq_task(void *, int);
static void lkpi_ieee80211_free_skb_mbuf(void *);
#ifdef LKPI_80211_WME
static int lkpi_wme_update(struct lkpi_hw *, struct ieee80211vap *, bool);
#endif
static void lkpi_ieee80211_wake_queues_locked(struct ieee80211_hw *);

static const char *
lkpi_rate_info_bw_to_str(enum rate_info_bw bw)
{

	switch (bw) {

        case RATE_INFO_BW_20:
		return ("20");
		break;
        case RATE_INFO_BW_5:
		return ("5");
		break;
        case RATE_INFO_BW_10:
		return ("10");
		break;
        case RATE_INFO_BW_40:
		return ("40");
		break;
        case RATE_INFO_BW_80:
		return ("80");
		break;
        case RATE_INFO_BW_160:
		return ("160");
		break;
        case RATE_INFO_BW_HE_RU:
		IMPROVE("nl80211_he_ru_alloc");
		return ("HE_RU");
		break;
        case RATE_INFO_BW_320:
		return ("320");
		break;
        case RATE_INFO_BW_EHT_RU:
		IMPROVE("nl80211_eht_ru_alloc");
		return ("EHT_RU");
		break;
	default:
		return ("?");
		break;
	}
}

static void
lkpi_nl80211_sta_info_to_str(struct sbuf *s, const char *prefix,
    const uint64_t flags)
{
	int bit, i;

	sbuf_printf(s, "%s %#010jx", prefix, flags);

	i = 0;
	for (bit = 0; bit < BITS_PER_TYPE(flags); bit++) {

		if ((flags & BIT_ULL(bit)) == 0)
			continue;

#define	EXPAND_CASE(_flag)						\
	case NL80211_STA_INFO_ ## _flag:				\
		sbuf_printf(s, "%c%s", (i == 0) ? '<' : ',', #_flag);	\
		i++;							\
		break;

		switch (bit) {
		EXPAND_CASE(BEACON_RX)
		EXPAND_CASE(BEACON_SIGNAL_AVG)
		EXPAND_CASE(BSS_PARAM)
		EXPAND_CASE(CHAIN_SIGNAL)
		EXPAND_CASE(CHAIN_SIGNAL_AVG)
		EXPAND_CASE(CONNECTED_TIME)
		EXPAND_CASE(INACTIVE_TIME)
		EXPAND_CASE(SIGNAL)
		EXPAND_CASE(SIGNAL_AVG)
		EXPAND_CASE(STA_FLAGS)
		EXPAND_CASE(RX_BITRATE)
		EXPAND_CASE(RX_PACKETS)
		EXPAND_CASE(RX_BYTES)
		EXPAND_CASE(RX_DROP_MISC)
		EXPAND_CASE(TX_BITRATE)
		EXPAND_CASE(TX_PACKETS)
		EXPAND_CASE(TX_BYTES)
		EXPAND_CASE(TX_BYTES64)
		EXPAND_CASE(RX_BYTES64)
		EXPAND_CASE(TX_FAILED)
		EXPAND_CASE(TX_RETRIES)
		EXPAND_CASE(RX_DURATION)
		EXPAND_CASE(TX_DURATION)
		EXPAND_CASE(ACK_SIGNAL)
		EXPAND_CASE(ACK_SIGNAL_AVG)
		default:
			sbuf_printf(s, "%c?%d", (i == 0) ? '<' : ',', bit);
			break;
		}
	}
#undef	EXPAND_CASE
	if (i > 0)
		sbuf_printf(s, ">");
	sbuf_printf(s, "\n");
}

static void
lkpi_80211_dump_lvif_stas(struct lkpi_vif *lvif, struct sbuf *s)
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct ieee80211vap *vap;
	struct ieee80211_vif *vif;
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;
	struct station_info sinfo;
	int error;

	vif = LVIF_TO_VIF(lvif);
	vap = LVIF_TO_VAP(lvif);
	lhw = vap->iv_ic->ic_softc;
	hw = LHW_TO_HW(lhw);

	wiphy_lock(hw->wiphy);
	list_for_each_entry(lsta, &lvif->lsta_list, lsta_list) {
		sta = LSTA_TO_STA(lsta);

		sbuf_putc(s, '\n');
		sbuf_printf(s, "lsta %p sta %p added_to_drv %d\n", lsta, sta, lsta->added_to_drv);

		memset(&sinfo, 0, sizeof(sinfo));
		error = lkpi_80211_mo_sta_statistics(hw, vif, sta, &sinfo);
		if (error == EEXIST)	/* Not added to driver. */
			continue;
		if (error == ENOTSUPP) {
			sbuf_printf(s, " sta_statistics not supported\n");
			continue;
		}
		if (error != 0) {
			sbuf_printf(s, " sta_statistics failed: %d\n", error);
			continue;
		}

		/* If no RX_BITRATE is reported, try to fill it in from the lsta sinfo. */
		if ((sinfo.filled & BIT_ULL(NL80211_STA_INFO_RX_BITRATE)) == 0 &&
		    (lsta->sinfo.filled & BIT_ULL(NL80211_STA_INFO_RX_BITRATE)) != 0) {
			memcpy(&sinfo.rxrate, &lsta->sinfo.rxrate, sizeof(sinfo.rxrate));
			sinfo.filled |= BIT_ULL(NL80211_STA_INFO_RX_BITRATE);
		}
		/* If no CHAIN_SIGNAL is reported,  try to fill it in from the lsta sinfo. */
		if ((sinfo.filled & BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL)) == 0 &&
		    (lsta->sinfo.filled & BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL)) != 0) {
			sinfo.chains = lsta->sinfo.chains;
			memcpy(sinfo.chain_signal, lsta->sinfo.chain_signal,
			    sizeof(sinfo.chain_signal));
			sinfo.filled |= BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL);
		}

		lkpi_nl80211_sta_info_to_str(s, " nl80211_sta_info (valid fields)", sinfo.filled);
		sbuf_printf(s, " connected_time %u inactive_time %u\n",
		    sinfo.connected_time, sinfo.inactive_time);
		sbuf_printf(s, " rx_bytes %ju rx_packets %u rx_dropped_misc %u\n",
		    (uintmax_t)sinfo.rx_bytes, sinfo.rx_packets, sinfo.rx_dropped_misc);
		sbuf_printf(s, " rx_duration %ju rx_beacon %u rx_beacon_signal_avg %d\n",
		    (uintmax_t)sinfo.rx_duration, sinfo.rx_beacon, (int8_t)sinfo.rx_beacon_signal_avg);

		sbuf_printf(s, " tx_bytes %ju tx_packets %u tx_failed %u\n",
		    (uintmax_t)sinfo.tx_bytes, sinfo.tx_packets, sinfo.tx_failed);
		sbuf_printf(s, " tx_duration %ju tx_retries %u\n",
		    (uintmax_t)sinfo.tx_duration, sinfo.tx_retries);

		sbuf_printf(s, " signal %d signal_avg %d ack_signal %d avg_ack_signal %d\n",
		    sinfo.signal, sinfo.signal_avg, sinfo.ack_signal, sinfo.avg_ack_signal);
		sbuf_printf(s, " generation %d assoc_req_ies_len %zu chains %#04x\n",
		    sinfo.generation, sinfo.assoc_req_ies_len, sinfo.chains);

		for (int i = 0; i < nitems(sinfo.chain_signal) && i < IEEE80211_MAX_CHAINS; i++) {
			if (!(sinfo.chains & BIT(i)))
				continue;
			sbuf_printf(s, "  chain[%d] signal %d signal_avg %d\n",
			    i, (int8_t)sinfo.chain_signal[i], (int8_t)sinfo.chain_signal_avg[i]);
		}

		/* assoc_req_ies, bss_param, sta_flags */

		sbuf_printf(s, " rxrate: flags %b bw %u(%s) legacy %u kbit/s mcs %u nss %u\n",
		    sinfo.rxrate.flags, CFG80211_RATE_INFO_FLAGS_BITS,
		    sinfo.rxrate.bw, lkpi_rate_info_bw_to_str(sinfo.rxrate.bw),
		    sinfo.rxrate.legacy * 100,
		    sinfo.rxrate.mcs, sinfo.rxrate.nss);
		sbuf_printf(s, "         he_dcm %u he_gi %u he_ru_alloc %u eht_gi %u\n",
		    sinfo.rxrate.he_dcm, sinfo.rxrate.he_gi, sinfo.rxrate.he_ru_alloc,
		    sinfo.rxrate.eht_gi);
		sbuf_printf(s, " txrate: flags %b bw %u(%s) legacy %u kbit/s mcs %u nss %u\n",
		    sinfo.txrate.flags, CFG80211_RATE_INFO_FLAGS_BITS,
		    sinfo.txrate.bw, lkpi_rate_info_bw_to_str(sinfo.txrate.bw),
		    sinfo.txrate.legacy * 100,
		    sinfo.txrate.mcs, sinfo.txrate.nss);
		sbuf_printf(s, "         he_dcm %u he_gi %u he_ru_alloc %u eht_gi %u\n",
		    sinfo.txrate.he_dcm, sinfo.txrate.he_gi, sinfo.txrate.he_ru_alloc,
		    sinfo.txrate.eht_gi);
	}
	wiphy_unlock(hw->wiphy);
}

static int
lkpi_80211_dump_stas(SYSCTL_HANDLER_ARGS)
{
	struct lkpi_vif *lvif;
	struct sbuf s;

	if (req->newptr)
		return (EPERM);

	lvif = (struct lkpi_vif *)arg1;

	sbuf_new_for_sysctl(&s, NULL, 1024, req);

	lkpi_80211_dump_lvif_stas(lvif, &s);

	sbuf_finish(&s);
	sbuf_delete(&s);

	return (0);
}

static enum ieee80211_sta_rx_bandwidth
lkpi_cw_to_rx_bw(enum nl80211_chan_width cw)
{
	switch (cw) {
	case NL80211_CHAN_WIDTH_320:
		return (IEEE80211_STA_RX_BW_320);
	case NL80211_CHAN_WIDTH_160:
	case NL80211_CHAN_WIDTH_80P80:
		return (IEEE80211_STA_RX_BW_160);
	case NL80211_CHAN_WIDTH_80:
		return (IEEE80211_STA_RX_BW_80);
	case NL80211_CHAN_WIDTH_40:
		return (IEEE80211_STA_RX_BW_40);
	case NL80211_CHAN_WIDTH_20:
	case NL80211_CHAN_WIDTH_20_NOHT:
		return (IEEE80211_STA_RX_BW_20);
	case NL80211_CHAN_WIDTH_5:
	case NL80211_CHAN_WIDTH_10:
		/* Unsupported input. */
		return (IEEE80211_STA_RX_BW_20);
	}
}

static enum nl80211_chan_width
lkpi_rx_bw_to_cw(enum ieee80211_sta_rx_bandwidth rx_bw)
{
	switch (rx_bw) {
	case IEEE80211_STA_RX_BW_20:
		return (NL80211_CHAN_WIDTH_20);	/* _NOHT */
	case IEEE80211_STA_RX_BW_40:
		return (NL80211_CHAN_WIDTH_40);
	case IEEE80211_STA_RX_BW_80:
		return (NL80211_CHAN_WIDTH_80);
	case IEEE80211_STA_RX_BW_160:
		return (NL80211_CHAN_WIDTH_160); /* 80P80 */
	case IEEE80211_STA_RX_BW_320:
		return (NL80211_CHAN_WIDTH_320);
	}
}

static void
lkpi_sync_chanctx_cw_from_rx_bw(struct ieee80211_hw *hw,
    struct ieee80211_vif *vif, struct ieee80211_sta *sta)
{
	struct ieee80211_chanctx_conf *chanctx_conf;
	enum ieee80211_sta_rx_bandwidth old_bw;
	uint32_t changed;

	chanctx_conf = rcu_dereference_protected(vif->bss_conf.chanctx_conf,
	    lockdep_is_held(&hw->wiphy->mtx));
	if (chanctx_conf == NULL)
		return;

	old_bw = lkpi_cw_to_rx_bw(chanctx_conf->def.width);
	if (old_bw == sta->deflink.bandwidth)
		return;

	chanctx_conf->def.width = lkpi_rx_bw_to_cw(sta->deflink.bandwidth);
	if (chanctx_conf->def.width == NL80211_CHAN_WIDTH_20 &&
	    !sta->deflink.ht_cap.ht_supported)
		chanctx_conf->def.width = NL80211_CHAN_WIDTH_20_NOHT;

	chanctx_conf->min_def = chanctx_conf->def;

	vif->bss_conf.chanreq.oper.width = chanctx_conf->def.width;

	changed = IEEE80211_CHANCTX_CHANGE_MIN_WIDTH;
	changed |= IEEE80211_CHANCTX_CHANGE_WIDTH;
	lkpi_80211_mo_change_chanctx(hw, chanctx_conf, changed);
}

#if defined(LKPI_80211_HT)
static void
lkpi_sta_sync_ht_from_ni(struct ieee80211_vif *vif, struct ieee80211_sta *sta,
    struct ieee80211_node *ni)
{
	struct ieee80211vap *vap;
	uint8_t *ie;
	struct ieee80211_ht_cap *htcap;
	int i, rx_nss;

	if ((ni->ni_flags & IEEE80211_NODE_HT) == 0) {
		sta->deflink.ht_cap.ht_supported = false;
		return;
	}

	sta->deflink.ht_cap.ht_supported = true;

	/* htcap->ampdu_params_info */
	vap = ni->ni_vap;
	sta->deflink.ht_cap.ampdu_density = _IEEE80211_MASKSHIFT(ni->ni_htparam, IEEE80211_HTCAP_MPDUDENSITY);
	if (sta->deflink.ht_cap.ampdu_density > vap->iv_ampdu_density)
		sta->deflink.ht_cap.ampdu_density = vap->iv_ampdu_density;
	sta->deflink.ht_cap.ampdu_factor = _IEEE80211_MASKSHIFT(ni->ni_htparam, IEEE80211_HTCAP_MAXRXAMPDU);
	if (sta->deflink.ht_cap.ampdu_factor > vap->iv_ampdu_rxmax)
		sta->deflink.ht_cap.ampdu_factor = vap->iv_ampdu_rxmax;

	ie = ni->ni_ies.htcap_ie;
	KASSERT(ie != NULL, ("%s: HT but no htcap_ie on ni %p\n", __func__, ni));
	if (ie[0] == IEEE80211_ELEMID_VENDOR)
		ie += 4;
	ie += 2;
	htcap = (struct ieee80211_ht_cap *)ie;
	sta->deflink.ht_cap.cap = htcap->cap_info;
	sta->deflink.ht_cap.mcs = htcap->mcs;

	/*
	 * 802.11n-2009 20.6 Parameters for HT MCSs gives the mandatory/
	 * optional MCS for Nss=1..4.  We need to check the first four
	 * MCS sets from the Rx MCS Bitmask; then there is MCS 32 and
	 * MCS33.. is UEQM.
	 */
	rx_nss = 0;
	for (i = 0; i < 4; i++) {
		if (htcap->mcs.rx_mask[i] != 0)
			rx_nss++;
	}
	if (rx_nss > 0) {
		sta->deflink.rx_nss = rx_nss;
	} else {
		sta->deflink.ht_cap.ht_supported = false;
		return;
	}

	if ((sta->deflink.ht_cap.cap & IEEE80211_HT_CAP_SUP_WIDTH_20_40) != 0 &&
	    IEEE80211_IS_CHAN_HT40(ni->ni_chan))
		sta->deflink.bandwidth = IEEE80211_STA_RX_BW_40;
	else
		sta->deflink.bandwidth = IEEE80211_STA_RX_BW_20;

	IMPROVE("sta->wme");

	if (sta->deflink.ht_cap.cap & IEEE80211_HT_CAP_MAX_AMSDU)
		sta->deflink.agg.max_amsdu_len = IEEE80211_MAX_MPDU_LEN_HT_7935;
	else
		sta->deflink.agg.max_amsdu_len = IEEE80211_MAX_MPDU_LEN_HT_3839;
	sta->deflink.agg.max_rc_amsdu_len = IEEE80211_MAX_MPDU_LEN_HT_BA;
#ifdef __handled_by_driver__	/* iwlwifi only? actually unused? */
	for (i = 0; i < nitems(sta.deflink.agg.max_tid_amsdu_len); i++) {
		sta->deflink.agg.max_tid_amsdu_len[j] = ;
	}
#endif
}
#endif

#if defined(LKPI_80211_VHT)
static void
lkpi_sta_sync_vht_from_ni(struct ieee80211_vif *vif, struct ieee80211_sta *sta,
    struct ieee80211_node *ni)
{
	enum ieee80211_sta_rx_bandwidth bw;
	uint32_t width;
	int rx_nss;
	uint16_t rx_mcs_map;
	uint8_t mcs;

	if ((ni->ni_flags & IEEE80211_NODE_VHT) == 0 ||
	    !IEEE80211_IS_CHAN_VHT_5GHZ(ni->ni_chan)) {
		sta->deflink.vht_cap.vht_supported = false;
		return;
	}

	sta->deflink.vht_cap.vht_supported = true;

	sta->deflink.vht_cap.cap = ni->ni_vhtcap;
	sta->deflink.vht_cap.vht_mcs = ni->ni_vht_mcsinfo;

	/*
	 * If VHT20/40 are selected do not update the bandwidth
	 * from HT but stya on VHT.
	 */
	if (ni->ni_vht_chanwidth == IEEE80211_VHT_CHANWIDTH_USE_HT)
		goto skip_bw;

	bw = sta->deflink.bandwidth;
	width = (sta->deflink.vht_cap.cap & IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_MASK);
	switch (width) {
	/* Deprecated. */
	case IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160MHZ:
	case IEEE80211_VHT_CAP_SUPP_CHAN_WIDTH_160_80PLUS80MHZ:
		bw = IEEE80211_STA_RX_BW_160;
		break;
	default:
		/* Check if we do support 160Mhz somehow after all. */
		if ((sta->deflink.vht_cap.cap & IEEE80211_VHT_CAP_EXT_NSS_BW_MASK) != 0)
			bw = IEEE80211_STA_RX_BW_160;
		else
			bw = IEEE80211_STA_RX_BW_80;
	}
	/*
	 * While we can set what is possibly supported we also need to be
	 * on a channel which supports that bandwidth; e.g., we can support
	 * VHT160 but the AP only does VHT80.
	 * Further ni_chan will also have filtered out what we disabled
	 * by configuration.
	 * Once net80211 channel selection is fixed for 802.11-2020 and
	 * VHT160 we can possibly spare ourselves the above.
	 */
	if (bw == IEEE80211_STA_RX_BW_160 &&
	    !IEEE80211_IS_CHAN_VHT160(ni->ni_chan) &&
	    !IEEE80211_IS_CHAN_VHT80P80(ni->ni_chan))
		bw = IEEE80211_STA_RX_BW_80;
	if (bw == IEEE80211_STA_RX_BW_80 &&
	    !IEEE80211_IS_CHAN_VHT80(ni->ni_chan))
		bw = sta->deflink.bandwidth;
	sta->deflink.bandwidth = bw;
skip_bw:

	rx_nss = 0;
	rx_mcs_map = sta->deflink.vht_cap.vht_mcs.rx_mcs_map;
	for (int i = 7; i >= 0; i--) {
		mcs = rx_mcs_map >> (2 * i);
		mcs &= 0x3;
		if (mcs != IEEE80211_VHT_MCS_NOT_SUPPORTED) {
			rx_nss = i + 1;
			break;
		}
	}
	if (rx_nss > 0)
		sta->deflink.rx_nss = rx_nss;

	switch (sta->deflink.vht_cap.cap & IEEE80211_VHT_CAP_MAX_MPDU_MASK) {
	case IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_11454:
		sta->deflink.agg.max_amsdu_len = IEEE80211_MAX_MPDU_LEN_VHT_11454;
		break;
	case IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_7991:
		sta->deflink.agg.max_amsdu_len = IEEE80211_MAX_MPDU_LEN_VHT_7991;
		break;
	case IEEE80211_VHT_CAP_MAX_MPDU_LENGTH_3895:
	default:
		sta->deflink.agg.max_amsdu_len = IEEE80211_MAX_MPDU_LEN_VHT_3895;
		break;
	}
}
#endif

static void
lkpi_sta_sync_from_ni(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    struct ieee80211_sta *sta, struct ieee80211_node *ni, bool updchnctx)
{

#if defined(LKPI_80211_HT)
	lkpi_sta_sync_ht_from_ni(vif, sta, ni);
#endif
#if defined(LKPI_80211_VHT)
	lkpi_sta_sync_vht_from_ni(vif, sta, ni);
#endif

	/*
	 * Ensure rx_nss is at least 1 as otherwise drivers run into
	 * unexpected problems.
	 */
	sta->deflink.rx_nss = MAX(1, sta->deflink.rx_nss);

	/*
	 * We are also called from node allocation which net80211
	 * can do even on `ifconfig down`; in that case the chanctx
	 * may still be valid and we get a discrepancy between
	 * sta and chanctx.  Thus do not try to update the chanctx
	 * when called from lkpi_lsta_alloc().
	 */
	if (updchnctx)
		lkpi_sync_chanctx_cw_from_rx_bw(hw, vif, sta);
}

static uint8_t
lkpi_get_max_rx_chains(struct ieee80211_node *ni)
{
	uint8_t chains;
#if defined(LKPI_80211_HT) || defined(LKPI_80211_VHT)
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;

	lsta = ni->ni_drv_data;
	sta = LSTA_TO_STA(lsta);
#endif

	chains = 1;
#if defined(LKPI_80211_HT)
	IMPROVE("We should factor counting MCS/NSS out for sync and here");
	if (sta->deflink.ht_cap.ht_supported)
		chains = MAX(chains, sta->deflink.rx_nss);
#endif

#if defined(LKPI_80211_VHT)
	if (sta->deflink.vht_cap.vht_supported)
		chains = MAX(chains, sta->deflink.rx_nss);
#endif

	return (chains);
}

static void
lkpi_lsta_dump(struct lkpi_sta *lsta, struct ieee80211_node *ni,
    const char *_f, int _l)
{

#ifdef LINUXKPI_DEBUG_80211
	if ((linuxkpi_debug_80211 & D80211_TRACE_STA) == 0)
		return;
	if (lsta == NULL)
		return;

	printf("%s:%d lsta %p ni %p sta %p\n",
	    _f, _l, lsta, ni, &lsta->sta);
	if (ni != NULL)
		ieee80211_dump_node(NULL, ni);
	printf("\ttxq_task txq len %d mtx\n", mbufq_len(&lsta->txq));
	printf("\tkc %p state %d added_to_drv %d in_mgd %d\n",
		&lsta->kc[0], lsta->state, lsta->added_to_drv, lsta->in_mgd);
#endif
}

static void
lkpi_lsta_remove(struct lkpi_sta *lsta, struct lkpi_vif *lvif)
{

	lockdep_assert_wiphy(lsta->hw->wiphy);

	KASSERT(!list_empty(&lsta->lsta_list),
	    ("%s: lsta %p ni %p\n", __func__, lsta, lsta->ni));
	list_del_init(&lsta->lsta_list);
}

static struct lkpi_sta *
lkpi_lsta_alloc(struct ieee80211vap *vap, const uint8_t mac[IEEE80211_ADDR_LEN],
    struct ieee80211_hw *hw, struct ieee80211_node *ni)
{
	struct lkpi_sta *lsta;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct ieee80211_sta *sta;
	int band, i, tid;

	lsta = malloc(sizeof(*lsta) + hw->sta_data_size, M_LKPI80211,
	    M_NOWAIT | M_ZERO);
	if (lsta == NULL)
		return (NULL);

	lsta->hw = hw;
	lsta->added_to_drv = false;
	lsta->state = IEEE80211_STA_NOTEXIST;
	/*
	 * Link the ni to the lsta here without taking a reference.
	 * For one we would have to take the reference in node_init()
	 * as ieee80211_alloc_node() will initialise the refcount after us.
	 * For the other a ni and an lsta are 1:1 mapped and always together
	 * from [ic_]node_alloc() to [ic_]node_free() so we are essentally
	 * using the ni references for the lsta as well despite it being
	 * two separate allocations.
	 */
	lsta->ni = ni;
	/* The back-pointer "drv_data" to net80211_node let's us get lsta. */
	ni->ni_drv_data = lsta;

	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);
	sta = LSTA_TO_STA(lsta);

	IEEE80211_ADDR_COPY(sta->addr, mac);

	/* TXQ */
	for (tid = 0; tid < nitems(sta->txq); tid++) {
		struct lkpi_txq *ltxq;

		/* We are not limiting ourselves to hw.queues here. */
		ltxq = malloc(sizeof(*ltxq) + hw->txq_data_size,
		    M_LKPI80211, M_NOWAIT | M_ZERO);
		if (ltxq == NULL)
			goto cleanup;
		/* iwlwifi//mvm/sta.c::tid_to_mac80211_ac[] */
		if (tid == IEEE80211_NUM_TIDS) {
			if (!ieee80211_hw_check(hw, STA_MMPDU_TXQ)) {
				free(ltxq, M_LKPI80211);
				continue;
			}
			IMPROVE("AP/if we support non-STA here too");
			ltxq->txq.ac = IEEE80211_AC_VO;
		} else {
			ltxq->txq.ac = ieee80211e_up_to_ac[tid & 7];
		}
		ltxq->seen_dequeue = false;
		ltxq->stopped = false;
		ltxq->txq.vif = vif;
		ltxq->txq.tid = tid;
		ltxq->txq.sta = sta;
		TAILQ_ELEM_INIT(ltxq, txq_entry);
		skb_queue_head_init(&ltxq->skbq);
		LKPI_80211_LTXQ_LOCK_INIT(ltxq);
		sta->txq[tid] = &ltxq->txq;
	}

	/* Deflink information. */
	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		struct ieee80211_supported_band *supband;

		supband = hw->wiphy->bands[band];
		if (supband == NULL)
			continue;

		for (i = 0; i < supband->n_bitrates; i++) {
			switch (band) {
			case NL80211_BAND_2GHZ:
				switch (supband->bitrates[i].bitrate) {
				case 240:	/* 11g only */
				case 120:	/* 11g only */
				case 110:
				case 60:	/* 11g only */
				case 55:
				case 20:
				case 10:
					sta->deflink.supp_rates[band] |= BIT(i);
					break;
				}
				break;
			case NL80211_BAND_5GHZ:
				switch (supband->bitrates[i].bitrate) {
				case 240:
				case 120:
				case 60:
					sta->deflink.supp_rates[band] |= BIT(i);
					break;
				}
				break;
			}
		}
	}

	sta->deflink.smps_mode = IEEE80211_SMPS_OFF;
	sta->deflink.bandwidth = IEEE80211_STA_RX_BW_20;
	sta->deflink.rx_nss = 1;

	lkpi_sta_sync_from_ni(hw, vif, sta, ni, false);

	IMPROVE("he, eht, bw_320, ... smps_mode, ..");

	/* Link configuration. */
	IEEE80211_ADDR_COPY(sta->deflink.addr, sta->addr);
	sta->link[0] = &sta->deflink;
	for (i = 1; i < nitems(sta->link); i++) {
		IMPROVE("more links; only link[0] = deflink currently.");
	}
	IMPROVE("11be");
	sta->mlo = false;

	/* Deferred TX path. */
	LKPI_80211_LSTA_TXQ_LOCK_INIT(lsta);
	TASK_INIT(&lsta->txq_task, 0, lkpi_80211_txq_task, lsta);
	mbufq_init(&lsta->txq, 32 * NAPI_POLL_WEIGHT);
	lsta->txq_ready = true;

	return (lsta);

cleanup:
	for (; tid >= 0; tid--) {
		struct lkpi_txq *ltxq;

		ltxq = TXQ_TO_LTXQ(sta->txq[tid]);
		LKPI_80211_LTXQ_LOCK_DESTROY(ltxq);
		free(sta->txq[tid], M_LKPI80211);
	}
	free(lsta, M_LKPI80211);
	return (NULL);
}

static void
lkpi_lsta_free(struct lkpi_sta *lsta, struct ieee80211_node *ni)
{
	struct mbuf *m;

	if (lsta->added_to_drv)
		panic("%s: Trying to free an lsta still known to firmware: "
		    "lsta %p ni %p added_to_drv %d\n",
		    __func__, lsta, ni, lsta->added_to_drv);

	/* XXX-BZ free resources, ... */
	IMPROVE();

	/* Drain sta->txq[] */

	LKPI_80211_LSTA_TXQ_LOCK(lsta);
	lsta->txq_ready = false;
	LKPI_80211_LSTA_TXQ_UNLOCK(lsta);

	/* Drain taskq, won't be restarted until added_to_drv is set again. */
	while (taskqueue_cancel(taskqueue_thread, &lsta->txq_task, NULL) != 0)
		taskqueue_drain(taskqueue_thread, &lsta->txq_task);

	/* Flush mbufq (make sure to release ni refs!). */
	m = mbufq_dequeue(&lsta->txq);
	while (m != NULL) {
		struct ieee80211_node *nim;

		nim = (struct ieee80211_node *)m->m_pkthdr.rcvif;
		if (nim != NULL)
			ieee80211_free_node(nim);
		m_freem(m);
		m = mbufq_dequeue(&lsta->txq);
	}
	KASSERT(mbufq_empty(&lsta->txq), ("%s: lsta %p has txq len %d != 0\n",
	    __func__, lsta, mbufq_len(&lsta->txq)));
	LKPI_80211_LSTA_TXQ_LOCK_DESTROY(lsta);

	/* Remove lsta from vif; that is done by the state machine.  Should assert it? */

	IMPROVE("Make sure everything is cleaned up.");

	/* Free lsta. */
	lsta->ni = NULL;
	ni->ni_drv_data = NULL;
	free(lsta, M_LKPI80211);
}


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

#ifdef LINUXKPI_DEBUG_80211
static const char *
lkpi_nl80211_band_name(enum nl80211_band band)
{
	switch (band) {
	case NL80211_BAND_2GHZ:
		return "2Ghz";
		break;
	case NL80211_BAND_5GHZ:
		return "5Ghz";
		break;
	case NL80211_BAND_60GHZ:
		return "60Ghz";
		break;
	case NL80211_BAND_6GHZ:
		return "6Ghz";
		break;
	default:
		panic("%s: unsupported band %u\n", __func__, band);
		break;
	}
}
#endif

#if 0
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
#endif

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

#ifdef LKPI_80211_HW_CRYPTO
static const char *
lkpi_cipher_suite_to_name(uint32_t wlan_cipher_suite)
{
	switch (wlan_cipher_suite) {
	case WLAN_CIPHER_SUITE_WEP40:
		return ("WEP40");
	case WLAN_CIPHER_SUITE_WEP104:
		return ("WEP104");
	case WLAN_CIPHER_SUITE_TKIP:
		return ("TKIP");
	case WLAN_CIPHER_SUITE_CCMP:
		return ("CCMP");
	case WLAN_CIPHER_SUITE_CCMP_256:
		return ("CCMP_256");
	case WLAN_CIPHER_SUITE_GCMP:
		return ("GCMP");
	case WLAN_CIPHER_SUITE_GCMP_256:
		return ("GCMP_256");
	case WLAN_CIPHER_SUITE_AES_CMAC:
		return ("AES_CMAC");
	case WLAN_CIPHER_SUITE_BIP_CMAC_256:
		return ("BIP_CMAC_256");
	case WLAN_CIPHER_SUITE_BIP_GMAC_128:
		return ("BIP_GMAC_128");
	case WLAN_CIPHER_SUITE_BIP_GMAC_256:
		return ("BIP_GMAC_256");
	default:
		return ("??");
	}
}

static uint32_t
lkpi_l80211_to_net80211_cyphers(struct ieee80211com *ic,
    uint32_t wlan_cipher_suite)
{
	switch (wlan_cipher_suite) {
	case WLAN_CIPHER_SUITE_WEP40:
		return (IEEE80211_CRYPTO_WEP);
	case WLAN_CIPHER_SUITE_WEP104:
		return (IEEE80211_CRYPTO_WEP);
	case WLAN_CIPHER_SUITE_TKIP:
		return (IEEE80211_CRYPTO_TKIP);
	case WLAN_CIPHER_SUITE_CCMP:
		return (IEEE80211_CRYPTO_AES_CCM);
	case WLAN_CIPHER_SUITE_CCMP_256:
		return (IEEE80211_CRYPTO_AES_CCM_256);
	case WLAN_CIPHER_SUITE_GCMP:
		return (IEEE80211_CRYPTO_AES_GCM_128);
	case WLAN_CIPHER_SUITE_GCMP_256:
		return (IEEE80211_CRYPTO_AES_GCM_256);
	case WLAN_CIPHER_SUITE_AES_CMAC:
		return (IEEE80211_CRYPTO_BIP_CMAC_128);
	case WLAN_CIPHER_SUITE_BIP_CMAC_256:
		return (IEEE80211_CRYPTO_BIP_CMAC_256);
	case WLAN_CIPHER_SUITE_BIP_GMAC_128:
		return (IEEE80211_CRYPTO_BIP_GMAC_128);
	case WLAN_CIPHER_SUITE_BIP_GMAC_256:
		return (IEEE80211_CRYPTO_BIP_GMAC_256);
	default:
		ic_printf(ic, "%s: unknown WLAN Cipher Suite %#08x | %u (%s)\n",
		    __func__,
		    wlan_cipher_suite >> 8, wlan_cipher_suite & 0xff,
		    lkpi_cipher_suite_to_name(wlan_cipher_suite));
		return (0);
	}
}

static uint32_t
lkpi_net80211_to_l80211_cipher_suite(uint32_t cipher, uint8_t keylen)
{

	switch (cipher) {
	case IEEE80211_CIPHER_WEP:
		if (keylen == (40/NBBY))
			return (WLAN_CIPHER_SUITE_WEP40);
		else if (keylen == (104/NBBY))
			return (WLAN_CIPHER_SUITE_WEP104);
		else {
			printf("%s: WEP with unsupported keylen %d\n",
			    __func__, keylen * NBBY);
			return (0);
		}
		break;
	case IEEE80211_CIPHER_TKIP:
		return (WLAN_CIPHER_SUITE_TKIP);
	case IEEE80211_CIPHER_AES_CCM:
		return (WLAN_CIPHER_SUITE_CCMP);
	case IEEE80211_CIPHER_AES_CCM_256:
		return (WLAN_CIPHER_SUITE_CCMP_256);
	case IEEE80211_CIPHER_AES_GCM_128:
		return (WLAN_CIPHER_SUITE_GCMP);
	case IEEE80211_CIPHER_AES_GCM_256:
		return (WLAN_CIPHER_SUITE_GCMP_256);
	case IEEE80211_CIPHER_BIP_CMAC_128:
		return (WLAN_CIPHER_SUITE_AES_CMAC);
	case IEEE80211_CIPHER_BIP_CMAC_256:
		return (WLAN_CIPHER_SUITE_BIP_CMAC_256);
	case IEEE80211_CIPHER_BIP_GMAC_128:
		return (WLAN_CIPHER_SUITE_BIP_GMAC_128);
	case IEEE80211_CIPHER_BIP_GMAC_256:
		return (WLAN_CIPHER_SUITE_BIP_GMAC_256);

	case IEEE80211_CIPHER_AES_OCB:
	case IEEE80211_CIPHER_TKIPMIC:
		/*
		 * TKIP w/ hw MIC support
		 * (gone wrong; should really be a crypto flag in net80211).
		 */
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
		if (channels[i].center_freq == c->ic_freq)
			return (&channels[i]);
	}

	return (NULL);
}

#if 0
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
#endif

struct linuxkpi_ieee80211_channel *
linuxkpi_ieee80211_get_channel(struct wiphy *wiphy, uint32_t freq)
{
	enum nl80211_band band;

	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		struct ieee80211_supported_band *supband;
		struct linuxkpi_ieee80211_channel *channels;
		int i;

		supband = wiphy->bands[band];
		if (supband == NULL || supband->n_channels == 0)
			continue;

		channels = supband->channels;
		for (i = 0; i < supband->n_channels; i++) {
			if (channels[i].center_freq == freq)
				return (&channels[i]);
		}
	}

	return (NULL);
}

#ifdef LKPI_80211_HW_CRYPTO
static int
lkpi_sta_del_keys(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    struct lkpi_sta *lsta)
{
	int error;

	if (!lkpi_hwcrypto)
		return (0);

	lockdep_assert_wiphy(hw->wiphy);
	ieee80211_ref_node(lsta->ni);

	error = 0;
	for (ieee80211_keyix keyix = 0; keyix < nitems(lsta->kc); keyix++) {
		struct ieee80211_key_conf *kc;
		int err;

		if (lsta->kc[keyix] == NULL)
			continue;
		kc = lsta->kc[keyix];

#ifdef LINUXKPI_DEBUG_80211
		if (linuxkpi_debug_80211 & D80211_TRACE_HW_CRYPTO)
			ic_printf(lsta->ni->ni_ic, "%d %lu %s: running set_key cmd %d(%s) for "
			    "sta %6D: keyidx %u hw_key_idx %u flags %b\n",
			    curthread->td_tid, jiffies, __func__,
			    DISABLE_KEY, "DISABLE", lsta->sta.addr, ":",
			    kc->keyidx, kc->hw_key_idx, kc->flags, IEEE80211_KEY_FLAG_BITS);
#endif

		err = lkpi_80211_mo_set_key(hw, DISABLE_KEY, vif,
		    LSTA_TO_STA(lsta), kc);
		if (err != 0) {
			ic_printf(lsta->ni->ni_ic, "%d %lu %s: set_key cmd %d(%s) for "
			    "sta %6D failed: %d\n", curthread->td_tid, jiffies, __func__,
			    DISABLE_KEY, "DISABLE", lsta->sta.addr, ":", err);
			error++;

			/*
			 * If we free the key here we will never be able to get it
			 * removed from the driver/fw which will likely make us
			 * crash (firmware).
			 */
			continue;
		}
#ifdef LINUXKPI_DEBUG_80211
		if (linuxkpi_debug_80211 & D80211_TRACE_HW_CRYPTO)
			ic_printf(lsta->ni->ni_ic, "%d %lu %s: set_key cmd %d(%s) for "
			    "sta %6D succeeded: keyidx %u hw_key_idx %u flags %b\n",
			    curthread->td_tid, jiffies, __func__,
			    DISABLE_KEY, "DISABLE", lsta->sta.addr, ":",
			    kc->keyidx, kc->hw_key_idx, kc->flags, IEEE80211_KEY_FLAG_BITS);
#endif

		lsta->kc[keyix] = NULL;
		free(kc, M_LKPI80211);
	}
	ieee80211_free_node(lsta->ni);
	return (error);
}

/* XXX-BZ one day we should replace this iterating over VIFs, or node list? */
/* See also lkpi_sta_del_keys() these days. */
static int
lkpi_iv_key_delete(struct ieee80211vap *vap, const struct ieee80211_key *k)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct lkpi_sta *lsta;
	struct ieee80211_vif *vif;
	struct ieee80211_sta *sta;
	struct ieee80211_node *ni;
	struct ieee80211_key_conf *kc;
	int error;

	ic = vap->iv_ic;
	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	/*
	 * Make sure we do not make it here without going through
	 * lkpi_iv_key_update_begin() first.
	 */
	lockdep_assert_wiphy(hw->wiphy);

	/*
	 * While we are assoc we may still send packets.  We cannot delete the
	 * keys as otherwise packets could go out unencrypted.  Some firmware
	 * does not like this and will fire an assert.
	 * net80211 needs to drive this better but given we want the disassoc
	 * frame out and have to unlock we are open to a race currently.
	 * This check should prevent problems.
	 * How to test: run 800Mbit/s UDP traffic and during that restart your
	 * supplicant.  You want to survive that.
	 */
	if (vif->cfg.assoc) {
		if (linuxkpi_debug_80211 & D80211_TRACE_HW_CRYPTO)
			ic_printf(ic, "%d %lu %s: vif still assoc; not deleting keys\n",
			    curthread->td_tid, jiffies, __func__);
		return (0);
	}

	if (IEEE80211_KEY_UNDEFINED(k)) {
		ic_printf(ic, "%s: vap %p key %p is undefined: %p %u\n",
		    __func__, vap, k, k->wk_cipher, k->wk_keyix);
		return (0);
	}

	if (vap->iv_bss == NULL) {
		ic_printf(ic, "%s: iv_bss %p for vap %p is NULL\n",
		    __func__, vap->iv_bss, vap);
		return (0);
	}

	ni = ieee80211_ref_node(vap->iv_bss);
	lsta = ni->ni_drv_data;
	if (lsta == NULL) {
		ic_printf(ic, "%s: ni %p (%6D) with lsta NULL\n",
		    __func__, ni, ni->ni_bssid, ":");
		ieee80211_free_node(ni);
		return (0);
	}
	sta = LSTA_TO_STA(lsta);

	if (lsta->kc[k->wk_keyix] == NULL) {
#ifdef LINUXKPI_DEBUG_80211
		if (linuxkpi_debug_80211 & D80211_TRACE_HW_CRYPTO)
			ic_printf(ic, "%d %lu %s: sta %6D and no key information, "
			    "keyidx %u wk_macaddr %6D; returning success\n",
			    curthread->td_tid, jiffies, __func__, sta->addr, ":",
			    k->wk_keyix, k->wk_macaddr, ":");
#endif
		ieee80211_free_node(ni);
		return (1);
	}
	kc = lsta->kc[k->wk_keyix];

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE_HW_CRYPTO)
		ic_printf(ic, "%d %lu %s: running set_key cmd %d(%s) for sta %6D: "
		    "keyidx %u hw_key_idx %u flags %b\n",
		    curthread->td_tid, jiffies, __func__,
		    DISABLE_KEY, "DISABLE", sta->addr, ":",
		    kc->keyidx, kc->hw_key_idx, kc->flags, IEEE80211_KEY_FLAG_BITS);
#endif

	error = lkpi_80211_mo_set_key(hw, DISABLE_KEY, vif, sta, kc);
	if (error != 0) {
		ic_printf(ic, "%d %lu %s: set_key cmd %d(%s) for sta %6D failed: %d\n",
		    curthread->td_tid, jiffies, __func__,
		    DISABLE_KEY, "DISABLE", sta->addr, ":", error);
		error = 0;
		goto out;
	}

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE_HW_CRYPTO)
		ic_printf(ic, "%d %lu %s: set_key cmd %d(%s) for sta %6D succeeded: "
		    "keyidx %u hw_key_idx %u flags %b\n",
		    curthread->td_tid, jiffies, __func__,
		    DISABLE_KEY, "DISABLE", sta->addr, ":",
		    kc->keyidx, kc->hw_key_idx, kc->flags, IEEE80211_KEY_FLAG_BITS);
#endif
	lsta->kc[k->wk_keyix] = NULL;
	free(kc, M_LKPI80211);
	error = 1;
out:
	ieee80211_free_node(ni);
	return (error);
}

static int
lkpi_iv_key_set(struct ieee80211vap *vap, const struct ieee80211_key *k)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct lkpi_sta *lsta;
	struct ieee80211_vif *vif;
	struct ieee80211_sta *sta;
	struct ieee80211_node *ni;
	struct ieee80211_key_conf *kc;
	uint32_t lcipher;
	uint16_t exp_flags;
	uint8_t keylen;
	int error;

	ic = vap->iv_ic;
	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);

	/*
	 * Make sure we do not make it here without going through
	 * lkpi_iv_key_update_begin() first.
	 */
	lockdep_assert_wiphy(hw->wiphy);

	if (IEEE80211_KEY_UNDEFINED(k)) {
		ic_printf(ic, "%s: vap %p key %p is undefined: %p %u\n",
		    __func__, vap, k, k->wk_cipher, k->wk_keyix);
		return (0);
	}

	if (vap->iv_bss == NULL) {
		ic_printf(ic, "%s: iv_bss %p for vap %p is NULL\n",
		    __func__, vap->iv_bss, vap);
		return (0);
	}
	ni = ieee80211_ref_node(vap->iv_bss);
	lsta = ni->ni_drv_data;
	if (lsta == NULL) {
		ic_printf(ic, "%s: ni %p (%6D) with lsta NULL\n",
		    __func__, ni, ni->ni_bssid, ":");
		ieee80211_free_node(ni);
		return (0);
	}
	sta = LSTA_TO_STA(lsta);

	keylen = k->wk_keylen;
	lcipher = lkpi_net80211_to_l80211_cipher_suite(
	    k->wk_cipher->ic_cipher, k->wk_keylen);
	switch (lcipher) {
	case WLAN_CIPHER_SUITE_TKIP:
		keylen += 2 * k->wk_cipher->ic_miclen;
		break;
	case WLAN_CIPHER_SUITE_CCMP:
	case WLAN_CIPHER_SUITE_GCMP:
		break;
	default:
		ic_printf(ic, "%s: CIPHER SUITE %#x (%s) not supported\n",
		    __func__, lcipher, lkpi_cipher_suite_to_name(lcipher));
		IMPROVE();
		ieee80211_free_node(ni);
		return (0);
	}

	if (lsta->kc[k->wk_keyix] != NULL) {
		IMPROVE("Still in firmware? Del first. Can we assert this cannot happen?");
		ic_printf(ic, "%s: sta %6D found with key information\n",
		    __func__, sta->addr, ":");
		kc = lsta->kc[k->wk_keyix];
		lsta->kc[k->wk_keyix] = NULL;
		free(kc, M_LKPI80211);
		kc = NULL;	/* safeguard */
	}

	kc = malloc(sizeof(*kc) + keylen, M_LKPI80211, M_WAITOK | M_ZERO);
	kc->_k = k;		/* Save the pointer to net80211. */
	kc->cipher = lcipher;
	kc->keyidx = k->wk_keyix;
#if 0
	kc->hw_key_idx = /* set by hw and needs to be passed for TX */;
#endif
	atomic64_set(&kc->tx_pn, k->wk_keytsc);
	kc->keylen = k->wk_keylen;
	memcpy(kc->key, k->wk_key, k->wk_keylen);

	if (k->wk_flags & (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV))
		kc->flags |= IEEE80211_KEY_FLAG_PAIRWISE;
	if (k->wk_flags & IEEE80211_KEY_GROUP)
		kc->flags &= ~IEEE80211_KEY_FLAG_PAIRWISE;

	kc->iv_len = k->wk_cipher->ic_header;
	kc->icv_len = k->wk_cipher->ic_trailer;

	switch (kc->cipher) {
	case WLAN_CIPHER_SUITE_TKIP:
		memcpy(kc->key + NL80211_TKIP_DATA_OFFSET_TX_MIC_KEY, k->wk_txmic, k->wk_cipher->ic_miclen);
		memcpy(kc->key + NL80211_TKIP_DATA_OFFSET_RX_MIC_KEY, k->wk_rxmic, k->wk_cipher->ic_miclen);
		break;
	case WLAN_CIPHER_SUITE_CCMP:
	case WLAN_CIPHER_SUITE_GCMP:
		break;
	default:
		/* currently UNREACH */
		IMPROVE();
		break;
	};
	lsta->kc[k->wk_keyix] = kc;

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE_HW_CRYPTO)
		ic_printf(ic, "%d %lu %s: running set_key cmd %d(%s) for sta %6D: "
		    "kc %p keyidx %u hw_key_idx %u keylen %u flags %b\n",
		    curthread->td_tid, jiffies, __func__,
		    SET_KEY, "SET", sta->addr, ":", kc, kc->keyidx, kc->hw_key_idx,
		    kc->keylen, kc->flags, IEEE80211_KEY_FLAG_BITS);
#endif

	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);
	error = lkpi_80211_mo_set_key(hw, SET_KEY, vif, sta, kc);
	if (error != 0) {
		ic_printf(ic, "%d %lu %s: set_key cmd %d(%s) for sta %6D failed: %d\n",
		    curthread->td_tid, jiffies, __func__,
		    SET_KEY, "SET", sta->addr, ":", error);
		lsta->kc[k->wk_keyix] = NULL;
		free(kc, M_LKPI80211);
		ieee80211_free_node(ni);
		return (0);
	}

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE_HW_CRYPTO)
		ic_printf(ic, "%d %lu %s: set_key cmd %d(%s) for sta %6D succeeded: "
		    "kc %p keyidx %u hw_key_idx %u flags %b\n",
		    curthread->td_tid, jiffies, __func__,
		    SET_KEY, "SET", sta->addr, ":",
		    kc, kc->keyidx, kc->hw_key_idx, kc->flags, IEEE80211_KEY_FLAG_BITS);
#endif

	exp_flags = 0;
	switch (kc->cipher) {
	case WLAN_CIPHER_SUITE_TKIP:
		exp_flags = (IEEE80211_KEY_FLAG_PAIRWISE |
			IEEE80211_KEY_FLAG_PUT_IV_SPACE |
			IEEE80211_KEY_FLAG_GENERATE_MMIC |
			IEEE80211_KEY_FLAG_PUT_MIC_SPACE);
#define	TKIP_INVAL_COMBINATION						\
     (IEEE80211_KEY_FLAG_PUT_MIC_SPACE|IEEE80211_KEY_FLAG_GENERATE_MMIC)
		if ((kc->flags & TKIP_INVAL_COMBINATION) == TKIP_INVAL_COMBINATION) {
			ic_printf(ic, "%s: SET_KEY for %s returned invalid "
			    "combination %b\n", __func__,
			    lkpi_cipher_suite_to_name(kc->cipher),
			    kc->flags, IEEE80211_KEY_FLAG_BITS);
		}
#undef	TKIP_INVAL_COMBINATION
#ifdef __notyet__
		/* Do flags surgery; special see linuxkpi_ieee80211_ifattach(). */
		if ((kc->flags & IEEE80211_KEY_FLAG_GENERATE_MMIC) != 0) {
			k->wk_flags &= ~(IEEE80211_KEY_NOMICMGT|IEEE80211_KEY_NOMIC);
			k->wk_flags |= IEEE80211_KEY_SWMIC;
			ic->ic_cryptocaps &= ~IEEE80211_CRYPTO_TKIPMIC
		}
#endif
		break;
	case WLAN_CIPHER_SUITE_CCMP:
	case WLAN_CIPHER_SUITE_GCMP:
		exp_flags = (IEEE80211_KEY_FLAG_PAIRWISE |
		    IEEE80211_KEY_FLAG_PUT_IV_SPACE |
		    IEEE80211_KEY_FLAG_GENERATE_IV |
		    IEEE80211_KEY_FLAG_GENERATE_IV_MGMT |	/* Only needs IV geeration for MGMT frames. */
		    IEEE80211_KEY_FLAG_SW_MGMT_TX);		/* MFP in software */
		break;
	}
	if ((kc->flags & ~exp_flags) != 0)
		ic_printf(ic, "%s: SET_KEY for %s returned unexpected key flags: "
		    " %#06x & ~%#06x = %b\n", __func__,
		    lkpi_cipher_suite_to_name(kc->cipher), kc->flags, exp_flags,
		    (kc->flags & ~exp_flags), IEEE80211_KEY_FLAG_BITS);

#ifdef __notyet__
	/* Do flags surgery. */
	if ((kc->flags & IEEE80211_KEY_FLAG_GENERATE_IV_MGMT) == 0)
		k->wk_flags |= IEEE80211_KEY_NOIVMGT;
	if ((kc->flags & IEEE80211_KEY_FLAG_GENERATE_IV) == 0)
		k->wk_flags |= IEEE80211_KEY_NOIV;
#endif

	ieee80211_free_node(ni);
	return (1);
}

static void
lkpi_iv_key_update_begin(struct ieee80211vap *vap)
{
	struct ieee80211_node_table *nt;
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_node *ni;
	bool icislocked, ntislocked;

	ic = vap->iv_ic;
	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	nt = &ic->ic_sta;

	icislocked = IEEE80211_IS_LOCKED(ic);
	ntislocked = IEEE80211_NODE_IS_LOCKED(nt);

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE_HW_CRYPTO)
		ic_printf(ic, "%d %lu %s: vap %p ic %p %slocked nt %p %slocked "
		    "lvif ic_unlocked %d nt_unlocked %d\n",
		    curthread->td_tid, jiffies, __func__, vap,
		    ic, icislocked ? "" : "un", nt, ntislocked ? "" : "un",
		    lvif->ic_unlocked, lvif->nt_unlocked);
#endif

	/*
	 * This is inconsistent net80211 locking to be fixed one day.
	 */
	/* Try to make sure the node does not go away while possibly unlocked. */
	ni = NULL;
	if (icislocked || ntislocked) {
		if (vap->iv_bss != NULL)
			ni = ieee80211_ref_node(vap->iv_bss);
	}

	if (icislocked)
		IEEE80211_UNLOCK(ic);
	if (ntislocked)
		IEEE80211_NODE_UNLOCK(nt);

	wiphy_lock(hw->wiphy);

	KASSERT(lvif->key_update_iv_bss == NULL, ("%s: key_update_iv_bss not NULL %p",
	    __func__, lvif->key_update_iv_bss));
	lvif->key_update_iv_bss = ni;

	/*
	 * ic/nt_unlocked could be a bool given we are under the lock and there
	 * must only be a single thread.
	 * In case anything in the future disturbs the order the refcnt will
	 * help us catching problems a lot easier.
	 */
	if (icislocked)
		refcount_acquire(&lvif->ic_unlocked);
	if (ntislocked)
		refcount_acquire(&lvif->nt_unlocked);

	/*
	 * Stop the queues while doing key updates.
	 */
	ieee80211_stop_queues(hw);
}

static void
lkpi_iv_key_update_end(struct ieee80211vap *vap)
{
	struct ieee80211_node_table *nt;
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	bool icislocked, ntislocked;

	ic = vap->iv_ic;
	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	nt = &ic->ic_sta;

	/*
	 * Re-enabled the queues after the key update.
	 */
	lkpi_ieee80211_wake_queues_locked(hw);

	icislocked = IEEE80211_IS_LOCKED(ic);
	MPASS(!icislocked);
	ntislocked = IEEE80211_NODE_IS_LOCKED(nt);
	MPASS(!ntislocked);

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE_HW_CRYPTO)
		ic_printf(ic, "%d %lu %s: vap %p ic %p %slocked nt %p %slocked "
		    "lvif ic_unlocked %d nt_unlocked %d\n",
		    curthread->td_tid, jiffies, __func__, vap,
		    ic, icislocked ? "" : "un", nt, ntislocked ? "" : "un",
		    lvif->ic_unlocked, lvif->nt_unlocked);
#endif

	/*
	 * Check under lock; see comment in lkpi_iv_key_update_begin().
	 * In case the refcnt gets out of sync locking in net80211 will
	 * quickly barf as well (trying to unlock a lock not held).
	 */
	icislocked = refcount_release_if_last(&lvif->ic_unlocked);
	ntislocked = refcount_release_if_last(&lvif->nt_unlocked);

	if (lvif->key_update_iv_bss != NULL) {
		ieee80211_free_node(lvif->key_update_iv_bss);
		lvif->key_update_iv_bss = NULL;
	}

	wiphy_unlock(hw->wiphy);

	/*
	 * This is inconsistent net80211 locking to be fixed one day.
	 * ic before nt to avoid a LOR.
	 */
	if (icislocked)
		IEEE80211_LOCK(ic);
	if (ntislocked)
		IEEE80211_NODE_LOCK(nt);
}
#endif

static void
lkpi_cleanup_mcast_list_locked(struct lkpi_hw *lhw)
{
	struct list_head *le, *next;
	struct netdev_hw_addr *addr;

	if (lhw->mc_list.count != 0) {
		list_for_each_safe(le, next, &lhw->mc_list.addr_list) {
			addr = list_entry(le, struct netdev_hw_addr, addr_list);
			list_del(le);
			lhw->mc_list.count--;
			free(addr, M_LKPI80211);
		}
	}
	KASSERT(lhw->mc_list.count == 0, ("%s: mc_list %p count %d != 0\n",
	    __func__, &lhw->mc_list, lhw->mc_list.count));
}

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

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE)
		printf("%s:%d: mc_list count %d: added %6D\n",
		    __func__, __LINE__, mc_list->count, addr->addr, ":");
#endif

	return (1);
}

static void
lkpi_update_mcast_filter(struct ieee80211com *ic)
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	u64 mc;
	unsigned int changed_flags, flags;
	bool scanning;

	lhw = ic->ic_softc;

	if (lhw->ops->prepare_multicast == NULL ||
	    lhw->ops->configure_filter == NULL)
		return;

	LKPI_80211_LHW_SCAN_LOCK(lhw);
	scanning = (lhw->scan_flags & LKPI_LHW_SCAN_RUNNING) != 0;
	LKPI_80211_LHW_SCAN_UNLOCK(lhw);

	LKPI_80211_LHW_MC_LOCK(lhw);

	flags = 0;
	if (scanning)
		flags |= FIF_BCN_PRBRESP_PROMISC;
	if (lhw->mc_all_multi)
		flags |= FIF_ALLMULTI;

	hw = LHW_TO_HW(lhw);
	mc = lkpi_80211_mo_prepare_multicast(hw, &lhw->mc_list);

	changed_flags = (lhw->mc_flags ^ flags) & FIF_FLAGS_MASK;
	lkpi_80211_mo_configure_filter(hw, changed_flags, &flags, mc);
	lhw->mc_flags = flags;

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE)
		printf("%s: changed_flags %#06x count %d mc_flags %#010x\n",
		    __func__, changed_flags, lhw->mc_list.count, lhw->mc_flags);
#endif

	LKPI_80211_LHW_MC_UNLOCK(lhw);
}

static enum ieee80211_bss_changed
lkpi_update_dtim_tsf(struct ieee80211_vif *vif, struct ieee80211_node *ni,
    struct ieee80211vap *vap, const char *_f, int _l)
{
	enum ieee80211_bss_changed bss_changed;

	bss_changed = 0;

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE)
		printf("%s:%d [%s:%d] assoc %d aid %d beacon_int %u "
		    "dtim_period %u sync_dtim_count %u sync_tsf %ju "
		    "sync_device_ts %u bss_changed %#010jx\n",
			__func__, __LINE__, _f, _l,
			vif->cfg.assoc, vif->cfg.aid,
			vif->bss_conf.beacon_int, vif->bss_conf.dtim_period,
			vif->bss_conf.sync_dtim_count,
			(uintmax_t)vif->bss_conf.sync_tsf,
			vif->bss_conf.sync_device_ts,
			(uintmax_t)bss_changed);
#endif

	if (vif->bss_conf.beacon_int != ni->ni_intval) {
		vif->bss_conf.beacon_int = ni->ni_intval;
		/* iwlwifi FW bug workaround; iwl_mvm_mac_sta_state. */
		if (vif->bss_conf.beacon_int < 16)
			vif->bss_conf.beacon_int = 16;
		bss_changed |= BSS_CHANGED_BEACON_INT;
	}

	/*
	 * lkpi_iv_sta_recv_mgmt() will directly call into this function.
	 * iwlwifi(4) in iwl_mvm_bss_info_changed_station_common() will
	 * stop seesion protection the moment it sees
	 * BSS_CHANGED_BEACON_INFO (with the expectations that it was
	 * "a beacon from the associated AP"). It will also update
	 * the beacon filter in that case.  This is the only place
	 * we set the BSS_CHANGED_BEACON_INFO on the non-teardown
	 * path so make sure we only do run this check once we are
	 * assoc. (*iv_recv_mgmt)() will be called before we enter
	 * here so the ni will be updates with information from the
	 * beacon via net80211::sta_recv_mgmt().  We also need to
	 * make sure we do not do it on every beacon we still may
	 * get so only do if something changed.  vif->bss_conf.dtim_period
	 * should be 0 as we start up (we also reset it on teardown).
	 */
	if (vif->cfg.assoc &&
	    vif->bss_conf.dtim_period != ni->ni_dtim_period &&
	    ni->ni_dtim_period > 0) {
		vif->bss_conf.dtim_period = ni->ni_dtim_period;
		bss_changed |= BSS_CHANGED_BEACON_INFO;
	}

	vif->bss_conf.sync_dtim_count = ni->ni_dtim_count;
	vif->bss_conf.sync_tsf = le64toh(ni->ni_tstamp.tsf);
	/* vif->bss_conf.sync_device_ts = set in linuxkpi_ieee80211_rx. */

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE)
		printf("%s:%d [%s:%d] assoc %d aid %d beacon_int %u "
		    "dtim_period %u sync_dtim_count %u sync_tsf %ju "
		    "sync_device_ts %u bss_changed %#010jx\n",
			__func__, __LINE__, _f, _l,
			vif->cfg.assoc, vif->cfg.aid,
			vif->bss_conf.beacon_int, vif->bss_conf.dtim_period,
			vif->bss_conf.sync_dtim_count,
			(uintmax_t)vif->bss_conf.sync_tsf,
			vif->bss_conf.sync_device_ts,
			(uintmax_t)bss_changed);
#endif

	return (bss_changed);
}

static void
lkpi_stop_hw_scan(struct lkpi_hw *lhw, struct ieee80211_vif *vif)
{
	struct ieee80211_hw *hw;
	int error;
	bool cancel;

	TRACE_SCAN(lhw->ic, "scan_flags %b", lhw->scan_flags, LKPI_LHW_SCAN_BITS);

	LKPI_80211_LHW_SCAN_LOCK(lhw);
	cancel = (lhw->scan_flags & LKPI_LHW_SCAN_RUNNING) != 0;
	LKPI_80211_LHW_SCAN_UNLOCK(lhw);
	if (!cancel)
		return;

	hw = LHW_TO_HW(lhw);

	IEEE80211_UNLOCK(lhw->ic);
	wiphy_lock(hw->wiphy);
	/* Need to cancel the scan. */
	lkpi_80211_mo_cancel_hw_scan(hw, vif);
	wiphy_unlock(hw->wiphy);

	/* Need to make sure we see ieee80211_scan_completed. */
	LKPI_80211_LHW_SCAN_LOCK(lhw);
	if ((lhw->scan_flags & LKPI_LHW_SCAN_RUNNING) != 0)
		error = msleep(lhw, &lhw->scan_mtx, 0, "lhwscanstop", hz/2);
	cancel = (lhw->scan_flags & LKPI_LHW_SCAN_RUNNING) != 0;
	LKPI_80211_LHW_SCAN_UNLOCK(lhw);

	IEEE80211_LOCK(lhw->ic);

	if (cancel)
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

static enum ieee80211_bss_changed
lkpi_disassoc(struct ieee80211_sta *sta, struct ieee80211_vif *vif,
    struct lkpi_hw *lhw)
{
	enum ieee80211_bss_changed changed;
	struct lkpi_vif *lvif;

	changed = 0;
	sta->aid = 0;
	if (vif->cfg.assoc) {

		vif->cfg.assoc = false;
		vif->cfg.aid = 0;
		changed |= BSS_CHANGED_ASSOC;
		IMPROVE();

		lkpi_update_mcast_filter(lhw->ic);

		/*
		 * Executing the bss_info_changed(BSS_CHANGED_ASSOC) with
		 * assoc = false right away here will remove the sta from
		 * firmware for iwlwifi.
		 * We no longer do this but only return the BSS_CHNAGED value.
		 * The caller is responsible for removing the sta gong to
		 * IEEE80211_STA_NOTEXIST and then executing the
		 * bss_info_changed() update.
		 * See lkpi_sta_run_to_init() for more detailed comment.
		 */

		lvif = VIF_TO_LVIF(vif);
		lvif->beacons = 0;
	}

	return (changed);
}

static void
lkpi_wake_tx_queues(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
    bool dequeue_seen, bool no_emptyq)
{
	struct lkpi_txq *ltxq;
	int tid;
	bool ltxq_empty;

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

		LKPI_80211_LTXQ_LOCK(ltxq);
		ltxq_empty = skb_queue_empty(&ltxq->skbq);
		LKPI_80211_LTXQ_UNLOCK(ltxq);
		if (no_emptyq && ltxq_empty)
			continue;

		lkpi_80211_mo_wake_tx_queue(hw, sta->txq[tid]);
	}
}

/*
 * On the way down from RUN -> ASSOC -> AUTH we may send a DISASSOC or DEAUTH
 * packet.  The problem is that the state machine functions tend to hold the
 * LHW lock which will prevent lkpi_80211_txq_tx_one() from sending the packet.
 * We call this after dropping the ic lock and before acquiring the LHW lock.
 * we make sure no further packets are queued and if they are queued the task
 * will finish or be cancelled.  At the end if a packet is left we manually
 * send it.  scan_to_auth() would re-enable sending if the lsta would be
 * re-used.
 */
static void
lkpi_80211_flush_tx(struct lkpi_hw *lhw, struct lkpi_sta *lsta)
{
	struct ieee80211_hw *hw;
	struct mbufq mq;
	struct mbuf *m;
	int len;

	/* There is no lockdep_assert_not_held_wiphy(). */
	hw = LHW_TO_HW(lhw);
	lockdep_assert_not_held(&hw->wiphy->mtx);

	/* Do not accept any new packets until scan_to_auth or lsta_free(). */
	LKPI_80211_LSTA_TXQ_LOCK(lsta);
	lsta->txq_ready = false;
	LKPI_80211_LSTA_TXQ_UNLOCK(lsta);

	while (taskqueue_cancel(taskqueue_thread, &lsta->txq_task, NULL) != 0)
		taskqueue_drain(taskqueue_thread, &lsta->txq_task);

	LKPI_80211_LSTA_TXQ_LOCK(lsta);
	len = mbufq_len(&lsta->txq);
	if (len <= 0) {
		LKPI_80211_LSTA_TXQ_UNLOCK(lsta);
		return;
	}

	mbufq_init(&mq, IFQ_MAXLEN);
	mbufq_concat(&mq, &lsta->txq);
	LKPI_80211_LSTA_TXQ_UNLOCK(lsta);

	m = mbufq_dequeue(&mq);
	while (m != NULL) {
		lkpi_80211_txq_tx_one(lsta, m);
		m = mbufq_dequeue(&mq);
	}
}


static void
lkpi_remove_chanctx(struct ieee80211_hw *hw, struct ieee80211_vif *vif)
{
	struct ieee80211_chanctx_conf *chanctx_conf;
	struct lkpi_chanctx *lchanctx;

	chanctx_conf = rcu_dereference_protected(vif->bss_conf.chanctx_conf,
	    lockdep_is_held(&hw->wiphy->mtx));

	if (chanctx_conf == NULL)
		return;

	/* Remove vif context. */
	lkpi_80211_mo_unassign_vif_chanctx(hw, vif, &vif->bss_conf, chanctx_conf);

	lkpi_hw_conf_idle(hw, true);

	/* Remove chan ctx. */
	lkpi_80211_mo_remove_chanctx(hw, chanctx_conf);

	/* Cleanup. */
	rcu_assign_pointer(vif->bss_conf.chanctx_conf, NULL);
	lchanctx = CHANCTX_CONF_TO_LCHANCTX(chanctx_conf);
	list_del(&lchanctx->entry);
	free(lchanctx, M_LKPI80211);
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
	struct lkpi_chanctx *lchanctx;
	struct ieee80211_chanctx_conf *chanctx_conf;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct ieee80211_node *ni;
	struct lkpi_sta *lsta;
	enum ieee80211_bss_changed bss_changed;
	struct ieee80211_prep_tx_info prep_tx_info;
	uint32_t changed;
	int error;
	bool synched;

	/*
	 * In here we use vap->iv_bss until lvif->lvif_bss is set.
	 * For all later (STATE >= AUTH) functions we need to use the lvif
	 * cache which will be tracked even through (*iv_update_bss)().
	 */

	if (vap->iv_bss == NULL) {
		ic_printf(vap->iv_ic, "%s: no iv_bss for vap %p\n", __func__, vap);
		return (EINVAL);
	}
	/*
	 * Keep the ni alive locally.  In theory (and practice) iv_bss can change
	 * once we unlock here.  This is due to net80211 allowing state changes
	 * and new join1() despite having an active node as well as due to
	 * the fact that the iv_bss can be swapped under the hood in (*iv_update_bss).
	 */
	ni = ieee80211_ref_node(vap->iv_bss);
	if (ni->ni_chan == NULL || ni->ni_chan == IEEE80211_CHAN_ANYC) {
		ic_printf(vap->iv_ic, "%s: no channel set for iv_bss ni %p "
		    "on vap %p\n", __func__, ni, vap);
		ieee80211_free_node(ni);	/* Error handling for the local ni. */
		return (EINVAL);
	}

	lhw = vap->iv_ic->ic_softc;
	chan = lkpi_find_lkpi80211_chan(lhw, ni->ni_chan);
	if (chan == NULL) {
		ic_printf(vap->iv_ic, "%s: failed to get LKPI channel from "
		    "iv_bss ni %p on vap %p\n", __func__, ni, vap);
		ieee80211_free_node(ni);	/* Error handling for the local ni. */
		return (ESRCH);
	}

	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	LKPI_80211_LVIF_LOCK(lvif);
	/* XXX-BZ KASSERT later? */
	if (lvif->lvif_bss_synched || lvif->lvif_bss != NULL) {
		ic_printf(vap->iv_ic, "%s:%d: lvif %p vap %p iv_bss %p lvif_bss %p "
		    "lvif_bss->ni %p synched %d\n", __func__, __LINE__,
		    lvif, vap, vap->iv_bss, lvif->lvif_bss,
		    (lvif->lvif_bss != NULL) ? lvif->lvif_bss->ni : NULL,
		    lvif->lvif_bss_synched);
		LKPI_80211_LVIF_UNLOCK(lvif);
		ieee80211_free_node(ni);	/* Error handling for the local ni. */
		return (EBUSY);
	}
	LKPI_80211_LVIF_UNLOCK(lvif);

	IEEE80211_UNLOCK(vap->iv_ic);
	wiphy_lock(hw->wiphy);

	/* Add chanctx (or if exists, change it). */
	chanctx_conf = rcu_dereference_protected(vif->bss_conf.chanctx_conf,
	    lockdep_is_held(&hw->wiphy->mtx));
	if (chanctx_conf != NULL) {
		lchanctx = CHANCTX_CONF_TO_LCHANCTX(chanctx_conf);
		IMPROVE("diff changes for changed, working on live copy, rcu");
	} else {
		/* Keep separate alloc as in Linux this is rcu managed? */
		lchanctx = malloc(sizeof(*lchanctx) + hw->chanctx_data_size,
		    M_LKPI80211, M_WAITOK | M_ZERO);
		chanctx_conf = &lchanctx->chanctx_conf;
	}

	chanctx_conf->rx_chains_static = 1;
	chanctx_conf->rx_chains_dynamic = 1;
	chanctx_conf->radar_enabled =
	    (chan->flags & IEEE80211_CHAN_RADAR) ? true : false;
	chanctx_conf->def.chan = chan;
	chanctx_conf->def.width = NL80211_CHAN_WIDTH_20_NOHT;
	chanctx_conf->def.center_freq1 = ieee80211_get_channel_center_freq1(ni->ni_chan);
	chanctx_conf->def.center_freq2 = ieee80211_get_channel_center_freq2(ni->ni_chan);
	IMPROVE("Check vht_cap from band not just chan?");
	KASSERT(ni->ni_chan != NULL && ni->ni_chan != IEEE80211_CHAN_ANYC,
	   ("%s:%d: ni %p ni_chan %p\n", __func__, __LINE__, ni, ni->ni_chan));

#ifdef LKPI_80211_HT
	if (IEEE80211_IS_CHAN_HT(ni->ni_chan)) {
		if (IEEE80211_IS_CHAN_HT40(ni->ni_chan))
			chanctx_conf->def.width = NL80211_CHAN_WIDTH_40;
		else
			chanctx_conf->def.width = NL80211_CHAN_WIDTH_20;
	}
#endif
#ifdef LKPI_80211_VHT
	if (IEEE80211_IS_CHAN_VHT_5GHZ(ni->ni_chan)) {
		if (IEEE80211_IS_CHAN_VHT80P80(ni->ni_chan))
			chanctx_conf->def.width = NL80211_CHAN_WIDTH_80P80;
		else if (IEEE80211_IS_CHAN_VHT160(ni->ni_chan))
			chanctx_conf->def.width = NL80211_CHAN_WIDTH_160;
		else if (IEEE80211_IS_CHAN_VHT80(ni->ni_chan))
			chanctx_conf->def.width = NL80211_CHAN_WIDTH_80;
	}
#endif
	chanctx_conf->rx_chains_dynamic = lkpi_get_max_rx_chains(ni);
	/* Responder ... */
#if 0
	chanctx_conf->min_def.chan = chanctx_conf->def.chan;
	chanctx_conf->min_def.width = NL80211_CHAN_WIDTH_20_NOHT;
#ifdef LKPI_80211_HT
	if (IEEE80211_IS_CHAN_HT(ni->ni_chan) || IEEE80211_IS_CHAN_VHT(ni->ni_chan))
		chanctx_conf->min_def.width = NL80211_CHAN_WIDTH_20;
#endif
	chanctx_conf->min_def.center_freq1 = chanctx_conf->def.center_freq1;
	chanctx_conf->min_def.center_freq2 = chanctx_conf->def.center_freq2;
#else
	chanctx_conf->min_def = chanctx_conf->def;
#endif

	/* Set bss info (bss_info_changed). */
	bss_changed = 0;
	vif->bss_conf.bssid = ni->ni_bssid;
	bss_changed |= BSS_CHANGED_BSSID;
	vif->bss_conf.txpower = ni->ni_txpower;
	bss_changed |= BSS_CHANGED_TXPOWER;
	vif->cfg.idle = false;
	bss_changed |= BSS_CHANGED_IDLE;

	/* vif->bss_conf.basic_rates ? Where exactly? */

	lvif->beacons = 0;
	/* Should almost assert it is this. */
	vif->cfg.assoc = false;
	vif->cfg.aid = 0;

	bss_changed |= lkpi_update_dtim_tsf(vif, ni, vap, __func__, __LINE__);

	error = 0;
	if (vif->bss_conf.chanctx_conf == chanctx_conf) {
		changed = IEEE80211_CHANCTX_CHANGE_MIN_WIDTH;
		changed |= IEEE80211_CHANCTX_CHANGE_RADAR;
		changed |= IEEE80211_CHANCTX_CHANGE_RX_CHAINS;
		changed |= IEEE80211_CHANCTX_CHANGE_WIDTH;
		lkpi_80211_mo_change_chanctx(hw, chanctx_conf, changed);
	} else {
		/* The device is no longer idle. */
		IMPROVE("Once we do multi-vif, only do for 1st chanctx");
		lkpi_hw_conf_idle(hw, false);

		error = lkpi_80211_mo_add_chanctx(hw, chanctx_conf);
		if (error == 0 || error == EOPNOTSUPP) {
			vif->bss_conf.chanreq.oper.chan = chanctx_conf->def.chan;
			vif->bss_conf.chanreq.oper.width = chanctx_conf->def.width;
			vif->bss_conf.chanreq.oper.center_freq1 =
			    chanctx_conf->def.center_freq1;
			vif->bss_conf.chanreq.oper.center_freq2 =
			    chanctx_conf->def.center_freq2;
		} else {
			ic_printf(vap->iv_ic, "%s:%d: mo_add_chanctx "
			    "failed: %d\n", __func__, __LINE__, error);
			goto out;
		}

		list_add_rcu(&lchanctx->entry, &lhw->lchanctx_list);
		rcu_assign_pointer(vif->bss_conf.chanctx_conf, chanctx_conf);

		/* Assign vif chanctx. */
		if (error == 0)
			error = lkpi_80211_mo_assign_vif_chanctx(hw, vif,
			    &vif->bss_conf, chanctx_conf);
		if (error == EOPNOTSUPP)
			error = 0;
		if (error != 0) {
			ic_printf(vap->iv_ic, "%s:%d: mo_assign_vif_chanctx "
			    "failed: %d\n", __func__, __LINE__, error);
			lkpi_80211_mo_remove_chanctx(hw, chanctx_conf);
			rcu_assign_pointer(vif->bss_conf.chanctx_conf, NULL);
			lchanctx = CHANCTX_CONF_TO_LCHANCTX(chanctx_conf);
			list_del(&lchanctx->entry);
			free(lchanctx, M_LKPI80211);
			goto out;
		}
	}
	IMPROVE("update radiotap chan fields too");

	/* RATES */
	IMPROVE("bss info: not all needs to come now and rates are missing");
	lkpi_80211_mo_bss_info_changed(hw, vif, &vif->bss_conf, bss_changed);

	/*
	 * Given ni and lsta are 1:1 from alloc to free we can assert that
	 * ni always has lsta data attach despite net80211 node swapping
	 * under the hoods.
	 */
	KASSERT(ni->ni_drv_data != NULL, ("%s: ni %p ni_drv_data %p\n",
	    __func__, ni, ni->ni_drv_data));
	lsta = ni->ni_drv_data;

	/* Insert the [l]sta into the list of known stations. */
	list_add_tail(&lsta->lsta_list, &lvif->lsta_list);

	/* Add (or adjust) sta and change state (from NOTEXIST) to NONE. */
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_NOTEXIST, ("%s: lsta %p state not "
	    "NOTEXIST: %#x\n", __func__, lsta, lsta->state));
	error = lkpi_80211_mo_sta_state(hw, vif, lsta, IEEE80211_STA_NONE);
	if (error != 0) {
		IMPROVE("do we need to undo the chan ctx?");
		ic_printf(vap->iv_ic, "%s:%d: mo_sta_state(NONE) "
		    "failed: %d\n", __func__, __LINE__, error);
		goto out;
	}
#if 0
	lsta->added_to_drv = true;	/* mo manages. */
#endif

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);

#if 0
	/*
	 * Wakeup all queues now that sta is there so we have as much time to
	 * possibly prepare the queue in the driver to be ready for the 1st
	 * packet;  lkpi_80211_txq_tx_one() still has a workaround as there
	 * is no guarantee or way to check.
	 * XXX-BZ and by now we know that this does not work on all drivers
	 * for all queues.
	 */
	lkpi_wake_tx_queues(hw, LSTA_TO_STA(lsta), false, false);
#endif

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

	wiphy_unlock(hw->wiphy);
	IEEE80211_LOCK(vap->iv_ic);

	LKPI_80211_LVIF_LOCK(lvif);
	/* Re-check given (*iv_update_bss) could have happened while we were unlocked. */
	if (lvif->lvif_bss_synched || lvif->lvif_bss != NULL ||
	    lsta->ni != vap->iv_bss)
		ic_printf(vap->iv_ic, "%s:%d: lvif %p vap %p iv_bss %p lvif_bss %p "
		    "lvif_bss->ni %p synched %d, ni %p lsta %p\n", __func__, __LINE__,
		    lvif, vap, vap->iv_bss, lvif->lvif_bss,
		    (lvif->lvif_bss != NULL) ? lvif->lvif_bss->ni : NULL,
		    lvif->lvif_bss_synched, ni, lsta);

	/*
	 * Reference the "ni" for caching the lsta/ni in lvif->lvif_bss.
	 * Given we cache lsta we use lsta->ni instead of ni here (even though
	 * lsta->ni == ni) to be distinct from the rest of the code where we do
	 * assume that ni == vap->iv_bss which it may or may not be.
	 * So do NOT use iv_bss here anymore as that may have diverged from our
	 * function local ni already while ic was unlocked and would lead to
	 * inconsistencies.  Go and see if we lost a race and do not update
	 * lvif_bss_synched in that case.
	 */
	ieee80211_ref_node(lsta->ni);
	lvif->lvif_bss = lsta;
	if (lsta->ni == vap->iv_bss) {
		lvif->lvif_bss_synched = synched = true;
	} else {
		/* Set to un-synched no matter what. */
		lvif->lvif_bss_synched = synched = false;
		/*
		 * We do not error as someone has to take us down.
		 * If we are followed by a 2nd, new net80211::join1() going to
		 * AUTH lkpi_sta_a_to_a() will error, lkpi_sta_auth_to_{scan,init}()
		 * will take the lvif->lvif_bss node down eventually.
		 * What happens with the vap->iv_bss node will entirely be up
		 * to net80211 as we never used the node beyond alloc()/free()
		 * and we do not hold an extra reference for that anymore given
		 * ni : lsta == 1:1.
		 * Problem is if we do not error a MGMT/AUTH frame will be
		 * sent from net80211::sta_newstate(); disable lsta queue below.
		 */
	}
	LKPI_80211_LVIF_UNLOCK(lvif);
	/*
	 * Make sure in case the sta did not change and we re-added it,
	 * that we can tx again but only if the vif/iv_bss are in sync.
	 * Otherwise this should prevent the MGMT/AUTH frame from being
	 * sent triggering a warning in iwlwifi.
	 */
	LKPI_80211_LSTA_TXQ_LOCK(lsta);
	lsta->txq_ready = synched;
	LKPI_80211_LSTA_TXQ_UNLOCK(lsta);
	goto out_relocked;

out:
	wiphy_unlock(hw->wiphy);
	IEEE80211_LOCK(vap->iv_ic);
out_relocked:
	/*
	 * Release the reference that kept the ni stable locally
	 * during the work of this function.
	 */
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
	enum ieee80211_bss_changed bss_changed;
	int error;

	lhw = vap->iv_ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	LKPI_80211_LVIF_LOCK(lvif);
#ifdef LINUXKPI_DEBUG_80211
	/* XXX-BZ KASSERT later; state going down so no action. */
	if (lvif->lvif_bss == NULL)
		ic_printf(vap->iv_ic, "%s:%d: lvif %p vap %p iv_bss %p lvif_bss %p "
		    "lvif_bss->ni %p synched %d\n", __func__, __LINE__,
		    lvif, vap, vap->iv_bss, lvif->lvif_bss,
		    (lvif->lvif_bss != NULL) ? lvif->lvif_bss->ni : NULL,
		    lvif->lvif_bss_synched);
#endif

	lsta = lvif->lvif_bss;
	LKPI_80211_LVIF_UNLOCK(lvif);
	KASSERT(lsta != NULL && lsta->ni != NULL, ("%s: lsta %p ni %p "
	    "lvif %p vap %p\n", __func__,
	    lsta, (lsta != NULL) ? lsta->ni : NULL, lvif, vap));
	ni = lsta->ni;			/* Reference held for lvif_bss. */
	sta = LSTA_TO_STA(lsta);

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);

	IEEE80211_UNLOCK(vap->iv_ic);
	wiphy_lock(hw->wiphy);

	/* flush, drop. */
	lkpi_80211_mo_flush(hw, vif,  nitems(sta->txq), true);

	/* Wake tx queues to get packet(s) out. */
	lkpi_wake_tx_queues(hw, sta, false, true);

	/* flush, no drop */
	lkpi_80211_mo_flush(hw, vif,  nitems(sta->txq), false);

	/* End mgd_complete_tx. */
	if (lsta->in_mgd) {
		memset(&prep_tx_info, 0, sizeof(prep_tx_info));
		prep_tx_info.success = false;
		lkpi_80211_mo_mgd_complete_tx(hw, vif, &prep_tx_info);
		lsta->in_mgd = false;
	}

	/* sync_rx_queues */
	lkpi_80211_mo_sync_rx_queues(hw);

	/* sta_pre_rcu_remove */
        lkpi_80211_mo_sta_pre_rcu_remove(hw, vif, sta);

	/* Take the station down. */

	/* Adjust sta and change state (from NONE) to NOTEXIST. */
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_NONE, ("%s: lsta %p state not "
	    "NONE: %#x, nstate %d arg %d\n", __func__, lsta, lsta->state, nstate, arg));
	error = lkpi_80211_mo_sta_state(hw, vif, lsta, IEEE80211_STA_NOTEXIST);
	if (error != 0) {
		IMPROVE("do we need to undo the chan ctx?");
		ic_printf(vap->iv_ic, "%s:%d: mo_sta_state(NOTEXIST) "
		    "failed: %d\n", __func__, __LINE__, error);
		goto out;
	}
#if 0
	lsta->added_to_drv = false;	/* mo manages. */
#endif

	bss_changed = 0;
	vif->bss_conf.dtim_period = 0; /* go back to 0. */
	bss_changed |= BSS_CHANGED_BEACON_INFO;
	lkpi_80211_mo_bss_info_changed(hw, vif, &vif->bss_conf, bss_changed);

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);

	LKPI_80211_LVIF_LOCK(lvif);
	/* Remove ni reference for this cache of lsta. */
	lvif->lvif_bss = NULL;
	lvif->lvif_bss_synched = false;
	LKPI_80211_LVIF_UNLOCK(lvif);
	lkpi_lsta_remove(lsta, lvif);

	/* conf_tx */

	lkpi_remove_chanctx(hw, vif);

out:
	wiphy_unlock(hw->wiphy);
	IEEE80211_LOCK(vap->iv_ic);
	if (error == 0) {
		/*
		 * We do this outside the wiphy lock as net80211::node_free() may call
		 * into crypto code to delete keys and we have a recursed on
		 * non-recursive sx panic.  Also only do this if we get here w/o error.
		 *
		 * The very last release the reference on the ni for the ni/lsta on
		 * lvif->lvif_bss.  Upon return from this both ni and lsta are invalid
		 * and potentially freed.
		 */
		ieee80211_free_node(ni);
	}
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
	struct lkpi_sta *lsta;
	struct ieee80211_prep_tx_info prep_tx_info;
	int error;

	lhw = vap->iv_ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	IEEE80211_UNLOCK(vap->iv_ic);
	wiphy_lock(hw->wiphy);

	LKPI_80211_LVIF_LOCK(lvif);
	/* XXX-BZ KASSERT later? */
	if (!lvif->lvif_bss_synched || lvif->lvif_bss == NULL) {
#ifdef LINUXKPI_DEBUG_80211
		ic_printf(vap->iv_ic, "%s:%d: lvif %p vap %p iv_bss %p lvif_bss %p "
		    "lvif_bss->ni %p synched %d\n", __func__, __LINE__,
		    lvif, vap, vap->iv_bss, lvif->lvif_bss,
		    (lvif->lvif_bss != NULL) ? lvif->lvif_bss->ni : NULL,
		    lvif->lvif_bss_synched);
#endif
		error = ENOTRECOVERABLE;
		LKPI_80211_LVIF_UNLOCK(lvif);
		goto out;
	}
	lsta = lvif->lvif_bss;
	LKPI_80211_LVIF_UNLOCK(lvif);

	KASSERT(lsta != NULL, ("%s: lsta %p\n", __func__, lsta));

	/* Finish auth. */
	IMPROVE("event callback");

	/* Update sta_state (NONE to AUTH). */
	KASSERT(lsta->state == IEEE80211_STA_NONE, ("%s: lsta %p state not "
	    "NONE: %#x\n", __func__, lsta, lsta->state));
	error = lkpi_80211_mo_sta_state(hw, vif, lsta, IEEE80211_STA_AUTH);
	if (error != 0) {
		ic_printf(vap->iv_ic, "%s:%d: mo_sta_state(AUTH) "
		    "failed: %d\n", __func__, __LINE__, error);
		goto out;
	}

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
	lkpi_wake_tx_queues(hw, LSTA_TO_STA(lsta), false, true);

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
	wiphy_unlock(hw->wiphy);
	IEEE80211_LOCK(vap->iv_ic);
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
	struct lkpi_sta *lsta;
	struct ieee80211_prep_tx_info prep_tx_info;
	int error;

	lhw = vap->iv_ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	IEEE80211_UNLOCK(vap->iv_ic);
	wiphy_lock(hw->wiphy);

	LKPI_80211_LVIF_LOCK(lvif);
	/* XXX-BZ KASSERT later? */
	if (!lvif->lvif_bss_synched || lvif->lvif_bss == NULL) {
#ifdef LINUXKPI_DEBUG_80211
		ic_printf(vap->iv_ic, "%s:%d: lvif %p vap %p iv_bss %p lvif_bss %p "
		    "lvif_bss->ni %p synched %d\n", __func__, __LINE__,
		    lvif, vap, vap->iv_bss, lvif->lvif_bss,
		    (lvif->lvif_bss != NULL) ? lvif->lvif_bss->ni : NULL,
		    lvif->lvif_bss_synched);
#endif
		LKPI_80211_LVIF_UNLOCK(lvif);
		error = ENOTRECOVERABLE;
		goto out;
	}
	lsta = lvif->lvif_bss;
	LKPI_80211_LVIF_UNLOCK(lvif);

	KASSERT(lsta != NULL, ("%s: lsta %p! lvif %p vap %p\n", __func__,
	    lsta, lvif, vap));

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

	error = 0;
out:
	wiphy_unlock(hw->wiphy);
	IEEE80211_LOCK(vap->iv_ic);

	return (error);
}

static int
_lkpi_sta_assoc_to_down(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
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
	wiphy_lock(hw->wiphy);

	LKPI_80211_LVIF_LOCK(lvif);
#ifdef LINUXKPI_DEBUG_80211
	/* XXX-BZ KASSERT later; state going down so no action. */
	if (lvif->lvif_bss == NULL)
		ic_printf(vap->iv_ic, "%s:%d: lvif %p vap %p iv_bss %p lvif_bss %p "
		    "lvif_bss->ni %p synched %d\n", __func__, __LINE__,
		    lvif, vap, vap->iv_bss, lvif->lvif_bss,
		    (lvif->lvif_bss != NULL) ? lvif->lvif_bss->ni : NULL,
		    lvif->lvif_bss_synched);
#endif
	lsta = lvif->lvif_bss;
	LKPI_80211_LVIF_UNLOCK(lvif);
	KASSERT(lsta != NULL && lsta->ni != NULL, ("%s: lsta %p ni %p "
	    "lvif %p vap %p\n", __func__,
	    lsta, (lsta != NULL) ? lsta->ni : NULL, lvif, vap));

	ni = lsta->ni;		/* Reference held for lvif_bss. */
	sta = LSTA_TO_STA(lsta);

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);

	/* flush, drop. */
	lkpi_80211_mo_flush(hw, vif,  nitems(sta->txq), true);

	IMPROVE("What are the proper conditions for DEAUTH_NEED_MGD_TX_PREP?");
	if (ieee80211_hw_check(hw, DEAUTH_NEED_MGD_TX_PREP) &&
	    !lsta->in_mgd) {
		memset(&prep_tx_info, 0, sizeof(prep_tx_info));
		prep_tx_info.duration = PREP_TX_INFO_DURATION;
		prep_tx_info.was_assoc = true;
		lkpi_80211_mo_mgd_prepare_tx(hw, vif, &prep_tx_info);
		lsta->in_mgd = true;
	}

	wiphy_unlock(hw->wiphy);
	IEEE80211_LOCK(vap->iv_ic);

	/* Call iv_newstate first so we get potential DEAUTH packet out. */
	error = lvif->iv_newstate(vap, nstate, arg);
	if (error != 0) {
		ic_printf(vap->iv_ic, "%s:%d: iv_newstate(%p, %d, %d) "
		    "failed: %d\n", __func__, __LINE__, vap, nstate, arg, error);
		goto outni;
	}

	IEEE80211_UNLOCK(vap->iv_ic);

	/* Ensure the packets get out. */
	lkpi_80211_flush_tx(lhw, lsta);

	wiphy_lock(hw->wiphy);

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);

	/* Wake tx queues to get packet(s) out. */
	lkpi_wake_tx_queues(hw, sta, false, true);

	/* flush, no drop */
	lkpi_80211_mo_flush(hw, vif,  nitems(sta->txq), false);

	/* End mgd_complete_tx. */
	if (lsta->in_mgd) {
		memset(&prep_tx_info, 0, sizeof(prep_tx_info));
		prep_tx_info.success = false;
		prep_tx_info.was_assoc = true;
		lkpi_80211_mo_mgd_complete_tx(hw, vif, &prep_tx_info);
		lsta->in_mgd = false;
	}

	/* sync_rx_queues */
	lkpi_80211_mo_sync_rx_queues(hw);

	/* sta_pre_rcu_remove */
        lkpi_80211_mo_sta_pre_rcu_remove(hw, vif, sta);

	/* Take the station down. */

	/* Update sta and change state (from AUTH) to NONE. */
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_AUTH, ("%s: lsta %p state not "
	    "AUTH: %#x\n", __func__, lsta, lsta->state));
	error = lkpi_80211_mo_sta_state(hw, vif, lsta, IEEE80211_STA_NONE);
	if (error != 0) {
		ic_printf(vap->iv_ic, "%s:%d: mo_sta_state(NONE) "
		    "failed: %d\n", __func__, __LINE__, error);
		goto out;
	}

	/* See comment in lkpi_sta_run_to_init(). */
	bss_changed = 0;
	bss_changed |= lkpi_disassoc(sta, vif, lhw);

#ifdef LKPI_80211_HW_CRYPTO
	/*
	 * In theory we remove keys here but there must not exist any for this
	 * state change until we clean them up again into small steps and no
	 * code duplication.
	 */
#endif

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);

	/* Adjust sta and change state (from NONE) to NOTEXIST. */
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_NONE, ("%s: lsta %p state not "
	    "NONE: %#x, nstate %d arg %d\n", __func__, lsta, lsta->state, nstate, arg));
	error = lkpi_80211_mo_sta_state(hw, vif, lsta, IEEE80211_STA_NOTEXIST);
	if (error != 0) {
		IMPROVE("do we need to undo the chan ctx?");
		ic_printf(vap->iv_ic, "%s:%d: mo_sta_state(NOTEXIST) "
		    "failed: %d\n", __func__, __LINE__, error);
		goto out;
	}

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);	/* sta no longer save to use. */

	IMPROVE("Any bss_info changes to announce?");
	vif->bss_conf.qos = 0;
	bss_changed |= BSS_CHANGED_QOS;
	vif->cfg.ssid_len = 0;
	memset(vif->cfg.ssid, '\0', sizeof(vif->cfg.ssid));
	bss_changed |= BSS_CHANGED_BSSID;
	vif->bss_conf.dtim_period = 0; /* go back to 0. */
	bss_changed |= BSS_CHANGED_BEACON_INFO;
	lkpi_80211_mo_bss_info_changed(hw, vif, &vif->bss_conf, bss_changed);

	LKPI_80211_LVIF_LOCK(lvif);
	/* Remove ni reference for this cache of lsta. */
	lvif->lvif_bss = NULL;
	lvif->lvif_bss_synched = false;
	LKPI_80211_LVIF_UNLOCK(lvif);
	lkpi_lsta_remove(lsta, lvif);

	/* conf_tx */

	lkpi_remove_chanctx(hw, vif);

	error = EALREADY;
out:
	wiphy_unlock(hw->wiphy);
	IEEE80211_LOCK(vap->iv_ic);
	if (error == EALREADY) {
		/*
		 * We do this outside the wiphy lock as net80211::node_free() may call
		 * into crypto code to delete keys and we have a recursed on
		 * non-recursive sx panic.  Also only do this if we get here w/o error.
		 *
		 * The very last release the reference on the ni for the ni/lsta on
		 * lvif->lvif_bss.  Upon return from this both ni and lsta are invalid
		 * and potentially freed.
		 */
		ieee80211_free_node(ni);
	}
outni:
	return (error);
}

static int
lkpi_sta_assoc_to_auth(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	int error;

	error = _lkpi_sta_assoc_to_down(vap, nstate, arg);
	if (error != 0 && error != EALREADY)
		return (error);

	/* At this point iv_bss is long a new node! */

	error |= lkpi_sta_scan_to_auth(vap, nstate, 0);
	return (error);
}

static int
lkpi_sta_assoc_to_scan(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	int error;

	error = _lkpi_sta_assoc_to_down(vap, nstate, arg);
	return (error);
}

static int
lkpi_sta_assoc_to_init(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	int error;

	error = _lkpi_sta_assoc_to_down(vap, nstate, arg);
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
	wiphy_lock(hw->wiphy);

	LKPI_80211_LVIF_LOCK(lvif);
	/* XXX-BZ KASSERT later? */
	if (!lvif->lvif_bss_synched || lvif->lvif_bss == NULL) {
#ifdef LINUXKPI_DEBUG_80211
		ic_printf(vap->iv_ic, "%s:%d: lvif %p vap %p iv_bss %p lvif_bss %p "
		    "lvif_bss->ni %p synched %d\n", __func__, __LINE__,
		    lvif, vap, vap->iv_bss, lvif->lvif_bss,
		    (lvif->lvif_bss != NULL) ? lvif->lvif_bss->ni : NULL,
		    lvif->lvif_bss_synched);
#endif
		LKPI_80211_LVIF_UNLOCK(lvif);
		error = ENOTRECOVERABLE;
		goto out;
	}
	lsta = lvif->lvif_bss;
	LKPI_80211_LVIF_UNLOCK(lvif);
	KASSERT(lsta != NULL && lsta->ni != NULL, ("%s: lsta %p ni %p "
	    "lvif %p vap %p\n", __func__,
	    lsta, (lsta != NULL) ? lsta->ni : NULL, lvif, vap));

	ni = lsta->ni;		/* Reference held for lvif_bss. */

	IMPROVE("ponder some of this moved to ic_newassoc, scan_assoc_success, "
	    "and to lesser extend ieee80211_notify_node_join");

	/* Finish assoc. */
	/* Update sta_state (AUTH to ASSOC) and set aid. */
	KASSERT(lsta->state == IEEE80211_STA_AUTH, ("%s: lsta %p state not "
	    "AUTH: %#x\n", __func__, lsta, lsta->state));
	sta = LSTA_TO_STA(lsta);
	sta->aid = IEEE80211_NODE_AID(ni);
#ifdef LKPI_80211_WME
	if (vap->iv_flags & IEEE80211_F_WME)
		sta->wme = true;
#endif
	error = lkpi_80211_mo_sta_state(hw, vif, lsta, IEEE80211_STA_ASSOC);
	if (error != 0) {
		ic_printf(vap->iv_ic, "%s:%d: mo_sta_state(ASSOC) "
		    "failed: %d\n", __func__, __LINE__, error);
		goto out;
	}

	IMPROVE("wme / conf_tx [all]");

	/* Update bss info (bss_info_changed) (assoc, aid, ..). */
	bss_changed = 0;
#ifdef LKPI_80211_WME
	bss_changed |= lkpi_wme_update(lhw, vap, true);
#endif
	if (!vif->cfg.assoc || vif->cfg.aid != IEEE80211_NODE_AID(ni)) {
		lvif->beacons = 0;
		vif->cfg.assoc = true;
		vif->cfg.aid = IEEE80211_NODE_AID(ni);
		bss_changed |= BSS_CHANGED_ASSOC;
	}
	/* We set SSID but this is not BSSID! */
	vif->cfg.ssid_len = ni->ni_esslen;
	memcpy(vif->cfg.ssid, ni->ni_essid, ni->ni_esslen);
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

	bss_changed |= lkpi_update_dtim_tsf(vif, ni, vap, __func__, __LINE__);
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

	/*
	 * And then:
	 * - (more packets)?
	 * - set_key
	 * - set_default_unicast_key
	 * - set_key (?)
	 * - ipv6_addr_change (?)
	 */

	if (!ieee80211_node_is_authorized(ni)) {
		IMPROVE("net80211 does not consider node authorized");
	}

	IMPROVE("Is this the right spot, has net80211 done all updates already?");
	lkpi_sta_sync_from_ni(hw, vif, sta, ni, true);

	/* Update thresholds. */
	hw->wiphy->frag_threshold = vap->iv_fragthreshold;
	lkpi_80211_mo_set_frag_threshold(hw, vap->iv_fragthreshold);
	hw->wiphy->rts_threshold = vap->iv_rtsthreshold;
	lkpi_80211_mo_set_rts_threshold(hw, vap->iv_rtsthreshold);

	/* Update sta_state (ASSOC to AUTHORIZED). */
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_ASSOC, ("%s: lsta %p state not "
	    "ASSOC: %#x\n", __func__, lsta, lsta->state));
	error = lkpi_80211_mo_sta_state(hw, vif, lsta, IEEE80211_STA_AUTHORIZED);
	if (error != 0) {
		IMPROVE("undo some changes?");
		ic_printf(vap->iv_ic, "%s:%d: mo_sta_state(AUTHORIZED) "
		    "failed: %d\n", __func__, __LINE__, error);
		goto out;
	}

	/* - drv_config (?)
	 * - bss_info_changed
	 * - set_rekey_data (?)
	 *
	 * And now we should be passing packets.
	 */
	IMPROVE("Need that bssid setting, and the keys");

	bss_changed = 0;
	bss_changed |= lkpi_update_dtim_tsf(vif, ni, vap, __func__, __LINE__);
	lkpi_80211_mo_bss_info_changed(hw, vif, &vif->bss_conf, bss_changed);

	/* Prepare_multicast && configure_filter. */
	lkpi_update_mcast_filter(vap->iv_ic);

out:
	wiphy_unlock(hw->wiphy);
	IEEE80211_LOCK(vap->iv_ic);
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
	struct ieee80211_prep_tx_info prep_tx_info;
#if 0
	enum ieee80211_bss_changed bss_changed;
#endif
	int error;

	lhw = vap->iv_ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	LKPI_80211_LVIF_LOCK(lvif);
#ifdef LINUXKPI_DEBUG_80211
	/* XXX-BZ KASSERT later; state going down so no action. */
	if (lvif->lvif_bss == NULL)
		ic_printf(vap->iv_ic, "%s:%d: lvif %p vap %p iv_bss %p lvif_bss %p "
		    "lvif_bss->ni %p synched %d\n", __func__, __LINE__,
		    lvif, vap, vap->iv_bss, lvif->lvif_bss,
		    (lvif->lvif_bss != NULL) ? lvif->lvif_bss->ni : NULL,
		    lvif->lvif_bss_synched);
#endif
	lsta = lvif->lvif_bss;
	LKPI_80211_LVIF_UNLOCK(lvif);
	KASSERT(lsta != NULL && lsta->ni != NULL, ("%s: lsta %p ni %p "
	    "lvif %p vap %p\n", __func__,
	    lsta, (lsta != NULL) ? lsta->ni : NULL, lvif, vap));

	ni = lsta->ni;		/* Reference held for lvif_bss. */
	sta = LSTA_TO_STA(lsta);

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);

	IEEE80211_UNLOCK(vap->iv_ic);
	wiphy_lock(hw->wiphy);

	/* flush, drop. */
	lkpi_80211_mo_flush(hw, vif,  nitems(sta->txq), true);

	IMPROVE("What are the proper conditions for DEAUTH_NEED_MGD_TX_PREP?");
	if (ieee80211_hw_check(hw, DEAUTH_NEED_MGD_TX_PREP) &&
	    !lsta->in_mgd) {
		memset(&prep_tx_info, 0, sizeof(prep_tx_info));
		prep_tx_info.duration = PREP_TX_INFO_DURATION;
		prep_tx_info.was_assoc = true;
		lkpi_80211_mo_mgd_prepare_tx(hw, vif, &prep_tx_info);
		lsta->in_mgd = true;
	}

	wiphy_unlock(hw->wiphy);
	IEEE80211_LOCK(vap->iv_ic);

	/* Call iv_newstate first so we get potential DISASSOC packet out. */
	error = lvif->iv_newstate(vap, nstate, arg);
	if (error != 0) {
		ic_printf(vap->iv_ic, "%s:%d: iv_newstate(%p, %d, %d) "
		    "failed: %d\n", __func__, __LINE__, vap, nstate, arg, error);
		goto outni;
	}

	IEEE80211_UNLOCK(vap->iv_ic);

	/* Ensure the packets get out. */
	lkpi_80211_flush_tx(lhw, lsta);

	wiphy_lock(hw->wiphy);

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);

	/* Wake tx queues to get packet(s) out. */
	lkpi_wake_tx_queues(hw, sta, false, true);

	/* flush, no drop */
	lkpi_80211_mo_flush(hw, vif,  nitems(sta->txq), false);

	/* End mgd_complete_tx. */
	if (lsta->in_mgd) {
		memset(&prep_tx_info, 0, sizeof(prep_tx_info));
		prep_tx_info.success = false;
		prep_tx_info.was_assoc = true;
		lkpi_80211_mo_mgd_complete_tx(hw, vif, &prep_tx_info);
		lsta->in_mgd = false;
	}

#if 0
	/* sync_rx_queues */
	lkpi_80211_mo_sync_rx_queues(hw);

	/* sta_pre_rcu_remove */
        lkpi_80211_mo_sta_pre_rcu_remove(hw, vif, sta);
#endif

	/* Take the station down. */

	/* Adjust sta and change state (from AUTHORIZED) to ASSOC. */
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_AUTHORIZED, ("%s: lsta %p state not "
	    "AUTHORIZED: %#x\n", __func__, lsta, lsta->state));
	error = lkpi_80211_mo_sta_state(hw, vif, lsta, IEEE80211_STA_ASSOC);
	if (error != 0) {
		ic_printf(vap->iv_ic, "%s:%d: mo_sta_state(ASSOC) "
		    "failed: %d\n", __func__, __LINE__, error);
		goto out;
	}

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);

#ifdef LKPI_80211_HW_CRYPTO
	if (lkpi_hwcrypto) {
		error = lkpi_sta_del_keys(hw, vif, lsta);
		if (error != 0) {
			ic_printf(vap->iv_ic, "%s:%d: lkpi_sta_del_keys "
			    "failed: %d\n", __func__, __LINE__, error);
			/*
			 * Either drv/fw will crash or cleanup itself,
			 * otherwise net80211 will delete the keys (at a
			 * less appropriate time).
			 */
			/* goto out; */
		}
	}
#endif

	/* Update sta_state (ASSOC to AUTH). */
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_ASSOC, ("%s: lsta %p state not "
	    "ASSOC: %#x\n", __func__, lsta, lsta->state));
	error = lkpi_80211_mo_sta_state(hw, vif, lsta, IEEE80211_STA_AUTH);
	if (error != 0) {
		ic_printf(vap->iv_ic, "%s:%d: mo_sta_state(AUTH) "
		    "failed: %d\n", __func__, __LINE__, error);
		goto out;
	}

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);

#if 0
	/* Update bss info (bss_info_changed) (assoc, aid, ..). */
	lkpi_disassoc(sta, vif, lhw);
#endif

	error = EALREADY;
out:
	wiphy_unlock(hw->wiphy);
	IEEE80211_LOCK(vap->iv_ic);
outni:
	return (error);
}

static int
lkpi_sta_run_to_init(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
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
	wiphy_lock(hw->wiphy);

	LKPI_80211_LVIF_LOCK(lvif);
#ifdef LINUXKPI_DEBUG_80211
	/* XXX-BZ KASSERT later; state going down so no action. */
	if (lvif->lvif_bss == NULL)
		ic_printf(vap->iv_ic, "%s:%d: lvif %p vap %p iv_bss %p lvif_bss %p "
		    "lvif_bss->ni %p synched %d\n", __func__, __LINE__,
		    lvif, vap, vap->iv_bss, lvif->lvif_bss,
		    (lvif->lvif_bss != NULL) ? lvif->lvif_bss->ni : NULL,
		    lvif->lvif_bss_synched);
#endif
	lsta = lvif->lvif_bss;
	LKPI_80211_LVIF_UNLOCK(lvif);
	KASSERT(lsta != NULL && lsta->ni != NULL, ("%s: lsta %p ni %p "
	    "lvif %p vap %p\n", __func__,
	    lsta, (lsta != NULL) ? lsta->ni : NULL, lvif, vap));

	ni = lsta->ni;		/* Reference held for lvif_bss. */
	sta = LSTA_TO_STA(lsta);

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);

	/* flush, drop. */
	lkpi_80211_mo_flush(hw, vif,  nitems(sta->txq), true);

	IMPROVE("What are the proper conditions for DEAUTH_NEED_MGD_TX_PREP?");
	if (ieee80211_hw_check(hw, DEAUTH_NEED_MGD_TX_PREP) &&
	    !lsta->in_mgd) {
		memset(&prep_tx_info, 0, sizeof(prep_tx_info));
		prep_tx_info.duration = PREP_TX_INFO_DURATION;
		prep_tx_info.was_assoc = true;
		lkpi_80211_mo_mgd_prepare_tx(hw, vif, &prep_tx_info);
		lsta->in_mgd = true;
	}

	wiphy_unlock(hw->wiphy);
	IEEE80211_LOCK(vap->iv_ic);

	/* Call iv_newstate first so we get potential DISASSOC packet out. */
	error = lvif->iv_newstate(vap, nstate, arg);
	if (error != 0) {
		ic_printf(vap->iv_ic, "%s:%d: iv_newstate(%p, %d, %d) "
		    "failed: %d\n", __func__, __LINE__, vap, nstate, arg, error);
		goto outni;
	}

	IEEE80211_UNLOCK(vap->iv_ic);

	/* Ensure the packets get out. */
	lkpi_80211_flush_tx(lhw, lsta);

	wiphy_lock(hw->wiphy);

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);

	/* Wake tx queues to get packet(s) out. */
	lkpi_wake_tx_queues(hw, sta, false, true);

	/* flush, no drop */
	lkpi_80211_mo_flush(hw, vif,  nitems(sta->txq), false);

	/* End mgd_complete_tx. */
	if (lsta->in_mgd) {
		memset(&prep_tx_info, 0, sizeof(prep_tx_info));
		prep_tx_info.success = false;
		prep_tx_info.was_assoc = true;
		lkpi_80211_mo_mgd_complete_tx(hw, vif, &prep_tx_info);
		lsta->in_mgd = false;
	}

	/* sync_rx_queues */
	lkpi_80211_mo_sync_rx_queues(hw);

	/* sta_pre_rcu_remove */
        lkpi_80211_mo_sta_pre_rcu_remove(hw, vif, sta);

	/* Take the station down. */

	/* Adjust sta and change state (from AUTHORIZED) to ASSOC. */
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_AUTHORIZED, ("%s: lsta %p state not "
	    "AUTHORIZED: %#x\n", __func__, lsta, lsta->state));
	error = lkpi_80211_mo_sta_state(hw, vif, lsta, IEEE80211_STA_ASSOC);
	if (error != 0) {
		ic_printf(vap->iv_ic, "%s:%d: mo_sta_state(ASSOC) "
		    "failed: %d\n", __func__, __LINE__, error);
		goto out;
	}

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);

#ifdef LKPI_80211_HW_CRYPTO
	if (lkpi_hwcrypto) {
		/*
		 * In theory we only need to do this if we changed assoc.
		 * If we were not assoc, there should be no keys and we
		 * should not be here.
		 */
#ifdef notyet
		KASSERT((bss_changed & BSS_CHANGED_ASSOC) != 0, ("%s: "
		    "trying to remove keys but were not assoc: %#010jx, lvif %p\n",
		    __func__, (uintmax_t)bss_changed, lvif));
#endif
		error = lkpi_sta_del_keys(hw, vif, lsta);
		if (error != 0) {
			ic_printf(vap->iv_ic, "%s:%d: lkpi_sta_del_keys "
			    "failed: %d\n", __func__, __LINE__, error);
			/*
			 * Either drv/fw will crash or cleanup itself,
			 * otherwise net80211 will delete the keys (at a
			 * less appropriate time).
			 */
			/* goto out; */
		}
	}
#endif

	/* Update sta_state (ASSOC to AUTH). */
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_ASSOC, ("%s: lsta %p state not "
	    "ASSOC: %#x\n", __func__, lsta, lsta->state));
	error = lkpi_80211_mo_sta_state(hw, vif, lsta, IEEE80211_STA_AUTH);
	if (error != 0) {
		ic_printf(vap->iv_ic, "%s:%d: mo_sta_state(AUTH) "
		    "failed: %d\n", __func__, __LINE__, error);
		goto out;
	}

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);

	/* Update sta and change state (from AUTH) to NONE. */
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_AUTH, ("%s: lsta %p state not "
	    "AUTH: %#x\n", __func__, lsta, lsta->state));
	error = lkpi_80211_mo_sta_state(hw, vif, lsta, IEEE80211_STA_NONE);
	if (error != 0) {
		ic_printf(vap->iv_ic, "%s:%d: mo_sta_state(NONE) "
		    "failed: %d\n", __func__, __LINE__, error);
		goto out;
	}

	bss_changed = 0;
	/*
	 * Start updating bss info (bss_info_changed) (assoc, aid, ..).
	 *
	 * One would expect this to happen when going off AUTHORIZED.
	 * See comment there; removes the sta from fw if not careful
	 * (bss_info_changed() change is executed right away).
	 *
	 * We need to do this now, before sta changes to IEEE80211_STA_NOTEXIST
	 * as otherwise drivers (iwlwifi at least) will silently not remove
	 * the sta from the firmware and when we will add a new one trigger
	 * a fw assert.
	 *
	 * The order which works best so far avoiding early removal or silent
	 * non-removal seems to be (for iwlwifi::mld-mac80211.c cases;
	 * the iwlwifi:mac80211.c case still to be tested):
	 * 1) lkpi_disassoc(): set vif->cfg.assoc = false (aid=0 side effect here)
	 * 2) call the last sta_state update -> IEEE80211_STA_NOTEXIST
	 *    (removes the sta given assoc is false)
	 * 3) add the remaining BSS_CHANGED changes and call bss_info_changed()
	 * 4) call unassign_vif_chanctx
	 * 5) call lkpi_hw_conf_idle
	 * 6) call remove_chanctx
	 *
	 * Note: vif->driver_flags & IEEE80211_VIF_REMOVE_AP_AFTER_DISASSOC
	 * might change this.
	 */
	bss_changed |= lkpi_disassoc(sta, vif, lhw);

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);

	/* Adjust sta and change state (from NONE) to NOTEXIST. */
	KASSERT(lsta != NULL, ("%s: ni %p lsta is NULL\n", __func__, ni));
	KASSERT(lsta->state == IEEE80211_STA_NONE, ("%s: lsta %p state not "
	    "NONE: %#x, nstate %d arg %d\n", __func__, lsta, lsta->state, nstate, arg));
	error = lkpi_80211_mo_sta_state(hw, vif, lsta, IEEE80211_STA_NOTEXIST);
	if (error != 0) {
		IMPROVE("do we need to undo the chan ctx?");
		ic_printf(vap->iv_ic, "%s:%d: mo_sta_state(NOTEXIST) "
		    "failed: %d\n", __func__, __LINE__, error);
		goto out;
	}

	lkpi_lsta_remove(lsta, lvif);

	lkpi_lsta_dump(lsta, ni, __func__, __LINE__);	/* sta no longer save to use. */

	IMPROVE("Any bss_info changes to announce?");
	vif->bss_conf.qos = 0;
	bss_changed |= BSS_CHANGED_QOS;
	vif->cfg.ssid_len = 0;
	memset(vif->cfg.ssid, '\0', sizeof(vif->cfg.ssid));
	bss_changed |= BSS_CHANGED_BSSID;
	vif->bss_conf.use_short_preamble = false;
	vif->bss_conf.qos = false;
	/* XXX BSS_CHANGED_???? */
	vif->bss_conf.dtim_period = 0; /* go back to 0. */
	bss_changed |= BSS_CHANGED_BEACON_INFO;
	lkpi_80211_mo_bss_info_changed(hw, vif, &vif->bss_conf, bss_changed);

	LKPI_80211_LVIF_LOCK(lvif);
	/* Remove ni reference for this cache of lsta. */
	lvif->lvif_bss = NULL;
	lvif->lvif_bss_synched = false;
	LKPI_80211_LVIF_UNLOCK(lvif);

	/* conf_tx */

	lkpi_remove_chanctx(hw, vif);

	error = EALREADY;
out:
	wiphy_unlock(hw->wiphy);
	IEEE80211_LOCK(vap->iv_ic);
	if (error == EALREADY) {
		/*
		 * We do this outside the wiphy lock as net80211::node_free() may call
		 * into crypto code to delete keys and we have a recursed on
		 * non-recursive sx panic.  Also only do this if we get here w/o error.
		 *
		 * The very last release the reference on the ni for the ni/lsta on
		 * lvif->lvif_bss.  Upon return from this both ni and lsta are invalid
		 * and potentially freed.
		 */
		ieee80211_free_node(ni);
	}
outni:
	return (error);
}

static int
lkpi_sta_run_to_scan(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{

	return (lkpi_sta_run_to_init(vap, nstate, arg));
}

static int
lkpi_sta_run_to_auth(struct ieee80211vap *vap, enum ieee80211_state nstate, int arg)
{
	int error;

	error = lkpi_sta_run_to_init(vap, nstate, arg);
	if (error != 0 && error != EALREADY)
		return (error);

	/* At this point iv_bss is long a new node! */

	error |= lkpi_sta_scan_to_auth(vap, nstate, 0);
	return (error);
}

/* -------------------------------------------------------------------------- */

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
	{ IEEE80211_S_ASSOC,	IEEE80211_S_INIT, lkpi_sta_assoc_to_init },	/* Send DEAUTH. */
	{ IEEE80211_S_RUN,	IEEE80211_S_INIT, lkpi_sta_run_to_init },	/* Send DISASSOC. */

	{ IEEE80211_S_INIT,	IEEE80211_S_SCAN, lkpi_sta_state_do_nada },
	{ IEEE80211_S_SCAN,	IEEE80211_S_SCAN, lkpi_sta_state_do_nada },
	{ IEEE80211_S_AUTH,	IEEE80211_S_SCAN, lkpi_sta_auth_to_scan },
	{ IEEE80211_S_ASSOC,	IEEE80211_S_SCAN, lkpi_sta_assoc_to_scan },
	{ IEEE80211_S_RUN,	IEEE80211_S_SCAN, lkpi_sta_run_to_scan },	/* Beacon miss. */

	{ IEEE80211_S_INIT,	IEEE80211_S_AUTH, lkpi_sta_scan_to_auth },	/* Send AUTH. */
	{ IEEE80211_S_SCAN,	IEEE80211_S_AUTH, lkpi_sta_scan_to_auth },	/* Send AUTH. */
	{ IEEE80211_S_AUTH,	IEEE80211_S_AUTH, lkpi_sta_a_to_a },		/* Send ?AUTH. */
	{ IEEE80211_S_ASSOC,	IEEE80211_S_AUTH, lkpi_sta_assoc_to_auth },	/* Send ?AUTH. */
	{ IEEE80211_S_RUN,	IEEE80211_S_AUTH, lkpi_sta_run_to_auth },	/* Send ?AUTH. */

	{ IEEE80211_S_AUTH,	IEEE80211_S_ASSOC, lkpi_sta_auth_to_assoc },	/* Send ASSOCREQ. */
	{ IEEE80211_S_ASSOC,	IEEE80211_S_ASSOC, lkpi_sta_a_to_a },		/* Send ASSOCREQ. */
	{ IEEE80211_S_RUN,	IEEE80211_S_ASSOC, lkpi_sta_run_to_assoc },	/* Send ASSOCREQ/REASSOCREQ. */

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

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE)
		ic_printf(vap->iv_ic, "%s:%d: vap %p nstate %#x arg %#x\n",
		    __func__, __LINE__, vap, nstate, arg);
#endif

	if (vap->iv_opmode == IEEE80211_M_STA) {

		lhw = ic->ic_softc;
		lvif = VAP_TO_LVIF(vap);
		vif = LVIF_TO_VIF(lvif);

		/* No need to replicate this in most state handlers. */
		if (nstate > IEEE80211_S_SCAN)
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
#ifdef LINUXKPI_DEBUG_80211
			if (linuxkpi_debug_80211 & D80211_TRACE)
				ic_printf(vap->iv_ic, "%s: new state %d (%s) ->"
				    " %d (%s): arg %d.\n", __func__,
				    ostate, ieee80211_state_name[ostate],
				    nstate, ieee80211_state_name[nstate], arg);
#endif
			error = s->handler(vap, nstate, arg);
			break;
		}
	}
	IEEE80211_LOCK_ASSERT(vap->iv_ic);

	if (s->handler == NULL) {
		IMPROVE("turn this into a KASSERT\n");
		ic_printf(vap->iv_ic, "%s: unsupported state transition "
		    "%d (%s) -> %d (%s)\n", __func__,
		    ostate, ieee80211_state_name[ostate],
		    nstate, ieee80211_state_name[nstate]);
		return (ENOSYS);
	}

	if (error == EALREADY) {
#ifdef LINUXKPI_DEBUG_80211
		if (linuxkpi_debug_80211 & D80211_TRACE)
			ic_printf(vap->iv_ic, "%s: state transition %d (%s) -> "
			    "%d (%s): iv_newstate already handled: %d.\n",
			    __func__, ostate, ieee80211_state_name[ostate],
			    nstate, ieee80211_state_name[nstate], error);
#endif
		return (0);
	}

	if (error != 0) {
		ic_printf(vap->iv_ic, "%s: error %d during state transition "
		    "%d (%s) -> %d (%s)\n", __func__, error,
		    ostate, ieee80211_state_name[ostate],
		    nstate, ieee80211_state_name[nstate]);
		return (error);
	}

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE)
		ic_printf(vap->iv_ic, "%s:%d: vap %p nstate %#x arg %#x "
		    "calling net80211 parent\n",
		    __func__, __LINE__, vap, nstate, arg);
#endif

	return (lvif->iv_newstate(vap, nstate, arg));
}

/* -------------------------------------------------------------------------- */

/*
 * We overload (*iv_update_bss) as otherwise we have cases in, e.g.,
 * net80211::ieee80211_sta_join1() where vap->iv_bss gets replaced by a
 * new node without us knowing and thus our ni/lsta are out of sync.
 */
static struct ieee80211_node *
lkpi_iv_update_bss(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	struct lkpi_vif *lvif;
	struct ieee80211_node *rni;

	IEEE80211_LOCK_ASSERT(vap->iv_ic);

	lvif = VAP_TO_LVIF(vap);

	LKPI_80211_LVIF_LOCK(lvif);
	lvif->lvif_bss_synched = false;
	LKPI_80211_LVIF_UNLOCK(lvif);

	rni = lvif->iv_update_bss(vap, ni);
	return (rni);
}

#ifdef LKPI_80211_WME
static int
lkpi_wme_update(struct lkpi_hw *lhw, struct ieee80211vap *vap, bool planned)
{
	struct ieee80211com *ic;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct chanAccParams chp;
	struct wmeParams wmeparr[WME_NUM_AC];
	struct ieee80211_tx_queue_params txqp;
	enum ieee80211_bss_changed changed;
	int error;
	uint16_t ac;

	hw = LHW_TO_HW(lhw);
	lockdep_assert_wiphy(hw->wiphy);

	IMPROVE();
	KASSERT(WME_NUM_AC == IEEE80211_NUM_ACS, ("%s: WME_NUM_AC %d != "
	    "IEEE80211_NUM_ACS %d\n", __func__, WME_NUM_AC, IEEE80211_NUM_ACS));

	if (vap == NULL)
		return (0);

	if ((vap->iv_flags & IEEE80211_F_WME) == 0)
		return (0);

	if (lhw->ops->conf_tx == NULL)
		return (0);

	if (!planned && (vap->iv_state != IEEE80211_S_RUN)) {
		lhw->update_wme = true;
		return (0);
	}
	lhw->update_wme = false;

	ic = lhw->ic;
	ieee80211_wme_ic_getparams(ic, &chp);
	IEEE80211_LOCK(ic);
	for (ac = 0; ac < WME_NUM_AC; ac++)
		wmeparr[ac] = chp.cap_wmeParams[ac];
	IEEE80211_UNLOCK(ic);

	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);

	/* Configure tx queues (conf_tx) & send BSS_CHANGED_QOS. */
	for (ac = 0; ac < IEEE80211_NUM_ACS; ac++) {
		struct wmeParams *wmep;

		wmep = &wmeparr[ac];
		bzero(&txqp, sizeof(txqp));
		txqp.cw_min = wmep->wmep_logcwmin;
		txqp.cw_max = wmep->wmep_logcwmax;
		txqp.txop = wmep->wmep_txopLimit;
		txqp.aifs = wmep->wmep_aifsn;
		error = lkpi_80211_mo_conf_tx(hw, vif, /* link_id */0, ac, &txqp);
		if (error != 0)
			ic_printf(ic, "%s: conf_tx ac %u failed %d\n",
			    __func__, ac, error);
	}
	changed = BSS_CHANGED_QOS;
	if (!planned)
		lkpi_80211_mo_bss_info_changed(hw, vif, &vif->bss_conf, changed);

	return (changed);
}
#endif

static int
lkpi_ic_wme_update(struct ieee80211com *ic)
{
#ifdef LKPI_80211_WME
	struct ieee80211vap *vap;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;

	IMPROVE("Use the per-VAP callback in net80211.");
	vap = TAILQ_FIRST(&ic->ic_vaps);
	if (vap == NULL)
		return (0);

	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);

	wiphy_lock(hw->wiphy);
	lkpi_wme_update(lhw, vap, false);
	wiphy_unlock(hw->wiphy);
#endif
	return (0);	/* unused */
}

static void
lkpi_iv_sta_recv_mgmt(struct ieee80211_node *ni, struct mbuf *m0,
    int subtype, const struct ieee80211_rx_stats *rxs, int rssi, int nf)
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	enum ieee80211_bss_changed bss_changed;

	lvif = VAP_TO_LVIF(ni->ni_vap);
	vif = LVIF_TO_VIF(lvif);

	lvif->iv_recv_mgmt(ni, m0, subtype, rxs, rssi, nf);

	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		break;
	case IEEE80211_FC0_SUBTYPE_BEACON:
		/*
		 * Only count beacons when assoc. SCAN has its own logging.
		 * This is for connection/beacon loss/session protection almost
		 * over debugging when trying to get into a stable RUN state.
		 */
		if (vif->cfg.assoc)
			lvif->beacons++;
		break;
	default:
		return;
	}

	lhw = ni->ni_ic->ic_softc;
	hw = LHW_TO_HW(lhw);

	/*
	 * If this direct call to mo_bss_info_changed will not work due to
	 * locking, see if queue_work() is fast enough.
	 */
	bss_changed = lkpi_update_dtim_tsf(vif, ni, ni->ni_vap, __func__, __LINE__);
	lkpi_80211_mo_bss_info_changed(hw, vif, &vif->bss_conf, bss_changed);
}

/*
 * Change link-layer address on the vif (if the vap is not started/"UP").
 * This can happen if a user changes 'ether' using ifconfig.
 * The code is based on net80211/ieee80211_freebsd.c::wlan_iflladdr() but
 * we do use a per-[l]vif event handler to be sure we exist as we
 * cannot assume that from every vap derives a vif and we have a hard
 * time checking based on net80211 information.
 * Should this ever become a real problem we could add a callback function
 * to wlan_iflladdr() to be set optionally but that would be for a
 * single-consumer (or needs a list) -- was just too complicated for an
 * otherwise perfect mechanism FreeBSD already provides.
 */
static void
lkpi_vif_iflladdr(void *arg, struct ifnet *ifp)
{
	struct epoch_tracker et;
	struct ieee80211_vif *vif;

	NET_EPOCH_ENTER(et);
	/* NB: identify vap's by if_transmit; left as an extra check. */
	if (if_gettransmitfn(ifp) != ieee80211_vap_transmit ||
	    (if_getflags(ifp) & IFF_UP) != 0) {
		NET_EPOCH_EXIT(et);
		return;
	}

	vif = arg;
	IEEE80211_ADDR_COPY(vif->bss_conf.addr, if_getlladdr(ifp));
	NET_EPOCH_EXIT(et);
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
	struct ieee80211_tx_queue_params txqp;
	enum ieee80211_bss_changed changed;
	struct sysctl_oid *node;
	size_t len;
	int error, i;
	uint16_t ac;

	if (!TAILQ_EMPTY(&ic->ic_vaps))	/* 1 so far. Add <n> once this works. */
		return (NULL);

	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);

	len = sizeof(*lvif);
	len += hw->vif_data_size;	/* vif->drv_priv */

	lvif = malloc(len, M_80211_VAP, M_WAITOK | M_ZERO);
	mtx_init(&lvif->mtx, "lvif", NULL, MTX_DEF);
	TASK_INIT(&lvif->sw_scan_task, 0, lkpi_sw_scan_task, lvif);
	INIT_LIST_HEAD(&lvif->lsta_list);
	lvif->lvif_bss = NULL;
	refcount_init(&lvif->nt_unlocked, 0);
	lvif->lvif_bss_synched = false;
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
	RCU_INIT_POINTER(vif->bss_conf.chanctx_conf, NULL);
	vif->bss_conf.vif = vif;
	/* vap->iv_myaddr is not set until net80211::vap_setup or vap_attach. */
	IEEE80211_ADDR_COPY(vif->bss_conf.addr, mac);
	lvif->lvif_ifllevent = EVENTHANDLER_REGISTER(iflladdr_event,
	    lkpi_vif_iflladdr, vif, EVENTHANDLER_PRI_ANY);
	vif->bss_conf.link_id = 0;	/* Non-MLO operation. */
	vif->bss_conf.chanreq.oper.width = NL80211_CHAN_WIDTH_20_NOHT;
	vif->bss_conf.use_short_preamble = false;	/* vap->iv_flags IEEE80211_F_SHPREAMBLE */
	vif->bss_conf.use_short_slot = false;		/* vap->iv_flags IEEE80211_F_SHSLOT */
	vif->bss_conf.qos = false;
	vif->bss_conf.use_cts_prot = false;		/* vap->iv_protmode */
	vif->bss_conf.ht_operation_mode = IEEE80211_HT_OP_MODE_PROTECTION_NONE;
	vif->cfg.aid = 0;
	vif->cfg.assoc = false;
	vif->cfg.idle = true;
	vif->cfg.ps = false;
	IMPROVE("Check other fields and then figure out whats is left elsewhere of them");
	/*
	 * We need to initialize it to something as the bss_info_changed call
	 * will try to copy from it in iwlwifi and NULL is a panic.
	 * We will set the proper one in scan_to_auth() before being assoc.
	 */
	vif->bss_conf.bssid = ieee80211broadcastaddr;
#endif
#if 0
	vif->bss_conf.dtim_period = 0; /* IEEE80211_DTIM_DEFAULT ; must stay 0. */
	IEEE80211_ADDR_COPY(vif->bss_conf.bssid, bssid);
	vif->bss_conf.beacon_int = ic->ic_bintval;
	/* iwlwifi bug. */
	if (vif->bss_conf.beacon_int < 16)
		vif->bss_conf.beacon_int = 16;
#endif

	/* Link Config */
	vif->link_conf[0] = &vif->bss_conf;
	for (i = 0; i < nitems(vif->link_conf); i++) {
		IMPROVE("more than 1 link one day");
	}

	/* Setup queue defaults; driver may override in (*add_interface). */
	for (i = 0; i < IEEE80211_NUM_ACS; i++) {
		if (ieee80211_hw_check(hw, QUEUE_CONTROL))
			vif->hw_queue[i] = IEEE80211_INVAL_HW_QUEUE;
		else if (hw->queues >= IEEE80211_NUM_ACS)
			vif->hw_queue[i] = i;
		else
			vif->hw_queue[i] = 0;

		/* Initialize the queue to running. Stopped? */
		lvif->hw_queue_stopped[i] = false;
	}
	vif->cab_queue = IEEE80211_INVAL_HW_QUEUE;

	IMPROVE();

	wiphy_lock(hw->wiphy);
	error = lkpi_80211_mo_start(hw);
	if (error != 0) {
		wiphy_unlock(hw->wiphy);
		ic_printf(ic, "%s: failed to start hw: %d\n", __func__, error);
		mtx_destroy(&lvif->mtx);
		free(lvif, M_80211_VAP);
		return (NULL);
	}

	error = lkpi_80211_mo_add_interface(hw, vif);
	if (error != 0) {
		IMPROVE();	/* XXX-BZ mo_stop()? */
		wiphy_unlock(hw->wiphy);
		ic_printf(ic, "%s: failed to add interface: %d\n", __func__, error);
		mtx_destroy(&lvif->mtx);
		free(lvif, M_80211_VAP);
		return (NULL);
	}
	wiphy_unlock(hw->wiphy);

	LKPI_80211_LHW_LVIF_LOCK(lhw);
	TAILQ_INSERT_TAIL(&lhw->lvif_head, lvif, lvif_entry);
	LKPI_80211_LHW_LVIF_UNLOCK(lhw);

	/* Set bss_info. */
	changed = 0;
	lkpi_80211_mo_bss_info_changed(hw, vif, &vif->bss_conf, changed);

	/* Configure tx queues (conf_tx), default WME & send BSS_CHANGED_QOS. */
	IMPROVE("Hardcoded values; to fix see 802.11-2016, 9.4.2.29 EDCA Parameter Set element");
	wiphy_lock(hw->wiphy);
	for (ac = 0; ac < IEEE80211_NUM_ACS; ac++) {

		bzero(&txqp, sizeof(txqp));
		txqp.cw_min = 15;
		txqp.cw_max = 1023;
		txqp.txop = 0;
		txqp.aifs = 2;
		error = lkpi_80211_mo_conf_tx(hw, vif, /* link_id */0, ac, &txqp);
		if (error != 0)
			ic_printf(ic, "%s: conf_tx ac %u failed %d\n",
			    __func__, ac, error);
	}
	wiphy_unlock(hw->wiphy);
	changed = BSS_CHANGED_QOS;
	lkpi_80211_mo_bss_info_changed(hw, vif, &vif->bss_conf, changed);

	/* Force MC init. */
	lkpi_update_mcast_filter(ic);

	ieee80211_vap_setup(ic, vap, name, unit, opmode, flags, bssid);

	/* Now we have a valid vap->iv_ifp.  Any checksum offloading goes below. */

	IMPROVE();

	/* Override with LinuxKPI method so we can drive mac80211/cfg80211. */
	lvif->iv_newstate = vap->iv_newstate;
	vap->iv_newstate = lkpi_iv_newstate;
	lvif->iv_update_bss = vap->iv_update_bss;
	vap->iv_update_bss = lkpi_iv_update_bss;
	lvif->iv_recv_mgmt = vap->iv_recv_mgmt;
	vap->iv_recv_mgmt = lkpi_iv_sta_recv_mgmt;

#ifdef LKPI_80211_HW_CRYPTO
	/* Key management. */
	if (lkpi_hwcrypto && lhw->ops->set_key != NULL) {
		vap->iv_key_set = lkpi_iv_key_set;
		vap->iv_key_delete = lkpi_iv_key_delete;
		vap->iv_key_update_begin = lkpi_iv_key_update_begin;
		vap->iv_key_update_end = lkpi_iv_key_update_end;
	}
#endif

#ifdef LKPI_80211_HT
	/* Stay with the iv_ampdu_rxmax,limit / iv_ampdu_density defaults until later. */
#endif

	ieee80211_ratectl_init(vap);

	/* Complete setup. */
	ieee80211_vap_attach(vap, ieee80211_media_change,
	    ieee80211_media_status, mac);

#ifdef LKPI_80211_HT
	/*
	 * Modern chipset/fw/drv will do A-MPDU in drv/fw and fail
	 * to do so if they cannot do the crypto too.
	 */
	if (!lkpi_hwcrypto && IEEE80211_CONF_AMPDU_OFFLOAD(ic))
		vap->iv_flags_ht &= ~IEEE80211_FHT_AMPDU_RX;
#endif

	if (hw->max_listen_interval == 0)
		hw->max_listen_interval = 7 * (ic->ic_lintval / ic->ic_bintval);
	hw->conf.listen_interval = hw->max_listen_interval;
	ic->ic_set_channel(ic);

	/* XXX-BZ do we need to be able to update these? */
	hw->wiphy->frag_threshold = vap->iv_fragthreshold;
	lkpi_80211_mo_set_frag_threshold(hw, vap->iv_fragthreshold);
	hw->wiphy->rts_threshold = vap->iv_rtsthreshold;
	lkpi_80211_mo_set_rts_threshold(hw, vap->iv_rtsthreshold);
	/* any others? */

	/* Add per-VIF/VAP sysctls. */
	sysctl_ctx_init(&lvif->sysctl_ctx);

	node = SYSCTL_ADD_NODE(&lvif->sysctl_ctx,
	    SYSCTL_CHILDREN(&sysctl___compat_linuxkpi_80211),
	    OID_AUTO, if_name(vap->iv_ifp),
	    CTLFLAG_RD | CTLFLAG_SKIP | CTLFLAG_MPSAFE, NULL, "VIF Information");

	SYSCTL_ADD_PROC(&lvif->sysctl_ctx,
	    SYSCTL_CHILDREN(node), OID_AUTO, "dump_stas",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_MPSAFE, lvif, 0,
	    lkpi_80211_dump_stas, "A", "Dump sta statistics of this vif");

	IMPROVE();

	return (vap);
}

void
linuxkpi_ieee80211_unregister_hw(struct ieee80211_hw *hw)
{

	wiphy_unregister(hw->wiphy);
	linuxkpi_ieee80211_ifdetach(hw);

	IMPROVE();
}

void
linuxkpi_ieee80211_restart_hw(struct ieee80211_hw *hw)
{

	TODO();
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

	EVENTHANDLER_DEREGISTER(iflladdr_event, lvif->lvif_ifllevent);

	/* Clear up per-VIF/VAP sysctls. */
	sysctl_ctx_free(&lvif->sysctl_ctx);

	ieee80211_draintask(ic, &lvif->sw_scan_task);

	LKPI_80211_LHW_LVIF_LOCK(lhw);
	TAILQ_REMOVE(&lhw->lvif_head, lvif, lvif_entry);
	LKPI_80211_LHW_LVIF_UNLOCK(lhw);

	ieee80211_ratectl_deinit(vap);
	ieee80211_vap_detach(vap);

	IMPROVE("clear up other bits in this state");

	lkpi_80211_mo_remove_interface(hw, vif);

	/* Single VAP, so we can do this here. */
	lkpi_80211_mo_stop(hw, false);			/* XXX SUSPEND */

	mtx_destroy(&lvif->mtx);
	free(lvif, M_80211_VAP);
}

static void
lkpi_ic_update_mcast(struct ieee80211com *ic)
{
	struct ieee80211vap *vap;
	struct lkpi_hw *lhw;

	lhw = ic->ic_softc;
	if (lhw->ops->prepare_multicast == NULL ||
	    lhw->ops->configure_filter == NULL)
		return;

	LKPI_80211_LHW_MC_LOCK(lhw);
	/* Cleanup anything on the current list. */
	lkpi_cleanup_mcast_list_locked(lhw);

	/* Build up the new list (or allmulti). */
	if (ic->ic_allmulti == 0) {
		TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next)
			if_foreach_llmaddr(vap->iv_ifp,
			    lkpi_ic_update_mcast_copy, &lhw->mc_list);
		lhw->mc_all_multi = false;
	} else {
		lhw->mc_all_multi = true;
	}
	LKPI_80211_LHW_MC_UNLOCK(lhw);

	lkpi_update_mcast_filter(ic);
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
#ifdef HW_START_STOP
	int error;
#endif
	bool start_all;

	IMPROVE();

	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	start_all = false;

	/* IEEE80211_UNLOCK(ic); */
	wiphy_lock(hw->wiphy);
	if (ic->ic_nrunning > 0) {
#ifdef HW_START_STOP
		error = lkpi_80211_mo_start(hw);
		if (error == 0)
#endif
			start_all = true;
	} else {
#ifdef HW_START_STOP
		lkpi_80211_mo_stop(hw, false);		/* XXX SUSPEND */
#endif
	}
	wiphy_unlock(hw->wiphy);
	/* IEEE80211_LOCK(ic); */

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

static uint8_t *
lkpi_scan_ies_add(uint8_t *p, struct ieee80211_scan_ies *scan_ies,
    uint32_t band_mask, struct ieee80211vap *vap, struct ieee80211_hw *hw)
{
	struct ieee80211_supported_band *supband;
	struct linuxkpi_ieee80211_channel *channels;
	struct ieee80211com *ic;
	const struct ieee80211_channel *chan;
	const struct ieee80211_rateset *rs;
	uint8_t *pb;
	int band, i;

	ic = vap->iv_ic;
	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		if ((band_mask & (1 << band)) == 0)
			continue;

		supband = hw->wiphy->bands[band];
		/*
		 * This should not happen;
		 * band_mask is a bitmask of valid bands to scan on.
		 */
		if (supband == NULL || supband->n_channels == 0)
			continue;

		/* Find a first channel to get the mode and rates from. */
		channels = supband->channels;
		chan = NULL;
		for (i = 0; i < supband->n_channels; i++) {
			uint32_t flags;

			if (channels[i].flags & IEEE80211_CHAN_DISABLED)
				continue;

			flags = 0;
			switch (band) {
			case NL80211_BAND_2GHZ:
				flags |= IEEE80211_CHAN_G;
				break;
			case NL80211_BAND_5GHZ:
				flags |= IEEE80211_CHAN_A;
				break;
			default:
				panic("%s:%d: unupported band %d\n",
				    __func__, __LINE__, band);
			}

			chan = ieee80211_find_channel(ic,
			    channels[i].center_freq, flags);
			if (chan != NULL)
				break;
		}

		/* This really should not happen. */
		if (chan == NULL)
			continue;

		pb = p;
		rs = ieee80211_get_suprates(ic, chan);	/* calls chan2mode */
		p = ieee80211_add_rates(p, rs);
		p = ieee80211_add_xrates(p, rs);

#if defined(LKPI_80211_HT)
		if ((vap->iv_flags_ht & IEEE80211_FHT_HT) != 0) {
			struct ieee80211_channel *c;

			c = ieee80211_ht_adjust_channel(ic, ic->ic_curchan,
			    vap->iv_flags_ht);
			p = ieee80211_add_htcap_ch(p, vap, c);
		}
#endif
#if defined(LKPI_80211_VHT)
		if (band == NL80211_BAND_5GHZ &&
		    (vap->iv_vht_flags & IEEE80211_FVHT_VHT) != 0) {
			struct ieee80211_channel *c;

			c = ieee80211_ht_adjust_channel(ic, ic->ic_curchan,
			    vap->iv_flags_ht);
			c = ieee80211_vht_adjust_channel(ic, c,
			    vap->iv_vht_flags);
			p = ieee80211_add_vhtcap_ch(p, vap, c);
		}
#endif

		scan_ies->ies[band] = pb;
		scan_ies->len[band] = p - pb;
	}

	/* Add common_ies */
	pb = p;
	if ((vap->iv_flags & IEEE80211_F_WPA1) != 0 &&
	    vap->iv_wpa_ie != NULL) {
		memcpy(p, vap->iv_wpa_ie, 2 + vap->iv_wpa_ie[1]);
		p += 2 + vap->iv_wpa_ie[1];
	}
	if (vap->iv_appie_probereq != NULL) {
		memcpy(p, vap->iv_appie_probereq->ie_data,
		    vap->iv_appie_probereq->ie_len);
		p += vap->iv_appie_probereq->ie_len;
	}
	scan_ies->common_ies = pb;
	scan_ies->common_ie_len = p - pb;

	return (p);
}

static void
lkpi_enable_hw_scan(struct lkpi_hw *lhw)
{

	if (lhw->ops->hw_scan) {
		/*
		 * Advertise full-offload scanning.
		 *
		 * Not limiting to SINGLE_SCAN_ON_ALL_BANDS here as otherwise
		 * we essentially disable hw_scan for all drivers not setting
		 * the flag.
		 */
		lhw->ic->ic_flags_ext |= IEEE80211_FEXT_SCAN_OFFLOAD;
		lhw->scan_flags |= LKPI_LHW_SCAN_HW;
	}
}

#ifndef LKPI_80211_USE_SCANLIST
static const uint32_t chan_pri[] = {
	5180, 5500, 5745,
	5260, 5580, 5660, 5825,
	5220, 5300, 5540, 5620, 5700, 5785, 5865,
	2437, 2412, 2422, 2462, 2472, 2432, 2452
};

static int
lkpi_scan_chan_list_idx(const struct linuxkpi_ieee80211_channel *lc)
{
	int i;

	for (i = 0; i < nitems(chan_pri); i++) {
		if (lc->center_freq == chan_pri[i])
			return (i);
	}

	return (-1);
}

static int
lkpi_scan_chan_list_comp(const  struct linuxkpi_ieee80211_channel *lc1,
    const  struct linuxkpi_ieee80211_channel *lc2)
{
	int idx1, idx2;

	/* Find index in list. */
	idx1 = lkpi_scan_chan_list_idx(lc1);
	idx2 = lkpi_scan_chan_list_idx(lc2);

	if (idx1 == -1 && idx2 != -1)
		return (1);
	if (idx1 != -1 && idx2 == -1)
		return (-1);

	/* Neither on the list, use center_freq. */
	if (idx1 == -1 && idx2 == -1)
		return (lc1->center_freq - lc2->center_freq);

	/* Whichever is first in the list. */
	return (idx1 - idx2);
}

static void
lkpi_scan_chan_list_resort(struct linuxkpi_ieee80211_channel **cpp, size_t nchan)
{
	struct linuxkpi_ieee80211_channel *lc, *nc;
	size_t i, j;
	int rc;

	for (i = (nchan - 1); i > 0; i--) {
		for (j = i; j > 0 ; j--) {
			lc = *(cpp + j);
			nc = *(cpp + j - 1);
			rc = lkpi_scan_chan_list_comp(lc, nc);
			if (rc < 0) {
				*(cpp + j) = nc;
				*(cpp + j - 1) = lc;
			}
		}
	}
}

static bool
lkpi_scan_chan(struct linuxkpi_ieee80211_channel *c,
    struct ieee80211com *ic, bool log)
{

	if ((c->flags & IEEE80211_CHAN_DISABLED) != 0) {
		if (log)
			TRACE_SCAN(ic, "Skipping disabled chan "
			    "on band %s [%#x/%u/%#x]",
			    lkpi_nl80211_band_name(c->band), c->hw_value,
			    c->center_freq, c->flags);
		return (false);
	}
	if (isclr(ic->ic_chan_active, ieee80211_mhz2ieee(c->center_freq,
	    lkpi_nl80211_band_to_net80211_band(c->band)))) {
		if (log)
			TRACE_SCAN(ic, "Skipping !active chan "
			    "on band %s [%#x/%u/%#x]",
			    lkpi_nl80211_band_name(c->band), c->hw_value,
			    c->center_freq, c->flags);
		return (false);
	}
	return (true);
}
#endif

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
	bool is_hw_scan;

	lhw = ic->ic_softc;
	ss = ic->ic_scan;
	vap = ss->ss_vap;
	TRACE_SCAN(ic, "scan_flags %b", lhw->scan_flags, LKPI_LHW_SCAN_BITS);

	LKPI_80211_LHW_SCAN_LOCK(lhw);
	if ((lhw->scan_flags & LKPI_LHW_SCAN_RUNNING) != 0) {
		/* A scan is still running. */
		LKPI_80211_LHW_SCAN_UNLOCK(lhw);
		TRACE_SCAN(ic, "Trying to start new scan while still running; "
		    "cancelling new net80211 scan; scan_flags %b",
		    lhw->scan_flags, LKPI_LHW_SCAN_BITS);
		ieee80211_cancel_scan(vap);
		return;
	}
	is_hw_scan = (lhw->scan_flags & LKPI_LHW_SCAN_HW) != 0;
	LKPI_80211_LHW_SCAN_UNLOCK(lhw);

#if 0
	if (vap->iv_state != IEEE80211_S_SCAN) {
		TODO("We need to be able to scan if not in S_SCAN");
		TRACE_SCAN(ic, "scan_flags %b iv_state %d",
		    lhw->scan_flags, LKPI_LHW_SCAN_BITS, vap->iv_state);
		ieee80211_cancel_scan(vap);
		return;
	}
#endif

	hw = LHW_TO_HW(lhw);
	if (!is_hw_scan) {
		/* If hw_scan is cleared clear FEXT_SCAN_OFFLOAD too. */
		vap->iv_flags_ext &= ~IEEE80211_FEXT_SCAN_OFFLOAD;

		lvif = VAP_TO_LVIF(vap);
		vif = LVIF_TO_VIF(lvif);

		if (vap->iv_state == IEEE80211_S_SCAN)
			lkpi_hw_conf_idle(hw, false);

		LKPI_80211_LHW_SCAN_LOCK(lhw);
		lhw->scan_flags |= LKPI_LHW_SCAN_RUNNING;
		LKPI_80211_LHW_SCAN_UNLOCK(lhw);

		lkpi_update_mcast_filter(ic);

		TRACE_SCAN(vap->iv_ic, "Starting SW_SCAN: scan_flags %b",
		    lhw->scan_flags, LKPI_LHW_SCAN_BITS);
		lkpi_80211_mo_sw_scan_start(hw, vif, vif->addr);
		/* net80211::scan_start() handled PS for us. */
		IMPROVE();
		/* XXX Also means it is too late to flush queues?
		 * need to check iv_sta_ps or overload? */
		/* XXX want to adjust ss end time/ maxdwell? */

	} else {
		struct ieee80211_scan_request *hw_req;
		struct linuxkpi_ieee80211_channel *lc, **cpp;
		struct cfg80211_ssid *ssids;
		struct cfg80211_scan_6ghz_params *s6gp;
		size_t chan_len, nchan, ssids_len, s6ghzlen;
		int band, i, ssid_count, common_ie_len;
#ifndef LKPI_80211_USE_SCANLIST
		int n;
#endif
		uint32_t band_mask;
		uint8_t *ie, *ieend;
		bool running;

		ssid_count = min(ss->ss_nssid, hw->wiphy->max_scan_ssids);
		ssids_len = ssid_count * sizeof(*ssids);
		s6ghzlen = 0 * (sizeof(*s6gp));			/* XXX-BZ */

		band_mask = 0;
		nchan = 0;
		if (ieee80211_hw_check(hw, SINGLE_SCAN_ON_ALL_BANDS)) {
#ifdef LKPI_80211_USE_SCANLIST
		/* Avoid net80211 scan lists until it has proper scan offload support. */
			for (i = ss->ss_next; i < ss->ss_last; i++) {
				nchan++;
				band = lkpi_net80211_chan_to_nl80211_band(
				    ss->ss_chans[ss->ss_next + i]);
				band_mask |= (1 << band);
			}
#else
			/* Instead we scan for all channels all the time. */
			for (band = 0; band < NUM_NL80211_BANDS; band++) {
				switch (band) {
				case NL80211_BAND_2GHZ:
				case NL80211_BAND_5GHZ:
					break;
				default:
					continue;
				}
				if (hw->wiphy->bands[band] != NULL) {
					struct linuxkpi_ieee80211_channel *channels;
					int n;

					band_mask |= (1 << band);

					channels = hw->wiphy->bands[band]->channels;
					n = hw->wiphy->bands[band]->n_channels;
					for (i = 0; i < n; i++) {
						if (lkpi_scan_chan(&channels[i], ic, true))
							nchan++;
					}
				}
			}
#endif
		} else {
			IMPROVE("individual band scans not yet supported, only scanning first band");
			/* In theory net80211 should drive this. */
			/* Probably we need to add local logic for now;
			 * need to deal with scan_complete
			 * and cancel_scan and keep local state.
			 * Also cut the nchan down above.
			 */
			/* XXX-BZ ath10k does not set this but still does it? &$%^ */
		}

		chan_len = nchan * (sizeof(lc) + sizeof(*lc));

		common_ie_len = 0;
		if ((vap->iv_flags & IEEE80211_F_WPA1) != 0 &&
		    vap->iv_wpa_ie != NULL)
			common_ie_len += vap->iv_wpa_ie[1];
		if (vap->iv_appie_probereq != NULL)
			common_ie_len += vap->iv_appie_probereq->ie_len;

		/* We would love to check this at an earlier stage... */
		if (common_ie_len >  hw->wiphy->max_scan_ie_len) {
			ic_printf(ic, "WARNING: %s: common_ie_len %d > "
			    "wiphy->max_scan_ie_len %d\n", __func__,
			    common_ie_len, hw->wiphy->max_scan_ie_len);
		}

		hw_req = malloc(sizeof(*hw_req) + ssids_len +
		    s6ghzlen + chan_len + lhw->supbands * lhw->scan_ie_len +
		    common_ie_len, M_LKPI80211, M_WAITOK | M_ZERO);

		hw_req->req.flags = 0;			/* XXX ??? */
		/* hw_req->req.wdev */
		hw_req->req.wiphy = hw->wiphy;
		hw_req->req.no_cck = false;		/* XXX */

		/*
		 * In general setting duration[_mandatory] seems to pessimise
		 * default scanning behaviour.  We only use it for BGSCANnig
		 * to keep the dwell times small.
		 * Setting duration_mandatory makes this the maximum dwell
		 * time (otherwise may be shorter).  Duration is in TU.
		 */
		if ((ic->ic_flags_ext & IEEE80211_FEXT_BGSCAN) != 0) {
			unsigned long dwell;

			if ((ic->ic_caps & IEEE80211_C_BGSCAN) == 0 ||
			    (vap->iv_flags & IEEE80211_F_BGSCAN) == 0)
				ic_printf(ic, "BGSCAN despite off: %b, %b, %b\n",
				    ic->ic_flags_ext, IEEE80211_FEXT_BITS,
				    vap->iv_flags, IEEE80211_F_BITS,
				    ic->ic_caps, IEEE80211_C_BITS);

			dwell = ss->ss_mindwell;
			if (dwell == 0)
				dwell = msecs_to_ticks(20);

			hw_req->req.duration_mandatory = true;
			hw_req->req.duration = TICKS_2_USEC(dwell) / 1024;
		}

#ifdef __notyet__
		hw_req->req.flags |= NL80211_SCAN_FLAG_RANDOM_ADDR;
		memcpy(hw_req->req.mac_addr, xxx, IEEE80211_ADDR_LEN);
		memset(hw_req->req.mac_addr_mask, 0xxx, IEEE80211_ADDR_LEN);
#endif
		eth_broadcast_addr(hw_req->req.bssid);

		hw_req->req.n_channels = nchan;
		cpp = (struct linuxkpi_ieee80211_channel **)(hw_req + 1);
		lc = (struct linuxkpi_ieee80211_channel *)(cpp + nchan);
#ifdef LKPI_80211_USE_SCANLIST
		for (i = 0; i < nchan; i++) {
			*(cpp + i) =
			    (struct linuxkpi_ieee80211_channel *)(lc + i);
		}
		/* Avoid net80211 scan lists until it has proper scan offload support. */
		for (i = 0; i < nchan; i++) {
			struct ieee80211_channel *c;

			c = ss->ss_chans[ss->ss_next + i];
			lc->center_freq = c->ic_freq;	/* XXX */
			/* lc->flags */
			lc->band = lkpi_net80211_chan_to_nl80211_band(c);
			lc->max_power = c->ic_maxpower;
			/* lc-> ... */
			lc++;
		}
#else
		/* Add bands in reverse order for scanning. */
		n = 0;
		for (band = NUM_NL80211_BANDS - 1; band >= 0; band--) {
			struct ieee80211_supported_band *supband;
			struct linuxkpi_ieee80211_channel *channels;

			/* Band disabled for scanning? */
			if ((band_mask & (1 << band)) == 0)
				continue;

			/* Nothing to scan in band? */
			supband = hw->wiphy->bands[band];
			if (supband == NULL || supband->n_channels == 0)
				continue;

			channels = supband->channels;
			for (i = 0; i < supband->n_channels; i++) {
				if (lkpi_scan_chan(&channels[i], ic, false))
					*(cpp + n++) = &channels[i];
			}
		}
		if (lkpi_order_scanlist)
			lkpi_scan_chan_list_resort(cpp, nchan);

		if ((linuxkpi_debug_80211 & D80211_SCAN) != 0) {
			printf("%s:%d: %s SCAN Channel List (nchan=%zu): ",
			    __func__, __LINE__, ic->ic_name, nchan);
			for (i = 0; i < nchan; i++) {
				struct linuxkpi_ieee80211_channel *xc;

				xc = *(cpp + i);
				printf(" %d(%d)",
				    ieee80211_mhz2ieee(xc->center_freq,
				        lkpi_nl80211_band_to_net80211_band(
					xc->band)),
				    xc->center_freq);
			}
			printf("\n");
		}
#endif

		hw_req->req.n_ssids = ssid_count;
		if (hw_req->req.n_ssids > 0) {
			ssids = (struct cfg80211_ssid *)lc;
			hw_req->req.ssids = ssids;
			for (i = 0; i < ssid_count; i++) {
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

		ie = ieend = (uint8_t *)s6gp;
		/* Copy per-band IEs, copy common IEs */
		ieend = lkpi_scan_ies_add(ie, &hw_req->ies, band_mask, vap, hw);
		hw_req->req.ie = ie;
		hw_req->req.ie_len = ieend - ie;
		hw_req->req.scan_start = jiffies;

		lvif = VAP_TO_LVIF(vap);
		vif = LVIF_TO_VIF(lvif);

		LKPI_80211_LHW_SCAN_LOCK(lhw);
		/* Re-check under lock. */
		running = (lhw->scan_flags & LKPI_LHW_SCAN_RUNNING) != 0;
		if (!running) {
			KASSERT(lhw->hw_req == NULL, ("%s: ic %p lhw %p hw_req %p "
			    "!= NULL\n", __func__, ic, lhw, lhw->hw_req));

			lhw->scan_flags |= LKPI_LHW_SCAN_RUNNING;
			lhw->hw_req = hw_req;
		}
		LKPI_80211_LHW_SCAN_UNLOCK(lhw);
		if (running) {
			free(hw_req, M_LKPI80211);
			TRACE_SCAN(ic, "Trying to start new scan while still "
			    "running (2); cancelling new net80211 scan; "
			    "scan_flags %b",
			    lhw->scan_flags, LKPI_LHW_SCAN_BITS);
			ieee80211_cancel_scan(vap);
			return;
		}

		lkpi_update_mcast_filter(ic);
		TRACE_SCAN(ic, "Starting HW_SCAN: scan_flags %b, "
		    "ie_len %d, n_ssids %d, n_chan %d, common_ie_len %d [%d, %d]",
		    lhw->scan_flags, LKPI_LHW_SCAN_BITS, hw_req->req.ie_len,
		    hw_req->req.n_ssids, hw_req->req.n_channels,
		    hw_req->ies.common_ie_len,
		    hw_req->ies.len[NL80211_BAND_2GHZ],
		    hw_req->ies.len[NL80211_BAND_5GHZ]);

		error = lkpi_80211_mo_hw_scan(hw, vif, hw_req);
		if (error != 0) {
			bool scan_done;
			int e;

			TRACE_SCAN(ic, "hw_scan failed; scan_flags %b, error %d",
			    lhw->scan_flags, LKPI_LHW_SCAN_BITS, error);
			ieee80211_cancel_scan(vap);

			/*
			 * ieee80211_scan_completed must be called in either
			 * case of error or none.  So let the free happen there
			 * and only there.
			 * That would be fine in theory but in practice drivers
			 * behave differently:
			 * ath10k does not return hw_scan until after scan_complete
			 *        and can then still return an error.
			 * rtw88 can return 1 or -EBUSY without scan_complete
			 * iwlwifi can return various errors before scan starts
			 * ...
			 * So we cannot rely on that behaviour and have to check
			 * and balance between both code paths.
			 */
			e = 0;
			scan_done = true;
			LKPI_80211_LHW_SCAN_LOCK(lhw);
			if ((lhw->scan_flags & LKPI_LHW_SCAN_RUNNING) != 0) {

				free(lhw->hw_req, M_LKPI80211);
				lhw->hw_req = NULL;
				/*
				 * The ieee80211_cancel_scan() above runs in a
				 * taskq and it may take ages for the previous
				 * scan to clear;  starting a new one right away
				 * we run into the problem that the old one is
				 * still active.
				 */
				e = msleep(lhw, &lhw->scan_mtx, 0, "lhwscanstop", hz);
				scan_done = (lhw->scan_flags & LKPI_LHW_SCAN_RUNNING) != 0;

				/*
				 * Now we can clear running if no one else did.
				 */
				lhw->scan_flags &= ~LKPI_LHW_SCAN_RUNNING;
			}
			LKPI_80211_LHW_SCAN_UNLOCK(lhw);
			lkpi_update_mcast_filter(ic);
			if (!scan_done) {
				ic_printf(ic, "ERROR: %s: timeout/error to wait "
				    "for ieee80211_cancel_scan: %d\n", __func__, e);
				return;
			}

			/*
			 * XXX-SIGH magic number.
			 * rtw88 has a magic "return 1" if offloading scan is
			 * not possible.  Fall back to sw scan in that case.
			 */
			if (error == 1) {
				/*
				 * We need to put this into some defered context
				 * the net80211 scan may not be done yet
				 * (ic_flags & IEEE80211_F_SCAN) and we cannot
				 * wait here; if we do scan_curchan_task always
				 * runs after our timeout to finalize the scan.
				 */
				ieee80211_runtask(ic, &lvif->sw_scan_task);
				return;
			}

			ic_printf(ic, "ERROR: %s: hw_scan returned %d\n",
			    __func__, error);
		}
	}
}

static void
lkpi_sw_scan_task(void *arg, int pending __unused)
{
	struct lkpi_hw *lhw;
	struct lkpi_vif *lvif;
	struct ieee80211vap *vap;
	struct ieee80211_scan_state *ss;

	lvif = arg;
	vap = LVIF_TO_VAP(lvif);
	lhw = vap->iv_ic->ic_softc;
	ss = vap->iv_ic->ic_scan;

	LKPI_80211_LHW_SCAN_LOCK(lhw);
	/*
	 * We will re-enable this at scan_end calling lkpi_enable_hw_scan().
	 * IEEE80211_FEXT_SCAN_OFFLOAD will be cleared by lkpi_ic_scan_start.
	 */
	lhw->scan_flags &= ~LKPI_LHW_SCAN_HW;
	LKPI_80211_LHW_SCAN_UNLOCK(lhw);

	TRACE_SCAN(vap->iv_ic, "Triggering SW_SCAN: pending %d, scan_flags %b",
	    pending, lhw->scan_flags, LKPI_LHW_SCAN_BITS);

	/*
	 * This will call ic_scan_start() and we will get into the right path
	 * unless other scans started in between.
	 */
	ieee80211_start_scan(vap,
	    IEEE80211_SCAN_ONCE,
	    msecs_to_ticks(10000), /* 10000 ms (=~ 50 chan * 200 ms) */
	    ss->ss_mindwell ? ss->ss_mindwell : msecs_to_ticks(20),
	    ss->ss_maxdwell ? ss->ss_maxdwell : msecs_to_ticks(200),
	    vap->iv_des_nssid, vap->iv_des_ssid);
}

static void
lkpi_ic_scan_end(struct ieee80211com *ic)
{
	struct lkpi_hw *lhw;
	bool is_hw_scan;

	lhw = ic->ic_softc;
	TRACE_SCAN(ic, "scan_flags %b", lhw->scan_flags, LKPI_LHW_SCAN_BITS);

	LKPI_80211_LHW_SCAN_LOCK(lhw);
	if ((lhw->scan_flags & LKPI_LHW_SCAN_RUNNING) == 0) {
		LKPI_80211_LHW_SCAN_UNLOCK(lhw);
		return;
	}
	is_hw_scan = (lhw->scan_flags & LKPI_LHW_SCAN_HW) != 0;
	LKPI_80211_LHW_SCAN_UNLOCK(lhw);

	if (!is_hw_scan) {
		struct ieee80211_scan_state *ss;
		struct ieee80211vap *vap;
		struct ieee80211_hw *hw;
		struct lkpi_vif *lvif;
		struct ieee80211_vif *vif;

		ss = ic->ic_scan;
		vap = ss->ss_vap;
		hw = LHW_TO_HW(lhw);
		lvif = VAP_TO_LVIF(vap);
		vif = LVIF_TO_VIF(lvif);

		lkpi_80211_mo_sw_scan_complete(hw, vif);

		/* Send PS to stop buffering if n80211 does not for us? */

		if (vap->iv_state == IEEE80211_S_SCAN)
			lkpi_hw_conf_idle(hw, true);
	}

	/*
	 * In case we disabled the hw_scan in lkpi_ic_scan_start() and
	 * switched to swscan, re-enable hw_scan if available.
	 */
	lkpi_enable_hw_scan(lhw);

	LKPI_80211_LHW_SCAN_LOCK(lhw);
	wakeup(lhw);
	LKPI_80211_LHW_SCAN_UNLOCK(lhw);
}

static void
lkpi_ic_scan_curchan(struct ieee80211_scan_state *ss,
    unsigned long maxdwell)
{
	struct lkpi_hw *lhw;
	bool is_hw_scan;

	lhw = ss->ss_ic->ic_softc;
	TRACE_SCAN(ss->ss_ic, "scan_flags %b chan %d maxdwell %lu",
	    lhw->scan_flags, LKPI_LHW_SCAN_BITS,
	    ss->ss_ic->ic_curchan->ic_ieee, maxdwell);

	LKPI_80211_LHW_SCAN_LOCK(lhw);
	is_hw_scan = (lhw->scan_flags & LKPI_LHW_SCAN_HW) != 0;
	LKPI_80211_LHW_SCAN_UNLOCK(lhw);
	if (!is_hw_scan)
		lhw->ic_scan_curchan(ss, maxdwell);
}

static void
lkpi_ic_scan_mindwell(struct ieee80211_scan_state *ss)
{
	struct lkpi_hw *lhw;
	bool is_hw_scan;

	lhw = ss->ss_ic->ic_softc;
	TRACE_SCAN(ss->ss_ic, "scan_flags %b chan %d mindwell %lu",
	    lhw->scan_flags, LKPI_LHW_SCAN_BITS,
	    ss->ss_ic->ic_curchan->ic_ieee, ss->ss_mindwell);

	LKPI_80211_LHW_SCAN_LOCK(lhw);
	is_hw_scan = (lhw->scan_flags & LKPI_LHW_SCAN_HW) != 0;
	LKPI_80211_LHW_SCAN_UNLOCK(lhw);
	if (!is_hw_scan)
		lhw->ic_scan_mindwell(ss);
}

static void
lkpi_ic_set_channel(struct ieee80211com *ic)
{
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct ieee80211_channel *c;
	struct linuxkpi_ieee80211_channel *chan;
	int error;
	bool hw_scan_running;

	lhw = ic->ic_softc;

	/* If we do not support (*config)() save us the work. */
	if (lhw->ops->config == NULL)
		return;

	/* If we have a hw_scan running do not switch channels. */
	LKPI_80211_LHW_SCAN_LOCK(lhw);
	hw_scan_running =
	    (lhw->scan_flags & (LKPI_LHW_SCAN_RUNNING|LKPI_LHW_SCAN_HW)) ==
		(LKPI_LHW_SCAN_RUNNING|LKPI_LHW_SCAN_HW);
	LKPI_80211_LHW_SCAN_UNLOCK(lhw);
	if (hw_scan_running)
		return;

	c = ic->ic_curchan;
	if (c == NULL || c == IEEE80211_CHAN_ANYC) {
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

	/* XXX max power for scanning? */
	IMPROVE();

	hw = LHW_TO_HW(lhw);
	cfg80211_chandef_create(&hw->conf.chandef, chan,
#ifdef LKPI_80211_HT
	    (ic->ic_flags_ht & IEEE80211_FHT_HT) ? NL80211_CHAN_HT20 :
#endif
	    NL80211_CHAN_NO_HT);

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

static struct ieee80211_node *
lkpi_ic_node_alloc(struct ieee80211vap *vap,
    const uint8_t mac[IEEE80211_ADDR_LEN])
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct ieee80211_node *ni;
	struct ieee80211_hw *hw;
	struct lkpi_sta *lsta;

	ic = vap->iv_ic;
	lhw = ic->ic_softc;

	/* We keep allocations de-coupled so we can deal with the two worlds. */
	if (lhw->ic_node_alloc == NULL)
		return (NULL);

	ni = lhw->ic_node_alloc(vap, mac);
	if (ni == NULL)
		return (NULL);

	hw = LHW_TO_HW(lhw);
	lsta = lkpi_lsta_alloc(vap, mac, hw, ni);
	if (lsta == NULL) {
		if (lhw->ic_node_free != NULL)
			lhw->ic_node_free(ni);
		return (NULL);
	}

	return (ni);
}

static int
lkpi_ic_node_init(struct ieee80211_node *ni)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	int error;

	ic = ni->ni_ic;
	lhw = ic->ic_softc;

	if (lhw->ic_node_init != NULL) {
		error = lhw->ic_node_init(ni);
		if (error != 0)
			return (error);
	}

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

	/* KASSERT lsta is not NULL here. Print ni/ni__refcnt. */

	/*
	 * Pass in the original ni just in case of error we could check that
	 * it is the same as lsta->ni.
	 */
	lkpi_lsta_free(lsta, ni);

	if (lhw->ic_node_free != NULL)
		lhw->ic_node_free(ni);
}

/*
 * lkpi_xmit() called from both the (*ic_raw_xmit) as well as the (*ic_transmit)
 * call path.
 * Unfortunately they have slightly different invariants.  See
 * ieee80211_raw_output() and ieee80211_parent_xmitpkt().
 * Both take care of the ni reference in case of error, and otherwise during
 * the callback after transmit.
 * The difference is that in case of error (*ic_raw_xmit) needs us to release
 * the mbuf, while (*ic_transmit) will free the mbuf itself.
 */
static int
lkpi_xmit(struct ieee80211_node *ni, struct mbuf *m,
    const struct ieee80211_bpf_params *params __unused,
    bool freem)
{
	struct lkpi_sta *lsta;
	int error;

	lsta = ni->ni_drv_data;
	LKPI_80211_LSTA_TXQ_LOCK(lsta);
#if 0
	if (!lsta->added_to_drv || !lsta->txq_ready) {
#else
	/*
	 * Backout this part of 886653492945f which breaks rtw88 or
	 * in general drivers without (*sta_state)() but only the
	 * legacy fallback to (*sta_add)().
	 */
	if (!lsta->txq_ready) {
#endif
		LKPI_80211_LSTA_TXQ_UNLOCK(lsta);
		if (freem)
			m_free(m);
		return (ENETDOWN);
	}

	/* Queue the packet and enqueue the task to handle it. */
	error = mbufq_enqueue(&lsta->txq, m);
	if (error != 0) {
		LKPI_80211_LSTA_TXQ_UNLOCK(lsta);
		if (freem)
			m_free(m);
#ifdef LINUXKPI_DEBUG_80211
		if (linuxkpi_debug_80211 & D80211_TRACE_TX)
			ic_printf(ni->ni_ic, "%s: mbufq_enqueue failed: %d\n",
			    __func__, error);
#endif
		return (ENETDOWN);
	}
	taskqueue_enqueue(taskqueue_thread, &lsta->txq_task);
	LKPI_80211_LSTA_TXQ_UNLOCK(lsta);

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE_TX)
		printf("%s:%d lsta %p ni %p %6D mbuf_qlen %d\n",
		    __func__, __LINE__, lsta, ni, ni->ni_macaddr, ":",
		    mbufq_len(&lsta->txq));
#endif

	return (0);
}

static int
lkpi_ic_raw_xmit(struct ieee80211_node *ni, struct mbuf *m,
        const struct ieee80211_bpf_params *params __unused)
{
	return (lkpi_xmit(ni, m, NULL, true));
}

#ifdef LKPI_80211_HW_CRYPTO
/*
 * This is a bit of a hack given we know we are operating on a
 * single frame and we know that hardware will deal with it.
 * But otherwise the enmic bit and the encrypt bit need to be
 * decoupled.
 */
static int
lkpi_hw_crypto_prepare_tkip(struct ieee80211_key *k,
    struct ieee80211_key_conf *kc, struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr;
	uint32_t hlen, hdrlen;
	uint8_t *p;

	/*
	 * TKIP only happens on data.
	 */
	hdr = (void *)skb->data;
	if (!ieee80211_is_data_present(hdr->frame_control))
		return (0);

	/*
	 * "enmic" (though we do not do that).
	 */
	/* any conditions to not apply this? */
	if (skb_tailroom(skb) < k->wk_cipher->ic_miclen)
		return (ENOBUFS);

	p = skb_put(skb, k->wk_cipher->ic_miclen);
	if ((kc->flags & IEEE80211_KEY_FLAG_PUT_MIC_SPACE) != 0)
		goto encrypt;

	/*
	 * (*enmic) which we hopefully do not have to do with hw accel.
	 * That means if we make it here we have a problem.
	 */
	TODO("(*enmic)");
	return (ENXIO);

encrypt:
	/*
	 * "encrypt" (though we do not do that).
	 */
	/*
	 * Check if we have anything to do as requested by driver
	 * or if we are done?
	 */
	if ((kc->flags & IEEE80211_KEY_FLAG_PUT_IV_SPACE) == 0 &&
	    (kc->flags & IEEE80211_KEY_FLAG_GENERATE_IV) == 0)
			return (0);

	hlen = k->wk_cipher->ic_header;
	if (skb_headroom(skb) < hlen)
		return (ENOBUFS);

	hdr = (void *)skb->data;
	hdrlen = ieee80211_hdrlen(hdr->frame_control);
	p = skb_push(skb, hlen);
	memmove(p, p + hlen, hdrlen);

	/* If driver request space only we are done. */
	if ((kc->flags & IEEE80211_KEY_FLAG_PUT_IV_SPACE) != 0)
		return (0);

	p += hdrlen;
	k->wk_cipher->ic_setiv(k, p);

	/* If we make it hear we do sw encryption. */
	TODO("sw encrypt");
	return (ENXIO);
}

static int
lkpi_hw_crypto_prepare_ccmp(struct ieee80211_key *k,
    struct ieee80211_key_conf *kc, struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr;
	uint32_t hlen, hdrlen;
	uint8_t *p;

	hdr = (void *)skb->data;

	/*
	 * Check if we have anythig to do as requested by driver
	 * or if we are done?
	 */
	if ((kc->flags & IEEE80211_KEY_FLAG_PUT_IV_SPACE) == 0 &&
	    (kc->flags & IEEE80211_KEY_FLAG_GENERATE_IV) == 0 &&
	    /* MFP */
	    !((kc->flags & IEEE80211_KEY_FLAG_GENERATE_IV_MGMT) != 0 &&
		ieee80211_is_mgmt(hdr->frame_control)))
			return (0);

	hlen = k->wk_cipher->ic_header;
	if (skb_headroom(skb) < hlen)
		return (ENOBUFS);

	hdrlen = ieee80211_hdrlen(hdr->frame_control);
	p = skb_push(skb, hlen);
	memmove(p, p + hlen, hdrlen);

	/* If driver request space only we are done. */
	if ((kc->flags & IEEE80211_KEY_FLAG_PUT_IV_SPACE) != 0)
		return (0);

	p += hdrlen;
	k->wk_cipher->ic_setiv(k, p);

	return (0);
}

static int
lkpi_hw_crypto_prepare(struct lkpi_sta *lsta, struct ieee80211_key *k,
    struct sk_buff *skb)
{
	struct ieee80211_tx_info *info;
	struct ieee80211_key_conf *kc;

	KASSERT(lsta != NULL, ("%s: lsta is NULL", __func__));
	KASSERT(k != NULL, ("%s: key is NULL", __func__));
	KASSERT(skb != NULL, ("%s: skb is NULL", __func__));

	kc = lsta->kc[k->wk_keyix];

	info = IEEE80211_SKB_CB(skb);
	info->control.hw_key = kc;

	/* MUST NOT happen. KASSERT? */
	if (kc == NULL) {
		ic_printf(lsta->ni->ni_ic, "%s: lsta %p k %p skb %p, "
		    "kc is NULL on hw crypto offload\n", __func__, lsta, k, skb);
		return (ENXIO);
	}

	switch (kc->cipher) {
	case WLAN_CIPHER_SUITE_TKIP:
		return (lkpi_hw_crypto_prepare_tkip(k, kc, skb));
	case WLAN_CIPHER_SUITE_CCMP:
		return (lkpi_hw_crypto_prepare_ccmp(k, kc, skb));
	case WLAN_CIPHER_SUITE_GCMP:
		return (lkpi_hw_crypto_prepare_ccmp(k, kc, skb));
	case WLAN_CIPHER_SUITE_WEP40:
	case WLAN_CIPHER_SUITE_WEP104:
	case WLAN_CIPHER_SUITE_CCMP_256:
	case WLAN_CIPHER_SUITE_GCMP_256:
	case WLAN_CIPHER_SUITE_AES_CMAC:
	case WLAN_CIPHER_SUITE_BIP_CMAC_256:
	case WLAN_CIPHER_SUITE_BIP_GMAC_128:
	case WLAN_CIPHER_SUITE_BIP_GMAC_256:
	default:
		ic_printf(lsta->ni->ni_ic, "%s: lsta %p k %p kc %p skb %p, "
		    "unsupported cipher suite %u (%s)\n", __func__, lsta, k, kc,
		    skb, kc->cipher, lkpi_cipher_suite_to_name(kc->cipher));
		return (EOPNOTSUPP);
	}
}

static uint8_t
lkpi_hw_crypto_tailroom(struct lkpi_sta *lsta, struct ieee80211_key *k)
{
	struct ieee80211_key_conf *kc;

	kc = lsta->kc[k->wk_keyix];
	if (kc == NULL)
		return (0);

	IMPROVE("which other flags need tailroom?");
	if (kc->flags & (IEEE80211_KEY_FLAG_PUT_MIC_SPACE))
		return (32);	/* Large enough to hold everything and pow2. */

	return (0);
}
#endif

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
	struct ieee80211_hdr *hdr;
	struct lkpi_txq *ltxq;
	void *buf;
	ieee80211_keyix keyix;
	uint8_t ac, tid, tailroom;

	M_ASSERTPKTHDR(m);
#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE_TX_DUMP)
		hexdump(mtod(m, const void *), m->m_len, "RAW TX (plain) ", 0);
#endif

	ni = lsta->ni;
	k = NULL;
	keyix = IEEE80211_KEYIX_NONE;
	wh = mtod(m, struct ieee80211_frame *);
	if (wh->i_fc[1] & IEEE80211_FC1_PROTECTED) {

#ifdef LKPI_80211_HW_CRYPTO
		if (lkpi_hwcrypto) {
			k = ieee80211_crypto_get_txkey(ni, m);
			if (k != NULL && lsta->kc[k->wk_keyix] != NULL)
				keyix = k->wk_keyix;
		}
#endif

		/* Encrypt the frame if need be. */
		if (keyix == IEEE80211_KEYIX_NONE) {
			/* Retrieve key for TX && do software encryption. */
			k = ieee80211_crypto_encap(ni, m);
			if (k == NULL) {
				ieee80211_free_node(ni);
				m_freem(m);
				return;
			}
		}
	}

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

#ifdef LKPI_80211_HW_CRYPTO
	if (lkpi_hwcrypto && keyix != IEEE80211_KEYIX_NONE)
		tailroom = lkpi_hw_crypto_tailroom(lsta, k);
	else
#endif
		tailroom = 0;

	/*
	 * net80211 should handle hw->extra_tx_headroom.
	 * Though for as long as we are copying we don't mind.
	 * XXX-BZ rtw88 asks for too much headroom for ipv6+tcp:
	 * https://lists.freebsd.org/archives/freebsd-transport/2022-February/000012.html
	 */
	skb = dev_alloc_skb(hw->extra_tx_headroom + tailroom + m->m_pkthdr.len);
	if (skb == NULL) {
		static uint8_t skb_alloc_failures = 0;

		if (skb_alloc_failures++ == 0) {
			int tid;

			sta = LSTA_TO_STA(lsta);
			ic_printf(ic, "ERROR %s: skb alloc failed %d + %d, lsta %p sta %p ni %p\n",
			    __func__, hw->extra_tx_headroom, m->m_pkthdr.len, lsta, sta, ni);
			for (tid = 0; tid < nitems(sta->txq); tid++) {
				if (sta->txq[tid] == NULL)
					continue;
				ltxq = TXQ_TO_LTXQ(sta->txq[tid]);
				ic_printf(ic, "  tid %d ltxq %p seen_dequeue %d stopped %d skb_queue_len %u\n",
				    tid, ltxq, ltxq->seen_dequeue, ltxq->stopped, skb_queue_len(&ltxq->skbq));
			}
		}
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

	hdr = (void *)skb->data;
	tid = linuxkpi_ieee80211_get_tid(hdr, true);
	if (tid == IEEE80211_NONQOS_TID) { /* == IEEE80211_NUM_TIDS */
		if (!ieee80211_is_data(hdr->frame_control)) {
			/* MGMT and CTRL frames go on TID 7/VO. */
			skb->priority = 7;
			ac = IEEE80211_AC_VO;
		} else {
			/* Other non-QOS traffic goes to BE. */
			/* Contrary to net80211 we MUST NOT promote M_EAPOL. */
			skb->priority = 0;
			ac = IEEE80211_AC_BE;
		}
	} else {
		skb->priority = tid & IEEE80211_QOS_CTL_TID_MASK;
		ac = ieee80211e_up_to_ac[tid & 7];
	}
	skb_set_queue_mapping(skb, ac);

	info = IEEE80211_SKB_CB(skb);
	info->flags |= IEEE80211_TX_CTL_REQ_TX_STATUS;
	/* Slight delay; probably only happens on scanning so fine? */
	if (c == NULL || c == IEEE80211_CHAN_ANYC)
		c = ic->ic_curchan;
	info->band = lkpi_net80211_chan_to_nl80211_band(c);
	info->hw_queue = vif->hw_queue[ac];
	if (m->m_flags & M_EAPOL)
		info->control.flags |= IEEE80211_TX_CTRL_PORT_CTRL_PROTO;
	info->control.vif = vif;
	/* XXX-BZ info->control.rates */
#ifdef __notyet__
#ifdef LKPI_80211_HT
	info->control.rts_cts_rate_idx=
	info->control.use_rts= /* RTS */
	info->control.use_cts_prot= /* RTS/CTS*/
#endif
#endif

	sta = LSTA_TO_STA(lsta);
#ifdef LKPI_80211_HW_CRYPTO
	if (lkpi_hwcrypto && keyix != IEEE80211_KEYIX_NONE) {
		int error;

		error = lkpi_hw_crypto_prepare(lsta, k, skb);
		if (error != 0) {
			/*
			 * We only have to free the skb which will free the
			 * mbuf and release the reference on the ni.
			 */
			dev_kfree_skb(skb);
			return;
		}
	}
#endif

	IMPROVE();

	ltxq = NULL;
	if (!ieee80211_is_data_present(hdr->frame_control)) {
		if (vif->type == NL80211_IFTYPE_STATION &&
		    lsta->added_to_drv &&
		    sta->txq[IEEE80211_NUM_TIDS] != NULL)
			ltxq = TXQ_TO_LTXQ(sta->txq[IEEE80211_NUM_TIDS]);
	} else if (lsta->added_to_drv &&
	    sta->txq[skb->priority] != NULL) {
		ltxq = TXQ_TO_LTXQ(sta->txq[skb->priority]);
	}
	if (ltxq == NULL)
		goto ops_tx;

	KASSERT(ltxq != NULL, ("%s: lsta %p sta %p m %p skb %p "
	    "ltxq %p != NULL\n", __func__, lsta, sta, m, skb, ltxq));

	LKPI_80211_LTXQ_LOCK(ltxq);
	skb_queue_tail(&ltxq->skbq, skb);
#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE_TX)
		printf("%s:%d mo_wake_tx_queue :: %d %lu lsta %p sta %p "
		    "ni %p %6D skb %p lxtq %p { qlen %u, ac %d tid %u } "
		    "WAKE_TX_Q ac %d prio %u qmap %u\n",
		    __func__, __LINE__,
		    curthread->td_tid, jiffies,
		    lsta, sta, ni, ni->ni_macaddr, ":", skb, ltxq,
		    skb_queue_len(&ltxq->skbq), ltxq->txq.ac,
		    ltxq->txq.tid, ac, skb->priority, skb->qmap);
#endif
	LKPI_80211_LTXQ_UNLOCK(ltxq);
	wiphy_lock(hw->wiphy);
	lkpi_80211_mo_wake_tx_queue(hw, &ltxq->txq);
	wiphy_unlock(hw->wiphy);
	return;

ops_tx:
#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE_TX)
		printf("%s:%d mo_tx :: lsta %p sta %p ni %p %6D skb %p "
		    "TX ac %d prio %u qmap %u\n",
		    __func__, __LINE__, lsta, sta, ni, ni->ni_macaddr, ":",
		    skb, ac, skb->priority, skb->qmap);
#endif
	memset(&control, 0, sizeof(control));
	control.sta = sta;
	wiphy_lock(hw->wiphy);
	lkpi_80211_mo_tx(hw, &control, skb);
	wiphy_unlock(hw->wiphy);
}

static void
lkpi_80211_txq_task(void *ctx, int pending)
{
	struct lkpi_sta *lsta;
	struct mbufq mq;
	struct mbuf *m;
	bool shall_tx;

	lsta = ctx;

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE_TX)
		printf("%s:%d lsta %p ni %p %6D pending %d mbuf_qlen %d\n",
		    __func__, __LINE__, lsta, lsta->ni, lsta->ni->ni_macaddr, ":",
		    pending, mbufq_len(&lsta->txq));
#endif

	mbufq_init(&mq, IFQ_MAXLEN);

	LKPI_80211_LSTA_TXQ_LOCK(lsta);
	/*
	 * Do not re-check lsta->txq_ready here; we may have a pending
	 * disassoc/deauth frame still.  On the contrary if txq_ready is
	 * false we do not have a valid sta anymore in the firmware so no
	 * point to try to TX.
	 * We also use txq_ready as a semaphore and will drain the txq manually
	 * if needed on our way towards SCAN/INIT in the state machine.
	 */
#if 0
	shall_tx = lsta->added_to_drv && lsta->txq_ready;
#else
	/*
	 * Backout this part of 886653492945f which breaks rtw88 or
	 * in general drivers without (*sta_state)() but only the
	 * legacy fallback to (*sta_add)().
	 */
	shall_tx = lsta->txq_ready;
#endif
	if (__predict_true(shall_tx))
		mbufq_concat(&mq, &lsta->txq);
	/*
	 * else a state change will push the packets out manually or
	 * lkpi_lsta_free() will drain the lsta->txq and free the mbufs.
	 */
	LKPI_80211_LSTA_TXQ_UNLOCK(lsta);

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
	return (lkpi_xmit(ni, m, NULL, false));
}

#ifdef LKPI_80211_HT
static int
lkpi_ic_recv_action(struct ieee80211_node *ni, const struct ieee80211_frame *wh,
    const uint8_t *frm, const uint8_t *efrm)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;

	ic = ni->ni_ic;
	lhw = ic->ic_softc;

	IMPROVE_HT("recv_action called; nothing to do in lkpi; make debugging");

	return (lhw->ic_recv_action(ni, wh, frm, efrm));
}

static int
lkpi_ic_send_action(struct ieee80211_node *ni, int category, int action, void *sa)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;

	ic = ni->ni_ic;
	lhw = ic->ic_softc;

	IMPROVE_HT("send_action called; nothing to do in lkpi; make debugging");

	return (lhw->ic_send_action(ni, category, action, sa));
}


static int
lkpi_ic_ampdu_enable(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;

	ic = ni->ni_ic;
	lhw = ic->ic_softc;

	IMPROVE_HT("ieee80211_ampdu_enable called; nothing to do in lkpi for now; make debugging");

	return (lhw->ic_ampdu_enable(ni, tap));
}

/*
 * (*ic_addba_request)() is called by ieee80211_ampdu_request() before
 * calling send_action(CAT_BA, BA_ADDBA_REQUEST).
 *
 * NB: returns 0 on ERROR!
 */
static int
lkpi_ic_addba_request(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap,
    int dialogtoken, int baparamset, int batimeout)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct ieee80211vap *vap;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;
	struct ieee80211_ampdu_params params = { };
	int error;

	ic = ni->ni_ic;
	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	vap = ni->ni_vap;
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);
	lsta = ni->ni_drv_data;
	sta = LSTA_TO_STA(lsta);

	if (!lsta->added_to_drv) {
		ic_printf(ic, "%s: lsta %p ni %p, sta %p not added to firmware\n",
		    __func__, lsta, ni, sta);
		return (0);
	}

	params.sta = sta;
	params.action = IEEE80211_AMPDU_TX_START;
	/* Keep 0 here! */
	params.buf_size = 0;
	params.timeout = 0;
	params.ssn = tap->txa_start & (IEEE80211_SEQ_RANGE-1);
	params.tid = tap->txa_tid;
	params.amsdu = false;

	IEEE80211_UNLOCK(ic);
	wiphy_lock(hw->wiphy);
	error = lkpi_80211_mo_ampdu_action(hw, vif, &params);
	wiphy_unlock(hw->wiphy);
	IEEE80211_LOCK(ic);
	if (error != 0) {
		ic_printf(ic, "%s: mo_ampdu_action returned %d. ni %p tap %p\n",
		    __func__, error, ni, tap);
		return (0);
	}

	return (lhw->ic_addba_request(ni, tap, dialogtoken, baparamset, batimeout));
}

/*
 * (*ic_addba_response)() is called from ht_recv_action_ba_addba_response()
 * and calls the default ieee80211_addba_response() which always returns 1.
 *
 * NB: No error checking in net80211!
 * Staying with 0 is an error.
 */
static int
lkpi_ic_addba_response(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap,
    int status, int baparamset, int batimeout)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct ieee80211vap *vap;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;
	struct ieee80211_ampdu_params params = { };
	int error;

	ic = ni->ni_ic;
	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	vap = ni->ni_vap;
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);
	lsta = ni->ni_drv_data;
	sta = LSTA_TO_STA(lsta);

	if (!lsta->added_to_drv) {
		ic_printf(ic, "%s: lsta %p ni %p, sta %p not added to firmware\n",
		    __func__, lsta, ni, sta);
		return (0);
	}

	if (status == IEEE80211_STATUS_SUCCESS) {
		params.sta = sta;
		params.action = IEEE80211_AMPDU_TX_OPERATIONAL;
		params.buf_size = tap->txa_wnd;
		params.timeout = 0;
		params.ssn = 0;
		params.tid = tap->txa_tid;
		if ((tap->txa_flags & IEEE80211_AGGR_AMSDU) != 0)
			params.amsdu = true;
		else
			params.amsdu = false;
	} else {
		/* We need to free the allocated resources. */
		params.sta = sta;
		switch (status) {
			/* params.action = FLUSH, FLUSH_CONT */
		default:
			params.action = IEEE80211_AMPDU_TX_STOP_CONT;
			break;
		}
		params.buf_size = 0;
		params.timeout = 0;
		params.ssn = 0;
		params.tid = tap->txa_tid;
		params.amsdu = false;
	}

	IEEE80211_UNLOCK(ic);
	wiphy_lock(hw->wiphy);
	error = lkpi_80211_mo_ampdu_action(hw, vif, &params);
	wiphy_unlock(hw->wiphy);
	IEEE80211_LOCK(ic);
	if (error != 0) {
		ic_printf(ic, "%s: mo_ampdu_action returned %d. ni %p tap %p\n",
		    __func__, error, ni, tap);
		return (0);
	}

	IMPROVE_HT("who unleashes the TXQ? and when?, do we need to ni->ni_txseqs[tid] = tap->txa_start & 0xfff;");

	return (lhw->ic_addba_response(ni, tap, status, baparamset, batimeout));
}

/*
 * (*ic_addba_stop)() is called from ampdu_tx_stop(), ht_recv_action_ba_delba(),
 * and ieee80211_ampdu_stop() and calls the default ieee80211_addba_stop().
 */
static void
lkpi_ic_addba_stop(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct ieee80211vap *vap;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;
	struct ieee80211_ampdu_params params = { };
	int error;

	ic = ni->ni_ic;
	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	vap = ni->ni_vap;
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);
	lsta = ni->ni_drv_data;
	sta = LSTA_TO_STA(lsta);

	if (!lsta->added_to_drv) {
		ic_printf(ic, "%s: lsta %p ni %p, sta %p not added to firmware\n",
		    __func__, lsta, ni, sta);
		goto n80211;
	}

	/* We need to free the allocated resources. */
	params.sta = sta;
	IMPROVE("net80211 does not provide a reason to us");
	params.action = IEEE80211_AMPDU_TX_STOP_CONT; /* params.action = FLUSH, FLUSH_CONT */
	params.buf_size = 0;
	params.timeout = 0;
	params.ssn = 0;
	params.tid = tap->txa_tid;
	params.amsdu = false;

	IEEE80211_UNLOCK(ic);
	wiphy_lock(hw->wiphy);
	error = lkpi_80211_mo_ampdu_action(hw, vif, &params);
	wiphy_unlock(hw->wiphy);
	IEEE80211_LOCK(ic);
	if (error != 0) {
		ic_printf(ic, "%s: mo_ampdu_action returned %d. ni %p tap %p\n",
		    __func__, error, ni, tap);
		goto n80211;
	}

	IMPROVE_HT("anyting else?");

n80211:
	lhw->ic_addba_stop(ni, tap);
}

static void
lkpi_ic_addba_response_timeout(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;

	ic = ni->ni_ic;
	lhw = ic->ic_softc;

	IMPROVE_HT();

	lhw->ic_addba_response_timeout(ni, tap);
}

static void
lkpi_ic_bar_response(struct ieee80211_node *ni, struct ieee80211_tx_ampdu *tap,
    int status)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;

	ic = ni->ni_ic;
	lhw = ic->ic_softc;

	IMPROVE_HT();

	lhw->ic_bar_response(ni, tap, status);
}

static int
lkpi_ic_ampdu_rx_start(struct ieee80211_node *ni, struct ieee80211_rx_ampdu *rap,
    int baparamset, int batimeout, int baseqctl)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct ieee80211vap *vap;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;
	struct ieee80211_ampdu_params params = { };
	int error;

	ic = ni->ni_ic;
	lhw = ic->ic_softc;
	hw = LHW_TO_HW(lhw);
	vap = ni->ni_vap;
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);
	lsta = ni->ni_drv_data;
	sta = LSTA_TO_STA(lsta);

	IEEE80211_UNLOCK_ASSERT(ic);

	if (!lsta->added_to_drv) {
		ic_printf(ic, "%s: lsta %p ni %p vap %p, sta %p not added to firmware\n",
		    __func__, lsta, ni, vap, sta);
		return (-ENXIO);
	}

	if (lsta->state != IEEE80211_STA_AUTHORIZED) {
		ic_printf(ic, "%s: lsta %p ni %p vap %p, sta %p state %d not AUTHORIZED\n",
		    __func__, lsta, ni, vap, sta, lsta->state);
		return (-ENXIO);
	}

	params.sta = sta;
	params.action = IEEE80211_AMPDU_RX_START;
	params.buf_size = _IEEE80211_MASKSHIFT(le16toh(baparamset), IEEE80211_BAPS_BUFSIZ);
	if (params.buf_size == 0)
		params.buf_size = IEEE80211_MAX_AMPDU_BUF_HT;
	else
		params.buf_size = min(params.buf_size, IEEE80211_MAX_AMPDU_BUF_HT);
	if (hw->max_rx_aggregation_subframes > 0 &&
	    params.buf_size > hw->max_rx_aggregation_subframes)
		params.buf_size = hw->max_rx_aggregation_subframes;
	params.timeout = le16toh(batimeout);
	params.ssn = _IEEE80211_MASKSHIFT(le16toh(baseqctl), IEEE80211_BASEQ_START);
	params.tid = _IEEE80211_MASKSHIFT(le16toh(baparamset), IEEE80211_BAPS_TID);

	/* Based on net80211::ampdu_rx_start(). */
	if ((vap->iv_htcaps & IEEE80211_HTC_RX_AMSDU_AMPDU) &&
	    (_IEEE80211_MASKSHIFT(baparamset, IEEE80211_BAPS_AMSDU)))
		params.amsdu = true;
	else
		params.amsdu = false;

	wiphy_lock(hw->wiphy);
	error = lkpi_80211_mo_ampdu_action(hw, vif, &params);
	wiphy_unlock(hw->wiphy);
	if (error != 0) {
		ic_printf(ic, "%s: mo_ampdu_action returned %d. ni %p rap %p\n",
		    __func__, error, ni, rap);
		return (error);
	}

	if (!ieee80211_hw_check(hw, SUPPORTS_REORDERING_BUFFER)) {
		IMPROVE("%s: TODO: SUPPORTS_REORDERING_BUFFER not set; check net80211\n", __func__);
	}

	IMPROVE_HT("net80211 is missing the error check on return and assumes success");

	error = lhw->ic_ampdu_rx_start(ni, rap, baparamset, batimeout, baseqctl);
	return (error);
}

static void
lkpi_ic_ampdu_rx_stop(struct ieee80211_node *ni, struct ieee80211_rx_ampdu *rap)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	struct ieee80211_hw *hw;
	struct ieee80211vap *vap;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;
	struct ieee80211_ampdu_params params = { };
	int error;
	uint8_t tid;
	bool ic_locked;

	ic = ni->ni_ic;
	lhw = ic->ic_softc;

	/*
	 * We should not (cannot) call into mac80211 ops with AMPDU_RX_STOP if
	 * we did not START.  Some drivers pass it down to firmware which will
	 * simply barf and net80211 calls ieee80211_ht_node_cleanup() from
	 * ieee80211_ht_node_init() amongst others which will iterate over all
	 * tid and call ic_ampdu_rx_stop() unconditionally.
	 * XXX net80211 should probably be more "gentle" in these cases and
	 * track some state itself.
	 */
	if ((rap->rxa_flags & IEEE80211_AGGR_RUNNING) == 0)
		goto net80211_only;

	hw = LHW_TO_HW(lhw);
	vap = ni->ni_vap;
	lvif = VAP_TO_LVIF(vap);
	vif = LVIF_TO_VIF(lvif);
	lsta = ni->ni_drv_data;
	if (lsta == NULL) {
		ic_printf(ic, "%s: lsta %p ni %p vap %p, lsta is NULL\n",
		    __func__, lsta, ni, vap);
		goto net80211_only;
	}
	sta = LSTA_TO_STA(lsta);

	if (!lsta->added_to_drv) {
		ic_printf(ic, "%s: lsta %p ni %p vap %p, sta %p not added to firmware\n",
		    __func__, lsta, ni, vap, sta);
		goto net80211_only;
	}

	if (lsta->state != IEEE80211_STA_AUTHORIZED) {
		ic_printf(ic, "%s: lsta %p ni %p vap %p, sta %p state %d not AUTHORIZED\n",
		    __func__, lsta, ni, vap, sta, lsta->state);
		goto net80211_only;
	}

	IMPROVE_HT("This really should be passed from ht_recv_action_ba_delba.");
	for (tid = 0; tid < WME_NUM_TID; tid++) {
		if (&ni->ni_rx_ampdu[tid] == rap)
			break;
	}
	if (tid == WME_NUM_TID) {
		ic_printf(ic, "%s: lsta %p ni %p vap %p, sta %p TID not found\n",
		    __func__, lsta, ni, vap, sta);
		goto net80211_only;
	}

	params.sta = sta;
	params.action = IEEE80211_AMPDU_RX_STOP;
	params.buf_size = 0;
	params.timeout = 0;
	params.ssn = 0;
	params.tid = tid;
	params.amsdu = false;

	ic_locked = IEEE80211_IS_LOCKED(ic);
	if (ic_locked)
		IEEE80211_UNLOCK(ic);
	wiphy_lock(hw->wiphy);
	error = lkpi_80211_mo_ampdu_action(hw, vif, &params);
	wiphy_unlock(hw->wiphy);
	if (ic_locked)
		IEEE80211_LOCK(ic);
	if (error != 0)
		ic_printf(ic, "%s: mo_ampdu_action returned %d. ni %p rap %p\n",
		    __func__, error, ni, rap);

net80211_only:
	lhw->ic_ampdu_rx_stop(ni, rap);
}
#endif

static void
lkpi_ic_getradiocaps_ht(struct ieee80211com *ic, struct ieee80211_hw *hw,
    uint8_t *bands, int *chan_flags, enum nl80211_band band)
{
#ifdef LKPI_80211_HT
	struct ieee80211_sta_ht_cap *ht_cap;

	ht_cap = &hw->wiphy->bands[band]->ht_cap;
	if (!ht_cap->ht_supported)
		return;

	switch (band) {
	case NL80211_BAND_2GHZ:
		setbit(bands, IEEE80211_MODE_11NG);
		break;
	case NL80211_BAND_5GHZ:
		setbit(bands, IEEE80211_MODE_11NA);
		break;
	default:
		IMPROVE("Unsupported band %d", band);
		return;
	}

	ic->ic_htcaps = IEEE80211_HTC_HT;	/* HT operation */

	/*
	 * Rather than manually checking each flag and
	 * translating IEEE80211_HT_CAP_ to IEEE80211_HTCAP_,
	 * simply copy the 16bits.
	 */
	ic->ic_htcaps |= ht_cap->cap;

	/* Then deal with the other flags. */
	if (ieee80211_hw_check(hw, AMPDU_AGGREGATION))
		ic->ic_htcaps |= IEEE80211_HTC_AMPDU;
#ifdef __notyet__
	if (ieee80211_hw_check(hw, TX_AMSDU))
		ic->ic_htcaps |= IEEE80211_HTC_AMSDU;
	if (ieee80211_hw_check(hw, SUPPORTS_AMSDU_IN_AMPDU))
		ic->ic_htcaps |= (IEEE80211_HTC_RX_AMSDU_AMPDU |
		    IEEE80211_HTC_TX_AMSDU_AMPDU);
#endif

	IMPROVE("PS, ampdu_*, ht_cap.mcs.tx_params, ...");
	ic->ic_htcaps |= IEEE80211_HTCAP_SMPS_OFF;

	/* Only add HT40 channels if supported. */
	if ((ic->ic_htcaps & IEEE80211_HTCAP_CHWIDTH40) != 0 &&
	    chan_flags != NULL)
		*chan_flags |= NET80211_CBW_FLAG_HT40;
#endif
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

		IMPROVE("the bitrates may have flags?");
		setbit(bands, IEEE80211_MODE_11G);

		lkpi_ic_getradiocaps_ht(ic, hw, bands, &chan_flags,
		    NL80211_BAND_2GHZ);

		channels = hw->wiphy->bands[NL80211_BAND_2GHZ]->channels;
		for (i = 0; i < nchans && *n < maxchan; i++) {
			uint32_t nflags = 0;
			int cflags = chan_flags;

			if (channels[i].flags & IEEE80211_CHAN_DISABLED) {
				ic_printf(ic, "%s: Skipping disabled chan "
				    "[%u/%u/%#x]\n", __func__,
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
			    ieee80211_mhz2ieee(channels[i].center_freq,
				lkpi_nl80211_band_to_net80211_band(channels[i].band)),
			    channels[i].center_freq, channels[i].max_power,
			    nflags, bands, cflags);
			/* net80211::ENOBUFS: *n >= maxchans */
			if (error != 0 && error != ENOBUFS)
				ic_printf(ic, "%s: Adding chan %u/%u/%#x/%#x/%#x/%#x "
				    "returned error %d\n",
				    __func__, channels[i].hw_value,
				    channels[i].center_freq, channels[i].flags,
				    nflags, chan_flags, cflags, error);
			if (error != 0)
				break;
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

		lkpi_ic_getradiocaps_ht(ic, hw, bands, &chan_flags,
		    NL80211_BAND_5GHZ);

#ifdef LKPI_80211_VHT
		if (hw->wiphy->bands[NL80211_BAND_5GHZ]->vht_cap.vht_supported) {

			ic->ic_flags_ext |= IEEE80211_FEXT_VHT;
			ic->ic_vht_cap.vht_cap_info =
			    hw->wiphy->bands[NL80211_BAND_5GHZ]->vht_cap.cap;
			ic->ic_vht_cap.supp_mcs =
			    hw->wiphy->bands[NL80211_BAND_5GHZ]->vht_cap.vht_mcs;

			setbit(bands, IEEE80211_MODE_VHT_5GHZ);
			chan_flags |= NET80211_CBW_FLAG_VHT80;
			if (IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_IS_160MHZ(
			    ic->ic_vht_cap.vht_cap_info))
				chan_flags |= NET80211_CBW_FLAG_VHT160;
			if (IEEE80211_VHTCAP_SUPP_CHAN_WIDTH_IS_160_80P80MHZ(
			    ic->ic_vht_cap.vht_cap_info))
				chan_flags |= NET80211_CBW_FLAG_VHT80P80;
		}
#endif

		channels = hw->wiphy->bands[NL80211_BAND_5GHZ]->channels;
		for (i = 0; i < nchans && *n < maxchan; i++) {
			uint32_t nflags = 0;
			int cflags = chan_flags;

			if (channels[i].flags & IEEE80211_CHAN_DISABLED) {
				ic_printf(ic, "%s: Skipping disabled chan "
				    "[%u/%u/%#x]\n", __func__,
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
			    ieee80211_mhz2ieee(channels[i].center_freq,
				lkpi_nl80211_band_to_net80211_band(channels[i].band)),
			    channels[i].center_freq, channels[i].max_power,
			    nflags, bands, cflags);
			/* net80211::ENOBUFS: *n >= maxchans */
			if (error != 0 && error != ENOBUFS)
				ic_printf(ic, "%s: Adding chan %u/%u/%#x/%#x/%#x/%#x "
				    "returned error %d\n",
				    __func__, channels[i].hw_value,
				    channels[i].center_freq, channels[i].flags,
				    nflags, chan_flags, cflags, error);
			if (error != 0)
				break;
		}
	}
}

static void *
lkpi_ieee80211_ifalloc(void)
{
	struct ieee80211com *ic;

	ic = malloc(sizeof(*ic), M_LKPI80211, M_WAITOK | M_ZERO);

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
	int ac;

	/* Get us and the driver data also allocated. */
	wiphy = wiphy_new(&linuxkpi_mac80211cfgops, sizeof(*lhw) + priv_len);
	if (wiphy == NULL)
		return (NULL);

	lhw = wiphy_priv(wiphy);
	lhw->ops = ops;

	LKPI_80211_LHW_SCAN_LOCK_INIT(lhw);
	LKPI_80211_LHW_TXQ_LOCK_INIT(lhw);
	spin_lock_init(&lhw->txq_lock);
	sx_init_flags(&lhw->lvif_sx, "lhw-lvif", SX_RECURSE | SX_DUPOK);
	LKPI_80211_LHW_MC_LOCK_INIT(lhw);
	TAILQ_INIT(&lhw->lvif_head);
	__hw_addr_init(&lhw->mc_list);
	for (ac = 0; ac < IEEE80211_NUM_ACS; ac++) {
		lhw->txq_generation[ac] = 1;
		TAILQ_INIT(&lhw->scheduled_txqs[ac]);
	}

	/* Chanctx_conf */
	INIT_LIST_HEAD(&lhw->lchanctx_list);

	/* Deferred RX path. */
	LKPI_80211_LHW_RXQ_LOCK_INIT(lhw);
	TASK_INIT(&lhw->rxq_task, 0, lkpi_80211_lhw_rxq_task, lhw);
	mbufq_init(&lhw->rxq, 32 * NAPI_POLL_WEIGHT);
	lhw->rxq_stopped = false;

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

	IMPROVE();

	return (hw);
}

void
linuxkpi_ieee80211_iffree(struct ieee80211_hw *hw)
{
	struct lkpi_hw *lhw;
	struct mbuf *m;

	lhw = HW_TO_LHW(hw);
	free(lhw->ic, M_LKPI80211);
	lhw->ic = NULL;

	/*
	 * Drain the deferred RX path.
	 */
	LKPI_80211_LHW_RXQ_LOCK(lhw);
	lhw->rxq_stopped = true;
	LKPI_80211_LHW_RXQ_UNLOCK(lhw);

	/* Drain taskq, won't be restarted due to rxq_stopped being set. */
	while (taskqueue_cancel(taskqueue_thread, &lhw->rxq_task, NULL) != 0)
		taskqueue_drain(taskqueue_thread, &lhw->rxq_task);

	/* Flush mbufq (make sure to release ni refs!). */
	m = mbufq_dequeue(&lhw->rxq);
	while (m != NULL) {
#ifdef LKPI_80211_USE_MTAG
		struct m_tag *mtag;

		mtag = m_tag_locate(m, MTAG_ABI_LKPI80211, LKPI80211_TAG_RXNI, NULL);
		if (mtag != NULL) {
			struct lkpi_80211_tag_rxni *rxni;

			rxni = (struct lkpi_80211_tag_rxni *)(mtag + 1);
			ieee80211_free_node(rxni->ni);
		}
#else
		if (m->m_pkthdr.PH_loc.ptr != NULL) {
			struct ieee80211_node *ni;

			ni = m->m_pkthdr.PH_loc.ptr;
			ieee80211_free_node(ni);
		}
#endif
		m_freem(m);
		m = mbufq_dequeue(&lhw->rxq);
	}
	KASSERT(mbufq_empty(&lhw->rxq), ("%s: lhw %p has rxq len %d != 0\n",
	    __func__, lhw, mbufq_len(&lhw->rxq)));
	LKPI_80211_LHW_RXQ_LOCK_DESTROY(lhw);

	/* Chanctx_conf. */
	if (!list_empty_careful(&lhw->lchanctx_list)) {
		struct lkpi_chanctx *lchanctx, *next;
		struct ieee80211_chanctx_conf *chanctx_conf;

		list_for_each_entry_safe(lchanctx, next, &lhw->lchanctx_list, entry) {
			if (lchanctx->added_to_drv) {
				/* In reality we should panic? */
				chanctx_conf = &lchanctx->chanctx_conf;
				lkpi_80211_mo_remove_chanctx(hw, chanctx_conf);
			}
			list_del(&lchanctx->entry);
			free(lchanctx, M_LKPI80211);
		}
	}

	LKPI_80211_LHW_MC_LOCK(lhw);
	lkpi_cleanup_mcast_list_locked(lhw);
	LKPI_80211_LHW_MC_UNLOCK(lhw);

	/* Cleanup more of lhw here or in wiphy_free()? */
	spin_lock_destroy(&lhw->txq_lock);
	LKPI_80211_LHW_TXQ_LOCK_DESTROY(lhw);
	LKPI_80211_LHW_SCAN_LOCK_DESTROY(lhw);
	sx_destroy(&lhw->lvif_sx);
	LKPI_80211_LHW_MC_LOCK_DESTROY(lhw)
	IMPROVE();
}

void
linuxkpi_set_ieee80211_dev(struct ieee80211_hw *hw)
{
	struct lkpi_hw *lhw;
	struct ieee80211com *ic;
	struct device *dev;

	lhw = HW_TO_LHW(hw);
	ic = lhw->ic;

	/* Save the backpointer from net80211 to LinuxKPI. */
	ic->ic_softc = lhw;

	/*
	 * Set a proper name before ieee80211_ifattach() if dev is set.
	 * ath1xk also unset the dev so we need to check.
	 */
	dev = wiphy_dev(hw->wiphy);
	if (dev != NULL) {
		ic->ic_name = dev_name(dev);
	} else {
		TODO("adjust arguments to still have the old dev or go through "
		    "the hoops of getting the bsddev from hw and detach; "
		    "or do in XXX; check ath1kx drivers");
	}

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

int
linuxkpi_ieee80211_ifattach(struct ieee80211_hw *hw)
{
	struct ieee80211com *ic;
	struct lkpi_hw *lhw;
	int band, i;

	lhw = HW_TO_LHW(hw);
	ic = lhw->ic;

	/* We do it this late as wiphy->dev should be set for the name. */
	lhw->workq = alloc_ordered_workqueue(wiphy_name(hw->wiphy), 0);
	if (lhw->workq == NULL)
		return (-EAGAIN);

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

#ifdef __not_yet__
	/* See comment in lkpi_80211_txq_tx_one(). */
	ic->ic_headroom = hw->extra_tx_headroom;
#endif

	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;

	/* Set device capabilities. */
	/* XXX-BZ we need to get these from linux80211/drivers and convert. */
	ic->ic_caps =
	    IEEE80211_C_STA |
	    IEEE80211_C_MONITOR |
	    IEEE80211_C_WPA |		/* WPA/RSN */
#ifdef LKPI_80211_WME
	    IEEE80211_C_WME |
#endif
#if 0
	    IEEE80211_C_PMGT |
#endif
	    IEEE80211_C_SHSLOT |	/* short slot time supported */
	    IEEE80211_C_SHPREAMBLE	/* short preamble supported */
	    ;

#ifdef LKPI_80211_BGSCAN
	if (lhw->ops->hw_scan)
		ic->ic_caps |= IEEE80211_C_BGSCAN;
#endif

	lkpi_enable_hw_scan(lhw);

	/* Does HW support Fragmentation offload? */
	if (ieee80211_hw_check(hw, SUPPORTS_TX_FRAG))
		ic->ic_flags_ext |= IEEE80211_FEXT_FRAG_OFFLOAD;

	/* Does HW support full AMPDU[-TX] offload? */
	if (ieee80211_hw_check(hw, AMPDU_AGGREGATION))
		ic->ic_flags_ext |= IEEE80211_FEXT_AMPDU_OFFLOAD;
#ifdef __notyet__
	if (ieee80211_hw_check(hw, TX_AMSDU))
	if (ieee80211_hw_check(hw, SUPPORTS_AMSDU_IN_AMPDU))
#endif

	/*
	 * The wiphy variables report bitmasks of avail antennas.
	 * (*get_antenna) get the current bitmask sets which can be
	 * altered by (*set_antenna) for some drivers.
	 * XXX-BZ will the count alone do us much good long-term in net80211?
	 */
	if (hw->wiphy->available_antennas_rx ||
	    hw->wiphy->available_antennas_tx) {
		uint32_t rxs, txs;

		if (lkpi_80211_mo_get_antenna(hw, &txs, &rxs) == 0) {
			ic->ic_rxstream = bitcount32(rxs);
			ic->ic_txstream = bitcount32(txs);
		}
	}

	ic->ic_cryptocaps = 0;
#ifdef LKPI_80211_HW_CRYPTO
	if (lkpi_hwcrypto && hw->wiphy->n_cipher_suites > 0) {
		uint32_t hwciphers;

		hwciphers = 0;
		for (i = 0; i < hw->wiphy->n_cipher_suites; i++) {
			uint32_t cs;

			cs = lkpi_l80211_to_net80211_cyphers(
			    ic, hw->wiphy->cipher_suites[i]);
			if (cs == IEEE80211_CRYPTO_TKIP) {
				/*
				 * We do set this here.  We will only find out
				 * when doing a SET_KEY operation depending on
				 * what the driver returns.
				 * net80211::ieee80211_crypto_newkey()
				 * checks this so we will have to do flags
				 * surgery later.
				 */
				cs |= IEEE80211_CRYPTO_TKIPMIC;
			}
			hwciphers |= cs;
		}
		/*
		 * (20250415) nothing anywhere in the path checks we actually
		 * support all these in net80211.
		 * net80211 supports _256 variants but the ioctl does not.
		 */
		IMPROVE("as net80211 grows more support, enable them");
		hwciphers &= (IEEE80211_CRYPTO_WEP |
		    IEEE80211_CRYPTO_TKIP | IEEE80211_CRYPTO_TKIPMIC |
		    IEEE80211_CRYPTO_AES_CCM | IEEE80211_CRYPTO_AES_GCM_128);
		/*
		 * We only support CCMP here, so further filter.
		 * Also permit TKIP if turned on.
		 */
		hwciphers &= (IEEE80211_CRYPTO_AES_CCM |
		    IEEE80211_CRYPTO_AES_GCM_128 |
		    (lkpi_hwcrypto_tkip ? (IEEE80211_CRYPTO_TKIP |
		    IEEE80211_CRYPTO_TKIPMIC) : 0));
		ieee80211_set_hardware_ciphers(ic, hwciphers);
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
	ic->ic_set_channel = lkpi_ic_set_channel;
	ic->ic_transmit = lkpi_ic_transmit;
	ic->ic_raw_xmit = lkpi_ic_raw_xmit;
	ic->ic_vap_create = lkpi_ic_vap_create;
	ic->ic_vap_delete = lkpi_ic_vap_delete;
	ic->ic_getradiocaps = lkpi_ic_getradiocaps;
	ic->ic_wme.wme_update = lkpi_ic_wme_update;

	lhw->ic_scan_curchan = ic->ic_scan_curchan;
	ic->ic_scan_curchan = lkpi_ic_scan_curchan;
	lhw->ic_scan_mindwell = ic->ic_scan_mindwell;
	ic->ic_scan_mindwell = lkpi_ic_scan_mindwell;

	lhw->ic_node_alloc = ic->ic_node_alloc;
	ic->ic_node_alloc = lkpi_ic_node_alloc;
	lhw->ic_node_init = ic->ic_node_init;
	ic->ic_node_init = lkpi_ic_node_init;
	lhw->ic_node_cleanup = ic->ic_node_cleanup;
	ic->ic_node_cleanup = lkpi_ic_node_cleanup;
	lhw->ic_node_free = ic->ic_node_free;
	ic->ic_node_free = lkpi_ic_node_free;

#ifdef LKPI_80211_HT
	/*
	 * Only attach if the driver/firmware supports (*ampdu_action)().
	 * Otherwise it is in the hands of net80211.
	 */
	if (lhw->ops->ampdu_action != NULL) {
		lhw->ic_recv_action = ic->ic_recv_action;
		ic->ic_recv_action = lkpi_ic_recv_action;
		lhw->ic_send_action = ic->ic_send_action;
		ic->ic_send_action = lkpi_ic_send_action;

		lhw->ic_ampdu_enable = ic->ic_ampdu_enable;
		ic->ic_ampdu_enable = lkpi_ic_ampdu_enable;

		lhw->ic_addba_request = ic->ic_addba_request;
		ic->ic_addba_request = lkpi_ic_addba_request;
		lhw->ic_addba_response = ic->ic_addba_response;
		ic->ic_addba_response = lkpi_ic_addba_response;
		lhw->ic_addba_stop = ic->ic_addba_stop;
		ic->ic_addba_stop = lkpi_ic_addba_stop;
		lhw->ic_addba_response_timeout = ic->ic_addba_response_timeout;
		ic->ic_addba_response_timeout = lkpi_ic_addba_response_timeout;

		lhw->ic_bar_response = ic->ic_bar_response;
		ic->ic_bar_response = lkpi_ic_bar_response;

		lhw->ic_ampdu_rx_start = ic->ic_ampdu_rx_start;
		ic->ic_ampdu_rx_start = lkpi_ic_ampdu_rx_start;
		lhw->ic_ampdu_rx_stop = ic->ic_ampdu_rx_stop;
		ic->ic_ampdu_rx_stop = lkpi_ic_ampdu_rx_stop;
	}
#endif

	lkpi_radiotap_attach(lhw);

	/*
	 * Assign the first possible channel for now;  seems Realtek drivers
	 * expect one.
	 * Also remember the amount of bands we support and the most rates
	 * in any band so we can scale [(ext) sup rates] IE(s) accordingly.
	 */
	lhw->supbands = lhw->max_rates = 0;
	for (band = 0; band < NUM_NL80211_BANDS; band++) {
		struct ieee80211_supported_band *supband;
		struct linuxkpi_ieee80211_channel *channels;

		supband = hw->wiphy->bands[band];
		if (supband == NULL || supband->n_channels == 0)
			continue;

		lhw->supbands++;
		lhw->max_rates = max(lhw->max_rates, supband->n_bitrates);

		/* If we have a channel, we need to keep counting supbands. */
		if (hw->conf.chandef.chan != NULL)
			continue;

		channels = supband->channels;
		for (i = 0; i < supband->n_channels; i++) {

			if (channels[i].flags & IEEE80211_CHAN_DISABLED)
				continue;

			cfg80211_chandef_create(&hw->conf.chandef, &channels[i],
#ifdef LKPI_80211_HT
			    (ic->ic_flags_ht & IEEE80211_FHT_HT) ? NL80211_CHAN_HT20 :
#endif
			    NL80211_CHAN_NO_HT);
			break;
		}
	}

	IMPROVE("see net80211::ieee80211_chan_init vs. wiphy->bands[].bitrates possibly in lkpi_ic_getradiocaps?");

	/* Make sure we do not support more than net80211 is willing to take. */
	if (lhw->max_rates > IEEE80211_RATE_MAXSIZE) {
		ic_printf(ic, "%s: limiting max_rates %d to %d!\n", __func__,
		    lhw->max_rates, IEEE80211_RATE_MAXSIZE);
		lhw->max_rates = IEEE80211_RATE_MAXSIZE;
	}

	/*
	 * The maximum supported bitrates on any band + size for
	 * DSSS Parameter Set give our per-band IE size.
	 * SSID is the responsibility of the driver and goes on the side.
	 * The user specified bits coming from the vap go into the
	 * "common ies" fields.
	 */
	lhw->scan_ie_len = 2 + IEEE80211_RATE_SIZE;
	if (lhw->max_rates > IEEE80211_RATE_SIZE)
		lhw->scan_ie_len += 2 + (lhw->max_rates - IEEE80211_RATE_SIZE);

	if (hw->wiphy->features & NL80211_FEATURE_DS_PARAM_SET_IE_IN_PROBES) {
		/*
		 * net80211 does not seem to support the DSSS Parameter Set but
		 * some of the drivers insert it so calculate the extra fixed
		 * space in.
		 */
		lhw->scan_ie_len += 2 + 1;
	}

#if defined(LKPI_80211_HT)
	if ((ic->ic_htcaps & IEEE80211_HTC_HT) != 0)
		lhw->scan_ie_len += sizeof(struct ieee80211_ie_htcap);
#endif
#if defined(LKPI_80211_VHT)
	if (IEEE80211_CONF_VHT(ic))
		lhw->scan_ie_len += 2 + sizeof(struct ieee80211_vht_cap);
#endif

	/* Reduce the max_scan_ie_len "left" by the amount we consume already. */
	if (hw->wiphy->max_scan_ie_len > 0) {
		if (lhw->scan_ie_len > hw->wiphy->max_scan_ie_len)
			goto err;
		hw->wiphy->max_scan_ie_len -= lhw->scan_ie_len;
	}

	if (bootverbose) {
		ic_printf(ic, "netdev_features %b\n", hw->netdev_features, NETIF_F_BITS);
		ieee80211_announce(ic);
	}

	return (0);
err:
	IMPROVE("TODO FIXME CLEANUP");
	return (-EAGAIN);
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
	bool active, atomic, nin_drv;

	lhw = HW_TO_LHW(hw);

	if (flags & ~(IEEE80211_IFACE_ITER_NORMAL|
	    IEEE80211_IFACE_ITER_RESUME_ALL|
	    IEEE80211_IFACE_SKIP_SDATA_NOT_IN_DRIVER|
	    IEEE80211_IFACE_ITER_ACTIVE|IEEE80211_IFACE_ITER__ATOMIC)) {
		ic_printf(lhw->ic, "XXX TODO %s flags(%#x) not yet supported.\n",
		    __func__, flags);
	}

	active = (flags & IEEE80211_IFACE_ITER_ACTIVE) != 0;
	atomic = (flags & IEEE80211_IFACE_ITER__ATOMIC) != 0;
	nin_drv = (flags & IEEE80211_IFACE_SKIP_SDATA_NOT_IN_DRIVER) != 0;

	if (atomic)
		LKPI_80211_LHW_LVIF_LOCK(lhw);
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
		 * If we shall skip interfaces not added to the driver do so
		 * if we haven't yet.
		 */
		if (nin_drv && !lvif->added_to_drv)
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
		LKPI_80211_LHW_LVIF_UNLOCK(lhw);
}

static void
lkpi_ieee80211_iterate_keys(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
    ieee80211_keyix keyix, struct lkpi_sta *lsta,
    void(*iterfunc)(struct ieee80211_hw *, struct ieee80211_vif *,
	struct ieee80211_sta *, struct ieee80211_key_conf *, void *),
    void *arg)
{
	if (!lsta->added_to_drv)
		return;

	if (lsta->kc[keyix] == NULL)
		return;

	iterfunc(hw, vif, LSTA_TO_STA(lsta), lsta->kc[keyix], arg);
}

void
linuxkpi_ieee80211_iterate_keys(struct ieee80211_hw *hw,
    struct ieee80211_vif *vif,
    void(*iterfunc)(struct ieee80211_hw *, struct ieee80211_vif *,
        struct ieee80211_sta *, struct ieee80211_key_conf *, void *),
    void *arg, bool rcu)
{
	struct lkpi_sta *lsta;
	struct lkpi_vif *lvif;

	lvif = VIF_TO_LVIF(vif);

	if (rcu) {
		rcu_read_lock_held();		/* XXX-BZ is this correct? */

		if (vif == NULL) {
			TODO();
		} else {
			list_for_each_entry_rcu(lsta, &lvif->lsta_list, lsta_list) {
				for (ieee80211_keyix keyix = 0; keyix < nitems(lsta->kc);
				    keyix++)
					lkpi_ieee80211_iterate_keys(hw, vif,
					    keyix, lsta, iterfunc, arg);
			}
		}
	} else {
		TODO("Used by suspend/resume; order of keys as installed to "
		"firmware is important; we'll need to rewrite some code for that");
		lockdep_assert_wiphy(hw->wiphy);

		if (vif == NULL) {
			TODO();
		} else {
			list_for_each_entry(lsta, &lvif->lsta_list, lsta_list) {
				for (ieee80211_keyix keyix = 0; keyix < nitems(lsta->kc);
				    keyix++)
					lkpi_ieee80211_iterate_keys(hw, vif,
					    keyix, lsta, iterfunc, arg);
			}
		}
	}
}

void
linuxkpi_ieee80211_iterate_chan_contexts(struct ieee80211_hw *hw,
    void(*iterfunc)(struct ieee80211_hw *, struct ieee80211_chanctx_conf *,
	void *),
    void *arg)
{
	struct lkpi_hw *lhw;
	struct lkpi_chanctx *lchanctx;

	KASSERT(hw != NULL && iterfunc != NULL,
	    ("%s: hw %p iterfunc %p arg %p\n", __func__, hw, iterfunc, arg));

	lhw = HW_TO_LHW(hw);

	rcu_read_lock();
	list_for_each_entry_rcu(lchanctx, &lhw->lchanctx_list, entry) {
		if (!lchanctx->added_to_drv)
			continue;
		iterfunc(hw, &lchanctx->chanctx_conf, arg);
	}
	rcu_read_unlock();
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

	LKPI_80211_LHW_LVIF_LOCK(lhw);
	TAILQ_FOREACH(lvif, &lhw->lvif_head, lvif_entry) {

		rcu_read_lock();
		list_for_each_entry_rcu(lsta, &lvif->lsta_list, lsta_list) {
			if (!lsta->added_to_drv)
				continue;
			sta = LSTA_TO_STA(lsta);
			iterfunc(arg, sta);
		}
		rcu_read_unlock();
	}
	LKPI_80211_LHW_LVIF_UNLOCK(lhw);
}

struct linuxkpi_ieee80211_regdomain *
lkpi_get_linuxkpi_ieee80211_regdomain(size_t n)
{
	struct linuxkpi_ieee80211_regdomain *regd;

	regd = kzalloc(sizeof(*regd) + n * sizeof(struct ieee80211_reg_rule),
	    GFP_KERNEL);
	return (regd);
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

	TRACE_SCAN(ic, "scan_flags %b info { %ju, %6D, aborted %d }",
	    lhw->scan_flags, LKPI_LHW_SCAN_BITS,
	    (uintmax_t)info->scan_start_tsf, info->tsf_bssid, ":",
	    info->aborted);

	ieee80211_scan_done(ss->ss_vap);

	LKPI_80211_LHW_SCAN_LOCK(lhw);
	free(lhw->hw_req, M_LKPI80211);
	lhw->hw_req = NULL;
	lhw->scan_flags &= ~LKPI_LHW_SCAN_RUNNING;
	/* The wakeup(lhw) will be called from lkpi_ic_scan_end(). */
	/* wakeup(lhw); */
	LKPI_80211_LHW_SCAN_UNLOCK(lhw);

	return;
}

static void
lkpi_80211_lhw_rxq_rx_one(struct lkpi_hw *lhw, struct mbuf *m)
{
	struct ieee80211_node *ni;
#ifdef LKPI_80211_USE_MTAG
	struct m_tag *mtag;
#endif
	int ok;

	ni = NULL;
#ifdef LKPI_80211_USE_MTAG
        mtag = m_tag_locate(m, MTAG_ABI_LKPI80211, LKPI80211_TAG_RXNI, NULL);
	if (mtag != NULL) {
		struct lkpi_80211_tag_rxni *rxni;

		rxni = (struct lkpi_80211_tag_rxni *)(mtag + 1);
		ni = rxni->ni;
	}
#else
	if (m->m_pkthdr.PH_loc.ptr != NULL) {
		ni = m->m_pkthdr.PH_loc.ptr;
		m->m_pkthdr.PH_loc.ptr = NULL;
	}
#endif

	if (ni != NULL) {
		ok = ieee80211_input_mimo(ni, m);
		ieee80211_free_node(ni);		/* Release the reference. */
		if (ok < 0)
			m_freem(m);
	} else {
		ok = ieee80211_input_mimo_all(lhw->ic, m);
		/* mbuf got consumed. */
	}

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE_RX)
		printf("TRACE-RX: %s: handled frame type %#0x\n", __func__, ok);
#endif
}

static void
lkpi_80211_lhw_rxq_task(void *ctx, int pending)
{
	struct lkpi_hw *lhw;
	struct mbufq mq;
	struct mbuf *m;

	lhw = ctx;

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE_RX)
		printf("TRACE-RX: %s: lhw %p pending %d mbuf_qlen %d\n",
		    __func__, lhw, pending, mbufq_len(&lhw->rxq));
#endif

	mbufq_init(&mq, IFQ_MAXLEN);

	LKPI_80211_LHW_RXQ_LOCK(lhw);
	mbufq_concat(&mq, &lhw->rxq);
	LKPI_80211_LHW_RXQ_UNLOCK(lhw);

	m = mbufq_dequeue(&mq);
	while (m != NULL) {
		lkpi_80211_lhw_rxq_rx_one(lhw, m);
		m = mbufq_dequeue(&mq);
	}
}

static void
lkpi_convert_rx_status(struct ieee80211_hw *hw, struct lkpi_sta *lsta,
    struct ieee80211_rx_status *rx_status,
    struct ieee80211_rx_stats *rx_stats,
    uint8_t *rssip)
{
	struct ieee80211_supported_band *supband;
	struct rate_info rxrate;
	int i;
	uint8_t rssi;

	memset(&rxrate, 0, sizeof(rxrate));
	memset(rx_stats, 0, sizeof(*rx_stats));
	rx_stats->r_flags = IEEE80211_R_NF | IEEE80211_R_RSSI;
	/* XXX-BZ correct hardcoded noise floor, survey data? */
	rx_stats->c_nf = -96;
	if (ieee80211_hw_check(hw, SIGNAL_DBM) &&
	    !(rx_status->flag & RX_FLAG_NO_SIGNAL_VAL))
		rssi = rx_status->signal;
	else
		rssi = rx_stats->c_nf;
	/*
	 * net80211 signal strength data are in .5 dBm units relative to
	 * the current noise floor (see comment in ieee80211_node.h).
	 */
	rssi -= rx_stats->c_nf;
	if (rssip != NULL)
		*rssip = rssi;
	rx_stats->c_rssi = rssi * 2;
	rx_stats->r_flags |= IEEE80211_R_BAND;
	rx_stats->c_band =
	    lkpi_nl80211_band_to_net80211_band(rx_status->band);
	rx_stats->r_flags |= IEEE80211_R_FREQ | IEEE80211_R_IEEE;
	rx_stats->c_freq = rx_status->freq;
	rx_stats->c_ieee = ieee80211_mhz2ieee(rx_stats->c_freq, rx_stats->c_band);

	rx_stats->c_rx_tsf = rx_status->mactime;

	/* XXX RX_FLAG_MACTIME_IS_RTAP_TS64 ? */
	if ((rx_status->flag & RX_FLAG_MACTIME) ==
	    (RX_FLAG_MACTIME_START|RX_FLAG_MACTIME_END)) {
		rx_stats->r_flags |= IEEE80211_R_TSF64;
		/* XXX RX_FLAG_MACTIME_PLCP_START ? */
		if ((rx_status->flag & RX_FLAG_MACTIME) == RX_FLAG_MACTIME_START)
			rx_stats->r_flags |= IEEE80211_R_TSF_START;
		if ((rx_status->flag & RX_FLAG_MACTIME) == RX_FLAG_MACTIME_END)
			rx_stats->r_flags |= IEEE80211_R_TSF_END;
		/* XXX-BZ if TSF_END will net80211 do the unwind of time? */
	}

	if (rx_status->chains != 0) {
		int cc;
		int8_t crssi;

		rx_stats->c_chain = rx_status->chains;
		rx_stats->r_flags |= IEEE80211_R_C_CHAIN;

		cc = 0;
		for (i = 0; i < nitems(rx_status->chain_signal); i++) {
			if (!(rx_status->chains & BIT(i)))
				continue;
			crssi = rx_status->chain_signal[i];
			crssi -= rx_stats->c_nf;
			rx_stats->c_rssi_ctl[i] = crssi * 2;
			rx_stats->c_rssi_ext[i] = crssi * 2;	/* XXX _ext ??? ATH thing? */
			/* We currently only have the global noise floor value. */
			rx_stats->c_nf_ctl[i] = rx_stats->c_nf;
			rx_stats->c_nf_ext[i] = rx_stats->c_nf;
			cc++;
		}
		if (cc > 0)
			 rx_stats->r_flags |= (IEEE80211_R_C_NF | IEEE80211_R_C_RSSI);
	}

	/* XXX-NET80211 We are not going to populate c_phytype! */

	switch (rx_status->encoding) {
	case RX_ENC_LEGACY:
	{
		uint32_t legacy = 0;

		supband = hw->wiphy->bands[rx_status->band];
		if (supband != NULL)
			legacy = supband->bitrates[rx_status->rate_idx].bitrate;
		rx_stats->c_rate = legacy;
		rxrate.legacy = legacy;
		/* Is there a LinuxKPI way of reporting IEEE80211_RX_F_CCK / _OFDM? */
		break;
	}
	case RX_ENC_HT:
		rx_stats->c_pktflags |= IEEE80211_RX_F_HT;
		rx_stats->c_rate = rx_status->rate_idx;		/* mcs */
		rxrate.flags |= RATE_INFO_FLAGS_MCS;
		rxrate.mcs = rx_status->rate_idx;
		if ((rx_status->enc_flags & RX_ENC_FLAG_SHORT_GI) != 0) {
			rx_stats->c_pktflags |= IEEE80211_RX_F_SHORTGI;
			rxrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
		}
		break;
	case RX_ENC_VHT:
		rx_stats->c_pktflags |= IEEE80211_RX_F_VHT;
		rx_stats->c_rate = rx_status->rate_idx;		/* mcs */
		rx_stats->c_vhtnss = rx_status->nss;
		rxrate.flags |= RATE_INFO_FLAGS_VHT_MCS;
		rxrate.mcs = rx_status->rate_idx;
		rxrate.nss = rx_status->nss;
		if ((rx_status->enc_flags & RX_ENC_FLAG_SHORT_GI) != 0) {
			rx_stats->c_pktflags |= IEEE80211_RX_F_SHORTGI;
			rxrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
		}
		break;
	case RX_ENC_HE:
		rxrate.flags |= RATE_INFO_FLAGS_HE_MCS;
		rxrate.mcs = rx_status->rate_idx;
		rxrate.nss = rx_status->nss;
		/* XXX TODO */
		TODO("net80211 has not matching encoding for %u", rx_status->encoding);
		break;
	case RX_ENC_EHT:
		rxrate.flags |= RATE_INFO_FLAGS_EHT_MCS;
		rxrate.mcs = rx_status->rate_idx;
		rxrate.nss = rx_status->nss;
		/* XXX TODO */
		TODO("net80211 has not matching encoding for %u", rx_status->encoding);
		break;
	}

	rxrate.bw = rx_status->bw;
	switch (rx_status->bw) {
	case RATE_INFO_BW_20:
		rx_stats->c_width = IEEE80211_RX_FW_20MHZ;
		break;
	case RATE_INFO_BW_40:
		rx_stats->c_width = IEEE80211_RX_FW_40MHZ;
		break;
	case RATE_INFO_BW_80:
		rx_stats->c_width = IEEE80211_RX_FW_80MHZ;
		break;
	case RATE_INFO_BW_160:
		rx_stats->c_width = IEEE80211_RX_FW_160MHZ;
		break;
	case RATE_INFO_BW_320:
	case RATE_INFO_BW_HE_RU:
	case RATE_INFO_BW_EHT_RU:
	case RATE_INFO_BW_5:
	case RATE_INFO_BW_10:
		TODO("net80211 has not matching bandwidth for %u", rx_status->bw);
		break;
	}

	if ((rx_status->enc_flags & RX_ENC_FLAG_LDPC) != 0)
		rx_stats->c_pktflags |= IEEE80211_RX_F_LDPC;
	if ((rx_status->enc_flags & RX_ENC_FLAG_STBC_MASK) != 0)
		 rx_stats->c_pktflags |= IEEE80211_RX_F_STBC;

	/*
	 * We only need these for LKPI_80211_HW_CRYPTO in theory but in
	 * case the hardware does something we do not expect always leave
	 * these enabled.  Leaving this commant as documentation for the || 1.
	 */
#if defined(LKPI_80211_HW_CRYPTO) || 1
	if (rx_status->flag & RX_FLAG_DECRYPTED) {
		rx_stats->c_pktflags |= IEEE80211_RX_F_DECRYPTED;
		/* Only valid if decrypted is set. */
		if (rx_status->flag & RX_FLAG_PN_VALIDATED)
			rx_stats->c_pktflags |= IEEE80211_RX_F_PN_VALIDATED;
	}
	if (rx_status->flag & RX_FLAG_IV_STRIPPED)
		rx_stats->c_pktflags |= IEEE80211_RX_F_IV_STRIP;
	if (rx_status->flag & RX_FLAG_ICV_STRIPPED)
		rx_stats->c_pktflags |= IEEE80211_RX_F_ICV_STRIP;
	if (rx_status->flag & RX_FLAG_MIC_STRIPPED)
		rx_stats->c_pktflags |= IEEE80211_RX_F_MIC_STRIP;
	if (rx_status->flag & RX_FLAG_MMIC_STRIPPED)
		rx_stats->c_pktflags |= IEEE80211_RX_F_MMIC_STRIP;
	if (rx_status->flag & RX_FLAG_MMIC_ERROR)
		rx_stats->c_pktflags |= IEEE80211_RX_F_FAIL_MMIC;
	if (rx_status->flag & RX_FLAG_FAILED_FCS_CRC)
		rx_stats->c_pktflags |= IEEE80211_RX_F_FAIL_FCSCRC;
#endif

	/* Fill in some sinfo bits to fill gaps not reported byt the driver. */
	if (lsta != NULL) {
		memcpy(&lsta->sinfo.rxrate, &rxrate, sizeof(rxrate));
		lsta->sinfo.filled |= BIT_ULL(NL80211_STA_INFO_RX_BITRATE);

		if (rx_status->chains != 0) {
			lsta->sinfo.chains = rx_status->chains;
			memcpy(lsta->sinfo.chain_signal, rx_status->chain_signal,
			    sizeof(lsta->sinfo.chain_signal));
			lsta->sinfo.filled |= BIT_ULL(NL80211_STA_INFO_CHAIN_SIGNAL);
		}
	}
}

#ifdef LINUXKPI_DEBUG_80211
static void
lkpi_rx_log_beacon(struct mbuf *m, struct lkpi_hw *lhw,
    struct ieee80211_rx_status *rx_status)
{
	struct ieee80211_mgmt *f;
	uint8_t *e;
	char ssid[IEEE80211_NWID_LEN * 4 + 1];

	memset(ssid, '\0', sizeof(ssid));

	f = mtod(m, struct ieee80211_mgmt *);
	e = f->u.beacon.variable;
	/*
	 * Usually SSID is right after the fixed part and for debugging we will
	 * be fine should we miss it if it is not.
	 */
	while ((e - (uint8_t *)f) < m->m_len) {
		if (*e == IEEE80211_ELEMID_SSID)
			break;
		e += (2 + *(e + 1));
	}
	if (*e == IEEE80211_ELEMID_SSID) {
		int i, len;
		char *p;

		p = ssid;
		len = m->m_len - ((e + 2) - (uint8_t *)f);
		if (len > *(e + 1))
			len = *(e + 1);
		e += 2;
		for (i = 0; i < len; i++) {
			/* Printable character? */
			if (*e >= 0x20 && *e < 0x7f) {
				*p++ = *e++;
			} else {
				snprintf(p, 5, "%#04x", *e++);
				p += 4;
			}
		}
		*p = '\0';
	}

	/* We print skb, skb->data, m as we are seeing 'ghost beacons'. */
	TRACE_SCAN_BEACON(lhw->ic, "Beacon: scan_flags %b, band %s freq %u chan %-4d "
	    "len %d { %#06x %#06x %6D %6D %6D %#06x %ju %u %#06x SSID '%s' }",
	    lhw->scan_flags, LKPI_LHW_SCAN_BITS,
	    lkpi_nl80211_band_name(rx_status->band), rx_status->freq,
	    linuxkpi_ieee80211_frequency_to_channel(rx_status->freq, 0),
	    m->m_pkthdr.len, f->frame_control, f->duration_id,
	    f->da, ":", f->sa, ":", f->bssid, ":", f->seq_ctrl,
	    (uintmax_t)le64_to_cpu(f->u.beacon.timestamp),
	    le16_to_cpu(f->u.beacon.beacon_int),
	    le16_to_cpu(f->u.beacon.capab_info), ssid);
}
#endif

/* For %list see comment towards the end of the function. */
void
linuxkpi_ieee80211_rx(struct ieee80211_hw *hw, struct sk_buff *skb,
    struct ieee80211_sta *sta, struct napi_struct *napi __unused,
    struct list_head *list __unused)
{
	struct lkpi_hw *lhw;
	struct ieee80211com *ic;
	struct mbuf *m;
	struct skb_shared_info *shinfo;
	struct ieee80211_rx_status *rx_status;
	struct ieee80211_rx_stats rx_stats;
	struct ieee80211_node *ni;
	struct ieee80211vap *vap;
	struct ieee80211_hdr *hdr;
	struct lkpi_sta *lsta;
	int i, offset, ok, error;
	uint8_t rssi;
	bool is_beacon;

	lhw = HW_TO_LHW(hw);
	ic = lhw->ic;

	if (skb->len < 2) {
		/* Need 80211 stats here. */
		counter_u64_add(ic->ic_ierrors, 1);
		IMPROVE();
		goto err;
	}

	/*
	 * For now do the data copy; we can later improve things. Might even
	 * have an mbuf backing the skb data then?
	 */
	m = m_get3(skb->len, M_NOWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL) {
		counter_u64_add(ic->ic_ierrors, 1);
		goto err;
	}
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

	hdr = (void *)skb->data;
	is_beacon = ieee80211_is_beacon(hdr->frame_control);

#ifdef LINUXKPI_DEBUG_80211
	/*
	 * We use the mbuf here as otherwise the variable part might
	 * be in skb frags.
	 */
	if (is_beacon && ((linuxkpi_debug_80211 & D80211_SCAN_BEACON) != 0))
		lkpi_rx_log_beacon(m, lhw, rx_status);

	if (is_beacon && (linuxkpi_debug_80211 & D80211_TRACE_RX_BEACONS) == 0 &&
	   (linuxkpi_debug_80211 & D80211_SCAN_BEACON) == 0)
		goto no_trace_beacons;

	if (linuxkpi_debug_80211 & D80211_TRACE_RX)
		printf("TRACE-RX: %s: skb %p l/d/t-len (%u/%u/%u) "
		    "h %p d %p t %p e %p sh %p (%u) m %p plen %u len %u%s\n",
		    __func__, skb, skb->len, skb->data_len,
		    skb->truesize, skb->head, skb->data, skb->tail, skb->end,
		    shinfo, shinfo->nr_frags,
		    m, m->m_pkthdr.len, m->m_len, is_beacon ? " beacon" : "");

	if (linuxkpi_debug_80211 & D80211_TRACE_RX_DUMP)
		hexdump(mtod(m, const void *), m->m_len, "RX (raw) ", 0);

	/* Implement a dump_rxcb() !!! */
	if ((linuxkpi_debug_80211 & D80211_TRACE_RX) != 0 ||
	    (linuxkpi_debug_80211 & D80211_SCAN_BEACON) != 0)
		printf("TRACE-RX: %s: RXCB: %ju %ju %u, %b, %u, %#0x, %#0x, "
		    "%u band %u, %u { %d %d %d %d }, %d, %#x %#x %#x %#x %u %u %u\n",
			__func__,
			(uintmax_t)rx_status->boottime_ns,
			(uintmax_t)rx_status->mactime,
			rx_status->device_timestamp,
			rx_status->flag, IEEE80211_RX_STATUS_FLAGS_BITS,
			rx_status->freq,
			rx_status->bw,
			rx_status->encoding,
			rx_status->ampdu_reference,
			rx_status->band,
			rx_status->chains,
			rx_status->chain_signal[0],
			rx_status->chain_signal[1],
			rx_status->chain_signal[2],
			rx_status->chain_signal[3],
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

	lsta = NULL;
	if (sta != NULL) {
		lsta = STA_TO_LSTA(sta);
		ni = ieee80211_ref_node(lsta->ni);
	} else {
		struct ieee80211_frame_min *wh;

		wh = mtod(m, struct ieee80211_frame_min *);
		ni = ieee80211_find_rxnode(ic, wh);
		if (ni != NULL)
			lsta = ni->ni_drv_data;
	}

	rssi = 0;
	lkpi_convert_rx_status(hw, lsta, rx_status, &rx_stats, &rssi);

	ok = ieee80211_add_rx_params(m, &rx_stats);
	if (ok == 0) {
		m_freem(m);
		counter_u64_add(ic->ic_ierrors, 1);
		goto err;
	}

	if (ni != NULL)
		vap = ni->ni_vap;
	else
		/*
		 * XXX-BZ can we improve this by looking at the frame hdr
		 * or other meta-data passed up?
		 */
		vap = TAILQ_FIRST(&ic->ic_vaps);

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE_RX)
		printf("TRACE-RX: %s: sta %p lsta %p state %d ni %p vap %p%s\n",
		    __func__, sta, lsta, (lsta != NULL) ? lsta->state : -1,
		    ni, vap, is_beacon ? " beacon" : "");
#endif

	if (ni != NULL && vap != NULL && is_beacon &&
	    rx_status->device_timestamp > 0 &&
	    m->m_pkthdr.len >= sizeof(struct ieee80211_frame)) {
		struct lkpi_vif *lvif;
		struct ieee80211_vif *vif;
		struct ieee80211_frame *wh;

		wh = mtod(m, struct ieee80211_frame *);
		if (!IEEE80211_ADDR_EQ(wh->i_addr2, ni->ni_bssid))
			goto skip_device_ts;

		lvif = VAP_TO_LVIF(vap);
		vif = LVIF_TO_VIF(lvif);

		IMPROVE("TIMING_BEACON_ONLY?");
		/* mac80211 specific (not net80211) so keep it here. */
		vif->bss_conf.sync_device_ts = rx_status->device_timestamp;
		/*
		 * net80211 should take care of the other information (sync_tsf,
		 * sync_dtim_count) as otherwise we need to parse the beacon.
		 */
skip_device_ts:
		;
	}

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
		rtap->wr_dbm_antsignal = rssi;
		rtap->wr_dbm_antnoise = rx_stats.c_nf;
	}

	if (ieee80211_hw_check(hw, RX_INCLUDES_FCS))
		m_adj(m, -IEEE80211_CRC_LEN);

#if 0
	if (list != NULL) {
		/*
		* Normally this would be queued up and delivered by
		* netif_receive_skb_list(), napi_gro_receive(), or the like.
		* See mt76::mac80211.c as only current possible consumer.
		*/
		IMPROVE("we simply pass the packet to net80211 to deal with.");
	}
#endif

	/* Attach meta-information to the mbuf for the deferred RX path. */
	if (ni != NULL) {
#ifdef LKPI_80211_USE_MTAG
		struct m_tag *mtag;
		struct lkpi_80211_tag_rxni *rxni;

		mtag = m_tag_alloc(MTAG_ABI_LKPI80211, LKPI80211_TAG_RXNI,
		    sizeof(*rxni), IEEE80211_M_NOWAIT);
		if (mtag == NULL) {
			m_freem(m);
			counter_u64_add(ic->ic_ierrors, 1);
			goto err;
		}
		rxni = (struct lkpi_80211_tag_rxni *)(mtag + 1);
		rxni->ni = ni;		/* We hold a reference. */
		m_tag_prepend(m, mtag);
#else
		m->m_pkthdr.PH_loc.ptr = ni;	/* We hold a reference. */
#endif
	}

	LKPI_80211_LHW_RXQ_LOCK(lhw);
	if (lhw->rxq_stopped) {
		LKPI_80211_LHW_RXQ_UNLOCK(lhw);
		m_freem(m);
		counter_u64_add(ic->ic_ierrors, 1);
		goto err;
	}

	error = mbufq_enqueue(&lhw->rxq, m);
	if (error != 0) {
		LKPI_80211_LHW_RXQ_UNLOCK(lhw);
		m_freem(m);
		counter_u64_add(ic->ic_ierrors, 1);
#ifdef LINUXKPI_DEBUG_80211
		if (linuxkpi_debug_80211 & D80211_TRACE_RX)
			ic_printf(ni->ni_ic, "%s: mbufq_enqueue failed: %d\n",
			    __func__, error);
#endif
		goto err;
	}
	taskqueue_enqueue(taskqueue_thread, &lhw->rxq_task);
	LKPI_80211_LHW_RXQ_UNLOCK(lhw);

	IMPROVE();

err:
	/* The skb is ours so we can free it :-) */
	kfree_skb(skb);
}

uint8_t
linuxkpi_ieee80211_get_tid(struct ieee80211_hdr *hdr, bool nonqos_ok)
{
	const struct ieee80211_frame *wh;
	uint8_t tid;

	/* Linux seems to assume this is a QOS-Data-Frame */
	KASSERT(nonqos_ok || ieee80211_is_data_qos(hdr->frame_control),
	   ("%s: hdr %p fc %#06x not qos_data\n", __func__, hdr,
	   hdr->frame_control));

	wh = (const struct ieee80211_frame *)hdr;
	tid = ieee80211_gettid(wh);
	KASSERT(nonqos_ok || tid == (tid & IEEE80211_QOS_TID), ("%s: tid %u "
	   "not expected (%u?)\n", __func__, tid, IEEE80211_NONQOS_TID));

	return (tid);
}

/* -------------------------------------------------------------------------- */

static void
lkpi_wiphy_work(struct work_struct *work)
{
	struct lkpi_wiphy *lwiphy;
	struct wiphy *wiphy;
	struct wiphy_work *wk;

	lwiphy = container_of(work, struct lkpi_wiphy, wwk);
	wiphy = LWIPHY_TO_WIPHY(lwiphy);

	wiphy_lock(wiphy);

	LKPI_80211_LWIPHY_WORK_LOCK(lwiphy);
	wk = list_first_entry_or_null(&lwiphy->wwk_list, struct wiphy_work, entry);
	/* If there is nothing we do nothing. */
	if (wk == NULL) {
		LKPI_80211_LWIPHY_WORK_UNLOCK(lwiphy);
		wiphy_unlock(wiphy);
		return;
	}
	list_del_init(&wk->entry);

	/* More work to do? */
	if (!list_empty(&lwiphy->wwk_list))
		schedule_work(work);
	LKPI_80211_LWIPHY_WORK_UNLOCK(lwiphy);

	/* Finally call the (*wiphy_work_fn)() function. */
	wk->fn(wiphy, wk);

	wiphy_unlock(wiphy);
}

void
linuxkpi_wiphy_work_queue(struct wiphy *wiphy, struct wiphy_work *wwk)
{
	struct lkpi_wiphy *lwiphy;

	lwiphy = WIPHY_TO_LWIPHY(wiphy);

	LKPI_80211_LWIPHY_WORK_LOCK(lwiphy);
	/* Do not double-queue. */
	if (list_empty(&wwk->entry))
		list_add_tail(&wwk->entry, &lwiphy->wwk_list);
	LKPI_80211_LWIPHY_WORK_UNLOCK(lwiphy);

	/*
	 * See how ieee80211_queue_work() work continues in Linux or if things
	 * migrate here over time?
	 * Use a system queue from linux/workqueue.h for now.
	 */
	queue_work(system_wq, &lwiphy->wwk);
}

void
linuxkpi_wiphy_work_cancel(struct wiphy *wiphy, struct wiphy_work *wwk)
{
	struct lkpi_wiphy *lwiphy;

	lwiphy = WIPHY_TO_LWIPHY(wiphy);

	LKPI_80211_LWIPHY_WORK_LOCK(lwiphy);
	/* Only cancel if queued. */
	if (!list_empty(&wwk->entry))
		list_del_init(&wwk->entry);
	LKPI_80211_LWIPHY_WORK_UNLOCK(lwiphy);
}

void
linuxkpi_wiphy_work_flush(struct wiphy *wiphy, struct wiphy_work *wwk)
{
	struct lkpi_wiphy *lwiphy;
	struct wiphy_work *wk;

	lwiphy = WIPHY_TO_LWIPHY(wiphy);
	LKPI_80211_LWIPHY_WORK_LOCK(lwiphy);
	/* If wwk is unset, flush everything; called when wiphy is shut down. */
	if (wwk != NULL && list_empty(&wwk->entry)) {
		LKPI_80211_LWIPHY_WORK_UNLOCK(lwiphy);
		return;
	}

	while (!list_empty(&lwiphy->wwk_list)) {

		wk = list_first_entry(&lwiphy->wwk_list, struct wiphy_work,
		    entry);
		list_del_init(&wk->entry);
		LKPI_80211_LWIPHY_WORK_UNLOCK(lwiphy);
		wk->fn(wiphy, wk);
		LKPI_80211_LWIPHY_WORK_LOCK(lwiphy);
		if (wk == wwk)
			break;
	}
	LKPI_80211_LWIPHY_WORK_UNLOCK(lwiphy);
}

void
lkpi_wiphy_delayed_work_timer(struct timer_list *tl)
{
	struct wiphy_delayed_work *wdwk;

	wdwk = timer_container_of(wdwk, tl, timer);
	wiphy_work_queue(wdwk->wiphy, &wdwk->work);
}

void
linuxkpi_wiphy_delayed_work_queue(struct wiphy *wiphy,
    struct wiphy_delayed_work *wdwk, unsigned long delay)
{
	if (delay == 0) {
		/* Run right away. */
		del_timer(&wdwk->timer);
		wiphy_work_queue(wiphy, &wdwk->work);
	} else {
		wdwk->wiphy = wiphy;
		mod_timer(&wdwk->timer, jiffies + delay);
	}
}

void
linuxkpi_wiphy_delayed_work_cancel(struct wiphy *wiphy,
    struct wiphy_delayed_work *wdwk)
{
	del_timer_sync(&wdwk->timer);
	wiphy_work_cancel(wiphy, &wdwk->work);
}

void
linuxkpi_wiphy_delayed_work_flush(struct wiphy *wiphy,
    struct wiphy_delayed_work *wdwk)
{
	lockdep_assert_held(&wiphy->mtx);

	del_timer_sync(&wdwk->timer);
	wiphy_work_flush(wiphy, &wdwk->work);
}

/* -------------------------------------------------------------------------- */

struct wiphy *
linuxkpi_wiphy_new(const struct cfg80211_ops *ops, size_t priv_len)
{
	struct lkpi_wiphy *lwiphy;
	struct wiphy *wiphy;

	lwiphy = kzalloc(sizeof(*lwiphy) + priv_len, GFP_KERNEL);
	if (lwiphy == NULL)
		return (NULL);
	lwiphy->ops = ops;

	LKPI_80211_LWIPHY_WORK_LOCK_INIT(lwiphy);
	INIT_LIST_HEAD(&lwiphy->wwk_list);
	INIT_WORK(&lwiphy->wwk, lkpi_wiphy_work);

	wiphy = LWIPHY_TO_WIPHY(lwiphy);

	mutex_init(&wiphy->mtx);
	TODO();

	return (wiphy);
}

void
linuxkpi_wiphy_free(struct wiphy *wiphy)
{
	struct lkpi_wiphy *lwiphy;

	if (wiphy == NULL)
		return;

	linuxkpi_wiphy_work_flush(wiphy, NULL);
	mutex_destroy(&wiphy->mtx);

	lwiphy = WIPHY_TO_LWIPHY(wiphy);
	LKPI_80211_LWIPHY_WORK_LOCK_DESTROY(lwiphy);

	kfree(lwiphy);
}

static uint32_t
lkpi_cfg80211_calculate_bitrate_ht(struct rate_info *rate)
{
	TODO("cfg80211_calculate_bitrate_ht");
	return (rate->legacy);
}

static uint32_t
lkpi_cfg80211_calculate_bitrate_vht(struct rate_info *rate)
{
	TODO("cfg80211_calculate_bitrate_vht");
	return (rate->legacy);
}

uint32_t
linuxkpi_cfg80211_calculate_bitrate(struct rate_info *rate)
{

	/* Beware: order! */
	if (rate->flags & RATE_INFO_FLAGS_MCS)
		return (lkpi_cfg80211_calculate_bitrate_ht(rate));

	if (rate->flags & RATE_INFO_FLAGS_VHT_MCS)
		return (lkpi_cfg80211_calculate_bitrate_vht(rate));

	IMPROVE("HE/EHT/...");

	return (rate->legacy);
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

#if 0
static struct lkpi_sta *
lkpi_find_lsta_by_ni(struct lkpi_vif *lvif, struct ieee80211_node *ni)
{
	struct lkpi_sta *lsta, *temp;

	rcu_read_lock();
	list_for_each_entry_rcu(lsta, &lvif->lsta_list, lsta_list) {
		if (lsta->ni == ni) {
			rcu_read_unlock();
			return (lsta);
		}
	}
	rcu_read_unlock();

	return (NULL);
}
#endif

struct ieee80211_sta *
linuxkpi_ieee80211_find_sta(struct ieee80211_vif *vif, const u8 *peer)
{
	struct lkpi_vif *lvif;
	struct lkpi_sta *lsta;
	struct ieee80211_sta *sta;

	lvif = VIF_TO_LVIF(vif);

	rcu_read_lock();
	list_for_each_entry_rcu(lsta, &lvif->lsta_list, lsta_list) {
		sta = LSTA_TO_STA(lsta);
		if (IEEE80211_ADDR_EQ(sta->addr, peer)) {
			rcu_read_unlock();
			return (sta);
		}
	}
	rcu_read_unlock();
	return (NULL);
}

struct ieee80211_sta *
linuxkpi_ieee80211_find_sta_by_ifaddr(struct ieee80211_hw *hw,
    const uint8_t *addr, const uint8_t *ourvifaddr)
{
	struct lkpi_hw *lhw;
	struct lkpi_vif *lvif;
	struct lkpi_sta *lsta;
	struct ieee80211_vif *vif;
	struct ieee80211_sta *sta;

	lhw = wiphy_priv(hw->wiphy);
	sta = NULL;

	LKPI_80211_LHW_LVIF_LOCK(lhw);
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
	LKPI_80211_LHW_LVIF_UNLOCK(lhw);

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
	struct lkpi_vif *lvif;
	struct sk_buff *skb;

	IMPROVE("wiphy_lock? or assert?");
	skb = NULL;
	ltxq = TXQ_TO_LTXQ(txq);
	ltxq->seen_dequeue = true;

	if (ltxq->stopped)
		goto stopped;

	lvif = VIF_TO_LVIF(ltxq->txq.vif);
	if (lvif->hw_queue_stopped[ltxq->txq.ac]) {
		ltxq->stopped = true;
		goto stopped;
	}

	IMPROVE("hw(TX_FRAG_LIST)");

	LKPI_80211_LTXQ_LOCK(ltxq);
	skb = skb_dequeue(&ltxq->skbq);
	LKPI_80211_LTXQ_UNLOCK(ltxq);

stopped:
	return (skb);
}

void
linuxkpi_ieee80211_txq_get_depth(struct ieee80211_txq *txq,
    unsigned long *frame_cnt, unsigned long *byte_cnt)
{
	struct lkpi_txq *ltxq;
	struct sk_buff *skb;
	unsigned long fc, bc;

	ltxq = TXQ_TO_LTXQ(txq);

	fc = bc = 0;
	LKPI_80211_LTXQ_LOCK(ltxq);
	skb_queue_walk(&ltxq->skbq, skb) {
		fc++;
		bc += skb->len;
	}
	LKPI_80211_LTXQ_UNLOCK(ltxq);
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
static void
_lkpi_ieee80211_free_txskb(struct ieee80211_hw *hw, struct sk_buff *skb,
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
}

void
linuxkpi_ieee80211_free_txskb(struct ieee80211_hw *hw, struct sk_buff *skb,
    int status)
{

	_lkpi_ieee80211_free_txskb(hw, skb, status);
	kfree_skb(skb);
}

void
linuxkpi_ieee80211_tx_status_ext(struct ieee80211_hw *hw,
    struct ieee80211_tx_status *txstat)
{
	struct sk_buff *skb;
	struct ieee80211_tx_info *info;
	struct ieee80211_ratectl_tx_status txs;
	struct ieee80211_node *ni;
	int status;

	skb = txstat->skb;
	if (skb->m != NULL) {
		struct mbuf *m;

		m = skb->m;
		ni = m->m_pkthdr.PH_loc.ptr;
		memset(&txs, 0, sizeof(txs));
	} else {
		ni = NULL;
	}

	info = txstat->info;
	if (info->flags & IEEE80211_TX_STAT_ACK) {
		status = 0;	/* No error. */
		txs.status = IEEE80211_RATECTL_TX_SUCCESS;
	} else {
		status = 1;
		txs.status = IEEE80211_RATECTL_TX_FAIL_UNSPECIFIED;
	}

	if (ni != NULL) {
		txs.pktlen = skb->len;
		txs.flags |= IEEE80211_RATECTL_STATUS_PKTLEN;
		if (info->status.rates[0].count > 1) {
			txs.long_retries = info->status.rates[0].count - 1;	/* 1 + retries in drivers. */
			txs.flags |= IEEE80211_RATECTL_STATUS_LONG_RETRY;
		}
#if 0		/* Unused in net80211 currently. */
		/* XXX-BZ convert check .flags for MCS/VHT/.. */
		txs.final_rate = info->status.rates[0].idx;
		txs.flags |= IEEE80211_RATECTL_STATUS_FINAL_RATE;
#endif
		if (info->status.flags & IEEE80211_TX_STATUS_ACK_SIGNAL_VALID) {
			txs.rssi = info->status.ack_signal;		/* XXX-BZ CONVERT? */
			txs.flags |= IEEE80211_RATECTL_STATUS_RSSI;
		}

		IMPROVE("only update rate if needed but that requires us to get a proper rate from mo_sta_statistics");
		ieee80211_ratectl_tx_complete(ni, &txs);
		ieee80211_ratectl_rate(ni->ni_vap->iv_bss, NULL, 0);

#ifdef LINUXKPI_DEBUG_80211
		if (linuxkpi_debug_80211 & D80211_TRACE_TX) {
			printf("TX-RATE: %s: long_retries %d\n", __func__,
			    txs.long_retries);
		}
#endif
	}

#ifdef LINUXKPI_DEBUG_80211
	if (linuxkpi_debug_80211 & D80211_TRACE_TX)
		printf("TX-STATUS: %s: hw %p skb %p status %d : flags %#x "
		    "band %u hw_queue %u tx_time_est %d : "
		    "rates [ %u %u %#x, %u %u %#x, %u %u %#x, %u %u %#x ] "
		    "ack_signal %u ampdu_ack_len %u ampdu_len %u antenna %u "
		    "tx_time %u flags %#x "
		    "status_driver_data [ %p %p ]\n",
		    __func__, hw, skb, status, info->flags,
		    info->band, info->hw_queue, info->tx_time_est,
		    info->status.rates[0].idx, info->status.rates[0].count,
		    info->status.rates[0].flags,
		    info->status.rates[1].idx, info->status.rates[1].count,
		    info->status.rates[1].flags,
		    info->status.rates[2].idx, info->status.rates[2].count,
		    info->status.rates[2].flags,
		    info->status.rates[3].idx, info->status.rates[3].count,
		    info->status.rates[3].flags,
		    info->status.ack_signal, info->status.ampdu_ack_len,
		    info->status.ampdu_len, info->status.antenna,
		    info->status.tx_time, info->status.flags,
		    info->status.status_driver_data[0],
		    info->status.status_driver_data[1]);
#endif

	if (txstat->free_list) {
		_lkpi_ieee80211_free_txskb(hw, skb, status);
		list_add_tail(&skb->list, txstat->free_list);
	} else {
		linuxkpi_ieee80211_free_txskb(hw, skb, status);
	}
}

void
linuxkpi_ieee80211_tx_status(struct ieee80211_hw *hw, struct sk_buff *skb)
{
	struct ieee80211_tx_status status;

	memset(&status, 0, sizeof(status));
	status.info = IEEE80211_SKB_CB(skb);
	status.skb = skb;
	/* sta, n_rates, rates, free_list? */

	ieee80211_tx_status_ext(hw, &status);
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
linuxkpi_ieee80211_probereq_get(struct ieee80211_hw *hw, const uint8_t *addr,
    const uint8_t *ssid, size_t ssid_len, size_t tailroom)
{
	struct sk_buff *skb;
	struct ieee80211_frame *wh;
	uint8_t *p;
	size_t len;

	len = sizeof(*wh);
	len += 2 + ssid_len;

	skb = dev_alloc_skb(hw->extra_tx_headroom + len + tailroom);
	if (skb == NULL)
		return (NULL);

	skb_reserve(skb, hw->extra_tx_headroom);

	wh = skb_put_zero(skb, sizeof(*wh));
	wh->i_fc[0] = IEEE80211_FC0_VERSION_0;
	wh->i_fc[0] |= IEEE80211_FC0_SUBTYPE_PROBE_REQ | IEEE80211_FC0_TYPE_MGT;
	IEEE80211_ADDR_COPY(wh->i_addr1, ieee80211broadcastaddr);
	IEEE80211_ADDR_COPY(wh->i_addr2, addr);
	IEEE80211_ADDR_COPY(wh->i_addr3, ieee80211broadcastaddr);

	p = skb_put(skb, 2 + ssid_len);
	*p++ = IEEE80211_ELEMID_SSID;
	*p++ = ssid_len;
	if (ssid_len > 0)
		memcpy(p, ssid, ssid_len);

	return (skb);
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
	v = htole16(vif->cfg.aid | 1<<15 | 1<<16);
	memcpy(&psp->i_aid, &v, sizeof(v));
	IEEE80211_ADDR_COPY(psp->i_bssid, vap->iv_bss->ni_macaddr);
	IEEE80211_ADDR_COPY(psp->i_ta, vif->addr);

	return (skb);
}

struct sk_buff *
linuxkpi_ieee80211_nullfunc_get(struct ieee80211_hw *hw,
    struct ieee80211_vif *vif, int linkid, bool qos)
{
	struct lkpi_vif *lvif;
	struct ieee80211vap *vap;
	struct sk_buff *skb;
	struct ieee80211_frame *nullf;

	IMPROVE("linkid");

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
	 * Go to init; otherwise we need to elaborately check state and
	 * handle accordingly, e.g., if in RUN we could call iv_bmiss.
	 * Let the statemachine handle all neccessary changes.
	 */
	nstate = IEEE80211_S_INIT;
	arg = 0;	/* Not a valid reason. */

	ic_printf(vap->iv_ic, "%s: vif %p vap %p state %s (synched %d, assoc %d "
	    "beacons %d dtim_period %d)\n", __func__, vif, vap,
	    ieee80211_state_name[vap->iv_state],
	    lvif->lvif_bss_synched, vif->cfg.assoc, lvif->beacons,
	    vif->bss_conf.dtim_period);
	ieee80211_new_state(vap, nstate, arg);
}

void
linuxkpi_ieee80211_beacon_loss(struct ieee80211_vif *vif)
{
	struct lkpi_vif *lvif;
	struct ieee80211vap *vap;

	lvif = VIF_TO_LVIF(vif);
	vap = LVIF_TO_VAP(lvif);

	ic_printf(vap->iv_ic, "%s: vif %p vap %p state %s (synched %d, assoc %d "
	    "beacons %d dtim_period %d)\n", __func__, vif, vap,
	    ieee80211_state_name[vap->iv_state],
	    lvif->lvif_bss_synched, vif->cfg.assoc, lvif->beacons,
	    vif->bss_conf.dtim_period);
	ieee80211_beacon_miss(vap->iv_ic);
}

/* -------------------------------------------------------------------------- */

void
linuxkpi_ieee80211_stop_queue(struct ieee80211_hw *hw, int qnum)
{
	struct lkpi_hw *lhw;
	struct lkpi_vif *lvif;
	struct ieee80211_vif *vif;
	int ac_count, ac;

	KASSERT(qnum < hw->queues, ("%s: qnum %d >= hw->queues %d, hw %p\n",
	    __func__, qnum, hw->queues, hw));

	lhw = wiphy_priv(hw->wiphy);

	/* See lkpi_ic_vap_create(). */
	if (hw->queues >= IEEE80211_NUM_ACS)
		ac_count = IEEE80211_NUM_ACS;
	else
		ac_count = 1;

	LKPI_80211_LHW_LVIF_LOCK(lhw);
	TAILQ_FOREACH(lvif, &lhw->lvif_head, lvif_entry) {

		vif = LVIF_TO_VIF(lvif);
		for (ac = 0; ac < ac_count; ac++) {
			IMPROVE_TXQ("LOCKING");
			if (qnum == vif->hw_queue[ac]) {
#ifdef LINUXKPI_DEBUG_80211
				/*
				 * For now log this to better understand
				 * how this is supposed to work.
				 */
				if (lvif->hw_queue_stopped[ac] &&
				    (linuxkpi_debug_80211 & D80211_IMPROVE_TXQ) != 0)
					ic_printf(lhw->ic, "%s:%d: lhw %p hw %p "
					    "lvif %p vif %p ac %d qnum %d already "
					    "stopped\n", __func__, __LINE__,
					    lhw, hw, lvif, vif, ac, qnum);
#endif
				lvif->hw_queue_stopped[ac] = true;
			}
		}
	}
	LKPI_80211_LHW_LVIF_UNLOCK(lhw);
}

void
linuxkpi_ieee80211_stop_queues(struct ieee80211_hw *hw)
{
	int i;

	IMPROVE_TXQ("Locking; do we need further info?");
	for (i = 0; i < hw->queues; i++)
		linuxkpi_ieee80211_stop_queue(hw, i);
}


static void
lkpi_ieee80211_wake_queues(struct ieee80211_hw *hw, int hwq)
{
	struct lkpi_hw *lhw;
	struct lkpi_vif *lvif;
	struct lkpi_sta *lsta;
	int ac_count, ac, tid;

	/* See lkpi_ic_vap_create(). */
	if (hw->queues >= IEEE80211_NUM_ACS)
		ac_count = IEEE80211_NUM_ACS;
	else
		ac_count = 1;

	lhw = wiphy_priv(hw->wiphy);

	IMPROVE_TXQ("Locking");
	LKPI_80211_LHW_LVIF_LOCK(lhw);
	TAILQ_FOREACH(lvif, &lhw->lvif_head, lvif_entry) {
		struct ieee80211_vif *vif;

		vif = LVIF_TO_VIF(lvif);
		for (ac = 0; ac < ac_count; ac++) {

			if (hwq == vif->hw_queue[ac]) {

				/* XXX-BZ what about software scan? */

#ifdef LINUXKPI_DEBUG_80211
				/*
				 * For now log this to better understand
				 * how this is supposed to work.
				 */
				if (!lvif->hw_queue_stopped[ac] &&
				    (linuxkpi_debug_80211 & D80211_IMPROVE_TXQ) != 0)
					ic_printf(lhw->ic, "%s:%d: lhw %p hw %p "
					    "lvif %p vif %p ac %d hw_q not stopped\n",
					    __func__, __LINE__,
					    lhw, hw, lvif, vif, ac);
#endif
				lvif->hw_queue_stopped[ac] = false;

				rcu_read_lock();
				list_for_each_entry_rcu(lsta, &lvif->lsta_list, lsta_list) {
					struct ieee80211_sta *sta;

					sta = LSTA_TO_STA(lsta);
					for (tid = 0; tid < nitems(sta->txq); tid++) {
						struct lkpi_txq *ltxq;

						if (sta->txq[tid] == NULL)
							continue;

						if (sta->txq[tid]->ac != ac)
							continue;

						ltxq = TXQ_TO_LTXQ(sta->txq[tid]);
						if (!ltxq->stopped)
							continue;

						ltxq->stopped = false;

						if (!skb_queue_empty(&ltxq->skbq))
							lkpi_80211_mo_wake_tx_queue(hw, sta->txq[tid]);
					}
				}
				rcu_read_unlock();
			}
		}
	}
	LKPI_80211_LHW_LVIF_UNLOCK(lhw);
}

static void
lkpi_ieee80211_wake_queues_locked(struct ieee80211_hw *hw)
{
	int i;

	IMPROVE_TXQ("Is this all/enough here?");
	for (i = 0; i < hw->queues; i++)
		lkpi_ieee80211_wake_queues(hw, i);
}

void
linuxkpi_ieee80211_wake_queues(struct ieee80211_hw *hw)
{
	struct lkpi_hw *lhw;
	unsigned long flags;

	lhw = HW_TO_LHW(hw);

	spin_lock_irqsave(&lhw->txq_lock, flags);
	lkpi_ieee80211_wake_queues_locked(hw);
	spin_unlock_irqrestore(&lhw->txq_lock, flags);
}

void
linuxkpi_ieee80211_wake_queue(struct ieee80211_hw *hw, int qnum)
{
	struct lkpi_hw *lhw;
	unsigned long flags;

	KASSERT(qnum < hw->queues, ("%s: qnum %d >= hw->queues %d, hw %p\n",
	    __func__, qnum, hw->queues, hw));

	lhw = HW_TO_LHW(hw);

	spin_lock_irqsave(&lhw->txq_lock, flags);
	lkpi_ieee80211_wake_queues(hw, qnum);
	spin_unlock_irqrestore(&lhw->txq_lock, flags);
}

/* This is just hardware queues. */
void
linuxkpi_ieee80211_txq_schedule_start(struct ieee80211_hw *hw, uint8_t ac)
{
	struct lkpi_hw *lhw;

	lhw = HW_TO_LHW(hw);

	IMPROVE_TXQ("Are there reasons why we wouldn't schedule?");
	IMPROVE_TXQ("LOCKING");
	if (++lhw->txq_generation[ac] == 0)
		lhw->txq_generation[ac]++;
}

struct ieee80211_txq *
linuxkpi_ieee80211_next_txq(struct ieee80211_hw *hw, uint8_t ac)
{
	struct lkpi_hw *lhw;
	struct ieee80211_txq *txq;
	struct lkpi_txq *ltxq;

	lhw = HW_TO_LHW(hw);
	txq = NULL;

	IMPROVE_TXQ("LOCKING");

	/* Check that we are scheduled. */
	if (lhw->txq_generation[ac] == 0)
		goto out;

	ltxq = TAILQ_FIRST(&lhw->scheduled_txqs[ac]);
	if (ltxq == NULL)
		goto out;
	if (ltxq->txq_generation == lhw->txq_generation[ac])
		goto out;

	ltxq->txq_generation = lhw->txq_generation[ac];
	TAILQ_REMOVE(&lhw->scheduled_txqs[ac], ltxq, txq_entry);
	txq = &ltxq->txq;
	TAILQ_ELEM_INIT(ltxq, txq_entry);

out:
	return (txq);
}

void linuxkpi_ieee80211_schedule_txq(struct ieee80211_hw *hw,
    struct ieee80211_txq *txq, bool withoutpkts)
{
	struct lkpi_hw *lhw;
	struct lkpi_txq *ltxq;
	bool ltxq_empty;

	ltxq = TXQ_TO_LTXQ(txq);

	IMPROVE_TXQ("LOCKING");

	/* Only schedule if work to do or asked to anyway. */
	LKPI_80211_LTXQ_LOCK(ltxq);
	ltxq_empty = skb_queue_empty(&ltxq->skbq);
	LKPI_80211_LTXQ_UNLOCK(ltxq);
	if (!withoutpkts && ltxq_empty)
		goto out;

	/*
	 * Make sure we do not double-schedule. We do this by checking tqe_prev,
	 * the previous entry in our tailq. tqe_prev is always valid if this entry
	 * is queued, tqe_next may be NULL if this is the only element in the list.
	 */
	if (ltxq->txq_entry.tqe_prev != NULL)
		goto out;

	lhw = HW_TO_LHW(hw);
	TAILQ_INSERT_TAIL(&lhw->scheduled_txqs[txq->ac], ltxq, txq_entry);
out:
	return;
}

void
linuxkpi_ieee80211_handle_wake_tx_queue(struct ieee80211_hw *hw,
    struct ieee80211_txq *txq)
{
	struct lkpi_hw *lhw;
	struct ieee80211_txq *ntxq;
	struct ieee80211_tx_control control;
        struct sk_buff *skb;

	lhw = HW_TO_LHW(hw);

	LKPI_80211_LHW_TXQ_LOCK(lhw);
	ieee80211_txq_schedule_start(hw, txq->ac);
	do {
		ntxq = ieee80211_next_txq(hw, txq->ac);
		if (ntxq == NULL)
			break;

		memset(&control, 0, sizeof(control));
		control.sta = ntxq->sta;
		do {
			skb = linuxkpi_ieee80211_tx_dequeue(hw, ntxq);
			if (skb == NULL)
				break;
			lkpi_80211_mo_tx(hw, &control, skb);
		} while(1);

		ieee80211_return_txq(hw, ntxq, false);
	} while (1);
	ieee80211_txq_schedule_end(hw, txq->ac);
	LKPI_80211_LHW_TXQ_UNLOCK(lhw);
}

/* -------------------------------------------------------------------------- */

struct lkpi_cfg80211_bss {
	u_int refcnt;
	struct cfg80211_bss bss;
};

struct lkpi_cfg80211_get_bss_iter_lookup {
	struct wiphy *wiphy;
	struct linuxkpi_ieee80211_channel *chan;
	const uint8_t *bssid;
	const uint8_t *ssid;
	size_t ssid_len;
	enum ieee80211_bss_type bss_type;
	enum ieee80211_privacy privacy;

	/*
	 * Something to store a copy of the result as the net80211 scan cache
	 * is not refoucnted so a scan entry might go away any time.
	 */
	bool match;
	struct cfg80211_bss *bss;
};

static void
lkpi_cfg80211_get_bss_iterf(void *arg, const struct ieee80211_scan_entry *se)
{
	struct lkpi_cfg80211_get_bss_iter_lookup *lookup;
	size_t ielen;

	lookup = arg;

	/* Do not try to find another match. */
	if (lookup->match)
		return;

	/* Nothing to store result. */
	if (lookup->bss == NULL)
		return;

	if (lookup->privacy != IEEE80211_PRIVACY_ANY) {
		/* if (se->se_capinfo & IEEE80211_CAPINFO_PRIVACY) */
		/* We have no idea what to compare to as the drivers only request ANY */
		return;
	}

	if (lookup->bss_type != IEEE80211_BSS_TYPE_ANY) {
		/* if (se->se_capinfo & (IEEE80211_CAPINFO_IBSS|IEEE80211_CAPINFO_ESS)) */
		/* We have no idea what to compare to as the drivers only request ANY */
		return;
	}

	if (lookup->chan != NULL) {
		struct linuxkpi_ieee80211_channel *chan;

		chan = linuxkpi_ieee80211_get_channel(lookup->wiphy,
		    se->se_chan->ic_freq);
		if (chan == NULL || chan != lookup->chan)
			return;
	}

	if (lookup->bssid && !IEEE80211_ADDR_EQ(lookup->bssid, se->se_bssid))
		return;

	if (lookup->ssid) {
		if (lookup->ssid_len != se->se_ssid[1] ||
		    se->se_ssid[1] == 0)
			return;
		if (memcmp(lookup->ssid, se->se_ssid+2, lookup->ssid_len) != 0)
			return;
	}

	ielen = se->se_ies.len;

	lookup->bss->ies = malloc(sizeof(*lookup->bss->ies) + ielen,
	    M_LKPI80211, M_NOWAIT | M_ZERO);
	if (lookup->bss->ies == NULL)
		return;

	lookup->bss->ies->data = (uint8_t *)lookup->bss->ies + sizeof(*lookup->bss->ies);
	lookup->bss->ies->len = ielen;
	if (ielen)
		memcpy(lookup->bss->ies->data, se->se_ies.data, ielen);

	lookup->match = true;
}

struct cfg80211_bss *
linuxkpi_cfg80211_get_bss(struct wiphy *wiphy, struct linuxkpi_ieee80211_channel *chan,
    const uint8_t *bssid, const uint8_t *ssid, size_t ssid_len,
    enum ieee80211_bss_type bss_type, enum ieee80211_privacy privacy)
{
	struct lkpi_cfg80211_bss *lbss;
	struct lkpi_cfg80211_get_bss_iter_lookup lookup;
	struct lkpi_hw *lhw;
	struct ieee80211vap *vap;

	lhw = wiphy_priv(wiphy);

	/* Let's hope we can alloc. */
	lbss = malloc(sizeof(*lbss), M_LKPI80211, M_NOWAIT | M_ZERO);
	if (lbss == NULL) {
		ic_printf(lhw->ic, "%s: alloc failed.\n", __func__);
		return (NULL);
	}

	lookup.wiphy = wiphy;
	lookup.chan = chan;
	lookup.bssid = bssid;
	lookup.ssid = ssid;
	lookup.ssid_len = ssid_len;
	lookup.bss_type = bss_type;
	lookup.privacy = privacy;
	lookup.match = false;
	lookup.bss = &lbss->bss;

	IMPROVE("Iterate over all VAPs comparing perm_addr and addresses?");
	vap = TAILQ_FIRST(&lhw->ic->ic_vaps);
	ieee80211_scan_iterate(vap, lkpi_cfg80211_get_bss_iterf, &lookup);
	if (!lookup.match) {
		free(lbss, M_LKPI80211);
		return (NULL);
	}

	refcount_init(&lbss->refcnt, 1);
	return (&lbss->bss);
}

void
linuxkpi_cfg80211_put_bss(struct wiphy *wiphy, struct cfg80211_bss *bss)
{
	struct lkpi_cfg80211_bss *lbss;

	lbss = container_of(bss, struct lkpi_cfg80211_bss, bss);

	/* Free everything again on refcount ... */
	if (refcount_release(&lbss->refcnt)) {
		free(lbss->bss.ies, M_LKPI80211);
		free(lbss, M_LKPI80211);
	}
}

void
linuxkpi_cfg80211_bss_flush(struct wiphy *wiphy)
{
	struct lkpi_hw *lhw;
	struct ieee80211com *ic;
	struct ieee80211vap *vap;

	lhw = wiphy_priv(wiphy);
	ic = lhw->ic;

	/*
	 * If we haven't called ieee80211_ifattach() yet
	 * or there is no VAP, there are no scans to flush.
	 */
	if (ic == NULL ||
	    (lhw->sc_flags & LKPI_MAC80211_DRV_STARTED) == 0)
		return;

	/* Should only happen on the current one? Not seen it late enough. */
	IEEE80211_LOCK(ic);
	TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next)
		ieee80211_scan_flush(vap);
	IEEE80211_UNLOCK(ic);
}

/* -------------------------------------------------------------------------- */

/*
 * hw->conf get initialized/set in various places for us:
 * - linuxkpi_ieee80211_alloc_hw(): flags
 * - linuxkpi_ieee80211_ifattach(): chandef
 * - lkpi_ic_vap_create(): listen_interval
 * - lkpi_ic_set_channel(): chandef, flags
 */

int lkpi_80211_update_chandef(struct ieee80211_hw *hw,
    struct ieee80211_chanctx_conf *new)
{
	struct cfg80211_chan_def *cd;
	uint32_t changed;
	int error;

	changed = 0;
	if (new == NULL || new->def.chan == NULL)
		cd = NULL;
	else
		cd = &new->def;

	if (cd && cd->chan != hw->conf.chandef.chan) {
		/* Copy; the chan pointer is fine and will stay valid. */
		hw->conf.chandef = *cd;
		changed |= IEEE80211_CONF_CHANGE_CHANNEL;
	}
	IMPROVE("IEEE80211_CONF_CHANGE_PS, IEEE80211_CONF_CHANGE_POWER");

	if (changed == 0)
		return (0);

	error = lkpi_80211_mo_config(hw, changed);
	return (error);
}

/* -------------------------------------------------------------------------- */

MODULE_VERSION(linuxkpi_wlan, 1);
MODULE_DEPEND(linuxkpi_wlan, linuxkpi, 1, 1, 1);
MODULE_DEPEND(linuxkpi_wlan, wlan, 1, 1, 1);
