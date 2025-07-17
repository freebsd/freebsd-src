// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2018-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */
#include <linux/skbuff.h>
#include <linux/ctype.h>
#include <net/mac80211.h>
#include <net/cfg80211.h>
#include <linux/completion.h>
#include <linux/if_ether.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/uuid.h>
#include <linux/time.h>
#include <linux/of.h>
#include "core.h"
#include "debug.h"
#include "mac.h"
#include "hw.h"
#include "peer.h"

struct ath12k_wmi_svc_ready_parse {
	bool wmi_svc_bitmap_done;
};

struct ath12k_wmi_dma_ring_caps_parse {
	struct ath12k_wmi_dma_ring_caps_params *dma_ring_caps;
	u32 n_dma_ring_caps;
};

struct ath12k_wmi_service_ext_arg {
	u32 default_conc_scan_config_bits;
	u32 default_fw_config_bits;
	struct ath12k_wmi_ppe_threshold_arg ppet;
	u32 he_cap_info;
	u32 mpdu_density;
	u32 max_bssid_rx_filters;
	u32 num_hw_modes;
	u32 num_phy;
};

struct ath12k_wmi_svc_rdy_ext_parse {
	struct ath12k_wmi_service_ext_arg arg;
	const struct ath12k_wmi_soc_mac_phy_hw_mode_caps_params *hw_caps;
	const struct ath12k_wmi_hw_mode_cap_params *hw_mode_caps;
	u32 n_hw_mode_caps;
	u32 tot_phy_id;
	struct ath12k_wmi_hw_mode_cap_params pref_hw_mode_caps;
	struct ath12k_wmi_mac_phy_caps_params *mac_phy_caps;
	u32 n_mac_phy_caps;
	const struct ath12k_wmi_soc_hal_reg_caps_params *soc_hal_reg_caps;
	const struct ath12k_wmi_hal_reg_caps_ext_params *ext_hal_reg_caps;
	u32 n_ext_hal_reg_caps;
	struct ath12k_wmi_dma_ring_caps_parse dma_caps_parse;
	bool hw_mode_done;
	bool mac_phy_done;
	bool ext_hal_reg_done;
	bool mac_phy_chainmask_combo_done;
	bool mac_phy_chainmask_cap_done;
	bool oem_dma_ring_cap_done;
	bool dma_ring_cap_done;
};

struct ath12k_wmi_svc_rdy_ext2_arg {
	u32 reg_db_version;
	u32 hw_min_max_tx_power_2ghz;
	u32 hw_min_max_tx_power_5ghz;
	u32 chwidth_num_peer_caps;
	u32 preamble_puncture_bw;
	u32 max_user_per_ppdu_ofdma;
	u32 max_user_per_ppdu_mumimo;
	u32 target_cap_flags;
	u32 eht_cap_mac_info[WMI_MAX_EHTCAP_MAC_SIZE];
	u32 max_num_linkview_peers;
	u32 max_num_msduq_supported_per_tid;
	u32 default_num_msduq_supported_per_tid;
};

struct ath12k_wmi_svc_rdy_ext2_parse {
	struct ath12k_wmi_svc_rdy_ext2_arg arg;
	struct ath12k_wmi_dma_ring_caps_parse dma_caps_parse;
	bool dma_ring_cap_done;
	bool spectral_bin_scaling_done;
	bool mac_phy_caps_ext_done;
};

struct ath12k_wmi_rdy_parse {
	u32 num_extra_mac_addr;
};

struct ath12k_wmi_dma_buf_release_arg {
	struct ath12k_wmi_dma_buf_release_fixed_params fixed;
	const struct ath12k_wmi_dma_buf_release_entry_params *buf_entry;
	const struct ath12k_wmi_dma_buf_release_meta_data_params *meta_data;
	u32 num_buf_entry;
	u32 num_meta;
	bool buf_entry_done;
	bool meta_data_done;
};

struct ath12k_wmi_tlv_policy {
	size_t min_len;
};

struct wmi_tlv_mgmt_rx_parse {
	const struct ath12k_wmi_mgmt_rx_params *fixed;
	const u8 *frame_buf;
	bool frame_buf_done;
};

static const struct ath12k_wmi_tlv_policy ath12k_wmi_tlv_policies[] = {
	[WMI_TAG_ARRAY_BYTE] = { .min_len = 0 },
	[WMI_TAG_ARRAY_UINT32] = { .min_len = 0 },
	[WMI_TAG_SERVICE_READY_EVENT] = {
		.min_len = sizeof(struct wmi_service_ready_event) },
	[WMI_TAG_SERVICE_READY_EXT_EVENT] = {
		.min_len = sizeof(struct wmi_service_ready_ext_event) },
	[WMI_TAG_SOC_MAC_PHY_HW_MODE_CAPS] = {
		.min_len = sizeof(struct ath12k_wmi_soc_mac_phy_hw_mode_caps_params) },
	[WMI_TAG_SOC_HAL_REG_CAPABILITIES] = {
		.min_len = sizeof(struct ath12k_wmi_soc_hal_reg_caps_params) },
	[WMI_TAG_VDEV_START_RESPONSE_EVENT] = {
		.min_len = sizeof(struct wmi_vdev_start_resp_event) },
	[WMI_TAG_PEER_DELETE_RESP_EVENT] = {
		.min_len = sizeof(struct wmi_peer_delete_resp_event) },
	[WMI_TAG_OFFLOAD_BCN_TX_STATUS_EVENT] = {
		.min_len = sizeof(struct wmi_bcn_tx_status_event) },
	[WMI_TAG_VDEV_STOPPED_EVENT] = {
		.min_len = sizeof(struct wmi_vdev_stopped_event) },
	[WMI_TAG_REG_CHAN_LIST_CC_EXT_EVENT] = {
		.min_len = sizeof(struct wmi_reg_chan_list_cc_ext_event) },
	[WMI_TAG_MGMT_RX_HDR] = {
		.min_len = sizeof(struct ath12k_wmi_mgmt_rx_params) },
	[WMI_TAG_MGMT_TX_COMPL_EVENT] = {
		.min_len = sizeof(struct wmi_mgmt_tx_compl_event) },
	[WMI_TAG_SCAN_EVENT] = {
		.min_len = sizeof(struct wmi_scan_event) },
	[WMI_TAG_PEER_STA_KICKOUT_EVENT] = {
		.min_len = sizeof(struct wmi_peer_sta_kickout_event) },
	[WMI_TAG_ROAM_EVENT] = {
		.min_len = sizeof(struct wmi_roam_event) },
	[WMI_TAG_CHAN_INFO_EVENT] = {
		.min_len = sizeof(struct wmi_chan_info_event) },
	[WMI_TAG_PDEV_BSS_CHAN_INFO_EVENT] = {
		.min_len = sizeof(struct wmi_pdev_bss_chan_info_event) },
	[WMI_TAG_VDEV_INSTALL_KEY_COMPLETE_EVENT] = {
		.min_len = sizeof(struct wmi_vdev_install_key_compl_event) },
	[WMI_TAG_READY_EVENT] = {
		.min_len = sizeof(struct ath12k_wmi_ready_event_min_params) },
	[WMI_TAG_SERVICE_AVAILABLE_EVENT] = {
		.min_len = sizeof(struct wmi_service_available_event) },
	[WMI_TAG_PEER_ASSOC_CONF_EVENT] = {
		.min_len = sizeof(struct wmi_peer_assoc_conf_event) },
	[WMI_TAG_PDEV_CTL_FAILSAFE_CHECK_EVENT] = {
		.min_len = sizeof(struct wmi_pdev_ctl_failsafe_chk_event) },
	[WMI_TAG_HOST_SWFDA_EVENT] = {
		.min_len = sizeof(struct wmi_fils_discovery_event) },
	[WMI_TAG_OFFLOAD_PRB_RSP_TX_STATUS_EVENT] = {
		.min_len = sizeof(struct wmi_probe_resp_tx_status_event) },
	[WMI_TAG_VDEV_DELETE_RESP_EVENT] = {
		.min_len = sizeof(struct wmi_vdev_delete_resp_event) },
};

static __le32 ath12k_wmi_tlv_hdr(u32 cmd, u32 len)
{
	return le32_encode_bits(cmd, WMI_TLV_TAG) |
		le32_encode_bits(len, WMI_TLV_LEN);
}

static __le32 ath12k_wmi_tlv_cmd_hdr(u32 cmd, u32 len)
{
	return ath12k_wmi_tlv_hdr(cmd, len - TLV_HDR_SIZE);
}

void ath12k_wmi_init_qcn9274(struct ath12k_base *ab,
			     struct ath12k_wmi_resource_config_arg *config)
{
	config->num_vdevs = ab->num_radios * TARGET_NUM_VDEVS;

	if (ab->num_radios == 2) {
		config->num_peers = TARGET_NUM_PEERS(DBS);
		config->num_tids = TARGET_NUM_TIDS(DBS);
	} else if (ab->num_radios == 3) {
		config->num_peers = TARGET_NUM_PEERS(DBS_SBS);
		config->num_tids = TARGET_NUM_TIDS(DBS_SBS);
	} else {
		/* Control should not reach here */
		config->num_peers = TARGET_NUM_PEERS(SINGLE);
		config->num_tids = TARGET_NUM_TIDS(SINGLE);
	}
	config->num_offload_peers = TARGET_NUM_OFFLD_PEERS;
	config->num_offload_reorder_buffs = TARGET_NUM_OFFLD_REORDER_BUFFS;
	config->num_peer_keys = TARGET_NUM_PEER_KEYS;
	config->ast_skid_limit = TARGET_AST_SKID_LIMIT;
	config->tx_chain_mask = (1 << ab->target_caps.num_rf_chains) - 1;
	config->rx_chain_mask = (1 << ab->target_caps.num_rf_chains) - 1;
	config->rx_timeout_pri[0] = TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[1] = TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[2] = TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[3] = TARGET_RX_TIMEOUT_HI_PRI;

	if (test_bit(ATH12K_FLAG_RAW_MODE, &ab->dev_flags))
		config->rx_decap_mode = TARGET_DECAP_MODE_RAW;
	else
		config->rx_decap_mode = TARGET_DECAP_MODE_NATIVE_WIFI;

	config->scan_max_pending_req = TARGET_SCAN_MAX_PENDING_REQS;
	config->bmiss_offload_max_vdev = TARGET_BMISS_OFFLOAD_MAX_VDEV;
	config->roam_offload_max_vdev = TARGET_ROAM_OFFLOAD_MAX_VDEV;
	config->roam_offload_max_ap_profiles = TARGET_ROAM_OFFLOAD_MAX_AP_PROFILES;
	config->num_mcast_groups = TARGET_NUM_MCAST_GROUPS;
	config->num_mcast_table_elems = TARGET_NUM_MCAST_TABLE_ELEMS;
	config->mcast2ucast_mode = TARGET_MCAST2UCAST_MODE;
	config->tx_dbg_log_size = TARGET_TX_DBG_LOG_SIZE;
	config->num_wds_entries = TARGET_NUM_WDS_ENTRIES;
	config->dma_burst_size = TARGET_DMA_BURST_SIZE;
	config->rx_skip_defrag_timeout_dup_detection_check =
		TARGET_RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK;
	config->vow_config = TARGET_VOW_CONFIG;
	config->gtk_offload_max_vdev = TARGET_GTK_OFFLOAD_MAX_VDEV;
	config->num_msdu_desc = TARGET_NUM_MSDU_DESC;
	config->beacon_tx_offload_max_vdev = ab->num_radios * TARGET_MAX_BCN_OFFLD;
	config->rx_batchmode = TARGET_RX_BATCHMODE;
	/* Indicates host supports peer map v3 and unmap v2 support */
	config->peer_map_unmap_version = 0x32;
	config->twt_ap_pdev_count = ab->num_radios;
	config->twt_ap_sta_count = 1000;
}

void ath12k_wmi_init_wcn7850(struct ath12k_base *ab,
			     struct ath12k_wmi_resource_config_arg *config)
{
	config->num_vdevs = 4;
	config->num_peers = 16;
	config->num_tids = 32;

	config->num_offload_peers = 3;
	config->num_offload_reorder_buffs = 3;
	config->num_peer_keys = TARGET_NUM_PEER_KEYS;
	config->ast_skid_limit = TARGET_AST_SKID_LIMIT;
	config->tx_chain_mask = (1 << ab->target_caps.num_rf_chains) - 1;
	config->rx_chain_mask = (1 << ab->target_caps.num_rf_chains) - 1;
	config->rx_timeout_pri[0] = TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[1] = TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[2] = TARGET_RX_TIMEOUT_LO_PRI;
	config->rx_timeout_pri[3] = TARGET_RX_TIMEOUT_HI_PRI;
	config->rx_decap_mode = TARGET_DECAP_MODE_NATIVE_WIFI;
	config->scan_max_pending_req = TARGET_SCAN_MAX_PENDING_REQS;
	config->bmiss_offload_max_vdev = TARGET_BMISS_OFFLOAD_MAX_VDEV;
	config->roam_offload_max_vdev = TARGET_ROAM_OFFLOAD_MAX_VDEV;
	config->roam_offload_max_ap_profiles = TARGET_ROAM_OFFLOAD_MAX_AP_PROFILES;
	config->num_mcast_groups = 0;
	config->num_mcast_table_elems = 0;
	config->mcast2ucast_mode = 0;
	config->tx_dbg_log_size = TARGET_TX_DBG_LOG_SIZE;
	config->num_wds_entries = 0;
	config->dma_burst_size = 0;
	config->rx_skip_defrag_timeout_dup_detection_check = 0;
	config->vow_config = TARGET_VOW_CONFIG;
	config->gtk_offload_max_vdev = 2;
	config->num_msdu_desc = 0x400;
	config->beacon_tx_offload_max_vdev = 2;
	config->rx_batchmode = TARGET_RX_BATCHMODE;

	config->peer_map_unmap_version = 0x1;
	config->use_pdev_id = 1;
	config->max_frag_entries = 0xa;
	config->num_tdls_vdevs = 0x1;
	config->num_tdls_conn_table_entries = 8;
	config->beacon_tx_offload_max_vdev = 0x2;
	config->num_multicast_filter_entries = 0x20;
	config->num_wow_filters = 0x16;
	config->num_keep_alive_pattern = 0;
}

#define PRIMAP(_hw_mode_) \
	[_hw_mode_] = _hw_mode_##_PRI

static const int ath12k_hw_mode_pri_map[] = {
	PRIMAP(WMI_HOST_HW_MODE_SINGLE),
	PRIMAP(WMI_HOST_HW_MODE_DBS),
	PRIMAP(WMI_HOST_HW_MODE_SBS_PASSIVE),
	PRIMAP(WMI_HOST_HW_MODE_SBS),
	PRIMAP(WMI_HOST_HW_MODE_DBS_SBS),
	PRIMAP(WMI_HOST_HW_MODE_DBS_OR_SBS),
	/* keep last */
	PRIMAP(WMI_HOST_HW_MODE_MAX),
};

static int
#if defined(__linux__)
ath12k_wmi_tlv_iter(struct ath12k_base *ab, const void *ptr, size_t len,
#elif defined(__FreeBSD__)
ath12k_wmi_tlv_iter(struct ath12k_base *ab, const u8 *ptr, size_t len,
#endif
		    int (*iter)(struct ath12k_base *ab, u16 tag, u16 len,
				const void *ptr, void *data),
		    void *data)
{
#if defined(__linux__)
	const void *begin = ptr;
#elif defined(__FreeBSD__)
	const u8 *begin = ptr;
#endif
	const struct wmi_tlv *tlv;
	u16 tlv_tag, tlv_len;
	int ret;

	while (len > 0) {
		if (len < sizeof(*tlv)) {
			ath12k_err(ab, "wmi tlv parse failure at byte %zd (%zu bytes left, %zu expected)\n",
				   ptr - begin, len, sizeof(*tlv));
			return -EINVAL;
		}

#if defined(__linux__)
		tlv = ptr;
#elif defined(__FreeBSD__)
		tlv = (const void *)ptr;
#endif
		tlv_tag = le32_get_bits(tlv->header, WMI_TLV_TAG);
		tlv_len = le32_get_bits(tlv->header, WMI_TLV_LEN);
		ptr += sizeof(*tlv);
		len -= sizeof(*tlv);

		if (tlv_len > len) {
			ath12k_err(ab, "wmi tlv parse failure of tag %u at byte %zd (%zu bytes left, %u expected)\n",
				   tlv_tag, ptr - begin, len, tlv_len);
			return -EINVAL;
		}

		if (tlv_tag < ARRAY_SIZE(ath12k_wmi_tlv_policies) &&
		    ath12k_wmi_tlv_policies[tlv_tag].min_len &&
		    ath12k_wmi_tlv_policies[tlv_tag].min_len > tlv_len) {
			ath12k_err(ab, "wmi tlv parse failure of tag %u at byte %zd (%u bytes is less than min length %zu)\n",
				   tlv_tag, ptr - begin, tlv_len,
				   ath12k_wmi_tlv_policies[tlv_tag].min_len);
			return -EINVAL;
		}

		ret = iter(ab, tlv_tag, tlv_len, ptr, data);
		if (ret)
			return ret;

		ptr += tlv_len;
		len -= tlv_len;
	}

	return 0;
}

static int ath12k_wmi_tlv_iter_parse(struct ath12k_base *ab, u16 tag, u16 len,
				     const void *ptr, void *data)
{
	const void **tb = data;

	if (tag < WMI_TAG_MAX)
		tb[tag] = ptr;

	return 0;
}

static int ath12k_wmi_tlv_parse(struct ath12k_base *ar, const void **tb,
				const void *ptr, size_t len)
{
	return ath12k_wmi_tlv_iter(ar, ptr, len, ath12k_wmi_tlv_iter_parse,
				   (void *)tb);
}

static const void **
ath12k_wmi_tlv_parse_alloc(struct ath12k_base *ab, const void *ptr,
			   size_t len, gfp_t gfp)
{
	const void **tb;
	int ret;

	tb = kcalloc(WMI_TAG_MAX, sizeof(*tb), gfp);
	if (!tb)
		return ERR_PTR(-ENOMEM);

	ret = ath12k_wmi_tlv_parse(ab, tb, ptr, len);
	if (ret) {
		kfree(tb);
		return ERR_PTR(ret);
	}

	return tb;
}

static int ath12k_wmi_cmd_send_nowait(struct ath12k_wmi_pdev *wmi, struct sk_buff *skb,
				      u32 cmd_id)
{
	struct ath12k_skb_cb *skb_cb = ATH12K_SKB_CB(skb);
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_cmd_hdr *cmd_hdr;
	int ret;

	if (!skb_push(skb, sizeof(struct wmi_cmd_hdr)))
		return -ENOMEM;

	cmd_hdr = (struct wmi_cmd_hdr *)skb->data;
	cmd_hdr->cmd_id = le32_encode_bits(cmd_id, WMI_CMD_HDR_CMD_ID);

	memset(skb_cb, 0, sizeof(*skb_cb));
	ret = ath12k_htc_send(&ab->htc, wmi->eid, skb);

	if (ret)
		goto err_pull;

	return 0;

err_pull:
	skb_pull(skb, sizeof(struct wmi_cmd_hdr));
	return ret;
}

int ath12k_wmi_cmd_send(struct ath12k_wmi_pdev *wmi, struct sk_buff *skb,
			u32 cmd_id)
{
	struct ath12k_wmi_base *wmi_sc = wmi->wmi_ab;
	int ret = -EOPNOTSUPP;

	might_sleep();

	wait_event_timeout(wmi_sc->tx_credits_wq, ({
		ret = ath12k_wmi_cmd_send_nowait(wmi, skb, cmd_id);

		if (ret && test_bit(ATH12K_FLAG_CRASH_FLUSH, &wmi_sc->ab->dev_flags))
			ret = -ESHUTDOWN;

		(ret != -EAGAIN);
	}), WMI_SEND_TIMEOUT_HZ);

	if (ret == -EAGAIN)
		ath12k_warn(wmi_sc->ab, "wmi command %d timeout\n", cmd_id);

	return ret;
}

static int ath12k_pull_svc_ready_ext(struct ath12k_wmi_pdev *wmi_handle,
				     const void *ptr,
				     struct ath12k_wmi_service_ext_arg *arg)
{
	const struct wmi_service_ready_ext_event *ev = ptr;
	int i;

	if (!ev)
		return -EINVAL;

	/* Move this to host based bitmap */
	arg->default_conc_scan_config_bits =
		le32_to_cpu(ev->default_conc_scan_config_bits);
	arg->default_fw_config_bits = le32_to_cpu(ev->default_fw_config_bits);
	arg->he_cap_info = le32_to_cpu(ev->he_cap_info);
	arg->mpdu_density = le32_to_cpu(ev->mpdu_density);
	arg->max_bssid_rx_filters = le32_to_cpu(ev->max_bssid_rx_filters);
	arg->ppet.numss_m1 = le32_to_cpu(ev->ppet.numss_m1);
	arg->ppet.ru_bit_mask = le32_to_cpu(ev->ppet.ru_info);

	for (i = 0; i < WMI_MAX_NUM_SS; i++)
		arg->ppet.ppet16_ppet8_ru3_ru0[i] =
			le32_to_cpu(ev->ppet.ppet16_ppet8_ru3_ru0[i]);

	return 0;
}

static int
ath12k_pull_mac_phy_cap_svc_ready_ext(struct ath12k_wmi_pdev *wmi_handle,
				      struct ath12k_wmi_svc_rdy_ext_parse *svc,
				      u8 hw_mode_id, u8 phy_id,
				      struct ath12k_pdev *pdev)
{
	const struct ath12k_wmi_mac_phy_caps_params *mac_caps;
	const struct ath12k_wmi_soc_mac_phy_hw_mode_caps_params *hw_caps = svc->hw_caps;
	const struct ath12k_wmi_hw_mode_cap_params *wmi_hw_mode_caps = svc->hw_mode_caps;
	const struct ath12k_wmi_mac_phy_caps_params *wmi_mac_phy_caps = svc->mac_phy_caps;
	struct ath12k_base *ab = wmi_handle->wmi_ab->ab;
	struct ath12k_band_cap *cap_band;
	struct ath12k_pdev_cap *pdev_cap = &pdev->cap;
	struct ath12k_fw_pdev *fw_pdev;
	u32 phy_map;
	u32 hw_idx, phy_idx = 0;
	int i;

	if (!hw_caps || !wmi_hw_mode_caps || !svc->soc_hal_reg_caps)
		return -EINVAL;

	for (hw_idx = 0; hw_idx < le32_to_cpu(hw_caps->num_hw_modes); hw_idx++) {
		if (hw_mode_id == le32_to_cpu(wmi_hw_mode_caps[hw_idx].hw_mode_id))
			break;

		phy_map = le32_to_cpu(wmi_hw_mode_caps[hw_idx].phy_id_map);
		phy_idx = fls(phy_map);
	}

	if (hw_idx == le32_to_cpu(hw_caps->num_hw_modes))
		return -EINVAL;

	phy_idx += phy_id;
	if (phy_id >= le32_to_cpu(svc->soc_hal_reg_caps->num_phy))
		return -EINVAL;

	mac_caps = wmi_mac_phy_caps + phy_idx;

	pdev->pdev_id = le32_to_cpu(mac_caps->pdev_id);
	pdev_cap->supported_bands |= le32_to_cpu(mac_caps->supported_bands);
	pdev_cap->ampdu_density = le32_to_cpu(mac_caps->ampdu_density);

	fw_pdev = &ab->fw_pdev[ab->fw_pdev_count];
	fw_pdev->supported_bands = le32_to_cpu(mac_caps->supported_bands);
	fw_pdev->pdev_id = le32_to_cpu(mac_caps->pdev_id);
	fw_pdev->phy_id = le32_to_cpu(mac_caps->phy_id);
	ab->fw_pdev_count++;

	/* Take non-zero tx/rx chainmask. If tx/rx chainmask differs from
	 * band to band for a single radio, need to see how this should be
	 * handled.
	 */
	if (le32_to_cpu(mac_caps->supported_bands) & WMI_HOST_WLAN_2G_CAP) {
		pdev_cap->tx_chain_mask = le32_to_cpu(mac_caps->tx_chain_mask_2g);
		pdev_cap->rx_chain_mask = le32_to_cpu(mac_caps->rx_chain_mask_2g);
	} else if (le32_to_cpu(mac_caps->supported_bands) & WMI_HOST_WLAN_5G_CAP) {
		pdev_cap->vht_cap = le32_to_cpu(mac_caps->vht_cap_info_5g);
		pdev_cap->vht_mcs = le32_to_cpu(mac_caps->vht_supp_mcs_5g);
		pdev_cap->he_mcs = le32_to_cpu(mac_caps->he_supp_mcs_5g);
		pdev_cap->tx_chain_mask = le32_to_cpu(mac_caps->tx_chain_mask_5g);
		pdev_cap->rx_chain_mask = le32_to_cpu(mac_caps->rx_chain_mask_5g);
	} else {
		return -EINVAL;
	}

	/* tx/rx chainmask reported from fw depends on the actual hw chains used,
	 * For example, for 4x4 capable macphys, first 4 chains can be used for first
	 * mac and the remaining 4 chains can be used for the second mac or vice-versa.
	 * In this case, tx/rx chainmask 0xf will be advertised for first mac and 0xf0
	 * will be advertised for second mac or vice-versa. Compute the shift value
	 * for tx/rx chainmask which will be used to advertise supported ht/vht rates to
	 * mac80211.
	 */
	pdev_cap->tx_chain_mask_shift =
			find_first_bit((unsigned long *)&pdev_cap->tx_chain_mask, 32);
	pdev_cap->rx_chain_mask_shift =
			find_first_bit((unsigned long *)&pdev_cap->rx_chain_mask, 32);

	if (le32_to_cpu(mac_caps->supported_bands) & WMI_HOST_WLAN_2G_CAP) {
		cap_band = &pdev_cap->band[NL80211_BAND_2GHZ];
		cap_band->phy_id = le32_to_cpu(mac_caps->phy_id);
		cap_band->max_bw_supported = le32_to_cpu(mac_caps->max_bw_supported_2g);
		cap_band->ht_cap_info = le32_to_cpu(mac_caps->ht_cap_info_2g);
		cap_band->he_cap_info[0] = le32_to_cpu(mac_caps->he_cap_info_2g);
		cap_band->he_cap_info[1] = le32_to_cpu(mac_caps->he_cap_info_2g_ext);
		cap_band->he_mcs = le32_to_cpu(mac_caps->he_supp_mcs_2g);
		for (i = 0; i < WMI_MAX_HECAP_PHY_SIZE; i++)
			cap_band->he_cap_phy_info[i] =
				le32_to_cpu(mac_caps->he_cap_phy_info_2g[i]);

		cap_band->he_ppet.numss_m1 = le32_to_cpu(mac_caps->he_ppet2g.numss_m1);
		cap_band->he_ppet.ru_bit_mask = le32_to_cpu(mac_caps->he_ppet2g.ru_info);

		for (i = 0; i < WMI_MAX_NUM_SS; i++)
			cap_band->he_ppet.ppet16_ppet8_ru3_ru0[i] =
				le32_to_cpu(mac_caps->he_ppet2g.ppet16_ppet8_ru3_ru0[i]);
	}

	if (le32_to_cpu(mac_caps->supported_bands) & WMI_HOST_WLAN_5G_CAP) {
		cap_band = &pdev_cap->band[NL80211_BAND_5GHZ];
		cap_band->phy_id = le32_to_cpu(mac_caps->phy_id);
		cap_band->max_bw_supported =
			le32_to_cpu(mac_caps->max_bw_supported_5g);
		cap_band->ht_cap_info = le32_to_cpu(mac_caps->ht_cap_info_5g);
		cap_band->he_cap_info[0] = le32_to_cpu(mac_caps->he_cap_info_5g);
		cap_band->he_cap_info[1] = le32_to_cpu(mac_caps->he_cap_info_5g_ext);
		cap_band->he_mcs = le32_to_cpu(mac_caps->he_supp_mcs_5g);
		for (i = 0; i < WMI_MAX_HECAP_PHY_SIZE; i++)
			cap_band->he_cap_phy_info[i] =
				le32_to_cpu(mac_caps->he_cap_phy_info_5g[i]);

		cap_band->he_ppet.numss_m1 = le32_to_cpu(mac_caps->he_ppet5g.numss_m1);
		cap_band->he_ppet.ru_bit_mask = le32_to_cpu(mac_caps->he_ppet5g.ru_info);

		for (i = 0; i < WMI_MAX_NUM_SS; i++)
			cap_band->he_ppet.ppet16_ppet8_ru3_ru0[i] =
				le32_to_cpu(mac_caps->he_ppet5g.ppet16_ppet8_ru3_ru0[i]);

		cap_band = &pdev_cap->band[NL80211_BAND_6GHZ];
		cap_band->max_bw_supported =
			le32_to_cpu(mac_caps->max_bw_supported_5g);
		cap_band->ht_cap_info = le32_to_cpu(mac_caps->ht_cap_info_5g);
		cap_band->he_cap_info[0] = le32_to_cpu(mac_caps->he_cap_info_5g);
		cap_band->he_cap_info[1] = le32_to_cpu(mac_caps->he_cap_info_5g_ext);
		cap_band->he_mcs = le32_to_cpu(mac_caps->he_supp_mcs_5g);
		for (i = 0; i < WMI_MAX_HECAP_PHY_SIZE; i++)
			cap_band->he_cap_phy_info[i] =
				le32_to_cpu(mac_caps->he_cap_phy_info_5g[i]);

		cap_band->he_ppet.numss_m1 = le32_to_cpu(mac_caps->he_ppet5g.numss_m1);
		cap_band->he_ppet.ru_bit_mask = le32_to_cpu(mac_caps->he_ppet5g.ru_info);

		for (i = 0; i < WMI_MAX_NUM_SS; i++)
			cap_band->he_ppet.ppet16_ppet8_ru3_ru0[i] =
				le32_to_cpu(mac_caps->he_ppet5g.ppet16_ppet8_ru3_ru0[i]);
	}

	return 0;
}

static int
ath12k_pull_reg_cap_svc_rdy_ext(struct ath12k_wmi_pdev *wmi_handle,
				const struct ath12k_wmi_soc_hal_reg_caps_params *reg_caps,
				const struct ath12k_wmi_hal_reg_caps_ext_params *ext_caps,
				u8 phy_idx,
				struct ath12k_wmi_hal_reg_capabilities_ext_arg *param)
{
	const struct ath12k_wmi_hal_reg_caps_ext_params *ext_reg_cap;

	if (!reg_caps || !ext_caps)
		return -EINVAL;

	if (phy_idx >= le32_to_cpu(reg_caps->num_phy))
		return -EINVAL;

	ext_reg_cap = &ext_caps[phy_idx];

	param->phy_id = le32_to_cpu(ext_reg_cap->phy_id);
	param->eeprom_reg_domain = le32_to_cpu(ext_reg_cap->eeprom_reg_domain);
	param->eeprom_reg_domain_ext =
		le32_to_cpu(ext_reg_cap->eeprom_reg_domain_ext);
	param->regcap1 = le32_to_cpu(ext_reg_cap->regcap1);
	param->regcap2 = le32_to_cpu(ext_reg_cap->regcap2);
	/* check if param->wireless_mode is needed */
	param->low_2ghz_chan = le32_to_cpu(ext_reg_cap->low_2ghz_chan);
	param->high_2ghz_chan = le32_to_cpu(ext_reg_cap->high_2ghz_chan);
	param->low_5ghz_chan = le32_to_cpu(ext_reg_cap->low_5ghz_chan);
	param->high_5ghz_chan = le32_to_cpu(ext_reg_cap->high_5ghz_chan);

	return 0;
}

static int ath12k_pull_service_ready_tlv(struct ath12k_base *ab,
					 const void *evt_buf,
					 struct ath12k_wmi_target_cap_arg *cap)
{
	const struct wmi_service_ready_event *ev = evt_buf;

	if (!ev) {
		ath12k_err(ab, "%s: failed by NULL param\n",
			   __func__);
		return -EINVAL;
	}

	cap->phy_capability = le32_to_cpu(ev->phy_capability);
	cap->max_frag_entry = le32_to_cpu(ev->max_frag_entry);
	cap->num_rf_chains = le32_to_cpu(ev->num_rf_chains);
	cap->ht_cap_info = le32_to_cpu(ev->ht_cap_info);
	cap->vht_cap_info = le32_to_cpu(ev->vht_cap_info);
	cap->vht_supp_mcs = le32_to_cpu(ev->vht_supp_mcs);
	cap->hw_min_tx_power = le32_to_cpu(ev->hw_min_tx_power);
	cap->hw_max_tx_power = le32_to_cpu(ev->hw_max_tx_power);
	cap->sys_cap_info = le32_to_cpu(ev->sys_cap_info);
	cap->min_pkt_size_enable = le32_to_cpu(ev->min_pkt_size_enable);
	cap->max_bcn_ie_size = le32_to_cpu(ev->max_bcn_ie_size);
	cap->max_num_scan_channels = le32_to_cpu(ev->max_num_scan_channels);
	cap->max_supported_macs = le32_to_cpu(ev->max_supported_macs);
	cap->wmi_fw_sub_feat_caps = le32_to_cpu(ev->wmi_fw_sub_feat_caps);
	cap->txrx_chainmask = le32_to_cpu(ev->txrx_chainmask);
	cap->default_dbs_hw_mode_index = le32_to_cpu(ev->default_dbs_hw_mode_index);
	cap->num_msdu_desc = le32_to_cpu(ev->num_msdu_desc);

	return 0;
}

/* Save the wmi_service_bitmap into a linear bitmap. The wmi_services in
 * wmi_service ready event are advertised in b0-b3 (LSB 4-bits) of each
 * 4-byte word.
 */
static void ath12k_wmi_service_bitmap_copy(struct ath12k_wmi_pdev *wmi,
					   const u32 *wmi_svc_bm)
{
	int i, j;

	for (i = 0, j = 0; i < WMI_SERVICE_BM_SIZE && j < WMI_MAX_SERVICE; i++) {
		do {
			if (wmi_svc_bm[i] & BIT(j % WMI_SERVICE_BITS_IN_SIZE32))
				set_bit(j, wmi->wmi_ab->svc_map);
		} while (++j % WMI_SERVICE_BITS_IN_SIZE32);
	}
}

static int ath12k_wmi_svc_rdy_parse(struct ath12k_base *ab, u16 tag, u16 len,
				    const void *ptr, void *data)
{
	struct ath12k_wmi_svc_ready_parse *svc_ready = data;
	struct ath12k_wmi_pdev *wmi_handle = &ab->wmi_ab.wmi[0];
	u16 expect_len;

	switch (tag) {
	case WMI_TAG_SERVICE_READY_EVENT:
		if (ath12k_pull_service_ready_tlv(ab, ptr, &ab->target_caps))
			return -EINVAL;
		break;

	case WMI_TAG_ARRAY_UINT32:
		if (!svc_ready->wmi_svc_bitmap_done) {
			expect_len = WMI_SERVICE_BM_SIZE * sizeof(u32);
			if (len < expect_len) {
				ath12k_warn(ab, "invalid len %d for the tag 0x%x\n",
					    len, tag);
				return -EINVAL;
			}

			ath12k_wmi_service_bitmap_copy(wmi_handle, ptr);

			svc_ready->wmi_svc_bitmap_done = true;
		}
		break;
	default:
		break;
	}

	return 0;
}

static int ath12k_service_ready_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_wmi_svc_ready_parse svc_ready = { };
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_svc_rdy_parse,
				  &svc_ready);
	if (ret) {
		ath12k_warn(ab, "failed to parse tlv %d\n", ret);
		return ret;
	}

	return 0;
}

