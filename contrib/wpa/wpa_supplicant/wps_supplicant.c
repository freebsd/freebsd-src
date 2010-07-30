/*
 * wpa_supplicant / WPS integration
 * Copyright (c) 2008, Jouni Malinen <j@w1.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#include "includes.h"

#include "common.h"
#include "ieee802_11_defs.h"
#include "wpa_common.h"
#include "config.h"
#include "eap_peer/eap.h"
#include "wpa_supplicant_i.h"
#include "eloop.h"
#include "uuid.h"
#include "wpa_ctrl.h"
#include "ctrl_iface_dbus.h"
#include "eap_common/eap_wsc_common.h"
#include "blacklist.h"
#include "wpa.h"
#include "wps_supplicant.h"


#define WPS_PIN_SCAN_IGNORE_SEL_REG 3

static void wpas_wps_timeout(void *eloop_ctx, void *timeout_ctx);
static void wpas_clear_wps(struct wpa_supplicant *wpa_s);


int wpas_wps_eapol_cb(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->wps_success &&
	    wpa_s->current_ssid &&
	    eap_is_wps_pin_enrollee(&wpa_s->current_ssid->eap)) {
		const u8 *bssid = wpa_s->bssid;
		if (is_zero_ether_addr(bssid))
			bssid = wpa_s->pending_bssid;

		wpa_printf(MSG_DEBUG, "WPS: PIN registration with " MACSTR
			   " did not succeed - continue trying to find "
			   "suitable AP", MAC2STR(bssid));
		wpa_blacklist_add(wpa_s, bssid);

		wpa_supplicant_deauthenticate(wpa_s,
					      WLAN_REASON_DEAUTH_LEAVING);
		wpa_s->reassociate = 1;
		wpa_supplicant_req_scan(wpa_s,
					wpa_s->blacklist_cleared ? 5 : 0, 0);
		wpa_s->blacklist_cleared = 0;
		return 1;
	}

	eloop_cancel_timeout(wpas_wps_timeout, wpa_s, NULL);

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_WPS && wpa_s->current_ssid &&
	    !(wpa_s->current_ssid->key_mgmt & WPA_KEY_MGMT_WPS)) {
		wpa_printf(MSG_DEBUG, "WPS: Network configuration replaced - "
			   "try to associate with the received credential");
		wpa_supplicant_deauthenticate(wpa_s,
					      WLAN_REASON_DEAUTH_LEAVING);
		wpa_s->reassociate = 1;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
		return 1;
	}

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_WPS && wpa_s->current_ssid) {
		wpa_printf(MSG_DEBUG, "WPS: Registration completed - waiting "
			   "for external credential processing");
		wpas_clear_wps(wpa_s);
		wpa_supplicant_deauthenticate(wpa_s,
					      WLAN_REASON_DEAUTH_LEAVING);
		return 1;
	}

	return 0;
}


static void wpas_wps_security_workaround(struct wpa_supplicant *wpa_s,
					 struct wpa_ssid *ssid,
					 const struct wps_credential *cred)
{
	struct wpa_driver_capa capa;
	size_t i;
	struct wpa_scan_res *bss;
	const u8 *ie;
	struct wpa_ie_data adv;
	int wpa2 = 0, ccmp = 0;

	/*
	 * Many existing WPS APs do not know how to negotiate WPA2 or CCMP in
	 * case they are configured for mixed mode operation (WPA+WPA2 and
	 * TKIP+CCMP). Try to use scan results to figure out whether the AP
	 * actually supports stronger security and select that if the client
	 * has support for it, too.
	 */

	if (wpa_drv_get_capa(wpa_s, &capa))
		return; /* Unknown what driver supports */

	if (wpa_supplicant_get_scan_results(wpa_s) || wpa_s->scan_res == NULL)
		return; /* Could not get scan results for checking advertised
			 * parameters */

	for (i = 0; i < wpa_s->scan_res->num; i++) {
		bss = wpa_s->scan_res->res[i];
		if (os_memcmp(bss->bssid, cred->mac_addr, ETH_ALEN) != 0)
			continue;
		ie = wpa_scan_get_ie(bss, WLAN_EID_SSID);
		if (ie == NULL)
			continue;
		if (ie[1] != ssid->ssid_len || ssid->ssid == NULL ||
		    os_memcmp(ie + 2, ssid->ssid, ssid->ssid_len) != 0)
			continue;

		wpa_printf(MSG_DEBUG, "WPS: AP found from scan results");
		break;
	}

	if (i == wpa_s->scan_res->num) {
		wpa_printf(MSG_DEBUG, "WPS: The AP was not found from scan "
			   "results - use credential as-is");
		return;
	}

	ie = wpa_scan_get_ie(bss, WLAN_EID_RSN);
	if (ie && wpa_parse_wpa_ie(ie, 2 + ie[1], &adv) == 0) {
		wpa2 = 1;
		if (adv.pairwise_cipher & WPA_CIPHER_CCMP)
			ccmp = 1;
	} else {
		ie = wpa_scan_get_vendor_ie(bss, WPA_IE_VENDOR_TYPE);
		if (ie && wpa_parse_wpa_ie(ie, 2 + ie[1], &adv) == 0 &&
		    adv.pairwise_cipher & WPA_CIPHER_CCMP)
			ccmp = 1;
	}

	if (ie == NULL && (ssid->proto & WPA_PROTO_WPA) &&
	    (ssid->pairwise_cipher & WPA_CIPHER_TKIP)) {
		/*
		 * TODO: This could be the initial AP configuration and the
		 * Beacon contents could change shortly. Should request a new
		 * scan and delay addition of the network until the updated
		 * scan results are available.
		 */
		wpa_printf(MSG_DEBUG, "WPS: The AP did not yet advertise WPA "
			   "support - use credential as-is");
		return;
	}

	if (ccmp && !(ssid->pairwise_cipher & WPA_CIPHER_CCMP) &&
	    (ssid->pairwise_cipher & WPA_CIPHER_TKIP) &&
	    (capa.key_mgmt & WPA_DRIVER_CAPA_KEY_MGMT_WPA2_PSK)) {
		wpa_printf(MSG_DEBUG, "WPS: Add CCMP into the credential "
			   "based on scan results");
		if (wpa_s->conf->ap_scan == 1)
			ssid->pairwise_cipher |= WPA_CIPHER_CCMP;
		else
			ssid->pairwise_cipher = WPA_CIPHER_CCMP;
	}

	if (wpa2 && !(ssid->proto & WPA_PROTO_RSN) &&
	    (ssid->proto & WPA_PROTO_WPA) &&
	    (capa.enc & WPA_DRIVER_CAPA_ENC_CCMP)) {
		wpa_printf(MSG_DEBUG, "WPS: Add WPA2 into the credential "
			   "based on scan results");
		if (wpa_s->conf->ap_scan == 1)
			ssid->proto |= WPA_PROTO_RSN;
		else
			ssid->proto = WPA_PROTO_RSN;
	}
}


