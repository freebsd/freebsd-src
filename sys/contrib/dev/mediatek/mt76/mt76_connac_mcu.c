// SPDX-License-Identifier: ISC
/* Copyright (C) 2020 MediaTek Inc. */

#include <linux/firmware.h>
#include "mt76_connac2_mac.h"
#include "mt76_connac_mcu.h"

int mt76_connac_mcu_start_firmware(struct mt76_dev *dev, u32 addr, u32 option)
{
	struct {
		__le32 option;
		__le32 addr;
	} req = {
		.option = cpu_to_le32(option),
		.addr = cpu_to_le32(addr),
	};

	return mt76_mcu_send_msg(dev, MCU_CMD(FW_START_REQ), &req,
				 sizeof(req), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_start_firmware);

int mt76_connac_mcu_patch_sem_ctrl(struct mt76_dev *dev, bool get)
{
	u32 op = get ? PATCH_SEM_GET : PATCH_SEM_RELEASE;
	struct {
		__le32 op;
	} req = {
		.op = cpu_to_le32(op),
	};

	return mt76_mcu_send_msg(dev, MCU_CMD(PATCH_SEM_CONTROL),
				 &req, sizeof(req), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_patch_sem_ctrl);

int mt76_connac_mcu_start_patch(struct mt76_dev *dev)
{
	struct {
		u8 check_crc;
		u8 reserved[3];
	} req = {
		.check_crc = 0,
	};

	return mt76_mcu_send_msg(dev, MCU_CMD(PATCH_FINISH_REQ),
				 &req, sizeof(req), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_start_patch);

#define MCU_PATCH_ADDRESS	0x200000

int mt76_connac_mcu_init_download(struct mt76_dev *dev, u32 addr, u32 len,
				  u32 mode)
{
	struct {
		__le32 addr;
		__le32 len;
		__le32 mode;
	} req = {
		.addr = cpu_to_le32(addr),
		.len = cpu_to_le32(len),
		.mode = cpu_to_le32(mode),
	};
	int cmd;

	if ((!is_connac_v1(dev) && addr == MCU_PATCH_ADDRESS) ||
	    (is_mt7921(dev) && addr == 0x900000) ||
	    (is_mt7925(dev) && (addr == 0x900000 || addr == 0xe0002800)) ||
	    (is_mt7996(dev) && addr == 0x900000) ||
	    (is_mt7992(dev) && addr == 0x900000))
		cmd = MCU_CMD(PATCH_START_REQ);
	else
		cmd = MCU_CMD(TARGET_ADDRESS_LEN_REQ);

	return mt76_mcu_send_msg(dev, cmd, &req, sizeof(req), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_init_download);

int mt76_connac_mcu_set_channel_domain(struct mt76_phy *phy)
{
	int len, i, n_max_channels, n_2ch = 0, n_5ch = 0, n_6ch = 0;
	struct mt76_connac_mcu_channel_domain {
		u8 alpha2[4]; /* regulatory_request.alpha2 */
		u8 bw_2g; /* BW_20_40M		0
			   * BW_20M		1
			   * BW_20_40_80M	2
			   * BW_20_40_80_160M	3
			   * BW_20_40_80_8080M	4
			   */
		u8 bw_5g;
		u8 bw_6g;
		u8 pad;
		u8 n_2ch;
		u8 n_5ch;
		u8 n_6ch;
		u8 pad2;
	} __packed hdr = {
		.bw_2g = 0,
		.bw_5g = 3, /* BW_20_40_80_160M */
		.bw_6g = 3,
	};
	struct mt76_connac_mcu_chan {
		__le16 hw_value;
		__le16 pad;
		__le32 flags;
	} __packed channel;
	struct mt76_dev *dev = phy->dev;
	struct ieee80211_channel *chan;
	struct sk_buff *skb;

	n_max_channels = phy->sband_2g.sband.n_channels +
			 phy->sband_5g.sband.n_channels +
			 phy->sband_6g.sband.n_channels;
	len = sizeof(hdr) + n_max_channels * sizeof(channel);

	skb = mt76_mcu_msg_alloc(dev, NULL, len);
	if (!skb)
		return -ENOMEM;

	skb_reserve(skb, sizeof(hdr));

	for (i = 0; i < phy->sband_2g.sband.n_channels; i++) {
		chan = &phy->sband_2g.sband.channels[i];
		if (chan->flags & IEEE80211_CHAN_DISABLED)
			continue;

		channel.hw_value = cpu_to_le16(chan->hw_value);
		channel.flags = cpu_to_le32(chan->flags);
		channel.pad = 0;

		skb_put_data(skb, &channel, sizeof(channel));
		n_2ch++;
	}
	for (i = 0; i < phy->sband_5g.sband.n_channels; i++) {
		chan = &phy->sband_5g.sband.channels[i];
		if (chan->flags & IEEE80211_CHAN_DISABLED)
			continue;

		channel.hw_value = cpu_to_le16(chan->hw_value);
		channel.flags = cpu_to_le32(chan->flags);
		channel.pad = 0;

		skb_put_data(skb, &channel, sizeof(channel));
		n_5ch++;
	}
	for (i = 0; i < phy->sband_6g.sband.n_channels; i++) {
		chan = &phy->sband_6g.sband.channels[i];
		if (chan->flags & IEEE80211_CHAN_DISABLED)
			continue;

		channel.hw_value = cpu_to_le16(chan->hw_value);
		channel.flags = cpu_to_le32(chan->flags);
		channel.pad = 0;

		skb_put_data(skb, &channel, sizeof(channel));
		n_6ch++;
	}

	BUILD_BUG_ON(sizeof(dev->alpha2) > sizeof(hdr.alpha2));
	memcpy(hdr.alpha2, dev->alpha2, sizeof(dev->alpha2));
	hdr.n_2ch = n_2ch;
	hdr.n_5ch = n_5ch;
	hdr.n_6ch = n_6ch;

	memcpy(__skb_push(skb, sizeof(hdr)), &hdr, sizeof(hdr));

	return mt76_mcu_skb_send_msg(dev, skb, MCU_CE_CMD(SET_CHAN_DOMAIN),
				     false);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_set_channel_domain);

int mt76_connac_mcu_set_mac_enable(struct mt76_dev *dev, int band, bool enable,
				   bool hdr_trans)
{
	struct {
		u8 enable;
		u8 band;
		u8 rsv[2];
	} __packed req_mac = {
		.enable = enable,
		.band = band,
	};

	return mt76_mcu_send_msg(dev, MCU_EXT_CMD(MAC_INIT_CTRL), &req_mac,
				 sizeof(req_mac), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_set_mac_enable);

int mt76_connac_mcu_set_vif_ps(struct mt76_dev *dev, struct ieee80211_vif *vif)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct {
		u8 bss_idx;
		u8 ps_state; /* 0: device awake
			      * 1: static power save
			      * 2: dynamic power saving
			      */
	} req = {
		.bss_idx = mvif->idx,
		.ps_state = vif->cfg.ps ? 2 : 0,
	};

	if (vif->type != NL80211_IFTYPE_STATION)
		return -EOPNOTSUPP;

	return mt76_mcu_send_msg(dev, MCU_CE_CMD(SET_PS_PROFILE),
				 &req, sizeof(req), false);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_set_vif_ps);

int mt76_connac_mcu_set_rts_thresh(struct mt76_dev *dev, u32 val, u8 band)
{
	struct {
		u8 prot_idx;
		u8 band;
		u8 rsv[2];
		__le32 len_thresh;
		__le32 pkt_thresh;
	} __packed req = {
		.prot_idx = 1,
		.band = band,
		.len_thresh = cpu_to_le32(val),
		.pkt_thresh = cpu_to_le32(0x2),
	};

	return mt76_mcu_send_msg(dev, MCU_EXT_CMD(PROTECT_CTRL), &req,
				 sizeof(req), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_set_rts_thresh);

void mt76_connac_mcu_beacon_loss_iter(void *priv, u8 *mac,
				      struct ieee80211_vif *vif)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct mt76_connac_beacon_loss_event *event = priv;

	if (mvif->idx != event->bss_idx)
		return;

	if (!(vif->driver_flags & IEEE80211_VIF_BEACON_FILTER))
		return;

	ieee80211_beacon_loss(vif);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_beacon_loss_iter);

struct tlv *
mt76_connac_mcu_add_nested_tlv(struct sk_buff *skb, int tag, int len,
			       void *sta_ntlv, void *sta_wtbl)
{
	struct sta_ntlv_hdr *ntlv_hdr = sta_ntlv;
	struct tlv *sta_hdr = sta_wtbl;
	struct tlv *ptlv, tlv = {
		.tag = cpu_to_le16(tag),
		.len = cpu_to_le16(len),
	};
	u16 ntlv;

	ptlv = skb_put_zero(skb, len);
	memcpy(ptlv, &tlv, sizeof(tlv));

	ntlv = le16_to_cpu(ntlv_hdr->tlv_num);
	ntlv_hdr->tlv_num = cpu_to_le16(ntlv + 1);

	if (sta_hdr) {
		len += le16_to_cpu(sta_hdr->len);
		sta_hdr->len = cpu_to_le16(len);
	}

	return ptlv;
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_add_nested_tlv);

struct sk_buff *
__mt76_connac_mcu_alloc_sta_req(struct mt76_dev *dev, struct mt76_vif_link *mvif,
				struct mt76_wcid *wcid, int len)
{
	struct sta_req_hdr hdr = {
		.bss_idx = mvif->idx,
		.muar_idx = wcid ? mvif->omac_idx : 0,
		.is_tlv_append = 1,
	};
	struct sk_buff *skb;

	if (wcid && !wcid->sta && !wcid->sta_disabled)
		hdr.muar_idx = 0xe;

	mt76_connac_mcu_get_wlan_idx(dev, wcid, &hdr.wlan_idx_lo,
				     &hdr.wlan_idx_hi);
	skb = mt76_mcu_msg_alloc(dev, NULL, len);
	if (!skb)
		return ERR_PTR(-ENOMEM);

	skb_put_data(skb, &hdr, sizeof(hdr));

	return skb;
}
EXPORT_SYMBOL_GPL(__mt76_connac_mcu_alloc_sta_req);

struct wtbl_req_hdr *
mt76_connac_mcu_alloc_wtbl_req(struct mt76_dev *dev, struct mt76_wcid *wcid,
			       int cmd, void *sta_wtbl, struct sk_buff **skb)
{
	struct tlv *sta_hdr = sta_wtbl;
	struct wtbl_req_hdr hdr = {
		.operation = cmd,
	};
	struct sk_buff *nskb = *skb;

	mt76_connac_mcu_get_wlan_idx(dev, wcid, &hdr.wlan_idx_lo,
				     &hdr.wlan_idx_hi);
	if (!nskb) {
		nskb = mt76_mcu_msg_alloc(dev, NULL,
					  MT76_CONNAC_WTBL_UPDATE_MAX_SIZE);
		if (!nskb)
			return ERR_PTR(-ENOMEM);

		*skb = nskb;
	}

	if (sta_hdr)
		le16_add_cpu(&sta_hdr->len, sizeof(hdr));

	return skb_put_data(nskb, &hdr, sizeof(hdr));
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_alloc_wtbl_req);

void mt76_connac_mcu_bss_omac_tlv(struct sk_buff *skb,
				  struct ieee80211_vif *vif)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	u8 omac_idx = mvif->omac_idx;
	struct bss_info_omac *omac;
	struct tlv *tlv;
	u32 type = 0;

	switch (vif->type) {
	case NL80211_IFTYPE_MONITOR:
	case NL80211_IFTYPE_MESH_POINT:
	case NL80211_IFTYPE_AP:
		if (vif->p2p)
			type = CONNECTION_P2P_GO;
		else
			type = CONNECTION_INFRA_AP;
		break;
	case NL80211_IFTYPE_STATION:
		if (vif->p2p)
			type = CONNECTION_P2P_GC;
		else
			type = CONNECTION_INFRA_STA;
		break;
	case NL80211_IFTYPE_ADHOC:
		type = CONNECTION_IBSS_ADHOC;
		break;
	default:
		WARN_ON(1);
		break;
	}

	tlv = mt76_connac_mcu_add_tlv(skb, BSS_INFO_OMAC, sizeof(*omac));

	omac = (struct bss_info_omac *)tlv;
	omac->conn_type = cpu_to_le32(type);
	omac->omac_idx = mvif->omac_idx;
	omac->band_idx = mvif->band_idx;
	omac->hw_bss_idx = omac_idx > EXT_BSSID_START ? HW_BSSID_0 : omac_idx;
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_bss_omac_tlv);

void mt76_connac_mcu_sta_basic_tlv(struct mt76_dev *dev, struct sk_buff *skb,
				   struct ieee80211_bss_conf *link_conf,
				   struct ieee80211_link_sta *link_sta,
				   int conn_state, bool newly)
{
	struct ieee80211_vif *vif = link_conf->vif;
	struct sta_rec_basic *basic;
	struct tlv *tlv;
	int conn_type;

	tlv = mt76_connac_mcu_add_tlv(skb, STA_REC_BASIC, sizeof(*basic));

	basic = (struct sta_rec_basic *)tlv;
	basic->extra_info = cpu_to_le16(EXTRA_INFO_VER);

	if (newly && conn_state != CONN_STATE_DISCONNECT)
		basic->extra_info |= cpu_to_le16(EXTRA_INFO_NEW);
	basic->conn_state = conn_state;

	if (!link_sta) {
		basic->conn_type = cpu_to_le32(CONNECTION_INFRA_BC);

		if (vif->type == NL80211_IFTYPE_STATION &&
		    !is_zero_ether_addr(link_conf->bssid)) {
			memcpy(basic->peer_addr, link_conf->bssid, ETH_ALEN);
			basic->aid = cpu_to_le16(vif->cfg.aid);
		} else {
			eth_broadcast_addr(basic->peer_addr);
		}
		return;
	}

	switch (vif->type) {
	case NL80211_IFTYPE_MESH_POINT:
	case NL80211_IFTYPE_AP:
		if (vif->p2p && !is_mt7921(dev))
			conn_type = CONNECTION_P2P_GC;
		else
			conn_type = CONNECTION_INFRA_STA;
		basic->conn_type = cpu_to_le32(conn_type);
		basic->aid = cpu_to_le16(link_sta->sta->aid);
		break;
	case NL80211_IFTYPE_STATION:
		if (vif->p2p && !is_mt7921(dev))
			conn_type = CONNECTION_P2P_GO;
		else
			conn_type = CONNECTION_INFRA_AP;
		basic->conn_type = cpu_to_le32(conn_type);
		basic->aid = cpu_to_le16(vif->cfg.aid);
		break;
	case NL80211_IFTYPE_ADHOC:
		basic->conn_type = cpu_to_le32(CONNECTION_IBSS_ADHOC);
		basic->aid = cpu_to_le16(link_sta->sta->aid);
		break;
	default:
		WARN_ON(1);
		break;
	}

	memcpy(basic->peer_addr, link_sta->addr, ETH_ALEN);
	basic->qos = link_sta->sta->wme;
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_sta_basic_tlv);

void mt76_connac_mcu_sta_uapsd(struct sk_buff *skb, struct ieee80211_vif *vif,
			       struct ieee80211_sta *sta)
{
	struct sta_rec_uapsd *uapsd;
	struct tlv *tlv;

	if (vif->type != NL80211_IFTYPE_AP || !sta->wme)
		return;

	tlv = mt76_connac_mcu_add_tlv(skb, STA_REC_APPS, sizeof(*uapsd));
	uapsd = (struct sta_rec_uapsd *)tlv;

	if (sta->uapsd_queues & IEEE80211_WMM_IE_STA_QOSINFO_AC_VO) {
		uapsd->dac_map |= BIT(3);
		uapsd->tac_map |= BIT(3);
	}
	if (sta->uapsd_queues & IEEE80211_WMM_IE_STA_QOSINFO_AC_VI) {
		uapsd->dac_map |= BIT(2);
		uapsd->tac_map |= BIT(2);
	}
	if (sta->uapsd_queues & IEEE80211_WMM_IE_STA_QOSINFO_AC_BE) {
		uapsd->dac_map |= BIT(1);
		uapsd->tac_map |= BIT(1);
	}
	if (sta->uapsd_queues & IEEE80211_WMM_IE_STA_QOSINFO_AC_BK) {
		uapsd->dac_map |= BIT(0);
		uapsd->tac_map |= BIT(0);
	}
	uapsd->max_sp = sta->max_sp;
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_sta_uapsd);

void mt76_connac_mcu_wtbl_hdr_trans_tlv(struct sk_buff *skb,
					struct ieee80211_vif *vif,
					struct mt76_wcid *wcid,
					void *sta_wtbl, void *wtbl_tlv)
{
	struct wtbl_hdr_trans *htr;
	struct tlv *tlv;

	tlv = mt76_connac_mcu_add_nested_tlv(skb, WTBL_HDR_TRANS,
					     sizeof(*htr),
					     wtbl_tlv, sta_wtbl);
	htr = (struct wtbl_hdr_trans *)tlv;
	htr->no_rx_trans = true;

	if (vif->type == NL80211_IFTYPE_STATION)
		htr->to_ds = true;
	else
		htr->from_ds = true;

	if (!wcid)
		return;

	htr->no_rx_trans = !test_bit(MT_WCID_FLAG_HDR_TRANS, &wcid->flags);
	if (test_bit(MT_WCID_FLAG_4ADDR, &wcid->flags)) {
		htr->to_ds = true;
		htr->from_ds = true;
	}
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_wtbl_hdr_trans_tlv);

int mt76_connac_mcu_sta_update_hdr_trans(struct mt76_dev *dev,
					 struct ieee80211_vif *vif,
					 struct mt76_wcid *wcid, int cmd)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct wtbl_req_hdr *wtbl_hdr;
	struct tlv *sta_wtbl;
	struct sk_buff *skb;

	skb = mt76_connac_mcu_alloc_sta_req(dev, mvif, wcid);
	if (IS_ERR(skb))
		return PTR_ERR(skb);

	sta_wtbl = mt76_connac_mcu_add_tlv(skb, STA_REC_WTBL,
					   sizeof(struct tlv));

	wtbl_hdr = mt76_connac_mcu_alloc_wtbl_req(dev, wcid, WTBL_SET,
						  sta_wtbl, &skb);
	if (IS_ERR(wtbl_hdr))
		return PTR_ERR(wtbl_hdr);

	mt76_connac_mcu_wtbl_hdr_trans_tlv(skb, vif, wcid, sta_wtbl, wtbl_hdr);

	return mt76_mcu_skb_send_msg(dev, skb, cmd, true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_sta_update_hdr_trans);

int mt76_connac_mcu_wtbl_update_hdr_trans(struct mt76_dev *dev,
					  struct ieee80211_vif *vif,
					  struct ieee80211_sta *sta)
{
	struct mt76_wcid *wcid = (struct mt76_wcid *)sta->drv_priv;
	struct wtbl_req_hdr *wtbl_hdr;
	struct sk_buff *skb = NULL;

	wtbl_hdr = mt76_connac_mcu_alloc_wtbl_req(dev, wcid, WTBL_SET, NULL,
						  &skb);
	if (IS_ERR(wtbl_hdr))
		return PTR_ERR(wtbl_hdr);

	mt76_connac_mcu_wtbl_hdr_trans_tlv(skb, vif, wcid, NULL, wtbl_hdr);

	return mt76_mcu_skb_send_msg(dev, skb, MCU_EXT_CMD(WTBL_UPDATE), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_wtbl_update_hdr_trans);

void mt76_connac_mcu_wtbl_generic_tlv(struct mt76_dev *dev,
				      struct sk_buff *skb,
				      struct ieee80211_vif *vif,
				      struct ieee80211_sta *sta,
				      void *sta_wtbl, void *wtbl_tlv)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct wtbl_generic *generic;
	struct wtbl_rx *rx;
	struct wtbl_spe *spe;
	struct tlv *tlv;

	tlv = mt76_connac_mcu_add_nested_tlv(skb, WTBL_GENERIC,
					     sizeof(*generic),
					     wtbl_tlv, sta_wtbl);

	generic = (struct wtbl_generic *)tlv;

	if (sta) {
		if (vif->type == NL80211_IFTYPE_STATION)
			generic->partial_aid = cpu_to_le16(vif->cfg.aid);
		else
			generic->partial_aid = cpu_to_le16(sta->aid);
		memcpy(generic->peer_addr, sta->addr, ETH_ALEN);
		generic->muar_idx = mvif->omac_idx;
		generic->qos = sta->wme;
	} else {
		if (!is_connac_v1(dev) && vif->type == NL80211_IFTYPE_STATION)
			memcpy(generic->peer_addr, vif->bss_conf.bssid,
			       ETH_ALEN);
		else
			eth_broadcast_addr(generic->peer_addr);

		generic->muar_idx = 0xe;
	}

	tlv = mt76_connac_mcu_add_nested_tlv(skb, WTBL_RX, sizeof(*rx),
					     wtbl_tlv, sta_wtbl);

	rx = (struct wtbl_rx *)tlv;
	rx->rca1 = sta ? vif->type != NL80211_IFTYPE_AP : 1;
	rx->rca2 = 1;
	rx->rv = 1;

	if (!is_connac_v1(dev))
		return;

	tlv = mt76_connac_mcu_add_nested_tlv(skb, WTBL_SPE, sizeof(*spe),
					     wtbl_tlv, sta_wtbl);
	spe = (struct wtbl_spe *)tlv;
	spe->spe_idx = 24;
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_wtbl_generic_tlv);

static void
mt76_connac_mcu_sta_amsdu_tlv(struct sk_buff *skb, struct ieee80211_sta *sta,
			      struct ieee80211_vif *vif)
{
	struct mt76_wcid *wcid = (struct mt76_wcid *)sta->drv_priv;
	struct sta_rec_amsdu *amsdu;
	struct tlv *tlv;

	if (vif->type != NL80211_IFTYPE_AP &&
	    vif->type != NL80211_IFTYPE_STATION)
		return;

	if (!sta->deflink.agg.max_amsdu_len)
		return;

	tlv = mt76_connac_mcu_add_tlv(skb, STA_REC_HW_AMSDU, sizeof(*amsdu));
	amsdu = (struct sta_rec_amsdu *)tlv;
	amsdu->max_amsdu_num = 8;
	amsdu->amsdu_en = true;
	amsdu->max_mpdu_size = sta->deflink.agg.max_amsdu_len >=
			       IEEE80211_MAX_MPDU_LEN_VHT_7991;

	wcid->amsdu = true;
}

#define HE_PHY(p, c)	u8_get_bits(c, IEEE80211_HE_PHY_##p)
#define HE_MAC(m, c)	u8_get_bits(c, IEEE80211_HE_MAC_##m)
static void
mt76_connac_mcu_sta_he_tlv(struct sk_buff *skb, struct ieee80211_sta *sta)
{
	struct ieee80211_sta_he_cap *he_cap = &sta->deflink.he_cap;
	struct ieee80211_he_cap_elem *elem = &he_cap->he_cap_elem;
	struct sta_rec_he *he;
	struct tlv *tlv;
	u32 cap = 0;

	tlv = mt76_connac_mcu_add_tlv(skb, STA_REC_HE, sizeof(*he));

	he = (struct sta_rec_he *)tlv;

	if (elem->mac_cap_info[0] & IEEE80211_HE_MAC_CAP0_HTC_HE)
		cap |= STA_REC_HE_CAP_HTC;

	if (elem->mac_cap_info[2] & IEEE80211_HE_MAC_CAP2_BSR)
		cap |= STA_REC_HE_CAP_BSR;

	if (elem->mac_cap_info[3] & IEEE80211_HE_MAC_CAP3_OMI_CONTROL)
		cap |= STA_REC_HE_CAP_OM;

	if (elem->mac_cap_info[4] & IEEE80211_HE_MAC_CAP4_AMSDU_IN_AMPDU)
		cap |= STA_REC_HE_CAP_AMSDU_IN_AMPDU;

	if (elem->mac_cap_info[4] & IEEE80211_HE_MAC_CAP4_BQR)
		cap |= STA_REC_HE_CAP_BQR;

	if (elem->phy_cap_info[0] &
	    (IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_RU_MAPPING_IN_2G |
	     IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_RU_MAPPING_IN_5G))
		cap |= STA_REC_HE_CAP_BW20_RU242_SUPPORT;

	if (elem->phy_cap_info[1] &
	    IEEE80211_HE_PHY_CAP1_LDPC_CODING_IN_PAYLOAD)
		cap |= STA_REC_HE_CAP_LDPC;

	if (elem->phy_cap_info[1] &
	    IEEE80211_HE_PHY_CAP1_HE_LTF_AND_GI_FOR_HE_PPDUS_0_8US)
		cap |= STA_REC_HE_CAP_SU_PPDU_1LTF_8US_GI;

	if (elem->phy_cap_info[2] &
	    IEEE80211_HE_PHY_CAP2_NDP_4x_LTF_AND_3_2US)
		cap |= STA_REC_HE_CAP_NDP_4LTF_3DOT2MS_GI;

	if (elem->phy_cap_info[2] &
	    IEEE80211_HE_PHY_CAP2_STBC_TX_UNDER_80MHZ)
		cap |= STA_REC_HE_CAP_LE_EQ_80M_TX_STBC;

	if (elem->phy_cap_info[2] &
	    IEEE80211_HE_PHY_CAP2_STBC_RX_UNDER_80MHZ)
		cap |= STA_REC_HE_CAP_LE_EQ_80M_RX_STBC;

	if (elem->phy_cap_info[6] &
	    IEEE80211_HE_PHY_CAP6_PARTIAL_BW_EXT_RANGE)
		cap |= STA_REC_HE_CAP_PARTIAL_BW_EXT_RANGE;

	if (elem->phy_cap_info[7] &
	    IEEE80211_HE_PHY_CAP7_HE_SU_MU_PPDU_4XLTF_AND_08_US_GI)
		cap |= STA_REC_HE_CAP_SU_MU_PPDU_4LTF_8US_GI;

	if (elem->phy_cap_info[7] &
	    IEEE80211_HE_PHY_CAP7_STBC_TX_ABOVE_80MHZ)
		cap |= STA_REC_HE_CAP_GT_80M_TX_STBC;

	if (elem->phy_cap_info[7] &
	    IEEE80211_HE_PHY_CAP7_STBC_RX_ABOVE_80MHZ)
		cap |= STA_REC_HE_CAP_GT_80M_RX_STBC;

	if (elem->phy_cap_info[8] &
	    IEEE80211_HE_PHY_CAP8_HE_ER_SU_PPDU_4XLTF_AND_08_US_GI)
		cap |= STA_REC_HE_CAP_ER_SU_PPDU_4LTF_8US_GI;

	if (elem->phy_cap_info[8] &
	    IEEE80211_HE_PHY_CAP8_HE_ER_SU_1XLTF_AND_08_US_GI)
		cap |= STA_REC_HE_CAP_ER_SU_PPDU_1LTF_8US_GI;

	if (elem->phy_cap_info[9] &
	    IEEE80211_HE_PHY_CAP9_NON_TRIGGERED_CQI_FEEDBACK)
		cap |= STA_REC_HE_CAP_TRIG_CQI_FK;

	if (elem->phy_cap_info[9] &
	    IEEE80211_HE_PHY_CAP9_TX_1024_QAM_LESS_THAN_242_TONE_RU)
		cap |= STA_REC_HE_CAP_TX_1024QAM_UNDER_RU242;

	if (elem->phy_cap_info[9] &
	    IEEE80211_HE_PHY_CAP9_RX_1024_QAM_LESS_THAN_242_TONE_RU)
		cap |= STA_REC_HE_CAP_RX_1024QAM_UNDER_RU242;

	he->he_cap = cpu_to_le32(cap);

	switch (sta->deflink.bandwidth) {
	case IEEE80211_STA_RX_BW_160:
		if (elem->phy_cap_info[0] &
		    IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_80PLUS80_MHZ_IN_5G)
			he->max_nss_mcs[CMD_HE_MCS_BW8080] =
				he_cap->he_mcs_nss_supp.rx_mcs_80p80;

		he->max_nss_mcs[CMD_HE_MCS_BW160] =
				he_cap->he_mcs_nss_supp.rx_mcs_160;
		fallthrough;
	default:
		he->max_nss_mcs[CMD_HE_MCS_BW80] =
				he_cap->he_mcs_nss_supp.rx_mcs_80;
		break;
	}

	he->t_frame_dur =
		HE_MAC(CAP1_TF_MAC_PAD_DUR_MASK, elem->mac_cap_info[1]);
	he->max_ampdu_exp =
		HE_MAC(CAP3_MAX_AMPDU_LEN_EXP_MASK, elem->mac_cap_info[3]);

	he->bw_set =
		HE_PHY(CAP0_CHANNEL_WIDTH_SET_MASK, elem->phy_cap_info[0]);
	he->device_class =
		HE_PHY(CAP1_DEVICE_CLASS_A, elem->phy_cap_info[1]);
	he->punc_pream_rx =
		HE_PHY(CAP1_PREAMBLE_PUNC_RX_MASK, elem->phy_cap_info[1]);

	he->dcm_tx_mode =
		HE_PHY(CAP3_DCM_MAX_CONST_TX_MASK, elem->phy_cap_info[3]);
	he->dcm_tx_max_nss =
		HE_PHY(CAP3_DCM_MAX_TX_NSS_2, elem->phy_cap_info[3]);
	he->dcm_rx_mode =
		HE_PHY(CAP3_DCM_MAX_CONST_RX_MASK, elem->phy_cap_info[3]);
	he->dcm_rx_max_nss =
		HE_PHY(CAP3_DCM_MAX_RX_NSS_2, elem->phy_cap_info[3]);
	he->dcm_rx_max_nss =
		HE_PHY(CAP8_DCM_MAX_RU_MASK, elem->phy_cap_info[8]);

	he->pkt_ext = 2;
}

void
mt76_connac_mcu_sta_he_tlv_v2(struct sk_buff *skb, struct ieee80211_sta *sta)
{
	struct ieee80211_sta_he_cap *he_cap = &sta->deflink.he_cap;
	struct ieee80211_he_cap_elem *elem = &he_cap->he_cap_elem;
	struct sta_rec_he_v2 *he;
	struct tlv *tlv;

	tlv = mt76_connac_mcu_add_tlv(skb, STA_REC_HE_V2, sizeof(*he));

	he = (struct sta_rec_he_v2 *)tlv;
	memcpy(he->he_phy_cap, elem->phy_cap_info, sizeof(he->he_phy_cap));
	memcpy(he->he_mac_cap, elem->mac_cap_info, sizeof(he->he_mac_cap));

	switch (sta->deflink.bandwidth) {
	case IEEE80211_STA_RX_BW_160:
		if (elem->phy_cap_info[0] &
		    IEEE80211_HE_PHY_CAP0_CHANNEL_WIDTH_SET_80PLUS80_MHZ_IN_5G)
			he->max_nss_mcs[CMD_HE_MCS_BW8080] =
				he_cap->he_mcs_nss_supp.rx_mcs_80p80;

		he->max_nss_mcs[CMD_HE_MCS_BW160] =
				he_cap->he_mcs_nss_supp.rx_mcs_160;
		fallthrough;
	default:
		he->max_nss_mcs[CMD_HE_MCS_BW80] =
				he_cap->he_mcs_nss_supp.rx_mcs_80;
		break;
	}

	he->pkt_ext = IEEE80211_HE_PHY_CAP9_NOMINAL_PKT_PADDING_16US;
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_sta_he_tlv_v2);

u8
mt76_connac_get_phy_mode_v2(struct mt76_phy *mphy, struct ieee80211_vif *vif,
			    enum nl80211_band band,
			    struct ieee80211_link_sta *link_sta)
{
	struct ieee80211_sta_ht_cap *ht_cap;
	struct ieee80211_sta_vht_cap *vht_cap;
	const struct ieee80211_sta_he_cap *he_cap;
	const struct ieee80211_sta_eht_cap *eht_cap;
	u8 mode = 0;

	if (link_sta) {
		ht_cap = &link_sta->ht_cap;
		vht_cap = &link_sta->vht_cap;
		he_cap = &link_sta->he_cap;
		eht_cap = &link_sta->eht_cap;
	} else {
		struct ieee80211_supported_band *sband;

		sband = mphy->hw->wiphy->bands[band];
		ht_cap = &sband->ht_cap;
		vht_cap = &sband->vht_cap;
		he_cap = ieee80211_get_he_iftype_cap(sband, vif->type);
		eht_cap = ieee80211_get_eht_iftype_cap(sband, vif->type);
	}

	if (band == NL80211_BAND_2GHZ) {
		mode |= PHY_TYPE_BIT_HR_DSSS | PHY_TYPE_BIT_ERP;

		if (ht_cap->ht_supported)
			mode |= PHY_TYPE_BIT_HT;

		if (he_cap && he_cap->has_he)
			mode |= PHY_TYPE_BIT_HE;

		if (eht_cap && eht_cap->has_eht)
			mode |= PHY_TYPE_BIT_BE;
	} else if (band == NL80211_BAND_5GHZ || band == NL80211_BAND_6GHZ) {
		mode |= PHY_TYPE_BIT_OFDM;

		if (ht_cap->ht_supported)
			mode |= PHY_TYPE_BIT_HT;

		if (vht_cap->vht_supported)
			mode |= PHY_TYPE_BIT_VHT;

		if (he_cap && he_cap->has_he)
			mode |= PHY_TYPE_BIT_HE;

		if (eht_cap && eht_cap->has_eht)
			mode |= PHY_TYPE_BIT_BE;
	}

	return mode;
}
EXPORT_SYMBOL_GPL(mt76_connac_get_phy_mode_v2);

void mt76_connac_mcu_sta_tlv(struct mt76_phy *mphy, struct sk_buff *skb,
			     struct ieee80211_sta *sta,
			     struct ieee80211_vif *vif,
			     u8 rcpi, u8 sta_state)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct cfg80211_chan_def *chandef = mvif->ctx ?
					    &mvif->ctx->def : &mphy->chandef;
	enum nl80211_band band = chandef->chan->band;
	struct mt76_dev *dev = mphy->dev;
	struct sta_rec_ra_info *ra_info;
	struct sta_rec_state *state;
	struct sta_rec_phy *phy;
	struct tlv *tlv;
	u16 supp_rates;

	/* starec ht */
	if (sta->deflink.ht_cap.ht_supported) {
		struct sta_rec_ht *ht;

		tlv = mt76_connac_mcu_add_tlv(skb, STA_REC_HT, sizeof(*ht));
		ht = (struct sta_rec_ht *)tlv;
		ht->ht_cap = cpu_to_le16(sta->deflink.ht_cap.cap);
	}

	/* starec vht */
	if (sta->deflink.vht_cap.vht_supported) {
		struct sta_rec_vht *vht;
		int len;

		len = is_mt7921(dev) ? sizeof(*vht) : sizeof(*vht) - 4;
		tlv = mt76_connac_mcu_add_tlv(skb, STA_REC_VHT, len);
		vht = (struct sta_rec_vht *)tlv;
		vht->vht_cap = cpu_to_le32(sta->deflink.vht_cap.cap);
		vht->vht_rx_mcs_map = sta->deflink.vht_cap.vht_mcs.rx_mcs_map;
		vht->vht_tx_mcs_map = sta->deflink.vht_cap.vht_mcs.tx_mcs_map;
	}

	/* starec uapsd */
	mt76_connac_mcu_sta_uapsd(skb, vif, sta);

	if (!is_mt7921(dev))
		return;

	if (sta->deflink.ht_cap.ht_supported || sta->deflink.he_cap.has_he)
		mt76_connac_mcu_sta_amsdu_tlv(skb, sta, vif);

	/* starec he */
	if (sta->deflink.he_cap.has_he) {
		mt76_connac_mcu_sta_he_tlv(skb, sta);
		mt76_connac_mcu_sta_he_tlv_v2(skb, sta);
		if (band == NL80211_BAND_6GHZ &&
		    sta_state == MT76_STA_INFO_STATE_ASSOC) {
			struct sta_rec_he_6g_capa *he_6g_capa;

			tlv = mt76_connac_mcu_add_tlv(skb, STA_REC_HE_6G,
						      sizeof(*he_6g_capa));
			he_6g_capa = (struct sta_rec_he_6g_capa *)tlv;
			he_6g_capa->capa = sta->deflink.he_6ghz_capa.capa;
		}
	}

	tlv = mt76_connac_mcu_add_tlv(skb, STA_REC_PHY, sizeof(*phy));
	phy = (struct sta_rec_phy *)tlv;
	phy->phy_type = mt76_connac_get_phy_mode_v2(mphy, vif, band,
						    &sta->deflink);
	phy->basic_rate = cpu_to_le16((u16)vif->bss_conf.basic_rates);
	phy->rcpi = rcpi;
	phy->ampdu = FIELD_PREP(IEEE80211_HT_AMPDU_PARM_FACTOR,
				sta->deflink.ht_cap.ampdu_factor) |
		     FIELD_PREP(IEEE80211_HT_AMPDU_PARM_DENSITY,
				sta->deflink.ht_cap.ampdu_density);

	tlv = mt76_connac_mcu_add_tlv(skb, STA_REC_RA, sizeof(*ra_info));
	ra_info = (struct sta_rec_ra_info *)tlv;

	supp_rates = sta->deflink.supp_rates[band];
	if (band == NL80211_BAND_2GHZ)
		supp_rates = FIELD_PREP(RA_LEGACY_OFDM, supp_rates >> 4) |
			     FIELD_PREP(RA_LEGACY_CCK, supp_rates & 0xf);
	else
		supp_rates = FIELD_PREP(RA_LEGACY_OFDM, supp_rates);

	ra_info->legacy = cpu_to_le16(supp_rates);

	if (sta->deflink.ht_cap.ht_supported)
		memcpy(ra_info->rx_mcs_bitmask,
		       sta->deflink.ht_cap.mcs.rx_mask,
		       HT_MCS_MASK_NUM);

	tlv = mt76_connac_mcu_add_tlv(skb, STA_REC_STATE, sizeof(*state));
	state = (struct sta_rec_state *)tlv;
	state->state = sta_state;

	if (sta->deflink.vht_cap.vht_supported) {
		state->vht_opmode = sta->deflink.bandwidth;
		state->vht_opmode |= (sta->deflink.rx_nss - 1) <<
			IEEE80211_OPMODE_NOTIF_RX_NSS_SHIFT;
	}
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_sta_tlv);

void mt76_connac_mcu_wtbl_smps_tlv(struct sk_buff *skb,
				   struct ieee80211_sta *sta,
				   void *sta_wtbl, void *wtbl_tlv)
{
	struct wtbl_smps *smps;
	struct tlv *tlv;

	tlv = mt76_connac_mcu_add_nested_tlv(skb, WTBL_SMPS, sizeof(*smps),
					     wtbl_tlv, sta_wtbl);
	smps = (struct wtbl_smps *)tlv;
	smps->smps = (sta->deflink.smps_mode == IEEE80211_SMPS_DYNAMIC);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_wtbl_smps_tlv);

void mt76_connac_mcu_wtbl_ht_tlv(struct mt76_dev *dev, struct sk_buff *skb,
				 struct ieee80211_sta *sta, void *sta_wtbl,
				 void *wtbl_tlv, bool ht_ldpc, bool vht_ldpc)
{
	struct wtbl_ht *ht = NULL;
	struct tlv *tlv;
	u32 flags = 0;

	if (sta->deflink.ht_cap.ht_supported || sta->deflink.he_6ghz_capa.capa) {
		tlv = mt76_connac_mcu_add_nested_tlv(skb, WTBL_HT, sizeof(*ht),
						     wtbl_tlv, sta_wtbl);
		ht = (struct wtbl_ht *)tlv;
		ht->ldpc = ht_ldpc &&
			   !!(sta->deflink.ht_cap.cap & IEEE80211_HT_CAP_LDPC_CODING);

		if (sta->deflink.ht_cap.ht_supported) {
			ht->af = sta->deflink.ht_cap.ampdu_factor;
			ht->mm = sta->deflink.ht_cap.ampdu_density;
		} else {
			ht->af = le16_get_bits(sta->deflink.he_6ghz_capa.capa,
					       IEEE80211_HE_6GHZ_CAP_MAX_AMPDU_LEN_EXP);
			ht->mm = le16_get_bits(sta->deflink.he_6ghz_capa.capa,
					       IEEE80211_HE_6GHZ_CAP_MIN_MPDU_START);
		}

		ht->ht = true;
	}

	if (sta->deflink.vht_cap.vht_supported || sta->deflink.he_6ghz_capa.capa) {
		struct wtbl_vht *vht;
		u8 af;

		tlv = mt76_connac_mcu_add_nested_tlv(skb, WTBL_VHT,
						     sizeof(*vht), wtbl_tlv,
						     sta_wtbl);
		vht = (struct wtbl_vht *)tlv;
		vht->ldpc = vht_ldpc &&
			    !!(sta->deflink.vht_cap.cap & IEEE80211_VHT_CAP_RXLDPC);
		vht->vht = true;

		af = FIELD_GET(IEEE80211_VHT_CAP_MAX_A_MPDU_LENGTH_EXPONENT_MASK,
			       sta->deflink.vht_cap.cap);
		if (ht)
			ht->af = max(ht->af, af);
	}

	mt76_connac_mcu_wtbl_smps_tlv(skb, sta, sta_wtbl, wtbl_tlv);

	if (is_connac_v1(dev) && sta->deflink.ht_cap.ht_supported) {
		/* sgi */
		u32 msk = MT_WTBL_W5_SHORT_GI_20 | MT_WTBL_W5_SHORT_GI_40 |
			  MT_WTBL_W5_SHORT_GI_80 | MT_WTBL_W5_SHORT_GI_160;
		struct wtbl_raw *raw;

		tlv = mt76_connac_mcu_add_nested_tlv(skb, WTBL_RAW_DATA,
						     sizeof(*raw), wtbl_tlv,
						     sta_wtbl);

		if (sta->deflink.ht_cap.cap & IEEE80211_HT_CAP_SGI_20)
			flags |= MT_WTBL_W5_SHORT_GI_20;
		if (sta->deflink.ht_cap.cap & IEEE80211_HT_CAP_SGI_40)
			flags |= MT_WTBL_W5_SHORT_GI_40;

		if (sta->deflink.vht_cap.vht_supported) {
			if (sta->deflink.vht_cap.cap & IEEE80211_VHT_CAP_SHORT_GI_80)
				flags |= MT_WTBL_W5_SHORT_GI_80;
			if (sta->deflink.vht_cap.cap & IEEE80211_VHT_CAP_SHORT_GI_160)
				flags |= MT_WTBL_W5_SHORT_GI_160;
		}
		raw = (struct wtbl_raw *)tlv;
		raw->val = cpu_to_le32(flags);
		raw->msk = cpu_to_le32(~msk);
		raw->wtbl_idx = 1;
		raw->dw = 5;
	}
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_wtbl_ht_tlv);

int mt76_connac_mcu_sta_cmd(struct mt76_phy *phy,
			    struct mt76_sta_cmd_info *info)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)info->vif->drv_priv;
	struct ieee80211_link_sta *link_sta;
	struct mt76_dev *dev = phy->dev;
	struct wtbl_req_hdr *wtbl_hdr;
	struct tlv *sta_wtbl;
	struct sk_buff *skb;
	int conn_state;

	if (!info->link_conf)
		info->link_conf = &info->vif->bss_conf;

	skb = mt76_connac_mcu_alloc_sta_req(dev, mvif, info->wcid);
	if (IS_ERR(skb))
		return PTR_ERR(skb);

	conn_state = info->enable ? CONN_STATE_PORT_SECURE :
				    CONN_STATE_DISCONNECT;
	link_sta = info->sta ? &info->sta->deflink : NULL;
	if (info->sta || !info->offload_fw)
		mt76_connac_mcu_sta_basic_tlv(dev, skb, info->link_conf,
					      link_sta, conn_state,
					      info->newly);
	if (info->sta && info->enable)
		mt76_connac_mcu_sta_tlv(phy, skb, info->sta,
					info->vif, info->rcpi,
					info->state);

	sta_wtbl = mt76_connac_mcu_add_tlv(skb, STA_REC_WTBL,
					   sizeof(struct tlv));

	wtbl_hdr = mt76_connac_mcu_alloc_wtbl_req(dev, info->wcid,
						  WTBL_RESET_AND_SET,
						  sta_wtbl, &skb);
	if (IS_ERR(wtbl_hdr))
		return PTR_ERR(wtbl_hdr);

	if (info->enable) {
		mt76_connac_mcu_wtbl_generic_tlv(dev, skb, info->vif,
						 info->sta, sta_wtbl,
						 wtbl_hdr);
		mt76_connac_mcu_wtbl_hdr_trans_tlv(skb, info->vif, info->wcid,
						   sta_wtbl, wtbl_hdr);
		if (info->sta)
			mt76_connac_mcu_wtbl_ht_tlv(dev, skb, info->sta,
						    sta_wtbl, wtbl_hdr,
						    true, true);
	}

	return mt76_mcu_skb_send_msg(dev, skb, info->cmd, true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_sta_cmd);

void mt76_connac_mcu_wtbl_ba_tlv(struct mt76_dev *dev, struct sk_buff *skb,
				 struct ieee80211_ampdu_params *params,
				 bool enable, bool tx, void *sta_wtbl,
				 void *wtbl_tlv)
{
	struct wtbl_ba *ba;
	struct tlv *tlv;

	tlv = mt76_connac_mcu_add_nested_tlv(skb, WTBL_BA, sizeof(*ba),
					     wtbl_tlv, sta_wtbl);

	ba = (struct wtbl_ba *)tlv;
	ba->tid = params->tid;

	if (tx) {
		ba->ba_type = MT_BA_TYPE_ORIGINATOR;
		ba->sn = enable ? cpu_to_le16(params->ssn) : 0;
		ba->ba_winsize = enable ? cpu_to_le16(params->buf_size) : 0;
		ba->ba_en = enable;
	} else {
		memcpy(ba->peer_addr, params->sta->addr, ETH_ALEN);
		ba->ba_type = MT_BA_TYPE_RECIPIENT;
		ba->rst_ba_tid = params->tid;
		ba->rst_ba_sel = RST_BA_MAC_TID_MATCH;
		ba->rst_ba_sb = 1;
	}

	if (!is_connac_v1(dev)) {
		ba->ba_winsize = enable ? cpu_to_le16(params->buf_size) : 0;
		return;
	}

	if (enable && tx) {
		static const u8 ba_range[] = { 4, 8, 12, 24, 36, 48, 54, 64 };
		int i;

		for (i = 7; i > 0; i--) {
			if (params->buf_size >= ba_range[i])
				break;
		}
		ba->ba_winsize_idx = i;
	}
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_wtbl_ba_tlv);

int mt76_connac_mcu_uni_add_dev(struct mt76_phy *phy,
				struct ieee80211_bss_conf *bss_conf,
				struct mt76_vif_link *mvif,
				struct mt76_wcid *wcid,
				bool enable)
{
	struct mt76_dev *dev = phy->dev;
	struct {
		struct {
			u8 omac_idx;
			u8 band_idx;
			__le16 pad;
		} __packed hdr;
		struct req_tlv {
			__le16 tag;
			__le16 len;
			u8 active;
			u8 link_idx; /* not link_id */
			u8 omac_addr[ETH_ALEN];
		} __packed tlv;
	} dev_req = {
		.hdr = {
			.omac_idx = mvif->omac_idx,
			.band_idx = mvif->band_idx,
		},
		.tlv = {
			.tag = cpu_to_le16(DEV_INFO_ACTIVE),
			.len = cpu_to_le16(sizeof(struct req_tlv)),
			.active = enable,
			.link_idx = mvif->idx,
		},
	};
	struct {
		struct {
			u8 bss_idx;
			u8 pad[3];
		} __packed hdr;
		struct mt76_connac_bss_basic_tlv basic;
	} basic_req = {
		.hdr = {
			.bss_idx = mvif->idx,
		},
		.basic = {
			.tag = cpu_to_le16(UNI_BSS_INFO_BASIC),
			.len = cpu_to_le16(sizeof(struct mt76_connac_bss_basic_tlv)),
			.omac_idx = mvif->omac_idx,
			.band_idx = mvif->band_idx,
			.wmm_idx = mvif->wmm_idx,
			.active = enable,
			.bmc_tx_wlan_idx = cpu_to_le16(wcid->idx),
			.sta_idx = cpu_to_le16(wcid->idx),
			.conn_state = 1,
			.link_idx = mvif->idx,
		},
	};
	int err, idx, cmd, len;
	void *data;

	switch (bss_conf->vif->type) {
	case NL80211_IFTYPE_MESH_POINT:
	case NL80211_IFTYPE_MONITOR:
	case NL80211_IFTYPE_AP:
		basic_req.basic.conn_type = cpu_to_le32(CONNECTION_INFRA_AP);
		break;
	case NL80211_IFTYPE_STATION:
		basic_req.basic.conn_type = cpu_to_le32(CONNECTION_INFRA_STA);
		break;
	case NL80211_IFTYPE_P2P_DEVICE:
		basic_req.basic.conn_type = cpu_to_le32(CONNECTION_P2P_GO);
		break;
	case NL80211_IFTYPE_ADHOC:
		basic_req.basic.conn_type = cpu_to_le32(CONNECTION_IBSS_ADHOC);
		break;
	default:
		WARN_ON(1);
		break;
	}

	idx = mvif->omac_idx > EXT_BSSID_START ? HW_BSSID_0 : mvif->omac_idx;
	basic_req.basic.hw_bss_idx = idx;

	memcpy(dev_req.tlv.omac_addr, bss_conf->addr, ETH_ALEN);

	cmd = enable ? MCU_UNI_CMD(DEV_INFO_UPDATE) : MCU_UNI_CMD(BSS_INFO_UPDATE);
	data = enable ? (void *)&dev_req : (void *)&basic_req;
	len = enable ? sizeof(dev_req) : sizeof(basic_req);

	err = mt76_mcu_send_msg(dev, cmd, data, len, true);
	if (err < 0)
		return err;

	cmd = enable ? MCU_UNI_CMD(BSS_INFO_UPDATE) : MCU_UNI_CMD(DEV_INFO_UPDATE);
	data = enable ? (void *)&basic_req : (void *)&dev_req;
	len = enable ? sizeof(basic_req) : sizeof(dev_req);

	return mt76_mcu_send_msg(dev, cmd, data, len, true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_uni_add_dev);

void mt76_connac_mcu_sta_ba_tlv(struct sk_buff *skb,
				struct ieee80211_ampdu_params *params,
				bool enable, bool tx)
{
	struct sta_rec_ba *ba;
	struct tlv *tlv;

	tlv = mt76_connac_mcu_add_tlv(skb, STA_REC_BA, sizeof(*ba));

	ba = (struct sta_rec_ba *)tlv;
	ba->ba_type = tx ? MT_BA_TYPE_ORIGINATOR : MT_BA_TYPE_RECIPIENT;
	ba->winsize = cpu_to_le16(params->buf_size);
	ba->ssn = cpu_to_le16(params->ssn);
	ba->ba_en = enable << params->tid;
	ba->amsdu = params->amsdu;
	ba->tid = params->tid;
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_sta_ba_tlv);

int mt76_connac_mcu_sta_wed_update(struct mt76_dev *dev, struct sk_buff *skb)
{
	if (!mt76_is_mmio(dev))
		return 0;

	if (!mtk_wed_device_active(&dev->mmio.wed))
		return 0;

	return mtk_wed_device_update_msg(&dev->mmio.wed, WED_WO_STA_REC,
					 skb->data, skb->len);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_sta_wed_update);

int mt76_connac_mcu_sta_ba(struct mt76_dev *dev, struct mt76_vif_link *mvif,
			   struct ieee80211_ampdu_params *params,
			   int cmd, bool enable, bool tx)
{
	struct mt76_wcid *wcid = (struct mt76_wcid *)params->sta->drv_priv;
	struct wtbl_req_hdr *wtbl_hdr;
	struct tlv *sta_wtbl;
	struct sk_buff *skb;
	int ret;

	skb = mt76_connac_mcu_alloc_sta_req(dev, mvif, wcid);
	if (IS_ERR(skb))
		return PTR_ERR(skb);

	sta_wtbl = mt76_connac_mcu_add_tlv(skb, STA_REC_WTBL,
					   sizeof(struct tlv));

	wtbl_hdr = mt76_connac_mcu_alloc_wtbl_req(dev, wcid, WTBL_SET,
						  sta_wtbl, &skb);
	if (IS_ERR(wtbl_hdr))
		return PTR_ERR(wtbl_hdr);

	mt76_connac_mcu_wtbl_ba_tlv(dev, skb, params, enable, tx, sta_wtbl,
				    wtbl_hdr);

	ret = mt76_connac_mcu_sta_wed_update(dev, skb);
	if (ret)
		return ret;

	ret = mt76_mcu_skb_send_msg(dev, skb, cmd, true);
	if (ret)
		return ret;

	skb = mt76_connac_mcu_alloc_sta_req(dev, mvif, wcid);
	if (IS_ERR(skb))
		return PTR_ERR(skb);

	mt76_connac_mcu_sta_ba_tlv(skb, params, enable, tx);

	ret = mt76_connac_mcu_sta_wed_update(dev, skb);
	if (ret)
		return ret;

	return mt76_mcu_skb_send_msg(dev, skb, cmd, true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_sta_ba);

u8 mt76_connac_get_phy_mode(struct mt76_phy *phy, struct ieee80211_vif *vif,
			    enum nl80211_band band,
			    struct ieee80211_link_sta *link_sta)
{
	struct mt76_dev *dev = phy->dev;
	const struct ieee80211_sta_he_cap *he_cap;
	struct ieee80211_sta_vht_cap *vht_cap;
	struct ieee80211_sta_ht_cap *ht_cap;
	u8 mode = 0;

	if (is_connac_v1(dev))
		return 0x38;

	if (link_sta) {
		ht_cap = &link_sta->ht_cap;
		vht_cap = &link_sta->vht_cap;
		he_cap = &link_sta->he_cap;
	} else {
		struct ieee80211_supported_band *sband;

		sband = phy->hw->wiphy->bands[band];
		ht_cap = &sband->ht_cap;
		vht_cap = &sband->vht_cap;
		he_cap = ieee80211_get_he_iftype_cap(sband, vif->type);
	}

	if (band == NL80211_BAND_2GHZ) {
		mode |= PHY_MODE_B | PHY_MODE_G;

		if (ht_cap->ht_supported)
			mode |= PHY_MODE_GN;

		if (he_cap && he_cap->has_he)
			mode |= PHY_MODE_AX_24G;
	} else if (band == NL80211_BAND_5GHZ) {
		mode |= PHY_MODE_A;

		if (ht_cap->ht_supported)
			mode |= PHY_MODE_AN;

		if (vht_cap->vht_supported)
			mode |= PHY_MODE_AC;

		if (he_cap && he_cap->has_he)
			mode |= PHY_MODE_AX_5G;
	} else if (band == NL80211_BAND_6GHZ) {
		mode |= PHY_MODE_A | PHY_MODE_AN |
			PHY_MODE_AC | PHY_MODE_AX_5G;
	}

	return mode;
}
EXPORT_SYMBOL_GPL(mt76_connac_get_phy_mode);

u8 mt76_connac_get_phy_mode_ext(struct mt76_phy *phy, struct ieee80211_bss_conf *conf,
				enum nl80211_band band)
{
	const struct ieee80211_sta_eht_cap *eht_cap;
	struct ieee80211_supported_band *sband;
	u8 mode = 0;

	if (band == NL80211_BAND_6GHZ)
		mode |= PHY_MODE_AX_6G;

	sband = phy->hw->wiphy->bands[band];
	eht_cap = ieee80211_get_eht_iftype_cap(sband, conf->vif->type);

	if (!eht_cap || !eht_cap->has_eht || !conf->eht_support)
		return mode;

	switch (band) {
	case NL80211_BAND_6GHZ:
		mode |= PHY_MODE_BE_6G;
		break;
	case NL80211_BAND_5GHZ:
		mode |= PHY_MODE_BE_5G;
		break;
	case NL80211_BAND_2GHZ:
		mode |= PHY_MODE_BE_24G;
		break;
	default:
		break;
	}

	return mode;
}
EXPORT_SYMBOL_GPL(mt76_connac_get_phy_mode_ext);

const struct ieee80211_sta_he_cap *
mt76_connac_get_he_phy_cap(struct mt76_phy *phy, struct ieee80211_vif *vif)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct cfg80211_chan_def *chandef = mvif->ctx ?
					    &mvif->ctx->def : &phy->chandef;
	enum nl80211_band band = chandef->chan->band;
	struct ieee80211_supported_band *sband;

	sband = phy->hw->wiphy->bands[band];

	return ieee80211_get_he_iftype_cap(sband, vif->type);
}
EXPORT_SYMBOL_GPL(mt76_connac_get_he_phy_cap);

const struct ieee80211_sta_eht_cap *
mt76_connac_get_eht_phy_cap(struct mt76_phy *phy, struct ieee80211_vif *vif)
{
	enum nl80211_band band = phy->chandef.chan->band;
	struct ieee80211_supported_band *sband;

	sband = phy->hw->wiphy->bands[band];

	return ieee80211_get_eht_iftype_cap(sband, vif->type);
}
EXPORT_SYMBOL_GPL(mt76_connac_get_eht_phy_cap);

#define DEFAULT_HE_PE_DURATION		4
#define DEFAULT_HE_DURATION_RTS_THRES	1023
static void
mt76_connac_mcu_uni_bss_he_tlv(struct mt76_phy *phy, struct ieee80211_vif *vif,
			       struct tlv *tlv)
{
	const struct ieee80211_sta_he_cap *cap;
	struct bss_info_uni_he *he;

	cap = mt76_connac_get_he_phy_cap(phy, vif);

	he = (struct bss_info_uni_he *)tlv;
	he->he_pe_duration = vif->bss_conf.htc_trig_based_pkt_ext;
	if (!he->he_pe_duration)
		he->he_pe_duration = DEFAULT_HE_PE_DURATION;

	he->he_rts_thres = cpu_to_le16(vif->bss_conf.frame_time_rts_th);
	if (!he->he_rts_thres)
		he->he_rts_thres = cpu_to_le16(DEFAULT_HE_DURATION_RTS_THRES);

	he->max_nss_mcs[CMD_HE_MCS_BW80] = cap->he_mcs_nss_supp.tx_mcs_80;
	he->max_nss_mcs[CMD_HE_MCS_BW160] = cap->he_mcs_nss_supp.tx_mcs_160;
	he->max_nss_mcs[CMD_HE_MCS_BW8080] = cap->he_mcs_nss_supp.tx_mcs_80p80;
}

int mt76_connac_mcu_uni_set_chctx(struct mt76_phy *phy, struct mt76_vif_link *mvif,
				  struct ieee80211_chanctx_conf *ctx)
{
	struct cfg80211_chan_def *chandef = ctx ? &ctx->def : &phy->chandef;
	int freq1 = chandef->center_freq1, freq2 = chandef->center_freq2;
	enum nl80211_band band = chandef->chan->band;
	struct mt76_dev *mdev = phy->dev;
	struct {
		struct {
			u8 bss_idx;
			u8 pad[3];
		} __packed hdr;
		struct rlm_tlv {
			__le16 tag;
			__le16 len;
			u8 control_channel;
			u8 center_chan;
			u8 center_chan2;
			u8 bw;
			u8 tx_streams;
			u8 rx_streams;
			u8 short_st;
			u8 ht_op_info;
			u8 sco;
			u8 band;
			u8 pad[2];
		} __packed rlm;
	} __packed rlm_req = {
		.hdr = {
			.bss_idx = mvif->idx,
		},
		.rlm = {
			.tag = cpu_to_le16(UNI_BSS_INFO_RLM),
			.len = cpu_to_le16(sizeof(struct rlm_tlv)),
			.control_channel = chandef->chan->hw_value,
			.center_chan = ieee80211_frequency_to_channel(freq1),
			.center_chan2 = ieee80211_frequency_to_channel(freq2),
			.tx_streams = hweight8(phy->antenna_mask),
			.ht_op_info = 4, /* set HT 40M allowed */
			.rx_streams = phy->chainmask,
			.short_st = true,
			.band = band,
		},
	};

	switch (chandef->width) {
	case NL80211_CHAN_WIDTH_40:
		rlm_req.rlm.bw = CMD_CBW_40MHZ;
		break;
	case NL80211_CHAN_WIDTH_80:
		rlm_req.rlm.bw = CMD_CBW_80MHZ;
		break;
	case NL80211_CHAN_WIDTH_80P80:
		rlm_req.rlm.bw = CMD_CBW_8080MHZ;
		break;
	case NL80211_CHAN_WIDTH_160:
		rlm_req.rlm.bw = CMD_CBW_160MHZ;
		break;
	case NL80211_CHAN_WIDTH_5:
		rlm_req.rlm.bw = CMD_CBW_5MHZ;
		break;
	case NL80211_CHAN_WIDTH_10:
		rlm_req.rlm.bw = CMD_CBW_10MHZ;
		break;
	case NL80211_CHAN_WIDTH_20_NOHT:
	case NL80211_CHAN_WIDTH_20:
	default:
		rlm_req.rlm.bw = CMD_CBW_20MHZ;
		rlm_req.rlm.ht_op_info = 0;
		break;
	}

	if (rlm_req.rlm.control_channel < rlm_req.rlm.center_chan)
		rlm_req.rlm.sco = 1; /* SCA */
	else if (rlm_req.rlm.control_channel > rlm_req.rlm.center_chan)
		rlm_req.rlm.sco = 3; /* SCB */

	return mt76_mcu_send_msg(mdev, MCU_UNI_CMD(BSS_INFO_UPDATE), &rlm_req,
				 sizeof(rlm_req), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_uni_set_chctx);

int mt76_connac_mcu_uni_add_bss(struct mt76_phy *phy,
				struct ieee80211_vif *vif,
				struct mt76_wcid *wcid,
				bool enable,
				struct ieee80211_chanctx_conf *ctx)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct cfg80211_chan_def *chandef = ctx ? &ctx->def : &phy->chandef;
	enum nl80211_band band = chandef->chan->band;
	struct mt76_dev *mdev = phy->dev;
	struct {
		struct {
			u8 bss_idx;
			u8 pad[3];
		} __packed hdr;
		struct mt76_connac_bss_basic_tlv basic;
		struct mt76_connac_bss_qos_tlv qos;
	} basic_req = {
		.hdr = {
			.bss_idx = mvif->idx,
		},
		.basic = {
			.tag = cpu_to_le16(UNI_BSS_INFO_BASIC),
			.len = cpu_to_le16(sizeof(struct mt76_connac_bss_basic_tlv)),
			.bcn_interval = cpu_to_le16(vif->bss_conf.beacon_int),
			.dtim_period = vif->bss_conf.dtim_period,
			.omac_idx = mvif->omac_idx,
			.band_idx = mvif->band_idx,
			.wmm_idx = mvif->wmm_idx,
			.active = true, /* keep bss deactivated */
			.phymode = mt76_connac_get_phy_mode(phy, vif, band, NULL),
		},
		.qos = {
			.tag = cpu_to_le16(UNI_BSS_INFO_QBSS),
			.len = cpu_to_le16(sizeof(struct mt76_connac_bss_qos_tlv)),
			.qos = vif->bss_conf.qos,
		},
	};
	int err, conn_type;
	u8 idx, basic_phy;

	idx = mvif->omac_idx > EXT_BSSID_START ? HW_BSSID_0 : mvif->omac_idx;
	basic_req.basic.hw_bss_idx = idx;
	if (band == NL80211_BAND_6GHZ)
		basic_req.basic.phymode_ext = PHY_MODE_AX_6G;

	basic_phy = mt76_connac_get_phy_mode_v2(phy, vif, band, NULL);
	basic_req.basic.nonht_basic_phy = cpu_to_le16(basic_phy);

	switch (vif->type) {
	case NL80211_IFTYPE_MESH_POINT:
	case NL80211_IFTYPE_AP:
		if (vif->p2p)
			conn_type = CONNECTION_P2P_GO;
		else
			conn_type = CONNECTION_INFRA_AP;
		basic_req.basic.conn_type = cpu_to_le32(conn_type);
		/* Fully active/deactivate BSS network in AP mode only */
		basic_req.basic.active = enable;
		break;
	case NL80211_IFTYPE_STATION:
		if (vif->p2p)
			conn_type = CONNECTION_P2P_GC;
		else
			conn_type = CONNECTION_INFRA_STA;
		basic_req.basic.conn_type = cpu_to_le32(conn_type);
		break;
	case NL80211_IFTYPE_ADHOC:
		basic_req.basic.conn_type = cpu_to_le32(CONNECTION_IBSS_ADHOC);
		break;
	default:
		WARN_ON(1);
		break;
	}

	memcpy(basic_req.basic.bssid, vif->bss_conf.bssid, ETH_ALEN);
	basic_req.basic.bmc_tx_wlan_idx = cpu_to_le16(wcid->idx);
	basic_req.basic.sta_idx = cpu_to_le16(wcid->idx);
	basic_req.basic.conn_state = !enable;

	err = mt76_mcu_send_msg(mdev, MCU_UNI_CMD(BSS_INFO_UPDATE), &basic_req,
				sizeof(basic_req), true);
	if (err < 0)
		return err;

	if (vif->bss_conf.he_support) {
		struct {
			struct {
				u8 bss_idx;
				u8 pad[3];
			} __packed hdr;
			struct bss_info_uni_he he;
			struct bss_info_uni_bss_color bss_color;
		} he_req = {
			.hdr = {
				.bss_idx = mvif->idx,
			},
			.he = {
				.tag = cpu_to_le16(UNI_BSS_INFO_HE_BASIC),
				.len = cpu_to_le16(sizeof(struct bss_info_uni_he)),
			},
			.bss_color = {
				.tag = cpu_to_le16(UNI_BSS_INFO_BSS_COLOR),
				.len = cpu_to_le16(sizeof(struct bss_info_uni_bss_color)),
				.enable = 0,
				.bss_color = 0,
			},
		};

		if (enable) {
			he_req.bss_color.enable =
				vif->bss_conf.he_bss_color.enabled;
			he_req.bss_color.bss_color =
				vif->bss_conf.he_bss_color.color;
		}

		mt76_connac_mcu_uni_bss_he_tlv(phy, vif,
					       (struct tlv *)&he_req.he);
		err = mt76_mcu_send_msg(mdev, MCU_UNI_CMD(BSS_INFO_UPDATE),
					&he_req, sizeof(he_req), true);
		if (err < 0)
			return err;
	}

	return mt76_connac_mcu_uni_set_chctx(phy, mvif, ctx);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_uni_add_bss);

#define MT76_CONNAC_SCAN_CHANNEL_TIME		60
int mt76_connac_mcu_hw_scan(struct mt76_phy *phy, struct ieee80211_vif *vif,
			    struct ieee80211_scan_request *scan_req)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct cfg80211_scan_request *sreq = &scan_req->req;
	int n_ssids = 0, err, i, duration;
	int ext_channels_num = max_t(int, sreq->n_channels - 32, 0);
	struct ieee80211_channel **scan_list = sreq->channels;
	struct mt76_dev *mdev = phy->dev;
	struct mt76_connac_mcu_scan_channel *chan;
	struct mt76_connac_hw_scan_req *req;
	struct sk_buff *skb;

	if (test_bit(MT76_HW_SCANNING, &phy->state))
		return -EBUSY;

	skb = mt76_mcu_msg_alloc(mdev, NULL, sizeof(*req));
	if (!skb)
		return -ENOMEM;

	set_bit(MT76_HW_SCANNING, &phy->state);
	mvif->scan_seq_num = (mvif->scan_seq_num + 1) & 0x7f;

	req = (struct mt76_connac_hw_scan_req *)skb_put_zero(skb, sizeof(*req));

	req->seq_num = mvif->scan_seq_num | mvif->band_idx << 7;
	req->bss_idx = mvif->idx;
	req->scan_type = sreq->n_ssids ? 1 : 0;
	req->probe_req_num = sreq->n_ssids ? 2 : 0;
	req->version = 1;

	for (i = 0; i < sreq->n_ssids; i++) {
		if (!sreq->ssids[i].ssid_len)
			continue;

		req->ssids[i].ssid_len = cpu_to_le32(sreq->ssids[i].ssid_len);
		memcpy(req->ssids[i].ssid, sreq->ssids[i].ssid,
		       sreq->ssids[i].ssid_len);
		n_ssids++;
	}
	req->ssid_type = n_ssids ? BIT(2) : BIT(0);
	req->ssid_type_ext = n_ssids ? BIT(0) : 0;
	req->ssids_num = n_ssids;

	duration = is_mt7921(phy->dev) ? 0 : MT76_CONNAC_SCAN_CHANNEL_TIME;
	/* increase channel time for passive scan */
	if (!sreq->n_ssids)
		duration *= 2;
	req->timeout_value = cpu_to_le16(sreq->n_channels * duration);
	req->channel_min_dwell_time = cpu_to_le16(duration);
	req->channel_dwell_time = cpu_to_le16(duration);

	if (sreq->n_channels == 0 || sreq->n_channels > 64) {
		req->channel_type = 0;
		req->channels_num = 0;
		req->ext_channels_num = 0;
	} else {
		req->channel_type = 4;
		req->channels_num = min_t(u8, sreq->n_channels, 32);
		req->ext_channels_num = min_t(u8, ext_channels_num, 32);
	}

	for (i = 0; i < req->channels_num + req->ext_channels_num; i++) {
		if (i >= 32)
			chan = &req->ext_channels[i - 32];
		else
			chan = &req->channels[i];

		switch (scan_list[i]->band) {
		case NL80211_BAND_2GHZ:
			chan->band = 1;
			break;
		case NL80211_BAND_6GHZ:
			chan->band = 3;
			break;
		default:
			chan->band = 2;
			break;
		}
		chan->channel_num = scan_list[i]->hw_value;
	}

	if (sreq->ie_len > 0) {
		memcpy(req->ies, sreq->ie, sreq->ie_len);
		req->ies_len = cpu_to_le16(sreq->ie_len);
	}

	if (is_mt7921(phy->dev))
		req->scan_func |= SCAN_FUNC_SPLIT_SCAN;

	memcpy(req->bssid, sreq->bssid, ETH_ALEN);
	if (sreq->flags & NL80211_SCAN_FLAG_RANDOM_ADDR) {
		get_random_mask_addr(req->random_mac, sreq->mac_addr,
				     sreq->mac_addr_mask);
		req->scan_func |= SCAN_FUNC_RANDOM_MAC;
	}

	err = mt76_mcu_skb_send_msg(mdev, skb, MCU_CE_CMD(START_HW_SCAN),
				    false);
	if (err < 0)
		clear_bit(MT76_HW_SCANNING, &phy->state);

	return err;
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_hw_scan);

int mt76_connac_mcu_cancel_hw_scan(struct mt76_phy *phy,
				   struct ieee80211_vif *vif)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct {
		u8 seq_num;
		u8 is_ext_channel;
		u8 rsv[2];
	} __packed req = {
		.seq_num = mvif->scan_seq_num,
	};

	if (test_and_clear_bit(MT76_HW_SCANNING, &phy->state)) {
		struct cfg80211_scan_info info = {
			.aborted = true,
		};

		ieee80211_scan_completed(phy->hw, &info);
	}

	return mt76_mcu_send_msg(phy->dev, MCU_CE_CMD(CANCEL_HW_SCAN),
				 &req, sizeof(req), false);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_cancel_hw_scan);

