// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/* Copyright(c) 2019-2020  Realtek Corporation
 */

#include "cam.h"
#include "chan.h"
#include "coex.h"
#include "debug.h"
#include "fw.h"
#include "mac.h"
#include "phy.h"
#include "ps.h"
#include "reg.h"
#include "sar.h"
#include "ser.h"
#include "util.h"
#include "wow.h"

#if defined(__FreeBSD__)
DEFINE_GUARD(mutex, struct mutex *, mutex_lock(_T), mutex_unlock(_T))
#endif

static void rtw89_ops_tx(struct ieee80211_hw *hw,
			 struct ieee80211_tx_control *control,
			 struct sk_buff *skb)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct ieee80211_tx_info *info = IEEE80211_SKB_CB(skb);
	struct ieee80211_vif *vif = info->control.vif;
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct ieee80211_sta *sta = control->sta;
	u32 flags = IEEE80211_SKB_CB(skb)->flags;
	int ret, qsel;

	if (rtwvif->offchan && !(flags & IEEE80211_TX_CTL_TX_OFFCHAN) && sta) {
		struct rtw89_sta *rtwsta = sta_to_rtwsta(sta);

		rtw89_debug(rtwdev, RTW89_DBG_TXRX, "ops_tx during offchan\n");
		skb_queue_tail(&rtwsta->roc_queue, skb);
		return;
	}

	ret = rtw89_core_tx_write(rtwdev, vif, sta, skb, &qsel);
	if (ret) {
		rtw89_err(rtwdev, "failed to transmit skb: %d\n", ret);
		ieee80211_free_txskb(hw, skb);
		return;
	}
	rtw89_core_tx_kick_off(rtwdev, qsel);
}

static void rtw89_ops_wake_tx_queue(struct ieee80211_hw *hw,
				    struct ieee80211_txq *txq)
{
	struct rtw89_dev *rtwdev = hw->priv;

	ieee80211_schedule_txq(hw, txq);
	queue_work(rtwdev->txq_wq, &rtwdev->txq_work);
}

static int rtw89_ops_start(struct ieee80211_hw *hw)
{
	struct rtw89_dev *rtwdev = hw->priv;
	int ret;

	mutex_lock(&rtwdev->mutex);
	ret = rtw89_core_start(rtwdev);
	mutex_unlock(&rtwdev->mutex);

	return ret;
}

static void rtw89_ops_stop(struct ieee80211_hw *hw, bool suspend)
{
	struct rtw89_dev *rtwdev = hw->priv;

	mutex_lock(&rtwdev->mutex);
	rtw89_core_stop(rtwdev);
	mutex_unlock(&rtwdev->mutex);
}

static int rtw89_ops_config(struct ieee80211_hw *hw, u32 changed)
{
	struct rtw89_dev *rtwdev = hw->priv;

	/* let previous ips work finish to ensure we don't leave ips twice */
	cancel_work_sync(&rtwdev->ips_work);

	mutex_lock(&rtwdev->mutex);
	rtw89_leave_ps_mode(rtwdev);

	if ((changed & IEEE80211_CONF_CHANGE_IDLE) &&
	    !(hw->conf.flags & IEEE80211_CONF_IDLE))
		rtw89_leave_ips(rtwdev);

	if (changed & IEEE80211_CONF_CHANGE_CHANNEL) {
		rtw89_config_entity_chandef(rtwdev, RTW89_CHANCTX_0,
					    &hw->conf.chandef);
		rtw89_set_channel(rtwdev);
	}

	if ((changed & IEEE80211_CONF_CHANGE_IDLE) &&
	    (hw->conf.flags & IEEE80211_CONF_IDLE) &&
	    !rtwdev->scanning)
		rtw89_enter_ips(rtwdev);

	mutex_unlock(&rtwdev->mutex);

	return 0;
}

static int __rtw89_ops_add_iface_link(struct rtw89_dev *rtwdev,
				      struct rtw89_vif_link *rtwvif_link)
{
	struct ieee80211_bss_conf *bss_conf;
	int ret;

	rtw89_leave_ps_mode(rtwdev);

	rtw89_vif_type_mapping(rtwvif_link, false);

	INIT_WORK(&rtwvif_link->update_beacon_work, rtw89_core_update_beacon_work);
	INIT_LIST_HEAD(&rtwvif_link->general_pkt_list);

	rtwvif_link->hit_rule = 0;
	rtwvif_link->bcn_hit_cond = 0;
	rtwvif_link->chanctx_assigned = false;
	rtwvif_link->chanctx_idx = RTW89_CHANCTX_0;
	rtwvif_link->reg_6ghz_power = RTW89_REG_6GHZ_POWER_DFLT;

	rcu_read_lock();

	bss_conf = rtw89_vif_rcu_dereference_link(rtwvif_link, true);
	ether_addr_copy(rtwvif_link->mac_addr, bss_conf->addr);

	rcu_read_unlock();

	ret = rtw89_mac_add_vif(rtwdev, rtwvif_link);
	if (ret)
		return ret;

	rtw89_btc_ntfy_role_info(rtwdev, rtwvif_link, NULL, BTC_ROLE_START);
	return 0;
}

static void __rtw89_ops_remove_iface_link(struct rtw89_dev *rtwdev,
					  struct rtw89_vif_link *rtwvif_link)
{
	mutex_unlock(&rtwdev->mutex);
	cancel_work_sync(&rtwvif_link->update_beacon_work);
	mutex_lock(&rtwdev->mutex);

	rtw89_leave_ps_mode(rtwdev);

	rtw89_btc_ntfy_role_info(rtwdev, rtwvif_link, NULL, BTC_ROLE_STOP);

	rtw89_mac_remove_vif(rtwdev, rtwvif_link);
}

static int rtw89_ops_add_interface(struct ieee80211_hw *hw,
				   struct ieee80211_vif *vif)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct rtw89_vif_link *rtwvif_link;
	u8 mac_id, port;
	int ret = 0;

	rtw89_debug(rtwdev, RTW89_DBG_STATE, "add vif %pM type %d, p2p %d\n",
		    vif->addr, vif->type, vif->p2p);

	mutex_lock(&rtwdev->mutex);

	rtw89_leave_ips_by_hwflags(rtwdev);

	if (RTW89_CHK_FW_FEATURE(BEACON_FILTER, &rtwdev->fw))
		vif->driver_flags |= IEEE80211_VIF_BEACON_FILTER |
				     IEEE80211_VIF_SUPPORTS_CQM_RSSI;

	mac_id = rtw89_acquire_mac_id(rtwdev);
	if (mac_id == RTW89_MAX_MAC_ID_NUM) {
		ret = -ENOSPC;
		goto err;
	}

	port = rtw89_core_acquire_bit_map(rtwdev->hw_port, RTW89_PORT_NUM);
	if (port == RTW89_PORT_NUM) {
		ret = -ENOSPC;
		goto release_macid;
	}

	rtw89_init_vif(rtwdev, rtwvif, mac_id, port);

	rtw89_core_txq_init(rtwdev, vif->txq);

	if (!rtw89_rtwvif_in_list(rtwdev, rtwvif)) {
		list_add_tail(&rtwvif->list, &rtwdev->rtwvifs_list);
		INIT_LIST_HEAD(&rtwvif->mgnt_entry);
	}

	ether_addr_copy(rtwvif->mac_addr, vif->addr);

	rtwvif->offchan = false;
	rtwvif->roc.state = RTW89_ROC_IDLE;
	INIT_DELAYED_WORK(&rtwvif->roc.roc_work, rtw89_roc_work);

	rtw89_traffic_stats_init(rtwdev, &rtwvif->stats);

	rtwvif_link = rtw89_vif_set_link(rtwvif, RTW89_VIF_IDLE_LINK_ID);
	if (!rtwvif_link) {
		ret = -EINVAL;
		goto release_port;
	}

	ret = __rtw89_ops_add_iface_link(rtwdev, rtwvif_link);
	if (ret)
		goto unset_link;

	rtw89_recalc_lps(rtwdev);

	mutex_unlock(&rtwdev->mutex);
	return 0;

unset_link:
	rtw89_vif_unset_link(rtwvif, RTW89_VIF_IDLE_LINK_ID);
release_port:
	list_del_init(&rtwvif->list);
	rtw89_core_release_bit_map(rtwdev->hw_port, port);
release_macid:
	rtw89_release_mac_id(rtwdev, mac_id);
err:
	mutex_unlock(&rtwdev->mutex);

	return ret;
}