static int wpa_supplicant_wps_cred(void *ctx,
				   const struct wps_credential *cred)
{
	struct wpa_supplicant *wpa_s = ctx;
	struct wpa_ssid *ssid = wpa_s->current_ssid;
	u8 key_idx = 0;
	u16 auth_type;

	if ((wpa_s->conf->wps_cred_processing == 1 ||
	     wpa_s->conf->wps_cred_processing == 2) && cred->cred_attr) {
		size_t blen = cred->cred_attr_len * 2 + 1;
		char *buf = os_malloc(blen);
		if (buf) {
			wpa_snprintf_hex(buf, blen,
					 cred->cred_attr, cred->cred_attr_len);
			wpa_msg(wpa_s, MSG_INFO, "%s%s",
				WPS_EVENT_CRED_RECEIVED, buf);
			os_free(buf);
		}
		wpa_supplicant_dbus_notify_wps_cred(wpa_s, cred);
	} else
		wpa_msg(wpa_s, MSG_INFO, WPS_EVENT_CRED_RECEIVED);

	wpa_hexdump_key(MSG_DEBUG, "WPS: Received Credential attribute",
			cred->cred_attr, cred->cred_attr_len);

	if (wpa_s->conf->wps_cred_processing == 1)
		return 0;

	wpa_hexdump_ascii(MSG_DEBUG, "WPS: SSID", cred->ssid, cred->ssid_len);
	wpa_printf(MSG_DEBUG, "WPS: Authentication Type 0x%x",
		   cred->auth_type);
	wpa_printf(MSG_DEBUG, "WPS: Encryption Type 0x%x", cred->encr_type);
	wpa_printf(MSG_DEBUG, "WPS: Network Key Index %d", cred->key_idx);
	wpa_hexdump_key(MSG_DEBUG, "WPS: Network Key",
			cred->key, cred->key_len);
	wpa_printf(MSG_DEBUG, "WPS: MAC Address " MACSTR,
		   MAC2STR(cred->mac_addr));

	auth_type = cred->auth_type;
	if (auth_type == (WPS_AUTH_WPAPSK | WPS_AUTH_WPA2PSK)) {
		wpa_printf(MSG_DEBUG, "WPS: Workaround - convert mixed-mode "
			   "auth_type into WPA2PSK");
		auth_type = WPS_AUTH_WPA2PSK;
	}

	if (auth_type != WPS_AUTH_OPEN &&
	    auth_type != WPS_AUTH_SHARED &&
	    auth_type != WPS_AUTH_WPAPSK &&
	    auth_type != WPS_AUTH_WPA2PSK) {
		wpa_printf(MSG_DEBUG, "WPS: Ignored credentials for "
			   "unsupported authentication type 0x%x",
			   auth_type);
		return 0;
	}

	if (ssid && (ssid->key_mgmt & WPA_KEY_MGMT_WPS)) {
		wpa_printf(MSG_DEBUG, "WPS: Replace WPS network block based "
			   "on the received credential");
		os_free(ssid->eap.identity);
		ssid->eap.identity = NULL;
		ssid->eap.identity_len = 0;
		os_free(ssid->eap.phase1);
		ssid->eap.phase1 = NULL;
		os_free(ssid->eap.eap_methods);
		ssid->eap.eap_methods = NULL;
	} else {
		wpa_printf(MSG_DEBUG, "WPS: Create a new network based on the "
			   "received credential");
		ssid = wpa_config_add_network(wpa_s->conf);
		if (ssid == NULL)
			return -1;
	}

	wpa_config_set_network_defaults(ssid);

	os_free(ssid->ssid);
	ssid->ssid = os_malloc(cred->ssid_len);
	if (ssid->ssid) {
		os_memcpy(ssid->ssid, cred->ssid, cred->ssid_len);
		ssid->ssid_len = cred->ssid_len;
	}

	switch (cred->encr_type) {
	case WPS_ENCR_NONE:
		break;
	case WPS_ENCR_WEP:
		if (cred->key_len <= 0)
			break;
		if (cred->key_len != 5 && cred->key_len != 13 &&
		    cred->key_len != 10 && cred->key_len != 26) {
			wpa_printf(MSG_ERROR, "WPS: Invalid WEP Key length "
				   "%lu", (unsigned long) cred->key_len);
			return -1;
		}
		if (cred->key_idx > NUM_WEP_KEYS) {
			wpa_printf(MSG_ERROR, "WPS: Invalid WEP Key index %d",
				   cred->key_idx);
			return -1;
		}
		if (cred->key_idx)
			key_idx = cred->key_idx - 1;
		if (cred->key_len == 10 || cred->key_len == 26) {
			if (hexstr2bin((char *) cred->key,
				       ssid->wep_key[key_idx],
				       cred->key_len / 2) < 0) {
				wpa_printf(MSG_ERROR, "WPS: Invalid WEP Key "
					   "%d", key_idx);
				return -1;
			}
			ssid->wep_key_len[key_idx] = cred->key_len / 2;
		} else {
			os_memcpy(ssid->wep_key[key_idx], cred->key,
				  cred->key_len);
			ssid->wep_key_len[key_idx] = cred->key_len;
		}
		ssid->wep_tx_keyidx = key_idx;
		break;
	case WPS_ENCR_TKIP:
		ssid->pairwise_cipher = WPA_CIPHER_TKIP;
		break;
	case WPS_ENCR_AES:
		ssid->pairwise_cipher = WPA_CIPHER_CCMP;
		break;
	}

	switch (auth_type) {
	case WPS_AUTH_OPEN:
		ssid->auth_alg = WPA_AUTH_ALG_OPEN;
		ssid->key_mgmt = WPA_KEY_MGMT_NONE;
		ssid->proto = 0;
		break;
	case WPS_AUTH_SHARED:
		ssid->auth_alg = WPA_AUTH_ALG_SHARED;
		ssid->key_mgmt = WPA_KEY_MGMT_NONE;
		ssid->proto = 0;
		break;
	case WPS_AUTH_WPAPSK:
		ssid->auth_alg = WPA_AUTH_ALG_OPEN;
		ssid->key_mgmt = WPA_KEY_MGMT_PSK;
		ssid->proto = WPA_PROTO_WPA;
		break;
	case WPS_AUTH_WPA:
		ssid->auth_alg = WPA_AUTH_ALG_OPEN;
		ssid->key_mgmt = WPA_KEY_MGMT_IEEE8021X;
		ssid->proto = WPA_PROTO_WPA;
		break;
	case WPS_AUTH_WPA2:
		ssid->auth_alg = WPA_AUTH_ALG_OPEN;
		ssid->key_mgmt = WPA_KEY_MGMT_IEEE8021X;
		ssid->proto = WPA_PROTO_RSN;
		break;
	case WPS_AUTH_WPA2PSK:
		ssid->auth_alg = WPA_AUTH_ALG_OPEN;
		ssid->key_mgmt = WPA_KEY_MGMT_PSK;
		ssid->proto = WPA_PROTO_RSN;
		break;
	}

	if (ssid->key_mgmt == WPA_KEY_MGMT_PSK) {
		if (cred->key_len == 2 * PMK_LEN) {
			if (hexstr2bin((const char *) cred->key, ssid->psk,
				       PMK_LEN)) {
				wpa_printf(MSG_ERROR, "WPS: Invalid Network "
					   "Key");
				return -1;
			}
			ssid->psk_set = 1;
		} else if (cred->key_len >= 8 && cred->key_len < 2 * PMK_LEN) {
			os_free(ssid->passphrase);
			ssid->passphrase = os_malloc(cred->key_len + 1);
			if (ssid->passphrase == NULL)
				return -1;
			os_memcpy(ssid->passphrase, cred->key, cred->key_len);
			ssid->passphrase[cred->key_len] = '\0';
			wpa_config_update_psk(ssid);
		} else {
			wpa_printf(MSG_ERROR, "WPS: Invalid Network Key "
				   "length %lu",
				   (unsigned long) cred->key_len);
			return -1;
		}
	}

	wpas_wps_security_workaround(wpa_s, ssid, cred);

#ifndef CONFIG_NO_CONFIG_WRITE
	if (wpa_s->conf->update_config &&
	    wpa_config_write(wpa_s->confname, wpa_s->conf)) {
		wpa_printf(MSG_DEBUG, "WPS: Failed to update configuration");
		return -1;
	}
#endif /* CONFIG_NO_CONFIG_WRITE */

	return 0;
}