int mt76_connac_mcu_sched_scan_req(struct mt76_phy *phy,
				   struct ieee80211_vif *vif,
				   struct cfg80211_sched_scan_request *sreq)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct ieee80211_channel **scan_list = sreq->channels;
	struct mt76_connac_mcu_scan_channel *chan;
	struct mt76_connac_sched_scan_req *req;
	struct mt76_dev *mdev = phy->dev;
	struct cfg80211_match_set *match;
	struct cfg80211_ssid *ssid;
	struct sk_buff *skb;
	int i;

	skb = mt76_mcu_msg_alloc(mdev, NULL, sizeof(*req) + sreq->ie_len);
	if (!skb)
		return -ENOMEM;

	mvif->scan_seq_num = (mvif->scan_seq_num + 1) & 0x7f;

	req = (struct mt76_connac_sched_scan_req *)skb_put_zero(skb, sizeof(*req));
	req->version = 1;
	req->seq_num = mvif->scan_seq_num | mvif->band_idx << 7;

	if (sreq->flags & NL80211_SCAN_FLAG_RANDOM_ADDR) {
		u8 *addr = is_mt7663(phy->dev) ? req->mt7663.random_mac
					       : req->mt7921.random_mac;

		req->scan_func = 1;
		get_random_mask_addr(addr, sreq->mac_addr,
				     sreq->mac_addr_mask);
	}
	if (is_mt7921(phy->dev)) {
		req->mt7921.bss_idx = mvif->idx;
		req->mt7921.delay = cpu_to_le32(sreq->delay);
	}

	req->ssids_num = sreq->n_ssids;
	for (i = 0; i < req->ssids_num; i++) {
		ssid = &sreq->ssids[i];
		memcpy(req->ssids[i].ssid, ssid->ssid, ssid->ssid_len);
		req->ssids[i].ssid_len = cpu_to_le32(ssid->ssid_len);
	}

	req->match_num = sreq->n_match_sets;
	for (i = 0; i < req->match_num; i++) {
		match = &sreq->match_sets[i];
		memcpy(req->match[i].ssid, match->ssid.ssid,
		       match->ssid.ssid_len);
		req->match[i].rssi_th = cpu_to_le32(match->rssi_thold);
		req->match[i].ssid_len = match->ssid.ssid_len;
	}

	req->channel_type = sreq->n_channels ? 4 : 0;
	req->channels_num = min_t(u8, sreq->n_channels, 64);
	for (i = 0; i < req->channels_num; i++) {
		chan = &req->channels[i];

		switch (scan_list[i]->band) {
		case NL80211_BAND_2GHZ:
			chan->band = 1;
			break;
		case NL80211_BAND_6GHZ:
			chan->band = 3;
			break;
		default:
			chan->band = 2;
			break;
		}
		chan->channel_num = scan_list[i]->hw_value;
	}

	req->intervals_num = sreq->n_scan_plans;
	for (i = 0; i < req->intervals_num; i++)
		req->intervals[i] = cpu_to_le16(sreq->scan_plans[i].interval);

	if (sreq->ie_len > 0) {
		req->ie_len = cpu_to_le16(sreq->ie_len);
		memcpy(skb_put(skb, sreq->ie_len), sreq->ie, sreq->ie_len);
	}

	return mt76_mcu_skb_send_msg(mdev, skb, MCU_CE_CMD(SCHED_SCAN_REQ),
				     false);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_sched_scan_req);