static void rtw89_ops_remove_interface(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	u8 macid = rtw89_vif_get_main_macid(rtwvif);
	u8 port = rtw89_vif_get_main_port(rtwvif);
	struct rtw89_vif_link *rtwvif_link;

	rtw89_debug(rtwdev, RTW89_DBG_STATE, "remove vif %pM type %d p2p %d\n",
		    vif->addr, vif->type, vif->p2p);

	cancel_delayed_work_sync(&rtwvif->roc.roc_work);

	mutex_lock(&rtwdev->mutex);

	rtwvif_link = rtwvif->links[RTW89_VIF_IDLE_LINK_ID];
	if (unlikely(!rtwvif_link)) {
		rtw89_err(rtwdev,
			  "%s: rtwvif link (link_id %u) is not active\n",
			  __func__, RTW89_VIF_IDLE_LINK_ID);
		goto bottom;
	}

	__rtw89_ops_remove_iface_link(rtwdev, rtwvif_link);

	rtw89_vif_unset_link(rtwvif, RTW89_VIF_IDLE_LINK_ID);

bottom:
	list_del_init(&rtwvif->list);
	rtw89_core_release_bit_map(rtwdev->hw_port, port);
	rtw89_release_mac_id(rtwdev, macid);

	rtw89_recalc_lps(rtwdev);
	rtw89_enter_ips_by_hwflags(rtwdev);

	mutex_unlock(&rtwdev->mutex);
}

static int rtw89_ops_change_interface(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif,
				      enum nl80211_iftype type, bool p2p)
{
	struct rtw89_dev *rtwdev = hw->priv;
	int ret;

	set_bit(RTW89_FLAG_CHANGING_INTERFACE, rtwdev->flags);

	rtw89_debug(rtwdev, RTW89_DBG_STATE, "change vif %pM (%d)->(%d), p2p (%d)->(%d)\n",
		    vif->addr, vif->type, type, vif->p2p, p2p);

	rtw89_ops_remove_interface(hw, vif);

	vif->type = type;
	vif->p2p = p2p;

	ret = rtw89_ops_add_interface(hw, vif);
	if (ret)
		rtw89_warn(rtwdev, "failed to change interface %d\n", ret);

	clear_bit(RTW89_FLAG_CHANGING_INTERFACE, rtwdev->flags);

	return ret;
}

static void rtw89_ops_configure_filter(struct ieee80211_hw *hw,
				       unsigned int changed_flags,
				       unsigned int *new_flags,
				       u64 multicast)
{
	struct rtw89_dev *rtwdev = hw->priv;
	const struct rtw89_mac_gen_def *mac = rtwdev->chip->mac_def;
	u32 rx_fltr;

	mutex_lock(&rtwdev->mutex);
	rtw89_leave_ps_mode(rtwdev);

	*new_flags &= FIF_ALLMULTI | FIF_OTHER_BSS | FIF_FCSFAIL |
		      FIF_BCN_PRBRESP_PROMISC | FIF_PROBE_REQ;

	if (changed_flags & FIF_ALLMULTI) {
		if (*new_flags & FIF_ALLMULTI)
			rtwdev->hal.rx_fltr &= ~B_AX_A_MC;
		else
			rtwdev->hal.rx_fltr |= B_AX_A_MC;
	}
	if (changed_flags & FIF_FCSFAIL) {
		if (*new_flags & FIF_FCSFAIL)
			rtwdev->hal.rx_fltr |= B_AX_A_CRC32_ERR;
		else
			rtwdev->hal.rx_fltr &= ~B_AX_A_CRC32_ERR;
	}
	if (changed_flags & FIF_OTHER_BSS) {
		if (*new_flags & FIF_OTHER_BSS)
			rtwdev->hal.rx_fltr &= ~B_AX_A_A1_MATCH;
		else
			rtwdev->hal.rx_fltr |= B_AX_A_A1_MATCH;
	}
	if (changed_flags & FIF_BCN_PRBRESP_PROMISC) {
		if (*new_flags & FIF_BCN_PRBRESP_PROMISC) {
			rtwdev->hal.rx_fltr &= ~B_AX_A_BCN_CHK_EN;
			rtwdev->hal.rx_fltr &= ~B_AX_A_BC;
			rtwdev->hal.rx_fltr &= ~B_AX_A_A1_MATCH;
		} else {
			rtwdev->hal.rx_fltr |= B_AX_A_BCN_CHK_EN;
			rtwdev->hal.rx_fltr |= B_AX_A_BC;
			rtwdev->hal.rx_fltr |= B_AX_A_A1_MATCH;
		}
	}
	if (changed_flags & FIF_PROBE_REQ) {
		if (*new_flags & FIF_PROBE_REQ) {
			rtwdev->hal.rx_fltr &= ~B_AX_A_BC_CAM_MATCH;
			rtwdev->hal.rx_fltr &= ~B_AX_A_UC_CAM_MATCH;
		} else {
			rtwdev->hal.rx_fltr |= B_AX_A_BC_CAM_MATCH;
			rtwdev->hal.rx_fltr |= B_AX_A_UC_CAM_MATCH;
		}
	}

	rx_fltr = rtwdev->hal.rx_fltr;

	/* mac80211 doesn't configure filter when HW scan, driver need to
	 * set by itself. However, during P2P scan might have configure
	 * filter to overwrite filter that HW scan needed, so we need to
	 * check scan and append related filter
	 */
	if (rtwdev->scanning) {
		rx_fltr &= ~B_AX_A_BCN_CHK_EN;
		rx_fltr &= ~B_AX_A_BC;
		rx_fltr &= ~B_AX_A_A1_MATCH;
	}

	rtw89_write32_mask(rtwdev,
			   rtw89_mac_reg_by_idx(rtwdev, mac->rx_fltr, RTW89_MAC_0),
			   B_AX_RX_FLTR_CFG_MASK,
			   rx_fltr);
	if (!rtwdev->dbcc_en)
		goto out;
	rtw89_write32_mask(rtwdev,
			   rtw89_mac_reg_by_idx(rtwdev, mac->rx_fltr, RTW89_MAC_1),
			   B_AX_RX_FLTR_CFG_MASK,
			   rx_fltr);

out:
	mutex_unlock(&rtwdev->mutex);
}

static const u8 ac_to_fw_idx[IEEE80211_NUM_ACS] = {
	[IEEE80211_AC_VO] = 3,
	[IEEE80211_AC_VI] = 2,
	[IEEE80211_AC_BE] = 0,
	[IEEE80211_AC_BK] = 1,
};

static u8 rtw89_aifsn_to_aifs(struct rtw89_dev *rtwdev,
			      struct rtw89_vif_link *rtwvif_link, u8 aifsn)
{
	const struct rtw89_chan *chan = rtw89_chan_get(rtwdev,
						       rtwvif_link->chanctx_idx);
	struct ieee80211_bss_conf *bss_conf;
	u8 slot_time;
	u8 sifs;

	rcu_read_lock();

	bss_conf = rtw89_vif_rcu_dereference_link(rtwvif_link, true);
	slot_time = bss_conf->use_short_slot ? 9 : 20;

	rcu_read_unlock();

	sifs = chan->band_type == RTW89_BAND_2G ? 10 : 16;

	return aifsn * slot_time + sifs;
}

static void ____rtw89_conf_tx_edca(struct rtw89_dev *rtwdev,
				   struct rtw89_vif_link *rtwvif_link, u16 ac)
{
	struct ieee80211_tx_queue_params *params = &rtwvif_link->tx_params[ac];
	u32 val;
	u8 ecw_max, ecw_min;
	u8 aifs;

	/* 2^ecw - 1 = cw; ecw = log2(cw + 1) */
	ecw_max = ilog2(params->cw_max + 1);
	ecw_min = ilog2(params->cw_min + 1);
	aifs = rtw89_aifsn_to_aifs(rtwdev, rtwvif_link, params->aifs);
	val = FIELD_PREP(FW_EDCA_PARAM_TXOPLMT_MSK, params->txop) |
	      FIELD_PREP(FW_EDCA_PARAM_CWMAX_MSK, ecw_max) |
	      FIELD_PREP(FW_EDCA_PARAM_CWMIN_MSK, ecw_min) |
	      FIELD_PREP(FW_EDCA_PARAM_AIFS_MSK, aifs);
	rtw89_fw_h2c_set_edca(rtwdev, rtwvif_link, ac_to_fw_idx[ac], val);
}

#define R_MUEDCA_ACS_PARAM(acs) {R_AX_MUEDCA_ ## acs ## _PARAM_0, \
				 R_BE_MUEDCA_ ## acs ## _PARAM_0}

static const u32 ac_to_mu_edca_param[IEEE80211_NUM_ACS][RTW89_CHIP_GEN_NUM] = {
	[IEEE80211_AC_VO] = R_MUEDCA_ACS_PARAM(VO),
	[IEEE80211_AC_VI] = R_MUEDCA_ACS_PARAM(VI),
	[IEEE80211_AC_BE] = R_MUEDCA_ACS_PARAM(BE),
	[IEEE80211_AC_BK] = R_MUEDCA_ACS_PARAM(BK),
};