struct sk_buff *ath12k_wmi_alloc_skb(struct ath12k_wmi_base *wmi_sc, u32 len)
{
	struct sk_buff *skb;
	struct ath12k_base *ab = wmi_sc->ab;
	u32 round_len = roundup(len, 4);

	skb = ath12k_htc_alloc_skb(ab, WMI_SKB_HEADROOM + round_len);
	if (!skb)
		return NULL;

	skb_reserve(skb, WMI_SKB_HEADROOM);
	if (!IS_ALIGNED((unsigned long)skb->data, 4))
		ath12k_warn(ab, "unaligned WMI skb data\n");

	skb_put(skb, round_len);
	memset(skb->data, 0, round_len);

	return skb;
}

int ath12k_wmi_mgmt_send(struct ath12k *ar, u32 vdev_id, u32 buf_id,
			 struct sk_buff *frame)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_mgmt_send_cmd *cmd;
	struct wmi_tlv *frame_tlv;
	struct sk_buff *skb;
	u32 buf_len;
	int ret, len;

	buf_len = min_t(int, frame->len, WMI_MGMT_SEND_DOWNLD_LEN);

	len = sizeof(*cmd) + sizeof(*frame_tlv) + roundup(buf_len, 4);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_mgmt_send_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_MGMT_TX_SEND_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->desc_id = cpu_to_le32(buf_id);
	cmd->chanfreq = 0;
	cmd->paddr_lo = cpu_to_le32(lower_32_bits(ATH12K_SKB_CB(frame)->paddr));
	cmd->paddr_hi = cpu_to_le32(upper_32_bits(ATH12K_SKB_CB(frame)->paddr));
	cmd->frame_len = cpu_to_le32(frame->len);
	cmd->buf_len = cpu_to_le32(buf_len);
	cmd->tx_params_valid = 0;

	frame_tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	frame_tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, buf_len);

	memcpy(frame_tlv->value, frame->data, buf_len);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_MGMT_TX_SEND_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_MGMT_TX_SEND_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_create(struct ath12k *ar, u8 *macaddr,
			   struct ath12k_wmi_vdev_create_arg *args)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_create_cmd *cmd;
	struct sk_buff *skb;
	struct ath12k_wmi_vdev_txrx_streams_params *txrx_streams;
	struct wmi_tlv *tlv;
	int ret, len;
#if defined(__linux__)
	void *ptr;
#elif defined(__FreeBSD__)
	u8 *ptr;
#endif

	/* It can be optimized my sending tx/rx chain configuration
	 * only for supported bands instead of always sending it for
	 * both the bands.
	 */
	len = sizeof(*cmd) + TLV_HDR_SIZE +
		(WMI_NUM_SUPPORTED_BAND_MAX * sizeof(*txrx_streams));

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_create_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_CREATE_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(args->if_id);
	cmd->vdev_type = cpu_to_le32(args->type);
	cmd->vdev_subtype = cpu_to_le32(args->subtype);
	cmd->num_cfg_txrx_streams = cpu_to_le32(WMI_NUM_SUPPORTED_BAND_MAX);
	cmd->pdev_id = cpu_to_le32(args->pdev_id);
	cmd->vdev_stats_id = cpu_to_le32(args->if_stats_id);
	ether_addr_copy(cmd->vdev_macaddr.addr, macaddr);

	ptr = skb->data + sizeof(*cmd);
	len = WMI_NUM_SUPPORTED_BAND_MAX * sizeof(*txrx_streams);

#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);

	ptr += TLV_HDR_SIZE;
#if defined(__linux__)
	txrx_streams = ptr;
#elif defined(__FreeBSD__)
	txrx_streams = (void *)ptr;
#endif
	len = sizeof(*txrx_streams);
	txrx_streams->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_TXRX_STREAMS,
							  len);
	txrx_streams->band = WMI_TPC_CHAINMASK_CONFIG_BAND_2G;
	txrx_streams->supported_tx_streams =
				 args->chains[NL80211_BAND_2GHZ].tx;
	txrx_streams->supported_rx_streams =
				 args->chains[NL80211_BAND_2GHZ].rx;

	txrx_streams++;
	txrx_streams->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_TXRX_STREAMS,
							  len);
	txrx_streams->band = WMI_TPC_CHAINMASK_CONFIG_BAND_5G;
	txrx_streams->supported_tx_streams =
				 args->chains[NL80211_BAND_5GHZ].tx;
	txrx_streams->supported_rx_streams =
				 args->chains[NL80211_BAND_5GHZ].rx;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev create: id %d type %d subtype %d macaddr %pM pdevid %d\n",
		   args->if_id, args->type, args->subtype,
		   macaddr, args->pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_CREATE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to submit WMI_VDEV_CREATE_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_delete(struct ath12k *ar, u8 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_delete_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_delete_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_DELETE_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "WMI vdev delete id %d\n", vdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_DELETE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI_VDEV_DELETE_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_stop(struct ath12k *ar, u8 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_stop_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_stop_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_STOP_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "WMI vdev stop id 0x%x\n", vdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_STOP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI_VDEV_STOP cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_down(struct ath12k *ar, u8 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_down_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_down_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_DOWN_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "WMI vdev down id 0x%x\n", vdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_DOWN_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI_VDEV_DOWN cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static void ath12k_wmi_put_wmi_channel(struct ath12k_wmi_channel_params *chan,
				       struct wmi_vdev_start_req_arg *arg)
{
	memset(chan, 0, sizeof(*chan));

	chan->mhz = cpu_to_le32(arg->freq);
	chan->band_center_freq1 = cpu_to_le32(arg->band_center_freq1);
	if (arg->mode == MODE_11AC_VHT80_80)
		chan->band_center_freq2 = cpu_to_le32(arg->band_center_freq2);
	else
		chan->band_center_freq2 = 0;

	chan->info |= le32_encode_bits(arg->mode, WMI_CHAN_INFO_MODE);
	if (arg->passive)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_PASSIVE);
	if (arg->allow_ibss)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_ADHOC_ALLOWED);
	if (arg->allow_ht)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HT);
	if (arg->allow_vht)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_VHT);
	if (arg->allow_he)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HE);
	if (arg->ht40plus)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_HT40_PLUS);
	if (arg->chan_radar)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_DFS);
	if (arg->freq2_radar)
		chan->info |= cpu_to_le32(WMI_CHAN_INFO_DFS_FREQ2);

	chan->reg_info_1 = le32_encode_bits(arg->max_power,
					    WMI_CHAN_REG_INFO1_MAX_PWR) |
		le32_encode_bits(arg->max_reg_power,
				 WMI_CHAN_REG_INFO1_MAX_REG_PWR);

	chan->reg_info_2 = le32_encode_bits(arg->max_antenna_gain,
					    WMI_CHAN_REG_INFO2_ANT_MAX) |
		le32_encode_bits(arg->max_power, WMI_CHAN_REG_INFO2_MAX_TX_PWR);
}

int ath12k_wmi_vdev_start(struct ath12k *ar, struct wmi_vdev_start_req_arg *arg,
			  bool restart)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_start_request_cmd *cmd;
	struct sk_buff *skb;
	struct ath12k_wmi_channel_params *chan;
	struct wmi_tlv *tlv;
#if defined(__linux__)
	void *ptr;
#elif defined(__FreeBSD__)
	u8 *ptr;
#endif
	int ret, len;

	if (WARN_ON(arg->ssid_len > sizeof(cmd->ssid.ssid)))
		return -EINVAL;

	len = sizeof(*cmd) + sizeof(*chan) + TLV_HDR_SIZE;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_start_request_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_START_REQUEST_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->beacon_interval = cpu_to_le32(arg->bcn_intval);
	cmd->bcn_tx_rate = cpu_to_le32(arg->bcn_tx_rate);
	cmd->dtim_period = cpu_to_le32(arg->dtim_period);
	cmd->num_noa_descriptors = cpu_to_le32(arg->num_noa_descriptors);
	cmd->preferred_rx_streams = cpu_to_le32(arg->pref_rx_streams);
	cmd->preferred_tx_streams = cpu_to_le32(arg->pref_tx_streams);
	cmd->cac_duration_ms = cpu_to_le32(arg->cac_duration_ms);
	cmd->regdomain = cpu_to_le32(arg->regdomain);
	cmd->he_ops = cpu_to_le32(arg->he_ops);
	cmd->punct_bitmap = cpu_to_le32(arg->punct_bitmap);

	if (!restart) {
		if (arg->ssid) {
			cmd->ssid.ssid_len = cpu_to_le32(arg->ssid_len);
			memcpy(cmd->ssid.ssid, arg->ssid, arg->ssid_len);
		}
		if (arg->hidden_ssid)
			cmd->flags |= cpu_to_le32(WMI_VDEV_START_HIDDEN_SSID);
		if (arg->pmf_enabled)
			cmd->flags |= cpu_to_le32(WMI_VDEV_START_PMF_ENABLED);
	}

	cmd->flags |= cpu_to_le32(WMI_VDEV_START_LDPC_RX_ENABLED);

	ptr = skb->data + sizeof(*cmd);
#if defined(__linux__)
	chan = ptr;
#elif defined(__FreeBSD__)
	chan = (void *)ptr;
#endif

	ath12k_wmi_put_wmi_channel(chan, arg);

	chan->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CHANNEL,
						  sizeof(*chan));
	ptr += sizeof(*chan);

#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, 0);

	/* Note: This is a nested TLV containing:
	 * [wmi_tlv][wmi_p2p_noa_descriptor][wmi_tlv]..
	 */

	ptr += sizeof(*tlv);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI, "vdev %s id 0x%x freq 0x%x mode 0x%x\n",
		   restart ? "restart" : "start", arg->vdev_id,
		   arg->freq, arg->mode);

	if (restart)
		ret = ath12k_wmi_cmd_send(wmi, skb,
					  WMI_VDEV_RESTART_REQUEST_CMDID);
	else
		ret = ath12k_wmi_cmd_send(wmi, skb,
					  WMI_VDEV_START_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit vdev_%s cmd\n",
			    restart ? "restart" : "start");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_up(struct ath12k *ar, u32 vdev_id, u32 aid, const u8 *bssid)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_up_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_up_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_UP_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->vdev_assoc_id = cpu_to_le32(aid);

	ether_addr_copy(cmd->vdev_bssid.addr, bssid);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI mgmt vdev up id 0x%x assoc id %d bssid %pM\n",
		   vdev_id, aid, bssid);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_UP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI_VDEV_UP cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_peer_create_cmd(struct ath12k *ar,
				    struct ath12k_wmi_peer_create_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_create_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_create_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_CREATE_CMD,
						 sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, arg->peer_addr);
	cmd->peer_type = cpu_to_le32(arg->peer_type);
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI peer create vdev_id %d peer_addr %pM\n",
		   arg->vdev_id, arg->peer_addr);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_CREATE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to submit WMI_PEER_CREATE cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_peer_delete_cmd(struct ath12k *ar,
				    const u8 *peer_addr, u8 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_delete_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_delete_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_DELETE_CMD,
						 sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->vdev_id = cpu_to_le32(vdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI peer delete vdev_id %d peer_addr %pM\n",
		   vdev_id,  peer_addr);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_DELETE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PEER_DELETE cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_pdev_set_regdomain(struct ath12k *ar,
				       struct ath12k_wmi_pdev_set_regdomain_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_set_regdomain_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_regdomain_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_REGDOMAIN_CMD,
						 sizeof(*cmd));

	cmd->reg_domain = cpu_to_le32(arg->current_rd_in_use);
	cmd->reg_domain_2g = cpu_to_le32(arg->current_rd_2g);
	cmd->reg_domain_5g = cpu_to_le32(arg->current_rd_5g);
	cmd->conformance_test_limit_2g = cpu_to_le32(arg->ctl_2g);
	cmd->conformance_test_limit_5g = cpu_to_le32(arg->ctl_5g);
	cmd->dfs_domain = cpu_to_le32(arg->dfs_domain);
	cmd->pdev_id = cpu_to_le32(arg->pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev regd rd %d rd2g %d rd5g %d domain %d pdev id %d\n",
		   arg->current_rd_in_use, arg->current_rd_2g,
		   arg->current_rd_5g, arg->dfs_domain, arg->pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_SET_REGDOMAIN_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_SET_REGDOMAIN cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_set_peer_param(struct ath12k *ar, const u8 *peer_addr,
			      u32 vdev_id, u32 param_id, u32 param_val)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_set_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_set_param_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_SET_PARAM_CMD,
						 sizeof(*cmd));
	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->param_id = cpu_to_le32(param_id);
	cmd->param_value = cpu_to_le32(param_val);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev %d peer 0x%pM set param %d value %d\n",
		   vdev_id, peer_addr, param_id, param_val);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_SET_PARAM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PEER_SET_PARAM cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_peer_flush_tids_cmd(struct ath12k *ar,
					u8 peer_addr[ETH_ALEN],
					u32 peer_tid_bitmap,
					u8 vdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_flush_tids_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_flush_tids_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_FLUSH_TIDS_CMD,
						 sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->peer_tid_bitmap = cpu_to_le32(peer_tid_bitmap);
	cmd->vdev_id = cpu_to_le32(vdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI peer flush vdev_id %d peer_addr %pM tids %08x\n",
		   vdev_id, peer_addr, peer_tid_bitmap);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_FLUSH_TIDS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PEER_FLUSH_TIDS cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_peer_rx_reorder_queue_setup(struct ath12k *ar,
					   int vdev_id, const u8 *addr,
					   dma_addr_t paddr, u8 tid,
					   u8 ba_window_size_valid,
					   u32 ba_window_size)
{
	struct wmi_peer_reorder_queue_setup_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_reorder_queue_setup_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_REORDER_QUEUE_SETUP_CMD,
						 sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, addr);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->tid = cpu_to_le32(tid);
	cmd->queue_ptr_lo = cpu_to_le32(lower_32_bits(paddr));
	cmd->queue_ptr_hi = cpu_to_le32(upper_32_bits(paddr));
	cmd->queue_no = cpu_to_le32(tid);
	cmd->ba_window_size_valid = cpu_to_le32(ba_window_size_valid);
	cmd->ba_window_size = cpu_to_le32(ba_window_size);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi rx reorder queue setup addr %pM vdev_id %d tid %d\n",
		   addr, vdev_id, tid);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb,
				  WMI_PEER_REORDER_QUEUE_SETUP_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PEER_REORDER_QUEUE_SETUP\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_rx_reord_queue_remove(struct ath12k *ar,
				 struct ath12k_wmi_rx_reorder_queue_remove_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_reorder_queue_remove_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_peer_reorder_queue_remove_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_REORDER_QUEUE_REMOVE_CMD,
						 sizeof(*cmd));

	ether_addr_copy(cmd->peer_macaddr.addr, arg->peer_macaddr);
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->tid_mask = cpu_to_le32(arg->peer_tid_bitmap);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "%s: peer_macaddr %pM vdev_id %d, tid_map %d", __func__,
		   arg->peer_macaddr, arg->vdev_id, arg->peer_tid_bitmap);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PEER_REORDER_QUEUE_REMOVE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PEER_REORDER_QUEUE_REMOVE_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_set_param(struct ath12k *ar, u32 param_id,
			      u32 param_value, u8 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_set_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_param_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_PARAM_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(pdev_id);
	cmd->param_id = cpu_to_le32(param_id);
	cmd->param_value = cpu_to_le32(param_value);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev set param %d pdev id %d value %d\n",
		   param_id, pdev_id, param_value);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_SET_PARAM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_SET_PARAM cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_set_ps_mode(struct ath12k *ar, int vdev_id, u32 enable)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_set_ps_mode_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_set_ps_mode_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_STA_POWERSAVE_MODE_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->sta_ps_mode = cpu_to_le32(enable);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev set psmode %d vdev id %d\n",
		   enable, vdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_STA_POWERSAVE_MODE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_SET_PARAM cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_suspend(struct ath12k *ar, u32 suspend_opt,
			    u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_suspend_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_suspend_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SUSPEND_CMD,
						 sizeof(*cmd));

	cmd->suspend_opt = cpu_to_le32(suspend_opt);
	cmd->pdev_id = cpu_to_le32(pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev suspend pdev_id %d\n", pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_SUSPEND_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_SUSPEND cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_resume(struct ath12k *ar, u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_resume_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_resume_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_RESUME_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev resume pdev id %d\n", pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_RESUME_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_RESUME cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

/* TODO FW Support for the cmd is not available yet.
 * Can be tested once the command and corresponding
 * event is implemented in FW
 */
int ath12k_wmi_pdev_bss_chan_info_request(struct ath12k *ar,
					  enum wmi_bss_chan_info_req_type type)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_pdev_bss_chan_info_req_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_pdev_bss_chan_info_req_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_BSS_CHAN_INFO_REQUEST,
						 sizeof(*cmd));
	cmd->req_type = cpu_to_le32(type);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI bss chan info req type %d\n", type);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_BSS_CHAN_INFO_REQUEST_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_BSS_CHAN_INFO_REQUEST cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_set_ap_ps_param_cmd(struct ath12k *ar, u8 *peer_addr,
					struct ath12k_wmi_ap_ps_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_ap_ps_peer_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_ap_ps_peer_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_AP_PS_PEER_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, peer_addr);
	cmd->param = cpu_to_le32(arg->param);
	cmd->value = cpu_to_le32(arg->value);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI set ap ps vdev id %d peer %pM param %d value %d\n",
		   arg->vdev_id, peer_addr, arg->param, arg->value);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_AP_PS_PEER_PARAM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_AP_PS_PEER_PARAM_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_set_sta_ps_param(struct ath12k *ar, u32 vdev_id,
				u32 param, u32 param_value)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_sta_powersave_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_sta_powersave_param_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_STA_POWERSAVE_PARAM_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->param = cpu_to_le32(param);
	cmd->value = cpu_to_le32(param_value);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI set sta ps vdev_id %d param %d value %d\n",
		   vdev_id, param, param_value);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_STA_POWERSAVE_PARAM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_STA_POWERSAVE_PARAM_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_force_fw_hang_cmd(struct ath12k *ar, u32 type, u32 delay_time_ms)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_force_fw_hang_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_force_fw_hang_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_FORCE_FW_HANG_CMD,
						 len);

	cmd->type = cpu_to_le32(type);
	cmd->delay_time_ms = cpu_to_le32(delay_time_ms);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_FORCE_FW_HANG_CMDID);

	if (ret) {
		ath12k_warn(ar->ab, "Failed to send WMI_FORCE_FW_HANG_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_vdev_set_param_cmd(struct ath12k *ar, u32 vdev_id,
				  u32 param_id, u32 param_value)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_set_param_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_set_param_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SET_PARAM_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->param_id = cpu_to_le32(param_id);
	cmd->param_value = cpu_to_le32(param_value);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev id 0x%x set param %d value %d\n",
		   vdev_id, param_id, param_value);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_SET_PARAM_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_SET_PARAM_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_pdev_temperature_cmd(struct ath12k *ar)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_get_pdev_temperature_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_get_pdev_temperature_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_GET_TEMPERATURE_CMD,
						 sizeof(*cmd));
	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI pdev get temperature for pdev_id %d\n", ar->pdev->pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PDEV_GET_TEMPERATURE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_PDEV_GET_TEMPERATURE cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_bcn_offload_control_cmd(struct ath12k *ar,
					    u32 vdev_id, u32 bcn_ctrl_op)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_bcn_offload_ctrl_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_bcn_offload_ctrl_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BCN_OFFLOAD_CTRL_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->bcn_ctrl_op = cpu_to_le32(bcn_ctrl_op);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI bcn ctrl offload vdev id %d ctrl_op %d\n",
		   vdev_id, bcn_ctrl_op);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_BCN_OFFLOAD_CTRL_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_BCN_OFFLOAD_CTRL_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_bcn_tmpl(struct ath12k *ar, u32 vdev_id,
			struct ieee80211_mutable_offsets *offs,
			struct sk_buff *bcn)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_bcn_tmpl_cmd *cmd;
	struct ath12k_wmi_bcn_prb_info_params *bcn_prb_info;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
#if defined(__linux__)
	void *ptr;
#elif defined(__FreeBSD__)
	u8 *ptr;
#endif
	int ret, len;
	size_t aligned_len = roundup(bcn->len, 4);

	len = sizeof(*cmd) + sizeof(*bcn_prb_info) + TLV_HDR_SIZE + aligned_len;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_bcn_tmpl_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BCN_TMPL_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->tim_ie_offset = cpu_to_le32(offs->tim_offset);
	cmd->csa_switch_count_offset = cpu_to_le32(offs->cntdwn_counter_offs[0]);
	cmd->ext_csa_switch_count_offset = cpu_to_le32(offs->cntdwn_counter_offs[1]);
	cmd->buf_len = cpu_to_le32(bcn->len);

	ptr = skb->data + sizeof(*cmd);

#if defined(__linux__)
	bcn_prb_info = ptr;
#elif defined(__FreeBSD__)
	bcn_prb_info = (void *)ptr;
#endif
	len = sizeof(*bcn_prb_info);
	bcn_prb_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BCN_PRB_INFO,
							  len);
	bcn_prb_info->caps = 0;
	bcn_prb_info->erp = 0;

	ptr += sizeof(*bcn_prb_info);

#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, aligned_len);
	memcpy(tlv->value, bcn->data, bcn->len);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_BCN_TMPL_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_BCN_TMPL_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_vdev_install_key(struct ath12k *ar,
				struct wmi_vdev_install_key_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_install_key_cmd *cmd;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	int ret, len, key_len_aligned;

	/* WMI_TAG_ARRAY_BYTE needs to be aligned with 4, the actual key
	 * length is specified in cmd->key_len.
	 */
	key_len_aligned = roundup(arg->key_len, 4);

	len = sizeof(*cmd) + TLV_HDR_SIZE + key_len_aligned;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_install_key_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_INSTALL_KEY_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, arg->macaddr);
	cmd->key_idx = cpu_to_le32(arg->key_idx);
	cmd->key_flags = cpu_to_le32(arg->key_flags);
	cmd->key_cipher = cpu_to_le32(arg->key_cipher);
	cmd->key_len = cpu_to_le32(arg->key_len);
	cmd->key_txmic_len = cpu_to_le32(arg->key_txmic_len);
	cmd->key_rxmic_len = cpu_to_le32(arg->key_rxmic_len);

	if (arg->key_rsc_counter)
		cmd->key_rsc_counter = cpu_to_le64(arg->key_rsc_counter);

	tlv = (struct wmi_tlv *)(skb->data + sizeof(*cmd));
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, key_len_aligned);
	memcpy(tlv->value, arg->key_data, arg->key_len);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev install key idx %d cipher %d len %d\n",
		   arg->key_idx, arg->key_cipher, arg->key_len);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_VDEV_INSTALL_KEY_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_INSTALL_KEY cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

static void ath12k_wmi_copy_peer_flags(struct wmi_peer_assoc_complete_cmd *cmd,
				       struct ath12k_wmi_peer_assoc_arg *arg,
				       bool hw_crypto_disabled)
{
	cmd->peer_flags = 0;
	cmd->peer_flags_ext = 0;

	if (arg->is_wme_set) {
		if (arg->qos_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_QOS);
		if (arg->apsd_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_APSD);
		if (arg->ht_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_HT);
		if (arg->bw_40)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_40MHZ);
		if (arg->bw_80)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_80MHZ);
		if (arg->bw_160)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_160MHZ);
		if (arg->bw_320)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_EXT_320MHZ);

		/* Typically if STBC is enabled for VHT it should be enabled
		 * for HT as well
		 **/
		if (arg->stbc_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_STBC);

		/* Typically if LDPC is enabled for VHT it should be enabled
		 * for HT as well
		 **/
		if (arg->ldpc_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_LDPC);

		if (arg->static_mimops_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_STATIC_MIMOPS);
		if (arg->dynamic_mimops_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_DYN_MIMOPS);
		if (arg->spatial_mux_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_SPATIAL_MUX);
		if (arg->vht_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_VHT);
		if (arg->he_flag)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_HE);
		if (arg->twt_requester)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_TWT_REQ);
		if (arg->twt_responder)
			cmd->peer_flags |= cpu_to_le32(WMI_PEER_TWT_RESP);
		if (arg->eht_flag)
			cmd->peer_flags_ext |= cpu_to_le32(WMI_PEER_EXT_EHT);
	}

	/* Suppress authorization for all AUTH modes that need 4-way handshake
	 * (during re-association).
	 * Authorization will be done for these modes on key installation.
	 */
	if (arg->auth_flag)
		cmd->peer_flags |= cpu_to_le32(WMI_PEER_AUTH);
	if (arg->need_ptk_4_way) {
		cmd->peer_flags |= cpu_to_le32(WMI_PEER_NEED_PTK_4_WAY);
		if (!hw_crypto_disabled)
			cmd->peer_flags &= cpu_to_le32(~WMI_PEER_AUTH);
	}
	if (arg->need_gtk_2_way)
		cmd->peer_flags |= cpu_to_le32(WMI_PEER_NEED_GTK_2_WAY);
	/* safe mode bypass the 4-way handshake */
	if (arg->safe_mode_enabled)
		cmd->peer_flags &= cpu_to_le32(~(WMI_PEER_NEED_PTK_4_WAY |
						 WMI_PEER_NEED_GTK_2_WAY));

	if (arg->is_pmf_enabled)
		cmd->peer_flags |= cpu_to_le32(WMI_PEER_PMF);

	/* Disable AMSDU for station transmit, if user configures it */
	/* Disable AMSDU for AP transmit to 11n Stations, if user configures
	 * it
	 * if (arg->amsdu_disable) Add after FW support
	 **/

	/* Target asserts if node is marked HT and all MCS is set to 0.
	 * Mark the node as non-HT if all the mcs rates are disabled through
	 * iwpriv
	 **/
	if (arg->peer_ht_rates.num_rates == 0)
		cmd->peer_flags &= cpu_to_le32(~WMI_PEER_HT);
}

