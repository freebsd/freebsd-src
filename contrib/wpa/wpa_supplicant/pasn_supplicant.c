/*
 * wpa_supplicant - PASN processing
 *
 * Copyright (C) 2019 Intel Corporation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/dragonfly.h"
#include "common/ptksa_cache.h"
#include "utils/eloop.h"
#include "drivers/driver.h"
#include "crypto/crypto.h"
#include "crypto/random.h"
#include "eap_common/eap_defs.h"
#include "rsn_supp/wpa.h"
#include "rsn_supp/pmksa_cache.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "bss.h"
#include "scan.h"
#include "config.h"

static const int dot11RSNAConfigPMKLifetime = 43200;

struct wpa_pasn_auth_work {
	u8 own_addr[ETH_ALEN];
	u8 peer_addr[ETH_ALEN];
	int akmp;
	int cipher;
	u16 group;
	int network_id;
	struct wpabuf *comeback;
};


static int wpas_pasn_send_mlme(void *ctx, const u8 *data, size_t data_len,
			       int noack, unsigned int freq, unsigned int wait)
{
	struct wpa_supplicant *wpa_s = ctx;

	return wpa_drv_send_mlme(wpa_s, data, data_len, noack, freq, wait);
}


static void wpas_pasn_free_auth_work(struct wpa_pasn_auth_work *awork)
{
	wpabuf_free(awork->comeback);
	awork->comeback = NULL;
	os_free(awork);
}


static void wpas_pasn_auth_work_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;

	wpa_printf(MSG_DEBUG, "PASN: Auth work timeout - stopping auth");

	wpas_pasn_auth_stop(wpa_s);

	wpas_pasn_auth_work_done(wpa_s, PASN_STATUS_FAILURE);
}


static void wpas_pasn_cancel_auth_work(struct wpa_supplicant *wpa_s)
{
	wpa_printf(MSG_DEBUG, "PASN: Cancel pasn-start-auth work");

	/* Remove pending/started work */
	radio_remove_works(wpa_s, "pasn-start-auth", 0);
}


static void wpas_pasn_auth_status(struct wpa_supplicant *wpa_s,
				  const u8 *peer_addr,
				  int akmp, int cipher, u8 status,
				  struct wpabuf *comeback,
				  u16 comeback_after)
{
	if (comeback) {
		size_t comeback_len = wpabuf_len(comeback);
		size_t buflen = comeback_len * 2 + 1;
		char *comeback_txt = os_malloc(buflen);

		if (comeback_txt) {
			wpa_snprintf_hex(comeback_txt, buflen,
					 wpabuf_head(comeback), comeback_len);

			wpa_msg(wpa_s, MSG_INFO, PASN_AUTH_STATUS MACSTR
				" akmp=%s, status=%u comeback_after=%u comeback=%s",
				MAC2STR(peer_addr),
				wpa_key_mgmt_txt(akmp, WPA_PROTO_RSN),
				status, comeback_after, comeback_txt);

			os_free(comeback_txt);
			return;
		}
	}

	wpa_msg(wpa_s, MSG_INFO,
		PASN_AUTH_STATUS MACSTR " akmp=%s, status=%u",
		MAC2STR(peer_addr), wpa_key_mgmt_txt(akmp, WPA_PROTO_RSN),
		status);
}


#ifdef CONFIG_SAE

static struct sae_pt *
wpas_pasn_sae_derive_pt(struct wpa_ssid *ssid, int group)
{
	const char *password = ssid->sae_password;
	int groups[2] = { group, 0 };

	if (!password)
		password = ssid->passphrase;

	if (!password) {
		wpa_printf(MSG_DEBUG, "PASN: SAE without a password");
		return NULL;
	}

	return sae_derive_pt(groups, ssid->ssid, ssid->ssid_len,
			    (const u8 *) password, os_strlen(password),
			    ssid->sae_password_id);
}


static int wpas_pasn_sae_setup_pt(struct wpa_ssid *ssid, int group)
{
	if (!ssid->sae_password && !ssid->passphrase) {
		wpa_printf(MSG_DEBUG, "PASN: SAE without a password");
		return -1;
	}

	if (ssid->pt)
		return 0; /* PT already derived */

	ssid->pt = wpas_pasn_sae_derive_pt(ssid, group);

	return ssid->pt ? 0 : -1;
}

#endif /* CONFIG_SAE */


static int wpas_pasn_get_params_from_bss(struct wpa_supplicant *wpa_s,
					 struct pasn_peer *peer)
{
	int ret;
	const u8 *rsne, *rsnxe;
	struct wpa_bss *bss;
	struct wpa_ie_data rsne_data;
	int sel, key_mgmt, pairwise_cipher;
	int network_id = 0, group = 19;
	struct wpa_ssid *ssid = NULL;
	size_t ssid_str_len = 0;
	const u8 *ssid_str = NULL;
	const u8 *peer_addr = peer->peer_addr;

