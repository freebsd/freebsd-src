/*
 * wpa_supplicant - SME
 * Copyright (c) 2009-2014, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "utils/eloop.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "common/wpa_common.h"
#include "common/sae.h"
#include "rsn_supp/wpa.h"
#include "rsn_supp/pmksa_cache.h"
#include "config.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "wpas_glue.h"
#include "wps_supplicant.h"
#include "p2p_supplicant.h"
#include "notify.h"
#include "bss.h"
#include "scan.h"
#include "sme.h"
#include "hs20_supplicant.h"

#define SME_AUTH_TIMEOUT 5
#define SME_ASSOC_TIMEOUT 5

static void sme_auth_timer(void *eloop_ctx, void *timeout_ctx);
static void sme_assoc_timer(void *eloop_ctx, void *timeout_ctx);
static void sme_obss_scan_timeout(void *eloop_ctx, void *timeout_ctx);
#ifdef CONFIG_IEEE80211W
static void sme_stop_sa_query(struct wpa_supplicant *wpa_s);
#endif /* CONFIG_IEEE80211W */


#ifdef CONFIG_SAE

static int index_within_array(const int *array, int idx)
{
	int i;
	for (i = 0; i < idx; i++) {
		if (array[i] <= 0)
			return 0;
	}
	return 1;
}


static int sme_set_sae_group(struct wpa_supplicant *wpa_s)
{
	int *groups = wpa_s->conf->sae_groups;
	int default_groups[] = { 19, 20, 21, 25, 26, 0 };

	if (!groups || groups[0] <= 0)
		groups = default_groups;

	/* Configuration may have changed, so validate current index */
	if (!index_within_array(groups, wpa_s->sme.sae_group_index))
		return -1;

	for (;;) {
		int group = groups[wpa_s->sme.sae_group_index];
		if (group <= 0)
			break;
		if (sae_set_group(&wpa_s->sme.sae, group) == 0) {
			wpa_dbg(wpa_s, MSG_DEBUG, "SME: Selected SAE group %d",
				wpa_s->sme.sae.group);
		       return 0;
		}
		wpa_s->sme.sae_group_index++;
	}

	return -1;
}


static struct wpabuf * sme_auth_build_sae_commit(struct wpa_supplicant *wpa_s,
						 struct wpa_ssid *ssid,
						 const u8 *bssid)
{
	struct wpabuf *buf;
	size_t len;

	if (ssid->passphrase == NULL) {
		wpa_printf(MSG_DEBUG, "SAE: No password available");
		return NULL;
	}

	if (sme_set_sae_group(wpa_s) < 0) {
		wpa_printf(MSG_DEBUG, "SAE: Failed to select group");
		return NULL;
	}

	if (sae_prepare_commit(wpa_s->own_addr, bssid,
			       (u8 *) ssid->passphrase,
			       os_strlen(ssid->passphrase),
			       &wpa_s->sme.sae) < 0) {
		wpa_printf(MSG_DEBUG, "SAE: Could not pick PWE");
		return NULL;
	}

	len = wpa_s->sme.sae_token ? wpabuf_len(wpa_s->sme.sae_token) : 0;
	buf = wpabuf_alloc(4 + SAE_COMMIT_MAX_LEN + len);
	if (buf == NULL)
		return NULL;

	wpabuf_put_le16(buf, 1); /* Transaction seq# */
	wpabuf_put_le16(buf, WLAN_STATUS_SUCCESS);
	sae_write_commit(&wpa_s->sme.sae, buf, wpa_s->sme.sae_token);

	return buf;
}


static struct wpabuf * sme_auth_build_sae_confirm(struct wpa_supplicant *wpa_s)
{
	struct wpabuf *buf;

	buf = wpabuf_alloc(4 + SAE_CONFIRM_MAX_LEN);
	if (buf == NULL)
		return NULL;

	wpabuf_put_le16(buf, 2); /* Transaction seq# */
	wpabuf_put_le16(buf, WLAN_STATUS_SUCCESS);
	sae_write_confirm(&wpa_s->sme.sae, buf);

	return buf;
}

#endif /* CONFIG_SAE */


/**
 * sme_auth_handle_rrm - Handle RRM aspects of current authentication attempt
 * @wpa_s: Pointer to wpa_supplicant data
 * @bss: Pointer to the bss which is the target of authentication attempt
 */
static void sme_auth_handle_rrm(struct wpa_supplicant *wpa_s,
				struct wpa_bss *bss)
{
	const u8 rrm_ie_len = 5;
	u8 *pos;
	const u8 *rrm_ie;

	wpa_s->rrm.rrm_used = 0;

	wpa_printf(MSG_DEBUG,
		   "RRM: Determining whether RRM can be used - device support: 0x%x",
		   wpa_s->drv_rrm_flags);

	rrm_ie = wpa_bss_get_ie(bss, WLAN_EID_RRM_ENABLED_CAPABILITIES);
	if (!rrm_ie || !(bss->caps & IEEE80211_CAP_RRM)) {
		wpa_printf(MSG_DEBUG, "RRM: No RRM in network");
		return;
	}

	if (!(wpa_s->drv_rrm_flags &
	      WPA_DRIVER_FLAGS_DS_PARAM_SET_IE_IN_PROBES) ||
	    !(wpa_s->drv_rrm_flags & WPA_DRIVER_FLAGS_QUIET)) {
		wpa_printf(MSG_DEBUG,
			   "RRM: Insufficient RRM support in driver - do not use RRM");
		return;
	}

	if (sizeof(wpa_s->sme.assoc_req_ie) <
	    wpa_s->sme.assoc_req_ie_len + rrm_ie_len + 2) {
		wpa_printf(MSG_INFO,
			   "RRM: Unable to use RRM, no room for RRM IE");
		return;
	}

	wpa_printf(MSG_DEBUG, "RRM: Adding RRM IE to Association Request");
	pos = wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len;
	os_memset(pos, 0, 2 + rrm_ie_len);
	*pos++ = WLAN_EID_RRM_ENABLED_CAPABILITIES;
	*pos++ = rrm_ie_len;

	/* Set supported capabilites flags */
	if (wpa_s->drv_rrm_flags & WPA_DRIVER_FLAGS_TX_POWER_INSERTION)
		*pos |= WLAN_RRM_CAPS_LINK_MEASUREMENT;

	wpa_s->sme.assoc_req_ie_len += rrm_ie_len + 2;
	wpa_s->rrm.rrm_used = 1;
}


static void sme_send_authentication(struct wpa_supplicant *wpa_s,
				    struct wpa_bss *bss, struct wpa_ssid *ssid,
				    int start)
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
	struct wpabuf *resp = NULL;
	u8 ext_capab[18];
	int ext_capab_len;
	int skip_auth;

	if (bss == NULL) {
		wpa_msg(wpa_s, MSG_ERROR, "SME: No scan result available for "
			"the network");
		wpas_connect_work_done(wpa_s);
		return;
	}

	skip_auth = wpa_s->conf->reassoc_same_bss_optim &&
		wpa_s->reassoc_same_bss;
	wpa_s->current_bss = bss;

	os_memset(&params, 0, sizeof(params));
	wpa_s->reassociate = 0;

	params.freq = bss->freq;
	params.bssid = bss->bssid;
	params.ssid = bss->ssid;
	params.ssid_len = bss->ssid_len;
	params.p2p = ssid->p2p_group;

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
	wpa_dbg(wpa_s, MSG_DEBUG, "Automatic auth_alg selection: 0x%x",
		params.auth_alg);
	if (ssid->auth_alg) {
		params.auth_alg = ssid->auth_alg;
		wpa_dbg(wpa_s, MSG_DEBUG, "Overriding auth_alg selection: "
			"0x%x", params.auth_alg);
	}