int ath12k_wmi_send_peer_assoc_cmd(struct ath12k *ar,
				   struct ath12k_wmi_peer_assoc_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_peer_assoc_complete_cmd *cmd;
	struct ath12k_wmi_vht_rate_set_params *mcs;
	struct ath12k_wmi_he_rate_set_params *he_mcs;
	struct ath12k_wmi_eht_rate_set_params *eht_mcs;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
#if defined(__linux__)
	void *ptr;
#elif defined(__FreeBSD__)
	u8 *ptr;
#endif
	u32 peer_legacy_rates_align;
	u32 peer_ht_rates_align;
	int i, ret, len;

	peer_legacy_rates_align = roundup(arg->peer_legacy_rates.num_rates,
					  sizeof(u32));
	peer_ht_rates_align = roundup(arg->peer_ht_rates.num_rates,
				      sizeof(u32));

	len = sizeof(*cmd) +
	      TLV_HDR_SIZE + (peer_legacy_rates_align * sizeof(u8)) +
	      TLV_HDR_SIZE + (peer_ht_rates_align * sizeof(u8)) +
	      sizeof(*mcs) + TLV_HDR_SIZE +
	      (sizeof(*he_mcs) * arg->peer_he_mcs_count) +
	      TLV_HDR_SIZE + (sizeof(*eht_mcs) * arg->peer_eht_mcs_count) +
	      TLV_HDR_SIZE + TLV_HDR_SIZE;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

#if defined(__linux__)
	cmd = ptr;
#elif defined(__FreeBSD__)
	cmd = (void *)ptr;
#endif
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PEER_ASSOC_COMPLETE_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(arg->vdev_id);

	cmd->peer_new_assoc = cpu_to_le32(arg->peer_new_assoc);
	cmd->peer_associd = cpu_to_le32(arg->peer_associd);
	cmd->punct_bitmap = cpu_to_le32(arg->punct_bitmap);

	ath12k_wmi_copy_peer_flags(cmd, arg,
				   test_bit(ATH12K_FLAG_HW_CRYPTO_DISABLED,
					    &ar->ab->dev_flags));

	ether_addr_copy(cmd->peer_macaddr.addr, arg->peer_mac);

	cmd->peer_rate_caps = cpu_to_le32(arg->peer_rate_caps);
	cmd->peer_caps = cpu_to_le32(arg->peer_caps);
	cmd->peer_listen_intval = cpu_to_le32(arg->peer_listen_intval);
	cmd->peer_ht_caps = cpu_to_le32(arg->peer_ht_caps);
	cmd->peer_max_mpdu = cpu_to_le32(arg->peer_max_mpdu);
	cmd->peer_mpdu_density = cpu_to_le32(arg->peer_mpdu_density);
	cmd->peer_vht_caps = cpu_to_le32(arg->peer_vht_caps);
	cmd->peer_phymode = cpu_to_le32(arg->peer_phymode);

	/* Update 11ax capabilities */
	cmd->peer_he_cap_info = cpu_to_le32(arg->peer_he_cap_macinfo[0]);
	cmd->peer_he_cap_info_ext = cpu_to_le32(arg->peer_he_cap_macinfo[1]);
	cmd->peer_he_cap_info_internal = cpu_to_le32(arg->peer_he_cap_macinfo_internal);
	cmd->peer_he_caps_6ghz = cpu_to_le32(arg->peer_he_caps_6ghz);
	cmd->peer_he_ops = cpu_to_le32(arg->peer_he_ops);
	for (i = 0; i < WMI_MAX_HECAP_PHY_SIZE; i++)
		cmd->peer_he_cap_phy[i] =
			cpu_to_le32(arg->peer_he_cap_phyinfo[i]);
	cmd->peer_ppet.numss_m1 = cpu_to_le32(arg->peer_ppet.numss_m1);
	cmd->peer_ppet.ru_info = cpu_to_le32(arg->peer_ppet.ru_bit_mask);
	for (i = 0; i < WMI_MAX_NUM_SS; i++)
		cmd->peer_ppet.ppet16_ppet8_ru3_ru0[i] =
			cpu_to_le32(arg->peer_ppet.ppet16_ppet8_ru3_ru0[i]);

	/* Update 11be capabilities */
	memcpy_and_pad(cmd->peer_eht_cap_mac, sizeof(cmd->peer_eht_cap_mac),
		       arg->peer_eht_cap_mac, sizeof(arg->peer_eht_cap_mac),
		       0);
	memcpy_and_pad(cmd->peer_eht_cap_phy, sizeof(cmd->peer_eht_cap_phy),
		       arg->peer_eht_cap_phy, sizeof(arg->peer_eht_cap_phy),
		       0);
	memcpy_and_pad(&cmd->peer_eht_ppet, sizeof(cmd->peer_eht_ppet),
		       &arg->peer_eht_ppet, sizeof(arg->peer_eht_ppet), 0);

	/* Update peer legacy rate information */
	ptr += sizeof(*cmd);

#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, peer_legacy_rates_align);

	ptr += TLV_HDR_SIZE;

	cmd->num_peer_legacy_rates = cpu_to_le32(arg->peer_legacy_rates.num_rates);
	memcpy(ptr, arg->peer_legacy_rates.rates,
	       arg->peer_legacy_rates.num_rates);

	/* Update peer HT rate information */
	ptr += peer_legacy_rates_align;

#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, peer_ht_rates_align);
	ptr += TLV_HDR_SIZE;
	cmd->num_peer_ht_rates = cpu_to_le32(arg->peer_ht_rates.num_rates);
	memcpy(ptr, arg->peer_ht_rates.rates,
	       arg->peer_ht_rates.num_rates);

	/* VHT Rates */
	ptr += peer_ht_rates_align;

#if defined(__linux__)
	mcs = ptr;
#elif defined(__FreeBSD__)
	mcs = (void *)ptr;
#endif

	mcs->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VHT_RATE_SET,
						 sizeof(*mcs));

	cmd->peer_nss = cpu_to_le32(arg->peer_nss);

	/* Update bandwidth-NSS mapping */
	cmd->peer_bw_rxnss_override = 0;
	cmd->peer_bw_rxnss_override |= cpu_to_le32(arg->peer_bw_rxnss_override);

	if (arg->vht_capable) {
		mcs->rx_max_rate = cpu_to_le32(arg->rx_max_rate);
		mcs->rx_mcs_set = cpu_to_le32(arg->rx_mcs_set);
		mcs->tx_max_rate = cpu_to_le32(arg->tx_max_rate);
		mcs->tx_mcs_set = cpu_to_le32(arg->tx_mcs_set);
	}

	/* HE Rates */
	cmd->peer_he_mcs = cpu_to_le32(arg->peer_he_mcs_count);
	cmd->min_data_rate = cpu_to_le32(arg->min_data_rate);

	ptr += sizeof(*mcs);

	len = arg->peer_he_mcs_count * sizeof(*he_mcs);

#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;

	/* Loop through the HE rate set */
	for (i = 0; i < arg->peer_he_mcs_count; i++) {
#if defined(__linux__)
		he_mcs = ptr;
#elif defined(__FreeBSD__)
		he_mcs = (void *)ptr;
#endif
		he_mcs->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_HE_RATE_SET,
							    sizeof(*he_mcs));

		he_mcs->rx_mcs_set = cpu_to_le32(arg->peer_he_rx_mcs_set[i]);
		he_mcs->tx_mcs_set = cpu_to_le32(arg->peer_he_tx_mcs_set[i]);
		ptr += sizeof(*he_mcs);
	}

	/* MLO header tag with 0 length */
	len = 0;
#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;

	/* Loop through the EHT rate set */
	len = arg->peer_eht_mcs_count * sizeof(*eht_mcs);
#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;

	for (i = 0; i < arg->peer_eht_mcs_count; i++) {
#if defined(__linux__)
		eht_mcs = ptr;
#elif defined(__FreeBSD__)
		eht_mcs = (void *)ptr;
#endif
		eht_mcs->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_HE_RATE_SET,
							     sizeof(*eht_mcs));

		eht_mcs->rx_mcs_set = cpu_to_le32(arg->peer_eht_rx_mcs_set[i]);
		eht_mcs->tx_mcs_set = cpu_to_le32(arg->peer_eht_tx_mcs_set[i]);
		ptr += sizeof(*eht_mcs);
	}

	/* ML partner links tag with 0 length */
	len = 0;
#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi peer assoc vdev id %d assoc id %d peer mac %pM peer_flags %x rate_caps %x peer_caps %x listen_intval %d ht_caps %x max_mpdu %d nss %d phymode %d peer_mpdu_density %d vht_caps %x he cap_info %x he ops %x he cap_info_ext %x he phy %x %x %x peer_bw_rxnss_override %x peer_flags_ext %x eht mac_cap %x %x eht phy_cap %x %x %x\n",
		   cmd->vdev_id, cmd->peer_associd, arg->peer_mac,
		   cmd->peer_flags, cmd->peer_rate_caps, cmd->peer_caps,
		   cmd->peer_listen_intval, cmd->peer_ht_caps,
		   cmd->peer_max_mpdu, cmd->peer_nss, cmd->peer_phymode,
		   cmd->peer_mpdu_density,
		   cmd->peer_vht_caps, cmd->peer_he_cap_info,
		   cmd->peer_he_ops, cmd->peer_he_cap_info_ext,
		   cmd->peer_he_cap_phy[0], cmd->peer_he_cap_phy[1],
		   cmd->peer_he_cap_phy[2],
		   cmd->peer_bw_rxnss_override, cmd->peer_flags_ext,
		   cmd->peer_eht_cap_mac[0], cmd->peer_eht_cap_mac[1],
		   cmd->peer_eht_cap_phy[0], cmd->peer_eht_cap_phy[1],
		   cmd->peer_eht_cap_phy[2]);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_PEER_ASSOC_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PEER_ASSOC_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

void ath12k_wmi_start_scan_init(struct ath12k *ar,
				struct ath12k_wmi_scan_req_arg *arg)
{
	/* setup commonly used values */
	arg->scan_req_id = 1;
	arg->scan_priority = WMI_SCAN_PRIORITY_LOW;
	arg->dwell_time_active = 50;
	arg->dwell_time_active_2g = 0;
	arg->dwell_time_passive = 150;
	arg->dwell_time_active_6g = 40;
	arg->dwell_time_passive_6g = 30;
	arg->min_rest_time = 50;
	arg->max_rest_time = 500;
	arg->repeat_probe_time = 0;
	arg->probe_spacing_time = 0;
	arg->idle_time = 0;
	arg->max_scan_time = 20000;
	arg->probe_delay = 5;
	arg->notify_scan_events = WMI_SCAN_EVENT_STARTED |
				  WMI_SCAN_EVENT_COMPLETED |
				  WMI_SCAN_EVENT_BSS_CHANNEL |
				  WMI_SCAN_EVENT_FOREIGN_CHAN |
				  WMI_SCAN_EVENT_DEQUEUED;
	arg->scan_flags |= WMI_SCAN_CHAN_STAT_EVENT;
	arg->num_bssid = 1;

	/* fill bssid_list[0] with 0xff, otherwise bssid and RA will be
	 * ZEROs in probe request
	 */
	eth_broadcast_addr(arg->bssid_list[0].addr);
}

static void ath12k_wmi_copy_scan_event_cntrl_flags(struct wmi_start_scan_cmd *cmd,
						   struct ath12k_wmi_scan_req_arg *arg)
{
	/* Scan events subscription */
	if (arg->scan_ev_started)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_STARTED);
	if (arg->scan_ev_completed)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_COMPLETED);
	if (arg->scan_ev_bss_chan)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_BSS_CHANNEL);
	if (arg->scan_ev_foreign_chan)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_FOREIGN_CHAN);
	if (arg->scan_ev_dequeued)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_DEQUEUED);
	if (arg->scan_ev_preempted)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_PREEMPTED);
	if (arg->scan_ev_start_failed)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_START_FAILED);
	if (arg->scan_ev_restarted)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_RESTARTED);
	if (arg->scan_ev_foreign_chn_exit)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_FOREIGN_CHAN_EXIT);
	if (arg->scan_ev_suspended)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_SUSPENDED);
	if (arg->scan_ev_resumed)
		cmd->notify_scan_events |= cpu_to_le32(WMI_SCAN_EVENT_RESUMED);

	/** Set scan control flags */
	cmd->scan_ctrl_flags = 0;
	if (arg->scan_f_passive)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_PASSIVE);
	if (arg->scan_f_strict_passive_pch)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_STRICT_PASSIVE_ON_PCHN);
	if (arg->scan_f_promisc_mode)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FILTER_PROMISCUOS);
	if (arg->scan_f_capture_phy_err)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_CAPTURE_PHY_ERROR);
	if (arg->scan_f_half_rate)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_HALF_RATE_SUPPORT);
	if (arg->scan_f_quarter_rate)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_QUARTER_RATE_SUPPORT);
	if (arg->scan_f_cck_rates)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_CCK_RATES);
	if (arg->scan_f_ofdm_rates)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_OFDM_RATES);
	if (arg->scan_f_chan_stat_evnt)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_CHAN_STAT_EVENT);
	if (arg->scan_f_filter_prb_req)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FILTER_PROBE_REQ);
	if (arg->scan_f_bcast_probe)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_BCAST_PROBE_REQ);
	if (arg->scan_f_offchan_mgmt_tx)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_OFFCHAN_MGMT_TX);
	if (arg->scan_f_offchan_data_tx)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_OFFCHAN_DATA_TX);
	if (arg->scan_f_force_active_dfs_chn)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_FLAG_FORCE_ACTIVE_ON_DFS);
	if (arg->scan_f_add_tpc_ie_in_probe)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_TPC_IE_IN_PROBE_REQ);
	if (arg->scan_f_add_ds_ie_in_probe)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_DS_IE_IN_PROBE_REQ);
	if (arg->scan_f_add_spoofed_mac_in_probe)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_ADD_SPOOF_MAC_IN_PROBE_REQ);
	if (arg->scan_f_add_rand_seq_in_probe)
		cmd->scan_ctrl_flags |= cpu_to_le32(WMI_SCAN_RANDOM_SEQ_NO_IN_PROBE_REQ);
	if (arg->scan_f_en_ie_whitelist_in_probe)
		cmd->scan_ctrl_flags |=
			cpu_to_le32(WMI_SCAN_ENABLE_IE_WHTELIST_IN_PROBE_REQ);

	cmd->scan_ctrl_flags |= le32_encode_bits(arg->adaptive_dwell_time_mode,
						 WMI_SCAN_DWELL_MODE_MASK);
}

int ath12k_wmi_send_scan_start_cmd(struct ath12k *ar,
				   struct ath12k_wmi_scan_req_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_start_scan_cmd *cmd;
	struct ath12k_wmi_ssid_params *ssid = NULL;
	struct ath12k_wmi_mac_addr_params *bssid;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
#if defined(__linux__)
	void *ptr;
#elif defined(__FreeBSD__)
	u8 *ptr;
#endif
	int i, ret, len;
	u32 *tmp_ptr, extraie_len_with_pad = 0;
	struct ath12k_wmi_hint_short_ssid_arg *s_ssid = NULL;
	struct ath12k_wmi_hint_bssid_arg *hint_bssid = NULL;

	len = sizeof(*cmd);

	len += TLV_HDR_SIZE;
	if (arg->num_chan)
		len += arg->num_chan * sizeof(u32);

	len += TLV_HDR_SIZE;
	if (arg->num_ssids)
		len += arg->num_ssids * sizeof(*ssid);

	len += TLV_HDR_SIZE;
	if (arg->num_bssid)
		len += sizeof(*bssid) * arg->num_bssid;

	len += TLV_HDR_SIZE;
	if (arg->extraie.len)
		extraie_len_with_pad =
			roundup(arg->extraie.len, sizeof(u32));
	len += extraie_len_with_pad;

	if (arg->num_hint_bssid)
		len += TLV_HDR_SIZE +
		       arg->num_hint_bssid * sizeof(*hint_bssid);

	if (arg->num_hint_s_ssid)
		len += TLV_HDR_SIZE +
		       arg->num_hint_s_ssid * sizeof(*s_ssid);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	ptr = skb->data;

#if defined(__linux__)
	cmd = ptr;
#elif defined(__FreeBSD__)
	cmd = (void *)ptr;
#endif
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_START_SCAN_CMD,
						 sizeof(*cmd));

	cmd->scan_id = cpu_to_le32(arg->scan_id);
	cmd->scan_req_id = cpu_to_le32(arg->scan_req_id);
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->scan_priority = cpu_to_le32(arg->scan_priority);
	cmd->notify_scan_events = cpu_to_le32(arg->notify_scan_events);

	ath12k_wmi_copy_scan_event_cntrl_flags(cmd, arg);

	cmd->dwell_time_active = cpu_to_le32(arg->dwell_time_active);
	cmd->dwell_time_active_2g = cpu_to_le32(arg->dwell_time_active_2g);
	cmd->dwell_time_passive = cpu_to_le32(arg->dwell_time_passive);
	cmd->dwell_time_active_6g = cpu_to_le32(arg->dwell_time_active_6g);
	cmd->dwell_time_passive_6g = cpu_to_le32(arg->dwell_time_passive_6g);
	cmd->min_rest_time = cpu_to_le32(arg->min_rest_time);
	cmd->max_rest_time = cpu_to_le32(arg->max_rest_time);
	cmd->repeat_probe_time = cpu_to_le32(arg->repeat_probe_time);
	cmd->probe_spacing_time = cpu_to_le32(arg->probe_spacing_time);
	cmd->idle_time = cpu_to_le32(arg->idle_time);
	cmd->max_scan_time = cpu_to_le32(arg->max_scan_time);
	cmd->probe_delay = cpu_to_le32(arg->probe_delay);
	cmd->burst_duration = cpu_to_le32(arg->burst_duration);
	cmd->num_chan = cpu_to_le32(arg->num_chan);
	cmd->num_bssid = cpu_to_le32(arg->num_bssid);
	cmd->num_ssids = cpu_to_le32(arg->num_ssids);
	cmd->ie_len = cpu_to_le32(arg->extraie.len);
	cmd->n_probes = cpu_to_le32(arg->n_probes);

	ptr += sizeof(*cmd);

	len = arg->num_chan * sizeof(u32);

#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, len);
	ptr += TLV_HDR_SIZE;
	tmp_ptr = (u32 *)ptr;

	memcpy(tmp_ptr, arg->chan_list, arg->num_chan * 4);

	ptr += len;

	len = arg->num_ssids * sizeof(*ssid);
#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, len);

	ptr += TLV_HDR_SIZE;

	if (arg->num_ssids) {
#if defined(__linux__)
		ssid = ptr;
#elif defined(__FreeBSD__)
		ssid = (void *)ptr;
#endif
		for (i = 0; i < arg->num_ssids; ++i) {
			ssid->ssid_len = cpu_to_le32(arg->ssid[i].ssid_len);
			memcpy(ssid->ssid, arg->ssid[i].ssid,
			       arg->ssid[i].ssid_len);
			ssid++;
		}
	}

	ptr += (arg->num_ssids * sizeof(*ssid));
	len = arg->num_bssid * sizeof(*bssid);
#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, len);

	ptr += TLV_HDR_SIZE;
#if defined(__linux__)
	bssid = ptr;
#elif defined(__FreeBSD__)
	bssid = (void *)ptr;
#endif

	if (arg->num_bssid) {
		for (i = 0; i < arg->num_bssid; ++i) {
			ether_addr_copy(bssid->addr,
					arg->bssid_list[i].addr);
			bssid++;
		}
	}

	ptr += arg->num_bssid * sizeof(*bssid);

	len = extraie_len_with_pad;
#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, len);
	ptr += TLV_HDR_SIZE;

	if (arg->extraie.len)
		memcpy(ptr, arg->extraie.ptr,
		       arg->extraie.len);

	ptr += extraie_len_with_pad;

	if (arg->num_hint_s_ssid) {
		len = arg->num_hint_s_ssid * sizeof(*s_ssid);
#if defined(__linux__)
		tlv = ptr;
#elif defined(__FreeBSD__)
		tlv = (void *)ptr;
#endif
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, len);
		ptr += TLV_HDR_SIZE;
#if defined(__linux__)
		s_ssid = ptr;
#elif defined(__FreeBSD__)
		s_ssid = (void *)ptr;
#endif
		for (i = 0; i < arg->num_hint_s_ssid; ++i) {
			s_ssid->freq_flags = arg->hint_s_ssid[i].freq_flags;
			s_ssid->short_ssid = arg->hint_s_ssid[i].short_ssid;
			s_ssid++;
		}
		ptr += len;
	}

	if (arg->num_hint_bssid) {
		len = arg->num_hint_bssid * sizeof(struct ath12k_wmi_hint_bssid_arg);
#if defined(__linux__)
		tlv = ptr;
#elif defined(__FreeBSD__)
		tlv = (void *)ptr;
#endif
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_FIXED_STRUCT, len);
		ptr += TLV_HDR_SIZE;
#if defined(__linux__)
		hint_bssid = ptr;
#elif defined(__FreeBSD__)
		hint_bssid = (void *)ptr;
#endif
		for (i = 0; i < arg->num_hint_bssid; ++i) {
			hint_bssid->freq_flags =
				arg->hint_bssid[i].freq_flags;
			ether_addr_copy(&arg->hint_bssid[i].bssid.addr[0],
					&hint_bssid->bssid.addr[0]);
			hint_bssid++;
		}
	}

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_START_SCAN_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_START_SCAN_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_scan_stop_cmd(struct ath12k *ar,
				  struct ath12k_wmi_scan_cancel_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_stop_scan_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_stop_scan_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_STOP_SCAN_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->requestor = cpu_to_le32(arg->requester);
	cmd->scan_id = cpu_to_le32(arg->scan_id);
	cmd->pdev_id = cpu_to_le32(arg->pdev_id);
	/* stop the scan with the corresponding scan_id */
	if (arg->req_type == WLAN_SCAN_CANCEL_PDEV_ALL) {
		/* Cancelling all scans */
		cmd->req_type = cpu_to_le32(WMI_SCAN_STOP_ALL);
	} else if (arg->req_type == WLAN_SCAN_CANCEL_VDEV_ALL) {
		/* Cancelling VAP scans */
		cmd->req_type = cpu_to_le32(WMI_SCAN_STOP_VAP_ALL);
	} else if (arg->req_type == WLAN_SCAN_CANCEL_SINGLE) {
		/* Cancelling specific scan */
		cmd->req_type = WMI_SCAN_STOP_ONE;
	} else {
		ath12k_warn(ar->ab, "invalid scan cancel req_type %d",
			    arg->req_type);
		dev_kfree_skb(skb);
		return -EINVAL;
	}

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_STOP_SCAN_CMDID);
	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_STOP_SCAN_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_scan_chan_list_cmd(struct ath12k *ar,
				       struct ath12k_wmi_scan_chan_list_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_scan_chan_list_cmd *cmd;
	struct sk_buff *skb;
	struct ath12k_wmi_channel_params *chan_info;
	struct ath12k_wmi_channel_arg *channel_arg;
	struct wmi_tlv *tlv;
#if defined(__linux__)
	void *ptr;
#elif defined(__FreeBSD__)
	u8 *ptr;
#endif
	int i, ret, len;
	u16 num_send_chans, num_sends = 0, max_chan_limit = 0;
	__le32 *reg1, *reg2;

	channel_arg = &arg->channel[0];
	while (arg->nallchans) {
		len = sizeof(*cmd) + TLV_HDR_SIZE;
		max_chan_limit = (wmi->wmi_ab->max_msg_len[ar->pdev_idx] - len) /
			sizeof(*chan_info);

		num_send_chans = min(arg->nallchans, max_chan_limit);

		arg->nallchans -= num_send_chans;
		len += sizeof(*chan_info) * num_send_chans;

		skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
		if (!skb)
			return -ENOMEM;

		cmd = (struct wmi_scan_chan_list_cmd *)skb->data;
		cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SCAN_CHAN_LIST_CMD,
							 sizeof(*cmd));
		cmd->pdev_id = cpu_to_le32(arg->pdev_id);
		cmd->num_scan_chans = cpu_to_le32(num_send_chans);
		if (num_sends)
			cmd->flags |= cpu_to_le32(WMI_APPEND_TO_EXISTING_CHAN_LIST_FLAG);

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "WMI no.of chan = %d len = %d pdev_id = %d num_sends = %d\n",
			   num_send_chans, len, cmd->pdev_id, num_sends);

		ptr = skb->data + sizeof(*cmd);

		len = sizeof(*chan_info) * num_send_chans;
#if defined(__linux__)
		tlv = ptr;
#elif defined(__FreeBSD__)
		tlv = (void *)ptr;
#endif
		tlv->header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ARRAY_STRUCT,
						     len);
		ptr += TLV_HDR_SIZE;

		for (i = 0; i < num_send_chans; ++i) {
#if defined(__linux__)
			chan_info = ptr;
#elif defined(__FreeBSD__)
			chan_info = (void *)ptr;
#endif
			memset(chan_info, 0, sizeof(*chan_info));
			len = sizeof(*chan_info);
			chan_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_CHANNEL,
								       len);

			reg1 = &chan_info->reg_info_1;
			reg2 = &chan_info->reg_info_2;
			chan_info->mhz = cpu_to_le32(channel_arg->mhz);
			chan_info->band_center_freq1 = cpu_to_le32(channel_arg->cfreq1);
			chan_info->band_center_freq2 = cpu_to_le32(channel_arg->cfreq2);

			if (channel_arg->is_chan_passive)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_PASSIVE);
			if (channel_arg->allow_he)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HE);
			else if (channel_arg->allow_vht)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_VHT);
			else if (channel_arg->allow_ht)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_ALLOW_HT);
			if (channel_arg->half_rate)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_HALF_RATE);
			if (channel_arg->quarter_rate)
				chan_info->info |=
					cpu_to_le32(WMI_CHAN_INFO_QUARTER_RATE);

			if (channel_arg->psc_channel)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_PSC);

			if (channel_arg->dfs_set)
				chan_info->info |= cpu_to_le32(WMI_CHAN_INFO_DFS);

			chan_info->info |= le32_encode_bits(channel_arg->phy_mode,
							    WMI_CHAN_INFO_MODE);
			*reg1 |= le32_encode_bits(channel_arg->minpower,
						  WMI_CHAN_REG_INFO1_MIN_PWR);
			*reg1 |= le32_encode_bits(channel_arg->maxpower,
						  WMI_CHAN_REG_INFO1_MAX_PWR);
			*reg1 |= le32_encode_bits(channel_arg->maxregpower,
						  WMI_CHAN_REG_INFO1_MAX_REG_PWR);
			*reg1 |= le32_encode_bits(channel_arg->reg_class_id,
						  WMI_CHAN_REG_INFO1_REG_CLS);
			*reg2 |= le32_encode_bits(channel_arg->antennamax,
						  WMI_CHAN_REG_INFO2_ANT_MAX);

			ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
				   "WMI chan scan list chan[%d] = %u, chan_info->info %8x\n",
				   i, chan_info->mhz, chan_info->info);

			ptr += sizeof(*chan_info);

			channel_arg++;
		}

		ret = ath12k_wmi_cmd_send(wmi, skb, WMI_SCAN_CHAN_LIST_CMDID);
		if (ret) {
			ath12k_warn(ar->ab, "failed to send WMI_SCAN_CHAN_LIST cmd\n");
			dev_kfree_skb(skb);
			return ret;
		}

		num_sends++;
	}

	return 0;
}

