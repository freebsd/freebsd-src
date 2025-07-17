/*
 * hostapd / WPA authenticator glue code
 * Copyright (c) 2002-2022, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "utils/list.h"
#include "common/ieee802_11_defs.h"
#include "common/sae.h"
#include "common/wpa_ctrl.h"
#include "common/ptksa_cache.h"
#include "crypto/sha1.h"
#include "eapol_auth/eapol_auth_sm.h"
#include "eapol_auth/eapol_auth_sm_i.h"
#include "eap_server/eap.h"
#include "l2_packet/l2_packet.h"
#include "eth_p_oui.h"
#include "hostapd.h"
#include "ieee802_1x.h"
#include "preauth_auth.h"
#include "sta_info.h"
#include "tkip_countermeasures.h"
#include "ap_drv_ops.h"
#include "ap_config.h"
#include "ieee802_11.h"
#include "ieee802_11_auth.h"
#include "pmksa_cache_auth.h"
#include "wpa_auth.h"
#include "wpa_auth_glue.h"


static void hostapd_wpa_auth_conf(struct hostapd_bss_config *conf,
				  struct hostapd_config *iconf,
				  struct wpa_auth_config *wconf)
{
	int sae_pw_id;

	os_memset(wconf, 0, sizeof(*wconf));
	wconf->wpa = conf->wpa;
	wconf->extended_key_id = conf->extended_key_id;
	wconf->wpa_key_mgmt = conf->wpa_key_mgmt;
	wconf->wpa_pairwise = conf->wpa_pairwise;
	wconf->wpa_group = conf->wpa_group;
	wconf->wpa_group_rekey = conf->wpa_group_rekey;
	wconf->wpa_strict_rekey = conf->wpa_strict_rekey;
	wconf->wpa_gmk_rekey = conf->wpa_gmk_rekey;
	wconf->wpa_ptk_rekey = conf->wpa_ptk_rekey;
	wconf->wpa_group_update_count = conf->wpa_group_update_count;
	wconf->wpa_disable_eapol_key_retries =
		conf->wpa_disable_eapol_key_retries;
	wconf->wpa_pairwise_update_count = conf->wpa_pairwise_update_count;
	wconf->rsn_pairwise = conf->rsn_pairwise;
	wconf->rsn_preauth = conf->rsn_preauth;
	wconf->eapol_version = conf->eapol_version;
#ifdef CONFIG_MACSEC
	if (wconf->eapol_version > 2)
		wconf->eapol_version = 2;
#endif /* CONFIG_MACSEC */
	wconf->wmm_enabled = conf->wmm_enabled;
	wconf->wmm_uapsd = conf->wmm_uapsd;
	wconf->disable_pmksa_caching = conf->disable_pmksa_caching;
#ifdef CONFIG_OCV
	wconf->ocv = conf->ocv;
#endif /* CONFIG_OCV */
	wconf->okc = conf->okc;
	wconf->ieee80211w = conf->ieee80211w;
	wconf->beacon_prot = conf->beacon_prot;
	wconf->group_mgmt_cipher = conf->group_mgmt_cipher;
	wconf->sae_require_mfp = conf->sae_require_mfp;
	wconf->ssid_protection = conf->ssid_protection;
	wconf->ssid_len = conf->ssid.ssid_len;
	if (wconf->ssid_len > SSID_MAX_LEN)
		wconf->ssid_len = SSID_MAX_LEN;
	os_memcpy(wconf->ssid, conf->ssid.ssid, wconf->ssid_len);
#ifdef CONFIG_IEEE80211R_AP
	os_memcpy(wconf->mobility_domain, conf->mobility_domain,
		  MOBILITY_DOMAIN_ID_LEN);
	if (conf->nas_identifier &&
	    os_strlen(conf->nas_identifier) <= FT_R0KH_ID_MAX_LEN) {
		wconf->r0_key_holder_len = os_strlen(conf->nas_identifier);
		os_memcpy(wconf->r0_key_holder, conf->nas_identifier,
			  wconf->r0_key_holder_len);
	}
	os_memcpy(wconf->r1_key_holder, conf->r1_key_holder, FT_R1KH_ID_LEN);
	wconf->r0_key_lifetime = conf->r0_key_lifetime;
	wconf->r1_max_key_lifetime = conf->r1_max_key_lifetime;
	wconf->reassociation_deadline = conf->reassociation_deadline;
	wconf->rkh_pos_timeout = conf->rkh_pos_timeout;
	wconf->rkh_neg_timeout = conf->rkh_neg_timeout;
	wconf->rkh_pull_timeout = conf->rkh_pull_timeout;
	wconf->rkh_pull_retries = conf->rkh_pull_retries;
	wconf->r0kh_list = &conf->r0kh_list;
	wconf->r1kh_list = &conf->r1kh_list;
	wconf->pmk_r1_push = conf->pmk_r1_push;
	wconf->ft_over_ds = conf->ft_over_ds;
	wconf->ft_psk_generate_local = conf->ft_psk_generate_local;
#endif /* CONFIG_IEEE80211R_AP */
#ifdef CONFIG_HS20
	wconf->disable_gtk = conf->disable_dgaf;
	if (conf->osen) {
		wconf->disable_gtk = 1;
		wconf->wpa = WPA_PROTO_OSEN;
		wconf->wpa_key_mgmt = WPA_KEY_MGMT_OSEN;
		wconf->wpa_pairwise = 0;
		wconf->wpa_group = WPA_CIPHER_CCMP;
		wconf->rsn_pairwise = WPA_CIPHER_CCMP;
		wconf->rsn_preauth = 0;
		wconf->disable_pmksa_caching = 1;
		wconf->ieee80211w = 1;
	}
#endif /* CONFIG_HS20 */
#ifdef CONFIG_TESTING_OPTIONS
	wconf->corrupt_gtk_rekey_mic_probability =
		iconf->corrupt_gtk_rekey_mic_probability;
	wconf->delay_eapol_tx = iconf->delay_eapol_tx;
	if (conf->own_ie_override &&
	    wpabuf_len(conf->own_ie_override) <= MAX_OWN_IE_OVERRIDE) {
		wconf->own_ie_override_len = wpabuf_len(conf->own_ie_override);
		os_memcpy(wconf->own_ie_override,
			  wpabuf_head(conf->own_ie_override),
			  wconf->own_ie_override_len);
	}
	if (conf->rsne_override_eapol &&
	    wpabuf_len(conf->rsne_override_eapol) <= MAX_OWN_IE_OVERRIDE) {
		wconf->rsne_override_eapol_set = 1;
		wconf->rsne_override_eapol_len =
			wpabuf_len(conf->rsne_override_eapol);
		os_memcpy(wconf->rsne_override_eapol,
			  wpabuf_head(conf->rsne_override_eapol),
			  wconf->rsne_override_eapol_len);
	}
	if (conf->rsnxe_override_eapol &&
	    wpabuf_len(conf->rsnxe_override_eapol) <= MAX_OWN_IE_OVERRIDE) {
		wconf->rsnxe_override_eapol_set = 1;
		wconf->rsnxe_override_eapol_len =
			wpabuf_len(conf->rsnxe_override_eapol);
		os_memcpy(wconf->rsnxe_override_eapol,
			  wpabuf_head(conf->rsnxe_override_eapol),
			  wconf->rsnxe_override_eapol_len);
	}
	if (conf->rsne_override_ft &&
	    wpabuf_len(conf->rsne_override_ft) <= MAX_OWN_IE_OVERRIDE) {
		wconf->rsne_override_ft_set = 1;
		wconf->rsne_override_ft_len =
			wpabuf_len(conf->rsne_override_ft);
		os_memcpy(wconf->rsne_override_ft,
			  wpabuf_head(conf->rsne_override_ft),
			  wconf->rsne_override_ft_len);
	}
	if (conf->rsnxe_override_ft &&
	    wpabuf_len(conf->rsnxe_override_ft) <= MAX_OWN_IE_OVERRIDE) {
		wconf->rsnxe_override_ft_set = 1;
		wconf->rsnxe_override_ft_len =
			wpabuf_len(conf->rsnxe_override_ft);
		os_memcpy(wconf->rsnxe_override_ft,
			  wpabuf_head(conf->rsnxe_override_ft),
			  wconf->rsnxe_override_ft_len);
	}
	if (conf->gtk_rsc_override &&
	    wpabuf_len(conf->gtk_rsc_override) > 0 &&
	    wpabuf_len(conf->gtk_rsc_override) <= WPA_KEY_RSC_LEN) {
		os_memcpy(wconf->gtk_rsc_override,
			  wpabuf_head(conf->gtk_rsc_override),
			  wpabuf_len(conf->gtk_rsc_override));
		wconf->gtk_rsc_override_set = 1;
	}
	if (conf->igtk_rsc_override &&
	    wpabuf_len(conf->igtk_rsc_override) > 0 &&
	    wpabuf_len(conf->igtk_rsc_override) <= WPA_KEY_RSC_LEN) {
		os_memcpy(wconf->igtk_rsc_override,
			  wpabuf_head(conf->igtk_rsc_override),
			  wpabuf_len(conf->igtk_rsc_override));
		wconf->igtk_rsc_override_set = 1;
	}
	wconf->ft_rsnxe_used = conf->ft_rsnxe_used;
	wconf->oci_freq_override_eapol_m3 = conf->oci_freq_override_eapol_m3;
	wconf->oci_freq_override_eapol_g1 = conf->oci_freq_override_eapol_g1;
	wconf->oci_freq_override_ft_assoc = conf->oci_freq_override_ft_assoc;
	wconf->oci_freq_override_fils_assoc =
		conf->oci_freq_override_fils_assoc;

	if (conf->eapol_m1_elements)
		wconf->eapol_m1_elements = wpabuf_dup(conf->eapol_m1_elements);
	if (conf->eapol_m3_elements)
		wconf->eapol_m3_elements = wpabuf_dup(conf->eapol_m3_elements);
	wconf->eapol_m3_no_encrypt = conf->eapol_m3_no_encrypt;
