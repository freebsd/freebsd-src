/*
 * WPA Supplicant - Driver event processing
 * Copyright (c) 2003-2012, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
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
#include "p2p/p2p.h"
#include "wnm_sta.h"
#include "notify.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "crypto/random.h"
#include "blacklist.h"
#include "wpas_glue.h"
#include "wps_supplicant.h"
#include "ibss_rsn.h"
#include "sme.h"
#include "gas_query.h"
#include "p2p_supplicant.h"
#include "bgscan.h"
#include "autoscan.h"
#include "ap.h"
#include "bss.h"
#include "scan.h"
#include "offchannel.h"
#include "interworking.h"


static int wpas_temp_disabled(struct wpa_supplicant *wpa_s,
			      struct wpa_ssid *ssid)
{
	struct os_time now;

	if (ssid == NULL || ssid->disabled_until.sec == 0)
		return 0;

	os_get_time(&now);
	if (ssid->disabled_until.sec > now.sec)
		return ssid->disabled_until.sec - now.sec;

	wpas_clear_temp_disabled(wpa_s, ssid, 0);

	return 0;
}


static int wpa_supplicant_select_config(struct wpa_supplicant *wpa_s)
{
	struct wpa_ssid *ssid, *old_ssid;
	int res;

	if (wpa_s->conf->ap_scan == 1 && wpa_s->current_ssid)
		return 0;

	wpa_dbg(wpa_s, MSG_DEBUG, "Select network based on association "
		"information");
	ssid = wpa_supplicant_get_ssid(wpa_s);
	if (ssid == NULL) {
		wpa_msg(wpa_s, MSG_INFO,
			"No network configuration found for the current AP");
		return -1;
	}

	if (wpas_network_disabled(wpa_s, ssid)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Selected network is disabled");
		return -1;
	}

	if (disallowed_bssid(wpa_s, wpa_s->bssid) ||
	    disallowed_ssid(wpa_s, ssid->ssid, ssid->ssid_len)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Selected BSS is disallowed");
		return -1;
	}

	res = wpas_temp_disabled(wpa_s, ssid);
	if (res > 0) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Selected network is temporarily "
			"disabled for %d second(s)", res);
		return -1;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "Network configuration found for the "
		"current AP");
	if (wpa_key_mgmt_wpa_any(ssid->key_mgmt)) {
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


void wpa_supplicant_stop_countermeasures(void *eloop_ctx, void *sock_ctx)
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

	wnm_bss_keep_alive_deinit(wpa_s);

#ifdef CONFIG_IBSS_RSN
	ibss_rsn_deinit(wpa_s->ibss_rsn);
	wpa_s->ibss_rsn = NULL;
#endif /* CONFIG_IBSS_RSN */

#ifdef CONFIG_AP
	wpa_supplicant_ap_deinit(wpa_s);
#endif /* CONFIG_AP */

	if (wpa_s->wpa_state == WPA_INTERFACE_DISABLED)
		return;

	wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
	bssid_changed = !is_zero_ether_addr(wpa_s->bssid);
	os_memset(wpa_s->bssid, 0, ETH_ALEN);
	os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
#ifdef CONFIG_SME
	wpa_s->sme.prev_bssid_set = 0;
#endif /* CONFIG_SME */
#ifdef CONFIG_P2P
	os_memset(wpa_s->go_dev_addr, 0, ETH_ALEN);
#endif /* CONFIG_P2P */
	wpa_s->current_bss = NULL;
	wpa_s->assoc_freq = 0;
#ifdef CONFIG_IEEE80211R
#ifdef CONFIG_SME
	if (wpa_s->sme.ft_ies)
		sme_update_ft_ies(wpa_s, NULL, NULL, 0);
#endif /* CONFIG_SME */
#endif /* CONFIG_IEEE80211R */

	if (bssid_changed)
		wpas_notify_bssid_changed(wpa_s);

	eapol_sm_notify_portEnabled(wpa_s->eapol, FALSE);
	eapol_sm_notify_portValid(wpa_s->eapol, FALSE);
	if (wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt))
		eapol_sm_notify_eap_success(wpa_s->eapol, FALSE);
	wpa_s->ap_ies_from_associnfo = 0;
	wpa_s->current_ssid = NULL;
	wpa_s->key_mgmt = 0;
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

	wpa_dbg(wpa_s, MSG_DEBUG, "RSN: PMKID from assoc IE %sfound from "
		"PMKSA cache", pmksa_set == 0 ? "" : "not ");
}


static void wpa_supplicant_event_pmkid_candidate(struct wpa_supplicant *wpa_s,
						 union wpa_event_data *data)
{
	if (data == NULL) {
		wpa_dbg(wpa_s, MSG_DEBUG, "RSN: No data in PMKID candidate "
			"event");
		return;
	}
	wpa_dbg(wpa_s, MSG_DEBUG, "RSN: PMKID candidate event - bssid=" MACSTR
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
#ifdef PCSC_FUNCS
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
				else if (eap->method == EAP_TYPE_AKA ||
					 eap->method == EAP_TYPE_AKA_PRIME)
					aka = 1;
			}
			eap++;
		}
	}

	if (eap_peer_get_eap_method(EAP_VENDOR_IETF, EAP_TYPE_SIM) == NULL)
		sim = 0;
	if (eap_peer_get_eap_method(EAP_VENDOR_IETF, EAP_TYPE_AKA) == NULL &&
	    eap_peer_get_eap_method(EAP_VENDOR_IETF, EAP_TYPE_AKA_PRIME) ==
	    NULL)
		aka = 0;

	if (!sim && !aka) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Selected network is configured to "
			"use SIM, but neither EAP-SIM nor EAP-AKA are "
			"enabled");
		return 0;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "Selected network is configured to use SIM "
		"(sim=%d aka=%d) - initialize PCSC", sim, aka);
	if (sim && aka)
		type = SCARD_TRY_BOTH;
	else if (aka)
		type = SCARD_USIM_ONLY;
	else
		type = SCARD_GSM_SIM_ONLY;

	wpa_s->scard = scard_init(type, NULL);
	if (wpa_s->scard == NULL) {
		wpa_msg(wpa_s, MSG_WARNING, "Failed to initialize SIM "
			"(pcsc-lite)");
		return -1;
	}
	wpa_sm_set_scard_ctx(wpa_s->wpa, wpa_s->scard);
	eapol_sm_register_scard_ctx(wpa_s->eapol, wpa_s->scard);
#endif /* PCSC_FUNCS */
#endif /* IEEE8021X_EAPOL */

	return 0;
}


#ifndef CONFIG_NO_SCAN_PROCESSING
static int wpa_supplicant_match_privacy(struct wpa_bss *bss,
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

	if (wpa_key_mgmt_wpa(ssid->key_mgmt))
		privacy = 1;

	if (bss->caps & IEEE80211_CAP_PRIVACY)
		return privacy;
	return !privacy;
}


static int wpa_supplicant_ssid_bss_match(struct wpa_supplicant *wpa_s,
					 struct wpa_ssid *ssid,
					 struct wpa_bss *bss)
{
	struct wpa_ie_data ie;
	int proto_match = 0;
	const u8 *rsn_ie, *wpa_ie;
	int ret;
	int wep_ok;

	ret = wpas_wps_ssid_bss_match(wpa_s, ssid, bss);
	if (ret >= 0)
		return ret;

	/* Allow TSN if local configuration accepts WEP use without WPA/WPA2 */
	wep_ok = !wpa_key_mgmt_wpa(ssid->key_mgmt) &&
		(((ssid->key_mgmt & WPA_KEY_MGMT_NONE) &&
		  ssid->wep_key_len[ssid->wep_tx_keyidx] > 0) ||
		 (ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA));

	rsn_ie = wpa_bss_get_ie(bss, WLAN_EID_RSN);
	while ((ssid->proto & WPA_PROTO_RSN) && rsn_ie) {
		proto_match++;

		if (wpa_parse_wpa_ie(rsn_ie, 2 + rsn_ie[1], &ie)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip RSN IE - parse "
				"failed");
			break;
		}

		if (wep_ok &&
		    (ie.group_cipher & (WPA_CIPHER_WEP40 | WPA_CIPHER_WEP104)))
		{
			wpa_dbg(wpa_s, MSG_DEBUG, "   selected based on TSN "
				"in RSN IE");
			return 1;
		}

		if (!(ie.proto & ssid->proto)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip RSN IE - proto "
				"mismatch");
			break;
		}

		if (!(ie.pairwise_cipher & ssid->pairwise_cipher)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip RSN IE - PTK "
				"cipher mismatch");
			break;
		}

		if (!(ie.group_cipher & ssid->group_cipher)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip RSN IE - GTK "
				"cipher mismatch");
			break;
		}

		if (!(ie.key_mgmt & ssid->key_mgmt)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip RSN IE - key mgmt "
				"mismatch");
			break;
		}

#ifdef CONFIG_IEEE80211W
		if (!(ie.capabilities & WPA_CAPABILITY_MFPC) &&
		    (ssid->ieee80211w == MGMT_FRAME_PROTECTION_DEFAULT ?
		     wpa_s->conf->pmf : ssid->ieee80211w) ==
		    MGMT_FRAME_PROTECTION_REQUIRED) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip RSN IE - no mgmt "
				"frame protection");
			break;
		}
#endif /* CONFIG_IEEE80211W */

		wpa_dbg(wpa_s, MSG_DEBUG, "   selected based on RSN IE");
		return 1;
	}

	wpa_ie = wpa_bss_get_vendor_ie(bss, WPA_IE_VENDOR_TYPE);
	while ((ssid->proto & WPA_PROTO_WPA) && wpa_ie) {
		proto_match++;

		if (wpa_parse_wpa_ie(wpa_ie, 2 + wpa_ie[1], &ie)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip WPA IE - parse "
				"failed");
			break;
		}

		if (wep_ok &&
		    (ie.group_cipher & (WPA_CIPHER_WEP40 | WPA_CIPHER_WEP104)))
		{
			wpa_dbg(wpa_s, MSG_DEBUG, "   selected based on TSN "
				"in WPA IE");
			return 1;
		}

		if (!(ie.proto & ssid->proto)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip WPA IE - proto "
				"mismatch");
			break;
		}

		if (!(ie.pairwise_cipher & ssid->pairwise_cipher)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip WPA IE - PTK "
				"cipher mismatch");
			break;
		}

		if (!(ie.group_cipher & ssid->group_cipher)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip WPA IE - GTK "
				"cipher mismatch");
			break;
		}

		if (!(ie.key_mgmt & ssid->key_mgmt)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip WPA IE - key mgmt "
				"mismatch");
			break;
		}

		wpa_dbg(wpa_s, MSG_DEBUG, "   selected based on WPA IE");
		return 1;
	}

	if ((ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA) && !wpa_ie &&
	    !rsn_ie) {
		wpa_dbg(wpa_s, MSG_DEBUG, "   allow for non-WPA IEEE 802.1X");
		return 1;
	}

	if ((ssid->proto & (WPA_PROTO_WPA | WPA_PROTO_RSN)) &&
	    wpa_key_mgmt_wpa(ssid->key_mgmt) && proto_match == 0) {
		wpa_dbg(wpa_s, MSG_DEBUG, "   skip - no WPA/RSN proto match");
		return 0;
	}

	if (!wpa_key_mgmt_wpa(ssid->key_mgmt)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "   allow in non-WPA/WPA2");
		return 1;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "   reject due to mismatch with "
		"WPA/WPA2");

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