int ath12k_wmi_send_wmm_update_cmd(struct ath12k *ar, u32 vdev_id,
				   struct wmi_wmm_params_all_arg *param)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_vdev_set_wmm_params_cmd *cmd;
	struct wmi_wmm_params *wmm_param;
	struct wmi_wmm_params_arg *wmi_wmm_arg;
	struct sk_buff *skb;
	int ret, ac;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_vdev_set_wmm_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SET_WMM_PARAMS_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->wmm_param_type = 0;

	for (ac = 0; ac < WME_NUM_AC; ac++) {
		switch (ac) {
		case WME_AC_BE:
			wmi_wmm_arg = &param->ac_be;
			break;
		case WME_AC_BK:
			wmi_wmm_arg = &param->ac_bk;
			break;
		case WME_AC_VI:
			wmi_wmm_arg = &param->ac_vi;
			break;
		case WME_AC_VO:
			wmi_wmm_arg = &param->ac_vo;
			break;
		}

		wmm_param = (struct wmi_wmm_params *)&cmd->wmm_params[ac];
		wmm_param->tlv_header =
			ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SET_WMM_PARAMS_CMD,
					       sizeof(*wmm_param));

		wmm_param->aifs = cpu_to_le32(wmi_wmm_arg->aifs);
		wmm_param->cwmin = cpu_to_le32(wmi_wmm_arg->cwmin);
		wmm_param->cwmax = cpu_to_le32(wmi_wmm_arg->cwmax);
		wmm_param->txoplimit = cpu_to_le32(wmi_wmm_arg->txop);
		wmm_param->acm = cpu_to_le32(wmi_wmm_arg->acm);
		wmm_param->no_ack = cpu_to_le32(wmi_wmm_arg->no_ack);

		ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
			   "wmi wmm set ac %d aifs %d cwmin %d cwmax %d txop %d acm %d no_ack %d\n",
			   ac, wmm_param->aifs, wmm_param->cwmin,
			   wmm_param->cwmax, wmm_param->txoplimit,
			   wmm_param->acm, wmm_param->no_ack);
	}
	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_VDEV_SET_WMM_PARAMS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_VDEV_SET_WMM_PARAMS_CMDID");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_dfs_phyerr_offload_enable_cmd(struct ath12k *ar,
						  u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_dfs_phyerr_offload_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_dfs_phyerr_offload_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_DFS_PHYERR_OFFLOAD_ENABLE_CMD,
				       sizeof(*cmd));

	cmd->pdev_id = cpu_to_le32(pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI dfs phy err offload enable pdev id %d\n", pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_DFS_PHYERR_OFFLOAD_ENABLE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_PDEV_DFS_PHYERR_OFFLOAD_ENABLE cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_delba_send(struct ath12k *ar, u32 vdev_id, const u8 *mac,
			  u32 tid, u32 initiator, u32 reason)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_delba_send_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_delba_send_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_DELBA_SEND_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, mac);
	cmd->tid = cpu_to_le32(tid);
	cmd->initiator = cpu_to_le32(initiator);
	cmd->reasoncode = cpu_to_le32(reason);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi delba send vdev_id 0x%X mac_addr %pM tid %u initiator %u reason %u\n",
		   vdev_id, mac, tid, initiator, reason);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_DELBA_SEND_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_DELBA_SEND_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_addba_set_resp(struct ath12k *ar, u32 vdev_id, const u8 *mac,
			      u32 tid, u32 status)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_addba_setresponse_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_addba_setresponse_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ADDBA_SETRESPONSE_CMD,
				       sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, mac);
	cmd->tid = cpu_to_le32(tid);
	cmd->statuscode = cpu_to_le32(status);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi addba set resp vdev_id 0x%X mac_addr %pM tid %u status %u\n",
		   vdev_id, mac, tid, status);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_ADDBA_SET_RESP_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_ADDBA_SET_RESP_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_addba_send(struct ath12k *ar, u32 vdev_id, const u8 *mac,
			  u32 tid, u32 buf_size)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_addba_send_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_addba_send_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ADDBA_SEND_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, mac);
	cmd->tid = cpu_to_le32(tid);
	cmd->buffersize = cpu_to_le32(buf_size);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi addba send vdev_id 0x%X mac_addr %pM tid %u bufsize %u\n",
		   vdev_id, mac, tid, buf_size);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_ADDBA_SEND_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_ADDBA_SEND_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_addba_clear_resp(struct ath12k *ar, u32 vdev_id, const u8 *mac)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_addba_clear_resp_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_addba_clear_resp_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ADDBA_CLEAR_RESP_CMD,
				       sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	ether_addr_copy(cmd->peer_macaddr.addr, mac);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi addba clear resp vdev_id 0x%X mac_addr %pM\n",
		   vdev_id, mac);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_ADDBA_CLEAR_RESP_CMDID);

	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_ADDBA_CLEAR_RESP_CMDID cmd\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_send_init_country_cmd(struct ath12k *ar,
				     struct ath12k_wmi_init_country_arg *arg)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_init_country_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_init_country_cmd *)skb->data;
	cmd->tlv_header =
		ath12k_wmi_tlv_cmd_hdr(WMI_TAG_SET_INIT_COUNTRY_CMD,
				       sizeof(*cmd));

	cmd->pdev_id = cpu_to_le32(ar->pdev->pdev_id);

	switch (arg->flags) {
	case ALPHA_IS_SET:
		cmd->init_cc_type = WMI_COUNTRY_INFO_TYPE_ALPHA;
		memcpy(&cmd->cc_info.alpha2, arg->cc_info.alpha2, 3);
		break;
	case CC_IS_SET:
		cmd->init_cc_type = cpu_to_le32(WMI_COUNTRY_INFO_TYPE_COUNTRY_CODE);
		cmd->cc_info.country_code =
			cpu_to_le32(arg->cc_info.country_code);
		break;
	case REGDMN_IS_SET:
		cmd->init_cc_type = cpu_to_le32(WMI_COUNTRY_INFO_TYPE_REGDOMAIN);
		cmd->cc_info.regdom_id = cpu_to_le32(arg->cc_info.regdom_id);
		break;
	default:
		ret = -EINVAL;
		goto out;
	}

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_SET_INIT_COUNTRY_CMDID);

out:
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send WMI_SET_INIT_COUNTRY CMD :%d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int
ath12k_wmi_send_twt_enable_cmd(struct ath12k *ar, u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_enable_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_enable_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_TWT_ENABLE_CMD,
						 len);
	cmd->pdev_id = cpu_to_le32(pdev_id);
	cmd->sta_cong_timer_ms = cpu_to_le32(ATH12K_TWT_DEF_STA_CONG_TIMER_MS);
	cmd->default_slot_size = cpu_to_le32(ATH12K_TWT_DEF_DEFAULT_SLOT_SIZE);
	cmd->congestion_thresh_setup =
		cpu_to_le32(ATH12K_TWT_DEF_CONGESTION_THRESH_SETUP);
	cmd->congestion_thresh_teardown =
		cpu_to_le32(ATH12K_TWT_DEF_CONGESTION_THRESH_TEARDOWN);
	cmd->congestion_thresh_critical =
		cpu_to_le32(ATH12K_TWT_DEF_CONGESTION_THRESH_CRITICAL);
	cmd->interference_thresh_teardown =
		cpu_to_le32(ATH12K_TWT_DEF_INTERFERENCE_THRESH_TEARDOWN);
	cmd->interference_thresh_setup =
		cpu_to_le32(ATH12K_TWT_DEF_INTERFERENCE_THRESH_SETUP);
	cmd->min_no_sta_setup = cpu_to_le32(ATH12K_TWT_DEF_MIN_NO_STA_SETUP);
	cmd->min_no_sta_teardown = cpu_to_le32(ATH12K_TWT_DEF_MIN_NO_STA_TEARDOWN);
	cmd->no_of_bcast_mcast_slots =
		cpu_to_le32(ATH12K_TWT_DEF_NO_OF_BCAST_MCAST_SLOTS);
	cmd->min_no_twt_slots = cpu_to_le32(ATH12K_TWT_DEF_MIN_NO_TWT_SLOTS);
	cmd->max_no_sta_twt = cpu_to_le32(ATH12K_TWT_DEF_MAX_NO_STA_TWT);
	cmd->mode_check_interval = cpu_to_le32(ATH12K_TWT_DEF_MODE_CHECK_INTERVAL);
	cmd->add_sta_slot_interval = cpu_to_le32(ATH12K_TWT_DEF_ADD_STA_SLOT_INTERVAL);
	cmd->remove_sta_slot_interval =
		cpu_to_le32(ATH12K_TWT_DEF_REMOVE_STA_SLOT_INTERVAL);
	/* TODO add MBSSID support */
	cmd->mbss_support = 0;

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_TWT_ENABLE_CMDID);
	if (ret) {
		ath12k_warn(ab, "Failed to send WMI_TWT_ENABLE_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

int
ath12k_wmi_send_twt_disable_cmd(struct ath12k *ar, u32 pdev_id)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_twt_disable_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_twt_disable_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_TWT_DISABLE_CMD,
						 len);
	cmd->pdev_id = cpu_to_le32(pdev_id);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_TWT_DISABLE_CMDID);
	if (ret) {
		ath12k_warn(ab, "Failed to send WMI_TWT_DISABLE_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

int
ath12k_wmi_send_obss_spr_cmd(struct ath12k *ar, u32 vdev_id,
			     struct ieee80211_he_obss_pd *he_obss_pd)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_obss_spatial_reuse_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_obss_spatial_reuse_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_OBSS_SPATIAL_REUSE_SET_CMD,
						 len);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->enable = cpu_to_le32(he_obss_pd->enable);
	cmd->obss_min = a_cpu_to_sle32(he_obss_pd->min_offset);
	cmd->obss_max = a_cpu_to_sle32(he_obss_pd->max_offset);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_PDEV_OBSS_PD_SPATIAL_REUSE_CMDID);
	if (ret) {
		ath12k_warn(ab,
			    "Failed to send WMI_PDEV_OBSS_PD_SPATIAL_REUSE_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_obss_color_cfg_cmd(struct ath12k *ar, u32 vdev_id,
				  u8 bss_color, u32 period,
				  bool enable)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_obss_color_collision_cfg_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_obss_color_collision_cfg_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_OBSS_COLOR_COLLISION_DET_CONFIG,
						 len);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->evt_type = enable ? cpu_to_le32(ATH12K_OBSS_COLOR_COLLISION_DETECTION) :
		cpu_to_le32(ATH12K_OBSS_COLOR_COLLISION_DETECTION_DISABLE);
	cmd->current_bss_color = cpu_to_le32(bss_color);
	cmd->detection_period_ms = cpu_to_le32(period);
	cmd->scan_period_ms = cpu_to_le32(ATH12K_BSS_COLOR_COLLISION_SCAN_PERIOD_MS);
	cmd->free_slot_expiry_time_ms = 0;
	cmd->flags = 0;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi_send_obss_color_collision_cfg id %d type %d bss_color %d detect_period %d scan_period %d\n",
		   cmd->vdev_id, cmd->evt_type, cmd->current_bss_color,
		   cmd->detection_period_ms, cmd->scan_period_ms);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_OBSS_COLOR_COLLISION_DET_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ab, "Failed to send WMI_OBSS_COLOR_COLLISION_DET_CONFIG_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_send_bss_color_change_enable_cmd(struct ath12k *ar, u32 vdev_id,
						bool enable)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct wmi_bss_color_change_enable_params_cmd *cmd;
	struct sk_buff *skb;
	int ret, len;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_bss_color_change_enable_params_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BSS_COLOR_CHANGE_ENABLE,
						 len);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->enable = enable ? cpu_to_le32(1) : 0;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "wmi_send_bss_color_change_enable id %d enable %d\n",
		   cmd->vdev_id, cmd->enable);

	ret = ath12k_wmi_cmd_send(wmi, skb,
				  WMI_BSS_COLOR_CHANGE_ENABLE_CMDID);
	if (ret) {
		ath12k_warn(ab, "Failed to send WMI_BSS_COLOR_CHANGE_ENABLE_CMDID");
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_fils_discovery_tmpl(struct ath12k *ar, u32 vdev_id,
				   struct sk_buff *tmpl)
{
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
	void *ptr;
	int ret, len;
	size_t aligned_len;
	struct wmi_fils_discovery_tmpl_cmd *cmd;

	aligned_len = roundup(tmpl->len, 4);
	len = sizeof(*cmd) + TLV_HDR_SIZE + aligned_len;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev %i set FILS discovery template\n", vdev_id);

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_fils_discovery_tmpl_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_FILS_DISCOVERY_TMPL_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->buf_len = cpu_to_le32(tmpl->len);
	ptr = skb->data + sizeof(*cmd);

	tlv = ptr;
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, aligned_len);
	memcpy(tlv->value, tmpl->data, tmpl->len);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_FILS_DISCOVERY_TMPL_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI vdev %i failed to send FILS discovery template command\n",
			    vdev_id);
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_probe_resp_tmpl(struct ath12k *ar, u32 vdev_id,
			       struct sk_buff *tmpl)
{
	struct wmi_probe_tmpl_cmd *cmd;
	struct ath12k_wmi_bcn_prb_info_params *probe_info;
	struct wmi_tlv *tlv;
	struct sk_buff *skb;
#if defined(__linux__)
	void *ptr;
#elif defined(__FreeBSD__)
	u8 *ptr;
#endif
	int ret, len;
	size_t aligned_len = roundup(tmpl->len, 4);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev %i set probe response template\n", vdev_id);

	len = sizeof(*cmd) + sizeof(*probe_info) + TLV_HDR_SIZE + aligned_len;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_probe_tmpl_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PRB_TMPL_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->buf_len = cpu_to_le32(tmpl->len);

	ptr = skb->data + sizeof(*cmd);

#if defined(__linux__)
	probe_info = ptr;
#elif defined(__FreeBSD__)
	probe_info = (void *)ptr;
#endif
	len = sizeof(*probe_info);
	probe_info->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_BCN_PRB_INFO,
							len);
	probe_info->caps = 0;
	probe_info->erp = 0;

	ptr += sizeof(*probe_info);

#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_BYTE, aligned_len);
	memcpy(tlv->value, tmpl->data, tmpl->len);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_PRB_TMPL_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI vdev %i failed to send probe response template command\n",
			    vdev_id);
		dev_kfree_skb(skb);
	}
	return ret;
}

int ath12k_wmi_fils_discovery(struct ath12k *ar, u32 vdev_id, u32 interval,
			      bool unsol_bcast_probe_resp_enabled)
{
	struct sk_buff *skb;
	int ret, len;
	struct wmi_fils_discovery_cmd *cmd;

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI vdev %i set %s interval to %u TU\n",
		   vdev_id, unsol_bcast_probe_resp_enabled ?
		   "unsolicited broadcast probe response" : "FILS discovery",
		   interval);

	len = sizeof(*cmd);
	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_fils_discovery_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_ENABLE_FILS_CMD,
						 len);
	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->interval = cpu_to_le32(interval);
	cmd->config = cpu_to_le32(unsol_bcast_probe_resp_enabled);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_ENABLE_FILS_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "WMI vdev %i failed to send FILS discovery enable/disable command\n",
			    vdev_id);
		dev_kfree_skb(skb);
	}
	return ret;
}

static void
ath12k_fill_band_to_mac_param(struct ath12k_base  *soc,
			      struct ath12k_wmi_pdev_band_arg *arg)
{
	u8 i;
	struct ath12k_wmi_hal_reg_capabilities_ext_arg *hal_reg_cap;
	struct ath12k_pdev *pdev;

	for (i = 0; i < soc->num_radios; i++) {
		pdev = &soc->pdevs[i];
		hal_reg_cap = &soc->hal_reg_cap[i];
		arg[i].pdev_id = pdev->pdev_id;

		switch (pdev->cap.supported_bands) {
		case WMI_HOST_WLAN_2G_5G_CAP:
			arg[i].start_freq = hal_reg_cap->low_2ghz_chan;
			arg[i].end_freq = hal_reg_cap->high_5ghz_chan;
			break;
		case WMI_HOST_WLAN_2G_CAP:
			arg[i].start_freq = hal_reg_cap->low_2ghz_chan;
			arg[i].end_freq = hal_reg_cap->high_2ghz_chan;
			break;
		case WMI_HOST_WLAN_5G_CAP:
			arg[i].start_freq = hal_reg_cap->low_5ghz_chan;
			arg[i].end_freq = hal_reg_cap->high_5ghz_chan;
			break;
		default:
			break;
		}
	}
}

static void
ath12k_wmi_copy_resource_config(struct ath12k_wmi_resource_config_params *wmi_cfg,
				struct ath12k_wmi_resource_config_arg *tg_cfg)
{
	wmi_cfg->num_vdevs = cpu_to_le32(tg_cfg->num_vdevs);
	wmi_cfg->num_peers = cpu_to_le32(tg_cfg->num_peers);
	wmi_cfg->num_offload_peers = cpu_to_le32(tg_cfg->num_offload_peers);
	wmi_cfg->num_offload_reorder_buffs =
		cpu_to_le32(tg_cfg->num_offload_reorder_buffs);
	wmi_cfg->num_peer_keys = cpu_to_le32(tg_cfg->num_peer_keys);
	wmi_cfg->num_tids = cpu_to_le32(tg_cfg->num_tids);
	wmi_cfg->ast_skid_limit = cpu_to_le32(tg_cfg->ast_skid_limit);
	wmi_cfg->tx_chain_mask = cpu_to_le32(tg_cfg->tx_chain_mask);
	wmi_cfg->rx_chain_mask = cpu_to_le32(tg_cfg->rx_chain_mask);
	wmi_cfg->rx_timeout_pri[0] = cpu_to_le32(tg_cfg->rx_timeout_pri[0]);
	wmi_cfg->rx_timeout_pri[1] = cpu_to_le32(tg_cfg->rx_timeout_pri[1]);
	wmi_cfg->rx_timeout_pri[2] = cpu_to_le32(tg_cfg->rx_timeout_pri[2]);
	wmi_cfg->rx_timeout_pri[3] = cpu_to_le32(tg_cfg->rx_timeout_pri[3]);
	wmi_cfg->rx_decap_mode = cpu_to_le32(tg_cfg->rx_decap_mode);
	wmi_cfg->scan_max_pending_req = cpu_to_le32(tg_cfg->scan_max_pending_req);
	wmi_cfg->bmiss_offload_max_vdev = cpu_to_le32(tg_cfg->bmiss_offload_max_vdev);
	wmi_cfg->roam_offload_max_vdev = cpu_to_le32(tg_cfg->roam_offload_max_vdev);
	wmi_cfg->roam_offload_max_ap_profiles =
		cpu_to_le32(tg_cfg->roam_offload_max_ap_profiles);
	wmi_cfg->num_mcast_groups = cpu_to_le32(tg_cfg->num_mcast_groups);
	wmi_cfg->num_mcast_table_elems = cpu_to_le32(tg_cfg->num_mcast_table_elems);
	wmi_cfg->mcast2ucast_mode = cpu_to_le32(tg_cfg->mcast2ucast_mode);
	wmi_cfg->tx_dbg_log_size = cpu_to_le32(tg_cfg->tx_dbg_log_size);
	wmi_cfg->num_wds_entries = cpu_to_le32(tg_cfg->num_wds_entries);
	wmi_cfg->dma_burst_size = cpu_to_le32(tg_cfg->dma_burst_size);
	wmi_cfg->mac_aggr_delim = cpu_to_le32(tg_cfg->mac_aggr_delim);
	wmi_cfg->rx_skip_defrag_timeout_dup_detection_check =
		cpu_to_le32(tg_cfg->rx_skip_defrag_timeout_dup_detection_check);
	wmi_cfg->vow_config = cpu_to_le32(tg_cfg->vow_config);
	wmi_cfg->gtk_offload_max_vdev = cpu_to_le32(tg_cfg->gtk_offload_max_vdev);
	wmi_cfg->num_msdu_desc = cpu_to_le32(tg_cfg->num_msdu_desc);
	wmi_cfg->max_frag_entries = cpu_to_le32(tg_cfg->max_frag_entries);
	wmi_cfg->num_tdls_vdevs = cpu_to_le32(tg_cfg->num_tdls_vdevs);
	wmi_cfg->num_tdls_conn_table_entries =
		cpu_to_le32(tg_cfg->num_tdls_conn_table_entries);
	wmi_cfg->beacon_tx_offload_max_vdev =
		cpu_to_le32(tg_cfg->beacon_tx_offload_max_vdev);
	wmi_cfg->num_multicast_filter_entries =
		cpu_to_le32(tg_cfg->num_multicast_filter_entries);
	wmi_cfg->num_wow_filters = cpu_to_le32(tg_cfg->num_wow_filters);
	wmi_cfg->num_keep_alive_pattern = cpu_to_le32(tg_cfg->num_keep_alive_pattern);
	wmi_cfg->keep_alive_pattern_size = cpu_to_le32(tg_cfg->keep_alive_pattern_size);
	wmi_cfg->max_tdls_concurrent_sleep_sta =
		cpu_to_le32(tg_cfg->max_tdls_concurrent_sleep_sta);
	wmi_cfg->max_tdls_concurrent_buffer_sta =
		cpu_to_le32(tg_cfg->max_tdls_concurrent_buffer_sta);
	wmi_cfg->wmi_send_separate = cpu_to_le32(tg_cfg->wmi_send_separate);
	wmi_cfg->num_ocb_vdevs = cpu_to_le32(tg_cfg->num_ocb_vdevs);
	wmi_cfg->num_ocb_channels = cpu_to_le32(tg_cfg->num_ocb_channels);
	wmi_cfg->num_ocb_schedules = cpu_to_le32(tg_cfg->num_ocb_schedules);
	wmi_cfg->bpf_instruction_size = cpu_to_le32(tg_cfg->bpf_instruction_size);
	wmi_cfg->max_bssid_rx_filters = cpu_to_le32(tg_cfg->max_bssid_rx_filters);
	wmi_cfg->use_pdev_id = cpu_to_le32(tg_cfg->use_pdev_id);
	wmi_cfg->flag1 = cpu_to_le32(tg_cfg->atf_config);
	wmi_cfg->peer_map_unmap_version = cpu_to_le32(tg_cfg->peer_map_unmap_version);
	wmi_cfg->sched_params = cpu_to_le32(tg_cfg->sched_params);
	wmi_cfg->twt_ap_pdev_count = cpu_to_le32(tg_cfg->twt_ap_pdev_count);
	wmi_cfg->twt_ap_sta_count = cpu_to_le32(tg_cfg->twt_ap_sta_count);
	wmi_cfg->host_service_flags = cpu_to_le32(tg_cfg->is_reg_cc_ext_event_supported <<
				WMI_RSRC_CFG_HOST_SVC_FLAG_REG_CC_EXT_SUPPORT_BIT);
}

static int ath12k_init_cmd_send(struct ath12k_wmi_pdev *wmi,
				struct ath12k_wmi_init_cmd_arg *arg)
{
	struct ath12k_base *ab = wmi->wmi_ab->ab;
	struct sk_buff *skb;
	struct wmi_init_cmd *cmd;
	struct ath12k_wmi_resource_config_params *cfg;
	struct ath12k_wmi_pdev_set_hw_mode_cmd *hw_mode;
	struct ath12k_wmi_pdev_band_to_mac_params *band_to_mac;
	struct ath12k_wmi_host_mem_chunk_params *host_mem_chunks;
	struct wmi_tlv *tlv;
	size_t ret, len;
#if defined(__linux__)
	void *ptr;
#elif defined(__FreeBSD__)
	u8 *ptr;
#endif
	u32 hw_mode_len = 0;
	u16 idx;

	if (arg->hw_mode_id != WMI_HOST_HW_MODE_MAX)
		hw_mode_len = sizeof(*hw_mode) + TLV_HDR_SIZE +
			      (arg->num_band_to_mac * sizeof(*band_to_mac));

	len = sizeof(*cmd) + TLV_HDR_SIZE + sizeof(*cfg) + hw_mode_len +
	      (arg->num_mem_chunks ? (sizeof(*host_mem_chunks) * WMI_MAX_MEM_REQS) : 0);

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_init_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_INIT_CMD,
						 sizeof(*cmd));

	ptr = skb->data + sizeof(*cmd);
#if defined(__linux__)
	cfg = ptr;
#elif defined(__FreeBSD__)
	cfg = (void *)ptr;
#endif

	ath12k_wmi_copy_resource_config(cfg, &arg->res_cfg);

	cfg->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_RESOURCE_CONFIG,
						 sizeof(*cfg));

	ptr += sizeof(*cfg);
#if defined(__linux__)
	host_mem_chunks = ptr + TLV_HDR_SIZE;
#elif defined(__FreeBSD__)
	host_mem_chunks = (void *)(ptr + TLV_HDR_SIZE);
#endif
	len = sizeof(struct ath12k_wmi_host_mem_chunk_params);

	for (idx = 0; idx < arg->num_mem_chunks; ++idx) {
		host_mem_chunks[idx].tlv_header =
			ath12k_wmi_tlv_hdr(WMI_TAG_WLAN_HOST_MEMORY_CHUNK,
					   len);

		host_mem_chunks[idx].ptr = cpu_to_le32(arg->mem_chunks[idx].paddr);
		host_mem_chunks[idx].size = cpu_to_le32(arg->mem_chunks[idx].len);
		host_mem_chunks[idx].req_id = cpu_to_le32(arg->mem_chunks[idx].req_id);

		ath12k_dbg(ab, ATH12K_DBG_WMI,
#if defined(__linux__)
			   "WMI host mem chunk req_id %d paddr 0x%llx len %d\n",
#elif defined(__FreeBSD__)
			   "WMI host mem chunk req_id %d paddr 0x%jx len %d\n",
#endif
			   arg->mem_chunks[idx].req_id,
#if defined(__linux__)
			   (u64)arg->mem_chunks[idx].paddr,
#elif defined(__FreeBSD__)
			   (uintmax_t)arg->mem_chunks[idx].paddr,
#endif
			   arg->mem_chunks[idx].len);
	}
	cmd->num_host_mem_chunks = cpu_to_le32(arg->num_mem_chunks);
	len = sizeof(struct ath12k_wmi_host_mem_chunk_params) * arg->num_mem_chunks;

	/* num_mem_chunks is zero */
#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);
	ptr += TLV_HDR_SIZE + len;

	if (arg->hw_mode_id != WMI_HOST_HW_MODE_MAX) {
		hw_mode = (struct ath12k_wmi_pdev_set_hw_mode_cmd *)ptr;
		hw_mode->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_HW_MODE_CMD,
							     sizeof(*hw_mode));

		hw_mode->hw_mode_index = cpu_to_le32(arg->hw_mode_id);
		hw_mode->num_band_to_mac = cpu_to_le32(arg->num_band_to_mac);

		ptr += sizeof(*hw_mode);

		len = arg->num_band_to_mac * sizeof(*band_to_mac);
#if defined(__linux__)
		tlv = ptr;
#elif defined(__FreeBSD__)
		tlv = (void *)ptr;
#endif
		tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_STRUCT, len);

		ptr += TLV_HDR_SIZE;
		len = sizeof(*band_to_mac);

		for (idx = 0; idx < arg->num_band_to_mac; idx++) {
			band_to_mac = (void *)ptr;

			band_to_mac->tlv_header =
				ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_BAND_TO_MAC,
						       len);
			band_to_mac->pdev_id = cpu_to_le32(arg->band_to_mac[idx].pdev_id);
			band_to_mac->start_freq =
				cpu_to_le32(arg->band_to_mac[idx].start_freq);
			band_to_mac->end_freq =
				cpu_to_le32(arg->band_to_mac[idx].end_freq);
			ptr += sizeof(*band_to_mac);
		}
	}

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_INIT_CMDID);
	if (ret) {
		ath12k_warn(ab, "failed to send WMI_INIT_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_pdev_lro_cfg(struct ath12k *ar,
			    int pdev_id)
{
	struct ath12k_wmi_pdev_lro_config_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath12k_wmi_pdev_lro_config_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_LRO_INFO_CMD,
						 sizeof(*cmd));

	get_random_bytes(cmd->th_4, sizeof(cmd->th_4));
	get_random_bytes(cmd->th_6, sizeof(cmd->th_6));

	cmd->pdev_id = cpu_to_le32(pdev_id);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI lro cfg cmd pdev_id 0x%x\n", pdev_id);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb, WMI_LRO_CONFIG_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send lro cfg req wmi cmd\n");
		goto err;
	}

	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

int ath12k_wmi_wait_for_service_ready(struct ath12k_base *ab)
{
	unsigned long time_left;

	time_left = wait_for_completion_timeout(&ab->wmi_ab.service_ready,
						WMI_SERVICE_READY_TIMEOUT_HZ);
	if (!time_left)
		return -ETIMEDOUT;

	return 0;
}

int ath12k_wmi_wait_for_unified_ready(struct ath12k_base *ab)
{
	unsigned long time_left;

	time_left = wait_for_completion_timeout(&ab->wmi_ab.unified_ready,
						WMI_SERVICE_READY_TIMEOUT_HZ);
	if (!time_left)
		return -ETIMEDOUT;

	return 0;
}

int ath12k_wmi_set_hw_mode(struct ath12k_base *ab,
			   enum wmi_host_hw_mode_config_type mode)
{
	struct ath12k_wmi_pdev_set_hw_mode_cmd *cmd;
	struct sk_buff *skb;
	struct ath12k_wmi_base *wmi_ab = &ab->wmi_ab;
	int len;
	int ret;

	len = sizeof(*cmd);

