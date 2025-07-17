// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include "core.h"
#include "dp_tx.h"
#include "debug.h"
#include "debugfs_sta.h"
#include "hw.h"
#include "peer.h"
#include "mac.h"

static enum hal_tcl_encap_type
ath11k_dp_tx_get_encap_type(struct ath11k_vif *arvif, struct sk_buff *skb)
{
	struct ieee80211_tx_info *tx_info = IEEE80211_SKB_CB(skb);
	struct ath11k_base *ab = arvif->ar->ab;

	if (test_bit(ATH11K_FLAG_RAW_MODE, &ab->dev_flags))
		return HAL_TCL_ENCAP_TYPE_RAW;

	if (tx_info->flags & IEEE80211_TX_CTL_HW_80211_ENCAP)
		return HAL_TCL_ENCAP_TYPE_ETHERNET;

	return HAL_TCL_ENCAP_TYPE_NATIVE_WIFI;
}

static void ath11k_dp_tx_encap_nwifi(struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr = (void *)skb->data;
	u8 *qos_ctl;

	if (!ieee80211_is_data_qos(hdr->frame_control))
		return;

	qos_ctl = ieee80211_get_qos_ctl(hdr);
	memmove(skb->data + IEEE80211_QOS_CTL_LEN,
#if defined(__linux__)
		skb->data, (void *)qos_ctl - (void *)skb->data);
#elif defined(__FreeBSD__)
		skb->data, qos_ctl - (u8 *)skb->data);
#endif
	skb_pull(skb, IEEE80211_QOS_CTL_LEN);

	hdr = (void *)skb->data;
	hdr->frame_control &= ~__cpu_to_le16(IEEE80211_STYPE_QOS_DATA);
}

static u8 ath11k_dp_tx_get_tid(struct sk_buff *skb)
{
	struct ieee80211_hdr *hdr = (void *)skb->data;
	struct ath11k_skb_cb *cb = ATH11K_SKB_CB(skb);

	if (cb->flags & ATH11K_SKB_HW_80211_ENCAP)
		return skb->priority & IEEE80211_QOS_CTL_TID_MASK;
	else if (!ieee80211_is_data_qos(hdr->frame_control))
		return HAL_DESC_REO_NON_QOS_TID;
	else
		return skb->priority & IEEE80211_QOS_CTL_TID_MASK;
}

enum hal_encrypt_type ath11k_dp_tx_get_encrypt_type(u32 cipher)
{
	switch (cipher) {
	case WLAN_CIPHER_SUITE_WEP40:
		return HAL_ENCRYPT_TYPE_WEP_40;
	case WLAN_CIPHER_SUITE_WEP104:
		return HAL_ENCRYPT_TYPE_WEP_104;
	case WLAN_CIPHER_SUITE_TKIP:
		return HAL_ENCRYPT_TYPE_TKIP_MIC;
	case WLAN_CIPHER_SUITE_CCMP:
		return HAL_ENCRYPT_TYPE_CCMP_128;
	case WLAN_CIPHER_SUITE_CCMP_256:
		return HAL_ENCRYPT_TYPE_CCMP_256;
	case WLAN_CIPHER_SUITE_GCMP:
		return HAL_ENCRYPT_TYPE_GCMP_128;
	case WLAN_CIPHER_SUITE_GCMP_256:
		return HAL_ENCRYPT_TYPE_AES_GCMP_256;
	default:
		return HAL_ENCRYPT_TYPE_OPEN;
	}
}

int ath11k_dp_tx(struct ath11k *ar, struct ath11k_vif *arvif,
		 struct ath11k_sta *arsta, struct sk_buff *skb)
{
	struct ath11k_base *ab = ar->ab;
	struct ath11k_dp *dp = &ab->dp;
	struct hal_tx_info ti = {0};
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ath11k_skb_cb *skb_cb = ATH11K_SKB_CB(skb);
	struct hal_srng *tcl_ring;
	struct ieee80211_hdr *hdr = (void *)skb->data;
	struct dp_tx_ring *tx_ring;
#if defined(__linux__)
	void *hal_tcl_desc;
#elif defined(__FreeBSD__)
	u8 *hal_tcl_desc;
#endif
	u8 pool_id;
	u8 hal_ring_id;
	int ret;
	u32 ring_selector = 0;
	u8 ring_map = 0;
	bool tcl_ring_retry;

	if (unlikely(test_bit(ATH11K_FLAG_CRASH_FLUSH, &ar->ab->dev_flags)))
		return -ESHUTDOWN;

	if (unlikely(!(info->flags & IEEE80211_TX_CTL_HW_80211_ENCAP) &&
		     !ieee80211_is_data(hdr->frame_control)))
		return -ENOTSUPP;

	pool_id = skb_get_queue_mapping(skb) & (ATH11K_HW_MAX_QUEUES - 1);

	ring_selector = ab->hw_params.hw_ops->get_ring_selector(skb);

tcl_ring_sel:
	tcl_ring_retry = false;

	ti.ring_id = ring_selector % ab->hw_params.max_tx_ring;
	ti.rbm_id = ab->hw_params.hal_params->tcl2wbm_rbm_map[ti.ring_id].rbm_id;

	ring_map |= BIT(ti.ring_id);

	tx_ring = &dp->tx_ring[ti.ring_id];

	spin_lock_bh(&tx_ring->tx_idr_lock);
	ret = idr_alloc(&tx_ring->txbuf_idr, skb, 0,
			DP_TX_IDR_SIZE - 1, GFP_ATOMIC);
	spin_unlock_bh(&tx_ring->tx_idr_lock);

	if (unlikely(ret < 0)) {
		if (ring_map == (BIT(ab->hw_params.max_tx_ring) - 1) ||
		    !ab->hw_params.tcl_ring_retry) {
			atomic_inc(&ab->soc_stats.tx_err.misc_fail);
			return -ENOSPC;
		}

		/* Check if the next ring is available */
		ring_selector++;
		goto tcl_ring_sel;
	}

	ti.desc_id = FIELD_PREP(DP_TX_DESC_ID_MAC_ID, ar->pdev_idx) |
		     FIELD_PREP(DP_TX_DESC_ID_MSDU_ID, ret) |
		     FIELD_PREP(DP_TX_DESC_ID_POOL_ID, pool_id);
	ti.encap_type = ath11k_dp_tx_get_encap_type(arvif, skb);

