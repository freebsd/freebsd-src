/*
 * WPA Supplicant
 * Copyright (c) 2003-2012, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * This file implements functions for registering and unregistering
 * %wpa_supplicant interfaces. In addition, this file contains number of
 * functions for managing network connections.
 */

#include "includes.h"

#include "common.h"
#include "crypto/random.h"
#include "crypto/sha1.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "eap_peer/eap.h"
#include "eap_server/eap_methods.h"
#include "rsn_supp/wpa.h"
#include "eloop.h"
#include "config.h"
#include "utils/ext_password.h"
#include "l2_packet/l2_packet.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "ctrl_iface.h"
#include "pcsc_funcs.h"
#include "common/version.h"
#include "rsn_supp/preauth.h"
#include "rsn_supp/pmksa_cache.h"
#include "common/wpa_ctrl.h"
#include "common/ieee802_11_defs.h"
#include "p2p/p2p.h"
#include "blacklist.h"
#include "wpas_glue.h"
#include "wps_supplicant.h"
#include "ibss_rsn.h"
#include "sme.h"
#include "gas_query.h"
#include "ap.h"
#include "p2p_supplicant.h"
#include "wifi_display.h"
#include "notify.h"
#include "bgscan.h"
#include "autoscan.h"
#include "bss.h"
#include "scan.h"
#include "offchannel.h"
#include "hs20_supplicant.h"

const char *wpa_supplicant_version =
"wpa_supplicant v" VERSION_STR "\n"
"Copyright (c) 2003-2012, Jouni Malinen <j@w1.fi> and contributors";

const char *wpa_supplicant_license =
"This software may be distributed under the terms of the BSD license.\n"
"See README for more details.\n"
#ifdef EAP_TLS_OPENSSL
"\nThis product includes software developed by the OpenSSL Project\n"
"for use in the OpenSSL Toolkit (http://www.openssl.org/)\n"
#endif /* EAP_TLS_OPENSSL */
;

#ifndef CONFIG_NO_STDOUT_DEBUG
/* Long text divided into parts in order to fit in C89 strings size limits. */
const char *wpa_supplicant_full_license1 =
"";
const char *wpa_supplicant_full_license2 =
"This software may be distributed under the terms of the BSD license.\n"
"\n"
"Redistribution and use in source and binary forms, with or without\n"
"modification, are permitted provided that the following conditions are\n"
"met:\n"
"\n";
const char *wpa_supplicant_full_license3 =
"1. Redistributions of source code must retain the above copyright\n"
"   notice, this list of conditions and the following disclaimer.\n"
"\n"
"2. Redistributions in binary form must reproduce the above copyright\n"
"   notice, this list of conditions and the following disclaimer in the\n"
"   documentation and/or other materials provided with the distribution.\n"
"\n";
const char *wpa_supplicant_full_license4 =
"3. Neither the name(s) of the above-listed copyright holder(s) nor the\n"
"   names of its contributors may be used to endorse or promote products\n"
"   derived from this software without specific prior written permission.\n"
"\n"
"THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n"
"\"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n"
"LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n"
"A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\n";
const char *wpa_supplicant_full_license5 =
"OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n"
"SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n"
"LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n"
"DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n"
"THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n"
"(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n"
"OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"
"\n";
#endif /* CONFIG_NO_STDOUT_DEBUG */

extern int wpa_debug_level;
extern int wpa_debug_show_keys;
extern int wpa_debug_timestamp;
extern struct wpa_driver_ops *wpa_drivers[];

/* Configure default/group WEP keys for static WEP */
int wpa_set_wep_keys(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid)
{
	int i, set = 0;

	for (i = 0; i < NUM_WEP_KEYS; i++) {
		if (ssid->wep_key_len[i] == 0)
			continue;

		set = 1;
		wpa_drv_set_key(wpa_s, WPA_ALG_WEP, NULL,
				i, i == ssid->wep_tx_keyidx, NULL, 0,
				ssid->wep_key[i], ssid->wep_key_len[i]);
	}

	return set;
}


static int wpa_supplicant_set_wpa_none_key(struct wpa_supplicant *wpa_s,
					   struct wpa_ssid *ssid)
{
	u8 key[32];
	size_t keylen;
	enum wpa_alg alg;
	u8 seq[6] = { 0 };

	/* IBSS/WPA-None uses only one key (Group) for both receiving and
	 * sending unicast and multicast packets. */

	if (ssid->mode != WPAS_MODE_IBSS) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Invalid mode %d (not "
			"IBSS/ad-hoc) for WPA-None", ssid->mode);
		return -1;
	}

	if (!ssid->psk_set) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: No PSK configured for "
			"WPA-None");
		return -1;
	}

	switch (wpa_s->group_cipher) {
	case WPA_CIPHER_CCMP:
		os_memcpy(key, ssid->psk, 16);
		keylen = 16;
		alg = WPA_ALG_CCMP;
		break;
	case WPA_CIPHER_GCMP:
		os_memcpy(key, ssid->psk, 16);
		keylen = 16;
		alg = WPA_ALG_GCMP;
		break;
	case WPA_CIPHER_TKIP:
		/* WPA-None uses the same Michael MIC key for both TX and RX */
		os_memcpy(key, ssid->psk, 16 + 8);
		os_memcpy(key + 16 + 8, ssid->psk + 16, 8);
		keylen = 32;
		alg = WPA_ALG_TKIP;
		break;
	default:
		wpa_msg(wpa_s, MSG_INFO, "WPA: Invalid group cipher %d for "
			"WPA-None", wpa_s->group_cipher);
		return -1;
	}

	/* TODO: should actually remember the previously used seq#, both for TX
	 * and RX from each STA.. */

	return wpa_drv_set_key(wpa_s, alg, NULL, 0, 1, seq, 6, key, keylen);
}


static void wpa_supplicant_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	const u8 *bssid = wpa_s->bssid;
	if (is_zero_ether_addr(bssid))
		bssid = wpa_s->pending_bssid;
	wpa_msg(wpa_s, MSG_INFO, "Authentication with " MACSTR " timed out.",
		MAC2STR(bssid));
	wpa_blacklist_add(wpa_s, bssid);
	wpa_sm_notify_disassoc(wpa_s->wpa);
	wpa_supplicant_deauthenticate(wpa_s, WLAN_REASON_DEAUTH_LEAVING);
	wpa_s->reassociate = 1;

	/*
	 * If we timed out, the AP or the local radio may be busy.
	 * So, wait a second until scanning again.
	 */
	wpa_supplicant_req_scan(wpa_s, 1, 0);

#ifdef CONFIG_P2P
	if (wpa_s->global->p2p_cb_on_scan_complete && !wpa_s->global->p2p_disabled &&
	    wpa_s->global->p2p != NULL) {
		wpa_s->global->p2p_cb_on_scan_complete = 0;
		if (p2p_other_scan_completed(wpa_s->global->p2p) == 1) {
			wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Pending P2P operation "
				"continued after timed out authentication");
		}
	}
#endif /* CONFIG_P2P */
}


/**
 * wpa_supplicant_req_auth_timeout - Schedule a timeout for authentication
 * @wpa_s: Pointer to wpa_supplicant data
 * @sec: Number of seconds after which to time out authentication
 * @usec: Number of microseconds after which to time out authentication
 *
 * This function is used to schedule a timeout for the current authentication
 * attempt.
 */
void wpa_supplicant_req_auth_timeout(struct wpa_supplicant *wpa_s,
				     int sec, int usec)
{
	if (wpa_s->conf && wpa_s->conf->ap_scan == 0 &&
	    (wpa_s->drv_flags & WPA_DRIVER_FLAGS_WIRED))
		return;

	wpa_dbg(wpa_s, MSG_DEBUG, "Setting authentication timeout: %d sec "
		"%d usec", sec, usec);
	eloop_cancel_timeout(wpa_supplicant_timeout, wpa_s, NULL);
	eloop_register_timeout(sec, usec, wpa_supplicant_timeout, wpa_s, NULL);
}


/**
 * wpa_supplicant_cancel_auth_timeout - Cancel authentication timeout
 * @wpa_s: Pointer to wpa_supplicant data
 *
 * This function is used to cancel authentication timeout scheduled with
 * wpa_supplicant_req_auth_timeout() and it is called when authentication has
 * been completed.
 */
void wpa_supplicant_cancel_auth_timeout(struct wpa_supplicant *wpa_s)
{
	wpa_dbg(wpa_s, MSG_DEBUG, "Cancelling authentication timeout");
	eloop_cancel_timeout(wpa_supplicant_timeout, wpa_s, NULL);
	wpa_blacklist_del(wpa_s, wpa_s->bssid);
}


/**
 * wpa_supplicant_initiate_eapol - Configure EAPOL state machine
 * @wpa_s: Pointer to wpa_supplicant data
 *
 * This function is used to configure EAPOL state machine based on the selected
 * authentication mode.
 */
void wpa_supplicant_initiate_eapol(struct wpa_supplicant *wpa_s)
{
#ifdef IEEE8021X_EAPOL
	struct eapol_config eapol_conf;
	struct wpa_ssid *ssid = wpa_s->current_ssid;

#ifdef CONFIG_IBSS_RSN
	if (ssid->mode == WPAS_MODE_IBSS &&
	    wpa_s->key_mgmt != WPA_KEY_MGMT_NONE &&
	    wpa_s->key_mgmt != WPA_KEY_MGMT_WPA_NONE) {
		/*
		 * RSN IBSS authentication is per-STA and we can disable the
		 * per-BSSID EAPOL authentication.
		 */
		eapol_sm_notify_portControl(wpa_s->eapol, ForceAuthorized);
		eapol_sm_notify_eap_success(wpa_s->eapol, TRUE);
		eapol_sm_notify_eap_fail(wpa_s->eapol, FALSE);
		return;
	}
#endif /* CONFIG_IBSS_RSN */

	eapol_sm_notify_eap_success(wpa_s->eapol, FALSE);
	eapol_sm_notify_eap_fail(wpa_s->eapol, FALSE);

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_NONE ||
	    wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE)
		eapol_sm_notify_portControl(wpa_s->eapol, ForceAuthorized);
	else
		eapol_sm_notify_portControl(wpa_s->eapol, Auto);

	os_memset(&eapol_conf, 0, sizeof(eapol_conf));
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
		eapol_conf.accept_802_1x_keys = 1;
		eapol_conf.required_keys = 0;
		if (ssid->eapol_flags & EAPOL_FLAG_REQUIRE_KEY_UNICAST) {
			eapol_conf.required_keys |= EAPOL_REQUIRE_KEY_UNICAST;
		}
		if (ssid->eapol_flags & EAPOL_FLAG_REQUIRE_KEY_BROADCAST) {
			eapol_conf.required_keys |=
				EAPOL_REQUIRE_KEY_BROADCAST;
		}

		if (wpa_s->conf && (wpa_s->drv_flags & WPA_DRIVER_FLAGS_WIRED))
			eapol_conf.required_keys = 0;
	}
	if (wpa_s->conf)
		eapol_conf.fast_reauth = wpa_s->conf->fast_reauth;
	eapol_conf.workaround = ssid->eap_workaround;
	eapol_conf.eap_disabled =
		!wpa_key_mgmt_wpa_ieee8021x(wpa_s->key_mgmt) &&
		wpa_s->key_mgmt != WPA_KEY_MGMT_IEEE8021X_NO_WPA &&
		wpa_s->key_mgmt != WPA_KEY_MGMT_WPS;
	eapol_sm_notify_config(wpa_s->eapol, &ssid->eap, &eapol_conf);
#endif /* IEEE8021X_EAPOL */
}


/**
 * wpa_supplicant_set_non_wpa_policy - Set WPA parameters to non-WPA mode
 * @wpa_s: Pointer to wpa_supplicant data
 * @ssid: Configuration data for the network
 *
 * This function is used to configure WPA state machine and related parameters
 * to a mode where WPA is not enabled. This is called as part of the
 * authentication configuration when the selected network does not use WPA.
 */
void wpa_supplicant_set_non_wpa_policy(struct wpa_supplicant *wpa_s,
				       struct wpa_ssid *ssid)
{
	int i;

	if (ssid->key_mgmt & WPA_KEY_MGMT_WPS)
		wpa_s->key_mgmt = WPA_KEY_MGMT_WPS;
	else if (ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA)
		wpa_s->key_mgmt = WPA_KEY_MGMT_IEEE8021X_NO_WPA;
	else
		wpa_s->key_mgmt = WPA_KEY_MGMT_NONE;
	wpa_sm_set_ap_wpa_ie(wpa_s->wpa, NULL, 0);
	wpa_sm_set_ap_rsn_ie(wpa_s->wpa, NULL, 0);
	wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, NULL, 0);
	wpa_s->pairwise_cipher = WPA_CIPHER_NONE;
	wpa_s->group_cipher = WPA_CIPHER_NONE;
	wpa_s->mgmt_group_cipher = 0;

	for (i = 0; i < NUM_WEP_KEYS; i++) {
		if (ssid->wep_key_len[i] > 5) {
			wpa_s->pairwise_cipher = WPA_CIPHER_WEP104;
			wpa_s->group_cipher = WPA_CIPHER_WEP104;
			break;
		} else if (ssid->wep_key_len[i] > 0) {
			wpa_s->pairwise_cipher = WPA_CIPHER_WEP40;
			wpa_s->group_cipher = WPA_CIPHER_WEP40;
			break;
		}
	}

	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_RSN_ENABLED, 0);
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_KEY_MGMT, wpa_s->key_mgmt);
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_PAIRWISE,
			 wpa_s->pairwise_cipher);
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_GROUP, wpa_s->group_cipher);
#ifdef CONFIG_IEEE80211W
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_MGMT_GROUP,
			 wpa_s->mgmt_group_cipher);
#endif /* CONFIG_IEEE80211W */

	pmksa_cache_clear_current(wpa_s->wpa);
}


void free_hw_features(struct wpa_supplicant *wpa_s)
{
	int i;
	if (wpa_s->hw.modes == NULL)
		return;

	for (i = 0; i < wpa_s->hw.num_modes; i++) {
		os_free(wpa_s->hw.modes[i].channels);
		os_free(wpa_s->hw.modes[i].rates);
	}

	os_free(wpa_s->hw.modes);
	wpa_s->hw.modes = NULL;
}


static void wpa_supplicant_cleanup(struct wpa_supplicant *wpa_s)
{
	bgscan_deinit(wpa_s);
	autoscan_deinit(wpa_s);
	scard_deinit(wpa_s->scard);
	wpa_s->scard = NULL;
	wpa_sm_set_scard_ctx(wpa_s->wpa, NULL);
	eapol_sm_register_scard_ctx(wpa_s->eapol, NULL);
	l2_packet_deinit(wpa_s->l2);
	wpa_s->l2 = NULL;
	if (wpa_s->l2_br) {
		l2_packet_deinit(wpa_s->l2_br);
		wpa_s->l2_br = NULL;
	}

	if (wpa_s->conf != NULL) {
		struct wpa_ssid *ssid;
		for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next)
			wpas_notify_network_removed(wpa_s, ssid);
	}

	os_free(wpa_s->confname);
	wpa_s->confname = NULL;

	wpa_sm_set_eapol(wpa_s->wpa, NULL);
	eapol_sm_deinit(wpa_s->eapol);
	wpa_s->eapol = NULL;

	rsn_preauth_deinit(wpa_s->wpa);

#ifdef CONFIG_TDLS
	wpa_tdls_deinit(wpa_s->wpa);
#endif /* CONFIG_TDLS */

	pmksa_candidate_free(wpa_s->wpa);
	wpa_sm_deinit(wpa_s->wpa);
	wpa_s->wpa = NULL;
	wpa_blacklist_clear(wpa_s);

	wpa_bss_deinit(wpa_s);

	wpa_supplicant_cancel_scan(wpa_s);
	wpa_supplicant_cancel_auth_timeout(wpa_s);
	eloop_cancel_timeout(wpa_supplicant_stop_countermeasures, wpa_s, NULL);
#ifdef CONFIG_DELAYED_MIC_ERROR_REPORT
	eloop_cancel_timeout(wpa_supplicant_delayed_mic_error_report,
			     wpa_s, NULL);
#endif /* CONFIG_DELAYED_MIC_ERROR_REPORT */

	wpas_wps_deinit(wpa_s);

	wpabuf_free(wpa_s->pending_eapol_rx);
	wpa_s->pending_eapol_rx = NULL;

#ifdef CONFIG_IBSS_RSN
	ibss_rsn_deinit(wpa_s->ibss_rsn);
	wpa_s->ibss_rsn = NULL;
#endif /* CONFIG_IBSS_RSN */

	sme_deinit(wpa_s);

#ifdef CONFIG_AP
	wpa_supplicant_ap_deinit(wpa_s);
#endif /* CONFIG_AP */

#ifdef CONFIG_P2P
	wpas_p2p_deinit(wpa_s);
#endif /* CONFIG_P2P */

#ifdef CONFIG_OFFCHANNEL
	offchannel_deinit(wpa_s);
#endif /* CONFIG_OFFCHANNEL */

	wpa_supplicant_cancel_sched_scan(wpa_s);

	os_free(wpa_s->next_scan_freqs);
	wpa_s->next_scan_freqs = NULL;

	gas_query_deinit(wpa_s->gas);
	wpa_s->gas = NULL;

	free_hw_features(wpa_s);

	os_free(wpa_s->bssid_filter);
	wpa_s->bssid_filter = NULL;

	os_free(wpa_s->disallow_aps_bssid);
	wpa_s->disallow_aps_bssid = NULL;
	os_free(wpa_s->disallow_aps_ssid);
	wpa_s->disallow_aps_ssid = NULL;

	wnm_bss_keep_alive_deinit(wpa_s);

	ext_password_deinit(wpa_s->ext_pw);
	wpa_s->ext_pw = NULL;

	wpabuf_free(wpa_s->last_gas_resp);

	os_free(wpa_s->last_scan_res);
	wpa_s->last_scan_res = NULL;
}


