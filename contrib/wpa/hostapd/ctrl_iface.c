/*
 * hostapd / UNIX domain socket -based control interface
 * Copyright (c) 2004-2018, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#ifndef CONFIG_NATIVE_WINDOWS

#ifdef CONFIG_TESTING_OPTIONS
#ifdef __NetBSD__
#include <net/if_ether.h>
#else
#include <net/ethernet.h>
#endif
#include <netinet/ip.h>
#endif /* CONFIG_TESTING_OPTIONS */

#include <sys/un.h>
#include <sys/stat.h>
#include <stddef.h>

#ifdef CONFIG_CTRL_IFACE_UDP
#include <netdb.h>
#endif /* CONFIG_CTRL_IFACE_UDP */

#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/module_tests.h"
#include "common/version.h"
#include "common/ieee802_11_defs.h"
#include "common/ctrl_iface_common.h"
#ifdef CONFIG_DPP
#include "common/dpp.h"
#endif /* CONFIG_DPP */
#include "common/wpa_ctrl.h"
#include "common/ptksa_cache.h"
#include "common/hw_features_common.h"
#include "common/nan_de.h"
#include "crypto/tls.h"
#include "drivers/driver.h"
#include "eapol_auth/eapol_auth_sm.h"
#include "radius/radius_client.h"
#include "radius/radius_server.h"
#include "l2_packet/l2_packet.h"
#include "ap/hostapd.h"
#include "ap/ap_config.h"
#include "ap/ieee802_1x.h"
#include "ap/wpa_auth.h"
#include "ap/pmksa_cache_auth.h"
#include "ap/ieee802_11.h"
#include "ap/sta_info.h"
#include "ap/wps_hostapd.h"
#include "ap/ctrl_iface_ap.h"
#include "ap/ap_drv_ops.h"
#include "ap/hs20.h"
#include "ap/wnm_ap.h"
#include "ap/wpa_auth.h"
#include "ap/beacon.h"
#include "ap/neighbor_db.h"
#include "ap/rrm.h"
#include "ap/dpp_hostapd.h"
#include "ap/dfs.h"
#include "ap/nan_usd_ap.h"
#include "wps/wps_defs.h"
#include "wps/wps.h"
#include "fst/fst_ctrl_iface.h"
#include "config_file.h"
#include "ctrl_iface.h"


#define HOSTAPD_CLI_DUP_VALUE_MAX_LEN 256

#ifdef CONFIG_CTRL_IFACE_UDP
#define HOSTAPD_CTRL_IFACE_PORT		8877
#define HOSTAPD_CTRL_IFACE_PORT_LIMIT	50
#define HOSTAPD_GLOBAL_CTRL_IFACE_PORT		8878
#define HOSTAPD_GLOBAL_CTRL_IFACE_PORT_LIMIT	50
#endif /* CONFIG_CTRL_IFACE_UDP */

static void hostapd_ctrl_iface_send(struct hostapd_data *hapd, int level,
				    enum wpa_msg_type type,
				    const char *buf, size_t len);


static int hostapd_ctrl_iface_attach(struct hostapd_data *hapd,
				     struct sockaddr_storage *from,
				     socklen_t fromlen, const char *input)
{
	return ctrl_iface_attach(&hapd->ctrl_dst, from, fromlen, input);
}


static int hostapd_ctrl_iface_detach(struct hostapd_data *hapd,
				     struct sockaddr_storage *from,
				     socklen_t fromlen)
{
	return ctrl_iface_detach(&hapd->ctrl_dst, from, fromlen);
}


static int hostapd_ctrl_iface_level(struct hostapd_data *hapd,
				    struct sockaddr_storage *from,
				    socklen_t fromlen,
				    char *level)
{
	return ctrl_iface_level(&hapd->ctrl_dst, from, fromlen, level);
}


static int hostapd_ctrl_iface_new_sta(struct hostapd_data *hapd,
				      const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE NEW_STA %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (sta)
		return 0;

	wpa_printf(MSG_DEBUG, "Add new STA " MACSTR " based on ctrl_iface "
		   "notification", MAC2STR(addr));
	sta = ap_sta_add(hapd, addr);
	if (sta == NULL)
		return -1;

	hostapd_new_assoc_sta(hapd, sta, 0);
	return 0;
}


#ifdef NEED_AP_MLME
static int hostapd_ctrl_iface_sa_query(struct hostapd_data *hapd,
				       const char *txtaddr)
{
	u8 addr[ETH_ALEN];
	u8 trans_id[WLAN_SA_QUERY_TR_ID_LEN];

	wpa_printf(MSG_DEBUG, "CTRL_IFACE SA_QUERY %s", txtaddr);

	if (hwaddr_aton(txtaddr, addr) ||
	    os_get_random(trans_id, WLAN_SA_QUERY_TR_ID_LEN) < 0)
		return -1;

	ieee802_11_send_sa_query_req(hapd, addr, trans_id);

	return 0;
}
#endif /* NEED_AP_MLME */


#ifdef CONFIG_WPS
static int hostapd_ctrl_iface_wps_pin(struct hostapd_data *hapd, char *txt)
{
	char *pin = os_strchr(txt, ' ');
	char *timeout_txt;
	int timeout;
	u8 addr_buf[ETH_ALEN], *addr = NULL;
	char *pos;

	if (pin == NULL)
		return -1;
	*pin++ = '\0';

	timeout_txt = os_strchr(pin, ' ');
	if (timeout_txt) {
		*timeout_txt++ = '\0';
		timeout = atoi(timeout_txt);
		pos = os_strchr(timeout_txt, ' ');
		if (pos) {
			*pos++ = '\0';
			if (hwaddr_aton(pos, addr_buf) == 0)
				addr = addr_buf;
		}
	} else
		timeout = 0;

	return hostapd_wps_add_pin(hapd, addr, txt, pin, timeout);
}


static int hostapd_ctrl_iface_wps_check_pin(
	struct hostapd_data *hapd, char *cmd, char *buf, size_t buflen)
{
	char pin[9];
	size_t len;
	char *pos;
	int ret;

	wpa_hexdump_ascii_key(MSG_DEBUG, "WPS_CHECK_PIN",
			      (u8 *) cmd, os_strlen(cmd));
	for (pos = cmd, len = 0; *pos != '\0'; pos++) {
		if (*pos < '0' || *pos > '9')
			continue;
		pin[len++] = *pos;
		if (len == 9) {
			wpa_printf(MSG_DEBUG, "WPS: Too long PIN");
			return -1;
		}
	}
	if (len != 4 && len != 8) {
		wpa_printf(MSG_DEBUG, "WPS: Invalid PIN length %d", (int) len);
		return -1;
	}
	pin[len] = '\0';

	if (len == 8) {
		unsigned int pin_val;
		pin_val = atoi(pin);
		if (!wps_pin_valid(pin_val)) {
			wpa_printf(MSG_DEBUG, "WPS: Invalid checksum digit");
			ret = os_snprintf(buf, buflen, "FAIL-CHECKSUM\n");
			if (os_snprintf_error(buflen, ret))
				return -1;
			return ret;
		}
	}

	ret = os_snprintf(buf, buflen, "%s", pin);
	if (os_snprintf_error(buflen, ret))
		return -1;

	return ret;
}


#ifdef CONFIG_WPS_NFC
static int hostapd_ctrl_iface_wps_nfc_tag_read(struct hostapd_data *hapd,
					       char *pos)
{
	size_t len;
	struct wpabuf *buf;
	int ret;

	len = os_strlen(pos);
	if (len & 0x01)
		return -1;
	len /= 2;

	buf = wpabuf_alloc(len);
	if (buf == NULL)
		return -1;
	if (hexstr2bin(pos, wpabuf_put(buf, len), len) < 0) {
		wpabuf_free(buf);
		return -1;
	}

	ret = hostapd_wps_nfc_tag_read(hapd, buf);
	wpabuf_free(buf);

	return ret;
}


static int hostapd_ctrl_iface_wps_nfc_config_token(struct hostapd_data *hapd,
						   char *cmd, char *reply,
						   size_t max_len)
{
	int ndef;
	struct wpabuf *buf;
	int res;

	if (os_strcmp(cmd, "WPS") == 0)
		ndef = 0;
	else if (os_strcmp(cmd, "NDEF") == 0)
		ndef = 1;
	else
		return -1;

	buf = hostapd_wps_nfc_config_token(hapd, ndef);
	if (buf == NULL)
		return -1;

	res = wpa_snprintf_hex_uppercase(reply, max_len, wpabuf_head(buf),
					 wpabuf_len(buf));
	reply[res++] = '\n';
	reply[res] = '\0';

	wpabuf_free(buf);

	return res;
}


static int hostapd_ctrl_iface_wps_nfc_token_gen(struct hostapd_data *hapd,
						char *reply, size_t max_len,
						int ndef)
{
	struct wpabuf *buf;
	int res;

	buf = hostapd_wps_nfc_token_gen(hapd, ndef);
	if (buf == NULL)
		return -1;

	res = wpa_snprintf_hex_uppercase(reply, max_len, wpabuf_head(buf),
					 wpabuf_len(buf));
	reply[res++] = '\n';
	reply[res] = '\0';

	wpabuf_free(buf);

	return res;
}


static int hostapd_ctrl_iface_wps_nfc_token(struct hostapd_data *hapd,
					    char *cmd, char *reply,
					    size_t max_len)
{
	if (os_strcmp(cmd, "WPS") == 0)
		return hostapd_ctrl_iface_wps_nfc_token_gen(hapd, reply,
							    max_len, 0);

	if (os_strcmp(cmd, "NDEF") == 0)
		return hostapd_ctrl_iface_wps_nfc_token_gen(hapd, reply,
							    max_len, 1);

	if (os_strcmp(cmd, "enable") == 0)
		return hostapd_wps_nfc_token_enable(hapd);

	if (os_strcmp(cmd, "disable") == 0) {
		hostapd_wps_nfc_token_disable(hapd);
		return 0;
	}

	return -1;
}


static int hostapd_ctrl_iface_nfc_get_handover_sel(struct hostapd_data *hapd,
						   char *cmd, char *reply,
						   size_t max_len)
{
	struct wpabuf *buf;
	int res;
	char *pos;
	int ndef;

	pos = os_strchr(cmd, ' ');
	if (pos == NULL)
		return -1;
	*pos++ = '\0';

	if (os_strcmp(cmd, "WPS") == 0)
		ndef = 0;
	else if (os_strcmp(cmd, "NDEF") == 0)
		ndef = 1;
	else
		return -1;

	if (os_strcmp(pos, "WPS-CR") == 0)
		buf = hostapd_wps_nfc_hs_cr(hapd, ndef);
	else
		buf = NULL;
	if (buf == NULL)
		return -1;

	res = wpa_snprintf_hex_uppercase(reply, max_len, wpabuf_head(buf),
					 wpabuf_len(buf));
	reply[res++] = '\n';
	reply[res] = '\0';

	wpabuf_free(buf);

	return res;
}


static int hostapd_ctrl_iface_nfc_report_handover(struct hostapd_data *hapd,
						  char *cmd)
{
	size_t len;
	struct wpabuf *req, *sel;
	int ret;
	char *pos, *role, *type, *pos2;

	role = cmd;
	pos = os_strchr(role, ' ');
	if (pos == NULL)
		return -1;
	*pos++ = '\0';

	type = pos;
	pos = os_strchr(type, ' ');
	if (pos == NULL)
		return -1;
	*pos++ = '\0';

	pos2 = os_strchr(pos, ' ');
	if (pos2 == NULL)
		return -1;
	*pos2++ = '\0';

	len = os_strlen(pos);
	if (len & 0x01)
		return -1;
	len /= 2;

	req = wpabuf_alloc(len);
	if (req == NULL)
		return -1;
	if (hexstr2bin(pos, wpabuf_put(req, len), len) < 0) {
		wpabuf_free(req);
		return -1;
	}

	len = os_strlen(pos2);
	if (len & 0x01) {
		wpabuf_free(req);
		return -1;
	}
	len /= 2;

	sel = wpabuf_alloc(len);
	if (sel == NULL) {
		wpabuf_free(req);
		return -1;
	}
	if (hexstr2bin(pos2, wpabuf_put(sel, len), len) < 0) {
		wpabuf_free(req);
		wpabuf_free(sel);
		return -1;
	}

	if (os_strcmp(role, "RESP") == 0 && os_strcmp(type, "WPS") == 0) {
		ret = hostapd_wps_nfc_report_handover(hapd, req, sel);
	} else {
		wpa_printf(MSG_DEBUG, "NFC: Unsupported connection handover "
			   "reported: role=%s type=%s", role, type);
		ret = -1;
	}
	wpabuf_free(req);
	wpabuf_free(sel);

	return ret;
}

#endif /* CONFIG_WPS_NFC */


static int hostapd_ctrl_iface_wps_ap_pin(struct hostapd_data *hapd, char *txt,
					 char *buf, size_t buflen)
{
	int timeout = 300;
	char *pos;
	const char *pin_txt;

	pos = os_strchr(txt, ' ');
	if (pos)
		*pos++ = '\0';

	if (os_strcmp(txt, "disable") == 0) {
		hostapd_wps_ap_pin_disable(hapd);
		return os_snprintf(buf, buflen, "OK\n");
	}

	if (os_strcmp(txt, "random") == 0) {
		if (pos)
			timeout = atoi(pos);
		pin_txt = hostapd_wps_ap_pin_random(hapd, timeout);
		if (pin_txt == NULL)
			return -1;
		return os_snprintf(buf, buflen, "%s", pin_txt);
	}

	if (os_strcmp(txt, "get") == 0) {
		pin_txt = hostapd_wps_ap_pin_get(hapd);
		if (pin_txt == NULL)
			return -1;
		return os_snprintf(buf, buflen, "%s", pin_txt);
	}

	if (os_strcmp(txt, "set") == 0) {
		char *pin;
		if (pos == NULL)
			return -1;
		pin = pos;
		pos = os_strchr(pos, ' ');
		if (pos) {
			*pos++ = '\0';
			timeout = atoi(pos);
		}
		if (os_strlen(pin) > buflen)
			return -1;
		if (hostapd_wps_ap_pin_set(hapd, pin, timeout) < 0)
			return -1;
		return os_snprintf(buf, buflen, "%s", pin);
	}

	return -1;
}


static int hostapd_ctrl_iface_wps_config(struct hostapd_data *hapd, char *txt)
{
	char *pos;
	char *ssid, *auth, *encr = NULL, *key = NULL;

	ssid = txt;
	pos = os_strchr(txt, ' ');
	if (!pos)
		return -1;
	*pos++ = '\0';

	auth = pos;
	pos = os_strchr(pos, ' ');
	if (pos) {
		*pos++ = '\0';
		encr = pos;
		pos = os_strchr(pos, ' ');
		if (pos) {
			*pos++ = '\0';
			key = pos;
		}
	}

	return hostapd_wps_config_ap(hapd, ssid, auth, encr, key);
}


static const char * pbc_status_str(enum pbc_status status)
{
	switch (status) {
	case WPS_PBC_STATUS_DISABLE:
		return "Disabled";
	case WPS_PBC_STATUS_ACTIVE:
		return "Active";
	case WPS_PBC_STATUS_TIMEOUT:
		return "Timed-out";
	case WPS_PBC_STATUS_OVERLAP:
		return "Overlap";
	default:
		return "Unknown";
	}
}


static int hostapd_ctrl_iface_wps_get_status(struct hostapd_data *hapd,
					     char *buf, size_t buflen)
{
	int ret;
	char *pos, *end;

	pos = buf;
	end = buf + buflen;

	ret = os_snprintf(pos, end - pos, "PBC Status: %s\n",
			  pbc_status_str(hapd->wps_stats.pbc_status));

	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	ret = os_snprintf(pos, end - pos, "Last WPS result: %s\n",
			  (hapd->wps_stats.status == WPS_STATUS_SUCCESS ?
			   "Success":
			   (hapd->wps_stats.status == WPS_STATUS_FAILURE ?
			    "Failed" : "None")));

	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	/* If status == Failure - Add possible Reasons */
	if(hapd->wps_stats.status == WPS_STATUS_FAILURE &&
	   hapd->wps_stats.failure_reason > 0) {
		ret = os_snprintf(pos, end - pos,
				  "Failure Reason: %s\n",
				  wps_ei_str(hapd->wps_stats.failure_reason));

		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if (hapd->wps_stats.status) {
		ret = os_snprintf(pos, end - pos, "Peer Address: " MACSTR "\n",
				  MAC2STR(hapd->wps_stats.peer_addr));

		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	return pos - buf;
}

#endif /* CONFIG_WPS */

#ifdef CONFIG_HS20

static int hostapd_ctrl_iface_hs20_wnm_notif(struct hostapd_data *hapd,
					     const char *cmd)
{
	u8 addr[ETH_ALEN];
	const char *url;

	if (hwaddr_aton(cmd, addr))
		return -1;
	url = cmd + 17;
	if (*url == '\0') {
		url = NULL;
	} else {
		if (*url != ' ')
			return -1;
		url++;
		if (*url == '\0')
			url = NULL;
	}

	return hs20_send_wnm_notification(hapd, addr, 1, url);
}


static int hostapd_ctrl_iface_hs20_deauth_req(struct hostapd_data *hapd,
					      const char *cmd)
{
	u8 addr[ETH_ALEN];
	int code, reauth_delay, ret;
	const char *pos;
	size_t url_len;
	struct wpabuf *req;

	/* <STA MAC Addr> <Code(0/1)> <Re-auth-Delay(sec)> [URL] */
	if (hwaddr_aton(cmd, addr))
		return -1;

	pos = os_strchr(cmd, ' ');
	if (pos == NULL)
		return -1;
	pos++;
	code = atoi(pos);

	pos = os_strchr(pos, ' ');
	if (pos == NULL)
		return -1;
	pos++;
	reauth_delay = atoi(pos);

	url_len = 0;
	pos = os_strchr(pos, ' ');
	if (pos) {
		pos++;
		url_len = os_strlen(pos);
	}

	req = wpabuf_alloc(4 + url_len);
	if (req == NULL)
		return -1;
	wpabuf_put_u8(req, code);
	wpabuf_put_le16(req, reauth_delay);
	wpabuf_put_u8(req, url_len);
	if (pos)
		wpabuf_put_data(req, pos, url_len);

	wpa_printf(MSG_DEBUG, "HS 2.0: Send WNM-Notification to " MACSTR
		   " to indicate imminent deauthentication (code=%d "
		   "reauth_delay=%d)", MAC2STR(addr), code, reauth_delay);
	ret = hs20_send_wnm_notification_deauth_req(hapd, addr, req);
	wpabuf_free(req);
	return ret;
}

#endif /* CONFIG_HS20 */


#ifdef CONFIG_INTERWORKING

static int hostapd_ctrl_iface_set_qos_map_set(struct hostapd_data *hapd,
					      const char *cmd)
{
	u8 qos_map_set[16 + 2 * 21], count = 0;
	const char *pos = cmd;
	int val, ret;

	for (;;) {
		if (count == sizeof(qos_map_set)) {
			wpa_printf(MSG_ERROR, "Too many qos_map_set parameters");
			return -1;
		}

		val = atoi(pos);
		if (val < 0 || val > 255) {
			wpa_printf(MSG_INFO, "Invalid QoS Map Set");
			return -1;
		}

		qos_map_set[count++] = val;
		pos = os_strchr(pos, ',');
		if (!pos)
			break;
		pos++;
	}

	if (count < 16 || count & 1) {
		wpa_printf(MSG_INFO, "Invalid QoS Map Set");
		return -1;
	}

	ret = hostapd_drv_set_qos_map(hapd, qos_map_set, count);
	if (ret) {
		wpa_printf(MSG_INFO, "Failed to set QoS Map Set");
		return -1;
	}

	os_memcpy(hapd->conf->qos_map_set, qos_map_set, count);
	hapd->conf->qos_map_set_len = count;

	return 0;
}


static int hostapd_ctrl_iface_send_qos_map_conf(struct hostapd_data *hapd,
						const char *cmd)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	struct wpabuf *buf;
	u8 *qos_map_set = hapd->conf->qos_map_set;
	u8 qos_map_set_len = hapd->conf->qos_map_set_len;
	int ret;

	if (!qos_map_set_len) {
		wpa_printf(MSG_INFO, "QoS Map Set is not set");
		return -1;
	}

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR " not found "
			   "for QoS Map Configuration message",
			   MAC2STR(addr));
		return -1;
	}

	if (!sta->qos_map_enabled) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR " did not indicate "
			   "support for QoS Map", MAC2STR(addr));
		return -1;
	}

	buf = wpabuf_alloc(2 + 2 + qos_map_set_len);
	if (buf == NULL)
		return -1;

	wpabuf_put_u8(buf, WLAN_ACTION_QOS);
	wpabuf_put_u8(buf, QOS_QOS_MAP_CONFIG);

	/* QoS Map Set Element */
	wpabuf_put_u8(buf, WLAN_EID_QOS_MAP_SET);
	wpabuf_put_u8(buf, qos_map_set_len);
	wpabuf_put_data(buf, qos_map_set, qos_map_set_len);

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, addr,
				      wpabuf_head(buf), wpabuf_len(buf));
	wpabuf_free(buf);

	return ret;
}

#endif /* CONFIG_INTERWORKING */


#ifdef CONFIG_WNM_AP

static int hostapd_ctrl_iface_coloc_intf_req(struct hostapd_data *hapd,
					     const char *cmd)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	const char *pos;
	unsigned int auto_report, timeout;

	if (hwaddr_aton(cmd, addr)) {
		wpa_printf(MSG_DEBUG, "Invalid STA MAC address");
		return -1;
	}

	sta = ap_get_sta(hapd, addr);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR
			   " not found for Collocated Interference Request",
			   MAC2STR(addr));
		return -1;
	}

	pos = cmd + 17;
	if (*pos != ' ')
		return -1;
	pos++;
	auto_report = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;
	timeout = atoi(pos);

	return wnm_send_coloc_intf_req(hapd, sta, auto_report, timeout);
}

#endif /* CONFIG_WNM_AP */


