/*
 * WPA Supplicant - Driver event processing
 * Copyright (c) 2003-2010, Jouni Malinen <j@w1.fi>
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
#include "eapol_supp/eapol_supp_sm.h"
#include "rsn_supp/wpa.h"
#include "eloop.h"
#include "config.h"
#include "l2_packet/l2_packet.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "pcsc_funcs.h"
#include "rsn_supp/preauth.h"
#include "rsn_supp/pmksa_cache.h"
#include "common/wpa_ctrl.h"
#include "eap_peer/eap.h"
#include "ap/hostapd.h"
#include "notify.h"
#include "common/ieee802_11_defs.h"
#include "blacklist.h"
#include "wpas_glue.h"
#include "wps_supplicant.h"
#include "ibss_rsn.h"
#include "sme.h"
#include "bgscan.h"
#include "ap.h"
#include "bss.h"
#include "mlme.h"
#include "scan.h"


static int wpa_supplicant_select_config(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid, *old_ssid;

	if (wpa_s->conf->ap_scan == 1 && wpa_s->current_ssid)
		return 0;

	wpa_printf(MSG_DEBUG, "Select network based on association "
		   "information");
	ssid = wpa_supplicant_get_ssid(wpa_s);
	if (ssid == NULL) {
		wpa_printf(MSG_INFO, "No network configuration found for the "
			   "current AP");
		return -1;
	}

	if (ssid->disabled) {
		wpa_printf(MSG_DEBUG, "Selected network is disabled");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "Network configuration found for the current "
		   "AP");
	if (ssid->key_mgmt & (WPA_KEY_MGMT_PSK | WPA_KEY_MGMT_IEEE8021X |
			      WPA_KEY_MGMT_WPA_NONE |
			      WPA_KEY_MGMT_FT_PSK | WPA_KEY_MGMT_FT_IEEE8021X |
			      WPA_KEY_MGMT_PSK_SHA256 |
			      WPA_KEY_MGMT_IEEE8021X_SHA256)) {
		u8 wpa_ie[80];
		size_t wpa_ie_len = sizeof(wpa_ie);
		wpa_supplicant_set_suites(wpa_s, NULL, ssid,
					  wpa_ie, &wpa_ie_len);
	} else {
		wpa_supplicant_set_non_wpa_policy(wpa_s, ssid);
	}

	if (wpa_s->current_ssid && wpa_s->current_ssid != ssid)
		eapol_sm_invalidate_cached_session(wpa_s->eapol);
	old_ssid = wpa_s->current_ssid;
	wpa_s->current_ssid = ssid;
	wpa_supplicant_rsn_supp_set_config(wpa_s, wpa_s->current_ssid);
	wpa_supplicant_initiate_eapol(wpa_s);
	if (old_ssid != wpa_s->current_ssid)
		wpas_notify_network_changed(wpa_s);

	return 0;
}


static void wpa_supplicant_stop_countermeasures(void *eloop_ctx,
						void *sock_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	if (wpa_s->countermeasures) {
		wpa_s->countermeasures = 0;
		wpa_drv_set_countermeasures(wpa_s, 0);
		wpa_msg(wpa_s, MSG_INFO, "WPA: TKIP countermeasures stopped");
		wpa_supplicant_req_scan(wpa_s, 0, 0);
	}
}


void wpa_supplicant_mark_disassoc(struct wpa_supplicant *wpa_s)
{
	int bssid_changed;

	wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
	bssid_changed = !is_zero_ether_addr(wpa_s->bssid);
	os_memset(wpa_s->bssid, 0, ETH_ALEN);
	os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
	wpa_s->current_bss = NULL;
	if (bssid_changed)
		wpas_notify_bssid_changed(wpa_s);

	eapol_sm_notify_portEnabled(wpa_s->eapol, FALSE);
	eapol_sm_notify_portValid(wpa_s->eapol, FALSE);
	if (wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt))
		eapol_sm_notify_eap_success(wpa_s->eapol, FALSE);
	wpa_s->ap_ies_from_associnfo = 0;
}


static void wpa_find_assoc_pmkid(struct wpa_supplicant *wpa_s)
{
	struct wpa_ie_data ie;
	int pmksa_set = -1;
	size_t i;

	if (wpa_sm_parse_own_wpa_ie(wpa_s->wpa, &ie) < 0 ||
	    ie.pmkid == NULL)
		return;

	for (i = 0; i < ie.num_pmkid; i++) {
		pmksa_set = pmksa_cache_set_current(wpa_s->wpa,
						    ie.pmkid + i * PMKID_LEN,
						    NULL, NULL, 0);
		if (pmksa_set == 0) {
			eapol_sm_notify_pmkid_attempt(wpa_s->eapol, 1);
			break;
		}
	}

	wpa_printf(MSG_DEBUG, "RSN: PMKID from assoc IE %sfound from PMKSA "
		   "cache", pmksa_set == 0 ? "" : "not ");
}


static void wpa_supplicant_event_pmkid_candidate(struct wpa_supplicant *wpa_s,
						 union wpa_event_data *data)
{
	if (data == NULL) {
		wpa_printf(MSG_DEBUG, "RSN: No data in PMKID candidate event");
		return;
	}
	wpa_printf(MSG_DEBUG, "RSN: PMKID candidate event - bssid=" MACSTR
		   " index=%d preauth=%d",
		   MAC2STR(data->pmkid_candidate.bssid),
		   data->pmkid_candidate.index,
		   data->pmkid_candidate.preauth);

	pmksa_candidate_add(wpa_s->wpa, data->pmkid_candidate.bssid,
			    data->pmkid_candidate.index,
			    data->pmkid_candidate.preauth);
}


static int wpa_supplicant_dynamic_keys(struct wpa_supplicant *wpa_s)
{
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_NONE ||
	    wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE)
		return 0;

#ifdef IEEE8021X_EAPOL
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA &&
	    wpa_s->current_ssid &&
	    !(wpa_s->current_ssid->eapol_flags &
	      (EAPOL_FLAG_REQUIRE_KEY_UNICAST |
	       EAPOL_FLAG_REQUIRE_KEY_BROADCAST))) {
		/* IEEE 802.1X, but not using dynamic WEP keys (i.e., either
		 * plaintext or static WEP keys). */
		return 0;
	}
#endif /* IEEE8021X_EAPOL */

	return 1;
}


/**
 * wpa_supplicant_scard_init - Initialize SIM/USIM access with PC/SC
 * @wpa_s: pointer to wpa_supplicant data
 * @ssid: Configuration data for the network
 * Returns: 0 on success, -1 on failure
 *
 * This function is called when starting authentication with a network that is
 * configured to use PC/SC for SIM/USIM access (EAP-SIM or EAP-AKA).
 */
int wpa_supplicant_scard_init(struct wpa_supplicant *wpa_s,
			      struct wpa_ssid *ssid)
{
#ifdef IEEE8021X_EAPOL
	int aka = 0, sim = 0, type;

	if (ssid->eap.pcsc == NULL || wpa_s->scard != NULL)
		return 0;

	if (ssid->eap.eap_methods == NULL) {
		sim = 1;
		aka = 1;
	} else {
		struct eap_method_type *eap = ssid->eap.eap_methods;
		while (eap->vendor != EAP_VENDOR_IETF ||
		       eap->method != EAP_TYPE_NONE) {
			if (eap->vendor == EAP_VENDOR_IETF) {
				if (eap->method == EAP_TYPE_SIM)
					sim = 1;
				else if (eap->method == EAP_TYPE_AKA)
					aka = 1;
			}
			eap++;
		}
	}

	if (eap_peer_get_eap_method(EAP_VENDOR_IETF, EAP_TYPE_SIM) == NULL)
		sim = 0;
	if (eap_peer_get_eap_method(EAP_VENDOR_IETF, EAP_TYPE_AKA) == NULL)
		aka = 0;

	if (!sim && !aka) {
		wpa_printf(MSG_DEBUG, "Selected network is configured to use "
			   "SIM, but neither EAP-SIM nor EAP-AKA are enabled");
		return 0;
	}