/**
 * wpa_clear_keys - Clear keys configured for the driver
 * @wpa_s: Pointer to wpa_supplicant data
 * @addr: Previously used BSSID or %NULL if not available
 *
 * This function clears the encryption keys that has been previously configured
 * for the driver.
 */
void wpa_clear_keys(struct wpa_supplicant *wpa_s, const u8 *addr)
{
	if (wpa_s->keys_cleared) {
		/* Some drivers (e.g., ndiswrapper & NDIS drivers) seem to have
		 * timing issues with keys being cleared just before new keys
		 * are set or just after association or something similar. This
		 * shows up in group key handshake failing often because of the
		 * client not receiving the first encrypted packets correctly.
		 * Skipping some of the extra key clearing steps seems to help
		 * in completing group key handshake more reliably. */
		wpa_dbg(wpa_s, MSG_DEBUG, "No keys have been configured - "
			"skip key clearing");
		return;
	}

	/* MLME-DELETEKEYS.request */
	wpa_drv_set_key(wpa_s, WPA_ALG_NONE, NULL, 0, 0, NULL, 0, NULL, 0);
	wpa_drv_set_key(wpa_s, WPA_ALG_NONE, NULL, 1, 0, NULL, 0, NULL, 0);
	wpa_drv_set_key(wpa_s, WPA_ALG_NONE, NULL, 2, 0, NULL, 0, NULL, 0);
	wpa_drv_set_key(wpa_s, WPA_ALG_NONE, NULL, 3, 0, NULL, 0, NULL, 0);
#ifdef CONFIG_IEEE80211W
	wpa_drv_set_key(wpa_s, WPA_ALG_NONE, NULL, 4, 0, NULL, 0, NULL, 0);
	wpa_drv_set_key(wpa_s, WPA_ALG_NONE, NULL, 5, 0, NULL, 0, NULL, 0);
#endif /* CONFIG_IEEE80211W */
	if (addr) {
		wpa_drv_set_key(wpa_s, WPA_ALG_NONE, addr, 0, 0, NULL, 0, NULL,
				0);
		/* MLME-SETPROTECTION.request(None) */
		wpa_drv_mlme_setprotection(
			wpa_s, addr,
			MLME_SETPROTECTION_PROTECT_TYPE_NONE,
			MLME_SETPROTECTION_KEY_TYPE_PAIRWISE);
	}
	wpa_s->keys_cleared = 1;
}


/**
 * wpa_supplicant_state_txt - Get the connection state name as a text string
 * @state: State (wpa_state; WPA_*)
 * Returns: The state name as a printable text string
 */
const char * wpa_supplicant_state_txt(enum wpa_states state)
{
	switch (state) {
	case WPA_DISCONNECTED:
		return "DISCONNECTED";
	case WPA_INACTIVE:
		return "INACTIVE";
	case WPA_INTERFACE_DISABLED:
		return "INTERFACE_DISABLED";
	case WPA_SCANNING:
		return "SCANNING";
	case WPA_AUTHENTICATING:
		return "AUTHENTICATING";
	case WPA_ASSOCIATING:
		return "ASSOCIATING";
	case WPA_ASSOCIATED:
		return "ASSOCIATED";
	case WPA_4WAY_HANDSHAKE:
		return "4WAY_HANDSHAKE";
	case WPA_GROUP_HANDSHAKE:
		return "GROUP_HANDSHAKE";
	case WPA_COMPLETED:
		return "COMPLETED";
	default:
		return "UNKNOWN";
	}
}


#ifdef CONFIG_BGSCAN

static void wpa_supplicant_start_bgscan(struct wpa_supplicant *wpa_s)
{
	if (wpas_driver_bss_selection(wpa_s))
		return;
	if (wpa_s->current_ssid == wpa_s->bgscan_ssid)
		return;

	bgscan_deinit(wpa_s);
	if (wpa_s->current_ssid && wpa_s->current_ssid->bgscan) {
		if (bgscan_init(wpa_s, wpa_s->current_ssid)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "Failed to initialize "
				"bgscan");
			/*
			 * Live without bgscan; it is only used as a roaming
			 * optimization, so the initial connection is not
			 * affected.
			 */
		} else {
			struct wpa_scan_results *scan_res;
			wpa_s->bgscan_ssid = wpa_s->current_ssid;
			scan_res = wpa_supplicant_get_scan_results(wpa_s, NULL,
								   0);
			if (scan_res) {
				bgscan_notify_scan(wpa_s, scan_res);
				wpa_scan_results_free(scan_res);
			}
		}
	} else
		wpa_s->bgscan_ssid = NULL;
}


static void wpa_supplicant_stop_bgscan(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->bgscan_ssid != NULL) {
		bgscan_deinit(wpa_s);
		wpa_s->bgscan_ssid = NULL;
	}
}

#endif /* CONFIG_BGSCAN */


static void wpa_supplicant_start_autoscan(struct wpa_supplicant *wpa_s)
{
	if (autoscan_init(wpa_s, 0))
		wpa_dbg(wpa_s, MSG_DEBUG, "Failed to initialize autoscan");
}


static void wpa_supplicant_stop_autoscan(struct wpa_supplicant *wpa_s)
{
	autoscan_deinit(wpa_s);
}


void wpa_supplicant_reinit_autoscan(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->wpa_state == WPA_DISCONNECTED ||
	    wpa_s->wpa_state == WPA_SCANNING) {
		autoscan_deinit(wpa_s);
		wpa_supplicant_start_autoscan(wpa_s);
	}
}


/**
 * wpa_supplicant_set_state - Set current connection state
 * @wpa_s: Pointer to wpa_supplicant data
 * @state: The new connection state
 *
 * This function is called whenever the connection state changes, e.g.,
 * association is completed for WPA/WPA2 4-Way Handshake is started.
 */
void wpa_supplicant_set_state(struct wpa_supplicant *wpa_s,
			      enum wpa_states state)
{
	enum wpa_states old_state = wpa_s->wpa_state;

	wpa_dbg(wpa_s, MSG_DEBUG, "State: %s -> %s",
		wpa_supplicant_state_txt(wpa_s->wpa_state),
		wpa_supplicant_state_txt(state));

	if (state != WPA_SCANNING)
		wpa_supplicant_notify_scanning(wpa_s, 0);

	if (state == WPA_COMPLETED && wpa_s->new_connection) {
#if defined(CONFIG_CTRL_IFACE) || !defined(CONFIG_NO_STDOUT_DEBUG)
		struct wpa_ssid *ssid = wpa_s->current_ssid;
		wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_CONNECTED "- Connection to "
			MACSTR " completed [id=%d id_str=%s]",
			MAC2STR(wpa_s->bssid),
			ssid ? ssid->id : -1,
			ssid && ssid->id_str ? ssid->id_str : "");
#endif /* CONFIG_CTRL_IFACE || !CONFIG_NO_STDOUT_DEBUG */
		wpas_clear_temp_disabled(wpa_s, ssid, 1);
		wpa_s->extra_blacklist_count = 0;
		wpa_s->new_connection = 0;
		wpa_drv_set_operstate(wpa_s, 1);
#ifndef IEEE8021X_EAPOL
		wpa_drv_set_supp_port(wpa_s, 1);
#endif /* IEEE8021X_EAPOL */
		wpa_s->after_wps = 0;
#ifdef CONFIG_P2P
		wpas_p2p_completed(wpa_s);
#endif /* CONFIG_P2P */

		sme_sched_obss_scan(wpa_s, 1);
	} else if (state == WPA_DISCONNECTED || state == WPA_ASSOCIATING ||
		   state == WPA_ASSOCIATED) {
		wpa_s->new_connection = 1;
		wpa_drv_set_operstate(wpa_s, 0);
#ifndef IEEE8021X_EAPOL
		wpa_drv_set_supp_port(wpa_s, 0);
#endif /* IEEE8021X_EAPOL */
		sme_sched_obss_scan(wpa_s, 0);
	}
	wpa_s->wpa_state = state;

#ifdef CONFIG_BGSCAN
	if (state == WPA_COMPLETED)
		wpa_supplicant_start_bgscan(wpa_s);
	else
		wpa_supplicant_stop_bgscan(wpa_s);
#endif /* CONFIG_BGSCAN */

	if (state == WPA_AUTHENTICATING)
		wpa_supplicant_stop_autoscan(wpa_s);

	if (state == WPA_DISCONNECTED || state == WPA_INACTIVE)
		wpa_supplicant_start_autoscan(wpa_s);

	if (wpa_s->wpa_state != old_state) {
		wpas_notify_state_changed(wpa_s, wpa_s->wpa_state, old_state);

		if (wpa_s->wpa_state == WPA_COMPLETED ||
		    old_state == WPA_COMPLETED)
			wpas_notify_auth_changed(wpa_s);
	}
}


void wpa_supplicant_terminate_proc(struct wpa_global *global)
{
	int pending = 0;
#ifdef CONFIG_WPS
	struct wpa_supplicant *wpa_s = global->ifaces;
	while (wpa_s) {
		if (wpas_wps_terminate_pending(wpa_s) == 1)
			pending = 1;
		wpa_s = wpa_s->next;
	}
#endif /* CONFIG_WPS */
	if (pending)
		return;
	eloop_terminate();
}


static void wpa_supplicant_terminate(int sig, void *signal_ctx)
{
	struct wpa_global *global = signal_ctx;
	wpa_supplicant_terminate_proc(global);
}


void wpa_supplicant_clear_status(struct wpa_supplicant *wpa_s)
{
	enum wpa_states old_state = wpa_s->wpa_state;

	wpa_s->pairwise_cipher = 0;
	wpa_s->group_cipher = 0;
	wpa_s->mgmt_group_cipher = 0;
	wpa_s->key_mgmt = 0;
	if (wpa_s->wpa_state != WPA_INTERFACE_DISABLED)
		wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);

	if (wpa_s->wpa_state != old_state)
		wpas_notify_state_changed(wpa_s, wpa_s->wpa_state, old_state);
}


/**
 * wpa_supplicant_reload_configuration - Reload configuration data
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: 0 on success or -1 if configuration parsing failed
 *
 * This function can be used to request that the configuration data is reloaded
 * (e.g., after configuration file change). This function is reloading
 * configuration only for one interface, so this may need to be called multiple
 * times if %wpa_supplicant is controlling multiple interfaces and all
 * interfaces need reconfiguration.
 */
int wpa_supplicant_reload_configuration(struct wpa_supplicant *wpa_s)
{
	struct wpa_config *conf;
	int reconf_ctrl;
	int old_ap_scan;

	if (wpa_s->confname == NULL)
		return -1;
	conf = wpa_config_read(wpa_s->confname);
	if (conf == NULL) {
		wpa_msg(wpa_s, MSG_ERROR, "Failed to parse the configuration "
			"file '%s' - exiting", wpa_s->confname);
		return -1;
	}
	conf->changed_parameters = (unsigned int) -1;

	reconf_ctrl = !!conf->ctrl_interface != !!wpa_s->conf->ctrl_interface
		|| (conf->ctrl_interface && wpa_s->conf->ctrl_interface &&
		    os_strcmp(conf->ctrl_interface,
			      wpa_s->conf->ctrl_interface) != 0);

	if (reconf_ctrl && wpa_s->ctrl_iface) {
		wpa_supplicant_ctrl_iface_deinit(wpa_s->ctrl_iface);
		wpa_s->ctrl_iface = NULL;
	}

	eapol_sm_invalidate_cached_session(wpa_s->eapol);
	if (wpa_s->current_ssid) {
		wpa_supplicant_deauthenticate(wpa_s,
					      WLAN_REASON_DEAUTH_LEAVING);
	}

	/*
	 * TODO: should notify EAPOL SM about changes in opensc_engine_path,
	 * pkcs11_engine_path, pkcs11_module_path.
	 */
	if (wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt)) {
		/*
		 * Clear forced success to clear EAP state for next
		 * authentication.
		 */
		eapol_sm_notify_eap_success(wpa_s->eapol, FALSE);
	}
	eapol_sm_notify_config(wpa_s->eapol, NULL, NULL);
	wpa_sm_set_config(wpa_s->wpa, NULL);
	wpa_sm_pmksa_cache_flush(wpa_s->wpa, NULL);
	wpa_sm_set_fast_reauth(wpa_s->wpa, wpa_s->conf->fast_reauth);
	rsn_preauth_deinit(wpa_s->wpa);

	old_ap_scan = wpa_s->conf->ap_scan;
	wpa_config_free(wpa_s->conf);
	wpa_s->conf = conf;
	if (old_ap_scan != wpa_s->conf->ap_scan)
		wpas_notify_ap_scan_changed(wpa_s);

	if (reconf_ctrl)
		wpa_s->ctrl_iface = wpa_supplicant_ctrl_iface_init(wpa_s);

	wpa_supplicant_update_config(wpa_s);

	wpa_supplicant_clear_status(wpa_s);
	if (wpa_supplicant_enabled_networks(wpa_s)) {
		wpa_s->reassociate = 1;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	}
	wpa_dbg(wpa_s, MSG_DEBUG, "Reconfiguration completed");
	return 0;
}


static void wpa_supplicant_reconfig(int sig, void *signal_ctx)
{
	struct wpa_global *global = signal_ctx;
	struct wpa_supplicant *wpa_s;
	for (wpa_s = global->ifaces; wpa_s; wpa_s = wpa_s->next) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Signal %d received - reconfiguring",
			sig);
		if (wpa_supplicant_reload_configuration(wpa_s) < 0) {
			wpa_supplicant_terminate_proc(global);
		}
	}
}


enum wpa_cipher cipher_suite2driver(int cipher)
{
	switch (cipher) {
	case WPA_CIPHER_NONE:
		return CIPHER_NONE;
	case WPA_CIPHER_WEP40:
		return CIPHER_WEP40;
	case WPA_CIPHER_WEP104:
		return CIPHER_WEP104;
	case WPA_CIPHER_CCMP:
		return CIPHER_CCMP;
	case WPA_CIPHER_GCMP:
		return CIPHER_GCMP;
	case WPA_CIPHER_TKIP:
	default:
		return CIPHER_TKIP;
	}
}


enum wpa_key_mgmt key_mgmt2driver(int key_mgmt)
{
	switch (key_mgmt) {
	case WPA_KEY_MGMT_NONE:
		return KEY_MGMT_NONE;
	case WPA_KEY_MGMT_IEEE8021X_NO_WPA:
		return KEY_MGMT_802_1X_NO_WPA;
	case WPA_KEY_MGMT_IEEE8021X:
		return KEY_MGMT_802_1X;
	case WPA_KEY_MGMT_WPA_NONE:
		return KEY_MGMT_WPA_NONE;
	case WPA_KEY_MGMT_FT_IEEE8021X:
		return KEY_MGMT_FT_802_1X;
	case WPA_KEY_MGMT_FT_PSK:
		return KEY_MGMT_FT_PSK;
	case WPA_KEY_MGMT_IEEE8021X_SHA256:
		return KEY_MGMT_802_1X_SHA256;
	case WPA_KEY_MGMT_PSK_SHA256:
		return KEY_MGMT_PSK_SHA256;
	case WPA_KEY_MGMT_WPS:
		return KEY_MGMT_WPS;
	case WPA_KEY_MGMT_PSK:
	default:
		return KEY_MGMT_PSK;
	}
}


static int wpa_supplicant_suites_from_ai(struct wpa_supplicant *wpa_s,
					 struct wpa_ssid *ssid,
					 struct wpa_ie_data *ie)
{
	int ret = wpa_sm_parse_own_wpa_ie(wpa_s->wpa, ie);
	if (ret) {
		if (ret == -2) {
			wpa_msg(wpa_s, MSG_INFO, "WPA: Failed to parse WPA IE "
				"from association info");
		}
		return -1;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Using WPA IE from AssocReq to set "
		"cipher suites");
	if (!(ie->group_cipher & ssid->group_cipher)) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Driver used disabled group "
			"cipher 0x%x (mask 0x%x) - reject",
			ie->group_cipher, ssid->group_cipher);
		return -1;
	}
	if (!(ie->pairwise_cipher & ssid->pairwise_cipher)) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Driver used disabled pairwise "
			"cipher 0x%x (mask 0x%x) - reject",
			ie->pairwise_cipher, ssid->pairwise_cipher);
		return -1;
	}
	if (!(ie->key_mgmt & ssid->key_mgmt)) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Driver used disabled key "
			"management 0x%x (mask 0x%x) - reject",
			ie->key_mgmt, ssid->key_mgmt);
		return -1;
	}

#ifdef CONFIG_IEEE80211W
	if (!(ie->capabilities & WPA_CAPABILITY_MFPC) &&
	    (ssid->ieee80211w == MGMT_FRAME_PROTECTION_DEFAULT ?
	     wpa_s->conf->pmf : ssid->ieee80211w) ==
	    MGMT_FRAME_PROTECTION_REQUIRED) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Driver associated with an AP "
			"that does not support management frame protection - "
			"reject");
		return -1;
	}
#endif /* CONFIG_IEEE80211W */

	return 0;
}