#endif /* CONFIG_TESTING_OPTIONS */
#ifdef CONFIG_P2P
	os_memcpy(wconf->ip_addr_go, conf->ip_addr_go, 4);
	os_memcpy(wconf->ip_addr_mask, conf->ip_addr_mask, 4);
	os_memcpy(wconf->ip_addr_start, conf->ip_addr_start, 4);
	os_memcpy(wconf->ip_addr_end, conf->ip_addr_end, 4);
#endif /* CONFIG_P2P */
#ifdef CONFIG_FILS
	wconf->fils_cache_id_set = conf->fils_cache_id_set;
	os_memcpy(wconf->fils_cache_id, conf->fils_cache_id,
		  FILS_CACHE_ID_LEN);
#endif /* CONFIG_FILS */
	wconf->sae_pwe = conf->sae_pwe;
	sae_pw_id = hostapd_sae_pw_id_in_use(conf);
	if (sae_pw_id == 2 && wconf->sae_pwe != SAE_PWE_FORCE_HUNT_AND_PECK)
		wconf->sae_pwe = SAE_PWE_HASH_TO_ELEMENT;
	else if (sae_pw_id == 1 && wconf->sae_pwe == SAE_PWE_HUNT_AND_PECK)
		wconf->sae_pwe = SAE_PWE_BOTH;
#ifdef CONFIG_SAE_PK
	wconf->sae_pk = hostapd_sae_pk_in_use(conf);
#endif /* CONFIG_SAE_PK */
#ifdef CONFIG_OWE
	wconf->owe_ptk_workaround = conf->owe_ptk_workaround;
#endif /* CONFIG_OWE */
	wconf->transition_disable = conf->transition_disable;
#ifdef CONFIG_DPP2
	wconf->dpp_pfs = conf->dpp_pfs;
#endif /* CONFIG_DPP2 */
#ifdef CONFIG_PASN
#ifdef CONFIG_TESTING_OPTIONS
	wconf->force_kdk_derivation = conf->force_kdk_derivation;
#endif /* CONFIG_TESTING_OPTIONS */
#endif /* CONFIG_PASN */

	wconf->radius_psk = conf->wpa_psk_radius == PSK_RADIUS_DURING_4WAY_HS;
	wconf->no_disconnect_on_group_keyerror =
		conf->bss_max_idle && conf->ap_max_inactivity &&
		conf->no_disconnect_on_group_keyerror;
}


static void hostapd_wpa_auth_logger(void *ctx, const u8 *addr,
				    logger_level level, const char *txt)
{
#ifndef CONFIG_NO_HOSTAPD_LOGGER
	struct hostapd_data *hapd = ctx;
	int hlevel;

	switch (level) {
	case LOGGER_WARNING:
		hlevel = HOSTAPD_LEVEL_WARNING;
		break;
	case LOGGER_INFO:
		hlevel = HOSTAPD_LEVEL_INFO;
		break;
	case LOGGER_DEBUG:
	default:
		hlevel = HOSTAPD_LEVEL_DEBUG;
		break;
	}

	hostapd_logger(hapd, addr, HOSTAPD_MODULE_WPA, hlevel, "%s", txt);
#endif /* CONFIG_NO_HOSTAPD_LOGGER */
}


static void hostapd_wpa_auth_disconnect(void *ctx, const u8 *addr,
					u16 reason)
{
	struct hostapd_data *hapd = ctx;
	wpa_printf(MSG_DEBUG, "%s: WPA authenticator requests disconnect: "
		   "STA " MACSTR " reason %d",
		   __func__, MAC2STR(addr), reason);
	ap_sta_disconnect(hapd, NULL, addr, reason);
}


static int hostapd_wpa_auth_mic_failure_report(void *ctx, const u8 *addr)
{
	struct hostapd_data *hapd = ctx;
	return michael_mic_failure(hapd, addr, 0);
}


static void hostapd_wpa_auth_psk_failure_report(void *ctx, const u8 *addr)
{
	struct hostapd_data *hapd = ctx;
	wpa_msg(hapd->msg_ctx, MSG_INFO, AP_STA_POSSIBLE_PSK_MISMATCH MACSTR,
		MAC2STR(addr));
}


static void hostapd_wpa_auth_set_eapol(void *ctx, const u8 *addr,
				       wpa_eapol_variable var, int value)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta = ap_get_sta(hapd, addr);
	if (sta == NULL)
		return;
	switch (var) {
	case WPA_EAPOL_portEnabled:
		ieee802_1x_notify_port_enabled(sta->eapol_sm, value);
		break;
	case WPA_EAPOL_portValid:
		ieee802_1x_notify_port_valid(sta->eapol_sm, value);
		break;
	case WPA_EAPOL_authorized:
		ieee802_1x_set_sta_authorized(hapd, sta, value);
		break;
	case WPA_EAPOL_portControl_Auto:
		if (sta->eapol_sm)
			sta->eapol_sm->portControl = Auto;
		break;
	case WPA_EAPOL_keyRun:
		if (sta->eapol_sm)
			sta->eapol_sm->keyRun = value;
		break;
	case WPA_EAPOL_keyAvailable:
		if (sta->eapol_sm)
			sta->eapol_sm->eap_if->eapKeyAvailable = value;
		break;
	case WPA_EAPOL_keyDone:
		if (sta->eapol_sm)
			sta->eapol_sm->keyDone = value;
		break;
	case WPA_EAPOL_inc_EapolFramesTx:
		if (sta->eapol_sm)
			sta->eapol_sm->dot1xAuthEapolFramesTx++;
		break;
	}
}


static int hostapd_wpa_auth_get_eapol(void *ctx, const u8 *addr,
				      wpa_eapol_variable var)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta = ap_get_sta(hapd, addr);
	if (sta == NULL || sta->eapol_sm == NULL)
		return -1;
	switch (var) {
	case WPA_EAPOL_keyRun:
		return sta->eapol_sm->keyRun;
	case WPA_EAPOL_keyAvailable:
		return sta->eapol_sm->eap_if->eapKeyAvailable;
	default:
		return -1;
	}
}


static const u8 * hostapd_wpa_auth_get_psk(void *ctx, const u8 *addr,
					   const u8 *p2p_dev_addr,
					   const u8 *prev_psk, size_t *psk_len,
					   int *vlan_id)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta = ap_get_sta(hapd, addr);
	const u8 *psk;

	if (vlan_id)
		*vlan_id = 0;
	if (psk_len)
		*psk_len = PMK_LEN;

#ifdef CONFIG_SAE
	if (sta && sta->auth_alg == WLAN_AUTH_SAE) {
		if (!sta->sae || prev_psk)
			return NULL;
		if (psk_len)
			*psk_len = sta->sae->pmk_len;
		return sta->sae->pmk;
	}
	if (sta && wpa_auth_uses_sae(sta->wpa_sm)) {
		wpa_printf(MSG_DEBUG,
			   "No PSK for STA trying to use SAE with PMKSA caching");
		return NULL;
	}