int mt76_connac_mcu_sched_scan_enable(struct mt76_phy *phy,
				      struct ieee80211_vif *vif,
				      bool enable)
{
	struct {
		u8 active; /* 0: enabled 1: disabled */
		u8 rsv[3];
	} __packed req = {
		.active = !enable,
	};

	if (enable)
		set_bit(MT76_HW_SCHED_SCANNING, &phy->state);
	else
		clear_bit(MT76_HW_SCHED_SCANNING, &phy->state);

	return mt76_mcu_send_msg(phy->dev, MCU_CE_CMD(SCHED_SCAN_ENABLE),
				 &req, sizeof(req), false);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_sched_scan_enable);

int mt76_connac_mcu_chip_config(struct mt76_dev *dev)
{
	struct mt76_connac_config req = {
		.resp_type = 0,
	};

	memcpy(req.data, "assert", 7);

	return mt76_mcu_send_msg(dev, MCU_CE_CMD(CHIP_CONFIG),
				 &req, sizeof(req), false);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_chip_config);

int mt76_connac_mcu_set_deep_sleep(struct mt76_dev *dev, bool enable)
{
	struct mt76_connac_config req = {
		.resp_type = 0,
	};

	snprintf(req.data, sizeof(req.data), "KeepFullPwr %d", !enable);

	return mt76_mcu_send_msg(dev, MCU_CE_CMD(CHIP_CONFIG),
				 &req, sizeof(req), false);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_set_deep_sleep);