#ifdef CONFIG_SAE
	wpa_s->sme.sae_pmksa_caching = 0;
	if (wpa_key_mgmt_sae(ssid->key_mgmt)) {
		const u8 *rsn;
		struct wpa_ie_data ied;

		rsn = wpa_bss_get_ie(bss, WLAN_EID_RSN);
		if (!rsn) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"SAE enabled, but target BSS does not advertise RSN");
		} else if (wpa_parse_wpa_ie(rsn, 2 + rsn[1], &ied) == 0 &&
			   wpa_key_mgmt_sae(ied.key_mgmt)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "Using SAE auth_alg");
			params.auth_alg = WPA_AUTH_ALG_SAE;
		} else {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"SAE enabled, but target BSS does not advertise SAE AKM for RSN");
		}
	}
#endif /* CONFIG_SAE */

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
	    wpa_key_mgmt_wpa(ssid->key_mgmt)) {
		int try_opportunistic;
		try_opportunistic = (ssid->proactive_key_caching < 0 ?
				     wpa_s->conf->okc :
				     ssid->proactive_key_caching) &&
			(ssid->proto & WPA_PROTO_RSN);
		if (pmksa_cache_set_current(wpa_s->wpa, NULL, bss->bssid,
					    wpa_s->current_ssid,
					    try_opportunistic) == 0)
			eapol_sm_notify_pmkid_attempt(wpa_s->eapol);
		wpa_s->sme.assoc_req_ie_len = sizeof(wpa_s->sme.assoc_req_ie);
		if (wpa_supplicant_set_suites(wpa_s, bss, ssid,
					      wpa_s->sme.assoc_req_ie,
					      &wpa_s->sme.assoc_req_ie_len)) {
			wpa_msg(wpa_s, MSG_WARNING, "SME: Failed to set WPA "
				"key management and encryption suites");
			wpas_connect_work_done(wpa_s);
			return;
		}
	} else if ((ssid->key_mgmt & WPA_KEY_MGMT_IEEE8021X_NO_WPA) &&
		   wpa_key_mgmt_wpa_ieee8021x(ssid->key_mgmt)) {
		/*
		 * Both WPA and non-WPA IEEE 802.1X enabled in configuration -
		 * use non-WPA since the scan results did not indicate that the
		 * AP is using WPA or WPA2.
		 */
		wpa_supplicant_set_non_wpa_policy(wpa_s, ssid);
		wpa_s->sme.assoc_req_ie_len = 0;
	} else if (wpa_key_mgmt_wpa_any(ssid->key_mgmt)) {
		wpa_s->sme.assoc_req_ie_len = sizeof(wpa_s->sme.assoc_req_ie);
		if (wpa_supplicant_set_suites(wpa_s, NULL, ssid,
					      wpa_s->sme.assoc_req_ie,
					      &wpa_s->sme.assoc_req_ie_len)) {
			wpa_msg(wpa_s, MSG_WARNING, "SME: Failed to set WPA "
				"key management and encryption suites (no "
				"scan results)");
			wpas_connect_work_done(wpa_s);
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

	if (md && wpa_key_mgmt_ft(ssid->key_mgmt)) {
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
			wpa_dbg(wpa_s, MSG_DEBUG, "SME: Trying to use FT "
				"over-the-air");
			params.auth_alg = WPA_AUTH_ALG_FT;
			params.ie = wpa_s->sme.ft_ies;
			params.ie_len = wpa_s->sme.ft_ies_len;
		}
	}
#endif /* CONFIG_IEEE80211R */

#ifdef CONFIG_IEEE80211W
	wpa_s->sme.mfp = wpas_get_ssid_pmf(wpa_s, ssid);
	if (wpa_s->sme.mfp != NO_MGMT_FRAME_PROTECTION) {
		const u8 *rsn = wpa_bss_get_ie(bss, WLAN_EID_RSN);
		struct wpa_ie_data _ie;
		if (rsn && wpa_parse_wpa_ie(rsn, 2 + rsn[1], &_ie) == 0 &&
		    _ie.capabilities &
		    (WPA_CAPABILITY_MFPC | WPA_CAPABILITY_MFPR)) {
			wpa_dbg(wpa_s, MSG_DEBUG, "SME: Selected AP supports "
				"MFP: require MFP");
			wpa_s->sme.mfp = MGMT_FRAME_PROTECTION_REQUIRED;
		}
	}
#endif /* CONFIG_IEEE80211W */

#ifdef CONFIG_P2P
	if (wpa_s->global->p2p) {
		u8 *pos;
		size_t len;
		int res;
		pos = wpa_s->sme.assoc_req_ie + wpa_s->sme.assoc_req_ie_len;
		len = sizeof(wpa_s->sme.assoc_req_ie) -
			wpa_s->sme.assoc_req_ie_len;
		res = wpas_p2p_assoc_req_ie(wpa_s, bss, pos, len,
					    ssid->p2p_group);
		if (res >= 0)
			wpa_s->sme.assoc_req_ie_len += res;
	}
#endif /* CONFIG_P2P */

#ifdef CONFIG_HS20
	if (is_hs20_network(wpa_s, ssid, bss)) {
		struct wpabuf *hs20;
		hs20 = wpabuf_alloc(20);
		if (hs20) {
			int pps_mo_id = hs20_get_pps_mo_id(wpa_s, ssid);
			size_t len;

			wpas_hs20_add_indication(hs20, pps_mo_id);
			len = sizeof(wpa_s->sme.assoc_req_ie) -
				wpa_s->sme.assoc_req_ie_len;
			if (wpabuf_len(hs20) <= len) {
				os_memcpy(wpa_s->sme.assoc_req_ie +
					  wpa_s->sme.assoc_req_ie_len,
					  wpabuf_head(hs20), wpabuf_len(hs20));
				wpa_s->sme.assoc_req_ie_len += wpabuf_len(hs20);
			}
			wpabuf_free(hs20);
		}
	}
#endif /* CONFIG_HS20 */

#ifdef CONFIG_FST
	if (wpa_s->fst_ies) {
		int fst_ies_len = wpabuf_len(wpa_s->fst_ies);

		if (wpa_s->sme.assoc_req_ie_len + fst_ies_len <=
		    sizeof(wpa_s->sme.assoc_req_ie)) {
			os_memcpy(wpa_s->sme.assoc_req_ie +
				  wpa_s->sme.assoc_req_ie_len,
				  wpabuf_head(wpa_s->fst_ies),
				  fst_ies_len);
			wpa_s->sme.assoc_req_ie_len += fst_ies_len;
		}
	}
#endif /* CONFIG_FST */

	ext_capab_len = wpas_build_ext_capab(wpa_s, ext_capab,
					     sizeof(ext_capab));
	if (ext_capab_len > 0) {
		u8 *pos = wpa_s->sme.assoc_req_ie;
		if (wpa_s->sme.assoc_req_ie_len > 0 && pos[0] == WLAN_EID_RSN)
			pos += 2 + pos[1];
		os_memmove(pos + ext_capab_len, pos,
			   wpa_s->sme.assoc_req_ie_len -
			   (pos - wpa_s->sme.assoc_req_ie));
		wpa_s->sme.assoc_req_ie_len += ext_capab_len;
		os_memcpy(pos, ext_capab, ext_capab_len);
	}

	if (wpa_s->vendor_elem[VENDOR_ELEM_ASSOC_REQ]) {
		struct wpabuf *buf = wpa_s->vendor_elem[VENDOR_ELEM_ASSOC_REQ];
		size_t len;

		len = sizeof(wpa_s->sme.assoc_req_ie) -
			wpa_s->sme.assoc_req_ie_len;
		if (wpabuf_len(buf) <= len) {
			os_memcpy(wpa_s->sme.assoc_req_ie +
				  wpa_s->sme.assoc_req_ie_len,
				  wpabuf_head(buf), wpabuf_len(buf));
			wpa_s->sme.assoc_req_ie_len += wpabuf_len(buf);
		}
	}

	sme_auth_handle_rrm(wpa_s, bss);

#ifdef CONFIG_SAE
	if (!skip_auth && params.auth_alg == WPA_AUTH_ALG_SAE &&
	    pmksa_cache_set_current(wpa_s->wpa, NULL, bss->bssid, ssid, 0) == 0)
	{
		wpa_dbg(wpa_s, MSG_DEBUG,
			"PMKSA cache entry found - try to use PMKSA caching instead of new SAE authentication");
		params.auth_alg = WPA_AUTH_ALG_OPEN;
		wpa_s->sme.sae_pmksa_caching = 1;
	}

	if (!skip_auth && params.auth_alg == WPA_AUTH_ALG_SAE) {
		if (start)
			resp = sme_auth_build_sae_commit(wpa_s, ssid,
							 bss->bssid);
		else
			resp = sme_auth_build_sae_confirm(wpa_s);
		if (resp == NULL) {
			wpas_connection_failed(wpa_s, bss->bssid);
			return;
		}
		params.sae_data = wpabuf_head(resp);
		params.sae_data_len = wpabuf_len(resp);
		wpa_s->sme.sae.state = start ? SAE_COMMITTED : SAE_CONFIRMED;
	}
#endif /* CONFIG_SAE */

	wpa_supplicant_cancel_sched_scan(wpa_s);
	wpa_supplicant_cancel_scan(wpa_s);

	wpa_msg(wpa_s, MSG_INFO, "SME: Trying to authenticate with " MACSTR
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

#ifdef CONFIG_P2P
	/*
	 * If multi-channel concurrency is not supported, check for any
	 * frequency conflict. In case of any frequency conflict, remove the
	 * least prioritized connection.
	 */
	if (wpa_s->num_multichan_concurrent < 2) {
		int freq, num;
		num = get_shared_radio_freqs(wpa_s, &freq, 1);
		if (num > 0 && freq > 0 && freq != params.freq) {
			wpa_printf(MSG_DEBUG,
				   "Conflicting frequency found (%d != %d)",
				   freq, params.freq);
			if (wpas_p2p_handle_frequency_conflicts(wpa_s,
								params.freq,
								ssid) < 0) {
				wpas_connection_failed(wpa_s, bss->bssid);
				wpa_supplicant_mark_disassoc(wpa_s);
				wpabuf_free(resp);
				wpas_connect_work_done(wpa_s);
				return;
			}
		}
	}
#endif /* CONFIG_P2P */

	if (skip_auth) {
		wpa_msg(wpa_s, MSG_DEBUG,
			"SME: Skip authentication step on reassoc-to-same-BSS");
		wpabuf_free(resp);
		sme_associate(wpa_s, ssid->mode, bss->bssid, WLAN_AUTH_OPEN);
		return;
	}


	wpa_s->sme.auth_alg = params.auth_alg;
	if (wpa_drv_authenticate(wpa_s, &params) < 0) {
		wpa_msg(wpa_s, MSG_INFO, "SME: Authentication request to the "
			"driver failed");
		wpas_connection_failed(wpa_s, bss->bssid);
		wpa_supplicant_mark_disassoc(wpa_s);
		wpabuf_free(resp);
		wpas_connect_work_done(wpa_s);
		return;
	}

	eloop_register_timeout(SME_AUTH_TIMEOUT, 0, sme_auth_timer, wpa_s,
			       NULL);

	/*
	 * Association will be started based on the authentication event from
	 * the driver.
	 */

	wpabuf_free(resp);
}


static void sme_auth_start_cb(struct wpa_radio_work *work, int deinit)
{
	struct wpa_connect_work *cwork = work->ctx;
	struct wpa_supplicant *wpa_s = work->wpa_s;

	if (deinit) {
		if (work->started)
			wpa_s->connect_work = NULL;

		wpas_connect_work_free(cwork);
		return;
	}

	wpa_s->connect_work = work;

	if (cwork->bss_removed ||
	    !wpas_valid_bss_ssid(wpa_s, cwork->bss, cwork->ssid) ||
	    wpas_network_disabled(wpa_s, cwork->ssid)) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: BSS/SSID entry for authentication not valid anymore - drop connection attempt");
		wpas_connect_work_done(wpa_s);
		return;
	}

	sme_send_authentication(wpa_s, cwork->bss, cwork->ssid, 1);
}