#endif /* CONFIG_SAE */

#ifdef CONFIG_OWE
	if ((hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_OWE) &&
	    sta && sta->owe_pmk) {
		if (psk_len)
			*psk_len = sta->owe_pmk_len;
		return sta->owe_pmk;
	}
	if ((hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_OWE) && sta) {
		struct rsn_pmksa_cache_entry *sa;

		sa = wpa_auth_sta_get_pmksa(sta->wpa_sm);
		if (sa && sa->akmp == WPA_KEY_MGMT_OWE) {
			if (psk_len)
				*psk_len = sa->pmk_len;
			return sa->pmk;
		}
	}
#endif /* CONFIG_OWE */

	psk = hostapd_get_psk(hapd->conf, addr, p2p_dev_addr, prev_psk,
			      vlan_id);
	/*
	 * This is about to iterate over all psks, prev_psk gives the last
	 * returned psk which should not be returned again.
	 * logic list (all hostapd_get_psk; all sta->psk)
	 */
	if (sta && sta->psk && !psk) {
		struct hostapd_sta_wpa_psk_short *pos;

		if (vlan_id)
			*vlan_id = 0;
		psk = sta->psk->psk;
		for (pos = sta->psk; pos; pos = pos->next) {
			if (pos->is_passphrase) {
				if (pbkdf2_sha1(pos->passphrase,
						hapd->conf->ssid.ssid,
						hapd->conf->ssid.ssid_len, 4096,
						pos->psk, PMK_LEN) != 0) {
					wpa_printf(MSG_WARNING,
						   "Error in pbkdf2_sha1()");
					continue;
				}
				pos->is_passphrase = 0;
			}
			if (pos->psk == prev_psk) {
				psk = pos->next ? pos->next->psk : NULL;
				break;
			}
		}
	}
	return psk;
}


static int hostapd_wpa_auth_get_msk(void *ctx, const u8 *addr, u8 *msk,
				    size_t *len)
{
	struct hostapd_data *hapd = ctx;
	const u8 *key;
	size_t keylen;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL) {
		wpa_printf(MSG_DEBUG, "AUTH_GET_MSK: Cannot find STA");
		return -1;
	}

	key = ieee802_1x_get_key(sta->eapol_sm, &keylen);
	if (key == NULL) {
		wpa_printf(MSG_DEBUG, "AUTH_GET_MSK: Key is null, eapol_sm: %p",
			   sta->eapol_sm);
		return -1;
	}

	if (keylen > *len)
		keylen = *len;
	os_memcpy(msk, key, keylen);
	*len = keylen;

	return 0;
}


static int hostapd_wpa_auth_set_key(void *ctx, int vlan_id, enum wpa_alg alg,
				    const u8 *addr, int idx, u8 *key,
				    size_t key_len, enum key_flag key_flag)
{
	struct hostapd_data *hapd = ctx;
	const char *ifname = hapd->conf->iface;

	if (vlan_id > 0) {
		ifname = hostapd_get_vlan_id_ifname(hapd->conf->vlan, vlan_id);
		if (!ifname) {
			if (!(hapd->iface->drv_flags &
			      WPA_DRIVER_FLAGS_VLAN_OFFLOAD))
				return -1;
			ifname = hapd->conf->iface;
		}
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (key_flag & KEY_FLAG_MODIFY) {
		/* We are updating an already installed key. Don't overwrite
		 * the already stored key information with zeros.
		 */
	} else if (addr && !is_broadcast_ether_addr(addr)) {
		struct sta_info *sta;

		sta = ap_get_sta(hapd, addr);
		if (sta) {
			sta->last_tk_alg = alg;
			sta->last_tk_key_idx = idx;
			if (key)
				os_memcpy(sta->last_tk, key, key_len);
			sta->last_tk_len = key_len;
		}
	} else if (alg == WPA_ALG_BIP_CMAC_128 ||
		   alg == WPA_ALG_BIP_GMAC_128 ||
		   alg == WPA_ALG_BIP_GMAC_256 ||
		   alg == WPA_ALG_BIP_CMAC_256) {
		if (idx == 4 || idx == 5) {
			hapd->last_igtk_alg = alg;
			hapd->last_igtk_key_idx = idx;
			if (key)
				os_memcpy(hapd->last_igtk, key, key_len);
			hapd->last_igtk_len = key_len;
		} else if (idx == 6 || idx == 7) {
			hapd->last_bigtk_alg = alg;
			hapd->last_bigtk_key_idx = idx;
			if (key)
				os_memcpy(hapd->last_bigtk, key, key_len);
			hapd->last_bigtk_len = key_len;
		}
	} else {
		hapd->last_gtk_alg = alg;
		hapd->last_gtk_key_idx = idx;
		if (key)
			os_memcpy(hapd->last_gtk, key, key_len);
		hapd->last_gtk_len = key_len;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	return hostapd_drv_set_key(ifname, hapd, alg, addr, idx, vlan_id, 1,
				   NULL, 0, key, key_len, key_flag);
}


static int hostapd_wpa_auth_get_seqnum(void *ctx, const u8 *addr, int idx,
				       u8 *seq)
{
	struct hostapd_data *hapd = ctx;
	int link_id = -1;

#ifdef CONFIG_IEEE80211BE
	if (hapd->conf->mld_ap && idx)
		link_id = hapd->mld_link_id;
#endif /* CONFIG_IEEE80211BE */
	return hostapd_get_seqnum(hapd->conf->iface, hapd, addr, idx, link_id,
				  seq);
}


int hostapd_wpa_auth_send_eapol(void *ctx, const u8 *addr,
				const u8 *data, size_t data_len,
				int encrypt)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;
	u32 flags = 0;
	int link_id = -1;

#ifdef CONFIG_IEEE80211BE
	link_id = hapd->conf->mld_ap ? hapd->mld_link_id : -1;
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->ext_eapol_frame_io) {
		size_t hex_len = 2 * data_len + 1;
		char *hex = os_malloc(hex_len);

		if (hex == NULL)
			return -1;
		wpa_snprintf_hex(hex, hex_len, data, data_len);
		wpa_msg(hapd->msg_ctx, MSG_INFO, "EAPOL-TX " MACSTR " %s",
			MAC2STR(addr), hex);
		os_free(hex);
		return 0;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	sta = ap_get_sta(hapd, addr);
	if (sta) {
		flags = hostapd_sta_flags_to_drv(sta->flags);
#ifdef CONFIG_IEEE80211BE
		if (ap_sta_is_mld(hapd, sta) &&
		    (sta->flags & WLAN_STA_AUTHORIZED))
			link_id = -1;
#endif /* CONFIG_IEEE80211BE */
	}

	return hostapd_drv_hapd_send_eapol(hapd, addr, data, data_len,
					   encrypt, flags, link_id);
}


static int hostapd_wpa_auth_get_sta_count(void *ctx)
{
	struct hostapd_data *hapd = ctx;

	return hapd->num_sta;
}


static int hostapd_wpa_auth_for_each_sta(
	void *ctx, int (*cb)(struct wpa_state_machine *sm, void *ctx),
	void *cb_ctx)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
		if (sta->wpa_sm && cb(sta->wpa_sm, cb_ctx))
			return 1;
	}
	return 0;
}


struct wpa_auth_iface_iter_data {
	int (*cb)(struct wpa_authenticator *sm, void *ctx);
	void *cb_ctx;
};

static int wpa_auth_iface_iter(struct hostapd_iface *iface, void *ctx)
{
	struct wpa_auth_iface_iter_data *data = ctx;
	size_t i;
	for (i = 0; i < iface->num_bss; i++) {
		if (iface->bss[i]->wpa_auth &&
		    data->cb(iface->bss[i]->wpa_auth, data->cb_ctx))
			return 1;
	}
	return 0;
}


static int hostapd_wpa_auth_for_each_auth(
	void *ctx, int (*cb)(struct wpa_authenticator *sm, void *ctx),
	void *cb_ctx)
{
	struct hostapd_data *hapd = ctx;
	struct wpa_auth_iface_iter_data data;
	if (hapd->iface->interfaces == NULL ||
	    hapd->iface->interfaces->for_each_interface == NULL)
		return -1;
	data.cb = cb;
	data.cb_ctx = cb_ctx;
	return hapd->iface->interfaces->for_each_interface(
		hapd->iface->interfaces, wpa_auth_iface_iter, &data);
}


