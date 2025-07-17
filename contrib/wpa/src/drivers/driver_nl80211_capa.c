/*
 * Driver interaction with Linux nl80211/cfg80211 - Capabilities
 * Copyright (c) 2002-2015, Jouni Malinen <j@w1.fi>
 * Copyright (c) 2007, Johannes Berg <johannes@sipsolutions.net>
 * Copyright (c) 2009-2010, Atheros Communications
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <netlink/genl/genl.h>

#include "utils/common.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_common.h"
#include "common/qca-vendor.h"
#include "common/qca-vendor-attr.h"
#include "common/brcm_vendor.h"
#include "driver_nl80211.h"


static int protocol_feature_handler(struct nl_msg *msg, void *arg)
{
	u32 *feat = arg;
	struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));

	nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb_msg[NL80211_ATTR_PROTOCOL_FEATURES])
		*feat = nla_get_u32(tb_msg[NL80211_ATTR_PROTOCOL_FEATURES]);

	return NL_SKIP;
}


static u32 get_nl80211_protocol_features(struct wpa_driver_nl80211_data *drv)
{
	u32 feat = 0;
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return 0;

	if (!nl80211_cmd(drv, msg, 0, NL80211_CMD_GET_PROTOCOL_FEATURES)) {
		nlmsg_free(msg);
		return 0;
	}

	if (send_and_recv_resp(drv, msg, protocol_feature_handler, &feat) == 0)
		return feat;

	return 0;
}


struct wiphy_info_data {
	struct wpa_driver_nl80211_data *drv;
	struct wpa_driver_capa *capa;

	unsigned int num_multichan_concurrent;

	unsigned int error:1;
	unsigned int device_ap_sme:1;
	unsigned int poll_command_supported:1;
	unsigned int data_tx_status:1;
	unsigned int auth_supported:1;
	unsigned int connect_supported:1;
	unsigned int p2p_go_supported:1;
	unsigned int p2p_client_supported:1;
	unsigned int p2p_go_ctwindow_supported:1;
	unsigned int p2p_concurrent:1;
	unsigned int channel_switch_supported:1;
	unsigned int set_qos_map_supported:1;
	unsigned int have_low_prio_scan:1;
	unsigned int wmm_ac_supported:1;
	unsigned int mac_addr_rand_scan_supported:1;
	unsigned int mac_addr_rand_sched_scan_supported:1;
	unsigned int update_ft_ies_supported:1;
	unsigned int has_key_mgmt:1;
	unsigned int has_key_mgmt_iftype:1;
};


static unsigned int probe_resp_offload_support(int supp_protocols)
{
	unsigned int prot = 0;

	if (supp_protocols & NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS)
		prot |= WPA_DRIVER_PROBE_RESP_OFFLOAD_WPS;
	if (supp_protocols & NL80211_PROBE_RESP_OFFLOAD_SUPPORT_WPS2)
		prot |= WPA_DRIVER_PROBE_RESP_OFFLOAD_WPS2;
	if (supp_protocols & NL80211_PROBE_RESP_OFFLOAD_SUPPORT_P2P)
		prot |= WPA_DRIVER_PROBE_RESP_OFFLOAD_P2P;
	if (supp_protocols & NL80211_PROBE_RESP_OFFLOAD_SUPPORT_80211U)
		prot |= WPA_DRIVER_PROBE_RESP_OFFLOAD_INTERWORKING;

	return prot;
}


static void wiphy_info_supported_iftypes(struct wiphy_info_data *info,
					 struct nlattr *tb)
{
	struct nlattr *nl_mode;
	int i;

	if (tb == NULL)
		return;

	nla_for_each_nested(nl_mode, tb, i) {
		switch (nla_type(nl_mode)) {
		case NL80211_IFTYPE_AP:
			info->capa->flags |= WPA_DRIVER_FLAGS_AP;
			break;
		case NL80211_IFTYPE_MESH_POINT:
			info->capa->flags |= WPA_DRIVER_FLAGS_MESH;
			break;
		case NL80211_IFTYPE_ADHOC:
			info->capa->flags |= WPA_DRIVER_FLAGS_IBSS;
			break;
		case NL80211_IFTYPE_P2P_DEVICE:
			info->capa->flags |=
				WPA_DRIVER_FLAGS_DEDICATED_P2P_DEVICE;
			break;
		case NL80211_IFTYPE_P2P_GO:
			info->p2p_go_supported = 1;
			break;
		case NL80211_IFTYPE_P2P_CLIENT:
			info->p2p_client_supported = 1;
			break;
		}
	}
}


static int wiphy_info_iface_comb_process(struct wiphy_info_data *info,
					 struct nlattr *nl_combi)
{
	struct nlattr *tb_comb[NUM_NL80211_IFACE_COMB];
	struct nlattr *tb_limit[NUM_NL80211_IFACE_LIMIT];
	struct nlattr *nl_limit, *nl_mode;
	int err, rem_limit, rem_mode;
	int combination_has_p2p = 0, combination_has_mgd = 0;
	static struct nla_policy
	iface_combination_policy[NUM_NL80211_IFACE_COMB] = {
		[NL80211_IFACE_COMB_LIMITS] = { .type = NLA_NESTED },
		[NL80211_IFACE_COMB_MAXNUM] = { .type = NLA_U32 },
		[NL80211_IFACE_COMB_STA_AP_BI_MATCH] = { .type = NLA_FLAG },
		[NL80211_IFACE_COMB_NUM_CHANNELS] = { .type = NLA_U32 },
		[NL80211_IFACE_COMB_RADAR_DETECT_WIDTHS] = { .type = NLA_U32 },
	},
	iface_limit_policy[NUM_NL80211_IFACE_LIMIT] = {
		[NL80211_IFACE_LIMIT_TYPES] = { .type = NLA_NESTED },
		[NL80211_IFACE_LIMIT_MAX] = { .type = NLA_U32 },
	};

	err = nla_parse_nested(tb_comb, MAX_NL80211_IFACE_COMB,
			       nl_combi, iface_combination_policy);
	if (err || !tb_comb[NL80211_IFACE_COMB_LIMITS] ||
	    !tb_comb[NL80211_IFACE_COMB_MAXNUM] ||
	    !tb_comb[NL80211_IFACE_COMB_NUM_CHANNELS])
		return 0; /* broken combination */

	if (tb_comb[NL80211_IFACE_COMB_RADAR_DETECT_WIDTHS])
		info->capa->flags |= WPA_DRIVER_FLAGS_RADAR;

	nla_for_each_nested(nl_limit, tb_comb[NL80211_IFACE_COMB_LIMITS],
			    rem_limit) {
		err = nla_parse_nested(tb_limit, MAX_NL80211_IFACE_LIMIT,
				       nl_limit, iface_limit_policy);
		if (err || !tb_limit[NL80211_IFACE_LIMIT_TYPES])
			return 0; /* broken combination */

		nla_for_each_nested(nl_mode,
				    tb_limit[NL80211_IFACE_LIMIT_TYPES],
				    rem_mode) {
			int ift = nla_type(nl_mode);
			if (ift == NL80211_IFTYPE_P2P_GO ||
			    ift == NL80211_IFTYPE_P2P_CLIENT)
				combination_has_p2p = 1;
			if (ift == NL80211_IFTYPE_STATION)
				combination_has_mgd = 1;
		}
		if (combination_has_p2p && combination_has_mgd)
			break;
	}

	if (combination_has_p2p && combination_has_mgd) {
		unsigned int num_channels =
			nla_get_u32(tb_comb[NL80211_IFACE_COMB_NUM_CHANNELS]);

		info->p2p_concurrent = 1;
		if (info->num_multichan_concurrent < num_channels)
			info->num_multichan_concurrent = num_channels;
	}

	return 0;
}


static void wiphy_info_iface_comb(struct wiphy_info_data *info,
				  struct nlattr *tb)
{
	struct nlattr *nl_combi;
	int rem_combi;

	if (tb == NULL)
		return;

	nla_for_each_nested(nl_combi, tb, rem_combi) {
		if (wiphy_info_iface_comb_process(info, nl_combi) > 0)
			break;
	}
}


static void wiphy_info_supp_cmds(struct wiphy_info_data *info,
				 struct nlattr *tb)
{
	struct nlattr *nl_cmd;
	int i;

	if (tb == NULL)
		return;

	nla_for_each_nested(nl_cmd, tb, i) {
		switch (nla_get_u32(nl_cmd)) {
		case NL80211_CMD_AUTHENTICATE:
			info->auth_supported = 1;
			break;
		case NL80211_CMD_CONNECT:
			info->connect_supported = 1;
			break;
		case NL80211_CMD_START_SCHED_SCAN:
			info->capa->sched_scan_supported = 1;
			break;
		case NL80211_CMD_PROBE_CLIENT:
			info->poll_command_supported = 1;
			break;
		case NL80211_CMD_CHANNEL_SWITCH:
			info->channel_switch_supported = 1;
			break;
		case NL80211_CMD_SET_QOS_MAP:
			info->set_qos_map_supported = 1;
			break;
		case NL80211_CMD_UPDATE_FT_IES:
			info->update_ft_ies_supported = 1;
			break;
		}
	}
}


static unsigned int get_akm_suites_info(struct nlattr *tb)
{
	int i, num;
	unsigned int key_mgmt = 0;
	u32 *akms;

	if (!tb)
		return 0;

	num = nla_len(tb) / sizeof(u32);
	akms = nla_data(tb);
	for (i = 0; i < num; i++) {
		switch (akms[i]) {
		case RSN_AUTH_KEY_MGMT_UNSPEC_802_1X:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_WPA |
				WPA_DRIVER_CAPA_KEY_MGMT_WPA2;
			break;
		case RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK |
				WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK;
			break;
		case RSN_AUTH_KEY_MGMT_FT_802_1X:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_FT;
			break;
		case RSN_AUTH_KEY_MGMT_FT_PSK:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_FT_PSK;
			break;
		case RSN_AUTH_KEY_MGMT_802_1X_SHA256:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_802_1X_SHA256;
			break;
		case RSN_AUTH_KEY_MGMT_PSK_SHA256:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_PSK_SHA256;
			break;
		case RSN_AUTH_KEY_MGMT_TPK_HANDSHAKE:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_TPK_HANDSHAKE;
			break;
		case RSN_AUTH_KEY_MGMT_FT_SAE:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_FT_SAE;
			break;
		case RSN_AUTH_KEY_MGMT_FT_SAE_EXT_KEY:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_FT_SAE_EXT_KEY;
			break;
		case RSN_AUTH_KEY_MGMT_FT_802_1X_SHA384:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_FT_802_1X_SHA384;
			break;
		case RSN_AUTH_KEY_MGMT_CCKM:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_CCKM;
			break;
		case RSN_AUTH_KEY_MGMT_OSEN:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_OSEN;
			break;
		case RSN_AUTH_KEY_MGMT_802_1X_SUITE_B:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_SUITE_B;
			break;
		case RSN_AUTH_KEY_MGMT_802_1X_SUITE_B_192:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_SUITE_B_192;
			break;
		case RSN_AUTH_KEY_MGMT_OWE:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_OWE;
			break;
		case RSN_AUTH_KEY_MGMT_DPP:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_DPP;
			break;
		case RSN_AUTH_KEY_MGMT_FILS_SHA256:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_FILS_SHA256;
			break;
		case RSN_AUTH_KEY_MGMT_FILS_SHA384:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_FILS_SHA384;
			break;
		case RSN_AUTH_KEY_MGMT_FT_FILS_SHA256:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_FT_FILS_SHA256;
			break;
		case RSN_AUTH_KEY_MGMT_FT_FILS_SHA384:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_FT_FILS_SHA384;
			break;
		case RSN_AUTH_KEY_MGMT_SAE:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_SAE;
			break;
		case RSN_AUTH_KEY_MGMT_SAE_EXT_KEY:
			key_mgmt |= WPA_DRIVER_CAPA_KEY_MGMT_SAE_EXT_KEY;
			break;
		}
	}

	return key_mgmt;
}


static void get_iface_akm_suites_info(struct wiphy_info_data *info,
					struct nlattr *nl_akms)
{
	struct nlattr *tb[NL80211_IFTYPE_AKM_ATTR_MAX + 1];
	struct nlattr *nl_iftype;
	unsigned int key_mgmt;
	int i;

	if (!nl_akms)
		return;

	nla_parse(tb, NL80211_IFTYPE_AKM_ATTR_MAX,
		  nla_data(nl_akms), nla_len(nl_akms), NULL);

	if (!tb[NL80211_IFTYPE_AKM_ATTR_IFTYPES] ||
	    !tb[NL80211_IFTYPE_AKM_ATTR_SUITES])
		return;

	info->has_key_mgmt_iftype = 1;
	key_mgmt = get_akm_suites_info(tb[NL80211_IFTYPE_AKM_ATTR_SUITES]);