void sme_authenticate(struct wpa_supplicant *wpa_s,
		      struct wpa_bss *bss, struct wpa_ssid *ssid)
{
	struct wpa_connect_work *cwork;

	if (bss == NULL || ssid == NULL)
		return;
	if (wpa_s->connect_work) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Reject sme_authenticate() call since connect_work exist");
		return;
	}

	if (radio_work_pending(wpa_s, "sme-connect")) {
		/*
		 * The previous sme-connect work might no longer be valid due to
		 * the fact that the BSS list was updated. In addition, it makes
		 * sense to adhere to the 'newer' decision.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG,
			"SME: Remove previous pending sme-connect");
		radio_remove_works(wpa_s, "sme-connect", 0);
	}

	cwork = os_zalloc(sizeof(*cwork));
	if (cwork == NULL)
		return;
	cwork->bss = bss;
	cwork->ssid = ssid;
	cwork->sme = 1;

#ifdef CONFIG_SAE
	wpa_s->sme.sae.state = SAE_NOTHING;
	wpa_s->sme.sae.send_confirm = 0;
	wpa_s->sme.sae_group_index = 0;
#endif /* CONFIG_SAE */

	if (radio_add_work(wpa_s, bss->freq, "sme-connect", 1,
			   sme_auth_start_cb, cwork) < 0)
		wpas_connect_work_free(cwork);
}


#ifdef CONFIG_SAE