	wpa_printf(MSG_DEBUG, "Selected network is configured to use SIM "
		   "(sim=%d aka=%d) - initialize PCSC", sim, aka);
	if (sim && aka)
		type = SCARD_TRY_BOTH;
	else if (aka)
		type = SCARD_USIM_ONLY;
	else
		type = SCARD_GSM_SIM_ONLY;

	wpa_s->scard = scard_init(type);
	if (wpa_s->scard == NULL) {
		wpa_printf(MSG_WARNING, "Failed to initialize SIM "
			   "(pcsc-lite)");
		return -1;
	}
	wpa_sm_set_scard_ctx(wpa_s->wpa, wpa_s->scard);
	eapol_sm_register_scard_ctx(wpa_s->eapol, wpa_s->scard);
#endif /* IEEE8021X_EAPOL */

	return 0;
}


#ifndef CONFIG_NO_SCAN_PROCESSING
static int wpa_supplicant_match_privacy(struct wpa_scan_res *bss,
					struct wpa_ssid *ssid)
{
	int i, privacy = 0;

	if (ssid->mixed_cell)
		return 1;

#ifdef CONFIG_WPS
	if (ssid->key_mgmt & WPA_KEY_MGMT_WPS)
		return 1;
#endif /* CONFIG_WPS */

	for (i = 0; i < NUM_WEP_KEYS; i++) {
		if (ssid->wep_key_len[i]) {
			privacy = 1;
			break;
		}
	}
#ifdef IEEE8021X_EAPOL
	if ((ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA) &&
	    ssid->eapol_flags & (EAPOL_FLAG_REQUIRE_KEY_UNICAST |
				 EAPOL_FLAG_REQUIRE_KEY_BROADCAST))
		privacy = 1;
#endif /* IEEE8021X_EAPOL */

	if (bss->caps & IEEE80211_CAP_PRIVACY)
		return privacy;
	return !privacy;
}


static int wpa_supplicant_ssid_bss_match(struct wpa_supplicant *wpa_s,
					 struct wpa_ssid *ssid,
					 struct wpa_scan_res *bss)
{
	struct wpa_ie_data ie;
	int proto_match = 0;
	const u8 *rsn_ie, *wpa_ie;
	int ret;

	ret = wpas_wps_ssid_bss_match(wpa_s, ssid, bss);
	if (ret >= 0)
		return ret;

	rsn_ie = wpa_scan_get_ie(bss, WLAN_EID_RSN);
	while ((ssid->proto & WPA_PROTO_RSN) && rsn_ie) {
		proto_match++;

		if (wpa_parse_wpa_ie(rsn_ie, 2 + rsn_ie[1], &ie)) {
			wpa_printf(MSG_DEBUG, "   skip RSN IE - parse failed");
			break;
		}
		if (!(ie.proto & ssid->proto)) {
			wpa_printf(MSG_DEBUG, "   skip RSN IE - proto "
				   "mismatch");
			break;
		}

		if (!(ie.pairwise_cipher & ssid->pairwise_cipher)) {
			wpa_printf(MSG_DEBUG, "   skip RSN IE - PTK cipher "
				   "mismatch");
			break;
		}

		if (!(ie.group_cipher & ssid->group_cipher)) {
			wpa_printf(MSG_DEBUG, "   skip RSN IE - GTK cipher "
				   "mismatch");
			break;
		}

		if (!(ie.key_mgmt & ssid->key_mgmt)) {
			wpa_printf(MSG_DEBUG, "   skip RSN IE - key mgmt "
				   "mismatch");
			break;
		}

#ifdef CONFIG_IEEE80211W
		if (!(ie.capabilities & WPA_CAPABILITY_MFPC) &&
		    ssid->ieee80211w == MGMT_FRAME_PROTECTION_REQUIRED) {
			wpa_printf(MSG_DEBUG, "   skip RSN IE - no mgmt frame "
				   "protection");
			break;
		}
#endif /* CONFIG_IEEE80211W */

		wpa_printf(MSG_DEBUG, "   selected based on RSN IE");
		return 1;
	}

	wpa_ie = wpa_scan_get_vendor_ie(bss, WPA_IE_VENDOR_TYPE);
	while ((ssid->proto & WPA_PROTO_WPA) && wpa_ie) {
		proto_match++;

		if (wpa_parse_wpa_ie(wpa_ie, 2 + wpa_ie[1], &ie)) {
			wpa_printf(MSG_DEBUG, "   skip WPA IE - parse failed");
			break;
		}
		if (!(ie.proto & ssid->proto)) {
			wpa_printf(MSG_DEBUG, "   skip WPA IE - proto "
				   "mismatch");
			break;
		}

		if (!(ie.pairwise_cipher & ssid->pairwise_cipher)) {
			wpa_printf(MSG_DEBUG, "   skip WPA IE - PTK cipher "
				   "mismatch");
			break;
		}

		if (!(ie.group_cipher & ssid->group_cipher)) {
			wpa_printf(MSG_DEBUG, "   skip WPA IE - GTK cipher "
				   "mismatch");
			break;
		}

		if (!(ie.key_mgmt & ssid->key_mgmt)) {
			wpa_printf(MSG_DEBUG, "   skip WPA IE - key mgmt "
				   "mismatch");
			break;
		}

		wpa_printf(MSG_DEBUG, "   selected based on WPA IE");
		return 1;
	}

	if (proto_match == 0)
		wpa_printf(MSG_DEBUG, "   skip - no WPA/RSN proto match");

	return 0;
}


static int freq_allowed(int *freqs, int freq)
{
	int i;

	if (freqs == NULL)
		return 1;

	for (i = 0; freqs[i]; i++)
		if (freqs[i] == freq)
			return 1;
	return 0;
}


static struct wpa_bss *
wpa_supplicant_select_bss_wpa(struct wpa_supplicant *wpa_s,
			      struct wpa_scan_results *scan_res,
			      struct wpa_ssid *group,
			      struct wpa_ssid **selected_ssid)
{
	struct wpa_ssid *ssid;
	struct wpa_scan_res *bss;
	size_t i;
	struct wpa_blacklist *e;
	const u8 *ie;

	wpa_printf(MSG_DEBUG, "Try to find WPA-enabled AP");
	for (i = 0; i < scan_res->num; i++) {
		const u8 *ssid_;
		u8 wpa_ie_len, rsn_ie_len, ssid_len;
		bss = scan_res->res[i];

		ie = wpa_scan_get_ie(bss, WLAN_EID_SSID);
		ssid_ = ie ? ie + 2 : (u8 *) "";
		ssid_len = ie ? ie[1] : 0;

		ie = wpa_scan_get_vendor_ie(bss, WPA_IE_VENDOR_TYPE);
		wpa_ie_len = ie ? ie[1] : 0;

		ie = wpa_scan_get_ie(bss, WLAN_EID_RSN);
		rsn_ie_len = ie ? ie[1] : 0;

		wpa_printf(MSG_DEBUG, "%d: " MACSTR " ssid='%s' "
			   "wpa_ie_len=%u rsn_ie_len=%u caps=0x%x",
			   (int) i, MAC2STR(bss->bssid),
			   wpa_ssid_txt(ssid_, ssid_len),
			   wpa_ie_len, rsn_ie_len, bss->caps);

		e = wpa_blacklist_get(wpa_s, bss->bssid);
		if (e && e->count > 1) {
			wpa_printf(MSG_DEBUG, "   skip - blacklisted");
			continue;
		}

		if (ssid_len == 0) {
			wpa_printf(MSG_DEBUG, "   skip - SSID not known");
			continue;
		}

		if (wpa_ie_len == 0 && rsn_ie_len == 0) {
			wpa_printf(MSG_DEBUG, "   skip - no WPA/RSN IE");
			continue;
		}

		for (ssid = group; ssid; ssid = ssid->pnext) {
			int check_ssid = 1;

			if (ssid->disabled) {
				wpa_printf(MSG_DEBUG, "   skip - disabled");
				continue;
			}

#ifdef CONFIG_WPS
			if (ssid->ssid_len == 0 &&
			    wpas_wps_ssid_wildcard_ok(wpa_s, ssid, bss))
				check_ssid = 0;
#endif /* CONFIG_WPS */

			if (check_ssid &&
			    (ssid_len != ssid->ssid_len ||
			     os_memcmp(ssid_, ssid->ssid, ssid_len) != 0)) {
				wpa_printf(MSG_DEBUG, "   skip - "
					   "SSID mismatch");
				continue;
			}

			if (ssid->bssid_set &&
			    os_memcmp(bss->bssid, ssid->bssid, ETH_ALEN) != 0)
			{
				wpa_printf(MSG_DEBUG, "   skip - "
					   "BSSID mismatch");
				continue;
			}

			if (!wpa_supplicant_ssid_bss_match(wpa_s, ssid, bss))
				continue;

			if (!freq_allowed(ssid->freq_list, bss->freq)) {
				wpa_printf(MSG_DEBUG, "   skip - "
					   "frequency not allowed");
				continue;
			}

			wpa_printf(MSG_DEBUG, "   selected WPA AP "
				   MACSTR " ssid='%s'",
				   MAC2STR(bss->bssid),
				   wpa_ssid_txt(ssid_, ssid_len));
			*selected_ssid = ssid;
			return wpa_bss_get(wpa_s, bss->bssid, ssid_, ssid_len);
		}
	}

