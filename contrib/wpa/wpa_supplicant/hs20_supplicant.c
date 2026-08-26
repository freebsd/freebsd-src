/*
 * Copyright (c) 2009, Atheros Communications, Inc.
 * Copyright (c) 2011-2013, Qualcomm Atheros, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <sys/stat.h>

#include "common.h"
#include "eloop.h"
#include "common/ieee802_11_common.h"
#include "common/ieee802_11_defs.h"
#include "common/gas.h"
#include "common/wpa_ctrl.h"
#include "rsn_supp/wpa.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "config.h"
#include "scan.h"
#include "bss.h"
#include "bssid_ignore.h"
#include "gas_query.h"
#include "interworking.h"
#include "hs20_supplicant.h"
#include "base64.h"
#include "notify.h"


void wpas_hs20_add_indication(struct wpabuf *buf, int pps_mo_id, int ap_release)
{
	int release;
	u8 conf;

	release = (HS20_VERSION >> 4) + 1;
	if (ap_release > 0 && release > ap_release)
		release = ap_release;
	if (release < 2)
		pps_mo_id = -1;

	wpabuf_put_u8(buf, WLAN_EID_VENDOR_SPECIFIC);
	wpabuf_put_u8(buf, pps_mo_id >= 0 ? 7 : 5);
	wpabuf_put_be24(buf, OUI_WFA);
	wpabuf_put_u8(buf, HS20_INDICATION_OUI_TYPE);
	conf = (release - 1) << 4;
	if (pps_mo_id >= 0)
		conf |= HS20_PPS_MO_ID_PRESENT;
	wpabuf_put_u8(buf, conf);
	if (pps_mo_id >= 0)
		wpabuf_put_le16(buf, pps_mo_id);
}


void wpas_hs20_add_roam_cons_sel(struct wpabuf *buf,
				 const struct wpa_ssid *ssid)
{
	if (!ssid->roaming_consortium_selection ||
	    !ssid->roaming_consortium_selection_len)
		return;

	wpabuf_put_u8(buf, WLAN_EID_VENDOR_SPECIFIC);
	wpabuf_put_u8(buf, 4 + ssid->roaming_consortium_selection_len);
	wpabuf_put_be24(buf, OUI_WFA);
	wpabuf_put_u8(buf, HS20_ROAMING_CONS_SEL_OUI_TYPE);
	wpabuf_put_data(buf, ssid->roaming_consortium_selection,
			ssid->roaming_consortium_selection_len);
}


int get_hs20_version(struct wpa_bss *bss)
{
	const u8 *ie;

	if (!bss)
		return 0;

	ie = wpa_bss_get_vendor_ie(bss, HS20_IE_VENDOR_TYPE);
	if (!ie || ie[1] < 5)
		return 0;

	return ((ie[6] >> 4) & 0x0f) + 1;
}


int is_hs20_network(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid,
		    struct wpa_bss *bss)
{
	if (!wpa_s->conf->hs20 || !ssid)
		return 0;

	if (ssid->parent_cred)
		return 1;

	if (bss && !wpa_bss_get_vendor_ie(bss, HS20_IE_VENDOR_TYPE))
		return 0;

	/*
	 * This may catch some non-Hotspot 2.0 cases, but it is safer to do that
	 * than cause Hotspot 2.0 connections without indication element getting
	 * added. Non-Hotspot 2.0 APs should ignore the unknown vendor element.
	 */

	if (!(ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X))
		return 0;
	if (!(ssid->pairwise_cipher & WPA_CIPHER_CCMP))
		return 0;
	if (ssid->proto != WPA_PROTO_RSN)
		return 0;

	return 1;
}


int hs20_get_pps_mo_id(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid)
{
	struct wpa_cred *cred;

	if (ssid == NULL)
		return 0;

	if (ssid->update_identifier)
		return ssid->update_identifier;

	if (ssid->parent_cred == NULL)
		return 0;

	for (cred = wpa_s->conf->cred; cred; cred = cred->next) {
		if (ssid->parent_cred == cred)
			return cred->update_identifier;
	}

	return 0;
}


void hs20_put_anqp_req(u32 stypes, const u8 *payload, size_t payload_len,
		       struct wpabuf *buf)
{
	u8 *len_pos;

	if (buf == NULL)
		return;