static int sme_sae_auth(struct wpa_supplicant *wpa_s, u16 auth_transaction,
			u16 status_code, const u8 *data, size_t len)
{
	int *groups;

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: SAE authentication transaction %u "
		"status code %u", auth_transaction, status_code);

	if (auth_transaction == 1 &&
	    status_code == WLAN_STATUS_ANTI_CLOGGING_TOKEN_REQ &&
	    wpa_s->sme.sae.state == SAE_COMMITTED &&
	    wpa_s->current_bss && wpa_s->current_ssid) {
		int default_groups[] = { 19, 20, 21, 25, 26, 0 };
		u16 group;

		groups = wpa_s->conf->sae_groups;
		if (!groups || groups[0] <= 0)
			groups = default_groups;

		if (len < sizeof(le16)) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"SME: Too short SAE anti-clogging token request");
			return -1;
		}
		group = WPA_GET_LE16(data);
		wpa_dbg(wpa_s, MSG_DEBUG,
			"SME: SAE anti-clogging token requested (group %u)",
			group);
		if (sae_group_allowed(&wpa_s->sme.sae, groups, group) !=
		    WLAN_STATUS_SUCCESS) {
			wpa_dbg(wpa_s, MSG_ERROR,
				"SME: SAE group %u of anti-clogging request is invalid",
				group);
			return -1;
		}
		wpabuf_free(wpa_s->sme.sae_token);
		wpa_s->sme.sae_token = wpabuf_alloc_copy(data + sizeof(le16),
							 len - sizeof(le16));
		sme_send_authentication(wpa_s, wpa_s->current_bss,
					wpa_s->current_ssid, 1);
		return 0;
	}

	if (auth_transaction == 1 &&
	    status_code == WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED &&
	    wpa_s->sme.sae.state == SAE_COMMITTED &&
	    wpa_s->current_bss && wpa_s->current_ssid) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: SAE group not supported");
		wpa_s->sme.sae_group_index++;
		if (sme_set_sae_group(wpa_s) < 0)
			return -1; /* no other groups enabled */
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Try next enabled SAE group");
		sme_send_authentication(wpa_s, wpa_s->current_bss,
					wpa_s->current_ssid, 1);
		return 0;
	}

	if (status_code != WLAN_STATUS_SUCCESS)
		return -1;

	if (auth_transaction == 1) {
		u16 res;

		groups = wpa_s->conf->sae_groups;

		wpa_dbg(wpa_s, MSG_DEBUG, "SME SAE commit");
		if (wpa_s->current_bss == NULL ||
		    wpa_s->current_ssid == NULL)
			return -1;
		if (wpa_s->sme.sae.state != SAE_COMMITTED)
			return -1;
		if (groups && groups[0] <= 0)
			groups = NULL;
		res = sae_parse_commit(&wpa_s->sme.sae, data, len, NULL, NULL,
				       groups);
		if (res == SAE_SILENTLY_DISCARD) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Drop commit message due to reflection attack");
			return 0;
		}
		if (res != WLAN_STATUS_SUCCESS)
			return -1;

		if (sae_process_commit(&wpa_s->sme.sae) < 0) {
			wpa_printf(MSG_DEBUG, "SAE: Failed to process peer "
				   "commit");
			return -1;
		}

		wpabuf_free(wpa_s->sme.sae_token);
		wpa_s->sme.sae_token = NULL;
		sme_send_authentication(wpa_s, wpa_s->current_bss,
					wpa_s->current_ssid, 0);
		return 0;
	} else if (auth_transaction == 2) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME SAE confirm");
		if (wpa_s->sme.sae.state != SAE_CONFIRMED)
			return -1;
		if (sae_check_confirm(&wpa_s->sme.sae, data, len) < 0)
			return -1;
		wpa_s->sme.sae.state = SAE_ACCEPTED;
		sae_clear_temp_data(&wpa_s->sme.sae);
		return 1;
	}

	return -1;
}
#endif /* CONFIG_SAE */


void sme_event_auth(struct wpa_supplicant *wpa_s, union wpa_event_data *data)
{
	struct wpa_ssid *ssid = wpa_s->current_ssid;

	if (ssid == NULL) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Ignore authentication event "
			"when network is not selected");
		return;
	}

	if (wpa_s->wpa_state != WPA_AUTHENTICATING) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Ignore authentication event "
			"when not in authenticating state");
		return;
	}

	if (os_memcmp(wpa_s->pending_bssid, data->auth.peer, ETH_ALEN) != 0) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Ignore authentication with "
			"unexpected peer " MACSTR,
			MAC2STR(data->auth.peer));
		return;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Authentication response: peer=" MACSTR
		" auth_type=%d auth_transaction=%d status_code=%d",
		MAC2STR(data->auth.peer), data->auth.auth_type,
		data->auth.auth_transaction, data->auth.status_code);
	wpa_hexdump(MSG_MSGDUMP, "SME: Authentication response IEs",
		    data->auth.ies, data->auth.ies_len);

	eloop_cancel_timeout(sme_auth_timer, wpa_s, NULL);

#ifdef CONFIG_SAE
	if (data->auth.auth_type == WLAN_AUTH_SAE) {
		int res;
		res = sme_sae_auth(wpa_s, data->auth.auth_transaction,
				   data->auth.status_code, data->auth.ies,
				   data->auth.ies_len);
		if (res < 0) {
			wpas_connection_failed(wpa_s, wpa_s->pending_bssid);
			wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);

		}
		if (res != 1)
			return;

		wpa_printf(MSG_DEBUG, "SME: SAE completed - setting PMK for "
			   "4-way handshake");
		wpa_sm_set_pmk(wpa_s->wpa, wpa_s->sme.sae.pmk, PMK_LEN,
			       wpa_s->pending_bssid);
	}
#endif /* CONFIG_SAE */

	if (data->auth.status_code != WLAN_STATUS_SUCCESS) {
		char *ie_txt = NULL;

		if (data->auth.ies && data->auth.ies_len) {
			size_t buflen = 2 * data->auth.ies_len + 1;
			ie_txt = os_malloc(buflen);
			if (ie_txt) {
				wpa_snprintf_hex(ie_txt, buflen, data->auth.ies,
						 data->auth.ies_len);
			}
		}
		wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_AUTH_REJECT MACSTR
			" auth_type=%u auth_transaction=%u status_code=%u ie=%s",
			MAC2STR(data->auth.peer), data->auth.auth_type,
			data->auth.auth_transaction, data->auth.status_code,
			ie_txt);
		os_free(ie_txt);

		if (data->auth.status_code !=
		    WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG ||
		    wpa_s->sme.auth_alg == data->auth.auth_type ||
		    wpa_s->current_ssid->auth_alg == WPA_AUTH_ALG_LEAP) {
			wpas_connection_failed(wpa_s, wpa_s->pending_bssid);
			wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
			return;
		}

		wpas_connect_work_done(wpa_s);

		switch (data->auth.auth_type) {
		case WLAN_AUTH_OPEN:
			wpa_s->current_ssid->auth_alg = WPA_AUTH_ALG_SHARED;

			wpa_dbg(wpa_s, MSG_DEBUG, "SME: Trying SHARED auth");
			wpa_supplicant_associate(wpa_s, wpa_s->current_bss,
						 wpa_s->current_ssid);
			return;

		case WLAN_AUTH_SHARED_KEY:
			wpa_s->current_ssid->auth_alg = WPA_AUTH_ALG_LEAP;

			wpa_dbg(wpa_s, MSG_DEBUG, "SME: Trying LEAP auth");
			wpa_supplicant_associate(wpa_s, wpa_s->current_bss,
						 wpa_s->current_ssid);
			return;

		default:
			return;
		}
	}