	return NULL;
}


static struct wpa_bss *
wpa_supplicant_select_bss_non_wpa(struct wpa_supplicant *wpa_s,
				  struct wpa_scan_results *scan_res,
				  struct wpa_ssid *group,
				  struct wpa_ssid **selected_ssid)
{
	struct wpa_ssid *ssid;
	struct wpa_scan_res *bss;
	size_t i;
	struct wpa_blacklist *e;
	const u8 *ie;

	wpa_printf(MSG_DEBUG, "Try to find non-WPA AP");
	for (i = 0; i < scan_res->num; i++) {
		const u8 *ssid_;
		u8 wpa_ie_len, rsn_ie_len, ssid_len;
		bss = scan_res->res[i];

		ie = wpa_scan_get_ie(bss, WLAN_EID_SSID);
		ssid_ = ie ? ie + 2 : (u8 *) "";
		ssid_len = ie ? ie[1] : 0;

		ie = wpa_scan_get_vendor_ie(bss, WPA_IE_VENDOR_TYPE);
		wpa_ie_len = ie ? ie[1] : 0;

		ie = wpa_scan_get_ie(bss, WLAN_EID_RSN);
		rsn_ie_len = ie ? ie[1] : 0;

		wpa_printf(MSG_DEBUG, "%d: " MACSTR " ssid='%s' "
			   "wpa_ie_len=%u rsn_ie_len=%u caps=0x%x",
			   (int) i, MAC2STR(bss->bssid),
			   wpa_ssid_txt(ssid_, ssid_len),
			   wpa_ie_len, rsn_ie_len, bss->caps);

		e = wpa_blacklist_get(wpa_s, bss->bssid);
		if (e && e->count > 1) {
			wpa_printf(MSG_DEBUG, "   skip - blacklisted");
			continue;
		}

		if (ssid_len == 0) {
			wpa_printf(MSG_DEBUG, "   skip - SSID not known");
			continue;
		}

		for (ssid = group; ssid; ssid = ssid->pnext) {
			int check_ssid = ssid->ssid_len != 0;

			if (ssid->disabled) {
				wpa_printf(MSG_DEBUG, "   skip - disabled");
				continue;
			}

#ifdef CONFIG_WPS
			if (ssid->key_mgmt & WPA_KEY_MGMT_WPS) {
				/* Only allow wildcard SSID match if an AP
				 * advertises active WPS operation that matches
				 * with our mode. */
				check_ssid = 1;
				if (ssid->ssid_len == 0 &&
				    wpas_wps_ssid_wildcard_ok(wpa_s, ssid,
							      bss))
					check_ssid = 0;
			}
#endif /* CONFIG_WPS */

			if (check_ssid &&
			    (ssid_len != ssid->ssid_len ||
			     os_memcmp(ssid_, ssid->ssid, ssid_len) != 0)) {
				wpa_printf(MSG_DEBUG, "   skip - "
					   "SSID mismatch");
				continue;
			}

			if (ssid->bssid_set &&
			    os_memcmp(bss->bssid, ssid->bssid, ETH_ALEN) != 0)
			{
				wpa_printf(MSG_DEBUG, "   skip - "
					   "BSSID mismatch");
				continue;
			}

			if (!(ssid->key_mgmt & WPA_KEY_MGMT_NONE) &&
			    !(ssid->key_mgmt & WPA_KEY_MGMT_WPS) &&
			    !(ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA))
			{
				wpa_printf(MSG_DEBUG, "   skip - "
					   "non-WPA network not allowed");
				continue;
			}

			if ((ssid->key_mgmt &
			     (WPA_KEY_MGMT_IEEE8021X | WPA_KEY_MGMT_PSK |
			      WPA_KEY_MGMT_FT_IEEE8021X | WPA_KEY_MGMT_FT_PSK |
			      WPA_KEY_MGMT_IEEE8021X_SHA256 |
			      WPA_KEY_MGMT_PSK_SHA256)) &&
			    (wpa_ie_len != 0 || rsn_ie_len != 0)) {
				wpa_printf(MSG_DEBUG, "   skip - "
					   "WPA network");
				continue;
			}

			if (!wpa_supplicant_match_privacy(bss, ssid)) {
				wpa_printf(MSG_DEBUG, "   skip - "
					   "privacy mismatch");
				continue;
			}

			if (bss->caps & IEEE80211_CAP_IBSS) {
				wpa_printf(MSG_DEBUG, "   skip - "
					   "IBSS (adhoc) network");
				continue;
			}

			if (!freq_allowed(ssid->freq_list, bss->freq)) {
				wpa_printf(MSG_DEBUG, "   skip - "
					   "frequency not allowed");
				continue;
			}

			wpa_printf(MSG_DEBUG, "   selected non-WPA AP "
				   MACSTR " ssid='%s'",
				   MAC2STR(bss->bssid),
				   wpa_ssid_txt(ssid_, ssid_len));
			*selected_ssid = ssid;
			return wpa_bss_get(wpa_s, bss->bssid, ssid_, ssid_len);
		}
	}

	return NULL;
}


static struct wpa_bss *
wpa_supplicant_select_bss(struct wpa_supplicant *wpa_s,
			  struct wpa_scan_results *scan_res,
			  struct wpa_ssid *group,
			  struct wpa_ssid **selected_ssid)
{
	struct wpa_bss *selected;

	wpa_printf(MSG_DEBUG, "Selecting BSS from priority group %d",
		   group->priority);

	/* First, try to find WPA-enabled AP */
	selected = wpa_supplicant_select_bss_wpa(wpa_s, scan_res, group,
						 selected_ssid);
	if (selected)
		return selected;

	/* If no WPA-enabled AP found, try to find non-WPA AP, if configuration
	 * allows this. */
	return wpa_supplicant_select_bss_non_wpa(wpa_s, scan_res, group,
						 selected_ssid);
}


static struct wpa_bss *
wpa_supplicant_pick_network(struct wpa_supplicant *wpa_s,
			    struct wpa_scan_results *scan_res,
			    struct wpa_ssid **selected_ssid)
{
	struct wpa_bss *selected = NULL;
	int prio;

	while (selected == NULL) {
		for (prio = 0; prio < wpa_s->conf->num_prio; prio++) {
			selected = wpa_supplicant_select_bss(
				wpa_s, scan_res, wpa_s->conf->pssid[prio],
				selected_ssid);
			if (selected)
				break;
		}

		if (selected == NULL && wpa_s->blacklist) {
			wpa_printf(MSG_DEBUG, "No APs found - clear blacklist "
				   "and try again");
			wpa_blacklist_clear(wpa_s);
			wpa_s->blacklist_cleared++;
		} else if (selected == NULL)
			break;
	}