static void ____rtw89_conf_tx_mu_edca(struct rtw89_dev *rtwdev,
				      struct rtw89_vif_link *rtwvif_link, u16 ac)
{
	struct ieee80211_tx_queue_params *params = &rtwvif_link->tx_params[ac];
	struct ieee80211_he_mu_edca_param_ac_rec *mu_edca;
	int gen = rtwdev->chip->chip_gen;
	u8 aifs, aifsn;
	u16 timer_32us;
	u32 reg;
	u32 val;

	if (!params->mu_edca)
		return;

	mu_edca = &params->mu_edca_param_rec;
	aifsn = FIELD_GET(GENMASK(3, 0), mu_edca->aifsn);
	aifs = aifsn ? rtw89_aifsn_to_aifs(rtwdev, rtwvif_link, aifsn) : 0;
	timer_32us = mu_edca->mu_edca_timer << 8;

	val = FIELD_PREP(B_AX_MUEDCA_BE_PARAM_0_TIMER_MASK, timer_32us) |
	      FIELD_PREP(B_AX_MUEDCA_BE_PARAM_0_CW_MASK, mu_edca->ecw_min_max) |
	      FIELD_PREP(B_AX_MUEDCA_BE_PARAM_0_AIFS_MASK, aifs);
	reg = rtw89_mac_reg_by_idx(rtwdev, ac_to_mu_edca_param[ac][gen],
				   rtwvif_link->mac_idx);
	rtw89_write32(rtwdev, reg, val);

	rtw89_mac_set_hw_muedca_ctrl(rtwdev, rtwvif_link, true);
}

static void __rtw89_conf_tx(struct rtw89_dev *rtwdev,
			    struct rtw89_vif_link *rtwvif_link, u16 ac)
{
	____rtw89_conf_tx_edca(rtwdev, rtwvif_link, ac);
	____rtw89_conf_tx_mu_edca(rtwdev, rtwvif_link, ac);
}

static void rtw89_conf_tx(struct rtw89_dev *rtwdev,
			  struct rtw89_vif_link *rtwvif_link)
{
	u16 ac;

	for (ac = 0; ac < IEEE80211_NUM_ACS; ac++)
		__rtw89_conf_tx(rtwdev, rtwvif_link, ac);
}

static int __rtw89_ops_sta_add(struct rtw89_dev *rtwdev,
			       struct ieee80211_vif *vif,
			       struct ieee80211_sta *sta)
{
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct rtw89_sta *rtwsta = sta_to_rtwsta(sta);
	struct rtw89_vif_link *rtwvif_link;
	struct rtw89_sta_link *rtwsta_link;
	bool acquire_macid = false;
	u8 macid;
	int ret;
	int i;

	if (vif->type == NL80211_IFTYPE_STATION && !sta->tdls) {
		/* for station mode, assign the mac_id from itself */
		macid = rtw89_vif_get_main_macid(rtwvif);
	} else {
		macid = rtw89_acquire_mac_id(rtwdev);
		if (macid == RTW89_MAX_MAC_ID_NUM)
			return -ENOSPC;

		acquire_macid = true;
	}

	rtw89_init_sta(rtwdev, rtwvif, rtwsta, macid);

	for (i = 0; i < ARRAY_SIZE(sta->txq); i++)
		rtw89_core_txq_init(rtwdev, sta->txq[i]);

	skb_queue_head_init(&rtwsta->roc_queue);
	bitmap_zero(rtwsta->pairwise_sec_cam_map, RTW89_MAX_SEC_CAM_NUM);

	rtwsta_link = rtw89_sta_set_link(rtwsta, sta->deflink.link_id);
	if (!rtwsta_link) {
		ret = -EINVAL;
		goto err;
	}

	rtwvif_link = rtwsta_link->rtwvif_link;

	ret = rtw89_core_sta_link_add(rtwdev, rtwvif_link, rtwsta_link);
	if (ret)
		goto unset_link;

	if (vif->type == NL80211_IFTYPE_AP || sta->tdls)
		rtw89_queue_chanctx_change(rtwdev, RTW89_CHANCTX_REMOTE_STA_CHANGE);

	return 0;

unset_link:
	rtw89_sta_unset_link(rtwsta, sta->deflink.link_id);
err:
	if (acquire_macid)
		rtw89_release_mac_id(rtwdev, macid);

	return ret;
}

static int __rtw89_ops_sta_assoc(struct rtw89_dev *rtwdev,
				 struct ieee80211_vif *vif,
				 struct ieee80211_sta *sta,
				 bool station_mode)
{
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct rtw89_sta *rtwsta = sta_to_rtwsta(sta);
	struct rtw89_vif_link *rtwvif_link;
	struct rtw89_sta_link *rtwsta_link;
	unsigned int link_id;
	int ret;

	rtw89_sta_for_each_link(rtwsta, rtwsta_link, link_id) {
		rtwvif_link = rtwsta_link->rtwvif_link;

		if (station_mode)
			rtw89_vif_type_mapping(rtwvif_link, true);

		ret = rtw89_core_sta_link_assoc(rtwdev, rtwvif_link, rtwsta_link);
		if (ret)
			return ret;
	}

	rtwdev->total_sta_assoc++;
	if (sta->tdls)
		rtwvif->tdls_peer++;

	return 0;
}

static int __rtw89_ops_sta_disassoc(struct rtw89_dev *rtwdev,
				    struct ieee80211_vif *vif,
				    struct ieee80211_sta *sta)
{
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct rtw89_sta *rtwsta = sta_to_rtwsta(sta);
	struct rtw89_vif_link *rtwvif_link;
	struct rtw89_sta_link *rtwsta_link;
	unsigned int link_id;
	int ret;

	rtw89_sta_for_each_link(rtwsta, rtwsta_link, link_id) {
		rtwvif_link = rtwsta_link->rtwvif_link;
		ret = rtw89_core_sta_link_disassoc(rtwdev, rtwvif_link, rtwsta_link);
		if (ret)
			return ret;
	}

	rtwsta->disassoc = true;

	rtwdev->total_sta_assoc--;
	if (sta->tdls)
		rtwvif->tdls_peer--;

	return 0;
}

static int __rtw89_ops_sta_disconnect(struct rtw89_dev *rtwdev,
				      struct ieee80211_vif *vif,
				      struct ieee80211_sta *sta)
{
	struct rtw89_sta *rtwsta = sta_to_rtwsta(sta);
	struct rtw89_vif_link *rtwvif_link;
	struct rtw89_sta_link *rtwsta_link;
	unsigned int link_id;
	int ret;

	rtw89_core_free_sta_pending_ba(rtwdev, sta);
	rtw89_core_free_sta_pending_forbid_ba(rtwdev, sta);
	rtw89_core_free_sta_pending_roc_tx(rtwdev, sta);

	rtw89_sta_for_each_link(rtwsta, rtwsta_link, link_id) {
		rtwvif_link = rtwsta_link->rtwvif_link;
		ret = rtw89_core_sta_link_disconnect(rtwdev, rtwvif_link, rtwsta_link);
		if (ret)
			return ret;
	}

	return 0;
}

static int __rtw89_ops_sta_remove(struct rtw89_dev *rtwdev,
				  struct ieee80211_vif *vif,
				  struct ieee80211_sta *sta)
{
	struct rtw89_sta *rtwsta = sta_to_rtwsta(sta);
	u8 macid = rtw89_sta_get_main_macid(rtwsta);
	struct rtw89_vif_link *rtwvif_link;
	struct rtw89_sta_link *rtwsta_link;
	unsigned int link_id;
	int ret;

	rtw89_sta_for_each_link(rtwsta, rtwsta_link, link_id) {
		rtwvif_link = rtwsta_link->rtwvif_link;
		ret = rtw89_core_sta_link_remove(rtwdev, rtwvif_link, rtwsta_link);
		if (ret)
			return ret;

		rtw89_sta_unset_link(rtwsta, link_id);
	}

	if (vif->type == NL80211_IFTYPE_AP || sta->tdls) {
		rtw89_release_mac_id(rtwdev, macid);
		rtw89_queue_chanctx_change(rtwdev, RTW89_CHANCTX_REMOTE_STA_CHANGE);
	}

	return 0;
}

static void rtw89_station_mode_sta_assoc(struct rtw89_dev *rtwdev,
					 struct ieee80211_vif *vif)
{
	struct ieee80211_sta *sta;

	if (vif->type != NL80211_IFTYPE_STATION)
		return;

	sta = ieee80211_find_sta(vif, vif->cfg.ap_addr);
	if (!sta) {
		rtw89_err(rtwdev, "can't find sta to set sta_assoc state\n");
		return;
	}

	__rtw89_ops_sta_assoc(rtwdev, vif, sta, true);
}