	bss = wpa_bss_get_bssid(wpa_s, peer_addr);
	if (!bss) {
		wpa_supplicant_update_scan_results(wpa_s, peer_addr);
		bss = wpa_bss_get_bssid(wpa_s, peer_addr);
		if (!bss) {
			wpa_printf(MSG_DEBUG, "PASN: BSS not found");
			return -1;
		}
	}

	rsne = wpa_bss_get_ie(bss, WLAN_EID_RSN);
	if (!rsne) {
		wpa_printf(MSG_DEBUG, "PASN: BSS without RSNE");
		return -1;
	}

	ret = wpa_parse_wpa_ie(rsne, *(rsne + 1) + 2, &rsne_data);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed parsing RSNE data");
		return -1;
	}

	rsnxe = wpa_bss_get_ie(bss, WLAN_EID_RSNX);

	ssid_str_len = bss->ssid_len;
	ssid_str = bss->ssid;

	/* Get the network configuration based on the obtained SSID */
	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (!wpas_network_disabled(wpa_s, ssid) &&
		    ssid_str_len == ssid->ssid_len &&
		    os_memcmp(ssid_str, ssid->ssid, ssid_str_len) == 0)
			break;
	}

	if (ssid)
		network_id = ssid->id;

	sel = rsne_data.pairwise_cipher;
	if (ssid && ssid->pairwise_cipher)
		sel &= ssid->pairwise_cipher;

	wpa_printf(MSG_DEBUG, "PASN: peer pairwise 0x%x, select 0x%x",
		   rsne_data.pairwise_cipher, sel);

	pairwise_cipher = wpa_pick_pairwise_cipher(sel, 1);
	if (pairwise_cipher < 0) {
		wpa_msg(wpa_s, MSG_WARNING,
			"PASN: Failed to select pairwise cipher");
		return -1;
	}

	sel = rsne_data.key_mgmt;
	if (ssid && ssid->key_mgmt)
		sel &= ssid->key_mgmt;

	wpa_printf(MSG_DEBUG, "PASN: peer AKMP 0x%x, select 0x%x",
		   rsne_data.key_mgmt, sel);
#ifdef CONFIG_SAE
	if (!(wpa_s->drv_flags & WPA_DRIVER_FLAGS_SAE) || !ssid)
		sel &= ~(WPA_KEY_MGMT_SAE | WPA_KEY_MGMT_SAE_EXT_KEY |
			 WPA_KEY_MGMT_FT_SAE | WPA_KEY_MGMT_FT_SAE_EXT_KEY);
#endif /* CONFIG_SAE */
#ifdef CONFIG_IEEE80211R
	if (!(wpa_s->drv_flags & (WPA_DRIVER_FLAGS_SME |
				  WPA_DRIVER_FLAGS_UPDATE_FT_IES)))
		sel &= ~WPA_KEY_MGMT_FT;