	return selected;
}


static void wpa_supplicant_req_new_scan(struct wpa_supplicant *wpa_s,
					int timeout_sec, int timeout_usec)
{
	if (!wpa_supplicant_enabled_networks(wpa_s->conf)) {
		/*
		 * No networks are enabled; short-circuit request so
		 * we don't wait timeout seconds before transitioning
		 * to INACTIVE state.
		 */
		wpa_supplicant_set_state(wpa_s, WPA_INACTIVE);
		return;
	}
	wpa_supplicant_req_scan(wpa_s, timeout_sec, timeout_usec);
}


void wpa_supplicant_connect(struct wpa_supplicant *wpa_s,
			    struct wpa_bss *selected,
			    struct wpa_ssid *ssid)
{
	if (wpas_wps_scan_pbc_overlap(wpa_s, selected, ssid)) {
		wpa_msg(wpa_s, MSG_INFO, WPS_EVENT_OVERLAP
			"PBC session overlap");
		wpa_supplicant_req_new_scan(wpa_s, 10, 0);
		return;
	}

	/*
	 * Do not trigger new association unless the BSSID has changed or if
	 * reassociation is requested. If we are in process of associating with
	 * the selected BSSID, do not trigger new attempt.
	 */
	if (wpa_s->reassociate ||
	    (os_memcmp(selected->bssid, wpa_s->bssid, ETH_ALEN) != 0 &&
	     (wpa_s->wpa_state != WPA_ASSOCIATING ||
	      os_memcmp(selected->bssid, wpa_s->pending_bssid, ETH_ALEN) !=
	      0))) {
		if (wpa_supplicant_scard_init(wpa_s, ssid)) {
			wpa_supplicant_req_new_scan(wpa_s, 10, 0);
			return;
		}
		wpa_supplicant_associate(wpa_s, selected, ssid);
	} else {
		wpa_printf(MSG_DEBUG, "Already associated with the selected "
			   "AP");
	}
}


static struct wpa_ssid *
wpa_supplicant_pick_new_network(struct wpa_supplicant *wpa_s)
{
	int prio;
	struct wpa_ssid *ssid;

	for (prio = 0; prio < wpa_s->conf->num_prio; prio++) {
		for (ssid = wpa_s->conf->pssid[prio]; ssid; ssid = ssid->pnext)
		{
			if (ssid->disabled)
				continue;
			if (ssid->mode == IEEE80211_MODE_IBSS ||
			    ssid->mode == IEEE80211_MODE_AP)
				return ssid;
		}
	}
	return NULL;
}


/* TODO: move the rsn_preauth_scan_result*() to be called from notify.c based
 * on BSS added and BSS changed events */
static void wpa_supplicant_rsn_preauth_scan_results(
	struct wpa_supplicant *wpa_s, struct wpa_scan_results *scan_res)
{
	int i;

	if (rsn_preauth_scan_results(wpa_s->wpa) < 0)
		return;

	for (i = scan_res->num - 1; i >= 0; i--) {
		const u8 *ssid, *rsn;
		struct wpa_scan_res *r;

		r = scan_res->res[i];

		ssid = wpa_scan_get_ie(r, WLAN_EID_SSID);
		if (ssid == NULL)
			continue;

		rsn = wpa_scan_get_ie(r, WLAN_EID_RSN);
		if (rsn == NULL)
			continue;

		rsn_preauth_scan_result(wpa_s->wpa, r->bssid, ssid, rsn);
	}

}


static int wpa_supplicant_need_to_roam(struct wpa_supplicant *wpa_s,
				       struct wpa_bss *selected,
				       struct wpa_ssid *ssid,
				       struct wpa_scan_results *scan_res)
{
	size_t i;
	struct wpa_scan_res *current_bss = NULL;
	int min_diff;

	if (wpa_s->reassociate)
		return 1; /* explicit request to reassociate */
	if (wpa_s->wpa_state < WPA_ASSOCIATED)
		return 1; /* we are not associated; continue */
	if (wpa_s->current_ssid == NULL)
		return 1; /* unknown current SSID */
	if (wpa_s->current_ssid != ssid)
		return 1; /* different network block */

	for (i = 0; i < scan_res->num; i++) {
		struct wpa_scan_res *res = scan_res->res[i];
		const u8 *ie;
		if (os_memcmp(res->bssid, wpa_s->bssid, ETH_ALEN) != 0)
			continue;

		ie = wpa_scan_get_ie(res, WLAN_EID_SSID);
		if (ie == NULL)
			continue;
		if (ie[1] != wpa_s->current_ssid->ssid_len ||
		    os_memcmp(ie + 2, wpa_s->current_ssid->ssid, ie[1]) != 0)
			continue;
		current_bss = res;
		break;
	}

	if (!current_bss)
		return 1; /* current BSS not seen in scan results */

	wpa_printf(MSG_DEBUG, "Considering within-ESS reassociation");
	wpa_printf(MSG_DEBUG, "Current BSS: " MACSTR " level=%d",
		   MAC2STR(current_bss->bssid), current_bss->level);
	wpa_printf(MSG_DEBUG, "Selected BSS: " MACSTR " level=%d",
		   MAC2STR(selected->bssid), selected->level);

	if (wpa_s->current_ssid->bssid_set &&
	    os_memcmp(selected->bssid, wpa_s->current_ssid->bssid, ETH_ALEN) ==
	    0) {
		wpa_printf(MSG_DEBUG, "Allow reassociation - selected BSS has "
			   "preferred BSSID");
		return 1;
	}

	min_diff = 2;
	if (current_bss->level < 0) {
		if (current_bss->level < -85)
			min_diff = 1;
		else if (current_bss->level < -80)
			min_diff = 2;
		else if (current_bss->level < -75)
			min_diff = 3;
		else if (current_bss->level < -70)
			min_diff = 4;
		else
			min_diff = 5;
	}
	if (abs(current_bss->level - selected->level) < min_diff) {
		wpa_printf(MSG_DEBUG, "Skip roam - too small difference in "
			   "signal level");
		return 0;
	}

	return 1;
}


static void wpa_supplicant_event_scan_results(struct wpa_supplicant *wpa_s,
					      union wpa_event_data *data)
{
	struct wpa_bss *selected;
	struct wpa_ssid *ssid = NULL;
	struct wpa_scan_results *scan_res;
	int ap = 0;

#ifdef CONFIG_AP
	if (wpa_s->ap_iface)
		ap = 1;
#endif /* CONFIG_AP */

	wpa_supplicant_notify_scanning(wpa_s, 0);

	scan_res = wpa_supplicant_get_scan_results(wpa_s,
						   data ? &data->scan_info :
						   NULL, 1);
	if (scan_res == NULL) {
		if (wpa_s->conf->ap_scan == 2 || ap)
			return;
		wpa_printf(MSG_DEBUG, "Failed to get scan results - try "
			   "scanning again");
		wpa_supplicant_req_new_scan(wpa_s, 1, 0);
		return;
	}

	if (wpa_s->scan_res_handler) {
		wpa_s->scan_res_handler(wpa_s, scan_res);
		wpa_s->scan_res_handler = NULL;
		wpa_scan_results_free(scan_res);
		return;
	}

	if (ap) {
		wpa_printf(MSG_DEBUG, "Ignore scan results in AP mode");
		wpa_scan_results_free(scan_res);
		return;
	}

	wpa_printf(MSG_DEBUG, "New scan results available");
	wpa_msg_ctrl(wpa_s, MSG_INFO, WPA_EVENT_SCAN_RESULTS);
	wpas_notify_scan_results(wpa_s);

	wpas_notify_scan_done(wpa_s, 1);

	if ((wpa_s->conf->ap_scan == 2 && !wpas_wps_searching(wpa_s))) {
		wpa_scan_results_free(scan_res);
		return;
	}

	if (wpa_s->disconnected) {
		wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
		wpa_scan_results_free(scan_res);
		return;
	}

	if (bgscan_notify_scan(wpa_s) == 1) {
		wpa_scan_results_free(scan_res);
		return;
	}

