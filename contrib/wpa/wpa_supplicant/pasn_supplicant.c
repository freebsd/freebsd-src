/*
 * wpa_supplicant - PASN processing
 *
 * Copyright (C) 2019 Intel Corporation
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
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
#include "rsn_supp/wpa_ie.h"
#include "rsn_supp/pmksa_cache.h"
#include "wpa_supplicant_i.h"
#include "driver_i.h"
#include "bss.h"
#include "scan.h"
#include "config.h"
#include "sme.h"

static const int dot11RSNAConfigPMKLifetime = 43200;

struct wpa_pasn_auth_work {
	u8 own_addr[ETH_ALEN];
	u8 peer_addr[ETH_ALEN];
	int akmp;
	int cipher;
	u16 group;
	int network_id;
	struct wpabuf *comeback;
	unsigned int auth_alg;
	int group_cipher;
	int group_mgmt_cipher;
#ifdef CONFIG_ENC_ASSOC
	u16 rsn_capab;
	u8 *rsnxe_data;
	bool is_ml_peer;
#endif /* CONFIG_ENC_ASSOC */
};


static void wpas_pasn_free_peer_password(struct pasn_peer *peer)
{
	str_clear_free(peer->password);
	peer->password = NULL;
}


static void wpas_pasn_free_peer_comeback(struct pasn_peer *peer)
{
	os_free(peer->comeback);
	peer->comeback = NULL;
}


static void wpas_pasn_free_peer(struct pasn_peer *peer)
{
	wpas_pasn_free_peer_password(peer);
	wpas_pasn_free_peer_comeback(peer);
}


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
#ifdef CONFIG_ENC_ASSOC
	os_free(awork->rsnxe_data);
	awork->rsnxe_data = NULL;
#endif /* CONFIG_ENC_ASSOC */
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
			     (const u8 *) ssid->sae_password_id,
			     ssid->sae_password_id ?
			     os_strlen(ssid->sae_password_id) : 0);
}


#ifdef CONFIG_ENC_ASSOC
struct sae_pt *
wpas_pasn_sae_derive_pt_for_eppke(struct wpa_ssid *ssid, int group)
{
	const char *password = ssid->sae_password;
	int groups[2] = { group, 0 };
	const u8 *password_id = NULL;
	size_t password_id_len = 0;

	if (!password)
		password = ssid->passphrase;

	if (!password) {
		wpa_printf(MSG_DEBUG, "EPPKE: SAE without a password");
		return NULL;
	}

	/* Prefer an alternative (changing) password identifier if available */
	if (ssid->alt_sae_password_ids && ssid->alt_sae_password_ids->num) {
		unsigned int idx =
			os_random() % ssid->alt_sae_password_ids->num;
		struct wpabuf *id = ssid->alt_sae_password_ids->buf[idx];

		password_id = wpabuf_head(id);
		password_id_len = wpabuf_len(id);
		wpa_hexdump(MSG_DEBUG,
			    "EPPKE: Prepare PT for alternative password ID",
			    password_id, password_id_len);
		ssid->alt_sae_passwords_ids_idx = idx;
		ssid->alt_sae_passwords_ids_used = true;
	} else if (ssid->sae_password_id) {
		password_id = (const u8 *) ssid->sae_password_id;
		password_id_len = os_strlen(ssid->sae_password_id);
	}

	return sae_derive_pt(groups, ssid->ssid, ssid->ssid_len,
			     (const u8 *) password, os_strlen(password),
			     password_id, password_id_len);
}
#endif /* CONFIG_ENC_ASSOC */


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


int wpas_pasn_get_group(struct wpa_supplicant *wpa_s,
			struct wpa_ssid *ssid, struct pasn_data *pasn)
{
	static const int default_groups[] = { 19, 20, 21, 0 };
	const int *groups;
	unsigned int i, j;

	if (ssid && ssid->pasn_groups)
		groups = ssid->pasn_groups;
	else if (wpa_s->conf->pasn_groups)
		groups = wpa_s->conf->pasn_groups;
	else
		groups = default_groups;

	for (i = 0; groups[i]; i++) {
		bool rejected = false;
		bool ap_supported = true;

		if (!dragonfly_suitable_group(groups[i], 1))
			continue;

		if (!pasn)
			return groups[i];

		/* Skip groups already rejected in this session */
		for (j = 0; j < pasn->rejected_group_idx; j++) {
			if (groups[i] == pasn->rejected_groups[j]) {
				rejected = true;
				break;
			}
		}
		if (rejected)
			continue;

		/* Take intersection with AP's supported groups */
		if (pasn->ap_supported_group_idx > 0) {
			ap_supported = false;
			for (j = 0; j < pasn->ap_supported_group_idx; j++) {
				if (groups[i] == pasn->ap_supported_groups[j]) {
					ap_supported = true;
					break;
				}
			}
		}
		if (ap_supported)
			return groups[i];
	}