	if (ieee80211_has_a4(hdr->frame_control) &&
	    is_multicast_ether_addr(hdr->addr3) && arsta &&
	    arsta->use_4addr_set) {
		ti.meta_data_flags = arsta->tcl_metadata;
		ti.flags0 |= FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_TO_FW, 1);
	} else {
		ti.meta_data_flags = arvif->tcl_metadata;
	}

	if (unlikely(ti.encap_type == HAL_TCL_ENCAP_TYPE_RAW)) {
		if (skb_cb->flags & ATH11K_SKB_CIPHER_SET) {
			ti.encrypt_type =
				ath11k_dp_tx_get_encrypt_type(skb_cb->cipher);

			if (ieee80211_has_protected(hdr->frame_control))
				skb_put(skb, IEEE80211_CCMP_MIC_LEN);
		} else {
			ti.encrypt_type = HAL_ENCRYPT_TYPE_OPEN;
		}
	}

	ti.addr_search_flags = arvif->hal_addr_search_flags;
	ti.search_type = arvif->search_type;
	ti.type = HAL_TCL_DESC_TYPE_BUFFER;
	ti.pkt_offset = 0;
	ti.lmac_id = ar->lmac_id;
	ti.bss_ast_hash = arvif->ast_hash;
	ti.bss_ast_idx = arvif->ast_idx;
	ti.dscp_tid_tbl_idx = 0;

	if (likely(skb->ip_summed == CHECKSUM_PARTIAL &&
		   ti.encap_type != HAL_TCL_ENCAP_TYPE_RAW)) {
		ti.flags0 |= FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_IP4_CKSUM_EN, 1) |
			     FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_UDP4_CKSUM_EN, 1) |
			     FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_UDP6_CKSUM_EN, 1) |
			     FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_TCP4_CKSUM_EN, 1) |
			     FIELD_PREP(HAL_TCL_DATA_CMD_INFO1_TCP6_CKSUM_EN, 1);
	}

	if (ieee80211_vif_is_mesh(arvif->vif))
		ti.enable_mesh = true;

	ti.flags1 |= FIELD_PREP(HAL_TCL_DATA_CMD_INFO2_TID_OVERWRITE, 1);

	ti.tid = ath11k_dp_tx_get_tid(skb);

	switch (ti.encap_type) {
	case HAL_TCL_ENCAP_TYPE_NATIVE_WIFI:
		ath11k_dp_tx_encap_nwifi(skb);
		break;
	case HAL_TCL_ENCAP_TYPE_RAW:
		if (!test_bit(ATH11K_FLAG_RAW_MODE, &ab->dev_flags)) {
			ret = -EINVAL;
			goto fail_remove_idr;
		}
		break;
	case HAL_TCL_ENCAP_TYPE_ETHERNET:
		/* no need to encap */
		break;
	case HAL_TCL_ENCAP_TYPE_802_3:
	default:
		/* TODO: Take care of other encap modes as well */
		ret = -EINVAL;
		atomic_inc(&ab->soc_stats.tx_err.misc_fail);
		goto fail_remove_idr;
	}

	ti.paddr = dma_map_single(ab->dev, skb->data, skb->len, DMA_TO_DEVICE);
	if (unlikely(dma_mapping_error(ab->dev, ti.paddr))) {
		atomic_inc(&ab->soc_stats.tx_err.misc_fail);
		ath11k_warn(ab, "failed to DMA map data Tx buffer\n");
		ret = -ENOMEM;
		goto fail_remove_idr;
	}

	ti.data_len = skb->len;
	skb_cb->paddr = ti.paddr;
	skb_cb->vif = arvif->vif;
	skb_cb->ar = ar;

	hal_ring_id = tx_ring->tcl_data_ring.ring_id;
	tcl_ring = &ab->hal.srng_list[hal_ring_id];

	spin_lock_bh(&tcl_ring->lock);

	ath11k_hal_srng_access_begin(ab, tcl_ring);

	hal_tcl_desc = (void *)ath11k_hal_srng_src_get_next_entry(ab, tcl_ring);
	if (unlikely(!hal_tcl_desc)) {
		/* NOTE: It is highly unlikely we'll be running out of tcl_ring
		 * desc because the desc is directly enqueued onto hw queue.
		 */
		ath11k_hal_srng_access_end(ab, tcl_ring);
		ab->soc_stats.tx_err.desc_na[ti.ring_id]++;
		spin_unlock_bh(&tcl_ring->lock);
		ret = -ENOMEM;

		/* Checking for available tcl descritors in another ring in
		 * case of failure due to full tcl ring now, is better than
		 * checking this ring earlier for each pkt tx.
		 * Restart ring selection if some rings are not checked yet.
		 */
		if (unlikely(ring_map != (BIT(ab->hw_params.max_tx_ring)) - 1) &&
		    ab->hw_params.tcl_ring_retry && ab->hw_params.max_tx_ring > 1) {
			tcl_ring_retry = true;
			ring_selector++;
		}

		goto fail_unmap_dma;
	}

	ath11k_hal_tx_cmd_desc_setup(ab, hal_tcl_desc +
					 sizeof(struct hal_tlv_hdr), &ti);

	ath11k_hal_srng_access_end(ab, tcl_ring);

	ath11k_dp_shadow_start_timer(ab, tcl_ring, &dp->tx_ring_timer[ti.ring_id]);

	spin_unlock_bh(&tcl_ring->lock);

	ath11k_dbg_dump(ab, ATH11K_DBG_DP_TX, NULL, "dp tx msdu: ",
			skb->data, skb->len);

	atomic_inc(&ar->dp.num_tx_pending);

	return 0;

fail_unmap_dma:
	dma_unmap_single(ab->dev, ti.paddr, ti.data_len, DMA_TO_DEVICE);

fail_remove_idr:
	spin_lock_bh(&tx_ring->tx_idr_lock);
	idr_remove(&tx_ring->txbuf_idr,
		   FIELD_GET(DP_TX_DESC_ID_MSDU_ID, ti.desc_id));
	spin_unlock_bh(&tx_ring->tx_idr_lock);

	if (tcl_ring_retry)
		goto tcl_ring_sel;

	return ret;
}

static void ath11k_dp_tx_free_txbuf(struct ath11k_base *ab, u8 mac_id,
				    int msdu_id,
				    struct dp_tx_ring *tx_ring)
{
	struct ath11k *ar;
	struct sk_buff *msdu;
	struct ath11k_skb_cb *skb_cb;

	spin_lock(&tx_ring->tx_idr_lock);
	msdu = idr_remove(&tx_ring->txbuf_idr, msdu_id);
	spin_unlock(&tx_ring->tx_idr_lock);

	if (unlikely(!msdu)) {
		ath11k_warn(ab, "tx completion for unknown msdu_id %d\n",
			    msdu_id);
		return;
	}

	skb_cb = ATH11K_SKB_CB(msdu);

	dma_unmap_single(ab->dev, skb_cb->paddr, msdu->len, DMA_TO_DEVICE);
	dev_kfree_skb_any(msdu);

	ar = ab->pdevs[mac_id].ar;
	if (atomic_dec_and_test(&ar->dp.num_tx_pending))
		wake_up(&ar->dp.tx_empty_waitq);
}

static void
ath11k_dp_tx_htt_tx_complete_buf(struct ath11k_base *ab,
				 struct dp_tx_ring *tx_ring,
				 struct ath11k_dp_htt_wbm_tx_status *ts)
{
	struct ieee80211_tx_status status = { 0 };
	struct sk_buff *msdu;
	struct ieee80211_tx_info *info;
	struct ath11k_skb_cb *skb_cb;
	struct ath11k *ar;
	struct ath11k_peer *peer;