int mt76_connac_sta_state_dp(struct mt76_dev *dev,
			     enum ieee80211_sta_state old_state,
			     enum ieee80211_sta_state new_state)
{
	if ((old_state == IEEE80211_STA_ASSOC &&
	     new_state == IEEE80211_STA_AUTHORIZED) ||
	    (old_state == IEEE80211_STA_NONE &&
	     new_state == IEEE80211_STA_NOTEXIST))
		mt76_connac_mcu_set_deep_sleep(dev, true);

	if ((old_state == IEEE80211_STA_NOTEXIST &&
	     new_state == IEEE80211_STA_NONE) ||
	    (old_state == IEEE80211_STA_AUTHORIZED &&
	     new_state == IEEE80211_STA_ASSOC))
		mt76_connac_mcu_set_deep_sleep(dev, false);

	return 0;
}
EXPORT_SYMBOL_GPL(mt76_connac_sta_state_dp);

void mt76_connac_mcu_coredump_event(struct mt76_dev *dev, struct sk_buff *skb,
				    struct mt76_connac_coredump *coredump)
{
	spin_lock_bh(&dev->lock);
	__skb_queue_tail(&coredump->msg_list, skb);
	spin_unlock_bh(&dev->lock);

	coredump->last_activity = jiffies;

	queue_delayed_work(dev->wq, &coredump->work,
			   MT76_CONNAC_COREDUMP_TIMEOUT);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_coredump_event);