	nla_for_each_nested(nl_iftype, tb[NL80211_IFTYPE_AKM_ATTR_IFTYPES], i) {
		switch (nla_type(nl_iftype)) {
		case NL80211_IFTYPE_ADHOC:
			info->drv->capa.key_mgmt_iftype[WPA_IF_IBSS] = key_mgmt;
			break;
		case NL80211_IFTYPE_STATION:
			info->drv->capa.key_mgmt_iftype[WPA_IF_STATION] =
				key_mgmt;
			break;
		case NL80211_IFTYPE_AP:
			info->drv->capa.key_mgmt_iftype[WPA_IF_AP_BSS] =
				key_mgmt;
			break;
		case NL80211_IFTYPE_AP_VLAN:
			info->drv->capa.key_mgmt_iftype[WPA_IF_AP_VLAN] =
				key_mgmt;
			break;
		case NL80211_IFTYPE_MESH_POINT:
			info->drv->capa.key_mgmt_iftype[WPA_IF_MESH] = key_mgmt;
			break;
		case NL80211_IFTYPE_P2P_CLIENT:
			info->drv->capa.key_mgmt_iftype[WPA_IF_P2P_CLIENT] =
				key_mgmt;
			break;
		case NL80211_IFTYPE_P2P_GO:
			info->drv->capa.key_mgmt_iftype[WPA_IF_P2P_GO] =
				key_mgmt;
			break;
		case NL80211_IFTYPE_P2P_DEVICE:
			info->drv->capa.key_mgmt_iftype[WPA_IF_P2P_DEVICE] =
				key_mgmt;
			break;
		case NL80211_IFTYPE_NAN:
			info->drv->capa.key_mgmt_iftype[WPA_IF_NAN] = key_mgmt;
			break;
		}
		wpa_printf(MSG_DEBUG, "nl80211: %s supported key_mgmt 0x%x",
			   nl80211_iftype_str(nla_type(nl_iftype)),
			   key_mgmt);
	}
}


static void wiphy_info_iftype_akm_suites(struct wiphy_info_data *info,
					 struct nlattr *tb)
{
	struct nlattr *nl_if;
	int rem_if;

	if (!tb)
		return;

	nla_for_each_nested(nl_if, tb, rem_if)
		get_iface_akm_suites_info(info, nl_if);
}


static void wiphy_info_akm_suites(struct wiphy_info_data *info,
				  struct nlattr *tb)
{
	if (!tb)
		return;

	info->has_key_mgmt = 1;
	info->capa->key_mgmt = get_akm_suites_info(tb);
	wpa_printf(MSG_DEBUG, "nl80211: wiphy supported key_mgmt 0x%x",
		   info->capa->key_mgmt);
}


static void wiphy_info_cipher_suites(struct wiphy_info_data *info,
				     struct nlattr *tb)
{
	int i, num;
	u32 *ciphers;

	if (tb == NULL)
		return;

	num = nla_len(tb) / sizeof(u32);
	ciphers = nla_data(tb);
	for (i = 0; i < num; i++) {
		u32 c = ciphers[i];

		wpa_printf(MSG_DEBUG, "nl80211: Supported cipher %02x-%02x-%02x:%d",
			   c >> 24, (c >> 16) & 0xff,
			   (c >> 8) & 0xff, c & 0xff);
		switch (c) {
		case RSN_CIPHER_SUITE_CCMP_256:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_CCMP_256;
			break;
		case RSN_CIPHER_SUITE_GCMP_256:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_GCMP_256;
			break;
		case RSN_CIPHER_SUITE_CCMP:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_CCMP;
			break;
		case RSN_CIPHER_SUITE_GCMP:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_GCMP;
			break;
		case RSN_CIPHER_SUITE_TKIP:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_TKIP;
			break;
		case RSN_CIPHER_SUITE_WEP104:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_WEP104;
			break;
		case RSN_CIPHER_SUITE_WEP40:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_WEP40;
			break;
		case RSN_CIPHER_SUITE_AES_128_CMAC:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_BIP;
			break;
		case RSN_CIPHER_SUITE_BIP_GMAC_128:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_BIP_GMAC_128;
			break;
		case RSN_CIPHER_SUITE_BIP_GMAC_256:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_BIP_GMAC_256;
			break;
		case RSN_CIPHER_SUITE_BIP_CMAC_256:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_BIP_CMAC_256;
			break;
		case RSN_CIPHER_SUITE_NO_GROUP_ADDRESSED:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_GTK_NOT_USED;
			break;
		}
	}
}


static void wiphy_info_max_roc(struct wpa_driver_capa *capa,
			       struct nlattr *tb)
{
	if (tb)
		capa->max_remain_on_chan = nla_get_u32(tb);
}


static void wiphy_info_tdls(struct wpa_driver_capa *capa, struct nlattr *tdls,
			    struct nlattr *ext_setup)
{
	if (tdls == NULL)
		return;

	wpa_printf(MSG_DEBUG, "nl80211: TDLS supported");
	capa->flags |= WPA_DRIVER_FLAGS_TDLS_SUPPORT;

	if (ext_setup) {
		wpa_printf(MSG_DEBUG, "nl80211: TDLS external setup");
		capa->flags |= WPA_DRIVER_FLAGS_TDLS_EXTERNAL_SETUP;
	}
}


static int ext_feature_isset(const u8 *ext_features, int ext_features_len,
			     enum nl80211_ext_feature_index ftidx)
{
	u8 ft_byte;

	if ((int) ftidx / 8 >= ext_features_len)
		return 0;

	ft_byte = ext_features[ftidx / 8];
	return (ft_byte & BIT(ftidx % 8)) != 0;
}


static void wiphy_info_ext_feature_flags(struct wiphy_info_data *info,
					 struct nlattr *tb)
{
	struct wpa_driver_capa *capa = info->capa;
	u8 *ext_features;
	int len;

	if (tb == NULL)
		return;

	ext_features = nla_data(tb);
	len = nla_len(tb);

	if (ext_feature_isset(ext_features, len, NL80211_EXT_FEATURE_VHT_IBSS))
		capa->flags |= WPA_DRIVER_FLAGS_VHT_IBSS;

	if (ext_feature_isset(ext_features, len, NL80211_EXT_FEATURE_RRM))
		capa->rrm_flags |= WPA_DRIVER_FLAGS_SUPPORT_RRM;

	if (ext_feature_isset(ext_features, len, NL80211_EXT_FEATURE_FILS_STA))
		capa->flags |= WPA_DRIVER_FLAGS_SUPPORT_FILS;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_BEACON_RATE_LEGACY))
		capa->flags |= WPA_DRIVER_FLAGS_BEACON_RATE_LEGACY;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_BEACON_RATE_HT))
		capa->flags |= WPA_DRIVER_FLAGS_BEACON_RATE_HT;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_BEACON_RATE_VHT))
		capa->flags |= WPA_DRIVER_FLAGS_BEACON_RATE_VHT;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_BEACON_RATE_HE))
		capa->flags2 |= WPA_DRIVER_FLAGS2_BEACON_RATE_HE;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_SET_SCAN_DWELL))
		capa->rrm_flags |= WPA_DRIVER_FLAGS_SUPPORT_SET_SCAN_DWELL;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_SCAN_START_TIME) &&
	    ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_BSS_PARENT_TSF) &&
	    ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_SET_SCAN_DWELL))
		capa->rrm_flags |= WPA_DRIVER_FLAGS_SUPPORT_BEACON_REPORT;
	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_MGMT_TX_RANDOM_TA))
		capa->flags |= WPA_DRIVER_FLAGS_MGMT_TX_RANDOM_TA;
	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_MGMT_TX_RANDOM_TA_CONNECTED))
		capa->flags |= WPA_DRIVER_FLAGS_MGMT_TX_RANDOM_TA_CONNECTED;
	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_SCHED_SCAN_RELATIVE_RSSI))
		capa->flags |= WPA_DRIVER_FLAGS_SCHED_SCAN_RELATIVE_RSSI;
	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_FILS_SK_OFFLOAD))
		capa->flags |= WPA_DRIVER_FLAGS_FILS_SK_OFFLOAD;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_4WAY_HANDSHAKE_STA_PSK))
		capa->flags |= WPA_DRIVER_FLAGS_4WAY_HANDSHAKE_PSK;
	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_4WAY_HANDSHAKE_STA_1X))
		capa->flags |= WPA_DRIVER_FLAGS_4WAY_HANDSHAKE_8021X;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_SAE_OFFLOAD))
		capa->flags2 |= WPA_DRIVER_FLAGS2_SAE_OFFLOAD_STA;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_MFP_OPTIONAL))
		capa->flags |= WPA_DRIVER_FLAGS_MFP_OPTIONAL;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_DFS_OFFLOAD))
		capa->flags |= WPA_DRIVER_FLAGS_DFS_OFFLOAD;

#ifdef CONFIG_MBO
	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_FILS_MAX_CHANNEL_TIME) &&
	    ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_ACCEPT_BCAST_PROBE_RESP) &&
	    ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_OCE_PROBE_REQ_HIGH_TX_RATE) &&
	    ext_feature_isset(
		    ext_features, len,
		    NL80211_EXT_FEATURE_OCE_PROBE_REQ_DEFERRAL_SUPPRESSION))
		capa->flags |= WPA_DRIVER_FLAGS_OCE_STA;
#endif /* CONFIG_MBO */

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_ENABLE_FTM_RESPONDER))
		capa->flags |= WPA_DRIVER_FLAGS_FTM_RESPONDER;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_CONTROL_PORT_OVER_NL80211))
		capa->flags |= WPA_DRIVER_FLAGS_CONTROL_PORT;
	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_CONTROL_PORT_NO_PREAUTH))
		capa->flags2 |= WPA_DRIVER_FLAGS2_CONTROL_PORT_RX;
	if (ext_feature_isset(
		    ext_features, len,
		    NL80211_EXT_FEATURE_CONTROL_PORT_OVER_NL80211_TX_STATUS))
		capa->flags2 |= WPA_DRIVER_FLAGS2_CONTROL_PORT_TX_STATUS;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_VLAN_OFFLOAD))
		capa->flags |= WPA_DRIVER_FLAGS_VLAN_OFFLOAD;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_CAN_REPLACE_PTK0))
		capa->flags |= WPA_DRIVER_FLAGS_SAFE_PTK0_REKEYS;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_BEACON_PROTECTION))
		capa->flags |= WPA_DRIVER_FLAGS_BEACON_PROTECTION;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_EXT_KEY_ID))
		capa->flags |= WPA_DRIVER_FLAGS_EXTENDED_KEY_ID;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_MULTICAST_REGISTRATIONS))
		info->drv->multicast_registrations = 1;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_FILS_DISCOVERY))
		info->drv->fils_discovery = 1;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_UNSOL_BCAST_PROBE_RESP))
		info->drv->unsol_bcast_probe_resp = 1;

	if (ext_feature_isset(ext_features, len, NL80211_EXT_FEATURE_PUNCT))
		info->drv->puncturing = 1;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_BEACON_PROTECTION_CLIENT))
		capa->flags2 |= WPA_DRIVER_FLAGS2_BEACON_PROTECTION_CLIENT;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_OPERATING_CHANNEL_VALIDATION))
		capa->flags2 |= WPA_DRIVER_FLAGS2_OCV;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_RADAR_BACKGROUND))
		capa->flags2 |= WPA_DRIVER_FLAGS2_RADAR_BACKGROUND;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_SECURE_LTF)) {
		capa->flags2 |= WPA_DRIVER_FLAGS2_SEC_LTF_STA;
		capa->flags2 |= WPA_DRIVER_FLAGS2_SEC_LTF_AP;
	}

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_SECURE_RTT)) {
		capa->flags2 |= WPA_DRIVER_FLAGS2_SEC_RTT_STA;
		capa->flags2 |= WPA_DRIVER_FLAGS2_SEC_RTT_AP;
	}

	if (ext_feature_isset(
		    ext_features, len,
		    NL80211_EXT_FEATURE_PROT_RANGE_NEGO_AND_MEASURE)) {
		capa->flags2 |= WPA_DRIVER_FLAGS2_PROT_RANGE_NEG_STA;
		capa->flags2 |= WPA_DRIVER_FLAGS2_PROT_RANGE_NEG_AP;
	}

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_SCAN_MIN_PREQ_CONTENT))
		capa->flags2 |= WPA_DRIVER_FLAGS2_SCAN_MIN_PREQ;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_4WAY_HANDSHAKE_AP_PSK))
		capa->flags2 |= WPA_DRIVER_FLAGS2_4WAY_HANDSHAKE_AP_PSK;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_OWE_OFFLOAD))
		capa->flags2 |= WPA_DRIVER_FLAGS2_OWE_OFFLOAD_STA;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_OWE_OFFLOAD_AP))
		capa->flags2 |= WPA_DRIVER_FLAGS2_OWE_OFFLOAD_AP;

	if (ext_feature_isset(ext_features, len,
			      NL80211_EXT_FEATURE_SAE_OFFLOAD_AP))
		capa->flags2 |= WPA_DRIVER_FLAGS2_SAE_OFFLOAD_AP;
}


static void wiphy_info_feature_flags(struct wiphy_info_data *info,
				     struct nlattr *tb)
{
	u32 flags;
	struct wpa_driver_capa *capa = info->capa;

	if (tb == NULL)
		return;

	flags = nla_get_u32(tb);

	if (flags & NL80211_FEATURE_SK_TX_STATUS)
		info->data_tx_status = 1;

	if (flags & NL80211_FEATURE_INACTIVITY_TIMER)
		capa->flags |= WPA_DRIVER_FLAGS_INACTIVITY_TIMER;

	if (flags & NL80211_FEATURE_SAE)
		capa->flags |= WPA_DRIVER_FLAGS_SAE;

	if (flags & NL80211_FEATURE_NEED_OBSS_SCAN)
		capa->flags |= WPA_DRIVER_FLAGS_OBSS_SCAN;

	if (flags & NL80211_FEATURE_AP_MODE_CHAN_WIDTH_CHANGE)
		capa->flags |= WPA_DRIVER_FLAGS_HT_2040_COEX;

	if (flags & NL80211_FEATURE_TDLS_CHANNEL_SWITCH) {
		wpa_printf(MSG_DEBUG, "nl80211: TDLS channel switch");
		capa->flags |= WPA_DRIVER_FLAGS_TDLS_CHANNEL_SWITCH;
	}

	if (flags & NL80211_FEATURE_P2P_GO_CTWIN)
		info->p2p_go_ctwindow_supported = 1;