	skb = ath12k_wmi_alloc_skb(wmi_ab, len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath12k_wmi_pdev_set_hw_mode_cmd *)skb->data;

	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_PDEV_SET_HW_MODE_CMD,
						 sizeof(*cmd));

	cmd->pdev_id = WMI_PDEV_ID_SOC;
	cmd->hw_mode_index = cpu_to_le32(mode);

	ret = ath12k_wmi_cmd_send(&wmi_ab->wmi[0], skb, WMI_PDEV_SET_HW_MODE_CMDID);
	if (ret) {
		ath12k_warn(ab, "failed to send WMI_PDEV_SET_HW_MODE_CMDID\n");
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_cmd_init(struct ath12k_base *ab)
{
	struct ath12k_wmi_base *wmi_sc = &ab->wmi_ab;
	struct ath12k_wmi_init_cmd_arg arg = {};

	if (test_bit(WMI_TLV_SERVICE_REG_CC_EXT_EVENT_SUPPORT,
		     ab->wmi_ab.svc_map))
		arg.res_cfg.is_reg_cc_ext_event_supported = true;

	ab->hw_params->wmi_init(ab, &arg.res_cfg);

	arg.num_mem_chunks = wmi_sc->num_mem_chunks;
	arg.hw_mode_id = wmi_sc->preferred_hw_mode;
	arg.mem_chunks = wmi_sc->mem_chunks;

	if (ab->hw_params->single_pdev_only)
		arg.hw_mode_id = WMI_HOST_HW_MODE_MAX;

	arg.num_band_to_mac = ab->num_radios;
	ath12k_fill_band_to_mac_param(ab, arg.band_to_mac);

	return ath12k_init_cmd_send(&wmi_sc->wmi[0], &arg);
}

int ath12k_wmi_vdev_spectral_conf(struct ath12k *ar,
				  struct ath12k_wmi_vdev_spectral_conf_arg *arg)
{
	struct ath12k_wmi_vdev_spectral_conf_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath12k_wmi_vdev_spectral_conf_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SPECTRAL_CONFIGURE_CMD,
						 sizeof(*cmd));
	cmd->vdev_id = cpu_to_le32(arg->vdev_id);
	cmd->scan_count = cpu_to_le32(arg->scan_count);
	cmd->scan_period = cpu_to_le32(arg->scan_period);
	cmd->scan_priority = cpu_to_le32(arg->scan_priority);
	cmd->scan_fft_size = cpu_to_le32(arg->scan_fft_size);
	cmd->scan_gc_ena = cpu_to_le32(arg->scan_gc_ena);
	cmd->scan_restart_ena = cpu_to_le32(arg->scan_restart_ena);
	cmd->scan_noise_floor_ref = cpu_to_le32(arg->scan_noise_floor_ref);
	cmd->scan_init_delay = cpu_to_le32(arg->scan_init_delay);
	cmd->scan_nb_tone_thr = cpu_to_le32(arg->scan_nb_tone_thr);
	cmd->scan_str_bin_thr = cpu_to_le32(arg->scan_str_bin_thr);
	cmd->scan_wb_rpt_mode = cpu_to_le32(arg->scan_wb_rpt_mode);
	cmd->scan_rssi_rpt_mode = cpu_to_le32(arg->scan_rssi_rpt_mode);
	cmd->scan_rssi_thr = cpu_to_le32(arg->scan_rssi_thr);
	cmd->scan_pwr_format = cpu_to_le32(arg->scan_pwr_format);
	cmd->scan_rpt_mode = cpu_to_le32(arg->scan_rpt_mode);
	cmd->scan_bin_scale = cpu_to_le32(arg->scan_bin_scale);
	cmd->scan_dbm_adj = cpu_to_le32(arg->scan_dbm_adj);
	cmd->scan_chn_mask = cpu_to_le32(arg->scan_chn_mask);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI spectral scan config cmd vdev_id 0x%x\n",
		   arg->vdev_id);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb,
				  WMI_VDEV_SPECTRAL_SCAN_CONFIGURE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send spectral scan config wmi cmd\n");
		goto err;
	}

	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

int ath12k_wmi_vdev_spectral_enable(struct ath12k *ar, u32 vdev_id,
				    u32 trigger, u32 enable)
{
	struct ath12k_wmi_vdev_spectral_enable_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath12k_wmi_vdev_spectral_enable_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_VDEV_SPECTRAL_ENABLE_CMD,
						 sizeof(*cmd));

	cmd->vdev_id = cpu_to_le32(vdev_id);
	cmd->trigger_cmd = cpu_to_le32(trigger);
	cmd->enable_cmd = cpu_to_le32(enable);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI spectral enable cmd vdev id 0x%x\n",
		   vdev_id);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb,
				  WMI_VDEV_SPECTRAL_SCAN_ENABLE_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send spectral enable wmi cmd\n");
		goto err;
	}

	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

int ath12k_wmi_pdev_dma_ring_cfg(struct ath12k *ar,
				 struct ath12k_wmi_pdev_dma_ring_cfg_arg *arg)
{
	struct ath12k_wmi_pdev_dma_ring_cfg_req_cmd *cmd;
	struct sk_buff *skb;
	int ret;

	skb = ath12k_wmi_alloc_skb(ar->wmi->wmi_ab, sizeof(*cmd));
	if (!skb)
		return -ENOMEM;

	cmd = (struct ath12k_wmi_pdev_dma_ring_cfg_req_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_DMA_RING_CFG_REQ,
						 sizeof(*cmd));

	cmd->pdev_id = cpu_to_le32(DP_SW2HW_MACID(arg->pdev_id));
	cmd->module_id = cpu_to_le32(arg->module_id);
	cmd->base_paddr_lo = cpu_to_le32(arg->base_paddr_lo);
	cmd->base_paddr_hi = cpu_to_le32(arg->base_paddr_hi);
	cmd->head_idx_paddr_lo = cpu_to_le32(arg->head_idx_paddr_lo);
	cmd->head_idx_paddr_hi = cpu_to_le32(arg->head_idx_paddr_hi);
	cmd->tail_idx_paddr_lo = cpu_to_le32(arg->tail_idx_paddr_lo);
	cmd->tail_idx_paddr_hi = cpu_to_le32(arg->tail_idx_paddr_hi);
	cmd->num_elems = cpu_to_le32(arg->num_elems);
	cmd->buf_size = cpu_to_le32(arg->buf_size);
	cmd->num_resp_per_event = cpu_to_le32(arg->num_resp_per_event);
	cmd->event_timeout_ms = cpu_to_le32(arg->event_timeout_ms);

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI DMA ring cfg req cmd pdev_id 0x%x\n",
		   arg->pdev_id);

	ret = ath12k_wmi_cmd_send(ar->wmi, skb,
				  WMI_PDEV_DMA_RING_CFG_REQ_CMDID);
	if (ret) {
		ath12k_warn(ar->ab,
			    "failed to send dma ring cfg req wmi cmd\n");
		goto err;
	}

	return 0;
err:
	dev_kfree_skb(skb);
	return ret;
}

static int ath12k_wmi_dma_buf_entry_parse(struct ath12k_base *soc,
					  u16 tag, u16 len,
					  const void *ptr, void *data)
{
	struct ath12k_wmi_dma_buf_release_arg *arg = data;

	if (tag != WMI_TAG_DMA_BUF_RELEASE_ENTRY)
		return -EPROTO;

	if (arg->num_buf_entry >= le32_to_cpu(arg->fixed.num_buf_release_entry))
		return -ENOBUFS;

	arg->num_buf_entry++;
	return 0;
}

static int ath12k_wmi_dma_buf_meta_parse(struct ath12k_base *soc,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct ath12k_wmi_dma_buf_release_arg *arg = data;

	if (tag != WMI_TAG_DMA_BUF_RELEASE_SPECTRAL_META_DATA)
		return -EPROTO;

	if (arg->num_meta >= le32_to_cpu(arg->fixed.num_meta_data_entry))
		return -ENOBUFS;

	arg->num_meta++;

	return 0;
}

static int ath12k_wmi_dma_buf_parse(struct ath12k_base *ab,
				    u16 tag, u16 len,
				    const void *ptr, void *data)
{
	struct ath12k_wmi_dma_buf_release_arg *arg = data;
	const struct ath12k_wmi_dma_buf_release_fixed_params *fixed;
	u32 pdev_id;
	int ret;

	switch (tag) {
	case WMI_TAG_DMA_BUF_RELEASE:
		fixed = ptr;
		arg->fixed = *fixed;
		pdev_id = DP_HW2SW_MACID(le32_to_cpu(fixed->pdev_id));
		arg->fixed.pdev_id = cpu_to_le32(pdev_id);
		break;
	case WMI_TAG_ARRAY_STRUCT:
		if (!arg->buf_entry_done) {
			arg->num_buf_entry = 0;
			arg->buf_entry = ptr;

			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_dma_buf_entry_parse,
						  arg);
			if (ret) {
				ath12k_warn(ab, "failed to parse dma buf entry tlv %d\n",
					    ret);
				return ret;
			}

			arg->buf_entry_done = true;
		} else if (!arg->meta_data_done) {
			arg->num_meta = 0;
			arg->meta_data = ptr;

			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_dma_buf_meta_parse,
						  arg);
			if (ret) {
				ath12k_warn(ab, "failed to parse dma buf meta tlv %d\n",
					    ret);
				return ret;
			}

			arg->meta_data_done = true;
		}
		break;
	default:
		break;
	}
	return 0;
}

static void ath12k_wmi_pdev_dma_ring_buf_release_event(struct ath12k_base *ab,
						       struct sk_buff *skb)
{
	struct ath12k_wmi_dma_buf_release_arg arg = {};
	struct ath12k_dbring_buf_release_event param;
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_dma_buf_parse,
				  &arg);
	if (ret) {
		ath12k_warn(ab, "failed to parse dma buf release tlv %d\n", ret);
		return;
	}

	param.fixed = arg.fixed;
	param.buf_entry = arg.buf_entry;
	param.num_buf_entry = arg.num_buf_entry;
	param.meta_data = arg.meta_data;
	param.num_meta = arg.num_meta;

	ret = ath12k_dbring_buffer_release_event(ab, &param);
	if (ret) {
		ath12k_warn(ab, "failed to handle dma buf release event %d\n", ret);
		return;
	}
}

static int ath12k_wmi_hw_mode_caps_parse(struct ath12k_base *soc,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct ath12k_wmi_hw_mode_cap_params *hw_mode_cap;
	u32 phy_map = 0;

	if (tag != WMI_TAG_HW_MODE_CAPABILITIES)
		return -EPROTO;

	if (svc_rdy_ext->n_hw_mode_caps >= svc_rdy_ext->arg.num_hw_modes)
		return -ENOBUFS;

	hw_mode_cap = container_of(ptr, struct ath12k_wmi_hw_mode_cap_params,
				   hw_mode_id);
	svc_rdy_ext->n_hw_mode_caps++;

	phy_map = le32_to_cpu(hw_mode_cap->phy_id_map);
	svc_rdy_ext->tot_phy_id += fls(phy_map);

	return 0;
}

static int ath12k_wmi_hw_mode_caps(struct ath12k_base *soc,
				   u16 len, const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	const struct ath12k_wmi_hw_mode_cap_params *hw_mode_caps;
	enum wmi_host_hw_mode_config_type mode, pref;
	u32 i;
	int ret;

	svc_rdy_ext->n_hw_mode_caps = 0;
	svc_rdy_ext->hw_mode_caps = ptr;

	ret = ath12k_wmi_tlv_iter(soc, ptr, len,
				  ath12k_wmi_hw_mode_caps_parse,
				  svc_rdy_ext);
	if (ret) {
		ath12k_warn(soc, "failed to parse tlv %d\n", ret);
		return ret;
	}

	for (i = 0 ; i < svc_rdy_ext->n_hw_mode_caps; i++) {
		hw_mode_caps = &svc_rdy_ext->hw_mode_caps[i];
		mode = le32_to_cpu(hw_mode_caps->hw_mode_id);

		if (mode >= WMI_HOST_HW_MODE_MAX)
			continue;

		pref = soc->wmi_ab.preferred_hw_mode;

		if (ath12k_hw_mode_pri_map[mode] < ath12k_hw_mode_pri_map[pref]) {
			svc_rdy_ext->pref_hw_mode_caps = *hw_mode_caps;
			soc->wmi_ab.preferred_hw_mode = mode;
		}
	}

	ath12k_dbg(soc, ATH12K_DBG_WMI, "preferred_hw_mode:%d\n",
		   soc->wmi_ab.preferred_hw_mode);
	if (soc->wmi_ab.preferred_hw_mode == WMI_HOST_HW_MODE_MAX)
		return -EINVAL;

	return 0;
}

static int ath12k_wmi_mac_phy_caps_parse(struct ath12k_base *soc,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;

	if (tag != WMI_TAG_MAC_PHY_CAPABILITIES)
		return -EPROTO;

	if (svc_rdy_ext->n_mac_phy_caps >= svc_rdy_ext->tot_phy_id)
		return -ENOBUFS;

	len = min_t(u16, len, sizeof(struct ath12k_wmi_mac_phy_caps_params));
	if (!svc_rdy_ext->n_mac_phy_caps) {
		svc_rdy_ext->mac_phy_caps = kzalloc((svc_rdy_ext->tot_phy_id) * len,
						    GFP_ATOMIC);
		if (!svc_rdy_ext->mac_phy_caps)
			return -ENOMEM;
	}

	memcpy(svc_rdy_ext->mac_phy_caps + svc_rdy_ext->n_mac_phy_caps, ptr, len);
	svc_rdy_ext->n_mac_phy_caps++;
	return 0;
}

static int ath12k_wmi_ext_hal_reg_caps_parse(struct ath12k_base *soc,
					     u16 tag, u16 len,
					     const void *ptr, void *data)
{
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;

	if (tag != WMI_TAG_HAL_REG_CAPABILITIES_EXT)
		return -EPROTO;

	if (svc_rdy_ext->n_ext_hal_reg_caps >= svc_rdy_ext->arg.num_phy)
		return -ENOBUFS;

	svc_rdy_ext->n_ext_hal_reg_caps++;
	return 0;
}

static int ath12k_wmi_ext_hal_reg_caps(struct ath12k_base *soc,
				       u16 len, const void *ptr, void *data)
{
	struct ath12k_wmi_pdev *wmi_handle = &soc->wmi_ab.wmi[0];
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	struct ath12k_wmi_hal_reg_capabilities_ext_arg reg_cap;
	int ret;
	u32 i;

	svc_rdy_ext->n_ext_hal_reg_caps = 0;
	svc_rdy_ext->ext_hal_reg_caps = ptr;
	ret = ath12k_wmi_tlv_iter(soc, ptr, len,
				  ath12k_wmi_ext_hal_reg_caps_parse,
				  svc_rdy_ext);
	if (ret) {
		ath12k_warn(soc, "failed to parse tlv %d\n", ret);
		return ret;
	}

	for (i = 0; i < svc_rdy_ext->arg.num_phy; i++) {
		ret = ath12k_pull_reg_cap_svc_rdy_ext(wmi_handle,
						      svc_rdy_ext->soc_hal_reg_caps,
						      svc_rdy_ext->ext_hal_reg_caps, i,
						      &reg_cap);
		if (ret) {
			ath12k_warn(soc, "failed to extract reg cap %d\n", i);
			return ret;
		}
		soc->hal_reg_cap[reg_cap.phy_id] = reg_cap;
	}
	return 0;
}

static int ath12k_wmi_ext_soc_hal_reg_caps_parse(struct ath12k_base *soc,
						 u16 len, const void *ptr,
						 void *data)
{
	struct ath12k_wmi_pdev *wmi_handle = &soc->wmi_ab.wmi[0];
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	u8 hw_mode_id = le32_to_cpu(svc_rdy_ext->pref_hw_mode_caps.hw_mode_id);
	u32 phy_id_map;
	int pdev_index = 0;
	int ret;

	svc_rdy_ext->soc_hal_reg_caps = ptr;
	svc_rdy_ext->arg.num_phy = le32_to_cpu(svc_rdy_ext->soc_hal_reg_caps->num_phy);

	soc->num_radios = 0;
	phy_id_map = le32_to_cpu(svc_rdy_ext->pref_hw_mode_caps.phy_id_map);
	soc->fw_pdev_count = 0;

	while (phy_id_map && soc->num_radios < MAX_RADIOS) {
		ret = ath12k_pull_mac_phy_cap_svc_ready_ext(wmi_handle,
							    svc_rdy_ext,
							    hw_mode_id, soc->num_radios,
							    &soc->pdevs[pdev_index]);
		if (ret) {
			ath12k_warn(soc, "failed to extract mac caps, idx :%d\n",
				    soc->num_radios);
			return ret;
		}

		soc->num_radios++;

		/* For single_pdev_only targets,
		 * save mac_phy capability in the same pdev
		 */
		if (soc->hw_params->single_pdev_only)
			pdev_index = 0;
		else
			pdev_index = soc->num_radios;

		/* TODO: mac_phy_cap prints */
		phy_id_map >>= 1;
	}

	if (soc->hw_params->single_pdev_only) {
		soc->num_radios = 1;
		soc->pdevs[0].pdev_id = 0;
	}

	return 0;
}

static int ath12k_wmi_dma_ring_caps_parse(struct ath12k_base *soc,
					  u16 tag, u16 len,
					  const void *ptr, void *data)
{
	struct ath12k_wmi_dma_ring_caps_parse *parse = data;

	if (tag != WMI_TAG_DMA_RING_CAPABILITIES)
		return -EPROTO;

	parse->n_dma_ring_caps++;
	return 0;
}

static int ath12k_wmi_alloc_dbring_caps(struct ath12k_base *ab,
					u32 num_cap)
{
	size_t sz;
	void *ptr;

	sz = num_cap * sizeof(struct ath12k_dbring_cap);
	ptr = kzalloc(sz, GFP_ATOMIC);
	if (!ptr)
		return -ENOMEM;

	ab->db_caps = ptr;
	ab->num_db_cap = num_cap;

	return 0;
}

static void ath12k_wmi_free_dbring_caps(struct ath12k_base *ab)
{
	kfree(ab->db_caps);
	ab->db_caps = NULL;
}

static int ath12k_wmi_dma_ring_caps(struct ath12k_base *ab,
				    u16 len, const void *ptr, void *data)
{
	struct ath12k_wmi_dma_ring_caps_parse *dma_caps_parse = data;
#if defined(__linux__)
	struct ath12k_wmi_dma_ring_caps_params *dma_caps;
#elif defined(__FreeBSD__)
	const struct ath12k_wmi_dma_ring_caps_params *dma_caps;
#endif
	struct ath12k_dbring_cap *dir_buff_caps;
	int ret;
	u32 i;

	dma_caps_parse->n_dma_ring_caps = 0;
#if defined(__linux__)
	dma_caps = (struct ath12k_wmi_dma_ring_caps_params *)ptr;
#elif defined(__FreeBSD__)
	dma_caps = (const struct ath12k_wmi_dma_ring_caps_params *)ptr;
#endif
	ret = ath12k_wmi_tlv_iter(ab, ptr, len,
				  ath12k_wmi_dma_ring_caps_parse,
				  dma_caps_parse);
	if (ret) {
		ath12k_warn(ab, "failed to parse dma ring caps tlv %d\n", ret);
		return ret;
	}

	if (!dma_caps_parse->n_dma_ring_caps)
		return 0;

	if (ab->num_db_cap) {
		ath12k_warn(ab, "Already processed, so ignoring dma ring caps\n");
		return 0;
	}

	ret = ath12k_wmi_alloc_dbring_caps(ab, dma_caps_parse->n_dma_ring_caps);
	if (ret)
		return ret;

	dir_buff_caps = ab->db_caps;
	for (i = 0; i < dma_caps_parse->n_dma_ring_caps; i++) {
		if (le32_to_cpu(dma_caps[i].module_id) >= WMI_DIRECT_BUF_MAX) {
			ath12k_warn(ab, "Invalid module id %d\n",
				    le32_to_cpu(dma_caps[i].module_id));
			ret = -EINVAL;
			goto free_dir_buff;
		}

		dir_buff_caps[i].id = le32_to_cpu(dma_caps[i].module_id);
		dir_buff_caps[i].pdev_id =
			DP_HW2SW_MACID(le32_to_cpu(dma_caps[i].pdev_id));
		dir_buff_caps[i].min_elem = le32_to_cpu(dma_caps[i].min_elem);
		dir_buff_caps[i].min_buf_sz = le32_to_cpu(dma_caps[i].min_buf_sz);
		dir_buff_caps[i].min_buf_align = le32_to_cpu(dma_caps[i].min_buf_align);
	}

	return 0;

free_dir_buff:
	ath12k_wmi_free_dbring_caps(ab);
	return ret;
}

static int ath12k_wmi_svc_rdy_ext_parse(struct ath12k_base *ab,
					u16 tag, u16 len,
					const void *ptr, void *data)
{
	struct ath12k_wmi_pdev *wmi_handle = &ab->wmi_ab.wmi[0];
	struct ath12k_wmi_svc_rdy_ext_parse *svc_rdy_ext = data;
	int ret;

	switch (tag) {
	case WMI_TAG_SERVICE_READY_EXT_EVENT:
		ret = ath12k_pull_svc_ready_ext(wmi_handle, ptr,
						&svc_rdy_ext->arg);
		if (ret) {
			ath12k_warn(ab, "unable to extract ext params\n");
			return ret;
		}
		break;

	case WMI_TAG_SOC_MAC_PHY_HW_MODE_CAPS:
		svc_rdy_ext->hw_caps = ptr;
		svc_rdy_ext->arg.num_hw_modes =
			le32_to_cpu(svc_rdy_ext->hw_caps->num_hw_modes);
		break;

	case WMI_TAG_SOC_HAL_REG_CAPABILITIES:
		ret = ath12k_wmi_ext_soc_hal_reg_caps_parse(ab, len, ptr,
							    svc_rdy_ext);
		if (ret)
			return ret;
		break;

	case WMI_TAG_ARRAY_STRUCT:
		if (!svc_rdy_ext->hw_mode_done) {
			ret = ath12k_wmi_hw_mode_caps(ab, len, ptr, svc_rdy_ext);
			if (ret)
				return ret;

			svc_rdy_ext->hw_mode_done = true;
		} else if (!svc_rdy_ext->mac_phy_done) {
			svc_rdy_ext->n_mac_phy_caps = 0;
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_mac_phy_caps_parse,
						  svc_rdy_ext);
			if (ret) {
				ath12k_warn(ab, "failed to parse tlv %d\n", ret);
				return ret;
			}

			svc_rdy_ext->mac_phy_done = true;
		} else if (!svc_rdy_ext->ext_hal_reg_done) {
			ret = ath12k_wmi_ext_hal_reg_caps(ab, len, ptr, svc_rdy_ext);
			if (ret)
				return ret;

			svc_rdy_ext->ext_hal_reg_done = true;
		} else if (!svc_rdy_ext->mac_phy_chainmask_combo_done) {
			svc_rdy_ext->mac_phy_chainmask_combo_done = true;
		} else if (!svc_rdy_ext->mac_phy_chainmask_cap_done) {
			svc_rdy_ext->mac_phy_chainmask_cap_done = true;
		} else if (!svc_rdy_ext->oem_dma_ring_cap_done) {
			svc_rdy_ext->oem_dma_ring_cap_done = true;
		} else if (!svc_rdy_ext->dma_ring_cap_done) {
			ret = ath12k_wmi_dma_ring_caps(ab, len, ptr,
						       &svc_rdy_ext->dma_caps_parse);
			if (ret)
				return ret;

			svc_rdy_ext->dma_ring_cap_done = true;
		}
		break;

	default:
		break;
	}
	return 0;
}

static int ath12k_service_ready_ext_event(struct ath12k_base *ab,
					  struct sk_buff *skb)
{
	struct ath12k_wmi_svc_rdy_ext_parse svc_rdy_ext = { };
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_svc_rdy_ext_parse,
				  &svc_rdy_ext);
	if (ret) {
		ath12k_warn(ab, "failed to parse tlv %d\n", ret);
		goto err;
	}

	if (!test_bit(WMI_TLV_SERVICE_EXT2_MSG, ab->wmi_ab.svc_map))
		complete(&ab->wmi_ab.service_ready);

	kfree(svc_rdy_ext.mac_phy_caps);
	return 0;

err:
	ath12k_wmi_free_dbring_caps(ab);
	return ret;
}

static int ath12k_pull_svc_ready_ext2(struct ath12k_wmi_pdev *wmi_handle,
				      const void *ptr,
				      struct ath12k_wmi_svc_rdy_ext2_arg *arg)
{
	const struct wmi_service_ready_ext2_event *ev = ptr;

	if (!ev)
		return -EINVAL;

	arg->reg_db_version = le32_to_cpu(ev->reg_db_version);
	arg->hw_min_max_tx_power_2ghz = le32_to_cpu(ev->hw_min_max_tx_power_2ghz);
	arg->hw_min_max_tx_power_5ghz = le32_to_cpu(ev->hw_min_max_tx_power_5ghz);
	arg->chwidth_num_peer_caps = le32_to_cpu(ev->chwidth_num_peer_caps);
	arg->preamble_puncture_bw = le32_to_cpu(ev->preamble_puncture_bw);
	arg->max_user_per_ppdu_ofdma = le32_to_cpu(ev->max_user_per_ppdu_ofdma);
	arg->max_user_per_ppdu_mumimo = le32_to_cpu(ev->max_user_per_ppdu_mumimo);
	arg->target_cap_flags = le32_to_cpu(ev->target_cap_flags);
	return 0;
}

static void ath12k_wmi_eht_caps_parse(struct ath12k_pdev *pdev, u32 band,
				      const __le32 cap_mac_info[],
				      const __le32 cap_phy_info[],
				      const __le32 supp_mcs[],
				      const struct ath12k_wmi_ppe_threshold_params *ppet,
				       __le32 cap_info_internal)
{
	struct ath12k_band_cap *cap_band = &pdev->cap.band[band];
	u8 i;

	for (i = 0; i < WMI_MAX_EHTCAP_MAC_SIZE; i++)
		cap_band->eht_cap_mac_info[i] = le32_to_cpu(cap_mac_info[i]);

	for (i = 0; i < WMI_MAX_EHTCAP_PHY_SIZE; i++)
		cap_band->eht_cap_phy_info[i] = le32_to_cpu(cap_phy_info[i]);

	cap_band->eht_mcs_20_only = le32_to_cpu(supp_mcs[0]);
	cap_band->eht_mcs_80 = le32_to_cpu(supp_mcs[1]);
	if (band != NL80211_BAND_2GHZ) {
		cap_band->eht_mcs_160 = le32_to_cpu(supp_mcs[2]);
		cap_band->eht_mcs_320 = le32_to_cpu(supp_mcs[3]);
	}

	cap_band->eht_ppet.numss_m1 = le32_to_cpu(ppet->numss_m1);
	cap_band->eht_ppet.ru_bit_mask = le32_to_cpu(ppet->ru_info);
	for (i = 0; i < WMI_MAX_NUM_SS; i++)
		cap_band->eht_ppet.ppet16_ppet8_ru3_ru0[i] =
			le32_to_cpu(ppet->ppet16_ppet8_ru3_ru0[i]);

	cap_band->eht_cap_info_internal = le32_to_cpu(cap_info_internal);
}

static int
ath12k_wmi_tlv_mac_phy_caps_ext_parse(struct ath12k_base *ab,
				      const struct ath12k_wmi_caps_ext_params *caps,
				      struct ath12k_pdev *pdev)
{
	u32 bands;
	int i;

	if (ab->hw_params->single_pdev_only) {
		for (i = 0; i < ab->fw_pdev_count; i++) {
			struct ath12k_fw_pdev *fw_pdev = &ab->fw_pdev[i];

			if (fw_pdev->pdev_id == le32_to_cpu(caps->pdev_id) &&
			    fw_pdev->phy_id == le32_to_cpu(caps->phy_id)) {
				bands = fw_pdev->supported_bands;
				break;
			}
		}

		if (i == ab->fw_pdev_count)
			return -EINVAL;
	} else {
		bands = pdev->cap.supported_bands;
	}

	if (bands & WMI_HOST_WLAN_2G_CAP) {
		ath12k_wmi_eht_caps_parse(pdev, NL80211_BAND_2GHZ,
					  caps->eht_cap_mac_info_2ghz,
					  caps->eht_cap_phy_info_2ghz,
					  caps->eht_supp_mcs_ext_2ghz,
					  &caps->eht_ppet_2ghz,
					  caps->eht_cap_info_internal);
	}

	if (bands & WMI_HOST_WLAN_5G_CAP) {
		ath12k_wmi_eht_caps_parse(pdev, NL80211_BAND_5GHZ,
					  caps->eht_cap_mac_info_5ghz,
					  caps->eht_cap_phy_info_5ghz,
					  caps->eht_supp_mcs_ext_5ghz,
					  &caps->eht_ppet_5ghz,
					  caps->eht_cap_info_internal);

		ath12k_wmi_eht_caps_parse(pdev, NL80211_BAND_6GHZ,
					  caps->eht_cap_mac_info_5ghz,
					  caps->eht_cap_phy_info_5ghz,
					  caps->eht_supp_mcs_ext_5ghz,
					  &caps->eht_ppet_5ghz,
					  caps->eht_cap_info_internal);
	}

	return 0;
}

static int ath12k_wmi_tlv_mac_phy_caps_ext(struct ath12k_base *ab, u16 tag,
					   u16 len, const void *ptr,
					   void *data)
{
	const struct ath12k_wmi_caps_ext_params *caps = ptr;
	int i = 0, ret;

	if (tag != WMI_TAG_MAC_PHY_CAPABILITIES_EXT)
		return -EPROTO;

	if (ab->hw_params->single_pdev_only) {
		if (ab->wmi_ab.preferred_hw_mode != le32_to_cpu(caps->hw_mode_id))
			return 0;
	} else {
		for (i = 0; i < ab->num_radios; i++) {
			if (ab->pdevs[i].pdev_id == le32_to_cpu(caps->pdev_id))
				break;
		}

		if (i == ab->num_radios)
			return -EINVAL;
	}

	ret = ath12k_wmi_tlv_mac_phy_caps_ext_parse(ab, caps, &ab->pdevs[i]);
	if (ret) {
		ath12k_warn(ab,
			    "failed to parse extended MAC PHY capabilities for pdev %d: %d\n",
			    ret, ab->pdevs[i].pdev_id);
		return ret;
	}

	return 0;
}

static int ath12k_wmi_svc_rdy_ext2_parse(struct ath12k_base *ab,
					 u16 tag, u16 len,
					 const void *ptr, void *data)
{
	struct ath12k_wmi_pdev *wmi_handle = &ab->wmi_ab.wmi[0];
	struct ath12k_wmi_svc_rdy_ext2_parse *parse = data;
	int ret;

	switch (tag) {
	case WMI_TAG_SERVICE_READY_EXT2_EVENT:
		ret = ath12k_pull_svc_ready_ext2(wmi_handle, ptr,
						 &parse->arg);
		if (ret) {
			ath12k_warn(ab,
				    "failed to extract wmi service ready ext2 parameters: %d\n",
				    ret);
			return ret;
		}
		break;

	case WMI_TAG_ARRAY_STRUCT:
		if (!parse->dma_ring_cap_done) {
			ret = ath12k_wmi_dma_ring_caps(ab, len, ptr,
						       &parse->dma_caps_parse);
			if (ret)
				return ret;

			parse->dma_ring_cap_done = true;
		} else if (!parse->spectral_bin_scaling_done) {
			/* TODO: This is a place-holder as WMI tag for
			 * spectral scaling is before
			 * WMI_TAG_MAC_PHY_CAPABILITIES_EXT
			 */
			parse->spectral_bin_scaling_done = true;
		} else if (!parse->mac_phy_caps_ext_done) {
			ret = ath12k_wmi_tlv_iter(ab, ptr, len,
						  ath12k_wmi_tlv_mac_phy_caps_ext,
						  parse);
			if (ret) {
				ath12k_warn(ab, "failed to parse extended MAC PHY capabilities WMI TLV: %d\n",
					    ret);
				return ret;
			}

			parse->mac_phy_caps_ext_done = true;
		}
		break;
	default:
		break;
	}

	return 0;
}