#ifdef CONFIG_IEEE80211R
	if (data->auth.auth_type == WLAN_AUTH_FT) {
		if (wpa_ft_process_response(wpa_s->wpa, data->auth.ies,
					    data->auth.ies_len, 0,
					    data->auth.peer, NULL, 0) < 0) {
			wpa_dbg(wpa_s, MSG_DEBUG,
				"SME: FT Authentication response processing failed");
			wpa_msg(wpa_s, MSG_INFO, WPA_EVENT_DISCONNECTED "bssid="
				MACSTR
				" reason=%d locally_generated=1",
				MAC2STR(wpa_s->pending_bssid),
				WLAN_REASON_DEAUTH_LEAVING);
			wpas_connection_failed(wpa_s, wpa_s->pending_bssid);
			wpa_supplicant_mark_disassoc(wpa_s);
			return;
		}
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
#ifdef CONFIG_HT_OVERRIDES
	struct ieee80211_ht_capabilities htcaps;
	struct ieee80211_ht_capabilities htcaps_mask;
#endif /* CONFIG_HT_OVERRIDES */
#ifdef CONFIG_VHT_OVERRIDES
	struct ieee80211_vht_capabilities vhtcaps;
	struct ieee80211_vht_capabilities vhtcaps_mask;
#endif /* CONFIG_VHT_OVERRIDES */

	os_memset(&params, 0, sizeof(params));
	params.bssid = bssid;
	params.ssid = wpa_s->sme.ssid;
	params.ssid_len = wpa_s->sme.ssid_len;
	params.freq.freq = wpa_s->sme.freq;
	params.bg_scan_period = wpa_s->current_ssid ?
		wpa_s->current_ssid->bg_scan_period : -1;
	params.wpa_ie = wpa_s->sme.assoc_req_ie_len ?
		wpa_s->sme.assoc_req_ie : NULL;
	params.wpa_ie_len = wpa_s->sme.assoc_req_ie_len;
	params.pairwise_suite = wpa_s->pairwise_cipher;
	params.group_suite = wpa_s->group_cipher;
	params.key_mgmt_suite = wpa_s->key_mgmt;
	params.wpa_proto = wpa_s->wpa_proto;
#ifdef CONFIG_HT_OVERRIDES
	os_memset(&htcaps, 0, sizeof(htcaps));
	os_memset(&htcaps_mask, 0, sizeof(htcaps_mask));
	params.htcaps = (u8 *) &htcaps;
	params.htcaps_mask = (u8 *) &htcaps_mask;
	wpa_supplicant_apply_ht_overrides(wpa_s, wpa_s->current_ssid, &params);
#endif /* CONFIG_HT_OVERRIDES */
#ifdef CONFIG_VHT_OVERRIDES
	os_memset(&vhtcaps, 0, sizeof(vhtcaps));
	os_memset(&vhtcaps_mask, 0, sizeof(vhtcaps_mask));
	params.vhtcaps = &vhtcaps;
	params.vhtcaps_mask = &vhtcaps_mask;
	wpa_supplicant_apply_vht_overrides(wpa_s, wpa_s->current_ssid, &params);
#endif /* CONFIG_VHT_OVERRIDES */
#ifdef CONFIG_IEEE80211R
	if (auth_type == WLAN_AUTH_FT && wpa_s->sme.ft_ies) {
		params.wpa_ie = wpa_s->sme.ft_ies;
		params.wpa_ie_len = wpa_s->sme.ft_ies_len;
	}
#endif /* CONFIG_IEEE80211R */
	params.mode = mode;
	params.mgmt_frame_protection = wpa_s->sme.mfp;
	params.rrm_used = wpa_s->rrm.rrm_used;
	if (wpa_s->sme.prev_bssid_set)
		params.prev_bssid = wpa_s->sme.prev_bssid;

	wpa_msg(wpa_s, MSG_INFO, "Trying to associate with " MACSTR
		" (SSID='%s' freq=%d MHz)", MAC2STR(params.bssid),
		params.ssid ? wpa_ssid_txt(params.ssid, params.ssid_len) : "",
		params.freq.freq);

	wpa_supplicant_set_state(wpa_s, WPA_ASSOCIATING);

	if (params.wpa_ie == NULL ||
	    ieee802_11_parse_elems(params.wpa_ie, params.wpa_ie_len, &elems, 0)
	    < 0) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Could not parse own IEs?!");
		os_memset(&elems, 0, sizeof(elems));
	}
	if (elems.rsn_ie) {
		params.wpa_proto = WPA_PROTO_RSN;
		wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, elems.rsn_ie - 2,
					elems.rsn_ie_len + 2);
	} else if (elems.wpa_ie) {
		params.wpa_proto = WPA_PROTO_WPA;
		wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, elems.wpa_ie - 2,
					elems.wpa_ie_len + 2);
	} else if (elems.osen) {
		params.wpa_proto = WPA_PROTO_OSEN;
		wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, elems.osen - 2,
					elems.osen_len + 2);
	} else
		wpa_sm_set_assoc_wpa_ie(wpa_s->wpa, NULL, 0);
	if (wpa_s->current_ssid && wpa_s->current_ssid->p2p_group)
		params.p2p = 1;

	if (wpa_s->parent->set_sta_uapsd)
		params.uapsd = wpa_s->parent->sta_uapsd;
	else
		params.uapsd = -1;

	if (wpa_drv_associate(wpa_s, &params) < 0) {
		wpa_msg(wpa_s, MSG_INFO, "SME: Association request to the "
			"driver failed");
		wpas_connection_failed(wpa_s, wpa_s->pending_bssid);
		wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
		os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
		return;
	}

	eloop_register_timeout(SME_ASSOC_TIMEOUT, 0, sme_assoc_timer, wpa_s,
			       NULL);
}


int sme_update_ft_ies(struct wpa_supplicant *wpa_s, const u8 *md,
		      const u8 *ies, size_t ies_len)
{
	if (md == NULL || ies == NULL) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Remove mobility domain");
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


static void sme_deauth(struct wpa_supplicant *wpa_s)
{
	int bssid_changed;

	bssid_changed = !is_zero_ether_addr(wpa_s->bssid);

	if (wpa_drv_deauthenticate(wpa_s, wpa_s->pending_bssid,
				   WLAN_REASON_DEAUTH_LEAVING) < 0) {
		wpa_msg(wpa_s, MSG_INFO, "SME: Deauth request to the driver "
			"failed");
	}
	wpa_s->sme.prev_bssid_set = 0;

	wpas_connection_failed(wpa_s, wpa_s->pending_bssid);
	wpa_supplicant_set_state(wpa_s, WPA_DISCONNECTED);
	os_memset(wpa_s->bssid, 0, ETH_ALEN);
	os_memset(wpa_s->pending_bssid, 0, ETH_ALEN);
	if (bssid_changed)
		wpas_notify_bssid_changed(wpa_s);
}


void sme_event_assoc_reject(struct wpa_supplicant *wpa_s,
			    union wpa_event_data *data)
{
	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Association with " MACSTR " failed: "
		"status code %d", MAC2STR(wpa_s->pending_bssid),
		data->assoc_reject.status_code);

	eloop_cancel_timeout(sme_assoc_timer, wpa_s, NULL);

#ifdef CONFIG_SAE
	if (wpa_s->sme.sae_pmksa_caching && wpa_s->current_ssid &&
	    wpa_key_mgmt_sae(wpa_s->current_ssid->key_mgmt)) {
		wpa_dbg(wpa_s, MSG_DEBUG,
			"PMKSA caching attempt rejected - drop PMKSA cache entry and fall back to SAE authentication");
		wpa_sm_aborted_cached(wpa_s->wpa);
		wpa_sm_pmksa_cache_flush(wpa_s->wpa, wpa_s->current_ssid);
		if (wpa_s->current_bss) {
			struct wpa_bss *bss = wpa_s->current_bss;
			struct wpa_ssid *ssid = wpa_s->current_ssid;

			wpa_drv_deauthenticate(wpa_s, wpa_s->pending_bssid,
					       WLAN_REASON_DEAUTH_LEAVING);
			wpas_connect_work_done(wpa_s);
			wpa_supplicant_mark_disassoc(wpa_s);
			wpa_supplicant_connect(wpa_s, bss, ssid);
			return;
		}
	}
#endif /* CONFIG_SAE */

	/*
	 * For now, unconditionally terminate the previous authentication. In
	 * theory, this should not be needed, but mac80211 gets quite confused
	 * if the authentication is left pending.. Some roaming cases might
	 * benefit from using the previous authentication, so this could be
	 * optimized in the future.
	 */
	sme_deauth(wpa_s);
}


