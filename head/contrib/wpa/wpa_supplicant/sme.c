/*
 * wpa_supplicant - SME
 * Copyright (c) 2009-2010, Jouni Malinen <j@w1.fi>
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
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "common/wpa_common.h"
#include "rsn_supp/wpa.h"
#include "rsn_supp/pmksa_cache.h"
#include "config.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "wpas_glue.h"
#include "wps_supplicant.h"
#include "notify.h"
#include "blacklist.h"
#include "bss.h"
#include "scan.h"
#include "sme.h"

void sme_authenticate(struct wpa_supplicant *wpa_s,
		      struct wpa_bss *bss, struct wpa_ssid *ssid)
{
	struct wpa_driver_auth_params params;
	struct wpa_ssid *old_ssid;
#ifdef CONFIG_IEEE80211R
	const u8 *ie;
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_IEEE80211R
	const u8 *md = NULL;
#endif /* CONFIG_IEEE80211R */
	int i, bssid_changed;

	if (bss == NULL) {
		wpa_printf(MSG_ERROR, "SME: No scan result available for the "
			   "network");
		return;
	}

	wpa_s->current_bss = bss;

	os_memset(&params, 0, sizeof(params));
	wpa_s->reassociate = 0;

	params.freq = bss->freq;
	params.bssid = bss->bssid;
	params.ssid = bss->ssid;
	params.ssid_len = bss->ssid_len;

	if (wpa_s->sme.ssid_len != params.ssid_len ||
	    os_memcmp(wpa_s->sme.ssid, params.ssid, params.ssid_len) != 0)
		wpa_s->sme.prev_bssid_set = 0;

	wpa_s->sme.freq = params.freq;
	os_memcpy(wpa_s->sme.ssid, params.ssid, params.ssid_len);
	wpa_s->sme.ssid_len = params.ssid_len;

	params.auth_alg = WPA_AUTH_ALG_OPEN;
#ifdef IEEE8021X_EAPOL
	if (ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA) {
		if (ssid->leap) {
			if (ssid->non_leap == 0)
				params.auth_alg = WPA_AUTH_ALG_LEAP;
			else
				params.auth_alg |= WPA_AUTH_ALG_LEAP;
		}
	}