	/* pasn_groups configured but no suitable group found - failure */
	return 0;
}


static int wpas_pasn_get_params_from_bss(struct wpa_supplicant *wpa_s,
					 struct pasn_peer *peer,
					 struct wpa_bss *bss,
					 struct wpa_ssid *ssid)
{
	int ret;
	const u8 *rsne, *rsnxe;
	struct wpa_ie_data rsne_data;
	int sel, key_mgmt, pairwise_cipher;
	int group;

	group = wpas_pasn_get_group(wpa_s, ssid, NULL);

	if (!group) {
		wpa_printf(MSG_INFO,
			   "PASN: No suitable group found; cannot start authentication");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "PASN: Selected group %d", group);

	rsne = wpa_bss_get_rsne(wpa_s, bss, NULL, false);
	if (!rsne) {
		wpa_printf(MSG_DEBUG, "PASN: BSS without RSNE");
		return -1;
	}

	ret = wpa_parse_wpa_ie(rsne, *(rsne + 1) + 2, &rsne_data);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed parsing RSNE data");
		return -1;
	}

	rsnxe = wpa_bss_get_rsnxe(wpa_s, bss, NULL, false);


	sel = rsne_data.pairwise_cipher;
	if (peer->cipher && peer->cipher != WPA_CIPHER_NONE)
		sel &= peer->cipher;
	else if (ssid && !ssid->temporary && ssid->pairwise_cipher)
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
	if (peer->akmp && peer->akmp != WPA_KEY_MGMT_NONE)
		sel &= peer->akmp;
	else if (ssid && !ssid->temporary && ssid->key_mgmt)
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
	} else if ((sel & WPA_KEY_MGMT_SAE_EXT_KEY) && ssid &&
		   (ieee802_11_rsnx_capab(rsnxe,
					   WLAN_RSNX_CAPAB_SAE_H2E)) &&
		   (wpas_pasn_sae_setup_pt(ssid, group) == 0)) {
		key_mgmt = WPA_KEY_MGMT_SAE_EXT_KEY;
		wpa_printf(MSG_DEBUG, "PASN: using KEY_MGMT SAE (ext key)");
	} else if ((sel & WPA_KEY_MGMT_SAE) && ssid &&
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
	if (ssid)
		peer->network_id = ssid->id;
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


static struct wpa_ssid *
wpas_pasn_add_temporary_network(struct wpa_supplicant *wpa_s,
				const struct wpa_bss *bss, const char *password)
{
	struct wpa_ssid *ssid;

	ssid = wpa_config_add_network(wpa_s->conf);
	if (!ssid) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to allocate SSID block");
		return NULL;
	}

	ssid->ssid = os_memdup(bss->ssid, bss->ssid_len);
	if (!ssid->ssid)
		return NULL;

	ssid->ssid_len = bss->ssid_len;
	ssid->passphrase = os_strdup(password);
	if (!ssid->passphrase) {
		wpa_config_free_ssid(ssid);
		wpa_printf(MSG_DEBUG, "PASN: Failed to copy password");
		return NULL;
	}

	ssid->temporary = true;
	wpa_printf(MSG_DEBUG, "PASN: Created temporary network block for "
		   MACSTR, MAC2STR(bss->bssid));

	return ssid;
}


static struct wpa_bss * wpas_pasn_get_bss(struct wpa_supplicant *wpa_s,
					  const u8 *peer_addr)
{
	struct wpa_bss *bss;

	bss = wpa_bss_get_bssid(wpa_s, peer_addr);
	if (!bss) {
		wpa_supplicant_update_scan_results(wpa_s, peer_addr);
		bss = wpa_bss_get_bssid(wpa_s, peer_addr);
	}

	return bss;
}


static struct wpa_ssid * wpas_pasn_get_network(struct wpa_supplicant *wpa_s,
					       struct wpa_bss *bss)
{
	size_t ssid_str_len;
	const u8 *ssid_str;
	struct wpa_ssid *ssid;

	ssid_str_len = bss->ssid_len;
	ssid_str = bss->ssid;