static void wpa_supplicant_wps_event_m2d(struct wpa_supplicant *wpa_s,
					 struct wps_event_m2d *m2d)
{
	wpa_msg(wpa_s, MSG_INFO, WPS_EVENT_M2D
		"dev_password_id=%d config_error=%d",
		m2d->dev_password_id, m2d->config_error);
}


static void wpa_supplicant_wps_event_fail(struct wpa_supplicant *wpa_s,
					  struct wps_event_fail *fail)
{
	wpa_msg(wpa_s, MSG_INFO, WPS_EVENT_FAIL "msg=%d", fail->msg);
	wpas_clear_wps(wpa_s);
}


static void wpa_supplicant_wps_event_success(struct wpa_supplicant *wpa_s)
{
	wpa_msg(wpa_s, MSG_INFO, WPS_EVENT_SUCCESS);
	wpa_s->wps_success = 1;
}


static void wpa_supplicant_wps_event(void *ctx, enum wps_event event,
				     union wps_event_data *data)
{
	struct wpa_supplicant *wpa_s = ctx;
	switch (event) {
	case WPS_EV_M2D:
		wpa_supplicant_wps_event_m2d(wpa_s, &data->m2d);
		break;
	case WPS_EV_FAIL:
		wpa_supplicant_wps_event_fail(wpa_s, &data->fail);
		break;
	case WPS_EV_SUCCESS:
		wpa_supplicant_wps_event_success(wpa_s);
		break;
	case WPS_EV_PWD_AUTH_FAIL:
		break;
	case WPS_EV_PBC_OVERLAP:
		break;
	case WPS_EV_PBC_TIMEOUT:
		break;
	}
}