#endif /* IEEE8021X_EAPOL */
	wpa_printf(MSG_DEBUG, "Automatic auth_alg selection: 0x%x",
		   params.auth_alg);
	if (ssid->auth_alg) {
		params.auth_alg = ssid->auth_alg;
		wpa_printf(MSG_DEBUG, "Overriding auth_alg selection: 0x%x",
			   params.auth_alg);
	}

	for (i = 0; i < NUM_WEP_KEYS; i++) {
		if (ssid->wep_key_len[i])
			params.wep_key[i] = ssid->wep_key[i];
		params.wep_key_len[i] = ssid->wep_key_len[i];
	}
	params.wep_tx_keyidx = ssid->wep_tx_keyidx;

	bssid_changed = !is_zero_ether_addr(wpa_s->bssid);
	os_memset(wpa_s->bssid, 0, ETH_ALEN);
	os_memcpy(wpa_s->pending_bssid, bss->bssid, ETH_ALEN);
	if (bssid_changed)
		wpas_notify_bssid_changed(wpa_s);

	if ((wpa_bss_get_vendor_ie(bss, WPA_IE_VENDOR_TYPE) ||
	     wpa_bss_get_ie(bss, WLAN_EID_RSN)) &&
	    (ssid->key_mgmt & (WPA_KEY_MGMT_IEEE8021X | WPA_KEY_MGMT_PSK |
			       WPA_KEY_MGMT_FT_IEEE8021X |
			       WPA_KEY_MGMT_FT_PSK |
			       WPA_KEY_MGMT_IEEE8021X_SHA256 |
			       WPA_KEY_MGMT_PSK_SHA256))) {
		int try_opportunistic;
		try_opportunistic = ssid->proactive_key_caching &&
			(ssid->proto & WPA_PROTO_RSN);
		if (pmksa_cache_set_current(wpa_s->wpa, NULL, bss->bssid,
					    wpa_s->current_ssid,
					    try_opportunistic) == 0)
			eapol_sm_notify_pmkid_attempt(wpa_s->eapol, 1);
		wpa_s->sme.assoc_req_ie_len = sizeof(wpa_s->sme.assoc_req_ie);
		if (wpa_supplicant_set_suites(wpa_s, bss, ssid,
					      wpa_s->sme.assoc_req_ie,
					      &wpa_s->sme.assoc_req_ie_len)) {
			wpa_printf(MSG_WARNING, "SME: Failed to set WPA key "
				   "management and encryption suites");
			return;
		}
	} else if (ssid->key_mgmt &
		   (WPA_KEY_MGMT_PSK | WPA_KEY_MGMT_IEEE8021X |
		    WPA_KEY_MGMT_WPA_NONE | WPA_KEY_MGMT_FT_PSK |
		    WPA_KEY_MGMT_FT_IEEE8021X | WPA_KEY_MGMT_PSK_SHA256 |
		    WPA_KEY_MGMT_IEEE8021X_SHA256)) {
		wpa_s->sme.assoc_req_ie_len = sizeof(wpa_s->sme.assoc_req_ie);
		if (wpa_supplicant_set_suites(wpa_s, NULL, ssid,
					      wpa_s->sme.assoc_req_ie,
					      &wpa_s->sme.assoc_req_ie_len)) {
			wpa_printf(MSG_WARNING, "SME: Failed to set WPA key "
				   "management and encryption suites (no scan "
				   "results)");
			return;
		}
#ifdef CONFIG_WPS
	} else if (ssid->key_mgmt & WPA_KEY_MGMT_WPS) {
		struct wpabuf *wps_ie;
		wps_ie = wps_build_assoc_req_ie(wpas_wps_get_req_type(ssid));
		if (wps_ie && wpabuf_len(wps_ie) <=
		    sizeof(wpa_s->sme.assoc_req_ie)) {
			wpa_s->sme.assoc_req_ie_len = wpabuf_len(wps_ie);
			os_memcpy(wpa_s->sme.assoc_req_ie, wpabuf_head(wps_ie),
				  wpa_s->sme.assoc_req_ie_len);
		} else
			wpa_s->sme.assoc_req_ie_len = 0;
		wpabuf_free(wps_ie);
		wpa_supplicant_set_non_wpa_policy(wpa_s, ssid);
#endif /* CONFIG_WPS */
	} else {
		wpa_supplicant_set_non_wpa_policy(wpa_s, ssid);
		wpa_s->sme.assoc_req_ie_len = 0;
	}

#ifdef CONFIG_IEEE80211R
	ie = wpa_bss_get_ie(bss, WLAN_EID_MOBILITY_DOMAIN);
	if (ie && ie[1] >= MOBILITY_DOMAIN_ID_LEN)
		md = ie + 2;
	wpa_sm_set_ft_params(wpa_s->wpa, ie, ie ? 2 + ie[1] : 0);
	if (md) {
		/* Prepare for the next transition */
		wpa_ft_prepare_auth_request(wpa_s->wpa, ie);
	}

	if (md && ssid->key_mgmt & (WPA_KEY_MGMT_FT_PSK |
				    WPA_KEY_MGMT_FT_IEEE8021X)) {
		if (wpa_s->sme.assoc_req_ie_len + 5 <
		    sizeof(wpa_s->sme.assoc_req_ie)) {
			struct rsn_mdie *mdie;
			u8 *pos = wpa_s->sme.assoc_req_ie +
				wpa_s->sme.assoc_req_ie_len;
			*pos++ = WLAN_EID_MOBILITY_DOMAIN;
			*pos++ = sizeof(*mdie);
			mdie = (struct rsn_mdie *) pos;
			os_memcpy(mdie->mobility_domain, md,
				  MOBILITY_DOMAIN_ID_LEN);
			mdie->ft_capab = md[MOBILITY_DOMAIN_ID_LEN];
			wpa_s->sme.assoc_req_ie_len += 5;
		}

		if (wpa_s->sme.ft_used &&
		    os_memcmp(md, wpa_s->sme.mobility_domain, 2) == 0 &&
		    wpa_sm_has_ptk(wpa_s->wpa)) {
			wpa_printf(MSG_DEBUG, "SME: Trying to use FT "
				   "over-the-air");
			params.auth_alg = WPA_AUTH_ALG_FT;
			params.ie = wpa_s->sme.ft_ies;
			params.ie_len = wpa_s->sme.ft_ies_len;
		}
	}