	/* Get the network configuration based on the obtained SSID */
	for (ssid = wpa_s->conf->ssid; ssid; ssid = ssid->next) {
		if (ssid_str_len == ssid->ssid_len &&
		    os_memcmp(ssid_str, ssid->ssid, ssid_str_len) == 0)
			break;
	}

	return ssid;
}


static void wpas_pasn_configure_next_peer(struct wpa_supplicant *wpa_s,
					  struct pasn_auth *pasn_params)
{
	struct pasn_peer *peer;
	struct wpa_ssid *ssid;

	if (!pasn_params)
		return;

	while (wpa_s->pasn_count < pasn_params->num_peers) {
		struct wpa_bss *bss;
		bool check_cache = true;

		peer = &pasn_params->peer[wpa_s->pasn_count];

		if (ether_addr_equal(wpa_s->bssid, peer->peer_addr)) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Associated peer is not expected");
			peer->status = PASN_STATUS_FAILURE;
			wpa_s->pasn_count++;
			continue;
		}

		bss = wpas_pasn_get_bss(wpa_s, peer->peer_addr);
		if (!bss) {
			wpa_printf(MSG_DEBUG, "PASN: BSS not found");
			peer->status = PASN_STATUS_FAILURE;
			wpa_s->pasn_count++;
			continue;
		}

		ssid = wpas_pasn_get_network(wpa_s, bss);
		if (peer->password && peer->akmp &&
		    peer->akmp != WPA_KEY_MGMT_NONE) {
			ssid = wpas_pasn_add_temporary_network(wpa_s, bss,
							       peer->password);

			if (!ssid) {
				wpa_printf(MSG_DEBUG,
					   "PASN: Failed to create temporary network");
				return;
			}
			peer->temporary_network = true;
		}

		if (ssid && ssid->temporary)
			check_cache = false;

		if (wpas_pasn_get_params_from_bss(wpa_s, peer, bss, ssid)) {
			peer->status = PASN_STATUS_FAILURE;
			wpa_s->pasn_count++;
			continue;
		}

		if (check_cache &&
		    wpas_pasn_set_keys_from_cache(wpa_s, peer->own_addr,
						  peer->peer_addr,
						  peer->cipher,
						  peer->akmp) == 0) {
			peer->status = PASN_STATUS_SUCCESS;
			wpa_s->pasn_count++;
			continue;
		}

		if (wpas_pasn_auth_start(wpa_s, peer->own_addr,
					 peer->peer_addr, peer->akmp,
					 peer->cipher, peer->group,
					 peer->network_id,
					 peer->comeback, peer->comeback_len,
					 WLAN_AUTH_PASN, 0, 0, 0, NULL,
					 false)) {
			peer->status = PASN_STATUS_FAILURE;
			wpa_msg(wpa_s, MSG_INFO, PASN_AUTH_STATUS MACSTR
				" akmp=%s, status=%u",
				MAC2STR(peer->peer_addr),
				wpa_key_mgmt_txt(peer->akmp, WPA_PROTO_RSN),
				peer->status);
			wpa_s->pasn_count++;
			wpas_pasn_free_peer(peer);
			continue;
		}
		wpa_printf(MSG_DEBUG, "PASN: Sent PASN auth start for " MACSTR,
			   MAC2STR(peer->peer_addr));
		return;
	}

	if (wpa_s->pasn_count == pasn_params->num_peers) {
		unsigned int i;

		wpa_drv_send_pasn_resp(wpa_s, pasn_params);
		wpa_printf(MSG_DEBUG, "PASN: Response sent");
		for (i = 0; i < pasn_params->num_peers; i++) {
			peer = &pasn_params->peer[i];
			wpas_pasn_free_peer(peer);

			if (peer->temporary_network) {
				ssid = wpa_config_get_network(wpa_s->conf,
							      peer->network_id);

				if (ssid && ssid->temporary) {
					wpa_config_remove_network(
						wpa_s->conf, peer->network_id);
					wpa_printf(MSG_DEBUG,
						   "PASN: Remove temporary network block of "
						   MACSTR, MAC2STR(peer->peer_addr));
				}
			}
		}
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
		wpas_pasn_free_peer_password(peer);
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
					  int cipher, int auth_alg,
					  int group_cipher,
					  int group_mgmt_cipher)
{
	struct wpa_bss *bss;
	const u8 *rsne;
	struct wpa_ie_data rsne_data;
	int ret;

	if (auth_alg != WLAN_AUTH_EPPKE &&
	    ether_addr_equal(wpa_s->bssid, peer_addr)) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Not doing authentication with current BSS");
		return NULL;
	}

	if (auth_alg == WLAN_AUTH_EPPKE) {
#if defined(CONFIG_SME) && defined(CONFIG_SAE)
		/* EPPKE processing can reach here only when external
		 * authentication is used.
		 *
		 * In this flow, peer_addr is the peer MLD address for MLO.
		 * However, wpa_bss_get_bssid_latest() matches a link BSSID
		 * entry in the BSS table. Use the link BSSID saved by SME
		 * in ext_auth_bssid for BSS lookup.
		 */
		bss = wpa_bss_get_bssid_latest(wpa_s,
					       wpa_s->sme.ext_auth_bssid);
#else /* CONFIG_SME && CONFIG_SAE */
		wpa_printf(MSG_ERROR,
			   "EPPKE ext-auth requires CONFIG_SME and CONFIG_SAE");
		return NULL;
#endif /* CONFIG_SME && CONFIG_SAE */
	} else {
		bss = wpa_bss_get_bssid_latest(wpa_s, peer_addr);
	}
	if (!bss) {
		wpa_printf(MSG_DEBUG, "PASN: BSS not found");
		return NULL;
	}

	rsne = wpa_bss_get_rsne(wpa_s, bss, NULL, false);
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