/**
 * wpa_supplicant_set_suites - Set authentication and encryption parameters
 * @wpa_s: Pointer to wpa_supplicant data
 * @bss: Scan results for the selected BSS, or %NULL if not available
 * @ssid: Configuration data for the selected network
 * @wpa_ie: Buffer for the WPA/RSN IE
 * @wpa_ie_len: Maximum wpa_ie buffer size on input. This is changed to be the
 * used buffer length in case the functions returns success.
 * Returns: 0 on success or -1 on failure
 *
 * This function is used to configure authentication and encryption parameters
 * based on the network configuration and scan result for the selected BSS (if
 * available).
 */
int wpa_supplicant_set_suites(struct wpa_supplicant *wpa_s,
			      struct wpa_bss *bss, struct wpa_ssid *ssid,
			      u8 *wpa_ie, size_t *wpa_ie_len)
{
	struct wpa_ie_data ie;
	int sel, proto;
	const u8 *bss_wpa, *bss_rsn;

	if (bss) {
		bss_wpa = wpa_bss_get_vendor_ie(bss, WPA_IE_VENDOR_TYPE);
		bss_rsn = wpa_bss_get_ie(bss, WLAN_EID_RSN);
	} else
		bss_wpa = bss_rsn = NULL;

	if (bss_rsn && (ssid->proto & WPA_PROTO_RSN) &&
	    wpa_parse_wpa_ie(bss_rsn, 2 + bss_rsn[1], &ie) == 0 &&
	    (ie.group_cipher & ssid->group_cipher) &&
	    (ie.pairwise_cipher & ssid->pairwise_cipher) &&
	    (ie.key_mgmt & ssid->key_mgmt)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "RSN: using IEEE 802.11i/D9.0");
		proto = WPA_PROTO_RSN;
	} else if (bss_wpa && (ssid->proto & WPA_PROTO_WPA) &&
		   wpa_parse_wpa_ie(bss_wpa, 2 +bss_wpa[1], &ie) == 0 &&
		   (ie.group_cipher & ssid->group_cipher) &&
		   (ie.pairwise_cipher & ssid->pairwise_cipher) &&
		   (ie.key_mgmt & ssid->key_mgmt)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using IEEE 802.11i/D3.0");
		proto = WPA_PROTO_WPA;
	} else if (bss) {
		wpa_msg(wpa_s, MSG_WARNING, "WPA: Failed to select WPA/RSN");
		return -1;
	} else {
		if (ssid->proto & WPA_PROTO_RSN)
			proto = WPA_PROTO_RSN;
		else
			proto = WPA_PROTO_WPA;
		if (wpa_supplicant_suites_from_ai(wpa_s, ssid, &ie) < 0) {
			os_memset(&ie, 0, sizeof(ie));
			ie.group_cipher = ssid->group_cipher;
			ie.pairwise_cipher = ssid->pairwise_cipher;
			ie.key_mgmt = ssid->key_mgmt;
#ifdef CONFIG_IEEE80211W
			ie.mgmt_group_cipher =
				ssid->ieee80211w != NO_MGMT_FRAME_PROTECTION ?
				WPA_CIPHER_AES_128_CMAC : 0;
#endif /* CONFIG_IEEE80211W */
			wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Set cipher suites "
				"based on configuration");
		} else
			proto = ie.proto;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Selected cipher suites: group %d "
		"pairwise %d key_mgmt %d proto %d",
		ie.group_cipher, ie.pairwise_cipher, ie.key_mgmt, proto);
#ifdef CONFIG_IEEE80211W
	if (ssid->ieee80211w) {
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Selected mgmt group cipher %d",
			ie.mgmt_group_cipher);
	}
#endif /* CONFIG_IEEE80211W */

	wpa_s->wpa_proto = proto;
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_PROTO, proto);
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_RSN_ENABLED,
			 !!(ssid->proto & WPA_PROTO_RSN));

	if (bss || !wpa_s->ap_ies_from_associnfo) {
		if (wpa_sm_set_ap_wpa_ie(wpa_s->wpa, bss_wpa,
					 bss_wpa ? 2 + bss_wpa[1] : 0) ||
		    wpa_sm_set_ap_rsn_ie(wpa_s->wpa, bss_rsn,
					 bss_rsn ? 2 + bss_rsn[1] : 0))
			return -1;
	}

	sel = ie.group_cipher & ssid->group_cipher;
	if (sel & WPA_CIPHER_CCMP) {
		wpa_s->group_cipher = WPA_CIPHER_CCMP;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using GTK CCMP");
	} else if (sel & WPA_CIPHER_GCMP) {
		wpa_s->group_cipher = WPA_CIPHER_GCMP;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using GTK GCMP");
	} else if (sel & WPA_CIPHER_TKIP) {
		wpa_s->group_cipher = WPA_CIPHER_TKIP;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using GTK TKIP");
	} else if (sel & WPA_CIPHER_WEP104) {
		wpa_s->group_cipher = WPA_CIPHER_WEP104;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using GTK WEP104");
	} else if (sel & WPA_CIPHER_WEP40) {
		wpa_s->group_cipher = WPA_CIPHER_WEP40;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using GTK WEP40");
	} else {
		wpa_msg(wpa_s, MSG_WARNING, "WPA: Failed to select group "
			"cipher");
		return -1;
	}

	sel = ie.pairwise_cipher & ssid->pairwise_cipher;
	if (sel & WPA_CIPHER_CCMP) {
		wpa_s->pairwise_cipher = WPA_CIPHER_CCMP;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using PTK CCMP");
	} else if (sel & WPA_CIPHER_GCMP) {
		wpa_s->pairwise_cipher = WPA_CIPHER_GCMP;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using PTK GCMP");
	} else if (sel & WPA_CIPHER_TKIP) {
		wpa_s->pairwise_cipher = WPA_CIPHER_TKIP;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using PTK TKIP");
	} else if (sel & WPA_CIPHER_NONE) {
		wpa_s->pairwise_cipher = WPA_CIPHER_NONE;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using PTK NONE");
	} else {
		wpa_msg(wpa_s, MSG_WARNING, "WPA: Failed to select pairwise "
			"cipher");
		return -1;
	}

	sel = ie.key_mgmt & ssid->key_mgmt;
#ifdef CONFIG_SAE
	if (!(wpa_s->drv_flags & WPA_DRIVER_FLAGS_SAE))
		sel &= ~(WPA_KEY_MGMT_SAE | WPA_KEY_MGMT_FT_SAE);
#endif /* CONFIG_SAE */
	if (0) {
#ifdef CONFIG_IEEE80211R
	} else if (sel & WPA_KEY_MGMT_FT_IEEE8021X) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_FT_IEEE8021X;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using KEY_MGMT FT/802.1X");
	} else if (sel & WPA_KEY_MGMT_FT_PSK) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_FT_PSK;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using KEY_MGMT FT/PSK");
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_SAE
	} else if (sel & WPA_KEY_MGMT_SAE) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_SAE;
		wpa_dbg(wpa_s, MSG_DEBUG, "RSN: using KEY_MGMT SAE");
	} else if (sel & WPA_KEY_MGMT_FT_SAE) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_FT_SAE;
		wpa_dbg(wpa_s, MSG_DEBUG, "RSN: using KEY_MGMT FT/SAE");
#endif /* CONFIG_SAE */
#ifdef CONFIG_IEEE80211W
	} else if (sel & WPA_KEY_MGMT_IEEE8021X_SHA256) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_IEEE8021X_SHA256;
		wpa_dbg(wpa_s, MSG_DEBUG,
			"WPA: using KEY_MGMT 802.1X with SHA256");
	} else if (sel & WPA_KEY_MGMT_PSK_SHA256) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_PSK_SHA256;
		wpa_dbg(wpa_s, MSG_DEBUG,
			"WPA: using KEY_MGMT PSK with SHA256");
#endif /* CONFIG_IEEE80211W */
	} else if (sel & WPA_KEY_MGMT_IEEE8021X) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_IEEE8021X;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using KEY_MGMT 802.1X");
	} else if (sel & WPA_KEY_MGMT_PSK) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_PSK;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using KEY_MGMT WPA-PSK");
	} else if (sel & WPA_KEY_MGMT_WPA_NONE) {
		wpa_s->key_mgmt = WPA_KEY_MGMT_WPA_NONE;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using KEY_MGMT WPA-NONE");
	} else {
		wpa_msg(wpa_s, MSG_WARNING, "WPA: Failed to select "
			"authenticated key management type");
		return -1;
	}

	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_KEY_MGMT, wpa_s->key_mgmt);
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_PAIRWISE,
			 wpa_s->pairwise_cipher);
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_GROUP, wpa_s->group_cipher);

#ifdef CONFIG_IEEE80211W
	sel = ie.mgmt_group_cipher;
	if ((ssid->ieee80211w == MGMT_FRAME_PROTECTION_DEFAULT ?
	     wpa_s->conf->pmf : ssid->ieee80211w) == NO_MGMT_FRAME_PROTECTION ||
	    !(ie.capabilities & WPA_CAPABILITY_MFPC))
		sel = 0;
	if (sel & WPA_CIPHER_AES_128_CMAC) {
		wpa_s->mgmt_group_cipher = WPA_CIPHER_AES_128_CMAC;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: using MGMT group cipher "
			"AES-128-CMAC");
	} else {
		wpa_s->mgmt_group_cipher = 0;
		wpa_dbg(wpa_s, MSG_DEBUG, "WPA: not using MGMT group cipher");
	}
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_MGMT_GROUP,
			 wpa_s->mgmt_group_cipher);
	wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_MFP,
			 (ssid->ieee80211w == MGMT_FRAME_PROTECTION_DEFAULT ?
			  wpa_s->conf->pmf : ssid->ieee80211w));
#endif /* CONFIG_IEEE80211W */

	if (wpa_sm_set_assoc_wpa_ie_default(wpa_s->wpa, wpa_ie, wpa_ie_len)) {
		wpa_msg(wpa_s, MSG_WARNING, "WPA: Failed to generate WPA IE");
		return -1;
	}

	if (wpa_key_mgmt_wpa_psk(ssid->key_mgmt)) {
		wpa_sm_set_pmk(wpa_s->wpa, ssid->psk, PMK_LEN);
#ifndef CONFIG_NO_PBKDF2
		if (bss && ssid->bssid_set && ssid->ssid_len == 0 &&
		    ssid->passphrase) {
			u8 psk[PMK_LEN];
		        pbkdf2_sha1(ssid->passphrase, bss->ssid, bss->ssid_len,
				    4096, psk, PMK_LEN);
		        wpa_hexdump_key(MSG_MSGDUMP, "PSK (from passphrase)",
					psk, PMK_LEN);
			wpa_sm_set_pmk(wpa_s->wpa, psk, PMK_LEN);
		}
#endif /* CONFIG_NO_PBKDF2 */
#ifdef CONFIG_EXT_PASSWORD
		if (ssid->ext_psk) {
			struct wpabuf *pw = ext_password_get(wpa_s->ext_pw,
							     ssid->ext_psk);
			char pw_str[64 + 1];
			u8 psk[PMK_LEN];

			if (pw == NULL) {
				wpa_msg(wpa_s, MSG_INFO, "EXT PW: No PSK "
					"found from external storage");
				return -1;
			}

			if (wpabuf_len(pw) < 8 || wpabuf_len(pw) > 64) {
				wpa_msg(wpa_s, MSG_INFO, "EXT PW: Unexpected "
					"PSK length %d in external storage",
					(int) wpabuf_len(pw));
				ext_password_free(pw);
				return -1;
			}

			os_memcpy(pw_str, wpabuf_head(pw), wpabuf_len(pw));
			pw_str[wpabuf_len(pw)] = '\0';

#ifndef CONFIG_NO_PBKDF2
			if (wpabuf_len(pw) >= 8 && wpabuf_len(pw) < 64 && bss)
			{
				pbkdf2_sha1(pw_str, bss->ssid, bss->ssid_len,
					    4096, psk, PMK_LEN);
				os_memset(pw_str, 0, sizeof(pw_str));
				wpa_hexdump_key(MSG_MSGDUMP, "PSK (from "
						"external passphrase)",
						psk, PMK_LEN);
				wpa_sm_set_pmk(wpa_s->wpa, psk, PMK_LEN);
			} else
#endif /* CONFIG_NO_PBKDF2 */
			if (wpabuf_len(pw) == 2 * PMK_LEN) {
				if (hexstr2bin(pw_str, psk, PMK_LEN) < 0) {
					wpa_msg(wpa_s, MSG_INFO, "EXT PW: "
						"Invalid PSK hex string");
					os_memset(pw_str, 0, sizeof(pw_str));
					ext_password_free(pw);
					return -1;
				}
				wpa_sm_set_pmk(wpa_s->wpa, psk, PMK_LEN);
			} else {
				wpa_msg(wpa_s, MSG_INFO, "EXT PW: No suitable "
					"PSK available");
				os_memset(pw_str, 0, sizeof(pw_str));
				ext_password_free(pw);
				return -1;
			}

			os_memset(pw_str, 0, sizeof(pw_str));
			ext_password_free(pw);
		}
#endif /* CONFIG_EXT_PASSWORD */
	} else
		wpa_sm_set_pmk_from_pmksa(wpa_s->wpa);

	return 0;
}


int wpas_build_ext_capab(struct wpa_supplicant *wpa_s, u8 *buf)
{
	u32 ext_capab = 0;
	u8 *pos = buf;

#ifdef CONFIG_INTERWORKING
	if (wpa_s->conf->interworking)
		ext_capab |= BIT(31); /* Interworking */
#endif /* CONFIG_INTERWORKING */

#ifdef CONFIG_WNM
	ext_capab |= BIT(17); /* WNM-Sleep Mode */
	ext_capab |= BIT(19); /* BSS Transition */
#endif /* CONFIG_WNM */

	if (!ext_capab)
		return 0;

	*pos++ = WLAN_EID_EXT_CAPAB;
	*pos++ = 4;
	WPA_PUT_LE32(pos, ext_capab);
	pos += 4;

	return pos - buf;
}


/**
 * wpa_supplicant_associate - Request association
 * @wpa_s: Pointer to wpa_supplicant data
 * @bss: Scan results for the selected BSS, or %NULL if not available
 * @ssid: Configuration data for the selected network
 *
 * This function is used to request %wpa_supplicant to associate with a BSS.
 */
void wpa_supplicant_associate(struct wpa_supplicant *wpa_s,
			      struct wpa_bss *bss, struct wpa_ssid *ssid)
{
	u8 wpa_ie[200];
	size_t wpa_ie_len;
	int use_crypt, ret, i, bssid_changed;
	int algs = WPA_AUTH_ALG_OPEN;
	enum wpa_cipher cipher_pairwise, cipher_group;
	struct wpa_driver_associate_params params;
	int wep_keys_set = 0;
	struct wpa_driver_capa capa;
	int assoc_failed = 0;
	struct wpa_ssid *old_ssid;
	u8 ext_capab[10];
	int ext_capab_len;
#ifdef CONFIG_HT_OVERRIDES
	struct ieee80211_ht_capabilities htcaps;
	struct ieee80211_ht_capabilities htcaps_mask;
#endif /* CONFIG_HT_OVERRIDES */

#ifdef CONFIG_IBSS_RSN
	ibss_rsn_deinit(wpa_s->ibss_rsn);
	wpa_s->ibss_rsn = NULL;
#endif /* CONFIG_IBSS_RSN */

	if (ssid->mode == WPAS_MODE_AP || ssid->mode == WPAS_MODE_P2P_GO ||
	    ssid->mode == WPAS_MODE_P2P_GROUP_FORMATION) {
#ifdef CONFIG_AP
		if (!(wpa_s->drv_flags & WPA_DRIVER_FLAGS_AP)) {
			wpa_msg(wpa_s, MSG_INFO, "Driver does not support AP "
				"mode");
			return;
		}
		if (wpa_supplicant_create_ap(wpa_s, ssid) < 0) {
			wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
			return;
		}
		wpa_s->current_bss = bss;
#else /* CONFIG_AP */
		wpa_msg(wpa_s, MSG_ERROR, "AP mode support not included in "
			"the build");
#endif /* CONFIG_AP */
		return;
	}

#ifdef CONFIG_TDLS
	if (bss)
		wpa_tdls_ap_ies(wpa_s->wpa, (const u8 *) (bss + 1),
				bss->ie_len);
#endif /* CONFIG_TDLS */

	if ((wpa_s->drv_flags & WPA_DRIVER_FLAGS_SME) &&
	    ssid->mode == IEEE80211_MODE_INFRA) {
		sme_authenticate(wpa_s, bss, ssid);
		return;
	}

	os_memset(&params, 0, sizeof(params));
	wpa_s->reassociate = 0;
	if (bss && !wpas_driver_bss_selection(wpa_s)) {
#ifdef CONFIG_IEEE80211R
		const u8 *ie, *md = NULL;
#endif /* CONFIG_IEEE80211R */
		wpa_msg(wpa_s, MSG_INFO, "Trying to associate with " MACSTR
			" (SSID='%s' freq=%d MHz)", MAC2STR(bss->bssid),
			wpa_ssid_txt(bss->ssid, bss->ssid_len), bss->freq);
		bssid_changed = !is_zero_ether_addr(wpa_s->bssid);
		os_memset(wpa_s->bssid, 0, ETH_ALEN);
		os_memcpy(wpa_s->pending_bssid, bss->bssid, ETH_ALEN);
		if (bssid_changed)
			wpas_notify_bssid_changed(wpa_s);
#ifdef CONFIG_IEEE80211R
		ie = wpa_bss_get_ie(bss, WLAN_EID_MOBILITY_DOMAIN);
		if (ie && ie[1] >= MOBILITY_DOMAIN_ID_LEN)
			md = ie + 2;
		wpa_sm_set_ft_params(wpa_s->wpa, ie, ie ? 2 + ie[1] : 0);
		if (md) {
			/* Prepare for the next transition */
			wpa_ft_prepare_auth_request(wpa_s->wpa, ie);
		}
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_WPS
	} else if ((ssid->ssid == NULL || ssid->ssid_len == 0) &&
		   wpa_s->conf->ap_scan == 2 &&
		   (ssid->key_mgmt & WPA_KEY_MGMT_WPS)) {
		/* Use ap_scan==1 style network selection to find the network
		 */
		wpa_s->scan_req = MANUAL_SCAN_REQ;
		wpa_s->reassociate = 1;
		wpa_supplicant_req_scan(wpa_s, 0, 0);
		return;
#endif /* CONFIG_WPS */
	} else {
		wpa_msg(wpa_s, MSG_INFO, "Trying to associate with SSID '%s'",
			wpa_ssid_txt(ssid->ssid, ssid->ssid_len));
		os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
	}
	wpa_supplicant_cancel_sched_scan(wpa_s);
	wpa_supplicant_cancel_scan(wpa_s);