	if (flags & NL80211_FEATURE_LOW_PRIORITY_SCAN)
		info->have_low_prio_scan = 1;

	if (flags & NL80211_FEATURE_SCAN_RANDOM_MAC_ADDR)
		info->mac_addr_rand_scan_supported = 1;

	if (flags & NL80211_FEATURE_SCHED_SCAN_RANDOM_MAC_ADDR)
		info->mac_addr_rand_sched_scan_supported = 1;

	if (flags & NL80211_FEATURE_SUPPORTS_WMM_ADMISSION)
		info->wmm_ac_supported = 1;

	if (flags & NL80211_FEATURE_DS_PARAM_SET_IE_IN_PROBES)
		capa->rrm_flags |= WPA_DRIVER_FLAGS_DS_PARAM_SET_IE_IN_PROBES;

	if (flags & NL80211_FEATURE_WFA_TPC_IE_IN_PROBES)
		capa->rrm_flags |= WPA_DRIVER_FLAGS_WFA_TPC_IE_IN_PROBES;

	if (flags & NL80211_FEATURE_QUIET)
		capa->rrm_flags |= WPA_DRIVER_FLAGS_QUIET;

	if (flags & NL80211_FEATURE_TX_POWER_INSERTION)
		capa->rrm_flags |= WPA_DRIVER_FLAGS_TX_POWER_INSERTION;

	if (flags & NL80211_FEATURE_HT_IBSS)
		capa->flags |= WPA_DRIVER_FLAGS_HT_IBSS;

	if (flags & NL80211_FEATURE_FULL_AP_CLIENT_STATE)
		capa->flags |= WPA_DRIVER_FLAGS_FULL_AP_CLIENT_STATE;
}


static void wiphy_info_probe_resp_offload(struct wpa_driver_capa *capa,
					  struct nlattr *tb)
{
	u32 protocols;

	if (tb == NULL)
		return;

	protocols = nla_get_u32(tb);
	wpa_printf(MSG_DEBUG, "nl80211: Supports Probe Response offload in AP "
		   "mode");
	capa->flags |= WPA_DRIVER_FLAGS_PROBE_RESP_OFFLOAD;
	capa->probe_resp_offloads = probe_resp_offload_support(protocols);
}


static void wiphy_info_wowlan_triggers(struct wpa_driver_capa *capa,
				       struct nlattr *tb)
{
	struct nlattr *triggers[MAX_NL80211_WOWLAN_TRIG + 1];

	if (tb == NULL)
		return;

	if (nla_parse_nested(triggers, MAX_NL80211_WOWLAN_TRIG,
			     tb, NULL))
		return;

	if (triggers[NL80211_WOWLAN_TRIG_ANY])
		capa->wowlan_triggers.any = 1;
	if (triggers[NL80211_WOWLAN_TRIG_DISCONNECT])
		capa->wowlan_triggers.disconnect = 1;
	if (triggers[NL80211_WOWLAN_TRIG_MAGIC_PKT])
		capa->wowlan_triggers.magic_pkt = 1;
	if (triggers[NL80211_WOWLAN_TRIG_GTK_REKEY_FAILURE])
		capa->wowlan_triggers.gtk_rekey_failure = 1;
	if (triggers[NL80211_WOWLAN_TRIG_EAP_IDENT_REQUEST])
		capa->wowlan_triggers.eap_identity_req = 1;
	if (triggers[NL80211_WOWLAN_TRIG_4WAY_HANDSHAKE])
		capa->wowlan_triggers.four_way_handshake = 1;
	if (triggers[NL80211_WOWLAN_TRIG_RFKILL_RELEASE])
		capa->wowlan_triggers.rfkill_release = 1;
}


static void wiphy_info_extended_capab(struct wpa_driver_nl80211_data *drv,
				      struct nlattr *tb)
{
	int rem = 0, i;
	struct nlattr *tb1[NL80211_ATTR_MAX + 1], *attr;

	if (!tb || drv->num_iface_capa == NL80211_IFTYPE_MAX)
		return;

	nla_for_each_nested(attr, tb, rem) {
		unsigned int len;
		struct drv_nl80211_iface_capa *capa;

		nla_parse(tb1, NL80211_ATTR_MAX, nla_data(attr),
			  nla_len(attr), NULL);

		if (!tb1[NL80211_ATTR_IFTYPE] ||
		    !tb1[NL80211_ATTR_EXT_CAPA] ||
		    !tb1[NL80211_ATTR_EXT_CAPA_MASK])
			continue;

		capa = &drv->iface_capa[drv->num_iface_capa];
		capa->iftype = nla_get_u32(tb1[NL80211_ATTR_IFTYPE]);
		wpa_printf(MSG_DEBUG,
			   "nl80211: Driver-advertised extended capabilities for interface type %s",
			   nl80211_iftype_str(capa->iftype));

		len = nla_len(tb1[NL80211_ATTR_EXT_CAPA]);
		capa->ext_capa = os_memdup(nla_data(tb1[NL80211_ATTR_EXT_CAPA]),
					   len);
		if (!capa->ext_capa)
			goto err;

		capa->ext_capa_len = len;
		wpa_hexdump(MSG_DEBUG, "nl80211: Extended capabilities",
			    capa->ext_capa, capa->ext_capa_len);

		len = nla_len(tb1[NL80211_ATTR_EXT_CAPA_MASK]);
		capa->ext_capa_mask =
			os_memdup(nla_data(tb1[NL80211_ATTR_EXT_CAPA_MASK]),
				  len);
		if (!capa->ext_capa_mask)
			goto err;

		wpa_hexdump(MSG_DEBUG, "nl80211: Extended capabilities mask",
			    capa->ext_capa_mask, capa->ext_capa_len);

		if (tb1[NL80211_ATTR_EML_CAPABILITY] &&
		    tb1[NL80211_ATTR_MLD_CAPA_AND_OPS]) {
			capa->eml_capa =
				nla_get_u16(tb1[NL80211_ATTR_EML_CAPABILITY]);
			capa->mld_capa_and_ops =
				nla_get_u16(tb1[NL80211_ATTR_MLD_CAPA_AND_OPS]);
		}

		wpa_printf(MSG_DEBUG,
			   "nl80211: EML Capability: 0x%x MLD Capability: 0x%x",
			   capa->eml_capa, capa->mld_capa_and_ops);

		drv->num_iface_capa++;
		if (drv->num_iface_capa == NL80211_IFTYPE_MAX)
			break;
	}

	return;

err:
	/* Cleanup allocated memory on error */
	for (i = 0; i < NL80211_IFTYPE_MAX; i++) {
		os_free(drv->iface_capa[i].ext_capa);
		drv->iface_capa[i].ext_capa = NULL;
		os_free(drv->iface_capa[i].ext_capa_mask);
		drv->iface_capa[i].ext_capa_mask = NULL;
		drv->iface_capa[i].ext_capa_len = 0;
	}
	drv->num_iface_capa = 0;
}


static void wiphy_info_mbssid(struct wpa_driver_capa *cap, struct nlattr *attr)
{
	struct nlattr *config[NL80211_MBSSID_CONFIG_ATTR_MAX + 1];

	if (nla_parse_nested(config, NL80211_MBSSID_CONFIG_ATTR_MAX, attr,
			     NULL))
		return;

	if (!config[NL80211_MBSSID_CONFIG_ATTR_MAX_INTERFACES])
		return;

	cap->mbssid_max_interfaces =
		nla_get_u8(config[NL80211_MBSSID_CONFIG_ATTR_MAX_INTERFACES]);

	if (config[NL80211_MBSSID_CONFIG_ATTR_MAX_EMA_PROFILE_PERIODICITY])
		cap->ema_max_periodicity =
			nla_get_u8(config[NL80211_MBSSID_CONFIG_ATTR_MAX_EMA_PROFILE_PERIODICITY]);

	wpa_printf(MSG_DEBUG,
		   "mbssid: max interfaces %u, max profile periodicity %u",
		   cap->mbssid_max_interfaces, cap->ema_max_periodicity);
}