#endif /* CONFIG_IEEE80211R */
	if (0) {
#ifdef CONFIG_IEEE80211R
#ifdef CONFIG_SHA384
	} else if ((sel & WPA_KEY_MGMT_FT_IEEE8021X_SHA384) &&
		   os_strcmp(wpa_supplicant_get_eap_mode(wpa_s), "LEAP") != 0) {
		key_mgmt = WPA_KEY_MGMT_FT_IEEE8021X_SHA384;
		wpa_printf(MSG_DEBUG, "PASN: using KEY_MGMT FT/802.1X-SHA384");
		if (ssid && !ssid->ft_eap_pmksa_caching &&
		    pmksa_cache_get_current(wpa_s->wpa)) {
			/* PMKSA caching with FT may have interoperability
			 * issues, so disable that case by default for now.
			 */
			wpa_printf(MSG_DEBUG,
				   "PASN: Disable PMKSA caching for FT/802.1X connection");
			pmksa_cache_clear_current(wpa_s->wpa);
		}
#endif /* CONFIG_SHA384 */
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_SAE
	} else if ((sel & WPA_KEY_MGMT_SAE_EXT_KEY) &&
		   (ieee802_11_rsnx_capab(rsnxe,
					   WLAN_RSNX_CAPAB_SAE_H2E)) &&
		   (wpas_pasn_sae_setup_pt(ssid, group) == 0)) {
		key_mgmt = WPA_KEY_MGMT_SAE_EXT_KEY;
		wpa_printf(MSG_DEBUG, "PASN: using KEY_MGMT SAE (ext key)");
	} else if ((sel & WPA_KEY_MGMT_SAE) &&
		   (ieee802_11_rsnx_capab(rsnxe,
					   WLAN_RSNX_CAPAB_SAE_H2E)) &&
		   (wpas_pasn_sae_setup_pt(ssid, group) == 0)) {
		key_mgmt = WPA_KEY_MGMT_SAE;
		wpa_printf(MSG_DEBUG, "PASN: using KEY_MGMT SAE");
#endif /* CONFIG_SAE */
#ifdef CONFIG_FILS
	} else if (sel & WPA_KEY_MGMT_FILS_SHA384) {
		key_mgmt = WPA_KEY_MGMT_FILS_SHA384;
		wpa_printf(MSG_DEBUG, "PASN: using KEY_MGMT FILS-SHA384");
	} else if (sel & WPA_KEY_MGMT_FILS_SHA256) {
		key_mgmt = WPA_KEY_MGMT_FILS_SHA256;
		wpa_printf(MSG_DEBUG, "PASN: using KEY_MGMT FILS-SHA256");
#endif /* CONFIG_FILS */
#ifdef CONFIG_IEEE80211R
	} else if ((sel & WPA_KEY_MGMT_FT_IEEE8021X) &&
		   os_strcmp(wpa_supplicant_get_eap_mode(wpa_s), "LEAP") != 0) {
		key_mgmt = WPA_KEY_MGMT_FT_IEEE8021X;
		wpa_printf(MSG_DEBUG, "PASN: using KEY_MGMT FT/802.1X");
		if (ssid && !ssid->ft_eap_pmksa_caching &&
		    pmksa_cache_get_current(wpa_s->wpa)) {
			/* PMKSA caching with FT may have interoperability
			 * issues, so disable that case by default for now.
			 */
			wpa_printf(MSG_DEBUG,
				   "PASN: Disable PMKSA caching for FT/802.1X connection");
			pmksa_cache_clear_current(wpa_s->wpa);
		}
	} else if (sel & WPA_KEY_MGMT_FT_PSK) {
		key_mgmt = WPA_KEY_MGMT_FT_PSK;
		wpa_printf(MSG_DEBUG, "PASN: using KEY_MGMT FT/PSK");
#endif /* CONFIG_IEEE80211R */
	} else if (sel & WPA_KEY_MGMT_PASN) {
		key_mgmt = WPA_KEY_MGMT_PASN;
		wpa_printf(MSG_DEBUG, "PASN: using KEY_MGMT PASN");
	} else {
		wpa_printf(MSG_DEBUG, "PASN: invalid AKMP");
		return -1;
	}

	peer->akmp = key_mgmt;
	peer->cipher = pairwise_cipher;
	peer->network_id = network_id;
	peer->group = group;
	return 0;
}


static int wpas_pasn_set_keys_from_cache(struct wpa_supplicant *wpa_s,
					 const u8 *own_addr,
					 const u8 *peer_addr,
					 int cipher, int akmp)
{
	struct ptksa_cache_entry *entry;

	entry = ptksa_cache_get(wpa_s->ptksa, peer_addr, cipher);
	if (!entry) {
		wpa_printf(MSG_DEBUG, "PASN: peer " MACSTR
			   " not present in PTKSA cache", MAC2STR(peer_addr));
		return -1;
	}

	if (!ether_addr_equal(entry->own_addr, own_addr)) {
		wpa_printf(MSG_DEBUG,
			   "PASN: own addr " MACSTR " and PTKSA entry own addr "
			   MACSTR " differ",
			   MAC2STR(own_addr), MAC2STR(entry->own_addr));
		return -1;
	}

	wpa_printf(MSG_DEBUG, "PASN: " MACSTR " present in PTKSA cache",
		   MAC2STR(peer_addr));
	wpa_drv_set_secure_ranging_ctx(wpa_s, own_addr, peer_addr, cipher,
				       entry->ptk.tk_len,
				       entry->ptk.tk,
				       entry->ptk.ltf_keyseed_len,
				       entry->ptk.ltf_keyseed, 0);
	return 0;
}


static void wpas_pasn_configure_next_peer(struct wpa_supplicant *wpa_s,
					  struct pasn_auth *pasn_params)
{
	struct pasn_peer *peer;
	u8 comeback_len = 0;
	const u8 *comeback = NULL;

	if (!pasn_params)
		return;

	while (wpa_s->pasn_count < pasn_params->num_peers) {
		peer = &pasn_params->peer[wpa_s->pasn_count];

		if (ether_addr_equal(wpa_s->bssid, peer->peer_addr)) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Associated peer is not expected");
			peer->status = PASN_STATUS_FAILURE;
			wpa_s->pasn_count++;
			continue;
		}