	/* Starting new association, so clear the possibly used WPA IE from the
	 * previous association. */
	wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, NULL, 0);

#ifdef IEEE8021X_EAPOL
	if (ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
		if (ssid->leap) {
			if (ssid->non_leap == 0)
				algs = WPA_AUTH_ALG_LEAP;
			else
				algs |= WPA_AUTH_ALG_LEAP;
		}
	}
#endif /* IEEE8021X_EAPOL */
	wpa_dbg(wpa_s, MSG_DEBUG, "Automatic auth_alg selection: 0x%x", algs);
	if (ssid->auth_alg) {
		algs = ssid->auth_alg;
		wpa_dbg(wpa_s, MSG_DEBUG, "Overriding auth_alg selection: "
			"0x%x", algs);
	}

	if (bss && (wpa_bss_get_vendor_ie(bss, WPA_IE_VENDOR_TYPE) ||
		    wpa_bss_get_ie(bss, WLAN_EID_RSN)) &&
	    wpa_key_mgmt_wpa(ssid->key_mgmt)) {
		int try_opportunistic;
		try_opportunistic = (ssid->proactive_key_caching < 0 ?
				     wpa_s->conf->okc :
				     ssid->proactive_key_caching) &&
			(ssid->proto & WPA_PROTO_RSN);
		if (pmksa_cache_set_current(wpa_s->wpa, NULL, bss->bssid,
					    wpa_s->current_ssid,
					    try_opportunistic) == 0)
			eapol_sm_notify_pmkid_attempt(wpa_s->eapol, 1);
		wpa_ie_len = sizeof(wpa_ie);
		if (wpa_supplicant_set_suites(wpa_s, bss, ssid,
					      wpa_ie, &wpa_ie_len)) {
			wpa_msg(wpa_s, MSG_WARNING, "WPA: Failed to set WPA "
				"key management and encryption suites");
			return;
		}
	} else if ((ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA) && bss &&
		   wpa_key_mgmt_wpa_ieee8021x(ssid->key_mgmt)) {
		/*
		 * Both WPA and non-WPA IEEE 802.1X enabled in configuration -
		 * use non-WPA since the scan results did not indicate that the
		 * AP is using WPA or WPA2.
		 */
		wpa_supplicant_set_non_wpa_policy(wpa_s, ssid);
		wpa_ie_len = 0;
		wpa_s->wpa_proto = 0;
	} else if (wpa_key_mgmt_wpa_any(ssid->key_mgmt)) {
		wpa_ie_len = sizeof(wpa_ie);
		if (wpa_supplicant_set_suites(wpa_s, NULL, ssid,
					      wpa_ie, &wpa_ie_len)) {
			wpa_msg(wpa_s, MSG_WARNING, "WPA: Failed to set WPA "
				"key management and encryption suites (no "
				"scan results)");
			return;
		}
#ifdef CONFIG_WPS
	} else if (ssid->key_mgmt & WPA_KEY_MGMT_WPS) {
		struct wpabuf *wps_ie;
		wps_ie = wps_build_assoc_req_ie(wpas_wps_get_req_type(ssid));
		if (wps_ie && wpabuf_len(wps_ie) <= sizeof(wpa_ie)) {
			wpa_ie_len = wpabuf_len(wps_ie);
			os_memcpy(wpa_ie, wpabuf_head(wps_ie), wpa_ie_len);
		} else
			wpa_ie_len = 0;
		wpabuf_free(wps_ie);
		wpa_supplicant_set_non_wpa_policy(wpa_s, ssid);
		if (!bss || (bss->caps & IEEE80211_CAP_PRIVACY))
			params.wps = WPS_MODE_PRIVACY;
		else
			params.wps = WPS_MODE_OPEN;
		wpa_s->wpa_proto = 0;
#endif /* CONFIG_WPS */
	} else {
		wpa_supplicant_set_non_wpa_policy(wpa_s, ssid);
		wpa_ie_len = 0;
		wpa_s->wpa_proto = 0;
	}

#ifdef CONFIG_P2P
	if (wpa_s->global->p2p) {
		u8 *pos;
		size_t len;
		int res;
		pos = wpa_ie + wpa_ie_len;
		len = sizeof(wpa_ie) - wpa_ie_len;
		res = wpas_p2p_assoc_req_ie(wpa_s, bss, pos, len,
					    ssid->p2p_group);
		if (res >= 0)
			wpa_ie_len += res;
	}

	wpa_s->cross_connect_disallowed = 0;
	if (bss) {
		struct wpabuf *p2p;
		p2p = wpa_bss_get_vendor_ie_multi(bss, P2P_IE_VENDOR_TYPE);
		if (p2p) {
			wpa_s->cross_connect_disallowed =
				p2p_get_cross_connect_disallowed(p2p);
			wpabuf_free(p2p);
			wpa_dbg(wpa_s, MSG_DEBUG, "P2P: WLAN AP %s cross "
				"connection",
				wpa_s->cross_connect_disallowed ?
				"disallows" : "allows");
		}
	}
#endif /* CONFIG_P2P */

#ifdef CONFIG_HS20
	if (wpa_s->conf->hs20) {
		struct wpabuf *hs20;
		hs20 = wpabuf_alloc(20);
		if (hs20) {
			wpas_hs20_add_indication(hs20);
			os_memcpy(wpa_ie + wpa_ie_len, wpabuf_head(hs20),
				  wpabuf_len(hs20));
			wpa_ie_len += wpabuf_len(hs20);
			wpabuf_free(hs20);
		}
	}
#endif /* CONFIG_HS20 */

	ext_capab_len = wpas_build_ext_capab(wpa_s, ext_capab);
	if (ext_capab_len > 0) {
		u8 *pos = wpa_ie;
		if (wpa_ie_len > 0 && pos[0] == WLAN_EID_RSN)
			pos += 2 + pos[1];
		os_memmove(pos + ext_capab_len, pos,
			   wpa_ie_len - (pos - wpa_ie));
		wpa_ie_len += ext_capab_len;
		os_memcpy(pos, ext_capab, ext_capab_len);
	}

	wpa_clear_keys(wpa_s, bss ? bss->bssid : NULL);
	use_crypt = 1;
	cipher_pairwise = cipher_suite2driver(wpa_s->pairwise_cipher);
	cipher_group = cipher_suite2driver(wpa_s->group_cipher);
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_NONE ||
	    wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
		if (wpa_s->key_mgmt == WPA_KEY_MGMT_NONE)
			use_crypt = 0;
		if (wpa_set_wep_keys(wpa_s, ssid)) {
			use_crypt = 1;
			wep_keys_set = 1;
		}
	}
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_WPS)
		use_crypt = 0;

#ifdef IEEE8021X_EAPOL
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
		if ((ssid->eapol_flags &
		     (EAPOL_FLAG_REQUIRE_KEY_UNICAST |
		      EAPOL_FLAG_REQUIRE_KEY_BROADCAST)) == 0 &&
		    !wep_keys_set) {
			use_crypt = 0;
		} else {
			/* Assume that dynamic WEP-104 keys will be used and
			 * set cipher suites in order for drivers to expect
			 * encryption. */
			cipher_pairwise = cipher_group = CIPHER_WEP104;
		}
	}
#endif /* IEEE8021X_EAPOL */

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE) {
		/* Set the key before (and later after) association */
		wpa_supplicant_set_wpa_none_key(wpa_s, ssid);
	}

	wpa_supplicant_set_state(wpa_s, WPA_ASSOCIATING);
	if (bss) {
		params.ssid = bss->ssid;
		params.ssid_len = bss->ssid_len;
		if (!wpas_driver_bss_selection(wpa_s) || ssid->bssid_set) {
			wpa_printf(MSG_DEBUG, "Limit connection to BSSID "
				   MACSTR " freq=%u MHz based on scan results "
				   "(bssid_set=%d)",
				   MAC2STR(bss->bssid), bss->freq,
				   ssid->bssid_set);
			params.bssid = bss->bssid;
			params.freq = bss->freq;
		}
	} else {
		params.ssid = ssid->ssid;
		params.ssid_len = ssid->ssid_len;
	}

	if (ssid->mode == WPAS_MODE_IBSS && ssid->bssid_set &&
	    wpa_s->conf->ap_scan == 2) {
		params.bssid = ssid->bssid;
		params.fixed_bssid = 1;
	}

	if (ssid->mode == WPAS_MODE_IBSS && ssid->frequency > 0 &&
	    params.freq == 0)
		params.freq = ssid->frequency; /* Initial channel for IBSS */
	params.wpa_ie = wpa_ie;
	params.wpa_ie_len = wpa_ie_len;
	params.pairwise_suite = cipher_pairwise;
	params.group_suite = cipher_group;
	params.key_mgmt_suite = key_mgmt2driver(wpa_s->key_mgmt);
	params.wpa_proto = wpa_s->wpa_proto;
	params.auth_alg = algs;
	params.mode = ssid->mode;
	params.bg_scan_period = ssid->bg_scan_period;
	for (i = 0; i < NUM_WEP_KEYS; i++) {
		if (ssid->wep_key_len[i])
			params.wep_key[i] = ssid->wep_key[i];
		params.wep_key_len[i] = ssid->wep_key_len[i];
	}
	params.wep_tx_keyidx = ssid->wep_tx_keyidx;

	if ((wpa_s->drv_flags & WPA_DRIVER_FLAGS_4WAY_HANDSHAKE) &&
	    (params.key_mgmt_suite == KEY_MGMT_PSK ||
	     params.key_mgmt_suite == KEY_MGMT_FT_PSK)) {
		params.passphrase = ssid->passphrase;
		if (ssid->psk_set)
			params.psk = ssid->psk;
	}

	params.drop_unencrypted = use_crypt;

#ifdef CONFIG_IEEE80211W
	params.mgmt_frame_protection =
		ssid->ieee80211w == MGMT_FRAME_PROTECTION_DEFAULT ?
		wpa_s->conf->pmf : ssid->ieee80211w;
	if (params.mgmt_frame_protection != NO_MGMT_FRAME_PROTECTION && bss) {
		const u8 *rsn = wpa_bss_get_ie(bss, WLAN_EID_RSN);
		struct wpa_ie_data ie;
		if (rsn && wpa_parse_wpa_ie(rsn, 2 + rsn[1], &ie) == 0 &&
		    ie.capabilities &
		    (WPA_CAPABILITY_MFPC | WPA_CAPABILITY_MFPR)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Selected AP supports "
				"MFP: require MFP");
			params.mgmt_frame_protection =
				MGMT_FRAME_PROTECTION_REQUIRED;
		}
	}
#endif /* CONFIG_IEEE80211W */

	params.p2p = ssid->p2p_group;

	if (wpa_s->parent->set_sta_uapsd)
		params.uapsd = wpa_s->parent->sta_uapsd;
	else
		params.uapsd = -1;

#ifdef CONFIG_HT_OVERRIDES
	os_memset(&htcaps, 0, sizeof(htcaps));
	os_memset(&htcaps_mask, 0, sizeof(htcaps_mask));
	params.htcaps = (u8 *) &htcaps;
	params.htcaps_mask = (u8 *) &htcaps_mask;
	wpa_supplicant_apply_ht_overrides(wpa_s, ssid, &params);
#endif /* CONFIG_HT_OVERRIDES */

	ret = wpa_drv_associate(wpa_s, &params);
	if (ret < 0) {
		wpa_msg(wpa_s, MSG_INFO, "Association request to the driver "
			"failed");
		if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_SANE_ERROR_CODES) {
			/*
			 * The driver is known to mean what is saying, so we
			 * can stop right here; the association will not
			 * succeed.
			 */
			wpas_connection_failed(wpa_s, wpa_s->pending_bssid);
			wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
			os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
			return;
		}
		/* try to continue anyway; new association will be tried again
		 * after timeout */
		assoc_failed = 1;
	}

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE) {
		/* Set the key after the association just in case association
		 * cleared the previously configured key. */
		wpa_supplicant_set_wpa_none_key(wpa_s, ssid);
		/* No need to timeout authentication since there is no key
		 * management. */
		wpa_supplicant_cancel_auth_timeout(wpa_s);
		wpa_supplicant_set_state(wpa_s, WPA_COMPLETED);
#ifdef CONFIG_IBSS_RSN
	} else if (ssid->mode == WPAS_MODE_IBSS &&
		   wpa_s->key_mgmt != WPA_KEY_MGMT_NONE &&
		   wpa_s->key_mgmt != WPA_KEY_MGMT_WPA_NONE) {
		/*
		 * RSN IBSS authentication is per-STA and we can disable the
		 * per-BSSID authentication.
		 */
		wpa_supplicant_cancel_auth_timeout(wpa_s);
#endif /* CONFIG_IBSS_RSN */
	} else {
		/* Timeout for IEEE 802.11 authentication and association */
		int timeout = 60;

		if (assoc_failed) {
			/* give IBSS a bit more time */
			timeout = ssid->mode == WPAS_MODE_IBSS ? 10 : 5;
		} else if (wpa_s->conf->ap_scan == 1) {
			/* give IBSS a bit more time */
			timeout = ssid->mode == WPAS_MODE_IBSS ? 20 : 10;
		}
		wpa_supplicant_req_auth_timeout(wpa_s, timeout, 0);
	}

	if (wep_keys_set && wpa_drv_get_capa(wpa_s, &capa) == 0 &&
	    capa.flags & WPA_DRIVER_FLAGS_SET_KEYS_AFTER_ASSOC) {
		/* Set static WEP keys again */
		wpa_set_wep_keys(wpa_s, ssid);
	}

	if (wpa_s->current_ssid && wpa_s->current_ssid != ssid) {
		/*
		 * Do not allow EAP session resumption between different
		 * network configurations.
		 */
		eapol_sm_invalidate_cached_session(wpa_s->eapol);
	}
	old_ssid = wpa_s->current_ssid;
	wpa_s->current_ssid = ssid;
	wpa_s->current_bss = bss;
	wpa_supplicant_rsn_supp_set_config(wpa_s, wpa_s->current_ssid);
	wpa_supplicant_initiate_eapol(wpa_s);
	if (old_ssid != wpa_s->current_ssid)
		wpas_notify_network_changed(wpa_s);
}


static void wpa_supplicant_clear_connection(struct wpa_supplicant *wpa_s,
					    const u8 *addr)
{
	struct wpa_ssid *old_ssid;

	wpa_clear_keys(wpa_s, addr);
	old_ssid = wpa_s->current_ssid;
	wpa_supplicant_mark_disassoc(wpa_s);
	wpa_sm_set_config(wpa_s->wpa, NULL);
	eapol_sm_notify_config(wpa_s->eapol, NULL, NULL);
	if (old_ssid != wpa_s->current_ssid)
		wpas_notify_network_changed(wpa_s);
	eloop_cancel_timeout(wpa_supplicant_timeout, wpa_s, NULL);
}


/**
 * wpa_supplicant_deauthenticate - Deauthenticate the current connection
 * @wpa_s: Pointer to wpa_supplicant data
 * @reason_code: IEEE 802.11 reason code for the deauthenticate frame
 *
 * This function is used to request %wpa_supplicant to deauthenticate from the
 * current AP.
 */
void wpa_supplicant_deauthenticate(struct wpa_supplicant *wpa_s,
				   int reason_code)
{
	u8 *addr = NULL;
	union wpa_event_data event;
	int zero_addr = 0;

	wpa_dbg(wpa_s, MSG_DEBUG, "Request to deauthenticate - bssid=" MACSTR
		" pending_bssid=" MACSTR " reason=%d state=%s",
		MAC2STR(wpa_s->bssid), MAC2STR(wpa_s->pending_bssid),
		reason_code, wpa_supplicant_state_txt(wpa_s->wpa_state));

	if (!is_zero_ether_addr(wpa_s->bssid))
		addr = wpa_s->bssid;
	else if (!is_zero_ether_addr(wpa_s->pending_bssid) &&
		 (wpa_s->wpa_state == WPA_AUTHENTICATING ||
		  wpa_s->wpa_state == WPA_ASSOCIATING))
		addr = wpa_s->pending_bssid;
	else if (wpa_s->wpa_state == WPA_ASSOCIATING) {
		/*
		 * When using driver-based BSS selection, we may not know the
		 * BSSID with which we are currently trying to associate. We
		 * need to notify the driver of this disconnection even in such
		 * a case, so use the all zeros address here.
		 */
		addr = wpa_s->bssid;
		zero_addr = 1;
	}

	if (addr) {
		wpa_drv_deauthenticate(wpa_s, addr, reason_code);
		os_memset(&event, 0, sizeof(event));
		event.deauth_info.reason_code = (u16) reason_code;
		event.deauth_info.locally_generated = 1;
		wpa_supplicant_event(wpa_s, EVENT_DEAUTH, &event);
		if (zero_addr)
			addr = NULL;
	}

	wpa_supplicant_clear_connection(wpa_s, addr);
}