#endif /* CONFIG_IEEE80211R */

#ifdef CONFIG_IEEE80211W
	wpa_s->sme.mfp = ssid->ieee80211w;
	if (ssid->ieee80211w != NO_MGMT_FRAME_PROTECTION) {
		const u8 *rsn = wpa_bss_get_ie(bss, WLAN_EID_RSN);
		struct wpa_ie_data _ie;
		if (rsn && wpa_parse_wpa_ie(rsn, 2 + rsn[1], &_ie) == 0 &&
		    _ie.capabilities &
		    (WPA_CAPABILITY_MFPC | WPA_CAPABILITY_MFPR)) {
			wpa_printf(MSG_DEBUG, "WPA: Selected AP supports MFP: "
				   "require MFP");
			wpa_s->sme.mfp = MGMT_FRAME_PROTECTION_REQUIRED;
		}
	}
#endif /* CONFIG_IEEE80211W */

	wpa_supplicant_cancel_scan(wpa_s);

	wpa_msg(wpa_s, MSG_INFO, "Trying to authenticate with " MACSTR
		" (SSID='%s' freq=%d MHz)", MAC2STR(params.bssid),
		wpa_ssid_txt(params.ssid, params.ssid_len), params.freq);

	wpa_clear_keys(wpa_s, bss->bssid);
	wpa_supplicant_set_state(wpa_s, WPA_AUTHENTICATING);
	old_ssid = wpa_s->current_ssid;
	wpa_s->current_ssid = ssid;
	wpa_supplicant_rsn_supp_set_config(wpa_s, wpa_s->current_ssid);
	wpa_supplicant_initiate_eapol(wpa_s);
	if (old_ssid != wpa_s->current_ssid)
		wpas_notify_network_changed(wpa_s);

	wpa_s->sme.auth_alg = params.auth_alg;
	if (wpa_drv_authenticate(wpa_s, &params) < 0) {
		wpa_msg(wpa_s, MSG_INFO, "Authentication request to the "
			"driver failed");
		wpa_supplicant_req_scan(wpa_s, 1, 0);
		return;
	}

	/* TODO: add timeout on authentication */

	/*
	 * Association will be started based on the authentication event from
	 * the driver.
	 */
}


void sme_event_auth(struct wpa_supplicant *wpa_s, union wpa_event_data *data)
{
	struct wpa_ssid *ssid = wpa_s->current_ssid;

	if (ssid == NULL) {
		wpa_printf(MSG_DEBUG, "SME: Ignore authentication event when "
			   "network is not selected");
		return;
	}

	if (wpa_s->wpa_state != WPA_AUTHENTICATING) {
		wpa_printf(MSG_DEBUG, "SME: Ignore authentication event when "
			   "not in authenticating state");
		return;
	}

	if (os_memcmp(wpa_s->pending_bssid, data->auth.peer, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "SME: Ignore authentication with "
			   "unexpected peer " MACSTR,
			   MAC2STR(data->auth.peer));
		return;
	}

	wpa_printf(MSG_DEBUG, "SME: Authentication response: peer=" MACSTR
		   " auth_type=%d status_code=%d",
		   MAC2STR(data->auth.peer), data->auth.auth_type,
		   data->auth.status_code);
	wpa_hexdump(MSG_MSGDUMP, "SME: Authentication response IEs",
		    data->auth.ies, data->auth.ies_len);

	if (data->auth.status_code != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG, "SME: Authentication failed (status "
			   "code %d)", data->auth.status_code);

		if (data->auth.status_code !=
		    WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG ||
		    wpa_s->sme.auth_alg == data->auth.auth_type ||
		    wpa_s->current_ssid->auth_alg == WPA_AUTH_ALG_LEAP)
			return;

		switch (data->auth.auth_type) {
		case WLAN_AUTH_OPEN:
			wpa_s->current_ssid->auth_alg = WPA_AUTH_ALG_SHARED;

			wpa_printf(MSG_DEBUG, "SME: Trying SHARED auth");
			wpa_supplicant_associate(wpa_s, wpa_s->current_bss,
						 wpa_s->current_ssid);
			return;

		case WLAN_AUTH_SHARED_KEY:
			wpa_s->current_ssid->auth_alg = WPA_AUTH_ALG_LEAP;

			wpa_printf(MSG_DEBUG, "SME: Trying LEAP auth");
			wpa_supplicant_associate(wpa_s, wpa_s->current_bss,
						 wpa_s->current_ssid);
			return;

		default:
			return;
		}
	}