	spin_lock(&tx_ring->tx_idr_lock);
	msdu = idr_remove(&tx_ring->txbuf_idr, ts->msdu_id);
	spin_unlock(&tx_ring->tx_idr_lock);

	if (unlikely(!msdu)) {
		ath11k_warn(ab, "htt tx completion for unknown msdu_id %d\n",
			    ts->msdu_id);
		return;
	}

	skb_cb = ATH11K_SKB_CB(msdu);
	info = IEEE80211_SKB_CB(msdu);

	ar = skb_cb->ar;

	if (atomic_dec_and_test(&ar->dp.num_tx_pending))
		wake_up(&ar->dp.tx_empty_waitq);

	dma_unmap_single(ab->dev, skb_cb->paddr, msdu->len, DMA_TO_DEVICE);

	if (!skb_cb->vif) {
		dev_kfree_skb_any(msdu);
		return;
	}

	memset(&info->status, 0, sizeof(info->status));

	if (ts->acked) {
		if (!(info->flags & IEEE80211_TX_CTL_NO_ACK)) {
			info->flags |= IEEE80211_TX_STAT_ACK;
			info->status.ack_signal = ATH11K_DEFAULT_NOISE_FLOOR +
						  ts->ack_rssi;
			info->status.flags |=
				IEEE80211_TX_STATUS_ACK_SIGNAL_VALID;
		} else {
			info->flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;
		}
	}

	spin_lock_bh(&ab->base_lock);
	peer = ath11k_peer_find_by_id(ab, ts->peer_id);
	if (!peer || !peer->sta) {
		ath11k_dbg(ab, ATH11K_DBG_DATA,
			   "dp_tx: failed to find the peer with peer_id %d\n",
			    ts->peer_id);
		spin_unlock_bh(&ab->base_lock);
		dev_kfree_skb_any(msdu);
		return;
	}
	spin_unlock_bh(&ab->base_lock);

	status.sta = peer->sta;
	status.info = info;
	status.skb = msdu;

	ieee80211_tx_status_ext(ar->hw, &status);
}

static void
ath11k_dp_tx_process_htt_tx_complete(struct ath11k_base *ab,
#if defined(__linux__)
				     void *desc, u8 mac_id,
#elif defined(__FreeBSD__)
				     u8 *desc, u8 mac_id,
#endif
				     u32 msdu_id, struct dp_tx_ring *tx_ring)
{
	struct htt_tx_wbm_completion *status_desc;
	struct ath11k_dp_htt_wbm_tx_status ts = {0};
	enum hal_wbm_htt_tx_comp_status wbm_status;

#if defined(__linux__)
	status_desc = desc + HTT_TX_WBM_COMP_STATUS_OFFSET;
#elif defined(__FreeBSD__)
	status_desc = (void *)(desc + HTT_TX_WBM_COMP_STATUS_OFFSET);
#endif

	wbm_status = FIELD_GET(HTT_TX_WBM_COMP_INFO0_STATUS,
			       status_desc->info0);
	switch (wbm_status) {
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_OK:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_DROP:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_TTL:
		ts.acked = (wbm_status == HAL_WBM_REL_HTT_TX_COMP_STATUS_OK);
		ts.msdu_id = msdu_id;
		ts.ack_rssi = FIELD_GET(HTT_TX_WBM_COMP_INFO1_ACK_RSSI,
					status_desc->info1);

		if (FIELD_GET(HTT_TX_WBM_COMP_INFO2_VALID, status_desc->info2))
			ts.peer_id = FIELD_GET(HTT_TX_WBM_COMP_INFO2_SW_PEER_ID,
					       status_desc->info2);
		else
			ts.peer_id = HTT_INVALID_PEER_ID;

		ath11k_dp_tx_htt_tx_complete_buf(ab, tx_ring, &ts);

		break;
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_REINJ:
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_INSPECT:
		ath11k_dp_tx_free_txbuf(ab, mac_id, msdu_id, tx_ring);
		break;
	case HAL_WBM_REL_HTT_TX_COMP_STATUS_MEC_NOTIFY:
		/* This event is to be handled only when the driver decides to
		 * use WDS offload functionality.
		 */
		break;
	default:
		ath11k_warn(ab, "Unknown htt tx status %d\n", wbm_status);
		break;
	}
}

static void ath11k_dp_tx_cache_peer_stats(struct ath11k *ar,
					  struct sk_buff *msdu,
					  struct hal_tx_status *ts)
{
	struct ath11k_per_peer_tx_stats *peer_stats = &ar->cached_stats;

	if (ts->try_cnt > 1) {
		peer_stats->retry_pkts += ts->try_cnt - 1;
		peer_stats->retry_bytes += (ts->try_cnt - 1) * msdu->len;

		if (ts->status != HAL_WBM_TQM_REL_REASON_FRAME_ACKED) {
			peer_stats->failed_pkts += 1;
			peer_stats->failed_bytes += msdu->len;
		}
	}
}

void ath11k_dp_tx_update_txcompl(struct ath11k *ar, struct hal_tx_status *ts)
{
	struct ath11k_base *ab = ar->ab;
	struct ath11k_per_peer_tx_stats *peer_stats = &ar->cached_stats;
	enum hal_tx_rate_stats_pkt_type pkt_type;
	enum hal_tx_rate_stats_sgi sgi;
	enum hal_tx_rate_stats_bw bw;
	struct ath11k_peer *peer;
	struct ath11k_sta *arsta;
	struct ieee80211_sta *sta;
	u16 rate, ru_tones;
	u8 mcs, rate_idx = 0, ofdma;
	int ret;

	spin_lock_bh(&ab->base_lock);
	peer = ath11k_peer_find_by_id(ab, ts->peer_id);
	if (!peer || !peer->sta) {
		ath11k_dbg(ab, ATH11K_DBG_DP_TX,
			   "failed to find the peer by id %u\n", ts->peer_id);
		goto err_out;
	}

	sta = peer->sta;
	arsta = (struct ath11k_sta *)sta->drv_priv;

	memset(&arsta->txrate, 0, sizeof(arsta->txrate));
	pkt_type = FIELD_GET(HAL_TX_RATE_STATS_INFO0_PKT_TYPE,
			     ts->rate_stats);
	mcs = FIELD_GET(HAL_TX_RATE_STATS_INFO0_MCS,
			ts->rate_stats);
	sgi = FIELD_GET(HAL_TX_RATE_STATS_INFO0_SGI,
			ts->rate_stats);
	bw = FIELD_GET(HAL_TX_RATE_STATS_INFO0_BW, ts->rate_stats);
	ru_tones = FIELD_GET(HAL_TX_RATE_STATS_INFO0_TONES_IN_RU, ts->rate_stats);
	ofdma = FIELD_GET(HAL_TX_RATE_STATS_INFO0_OFDMA_TX, ts->rate_stats);

	/* This is to prefer choose the real NSS value arsta->last_txrate.nss,
	 * if it is invalid, then choose the NSS value while assoc.
	 */
	if (arsta->last_txrate.nss)
		arsta->txrate.nss = arsta->last_txrate.nss;
	else
		arsta->txrate.nss = arsta->peer_nss;

	if (pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11A ||
	    pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11B) {
		ret = ath11k_mac_hw_ratecode_to_legacy_rate(mcs,
							    pkt_type,
							    &rate_idx,
							    &rate);
		if (ret < 0)
			goto err_out;
		arsta->txrate.legacy = rate;
	} else if (pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11N) {
		if (mcs > 7) {
			ath11k_warn(ab, "Invalid HT mcs index %d\n", mcs);
			goto err_out;
		}

		if (arsta->txrate.nss != 0)
			arsta->txrate.mcs = mcs + 8 * (arsta->txrate.nss - 1);
		arsta->txrate.flags = RATE_INFO_FLAGS_MCS;
		if (sgi)
			arsta->txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
	} else if (pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11AC) {
		if (mcs > 9) {
			ath11k_warn(ab, "Invalid VHT mcs index %d\n", mcs);
			goto err_out;
		}

		arsta->txrate.mcs = mcs;
		arsta->txrate.flags = RATE_INFO_FLAGS_VHT_MCS;
		if (sgi)
			arsta->txrate.flags |= RATE_INFO_FLAGS_SHORT_GI;
	} else if (pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11AX) {
		if (mcs > 11) {
			ath11k_warn(ab, "Invalid HE mcs index %d\n", mcs);
			goto err_out;
		}

		arsta->txrate.mcs = mcs;
		arsta->txrate.flags = RATE_INFO_FLAGS_HE_MCS;
		arsta->txrate.he_gi = ath11k_mac_he_gi_to_nl80211_he_gi(sgi);
	}

	arsta->txrate.bw = ath11k_mac_bw_to_mac80211_bw(bw);
	if (ofdma && pkt_type == HAL_TX_RATE_STATS_PKT_TYPE_11AX) {
		arsta->txrate.bw = RATE_INFO_BW_HE_RU;
		arsta->txrate.he_ru_alloc =
			ath11k_mac_he_ru_tones_to_nl80211_he_ru_alloc(ru_tones);
	}

	if (ath11k_debugfs_is_extd_tx_stats_enabled(ar))
		ath11k_debugfs_sta_add_tx_stats(arsta, peer_stats, rate_idx);

err_out:
	spin_unlock_bh(&ab->base_lock);
}