static void
mt76_connac_mcu_build_sku(struct mt76_dev *dev, s8 *sku,
			  struct mt76_power_limits *limits,
			  enum nl80211_band band)
{
	int max_power = is_mt7921(dev) ? 127 : 63;
	int i, offset = sizeof(limits->cck);

	memset(sku, max_power, MT_SKU_POWER_LIMIT);

	if (band == NL80211_BAND_2GHZ) {
		/* cck */
		memcpy(sku, limits->cck, sizeof(limits->cck));
	}

	/* ofdm */
	memcpy(&sku[offset], limits->ofdm, sizeof(limits->ofdm));
	offset += sizeof(limits->ofdm);

	/* ht */
	for (i = 0; i < 2; i++) {
		memcpy(&sku[offset], limits->mcs[i], 8);
		offset += 8;
	}
	sku[offset++] = limits->mcs[0][0];

	/* vht */
	for (i = 0; i < ARRAY_SIZE(limits->mcs); i++) {
		memcpy(&sku[offset], limits->mcs[i],
		       ARRAY_SIZE(limits->mcs[i]));
		offset += 12;
	}

	if (!is_mt7921(dev))
		return;

	/* he */
	for (i = 0; i < ARRAY_SIZE(limits->ru); i++) {
		memcpy(&sku[offset], limits->ru[i], ARRAY_SIZE(limits->ru[i]));
		offset += ARRAY_SIZE(limits->ru[i]);
	}
}