static void __rtw89_ops_bss_link_assoc(struct rtw89_dev *rtwdev,
				       struct rtw89_vif_link *rtwvif_link)
{
	rtw89_phy_set_bss_color(rtwdev, rtwvif_link);
	rtw89_chip_cfg_txpwr_ul_tb_offset(rtwdev, rtwvif_link);
	rtw89_mac_port_update(rtwdev, rtwvif_link);
	rtw89_mac_set_he_obss_narrow_bw_ru(rtwdev, rtwvif_link);
}

static void __rtw89_ops_bss_assoc(struct rtw89_dev *rtwdev,
				  struct ieee80211_vif *vif)
{
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct rtw89_vif_link *rtwvif_link;
	unsigned int link_id;

	rtw89_vif_for_each_link(rtwvif, rtwvif_link, link_id)
		__rtw89_ops_bss_link_assoc(rtwdev, rtwvif_link);
}

static void rtw89_ops_vif_cfg_changed(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif, u64 changed)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);

	mutex_lock(&rtwdev->mutex);
	rtw89_leave_ps_mode(rtwdev);

	if (changed & BSS_CHANGED_ASSOC) {
		if (vif->cfg.assoc) {
			rtw89_station_mode_sta_assoc(rtwdev, vif);
			__rtw89_ops_bss_assoc(rtwdev, vif);

			rtw89_queue_chanctx_work(rtwdev);
		} else {
			/* Abort ongoing scan if cancel_scan isn't issued
			 * when disconnected by peer
			 */
			if (rtwdev->scanning)
				rtw89_hw_scan_abort(rtwdev, rtwdev->scan_info.scanning_vif);
		}
	}

	if (changed & BSS_CHANGED_PS)
		rtw89_recalc_lps(rtwdev);

	if (changed & BSS_CHANGED_ARP_FILTER)
		rtwvif->ip_addr = vif->cfg.arp_addr_list[0];

	mutex_unlock(&rtwdev->mutex);
}

static void rtw89_ops_link_info_changed(struct ieee80211_hw *hw,
					struct ieee80211_vif *vif,
					struct ieee80211_bss_conf *conf,
					u64 changed)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct rtw89_vif_link *rtwvif_link;

	mutex_lock(&rtwdev->mutex);
	rtw89_leave_ps_mode(rtwdev);

	rtwvif_link = rtwvif->links[conf->link_id];
	if (unlikely(!rtwvif_link)) {
		rtw89_err(rtwdev,
			  "%s: rtwvif link (link_id %u) is not active\n",
			  __func__, conf->link_id);
		goto out;
	}

	if (changed & BSS_CHANGED_BSSID) {
		ether_addr_copy(rtwvif_link->bssid, conf->bssid);
		rtw89_cam_bssid_changed(rtwdev, rtwvif_link);
		rtw89_fw_h2c_cam(rtwdev, rtwvif_link, NULL, NULL);
		WRITE_ONCE(rtwvif_link->sync_bcn_tsf, 0);
	}

	if (changed & BSS_CHANGED_BEACON)
		rtw89_chip_h2c_update_beacon(rtwdev, rtwvif_link);

	if (changed & BSS_CHANGED_ERP_SLOT)
		rtw89_conf_tx(rtwdev, rtwvif_link);

	if (changed & BSS_CHANGED_HE_BSS_COLOR)
		rtw89_phy_set_bss_color(rtwdev, rtwvif_link);

	if (changed & BSS_CHANGED_MU_GROUPS)
		rtw89_mac_bf_set_gid_table(rtwdev, vif, conf);

	if (changed & BSS_CHANGED_P2P_PS)
		rtw89_core_update_p2p_ps(rtwdev, rtwvif_link, conf);

	if (changed & BSS_CHANGED_CQM)
		rtw89_fw_h2c_set_bcn_fltr_cfg(rtwdev, rtwvif_link, true);

	if (changed & BSS_CHANGED_TPE)
		rtw89_reg_6ghz_recalc(rtwdev, rtwvif_link, true);

out:
	mutex_unlock(&rtwdev->mutex);
}

static int rtw89_ops_start_ap(struct ieee80211_hw *hw,
			      struct ieee80211_vif *vif,
			      struct ieee80211_bss_conf *link_conf)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct rtw89_vif_link *rtwvif_link;
	const struct rtw89_chan *chan;
	int ret = 0;

	mutex_lock(&rtwdev->mutex);

	rtwvif_link = rtwvif->links[link_conf->link_id];
	if (unlikely(!rtwvif_link)) {
		rtw89_err(rtwdev,
			  "%s: rtwvif link (link_id %u) is not active\n",
			  __func__, link_conf->link_id);
		ret = -ENOLINK;
		goto out;
	}

	chan = rtw89_chan_get(rtwdev, rtwvif_link->chanctx_idx);
	if (chan->band_type == RTW89_BAND_6G) {
		mutex_unlock(&rtwdev->mutex);
		return -EOPNOTSUPP;
	}

	if (rtwdev->scanning)
		rtw89_hw_scan_abort(rtwdev, rtwdev->scan_info.scanning_vif);

	ether_addr_copy(rtwvif_link->bssid, link_conf->bssid);
	rtw89_cam_bssid_changed(rtwdev, rtwvif_link);
	rtw89_mac_port_update(rtwdev, rtwvif_link);
	rtw89_chip_h2c_assoc_cmac_tbl(rtwdev, rtwvif_link, NULL);
	rtw89_fw_h2c_role_maintain(rtwdev, rtwvif_link, NULL, RTW89_ROLE_TYPE_CHANGE);
	rtw89_fw_h2c_join_info(rtwdev, rtwvif_link, NULL, true);
	rtw89_fw_h2c_cam(rtwdev, rtwvif_link, NULL, NULL);
	rtw89_chip_rfk_channel(rtwdev, rtwvif_link);

	if (RTW89_CHK_FW_FEATURE(NOTIFY_AP_INFO, &rtwdev->fw)) {
		ret = rtw89_fw_h2c_ap_info_refcount(rtwdev, true);
		if (ret)
			goto out;
	}

	rtw89_queue_chanctx_work(rtwdev);

out:
	mutex_unlock(&rtwdev->mutex);

	return ret;
}

static
void rtw89_ops_stop_ap(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
		       struct ieee80211_bss_conf *link_conf)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct rtw89_vif_link *rtwvif_link;

	mutex_lock(&rtwdev->mutex);

	rtwvif_link = rtwvif->links[link_conf->link_id];
	if (unlikely(!rtwvif_link)) {
		rtw89_err(rtwdev,
			  "%s: rtwvif link (link_id %u) is not active\n",
			  __func__, link_conf->link_id);
		goto out;
	}

	if (RTW89_CHK_FW_FEATURE(NOTIFY_AP_INFO, &rtwdev->fw))
		rtw89_fw_h2c_ap_info_refcount(rtwdev, false);

	rtw89_mac_stop_ap(rtwdev, rtwvif_link);
	rtw89_chip_h2c_assoc_cmac_tbl(rtwdev, rtwvif_link, NULL);
	rtw89_fw_h2c_join_info(rtwdev, rtwvif_link, NULL, true);

out:
	mutex_unlock(&rtwdev->mutex);
}

static int rtw89_ops_set_tim(struct ieee80211_hw *hw, struct ieee80211_sta *sta,
			     bool set)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_sta *rtwsta = sta_to_rtwsta(sta);
	struct rtw89_vif *rtwvif = rtwsta->rtwvif;
	struct rtw89_vif_link *rtwvif_link;
	unsigned int link_id;

	rtw89_vif_for_each_link(rtwvif, rtwvif_link, link_id)
		ieee80211_queue_work(rtwdev->hw, &rtwvif_link->update_beacon_work);

	return 0;
}

static int rtw89_ops_conf_tx(struct ieee80211_hw *hw,
			     struct ieee80211_vif *vif,
			     unsigned int link_id, u16 ac,
			     const struct ieee80211_tx_queue_params *params)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct rtw89_vif_link *rtwvif_link;
	int ret = 0;

	mutex_lock(&rtwdev->mutex);
	rtw89_leave_ps_mode(rtwdev);

	rtwvif_link = rtwvif->links[link_id];
	if (unlikely(!rtwvif_link)) {
		rtw89_err(rtwdev,
			  "%s: rtwvif link (link_id %u) is not active\n",
			  __func__, link_id);
		ret = -ENOLINK;
		goto out;
	}

	rtwvif_link->tx_params[ac] = *params;
	__rtw89_conf_tx(rtwdev, rtwvif_link, ac);

out:
	mutex_unlock(&rtwdev->mutex);

	return ret;
}

static int __rtw89_ops_sta_state(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif,
				 struct ieee80211_sta *sta,
				 enum ieee80211_sta_state old_state,
				 enum ieee80211_sta_state new_state)
{
	struct rtw89_dev *rtwdev = hw->priv;