		if (wpas_pasn_set_keys_from_cache(wpa_s, peer->own_addr,
						  peer->peer_addr,
						  peer->cipher,
						  peer->akmp) == 0) {
			peer->status = PASN_STATUS_SUCCESS;
			wpa_s->pasn_count++;
			continue;
		}

		if (wpas_pasn_get_params_from_bss(wpa_s, peer)) {
			peer->status = PASN_STATUS_FAILURE;
			wpa_s->pasn_count++;
			continue;
		}

		if (wpas_pasn_auth_start(wpa_s, peer->own_addr,
					 peer->peer_addr, peer->akmp,
					 peer->cipher, peer->group,
					 peer->network_id,
					 comeback, comeback_len)) {
			peer->status = PASN_STATUS_FAILURE;
			wpa_s->pasn_count++;
			continue;
		}
		wpa_printf(MSG_DEBUG, "PASN: Sent PASN auth start for " MACSTR,
			   MAC2STR(peer->peer_addr));
		return;
	}

	if (wpa_s->pasn_count == pasn_params->num_peers) {
		wpa_drv_send_pasn_resp(wpa_s, pasn_params);
		wpa_printf(MSG_DEBUG, "PASN: Response sent");
		os_free(wpa_s->pasn_params);
		wpa_s->pasn_params = NULL;
	}
}


void wpas_pasn_auth_work_done(struct wpa_supplicant *wpa_s, int status)
{
	if (!wpa_s->pasn_params)
		return;

	wpa_s->pasn_params->peer[wpa_s->pasn_count].status = status;
	wpa_s->pasn_count++;
	wpas_pasn_configure_next_peer(wpa_s, wpa_s->pasn_params);
}


static void wpas_pasn_delete_peers(struct wpa_supplicant *wpa_s,
				   struct pasn_auth *pasn_params)
{
	struct pasn_peer *peer;
	unsigned int i;

	if (!pasn_params)
		return;

	for (i = 0; i < pasn_params->num_peers; i++) {
		peer = &pasn_params->peer[i];
		ptksa_cache_flush(wpa_s->ptksa, peer->peer_addr,
				  WPA_CIPHER_NONE);
	}
}


#ifdef CONFIG_FILS
static void wpas_pasn_initiate_eapol(struct pasn_data *pasn,
				     struct wpa_ssid *ssid)
{
	struct eapol_config eapol_conf;

	wpa_printf(MSG_DEBUG, "PASN: FILS: Initiating EAPOL");

	eapol_sm_notify_eap_success(pasn->eapol, false);
	eapol_sm_notify_eap_fail(pasn->eapol, false);
	eapol_sm_notify_portControl(pasn->eapol, Auto);

	os_memset(&eapol_conf, 0, sizeof(eapol_conf));
	eapol_conf.fast_reauth = pasn->fast_reauth;
	eapol_conf.workaround = ssid->eap_workaround;

	eapol_sm_notify_config(pasn->eapol, &ssid->eap, &eapol_conf);
}
#endif /* CONFIG_FILS */


static void wpas_pasn_reset(struct wpa_supplicant *wpa_s)
{
	struct pasn_data *pasn = &wpa_s->pasn;

	wpas_pasn_cancel_auth_work(wpa_s);
	wpa_s->pasn_auth_work = NULL;
	eloop_cancel_timeout(wpas_pasn_auth_work_timeout, wpa_s, NULL);

	wpa_pasn_reset(pasn);
}


static struct wpa_bss * wpas_pasn_allowed(struct wpa_supplicant *wpa_s,
					  const u8 *peer_addr, int akmp,
					  int cipher)
{
	struct wpa_bss *bss;
	const u8 *rsne;
	struct wpa_ie_data rsne_data;
	int ret;

	if (ether_addr_equal(wpa_s->bssid, peer_addr)) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Not doing authentication with current BSS");
		return NULL;
	}

	bss = wpa_bss_get_bssid_latest(wpa_s, peer_addr);
	if (!bss) {
		wpa_printf(MSG_DEBUG, "PASN: BSS not found");
		return NULL;
	}

	rsne = wpa_bss_get_ie(bss, WLAN_EID_RSN);
	if (!rsne) {
		wpa_printf(MSG_DEBUG, "PASN: BSS without RSNE");
		return NULL;
	}

	ret = wpa_parse_wpa_ie(rsne, *(rsne + 1) + 2, &rsne_data);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed parsing RSNE data");
		return NULL;
	}

	if (!(rsne_data.key_mgmt & akmp) ||
	    !(rsne_data.pairwise_cipher & cipher)) {
		wpa_printf(MSG_DEBUG,
			   "PASN: AP does not support requested AKMP or cipher");
		return NULL;
	}

	return bss;
}