static int ht_supported(const struct hostapd_hw_modes *mode)
{
	if (!(mode->flags & HOSTAPD_MODE_FLAG_HT_INFO_KNOWN)) {
		/*
		 * The driver did not indicate whether it supports HT. Assume
		 * it does to avoid connection issues.
		 */
		return 1;
	}

	/*
	 * IEEE Std 802.11n-2009 20.1.1:
	 * An HT non-AP STA shall support all EQM rates for one spatial stream.
	 */
	return mode->mcs_set[0] == 0xff;
}


static int rate_match(struct wpa_supplicant *wpa_s, struct wpa_bss *bss)
{
	const struct hostapd_hw_modes *mode = NULL, *modes;
	const u8 scan_ie[2] = { WLAN_EID_SUPP_RATES, WLAN_EID_EXT_SUPP_RATES };
	const u8 *rate_ie;
	int i, j, k;

	if (bss->freq == 0)
		return 1; /* Cannot do matching without knowing band */

	modes = wpa_s->hw.modes;
	if (modes == NULL) {
		/*
		 * The driver does not provide any additional information
		 * about the utilized hardware, so allow the connection attempt
		 * to continue.
		 */
		return 1;
	}

	for (i = 0; i < wpa_s->hw.num_modes; i++) {
		for (j = 0; j < modes[i].num_channels; j++) {
			int freq = modes[i].channels[j].freq;
			if (freq == bss->freq) {
				if (mode &&
				    mode->mode == HOSTAPD_MODE_IEEE80211G)
					break; /* do not allow 802.11b replace
						* 802.11g */
				mode = &modes[i];
				break;
			}
		}
	}

	if (mode == NULL)
		return 0;

	for (i = 0; i < (int) sizeof(scan_ie); i++) {
		rate_ie = wpa_bss_get_ie(bss, scan_ie[i]);
		if (rate_ie == NULL)
			continue;

		for (j = 2; j < rate_ie[1] + 2; j++) {
			int flagged = !!(rate_ie[j] & 0x80);
			int r = (rate_ie[j] & 0x7f) * 5;

			/*
			 * IEEE Std 802.11n-2009 7.3.2.2:
			 * The new BSS Membership selector value is encoded
			 * like a legacy basic rate, but it is not a rate and
			 * only indicates if the BSS members are required to
			 * support the mandatory features of Clause 20 [HT PHY]
			 * in order to join the BSS.
			 */
			if (flagged && ((rate_ie[j] & 0x7f) ==
					BSS_MEMBERSHIP_SELECTOR_HT_PHY)) {
				if (!ht_supported(mode)) {
					wpa_dbg(wpa_s, MSG_DEBUG,
						"   hardware does not support "
						"HT PHY");
					return 0;
				}
				continue;
			}

			if (!flagged)
				continue;

			/* check for legacy basic rates */
			for (k = 0; k < mode->num_rates; k++) {
				if (mode->rates[k] == r)
					break;
			}
			if (k == mode->num_rates) {
				/*
				 * IEEE Std 802.11-2007 7.3.2.2 demands that in
				 * order to join a BSS all required rates
				 * have to be supported by the hardware.
				 */
				wpa_dbg(wpa_s, MSG_DEBUG, "   hardware does "
					"not support required rate %d.%d Mbps",
					r / 10, r % 10);
				return 0;
			}
		}
	}

	return 1;
}


static struct wpa_ssid * wpa_scan_res_match(struct wpa_supplicant *wpa_s,
					    int i, struct wpa_bss *bss,
					    struct wpa_ssid *group)
{
	u8 wpa_ie_len, rsn_ie_len;
	int wpa;
	struct wpa_blacklist *e;
	const u8 *ie;
	struct wpa_ssid *ssid;

	ie = wpa_bss_get_vendor_ie(bss, WPA_IE_VENDOR_TYPE);
	wpa_ie_len = ie ? ie[1] : 0;

	ie = wpa_bss_get_ie(bss, WLAN_EID_RSN);
	rsn_ie_len = ie ? ie[1] : 0;

	wpa_dbg(wpa_s, MSG_DEBUG, "%d: " MACSTR " ssid='%s' "
		"wpa_ie_len=%u rsn_ie_len=%u caps=0x%x level=%d%s",
		i, MAC2STR(bss->bssid), wpa_ssid_txt(bss->ssid, bss->ssid_len),
		wpa_ie_len, rsn_ie_len, bss->caps, bss->level,
		wpa_bss_get_vendor_ie(bss, WPS_IE_VENDOR_TYPE) ? " wps" : "");

	e = wpa_blacklist_get(wpa_s, bss->bssid);
	if (e) {
		int limit = 1;
		if (wpa_supplicant_enabled_networks(wpa_s) == 1) {
			/*
			 * When only a single network is enabled, we can
			 * trigger blacklisting on the first failure. This
			 * should not be done with multiple enabled networks to
			 * avoid getting forced to move into a worse ESS on
			 * single error if there are no other BSSes of the
			 * current ESS.
			 */
			limit = 0;
		}
		if (e->count > limit) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip - blacklisted "
				"(count=%d limit=%d)", e->count, limit);
			return NULL;
		}
	}

	if (bss->ssid_len == 0) {
		wpa_dbg(wpa_s, MSG_DEBUG, "   skip - SSID not known");
		return NULL;
	}

	if (disallowed_bssid(wpa_s, bss->bssid)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "   skip - BSSID disallowed");
		return NULL;
	}

	if (disallowed_ssid(wpa_s, bss->ssid, bss->ssid_len)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "   skip - SSID disallowed");
		return NULL;
	}

	wpa = wpa_ie_len > 0 || rsn_ie_len > 0;

	for (ssid = group; ssid; ssid = ssid->pnext) {
		int check_ssid = wpa ? 1 : (ssid->ssid_len != 0);
		int res;

		if (wpas_network_disabled(wpa_s, ssid)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip - disabled");
			continue;
		}

		res = wpas_temp_disabled(wpa_s, ssid);
		if (res > 0) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip - disabled "
				"temporarily for %d second(s)", res);
			continue;
		}

#ifdef CONFIG_WPS
		if ((ssid->key_mgmt & WPA_KEY_MGMT_WPS) && e && e->count > 0) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip - blacklisted "
				"(WPS)");
			continue;
		}

		if (wpa && ssid->ssid_len == 0 &&
		    wpas_wps_ssid_wildcard_ok(wpa_s, ssid, bss))
			check_ssid = 0;

		if (!wpa && (ssid->key_mgmt & WPA_KEY_MGMT_WPS)) {
			/* Only allow wildcard SSID match if an AP
			 * advertises active WPS operation that matches
			 * with our mode. */
			check_ssid = 1;
			if (ssid->ssid_len == 0 &&
			    wpas_wps_ssid_wildcard_ok(wpa_s, ssid, bss))
				check_ssid = 0;
		}
#endif /* CONFIG_WPS */

		if (ssid->bssid_set && ssid->ssid_len == 0 &&
		    os_memcmp(bss->bssid, ssid->bssid, ETH_ALEN) == 0)
			check_ssid = 0;

		if (check_ssid &&
		    (bss->ssid_len != ssid->ssid_len ||
		     os_memcmp(bss->ssid, ssid->ssid, bss->ssid_len) != 0)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip - SSID mismatch");
			continue;
		}

		if (ssid->bssid_set &&
		    os_memcmp(bss->bssid, ssid->bssid, ETH_ALEN) != 0) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip - BSSID mismatch");
			continue;
		}

		if (!wpa_supplicant_ssid_bss_match(wpa_s, ssid, bss))
			continue;

		if (!wpa &&
		    !(ssid->key_mgmt & WPA_KEY_MGMT_NONE) &&
		    !(ssid->key_mgmt & WPA_KEY_MGMT_WPS) &&
		    !(ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip - non-WPA network "
				"not allowed");
			continue;
		}

		if (!wpa_supplicant_match_privacy(bss, ssid)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip - privacy "
				"mismatch");
			continue;
		}

		if (bss->caps & IEEE80211_CAP_IBSS) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip - IBSS (adhoc) "
				"network");
			continue;
		}

		if (!freq_allowed(ssid->freq_list, bss->freq)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip - frequency not "
				"allowed");
			continue;
		}

		if (!rate_match(wpa_s, bss)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "   skip - rate sets do "
				"not match");
			continue;
		}

#ifdef CONFIG_P2P
		/*
		 * TODO: skip the AP if its P2P IE has Group Formation
		 * bit set in the P2P Group Capability Bitmap and we
		 * are not in Group Formation with that device.
		 */
#endif /* CONFIG_P2P */

		/* Matching configuration found */
		return ssid;
	}

	/* No matching configuration found */
	return NULL;
}


static struct wpa_bss *
wpa_supplicant_select_bss(struct wpa_supplicant *wpa_s,
			  struct wpa_ssid *group,
			  struct wpa_ssid **selected_ssid)
{
	unsigned int i;

	wpa_dbg(wpa_s, MSG_DEBUG, "Selecting BSS from priority group %d",
		group->priority);

	for (i = 0; i < wpa_s->last_scan_res_used; i++) {
		struct wpa_bss *bss = wpa_s->last_scan_res[i];
		*selected_ssid = wpa_scan_res_match(wpa_s, i, bss, group);
		if (!*selected_ssid)
			continue;
		wpa_dbg(wpa_s, MSG_DEBUG, "   selected BSS " MACSTR
			" ssid='%s'",
			MAC2STR(bss->bssid),
			wpa_ssid_txt(bss->ssid, bss->ssid_len));
		return bss;
	}

	return NULL;
}


static struct wpa_bss *
wpa_supplicant_pick_network(struct wpa_supplicant *wpa_s,
			    struct wpa_ssid **selected_ssid)
{
	struct wpa_bss *selected = NULL;
	int prio;

	if (wpa_s->last_scan_res == NULL ||
	    wpa_s->last_scan_res_used == 0)
		return NULL; /* no scan results from last update */