static int wiphy_info_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct wiphy_info_data *info = arg;
	struct wpa_driver_capa *capa = info->capa;
	struct wpa_driver_nl80211_data *drv = info->drv;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_WIPHY])
		drv->wiphy_idx = nla_get_u32(tb[NL80211_ATTR_WIPHY]);

	if (tb[NL80211_ATTR_WIPHY_NAME])
		os_strlcpy(drv->phyname,
			   nla_get_string(tb[NL80211_ATTR_WIPHY_NAME]),
			   sizeof(drv->phyname));
	if (tb[NL80211_ATTR_MAX_NUM_SCAN_SSIDS])
		capa->max_scan_ssids =
			nla_get_u8(tb[NL80211_ATTR_MAX_NUM_SCAN_SSIDS]);

	if (tb[NL80211_ATTR_MAX_NUM_SCHED_SCAN_SSIDS])
		capa->max_sched_scan_ssids =
			nla_get_u8(tb[NL80211_ATTR_MAX_NUM_SCHED_SCAN_SSIDS]);

	if (tb[NL80211_ATTR_MAX_NUM_SCHED_SCAN_PLANS] &&
	    tb[NL80211_ATTR_MAX_SCAN_PLAN_INTERVAL] &&
	    tb[NL80211_ATTR_MAX_SCAN_PLAN_ITERATIONS]) {
		capa->max_sched_scan_plans =
			nla_get_u32(tb[NL80211_ATTR_MAX_NUM_SCHED_SCAN_PLANS]);

		capa->max_sched_scan_plan_interval =
			nla_get_u32(tb[NL80211_ATTR_MAX_SCAN_PLAN_INTERVAL]);

		capa->max_sched_scan_plan_iterations =
			nla_get_u32(tb[NL80211_ATTR_MAX_SCAN_PLAN_ITERATIONS]);
	}

	if (tb[NL80211_ATTR_MAX_MATCH_SETS])
		capa->max_match_sets =
			nla_get_u8(tb[NL80211_ATTR_MAX_MATCH_SETS]);

	if (tb[NL80211_ATTR_MAC_ACL_MAX])
		capa->max_acl_mac_addrs =
			nla_get_u32(tb[NL80211_ATTR_MAC_ACL_MAX]);

	wiphy_info_supported_iftypes(info, tb[NL80211_ATTR_SUPPORTED_IFTYPES]);
	wiphy_info_iface_comb(info, tb[NL80211_ATTR_INTERFACE_COMBINATIONS]);
	wiphy_info_supp_cmds(info, tb[NL80211_ATTR_SUPPORTED_COMMANDS]);
	wiphy_info_cipher_suites(info, tb[NL80211_ATTR_CIPHER_SUITES]);
	wiphy_info_akm_suites(info, tb[NL80211_ATTR_AKM_SUITES]);
	wiphy_info_iftype_akm_suites(info, tb[NL80211_ATTR_IFTYPE_AKM_SUITES]);

	if (tb[NL80211_ATTR_OFFCHANNEL_TX_OK]) {
		wpa_printf(MSG_DEBUG, "nl80211: Using driver-based "
			   "off-channel TX");
		capa->flags |= WPA_DRIVER_FLAGS_OFFCHANNEL_TX;
	}

	if (tb[NL80211_ATTR_ROAM_SUPPORT]) {
		wpa_printf(MSG_DEBUG, "nl80211: Using driver-based roaming");
		capa->flags |= WPA_DRIVER_FLAGS_BSS_SELECTION;
	}

	wiphy_info_max_roc(capa,
			   tb[NL80211_ATTR_MAX_REMAIN_ON_CHANNEL_DURATION]);

	if (tb[NL80211_ATTR_SUPPORT_AP_UAPSD])
		capa->flags |= WPA_DRIVER_FLAGS_AP_UAPSD;

	wiphy_info_tdls(capa, tb[NL80211_ATTR_TDLS_SUPPORT],
			tb[NL80211_ATTR_TDLS_EXTERNAL_SETUP]);

	if (tb[NL80211_ATTR_DEVICE_AP_SME]) {
		u32 ap_sme_features_flags =
			nla_get_u32(tb[NL80211_ATTR_DEVICE_AP_SME]);

		if (ap_sme_features_flags & NL80211_AP_SME_SA_QUERY_OFFLOAD)
			capa->flags2 |= WPA_DRIVER_FLAGS2_SA_QUERY_OFFLOAD_AP;

		info->device_ap_sme = 1;
	}

	wiphy_info_feature_flags(info, tb[NL80211_ATTR_FEATURE_FLAGS]);
	wiphy_info_ext_feature_flags(info, tb[NL80211_ATTR_EXT_FEATURES]);
	wiphy_info_probe_resp_offload(capa,
				      tb[NL80211_ATTR_PROBE_RESP_OFFLOAD]);

	if (tb[NL80211_ATTR_EXT_CAPA] && tb[NL80211_ATTR_EXT_CAPA_MASK] &&
	    drv->extended_capa == NULL) {
		drv->extended_capa =
			os_malloc(nla_len(tb[NL80211_ATTR_EXT_CAPA]));
		if (drv->extended_capa) {
			os_memcpy(drv->extended_capa,
				  nla_data(tb[NL80211_ATTR_EXT_CAPA]),
				  nla_len(tb[NL80211_ATTR_EXT_CAPA]));
			drv->extended_capa_len =
				nla_len(tb[NL80211_ATTR_EXT_CAPA]);
			wpa_hexdump(MSG_DEBUG,
				    "nl80211: Driver-advertised extended capabilities (default)",
				    drv->extended_capa, drv->extended_capa_len);
		}
		drv->extended_capa_mask =
			os_malloc(nla_len(tb[NL80211_ATTR_EXT_CAPA_MASK]));
		if (drv->extended_capa_mask) {
			os_memcpy(drv->extended_capa_mask,
				  nla_data(tb[NL80211_ATTR_EXT_CAPA_MASK]),
				  nla_len(tb[NL80211_ATTR_EXT_CAPA_MASK]));
			wpa_hexdump(MSG_DEBUG,
				    "nl80211: Driver-advertised extended capabilities mask (default)",
				    drv->extended_capa_mask,
				    drv->extended_capa_len);
		} else {
			os_free(drv->extended_capa);
			drv->extended_capa = NULL;
			drv->extended_capa_len = 0;
		}
	}

	wiphy_info_extended_capab(drv, tb[NL80211_ATTR_IFTYPE_EXT_CAPA]);

	if (tb[NL80211_ATTR_VENDOR_DATA]) {
		struct nlattr *nl;
		int rem;

		nla_for_each_nested(nl, tb[NL80211_ATTR_VENDOR_DATA], rem) {
			struct nl80211_vendor_cmd_info *vinfo;
			if (nla_len(nl) != sizeof(*vinfo)) {
				wpa_printf(MSG_DEBUG, "nl80211: Unexpected vendor data info");
				continue;
			}
			vinfo = nla_data(nl);
			if (vinfo->vendor_id == OUI_QCA) {
				switch (vinfo->subcmd) {
				case QCA_NL80211_VENDOR_SUBCMD_TEST:
					drv->vendor_cmd_test_avail = 1;
					break;
#ifdef CONFIG_DRIVER_NL80211_QCA
				case QCA_NL80211_VENDOR_SUBCMD_ROAMING:
					drv->roaming_vendor_cmd_avail = 1;
					break;
				case QCA_NL80211_VENDOR_SUBCMD_DFS_CAPABILITY:
					drv->dfs_vendor_cmd_avail = 1;
					break;
				case QCA_NL80211_VENDOR_SUBCMD_GET_FEATURES:
					drv->get_features_vendor_cmd_avail = 1;
					break;
				case QCA_NL80211_VENDOR_SUBCMD_GET_PREFERRED_FREQ_LIST:
					drv->get_pref_freq_list = 1;
					break;
				case QCA_NL80211_VENDOR_SUBCMD_SET_PROBABLE_OPER_CHANNEL:
					drv->set_prob_oper_freq = 1;
					break;
				case QCA_NL80211_VENDOR_SUBCMD_DO_ACS:
					drv->capa.flags |=
						WPA_DRIVER_FLAGS_ACS_OFFLOAD;
					drv->qca_do_acs = 1;
					break;
				case QCA_NL80211_VENDOR_SUBCMD_SETBAND:
					drv->setband_vendor_cmd_avail = 1;
					break;
				case QCA_NL80211_VENDOR_SUBCMD_TRIGGER_SCAN:
					drv->scan_vendor_cmd_avail = 1;
					break;
				case QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION:
					drv->set_wifi_conf_vendor_cmd_avail = 1;
					break;
				case QCA_NL80211_VENDOR_SUBCMD_FETCH_BSS_TRANSITION_STATUS:
					drv->fetch_bss_trans_status = 1;
					break;
				case QCA_NL80211_VENDOR_SUBCMD_ROAM:
					drv->roam_vendor_cmd_avail = 1;
					break;
				case QCA_NL80211_VENDOR_SUBCMD_ADD_STA_NODE:
					drv->add_sta_node_vendor_cmd_avail = 1;
					break;
				case QCA_NL80211_VENDOR_SUBCMD_GET_STA_INFO:
					drv->get_sta_info_vendor_cmd_avail = 1;
					break;
				case QCA_NL80211_VENDOR_SUBCMD_SECURE_RANGING_CONTEXT:
					drv->secure_ranging_ctx_vendor_cmd_avail = 1;
					break;
#endif /* CONFIG_DRIVER_NL80211_QCA */
				}
#ifdef CONFIG_DRIVER_NL80211_BRCM
			} else if (vinfo->vendor_id == OUI_BRCM) {
				switch (vinfo->subcmd) {
				case BRCM_VENDOR_SCMD_ACS:
					drv->capa.flags |=
						WPA_DRIVER_FLAGS_ACS_OFFLOAD;
					wpa_printf(MSG_DEBUG,
						   "Enabled BRCM ACS");
					drv->brcm_do_acs = 1;
					break;
				}
#endif /* CONFIG_DRIVER_NL80211_BRCM */
			}

			wpa_printf(MSG_DEBUG, "nl80211: Supported vendor command: vendor_id=0x%x subcmd=%u",
				   vinfo->vendor_id, vinfo->subcmd);
		}
	}

	if (tb[NL80211_ATTR_VENDOR_EVENTS]) {
		struct nlattr *nl;
		int rem;

		nla_for_each_nested(nl, tb[NL80211_ATTR_VENDOR_EVENTS], rem) {
			struct nl80211_vendor_cmd_info *vinfo;
			if (nla_len(nl) != sizeof(*vinfo)) {
				wpa_printf(MSG_DEBUG, "nl80211: Unexpected vendor data info");
				continue;
			}
			vinfo = nla_data(nl);
			wpa_printf(MSG_DEBUG, "nl80211: Supported vendor event: vendor_id=0x%x subcmd=%u",
				   vinfo->vendor_id, vinfo->subcmd);
		}
	}

	wiphy_info_wowlan_triggers(capa,
				   tb[NL80211_ATTR_WOWLAN_TRIGGERS_SUPPORTED]);

	if (tb[NL80211_ATTR_MAX_AP_ASSOC_STA])
		capa->max_stations =
			nla_get_u32(tb[NL80211_ATTR_MAX_AP_ASSOC_STA]);

	if (tb[NL80211_ATTR_MAX_CSA_COUNTERS])
		capa->max_csa_counters =
			nla_get_u8(tb[NL80211_ATTR_MAX_CSA_COUNTERS]);

	if (tb[NL80211_ATTR_WIPHY_SELF_MANAGED_REG])
		capa->flags |= WPA_DRIVER_FLAGS_SELF_MANAGED_REGULATORY;

	if (tb[NL80211_ATTR_MAX_NUM_AKM_SUITES])
		capa->max_num_akms =
			nla_get_u16(tb[NL80211_ATTR_MAX_NUM_AKM_SUITES]);

	if (tb[NL80211_ATTR_MBSSID_CONFIG])
		wiphy_info_mbssid(capa, tb[NL80211_ATTR_MBSSID_CONFIG]);

	if (tb[NL80211_ATTR_MLO_SUPPORT])
		capa->flags2 |= WPA_DRIVER_FLAGS2_MLO;

	return NL_SKIP;
}


static int wpa_driver_nl80211_get_info(struct wpa_driver_nl80211_data *drv,
				       struct wiphy_info_data *info)
{
	u32 feat;
	struct nl_msg *msg;
	int flags = 0;

	os_memset(info, 0, sizeof(*info));
	info->capa = &drv->capa;
	info->drv = drv;

	feat = get_nl80211_protocol_features(drv);
	if (feat & NL80211_PROTOCOL_FEATURE_SPLIT_WIPHY_DUMP)
		flags = NLM_F_DUMP;
	msg = nl80211_cmd_msg(drv->first_bss, flags, NL80211_CMD_GET_WIPHY);
	if (!msg || nla_put_flag(msg, NL80211_ATTR_SPLIT_WIPHY_DUMP)) {
		nlmsg_free(msg);
		return -1;
	}

	if (send_and_recv_resp(drv, msg, wiphy_info_handler, info))
		return -1;

	if (info->auth_supported)
		drv->capa.flags |= WPA_DRIVER_FLAGS_SME;
	else if (!info->connect_supported) {
		wpa_printf(MSG_INFO, "nl80211: Driver does not support "
			   "authentication/association or connect commands");
		info->error = 1;
	}

	if (info->p2p_go_supported && info->p2p_client_supported)
		drv->capa.flags |= WPA_DRIVER_FLAGS_P2P_CAPABLE;
	if (info->p2p_concurrent) {
		wpa_printf(MSG_DEBUG, "nl80211: Use separate P2P group "
			   "interface (driver advertised support)");
		drv->capa.flags |= WPA_DRIVER_FLAGS_P2P_CONCURRENT;
		drv->capa.flags |= WPA_DRIVER_FLAGS_P2P_MGMT_AND_NON_P2P;
	}
	if (info->num_multichan_concurrent > 1) {
		wpa_printf(MSG_DEBUG, "nl80211: Enable multi-channel "
			   "concurrent (driver advertised support)");
		drv->capa.num_multichan_concurrent =
			info->num_multichan_concurrent;
	}
	if (drv->capa.flags & WPA_DRIVER_FLAGS_DEDICATED_P2P_DEVICE)
		wpa_printf(MSG_DEBUG, "nl80211: use P2P_DEVICE support");

	/* default to 5000 since early versions of mac80211 don't set it */
	if (!drv->capa.max_remain_on_chan)
		drv->capa.max_remain_on_chan = 5000;

	drv->capa.wmm_ac_supported = info->wmm_ac_supported;

	drv->capa.mac_addr_rand_sched_scan_supported =
		info->mac_addr_rand_sched_scan_supported;
	drv->capa.mac_addr_rand_scan_supported =
		info->mac_addr_rand_scan_supported;

	if (info->channel_switch_supported) {
		drv->capa.flags |= WPA_DRIVER_FLAGS_AP_CSA;
		if (!drv->capa.max_csa_counters)
			drv->capa.max_csa_counters = 1;
	}

	if (!drv->capa.max_sched_scan_plans) {
		drv->capa.max_sched_scan_plans = 1;
		drv->capa.max_sched_scan_plan_interval = UINT32_MAX;
		drv->capa.max_sched_scan_plan_iterations = 0;
	}

	if (info->update_ft_ies_supported)
		drv->capa.flags |= WPA_DRIVER_FLAGS_UPDATE_FT_IES;

	if (!drv->capa.max_num_akms)
		drv->capa.max_num_akms = NL80211_MAX_NR_AKM_SUITES;

	return 0;
}


#ifdef CONFIG_DRIVER_NL80211_QCA

static int dfs_info_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	int *dfs_capability_ptr = arg;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (tb[NL80211_ATTR_VENDOR_DATA]) {
		struct nlattr *nl_vend = tb[NL80211_ATTR_VENDOR_DATA];
		struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_MAX + 1];

		nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_MAX,
			  nla_data(nl_vend), nla_len(nl_vend), NULL);

		if (tb_vendor[QCA_WLAN_VENDOR_ATTR_DFS]) {
			u32 val;
			val = nla_get_u32(tb_vendor[QCA_WLAN_VENDOR_ATTR_DFS]);
			wpa_printf(MSG_DEBUG, "nl80211: DFS offload capability: %u",
				   val);
			*dfs_capability_ptr = val;
		}
	}

	return NL_SKIP;
}


static void qca_nl80211_check_dfs_capa(struct wpa_driver_nl80211_data *drv)
{
	struct nl_msg *msg;
	int dfs_capability = 0;
	int ret;

	if (!drv->dfs_vendor_cmd_avail)
		return;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_DFS_CAPABILITY)) {
		nlmsg_free(msg);
		return;
	}

	ret = send_and_recv_resp(drv, msg, dfs_info_handler, &dfs_capability);
	if (!ret && dfs_capability)
		drv->capa.flags |= WPA_DRIVER_FLAGS_DFS_OFFLOAD;
}


struct features_info {
	u8 *flags;
	size_t flags_len;
	struct wpa_driver_capa *capa;
};


static int features_info_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct features_info *info = arg;
	struct nlattr *nl_vend, *attr;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	nl_vend = tb[NL80211_ATTR_VENDOR_DATA];
	if (nl_vend) {
		struct nlattr *tb_vendor[QCA_WLAN_VENDOR_ATTR_MAX + 1];

		nla_parse(tb_vendor, QCA_WLAN_VENDOR_ATTR_MAX,
			  nla_data(nl_vend), nla_len(nl_vend), NULL);

		attr = tb_vendor[QCA_WLAN_VENDOR_ATTR_FEATURE_FLAGS];
		if (attr) {
			int len = nla_len(attr);
			info->flags = os_malloc(len);
			if (info->flags != NULL) {
				os_memcpy(info->flags, nla_data(attr), len);
				info->flags_len = len;
			}
		}
		attr = tb_vendor[QCA_WLAN_VENDOR_ATTR_CONCURRENCY_CAPA];
		if (attr)
			info->capa->conc_capab = nla_get_u32(attr);

		attr = tb_vendor[
			QCA_WLAN_VENDOR_ATTR_MAX_CONCURRENT_CHANNELS_2_4_BAND];
		if (attr)
			info->capa->max_conc_chan_2_4 = nla_get_u32(attr);

		attr = tb_vendor[
			QCA_WLAN_VENDOR_ATTR_MAX_CONCURRENT_CHANNELS_5_0_BAND];
		if (attr)
			info->capa->max_conc_chan_5_0 = nla_get_u32(attr);
	}

	return NL_SKIP;
}


static int check_feature(enum qca_wlan_vendor_features feature,
			 struct features_info *info)
{
	size_t idx = feature / 8;

	return (idx < info->flags_len) &&
		(info->flags[idx] & BIT(feature % 8));
}