static int ath12k_service_ready_ext2_event(struct ath12k_base *ab,
					   struct sk_buff *skb)
{
	struct ath12k_wmi_svc_rdy_ext2_parse svc_rdy_ext2 = { };
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_svc_rdy_ext2_parse,
				  &svc_rdy_ext2);
	if (ret) {
		ath12k_warn(ab, "failed to parse ext2 event tlv %d\n", ret);
		goto err;
	}

	complete(&ab->wmi_ab.service_ready);

	return 0;

err:
	ath12k_wmi_free_dbring_caps(ab);
	return ret;
}

static int ath12k_pull_vdev_start_resp_tlv(struct ath12k_base *ab, struct sk_buff *skb,
					   struct wmi_vdev_start_resp_event *vdev_rsp)
{
	const void **tb;
	const struct wmi_vdev_start_resp_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_START_RESPONSE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch vdev start resp ev");
		kfree(tb);
		return -EPROTO;
	}

	*vdev_rsp = *ev;

	kfree(tb);
	return 0;
}

static struct ath12k_reg_rule
*create_ext_reg_rules_from_wmi(u32 num_reg_rules,
#if defined(__linux__)
			       struct ath12k_wmi_reg_rule_ext_params *wmi_reg_rule)
#elif defined(__FreeBSD__)
			       const struct ath12k_wmi_reg_rule_ext_params *wmi_reg_rule)
#endif
{
	struct ath12k_reg_rule *reg_rule_ptr;
	u32 count;

	reg_rule_ptr = kzalloc((num_reg_rules * sizeof(*reg_rule_ptr)),
			       GFP_ATOMIC);

	if (!reg_rule_ptr)
		return NULL;

	for (count = 0; count < num_reg_rules; count++) {
		reg_rule_ptr[count].start_freq =
			le32_get_bits(wmi_reg_rule[count].freq_info,
				      REG_RULE_START_FREQ);
		reg_rule_ptr[count].end_freq =
			le32_get_bits(wmi_reg_rule[count].freq_info,
				      REG_RULE_END_FREQ);
		reg_rule_ptr[count].max_bw =
			le32_get_bits(wmi_reg_rule[count].bw_pwr_info,
				      REG_RULE_MAX_BW);
		reg_rule_ptr[count].reg_power =
			le32_get_bits(wmi_reg_rule[count].bw_pwr_info,
				      REG_RULE_REG_PWR);
		reg_rule_ptr[count].ant_gain =
			le32_get_bits(wmi_reg_rule[count].bw_pwr_info,
				      REG_RULE_ANT_GAIN);
		reg_rule_ptr[count].flags =
			le32_get_bits(wmi_reg_rule[count].flag_info,
				      REG_RULE_FLAGS);
		reg_rule_ptr[count].psd_flag =
			le32_get_bits(wmi_reg_rule[count].psd_power_info,
				      REG_RULE_PSD_INFO);
		reg_rule_ptr[count].psd_eirp =
			le32_get_bits(wmi_reg_rule[count].psd_power_info,
				      REG_RULE_PSD_EIRP);
	}

	return reg_rule_ptr;
}

static int ath12k_pull_reg_chan_list_ext_update_ev(struct ath12k_base *ab,
						   struct sk_buff *skb,
						   struct ath12k_reg_info *reg_info)
{
	const void **tb;
	const struct wmi_reg_chan_list_cc_ext_event *ev;
#if defined(__linux__)
	struct ath12k_wmi_reg_rule_ext_params *ext_wmi_reg_rule;
#elif defined(__FreeBSD__)
	const struct ath12k_wmi_reg_rule_ext_params *ext_wmi_reg_rule;
#endif
	u32 num_2g_reg_rules, num_5g_reg_rules;
	u32 num_6g_reg_rules_ap[WMI_REG_CURRENT_MAX_AP_TYPE];
	u32 num_6g_reg_rules_cl[WMI_REG_CURRENT_MAX_AP_TYPE][WMI_REG_MAX_CLIENT_TYPE];
	u32 total_reg_rules = 0;
	int ret, i, j;

	ath12k_dbg(ab, ATH12K_DBG_WMI, "processing regulatory ext channel list\n");

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_REG_CHAN_LIST_CC_EXT_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch reg chan list ext update ev\n");
		kfree(tb);
		return -EPROTO;
	}

	reg_info->num_2g_reg_rules = le32_to_cpu(ev->num_2g_reg_rules);
	reg_info->num_5g_reg_rules = le32_to_cpu(ev->num_5g_reg_rules);
	reg_info->num_6g_reg_rules_ap[WMI_REG_INDOOR_AP] =
		le32_to_cpu(ev->num_6g_reg_rules_ap_lpi);
	reg_info->num_6g_reg_rules_ap[WMI_REG_STD_POWER_AP] =
		le32_to_cpu(ev->num_6g_reg_rules_ap_sp);
	reg_info->num_6g_reg_rules_ap[WMI_REG_VLP_AP] =
		le32_to_cpu(ev->num_6g_reg_rules_ap_vlp);

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		reg_info->num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][i] =
			le32_to_cpu(ev->num_6g_reg_rules_cl_lpi[i]);
		reg_info->num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][i] =
			le32_to_cpu(ev->num_6g_reg_rules_cl_sp[i]);
		reg_info->num_6g_reg_rules_cl[WMI_REG_VLP_AP][i] =
			le32_to_cpu(ev->num_6g_reg_rules_cl_vlp[i]);
	}

	num_2g_reg_rules = reg_info->num_2g_reg_rules;
	total_reg_rules += num_2g_reg_rules;
	num_5g_reg_rules = reg_info->num_5g_reg_rules;
	total_reg_rules += num_5g_reg_rules;

	if (num_2g_reg_rules > MAX_REG_RULES || num_5g_reg_rules > MAX_REG_RULES) {
		ath12k_warn(ab, "Num reg rules for 2G/5G exceeds max limit (num_2g_reg_rules: %d num_5g_reg_rules: %d max_rules: %d)\n",
			    num_2g_reg_rules, num_5g_reg_rules, MAX_REG_RULES);
		kfree(tb);
		return -EINVAL;
	}

	for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
		num_6g_reg_rules_ap[i] = reg_info->num_6g_reg_rules_ap[i];

		if (num_6g_reg_rules_ap[i] > MAX_6G_REG_RULES) {
			ath12k_warn(ab, "Num 6G reg rules for AP mode(%d) exceeds max limit (num_6g_reg_rules_ap: %d, max_rules: %d)\n",
				    i, num_6g_reg_rules_ap[i], MAX_6G_REG_RULES);
			kfree(tb);
			return -EINVAL;
		}

		total_reg_rules += num_6g_reg_rules_ap[i];
	}

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][i] =
				reg_info->num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][i];
		total_reg_rules += num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][i];

		num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][i] =
				reg_info->num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][i];
		total_reg_rules += num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][i];

		num_6g_reg_rules_cl[WMI_REG_VLP_AP][i] =
				reg_info->num_6g_reg_rules_cl[WMI_REG_VLP_AP][i];
		total_reg_rules += num_6g_reg_rules_cl[WMI_REG_VLP_AP][i];

		if (num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][i] > MAX_6G_REG_RULES ||
		    num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][i] > MAX_6G_REG_RULES ||
		    num_6g_reg_rules_cl[WMI_REG_VLP_AP][i] >  MAX_6G_REG_RULES) {
			ath12k_warn(ab, "Num 6g client reg rules exceeds max limit, for client(type: %d)\n",
				    i);
			kfree(tb);
			return -EINVAL;
		}
	}

	if (!total_reg_rules) {
		ath12k_warn(ab, "No reg rules available\n");
		kfree(tb);
		return -EINVAL;
	}

	memcpy(reg_info->alpha2, &ev->alpha2, REG_ALPHA2_LEN);

	/* FIXME: Currently FW includes 6G reg rule also in 5G rule
	 * list for country US.
	 * Having same 6G reg rule in 5G and 6G rules list causes
	 * intersect check to be true, and same rules will be shown
	 * multiple times in iw cmd. So added hack below to avoid
	 * parsing 6G rule from 5G reg rule list, and this can be
	 * removed later, after FW updates to remove 6G reg rule
	 * from 5G rules list.
	 */
	if (memcmp(reg_info->alpha2, "US", 2) == 0) {
		reg_info->num_5g_reg_rules = REG_US_5G_NUM_REG_RULES;
		num_5g_reg_rules = reg_info->num_5g_reg_rules;
	}

	reg_info->dfs_region = le32_to_cpu(ev->dfs_region);
	reg_info->phybitmap = le32_to_cpu(ev->phybitmap);
	reg_info->num_phy = le32_to_cpu(ev->num_phy);
	reg_info->phy_id = le32_to_cpu(ev->phy_id);
	reg_info->ctry_code = le32_to_cpu(ev->country_id);
	reg_info->reg_dmn_pair = le32_to_cpu(ev->domain_code);

	switch (le32_to_cpu(ev->status_code)) {
	case WMI_REG_SET_CC_STATUS_PASS:
		reg_info->status_code = REG_SET_CC_STATUS_PASS;
		break;
	case WMI_REG_CURRENT_ALPHA2_NOT_FOUND:
		reg_info->status_code = REG_CURRENT_ALPHA2_NOT_FOUND;
		break;
	case WMI_REG_INIT_ALPHA2_NOT_FOUND:
		reg_info->status_code = REG_INIT_ALPHA2_NOT_FOUND;
		break;
	case WMI_REG_SET_CC_CHANGE_NOT_ALLOWED:
		reg_info->status_code = REG_SET_CC_CHANGE_NOT_ALLOWED;
		break;
	case WMI_REG_SET_CC_STATUS_NO_MEMORY:
		reg_info->status_code = REG_SET_CC_STATUS_NO_MEMORY;
		break;
	case WMI_REG_SET_CC_STATUS_FAIL:
		reg_info->status_code = REG_SET_CC_STATUS_FAIL;
		break;
	}

	reg_info->is_ext_reg_event = true;

	reg_info->min_bw_2g = le32_to_cpu(ev->min_bw_2g);
	reg_info->max_bw_2g = le32_to_cpu(ev->max_bw_2g);
	reg_info->min_bw_5g = le32_to_cpu(ev->min_bw_5g);
	reg_info->max_bw_5g = le32_to_cpu(ev->max_bw_5g);
	reg_info->min_bw_6g_ap[WMI_REG_INDOOR_AP] = le32_to_cpu(ev->min_bw_6g_ap_lpi);
	reg_info->max_bw_6g_ap[WMI_REG_INDOOR_AP] = le32_to_cpu(ev->max_bw_6g_ap_lpi);
	reg_info->min_bw_6g_ap[WMI_REG_STD_POWER_AP] = le32_to_cpu(ev->min_bw_6g_ap_sp);
	reg_info->max_bw_6g_ap[WMI_REG_STD_POWER_AP] = le32_to_cpu(ev->max_bw_6g_ap_sp);
	reg_info->min_bw_6g_ap[WMI_REG_VLP_AP] = le32_to_cpu(ev->min_bw_6g_ap_vlp);
	reg_info->max_bw_6g_ap[WMI_REG_VLP_AP] = le32_to_cpu(ev->max_bw_6g_ap_vlp);

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		reg_info->min_bw_6g_client[WMI_REG_INDOOR_AP][i] =
			le32_to_cpu(ev->min_bw_6g_client_lpi[i]);
		reg_info->max_bw_6g_client[WMI_REG_INDOOR_AP][i] =
			le32_to_cpu(ev->max_bw_6g_client_lpi[i]);
		reg_info->min_bw_6g_client[WMI_REG_STD_POWER_AP][i] =
			le32_to_cpu(ev->min_bw_6g_client_sp[i]);
		reg_info->max_bw_6g_client[WMI_REG_STD_POWER_AP][i] =
			le32_to_cpu(ev->max_bw_6g_client_sp[i]);
		reg_info->min_bw_6g_client[WMI_REG_VLP_AP][i] =
			le32_to_cpu(ev->min_bw_6g_client_vlp[i]);
		reg_info->max_bw_6g_client[WMI_REG_VLP_AP][i] =
			le32_to_cpu(ev->max_bw_6g_client_vlp[i]);
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "%s:cc_ext %s dsf %d BW: min_2g %d max_2g %d min_5g %d max_5g %d",
		   __func__, reg_info->alpha2, reg_info->dfs_region,
		   reg_info->min_bw_2g, reg_info->max_bw_2g,
		   reg_info->min_bw_5g, reg_info->max_bw_5g);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "num_2g_reg_rules %d num_5g_reg_rules %d",
		   num_2g_reg_rules, num_5g_reg_rules);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "num_6g_reg_rules_ap_lpi: %d num_6g_reg_rules_ap_sp: %d num_6g_reg_rules_ap_vlp: %d",
		   num_6g_reg_rules_ap[WMI_REG_INDOOR_AP],
		   num_6g_reg_rules_ap[WMI_REG_STD_POWER_AP],
		   num_6g_reg_rules_ap[WMI_REG_VLP_AP]);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "6g Regular client: num_6g_reg_rules_lpi: %d num_6g_reg_rules_sp: %d num_6g_reg_rules_vlp: %d",
		   num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][WMI_REG_DEFAULT_CLIENT],
		   num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][WMI_REG_DEFAULT_CLIENT],
		   num_6g_reg_rules_cl[WMI_REG_VLP_AP][WMI_REG_DEFAULT_CLIENT]);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "6g Subordinate client: num_6g_reg_rules_lpi: %d num_6g_reg_rules_sp: %d num_6g_reg_rules_vlp: %d",
		   num_6g_reg_rules_cl[WMI_REG_INDOOR_AP][WMI_REG_SUBORDINATE_CLIENT],
		   num_6g_reg_rules_cl[WMI_REG_STD_POWER_AP][WMI_REG_SUBORDINATE_CLIENT],
		   num_6g_reg_rules_cl[WMI_REG_VLP_AP][WMI_REG_SUBORDINATE_CLIENT]);

	ext_wmi_reg_rule =
#if defined(__linux__)
		(struct ath12k_wmi_reg_rule_ext_params *)((u8 *)ev
#elif defined(__FreeBSD__)
		(const struct ath12k_wmi_reg_rule_ext_params *)((const u8 *)ev
#endif
			+ sizeof(*ev)
			+ sizeof(struct wmi_tlv));

	if (num_2g_reg_rules) {
		reg_info->reg_rules_2g_ptr =
			create_ext_reg_rules_from_wmi(num_2g_reg_rules,
						      ext_wmi_reg_rule);

		if (!reg_info->reg_rules_2g_ptr) {
			kfree(tb);
			ath12k_warn(ab, "Unable to Allocate memory for 2g rules\n");
			return -ENOMEM;
		}
	}

	if (num_5g_reg_rules) {
		ext_wmi_reg_rule += num_2g_reg_rules;
		reg_info->reg_rules_5g_ptr =
			create_ext_reg_rules_from_wmi(num_5g_reg_rules,
						      ext_wmi_reg_rule);

		if (!reg_info->reg_rules_5g_ptr) {
			kfree(tb);
			ath12k_warn(ab, "Unable to Allocate memory for 5g rules\n");
			return -ENOMEM;
		}
	}

	ext_wmi_reg_rule += num_5g_reg_rules;

	for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
		reg_info->reg_rules_6g_ap_ptr[i] =
			create_ext_reg_rules_from_wmi(num_6g_reg_rules_ap[i],
						      ext_wmi_reg_rule);

		if (!reg_info->reg_rules_6g_ap_ptr[i]) {
			kfree(tb);
			ath12k_warn(ab, "Unable to Allocate memory for 6g ap rules\n");
			return -ENOMEM;
		}

		ext_wmi_reg_rule += num_6g_reg_rules_ap[i];
	}

	for (j = 0; j < WMI_REG_CURRENT_MAX_AP_TYPE; j++) {
		for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
			reg_info->reg_rules_6g_client_ptr[j][i] =
				create_ext_reg_rules_from_wmi(num_6g_reg_rules_cl[j][i],
							      ext_wmi_reg_rule);

			if (!reg_info->reg_rules_6g_client_ptr[j][i]) {
				kfree(tb);
				ath12k_warn(ab, "Unable to Allocate memory for 6g client rules\n");
				return -ENOMEM;
			}

			ext_wmi_reg_rule += num_6g_reg_rules_cl[j][i];
		}
	}

	reg_info->client_type = le32_to_cpu(ev->client_type);
	reg_info->rnr_tpe_usable = ev->rnr_tpe_usable;
	reg_info->unspecified_ap_usable = ev->unspecified_ap_usable;
	reg_info->domain_code_6g_ap[WMI_REG_INDOOR_AP] =
		le32_to_cpu(ev->domain_code_6g_ap_lpi);
	reg_info->domain_code_6g_ap[WMI_REG_STD_POWER_AP] =
		le32_to_cpu(ev->domain_code_6g_ap_sp);
	reg_info->domain_code_6g_ap[WMI_REG_VLP_AP] =
		le32_to_cpu(ev->domain_code_6g_ap_vlp);

	for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++) {
		reg_info->domain_code_6g_client[WMI_REG_INDOOR_AP][i] =
			le32_to_cpu(ev->domain_code_6g_client_lpi[i]);
		reg_info->domain_code_6g_client[WMI_REG_STD_POWER_AP][i] =
			le32_to_cpu(ev->domain_code_6g_client_sp[i]);
		reg_info->domain_code_6g_client[WMI_REG_VLP_AP][i] =
			le32_to_cpu(ev->domain_code_6g_client_vlp[i]);
	}

	reg_info->domain_code_6g_super_id = le32_to_cpu(ev->domain_code_6g_super_id);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "6g client_type: %d domain_code_6g_super_id: %d",
		   reg_info->client_type, reg_info->domain_code_6g_super_id);

	ath12k_dbg(ab, ATH12K_DBG_WMI, "processed regulatory ext channel list\n");

	kfree(tb);
	return 0;
}

static int ath12k_pull_peer_del_resp_ev(struct ath12k_base *ab, struct sk_buff *skb,
					struct wmi_peer_delete_resp_event *peer_del_resp)
{
	const void **tb;
	const struct wmi_peer_delete_resp_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PEER_DELETE_RESP_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch peer delete resp ev");
		kfree(tb);
		return -EPROTO;
	}

	memset(peer_del_resp, 0, sizeof(*peer_del_resp));

	peer_del_resp->vdev_id = ev->vdev_id;
	ether_addr_copy(peer_del_resp->peer_macaddr.addr,
			ev->peer_macaddr.addr);

	kfree(tb);
	return 0;
}

static int ath12k_pull_vdev_del_resp_ev(struct ath12k_base *ab,
					struct sk_buff *skb,
					u32 *vdev_id)
{
	const void **tb;
	const struct wmi_vdev_delete_resp_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_DELETE_RESP_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch vdev delete resp ev");
		kfree(tb);
		return -EPROTO;
	}

	*vdev_id = le32_to_cpu(ev->vdev_id);

	kfree(tb);
	return 0;
}

static int ath12k_pull_bcn_tx_status_ev(struct ath12k_base *ab, void *evt_buf,
					u32 len, u32 *vdev_id,
					u32 *tx_status)
{
	const void **tb;
	const struct wmi_bcn_tx_status_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, evt_buf, len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_OFFLOAD_BCN_TX_STATUS_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch bcn tx status ev");
		kfree(tb);
		return -EPROTO;
	}

	*vdev_id = le32_to_cpu(ev->vdev_id);
	*tx_status = le32_to_cpu(ev->tx_status);

	kfree(tb);
	return 0;
}

static int ath12k_pull_vdev_stopped_param_tlv(struct ath12k_base *ab, struct sk_buff *skb,
					      u32 *vdev_id)
{
	const void **tb;
	const struct wmi_vdev_stopped_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_STOPPED_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch vdev stop ev");
		kfree(tb);
		return -EPROTO;
	}

	*vdev_id = le32_to_cpu(ev->vdev_id);

	kfree(tb);
	return 0;
}

static int ath12k_wmi_tlv_mgmt_rx_parse(struct ath12k_base *ab,
					u16 tag, u16 len,
					const void *ptr, void *data)
{
	struct wmi_tlv_mgmt_rx_parse *parse = data;

	switch (tag) {
	case WMI_TAG_MGMT_RX_HDR:
		parse->fixed = ptr;
		break;
	case WMI_TAG_ARRAY_BYTE:
		if (!parse->frame_buf_done) {
			parse->frame_buf = ptr;
			parse->frame_buf_done = true;
		}
		break;
	}
	return 0;
}

static int ath12k_pull_mgmt_rx_params_tlv(struct ath12k_base *ab,
					  struct sk_buff *skb,
					  struct ath12k_wmi_mgmt_rx_arg *hdr)
{
	struct wmi_tlv_mgmt_rx_parse parse = { };
	const struct ath12k_wmi_mgmt_rx_params *ev;
	const u8 *frame;
	int i, ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_mgmt_rx_parse,
				  &parse);
	if (ret) {
		ath12k_warn(ab, "failed to parse mgmt rx tlv %d\n", ret);
		return ret;
	}

	ev = parse.fixed;
	frame = parse.frame_buf;

	if (!ev || !frame) {
		ath12k_warn(ab, "failed to fetch mgmt rx hdr");
		return -EPROTO;
	}

	hdr->pdev_id = le32_to_cpu(ev->pdev_id);
	hdr->chan_freq = le32_to_cpu(ev->chan_freq);
	hdr->channel = le32_to_cpu(ev->channel);
	hdr->snr = le32_to_cpu(ev->snr);
	hdr->rate = le32_to_cpu(ev->rate);
	hdr->phy_mode = le32_to_cpu(ev->phy_mode);
	hdr->buf_len = le32_to_cpu(ev->buf_len);
	hdr->status = le32_to_cpu(ev->status);
	hdr->flags = le32_to_cpu(ev->flags);
	hdr->rssi = a_sle32_to_cpu(ev->rssi);
	hdr->tsf_delta = le32_to_cpu(ev->tsf_delta);

	for (i = 0; i < ATH_MAX_ANTENNA; i++)
		hdr->rssi_ctl[i] = le32_to_cpu(ev->rssi_ctl[i]);

	if (skb->len < (frame - skb->data) + hdr->buf_len) {
		ath12k_warn(ab, "invalid length in mgmt rx hdr ev");
		return -EPROTO;
	}

	/* shift the sk_buff to point to `frame` */
	skb_trim(skb, 0);
	skb_put(skb, frame - skb->data);
	skb_pull(skb, frame - skb->data);
	skb_put(skb, hdr->buf_len);

	return 0;
}

static int wmi_process_mgmt_tx_comp(struct ath12k *ar, u32 desc_id,
				    u32 status)
{
	struct sk_buff *msdu;
	struct ieee80211_tx_info *info;
	struct ath12k_skb_cb *skb_cb;
	int num_mgmt;

	spin_lock_bh(&ar->txmgmt_idr_lock);
	msdu = idr_find(&ar->txmgmt_idr, desc_id);

	if (!msdu) {
		ath12k_warn(ar->ab, "received mgmt tx compl for invalid msdu_id: %d\n",
			    desc_id);
		spin_unlock_bh(&ar->txmgmt_idr_lock);
		return -ENOENT;
	}

	idr_remove(&ar->txmgmt_idr, desc_id);
	spin_unlock_bh(&ar->txmgmt_idr_lock);

	skb_cb = ATH12K_SKB_CB(msdu);
	dma_unmap_single(ar->ab->dev, skb_cb->paddr, msdu->len, DMA_TO_DEVICE);

	info = IEEE80211_SKB_CB(msdu);
	if ((!(info->flags & IEEE80211_TX_CTL_NO_ACK)) && !status)
		info->flags |= IEEE80211_TX_STAT_ACK;

	ieee80211_tx_status_irqsafe(ar->hw, msdu);

	num_mgmt = atomic_dec_if_positive(&ar->num_pending_mgmt_tx);

	/* WARN when we received this event without doing any mgmt tx */
	if (num_mgmt < 0)
		WARN_ON_ONCE(1);

	if (!num_mgmt)
		wake_up(&ar->txmgmt_empty_waitq);

	return 0;
}

static int ath12k_pull_mgmt_tx_compl_param_tlv(struct ath12k_base *ab,
					       struct sk_buff *skb,
					       struct wmi_mgmt_tx_compl_event *param)
{
	const void **tb;
	const struct wmi_mgmt_tx_compl_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_MGMT_TX_COMPL_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch mgmt tx compl ev");
		kfree(tb);
		return -EPROTO;
	}

	param->pdev_id = ev->pdev_id;
	param->desc_id = ev->desc_id;
	param->status = ev->status;

	kfree(tb);
	return 0;
}

static void ath12k_wmi_event_scan_started(struct ath12k *ar)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		ath12k_warn(ar->ab, "received scan started event in an invalid scan state: %s (%d)\n",
			    ath12k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH12K_SCAN_STARTING:
		ar->scan.state = ATH12K_SCAN_RUNNING;
		complete(&ar->scan.started);
		break;
	}
}

static void ath12k_wmi_event_scan_start_failed(struct ath12k *ar)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		ath12k_warn(ar->ab, "received scan start failed event in an invalid scan state: %s (%d)\n",
			    ath12k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH12K_SCAN_STARTING:
		complete(&ar->scan.started);
		__ath12k_mac_scan_finish(ar);
		break;
	}
}

static void ath12k_wmi_event_scan_completed(struct ath12k *ar)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_STARTING:
		/* One suspected reason scan can be completed while starting is
		 * if firmware fails to deliver all scan events to the host,
		 * e.g. when transport pipe is full. This has been observed
		 * with spectral scan phyerr events starving wmi transport
		 * pipe. In such case the "scan completed" event should be (and
		 * is) ignored by the host as it may be just firmware's scan
		 * state machine recovering.
		 */
		ath12k_warn(ar->ab, "received scan completed event in an invalid scan state: %s (%d)\n",
			    ath12k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		__ath12k_mac_scan_finish(ar);
		break;
	}
}

static void ath12k_wmi_event_scan_bss_chan(struct ath12k *ar)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_STARTING:
		ath12k_warn(ar->ab, "received scan bss chan event in an invalid scan state: %s (%d)\n",
			    ath12k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		ar->scan_channel = NULL;
		break;
	}
}

static void ath12k_wmi_event_scan_foreign_chan(struct ath12k *ar, u32 freq)
{
	lockdep_assert_held(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_STARTING:
		ath12k_warn(ar->ab, "received scan foreign chan event in an invalid scan state: %s (%d)\n",
			    ath12k_scan_state_str(ar->scan.state),
			    ar->scan.state);
		break;
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		ar->scan_channel = ieee80211_get_channel(ar->hw->wiphy, freq);
		break;
	}
}

static const char *
ath12k_wmi_event_scan_type_str(enum wmi_scan_event_type type,
			       enum wmi_scan_completion_reason reason)
{
	switch (type) {
	case WMI_SCAN_EVENT_STARTED:
		return "started";
	case WMI_SCAN_EVENT_COMPLETED:
		switch (reason) {
		case WMI_SCAN_REASON_COMPLETED:
			return "completed";
		case WMI_SCAN_REASON_CANCELLED:
			return "completed [cancelled]";
		case WMI_SCAN_REASON_PREEMPTED:
			return "completed [preempted]";
		case WMI_SCAN_REASON_TIMEDOUT:
			return "completed [timedout]";
		case WMI_SCAN_REASON_INTERNAL_FAILURE:
			return "completed [internal err]";
		case WMI_SCAN_REASON_MAX:
			break;
		}
		return "completed [unknown]";
	case WMI_SCAN_EVENT_BSS_CHANNEL:
		return "bss channel";
	case WMI_SCAN_EVENT_FOREIGN_CHAN:
		return "foreign channel";
	case WMI_SCAN_EVENT_DEQUEUED:
		return "dequeued";
	case WMI_SCAN_EVENT_PREEMPTED:
		return "preempted";
	case WMI_SCAN_EVENT_START_FAILED:
		return "start failed";
	case WMI_SCAN_EVENT_RESTARTED:
		return "restarted";
	case WMI_SCAN_EVENT_FOREIGN_CHAN_EXIT:
		return "foreign channel exit";
	default:
		return "unknown";
	}
}

static int ath12k_pull_scan_ev(struct ath12k_base *ab, struct sk_buff *skb,
			       struct wmi_scan_event *scan_evt_param)
{
	const void **tb;
	const struct wmi_scan_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_SCAN_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch scan ev");
		kfree(tb);
		return -EPROTO;
	}

	scan_evt_param->event_type = ev->event_type;
	scan_evt_param->reason = ev->reason;
	scan_evt_param->channel_freq = ev->channel_freq;
	scan_evt_param->scan_req_id = ev->scan_req_id;
	scan_evt_param->scan_id = ev->scan_id;
	scan_evt_param->vdev_id = ev->vdev_id;
	scan_evt_param->tsf_timestamp = ev->tsf_timestamp;

	kfree(tb);
	return 0;
}

static int ath12k_pull_peer_sta_kickout_ev(struct ath12k_base *ab, struct sk_buff *skb,
					   struct wmi_peer_sta_kickout_arg *arg)
{
	const void **tb;
	const struct wmi_peer_sta_kickout_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PEER_STA_KICKOUT_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch peer sta kickout ev");
		kfree(tb);
		return -EPROTO;
	}

	arg->mac_addr = ev->peer_macaddr.addr;

	kfree(tb);
	return 0;
}