enum wps_request_type wpas_wps_get_req_type(struct wpa_ssid *ssid)
{
	if (eap_is_wps_pbc_enrollee(&ssid->eap) ||
	    eap_is_wps_pin_enrollee(&ssid->eap))
		return WPS_REQ_ENROLLEE;
	else
		return WPS_REQ_REGISTRAR;
}


static void wpas_clear_wps(struct wpa_supplicant *wpa_s)
{
	int id;
	struct wpa_ssid *ssid;

	eloop_cancel_timeout(wpas_wps_timeout, wpa_s, NULL);

	/* Remove any existing WPS network from configuration */
	ssid = wpa_s->conf->ssid;
	while (ssid) {
		if (ssid->key_mgmt & WPA_KEY_MGMT_WPS) {
			if (ssid == wpa_s->current_ssid)
				wpa_s->current_ssid = NULL;
			id = ssid->id;
		} else
			id = -1;
		ssid = ssid->next;
		if (id >= 0)
			wpa_config_remove_network(wpa_s->conf, id);
	}
}


static void wpas_wps_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	wpa_printf(MSG_INFO, WPS_EVENT_TIMEOUT "Requested operation timed "
		   "out");
	wpas_clear_wps(wpa_s);
}


static struct wpa_ssid * wpas_wps_add_network(struct wpa_supplicant *wpa_s,
					      int registrar, const u8 *bssid)
{
	struct wpa_ssid *ssid;

	ssid = wpa_config_add_network(wpa_s->conf);
	if (ssid == NULL)
		return NULL;
	wpa_config_set_network_defaults(ssid);
	if (wpa_config_set(ssid, "key_mgmt", "WPS", 0) < 0 ||
	    wpa_config_set(ssid, "eap", "WSC", 0) < 0 ||
	    wpa_config_set(ssid, "identity", registrar ?
			   "\"" WSC_ID_REGISTRAR "\"" :
			   "\"" WSC_ID_ENROLLEE "\"", 0) < 0) {
		wpa_config_remove_network(wpa_s->conf, ssid->id);
		return NULL;
	}

	if (bssid) {
		size_t i;
		int count = 0;

		os_memcpy(ssid->bssid, bssid, ETH_ALEN);
		ssid->bssid_set = 1;

		/* Try to get SSID from scan results */
		if (wpa_s->scan_res == NULL &&
		    wpa_supplicant_get_scan_results(wpa_s) < 0)
			return ssid; /* Could not find any scan results */

		for (i = 0; i < wpa_s->scan_res->num; i++) {
			const u8 *ie;
			struct wpa_scan_res *res;

			res = wpa_s->scan_res->res[i];
			if (os_memcmp(bssid, res->bssid, ETH_ALEN) != 0)
				continue;

			ie = wpa_scan_get_ie(res, WLAN_EID_SSID);
			if (ie == NULL)
				break;
			os_free(ssid->ssid);
			ssid->ssid = os_malloc(ie[1]);
			if (ssid->ssid == NULL)
				break;
			os_memcpy(ssid->ssid, ie + 2, ie[1]);
			ssid->ssid_len = ie[1];
			wpa_hexdump_ascii(MSG_DEBUG, "WPS: Picked SSID from "
					  "scan results",
					  ssid->ssid, ssid->ssid_len);
			count++;
		}

		if (count > 1) {
			wpa_printf(MSG_DEBUG, "WPS: More than one SSID found "
				   "for the AP; use wildcard");
			os_free(ssid->ssid);
			ssid->ssid = NULL;
			ssid->ssid_len = 0;
		}
	}

	return ssid;
}