static void qca_nl80211_get_features(struct wpa_driver_nl80211_data *drv)
{
	struct nl_msg *msg;
	struct features_info info;
	int ret;

	if (!drv->get_features_vendor_cmd_avail)
		return;

	if (!(msg = nl80211_drv_msg(drv, 0, NL80211_CMD_VENDOR)) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_ID, OUI_QCA) ||
	    nla_put_u32(msg, NL80211_ATTR_VENDOR_SUBCMD,
			QCA_NL80211_VENDOR_SUBCMD_GET_FEATURES)) {
		nlmsg_free(msg);
		return;
	}

	os_memset(&info, 0, sizeof(info));
	info.capa = &drv->capa;
	ret = send_and_recv_resp(drv, msg, features_info_handler, &info);
	if (ret || !info.flags)
		return;

	if (check_feature(QCA_WLAN_VENDOR_FEATURE_KEY_MGMT_OFFLOAD, &info))
		drv->capa.flags |= WPA_DRIVER_FLAGS_KEY_MGMT_OFFLOAD;

	if (check_feature(QCA_WLAN_VENDOR_FEATURE_SUPPORT_HW_MODE_ANY, &info))
		drv->capa.flags |= WPA_DRIVER_FLAGS_SUPPORT_HW_MODE_ANY;

	if (check_feature(QCA_WLAN_VENDOR_FEATURE_OFFCHANNEL_SIMULTANEOUS,
			  &info))
		drv->capa.flags |= WPA_DRIVER_FLAGS_OFFCHANNEL_SIMULTANEOUS;
	if (check_feature(QCA_WLAN_VENDOR_FEATURE_P2P_LISTEN_OFFLOAD, &info))
		drv->capa.flags |= WPA_DRIVER_FLAGS_P2P_LISTEN_OFFLOAD;
	if (check_feature(QCA_WLAN_VENDOR_FEATURE_OCE_STA, &info))
		drv->capa.flags |= WPA_DRIVER_FLAGS_OCE_STA;
	if (check_feature(QCA_WLAN_VENDOR_FEATURE_OCE_AP, &info))
		drv->capa.flags |= WPA_DRIVER_FLAGS_OCE_AP;
	if (check_feature(QCA_WLAN_VENDOR_FEATURE_OCE_STA_CFON, &info))
		drv->capa.flags |= WPA_DRIVER_FLAGS_OCE_STA_CFON;
	if (check_feature(QCA_WLAN_VENDOR_FEATURE_SECURE_LTF_STA, &info))
		drv->capa.flags2 |= WPA_DRIVER_FLAGS2_SEC_LTF_STA;
	if (check_feature(QCA_WLAN_VENDOR_FEATURE_SECURE_LTF_AP, &info))
		drv->capa.flags2 |= WPA_DRIVER_FLAGS2_SEC_LTF_AP;
	if (check_feature(QCA_WLAN_VENDOR_FEATURE_SECURE_RTT_STA, &info))
		drv->capa.flags2 |= WPA_DRIVER_FLAGS2_SEC_RTT_STA;
	if (check_feature(QCA_WLAN_VENDOR_FEATURE_SECURE_RTT_AP, &info))
		drv->capa.flags2 |= WPA_DRIVER_FLAGS2_SEC_RTT_AP;
	if (check_feature(
		    QCA_WLAN_VENDOR_FEATURE_PROT_RANGE_NEGO_AND_MEASURE_STA,
		    &info))
		drv->capa.flags2 |= WPA_DRIVER_FLAGS2_PROT_RANGE_NEG_STA;
	if (check_feature(
		    QCA_WLAN_VENDOR_FEATURE_PROT_RANGE_NEGO_AND_MEASURE_AP,
		    &info))
		drv->capa.flags2 |= WPA_DRIVER_FLAGS2_PROT_RANGE_NEG_AP;
	if (check_feature(QCA_WLAN_VENDOR_FEATURE_AP_ALLOWED_FREQ_LIST,
			  &info))
		drv->qca_ap_allowed_freqs = 1;
	if (check_feature(QCA_WLAN_VENDOR_FEATURE_HT_VHT_TWT_RESPONDER, &info))
		drv->capa.flags2 |= WPA_DRIVER_FLAGS2_HT_VHT_TWT_RESPONDER;
	os_free(info.flags);
}

#endif /* CONFIG_DRIVER_NL80211_QCA */


int wpa_driver_nl80211_capa(struct wpa_driver_nl80211_data *drv)
{
	struct wiphy_info_data info;
	int i;

	if (wpa_driver_nl80211_get_info(drv, &info))
		return -1;

	if (info.error)
		return -1;

	drv->has_capability = 1;
	drv->has_driver_key_mgmt = info.has_key_mgmt | info.has_key_mgmt_iftype;

	/* Fallback to hardcoded defaults if the driver does not advertise any
	 * AKM capabilities. */
	if (!drv->has_driver_key_mgmt) {
		drv->capa.key_mgmt = WPA_DRIVER_CAPA_KEY_MGMT_WPA |
			WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK |
			WPA_DRIVER_CAPA_KEY_MGMT_WPA2 |
			WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK |
			WPA_DRIVER_CAPA_KEY_MGMT_SUITE_B |
			WPA_DRIVER_CAPA_KEY_MGMT_OWE |
			WPA_DRIVER_CAPA_KEY_MGMT_DPP;

		if (drv->capa.enc & (WPA_DRIVER_CAPA_ENC_CCMP_256 |
				     WPA_DRIVER_CAPA_ENC_GCMP_256))
			drv->capa.key_mgmt |=
				WPA_DRIVER_CAPA_KEY_MGMT_SUITE_B_192;

		if (drv->capa.flags & WPA_DRIVER_FLAGS_SME)
			drv->capa.key_mgmt |=
				WPA_DRIVER_CAPA_KEY_MGMT_FILS_SHA256 |
				WPA_DRIVER_CAPA_KEY_MGMT_FILS_SHA384 |
				WPA_DRIVER_CAPA_KEY_MGMT_FT_FILS_SHA256 |
				WPA_DRIVER_CAPA_KEY_MGMT_FT_FILS_SHA384 |
				WPA_DRIVER_CAPA_KEY_MGMT_SAE;
		else if (drv->capa.flags & WPA_DRIVER_FLAGS_FILS_SK_OFFLOAD)
			drv->capa.key_mgmt |=
				WPA_DRIVER_CAPA_KEY_MGMT_FILS_SHA256 |
				WPA_DRIVER_CAPA_KEY_MGMT_FILS_SHA384;
	}

	if (!info.has_key_mgmt_iftype) {
		/* If the driver does not advertize per interface AKM
		 * capabilities, consider all interfaces to support default AKMs
		 * in key_mgmt. */
		for (i = 0; i < WPA_IF_MAX; i++)
			drv->capa.key_mgmt_iftype[i] = drv->capa.key_mgmt;
	} else if (info.has_key_mgmt_iftype && !info.has_key_mgmt) {
		/* If the driver advertizes only per interface supported AKMs
		 * but does not advertize per wiphy AKM capabilities, consider
		 * the default key_mgmt as a mask of per interface supported
		 * AKMs. */
		drv->capa.key_mgmt = 0;
		for (i = 0; i < WPA_IF_MAX; i++)
			drv->capa.key_mgmt |= drv->capa.key_mgmt_iftype[i];
	} else if (info.has_key_mgmt_iftype && info.has_key_mgmt) {
		/* If the driver advertizes AKM capabilities both per wiphy and
		 * per interface, consider the interfaces for which per
		 * interface AKM capabilities were not received to support the
		 * default key_mgmt capabilities.
		 */
		for (i = 0; i < WPA_IF_MAX; i++)
			if (!drv->capa.key_mgmt_iftype[i])
				drv->capa.key_mgmt_iftype[i] =
					drv->capa.key_mgmt;
	}

	drv->capa.auth = WPA_DRIVER_AUTH_OPEN |
		WPA_DRIVER_AUTH_SHARED |
		WPA_DRIVER_AUTH_LEAP;

	drv->capa.flags |= WPA_DRIVER_FLAGS_VALID_ERROR_CODES;
	drv->capa.flags |= WPA_DRIVER_FLAGS_SET_KEYS_AFTER_ASSOC_DONE;
	drv->capa.flags |= WPA_DRIVER_FLAGS_EAPOL_TX_STATUS;

	/*
	 * As all cfg80211 drivers must support cases where the AP interface is
	 * removed without the knowledge of wpa_supplicant/hostapd, e.g., in
	 * case that the user space daemon has crashed, they must be able to
	 * cleanup all stations and key entries in the AP tear down flow. Thus,
	 * this flag can/should always be set for cfg80211 drivers.
	 */
	drv->capa.flags |= WPA_DRIVER_FLAGS_AP_TEARDOWN_SUPPORT;

	if (!info.device_ap_sme) {
		drv->capa.flags |= WPA_DRIVER_FLAGS_DEAUTH_TX_STATUS;
		drv->capa.flags2 |= WPA_DRIVER_FLAGS2_AP_SME;

		/*
		 * No AP SME is currently assumed to also indicate no AP MLME
		 * in the driver/firmware.
		 */
		drv->capa.flags |= WPA_DRIVER_FLAGS_AP_MLME;
	}

	drv->device_ap_sme = info.device_ap_sme;
	drv->poll_command_supported = info.poll_command_supported;
	drv->data_tx_status = info.data_tx_status;
	drv->p2p_go_ctwindow_supported = info.p2p_go_ctwindow_supported;
	if (info.set_qos_map_supported)
		drv->capa.flags |= WPA_DRIVER_FLAGS_QOS_MAPPING;
	drv->have_low_prio_scan = info.have_low_prio_scan;

	/*
	 * If poll command and tx status are supported, mac80211 is new enough
	 * to have everything we need to not need monitor interfaces.
	 */
	drv->use_monitor = !info.device_ap_sme &&
		(!info.poll_command_supported || !info.data_tx_status);

	/*
	 * If we aren't going to use monitor interfaces, but the
	 * driver doesn't support data TX status, we won't get TX
	 * status for EAPOL frames.
	 */
	if (!drv->use_monitor && !info.data_tx_status)
		drv->capa.flags &= ~WPA_DRIVER_FLAGS_EAPOL_TX_STATUS;

#ifdef CONFIG_DRIVER_NL80211_QCA
	if (!(info.capa->flags & WPA_DRIVER_FLAGS_DFS_OFFLOAD))
		qca_nl80211_check_dfs_capa(drv);
	qca_nl80211_get_features(drv);

	/*
	 * To enable offchannel simultaneous support in wpa_supplicant, the
	 * underlying driver needs to support the same along with offchannel TX.
	 * Offchannel TX support is needed since remain_on_channel and
	 * action_tx use some common data structures and hence cannot be
	 * scheduled simultaneously.
	 */
	if (!(drv->capa.flags & WPA_DRIVER_FLAGS_OFFCHANNEL_TX))
		drv->capa.flags &= ~WPA_DRIVER_FLAGS_OFFCHANNEL_SIMULTANEOUS;
#endif /* CONFIG_DRIVER_NL80211_QCA */

	wpa_printf(MSG_DEBUG,
		   "nl80211: key_mgmt=0x%x enc=0x%x auth=0x%x flags=0x%llx flags2=0x%llx rrm_flags=0x%x probe_resp_offloads=0x%x max_stations=%u max_remain_on_chan=%u max_scan_ssids=%d",
		   drv->capa.key_mgmt, drv->capa.enc, drv->capa.auth,
		   (unsigned long long) drv->capa.flags,
		   (unsigned long long) drv->capa.flags2, drv->capa.rrm_flags,
		   drv->capa.probe_resp_offloads, drv->capa.max_stations,
		   drv->capa.max_remain_on_chan, drv->capa.max_scan_ssids);
	return 0;
}


struct phy_info_arg {
	u16 *num_modes;
	struct hostapd_hw_modes *modes;
	int last_mode, last_chan_idx;
	int failed;
	u8 dfs_domain;
};

static void phy_info_ht_capa(struct hostapd_hw_modes *mode, struct nlattr *capa,
			     struct nlattr *ampdu_factor,
			     struct nlattr *ampdu_density,
			     struct nlattr *mcs_set)
{
	if (capa)
		mode->ht_capab = nla_get_u16(capa);

	if (ampdu_factor)
		mode->a_mpdu_params |= nla_get_u8(ampdu_factor) & 0x03;

	if (ampdu_density)
		mode->a_mpdu_params |= nla_get_u8(ampdu_density) << 2;

	if (mcs_set && nla_len(mcs_set) >= 16) {
		u8 *mcs;
		mcs = nla_data(mcs_set);
		os_memcpy(mode->mcs_set, mcs, 16);
	}
}


static void phy_info_vht_capa(struct hostapd_hw_modes *mode,
			      struct nlattr *capa,
			      struct nlattr *mcs_set)
{
	if (capa)
		mode->vht_capab = nla_get_u32(capa);

	if (mcs_set && nla_len(mcs_set) >= 8) {
		u8 *mcs;
		mcs = nla_data(mcs_set);
		os_memcpy(mode->vht_mcs_set, mcs, 8);
	}
}


static int phy_info_edmg_capa(struct hostapd_hw_modes *mode,
			      struct nlattr *bw_config,
			      struct nlattr *channels)
{
	if (!bw_config || !channels)
		return NL_OK;

	mode->edmg.bw_config = nla_get_u8(bw_config);
	mode->edmg.channels = nla_get_u8(channels);

	if (!mode->edmg.channels || !mode->edmg.bw_config)
		return NL_STOP;

	return NL_OK;
}


static int cw2ecw(unsigned int cw)
{
	int bit;

	if (cw == 0)
		return 0;

	for (bit = 1; cw != 1; bit++)
		cw >>= 1;

	return bit;
}


static void phy_info_freq(struct hostapd_hw_modes *mode,
			  struct hostapd_channel_data *chan,
			  struct nlattr *tb_freq[])
{
	u8 channel;

	os_memset(chan, 0, sizeof(*chan));
	chan->freq = nla_get_u32(tb_freq[NL80211_FREQUENCY_ATTR_FREQ]);
	chan->flag = 0;
	chan->allowed_bw = ~0;
	chan->dfs_cac_ms = 0;
	if (ieee80211_freq_to_chan(chan->freq, &channel) != NUM_HOSTAPD_MODES)
		chan->chan = channel;
	else
		wpa_printf(MSG_DEBUG,
			   "nl80211: No channel number found for frequency %u MHz",
			   chan->freq);