#ifdef CONFIG_ENC_ASSOC
	if (auth_alg == WLAN_AUTH_EPPKE) {
		if (group_cipher &
		    !(rsne_data.group_cipher & group_cipher)) {
			wpa_printf(MSG_DEBUG,
				   "EPPKE: AP does not support requested group cipher");
			return NULL;
		}
		if (group_mgmt_cipher &&
		    !(rsne_data.mgmt_group_cipher & group_mgmt_cipher)) {
			wpa_printf(MSG_DEBUG,
				   "EPPKE: AP does not support requested group mgmt cipher");
			return NULL;
		}
	}
#endif /* CONFIG_ENC_ASSOC */

	return bss;
}


#ifdef CONFIG_ENC_ASSOC
/*
 * Build RSNE for EPPKE in SME-in-driver mode.
 */
static int wpas_eppke_set_rsne(struct wpa_supplicant *wpa_s,
			       struct pasn_data *pasn,
			       struct wpa_pasn_auth_work *awork)
{
	u8 rsne[257];
	int rsne_len;

	rsne_len = wpa_external_auth_add_rsne(rsne, sizeof(rsne),
					      awork->akmp, awork->cipher,
					      awork->group_cipher,
					      awork->group_mgmt_cipher,
					      awork->rsn_capab,
					      sme_get_ext_auth_pmkid(wpa_s));
	if (rsne_len < 0) {
		wpa_printf(MSG_DEBUG, "EPPKE: Failed to build RSNE");
		return -1;
	}

	pasn_set_rsne(pasn, rsne);
	if (!pasn->rsn_ie)
		return -1;

	wpa_printf(MSG_DEBUG,
		   "EPPKE: RSNE for ext-auth (group=0x%x mgmt=0x%x capab=0x%x)",
		   awork->group_cipher, awork->group_mgmt_cipher,
		   awork->rsn_capab);
	return 0;
}
#endif /* CONFIG_ENC_ASSOC */


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
	u64 capab = 0;
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
				awork->cipher, awork->auth_alg,
				awork->group_cipher,
				awork->group_mgmt_cipher);
	if (!bss) {
		wpa_printf(MSG_DEBUG, "PASN: auth_start_cb: Not allowed");
		goto fail;
	}

	rsne = wpa_bss_get_ie(bss, WLAN_EID_RSN);
	if (!rsne) {
		wpa_printf(MSG_DEBUG, "PASN: BSS without RSNE");
		goto fail;
	}

	/* Use the RSNXOE, if it was included, for actual AP capability check */
	rsnxe = wpa_bss_get_rsnxe(wpa_s, bss, NULL, false);

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

	if (wpa_key_mgmt_sae(awork->akmp))
		capab |= BIT(WLAN_RSNX_CAPAB_SAE_H2E);
	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_LTF_STA)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_LTF);
	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_RTT_STA)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_RTT);
	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_PROT_RANGE_NEG_STA) {
		/*
		 * URNM_MFPR_X20 is a subset of URNM_MFPR which excludes 20 MHz
		 * bandwidth from mandating protected Management frames. Set
		 * URNM_MFPR only when URNM_MFPR_X20 is not set.
		 */
		if (wpa_s->disable_urnm_mfpr) {
			wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_URNM_MFPR, 0);
		} else {
			capab |= BIT(WLAN_RSNX_CAPAB_URNM_MFPR);
			wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_URNM_MFPR, 1);
		}
		if (wpa_s->urnm_mfpr_x20) {
			capab |= BIT(WLAN_RSNX_CAPAB_URNM_MFPR_X20);
			wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_URNM_MFPR_X20,
					 1);
		} else {
			wpa_sm_set_param(wpa_s->wpa, WPA_PARAM_URNM_MFPR_X20,
					 0);
		}
	}
	if ((wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SPP_AMSDU) &&
	    ieee802_11_rsnx_capab(rsnxe, WLAN_RSNX_CAPAB_SPP_A_MSDU))
		capab |= BIT(WLAN_RSNX_CAPAB_SPP_A_MSDU);
	ssid = wpa_config_get_network(wpa_s->conf, awork->network_id);