#ifdef CONFIG_IEEE80211R_AP

struct wpa_ft_rrb_rx_later_data {
	struct dl_list list;
	u8 addr[ETH_ALEN];
	size_t data_len;
	/* followed by data_len octets of data */
};

static void hostapd_wpa_ft_rrb_rx_later(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct wpa_ft_rrb_rx_later_data *data, *n;

	dl_list_for_each_safe(data, n, &hapd->l2_queue,
			      struct wpa_ft_rrb_rx_later_data, list) {
		if (hapd->wpa_auth) {
			wpa_ft_rrb_rx(hapd->wpa_auth, data->addr,
				      (const u8 *) (data + 1),
				      data->data_len);
		}
		dl_list_del(&data->list);
		os_free(data);
	}
}


struct wpa_auth_ft_iface_iter_data {
	struct hostapd_data *src_hapd;
	const u8 *dst;
	const u8 *data;
	size_t data_len;
};


static int hostapd_wpa_auth_ft_iter(struct hostapd_iface *iface, void *ctx)
{
	struct wpa_auth_ft_iface_iter_data *idata = ctx;
	struct wpa_ft_rrb_rx_later_data *data;
	struct hostapd_data *hapd;
	size_t j;

	for (j = 0; j < iface->num_bss; j++) {
		hapd = iface->bss[j];
		if (hapd == idata->src_hapd ||
		    !hapd->wpa_auth ||
		    !ether_addr_equal(hapd->own_addr, idata->dst))
			continue;

		wpa_printf(MSG_DEBUG,
			   "FT: Send RRB data directly to locally managed BSS "
			   MACSTR "@%s -> " MACSTR "@%s",
			   MAC2STR(idata->src_hapd->own_addr),
			   idata->src_hapd->conf->iface,
			   MAC2STR(hapd->own_addr), hapd->conf->iface);

		/* Defer wpa_ft_rrb_rx() until next eloop step as this is
		 * when it would be triggered when reading from a socket.
		 * This avoids
		 * hapd0:send -> hapd1:recv -> hapd1:send -> hapd0:recv,
		 * that is calling hapd0:recv handler from within
		 * hapd0:send directly.
		 */
		data = os_zalloc(sizeof(*data) + idata->data_len);
		if (!data)
			return 1;

		os_memcpy(data->addr, idata->src_hapd->own_addr, ETH_ALEN);
		os_memcpy(data + 1, idata->data, idata->data_len);
		data->data_len = idata->data_len;

		dl_list_add(&hapd->l2_queue, &data->list);

		if (!eloop_is_timeout_registered(hostapd_wpa_ft_rrb_rx_later,
						 hapd, NULL))
			eloop_register_timeout(0, 0,
					       hostapd_wpa_ft_rrb_rx_later,
					       hapd, NULL);

		return 1;
	}

	return 0;
}

#endif /* CONFIG_IEEE80211R_AP */


static int hostapd_wpa_auth_send_ether(void *ctx, const u8 *dst, u16 proto,
				       const u8 *data, size_t data_len)
{
	struct hostapd_data *hapd = ctx;
	struct l2_ethhdr *buf;
	int ret;

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->ext_eapol_frame_io && proto == ETH_P_EAPOL) {
		size_t hex_len = 2 * data_len + 1;
		char *hex = os_malloc(hex_len);

		if (hex == NULL)
			return -1;
		wpa_snprintf_hex(hex, hex_len, data, data_len);
		wpa_msg(hapd->msg_ctx, MSG_INFO, "EAPOL-TX " MACSTR " %s",
			MAC2STR(dst), hex);
		os_free(hex);
		return 0;
	}
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_IEEE80211R_AP
	if (proto == ETH_P_RRB && hapd->iface->interfaces &&
	    hapd->iface->interfaces->for_each_interface) {
		int res;
		struct wpa_auth_ft_iface_iter_data idata;
		idata.src_hapd = hapd;
		idata.dst = dst;
		idata.data = data;
		idata.data_len = data_len;
		res = hapd->iface->interfaces->for_each_interface(
			hapd->iface->interfaces, hostapd_wpa_auth_ft_iter,
			&idata);
		if (res == 1)
			return data_len;
	}
#endif /* CONFIG_IEEE80211R_AP */

	if (hapd->l2 == NULL)
		return -1;

	buf = os_malloc(sizeof(*buf) + data_len);
	if (buf == NULL)
		return -1;
	os_memcpy(buf->h_dest, dst, ETH_ALEN);
	os_memcpy(buf->h_source, hapd->own_addr, ETH_ALEN);
	buf->h_proto = host_to_be16(proto);
	os_memcpy(buf + 1, data, data_len);
	ret = l2_packet_send(hapd->l2, dst, proto, (u8 *) buf,
			     sizeof(*buf) + data_len);
	os_free(buf);
	return ret;
}


#ifdef CONFIG_ETH_P_OUI
static struct eth_p_oui_ctx * hostapd_wpa_get_oui(struct hostapd_data *hapd,
						  u8 oui_suffix)
{
	switch (oui_suffix) {
#ifdef CONFIG_IEEE80211R_AP
	case FT_PACKET_R0KH_R1KH_PULL:
		return hapd->oui_pull;
	case FT_PACKET_R0KH_R1KH_RESP:
		return hapd->oui_resp;
	case FT_PACKET_R0KH_R1KH_PUSH:
		return hapd->oui_push;
	case FT_PACKET_R0KH_R1KH_SEQ_REQ:
		return hapd->oui_sreq;
	case FT_PACKET_R0KH_R1KH_SEQ_RESP:
		return hapd->oui_sresp;
#endif /* CONFIG_IEEE80211R_AP */
	default:
		return NULL;
	}
}
#endif /* CONFIG_ETH_P_OUI */


#ifdef CONFIG_IEEE80211R_AP

struct oui_deliver_later_data {
	struct dl_list list;
	u8 src_addr[ETH_ALEN];
	u8 dst_addr[ETH_ALEN];
	size_t data_len;
	u8 oui_suffix;
	/* followed by data_len octets of data */
};

static void hostapd_oui_deliver_later(void *eloop_ctx, void *timeout_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct oui_deliver_later_data *data, *n;
	struct eth_p_oui_ctx *oui_ctx;

	dl_list_for_each_safe(data, n, &hapd->l2_oui_queue,
			      struct oui_deliver_later_data, list) {
		oui_ctx = hostapd_wpa_get_oui(hapd, data->oui_suffix);
		wpa_printf(MSG_DEBUG, "RRB(%s): %s src=" MACSTR " dst=" MACSTR
			   " oui_suffix=%u data_len=%u data=%p",
			   hapd->conf->iface, __func__,
			   MAC2STR(data->src_addr), MAC2STR(data->dst_addr),
			   data->oui_suffix, (unsigned int) data->data_len,
			   data);
		if (hapd->wpa_auth && oui_ctx) {
			eth_p_oui_deliver(oui_ctx, data->src_addr,
					  data->dst_addr,
					  (const u8 *) (data + 1),
					  data->data_len);
		}
		dl_list_del(&data->list);
		os_free(data);
	}
}


struct wpa_auth_oui_iface_iter_data {
	struct hostapd_data *src_hapd;
	const u8 *dst_addr;
	const u8 *data;
	size_t data_len;
	u8 oui_suffix;
};