	if (tb_freq[NL80211_FREQUENCY_ATTR_DISABLED])
		chan->flag |= HOSTAPD_CHAN_DISABLED;
	if (tb_freq[NL80211_FREQUENCY_ATTR_NO_IR])
		chan->flag |= HOSTAPD_CHAN_NO_IR;
	if (tb_freq[NL80211_FREQUENCY_ATTR_RADAR])
		chan->flag |= HOSTAPD_CHAN_RADAR;
	if (tb_freq[NL80211_FREQUENCY_ATTR_INDOOR_ONLY])
		chan->flag |= HOSTAPD_CHAN_INDOOR_ONLY;
	if (tb_freq[NL80211_FREQUENCY_ATTR_GO_CONCURRENT])
		chan->flag |= HOSTAPD_CHAN_GO_CONCURRENT;

	if (tb_freq[NL80211_FREQUENCY_ATTR_NO_10MHZ])
		chan->allowed_bw &= ~HOSTAPD_CHAN_WIDTH_10;
	if (tb_freq[NL80211_FREQUENCY_ATTR_NO_20MHZ])
		chan->allowed_bw &= ~HOSTAPD_CHAN_WIDTH_20;
	if (tb_freq[NL80211_FREQUENCY_ATTR_NO_HT40_PLUS])
		chan->allowed_bw &= ~HOSTAPD_CHAN_WIDTH_40P;
	if (tb_freq[NL80211_FREQUENCY_ATTR_NO_HT40_MINUS])
		chan->allowed_bw &= ~HOSTAPD_CHAN_WIDTH_40M;
	if (tb_freq[NL80211_FREQUENCY_ATTR_NO_80MHZ])
		chan->allowed_bw &= ~HOSTAPD_CHAN_WIDTH_80;
	if (tb_freq[NL80211_FREQUENCY_ATTR_NO_160MHZ])
		chan->allowed_bw &= ~HOSTAPD_CHAN_WIDTH_160;

	if (tb_freq[NL80211_FREQUENCY_ATTR_DFS_STATE]) {
		enum nl80211_dfs_state state =
			nla_get_u32(tb_freq[NL80211_FREQUENCY_ATTR_DFS_STATE]);

		switch (state) {
		case NL80211_DFS_USABLE:
			chan->flag |= HOSTAPD_CHAN_DFS_USABLE;
			break;
		case NL80211_DFS_AVAILABLE:
			chan->flag |= HOSTAPD_CHAN_DFS_AVAILABLE;
			break;
		case NL80211_DFS_UNAVAILABLE:
			chan->flag |= HOSTAPD_CHAN_DFS_UNAVAILABLE;
			break;
		}
	}

	if (tb_freq[NL80211_FREQUENCY_ATTR_DFS_CAC_TIME]) {
		chan->dfs_cac_ms = nla_get_u32(
			tb_freq[NL80211_FREQUENCY_ATTR_DFS_CAC_TIME]);
	}

	chan->wmm_rules_valid = 0;
	if (tb_freq[NL80211_FREQUENCY_ATTR_WMM]) {
		static struct nla_policy wmm_policy[NL80211_WMMR_MAX + 1] = {
			[NL80211_WMMR_CW_MIN] = { .type = NLA_U16 },
			[NL80211_WMMR_CW_MAX] = { .type = NLA_U16 },
			[NL80211_WMMR_AIFSN] = { .type = NLA_U8 },
			[NL80211_WMMR_TXOP] = { .type = NLA_U16 },
		};
		static const u8 wmm_map[4] = {
			[NL80211_AC_BE] = WMM_AC_BE,
			[NL80211_AC_BK] = WMM_AC_BK,
			[NL80211_AC_VI] = WMM_AC_VI,
			[NL80211_AC_VO] = WMM_AC_VO,
		};
		struct nlattr *nl_wmm;
		struct nlattr *tb_wmm[NL80211_WMMR_MAX + 1];
		int rem_wmm, ac, count = 0;

		nla_for_each_nested(nl_wmm, tb_freq[NL80211_FREQUENCY_ATTR_WMM],
				    rem_wmm) {
			if (nla_parse_nested(tb_wmm, NL80211_WMMR_MAX, nl_wmm,
					     wmm_policy)) {
				wpa_printf(MSG_DEBUG,
					   "nl80211: Failed to parse WMM rules attribute");
				return;
			}
			if (!tb_wmm[NL80211_WMMR_CW_MIN] ||
			    !tb_wmm[NL80211_WMMR_CW_MAX] ||
			    !tb_wmm[NL80211_WMMR_AIFSN] ||
			    !tb_wmm[NL80211_WMMR_TXOP]) {
				wpa_printf(MSG_DEBUG,
					   "nl80211: Channel is missing WMM rule attribute");
				return;
			}
			ac = nl_wmm->nla_type;
			if ((unsigned int) ac >= ARRAY_SIZE(wmm_map)) {
				wpa_printf(MSG_DEBUG,
					   "nl80211: Invalid AC value %d", ac);
				return;
			}

			ac = wmm_map[ac];
			chan->wmm_rules[ac].min_cwmin =
				cw2ecw(nla_get_u16(
					       tb_wmm[NL80211_WMMR_CW_MIN]));
			chan->wmm_rules[ac].min_cwmax =
				cw2ecw(nla_get_u16(
					       tb_wmm[NL80211_WMMR_CW_MAX]));
			chan->wmm_rules[ac].min_aifs =
				nla_get_u8(tb_wmm[NL80211_WMMR_AIFSN]);
			chan->wmm_rules[ac].max_txop =
				nla_get_u16(tb_wmm[NL80211_WMMR_TXOP]) / 32;
			count++;
		}

		/* Set valid flag if all the AC rules are present */
		if (count == WMM_AC_NUM)
			chan->wmm_rules_valid = 1;
	}
}


static int phy_info_freqs(struct phy_info_arg *phy_info,
			  struct hostapd_hw_modes *mode, struct nlattr *tb)
{
	static struct nla_policy freq_policy[NL80211_FREQUENCY_ATTR_MAX + 1] = {
		[NL80211_FREQUENCY_ATTR_FREQ] = { .type = NLA_U32 },
		[NL80211_FREQUENCY_ATTR_DISABLED] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_NO_IR] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_RADAR] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_MAX_TX_POWER] = { .type = NLA_U32 },
		[NL80211_FREQUENCY_ATTR_DFS_STATE] = { .type = NLA_U32 },
		[NL80211_FREQUENCY_ATTR_NO_10MHZ] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_NO_20MHZ] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_NO_HT40_PLUS] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_NO_HT40_MINUS] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_NO_80MHZ] = { .type = NLA_FLAG },
		[NL80211_FREQUENCY_ATTR_NO_160MHZ] = { .type = NLA_FLAG },
	};
	int new_channels = 0;
	struct hostapd_channel_data *channel;
	struct nlattr *tb_freq[NL80211_FREQUENCY_ATTR_MAX + 1];
	struct nlattr *nl_freq;
	int rem_freq, idx;

	if (tb == NULL)
		return NL_OK;

	nla_for_each_nested(nl_freq, tb, rem_freq) {
		nla_parse(tb_freq, NL80211_FREQUENCY_ATTR_MAX,
			  nla_data(nl_freq), nla_len(nl_freq), freq_policy);
		if (!tb_freq[NL80211_FREQUENCY_ATTR_FREQ])
			continue;
		new_channels++;
	}

	channel = os_realloc_array(mode->channels,
				   mode->num_channels + new_channels,
				   sizeof(struct hostapd_channel_data));
	if (!channel)
		return NL_STOP;

	mode->channels = channel;
	mode->num_channels += new_channels;

	idx = phy_info->last_chan_idx;

	nla_for_each_nested(nl_freq, tb, rem_freq) {
		nla_parse(tb_freq, NL80211_FREQUENCY_ATTR_MAX,
			  nla_data(nl_freq), nla_len(nl_freq), freq_policy);
		if (!tb_freq[NL80211_FREQUENCY_ATTR_FREQ])
			continue;
		phy_info_freq(mode, &mode->channels[idx], tb_freq);
		idx++;
	}
	phy_info->last_chan_idx = idx;

	return NL_OK;
}


static int phy_info_rates(struct hostapd_hw_modes *mode, struct nlattr *tb)
{
	static struct nla_policy rate_policy[NL80211_BITRATE_ATTR_MAX + 1] = {
		[NL80211_BITRATE_ATTR_RATE] = { .type = NLA_U32 },
		[NL80211_BITRATE_ATTR_2GHZ_SHORTPREAMBLE] =
		{ .type = NLA_FLAG },
	};
	struct nlattr *tb_rate[NL80211_BITRATE_ATTR_MAX + 1];
	struct nlattr *nl_rate;
	int rem_rate, idx;

	if (tb == NULL)
		return NL_OK;

	nla_for_each_nested(nl_rate, tb, rem_rate) {
		nla_parse(tb_rate, NL80211_BITRATE_ATTR_MAX,
			  nla_data(nl_rate), nla_len(nl_rate),
			  rate_policy);
		if (!tb_rate[NL80211_BITRATE_ATTR_RATE])
			continue;
		mode->num_rates++;
	}

	mode->rates = os_calloc(mode->num_rates, sizeof(int));
	if (!mode->rates)
		return NL_STOP;

	idx = 0;

	nla_for_each_nested(nl_rate, tb, rem_rate) {
		nla_parse(tb_rate, NL80211_BITRATE_ATTR_MAX,
			  nla_data(nl_rate), nla_len(nl_rate),
			  rate_policy);
		if (!tb_rate[NL80211_BITRATE_ATTR_RATE])
			continue;
		mode->rates[idx] = nla_get_u32(
			tb_rate[NL80211_BITRATE_ATTR_RATE]);
		idx++;
	}

	return NL_OK;
}


static void phy_info_iftype_copy(struct hostapd_hw_modes *mode,
				 enum ieee80211_op_mode opmode,
				 struct nlattr **tb, struct nlattr **tb_flags)
{
	enum nl80211_iftype iftype;
	size_t len;
	struct he_capabilities *he_capab = &mode->he_capab[opmode];
	struct eht_capabilities *eht_capab = &mode->eht_capab[opmode];

	switch (opmode) {
	case IEEE80211_MODE_INFRA:
		iftype = NL80211_IFTYPE_STATION;
		break;
	case IEEE80211_MODE_IBSS:
		iftype = NL80211_IFTYPE_ADHOC;
		break;
	case IEEE80211_MODE_AP:
		iftype = NL80211_IFTYPE_AP;
		break;
	case IEEE80211_MODE_MESH:
		iftype = NL80211_IFTYPE_MESH_POINT;
		break;
	default:
		return;
	}

	if (!nla_get_flag(tb_flags[iftype]))
		return;

	he_capab->he_supported = 1;

	if (tb[NL80211_BAND_IFTYPE_ATTR_HE_CAP_PHY]) {
		len = nla_len(tb[NL80211_BAND_IFTYPE_ATTR_HE_CAP_PHY]);

		if (len > sizeof(he_capab->phy_cap))
			len = sizeof(he_capab->phy_cap);
		os_memcpy(he_capab->phy_cap,
			  nla_data(tb[NL80211_BAND_IFTYPE_ATTR_HE_CAP_PHY]),
			  len);
	}

	if (tb[NL80211_BAND_IFTYPE_ATTR_HE_CAP_MAC]) {
		len = nla_len(tb[NL80211_BAND_IFTYPE_ATTR_HE_CAP_MAC]);

		if (len > sizeof(he_capab->mac_cap))
			len = sizeof(he_capab->mac_cap);
		os_memcpy(he_capab->mac_cap,
			  nla_data(tb[NL80211_BAND_IFTYPE_ATTR_HE_CAP_MAC]),
			  len);
	}

	if (tb[NL80211_BAND_IFTYPE_ATTR_HE_CAP_MCS_SET]) {
		len = nla_len(tb[NL80211_BAND_IFTYPE_ATTR_HE_CAP_MCS_SET]);

		if (len > sizeof(he_capab->mcs))
			len = sizeof(he_capab->mcs);
		os_memcpy(he_capab->mcs,
			  nla_data(tb[NL80211_BAND_IFTYPE_ATTR_HE_CAP_MCS_SET]),
			  len);
	}

	if (tb[NL80211_BAND_IFTYPE_ATTR_HE_CAP_PPE]) {
		len = nla_len(tb[NL80211_BAND_IFTYPE_ATTR_HE_CAP_PPE]);

		if (len > sizeof(he_capab->ppet))
			len = sizeof(he_capab->ppet);
		os_memcpy(&he_capab->ppet,
			  nla_data(tb[NL80211_BAND_IFTYPE_ATTR_HE_CAP_PPE]),
			  len);
	}

	if (tb[NL80211_BAND_IFTYPE_ATTR_HE_6GHZ_CAPA]) {
		u16 capa;

		capa = nla_get_u16(tb[NL80211_BAND_IFTYPE_ATTR_HE_6GHZ_CAPA]);
		he_capab->he_6ghz_capa = le_to_host16(capa);
	}

	if (!tb[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_MAC] ||
	    !tb[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_PHY])
		return;

	eht_capab->eht_supported = true;

	if (tb[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_MAC] &&
	    nla_len(tb[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_MAC]) >= 2) {
		    const u8 *pos;

		    pos = nla_data(tb[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_MAC]);
		    eht_capab->mac_cap = WPA_GET_LE16(pos);
	}

	if (tb[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_PHY]) {
		len = nla_len(tb[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_PHY]);
		if (len > sizeof(eht_capab->phy_cap))
			len = sizeof(eht_capab->phy_cap);
		os_memcpy(eht_capab->phy_cap,
			  nla_data(tb[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_PHY]),
			  len);
	}