static void wpas_wps_reassoc(struct wpa_supplicant *wpa_s,
			     struct wpa_ssid *selected)
{
	struct wpa_ssid *ssid;

	/* Mark all other networks disabled and trigger reassociation */
	ssid = wpa_s->conf->ssid;
	while (ssid) {
		ssid->disabled = ssid != selected;
		ssid = ssid->next;
	}
	wpa_s->disconnected = 0;
	wpa_s->reassociate = 1;
	wpa_s->scan_runs = 0;
	wpa_s->wps_success = 0;
	wpa_s->blacklist_cleared = 0;
	wpa_supplicant_req_scan(wpa_s, 0, 0);
}


int wpas_wps_start_pbc(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	struct wpa_ssid *ssid;
	wpas_clear_wps(wpa_s);
	ssid = wpas_wps_add_network(wpa_s, 0, bssid);
	if (ssid == NULL)
		return -1;
	wpa_config_set(ssid, "phase1", "\"pbc=1\"", 0);
	eloop_register_timeout(WPS_PBC_WALK_TIME, 0, wpas_wps_timeout,
			       wpa_s, NULL);
	wpas_wps_reassoc(wpa_s, ssid);
	return 0;
}


int wpas_wps_start_pin(struct wpa_supplicant *wpa_s, const u8 *bssid,
		       const char *pin)
{
	struct wpa_ssid *ssid;
	char val[30];
	unsigned int rpin = 0;

	wpas_clear_wps(wpa_s);
	ssid = wpas_wps_add_network(wpa_s, 0, bssid);
	if (ssid == NULL)
		return -1;
	if (pin)
		os_snprintf(val, sizeof(val), "\"pin=%s\"", pin);
	else {
		rpin = wps_generate_pin();
		os_snprintf(val, sizeof(val), "\"pin=%08d\"", rpin);
	}
	wpa_config_set(ssid, "phase1", val, 0);
	eloop_register_timeout(WPS_PBC_WALK_TIME, 0, wpas_wps_timeout,
			       wpa_s, NULL);
	wpas_wps_reassoc(wpa_s, ssid);
	return rpin;
}