static void wpas_pasn_auth_start_cb(struct wpa_radio_work *work, int deinit)
{
	struct wpa_supplicant *wpa_s = work->wpa_s;
	struct wpa_pasn_auth_work *awork = work->ctx;
	struct pasn_data *pasn = &wpa_s->pasn;
	struct wpa_ssid *ssid;
	struct wpa_bss *bss;
	const u8 *rsne, *rsnxe;
#ifdef CONFIG_FILS
	const u8 *indic;
	u16 fils_info;
#endif /* CONFIG_FILS */
	u16 capab = 0;
	bool derive_kdk;
	int ret;

	wpa_printf(MSG_DEBUG, "PASN: auth_start_cb: deinit=%d", deinit);

	if (deinit) {
		if (work->started) {
			eloop_cancel_timeout(wpas_pasn_auth_work_timeout,
					     wpa_s, NULL);
			wpa_s->pasn_auth_work = NULL;
		}

		wpas_pasn_free_auth_work(awork);
		return;
	}

	/*
	 * It is possible that by the time the callback is called, the PASN
	 * authentication is not allowed, e.g., a connection with the AP was
	 * established.
	 */
	bss = wpas_pasn_allowed(wpa_s, awork->peer_addr, awork->akmp,
				awork->cipher);
	if (!bss) {
		wpa_printf(MSG_DEBUG, "PASN: auth_start_cb: Not allowed");
		goto fail;
	}

	rsne = wpa_bss_get_ie(bss, WLAN_EID_RSN);
	if (!rsne) {
		wpa_printf(MSG_DEBUG, "PASN: BSS without RSNE");
		goto fail;
	}

	rsnxe = wpa_bss_get_ie(bss, WLAN_EID_RSNX);

	derive_kdk = (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_LTF_STA) &&
		ieee802_11_rsnx_capab(rsnxe,
				      WLAN_RSNX_CAPAB_SECURE_LTF);
#ifdef CONFIG_TESTING_OPTIONS
	if (!derive_kdk)
		derive_kdk = wpa_s->conf->force_kdk_derivation;
#endif /* CONFIG_TESTING_OPTIONS */
	if (derive_kdk)
		pasn_enable_kdk_derivation(pasn);
	else
		pasn_disable_kdk_derivation(pasn);

	wpa_printf(MSG_DEBUG, "PASN: kdk_len=%zu", pasn->kdk_len);

	if ((wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_LTF_STA) &&
	    ieee802_11_rsnx_capab(rsnxe, WLAN_RSNX_CAPAB_SECURE_LTF))
		pasn->secure_ltf = true;
	else
		pasn->secure_ltf = false;

#ifdef CONFIG_TESTING_OPTIONS
	pasn->corrupt_mic = wpa_s->conf->pasn_corrupt_mic;
#endif /* CONFIG_TESTING_OPTIONS */

	capab |= BIT(WLAN_RSNX_CAPAB_SAE_H2E);
	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_LTF_STA)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_LTF);
	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_RTT_STA)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_RTT);
	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_PROT_RANGE_NEG_STA)
		capab |= BIT(WLAN_RSNX_CAPAB_URNM_MFPR);
	pasn_set_rsnxe_caps(pasn, capab);
	pasn_register_callbacks(pasn, wpa_s, wpas_pasn_send_mlme, NULL);
	ssid = wpa_config_get_network(wpa_s->conf, awork->network_id);

#ifdef CONFIG_SAE
	if (awork->akmp == WPA_KEY_MGMT_SAE) {
		if (!ssid) {
			wpa_printf(MSG_DEBUG,
				   "PASN: No network profile found for SAE");
			goto fail;
		}
		pasn_set_pt(pasn, wpas_pasn_sae_derive_pt(ssid, awork->group));
		if (!pasn->pt) {
			wpa_printf(MSG_DEBUG, "PASN: Failed to derive PT");
			goto fail;
		}
		pasn->network_id = ssid->id;
	}
#endif /* CONFIG_SAE */