#ifdef CONFIG_ENC_ASSOC
	if (awork->auth_alg == WLAN_AUTH_EPPKE) {
		if (!ssid) {
			wpa_printf(MSG_DEBUG,
				   "EPPKE: No network profile found");
			goto fail;
		}
		if (!ieee802_11_rsnx_capab(rsnxe, WLAN_RSNX_CAPAB_KEK_IN_PASN))
		{
			wpa_printf(MSG_INFO,
				   "EPPKE: KEK_IN_PASN not set in AP RSNXE");
			goto fail;
		}
		if (!ieee802_11_rsnx_capab(rsnxe,
					   WLAN_RSNX_CAPAB_ASSOC_FRAME_ENCRYPTION)) {
			wpa_printf(MSG_INFO,
				   "EPPKE: ASSOC_FRAME_ENCRYPTION not set in AP RSNXE");
			goto fail;
		}
		if (awork->akmp == WPA_KEY_MGMT_EPPKE &&
		    !ieee802_11_rsnx_capab(rsnxe,
					   WLAN_RSNX_CAPAB_UNAUTH_EPPKE)) {
			wpa_printf(MSG_DEBUG,
				   "EPPKE: AP does not support unauthenticated EPPKE");
			goto fail;
		}
		if (wpa_s->drv_flags2 &
		    WPA_DRIVER_FLAGS2_ASSOCIATION_FRAME_ENCRYPTION) {
			capab |= BIT(WLAN_RSNX_CAPAB_ASSOC_FRAME_ENCRYPTION);
			capab |= BIT(WLAN_RSNX_CAPAB_KEK_IN_PASN);
			pasn->derive_kek = true;
#ifdef CONFIG_SAE
			/*
			 * Advertise support for changing SAE password
			 * identifiers if configured per network profile.
			 */
			if (ssid && ssid->sae_password_id &&
			    ssid->sae_password_id_change &&
			    wpa_key_mgmt_sae_ext_key(awork->akmp)) {
				capab |= BIT_ULL(
					WLAN_RSNX_CAPAB_SAE_PW_ID_CHANGE);
				wpa_sm_set_param(wpa_s->wpa,
						 WPA_PARAM_SAE_PW_ID_CHANGE, 1);
			}
#endif /* CONFIG_SAE */
#ifdef CONFIG_PMKSA_PRIVACY
			if ((wpa_s->drv_flags2 &
			     WPA_DRIVER_FLAGS2_PMKSA_PRIVACY) &&
			    ssid->pmksa_privacy &&
			    ieee802_11_rsnx_capab(
				    rsnxe,
				    WLAN_RSNX_CAPAB_PMKSA_CACHING_PRIVACY))
				capab |= BIT(
					WLAN_RSNX_CAPAB_PMKSA_CACHING_PRIVACY);
#endif /* CONFIG_PMKSA_PRIVACY */
		}
	}
#endif /* CONFIG_ENC_ASSOC */

	pasn_set_rsnxe_caps(pasn, capab);
	pasn_register_callbacks(pasn, wpa_s, wpas_pasn_send_mlme, NULL, NULL,
				NULL);