	wpa_supplicant_rsn_preauth_scan_results(wpa_s, scan_res);

	selected = wpa_supplicant_pick_network(wpa_s, scan_res, &ssid);

	if (selected) {
		int skip;
		skip = !wpa_supplicant_need_to_roam(wpa_s, selected, ssid,
						    scan_res);
		wpa_scan_results_free(scan_res);
		if (skip)
			return;
		wpa_supplicant_connect(wpa_s, selected, ssid);
	} else {
		wpa_scan_results_free(scan_res);
		wpa_printf(MSG_DEBUG, "No suitable network found");
		ssid = wpa_supplicant_pick_new_network(wpa_s);
		if (ssid) {
			wpa_printf(MSG_DEBUG, "Setup a new network");
			wpa_supplicant_associate(wpa_s, NULL, ssid);
		} else {
			int timeout_sec = 5;
			int timeout_usec = 0;
			wpa_supplicant_req_new_scan(wpa_s, timeout_sec,
						    timeout_usec);
		}
	}
}
#endif /* CONFIG_NO_SCAN_PROCESSING */


static int wpa_supplicant_event_associnfo(struct wpa_supplicant *wpa_s,
					  union wpa_event_data *data)
{
	int l, len, found = 0, wpa_found, rsn_found;
	const u8 *p;

	wpa_printf(MSG_DEBUG, "Association info event");
	if (data->assoc_info.req_ies)
		wpa_hexdump(MSG_DEBUG, "req_ies", data->assoc_info.req_ies,
			    data->assoc_info.req_ies_len);
	if (data->assoc_info.resp_ies)
		wpa_hexdump(MSG_DEBUG, "resp_ies", data->assoc_info.resp_ies,
			    data->assoc_info.resp_ies_len);
	if (data->assoc_info.beacon_ies)
		wpa_hexdump(MSG_DEBUG, "beacon_ies",
			    data->assoc_info.beacon_ies,
			    data->assoc_info.beacon_ies_len);
	if (data->assoc_info.freq)
		wpa_printf(MSG_DEBUG, "freq=%u MHz", data->assoc_info.freq);

	p = data->assoc_info.req_ies;
	l = data->assoc_info.req_ies_len;

	/* Go through the IEs and make a copy of the WPA/RSN IE, if present. */
	while (p && l >= 2) {
		len = p[1] + 2;
		if (len > l) {
			wpa_hexdump(MSG_DEBUG, "Truncated IE in assoc_info",
				    p, l);
			break;
		}
		if ((p[0] == WLAN_EID_VENDOR_SPECIFIC && p[1] >= 6 &&
		     (os_memcmp(&p[2], "\x00\x50\xF2\x01\x01\x00", 6) == 0)) ||
		    (p[0] == WLAN_EID_RSN && p[1] >= 2)) {
			if (wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, p, len))
				break;
			found = 1;
			wpa_find_assoc_pmkid(wpa_s);
			break;
		}
		l -= len;
		p += len;
	}
	if (!found && data->assoc_info.req_ies)
		wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, NULL, 0);

#ifdef CONFIG_IEEE80211R
#ifdef CONFIG_SME
	if (wpa_s->sme.auth_alg == WPA_AUTH_ALG_FT) {
		u8 bssid[ETH_ALEN];
		if (wpa_drv_get_bssid(wpa_s, bssid) < 0 ||
		    wpa_ft_validate_reassoc_resp(wpa_s->wpa,
						 data->assoc_info.resp_ies,
						 data->assoc_info.resp_ies_len,
						 bssid) < 0) {
			wpa_printf(MSG_DEBUG, "FT: Validation of "
				   "Reassociation Response failed");
			wpa_supplicant_deauthenticate(
				wpa_s, WLAN_REASON_INVALID_IE);
			return -1;
		}
	}

	p = data->assoc_info.resp_ies;
	l = data->assoc_info.resp_ies_len;

	/* Go through the IEs and make a copy of the MDIE, if present. */
	while (p && l >= 2) {
		len = p[1] + 2;
		if (len > l) {
			wpa_hexdump(MSG_DEBUG, "Truncated IE in assoc_info",
				    p, l);
			break;
		}
		if (p[0] == WLAN_EID_MOBILITY_DOMAIN &&
		    p[1] >= MOBILITY_DOMAIN_ID_LEN) {
			wpa_s->sme.ft_used = 1;
			os_memcpy(wpa_s->sme.mobility_domain, p + 2,
				  MOBILITY_DOMAIN_ID_LEN);
			break;
		}
		l -= len;
		p += len;
	}
#endif /* CONFIG_SME */

	wpa_sm_set_ft_params(wpa_s->wpa, data->assoc_info.resp_ies,
			     data->assoc_info.resp_ies_len);
#endif /* CONFIG_IEEE80211R */

	/* WPA/RSN IE from Beacon/ProbeResp */
	p = data->assoc_info.beacon_ies;
	l = data->assoc_info.beacon_ies_len;

	/* Go through the IEs and make a copy of the WPA/RSN IEs, if present.
	 */
	wpa_found = rsn_found = 0;
	while (p && l >= 2) {
		len = p[1] + 2;
		if (len > l) {
			wpa_hexdump(MSG_DEBUG, "Truncated IE in beacon_ies",
				    p, l);
			break;
		}
		if (!wpa_found &&
		    p[0] == WLAN_EID_VENDOR_SPECIFIC && p[1] >= 6 &&
		    os_memcmp(&p[2], "\x00\x50\xF2\x01\x01\x00", 6) == 0) {
			wpa_found = 1;
			wpa_sm_set_ap_wpa_ie(wpa_s->wpa, p, len);
		}

		if (!rsn_found &&
		    p[0] == WLAN_EID_RSN && p[1] >= 2) {
			rsn_found = 1;
			wpa_sm_set_ap_rsn_ie(wpa_s->wpa, p, len);
		}

		l -= len;
		p += len;
	}

	if (!wpa_found && data->assoc_info.beacon_ies)
		wpa_sm_set_ap_wpa_ie(wpa_s->wpa, NULL, 0);
	if (!rsn_found && data->assoc_info.beacon_ies)
		wpa_sm_set_ap_rsn_ie(wpa_s->wpa, NULL, 0);
	if (wpa_found || rsn_found)
		wpa_s->ap_ies_from_associnfo = 1;

	wpa_s->assoc_freq = data->assoc_info.freq;

	return 0;
}


static void wpa_supplicant_event_assoc(struct wpa_supplicant *wpa_s,
				       union wpa_event_data *data)
{
	u8 bssid[ETH_ALEN];
	int ft_completed;
	int bssid_changed;
	struct wpa_driver_capa capa;

#ifdef CONFIG_AP
	if (wpa_s->ap_iface) {
		hostapd_notif_assoc(wpa_s->ap_iface->bss[0],
				    data->assoc_info.addr,
				    data->assoc_info.req_ies,
				    data->assoc_info.req_ies_len);
		return;
	}
#endif /* CONFIG_AP */

	ft_completed = wpa_ft_is_completed(wpa_s->wpa);
	if (data && wpa_supplicant_event_associnfo(wpa_s, data) < 0)
		return;

	wpa_supplicant_set_state(wpa_s, WPA_ASSOCIATED);
	if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_USER_SPACE_MLME)
		os_memcpy(bssid, wpa_s->bssid, ETH_ALEN);
	if ((wpa_s->drv_flags & WPA_DRIVER_FLAGS_USER_SPACE_MLME) ||
	    (wpa_drv_get_bssid(wpa_s, bssid) >= 0 &&
	     os_memcmp(bssid, wpa_s->bssid, ETH_ALEN) != 0)) {
		wpa_msg(wpa_s, MSG_DEBUG, "Associated to a new BSS: BSSID="
			MACSTR, MAC2STR(bssid));
		bssid_changed = os_memcmp(wpa_s->bssid, bssid, ETH_ALEN);
		os_memcpy(wpa_s->bssid, bssid, ETH_ALEN);
		os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
		if (bssid_changed)
			wpas_notify_bssid_changed(wpa_s);

		if (wpa_supplicant_dynamic_keys(wpa_s) && !ft_completed) {
			wpa_clear_keys(wpa_s, bssid);
		}
		if (wpa_supplicant_select_config(wpa_s) < 0) {
			wpa_supplicant_disassociate(
				wpa_s, WLAN_REASON_DEAUTH_LEAVING);
			return;
		}
		if (wpa_s->current_ssid) {
			struct wpa_bss *bss = NULL;
			struct wpa_ssid *ssid = wpa_s->current_ssid;
			if (ssid->ssid_len > 0)
				bss = wpa_bss_get(wpa_s, bssid,
						  ssid->ssid, ssid->ssid_len);
			if (!bss)
				bss = wpa_bss_get_bssid(wpa_s, bssid);
			if (bss)
				wpa_s->current_bss = bss;
		}
	}