void sme_event_auth_timed_out(struct wpa_supplicant *wpa_s,
			      union wpa_event_data *data)
{
	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Authentication timed out");
	wpas_connection_failed(wpa_s, wpa_s->pending_bssid);
	wpa_supplicant_mark_disassoc(wpa_s);
}


void sme_event_assoc_timed_out(struct wpa_supplicant *wpa_s,
			       union wpa_event_data *data)
{
	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Association timed out");
	wpas_connection_failed(wpa_s, wpa_s->pending_bssid);
	wpa_supplicant_mark_disassoc(wpa_s);
}


void sme_event_disassoc(struct wpa_supplicant *wpa_s,
			struct disassoc_info *info)
{
	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Disassociation event received");
	if (wpa_s->sme.prev_bssid_set) {
		/*
		 * cfg80211/mac80211 can get into somewhat confused state if
		 * the AP only disassociates us and leaves us in authenticated
		 * state. For now, force the state to be cleared to avoid
		 * confusing errors if we try to associate with the AP again.
		 */
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: Deauthenticate to clear "
			"driver state");
		wpa_drv_deauthenticate(wpa_s, wpa_s->sme.prev_bssid,
				       WLAN_REASON_DEAUTH_LEAVING);
	}
}


static void sme_auth_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	if (wpa_s->wpa_state == WPA_AUTHENTICATING) {
		wpa_msg(wpa_s, MSG_DEBUG, "SME: Authentication timeout");
		sme_deauth(wpa_s);
	}
}


static void sme_assoc_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	if (wpa_s->wpa_state == WPA_ASSOCIATING) {
		wpa_msg(wpa_s, MSG_DEBUG, "SME: Association timeout");
		sme_deauth(wpa_s);
	}
}


void sme_state_changed(struct wpa_supplicant *wpa_s)
{
	/* Make sure timers are cleaned up appropriately. */
	if (wpa_s->wpa_state != WPA_ASSOCIATING)
		eloop_cancel_timeout(sme_assoc_timer, wpa_s, NULL);
	if (wpa_s->wpa_state != WPA_AUTHENTICATING)
		eloop_cancel_timeout(sme_auth_timer, wpa_s, NULL);
}


void sme_disassoc_while_authenticating(struct wpa_supplicant *wpa_s,
				       const u8 *prev_pending_bssid)
{
	/*
	 * mac80211-workaround to force deauth on failed auth cmd,
	 * requires us to remain in authenticating state to allow the
	 * second authentication attempt to be continued properly.
	 */
	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Allow pending authentication "
		"to proceed after disconnection event");
	wpa_supplicant_set_state(wpa_s, WPA_AUTHENTICATING);
	os_memcpy(wpa_s->pending_bssid, prev_pending_bssid, ETH_ALEN);

	/*
	 * Re-arm authentication timer in case auth fails for whatever reason.
	 */
	eloop_cancel_timeout(sme_auth_timer, wpa_s, NULL);
	eloop_register_timeout(SME_AUTH_TIMEOUT, 0, sme_auth_timer, wpa_s,
			       NULL);
}


void sme_clear_on_disassoc(struct wpa_supplicant *wpa_s)
{
	wpa_s->sme.prev_bssid_set = 0;
#ifdef CONFIG_SAE
	wpabuf_free(wpa_s->sme.sae_token);
	wpa_s->sme.sae_token = NULL;
	sae_clear_data(&wpa_s->sme.sae);
#endif /* CONFIG_SAE */
#ifdef CONFIG_IEEE80211R
	if (wpa_s->sme.ft_ies)
		sme_update_ft_ies(wpa_s, NULL, NULL, 0);
#endif /* CONFIG_IEEE80211R */
}


void sme_deinit(struct wpa_supplicant *wpa_s)
{
	os_free(wpa_s->sme.ft_ies);
	wpa_s->sme.ft_ies = NULL;
	wpa_s->sme.ft_ies_len = 0;
#ifdef CONFIG_IEEE80211W
	sme_stop_sa_query(wpa_s);
#endif /* CONFIG_IEEE80211W */
	sme_clear_on_disassoc(wpa_s);

	eloop_cancel_timeout(sme_assoc_timer, wpa_s, NULL);
	eloop_cancel_timeout(sme_auth_timer, wpa_s, NULL);
	eloop_cancel_timeout(sme_obss_scan_timeout, wpa_s, NULL);
}


static void sme_send_2040_bss_coex(struct wpa_supplicant *wpa_s,
				   const u8 *chan_list, u8 num_channels,
				   u8 num_intol)
{
	struct ieee80211_2040_bss_coex_ie *bc_ie;
	struct ieee80211_2040_intol_chan_report *ic_report;
	struct wpabuf *buf;

	wpa_printf(MSG_DEBUG, "SME: Send 20/40 BSS Coexistence to " MACSTR
		   " (num_channels=%u num_intol=%u)",
		   MAC2STR(wpa_s->bssid), num_channels, num_intol);
	wpa_hexdump(MSG_DEBUG, "SME: 20/40 BSS Intolerant Channels",
		    chan_list, num_channels);

	buf = wpabuf_alloc(2 + /* action.category + action_code */
			   sizeof(struct ieee80211_2040_bss_coex_ie) +
			   sizeof(struct ieee80211_2040_intol_chan_report) +
			   num_channels);
	if (buf == NULL)
		return;

	wpabuf_put_u8(buf, WLAN_ACTION_PUBLIC);
	wpabuf_put_u8(buf, WLAN_PA_20_40_BSS_COEX);

	bc_ie = wpabuf_put(buf, sizeof(*bc_ie));
	bc_ie->element_id = WLAN_EID_20_40_BSS_COEXISTENCE;
	bc_ie->length = 1;
	if (num_intol)
		bc_ie->coex_param |= WLAN_20_40_BSS_COEX_20MHZ_WIDTH_REQ;

	if (num_channels > 0) {
		ic_report = wpabuf_put(buf, sizeof(*ic_report));
		ic_report->element_id = WLAN_EID_20_40_BSS_INTOLERANT;
		ic_report->length = num_channels + 1;
		ic_report->op_class = 0;
		os_memcpy(wpabuf_put(buf, num_channels), chan_list,
			  num_channels);
	}

	if (wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				wpa_s->own_addr, wpa_s->bssid,
				wpabuf_head(buf), wpabuf_len(buf), 0) < 0) {
		wpa_msg(wpa_s, MSG_INFO,
			"SME: Failed to send 20/40 BSS Coexistence frame");
	}

	wpabuf_free(buf);
}