#ifdef CONFIG_FILS
	/* Prepare needed information for wpas_pasn_wd_fils_auth(). */
	if (awork->akmp == WPA_KEY_MGMT_FILS_SHA256 ||
	    awork->akmp == WPA_KEY_MGMT_FILS_SHA384) {
		indic = wpa_bss_get_ie(bss, WLAN_EID_FILS_INDICATION);
		if (!ssid) {
			wpa_printf(MSG_DEBUG, "PASN: FILS: No network block");
		} else if (!indic || indic[1] < 2) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Missing FILS Indication IE");
		} else {
			fils_info = WPA_GET_LE16(indic + 2);
			if ((fils_info & BIT(9)) && ssid) {
				pasn->eapol = wpa_s->eapol;
				pasn->network_id = ssid->id;
				wpas_pasn_initiate_eapol(pasn, ssid);
				pasn->fils_eapol = true;
			} else {
				wpa_printf(MSG_DEBUG,
					   "PASN: FILS auth without PFS not supported");
			}
		}
		pasn->fast_reauth = wpa_s->conf->fast_reauth;
	}
#endif /* CONFIG_FILS */

	pasn_set_initiator_pmksa(pasn, wpa_sm_get_pmksa_cache(wpa_s->wpa));

	if (wpa_key_mgmt_ft(awork->akmp)) {
#ifdef CONFIG_IEEE80211R
		ret = wpa_pasn_ft_derive_pmk_r1(wpa_s->wpa, awork->akmp,
						awork->peer_addr,
						pasn->pmk_r1,
						&pasn->pmk_r1_len,
						pasn->pmk_r1_name);
		if (ret) {
			wpa_printf(MSG_DEBUG,
				   "PASN: FT: Failed to derive keys");
			goto fail;
		}
#else /* CONFIG_IEEE80211R */
		goto fail;
#endif /* CONFIG_IEEE80211R */
	}


	ret = wpas_pasn_start(pasn, awork->own_addr, awork->peer_addr,
			      awork->peer_addr, awork->akmp, awork->cipher,
			      awork->group, bss->freq, rsne, *(rsne + 1) + 2,
			      rsnxe, rsnxe ? *(rsnxe + 1) + 2 : 0,
			      awork->comeback);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Failed to start PASN authentication");
		goto fail;
	}
	eloop_register_timeout(2, 0, wpas_pasn_auth_work_timeout, wpa_s, NULL);

	/* comeback token is no longer needed at this stage */
	wpabuf_free(awork->comeback);
	awork->comeback = NULL;

	wpa_s->pasn_auth_work = work;
	return;
fail:
	wpas_pasn_free_auth_work(awork);
	work->ctx = NULL;
	radio_work_done(work);
}


int wpas_pasn_auth_start(struct wpa_supplicant *wpa_s,
			 const u8 *own_addr, const u8 *peer_addr,
			 int akmp, int cipher, u16 group, int network_id,
			 const u8 *comeback, size_t comeback_len)
{
	struct wpa_pasn_auth_work *awork;
	struct wpa_bss *bss;

	wpa_printf(MSG_DEBUG, "PASN: Start: " MACSTR " akmp=0x%x, cipher=0x%x",
		   MAC2STR(peer_addr), akmp, cipher);

	/*
	 * TODO: Consider modifying the offchannel logic to handle additional
	 * Management frames other then Action frames. For now allow PASN only
	 * with drivers that support off-channel TX.
	 */
	if (!(wpa_s->drv_flags & WPA_DRIVER_FLAGS_OFFCHANNEL_TX)) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Driver does not support offchannel TX");
		return -1;
	}

	if (radio_work_pending(wpa_s, "pasn-start-auth")) {
		wpa_printf(MSG_DEBUG,
			   "PASN: send_auth: Work is already pending");
		return -1;
	}

	if (wpa_s->pasn_auth_work) {
		wpa_printf(MSG_DEBUG, "PASN: send_auth: Already in progress");
		return -1;
	}

	bss = wpas_pasn_allowed(wpa_s, peer_addr, akmp, cipher);
	if (!bss)
		return -1;

	wpas_pasn_reset(wpa_s);

	awork = os_zalloc(sizeof(*awork));
	if (!awork)
		return -1;

	os_memcpy(awork->own_addr, own_addr, ETH_ALEN);
	os_memcpy(awork->peer_addr, peer_addr, ETH_ALEN);
	awork->akmp = akmp;
	awork->cipher = cipher;
	awork->group = group;
	awork->network_id = network_id;

	if (comeback && comeback_len) {
		awork->comeback = wpabuf_alloc_copy(comeback, comeback_len);
		if (!awork->comeback) {
			wpas_pasn_free_auth_work(awork);
			return -1;
		}
	}

	if (radio_add_work(wpa_s, bss->freq, "pasn-start-auth", 1,
			   wpas_pasn_auth_start_cb, awork) < 0) {
		wpas_pasn_free_auth_work(awork);
		return -1;
	}

	wpa_printf(MSG_DEBUG, "PASN: Auth work successfully added");
	return 0;
}