static int hostapd_wpa_auth_oui_iter(struct hostapd_iface *iface, void *ctx)
{
	struct wpa_auth_oui_iface_iter_data *idata = ctx;
	struct oui_deliver_later_data *data;
	struct hostapd_data *hapd, *src_hapd = idata->src_hapd;
	size_t j;

	for (j = 0; j < iface->num_bss; j++) {
		hapd = iface->bss[j];
		if (hapd == src_hapd)
			continue; /* don't deliver back to same interface */
		if (!wpa_key_mgmt_ft(hapd->conf->wpa_key_mgmt) ||
		    hapd->conf->ssid.ssid_len !=
		    src_hapd->conf->ssid.ssid_len ||
		    os_memcmp(hapd->conf->ssid.ssid,
			      src_hapd->conf->ssid.ssid,
			      hapd->conf->ssid.ssid_len) != 0 ||
		    os_memcmp(hapd->conf->mobility_domain,
			      src_hapd->conf->mobility_domain,
			      MOBILITY_DOMAIN_ID_LEN) != 0)
			continue; /* no matching FT SSID/mobility domain */
		if (!is_multicast_ether_addr(idata->dst_addr) &&
		    !ether_addr_equal(hapd->own_addr, idata->dst_addr))
			continue; /* destination address does not match */

		/* defer eth_p_oui_deliver until next eloop step as this is
		 * when it would be triggerd from reading from sock
		 * This avoids
		 * hapd0:send -> hapd1:recv -> hapd1:send -> hapd0:recv,
		 * that is calling hapd0:recv handler from within
		 * hapd0:send directly.
		 */
		data = os_zalloc(sizeof(*data) + idata->data_len);
		if (!data)
			return 1;
		wpa_printf(MSG_DEBUG,
			   "RRB(%s): local delivery to %s dst=" MACSTR
			   " oui_suffix=%u data_len=%u data=%p",
			   src_hapd->conf->iface, hapd->conf->iface,
			   MAC2STR(idata->dst_addr), idata->oui_suffix,
			   (unsigned int) idata->data_len, data);

		os_memcpy(data->src_addr, src_hapd->own_addr, ETH_ALEN);
		os_memcpy(data->dst_addr, idata->dst_addr, ETH_ALEN);
		os_memcpy(data + 1, idata->data, idata->data_len);
		data->data_len = idata->data_len;
		data->oui_suffix = idata->oui_suffix;

		dl_list_add_tail(&hapd->l2_oui_queue, &data->list);

		if (!eloop_is_timeout_registered(hostapd_oui_deliver_later,
						 hapd, NULL))
			eloop_register_timeout(0, 0,
					       hostapd_oui_deliver_later,
					       hapd, NULL);

		/* If dst_addr is a multicast address, do not return any
		 * non-zero value here. Otherwise, the iteration of
		 * for_each_interface() will be stopped. */
		if (!is_multicast_ether_addr(idata->dst_addr))
			return 1;
	}

	return 0;
}

#endif /* CONFIG_IEEE80211R_AP */


static int hostapd_wpa_auth_send_oui(void *ctx, const u8 *dst, u8 oui_suffix,
				     const u8 *data, size_t data_len)
{
#ifdef CONFIG_ETH_P_OUI
	struct hostapd_data *hapd = ctx;
	struct eth_p_oui_ctx *oui_ctx;

	wpa_printf(MSG_DEBUG, "RRB(%s): send to dst=" MACSTR
		   " oui_suffix=%u data_len=%u",
		   hapd->conf->iface, MAC2STR(dst), oui_suffix,
		   (unsigned int) data_len);
#ifdef CONFIG_IEEE80211R_AP
	if (hapd->iface->interfaces &&
	    hapd->iface->interfaces->for_each_interface) {
		struct wpa_auth_oui_iface_iter_data idata;
		int res;

		idata.src_hapd = hapd;
		idata.dst_addr = dst;
		idata.data = data;
		idata.data_len = data_len;
		idata.oui_suffix = oui_suffix;
		res = hapd->iface->interfaces->for_each_interface(
			hapd->iface->interfaces, hostapd_wpa_auth_oui_iter,
			&idata);
		if (res == 1)
			return data_len;
	}
#endif /* CONFIG_IEEE80211R_AP */

	oui_ctx = hostapd_wpa_get_oui(hapd, oui_suffix);
	if (!oui_ctx)
		return -1;

	return eth_p_oui_send(oui_ctx, hapd->own_addr, dst, data, data_len);
#else /* CONFIG_ETH_P_OUI */
	return -1;
#endif /* CONFIG_ETH_P_OUI */
}


static int hostapd_channel_info(void *ctx, struct wpa_channel_info *ci)
{
	struct hostapd_data *hapd = ctx;

	return hostapd_drv_channel_info(hapd, ci);
}


#ifdef CONFIG_PASN

static void hostapd_store_ptksa(void *ctx, const u8 *addr,int cipher,
				u32 life_time, const struct wpa_ptk *ptk)
{
	struct hostapd_data *hapd = ctx;

	ptksa_cache_add(hapd->ptksa, hapd->own_addr, addr, cipher, life_time,
			ptk, NULL, NULL, 0);
}


static void hostapd_clear_ptksa(void *ctx, const u8 *addr, int cipher)
{
	struct hostapd_data *hapd = ctx;

	ptksa_cache_flush(hapd->ptksa, addr, cipher);
}

#endif /* CONFIG_PASN */


static int hostapd_wpa_auth_update_vlan(void *ctx, const u8 *addr, int vlan_id)
{
#ifndef CONFIG_NO_VLAN
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, addr);
	if (!sta)
		return -1;

	if (!(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_VLAN_OFFLOAD)) {
		struct vlan_description vlan_desc;

		os_memset(&vlan_desc, 0, sizeof(vlan_desc));
		vlan_desc.notempty = 1;
		vlan_desc.untagged = vlan_id;
		if (!hostapd_vlan_valid(hapd->conf->vlan, &vlan_desc)) {
			wpa_printf(MSG_INFO,
				   "Invalid VLAN ID %d in wpa_psk_file",
				   vlan_id);
			return -1;
		}

		if (ap_sta_set_vlan(hapd, sta, &vlan_desc) < 0) {
			wpa_printf(MSG_INFO,
				   "Failed to assign VLAN ID %d from wpa_psk_file to "
				   MACSTR, vlan_id, MAC2STR(sta->addr));
			return -1;
		}
	} else {
		sta->vlan_id = vlan_id;
	}

	wpa_printf(MSG_INFO,
		   "Assigned VLAN ID %d from wpa_psk_file to " MACSTR,
		   vlan_id, MAC2STR(sta->addr));
	if ((sta->flags & WLAN_STA_ASSOC) &&
	    ap_sta_bind_vlan(hapd, sta) < 0)
		return -1;
#endif /* CONFIG_NO_VLAN */

	return 0;
}


#ifdef CONFIG_OCV
static int hostapd_get_sta_tx_params(void *ctx, const u8 *addr,
				     int ap_max_chanwidth, int ap_seg1_idx,
				     int *bandwidth, int *seg1_idx)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, addr);
	if (!sta) {
		hostapd_wpa_auth_logger(hapd, addr, LOGGER_INFO,
					"Failed to get STA info to validate received OCI");
		return -1;
	}

	return get_tx_parameters(sta, ap_max_chanwidth, ap_seg1_idx, bandwidth,
				 seg1_idx);
}
#endif /* CONFIG_OCV */


#ifdef CONFIG_IEEE80211R_AP

static int hostapd_wpa_auth_send_ft_action(void *ctx, const u8 *dst,
					   const u8 *data, size_t data_len)
{
	struct hostapd_data *hapd = ctx;
	int res;
	struct ieee80211_mgmt *m;
	size_t mlen;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, dst);
	if (sta == NULL || sta->wpa_sm == NULL)
		return -1;

	m = os_zalloc(sizeof(*m) + data_len);
	if (m == NULL)
		return -1;
	mlen = ((u8 *) &m->u - (u8 *) m) + data_len;
	m->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					WLAN_FC_STYPE_ACTION);
	os_memcpy(m->da, dst, ETH_ALEN);
	os_memcpy(m->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(m->bssid, hapd->own_addr, ETH_ALEN);
	os_memcpy(&m->u, data, data_len);

	res = hostapd_drv_send_mlme(hapd, (u8 *) m, mlen, 0, NULL, 0, 0);
	os_free(m);
	return res;
}


static struct wpa_state_machine *
hostapd_wpa_auth_add_sta(void *ctx, const u8 *sta_addr)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;
	int ret;

	wpa_printf(MSG_DEBUG, "Add station entry for " MACSTR
		   " based on WPA authenticator callback",
		   MAC2STR(sta_addr));
	ret = hostapd_add_sta_node(hapd, sta_addr, WLAN_AUTH_FT);

	/*
	 * The expected return values from hostapd_add_sta_node() are
	 * 0: successfully added STA entry
	 * -EOPNOTSUPP: driver or driver wrapper does not support/need this
	 *	operations
	 * any other negative value: error in adding the STA entry */
	if (ret < 0 && ret != -EOPNOTSUPP)
		return NULL;

	sta = ap_sta_add(hapd, sta_addr);
	if (sta == NULL)
		return NULL;
	if (ret == 0)
		sta->added_unassoc = 1;

	sta->ft_over_ds = 1;
	if (sta->wpa_sm) {
		sta->auth_alg = WLAN_AUTH_FT;
		return sta->wpa_sm;
	}

	sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth, sta->addr, NULL);
	if (sta->wpa_sm == NULL) {
		ap_free_sta(hapd, sta);
		return NULL;
	}
	sta->auth_alg = WLAN_AUTH_FT;

	return sta->wpa_sm;
}