static void ath11k_dp_tx_complete_msdu(struct ath11k *ar,
				       struct sk_buff *msdu,
				       struct hal_tx_status *ts)
{
	struct ieee80211_tx_status status = { 0 };
	struct ieee80211_rate_status status_rate = { 0 };
	struct ath11k_base *ab = ar->ab;
	struct ieee80211_tx_info *info;
	struct ath11k_skb_cb *skb_cb;
	struct ath11k_peer *peer;
	struct ath11k_sta *arsta;
	struct rate_info rate;

	if (WARN_ON_ONCE(ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_TQM)) {
		/* Must not happen */
		return;
	}

	skb_cb = ATH11K_SKB_CB(msdu);

	dma_unmap_single(ab->dev, skb_cb->paddr, msdu->len, DMA_TO_DEVICE);

	if (unlikely(!rcu_access_pointer(ab->pdevs_active[ar->pdev_idx]))) {
		dev_kfree_skb_any(msdu);
		return;
	}

	if (unlikely(!skb_cb->vif)) {
		dev_kfree_skb_any(msdu);
		return;
	}

	info = IEEE80211_SKB_CB(msdu);
	memset(&info->status, 0, sizeof(info->status));

	/* skip tx rate update from ieee80211_status*/
	info->status.rates[0].idx = -1;

	if (ts->status == HAL_WBM_TQM_REL_REASON_FRAME_ACKED &&
	    !(info->flags & IEEE80211_TX_CTL_NO_ACK)) {
		info->flags |= IEEE80211_TX_STAT_ACK;
		info->status.ack_signal = ATH11K_DEFAULT_NOISE_FLOOR +
					  ts->ack_rssi;
		info->status.flags |= IEEE80211_TX_STATUS_ACK_SIGNAL_VALID;
	}

	if (ts->status == HAL_WBM_TQM_REL_REASON_CMD_REMOVE_TX &&
	    (info->flags & IEEE80211_TX_CTL_NO_ACK))
		info->flags |= IEEE80211_TX_STAT_NOACK_TRANSMITTED;

	if (unlikely(ath11k_debugfs_is_extd_tx_stats_enabled(ar)) ||
	    ab->hw_params.single_pdev_only) {
		if (ts->flags & HAL_TX_STATUS_FLAGS_FIRST_MSDU) {
			if (ar->last_ppdu_id == 0) {
				ar->last_ppdu_id = ts->ppdu_id;
			} else if (ar->last_ppdu_id == ts->ppdu_id ||
				   ar->cached_ppdu_id == ar->last_ppdu_id) {
				ar->cached_ppdu_id = ar->last_ppdu_id;
				ar->cached_stats.is_ampdu = true;
				ath11k_dp_tx_update_txcompl(ar, ts);
				memset(&ar->cached_stats, 0,
				       sizeof(struct ath11k_per_peer_tx_stats));
			} else {
				ar->cached_stats.is_ampdu = false;
				ath11k_dp_tx_update_txcompl(ar, ts);
				memset(&ar->cached_stats, 0,
				       sizeof(struct ath11k_per_peer_tx_stats));
			}
			ar->last_ppdu_id = ts->ppdu_id;
		}

		ath11k_dp_tx_cache_peer_stats(ar, msdu, ts);
	}

	spin_lock_bh(&ab->base_lock);
	peer = ath11k_peer_find_by_id(ab, ts->peer_id);
	if (!peer || !peer->sta) {
		ath11k_dbg(ab, ATH11K_DBG_DATA,
			   "dp_tx: failed to find the peer with peer_id %d\n",
			    ts->peer_id);
		spin_unlock_bh(&ab->base_lock);
		dev_kfree_skb_any(msdu);
		return;
	}
	arsta = (struct ath11k_sta *)peer->sta->drv_priv;
	status.sta = peer->sta;
	status.skb = msdu;
	status.info = info;
	rate = arsta->last_txrate;

	status_rate.rate_idx = rate;
	status_rate.try_count = 1;

	status.rates = &status_rate;
	status.n_rates = 1;

	spin_unlock_bh(&ab->base_lock);

	ieee80211_tx_status_ext(ar->hw, &status);
}

static inline void ath11k_dp_tx_status_parse(struct ath11k_base *ab,
					     struct hal_wbm_release_ring *desc,
					     struct hal_tx_status *ts)
{
	ts->buf_rel_source =
		FIELD_GET(HAL_WBM_RELEASE_INFO0_REL_SRC_MODULE, desc->info0);
	if (unlikely(ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_FW &&
		     ts->buf_rel_source != HAL_WBM_REL_SRC_MODULE_TQM))
		return;

	if (unlikely(ts->buf_rel_source == HAL_WBM_REL_SRC_MODULE_FW))
		return;

