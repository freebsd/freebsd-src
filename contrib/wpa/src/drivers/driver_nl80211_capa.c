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
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/qca-vendor.h"
#include "common/qca-vendor-attr.h"
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

	if (send_and_recv_msgs(drv, msg, protocol_feature_handler, &feat) == 0)
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
	unsigned int monitor_supported:1;
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
		case NL80211_IFTYPE_MONITOR:
			info->monitor_supported = 1;
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
		}
	}
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
		case WLAN_CIPHER_SUITE_CCMP_256:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_CCMP_256;
			break;
		case WLAN_CIPHER_SUITE_GCMP_256:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_GCMP_256;
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_CCMP;
			break;
		case WLAN_CIPHER_SUITE_GCMP:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_GCMP;
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_TKIP;
			break;
		case WLAN_CIPHER_SUITE_WEP104:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_WEP104;
			break;
		case WLAN_CIPHER_SUITE_WEP40:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_WEP40;
			break;
		case WLAN_CIPHER_SUITE_AES_CMAC:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_BIP;
			break;
		case WLAN_CIPHER_SUITE_BIP_GMAC_128:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_BIP_GMAC_128;
			break;
		case WLAN_CIPHER_SUITE_BIP_GMAC_256:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_BIP_GMAC_256;
			break;
		case WLAN_CIPHER_SUITE_BIP_CMAC_256:
			info->capa->enc |= WPA_DRIVER_CAPA_ENC_BIP_CMAC_256;
			break;
		case WLAN_CIPHER_SUITE_NO_GROUP_ADDR:
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

	if (flags & NL80211_FEATURE_STATIC_SMPS)
		capa->smps_modes |= WPA_DRIVER_SMPS_MODE_STATIC;

	if (flags & NL80211_FEATURE_DYNAMIC_SMPS)
		capa->smps_modes |= WPA_DRIVER_SMPS_MODE_DYNAMIC;

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


static int wiphy_info_handler(struct nl_msg *msg, void *arg)
{
	struct nlattr *tb[NL80211_ATTR_MAX + 1];
	struct genlmsghdr *gnlh = nlmsg_data(nlmsg_hdr(msg));
	struct wiphy_info_data *info = arg;
	struct wpa_driver_capa *capa = info->capa;
	struct wpa_driver_nl80211_data *drv = info->drv;

	nla_parse(tb, NL80211_ATTR_MAX, genlmsg_attrdata(gnlh, 0),
		  genlmsg_attrlen(gnlh, 0), NULL);

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

	if (tb[NL80211_ATTR_MAX_MATCH_SETS])
		capa->max_match_sets =
			nla_get_u8(tb[NL80211_ATTR_MAX_MATCH_SETS]);

	if (tb[NL80211_ATTR_MAC_ACL_MAX])
		capa->max_acl_mac_addrs =
			nla_get_u8(tb[NL80211_ATTR_MAC_ACL_MAX]);

	wiphy_info_supported_iftypes(info, tb[NL80211_ATTR_SUPPORTED_IFTYPES]);
	wiphy_info_iface_comb(info, tb[NL80211_ATTR_INTERFACE_COMBINATIONS]);
	wiphy_info_supp_cmds(info, tb[NL80211_ATTR_SUPPORTED_COMMANDS]);
	wiphy_info_cipher_suites(info, tb[NL80211_ATTR_CIPHER_SUITES]);

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

	if (tb[NL80211_ATTR_DEVICE_AP_SME])
		info->device_ap_sme = 1;

	wiphy_info_feature_flags(info, tb[NL80211_ATTR_FEATURE_FLAGS]);
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
		}
		drv->extended_capa_mask =
			os_malloc(nla_len(tb[NL80211_ATTR_EXT_CAPA_MASK]));
		if (drv->extended_capa_mask) {
			os_memcpy(drv->extended_capa_mask,
				  nla_data(tb[NL80211_ATTR_EXT_CAPA_MASK]),
				  nla_len(tb[NL80211_ATTR_EXT_CAPA_MASK]));
		} else {
			os_free(drv->extended_capa);
			drv->extended_capa = NULL;
			drv->extended_capa_len = 0;
		}
	}

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
			switch (vinfo->subcmd) {
			case QCA_NL80211_VENDOR_SUBCMD_TEST:
				drv->vendor_cmd_test_avail = 1;
				break;
			case QCA_NL80211_VENDOR_SUBCMD_ROAMING:
				drv->roaming_vendor_cmd_avail = 1;
				break;
			case QCA_NL80211_VENDOR_SUBCMD_DFS_CAPABILITY:
				drv->dfs_vendor_cmd_avail = 1;
				break;
			case QCA_NL80211_VENDOR_SUBCMD_GET_FEATURES:
				drv->get_features_vendor_cmd_avail = 1;
				break;
			case QCA_NL80211_VENDOR_SUBCMD_DO_ACS:
				drv->capa.flags |= WPA_DRIVER_FLAGS_ACS_OFFLOAD;
				break;
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

	if (send_and_recv_msgs(drv, msg, wiphy_info_handler, info))
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

	if (info->channel_switch_supported)
		drv->capa.flags |= WPA_DRIVER_FLAGS_AP_CSA;
	drv->capa.wmm_ac_supported = info->wmm_ac_supported;

	drv->capa.mac_addr_rand_sched_scan_supported =
		info->mac_addr_rand_sched_scan_supported;
	drv->capa.mac_addr_rand_scan_supported =
		info->mac_addr_rand_scan_supported;

	return 0;
}


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

	ret = send_and_recv_msgs(drv, msg, dfs_info_handler, &dfs_capability);
	if (!ret && dfs_capability)
		drv->capa.flags |= WPA_DRIVER_FLAGS_DFS_OFFLOAD;
}