#ifdef CONFIG_SME
	os_memcpy(wpa_s->sme.prev_bssid, bssid, ETH_ALEN);
	wpa_s->sme.prev_bssid_set = 1;
#endif /* CONFIG_SME */

	wpa_msg(wpa_s, MSG_INFO, "Associated with " MACSTR, MAC2STR(bssid));
	if (wpa_s->current_ssid) {
		/* When using scanning (ap_scan=1), SIM PC/SC interface can be
		 * initialized before association, but for other modes,
		 * initialize PC/SC here, if the current configuration needs
		 * smartcard or SIM/USIM. */
		wpa_supplicant_scard_init(wpa_s, wpa_s->current_ssid);
	}
	wpa_sm_notify_assoc(wpa_s->wpa, bssid);
	if (wpa_s->l2)
		l2_packet_notify_auth_start(wpa_s->l2);

	/*
	 * Set portEnabled first to FALSE in order to get EAP state machine out
	 * of the SUCCESS state and eapSuccess cleared. Without this, EAPOL PAE
	 * state machine may transit to AUTHENTICATING state based on obsolete
	 * eapSuccess and then trigger BE_AUTH to SUCCESS and PAE to
	 * AUTHENTICATED without ever giving chance to EAP state machine to
	 * reset the state.
	 */
	if (!ft_completed) {
		eapol_sm_notify_portEnabled(wpa_s->eapol, FALSE);
		eapol_sm_notify_portValid(wpa_s->eapol, FALSE);
	}
	if (wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt) || ft_completed)
		eapol_sm_notify_eap_success(wpa_s->eapol, FALSE);
	/* 802.1X::portControl = Auto */
	eapol_sm_notify_portEnabled(wpa_s->eapol, TRUE);
	wpa_s->eapol_received = 0;
	if (wpa_s->key_mgmt == WPA_KEY_MGMT_NONE ||
	    wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE ||
	    (wpa_s->current_ssid &&
	     wpa_s->current_ssid->mode == IEEE80211_MODE_IBSS)) {
		wpa_supplicant_cancel_auth_timeout(wpa_s);
		wpa_supplicant_set_state(wpa_s, WPA_COMPLETED);
	} else if (!ft_completed) {
		/* Timeout for receiving the first EAPOL packet */
		wpa_supplicant_req_auth_timeout(wpa_s, 10, 0);
	}
	wpa_supplicant_cancel_scan(wpa_s);

	if ((wpa_s->drv_flags & WPA_DRIVER_FLAGS_4WAY_HANDSHAKE) &&
	    wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt)) {
		/*
		 * We are done; the driver will take care of RSN 4-way
		 * handshake.
		 */
		wpa_supplicant_cancel_auth_timeout(wpa_s);
		wpa_supplicant_set_state(wpa_s, WPA_COMPLETED);
		eapol_sm_notify_portValid(wpa_s->eapol, TRUE);
		eapol_sm_notify_eap_success(wpa_s->eapol, TRUE);
	}

	if (wpa_s->pending_eapol_rx) {
		struct os_time now, age;
		os_get_time(&now);
		os_time_sub(&now, &wpa_s->pending_eapol_rx_time, &age);
		if (age.sec == 0 && age.usec < 100000 &&
		    os_memcmp(wpa_s->pending_eapol_rx_src, bssid, ETH_ALEN) ==
		    0) {
			wpa_printf(MSG_DEBUG, "Process pending EAPOL frame "
				   "that was received just before association "
				   "notification");
			wpa_supplicant_rx_eapol(
				wpa_s, wpa_s->pending_eapol_rx_src,
				wpabuf_head(wpa_s->pending_eapol_rx),
				wpabuf_len(wpa_s->pending_eapol_rx));
		}
		wpabuf_free(wpa_s->pending_eapol_rx);
		wpa_s->pending_eapol_rx = NULL;
	}

#ifdef CONFIG_BGSCAN
	if (wpa_s->current_ssid != wpa_s->bgscan_ssid) {
		bgscan_deinit(wpa_s);
		if (wpa_s->current_ssid && wpa_s->current_ssid->bgscan) {
			if (bgscan_init(wpa_s, wpa_s->current_ssid)) {
				wpa_printf(MSG_DEBUG, "Failed to initialize "
					   "bgscan");
				/*
				 * Live without bgscan; it is only used as a
				 * roaming optimization, so the initial
				 * connection is not affected.
				 */
			} else
				wpa_s->bgscan_ssid = wpa_s->current_ssid;
		} else
			wpa_s->bgscan_ssid = NULL;
	}
#endif /* CONFIG_BGSCAN */

	if ((wpa_s->key_mgmt == WPA_KEY_MGMT_NONE ||
	     wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA) &&
	    wpa_s->current_ssid && wpa_drv_get_capa(wpa_s, &capa) == 0 &&
	    capa.flags & WPA_DRIVER_FLAGS_SET_KEYS_AFTER_ASSOC_DONE) {
		/* Set static WEP keys again */
		wpa_set_wep_keys(wpa_s, wpa_s->current_ssid);
	}
}


static void wpa_supplicant_event_disassoc(struct wpa_supplicant *wpa_s,
					  u16 reason_code)
{
	const u8 *bssid;
#ifdef CONFIG_SME
	int authenticating;
	u8 prev_pending_bssid[ETH_ALEN];

	authenticating = wpa_s->wpa_state == WPA_AUTHENTICATING;
	os_memcpy(prev_pending_bssid, wpa_s->pending_bssid, ETH_ALEN);
#endif /* CONFIG_SME */

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE) {
		/*
		 * At least Host AP driver and a Prism3 card seemed to be
		 * generating streams of disconnected events when configuring
		 * IBSS for WPA-None. Ignore them for now.
		 */
		wpa_printf(MSG_DEBUG, "Disconnect event - ignore in "
			   "IBSS/WPA-None mode");
		return;
	}

	if (wpa_s->wpa_state == WPA_4WAY_HANDSHAKE &&
	    wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt)) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: 4-Way Handshake failed - "
			"pre-shared key may be incorrect");
	}
	if (wpa_s->wpa_state >= WPA_ASSOCIATED)
		wpa_supplicant_req_scan(wpa_s, 0, 100000);
	bssid = wpa_s->bssid;
	if (is_zero_ether_addr(bssid))
		bssid = wpa_s->pending_bssid;
	wpa_blacklist_add(wpa_s, bssid);
	wpa_sm_notify_disassoc(wpa_s->wpa);
	wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DISCONNECTED "bssid=" MACSTR
		" reason=%d",
		MAC2STR(bssid), reason_code);
	if (wpa_supplicant_dynamic_keys(wpa_s)) {
		wpa_printf(MSG_DEBUG, "Disconnect event - remove keys");
		wpa_s->keys_cleared = 0;
		wpa_clear_keys(wpa_s, wpa_s->bssid);
	}
	wpa_supplicant_mark_disassoc(wpa_s);
	bgscan_deinit(wpa_s);
	wpa_s->bgscan_ssid = NULL;