	len_pos = gas_anqp_add_element(buf, ANQP_VENDOR_SPECIFIC);
	wpabuf_put_be24(buf, OUI_WFA);
	wpabuf_put_u8(buf, HS20_ANQP_OUI_TYPE);
	if (stypes == BIT(HS20_STYPE_NAI_HOME_REALM_QUERY)) {
		wpabuf_put_u8(buf, HS20_STYPE_NAI_HOME_REALM_QUERY);
		wpabuf_put_u8(buf, 0); /* Reserved */
		if (payload)
			wpabuf_put_data(buf, payload, payload_len);
	} else {
		u8 i;
		wpabuf_put_u8(buf, HS20_STYPE_QUERY_LIST);
		wpabuf_put_u8(buf, 0); /* Reserved */
		for (i = 0; i < 32; i++) {
			if (stypes & BIT(i))
				wpabuf_put_u8(buf, i);
		}
	}
	gas_anqp_set_element_len(buf, len_pos);

	gas_anqp_set_len(buf);
}


static struct wpabuf * hs20_build_anqp_req(u32 stypes, const u8 *payload,
					   size_t payload_len)
{
	struct wpabuf *buf;

	buf = gas_anqp_build_initial_req(0, 100 + payload_len);
	if (buf == NULL)
		return NULL;

	hs20_put_anqp_req(stypes, payload, payload_len, buf);

	return buf;
}


int hs20_anqp_send_req(struct wpa_supplicant *wpa_s, const u8 *dst, u32 stypes,
		       const u8 *payload, size_t payload_len)
{
	struct wpabuf *buf;
	int ret = 0;
	int freq;
	struct wpa_bss *bss;
	int res;

	bss = wpa_bss_get_bssid(wpa_s, dst);
	if (!bss) {
		wpa_printf(MSG_WARNING,
			   "ANQP: Cannot send query to unknown BSS "
			   MACSTR, MAC2STR(dst));
		return -1;
	}

	wpa_bss_anqp_unshare_alloc(bss);
	freq = bss->freq;

	wpa_printf(MSG_DEBUG, "HS20: ANQP Query Request to " MACSTR " for "
		   "subtypes 0x%x", MAC2STR(dst), stypes);

	buf = hs20_build_anqp_req(stypes, payload, payload_len);
	if (buf == NULL)
		return -1;

	res = gas_query_req(wpa_s->gas, dst, freq, 0, 0, buf, anqp_resp_cb,
			    wpa_s);
	if (res < 0) {
		wpa_printf(MSG_DEBUG, "ANQP: Failed to send Query Request");
		wpabuf_free(buf);
		return -1;
	} else
		wpa_printf(MSG_DEBUG, "ANQP: Query started with dialog token "
			   "%u", res);

	return ret;
}