	ts->status = FIELD_GET(HAL_WBM_RELEASE_INFO0_TQM_RELEASE_REASON,
			       desc->info0);
	ts->ppdu_id = FIELD_GET(HAL_WBM_RELEASE_INFO1_TQM_STATUS_NUMBER,
				desc->info1);
	ts->try_cnt = FIELD_GET(HAL_WBM_RELEASE_INFO1_TRANSMIT_COUNT,
				desc->info1);
	ts->ack_rssi = FIELD_GET(HAL_WBM_RELEASE_INFO2_ACK_FRAME_RSSI,
				 desc->info2);
	if (desc->info2 & HAL_WBM_RELEASE_INFO2_FIRST_MSDU)
		ts->flags |= HAL_TX_STATUS_FLAGS_FIRST_MSDU;
	ts->peer_id = FIELD_GET(HAL_WBM_RELEASE_INFO3_PEER_ID, desc->info3);
	ts->tid = FIELD_GET(HAL_WBM_RELEASE_INFO3_TID, desc->info3);
	if (desc->rate_stats.info0 & HAL_TX_RATE_STATS_INFO0_VALID)
		ts->rate_stats = desc->rate_stats.info0;
	else
		ts->rate_stats = 0;
}

void ath11k_dp_tx_completion_handler(struct ath11k_base *ab, int ring_id)
{
	struct ath11k *ar;
	struct ath11k_dp *dp = &ab->dp;
	int hal_ring_id = dp->tx_ring[ring_id].tcl_comp_ring.ring_id;
	struct hal_srng *status_ring = &ab->hal.srng_list[hal_ring_id];
	struct sk_buff *msdu;
	struct hal_tx_status ts = { 0 };
	struct dp_tx_ring *tx_ring = &dp->tx_ring[ring_id];
	u32 *desc;
	u32 msdu_id;
	u8 mac_id;

	spin_lock_bh(&status_ring->lock);

	ath11k_hal_srng_access_begin(ab, status_ring);

	while ((ATH11K_TX_COMPL_NEXT(tx_ring->tx_status_head) !=
		tx_ring->tx_status_tail) &&
	       (desc = ath11k_hal_srng_dst_get_next_entry(ab, status_ring))) {
		memcpy(&tx_ring->tx_status[tx_ring->tx_status_head],
		       desc, sizeof(struct hal_wbm_release_ring));
		tx_ring->tx_status_head =
			ATH11K_TX_COMPL_NEXT(tx_ring->tx_status_head);
	}

	if (unlikely((ath11k_hal_srng_dst_peek(ab, status_ring) != NULL) &&
		     (ATH11K_TX_COMPL_NEXT(tx_ring->tx_status_head) ==
		      tx_ring->tx_status_tail))) {
		/* TODO: Process pending tx_status messages when kfifo_is_full() */
		ath11k_warn(ab, "Unable to process some of the tx_status ring desc because status_fifo is full\n");
	}

	ath11k_hal_srng_access_end(ab, status_ring);

	spin_unlock_bh(&status_ring->lock);

	while (ATH11K_TX_COMPL_NEXT(tx_ring->tx_status_tail) != tx_ring->tx_status_head) {
		struct hal_wbm_release_ring *tx_status;
		u32 desc_id;

		tx_ring->tx_status_tail =
			ATH11K_TX_COMPL_NEXT(tx_ring->tx_status_tail);
		tx_status = &tx_ring->tx_status[tx_ring->tx_status_tail];
		ath11k_dp_tx_status_parse(ab, tx_status, &ts);

		desc_id = FIELD_GET(BUFFER_ADDR_INFO1_SW_COOKIE,
				    tx_status->buf_addr_info.info1);
		mac_id = FIELD_GET(DP_TX_DESC_ID_MAC_ID, desc_id);
		msdu_id = FIELD_GET(DP_TX_DESC_ID_MSDU_ID, desc_id);

		if (unlikely(ts.buf_rel_source == HAL_WBM_REL_SRC_MODULE_FW)) {
			ath11k_dp_tx_process_htt_tx_complete(ab,
							     (void *)tx_status,
							     mac_id, msdu_id,
							     tx_ring);
			continue;
		}

		spin_lock(&tx_ring->tx_idr_lock);
		msdu = idr_remove(&tx_ring->txbuf_idr, msdu_id);
		if (unlikely(!msdu)) {
			ath11k_warn(ab, "tx completion for unknown msdu_id %d\n",
				    msdu_id);
			spin_unlock(&tx_ring->tx_idr_lock);
			continue;
		}

		spin_unlock(&tx_ring->tx_idr_lock);

		ar = ab->pdevs[mac_id].ar;

		if (atomic_dec_and_test(&ar->dp.num_tx_pending))
			wake_up(&ar->dp.tx_empty_waitq);

		ath11k_dp_tx_complete_msdu(ar, msdu, &ts);
	}
}

int ath11k_dp_tx_send_reo_cmd(struct ath11k_base *ab, struct dp_rx_tid *rx_tid,
			      enum hal_reo_cmd_type type,
			      struct ath11k_hal_reo_cmd *cmd,
			      void (*cb)(struct ath11k_dp *, void *,
					 enum hal_reo_cmd_status))
{
	struct ath11k_dp *dp = &ab->dp;
	struct dp_reo_cmd *dp_cmd;
	struct hal_srng *cmd_ring;
	int cmd_num;

	if (test_bit(ATH11K_FLAG_CRASH_FLUSH, &ab->dev_flags))
		return -ESHUTDOWN;

	cmd_ring = &ab->hal.srng_list[dp->reo_cmd_ring.ring_id];
	cmd_num = ath11k_hal_reo_cmd_send(ab, cmd_ring, type, cmd);

	/* cmd_num should start from 1, during failure return the error code */
	if (cmd_num < 0)
		return cmd_num;

	/* reo cmd ring descriptors has cmd_num starting from 1 */
	if (cmd_num == 0)
		return -EINVAL;

	if (!cb)
		return 0;

	/* Can this be optimized so that we keep the pending command list only
	 * for tid delete command to free up the resource on the command status
	 * indication?
	 */
	dp_cmd = kzalloc(sizeof(*dp_cmd), GFP_ATOMIC);

	if (!dp_cmd)
		return -ENOMEM;

	memcpy(&dp_cmd->data, rx_tid, sizeof(struct dp_rx_tid));
	dp_cmd->cmd_num = cmd_num;
	dp_cmd->handler = cb;

	spin_lock_bh(&dp->reo_cmd_lock);
	list_add_tail(&dp_cmd->list, &dp->reo_cmd_list);
	spin_unlock_bh(&dp->reo_cmd_lock);

	return 0;
}

static int
ath11k_dp_tx_get_ring_id_type(struct ath11k_base *ab,
			      int mac_id, u32 ring_id,
			      enum hal_ring_type ring_type,
			      enum htt_srng_ring_type *htt_ring_type,
			      enum htt_srng_ring_id *htt_ring_id)
{
	int lmac_ring_id_offset = 0;
	int ret = 0;