/**
 * wpa_supplicant_enable_network - Mark a configured network as enabled
 * @wpa_s: wpa_supplicant structure for a network interface
 * @ssid: wpa_ssid structure for a configured network or %NULL
 *
 * Enables the specified network or all networks if no network specified.
 */
void wpa_supplicant_enable_network(struct wpa_supplicant *wpa_s,
				   struct wpa_ssid *ssid)
{
	struct wpa_ssid *other_ssid;
	int was_disabled;

	if (ssid == NULL) {
		for (other_ssid = wpa_s->conf->ssid; other_ssid;
		     other_ssid = other_ssid->next) {
			if (other_ssid->disabled == 2)
				continue; /* do not change persistent P2P group
					   * data */
			if (other_ssid == wpa_s->current_ssid &&
			    other_ssid->disabled)
				wpa_s->reassociate = 1;

			was_disabled = other_ssid->disabled;

			other_ssid->disabled = 0;
			if (was_disabled)
				wpas_clear_temp_disabled(wpa_s, other_ssid, 0);

			if (was_disabled != other_ssid->disabled)
				wpas_notify_network_enabled_changed(
					wpa_s, other_ssid);
		}
		if (wpa_s->reassociate)
			wpa_supplicant_req_scan(wpa_s, 0, 0);
	} else if (ssid->disabled && ssid->disabled != 2) {
		if (wpa_s->current_ssid == NULL) {
			/*
			 * Try to reassociate since there is no current
			 * configuration and a new network was made available.
			 */
			wpa_s->reassociate = 1;
			wpa_supplicant_req_scan(wpa_s, 0, 0);
		}

		was_disabled = ssid->disabled;

		ssid->disabled = 0;
		wpas_clear_temp_disabled(wpa_s, ssid, 1);

		if (was_disabled != ssid->disabled)
			wpas_notify_network_enabled_changed(wpa_s, ssid);
	}
}


/**
 * wpa_supplicant_disable_network - Mark a configured network as disabled
 * @wpa_s: wpa_supplicant structure for a network interface
 * @ssid: wpa_ssid structure for a configured network or %NULL
 *
 * Disables the specified network or all networks if no network specified.
 */
void wpa_supplicant_disable_network(struct wpa_supplicant *wpa_s,
				    struct wpa_ssid *ssid)
{
	struct wpa_ssid *other_ssid;
	int was_disabled;

	if (ssid == NULL) {
		for (other_ssid = wpa_s->conf->ssid; other_ssid;
		     other_ssid = other_ssid->next) {
			was_disabled = other_ssid->disabled;
			if (was_disabled == 2)
				continue; /* do not change persistent P2P group
					   * data */

			other_ssid->disabled = 1;

			if (was_disabled != other_ssid->disabled)
				wpas_notify_network_enabled_changed(
					wpa_s, other_ssid);
		}
		if (wpa_s->current_ssid)
			wpa_supplicant_deauthenticate(
				wpa_s, WLAN_REASON_DEAUTH_LEAVING);
	} else if (ssid->disabled != 2) {
		if (ssid == wpa_s->current_ssid)
			wpa_supplicant_deauthenticate(
				wpa_s, WLAN_REASON_DEAUTH_LEAVING);

		was_disabled = ssid->disabled;

		ssid->disabled = 1;

		if (was_disabled != ssid->disabled)
			wpas_notify_network_enabled_changed(wpa_s, ssid);
	}
}


/**
 * wpa_supplicant_select_network - Attempt association with a network
 * @wpa_s: wpa_supplicant structure for a network interface
 * @ssid: wpa_ssid structure for a configured network or %NULL for any network
 */
void wpa_supplicant_select_network(struct wpa_supplicant *wpa_s,
				   struct wpa_ssid *ssid)
{

	struct wpa_ssid *other_ssid;
	int disconnected = 0;

	if (ssid && ssid != wpa_s->current_ssid && wpa_s->current_ssid) {
		wpa_supplicant_deauthenticate(
			wpa_s, WLAN_REASON_DEAUTH_LEAVING);
		disconnected = 1;
	}

	if (ssid)
		wpas_clear_temp_disabled(wpa_s, ssid, 1);

	/*
	 * Mark all other networks disabled or mark all networks enabled if no
	 * network specified.
	 */
	for (other_ssid = wpa_s->conf->ssid; other_ssid;
	     other_ssid = other_ssid->next) {
		int was_disabled = other_ssid->disabled;
		if (was_disabled == 2)
			continue; /* do not change persistent P2P group data */

		other_ssid->disabled = ssid ? (ssid->id != other_ssid->id) : 0;
		if (was_disabled && !other_ssid->disabled)
			wpas_clear_temp_disabled(wpa_s, other_ssid, 0);

		if (was_disabled != other_ssid->disabled)
			wpas_notify_network_enabled_changed(wpa_s, other_ssid);
	}

	if (ssid && ssid == wpa_s->current_ssid && wpa_s->current_ssid) {
		/* We are already associated with the selected network */
		wpa_printf(MSG_DEBUG, "Already associated with the "
			   "selected network - do nothing");
		return;
	}

	if (ssid)
		wpa_s->current_ssid = ssid;
	wpa_s->connect_without_scan = NULL;
	wpa_s->disconnected = 0;
	wpa_s->reassociate = 1;
	wpa_supplicant_req_scan(wpa_s, 0, disconnected ? 100000 : 0);

	if (ssid)
		wpas_notify_network_selected(wpa_s, ssid);
}


/**
 * wpa_supplicant_set_ap_scan - Set AP scan mode for interface
 * @wpa_s: wpa_supplicant structure for a network interface
 * @ap_scan: AP scan mode
 * Returns: 0 if succeed or -1 if ap_scan has an invalid value
 *
 */
int wpa_supplicant_set_ap_scan(struct wpa_supplicant *wpa_s, int ap_scan)
{

	int old_ap_scan;

	if (ap_scan < 0 || ap_scan > 2)
		return -1;

#ifdef ANDROID
	if (ap_scan == 2 && ap_scan != wpa_s->conf->ap_scan &&
	    wpa_s->wpa_state >= WPA_ASSOCIATING &&
	    wpa_s->wpa_state < WPA_COMPLETED) {
		wpa_printf(MSG_ERROR, "ap_scan = %d (%d) rejected while "
			   "associating", wpa_s->conf->ap_scan, ap_scan);
		return 0;
	}
#endif /* ANDROID */

	old_ap_scan = wpa_s->conf->ap_scan;
	wpa_s->conf->ap_scan = ap_scan;

	if (old_ap_scan != wpa_s->conf->ap_scan)
		wpas_notify_ap_scan_changed(wpa_s);

	return 0;
}


/**
 * wpa_supplicant_set_bss_expiration_age - Set BSS entry expiration age
 * @wpa_s: wpa_supplicant structure for a network interface
 * @expire_age: Expiration age in seconds
 * Returns: 0 if succeed or -1 if expire_age has an invalid value
 *
 */
int wpa_supplicant_set_bss_expiration_age(struct wpa_supplicant *wpa_s,
					  unsigned int bss_expire_age)
{
	if (bss_expire_age < 10) {
		wpa_msg(wpa_s, MSG_ERROR, "Invalid bss expiration age %u",
			bss_expire_age);
		return -1;
	}
	wpa_msg(wpa_s, MSG_DEBUG, "Setting bss expiration age: %d sec",
		bss_expire_age);
	wpa_s->conf->bss_expiration_age = bss_expire_age;

	return 0;
}


/**
 * wpa_supplicant_set_bss_expiration_count - Set BSS entry expiration scan count
 * @wpa_s: wpa_supplicant structure for a network interface
 * @expire_count: number of scans after which an unseen BSS is reclaimed
 * Returns: 0 if succeed or -1 if expire_count has an invalid value
 *
 */
int wpa_supplicant_set_bss_expiration_count(struct wpa_supplicant *wpa_s,
					    unsigned int bss_expire_count)
{
	if (bss_expire_count < 1) {
		wpa_msg(wpa_s, MSG_ERROR, "Invalid bss expiration count %u",
			bss_expire_count);
		return -1;
	}
	wpa_msg(wpa_s, MSG_DEBUG, "Setting bss expiration scan count: %u",
		bss_expire_count);
	wpa_s->conf->bss_expiration_scan_count = bss_expire_count;

	return 0;
}


/**
 * wpa_supplicant_set_scan_interval - Set scan interval
 * @wpa_s: wpa_supplicant structure for a network interface
 * @scan_interval: scan interval in seconds
 * Returns: 0 if succeed or -1 if scan_interval has an invalid value
 *
 */
int wpa_supplicant_set_scan_interval(struct wpa_supplicant *wpa_s,
				     int scan_interval)
{
	if (scan_interval < 0) {
		wpa_msg(wpa_s, MSG_ERROR, "Invalid scan interval %d",
			scan_interval);
		return -1;
	}
	wpa_msg(wpa_s, MSG_DEBUG, "Setting scan interval: %d sec",
		scan_interval);
	wpa_s->scan_interval = scan_interval;

	return 0;
}


/**
 * wpa_supplicant_set_debug_params - Set global debug params
 * @global: wpa_global structure
 * @debug_level: debug level
 * @debug_timestamp: determines if show timestamp in debug data
 * @debug_show_keys: determines if show keys in debug data
 * Returns: 0 if succeed or -1 if debug_level has wrong value
 */
int wpa_supplicant_set_debug_params(struct wpa_global *global, int debug_level,
				    int debug_timestamp, int debug_show_keys)
{

	int old_level, old_timestamp, old_show_keys;

	/* check for allowed debuglevels */
	if (debug_level != MSG_EXCESSIVE &&
	    debug_level != MSG_MSGDUMP &&
	    debug_level != MSG_DEBUG &&
	    debug_level != MSG_INFO &&
	    debug_level != MSG_WARNING &&
	    debug_level != MSG_ERROR)
		return -1;

	old_level = wpa_debug_level;
	old_timestamp = wpa_debug_timestamp;
	old_show_keys = wpa_debug_show_keys;

	wpa_debug_level = debug_level;
	wpa_debug_timestamp = debug_timestamp ? 1 : 0;
	wpa_debug_show_keys = debug_show_keys ? 1 : 0;

	if (wpa_debug_level != old_level)
		wpas_notify_debug_level_changed(global);
	if (wpa_debug_timestamp != old_timestamp)
		wpas_notify_debug_timestamp_changed(global);
	if (wpa_debug_show_keys != old_show_keys)
		wpas_notify_debug_show_keys_changed(global);

	return 0;
}


/**
 * wpa_supplicant_get_ssid - Get a pointer to the current network structure
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: A pointer to the current network structure or %NULL on failure
 */
struct wpa_ssid * wpa_supplicant_get_ssid(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *entry;
	u8 ssid[MAX_SSID_LEN];
	int res;
	size_t ssid_len;
	u8 bssid[ETH_ALEN];
	int wired;

	res = wpa_drv_get_ssid(wpa_s, ssid);
	if (res < 0) {
		wpa_msg(wpa_s, MSG_WARNING, "Could not read SSID from "
			"driver");
		return NULL;
	}
	ssid_len = res;

	if (wpa_drv_get_bssid(wpa_s, bssid) < 0) {
		wpa_msg(wpa_s, MSG_WARNING, "Could not read BSSID from "
			"driver");
		return NULL;
	}

	wired = wpa_s->conf->ap_scan == 0 &&
		(wpa_s->drv_flags & WPA_DRIVER_FLAGS_WIRED);

	entry = wpa_s->conf->ssid;
	while (entry) {
		if (!wpas_network_disabled(wpa_s, entry) &&
		    ((ssid_len == entry->ssid_len &&
		      os_memcmp(ssid, entry->ssid, ssid_len) == 0) || wired) &&
		    (!entry->bssid_set ||
		     os_memcmp(bssid, entry->bssid, ETH_ALEN) == 0))
			return entry;
#ifdef CONFIG_WPS
		if (!wpas_network_disabled(wpa_s, entry) &&
		    (entry->key_mgmt & WPA_KEY_MGMT_WPS) &&
		    (entry->ssid == NULL || entry->ssid_len == 0) &&
		    (!entry->bssid_set ||
		     os_memcmp(bssid, entry->bssid, ETH_ALEN) == 0))
			return entry;
#endif /* CONFIG_WPS */

		if (!wpas_network_disabled(wpa_s, entry) && entry->bssid_set &&
		    entry->ssid_len == 0 &&
		    os_memcmp(bssid, entry->bssid, ETH_ALEN) == 0)
			return entry;

		entry = entry->next;
	}

	return NULL;
}


static int select_driver(struct wpa_supplicant *wpa_s, int i)
{
	struct wpa_global *global = wpa_s->global;

	if (wpa_drivers[i]->global_init && global->drv_priv[i] == NULL) {
		global->drv_priv[i] = wpa_drivers[i]->global_init();
		if (global->drv_priv[i] == NULL) {
			wpa_printf(MSG_ERROR, "Failed to initialize driver "
				   "'%s'", wpa_drivers[i]->name);
			return -1;
		}
	}

	wpa_s->driver = wpa_drivers[i];
	wpa_s->global_drv_priv = global->drv_priv[i];

	return 0;
}


static int wpa_supplicant_set_driver(struct wpa_supplicant *wpa_s,
				     const char *name)
{
	int i;
	size_t len;
	const char *pos, *driver = name;

	if (wpa_s == NULL)
		return -1;

	if (wpa_drivers[0] == NULL) {
		wpa_msg(wpa_s, MSG_ERROR, "No driver interfaces build into "
			"wpa_supplicant");
		return -1;
	}

	if (name == NULL) {
		/* default to first driver in the list */
		return select_driver(wpa_s, 0);
	}

	do {
		pos = os_strchr(driver, ',');
		if (pos)
			len = pos - driver;
		else
			len = os_strlen(driver);

		for (i = 0; wpa_drivers[i]; i++) {
			if (os_strlen(wpa_drivers[i]->name) == len &&
			    os_strncmp(driver, wpa_drivers[i]->name, len) ==
			    0) {
				/* First driver that succeeds wins */
				if (select_driver(wpa_s, i) == 0)
					return 0;
			}
		}

		driver = pos + 1;
	} while (pos);

	wpa_msg(wpa_s, MSG_ERROR, "Unsupported driver '%s'", name);
	return -1;
}


/**
 * wpa_supplicant_rx_eapol - Deliver a received EAPOL frame to wpa_supplicant
 * @ctx: Context pointer (wpa_s); this is the ctx variable registered
 *	with struct wpa_driver_ops::init()
 * @src_addr: Source address of the EAPOL frame
 * @buf: EAPOL data starting from the EAPOL header (i.e., no Ethernet header)
 * @len: Length of the EAPOL data
 *
 * This function is called for each received EAPOL frame. Most driver
 * interfaces rely on more generic OS mechanism for receiving frames through
 * l2_packet, but if such a mechanism is not available, the driver wrapper may
 * take care of received EAPOL frames and deliver them to the core supplicant
 * code by calling this function.
 */
void wpa_supplicant_rx_eapol(void *ctx, const u8 *src_addr,
			     const u8 *buf, size_t len)
{
	struct wpa_supplicant *wpa_s = ctx;

	wpa_dbg(wpa_s, MSG_DEBUG, "RX EAPOL from " MACSTR, MAC2STR(src_addr));
	wpa_hexdump(MSG_MSGDUMP, "RX EAPOL", buf, len);

	if (wpa_s->wpa_state < WPA_ASSOCIATED ||
	    (wpa_s->last_eapol_matches_bssid &&
#ifdef CONFIG_AP
	     !wpa_s->ap_iface &&
#endif /* CONFIG_AP */
	     os_memcmp(src_addr, wpa_s->bssid, ETH_ALEN) != 0)) {
		/*
		 * There is possible race condition between receiving the
		 * association event and the EAPOL frame since they are coming
		 * through different paths from the driver. In order to avoid
		 * issues in trying to process the EAPOL frame before receiving
		 * association information, lets queue it for processing until
		 * the association event is received. This may also be needed in
		 * driver-based roaming case, so also use src_addr != BSSID as a
		 * trigger if we have previously confirmed that the
		 * Authenticator uses BSSID as the src_addr (which is not the
		 * case with wired IEEE 802.1X).
		 */
		wpa_dbg(wpa_s, MSG_DEBUG, "Not associated - Delay processing "
			"of received EAPOL frame (state=%s bssid=" MACSTR ")",
			wpa_supplicant_state_txt(wpa_s->wpa_state),
			MAC2STR(wpa_s->bssid));
		wpabuf_free(wpa_s->pending_eapol_rx);
		wpa_s->pending_eapol_rx = wpabuf_alloc_copy(buf, len);
		if (wpa_s->pending_eapol_rx) {
			os_get_time(&wpa_s->pending_eapol_rx_time);
			os_memcpy(wpa_s->pending_eapol_rx_src, src_addr,
				  ETH_ALEN);
		}
		return;
	}

	wpa_s->last_eapol_matches_bssid =
		os_memcmp(src_addr, wpa_s->bssid, ETH_ALEN) == 0;

#ifdef CONFIG_AP
	if (wpa_s->ap_iface) {
		wpa_supplicant_ap_rx_eapol(wpa_s, src_addr, buf, len);
		return;
	}
#endif /* CONFIG_AP */

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_NONE) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Ignored received EAPOL frame since "
			"no key management is configured");
		return;
	}

	if (wpa_s->eapol_received == 0 &&
	    (!(wpa_s->drv_flags & WPA_DRIVER_FLAGS_4WAY_HANDSHAKE) ||
	     !wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt) ||
	     wpa_s->wpa_state != WPA_COMPLETED) &&
	    (wpa_s->current_ssid == NULL ||
	     wpa_s->current_ssid->mode != IEEE80211_MODE_IBSS)) {
		/* Timeout for completing IEEE 802.1X and WPA authentication */
		wpa_supplicant_req_auth_timeout(
			wpa_s,
			(wpa_key_mgmt_wpa_ieee8021x(wpa_s->key_mgmt) ||
			 wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA ||
			 wpa_s->key_mgmt == WPA_KEY_MGMT_WPS) ?
			70 : 10, 0);
	}
	wpa_s->eapol_received++;

	if (wpa_s->countermeasures) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: Countermeasures - dropped "
			"EAPOL packet");
		return;
	}