int wpas_wps_start_reg(struct wpa_supplicant *wpa_s, const u8 *bssid,
		       const char *pin)
{
	struct wpa_ssid *ssid;
	char val[30];

	if (!pin)
		return -1;
	wpas_clear_wps(wpa_s);
	ssid = wpas_wps_add_network(wpa_s, 1, bssid);
	if (ssid == NULL)
		return -1;
	os_snprintf(val, sizeof(val), "\"pin=%s\"", pin);
	wpa_config_set(ssid, "phase1", val, 0);
	eloop_register_timeout(WPS_PBC_WALK_TIME, 0, wpas_wps_timeout,
			       wpa_s, NULL);
	wpas_wps_reassoc(wpa_s, ssid);
	return 0;
}


static int wpas_wps_new_psk_cb(void *ctx, const u8 *mac_addr, const u8 *psk,
			       size_t psk_len)
{
	wpa_printf(MSG_DEBUG, "WPS: Received new WPA/WPA2-PSK from WPS for "
		   "STA " MACSTR, MAC2STR(mac_addr));
	wpa_hexdump_key(MSG_DEBUG, "Per-device PSK", psk, psk_len);

	/* TODO */

	return 0;
}


static void wpas_wps_pin_needed_cb(void *ctx, const u8 *uuid_e,
				   const struct wps_device_data *dev)
{
	char uuid[40], txt[400];
	int len;
	if (uuid_bin2str(uuid_e, uuid, sizeof(uuid)))
		return;
	wpa_printf(MSG_DEBUG, "WPS: PIN needed for UUID-E %s", uuid);
	len = os_snprintf(txt, sizeof(txt), "WPS-EVENT-PIN-NEEDED %s " MACSTR
			  " [%s|%s|%s|%s|%s|%d-%08X-%d]",
			  uuid, MAC2STR(dev->mac_addr), dev->device_name,
			  dev->manufacturer, dev->model_name,
			  dev->model_number, dev->serial_number,
			  dev->categ, dev->oui, dev->sub_categ);
	if (len > 0 && len < (int) sizeof(txt))
		wpa_printf(MSG_INFO, "%s", txt);
}