int sme_proc_obss_scan(struct wpa_supplicant *wpa_s)
{
	struct wpa_bss *bss;
	const u8 *ie;
	u16 ht_cap;
	u8 chan_list[P2P_MAX_CHANNELS], channel;
	u8 num_channels = 0, num_intol = 0, i;

	if (!wpa_s->sme.sched_obss_scan)
		return 0;

	wpa_s->sme.sched_obss_scan = 0;
	if (!wpa_s->current_bss || wpa_s->wpa_state != WPA_COMPLETED)
		return 1;

	/*
	 * Check whether AP uses regulatory triplet or channel triplet in
	 * country info. Right now the operating class of the BSS channel
	 * width trigger event is "unknown" (IEEE Std 802.11-2012 10.15.12),
	 * based on the assumption that operating class triplet is not used in
	 * beacon frame. If the First Channel Number/Operating Extension
	 * Identifier octet has a positive integer value of 201 or greater,
	 * then its operating class triplet.
	 *
	 * TODO: If Supported Operating Classes element is present in beacon
	 * frame, have to lookup operating class in Annex E and fill them in
	 * 2040 coex frame.
	 */
	ie = wpa_bss_get_ie(wpa_s->current_bss, WLAN_EID_COUNTRY);
	if (ie && (ie[1] >= 6) && (ie[5] >= 201))
		return 1;

	os_memset(chan_list, 0, sizeof(chan_list));

	dl_list_for_each(bss, &wpa_s->bss, struct wpa_bss, list) {
		/* Skip other band bss */
		enum hostapd_hw_mode mode;
		mode = ieee80211_freq_to_chan(bss->freq, &channel);
		if (mode != HOSTAPD_MODE_IEEE80211G &&
		    mode != HOSTAPD_MODE_IEEE80211B)
			continue;

		ie = wpa_bss_get_ie(bss, WLAN_EID_HT_CAP);
		ht_cap = (ie && (ie[1] == 26)) ? WPA_GET_LE16(ie + 2) : 0;
		wpa_printf(MSG_DEBUG, "SME OBSS scan BSS " MACSTR
			   " freq=%u chan=%u ht_cap=0x%x",
			   MAC2STR(bss->bssid), bss->freq, channel, ht_cap);

		if (!ht_cap || (ht_cap & HT_CAP_INFO_40MHZ_INTOLERANT)) {
			if (ht_cap & HT_CAP_INFO_40MHZ_INTOLERANT)
				num_intol++;

			/* Check whether the channel is already considered */
			for (i = 0; i < num_channels; i++) {
				if (channel == chan_list[i])
					break;
			}
			if (i != num_channels)
				continue;

			chan_list[num_channels++] = channel;
		}
	}

	sme_send_2040_bss_coex(wpa_s, chan_list, num_channels, num_intol);
	return 1;
}


static struct hostapd_hw_modes * get_mode(struct hostapd_hw_modes *modes,
					  u16 num_modes,
					  enum hostapd_hw_mode mode)
{
	u16 i;

	for (i = 0; i < num_modes; i++) {
		if (modes[i].mode == mode)
			return &modes[i];
	}

	return NULL;
}


static void wpa_obss_scan_freqs_list(struct wpa_supplicant *wpa_s,
				     struct wpa_driver_scan_params *params)
{
	/* Include only affected channels */
	struct hostapd_hw_modes *mode;
	int count, i;
	int start, end;

	mode = get_mode(wpa_s->hw.modes, wpa_s->hw.num_modes,
			HOSTAPD_MODE_IEEE80211G);
	if (mode == NULL) {
		/* No channels supported in this band - use empty list */
		params->freqs = os_zalloc(sizeof(int));
		return;
	}

	if (wpa_s->sme.ht_sec_chan == HT_SEC_CHAN_UNKNOWN &&
	    wpa_s->current_bss) {
		const u8 *ie;

		ie = wpa_bss_get_ie(wpa_s->current_bss, WLAN_EID_HT_OPERATION);
		if (ie && ie[1] >= 2) {
			u8 o;

			o = ie[3] & HT_INFO_HT_PARAM_SECONDARY_CHNL_OFF_MASK;
			if (o == HT_INFO_HT_PARAM_SECONDARY_CHNL_ABOVE)
				wpa_s->sme.ht_sec_chan = HT_SEC_CHAN_ABOVE;
			else if (o == HT_INFO_HT_PARAM_SECONDARY_CHNL_BELOW)
				wpa_s->sme.ht_sec_chan = HT_SEC_CHAN_BELOW;
		}
	}

	start = wpa_s->assoc_freq - 10;
	end = wpa_s->assoc_freq + 10;
	switch (wpa_s->sme.ht_sec_chan) {
	case HT_SEC_CHAN_UNKNOWN:
		/* HT40+ possible on channels 1..9 */
		if (wpa_s->assoc_freq <= 2452)
			start -= 20;
		/* HT40- possible on channels 5-13 */
		if (wpa_s->assoc_freq >= 2432)
			end += 20;
		break;
	case HT_SEC_CHAN_ABOVE:
		end += 20;
		break;
	case HT_SEC_CHAN_BELOW:
		start -= 20;
		break;
	}
	wpa_printf(MSG_DEBUG,
		   "OBSS: assoc_freq %d possible affected range %d-%d",
		   wpa_s->assoc_freq, start, end);

	params->freqs = os_calloc(mode->num_channels + 1, sizeof(int));
	if (params->freqs == NULL)
		return;
	for (count = 0, i = 0; i < mode->num_channels; i++) {
		int freq;

		if (mode->channels[i].flag & HOSTAPD_CHAN_DISABLED)
			continue;
		freq = mode->channels[i].freq;
		if (freq - 10 >= end || freq + 10 <= start)
			continue; /* not affected */
		params->freqs[count++] = freq;
	}
}


static void sme_obss_scan_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct wpa_driver_scan_params params;

	if (!wpa_s->current_bss) {
		wpa_printf(MSG_DEBUG, "SME OBSS: Ignore scan request");
		return;
	}

	os_memset(&params, 0, sizeof(params));
	wpa_obss_scan_freqs_list(wpa_s, &params);
	params.low_priority = 1;
	wpa_printf(MSG_DEBUG, "SME OBSS: Request an OBSS scan");

	if (wpa_supplicant_trigger_scan(wpa_s, &params))
		wpa_printf(MSG_DEBUG, "SME OBSS: Failed to trigger scan");
	else
		wpa_s->sme.sched_obss_scan = 1;
	os_free(params.freqs);

	eloop_register_timeout(wpa_s->sme.obss_scan_int, 0,
			       sme_obss_scan_timeout, wpa_s, NULL);
}


void sme_sched_obss_scan(struct wpa_supplicant *wpa_s, int enable)
{
	const u8 *ie;
	struct wpa_bss *bss = wpa_s->current_bss;
	struct wpa_ssid *ssid = wpa_s->current_ssid;
	struct hostapd_hw_modes *hw_mode = NULL;
	int i;

	eloop_cancel_timeout(sme_obss_scan_timeout, wpa_s, NULL);
	wpa_s->sme.sched_obss_scan = 0;
	wpa_s->sme.ht_sec_chan = HT_SEC_CHAN_UNKNOWN;
	if (!enable)
		return;

	/*
	 * Schedule OBSS scan if driver is using station SME in wpa_supplicant
	 * or it expects OBSS scan to be performed by wpa_supplicant.
	 */
	if (!((wpa_s->drv_flags & WPA_DRIVER_FLAGS_SME) ||
	      (wpa_s->drv_flags & WPA_DRIVER_FLAGS_OBSS_SCAN)) ||
	    ssid == NULL || ssid->mode != IEEE80211_MODE_INFRA)
		return;

	if (!wpa_s->hw.modes)
		return;

	/* only HT caps in 11g mode are relevant */
	for (i = 0; i < wpa_s->hw.num_modes; i++) {
		hw_mode = &wpa_s->hw.modes[i];
		if (hw_mode->mode == HOSTAPD_MODE_IEEE80211G)
			break;
	}

	/* Driver does not support HT40 for 11g or doesn't have 11g. */
	if (i == wpa_s->hw.num_modes || !hw_mode ||
	    !(hw_mode->ht_capab & HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET))
		return;

	if (bss == NULL || bss->freq < 2400 || bss->freq > 2500)
		return; /* Not associated on 2.4 GHz band */

	/* Check whether AP supports HT40 */
	ie = wpa_bss_get_ie(wpa_s->current_bss, WLAN_EID_HT_CAP);
	if (!ie || ie[1] < 2 ||
	    !(WPA_GET_LE16(ie + 2) & HT_CAP_INFO_SUPP_CHANNEL_WIDTH_SET))
		return; /* AP does not support HT40 */

	ie = wpa_bss_get_ie(wpa_s->current_bss,
			    WLAN_EID_OVERLAPPING_BSS_SCAN_PARAMS);
	if (!ie || ie[1] < 14)
		return; /* AP does not request OBSS scans */

	wpa_s->sme.obss_scan_int = WPA_GET_LE16(ie + 6);
	if (wpa_s->sme.obss_scan_int < 10) {
		wpa_printf(MSG_DEBUG, "SME: Invalid OBSS Scan Interval %u "
			   "replaced with the minimum 10 sec",
			   wpa_s->sme.obss_scan_int);
		wpa_s->sme.obss_scan_int = 10;
	}
	wpa_printf(MSG_DEBUG, "SME: OBSS Scan Interval %u sec",
		   wpa_s->sme.obss_scan_int);
	eloop_register_timeout(wpa_s->sme.obss_scan_int, 0,
			       sme_obss_scan_timeout, wpa_s, NULL);
}