struct features_info {
	u8 *flags;
	size_t flags_len;
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
			info->flags = nla_data(attr);
			info->flags_len = nla_len(attr);
		}
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
	ret = send_and_recv_msgs(drv, msg, features_info_handler, &info);
	if (ret || !info.flags)
		return;

	if (check_feature(QCA_WLAN_VENDOR_FEATURE_KEY_MGMT_OFFLOAD, &info))
		drv->capa.flags |= WPA_DRIVER_FLAGS_KEY_MGMT_OFFLOAD;
}


int wpa_driver_nl80211_capa(struct wpa_driver_nl80211_data *drv)
{
	struct wiphy_info_data info;
	if (wpa_driver_nl80211_get_info(drv, &info))
		return -1;

	if (info.error)
		return -1;

	drv->has_capability = 1;
	drv->capa.key_mgmt = WPA_DRIVER_CAPA_KEY_MGMT_WPA |
		WPA_DRIVER_CAPA_KEY_MGMT_WPA_PSK |
		WPA_DRIVER_CAPA_KEY_MGMT_WPA2 |
		WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK |
		WPA_DRIVER_CAPA_KEY_MGMT_SUITE_B |
		WPA_DRIVER_CAPA_KEY_MGMT_SUITE_B_192;
	drv->capa.auth = WPA_DRIVER_AUTH_OPEN |
		WPA_DRIVER_AUTH_SHARED |
		WPA_DRIVER_AUTH_LEAP;

	drv->capa.flags |= WPA_DRIVER_FLAGS_SANE_ERROR_CODES;
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
	drv->use_monitor = !info.poll_command_supported || !info.data_tx_status;

	if (drv->device_ap_sme && drv->use_monitor) {
		/*
		 * Non-mac80211 drivers may not support monitor interface.
		 * Make sure we do not get stuck with incorrect capability here
		 * by explicitly testing this.
		 */
		if (!info.monitor_supported) {
			wpa_printf(MSG_DEBUG, "nl80211: Disable use_monitor "
				   "with device_ap_sme since no monitor mode "
				   "support detected");
			drv->use_monitor = 0;
		}
	}

	/*
	 * If we aren't going to use monitor interfaces, but the
	 * driver doesn't support data TX status, we won't get TX
	 * status for EAPOL frames.
	 */
	if (!drv->use_monitor && !info.data_tx_status)
		drv->capa.flags &= ~WPA_DRIVER_FLAGS_EAPOL_TX_STATUS;

	qca_nl80211_check_dfs_capa(drv);
	qca_nl80211_get_features(drv);

	return 0;
}