static int hostapd_wpa_auth_add_sta_ft(void *ctx, const u8 *sta_addr)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, sta_addr);
	if (!sta)
		return -1;

	if (FULL_AP_CLIENT_STATE_SUPP(hapd->iface->drv_flags) &&
	    (sta->flags & WLAN_STA_MFP) && ap_sta_is_authorized(sta) &&
	    !(hapd->conf->mesh & MESH_ENABLED) && !(sta->added_unassoc)) {
		/* We could not do this in handle_auth() since there was a
		 * PMF-enabled association for the STA and the new
		 * authentication attempt was not yet fully processed. Now that
		 * we are ready to configure the TK to the driver,
		 * authentication has succeeded and we can clean up the driver
		 * STA entry to avoid issues with any maintained state from the
		 * previous association. */
		wpa_printf(MSG_DEBUG,
			   "FT: Remove and re-add driver STA entry after successful FT authentication");
		return ap_sta_re_add(hapd, sta);
	}

	return 0;
}


static int hostapd_wpa_auth_set_vlan(void *ctx, const u8 *sta_addr,
				     struct vlan_description *vlan)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, sta_addr);
	if (!sta || !sta->wpa_sm)
		return -1;

	if (!(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_VLAN_OFFLOAD)) {
		if (vlan->notempty &&
		    !hostapd_vlan_valid(hapd->conf->vlan, vlan)) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_INFO,
				       "Invalid VLAN %d%s received from FT",
				       vlan->untagged, vlan->tagged[0] ?
				       "+" : "");
			return -1;
		}

		if (ap_sta_set_vlan(hapd, sta, vlan) < 0)
			return -1;

	} else {
		if (vlan->notempty)
			sta->vlan_id = vlan->untagged;
	}
	/* Configure wpa_group for GTK but ignore error due to driver not
	 * knowing this STA. */
	ap_sta_bind_vlan(hapd, sta);

	if (sta->vlan_id)
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "VLAN ID %d", sta->vlan_id);

	return 0;
}


static int hostapd_wpa_auth_get_vlan(void *ctx, const u8 *sta_addr,
				     struct vlan_description *vlan)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, sta_addr);
	if (!sta)
		return -1;

	if (sta->vlan_desc) {
		*vlan = *sta->vlan_desc;
	} else if ((hapd->iface->drv_flags & WPA_DRIVER_FLAGS_VLAN_OFFLOAD) &&
		   sta->vlan_id) {
		vlan->notempty = 1;
		vlan->untagged = sta->vlan_id;
	} else {
		os_memset(vlan, 0, sizeof(*vlan));
	}

	return 0;
}


static int
hostapd_wpa_auth_set_identity(void *ctx, const u8 *sta_addr,
			      const u8 *identity, size_t identity_len)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, sta_addr);
	if (!sta)
		return -1;

	os_free(sta->identity);
	sta->identity = NULL;

	if (sta->eapol_sm) {
		os_free(sta->eapol_sm->identity);
		sta->eapol_sm->identity = NULL;
		sta->eapol_sm->identity_len = 0;
	}

	if (!identity_len)
		return 0;

	/* sta->identity is NULL terminated */
	sta->identity = os_zalloc(identity_len + 1);
	if (!sta->identity)
		return -1;
	os_memcpy(sta->identity, identity, identity_len);

	if (sta->eapol_sm) {
		sta->eapol_sm->identity = os_zalloc(identity_len);
		if (!sta->eapol_sm->identity)
			return -1;
		os_memcpy(sta->eapol_sm->identity, identity, identity_len);
		sta->eapol_sm->identity_len = identity_len;
	}

	return 0;
}


static size_t
hostapd_wpa_auth_get_identity(void *ctx, const u8 *sta_addr, const u8 **buf)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;
	size_t len;
	char *identity;

	sta = ap_get_sta(hapd, sta_addr);
	if (!sta)
		return 0;

	*buf = ieee802_1x_get_identity(sta->eapol_sm, &len);
	if (*buf && len)
		return len;

	if (!sta->identity) {
		*buf = NULL;
		return 0;
	}

	identity = sta->identity;
	len = os_strlen(identity);
	*buf = (u8 *) identity;

	return len;
}


static int
hostapd_wpa_auth_set_radius_cui(void *ctx, const u8 *sta_addr,
				const u8 *radius_cui, size_t radius_cui_len)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, sta_addr);
	if (!sta)
		return -1;

	os_free(sta->radius_cui);
	sta->radius_cui = NULL;

	if (sta->eapol_sm) {
		wpabuf_free(sta->eapol_sm->radius_cui);
		sta->eapol_sm->radius_cui = NULL;
	}

	if (!radius_cui)
		return 0;

	/* sta->radius_cui is NULL terminated */
	sta->radius_cui = os_zalloc(radius_cui_len + 1);
	if (!sta->radius_cui)
		return -1;
	os_memcpy(sta->radius_cui, radius_cui, radius_cui_len);

	if (sta->eapol_sm) {
		sta->eapol_sm->radius_cui = wpabuf_alloc_copy(radius_cui,
							      radius_cui_len);
		if (!sta->eapol_sm->radius_cui)
			return -1;
	}

	return 0;
}


static size_t
hostapd_wpa_auth_get_radius_cui(void *ctx, const u8 *sta_addr, const u8 **buf)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;
	struct wpabuf *b;
	size_t len;
	char *radius_cui;

	sta = ap_get_sta(hapd, sta_addr);
	if (!sta)
		return 0;

	b = ieee802_1x_get_radius_cui(sta->eapol_sm);
	if (b) {
		len = wpabuf_len(b);
		*buf = wpabuf_head(b);
		return len;
	}

	if (!sta->radius_cui) {
		*buf = NULL;
		return 0;
	}

	radius_cui = sta->radius_cui;
	len = os_strlen(radius_cui);
	*buf = (u8 *) radius_cui;

	return len;
}


static void hostapd_wpa_auth_set_session_timeout(void *ctx, const u8 *sta_addr,
						 int session_timeout)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;

	sta = ap_get_sta(hapd, sta_addr);
	if (!sta)
		return;

	if (session_timeout) {
		os_get_reltime(&sta->session_timeout);
		sta->session_timeout.sec += session_timeout;
		sta->session_timeout_set = 1;
		ap_sta_session_timeout(hapd, sta, session_timeout);
	} else {
		sta->session_timeout_set = 0;
		ap_sta_no_session_timeout(hapd, sta);
	}
}


static int hostapd_wpa_auth_get_session_timeout(void *ctx, const u8 *sta_addr)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;
	struct os_reltime now, remaining;

	sta = ap_get_sta(hapd, sta_addr);
	if (!sta || !sta->session_timeout_set)
		return 0;

	os_get_reltime(&now);
	if (os_reltime_before(&sta->session_timeout, &now)) {
		/* already expired, return >0 as timeout was set */
		return 1;
	}

	os_reltime_sub(&sta->session_timeout, &now, &remaining);

	return (remaining.sec > 0) ? remaining.sec : 1;
}


static void hostapd_rrb_receive(void *ctx, const u8 *src_addr, const u8 *buf,
				size_t len)
{
	struct hostapd_data *hapd = ctx;
	struct l2_ethhdr *ethhdr;
	if (len < sizeof(*ethhdr))
		return;
	ethhdr = (struct l2_ethhdr *) buf;
	wpa_printf(MSG_DEBUG, "FT: RRB received packet " MACSTR " -> "
		   MACSTR, MAC2STR(ethhdr->h_source), MAC2STR(ethhdr->h_dest));
	if (!is_multicast_ether_addr(ethhdr->h_dest) &&
	    !ether_addr_equal(hapd->own_addr, ethhdr->h_dest))
		return;
	wpa_ft_rrb_rx(hapd->wpa_auth, ethhdr->h_source, buf + sizeof(*ethhdr),
		      len - sizeof(*ethhdr));
}