static int ath12k_pull_roam_ev(struct ath12k_base *ab, struct sk_buff *skb,
			       struct wmi_roam_event *roam_ev)
{
	const void **tb;
	const struct wmi_roam_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_ROAM_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch roam ev");
		kfree(tb);
		return -EPROTO;
	}

	roam_ev->vdev_id = ev->vdev_id;
	roam_ev->reason = ev->reason;
	roam_ev->rssi = ev->rssi;

	kfree(tb);
	return 0;
}

static int freq_to_idx(struct ath12k *ar, int freq)
{
	struct ieee80211_supported_band *sband;
	int band, ch, idx = 0;

	for (band = NL80211_BAND_2GHZ; band < NUM_NL80211_BANDS; band++) {
		if (!ar->mac.sbands[band].channels)
			continue;

		sband = ar->hw->wiphy->bands[band];
		if (!sband)
			continue;

		for (ch = 0; ch < sband->n_channels; ch++, idx++)
			if (sband->channels[ch].center_freq == freq)
				goto exit;
	}

exit:
	return idx;
}

static int ath12k_pull_chan_info_ev(struct ath12k_base *ab, u8 *evt_buf,
				    u32 len, struct wmi_chan_info_event *ch_info_ev)
{
	const void **tb;
	const struct wmi_chan_info_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, evt_buf, len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_CHAN_INFO_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch chan info ev");
		kfree(tb);
		return -EPROTO;
	}

	ch_info_ev->err_code = ev->err_code;
	ch_info_ev->freq = ev->freq;
	ch_info_ev->cmd_flags = ev->cmd_flags;
	ch_info_ev->noise_floor = ev->noise_floor;
	ch_info_ev->rx_clear_count = ev->rx_clear_count;
	ch_info_ev->cycle_count = ev->cycle_count;
	ch_info_ev->chan_tx_pwr_range = ev->chan_tx_pwr_range;
	ch_info_ev->chan_tx_pwr_tp = ev->chan_tx_pwr_tp;
	ch_info_ev->rx_frame_count = ev->rx_frame_count;
	ch_info_ev->tx_frame_cnt = ev->tx_frame_cnt;
	ch_info_ev->mac_clk_mhz = ev->mac_clk_mhz;
	ch_info_ev->vdev_id = ev->vdev_id;

	kfree(tb);
	return 0;
}

static int
ath12k_pull_pdev_bss_chan_info_ev(struct ath12k_base *ab, struct sk_buff *skb,
				  struct wmi_pdev_bss_chan_info_event *bss_ch_info_ev)
{
	const void **tb;
	const struct wmi_pdev_bss_chan_info_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PDEV_BSS_CHAN_INFO_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch pdev bss chan info ev");
		kfree(tb);
		return -EPROTO;
	}

	bss_ch_info_ev->pdev_id = ev->pdev_id;
	bss_ch_info_ev->freq = ev->freq;
	bss_ch_info_ev->noise_floor = ev->noise_floor;
	bss_ch_info_ev->rx_clear_count_low = ev->rx_clear_count_low;
	bss_ch_info_ev->rx_clear_count_high = ev->rx_clear_count_high;
	bss_ch_info_ev->cycle_count_low = ev->cycle_count_low;
	bss_ch_info_ev->cycle_count_high = ev->cycle_count_high;
	bss_ch_info_ev->tx_cycle_count_low = ev->tx_cycle_count_low;
	bss_ch_info_ev->tx_cycle_count_high = ev->tx_cycle_count_high;
	bss_ch_info_ev->rx_cycle_count_low = ev->rx_cycle_count_low;
	bss_ch_info_ev->rx_cycle_count_high = ev->rx_cycle_count_high;
	bss_ch_info_ev->rx_bss_cycle_count_low = ev->rx_bss_cycle_count_low;
	bss_ch_info_ev->rx_bss_cycle_count_high = ev->rx_bss_cycle_count_high;

	kfree(tb);
	return 0;
}

static int
ath12k_pull_vdev_install_key_compl_ev(struct ath12k_base *ab, struct sk_buff *skb,
				      struct wmi_vdev_install_key_complete_arg *arg)
{
	const void **tb;
	const struct wmi_vdev_install_key_compl_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_VDEV_INSTALL_KEY_COMPLETE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch vdev install key compl ev");
		kfree(tb);
		return -EPROTO;
	}

	arg->vdev_id = le32_to_cpu(ev->vdev_id);
	arg->macaddr = ev->peer_macaddr.addr;
	arg->key_idx = le32_to_cpu(ev->key_idx);
	arg->key_flags = le32_to_cpu(ev->key_flags);
	arg->status = le32_to_cpu(ev->status);

	kfree(tb);
	return 0;
}

static int ath12k_pull_peer_assoc_conf_ev(struct ath12k_base *ab, struct sk_buff *skb,
					  struct wmi_peer_assoc_conf_arg *peer_assoc_conf)
{
	const void **tb;
	const struct wmi_peer_assoc_conf_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PEER_ASSOC_CONF_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch peer assoc conf ev");
		kfree(tb);
		return -EPROTO;
	}

	peer_assoc_conf->vdev_id = le32_to_cpu(ev->vdev_id);
	peer_assoc_conf->macaddr = ev->peer_macaddr.addr;

	kfree(tb);
	return 0;
}

static int
ath12k_pull_pdev_temp_ev(struct ath12k_base *ab, u8 *evt_buf,
			 u32 len, const struct wmi_pdev_temperature_event *ev)
{
	const void **tb;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, evt_buf, len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return ret;
	}

	ev = tb[WMI_TAG_PDEV_TEMPERATURE_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch pdev temp ev");
		kfree(tb);
		return -EPROTO;
	}

	kfree(tb);
	return 0;
}

static void ath12k_wmi_op_ep_tx_credits(struct ath12k_base *ab)
{
	/* try to send pending beacons first. they take priority */
	wake_up(&ab->wmi_ab.tx_credits_wq);
}

static void ath12k_wmi_htc_tx_complete(struct ath12k_base *ab,
				       struct sk_buff *skb)
{
	dev_kfree_skb(skb);
}

static bool ath12k_reg_is_world_alpha(char *alpha)
{
	return alpha[0] == '0' && alpha[1] == '0';
}

static int ath12k_reg_chan_list_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_reg_info *reg_info = NULL;
	struct ieee80211_regdomain *regd = NULL;
	bool intersect = false;
	int ret = 0, pdev_idx, i, j;
	struct ath12k *ar;

	reg_info = kzalloc(sizeof(*reg_info), GFP_ATOMIC);
	if (!reg_info) {
		ret = -ENOMEM;
		goto fallback;
	}

	ret = ath12k_pull_reg_chan_list_ext_update_ev(ab, skb, reg_info);

	if (ret) {
		ath12k_warn(ab, "failed to extract regulatory info from received event\n");
		goto fallback;
	}

	if (reg_info->status_code != REG_SET_CC_STATUS_PASS) {
		/* In case of failure to set the requested ctry,
		 * fw retains the current regd. We print a failure info
		 * and return from here.
		 */
		ath12k_warn(ab, "Failed to set the requested Country regulatory setting\n");
		goto mem_free;
	}

	pdev_idx = reg_info->phy_id;

	if (pdev_idx >= ab->num_radios) {
		/* Process the event for phy0 only if single_pdev_only
		 * is true. If pdev_idx is valid but not 0, discard the
		 * event. Otherwise, it goes to fallback.
		 */
		if (ab->hw_params->single_pdev_only &&
		    pdev_idx < ab->hw_params->num_rxmda_per_pdev)
			goto mem_free;
		else
			goto fallback;
	}

	/* Avoid multiple overwrites to default regd, during core
	 * stop-start after mac registration.
	 */
	if (ab->default_regd[pdev_idx] && !ab->new_regd[pdev_idx] &&
	    !memcmp(ab->default_regd[pdev_idx]->alpha2,
		    reg_info->alpha2, 2))
		goto mem_free;

	/* Intersect new rules with default regd if a new country setting was
	 * requested, i.e a default regd was already set during initialization
	 * and the regd coming from this event has a valid country info.
	 */
	if (ab->default_regd[pdev_idx] &&
	    !ath12k_reg_is_world_alpha((char *)
		ab->default_regd[pdev_idx]->alpha2) &&
	    !ath12k_reg_is_world_alpha((char *)reg_info->alpha2))
		intersect = true;

	regd = ath12k_reg_build_regd(ab, reg_info, intersect);
	if (!regd) {
		ath12k_warn(ab, "failed to build regd from reg_info\n");
		goto fallback;
	}

	spin_lock(&ab->base_lock);
	if (test_bit(ATH12K_FLAG_REGISTERED, &ab->dev_flags)) {
		/* Once mac is registered, ar is valid and all CC events from
		 * fw is considered to be received due to user requests
		 * currently.
		 * Free previously built regd before assigning the newly
		 * generated regd to ar. NULL pointer handling will be
		 * taken care by kfree itself.
		 */
		ar = ab->pdevs[pdev_idx].ar;
		kfree(ab->new_regd[pdev_idx]);
		ab->new_regd[pdev_idx] = regd;
		queue_work(ab->workqueue, &ar->regd_update_work);
	} else {
		/* Multiple events for the same *ar is not expected. But we
		 * can still clear any previously stored default_regd if we
		 * are receiving this event for the same radio by mistake.
		 * NULL pointer handling will be taken care by kfree itself.
		 */
		kfree(ab->default_regd[pdev_idx]);
		/* This regd would be applied during mac registration */
		ab->default_regd[pdev_idx] = regd;
	}
	ab->dfs_region = reg_info->dfs_region;
	spin_unlock(&ab->base_lock);

	goto mem_free;

fallback:
	/* Fallback to older reg (by sending previous country setting
	 * again if fw has succeeded and we failed to process here.
	 * The Regdomain should be uniform across driver and fw. Since the
	 * FW has processed the command and sent a success status, we expect
	 * this function to succeed as well. If it doesn't, CTRY needs to be
	 * reverted at the fw and the old SCAN_CHAN_LIST cmd needs to be sent.
	 */
	/* TODO: This is rare, but still should also be handled */
	WARN_ON(1);
mem_free:
	if (reg_info) {
		kfree(reg_info->reg_rules_2g_ptr);
		kfree(reg_info->reg_rules_5g_ptr);
		if (reg_info->is_ext_reg_event) {
			for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++)
				kfree(reg_info->reg_rules_6g_ap_ptr[i]);

			for (j = 0; j < WMI_REG_CURRENT_MAX_AP_TYPE; j++)
				for (i = 0; i < WMI_REG_MAX_CLIENT_TYPE; i++)
					kfree(reg_info->reg_rules_6g_client_ptr[j][i]);
		}
		kfree(reg_info);
	}
	return ret;
}

static int ath12k_wmi_rdy_parse(struct ath12k_base *ab, u16 tag, u16 len,
				const void *ptr, void *data)
{
	struct ath12k_wmi_rdy_parse *rdy_parse = data;
	struct wmi_ready_event fixed_param;
#if defined(__linux__)
	struct ath12k_wmi_mac_addr_params *addr_list;
#elif defined(__FreeBSD__)
	const struct ath12k_wmi_mac_addr_params *addr_list;
#endif
	struct ath12k_pdev *pdev;
	u32 num_mac_addr;
	int i;

	switch (tag) {
	case WMI_TAG_READY_EVENT:
		memset(&fixed_param, 0, sizeof(fixed_param));
#if defined(__linux__)
		memcpy(&fixed_param, (struct wmi_ready_event *)ptr,
#elif defined(__FreeBSD__)
		memcpy(&fixed_param, (const struct wmi_ready_event *)ptr,
#endif
		       min_t(u16, sizeof(fixed_param), len));
		ab->wlan_init_status = le32_to_cpu(fixed_param.ready_event_min.status);
		rdy_parse->num_extra_mac_addr =
			le32_to_cpu(fixed_param.ready_event_min.num_extra_mac_addr);

		ether_addr_copy(ab->mac_addr,
				fixed_param.ready_event_min.mac_addr.addr);
		ab->pktlog_defs_checksum = le32_to_cpu(fixed_param.pktlog_defs_checksum);
		ab->wmi_ready = true;
		break;
	case WMI_TAG_ARRAY_FIXED_STRUCT:
#if defined(__linux__)
		addr_list = (struct ath12k_wmi_mac_addr_params *)ptr;
#elif defined(__FreeBSD__)
		addr_list = (const struct ath12k_wmi_mac_addr_params *)ptr;
#endif
		num_mac_addr = rdy_parse->num_extra_mac_addr;

		if (!(ab->num_radios > 1 && num_mac_addr >= ab->num_radios))
			break;

		for (i = 0; i < ab->num_radios; i++) {
			pdev = &ab->pdevs[i];
			ether_addr_copy(pdev->mac_addr, addr_list[i].addr);
		}
		ab->pdevs_macaddr_valid = true;
		break;
	default:
		break;
	}

	return 0;
}

static int ath12k_ready_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_wmi_rdy_parse rdy_parse = { };
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_rdy_parse, &rdy_parse);
	if (ret) {
		ath12k_warn(ab, "failed to parse tlv %d\n", ret);
		return ret;
	}

	complete(&ab->wmi_ab.unified_ready);
	return 0;
}

static void ath12k_peer_delete_resp_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_peer_delete_resp_event peer_del_resp;
	struct ath12k *ar;

	if (ath12k_pull_peer_del_resp_ev(ab, skb, &peer_del_resp) != 0) {
		ath12k_warn(ab, "failed to extract peer delete resp");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, le32_to_cpu(peer_del_resp.vdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in peer delete resp ev %d",
			    peer_del_resp.vdev_id);
		rcu_read_unlock();
		return;
	}

	complete(&ar->peer_delete_done);
	rcu_read_unlock();
	ath12k_dbg(ab, ATH12K_DBG_WMI, "peer delete resp for vdev id %d addr %pM\n",
		   peer_del_resp.vdev_id, peer_del_resp.peer_macaddr.addr);
}

static void ath12k_vdev_delete_resp_event(struct ath12k_base *ab,
					  struct sk_buff *skb)
{
	struct ath12k *ar;
	u32 vdev_id = 0;

	if (ath12k_pull_vdev_del_resp_ev(ab, skb, &vdev_id) != 0) {
		ath12k_warn(ab, "failed to extract vdev delete resp");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in vdev delete resp ev %d",
			    vdev_id);
		rcu_read_unlock();
		return;
	}

	complete(&ar->vdev_delete_done);

	rcu_read_unlock();

	ath12k_dbg(ab, ATH12K_DBG_WMI, "vdev delete resp for vdev id %d\n",
		   vdev_id);
}

static const char *ath12k_wmi_vdev_resp_print(u32 vdev_resp_status)
{
	switch (vdev_resp_status) {
	case WMI_VDEV_START_RESPONSE_INVALID_VDEVID:
		return "invalid vdev id";
	case WMI_VDEV_START_RESPONSE_NOT_SUPPORTED:
		return "not supported";
	case WMI_VDEV_START_RESPONSE_DFS_VIOLATION:
		return "dfs violation";
	case WMI_VDEV_START_RESPONSE_INVALID_REGDOMAIN:
		return "invalid regdomain";
	default:
		return "unknown";
	}
}

static void ath12k_vdev_start_resp_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_vdev_start_resp_event vdev_start_resp;
	struct ath12k *ar;
	u32 status;

	if (ath12k_pull_vdev_start_resp_tlv(ab, skb, &vdev_start_resp) != 0) {
		ath12k_warn(ab, "failed to extract vdev start resp");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, le32_to_cpu(vdev_start_resp.vdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in vdev start resp ev %d",
			    vdev_start_resp.vdev_id);
		rcu_read_unlock();
		return;
	}

	ar->last_wmi_vdev_start_status = 0;

	status = le32_to_cpu(vdev_start_resp.status);

	if (WARN_ON_ONCE(status)) {
		ath12k_warn(ab, "vdev start resp error status %d (%s)\n",
			    status, ath12k_wmi_vdev_resp_print(status));
		ar->last_wmi_vdev_start_status = status;
	}

	complete(&ar->vdev_setup_done);

	rcu_read_unlock();

	ath12k_dbg(ab, ATH12K_DBG_WMI, "vdev start resp for vdev id %d",
		   vdev_start_resp.vdev_id);
}

static void ath12k_bcn_tx_status_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	u32 vdev_id, tx_status;

	if (ath12k_pull_bcn_tx_status_ev(ab, skb->data, skb->len,
					 &vdev_id, &tx_status) != 0) {
		ath12k_warn(ab, "failed to extract bcn tx status");
		return;
	}
}

static void ath12k_vdev_stopped_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k *ar;
	u32 vdev_id = 0;

	if (ath12k_pull_vdev_stopped_param_tlv(ab, skb, &vdev_id) != 0) {
		ath12k_warn(ab, "failed to extract vdev stopped event");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in vdev stopped ev %d",
			    vdev_id);
		rcu_read_unlock();
		return;
	}

	complete(&ar->vdev_setup_done);

	rcu_read_unlock();

	ath12k_dbg(ab, ATH12K_DBG_WMI, "vdev stopped for vdev id %d", vdev_id);
}

static void ath12k_mgmt_rx_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k_wmi_mgmt_rx_arg rx_ev = {0};
	struct ath12k *ar;
	struct ieee80211_rx_status *status = IEEE80211_SKB_RXCB(skb);
	struct ieee80211_hdr *hdr;
	u16 fc;
	struct ieee80211_supported_band *sband;

	if (ath12k_pull_mgmt_rx_params_tlv(ab, skb, &rx_ev) != 0) {
		ath12k_warn(ab, "failed to extract mgmt rx event");
		dev_kfree_skb(skb);
		return;
	}

	memset(status, 0, sizeof(*status));

	ath12k_dbg(ab, ATH12K_DBG_MGMT, "mgmt rx event status %08x\n",
		   rx_ev.status);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, rx_ev.pdev_id);

	if (!ar) {
		ath12k_warn(ab, "invalid pdev_id %d in mgmt_rx_event\n",
			    rx_ev.pdev_id);
		dev_kfree_skb(skb);
		goto exit;
	}

	if ((test_bit(ATH12K_CAC_RUNNING, &ar->dev_flags)) ||
	    (rx_ev.status & (WMI_RX_STATUS_ERR_DECRYPT |
			     WMI_RX_STATUS_ERR_KEY_CACHE_MISS |
			     WMI_RX_STATUS_ERR_CRC))) {
		dev_kfree_skb(skb);
		goto exit;
	}

	if (rx_ev.status & WMI_RX_STATUS_ERR_MIC)
		status->flag |= RX_FLAG_MMIC_ERROR;

	if (rx_ev.chan_freq >= ATH12K_MIN_6G_FREQ) {
		status->band = NL80211_BAND_6GHZ;
	} else if (rx_ev.channel >= 1 && rx_ev.channel <= 14) {
		status->band = NL80211_BAND_2GHZ;
	} else if (rx_ev.channel >= 36 && rx_ev.channel <= ATH12K_MAX_5G_CHAN) {
		status->band = NL80211_BAND_5GHZ;
	} else {
		/* Shouldn't happen unless list of advertised channels to
		 * mac80211 has been changed.
		 */
		WARN_ON_ONCE(1);
		dev_kfree_skb(skb);
		goto exit;
	}

	if (rx_ev.phy_mode == MODE_11B &&
	    (status->band == NL80211_BAND_5GHZ || status->band == NL80211_BAND_6GHZ))
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "wmi mgmt rx 11b (CCK) on 5/6GHz, band = %d\n", status->band);

	sband = &ar->mac.sbands[status->band];

	status->freq = ieee80211_channel_to_frequency(rx_ev.channel,
						      status->band);
	status->signal = rx_ev.snr + ATH12K_DEFAULT_NOISE_FLOOR;
	status->rate_idx = ath12k_mac_bitrate_to_idx(sband, rx_ev.rate / 100);

	hdr = (struct ieee80211_hdr *)skb->data;
	fc = le16_to_cpu(hdr->frame_control);

	/* Firmware is guaranteed to report all essential management frames via
	 * WMI while it can deliver some extra via HTT. Since there can be
	 * duplicates split the reporting wrt monitor/sniffing.
	 */
	status->flag |= RX_FLAG_SKIP_MONITOR;

	/* In case of PMF, FW delivers decrypted frames with Protected Bit set
	 * including group privacy action frames.
	 */
	if (ieee80211_has_protected(hdr->frame_control)) {
		status->flag |= RX_FLAG_DECRYPTED;

		if (!ieee80211_is_robust_mgmt_frame(skb)) {
			status->flag |= RX_FLAG_IV_STRIPPED |
					RX_FLAG_MMIC_STRIPPED;
			hdr->frame_control = __cpu_to_le16(fc &
					     ~IEEE80211_FCTL_PROTECTED);
		}
	}

	/* TODO: Pending handle beacon implementation
	 *if (ieee80211_is_beacon(hdr->frame_control))
	 *	ath12k_mac_handle_beacon(ar, skb);
	 */

	ath12k_dbg(ab, ATH12K_DBG_MGMT,
		   "event mgmt rx skb %pK len %d ftype %02x stype %02x\n",
		   skb, skb->len,
		   fc & IEEE80211_FCTL_FTYPE, fc & IEEE80211_FCTL_STYPE);

	ath12k_dbg(ab, ATH12K_DBG_MGMT,
		   "event mgmt rx freq %d band %d snr %d, rate_idx %d\n",
		   status->freq, status->band, status->signal,
		   status->rate_idx);

	ieee80211_rx_ni(ar->hw, skb);

exit:
	rcu_read_unlock();
}

static void ath12k_mgmt_tx_compl_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_mgmt_tx_compl_event tx_compl_param = {0};
	struct ath12k *ar;

	if (ath12k_pull_mgmt_tx_compl_param_tlv(ab, skb, &tx_compl_param) != 0) {
		ath12k_warn(ab, "failed to extract mgmt tx compl event");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, le32_to_cpu(tx_compl_param.pdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid pdev id %d in mgmt_tx_compl_event\n",
			    tx_compl_param.pdev_id);
		goto exit;
	}

	wmi_process_mgmt_tx_comp(ar, le32_to_cpu(tx_compl_param.desc_id),
				 le32_to_cpu(tx_compl_param.status));

	ath12k_dbg(ab, ATH12K_DBG_MGMT,
		   "mgmt tx compl ev pdev_id %d, desc_id %d, status %d",
		   tx_compl_param.pdev_id, tx_compl_param.desc_id,
		   tx_compl_param.status);

exit:
	rcu_read_unlock();
}

static struct ath12k *ath12k_get_ar_on_scan_abort(struct ath12k_base *ab,
						  u32 vdev_id)
{
	int i;
	struct ath12k_pdev *pdev;
	struct ath12k *ar;

	for (i = 0; i < ab->num_radios; i++) {
		pdev = rcu_dereference(ab->pdevs_active[i]);
		if (pdev && pdev->ar) {
			ar = pdev->ar;

			spin_lock_bh(&ar->data_lock);
			if (ar->scan.state == ATH12K_SCAN_ABORTING &&
			    ar->scan.vdev_id == vdev_id) {
				spin_unlock_bh(&ar->data_lock);
				return ar;
			}
			spin_unlock_bh(&ar->data_lock);
		}
	}
	return NULL;
}

static void ath12k_scan_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct ath12k *ar;
	struct wmi_scan_event scan_ev = {0};

	if (ath12k_pull_scan_ev(ab, skb, &scan_ev) != 0) {
		ath12k_warn(ab, "failed to extract scan event");
		return;
	}

	rcu_read_lock();

	/* In case the scan was cancelled, ex. during interface teardown,
	 * the interface will not be found in active interfaces.
	 * Rather, in such scenarios, iterate over the active pdev's to
	 * search 'ar' if the corresponding 'ar' scan is ABORTING and the
	 * aborting scan's vdev id matches this event info.
	 */
	if (le32_to_cpu(scan_ev.event_type) == WMI_SCAN_EVENT_COMPLETED &&
	    le32_to_cpu(scan_ev.reason) == WMI_SCAN_REASON_CANCELLED)
		ar = ath12k_get_ar_on_scan_abort(ab, le32_to_cpu(scan_ev.vdev_id));
	else
		ar = ath12k_mac_get_ar_by_vdev_id(ab, le32_to_cpu(scan_ev.vdev_id));

	if (!ar) {
		ath12k_warn(ab, "Received scan event for unknown vdev");
		rcu_read_unlock();
		return;
	}

	spin_lock_bh(&ar->data_lock);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "scan event %s type %d reason %d freq %d req_id %d scan_id %d vdev_id %d state %s (%d)\n",
		   ath12k_wmi_event_scan_type_str(le32_to_cpu(scan_ev.event_type),
						  le32_to_cpu(scan_ev.reason)),
		   le32_to_cpu(scan_ev.event_type),
		   le32_to_cpu(scan_ev.reason),
		   le32_to_cpu(scan_ev.channel_freq),
		   le32_to_cpu(scan_ev.scan_req_id),
		   le32_to_cpu(scan_ev.scan_id),
		   le32_to_cpu(scan_ev.vdev_id),
		   ath12k_scan_state_str(ar->scan.state), ar->scan.state);

	switch (le32_to_cpu(scan_ev.event_type)) {
	case WMI_SCAN_EVENT_STARTED:
		ath12k_wmi_event_scan_started(ar);
		break;
	case WMI_SCAN_EVENT_COMPLETED:
		ath12k_wmi_event_scan_completed(ar);
		break;
	case WMI_SCAN_EVENT_BSS_CHANNEL:
		ath12k_wmi_event_scan_bss_chan(ar);
		break;
	case WMI_SCAN_EVENT_FOREIGN_CHAN:
		ath12k_wmi_event_scan_foreign_chan(ar, le32_to_cpu(scan_ev.channel_freq));
		break;
	case WMI_SCAN_EVENT_START_FAILED:
		ath12k_warn(ab, "received scan start failure event\n");
		ath12k_wmi_event_scan_start_failed(ar);
		break;
	case WMI_SCAN_EVENT_DEQUEUED:
		__ath12k_mac_scan_finish(ar);
		break;
	case WMI_SCAN_EVENT_PREEMPTED:
	case WMI_SCAN_EVENT_RESTARTED:
	case WMI_SCAN_EVENT_FOREIGN_CHAN_EXIT:
	default:
		break;
	}

	spin_unlock_bh(&ar->data_lock);

	rcu_read_unlock();
}

static void ath12k_peer_sta_kickout_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_peer_sta_kickout_arg arg = {};
	struct ieee80211_sta *sta;
	struct ath12k_peer *peer;
	struct ath12k *ar;

	if (ath12k_pull_peer_sta_kickout_ev(ab, skb, &arg) != 0) {
		ath12k_warn(ab, "failed to extract peer sta kickout event");
		return;
	}

	rcu_read_lock();

	spin_lock_bh(&ab->base_lock);

	peer = ath12k_peer_find_by_addr(ab, arg.mac_addr);

	if (!peer) {
		ath12k_warn(ab, "peer not found %pM\n",
			    arg.mac_addr);
		goto exit;
	}

	ar = ath12k_mac_get_ar_by_vdev_id(ab, peer->vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in peer sta kickout ev %d",
			    peer->vdev_id);
		goto exit;
	}

	sta = ieee80211_find_sta_by_ifaddr(ar->hw,
					   arg.mac_addr, NULL);
	if (!sta) {
		ath12k_warn(ab, "Spurious quick kickout for STA %pM\n",
			    arg.mac_addr);
		goto exit;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI, "peer sta kickout event %pM",
		   arg.mac_addr);

	ieee80211_report_low_ack(sta, 10);

exit:
	spin_unlock_bh(&ab->base_lock);
	rcu_read_unlock();
}

static void ath12k_roam_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_roam_event roam_ev = {};
	struct ath12k *ar;

	if (ath12k_pull_roam_ev(ab, skb, &roam_ev) != 0) {
		ath12k_warn(ab, "failed to extract roam event");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "wmi roam event vdev %u reason 0x%08x rssi %d\n",
		   roam_ev.vdev_id, roam_ev.reason, roam_ev.rssi);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, le32_to_cpu(roam_ev.vdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in roam ev %d",
			    roam_ev.vdev_id);
		rcu_read_unlock();
		return;
	}

	if (le32_to_cpu(roam_ev.reason) >= WMI_ROAM_REASON_MAX)
		ath12k_warn(ab, "ignoring unknown roam event reason %d on vdev %i\n",
			    roam_ev.reason, roam_ev.vdev_id);

	switch (le32_to_cpu(roam_ev.reason)) {
	case WMI_ROAM_REASON_BEACON_MISS:
		/* TODO: Pending beacon miss and connection_loss_work
		 * implementation
		 * ath12k_mac_handle_beacon_miss(ar, vdev_id);
		 */
		break;
	case WMI_ROAM_REASON_BETTER_AP:
	case WMI_ROAM_REASON_LOW_RSSI:
	case WMI_ROAM_REASON_SUITABLE_AP_FOUND:
	case WMI_ROAM_REASON_HO_FAILED:
		ath12k_warn(ab, "ignoring not implemented roam event reason %d on vdev %i\n",
			    roam_ev.reason, roam_ev.vdev_id);
		break;
	}

	rcu_read_unlock();
}