#ifdef CONFIG_SAE
	if (awork->akmp == WPA_KEY_MGMT_SAE ||
	    awork->akmp == WPA_KEY_MGMT_SAE_EXT_KEY) {
		struct sae_pt *pt = NULL;

		if (!ssid) {
			wpa_printf(MSG_DEBUG,
				   "PASN: No network profile found for SAE");
			goto fail;
		}
#ifdef CONFIG_ENC_ASSOC
		if (awork->auth_alg == WLAN_AUTH_EPPKE)
			pt = wpas_pasn_sae_derive_pt_for_eppke(ssid,
							       awork->group);
#endif /* CONFIG_ENC_ASSOC */
		if (awork->auth_alg != WLAN_AUTH_EPPKE)
			pt = wpas_pasn_sae_derive_pt(ssid, awork->group);
		pasn_set_pt(pasn, pt);
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

#ifdef CONFIG_ENC_ASSOC
	pasn->auth_alg = awork->auth_alg;
	pasn->group_cipher = awork->group_cipher;
	pasn->group_mgmt_cipher = awork->group_mgmt_cipher;
	pasn->rsn_capab = awork->rsn_capab;
	pasn_set_rsnxe_ie(pasn, awork->rsnxe_data);
	pasn->is_ml_peer = awork->is_ml_peer;
	/*
	 * Set network_ctx so the PMKSA entry is stored with the correct
	 * network context for lookup on reconnection.
	 */
	if (awork->auth_alg == WLAN_AUTH_EPPKE && ssid)
		pasn->network_ctx = ssid;

	/* Build RSNE for EPPKE Authentication in SME-in-driver mode */
	if (awork->auth_alg == WLAN_AUTH_EPPKE &&
	    wpas_eppke_set_rsne(wpa_s, pasn, awork) < 0) {
		wpa_printf(MSG_DEBUG, "EPPKE: Failed to configure RSNE");
		goto fail;
	}
#endif /* CONFIG_ENC_ASSOC */

	/* PASN has not been defined to be modified for RSN overriding, so use
	 * the RSNE and RSNXE from the AP for PASN MIC calculation instead of
	 * the RSNO elements, if any. */
	rsnxe = wpa_bss_get_ie(bss, WLAN_EID_RSNX);

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
			 const u8 *comeback, size_t comeback_len,
			 unsigned int auth_alg, int group_cipher,
			 int group_mgmt_cipher, u16 rsn_capab,
			 const u8 *rsnxe_data, bool is_ml_peer)
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

	bss = wpas_pasn_allowed(wpa_s, peer_addr, akmp, cipher, auth_alg,
				group_cipher, group_mgmt_cipher);
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
	awork->auth_alg = auth_alg;
	awork->group_cipher = group_cipher;
	awork->group_mgmt_cipher = group_mgmt_cipher;
#ifdef CONFIG_ENC_ASSOC
	awork->rsn_capab = rsn_capab;
	awork->is_ml_peer = is_ml_peer;

	if (rsnxe_data) {
		awork->rsnxe_data = os_memdup(rsnxe_data, 2 + rsnxe_data[1]);
		if (!awork->rsnxe_data) {
			wpas_pasn_free_auth_work(awork);
			return -1;
		}
	}
#endif /* CONFIG_ENC_ASSOC */

	if (comeback && comeback_len) {
		awork->comeback = wpabuf_alloc_copy(comeback, comeback_len);
		if (!awork->comeback) {
			wpas_pasn_free_auth_work(awork);
			return -1;
		}
	}

	if (!radio_add_work(wpa_s, bss->freq, "pasn-start-auth", 1,
			    wpas_pasn_auth_start_cb, awork)) {
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

	/* Reset rejected group state when authentication ends */
	pasn->rejected_group_idx = 0;
	os_memset(pasn->rejected_groups, 0, sizeof(pasn->rejected_groups));
}


void wpas_pasn_free_params(struct wpa_supplicant *wpa_s)
{
	unsigned int i;

	if (!wpa_s->pasn_params)
		return;

	for (i = 0; i < wpa_s->pasn_params->num_peers; i++)
		wpas_pasn_free_peer(&wpa_s->pasn_params->peer[i]);

	os_free(wpa_s->pasn_params);
	wpa_s->pasn_params = NULL;
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
	int network_id;
	unsigned int auth_alg;

	wpa_printf(MSG_DEBUG, "PASN: Immediate retry");
	os_memcpy(own_addr, pasn->own_addr, ETH_ALEN);
	os_memcpy(peer_addr, pasn->peer_addr, ETH_ALEN);

	/* Hold network ID to avoid losing it in wpas_pasn_reset(). */
	network_id = pasn->network_id;

	/*
	 * Cache auth_alg before reset as wpas_pasn_reset() clears the pasn
	 * struct. This path is shared with EPPKE, so without preserving it,
	 * a group rejection retry would incorrectly restart with PASN instead
	 * of EPPKE.
	 */
	auth_alg = pasn->auth_alg;

	wpas_pasn_reset(wpa_s);

	return wpas_pasn_auth_start(wpa_s, own_addr, peer_addr, akmp, cipher,
				    group, network_id, params->comeback,
				    params->comeback_len, auth_alg,
				    pasn->group_cipher,
				    pasn->group_mgmt_cipher, pasn->rsn_capab,
				    pasn->rsnxe_ie, pasn->is_ml_peer);
}