int wpas_wps_init(struct wpa_supplicant *wpa_s)
{
	struct wps_context *wps;
	struct wps_registrar_config rcfg;

	wps = os_zalloc(sizeof(*wps));
	if (wps == NULL)
		return -1;

	wps->cred_cb = wpa_supplicant_wps_cred;
	wps->event_cb = wpa_supplicant_wps_event;
	wps->cb_ctx = wpa_s;

	wps->dev.device_name = wpa_s->conf->device_name;
	wps->dev.manufacturer = wpa_s->conf->manufacturer;
	wps->dev.model_name = wpa_s->conf->model_name;
	wps->dev.model_number = wpa_s->conf->model_number;
	wps->dev.serial_number = wpa_s->conf->serial_number;
	if (wpa_s->conf->device_type) {
		char *pos;
		u8 oui[4];
		/* <categ>-<OUI>-<subcateg> */
		wps->dev.categ = atoi(wpa_s->conf->device_type);
		pos = os_strchr(wpa_s->conf->device_type, '-');
		if (pos == NULL) {
			wpa_printf(MSG_ERROR, "WPS: Invalid device_type");
			os_free(wps);
			return -1;
		}
		pos++;
		if (hexstr2bin(pos, oui, 4)) {
			wpa_printf(MSG_ERROR, "WPS: Invalid device_type OUI");
			os_free(wps);
			return -1;
		}
		wps->dev.oui = WPA_GET_BE32(oui);
		pos = os_strchr(pos, '-');
		if (pos == NULL) {
			wpa_printf(MSG_ERROR, "WPS: Invalid device_type");
			os_free(wps);
			return -1;
		}
		pos++;
		wps->dev.sub_categ = atoi(pos);
	}
	wps->dev.os_version = WPA_GET_BE32(wpa_s->conf->os_version);
	wps->dev.rf_bands = WPS_RF_24GHZ | WPS_RF_50GHZ; /* TODO: config */
	os_memcpy(wps->dev.mac_addr, wpa_s->own_addr, ETH_ALEN);
	if (is_nil_uuid(wpa_s->conf->uuid)) {
		uuid_gen_mac_addr(wpa_s->own_addr, wps->uuid);
		wpa_hexdump(MSG_DEBUG, "WPS: UUID based on MAC address",
			    wps->uuid, WPS_UUID_LEN);
	} else
		os_memcpy(wps->uuid, wpa_s->conf->uuid, WPS_UUID_LEN);

	wps->auth_types = WPS_AUTH_WPA2PSK | WPS_AUTH_WPAPSK;
	wps->encr_types = WPS_ENCR_AES | WPS_ENCR_TKIP;

	os_memset(&rcfg, 0, sizeof(rcfg));
	rcfg.new_psk_cb = wpas_wps_new_psk_cb;
	rcfg.pin_needed_cb = wpas_wps_pin_needed_cb;
	rcfg.cb_ctx = wpa_s;

	wps->registrar = wps_registrar_init(wps, &rcfg);
	if (wps->registrar == NULL) {
		wpa_printf(MSG_DEBUG, "Failed to initialize WPS Registrar");
		os_free(wps);
		return -1;
	}

	wpa_s->wps = wps;

	return 0;
}


void wpas_wps_deinit(struct wpa_supplicant *wpa_s)
{
	eloop_cancel_timeout(wpas_wps_timeout, wpa_s, NULL);

	if (wpa_s->wps == NULL)
		return;

	wps_registrar_deinit(wpa_s->wps->registrar);
	os_free(wpa_s->wps->network_key);
	os_free(wpa_s->wps);
	wpa_s->wps = NULL;
}


int wpas_wps_ssid_bss_match(struct wpa_supplicant *wpa_s,
			    struct wpa_ssid *ssid, struct wpa_scan_res *bss)
{
	struct wpabuf *wps_ie;

	if (!(ssid->key_mgmt & WPA_KEY_MGMT_WPS))
		return -1;

	wps_ie = wpa_scan_get_vendor_ie_multi(bss, WPS_IE_VENDOR_TYPE);
	if (eap_is_wps_pbc_enrollee(&ssid->eap)) {
		if (!wps_ie) {
			wpa_printf(MSG_DEBUG, "   skip - non-WPS AP");
			return 0;
		}

		if (!wps_is_selected_pbc_registrar(wps_ie)) {
			wpa_printf(MSG_DEBUG, "   skip - WPS AP "
				   "without active PBC Registrar");
			wpabuf_free(wps_ie);
			return 0;
		}

		/* TODO: overlap detection */
		wpa_printf(MSG_DEBUG, "   selected based on WPS IE "
			   "(Active PBC)");
		wpabuf_free(wps_ie);
		return 1;
	}

	if (eap_is_wps_pin_enrollee(&ssid->eap)) {
		if (!wps_ie) {
			wpa_printf(MSG_DEBUG, "   skip - non-WPS AP");
			return 0;
		}

		/*
		 * Start with WPS APs that advertise active PIN Registrar and
		 * allow any WPS AP after third scan since some APs do not set
		 * Selected Registrar attribute properly when using external
		 * Registrar.
		 */
		if (!wps_is_selected_pin_registrar(wps_ie)) {
			if (wpa_s->scan_runs < WPS_PIN_SCAN_IGNORE_SEL_REG) {
				wpa_printf(MSG_DEBUG, "   skip - WPS AP "
					   "without active PIN Registrar");
				wpabuf_free(wps_ie);
				return 0;
			}
			wpa_printf(MSG_DEBUG, "   selected based on WPS IE");
		} else {
			wpa_printf(MSG_DEBUG, "   selected based on WPS IE "
				   "(Active PIN)");
		}
		wpabuf_free(wps_ie);
		return 1;
	}