static void ath12k_chan_info_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_chan_info_event ch_info_ev = {0};
	struct ath12k *ar;
	struct survey_info *survey;
	int idx;
	/* HW channel counters frequency value in hertz */
	u32 cc_freq_hz = ab->cc_freq_hz;

	if (ath12k_pull_chan_info_ev(ab, skb->data, skb->len, &ch_info_ev) != 0) {
		ath12k_warn(ab, "failed to extract chan info event");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "chan info vdev_id %d err_code %d freq %d cmd_flags %d noise_floor %d rx_clear_count %d cycle_count %d mac_clk_mhz %d\n",
		   ch_info_ev.vdev_id, ch_info_ev.err_code, ch_info_ev.freq,
		   ch_info_ev.cmd_flags, ch_info_ev.noise_floor,
		   ch_info_ev.rx_clear_count, ch_info_ev.cycle_count,
		   ch_info_ev.mac_clk_mhz);

	if (le32_to_cpu(ch_info_ev.cmd_flags) == WMI_CHAN_INFO_END_RESP) {
		ath12k_dbg(ab, ATH12K_DBG_WMI, "chan info report completed\n");
		return;
	}

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, le32_to_cpu(ch_info_ev.vdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in chan info ev %d",
			    ch_info_ev.vdev_id);
		rcu_read_unlock();
		return;
	}
	spin_lock_bh(&ar->data_lock);

	switch (ar->scan.state) {
	case ATH12K_SCAN_IDLE:
	case ATH12K_SCAN_STARTING:
		ath12k_warn(ab, "received chan info event without a scan request, ignoring\n");
		goto exit;
	case ATH12K_SCAN_RUNNING:
	case ATH12K_SCAN_ABORTING:
		break;
	}

	idx = freq_to_idx(ar, le32_to_cpu(ch_info_ev.freq));
	if (idx >= ARRAY_SIZE(ar->survey)) {
		ath12k_warn(ab, "chan info: invalid frequency %d (idx %d out of bounds)\n",
			    ch_info_ev.freq, idx);
		goto exit;
	}

	/* If FW provides MAC clock frequency in Mhz, overriding the initialized
	 * HW channel counters frequency value
	 */
	if (ch_info_ev.mac_clk_mhz)
		cc_freq_hz = (le32_to_cpu(ch_info_ev.mac_clk_mhz) * 1000);

	if (ch_info_ev.cmd_flags == WMI_CHAN_INFO_START_RESP) {
		survey = &ar->survey[idx];
		memset(survey, 0, sizeof(*survey));
		survey->noise = le32_to_cpu(ch_info_ev.noise_floor);
		survey->filled = SURVEY_INFO_NOISE_DBM | SURVEY_INFO_TIME |
				 SURVEY_INFO_TIME_BUSY;
		survey->time = div_u64(le32_to_cpu(ch_info_ev.cycle_count), cc_freq_hz);
		survey->time_busy = div_u64(le32_to_cpu(ch_info_ev.rx_clear_count),
					    cc_freq_hz);
	}
exit:
	spin_unlock_bh(&ar->data_lock);
	rcu_read_unlock();
}

static void
ath12k_pdev_bss_chan_info_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_pdev_bss_chan_info_event bss_ch_info_ev = {};
	struct survey_info *survey;
	struct ath12k *ar;
	u32 cc_freq_hz = ab->cc_freq_hz;
	u64 busy, total, tx, rx, rx_bss;
	int idx;

	if (ath12k_pull_pdev_bss_chan_info_ev(ab, skb, &bss_ch_info_ev) != 0) {
		ath12k_warn(ab, "failed to extract pdev bss chan info event");
		return;
	}

	busy = (u64)(le32_to_cpu(bss_ch_info_ev.rx_clear_count_high)) << 32 |
		le32_to_cpu(bss_ch_info_ev.rx_clear_count_low);

	total = (u64)(le32_to_cpu(bss_ch_info_ev.cycle_count_high)) << 32 |
		le32_to_cpu(bss_ch_info_ev.cycle_count_low);

	tx = (u64)(le32_to_cpu(bss_ch_info_ev.tx_cycle_count_high)) << 32 |
		le32_to_cpu(bss_ch_info_ev.tx_cycle_count_low);

	rx = (u64)(le32_to_cpu(bss_ch_info_ev.rx_cycle_count_high)) << 32 |
		le32_to_cpu(bss_ch_info_ev.rx_cycle_count_low);

	rx_bss = (u64)(le32_to_cpu(bss_ch_info_ev.rx_bss_cycle_count_high)) << 32 |
		le32_to_cpu(bss_ch_info_ev.rx_bss_cycle_count_low);

	ath12k_dbg(ab, ATH12K_DBG_WMI,
#if defined(__linux__)
		   "pdev bss chan info:\n pdev_id: %d freq: %d noise: %d cycle: busy %llu total %llu tx %llu rx %llu rx_bss %llu\n",
		   bss_ch_info_ev.pdev_id, bss_ch_info_ev.freq,
		   bss_ch_info_ev.noise_floor, busy, total,
		   tx, rx, rx_bss);
#elif defined(__FreeBSD__)
		   "pdev bss chan info:\n pdev_id: %d freq: %d noise: %d cycle: busy %ju total %ju tx %ju rx %ju rx_bss %ju\n",
		   bss_ch_info_ev.pdev_id, bss_ch_info_ev.freq,
		   bss_ch_info_ev.noise_floor, (uintmax_t)busy, (uintmax_t)total,
		   (uintmax_t)tx, (uintmax_t)rx, (uintmax_t)rx_bss);
#endif

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_pdev_id(ab, le32_to_cpu(bss_ch_info_ev.pdev_id));

	if (!ar) {
		ath12k_warn(ab, "invalid pdev id %d in bss_chan_info event\n",
			    bss_ch_info_ev.pdev_id);
		rcu_read_unlock();
		return;
	}

	spin_lock_bh(&ar->data_lock);
	idx = freq_to_idx(ar, le32_to_cpu(bss_ch_info_ev.freq));
	if (idx >= ARRAY_SIZE(ar->survey)) {
		ath12k_warn(ab, "bss chan info: invalid frequency %d (idx %d out of bounds)\n",
			    bss_ch_info_ev.freq, idx);
		goto exit;
	}

	survey = &ar->survey[idx];

	survey->noise     = le32_to_cpu(bss_ch_info_ev.noise_floor);
	survey->time      = div_u64(total, cc_freq_hz);
	survey->time_busy = div_u64(busy, cc_freq_hz);
	survey->time_rx   = div_u64(rx_bss, cc_freq_hz);
	survey->time_tx   = div_u64(tx, cc_freq_hz);
	survey->filled   |= (SURVEY_INFO_NOISE_DBM |
			     SURVEY_INFO_TIME |
			     SURVEY_INFO_TIME_BUSY |
			     SURVEY_INFO_TIME_RX |
			     SURVEY_INFO_TIME_TX);
exit:
	spin_unlock_bh(&ar->data_lock);
	complete(&ar->bss_survey_done);

	rcu_read_unlock();
}

static void ath12k_vdev_install_key_compl_event(struct ath12k_base *ab,
						struct sk_buff *skb)
{
	struct wmi_vdev_install_key_complete_arg install_key_compl = {0};
	struct ath12k *ar;

	if (ath12k_pull_vdev_install_key_compl_ev(ab, skb, &install_key_compl) != 0) {
		ath12k_warn(ab, "failed to extract install key compl event");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "vdev install key ev idx %d flags %08x macaddr %pM status %d\n",
		   install_key_compl.key_idx, install_key_compl.key_flags,
		   install_key_compl.macaddr, install_key_compl.status);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, install_key_compl.vdev_id);
	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in install key compl ev %d",
			    install_key_compl.vdev_id);
		rcu_read_unlock();
		return;
	}

	ar->install_key_status = 0;

	if (install_key_compl.status != WMI_VDEV_INSTALL_KEY_COMPL_STATUS_SUCCESS) {
		ath12k_warn(ab, "install key failed for %pM status %d\n",
			    install_key_compl.macaddr, install_key_compl.status);
		ar->install_key_status = install_key_compl.status;
	}

	complete(&ar->install_key_done);
	rcu_read_unlock();
}

static int ath12k_wmi_tlv_services_parser(struct ath12k_base *ab,
					  u16 tag, u16 len,
					  const void *ptr,
					  void *data)
{
	const struct wmi_service_available_event *ev;
#if defined(__linux__)
	u32 *wmi_ext2_service_bitmap;
#elif defined(__FreeBSD__)
	const u32 *wmi_ext2_service_bitmap;
#endif
	int i, j;
	u16 expected_len;

	expected_len = WMI_SERVICE_SEGMENT_BM_SIZE32 * sizeof(u32);
	if (len < expected_len) {
		ath12k_warn(ab, "invalid length %d for the WMI services available tag 0x%x\n",
			    len, tag);
		return -EINVAL;
	}

	switch (tag) {
	case WMI_TAG_SERVICE_AVAILABLE_EVENT:
#if defined(__linux__)
		ev = (struct wmi_service_available_event *)ptr;
#elif defined(__FreeBSD__)
		ev = (const struct wmi_service_available_event *)ptr;
#endif
		for (i = 0, j = WMI_MAX_SERVICE;
		     i < WMI_SERVICE_SEGMENT_BM_SIZE32 && j < WMI_MAX_EXT_SERVICE;
		     i++) {
			do {
				if (le32_to_cpu(ev->wmi_service_segment_bitmap[i]) &
				    BIT(j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32))
					set_bit(j, ab->wmi_ab.svc_map);
			} while (++j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32);
		}

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "wmi_ext_service_bitmap 0x%x 0x%x 0x%x 0x%x",
			   ev->wmi_service_segment_bitmap[0],
			   ev->wmi_service_segment_bitmap[1],
			   ev->wmi_service_segment_bitmap[2],
			   ev->wmi_service_segment_bitmap[3]);
		break;
	case WMI_TAG_ARRAY_UINT32:
#if defined(__linux__)
		wmi_ext2_service_bitmap = (u32 *)ptr;
#elif defined(__FreeBSD__)
		wmi_ext2_service_bitmap = (const u32 *)ptr;
#endif
		for (i = 0, j = WMI_MAX_EXT_SERVICE;
		     i < WMI_SERVICE_SEGMENT_BM_SIZE32 && j < WMI_MAX_EXT2_SERVICE;
		     i++) {
			do {
				if (wmi_ext2_service_bitmap[i] &
				    BIT(j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32))
					set_bit(j, ab->wmi_ab.svc_map);
			} while (++j % WMI_AVAIL_SERVICE_BITS_IN_SIZE32);
		}

		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "wmi_ext2_service_bitmap 0x%04x 0x%04x 0x%04x 0x%04x",
			   wmi_ext2_service_bitmap[0], wmi_ext2_service_bitmap[1],
			   wmi_ext2_service_bitmap[2], wmi_ext2_service_bitmap[3]);
		break;
	}
	return 0;
}

static int ath12k_service_available_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	int ret;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_tlv_services_parser,
				  NULL);
	return ret;
}

static void ath12k_peer_assoc_conf_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_peer_assoc_conf_arg peer_assoc_conf = {0};
	struct ath12k *ar;

	if (ath12k_pull_peer_assoc_conf_ev(ab, skb, &peer_assoc_conf) != 0) {
		ath12k_warn(ab, "failed to extract peer assoc conf event");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "peer assoc conf ev vdev id %d macaddr %pM\n",
		   peer_assoc_conf.vdev_id, peer_assoc_conf.macaddr);

	rcu_read_lock();
	ar = ath12k_mac_get_ar_by_vdev_id(ab, peer_assoc_conf.vdev_id);

	if (!ar) {
		ath12k_warn(ab, "invalid vdev id in peer assoc conf ev %d",
			    peer_assoc_conf.vdev_id);
		rcu_read_unlock();
		return;
	}

	complete(&ar->peer_assoc_done);
	rcu_read_unlock();
}

static void ath12k_update_stats_event(struct ath12k_base *ab, struct sk_buff *skb)
{
}

/* PDEV_CTL_FAILSAFE_CHECK_EVENT is received from FW when the frequency scanned
 * is not part of BDF CTL(Conformance test limits) table entries.
 */
static void ath12k_pdev_ctl_failsafe_check_event(struct ath12k_base *ab,
						 struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_pdev_ctl_failsafe_chk_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PDEV_CTL_FAILSAFE_CHECK_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch pdev ctl failsafe check ev");
		kfree(tb);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pdev ctl failsafe check ev status %d\n",
		   ev->ctl_failsafe_status);

	/* If ctl_failsafe_status is set to 1 FW will max out the Transmit power
	 * to 10 dBm else the CTL power entry in the BDF would be picked up.
	 */
	if (ev->ctl_failsafe_status != 0)
		ath12k_warn(ab, "pdev ctl failsafe failure status %d",
			    ev->ctl_failsafe_status);

	kfree(tb);
}

static void
ath12k_wmi_process_csa_switch_count_event(struct ath12k_base *ab,
					  const struct ath12k_wmi_pdev_csa_event *ev,
					  const u32 *vdev_ids)
{
	int i;
	struct ath12k_vif *arvif;

	/* Finish CSA once the switch count becomes NULL */
	if (ev->current_switch_count)
		return;

	rcu_read_lock();
	for (i = 0; i < le32_to_cpu(ev->num_vdevs); i++) {
		arvif = ath12k_mac_get_arvif_by_vdev_id(ab, vdev_ids[i]);

		if (!arvif) {
			ath12k_warn(ab, "Recvd csa status for unknown vdev %d",
				    vdev_ids[i]);
			continue;
		}

		if (arvif->is_up && arvif->vif->bss_conf.csa_active)
			ieee80211_csa_finish(arvif->vif);
	}
	rcu_read_unlock();
}

static void
ath12k_wmi_pdev_csa_switch_count_status_event(struct ath12k_base *ab,
					      struct sk_buff *skb)
{
	const void **tb;
	const struct ath12k_wmi_pdev_csa_event *ev;
	const u32 *vdev_ids;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PDEV_CSA_SWITCH_COUNT_STATUS_EVENT];
	vdev_ids = tb[WMI_TAG_ARRAY_UINT32];

	if (!ev || !vdev_ids) {
		ath12k_warn(ab, "failed to fetch pdev csa switch count ev");
		kfree(tb);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pdev csa switch count %d for pdev %d, num_vdevs %d",
		   ev->current_switch_count, ev->pdev_id,
		   ev->num_vdevs);

	ath12k_wmi_process_csa_switch_count_event(ab, ev, vdev_ids);

	kfree(tb);
}

static void
ath12k_wmi_pdev_dfs_radar_detected_event(struct ath12k_base *ab, struct sk_buff *skb)
{
	const void **tb;
	const struct ath12k_wmi_pdev_radar_event *ev;
	struct ath12k *ar;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab, "failed to parse tlv: %d\n", ret);
		return;
	}

	ev = tb[WMI_TAG_PDEV_DFS_RADAR_DETECTION_EVENT];

	if (!ev) {
		ath12k_warn(ab, "failed to fetch pdev dfs radar detected ev");
		kfree(tb);
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pdev dfs radar detected on pdev %d, detection mode %d, chan freq %d, chan_width %d, detector id %d, seg id %d, timestamp %d, chirp %d, freq offset %d, sidx %d",
		   ev->pdev_id, ev->detection_mode, ev->chan_freq, ev->chan_width,
		   ev->detector_id, ev->segment_id, ev->timestamp, ev->is_chirp,
		   ev->freq_offset, ev->sidx);

	ar = ath12k_mac_get_ar_by_pdev_id(ab, le32_to_cpu(ev->pdev_id));

	if (!ar) {
		ath12k_warn(ab, "radar detected in invalid pdev %d\n",
			    ev->pdev_id);
		goto exit;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_REG, "DFS Radar Detected in pdev %d\n",
		   ev->pdev_id);

	if (ar->dfs_block_radar_events)
		ath12k_info(ab, "DFS Radar detected, but ignored as requested\n");
	else
		ieee80211_radar_detected(ar->hw);

exit:
	kfree(tb);
}

static void
ath12k_wmi_pdev_temperature_event(struct ath12k_base *ab,
				  struct sk_buff *skb)
{
	struct ath12k *ar;
	struct wmi_pdev_temperature_event ev = {0};

	if (ath12k_pull_pdev_temp_ev(ab, skb->data, skb->len, &ev) != 0) {
		ath12k_warn(ab, "failed to extract pdev temperature event");
		return;
	}

	ath12k_dbg(ab, ATH12K_DBG_WMI,
		   "pdev temperature ev temp %d pdev_id %d\n", ev.temp, ev.pdev_id);

	ar = ath12k_mac_get_ar_by_pdev_id(ab, le32_to_cpu(ev.pdev_id));
	if (!ar) {
		ath12k_warn(ab, "invalid pdev id in pdev temperature ev %d", ev.pdev_id);
		return;
	}
}

static void ath12k_fils_discovery_event(struct ath12k_base *ab,
					struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_fils_discovery_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab,
			    "failed to parse FILS discovery event tlv %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_HOST_SWFDA_EVENT];
	if (!ev) {
		ath12k_warn(ab, "failed to fetch FILS discovery event\n");
		kfree(tb);
		return;
	}

	ath12k_warn(ab,
		    "FILS discovery frame expected from host for vdev_id: %u, transmission scheduled at %u, next TBTT: %u\n",
		    ev->vdev_id, ev->fils_tt, ev->tbtt);

	kfree(tb);
}

static void ath12k_probe_resp_tx_status_event(struct ath12k_base *ab,
					      struct sk_buff *skb)
{
	const void **tb;
	const struct wmi_probe_resp_tx_status_event *ev;
	int ret;

	tb = ath12k_wmi_tlv_parse_alloc(ab, skb->data, skb->len, GFP_ATOMIC);
	if (IS_ERR(tb)) {
		ret = PTR_ERR(tb);
		ath12k_warn(ab,
			    "failed to parse probe response transmission status event tlv: %d\n",
			    ret);
		return;
	}

	ev = tb[WMI_TAG_OFFLOAD_PRB_RSP_TX_STATUS_EVENT];
	if (!ev) {
		ath12k_warn(ab,
			    "failed to fetch probe response transmission status event");
		kfree(tb);
		return;
	}

	if (ev->tx_status)
		ath12k_warn(ab,
			    "Probe response transmission failed for vdev_id %u, status %u\n",
			    ev->vdev_id, ev->tx_status);

	kfree(tb);
}

static void ath12k_wmi_op_rx(struct ath12k_base *ab, struct sk_buff *skb)
{
	struct wmi_cmd_hdr *cmd_hdr;
	enum wmi_tlv_event_id id;

	cmd_hdr = (struct wmi_cmd_hdr *)skb->data;
	id = le32_get_bits(cmd_hdr->cmd_id, WMI_CMD_HDR_CMD_ID);

	if (!skb_pull(skb, sizeof(struct wmi_cmd_hdr)))
		goto out;

	switch (id) {
		/* Process all the WMI events here */
	case WMI_SERVICE_READY_EVENTID:
		ath12k_service_ready_event(ab, skb);
		break;
	case WMI_SERVICE_READY_EXT_EVENTID:
		ath12k_service_ready_ext_event(ab, skb);
		break;
	case WMI_SERVICE_READY_EXT2_EVENTID:
		ath12k_service_ready_ext2_event(ab, skb);
		break;
	case WMI_REG_CHAN_LIST_CC_EXT_EVENTID:
		ath12k_reg_chan_list_event(ab, skb);
		break;
	case WMI_READY_EVENTID:
		ath12k_ready_event(ab, skb);
		break;
	case WMI_PEER_DELETE_RESP_EVENTID:
		ath12k_peer_delete_resp_event(ab, skb);
		break;
	case WMI_VDEV_START_RESP_EVENTID:
		ath12k_vdev_start_resp_event(ab, skb);
		break;
	case WMI_OFFLOAD_BCN_TX_STATUS_EVENTID:
		ath12k_bcn_tx_status_event(ab, skb);
		break;
	case WMI_VDEV_STOPPED_EVENTID:
		ath12k_vdev_stopped_event(ab, skb);
		break;
	case WMI_MGMT_RX_EVENTID:
		ath12k_mgmt_rx_event(ab, skb);
		/* mgmt_rx_event() owns the skb now! */
		return;
	case WMI_MGMT_TX_COMPLETION_EVENTID:
		ath12k_mgmt_tx_compl_event(ab, skb);
		break;
	case WMI_SCAN_EVENTID:
		ath12k_scan_event(ab, skb);
		break;
	case WMI_PEER_STA_KICKOUT_EVENTID:
		ath12k_peer_sta_kickout_event(ab, skb);
		break;
	case WMI_ROAM_EVENTID:
		ath12k_roam_event(ab, skb);
		break;
	case WMI_CHAN_INFO_EVENTID:
		ath12k_chan_info_event(ab, skb);
		break;
	case WMI_PDEV_BSS_CHAN_INFO_EVENTID:
		ath12k_pdev_bss_chan_info_event(ab, skb);
		break;
	case WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID:
		ath12k_vdev_install_key_compl_event(ab, skb);
		break;
	case WMI_SERVICE_AVAILABLE_EVENTID:
		ath12k_service_available_event(ab, skb);
		break;
	case WMI_PEER_ASSOC_CONF_EVENTID:
		ath12k_peer_assoc_conf_event(ab, skb);
		break;
	case WMI_UPDATE_STATS_EVENTID:
		ath12k_update_stats_event(ab, skb);
		break;
	case WMI_PDEV_CTL_FAILSAFE_CHECK_EVENTID:
		ath12k_pdev_ctl_failsafe_check_event(ab, skb);
		break;
	case WMI_PDEV_CSA_SWITCH_COUNT_STATUS_EVENTID:
		ath12k_wmi_pdev_csa_switch_count_status_event(ab, skb);
		break;
	case WMI_PDEV_TEMPERATURE_EVENTID:
		ath12k_wmi_pdev_temperature_event(ab, skb);
		break;
	case WMI_PDEV_DMA_RING_BUF_RELEASE_EVENTID:
		ath12k_wmi_pdev_dma_ring_buf_release_event(ab, skb);
		break;
	case WMI_HOST_FILS_DISCOVERY_EVENTID:
		ath12k_fils_discovery_event(ab, skb);
		break;
	case WMI_OFFLOAD_PROB_RESP_TX_STATUS_EVENTID:
		ath12k_probe_resp_tx_status_event(ab, skb);
		break;
	/* add Unsupported events here */
	case WMI_TBTTOFFSET_EXT_UPDATE_EVENTID:
	case WMI_PEER_OPER_MODE_CHANGE_EVENTID:
	case WMI_TWT_ENABLE_EVENTID:
	case WMI_TWT_DISABLE_EVENTID:
	case WMI_PDEV_DMA_RING_CFG_RSP_EVENTID:
		ath12k_dbg(ab, ATH12K_DBG_WMI,
			   "ignoring unsupported event 0x%x\n", id);
		break;
	case WMI_PDEV_DFS_RADAR_DETECTION_EVENTID:
		ath12k_wmi_pdev_dfs_radar_detected_event(ab, skb);
		break;
	case WMI_VDEV_DELETE_RESP_EVENTID:
		ath12k_vdev_delete_resp_event(ab, skb);
		break;
	/* TODO: Add remaining events */
	default:
		ath12k_dbg(ab, ATH12K_DBG_WMI, "Unknown eventid: 0x%x\n", id);
		break;
	}

out:
	dev_kfree_skb(skb);
}

static int ath12k_connect_pdev_htc_service(struct ath12k_base *ab,
					   u32 pdev_idx)
{
	int status;
	u32 svc_id[] = { ATH12K_HTC_SVC_ID_WMI_CONTROL,
			 ATH12K_HTC_SVC_ID_WMI_CONTROL_MAC1,
			 ATH12K_HTC_SVC_ID_WMI_CONTROL_MAC2 };
	struct ath12k_htc_svc_conn_req conn_req = {};
	struct ath12k_htc_svc_conn_resp conn_resp = {};

	/* these fields are the same for all service endpoints */
	conn_req.ep_ops.ep_tx_complete = ath12k_wmi_htc_tx_complete;
	conn_req.ep_ops.ep_rx_complete = ath12k_wmi_op_rx;
	conn_req.ep_ops.ep_tx_credits = ath12k_wmi_op_ep_tx_credits;

	/* connect to control service */
	conn_req.service_id = svc_id[pdev_idx];

	status = ath12k_htc_connect_service(&ab->htc, &conn_req, &conn_resp);
	if (status) {
		ath12k_warn(ab, "failed to connect to WMI CONTROL service status: %d\n",
			    status);
		return status;
	}

	ab->wmi_ab.wmi_endpoint_id[pdev_idx] = conn_resp.eid;
	ab->wmi_ab.wmi[pdev_idx].eid = conn_resp.eid;
	ab->wmi_ab.max_msg_len[pdev_idx] = conn_resp.max_msg_len;

	return 0;
}

static int
ath12k_wmi_send_unit_test_cmd(struct ath12k *ar,
			      struct wmi_unit_test_cmd ut_cmd,
			      u32 *test_args)
{
	struct ath12k_wmi_pdev *wmi = ar->wmi;
	struct wmi_unit_test_cmd *cmd;
	struct sk_buff *skb;
	struct wmi_tlv *tlv;
#if defined(__linux__)
	void *ptr;
#elif defined(__FreeBSD__)
	u8 *ptr;
#endif
	u32 *ut_cmd_args;
	int buf_len, arg_len;
	int ret;
	int i;

	arg_len = sizeof(u32) * le32_to_cpu(ut_cmd.num_args);
	buf_len = sizeof(ut_cmd) + arg_len + TLV_HDR_SIZE;

	skb = ath12k_wmi_alloc_skb(wmi->wmi_ab, buf_len);
	if (!skb)
		return -ENOMEM;

	cmd = (struct wmi_unit_test_cmd *)skb->data;
	cmd->tlv_header = ath12k_wmi_tlv_cmd_hdr(WMI_TAG_UNIT_TEST_CMD,
						 sizeof(ut_cmd));

	cmd->vdev_id = ut_cmd.vdev_id;
	cmd->module_id = ut_cmd.module_id;
	cmd->num_args = ut_cmd.num_args;
	cmd->diag_token = ut_cmd.diag_token;

	ptr = skb->data + sizeof(ut_cmd);

#if defined(__linux__)
	tlv = ptr;
#elif defined(__FreeBSD__)
	tlv = (void *)ptr;
#endif
	tlv->header = ath12k_wmi_tlv_hdr(WMI_TAG_ARRAY_UINT32, arg_len);

	ptr += TLV_HDR_SIZE;

#if defined(__linux__)
	ut_cmd_args = ptr;
#elif defined(__FreeBSD__)
	ut_cmd_args = (void *)ptr;
#endif
	for (i = 0; i < le32_to_cpu(ut_cmd.num_args); i++)
		ut_cmd_args[i] = test_args[i];

	ath12k_dbg(ar->ab, ATH12K_DBG_WMI,
		   "WMI unit test : module %d vdev %d n_args %d token %d\n",
		   cmd->module_id, cmd->vdev_id, cmd->num_args,
		   cmd->diag_token);

	ret = ath12k_wmi_cmd_send(wmi, skb, WMI_UNIT_TEST_CMDID);

	if (ret) {
		ath12k_warn(ar->ab, "failed to send WMI_UNIT_TEST CMD :%d\n",
			    ret);
		dev_kfree_skb(skb);
	}

	return ret;
}

int ath12k_wmi_simulate_radar(struct ath12k *ar)
{
	struct ath12k_vif *arvif;
	u32 dfs_args[DFS_MAX_TEST_ARGS];
	struct wmi_unit_test_cmd wmi_ut;
	bool arvif_found = false;

	list_for_each_entry(arvif, &ar->arvifs, list) {
		if (arvif->is_started && arvif->vdev_type == WMI_VDEV_TYPE_AP) {
			arvif_found = true;
			break;
		}
	}

	if (!arvif_found)
		return -EINVAL;

	dfs_args[DFS_TEST_CMDID] = 0;
	dfs_args[DFS_TEST_PDEV_ID] = ar->pdev->pdev_id;
	/* Currently we could pass segment_id(b0 - b1), chirp(b2)
	 * freq offset (b3 - b10) to unit test. For simulation
	 * purpose this can be set to 0 which is valid.
	 */
	dfs_args[DFS_TEST_RADAR_PARAM] = 0;

	wmi_ut.vdev_id = cpu_to_le32(arvif->vdev_id);
	wmi_ut.module_id = cpu_to_le32(DFS_UNIT_TEST_MODULE);
	wmi_ut.num_args = cpu_to_le32(DFS_MAX_TEST_ARGS);
	wmi_ut.diag_token = cpu_to_le32(DFS_UNIT_TEST_TOKEN);

	ath12k_dbg(ar->ab, ATH12K_DBG_REG, "Triggering Radar Simulation\n");

	return ath12k_wmi_send_unit_test_cmd(ar, wmi_ut, dfs_args);
}

int ath12k_wmi_connect(struct ath12k_base *ab)
{
	u32 i;
	u8 wmi_ep_count;

	wmi_ep_count = ab->htc.wmi_ep_count;
	if (wmi_ep_count > ab->hw_params->max_radios)
		return -1;

	for (i = 0; i < wmi_ep_count; i++)
		ath12k_connect_pdev_htc_service(ab, i);

	return 0;
}

static void ath12k_wmi_pdev_detach(struct ath12k_base *ab, u8 pdev_id)
{
	if (WARN_ON(pdev_id >= MAX_RADIOS))
		return;

	/* TODO: Deinit any pdev specific wmi resource */
}

int ath12k_wmi_pdev_attach(struct ath12k_base *ab,
			   u8 pdev_id)
{
	struct ath12k_wmi_pdev *wmi_handle;

	if (pdev_id >= ab->hw_params->max_radios)
		return -EINVAL;

	wmi_handle = &ab->wmi_ab.wmi[pdev_id];

	wmi_handle->wmi_ab = &ab->wmi_ab;

	ab->wmi_ab.ab = ab;
	/* TODO: Init remaining resource specific to pdev */

	return 0;
}

int ath12k_wmi_attach(struct ath12k_base *ab)
{
	int ret;

	ret = ath12k_wmi_pdev_attach(ab, 0);
	if (ret)
		return ret;

	ab->wmi_ab.ab = ab;
	ab->wmi_ab.preferred_hw_mode = WMI_HOST_HW_MODE_MAX;

	/* It's overwritten when service_ext_ready is handled */
	if (ab->hw_params->single_pdev_only)
		ab->wmi_ab.preferred_hw_mode = WMI_HOST_HW_MODE_SINGLE;

	/* TODO: Init remaining wmi soc resources required */
	init_completion(&ab->wmi_ab.service_ready);
	init_completion(&ab->wmi_ab.unified_ready);

	return 0;
}

void ath12k_wmi_detach(struct ath12k_base *ab)
{
	int i;

	/* TODO: Deinit wmi resource specific to SOC as required */

	for (i = 0; i < ab->htc.wmi_ep_count; i++)
		ath12k_wmi_pdev_detach(ab, i);

	ath12k_wmi_free_dbring_caps(ab);
}