struct phy_info_arg {
	u16 *num_modes;
	struct hostapd_hw_modes *modes;
	int last_mode, last_chan_idx;
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


static void phy_info_freq(struct hostapd_hw_modes *mode,
			  struct hostapd_channel_data *chan,
			  struct nlattr *tb_freq[])
{
	u8 channel;
	chan->freq = nla_get_u32(tb_freq[NL80211_FREQUENCY_ATTR_FREQ]);
	chan->flag = 0;
	chan->dfs_cac_ms = 0;
	if (ieee80211_freq_to_chan(chan->freq, &channel) != NUM_HOSTAPD_MODES)
		chan->chan = channel;

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
		return NL_SKIP;

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
		return NL_SKIP;

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


static int phy_info_band(struct phy_info_arg *phy_info, struct nlattr *nl_band)
{
	struct nlattr *tb_band[NL80211_BAND_ATTR_MAX + 1];
	struct hostapd_hw_modes *mode;
	int ret;

	if (phy_info->last_mode != nl_band->nla_type) {
		mode = os_realloc_array(phy_info->modes,
					*phy_info->num_modes + 1,
					sizeof(*mode));
		if (!mode)
			return NL_SKIP;
		phy_info->modes = mode;

		mode = &phy_info->modes[*(phy_info->num_modes)];
		os_memset(mode, 0, sizeof(*mode));
		mode->mode = NUM_HOSTAPD_MODES;
		mode->flags = HOSTAPD_MODE_FLAG_HT_INFO_KNOWN |
			HOSTAPD_MODE_FLAG_VHT_INFO_KNOWN;

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
	ret = phy_info_freqs(phy_info, mode, tb_band[NL80211_BAND_ATTR_FREQS]);
	if (ret != NL_OK)
		return ret;
	ret = phy_info_rates(mode, tb_band[NL80211_BAND_ATTR_RATES]);
	if (ret != NL_OK)
		return ret;

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
		if (modes[m].channels[0].freq < 4000) {
			modes[m].mode = HOSTAPD_MODE_IEEE80211B;
			for (i = 0; i < modes[m].num_rates; i++) {
				if (modes[m].rates[i] > 200) {
					modes[m].mode = HOSTAPD_MODE_IEEE80211G;
					break;
				}
			}
		} else if (modes[m].channels[0].freq > 50000)
			modes[m].mode = HOSTAPD_MODE_IEEE80211AD;
		else
			modes[m].mode = HOSTAPD_MODE_IEEE80211A;
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
	mode->channels = os_malloc(mode11g->num_channels *
				   sizeof(struct hostapd_channel_data));
	if (mode->channels == NULL) {
		(*num_modes)--;
		return modes; /* Could not add 802.11b mode */
	}
	os_memcpy(mode->channels, mode11g->channels,
		  mode11g->num_channels * sizeof(struct hostapd_channel_data));

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
				 int end)
{
	int c;

	for (c = 0; c < mode->num_channels; c++) {
		struct hostapd_channel_data *chan = &mode->channels[c];
		if (chan->freq - 10 >= start && chan->freq + 70 <= end)
			chan->flag |= HOSTAPD_CHAN_VHT_10_70;

		if (chan->freq - 30 >= start && chan->freq + 50 <= end)
			chan->flag |= HOSTAPD_CHAN_VHT_30_50;

		if (chan->freq - 50 >= start && chan->freq + 30 <= end)
			chan->flag |= HOSTAPD_CHAN_VHT_50_30;

		if (chan->freq - 70 >= start && chan->freq + 10 <= end)
			chan->flag |= HOSTAPD_CHAN_VHT_70_10;
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

		nl80211_set_vht_mode(&results->modes[m], start, end);
	}
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
	return send_and_recv_msgs(drv, msg, nl80211_get_reg, results);
}


struct hostapd_hw_modes *
nl80211_get_hw_feature_data(void *priv, u16 *num_modes, u16 *flags)
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
	};

	*num_modes = 0;
	*flags = 0;

	feat = get_nl80211_protocol_features(drv);
	if (feat & NL80211_PROTOCOL_FEATURE_SPLIT_WIPHY_DUMP)
		nl_flags = NLM_F_DUMP;
	if (!(msg = nl80211_cmd_msg(bss, nl_flags, NL80211_CMD_GET_WIPHY)) ||
	    nla_put_flag(msg, NL80211_ATTR_SPLIT_WIPHY_DUMP)) {
		nlmsg_free(msg);
		return NULL;
	}

	if (send_and_recv_msgs(drv, msg, phy_info_handler, &result) == 0) {
		nl80211_set_regulatory_flags(drv, &result);
		return wpa_driver_nl80211_postprocess_modes(result.modes,
							    num_modes);
	}

	return NULL;
}