	while (selected == NULL) {
		for (prio = 0; prio < wpa_s->conf->num_prio; prio++) {
			selected = wpa_supplicant_select_bss(
				wpa_s, wpa_s->conf->pssid[prio],
				selected_ssid);
			if (selected)
				break;
		}

		if (selected == NULL && wpa_s->blacklist &&
		    !wpa_s->countermeasures) {
			wpa_dbg(wpa_s, MSG_DEBUG, "No APs found - clear "
				"blacklist and try again");
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
	if (!wpa_supplicant_enabled_networks(wpa_s)) {
		/*
		 * No networks are enabled; short-circuit request so
		 * we don't wait timeout seconds before transitioning
		 * to INACTIVE state.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG, "Short-circuit new scan request "
			"since there are no enabled networks");
		wpa_supplicant_set_state(wpa_s, WPA_INACTIVE);
#ifdef CONFIG_P2P
		wpa_s->sta_scan_pending = 0;
#endif /* CONFIG_P2P */
		return;
	}

	wpa_s->scan_for_connection = 1;
	wpa_supplicant_req_scan(wpa_s, timeout_sec, timeout_usec);
}


int wpa_supplicant_connect(struct wpa_supplicant *wpa_s,
			   struct wpa_bss *selected,
			   struct wpa_ssid *ssid)
{
	if (wpas_wps_scan_pbc_overlap(wpa_s, selected, ssid)) {
		wpa_msg(wpa_s, MSG_INFO, WPS_EVENT_OVERLAP
			"PBC session overlap");
#ifdef CONFIG_P2P
		if (wpas_p2p_notif_pbc_overlap(wpa_s) == 1)
			return -1;
#endif /* CONFIG_P2P */

#ifdef CONFIG_WPS
		wpas_wps_cancel(wpa_s);
#endif /* CONFIG_WPS */
		return -1;
	}

	/*
	 * Do not trigger new association unless the BSSID has changed or if
	 * reassociation is requested. If we are in process of associating with
	 * the selected BSSID, do not trigger new attempt.
	 */
	if (wpa_s->reassociate ||
	    (os_memcmp(selected->bssid, wpa_s->bssid, ETH_ALEN) != 0 &&
	     ((wpa_s->wpa_state != WPA_ASSOCIATING &&
	       wpa_s->wpa_state != WPA_AUTHENTICATING) ||
	      os_memcmp(selected->bssid, wpa_s->pending_bssid, ETH_ALEN) !=
	      0))) {
		if (wpa_supplicant_scard_init(wpa_s, ssid)) {
			wpa_supplicant_req_new_scan(wpa_s, 10, 0);
			return 0;
		}
		wpa_msg(wpa_s, MSG_DEBUG, "Request association: "
			"reassociate: %d  selected: "MACSTR "  bssid: " MACSTR
			"  pending: " MACSTR "  wpa_state: %s",
			wpa_s->reassociate, MAC2STR(selected->bssid),
			MAC2STR(wpa_s->bssid), MAC2STR(wpa_s->pending_bssid),
			wpa_supplicant_state_txt(wpa_s->wpa_state));
		wpa_supplicant_associate(wpa_s, selected, ssid);
	} else {
		wpa_dbg(wpa_s, MSG_DEBUG, "Already associated with the "
			"selected AP");
	}

	return 0;
}


static struct wpa_ssid *
wpa_supplicant_pick_new_network(struct wpa_supplicant *wpa_s)
{
	int prio;
	struct wpa_ssid *ssid;

	for (prio = 0; prio < wpa_s->conf->num_prio; prio++) {
		for (ssid = wpa_s->conf->pssid[prio]; ssid; ssid = ssid->pnext)
		{
			if (wpas_network_disabled(wpa_s, ssid))
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
	struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss;

	if (rsn_preauth_scan_results(wpa_s->wpa) < 0)
		return;

	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		const u8 *ssid, *rsn;

		ssid = wpa_bss_get_ie(bss, WLAN_EID_SSID);
		if (ssid == NULL)
			continue;

		rsn = wpa_bss_get_ie(bss, WLAN_EID_RSN);
		if (rsn == NULL)
			continue;

		rsn_preauth_scan_result(wpa_s->wpa, bss->bssid, ssid, rsn);
	}

}


static int wpa_supplicant_need_to_roam(struct wpa_supplicant *wpa_s,
				       struct wpa_bss *selected,
				       struct wpa_ssid *ssid)
{
	struct wpa_bss *current_bss = NULL;
	int min_diff;

	if (wpa_s->reassociate)
		return 1; /* explicit request to reassociate */
	if (wpa_s->wpa_state < WPA_ASSOCIATED)
		return 1; /* we are not associated; continue */
	if (wpa_s->current_ssid == NULL)
		return 1; /* unknown current SSID */
	if (wpa_s->current_ssid != ssid)
		return 1; /* different network block */

	if (wpas_driver_bss_selection(wpa_s))
		return 0; /* Driver-based roaming */

	if (wpa_s->current_ssid->ssid)
		current_bss = wpa_bss_get(wpa_s, wpa_s->bssid,
					  wpa_s->current_ssid->ssid,
					  wpa_s->current_ssid->ssid_len);
	if (!current_bss)
		current_bss = wpa_bss_get_bssid(wpa_s, wpa_s->bssid);

	if (!current_bss)
		return 1; /* current BSS not seen in scan results */

	if (current_bss == selected)
		return 0;

	if (selected->last_update_idx > current_bss->last_update_idx)
		return 1; /* current BSS not seen in the last scan */

#ifndef CONFIG_NO_ROAMING
	wpa_dbg(wpa_s, MSG_DEBUG, "Considering within-ESS reassociation");
	wpa_dbg(wpa_s, MSG_DEBUG, "Current BSS: " MACSTR " level=%d",
		MAC2STR(current_bss->bssid), current_bss->level);
	wpa_dbg(wpa_s, MSG_DEBUG, "Selected BSS: " MACSTR " level=%d",
		MAC2STR(selected->bssid), selected->level);

	if (wpa_s->current_ssid->bssid_set &&
	    os_memcmp(selected->bssid, wpa_s->current_ssid->bssid, ETH_ALEN) ==
	    0) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Allow reassociation - selected BSS "
			"has preferred BSSID");
		return 1;
	}

	if (current_bss->level < 0 && current_bss->level > selected->level) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Skip roam - Current BSS has better "
			"signal level");
		return 0;
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
		wpa_dbg(wpa_s, MSG_DEBUG, "Skip roam - too small difference "
			"in signal level");
		return 0;
	}

	return 1;
#else /* CONFIG_NO_ROAMING */
	return 0;
#endif /* CONFIG_NO_ROAMING */
}


/* Return != 0 if no scan results could be fetched or if scan results should not
 * be shared with other virtual interfaces. */
static int _wpa_supplicant_event_scan_results(struct wpa_supplicant *wpa_s,
					      union wpa_event_data *data)
{
	struct wpa_scan_results *scan_res;
	int ap = 0;
#ifndef CONFIG_NO_RANDOM_POOL
	size_t i, num;
#endif /* CONFIG_NO_RANDOM_POOL */

#ifdef CONFIG_AP
	if (wpa_s->ap_iface)
		ap = 1;
#endif /* CONFIG_AP */

	wpa_supplicant_notify_scanning(wpa_s, 0);

#ifdef CONFIG_P2P
	if (wpa_s->global->p2p_cb_on_scan_complete &&
	    !wpa_s->global->p2p_disabled &&
	    wpa_s->global->p2p != NULL && !wpa_s->sta_scan_pending &&
	    !wpa_s->scan_res_handler) {
		wpa_s->global->p2p_cb_on_scan_complete = 0;
		if (p2p_other_scan_completed(wpa_s->global->p2p) == 1) {
			wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Pending P2P operation "
				"stopped scan processing");
			wpa_s->sta_scan_pending = 1;
			wpa_supplicant_req_scan(wpa_s, 5, 0);
			return -1;
		}
	}
	wpa_s->sta_scan_pending = 0;
#endif /* CONFIG_P2P */

	scan_res = wpa_supplicant_get_scan_results(wpa_s,
						   data ? &data->scan_info :
						   NULL, 1);
	if (scan_res == NULL) {
		if (wpa_s->conf->ap_scan == 2 || ap)
			return -1;
		wpa_dbg(wpa_s, MSG_DEBUG, "Failed to get scan results - try "
			"scanning again");
		wpa_supplicant_req_new_scan(wpa_s, 1, 0);
		return -1;
	}

#ifndef CONFIG_NO_RANDOM_POOL
	num = scan_res->num;
	if (num > 10)
		num = 10;
	for (i = 0; i < num; i++) {
		u8 buf[5];
		struct wpa_scan_res *res = scan_res->res[i];
		buf[0] = res->bssid[5];
		buf[1] = res->qual & 0xff;
		buf[2] = res->noise & 0xff;
		buf[3] = res->level & 0xff;
		buf[4] = res->tsf & 0xff;
		random_add_randomness(buf, sizeof(buf));
	}
#endif /* CONFIG_NO_RANDOM_POOL */

	if (wpa_s->scan_res_handler) {
		void (*scan_res_handler)(struct wpa_supplicant *wpa_s,
					 struct wpa_scan_results *scan_res);

		scan_res_handler = wpa_s->scan_res_handler;
		wpa_s->scan_res_handler = NULL;
		scan_res_handler(wpa_s, scan_res);

		wpa_scan_results_free(scan_res);
		return -2;
	}

	if (ap) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Ignore scan results in AP mode");
#ifdef CONFIG_AP
		if (wpa_s->ap_iface->scan_cb)
			wpa_s->ap_iface->scan_cb(wpa_s->ap_iface);
#endif /* CONFIG_AP */
		wpa_scan_results_free(scan_res);
		return 0;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "New scan results available");
	wpa_msg_ctrl(wpa_s, MSG_INFO, WPA_EVENT_SCAN_RESULTS);
	wpas_notify_scan_results(wpa_s);

	wpas_notify_scan_done(wpa_s, 1);

	if (sme_proc_obss_scan(wpa_s) > 0) {
		wpa_scan_results_free(scan_res);
		return 0;
	}

	if ((wpa_s->conf->ap_scan == 2 && !wpas_wps_searching(wpa_s))) {
		wpa_scan_results_free(scan_res);
		return 0;
	}

	if (autoscan_notify_scan(wpa_s, scan_res)) {
		wpa_scan_results_free(scan_res);
		return 0;
	}

	if (wpa_s->disconnected) {
		wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
		wpa_scan_results_free(scan_res);
		return 0;
	}

	if (!wpas_driver_bss_selection(wpa_s) &&
	    bgscan_notify_scan(wpa_s, scan_res) == 1) {
		wpa_scan_results_free(scan_res);
		return 0;
	}

	wpas_wps_update_ap_info(wpa_s, scan_res);

	wpa_scan_results_free(scan_res);

	return wpas_select_network_from_last_scan(wpa_s);
}


