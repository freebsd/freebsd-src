/*
 * Control interface for shared AP commands
 * Copyright (c) 2004-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/sae.h"
#include "eapol_auth/eapol_auth_sm.h"
#include "fst/fst_ctrl_iface.h"
#include "hostapd.h"
#include "ieee802_1x.h"
#include "wpa_auth.h"
#include "ieee802_11.h"
#include "sta_info.h"
#include "wps_hostapd.h"
#include "p2p_hostapd.h"
#include "ctrl_iface_ap.h"
#include "ap_drv_ops.h"
#include "mbo_ap.h"
#include "taxonomy.h"
#include "wnm_ap.h"


static size_t hostapd_write_ht_mcs_bitmask(char *buf, size_t buflen,
					   size_t curr_len, const u8 *mcs_set)
{
	int ret;
	size_t len = curr_len;

	ret = os_snprintf(buf + len, buflen - len,
			  "ht_mcs_bitmask=");
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	/* 77 first bits (+ 3 reserved bits) */
	len += wpa_snprintf_hex(buf + len, buflen - len, mcs_set, 10);

	ret = os_snprintf(buf + len, buflen - len, "\n");
	if (os_snprintf_error(buflen - len, ret))
		return curr_len;
	len += ret;

	return len;
}


static int hostapd_get_sta_conn_time(struct sta_info *sta,
				     struct hostap_sta_driver_data *data,
				     char *buf, size_t buflen)
{
	struct os_reltime age;
	unsigned long secs;
	int ret;

	if (sta->connected_time.sec) {
		/* Locally maintained time in AP mode */
		os_reltime_age(&sta->connected_time, &age);
		secs = (unsigned long) age.sec;
	} else if (data->flags & STA_DRV_DATA_CONN_TIME) {
		/* Time from the driver in mesh mode */
		secs = data->connected_sec;
	} else {
		return 0;
	}

	ret = os_snprintf(buf, buflen, "connected_time=%lu\n", secs);
	if (os_snprintf_error(buflen, ret))
		return 0;
	return ret;
}