	if (old_state == IEEE80211_STA_NOTEXIST &&
	    new_state == IEEE80211_STA_NONE)
		return __rtw89_ops_sta_add(rtwdev, vif, sta);

	if (old_state == IEEE80211_STA_AUTH &&
	    new_state == IEEE80211_STA_ASSOC) {
		if (vif->type == NL80211_IFTYPE_STATION && !sta->tdls)
			return 0; /* defer to bss_info_changed to have vif info */
		return __rtw89_ops_sta_assoc(rtwdev, vif, sta, false);
	}

	if (old_state == IEEE80211_STA_ASSOC &&
	    new_state == IEEE80211_STA_AUTH)
		return __rtw89_ops_sta_disassoc(rtwdev, vif, sta);

	if (old_state == IEEE80211_STA_AUTH &&
	    new_state == IEEE80211_STA_NONE)
		return __rtw89_ops_sta_disconnect(rtwdev, vif, sta);

	if (old_state == IEEE80211_STA_NONE &&
	    new_state == IEEE80211_STA_NOTEXIST)
		return __rtw89_ops_sta_remove(rtwdev, vif, sta);

	return 0;
}

static int rtw89_ops_sta_state(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif,
			       struct ieee80211_sta *sta,
			       enum ieee80211_sta_state old_state,
			       enum ieee80211_sta_state new_state)
{
	struct rtw89_dev *rtwdev = hw->priv;
	int ret;

	mutex_lock(&rtwdev->mutex);
	rtw89_leave_ps_mode(rtwdev);
	ret = __rtw89_ops_sta_state(hw, vif, sta, old_state, new_state);
	mutex_unlock(&rtwdev->mutex);

	return ret;
}

static int rtw89_ops_set_key(struct ieee80211_hw *hw, enum set_key_cmd cmd,
			     struct ieee80211_vif *vif,
			     struct ieee80211_sta *sta,
			     struct ieee80211_key_conf *key)
{
	struct rtw89_dev *rtwdev = hw->priv;
	int ret = 0;

	mutex_lock(&rtwdev->mutex);
	rtw89_leave_ps_mode(rtwdev);

	switch (cmd) {
	case SET_KEY:
		rtw89_btc_ntfy_specific_packet(rtwdev, PACKET_EAPOL_END);
		ret = rtw89_cam_sec_key_add(rtwdev, vif, sta, key);
		if (ret && ret != -EOPNOTSUPP) {
			rtw89_err(rtwdev, "failed to add key to sec cam\n");
			goto out;
		}
		break;
	case DISABLE_KEY:
		rtw89_hci_flush_queues(rtwdev, BIT(rtwdev->hw->queues) - 1,
				       false);
		rtw89_mac_flush_txq(rtwdev, BIT(rtwdev->hw->queues) - 1, false);
		ret = rtw89_cam_sec_key_del(rtwdev, vif, sta, key, true);
		if (ret) {
			rtw89_err(rtwdev, "failed to remove key from sec cam\n");
			goto out;
		}
		break;
	}

out:
	mutex_unlock(&rtwdev->mutex);

	return ret;
}

static int rtw89_ops_ampdu_action(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif,
				  struct ieee80211_ampdu_params *params)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct ieee80211_sta *sta = params->sta;
	struct rtw89_sta *rtwsta = sta_to_rtwsta(sta);
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	u16 tid = params->tid;
	struct ieee80211_txq *txq = sta->txq[tid];
	struct rtw89_txq *rtwtxq = (struct rtw89_txq *)txq->drv_priv;

	switch (params->action) {
	case IEEE80211_AMPDU_TX_START:
		return IEEE80211_AMPDU_TX_START_IMMEDIATE;
	case IEEE80211_AMPDU_TX_STOP_CONT:
	case IEEE80211_AMPDU_TX_STOP_FLUSH:
	case IEEE80211_AMPDU_TX_STOP_FLUSH_CONT:
		mutex_lock(&rtwdev->mutex);
		clear_bit(RTW89_TXQ_F_AMPDU, &rtwtxq->flags);
		clear_bit(tid, rtwsta->ampdu_map);
		rtw89_chip_h2c_ampdu_cmac_tbl(rtwdev, rtwvif, rtwsta);
		mutex_unlock(&rtwdev->mutex);
		ieee80211_stop_tx_ba_cb_irqsafe(vif, sta->addr, tid);
		break;
	case IEEE80211_AMPDU_TX_OPERATIONAL:
		mutex_lock(&rtwdev->mutex);
		set_bit(RTW89_TXQ_F_AMPDU, &rtwtxq->flags);
		rtwsta->ampdu_params[tid].agg_num = params->buf_size;
		rtwsta->ampdu_params[tid].amsdu = params->amsdu;
		set_bit(tid, rtwsta->ampdu_map);
		rtw89_leave_ps_mode(rtwdev);
		rtw89_chip_h2c_ampdu_cmac_tbl(rtwdev, rtwvif, rtwsta);
		mutex_unlock(&rtwdev->mutex);
		break;
	case IEEE80211_AMPDU_RX_START:
		mutex_lock(&rtwdev->mutex);
		rtw89_chip_h2c_ba_cam(rtwdev, rtwsta, true, params);
		mutex_unlock(&rtwdev->mutex);
		break;
	case IEEE80211_AMPDU_RX_STOP:
		mutex_lock(&rtwdev->mutex);
		rtw89_chip_h2c_ba_cam(rtwdev, rtwsta, false, params);
		mutex_unlock(&rtwdev->mutex);
		break;
	default:
		WARN_ON(1);
		return -ENOTSUPP;
	}

	return 0;
}

static int rtw89_ops_set_rts_threshold(struct ieee80211_hw *hw, u32 value)
{
	struct rtw89_dev *rtwdev = hw->priv;

	mutex_lock(&rtwdev->mutex);
	rtw89_leave_ps_mode(rtwdev);
	if (test_bit(RTW89_FLAG_POWERON, rtwdev->flags))
		rtw89_mac_update_rts_threshold(rtwdev);
	mutex_unlock(&rtwdev->mutex);

	return 0;
}

static void rtw89_ops_sta_statistics(struct ieee80211_hw *hw,
				     struct ieee80211_vif *vif,
				     struct ieee80211_sta *sta,
				     struct station_info *sinfo)
{
	struct rtw89_sta *rtwsta = sta_to_rtwsta(sta);
	struct rtw89_sta_link *rtwsta_link;

	rtwsta_link = rtw89_sta_get_link_inst(rtwsta, 0);
	if (unlikely(!rtwsta_link))
		return;

	sinfo->txrate = rtwsta_link->ra_report.txrate;
	sinfo->filled |= BIT_ULL(NL80211_STA_INFO_TX_BITRATE);
}

static
void __rtw89_drop_packets(struct rtw89_dev *rtwdev, struct ieee80211_vif *vif)
{
	struct rtw89_vif *rtwvif;

	if (vif) {
		rtwvif = vif_to_rtwvif(vif);
		rtw89_mac_pkt_drop_vif(rtwdev, rtwvif);
	} else {
		rtw89_for_each_rtwvif(rtwdev, rtwvif)
			rtw89_mac_pkt_drop_vif(rtwdev, rtwvif);
	}
}

static void rtw89_ops_flush(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			    u32 queues, bool drop)
{
	struct rtw89_dev *rtwdev = hw->priv;

	mutex_lock(&rtwdev->mutex);
	rtw89_leave_lps(rtwdev);
	rtw89_hci_flush_queues(rtwdev, queues, drop);

	if (drop && !RTW89_CHK_FW_FEATURE(NO_PACKET_DROP, &rtwdev->fw))
		__rtw89_drop_packets(rtwdev, vif);
	else
		rtw89_mac_flush_txq(rtwdev, queues, drop);

	mutex_unlock(&rtwdev->mutex);
}

struct rtw89_iter_bitrate_mask_data {
	struct rtw89_dev *rtwdev;
	struct ieee80211_vif *vif;
	const struct cfg80211_bitrate_mask *mask;
};

static void rtw89_ra_mask_info_update_iter(void *data, struct ieee80211_sta *sta)
{
	struct rtw89_iter_bitrate_mask_data *br_data = data;
	struct rtw89_sta *rtwsta = sta_to_rtwsta(sta);
	struct rtw89_vif *rtwvif = rtwsta->rtwvif;
	struct ieee80211_vif *vif = rtwvif_to_vif(rtwvif);
	struct rtw89_sta_link *rtwsta_link;
	unsigned int link_id;

	if (vif != br_data->vif || vif->p2p)
		return;

	rtw89_sta_for_each_link(rtwsta, rtwsta_link, link_id) {
		rtwsta_link->use_cfg_mask = true;
		rtwsta_link->mask = *br_data->mask;
	}

	rtw89_phy_ra_update_sta(br_data->rtwdev, sta, IEEE80211_RC_SUPP_RATES_CHANGED);
}