int wpas_select_network_from_last_scan(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *selected;
	struct wpa_ssid *ssid = NULL;

	selected = wpa_supplicant_pick_network(wpa_s, &ssid);

	if (selected) {
		int skip;
		skip = !wpa_supplicant_need_to_roam(wpa_s, selected, ssid);
		if (skip) {
			wpa_supplicant_rsn_preauth_scan_results(wpa_s);
			return 0;
		}

		if (wpa_supplicant_connect(wpa_s, selected, ssid) < 0) {
			wpa_dbg(wpa_s, MSG_DEBUG, "Connect failed");
			return -1;
		}
		wpa_supplicant_rsn_preauth_scan_results(wpa_s);
		/*
		 * Do not notify other virtual radios of scan results since we do not
		 * want them to start other associations at the same time.
		 */
		return 1;
	} else {
		wpa_dbg(wpa_s, MSG_DEBUG, "No suitable network found");
		ssid = wpa_supplicant_pick_new_network(wpa_s);
		if (ssid) {
			wpa_dbg(wpa_s, MSG_DEBUG, "Setup a new network");
			wpa_supplicant_associate(wpa_s, NULL, ssid);
			wpa_supplicant_rsn_preauth_scan_results(wpa_s);
		} else {
			int timeout_sec = wpa_s->scan_interval;
			int timeout_usec = 0;
#ifdef CONFIG_P2P
			if (wpas_p2p_scan_no_go_seen(wpa_s) == 1)
				return 0;

			if (wpa_s->p2p_in_provisioning) {
				/*
				 * Use shorter wait during P2P Provisioning
				 * state to speed up group formation.
				 */
				timeout_sec = 0;
				timeout_usec = 250000;
				wpa_supplicant_req_new_scan(wpa_s, timeout_sec,
							    timeout_usec);
				return 0;
			}
#endif /* CONFIG_P2P */
#ifdef CONFIG_INTERWORKING
			if (wpa_s->conf->auto_interworking &&
			    wpa_s->conf->interworking &&
			    wpa_s->conf->cred) {
				wpa_dbg(wpa_s, MSG_DEBUG, "Interworking: "
					"start ANQP fetch since no matching "
					"networks found");
				wpa_s->network_select = 1;
				wpa_s->auto_network_select = 1;
				interworking_start_fetch_anqp(wpa_s);
				return 1;
			}
#endif /* CONFIG_INTERWORKING */
			if (wpa_supplicant_req_sched_scan(wpa_s))
				wpa_supplicant_req_new_scan(wpa_s, timeout_sec,
							    timeout_usec);
		}
	}
	return 0;
}


static void wpa_supplicant_event_scan_results(struct wpa_supplicant *wpa_s,
					      union wpa_event_data *data)
{
	const char *rn, *rn2;
	struct wpa_supplicant *ifs;

	if (_wpa_supplicant_event_scan_results(wpa_s, data) != 0) {
		/*
		 * If no scan results could be fetched, then no need to
		 * notify those interfaces that did not actually request
		 * this scan. Similarly, if scan results started a new operation on this
		 * interface, do not notify other interfaces to avoid concurrent
		 * operations during a connection attempt.
		 */
		return;
	}

	/*
	 * Check other interfaces to see if they have the same radio-name. If
	 * so, they get updated with this same scan info.
	 */
	if (!wpa_s->driver->get_radio_name)
		return;

	rn = wpa_s->driver->get_radio_name(wpa_s->drv_priv);
	if (rn == NULL || rn[0] == '\0')
		return;

	wpa_dbg(wpa_s, MSG_DEBUG, "Checking for other virtual interfaces "
		"sharing same radio (%s) in event_scan_results", rn);

	for (ifs = wpa_s->global->ifaces; ifs; ifs = ifs->next) {
		if (ifs == wpa_s || !ifs->driver->get_radio_name)
			continue;

		rn2 = ifs->driver->get_radio_name(ifs->drv_priv);
		if (rn2 && os_strcmp(rn, rn2) == 0) {
			wpa_printf(MSG_DEBUG, "%s: Updating scan results from "
				   "sibling", ifs->ifname);
			_wpa_supplicant_event_scan_results(ifs, data);
		}
	}
}

#endif /* CONFIG_NO_SCAN_PROCESSING */


#ifdef CONFIG_WNM

static void wnm_bss_keep_alive(void *eloop_ctx, void *sock_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	if (wpa_s->wpa_state < WPA_ASSOCIATED)
		return;

	if (!wpa_s->no_keep_alive) {
		wpa_printf(MSG_DEBUG, "WNM: Send keep-alive to AP " MACSTR,
			   MAC2STR(wpa_s->bssid));
		/* TODO: could skip this if normal data traffic has been sent */
		/* TODO: Consider using some more appropriate data frame for
		 * this */
		if (wpa_s->l2)
			l2_packet_send(wpa_s->l2, wpa_s->bssid, 0x0800,
				       (u8 *) "", 0);
	}

#ifdef CONFIG_SME
	if (wpa_s->sme.bss_max_idle_period) {
		unsigned int msec;
		msec = wpa_s->sme.bss_max_idle_period * 1024; /* times 1000 */
		if (msec > 100)
			msec -= 100;
		eloop_register_timeout(msec / 1000, msec % 1000 * 1000,
				       wnm_bss_keep_alive, wpa_s, NULL);
	}
#endif /* CONFIG_SME */
}


static void wnm_process_assoc_resp(struct wpa_supplicant *wpa_s,
				   const u8 *ies, size_t ies_len)
{
	struct ieee802_11_elems elems;

	if (ies == NULL)
		return;

	if (ieee802_11_parse_elems(ies, ies_len, &elems, 1) == ParseFailed)
		return;

#ifdef CONFIG_SME
	if (elems.bss_max_idle_period) {
		unsigned int msec;
		wpa_s->sme.bss_max_idle_period =
			WPA_GET_LE16(elems.bss_max_idle_period);
		wpa_printf(MSG_DEBUG, "WNM: BSS Max Idle Period: %u (* 1000 "
			   "TU)%s", wpa_s->sme.bss_max_idle_period,
			   (elems.bss_max_idle_period[2] & 0x01) ?
			   " (protected keep-live required)" : "");
		if (wpa_s->sme.bss_max_idle_period == 0)
			wpa_s->sme.bss_max_idle_period = 1;
		if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_SME) {
			eloop_cancel_timeout(wnm_bss_keep_alive, wpa_s, NULL);
			 /* msec times 1000 */
			msec = wpa_s->sme.bss_max_idle_period * 1024;
			if (msec > 100)
				msec -= 100;
			eloop_register_timeout(msec / 1000, msec % 1000 * 1000,
					       wnm_bss_keep_alive, wpa_s,
					       NULL);
		}
	}
#endif /* CONFIG_SME */
}

#endif /* CONFIG_WNM */


void wnm_bss_keep_alive_deinit(struct wpa_supplicant *wpa_s)
{
#ifdef CONFIG_WNM
	eloop_cancel_timeout(wnm_bss_keep_alive, wpa_s, NULL);
#endif /* CONFIG_WNM */
}


static int wpa_supplicant_event_associnfo(struct wpa_supplicant *wpa_s,
					  union wpa_event_data *data)
{
	int l, len, found = 0, wpa_found, rsn_found;
	const u8 *p;

	wpa_dbg(wpa_s, MSG_DEBUG, "Association info event");
	if (data->assoc_info.req_ies)
		wpa_hexdump(MSG_DEBUG, "req_ies", data->assoc_info.req_ies,
			    data->assoc_info.req_ies_len);
	if (data->assoc_info.resp_ies) {
		wpa_hexdump(MSG_DEBUG, "resp_ies", data->assoc_info.resp_ies,
			    data->assoc_info.resp_ies_len);
#ifdef CONFIG_TDLS
		wpa_tdls_assoc_resp_ies(wpa_s->wpa, data->assoc_info.resp_ies,
					data->assoc_info.resp_ies_len);
#endif /* CONFIG_TDLS */
#ifdef CONFIG_WNM
		wnm_process_assoc_resp(wpa_s, data->assoc_info.resp_ies,
				       data->assoc_info.resp_ies_len);
#endif /* CONFIG_WNM */
	}
	if (data->assoc_info.beacon_ies)
		wpa_hexdump(MSG_DEBUG, "beacon_ies",
			    data->assoc_info.beacon_ies,
			    data->assoc_info.beacon_ies_len);
	if (data->assoc_info.freq)
		wpa_dbg(wpa_s, MSG_DEBUG, "freq=%u MHz",
			data->assoc_info.freq);

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
			wpa_dbg(wpa_s, MSG_DEBUG, "FT: Validation of "
				"Reassociation Response failed");
			wpa_supplicant_deauthenticate(
				wpa_s, WLAN_REASON_INVALID_IE);
			return -1;
		}
	}

	p = data->assoc_info.resp_ies;
	l = data->assoc_info.resp_ies_len;

#ifdef CONFIG_WPS_STRICT
	if (p && wpa_s->current_ssid &&
	    wpa_s->current_ssid->key_mgmt == WPA_KEY_MGMT_WPS) {
		struct wpabuf *wps;
		wps = ieee802_11_vendor_ie_concat(p, l, WPS_IE_VENDOR_TYPE);
		if (wps == NULL) {
			wpa_msg(wpa_s, MSG_INFO, "WPS-STRICT: AP did not "
				"include WPS IE in (Re)Association Response");
			return -1;
		}

		if (wps_validate_assoc_resp(wps) < 0) {
			wpabuf_free(wps);
			wpa_supplicant_deauthenticate(
				wpa_s, WLAN_REASON_INVALID_IE);
			return -1;
		}
		wpabuf_free(wps);
	}
#endif /* CONFIG_WPS_STRICT */

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

	if (wpa_s->assoc_freq && data->assoc_info.freq &&
	    wpa_s->assoc_freq != data->assoc_info.freq) {
		wpa_printf(MSG_DEBUG, "Operating frequency changed from "
			   "%u to %u MHz",
			   wpa_s->assoc_freq, data->assoc_info.freq);
		wpa_supplicant_update_scan_results(wpa_s);
	}

	wpa_s->assoc_freq = data->assoc_info.freq;

	return 0;
}


static struct wpa_bss * wpa_supplicant_get_new_bss(
	struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	struct wpa_bss *bss = NULL;
	struct wpa_ssid *ssid = wpa_s->current_ssid;

	if (ssid->ssid_len > 0)
		bss = wpa_bss_get(wpa_s, bssid, ssid->ssid, ssid->ssid_len);
	if (!bss)
		bss = wpa_bss_get_bssid(wpa_s, bssid);

	return bss;
}