static void hostapd_rrb_oui_receive(void *ctx, const u8 *src_addr,
				    const u8 *dst_addr, u8 oui_suffix,
				    const u8 *buf, size_t len)
{
	struct hostapd_data *hapd = ctx;

	wpa_printf(MSG_DEBUG, "FT: RRB received packet " MACSTR " -> "
		   MACSTR, MAC2STR(src_addr), MAC2STR(dst_addr));
	if (!is_multicast_ether_addr(dst_addr) &&
	    !ether_addr_equal(hapd->own_addr, dst_addr))
		return;
	wpa_ft_rrb_oui_rx(hapd->wpa_auth, src_addr, dst_addr, oui_suffix, buf,
			  len);
}


static int hostapd_wpa_auth_add_tspec(void *ctx, const u8 *sta_addr,
				      u8 *tspec_ie, size_t tspec_ielen)
{
	struct hostapd_data *hapd = ctx;
	return hostapd_add_tspec(hapd, sta_addr, tspec_ie, tspec_ielen);
}



static int hostapd_wpa_register_ft_oui(struct hostapd_data *hapd,
				       const char *ft_iface)
{
	hapd->oui_pull = eth_p_oui_register(hapd, ft_iface,
					    FT_PACKET_R0KH_R1KH_PULL,
					    hostapd_rrb_oui_receive, hapd);
	if (!hapd->oui_pull)
		return -1;

	hapd->oui_resp = eth_p_oui_register(hapd, ft_iface,
					    FT_PACKET_R0KH_R1KH_RESP,
					    hostapd_rrb_oui_receive, hapd);
	if (!hapd->oui_resp)
		return -1;

	hapd->oui_push = eth_p_oui_register(hapd, ft_iface,
					    FT_PACKET_R0KH_R1KH_PUSH,
					    hostapd_rrb_oui_receive, hapd);
	if (!hapd->oui_push)
		return -1;

	hapd->oui_sreq = eth_p_oui_register(hapd, ft_iface,
					    FT_PACKET_R0KH_R1KH_SEQ_REQ,
					    hostapd_rrb_oui_receive, hapd);
	if (!hapd->oui_sreq)
		return -1;

	hapd->oui_sresp = eth_p_oui_register(hapd, ft_iface,
					     FT_PACKET_R0KH_R1KH_SEQ_RESP,
					     hostapd_rrb_oui_receive, hapd);
	if (!hapd->oui_sresp)
		return -1;

	return 0;
}


static void hostapd_wpa_unregister_ft_oui(struct hostapd_data *hapd)
{
	eth_p_oui_unregister(hapd->oui_pull);
	hapd->oui_pull = NULL;
	eth_p_oui_unregister(hapd->oui_resp);
	hapd->oui_resp = NULL;
	eth_p_oui_unregister(hapd->oui_push);
	hapd->oui_push = NULL;
	eth_p_oui_unregister(hapd->oui_sreq);
	hapd->oui_sreq = NULL;
	eth_p_oui_unregister(hapd->oui_sresp);
	hapd->oui_sresp = NULL;
}
#endif /* CONFIG_IEEE80211R_AP */


#ifndef CONFIG_NO_RADIUS
static void hostapd_request_radius_psk(void *ctx, const u8 *addr, int key_mgmt,
				       const u8 *anonce,
				       const u8 *eapol, size_t eapol_len)
{
	struct hostapd_data *hapd = ctx;

	wpa_printf(MSG_DEBUG, "RADIUS PSK request for " MACSTR " key_mgmt=0x%x",
		   MAC2STR(addr), key_mgmt);
	wpa_hexdump(MSG_DEBUG, "ANonce", anonce, WPA_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "EAPOL", eapol, eapol_len);
	hostapd_acl_req_radius_psk(hapd, addr, key_mgmt, anonce, eapol,
				   eapol_len);
}
#endif /* CONFIG_NO_RADIUS */


#ifdef CONFIG_PASN
static int hostapd_set_ltf_keyseed(void *ctx, const u8 *peer_addr,
				   const u8 *ltf_keyseed,
				   size_t ltf_keyseed_len)
{
	struct hostapd_data *hapd = ctx;

	return hostapd_drv_set_secure_ranging_ctx(hapd, hapd->own_addr,
						  peer_addr, 0, 0, NULL,
						  ltf_keyseed_len,
						  ltf_keyseed, 0);
}
#endif /* CONFIG_PASN */


#ifdef CONFIG_IEEE80211BE

static int hostapd_wpa_auth_get_ml_key_info(void *ctx,
					    struct wpa_auth_ml_key_info *info)
{
	struct hostapd_data *hapd = ctx;
	unsigned int i;

	wpa_printf(MSG_DEBUG, "WPA_AUTH: MLD: Get key info CB: n_mld_links=%u",
		   info->n_mld_links);

	if (!hapd->conf->mld_ap || !hapd->iface || !hapd->iface->interfaces)
		return -1;

	for (i = 0; i < info->n_mld_links; i++) {
		struct hostapd_data *bss;
		u8 link_id = info->links[i].link_id;
		bool link_bss_found = false;

		wpa_printf(MSG_DEBUG,
			   "WPA_AUTH: MLD: Get link info CB: link_id=%u",
			   link_id);

		if (hapd->mld_link_id == link_id) {
			wpa_auth_ml_get_key_info(hapd->wpa_auth,
						 &info->links[i],
						 info->mgmt_frame_prot,
						 info->beacon_prot);
			continue;
		}

		for_each_mld_link(bss, hapd) {
			if (bss == hapd || bss->mld_link_id != link_id)
				continue;

			wpa_auth_ml_get_key_info(bss->wpa_auth,
						 &info->links[i],
						 info->mgmt_frame_prot,
						 info->beacon_prot);
			link_bss_found = true;
			break;
		}

		if (!link_bss_found)
			wpa_printf(MSG_DEBUG,
				   "WPA_AUTH: MLD: link=%u not found", link_id);
	}

	return 0;
}

#endif /* CONFIG_IEEE80211BE */


static int hostapd_wpa_auth_get_drv_flags(void *ctx,
					  u64 *drv_flags, u64 *drv_flags2)
{
	struct hostapd_data *hapd = ctx;

	if (drv_flags)
		*drv_flags = hapd->iface->drv_flags;
	if (drv_flags2)
		*drv_flags2 = hapd->iface->drv_flags2;

	return 0;
}