static int hostapd_ctrl_iface_get_key_mgmt(struct hostapd_data *hapd,
					   char *buf, size_t buflen)
{
	int ret = 0;
	char *pos, *end;

	pos = buf;
	end = buf + buflen;

	WPA_ASSERT(hapd->conf->wpa_key_mgmt);

	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK) {
		ret = os_snprintf(pos, end - pos, "WPA-PSK ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X) {
		ret = os_snprintf(pos, end - pos, "WPA-EAP ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#ifdef CONFIG_IEEE80211R_AP
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_PSK) {
		ret = os_snprintf(pos, end - pos, "FT-PSK ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X) {
		ret = os_snprintf(pos, end - pos, "FT-EAP ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#ifdef CONFIG_SHA384
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_IEEE8021X_SHA384) {
		ret = os_snprintf(pos, end - pos, "FT-EAP-SHA384 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_SHA384 */
#ifdef CONFIG_SAE
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_SAE) {
		ret = os_snprintf(pos, end - pos, "FT-SAE ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_SAE_EXT_KEY) {
		ret = os_snprintf(pos, end - pos, "FT-SAE-EXT-KEY ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_SAE */
#ifdef CONFIG_FILS
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA256) {
		ret = os_snprintf(pos, end - pos, "FT-FILS-SHA256 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FT_FILS_SHA384) {
		ret = os_snprintf(pos, end - pos, "FT-FILS-SHA384 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_FILS */
#endif /* CONFIG_IEEE80211R_AP */
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_PSK_SHA256) {
		ret = os_snprintf(pos, end - pos, "WPA-PSK-SHA256 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X_SHA256) {
		ret = os_snprintf(pos, end - pos, "WPA-EAP-SHA256 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#ifdef CONFIG_SAE
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_SAE) {
		ret = os_snprintf(pos, end - pos, "SAE ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_SAE_EXT_KEY) {
		ret = os_snprintf(pos, end - pos, "SAE-EXT-KEY ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_SAE */
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B) {
		ret = os_snprintf(pos, end - pos, "WPA-EAP-SUITE-B ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt &
	    WPA_KEY_MGMT_IEEE8021X_SUITE_B_192) {
		ret = os_snprintf(pos, end - pos,
				  "WPA-EAP-SUITE-B-192 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#ifdef CONFIG_FILS
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FILS_SHA256) {
		ret = os_snprintf(pos, end - pos, "FILS-SHA256 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_FILS_SHA384) {
		ret = os_snprintf(pos, end - pos, "FILS-SHA384 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_FILS */

#ifdef CONFIG_OWE
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_OWE) {
		ret = os_snprintf(pos, end - pos, "OWE ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_OWE */

#ifdef CONFIG_DPP
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_DPP) {
		ret = os_snprintf(pos, end - pos, "DPP ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_DPP */
#ifdef CONFIG_SHA384
	if (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_IEEE8021X_SHA384) {
		ret = os_snprintf(pos, end - pos, "WPA-EAP-SHA384 ");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}
#endif /* CONFIG_SHA384 */

	if (pos > buf && *(pos - 1) == ' ') {
		*(pos - 1) = '\0';
		pos--;
	}

	return pos - buf;
}


static int hostapd_ctrl_iface_get_config(struct hostapd_data *hapd,
					 char *buf, size_t buflen)
{
	int ret;
	char *pos, *end;

	pos = buf;
	end = buf + buflen;

	ret = os_snprintf(pos, end - pos, "bssid=" MACSTR "\n"
			  "ssid=%s\n",
			  MAC2STR(hapd->own_addr),
			  wpa_ssid_txt(hapd->conf->ssid.ssid,
				       hapd->conf->ssid.ssid_len));
	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	if ((hapd->conf->config_id)) {
		ret = os_snprintf(pos, end - pos, "config_id=%s\n",
				  hapd->conf->config_id);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

#ifdef CONFIG_WPS
	ret = os_snprintf(pos, end - pos, "wps_state=%s\n",
			  hapd->conf->wps_state == 0 ? "disabled" :
			  (hapd->conf->wps_state == 1 ? "not configured" :
			   "configured"));
	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	if (hapd->conf->wps_state && hapd->conf->wpa &&
	    hapd->conf->ssid.wpa_passphrase) {
		ret = os_snprintf(pos, end - pos, "passphrase=%s\n",
				  hapd->conf->ssid.wpa_passphrase);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if (hapd->conf->wps_state && hapd->conf->wpa &&
	    hapd->conf->ssid.wpa_psk &&
	    hapd->conf->ssid.wpa_psk->group) {
		char hex[PMK_LEN * 2 + 1];
		wpa_snprintf_hex(hex, sizeof(hex),
				 hapd->conf->ssid.wpa_psk->psk, PMK_LEN);
		ret = os_snprintf(pos, end - pos, "psk=%s\n", hex);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if (hapd->conf->multi_ap) {
		struct hostapd_ssid *ssid = &hapd->conf->multi_ap_backhaul_ssid;

		ret = os_snprintf(pos, end - pos, "multi_ap=%d\n",
				  hapd->conf->multi_ap);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;

		if (ssid->ssid_len) {
			ret = os_snprintf(pos, end - pos,
					  "multi_ap_backhaul_ssid=%s\n",
					  wpa_ssid_txt(ssid->ssid,
						       ssid->ssid_len));
			if (os_snprintf_error(end - pos, ret))
				return pos - buf;
			pos += ret;
		}

		if (hapd->conf->wps_state && hapd->conf->wpa &&
			ssid->wpa_passphrase) {
			ret = os_snprintf(pos, end - pos,
					  "multi_ap_backhaul_wpa_passphrase=%s\n",
					  ssid->wpa_passphrase);
			if (os_snprintf_error(end - pos, ret))
				return pos - buf;
			pos += ret;
		}

		if (hapd->conf->wps_state && hapd->conf->wpa &&
		    ssid->wpa_psk &&
		    ssid->wpa_psk->group) {
			char hex[PMK_LEN * 2 + 1];

			wpa_snprintf_hex(hex, sizeof(hex), ssid->wpa_psk->psk,
					 PMK_LEN);
			ret = os_snprintf(pos, end - pos,
					  "multi_ap_backhaul_wpa_psk=%s\n",
					  hex);
			forced_memzero(hex, sizeof(hex));
			if (os_snprintf_error(end - pos, ret))
				return pos - buf;
			pos += ret;
		}
	}
#endif /* CONFIG_WPS */

	if (hapd->conf->wpa) {
		ret = os_snprintf(pos, end - pos, "wpa=%d\n", hapd->conf->wpa);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if (hapd->conf->wpa && hapd->conf->wpa_key_mgmt) {
		ret = os_snprintf(pos, end - pos, "key_mgmt=");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;

		pos += hostapd_ctrl_iface_get_key_mgmt(hapd, pos, end - pos);

		ret = os_snprintf(pos, end - pos, "\n");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if (hapd->conf->wpa) {
		ret = os_snprintf(pos, end - pos, "group_cipher=%s\n",
				  wpa_cipher_txt(hapd->conf->wpa_group));
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if ((hapd->conf->wpa & WPA_PROTO_RSN) && hapd->conf->rsn_pairwise) {
		ret = os_snprintf(pos, end - pos, "rsn_pairwise_cipher=");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;

		ret = wpa_write_ciphers(pos, end, hapd->conf->rsn_pairwise,
					" ");
		if (ret < 0)
			return pos - buf;
		pos += ret;

		ret = os_snprintf(pos, end - pos, "\n");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if ((hapd->conf->wpa & WPA_PROTO_WPA) && hapd->conf->wpa_pairwise) {
		ret = os_snprintf(pos, end - pos, "wpa_pairwise_cipher=");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;

		ret = wpa_write_ciphers(pos, end, hapd->conf->wpa_pairwise,
					" ");
		if (ret < 0)
			return pos - buf;
		pos += ret;

		ret = os_snprintf(pos, end - pos, "\n");
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if (hapd->conf->wpa && hapd->conf->wpa_deny_ptk0_rekey) {
		ret = os_snprintf(pos, end - pos, "wpa_deny_ptk0_rekey=%d\n",
				  hapd->conf->wpa_deny_ptk0_rekey);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	if ((hapd->conf->wpa & WPA_PROTO_RSN) && hapd->conf->extended_key_id) {
		ret = os_snprintf(pos, end - pos, "extended_key_id=%d\n",
				  hapd->conf->extended_key_id);
		if (os_snprintf_error(end - pos, ret))
			return pos - buf;
		pos += ret;
	}

	return pos - buf;
}


static int hostapd_ctrl_iface_set_band(struct hostapd_data *hapd,
				       const char *bands)
{
	union wpa_event_data event;
	u32 setband_mask = WPA_SETBAND_AUTO;

	/*
	 * For example:
	 *  SET setband 2G,6G
	 *  SET setband 5G
	 *  SET setband AUTO
	 */
	if (!os_strstr(bands, "AUTO")) {
		if (os_strstr(bands, "5G"))
			setband_mask |= WPA_SETBAND_5G;
		if (os_strstr(bands, "6G"))
			setband_mask |= WPA_SETBAND_6G;
		if (os_strstr(bands, "2G"))
			setband_mask |= WPA_SETBAND_2G;
		if (setband_mask == WPA_SETBAND_AUTO)
			return -1;
	}

	if (hostapd_drv_set_band(hapd, setband_mask) == 0) {
		os_memset(&event, 0, sizeof(event));
		event.channel_list_changed.initiator = REGDOM_SET_BY_USER;
		event.channel_list_changed.type = REGDOM_TYPE_UNKNOWN;
		wpa_supplicant_event(hapd, EVENT_CHANNEL_LIST_CHANGED, &event);
	}

	return 0;
}


static int hostapd_ctrl_iface_set(struct hostapd_data *hapd, char *cmd)
{
	char *value;
	int ret = 0;

	value = os_strchr(cmd, ' ');
	if (value == NULL)
		return -1;
	*value++ = '\0';

	wpa_printf(MSG_DEBUG, "CTRL_IFACE SET '%s'='%s'", cmd, value);
	if (0) {
#ifdef CONFIG_WPS_TESTING
	} else if (os_strcasecmp(cmd, "wps_version_number") == 0) {
		long int val;
		val = strtol(value, NULL, 0);
		if (val < 0 || val > 0xff) {
			ret = -1;
			wpa_printf(MSG_DEBUG, "WPS: Invalid "
				   "wps_version_number %ld", val);
		} else {
			wps_version_number = val;
			wpa_printf(MSG_DEBUG, "WPS: Testing - force WPS "
				   "version %u.%u",
				   (wps_version_number & 0xf0) >> 4,
				   wps_version_number & 0x0f);
			hostapd_wps_update_ie(hapd);
		}
	} else if (os_strcasecmp(cmd, "wps_testing_stub_cred") == 0) {
		wps_testing_stub_cred = atoi(value);
		wpa_printf(MSG_DEBUG, "WPS: Testing - stub_cred=%d",
			   wps_testing_stub_cred);
	} else if (os_strcasecmp(cmd, "wps_corrupt_pkhash") == 0) {
		wps_corrupt_pkhash = atoi(value);
		wpa_printf(MSG_DEBUG, "WPS: Testing - wps_corrupt_pkhash=%d",
			   wps_corrupt_pkhash);
#endif /* CONFIG_WPS_TESTING */
#ifdef CONFIG_TESTING_OPTIONS
	} else if (os_strcasecmp(cmd, "ext_mgmt_frame_handling") == 0) {
		hapd->ext_mgmt_frame_handling = atoi(value);
	} else if (os_strcasecmp(cmd, "ext_eapol_frame_io") == 0) {
		hapd->ext_eapol_frame_io = atoi(value);
	} else if (os_strcasecmp(cmd, "force_backlog_bytes") == 0) {
		hapd->force_backlog_bytes = atoi(value);
#ifdef CONFIG_DPP
	} else if (os_strcasecmp(cmd, "dpp_config_obj_override") == 0) {
		os_free(hapd->dpp_config_obj_override);
		hapd->dpp_config_obj_override = os_strdup(value);
	} else if (os_strcasecmp(cmd, "dpp_discovery_override") == 0) {
		os_free(hapd->dpp_discovery_override);
		hapd->dpp_discovery_override = os_strdup(value);
	} else if (os_strcasecmp(cmd, "dpp_groups_override") == 0) {
		os_free(hapd->dpp_groups_override);
		hapd->dpp_groups_override = os_strdup(value);
	} else if (os_strcasecmp(cmd,
				 "dpp_ignore_netaccesskey_mismatch") == 0) {
		hapd->dpp_ignore_netaccesskey_mismatch = atoi(value);
	} else if (os_strcasecmp(cmd, "dpp_test") == 0) {
		dpp_test = atoi(value);
	} else if (os_strcasecmp(cmd, "dpp_version_override") == 0) {
		dpp_version_override = atoi(value);
#endif /* CONFIG_DPP */
#endif /* CONFIG_TESTING_OPTIONS */
#ifdef CONFIG_MBO
	} else if (os_strcasecmp(cmd, "mbo_assoc_disallow") == 0) {
		int val;

		if (!hapd->conf->mbo_enabled)
			return -1;

		val = atoi(value);
		if (val < 0 || val > MBO_ASSOC_DISALLOW_REASON_LOW_RSSI)
			return -1;

		hapd->mbo_assoc_disallow = val;
		ieee802_11_update_beacons(hapd->iface);

		/*
		 * TODO: Need to configure drivers that do AP MLME offload with
		 * disallowing station logic.
		 */
#endif /* CONFIG_MBO */
#ifdef CONFIG_DPP
	} else if (os_strcasecmp(cmd, "dpp_configurator_params") == 0) {
		os_free(hapd->dpp_configurator_params);
		hapd->dpp_configurator_params = os_strdup(value);
#ifdef CONFIG_DPP2
		dpp_controller_set_params(hapd->iface->interfaces->dpp, value);
#endif /* CONFIG_DPP2 */
	} else if (os_strcasecmp(cmd, "dpp_init_max_tries") == 0) {
		hapd->dpp_init_max_tries = atoi(value);
	} else if (os_strcasecmp(cmd, "dpp_init_retry_time") == 0) {
		hapd->dpp_init_retry_time = atoi(value);
	} else if (os_strcasecmp(cmd, "dpp_resp_wait_time") == 0) {
		hapd->dpp_resp_wait_time = atoi(value);
	} else if (os_strcasecmp(cmd, "dpp_resp_max_tries") == 0) {
		hapd->dpp_resp_max_tries = atoi(value);
	} else if (os_strcasecmp(cmd, "dpp_resp_retry_time") == 0) {
		hapd->dpp_resp_retry_time = atoi(value);
#endif /* CONFIG_DPP */
	} else if (os_strcasecmp(cmd, "setband") == 0) {
		ret = hostapd_ctrl_iface_set_band(hapd, value);
	} else {
		ret = hostapd_set_iface(hapd->iconf, hapd->conf, cmd, value);
		if (ret)
			return ret;

		if (os_strcasecmp(cmd, "deny_mac_file") == 0) {
			hostapd_disassoc_deny_mac(hapd);
		} else if (os_strcasecmp(cmd, "accept_mac_file") == 0) {
			hostapd_disassoc_accept_mac(hapd);
		} else if (os_strcasecmp(cmd, "ssid") == 0) {
			hostapd_neighbor_sync_own_report(hapd);
		} else if (os_strncmp(cmd, "wme_ac_", 7) == 0 ||
			   os_strncmp(cmd, "wmm_ac_", 7) == 0) {
			hapd->parameter_set_count++;
			if (ieee802_11_update_beacons(hapd->iface))
				wpa_printf(MSG_DEBUG,
					   "Failed to update beacons with WMM parameters");
		} else if (os_strcmp(cmd, "wpa_passphrase") == 0 ||
			   os_strcmp(cmd, "sae_password") == 0 ||
			   os_strcmp(cmd, "sae_pwe") == 0) {
			if (hapd->started)
				hostapd_setup_sae_pt(hapd->conf);
		} else if (os_strcasecmp(cmd, "transition_disable") == 0) {
			wpa_auth_set_transition_disable(hapd->wpa_auth,
							hapd->conf->transition_disable);
		}

#ifdef CONFIG_TESTING_OPTIONS
		if (os_strcmp(cmd, "ft_rsnxe_used") == 0)
			wpa_auth_set_ft_rsnxe_used(hapd->wpa_auth,
						   hapd->conf->ft_rsnxe_used);
		else if (os_strcmp(cmd, "oci_freq_override_eapol_m3") == 0)
			wpa_auth_set_ocv_override_freq(
				hapd->wpa_auth, WPA_AUTH_OCV_OVERRIDE_EAPOL_M3,
				atoi(value));
		else if (os_strcmp(cmd, "oci_freq_override_eapol_g1") == 0)
			wpa_auth_set_ocv_override_freq(
				hapd->wpa_auth, WPA_AUTH_OCV_OVERRIDE_EAPOL_G1,
				atoi(value));
		else if (os_strcmp(cmd, "oci_freq_override_ft_assoc") == 0)
			wpa_auth_set_ocv_override_freq(
				hapd->wpa_auth, WPA_AUTH_OCV_OVERRIDE_FT_ASSOC,
				atoi(value));
		else if (os_strcmp(cmd, "oci_freq_override_fils_assoc") == 0)
			wpa_auth_set_ocv_override_freq(
				hapd->wpa_auth,
				WPA_AUTH_OCV_OVERRIDE_FILS_ASSOC, atoi(value));
#endif /* CONFIG_TESTING_OPTIONS */
	}

	return ret;
}


static int hostapd_ctrl_iface_get(struct hostapd_data *hapd, char *cmd,
				  char *buf, size_t buflen)
{
	int res;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE GET '%s'", cmd);

	if (os_strcmp(cmd, "version") == 0) {
		res = os_snprintf(buf, buflen, "%s", VERSION_STR);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	} else if (os_strcmp(cmd, "tls_library") == 0) {
		res = tls_get_library_version(buf, buflen);
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	}

	return -1;
}


static int hostapd_ctrl_iface_enable(struct hostapd_iface *iface)
{
	if (hostapd_enable_iface(iface) < 0) {
		wpa_printf(MSG_ERROR, "Enabling of interface failed");
		return -1;
	}
	return 0;
}


static int hostapd_ctrl_iface_reload(struct hostapd_iface *iface)
{
	if (hostapd_reload_iface(iface) < 0) {
		wpa_printf(MSG_ERROR, "Reloading of interface failed");
		return -1;
	}
	return 0;
}


static int hostapd_ctrl_iface_reload_bss(struct hostapd_data *bss)
{
	if (hostapd_reload_bss_only(bss) < 0) {
		wpa_printf(MSG_ERROR, "Reloading of BSS failed");
		return -1;
	}
	return 0;
}


static int hostapd_ctrl_iface_disable(struct hostapd_iface *iface)
{
	if (hostapd_disable_iface(iface) < 0) {
		wpa_printf(MSG_ERROR, "Disabling of interface failed");
		return -1;
	}
	return 0;
}


static int
hostapd_ctrl_iface_kick_mismatch_psk_sta_iter(struct hostapd_data *hapd,
					      struct sta_info *sta, void *ctx)
{
	struct hostapd_wpa_psk *psk;
	const u8 *pmk;
	int pmk_len;
	int pmk_match;
	int sta_match;
	int bss_match;
	int reason;

	pmk = wpa_auth_get_pmk(sta->wpa_sm, &pmk_len);

	for (psk = hapd->conf->ssid.wpa_psk; pmk && psk; psk = psk->next) {
		pmk_match = PMK_LEN == pmk_len &&
			os_memcmp(psk->psk, pmk, pmk_len) == 0;
		sta_match = psk->group == 0 &&
			ether_addr_equal(sta->addr, psk->addr);
		bss_match = psk->group == 1;

		if (pmk_match && (sta_match || bss_match))
			return 0;
	}

	wpa_printf(MSG_INFO, "STA " MACSTR
		   " PSK/passphrase no longer valid - disconnect",
		   MAC2STR(sta->addr));
	reason = WLAN_REASON_PREV_AUTH_NOT_VALID;
	hostapd_drv_sta_deauth(hapd, sta->addr, reason);
	ap_sta_deauthenticate(hapd, sta, reason);

	return 0;
}


static int hostapd_ctrl_iface_reload_wpa_psk(struct hostapd_data *hapd)
{
	struct hostapd_bss_config *conf = hapd->conf;
	int err;

	hostapd_config_clear_wpa_psk(&conf->ssid.wpa_psk);

	err = hostapd_setup_wpa_psk(conf);
	if (err < 0) {
		wpa_printf(MSG_ERROR, "Reloading WPA-PSK passwords failed: %d",
			   err);
		return -1;
	}

	ap_for_each_sta(hapd, hostapd_ctrl_iface_kick_mismatch_psk_sta_iter,
			NULL);

	return 0;
}


#ifdef CONFIG_IEEE80211R_AP

static int hostapd_ctrl_iface_get_rxkhs(struct hostapd_data *hapd,
					char *buf, size_t buflen)
{
	int ret, start_pos;
	char *pos, *end;
	struct ft_remote_r0kh *r0kh;
	struct ft_remote_r1kh *r1kh;
	struct hostapd_bss_config *conf = hapd->conf;

	pos = buf;
	end = buf + buflen;

	for (r0kh = conf->r0kh_list; r0kh; r0kh=r0kh->next) {
		start_pos = pos - buf;
		ret = os_snprintf(pos, end - pos, "r0kh=" MACSTR " ",
				  MAC2STR(r0kh->addr));
		if (os_snprintf_error(end - pos, ret))
			return start_pos;
		pos += ret;
		if (r0kh->id_len + 1 >= (size_t) (end - pos))
			return start_pos;
		os_memcpy(pos, r0kh->id, r0kh->id_len);
		pos += r0kh->id_len;
		*pos++ = ' ';
		pos += wpa_snprintf_hex(pos, end - pos, r0kh->key,
					sizeof(r0kh->key));
		ret = os_snprintf(pos, end - pos, "\n");
		if (os_snprintf_error(end - pos, ret))
			return start_pos;
		pos += ret;
	}

	for (r1kh = conf->r1kh_list; r1kh; r1kh=r1kh->next) {
		start_pos = pos - buf;
		ret = os_snprintf(pos, end - pos, "r1kh=" MACSTR " " MACSTR " ",
			MAC2STR(r1kh->addr), MAC2STR(r1kh->id));
		if (os_snprintf_error(end - pos, ret))
			return start_pos;
		pos += ret;
		pos += wpa_snprintf_hex(pos, end - pos, r1kh->key,
					sizeof(r1kh->key));
		ret = os_snprintf(pos, end - pos, "\n");
		if (os_snprintf_error(end - pos, ret))
			return start_pos;
		pos += ret;
	}

	return pos - buf;
}


static int hostapd_ctrl_iface_reload_rxkhs(struct hostapd_data *hapd)
{
	struct hostapd_bss_config *conf = hapd->conf;
	int err;

	hostapd_config_clear_rxkhs(conf);

	err = hostapd_config_read_rxkh_file(conf, conf->rxkh_file);
	if (err < 0) {
		wpa_printf(MSG_ERROR, "Reloading RxKHs failed: %d",
			   err);
		return -1;
	}

	return 0;
}

#endif /* CONFIG_IEEE80211R_AP */


#ifdef CONFIG_TESTING_OPTIONS

static int hostapd_ctrl_iface_radar(struct hostapd_data *hapd, char *cmd)
{
	union wpa_event_data data;
	char *pos, *param;
	enum wpa_event_type event;

	wpa_printf(MSG_DEBUG, "RADAR TEST: %s", cmd);

	os_memset(&data, 0, sizeof(data));

	param = os_strchr(cmd, ' ');
	if (param == NULL)
		return -1;
	*param++ = '\0';

	if (os_strcmp(cmd, "DETECTED") == 0)
		event = EVENT_DFS_RADAR_DETECTED;
	else if (os_strcmp(cmd, "CAC-FINISHED") == 0)
		event = EVENT_DFS_CAC_FINISHED;
	else if (os_strcmp(cmd, "CAC-ABORTED") == 0)
		event = EVENT_DFS_CAC_ABORTED;
	else if (os_strcmp(cmd, "NOP-FINISHED") == 0)
		event = EVENT_DFS_NOP_FINISHED;
	else {
		wpa_printf(MSG_DEBUG, "Unsupported RADAR test command: %s",
			   cmd);
		return -1;
	}

	pos = os_strstr(param, "freq=");
	if (pos)
		data.dfs_event.freq = atoi(pos + 5);

	pos = os_strstr(param, "ht_enabled=1");
	if (pos)
		data.dfs_event.ht_enabled = 1;

	pos = os_strstr(param, "chan_offset=");
	if (pos)
		data.dfs_event.chan_offset = atoi(pos + 12);

	pos = os_strstr(param, "chan_width=");
	if (pos)
		data.dfs_event.chan_width = atoi(pos + 11);

	pos = os_strstr(param, "cf1=");
	if (pos)
		data.dfs_event.cf1 = atoi(pos + 4);

	pos = os_strstr(param, "cf2=");
	if (pos)
		data.dfs_event.cf2 = atoi(pos + 4);

	wpa_supplicant_event(hapd, event, &data);

	return 0;
}


static int hostapd_ctrl_iface_mgmt_tx(struct hostapd_data *hapd, char *cmd)
{
	size_t len;
	u8 *buf;
	int res;

	wpa_printf(MSG_DEBUG, "External MGMT TX: %s", cmd);

	len = os_strlen(cmd);
	if (len & 1)
		return -1;
	len /= 2;

	buf = os_malloc(len);
	if (buf == NULL)
		return -1;

	if (hexstr2bin(cmd, buf, len) < 0) {
		os_free(buf);
		return -1;
	}

	res = hostapd_drv_send_mlme(hapd, buf, len, 0, NULL, 0, 0);
	os_free(buf);
	return res;
}


static int hostapd_ctrl_iface_mgmt_tx_status_process(struct hostapd_data *hapd,
						     char *cmd)
{
	char *pos, *param;
	size_t len;
	u8 *buf;
	int stype = 0, ok = 0;
	union wpa_event_data event;

	if (!hapd->ext_mgmt_frame_handling)
		return -1;

	/* stype=<val> ok=<0/1> buf=<frame hexdump> */

	wpa_printf(MSG_DEBUG, "External MGMT TX status process: %s", cmd);

	pos = cmd;
	param = os_strstr(pos, "stype=");
	if (param) {
		param += 6;
		stype = atoi(param);
	}

	param = os_strstr(pos, " ok=");
	if (param) {
		param += 4;
		ok = atoi(param);
	}

	param = os_strstr(pos, " buf=");
	if (!param)
		return -1;
	param += 5;

	len = os_strlen(param);
	if (len & 1)
		return -1;
	len /= 2;

	buf = os_malloc(len);
	if (!buf || hexstr2bin(param, buf, len) < 0) {
		os_free(buf);
		return -1;
	}

	os_memset(&event, 0, sizeof(event));
	event.tx_status.type = WLAN_FC_TYPE_MGMT;
	event.tx_status.data = buf;
	event.tx_status.data_len = len;
	event.tx_status.stype = stype;
	event.tx_status.ack = ok;
	hapd->ext_mgmt_frame_handling = 0;
	wpa_supplicant_event(hapd, EVENT_TX_STATUS, &event);
	hapd->ext_mgmt_frame_handling = 1;

	os_free(buf);

	return 0;
}


static int hostapd_ctrl_iface_mgmt_rx_process(struct hostapd_data *hapd,
					      char *cmd)
{
	char *pos, *param;
	size_t len;
	u8 *buf;
	int freq = 0, datarate = 0, ssi_signal = 0;
	union wpa_event_data event;

	if (!hapd->ext_mgmt_frame_handling)
		return -1;

	/* freq=<MHz> datarate=<val> ssi_signal=<val> frame=<frame hexdump> */

	wpa_printf(MSG_DEBUG, "External MGMT RX process: %s", cmd);

	pos = cmd;
	param = os_strstr(pos, "freq=");
	if (param) {
		param += 5;
		freq = atoi(param);
	}

	param = os_strstr(pos, " datarate=");
	if (param) {
		param += 10;
		datarate = atoi(param);
	}

	param = os_strstr(pos, " ssi_signal=");
	if (param) {
		param += 12;
		ssi_signal = atoi(param);
	}

	param = os_strstr(pos, " frame=");
	if (param == NULL)
		return -1;
	param += 7;

	len = os_strlen(param);
	if (len & 1)
		return -1;
	len /= 2;

	buf = os_malloc(len);
	if (buf == NULL)
		return -1;

	if (hexstr2bin(param, buf, len) < 0) {
		os_free(buf);
		return -1;
	}

	os_memset(&event, 0, sizeof(event));
	event.rx_mgmt.freq = freq;
	event.rx_mgmt.frame = buf;
	event.rx_mgmt.frame_len = len;
	event.rx_mgmt.ssi_signal = ssi_signal;
	event.rx_mgmt.datarate = datarate;
	hapd->ext_mgmt_frame_handling = 0;
	wpa_supplicant_event(hapd, EVENT_RX_MGMT, &event);
	hapd->ext_mgmt_frame_handling = 1;

	os_free(buf);

	return 0;
}


static int hostapd_ctrl_iface_eapol_rx(struct hostapd_data *hapd, char *cmd)
{
	char *pos;
	u8 src[ETH_ALEN], *buf;
	int used;
	size_t len;

	wpa_printf(MSG_DEBUG, "External EAPOL RX: %s", cmd);

	pos = cmd;
	used = hwaddr_aton2(pos, src);
	if (used < 0)
		return -1;
	pos += used;
	while (*pos == ' ')
		pos++;

	len = os_strlen(pos);
	if (len & 1)
		return -1;
	len /= 2;

	buf = os_malloc(len);
	if (buf == NULL)
		return -1;

	if (hexstr2bin(pos, buf, len) < 0) {
		os_free(buf);
		return -1;
	}

	ieee802_1x_receive(hapd, src, buf, len, FRAME_ENCRYPTION_UNKNOWN);
	os_free(buf);

	return 0;
}


static int hostapd_ctrl_iface_eapol_tx(struct hostapd_data *hapd, char *cmd)
{
	char *pos, *pos2;
	u8 dst[ETH_ALEN], *buf;
	int used, ret;
	size_t len;
	unsigned int prev;
	int encrypt = 0;

	wpa_printf(MSG_DEBUG, "External EAPOL TX: %s", cmd);

	pos = cmd;
	used = hwaddr_aton2(pos, dst);
	if (used < 0)
		return -1;
	pos += used;
	while (*pos == ' ')
		pos++;

	pos2 = os_strchr(pos, ' ');
	if (pos2) {
		len = pos2 - pos;
		encrypt = os_strstr(pos2, "encrypt=1") != NULL;
	} else {
		len = os_strlen(pos);
	}
	if (len & 1)
		return -1;
	len /= 2;

	buf = os_malloc(len);
	if (!buf || hexstr2bin(pos, buf, len) < 0) {
		os_free(buf);
		return -1;
	}

	prev = hapd->ext_eapol_frame_io;
	hapd->ext_eapol_frame_io = 0;
	ret = hostapd_wpa_auth_send_eapol(hapd, dst, buf, len, encrypt);
	hapd->ext_eapol_frame_io = prev;
	os_free(buf);

	return ret;
}


static u16 ipv4_hdr_checksum(const void *buf, size_t len)
{
	size_t i;
	u32 sum = 0;
	const u16 *pos = buf;

	for (i = 0; i < len / 2; i++)
		sum += *pos++;

	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return sum ^ 0xffff;
}


#define HWSIM_PACKETLEN 1500
#define HWSIM_IP_LEN (HWSIM_PACKETLEN - sizeof(struct ether_header))

static void hostapd_data_test_rx(void *ctx, const u8 *src_addr, const u8 *buf,
				 size_t len)
{
	struct hostapd_data *hapd = ctx;
	const struct ether_header *eth;
	struct ip ip;
	const u8 *pos;
	unsigned int i;
	char extra[30];

	if (len < sizeof(*eth) + sizeof(ip) || len > HWSIM_PACKETLEN) {
		wpa_printf(MSG_DEBUG,
			   "test data: RX - ignore unexpected length %d",
			   (int) len);
		return;
	}

	eth = (const struct ether_header *) buf;
	os_memcpy(&ip, eth + 1, sizeof(ip));
	pos = &buf[sizeof(*eth) + sizeof(ip)];

	if (ip.ip_hl != 5 || ip.ip_v != 4 ||
	    ntohs(ip.ip_len) > HWSIM_IP_LEN) {
		wpa_printf(MSG_DEBUG,
			   "test data: RX - ignore unexpected IP header");
		return;
	}

	for (i = 0; i < ntohs(ip.ip_len) - sizeof(ip); i++) {
		if (*pos != (u8) i) {
			wpa_printf(MSG_DEBUG,
				   "test data: RX - ignore mismatching payload");
			return;
		}
		pos++;
	}

	extra[0] = '\0';
	if (ntohs(ip.ip_len) != HWSIM_IP_LEN)
		os_snprintf(extra, sizeof(extra), " len=%d", ntohs(ip.ip_len));
	wpa_msg(hapd->msg_ctx, MSG_INFO, "DATA-TEST-RX " MACSTR " " MACSTR "%s",
		MAC2STR(eth->ether_dhost), MAC2STR(eth->ether_shost), extra);
}


static int hostapd_ctrl_iface_data_test_config(struct hostapd_data *hapd,
					       char *cmd)
{
	int enabled = atoi(cmd);
	char *pos;
	const char *ifname;
	const u8 *addr = hapd->own_addr;

	if (!enabled) {
		if (hapd->l2_test) {
			l2_packet_deinit(hapd->l2_test);
			hapd->l2_test = NULL;
			wpa_dbg(hapd->msg_ctx, MSG_DEBUG,
				"test data: Disabled");
		}
		return 0;
	}

	if (hapd->l2_test)
		return 0;

	pos = os_strstr(cmd, " ifname=");
	if (pos)
		ifname = pos + 8;
	else
		ifname = hapd->conf->iface;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap)
		addr = hapd->mld->mld_addr;
#endif /* CONFIG_IEEE80211BE */
	hapd->l2_test = l2_packet_init(ifname, addr,
					ETHERTYPE_IP, hostapd_data_test_rx,
					hapd, 1);
	if (hapd->l2_test == NULL)
		return -1;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "test data: Enabled");

	return 0;
}


static int hostapd_ctrl_iface_data_test_tx(struct hostapd_data *hapd, char *cmd)
{
	u8 dst[ETH_ALEN], src[ETH_ALEN];
	char *pos, *pos2;
	int used;
	long int val;
	u8 tos;
	u8 buf[2 + HWSIM_PACKETLEN];
	struct ether_header *eth;
	struct ip *ip;
	u8 *dpos;
	unsigned int i;
	size_t send_len = HWSIM_IP_LEN;

	if (hapd->l2_test == NULL)
		return -1;

	/* format: <dst> <src> <tos> [len=<length>] */

	pos = cmd;
	used = hwaddr_aton2(pos, dst);
	if (used < 0)
		return -1;
	pos += used;
	while (*pos == ' ')
		pos++;
	used = hwaddr_aton2(pos, src);
	if (used < 0)
		return -1;
	pos += used;

	val = strtol(pos, &pos2, 0);
	if (val < 0 || val > 0xff)
		return -1;
	tos = val;

	pos = os_strstr(pos2, " len=");
	if (pos) {
		i = atoi(pos + 5);
		if (i < sizeof(*ip) || i > HWSIM_IP_LEN)
			return -1;
		send_len = i;
	}

	eth = (struct ether_header *) &buf[2];
	os_memcpy(eth->ether_dhost, dst, ETH_ALEN);
	os_memcpy(eth->ether_shost, src, ETH_ALEN);
	eth->ether_type = htons(ETHERTYPE_IP);
	ip = (struct ip *) (eth + 1);
	os_memset(ip, 0, sizeof(*ip));
	ip->ip_hl = 5;
	ip->ip_v = 4;
	ip->ip_ttl = 64;
	ip->ip_tos = tos;
	ip->ip_len = htons(send_len);
	ip->ip_p = 1;
	ip->ip_src.s_addr = htonl(192U << 24 | 168 << 16 | 1 << 8 | 1);
	ip->ip_dst.s_addr = htonl(192U << 24 | 168 << 16 | 1 << 8 | 2);
	ip->ip_sum = ipv4_hdr_checksum(ip, sizeof(*ip));
	dpos = (u8 *) (ip + 1);
	for (i = 0; i < send_len - sizeof(*ip); i++)
		*dpos++ = i;

	if (l2_packet_send(hapd->l2_test, dst, ETHERTYPE_IP, &buf[2],
			   sizeof(struct ether_header) + send_len) < 0)
		return -1;

	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "test data: TX dst=" MACSTR
		" src=" MACSTR " tos=0x%x", MAC2STR(dst), MAC2STR(src), tos);

	return 0;
}


static int hostapd_ctrl_iface_data_test_frame(struct hostapd_data *hapd,
					      char *cmd)
{
	u8 *buf;
	struct ether_header *eth;
	struct l2_packet_data *l2 = NULL;
	size_t len;
	u16 ethertype;
	int res = -1;
	const char *ifname = hapd->conf->iface;

	if (os_strncmp(cmd, "ifname=", 7) == 0) {
		cmd += 7;
		ifname = cmd;
		cmd = os_strchr(cmd, ' ');
		if (cmd == NULL)
			return -1;
		*cmd++ = '\0';
	}

	len = os_strlen(cmd);
	if (len & 1 || len < ETH_HLEN * 2)
		return -1;
	len /= 2;

	buf = os_malloc(len);
	if (buf == NULL)
		return -1;

	if (hexstr2bin(cmd, buf, len) < 0)
		goto done;

	eth = (struct ether_header *) buf;
	ethertype = ntohs(eth->ether_type);

	l2 = l2_packet_init(ifname, hapd->own_addr, ethertype,
			    hostapd_data_test_rx, hapd, 1);
	if (l2 == NULL)
		goto done;

	res = l2_packet_send(l2, eth->ether_dhost, ethertype, buf, len);
	wpa_dbg(hapd->msg_ctx, MSG_DEBUG, "test data: TX frame res=%d", res);
done:
	if (l2)
		l2_packet_deinit(l2);
	os_free(buf);

	return res < 0 ? -1 : 0;
}


static int hostapd_ctrl_reset_pn(struct hostapd_data *hapd, const char *cmd)
{
	struct sta_info *sta;
	u8 addr[ETH_ALEN];
	u8 zero[WPA_TK_MAX_LEN];

	os_memset(zero, 0, sizeof(zero));

	if (hwaddr_aton(cmd, addr))
		return -1;

	if (is_broadcast_ether_addr(addr) && os_strstr(cmd, " BIGTK")) {
		if (hapd->last_bigtk_alg == WPA_ALG_NONE)
			return -1;

		wpa_printf(MSG_INFO, "TESTING: Reset BIPN for BIGTK");

		/* First, use a zero key to avoid any possible duplicate key
		 * avoidance in the driver. */
		if (hostapd_drv_set_key(hapd->conf->iface, hapd,
					hapd->last_bigtk_alg,
					broadcast_ether_addr,
					hapd->last_bigtk_key_idx, 0, 1, NULL, 0,
					zero, hapd->last_bigtk_len,
					KEY_FLAG_GROUP_TX_DEFAULT) < 0)
			return -1;

		/* Set the previously configured key to reset its TSC */
		return hostapd_drv_set_key(hapd->conf->iface, hapd,
					   hapd->last_bigtk_alg,
					   broadcast_ether_addr,
					   hapd->last_bigtk_key_idx, 0, 1, NULL,
					   0, hapd->last_bigtk,
					   hapd->last_bigtk_len,
					   KEY_FLAG_GROUP_TX_DEFAULT);
	}

	if (is_broadcast_ether_addr(addr) && os_strstr(cmd, "IGTK")) {
		if (hapd->last_igtk_alg == WPA_ALG_NONE)
			return -1;

		wpa_printf(MSG_INFO, "TESTING: Reset IPN for IGTK");

		/* First, use a zero key to avoid any possible duplicate key
		 * avoidance in the driver. */
		if (hostapd_drv_set_key(hapd->conf->iface, hapd,
					hapd->last_igtk_alg,
					broadcast_ether_addr,
					hapd->last_igtk_key_idx, 0, 1, NULL, 0,
					zero, hapd->last_igtk_len,
					KEY_FLAG_GROUP_TX_DEFAULT) < 0)
			return -1;

		/* Set the previously configured key to reset its TSC */
		return hostapd_drv_set_key(hapd->conf->iface, hapd,
					   hapd->last_igtk_alg,
					   broadcast_ether_addr,
					   hapd->last_igtk_key_idx, 0, 1, NULL,
					   0, hapd->last_igtk,
					   hapd->last_igtk_len,
					   KEY_FLAG_GROUP_TX_DEFAULT);
	}

	if (is_broadcast_ether_addr(addr)) {
		if (hapd->last_gtk_alg == WPA_ALG_NONE)
			return -1;

		wpa_printf(MSG_INFO, "TESTING: Reset PN for GTK");

		/* First, use a zero key to avoid any possible duplicate key
		 * avoidance in the driver. */
		if (hostapd_drv_set_key(hapd->conf->iface, hapd,
					hapd->last_gtk_alg,
					broadcast_ether_addr,
					hapd->last_gtk_key_idx, 0, 1, NULL, 0,
					zero, hapd->last_gtk_len,
					KEY_FLAG_GROUP_TX_DEFAULT) < 0)
			return -1;

		/* Set the previously configured key to reset its TSC */
		return hostapd_drv_set_key(hapd->conf->iface, hapd,
					   hapd->last_gtk_alg,
					   broadcast_ether_addr,
					   hapd->last_gtk_key_idx, 0, 1, NULL,
					   0, hapd->last_gtk,
					   hapd->last_gtk_len,
					   KEY_FLAG_GROUP_TX_DEFAULT);
	}

	sta = ap_get_sta(hapd, addr);
	if (!sta)
		return -1;

	if (sta->last_tk_alg == WPA_ALG_NONE)
		return -1;

	wpa_printf(MSG_INFO, "TESTING: Reset PN for " MACSTR,
		   MAC2STR(sta->addr));

	/* First, use a zero key to avoid any possible duplicate key avoidance
	 * in the driver. */
	if (hostapd_drv_set_key(hapd->conf->iface, hapd, sta->last_tk_alg,
				sta->addr, sta->last_tk_key_idx, 0, 1, NULL, 0,
				zero, sta->last_tk_len,
				KEY_FLAG_PAIRWISE_RX_TX) < 0)
		return -1;

	/* Set the previously configured key to reset its TSC/RSC */
	return hostapd_drv_set_key(hapd->conf->iface, hapd, sta->last_tk_alg,
				   sta->addr, sta->last_tk_key_idx, 0, 1, NULL,
				   0, sta->last_tk, sta->last_tk_len,
				   KEY_FLAG_PAIRWISE_RX_TX);
}


static int hostapd_ctrl_set_key(struct hostapd_data *hapd, const char *cmd)
{
	u8 addr[ETH_ALEN];
	const char *pos = cmd;
	enum wpa_alg alg;
	enum key_flag key_flag;
	int idx, set_tx;
	u8 seq[6], key[WPA_TK_MAX_LEN];
	size_t key_len;

	/* parameters: alg addr idx set_tx seq key key_flag */

	alg = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;
	if (hwaddr_aton(pos, addr))
		return -1;
	pos += 17;
	if (*pos != ' ')
		return -1;
	pos++;
	idx = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;
	set_tx = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	pos++;
	if (hexstr2bin(pos, seq, sizeof(seq)) < 0)
		return -1;
	pos += 2 * 6;
	if (*pos != ' ')
		return -1;
	pos++;
	if (!os_strchr(pos, ' '))
		return -1;
	key_len = (os_strchr(pos, ' ') - pos) / 2;
	if (hexstr2bin(pos, key, key_len) < 0)
		return -1;
	pos += 2 * key_len;
	if (*pos != ' ')
		return -1;

	pos++;
	key_flag = atoi(pos);
	pos = os_strchr(pos, ' ');
	if (pos)
		return -1;

	wpa_printf(MSG_INFO, "TESTING: Set key");
	return hostapd_drv_set_key(hapd->conf->iface, hapd, alg, addr, idx, 0,
				   set_tx, seq, 6, key, key_len, key_flag);
}


static void restore_tk(void *ctx1, void *ctx2)
{
	struct hostapd_data *hapd = ctx1;
	struct sta_info *sta = ctx2;

	wpa_printf(MSG_INFO, "TESTING: Restore TK for " MACSTR,
		   MAC2STR(sta->addr));
	/* This does not really restore the TSC properly, so this will result
	 * in replay protection issues for now since there is no clean way of
	 * preventing encryption of a single EAPOL frame. */
	hostapd_drv_set_key(hapd->conf->iface, hapd, sta->last_tk_alg,
			    sta->addr, sta->last_tk_key_idx, 0, 1, NULL, 0,
			    sta->last_tk, sta->last_tk_len,
			    KEY_FLAG_PAIRWISE_RX_TX);
}


static int hostapd_ctrl_resend_m1(struct hostapd_data *hapd, const char *cmd)
{
	struct sta_info *sta;
	u8 addr[ETH_ALEN];
	int plain = os_strstr(cmd, "plaintext") != NULL;

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta || !sta->wpa_sm)
		return -1;

	if (plain && sta->last_tk_alg == WPA_ALG_NONE)
		plain = 0; /* no need for special processing */
	if (plain) {
		wpa_printf(MSG_INFO, "TESTING: Clear TK for " MACSTR,
			   MAC2STR(sta->addr));
		hostapd_drv_set_key(hapd->conf->iface, hapd, WPA_ALG_NONE,
				    sta->addr, sta->last_tk_key_idx, 0, 0, NULL,
				    0, NULL, 0, KEY_FLAG_PAIRWISE);
	}

	wpa_printf(MSG_INFO, "TESTING: Send M1 to " MACSTR, MAC2STR(sta->addr));
	return wpa_auth_resend_m1(sta->wpa_sm,
				  os_strstr(cmd, "change-anonce") != NULL,
				  plain ? restore_tk : NULL, hapd, sta);
}


static int hostapd_ctrl_resend_m3(struct hostapd_data *hapd, const char *cmd)
{
	struct sta_info *sta;
	u8 addr[ETH_ALEN];
	int plain = os_strstr(cmd, "plaintext") != NULL;

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta || !sta->wpa_sm)
		return -1;

	if (plain && sta->last_tk_alg == WPA_ALG_NONE)
		plain = 0; /* no need for special processing */
	if (plain) {
		wpa_printf(MSG_INFO, "TESTING: Clear TK for " MACSTR,
			   MAC2STR(sta->addr));
		hostapd_drv_set_key(hapd->conf->iface, hapd, WPA_ALG_NONE,
				    sta->addr, sta->last_tk_key_idx, 0, 0, NULL,
				    0, NULL, 0, KEY_FLAG_PAIRWISE);
	}

	wpa_printf(MSG_INFO, "TESTING: Send M3 to " MACSTR, MAC2STR(sta->addr));
	return wpa_auth_resend_m3(sta->wpa_sm,
				  plain ? restore_tk : NULL, hapd, sta);
}


static int hostapd_ctrl_resend_group_m1(struct hostapd_data *hapd,
					const char *cmd)
{
	struct sta_info *sta;
	u8 addr[ETH_ALEN];
	int plain = os_strstr(cmd, "plaintext") != NULL;

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta || !sta->wpa_sm)
		return -1;

	if (plain && sta->last_tk_alg == WPA_ALG_NONE)
		plain = 0; /* no need for special processing */
	if (plain) {
		wpa_printf(MSG_INFO, "TESTING: Clear TK for " MACSTR,
			   MAC2STR(sta->addr));
		hostapd_drv_set_key(hapd->conf->iface, hapd, WPA_ALG_NONE,
				    sta->addr, sta->last_tk_key_idx, 0, 0, NULL,
				    0, NULL, 0, KEY_FLAG_PAIRWISE);
	}

	wpa_printf(MSG_INFO,
		   "TESTING: Send group M1 for the same GTK and zero RSC to "
		   MACSTR, MAC2STR(sta->addr));
	return wpa_auth_resend_group_m1(sta->wpa_sm,
					plain ? restore_tk : NULL, hapd, sta);
}


static int hostapd_ctrl_rekey_ptk(struct hostapd_data *hapd, const char *cmd)
{
	struct sta_info *sta;
	u8 addr[ETH_ALEN];

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta || !sta->wpa_sm)
		return -1;

	return wpa_auth_rekey_ptk(hapd->wpa_auth, sta->wpa_sm);
}


static int hostapd_ctrl_get_pmksa_pmk(struct hostapd_data *hapd, const u8 *addr,
				      char *buf, size_t buflen)
{
	struct rsn_pmksa_cache_entry *pmksa;

	pmksa = wpa_auth_pmksa_get(hapd->wpa_auth, addr, NULL);
	if (!pmksa)
		return -1;

	return wpa_snprintf_hex(buf, buflen, pmksa->pmk, pmksa->pmk_len);
}


static int hostapd_ctrl_get_pmk(struct hostapd_data *hapd, const char *cmd,
				char *buf, size_t buflen)
{
	struct sta_info *sta;
	u8 addr[ETH_ALEN];
	const u8 *pmk;
	int pmk_len;

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta || !sta->wpa_sm) {
		wpa_printf(MSG_DEBUG, "No STA WPA state machine for " MACSTR,
			   MAC2STR(addr));
		return hostapd_ctrl_get_pmksa_pmk(hapd, addr, buf, buflen);
	}
	pmk = wpa_auth_get_pmk(sta->wpa_sm, &pmk_len);
	if (!pmk || !pmk_len) {
		wpa_printf(MSG_DEBUG, "No PMK stored for " MACSTR,
			   MAC2STR(addr));
		return hostapd_ctrl_get_pmksa_pmk(hapd, addr, buf, buflen);
	}

	return wpa_snprintf_hex(buf, buflen, pmk, pmk_len);
}


static int hostapd_ctrl_register_frame(struct hostapd_data *hapd,
				       const char *cmd)
{
	u16 type;
	char *pos, *end;
	u8 match[10];
	size_t match_len;
	bool multicast = false;

	type = strtol(cmd, &pos, 16);
	if (*pos != ' ')
		return -1;
	pos++;
	end = os_strchr(pos, ' ');
	if (end) {
		match_len = end - pos;
		multicast = os_strstr(end, "multicast") != NULL;
	} else {
		match_len = os_strlen(pos) / 2;
	}
	if (hexstr2bin(pos, match, match_len))
		return -1;

	return hostapd_drv_register_frame(hapd, type, match, match_len,
					  multicast);
}

#endif /* CONFIG_TESTING_OPTIONS */


#ifdef NEED_AP_MLME
static int hostapd_ctrl_check_freq_params(struct hostapd_freq_params *params,
					  u16 punct_bitmap)
{
	u32 start_freq;

	if (is_6ghz_freq(params->freq)) {
		const int bw_idx[] = { 20, 40, 80, 160, 320 };
		int idx, bw;

		/* The 6 GHz band requires HE to be enabled. */
		params->he_enabled = 1;

		if (params->center_freq1) {
			if (params->freq == 5935)
				idx = (params->center_freq1 - 5925) / 5;
			else
				idx = (params->center_freq1 - 5950) / 5;

			bw = center_idx_to_bw_6ghz(idx);
			if (bw < 0 || bw > (int) ARRAY_SIZE(bw_idx) ||
			    bw_idx[bw] != params->bandwidth)
				return -1;
		}
	} else { /* Non-6 GHz channel */
		/* An EHT STA is also an HE STA as defined in
		 * IEEE P802.11be/D5.0, 4.3.16a. */
		if (params->he_enabled || params->eht_enabled) {
			params->he_enabled = 1;
			/* An HE STA is also a VHT STA if operating in the 5 GHz
			 * band and an HE STA is also an HT STA in the 2.4 GHz
			 * band as defined in IEEE Std 802.11ax-2021, 4.3.15a.
			 * A VHT STA is an HT STA as defined in IEEE
			 * Std 802.11, 4.3.15. */
			if (IS_5GHZ(params->freq))
				params->vht_enabled = 1;

			params->ht_enabled = 1;
		}
	}

	switch (params->bandwidth) {
	case 0:
		/* bandwidth not specified: use 20 MHz by default */
		/* fall-through */
	case 20:
		if (params->center_freq1 &&
		    params->center_freq1 != params->freq)
			return -1;

		if (params->center_freq2 || params->sec_channel_offset)
			return -1;

		if (punct_bitmap)
			return -1;
		break;
	case 40:
		if (params->center_freq2 || !params->sec_channel_offset)
			return -1;

		if (punct_bitmap)
			return -1;

		if (!params->center_freq1)
			break;
		switch (params->sec_channel_offset) {
		case 1:
			if (params->freq + 10 != params->center_freq1)
				return -1;
			break;
		case -1:
			if (params->freq - 10 != params->center_freq1)
				return -1;
			break;
		default:
			return -1;
		}
		break;
	case 80:
		if (!params->center_freq1 || !params->sec_channel_offset)
			return 1;

		switch (params->sec_channel_offset) {
		case 1:
			if (params->freq - 10 != params->center_freq1 &&
			    params->freq + 30 != params->center_freq1)
				return 1;
			break;
		case -1:
			if (params->freq + 10 != params->center_freq1 &&
			    params->freq - 30 != params->center_freq1)
				return -1;
			break;
		default:
			return -1;
		}

		if (params->center_freq2 && punct_bitmap)
			return -1;

		/* Adjacent and overlapped are not allowed for 80+80 */
		if (params->center_freq2 &&
		    params->center_freq1 - params->center_freq2 <= 80 &&
		    params->center_freq2 - params->center_freq1 <= 80)
			return 1;
		break;
	case 160:
		if (!params->center_freq1 || params->center_freq2 ||
		    !params->sec_channel_offset)
			return -1;

		switch (params->sec_channel_offset) {
		case 1:
			if (params->freq + 70 != params->center_freq1 &&
			    params->freq + 30 != params->center_freq1 &&
			    params->freq - 10 != params->center_freq1 &&
			    params->freq - 50 != params->center_freq1)
				return -1;
			break;
		case -1:
			if (params->freq + 50 != params->center_freq1 &&
			    params->freq + 10 != params->center_freq1 &&
			    params->freq - 30 != params->center_freq1 &&
			    params->freq - 70 != params->center_freq1)
				return -1;
			break;
		default:
			return -1;
		}
		break;
	case 320:
		if (!params->center_freq1 || params->center_freq2 ||
		    !params->sec_channel_offset)
			return -1;

		switch (params->sec_channel_offset) {
		case 1:
			if (params->freq + 150 != params->center_freq1 &&
			    params->freq + 110 != params->center_freq1 &&
			    params->freq + 70 != params->center_freq1 &&
			    params->freq + 30 != params->center_freq1 &&
			    params->freq - 10 != params->center_freq1 &&
			    params->freq - 50 != params->center_freq1 &&
			    params->freq - 90 != params->center_freq1 &&
			    params->freq - 130 != params->center_freq1)
				return -1;
			break;
		case -1:
			if (params->freq + 130 != params->center_freq1 &&
			    params->freq + 90 != params->center_freq1 &&
			    params->freq + 50 != params->center_freq1 &&
			    params->freq + 10 != params->center_freq1 &&
			    params->freq - 30 != params->center_freq1 &&
			    params->freq - 70 != params->center_freq1 &&
			    params->freq - 110 != params->center_freq1 &&
			    params->freq - 150 != params->center_freq1)
				return -1;
			break;
		}
		break;
	default:
		return -1;
	}

	if (!punct_bitmap)
		return 0;

	if (!params->eht_enabled) {
		wpa_printf(MSG_ERROR,
			   "Preamble puncturing supported only in EHT");
		return -1;
	}

	if (params->freq >= 2412 && params->freq <= 2484) {
		wpa_printf(MSG_ERROR,
			   "Preamble puncturing is not supported in 2.4 GHz");
		return -1;
	}

	start_freq = params->center_freq1 - (params->bandwidth / 2);
	if (!is_punct_bitmap_valid(params->bandwidth,
				   (params->freq - start_freq) / 20,
				   punct_bitmap)) {
		wpa_printf(MSG_ERROR, "Invalid preamble puncturing bitmap");
		return -1;
	}

	return 0;
}
#endif /* NEED_AP_MLME */


static int hostapd_ctrl_iface_chan_switch(struct hostapd_iface *iface,
					  char *pos)
{
#ifdef NEED_AP_MLME
	struct csa_settings settings;
	int ret;
	int dfs_range = 0;
	unsigned int i;
	int bandwidth;
	u8 chan;
	unsigned int num_err = 0;
	int err = 0;

	ret = hostapd_parse_csa_settings(pos, &settings);
	if (ret)
		return ret;

	settings.link_id = -1;
#ifdef CONFIG_IEEE80211BE
	if (iface->num_bss && iface->bss[0]->conf->mld_ap)
		settings.link_id = iface->bss[0]->mld_link_id;
#endif /* CONFIG_IEEE80211BE */

	ret = hostapd_ctrl_check_freq_params(&settings.freq_params,
					     settings.punct_bitmap);
	if (ret) {
		wpa_printf(MSG_INFO,
			   "chanswitch: invalid frequency settings provided");
		return ret;
	}

	switch (settings.freq_params.bandwidth) {
	case 40:
		bandwidth = CHAN_WIDTH_40;
		break;
	case 80:
		if (settings.freq_params.center_freq2)
			bandwidth = CHAN_WIDTH_80P80;
		else
			bandwidth = CHAN_WIDTH_80;
		break;
	case 160:
		bandwidth = CHAN_WIDTH_160;
		break;
	case 320:
		bandwidth = CHAN_WIDTH_320;
		break;
	default:
		bandwidth = CHAN_WIDTH_20;
		break;
	}

	if (settings.freq_params.center_freq1)
		dfs_range += hostapd_is_dfs_overlap(
			iface, bandwidth, settings.freq_params.center_freq1);
	else
		dfs_range += hostapd_is_dfs_overlap(
			iface, bandwidth, settings.freq_params.freq);

	if (settings.freq_params.center_freq2)
		dfs_range += hostapd_is_dfs_overlap(
			iface, bandwidth, settings.freq_params.center_freq2);

	if (dfs_range) {
		ret = ieee80211_freq_to_chan(settings.freq_params.freq, &chan);
		if (ret == NUM_HOSTAPD_MODES) {
			wpa_printf(MSG_ERROR,
				   "Failed to get channel for (freq=%d, sec_channel_offset=%d, bw=%d)",
				   settings.freq_params.freq,
				   settings.freq_params.sec_channel_offset,
				   settings.freq_params.bandwidth);
			return -1;
		}

		settings.freq_params.channel = chan;

		wpa_printf(MSG_DEBUG,
			   "DFS/CAC to (channel=%u, freq=%d, sec_channel_offset=%d, bw=%d, center_freq1=%d)",
			   settings.freq_params.channel,
			   settings.freq_params.freq,
			   settings.freq_params.sec_channel_offset,
			   settings.freq_params.bandwidth,
			   settings.freq_params.center_freq1);

		/* Perform CAC and switch channel */
		iface->is_ch_switch_dfs = true;
		hostapd_switch_channel_fallback(iface, &settings.freq_params);
		return 0;
	}

	for (i = 0; i < iface->num_bss; i++) {

		/* Save CHAN_SWITCH VHT, HE, and EHT config */
		hostapd_chan_switch_config(iface->bss[i],
					   &settings.freq_params);

		err = hostapd_switch_channel(iface->bss[i], &settings);
		if (err) {
			ret = err;
			num_err++;
		}
	}

	return (iface->num_bss == num_err) ? ret : 0;
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}


#ifdef CONFIG_IEEE80211AX
static int hostapd_ctrl_iface_color_change(struct hostapd_iface *iface,
					   const char *pos)
{
#ifdef NEED_AP_MLME
	struct cca_settings settings;
	struct hostapd_data *hapd = iface->bss[0];
	int ret, color;
	unsigned int i;
	char *end;

	os_memset(&settings, 0, sizeof(settings));

	color = strtol(pos, &end, 10);
	if (pos == end || color < 0 || color > 63) {
		wpa_printf(MSG_ERROR, "color_change: Invalid color provided");
		return -1;
	}

	/* Color value is expected to be [1-63]. If 0 comes, assumption is this
	 * is to disable the color. In this case no need to do CCA, just
	 * changing Beacon frames is sufficient. */
	if (color == 0) {
		if (iface->conf->he_op.he_bss_color_disabled) {
			wpa_printf(MSG_ERROR,
				   "color_change: Color is already disabled");
			return -1;
		}

		iface->conf->he_op.he_bss_color_disabled = 1;

		for (i = 0; i < iface->num_bss; i++)
			ieee802_11_set_beacon(iface->bss[i]);

		return 0;
	}

	if (color == iface->conf->he_op.he_bss_color) {
		if (!iface->conf->he_op.he_bss_color_disabled) {
			wpa_printf(MSG_ERROR,
				   "color_change: Provided color is already set");
			return -1;
		}

		iface->conf->he_op.he_bss_color_disabled = 0;

		for (i = 0; i < iface->num_bss; i++)
			ieee802_11_set_beacon(iface->bss[i]);

		return 0;
	}

	if (hapd->cca_in_progress) {
		wpa_printf(MSG_ERROR,
			   "color_change: CCA is already in progress");
		return -1;
	}

	iface->conf->he_op.he_bss_color_disabled = 0;

	for (i = 0; i < iface->num_bss; i++) {
		struct hostapd_data *bss = iface->bss[i];

		hostapd_cleanup_cca_params(bss);

		bss->cca_color = color;
		bss->cca_count = 10;

		if (hostapd_fill_cca_settings(bss, &settings)) {
			wpa_printf(MSG_DEBUG,
				   "color_change: Filling CCA settings failed for color: %d\n",
				   color);
			hostapd_cleanup_cca_params(bss);
			continue;
		}

		wpa_printf(MSG_DEBUG, "Setting user selected color: %d", color);
		ret = hostapd_drv_switch_color(bss, &settings);
		if (ret)
			hostapd_cleanup_cca_params(bss);

		free_beacon_data(&settings.beacon_cca);
		free_beacon_data(&settings.beacon_after);
	}

	return 0;
#else /* NEED_AP_MLME */
	return -1;
#endif /* NEED_AP_MLME */
}
#endif /* CONFIG_IEEE80211AX */


static u8 hostapd_maxnss(struct hostapd_data *hapd, struct sta_info *sta)
{
	u8 *mcs_set = NULL;
	u16 mcs_map;
	u8 ht_rx_nss = 0;
	u8 vht_rx_nss = 1;
	u8 mcs;
	bool ht_supported = false;
	bool vht_supported = false;
	int i;

	if (sta->ht_capabilities && (sta->flags & WLAN_STA_HT)) {
		mcs_set = sta->ht_capabilities->supported_mcs_set;
		ht_supported = true;
	}

	if (sta->vht_capabilities && (sta->flags & WLAN_STA_VHT)) {
		mcs_map = le_to_host16(
			sta->vht_capabilities->vht_supported_mcs_set.rx_map);
		vht_supported = true;
	}

	if (ht_supported && mcs_set) {
		if (mcs_set[0])
			ht_rx_nss++;
		if (mcs_set[1])
			ht_rx_nss++;
		if (mcs_set[2])
			ht_rx_nss++;
		if (mcs_set[3])
			ht_rx_nss++;
	}
	if (vht_supported) {
		for (i = 7; i >= 0; i--) {
			mcs = (mcs_map >> (2 * i)) & 0x03;
			if (mcs != 0x03) {
				vht_rx_nss = i + 1;
				break;
			}
		}
	}

	return ht_rx_nss > vht_rx_nss ? ht_rx_nss : vht_rx_nss;
}


static char hostapd_ctrl_iface_notify_cw_htaction(struct hostapd_data *hapd,
						  const u8 *addr, u8 width)
{
	u8 buf[3];
	char ret;

	width = width >= 1 ? 1 : 0;

	buf[0] = WLAN_ACTION_HT;
	buf[1] = WLAN_HT_ACTION_NOTIFY_CHANWIDTH;
	buf[2] = width;

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, addr,
				      buf, sizeof(buf));
	if (ret)
		wpa_printf(MSG_DEBUG,
			   "Failed to send Notify Channel Width frame to "
			   MACSTR, MAC2STR(addr));

	return ret;
}


static char hostapd_ctrl_iface_notify_cw_vhtaction(struct hostapd_data *hapd,
						   const u8 *addr, u8 width)
{
	u8 buf[3];
	char ret;

	buf[0] = WLAN_ACTION_VHT;
	buf[1] = WLAN_VHT_ACTION_OPMODE_NOTIF;
	buf[2] = width;

	ret = hostapd_drv_send_action(hapd, hapd->iface->freq, 0, addr,
				      buf, sizeof(buf));
	if (ret)
		wpa_printf(MSG_DEBUG,
			   "Failed to send Opeating Mode Notification frame to "
			   MACSTR, MAC2STR(addr));

	return ret;
}


static char hostapd_ctrl_iface_notify_cw_change(struct hostapd_data *hapd,
						const char *cmd)
{
	u8 cw, operating_mode = 0, nss;
	struct sta_info *sta;
	enum hostapd_hw_mode hw_mode;

	if (is_6ghz_freq(hapd->iface->freq)) {
		wpa_printf(MSG_ERROR, "20/40 BSS coex not supported in 6 GHz");
		return -1;
	}

	cw = atoi(cmd);
	hw_mode = hapd->iface->current_mode->mode;
	if ((hw_mode == HOSTAPD_MODE_IEEE80211G ||
	     hw_mode == HOSTAPD_MODE_IEEE80211B) &&
	    !(cw == 0 || cw == 1)) {
		wpa_printf(MSG_ERROR,
			   "Channel width should be either 20 MHz or 40 MHz for 2.4 GHz band");
		return -1;
	}

	switch (cw) {
	case 0:
		operating_mode = 0;
		break;
	case 1:
		operating_mode = VHT_OPMODE_CHANNEL_40MHZ;
		break;
	case 2:
		operating_mode = VHT_OPMODE_CHANNEL_80MHZ;
		break;
	case 3:
		operating_mode = VHT_OPMODE_CHANNEL_160MHZ;
		break;
	default:
		wpa_printf(MSG_ERROR, "Channel width should be between 0 to 3");
		return -1;
	}

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if ((sta->flags & WLAN_STA_VHT) && sta->vht_capabilities) {
			nss = hostapd_maxnss(hapd, sta) - 1;
			hostapd_ctrl_iface_notify_cw_vhtaction(hapd, sta->addr,
							       operating_mode |
							       (u8) (nss << 4));
			continue;
		}

		if ((sta->flags & (WLAN_STA_HT | WLAN_STA_VHT)) ==
		    WLAN_STA_HT && sta->ht_capabilities)
			hostapd_ctrl_iface_notify_cw_htaction(hapd, sta->addr,
							      cw);
	}

	return 0;
}


static int hostapd_ctrl_iface_mib(struct hostapd_data *hapd, char *reply,
				  int reply_size, const char *param)
{
#ifdef RADIUS_SERVER
	if (os_strcmp(param, "radius_server") == 0) {
		return radius_server_get_mib(hapd->radius_srv, reply,
					     reply_size);
	}
#endif /* RADIUS_SERVER */
	return -1;
}


static int hostapd_ctrl_iface_vendor(struct hostapd_data *hapd, char *cmd,
				     char *buf, size_t buflen)
{
	int ret;
	char *pos, *temp = NULL;
	u8 *data = NULL;
	unsigned int vendor_id, subcmd;
	enum nested_attr nested_attr_flag = NESTED_ATTR_UNSPECIFIED;
	struct wpabuf *reply;
	size_t data_len = 0;

	/**
	 * cmd: <vendor id> <subcommand id> [<hex formatted data>]
	 * [nested=<0|1>]
	 */
	vendor_id = strtoul(cmd, &pos, 16);
	if (!isblank((unsigned char) *pos))
		return -EINVAL;

	subcmd = strtoul(pos, &pos, 10);

	if (*pos != '\0') {
		if (!isblank((unsigned char) *pos++))
			return -EINVAL;

		temp = os_strchr(pos, ' ');
		data_len = temp ? (size_t) (temp - pos) : os_strlen(pos);
	}

	if (data_len) {
		data_len /= 2;
		data = os_malloc(data_len);
		if (!data)
			return -ENOBUFS;

		if (hexstr2bin(pos, data, data_len)) {
			wpa_printf(MSG_DEBUG,
				   "Vendor command: wrong parameter format");
			os_free(data);
			return -EINVAL;
		}
	}

	pos = os_strstr(cmd, "nested=");
	if (pos)
		nested_attr_flag = atoi(pos + 7) ? NESTED_ATTR_USED :
			NESTED_ATTR_NOT_USED;

	reply = wpabuf_alloc((buflen - 1) / 2);
	if (!reply) {
		os_free(data);
		return -ENOBUFS;
	}

	ret = hostapd_drv_vendor_cmd(hapd, vendor_id, subcmd, data, data_len,
				     nested_attr_flag, reply);

	if (ret == 0)
		ret = wpa_snprintf_hex(buf, buflen, wpabuf_head_u8(reply),
				       wpabuf_len(reply));

	wpabuf_free(reply);
	os_free(data);

	return ret;
}


static int hostapd_ctrl_iface_eapol_reauth(struct hostapd_data *hapd,
					   const char *cmd)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;

	if (hwaddr_aton(cmd, addr))
		return -1;

	sta = ap_get_sta(hapd, addr);
	if (!sta || !sta->eapol_sm)
		return -1;

	eapol_auth_reauthenticate(sta->eapol_sm);
	return 0;
}


static int hostapd_ctrl_iface_eapol_set(struct hostapd_data *hapd, char *cmd)
{
	u8 addr[ETH_ALEN];
	struct sta_info *sta;
	char *pos = cmd, *param;

	if (hwaddr_aton(pos, addr) || pos[17] != ' ')
		return -1;
	pos += 18;
	param = pos;
	pos = os_strchr(pos, ' ');
	if (!pos)
		return -1;
	*pos++ = '\0';

	sta = ap_get_sta(hapd, addr);
	if (!sta || !sta->eapol_sm)
		return -1;

	return eapol_auth_set_conf(sta->eapol_sm, param, pos);
}


static int hostapd_ctrl_iface_log_level(struct hostapd_data *hapd, char *cmd,
					char *buf, size_t buflen)
{
	char *pos, *end, *stamp;
	int ret;

	/* cmd: "LOG_LEVEL [<level>]" */
	if (*cmd == '\0') {
		pos = buf;
		end = buf + buflen;
		ret = os_snprintf(pos, end - pos, "Current level: %s\n"
				  "Timestamp: %d\n",
				  debug_level_str(wpa_debug_level),
				  wpa_debug_timestamp);
		if (os_snprintf_error(end - pos, ret))
			ret = 0;

		return ret;
	}

	while (*cmd == ' ')
		cmd++;

	stamp = os_strchr(cmd, ' ');
	if (stamp) {
		*stamp++ = '\0';
		while (*stamp == ' ') {
			stamp++;
		}
	}

	if (os_strlen(cmd)) {
		int level = str_to_debug_level(cmd);
		if (level < 0)
			return -1;
		wpa_debug_level = level;
	}

	if (stamp && os_strlen(stamp))
		wpa_debug_timestamp = atoi(stamp);

	os_memcpy(buf, "OK\n", 3);
	return 3;
}


#ifdef NEED_AP_MLME
static int hostapd_ctrl_iface_track_sta_list(struct hostapd_data *hapd,
					     char *buf, size_t buflen)
{
	struct hostapd_iface *iface = hapd->iface;
	char *pos, *end;
	struct hostapd_sta_info *info;
	struct os_reltime now;

	if (!iface->num_sta_seen)
		return 0;

	sta_track_expire(iface, 0);

	pos = buf;
	end = buf + buflen;

	os_get_reltime(&now);
	dl_list_for_each_reverse(info, &iface->sta_seen,
				 struct hostapd_sta_info, list) {
		struct os_reltime age;
		int ret;

		os_reltime_sub(&now, &info->last_seen, &age);
		ret = os_snprintf(pos, end - pos, MACSTR " %u %d\n",
				  MAC2STR(info->addr), (unsigned int) age.sec,
				  info->ssi_signal);
		if (os_snprintf_error(end - pos, ret))
			break;
		pos += ret;
	}

	return pos - buf;
}
#endif /* NEED_AP_MLME */


static int hostapd_ctrl_iface_req_lci(struct hostapd_data *hapd,
				      const char *cmd)
{
	u8 addr[ETH_ALEN];

	if (hwaddr_aton(cmd, addr)) {
		wpa_printf(MSG_INFO, "CTRL: REQ_LCI: Invalid MAC address");
		return -1;
	}

	return hostapd_send_lci_req(hapd, addr);
}


static int hostapd_ctrl_iface_req_range(struct hostapd_data *hapd, char *cmd)
{
	u8 addr[ETH_ALEN];
	char *token, *context = NULL;
	int random_interval, min_ap;
	u8 responders[ETH_ALEN * RRM_RANGE_REQ_MAX_RESPONDERS];
	unsigned int n_responders;

	token = str_token(cmd, " ", &context);
	if (!token || hwaddr_aton(token, addr)) {
		wpa_printf(MSG_INFO,
			   "CTRL: REQ_RANGE - Bad destination address");
		return -1;
	}

	token = str_token(cmd, " ", &context);
	if (!token)
		return -1;

	random_interval = atoi(token);
	if (random_interval < 0 || random_interval > 0xffff)
		return -1;

	token = str_token(cmd, " ", &context);
	if (!token)
		return -1;

	min_ap = atoi(token);
	if (min_ap <= 0 || min_ap > WLAN_RRM_RANGE_REQ_MAX_MIN_AP)
		return -1;

	n_responders = 0;
	while ((token = str_token(cmd, " ", &context))) {
		if (n_responders == RRM_RANGE_REQ_MAX_RESPONDERS) {
			wpa_printf(MSG_INFO,
				   "CTRL: REQ_RANGE: Too many responders");
			return -1;
		}

		if (hwaddr_aton(token, responders + n_responders * ETH_ALEN)) {
			wpa_printf(MSG_INFO,
				   "CTRL: REQ_RANGE: Bad responder address");
			return -1;
		}

		n_responders++;
	}

	if (!n_responders) {
		wpa_printf(MSG_INFO,
			   "CTRL: REQ_RANGE - No FTM responder address");
		return -1;
	}

	return hostapd_send_range_req(hapd, addr, random_interval, min_ap,
				      responders, n_responders);
}


static int hostapd_ctrl_iface_req_beacon(struct hostapd_data *hapd,
					 const char *cmd, char *reply,
					 size_t reply_size)
{
	u8 addr[ETH_ALEN];
	const char *pos;
	struct wpabuf *req;
	int ret;
	u8 req_mode = 0;

	if (hwaddr_aton(cmd, addr))
		return -1;
	pos = os_strchr(cmd, ' ');
	if (!pos)
		return -1;
	pos++;
	if (os_strncmp(pos, "req_mode=", 9) == 0) {
		int val = hex2byte(pos + 9);

		if (val < 0)
			return -1;
		req_mode = val;
		pos += 11;
		pos = os_strchr(pos, ' ');
		if (!pos)
			return -1;
		pos++;
	}
	req = wpabuf_parse_bin(pos);
	if (!req)
		return -1;

	ret = hostapd_send_beacon_req(hapd, addr, req_mode, req);
	wpabuf_free(req);
	if (ret >= 0)
		ret = os_snprintf(reply, reply_size, "%d", ret);
	return ret;
}


static int hostapd_ctrl_iface_req_link_measurement(struct hostapd_data *hapd,
						   const char *cmd, char *reply,
						   size_t reply_size)
{
	u8 addr[ETH_ALEN];
	int ret;

	if (hwaddr_aton(cmd, addr)) {
		wpa_printf(MSG_ERROR,
			   "CTRL: REQ_LINK_MEASUREMENT: Invalid MAC address");
		return -1;
	}

	ret = hostapd_send_link_measurement_req(hapd, addr);
	if (ret >= 0)
		ret = os_snprintf(reply, reply_size, "%d", ret);
	return ret;
}


static int hostapd_ctrl_iface_show_neighbor(struct hostapd_data *hapd,
					    char *buf, size_t buflen)
{
	if (!(hapd->conf->radio_measurements[0] &
	      WLAN_RRM_CAPS_NEIGHBOR_REPORT)) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SHOW_NEIGHBOR: Neighbor report is not enabled");
		return -1;
	}

	return hostapd_neighbor_show(hapd, buf, buflen);
}


static int hostapd_ctrl_iface_set_neighbor(struct hostapd_data *hapd, char *buf)
{
	struct wpa_ssid_value ssid;
	u8 bssid[ETH_ALEN];
	struct wpabuf *nr, *lci = NULL, *civic = NULL;
	int stationary = 0;
	int bss_parameters = 0;
	char *tmp;
	int ret = -1;

	if (!(hapd->conf->radio_measurements[0] &
	      WLAN_RRM_CAPS_NEIGHBOR_REPORT)) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_NEIGHBOR: Neighbor report is not enabled");
		return -1;
	}

	if (hwaddr_aton(buf, bssid)) {
		wpa_printf(MSG_ERROR, "CTRL: SET_NEIGHBOR: Bad BSSID");
		return -1;
	}

	tmp = os_strstr(buf, "ssid=");
	if (!tmp || ssid_parse(tmp + 5, &ssid)) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_NEIGHBOR: Bad or missing SSID");
		return -1;
	}
	buf = os_strchr(tmp + 6, tmp[5] == '"' ? '"' : ' ');
	if (!buf)
		return -1;

	tmp = os_strstr(buf, "nr=");
	if (!tmp) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_NEIGHBOR: Missing Neighbor Report element");
		return -1;
	}

	buf = os_strchr(tmp, ' ');
	if (buf)
		*buf++ = '\0';

	nr = wpabuf_parse_bin(tmp + 3);
	if (!nr) {
		wpa_printf(MSG_ERROR,
			   "CTRL: SET_NEIGHBOR: Bad Neighbor Report element");
		return -1;
	}

	if (!buf)
		goto set;

	tmp = os_strstr(buf, "lci=");
	if (tmp) {
		buf = os_strchr(tmp, ' ');
		if (buf)
			*buf++ = '\0';
		lci = wpabuf_parse_bin(tmp + 4);
		if (!lci) {
			wpa_printf(MSG_ERROR,
				   "CTRL: SET_NEIGHBOR: Bad LCI subelement");
			goto fail;
		}
	}

	if (!buf)
		goto set;

	tmp = os_strstr(buf, "civic=");
	if (tmp) {
		buf = os_strchr(tmp, ' ');
		if (buf)
			*buf++ = '\0';
		civic = wpabuf_parse_bin(tmp + 6);
		if (!civic) {
			wpa_printf(MSG_ERROR,
				   "CTRL: SET_NEIGHBOR: Bad civic subelement");
			goto fail;
		}
	}

	if (!buf)
		goto set;

	if (os_strstr(buf, "stat"))
		stationary = 1;

	tmp = os_strstr(buf, "bss_parameter=");
	if (tmp) {
		bss_parameters = atoi(tmp + 14);
		if (bss_parameters < 0 || bss_parameters > 0xff) {
			wpa_printf(MSG_ERROR,
				   "CTRL: SET_NEIGHBOR: Bad bss_parameters subelement");
			goto fail;
		}
	}

set:
	ret = hostapd_neighbor_set(hapd, bssid, &ssid, nr, lci, civic,
				   stationary, bss_parameters);

fail:
	wpabuf_free(nr);
	wpabuf_free(lci);
	wpabuf_free(civic);

	return ret;
}


static int hostapd_ctrl_iface_remove_neighbor(struct hostapd_data *hapd,
					      char *buf)
{
	struct wpa_ssid_value ssid;
	struct wpa_ssid_value *ssidp = NULL;
	u8 bssid[ETH_ALEN];
	char *tmp;

	if (hwaddr_aton(buf, bssid)) {
		wpa_printf(MSG_ERROR, "CTRL: REMOVE_NEIGHBOR: Bad BSSID");
		return -1;
	}

	tmp = os_strstr(buf, "ssid=");
	if (tmp) {
		ssidp = &ssid;
		if (ssid_parse(tmp + 5, &ssid)) {
			wpa_printf(MSG_ERROR,
				   "CTRL: REMOVE_NEIGHBOR: Bad SSID");
			return -1;
		}
	}

	return hostapd_neighbor_remove(hapd, bssid, ssidp);
}


static int hostapd_ctrl_driver_flags(struct hostapd_iface *iface, char *buf,
				     size_t buflen)
{
	int ret, i;
	char *pos, *end;

	ret = os_snprintf(buf, buflen, "%016llX:\n",
			  (long long unsigned) iface->drv_flags);
	if (os_snprintf_error(buflen, ret))
		return -1;

	pos = buf + ret;
	end = buf + buflen;

	for (i = 0; i < 64; i++) {
		if (iface->drv_flags & (1LLU << i)) {
			ret = os_snprintf(pos, end - pos, "%s\n",
					  driver_flag_to_string(1LLU << i));
			if (os_snprintf_error(end - pos, ret))
				return -1;
			pos += ret;
		}
	}

	return pos - buf;
}


static int hostapd_ctrl_driver_flags2(struct hostapd_iface *iface, char *buf,
				      size_t buflen)
{
	int ret, i;
	char *pos, *end;

	ret = os_snprintf(buf, buflen, "%016llX:\n",
			  (long long unsigned) iface->drv_flags2);
	if (os_snprintf_error(buflen, ret))
		return -1;

	pos = buf + ret;
	end = buf + buflen;

	for (i = 0; i < 64; i++) {
		if (iface->drv_flags2 & (1LLU << i)) {
			ret = os_snprintf(pos, end - pos, "%s\n",
					  driver_flag2_to_string(1LLU << i));
			if (os_snprintf_error(end - pos, ret))
				return -1;
			pos += ret;
		}
	}

	return pos - buf;
}


static int hostapd_ctrl_iface_get_capability(struct hostapd_data *hapd,
					     const char *field, char *buf,
					     size_t buflen)
{
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: GET_CAPABILITY '%s'", field);

#ifdef CONFIG_DPP
	if (os_strcmp(field, "dpp") == 0) {
		int res;

#ifdef CONFIG_DPP3
		res = os_snprintf(buf, buflen, "DPP=3");
#elif defined(CONFIG_DPP2)
		res = os_snprintf(buf, buflen, "DPP=2");
#else /* CONFIG_DPP2 */
		res = os_snprintf(buf, buflen, "DPP=1");
#endif /* CONFIG_DPP2 */
		if (os_snprintf_error(buflen, res))
			return -1;
		return res;
	}
#endif /* CONFIG_DPP */

	wpa_printf(MSG_DEBUG, "CTRL_IFACE: Unknown GET_CAPABILITY field '%s'",
		   field);

	return -1;
}


#ifdef ANDROID
static int hostapd_ctrl_iface_driver_cmd(struct hostapd_data *hapd, char *cmd,
					 char *buf, size_t buflen)
{
	int ret;

	ret = hostapd_drv_driver_cmd(hapd, cmd, buf, buflen);
	if (ret == 0) {
		ret = os_snprintf(buf, buflen, "%s\n", "OK");
		if (os_snprintf_error(buflen, ret))
			ret = -1;
	}
	return ret;
}
#endif /* ANDROID */


#ifdef CONFIG_IEEE80211BE

static int hostapd_ctrl_iface_enable_mld(struct hostapd_iface *iface)
{
	unsigned int i;

	if (!iface || !iface->bss[0]->conf->mld_ap) {
		wpa_printf(MSG_ERROR,
			   "Trying to enable AP MLD on an interface that is not affiliated with an AP MLD");
		return -1;
	}

	for (i = 0; i < iface->interfaces->count; ++i) {
		struct hostapd_iface *h_iface = iface->interfaces->iface[i];
		struct hostapd_data *h_hapd = h_iface->bss[0];

		if (!hostapd_is_ml_partner(h_hapd, iface->bss[0]))
			continue;

		if (hostapd_enable_iface(h_iface)) {
			wpa_printf(MSG_ERROR, "Enabling of AP MLD failed");
			return -1;
		}
	}
	return 0;
}


static void hostapd_disable_iface_bss(struct hostapd_iface *iface)
{
	unsigned int i;

	for (i = 0; i < iface->num_bss; i++)
		hostapd_bss_deinit_no_free(iface->bss[i]);
}


static int hostapd_ctrl_iface_disable_mld(struct hostapd_iface *iface)
{
	unsigned int i;

	if (!iface || !iface->bss[0]->conf->mld_ap) {
		wpa_printf(MSG_ERROR,
			   "Trying to disable AP MLD on an interface that is not affiliated with an AP MLD.");
		return -1;
	}

	/* First, disable BSSs before stopping beaconing and doing driver
	 * deinit so that the broadcast Deauthentication frames go out. */

	for (i = 0; i < iface->interfaces->count; ++i) {
		struct hostapd_iface *h_iface = iface->interfaces->iface[i];
		struct hostapd_data *h_hapd = h_iface->bss[0];

		if (!hostapd_is_ml_partner(h_hapd, iface->bss[0]))
			continue;

		hostapd_disable_iface_bss(iface);
	}

	/* Then, fully disable interfaces */
	for (i = 0; i < iface->interfaces->count; ++i) {
		struct hostapd_iface *h_iface = iface->interfaces->iface[i];
		struct hostapd_data *h_hapd = h_iface->bss[0];

		if (!hostapd_is_ml_partner(h_hapd, iface->bss[0]))
			continue;

		if (hostapd_disable_iface(h_iface)) {
			wpa_printf(MSG_ERROR, "Disabling AP MLD failed");
			return -1;
		}
	}

	return 0;
}


#ifdef CONFIG_TESTING_OPTIONS
static int hostapd_ctrl_iface_link_remove(struct hostapd_data *hapd, char *cmd,
					  char *buf, size_t buflen)
{
	int ret;
	u32 count = atoi(cmd);

	if (!count)
		count = 1;

	ret = hostapd_link_remove(hapd, count);
	if (ret == 0) {
		ret = os_snprintf(buf, buflen, "%s\n", "OK");
		if (os_snprintf_error(buflen, ret))
			ret = -1;
		else
			ret = 0;
	}

	return ret;
}
#endif /* CONFIG_TESTING_OPTIONS */
#endif /* CONFIG_IEEE80211BE */


#ifdef CONFIG_NAN_USD

static int hostapd_ctrl_nan_publish(struct hostapd_data *hapd, char *cmd,
				    char *buf, size_t buflen)
{
	char *token, *context = NULL;
	int publish_id;
	struct nan_publish_params params;
	const char *service_name = NULL;
	struct wpabuf *ssi = NULL;
	int ret = -1;
	enum nan_service_protocol_type srv_proto_type = 0;

	os_memset(&params, 0, sizeof(params));
	/* USD shall use both solicited and unsolicited transmissions */
	params.unsolicited = true;
	params.solicited = true;
	/* USD shall require FSD without GAS */
	params.fsd = true;

	while ((token = str_token(cmd, " ", &context))) {
		if (os_strncmp(token, "service_name=", 13) == 0) {
			service_name = token + 13;
			continue;
		}

		if (os_strncmp(token, "ttl=", 4) == 0) {
			params.ttl = atoi(token + 4);
			continue;
		}

		if (os_strncmp(token, "srv_proto_type=", 15) == 0) {
			srv_proto_type = atoi(token + 15);
			continue;
		}

		if (os_strncmp(token, "ssi=", 4) == 0) {
			if (ssi)
				goto fail;
			ssi = wpabuf_parse_bin(token + 4);
			if (!ssi)
				goto fail;
			continue;
		}

		if (os_strcmp(token, "solicited=0") == 0) {
			params.solicited = false;
			continue;
		}

		if (os_strcmp(token, "unsolicited=0") == 0) {
			params.unsolicited = false;
			continue;
		}

		if (os_strcmp(token, "fsd=0") == 0) {
			params.fsd = false;
			continue;
		}

		wpa_printf(MSG_INFO, "CTRL: Invalid NAN_PUBLISH parameter: %s",
			   token);
		goto fail;
	}

	publish_id = hostapd_nan_usd_publish(hapd, service_name, srv_proto_type,
					     ssi, &params);
	if (publish_id > 0)
		ret = os_snprintf(buf, buflen, "%d", publish_id);
fail:
	wpabuf_free(ssi);
	return ret;
}


static int hostapd_ctrl_nan_cancel_publish(struct hostapd_data *hapd,
					   char *cmd)
{
	char *token, *context = NULL;
	int publish_id = 0;

	while ((token = str_token(cmd, " ", &context))) {
		if (sscanf(token, "publish_id=%i", &publish_id) == 1)
			continue;
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid NAN_CANCEL_PUBLISH parameter: %s",
			   token);
		return -1;
	}

	if (publish_id <= 0) {
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid or missing NAN_CANCEL_PUBLISH publish_id");
		return -1;
	}

	hostapd_nan_usd_cancel_publish(hapd, publish_id);
	return 0;
}


static int hostapd_ctrl_nan_update_publish(struct hostapd_data *hapd,
					   char *cmd)
{
	char *token, *context = NULL;
	int publish_id = 0;
	struct wpabuf *ssi = NULL;
	int ret = -1;

	while ((token = str_token(cmd, " ", &context))) {
		if (sscanf(token, "publish_id=%i", &publish_id) == 1)
			continue;
		if (os_strncmp(token, "ssi=", 4) == 0) {
			if (ssi)
				goto fail;
			ssi = wpabuf_parse_bin(token + 4);
			if (!ssi)
				goto fail;
			continue;
		}
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid NAN_UPDATE_PUBLISH parameter: %s",
			   token);
		goto fail;
	}

	if (publish_id <= 0) {
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid or missing NAN_UPDATE_PUBLISH publish_id");
		goto fail;
	}

	ret = hostapd_nan_usd_update_publish(hapd, publish_id, ssi);
fail:
	wpabuf_free(ssi);
	return ret;
}


static int hostapd_ctrl_nan_subscribe(struct hostapd_data *hapd, char *cmd,
				      char *buf, size_t buflen)
{
	char *token, *context = NULL;
	int subscribe_id;
	struct nan_subscribe_params params;
	const char *service_name = NULL;
	struct wpabuf *ssi = NULL;
	int ret = -1;
	enum nan_service_protocol_type srv_proto_type = 0;

	os_memset(&params, 0, sizeof(params));

	while ((token = str_token(cmd, " ", &context))) {
		if (os_strncmp(token, "service_name=", 13) == 0) {
			service_name = token + 13;
			continue;
		}

		if (os_strcmp(token, "active=1") == 0) {
			params.active = true;
			continue;
		}

		if (os_strncmp(token, "ttl=", 4) == 0) {
			params.ttl = atoi(token + 4);
			continue;
		}

		if (os_strncmp(token, "srv_proto_type=", 15) == 0) {
			srv_proto_type = atoi(token + 15);
			continue;
		}

		if (os_strncmp(token, "ssi=", 4) == 0) {
			if (ssi)
				goto fail;
			ssi = wpabuf_parse_bin(token + 4);
			if (!ssi)
				goto fail;
			continue;
		}

		wpa_printf(MSG_INFO,
			   "CTRL: Invalid NAN_SUBSCRIBE parameter: %s",
			   token);
		goto fail;
	}

	subscribe_id = hostapd_nan_usd_subscribe(hapd, service_name,
						 srv_proto_type, ssi,
						 &params);
	if (subscribe_id > 0)
		ret = os_snprintf(buf, buflen, "%d", subscribe_id);
fail:
	wpabuf_free(ssi);
	return ret;
}


static int hostapd_ctrl_nan_cancel_subscribe(struct hostapd_data *hapd,
					     char *cmd)
{
	char *token, *context = NULL;
	int subscribe_id = 0;

	while ((token = str_token(cmd, " ", &context))) {
		if (sscanf(token, "subscribe_id=%i", &subscribe_id) == 1)
			continue;
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid NAN_CANCEL_SUBSCRIBE parameter: %s",
			   token);
		return -1;
	}

	if (subscribe_id <= 0) {
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid or missing NAN_CANCEL_SUBSCRIBE subscribe_id");
		return -1;
	}

	hostapd_nan_usd_cancel_subscribe(hapd, subscribe_id);
	return 0;
}


static int hostapd_ctrl_nan_transmit(struct hostapd_data *hapd, char *cmd)
{
	char *token, *context = NULL;
	int handle = 0;
	int req_instance_id = 0;
	struct wpabuf *ssi = NULL;
	u8 peer_addr[ETH_ALEN];
	int ret = -1;

	os_memset(peer_addr, 0, ETH_ALEN);

	while ((token = str_token(cmd, " ", &context))) {
		if (sscanf(token, "handle=%i", &handle) == 1)
			continue;

		if (sscanf(token, "req_instance_id=%i", &req_instance_id) == 1)
			continue;

		if (os_strncmp(token, "address=", 8) == 0) {
			if (hwaddr_aton(token + 8, peer_addr) < 0)
				return -1;
			continue;
		}

		if (os_strncmp(token, "ssi=", 4) == 0) {
			if (ssi)
				goto fail;
			ssi = wpabuf_parse_bin(token + 4);
			if (!ssi)
				goto fail;
			continue;
		}

		wpa_printf(MSG_INFO,
			   "CTRL: Invalid NAN_TRANSMIT parameter: %s",
			   token);
		goto fail;
	}

	if (handle <= 0) {
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid or missing NAN_TRANSMIT handle");
		goto fail;
	}

	if (is_zero_ether_addr(peer_addr)) {
		wpa_printf(MSG_INFO,
			   "CTRL: Invalid or missing NAN_TRANSMIT address");
		goto fail;
	}

	ret = hostapd_nan_usd_transmit(hapd, handle, ssi, NULL, peer_addr,
				    req_instance_id);
fail:
	wpabuf_free(ssi);
	return ret;
}

#endif /* CONFIG_NAN_USD */


static int hostapd_ctrl_iface_receive_process(struct hostapd_data *hapd,
					      char *buf, char *reply,
					      int reply_size,
					      struct sockaddr_storage *from,
					      socklen_t fromlen)
{
	int reply_len, res;

	os_memcpy(reply, "OK\n", 3);
	reply_len = 3;

	if (os_strcmp(buf, "PING") == 0) {
		os_memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (os_strncmp(buf, "RELOG", 5) == 0) {
		if (wpa_debug_reopen_file() < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "CLOSE_LOG") == 0) {
		wpa_debug_stop_log();
	} else if (os_strncmp(buf, "NOTE ", 5) == 0) {
		wpa_printf(MSG_INFO, "NOTE: %s", buf + 5);
	} else if (os_strcmp(buf, "STATUS") == 0) {
		reply_len = hostapd_ctrl_iface_status(hapd, reply,
						      reply_size);
	} else if (os_strcmp(buf, "STATUS-DRIVER") == 0) {
		reply_len = hostapd_drv_status(hapd, reply, reply_size);
	} else if (os_strcmp(buf, "MIB") == 0) {
		reply_len = ieee802_11_get_mib(hapd, reply, reply_size);
		if (reply_len >= 0) {
			res = wpa_get_mib(hapd->wpa_auth, reply + reply_len,
					  reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
		if (reply_len >= 0) {
			res = ieee802_1x_get_mib(hapd, reply + reply_len,
						 reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
#ifndef CONFIG_NO_RADIUS
		if (reply_len >= 0) {
			res = radius_client_get_mib(hapd->radius,
						    reply + reply_len,
						    reply_size - reply_len);
			if (res < 0)
				reply_len = -1;
			else
				reply_len += res;
		}
#endif /* CONFIG_NO_RADIUS */
	} else if (os_strncmp(buf, "MIB ", 4) == 0) {
		reply_len = hostapd_ctrl_iface_mib(hapd, reply, reply_size,
						   buf + 4);
	} else if (os_strcmp(buf, "STA-FIRST") == 0) {
		reply_len = hostapd_ctrl_iface_sta_first(hapd, reply,
							 reply_size);
	} else if (os_strncmp(buf, "STA ", 4) == 0) {
		reply_len = hostapd_ctrl_iface_sta(hapd, buf + 4, reply,
						   reply_size);
	} else if (os_strncmp(buf, "STA-NEXT ", 9) == 0) {
		reply_len = hostapd_ctrl_iface_sta_next(hapd, buf + 9, reply,
							reply_size);
	} else if (os_strcmp(buf, "ATTACH") == 0) {
		if (hostapd_ctrl_iface_attach(hapd, from, fromlen, NULL))
			reply_len = -1;
	} else if (os_strncmp(buf, "ATTACH ", 7) == 0) {
		if (hostapd_ctrl_iface_attach(hapd, from, fromlen, buf + 7))
			reply_len = -1;
	} else if (os_strcmp(buf, "DETACH") == 0) {
		if (hostapd_ctrl_iface_detach(hapd, from, fromlen))
			reply_len = -1;
	} else if (os_strncmp(buf, "LEVEL ", 6) == 0) {
		if (hostapd_ctrl_iface_level(hapd, from, fromlen,
						    buf + 6))
			reply_len = -1;
	} else if (os_strncmp(buf, "NEW_STA ", 8) == 0) {
		if (hostapd_ctrl_iface_new_sta(hapd, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "DEAUTHENTICATE ", 15) == 0) {
		if (hostapd_ctrl_iface_deauthenticate(hapd, buf + 15))
			reply_len = -1;
	} else if (os_strncmp(buf, "DISASSOCIATE ", 13) == 0) {
		if (hostapd_ctrl_iface_disassociate(hapd, buf + 13))
			reply_len = -1;
#ifdef CONFIG_TAXONOMY
	} else if (os_strncmp(buf, "SIGNATURE ", 10) == 0) {
		reply_len = hostapd_ctrl_iface_signature(hapd, buf + 10,
							 reply, reply_size);
#endif /* CONFIG_TAXONOMY */
	} else if (os_strncmp(buf, "POLL_STA ", 9) == 0) {
		if (hostapd_ctrl_iface_poll_sta(hapd, buf + 9))
			reply_len = -1;
	} else if (os_strcmp(buf, "STOP_AP") == 0) {
		if (hostapd_ctrl_iface_stop_ap(hapd))
			reply_len = -1;
#ifdef NEED_AP_MLME
	} else if (os_strncmp(buf, "SA_QUERY ", 9) == 0) {
		if (hostapd_ctrl_iface_sa_query(hapd, buf + 9))
			reply_len = -1;
#endif /* NEED_AP_MLME */
#ifdef CONFIG_WPS
	} else if (os_strncmp(buf, "WPS_PIN ", 8) == 0) {
		if (hostapd_ctrl_iface_wps_pin(hapd, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_CHECK_PIN ", 14) == 0) {
		reply_len = hostapd_ctrl_iface_wps_check_pin(
			hapd, buf + 14, reply, reply_size);
	} else if (os_strcmp(buf, "WPS_PBC") == 0) {
		if (hostapd_wps_button_pushed(hapd, NULL))
			reply_len = -1;
	} else if (os_strcmp(buf, "WPS_CANCEL") == 0) {
		if (hostapd_wps_cancel(hapd))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_AP_PIN ", 11) == 0) {
		reply_len = hostapd_ctrl_iface_wps_ap_pin(hapd, buf + 11,
							  reply, reply_size);
	} else if (os_strncmp(buf, "WPS_CONFIG ", 11) == 0) {
		if (hostapd_ctrl_iface_wps_config(hapd, buf + 11) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_GET_STATUS", 13) == 0) {
		reply_len = hostapd_ctrl_iface_wps_get_status(hapd, reply,
							      reply_size);
#ifdef CONFIG_WPS_NFC
	} else if (os_strncmp(buf, "WPS_NFC_TAG_READ ", 17) == 0) {
		if (hostapd_ctrl_iface_wps_nfc_tag_read(hapd, buf + 17))
			reply_len = -1;
	} else if (os_strncmp(buf, "WPS_NFC_CONFIG_TOKEN ", 21) == 0) {
		reply_len = hostapd_ctrl_iface_wps_nfc_config_token(
			hapd, buf + 21, reply, reply_size);
	} else if (os_strncmp(buf, "WPS_NFC_TOKEN ", 14) == 0) {
		reply_len = hostapd_ctrl_iface_wps_nfc_token(
			hapd, buf + 14, reply, reply_size);
	} else if (os_strncmp(buf, "NFC_GET_HANDOVER_SEL ", 21) == 0) {
		reply_len = hostapd_ctrl_iface_nfc_get_handover_sel(
			hapd, buf + 21, reply, reply_size);
	} else if (os_strncmp(buf, "NFC_REPORT_HANDOVER ", 20) == 0) {
		if (hostapd_ctrl_iface_nfc_report_handover(hapd, buf + 20))
			reply_len = -1;
#endif /* CONFIG_WPS_NFC */
#endif /* CONFIG_WPS */
#ifdef CONFIG_INTERWORKING
	} else if (os_strncmp(buf, "SET_QOS_MAP_SET ", 16) == 0) {
		if (hostapd_ctrl_iface_set_qos_map_set(hapd, buf + 16))
			reply_len = -1;
	} else if (os_strncmp(buf, "SEND_QOS_MAP_CONF ", 18) == 0) {
		if (hostapd_ctrl_iface_send_qos_map_conf(hapd, buf + 18))
			reply_len = -1;
#endif /* CONFIG_INTERWORKING */
#ifdef CONFIG_HS20
	} else if (os_strncmp(buf, "HS20_WNM_NOTIF ", 15) == 0) {
		if (hostapd_ctrl_iface_hs20_wnm_notif(hapd, buf + 15))
			reply_len = -1;
	} else if (os_strncmp(buf, "HS20_DEAUTH_REQ ", 16) == 0) {
		if (hostapd_ctrl_iface_hs20_deauth_req(hapd, buf + 16))
			reply_len = -1;
#endif /* CONFIG_HS20 */
#ifdef CONFIG_WNM_AP
	} else if (os_strncmp(buf, "DISASSOC_IMMINENT ", 18) == 0) {
		if (hostapd_ctrl_iface_disassoc_imminent(hapd, buf + 18))
			reply_len = -1;
	} else if (os_strncmp(buf, "ESS_DISASSOC ", 13) == 0) {
		if (hostapd_ctrl_iface_ess_disassoc(hapd, buf + 13))
			reply_len = -1;
	} else if (os_strncmp(buf, "BSS_TM_REQ ", 11) == 0) {
		if (hostapd_ctrl_iface_bss_tm_req(hapd, buf + 11))
			reply_len = -1;
	} else if (os_strncmp(buf, "COLOC_INTF_REQ ", 15) == 0) {
		if (hostapd_ctrl_iface_coloc_intf_req(hapd, buf + 15))
			reply_len = -1;
#endif /* CONFIG_WNM_AP */
	} else if (os_strcmp(buf, "GET_CONFIG") == 0) {
		reply_len = hostapd_ctrl_iface_get_config(hapd, reply,
							  reply_size);
	} else if (os_strncmp(buf, "SET ", 4) == 0) {
		if (hostapd_ctrl_iface_set(hapd, buf + 4))
			reply_len = -1;
	} else if (os_strncmp(buf, "GET ", 4) == 0) {
		reply_len = hostapd_ctrl_iface_get(hapd, buf + 4, reply,
						   reply_size);
	} else if (os_strcmp(buf, "ENABLE") == 0) {
		if (hostapd_ctrl_iface_enable(hapd->iface))
			reply_len = -1;
	} else if (os_strcmp(buf, "RELOAD_WPA_PSK") == 0) {
		if (hostapd_ctrl_iface_reload_wpa_psk(hapd))
			reply_len = -1;
#ifdef CONFIG_IEEE80211R_AP
	} else if (os_strcmp(buf, "GET_RXKHS") == 0) {
		reply_len = hostapd_ctrl_iface_get_rxkhs(hapd, reply,
							 reply_size);
	} else if (os_strcmp(buf, "RELOAD_RXKHS") == 0) {
		if (hostapd_ctrl_iface_reload_rxkhs(hapd))
			reply_len = -1;
#endif /* CONFIG_IEEE80211R_AP */
	} else if (os_strcmp(buf, "RELOAD_BSS") == 0) {
		if (hostapd_ctrl_iface_reload_bss(hapd))
			reply_len = -1;
	} else if (os_strcmp(buf, "RELOAD_CONFIG") == 0) {
		if (hostapd_reload_config(hapd->iface))
			reply_len = -1;
	} else if (os_strcmp(buf, "RELOAD") == 0) {
		if (hostapd_ctrl_iface_reload(hapd->iface))
			reply_len = -1;
	} else if (os_strcmp(buf, "DISABLE") == 0) {
		if (hostapd_ctrl_iface_disable(hapd->iface))
			reply_len = -1;
	} else if (os_strcmp(buf, "UPDATE_BEACON") == 0) {
		if (ieee802_11_set_beacon(hapd))
			reply_len = -1;
#ifdef CONFIG_TESTING_OPTIONS
	} else if (os_strncmp(buf, "RADAR ", 6) == 0) {
		if (hostapd_ctrl_iface_radar(hapd, buf + 6))
			reply_len = -1;
	} else if (os_strncmp(buf, "MGMT_TX ", 8) == 0) {
		if (hostapd_ctrl_iface_mgmt_tx(hapd, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "MGMT_TX_STATUS_PROCESS ", 23) == 0) {
		if (hostapd_ctrl_iface_mgmt_tx_status_process(hapd,
							      buf + 23) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "MGMT_RX_PROCESS ", 16) == 0) {
		if (hostapd_ctrl_iface_mgmt_rx_process(hapd, buf + 16) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "EAPOL_RX ", 9) == 0) {
		if (hostapd_ctrl_iface_eapol_rx(hapd, buf + 9) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "EAPOL_TX ", 9) == 0) {
		if (hostapd_ctrl_iface_eapol_tx(hapd, buf + 9) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DATA_TEST_CONFIG ", 17) == 0) {
		if (hostapd_ctrl_iface_data_test_config(hapd, buf + 17) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DATA_TEST_TX ", 13) == 0) {
		if (hostapd_ctrl_iface_data_test_tx(hapd, buf + 13) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DATA_TEST_FRAME ", 16) == 0) {
		if (hostapd_ctrl_iface_data_test_frame(hapd, buf + 16) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "TEST_ALLOC_FAIL ", 16) == 0) {
		if (testing_set_fail_pattern(true, buf + 16) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "GET_ALLOC_FAIL") == 0) {
		reply_len = testing_get_fail_pattern(true, reply, reply_size);
	} else if (os_strncmp(buf, "TEST_FAIL ", 10) == 0) {
		if (testing_set_fail_pattern(false, buf + 10) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "GET_FAIL") == 0) {
		reply_len = testing_get_fail_pattern(false, reply, reply_size);
	} else if (os_strncmp(buf, "RESET_PN ", 9) == 0) {
		if (hostapd_ctrl_reset_pn(hapd, buf + 9) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "SET_KEY ", 8) == 0) {
		if (hostapd_ctrl_set_key(hapd, buf + 8) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "RESEND_M1 ", 10) == 0) {
		if (hostapd_ctrl_resend_m1(hapd, buf + 10) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "RESEND_M3 ", 10) == 0) {
		if (hostapd_ctrl_resend_m3(hapd, buf + 10) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "RESEND_GROUP_M1 ", 16) == 0) {
		if (hostapd_ctrl_resend_group_m1(hapd, buf + 16) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "REKEY_PTK ", 10) == 0) {
		if (hostapd_ctrl_rekey_ptk(hapd, buf + 10) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "REKEY_GTK") == 0) {
		if (wpa_auth_rekey_gtk(hapd->wpa_auth) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "GET_PMK ", 8) == 0) {
		reply_len = hostapd_ctrl_get_pmk(hapd, buf + 8, reply,
						 reply_size);
	} else if (os_strncmp(buf, "REGISTER_FRAME ", 15) == 0) {
		if (hostapd_ctrl_register_frame(hapd, buf + 16) < 0)
			reply_len = -1;
#endif /* CONFIG_TESTING_OPTIONS */
	} else if (os_strncmp(buf, "CHAN_SWITCH ", 12) == 0) {
		if (hostapd_ctrl_iface_chan_switch(hapd->iface, buf + 12))
			reply_len = -1;
#ifdef CONFIG_IEEE80211AX
	} else if (os_strncmp(buf, "COLOR_CHANGE ", 13) == 0) {
		if (hostapd_ctrl_iface_color_change(hapd->iface, buf + 13))
			reply_len = -1;
#endif /* CONFIG_IEEE80211AX */
	} else if (os_strncmp(buf, "NOTIFY_CW_CHANGE ", 17) == 0) {
		if (hostapd_ctrl_iface_notify_cw_change(hapd, buf + 17))
			reply_len = -1;
	} else if (os_strncmp(buf, "VENDOR ", 7) == 0) {
		reply_len = hostapd_ctrl_iface_vendor(hapd, buf + 7, reply,
						      reply_size);
	} else if (os_strcmp(buf, "ERP_FLUSH") == 0) {
		ieee802_1x_erp_flush(hapd);
#ifdef RADIUS_SERVER
		radius_server_erp_flush(hapd->radius_srv);
#endif /* RADIUS_SERVER */
	} else if (os_strncmp(buf, "EAPOL_REAUTH ", 13) == 0) {
		if (hostapd_ctrl_iface_eapol_reauth(hapd, buf + 13))
			reply_len = -1;
	} else if (os_strncmp(buf, "EAPOL_SET ", 10) == 0) {
		if (hostapd_ctrl_iface_eapol_set(hapd, buf + 10))
			reply_len = -1;
	} else if (os_strncmp(buf, "LOG_LEVEL", 9) == 0) {
		reply_len = hostapd_ctrl_iface_log_level(
			hapd, buf + 9, reply, reply_size);
#ifdef NEED_AP_MLME
	} else if (os_strcmp(buf, "TRACK_STA_LIST") == 0) {
		reply_len = hostapd_ctrl_iface_track_sta_list(
			hapd, reply, reply_size);
#endif /* NEED_AP_MLME */
	} else if (os_strcmp(buf, "PMKSA") == 0) {
		reply_len = hostapd_ctrl_iface_pmksa_list(hapd, reply,
							  reply_size);
	} else if (os_strcmp(buf, "PMKSA_FLUSH") == 0) {
		hostapd_ctrl_iface_pmksa_flush(hapd);
	} else if (os_strncmp(buf, "PMKSA_ADD ", 10) == 0) {
		if (hostapd_ctrl_iface_pmksa_add(hapd, buf + 10) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "SET_NEIGHBOR ", 13) == 0) {
		if (hostapd_ctrl_iface_set_neighbor(hapd, buf + 13))
			reply_len = -1;
	} else if (os_strcmp(buf, "SHOW_NEIGHBOR") == 0) {
		reply_len = hostapd_ctrl_iface_show_neighbor(hapd, reply,
							     reply_size);
	} else if (os_strncmp(buf, "REMOVE_NEIGHBOR ", 16) == 0) {
		if (hostapd_ctrl_iface_remove_neighbor(hapd, buf + 16))
			reply_len = -1;
	} else if (os_strncmp(buf, "REQ_LCI ", 8) == 0) {
		if (hostapd_ctrl_iface_req_lci(hapd, buf + 8))
			reply_len = -1;
	} else if (os_strncmp(buf, "REQ_RANGE ", 10) == 0) {
		if (hostapd_ctrl_iface_req_range(hapd, buf + 10))
			reply_len = -1;
	} else if (os_strncmp(buf, "REQ_BEACON ", 11) == 0) {
		reply_len = hostapd_ctrl_iface_req_beacon(hapd, buf + 11,
							  reply, reply_size);
	} else if (os_strncmp(buf, "REQ_LINK_MEASUREMENT ", 21) == 0) {
		reply_len = hostapd_ctrl_iface_req_link_measurement(
			hapd, buf + 21, reply, reply_size);
	} else if (os_strcmp(buf, "DRIVER_FLAGS") == 0) {
		reply_len = hostapd_ctrl_driver_flags(hapd->iface, reply,
						      reply_size);
	} else if (os_strcmp(buf, "DRIVER_FLAGS2") == 0) {
		reply_len = hostapd_ctrl_driver_flags2(hapd->iface, reply,
						       reply_size);
	} else if (os_strcmp(buf, "TERMINATE") == 0) {
		eloop_terminate();
	} else if (os_strncmp(buf, "ACCEPT_ACL ", 11) == 0) {
		if (os_strncmp(buf + 11, "ADD_MAC ", 8) == 0) {
			if (hostapd_ctrl_iface_acl_add_mac(
				    &hapd->conf->accept_mac,
				    &hapd->conf->num_accept_mac, buf + 19) ||
			    hostapd_set_acl(hapd))
				reply_len = -1;
		} else if (os_strncmp((buf + 11), "DEL_MAC ", 8) == 0) {
			if (hostapd_ctrl_iface_acl_del_mac(
				    &hapd->conf->accept_mac,
				    &hapd->conf->num_accept_mac, buf + 19) ||
			    hostapd_set_acl(hapd) ||
			    hostapd_disassoc_accept_mac(hapd))
				reply_len = -1;
		} else if (os_strcmp(buf + 11, "SHOW") == 0) {
			reply_len = hostapd_ctrl_iface_acl_show_mac(
				hapd->conf->accept_mac,
				hapd->conf->num_accept_mac, reply, reply_size);
		} else if (os_strcmp(buf + 11, "CLEAR") == 0) {
			hostapd_ctrl_iface_acl_clear_list(
				&hapd->conf->accept_mac,
				&hapd->conf->num_accept_mac);
			if (hostapd_set_acl(hapd) ||
			    hostapd_disassoc_accept_mac(hapd))
				reply_len = -1;
		} else {
			reply_len = -1;
		}
	} else if (os_strncmp(buf, "DENY_ACL ", 9) == 0) {
		if (os_strncmp(buf + 9, "ADD_MAC ", 8) == 0) {
			if (hostapd_ctrl_iface_acl_add_mac(
				    &hapd->conf->deny_mac,
				    &hapd->conf->num_deny_mac, buf + 17) ||
			    hostapd_set_acl(hapd) ||
			    hostapd_disassoc_deny_mac(hapd))
				reply_len = -1;
		} else if (os_strncmp(buf + 9, "DEL_MAC ", 8) == 0) {
			if (hostapd_ctrl_iface_acl_del_mac(
				    &hapd->conf->deny_mac,
				    &hapd->conf->num_deny_mac, buf + 17) ||
			    hostapd_set_acl(hapd))
				reply_len = -1;
		} else if (os_strcmp(buf + 9, "SHOW") == 0) {
			reply_len = hostapd_ctrl_iface_acl_show_mac(
				hapd->conf->deny_mac,
				hapd->conf->num_deny_mac, reply, reply_size);
		} else if (os_strcmp(buf + 9, "CLEAR") == 0) {
			hostapd_ctrl_iface_acl_clear_list(
				&hapd->conf->deny_mac,
				&hapd->conf->num_deny_mac);
			if (hostapd_set_acl(hapd))
				reply_len = -1;
		} else {
			reply_len = -1;
		}
#ifdef CONFIG_DPP
	} else if (os_strncmp(buf, "DPP_QR_CODE ", 12) == 0) {
		res = hostapd_dpp_qr_code(hapd, buf + 12);
		if (res < 0) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%d", res);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_NFC_URI ", 12) == 0) {
		res = hostapd_dpp_nfc_uri(hapd, buf + 12);
		if (res < 0) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%d", res);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_NFC_HANDOVER_REQ ", 21) == 0) {
		res = hostapd_dpp_nfc_handover_req(hapd, buf + 20);
		if (res < 0) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%d", res);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_NFC_HANDOVER_SEL ", 21) == 0) {
		res = hostapd_dpp_nfc_handover_sel(hapd, buf + 20);
		if (res < 0) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%d", res);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_BOOTSTRAP_GEN ", 18) == 0) {
		res = dpp_bootstrap_gen(hapd->iface->interfaces->dpp, buf + 18);
		if (res < 0) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%d", res);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_BOOTSTRAP_REMOVE ", 21) == 0) {
		if (dpp_bootstrap_remove(hapd->iface->interfaces->dpp,
					 buf + 21) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_BOOTSTRAP_GET_URI ", 22) == 0) {
		const char *uri;

		uri = dpp_bootstrap_get_uri(hapd->iface->interfaces->dpp,
					    atoi(buf + 22));
		if (!uri) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%s", uri);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_BOOTSTRAP_INFO ", 19) == 0) {
		reply_len = dpp_bootstrap_info(hapd->iface->interfaces->dpp,
					       atoi(buf + 19),
			reply, reply_size);
	} else if (os_strncmp(buf, "DPP_BOOTSTRAP_SET ", 18) == 0) {
		if (dpp_bootstrap_set(hapd->iface->interfaces->dpp,
				      atoi(buf + 18),
				      os_strchr(buf + 18, ' ')) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_AUTH_INIT ", 14) == 0) {
		if (hostapd_dpp_auth_init(hapd, buf + 13) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_LISTEN ", 11) == 0) {
		if (hostapd_dpp_listen(hapd, buf + 11) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "DPP_STOP_LISTEN") == 0) {
		hostapd_dpp_stop(hapd);
		hostapd_dpp_listen_stop(hapd);
	} else if (os_strncmp(buf, "DPP_CONFIGURATOR_ADD", 20) == 0) {
		res = dpp_configurator_add(hapd->iface->interfaces->dpp,
					   buf + 20);
		if (res < 0) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%d", res);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_CONFIGURATOR_SET ", 21) == 0) {
		if (dpp_configurator_set(hapd->iface->interfaces->dpp,
					 buf + 20) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_CONFIGURATOR_REMOVE ", 24) == 0) {
		if (dpp_configurator_remove(hapd->iface->interfaces->dpp,
					    buf + 24) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_CONFIGURATOR_SIGN ", 22) == 0) {
		if (hostapd_dpp_configurator_sign(hapd, buf + 21) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_CONFIGURATOR_GET_KEY ", 25) == 0) {
		reply_len = dpp_configurator_get_key_id(
			hapd->iface->interfaces->dpp,
			atoi(buf + 25),
			reply, reply_size);
	} else if (os_strncmp(buf, "DPP_PKEX_ADD ", 13) == 0) {
		res = hostapd_dpp_pkex_add(hapd, buf + 12);
		if (res < 0) {
			reply_len = -1;
		} else {
			reply_len = os_snprintf(reply, reply_size, "%d", res);
			if (os_snprintf_error(reply_size, reply_len))
				reply_len = -1;
		}
	} else if (os_strncmp(buf, "DPP_PKEX_REMOVE ", 16) == 0) {
		if (hostapd_dpp_pkex_remove(hapd, buf + 16) < 0)
			reply_len = -1;
#ifdef CONFIG_DPP2
	} else if (os_strncmp(buf, "DPP_CONTROLLER_START ", 21) == 0) {
		if (hostapd_dpp_controller_start(hapd, buf + 20) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "DPP_CONTROLLER_START") == 0) {
		if (hostapd_dpp_controller_start(hapd, NULL) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "DPP_CONTROLLER_STOP") == 0) {
		dpp_controller_stop(hapd->iface->interfaces->dpp);
	} else if (os_strncmp(buf, "DPP_CHIRP ", 10) == 0) {
		if (hostapd_dpp_chirp(hapd, buf + 9) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "DPP_STOP_CHIRP") == 0) {
		hostapd_dpp_chirp_stop(hapd);
	} else if (os_strncmp(buf, "DPP_RELAY_ADD_CONTROLLER ", 25) == 0) {
		if (hostapd_dpp_add_controller(hapd, buf + 25) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_RELAY_REMOVE_CONTROLLER ", 28) == 0) {
		hostapd_dpp_remove_controller(hapd, buf + 28);
#endif /* CONFIG_DPP2 */
#ifdef CONFIG_DPP3
	} else if (os_strcmp(buf, "DPP_PUSH_BUTTON") == 0) {
		if (hostapd_dpp_push_button(hapd, NULL) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "DPP_PUSH_BUTTON ", 16) == 0) {
		if (hostapd_dpp_push_button(hapd, buf + 15) < 0)
			reply_len = -1;
#endif /* CONFIG_DPP3 */
#endif /* CONFIG_DPP */
#ifdef CONFIG_NAN_USD
	} else if (os_strncmp(buf, "NAN_PUBLISH ", 12) == 0) {
		reply_len = hostapd_ctrl_nan_publish(hapd, buf + 12, reply,
						     reply_size);
	} else if (os_strncmp(buf, "NAN_CANCEL_PUBLISH ", 19) == 0) {
		if (hostapd_ctrl_nan_cancel_publish(hapd, buf + 19) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "NAN_UPDATE_PUBLISH ", 19) == 0) {
		if (hostapd_ctrl_nan_update_publish(hapd, buf + 19) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "NAN_SUBSCRIBE ", 14) == 0) {
		reply_len = hostapd_ctrl_nan_subscribe(hapd, buf + 14, reply,
						       reply_size);
	} else if (os_strncmp(buf, "NAN_CANCEL_SUBSCRIBE ", 21) == 0) {
		if (hostapd_ctrl_nan_cancel_subscribe(hapd, buf + 21) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "NAN_TRANSMIT ", 13) == 0) {
		if (hostapd_ctrl_nan_transmit(hapd, buf + 13) < 0)
			reply_len = -1;
#endif /* CONFIG_NAN_USD */
#ifdef RADIUS_SERVER
	} else if (os_strncmp(buf, "DAC_REQUEST ", 12) == 0) {
		if (radius_server_dac_request(hapd->radius_srv, buf + 12) < 0)
			reply_len = -1;
#endif /* RADIUS_SERVER */
	} else if (os_strncmp(buf, "GET_CAPABILITY ", 15) == 0) {
		reply_len = hostapd_ctrl_iface_get_capability(
			hapd, buf + 15, reply, reply_size);
#ifdef CONFIG_PASN
	} else if (os_strcmp(buf, "PTKSA_CACHE_LIST") == 0) {
		reply_len = ptksa_cache_list(hapd->ptksa, reply, reply_size);
#endif /* CONFIG_PASN */
#ifdef ANDROID
	} else if (os_strncmp(buf, "DRIVER ", 7) == 0) {
		reply_len = hostapd_ctrl_iface_driver_cmd(hapd, buf + 7, reply,
							  reply_size);
#endif /* ANDROID */
#ifdef CONFIG_IEEE80211BE
	} else if (os_strcmp(buf, "ENABLE_MLD") == 0) {
		if (hostapd_ctrl_iface_enable_mld(hapd->iface))
			reply_len = -1;
	} else if (os_strcmp(buf, "DISABLE_MLD") == 0) {
		if (hostapd_ctrl_iface_disable_mld(hapd->iface))
			reply_len = -1;
#ifdef CONFIG_TESTING_OPTIONS
	} else if (os_strncmp(buf, "LINK_REMOVE ", 12) == 0) {
		if (hostapd_ctrl_iface_link_remove(hapd, buf + 12,
						   reply, reply_size))
			reply_len = -1;
#endif /* CONFIG_TESTING_OPTIONS */
#endif /* CONFIG_IEEE80211BE */
	} else {
		os_memcpy(reply, "UNKNOWN COMMAND\n", 16);
		reply_len = 16;
	}

	if (reply_len < 0) {
		os_memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}

	return reply_len;
}


static void hostapd_ctrl_iface_receive(int sock, void *eloop_ctx,
				       void *sock_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	char buf[4096];
	int res;
	struct sockaddr_storage from;
	socklen_t fromlen = sizeof(from);
	char *reply, *pos = buf;
	const int reply_size = 4096;
	int reply_len;
	int level = MSG_DEBUG;
#ifdef CONFIG_CTRL_IFACE_UDP
	unsigned char lcookie[CTRL_IFACE_COOKIE_LEN];
#endif /* CONFIG_CTRL_IFACE_UDP */

	res = recvfrom(sock, buf, sizeof(buf) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		wpa_printf(MSG_ERROR, "recvfrom(ctrl_iface): %s",
			   strerror(errno));
		return;
	}
	buf[res] = '\0';

	reply = os_malloc(reply_size);
	if (reply == NULL) {
		if (sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from,
			   fromlen) < 0) {
			wpa_printf(MSG_DEBUG, "CTRL: sendto failed: %s",
				   strerror(errno));
		}
		return;
	}

#ifdef CONFIG_CTRL_IFACE_UDP
	if (os_strcmp(buf, "GET_COOKIE") == 0) {
		os_memcpy(reply, "COOKIE=", 7);
		wpa_snprintf_hex(reply + 7, 2 * CTRL_IFACE_COOKIE_LEN + 1,
				 hapd->ctrl_iface_cookie,
				 CTRL_IFACE_COOKIE_LEN);
		reply_len = 7 + 2 * CTRL_IFACE_COOKIE_LEN;
		goto done;
	}

	if (os_strncmp(buf, "COOKIE=", 7) != 0 ||
	    hexstr2bin(buf + 7, lcookie, CTRL_IFACE_COOKIE_LEN) < 0) {
		wpa_printf(MSG_DEBUG,
			   "CTRL: No cookie in the request - drop request");
		os_free(reply);
		return;
	}

	if (os_memcmp(hapd->ctrl_iface_cookie, lcookie,
		      CTRL_IFACE_COOKIE_LEN) != 0) {
		wpa_printf(MSG_DEBUG,
			   "CTRL: Invalid cookie in the request - drop request");
		os_free(reply);
		return;
	}

	pos = buf + 7 + 2 * CTRL_IFACE_COOKIE_LEN;
	while (*pos == ' ')
		pos++;
#endif /* CONFIG_CTRL_IFACE_UDP */

	if (os_strcmp(pos, "PING") == 0)
		level = MSG_EXCESSIVE;
	wpa_hexdump_ascii(level, "RX ctrl_iface", pos, res);

	reply_len = hostapd_ctrl_iface_receive_process(hapd, pos,
						       reply, reply_size,
						       &from, fromlen);

#ifdef CONFIG_CTRL_IFACE_UDP
done:
#endif /* CONFIG_CTRL_IFACE_UDP */
	if (sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from,
		   fromlen) < 0) {
		wpa_printf(MSG_DEBUG, "CTRL: sendto failed: %s",
			   strerror(errno));
	}
	os_free(reply);
}


#ifndef CONFIG_CTRL_IFACE_UDP
static char * hostapd_ctrl_iface_path(struct hostapd_data *hapd)
{
	char *buf;
	size_t len;

	if (hapd->conf->ctrl_interface == NULL)
		return NULL;

	len = os_strlen(hapd->conf->ctrl_interface) +
		os_strlen(hapd->conf->iface) + 2;
	buf = os_malloc(len);
	if (buf == NULL)
		return NULL;

	os_snprintf(buf, len, "%s/%s",
		    hapd->conf->ctrl_interface, hapd->conf->iface);
	buf[len - 1] = '\0';
	return buf;
}
#endif /* CONFIG_CTRL_IFACE_UDP */


static void hostapd_ctrl_iface_msg_cb(void *ctx, int level,
				      enum wpa_msg_type type,
				      const char *txt, size_t len)
{
	struct hostapd_data *hapd = ctx;
	if (hapd == NULL)
		return;
	hostapd_ctrl_iface_send(hapd, level, type, txt, len);
}


int hostapd_ctrl_iface_init(struct hostapd_data *hapd)
{
#ifdef CONFIG_CTRL_IFACE_UDP
	int port = HOSTAPD_CTRL_IFACE_PORT;
	char p[32] = { 0 };
	char port_str[40], *tmp;
	char *pos;
	struct addrinfo hints = { 0 }, *res, *saveres;
	int n;

	if (hapd->ctrl_sock > -1) {
		wpa_printf(MSG_DEBUG, "ctrl_iface already exists!");
		return 0;
	}

	if (hapd->conf->ctrl_interface == NULL)
		return 0;

	pos = os_strstr(hapd->conf->ctrl_interface, "udp:");
	if (pos) {
		pos += 4;
		port = atoi(pos);
		if (port <= 0) {
			wpa_printf(MSG_ERROR, "Invalid ctrl_iface UDP port");
			goto fail;
		}
	}

	dl_list_init(&hapd->ctrl_dst);
	hapd->ctrl_sock = -1;
	os_get_random(hapd->ctrl_iface_cookie, CTRL_IFACE_COOKIE_LEN);

#ifdef CONFIG_CTRL_IFACE_UDP_REMOTE
	hints.ai_flags = AI_PASSIVE;
#endif /* CONFIG_CTRL_IFACE_UDP_REMOTE */

#ifdef CONFIG_CTRL_IFACE_UDP_IPV6
	hints.ai_family = AF_INET6;
#else /* CONFIG_CTRL_IFACE_UDP_IPV6 */
	hints.ai_family = AF_INET;
#endif /* CONFIG_CTRL_IFACE_UDP_IPV6 */
	hints.ai_socktype = SOCK_DGRAM;

try_again:
	os_snprintf(p, sizeof(p), "%d", port);
	n = getaddrinfo(NULL, p, &hints, &res);
	if (n) {
		wpa_printf(MSG_ERROR, "getaddrinfo(): %s", gai_strerror(n));
		goto fail;
	}

	saveres = res;
	hapd->ctrl_sock = socket(res->ai_family, res->ai_socktype,
				 res->ai_protocol);
	if (hapd->ctrl_sock < 0) {
		wpa_printf(MSG_ERROR, "socket(PF_INET): %s", strerror(errno));
		goto fail;
	}

	if (bind(hapd->ctrl_sock, res->ai_addr, res->ai_addrlen) < 0) {
		port--;
		if ((HOSTAPD_CTRL_IFACE_PORT - port) <
		    HOSTAPD_CTRL_IFACE_PORT_LIMIT && !pos)
			goto try_again;
		wpa_printf(MSG_ERROR, "bind(AF_INET): %s", strerror(errno));
		goto fail;
	}

	freeaddrinfo(saveres);

	os_snprintf(port_str, sizeof(port_str), "udp:%d", port);
	tmp = os_strdup(port_str);
	if (tmp) {
		os_free(hapd->conf->ctrl_interface);
		hapd->conf->ctrl_interface = tmp;
	}
	wpa_printf(MSG_DEBUG, "ctrl_iface_init UDP port: %d", port);

	if (eloop_register_read_sock(hapd->ctrl_sock,
				     hostapd_ctrl_iface_receive, hapd, NULL) <
	    0) {
		hostapd_ctrl_iface_deinit(hapd);
		return -1;
	}

	hapd->msg_ctx = hapd;
	wpa_msg_register_cb(hostapd_ctrl_iface_msg_cb);

	return 0;

fail:
	if (hapd->ctrl_sock >= 0)
		close(hapd->ctrl_sock);
	return -1;
#else /* CONFIG_CTRL_IFACE_UDP */
	struct sockaddr_un addr;
	int s = -1;
	char *fname = NULL;

	if (hapd->ctrl_sock > -1) {
		wpa_printf(MSG_DEBUG, "ctrl_iface already exists!");
		return 0;
	}

	dl_list_init(&hapd->ctrl_dst);

	if (hapd->conf->ctrl_interface == NULL)
		return 0;

	if (mkdir(hapd->conf->ctrl_interface, S_IRWXU | S_IRWXG) < 0) {
		if (errno == EEXIST) {
			wpa_printf(MSG_DEBUG, "Using existing control "
				   "interface directory.");
		} else {
			wpa_printf(MSG_ERROR, "mkdir[ctrl_interface]: %s",
				   strerror(errno));
			goto fail;
		}
	}

	if (hapd->conf->ctrl_interface_gid_set &&
	    lchown(hapd->conf->ctrl_interface, -1,
		   hapd->conf->ctrl_interface_gid) < 0) {
		wpa_printf(MSG_ERROR, "lchown[ctrl_interface]: %s",
			   strerror(errno));
		return -1;
	}

	if (!hapd->conf->ctrl_interface_gid_set &&
	    hapd->iface->interfaces->ctrl_iface_group &&
	    lchown(hapd->conf->ctrl_interface, -1,
		   hapd->iface->interfaces->ctrl_iface_group) < 0) {
		wpa_printf(MSG_ERROR, "lchown[ctrl_interface]: %s",
			   strerror(errno));
		return -1;
	}

#ifdef ANDROID
	/*
	 * Android is using umask 0077 which would leave the control interface
	 * directory without group access. This breaks things since Wi-Fi
	 * framework assumes that this directory can be accessed by other
	 * applications in the wifi group. Fix this by adding group access even
	 * if umask value would prevent this.
	 */
	if (chmod(hapd->conf->ctrl_interface, S_IRWXU | S_IRWXG) < 0) {
		wpa_printf(MSG_ERROR, "CTRL: Could not chmod directory: %s",
			   strerror(errno));
		/* Try to continue anyway */
	}
#endif /* ANDROID */

	if (os_strlen(hapd->conf->ctrl_interface) + 1 +
	    os_strlen(hapd->conf->iface) >= sizeof(addr.sun_path))
		goto fail;

	s = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		wpa_printf(MSG_ERROR, "socket(PF_UNIX): %s", strerror(errno));
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
#ifdef __FreeBSD__
	addr.sun_len = sizeof(addr);
#endif /* __FreeBSD__ */
	addr.sun_family = AF_UNIX;
	fname = hostapd_ctrl_iface_path(hapd);
	if (fname == NULL)
		goto fail;
	os_strlcpy(addr.sun_path, fname, sizeof(addr.sun_path));
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		wpa_printf(MSG_DEBUG, "ctrl_iface bind(PF_UNIX) failed: %s",
			   strerror(errno));
		if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			wpa_printf(MSG_DEBUG, "ctrl_iface exists, but does not"
				   " allow connections - assuming it was left"
				   "over from forced program termination");
			if (unlink(fname) < 0) {
				wpa_printf(MSG_ERROR,
					   "Could not unlink existing ctrl_iface socket '%s': %s",
					   fname, strerror(errno));
				goto fail;
			}
			if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) <
			    0) {
				wpa_printf(MSG_ERROR,
					   "hostapd-ctrl-iface: bind(PF_UNIX): %s",
					   strerror(errno));
				goto fail;
			}
			wpa_printf(MSG_DEBUG, "Successfully replaced leftover "
				   "ctrl_iface socket '%s'", fname);
		} else {
			wpa_printf(MSG_INFO, "ctrl_iface exists and seems to "
				   "be in use - cannot override it");
			wpa_printf(MSG_INFO, "Delete '%s' manually if it is "
				   "not used anymore", fname);
			os_free(fname);
			fname = NULL;
			goto fail;
		}
	}

	if (hapd->conf->ctrl_interface_gid_set &&
	    lchown(fname, -1, hapd->conf->ctrl_interface_gid) < 0) {
		wpa_printf(MSG_ERROR, "lchown[ctrl_interface/ifname]: %s",
			   strerror(errno));
		goto fail;
	}

	if (!hapd->conf->ctrl_interface_gid_set &&
	    hapd->iface->interfaces->ctrl_iface_group &&
	    lchown(fname, -1, hapd->iface->interfaces->ctrl_iface_group) < 0) {
		wpa_printf(MSG_ERROR, "lchown[ctrl_interface/ifname]: %s",
			   strerror(errno));
		goto fail;
	}

	if (chmod(fname, S_IRWXU | S_IRWXG) < 0) {
		wpa_printf(MSG_ERROR, "chmod[ctrl_interface/ifname]: %s",
			   strerror(errno));
		goto fail;
	}
	os_free(fname);

	hapd->ctrl_sock = s;
	if (eloop_register_read_sock(s, hostapd_ctrl_iface_receive, hapd,
				     NULL) < 0) {
		hostapd_ctrl_iface_deinit(hapd);
		return -1;
	}
	hapd->msg_ctx = hapd;
	wpa_msg_register_cb(hostapd_ctrl_iface_msg_cb);

	return 0;

fail:
	if (s >= 0)
		close(s);
	if (fname) {
		unlink(fname);
		os_free(fname);
	}
	return -1;
#endif /* CONFIG_CTRL_IFACE_UDP */
}


void hostapd_ctrl_iface_deinit(struct hostapd_data *hapd)
{
	struct wpa_ctrl_dst *dst, *prev;

	if (hapd->ctrl_sock > -1) {
#ifndef CONFIG_CTRL_IFACE_UDP
		char *fname;
#endif /* !CONFIG_CTRL_IFACE_UDP */

		eloop_unregister_read_sock(hapd->ctrl_sock);
		close(hapd->ctrl_sock);
		hapd->ctrl_sock = -1;
#ifndef CONFIG_CTRL_IFACE_UDP
		fname = hostapd_ctrl_iface_path(hapd);
		if (fname)
			unlink(fname);
		os_free(fname);

		if (hapd->conf->ctrl_interface &&
		    rmdir(hapd->conf->ctrl_interface) < 0) {
			if (errno == ENOTEMPTY) {
				wpa_printf(MSG_DEBUG, "Control interface "
					   "directory not empty - leaving it "
					   "behind");
			} else {
				wpa_printf(MSG_ERROR,
					   "rmdir[ctrl_interface=%s]: %s",
					   hapd->conf->ctrl_interface,
					   strerror(errno));
			}
		}
#endif /* !CONFIG_CTRL_IFACE_UDP */
	}

	dl_list_for_each_safe(dst, prev, &hapd->ctrl_dst, struct wpa_ctrl_dst,
			      list)
		os_free(dst);

#ifdef CONFIG_TESTING_OPTIONS
	l2_packet_deinit(hapd->l2_test);
	hapd->l2_test = NULL;
#endif /* CONFIG_TESTING_OPTIONS */
}


static int hostapd_ctrl_iface_add(struct hapd_interfaces *interfaces,
				  char *buf)
{
	if (hostapd_add_iface(interfaces, buf) < 0) {
		wpa_printf(MSG_ERROR, "Adding interface %s failed", buf);
		return -1;
	}
	return 0;
}


static int hostapd_ctrl_iface_remove(struct hapd_interfaces *interfaces,
				     char *buf)
{
	if (hostapd_remove_iface(interfaces, buf) < 0) {
		wpa_printf(MSG_ERROR, "Removing interface %s failed", buf);
		return -1;
	}
	return 0;
}


static int hostapd_global_ctrl_iface_attach(struct hapd_interfaces *interfaces,
					    struct sockaddr_storage *from,
					    socklen_t fromlen, char *input)
{
	return ctrl_iface_attach(&interfaces->global_ctrl_dst, from, fromlen,
				 input);
}


static int hostapd_global_ctrl_iface_detach(struct hapd_interfaces *interfaces,
					    struct sockaddr_storage *from,
					    socklen_t fromlen)
{
	return ctrl_iface_detach(&interfaces->global_ctrl_dst, from, fromlen);
}


static void hostapd_ctrl_iface_flush(struct hapd_interfaces *interfaces)
{
#ifdef CONFIG_WPS_TESTING
	wps_version_number = 0x20;
	wps_testing_stub_cred = 0;
	wps_corrupt_pkhash = 0;
#endif /* CONFIG_WPS_TESTING */

#ifdef CONFIG_TESTING_OPTIONS
#ifdef CONFIG_DPP
	dpp_test = DPP_TEST_DISABLED;
#ifdef CONFIG_DPP3
	dpp_version_override = 3;
#elif defined(CONFIG_DPP2)
	dpp_version_override = 2;
#else /* CONFIG_DPP2 */
	dpp_version_override = 1;
#endif /* CONFIG_DPP2 */
#endif /* CONFIG_DPP */
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_DPP
	dpp_global_clear(interfaces->dpp);
#ifdef CONFIG_DPP3
	interfaces->dpp_pb_bi = NULL;
	{
		int i;

		for (i = 0; i < DPP_PB_INFO_COUNT; i++) {
			struct dpp_pb_info *info;

			info = &interfaces->dpp_pb[i];
			info->rx_time.sec = 0;
			info->rx_time.usec = 0;
		}
	}
#endif /* CONFIG_DPP3 */
#endif /* CONFIG_DPP */
}


#ifdef CONFIG_FST

static int
hostapd_global_ctrl_iface_fst_attach(struct hapd_interfaces *interfaces,
				     const char *cmd)
{
	char ifname[IFNAMSIZ + 1];
	struct fst_iface_cfg cfg;
	struct hostapd_data *hapd;
	struct fst_wpa_obj iface_obj;

	if (!fst_parse_attach_command(cmd, ifname, sizeof(ifname), &cfg)) {
		hapd = hostapd_get_iface(interfaces, ifname);
		if (hapd) {
			if (hapd->iface->fst) {
				wpa_printf(MSG_INFO, "FST: Already attached");
				return -1;
			}
			fst_hostapd_fill_iface_obj(hapd, &iface_obj);
			hapd->iface->fst = fst_attach(ifname, hapd->own_addr,
						      &iface_obj, &cfg);
			if (hapd->iface->fst)
				return 0;
		}
	}

	return -EINVAL;
}


static int
hostapd_global_ctrl_iface_fst_detach(struct hapd_interfaces *interfaces,
				     const char *cmd)
{
	char ifname[IFNAMSIZ + 1];
	struct hostapd_data * hapd;

	if (!fst_parse_detach_command(cmd, ifname, sizeof(ifname))) {
		hapd = hostapd_get_iface(interfaces, ifname);
		if (hapd) {
			if (!fst_iface_detach(ifname)) {
				hapd->iface->fst = NULL;
				hapd->iface->fst_ies = NULL;
				return 0;
			}
		}
	}

	return -EINVAL;
}

#endif /* CONFIG_FST */


static struct hostapd_data *
hostapd_interfaces_get_hapd(struct hapd_interfaces *interfaces,
			    const char *ifname)
{
	size_t i, j;

	for (i = 0; i < interfaces->count; i++) {
		struct hostapd_iface *iface = interfaces->iface[i];

		for (j = 0; j < iface->num_bss; j++) {
			struct hostapd_data *hapd;

			hapd = iface->bss[j];
			if (os_strcmp(ifname, hapd->conf->iface) == 0)
				return hapd;
		}
	}

	return NULL;
}


static int hostapd_ctrl_iface_dup_param(struct hostapd_data *src_hapd,
					struct hostapd_data *dst_hapd,
					const char *param)
{
	int res;
	char *value;

	value = os_zalloc(HOSTAPD_CLI_DUP_VALUE_MAX_LEN);
	if (!value) {
		wpa_printf(MSG_ERROR,
			   "DUP: cannot allocate buffer to stringify %s",
			   param);
		goto error_return;
	}

	if (os_strcmp(param, "wpa") == 0) {
		os_snprintf(value, HOSTAPD_CLI_DUP_VALUE_MAX_LEN, "%d",
			    src_hapd->conf->wpa);
	} else if (os_strcmp(param, "wpa_key_mgmt") == 0 &&
		   src_hapd->conf->wpa_key_mgmt) {
		res = hostapd_ctrl_iface_get_key_mgmt(
			src_hapd, value, HOSTAPD_CLI_DUP_VALUE_MAX_LEN);
		if (os_snprintf_error(HOSTAPD_CLI_DUP_VALUE_MAX_LEN, res))
			goto error_stringify;
	} else if (os_strcmp(param, "wpa_pairwise") == 0 &&
		   src_hapd->conf->wpa_pairwise) {
		res = wpa_write_ciphers(value,
					value + HOSTAPD_CLI_DUP_VALUE_MAX_LEN,
					src_hapd->conf->wpa_pairwise, " ");
		if (res < 0)
			goto error_stringify;
	} else if (os_strcmp(param, "rsn_pairwise") == 0 &&
		   src_hapd->conf->rsn_pairwise) {
		res = wpa_write_ciphers(value,
					value + HOSTAPD_CLI_DUP_VALUE_MAX_LEN,
					src_hapd->conf->rsn_pairwise, " ");
		if (res < 0)
			goto error_stringify;
	} else if (os_strcmp(param, "wpa_passphrase") == 0 &&
		   src_hapd->conf->ssid.wpa_passphrase) {
		os_snprintf(value, HOSTAPD_CLI_DUP_VALUE_MAX_LEN, "%s",
			    src_hapd->conf->ssid.wpa_passphrase);
	} else if (os_strcmp(param, "wpa_psk") == 0 &&
		   src_hapd->conf->ssid.wpa_psk_set) {
		wpa_snprintf_hex(value, HOSTAPD_CLI_DUP_VALUE_MAX_LEN,
			src_hapd->conf->ssid.wpa_psk->psk, PMK_LEN);
	} else {
		wpa_printf(MSG_WARNING, "DUP: %s cannot be duplicated", param);
		goto error_return;
	}

	res = hostapd_set_iface(dst_hapd->iconf, dst_hapd->conf, param, value);
	os_free(value);
	return res;

error_stringify:
	wpa_printf(MSG_ERROR, "DUP: cannot stringify %s", param);
error_return:
	os_free(value);
	return -1;
}


static int
hostapd_global_ctrl_iface_interfaces(struct hapd_interfaces *interfaces,
				     const char *input,
				     char *reply, int reply_size)
{
	size_t i, j;
	int res;
	char *pos, *end;
	struct hostapd_iface *iface;
	int show_ctrl = 0;

	if (input)
		show_ctrl = !!os_strstr(input, "ctrl");

	pos = reply;
	end = reply + reply_size;

	for (i = 0; i < interfaces->count; i++) {
		iface = interfaces->iface[i];

		for (j = 0; j < iface->num_bss; j++) {
			struct hostapd_bss_config *conf;

			conf = iface->conf->bss[j];
			if (show_ctrl)
				res = os_snprintf(pos, end - pos,
						  "%s ctrl_iface=%s\n",
						  conf->iface,
						  conf->ctrl_interface ?
						  conf->ctrl_interface : "N/A");
			else
				res = os_snprintf(pos, end - pos, "%s\n",
						  conf->iface);
			if (os_snprintf_error(end - pos, res)) {
				*pos = '\0';
				return pos - reply;
			}
			pos += res;
		}
	}

	return pos - reply;
}


static int
hostapd_global_ctrl_iface_dup_network(struct hapd_interfaces *interfaces,
				      char *cmd)
{
	char *p_start = cmd, *p_end;
	struct hostapd_data *src_hapd, *dst_hapd;

	/* cmd: "<src ifname> <dst ifname> <variable name> */

	p_end = os_strchr(p_start, ' ');
	if (!p_end) {
		wpa_printf(MSG_ERROR, "DUP: no src ifname found in cmd: '%s'",
			   cmd);
		return -1;
	}

	*p_end = '\0';
	src_hapd = hostapd_interfaces_get_hapd(interfaces, p_start);
	if (!src_hapd) {
		wpa_printf(MSG_ERROR, "DUP: no src ifname found: '%s'",
			   p_start);
		return -1;
	}

	p_start = p_end + 1;
	p_end = os_strchr(p_start, ' ');
	if (!p_end) {
		wpa_printf(MSG_ERROR, "DUP: no dst ifname found in cmd: '%s'",
			   cmd);
		return -1;
	}

	*p_end = '\0';
	dst_hapd = hostapd_interfaces_get_hapd(interfaces, p_start);
	if (!dst_hapd) {
		wpa_printf(MSG_ERROR, "DUP: no dst ifname found: '%s'",
			   p_start);
		return -1;
	}

	p_start = p_end + 1;
	return hostapd_ctrl_iface_dup_param(src_hapd, dst_hapd, p_start);
}


static int hostapd_global_ctrl_iface_ifname(struct hapd_interfaces *interfaces,
					    const char *ifname,
					    char *buf, char *reply,
					    int reply_size,
					    struct sockaddr_storage *from,
					    socklen_t fromlen)
{
	struct hostapd_data *hapd;

	hapd = hostapd_interfaces_get_hapd(interfaces, ifname);
	if (hapd == NULL) {
		int res;

		res = os_snprintf(reply, reply_size, "FAIL-NO-IFNAME-MATCH\n");
		if (os_snprintf_error(reply_size, res))
			return -1;
		return res;
	}

	return hostapd_ctrl_iface_receive_process(hapd, buf, reply,reply_size,
						  from, fromlen);
}


static void hostapd_global_ctrl_iface_receive(int sock, void *eloop_ctx,
					      void *sock_ctx)
{
	struct hapd_interfaces *interfaces = eloop_ctx;
	char buffer[256], *buf = buffer;
	int res;
	struct sockaddr_storage from;
	socklen_t fromlen = sizeof(from);
	char *reply;
	int reply_len;
	const int reply_size = 4096;
#ifdef CONFIG_CTRL_IFACE_UDP
	unsigned char lcookie[CTRL_IFACE_COOKIE_LEN];
#endif /* CONFIG_CTRL_IFACE_UDP */

	res = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (res < 0) {
		wpa_printf(MSG_ERROR, "recvfrom(ctrl_iface): %s",
			   strerror(errno));
		return;
	}
	buf[res] = '\0';
	wpa_printf(MSG_DEBUG, "Global ctrl_iface command: %s", buf);

	reply = os_malloc(reply_size);
	if (reply == NULL) {
		if (sendto(sock, "FAIL\n", 5, 0, (struct sockaddr *) &from,
			   fromlen) < 0) {
			wpa_printf(MSG_DEBUG, "CTRL: sendto failed: %s",
				   strerror(errno));
		}
		return;
	}

	os_memcpy(reply, "OK\n", 3);
	reply_len = 3;

#ifdef CONFIG_CTRL_IFACE_UDP
	if (os_strcmp(buf, "GET_COOKIE") == 0) {
		os_memcpy(reply, "COOKIE=", 7);
		wpa_snprintf_hex(reply + 7, 2 * CTRL_IFACE_COOKIE_LEN + 1,
				 interfaces->ctrl_iface_cookie,
				 CTRL_IFACE_COOKIE_LEN);
		reply_len = 7 + 2 * CTRL_IFACE_COOKIE_LEN;
		goto send_reply;
	}

	if (os_strncmp(buf, "COOKIE=", 7) != 0 ||
	    hexstr2bin(buf + 7, lcookie, CTRL_IFACE_COOKIE_LEN) < 0) {
		wpa_printf(MSG_DEBUG,
			   "CTRL: No cookie in the request - drop request");
		os_free(reply);
		return;
	}

	if (os_memcmp(interfaces->ctrl_iface_cookie, lcookie,
		      CTRL_IFACE_COOKIE_LEN) != 0) {
		wpa_printf(MSG_DEBUG,
			   "CTRL: Invalid cookie in the request - drop request");
		os_free(reply);
		return;
	}

	buf += 7 + 2 * CTRL_IFACE_COOKIE_LEN;
	while (*buf == ' ')
		buf++;
#endif /* CONFIG_CTRL_IFACE_UDP */

	if (os_strncmp(buf, "IFNAME=", 7) == 0) {
		char *pos = os_strchr(buf + 7, ' ');

		if (pos) {
			*pos++ = '\0';
			reply_len = hostapd_global_ctrl_iface_ifname(
				interfaces, buf + 7, pos, reply, reply_size,
				&from, fromlen);
			goto send_reply;
		}
	}

	if (os_strcmp(buf, "PING") == 0) {
		os_memcpy(reply, "PONG\n", 5);
		reply_len = 5;
	} else if (os_strncmp(buf, "RELOG", 5) == 0) {
		if (wpa_debug_reopen_file() < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "FLUSH") == 0) {
		hostapd_ctrl_iface_flush(interfaces);
	} else if (os_strncmp(buf, "ADD ", 4) == 0) {
		if (hostapd_ctrl_iface_add(interfaces, buf + 4) < 0)
			reply_len = -1;
	} else if (os_strncmp(buf, "REMOVE ", 7) == 0) {
		if (hostapd_ctrl_iface_remove(interfaces, buf + 7) < 0)
			reply_len = -1;
	} else if (os_strcmp(buf, "ATTACH") == 0) {
		if (hostapd_global_ctrl_iface_attach(interfaces, &from,
						     fromlen, NULL))
			reply_len = -1;
	} else if (os_strncmp(buf, "ATTACH ", 7) == 0) {
		if (hostapd_global_ctrl_iface_attach(interfaces, &from,
						     fromlen, buf + 7))
			reply_len = -1;
	} else if (os_strcmp(buf, "DETACH") == 0) {
		if (hostapd_global_ctrl_iface_detach(interfaces, &from,
			fromlen))
			reply_len = -1;
#ifdef CONFIG_MODULE_TESTS
	} else if (os_strcmp(buf, "MODULE_TESTS") == 0) {
		if (hapd_module_tests() < 0)
			reply_len = -1;
#endif /* CONFIG_MODULE_TESTS */
#ifdef CONFIG_FST
	} else if (os_strncmp(buf, "FST-ATTACH ", 11) == 0) {
		if (!hostapd_global_ctrl_iface_fst_attach(interfaces, buf + 11))
			reply_len = os_snprintf(reply, reply_size, "OK\n");
		else
			reply_len = -1;
	} else if (os_strncmp(buf, "FST-DETACH ", 11) == 0) {
		if (!hostapd_global_ctrl_iface_fst_detach(interfaces, buf + 11))
			reply_len = os_snprintf(reply, reply_size, "OK\n");
		else
			reply_len = -1;
	} else if (os_strncmp(buf, "FST-MANAGER ", 12) == 0) {
		reply_len = fst_ctrl_iface_receive(buf + 12, reply, reply_size);
#endif /* CONFIG_FST */
	} else if (os_strncmp(buf, "DUP_NETWORK ", 12) == 0) {
		if (!hostapd_global_ctrl_iface_dup_network(interfaces,
							   buf + 12))
			reply_len = os_snprintf(reply, reply_size, "OK\n");
		else
			reply_len = -1;
	} else if (os_strncmp(buf, "INTERFACES", 10) == 0) {
		reply_len = hostapd_global_ctrl_iface_interfaces(
			interfaces, buf + 10, reply, reply_size);
	} else if (os_strcmp(buf, "TERMINATE") == 0) {
		eloop_terminate();
	} else {
		wpa_printf(MSG_DEBUG, "Unrecognized global ctrl_iface command "
			   "ignored");
		reply_len = -1;
	}

send_reply:
	if (reply_len < 0) {
		os_memcpy(reply, "FAIL\n", 5);
		reply_len = 5;
	}

	if (sendto(sock, reply, reply_len, 0, (struct sockaddr *) &from,
		   fromlen) < 0) {
		wpa_printf(MSG_DEBUG, "CTRL: sendto failed: %s",
			   strerror(errno));
	}
	os_free(reply);
}


#ifndef CONFIG_CTRL_IFACE_UDP
static char * hostapd_global_ctrl_iface_path(struct hapd_interfaces *interface)
{
	char *buf;
	size_t len;

	if (interface->global_iface_path == NULL)
		return NULL;

	len = os_strlen(interface->global_iface_path) +
		os_strlen(interface->global_iface_name) + 2;
	buf = os_malloc(len);
	if (buf == NULL)
		return NULL;

	os_snprintf(buf, len, "%s/%s", interface->global_iface_path,
		    interface->global_iface_name);
	buf[len - 1] = '\0';
	return buf;
}
#endif /* CONFIG_CTRL_IFACE_UDP */


int hostapd_global_ctrl_iface_init(struct hapd_interfaces *interface)
{
#ifdef CONFIG_CTRL_IFACE_UDP
	int port = HOSTAPD_GLOBAL_CTRL_IFACE_PORT;
	char p[32] = { 0 };
	char *pos;
	struct addrinfo hints = { 0 }, *res, *saveres;
	int n;

	if (interface->global_ctrl_sock > -1) {
		wpa_printf(MSG_DEBUG, "ctrl_iface already exists!");
		return 0;
	}

	if (interface->global_iface_path == NULL)
		return 0;

	pos = os_strstr(interface->global_iface_path, "udp:");
	if (pos) {
		pos += 4;
		port = atoi(pos);
		if (port <= 0) {
			wpa_printf(MSG_ERROR, "Invalid global ctrl UDP port");
			goto fail;
		}
	}

	os_get_random(interface->ctrl_iface_cookie, CTRL_IFACE_COOKIE_LEN);

#ifdef CONFIG_CTRL_IFACE_UDP_REMOTE
	hints.ai_flags = AI_PASSIVE;
#endif /* CONFIG_CTRL_IFACE_UDP_REMOTE */

#ifdef CONFIG_CTRL_IFACE_UDP_IPV6
	hints.ai_family = AF_INET6;
#else /* CONFIG_CTRL_IFACE_UDP_IPV6 */
	hints.ai_family = AF_INET;
#endif /* CONFIG_CTRL_IFACE_UDP_IPV6 */
	hints.ai_socktype = SOCK_DGRAM;

try_again:
	os_snprintf(p, sizeof(p), "%d", port);
	n = getaddrinfo(NULL, p, &hints, &res);
	if (n) {
		wpa_printf(MSG_ERROR, "getaddrinfo(): %s", gai_strerror(n));
		goto fail;
	}

	saveres = res;
	interface->global_ctrl_sock = socket(res->ai_family, res->ai_socktype,
					     res->ai_protocol);
	if (interface->global_ctrl_sock < 0) {
		wpa_printf(MSG_ERROR, "socket(PF_INET): %s", strerror(errno));
		goto fail;
	}

	if (bind(interface->global_ctrl_sock, res->ai_addr, res->ai_addrlen) <
	    0) {
		port++;
		if ((port - HOSTAPD_GLOBAL_CTRL_IFACE_PORT) <
		    HOSTAPD_GLOBAL_CTRL_IFACE_PORT_LIMIT && !pos)
			goto try_again;
		wpa_printf(MSG_ERROR, "bind(AF_INET): %s", strerror(errno));
		goto fail;
	}

	freeaddrinfo(saveres);

	wpa_printf(MSG_DEBUG, "global ctrl_iface_init UDP port: %d", port);

	if (eloop_register_read_sock(interface->global_ctrl_sock,
				     hostapd_global_ctrl_iface_receive,
				     interface, NULL) < 0) {
		hostapd_global_ctrl_iface_deinit(interface);
		return -1;
	}

	wpa_msg_register_cb(hostapd_ctrl_iface_msg_cb);

	return 0;

fail:
	if (interface->global_ctrl_sock >= 0)
		close(interface->global_ctrl_sock);
	return -1;
#else /* CONFIG_CTRL_IFACE_UDP */
	struct sockaddr_un addr;
	int s = -1;
	char *fname = NULL;

	if (interface->global_iface_path == NULL) {
		wpa_printf(MSG_DEBUG, "ctrl_iface not configured!");
		return 0;
	}

	if (mkdir(interface->global_iface_path, S_IRWXU | S_IRWXG) < 0) {
		if (errno == EEXIST) {
			wpa_printf(MSG_DEBUG, "Using existing control "
				   "interface directory.");
		} else {
			wpa_printf(MSG_ERROR, "mkdir[ctrl_interface]: %s",
				   strerror(errno));
			goto fail;
		}
	} else if (interface->ctrl_iface_group &&
		   lchown(interface->global_iface_path, -1,
			  interface->ctrl_iface_group) < 0) {
		wpa_printf(MSG_ERROR, "lchown[ctrl_interface]: %s",
			   strerror(errno));
		goto fail;
	}

	if (os_strlen(interface->global_iface_path) + 1 +
	    os_strlen(interface->global_iface_name) >= sizeof(addr.sun_path))
		goto fail;

	s = socket(PF_UNIX, SOCK_DGRAM, 0);
	if (s < 0) {
		wpa_printf(MSG_ERROR, "socket(PF_UNIX): %s", strerror(errno));
		goto fail;
	}

	os_memset(&addr, 0, sizeof(addr));
#ifdef __FreeBSD__
	addr.sun_len = sizeof(addr);
#endif /* __FreeBSD__ */
	addr.sun_family = AF_UNIX;
	fname = hostapd_global_ctrl_iface_path(interface);
	if (fname == NULL)
		goto fail;
	os_strlcpy(addr.sun_path, fname, sizeof(addr.sun_path));
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		wpa_printf(MSG_DEBUG, "ctrl_iface bind(PF_UNIX) failed: %s",
			   strerror(errno));
		if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			wpa_printf(MSG_DEBUG, "ctrl_iface exists, but does not"
				   " allow connections - assuming it was left"
				   "over from forced program termination");
			if (unlink(fname) < 0) {
				wpa_printf(MSG_ERROR,
					   "Could not unlink existing ctrl_iface socket '%s': %s",
					   fname, strerror(errno));
				goto fail;
			}
			if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) <
			    0) {
				wpa_printf(MSG_ERROR, "bind(PF_UNIX): %s",
					   strerror(errno));
				goto fail;
			}
			wpa_printf(MSG_DEBUG, "Successfully replaced leftover "
				   "ctrl_iface socket '%s'", fname);
		} else {
			wpa_printf(MSG_INFO, "ctrl_iface exists and seems to "
				   "be in use - cannot override it");
			wpa_printf(MSG_INFO, "Delete '%s' manually if it is "
				   "not used anymore", fname);
			os_free(fname);
			fname = NULL;
			goto fail;
		}
	}

	if (interface->ctrl_iface_group &&
	    lchown(fname, -1, interface->ctrl_iface_group) < 0) {
		wpa_printf(MSG_ERROR, "lchown[ctrl_interface]: %s",
			   strerror(errno));
		goto fail;
	}

	if (chmod(fname, S_IRWXU | S_IRWXG) < 0) {
		wpa_printf(MSG_ERROR, "chmod[ctrl_interface/ifname]: %s",
			   strerror(errno));
		goto fail;
	}
	os_free(fname);

	interface->global_ctrl_sock = s;
	eloop_register_read_sock(s, hostapd_global_ctrl_iface_receive,
				 interface, NULL);

	wpa_msg_register_cb(hostapd_ctrl_iface_msg_cb);

	return 0;

fail:
	if (s >= 0)
		close(s);
	if (fname) {
		unlink(fname);
		os_free(fname);
	}
	return -1;
#endif /* CONFIG_CTRL_IFACE_UDP */
}


void hostapd_global_ctrl_iface_deinit(struct hapd_interfaces *interfaces)
{
#ifndef CONFIG_CTRL_IFACE_UDP
	char *fname = NULL;
#endif /* CONFIG_CTRL_IFACE_UDP */
	struct wpa_ctrl_dst *dst, *prev;

	if (interfaces->global_ctrl_sock > -1) {
		eloop_unregister_read_sock(interfaces->global_ctrl_sock);
		close(interfaces->global_ctrl_sock);
		interfaces->global_ctrl_sock = -1;
#ifndef CONFIG_CTRL_IFACE_UDP
		fname = hostapd_global_ctrl_iface_path(interfaces);
		if (fname) {
			unlink(fname);
			os_free(fname);
		}

		if (interfaces->global_iface_path &&
		    rmdir(interfaces->global_iface_path) < 0) {
			if (errno == ENOTEMPTY) {
				wpa_printf(MSG_DEBUG, "Control interface "
					   "directory not empty - leaving it "
					   "behind");
			} else {
				wpa_printf(MSG_ERROR,
					   "rmdir[ctrl_interface=%s]: %s",
					   interfaces->global_iface_path,
					   strerror(errno));
			}
		}
#endif /* CONFIG_CTRL_IFACE_UDP */
	}

	os_free(interfaces->global_iface_path);
	interfaces->global_iface_path = NULL;

	dl_list_for_each_safe(dst, prev, &interfaces->global_ctrl_dst,
			      struct wpa_ctrl_dst, list)
		os_free(dst);
}


static int hostapd_ctrl_check_event_enabled(struct wpa_ctrl_dst *dst,
					    const char *buf)
{
	/* Enable Probe Request events based on explicit request.
	 * Other events are enabled by default.
	 */
	if (str_starts(buf, RX_PROBE_REQUEST))
		return !!(dst->events & WPA_EVENT_RX_PROBE_REQUEST);
	return 1;
}


static void hostapd_ctrl_iface_send_internal(int sock, struct dl_list *ctrl_dst,
					     const char *ifname, int level,
					     const char *buf, size_t len)
{
	struct wpa_ctrl_dst *dst, *next;
	struct msghdr msg;
	int idx, res;
	struct iovec io[5];
	char levelstr[10];

	if (sock < 0 || dl_list_empty(ctrl_dst))
		return;

	res = os_snprintf(levelstr, sizeof(levelstr), "<%d>", level);
	if (os_snprintf_error(sizeof(levelstr), res))
		return;
	idx = 0;
	if (ifname) {
		io[idx].iov_base = "IFNAME=";
		io[idx].iov_len = 7;
		idx++;
		io[idx].iov_base = (char *) ifname;
		io[idx].iov_len = os_strlen(ifname);
		idx++;
		io[idx].iov_base = " ";
		io[idx].iov_len = 1;
		idx++;
	}
	io[idx].iov_base = levelstr;
	io[idx].iov_len = os_strlen(levelstr);
	idx++;
	io[idx].iov_base = (char *) buf;
	io[idx].iov_len = len;
	idx++;
	os_memset(&msg, 0, sizeof(msg));
	msg.msg_iov = io;
	msg.msg_iovlen = idx;

	idx = 0;
	dl_list_for_each_safe(dst, next, ctrl_dst, struct wpa_ctrl_dst, list) {
		if ((level >= dst->debug_level) &&
		     hostapd_ctrl_check_event_enabled(dst, buf)) {
			sockaddr_print(MSG_DEBUG, "CTRL_IFACE monitor send",
				       &dst->addr, dst->addrlen);
			msg.msg_name = &dst->addr;
			msg.msg_namelen = dst->addrlen;
			if (sendmsg(sock, &msg, 0) < 0) {
				int _errno = errno;
				wpa_printf(MSG_INFO, "CTRL_IFACE monitor[%d]: "
					   "%d - %s",
					   idx, errno, strerror(errno));
				dst->errors++;
				if (dst->errors > 10 || _errno == ENOENT) {
					ctrl_iface_detach(ctrl_dst,
							  &dst->addr,
							  dst->addrlen);
				}
			} else
				dst->errors = 0;
		}
		idx++;
	}
}


static void hostapd_ctrl_iface_send(struct hostapd_data *hapd, int level,
				    enum wpa_msg_type type,
				    const char *buf, size_t len)
{
	if (type != WPA_MSG_NO_GLOBAL) {
		hostapd_ctrl_iface_send_internal(
			hapd->iface->interfaces->global_ctrl_sock,
			&hapd->iface->interfaces->global_ctrl_dst,
			type != WPA_MSG_PER_INTERFACE ?
			NULL : hapd->conf->iface,
			level, buf, len);
	}

	if (type != WPA_MSG_ONLY_GLOBAL) {
		hostapd_ctrl_iface_send_internal(
			hapd->ctrl_sock, &hapd->ctrl_dst,
			NULL, level, buf, len);
	}
}

#endif /* CONFIG_NATIVE_WINDOWS */