static int wpa_supplicant_assoc_update_ie(struct wpa_supplicant *wpa_s)
{
	const u8 *bss_wpa = NULL, *bss_rsn = NULL;

	if (!wpa_s->current_bss || !wpa_s->current_ssid)
		return -1;

	if (!wpa_key_mgmt_wpa_any(wpa_s->current_ssid->key_mgmt))
		return 0;

	bss_wpa = wpa_bss_get_vendor_ie(wpa_s->current_bss,
					WPA_IE_VENDOR_TYPE);
	bss_rsn = wpa_bss_get_ie(wpa_s->current_bss, WLAN_EID_RSN);

	if (wpa_sm_set_ap_wpa_ie(wpa_s->wpa, bss_wpa,
				 bss_wpa ? 2 + bss_wpa[1] : 0) ||
	    wpa_sm_set_ap_rsn_ie(wpa_s->wpa, bss_rsn,
				 bss_rsn ? 2 + bss_rsn[1] : 0))
		return -1;

	return 0;
}


static void wpa_supplicant_event_assoc(struct wpa_supplicant *wpa_s,
				       union wpa_event_data *data)
{
	u8 bssid[ETH_ALEN];
	int ft_completed;
	struct wpa_driver_capa capa;

#ifdef CONFIG_AP
	if (wpa_s->ap_iface) {
		hostapd_notif_assoc(wpa_s->ap_iface->bss[0],
				    data->assoc_info.addr,
				    data->assoc_info.req_ies,
				    data->assoc_info.req_ies_len,
				    data->assoc_info.reassoc);
		return;
	}
#endif /* CONFIG_AP */

	ft_completed = wpa_ft_is_completed(wpa_s->wpa);
	if (data && wpa_supplicant_event_associnfo(wpa_s, data) < 0)
		return;

	if (wpa_drv_get_bssid(wpa_s, bssid) < 0) {
		wpa_dbg(wpa_s, MSG_ERROR, "Failed to get BSSID");
		wpa_supplicant_deauthenticate(
			wpa_s, WLAN_REASON_DEAUTH_LEAVING);
		return;
	}

	wpa_supplicant_set_state(wpa_s, WPA_ASSOCIATED);
	if (os_memcmp(bssid, wpa_s->bssid, ETH_ALEN) != 0) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Associated to a new BSS: BSSID="
			MACSTR, MAC2STR(bssid));
		random_add_randomness(bssid, ETH_ALEN);
		os_memcpy(wpa_s->bssid, bssid, ETH_ALEN);
		os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
		wpas_notify_bssid_changed(wpa_s);

		if (wpa_supplicant_dynamic_keys(wpa_s) && !ft_completed) {
			wpa_clear_keys(wpa_s, bssid);
		}
		if (wpa_supplicant_select_config(wpa_s) < 0) {
			wpa_supplicant_deauthenticate(
				wpa_s, WLAN_REASON_DEAUTH_LEAVING);
			return;
		}
		if (wpa_s->current_ssid) {
			struct wpa_bss *bss = NULL;

			bss = wpa_supplicant_get_new_bss(wpa_s, bssid);
			if (!bss) {
				wpa_supplicant_update_scan_results(wpa_s);

				/* Get the BSS from the new scan results */
				bss = wpa_supplicant_get_new_bss(wpa_s, bssid);
			}

			if (bss)
				wpa_s->current_bss = bss;
		}

		if (wpa_s->conf->ap_scan == 1 &&
		    wpa_s->drv_flags & WPA_DRIVER_FLAGS_BSS_SELECTION) {
			if (wpa_supplicant_assoc_update_ie(wpa_s) < 0)
				wpa_msg(wpa_s, MSG_WARNING,
					"WPA/RSN IEs not updated");
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
	} else if ((wpa_s->drv_flags & WPA_DRIVER_FLAGS_4WAY_HANDSHAKE) &&
		   wpa_key_mgmt_wpa_ieee8021x(wpa_s->key_mgmt)) {
		/*
		 * The driver will take care of RSN 4-way handshake, so we need
		 * to allow EAPOL supplicant to complete its work without
		 * waiting for WPA supplicant.
		 */
		eapol_sm_notify_portValid(wpa_s->eapol, TRUE);
	} else if (ft_completed) {
		/*
		 * FT protocol completed - make sure EAPOL state machine ends
		 * up in authenticated.
		 */
		wpa_supplicant_cancel_auth_timeout(wpa_s);
		wpa_supplicant_set_state(wpa_s, WPA_COMPLETED);
		eapol_sm_notify_portValid(wpa_s->eapol, TRUE);
		eapol_sm_notify_eap_success(wpa_s->eapol, TRUE);
	}

	wpa_s->last_eapol_matches_bssid = 0;

	if (wpa_s->pending_eapol_rx) {
		struct os_time now, age;
		os_get_time(&now);
		os_time_sub(&now, &wpa_s->pending_eapol_rx_time, &age);
		if (age.sec == 0 && age.usec < 100000 &&
		    os_memcmp(wpa_s->pending_eapol_rx_src, bssid, ETH_ALEN) ==
		    0) {
			wpa_dbg(wpa_s, MSG_DEBUG, "Process pending EAPOL "
				"frame that was received just before "
				"association notification");
			wpa_supplicant_rx_eapol(
				wpa_s, wpa_s->pending_eapol_rx_src,
				wpabuf_head(wpa_s->pending_eapol_rx),
				wpabuf_len(wpa_s->pending_eapol_rx));
		}
		wpabuf_free(wpa_s->pending_eapol_rx);
		wpa_s->pending_eapol_rx = NULL;
	}

	if ((wpa_s->key_mgmt == WPA_KEY_MGMT_NONE ||
	     wpa_s->key_mgmt == WPA_KEY_MGMT_IEEE8021X_NO_WPA) &&
	    wpa_s->current_ssid && wpa_drv_get_capa(wpa_s, &capa) == 0 &&
	    capa.flags & WPA_DRIVER_FLAGS_SET_KEYS_AFTER_ASSOC_DONE) {
		/* Set static WEP keys again */
		wpa_set_wep_keys(wpa_s, wpa_s->current_ssid);
	}

#ifdef CONFIG_IBSS_RSN
	if (wpa_s->current_ssid &&
	    wpa_s->current_ssid->mode == WPAS_MODE_IBSS &&
	    wpa_s->key_mgmt != WPA_KEY_MGMT_NONE &&
	    wpa_s->key_mgmt != WPA_KEY_MGMT_WPA_NONE &&
	    wpa_s->ibss_rsn == NULL) {
		wpa_s->ibss_rsn = ibss_rsn_init(wpa_s);
		if (!wpa_s->ibss_rsn) {
			wpa_msg(wpa_s, MSG_INFO, "Failed to init IBSS RSN");
			wpa_supplicant_deauthenticate(
				wpa_s, WLAN_REASON_DEAUTH_LEAVING);
			return;
		}

		ibss_rsn_set_psk(wpa_s->ibss_rsn, wpa_s->current_ssid->psk);
	}
#endif /* CONFIG_IBSS_RSN */

	wpas_wps_notify_assoc(wpa_s, bssid);
}


static int disconnect_reason_recoverable(u16 reason_code)
{
	return reason_code == WLAN_REASON_DISASSOC_DUE_TO_INACTIVITY ||
		reason_code == WLAN_REASON_CLASS2_FRAME_FROM_NONAUTH_STA ||
		reason_code == WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA;
}


static void wpa_supplicant_event_disassoc(struct wpa_supplicant *wpa_s,
					  u16 reason_code,
					  int locally_generated)
{
	const u8 *bssid;

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE) {
		/*
		 * At least Host AP driver and a Prism3 card seemed to be
		 * generating streams of disconnected events when configuring
		 * IBSS for WPA-None. Ignore them for now.
		 */
		return;
	}

	bssid = wpa_s->bssid;
	if (is_zero_ether_addr(bssid))
		bssid = wpa_s->pending_bssid;

	if (!is_zero_ether_addr(bssid) ||
	    wpa_s->wpa_state >= WPA_AUTHENTICATING) {
		wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DISCONNECTED "bssid=" MACSTR
			" reason=%d%s",
			MAC2STR(bssid), reason_code,
			locally_generated ? " locally_generated=1" : "");
	}
}


static int could_be_psk_mismatch(struct wpa_supplicant *wpa_s, u16 reason_code,
				 int locally_generated)
{
	if (wpa_s->wpa_state != WPA_4WAY_HANDSHAKE ||
	    !wpa_key_mgmt_wpa_psk(wpa_s->key_mgmt))
		return 0; /* Not in 4-way handshake with PSK */

	/*
	 * It looks like connection was lost while trying to go through PSK
	 * 4-way handshake. Filter out known disconnection cases that are caused
	 * by something else than PSK mismatch to avoid confusing reports.
	 */

	if (locally_generated) {
		if (reason_code == WLAN_REASON_IE_IN_4WAY_DIFFERS)
			return 0;
	}

	return 1;
}


static void wpa_supplicant_event_disassoc_finish(struct wpa_supplicant *wpa_s,
						 u16 reason_code,
						 int locally_generated)
{
	const u8 *bssid;
	int authenticating;
	u8 prev_pending_bssid[ETH_ALEN];
	struct wpa_bss *fast_reconnect = NULL;
	struct wpa_ssid *fast_reconnect_ssid = NULL;
	struct wpa_ssid *last_ssid;

	authenticating = wpa_s->wpa_state == WPA_AUTHENTICATING;
	os_memcpy(prev_pending_bssid, wpa_s->pending_bssid, ETH_ALEN);

	if (wpa_s->key_mgmt == WPA_KEY_MGMT_WPA_NONE) {
		/*
		 * At least Host AP driver and a Prism3 card seemed to be
		 * generating streams of disconnected events when configuring
		 * IBSS for WPA-None. Ignore them for now.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG, "Disconnect event - ignore in "
			"IBSS/WPA-None mode");
		return;
	}

	if (could_be_psk_mismatch(wpa_s, reason_code, locally_generated)) {
		wpa_msg(wpa_s, MSG_INFO, "WPA: 4-Way Handshake failed - "
			"pre-shared key may be incorrect");
		wpas_auth_failed(wpa_s);
	}
	if (!wpa_s->auto_reconnect_disabled ||
	    wpa_s->key_mgmt == WPA_KEY_MGMT_WPS) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Auto connect enabled: try to "
			"reconnect (wps=%d wpa_state=%d)",
			wpa_s->key_mgmt == WPA_KEY_MGMT_WPS,
			wpa_s->wpa_state);
		if (wpa_s->wpa_state == WPA_COMPLETED &&
		    wpa_s->current_ssid &&
		    wpa_s->current_ssid->mode == WPAS_MODE_INFRA &&
		    !locally_generated &&
		    disconnect_reason_recoverable(reason_code)) {
			/*
			 * It looks like the AP has dropped association with
			 * us, but could allow us to get back in. Try to
			 * reconnect to the same BSS without full scan to save
			 * time for some common cases.
			 */
			fast_reconnect = wpa_s->current_bss;
			fast_reconnect_ssid = wpa_s->current_ssid;
		} else if (wpa_s->wpa_state >= WPA_ASSOCIATING)
			wpa_supplicant_req_scan(wpa_s, 0, 100000);
		else
			wpa_dbg(wpa_s, MSG_DEBUG, "Do not request new "
				"immediate scan");
	} else {
		wpa_dbg(wpa_s, MSG_DEBUG, "Auto connect disabled: do not "
			"try to re-connect");
		wpa_s->reassociate = 0;
		wpa_s->disconnected = 1;
		wpa_supplicant_cancel_sched_scan(wpa_s);
	}
	bssid = wpa_s->bssid;
	if (is_zero_ether_addr(bssid))
		bssid = wpa_s->pending_bssid;
	if (wpa_s->wpa_state >= WPA_AUTHENTICATING)
		wpas_connection_failed(wpa_s, bssid);
	wpa_sm_notify_disassoc(wpa_s->wpa);
	if (locally_generated)
		wpa_s->disconnect_reason = -reason_code;
	else
		wpa_s->disconnect_reason = reason_code;
	wpas_notify_disconnect_reason(wpa_s);
	if (wpa_supplicant_dynamic_keys(wpa_s)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "Disconnect event - remove keys");
		wpa_s->keys_cleared = 0;
		wpa_clear_keys(wpa_s, wpa_s->bssid);
	}
	last_ssid = wpa_s->current_ssid;
	wpa_supplicant_mark_disassoc(wpa_s);

	if (authenticating && (wpa_s->drv_flags & WPA_DRIVER_FLAGS_SME)) {
		sme_disassoc_while_authenticating(wpa_s, prev_pending_bssid);
		wpa_s->current_ssid = last_ssid;
	}

	if (fast_reconnect) {
#ifndef CONFIG_NO_SCAN_PROCESSING
		wpa_dbg(wpa_s, MSG_DEBUG, "Try to reconnect to the same BSS");
		if (wpa_supplicant_connect(wpa_s, fast_reconnect,
					   fast_reconnect_ssid) < 0) {
			/* Recover through full scan */
			wpa_supplicant_req_scan(wpa_s, 0, 100000);
		}
#endif /* CONFIG_NO_SCAN_PROCESSING */
	}
}