void wpas_pasn_auth_stop(struct wpa_supplicant *wpa_s)
{
	struct pasn_data *pasn = &wpa_s->pasn;

	if (!wpa_s->pasn.ecdh)
		return;

	wpa_printf(MSG_DEBUG, "PASN: Stopping authentication");

	wpas_pasn_auth_status(wpa_s, pasn->peer_addr, pasn_get_akmp(pasn),
			      pasn_get_cipher(pasn),
			      pasn->status, pasn->comeback,
			      pasn->comeback_after);

	wpas_pasn_reset(wpa_s);
}


static int wpas_pasn_immediate_retry(struct wpa_supplicant *wpa_s,
				     struct pasn_data *pasn,
				     struct wpa_pasn_params_data *params)
{
	int akmp = pasn_get_akmp(pasn);
	int cipher = pasn_get_cipher(pasn);
	u16 group = pasn->group;
	u8 own_addr[ETH_ALEN];
	u8 peer_addr[ETH_ALEN];

	wpa_printf(MSG_DEBUG, "PASN: Immediate retry");
	os_memcpy(own_addr, pasn->own_addr, ETH_ALEN);
	os_memcpy(peer_addr, pasn->peer_addr, ETH_ALEN);
	wpas_pasn_reset(wpa_s);

	return wpas_pasn_auth_start(wpa_s, own_addr, peer_addr, akmp, cipher,
				    group, pasn->network_id,
				    params->comeback, params->comeback_len);
}


static void wpas_pasn_deauth_cb(struct ptksa_cache_entry *entry)
{
	struct wpa_supplicant *wpa_s = entry->ctx;
	u8 own_addr[ETH_ALEN];
	u8 peer_addr[ETH_ALEN];

	/* Use a copy of the addresses from the entry to avoid issues with the
	 * entry getting freed during deauthentication processing. */
	os_memcpy(own_addr, entry->own_addr, ETH_ALEN);
	os_memcpy(peer_addr, entry->addr, ETH_ALEN);
	wpas_pasn_deauthenticate(wpa_s, own_addr, peer_addr);
}


int wpas_pasn_auth_rx(struct wpa_supplicant *wpa_s,
		      const struct ieee80211_mgmt *mgmt, size_t len)
{
	struct pasn_data *pasn = &wpa_s->pasn;
	struct wpa_pasn_params_data pasn_data;
	int ret;

	if (!wpa_s->pasn_auth_work)
		return -2;

	pasn_register_callbacks(pasn, wpa_s, wpas_pasn_send_mlme, NULL);
	ret = wpa_pasn_auth_rx(pasn, (const u8 *) mgmt, len, &pasn_data);
	if (ret == 0) {
		ptksa_cache_add(wpa_s->ptksa, pasn->own_addr, pasn->peer_addr,
				pasn_get_cipher(pasn),
				dot11RSNAConfigPMKLifetime,
				pasn_get_ptk(pasn),
				wpa_s->pasn_params ? wpas_pasn_deauth_cb : NULL,
				wpa_s->pasn_params ? wpa_s : NULL,
				pasn_get_akmp(pasn));

		if (pasn->pmksa_entry)
			wpa_sm_set_cur_pmksa(wpa_s->wpa, pasn->pmksa_entry);
	}

	forced_memzero(pasn_get_ptk(pasn), sizeof(pasn->ptk));

	if (ret == -1) {
		wpas_pasn_auth_stop(wpa_s);
		wpas_pasn_auth_work_done(wpa_s, PASN_STATUS_FAILURE);
	}

	if (ret == 1)
		ret = wpas_pasn_immediate_retry(wpa_s, pasn, &pasn_data);

	return ret;
}


void wpas_pasn_auth_trigger(struct wpa_supplicant *wpa_s,
			    struct pasn_auth *pasn_auth)
{
	struct pasn_peer *src, *dst;
	unsigned int i, num_peers = pasn_auth->num_peers;

	if (wpa_s->pasn_params) {
		wpa_printf(MSG_DEBUG,
			   "PASN: auth_trigger: Already in progress");
		return;
	}

	if (!num_peers || num_peers > WPAS_MAX_PASN_PEERS) {
		wpa_printf(MSG_DEBUG,
			   "PASN: auth trigger: Invalid number of peers");
		return;
	}

	wpa_s->pasn_params = os_zalloc(sizeof(struct pasn_auth));
	if (!wpa_s->pasn_params) {
		wpa_printf(MSG_DEBUG,
			   "PASN: auth trigger: Failed to allocate a buffer");
		return;
	}

	wpa_s->pasn_count = 0;
	wpa_s->pasn_params->num_peers = num_peers;