static int wpas_pasn_retry_with_next_group(struct wpa_supplicant *wpa_s,
					   struct pasn_data *pasn)
{
	struct wpa_pasn_params_data params;
	u16 next_group;
	struct wpa_ssid *ssid = NULL;

#ifdef CONFIG_ENC_ASSOC
	if (pasn->auth_alg == WLAN_AUTH_EPPKE)
		ssid = wpa_s->current_ssid;
#endif /* CONFIG_ENC_ASSOC */

	next_group = (u16) wpas_pasn_get_group(wpa_s, ssid, pasn);
	if (!next_group) {
		wpa_printf(MSG_DEBUG,
			   "PASN: No more groups to try after rejection");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "PASN: Retrying with group %u after rejection",
		   next_group);

	pasn->group = next_group;

	os_memset(&params, 0, sizeof(params));
	return wpas_pasn_immediate_retry(wpa_s, pasn, &params);
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


static void wpas_pasn_store_comeback_data(struct wpa_supplicant *wpa_s,
					  const struct wpabuf *comeback,
					  u16 comeback_after)
{
	struct pasn_peer *peer;

	if (!wpa_s->pasn_params)
		return;

	peer = &wpa_s->pasn_params->peer[wpa_s->pasn_count];
	if (!peer)
		return;

	wpas_pasn_free_peer_comeback(peer);
	peer->comeback = os_memdup(wpabuf_head(comeback), wpabuf_len(comeback));
	if (!peer->comeback) {
		wpa_printf(MSG_ERROR,
			   "PASN: Mem alloc failed for comeback data");
		return;
	}

	peer->comeback_len = wpabuf_len(comeback);
	peer->comeback_after = comeback_after;
}


int wpas_pasn_auth_rx(struct wpa_supplicant *wpa_s,
		      const struct ieee80211_mgmt *mgmt, size_t len)
{
	struct pasn_data *pasn = &wpa_s->pasn;
	struct wpa_pasn_params_data pasn_data;
	int ret;

	if (!wpa_s->pasn_auth_work)
		return -2;

	wpabuf_free(pasn->frame);
	pasn->frame = NULL;

	pasn_register_callbacks(pasn, wpa_s, wpas_pasn_send_mlme, NULL, NULL,
				NULL);
	ret = wpa_pasn_auth_rx(pasn, (const u8 *) mgmt, len, &pasn_data);
	if (ret == 0) {
		ptksa_cache_add(wpa_s->ptksa, pasn->own_addr, pasn->peer_addr,
				pasn_get_cipher(pasn),
				dot11RSNAConfigPMKLifetime,
				pasn_get_ptk(pasn),
				wpa_s->pasn_params ? wpas_pasn_deauth_cb : NULL,
				wpa_s->pasn_params ? wpa_s : NULL,
				pasn_get_akmp(pasn), pasn->auth_alg);

		if (pasn->pmksa_entry)
			wpa_sm_set_cur_pmksa(wpa_s->wpa, pasn->pmksa_entry);

		if (pasn->auth_alg == WLAN_AUTH_EPPKE) {
#ifdef CONFIG_SME
			os_memcpy(wpa_s->sme.sae.pmkid, pasn->sae.pmkid,
				  PMKID_LEN);
#endif /* CONFIG_SME */
			wpa_sm_set_pmk(wpa_s->wpa, pasn->pmk, pasn->pmk_len,
				       pasn->sae.pmkid, NULL);
		}
	}

	forced_memzero(pasn_get_ptk(pasn), sizeof(pasn->ptk));

	if (ret == -1) {
		if (pasn->status == WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY &&
		    pasn->comeback && wpabuf_len(pasn->comeback))
			wpas_pasn_store_comeback_data(wpa_s, pasn->comeback,
						      pasn->comeback_after);
		wpas_pasn_auth_stop(wpa_s);
		wpas_pasn_auth_work_done(wpa_s, PASN_STATUS_FAILURE);
	}

	if (ret == 1)
		ret = wpas_pasn_immediate_retry(wpa_s, pasn, &pasn_data);

	if (ret == 2) {
		ret = wpas_pasn_retry_with_next_group(wpa_s, pasn);
		if (ret) {
			wpa_printf(MSG_INFO,
				   "PASN: Group rejection retry failed");
			wpas_pasn_auth_stop(wpa_s);
			wpas_pasn_auth_work_done(wpa_s, PASN_STATUS_FAILURE);
		}
	}

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
		dst->akmp = src->akmp;
		dst->cipher = src->cipher;
		if (src->password) {
			dst->password = os_strdup(src->password);
			if (!dst->password) {
				wpa_printf(MSG_DEBUG,
					   "PASN: Mem alloc failed for password");
				goto fail;
			}
		}
		if (src->comeback_len && src->comeback) {
			dst->comeback = os_memdup(src->comeback,
						  src->comeback_len);
			if (!dst->comeback) {
				wpa_printf(MSG_DEBUG,
					   "PASN: Mem alloc failed for comeback cookie");
				goto fail;
			}
			dst->comeback_len = src->comeback_len;
		}

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

	return;

fail:
	wpas_pasn_free_params(wpa_s);
}


#ifdef CONFIG_SME
#ifdef CONFIG_ENC_ASSOC
static u16 wpas_eppke_external_auth_set_keys(struct wpa_supplicant *wpa_s,
					     struct pasn_data *pasn, bool acked)
{
	static const u8 zero[WPA_TK_MAX_LEN] = { 0 };
	enum wpa_alg alg;
	struct ptksa_cache_entry *entry;