s8 mt76_connac_get_ch_power(struct mt76_phy *phy,
			    struct ieee80211_channel *chan,
			    s8 target_power)
{
	struct mt76_dev *dev = phy->dev;
	struct ieee80211_supported_band *sband;
	int i;

	switch (chan->band) {
	case NL80211_BAND_2GHZ:
		sband = &phy->sband_2g.sband;
		break;
	case NL80211_BAND_5GHZ:
		sband = &phy->sband_5g.sband;
		break;
	case NL80211_BAND_6GHZ:
		sband = &phy->sband_6g.sband;
		break;
	default:
		return target_power;
	}

	for (i = 0; i < sband->n_channels; i++) {
		struct ieee80211_channel *ch = &sband->channels[i];

		if (ch->hw_value == chan->hw_value) {
			if (!(ch->flags & IEEE80211_CHAN_DISABLED)) {
				int power = 2 * ch->max_reg_power;

				if (is_mt7663(dev) && (power > 63 || power < -64))
					power = 63;
				target_power = min_t(s8, power, target_power);
			}
			break;
		}
	}

	return target_power;
}
EXPORT_SYMBOL_GPL(mt76_connac_get_ch_power);

static int
mt76_connac_mcu_rate_txpower_band(struct mt76_phy *phy,
				  enum nl80211_band band)
{
	struct mt76_dev *dev = phy->dev;
	int sku_len, batch_len = is_mt7921(dev) ? 8 : 16;
	static const u8 chan_list_2ghz[] = {
		1, 2,  3,  4,  5,  6,  7,
		8, 9, 10, 11, 12, 13, 14
	};
	static const u8 chan_list_5ghz[] = {
		 36,  38,  40,  42,  44,  46,  48,
		 50,  52,  54,  56,  58,  60,  62,
		 64, 100, 102, 104, 106, 108, 110,
		112, 114, 116, 118, 120, 122, 124,
		126, 128, 132, 134, 136, 138, 140,
		142, 144, 149, 151, 153, 155, 157,
		159, 161, 165, 169, 173, 177
	};
	static const u8 chan_list_6ghz[] = {
		  1,   3,   5,   7,   9,  11,  13,
		 15,  17,  19,  21,  23,  25,  27,
		 29,  33,  35,  37,  39,  41,  43,
		 45,  47,  49,  51,  53,  55,  57,
		 59,  61,  65,  67,  69,  71,  73,
		 75,  77,  79,  81,  83,  85,  87,
		 89,  91,  93,  97,  99, 101, 103,
		105, 107, 109, 111, 113, 115, 117,
		119, 121, 123, 125, 129, 131, 133,
		135, 137, 139, 141, 143, 145, 147,
		149, 151, 153, 155, 157, 161, 163,
		165, 167, 169, 171, 173, 175, 177,
		179, 181, 183, 185, 187, 189, 193,
		195, 197, 199, 201, 203, 205, 207,
		209, 211, 213, 215, 217, 219, 221,
		225, 227, 229, 233
	};
	int i, n_chan, batch_size, idx = 0, tx_power, last_ch, err = 0;
	struct mt76_connac_sku_tlv sku_tlbv;
	struct mt76_power_limits *limits;
	const u8 *ch_list;

	limits = devm_kmalloc(dev->dev, sizeof(*limits), GFP_KERNEL);
	if (!limits)
		return -ENOMEM;

	sku_len = is_mt7921(dev) ? sizeof(sku_tlbv) : sizeof(sku_tlbv) - 92;
	tx_power = 2 * phy->hw->conf.power_level;
	if (!tx_power)
		tx_power = 127;

	if (band == NL80211_BAND_2GHZ) {
		n_chan = ARRAY_SIZE(chan_list_2ghz);
		ch_list = chan_list_2ghz;
	} else if (band == NL80211_BAND_6GHZ) {
		n_chan = ARRAY_SIZE(chan_list_6ghz);
		ch_list = chan_list_6ghz;
	} else {
		n_chan = ARRAY_SIZE(chan_list_5ghz);
		ch_list = chan_list_5ghz;
	}
	batch_size = DIV_ROUND_UP(n_chan, batch_len);

	if (phy->cap.has_6ghz)
		last_ch = chan_list_6ghz[ARRAY_SIZE(chan_list_6ghz) - 1];
	else if (phy->cap.has_5ghz)
		last_ch = chan_list_5ghz[ARRAY_SIZE(chan_list_5ghz) - 1];
	else
		last_ch = chan_list_2ghz[ARRAY_SIZE(chan_list_2ghz) - 1];

	for (i = 0; i < batch_size; i++) {
		struct mt76_connac_tx_power_limit_tlv tx_power_tlv = {};
		int j, msg_len, num_ch;
		struct sk_buff *skb;

		num_ch = i == batch_size - 1 ? n_chan - i * batch_len : batch_len;
		msg_len = sizeof(tx_power_tlv) + num_ch * sizeof(sku_tlbv);
		skb = mt76_mcu_msg_alloc(dev, NULL, msg_len);
		if (!skb) {
			err = -ENOMEM;
			goto out;
		}

		skb_reserve(skb, sizeof(tx_power_tlv));

		BUILD_BUG_ON(sizeof(dev->alpha2) > sizeof(tx_power_tlv.alpha2));
		memcpy(tx_power_tlv.alpha2, dev->alpha2, sizeof(dev->alpha2));
		tx_power_tlv.n_chan = num_ch;

		switch (band) {
		case NL80211_BAND_2GHZ:
			tx_power_tlv.band = 1;
			break;
		case NL80211_BAND_6GHZ:
			tx_power_tlv.band = 3;
			break;
		default:
			tx_power_tlv.band = 2;
			break;
		}

		for (j = 0; j < num_ch; j++, idx++) {
			struct ieee80211_channel chan = {
				.hw_value = ch_list[idx],
				.band = band,
			};
			s8 reg_power, sar_power;

			reg_power = mt76_connac_get_ch_power(phy, &chan,
							     tx_power);
			sar_power = mt76_get_sar_power(phy, &chan, reg_power);

			mt76_get_rate_power_limits(phy, &chan, limits,
						   sar_power);

			tx_power_tlv.last_msg = ch_list[idx] == last_ch;
			sku_tlbv.channel = ch_list[idx];

			mt76_connac_mcu_build_sku(dev, sku_tlbv.pwr_limit,
						  limits, band);
			skb_put_data(skb, &sku_tlbv, sku_len);
		}
		__skb_push(skb, sizeof(tx_power_tlv));
		memcpy(skb->data, &tx_power_tlv, sizeof(tx_power_tlv));

		err = mt76_mcu_skb_send_msg(dev, skb,
					    MCU_CE_CMD(SET_RATE_TX_POWER),
					    false);
		if (err < 0)
			goto out;
	}

out:
	devm_kfree(dev->dev, limits);
	return err;
}

int mt76_connac_mcu_set_rate_txpower(struct mt76_phy *phy)
{
	int err;

	if (phy->cap.has_2ghz) {
		err = mt76_connac_mcu_rate_txpower_band(phy,
							NL80211_BAND_2GHZ);
		if (err < 0)
			return err;
	}
	if (phy->cap.has_5ghz) {
		err = mt76_connac_mcu_rate_txpower_band(phy,
							NL80211_BAND_5GHZ);
		if (err < 0)
			return err;
	}
	if (phy->cap.has_6ghz) {
		err = mt76_connac_mcu_rate_txpower_band(phy,
							NL80211_BAND_6GHZ);
		if (err < 0)
			return err;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_set_rate_txpower);

int mt76_connac_mcu_update_arp_filter(struct mt76_dev *dev,
				      struct mt76_vif_link *vif,
				      struct ieee80211_bss_conf *info)
{
	struct ieee80211_vif *mvif = container_of(info, struct ieee80211_vif,
						  bss_conf);
	struct sk_buff *skb;
	int i, len = min_t(int, mvif->cfg.arp_addr_cnt,
			   IEEE80211_BSS_ARP_ADDR_LIST_LEN);
	struct {
		struct {
			u8 bss_idx;
			u8 pad[3];
		} __packed hdr;
		struct mt76_connac_arpns_tlv arp;
	} req_hdr = {
		.hdr = {
			.bss_idx = vif->idx,
		},
		.arp = {
			.tag = cpu_to_le16(UNI_OFFLOAD_OFFLOAD_ARP),
			.len = cpu_to_le16(sizeof(struct mt76_connac_arpns_tlv)),
			.ips_num = len,
			.mode = 2,  /* update */
			.option = 1,
		},
	};

	skb = mt76_mcu_msg_alloc(dev, NULL,
				 sizeof(req_hdr) + len * sizeof(__be32));
	if (!skb)
		return -ENOMEM;

	skb_put_data(skb, &req_hdr, sizeof(req_hdr));
	for (i = 0; i < len; i++)
		skb_put_data(skb, &mvif->cfg.arp_addr_list[i], sizeof(__be32));

	return mt76_mcu_skb_send_msg(dev, skb, MCU_UNI_CMD(OFFLOAD), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_update_arp_filter);

int mt76_connac_mcu_set_p2p_oppps(struct ieee80211_hw *hw,
				  struct ieee80211_vif *vif)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	int ct_window = vif->bss_conf.p2p_noa_attr.oppps_ctwindow;
	struct mt76_phy *phy = hw->priv;
	struct {
		__le32 ct_win;
		u8 bss_idx;
		u8 rsv[3];
	} __packed req = {
		.ct_win = cpu_to_le32(ct_window),
		.bss_idx = mvif->idx,
	};

	return mt76_mcu_send_msg(phy->dev, MCU_CE_CMD(SET_P2P_OPPPS),
				 &req, sizeof(req), false);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_set_p2p_oppps);

#ifdef CONFIG_PM

const struct wiphy_wowlan_support mt76_connac_wowlan_support = {
	.flags = WIPHY_WOWLAN_MAGIC_PKT | WIPHY_WOWLAN_DISCONNECT |
		 WIPHY_WOWLAN_SUPPORTS_GTK_REKEY | WIPHY_WOWLAN_NET_DETECT,
	.n_patterns = 1,
	.pattern_min_len = 1,
	.pattern_max_len = MT76_CONNAC_WOW_PATTEN_MAX_LEN,
	.max_nd_match_sets = 10,
};
EXPORT_SYMBOL_GPL(mt76_connac_wowlan_support);

static void
mt76_connac_mcu_key_iter(struct ieee80211_hw *hw,
			 struct ieee80211_vif *vif,
			 struct ieee80211_sta *sta,
			 struct ieee80211_key_conf *key,
			 void *data)
{
	struct mt76_connac_gtk_rekey_tlv *gtk_tlv = data;
	u32 cipher;

	if (key->cipher != WLAN_CIPHER_SUITE_AES_CMAC &&
	    key->cipher != WLAN_CIPHER_SUITE_CCMP &&
	    key->cipher != WLAN_CIPHER_SUITE_TKIP)
		return;

	if (key->cipher == WLAN_CIPHER_SUITE_TKIP)
		cipher = BIT(3);
	else
		cipher = BIT(4);

	/* we are assuming here to have a single pairwise key */
	if (key->flags & IEEE80211_KEY_FLAG_PAIRWISE) {
		if (key->cipher == WLAN_CIPHER_SUITE_TKIP)
			gtk_tlv->proto = cpu_to_le32(NL80211_WPA_VERSION_1);
		else
			gtk_tlv->proto = cpu_to_le32(NL80211_WPA_VERSION_2);

		gtk_tlv->pairwise_cipher = cpu_to_le32(cipher);
		gtk_tlv->keyid = key->keyidx;
	} else {
		gtk_tlv->group_cipher = cpu_to_le32(cipher);
	}
}

int mt76_connac_mcu_update_gtk_rekey(struct ieee80211_hw *hw,
				     struct ieee80211_vif *vif,
				     struct cfg80211_gtk_rekey_data *key)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct mt76_connac_gtk_rekey_tlv *gtk_tlv;
	struct mt76_phy *phy = hw->priv;
	struct sk_buff *skb;
	struct {
		u8 bss_idx;
		u8 pad[3];
	} __packed hdr = {
		.bss_idx = mvif->idx,
	};

	skb = mt76_mcu_msg_alloc(phy->dev, NULL,
				 sizeof(hdr) + sizeof(*gtk_tlv));
	if (!skb)
		return -ENOMEM;

	skb_put_data(skb, &hdr, sizeof(hdr));
	gtk_tlv = (struct mt76_connac_gtk_rekey_tlv *)skb_put_zero(skb,
							 sizeof(*gtk_tlv));
	gtk_tlv->tag = cpu_to_le16(UNI_OFFLOAD_OFFLOAD_GTK_REKEY);
	gtk_tlv->len = cpu_to_le16(sizeof(*gtk_tlv));
	gtk_tlv->rekey_mode = 2;
	gtk_tlv->option = 1;

	rcu_read_lock();
	ieee80211_iter_keys_rcu(hw, vif, mt76_connac_mcu_key_iter, gtk_tlv);
	rcu_read_unlock();

	memcpy(gtk_tlv->kek, key->kek, NL80211_KEK_LEN);
	memcpy(gtk_tlv->kck, key->kck, NL80211_KCK_LEN);
	memcpy(gtk_tlv->replay_ctr, key->replay_ctr, NL80211_REPLAY_CTR_LEN);

	return mt76_mcu_skb_send_msg(phy->dev, skb,
				     MCU_UNI_CMD(OFFLOAD), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_update_gtk_rekey);

static int
mt76_connac_mcu_set_arp_filter(struct mt76_dev *dev, struct ieee80211_vif *vif,
			       bool suspend)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct {
		struct {
			u8 bss_idx;
			u8 pad[3];
		} __packed hdr;
		struct mt76_connac_arpns_tlv arpns;
	} req = {
		.hdr = {
			.bss_idx = mvif->idx,
		},
		.arpns = {
			.tag = cpu_to_le16(UNI_OFFLOAD_OFFLOAD_ARP),
			.len = cpu_to_le16(sizeof(struct mt76_connac_arpns_tlv)),
			.mode = suspend,
		},
	};

	return mt76_mcu_send_msg(dev, MCU_UNI_CMD(OFFLOAD), &req,
				 sizeof(req), true);
}

