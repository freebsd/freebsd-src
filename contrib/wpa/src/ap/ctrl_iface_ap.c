/*
 * Control interface for shared AP commands
 * Copyright (c) 2004-2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_defs.h"
#include "hostapd.h"
#include "ieee802_1x.h"
#include "wpa_auth.h"
#include "ieee802_11.h"
#include "sta_info.h"
#include "wps_hostapd.h"
#include "p2p_hostapd.h"
#include "ctrl_iface_ap.h"
#include "ap_drv_ops.h"


static int hostapd_get_sta_conn_time(struct sta_info *sta,
				     char *buf, size_t buflen)
{
	struct os_time now, age;
	int len = 0, ret;

	if (!sta->connected_time.sec)
		return 0;

	os_get_time(&now);
	os_time_sub(&now, &sta->connected_time, &age);

	ret = os_snprintf(buf + len, buflen - len, "connected_time=%u\n",
			  (unsigned int) age.sec);
	if (ret < 0 || (size_t) ret >= buflen - len)
		return len;
	len += ret;

	return len;
}


static int hostapd_ctrl_iface_sta_mib(struct hostapd_data *hapd,
				      struct sta_info *sta,
				      char *buf, size_t buflen)
{
	int len, res, ret;

	if (sta == NULL) {
		ret = os_snprintf(buf, buflen, "FAIL\n");
		if (ret < 0 || (size_t) ret >= buflen)
			return 0;
		return ret;
	}

	len = 0;
	ret = os_snprintf(buf + len, buflen - len, MACSTR "\n",
			  MAC2STR(sta->addr));
	if (ret < 0 || (size_t) ret >= buflen - len)
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

	res = hostapd_get_sta_conn_time(sta, buf + len, buflen - len);
	if (res >= 0)
		len += res;

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

	if (hwaddr_aton(txtaddr, addr)) {
		ret = os_snprintf(buf, buflen, "FAIL\n");
		if (ret < 0 || (size_t) ret >= buflen)
			return 0;
		return ret;
	}
	return hostapd_ctrl_iface_sta_mib(hapd, ap_get_sta(hapd, addr),
					  buf, buflen);
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
		if (ret < 0 || (size_t) ret >= buflen)
			return 0;
		return ret;
	}		
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

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "P2P: Disconnect STA " MACSTR
		" with minor reason code %u (stype=%u)",
		MAC2STR(addr), minor_reason_code, stype);

	mgmt->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT, stype);
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
	WPA_PUT_BE24(pos, OUI_WFA);
	pos += 3;
	*pos++ = P2P_OUI_TYPE;

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

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "CTRL_IFACE DEAUTHENTICATE %s",
		txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

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
		mgmt.u.deauth.reason_code =
			host_to_le16(WLAN_REASON_PREV_AUTH_NOT_VALID);
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

	hostapd_drv_sta_deauth(hapd, addr, WLAN_REASON_PREV_AUTH_NOT_VALID);
	sta = ap_get_sta(hapd, addr);
	if (sta)
		ap_sta_deauthenticate(hapd, sta,
				      WLAN_REASON_PREV_AUTH_NOT_VALID);
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

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "CTRL_IFACE DISASSOCIATE %s",
		txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

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
		mgmt.u.disassoc.reason_code =
			host_to_le16(WLAN_REASON_PREV_AUTH_NOT_VALID);
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

	hostapd_drv_sta_disassoc(hapd, addr, WLAN_REASON_PREV_AUTH_NOT_VALID);
	sta = ap_get_sta(hapd, addr);
	if (sta)
		ap_sta_disassociate(hapd, sta,
				    WLAN_REASON_PREV_AUTH_NOT_VALID);
	else if (addr[0] == 0xff)
		hostapd_free_stas(hapd);

	return 0;
}