#ifdef CONFIG_IEEE80211R
	if (data->auth.auth_type == WLAN_AUTH_FT) {
		union wpa_event_data edata;
		os_memset(&edata, 0, sizeof(edata));
		edata.ft_ies.ies = data->auth.ies;
		edata.ft_ies.ies_len = data->auth.ies_len;
		os_memcpy(edata.ft_ies.target_ap, data->auth.peer, ETH_ALEN);
		wpa_supplicant_event(wpa_s, EVENT_FT_RESPONSE, &edata);
	}
#endif /* CONFIG_IEEE80211R */

	sme_associate(wpa_s, ssid->mode, data->auth.peer,
		      data->auth.auth_type);
}


void sme_associate(struct wpa_supplicant *wpa_s, enum wpas_mode mode,
		   const u8 *bssid, u16 auth_type)
{
	struct wpa_driver_associate_params params;
	struct ieee802_11_elems elems;

	os_memset(&params, 0, sizeof(params));
	params.bssid = bssid;
	params.ssid = wpa_s->sme.ssid;
	params.ssid_len = wpa_s->sme.ssid_len;
	params.freq = wpa_s->sme.freq;
	params.wpa_ie = wpa_s->sme.assoc_req_ie_len ?
		wpa_s->sme.assoc_req_ie : NULL;
	params.wpa_ie_len = wpa_s->sme.assoc_req_ie_len;
#ifdef CONFIG_IEEE80211R
	if (auth_type == WLAN_AUTH_FT && wpa_s->sme.ft_ies) {
		params.wpa_ie = wpa_s->sme.ft_ies;
		params.wpa_ie_len = wpa_s->sme.ft_ies_len;
	}
#endif /* CONFIG_IEEE80211R */
	params.mode = mode;
	params.mgmt_frame_protection = wpa_s->sme.mfp;
	if (wpa_s->sme.prev_bssid_set)
		params.prev_bssid = wpa_s->sme.prev_bssid;

	wpa_msg(wpa_s, MSG_INFO, "Trying to associate with " MACSTR
		" (SSID='%s' freq=%d MHz)", MAC2STR(params.bssid),
		params.ssid ? wpa_ssid_txt(params.ssid, params.ssid_len) : "",
		params.freq);

	wpa_supplicant_set_state(wpa_s, WPA_ASSOCIATING);

	if (params.wpa_ie == NULL ||
	    ieee802_11_parse_elems(params.wpa_ie, params.wpa_ie_len, &elems, 0)
	    < 0) {
		wpa_printf(MSG_DEBUG, "SME: Could not parse own IEs?!");
		os_memset(&elems, 0, sizeof(elems));
	}
	if (elems.rsn_ie)
		wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, elems.rsn_ie - 2,
					elems.rsn_ie_len + 2);
	else if (elems.wpa_ie)
		wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, elems.wpa_ie - 2,
					elems.wpa_ie_len + 2);
	else
		wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, NULL, 0);

	if (wpa_drv_associate(wpa_s, &params) < 0) {
		wpa_msg(wpa_s, MSG_INFO, "Association request to the driver "
			"failed");
		wpa_supplicant_req_scan(wpa_s, 5, 0);
		return;
	}

	/* TODO: add timeout on association */
}


