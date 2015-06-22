/*
 * Control interface for shared AP commands
 * Copyright (c) 2004-2014, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "common/sae.h"
#include "eapol_auth/eapol_auth_sm.h"
#include "hostapd.h"
#include "ieee802_1x.h"
#include "wpa_auth.h"
#include "ieee802_11.h"
#include "sta_info.h"
#include "wps_hostapd.h"
#include "p2p_hostapd.h"
#include "ctrl_iface_ap.h"
#include "ap_drv_ops.h"


static int hostapd_get_sta_tx_rx(struct hostapd_data *hapd,
				 struct sta_info *sta,
				 char *buf, size_t buflen)
{
	struct hostap_sta_driver_data data;
	int ret;

	if (hostapd_drv_read_sta_data(hapd, &data, sta->addr) < 0)
		return 0;

	ret = os_snprintf(buf, buflen, "rx_packets=%lu\ntx_packets=%lu\n"
			  "rx_bytes=%lu\ntx_bytes=%lu\n",
			  data.rx_packets, data.tx_packets,
			  data.rx_bytes, data.tx_bytes);
	if (os_snprintf_error(buflen, ret))
		return 0;
	return ret;
}


static int hostapd_get_sta_conn_time(struct sta_info *sta,
				     char *buf, size_t buflen)
{
	struct os_reltime age;
	int ret;

	if (!sta->connected_time.sec)
		return 0;

	os_reltime_age(&sta->connected_time, &age);

	ret = os_snprintf(buf, buflen, "connected_time=%u\n",
			  (unsigned int) age.sec);
	if (os_snprintf_error(buflen, ret))
		return 0;
	return ret;
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
	}

	return "?";
}


static int hostapd_ctrl_iface_sta_mib(struct hostapd_data *hapd,
				      struct sta_info *sta,
				      char *buf, size_t buflen)
{
	int len, res, ret, i;

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

	len += hostapd_get_sta_tx_rx(hapd, sta, buf + len, buflen - len);
	len += hostapd_get_sta_conn_time(sta, buf + len, buflen - len);

#ifdef CONFIG_SAE
	if (sta->sae && sta->sae->state == SAE_ACCEPTED) {
		res = os_snprintf(buf + len, buflen - len, "sae_group=%d\n",
				  sta->sae->group);
		if (!os_snprintf_error(buflen - len, res))
			len += res;
	}
#endif /* CONFIG_SAE */

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

	return hostapd_ctrl_iface_sta_mib(hapd, sta, buf, buflen);
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

	if (hapd->driver->send_frame == NULL)
		return -1;

	mgmt = os_zalloc(sizeof(*mgmt) + 100);
	if (mgmt == NULL)
		return -1;

	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT, stype);
	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "P2P: Disconnect STA " MACSTR
		" with minor reason code %u (stype=%u (%s))",
		MAC2STR(addr), minor_reason_code, stype,
		fc2str(mgmt->frame_control));

	os_memcpy(mgmt->da, addr, ETH_ALEN);
	os_memcpy(mgmt->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(mgmt->bssid, hapd->own_addr, ETH_ALEN);
	if (stype == WLAN_FC_STYPE_DEAUTH) {
		mgmt->u.deauth.reason_code =
			host_to_le16(WLAN_REASON_PREV_AUTH_NOT_VALID);
		pos = (u8 *) (&mgmt->u.deauth.reason_code + 1);
	} else {
		mgmt->u.disassoc.reason_code =
			host_to_le16(WLAN_REASON_PREV_AUTH_NOT_VALID);
		pos = (u8 *) (&mgmt->u.disassoc.reason_code + 1);
	}

	*pos++ = WLAN_EID_VENDOR_SPECIFIC;
	*pos++ = 4 + 3 + 1;
	WPA_PUT_BE32(pos, P2P_IE_VENDOR_TYPE);
	pos += 4;

	*pos++ = P2P_ATTR_MINOR_REASON_CODE;
	WPA_PUT_LE16(pos, 1);
	pos += 2;
	*pos++ = minor_reason_code;

	ret = hapd->driver->send_frame(hapd->drv_priv, (u8 *) mgmt,
				       pos - (u8 *) mgmt, 1);
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
		if (hapd->driver->send_frame == NULL)
			return -1;
		pos += 6;
		encrypt = atoi(pos);
		os_memset(&mgmt, 0, sizeof(mgmt));
		mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
						  WLAN_FC_STYPE_DEAUTH);
		os_memcpy(mgmt.da, addr, ETH_ALEN);
		os_memcpy(mgmt.sa, hapd->own_addr, ETH_ALEN);
		os_memcpy(mgmt.bssid, hapd->own_addr, ETH_ALEN);
		mgmt.u.deauth.reason_code = host_to_le16(reason);
		if (hapd->driver->send_frame(hapd->drv_priv, (u8 *) &mgmt,
					     IEEE80211_HDRLEN +
					     sizeof(mgmt.u.deauth),
					     encrypt) < 0)
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
		if (hapd->driver->send_frame == NULL)
			return -1;
		pos += 6;
		encrypt = atoi(pos);
		os_memset(&mgmt, 0, sizeof(mgmt));
		mgmt.frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
						  WLAN_FC_STYPE_DISASSOC);
		os_memcpy(mgmt.da, addr, ETH_ALEN);
		os_memcpy(mgmt.sa, hapd->own_addr, ETH_ALEN);
		os_memcpy(mgmt.bssid, hapd->own_addr, ETH_ALEN);
		mgmt.u.disassoc.reason_code = host_to_le16(reason);
		if (hapd->driver->send_frame(hapd->drv_priv, (u8 *) &mgmt,
					     IEEE80211_HDRLEN +
					     sizeof(mgmt.u.deauth),
					     encrypt) < 0)
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

	hostapd_drv_sta_disassoc(hapd, addr, reason);
	sta = ap_get_sta(hapd, addr);
	if (sta)
		ap_sta_disassociate(hapd, sta, reason);
	else if (addr[0] == 0xff)
		hostapd_free_stas(hapd);

	return 0;
}