	for (i = 0; i < num_peers; i++) {
		dst = &wpa_s->pasn_params->peer[i];
		src = &pasn_auth->peer[i];
		os_memcpy(dst->own_addr, wpa_s->own_addr, ETH_ALEN);
		os_memcpy(dst->peer_addr, src->peer_addr, ETH_ALEN);
		dst->ltf_keyseed_required = src->ltf_keyseed_required;
		dst->status = PASN_STATUS_SUCCESS;

		if (!is_zero_ether_addr(src->own_addr)) {
			os_memcpy(dst->own_addr, src->own_addr, ETH_ALEN);
			wpa_printf(MSG_DEBUG, "PASN: Own (source) MAC addr: "
				   MACSTR, MAC2STR(dst->own_addr));
		}
	}

	if (pasn_auth->action == PASN_ACTION_DELETE_SECURE_RANGING_CONTEXT) {
		wpas_pasn_delete_peers(wpa_s, wpa_s->pasn_params);
		os_free(wpa_s->pasn_params);
		wpa_s->pasn_params = NULL;
	} else if (pasn_auth->action == PASN_ACTION_AUTH) {
		wpas_pasn_configure_next_peer(wpa_s, wpa_s->pasn_params);
	}
}


int wpas_pasn_auth_tx_status(struct wpa_supplicant *wpa_s,
			     const u8 *data, size_t data_len, u8 acked)

{
	struct pasn_data *pasn = &wpa_s->pasn;
	int ret;

	if (!wpa_s->pasn_auth_work) {
		wpa_printf(MSG_DEBUG,
			   "PASN: auth_tx_status: no work in progress");
		return -1;
	}

	ret = wpa_pasn_auth_tx_status(pasn, data, data_len, acked);
	if (ret != 1)
		return ret;

	if (!wpa_s->pasn_params) {
		wpas_pasn_auth_stop(wpa_s);
		return 0;
	}

	wpas_pasn_set_keys_from_cache(wpa_s, pasn->own_addr, pasn->peer_addr,
				      pasn_get_cipher(pasn),
				      pasn_get_akmp(pasn));
	wpas_pasn_auth_stop(wpa_s);
	wpas_pasn_auth_work_done(wpa_s, PASN_STATUS_SUCCESS);

	return 0;
}


int wpas_pasn_deauthenticate(struct wpa_supplicant *wpa_s, const u8 *own_addr,
			     const u8 *peer_addr)
{
	struct wpa_bss *bss;
	struct wpabuf *buf;
	struct ieee80211_mgmt *deauth;
	int ret;

	if (ether_addr_equal(wpa_s->bssid, peer_addr)) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Cannot deauthenticate from current BSS");
		return -1;
	}

	wpa_drv_set_secure_ranging_ctx(wpa_s, own_addr, peer_addr, 0, 0, NULL,
				       0, NULL, 1);

	wpa_printf(MSG_DEBUG, "PASN: deauth: Flushing all PTKSA entries for "
		   MACSTR, MAC2STR(peer_addr));
	ptksa_cache_flush(wpa_s->ptksa, peer_addr, WPA_CIPHER_NONE);

	bss = wpa_bss_get_bssid(wpa_s, peer_addr);
	if (!bss) {
		wpa_printf(MSG_DEBUG, "PASN: deauth: BSS not found");
		return -1;
	}

	buf = wpabuf_alloc(64);
	if (!buf) {
		wpa_printf(MSG_DEBUG, "PASN: deauth: Failed wpabuf allocate");
		return -1;
	}

	deauth = wpabuf_put(buf, offsetof(struct ieee80211_mgmt,
					  u.deauth.variable));

	deauth->frame_control = host_to_le16((WLAN_FC_TYPE_MGMT << 2) |
					     (WLAN_FC_STYPE_DEAUTH << 4));

	os_memcpy(deauth->da, peer_addr, ETH_ALEN);
	os_memcpy(deauth->sa, own_addr, ETH_ALEN);
	os_memcpy(deauth->bssid, peer_addr, ETH_ALEN);
	deauth->u.deauth.reason_code =
		host_to_le16(WLAN_REASON_PREV_AUTH_NOT_VALID);

	/*
	 * Since we do not expect any response from the AP, implement the
	 * Deauthentication frame transmission using direct call to the driver
	 * without a radio work.
	 */
	ret = wpa_drv_send_mlme(wpa_s, wpabuf_head(buf), wpabuf_len(buf), 1,
				bss->freq, 0);

	wpabuf_free(buf);
	wpa_printf(MSG_DEBUG, "PASN: deauth: send_mlme ret=%d", ret);

	return ret;
}