	if (tb[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_MCS_SET]) {
		len = nla_len(tb[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_MCS_SET]);
		if (len > sizeof(eht_capab->mcs))
			len = sizeof(eht_capab->mcs);
		os_memcpy(eht_capab->mcs,
			  nla_data(tb[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_MCS_SET]),
			  len);
	}

	if (tb[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_PPE]) {
		len = nla_len(tb[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_PPE]);
		if (len > sizeof(eht_capab->ppet))
			len = sizeof(eht_capab->ppet);
		os_memcpy(&eht_capab->ppet,
			  nla_data(tb[NL80211_BAND_IFTYPE_ATTR_EHT_CAP_PPE]),
			  len);
	}
}


static int phy_info_iftype(struct hostapd_hw_modes *mode,
			   struct nlattr *nl_iftype)
{
	struct nlattr *tb[NL80211_BAND_IFTYPE_ATTR_MAX + 1];
	struct nlattr *tb_flags[NL80211_IFTYPE_MAX + 1];
	unsigned int i;

	nla_parse(tb, NL80211_BAND_IFTYPE_ATTR_MAX,
		  nla_data(nl_iftype), nla_len(nl_iftype), NULL);

	if (!tb[NL80211_BAND_IFTYPE_ATTR_IFTYPES])
		return NL_STOP;

	if (nla_parse_nested(tb_flags, NL80211_IFTYPE_MAX,
			     tb[NL80211_BAND_IFTYPE_ATTR_IFTYPES], NULL))
		return NL_STOP;

	for (i = 0; i < IEEE80211_MODE_NUM; i++)
		phy_info_iftype_copy(mode, i, tb, tb_flags);

	return NL_OK;
}


static int phy_info_band(struct phy_info_arg *phy_info, struct nlattr *nl_band)
{
	struct nlattr *tb_band[NL80211_BAND_ATTR_MAX + 1];
	struct hostapd_hw_modes *mode;
	int ret;

	if (phy_info->last_mode != nl_band->nla_type) {
		mode = os_realloc_array(phy_info->modes,
					*phy_info->num_modes + 1,
					sizeof(*mode));
		if (!mode) {
			phy_info->failed = 1;
			return NL_STOP;
		}
		phy_info->modes = mode;

		mode = &phy_info->modes[*(phy_info->num_modes)];
		os_memset(mode, 0, sizeof(*mode));
		mode->mode = NUM_HOSTAPD_MODES;
		mode->flags = HOSTAPD_MODE_FLAG_HT_INFO_KNOWN |
			HOSTAPD_MODE_FLAG_VHT_INFO_KNOWN |
			HOSTAPD_MODE_FLAG_HE_INFO_KNOWN;

		/*
		 * Unsupported VHT MCS stream is defined as value 3, so the VHT
		 * MCS RX/TX map must be initialized with 0xffff to mark all 8
		 * possible streams as unsupported. This will be overridden if
		 * driver advertises VHT support.
		 */
		mode->vht_mcs_set[0] = 0xff;
		mode->vht_mcs_set[1] = 0xff;
		mode->vht_mcs_set[4] = 0xff;
		mode->vht_mcs_set[5] = 0xff;

		*(phy_info->num_modes) += 1;
		phy_info->last_mode = nl_band->nla_type;
		phy_info->last_chan_idx = 0;
	} else
		mode = &phy_info->modes[*(phy_info->num_modes) - 1];

	nla_parse(tb_band, NL80211_BAND_ATTR_MAX, nla_data(nl_band),
		  nla_len(nl_band), NULL);

	phy_info_ht_capa(mode, tb_band[NL80211_BAND_ATTR_HT_CAPA],
			 tb_band[NL80211_BAND_ATTR_HT_AMPDU_FACTOR],
			 tb_band[NL80211_BAND_ATTR_HT_AMPDU_DENSITY],
			 tb_band[NL80211_BAND_ATTR_HT_MCS_SET]);
	phy_info_vht_capa(mode, tb_band[NL80211_BAND_ATTR_VHT_CAPA],
			  tb_band[NL80211_BAND_ATTR_VHT_MCS_SET]);
	ret = phy_info_edmg_capa(mode,
				 tb_band[NL80211_BAND_ATTR_EDMG_BW_CONFIG],
				 tb_band[NL80211_BAND_ATTR_EDMG_CHANNELS]);
	if (ret == NL_OK)
		ret = phy_info_freqs(phy_info, mode,
				     tb_band[NL80211_BAND_ATTR_FREQS]);
	if (ret == NL_OK)
		ret = phy_info_rates(mode, tb_band[NL80211_BAND_ATTR_RATES]);
	if (ret != NL_OK) {
		phy_info->failed = 1;
		return ret;
	}

	if (tb_band[NL80211_BAND_ATTR_IFTYPE_DATA]) {
		struct nlattr *nl_iftype;
		int rem_band;

		nla_for_each_nested(nl_iftype,
				    tb_band[NL80211_BAND_ATTR_IFTYPE_DATA],
				    rem_band) {
			ret = phy_info_iftype(mode, nl_iftype);
			if (ret != NL_OK)
				return ret;
		}
	}

	return NL_OK;
}


static int phy_info_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct phy_info_arg *phy_info = arg;
	struct nlattr *nl_band;
	int rem_band;

	nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

	if (!tb_msg[NL80211_ATTR_WIPHY_BANDS])
		return NL_SKIP;

	nla_for_each_nested(nl_band, tb_msg[NL80211_ATTR_WIPHY_BANDS], rem_band)
	{
		int res = phy_info_band(phy_info, nl_band);
		if (res != NL_OK)
			return res;
	}

	return NL_SKIP;
}


static struct hostapd_hw_modes *
wpa_driver_nl80211_postprocess_modes(struct hostapd_hw_modes *modes,
				     u16 *num_modes)
{
	u16 m;
	struct hostapd_hw_modes *mode11g = NULL, *nmodes, *mode;
	int i, mode11g_idx = -1;

	/* heuristic to set up modes */
	for (m = 0; m < *num_modes; m++) {
		if (!modes[m].num_channels)
			continue;

		modes[m].is_6ghz = false;

		if (modes[m].channels[0].freq < 2000) {
			modes[m].num_channels = 0;
			continue;
		} else if (modes[m].channels[0].freq < 4000) {
			modes[m].mode = HOSTAPD_MODE_IEEE80211B;
			for (i = 0; i < modes[m].num_rates; i++) {
				if (modes[m].rates[i] > 200) {
					modes[m].mode = HOSTAPD_MODE_IEEE80211G;
					break;
				}
			}
		} else if (modes[m].channels[0].freq > 50000) {
			modes[m].mode = HOSTAPD_MODE_IEEE80211AD;
		} else if (is_6ghz_freq(modes[m].channels[0].freq)) {
			modes[m].mode = HOSTAPD_MODE_IEEE80211A;
			modes[m].is_6ghz = true;
		} else {
			modes[m].mode = HOSTAPD_MODE_IEEE80211A;
		}
	}

	/* Remove unsupported bands */
	m = 0;
	while (m < *num_modes) {
		if (modes[m].mode == NUM_HOSTAPD_MODES) {
			wpa_printf(MSG_DEBUG,
				   "nl80211: Remove unsupported mode");
			os_free(modes[m].channels);
			os_free(modes[m].rates);
			if (m + 1 < *num_modes)
				os_memmove(&modes[m], &modes[m + 1],
					   sizeof(struct hostapd_hw_modes) *
					   (*num_modes - (m + 1)));
			(*num_modes)--;
			continue;
		}
		m++;
	}

	/* If only 802.11g mode is included, use it to construct matching
	 * 802.11b mode data. */

	for (m = 0; m < *num_modes; m++) {
		if (modes[m].mode == HOSTAPD_MODE_IEEE80211B)
			return modes; /* 802.11b already included */
		if (modes[m].mode == HOSTAPD_MODE_IEEE80211G)
			mode11g_idx = m;
	}

	if (mode11g_idx < 0)
		return modes; /* 2.4 GHz band not supported at all */

	nmodes = os_realloc_array(modes, *num_modes + 1, sizeof(*nmodes));
	if (nmodes == NULL)
		return modes; /* Could not add 802.11b mode */

	mode = &nmodes[*num_modes];
	os_memset(mode, 0, sizeof(*mode));
	(*num_modes)++;
	modes = nmodes;

	mode->mode = HOSTAPD_MODE_IEEE80211B;

	mode11g = &modes[mode11g_idx];
	mode->num_channels = mode11g->num_channels;
	mode->channels = os_memdup(mode11g->channels,
				   mode11g->num_channels *
				   sizeof(struct hostapd_channel_data));
	if (mode->channels == NULL) {
		(*num_modes)--;
		return modes; /* Could not add 802.11b mode */
	}

	mode->num_rates = 0;
	mode->rates = os_malloc(4 * sizeof(int));
	if (mode->rates == NULL) {
		os_free(mode->channels);
		(*num_modes)--;
		return modes; /* Could not add 802.11b mode */
	}

	for (i = 0; i < mode11g->num_rates; i++) {
		if (mode11g->rates[i] != 10 && mode11g->rates[i] != 20 &&
		    mode11g->rates[i] != 55 && mode11g->rates[i] != 110)
			continue;
		mode->rates[mode->num_rates] = mode11g->rates[i];
		mode->num_rates++;
		if (mode->num_rates == 4)
			break;
	}

	if (mode->num_rates == 0) {
		os_free(mode->channels);
		os_free(mode->rates);
		(*num_modes)--;
		return modes; /* No 802.11b rates */
	}

	wpa_printf(MSG_DEBUG, "nl80211: Added 802.11b mode based on 802.11g "
		   "information");

	return modes;
}


static void nl80211_set_ht40_mode(struct hostapd_hw_modes *mode, int start,
				  int end)
{
	int c;

	for (c = 0; c < mode->num_channels; c++) {
		struct hostapd_channel_data *chan = &mode->channels[c];
		if (chan->freq - 10 >= start && chan->freq + 10 <= end)
			chan->flag |= HOSTAPD_CHAN_HT40;
	}
}


static void nl80211_set_ht40_mode_sec(struct hostapd_hw_modes *mode, int start,
				      int end)
{
	int c;

	for (c = 0; c < mode->num_channels; c++) {
		struct hostapd_channel_data *chan = &mode->channels[c];
		if (!(chan->flag & HOSTAPD_CHAN_HT40))
			continue;
		if (chan->freq - 30 >= start && chan->freq - 10 <= end)
			chan->flag |= HOSTAPD_CHAN_HT40MINUS;
		if (chan->freq + 10 >= start && chan->freq + 30 <= end)
			chan->flag |= HOSTAPD_CHAN_HT40PLUS;
	}
}


static void nl80211_reg_rule_max_eirp(u32 start, u32 end, u32 max_eirp,
				      struct phy_info_arg *results)
{
	u16 m;

	for (m = 0; m < *results->num_modes; m++) {
		int c;
		struct hostapd_hw_modes *mode = &results->modes[m];

		for (c = 0; c < mode->num_channels; c++) {
			struct hostapd_channel_data *chan = &mode->channels[c];
			if ((u32) chan->freq - 10 >= start &&
			    (u32) chan->freq + 10 <= end)
				chan->max_tx_power = max_eirp;
		}
	}
}


static void nl80211_reg_rule_ht40(u32 start, u32 end,
				  struct phy_info_arg *results)
{
	u16 m;

	for (m = 0; m < *results->num_modes; m++) {
		if (!(results->modes[m].ht_capab &
		      HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET))
			continue;
		nl80211_set_ht40_mode(&results->modes[m], start, end);
	}
}


static void nl80211_reg_rule_sec(struct nlattr *tb[],
				 struct phy_info_arg *results)
{
	u32 start, end, max_bw;
	u16 m;

	if (tb[NL80211_ATTR_FREQ_RANGE_START] == NULL ||
	    tb[NL80211_ATTR_FREQ_RANGE_END] == NULL ||
	    tb[NL80211_ATTR_FREQ_RANGE_MAX_BW] == NULL)
		return;

	start = nla_get_u32(tb[NL80211_ATTR_FREQ_RANGE_START]) / 1000;
	end = nla_get_u32(tb[NL80211_ATTR_FREQ_RANGE_END]) / 1000;
	max_bw = nla_get_u32(tb[NL80211_ATTR_FREQ_RANGE_MAX_BW]) / 1000;

	if (max_bw < 20)
		return;

	for (m = 0; m < *results->num_modes; m++) {
		if (!(results->modes[m].ht_capab &
		      HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET))
			continue;
		nl80211_set_ht40_mode_sec(&results->modes[m], start, end);
	}
}


static void nl80211_set_vht_mode(struct hostapd_hw_modes *mode, int start,
				 int end, int max_bw)
{
	int c;

	for (c = 0; c < mode->num_channels; c++) {
		struct hostapd_channel_data *chan = &mode->channels[c];

		if (chan->freq - 10 < start || chan->freq + 10 > end)
			continue;

		if (max_bw >= 80)
			chan->flag |= HOSTAPD_CHAN_VHT_80MHZ_SUBCHANNEL;

		if (max_bw >= 160)
			chan->flag |= HOSTAPD_CHAN_VHT_160MHZ_SUBCHANNEL;
	}
}


static void nl80211_reg_rule_vht(struct nlattr *tb[],
				 struct phy_info_arg *results)
{
	u32 start, end, max_bw;
	u16 m;

	if (tb[NL80211_ATTR_FREQ_RANGE_START] == NULL ||
	    tb[NL80211_ATTR_FREQ_RANGE_END] == NULL ||
	    tb[NL80211_ATTR_FREQ_RANGE_MAX_BW] == NULL)
		return;