	if (!acked) {
		wpa_printf(MSG_DEBUG,
			   "EPPKE: Authentication frame 3 TX was not ACKed");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	alg = wpa_cipher_to_alg(pasn_get_cipher(pasn));
	entry = ptksa_cache_get(wpa_s->ptksa, pasn->peer_addr,
				pasn_get_cipher(pasn));
	if (!entry) {
		wpa_printf(MSG_INFO,
			   "EPPKE: No PTKSA found to configure keys");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	/* Install TK to driver. */
	if (wpa_drv_set_key(wpa_s, -1, alg, pasn->peer_addr, 0, 1, zero, 6,
			    entry->ptk.tk, entry->ptk.tk_len,
			    KEY_FLAG_PAIRWISE_RX_TX)) {
		wpa_printf(MSG_DEBUG,
			   "EPPKE: Failed to install TK to driver");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	/* Install LTF Keyseed to driver. */
	if (pasn->secure_ltf &&
	    wpa_drv_set_secure_ranging_ctx(wpa_s, pasn->own_addr,
					   pasn->peer_addr,
					   pasn_get_cipher(pasn), 0, NULL,
					   entry->ptk.ltf_keyseed_len,
					   entry->ptk.ltf_keyseed, 0)) {
		wpa_printf(MSG_DEBUG,
			   "EPPKE: Failed to install LTF Keyseed");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	return WLAN_STATUS_SUCCESS;
}
#endif /* CONFIG_ENC_ASSOC */
#endif /* CONFIG_SME */


int wpas_pasn_auth_tx_status(struct wpa_supplicant *wpa_s,
			     const u8 *data, size_t data_len, u8 acked)

{
	struct pasn_data *pasn = &wpa_s->pasn;
	int ret;
	enum pasn_status auth_status = PASN_STATUS_SUCCESS;

	if (!wpa_s->pasn_auth_work) {
		wpa_printf(MSG_DEBUG,
			   "PASN: auth_tx_status: no work in progress");
		return -1;
	}

	ret = wpa_pasn_auth_tx_status(pasn, data, data_len, acked);
	if (ret != 1)
		return ret;

	if (pasn->auth_alg == WLAN_AUTH_EPPKE) {
#ifdef CONFIG_SME
#ifdef CONFIG_ENC_ASSOC
		u16 status;

		status = wpas_eppke_external_auth_set_keys(wpa_s, pasn, acked);
		if (status != WLAN_STATUS_SUCCESS)
			auth_status = PASN_STATUS_FAILURE;
#ifdef CONFIG_SAE
		sme_send_external_auth_status(wpa_s, status);
#endif /* CONFIG_SAE */
#endif /* CONFIG_ENC_ASSOC */
#endif /* CONFIG_SME */
		goto auth_done;
	}

	if (!wpa_s->pasn_params) {
		wpas_pasn_auth_stop(wpa_s);
		return 0;
	}

	wpas_pasn_set_keys_from_cache(wpa_s, pasn->own_addr, pasn->peer_addr,
				      pasn_get_cipher(pasn),
				      pasn_get_akmp(pasn));
auth_done:
	wpas_pasn_auth_stop(wpa_s);
	wpas_pasn_auth_work_done(wpa_s, auth_status);

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