static void rtw89_ra_mask_info_update(struct rtw89_dev *rtwdev,
				      struct ieee80211_vif *vif,
				      const struct cfg80211_bitrate_mask *mask)
{
	struct rtw89_iter_bitrate_mask_data br_data = { .rtwdev = rtwdev,
							.vif = vif,
							.mask = mask};

	ieee80211_iterate_stations_atomic(rtwdev->hw, rtw89_ra_mask_info_update_iter,
					  &br_data);
}

static int rtw89_ops_set_bitrate_mask(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif,
				      const struct cfg80211_bitrate_mask *mask)
{
	struct rtw89_dev *rtwdev = hw->priv;

	mutex_lock(&rtwdev->mutex);
	rtw89_phy_rate_pattern_vif(rtwdev, vif, mask);
	rtw89_ra_mask_info_update(rtwdev, vif, mask);
	mutex_unlock(&rtwdev->mutex);

	return 0;
}

static
int rtw89_ops_set_antenna(struct ieee80211_hw *hw, u32 tx_ant, u32 rx_ant)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_hal *hal = &rtwdev->hal;

	if (hal->ant_diversity) {
		if (tx_ant != rx_ant || hweight32(tx_ant) != 1)
			return -EINVAL;
	} else if (rx_ant != hw->wiphy->available_antennas_rx && rx_ant != hal->antenna_rx) {
		return -EINVAL;
	}

	mutex_lock(&rtwdev->mutex);
	hal->antenna_tx = tx_ant;
	hal->antenna_rx = rx_ant;
	hal->tx_path_diversity = false;
	hal->ant_diversity_fixed = true;
	mutex_unlock(&rtwdev->mutex);

	return 0;
}

static
int rtw89_ops_get_antenna(struct ieee80211_hw *hw,  u32 *tx_ant, u32 *rx_ant)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_hal *hal = &rtwdev->hal;

	*tx_ant = hal->antenna_tx;
	*rx_ant = hal->antenna_rx;

	return 0;
}

static void rtw89_ops_sw_scan_start(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    const u8 *mac_addr)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct rtw89_vif_link *rtwvif_link;

	mutex_lock(&rtwdev->mutex);

	rtwvif_link = rtw89_vif_get_link_inst(rtwvif, 0);
	if (unlikely(!rtwvif_link)) {
		rtw89_err(rtwdev, "sw scan start: find no link on HW-0\n");
		goto out;
	}

	rtw89_core_scan_start(rtwdev, rtwvif_link, mac_addr, false);

out:
	mutex_unlock(&rtwdev->mutex);
}

static void rtw89_ops_sw_scan_complete(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct rtw89_vif_link *rtwvif_link;

	mutex_lock(&rtwdev->mutex);

	rtwvif_link = rtw89_vif_get_link_inst(rtwvif, 0);
	if (unlikely(!rtwvif_link)) {
		rtw89_err(rtwdev, "sw scan complete: find no link on HW-0\n");
		goto out;
	}

	rtw89_core_scan_complete(rtwdev, rtwvif_link, false);

out:
	mutex_unlock(&rtwdev->mutex);
}

static void rtw89_ops_reconfig_complete(struct ieee80211_hw *hw,
					enum ieee80211_reconfig_type reconfig_type)
{
	struct rtw89_dev *rtwdev = hw->priv;

	if (reconfig_type == IEEE80211_RECONFIG_TYPE_RESTART)
		rtw89_ser_recfg_done(rtwdev);
}

static int rtw89_ops_hw_scan(struct ieee80211_hw *hw, struct ieee80211_vif *vif,
			     struct ieee80211_scan_request *req)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct rtw89_vif_link *rtwvif_link;
	int ret;

	if (!RTW89_CHK_FW_FEATURE(SCAN_OFFLOAD, &rtwdev->fw))
		return 1;

	mutex_lock(&rtwdev->mutex);

	if (rtwdev->scanning || rtwvif->offchan) {
		ret = -EBUSY;
		goto out;
	}

	rtwvif_link = rtw89_vif_get_link_inst(rtwvif, 0);
	if (unlikely(!rtwvif_link)) {
		rtw89_err(rtwdev, "hw scan: find no link on HW-0\n");
		ret = -ENOLINK;
		goto out;
	}

	rtw89_hw_scan_start(rtwdev, rtwvif_link, req);
	ret = rtw89_hw_scan_offload(rtwdev, rtwvif_link, true);
	if (ret) {
		rtw89_hw_scan_abort(rtwdev, rtwvif_link);
		rtw89_err(rtwdev, "HW scan failed with status: %d\n", ret);
	}

out:
	mutex_unlock(&rtwdev->mutex);

	return ret;
}

static void rtw89_ops_cancel_hw_scan(struct ieee80211_hw *hw,
				     struct ieee80211_vif *vif)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct rtw89_vif_link *rtwvif_link;

	if (!RTW89_CHK_FW_FEATURE(SCAN_OFFLOAD, &rtwdev->fw))
		return;

	mutex_lock(&rtwdev->mutex);

	if (!rtwdev->scanning)
		goto out;

	rtwvif_link = rtw89_vif_get_link_inst(rtwvif, 0);
	if (unlikely(!rtwvif_link)) {
		rtw89_err(rtwdev, "cancel hw scan: find no link on HW-0\n");
		goto out;
	}

	rtw89_hw_scan_abort(rtwdev, rtwvif_link);

out:
	mutex_unlock(&rtwdev->mutex);
}

static void rtw89_ops_sta_rc_update(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    struct ieee80211_link_sta *link_sta,
				    u32 changed)
{
	struct rtw89_sta *rtwsta = sta_to_rtwsta(link_sta->sta);
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_sta_link *rtwsta_link;

	rtwsta_link = rtwsta->links[link_sta->link_id];
	if (unlikely(!rtwsta_link))
		return;

	rtw89_phy_ra_update_sta_link(rtwdev, rtwsta_link, changed);
}

static int rtw89_ops_add_chanctx(struct ieee80211_hw *hw,
				 struct ieee80211_chanctx_conf *ctx)
{
	struct rtw89_dev *rtwdev = hw->priv;
	int ret;

	mutex_lock(&rtwdev->mutex);
	ret = rtw89_chanctx_ops_add(rtwdev, ctx);
	mutex_unlock(&rtwdev->mutex);

	return ret;
}

static void rtw89_ops_remove_chanctx(struct ieee80211_hw *hw,
				     struct ieee80211_chanctx_conf *ctx)
{
	struct rtw89_dev *rtwdev = hw->priv;

	mutex_lock(&rtwdev->mutex);
	rtw89_chanctx_ops_remove(rtwdev, ctx);
	mutex_unlock(&rtwdev->mutex);
}

static void rtw89_ops_change_chanctx(struct ieee80211_hw *hw,
				     struct ieee80211_chanctx_conf *ctx,
				     u32 changed)
{
	struct rtw89_dev *rtwdev = hw->priv;

	mutex_lock(&rtwdev->mutex);
	rtw89_chanctx_ops_change(rtwdev, ctx, changed);
	mutex_unlock(&rtwdev->mutex);
}

static int rtw89_ops_assign_vif_chanctx(struct ieee80211_hw *hw,
					struct ieee80211_vif *vif,
					struct ieee80211_bss_conf *link_conf,
					struct ieee80211_chanctx_conf *ctx)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct rtw89_vif_link *rtwvif_link;
	int ret;

	mutex_lock(&rtwdev->mutex);

	rtwvif_link = rtwvif->links[link_conf->link_id];
	if (unlikely(!rtwvif_link)) {
		rtw89_err(rtwdev,
			  "%s: rtwvif link (link_id %u) is not active\n",
			  __func__, link_conf->link_id);
		ret = -ENOLINK;
		goto out;
	}

	ret = rtw89_chanctx_ops_assign_vif(rtwdev, rtwvif_link, ctx);

out:
	mutex_unlock(&rtwdev->mutex);

	return ret;
}

static void rtw89_ops_unassign_vif_chanctx(struct ieee80211_hw *hw,
					   struct ieee80211_vif *vif,
					   struct ieee80211_bss_conf *link_conf,
					   struct ieee80211_chanctx_conf *ctx)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	struct rtw89_vif_link *rtwvif_link;

	mutex_lock(&rtwdev->mutex);

	rtwvif_link = rtwvif->links[link_conf->link_id];
	if (unlikely(!rtwvif_link)) {
		mutex_unlock(&rtwdev->mutex);
		rtw89_err(rtwdev,
			  "%s: rtwvif link (link_id %u) is not active\n",
			  __func__, link_conf->link_id);
		return;
	}

	rtw89_chanctx_ops_unassign_vif(rtwdev, rtwvif_link, ctx);
	mutex_unlock(&rtwdev->mutex);
}