	start = nla_get_u32(tb[NL80211_ATTR_FREQ_RANGE_START]) / 1000;
	end = nla_get_u32(tb[NL80211_ATTR_FREQ_RANGE_END]) / 1000;
	max_bw = nla_get_u32(tb[NL80211_ATTR_FREQ_RANGE_MAX_BW]) / 1000;

	if (max_bw < 80)
		return;

	for (m = 0; m < *results->num_modes; m++) {
		if (!(results->modes[m].ht_capab &
		      HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET))
			continue;
		/* TODO: use a real VHT support indication */
		if (!results->modes[m].vht_capab)
			continue;

		nl80211_set_vht_mode(&results->modes[m], start, end, max_bw);
	}
}


static void nl80211_set_6ghz_mode(struct hostapd_hw_modes *mode, int start,
				  int end, int max_bw)
{
	int c;

	for (c = 0; c < mode->num_channels; c++) {
		struct hostapd_channel_data *chan = &mode->channels[c];

		if (chan->freq - 10 < start || chan->freq + 10 > end)
			continue;

		if (max_bw >= 80)
			chan->flag |= HOSTAPD_CHAN_VHT_80MHZ_SUBCHANNEL;

		if (max_bw >= 160)
			chan->flag |= HOSTAPD_CHAN_VHT_160MHZ_SUBCHANNEL;

		if (max_bw >= 320)
			chan->flag |= HOSTAPD_CHAN_EHT_320MHZ_SUBCHANNEL;
	}
}


static void nl80211_reg_rule_6ghz(struct nlattr *tb[],
				 struct phy_info_arg *results)
{
	u32 start, end, max_bw;
	u16 m;

	if (!tb[NL80211_ATTR_FREQ_RANGE_START] ||
	    !tb[NL80211_ATTR_FREQ_RANGE_END] ||
	    !tb[NL80211_ATTR_FREQ_RANGE_MAX_BW])
		return;

	start = nla_get_u32(tb[NL80211_ATTR_FREQ_RANGE_START]) / 1000;
	end = nla_get_u32(tb[NL80211_ATTR_FREQ_RANGE_END]) / 1000;
	max_bw = nla_get_u32(tb[NL80211_ATTR_FREQ_RANGE_MAX_BW]) / 1000;

	if (max_bw < 80)
		return;

	for (m = 0; m < *results->num_modes; m++) {
		if (results->modes[m].num_channels == 0 ||
		    !is_6ghz_freq(results->modes[m].channels[0].freq))
			continue;

		nl80211_set_6ghz_mode(&results->modes[m], start, end, max_bw);
	}
}


static void nl80211_set_dfs_domain(enum nl80211_dfs_regions region,
				   u8 *dfs_domain)
{
	if (region == NL80211_DFS_FCC)
		*dfs_domain = HOSTAPD_DFS_REGION_FCC;
	else if (region == NL80211_DFS_ETSI)
		*dfs_domain = HOSTAPD_DFS_REGION_ETSI;
	else if (region == NL80211_DFS_JP)
		*dfs_domain = HOSTAPD_DFS_REGION_JP;
	else
		*dfs_domain = 0;
}


static const char * dfs_domain_name(enum nl80211_dfs_regions region)
{
	switch (region) {
	case NL80211_DFS_UNSET:
		return "DFS-UNSET";
	case NL80211_DFS_FCC:
		return "DFS-FCC";
	case NL80211_DFS_ETSI:
		return "DFS-ETSI";
	case NL80211_DFS_JP:
		return "DFS-JP";
	default:
		return "DFS-invalid";
	}
}


static int nl80211_get_reg(struct nl_msg *msg, void *arg)
{
	struct phy_info_arg *results = arg;
	struct nlattr *tb_msg[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct nlattr *nl_rule;
	struct nlattr *tb_rule[NL80211_FREQUENCY_ATTR_MAX + 1];
	int rem_rule;
	static struct nla_policy reg_policy[NL80211_FREQUENCY_ATTR_MAX + 1] = {
		[NL80211_ATTR_REG_RULE_FLAGS] = { .type = NLA_U32 },
		[NL80211_ATTR_FREQ_RANGE_START] = { .type = NLA_U32 },
		[NL80211_ATTR_FREQ_RANGE_END] = { .type = NLA_U32 },
		[NL80211_ATTR_FREQ_RANGE_MAX_BW] = { .type = NLA_U32 },
		[NL80211_ATTR_POWER_RULE_MAX_ANT_GAIN] = { .type = NLA_U32 },
		[NL80211_ATTR_POWER_RULE_MAX_EIRP] = { .type = NLA_U32 },
	};

	nla_parse(tb_msg, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);
	if (!tb_msg[NL80211_ATTR_REG_ALPHA2] ||
	    !tb_msg[NL80211_ATTR_REG_RULES]) {
		wpa_printf(MSG_DEBUG, "nl80211: No regulatory information "
			   "available");
		return NL_SKIP;
	}

	if (tb_msg[NL80211_ATTR_DFS_REGION]) {
		enum nl80211_dfs_regions dfs_domain;
		dfs_domain = nla_get_u8(tb_msg[NL80211_ATTR_DFS_REGION]);
		nl80211_set_dfs_domain(dfs_domain, &results->dfs_domain);
		wpa_printf(MSG_DEBUG, "nl80211: Regulatory information - country=%s (%s)",
			   (char *) nla_data(tb_msg[NL80211_ATTR_REG_ALPHA2]),
			   dfs_domain_name(dfs_domain));
	} else {
		wpa_printf(MSG_DEBUG, "nl80211: Regulatory information - country=%s",
			   (char *) nla_data(tb_msg[NL80211_ATTR_REG_ALPHA2]));
	}

	nla_for_each_nested(nl_rule, tb_msg[NL80211_ATTR_REG_RULES], rem_rule)
	{
		u32 start, end, max_eirp = 0, max_bw = 0, flags = 0;
		nla_parse(tb_rule, NL80211_FREQUENCY_ATTR_MAX,
			  nla_data(nl_rule), nla_len(nl_rule), reg_policy);
		if (tb_rule[NL80211_ATTR_FREQ_RANGE_START] == NULL ||
		    tb_rule[NL80211_ATTR_FREQ_RANGE_END] == NULL)
			continue;
		start = nla_get_u32(tb_rule[NL80211_ATTR_FREQ_RANGE_START]) / 1000;
		end = nla_get_u32(tb_rule[NL80211_ATTR_FREQ_RANGE_END]) / 1000;
		if (tb_rule[NL80211_ATTR_POWER_RULE_MAX_EIRP])
			max_eirp = nla_get_u32(tb_rule[NL80211_ATTR_POWER_RULE_MAX_EIRP]) / 100;
		if (tb_rule[NL80211_ATTR_FREQ_RANGE_MAX_BW])
			max_bw = nla_get_u32(tb_rule[NL80211_ATTR_FREQ_RANGE_MAX_BW]) / 1000;
		if (tb_rule[NL80211_ATTR_REG_RULE_FLAGS])
			flags = nla_get_u32(tb_rule[NL80211_ATTR_REG_RULE_FLAGS]);

		wpa_printf(MSG_DEBUG, "nl80211: %u-%u @ %u MHz %u mBm%s%s%s%s%s%s%s%s",
			   start, end, max_bw, max_eirp,
			   flags & NL80211_RRF_NO_OFDM ? " (no OFDM)" : "",
			   flags & NL80211_RRF_NO_CCK ? " (no CCK)" : "",
			   flags & NL80211_RRF_NO_INDOOR ? " (no indoor)" : "",
			   flags & NL80211_RRF_NO_OUTDOOR ? " (no outdoor)" :
			   "",
			   flags & NL80211_RRF_DFS ? " (DFS)" : "",
			   flags & NL80211_RRF_PTP_ONLY ? " (PTP only)" : "",
			   flags & NL80211_RRF_PTMP_ONLY ? " (PTMP only)" : "",
			   flags & NL80211_RRF_NO_IR ? " (no IR)" : "");
		if (max_bw >= 40)
			nl80211_reg_rule_ht40(start, end, results);
		if (tb_rule[NL80211_ATTR_POWER_RULE_MAX_EIRP])
			nl80211_reg_rule_max_eirp(start, end, max_eirp,
						  results);
	}

	nla_for_each_nested(nl_rule, tb_msg[NL80211_ATTR_REG_RULES], rem_rule)
	{
		nla_parse(tb_rule, NL80211_FREQUENCY_ATTR_MAX,
			  nla_data(nl_rule), nla_len(nl_rule), reg_policy);
		nl80211_reg_rule_sec(tb_rule, results);
	}

	nla_for_each_nested(nl_rule, tb_msg[NL80211_ATTR_REG_RULES], rem_rule)
	{
		nla_parse(tb_rule, NL80211_FREQUENCY_ATTR_MAX,
			  nla_data(nl_rule), nla_len(nl_rule), reg_policy);
		nl80211_reg_rule_vht(tb_rule, results);
	}

	nla_for_each_nested(nl_rule, tb_msg[NL80211_ATTR_REG_RULES], rem_rule)
	{
		nla_parse(tb_rule, NL80211_FREQUENCY_ATTR_MAX,
			  nla_data(nl_rule), nla_len(nl_rule), reg_policy);
		nl80211_reg_rule_6ghz(tb_rule, results);
	}

	return NL_SKIP;
}


static int nl80211_set_regulatory_flags(struct wpa_driver_nl80211_data *drv,
					struct phy_info_arg *results)
{
	struct nl_msg *msg;

	msg = nlmsg_alloc();
	if (!msg)
		return -ENOMEM;

	nl80211_cmd(drv, msg, 0, NL80211_CMD_GET_REG);
	if (drv->capa.flags & WPA_DRIVER_FLAGS_SELF_MANAGED_REGULATORY) {
		if (nla_put_u32(msg, NL80211_ATTR_WIPHY, drv->wiphy_idx)) {
			nlmsg_free(msg);
			return -1;
		}
	}

	return send_and_recv_resp(drv, msg, nl80211_get_reg, results);
}


static const char * modestr(enum hostapd_hw_mode mode)
{
	switch (mode) {
	case HOSTAPD_MODE_IEEE80211B:
		return "802.11b";
	case HOSTAPD_MODE_IEEE80211G:
		return "802.11g";
	case HOSTAPD_MODE_IEEE80211A:
		return "802.11a";
	case HOSTAPD_MODE_IEEE80211AD:
		return "802.11ad";
	default:
		return "?";
	}
}


static void nl80211_dump_chan_list(struct wpa_driver_nl80211_data *drv,
				   struct hostapd_hw_modes *modes,
				   u16 num_modes)
{
	int i;

	if (!modes)
		return;

	for (i = 0; i < num_modes; i++) {
		struct hostapd_hw_modes *mode = &modes[i];
		char str[1000];
		char *pos = str;
		char *end = pos + sizeof(str);
		int j, res;

		for (j = 0; j < mode->num_channels; j++) {
			struct hostapd_channel_data *chan = &mode->channels[j];

			if (is_6ghz_freq(chan->freq))
				drv->uses_6ghz = true;
			if (chan->freq >= 900 && chan->freq < 1000)
				drv->uses_s1g = true;
			res = os_snprintf(pos, end - pos, " %d%s%s%s",
					  chan->freq,
					  (chan->flag & HOSTAPD_CHAN_DISABLED) ?
					  "[DISABLED]" : "",
					  (chan->flag & HOSTAPD_CHAN_NO_IR) ?
					  "[NO_IR]" : "",
					  (chan->flag & HOSTAPD_CHAN_RADAR) ?
					  "[RADAR]" : "");
			if (os_snprintf_error(end - pos, res))
				break;
			pos += res;
		}

		*pos = '\0';
		wpa_printf(MSG_DEBUG, "nl80211: Mode IEEE %s:%s",
			   modestr(mode->mode), str);
	}
}


struct hostapd_hw_modes *
nl80211_get_hw_feature_data(void *priv, u16 *num_modes, u16 *flags,
			    u8 *dfs_domain)
{
	u32 feat;
	struct i802_bss *bss = priv;
	struct wpa_driver_nl80211_data *drv = bss->drv;
	int nl_flags = 0;
	struct nl_msg *msg;
	struct phy_info_arg result = {
		.num_modes = num_modes,
		.modes = NULL,
		.last_mode = -1,
		.failed = 0,
		.dfs_domain = 0,
	};

	*num_modes = 0;
	*flags = 0;
	*dfs_domain = 0;

	feat = get_nl80211_protocol_features(drv);
	if (feat & NL80211_PROTOCOL_FEATURE_SPLIT_WIPHY_DUMP)
		nl_flags = NLM_F_DUMP;
	if (!(msg = nl80211_cmd_msg(bss, nl_flags, NL80211_CMD_GET_WIPHY)) ||
	    nla_put_flag(msg, NL80211_ATTR_SPLIT_WIPHY_DUMP)) {
		nlmsg_free(msg);
		return NULL;
	}

	if (send_and_recv_resp(drv, msg, phy_info_handler, &result) == 0) {
		struct hostapd_hw_modes *modes;

		nl80211_set_regulatory_flags(drv, &result);
		if (result.failed) {
			int i;

			for (i = 0; result.modes && i < *num_modes; i++) {
				os_free(result.modes[i].channels);
				os_free(result.modes[i].rates);
			}
			os_free(result.modes);
			*num_modes = 0;
			return NULL;
		}

		*dfs_domain = result.dfs_domain;

		modes = wpa_driver_nl80211_postprocess_modes(result.modes,
							     num_modes);
		nl80211_dump_chan_list(drv, modes, *num_modes);
		return modes;
	}

	return NULL;
}