int hostapd_setup_wpa(struct hostapd_data *hapd)
{
	struct wpa_auth_config _conf;
	static const struct wpa_auth_callbacks cb = {
		.logger = hostapd_wpa_auth_logger,
		.disconnect = hostapd_wpa_auth_disconnect,
		.mic_failure_report = hostapd_wpa_auth_mic_failure_report,
		.psk_failure_report = hostapd_wpa_auth_psk_failure_report,
		.set_eapol = hostapd_wpa_auth_set_eapol,
		.get_eapol = hostapd_wpa_auth_get_eapol,
		.get_psk = hostapd_wpa_auth_get_psk,
		.get_msk = hostapd_wpa_auth_get_msk,
		.set_key = hostapd_wpa_auth_set_key,
		.get_seqnum = hostapd_wpa_auth_get_seqnum,
		.send_eapol = hostapd_wpa_auth_send_eapol,
		.get_sta_count = hostapd_wpa_auth_get_sta_count,
		.for_each_sta = hostapd_wpa_auth_for_each_sta,
		.for_each_auth = hostapd_wpa_auth_for_each_auth,
		.send_ether = hostapd_wpa_auth_send_ether,
		.send_oui = hostapd_wpa_auth_send_oui,
		.channel_info = hostapd_channel_info,
		.update_vlan = hostapd_wpa_auth_update_vlan,
#ifdef CONFIG_PASN
		.store_ptksa = hostapd_store_ptksa,
		.clear_ptksa = hostapd_clear_ptksa,
#endif /* CONFIG_PASN */

#ifdef CONFIG_OCV
		.get_sta_tx_params = hostapd_get_sta_tx_params,
#endif /* CONFIG_OCV */
#ifdef CONFIG_IEEE80211R_AP
		.send_ft_action = hostapd_wpa_auth_send_ft_action,
		.add_sta = hostapd_wpa_auth_add_sta,
		.add_sta_ft = hostapd_wpa_auth_add_sta_ft,
		.add_tspec = hostapd_wpa_auth_add_tspec,
		.set_vlan = hostapd_wpa_auth_set_vlan,
		.get_vlan = hostapd_wpa_auth_get_vlan,
		.set_identity = hostapd_wpa_auth_set_identity,
		.get_identity = hostapd_wpa_auth_get_identity,
		.set_radius_cui = hostapd_wpa_auth_set_radius_cui,
		.get_radius_cui = hostapd_wpa_auth_get_radius_cui,
		.set_session_timeout = hostapd_wpa_auth_set_session_timeout,
		.get_session_timeout = hostapd_wpa_auth_get_session_timeout,
#endif /* CONFIG_IEEE80211R_AP */
#ifndef CONFIG_NO_RADIUS
		.request_radius_psk = hostapd_request_radius_psk,
#endif /* CONFIG_NO_RADIUS */
#ifdef CONFIG_PASN
		.set_ltf_keyseed = hostapd_set_ltf_keyseed,
#endif /* CONFIG_PASN */
#ifdef CONFIG_IEEE80211BE
		.get_ml_key_info = hostapd_wpa_auth_get_ml_key_info,
#endif /* CONFIG_IEEE80211BE */
		.get_drv_flags = hostapd_wpa_auth_get_drv_flags,
	};
	const u8 *wpa_ie;
	size_t wpa_ie_len;
	struct hostapd_data *tx_bss;

	hostapd_wpa_auth_conf(hapd->conf, hapd->iconf, &_conf);
	_conf.msg_ctx = hapd->msg_ctx;
	tx_bss = hostapd_mbssid_get_tx_bss(hapd);
	if (tx_bss != hapd)
		_conf.tx_bss_auth = tx_bss->wpa_auth;
	if (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_EAPOL_TX_STATUS)
		_conf.tx_status = 1;
	if (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_AP_MLME)
		_conf.ap_mlme = 1;

	if (!(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_WIRED) &&
	    (hapd->conf->wpa_deny_ptk0_rekey == PTK0_REKEY_ALLOW_NEVER ||
	     (hapd->conf->wpa_deny_ptk0_rekey == PTK0_REKEY_ALLOW_LOCAL_OK &&
	      !(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_SAFE_PTK0_REKEYS)))) {
		wpa_msg(hapd->msg_ctx, MSG_INFO,
			"Disable PTK0 rekey support - replaced with disconnect");
		_conf.wpa_deny_ptk0_rekey = 1;
	}

	if (_conf.extended_key_id &&
	    (hapd->iface->drv_flags & WPA_DRIVER_FLAGS_EXTENDED_KEY_ID))
		wpa_msg(hapd->msg_ctx, MSG_DEBUG, "Extended Key ID supported");
	else
		_conf.extended_key_id = 0;

	if (!(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_BEACON_PROTECTION))
		_conf.beacon_prot = 0;

#ifdef CONFIG_OCV
	if (!(hapd->iface->drv_flags2 &
	      (WPA_DRIVER_FLAGS2_AP_SME | WPA_DRIVER_FLAGS2_OCV)))
		_conf.ocv = 0;
#endif /* CONFIG_OCV */

	_conf.secure_ltf =
		!!(hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_LTF_AP);
	_conf.secure_rtt =
		!!(hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_RTT_AP);
	_conf.prot_range_neg =
		!!(hapd->iface->drv_flags2 &
		   WPA_DRIVER_FLAGS2_PROT_RANGE_NEG_AP);

#ifdef CONFIG_IEEE80211BE
	_conf.mld_addr = NULL;
	_conf.link_id = -1;
	_conf.first_link_auth = NULL;

	if (hapd->conf->mld_ap) {
		struct hostapd_data *lhapd;

		_conf.mld_addr = hapd->mld->mld_addr;
		_conf.link_id = hapd->mld_link_id;

		for_each_mld_link(lhapd, hapd) {
			if (lhapd == hapd)
				continue;

			if (lhapd->wpa_auth)
				_conf.first_link_auth = lhapd->wpa_auth;
		}
	}
#endif /* CONFIG_IEEE80211BE */

	hapd->wpa_auth = wpa_init(hapd->own_addr, &_conf, &cb, hapd);
	if (hapd->wpa_auth == NULL) {
		wpa_printf(MSG_ERROR, "WPA initialization failed.");
		return -1;
	}

	if (hostapd_set_privacy(hapd, 1)) {
		wpa_printf(MSG_ERROR, "Could not set PrivacyInvoked "
			   "for interface %s", hapd->conf->iface);
		return -1;
	}

	wpa_ie = wpa_auth_get_wpa_ie(hapd->wpa_auth, &wpa_ie_len);
	if (hostapd_set_generic_elem(hapd, wpa_ie, wpa_ie_len)) {
		wpa_printf(MSG_ERROR, "Failed to configure WPA IE for "
			   "the kernel driver.");
		return -1;
	}

	if (rsn_preauth_iface_init(hapd)) {
		wpa_printf(MSG_ERROR, "Initialization of RSN "
			   "pre-authentication failed.");
		return -1;
	}

	if (!hapd->ptksa)
		hapd->ptksa = ptksa_cache_init();
	if (!hapd->ptksa) {
		wpa_printf(MSG_ERROR, "Failed to allocate PTKSA cache");
		return -1;
	}

#ifdef CONFIG_IEEE80211R_AP
	if (!hostapd_drv_none(hapd) &&
	    wpa_key_mgmt_ft(hapd->conf->wpa_key_mgmt)) {
		const char *ft_iface;

		ft_iface = hapd->conf->bridge[0] ? hapd->conf->bridge :
			   hapd->conf->iface;
		hapd->l2 = l2_packet_init(ft_iface, NULL, ETH_P_RRB,
					  hostapd_rrb_receive, hapd, 1);
		if (!hapd->l2) {
			wpa_printf(MSG_ERROR, "Failed to open l2_packet "
				   "interface");
			return -1;
		}

		if (hostapd_wpa_register_ft_oui(hapd, ft_iface)) {
			wpa_printf(MSG_ERROR,
				   "Failed to open ETH_P_OUI interface");
			return -1;
		}
	}
#endif /* CONFIG_IEEE80211R_AP */

	return 0;

}


void hostapd_reconfig_wpa(struct hostapd_data *hapd)
{
	struct wpa_auth_config wpa_auth_conf;
	hostapd_wpa_auth_conf(hapd->conf, hapd->iconf, &wpa_auth_conf);
	wpa_reconfig(hapd->wpa_auth, &wpa_auth_conf);
}


void hostapd_deinit_wpa(struct hostapd_data *hapd)
{
	ieee80211_tkip_countermeasures_deinit(hapd);
	ptksa_cache_deinit(hapd->ptksa);
	hapd->ptksa = NULL;

	rsn_preauth_iface_deinit(hapd);
	if (hapd->wpa_auth) {
		wpa_deinit(hapd->wpa_auth);
		hapd->wpa_auth = NULL;

		if (hapd->drv_priv && hostapd_set_privacy(hapd, 0)) {
			wpa_printf(MSG_DEBUG, "Could not disable "
				   "PrivacyInvoked for interface %s",
				   hapd->conf->iface);
		}

		if (hapd->drv_priv &&
		    hostapd_set_generic_elem(hapd, (u8 *) "", 0)) {
			wpa_printf(MSG_DEBUG, "Could not remove generic "
				   "information element from interface %s",
				   hapd->conf->iface);
		}
	}
	ieee802_1x_deinit(hapd);

#ifdef CONFIG_IEEE80211R_AP
	eloop_cancel_timeout(hostapd_wpa_ft_rrb_rx_later, hapd, ELOOP_ALL_CTX);
	hostapd_wpa_ft_rrb_rx_later(hapd, NULL); /* flush without delivering */
	eloop_cancel_timeout(hostapd_oui_deliver_later, hapd, ELOOP_ALL_CTX);
	hostapd_oui_deliver_later(hapd, NULL); /* flush without delivering */
	l2_packet_deinit(hapd->l2);
	hapd->l2 = NULL;
	hostapd_wpa_unregister_ft_oui(hapd);
#endif /* CONFIG_IEEE80211R_AP */

#ifdef CONFIG_TESTING_OPTIONS
	forced_memzero(hapd->last_gtk, WPA_GTK_MAX_LEN);
	forced_memzero(hapd->last_igtk, WPA_IGTK_MAX_LEN);
	forced_memzero(hapd->last_bigtk, WPA_BIGTK_MAX_LEN);
#endif /* CONFIG_TESTING_OPTIONS */
}