#ifdef CONFIG_SME
	if (authenticating &&
	    (wpa_s->drv_flags & WPA_DRIVER_FLAGS_SME)) {
		/*
		 * mac80211-workaround to force deauth on failed auth cmd,
		 * requires us to remain in authenticating state to allow the
		 * second authentication attempt to be continued properly.
		 */
		wpa_printf(MSG_DEBUG, "SME: Allow pending authentication to "
			   "proceed after disconnection event");
		wpa_supplicant_set_state(wpa_s, WPA_AUTHENTICATING);
		os_memcpy(wpa_s->pending_bssid, prev_pending_bssid, ETH_ALEN);
	}
#endif /* CONFIG_SME */
}


#ifdef CONFIG_DELAYED_MIC_ERROR_REPORT
static void wpa_supplicant_delayed_mic_error_report(void *eloop_ctx,
						    void *sock_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	if (!wpa_s->pending_mic_error_report)
		return;

	wpa_printf(MSG_DEBUG, "WPA: Sending pending MIC error report");
	wpa_sm_key_request(wpa_s->wpa, 1, wpa_s->pending_mic_error_pairwise);
	wpa_s->pending_mic_error_report = 0;
}
#endif /* CONFIG_DELAYED_MIC_ERROR_REPORT */


static void
wpa_supplicant_event_michael_mic_failure(struct wpa_supplicant *wpa_s,
					 union wpa_event_data *data)
{
	int pairwise;
	struct os_time t;

	wpa_msg(wpa_s, MSG_WARNING, "Michael MIC failure detected");
	pairwise = (data && data->michael_mic_failure.unicast);
	os_get_time(&t);
	if ((wpa_s->last_michael_mic_error &&
	     t.sec - wpa_s->last_michael_mic_error <= 60) ||
	    wpa_s->pending_mic_error_report) {
		if (wpa_s->pending_mic_error_report) {
			/*
			 * Send the pending MIC error report immediately since
			 * we are going to start countermeasures and AP better
			 * do the same.
			 */
			wpa_sm_key_request(wpa_s->wpa, 1,
					   wpa_s->pending_mic_error_pairwise);
		}

		/* Send the new MIC error report immediately since we are going
		 * to start countermeasures and AP better do the same.
		 */
		wpa_sm_key_request(wpa_s->wpa, 1, pairwise);

		/* initialize countermeasures */
		wpa_s->countermeasures = 1;
		wpa_msg(wpa_s, MSG_WARNING, "TKIP countermeasures started");

		/*
		 * Need to wait for completion of request frame. We do not get
		 * any callback for the message completion, so just wait a
		 * short while and hope for the best. */
		os_sleep(0, 10000);

		wpa_drv_set_countermeasures(wpa_s, 1);
		wpa_supplicant_deauthenticate(wpa_s,
					      WLAN_REASON_MICHAEL_MIC_FAILURE);
		eloop_cancel_timeout(wpa_supplicant_stop_countermeasures,
				     wpa_s, NULL);
		eloop_register_timeout(60, 0,
				       wpa_supplicant_stop_countermeasures,
				       wpa_s, NULL);
		/* TODO: mark the AP rejected for 60 second. STA is
		 * allowed to associate with another AP.. */
	} else {
#ifdef CONFIG_DELAYED_MIC_ERROR_REPORT
		if (wpa_s->mic_errors_seen) {
			/*
			 * Reduce the effectiveness of Michael MIC error
			 * reports as a means for attacking against TKIP if
			 * more than one MIC failure is noticed with the same
			 * PTK. We delay the transmission of the reports by a
			 * random time between 0 and 60 seconds in order to
			 * force the attacker wait 60 seconds before getting
			 * the information on whether a frame resulted in a MIC
			 * failure.
			 */
			u8 rval[4];
			int sec;

			if (os_get_random(rval, sizeof(rval)) < 0)
				sec = os_random() % 60;
			else
				sec = WPA_GET_BE32(rval) % 60;
			wpa_printf(MSG_DEBUG, "WPA: Delay MIC error report %d "
				   "seconds", sec);
			wpa_s->pending_mic_error_report = 1;
			wpa_s->pending_mic_error_pairwise = pairwise;
			eloop_cancel_timeout(
				wpa_supplicant_delayed_mic_error_report,
				wpa_s, NULL);
			eloop_register_timeout(
				sec, os_random() % 1000000,
				wpa_supplicant_delayed_mic_error_report,
				wpa_s, NULL);
		} else {
			wpa_sm_key_request(wpa_s->wpa, 1, pairwise);
		}
#else /* CONFIG_DELAYED_MIC_ERROR_REPORT */
		wpa_sm_key_request(wpa_s->wpa, 1, pairwise);
#endif /* CONFIG_DELAYED_MIC_ERROR_REPORT */
	}
	wpa_s->last_michael_mic_error = t.sec;
	wpa_s->mic_errors_seen++;
}


#ifdef CONFIG_TERMINATE_ONLASTIF
static int any_interfaces(struct wpa_supplicant *head)
{
	struct wpa_supplicant *wpa_s;

	for (wpa_s = head; wpa_s != NULL; wpa_s = wpa_s->next)
		if (!wpa_s->interface_removed)
			return 1;
	return 0;
}
#endif /* CONFIG_TERMINATE_ONLASTIF */


static void
wpa_supplicant_event_interface_status(struct wpa_supplicant *wpa_s,
				      union wpa_event_data *data)
{
	if (os_strcmp(wpa_s->ifname, data->interface_status.ifname) != 0)
		return;

	switch (data->interface_status.ievent) {
	case EVENT_INTERFACE_ADDED:
		if (!wpa_s->interface_removed)
			break;
		wpa_s->interface_removed = 0;
		wpa_printf(MSG_DEBUG, "Configured interface was added.");
		if (wpa_supplicant_driver_init(wpa_s) < 0) {
			wpa_printf(MSG_INFO, "Failed to initialize the driver "
				   "after interface was added.");
		}
		break;
	case EVENT_INTERFACE_REMOVED:
		wpa_printf(MSG_DEBUG, "Configured interface was removed.");
		wpa_s->interface_removed = 1;
		wpa_supplicant_mark_disassoc(wpa_s);
		l2_packet_deinit(wpa_s->l2);
		wpa_s->l2 = NULL;
#ifdef CONFIG_TERMINATE_ONLASTIF
		/* check if last interface */
		if (!any_interfaces(wpa_s->global->ifaces))
			eloop_terminate();
#endif /* CONFIG_TERMINATE_ONLASTIF */
		break;
	}
}


#ifdef CONFIG_PEERKEY
static void
wpa_supplicant_event_stkstart(struct wpa_supplicant *wpa_s,
			      union wpa_event_data *data)
{
	if (data == NULL)
		return;
	wpa_sm_stkstart(wpa_s->wpa, data->stkstart.peer);
}
#endif /* CONFIG_PEERKEY */


#ifdef CONFIG_IEEE80211R
static void
wpa_supplicant_event_ft_response(struct wpa_supplicant *wpa_s,
				 union wpa_event_data *data)
{
	if (data == NULL)
		return;

	if (wpa_ft_process_response(wpa_s->wpa, data->ft_ies.ies,
				    data->ft_ies.ies_len,
				    data->ft_ies.ft_action,
				    data->ft_ies.target_ap,
				    data->ft_ies.ric_ies,
				    data->ft_ies.ric_ies_len) < 0) {
		/* TODO: prevent MLME/driver from trying to associate? */
	}
}
#endif /* CONFIG_IEEE80211R */


#ifdef CONFIG_IBSS_RSN
static void wpa_supplicant_event_ibss_rsn_start(struct wpa_supplicant *wpa_s,
						union wpa_event_data *data)
{
	if (data == NULL)
		return;
	ibss_rsn_start(wpa_s->ibss_rsn, data->ibss_rsn_start.peer);
}
#endif /* CONFIG_IBSS_RSN */