int
mt76_connac_mcu_set_gtk_rekey(struct mt76_dev *dev, struct ieee80211_vif *vif,
			      bool suspend)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct {
		struct {
			u8 bss_idx;
			u8 pad[3];
		} __packed hdr;
		struct mt76_connac_gtk_rekey_tlv gtk_tlv;
	} __packed req = {
		.hdr = {
			.bss_idx = mvif->idx,
		},
		.gtk_tlv = {
			.tag = cpu_to_le16(UNI_OFFLOAD_OFFLOAD_GTK_REKEY),
			.len = cpu_to_le16(sizeof(struct mt76_connac_gtk_rekey_tlv)),
			.rekey_mode = !suspend,
		},
	};

	return mt76_mcu_send_msg(dev, MCU_UNI_CMD(OFFLOAD), &req,
				 sizeof(req), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_set_gtk_rekey);

int
mt76_connac_mcu_set_suspend_mode(struct mt76_dev *dev,
				 struct ieee80211_vif *vif,
				 bool enable, u8 mdtim,
				 bool wow_suspend)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct {
		struct {
			u8 bss_idx;
			u8 pad[3];
		} __packed hdr;
		struct mt76_connac_suspend_tlv suspend_tlv;
	} req = {
		.hdr = {
			.bss_idx = mvif->idx,
		},
		.suspend_tlv = {
			.tag = cpu_to_le16(UNI_SUSPEND_MODE_SETTING),
			.len = cpu_to_le16(sizeof(struct mt76_connac_suspend_tlv)),
			.enable = enable,
			.mdtim = mdtim,
			.wow_suspend = wow_suspend,
		},
	};

	return mt76_mcu_send_msg(dev, MCU_UNI_CMD(SUSPEND), &req,
				 sizeof(req), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_set_suspend_mode);

static int
mt76_connac_mcu_set_wow_pattern(struct mt76_dev *dev,
				struct ieee80211_vif *vif,
				u8 index, bool enable,
				struct cfg80211_pkt_pattern *pattern)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct mt76_connac_wow_pattern_tlv *ptlv;
	struct sk_buff *skb;
	struct req_hdr {
		u8 bss_idx;
		u8 pad[3];
	} __packed hdr = {
		.bss_idx = mvif->idx,
	};

	skb = mt76_mcu_msg_alloc(dev, NULL, sizeof(hdr) + sizeof(*ptlv));
	if (!skb)
		return -ENOMEM;

	skb_put_data(skb, &hdr, sizeof(hdr));
	ptlv = (struct mt76_connac_wow_pattern_tlv *)skb_put_zero(skb, sizeof(*ptlv));
	ptlv->tag = cpu_to_le16(UNI_SUSPEND_WOW_PATTERN);
	ptlv->len = cpu_to_le16(sizeof(*ptlv));
	ptlv->data_len = pattern->pattern_len;
	ptlv->enable = enable;
	ptlv->index = index;

	memcpy(ptlv->pattern, pattern->pattern, pattern->pattern_len);
	memcpy(ptlv->mask, pattern->mask, DIV_ROUND_UP(pattern->pattern_len, 8));

	return mt76_mcu_skb_send_msg(dev, skb, MCU_UNI_CMD(SUSPEND), true);
}

int
mt76_connac_mcu_set_wow_ctrl(struct mt76_phy *phy, struct ieee80211_vif *vif,
			     bool suspend, struct cfg80211_wowlan *wowlan)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct mt76_dev *dev = phy->dev;
	struct {
		struct {
			u8 bss_idx;
			u8 pad[3];
		} __packed hdr;
		struct mt76_connac_wow_ctrl_tlv wow_ctrl_tlv;
		struct mt76_connac_wow_gpio_param_tlv gpio_tlv;
	} req = {
		.hdr = {
			.bss_idx = mvif->idx,
		},
		.wow_ctrl_tlv = {
			.tag = cpu_to_le16(UNI_SUSPEND_WOW_CTRL),
			.len = cpu_to_le16(sizeof(struct mt76_connac_wow_ctrl_tlv)),
			.cmd = suspend ? 1 : 2,
		},
		.gpio_tlv = {
			.tag = cpu_to_le16(UNI_SUSPEND_WOW_GPIO_PARAM),
			.len = cpu_to_le16(sizeof(struct mt76_connac_wow_gpio_param_tlv)),
			.gpio_pin = 0xff, /* follow fw about GPIO pin */
		},
	};

	if (wowlan->magic_pkt)
		req.wow_ctrl_tlv.trigger |= UNI_WOW_DETECT_TYPE_MAGIC;
	if (wowlan->disconnect)
		req.wow_ctrl_tlv.trigger |= (UNI_WOW_DETECT_TYPE_DISCONNECT |
					     UNI_WOW_DETECT_TYPE_BCN_LOST);
	if (wowlan->nd_config) {
		mt76_connac_mcu_sched_scan_req(phy, vif, wowlan->nd_config);
		req.wow_ctrl_tlv.trigger |= UNI_WOW_DETECT_TYPE_SCH_SCAN_HIT;
		mt76_connac_mcu_sched_scan_enable(phy, vif, suspend);
	}
	if (wowlan->n_patterns)
		req.wow_ctrl_tlv.trigger |= UNI_WOW_DETECT_TYPE_BITMAP;

	if (mt76_is_mmio(dev))
		req.wow_ctrl_tlv.wakeup_hif = WOW_PCIE;
	else if (mt76_is_usb(dev))
		req.wow_ctrl_tlv.wakeup_hif = WOW_USB;
	else if (mt76_is_sdio(dev))
		req.wow_ctrl_tlv.wakeup_hif = WOW_GPIO;

	return mt76_mcu_send_msg(dev, MCU_UNI_CMD(SUSPEND), &req,
				 sizeof(req), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_set_wow_ctrl);

int mt76_connac_mcu_set_hif_suspend(struct mt76_dev *dev, bool suspend, bool wait_resp)
{
	struct {
		struct {
			u8 hif_type; /* 0x0: HIF_SDIO
				      * 0x1: HIF_USB
				      * 0x2: HIF_PCIE
				      */
			u8 pad[3];
		} __packed hdr;
		struct hif_suspend_tlv {
			__le16 tag;
			__le16 len;
			u8 suspend;
			u8 pad[7];
		} __packed hif_suspend;
	} req = {
		.hif_suspend = {
			.tag = cpu_to_le16(0), /* 0: UNI_HIF_CTRL_BASIC */
			.len = cpu_to_le16(sizeof(struct hif_suspend_tlv)),
			.suspend = suspend,
		},
	};

	if (mt76_is_mmio(dev))
		req.hdr.hif_type = 2;
	else if (mt76_is_usb(dev))
		req.hdr.hif_type = 1;
	else if (mt76_is_sdio(dev))
		req.hdr.hif_type = 0;

	return mt76_mcu_send_msg(dev, MCU_UNI_CMD(HIF_CTRL), &req,
				 sizeof(req), wait_resp);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_set_hif_suspend);

void mt76_connac_mcu_set_suspend_iter(void *priv, u8 *mac,
				      struct ieee80211_vif *vif)
{
	struct mt76_phy *phy = priv;
	bool suspend = !test_bit(MT76_STATE_RUNNING, &phy->state);
	struct ieee80211_hw *hw = phy->hw;
	struct cfg80211_wowlan *wowlan = hw->wiphy->wowlan_config;
	int i;

	mt76_connac_mcu_set_gtk_rekey(phy->dev, vif, suspend);
	mt76_connac_mcu_set_arp_filter(phy->dev, vif, suspend);

	mt76_connac_mcu_set_suspend_mode(phy->dev, vif, suspend, 1, true);

	for (i = 0; i < wowlan->n_patterns; i++)
		mt76_connac_mcu_set_wow_pattern(phy->dev, vif, i, suspend,
						&wowlan->patterns[i]);
	mt76_connac_mcu_set_wow_ctrl(phy, vif, suspend, wowlan);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_set_suspend_iter);
#endif /* CONFIG_PM */