	if (wps_ie) {
		wpa_printf(MSG_DEBUG, "   selected based on WPS IE");
		wpabuf_free(wps_ie);
		return 1;
	}

	return -1;
}


int wpas_wps_ssid_wildcard_ok(struct wpa_supplicant *wpa_s,
			      struct wpa_ssid *ssid,
			      struct wpa_scan_res *bss)
{
	struct wpabuf *wps_ie = NULL;
	int ret = 0;

	if (eap_is_wps_pbc_enrollee(&ssid->eap)) {
		wps_ie = wpa_scan_get_vendor_ie_multi(bss, WPS_IE_VENDOR_TYPE);
		if (wps_ie && wps_is_selected_pbc_registrar(wps_ie)) {
			/* allow wildcard SSID for WPS PBC */
			ret = 1;
		}
	} else if (eap_is_wps_pin_enrollee(&ssid->eap)) {
		wps_ie = wpa_scan_get_vendor_ie_multi(bss, WPS_IE_VENDOR_TYPE);
		if (wps_ie &&
		    (wps_is_selected_pin_registrar(wps_ie) ||
		     wpa_s->scan_runs >= WPS_PIN_SCAN_IGNORE_SEL_REG)) {
			/* allow wildcard SSID for WPS PIN */
			ret = 1;
		}
	}

	if (!ret && ssid->bssid_set &&
	    os_memcmp(ssid->bssid, bss->bssid, ETH_ALEN) == 0) {
		/* allow wildcard SSID due to hardcoded BSSID match */
		ret = 1;
	}

	wpabuf_free(wps_ie);

	return ret;
}


int wpas_wps_scan_pbc_overlap(struct wpa_supplicant *wpa_s,
			      struct wpa_scan_res *selected,
			      struct wpa_ssid *ssid)
{
	const u8 *sel_uuid, *uuid;
	size_t i;
	struct wpabuf *wps_ie;
	int ret = 0;

	if (!eap_is_wps_pbc_enrollee(&ssid->eap))
		return 0;

	/* Make sure that only one AP is in active PBC mode */
	wps_ie = wpa_scan_get_vendor_ie_multi(selected, WPS_IE_VENDOR_TYPE);
	if (wps_ie)
		sel_uuid = wps_get_uuid_e(wps_ie);
	else
		sel_uuid = NULL;

	for (i = 0; i < wpa_s->scan_res->num; i++) {
		struct wpa_scan_res *bss = wpa_s->scan_res->res[i];
		struct wpabuf *ie;
		if (bss == selected)
			continue;
		ie = wpa_scan_get_vendor_ie_multi(bss, WPS_IE_VENDOR_TYPE);
		if (!ie)
			continue;
		if (!wps_is_selected_pbc_registrar(ie)) {
			wpabuf_free(ie);
			continue;
		}
		uuid = wps_get_uuid_e(ie);
		if (sel_uuid == NULL || uuid == NULL ||
		    os_memcmp(sel_uuid, uuid, 16) != 0) {
			ret = 1; /* PBC overlap */
			wpabuf_free(ie);
			break;
		}

		/* TODO: verify that this is reasonable dual-band situation */

		wpabuf_free(ie);
	}

	wpabuf_free(wps_ie);

	return ret;
}


void wpas_wps_notify_scan_results(struct wpa_supplicant *wpa_s)
{
	size_t i;

	if (wpa_s->disconnected || wpa_s->wpa_state >= WPA_ASSOCIATED)
		return;

	for (i = 0; i < wpa_s->scan_res->num; i++) {
		struct wpa_scan_res *bss = wpa_s->scan_res->res[i];
		struct wpabuf *ie;
		ie = wpa_scan_get_vendor_ie_multi(bss, WPS_IE_VENDOR_TYPE);
		if (!ie)
			continue;
		if (wps_is_selected_pbc_registrar(ie))
			wpa_msg(wpa_s, MSG_INFO, WPS_EVENT_AP_AVAILABLE_PBC);
		else if (wps_is_selected_pin_registrar(ie))
			wpa_msg(wpa_s, MSG_INFO, WPS_EVENT_AP_AVAILABLE_PIN);
		else
			wpa_msg(wpa_s, MSG_INFO, WPS_EVENT_AP_AVAILABLE);
		wpabuf_free(ie);
		break;
	}
}


int wpas_wps_searching(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid;

	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if ((ssid->key_mgmt & WPA_KEY_MGMT_WPS) && !ssid->disabled)
			return 1;
	}

	return 0;
}