#ifdef CONFIG_IEEE80211W

static const unsigned int sa_query_max_timeout = 1000;
static const unsigned int sa_query_retry_timeout = 201;

static int sme_check_sa_query_timeout(struct wpa_supplicant *wpa_s)
{
	u32 tu;
	struct os_reltime now, passed;
	os_get_reltime(&now);
	os_reltime_sub(&now, &wpa_s->sme.sa_query_start, &passed);
	tu = (passed.sec * 1000000 + passed.usec) / 1024;
	if (sa_query_max_timeout < tu) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: SA Query timed out");
		sme_stop_sa_query(wpa_s);
		wpa_supplicant_deauthenticate(
			wpa_s, WLAN_REASON_PREV_AUTH_NOT_VALID);
		return 1;
	}

	return 0;
}


static void sme_send_sa_query_req(struct wpa_supplicant *wpa_s,
				  const u8 *trans_id)
{
	u8 req[2 + WLAN_SA_QUERY_TR_ID_LEN];
	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Sending SA Query Request to "
		MACSTR, MAC2STR(wpa_s->bssid));
	wpa_hexdump(MSG_DEBUG, "SME: SA Query Transaction ID",
		    trans_id, WLAN_SA_QUERY_TR_ID_LEN);
	req[0] = WLAN_ACTION_SA_QUERY;
	req[1] = WLAN_SA_QUERY_REQUEST;
	os_memcpy(req + 2, trans_id, WLAN_SA_QUERY_TR_ID_LEN);
	if (wpa_drv_send_action(wpa_s, wpa_s->assoc_freq, 0, wpa_s->bssid,
				wpa_s->own_addr, wpa_s->bssid,
				req, sizeof(req), 0) < 0)
		wpa_msg(wpa_s, MSG_INFO, "SME: Failed to send SA Query "
			"Request");
}


static void sme_sa_query_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	unsigned int timeout, sec, usec;
	u8 *trans_id, *nbuf;

	if (wpa_s->sme.sa_query_count > 0 &&
	    sme_check_sa_query_timeout(wpa_s))
		return;

	nbuf = os_realloc_array(wpa_s->sme.sa_query_trans_id,
				wpa_s->sme.sa_query_count + 1,
				WLAN_SA_QUERY_TR_ID_LEN);
	if (nbuf == NULL)
		return;
	if (wpa_s->sme.sa_query_count == 0) {
		/* Starting a new SA Query procedure */
		os_get_reltime(&wpa_s->sme.sa_query_start);
	}
	trans_id = nbuf + wpa_s->sme.sa_query_count * WLAN_SA_QUERY_TR_ID_LEN;
	wpa_s->sme.sa_query_trans_id = nbuf;
	wpa_s->sme.sa_query_count++;

	if (os_get_random(trans_id, WLAN_SA_QUERY_TR_ID_LEN) < 0) {
		wpa_printf(MSG_DEBUG, "Could not generate SA Query ID");
		return;
	}

	timeout = sa_query_retry_timeout;
	sec = ((timeout / 1000) * 1024) / 1000;
	usec = (timeout % 1000) * 1024;
	eloop_register_timeout(sec, usec, sme_sa_query_timer, wpa_s, NULL);

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Association SA Query attempt %d",
		wpa_s->sme.sa_query_count);

	sme_send_sa_query_req(wpa_s, trans_id);
}


static void sme_start_sa_query(struct wpa_supplicant *wpa_s)
{
	sme_sa_query_timer(wpa_s, NULL);
}


static void sme_stop_sa_query(struct wpa_supplicant *wpa_s)
{
	eloop_cancel_timeout(sme_sa_query_timer, wpa_s, NULL);
	os_free(wpa_s->sme.sa_query_trans_id);
	wpa_s->sme.sa_query_trans_id = NULL;
	wpa_s->sme.sa_query_count = 0;
}


void sme_event_unprot_disconnect(struct wpa_supplicant *wpa_s, const u8 *sa,
				 const u8 *da, u16 reason_code)
{
	struct wpa_ssid *ssid;
	struct os_reltime now;

	if (wpa_s->wpa_state != WPA_COMPLETED)
		return;
	ssid = wpa_s->current_ssid;
	if (wpas_get_ssid_pmf(wpa_s, ssid) == NO_MGMT_FRAME_PROTECTION)
		return;
	if (os_memcmp(sa, wpa_s->bssid, ETH_ALEN) != 0)
		return;
	if (reason_code != WLAN_REASON_CLASS2_FRAME_FROM_NONAUTH_STA &&
	    reason_code != WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA)
		return;
	if (wpa_s->sme.sa_query_count > 0)
		return;

	os_get_reltime(&now);
	if (wpa_s->sme.last_unprot_disconnect.sec &&
	    !os_reltime_expired(&now, &wpa_s->sme.last_unprot_disconnect, 10))
		return; /* limit SA Query procedure frequency */
	wpa_s->sme.last_unprot_disconnect = now;

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Unprotected disconnect dropped - "
		"possible AP/STA state mismatch - trigger SA Query");
	sme_start_sa_query(wpa_s);
}


void sme_sa_query_rx(struct wpa_supplicant *wpa_s, const u8 *sa,
		     const u8 *data, size_t len)
{
	int i;

	if (wpa_s->sme.sa_query_trans_id == NULL ||
	    len < 1 + WLAN_SA_QUERY_TR_ID_LEN ||
	    data[0] != WLAN_SA_QUERY_RESPONSE)
		return;
	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Received SA Query response from "
		MACSTR " (trans_id %02x%02x)", MAC2STR(sa), data[1], data[2]);

	if (os_memcmp(sa, wpa_s->bssid, ETH_ALEN) != 0)
		return;

	for (i = 0; i < wpa_s->sme.sa_query_count; i++) {
		if (os_memcmp(wpa_s->sme.sa_query_trans_id +
			      i * WLAN_SA_QUERY_TR_ID_LEN,
			      data + 1, WLAN_SA_QUERY_TR_ID_LEN) == 0)
			break;
	}

	if (i >= wpa_s->sme.sa_query_count) {
		wpa_dbg(wpa_s, MSG_DEBUG, "SME: No matching SA Query "
			"transaction identifier found");
		return;
	}

	wpa_dbg(wpa_s, MSG_DEBUG, "SME: Reply to pending SA Query received "
		"from " MACSTR, MAC2STR(sa));
	sme_stop_sa_query(wpa_s);
}

#endif /* CONFIG_IEEE80211W */