int sme_update_ft_ies(struct wpa_supplicant *wpa_s, const u8 *md,
		      const u8 *ies, size_t ies_len)
{
	if (md == NULL || ies == NULL) {
		wpa_printf(MSG_DEBUG, "SME: Remove mobility domain");
		os_free(wpa_s->sme.ft_ies);
		wpa_s->sme.ft_ies = NULL;
		wpa_s->sme.ft_ies_len = 0;
		wpa_s->sme.ft_used = 0;
		return 0;
	}

	os_memcpy(wpa_s->sme.mobility_domain, md, MOBILITY_DOMAIN_ID_LEN);
	wpa_hexdump(MSG_DEBUG, "SME: FT IEs", ies, ies_len);
	os_free(wpa_s->sme.ft_ies);
	wpa_s->sme.ft_ies = os_malloc(ies_len);
	if (wpa_s->sme.ft_ies == NULL)
		return -1;
	os_memcpy(wpa_s->sme.ft_ies, ies, ies_len);
	wpa_s->sme.ft_ies_len = ies_len;
	return 0;
}


void sme_event_assoc_reject(struct wpa_supplicant *wpa_s,
			    union wpa_event_data *data)
{
	int bssid_changed;
	int timeout = 5000;

	wpa_printf(MSG_DEBUG, "SME: Association with " MACSTR " failed: "
		   "status code %d", MAC2STR(wpa_s->pending_bssid),
		   data->assoc_reject.status_code);

	bssid_changed = !is_zero_ether_addr(wpa_s->bssid);

	/*
	 * For now, unconditionally terminate the previous authentication. In
	 * theory, this should not be needed, but mac80211 gets quite confused
	 * if the authentication is left pending.. Some roaming cases might
	 * benefit from using the previous authentication, so this could be
	 * optimized in the future.
	 */
	if (wpa_drv_deauthenticate(wpa_s, wpa_s->pending_bssid,
				   WLAN_REASON_DEAUTH_LEAVING) < 0) {
		wpa_msg(wpa_s, MSG_INFO,
			"Deauth request to the driver failed");
	}
	wpa_s->sme.prev_bssid_set = 0;

	if (wpa_blacklist_add(wpa_s, wpa_s->pending_bssid) == 0) {
		struct wpa_blacklist *b;
		b = wpa_blacklist_get(wpa_s, wpa_s->pending_bssid);
		if (b && b->count < 3) {
			/*
			 * Speed up next attempt if there could be other APs
			 * that could accept association.
			 */
			timeout = 100;
		}
	}
	wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
	os_memset(wpa_s->bssid, 0, ETH_ALEN);
	os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
	if (bssid_changed)
		wpas_notify_bssid_changed(wpa_s);

	/*
	 * TODO: if more than one possible AP is available in scan results,
	 * could try the other ones before requesting a new scan.
	 */
	wpa_supplicant_req_scan(wpa_s, timeout / 1000,
				1000 * (timeout % 1000));
}


void sme_event_auth_timed_out(struct wpa_supplicant *wpa_s,
			      union wpa_event_data *data)
{
	wpa_printf(MSG_DEBUG, "SME: Authentication timed out");
	wpa_supplicant_req_scan(wpa_s, 5, 0);
}


void sme_event_assoc_timed_out(struct wpa_supplicant *wpa_s,
			       union wpa_event_data *data)
{
	wpa_printf(MSG_DEBUG, "SME: Association timed out");
	wpa_supplicant_mark_disassoc(wpa_s);
	wpa_supplicant_req_scan(wpa_s, 5, 0);
}


void sme_event_disassoc(struct wpa_supplicant *wpa_s,
			union wpa_event_data *data)
{
	wpa_printf(MSG_DEBUG, "SME: Disassociation event received");
	if (wpa_s->sme.prev_bssid_set &&
	    !(wpa_s->drv_flags & WPA_DRIVER_FLAGS_USER_SPACE_MLME)) {
		/*
		 * cfg80211/mac80211 can get into somewhat confused state if
		 * the AP only disassociates us and leaves us in authenticated
		 * state. For now, force the state to be cleared to avoid
		 * confusing errors if we try to associate with the AP again.
		 */
		wpa_printf(MSG_DEBUG, "SME: Deauthenticate to clear driver "
			   "state");
		wpa_drv_deauthenticate(wpa_s, wpa_s->sme.prev_bssid,
				       WLAN_REASON_DEAUTH_LEAVING);
	}
}