static int hostapd_get_sta_info(struct hostapd_data *hapd,
				struct sta_info *sta,
				char *buf, size_t buflen)
{
	struct hostap_sta_driver_data data;
	int ret;
	int len = 0;

	if (hostapd_drv_read_sta_data(hapd, &data, sta->addr) < 0)
		return 0;

	ret = os_snprintf(buf, buflen, "rx_packets=%lu\ntx_packets=%lu\n"
			  "rx_bytes=%llu\ntx_bytes=%llu\ninactive_msec=%lu\n"
			  "signal=%d\n",
			  data.rx_packets, data.tx_packets,
			  data.rx_bytes, data.tx_bytes, data.inactive_msec,
			  data.signal);
	if (os_snprintf_error(buflen, ret))
		return 0;
	len += ret;

	ret = os_snprintf(buf + len, buflen - len, "rx_rate_info=%lu",
			  data.current_rx_rate / 100);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;
	if (data.flags & STA_DRV_DATA_RX_MCS) {
		ret = os_snprintf(buf + len, buflen - len, " mcs %u",
				  data.rx_mcs);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_RX_VHT_MCS) {
		ret = os_snprintf(buf + len, buflen - len, " vhtmcs %u",
				  data.rx_vhtmcs);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_RX_VHT_NSS) {
		ret = os_snprintf(buf + len, buflen - len, " vhtnss %u",
				  data.rx_vht_nss);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_RX_SHORT_GI) {
		ret = os_snprintf(buf + len, buflen - len, " shortGI");
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	ret = os_snprintf(buf + len, buflen - len, "\n");
	if (!os_snprintf_error(buflen - len, ret))
		len += ret;

	ret = os_snprintf(buf + len, buflen - len, "tx_rate_info=%lu",
			  data.current_tx_rate / 100);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;
	if (data.flags & STA_DRV_DATA_TX_MCS) {
		ret = os_snprintf(buf + len, buflen - len, " mcs %u",
				  data.tx_mcs);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_TX_VHT_MCS) {
		ret = os_snprintf(buf + len, buflen - len, " vhtmcs %u",
				  data.tx_vhtmcs);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_TX_VHT_NSS) {
		ret = os_snprintf(buf + len, buflen - len, " vhtnss %u",
				  data.tx_vht_nss);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	if (data.flags & STA_DRV_DATA_TX_SHORT_GI) {
		ret = os_snprintf(buf + len, buflen - len, " shortGI");
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}
	ret = os_snprintf(buf + len, buflen - len, "\n");
	if (!os_snprintf_error(buflen - len, ret))
		len += ret;

	if ((sta->flags & WLAN_STA_VHT) && sta->vht_capabilities) {
		ret = os_snprintf(buf + len, buflen - len,
				  "rx_vht_mcs_map=%04x\n"
				  "tx_vht_mcs_map=%04x\n",
				  le_to_host16(sta->vht_capabilities->
					       vht_supported_mcs_set.rx_map),
				  le_to_host16(sta->vht_capabilities->
					       vht_supported_mcs_set.tx_map));
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

	if ((sta->flags & WLAN_STA_HT) && sta->ht_capabilities) {
		len = hostapd_write_ht_mcs_bitmask(buf, buflen, len,
						   sta->ht_capabilities->
						   supported_mcs_set);
	}

	if (data.flags & STA_DRV_DATA_LAST_ACK_RSSI) {
		ret = os_snprintf(buf + len, buflen - len,
				  "last_ack_signal=%d\n", data.last_ack_rssi);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

	len += hostapd_get_sta_conn_time(sta, &data, buf + len, buflen - len);

	return len;
}


static const char * timeout_next_str(int val)
{
	switch (val) {
	case STA_NULLFUNC:
		return "NULLFUNC POLL";
	case STA_DISASSOC:
		return "DISASSOC";
	case STA_DEAUTH:
		return "DEAUTH";
	case STA_REMOVE:
		return "REMOVE";
	case STA_DISASSOC_FROM_CLI:
		return "DISASSOC_FROM_CLI";
	default:
		return "?";
	}
}


static const char * hw_mode_str(enum hostapd_hw_mode mode)
{
	switch (mode) {
	case HOSTAPD_MODE_IEEE80211B:
		return "b";
	case HOSTAPD_MODE_IEEE80211G:
		return "g";
	case HOSTAPD_MODE_IEEE80211A:
		return "a";
	case HOSTAPD_MODE_IEEE80211AD:
		return "ad";
	case HOSTAPD_MODE_IEEE80211ANY:
		return "any";
	case NUM_HOSTAPD_MODES:
		return "invalid";
	}
	return "unknown";
}


static int hostapd_ctrl_iface_sta_mib(struct hostapd_data *hapd,
				      struct sta_info *sta,
				      char *buf, size_t buflen)
{
	int len, res, ret, i;
	const char *keyid;
	const u8 *dpp_pkhash;

	if (!sta)
		return 0;

	len = 0;
	ret = os_snprintf(buf + len, buflen - len, MACSTR "\nflags=",
			  MAC2STR(sta->addr));
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	ret = ap_sta_flags_txt(sta->flags, buf + len, buflen - len);
	if (ret < 0)
		return len;
	len += ret;

	ret = os_snprintf(buf + len, buflen - len, "\naid=%d\ncapability=0x%x\n"
			  "listen_interval=%d\nsupported_rates=",
			  sta->aid, sta->capability, sta->listen_interval);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	for (i = 0; i < sta->supported_rates_len; i++) {
		ret = os_snprintf(buf + len, buflen - len, "%02x%s",
				  sta->supported_rates[i],
				  i + 1 < sta->supported_rates_len ? " " : "");
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	ret = os_snprintf(buf + len, buflen - len, "\ntimeout_next=%s\n",
			  timeout_next_str(sta->timeout_next));
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	if (sta->max_idle_period) {
		ret = os_snprintf(buf + len, buflen - len,
				  "max_idle_period=%d\n", sta->max_idle_period);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	res = ieee802_11_get_mib_sta(hapd, sta, buf + len, buflen - len);
	if (res >= 0)
		len += res;
	res = wpa_get_mib_sta(sta->wpa_sm, buf + len, buflen - len);
	if (res >= 0)
		len += res;
	res = ieee802_1x_get_mib_sta(hapd, sta, buf + len, buflen - len);
	if (res >= 0)
		len += res;
	res = hostapd_wps_get_mib_sta(hapd, sta->addr, buf + len,
				      buflen - len);
	if (res >= 0)
		len += res;
	res = hostapd_p2p_get_mib_sta(hapd, sta, buf + len, buflen - len);
	if (res >= 0)
		len += res;

	len += hostapd_get_sta_info(hapd, sta, buf + len, buflen - len);

#ifdef CONFIG_SAE
	if (sta->sae && sta->sae->state == SAE_ACCEPTED) {
		res = os_snprintf(buf + len, buflen - len, "sae_group=%d\n",
				  sta->sae->group);
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}

	if (sta->sae && sta->sae->tmp) {
		const u8 *pos;
		unsigned int j, count;
		struct wpabuf *groups = sta->sae->tmp->peer_rejected_groups;

		res = os_snprintf(buf + len, buflen - len,
				  "sae_rejected_groups=");
		if (!os_snprintf_error(buflen - len, res))
			len += res;

		if (groups) {
			pos = wpabuf_head(groups);
			count = wpabuf_len(groups) / 2;
		} else {
			pos = NULL;
			count = 0;
		}
		for (j = 0; pos && j < count; j++) {
			res = os_snprintf(buf + len, buflen - len, "%s%d",
					  j == 0 ? "" : " ", WPA_GET_LE16(pos));
			if (!os_snprintf_error(buflen - len, res))
				len += res;
			pos += 2;
		}

		res = os_snprintf(buf + len, buflen - len, "\n");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}
#endif /* CONFIG_SAE */

	if (sta->vlan_id > 0) {
		res = os_snprintf(buf + len, buflen - len, "vlan_id=%d\n",
				  sta->vlan_id);
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}

	res = mbo_ap_get_info(sta, buf + len, buflen - len);
	if (res >= 0)
		len += res;

	if (sta->supp_op_classes &&
	    buflen - len > (unsigned) (17 + 2 * sta->supp_op_classes[0])) {
		res = os_snprintf(buf + len, buflen - len, "supp_op_classes=");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
		len += wpa_snprintf_hex(buf + len, buflen - len,
					sta->supp_op_classes + 1,
					sta->supp_op_classes[0]);
		res = os_snprintf(buf + len, buflen - len, "\n");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}

	if (sta->power_capab) {
		ret = os_snprintf(buf + len, buflen - len,
				  "min_txpower=%d\n"
				  "max_txpower=%d\n",
				  sta->min_tx_power, sta->max_tx_power);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

#ifdef CONFIG_IEEE80211AX
	if ((sta->flags & WLAN_STA_HE) && sta->he_capab) {
		res = os_snprintf(buf + len, buflen - len, "he_capab=");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
		len += wpa_snprintf_hex(buf + len, buflen - len,
					(const u8 *) sta->he_capab,
					sta->he_capab_len);
		res = os_snprintf(buf + len, buflen - len, "\n");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}
#endif /* CONFIG_IEEE80211AX */

#ifdef CONFIG_IEEE80211BE
	if ((sta->flags & WLAN_STA_EHT) && sta->eht_capab) {
		res = os_snprintf(buf + len, buflen - len, "eht_capab=");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
		len += wpa_snprintf_hex(buf + len, buflen - len,
					(const u8 *) sta->eht_capab,
					sta->eht_capab_len);
		res = os_snprintf(buf + len, buflen - len, "\n");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211AC
	if ((sta->flags & WLAN_STA_VHT) && sta->vht_capabilities) {
		res = os_snprintf(buf + len, buflen - len,
				  "vht_caps_info=0x%08x\n",
				  le_to_host32(sta->vht_capabilities->
					       vht_capabilities_info));
		if (!os_snprintf_error(buflen - len, res))
			len += res;

		res = os_snprintf(buf + len, buflen - len, "vht_capab=");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
		len += wpa_snprintf_hex(buf + len, buflen - len,
					(const u8 *) sta->vht_capabilities,
					sizeof(*sta->vht_capabilities));
		res = os_snprintf(buf + len, buflen - len, "\n");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}
#endif /* CONFIG_IEEE80211AC */

	if ((sta->flags & WLAN_STA_HT) && sta->ht_capabilities) {
		res = os_snprintf(buf + len, buflen - len,
				  "ht_caps_info=0x%04x\n",
				  le_to_host16(sta->ht_capabilities->
					       ht_capabilities_info));
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}

	if (sta->ext_capability &&
	    buflen - len > (unsigned) (11 + 2 * sta->ext_capability[0])) {
		res = os_snprintf(buf + len, buflen - len, "ext_capab=");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
		len += wpa_snprintf_hex(buf + len, buflen - len,
					sta->ext_capability + 1,
					sta->ext_capability[0]);
		res = os_snprintf(buf + len, buflen - len, "\n");
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}

	if (sta->flags & WLAN_STA_WDS && sta->ifname_wds) {
		ret = os_snprintf(buf + len, buflen - len,
				  "wds_sta_ifname=%s\n", sta->ifname_wds);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

	keyid = ap_sta_wpa_get_keyid(hapd, sta);
	if (keyid) {
		ret = os_snprintf(buf + len, buflen - len, "keyid=%s\n", keyid);
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

	dpp_pkhash = ap_sta_wpa_get_dpp_pkhash(hapd, sta);
	if (dpp_pkhash) {
		ret = os_snprintf(buf + len, buflen - len, "dpp_pkhash=");
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
		len += wpa_snprintf_hex(buf + len, buflen - len, dpp_pkhash,
					SHA256_MAC_LEN);
		ret = os_snprintf(buf + len, buflen - len, "\n");
		if (!os_snprintf_error(buflen - len, ret))
			len += ret;
	}

#ifdef CONFIG_IEEE80211BE
	if (sta->mld_info.mld_sta) {
		for (i = 0; i < MAX_NUM_MLD_LINKS; ++i) {
			if (!sta->mld_info.links[i].valid)
				continue;
			ret = os_snprintf(
				buf + len, buflen - len,
				"peer_addr[%d]=" MACSTR "\n",
				i, MAC2STR(sta->mld_info.links[i].peer_addr));
			if (!os_snprintf_error(buflen - len, ret))
				len += ret;
		}
	}
#endif /* CONFIG_IEEE80211BE */

	return len;
}


int hostapd_ctrl_iface_sta_first(struct hostapd_data *hapd,
				 char *buf, size_t buflen)
{
	return hostapd_ctrl_iface_sta_mib(hapd, hapd->sta_list, buf, buflen);
}


int hostapd_ctrl_iface_sta(struct hostapd_data *hapd, const char *txtaddr,
			   char *buf, size_t buflen)
{
	u8 addr[ETH_ALEN];
	int ret;
	const char *pos;
	struct sta_info *sta;

	if (hwaddr_aton(txtaddr, addr)) {
		ret = os_snprintf(buf, buflen, "FAIL\n");
		if (os_snprintf_error(buflen, ret))
			return 0;
		return ret;
	}

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL)
		return -1;

	pos = os_strchr(txtaddr, ' ');
	if (pos) {
		pos++;

#ifdef HOSTAPD_DUMP_STATE
		if (os_strcmp(pos, "eapol") == 0) {
			if (sta->eapol_sm == NULL)
				return -1;
			return eapol_auth_dump_state(sta->eapol_sm, buf,
						     buflen);
		}
#endif /* HOSTAPD_DUMP_STATE */

		return -1;
	}

	ret = hostapd_ctrl_iface_sta_mib(hapd, sta, buf, buflen);
	ret += fst_ctrl_iface_mb_info(addr, buf + ret, buflen - ret);

	return ret;
}


int hostapd_ctrl_iface_sta_next(struct hostapd_data *hapd, const char *txtaddr,
				char *buf, size_t buflen)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	int ret;

	if (hwaddr_aton(txtaddr, addr) ||
	    (sta = ap_get_sta(hapd, addr)) == NULL) {
		ret = os_snprintf(buf, buflen, "FAIL\n");
		if (os_snprintf_error(buflen, ret))
			return 0;
		return ret;
	}

	if (!sta->next)
		return 0;

	return hostapd_ctrl_iface_sta_mib(hapd, sta->next, buf, buflen);
}


#ifdef CONFIG_P2P_MANAGER
static int p2p_manager_disconnect(struct hostapd_data *hapd, u16 stype,
				  u8 minor_reason_code, const u8 *addr)
{
	struct ieee80211_mgmt *mgmt;
	int ret;
	u8 *pos;

	mgmt = os_zalloc(sizeof(*mgmt) + 100);
	if (mgmt == NULL)
		return -1;

	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT, stype);
	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "P2P: Disconnect STA " MACSTR
		" with minor reason code %u (stype=%u (%s))",
		MAC2STR(addr), minor_reason_code, stype,
		fc2str(le_to_host16(mgmt->frame_control)));

	os_memcpy(mgmt->da, addr, ETH_ALEN);
	os_memcpy(mgmt->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, hapd->own_addr, ETH_ALEN);
	if (stype == WLAN_FC_STYPE_DEAUTH) {
		mgmt->u.deauth.reason_code =
			host_to_le16(WLAN_REASON_PREV_AUTH_NOT_VALID);
		pos = mgmt->u.deauth.variable;
	} else {
		mgmt->u.disassoc.reason_code =
			host_to_le16(WLAN_REASON_PREV_AUTH_NOT_VALID);
		pos = mgmt->u.disassoc.variable;
	}

	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	*pos++ = 4 + 3 + 1;
	WPA_PUT_BE32(pos, P2P_IE_VENDOR_TYPE);
	pos += 4;

	*pos++ = P2P_ATTR_MINOR_REASON_CODE;
	WPA_PUT_LE16(pos, 1);
	pos += 2;
	*pos++ = minor_reason_code;

	ret = hostapd_drv_send_mlme(hapd, mgmt, pos - (u8 *) mgmt, 0, NULL, 0,
				    0);
	os_free(mgmt);

	return ret < 0 ? -1 : 0;
}
#endif /* CONFIG_P2P_MANAGER */


int hostapd_ctrl_iface_deauthenticate(struct hostapd_data *hapd,
				      const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	const char *pos;
	u16 reason = WLAN_REASON_PREV_AUTH_NOT_VALID;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "CTRL_IFACE DEAUTHENTICATE %s",
		txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	pos = os_strstr(txtaddr, " reason=");
	if (pos)
		reason = atoi(pos + 8);

	pos = os_strstr(txtaddr, " test=");
	if (pos) {
		struct ieee80211_mgmt mgmt;
		int encrypt;

		pos += 6;
		encrypt = atoi(pos);
		os_memset(&mgmt, 0, sizeof(mgmt));
		mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
						  WLAN_FC_STYPE_DEAUTH);
		os_memcpy(mgmt.da, addr, ETH_ALEN);
		os_memcpy(mgmt.sa, hapd->own_addr, ETH_ALEN);
		os_memcpy(mgmt.bssid, hapd->own_addr, ETH_ALEN);
		mgmt.u.deauth.reason_code = host_to_le16(reason);
		if (hostapd_drv_send_mlme(hapd, (u8 *) &mgmt,
					  IEEE80211_HDRLEN +
					  sizeof(mgmt.u.deauth),
					  0, NULL, 0, !encrypt) < 0)
			return -1;
		return 0;
	}

#ifdef CONFIG_P2P_MANAGER
	pos = os_strstr(txtaddr, " p2p=");
	if (pos) {
		return p2p_manager_disconnect(hapd, WLAN_FC_STYPE_DEAUTH,
					      atoi(pos + 5), addr);
	}
#endif /* CONFIG_P2P_MANAGER */

	if (os_strstr(txtaddr, " tx=0"))
		hostapd_drv_sta_remove(hapd, addr);
	else
		hostapd_drv_sta_deauth(hapd, addr, reason);
	sta = ap_get_sta(hapd, addr);
	if (sta)
		ap_sta_deauthenticate(hapd, sta, reason);
	else if (addr[0] == 0xff)
		hostapd_free_stas(hapd);

	return 0;
}


int hostapd_ctrl_iface_disassociate(struct hostapd_data *hapd,
				    const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	const char *pos;
	u16 reason = WLAN_REASON_PREV_AUTH_NOT_VALID;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "CTRL_IFACE DISASSOCIATE %s",
		txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	pos = os_strstr(txtaddr, " reason=");
	if (pos)
		reason = atoi(pos + 8);

	pos = os_strstr(txtaddr, " test=");
	if (pos) {
		struct ieee80211_mgmt mgmt;
		int encrypt;

		pos += 6;
		encrypt = atoi(pos);
		os_memset(&mgmt, 0, sizeof(mgmt));
		mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
						  WLAN_FC_STYPE_DISASSOC);
		os_memcpy(mgmt.da, addr, ETH_ALEN);
		os_memcpy(mgmt.sa, hapd->own_addr, ETH_ALEN);
		os_memcpy(mgmt.bssid, hapd->own_addr, ETH_ALEN);
		mgmt.u.disassoc.reason_code = host_to_le16(reason);
		if (hostapd_drv_send_mlme(hapd, (u8 *) &mgmt,
					  IEEE80211_HDRLEN +
					  sizeof(mgmt.u.deauth),
					  0, NULL, 0, !encrypt) < 0)
			return -1;
		return 0;
	}

#ifdef CONFIG_P2P_MANAGER
	pos = os_strstr(txtaddr, " p2p=");
	if (pos) {
		return p2p_manager_disconnect(hapd, WLAN_FC_STYPE_DISASSOC,
					      atoi(pos + 5), addr);
	}
#endif /* CONFIG_P2P_MANAGER */

	if (os_strstr(txtaddr, " tx=0"))
		hostapd_drv_sta_remove(hapd, addr);
	else
		hostapd_drv_sta_disassoc(hapd, addr, reason);
	sta = ap_get_sta(hapd, addr);
	if (sta)
		ap_sta_disassociate(hapd, sta, reason);
	else if (addr[0] == 0xff)
		hostapd_free_stas(hapd);

	return 0;
}


#ifdef CONFIG_TAXONOMY
int hostapd_ctrl_iface_signature(struct hostapd_data *hapd,
				 const char *txtaddr,
				 char *buf, size_t buflen)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "CTRL_IFACE SIGNATURE %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta)
		return -1;

	return retrieve_sta_taxonomy(hapd, sta, buf, buflen);
}
#endif /* CONFIG_TAXONOMY */


int hostapd_ctrl_iface_poll_sta(struct hostapd_data *hapd,
				const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "CTRL_IFACE POLL_STA %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta)
		return -1;

	hostapd_drv_poll_client(hapd, hapd->own_addr, addr,
				sta->flags & WLAN_STA_WMM);
	return 0;
}


int hostapd_ctrl_iface_status(struct hostapd_data *hapd, char *buf,
			      size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_hw_modes *mode = iface->current_mode;
	struct hostapd_config *iconf = hapd->iconf;
	int len = 0, ret, j;
	size_t i;

	ret = os_snprintf(buf + len, buflen - len,
			  "state=%s\n"
			  "phy=%s\n"
			  "freq=%d\n"
			  "num_sta_non_erp=%d\n"
			  "num_sta_no_short_slot_time=%d\n"
			  "num_sta_no_short_preamble=%d\n"
			  "olbc=%d\n"
			  "num_sta_ht_no_gf=%d\n"
			  "num_sta_no_ht=%d\n"
			  "num_sta_ht_20_mhz=%d\n"
			  "num_sta_ht40_intolerant=%d\n"
			  "olbc_ht=%d\n"
			  "ht_op_mode=0x%x\n",
			  hostapd_state_text(iface->state),
			  iface->phy,
			  iface->freq,
			  iface->num_sta_non_erp,
			  iface->num_sta_no_short_slot_time,
			  iface->num_sta_no_short_preamble,
			  iface->olbc,
			  iface->num_sta_ht_no_gf,
			  iface->num_sta_no_ht,
			  iface->num_sta_ht_20mhz,
			  iface->num_sta_ht40_intolerant,
			  iface->olbc_ht,
			  iface->ht_op_mode);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	if (mode) {
		ret = os_snprintf(buf + len, buflen - len, "hw_mode=%s\n",
				  hw_mode_str(mode->mode));
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	if (iconf->country[0] && iconf->country[1]) {
		ret = os_snprintf(buf + len, buflen - len,
				  "country_code=%c%c\ncountry3=0x%X\n",
				  iconf->country[0], iconf->country[1],
				  iconf->country[2]);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	if (!iface->cac_started || !iface->dfs_cac_ms) {
		ret = os_snprintf(buf + len, buflen - len,
				  "cac_time_seconds=%d\n"
				  "cac_time_left_seconds=N/A\n",
				  iface->dfs_cac_ms / 1000);
	} else {
		/* CAC started and CAC time set - calculate remaining time */
		struct os_reltime now;
		long left_time;

		os_reltime_age(&iface->dfs_cac_start, &now);
		left_time = (long) iface->dfs_cac_ms / 1000 - now.sec;
		ret = os_snprintf(buf + len, buflen - len,
				  "cac_time_seconds=%u\n"
				  "cac_time_left_seconds=%lu\n",
				  iface->dfs_cac_ms / 1000,
				  left_time > 0 ? left_time : 0);
	}
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	ret = os_snprintf(buf + len, buflen - len,
			  "channel=%u\n"
			  "edmg_enable=%d\n"
			  "edmg_channel=%d\n"
			  "secondary_channel=%d\n"
			  "ieee80211n=%d\n"
			  "ieee80211ac=%d\n"
			  "ieee80211ax=%d\n"
			  "ieee80211be=%d\n"
			  "beacon_int=%u\n"
			  "dtim_period=%d\n",
			  iface->conf->channel,
			  iface->conf->enable_edmg,
			  iface->conf->edmg_channel,
			  iface->conf->ieee80211n && !hapd->conf->disable_11n ?
			  iface->conf->secondary_channel : 0,
			  iface->conf->ieee80211n && !hapd->conf->disable_11n,
			  iface->conf->ieee80211ac &&
			  !hapd->conf->disable_11ac,
			  iface->conf->ieee80211ax &&
			  !hapd->conf->disable_11ax,
			  iface->conf->ieee80211be &&
			  !hapd->conf->disable_11be,
			  iface->conf->beacon_int,
			  hapd->conf->dtim_period);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

#ifdef CONFIG_IEEE80211BE
	if (iface->conf->ieee80211be && !hapd->conf->disable_11be) {
		ret = os_snprintf(buf + len, buflen - len,
				  "eht_oper_chwidth=%d\n"
				  "eht_oper_centr_freq_seg0_idx=%d\n",
				  iface->conf->eht_oper_chwidth,
				  iface->conf->eht_oper_centr_freq_seg0_idx);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		if (is_6ghz_op_class(iface->conf->op_class) &&
		    hostapd_get_oper_chwidth(iface->conf) ==
		    CONF_OPER_CHWIDTH_320MHZ) {
			ret = os_snprintf(buf + len, buflen - len,
					  "eht_bw320_offset=%d\n",
					  iface->conf->eht_bw320_offset);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}

		if (hapd->conf->mld_ap) {
			struct hostapd_data *link_bss;

			ret = os_snprintf(buf + len, buflen - len,
					  "num_links=%d\n",
					  hapd->mld->num_links);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;

			/* Self BSS */
			ret = os_snprintf(buf + len, buflen - len,
					  "link_id=%d\n"
					  "link_addr=" MACSTR "\n",
					  hapd->mld_link_id,
					  MAC2STR(hapd->own_addr));
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;

			/* Partner BSSs */
			for_each_mld_link(link_bss, hapd) {
				if (link_bss == hapd)
					continue;

				ret = os_snprintf(buf + len, buflen - len,
						  "partner_link[%d]=" MACSTR
						  "\n",
						  link_bss->mld_link_id,
						  MAC2STR(link_bss->own_addr));
				if (os_snprintf_error(buflen - len, ret))
					return len;
				len += ret;
			}
		}
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211AX
	if (iface->conf->ieee80211ax && !hapd->conf->disable_11ax) {
		ret = os_snprintf(buf + len, buflen - len,
				  "he_oper_chwidth=%d\n"
				  "he_oper_centr_freq_seg0_idx=%d\n"
				  "he_oper_centr_freq_seg1_idx=%d\n",
				  iface->conf->he_oper_chwidth,
				  iface->conf->he_oper_centr_freq_seg0_idx,
				  iface->conf->he_oper_centr_freq_seg1_idx);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		if (!iconf->he_op.he_bss_color_disabled &&
		    iconf->he_op.he_bss_color) {
			ret = os_snprintf(buf + len, buflen - len,
					  "he_bss_color=%d\n",
					  iconf->he_op.he_bss_color);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}
	}
#endif /* CONFIG_IEEE80211AX */

	if (iface->conf->ieee80211ac && !hapd->conf->disable_11ac) {
		ret = os_snprintf(buf + len, buflen - len,
				  "vht_oper_chwidth=%d\n"
				  "vht_oper_centr_freq_seg0_idx=%d\n"
				  "vht_oper_centr_freq_seg1_idx=%d\n"
				  "vht_caps_info=%08x\n",
				  iface->conf->vht_oper_chwidth,
				  iface->conf->vht_oper_centr_freq_seg0_idx,
				  iface->conf->vht_oper_centr_freq_seg1_idx,
				  iface->conf->vht_capab);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	if (iface->conf->ieee80211ac && !hapd->conf->disable_11ac && mode) {
		u16 rxmap = WPA_GET_LE16(&mode->vht_mcs_set[0]);
		u16 txmap = WPA_GET_LE16(&mode->vht_mcs_set[4]);

		ret = os_snprintf(buf + len, buflen - len,
				  "rx_vht_mcs_map=%04x\n"
				  "tx_vht_mcs_map=%04x\n",
				  rxmap, txmap);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	if (iface->conf->ieee80211n && !hapd->conf->disable_11n) {
		ret = os_snprintf(buf + len, buflen - len,
				  "ht_caps_info=%04x\n",
				  hapd->iconf->ht_capab);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	if (iface->conf->ieee80211n && !hapd->conf->disable_11n && mode) {
		len = hostapd_write_ht_mcs_bitmask(buf, buflen, len,
						   mode->mcs_set);
	}

	if (iface->current_rates && iface->num_rates) {
		ret = os_snprintf(buf + len, buflen - len, "supported_rates=");
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

		for (j = 0; j < iface->num_rates; j++) {
			ret = os_snprintf(buf + len, buflen - len, "%s%02x",
					  j > 0 ? " " : "",
					  iface->current_rates[j].rate / 5);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}
		ret = os_snprintf(buf + len, buflen - len, "\n");
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	for (j = 0; mode && j < mode->num_channels; j++) {
		if (mode->channels[j].freq == iface->freq) {
			ret = os_snprintf(buf + len, buflen - len,
					  "max_txpower=%u\n",
					  mode->channels[j].max_tx_power);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
			break;
		}
	}

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *bss = iface->bss[i];
		ret = os_snprintf(buf + len, buflen - len,
				  "bss[%d]=%s\n"
				  "bssid[%d]=" MACSTR "\n"
				  "ssid[%d]=%s\n"
				  "num_sta[%d]=%d\n",
				  (int) i, bss->conf->iface,
				  (int) i, MAC2STR(bss->own_addr),
				  (int) i,
				  wpa_ssid_txt(bss->conf->ssid.ssid,
					       bss->conf->ssid.ssid_len),
				  (int) i, bss->num_sta);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;

#ifdef CONFIG_IEEE80211BE
		if (bss->conf->mld_ap) {
			ret = os_snprintf(buf + len, buflen - len,
					  "mld_addr[%d]=" MACSTR "\n"
					  "mld_id[%d]=%d\n"
					  "mld_link_id[%d]=%d\n",
					  (int) i, MAC2STR(bss->mld->mld_addr),
					  (int) i, hostapd_get_mld_id(bss),
					  (int) i, bss->mld_link_id);
			if (os_snprintf_error(buflen - len, ret))
				return len;
			len += ret;
		}
#endif /* CONFIG_IEEE80211BE */
	}

	if (hapd->conf->chan_util_avg_period) {
		ret = os_snprintf(buf + len, buflen - len,
				  "chan_util_avg=%u\n",
				  iface->chan_util_average);
		if (os_snprintf_error(buflen - len, ret))
			return len;
		len += ret;
	}

	return len;
}


int hostapd_parse_csa_settings(const char *pos,
			       struct csa_settings *settings)
{
	char *end;

	os_memset(settings, 0, sizeof(*settings));
	settings->cs_count = strtol(pos, &end, 10);
	if (pos == end) {
		wpa_printf(MSG_ERROR, "chanswitch: invalid cs_count provided");
		return -1;
	}

	settings->freq_params.freq = atoi(end);
	if (settings->freq_params.freq == 0) {
		wpa_printf(MSG_ERROR, "chanswitch: invalid freq provided");
		return -1;
	}

#define SET_CSA_SETTING(str) \
	do { \
		const char *pos2 = os_strstr(pos, " " #str "="); \
		if (pos2) { \
			pos2 += sizeof(" " #str "=") - 1; \
			settings->freq_params.str = atoi(pos2); \
		} \
	} while (0)

#define SET_CSA_SETTING_EXT(str) \
	do { \
		const char *pos2 = os_strstr(pos, " " #str "="); \
		if (pos2) { \
			pos2 += sizeof(" " #str "=") - 1; \
			settings->str = atoi(pos2); \
		} \
	} while (0)

	SET_CSA_SETTING(center_freq1);
	SET_CSA_SETTING(center_freq2);
	SET_CSA_SETTING(bandwidth);
	SET_CSA_SETTING(sec_channel_offset);
	SET_CSA_SETTING_EXT(punct_bitmap);
	settings->freq_params.ht_enabled = !!os_strstr(pos, " ht");
	settings->freq_params.vht_enabled = !!os_strstr(pos, " vht");
	settings->freq_params.he_enabled = !!os_strstr(pos, " he");
	settings->freq_params.eht_enabled = !!os_strstr(pos, " eht");
	settings->block_tx = !!os_strstr(pos, " blocktx");
#undef SET_CSA_SETTING
#undef SET_CSA_SETTING_EXT

	return 0;
}


int hostapd_ctrl_iface_stop_ap(struct hostapd_data *hapd)
{
	return hostapd_drv_stop_ap(hapd);
}


int hostapd_ctrl_iface_pmksa_list(struct hostapd_data *hapd, char *buf,
				  size_t len)
{
	return wpa_auth_pmksa_list(hapd->wpa_auth, buf, len);
}


void hostapd_ctrl_iface_pmksa_flush(struct hostapd_data *hapd)
{
	wpa_auth_pmksa_flush(hapd->wpa_auth);
}


int hostapd_ctrl_iface_pmksa_add(struct hostapd_data *hapd, char *cmd)
{
	u8 spa[ETH_ALEN];
	u8 pmkid[PMKID_LEN];
	u8 pmk[PMK_LEN_MAX];
	size_t pmk_len;
	char *pos, *pos2;
	int akmp = 0, expiration = 0;

	/*
	 * Entry format:
	 * <STA addr> <PMKID> <PMK> <expiration in seconds> <akmp>
	 */

	if (hwaddr_aton(cmd, spa))
		return -1;

	pos = os_strchr(cmd, ' ');
	if (!pos)
		return -1;
	pos++;

	if (hexstr2bin(pos, pmkid, PMKID_LEN) < 0)
		return -1;

	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;

	pos2 = os_strchr(pos, ' ');
	if (!pos2)
		return -1;
	pmk_len = (pos2 - pos) / 2;
	if (pmk_len < PMK_LEN || pmk_len > PMK_LEN_MAX ||
	    hexstr2bin(pos, pmk, pmk_len) < 0)
		return -1;

	pos = pos2 + 1;

	if (sscanf(pos, "%d %d", &expiration, &akmp) != 2)
		return -1;

	return wpa_auth_pmksa_add2(hapd->wpa_auth, spa, pmk, pmk_len,
				   pmkid, expiration, akmp, NULL);
}


#ifdef CONFIG_PMKSA_CACHE_EXTERNAL
#ifdef CONFIG_MESH

int hostapd_ctrl_iface_pmksa_list_mesh(struct hostapd_data *hapd,
				       const u8 *addr, char *buf, size_t len)
{
	return wpa_auth_pmksa_list_mesh(hapd->wpa_auth, addr, buf, len);
}


void * hostapd_ctrl_iface_pmksa_create_entry(const u8 *aa, char *cmd)
{
	u8 spa[ETH_ALEN];
	u8 pmkid[PMKID_LEN];
	u8 pmk[PMK_LEN_MAX];
	char *pos;
	int expiration;

	/*
	 * Entry format:
	 * <BSSID> <PMKID> <PMK> <expiration in seconds>
	 */

	if (hwaddr_aton(cmd, spa))
		return NULL;

	pos = os_strchr(cmd, ' ');
	if (!pos)
		return NULL;
	pos++;

	if (hexstr2bin(pos, pmkid, PMKID_LEN) < 0)
		return NULL;

	pos = os_strchr(pos, ' ');
	if (!pos)
		return NULL;
	pos++;

	if (hexstr2bin(pos, pmk, PMK_LEN) < 0)
		return NULL;

	pos = os_strchr(pos, ' ');
	if (!pos)
		return NULL;
	pos++;

	if (sscanf(pos, "%d", &expiration) != 1)
		return NULL;

	return wpa_auth_pmksa_create_entry(aa, spa, pmk, PMK_LEN,
					   WPA_KEY_MGMT_SAE, pmkid, expiration);
}

#endif /* CONFIG_MESH */
#endif /* CONFIG_PMKSA_CACHE_EXTERNAL */


#ifdef CONFIG_WNM_AP

int hostapd_ctrl_iface_disassoc_imminent(struct hostapd_data *hapd,
					 const char *cmd)
{
	u8 addr[ETH_ALEN];
	int disassoc_timer;
	struct sta_info *sta;

	if (hwaddr_aton(cmd, addr))
		return -1;
	if (cmd[17] != ' ')
		return -1;
	disassoc_timer = atoi(cmd + 17);

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR
			   " not found for disassociation imminent message",
			   MAC2STR(addr));
		return -1;
	}

	return wnm_send_disassoc_imminent(hapd, sta, disassoc_timer);
}


int hostapd_ctrl_iface_ess_disassoc(struct hostapd_data *hapd,
				    const char *cmd)
{
	u8 addr[ETH_ALEN];
	const char *url, *timerstr;
	int disassoc_timer;
	struct sta_info *sta;

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR
			   " not found for ESS disassociation imminent message",
			   MAC2STR(addr));
		return -1;
	}

	timerstr = cmd + 17;
	if (*timerstr != ' ')
		return -1;
	timerstr++;
	disassoc_timer = atoi(timerstr);
	if (disassoc_timer < 0 || disassoc_timer > 65535)
		return -1;

	url = os_strchr(timerstr, ' ');
	if (url == NULL)
		return -1;
	url++;

	return wnm_send_ess_disassoc_imminent(hapd, sta, url, disassoc_timer);
}


int hostapd_ctrl_iface_bss_tm_req(struct hostapd_data *hapd,
				  const char *cmd)
{
	u8 addr[ETH_ALEN];
	const char *pos, *end;
	int disassoc_timer = 0;
	struct sta_info *sta;
	u8 req_mode = 0, valid_int = 0x01, dialog_token = 0x01;
	u8 bss_term_dur[12];
	char *url = NULL;
	int ret;
	u8 nei_rep[1000];
	int nei_len;
	u8 mbo[10];
	size_t mbo_len = 0;

	if (hwaddr_aton(cmd, addr)) {
		wpa_printf(MSG_DEBUG, "Invalid STA MAC address");
		return -1;
	}

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR
			   " not found for BSS TM Request message",
			   MAC2STR(addr));
		return -1;
	}

	pos = os_strstr(cmd, " disassoc_timer=");
	if (pos) {
		pos += 16;
		disassoc_timer = atoi(pos);
		if (disassoc_timer < 0 || disassoc_timer > 65535) {
			wpa_printf(MSG_DEBUG, "Invalid disassoc_timer");
			return -1;
		}
	}

	pos = os_strstr(cmd, " valid_int=");
	if (pos) {
		pos += 11;
		valid_int = atoi(pos);
	}

	pos = os_strstr(cmd, " dialog_token=");
	if (pos) {
		pos += 14;
		dialog_token = atoi(pos);
	}

	pos = os_strstr(cmd, " bss_term=");
	if (pos) {
		pos += 10;
		req_mode |= WNM_BSS_TM_REQ_BSS_TERMINATION_INCLUDED;
		/* TODO: TSF configurable/learnable */
		bss_term_dur[0] = 4; /* Subelement ID */
		bss_term_dur[1] = 10; /* Length */
		os_memset(&bss_term_dur[2], 0, 8);
		end = os_strchr(pos, ',');
		if (end == NULL) {
			wpa_printf(MSG_DEBUG, "Invalid bss_term data");
			return -1;
		}
		end++;
		WPA_PUT_LE16(&bss_term_dur[10], atoi(end));
	}

	nei_len = ieee802_11_parse_candidate_list(cmd, nei_rep,
						  sizeof(nei_rep));
	if (nei_len < 0)
		return -1;

	pos = os_strstr(cmd, " url=");
	if (pos) {
		size_t len;
		pos += 5;
		end = os_strchr(pos, ' ');
		if (end)
			len = end - pos;
		else
			len = os_strlen(pos);
		url = os_malloc(len + 1);
		if (url == NULL)
			return -1;
		os_memcpy(url, pos, len);
		url[len] = '\0';
		req_mode |= WNM_BSS_TM_REQ_ESS_DISASSOC_IMMINENT;
	}

	if (os_strstr(cmd, " pref=1"))
		req_mode |= WNM_BSS_TM_REQ_PREF_CAND_LIST_INCLUDED;
	if (os_strstr(cmd, " abridged=1"))
		req_mode |= WNM_BSS_TM_REQ_ABRIDGED;
	if (os_strstr(cmd, " disassoc_imminent=1"))
		req_mode |= WNM_BSS_TM_REQ_DISASSOC_IMMINENT;
	if (os_strstr(cmd, " link_removal_imminent=1"))
		req_mode |= WNM_BSS_TM_REQ_LINK_REMOVAL_IMMINENT;

#ifdef CONFIG_MBO
	pos = os_strstr(cmd, "mbo=");
	if (pos) {
		unsigned int mbo_reason, cell_pref, reassoc_delay;
		u8 *mbo_pos = mbo;

		ret = sscanf(pos, "mbo=%u:%u:%u", &mbo_reason,
			     &reassoc_delay, &cell_pref);
		if (ret != 3) {
			wpa_printf(MSG_DEBUG,
				   "MBO requires three arguments: mbo=<reason>:<reassoc_delay>:<cell_pref>");
			ret = -1;
			goto fail;
		}

		if (mbo_reason > MBO_TRANSITION_REASON_PREMIUM_AP) {
			wpa_printf(MSG_DEBUG,
				   "Invalid MBO transition reason code %u",
				   mbo_reason);
			ret = -1;
			goto fail;
		}

		/* Valid values for Cellular preference are: 0, 1, 255 */
		if (cell_pref != 0 && cell_pref != 1 && cell_pref != 255) {
			wpa_printf(MSG_DEBUG,
				   "Invalid MBO cellular capability %u",
				   cell_pref);
			ret = -1;
			goto fail;
		}

		if (reassoc_delay > 65535 ||
		    (reassoc_delay &&
		     !(req_mode & WNM_BSS_TM_REQ_DISASSOC_IMMINENT))) {
			wpa_printf(MSG_DEBUG,
				   "MBO: Assoc retry delay is only valid in disassoc imminent mode");
			ret = -1;
			goto fail;
		}

		*mbo_pos++ = MBO_ATTR_ID_TRANSITION_REASON;
		*mbo_pos++ = 1;
		*mbo_pos++ = mbo_reason;
		*mbo_pos++ = MBO_ATTR_ID_CELL_DATA_PREF;
		*mbo_pos++ = 1;
		*mbo_pos++ = cell_pref;

		if (reassoc_delay) {
			*mbo_pos++ = MBO_ATTR_ID_ASSOC_RETRY_DELAY;
			*mbo_pos++ = 2;
			WPA_PUT_LE16(mbo_pos, reassoc_delay);
			mbo_pos += 2;
		}

		mbo_len = mbo_pos - mbo;
	}
#endif /* CONFIG_MBO */

	ret = wnm_send_bss_tm_req(hapd, sta, req_mode, disassoc_timer,
				  valid_int, bss_term_dur, dialog_token, url,
				  nei_len ? nei_rep : NULL, nei_len,
				  mbo_len ? mbo : NULL, mbo_len);
#ifdef CONFIG_MBO
fail:
#endif /* CONFIG_MBO */
	os_free(url);
	return ret;
}

#endif /* CONFIG_WNM_AP */


int hostapd_ctrl_iface_acl_del_mac(struct mac_acl_entry **acl, int *num,
				   const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	struct vlan_description vlan_id;

	if (!(*num))
		return 0;

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	if (hostapd_maclist_found(*acl, *num, addr, &vlan_id))
		hostapd_remove_acl_mac(acl, num, addr);

	return 0;
}


void hostapd_ctrl_iface_acl_clear_list(struct mac_acl_entry **acl,
				       int *num)
{
	while (*num)
		hostapd_remove_acl_mac(acl, num, (*acl)[0].addr);
}


int hostapd_ctrl_iface_acl_show_mac(struct mac_acl_entry *acl, int num,
				    char *buf, size_t buflen)
{
	int i = 0, len = 0, ret = 0;

	if (!acl)
		return 0;

	while (i < num) {
		ret = os_snprintf(buf + len, buflen - len,
				  MACSTR " VLAN_ID=%d\n",
				  MAC2STR(acl[i].addr),
				  acl[i].vlan_id.untagged);
		if (ret < 0 || (size_t) ret >= buflen - len)
			return len;
		i++;
		len += ret;
	}
	return len;
}


int hostapd_ctrl_iface_acl_add_mac(struct mac_acl_entry **acl, int *num,
				   const char *cmd)
{
	u8 addr[ETH_ALEN];
	struct vlan_description vlan_id;
	int ret = 0, vlanid = 0;
	const char *pos;

	if (hwaddr_aton(cmd, addr))
		return -1;

	pos = os_strstr(cmd, "VLAN_ID=");
	if (pos)
		vlanid = atoi(pos + 8);

	if (!hostapd_maclist_found(*acl, *num, addr, &vlan_id)) {
		ret = hostapd_add_acl_maclist(acl, num, vlanid, addr);
		if (ret != -1 && *acl)
			qsort(*acl, *num, sizeof(**acl), hostapd_acl_comp);
	}

	return ret < 0 ? -1 : 0;
}


int hostapd_disassoc_accept_mac(struct hostapd_data *hapd)
{
	struct sta_info *sta;
	struct vlan_description vlan_id;

	if (hapd->conf->macaddr_acl != DENY_UNLESS_ACCEPTED)
		return 0;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (!hostapd_maclist_found(hapd->conf->accept_mac,
					   hapd->conf->num_accept_mac,
					   sta->addr, &vlan_id) ||
		    (vlan_id.notempty &&
		     vlan_compare(&vlan_id, sta->vlan_desc)))
			ap_sta_disconnect(hapd, sta, sta->addr,
					  WLAN_REASON_UNSPECIFIED);
	}

	return 0;
}


int hostapd_disassoc_deny_mac(struct hostapd_data *hapd)
{
	struct sta_info *sta;
	struct vlan_description vlan_id;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (hostapd_maclist_found(hapd->conf->deny_mac,
					  hapd->conf->num_deny_mac, sta->addr,
					  &vlan_id) &&
		    (!vlan_id.notempty ||
		     !vlan_compare(&vlan_id, sta->vlan_desc)))
			ap_sta_disconnect(hapd, sta, sta->addr,
					  WLAN_REASON_UNSPECIFIED);
	}

	return 0;
}