#ifdef CONFIG_DELAYED_MIC_ERROR_REPORT
void wpa_supplicant_delayed_mic_error_report(void *eloop_ctx, void *sock_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	if (!wpa_s->pending_mic_error_report)
		return;

	wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Sending pending MIC error report");
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

		wpa_blacklist_add(wpa_s, wpa_s->bssid);

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
			wpa_dbg(wpa_s, MSG_DEBUG, "WPA: Delay MIC error "
				"report %d seconds", sec);
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
		wpa_dbg(wpa_s, MSG_DEBUG, "Configured interface was added");
		if (wpa_supplicant_driver_init(wpa_s) < 0) {
			wpa_msg(wpa_s, MSG_INFO, "Failed to initialize the "
				"driver after interface was added");
		}
		wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
		break;
	case EVENT_INTERFACE_REMOVED:
		wpa_dbg(wpa_s, MSG_DEBUG, "Configured interface was removed");
		wpa_s->interface_removed = 1;
		wpa_supplicant_mark_disassoc(wpa_s);
		wpa_supplicant_set_state(wpa_s, WPA_INTERFACE_DISABLED);
		l2_packet_deinit(wpa_s->l2);
		wpa_s->l2 = NULL;
#ifdef CONFIG_IBSS_RSN
		ibss_rsn_deinit(wpa_s->ibss_rsn);
		wpa_s->ibss_rsn = NULL;
#endif /* CONFIG_IBSS_RSN */
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


#ifdef CONFIG_TDLS
static void wpa_supplicant_event_tdls(struct wpa_supplicant *wpa_s,
				      union wpa_event_data *data)
{
	if (data == NULL)
		return;
	switch (data->tdls.oper) {
	case TDLS_REQUEST_SETUP:
		wpa_tdls_start(wpa_s->wpa, data->tdls.peer);
		break;
	case TDLS_REQUEST_TEARDOWN:
		wpa_tdls_send_teardown(wpa_s->wpa, data->tdls.peer,
				       data->tdls.reason_code);
		break;
	}
}
#endif /* CONFIG_TDLS */


#ifdef CONFIG_WNM
static void wpa_supplicant_event_wnm(struct wpa_supplicant *wpa_s,
				     union wpa_event_data *data)
{
	if (data == NULL)
		return;
	switch (data->wnm.oper) {
	case WNM_OPER_SLEEP:
		wpa_printf(MSG_DEBUG, "Start sending WNM-Sleep Request "
			   "(action=%d, intval=%d)",
			   data->wnm.sleep_action, data->wnm.sleep_intval);
		ieee802_11_send_wnmsleep_req(wpa_s, data->wnm.sleep_action,
					     data->wnm.sleep_intval, NULL);
		break;
	}
}
#endif /* CONFIG_WNM */


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
	struct wpa_ssid *ssid;
	if (wpa_s->wpa_state < WPA_ASSOCIATED)
		return;
	if (data == NULL)
		return;
	ssid = wpa_s->current_ssid;
	if (ssid == NULL)
		return;
	if (ssid->mode != WPAS_MODE_IBSS || !wpa_key_mgmt_wpa(ssid->key_mgmt))
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
	wpa_dbg(wpa_s, MSG_DEBUG, "FT: Received FT Action Response: STA "
		MACSTR " TargetAP " MACSTR " status %u",
		MAC2STR(sta_addr), MAC2STR(target_ap_addr), status);

	if (os_memcmp(sta_addr, wpa_s->own_addr, ETH_ALEN) != 0) {
		wpa_dbg(wpa_s, MSG_DEBUG, "FT: Foreign STA Address " MACSTR
			" in FT Action Response", MAC2STR(sta_addr));
		return;
	}

	if (status) {
		wpa_dbg(wpa_s, MSG_DEBUG, "FT: FT Action Response indicates "
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


static void wpa_supplicant_event_unprot_deauth(struct wpa_supplicant *wpa_s,
					       struct unprot_deauth *e)
{
#ifdef CONFIG_IEEE80211W
	wpa_printf(MSG_DEBUG, "Unprotected Deauthentication frame "
		   "dropped: " MACSTR " -> " MACSTR
		   " (reason code %u)",
		   MAC2STR(e->sa), MAC2STR(e->da), e->reason_code);
	sme_event_unprot_disconnect(wpa_s, e->sa, e->da, e->reason_code);
#endif /* CONFIG_IEEE80211W */
}


static void wpa_supplicant_event_unprot_disassoc(struct wpa_supplicant *wpa_s,
						 struct unprot_disassoc *e)
{
#ifdef CONFIG_IEEE80211W
	wpa_printf(MSG_DEBUG, "Unprotected Disassociation frame "
		   "dropped: " MACSTR " -> " MACSTR
		   " (reason code %u)",
		   MAC2STR(e->sa), MAC2STR(e->da), e->reason_code);
	sme_event_unprot_disconnect(wpa_s, e->sa, e->da, e->reason_code);
#endif /* CONFIG_IEEE80211W */
}


void wpa_supplicant_event(void *ctx, enum wpa_event_type event,
			  union wpa_event_data *data)
{
	struct wpa_supplicant *wpa_s = ctx;
	u16 reason_code = 0;
	int locally_generated = 0;

	if (wpa_s->wpa_state == WPA_INTERFACE_DISABLED &&
	    event != EVENT_INTERFACE_ENABLED &&
	    event != EVENT_INTERFACE_STATUS &&
	    event != EVENT_SCHED_SCAN_STOPPED) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"Ignore event %s (%d) while interface is disabled",
			event_to_string(event), event);
		return;
	}

#ifndef CONFIG_NO_STDOUT_DEBUG
{
	int level = MSG_DEBUG;

	if (event == EVENT_RX_MGMT && data->rx_mgmt.frame_len >= 24) {
		const struct ieee80211_hdr *hdr;
		u16 fc;
		hdr = (const struct ieee80211_hdr *) data->rx_mgmt.frame;
		fc = le_to_host16(hdr->frame_control);
		if (WLAN_FC_GET_TYPE(fc) == WLAN_FC_TYPE_MGMT &&
		    WLAN_FC_GET_STYPE(fc) == WLAN_FC_STYPE_BEACON)
			level = MSG_EXCESSIVE;
	}

	wpa_dbg(wpa_s, level, "Event %s (%d) received",
		event_to_string(event), event);
}
#endif /* CONFIG_NO_STDOUT_DEBUG */

	switch (event) {
	case EVENT_AUTH:
		sme_event_auth(wpa_s, data);
		break;
	case EVENT_ASSOC:
		wpa_supplicant_event_assoc(wpa_s, data);
		break;
	case EVENT_DISASSOC:
		wpa_dbg(wpa_s, MSG_DEBUG, "Disassociation notification");
		if (data) {
			wpa_dbg(wpa_s, MSG_DEBUG, " * reason %u%s",
				data->disassoc_info.reason_code,
				data->disassoc_info.locally_generated ?
				" (locally generated)" : "");
			if (data->disassoc_info.addr)
				wpa_dbg(wpa_s, MSG_DEBUG, " * address " MACSTR,
					MAC2STR(data->disassoc_info.addr));
		}
#ifdef CONFIG_AP
		if (wpa_s->ap_iface && data && data->disassoc_info.addr) {
			hostapd_notif_disassoc(wpa_s->ap_iface->bss[0],
					       data->disassoc_info.addr);
			break;
		}
		if (wpa_s->ap_iface) {
			wpa_dbg(wpa_s, MSG_DEBUG, "Ignore disassoc event in "
				"AP mode");
			break;
		}
#endif /* CONFIG_AP */
		if (data) {
			reason_code = data->disassoc_info.reason_code;
			locally_generated =
				data->disassoc_info.locally_generated;
			wpa_hexdump(MSG_DEBUG, "Disassociation frame IE(s)",
				    data->disassoc_info.ie,
				    data->disassoc_info.ie_len);
#ifdef CONFIG_P2P
			wpas_p2p_disassoc_notif(
				wpa_s, data->disassoc_info.addr, reason_code,
				data->disassoc_info.ie,
				data->disassoc_info.ie_len,
				locally_generated);
#endif /* CONFIG_P2P */
		}
		if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_SME)
			sme_event_disassoc(wpa_s, data);
		/* fall through */
	case EVENT_DEAUTH:
		if (event == EVENT_DEAUTH) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"Deauthentication notification");
			if (data) {
				reason_code = data->deauth_info.reason_code;
				locally_generated =
					data->deauth_info.locally_generated;
				wpa_dbg(wpa_s, MSG_DEBUG, " * reason %u%s",
					data->deauth_info.reason_code,
					data->deauth_info.locally_generated ?
					" (locally generated)" : "");
				if (data->deauth_info.addr) {
					wpa_dbg(wpa_s, MSG_DEBUG, " * address "
						MACSTR,
						MAC2STR(data->deauth_info.
							addr));
				}
				wpa_hexdump(MSG_DEBUG,
					    "Deauthentication frame IE(s)",
					    data->deauth_info.ie,
					    data->deauth_info.ie_len);
			}
		}