	switch (ring_type) {
	case HAL_RXDMA_BUF:
		lmac_ring_id_offset = mac_id * HAL_SRNG_RINGS_PER_LMAC;

		/* for QCA6390, host fills rx buffer to fw and fw fills to
		 * rxbuf ring for each rxdma
		 */
		if (!ab->hw_params.rx_mac_buf_ring) {
			if (!(ring_id == (HAL_SRNG_RING_ID_WMAC1_SW2RXDMA0_BUF +
					  lmac_ring_id_offset) ||
				ring_id == (HAL_SRNG_RING_ID_WMAC1_SW2RXDMA1_BUF +
					lmac_ring_id_offset))) {
				ret = -EINVAL;
			}
			*htt_ring_id = HTT_RXDMA_HOST_BUF_RING;
			*htt_ring_type = HTT_SW_TO_HW_RING;
		} else {
			if (ring_id == HAL_SRNG_RING_ID_WMAC1_SW2RXDMA0_BUF) {
				*htt_ring_id = HTT_HOST1_TO_FW_RXBUF_RING;
				*htt_ring_type = HTT_SW_TO_SW_RING;
			} else {
				*htt_ring_id = HTT_RXDMA_HOST_BUF_RING;
				*htt_ring_type = HTT_SW_TO_HW_RING;
			}
		}
		break;
	case HAL_RXDMA_DST:
		*htt_ring_id = HTT_RXDMA_NON_MONITOR_DEST_RING;
		*htt_ring_type = HTT_HW_TO_SW_RING;
		break;
	case HAL_RXDMA_MONITOR_BUF:
		*htt_ring_id = HTT_RXDMA_MONITOR_BUF_RING;
		*htt_ring_type = HTT_SW_TO_HW_RING;
		break;
	case HAL_RXDMA_MONITOR_STATUS:
		*htt_ring_id = HTT_RXDMA_MONITOR_STATUS_RING;
		*htt_ring_type = HTT_SW_TO_HW_RING;
		break;
	case HAL_RXDMA_MONITOR_DST:
		*htt_ring_id = HTT_RXDMA_MONITOR_DEST_RING;
		*htt_ring_type = HTT_HW_TO_SW_RING;
		break;
	case HAL_RXDMA_MONITOR_DESC:
		*htt_ring_id = HTT_RXDMA_MONITOR_DESC_RING;
		*htt_ring_type = HTT_SW_TO_HW_RING;
		break;
	default:
		ath11k_warn(ab, "Unsupported ring type in DP :%d\n", ring_type);
		ret = -EINVAL;
	}
	return ret;
}

int ath11k_dp_tx_htt_srng_setup(struct ath11k_base *ab, u32 ring_id,
				int mac_id, enum hal_ring_type ring_type)
{
	struct htt_srng_setup_cmd *cmd;
	struct hal_srng *srng = &ab->hal.srng_list[ring_id];
	struct hal_srng_params params;
	struct sk_buff *skb;
	u32 ring_entry_sz;
	int len = sizeof(*cmd);
	dma_addr_t hp_addr, tp_addr;
	enum htt_srng_ring_type htt_ring_type;
	enum htt_srng_ring_id htt_ring_id;
	int ret;

	skb = ath11k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	memset(&params, 0, sizeof(params));
	ath11k_hal_srng_get_params(ab, srng, &params);

	hp_addr = ath11k_hal_srng_get_hp_addr(ab, srng);
	tp_addr = ath11k_hal_srng_get_tp_addr(ab, srng);

	ret = ath11k_dp_tx_get_ring_id_type(ab, mac_id, ring_id,
					    ring_type, &htt_ring_type,
					    &htt_ring_id);
	if (ret)
		goto err_free;

	skb_put(skb, len);
	cmd = (struct htt_srng_setup_cmd *)skb->data;
	cmd->info0 = FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO0_MSG_TYPE,
				HTT_H2T_MSG_TYPE_SRING_SETUP);
	if (htt_ring_type == HTT_SW_TO_HW_RING ||
	    htt_ring_type == HTT_HW_TO_SW_RING)
		cmd->info0 |= FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO0_PDEV_ID,
					 DP_SW2HW_MACID(mac_id));
	else
		cmd->info0 |= FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO0_PDEV_ID,
					 mac_id);
	cmd->info0 |= FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO0_RING_TYPE,
				 htt_ring_type);
	cmd->info0 |= FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO0_RING_ID, htt_ring_id);

	cmd->ring_base_addr_lo = params.ring_base_paddr &
				 HAL_ADDR_LSB_REG_MASK;

	cmd->ring_base_addr_hi = (u64)params.ring_base_paddr >>
				 HAL_ADDR_MSB_REG_SHIFT;

	ret = ath11k_hal_srng_get_entrysize(ab, ring_type);
	if (ret < 0)
		goto err_free;

	ring_entry_sz = ret;

	ring_entry_sz >>= 2;
	cmd->info1 = FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO1_RING_ENTRY_SIZE,
				ring_entry_sz);
	cmd->info1 |= FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO1_RING_SIZE,
				 params.num_entries * ring_entry_sz);
	cmd->info1 |= FIELD_PREP(HTT_SRNG_SETUP_CMD_INFO1_RING_FLAGS_MSI_SWAP,
				 !!(params.flags & HAL_SRNG_FLAGS_MSI_SWAP));
	cmd->info1 |= FIELD_PREP(
			HTT_SRNG_SETUP_CMD_INFO1_RING_FLAGS_TLV_SWAP,
			!!(params.flags & HAL_SRNG_FLAGS_DATA_TLV_SWAP));
	cmd->info1 |= FIELD_PREP(
			HTT_SRNG_SETUP_CMD_INFO1_RING_FLAGS_HOST_FW_SWAP,
			!!(params.flags & HAL_SRNG_FLAGS_RING_PTR_SWAP));
	if (htt_ring_type == HTT_SW_TO_HW_RING)
		cmd->info1 |= HTT_SRNG_SETUP_CMD_INFO1_RING_LOOP_CNT_DIS;

	cmd->ring_head_off32_remote_addr_lo = hp_addr & HAL_ADDR_LSB_REG_MASK;
	cmd->ring_head_off32_remote_addr_hi = (u64)hp_addr >>
					      HAL_ADDR_MSB_REG_SHIFT;

	cmd->ring_tail_off32_remote_addr_lo = tp_addr & HAL_ADDR_LSB_REG_MASK;
	cmd->ring_tail_off32_remote_addr_hi = (u64)tp_addr >>
					      HAL_ADDR_MSB_REG_SHIFT;

	cmd->ring_msi_addr_lo = lower_32_bits(params.msi_addr);
	cmd->ring_msi_addr_hi = upper_32_bits(params.msi_addr);
	cmd->msi_data = params.msi_data;

	cmd->intr_info = FIELD_PREP(
			HTT_SRNG_SETUP_CMD_INTR_INFO_BATCH_COUNTER_THRESH,
			params.intr_batch_cntr_thres_entries * ring_entry_sz);
	cmd->intr_info |= FIELD_PREP(
			HTT_SRNG_SETUP_CMD_INTR_INFO_INTR_TIMER_THRESH,
			params.intr_timer_thres_us >> 3);

	cmd->info2 = 0;
	if (params.flags & HAL_SRNG_FLAGS_LOW_THRESH_INTR_EN) {
		cmd->info2 = FIELD_PREP(
				HTT_SRNG_SETUP_CMD_INFO2_INTR_LOW_THRESH,
				params.low_threshold);
	}

	ath11k_dbg(ab, ATH11K_DBG_DP_TX,
		   "htt srng setup msi_addr_lo 0x%x msi_addr_hi 0x%x msi_data 0x%x ring_id %d ring_type %d intr_info 0x%x flags 0x%x\n",
		   cmd->ring_msi_addr_lo, cmd->ring_msi_addr_hi,
		   cmd->msi_data, ring_id, ring_type, cmd->intr_info, cmd->info2);

	ret = ath11k_htc_send(&ab->htc, ab->dp.eid, skb);
	if (ret)
		goto err_free;

	return 0;