int hostapd_ctrl_iface_status(struct hostapd_data *hapd, char *buf,
			      size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	int len = 0, ret;
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

	if (!iface->cac_started || !iface->dfs_cac_ms) {
		ret = os_snprintf(buf + len, buflen - len,
				  "cac_time_seconds=%d\n"
				  "cac_time_left_seconds=N/A\n",
				  iface->dfs_cac_ms / 1000);
	} else {
		/* CAC started and CAC time set - calculate remaining time */
		struct os_reltime now;
		unsigned int left_time;

		os_reltime_age(&iface->dfs_cac_start, &now);
		left_time = iface->dfs_cac_ms / 1000 - now.sec;
		ret = os_snprintf(buf + len, buflen - len,
				  "cac_time_seconds=%u\n"
				  "cac_time_left_seconds=%u\n",
				  iface->dfs_cac_ms / 1000,
				  left_time);
	}
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

	ret = os_snprintf(buf + len, buflen - len,
			  "channel=%u\n"
			  "secondary_channel=%d\n"
			  "ieee80211n=%d\n"
			  "ieee80211ac=%d\n"
			  "vht_oper_chwidth=%d\n"
			  "vht_oper_centr_freq_seg0_idx=%d\n"
			  "vht_oper_centr_freq_seg1_idx=%d\n",
			  iface->conf->channel,
			  iface->conf->secondary_channel,
			  iface->conf->ieee80211n,
			  iface->conf->ieee80211ac,
			  iface->conf->vht_oper_chwidth,
			  iface->conf->vht_oper_centr_freq_seg0_idx,
			  iface->conf->vht_oper_centr_freq_seg1_idx);
	if (os_snprintf_error(buflen - len, ret))
		return len;
	len += ret;

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

	SET_CSA_SETTING(center_freq1);
	SET_CSA_SETTING(center_freq2);
	SET_CSA_SETTING(bandwidth);
	SET_CSA_SETTING(sec_channel_offset);
	settings->freq_params.ht_enabled = !!os_strstr(pos, " ht");
	settings->freq_params.vht_enabled = !!os_strstr(pos, " vht");
	settings->block_tx = !!os_strstr(pos, " blocktx");
#undef SET_CSA_SETTING

	return 0;
}


int hostapd_ctrl_iface_stop_ap(struct hostapd_data *hapd)
{
	return hostapd_drv_stop_ap(hapd);
}