void hs20_parse_rx_hs20_anqp_resp(struct wpa_supplicant *wpa_s,
				  struct wpa_bss *bss, const u8 *sa,
				  const u8 *data, size_t slen, u8 dialog_token)
{
	const u8 *pos = data;
	u8 subtype;
	struct wpa_bss_anqp *anqp = NULL;

	if (slen < 2)
		return;

	if (bss)
		anqp = bss->anqp;

	subtype = *pos++;
	slen--;

	pos++; /* Reserved */
	slen--;

	switch (subtype) {
	case HS20_STYPE_CAPABILITY_LIST:
		wpa_msg(wpa_s, MSG_INFO, RX_HS20_ANQP MACSTR
			" HS Capability List", MAC2STR(sa));
		wpa_hexdump_ascii(MSG_DEBUG, "HS Capability List", pos, slen);
		if (anqp) {
			wpabuf_free(anqp->hs20_capability_list);
			anqp->hs20_capability_list =
				wpabuf_alloc_copy(pos, slen);
		}
		break;
	case HS20_STYPE_OPERATOR_FRIENDLY_NAME:
		wpa_msg(wpa_s, MSG_INFO, RX_HS20_ANQP MACSTR
			" Operator Friendly Name", MAC2STR(sa));
		wpa_hexdump_ascii(MSG_DEBUG, "oper friendly name", pos, slen);
		if (anqp) {
			wpabuf_free(anqp->hs20_operator_friendly_name);
			anqp->hs20_operator_friendly_name =
				wpabuf_alloc_copy(pos, slen);
		}
		break;
	case HS20_STYPE_WAN_METRICS:
		wpa_hexdump(MSG_DEBUG, "WAN Metrics", pos, slen);
		if (slen < 13) {
			wpa_dbg(wpa_s, MSG_DEBUG, "HS 2.0: Too short WAN "
				"Metrics value from " MACSTR, MAC2STR(sa));
			break;
		}
		wpa_msg(wpa_s, MSG_INFO, RX_HS20_ANQP MACSTR
			" WAN Metrics %02x:%u:%u:%u:%u:%u", MAC2STR(sa),
			pos[0], WPA_GET_LE32(pos + 1), WPA_GET_LE32(pos + 5),
			pos[9], pos[10], WPA_GET_LE16(pos + 11));
		if (anqp) {
			wpabuf_free(anqp->hs20_wan_metrics);
			anqp->hs20_wan_metrics = wpabuf_alloc_copy(pos, slen);
		}
		break;
	case HS20_STYPE_CONNECTION_CAPABILITY:
		wpa_msg(wpa_s, MSG_INFO, RX_HS20_ANQP MACSTR
			" Connection Capability", MAC2STR(sa));
		wpa_hexdump_ascii(MSG_DEBUG, "conn capability", pos, slen);
		if (anqp) {
			wpabuf_free(anqp->hs20_connection_capability);
			anqp->hs20_connection_capability =
				wpabuf_alloc_copy(pos, slen);
		}
		break;
	case HS20_STYPE_OPERATING_CLASS:
		wpa_msg(wpa_s, MSG_INFO, RX_HS20_ANQP MACSTR
			" Operating Class", MAC2STR(sa));
		wpa_hexdump_ascii(MSG_DEBUG, "Operating Class", pos, slen);
		if (anqp) {
			wpabuf_free(anqp->hs20_operating_class);
			anqp->hs20_operating_class =
				wpabuf_alloc_copy(pos, slen);
		}
		break;
	default:
		wpa_printf(MSG_DEBUG, "HS20: Unsupported subtype %u", subtype);
		break;
	}
}


void hs20_rx_deauth_imminent_notice(struct wpa_supplicant *wpa_s, u8 code,
				    u16 reauth_delay, const char *url)
{
	if (!wpa_sm_pmf_enabled(wpa_s->wpa)) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Ignore deauthentication imminent notice since PMF was not enabled");
		return;
	}

	wpa_msg(wpa_s, MSG_INFO, HS20_DEAUTH_IMMINENT_NOTICE "%u %u %s",
		code, reauth_delay, url);

	if (code == HS20_DEAUTH_REASON_CODE_BSS) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Add BSS to ignore list");
		wpa_bssid_ignore_add(wpa_s, wpa_s->bssid);
		/* TODO: For now, disable full ESS since some drivers may not
		 * support disabling per BSS. */
		if (wpa_s->current_ssid) {
			struct os_reltime now;
			os_get_reltime(&now);
			if (now.sec + reauth_delay <=
			    wpa_s->current_ssid->disabled_until.sec)
				return;
			wpa_printf(MSG_DEBUG, "HS 2.0: Disable network for %u seconds (BSS)",
				   reauth_delay);
			wpa_s->current_ssid->disabled_until.sec =
				now.sec + reauth_delay;
		}
	}

	if (code == HS20_DEAUTH_REASON_CODE_ESS && wpa_s->current_ssid) {
		struct os_reltime now;
		os_get_reltime(&now);
		if (now.sec + reauth_delay <=
		    wpa_s->current_ssid->disabled_until.sec)
			return;
		wpa_printf(MSG_DEBUG, "HS 2.0: Disable network for %u seconds",
			   reauth_delay);
		wpa_s->current_ssid->disabled_until.sec =
			now.sec + reauth_delay;
	}
}


void hs20_rx_t_c_acceptance(struct wpa_supplicant *wpa_s, const char *url)
{
	if (!wpa_sm_pmf_enabled(wpa_s->wpa)) {
		wpa_printf(MSG_DEBUG,
			   "HS 2.0: Ignore Terms and Conditions Acceptance since PMF was not enabled");
		return;
	}

	wpas_notify_hs20_t_c_acceptance(wpa_s, url);
}