#ifdef CONFIG_IBSS_RSN
	if (wpa_s->current_ssid &&
	    wpa_s->current_ssid->mode == WPAS_MODE_IBSS) {
		ibss_rsn_rx_eapol(wpa_s->ibss_rsn, src_addr, buf, len);
		return;
	}
#endif /* CONFIG_IBSS_RSN */

	/* Source address of the incoming EAPOL frame could be compared to the
	 * current BSSID. However, it is possible that a centralized
	 * Authenticator could be using another MAC address than the BSSID of
	 * an AP, so just allow any address to be used for now. The replies are
	 * still sent to the current BSSID (if available), though. */

	os_memcpy(wpa_s->last_eapol_src, src_addr, ETH_ALEN);
	if (!wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt) &&
	    eapol_sm_rx_eapol(wpa_s->eapol, src_addr, buf, len) > 0)
		return;
	wpa_drv_poll(wpa_s);
	if (!(wpa_s->drv_flags & WPA_DRIVER_FLAGS_4WAY_HANDSHAKE))
		wpa_sm_rx_eapol(wpa_s->wpa, src_addr, buf, len);
	else if (wpa_key_mgmt_wpa_ieee8021x(wpa_s->key_mgmt)) {
		/*
		 * Set portValid = TRUE here since we are going to skip 4-way
		 * handshake processing which would normally set portValid. We
		 * need this to allow the EAPOL state machines to be completed
		 * without going through EAPOL-Key handshake.
		 */
		eapol_sm_notify_portValid(wpa_s->eapol, TRUE);
	}
}


int wpa_supplicant_update_mac_addr(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->driver->send_eapol) {
		const u8 *addr = wpa_drv_get_mac_addr(wpa_s);
		if (addr)
			os_memcpy(wpa_s->own_addr, addr, ETH_ALEN);
	} else if (!(wpa_s->drv_flags &
		     WPA_DRIVER_FLAGS_P2P_DEDICATED_INTERFACE)) {
		l2_packet_deinit(wpa_s->l2);
		wpa_s->l2 = l2_packet_init(wpa_s->ifname,
					   wpa_drv_get_mac_addr(wpa_s),
					   ETH_P_EAPOL,
					   wpa_supplicant_rx_eapol, wpa_s, 0);
		if (wpa_s->l2 == NULL)
			return -1;
	} else {
		const u8 *addr = wpa_drv_get_mac_addr(wpa_s);
		if (addr)
			os_memcpy(wpa_s->own_addr, addr, ETH_ALEN);
	}

	if (wpa_s->l2 && l2_packet_get_own_addr(wpa_s->l2, wpa_s->own_addr)) {
		wpa_msg(wpa_s, MSG_ERROR, "Failed to get own L2 address");
		return -1;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "Own MAC address: " MACSTR,
		MAC2STR(wpa_s->own_addr));
	wpa_sm_set_own_addr(wpa_s->wpa, wpa_s->own_addr);

	return 0;
}


static void wpa_supplicant_rx_eapol_bridge(void *ctx, const u8 *src_addr,
					   const u8 *buf, size_t len)
{
	struct wpa_supplicant *wpa_s = ctx;
	const struct l2_ethhdr *eth;

	if (len < sizeof(*eth))
		return;
	eth = (const struct l2_ethhdr *) buf;

	if (os_memcmp(eth->h_dest, wpa_s->own_addr, ETH_ALEN) != 0 &&
	    !(eth->h_dest[0] & 0x01)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "RX EAPOL from " MACSTR " to " MACSTR
			" (bridge - not for this interface - ignore)",
			MAC2STR(src_addr), MAC2STR(eth->h_dest));
		return;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "RX EAPOL from " MACSTR " to " MACSTR
		" (bridge)", MAC2STR(src_addr), MAC2STR(eth->h_dest));
	wpa_supplicant_rx_eapol(wpa_s, src_addr, buf + sizeof(*eth),
				len - sizeof(*eth));
}


/**
 * wpa_supplicant_driver_init - Initialize driver interface parameters
 * @wpa_s: Pointer to wpa_supplicant data
 * Returns: 0 on success, -1 on failure
 *
 * This function is called to initialize driver interface parameters.
 * wpa_drv_init() must have been called before this function to initialize the
 * driver interface.
 */
int wpa_supplicant_driver_init(struct wpa_supplicant *wpa_s)
{
	static int interface_count = 0;

	if (wpa_supplicant_update_mac_addr(wpa_s) < 0)
		return -1;

	if (wpa_s->bridge_ifname[0]) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Receiving packets from bridge "
			"interface '%s'", wpa_s->bridge_ifname);
		wpa_s->l2_br = l2_packet_init(wpa_s->bridge_ifname,
					      wpa_s->own_addr,
					      ETH_P_EAPOL,
					      wpa_supplicant_rx_eapol_bridge,
					      wpa_s, 1);
		if (wpa_s->l2_br == NULL) {
			wpa_msg(wpa_s, MSG_ERROR, "Failed to open l2_packet "
				"connection for the bridge interface '%s'",
				wpa_s->bridge_ifname);
			return -1;
		}
	}

	wpa_clear_keys(wpa_s, NULL);

	/* Make sure that TKIP countermeasures are not left enabled (could
	 * happen if wpa_supplicant is killed during countermeasures. */
	wpa_drv_set_countermeasures(wpa_s, 0);

	wpa_dbg(wpa_s, MSG_DEBUG, "RSN: flushing PMKID list in the driver");
	wpa_drv_flush_pmkid(wpa_s);

	wpa_s->prev_scan_ssid = WILDCARD_SSID_SCAN;
	wpa_s->prev_scan_wildcard = 0;

	if (wpa_supplicant_enabled_networks(wpa_s)) {
		if (wpa_supplicant_delayed_sched_scan(wpa_s, interface_count,
						      100000))
			wpa_supplicant_req_scan(wpa_s, interface_count,
						100000);
		interface_count++;
	} else
		wpa_supplicant_set_state(wpa_s, WPA_INACTIVE);

	return 0;
}


static int wpa_supplicant_daemon(const char *pid_file)
{
	wpa_printf(MSG_DEBUG, "Daemonize..");
	return os_daemonize(pid_file);
}


static struct wpa_supplicant * wpa_supplicant_alloc(void)
{
	struct wpa_supplicant *wpa_s;

	wpa_s = os_zalloc(sizeof(*wpa_s));
	if (wpa_s == NULL)
		return NULL;
	wpa_s->scan_req = INITIAL_SCAN_REQ;
	wpa_s->scan_interval = 5;
	wpa_s->new_connection = 1;
	wpa_s->parent = wpa_s;
	wpa_s->sched_scanning = 0;

	return wpa_s;
}


#ifdef CONFIG_HT_OVERRIDES

static int wpa_set_htcap_mcs(struct wpa_supplicant *wpa_s,
			     struct ieee80211_ht_capabilities *htcaps,
			     struct ieee80211_ht_capabilities *htcaps_mask,
			     const char *ht_mcs)
{
	/* parse ht_mcs into hex array */
	int i;
	const char *tmp = ht_mcs;
	char *end = NULL;

	/* If ht_mcs is null, do not set anything */
	if (!ht_mcs)
		return 0;

	/* This is what we are setting in the kernel */
	os_memset(&htcaps->supported_mcs_set, 0, IEEE80211_HT_MCS_MASK_LEN);

	wpa_msg(wpa_s, MSG_DEBUG, "set_htcap, ht_mcs -:%s:-", ht_mcs);

	for (i = 0; i < IEEE80211_HT_MCS_MASK_LEN; i++) {
		errno = 0;
		long v = strtol(tmp, &end, 16);
		if (errno == 0) {
			wpa_msg(wpa_s, MSG_DEBUG,
				"htcap value[%i]: %ld end: %p  tmp: %p",
				i, v, end, tmp);
			if (end == tmp)
				break;

			htcaps->supported_mcs_set[i] = v;
			tmp = end;
		} else {
			wpa_msg(wpa_s, MSG_ERROR,
				"Failed to parse ht-mcs: %s, error: %s\n",
				ht_mcs, strerror(errno));
			return -1;
		}
	}

	/*
	 * If we were able to parse any values, then set mask for the MCS set.
	 */
	if (i) {
		os_memset(&htcaps_mask->supported_mcs_set, 0xff,
			  IEEE80211_HT_MCS_MASK_LEN - 1);
		/* skip the 3 reserved bits */
		htcaps_mask->supported_mcs_set[IEEE80211_HT_MCS_MASK_LEN - 1] =
			0x1f;
	}

	return 0;
}


static int wpa_disable_max_amsdu(struct wpa_supplicant *wpa_s,
				 struct ieee80211_ht_capabilities *htcaps,
				 struct ieee80211_ht_capabilities *htcaps_mask,
				 int disabled)
{
	u16 msk;

	wpa_msg(wpa_s, MSG_DEBUG, "set_disable_max_amsdu: %d", disabled);

	if (disabled == -1)
		return 0;

	msk = host_to_le16(HT_CAP_INFO_MAX_AMSDU_SIZE);
	htcaps_mask->ht_capabilities_info |= msk;
	if (disabled)
		htcaps->ht_capabilities_info &= msk;
	else
		htcaps->ht_capabilities_info |= msk;

	return 0;
}


static int wpa_set_ampdu_factor(struct wpa_supplicant *wpa_s,
				struct ieee80211_ht_capabilities *htcaps,
				struct ieee80211_ht_capabilities *htcaps_mask,
				int factor)
{
	wpa_msg(wpa_s, MSG_DEBUG, "set_ampdu_factor: %d", factor);

	if (factor == -1)
		return 0;

	if (factor < 0 || factor > 3) {
		wpa_msg(wpa_s, MSG_ERROR, "ampdu_factor: %d out of range. "
			"Must be 0-3 or -1", factor);
		return -EINVAL;
	}

	htcaps_mask->a_mpdu_params |= 0x3; /* 2 bits for factor */
	htcaps->a_mpdu_params &= ~0x3;
	htcaps->a_mpdu_params |= factor & 0x3;

	return 0;
}


static int wpa_set_ampdu_density(struct wpa_supplicant *wpa_s,
				 struct ieee80211_ht_capabilities *htcaps,
				 struct ieee80211_ht_capabilities *htcaps_mask,
				 int density)
{
	wpa_msg(wpa_s, MSG_DEBUG, "set_ampdu_density: %d", density);

	if (density == -1)
		return 0;

	if (density < 0 || density > 7) {
		wpa_msg(wpa_s, MSG_ERROR,
			"ampdu_density: %d out of range. Must be 0-7 or -1.",
			density);
		return -EINVAL;
	}

	htcaps_mask->a_mpdu_params |= 0x1C;
	htcaps->a_mpdu_params &= ~(0x1C);
	htcaps->a_mpdu_params |= (density << 2) & 0x1C;

	return 0;
}


static int wpa_set_disable_ht40(struct wpa_supplicant *wpa_s,
				struct ieee80211_ht_capabilities *htcaps,
				struct ieee80211_ht_capabilities *htcaps_mask,
				int disabled)
{
	/* Masking these out disables HT40 */
	u16 msk = host_to_le16(HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET |
			       HT_CAP_INFO_SHORT_GI40MHZ);

	wpa_msg(wpa_s, MSG_DEBUG, "set_disable_ht40: %d", disabled);

	if (disabled)
		htcaps->ht_capabilities_info &= ~msk;
	else
		htcaps->ht_capabilities_info |= msk;

	htcaps_mask->ht_capabilities_info |= msk;

	return 0;
}


static int wpa_set_disable_sgi(struct wpa_supplicant *wpa_s,
			       struct ieee80211_ht_capabilities *htcaps,
			       struct ieee80211_ht_capabilities *htcaps_mask,
			       int disabled)
{
	/* Masking these out disables SGI */
	u16 msk = host_to_le16(HT_CAP_INFO_SHORT_GI20MHZ |
			       HT_CAP_INFO_SHORT_GI40MHZ);

	wpa_msg(wpa_s, MSG_DEBUG, "set_disable_sgi: %d", disabled);

	if (disabled)
		htcaps->ht_capabilities_info &= ~msk;
	else
		htcaps->ht_capabilities_info |= msk;

	htcaps_mask->ht_capabilities_info |= msk;

	return 0;
}


void wpa_supplicant_apply_ht_overrides(
	struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid,
	struct wpa_driver_associate_params *params)
{
	struct ieee80211_ht_capabilities *htcaps;
	struct ieee80211_ht_capabilities *htcaps_mask;

	if (!ssid)
		return;

	params->disable_ht = ssid->disable_ht;
	if (!params->htcaps || !params->htcaps_mask)
		return;

	htcaps = (struct ieee80211_ht_capabilities *) params->htcaps;
	htcaps_mask = (struct ieee80211_ht_capabilities *) params->htcaps_mask;
	wpa_set_htcap_mcs(wpa_s, htcaps, htcaps_mask, ssid->ht_mcs);
	wpa_disable_max_amsdu(wpa_s, htcaps, htcaps_mask,
			      ssid->disable_max_amsdu);
	wpa_set_ampdu_factor(wpa_s, htcaps, htcaps_mask, ssid->ampdu_factor);
	wpa_set_ampdu_density(wpa_s, htcaps, htcaps_mask, ssid->ampdu_density);
	wpa_set_disable_ht40(wpa_s, htcaps, htcaps_mask, ssid->disable_ht40);
	wpa_set_disable_sgi(wpa_s, htcaps, htcaps_mask, ssid->disable_sgi);
}

#endif /* CONFIG_HT_OVERRIDES */


static int pcsc_reader_init(struct wpa_supplicant *wpa_s)
{
#ifdef PCSC_FUNCS
	size_t len;

	if (!wpa_s->conf->pcsc_reader)
		return 0;

	wpa_s->scard = scard_init(SCARD_TRY_BOTH, wpa_s->conf->pcsc_reader);
	if (!wpa_s->scard)
		return 1;

	if (wpa_s->conf->pcsc_pin &&
	    scard_set_pin(wpa_s->scard, wpa_s->conf->pcsc_pin) < 0) {
		scard_deinit(wpa_s->scard);
		wpa_s->scard = NULL;
		wpa_msg(wpa_s, MSG_ERROR, "PC/SC PIN validation failed");
		return -1;
	}

	len = sizeof(wpa_s->imsi) - 1;
	if (scard_get_imsi(wpa_s->scard, wpa_s->imsi, &len)) {
		scard_deinit(wpa_s->scard);
		wpa_s->scard = NULL;
		wpa_msg(wpa_s, MSG_ERROR, "Could not read IMSI");
		return -1;
	}
	wpa_s->imsi[len] = '\0';

	wpa_s->mnc_len = scard_get_mnc_len(wpa_s->scard);

	wpa_printf(MSG_DEBUG, "SCARD: IMSI %s (MNC length %d)",
		   wpa_s->imsi, wpa_s->mnc_len);

	wpa_sm_set_scard_ctx(wpa_s->wpa, wpa_s->scard);
	eapol_sm_register_scard_ctx(wpa_s->eapol, wpa_s->scard);
#endif /* PCSC_FUNCS */

	return 0;
}


int wpas_init_ext_pw(struct wpa_supplicant *wpa_s)
{
	char *val, *pos;

	ext_password_deinit(wpa_s->ext_pw);
	wpa_s->ext_pw = NULL;
	eapol_sm_set_ext_pw_ctx(wpa_s->eapol, NULL);

	if (!wpa_s->conf->ext_password_backend)
		return 0;

	val = os_strdup(wpa_s->conf->ext_password_backend);
	if (val == NULL)
		return -1;
	pos = os_strchr(val, ':');
	if (pos)
		*pos++ = '\0';

	wpa_printf(MSG_DEBUG, "EXT PW: Initialize backend '%s'", val);

	wpa_s->ext_pw = ext_password_init(val, pos);
	os_free(val);
	if (wpa_s->ext_pw == NULL) {
		wpa_printf(MSG_DEBUG, "EXT PW: Failed to initialize backend");
		return -1;
	}
	eapol_sm_set_ext_pw_ctx(wpa_s->eapol, wpa_s->ext_pw);

	return 0;
}


static int wpa_supplicant_init_iface(struct wpa_supplicant *wpa_s,
				     struct wpa_interface *iface)
{
	const char *ifname, *driver;
	struct wpa_driver_capa capa;

	wpa_printf(MSG_DEBUG, "Initializing interface '%s' conf '%s' driver "
		   "'%s' ctrl_interface '%s' bridge '%s'", iface->ifname,
		   iface->confname ? iface->confname : "N/A",
		   iface->driver ? iface->driver : "default",
		   iface->ctrl_interface ? iface->ctrl_interface : "N/A",
		   iface->bridge_ifname ? iface->bridge_ifname : "N/A");