static int rtw89_ops_remain_on_channel(struct ieee80211_hw *hw,
				       struct ieee80211_vif *vif,
				       struct ieee80211_channel *chan,
				       int duration,
				       enum ieee80211_roc_type type)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif_safe(vif);
	struct rtw89_roc *roc = &rtwvif->roc;

	if (!rtwvif)
		return -EINVAL;

	mutex_lock(&rtwdev->mutex);

	if (roc->state != RTW89_ROC_IDLE) {
		mutex_unlock(&rtwdev->mutex);
		return -EBUSY;
	}

	if (rtwdev->scanning)
		rtw89_hw_scan_abort(rtwdev, rtwdev->scan_info.scanning_vif);

	if (type == IEEE80211_ROC_TYPE_MGMT_TX)
		roc->state = RTW89_ROC_MGMT;
	else
		roc->state = RTW89_ROC_NORMAL;

	roc->duration = duration;
	roc->chan = *chan;
	roc->type = type;

	rtw89_roc_start(rtwdev, rtwvif);

	mutex_unlock(&rtwdev->mutex);

	return 0;
}

static int rtw89_ops_cancel_remain_on_channel(struct ieee80211_hw *hw,
					      struct ieee80211_vif *vif)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif_safe(vif);

	if (!rtwvif)
		return -EINVAL;

	cancel_delayed_work_sync(&rtwvif->roc.roc_work);

	mutex_lock(&rtwdev->mutex);
	rtw89_roc_end(rtwdev, rtwvif);
	mutex_unlock(&rtwdev->mutex);

	return 0;
}

static void rtw89_set_tid_config_iter(void *data, struct ieee80211_sta *sta)
{
	struct cfg80211_tid_config *tid_config = data;
	struct rtw89_sta *rtwsta = sta_to_rtwsta(sta);
	struct rtw89_dev *rtwdev = rtwsta->rtwdev;

	rtw89_core_set_tid_config(rtwdev, sta, tid_config);
}

static int rtw89_ops_set_tid_config(struct ieee80211_hw *hw,
				    struct ieee80211_vif *vif,
				    struct ieee80211_sta *sta,
				    struct cfg80211_tid_config *tid_config)
{
	struct rtw89_dev *rtwdev = hw->priv;

	mutex_lock(&rtwdev->mutex);
	if (sta)
		rtw89_core_set_tid_config(rtwdev, sta, tid_config);
	else
		ieee80211_iterate_stations_atomic(rtwdev->hw,
						  rtw89_set_tid_config_iter,
						  tid_config);
	mutex_unlock(&rtwdev->mutex);

	return 0;
}

static bool rtw89_can_work_on_links(struct rtw89_dev *rtwdev,
				    struct ieee80211_vif *vif, u16 links)
{
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	u8 w = hweight16(links);

	if (vif->type != NL80211_IFTYPE_STATION &&
	    w > RTW89_MLD_NON_STA_LINK_NUM)
		return false;

	return w <= rtwvif->links_inst_valid_num;
}

static bool rtw89_ops_can_activate_links(struct ieee80211_hw *hw,
					 struct ieee80211_vif *vif,
					 u16 active_links)
{
	struct rtw89_dev *rtwdev = hw->priv;

	guard(mutex)(&rtwdev->mutex);

	return rtw89_can_work_on_links(rtwdev, vif, active_links);
}

static void __rtw89_ops_clr_vif_links(struct rtw89_dev *rtwdev,
				      struct rtw89_vif *rtwvif,
				      unsigned long clr_links)
{
	struct rtw89_vif_link *rtwvif_link;
	unsigned int link_id;

	for_each_set_bit(link_id, &clr_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		rtwvif_link = rtwvif->links[link_id];
		if (unlikely(!rtwvif_link))
			continue;

		__rtw89_ops_remove_iface_link(rtwdev, rtwvif_link);

		rtw89_vif_unset_link(rtwvif, link_id);
	}
}

static int __rtw89_ops_set_vif_links(struct rtw89_dev *rtwdev,
				     struct rtw89_vif *rtwvif,
				     unsigned long set_links)
{
	struct rtw89_vif_link *rtwvif_link;
	unsigned int link_id;
	int ret;

	for_each_set_bit(link_id, &set_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		rtwvif_link = rtw89_vif_set_link(rtwvif, link_id);
		if (!rtwvif_link)
			return -EINVAL;

		ret = __rtw89_ops_add_iface_link(rtwdev, rtwvif_link);
		if (ret) {
			rtw89_err(rtwdev, "%s: failed to add iface (link id %u)\n",
				  __func__, link_id);
			return ret;
		}
	}

	return 0;
}

static
int rtw89_ops_change_vif_links(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif,
			       u16 old_links, u16 new_links,
			       struct ieee80211_bss_conf *old[IEEE80211_MLD_MAX_NUM_LINKS])
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_vif *rtwvif = vif_to_rtwvif(vif);
	unsigned long clr_links = old_links & ~new_links;
	unsigned long set_links = new_links & ~old_links;
	bool removing_links = !old_links || clr_links;
	struct rtw89_link_conf_container *snap;
	int ret = 0;
	int i;

	guard(mutex)(&rtwdev->mutex);

	rtw89_debug(rtwdev, RTW89_DBG_STATE,
		    "%s: old_links (0x%08x) -> new_links (0x%08x)\n",
		    __func__, old_links, new_links);

	if (!rtw89_can_work_on_links(rtwdev, vif, new_links))
		return -EOPNOTSUPP;

	if (removing_links) {
		snap = kzalloc(sizeof(*snap), GFP_KERNEL);
		if (!snap)
			return -ENOMEM;

		for (i = 0; i < ARRAY_SIZE(snap->link_conf); i++)
			snap->link_conf[i] = old[i];

		rcu_assign_pointer(rtwvif->snap_link_confs, snap);
	}

	/* might depend on @snap; don't change order */
	rtw89_leave_ips_by_hwflags(rtwdev);

	if (rtwdev->scanning)
		rtw89_hw_scan_abort(rtwdev, rtwdev->scan_info.scanning_vif);

	if (!old_links)
		__rtw89_ops_clr_vif_links(rtwdev, rtwvif,
					  BIT(RTW89_VIF_IDLE_LINK_ID));
	else if (clr_links)
		__rtw89_ops_clr_vif_links(rtwdev, rtwvif, clr_links);

	if (removing_links) {
		/* @snap is required if and only if during removing links.
		 * However, it's done here. So, cleanup @snap immediately.
		 */
		rcu_assign_pointer(rtwvif->snap_link_confs, NULL);

		/* The pointers in @old will free after this function return,
		 * so synchronously wait for all readers of snap to be done.
		 */
		synchronize_rcu();
		kfree(snap);
	}

	if (set_links) {
		ret = __rtw89_ops_set_vif_links(rtwdev, rtwvif, set_links);
		if (ret)
			__rtw89_ops_clr_vif_links(rtwdev, rtwvif, set_links);
	} else if (!new_links) {
		ret = __rtw89_ops_set_vif_links(rtwdev, rtwvif,
						BIT(RTW89_VIF_IDLE_LINK_ID));
		if (ret)
			__rtw89_ops_clr_vif_links(rtwdev, rtwvif,
						  BIT(RTW89_VIF_IDLE_LINK_ID));
	}

	rtw89_enter_ips_by_hwflags(rtwdev);
	return ret;
}

static void __rtw89_ops_clr_sta_links(struct rtw89_dev *rtwdev,
				      struct rtw89_sta *rtwsta,
				      unsigned long clr_links)
{
	struct rtw89_vif_link *rtwvif_link;
	struct rtw89_sta_link *rtwsta_link;
	unsigned int link_id;

	for_each_set_bit(link_id, &clr_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		rtwsta_link = rtwsta->links[link_id];
		if (unlikely(!rtwsta_link))
			continue;

		rtwvif_link = rtwsta_link->rtwvif_link;

		rtw89_core_sta_link_disassoc(rtwdev, rtwvif_link, rtwsta_link);
		rtw89_core_sta_link_disconnect(rtwdev, rtwvif_link, rtwsta_link);
		rtw89_core_sta_link_remove(rtwdev, rtwvif_link, rtwsta_link);

		rtw89_sta_unset_link(rtwsta, link_id);
	}
}