#ifdef CONFIG_AP
		if (wpa_s->ap_iface && data && data->deauth_info.addr) {
			hostapd_notif_disassoc(wpa_s->ap_iface->bss[0],
					       data->deauth_info.addr);
			break;
		}
		if (wpa_s->ap_iface) {
			wpa_dbg(wpa_s, MSG_DEBUG, "Ignore deauth event in "
				"AP mode");
			break;
		}
#endif /* CONFIG_AP */
		wpa_supplicant_event_disassoc(wpa_s, reason_code,
					      locally_generated);
		if (reason_code == WLAN_REASON_IEEE_802_1X_AUTH_FAILED ||
		    ((wpa_key_mgmt_wpa_ieee8021x(wpa_s->key_mgmt) ||
		      (wpa_s->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA)) &&
		     eapol_sm_failed(wpa_s->eapol)))
			wpas_auth_failed(wpa_s);
#ifdef CONFIG_P2P
		if (event == EVENT_DEAUTH && data) {
			if (wpas_p2p_deauth_notif(wpa_s,
						  data->deauth_info.addr,
						  reason_code,
						  data->deauth_info.ie,
						  data->deauth_info.ie_len,
						  locally_generated) > 0) {
				/*
				 * The interface was removed, so cannot
				 * continue processing any additional
				 * operations after this.
				 */
				break;
			}
		}
#endif /* CONFIG_P2P */
		wpa_supplicant_event_disassoc_finish(wpa_s, reason_code,
						     locally_generated);
		break;
	case EVENT_MICHAEL_MIC_FAILURE:
		wpa_supplicant_event_michael_mic_failure(wpa_s, data);
		break;
#ifndef CONFIG_NO_SCAN_PROCESSING
	case EVENT_SCAN_RESULTS:
		wpa_supplicant_event_scan_results(wpa_s, data);
#ifdef CONFIG_P2P
	if (wpa_s->global->p2p_cb_on_scan_complete && !wpa_s->global->p2p_disabled &&
	    wpa_s->global->p2p != NULL &&
	    wpa_s->wpa_state != WPA_AUTHENTICATING &&
	    wpa_s->wpa_state != WPA_ASSOCIATING) {
		wpa_s->global->p2p_cb_on_scan_complete = 0;
		if (p2p_other_scan_completed(wpa_s->global->p2p) == 1) {
			wpa_dbg(wpa_s, MSG_DEBUG, "P2P: Pending P2P operation "
				"continued after scan result processing");
		}
	}
#endif /* CONFIG_P2P */
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
#ifdef CONFIG_TDLS
	case EVENT_TDLS:
		wpa_supplicant_event_tdls(wpa_s, data);
		break;
#endif /* CONFIG_TDLS */
#ifdef CONFIG_WNM
	case EVENT_WNM:
		wpa_supplicant_event_wnm(wpa_s, data);
		break;
#endif /* CONFIG_WNM */
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
		if (data->assoc_reject.bssid)
			wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_ASSOC_REJECT
				"bssid=" MACSTR	" status_code=%u",
				MAC2STR(data->assoc_reject.bssid),
				data->assoc_reject.status_code);
		else
			wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_ASSOC_REJECT
				"status_code=%u",
				data->assoc_reject.status_code);
		if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_SME)
			sme_event_assoc_reject(wpa_s, data);
		else {
			const u8 *bssid = data->assoc_reject.bssid;
			if (bssid == NULL || is_zero_ether_addr(bssid))
				bssid = wpa_s->pending_bssid;
			wpas_connection_failed(wpa_s, bssid);
			wpa_supplicant_mark_disassoc(wpa_s);
		}
		break;
	case EVENT_AUTH_TIMED_OUT:
		if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_SME)
			sme_event_auth_timed_out(wpa_s, data);
		break;
	case EVENT_ASSOC_TIMED_OUT:
		if (wpa_s->drv_flags & WPA_DRIVER_FLAGS_SME)
			sme_event_assoc_timed_out(wpa_s, data);
		break;
	case EVENT_TX_STATUS:
		wpa_dbg(wpa_s, MSG_DEBUG, "EVENT_TX_STATUS dst=" MACSTR
			" type=%d stype=%d",
			MAC2STR(data->tx_status.dst),
			data->tx_status.type, data->tx_status.stype);
#ifdef CONFIG_AP
		if (wpa_s->ap_iface == NULL) {
#ifdef CONFIG_OFFCHANNEL
			if (data->tx_status.type == WLAN_FC_TYPE_MGMT &&
			    data->tx_status.stype == WLAN_FC_STYPE_ACTION)
				offchannel_send_action_tx_status(
					wpa_s, data->tx_status.dst,
					data->tx_status.data,
					data->tx_status.data_len,
					data->tx_status.ack ?
					OFFCHANNEL_SEND_ACTION_SUCCESS :
					OFFCHANNEL_SEND_ACTION_NO_ACK);
#endif /* CONFIG_OFFCHANNEL */
			break;
		}
#endif /* CONFIG_AP */
#ifdef CONFIG_OFFCHANNEL
		wpa_dbg(wpa_s, MSG_DEBUG, "EVENT_TX_STATUS pending_dst="
			MACSTR, MAC2STR(wpa_s->parent->pending_action_dst));
		/*
		 * Catch TX status events for Action frames we sent via group
		 * interface in GO mode.
		 */
		if (data->tx_status.type == WLAN_FC_TYPE_MGMT &&
		    data->tx_status.stype == WLAN_FC_STYPE_ACTION &&
		    os_memcmp(wpa_s->parent->pending_action_dst,
			      data->tx_status.dst, ETH_ALEN) == 0) {
			offchannel_send_action_tx_status(
				wpa_s->parent, data->tx_status.dst,
				data->tx_status.data,
				data->tx_status.data_len,
				data->tx_status.ack ?
				OFFCHANNEL_SEND_ACTION_SUCCESS :
				OFFCHANNEL_SEND_ACTION_NO_ACK);
			break;
		}
#endif /* CONFIG_OFFCHANNEL */
#ifdef CONFIG_AP
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
#endif /* CONFIG_AP */
		break;
#ifdef CONFIG_AP
	case EVENT_EAPOL_TX_STATUS:
		ap_eapol_tx_status(wpa_s, data->eapol_tx_status.dst,
				   data->eapol_tx_status.data,
				   data->eapol_tx_status.data_len,
				   data->eapol_tx_status.ack);
		break;
	case EVENT_DRIVER_CLIENT_POLL_OK:
		ap_client_poll_ok(wpa_s, data->client_poll.addr);
		break;
	case EVENT_RX_FROM_UNKNOWN:
		if (wpa_s->ap_iface == NULL)
			break;
		ap_rx_from_unknown_sta(wpa_s, data->rx_from_unknown.addr,
				       data->rx_from_unknown.wds);
		break;
	case EVENT_CH_SWITCH:
		if (!data)
			break;
		if (!wpa_s->ap_iface) {
			wpa_dbg(wpa_s, MSG_DEBUG, "AP: Ignore channel switch "
				"event in non-AP mode");
			break;
		}

#ifdef CONFIG_AP
		wpas_ap_ch_switch(wpa_s, data->ch_switch.freq,
				  data->ch_switch.ht_enabled,
				  data->ch_switch.ch_offset);
#endif /* CONFIG_AP */
		break;
	case EVENT_RX_MGMT: {
		u16 fc, stype;
		const struct ieee80211_mgmt *mgmt;

		mgmt = (const struct ieee80211_mgmt *)
			data->rx_mgmt.frame;
		fc = le_to_host16(mgmt->frame_control);
		stype = WLAN_FC_GET_STYPE(fc);

		if (wpa_s->ap_iface == NULL) {
#ifdef CONFIG_P2P
			if (stype == WLAN_FC_STYPE_PROBE_REQ &&
			    data->rx_mgmt.frame_len > 24) {
				const u8 *src = mgmt->sa;
				const u8 *ie = mgmt->u.probe_req.variable;
				size_t ie_len = data->rx_mgmt.frame_len -
					(mgmt->u.probe_req.variable -
					 data->rx_mgmt.frame);
				wpas_p2p_probe_req_rx(
					wpa_s, src, mgmt->da,
					mgmt->bssid, ie, ie_len,
					data->rx_mgmt.ssi_signal);
				break;
			}
#endif /* CONFIG_P2P */
			wpa_dbg(wpa_s, MSG_DEBUG, "AP: ignore received "
				"management frame in non-AP mode");
			break;
		}

		if (stype == WLAN_FC_STYPE_PROBE_REQ &&
		    data->rx_mgmt.frame_len > 24) {
			const u8 *ie = mgmt->u.probe_req.variable;
			size_t ie_len = data->rx_mgmt.frame_len -
				(mgmt->u.probe_req.variable -
				 data->rx_mgmt.frame);

			wpas_notify_preq(wpa_s, mgmt->sa, mgmt->da,
					 mgmt->bssid, ie, ie_len,
					 data->rx_mgmt.ssi_signal);
		}

		ap_mgmt_rx(wpa_s, &data->rx_mgmt);
		break;
		}