	if (iface->confname) {
#ifdef CONFIG_BACKEND_FILE
		wpa_s->confname = os_rel2abs_path(iface->confname);
		if (wpa_s->confname == NULL) {
			wpa_printf(MSG_ERROR, "Failed to get absolute path "
				   "for configuration file '%s'.",
				   iface->confname);
			return -1;
		}
		wpa_printf(MSG_DEBUG, "Configuration file '%s' -> '%s'",
			   iface->confname, wpa_s->confname);
#else /* CONFIG_BACKEND_FILE */
		wpa_s->confname = os_strdup(iface->confname);
#endif /* CONFIG_BACKEND_FILE */
		wpa_s->conf = wpa_config_read(wpa_s->confname);
		if (wpa_s->conf == NULL) {
			wpa_printf(MSG_ERROR, "Failed to read or parse "
				   "configuration '%s'.", wpa_s->confname);
			return -1;
		}

		/*
		 * Override ctrl_interface and driver_param if set on command
		 * line.
		 */
		if (iface->ctrl_interface) {
			os_free(wpa_s->conf->ctrl_interface);
			wpa_s->conf->ctrl_interface =
				os_strdup(iface->ctrl_interface);
		}

		if (iface->driver_param) {
			os_free(wpa_s->conf->driver_param);
			wpa_s->conf->driver_param =
				os_strdup(iface->driver_param);
		}
	} else
		wpa_s->conf = wpa_config_alloc_empty(iface->ctrl_interface,
						     iface->driver_param);

	if (wpa_s->conf == NULL) {
		wpa_printf(MSG_ERROR, "\nNo configuration found.");
		return -1;
	}

	if (iface->ifname == NULL) {
		wpa_printf(MSG_ERROR, "\nInterface name is required.");
		return -1;
	}
	if (os_strlen(iface->ifname) >= sizeof(wpa_s->ifname)) {
		wpa_printf(MSG_ERROR, "\nToo long interface name '%s'.",
			   iface->ifname);
		return -1;
	}
	os_strlcpy(wpa_s->ifname, iface->ifname, sizeof(wpa_s->ifname));

	if (iface->bridge_ifname) {
		if (os_strlen(iface->bridge_ifname) >=
		    sizeof(wpa_s->bridge_ifname)) {
			wpa_printf(MSG_ERROR, "\nToo long bridge interface "
				   "name '%s'.", iface->bridge_ifname);
			return -1;
		}
		os_strlcpy(wpa_s->bridge_ifname, iface->bridge_ifname,
			   sizeof(wpa_s->bridge_ifname));
	}

	/* RSNA Supplicant Key Management - INITIALIZE */
	eapol_sm_notify_portEnabled(wpa_s->eapol, FALSE);
	eapol_sm_notify_portValid(wpa_s->eapol, FALSE);

	/* Initialize driver interface and register driver event handler before
	 * L2 receive handler so that association events are processed before
	 * EAPOL-Key packets if both become available for the same select()
	 * call. */
	driver = iface->driver;
next_driver:
	if (wpa_supplicant_set_driver(wpa_s, driver) < 0)
		return -1;

	wpa_s->drv_priv = wpa_drv_init(wpa_s, wpa_s->ifname);
	if (wpa_s->drv_priv == NULL) {
		const char *pos;
		pos = driver ? os_strchr(driver, ',') : NULL;
		if (pos) {
			wpa_dbg(wpa_s, MSG_DEBUG, "Failed to initialize "
				"driver interface - try next driver wrapper");
			driver = pos + 1;
			goto next_driver;
		}
		wpa_msg(wpa_s, MSG_ERROR, "Failed to initialize driver "
			"interface");
		return -1;
	}
	if (wpa_drv_set_param(wpa_s, wpa_s->conf->driver_param) < 0) {
		wpa_msg(wpa_s, MSG_ERROR, "Driver interface rejected "
			"driver_param '%s'", wpa_s->conf->driver_param);
		return -1;
	}

	ifname = wpa_drv_get_ifname(wpa_s);
	if (ifname && os_strcmp(ifname, wpa_s->ifname) != 0) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Driver interface replaced "
			"interface name with '%s'", ifname);
		os_strlcpy(wpa_s->ifname, ifname, sizeof(wpa_s->ifname));
	}

	if (wpa_supplicant_init_wpa(wpa_s) < 0)
		return -1;

	wpa_sm_set_ifname(wpa_s->wpa, wpa_s->ifname,
			  wpa_s->bridge_ifname[0] ? wpa_s->bridge_ifname :
			  NULL);
	wpa_sm_set_fast_reauth(wpa_s->wpa, wpa_s->conf->fast_reauth);

	if (wpa_s->conf->dot11RSNAConfigPMKLifetime &&
	    wpa_sm_set_param(wpa_s->wpa, RSNA_PMK_LIFETIME,
			     wpa_s->conf->dot11RSNAConfigPMKLifetime)) {
		wpa_msg(wpa_s, MSG_ERROR, "Invalid WPA parameter value for "
			"dot11RSNAConfigPMKLifetime");
		return -1;
	}

	if (wpa_s->conf->dot11RSNAConfigPMKReauthThreshold &&
	    wpa_sm_set_param(wpa_s->wpa, RSNA_PMK_REAUTH_THRESHOLD,
			     wpa_s->conf->dot11RSNAConfigPMKReauthThreshold)) {
		wpa_msg(wpa_s, MSG_ERROR, "Invalid WPA parameter value for "
			"dot11RSNAConfigPMKReauthThreshold");
		return -1;
	}

	if (wpa_s->conf->dot11RSNAConfigSATimeout &&
	    wpa_sm_set_param(wpa_s->wpa, RSNA_SA_TIMEOUT,
			     wpa_s->conf->dot11RSNAConfigSATimeout)) {
		wpa_msg(wpa_s, MSG_ERROR, "Invalid WPA parameter value for "
			"dot11RSNAConfigSATimeout");
		return -1;
	}

	wpa_s->hw.modes = wpa_drv_get_hw_feature_data(wpa_s,
						      &wpa_s->hw.num_modes,
						      &wpa_s->hw.flags);

	if (wpa_drv_get_capa(wpa_s, &capa) == 0) {
		wpa_s->drv_capa_known = 1;
		wpa_s->drv_flags = capa.flags;
		wpa_s->drv_enc = capa.enc;
		wpa_s->probe_resp_offloads = capa.probe_resp_offloads;
		wpa_s->max_scan_ssids = capa.max_scan_ssids;
		wpa_s->max_sched_scan_ssids = capa.max_sched_scan_ssids;
		wpa_s->sched_scan_supported = capa.sched_scan_supported;
		wpa_s->max_match_sets = capa.max_match_sets;
		wpa_s->max_remain_on_chan = capa.max_remain_on_chan;
		wpa_s->max_stations = capa.max_stations;
	}
	if (wpa_s->max_remain_on_chan == 0)
		wpa_s->max_remain_on_chan = 1000;

	if (wpa_supplicant_driver_init(wpa_s) < 0)
		return -1;

#ifdef CONFIG_TDLS
	if (wpa_tdls_init(wpa_s->wpa))
		return -1;
#endif /* CONFIG_TDLS */

	if (wpa_s->conf->country[0] && wpa_s->conf->country[1] &&
	    wpa_drv_set_country(wpa_s, wpa_s->conf->country)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Failed to set country");
		return -1;
	}

	if (wpas_wps_init(wpa_s))
		return -1;

	if (wpa_supplicant_init_eapol(wpa_s) < 0)
		return -1;
	wpa_sm_set_eapol(wpa_s->wpa, wpa_s->eapol);

	wpa_s->ctrl_iface = wpa_supplicant_ctrl_iface_init(wpa_s);
	if (wpa_s->ctrl_iface == NULL) {
		wpa_printf(MSG_ERROR,
			   "Failed to initialize control interface '%s'.\n"
			   "You may have another wpa_supplicant process "
			   "already running or the file was\n"
			   "left by an unclean termination of wpa_supplicant "
			   "in which case you will need\n"
			   "to manually remove this file before starting "
			   "wpa_supplicant again.\n",
			   wpa_s->conf->ctrl_interface);
		return -1;
	}

	wpa_s->gas = gas_query_init(wpa_s);
	if (wpa_s->gas == NULL) {
		wpa_printf(MSG_ERROR, "Failed to initialize GAS query");
		return -1;
	}

#ifdef CONFIG_P2P
	if (wpas_p2p_init(wpa_s->global, wpa_s) < 0) {
		wpa_msg(wpa_s, MSG_ERROR, "Failed to init P2P");
		return -1;
	}
#endif /* CONFIG_P2P */

	if (wpa_bss_init(wpa_s) < 0)
		return -1;

	if (pcsc_reader_init(wpa_s) < 0)
		return -1;

	if (wpas_init_ext_pw(wpa_s) < 0)
		return -1;

	return 0;
}


static void wpa_supplicant_deinit_iface(struct wpa_supplicant *wpa_s,
					int notify, int terminate)
{
	if (wpa_s->drv_priv) {
		wpa_supplicant_deauthenticate(wpa_s,
					      WLAN_REASON_DEAUTH_LEAVING);

		wpa_drv_set_countermeasures(wpa_s, 0);
		wpa_clear_keys(wpa_s, NULL);
	}

	wpa_supplicant_cleanup(wpa_s);

#ifdef CONFIG_P2P
	if (wpa_s == wpa_s->global->p2p_init_wpa_s && wpa_s->global->p2p) {
		wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Disable P2P since removing "
			"the management interface is being removed");
		wpas_p2p_deinit_global(wpa_s->global);
	}
#endif /* CONFIG_P2P */

	if (wpa_s->drv_priv)
		wpa_drv_deinit(wpa_s);

	if (notify)
		wpas_notify_iface_removed(wpa_s);

	if (terminate)
		wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_TERMINATING);

	if (wpa_s->ctrl_iface) {
		wpa_supplicant_ctrl_iface_deinit(wpa_s->ctrl_iface);
		wpa_s->ctrl_iface = NULL;
	}

	if (wpa_s->conf != NULL) {
		wpa_config_free(wpa_s->conf);
		wpa_s->conf = NULL;
	}
}


/**
 * wpa_supplicant_add_iface - Add a new network interface
 * @global: Pointer to global data from wpa_supplicant_init()
 * @iface: Interface configuration options
 * Returns: Pointer to the created interface or %NULL on failure
 *
 * This function is used to add new network interfaces for %wpa_supplicant.
 * This can be called before wpa_supplicant_run() to add interfaces before the
 * main event loop has been started. In addition, new interfaces can be added
 * dynamically while %wpa_supplicant is already running. This could happen,
 * e.g., when a hotplug network adapter is inserted.
 */
struct wpa_supplicant * wpa_supplicant_add_iface(struct wpa_global *global,
						 struct wpa_interface *iface)
{
	struct wpa_supplicant *wpa_s;
	struct wpa_interface t_iface;
	struct wpa_ssid *ssid;

	if (global == NULL || iface == NULL)
		return NULL;

	wpa_s = wpa_supplicant_alloc();
	if (wpa_s == NULL)
		return NULL;

	wpa_s->global = global;

	t_iface = *iface;
	if (global->params.override_driver) {
		wpa_printf(MSG_DEBUG, "Override interface parameter: driver "
			   "('%s' -> '%s')",
			   iface->driver, global->params.override_driver);
		t_iface.driver = global->params.override_driver;
	}
	if (global->params.override_ctrl_interface) {
		wpa_printf(MSG_DEBUG, "Override interface parameter: "
			   "ctrl_interface ('%s' -> '%s')",
			   iface->ctrl_interface,
			   global->params.override_ctrl_interface);
		t_iface.ctrl_interface =
			global->params.override_ctrl_interface;
	}
	if (wpa_supplicant_init_iface(wpa_s, &t_iface)) {
		wpa_printf(MSG_DEBUG, "Failed to add interface %s",
			   iface->ifname);
		wpa_supplicant_deinit_iface(wpa_s, 0, 0);
		os_free(wpa_s);
		return NULL;
	}

	/* Notify the control interfaces about new iface */
	if (wpas_notify_iface_added(wpa_s)) {
		wpa_supplicant_deinit_iface(wpa_s, 1, 0);
		os_free(wpa_s);
		return NULL;
	}

	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next)
		wpas_notify_network_added(wpa_s, ssid);

	wpa_s->next = global->ifaces;
	global->ifaces = wpa_s;

	wpa_dbg(wpa_s, MSG_DEBUG, "Added interface %s", wpa_s->ifname);
	wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);

	return wpa_s;
}


/**
 * wpa_supplicant_remove_iface - Remove a network interface
 * @global: Pointer to global data from wpa_supplicant_init()
 * @wpa_s: Pointer to the network interface to be removed
 * Returns: 0 if interface was removed, -1 if interface was not found
 *
 * This function can be used to dynamically remove network interfaces from
 * %wpa_supplicant, e.g., when a hotplug network adapter is ejected. In
 * addition, this function is used to remove all remaining interfaces when
 * %wpa_supplicant is terminated.
 */
int wpa_supplicant_remove_iface(struct wpa_global *global,
				struct wpa_supplicant *wpa_s,
				int terminate)
{
	struct wpa_supplicant *prev;

	/* Remove interface from the global list of interfaces */
	prev = global->ifaces;
	if (prev == wpa_s) {
		global->ifaces = wpa_s->next;
	} else {
		while (prev && prev->next != wpa_s)
			prev = prev->next;
		if (prev == NULL)
			return -1;
		prev->next = wpa_s->next;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "Removing interface %s", wpa_s->ifname);

	if (global->p2p_group_formation == wpa_s)
		global->p2p_group_formation = NULL;
	wpa_supplicant_deinit_iface(wpa_s, 1, terminate);
	os_free(wpa_s);

	return 0;
}


/**
 * wpa_supplicant_get_eap_mode - Get the current EAP mode
 * @wpa_s: Pointer to the network interface
 * Returns: Pointer to the eap mode or the string "UNKNOWN" if not found
 */
const char * wpa_supplicant_get_eap_mode(struct wpa_supplicant *wpa_s)
{
	const char *eapol_method;

        if (wpa_key_mgmt_wpa_ieee8021x(wpa_s->key_mgmt) == 0 &&
            wpa_s->key_mgmt != WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
		return "NO-EAP";
	}

	eapol_method = eapol_sm_get_method_name(wpa_s->eapol);
	if (eapol_method == NULL)
		return "UNKNOWN-EAP";

	return eapol_method;
}


/**
 * wpa_supplicant_get_iface - Get a new network interface
 * @global: Pointer to global data from wpa_supplicant_init()
 * @ifname: Interface name
 * Returns: Pointer to the interface or %NULL if not found
 */
struct wpa_supplicant * wpa_supplicant_get_iface(struct wpa_global *global,
						 const char *ifname)
{
	struct wpa_supplicant *wpa_s;

	for (wpa_s = global->ifaces; wpa_s; wpa_s = wpa_s->next) {
		if (os_strcmp(wpa_s->ifname, ifname) == 0)
			return wpa_s;
	}
	return NULL;
}


#ifndef CONFIG_NO_WPA_MSG
static const char * wpa_supplicant_msg_ifname_cb(void *ctx)
{
	struct wpa_supplicant *wpa_s = ctx;
	if (wpa_s == NULL)
		return NULL;
	return wpa_s->ifname;
}
#endif /* CONFIG_NO_WPA_MSG */


/**
 * wpa_supplicant_init - Initialize %wpa_supplicant
 * @params: Parameters for %wpa_supplicant
 * Returns: Pointer to global %wpa_supplicant data, or %NULL on failure
 *
 * This function is used to initialize %wpa_supplicant. After successful
 * initialization, the returned data pointer can be used to add and remove
 * network interfaces, and eventually, to deinitialize %wpa_supplicant.
 */
struct wpa_global * wpa_supplicant_init(struct wpa_params *params)
{
	struct wpa_global *global;
	int ret, i;

	if (params == NULL)
		return NULL;

#ifdef CONFIG_DRIVER_NDIS
	{
		void driver_ndis_init_ops(void);
		driver_ndis_init_ops();
	}
#endif /* CONFIG_DRIVER_NDIS */

#ifndef CONFIG_NO_WPA_MSG
	wpa_msg_register_ifname_cb(wpa_supplicant_msg_ifname_cb);
#endif /* CONFIG_NO_WPA_MSG */

	wpa_debug_open_file(params->wpa_debug_file_path);
	if (params->wpa_debug_syslog)
		wpa_debug_open_syslog();
	if (params->wpa_debug_tracing) {
		ret = wpa_debug_open_linux_tracing();
		if (ret) {
			wpa_printf(MSG_ERROR,
				   "Failed to enable trace logging");
			return NULL;
		}
	}

	ret = eap_register_methods();
	if (ret) {
		wpa_printf(MSG_ERROR, "Failed to register EAP methods");
		if (ret == -2)
			wpa_printf(MSG_ERROR, "Two or more EAP methods used "
				   "the same EAP type.");
		return NULL;
	}