u32 mt76_connac_mcu_reg_rr(struct mt76_dev *dev, u32 offset)
{
	struct {
		__le32 addr;
		__le32 val;
	} __packed req = {
		.addr = cpu_to_le32(offset),
	};

	return mt76_mcu_send_msg(dev, MCU_CE_QUERY(REG_READ), &req,
				 sizeof(req), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_reg_rr);

void mt76_connac_mcu_reg_wr(struct mt76_dev *dev, u32 offset, u32 val)
{
	struct {
		__le32 addr;
		__le32 val;
	} __packed req = {
		.addr = cpu_to_le32(offset),
		.val = cpu_to_le32(val),
	};

	mt76_mcu_send_msg(dev, MCU_CE_CMD(REG_WRITE), &req,
			  sizeof(req), false);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_reg_wr);

static int
mt76_connac_mcu_sta_key_tlv(struct mt76_connac_sta_key_conf *sta_key_conf,
			    struct sk_buff *skb,
			    struct ieee80211_key_conf *key,
			    enum set_key_cmd cmd)
{
	struct sta_rec_sec *sec;
	u32 len = sizeof(*sec);
	struct tlv *tlv;

	tlv = mt76_connac_mcu_add_tlv(skb, STA_REC_KEY_V2, sizeof(*sec));
	sec = (struct sta_rec_sec *)tlv;
	sec->add = cmd;

	if (cmd == SET_KEY) {
		struct sec_key *sec_key;
		u8 cipher;

		cipher = mt76_connac_mcu_get_cipher(key->cipher);
		if (cipher == MCU_CIPHER_NONE)
			return -EOPNOTSUPP;

		sec_key = &sec->key[0];
		sec_key->cipher_len = sizeof(*sec_key);

		if (cipher == MCU_CIPHER_BIP_CMAC_128) {
			sec_key->cipher_id = MCU_CIPHER_AES_CCMP;
			sec_key->key_id = sta_key_conf->keyidx;
			sec_key->key_len = 16;
			memcpy(sec_key->key, sta_key_conf->key, 16);

			sec_key = &sec->key[1];
			sec_key->cipher_id = MCU_CIPHER_BIP_CMAC_128;
			sec_key->cipher_len = sizeof(*sec_key);
			sec_key->key_len = 16;
			memcpy(sec_key->key, key->key, 16);
			sec->n_cipher = 2;
		} else {
			sec_key->cipher_id = cipher;
			sec_key->key_id = key->keyidx;
			sec_key->key_len = key->keylen;
			memcpy(sec_key->key, key->key, key->keylen);

			if (cipher == MCU_CIPHER_TKIP) {
				/* Rx/Tx MIC keys are swapped */
				memcpy(sec_key->key + 16, key->key + 24, 8);
				memcpy(sec_key->key + 24, key->key + 16, 8);
			}

			/* store key_conf for BIP batch update */
			if (cipher == MCU_CIPHER_AES_CCMP) {
				memcpy(sta_key_conf->key, key->key, key->keylen);
				sta_key_conf->keyidx = key->keyidx;
			}

			len -= sizeof(*sec_key);
			sec->n_cipher = 1;
		}
	} else {
		len -= sizeof(sec->key);
		sec->n_cipher = 0;
	}
	sec->len = cpu_to_le16(len);

	return 0;
}

int mt76_connac_mcu_add_key(struct mt76_dev *dev, struct ieee80211_vif *vif,
			    struct mt76_connac_sta_key_conf *sta_key_conf,
			    struct ieee80211_key_conf *key, int mcu_cmd,
			    struct mt76_wcid *wcid, enum set_key_cmd cmd)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	struct sk_buff *skb;
	int ret;

	skb = mt76_connac_mcu_alloc_sta_req(dev, mvif, wcid);
	if (IS_ERR(skb))
		return PTR_ERR(skb);

	ret = mt76_connac_mcu_sta_key_tlv(sta_key_conf, skb, key, cmd);
	if (ret)
		return ret;

	ret = mt76_connac_mcu_sta_wed_update(dev, skb);
	if (ret)
		return ret;

	return mt76_mcu_skb_send_msg(dev, skb, mcu_cmd, true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_add_key);

/* SIFS 20us + 512 byte beacon transmitted by 1Mbps (3906us) */
#define BCN_TX_ESTIMATE_TIME (4096 + 20)
void mt76_connac_mcu_bss_ext_tlv(struct sk_buff *skb, struct mt76_vif_link *mvif)
{
	struct bss_info_ext_bss *ext;
	int ext_bss_idx, tsf_offset;
	struct tlv *tlv;

	ext_bss_idx = mvif->omac_idx - EXT_BSSID_START;
	if (ext_bss_idx < 0)
		return;

	tlv = mt76_connac_mcu_add_tlv(skb, BSS_INFO_EXT_BSS, sizeof(*ext));

	ext = (struct bss_info_ext_bss *)tlv;
	tsf_offset = ext_bss_idx * BCN_TX_ESTIMATE_TIME;
	ext->mbss_tsf_offset = cpu_to_le32(tsf_offset);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_bss_ext_tlv);

int mt76_connac_mcu_bss_basic_tlv(struct sk_buff *skb,
				  struct ieee80211_vif *vif,
				  struct ieee80211_sta *sta,
				  struct mt76_phy *phy, u16 wlan_idx,
				  bool enable)
{
	struct mt76_vif_link *mvif = (struct mt76_vif_link *)vif->drv_priv;
	u32 type = vif->p2p ? NETWORK_P2P : NETWORK_INFRA;
	struct bss_info_basic *bss;
	struct tlv *tlv;

	tlv = mt76_connac_mcu_add_tlv(skb, BSS_INFO_BASIC, sizeof(*bss));
	bss = (struct bss_info_basic *)tlv;

	switch (vif->type) {
	case NL80211_IFTYPE_MESH_POINT:
	case NL80211_IFTYPE_MONITOR:
		break;
	case NL80211_IFTYPE_AP:
		if (ieee80211_hw_check(phy->hw, SUPPORTS_MULTI_BSSID)) {
			u8 bssid_id = vif->bss_conf.bssid_indicator;
			struct wiphy *wiphy = phy->hw->wiphy;

			if (bssid_id > ilog2(wiphy->mbssid_max_interfaces))
				return -EINVAL;

			bss->non_tx_bssid = vif->bss_conf.bssid_index;
			bss->max_bssid = bssid_id;
		}
		break;
	case NL80211_IFTYPE_STATION:
		if (enable) {
			rcu_read_lock();
			if (!sta)
				sta = ieee80211_find_sta(vif,
							 vif->bss_conf.bssid);
			/* TODO: enable BSS_INFO_UAPSD & BSS_INFO_PM */
			if (sta) {
				struct mt76_wcid *wcid;

				wcid = (struct mt76_wcid *)sta->drv_priv;
				wlan_idx = wcid->idx;
			}
			rcu_read_unlock();
		}
		break;
	case NL80211_IFTYPE_ADHOC:
		type = NETWORK_IBSS;
		break;
	default:
		WARN_ON(1);
		break;
	}

	bss->network_type = cpu_to_le32(type);
	bss->bmc_wcid_lo = to_wcid_lo(wlan_idx);
	bss->bmc_wcid_hi = to_wcid_hi(wlan_idx);
	bss->wmm_idx = mvif->wmm_idx;
	bss->active = enable;
	bss->cipher = mvif->cipher;

	if (vif->type != NL80211_IFTYPE_MONITOR) {
		struct cfg80211_chan_def *chandef = &phy->chandef;

		memcpy(bss->bssid, vif->bss_conf.bssid, ETH_ALEN);
		bss->bcn_interval = cpu_to_le16(vif->bss_conf.beacon_int);
		bss->dtim_period = vif->bss_conf.dtim_period;
		bss->phy_mode = mt76_connac_get_phy_mode(phy, vif,
							 chandef->chan->band, NULL);
	} else {
		memcpy(bss->bssid, phy->macaddr, ETH_ALEN);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_bss_basic_tlv);

#define ENTER_PM_STATE		1
#define EXIT_PM_STATE		2
int mt76_connac_mcu_set_pm(struct mt76_dev *dev, int band, int enter)
{
	struct {
		u8 pm_number;
		u8 pm_state;
		u8 bssid[ETH_ALEN];
		u8 dtim_period;
		u8 wlan_idx_lo;
		__le16 bcn_interval;
		__le32 aid;
		__le32 rx_filter;
		u8 band_idx;
		u8 wlan_idx_hi;
		u8 rsv[2];
		__le32 feature;
		u8 omac_idx;
		u8 wmm_idx;
		u8 bcn_loss_cnt;
		u8 bcn_sp_duration;
	} __packed req = {
		.pm_number = 5,
		.pm_state = enter ? ENTER_PM_STATE : EXIT_PM_STATE,
		.band_idx = band,
	};

	return mt76_mcu_send_msg(dev, MCU_EXT_CMD(PM_STATE_CTRL), &req,
				 sizeof(req), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_set_pm);

int mt76_connac_mcu_restart(struct mt76_dev *dev)
{
	struct {
		u8 power_mode;
		u8 rsv[3];
	} req = {
		.power_mode = 1,
	};

	return mt76_mcu_send_msg(dev, MCU_CMD(NIC_POWER_CTRL), &req,
				 sizeof(req), false);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_restart);

int mt76_connac_mcu_del_wtbl_all(struct mt76_dev *dev)
{
	struct wtbl_req_hdr req = {
		.operation = WTBL_RESET_ALL,
	};

	return mt76_mcu_send_msg(dev, MCU_EXT_CMD(WTBL_UPDATE),
				 &req, sizeof(req), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_del_wtbl_all);

int mt76_connac_mcu_rdd_cmd(struct mt76_dev *dev, int cmd, u8 index,
			    u8 rx_sel, u8 val)
{
	struct {
		u8 ctrl;
		u8 rdd_idx;
		u8 rdd_rx_sel;
		u8 val;
		u8 rsv[4];
	} __packed req = {
		.ctrl = cmd,
		.rdd_idx = index,
		.rdd_rx_sel = rx_sel,
		.val = val,
	};

	return mt76_mcu_send_msg(dev, MCU_EXT_CMD(SET_RDD_CTRL), &req,
				 sizeof(req), true);
}
EXPORT_SYMBOL_GPL(mt76_connac_mcu_rdd_cmd);

static int
mt76_connac_mcu_send_ram_firmware(struct mt76_dev *dev,
				  const struct mt76_connac2_fw_trailer *hdr,
				  const u8 *data, bool is_wa)
{
	int i, offset = 0, max_len = mt76_is_sdio(dev) ? 2048 : 4096;
	u32 override = 0, option = 0;

	for (i = 0; i < hdr->n_region; i++) {
		const struct mt76_connac2_fw_region *region;
		u32 len, addr, mode;
		int err;

		region = (const void *)((const u8 *)hdr -
					(hdr->n_region - i) * sizeof(*region));
		mode = mt76_connac_mcu_gen_dl_mode(dev, region->feature_set,
						   is_wa);
		len = le32_to_cpu(region->len);
		addr = le32_to_cpu(region->addr);

		if (region->feature_set & FW_FEATURE_NON_DL)
			goto next;

		if (region->feature_set & FW_FEATURE_OVERRIDE_ADDR)
			override = addr;

		err = mt76_connac_mcu_init_download(dev, addr, len, mode);
		if (err) {
			dev_err(dev->dev, "Download request failed\n");
			return err;
		}

		err = __mt76_mcu_send_firmware(dev, MCU_CMD(FW_SCATTER),
					       data + offset, len, max_len);
		if (err) {
			dev_err(dev->dev, "Failed to send firmware.\n");
			return err;
		}

next:
		offset += len;
	}

	if (override)
		option |= FW_START_OVERRIDE;
	if (is_wa)
		option |= FW_START_WORKING_PDA_CR4;

	return mt76_connac_mcu_start_firmware(dev, override, option);
}

int mt76_connac2_load_ram(struct mt76_dev *dev, const char *fw_wm,
			  const char *fw_wa)
{
	const struct mt76_connac2_fw_trailer *hdr;
	const struct firmware *fw;
	int ret;

	ret = request_firmware(&fw, fw_wm, dev->dev);
	if (ret)
		return ret;

	if (!fw || !fw->data || fw->size < sizeof(*hdr)) {
		dev_err(dev->dev, "Invalid firmware\n");
		ret = -EINVAL;
		goto out;
	}

	hdr = (const void *)(fw->data + fw->size - sizeof(*hdr));
	dev_info(dev->dev, "WM Firmware Version: %.10s, Build Time: %.15s\n",
		 hdr->fw_ver, hdr->build_date);

	ret = mt76_connac_mcu_send_ram_firmware(dev, hdr, fw->data, false);
	if (ret) {
		dev_err(dev->dev, "Failed to start WM firmware\n");
		goto out;
	}

	snprintf(dev->hw->wiphy->fw_version,
		 sizeof(dev->hw->wiphy->fw_version),
		 "%.10s-%.15s", hdr->fw_ver, hdr->build_date);

	release_firmware(fw);

	if (!fw_wa)
		return 0;

	ret = request_firmware(&fw, fw_wa, dev->dev);
	if (ret)
		return ret;

	if (!fw || !fw->data || fw->size < sizeof(*hdr)) {
		dev_err(dev->dev, "Invalid firmware\n");
		ret = -EINVAL;
		goto out;
	}

	hdr = (const void *)(fw->data + fw->size - sizeof(*hdr));
	dev_info(dev->dev, "WA Firmware Version: %.10s, Build Time: %.15s\n",
		 hdr->fw_ver, hdr->build_date);

	ret = mt76_connac_mcu_send_ram_firmware(dev, hdr, fw->data, true);
	if (ret) {
		dev_err(dev->dev, "Failed to start WA firmware\n");
		goto out;
	}

	snprintf(dev->hw->wiphy->fw_version,
		 sizeof(dev->hw->wiphy->fw_version),
		 "%.10s-%.15s", hdr->fw_ver, hdr->build_date);

out:
	release_firmware(fw);

	return ret;
}
EXPORT_SYMBOL_GPL(mt76_connac2_load_ram);

static u32 mt76_connac2_get_data_mode(struct mt76_dev *dev, u32 info)
{
	u32 mode = DL_MODE_NEED_RSP;

	if ((!is_mt7921(dev) && !is_mt7925(dev)) || info == PATCH_SEC_NOT_SUPPORT)
		return mode;

	switch (FIELD_GET(PATCH_SEC_ENC_TYPE_MASK, info)) {
	case PATCH_SEC_ENC_TYPE_PLAIN:
		break;
	case PATCH_SEC_ENC_TYPE_AES:
		mode |= DL_MODE_ENCRYPT;
		mode |= FIELD_PREP(DL_MODE_KEY_IDX,
				(info & PATCH_SEC_ENC_AES_KEY_MASK)) & DL_MODE_KEY_IDX;
		mode |= DL_MODE_RESET_SEC_IV;
		break;
	case PATCH_SEC_ENC_TYPE_SCRAMBLE:
		mode |= DL_MODE_ENCRYPT;
		mode |= DL_CONFIG_ENCRY_MODE_SEL;
		mode |= DL_MODE_RESET_SEC_IV;
		break;
	default:
		dev_err(dev->dev, "Encryption type not support!\n");
	}

	return mode;
}

int mt76_connac2_load_patch(struct mt76_dev *dev, const char *fw_name)
{
	int i, ret, sem, max_len = mt76_is_sdio(dev) ? 2048 : 4096;
	const struct mt76_connac2_patch_hdr *hdr;
	const struct firmware *fw = NULL;

	sem = mt76_connac_mcu_patch_sem_ctrl(dev, true);
	switch (sem) {
	case PATCH_IS_DL:
		return 0;
	case PATCH_NOT_DL_SEM_SUCCESS:
		break;
	default:
		dev_err(dev->dev, "Failed to get patch semaphore\n");
		return -EAGAIN;
	}

	ret = request_firmware(&fw, fw_name, dev->dev);
	if (ret)
		goto out;

	if (!fw || !fw->data || fw->size < sizeof(*hdr)) {
		dev_err(dev->dev, "Invalid firmware\n");
		ret = -EINVAL;
		goto out;
	}

	hdr = (const void *)fw->data;
	dev_info(dev->dev, "HW/SW Version: 0x%x, Build Time: %.16s\n",
		 be32_to_cpu(hdr->hw_sw_ver), hdr->build_date);

	for (i = 0; i < be32_to_cpu(hdr->desc.n_region); i++) {
#if defined(__linux__)
		struct mt76_connac2_patch_sec *sec;
#elif defined(__FreeBSD__)
		const struct mt76_connac2_patch_sec *sec;
#endif
		u32 len, addr, mode;
		const u8 *dl;
		u32 sec_info;

#if defined(__linux__)
		sec = (void *)(fw->data + sizeof(*hdr) + i * sizeof(*sec));
#elif defined(__FreeBSD__)
		sec = (const void *)(fw->data + sizeof(*hdr) + i * sizeof(*sec));
#endif
		if ((be32_to_cpu(sec->type) & PATCH_SEC_TYPE_MASK) !=
		    PATCH_SEC_TYPE_INFO) {
			ret = -EINVAL;
			goto out;
		}

		addr = be32_to_cpu(sec->info.addr);
		len = be32_to_cpu(sec->info.len);
		dl = fw->data + be32_to_cpu(sec->offs);
		sec_info = be32_to_cpu(sec->info.sec_key_idx);
		mode = mt76_connac2_get_data_mode(dev, sec_info);

		ret = mt76_connac_mcu_init_download(dev, addr, len, mode);
		if (ret) {
			dev_err(dev->dev, "Download request failed\n");
			goto out;
		}

		ret = __mt76_mcu_send_firmware(dev, MCU_CMD(FW_SCATTER),
					       dl, len, max_len);
		if (ret) {
			dev_err(dev->dev, "Failed to send patch\n");
			goto out;
		}
	}

	ret = mt76_connac_mcu_start_patch(dev);
	if (ret)
		dev_err(dev->dev, "Failed to start patch\n");

out:
	sem = mt76_connac_mcu_patch_sem_ctrl(dev, false);
	switch (sem) {
	case PATCH_REL_SEM_SUCCESS:
		break;
	default:
		ret = -EAGAIN;
		dev_err(dev->dev, "Failed to release patch semaphore\n");
		break;
	}

	release_firmware(fw);

	return ret;
}
EXPORT_SYMBOL_GPL(mt76_connac2_load_patch);

int mt76_connac2_mcu_fill_message(struct mt76_dev *dev, struct sk_buff *skb,
				  int cmd, int *wait_seq)
{
	int txd_len, mcu_cmd = FIELD_GET(__MCU_CMD_FIELD_ID, cmd);
	struct mt76_connac2_mcu_uni_txd *uni_txd;
	struct mt76_connac2_mcu_txd *mcu_txd;
	__le32 *txd;
	u32 val;
	u8 seq;

	/* TODO: make dynamic based on msg type */
	dev->mcu.timeout = 20 * HZ;

	seq = ++dev->mcu.msg_seq & 0xf;
	if (!seq)
		seq = ++dev->mcu.msg_seq & 0xf;

	if (cmd == MCU_CMD(FW_SCATTER))
		goto exit;

	txd_len = cmd & __MCU_CMD_FIELD_UNI ? sizeof(*uni_txd) : sizeof(*mcu_txd);
	txd = (__le32 *)skb_push(skb, txd_len);

	val = FIELD_PREP(MT_TXD0_TX_BYTES, skb->len) |
	      FIELD_PREP(MT_TXD0_PKT_FMT, MT_TX_TYPE_CMD) |
	      FIELD_PREP(MT_TXD0_Q_IDX, MT_TX_MCU_PORT_RX_Q0);
	txd[0] = cpu_to_le32(val);

	val = MT_TXD1_LONG_FORMAT |
	      FIELD_PREP(MT_TXD1_HDR_FORMAT, MT_HDR_FORMAT_CMD);
	txd[1] = cpu_to_le32(val);

	if (cmd & __MCU_CMD_FIELD_UNI) {
		uni_txd = (struct mt76_connac2_mcu_uni_txd *)txd;
		uni_txd->len = cpu_to_le16(skb->len - sizeof(uni_txd->txd));
		uni_txd->option = MCU_CMD_UNI_EXT_ACK;
		uni_txd->cid = cpu_to_le16(mcu_cmd);
		uni_txd->s2d_index = MCU_S2D_H2N;
		uni_txd->pkt_type = MCU_PKT_ID;
		uni_txd->seq = seq;

		goto exit;
	}

	mcu_txd = (struct mt76_connac2_mcu_txd *)txd;
	mcu_txd->len = cpu_to_le16(skb->len - sizeof(mcu_txd->txd));
	mcu_txd->pq_id = cpu_to_le16(MCU_PQ_ID(MT_TX_PORT_IDX_MCU,
					       MT_TX_MCU_PORT_RX_Q0));
	mcu_txd->pkt_type = MCU_PKT_ID;
	mcu_txd->seq = seq;
	mcu_txd->cid = mcu_cmd;
	mcu_txd->ext_cid = FIELD_GET(__MCU_CMD_FIELD_EXT_ID, cmd);

	if (mcu_txd->ext_cid || (cmd & __MCU_CMD_FIELD_CE)) {
		if (cmd & __MCU_CMD_FIELD_QUERY)
			mcu_txd->set_query = MCU_Q_QUERY;
		else
			mcu_txd->set_query = MCU_Q_SET;
		mcu_txd->ext_cid_ack = !!mcu_txd->ext_cid;
	} else {
		mcu_txd->set_query = MCU_Q_NA;
	}

	if (cmd & __MCU_CMD_FIELD_WA)
		mcu_txd->s2d_index = MCU_S2D_H2C;
	else
		mcu_txd->s2d_index = MCU_S2D_H2N;

exit:
	if (wait_seq)
		*wait_seq = seq;

	return 0;
}
EXPORT_SYMBOL_GPL(mt76_connac2_mcu_fill_message);

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo@kernel.org>");
MODULE_DESCRIPTION("MediaTek MT76x connac layer helpers");
MODULE_LICENSE("Dual BSD/GPL");