#ifdef CONFIG_IEEE80211R
static void ft_rx_action(struct wpa_supplicant *wpa_s, const u8 *data,
			 size_t len)
{
	const u8 *sta_addr, *target_ap_addr;
	u16 status;

	wpa_hexdump(MSG_MSGDUMP, "FT: RX Action", data, len);
	if (!(wpa_s->drv_flags & WPA_DRIVER_FLAGS_SME))
		return; /* only SME case supported for now */
	if (len < 1 + 2 * ETH_ALEN + 2)
		return;
	if (data[0] != 2)
		return; /* Only FT Action Response is supported for now */
	sta_addr = data + 1;
	target_ap_addr = data + 1 + ETH_ALEN;
	status = WPA_GET_LE16(data + 1 + 2 * ETH_ALEN);
	wpa_printf(MSG_DEBUG, "FT: Received FT Action Response: STA " MACSTR
		   " TargetAP " MACSTR " status %u",
		   MAC2STR(sta_addr), MAC2STR(target_ap_addr), status);

	if (os_memcmp(sta_addr, wpa_s->own_addr, ETH_ALEN) != 0) {
		wpa_printf(MSG_DEBUG, "FT: Foreign STA Address " MACSTR
			   " in FT Action Response", MAC2STR(sta_addr));
		return;
	}

	if (status) {
		wpa_printf(MSG_DEBUG, "FT: FT Action Response indicates "
			   "failure (status code %d)", status);
		/* TODO: report error to FT code(?) */
		return;
	}

	if (wpa_ft_process_response(wpa_s->wpa, data + 1 + 2 * ETH_ALEN + 2,
				    len - (1 + 2 * ETH_ALEN + 2), 1,
				    target_ap_addr, NULL, 0) < 0)
		return;

#ifdef CONFIG_SME
	{
		struct wpa_bss *bss;
		bss = wpa_bss_get_bssid(wpa_s, target_ap_addr);
		if (bss)
			wpa_s->sme.freq = bss->freq;
		wpa_s->sme.auth_alg = WPA_AUTH_ALG_FT;
		sme_associate(wpa_s, WPAS_MODE_INFRA, target_ap_addr,
			      WLAN_AUTH_FT);
	}
#endif /* CONFIG_SME */
}
#endif /* CONFIG_IEEE80211R */


void wpa_supplicant_event(void *ctx, enum wpa_event_type event,
			  union wpa_event_data *data)
{
	struct wpa_supplicant *wpa_s = ctx;
	u16 reason_code = 0;

	switch (event) {
	case EVENT_AUTH:
		sme_event_auth(wpa_s, data);
		break;
	case EVENT_ASSOC:
		wpa_supplicant_event_assoc(wpa_s, data);
		break;
	case EVENT_DISASSOC:
		wpa_printf(MSG_DEBUG, "Disassociation notification");
#ifdef CONFIG_AP
		if (wpa_s->ap_iface && data && data->disassoc_info.addr) {
			hostapd_notif_disassoc(wpa_s->ap_iface->bss[0],
					       data->disassoc_info.addr);
			break;
		}
#endif /* CONFIG_AP */
		if (data)
			reason_code = data->deauth_info.reason_code;
		if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_SME)
			sme_event_disassoc(wpa_s, data);
		/* fall through */
	case EVENT_DEAUTH:
		if (event == EVENT_DEAUTH) {
			wpa_printf(MSG_DEBUG, "Deauthentication notification");
			if (data)
				reason_code = data->deauth_info.reason_code;
		}
#ifdef CONFIG_AP
		if (wpa_s->ap_iface && data && data->deauth_info.addr) {
			hostapd_notif_disassoc(wpa_s->ap_iface->bss[0],
					       data->deauth_info.addr);
			break;
		}
#endif /* CONFIG_AP */
		wpa_supplicant_event_disassoc(wpa_s, reason_code);
		break;
	case EVENT_MICHAEL_MIC_FAILURE:
		wpa_supplicant_event_michael_mic_failure(wpa_s, data);
		break;
#ifndef CONFIG_NO_SCAN_PROCESSING
	case EVENT_SCAN_RESULTS:
		wpa_supplicant_event_scan_results(wpa_s, data);
		break;
#endif /* CONFIG_NO_SCAN_PROCESSING */
	case EVENT_ASSOCINFO:
		wpa_supplicant_event_associnfo(wpa_s, data);
		break;
	case EVENT_INTERFACE_STATUS:
		wpa_supplicant_event_interface_status(wpa_s, data);
		break;
	case EVENT_PMKID_CANDIDATE:
		wpa_supplicant_event_pmkid_candidate(wpa_s, data);
		break;
#ifdef CONFIG_PEERKEY
	case EVENT_STKSTART:
		wpa_supplicant_event_stkstart(wpa_s, data);
		break;
#endif /* CONFIG_PEERKEY */
#ifdef CONFIG_IEEE80211R
	case EVENT_FT_RESPONSE:
		wpa_supplicant_event_ft_response(wpa_s, data);
		break;
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_IBSS_RSN
	case EVENT_IBSS_RSN_START:
		wpa_supplicant_event_ibss_rsn_start(wpa_s, data);
		break;
#endif /* CONFIG_IBSS_RSN */
	case EVENT_ASSOC_REJECT:
		sme_event_assoc_reject(wpa_s, data);
		break;
	case EVENT_AUTH_TIMED_OUT:
		sme_event_auth_timed_out(wpa_s, data);
		break;
	case EVENT_ASSOC_TIMED_OUT:
		sme_event_assoc_timed_out(wpa_s, data);
		break;
#ifdef CONFIG_AP
	case EVENT_TX_STATUS:
		if (wpa_s->ap_iface == NULL)
			break;
		switch (data->tx_status.type) {
		case WLAN_FC_TYPE_MGMT:
			ap_mgmt_tx_cb(wpa_s, data->tx_status.data,
				      data->tx_status.data_len,
				      data->tx_status.stype,
				      data->tx_status.ack);
			break;
		case WLAN_FC_TYPE_DATA:
			ap_tx_status(wpa_s, data->tx_status.dst,
				     data->tx_status.data,
				     data->tx_status.data_len,
				     data->tx_status.ack);
			break;
		}
		break;
	case EVENT_RX_FROM_UNKNOWN:
		if (wpa_s->ap_iface == NULL)
			break;
		ap_rx_from_unknown_sta(wpa_s, data->rx_from_unknown.frame,
				       data->rx_from_unknown.len);
		break;
	case EVENT_RX_MGMT:
		if (wpa_s->ap_iface == NULL)
			break;
		ap_mgmt_rx(wpa_s, &data->rx_mgmt);
		break;
#endif /* CONFIG_AP */
	case EVENT_RX_ACTION:
		wpa_printf(MSG_DEBUG, "Received Action frame: SA=" MACSTR
			   " Category=%u DataLen=%d freq=%d MHz",
			   MAC2STR(data->rx_action.sa),
			   data->rx_action.category, (int) data->rx_action.len,
			   data->rx_action.freq);
#ifdef CONFIG_IEEE80211R
		if (data->rx_action.category == WLAN_ACTION_FT) {
			ft_rx_action(wpa_s, data->rx_action.data,
				     data->rx_action.len);
			break;
		}
#endif /* CONFIG_IEEE80211R */
		break;
#ifdef CONFIG_CLIENT_MLME
	case EVENT_MLME_RX: {
		struct ieee80211_rx_status rx_status;
		os_memset(&rx_status, 0, sizeof(rx_status));
		rx_status.freq = data->mlme_rx.freq;
		rx_status.channel = data->mlme_rx.channel;
		rx_status.ssi = data->mlme_rx.ssi;
		ieee80211_sta_rx(wpa_s, data->mlme_rx.buf, data->mlme_rx.len,
				 &rx_status);
		break;
	}
#endif /* CONFIG_CLIENT_MLME */
	case EVENT_EAPOL_RX:
		wpa_supplicant_rx_eapol(wpa_s, data->eapol_rx.src,
					data->eapol_rx.data,
					data->eapol_rx.data_len);
		break;
	case EVENT_SIGNAL_CHANGE:
		bgscan_notify_signal_change(
			wpa_s, data->signal_change.above_threshold);
		break;
	default:
		wpa_printf(MSG_INFO, "Unknown event %d", event);
		break;
	}
}