#endif /* CONFIG_AP */
	case EVENT_RX_ACTION:
		wpa_dbg(wpa_s, MSG_DEBUG, "Received Action frame: SA=" MACSTR
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
#ifdef CONFIG_IEEE80211W
#ifdef CONFIG_SME
		if (data->rx_action.category == WLAN_ACTION_SA_QUERY) {
			sme_sa_query_rx(wpa_s, data->rx_action.sa,
					data->rx_action.data,
					data->rx_action.len);
			break;
		}
#endif /* CONFIG_SME */
#endif /* CONFIG_IEEE80211W */
#ifdef CONFIG_WNM
		if (data->rx_action.category == WLAN_ACTION_WNM) {
			ieee802_11_rx_wnm_action(wpa_s, &data->rx_action);
			break;
		}
#endif /* CONFIG_WNM */
#ifdef CONFIG_GAS
		if (data->rx_action.category == WLAN_ACTION_PUBLIC &&
		    gas_query_rx(wpa_s->gas, data->rx_action.da,
				 data->rx_action.sa, data->rx_action.bssid,
				 data->rx_action.data, data->rx_action.len,
				 data->rx_action.freq) == 0)
			break;
#endif /* CONFIG_GAS */
#ifdef CONFIG_TDLS
		if (data->rx_action.category == WLAN_ACTION_PUBLIC &&
		    data->rx_action.len >= 4 &&
		    data->rx_action.data[0] == WLAN_TDLS_DISCOVERY_RESPONSE) {
			wpa_dbg(wpa_s, MSG_DEBUG, "TDLS: Received Discovery "
				"Response from " MACSTR,
				MAC2STR(data->rx_action.sa));
			break;
		}
#endif /* CONFIG_TDLS */
#ifdef CONFIG_P2P
		wpas_p2p_rx_action(wpa_s, data->rx_action.da,
				   data->rx_action.sa,
				   data->rx_action.bssid,
				   data->rx_action.category,
				   data->rx_action.data,
				   data->rx_action.len, data->rx_action.freq);
#endif /* CONFIG_P2P */
		break;
	case EVENT_RX_PROBE_REQ:
		if (data->rx_probe_req.sa == NULL ||
		    data->rx_probe_req.ie == NULL)
			break;
#ifdef CONFIG_AP
		if (wpa_s->ap_iface) {
			hostapd_probe_req_rx(wpa_s->ap_iface->bss[0],
					     data->rx_probe_req.sa,
					     data->rx_probe_req.da,
					     data->rx_probe_req.bssid,
					     data->rx_probe_req.ie,
					     data->rx_probe_req.ie_len,
					     data->rx_probe_req.ssi_signal);
			break;
		}
#endif /* CONFIG_AP */
#ifdef CONFIG_P2P
		wpas_p2p_probe_req_rx(wpa_s, data->rx_probe_req.sa,
				      data->rx_probe_req.da,
				      data->rx_probe_req.bssid,
				      data->rx_probe_req.ie,
				      data->rx_probe_req.ie_len,
				      data->rx_probe_req.ssi_signal);
#endif /* CONFIG_P2P */
		break;
	case EVENT_REMAIN_ON_CHANNEL:
#ifdef CONFIG_OFFCHANNEL
		offchannel_remain_on_channel_cb(
			wpa_s, data->remain_on_channel.freq,
			data->remain_on_channel.duration);
#endif /* CONFIG_OFFCHANNEL */
#ifdef CONFIG_P2P
		wpas_p2p_remain_on_channel_cb(
			wpa_s, data->remain_on_channel.freq,
			data->remain_on_channel.duration);
#endif /* CONFIG_P2P */
		break;
	case EVENT_CANCEL_REMAIN_ON_CHANNEL:
#ifdef CONFIG_OFFCHANNEL
		offchannel_cancel_remain_on_channel_cb(
			wpa_s, data->remain_on_channel.freq);
#endif /* CONFIG_OFFCHANNEL */
#ifdef CONFIG_P2P
		wpas_p2p_cancel_remain_on_channel_cb(
			wpa_s, data->remain_on_channel.freq);
#endif /* CONFIG_P2P */
		break;
#ifdef CONFIG_P2P
	case EVENT_P2P_DEV_FOUND: {
		struct p2p_peer_info peer_info;

		os_memset(&peer_info, 0, sizeof(peer_info));
		if (data->p2p_dev_found.dev_addr)
			os_memcpy(peer_info.p2p_device_addr,
				  data->p2p_dev_found.dev_addr, ETH_ALEN);
		if (data->p2p_dev_found.pri_dev_type)
			os_memcpy(peer_info.pri_dev_type,
				  data->p2p_dev_found.pri_dev_type,
				  sizeof(peer_info.pri_dev_type));
		if (data->p2p_dev_found.dev_name)
			os_strlcpy(peer_info.device_name,
				   data->p2p_dev_found.dev_name,
				   sizeof(peer_info.device_name));
		peer_info.config_methods = data->p2p_dev_found.config_methods;
		peer_info.dev_capab = data->p2p_dev_found.dev_capab;
		peer_info.group_capab = data->p2p_dev_found.group_capab;

		/*
		 * FIX: new_device=1 is not necessarily correct. We should
		 * maintain a P2P peer database in wpa_supplicant and update
		 * this information based on whether the peer is truly new.
		 */
		wpas_dev_found(wpa_s, data->p2p_dev_found.addr, &peer_info, 1);
		break;
		}
	case EVENT_P2P_GO_NEG_REQ_RX:
		wpas_go_neg_req_rx(wpa_s, data->p2p_go_neg_req_rx.src,
				   data->p2p_go_neg_req_rx.dev_passwd_id);
		break;
	case EVENT_P2P_GO_NEG_COMPLETED:
		wpas_go_neg_completed(wpa_s, data->p2p_go_neg_completed.res);
		break;
	case EVENT_P2P_PROV_DISC_REQUEST:
		wpas_prov_disc_req(wpa_s, data->p2p_prov_disc_req.peer,
				   data->p2p_prov_disc_req.config_methods,
				   data->p2p_prov_disc_req.dev_addr,
				   data->p2p_prov_disc_req.pri_dev_type,
				   data->p2p_prov_disc_req.dev_name,
				   data->p2p_prov_disc_req.supp_config_methods,
				   data->p2p_prov_disc_req.dev_capab,
				   data->p2p_prov_disc_req.group_capab,
				   NULL, 0);
		break;
	case EVENT_P2P_PROV_DISC_RESPONSE:
		wpas_prov_disc_resp(wpa_s, data->p2p_prov_disc_resp.peer,
				    data->p2p_prov_disc_resp.config_methods);
		break;
	case EVENT_P2P_SD_REQUEST:
		wpas_sd_request(wpa_s, data->p2p_sd_req.freq,
				data->p2p_sd_req.sa,
				data->p2p_sd_req.dialog_token,
				data->p2p_sd_req.update_indic,
				data->p2p_sd_req.tlvs,
				data->p2p_sd_req.tlvs_len);
		break;
	case EVENT_P2P_SD_RESPONSE:
		wpas_sd_response(wpa_s, data->p2p_sd_resp.sa,
				 data->p2p_sd_resp.update_indic,
				 data->p2p_sd_resp.tlvs,
				 data->p2p_sd_resp.tlvs_len);
		break;
#endif /* CONFIG_P2P */
	case EVENT_EAPOL_RX:
		wpa_supplicant_rx_eapol(wpa_s, data->eapol_rx.src,
					data->eapol_rx.data,
					data->eapol_rx.data_len);
		break;
	case EVENT_SIGNAL_CHANGE:
		bgscan_notify_signal_change(
			wpa_s, data->signal_change.above_threshold,
			data->signal_change.current_signal,
			data->signal_change.current_noise,
			data->signal_change.current_txrate);
		break;
	case EVENT_INTERFACE_ENABLED:
		wpa_dbg(wpa_s, MSG_DEBUG, "Interface was enabled");
		if (wpa_s->wpa_state == WPA_INTERFACE_DISABLED) {
			wpa_supplicant_update_mac_addr(wpa_s);
#ifdef CONFIG_AP
			if (!wpa_s->ap_iface) {
				wpa_supplicant_set_state(wpa_s,
							 WPA_DISCONNECTED);
				wpa_supplicant_req_scan(wpa_s, 0, 0);
			} else
				wpa_supplicant_set_state(wpa_s,
							 WPA_COMPLETED);
#else /* CONFIG_AP */
			wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
			wpa_supplicant_req_scan(wpa_s, 0, 0);
#endif /* CONFIG_AP */
		}
		break;
	case EVENT_INTERFACE_DISABLED:
		wpa_dbg(wpa_s, MSG_DEBUG, "Interface was disabled");
		wpa_supplicant_mark_disassoc(wpa_s);
		wpa_supplicant_set_state(wpa_s, WPA_INTERFACE_DISABLED);
		break;
	case EVENT_CHANNEL_LIST_CHANGED:
		if (wpa_s->drv_priv == NULL)
			break; /* Ignore event during drv initialization */

		free_hw_features(wpa_s);
		wpa_s->hw.modes = wpa_drv_get_hw_feature_data(
			wpa_s, &wpa_s->hw.num_modes, &wpa_s->hw.flags);

#ifdef CONFIG_P2P
		wpas_p2p_update_channel_list(wpa_s);
#endif /* CONFIG_P2P */
		break;
	case EVENT_INTERFACE_UNAVAILABLE:
#ifdef CONFIG_P2P
		wpas_p2p_interface_unavailable(wpa_s);
#endif /* CONFIG_P2P */
		break;
	case EVENT_BEST_CHANNEL:
		wpa_dbg(wpa_s, MSG_DEBUG, "Best channel event received "
			"(%d %d %d)",
			data->best_chan.freq_24, data->best_chan.freq_5,
			data->best_chan.freq_overall);
		wpa_s->best_24_freq = data->best_chan.freq_24;
		wpa_s->best_5_freq = data->best_chan.freq_5;
		wpa_s->best_overall_freq = data->best_chan.freq_overall;
#ifdef CONFIG_P2P
		wpas_p2p_update_best_channels(wpa_s, data->best_chan.freq_24,
					      data->best_chan.freq_5,
					      data->best_chan.freq_overall);
#endif /* CONFIG_P2P */
		break;
	case EVENT_UNPROT_DEAUTH:
		wpa_supplicant_event_unprot_deauth(wpa_s,
						   &data->unprot_deauth);
		break;
	case EVENT_UNPROT_DISASSOC:
		wpa_supplicant_event_unprot_disassoc(wpa_s,
						     &data->unprot_disassoc);
		break;
	case EVENT_STATION_LOW_ACK:
#ifdef CONFIG_AP
		if (wpa_s->ap_iface && data)
			hostapd_event_sta_low_ack(wpa_s->ap_iface->bss[0],
						  data->low_ack.addr);
#endif /* CONFIG_AP */
#ifdef CONFIG_TDLS
		if (data)
			wpa_tdls_disable_link(wpa_s->wpa, data->low_ack.addr);
#endif /* CONFIG_TDLS */
		break;
	case EVENT_IBSS_PEER_LOST:
#ifdef CONFIG_IBSS_RSN
		ibss_rsn_stop(wpa_s->ibss_rsn, data->ibss_peer_lost.peer);
#endif /* CONFIG_IBSS_RSN */
		break;
	case EVENT_DRIVER_GTK_REKEY:
		if (os_memcmp(data->driver_gtk_rekey.bssid,
			      wpa_s->bssid, ETH_ALEN))
			break;
		if (!wpa_s->wpa)
			break;
		wpa_sm_update_replay_ctr(wpa_s->wpa,
					 data->driver_gtk_rekey.replay_ctr);
		break;
	case EVENT_SCHED_SCAN_STOPPED:
		wpa_s->sched_scanning = 0;
		wpa_supplicant_notify_scanning(wpa_s, 0);

		if (wpa_s->wpa_state == WPA_INTERFACE_DISABLED)
			break;

		/*
		 * If we timed out, start a new sched scan to continue
		 * searching for more SSIDs.
		 */
		if (wpa_s->sched_scan_timed_out)
			wpa_supplicant_req_sched_scan(wpa_s);
		break;
	case EVENT_WPS_BUTTON_PUSHED:
#ifdef CONFIG_WPS
		wpas_wps_start_pbc(wpa_s, NULL, 0);
#endif /* CONFIG_WPS */
		break;
	default:
		wpa_msg(wpa_s, MSG_INFO, "Unknown event %d", event);
		break;
	}
}