	global = os_zalloc(sizeof(*global));
	if (global == NULL)
		return NULL;
	dl_list_init(&global->p2p_srv_bonjour);
	dl_list_init(&global->p2p_srv_upnp);
	global->params.daemonize = params->daemonize;
	global->params.wait_for_monitor = params->wait_for_monitor;
	global->params.dbus_ctrl_interface = params->dbus_ctrl_interface;
	if (params->pid_file)
		global->params.pid_file = os_strdup(params->pid_file);
	if (params->ctrl_interface)
		global->params.ctrl_interface =
			os_strdup(params->ctrl_interface);
	if (params->override_driver)
		global->params.override_driver =
			os_strdup(params->override_driver);
	if (params->override_ctrl_interface)
		global->params.override_ctrl_interface =
			os_strdup(params->override_ctrl_interface);
	wpa_debug_level = global->params.wpa_debug_level =
		params->wpa_debug_level;
	wpa_debug_show_keys = global->params.wpa_debug_show_keys =
		params->wpa_debug_show_keys;
	wpa_debug_timestamp = global->params.wpa_debug_timestamp =
		params->wpa_debug_timestamp;

	wpa_printf(MSG_DEBUG, "wpa_supplicant v" VERSION_STR);

	if (eloop_init()) {
		wpa_printf(MSG_ERROR, "Failed to initialize event loop");
		wpa_supplicant_deinit(global);
		return NULL;
	}

	random_init(params->entropy_file);

	global->ctrl_iface = wpa_supplicant_global_ctrl_iface_init(global);
	if (global->ctrl_iface == NULL) {
		wpa_supplicant_deinit(global);
		return NULL;
	}

	if (wpas_notify_supplicant_initialized(global)) {
		wpa_supplicant_deinit(global);
		return NULL;
	}

	for (i = 0; wpa_drivers[i]; i++)
		global->drv_count++;
	if (global->drv_count == 0) {
		wpa_printf(MSG_ERROR, "No drivers enabled");
		wpa_supplicant_deinit(global);
		return NULL;
	}
	global->drv_priv = os_zalloc(global->drv_count * sizeof(void *));
	if (global->drv_priv == NULL) {
		wpa_supplicant_deinit(global);
		return NULL;
	}

#ifdef CONFIG_WIFI_DISPLAY
	if (wifi_display_init(global) < 0) {
		wpa_printf(MSG_ERROR, "Failed to initialize Wi-Fi Display");
		wpa_supplicant_deinit(global);
		return NULL;
	}
#endif /* CONFIG_WIFI_DISPLAY */

	return global;
}


/**
 * wpa_supplicant_run - Run the %wpa_supplicant main event loop
 * @global: Pointer to global data from wpa_supplicant_init()
 * Returns: 0 after successful event loop run, -1 on failure
 *
 * This function starts the main event loop and continues running as long as
 * there are any remaining events. In most cases, this function is running as
 * long as the %wpa_supplicant process in still in use.
 */
int wpa_supplicant_run(struct wpa_global *global)
{
	struct wpa_supplicant *wpa_s;

	if (global->params.daemonize &&
	    wpa_supplicant_daemon(global->params.pid_file))
		return -1;

	if (global->params.wait_for_monitor) {
		for (wpa_s = global->ifaces; wpa_s; wpa_s = wpa_s->next)
			if (wpa_s->ctrl_iface)
				wpa_supplicant_ctrl_iface_wait(
					wpa_s->ctrl_iface);
	}

	eloop_register_signal_terminate(wpa_supplicant_terminate, global);
	eloop_register_signal_reconfig(wpa_supplicant_reconfig, global);

	eloop_run();

	return 0;
}


/**
 * wpa_supplicant_deinit - Deinitialize %wpa_supplicant
 * @global: Pointer to global data from wpa_supplicant_init()
 *
 * This function is called to deinitialize %wpa_supplicant and to free all
 * allocated resources. Remaining network interfaces will also be removed.
 */
void wpa_supplicant_deinit(struct wpa_global *global)
{
	int i;

	if (global == NULL)
		return;

#ifdef CONFIG_WIFI_DISPLAY
	wifi_display_deinit(global);
#endif /* CONFIG_WIFI_DISPLAY */
#ifdef CONFIG_P2P
	wpas_p2p_deinit_global(global);
#endif /* CONFIG_P2P */

	while (global->ifaces)
		wpa_supplicant_remove_iface(global, global->ifaces, 1);

	if (global->ctrl_iface)
		wpa_supplicant_global_ctrl_iface_deinit(global->ctrl_iface);

	wpas_notify_supplicant_deinitialized(global);

	eap_peer_unregister_methods();
#ifdef CONFIG_AP
	eap_server_unregister_methods();
#endif /* CONFIG_AP */

	for (i = 0; wpa_drivers[i] && global->drv_priv; i++) {
		if (!global->drv_priv[i])
			continue;
		wpa_drivers[i]->global_deinit(global->drv_priv[i]);
	}
	os_free(global->drv_priv);

	random_deinit();

	eloop_destroy();

	if (global->params.pid_file) {
		os_daemonize_terminate(global->params.pid_file);
		os_free(global->params.pid_file);
	}
	os_free(global->params.ctrl_interface);
	os_free(global->params.override_driver);
	os_free(global->params.override_ctrl_interface);

	os_free(global->p2p_disallow_freq);

	os_free(global);
	wpa_debug_close_syslog();
	wpa_debug_close_file();
	wpa_debug_close_linux_tracing();
}


void wpa_supplicant_update_config(struct wpa_supplicant *wpa_s)
{
	if ((wpa_s->conf->changed_parameters & CFG_CHANGED_COUNTRY) &&
	    wpa_s->conf->country[0] && wpa_s->conf->country[1]) {
		char country[3];
		country[0] = wpa_s->conf->country[0];
		country[1] = wpa_s->conf->country[1];
		country[2] = '\0';
		if (wpa_drv_set_country(wpa_s, country) < 0) {
			wpa_printf(MSG_ERROR, "Failed to set country code "
				   "'%s'", country);
		}
	}

	if (wpa_s->conf->changed_parameters & CFG_CHANGED_EXT_PW_BACKEND)
		wpas_init_ext_pw(wpa_s);

#ifdef CONFIG_WPS
	wpas_wps_update_config(wpa_s);
#endif /* CONFIG_WPS */

#ifdef CONFIG_P2P
	wpas_p2p_update_config(wpa_s);
#endif /* CONFIG_P2P */

	wpa_s->conf->changed_parameters = 0;
}


static void add_freq(int *freqs, int *num_freqs, int freq)
{
	int i;

	for (i = 0; i < *num_freqs; i++) {
		if (freqs[i] == freq)
			return;
	}

	freqs[*num_freqs] = freq;
	(*num_freqs)++;
}


static int * get_bss_freqs_in_ess(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss, *cbss;
	const int max_freqs = 10;
	int *freqs;
	int num_freqs = 0;

	freqs = os_zalloc(sizeof(int) * (max_freqs + 1));
	if (freqs == NULL)
		return NULL;

	cbss = wpa_s->current_bss;

	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		if (bss == cbss)
			continue;
		if (bss->ssid_len == cbss->ssid_len &&
		    os_memcmp(bss->ssid, cbss->ssid, bss->ssid_len) == 0 &&
		    wpa_blacklist_get(wpa_s, bss->bssid) == NULL) {
			add_freq(freqs, &num_freqs, bss->freq);
			if (num_freqs == max_freqs)
				break;
		}
	}

	if (num_freqs == 0) {
		os_free(freqs);
		freqs = NULL;
	}

	return freqs;
}


void wpas_connection_failed(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	int timeout;
	int count;
	int *freqs = NULL;

	/*
	 * Remove possible authentication timeout since the connection failed.
	 */
	eloop_cancel_timeout(wpa_supplicant_timeout, wpa_s, NULL);

	/*
	 * Add the failed BSSID into the blacklist and speed up next scan
	 * attempt if there could be other APs that could accept association.
	 * The current blacklist count indicates how many times we have tried
	 * connecting to this AP and multiple attempts mean that other APs are
	 * either not available or has already been tried, so that we can start
	 * increasing the delay here to avoid constant scanning.
	 */
	count = wpa_blacklist_add(wpa_s, bssid);
	if (count == 1 && wpa_s->current_bss) {
		/*
		 * This BSS was not in the blacklist before. If there is
		 * another BSS available for the same ESS, we should try that
		 * next. Otherwise, we may as well try this one once more
		 * before allowing other, likely worse, ESSes to be considered.
		 */
		freqs = get_bss_freqs_in_ess(wpa_s);
		if (freqs) {
			wpa_dbg(wpa_s, MSG_DEBUG, "Another BSS in this ESS "
				"has been seen; try it next");
			wpa_blacklist_add(wpa_s, bssid);
			/*
			 * On the next scan, go through only the known channels
			 * used in this ESS based on previous scans to speed up
			 * common load balancing use case.
			 */
			os_free(wpa_s->next_scan_freqs);
			wpa_s->next_scan_freqs = freqs;
		}
	}

	/*
	 * Add previous failure count in case the temporary blacklist was
	 * cleared due to no other BSSes being available.
	 */
	count += wpa_s->extra_blacklist_count;

	switch (count) {
	case 1:
		timeout = 100;
		break;
	case 2:
		timeout = 500;
		break;
	case 3:
		timeout = 1000;
		break;
	case 4:
		timeout = 5000;
		break;
	default:
		timeout = 10000;
		break;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "Blacklist count %d --> request scan in %d "
		"ms", count, timeout);

	/*
	 * TODO: if more than one possible AP is available in scan results,
	 * could try the other ones before requesting a new scan.
	 */
	wpa_supplicant_req_scan(wpa_s, timeout / 1000,
				1000 * (timeout % 1000));

#ifdef CONFIG_P2P
	if (wpa_s->global->p2p_cb_on_scan_complete && !wpa_s->global->p2p_disabled &&
	    wpa_s->global->p2p != NULL) {
		wpa_s->global->p2p_cb_on_scan_complete = 0;
		if (p2p_other_scan_completed(wpa_s->global->p2p) == 1) {
			wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Pending P2P operation "
				"continued after failed association");
		}
	}
#endif /* CONFIG_P2P */
}


int wpas_driver_bss_selection(struct wpa_supplicant *wpa_s)
{
	return wpa_s->conf->ap_scan == 2 ||
		(wpa_s->drv_flags & WPA_DRIVER_FLAGS_BSS_SELECTION);
}


#if defined(CONFIG_CTRL_IFACE) || defined(CONFIG_CTRL_IFACE_DBUS_NEW)
int wpa_supplicant_ctrl_iface_ctrl_rsp_handle(struct wpa_supplicant *wpa_s,
					      struct wpa_ssid *ssid,
					      const char *field,
					      const char *value)
{
#ifdef IEEE8021X_EAPOL
	struct eap_peer_config *eap = &ssid->eap;

	wpa_printf(MSG_DEBUG, "CTRL_IFACE: response handle field=%s", field);
	wpa_hexdump_ascii_key(MSG_DEBUG, "CTRL_IFACE: response value",
			      (const u8 *) value, os_strlen(value));

	switch (wpa_supplicant_ctrl_req_from_string(field)) {
	case WPA_CTRL_REQ_EAP_IDENTITY:
		os_free(eap->identity);
		eap->identity = (u8 *) os_strdup(value);
		eap->identity_len = os_strlen(value);
		eap->pending_req_identity = 0;
		if (ssid == wpa_s->current_ssid)
			wpa_s->reassociate = 1;
		break;
	case WPA_CTRL_REQ_EAP_PASSWORD:
		os_free(eap->password);
		eap->password = (u8 *) os_strdup(value);
		eap->password_len = os_strlen(value);
		eap->pending_req_password = 0;
		if (ssid == wpa_s->current_ssid)
			wpa_s->reassociate = 1;
		break;
	case WPA_CTRL_REQ_EAP_NEW_PASSWORD:
		os_free(eap->new_password);
		eap->new_password = (u8 *) os_strdup(value);
		eap->new_password_len = os_strlen(value);
		eap->pending_req_new_password = 0;
		if (ssid == wpa_s->current_ssid)
			wpa_s->reassociate = 1;
		break;
	case WPA_CTRL_REQ_EAP_PIN:
		os_free(eap->pin);
		eap->pin = os_strdup(value);
		eap->pending_req_pin = 0;
		if (ssid == wpa_s->current_ssid)
			wpa_s->reassociate = 1;
		break;
	case WPA_CTRL_REQ_EAP_OTP:
		os_free(eap->otp);
		eap->otp = (u8 *) os_strdup(value);
		eap->otp_len = os_strlen(value);
		os_free(eap->pending_req_otp);
		eap->pending_req_otp = NULL;
		eap->pending_req_otp_len = 0;
		break;
	case WPA_CTRL_REQ_EAP_PASSPHRASE:
		os_free(eap->private_key_passwd);
		eap->private_key_passwd = (u8 *) os_strdup(value);
		eap->pending_req_passphrase = 0;
		if (ssid == wpa_s->current_ssid)
			wpa_s->reassociate = 1;
		break;
	default:
		wpa_printf(MSG_DEBUG, "CTRL_IFACE: Unknown field '%s'", field);
		return -1;
	}

	return 0;
#else /* IEEE8021X_EAPOL */
	wpa_printf(MSG_DEBUG, "CTRL_IFACE: IEEE 802.1X not included");
	return -1;
#endif /* IEEE8021X_EAPOL */
}
#endif /* CONFIG_CTRL_IFACE || CONFIG_CTRL_IFACE_DBUS_NEW */


int wpas_network_disabled(struct wpa_supplicant *wpa_s, struct wpa_ssid *ssid)
{
	int i;
	unsigned int drv_enc;

	if (ssid == NULL)
		return 1;

	if (ssid->disabled)
		return 1;

	if (wpa_s && wpa_s->drv_capa_known)
		drv_enc = wpa_s->drv_enc;
	else
		drv_enc = (unsigned int) -1;

	for (i = 0; i < NUM_WEP_KEYS; i++) {
		size_t len = ssid->wep_key_len[i];
		if (len == 0)
			continue;
		if (len == 5 && (drv_enc & WPA_DRIVER_CAPA_ENC_WEP40))
			continue;
		if (len == 13 && (drv_enc & WPA_DRIVER_CAPA_ENC_WEP104))
			continue;
		if (len == 16 && (drv_enc & WPA_DRIVER_CAPA_ENC_WEP128))
			continue;
		return 1; /* invalid WEP key */
	}

	if (wpa_key_mgmt_wpa_psk(ssid->key_mgmt) && !ssid->psk_set &&
	    !ssid->ext_psk)
		return 1;

	return 0;
}


int wpas_is_p2p_prioritized(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->global->conc_pref == WPA_CONC_PREF_P2P)
		return 1;
	if (wpa_s->global->conc_pref == WPA_CONC_PREF_STA)
		return 0;
	return -1;
}


void wpas_auth_failed(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid = wpa_s->current_ssid;
	int dur;
	struct os_time now;

	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "Authentication failure but no known "
			   "SSID block");
		return;
	}

	if (ssid->key_mgmt == WPA_KEY_MGMT_WPS)
		return;

	ssid->auth_failures++;
	if (ssid->auth_failures > 50)
		dur = 300;
	else if (ssid->auth_failures > 20)
		dur = 120;
	else if (ssid->auth_failures > 10)
		dur = 60;
	else if (ssid->auth_failures > 5)
		dur = 30;
	else if (ssid->auth_failures > 1)
		dur = 20;
	else
		dur = 10;

	os_get_time(&now);
	if (now.sec + dur <= ssid->disabled_until.sec)
		return;

	ssid->disabled_until.sec = now.sec + dur;

	wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_TEMP_DISABLED
		"id=%d ssid=\"%s\" auth_failures=%u duration=%d",
		ssid->id, wpa_ssid_txt(ssid->ssid, ssid->ssid_len),
		ssid->auth_failures, dur);
}


void wpas_clear_temp_disabled(struct wpa_supplicant *wpa_s,
			      struct wpa_ssid *ssid, int clear_failures)
{
	if (ssid == NULL)
		return;

	if (ssid->disabled_until.sec) {
		wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_REENABLED
			"id=%d ssid=\"%s\"",
			ssid->id, wpa_ssid_txt(ssid->ssid, ssid->ssid_len));
	}
	ssid->disabled_until.sec = 0;
	ssid->disabled_until.usec = 0;
	if (clear_failures)
		ssid->auth_failures = 0;
}


int disallowed_bssid(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	size_t i;

	if (wpa_s->disallow_aps_bssid == NULL)
		return 0;

	for (i = 0; i < wpa_s->disallow_aps_bssid_count; i++) {
		if (os_memcmp(wpa_s->disallow_aps_bssid + i * ETH_ALEN,
			      bssid, ETH_ALEN) == 0)
			return 1;
	}

	return 0;
}


int disallowed_ssid(struct wpa_supplicant *wpa_s, const u8 *ssid,
		    size_t ssid_len)
{
	size_t i;

	if (wpa_s->disallow_aps_ssid == NULL || ssid == NULL)
		return 0;

	for (i = 0; i < wpa_s->disallow_aps_ssid_count; i++) {
		struct wpa_ssid_value *s = &wpa_s->disallow_aps_ssid[i];
		if (ssid_len == s->ssid_len &&
		    os_memcmp(ssid, s->ssid, ssid_len) == 0)
			return 1;
	}

	return 0;
}


/**
 * wpas_request_connection - Request a new connection
 * @wpa_s: Pointer to the network interface
 *
 * This function is used to request a new connection to be found. It will mark
 * the interface to allow reassociation and request a new scan to find a
 * suitable network to connect to.
 */
void wpas_request_connection(struct wpa_supplicant *wpa_s)
{
	wpa_s->normal_scans = 0;
	wpa_supplicant_reinit_autoscan(wpa_s);
	wpa_s->extra_blacklist_count = 0;
	wpa_s->disconnected = 0;
	wpa_s->reassociate = 1;
	wpa_supplicant_req_scan(wpa_s, 0, 0);
}