err_free:
	dev_kfree_skb_any(skb);

	return ret;
}

#define HTT_TARGET_VERSION_TIMEOUT_HZ (3 * HZ)

int ath11k_dp_tx_htt_h2t_ver_req_msg(struct ath11k_base *ab)
{
	struct ath11k_dp *dp = &ab->dp;
	struct sk_buff *skb;
	struct htt_ver_req_cmd *cmd;
	int len = sizeof(*cmd);
	int ret;

	init_completion(&dp->htt_tgt_version_received);

	skb = ath11k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);
	cmd = (struct htt_ver_req_cmd *)skb->data;
	cmd->ver_reg_info = FIELD_PREP(HTT_VER_REQ_INFO_MSG_ID,
				       HTT_H2T_MSG_TYPE_VERSION_REQ);

	ret = ath11k_htc_send(&ab->htc, dp->eid, skb);
	if (ret) {
		dev_kfree_skb_any(skb);
		return ret;
	}

	ret = wait_for_completion_timeout(&dp->htt_tgt_version_received,
					  HTT_TARGET_VERSION_TIMEOUT_HZ);
	if (ret == 0) {
		ath11k_warn(ab, "htt target version request timed out\n");
		return -ETIMEDOUT;
	}

	if (dp->htt_tgt_ver_major != HTT_TARGET_VERSION_MAJOR) {
		ath11k_err(ab, "unsupported htt major version %d supported version is %d\n",
			   dp->htt_tgt_ver_major, HTT_TARGET_VERSION_MAJOR);
		return -ENOTSUPP;
	}

	return 0;
}

int ath11k_dp_tx_htt_h2t_ppdu_stats_req(struct ath11k *ar, u32 mask)
{
	struct ath11k_base *ab = ar->ab;
	struct ath11k_dp *dp = &ab->dp;
	struct sk_buff *skb;
	struct htt_ppdu_stats_cfg_cmd *cmd;
	int len = sizeof(*cmd);
	u8 pdev_mask;
	int ret;
	int i;

	for (i = 0; i < ab->hw_params.num_rxmda_per_pdev; i++) {
		skb = ath11k_htc_alloc_skb(ab, len);
		if (!skb)
			return -ENOMEM;

		skb_put(skb, len);
		cmd = (struct htt_ppdu_stats_cfg_cmd *)skb->data;
		cmd->msg = FIELD_PREP(HTT_PPDU_STATS_CFG_MSG_TYPE,
				      HTT_H2T_MSG_TYPE_PPDU_STATS_CFG);

		pdev_mask = 1 << (ar->pdev_idx + i);
		cmd->msg |= FIELD_PREP(HTT_PPDU_STATS_CFG_PDEV_ID, pdev_mask);
		cmd->msg |= FIELD_PREP(HTT_PPDU_STATS_CFG_TLV_TYPE_BITMASK, mask);

		ret = ath11k_htc_send(&ab->htc, dp->eid, skb);
		if (ret) {
			dev_kfree_skb_any(skb);
			return ret;
		}
	}

	return 0;
}

int ath11k_dp_tx_htt_rx_filter_setup(struct ath11k_base *ab, u32 ring_id,
				     int mac_id, enum hal_ring_type ring_type,
				     int rx_buf_size,
				     struct htt_rx_ring_tlv_filter *tlv_filter)
{
	struct htt_rx_ring_selection_cfg_cmd *cmd;
	struct hal_srng *srng = &ab->hal.srng_list[ring_id];
	struct hal_srng_params params;
	struct sk_buff *skb;
	int len = sizeof(*cmd);
	enum htt_srng_ring_type htt_ring_type;
	enum htt_srng_ring_id htt_ring_id;
	int ret;

	skb = ath11k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	memset(&params, 0, sizeof(params));
	ath11k_hal_srng_get_params(ab, srng, &params);

	ret = ath11k_dp_tx_get_ring_id_type(ab, mac_id, ring_id,
					    ring_type, &htt_ring_type,
					    &htt_ring_id);
	if (ret)
		goto err_free;

	skb_put(skb, len);
	cmd = (struct htt_rx_ring_selection_cfg_cmd *)skb->data;
	cmd->info0 = FIELD_PREP(HTT_RX_RING_SELECTION_CFG_CMD_INFO0_MSG_TYPE,
				HTT_H2T_MSG_TYPE_RX_RING_SELECTION_CFG);
	if (htt_ring_type == HTT_SW_TO_HW_RING ||
	    htt_ring_type == HTT_HW_TO_SW_RING)
		cmd->info0 |=
			FIELD_PREP(HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PDEV_ID,
				   DP_SW2HW_MACID(mac_id));
	else
		cmd->info0 |=
			FIELD_PREP(HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PDEV_ID,
				   mac_id);
	cmd->info0 |= FIELD_PREP(HTT_RX_RING_SELECTION_CFG_CMD_INFO0_RING_ID,
				 htt_ring_id);
	cmd->info0 |= FIELD_PREP(HTT_RX_RING_SELECTION_CFG_CMD_INFO0_SS,
				 !!(params.flags & HAL_SRNG_FLAGS_MSI_SWAP));
	cmd->info0 |= FIELD_PREP(HTT_RX_RING_SELECTION_CFG_CMD_INFO0_PS,
				 !!(params.flags & HAL_SRNG_FLAGS_DATA_TLV_SWAP));

	cmd->info1 = FIELD_PREP(HTT_RX_RING_SELECTION_CFG_CMD_INFO1_BUF_SIZE,
				rx_buf_size);
	cmd->pkt_type_en_flags0 = tlv_filter->pkt_filter_flags0;
	cmd->pkt_type_en_flags1 = tlv_filter->pkt_filter_flags1;
	cmd->pkt_type_en_flags2 = tlv_filter->pkt_filter_flags2;
	cmd->pkt_type_en_flags3 = tlv_filter->pkt_filter_flags3;
	cmd->rx_filter_tlv = tlv_filter->rx_filter;

	ret = ath11k_htc_send(&ab->htc, ab->dp.eid, skb);
	if (ret)
		goto err_free;

	return 0;

err_free:
	dev_kfree_skb_any(skb);

	return ret;
}