static int __rtw89_ops_set_sta_links(struct rtw89_dev *rtwdev,
				     struct rtw89_sta *rtwsta,
				     unsigned long set_links)
{
	struct rtw89_vif_link *rtwvif_link;
	struct rtw89_sta_link *rtwsta_link;
	unsigned int link_id;
	u8 sec_cam_idx;
	int ret;

	for_each_set_bit(link_id, &set_links, IEEE80211_MLD_MAX_NUM_LINKS) {
		rtwsta_link = rtw89_sta_set_link(rtwsta, link_id);
		if (!rtwsta_link)
			return -EINVAL;

		rtwvif_link = rtwsta_link->rtwvif_link;

		ret = rtw89_core_sta_link_add(rtwdev, rtwvif_link, rtwsta_link);
		if (ret) {
			rtw89_err(rtwdev, "%s: failed to add sta (link id %u)\n",
				  __func__, link_id);
			return ret;
		}

		rtw89_vif_type_mapping(rtwvif_link, true);

		ret = rtw89_core_sta_link_assoc(rtwdev, rtwvif_link, rtwsta_link);
		if (ret) {
			rtw89_err(rtwdev, "%s: failed to assoc sta (link id %u)\n",
				  __func__, link_id);
			return ret;
		}

		__rtw89_ops_bss_link_assoc(rtwdev, rtwvif_link);

		for_each_set_bit(sec_cam_idx, rtwsta->pairwise_sec_cam_map,
				 RTW89_MAX_SEC_CAM_NUM) {
			ret = rtw89_cam_attach_link_sec_cam(rtwdev,
							    rtwvif_link,
							    rtwsta_link,
							    sec_cam_idx);
			if (ret) {
				rtw89_err(rtwdev,
					  "%s: failed to apply pairwise key (link id %u)\n",
					  __func__, link_id);
				return ret;
			}
		}
	}

	return 0;
}

static
int rtw89_ops_change_sta_links(struct ieee80211_hw *hw,
			       struct ieee80211_vif *vif,
			       struct ieee80211_sta *sta,
			       u16 old_links, u16 new_links)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_sta *rtwsta = sta_to_rtwsta(sta);
	unsigned long clr_links = old_links & ~new_links;
	unsigned long set_links = new_links & ~old_links;
	int ret = 0;

	guard(mutex)(&rtwdev->mutex);

	rtw89_debug(rtwdev, RTW89_DBG_STATE,
		    "%s: old_links (0x%08x) -> new_links (0x%08x)\n",
		    __func__, old_links, new_links);

	if (!rtw89_can_work_on_links(rtwdev, vif, new_links))
		return -EOPNOTSUPP;

	rtw89_leave_ps_mode(rtwdev);

	if (clr_links)
		__rtw89_ops_clr_sta_links(rtwdev, rtwsta, clr_links);

	if (set_links) {
		ret = __rtw89_ops_set_sta_links(rtwdev, rtwsta, set_links);
		if (ret)
			__rtw89_ops_clr_sta_links(rtwdev, rtwsta, set_links);
	}

	return ret;
}

#ifdef CONFIG_PM
static int rtw89_ops_suspend(struct ieee80211_hw *hw,
			     struct cfg80211_wowlan *wowlan)
{
	struct rtw89_dev *rtwdev = hw->priv;
	int ret;

	set_bit(RTW89_FLAG_FORBIDDEN_TRACK_WROK, rtwdev->flags);
	cancel_delayed_work_sync(&rtwdev->track_work);

	mutex_lock(&rtwdev->mutex);
	ret = rtw89_wow_suspend(rtwdev, wowlan);
	mutex_unlock(&rtwdev->mutex);

	if (ret) {
		rtw89_warn(rtwdev, "failed to suspend for wow %d\n", ret);
		clear_bit(RTW89_FLAG_FORBIDDEN_TRACK_WROK, rtwdev->flags);
		return 1;
	}

	return 0;
}

static int rtw89_ops_resume(struct ieee80211_hw *hw)
{
	struct rtw89_dev *rtwdev = hw->priv;
	int ret;

	mutex_lock(&rtwdev->mutex);
	ret = rtw89_wow_resume(rtwdev);
	if (ret)
		rtw89_warn(rtwdev, "failed to resume for wow %d\n", ret);
	mutex_unlock(&rtwdev->mutex);

	clear_bit(RTW89_FLAG_FORBIDDEN_TRACK_WROK, rtwdev->flags);
	ieee80211_queue_delayed_work(rtwdev->hw, &rtwdev->track_work,
				     RTW89_TRACK_WORK_PERIOD);

	return ret ? 1 : 0;
}

static void rtw89_ops_set_wakeup(struct ieee80211_hw *hw, bool enabled)
{
	struct rtw89_dev *rtwdev = hw->priv;

	device_set_wakeup_enable(rtwdev->dev, enabled);
}

static void rtw89_set_rekey_data(struct ieee80211_hw *hw,
				 struct ieee80211_vif *vif,
				 struct cfg80211_gtk_rekey_data *data)
{
	struct rtw89_dev *rtwdev = hw->priv;
	struct rtw89_wow_param *rtw_wow = &rtwdev->wow;
	struct rtw89_wow_gtk_info *gtk_info = &rtw_wow->gtk_info;

	if (data->kek_len > sizeof(gtk_info->kek) ||
	    data->kck_len > sizeof(gtk_info->kck)) {
		rtw89_warn(rtwdev, "kek or kck length over fw limit\n");
		return;
	}

	mutex_lock(&rtwdev->mutex);

	memcpy(gtk_info->kek, data->kek, data->kek_len);
	memcpy(gtk_info->kck, data->kck, data->kck_len);

	mutex_unlock(&rtwdev->mutex);
}
#endif

static void rtw89_ops_rfkill_poll(struct ieee80211_hw *hw)
{
	struct rtw89_dev *rtwdev = hw->priv;

	mutex_lock(&rtwdev->mutex);

	/* wl_disable GPIO get floating when entering LPS */
	if (test_bit(RTW89_FLAG_RUNNING, rtwdev->flags))
		goto out;

	rtw89_core_rfkill_poll(rtwdev, false);

out:
	mutex_unlock(&rtwdev->mutex);
}

const struct ieee80211_ops rtw89_ops = {
	.tx			= rtw89_ops_tx,
	.wake_tx_queue		= rtw89_ops_wake_tx_queue,
	.start			= rtw89_ops_start,
	.stop			= rtw89_ops_stop,
	.config			= rtw89_ops_config,
	.add_interface		= rtw89_ops_add_interface,
	.change_interface       = rtw89_ops_change_interface,
	.remove_interface	= rtw89_ops_remove_interface,
	.configure_filter	= rtw89_ops_configure_filter,
	.vif_cfg_changed	= rtw89_ops_vif_cfg_changed,
	.link_info_changed	= rtw89_ops_link_info_changed,
	.start_ap		= rtw89_ops_start_ap,
	.stop_ap		= rtw89_ops_stop_ap,
	.set_tim		= rtw89_ops_set_tim,
	.conf_tx		= rtw89_ops_conf_tx,
	.sta_state		= rtw89_ops_sta_state,
	.set_key		= rtw89_ops_set_key,
	.ampdu_action		= rtw89_ops_ampdu_action,
	.set_rts_threshold	= rtw89_ops_set_rts_threshold,
	.sta_statistics		= rtw89_ops_sta_statistics,
	.flush			= rtw89_ops_flush,
	.set_bitrate_mask	= rtw89_ops_set_bitrate_mask,
	.set_antenna		= rtw89_ops_set_antenna,
	.get_antenna		= rtw89_ops_get_antenna,
	.sw_scan_start		= rtw89_ops_sw_scan_start,
	.sw_scan_complete	= rtw89_ops_sw_scan_complete,
	.reconfig_complete	= rtw89_ops_reconfig_complete,
	.hw_scan		= rtw89_ops_hw_scan,
	.cancel_hw_scan		= rtw89_ops_cancel_hw_scan,
	.add_chanctx		= rtw89_ops_add_chanctx,
	.remove_chanctx		= rtw89_ops_remove_chanctx,
	.change_chanctx		= rtw89_ops_change_chanctx,
	.assign_vif_chanctx	= rtw89_ops_assign_vif_chanctx,
	.unassign_vif_chanctx	= rtw89_ops_unassign_vif_chanctx,
	.remain_on_channel		= rtw89_ops_remain_on_channel,
	.cancel_remain_on_channel	= rtw89_ops_cancel_remain_on_channel,
	.set_sar_specs		= rtw89_ops_set_sar_specs,
	.link_sta_rc_update	= rtw89_ops_sta_rc_update,
	.set_tid_config		= rtw89_ops_set_tid_config,
	.can_activate_links	= rtw89_ops_can_activate_links,
	.change_vif_links	= rtw89_ops_change_vif_links,
	.change_sta_links	= rtw89_ops_change_sta_links,
#ifdef CONFIG_PM
	.suspend		= rtw89_ops_suspend,
	.resume			= rtw89_ops_resume,
	.set_wakeup		= rtw89_ops_set_wakeup,
	.set_rekey_data		= rtw89_set_rekey_data,
#endif
	.rfkill_poll		= rtw89_ops_rfkill_poll,
};
EXPORT_SYMBOL(rtw89_ops);