int
ath11k_dp_tx_htt_h2t_ext_stats_req(struct ath11k *ar, u8 type,
				   struct htt_ext_stats_cfg_params *cfg_params,
				   u64 cookie)
{
	struct ath11k_base *ab = ar->ab;
	struct ath11k_dp *dp = &ab->dp;
	struct sk_buff *skb;
	struct htt_ext_stats_cfg_cmd *cmd;
	u32 pdev_id;
	int len = sizeof(*cmd);
	int ret;

	skb = ath11k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);

	cmd = (struct htt_ext_stats_cfg_cmd *)skb->data;
	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr.msg_type = HTT_H2T_MSG_TYPE_EXT_STATS_CFG;

	if (ab->hw_params.single_pdev_only)
		pdev_id = ath11k_mac_get_target_pdev_id(ar);
	else
		pdev_id = ar->pdev->pdev_id;

	cmd->hdr.pdev_mask = 1 << pdev_id;

	cmd->hdr.stats_type = type;
	cmd->cfg_param0 = cfg_params->cfg0;
	cmd->cfg_param1 = cfg_params->cfg1;
	cmd->cfg_param2 = cfg_params->cfg2;
	cmd->cfg_param3 = cfg_params->cfg3;
	cmd->cookie_lsb = lower_32_bits(cookie);
	cmd->cookie_msb = upper_32_bits(cookie);

	ret = ath11k_htc_send(&ab->htc, dp->eid, skb);
	if (ret) {
		ath11k_warn(ab, "failed to send htt type stats request: %d",
			    ret);
		dev_kfree_skb_any(skb);
		return ret;
	}

	return 0;
}

int ath11k_dp_tx_htt_monitor_mode_ring_config(struct ath11k *ar, bool reset)
{
	struct ath11k_pdev_dp *dp = &ar->dp;
	struct ath11k_base *ab = ar->ab;
	struct htt_rx_ring_tlv_filter tlv_filter = {0};
	int ret = 0, ring_id = 0, i;

	if (ab->hw_params.full_monitor_mode) {
		ret = ath11k_dp_tx_htt_rx_full_mon_setup(ab,
							 dp->mac_id, !reset);
		if (ret < 0) {
			ath11k_err(ab, "failed to setup full monitor %d\n", ret);
			return ret;
		}
	}

	ring_id = dp->rxdma_mon_buf_ring.refill_buf_ring.ring_id;

	if (!reset) {
		tlv_filter.rx_filter = HTT_RX_MON_FILTER_TLV_FLAGS_MON_BUF_RING;
		tlv_filter.pkt_filter_flags0 =
					HTT_RX_MON_FP_MGMT_FILTER_FLAGS0 |
					HTT_RX_MON_MO_MGMT_FILTER_FLAGS0;
		tlv_filter.pkt_filter_flags1 =
					HTT_RX_MON_FP_MGMT_FILTER_FLAGS1 |
					HTT_RX_MON_MO_MGMT_FILTER_FLAGS1;
		tlv_filter.pkt_filter_flags2 =
					HTT_RX_MON_FP_CTRL_FILTER_FLASG2 |
					HTT_RX_MON_MO_CTRL_FILTER_FLASG2;
		tlv_filter.pkt_filter_flags3 =
					HTT_RX_MON_FP_CTRL_FILTER_FLASG3 |
					HTT_RX_MON_MO_CTRL_FILTER_FLASG3 |
					HTT_RX_MON_FP_DATA_FILTER_FLASG3 |
					HTT_RX_MON_MO_DATA_FILTER_FLASG3;
	}

	if (ab->hw_params.rxdma1_enable) {
		ret = ath11k_dp_tx_htt_rx_filter_setup(ar->ab, ring_id, dp->mac_id,
						       HAL_RXDMA_MONITOR_BUF,
						       DP_RXDMA_REFILL_RING_SIZE,
						       &tlv_filter);
	} else if (!reset) {
		/* set in monitor mode only */
		for (i = 0; i < ab->hw_params.num_rxmda_per_pdev; i++) {
			ring_id = dp->rx_mac_buf_ring[i].ring_id;
			ret = ath11k_dp_tx_htt_rx_filter_setup(ar->ab, ring_id,
							       dp->mac_id + i,
							       HAL_RXDMA_BUF,
							       1024,
							       &tlv_filter);
		}
	}

	if (ret)
		return ret;

	for (i = 0; i < ab->hw_params.num_rxmda_per_pdev; i++) {
		ring_id = dp->rx_mon_status_refill_ring[i].refill_buf_ring.ring_id;
		if (!reset) {
			tlv_filter.rx_filter =
					HTT_RX_MON_FILTER_TLV_FLAGS_MON_STATUS_RING;
		} else {
			tlv_filter = ath11k_mac_mon_status_filter_default;

			if (ath11k_debugfs_is_extd_rx_stats_enabled(ar))
				tlv_filter.rx_filter = ath11k_debugfs_rx_filter(ar);
		}

		ret = ath11k_dp_tx_htt_rx_filter_setup(ab, ring_id,
						       dp->mac_id + i,
						       HAL_RXDMA_MONITOR_STATUS,
						       DP_RXDMA_REFILL_RING_SIZE,
						       &tlv_filter);
	}

	if (!ar->ab->hw_params.rxdma1_enable)
		mod_timer(&ar->ab->mon_reap_timer, jiffies +
			  msecs_to_jiffies(ATH11K_MON_TIMER_INTERVAL));

	return ret;
}

int ath11k_dp_tx_htt_rx_full_mon_setup(struct ath11k_base *ab, int mac_id,
				       bool config)
{
	struct htt_rx_full_monitor_mode_cfg_cmd *cmd;
	struct sk_buff *skb;
	int ret, len = sizeof(*cmd);

	skb = ath11k_htc_alloc_skb(ab, len);
	if (!skb)
		return -ENOMEM;

	skb_put(skb, len);
	cmd = (struct htt_rx_full_monitor_mode_cfg_cmd *)skb->data;
	memset(cmd, 0, sizeof(*cmd));
	cmd->info0 = FIELD_PREP(HTT_RX_FULL_MON_MODE_CFG_CMD_INFO0_MSG_TYPE,
				HTT_H2T_MSG_TYPE_RX_FULL_MONITOR_MODE);

	cmd->info0 |= FIELD_PREP(HTT_RX_FULL_MON_MODE_CFG_CMD_INFO0_PDEV_ID, mac_id);

	cmd->cfg = HTT_RX_FULL_MON_MODE_CFG_CMD_CFG_ENABLE |
		   FIELD_PREP(HTT_RX_FULL_MON_MODE_CFG_CMD_CFG_RELEASE_RING,
			      HTT_RX_MON_RING_SW);
	if (config) {
		cmd->cfg |= HTT_RX_FULL_MON_MODE_CFG_CMD_CFG_ZERO_MPDUS_END |
			    HTT_RX_FULL_MON_MODE_CFG_CMD_CFG_NON_ZERO_MPDUS_END;
	}

	ret = ath11k_htc_send(&ab->htc, ab->dp.eid, skb);
	if (ret)
		goto err_free;

	return 0;

err_free:
	dev_kfree_skb_any(skb);

	return ret;
}
