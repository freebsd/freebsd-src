/*
 * hostapd / IEEE 802.11 Management
 * Copyright (c) 2002-2017, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#ifndef CONFIG_NATIVE_WINDOWS

#include "utils/common.h"
#include "utils/eloop.h"
#include "crypto/crypto.h"
#include "crypto/sha256.h"
#include "crypto/sha384.h"
#include "crypto/sha512.h"
#include "crypto/random.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_ctrl.h"
#include "common/sae.h"
#include "common/dpp.h"
#include "common/ocv.h"
#include "common/wpa_common.h"
#include "common/wpa_ctrl.h"
#include "common/ptksa_cache.h"
#include "radius/radius.h"
#include "radius/radius_client.h"
#include "p2p/p2p.h"
#include "wps/wps.h"
#include "fst/fst.h"
#include "hostapd.h"
#include "beacon.h"
#include "ieee802_11_auth.h"
#include "sta_info.h"
#include "ieee802_1x.h"
#include "wpa_auth.h"
#include "pmksa_cache_auth.h"
#include "wmm.h"
#include "ap_list.h"
#include "accounting.h"
#include "ap_config.h"
#include "ap_mlme.h"
#include "p2p_hostapd.h"
#include "ap_drv_ops.h"
#include "wnm_ap.h"
#include "hw_features.h"
#include "ieee802_11.h"
#include "dfs.h"
#include "mbo_ap.h"
#include "rrm.h"
#include "taxonomy.h"
#include "fils_hlp.h"
#include "dpp_hostapd.h"
#include "gas_query_ap.h"
#include "comeback_token.h"
#include "nan_usd_ap.h"
#include "pasn/pasn_common.h"


#ifdef CONFIG_FILS
static struct wpabuf *
prepare_auth_resp_fils(struct hostapd_data *hapd,
		       struct sta_info *sta, u16 *resp,
		       struct rsn_pmksa_cache_entry *pmksa,
		       struct wpabuf *erp_resp,
		       const u8 *msk, size_t msk_len,
		       int *is_pub);
#endif /* CONFIG_FILS */

#ifdef CONFIG_PASN
#ifdef CONFIG_FILS

static void pasn_fils_auth_resp(struct hostapd_data *hapd,
				struct sta_info *sta, u16 status,
				struct wpabuf *erp_resp,
				const u8 *msk, size_t msk_len);

#endif /* CONFIG_FILS */
#endif /* CONFIG_PASN */

static void handle_auth(struct hostapd_data *hapd,
			const struct ieee80211_mgmt *mgmt, size_t len,
			int rssi, int from_queue);
static int add_associated_sta(struct hostapd_data *hapd,
			      struct sta_info *sta, int reassoc);


static u8 * hostapd_eid_multi_ap(struct hostapd_data *hapd, u8 *eid, size_t len)
{
	struct multi_ap_params multi_ap = { 0 };

	if (!hapd->conf->multi_ap)
		return eid;

	if (hapd->conf->multi_ap & BACKHAUL_BSS)
		multi_ap.capability |= MULTI_AP_BACKHAUL_BSS;
	if (hapd->conf->multi_ap & FRONTHAUL_BSS)
		multi_ap.capability |= MULTI_AP_FRONTHAUL_BSS;

	if (hapd->conf->multi_ap_client_disallow &
	    PROFILE1_CLIENT_ASSOC_DISALLOW)
		multi_ap.capability |=
			MULTI_AP_PROFILE1_BACKHAUL_STA_DISALLOWED;
	if (hapd->conf->multi_ap_client_disallow &
	    PROFILE2_CLIENT_ASSOC_DISALLOW)
		multi_ap.capability |=
			MULTI_AP_PROFILE2_BACKHAUL_STA_DISALLOWED;

	multi_ap.profile = hapd->conf->multi_ap_profile;
	multi_ap.vlanid = hapd->conf->multi_ap_vlanid;

	return eid + add_multi_ap_ie(eid, len, &multi_ap);
}


u8 * hostapd_eid_supp_rates(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
	int i, num, count;
	int h2e_required;

	if (hapd->iface->current_rates == NULL)
		return eid;

	*pos++ = WLAN_EID_SUPP_RATES;
	num = hapd->iface->num_rates;
	if (hapd->iconf->ieee80211n && hapd->iconf->require_ht)
		num++;
	if (hapd->iconf->ieee80211ac && hapd->iconf->require_vht)
		num++;
#ifdef CONFIG_IEEE80211AX
	if (hapd->iconf->ieee80211ax && hapd->iconf->require_he)
		num++;
#endif /* CONFIG_IEEE80211AX */
	h2e_required = (hapd->conf->sae_pwe == SAE_PWE_HASH_TO_ELEMENT ||
			hostapd_sae_pw_id_in_use(hapd->conf) == 2) &&
		hapd->conf->sae_pwe != SAE_PWE_FORCE_HUNT_AND_PECK &&
		wpa_key_mgmt_sae(hapd->conf->wpa_key_mgmt);
	if (h2e_required)
		num++;
	if (num > 8) {
		/* rest of the rates are encoded in Extended supported
		 * rates element */
		num = 8;
	}

	*pos++ = num;
	for (i = 0, count = 0; i < hapd->iface->num_rates && count < num;
	     i++) {
		count++;
		*pos = hapd->iface->current_rates[i].rate / 5;
		if (hapd->iface->current_rates[i].flags & HOSTAPD_RATE_BASIC)
			*pos |= 0x80;
		pos++;
	}

	if (hapd->iconf->ieee80211n && hapd->iconf->require_ht && count < 8) {
		count++;
		*pos++ = 0x80 | BSS_MEMBERSHIP_SELECTOR_HT_PHY;
	}

	if (hapd->iconf->ieee80211ac && hapd->iconf->require_vht && count < 8) {
		count++;
		*pos++ = 0x80 | BSS_MEMBERSHIP_SELECTOR_VHT_PHY;
	}

#ifdef CONFIG_IEEE80211AX
	if (hapd->iconf->ieee80211ax && hapd->iconf->require_he && count < 8) {
		count++;
		*pos++ = 0x80 | BSS_MEMBERSHIP_SELECTOR_HE_PHY;
	}
#endif /* CONFIG_IEEE80211AX */

	if (h2e_required && count < 8) {
		count++;
		*pos++ = 0x80 | BSS_MEMBERSHIP_SELECTOR_SAE_H2E_ONLY;
	}

	return pos;
}


u8 * hostapd_eid_ext_supp_rates(struct hostapd_data *hapd, u8 *eid)
{
	u8 *pos = eid;
	int i, num, count;
	int h2e_required;

	hapd->conf->xrates_supported = false;
	if (hapd->iface->current_rates == NULL)
		return eid;

	num = hapd->iface->num_rates;
	if (hapd->iconf->ieee80211n && hapd->iconf->require_ht)
		num++;
	if (hapd->iconf->ieee80211ac && hapd->iconf->require_vht)
		num++;
#ifdef CONFIG_IEEE80211AX
	if (hapd->iconf->ieee80211ax && hapd->iconf->require_he)
		num++;
#endif /* CONFIG_IEEE80211AX */
	h2e_required = (hapd->conf->sae_pwe == SAE_PWE_HASH_TO_ELEMENT ||
			hostapd_sae_pw_id_in_use(hapd->conf) == 2) &&
		hapd->conf->sae_pwe != SAE_PWE_FORCE_HUNT_AND_PECK &&
		wpa_key_mgmt_sae(hapd->conf->wpa_key_mgmt);
	if (h2e_required)
		num++;
	if (num <= 8)
		return eid;
	num -= 8;

	*pos++ = WLAN_EID_EXT_SUPP_RATES;
	*pos++ = num;
	for (i = 0, count = 0; i < hapd->iface->num_rates && count < num + 8;
	     i++) {
		count++;
		if (count <= 8)
			continue; /* already in SuppRates IE */
		*pos = hapd->iface->current_rates[i].rate / 5;
		if (hapd->iface->current_rates[i].flags & HOSTAPD_RATE_BASIC)
			*pos |= 0x80;
		pos++;
	}

	if (hapd->iconf->ieee80211n && hapd->iconf->require_ht) {
		count++;
		if (count > 8)
			*pos++ = 0x80 | BSS_MEMBERSHIP_SELECTOR_HT_PHY;
	}

	if (hapd->iconf->ieee80211ac && hapd->iconf->require_vht) {
		count++;
		if (count > 8)
			*pos++ = 0x80 | BSS_MEMBERSHIP_SELECTOR_VHT_PHY;
	}

#ifdef CONFIG_IEEE80211AX
	if (hapd->iconf->ieee80211ax && hapd->iconf->require_he) {
		count++;
		if (count > 8)
			*pos++ = 0x80 | BSS_MEMBERSHIP_SELECTOR_HE_PHY;
	}
#endif /* CONFIG_IEEE80211AX */

	if (h2e_required) {
		count++;
		if (count > 8)
			*pos++ = 0x80 | BSS_MEMBERSHIP_SELECTOR_SAE_H2E_ONLY;
	}

	hapd->conf->xrates_supported = true;
	return pos;
}


u8 * hostapd_eid_rm_enabled_capab(struct hostapd_data *hapd, u8 *eid,
				  size_t len)
{
	size_t i;

	for (i = 0; i < RRM_CAPABILITIES_IE_LEN; i++) {
		if (hapd->conf->radio_measurements[i])
			break;
	}

	if (i == RRM_CAPABILITIES_IE_LEN || len < 2 + RRM_CAPABILITIES_IE_LEN)
		return eid;

	*eid++ = WLAN_EID_RRM_ENABLED_CAPABILITIES;
	*eid++ = RRM_CAPABILITIES_IE_LEN;
	os_memcpy(eid, hapd->conf->radio_measurements, RRM_CAPABILITIES_IE_LEN);

	return eid + RRM_CAPABILITIES_IE_LEN;
}


u16 hostapd_own_capab_info(struct hostapd_data *hapd)
{
	int capab = WLAN_CAPABILITY_ESS;
	int privacy = 0;
	int dfs;
	int i;

	/* Check if any of configured channels require DFS */
	dfs = hostapd_is_dfs_required(hapd->iface);
	if (dfs < 0) {
		wpa_printf(MSG_WARNING, "Failed to check if DFS is required; ret=%d",
			   dfs);
		dfs = 0;
	}

	if (hapd->iface->num_sta_no_short_preamble == 0 &&
	    hapd->iconf->preamble == SHORT_PREAMBLE)
		capab |= WLAN_CAPABILITY_SHORT_PREAMBLE;

#ifdef CONFIG_WEP
	privacy = hapd->conf->ssid.wep.keys_set;

	if (hapd->conf->ieee802_1x &&
	    (hapd->conf->default_wep_key_len ||
	     hapd->conf->individual_wep_key_len))
		privacy = 1;
#endif /* CONFIG_WEP */

	if (hapd->conf->wpa)
		privacy = 1;

#ifdef CONFIG_HS20
	if (hapd->conf->osen)
		privacy = 1;
#endif /* CONFIG_HS20 */

	if (privacy)
		capab |= WLAN_CAPABILITY_PRIVACY;

	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G &&
	    hapd->iface->num_sta_no_short_slot_time == 0)
		capab |= WLAN_CAPABILITY_SHORT_SLOT_TIME;

	/*
	 * Currently, Spectrum Management capability bit is set when directly
	 * requested in configuration by spectrum_mgmt_required or when AP is
	 * running on DFS channel.
	 * TODO: Also consider driver support for TPC to set Spectrum Mgmt bit
	 */
	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211A &&
	    (hapd->iconf->spectrum_mgmt_required || dfs))
		capab |= WLAN_CAPABILITY_SPECTRUM_MGMT;

	for (i = 0; i < RRM_CAPABILITIES_IE_LEN; i++) {
		if (hapd->conf->radio_measurements[i]) {
			capab |= IEEE80211_CAP_RRM;
			break;
		}
	}

	return capab;
}


#ifdef CONFIG_WEP
#ifndef CONFIG_NO_RC4
static u16 auth_shared_key(struct hostapd_data *hapd, struct sta_info *sta,
			   u16 auth_transaction, const u8 *challenge,
			   int iswep)
{
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "authentication (shared key, transaction %d)",
		       auth_transaction);

	if (auth_transaction == 1) {
		if (!sta->challenge) {
			/* Generate a pseudo-random challenge */
			u8 key[8];

			sta->challenge = os_zalloc(WLAN_AUTH_CHALLENGE_LEN);
			if (sta->challenge == NULL)
				return WLAN_STATUS_UNSPECIFIED_FAILURE;

			if (os_get_random(key, sizeof(key)) < 0) {
				os_free(sta->challenge);
				sta->challenge = NULL;
				return WLAN_STATUS_UNSPECIFIED_FAILURE;
			}

			rc4_skip(key, sizeof(key), 0,
				 sta->challenge, WLAN_AUTH_CHALLENGE_LEN);
		}
		return 0;
	}

	if (auth_transaction != 3)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	/* Transaction 3 */
	if (!iswep || !sta->challenge || !challenge ||
	    os_memcmp_const(sta->challenge, challenge,
			    WLAN_AUTH_CHALLENGE_LEN)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "shared key authentication - invalid "
			       "challenge-response");
		return WLAN_STATUS_CHALLENGE_FAIL;
	}

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "authentication OK (shared key)");
	sta->flags |= WLAN_STA_AUTH;
	wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
	os_free(sta->challenge);
	sta->challenge = NULL;

	return 0;
}
#endif /* CONFIG_NO_RC4 */
#endif /* CONFIG_WEP */


static int send_auth_reply(struct hostapd_data *hapd, struct sta_info *sta,
			   const u8 *dst,
			   u16 auth_alg, u16 auth_transaction, u16 resp,
			   const u8 *ies, size_t ies_len, const char *dbg)
{
	struct ieee80211_mgmt *reply;
	u8 *buf;
	size_t rlen;
	int reply_res = WLAN_STATUS_UNSPECIFIED_FAILURE;
	const u8 *sa = hapd->own_addr;
	struct wpabuf *ml_resp = NULL;

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta)) {
		ml_resp = hostapd_ml_auth_resp(hapd);
		if (!ml_resp)
			return -1;
	}
#endif /* CONFIG_IEEE80211BE */

	rlen = IEEE80211_HDRLEN + sizeof(reply->u.auth) + ies_len;
	if (ml_resp)
		rlen += wpabuf_len(ml_resp);
	buf = os_zalloc(rlen);
	if (!buf) {
		wpabuf_free(ml_resp);
		return -1;
	}

	reply = (struct ieee80211_mgmt *) buf;
	reply->frame_control = IEEE80211_FC(WLAN_FC_TYPE_MGMT,
					    WLAN_FC_STYPE_AUTH);
	os_memcpy(reply->da, dst, ETH_ALEN);
	os_memcpy(reply->sa, sa, ETH_ALEN);
	os_memcpy(reply->bssid, sa, ETH_ALEN);

	reply->u.auth.auth_alg = host_to_le16(auth_alg);
	reply->u.auth.auth_transaction = host_to_le16(auth_transaction);
	reply->u.auth.status_code = host_to_le16(resp);

	if (ies && ies_len)
		os_memcpy(reply->u.auth.variable, ies, ies_len);

#ifdef CONFIG_IEEE80211BE
	if (ml_resp)
		os_memcpy(reply->u.auth.variable + ies_len,
			  wpabuf_head(ml_resp), wpabuf_len(ml_resp));

	wpabuf_free(ml_resp);
#endif /* CONFIG_IEEE80211BE */

	wpa_printf(MSG_DEBUG, "authentication reply: STA=" MACSTR
		   " auth_alg=%d auth_transaction=%d resp=%d (IE len=%lu) (dbg=%s)",
		   MAC2STR(dst), auth_alg, auth_transaction,
		   resp, (unsigned long) ies_len, dbg);
#ifdef CONFIG_TESTING_OPTIONS
#ifdef CONFIG_SAE
	if (hapd->conf->sae_confirm_immediate == 2 &&
	    auth_alg == WLAN_AUTH_SAE) {
		if (auth_transaction == 1 && sta &&
		    (resp == WLAN_STATUS_SUCCESS ||
		     resp == WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
		     resp == WLAN_STATUS_SAE_PK)) {
			wpa_printf(MSG_DEBUG,
				   "TESTING: Postpone SAE Commit transmission until Confirm is ready");
			os_free(sta->sae_postponed_commit);
			sta->sae_postponed_commit = buf;
			sta->sae_postponed_commit_len = rlen;
			return WLAN_STATUS_SUCCESS;
		}

		if (auth_transaction == 2 && sta && sta->sae_postponed_commit) {
			wpa_printf(MSG_DEBUG,
				   "TESTING: Send postponed SAE Commit first, immediately followed by SAE Confirm");
			if (hostapd_drv_send_mlme(hapd,
						  sta->sae_postponed_commit,
						  sta->sae_postponed_commit_len,
						  0, NULL, 0, 0) < 0)
				wpa_printf(MSG_INFO, "send_auth_reply: send failed");
			os_free(sta->sae_postponed_commit);
			sta->sae_postponed_commit = NULL;
			sta->sae_postponed_commit_len = 0;
		}
	}
#endif /* CONFIG_SAE */
#endif /* CONFIG_TESTING_OPTIONS */
	if (hostapd_drv_send_mlme(hapd, reply, rlen, 0, NULL, 0, 0) < 0)
		wpa_printf(MSG_INFO, "send_auth_reply: send failed");
	else
		reply_res = WLAN_STATUS_SUCCESS;

	os_free(buf);

	return reply_res;
}


#ifdef CONFIG_IEEE80211R_AP
static void handle_auth_ft_finish(void *ctx, const u8 *dst,
				  u16 auth_transaction, u16 status,
				  const u8 *ies, size_t ies_len)
{
	struct hostapd_data *hapd = ctx;
	struct sta_info *sta;
	int reply_res;

	reply_res = send_auth_reply(hapd, NULL, dst, WLAN_AUTH_FT,
				    auth_transaction, status, ies, ies_len,
				    "auth-ft-finish");

	sta = ap_get_sta(hapd, dst);
	if (sta == NULL)
		return;

	if (sta->added_unassoc && (reply_res != WLAN_STATUS_SUCCESS ||
				   status != WLAN_STATUS_SUCCESS)) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		sta->added_unassoc = 0;
		return;
	}

	if (status != WLAN_STATUS_SUCCESS)
		return;

	hostapd_logger(hapd, dst, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG, "authentication OK (FT)");
	sta->flags |= WLAN_STA_AUTH;
	mlme_authenticate_indication(hapd, sta);
}
#endif /* CONFIG_IEEE80211R_AP */


#ifdef CONFIG_SAE

static void sae_set_state(struct sta_info *sta, enum sae_state state,
			  const char *reason)
{
	wpa_printf(MSG_DEBUG, "SAE: State %s -> %s for peer " MACSTR " (%s)",
		   sae_state_txt(sta->sae->state), sae_state_txt(state),
		   MAC2STR(sta->addr), reason);
	sta->sae->state = state;
}


const char * sae_get_password(struct hostapd_data *hapd,
			      struct sta_info *sta,
			      const char *rx_id,
			      struct sae_password_entry **pw_entry,
			      struct sae_pt **s_pt,
			      const struct sae_pk **s_pk)
{
	const char *password = NULL;
	struct sae_password_entry *pw;
	struct sae_pt *pt = NULL;
	const struct sae_pk *pk = NULL;
	struct hostapd_sta_wpa_psk_short *psk = NULL;

	for (pw = hapd->conf->sae_passwords; pw; pw = pw->next) {
		if (!is_broadcast_ether_addr(pw->peer_addr) &&
		    (!sta ||
		     !ether_addr_equal(pw->peer_addr, sta->addr)))
			continue;
		if ((rx_id && !pw->identifier) || (!rx_id && pw->identifier))
			continue;
		if (rx_id && pw->identifier &&
		    os_strcmp(rx_id, pw->identifier) != 0)
			continue;
		password = pw->password;
		pt = pw->pt;
		if (!(hapd->conf->mesh & MESH_ENABLED))
			pk = pw->pk;
		break;
	}
	if (!password) {
		password = hapd->conf->ssid.wpa_passphrase;
		pt = hapd->conf->ssid.pt;
	}

	if (!password && sta) {
		for (psk = sta->psk; psk; psk = psk->next) {
			if (psk->is_passphrase) {
				password = psk->passphrase;
				break;
			}
		}
	}

	if (pw_entry)
		*pw_entry = pw;
	if (s_pt)
		*s_pt = pt;
	if (s_pk)
		*s_pk = pk;

	return password;
}


static struct wpabuf * auth_build_sae_commit(struct hostapd_data *hapd,
					     struct sta_info *sta, int update,
					     int status_code)
{
	struct wpabuf *buf;
	const char *password = NULL;
	struct sae_password_entry *pw;
	const char *rx_id = NULL;
	int use_pt = 0;
	struct sae_pt *pt = NULL;
	const struct sae_pk *pk = NULL;
	const u8 *own_addr = hapd->own_addr;

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta))
		own_addr = hapd->mld->mld_addr;
#endif /* CONFIG_IEEE80211BE */

	if (sta->sae->tmp) {
		rx_id = sta->sae->tmp->pw_id;
		use_pt = sta->sae->h2e;
#ifdef CONFIG_SAE_PK
		os_memcpy(sta->sae->tmp->own_addr, own_addr, ETH_ALEN);
		os_memcpy(sta->sae->tmp->peer_addr, sta->addr, ETH_ALEN);
#endif /* CONFIG_SAE_PK */
	}

	if (rx_id && hapd->conf->sae_pwe != SAE_PWE_FORCE_HUNT_AND_PECK)
		use_pt = 1;
	else if (status_code == WLAN_STATUS_SUCCESS)
		use_pt = 0;
	else if (status_code == WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
		 status_code == WLAN_STATUS_SAE_PK)
		use_pt = 1;

	password = sae_get_password(hapd, sta, rx_id, &pw, &pt, &pk);
	if (!password || (use_pt && !pt)) {
		wpa_printf(MSG_DEBUG, "SAE: No password available");
		return NULL;
	}

	if (update && use_pt &&
	    sae_prepare_commit_pt(sta->sae, pt, own_addr, sta->addr,
				  NULL, pk) < 0)
		return NULL;

	if (update && !use_pt &&
	    sae_prepare_commit(own_addr, sta->addr,
			       (u8 *) password, os_strlen(password),
			       sta->sae) < 0) {
		wpa_printf(MSG_DEBUG, "SAE: Could not pick PWE");
		return NULL;
	}

	if (pw && pw->vlan_id) {
		if (!sta->sae->tmp) {
			wpa_printf(MSG_INFO,
				   "SAE: No temporary data allocated - cannot store VLAN ID");
			return NULL;
		}
		sta->sae->tmp->vlan_id = pw->vlan_id;
	}

	buf = wpabuf_alloc(SAE_COMMIT_MAX_LEN +
			   (rx_id ? 3 + os_strlen(rx_id) : 0));
	if (buf &&
	    sae_write_commit(sta->sae, buf, sta->sae->tmp ?
			     sta->sae->tmp->anti_clogging_token : NULL,
			     rx_id) < 0) {
		wpabuf_free(buf);
		buf = NULL;
	}

	return buf;
}


static struct wpabuf * auth_build_sae_confirm(struct hostapd_data *hapd,
					      struct sta_info *sta)
{
	struct wpabuf *buf;

	buf = wpabuf_alloc(SAE_CONFIRM_MAX_LEN);
	if (buf == NULL)
		return NULL;

#ifdef CONFIG_SAE_PK
#ifdef CONFIG_TESTING_OPTIONS
	if (sta->sae->tmp)
		sta->sae->tmp->omit_pk_elem = hapd->conf->sae_pk_omit;
#endif /* CONFIG_TESTING_OPTIONS */
#endif /* CONFIG_SAE_PK */

	if (sae_write_confirm(sta->sae, buf) < 0) {
		wpabuf_free(buf);
		return NULL;
	}

	return buf;
}


static int auth_sae_send_commit(struct hostapd_data *hapd,
				struct sta_info *sta,
				int update, int status_code)
{
	struct wpabuf *data;
	int reply_res;
	u16 status;

	data = auth_build_sae_commit(hapd, sta, update, status_code);
	if (!data && sta->sae->tmp && sta->sae->tmp->pw_id)
		return WLAN_STATUS_UNKNOWN_PASSWORD_IDENTIFIER;
	if (data == NULL)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	if (sta->sae->tmp && sta->sae->pk)
		status = WLAN_STATUS_SAE_PK;
	else if (sta->sae->tmp && sta->sae->h2e)
		status = WLAN_STATUS_SAE_HASH_TO_ELEMENT;
	else
		status = WLAN_STATUS_SUCCESS;
#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->sae_commit_status >= 0 &&
	    hapd->conf->sae_commit_status != status) {
		wpa_printf(MSG_INFO,
			   "TESTING: Override SAE commit status code %u --> %d",
			   status, hapd->conf->sae_commit_status);
		status = hapd->conf->sae_commit_status;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	reply_res = send_auth_reply(hapd, sta, sta->addr,
				    WLAN_AUTH_SAE, 1,
				    status, wpabuf_head(data),
				    wpabuf_len(data), "sae-send-commit");

	wpabuf_free(data);

	return reply_res;
}


static int auth_sae_send_confirm(struct hostapd_data *hapd,
				 struct sta_info *sta)
{
	struct wpabuf *data;
	int reply_res;

	data = auth_build_sae_confirm(hapd, sta);
	if (data == NULL)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	reply_res = send_auth_reply(hapd, sta, sta->addr,
				    WLAN_AUTH_SAE, 2,
				    WLAN_STATUS_SUCCESS, wpabuf_head(data),
				    wpabuf_len(data), "sae-send-confirm");

	wpabuf_free(data);

	return reply_res;
}

#endif /* CONFIG_SAE */


#if defined(CONFIG_SAE) || defined(CONFIG_PASN)

static int use_anti_clogging(struct hostapd_data *hapd)
{
	struct sta_info *sta;
	unsigned int open = 0;

	if (hapd->conf->anti_clogging_threshold == 0)
		return 1;

	for (sta = hapd->sta_list; sta; sta = sta->next) {
#ifdef CONFIG_SAE
		if (sta->sae &&
		    (sta->sae->state == SAE_COMMITTED ||
		     sta->sae->state == SAE_CONFIRMED))
			open++;
#endif /* CONFIG_SAE */
#ifdef CONFIG_PASN
		if (sta->pasn && sta->pasn->ecdh)
			open++;
#endif /* CONFIG_PASN */
		if (open >= hapd->conf->anti_clogging_threshold)
			return 1;
	}

#ifdef CONFIG_SAE
	/* In addition to already existing open SAE sessions, check whether
	 * there are enough pending commit messages in the processing queue to
	 * potentially result in too many open sessions. */
	if (open + dl_list_len(&hapd->sae_commit_queue) >=
	    hapd->conf->anti_clogging_threshold)
		return 1;
#endif /* CONFIG_SAE */

	return 0;
}

#endif /* defined(CONFIG_SAE) || defined(CONFIG_PASN) */


#ifdef CONFIG_SAE

static int sae_check_big_sync(struct hostapd_data *hapd, struct sta_info *sta)
{
	if (sta->sae->sync > hapd->conf->sae_sync) {
		sae_set_state(sta, SAE_NOTHING, "Sync > dot11RSNASAESync");
		sta->sae->sync = 0;
		if (sta->sae->tmp) {
			/* Disable this SAE instance for 10 seconds to avoid
			 * unnecessary flood of multiple SAE commits in
			 * unexpected mesh cases. */
			if (os_get_reltime(&sta->sae->tmp->disabled_until) == 0)
				sta->sae->tmp->disabled_until.sec += 10;
		}
		return -1;
	}
	return 0;
}


static bool sae_proto_instance_disabled(struct sta_info *sta)
{
	struct sae_temporary_data *tmp;

	if (!sta->sae)
		return false;
	tmp = sta->sae->tmp;
	if (!tmp)
		return false;

	if (os_reltime_initialized(&tmp->disabled_until)) {
		struct os_reltime now;

		os_get_reltime(&now);
		if (os_reltime_before(&now, &tmp->disabled_until))
			return true;
	}

	return false;
}


static void auth_sae_retransmit_timer(void *eloop_ctx, void *eloop_data)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = eloop_data;
	int ret;

	if (sae_check_big_sync(hapd, sta))
		return;
	sta->sae->sync++;
	wpa_printf(MSG_DEBUG, "SAE: Auth SAE retransmit timer for " MACSTR
		   " (sync=%d state=%s)",
		   MAC2STR(sta->addr), sta->sae->sync,
		   sae_state_txt(sta->sae->state));

	switch (sta->sae->state) {
	case SAE_COMMITTED:
		ret = auth_sae_send_commit(hapd, sta, 0, -1);
		eloop_register_timeout(0,
				       hapd->dot11RSNASAERetransPeriod * 1000,
				       auth_sae_retransmit_timer, hapd, sta);
		break;
	case SAE_CONFIRMED:
		ret = auth_sae_send_confirm(hapd, sta);
		eloop_register_timeout(0,
				       hapd->dot11RSNASAERetransPeriod * 1000,
				       auth_sae_retransmit_timer, hapd, sta);
		break;
	default:
		ret = -1;
		break;
	}

	if (ret != WLAN_STATUS_SUCCESS)
		wpa_printf(MSG_INFO, "SAE: Failed to retransmit: ret=%d", ret);
}


void sae_clear_retransmit_timer(struct hostapd_data *hapd, struct sta_info *sta)
{
	eloop_cancel_timeout(auth_sae_retransmit_timer, hapd, sta);
}


static void sae_set_retransmit_timer(struct hostapd_data *hapd,
				     struct sta_info *sta)
{
	if (!(hapd->conf->mesh & MESH_ENABLED))
		return;

	eloop_cancel_timeout(auth_sae_retransmit_timer, hapd, sta);
	eloop_register_timeout(0, hapd->dot11RSNASAERetransPeriod * 1000,
			       auth_sae_retransmit_timer, hapd, sta);
}


static void sae_sme_send_external_auth_status(struct hostapd_data *hapd,
					      struct sta_info *sta, u16 status)
{
	struct external_auth params;

	os_memset(&params, 0, sizeof(params));
	params.status = status;

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta))
		params.bssid =
			sta->mld_info.links[sta->mld_assoc_link_id].peer_addr;
#endif /* CONFIG_IEEE80211BE */
	if (!params.bssid)
		params.bssid = sta->addr;

	if (status == WLAN_STATUS_SUCCESS && sta->sae &&
	    !hapd->conf->disable_pmksa_caching)
		params.pmkid = sta->sae->pmkid;

	hostapd_drv_send_external_auth_status(hapd, &params);
}


void sae_accept_sta(struct hostapd_data *hapd, struct sta_info *sta)
{
#ifndef CONFIG_NO_VLAN
	struct vlan_description vlan_desc;

	if (sta->sae->tmp && sta->sae->tmp->vlan_id > 0) {
		wpa_printf(MSG_DEBUG, "SAE: Assign STA " MACSTR
			   " to VLAN ID %d",
			   MAC2STR(sta->addr), sta->sae->tmp->vlan_id);

		if (!(hapd->iface->drv_flags & WPA_DRIVER_FLAGS_VLAN_OFFLOAD)) {
			os_memset(&vlan_desc, 0, sizeof(vlan_desc));
			vlan_desc.notempty = 1;
			vlan_desc.untagged = sta->sae->tmp->vlan_id;
			if (!hostapd_vlan_valid(hapd->conf->vlan, &vlan_desc)) {
				wpa_printf(MSG_INFO,
					   "Invalid VLAN ID %d in sae_password",
					   sta->sae->tmp->vlan_id);
				return;
			}

			if (ap_sta_set_vlan(hapd, sta, &vlan_desc) < 0 ||
			    ap_sta_bind_vlan(hapd, sta) < 0) {
				wpa_printf(MSG_INFO,
					   "Failed to assign VLAN ID %d from sae_password to "
					   MACSTR, sta->sae->tmp->vlan_id,
					   MAC2STR(sta->addr));
				return;
			}
		} else {
			sta->vlan_id = sta->sae->tmp->vlan_id;
		}
	}
#endif /* CONFIG_NO_VLAN */

	sta->flags |= WLAN_STA_AUTH;
	sta->auth_alg = WLAN_AUTH_SAE;
	mlme_authenticate_indication(hapd, sta);
	wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
	sae_set_state(sta, SAE_ACCEPTED, "Accept Confirm");
	crypto_bignum_deinit(sta->sae->peer_commit_scalar_accepted, 0);
	sta->sae->peer_commit_scalar_accepted = sta->sae->peer_commit_scalar;
	sta->sae->peer_commit_scalar = NULL;
	wpa_auth_pmksa_add_sae(hapd->wpa_auth, sta->addr,
			       sta->sae->pmk, sta->sae->pmk_len,
			       sta->sae->pmkid, sta->sae->akmp);
	sae_sme_send_external_auth_status(hapd, sta, WLAN_STATUS_SUCCESS);
}


static int sae_sm_step(struct hostapd_data *hapd, struct sta_info *sta,
		       u16 auth_transaction, u16 status_code,
		       int allow_reuse, int *sta_removed)
{
	int ret;

	*sta_removed = 0;

	if (auth_transaction != 1 && auth_transaction != 2)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	wpa_printf(MSG_DEBUG, "SAE: Peer " MACSTR " state=%s auth_trans=%u",
		   MAC2STR(sta->addr), sae_state_txt(sta->sae->state),
		   auth_transaction);

	if (auth_transaction == 1 && sae_proto_instance_disabled(sta)) {
		wpa_printf(MSG_DEBUG,
			   "SAE: Protocol instance temporarily disabled - discard received SAE commit");
		return WLAN_STATUS_SUCCESS;
	}

	switch (sta->sae->state) {
	case SAE_NOTHING:
		if (auth_transaction == 1) {
			if (sta->sae->tmp) {
				sta->sae->h2e =
					(status_code ==
					 WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
					 status_code == WLAN_STATUS_SAE_PK);
				sta->sae->pk =
					status_code == WLAN_STATUS_SAE_PK;
			}
			ret = auth_sae_send_commit(hapd, sta,
						   !allow_reuse, status_code);
			if (ret)
				return ret;
			sae_set_state(sta, SAE_COMMITTED, "Sent Commit");

			if (sae_process_commit(sta->sae) < 0)
				return WLAN_STATUS_UNSPECIFIED_FAILURE;

			/*
			 * In mesh case, both Commit and Confirm are sent
			 * immediately. In infrastructure BSS, by default, only
			 * a single Authentication frame (Commit) is expected
			 * from the AP here and the second one (Confirm) will
			 * be sent once the STA has sent its second
			 * Authentication frame (Confirm). This behavior can be
			 * overridden with explicit configuration so that the
			 * infrastructure BSS case sends both frames together.
			 */
			if ((hapd->conf->mesh & MESH_ENABLED) ||
			    hapd->conf->sae_confirm_immediate) {
				/*
				 * Send both Commit and Confirm immediately
				 * based on SAE finite state machine
				 * Nothing -> Confirm transition.
				 */
				ret = auth_sae_send_confirm(hapd, sta);
				if (ret)
					return ret;
				sae_set_state(sta, SAE_CONFIRMED,
					      "Sent Confirm (mesh)");
			} else {
				/*
				 * For infrastructure BSS, send only the Commit
				 * message now to get alternating sequence of
				 * Authentication frames between the AP and STA.
				 * Confirm will be sent in
				 * Committed -> Confirmed/Accepted transition
				 * when receiving Confirm from STA.
				 */
			}
			sta->sae->sync = 0;
			sae_set_retransmit_timer(hapd, sta);
		} else {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG,
				       "SAE confirm before commit");
		}
		break;
	case SAE_COMMITTED:
		sae_clear_retransmit_timer(hapd, sta);
		if (auth_transaction == 1) {
			if (sae_process_commit(sta->sae) < 0)
				return WLAN_STATUS_UNSPECIFIED_FAILURE;

			ret = auth_sae_send_confirm(hapd, sta);
			if (ret)
				return ret;
			sae_set_state(sta, SAE_CONFIRMED, "Sent Confirm");
			sta->sae->sync = 0;
			sae_set_retransmit_timer(hapd, sta);
		} else if (hapd->conf->mesh & MESH_ENABLED) {
			/*
			 * In mesh case, follow SAE finite state machine and
			 * send Commit now, if sync count allows.
			 */
			if (sae_check_big_sync(hapd, sta))
				return WLAN_STATUS_SUCCESS;
			sta->sae->sync++;

			ret = auth_sae_send_commit(hapd, sta, 0, status_code);
			if (ret)
				return ret;

			sae_set_retransmit_timer(hapd, sta);
		} else {
			/*
			 * For instructure BSS, send the postponed Confirm from
			 * Nothing -> Confirmed transition that was reduced to
			 * Nothing -> Committed above.
			 */
			ret = auth_sae_send_confirm(hapd, sta);
			if (ret)
				return ret;

			sae_set_state(sta, SAE_CONFIRMED, "Sent Confirm");

			/*
			 * Since this was triggered on Confirm RX, run another
			 * step to get to Accepted without waiting for
			 * additional events.
			 */
			return sae_sm_step(hapd, sta, auth_transaction,
					   WLAN_STATUS_SUCCESS, 0, sta_removed);
		}
		break;
	case SAE_CONFIRMED:
		sae_clear_retransmit_timer(hapd, sta);
		if (auth_transaction == 1) {
			if (sae_check_big_sync(hapd, sta))
				return WLAN_STATUS_SUCCESS;
			sta->sae->sync++;

			ret = auth_sae_send_commit(hapd, sta, 1, status_code);
			if (ret)
				return ret;

			if (sae_process_commit(sta->sae) < 0)
				return WLAN_STATUS_UNSPECIFIED_FAILURE;

			ret = auth_sae_send_confirm(hapd, sta);
			if (ret)
				return ret;

			sae_set_retransmit_timer(hapd, sta);
		} else {
			sta->sae->send_confirm = 0xffff;
			sae_accept_sta(hapd, sta);
		}
		break;
	case SAE_ACCEPTED:
		if (auth_transaction == 1 &&
		    (hapd->conf->mesh & MESH_ENABLED)) {
			wpa_printf(MSG_DEBUG, "SAE: remove the STA (" MACSTR
				   ") doing reauthentication",
				   MAC2STR(sta->addr));
			wpa_auth_pmksa_remove(hapd->wpa_auth, sta->addr);
			ap_free_sta(hapd, sta);
			*sta_removed = 1;
		} else if (auth_transaction == 1) {
			wpa_printf(MSG_DEBUG, "SAE: Start reauthentication");
			ret = auth_sae_send_commit(hapd, sta, 1, status_code);
			if (ret)
				return ret;
			sae_set_state(sta, SAE_COMMITTED, "Sent Commit");

			if (sae_process_commit(sta->sae) < 0)
				return WLAN_STATUS_UNSPECIFIED_FAILURE;
			sta->sae->sync = 0;
			sae_set_retransmit_timer(hapd, sta);
		} else {
			if (sae_check_big_sync(hapd, sta))
				return WLAN_STATUS_SUCCESS;
			sta->sae->sync++;

			ret = auth_sae_send_confirm(hapd, sta);
			sae_clear_temp_data(sta->sae);
			if (ret)
				return ret;
		}
		break;
	default:
		wpa_printf(MSG_ERROR, "SAE: invalid state %d",
			   sta->sae->state);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
	return WLAN_STATUS_SUCCESS;
}


static void sae_pick_next_group(struct hostapd_data *hapd, struct sta_info *sta)
{
	struct sae_data *sae = sta->sae;
	int i, *groups = hapd->conf->sae_groups;
	int default_groups[] = { 19, 0 };

	if (sae->state != SAE_COMMITTED)
		return;

	wpa_printf(MSG_DEBUG, "SAE: Previously selected group: %d", sae->group);

	if (!groups)
		groups = default_groups;
	for (i = 0; groups[i] > 0; i++) {
		if (sae->group == groups[i])
			break;
	}

	if (groups[i] <= 0) {
		wpa_printf(MSG_DEBUG,
			   "SAE: Previously selected group not found from the current configuration");
		return;
	}

	for (;;) {
		i++;
		if (groups[i] <= 0) {
			wpa_printf(MSG_DEBUG,
				   "SAE: No alternative group enabled");
			return;
		}

		if (sae_set_group(sae, groups[i]) < 0)
			continue;

		break;
	}
	wpa_printf(MSG_DEBUG, "SAE: Selected new group: %d", groups[i]);
}


static int sae_status_success(struct hostapd_data *hapd, u16 status_code)
{
	enum sae_pwe sae_pwe = hapd->conf->sae_pwe;
	int id_in_use;
	bool sae_pk = false;

	id_in_use = hostapd_sae_pw_id_in_use(hapd->conf);
	if (id_in_use == 2 && sae_pwe != SAE_PWE_FORCE_HUNT_AND_PECK)
		sae_pwe = SAE_PWE_HASH_TO_ELEMENT;
	else if (id_in_use == 1 && sae_pwe == SAE_PWE_HUNT_AND_PECK)
		sae_pwe = SAE_PWE_BOTH;
#ifdef CONFIG_SAE_PK
	sae_pk = hostapd_sae_pk_in_use(hapd->conf);
	if (sae_pwe == SAE_PWE_HUNT_AND_PECK && sae_pk)
		sae_pwe = SAE_PWE_BOTH;
#endif /* CONFIG_SAE_PK */
	if (sae_pwe == SAE_PWE_HUNT_AND_PECK &&
	    (hapd->conf->wpa_key_mgmt &
	     (WPA_KEY_MGMT_SAE_EXT_KEY | WPA_KEY_MGMT_FT_SAE_EXT_KEY)))
		sae_pwe = SAE_PWE_BOTH;

	return ((sae_pwe == SAE_PWE_HUNT_AND_PECK ||
		 sae_pwe == SAE_PWE_FORCE_HUNT_AND_PECK) &&
		status_code == WLAN_STATUS_SUCCESS) ||
		(sae_pwe == SAE_PWE_HASH_TO_ELEMENT &&
		 (status_code == WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
		  (sae_pk && status_code == WLAN_STATUS_SAE_PK))) ||
		(sae_pwe == SAE_PWE_BOTH &&
		 (status_code == WLAN_STATUS_SUCCESS ||
		  status_code == WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
		  (sae_pk && status_code == WLAN_STATUS_SAE_PK)));
}


static int sae_is_group_enabled(struct hostapd_data *hapd, int group)
{
	int *groups = hapd->conf->sae_groups;
	int default_groups[] = { 19, 0 };
	int i;

	if (!groups)
		groups = default_groups;

	for (i = 0; groups[i] > 0; i++) {
		if (groups[i] == group)
			return 1;
	}

	return 0;
}


static int check_sae_rejected_groups(struct hostapd_data *hapd,
				     struct sae_data *sae)
{
	const struct wpabuf *groups;
	size_t i, count, len;
	const u8 *pos;

	if (!sae->tmp)
		return 0;
	groups = sae->tmp->peer_rejected_groups;
	if (!groups)
		return 0;

	pos = wpabuf_head(groups);
	len = wpabuf_len(groups);
	if (len & 1) {
		wpa_printf(MSG_DEBUG,
			   "SAE: Invalid length of the Rejected Groups element payload: %zu",
			   len);
		return 1;
	}

	count = len / 2;
	for (i = 0; i < count; i++) {
		int enabled;
		u16 group;

		group = WPA_GET_LE16(pos);
		pos += 2;
		enabled = sae_is_group_enabled(hapd, group);
		wpa_printf(MSG_DEBUG, "SAE: Rejected group %u is %s",
			   group, enabled ? "enabled" : "disabled");
		if (enabled)
			return 1;
	}

	return 0;
}


static void handle_auth_sae(struct hostapd_data *hapd, struct sta_info *sta,
			    const struct ieee80211_mgmt *mgmt, size_t len,
			    u16 auth_transaction, u16 status_code)
{
	int resp = WLAN_STATUS_SUCCESS;
	struct wpabuf *data = NULL;
	int *groups = hapd->conf->sae_groups;
	int default_groups[] = { 19, 0 };
	const u8 *pos, *end;
	int sta_removed = 0;
	bool success_status;

	if (!groups)
		groups = default_groups;

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->sae_reflection_attack && auth_transaction == 1) {
		wpa_printf(MSG_DEBUG, "SAE: TESTING - reflection attack");
		pos = mgmt->u.auth.variable;
		end = ((const u8 *) mgmt) + len;
		resp = status_code;
		send_auth_reply(hapd, sta, sta->addr,
				WLAN_AUTH_SAE,
				auth_transaction, resp, pos, end - pos,
				"auth-sae-reflection-attack");
		goto remove_sta;
	}

	if (hapd->conf->sae_commit_override && auth_transaction == 1) {
		wpa_printf(MSG_DEBUG, "SAE: TESTING - commit override");
		send_auth_reply(hapd, sta, sta->addr,
				WLAN_AUTH_SAE,
				auth_transaction, resp,
				wpabuf_head(hapd->conf->sae_commit_override),
				wpabuf_len(hapd->conf->sae_commit_override),
				"sae-commit-override");
		goto remove_sta;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	if (!sta->sae) {
		if (auth_transaction != 1 ||
		    !sae_status_success(hapd, status_code)) {
			wpa_printf(MSG_DEBUG, "SAE: Unexpected Status Code %u",
				   status_code);
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto reply;
		}
		sta->sae = os_zalloc(sizeof(*sta->sae));
		if (!sta->sae) {
			resp = -1;
			goto remove_sta;
		}
		sae_set_state(sta, SAE_NOTHING, "Init");
		sta->sae->sync = 0;
	}

	if (sta->mesh_sae_pmksa_caching) {
		wpa_printf(MSG_DEBUG,
			   "SAE: Cancel use of mesh PMKSA caching because peer starts SAE authentication");
		wpa_auth_pmksa_remove(hapd->wpa_auth, sta->addr);
		sta->mesh_sae_pmksa_caching = 0;
	}

	if (auth_transaction == 1) {
		const u8 *token = NULL;
		size_t token_len = 0;
		int allow_reuse = 0;

		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "start SAE authentication (RX commit, status=%u (%s))",
			       status_code, status2str(status_code));

		if ((hapd->conf->mesh & MESH_ENABLED) &&
		    status_code == WLAN_STATUS_ANTI_CLOGGING_TOKEN_REQ &&
		    sta->sae->tmp) {
			pos = mgmt->u.auth.variable;
			end = ((const u8 *) mgmt) + len;
			if (pos + sizeof(le16) > end) {
				wpa_printf(MSG_ERROR,
					   "SAE: Too short anti-clogging token request");
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto reply;
			}
			resp = sae_group_allowed(sta->sae, groups,
						 WPA_GET_LE16(pos));
			if (resp != WLAN_STATUS_SUCCESS) {
				wpa_printf(MSG_ERROR,
					   "SAE: Invalid group in anti-clogging token request");
				goto reply;
			}
			pos += sizeof(le16);

			wpabuf_free(sta->sae->tmp->anti_clogging_token);
			sta->sae->tmp->anti_clogging_token =
				wpabuf_alloc_copy(pos, end - pos);
			if (sta->sae->tmp->anti_clogging_token == NULL) {
				wpa_printf(MSG_ERROR,
					   "SAE: Failed to alloc for anti-clogging token");
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto remove_sta;
			}

			/*
			 * IEEE Std 802.11-2012, 11.3.8.6.4: If the Status code
			 * is 76, a new Commit Message shall be constructed
			 * with the Anti-Clogging Token from the received
			 * Authentication frame, and the commit-scalar and
			 * COMMIT-ELEMENT previously sent.
			 */
			resp = auth_sae_send_commit(hapd, sta, 0, status_code);
			if (resp != WLAN_STATUS_SUCCESS) {
				wpa_printf(MSG_ERROR,
					   "SAE: Failed to send commit message");
				goto remove_sta;
			}
			sae_set_state(sta, SAE_COMMITTED,
				      "Sent Commit (anti-clogging token case in mesh)");
			sta->sae->sync = 0;
			sae_set_retransmit_timer(hapd, sta);
			return;
		}

		if ((hapd->conf->mesh & MESH_ENABLED) &&
		    status_code ==
		    WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED &&
		    sta->sae->tmp) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Peer did not accept our SAE group");
			sae_pick_next_group(hapd, sta);
			goto remove_sta;
		}

		if (!sae_status_success(hapd, status_code))
			goto remove_sta;

		if (sae_proto_instance_disabled(sta)) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Protocol instance temporarily disabled - discard received SAE commit");
			return;
		}

		if (!(hapd->conf->mesh & MESH_ENABLED) &&
		    sta->sae->state == SAE_COMMITTED) {
			/* This is needed in the infrastructure BSS case to
			 * address a sequence where a STA entry may remain in
			 * hostapd across two attempts to do SAE authentication
			 * by the same STA. The second attempt may end up trying
			 * to use a different group and that would not be
			 * allowed if we remain in Committed state with the
			 * previously set parameters. */
			pos = mgmt->u.auth.variable;
			end = ((const u8 *) mgmt) + len;
			if (end - pos >= (int) sizeof(le16) &&
			    sae_group_allowed(sta->sae, groups,
					      WPA_GET_LE16(pos)) ==
			    WLAN_STATUS_SUCCESS) {
				/* Do not waste resources deriving the same PWE
				 * again since the same group is reused. */
				sae_set_state(sta, SAE_NOTHING,
					      "Allow previous PWE to be reused");
				allow_reuse = 1;
			} else {
				sae_set_state(sta, SAE_NOTHING,
					      "Clear existing state to allow restart");
				sae_clear_data(sta->sae);
			}
		}

		resp = sae_parse_commit(sta->sae, mgmt->u.auth.variable,
					((const u8 *) mgmt) + len -
					mgmt->u.auth.variable, &token,
					&token_len, groups, status_code ==
					WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
					status_code == WLAN_STATUS_SAE_PK,
					NULL);
		if (resp == SAE_SILENTLY_DISCARD) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Drop commit message from " MACSTR " due to reflection attack",
				   MAC2STR(sta->addr));
			goto remove_sta;
		}

		if (resp == WLAN_STATUS_UNKNOWN_PASSWORD_IDENTIFIER) {
			wpa_msg(hapd->msg_ctx, MSG_INFO,
				WPA_EVENT_SAE_UNKNOWN_PASSWORD_IDENTIFIER
				MACSTR, MAC2STR(sta->addr));
			sae_clear_retransmit_timer(hapd, sta);
			sae_set_state(sta, SAE_NOTHING,
				      "Unknown Password Identifier");
			goto remove_sta;
		}

		if (token &&
		    check_comeback_token(hapd->comeback_key,
					 hapd->comeback_pending_idx, sta->addr,
					 token, token_len)
		    < 0) {
			wpa_printf(MSG_DEBUG, "SAE: Drop commit message with "
				   "incorrect token from " MACSTR,
				   MAC2STR(sta->addr));
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto remove_sta;
		}

		if (resp != WLAN_STATUS_SUCCESS)
			goto reply;

		if (check_sae_rejected_groups(hapd, sta->sae)) {
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto reply;
		}

		if (!token && use_anti_clogging(hapd) && !allow_reuse) {
			int h2e = 0;

			wpa_printf(MSG_DEBUG,
				   "SAE: Request anti-clogging token from "
				   MACSTR, MAC2STR(sta->addr));
			if (sta->sae->tmp)
				h2e = sta->sae->h2e;
			if (status_code == WLAN_STATUS_SAE_HASH_TO_ELEMENT ||
			    status_code == WLAN_STATUS_SAE_PK)
				h2e = 1;
			data = auth_build_token_req(
				&hapd->last_comeback_key_update,
				hapd->comeback_key,
				hapd->comeback_idx,
				hapd->comeback_pending_idx,
				sizeof(hapd->comeback_pending_idx),
				sta->sae->group,
				sta->addr, h2e);
			resp = WLAN_STATUS_ANTI_CLOGGING_TOKEN_REQ;
			if (hapd->conf->mesh & MESH_ENABLED)
				sae_set_state(sta, SAE_NOTHING,
					      "Request anti-clogging token case in mesh");
			goto reply;
		}

		resp = sae_sm_step(hapd, sta, auth_transaction,
				   status_code, allow_reuse, &sta_removed);
	} else if (auth_transaction == 2) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "SAE authentication (RX confirm, status=%u (%s))",
			       status_code, status2str(status_code));
		if (status_code != WLAN_STATUS_SUCCESS)
			goto remove_sta;
		if (sta->sae->state >= SAE_CONFIRMED ||
		    !(hapd->conf->mesh & MESH_ENABLED)) {
			const u8 *var;
			size_t var_len;
			u16 peer_send_confirm;

			var = mgmt->u.auth.variable;
			var_len = ((u8 *) mgmt) + len - mgmt->u.auth.variable;
			if (var_len < 2) {
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto reply;
			}

			peer_send_confirm = WPA_GET_LE16(var);

			if (sta->sae->state == SAE_ACCEPTED &&
			    (peer_send_confirm <= sta->sae->rc ||
			     peer_send_confirm == 0xffff)) {
				wpa_printf(MSG_DEBUG,
					   "SAE: Silently ignore unexpected Confirm from peer "
					   MACSTR
					   " (peer-send-confirm=%u Rc=%u)",
					   MAC2STR(sta->addr),
					   peer_send_confirm, sta->sae->rc);
				return;
			}

			if (sae_check_confirm(sta->sae, var, var_len,
					      NULL) < 0) {
				resp = WLAN_STATUS_CHALLENGE_FAIL;
				goto reply;
			}
			sta->sae->rc = peer_send_confirm;
		}
		resp = sae_sm_step(hapd, sta, auth_transaction,
				   status_code, 0, &sta_removed);
	} else {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "unexpected SAE authentication transaction %u (status=%u (%s))",
			       auth_transaction, status_code,
			       status2str(status_code));
		if (status_code != WLAN_STATUS_SUCCESS)
			goto remove_sta;
		resp = WLAN_STATUS_UNKNOWN_AUTH_TRANSACTION;
	}

reply:
	if (!sta_removed && resp != WLAN_STATUS_SUCCESS) {
		pos = mgmt->u.auth.variable;
		end = ((const u8 *) mgmt) + len;

		/* Copy the Finite Cyclic Group field from the request if we
		 * rejected it as unsupported group. */
		if (resp == WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED &&
		    !data && end - pos >= 2)
			data = wpabuf_alloc_copy(pos, 2);

		sae_sme_send_external_auth_status(hapd, sta, resp);
		send_auth_reply(hapd, sta, sta->addr,
				WLAN_AUTH_SAE,
				auth_transaction, resp,
				data ? wpabuf_head(data) : (u8 *) "",
				data ? wpabuf_len(data) : 0, "auth-sae");
		if (sta->sae && sta->sae->tmp && sta->sae->tmp->pw_id &&
		    resp == WLAN_STATUS_UNKNOWN_PASSWORD_IDENTIFIER &&
		    auth_transaction == 1) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Clear stored password identifier since this SAE commit was not accepted");
			os_free(sta->sae->tmp->pw_id);
			sta->sae->tmp->pw_id = NULL;
		}
	}

remove_sta:
	if (auth_transaction == 1)
		success_status = sae_status_success(hapd, status_code);
	else
		success_status = status_code == WLAN_STATUS_SUCCESS;
	if (!sta_removed && sta->added_unassoc &&
	    (resp != WLAN_STATUS_SUCCESS || !success_status)) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		sta->added_unassoc = 0;
	}
	wpabuf_free(data);
}


/**
 * auth_sae_init_committed - Send COMMIT and start SAE in committed state
 * @hapd: BSS data for the device initiating the authentication
 * @sta: the peer to which commit authentication frame is sent
 *
 * This function implements Init event handling (IEEE Std 802.11-2012,
 * 11.3.8.6.3) in which initial COMMIT message is sent. Prior to calling, the
 * sta->sae structure should be initialized appropriately via a call to
 * sae_prepare_commit().
 */
int auth_sae_init_committed(struct hostapd_data *hapd, struct sta_info *sta)
{
	int ret;

	if (!sta->sae || !sta->sae->tmp)
		return -1;

	if (sta->sae->state != SAE_NOTHING)
		return -1;

	ret = auth_sae_send_commit(hapd, sta, 0, -1);
	if (ret)
		return -1;

	sae_set_state(sta, SAE_COMMITTED, "Init and sent commit");
	sta->sae->sync = 0;
	sae_set_retransmit_timer(hapd, sta);

	return 0;
}


void auth_sae_process_commit(void *eloop_ctx, void *user_ctx)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct hostapd_sae_commit_queue *q;
	unsigned int queue_len;

	q = dl_list_first(&hapd->sae_commit_queue,
			  struct hostapd_sae_commit_queue, list);
	if (!q)
		return;
	wpa_printf(MSG_DEBUG,
		   "SAE: Process next available message from queue");
	dl_list_del(&q->list);
	handle_auth(hapd, (const struct ieee80211_mgmt *) q->msg, q->len,
		    q->rssi, 1);
	os_free(q);

	if (eloop_is_timeout_registered(auth_sae_process_commit, hapd, NULL))
		return;
	queue_len = dl_list_len(&hapd->sae_commit_queue);
	eloop_register_timeout(0, queue_len * 10000, auth_sae_process_commit,
			       hapd, NULL);
}


static void auth_sae_queue(struct hostapd_data *hapd,
			   const struct ieee80211_mgmt *mgmt, size_t len,
			   int rssi)
{
	struct hostapd_sae_commit_queue *q, *q2;
	unsigned int queue_len;
	const struct ieee80211_mgmt *mgmt2;

	queue_len = dl_list_len(&hapd->sae_commit_queue);
	if (queue_len >= 15) {
		wpa_printf(MSG_DEBUG,
			   "SAE: No more room in message queue - drop the new frame from "
			   MACSTR, MAC2STR(mgmt->sa));
		return;
	}

	wpa_printf(MSG_DEBUG, "SAE: Queue Authentication message from "
		   MACSTR " for processing (queue_len %u)", MAC2STR(mgmt->sa),
		   queue_len);
	q = os_zalloc(sizeof(*q) + len);
	if (!q)
		return;
	q->rssi = rssi;
	q->len = len;
	os_memcpy(q->msg, mgmt, len);

	/* Check whether there is already a queued Authentication frame from the
	 * same station with the same transaction number and if so, replace that
	 * queue entry with the new one. This avoids issues with a peer that
	 * sends multiple times (e.g., due to frequent SAE retries). There is no
	 * point in us trying to process the old attempts after a new one has
	 * obsoleted them. */
	dl_list_for_each(q2, &hapd->sae_commit_queue,
			 struct hostapd_sae_commit_queue, list) {
		mgmt2 = (const struct ieee80211_mgmt *) q2->msg;
		if (ether_addr_equal(mgmt->sa, mgmt2->sa) &&
		    mgmt->u.auth.auth_transaction ==
		    mgmt2->u.auth.auth_transaction) {
			wpa_printf(MSG_DEBUG,
				   "SAE: Replace queued message from same STA with same transaction number");
			dl_list_add(&q2->list, &q->list);
			dl_list_del(&q2->list);
			os_free(q2);
			goto queued;
		}
	}

	/* No pending identical entry, so add to the end of the queue */
	dl_list_add_tail(&hapd->sae_commit_queue, &q->list);

queued:
	if (eloop_is_timeout_registered(auth_sae_process_commit, hapd, NULL))
		return;
	eloop_register_timeout(0, queue_len * 10000, auth_sae_process_commit,
			       hapd, NULL);
}


static int auth_sae_queued_addr(struct hostapd_data *hapd, const u8 *addr)
{
	struct hostapd_sae_commit_queue *q;
	const struct ieee80211_mgmt *mgmt;

	dl_list_for_each(q, &hapd->sae_commit_queue,
			 struct hostapd_sae_commit_queue, list) {
		mgmt = (const struct ieee80211_mgmt *) q->msg;
		if (ether_addr_equal(addr, mgmt->sa))
			return 1;
	}

	return 0;
}

#endif /* CONFIG_SAE */


static u16 wpa_res_to_status_code(enum wpa_validate_result res)
{
	switch (res) {
	case WPA_IE_OK:
		return WLAN_STATUS_SUCCESS;
	case WPA_INVALID_IE:
		return WLAN_STATUS_INVALID_IE;
	case WPA_INVALID_GROUP:
		return WLAN_STATUS_GROUP_CIPHER_NOT_VALID;
	case WPA_INVALID_PAIRWISE:
		return WLAN_STATUS_PAIRWISE_CIPHER_NOT_VALID;
	case WPA_INVALID_AKMP:
		return WLAN_STATUS_AKMP_NOT_VALID;
	case WPA_NOT_ENABLED:
		return WLAN_STATUS_INVALID_IE;
	case WPA_ALLOC_FAIL:
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	case WPA_MGMT_FRAME_PROTECTION_VIOLATION:
		return WLAN_STATUS_ROBUST_MGMT_FRAME_POLICY_VIOLATION;
	case WPA_INVALID_MGMT_GROUP_CIPHER:
		return WLAN_STATUS_CIPHER_REJECTED_PER_POLICY;
	case WPA_INVALID_MDIE:
		return WLAN_STATUS_INVALID_MDIE;
	case WPA_INVALID_PROTO:
		return WLAN_STATUS_INVALID_IE;
	case WPA_INVALID_PMKID:
		return WLAN_STATUS_INVALID_PMKID;
	case WPA_DENIED_OTHER_REASON:
		return WLAN_STATUS_ASSOC_DENIED_UNSPEC;
	}
	return WLAN_STATUS_INVALID_IE;
}


#ifdef CONFIG_FILS

static void handle_auth_fils_finish(struct hostapd_data *hapd,
				    struct sta_info *sta, u16 resp,
				    struct wpabuf *data, int pub);

void handle_auth_fils(struct hostapd_data *hapd, struct sta_info *sta,
		      const u8 *pos, size_t len, u16 auth_alg,
		      u16 auth_transaction, u16 status_code,
		      void (*cb)(struct hostapd_data *hapd,
				 struct sta_info *sta, u16 resp,
				 struct wpabuf *data, int pub))
{
	u16 resp = WLAN_STATUS_SUCCESS;
	const u8 *end;
	struct ieee802_11_elems elems;
	enum wpa_validate_result res;
	struct wpa_ie_data rsn;
	struct rsn_pmksa_cache_entry *pmksa = NULL;

	if (auth_transaction != 1 || status_code != WLAN_STATUS_SUCCESS)
		return;

	end = pos + len;

	wpa_hexdump(MSG_DEBUG, "FILS: Authentication frame fields",
		    pos, end - pos);

	/* TODO: FILS PK */
#ifdef CONFIG_FILS_SK_PFS
	if (auth_alg == WLAN_AUTH_FILS_SK_PFS) {
		u16 group;
		struct wpabuf *pub;
		size_t elem_len;

		/* Using FILS PFS */

		/* Finite Cyclic Group */
		if (end - pos < 2) {
			wpa_printf(MSG_DEBUG,
				   "FILS: No room for Finite Cyclic Group");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		group = WPA_GET_LE16(pos);
		pos += 2;
		if (group != hapd->conf->fils_dh_group) {
			wpa_printf(MSG_DEBUG,
				   "FILS: Unsupported Finite Cyclic Group: %u (expected %u)",
				   group, hapd->conf->fils_dh_group);
			resp = WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;
			goto fail;
		}

		crypto_ecdh_deinit(sta->fils_ecdh);
		sta->fils_ecdh = crypto_ecdh_init(group);
		if (!sta->fils_ecdh) {
			wpa_printf(MSG_INFO,
				   "FILS: Could not initialize ECDH with group %d",
				   group);
			resp = WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;
			goto fail;
		}

		pub = crypto_ecdh_get_pubkey(sta->fils_ecdh, 1);
		if (!pub) {
			wpa_printf(MSG_DEBUG,
				   "FILS: Failed to derive ECDH public key");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		elem_len = wpabuf_len(pub);
		wpabuf_free(pub);

		/* Element */
		if ((size_t) (end - pos) < elem_len) {
			wpa_printf(MSG_DEBUG, "FILS: No room for Element");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}

		wpabuf_free(sta->fils_g_sta);
		sta->fils_g_sta = wpabuf_alloc_copy(pos, elem_len);
		wpabuf_clear_free(sta->fils_dh_ss);
		sta->fils_dh_ss = crypto_ecdh_set_peerkey(sta->fils_ecdh, 1,
							  pos, elem_len);
		if (!sta->fils_dh_ss) {
			wpa_printf(MSG_DEBUG, "FILS: ECDH operation failed");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		wpa_hexdump_buf_key(MSG_DEBUG, "FILS: DH_SS", sta->fils_dh_ss);
		pos += elem_len;
	} else {
		crypto_ecdh_deinit(sta->fils_ecdh);
		sta->fils_ecdh = NULL;
		wpabuf_clear_free(sta->fils_dh_ss);
		sta->fils_dh_ss = NULL;
	}
#endif /* CONFIG_FILS_SK_PFS */

	wpa_hexdump(MSG_DEBUG, "FILS: Remaining IEs", pos, end - pos);
	if (ieee802_11_parse_elems(pos, end - pos, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "FILS: Could not parse elements");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	/* RSNE */
	wpa_hexdump(MSG_DEBUG, "FILS: RSN element",
		    elems.rsn_ie, elems.rsn_ie_len);
	if (!elems.rsn_ie ||
	    wpa_parse_wpa_ie_rsn(elems.rsn_ie - 2, elems.rsn_ie_len + 2,
				 &rsn) < 0) {
		wpa_printf(MSG_DEBUG, "FILS: No valid RSN element");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	if (!sta->wpa_sm)
		sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth, sta->addr,
						NULL);
	if (!sta->wpa_sm) {
		wpa_printf(MSG_DEBUG,
			   "FILS: Failed to initialize RSN state machine");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	res = wpa_validate_wpa_ie(hapd->wpa_auth, sta->wpa_sm,
				  hapd->iface->freq,
				  elems.rsn_ie - 2, elems.rsn_ie_len + 2,
				  elems.rsnxe ? elems.rsnxe - 2 : NULL,
				  elems.rsnxe ? elems.rsnxe_len + 2 : 0,
				  elems.mdie, elems.mdie_len, NULL, 0, NULL);
	resp = wpa_res_to_status_code(res);
	if (resp != WLAN_STATUS_SUCCESS)
		goto fail;

	if (!elems.fils_nonce) {
		wpa_printf(MSG_DEBUG, "FILS: No FILS Nonce field");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "FILS: SNonce", elems.fils_nonce,
		    FILS_NONCE_LEN);
	os_memcpy(sta->fils_snonce, elems.fils_nonce, FILS_NONCE_LEN);

	/* PMKID List */
	if (rsn.pmkid && rsn.num_pmkid > 0) {
		u8 num;
		const u8 *pmkid;

		wpa_hexdump(MSG_DEBUG, "FILS: PMKID List",
			    rsn.pmkid, rsn.num_pmkid * PMKID_LEN);

		pmkid = rsn.pmkid;
		num = rsn.num_pmkid;
		while (num) {
			wpa_hexdump(MSG_DEBUG, "FILS: PMKID", pmkid, PMKID_LEN);
			pmksa = wpa_auth_pmksa_get(hapd->wpa_auth, sta->addr,
						   pmkid);
			if (pmksa)
				break;
			pmksa = wpa_auth_pmksa_get_fils_cache_id(hapd->wpa_auth,
								 sta->addr,
								 pmkid);
			if (pmksa)
				break;
			pmkid += PMKID_LEN;
			num--;
		}
	}
	if (pmksa && wpa_auth_sta_key_mgmt(sta->wpa_sm) != pmksa->akmp) {
		wpa_printf(MSG_DEBUG,
			   "FILS: Matching PMKSA cache entry has different AKMP (0x%x != 0x%x) - ignore",
			   wpa_auth_sta_key_mgmt(sta->wpa_sm), pmksa->akmp);
		pmksa = NULL;
	}
	if (pmksa)
		wpa_printf(MSG_DEBUG, "FILS: Found matching PMKSA cache entry");

	/* FILS Session */
	if (!elems.fils_session) {
		wpa_printf(MSG_DEBUG, "FILS: No FILS Session element");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "FILS: FILS Session", elems.fils_session,
		    FILS_SESSION_LEN);
	os_memcpy(sta->fils_session, elems.fils_session, FILS_SESSION_LEN);

	/* Wrapped Data */
	if (elems.wrapped_data) {
		wpa_hexdump(MSG_DEBUG, "FILS: Wrapped Data",
			    elems.wrapped_data,
			    elems.wrapped_data_len);
		if (!pmksa) {
#ifndef CONFIG_NO_RADIUS
			if (!sta->eapol_sm) {
				sta->eapol_sm =
					ieee802_1x_alloc_eapol_sm(hapd, sta);
			}
			wpa_printf(MSG_DEBUG,
				   "FILS: Forward EAP-Initiate/Re-auth to authentication server");
			ieee802_1x_encapsulate_radius(
				hapd, sta, elems.wrapped_data,
				elems.wrapped_data_len);
			sta->fils_pending_cb = cb;
			wpa_printf(MSG_DEBUG,
				   "FILS: Will send Authentication frame once the response from authentication server is available");
			sta->flags |= WLAN_STA_PENDING_FILS_ERP;
			/* Calculate pending PMKID here so that we do not need
			 * to maintain a copy of the EAP-Initiate/Reauth
			 * message. */
			if (fils_pmkid_erp(wpa_auth_sta_key_mgmt(sta->wpa_sm),
					   elems.wrapped_data,
					   elems.wrapped_data_len,
					   sta->fils_erp_pmkid) == 0)
				sta->fils_erp_pmkid_set = 1;
			return;
#else /* CONFIG_NO_RADIUS */
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
#endif /* CONFIG_NO_RADIUS */
		}
	}

fail:
	if (cb) {
		struct wpabuf *data;
		int pub = 0;

		data = prepare_auth_resp_fils(hapd, sta, &resp, pmksa, NULL,
					      NULL, 0, &pub);
		if (!data) {
			wpa_printf(MSG_DEBUG,
				   "%s: prepare_auth_resp_fils() returned failure",
				   __func__);
		}

		cb(hapd, sta, resp, data, pub);
	}
}


static struct wpabuf *
prepare_auth_resp_fils(struct hostapd_data *hapd,
		       struct sta_info *sta, u16 *resp,
		       struct rsn_pmksa_cache_entry *pmksa,
		       struct wpabuf *erp_resp,
		       const u8 *msk, size_t msk_len,
		       int *is_pub)
{
	u8 fils_nonce[FILS_NONCE_LEN];
	size_t ielen;
	struct wpabuf *data = NULL;
	const u8 *ie;
	u8 *ie_buf = NULL;
	const u8 *pmk = NULL;
	size_t pmk_len = 0;
	u8 pmk_buf[PMK_LEN_MAX];
	struct wpabuf *pub = NULL;

	if (*resp != WLAN_STATUS_SUCCESS)
		goto fail;

	ie = wpa_auth_get_wpa_ie(hapd->wpa_auth, &ielen);
	if (!ie) {
		*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	if (pmksa) {
		/* Add PMKID of the selected PMKSA into RSNE */
		ie_buf = os_malloc(ielen + 2 + 2 + PMKID_LEN);
		if (!ie_buf) {
			*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}

		os_memcpy(ie_buf, ie, ielen);
		if (wpa_insert_pmkid(ie_buf, &ielen, pmksa->pmkid, true) < 0) {
			*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		ie = ie_buf;
	}

	if (random_get_bytes(fils_nonce, FILS_NONCE_LEN) < 0) {
		*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "RSN: Generated FILS Nonce",
		    fils_nonce, FILS_NONCE_LEN);

#ifdef CONFIG_FILS_SK_PFS
	if (sta->fils_dh_ss && sta->fils_ecdh) {
		pub = crypto_ecdh_get_pubkey(sta->fils_ecdh, 1);
		if (!pub) {
			*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
	}
#endif /* CONFIG_FILS_SK_PFS */

	data = wpabuf_alloc(1000 + ielen + (pub ? wpabuf_len(pub) : 0));
	if (!data) {
		*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	/* TODO: FILS PK */
#ifdef CONFIG_FILS_SK_PFS
	if (pub) {
		/* Finite Cyclic Group */
		wpabuf_put_le16(data, hapd->conf->fils_dh_group);

		/* Element */
		wpabuf_put_buf(data, pub);
	}
#endif /* CONFIG_FILS_SK_PFS */

	/* RSNE */
	wpabuf_put_data(data, ie, ielen);

	/* MDE when using FILS+FT (already included in ie,ielen with RSNE) */

#ifdef CONFIG_IEEE80211R_AP
	if (wpa_key_mgmt_ft(wpa_auth_sta_key_mgmt(sta->wpa_sm))) {
		/* FTE[R1KH-ID,R0KH-ID] when using FILS+FT */
		int res;

		res = wpa_auth_write_fte(hapd->wpa_auth, sta->wpa_sm,
					 wpabuf_put(data, 0),
					 wpabuf_tailroom(data));
		if (res < 0) {
			*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		wpabuf_put(data, res);
	}
#endif /* CONFIG_IEEE80211R_AP */

	/* FILS Nonce */
	wpabuf_put_u8(data, WLAN_EID_EXTENSION); /* Element ID */
	wpabuf_put_u8(data, 1 + FILS_NONCE_LEN); /* Length */
	/* Element ID Extension */
	wpabuf_put_u8(data, WLAN_EID_EXT_FILS_NONCE);
	wpabuf_put_data(data, fils_nonce, FILS_NONCE_LEN);

	/* FILS Session */
	wpabuf_put_u8(data, WLAN_EID_EXTENSION); /* Element ID */
	wpabuf_put_u8(data, 1 + FILS_SESSION_LEN); /* Length */
	/* Element ID Extension */
	wpabuf_put_u8(data, WLAN_EID_EXT_FILS_SESSION);
	wpabuf_put_data(data, sta->fils_session, FILS_SESSION_LEN);

	/* Wrapped Data */
	if (!pmksa && erp_resp) {
		wpabuf_put_u8(data, WLAN_EID_EXTENSION); /* Element ID */
		wpabuf_put_u8(data, 1 + wpabuf_len(erp_resp)); /* Length */
		/* Element ID Extension */
		wpabuf_put_u8(data, WLAN_EID_EXT_WRAPPED_DATA);
		wpabuf_put_buf(data, erp_resp);

		if (fils_rmsk_to_pmk(wpa_auth_sta_key_mgmt(sta->wpa_sm),
				     msk, msk_len, sta->fils_snonce, fils_nonce,
				     sta->fils_dh_ss ?
				     wpabuf_head(sta->fils_dh_ss) : NULL,
				     sta->fils_dh_ss ?
				     wpabuf_len(sta->fils_dh_ss) : 0,
				     pmk_buf, &pmk_len)) {
			wpa_printf(MSG_DEBUG, "FILS: Failed to derive PMK");
			*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			wpabuf_free(data);
			data = NULL;
			goto fail;
		}
		pmk = pmk_buf;

		/* Don't use DHss in PTK derivation if PMKSA caching is not
		 * used. */
		wpabuf_clear_free(sta->fils_dh_ss);
		sta->fils_dh_ss = NULL;

		if (sta->fils_erp_pmkid_set) {
			/* TODO: get PMKLifetime from WPA parameters */
			unsigned int dot11RSNAConfigPMKLifetime = 43200;
			int session_timeout;

			session_timeout = dot11RSNAConfigPMKLifetime;
			if (sta->session_timeout_set) {
				struct os_reltime now, diff;

				os_get_reltime(&now);
				os_reltime_sub(&sta->session_timeout, &now,
					       &diff);
				session_timeout = diff.sec;
			}

			sta->fils_erp_pmkid_set = 0;
			wpa_auth_add_fils_pmk_pmkid(sta->wpa_sm, pmk, pmk_len,
						    sta->fils_erp_pmkid);
			if (!hapd->conf->disable_pmksa_caching &&
			    wpa_auth_pmksa_add2(
				    hapd->wpa_auth, sta->addr,
				    pmk, pmk_len,
				    sta->fils_erp_pmkid,
				    session_timeout,
				    wpa_auth_sta_key_mgmt(sta->wpa_sm),
				    NULL) < 0) {
				wpa_printf(MSG_ERROR,
					   "FILS: Failed to add PMKSA cache entry based on ERP");
			}
		}
	} else if (pmksa) {
		pmk = pmksa->pmk;
		pmk_len = pmksa->pmk_len;
	}

	if (!pmk) {
		wpa_printf(MSG_DEBUG, "FILS: No PMK available");
		*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		wpabuf_free(data);
		data = NULL;
		goto fail;
	}

	if (fils_auth_pmk_to_ptk(sta->wpa_sm, pmk, pmk_len,
				 sta->fils_snonce, fils_nonce,
				 sta->fils_dh_ss ?
				 wpabuf_head(sta->fils_dh_ss) : NULL,
				 sta->fils_dh_ss ?
				 wpabuf_len(sta->fils_dh_ss) : 0,
				 sta->fils_g_sta, pub) < 0) {
		*resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		wpabuf_free(data);
		data = NULL;
		goto fail;
	}

fail:
	if (is_pub)
		*is_pub = pub != NULL;
	os_free(ie_buf);
	wpabuf_free(pub);
	wpabuf_clear_free(sta->fils_dh_ss);
	sta->fils_dh_ss = NULL;
#ifdef CONFIG_FILS_SK_PFS
	crypto_ecdh_deinit(sta->fils_ecdh);
	sta->fils_ecdh = NULL;
#endif /* CONFIG_FILS_SK_PFS */
	return data;
}


static void handle_auth_fils_finish(struct hostapd_data *hapd,
				    struct sta_info *sta, u16 resp,
				    struct wpabuf *data, int pub)
{
	u16 auth_alg;

	auth_alg = (pub ||
		    resp == WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED) ?
		WLAN_AUTH_FILS_SK_PFS : WLAN_AUTH_FILS_SK;
	send_auth_reply(hapd, sta, sta->addr, auth_alg, 2, resp,
			data ? wpabuf_head(data) : (u8 *) "",
			data ? wpabuf_len(data) : 0, "auth-fils-finish");
	wpabuf_free(data);

	if (resp == WLAN_STATUS_SUCCESS) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "authentication OK (FILS)");
		sta->flags |= WLAN_STA_AUTH;
		wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
		sta->auth_alg = pub ? WLAN_AUTH_FILS_SK_PFS : WLAN_AUTH_FILS_SK;
		mlme_authenticate_indication(hapd, sta);
	}
}


void ieee802_11_finish_fils_auth(struct hostapd_data *hapd,
				 struct sta_info *sta, int success,
				 struct wpabuf *erp_resp,
				 const u8 *msk, size_t msk_len)
{
	u16 resp;
	u32 flags = sta->flags;

	sta->flags &= ~(WLAN_STA_PENDING_FILS_ERP |
			WLAN_STA_PENDING_PASN_FILS_ERP);

	resp = success ? WLAN_STATUS_SUCCESS : WLAN_STATUS_UNSPECIFIED_FAILURE;

	if (flags & WLAN_STA_PENDING_FILS_ERP) {
		struct wpabuf *data;
		int pub = 0;

		if (!sta->fils_pending_cb)
			return;

		data = prepare_auth_resp_fils(hapd, sta, &resp, NULL, erp_resp,
					      msk, msk_len, &pub);
		if (!data) {
			wpa_printf(MSG_DEBUG,
				   "%s: prepare_auth_resp_fils() failure",
				   __func__);
		}
		sta->fils_pending_cb(hapd, sta, resp, data, pub);
#ifdef CONFIG_PASN
	} else if (flags & WLAN_STA_PENDING_PASN_FILS_ERP) {
		pasn_fils_auth_resp(hapd, sta, resp, erp_resp,
				    msk, msk_len);
#endif /* CONFIG_PASN */
	}
}

#endif /* CONFIG_FILS */


static int ieee802_11_allowed_address(struct hostapd_data *hapd, const u8 *addr,
				      const u8 *msg, size_t len,
				      struct radius_sta *info)
{
	int res;

	res = hostapd_allowed_address(hapd, addr, msg, len, info, 0);

	if (res == HOSTAPD_ACL_REJECT) {
		wpa_printf(MSG_DEBUG, "Station " MACSTR
			   " not allowed to authenticate",
			   MAC2STR(addr));
		return HOSTAPD_ACL_REJECT;
	}

	if (res == HOSTAPD_ACL_PENDING) {
		wpa_printf(MSG_DEBUG, "Authentication frame from " MACSTR
			   " waiting for an external authentication",
			   MAC2STR(addr));
		/* Authentication code will re-send the authentication frame
		 * after it has received (and cached) information from the
		 * external source. */
		return HOSTAPD_ACL_PENDING;
	}

	return res;
}


int ieee802_11_set_radius_info(struct hostapd_data *hapd, struct sta_info *sta,
			       int res, struct radius_sta *info)
{
	u32 session_timeout = info->session_timeout;
	u32 acct_interim_interval = info->acct_interim_interval;
	struct vlan_description *vlan_id = &info->vlan_id;
	struct hostapd_sta_wpa_psk_short *psk = info->psk;
	char *identity = info->identity;
	char *radius_cui = info->radius_cui;

	if (vlan_id->notempty &&
	    !hostapd_vlan_valid(hapd->conf->vlan, vlan_id)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_RADIUS,
			       HOSTAPD_LEVEL_INFO,
			       "Invalid VLAN %d%s received from RADIUS server",
			       vlan_id->untagged,
			       vlan_id->tagged[0] ? "+" : "");
		return -1;
	}
	if (ap_sta_set_vlan(hapd, sta, vlan_id) < 0)
		return -1;
	if (sta->vlan_id)
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_RADIUS,
			       HOSTAPD_LEVEL_INFO, "VLAN ID %d", sta->vlan_id);

	hostapd_free_psk_list(sta->psk);
	if (hapd->conf->wpa_psk_radius != PSK_RADIUS_IGNORED)
		hostapd_copy_psk_list(&sta->psk, psk);
	else
		sta->psk = NULL;

	os_free(sta->identity);
	if (identity)
		sta->identity = os_strdup(identity);
	else
		sta->identity = NULL;

	os_free(sta->radius_cui);
	if (radius_cui)
		sta->radius_cui = os_strdup(radius_cui);
	else
		sta->radius_cui = NULL;

	if (hapd->conf->acct_interim_interval == 0 && acct_interim_interval)
		sta->acct_interim_interval = acct_interim_interval;
	if (res == HOSTAPD_ACL_ACCEPT_TIMEOUT) {
		sta->session_timeout_set = 1;
		os_get_reltime(&sta->session_timeout);
		sta->session_timeout.sec += session_timeout;
		ap_sta_session_timeout(hapd, sta, session_timeout);
	} else {
		sta->session_timeout_set = 0;
		ap_sta_no_session_timeout(hapd, sta);
	}

	return 0;
}


#ifdef CONFIG_PASN
#ifdef CONFIG_FILS

static void pasn_fils_auth_resp(struct hostapd_data *hapd,
				struct sta_info *sta, u16 status,
				struct wpabuf *erp_resp,
				const u8 *msk, size_t msk_len)
{
	struct pasn_data *pasn = sta->pasn;
	struct pasn_fils *fils = &pasn->fils;
	u8 pmk[PMK_LEN_MAX];
	size_t pmk_len;
	int ret;

	wpa_printf(MSG_DEBUG, "PASN: FILS: Handle AS response - status=%u",
		   status);

	if (status != WLAN_STATUS_SUCCESS)
		goto fail;

	if (!pasn->secret) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Missing secret");
		goto fail;
	}

	if (random_get_bytes(fils->anonce, FILS_NONCE_LEN) < 0) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Failed to get ANonce");
		goto fail;
	}

	wpa_hexdump(MSG_DEBUG, "RSN: Generated FILS ANonce",
		    fils->anonce, FILS_NONCE_LEN);

	ret = fils_rmsk_to_pmk(pasn_get_akmp(pasn), msk, msk_len, fils->nonce,
			       fils->anonce, NULL, 0, pmk, &pmk_len);
	if (ret) {
		wpa_printf(MSG_DEBUG, "FILS: Failed to derive PMK");
		goto fail;
	}

	ret = pasn_pmk_to_ptk(pmk, pmk_len, sta->addr, hapd->own_addr,
			      wpabuf_head(pasn->secret),
			      wpabuf_len(pasn->secret),
			      pasn_get_ptk(sta->pasn), pasn_get_akmp(sta->pasn),
			      pasn_get_cipher(sta->pasn), sta->pasn->kdk_len);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Failed to derive PTK");
		goto fail;
	}

	if (pasn->secure_ltf) {
		ret = wpa_ltf_keyseed(pasn_get_ptk(pasn), pasn_get_akmp(pasn),
				      pasn_get_cipher(pasn));
		if (ret) {
			wpa_printf(MSG_DEBUG,
				   "PASN: FILS: Failed to derive LTF keyseed");
			goto fail;
		}
	}

	wpa_printf(MSG_DEBUG, "PASN: PTK successfully derived");

	wpabuf_free(pasn->secret);
	pasn->secret = NULL;

	fils->erp_resp = erp_resp;
	ret = handle_auth_pasn_resp(sta->pasn, hapd->own_addr, sta->addr, NULL,
				    WLAN_STATUS_SUCCESS);
	fils->erp_resp = NULL;

	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Failed to send response");
		goto fail;
	}

	fils->state = PASN_FILS_STATE_COMPLETE;
	return;
fail:
	ap_free_sta(hapd, sta);
}


static int pasn_wd_handle_fils(struct hostapd_data *hapd, struct sta_info *sta,
			       struct wpabuf *wd)
{
#ifdef CONFIG_NO_RADIUS
	wpa_printf(MSG_DEBUG, "PASN: FILS: RADIUS is not configured. Fail");
	return -1;
#else /* CONFIG_NO_RADIUS */
	struct pasn_data *pasn = sta->pasn;
	struct pasn_fils *fils = &pasn->fils;
	struct ieee802_11_elems elems;
	struct wpa_ie_data rsne_data;
	struct wpabuf *fils_wd;
	const u8 *data;
	size_t buf_len;
	u16 alg, seq, status;
	int ret;

	if (fils->state != PASN_FILS_STATE_NONE) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Not expecting wrapped data");
		return -1;
	}

	if (!wd) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: No wrapped data");
		return -1;
	}

	data = wpabuf_head_u8(wd);
	buf_len = wpabuf_len(wd);

	if (buf_len < 6) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Buffer too short. len=%zu",
			   buf_len);
		return -1;
	}

	alg = WPA_GET_LE16(data);
	seq = WPA_GET_LE16(data + 2);
	status = WPA_GET_LE16(data + 4);

	wpa_printf(MSG_DEBUG, "PASN: FILS: alg=%u, seq=%u, status=%u",
		   alg, seq, status);

	if (alg != WLAN_AUTH_FILS_SK || seq != 1 ||
	    status != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG,
			   "PASN: FILS: Dropping peer authentication");
		return -1;
	}

	data += 6;
	buf_len -= 6;

	if (ieee802_11_parse_elems(data, buf_len, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Could not parse elements");
		return -1;
	}

	if (!elems.rsn_ie || !elems.fils_nonce || !elems.fils_nonce ||
	    !elems.wrapped_data || !elems.fils_session) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Missing IEs");
		return -1;
	}

	ret = wpa_parse_wpa_ie_rsn(elems.rsn_ie - 2, elems.rsn_ie_len + 2,
				   &rsne_data);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Failed parsing RSNE");
		return -1;
	}

	ret = wpa_pasn_validate_rsne(&rsne_data);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Failed validating RSNE");
		return -1;
	}

	if (rsne_data.num_pmkid) {
		wpa_printf(MSG_DEBUG,
			   "PASN: FILS: Not expecting PMKID in RSNE");
		return -1;
	}

	wpa_hexdump(MSG_DEBUG, "PASN: FILS: Nonce", elems.fils_nonce,
		    FILS_NONCE_LEN);
	os_memcpy(fils->nonce, elems.fils_nonce, FILS_NONCE_LEN);

	wpa_hexdump(MSG_DEBUG, "PASN: FILS: Session", elems.fils_session,
		    FILS_SESSION_LEN);
	os_memcpy(fils->session, elems.fils_session, FILS_SESSION_LEN);

	fils_wd = ieee802_11_defrag(elems.wrapped_data, elems.wrapped_data_len,
				    true);

	if (!fils_wd) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Missing wrapped data");
		return -1;
	}

	if (!sta->eapol_sm)
		sta->eapol_sm = ieee802_1x_alloc_eapol_sm(hapd, sta);

	wpa_printf(MSG_DEBUG,
		   "PASN: FILS: Forward EAP-Initiate/Re-auth to AS");

	ieee802_1x_encapsulate_radius(hapd, sta, wpabuf_head(fils_wd),
				      wpabuf_len(fils_wd));

	sta->flags |= WLAN_STA_PENDING_PASN_FILS_ERP;

	fils->state = PASN_FILS_STATE_PENDING_AS;

	/*
	 * Calculate pending PMKID here so that we do not need to maintain a
	 * copy of the EAP-Initiate/Reautt message.
	 */
	fils_pmkid_erp(pasn_get_akmp(pasn),
		       wpabuf_head(fils_wd), wpabuf_len(fils_wd),
		       fils->erp_pmkid);

	wpabuf_free(fils_wd);
	return 0;
#endif /* CONFIG_NO_RADIUS */
}

#endif /* CONFIG_FILS */


static int hapd_pasn_send_mlme(void *ctx, const u8 *data, size_t data_len,
			       int noack, unsigned int freq, unsigned int wait)
{
	struct hostapd_data *hapd = ctx;

	return hostapd_drv_send_mlme(hapd, data, data_len, 0, NULL, 0, 0);
}


static void hapd_initialize_pasn(struct hostapd_data *hapd,
				 struct sta_info *sta)
{
	struct pasn_data *pasn = sta->pasn;

	pasn_register_callbacks(pasn, hapd, hapd_pasn_send_mlme, NULL);
	pasn_set_bssid(pasn, hapd->own_addr);
	pasn_set_own_addr(pasn, hapd->own_addr);
	pasn_set_peer_addr(pasn, sta->addr);
	pasn_set_wpa_key_mgmt(pasn, hapd->conf->wpa_key_mgmt);
	pasn_set_rsn_pairwise(pasn, hapd->conf->rsn_pairwise);
	pasn->pasn_groups = hapd->conf->pasn_groups;
	pasn->noauth = hapd->conf->pasn_noauth;
	if (hapd->iface->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_LTF_AP)
		pasn_enable_kdk_derivation(pasn);

#ifdef CONFIG_TESTING_OPTIONS
	pasn->corrupt_mic = hapd->conf->pasn_corrupt_mic;
	if (hapd->conf->force_kdk_derivation)
		pasn_enable_kdk_derivation(pasn);
#endif /* CONFIG_TESTING_OPTIONS */
	pasn->use_anti_clogging = use_anti_clogging(hapd);
	pasn_set_password(pasn, sae_get_password(hapd, sta, NULL, NULL,
						 &pasn->pt, NULL));
	pasn->rsn_ie = wpa_auth_get_wpa_ie(hapd->wpa_auth, &pasn->rsn_ie_len);
	pasn_set_rsnxe_ie(pasn, hostapd_wpa_ie(hapd, WLAN_EID_RSNX));
	pasn->disable_pmksa_caching = hapd->conf->disable_pmksa_caching;
	pasn_set_responder_pmksa(pasn,
				 wpa_auth_get_pmksa_cache(hapd->wpa_auth));

	pasn->comeback_after = hapd->conf->pasn_comeback_after;
	pasn->comeback_idx = hapd->comeback_idx;
	pasn->comeback_key =  hapd->comeback_key;
	pasn->comeback_pending_idx = hapd->comeback_pending_idx;
}


static int pasn_set_keys_from_cache(struct hostapd_data *hapd,
				    const u8 *own_addr, const u8 *sta_addr,
				    int cipher, int akmp)
{
	struct ptksa_cache_entry *entry;

	entry = ptksa_cache_get(hapd->ptksa, sta_addr, cipher);
	if (!entry) {
		wpa_printf(MSG_DEBUG, "PASN: peer " MACSTR
			   " not present in PTKSA cache", MAC2STR(sta_addr));
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
		   MAC2STR(sta_addr));
	hostapd_drv_set_secure_ranging_ctx(hapd, own_addr, sta_addr, cipher,
					   entry->ptk.tk_len, entry->ptk.tk,
					   entry->ptk.ltf_keyseed_len,
					   entry->ptk.ltf_keyseed, 0);

	return 0;
}


static void hapd_pasn_update_params(struct hostapd_data *hapd,
				    struct sta_info *sta,
				    const struct ieee80211_mgmt *mgmt,
				    size_t len)
{
	struct pasn_data *pasn = sta->pasn;
	struct ieee802_11_elems elems;
	struct wpa_ie_data rsn_data;
#ifdef CONFIG_FILS
	struct wpa_pasn_params_data pasn_params;
	struct wpabuf *wrapped_data = NULL;
#endif /* CONFIG_FILS */
	int akmp;

	if (ieee802_11_parse_elems(mgmt->u.auth.variable,
				   len - offsetof(struct ieee80211_mgmt,
						  u.auth.variable),
				   &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Failed parsing Authentication frame");
		return;
	}

	if (!elems.rsn_ie ||
	    wpa_parse_wpa_ie_rsn(elems.rsn_ie - 2, elems.rsn_ie_len + 2,
				 &rsn_data)) {
		wpa_printf(MSG_DEBUG, "PASN: Failed parsing RSNE");
		return;
	}

	if (!(rsn_data.key_mgmt & pasn->wpa_key_mgmt) ||
	    !(rsn_data.pairwise_cipher & pasn->rsn_pairwise)) {
		wpa_printf(MSG_DEBUG, "PASN: Mismatch in AKMP/cipher");
		return;
	}

	pasn_set_akmp(pasn, rsn_data.key_mgmt);
	pasn_set_cipher(pasn, rsn_data.pairwise_cipher);

	if (pasn->derive_kdk &&
	    !ieee802_11_rsnx_capab_len(elems.rsnxe, elems.rsnxe_len,
				       WLAN_RSNX_CAPAB_SECURE_LTF))
		pasn_disable_kdk_derivation(pasn);
#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->force_kdk_derivation)
		pasn_enable_kdk_derivation(pasn);
#endif /* CONFIG_TESTING_OPTIONS */
	akmp = pasn_get_akmp(pasn);

	if (wpa_key_mgmt_ft(akmp) && rsn_data.num_pmkid) {
#ifdef CONFIG_IEEE80211R_AP
		pasn->pmk_r1_len = 0;
		wpa_ft_fetch_pmk_r1(hapd->wpa_auth, sta->addr,
				    rsn_data.pmkid,
				    pasn->pmk_r1, &pasn->pmk_r1_len, NULL,
				    NULL, NULL, NULL,
				    NULL, NULL, NULL);
#endif /* CONFIG_IEEE80211R_AP */
	}
#ifdef CONFIG_FILS
	if (akmp != WPA_KEY_MGMT_FILS_SHA256 &&
	    akmp != WPA_KEY_MGMT_FILS_SHA384)
		return;
	if (!elems.pasn_params ||
	    wpa_pasn_parse_parameter_ie(elems.pasn_params - 3,
					elems.pasn_params_len + 3,
					false, &pasn_params)) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Failed validation of PASN Parameters element");
		return;
	}
	if (pasn_params.wrapped_data_format != WPA_PASN_WRAPPED_DATA_NO) {
		wrapped_data = ieee802_11_defrag(elems.wrapped_data,
						 elems.wrapped_data_len, true);
		if (!wrapped_data) {
			wpa_printf(MSG_DEBUG, "PASN: Missing wrapped data");
			return;
		}
		if (pasn_wd_handle_fils(hapd, sta, wrapped_data))
			wpa_printf(MSG_DEBUG,
				   "PASN: Failed processing FILS wrapped data");
		else
			pasn->fils_wd_valid = true;
	}
	wpabuf_free(wrapped_data);
#endif /* CONFIG_FILS */
}


static void handle_auth_pasn(struct hostapd_data *hapd, struct sta_info *sta,
			     const struct ieee80211_mgmt *mgmt, size_t len,
			     u16 trans_seq, u16 status)
{
	if (hapd->conf->wpa != WPA_PROTO_RSN) {
		wpa_printf(MSG_INFO, "PASN: RSN is not configured");
		return;
	}

	wpa_printf(MSG_INFO, "PASN authentication: sta=" MACSTR,
		   MAC2STR(sta->addr));

	if (trans_seq == 1) {
		if (sta->pasn) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Not expecting transaction == 1");
			return;
		}

		if (status != WLAN_STATUS_SUCCESS) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Failure status in transaction == 1");
			return;
		}

		sta->pasn = pasn_data_init();
		if (!sta->pasn) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Failed to allocate PASN context");
			return;
		}

		hapd_initialize_pasn(hapd, sta);

		hapd_pasn_update_params(hapd, sta, mgmt, len);
		if (handle_auth_pasn_1(sta->pasn, hapd->own_addr,
				       sta->addr, mgmt, len) < 0)
			ap_free_sta(hapd, sta);
	} else if (trans_seq == 3) {
		if (!sta->pasn) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Not expecting transaction == 3");
			return;
		}

		if (status != WLAN_STATUS_SUCCESS) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Failure status in transaction == 3");
			ap_free_sta_pasn(hapd, sta);
			return;
		}

		if (handle_auth_pasn_3(sta->pasn, hapd->own_addr,
				       sta->addr, mgmt, len) == 0) {
			ptksa_cache_add(hapd->ptksa, hapd->own_addr, sta->addr,
					pasn_get_cipher(sta->pasn), 43200,
					pasn_get_ptk(sta->pasn), NULL, NULL,
					pasn_get_akmp(sta->pasn));

			pasn_set_keys_from_cache(hapd, hapd->own_addr,
						 sta->addr,
						 pasn_get_cipher(sta->pasn),
						 pasn_get_akmp(sta->pasn));
		}
		ap_free_sta(hapd, sta);
	} else {
		wpa_printf(MSG_DEBUG,
			   "PASN: Invalid transaction %u - ignore", trans_seq);
	}
}

#endif /* CONFIG_PASN */


static void handle_auth(struct hostapd_data *hapd,
			const struct ieee80211_mgmt *mgmt, size_t len,
			int rssi, int from_queue)
{
	u16 auth_alg, auth_transaction, status_code;
	u16 resp = WLAN_STATUS_SUCCESS;
	struct sta_info *sta = NULL;
	int res, reply_res;
	u16 fc;
	const u8 *challenge = NULL;
	u8 resp_ies[2 + WLAN_AUTH_CHALLENGE_LEN];
	size_t resp_ies_len = 0;
	u16 seq_ctrl;
	struct radius_sta rad_info;
	const u8 *dst, *sa;
#ifdef CONFIG_IEEE80211BE
	bool mld_sta = false;
#endif /* CONFIG_IEEE80211BE */

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.auth)) {
		wpa_printf(MSG_INFO, "handle_auth - too short payload (len=%lu)",
			   (unsigned long) len);
		return;
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->iconf->ignore_auth_probability > 0.0 &&
	    drand48() < hapd->iconf->ignore_auth_probability) {
		wpa_printf(MSG_INFO,
			   "TESTING: ignoring auth frame from " MACSTR,
			   MAC2STR(mgmt->sa));
		return;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	sa = mgmt->sa;
#ifdef CONFIG_IEEE80211BE
	/*
	 * Handle MLO authentication before the station is added to hostapd and
	 * the driver so that the station MLD MAC address would be used in both
	 * hostapd and the driver.
	 */
	sa = hostapd_process_ml_auth(hapd, mgmt, len);
	if (sa)
		mld_sta = true;
	else
		sa = mgmt->sa;
#endif /* CONFIG_IEEE80211BE */

	auth_alg = le_to_host16(mgmt->u.auth.auth_alg);
	auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);
	status_code = le_to_host16(mgmt->u.auth.status_code);
	fc = le_to_host16(mgmt->frame_control);
	seq_ctrl = le_to_host16(mgmt->seq_ctrl);

	if (len >= IEEE80211_HDRLEN + sizeof(mgmt->u.auth) +
	    2 + WLAN_AUTH_CHALLENGE_LEN &&
	    mgmt->u.auth.variable[0] == WLAN_EID_CHALLENGE &&
	    mgmt->u.auth.variable[1] == WLAN_AUTH_CHALLENGE_LEN)
		challenge = &mgmt->u.auth.variable[2];

	wpa_printf(MSG_DEBUG, "authentication: STA=" MACSTR " auth_alg=%d "
		   "auth_transaction=%d status_code=%d wep=%d%s "
		   "seq_ctrl=0x%x%s%s",
		   MAC2STR(sa), auth_alg, auth_transaction,
		   status_code, !!(fc & WLAN_FC_ISWEP),
		   challenge ? " challenge" : "",
		   seq_ctrl, (fc & WLAN_FC_RETRY) ? " retry" : "",
		   from_queue ? " (from queue)" : "");

#ifdef CONFIG_NO_RC4
	if (auth_alg == WLAN_AUTH_SHARED_KEY) {
		wpa_printf(MSG_INFO,
			   "Unsupported authentication algorithm (%d)",
			   auth_alg);
		resp = WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG;
		goto fail;
	}
#endif /* CONFIG_NO_RC4 */

	if (hapd->tkip_countermeasures) {
		wpa_printf(MSG_DEBUG,
			   "Ongoing TKIP countermeasures (Michael MIC failure) - reject authentication");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	if (!(((hapd->conf->auth_algs & WPA_AUTH_ALG_OPEN) &&
	       auth_alg == WLAN_AUTH_OPEN) ||
#ifdef CONFIG_IEEE80211R_AP
	      (hapd->conf->wpa && wpa_key_mgmt_ft(hapd->conf->wpa_key_mgmt) &&
	       auth_alg == WLAN_AUTH_FT) ||
#endif /* CONFIG_IEEE80211R_AP */
#ifdef CONFIG_SAE
	      (hapd->conf->wpa && wpa_key_mgmt_sae(hapd->conf->wpa_key_mgmt) &&
	       auth_alg == WLAN_AUTH_SAE) ||
#endif /* CONFIG_SAE */
#ifdef CONFIG_FILS
	      (hapd->conf->wpa && wpa_key_mgmt_fils(hapd->conf->wpa_key_mgmt) &&
	       auth_alg == WLAN_AUTH_FILS_SK) ||
	      (hapd->conf->wpa && wpa_key_mgmt_fils(hapd->conf->wpa_key_mgmt) &&
	       hapd->conf->fils_dh_group &&
	       auth_alg == WLAN_AUTH_FILS_SK_PFS) ||
#endif /* CONFIG_FILS */
#ifdef CONFIG_PASN
	      (hapd->conf->wpa &&
	       (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_PASN) &&
	       auth_alg == WLAN_AUTH_PASN) ||
#endif /* CONFIG_PASN */
	      ((hapd->conf->auth_algs & WPA_AUTH_ALG_SHARED) &&
	       auth_alg == WLAN_AUTH_SHARED_KEY))) {
		wpa_printf(MSG_INFO, "Unsupported authentication algorithm (%d)",
			   auth_alg);
		resp = WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG;
		goto fail;
	}

	if (!(auth_transaction == 1 || auth_alg == WLAN_AUTH_SAE ||
#ifdef CONFIG_PASN
	      (auth_alg == WLAN_AUTH_PASN && auth_transaction == 3) ||
#endif /* CONFIG_PASN */
	      (auth_alg == WLAN_AUTH_SHARED_KEY && auth_transaction == 3))) {
		wpa_printf(MSG_INFO, "Unknown authentication transaction number (%d)",
			   auth_transaction);
		resp = WLAN_STATUS_UNKNOWN_AUTH_TRANSACTION;
		goto fail;
	}

	if (ether_addr_equal(mgmt->sa, hapd->own_addr)) {
		wpa_printf(MSG_INFO, "Station " MACSTR " not allowed to authenticate",
			   MAC2STR(sa));
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

#ifdef CONFIG_IEEE80211BE
	if (mld_sta &&
	    (ether_addr_equal(sa, hapd->own_addr) ||
	     ether_addr_equal(sa, hapd->mld->mld_addr))) {
		wpa_printf(MSG_INFO,
			   "Station " MACSTR " not allowed to authenticate",
			   MAC2STR(sa));
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}
#endif /* CONFIG_IEEE80211BE */

	if (hapd->conf->no_auth_if_seen_on) {
		struct hostapd_data *other;

		other = sta_track_seen_on(hapd->iface, sa,
					  hapd->conf->no_auth_if_seen_on);
		if (other) {
			u8 *pos;
			u32 info;
			u8 op_class, channel, phytype;

			wpa_printf(MSG_DEBUG, "%s: Reject authentication from "
				   MACSTR " since STA has been seen on %s",
				   hapd->conf->iface, MAC2STR(sa),
				   hapd->conf->no_auth_if_seen_on);

			resp = WLAN_STATUS_REJECTED_WITH_SUGGESTED_BSS_TRANSITION;
			pos = &resp_ies[0];
			*pos++ = WLAN_EID_NEIGHBOR_REPORT;
			*pos++ = 13;
			os_memcpy(pos, other->own_addr, ETH_ALEN);
			pos += ETH_ALEN;
			info = 0; /* TODO: BSSID Information */
			WPA_PUT_LE32(pos, info);
			pos += 4;
			if (other->iconf->hw_mode == HOSTAPD_MODE_IEEE80211AD)
				phytype = 8; /* dmg */
			else if (other->iconf->ieee80211ac)
				phytype = 9; /* vht */
			else if (other->iconf->ieee80211n)
				phytype = 7; /* ht */
			else if (other->iconf->hw_mode ==
				 HOSTAPD_MODE_IEEE80211A)
				phytype = 4; /* ofdm */
			else if (other->iconf->hw_mode ==
				 HOSTAPD_MODE_IEEE80211G)
				phytype = 6; /* erp */
			else
				phytype = 5; /* hrdsss */
			if (ieee80211_freq_to_channel_ext(
				    hostapd_hw_get_freq(other,
							other->iconf->channel),
				    other->iconf->secondary_channel,
				    other->iconf->ieee80211ac,
				    &op_class, &channel) == NUM_HOSTAPD_MODES) {
				op_class = 0;
				channel = other->iconf->channel;
			}
			*pos++ = op_class;
			*pos++ = channel;
			*pos++ = phytype;
			resp_ies_len = pos - &resp_ies[0];
			goto fail;
		}
	}

	res = ieee802_11_allowed_address(hapd, sa, (const u8 *) mgmt, len,
					 &rad_info);
	if (res == HOSTAPD_ACL_REJECT) {
		wpa_msg(hapd->msg_ctx, MSG_DEBUG,
			"Ignore Authentication frame from " MACSTR
			" due to ACL reject", MAC2STR(sa));
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}
	if (res == HOSTAPD_ACL_PENDING)
		return;

#ifdef CONFIG_SAE
	if (auth_alg == WLAN_AUTH_SAE && !from_queue &&
	    (auth_transaction == 1 ||
	     (auth_transaction == 2 && auth_sae_queued_addr(hapd, sa)))) {
		/* Handle SAE Authentication commit message through a queue to
		 * provide more control for postponing the needed heavy
		 * processing under a possible DoS attack scenario. In addition,
		 * queue SAE Authentication confirm message if there happens to
		 * be a queued commit message from the same peer. This is needed
		 * to avoid reordering Authentication frames within the same
		 * SAE exchange. */
		auth_sae_queue(hapd, mgmt, len, rssi);
		return;
	}
#endif /* CONFIG_SAE */

	sta = ap_get_sta(hapd, sa);
	if (sta) {
		sta->flags &= ~WLAN_STA_PENDING_FILS_ERP;
		sta->ft_over_ds = 0;
		if ((fc & WLAN_FC_RETRY) &&
		    sta->last_seq_ctrl != WLAN_INVALID_MGMT_SEQ &&
		    sta->last_seq_ctrl == seq_ctrl &&
		    sta->last_subtype == WLAN_FC_STYPE_AUTH) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG,
				       "Drop repeated authentication frame seq_ctrl=0x%x",
				       seq_ctrl);
			return;
		}
#ifdef CONFIG_PASN
		if (auth_alg == WLAN_AUTH_PASN &&
		    (sta->flags & WLAN_STA_ASSOC)) {
			wpa_printf(MSG_DEBUG,
				   "PASN: auth: Existing station: " MACSTR,
				   MAC2STR(sta->addr));
			return;
		}
#endif /* CONFIG_PASN */
	} else {
#ifdef CONFIG_MESH
		if (hapd->conf->mesh & MESH_ENABLED) {
			/* if the mesh peer is not available, we don't do auth.
			 */
			wpa_printf(MSG_DEBUG, "Mesh peer " MACSTR
				   " not yet known - drop Authentication frame",
				   MAC2STR(sa));
			/*
			 * Save a copy of the frame so that it can be processed
			 * if a new peer entry is added shortly after this.
			 */
			wpabuf_free(hapd->mesh_pending_auth);
			hapd->mesh_pending_auth = wpabuf_alloc_copy(mgmt, len);
			os_get_reltime(&hapd->mesh_pending_auth_time);
			return;
		}
#endif /* CONFIG_MESH */

		sta = ap_sta_add(hapd, sa);
		if (!sta) {
			wpa_printf(MSG_DEBUG, "ap_sta_add() failed");
			resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
			goto fail;
		}
	}

#ifdef CONFIG_IEEE80211BE
	/* Set the non-AP MLD information based on the initial Authentication
	 * frame. Once the STA entry has been added to the driver, the driver
	 * will translate addresses in the frame and we need to avoid overriding
	 * peer_addr based on mgmt->sa which would have been translated to the
	 * MLD MAC address. */
	if (!sta->added_unassoc && auth_transaction == 1) {
		ap_sta_free_sta_profile(&sta->mld_info);
		os_memset(&sta->mld_info, 0, sizeof(sta->mld_info));

		if (mld_sta) {
			u8 link_id = hapd->mld_link_id;

			ap_sta_set_mld(sta, true);
			sta->mld_assoc_link_id = link_id;

			/*
			 * Set the MLD address as the station address and the
			 * station addresses.
			 */
			os_memcpy(sta->mld_info.common_info.mld_addr, sa,
				  ETH_ALEN);
			os_memcpy(sta->mld_info.links[link_id].peer_addr,
				  mgmt->sa, ETH_ALEN);
			os_memcpy(sta->mld_info.links[link_id].local_addr,
				  hapd->own_addr, ETH_ALEN);
		}
	}
#endif /* CONFIG_IEEE80211BE */

	sta->last_seq_ctrl = seq_ctrl;
	sta->last_subtype = WLAN_FC_STYPE_AUTH;
#ifdef CONFIG_MBO
	sta->auth_rssi = rssi;
#endif /* CONFIG_MBO */

	res = ieee802_11_set_radius_info(hapd, sta, res, &rad_info);
	if (res) {
		wpa_printf(MSG_DEBUG, "ieee802_11_set_radius_info() failed");
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	sta->flags &= ~WLAN_STA_PREAUTH;
	ieee802_1x_notify_pre_auth(sta->eapol_sm, 0);

	/*
	 * If the driver supports full AP client state, add a station to the
	 * driver before sending authentication reply to make sure the driver
	 * has resources, and not to go through the entire authentication and
	 * association handshake, and fail it at the end.
	 *
	 * If this is not the first transaction, in a multi-step authentication
	 * algorithm, the station already exists in the driver
	 * (sta->added_unassoc = 1) so skip it.
	 *
	 * In mesh mode, the station was already added to the driver when the
	 * NEW_PEER_CANDIDATE event is received.
	 *
	 * If PMF was negotiated for the existing association, skip this to
	 * avoid dropping the STA entry and the associated keys. This is needed
	 * to allow the original connection work until the attempt can complete
	 * (re)association, so that unprotected Authentication frame cannot be
	 * used to bypass PMF protection.
	 *
	 * PASN authentication does not require adding/removing station to the
	 * driver so skip this flow in case of PASN authentication.
	 */
	if (FULL_AP_CLIENT_STATE_SUPP(hapd->iface->drv_flags) &&
	    (!(sta->flags & WLAN_STA_MFP) || !ap_sta_is_authorized(sta)) &&
	    !(hapd->conf->mesh & MESH_ENABLED) &&
	    !(sta->added_unassoc) && auth_alg != WLAN_AUTH_PASN) {
		if (ap_sta_re_add(hapd, sta) < 0) {
			resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
			goto fail;
		}
	}

	switch (auth_alg) {
	case WLAN_AUTH_OPEN:
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "authentication OK (open system)");
		sta->flags |= WLAN_STA_AUTH;
		wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
		sta->auth_alg = WLAN_AUTH_OPEN;
		mlme_authenticate_indication(hapd, sta);
		break;
#ifdef CONFIG_WEP
#ifndef CONFIG_NO_RC4
	case WLAN_AUTH_SHARED_KEY:
		resp = auth_shared_key(hapd, sta, auth_transaction, challenge,
				       fc & WLAN_FC_ISWEP);
		if (resp != 0)
			wpa_printf(MSG_DEBUG,
				   "auth_shared_key() failed: status=%d", resp);
		sta->auth_alg = WLAN_AUTH_SHARED_KEY;
		mlme_authenticate_indication(hapd, sta);
		if (sta->challenge && auth_transaction == 1) {
			resp_ies[0] = WLAN_EID_CHALLENGE;
			resp_ies[1] = WLAN_AUTH_CHALLENGE_LEN;
			os_memcpy(resp_ies + 2, sta->challenge,
				  WLAN_AUTH_CHALLENGE_LEN);
			resp_ies_len = 2 + WLAN_AUTH_CHALLENGE_LEN;
		}
		break;
#endif /* CONFIG_NO_RC4 */
#endif /* CONFIG_WEP */
#ifdef CONFIG_IEEE80211R_AP
	case WLAN_AUTH_FT:
		sta->auth_alg = WLAN_AUTH_FT;
		if (sta->wpa_sm == NULL)
			sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,
							sta->addr, NULL);
		if (sta->wpa_sm == NULL) {
			wpa_printf(MSG_DEBUG, "FT: Failed to initialize WPA "
				   "state machine");
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		wpa_ft_process_auth(sta->wpa_sm,
				    auth_transaction, mgmt->u.auth.variable,
				    len - IEEE80211_HDRLEN -
				    sizeof(mgmt->u.auth),
				    handle_auth_ft_finish, hapd);
		/* handle_auth_ft_finish() callback will complete auth. */
		return;
#endif /* CONFIG_IEEE80211R_AP */
#ifdef CONFIG_SAE
	case WLAN_AUTH_SAE:
#ifdef CONFIG_MESH
		if (status_code == WLAN_STATUS_SUCCESS &&
		    hapd->conf->mesh & MESH_ENABLED) {
			if (sta->wpa_sm == NULL)
				sta->wpa_sm =
					wpa_auth_sta_init(hapd->wpa_auth,
							  sta->addr, NULL);
			if (sta->wpa_sm == NULL) {
				wpa_printf(MSG_DEBUG,
					   "SAE: Failed to initialize WPA state machine");
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto fail;
			}
		}
#endif /* CONFIG_MESH */
		handle_auth_sae(hapd, sta, mgmt, len, auth_transaction,
				status_code);
		return;
#endif /* CONFIG_SAE */
#ifdef CONFIG_FILS
	case WLAN_AUTH_FILS_SK:
	case WLAN_AUTH_FILS_SK_PFS:
		handle_auth_fils(hapd, sta, mgmt->u.auth.variable,
				 len - IEEE80211_HDRLEN - sizeof(mgmt->u.auth),
				 auth_alg, auth_transaction, status_code,
				 handle_auth_fils_finish);
		return;
#endif /* CONFIG_FILS */
#ifdef CONFIG_PASN
	case WLAN_AUTH_PASN:
		handle_auth_pasn(hapd, sta, mgmt, len, auth_transaction,
				 status_code);
		return;
#endif /* CONFIG_PASN */
	}

 fail:
	dst = mgmt->sa;

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta))
		dst = sta->addr;
#endif /* CONFIG_IEEE80211BE */

	reply_res = send_auth_reply(hapd, sta, dst, auth_alg,
				    auth_alg == WLAN_AUTH_SAE ?
				    auth_transaction : auth_transaction + 1,
				    resp, resp_ies, resp_ies_len,
				    "handle-auth");

	if (sta && sta->added_unassoc && (resp != WLAN_STATUS_SUCCESS ||
					  reply_res != WLAN_STATUS_SUCCESS)) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		sta->added_unassoc = 0;
	}
}


static u8 hostapd_max_bssid_indicator(struct hostapd_data *hapd)
{
	size_t num_bss_nontx;
	u8 max_bssid_ind = 0;

	if (!hapd->iconf->mbssid || hapd->iface->num_bss <= 1)
		return 0;

	num_bss_nontx = hapd->iface->num_bss - 1;
	while (num_bss_nontx > 0) {
		max_bssid_ind++;
		num_bss_nontx >>= 1;
	}
	return max_bssid_ind;
}


static u32 hostapd_get_aid_word(struct hostapd_data *hapd,
				struct sta_info *sta, int i)
{
#ifdef CONFIG_IEEE80211BE
	u32 aid_word = 0;

	/* Do not assign an AID that is in use on any of the affiliated links
	 * when finding an AID for a non-AP MLD. */
	if (hapd->conf->mld_ap && sta->mld_info.mld_sta) {
		int j;

		for (j = 0; j < MAX_NUM_MLD_LINKS; j++) {
			struct hostapd_data *link_bss;

			if (!sta->mld_info.links[j].valid)
				continue;

			link_bss = hostapd_mld_get_link_bss(hapd, j);
			if (!link_bss) {
				/* This shouldn't happen, just skip */
				wpa_printf(MSG_ERROR,
					   "MLD: Failed to get link BSS for AID");
				continue;
			}

			aid_word |= link_bss->sta_aid[i];
		}

		return aid_word;
	}
#endif /* CONFIG_IEEE80211BE */

	return hapd->sta_aid[i];
}


int hostapd_get_aid(struct hostapd_data *hapd, struct sta_info *sta)
{
	int i, j = 32, aid;

	/* Transmitted and non-transmitted BSSIDs share the same AID pool, so
	 * use the shared storage in the transmitted BSS to find the next
	 * available value. */
	hapd = hostapd_mbssid_get_tx_bss(hapd);

	/* get a unique AID */
	if (sta->aid > 0) {
		wpa_printf(MSG_DEBUG, "  old AID %d", sta->aid);
		return 0;
	}

	if (TEST_FAIL())
		return -1;

	for (i = 0; i < AID_WORDS; i++) {
		u32 aid_word = hostapd_get_aid_word(hapd, sta, i);

		if (aid_word == (u32) -1)
			continue;
		for (j = 0; j < 32; j++) {
			if (!(aid_word & BIT(j)))
				break;
		}
		if (j < 32)
			break;
	}
	if (j == 32)
		return -1;
	aid = i * 32 + j + (1 << hostapd_max_bssid_indicator(hapd));
	if (aid > 2007)
		return -1;

	sta->aid = aid;
	hapd->sta_aid[i] |= BIT(j);
	wpa_printf(MSG_DEBUG, "  new AID %d", sta->aid);
	return 0;
}


static u16 check_ssid(struct hostapd_data *hapd, struct sta_info *sta,
		      const u8 *ssid_ie, size_t ssid_ie_len)
{
	if (ssid_ie == NULL)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	if (ssid_ie_len != hapd->conf->ssid.ssid_len ||
	    os_memcmp(ssid_ie, hapd->conf->ssid.ssid, ssid_ie_len) != 0) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "Station tried to associate with unknown SSID "
			       "'%s'", wpa_ssid_txt(ssid_ie, ssid_ie_len));
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	return WLAN_STATUS_SUCCESS;
}


static u16 check_wmm(struct hostapd_data *hapd, struct sta_info *sta,
		     const u8 *wmm_ie, size_t wmm_ie_len)
{
	sta->flags &= ~WLAN_STA_WMM;
	sta->qosinfo = 0;
	if (wmm_ie && hapd->conf->wmm_enabled) {
		struct wmm_information_element *wmm;

		if (!hostapd_eid_wmm_valid(hapd, wmm_ie, wmm_ie_len)) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_WPA,
				       HOSTAPD_LEVEL_DEBUG,
				       "invalid WMM element in association "
				       "request");
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
		}

		sta->flags |= WLAN_STA_WMM;
		wmm = (struct wmm_information_element *) wmm_ie;
		sta->qosinfo = wmm->qos_info;
	}
	return WLAN_STATUS_SUCCESS;
}

static u16 check_multi_ap(struct hostapd_data *hapd, struct sta_info *sta,
			  const u8 *multi_ap_ie, size_t multi_ap_len)
{
	struct multi_ap_params multi_ap;
	u16 status;

	sta->flags &= ~WLAN_STA_MULTI_AP;

	if (!hapd->conf->multi_ap)
		return WLAN_STATUS_SUCCESS;

	if (!multi_ap_ie) {
		if (!(hapd->conf->multi_ap & FRONTHAUL_BSS)) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_INFO,
				       "Non-Multi-AP STA tries to associate with backhaul-only BSS");
			return WLAN_STATUS_ASSOC_DENIED_UNSPEC;
		}

		return WLAN_STATUS_SUCCESS;
	}

	status = check_multi_ap_ie(multi_ap_ie + 4, multi_ap_len - 4,
				   &multi_ap);
	if (status != WLAN_STATUS_SUCCESS)
		return status;

	if (multi_ap.capability && multi_ap.capability != MULTI_AP_BACKHAUL_STA)
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "Multi-AP IE with unexpected value 0x%02x",
			       multi_ap.capability);

	if (multi_ap.profile == MULTI_AP_PROFILE_1 &&
	    (hapd->conf->multi_ap_client_disallow &
	     PROFILE1_CLIENT_ASSOC_DISALLOW)) {
		hostapd_logger(hapd, sta->addr,
			       HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "Multi-AP Profile-1 clients not allowed");
		return WLAN_STATUS_ASSOC_DENIED_UNSPEC;
	}

	if (multi_ap.profile >= MULTI_AP_PROFILE_2 &&
	    (hapd->conf->multi_ap_client_disallow &
	     PROFILE2_CLIENT_ASSOC_DISALLOW)) {
		hostapd_logger(hapd, sta->addr,
			       HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "Multi-AP Profile-2 clients not allowed");
		return WLAN_STATUS_ASSOC_DENIED_UNSPEC;
	}

	if (!(multi_ap.capability & MULTI_AP_BACKHAUL_STA)) {
		if (hapd->conf->multi_ap & FRONTHAUL_BSS)
			return WLAN_STATUS_SUCCESS;

		hostapd_logger(hapd, sta->addr,
			       HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "Non-Multi-AP STA tries to associate with backhaul-only BSS");
		return WLAN_STATUS_ASSOC_DENIED_UNSPEC;
	}

	if (!(hapd->conf->multi_ap & BACKHAUL_BSS))
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "Backhaul STA tries to associate with fronthaul-only BSS");

	sta->flags |= WLAN_STA_MULTI_AP;
	return WLAN_STATUS_SUCCESS;
}


static u16 copy_supp_rates(struct hostapd_data *hapd, struct sta_info *sta,
			   struct ieee802_11_elems *elems)
{
	/* Supported rates not used in IEEE 802.11ad/DMG */
	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211AD)
		return WLAN_STATUS_SUCCESS;

	if (!elems->supp_rates) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "No supported rates element in AssocReq");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	if (elems->supp_rates_len + elems->ext_supp_rates_len >
	    sizeof(sta->supported_rates)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "Invalid supported rates element length %d+%d",
			       elems->supp_rates_len,
			       elems->ext_supp_rates_len);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	sta->supported_rates_len = merge_byte_arrays(
		sta->supported_rates, sizeof(sta->supported_rates),
		elems->supp_rates, elems->supp_rates_len,
		elems->ext_supp_rates, elems->ext_supp_rates_len);

	return WLAN_STATUS_SUCCESS;
}


#ifdef CONFIG_OWE

static int owe_group_supported(struct hostapd_data *hapd, u16 group)
{
	int i;
	int *groups = hapd->conf->owe_groups;

	if (group != 19 && group != 20 && group != 21)
		return 0;

	if (!groups)
		return 1;

	for (i = 0; groups[i] > 0; i++) {
		if (groups[i] == group)
			return 1;
	}

	return 0;
}


static u16 owe_process_assoc_req(struct hostapd_data *hapd,
				 struct sta_info *sta, const u8 *owe_dh,
				 u8 owe_dh_len)
{
	struct wpabuf *secret, *pub, *hkey;
	int res;
	u8 prk[SHA512_MAC_LEN], pmkid[SHA512_MAC_LEN];
	const char *info = "OWE Key Generation";
	const u8 *addr[2];
	size_t len[2];
	u16 group;
	size_t hash_len, prime_len;

	if (wpa_auth_sta_get_pmksa(sta->wpa_sm)) {
		wpa_printf(MSG_DEBUG, "OWE: Using PMKSA caching");
		return WLAN_STATUS_SUCCESS;
	}

	group = WPA_GET_LE16(owe_dh);
	if (!owe_group_supported(hapd, group)) {
		wpa_printf(MSG_DEBUG, "OWE: Unsupported DH group %u", group);
		return WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;
	}
	if (group == 19)
		prime_len = 32;
	else if (group == 20)
		prime_len = 48;
	else if (group == 21)
		prime_len = 66;
	else
		return WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;

	if (sta->owe_group == group && sta->owe_ecdh) {
		/* This is a workaround for mac80211 behavior of retransmitting
		 * the Association Request frames multiple times if the link
		 * layer retries (i.e., seq# remains same) fail. The mac80211
		 * initiated retransmission will use a different seq# and as
		 * such, will go through duplicate detection. If we were to
		 * change our DH key for that attempt, there would be two
		 * different DH shared secrets and the STA would likely select
		 * the wrong one. */
		wpa_printf(MSG_DEBUG,
			   "OWE: Try to reuse own previous DH key since the STA tried to go through OWE association again");
	} else {
		crypto_ecdh_deinit(sta->owe_ecdh);
		sta->owe_ecdh = crypto_ecdh_init(group);
	}
	if (!sta->owe_ecdh)
		return WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;
	sta->owe_group = group;

	secret = crypto_ecdh_set_peerkey(sta->owe_ecdh, 0, owe_dh + 2,
					 owe_dh_len - 2);
	secret = wpabuf_zeropad(secret, prime_len);
	if (!secret) {
		wpa_printf(MSG_DEBUG, "OWE: Invalid peer DH public key");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
	wpa_hexdump_buf_key(MSG_DEBUG, "OWE: DH shared secret", secret);

	/* prk = HKDF-extract(C | A | group, z) */

	pub = crypto_ecdh_get_pubkey(sta->owe_ecdh, 0);
	if (!pub) {
		wpabuf_clear_free(secret);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	/* PMKID = Truncate-128(Hash(C | A)) */
	addr[0] = owe_dh + 2;
	len[0] = owe_dh_len - 2;
	addr[1] = wpabuf_head(pub);
	len[1] = wpabuf_len(pub);
	if (group == 19) {
		res = sha256_vector(2, addr, len, pmkid);
		hash_len = SHA256_MAC_LEN;
	} else if (group == 20) {
		res = sha384_vector(2, addr, len, pmkid);
		hash_len = SHA384_MAC_LEN;
	} else if (group == 21) {
		res = sha512_vector(2, addr, len, pmkid);
		hash_len = SHA512_MAC_LEN;
	} else {
		wpabuf_free(pub);
		wpabuf_clear_free(secret);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
	pub = wpabuf_zeropad(pub, prime_len);
	if (res < 0 || !pub) {
		wpabuf_free(pub);
		wpabuf_clear_free(secret);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	hkey = wpabuf_alloc(owe_dh_len - 2 + wpabuf_len(pub) + 2);
	if (!hkey) {
		wpabuf_free(pub);
		wpabuf_clear_free(secret);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	wpabuf_put_data(hkey, owe_dh + 2, owe_dh_len - 2); /* C */
	wpabuf_put_buf(hkey, pub); /* A */
	wpabuf_free(pub);
	wpabuf_put_le16(hkey, group); /* group */
	if (group == 19)
		res = hmac_sha256(wpabuf_head(hkey), wpabuf_len(hkey),
				  wpabuf_head(secret), wpabuf_len(secret), prk);
	else if (group == 20)
		res = hmac_sha384(wpabuf_head(hkey), wpabuf_len(hkey),
				  wpabuf_head(secret), wpabuf_len(secret), prk);
	else if (group == 21)
		res = hmac_sha512(wpabuf_head(hkey), wpabuf_len(hkey),
				  wpabuf_head(secret), wpabuf_len(secret), prk);
	wpabuf_clear_free(hkey);
	wpabuf_clear_free(secret);
	if (res < 0)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	wpa_hexdump_key(MSG_DEBUG, "OWE: prk", prk, hash_len);

	/* PMK = HKDF-expand(prk, "OWE Key Generation", n) */

	os_free(sta->owe_pmk);
	sta->owe_pmk = os_malloc(hash_len);
	if (!sta->owe_pmk) {
		os_memset(prk, 0, SHA512_MAC_LEN);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	if (group == 19)
		res = hmac_sha256_kdf(prk, hash_len, NULL, (const u8 *) info,
				      os_strlen(info), sta->owe_pmk, hash_len);
	else if (group == 20)
		res = hmac_sha384_kdf(prk, hash_len, NULL, (const u8 *) info,
				      os_strlen(info), sta->owe_pmk, hash_len);
	else if (group == 21)
		res = hmac_sha512_kdf(prk, hash_len, NULL, (const u8 *) info,
				      os_strlen(info), sta->owe_pmk, hash_len);
	os_memset(prk, 0, SHA512_MAC_LEN);
	if (res < 0) {
		os_free(sta->owe_pmk);
		sta->owe_pmk = NULL;
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
	sta->owe_pmk_len = hash_len;

	wpa_hexdump_key(MSG_DEBUG, "OWE: PMK", sta->owe_pmk, sta->owe_pmk_len);
	wpa_hexdump(MSG_DEBUG, "OWE: PMKID", pmkid, PMKID_LEN);
	wpa_auth_pmksa_add2(hapd->wpa_auth, sta->addr, sta->owe_pmk,
			    sta->owe_pmk_len, pmkid, 0, WPA_KEY_MGMT_OWE, NULL);

	return WLAN_STATUS_SUCCESS;
}


u16 owe_validate_request(struct hostapd_data *hapd, const u8 *peer,
			 const u8 *rsn_ie, size_t rsn_ie_len,
			 const u8 *owe_dh, size_t owe_dh_len)
{
	struct wpa_ie_data data;
	int res;

	if (!rsn_ie || rsn_ie_len < 2) {
		wpa_printf(MSG_DEBUG, "OWE: Invalid RSNE from " MACSTR,
			   MAC2STR(peer));
		return WLAN_STATUS_INVALID_IE;
	}
	rsn_ie -= 2;
	rsn_ie_len += 2;

	res = wpa_parse_wpa_ie_rsn(rsn_ie, rsn_ie_len, &data);
	if (res) {
		wpa_printf(MSG_DEBUG, "Failed to parse RSNE from " MACSTR
			   " (res=%d)", MAC2STR(peer), res);
		wpa_hexdump(MSG_DEBUG, "RSNE", rsn_ie, rsn_ie_len);
		return wpa_res_to_status_code(res);
	}
	if (!(data.key_mgmt & WPA_KEY_MGMT_OWE)) {
		wpa_printf(MSG_DEBUG,
			   "OWE: Unexpected key mgmt 0x%x from " MACSTR,
			   (unsigned int) data.key_mgmt, MAC2STR(peer));
		return WLAN_STATUS_AKMP_NOT_VALID;
	}
	if (!owe_dh) {
		wpa_printf(MSG_DEBUG,
			   "OWE: No Diffie-Hellman Parameter element from "
			   MACSTR, MAC2STR(peer));
		return WLAN_STATUS_AKMP_NOT_VALID;
	}

	return WLAN_STATUS_SUCCESS;
}


u16 owe_process_rsn_ie(struct hostapd_data *hapd,
		       struct sta_info *sta,
		       const u8 *rsn_ie, size_t rsn_ie_len,
		       const u8 *owe_dh, size_t owe_dh_len,
		       const u8 *link_addr)
{
	u16 status;
	u8 *owe_buf, ie[256 * 2];
	size_t ie_len = 0;
	enum wpa_validate_result res;

	if (!rsn_ie || rsn_ie_len < 2) {
		wpa_printf(MSG_DEBUG, "OWE: No RSNE in (Re)AssocReq");
		status = WLAN_STATUS_INVALID_IE;
		goto end;
	}

	if (!sta->wpa_sm)
		sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,	sta->addr,
						NULL);
	if (!sta->wpa_sm) {
		wpa_printf(MSG_WARNING,
			   "OWE: Failed to initialize WPA state machine");
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto end;
	}
#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta))
		wpa_auth_set_ml_info(sta->wpa_sm,
				     sta->mld_assoc_link_id, &sta->mld_info);
#endif /* CONFIG_IEEE80211BE */
	rsn_ie -= 2;
	rsn_ie_len += 2;
	res = wpa_validate_wpa_ie(hapd->wpa_auth, sta->wpa_sm,
				  hapd->iface->freq, rsn_ie, rsn_ie_len,
				  NULL, 0, NULL, 0, owe_dh, owe_dh_len, NULL);
	status = wpa_res_to_status_code(res);
	if (status != WLAN_STATUS_SUCCESS)
		goto end;
	status = owe_process_assoc_req(hapd, sta, owe_dh, owe_dh_len);
	if (status != WLAN_STATUS_SUCCESS)
		goto end;
	owe_buf = wpa_auth_write_assoc_resp_owe(sta->wpa_sm, ie, sizeof(ie),
						NULL, 0);
	if (!owe_buf) {
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto end;
	}

	if (sta->owe_ecdh) {
		struct wpabuf *pub;

		pub = crypto_ecdh_get_pubkey(sta->owe_ecdh, 0);
		if (!pub) {
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto end;
		}

		/* OWE Diffie-Hellman Parameter element */
		*owe_buf++ = WLAN_EID_EXTENSION; /* Element ID */
		*owe_buf++ = 1 + 2 + wpabuf_len(pub); /* Length */
		*owe_buf++ = WLAN_EID_EXT_OWE_DH_PARAM; /* Element ID Extension
							 */
		WPA_PUT_LE16(owe_buf, sta->owe_group);
		owe_buf += 2;
		os_memcpy(owe_buf, wpabuf_head(pub), wpabuf_len(pub));
		owe_buf += wpabuf_len(pub);
		wpabuf_free(pub);
		sta->external_dh_updated = 1;
	}
	ie_len = owe_buf - ie;

end:
	wpa_printf(MSG_DEBUG, "OWE: Update status %d, ie len %d for peer "
			      MACSTR, status, (unsigned int) ie_len,
			      MAC2STR(link_addr ? link_addr : sta->addr));
	hostapd_drv_update_dh_ie(hapd, link_addr ? link_addr : sta->addr,
				 status,
				 status == WLAN_STATUS_SUCCESS ? ie : NULL,
				 ie_len);

	return status;
}

#endif /* CONFIG_OWE */


static bool check_sa_query(struct hostapd_data *hapd, struct sta_info *sta,
			   int reassoc)
{
	if ((sta->flags &
	     (WLAN_STA_ASSOC | WLAN_STA_MFP | WLAN_STA_AUTHORIZED)) !=
	    (WLAN_STA_ASSOC | WLAN_STA_MFP | WLAN_STA_AUTHORIZED))
		return false;

	if (!sta->sa_query_timed_out && sta->sa_query_count > 0)
		ap_check_sa_query_timeout(hapd, sta);

	if (!sta->sa_query_timed_out &&
	    (!reassoc || sta->auth_alg != WLAN_AUTH_FT)) {
		/*
		 * STA has already been associated with MFP and SA Query timeout
		 * has not been reached. Reject the association attempt
		 * temporarily and start SA Query, if one is not pending.
		 */
		if (sta->sa_query_count == 0)
			ap_sta_start_sa_query(hapd, sta);

		return true;
	}

	return false;
}


static int __check_assoc_ies(struct hostapd_data *hapd, struct sta_info *sta,
			     const u8 *ies, size_t ies_len,
			     struct ieee802_11_elems *elems, int reassoc,
			     bool link)
{
	int resp;
	const u8 *wpa_ie;
	size_t wpa_ie_len;
	const u8 *p2p_dev_addr = NULL;
	struct hostapd_data *assoc_hapd;
	struct sta_info *assoc_sta = NULL;

	resp = check_ssid(hapd, sta, elems->ssid, elems->ssid_len);
	if (resp != WLAN_STATUS_SUCCESS)
		return resp;
	resp = check_wmm(hapd, sta, elems->wmm, elems->wmm_len);
	if (resp != WLAN_STATUS_SUCCESS)
		return resp;
	resp = check_ext_capab(hapd, sta, elems->ext_capab,
			       elems->ext_capab_len);
	if (resp != WLAN_STATUS_SUCCESS)
		return resp;
	resp = copy_supp_rates(hapd, sta, elems);
	if (resp != WLAN_STATUS_SUCCESS)
		return resp;

	resp = check_multi_ap(hapd, sta, elems->multi_ap, elems->multi_ap_len);
	if (resp != WLAN_STATUS_SUCCESS)
		return resp;

	resp = copy_sta_ht_capab(hapd, sta, elems->ht_capabilities);
	if (resp != WLAN_STATUS_SUCCESS)
		return resp;
	if (hapd->iconf->ieee80211n && hapd->iconf->require_ht &&
	    !(sta->flags & WLAN_STA_HT)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "Station does not support "
			       "mandatory HT PHY - reject association");
		return WLAN_STATUS_ASSOC_DENIED_NO_HT;
	}

#ifdef CONFIG_IEEE80211AC
	if (hapd->iconf->ieee80211ac) {
		resp = copy_sta_vht_capab(hapd, sta, elems->vht_capabilities);
		if (resp != WLAN_STATUS_SUCCESS)
			return resp;

		resp = set_sta_vht_opmode(hapd, sta, elems->opmode_notif);
		if (resp != WLAN_STATUS_SUCCESS)
			return resp;
	}

	if (hapd->iconf->ieee80211ac && hapd->iconf->require_vht &&
	    !(sta->flags & WLAN_STA_VHT)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "Station does not support "
			       "mandatory VHT PHY - reject association");
		return WLAN_STATUS_ASSOC_DENIED_NO_VHT;
	}

	if (hapd->conf->vendor_vht && !elems->vht_capabilities) {
		resp = copy_sta_vendor_vht(hapd, sta, elems->vendor_vht,
					   elems->vendor_vht_len);
		if (resp != WLAN_STATUS_SUCCESS)
			return resp;
	}
#endif /* CONFIG_IEEE80211AC */
#ifdef CONFIG_IEEE80211AX
	if (hapd->iconf->ieee80211ax && !hapd->conf->disable_11ax) {
		resp = copy_sta_he_capab(hapd, sta, IEEE80211_MODE_AP,
					 elems->he_capabilities,
					 elems->he_capabilities_len);
		if (resp != WLAN_STATUS_SUCCESS)
			return resp;

		if (hapd->iconf->require_he && !(sta->flags & WLAN_STA_HE)) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_INFO,
				       "Station does not support mandatory HE PHY - reject association");
			return WLAN_STATUS_DENIED_HE_NOT_SUPPORTED;
		}

		if (is_6ghz_op_class(hapd->iconf->op_class)) {
			if (!(sta->flags & WLAN_STA_HE)) {
				hostapd_logger(hapd, sta->addr,
					       HOSTAPD_MODULE_IEEE80211,
					       HOSTAPD_LEVEL_INFO,
					       "Station does not support mandatory HE PHY - reject association");
				return WLAN_STATUS_DENIED_HE_NOT_SUPPORTED;
			}
			resp = copy_sta_he_6ghz_capab(hapd, sta,
						      elems->he_6ghz_band_cap);
			if (resp != WLAN_STATUS_SUCCESS)
				return resp;
		}
	}
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211BE
	if (hapd->iconf->ieee80211be && !hapd->conf->disable_11be) {
		resp = copy_sta_eht_capab(hapd, sta, IEEE80211_MODE_AP,
					  elems->he_capabilities,
					  elems->he_capabilities_len,
					  elems->eht_capabilities,
					  elems->eht_capabilities_len);
		if (resp != WLAN_STATUS_SUCCESS)
			return resp;

		if (!link) {
			resp = hostapd_process_ml_assoc_req(hapd, elems, sta);
			if (resp != WLAN_STATUS_SUCCESS)
				return resp;
		}
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_P2P
	if (elems->p2p && ies && ies_len) {
		wpabuf_free(sta->p2p_ie);
		sta->p2p_ie = ieee802_11_vendor_ie_concat(ies, ies_len,
							  P2P_IE_VENDOR_TYPE);
		if (sta->p2p_ie)
			p2p_dev_addr = p2p_get_go_dev_addr(sta->p2p_ie);
	} else {
		wpabuf_free(sta->p2p_ie);
		sta->p2p_ie = NULL;
	}
#endif /* CONFIG_P2P */

	if ((hapd->conf->wpa & WPA_PROTO_RSN) && elems->rsn_ie) {
		wpa_ie = elems->rsn_ie;
		wpa_ie_len = elems->rsn_ie_len;
	} else if ((hapd->conf->wpa & WPA_PROTO_WPA) &&
		   elems->wpa_ie) {
		wpa_ie = elems->wpa_ie;
		wpa_ie_len = elems->wpa_ie_len;
	} else {
		wpa_ie = NULL;
		wpa_ie_len = 0;
	}

#ifdef CONFIG_WPS
	sta->flags &= ~(WLAN_STA_WPS | WLAN_STA_MAYBE_WPS | WLAN_STA_WPS2);
	if (hapd->conf->wps_state && elems->wps_ie && ies && ies_len) {
		wpa_printf(MSG_DEBUG, "STA included WPS IE in (Re)Association "
			   "Request - assume WPS is used");
		sta->flags |= WLAN_STA_WPS;
		wpabuf_free(sta->wps_ie);
		sta->wps_ie = ieee802_11_vendor_ie_concat(ies, ies_len,
							  WPS_IE_VENDOR_TYPE);
		if (sta->wps_ie && wps_is_20(sta->wps_ie)) {
			wpa_printf(MSG_DEBUG, "WPS: STA supports WPS 2.0");
			sta->flags |= WLAN_STA_WPS2;
		}
		wpa_ie = NULL;
		wpa_ie_len = 0;
		if (sta->wps_ie && wps_validate_assoc_req(sta->wps_ie) < 0) {
			wpa_printf(MSG_DEBUG, "WPS: Invalid WPS IE in "
				   "(Re)Association Request - reject");
			return WLAN_STATUS_INVALID_IE;
		}
	} else if (hapd->conf->wps_state && wpa_ie == NULL) {
		wpa_printf(MSG_DEBUG, "STA did not include WPA/RSN IE in "
			   "(Re)Association Request - possible WPS use");
		sta->flags |= WLAN_STA_MAYBE_WPS;
	} else
#endif /* CONFIG_WPS */
	if (hapd->conf->wpa && wpa_ie == NULL) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "No WPA/RSN IE in association request");
		return WLAN_STATUS_INVALID_IE;
	}

	if (hapd->conf->wpa && wpa_ie) {
		enum wpa_validate_result res;
#ifdef CONFIG_IEEE80211BE
		struct mld_info *info = &sta->mld_info;
		bool init = !sta->wpa_sm;
#endif /* CONFIG_IEEE80211BE */

		wpa_ie -= 2;
		wpa_ie_len += 2;

		if (!sta->wpa_sm) {
			if (!link)
				assoc_sta = hostapd_ml_get_assoc_sta(
					hapd, sta, &assoc_hapd);

			sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,
							sta->addr,
							p2p_dev_addr);

			if (!sta->wpa_sm) {
				wpa_printf(MSG_WARNING,
					   "Failed to initialize RSN state machine");
				return WLAN_STATUS_UNSPECIFIED_FAILURE;
			}
		}

#ifdef CONFIG_IEEE80211BE
		if (ap_sta_is_mld(hapd, sta)) {
			wpa_printf(MSG_DEBUG,
				   "MLD: %s ML info in RSN Authenticator",
				   init ? "Set" : "Reset");
			wpa_auth_set_ml_info(sta->wpa_sm,
					     sta->mld_assoc_link_id,
					     info);
		}
#endif /* CONFIG_IEEE80211BE */

		wpa_auth_set_auth_alg(sta->wpa_sm, sta->auth_alg);
		res = wpa_validate_wpa_ie(hapd->wpa_auth, sta->wpa_sm,
					  hapd->iface->freq,
					  wpa_ie, wpa_ie_len,
					  elems->rsnxe ? elems->rsnxe - 2 :
					  NULL,
					  elems->rsnxe ? elems->rsnxe_len + 2 :
					  0,
					  elems->mdie, elems->mdie_len,
					  elems->owe_dh, elems->owe_dh_len,
					  assoc_sta ? assoc_sta->wpa_sm : NULL);
		resp = wpa_res_to_status_code(res);
		if (resp != WLAN_STATUS_SUCCESS)
			return resp;

		if (wpa_auth_uses_mfp(sta->wpa_sm))
			sta->flags |= WLAN_STA_MFP;
		else
			sta->flags &= ~WLAN_STA_MFP;

#ifdef CONFIG_IEEE80211R_AP
		if (sta->auth_alg == WLAN_AUTH_FT) {
			if (!reassoc) {
				wpa_printf(MSG_DEBUG, "FT: " MACSTR " tried "
					   "to use association (not "
					   "re-association) with FT auth_alg",
					   MAC2STR(sta->addr));
				return WLAN_STATUS_UNSPECIFIED_FAILURE;
			}

			resp = wpa_ft_validate_reassoc(sta->wpa_sm, ies,
						       ies_len);
			if (resp != WLAN_STATUS_SUCCESS)
				return resp;
		}
#endif /* CONFIG_IEEE80211R_AP */

		if (link)
			goto skip_sae_owe;
#ifdef CONFIG_SAE
		if (wpa_auth_uses_sae(sta->wpa_sm) && sta->sae &&
		    sta->sae->state == SAE_ACCEPTED)
			wpa_auth_add_sae_pmkid(sta->wpa_sm, sta->sae->pmkid);

		if (wpa_auth_uses_sae(sta->wpa_sm) &&
		    sta->auth_alg == WLAN_AUTH_OPEN) {
			struct rsn_pmksa_cache_entry *sa;
			sa = wpa_auth_sta_get_pmksa(sta->wpa_sm);
			if (!sa || !wpa_key_mgmt_sae(sa->akmp)) {
				wpa_printf(MSG_DEBUG,
					   "SAE: No PMKSA cache entry found for "
					   MACSTR, MAC2STR(sta->addr));
				return WLAN_STATUS_INVALID_PMKID;
			}
			wpa_printf(MSG_DEBUG, "SAE: " MACSTR
				   " using PMKSA caching", MAC2STR(sta->addr));
		} else if (wpa_auth_uses_sae(sta->wpa_sm) &&
			   sta->auth_alg != WLAN_AUTH_SAE &&
			   !(sta->auth_alg == WLAN_AUTH_FT &&
			     wpa_auth_uses_ft_sae(sta->wpa_sm))) {
			wpa_printf(MSG_DEBUG, "SAE: " MACSTR " tried to use "
				   "SAE AKM after non-SAE auth_alg %u",
				   MAC2STR(sta->addr), sta->auth_alg);
			return WLAN_STATUS_NOT_SUPPORTED_AUTH_ALG;
		}

		if (hapd->conf->sae_pwe == SAE_PWE_BOTH &&
		    sta->auth_alg == WLAN_AUTH_SAE &&
		    sta->sae && !sta->sae->h2e &&
		    ieee802_11_rsnx_capab_len(elems->rsnxe, elems->rsnxe_len,
					      WLAN_RSNX_CAPAB_SAE_H2E)) {
			wpa_printf(MSG_INFO, "SAE: " MACSTR
				   " indicates support for SAE H2E, but did not use it",
				   MAC2STR(sta->addr));
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
		}
#endif /* CONFIG_SAE */

#ifdef CONFIG_OWE
		if ((hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_OWE) &&
		    wpa_auth_sta_key_mgmt(sta->wpa_sm) == WPA_KEY_MGMT_OWE &&
		    elems->owe_dh) {
			resp = owe_process_assoc_req(hapd, sta, elems->owe_dh,
						     elems->owe_dh_len);
			if (resp != WLAN_STATUS_SUCCESS)
				return resp;
		}
#endif /* CONFIG_OWE */
	skip_sae_owe:

#ifdef CONFIG_DPP2
		dpp_pfs_free(sta->dpp_pfs);
		sta->dpp_pfs = NULL;

		if (DPP_VERSION > 1 &&
		    (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_DPP) &&
		    hapd->conf->dpp_netaccesskey && sta->wpa_sm &&
		    wpa_auth_sta_key_mgmt(sta->wpa_sm) == WPA_KEY_MGMT_DPP &&
		    elems->owe_dh) {
			sta->dpp_pfs = dpp_pfs_init(
				wpabuf_head(hapd->conf->dpp_netaccesskey),
				wpabuf_len(hapd->conf->dpp_netaccesskey));
			if (!sta->dpp_pfs) {
				wpa_printf(MSG_DEBUG,
					   "DPP: Could not initialize PFS");
				/* Try to continue without PFS */
				goto pfs_fail;
			}

			if (dpp_pfs_process(sta->dpp_pfs, elems->owe_dh,
					    elems->owe_dh_len) < 0) {
				dpp_pfs_free(sta->dpp_pfs);
				sta->dpp_pfs = NULL;
				return WLAN_STATUS_UNSPECIFIED_FAILURE;
			}
		}

		wpa_auth_set_dpp_z(sta->wpa_sm, sta->dpp_pfs ?
				   sta->dpp_pfs->secret : NULL);
	pfs_fail:
#endif /* CONFIG_DPP2 */

		if ((sta->flags & (WLAN_STA_HT | WLAN_STA_VHT)) &&
		    wpa_auth_get_pairwise(sta->wpa_sm) == WPA_CIPHER_TKIP) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_INFO,
				       "Station tried to use TKIP with HT "
				       "association");
			return WLAN_STATUS_CIPHER_REJECTED_PER_POLICY;
		}

		wpa_auth_set_ssid_protection(
			sta->wpa_sm,
			hapd->conf->ssid_protection &&
			ieee802_11_rsnx_capab_len(
				elems->rsnxe, elems->rsnxe_len,
				WLAN_RSNX_CAPAB_SSID_PROTECTION));
#ifdef CONFIG_HS20
	} else if (hapd->conf->osen) {
		if (!elems->osen) {
			hostapd_logger(
				hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
				HOSTAPD_LEVEL_INFO,
				"No HS 2.0 OSEN element in association request");
			return WLAN_STATUS_INVALID_IE;
		}

		wpa_printf(MSG_DEBUG, "HS 2.0: OSEN association");
		if (sta->wpa_sm == NULL)
			sta->wpa_sm = wpa_auth_sta_init(hapd->wpa_auth,
							sta->addr, NULL);
		if (sta->wpa_sm == NULL) {
			wpa_printf(MSG_WARNING, "Failed to initialize WPA "
				   "state machine");
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
		}
		if (wpa_validate_osen(hapd->wpa_auth, sta->wpa_sm,
				      elems->osen - 2, elems->osen_len + 2) < 0)
			return WLAN_STATUS_INVALID_IE;
#endif /* CONFIG_HS20 */
	} else
		wpa_auth_sta_no_wpa(sta->wpa_sm);

#ifdef CONFIG_P2P
	p2p_group_notif_assoc(hapd->p2p_group, sta->addr, ies, ies_len);
#endif /* CONFIG_P2P */

#ifdef CONFIG_HS20
	wpabuf_free(sta->hs20_ie);
	if (elems->hs20 && elems->hs20_len > 4) {
		int release;

		sta->hs20_ie = wpabuf_alloc_copy(elems->hs20 + 4,
						 elems->hs20_len - 4);
		release = ((elems->hs20[4] >> 4) & 0x0f) + 1;
		if (release >= 2 && !wpa_auth_uses_mfp(sta->wpa_sm) &&
		    hapd->conf->ieee80211w != NO_MGMT_FRAME_PROTECTION) {
			wpa_printf(MSG_DEBUG,
				   "HS 2.0: PMF not negotiated by release %d station "
				   MACSTR, release, MAC2STR(sta->addr));
			return WLAN_STATUS_ROBUST_MGMT_FRAME_POLICY_VIOLATION;
		}
	} else {
		sta->hs20_ie = NULL;
	}

	wpabuf_free(sta->roaming_consortium);
	if (elems->roaming_cons_sel)
		sta->roaming_consortium = wpabuf_alloc_copy(
			elems->roaming_cons_sel + 4,
			elems->roaming_cons_sel_len - 4);
	else
		sta->roaming_consortium = NULL;
#endif /* CONFIG_HS20 */

#ifdef CONFIG_FST
	wpabuf_free(sta->mb_ies);
	if (hapd->iface->fst)
		sta->mb_ies = mb_ies_by_info(&elems->mb_ies);
	else
		sta->mb_ies = NULL;
#endif /* CONFIG_FST */

#ifdef CONFIG_MBO
	mbo_ap_check_sta_assoc(hapd, sta, elems);

	if (hapd->conf->mbo_enabled && (hapd->conf->wpa & 2) &&
	    elems->mbo && sta->cell_capa && !(sta->flags & WLAN_STA_MFP) &&
	    hapd->conf->ieee80211w != NO_MGMT_FRAME_PROTECTION) {
		wpa_printf(MSG_INFO,
			   "MBO: Reject WPA2 association without PMF");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
#endif /* CONFIG_MBO */

#if defined(CONFIG_FILS) && defined(CONFIG_OCV)
	if (wpa_auth_uses_ocv(sta->wpa_sm) &&
	    (sta->auth_alg == WLAN_AUTH_FILS_SK ||
	     sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	     sta->auth_alg == WLAN_AUTH_FILS_PK)) {
		struct wpa_channel_info ci;
		int tx_chanwidth;
		int tx_seg1_idx;
		enum oci_verify_result res;

		if (hostapd_drv_channel_info(hapd, &ci) != 0) {
			wpa_printf(MSG_WARNING,
				   "Failed to get channel info to validate received OCI in FILS (Re)Association Request frame");
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
		}

		if (get_sta_tx_parameters(sta->wpa_sm,
					  channel_width_to_int(ci.chanwidth),
					  ci.seg1_idx, &tx_chanwidth,
					  &tx_seg1_idx) < 0)
			return WLAN_STATUS_UNSPECIFIED_FAILURE;

		res = ocv_verify_tx_params(elems->oci, elems->oci_len, &ci,
					   tx_chanwidth, tx_seg1_idx);
		if (wpa_auth_uses_ocv(sta->wpa_sm) == 2 &&
		    res == OCI_NOT_FOUND) {
			/* Work around misbehaving STAs */
			wpa_printf(MSG_INFO,
				   "FILS: Disable OCV with a STA that does not send OCI");
			wpa_auth_set_ocv(sta->wpa_sm, 0);
		} else if (res != OCI_SUCCESS) {
			wpa_printf(MSG_WARNING, "FILS: OCV failed: %s",
				   ocv_errorstr);
			wpa_msg(hapd->msg_ctx, MSG_INFO, OCV_FAILURE "addr="
				MACSTR " frame=fils-reassoc-req error=%s",
				MAC2STR(sta->addr), ocv_errorstr);
			return WLAN_STATUS_UNSPECIFIED_FAILURE;
		}
	}
#endif /* CONFIG_FILS && CONFIG_OCV */

	ap_copy_sta_supp_op_classes(sta, elems->supp_op_classes,
				    elems->supp_op_classes_len);

	if ((sta->capability & WLAN_CAPABILITY_RADIO_MEASUREMENT) &&
	    elems->rrm_enabled &&
	    elems->rrm_enabled_len >= sizeof(sta->rrm_enabled_capa))
		os_memcpy(sta->rrm_enabled_capa, elems->rrm_enabled,
			  sizeof(sta->rrm_enabled_capa));

	if (elems->power_capab) {
		sta->min_tx_power = elems->power_capab[0];
		sta->max_tx_power = elems->power_capab[1];
		sta->power_capab = 1;
	} else {
		sta->power_capab = 0;
	}

	if (elems->bss_max_idle_period &&
	    hapd->conf->max_acceptable_idle_period) {
		u16 req;

		req = WPA_GET_LE16(elems->bss_max_idle_period);
		if (req <= hapd->conf->max_acceptable_idle_period)
			sta->max_idle_period = req;
		else if (hapd->conf->max_acceptable_idle_period >
			 hapd->conf->ap_max_inactivity)
			sta->max_idle_period =
				hapd->conf->max_acceptable_idle_period;
	}

	return WLAN_STATUS_SUCCESS;
}


static int check_assoc_ies(struct hostapd_data *hapd, struct sta_info *sta,
			   const u8 *ies, size_t ies_len, int reassoc)
{
	struct ieee802_11_elems elems;

	if (ieee802_11_parse_elems(ies, ies_len, &elems, 1) == ParseFailed) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO,
			       "Station sent an invalid association request");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	return __check_assoc_ies(hapd, sta, ies, ies_len, &elems, reassoc,
				 false);
}


#ifdef CONFIG_IEEE80211BE

static void ieee80211_ml_build_assoc_resp(struct hostapd_data *hapd,
					  struct mld_link_info *link)
{
	u8 buf[EHT_ML_MAX_STA_PROF_LEN];
	u8 *p = buf;
	size_t buflen = sizeof(buf);

	/* Capability Info */
	WPA_PUT_LE16(p, hostapd_own_capab_info(hapd));
	p += 2;

	/* Status Code */
	WPA_PUT_LE16(p, link->status);
	p += 2;

	if (link->status != WLAN_STATUS_SUCCESS)
		goto out;

	/* AID is not included */
	p = hostapd_eid_supp_rates(hapd, p);
	p = hostapd_eid_ext_supp_rates(hapd, p);
	p = hostapd_eid_rm_enabled_capab(hapd, p, buf + buflen - p);
	p = hostapd_eid_ht_capabilities(hapd, p);
	p = hostapd_eid_ht_operation(hapd, p);

	if (hapd->iconf->ieee80211ac && !hapd->conf->disable_11ac) {
		p = hostapd_eid_vht_capabilities(hapd, p, 0);
		p = hostapd_eid_vht_operation(hapd, p);
	}

	if (hapd->iconf->ieee80211ax && !hapd->conf->disable_11ax) {
		p = hostapd_eid_he_capab(hapd, p, IEEE80211_MODE_AP);
		p = hostapd_eid_he_operation(hapd, p);
		p = hostapd_eid_spatial_reuse(hapd, p);
		p = hostapd_eid_he_mu_edca_parameter_set(hapd, p);
		p = hostapd_eid_he_6ghz_band_cap(hapd, p);
		if (hapd->iconf->ieee80211be && !hapd->conf->disable_11be) {
			p = hostapd_eid_eht_capab(hapd, p, IEEE80211_MODE_AP);
			p = hostapd_eid_eht_operation(hapd, p);
		}
	}

	p = hostapd_eid_ext_capab(hapd, p, false);
	p = hostapd_eid_mbo(hapd, p, buf + buflen - p);
	p = hostapd_eid_wmm(hapd, p);

	if (hapd->conf->assocresp_elements &&
	    (size_t) (buf + buflen - p) >=
	    wpabuf_len(hapd->conf->assocresp_elements)) {
		os_memcpy(p, wpabuf_head(hapd->conf->assocresp_elements),
			  wpabuf_len(hapd->conf->assocresp_elements));
		p += wpabuf_len(hapd->conf->assocresp_elements);
	}

out:
	os_free(link->resp_sta_profile);
	link->resp_sta_profile = os_memdup(buf, p - buf);
	link->resp_sta_profile_len = link->resp_sta_profile ? p - buf : 0;
}


static int ieee80211_ml_process_link(struct hostapd_data *hapd,
				     struct sta_info *origin_sta,
				     struct mld_link_info *link,
				     const u8 *ies, size_t ies_len,
				     bool reassoc, bool offload)
{
	struct ieee802_11_elems elems;
	struct wpabuf *mlbuf = NULL;
	struct sta_info *sta = NULL;
	u16 status = WLAN_STATUS_SUCCESS;
	int i;

	wpa_printf(MSG_DEBUG, "MLD: link: link_id=%u, peer=" MACSTR,
		   hapd->mld_link_id, MAC2STR(link->peer_addr));

	if (ieee802_11_parse_elems(ies, ies_len, &elems, 1) == ParseFailed) {
		wpa_printf(MSG_DEBUG, "MLD: link: Element parsing failed");
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto out;
	}

	sta = ap_get_sta(hapd, origin_sta->addr);
	if (sta) {
		wpa_printf(MSG_INFO, "MLD: link: Station already exists");
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		sta = NULL;
		goto out;
	}

	sta = ap_sta_add(hapd, origin_sta->addr);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "MLD: link: ap_sta_add() failed");
		status = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
		goto out;
	}

	mlbuf = ieee802_11_defrag(elems.basic_mle, elems.basic_mle_len, true);
	if (!mlbuf)
		goto out;

	if (ieee802_11_parse_link_assoc_req(ies, ies_len, &elems, mlbuf,
					    hapd->mld_link_id, true) ==
	    ParseFailed) {
		wpa_printf(MSG_DEBUG,
			   "MLD: link: Failed to parse association request Multi-Link element");
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto out;
	}

	sta->flags |= origin_sta->flags | WLAN_STA_ASSOC_REQ_OK;
	sta->mld_assoc_link_id = origin_sta->mld_assoc_link_id;

	status = __check_assoc_ies(hapd, sta, NULL, 0, &elems, reassoc, true);
	if (status != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG, "MLD: link: Element check failed");
		goto out;
	}

	ap_sta_set_mld(sta, true);

	os_memcpy(&sta->mld_info, &origin_sta->mld_info, sizeof(sta->mld_info));
	for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
		struct mld_link_info *li = &sta->mld_info.links[i];

		li->resp_sta_profile = NULL;
		li->resp_sta_profile_len = 0;
	}

	if (!offload) {
		/*
		 * Get the AID from the station on which the association was
		 * performed, and mark it as used.
		 */
		sta->aid = origin_sta->aid;
		if (sta->aid == 0) {
			wpa_printf(MSG_DEBUG, "MLD: link: No AID assigned");
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto out;
		}
		hapd->sta_aid[(sta->aid - 1) / 32] |= BIT((sta->aid - 1) % 32);
		sta->listen_interval = origin_sta->listen_interval;
		if (update_ht_state(hapd, sta) > 0)
			ieee802_11_update_beacons(hapd->iface);
	}

	/* Maintain state machine reference on all link STAs, this is needed
	 * during group rekey handling.
	 */
	wpa_auth_sta_deinit(sta->wpa_sm);
	sta->wpa_sm = origin_sta->wpa_sm;

	/*
	 * Do not initialize the EAPOL state machine.
	 * TODO: Maybe it is needed?
	 */
	sta->eapol_sm = NULL;

	wpa_printf(MSG_DEBUG, "MLD: link=%u, association OK (aid=%u)",
		   hapd->mld_link_id, sta->aid);

	sta->flags |= WLAN_STA_AUTH | WLAN_STA_ASSOC_REQ_OK;

	/* TODO: What other processing is required? */

	if (!offload && add_associated_sta(hapd, sta, reassoc))
		status = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
out:
	wpabuf_free(mlbuf);
	link->status = status;

	if (!offload)
		ieee80211_ml_build_assoc_resp(hapd, link);

	wpa_printf(MSG_DEBUG, "MLD: link: status=%u", status);
	if (status != WLAN_STATUS_SUCCESS) {
		if (sta)
			ap_free_sta(hapd, sta);
		return -1;
	}

	return 0;
}


bool hostapd_is_mld_ap(struct hostapd_data *hapd)
{
	if (!hapd->conf->mld_ap)
		return false;

	if (!hapd->iface || !hapd->iface->interfaces ||
	    hapd->iface->interfaces->count <= 1)
		return false;

	return true;
}

#endif /* CONFIG_IEEE80211BE */


int hostapd_process_assoc_ml_info(struct hostapd_data *hapd,
				  struct sta_info *sta,
				  const u8 *ies, size_t ies_len,
				  bool reassoc, int tx_link_status,
				  bool offload)
{
#ifdef CONFIG_IEEE80211BE
	unsigned int i;

	if (!hostapd_is_mld_ap(hapd))
		return 0;

	for (i = 0; i < MAX_NUM_MLD_LINKS; i++) {
		struct hostapd_data *bss = NULL;
		struct mld_link_info *link = &sta->mld_info.links[i];
		bool link_bss_found = false;

		if (!link->valid || i == sta->mld_assoc_link_id)
			continue;

		for_each_mld_link(bss, hapd) {
			if (bss == hapd)
				continue;

			if (bss->mld_link_id != i)
				continue;

			link_bss_found = true;
			break;
		}

		if (!link_bss_found || TEST_FAIL()) {
			wpa_printf(MSG_DEBUG,
				   "MLD: No link match for link_id=%u", i);

			link->status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			if (!offload)
				ieee80211_ml_build_assoc_resp(hapd, link);
		} else if (tx_link_status != WLAN_STATUS_SUCCESS) {
			/* TX link rejected the connection */
			link->status = WLAN_STATUS_DENIED_TX_LINK_NOT_ACCEPTED;
			if (!offload)
				ieee80211_ml_build_assoc_resp(hapd, link);
		} else {
			if (ieee80211_ml_process_link(bss, sta, link,
						      ies, ies_len, reassoc,
						      offload))
				return -1;
		}
	}
#endif /* CONFIG_IEEE80211BE */

	return 0;
}


static void send_deauth(struct hostapd_data *hapd, const u8 *addr,
			u16 reason_code)
{
	int send_len;
	struct ieee80211_mgmt reply;

	os_memset(&reply, 0, sizeof(reply));
	reply.frame_control =
		IEEE80211_FC(WLAN_FC_TYPE_MGMT, WLAN_FC_STYPE_DEAUTH);
	os_memcpy(reply.da, addr, ETH_ALEN);
	os_memcpy(reply.sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(reply.bssid, hapd->own_addr, ETH_ALEN);

	send_len = IEEE80211_HDRLEN + sizeof(reply.u.deauth);
	reply.u.deauth.reason_code = host_to_le16(reason_code);

	if (hostapd_drv_send_mlme(hapd, &reply, send_len, 0, NULL, 0, 0) < 0)
		wpa_printf(MSG_INFO, "Failed to send deauth: %s",
			   strerror(errno));
}


static int add_associated_sta(struct hostapd_data *hapd,
			      struct sta_info *sta, int reassoc)
{
	struct ieee80211_ht_capabilities ht_cap;
	struct ieee80211_vht_capabilities vht_cap;
	struct ieee80211_he_capabilities he_cap;
	struct ieee80211_eht_capabilities eht_cap;
	int set = 1;
	const u8 *mld_link_addr = NULL;
	bool mld_link_sta = false;

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta)) {
		u8 mld_link_id = hapd->mld_link_id;

		mld_link_sta = sta->mld_assoc_link_id != mld_link_id;
		mld_link_addr = sta->mld_info.links[mld_link_id].peer_addr;

		if (hapd->mld_link_id != sta->mld_assoc_link_id)
			set = 0;
	}
#endif /* CONFIG_IEEE80211BE */

	/*
	 * Remove the STA entry to ensure the STA PS state gets cleared and
	 * configuration gets updated. This is relevant for cases, such as
	 * FT-over-the-DS, where a station re-associates back to the same AP but
	 * skips the authentication flow, or if working with a driver that
	 * does not support full AP client state.
	 *
	 * Skip this if the STA has already completed FT reassociation and the
	 * TK has been configured since the TX/RX PN must not be reset to 0 for
	 * the same key.
	 *
	 * FT-over-the-DS has a special case where the STA entry (and as such,
	 * the TK) has not yet been configured to the driver depending on which
	 * driver interface is used. For that case, allow add-STA operation to
	 * be used (instead of set-STA). This is needed to allow mac80211-based
	 * drivers to accept the STA parameter configuration. Since this is
	 * after a new FT-over-DS exchange, a new TK has been derived, so key
	 * reinstallation is not a concern for this case.
	 */
	wpa_printf(MSG_DEBUG, "Add associated STA " MACSTR
		   " (added_unassoc=%d auth_alg=%u ft_over_ds=%u reassoc=%d authorized=%d ft_tk=%d fils_tk=%d)",
		   MAC2STR(sta->addr), sta->added_unassoc, sta->auth_alg,
		   sta->ft_over_ds, reassoc,
		   !!(sta->flags & WLAN_STA_AUTHORIZED),
		   wpa_auth_sta_ft_tk_already_set(sta->wpa_sm),
		   wpa_auth_sta_fils_tk_already_set(sta->wpa_sm));

	if (!mld_link_sta && !sta->added_unassoc &&
	    (!(sta->flags & WLAN_STA_AUTHORIZED) ||
	     (reassoc && sta->ft_over_ds && sta->auth_alg == WLAN_AUTH_FT) ||
	     (!wpa_auth_sta_ft_tk_already_set(sta->wpa_sm) &&
	      !wpa_auth_sta_fils_tk_already_set(sta->wpa_sm)))) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		wpa_auth_sm_event(sta->wpa_sm, WPA_DRV_STA_REMOVED);
		set = 0;

		 /* Do not allow the FT-over-DS exception to be used more than
		  * once per authentication exchange to guarantee a new TK is
		  * used here */
		sta->ft_over_ds = 0;
	}

	if (sta->flags & WLAN_STA_HT)
		hostapd_get_ht_capab(hapd, sta->ht_capabilities, &ht_cap);
#ifdef CONFIG_IEEE80211AC
	if (sta->flags & WLAN_STA_VHT)
		hostapd_get_vht_capab(hapd, sta->vht_capabilities, &vht_cap);
#endif /* CONFIG_IEEE80211AC */
#ifdef CONFIG_IEEE80211AX
	if (sta->flags & WLAN_STA_HE) {
		hostapd_get_he_capab(hapd, sta->he_capab, &he_cap,
				     sta->he_capab_len);
	}
#endif /* CONFIG_IEEE80211AX */
#ifdef CONFIG_IEEE80211BE
	if (sta->flags & WLAN_STA_EHT)
		hostapd_get_eht_capab(hapd, sta->eht_capab, &eht_cap,
				      sta->eht_capab_len);
#endif /* CONFIG_IEEE80211BE */

	/*
	 * Add the station with forced WLAN_STA_ASSOC flag. The sta->flags
	 * will be set when the ACK frame for the (Re)Association Response frame
	 * is processed (TX status driver event).
	 */
	if (hostapd_sta_add(hapd, sta->addr, sta->aid, sta->capability,
			    sta->supported_rates, sta->supported_rates_len,
			    sta->listen_interval,
			    sta->flags & WLAN_STA_HT ? &ht_cap : NULL,
			    sta->flags & WLAN_STA_VHT ? &vht_cap : NULL,
			    sta->flags & WLAN_STA_HE ? &he_cap : NULL,
			    sta->flags & WLAN_STA_HE ? sta->he_capab_len : 0,
			    sta->flags & WLAN_STA_EHT ? &eht_cap : NULL,
			    sta->flags & WLAN_STA_EHT ? sta->eht_capab_len : 0,
			    sta->he_6ghz_capab,
			    sta->flags | WLAN_STA_ASSOC, sta->qosinfo,
			    sta->vht_opmode, sta->p2p_ie ? 1 : 0,
			    set, mld_link_addr, mld_link_sta)) {
		hostapd_logger(hapd, sta->addr,
			       HOSTAPD_MODULE_IEEE80211, HOSTAPD_LEVEL_NOTICE,
			       "Could not %s STA to kernel driver",
			       set ? "set" : "add");

		if (sta->added_unassoc) {
			hostapd_drv_sta_remove(hapd, sta->addr);
			sta->added_unassoc = 0;
		}

		return -1;
	}

	sta->added_unassoc = 0;

	return 0;
}


static u16 send_assoc_resp(struct hostapd_data *hapd, struct sta_info *sta,
			   const u8 *addr, u16 status_code, int reassoc,
			   const u8 *ies, size_t ies_len, int rssi,
			   int omit_rsnxe, bool allow_mld_addr_trans)
{
	int send_len;
	u8 *buf;
	size_t buflen;
	struct ieee80211_mgmt *reply;
	u8 *p;
	u16 res = WLAN_STATUS_SUCCESS;

	buflen = sizeof(struct ieee80211_mgmt) + 1024;
#ifdef CONFIG_FILS
	if (sta && sta->fils_hlp_resp)
		buflen += wpabuf_len(sta->fils_hlp_resp);
	if (sta)
		buflen += 150;
#endif /* CONFIG_FILS */
#ifdef CONFIG_OWE
	if (sta && (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_OWE))
		buflen += 150;
#endif /* CONFIG_OWE */
#ifdef CONFIG_DPP2
	if (sta && sta->dpp_pfs)
		buflen += 5 + sta->dpp_pfs->curve->prime_len;
#endif /* CONFIG_DPP2 */
#ifdef CONFIG_IEEE80211BE
	if (hapd->iconf->ieee80211be && !hapd->conf->disable_11be) {
		buflen += hostapd_eid_eht_capab_len(hapd, IEEE80211_MODE_AP);
		buflen += 3 + sizeof(struct ieee80211_eht_operation);
		if (hapd->iconf->punct_bitmap)
			buflen += EHT_OPER_DISABLED_SUBCHAN_BITMAP_SIZE;
	}
#endif /* CONFIG_IEEE80211BE */

	buf = os_zalloc(buflen);
	if (!buf) {
		res = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto done;
	}
	reply = (struct ieee80211_mgmt *) buf;
	reply->frame_control =
		IEEE80211_FC(WLAN_FC_TYPE_MGMT,
			     (reassoc ? WLAN_FC_STYPE_REASSOC_RESP :
			      WLAN_FC_STYPE_ASSOC_RESP));

	os_memcpy(reply->da, addr, ETH_ALEN);
	os_memcpy(reply->sa, hapd->own_addr, ETH_ALEN);
	os_memcpy(reply->bssid, hapd->own_addr, ETH_ALEN);

	send_len = IEEE80211_HDRLEN;
	send_len += sizeof(reply->u.assoc_resp);
	reply->u.assoc_resp.capab_info =
		host_to_le16(hostapd_own_capab_info(hapd));
	reply->u.assoc_resp.status_code = host_to_le16(status_code);

	reply->u.assoc_resp.aid = host_to_le16((sta ? sta->aid : 0) |
					       BIT(14) | BIT(15));
	/* Supported rates */
	p = hostapd_eid_supp_rates(hapd, reply->u.assoc_resp.variable);
	/* Extended supported rates */
	p = hostapd_eid_ext_supp_rates(hapd, p);

	/* Radio measurement capabilities */
	p = hostapd_eid_rm_enabled_capab(hapd, p, buf + buflen - p);

#ifdef CONFIG_MBO
	if (status_code == WLAN_STATUS_DENIED_POOR_CHANNEL_CONDITIONS &&
	    rssi != 0) {
		int delta = hapd->iconf->rssi_reject_assoc_rssi - rssi;

		p = hostapd_eid_mbo_rssi_assoc_rej(hapd, p, buf + buflen - p,
						   delta);
	}
#endif /* CONFIG_MBO */

#ifdef CONFIG_IEEE80211R_AP
	if (sta && status_code == WLAN_STATUS_SUCCESS) {
		/* IEEE 802.11r: Mobility Domain Information, Fast BSS
		 * Transition Information, RSN, [RIC Response] */
		p = wpa_sm_write_assoc_resp_ies(sta->wpa_sm, p,
						buf + buflen - p,
						sta->auth_alg, ies, ies_len,
						omit_rsnxe);
		if (!p) {
			wpa_printf(MSG_DEBUG,
				   "FT: Failed to write AssocResp IEs");
			res = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto done;
		}
	}
#endif /* CONFIG_IEEE80211R_AP */
#ifdef CONFIG_FILS
	if (sta && status_code == WLAN_STATUS_SUCCESS &&
	    (sta->auth_alg == WLAN_AUTH_FILS_SK ||
	     sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	     sta->auth_alg == WLAN_AUTH_FILS_PK))
		p = wpa_auth_write_assoc_resp_fils(sta->wpa_sm, p,
						   buf + buflen - p,
						   ies, ies_len);
#endif /* CONFIG_FILS */

#ifdef CONFIG_OWE
	if (sta && status_code == WLAN_STATUS_SUCCESS &&
	    (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_OWE))
		p = wpa_auth_write_assoc_resp_owe(sta->wpa_sm, p,
						  buf + buflen - p,
						  ies, ies_len);
#endif /* CONFIG_OWE */

	if (sta && status_code == WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY)
		p = hostapd_eid_assoc_comeback_time(hapd, sta, p);

	p = hostapd_eid_ht_capabilities(hapd, p);
	p = hostapd_eid_ht_operation(hapd, p);

#ifdef CONFIG_IEEE80211AC
	if (hapd->iconf->ieee80211ac && !hapd->conf->disable_11ac &&
	    !is_6ghz_op_class(hapd->iconf->op_class)) {
		u32 nsts = 0, sta_nsts;

		if (sta && hapd->conf->use_sta_nsts && sta->vht_capabilities) {
			struct ieee80211_vht_capabilities *capa;

			nsts = (hapd->iface->conf->vht_capab >>
				VHT_CAP_BEAMFORMEE_STS_OFFSET) & 7;
			capa = sta->vht_capabilities;
			sta_nsts = (le_to_host32(capa->vht_capabilities_info) >>
				    VHT_CAP_BEAMFORMEE_STS_OFFSET) & 7;

			if (nsts < sta_nsts)
				nsts = 0;
			else
				nsts = sta_nsts;
		}
		p = hostapd_eid_vht_capabilities(hapd, p, nsts);
		p = hostapd_eid_vht_operation(hapd, p);
	}
#endif /* CONFIG_IEEE80211AC */

#ifdef CONFIG_IEEE80211AX
	if (hapd->iconf->ieee80211ax && !hapd->conf->disable_11ax) {
		p = hostapd_eid_he_capab(hapd, p, IEEE80211_MODE_AP);
		p = hostapd_eid_he_operation(hapd, p);
		p = hostapd_eid_cca(hapd, p);
		p = hostapd_eid_spatial_reuse(hapd, p);
		p = hostapd_eid_he_mu_edca_parameter_set(hapd, p);
		p = hostapd_eid_he_6ghz_band_cap(hapd, p);
	}
#endif /* CONFIG_IEEE80211AX */

	p = hostapd_eid_ext_capab(hapd, p, false);
	p = hostapd_eid_bss_max_idle_period(hapd, p, sta->max_idle_period);
	if (sta && sta->qos_map_enabled)
		p = hostapd_eid_qos_map_set(hapd, p);

#ifdef CONFIG_FST
	if (hapd->iface->fst_ies) {
		os_memcpy(p, wpabuf_head(hapd->iface->fst_ies),
			  wpabuf_len(hapd->iface->fst_ies));
		p += wpabuf_len(hapd->iface->fst_ies);
	}
#endif /* CONFIG_FST */

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->rsnxe_override_ft &&
	    buf + buflen - p >=
	    (long int) wpabuf_len(hapd->conf->rsnxe_override_ft) &&
	    sta && sta->auth_alg == WLAN_AUTH_FT) {
		wpa_printf(MSG_DEBUG, "TESTING: RSNXE FT override");
		os_memcpy(p, wpabuf_head(hapd->conf->rsnxe_override_ft),
			  wpabuf_len(hapd->conf->rsnxe_override_ft));
		p += wpabuf_len(hapd->conf->rsnxe_override_ft);
		goto rsnxe_done;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	if (!omit_rsnxe)
		p = hostapd_eid_rsnxe(hapd, p, buf + buflen - p);
#ifdef CONFIG_TESTING_OPTIONS
rsnxe_done:
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_IEEE80211BE
	if (hapd->iconf->ieee80211be && !hapd->conf->disable_11be) {
		if (hapd->conf->mld_ap)
			p = hostapd_eid_eht_ml_assoc(hapd, sta, p);
		p = hostapd_eid_eht_capab(hapd, p, IEEE80211_MODE_AP);
		p = hostapd_eid_eht_operation(hapd, p);
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_OWE
	if ((hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_OWE) &&
	    sta && sta->owe_ecdh && status_code == WLAN_STATUS_SUCCESS &&
	    wpa_auth_sta_key_mgmt(sta->wpa_sm) == WPA_KEY_MGMT_OWE &&
	    !wpa_auth_sta_get_pmksa(sta->wpa_sm)) {
		struct wpabuf *pub;

		pub = crypto_ecdh_get_pubkey(sta->owe_ecdh, 0);
		if (!pub) {
			res = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto done;
		}
		/* OWE Diffie-Hellman Parameter element */
		*p++ = WLAN_EID_EXTENSION; /* Element ID */
		*p++ = 1 + 2 + wpabuf_len(pub); /* Length */
		*p++ = WLAN_EID_EXT_OWE_DH_PARAM; /* Element ID Extension */
		WPA_PUT_LE16(p, sta->owe_group);
		p += 2;
		os_memcpy(p, wpabuf_head(pub), wpabuf_len(pub));
		p += wpabuf_len(pub);
		wpabuf_free(pub);
	}
#endif /* CONFIG_OWE */

#ifdef CONFIG_DPP2
	if (DPP_VERSION > 1 && (hapd->conf->wpa_key_mgmt & WPA_KEY_MGMT_DPP) &&
	    sta && sta->dpp_pfs && status_code == WLAN_STATUS_SUCCESS &&
	    wpa_auth_sta_key_mgmt(sta->wpa_sm) == WPA_KEY_MGMT_DPP) {
		os_memcpy(p, wpabuf_head(sta->dpp_pfs->ie),
			  wpabuf_len(sta->dpp_pfs->ie));
		p += wpabuf_len(sta->dpp_pfs->ie);
	}
#endif /* CONFIG_DPP2 */

#ifdef CONFIG_IEEE80211AC
	if (sta && hapd->conf->vendor_vht && (sta->flags & WLAN_STA_VENDOR_VHT))
		p = hostapd_eid_vendor_vht(hapd, p);
#endif /* CONFIG_IEEE80211AC */

	if (sta && (sta->flags & WLAN_STA_WMM))
		p = hostapd_eid_wmm(hapd, p);

#ifdef CONFIG_WPS
	if (sta &&
	    ((sta->flags & WLAN_STA_WPS) ||
	     ((sta->flags & WLAN_STA_MAYBE_WPS) && hapd->conf->wpa))) {
		struct wpabuf *wps = wps_build_assoc_resp_ie();
		if (wps) {
			os_memcpy(p, wpabuf_head(wps), wpabuf_len(wps));
			p += wpabuf_len(wps);
			wpabuf_free(wps);
		}
	}
#endif /* CONFIG_WPS */

	if (sta && (sta->flags & WLAN_STA_MULTI_AP))
		p = hostapd_eid_multi_ap(hapd, p, buf + buflen - p);

#ifdef CONFIG_P2P
	if (sta && sta->p2p_ie && hapd->p2p_group) {
		struct wpabuf *p2p_resp_ie;
		enum p2p_status_code status;
		switch (status_code) {
		case WLAN_STATUS_SUCCESS:
			status = P2P_SC_SUCCESS;
			break;
		case WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA:
			status = P2P_SC_FAIL_LIMIT_REACHED;
			break;
		default:
			status = P2P_SC_FAIL_INVALID_PARAMS;
			break;
		}
		p2p_resp_ie = p2p_group_assoc_resp_ie(hapd->p2p_group, status);
		if (p2p_resp_ie) {
			os_memcpy(p, wpabuf_head(p2p_resp_ie),
				  wpabuf_len(p2p_resp_ie));
			p += wpabuf_len(p2p_resp_ie);
			wpabuf_free(p2p_resp_ie);
		}
	}
#endif /* CONFIG_P2P */

#ifdef CONFIG_P2P_MANAGER
	if (hapd->conf->p2p & P2P_MANAGE)
		p = hostapd_eid_p2p_manage(hapd, p);
#endif /* CONFIG_P2P_MANAGER */

	p = hostapd_eid_mbo(hapd, p, buf + buflen - p);

	if (hapd->conf->assocresp_elements &&
	    (size_t) (buf + buflen - p) >=
	    wpabuf_len(hapd->conf->assocresp_elements)) {
		os_memcpy(p, wpabuf_head(hapd->conf->assocresp_elements),
			  wpabuf_len(hapd->conf->assocresp_elements));
		p += wpabuf_len(hapd->conf->assocresp_elements);
	}

	send_len += p - reply->u.assoc_resp.variable;

#ifdef CONFIG_FILS
	if (sta &&
	    (sta->auth_alg == WLAN_AUTH_FILS_SK ||
	     sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	     sta->auth_alg == WLAN_AUTH_FILS_PK) &&
	    status_code == WLAN_STATUS_SUCCESS) {
		struct ieee802_11_elems elems;

		if (ieee802_11_parse_elems(ies, ies_len, &elems, 0) ==
		    ParseFailed || !elems.fils_session) {
			res = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto done;
		}

		/* FILS Session */
		*p++ = WLAN_EID_EXTENSION; /* Element ID */
		*p++ = 1 + FILS_SESSION_LEN; /* Length */
		*p++ = WLAN_EID_EXT_FILS_SESSION; /* Element ID Extension */
		os_memcpy(p, elems.fils_session, FILS_SESSION_LEN);
		send_len += 2 + 1 + FILS_SESSION_LEN;

		send_len = fils_encrypt_assoc(sta->wpa_sm, buf, send_len,
					      buflen, sta->fils_hlp_resp);
		if (send_len < 0) {
			res = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto done;
		}
	}
#endif /* CONFIG_FILS */

	if (hostapd_drv_send_mlme(hapd, reply, send_len, 0, NULL, 0, 0) < 0) {
		wpa_printf(MSG_INFO, "Failed to send assoc resp: %s",
			   strerror(errno));
		res = WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

done:
	os_free(buf);
	return res;
}


#ifdef CONFIG_OWE
u8 * owe_assoc_req_process(struct hostapd_data *hapd, struct sta_info *sta,
			   const u8 *owe_dh, u8 owe_dh_len,
			   u8 *owe_buf, size_t owe_buf_len, u16 *status)
{
#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->conf->own_ie_override) {
		wpa_printf(MSG_DEBUG, "OWE: Using IE override");
		*status = WLAN_STATUS_SUCCESS;
		return wpa_auth_write_assoc_resp_owe(sta->wpa_sm, owe_buf,
						     owe_buf_len, NULL, 0);
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (wpa_auth_sta_get_pmksa(sta->wpa_sm)) {
		wpa_printf(MSG_DEBUG, "OWE: Using PMKSA caching");
		owe_buf = wpa_auth_write_assoc_resp_owe(sta->wpa_sm, owe_buf,
							owe_buf_len, NULL, 0);
		*status = WLAN_STATUS_SUCCESS;
		return owe_buf;
	}

	if (sta->owe_pmk && sta->external_dh_updated) {
		wpa_printf(MSG_DEBUG, "OWE: Using previously derived PMK");
		*status = WLAN_STATUS_SUCCESS;
		return owe_buf;
	}

	*status = owe_process_assoc_req(hapd, sta, owe_dh, owe_dh_len);
	if (*status != WLAN_STATUS_SUCCESS)
		return NULL;

	owe_buf = wpa_auth_write_assoc_resp_owe(sta->wpa_sm, owe_buf,
						owe_buf_len, NULL, 0);

	if (sta->owe_ecdh && owe_buf) {
		struct wpabuf *pub;

		pub = crypto_ecdh_get_pubkey(sta->owe_ecdh, 0);
		if (!pub) {
			*status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			return owe_buf;
		}

		/* OWE Diffie-Hellman Parameter element */
		*owe_buf++ = WLAN_EID_EXTENSION; /* Element ID */
		*owe_buf++ = 1 + 2 + wpabuf_len(pub); /* Length */
		*owe_buf++ = WLAN_EID_EXT_OWE_DH_PARAM; /* Element ID Extension
							 */
		WPA_PUT_LE16(owe_buf, sta->owe_group);
		owe_buf += 2;
		os_memcpy(owe_buf, wpabuf_head(pub), wpabuf_len(pub));
		owe_buf += wpabuf_len(pub);
		wpabuf_free(pub);
	}

	return owe_buf;
}
#endif /* CONFIG_OWE */


#ifdef CONFIG_FILS

void fils_hlp_finish_assoc(struct hostapd_data *hapd, struct sta_info *sta)
{
	u16 reply_res;

	wpa_printf(MSG_DEBUG, "FILS: Finish association with " MACSTR,
		   MAC2STR(sta->addr));
	eloop_cancel_timeout(fils_hlp_timeout, hapd, sta);
	if (!sta->fils_pending_assoc_req)
		return;
	reply_res = send_assoc_resp(hapd, sta, sta->addr, WLAN_STATUS_SUCCESS,
				    sta->fils_pending_assoc_is_reassoc,
				    sta->fils_pending_assoc_req,
				    sta->fils_pending_assoc_req_len, 0, 0,
				    true);
	os_free(sta->fils_pending_assoc_req);
	sta->fils_pending_assoc_req = NULL;
	sta->fils_pending_assoc_req_len = 0;
	wpabuf_free(sta->fils_hlp_resp);
	sta->fils_hlp_resp = NULL;
	wpabuf_free(sta->hlp_dhcp_discover);
	sta->hlp_dhcp_discover = NULL;

	/*
	 * Remove the station in case transmission of a success response fails.
	 * At this point the station was already added associated to the driver.
	 */
	if (reply_res != WLAN_STATUS_SUCCESS)
		hostapd_drv_sta_remove(hapd, sta->addr);
}


void fils_hlp_timeout(void *eloop_ctx, void *eloop_data)
{
	struct hostapd_data *hapd = eloop_ctx;
	struct sta_info *sta = eloop_data;

	wpa_printf(MSG_DEBUG,
		   "FILS: HLP response timeout - continue with association response for "
		   MACSTR, MAC2STR(sta->addr));
	if (sta->fils_drv_assoc_finish)
		hostapd_notify_assoc_fils_finish(hapd, sta);
	else
		fils_hlp_finish_assoc(hapd, sta);
}

#endif /* CONFIG_FILS */


#ifdef CONFIG_IEEE80211BE
static struct sta_info * handle_mlo_translate(struct hostapd_data *hapd,
					      const struct ieee80211_mgmt *mgmt,
					      size_t len, bool reassoc,
					      struct hostapd_data **assoc_hapd)
{
	struct sta_info *sta;
	struct ieee802_11_elems elems;
	u8 mld_addr[ETH_ALEN];
	const u8 *pos;

	if (!hapd->iconf->ieee80211be || hapd->conf->disable_11be)
		return NULL;

	if (reassoc) {
		len -= IEEE80211_HDRLEN + sizeof(mgmt->u.reassoc_req);
		pos = mgmt->u.reassoc_req.variable;
	} else {
		len -= IEEE80211_HDRLEN + sizeof(mgmt->u.assoc_req);
		pos = mgmt->u.assoc_req.variable;
	}

	if (ieee802_11_parse_elems(pos, len, &elems, 1) == ParseFailed)
		return NULL;

	if (hostapd_process_ml_assoc_req_addr(hapd, elems.basic_mle,
					      elems.basic_mle_len,
					      mld_addr))
		return NULL;

	sta = ap_get_sta(hapd, mld_addr);
	if (!sta)
		return NULL;

	wpa_printf(MSG_DEBUG, "MLD: assoc: mld=" MACSTR ", link=" MACSTR,
		   MAC2STR(mld_addr), MAC2STR(mgmt->sa));

	return hostapd_ml_get_assoc_sta(hapd, sta, assoc_hapd);
}
#endif /* CONFIG_IEEE80211BE */


static void handle_assoc(struct hostapd_data *hapd,
			 const struct ieee80211_mgmt *mgmt, size_t len,
			 int reassoc, int rssi)
{
	u16 capab_info, listen_interval, seq_ctrl, fc;
	int resp = WLAN_STATUS_SUCCESS;
	u16 reply_res = WLAN_STATUS_UNSPECIFIED_FAILURE;
	const u8 *pos;
	int left, i;
	struct sta_info *sta;
	u8 *tmp = NULL;
#ifdef CONFIG_FILS
	int delay_assoc = 0;
#endif /* CONFIG_FILS */
	int omit_rsnxe = 0;
	bool set_beacon = false;
	bool mld_addrs_not_translated = false;

	if (len < IEEE80211_HDRLEN + (reassoc ? sizeof(mgmt->u.reassoc_req) :
				      sizeof(mgmt->u.assoc_req))) {
		wpa_printf(MSG_INFO, "handle_assoc(reassoc=%d) - too short payload (len=%lu)",
			   reassoc, (unsigned long) len);
		return;
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (reassoc) {
		if (hapd->iconf->ignore_reassoc_probability > 0.0 &&
		    drand48() < hapd->iconf->ignore_reassoc_probability) {
			wpa_printf(MSG_INFO,
				   "TESTING: ignoring reassoc request from "
				   MACSTR, MAC2STR(mgmt->sa));
			return;
		}
	} else {
		if (hapd->iconf->ignore_assoc_probability > 0.0 &&
		    drand48() < hapd->iconf->ignore_assoc_probability) {
			wpa_printf(MSG_INFO,
				   "TESTING: ignoring assoc request from "
				   MACSTR, MAC2STR(mgmt->sa));
			return;
		}
	}
#endif /* CONFIG_TESTING_OPTIONS */

	fc = le_to_host16(mgmt->frame_control);
	seq_ctrl = le_to_host16(mgmt->seq_ctrl);

	if (reassoc) {
		capab_info = le_to_host16(mgmt->u.reassoc_req.capab_info);
		listen_interval = le_to_host16(
			mgmt->u.reassoc_req.listen_interval);
		wpa_printf(MSG_DEBUG, "reassociation request: STA=" MACSTR
			   " capab_info=0x%02x listen_interval=%d current_ap="
			   MACSTR " seq_ctrl=0x%x%s",
			   MAC2STR(mgmt->sa), capab_info, listen_interval,
			   MAC2STR(mgmt->u.reassoc_req.current_ap),
			   seq_ctrl, (fc & WLAN_FC_RETRY) ? " retry" : "");
		left = len - (IEEE80211_HDRLEN + sizeof(mgmt->u.reassoc_req));
		pos = mgmt->u.reassoc_req.variable;
	} else {
		capab_info = le_to_host16(mgmt->u.assoc_req.capab_info);
		listen_interval = le_to_host16(
			mgmt->u.assoc_req.listen_interval);
		wpa_printf(MSG_DEBUG, "association request: STA=" MACSTR
			   " capab_info=0x%02x listen_interval=%d "
			   "seq_ctrl=0x%x%s",
			   MAC2STR(mgmt->sa), capab_info, listen_interval,
			   seq_ctrl, (fc & WLAN_FC_RETRY) ? " retry" : "");
		left = len - (IEEE80211_HDRLEN + sizeof(mgmt->u.assoc_req));
		pos = mgmt->u.assoc_req.variable;
	}

	sta = ap_get_sta(hapd, mgmt->sa);

#ifdef CONFIG_IEEE80211BE
	/*
	 * It is possible that the association frame is from an associated
	 * non-AP MLD station, that tries to re-associate using different link
	 * addresses. In such a case, try to find the station based on the AP
	 * MLD MAC address.
	 */
	if (!sta) {
		struct hostapd_data *assoc_hapd;

		sta = handle_mlo_translate(hapd, mgmt, len, reassoc,
					   &assoc_hapd);
		if (sta) {
			wpa_printf(MSG_DEBUG,
				   "MLD: Switching to assoc hapd/station");
			hapd = assoc_hapd;
			mld_addrs_not_translated = true;
		}
	}
#endif /* CONFIG_IEEE80211BE */

#ifdef CONFIG_IEEE80211R_AP
	if (sta && sta->auth_alg == WLAN_AUTH_FT &&
	    (sta->flags & WLAN_STA_AUTH) == 0) {
		wpa_printf(MSG_DEBUG, "FT: Allow STA " MACSTR " to associate "
			   "prior to authentication since it is using "
			   "over-the-DS FT", MAC2STR(mgmt->sa));

		/*
		 * Mark station as authenticated, to avoid adding station
		 * entry in the driver as associated and not authenticated
		 */
		sta->flags |= WLAN_STA_AUTH;
	} else
#endif /* CONFIG_IEEE80211R_AP */
	if (sta == NULL || (sta->flags & WLAN_STA_AUTH) == 0) {
		if (hapd->iface->current_mode &&
		    hapd->iface->current_mode->mode ==
			HOSTAPD_MODE_IEEE80211AD) {
			int acl_res;
			struct radius_sta info;

			acl_res = ieee802_11_allowed_address(hapd, mgmt->sa,
							     (const u8 *) mgmt,
							     len, &info);
			if (acl_res == HOSTAPD_ACL_REJECT) {
				wpa_msg(hapd->msg_ctx, MSG_DEBUG,
					"Ignore Association Request frame from "
					MACSTR " due to ACL reject",
					MAC2STR(mgmt->sa));
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto fail;
			}
			if (acl_res == HOSTAPD_ACL_PENDING)
				return;

			/* DMG/IEEE 802.11ad does not use authentication.
			 * Allocate sta entry upon association. */
			sta = ap_sta_add(hapd, mgmt->sa);
			if (!sta) {
				hostapd_logger(hapd, mgmt->sa,
					       HOSTAPD_MODULE_IEEE80211,
					       HOSTAPD_LEVEL_INFO,
					       "Failed to add STA");
				resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
				goto fail;
			}

			acl_res = ieee802_11_set_radius_info(
				hapd, sta, acl_res, &info);
			if (acl_res) {
				resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto fail;
			}

			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG,
				       "Skip authentication for DMG/IEEE 802.11ad");
			sta->flags |= WLAN_STA_AUTH;
			wpa_auth_sm_event(sta->wpa_sm, WPA_AUTH);
			sta->auth_alg = WLAN_AUTH_OPEN;
		} else {
			hostapd_logger(hapd, mgmt->sa,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_INFO,
				       "Station tried to associate before authentication (aid=%d flags=0x%x)",
				       sta ? sta->aid : -1,
				       sta ? sta->flags : 0);
			send_deauth(hapd, mgmt->sa,
				    WLAN_REASON_CLASS2_FRAME_FROM_NONAUTH_STA);
			return;
		}
	}

	if ((fc & WLAN_FC_RETRY) &&
	    sta->last_seq_ctrl != WLAN_INVALID_MGMT_SEQ &&
	    sta->last_seq_ctrl == seq_ctrl &&
	    sta->last_subtype == (reassoc ? WLAN_FC_STYPE_REASSOC_REQ :
				  WLAN_FC_STYPE_ASSOC_REQ)) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "Drop repeated association frame seq_ctrl=0x%x",
			       seq_ctrl);
		return;
	}
	sta->last_seq_ctrl = seq_ctrl;
	sta->last_subtype = reassoc ? WLAN_FC_STYPE_REASSOC_REQ :
		WLAN_FC_STYPE_ASSOC_REQ;

	if (hapd->tkip_countermeasures) {
		resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	if (listen_interval > hapd->conf->max_listen_interval) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "Too large Listen Interval (%d)",
			       listen_interval);
		resp = WLAN_STATUS_ASSOC_DENIED_LISTEN_INT_TOO_LARGE;
		goto fail;
	}

#ifdef CONFIG_MBO
	if (hapd->conf->mbo_enabled && hapd->mbo_assoc_disallow) {
		resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
		goto fail;
	}

	if (hapd->iconf->rssi_reject_assoc_rssi && rssi &&
	    rssi < hapd->iconf->rssi_reject_assoc_rssi &&
	    (sta->auth_rssi == 0 ||
	     sta->auth_rssi < hapd->iconf->rssi_reject_assoc_rssi)) {
		resp = WLAN_STATUS_DENIED_POOR_CHANNEL_CONDITIONS;
		goto fail;
	}
#endif /* CONFIG_MBO */

	if (hapd->conf->wpa && check_sa_query(hapd, sta, reassoc)) {
		resp = WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY;
		goto fail;
	}

	/*
	 * sta->capability is used in check_assoc_ies() for RRM enabled
	 * capability element.
	 */
	sta->capability = capab_info;

#ifdef CONFIG_FILS
	if (sta->auth_alg == WLAN_AUTH_FILS_SK ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	    sta->auth_alg == WLAN_AUTH_FILS_PK) {
		int res;

		/* The end of the payload is encrypted. Need to decrypt it
		 * before parsing. */

		tmp = os_memdup(pos, left);
		if (!tmp) {
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}

		res = fils_decrypt_assoc(sta->wpa_sm, sta->fils_session, mgmt,
					 len, tmp, left);
		if (res < 0) {
			resp = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto fail;
		}
		pos = tmp;
		left = res;
	}
#endif /* CONFIG_FILS */

	/* followed by SSID and Supported rates; and HT capabilities if 802.11n
	 * is used */
	resp = check_assoc_ies(hapd, sta, pos, left, reassoc);
	if (resp != WLAN_STATUS_SUCCESS)
		goto fail;
	omit_rsnxe = !get_ie(pos, left, WLAN_EID_RSNX);

	if (hostapd_get_aid(hapd, sta) < 0) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "No room for more AIDs");
		resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;
		goto fail;
	}

	sta->listen_interval = listen_interval;

	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G)
		sta->flags |= WLAN_STA_NONERP;
	for (i = 0; i < sta->supported_rates_len; i++) {
		if ((sta->supported_rates[i] & 0x7f) > 22) {
			sta->flags &= ~WLAN_STA_NONERP;
			break;
		}
	}
	if (sta->flags & WLAN_STA_NONERP && !sta->nonerp_set) {
		sta->nonerp_set = 1;
		hapd->iface->num_sta_non_erp++;
		if (hapd->iface->num_sta_non_erp == 1)
			set_beacon = true;
	}

	if (!(sta->capability & WLAN_CAPABILITY_SHORT_SLOT_TIME) &&
	    !sta->no_short_slot_time_set) {
		sta->no_short_slot_time_set = 1;
		hapd->iface->num_sta_no_short_slot_time++;
		if (hapd->iface->current_mode &&
		    hapd->iface->current_mode->mode ==
		    HOSTAPD_MODE_IEEE80211G &&
		    hapd->iface->num_sta_no_short_slot_time == 1)
			set_beacon = true;
	}

	if (sta->capability & WLAN_CAPABILITY_SHORT_PREAMBLE)
		sta->flags |= WLAN_STA_SHORT_PREAMBLE;
	else
		sta->flags &= ~WLAN_STA_SHORT_PREAMBLE;

	if (!(sta->capability & WLAN_CAPABILITY_SHORT_PREAMBLE) &&
	    !sta->no_short_preamble_set) {
		sta->no_short_preamble_set = 1;
		hapd->iface->num_sta_no_short_preamble++;
		if (hapd->iface->current_mode &&
		    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211G
		    && hapd->iface->num_sta_no_short_preamble == 1)
			set_beacon = true;
	}

	if (update_ht_state(hapd, sta) > 0)
		set_beacon = true;

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "association OK (aid %d)", sta->aid);
	/* Station will be marked associated, after it acknowledges AssocResp
	 */
	sta->flags |= WLAN_STA_ASSOC_REQ_OK;

	if ((sta->flags & WLAN_STA_MFP) && sta->sa_query_timed_out) {
		wpa_printf(MSG_DEBUG, "Allowing %sassociation after timed out "
			   "SA Query procedure", reassoc ? "re" : "");
		/* TODO: Send a protected Disassociate frame to the STA using
		 * the old key and Reason Code "Previous Authentication no
		 * longer valid". Make sure this is only sent protected since
		 * unprotected frame would be received by the STA that is now
		 * trying to associate.
		 */
	}

	/* Make sure that the previously registered inactivity timer will not
	 * remove the STA immediately. */
	sta->timeout_next = STA_NULLFUNC;

#ifdef CONFIG_TAXONOMY
	taxonomy_sta_info_assoc_req(hapd, sta, pos, left);
#endif /* CONFIG_TAXONOMY */

	sta->pending_wds_enable = 0;

#ifdef CONFIG_FILS
	if (sta->auth_alg == WLAN_AUTH_FILS_SK ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	    sta->auth_alg == WLAN_AUTH_FILS_PK) {
		if (fils_process_hlp(hapd, sta, pos, left) > 0)
			delay_assoc = 1;
	}
#endif /* CONFIG_FILS */

	if (set_beacon)
		ieee802_11_update_beacons(hapd->iface);

 fail:

	/*
	 * In case of a successful response, add the station to the driver.
	 * Otherwise, the kernel may ignore Data frames before we process the
	 * ACK frame (TX status). In case of a failure, this station will be
	 * removed.
	 *
	 * Note that this is not compliant with the IEEE 802.11 standard that
	 * states that a non-AP station should transition into the
	 * authenticated/associated state only after the station acknowledges
	 * the (Re)Association Response frame. However, still do this as:
	 *
	 * 1. In case the station does not acknowledge the (Re)Association
	 *    Response frame, it will be removed.
	 * 2. Data frames will be dropped in the kernel until the station is
	 *    set into authorized state, and there are no significant known
	 *    issues with processing other non-Data Class 3 frames during this
	 *    window.
	 */
	if (sta)
		hostapd_process_assoc_ml_info(hapd, sta, pos, left, reassoc,
					      resp, false);

	if (resp == WLAN_STATUS_SUCCESS && sta &&
	    add_associated_sta(hapd, sta, reassoc))
		resp = WLAN_STATUS_AP_UNABLE_TO_HANDLE_NEW_STA;

#ifdef CONFIG_FILS
	if (sta && delay_assoc && resp == WLAN_STATUS_SUCCESS &&
	    eloop_is_timeout_registered(fils_hlp_timeout, hapd, sta) &&
	    sta->fils_pending_assoc_req) {
		/* Do not reschedule fils_hlp_timeout in case the station
		 * retransmits (Re)Association Request frame while waiting for
		 * the previously started FILS HLP wait, so that the timeout can
		 * be determined from the first pending attempt. */
		wpa_printf(MSG_DEBUG,
			   "FILS: Continue waiting for HLP processing before sending (Re)Association Response frame to "
			   MACSTR, MAC2STR(sta->addr));
		os_free(tmp);
		return;
	}
	if (sta) {
		eloop_cancel_timeout(fils_hlp_timeout, hapd, sta);
		os_free(sta->fils_pending_assoc_req);
		sta->fils_pending_assoc_req = NULL;
		sta->fils_pending_assoc_req_len = 0;
		wpabuf_free(sta->fils_hlp_resp);
		sta->fils_hlp_resp = NULL;
	}
	if (sta && delay_assoc && resp == WLAN_STATUS_SUCCESS) {
		sta->fils_pending_assoc_req = tmp;
		sta->fils_pending_assoc_req_len = left;
		sta->fils_pending_assoc_is_reassoc = reassoc;
		sta->fils_drv_assoc_finish = 0;
		wpa_printf(MSG_DEBUG,
			   "FILS: Waiting for HLP processing before sending (Re)Association Response frame to "
			   MACSTR, MAC2STR(sta->addr));
		eloop_cancel_timeout(fils_hlp_timeout, hapd, sta);
		eloop_register_timeout(0, hapd->conf->fils_hlp_wait_time * 1024,
				       fils_hlp_timeout, hapd, sta);
		return;
	}
#endif /* CONFIG_FILS */

	if (resp >= 0)
		reply_res = send_assoc_resp(hapd,
					    mld_addrs_not_translated ?
					    NULL : sta,
					    mgmt->sa, resp, reassoc,
					    pos, left, rssi, omit_rsnxe,
					    !mld_addrs_not_translated);
	os_free(tmp);

	/*
	 * Remove the station in case transmission of a success response fails
	 * (the STA was added associated to the driver) or if the station was
	 * previously added unassociated.
	 */
	if (sta && ((reply_res != WLAN_STATUS_SUCCESS &&
		     resp == WLAN_STATUS_SUCCESS) || sta->added_unassoc)) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		sta->added_unassoc = 0;
	}
}


static void hostapd_deauth_sta(struct hostapd_data *hapd,
			       struct sta_info *sta,
			       const struct ieee80211_mgmt *mgmt)
{
	wpa_msg(hapd->msg_ctx, MSG_DEBUG,
		"deauthentication: STA=" MACSTR " reason_code=%d",
		MAC2STR(mgmt->sa), le_to_host16(mgmt->u.deauth.reason_code));

	ap_sta_set_authorized(hapd, sta, 0);
	sta->last_seq_ctrl = WLAN_INVALID_MGMT_SEQ;
	sta->flags &= ~(WLAN_STA_AUTH | WLAN_STA_ASSOC |
			WLAN_STA_ASSOC_REQ_OK);
	hostapd_set_sta_flags(hapd, sta);
	wpa_auth_sm_event(sta->wpa_sm, WPA_DEAUTH);
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG, "deauthenticated");
	mlme_deauthenticate_indication(
		hapd, sta, le_to_host16(mgmt->u.deauth.reason_code));
	sta->acct_terminate_cause = RADIUS_ACCT_TERMINATE_CAUSE_USER_REQUEST;
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
	ap_free_sta(hapd, sta);
}


static void hostapd_disassoc_sta(struct hostapd_data *hapd,
				 struct sta_info *sta,
				 const struct ieee80211_mgmt *mgmt)
{
	wpa_msg(hapd->msg_ctx, MSG_DEBUG,
		"disassocation: STA=" MACSTR " reason_code=%d",
		MAC2STR(mgmt->sa), le_to_host16(mgmt->u.disassoc.reason_code));

	ap_sta_set_authorized(hapd, sta, 0);
	sta->last_seq_ctrl = WLAN_INVALID_MGMT_SEQ;
	sta->flags &= ~(WLAN_STA_ASSOC | WLAN_STA_ASSOC_REQ_OK);
	hostapd_set_sta_flags(hapd, sta);
	wpa_auth_sm_event(sta->wpa_sm, WPA_DISASSOC);
	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO, "disassociated");
	sta->acct_terminate_cause = RADIUS_ACCT_TERMINATE_CAUSE_USER_REQUEST;
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 0);
	/* Stop Accounting and IEEE 802.1X sessions, but leave the STA
	 * authenticated. */
	accounting_sta_stop(hapd, sta);
	ieee802_1x_free_station(hapd, sta);
	if (sta->ipaddr)
		hostapd_drv_br_delete_ip_neigh(hapd, 4, (u8 *) &sta->ipaddr);
	ap_sta_ip6addr_del(hapd, sta);
	hostapd_drv_sta_remove(hapd, sta->addr);
	sta->added_unassoc = 0;

	if (sta->timeout_next == STA_NULLFUNC ||
	    sta->timeout_next == STA_DISASSOC) {
		sta->timeout_next = STA_DEAUTH;
		eloop_cancel_timeout(ap_handle_timer, hapd, sta);
		eloop_register_timeout(AP_DEAUTH_DELAY, 0, ap_handle_timer,
				       hapd, sta);
	}

	mlme_disassociate_indication(
		hapd, sta, le_to_host16(mgmt->u.disassoc.reason_code));

	/* DMG/IEEE 802.11ad does not use deauthication. Deallocate sta upon
	 * disassociation. */
	if (hapd->iface->current_mode &&
	    hapd->iface->current_mode->mode == HOSTAPD_MODE_IEEE80211AD) {
		sta->flags &= ~WLAN_STA_AUTH;
		wpa_auth_sm_event(sta->wpa_sm, WPA_DEAUTH);
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG, "deauthenticated");
		ap_free_sta(hapd, sta);
	}
}


static bool hostapd_ml_handle_disconnect(struct hostapd_data *hapd,
					 struct sta_info *sta,
					 const struct ieee80211_mgmt *mgmt,
					 bool disassoc)
{
#ifdef CONFIG_IEEE80211BE
	struct hostapd_data *assoc_hapd, *tmp_hapd;
	struct sta_info *assoc_sta;
	struct sta_info *tmp_sta;

	if (!hostapd_is_mld_ap(hapd))
		return false;

	/*
	 * Get the station on which the association was performed, as it holds
	 * the information about all the other links.
	 */
	assoc_sta = hostapd_ml_get_assoc_sta(hapd, sta, &assoc_hapd);
	if (!assoc_sta)
		return false;

	for_each_mld_link(tmp_hapd, assoc_hapd) {
		if (tmp_hapd == assoc_hapd)
			continue;

		if (!assoc_sta->mld_info.links[tmp_hapd->mld_link_id].valid)
			continue;

		for (tmp_sta = tmp_hapd->sta_list; tmp_sta;
		     tmp_sta = tmp_sta->next) {
			if (tmp_sta->mld_assoc_link_id !=
			    assoc_sta->mld_assoc_link_id ||
			    tmp_sta->aid != assoc_sta->aid)
				continue;

			if (!disassoc)
				hostapd_deauth_sta(tmp_hapd, tmp_sta, mgmt);
			else
				hostapd_disassoc_sta(tmp_hapd, tmp_sta, mgmt);
			break;
		}
	}

	/* Remove the station on which the association was performed. */
	if (!disassoc)
		hostapd_deauth_sta(assoc_hapd, assoc_sta, mgmt);
	else
		hostapd_disassoc_sta(assoc_hapd, assoc_sta, mgmt);

	return true;
#else /* CONFIG_IEEE80211BE */
	return false;
#endif /* CONFIG_IEEE80211BE */
}


static void handle_disassoc(struct hostapd_data *hapd,
			    const struct ieee80211_mgmt *mgmt, size_t len)
{
	struct sta_info *sta;

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.disassoc)) {
		wpa_msg(hapd->msg_ctx, MSG_DEBUG,
			   "handle_disassoc - too short payload (len=%lu)",
			   (unsigned long) len);
		return;
	}

	sta = ap_get_sta(hapd, mgmt->sa);
	if (!sta) {
		wpa_msg(hapd->msg_ctx, MSG_DEBUG, "Station " MACSTR
			" trying to disassociate, but it is not associated",
			MAC2STR(mgmt->sa));
		return;
	}

	if (hostapd_ml_handle_disconnect(hapd, sta, mgmt, true))
		return;

	hostapd_disassoc_sta(hapd, sta, mgmt);
}


static void handle_deauth(struct hostapd_data *hapd,
			  const struct ieee80211_mgmt *mgmt, size_t len)
{
	struct sta_info *sta;

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.deauth)) {
		wpa_msg(hapd->msg_ctx, MSG_DEBUG,
			"handle_deauth - too short payload (len=%lu)",
			(unsigned long) len);
		return;
	}

	/* Clear the PTKSA cache entries for PASN */
	ptksa_cache_flush(hapd->ptksa, mgmt->sa, WPA_CIPHER_NONE);

	sta = ap_get_sta(hapd, mgmt->sa);
	if (!sta) {
		wpa_msg(hapd->msg_ctx, MSG_DEBUG, "Station " MACSTR
			" trying to deauthenticate, but it is not authenticated",
			MAC2STR(mgmt->sa));
		return;
	}

	if (hostapd_ml_handle_disconnect(hapd, sta, mgmt, false))
		return;

	hostapd_deauth_sta(hapd, sta, mgmt);
}


static void handle_beacon(struct hostapd_data *hapd,
			  const struct ieee80211_mgmt *mgmt, size_t len,
			  struct hostapd_frame_info *fi)
{
	struct ieee802_11_elems elems;

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.beacon)) {
		wpa_printf(MSG_INFO, "handle_beacon - too short payload (len=%lu)",
			   (unsigned long) len);
		return;
	}

	(void) ieee802_11_parse_elems(mgmt->u.beacon.variable,
				      len - (IEEE80211_HDRLEN +
					     sizeof(mgmt->u.beacon)), &elems,
				      0);

	ap_list_process_beacon(hapd->iface, mgmt, &elems, fi);
}


static int robust_action_frame(u8 category)
{
	return category != WLAN_ACTION_PUBLIC &&
		category != WLAN_ACTION_HT;
}


static int handle_action(struct hostapd_data *hapd,
			 const struct ieee80211_mgmt *mgmt, size_t len,
			 unsigned int freq)
{
	struct sta_info *sta;
	u8 *action __maybe_unused;

	if (len < IEEE80211_HDRLEN + 2 + 1) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "handle_action - too short payload (len=%lu)",
			       (unsigned long) len);
		return 0;
	}

	action = (u8 *) &mgmt->u.action.u;
	wpa_printf(MSG_DEBUG, "RX_ACTION category %u action %u sa " MACSTR
		   " da " MACSTR " len %d freq %u",
		   mgmt->u.action.category, *action,
		   MAC2STR(mgmt->sa), MAC2STR(mgmt->da), (int) len, freq);

	sta = ap_get_sta(hapd, mgmt->sa);

	if (mgmt->u.action.category != WLAN_ACTION_PUBLIC &&
	    (sta == NULL || !(sta->flags & WLAN_STA_ASSOC))) {
		wpa_printf(MSG_DEBUG, "IEEE 802.11: Ignored Action "
			   "frame (category=%u) from unassociated STA " MACSTR,
			   mgmt->u.action.category, MAC2STR(mgmt->sa));
		return 0;
	}

	if (sta && (sta->flags & WLAN_STA_MFP) &&
	    !(mgmt->frame_control & host_to_le16(WLAN_FC_ISWEP)) &&
	    robust_action_frame(mgmt->u.action.category)) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "Dropped unprotected Robust Action frame from "
			       "an MFP STA");
		return 0;
	}

	if (sta) {
		u16 fc = le_to_host16(mgmt->frame_control);
		u16 seq_ctrl = le_to_host16(mgmt->seq_ctrl);

		if ((fc & WLAN_FC_RETRY) &&
		    sta->last_seq_ctrl != WLAN_INVALID_MGMT_SEQ &&
		    sta->last_seq_ctrl == seq_ctrl &&
		    sta->last_subtype == WLAN_FC_STYPE_ACTION) {
			hostapd_logger(hapd, sta->addr,
				       HOSTAPD_MODULE_IEEE80211,
				       HOSTAPD_LEVEL_DEBUG,
				       "Drop repeated action frame seq_ctrl=0x%x",
				       seq_ctrl);
			return 1;
		}

		sta->last_seq_ctrl = seq_ctrl;
		sta->last_subtype = WLAN_FC_STYPE_ACTION;
	}

	switch (mgmt->u.action.category) {
#ifdef CONFIG_IEEE80211R_AP
	case WLAN_ACTION_FT:
		if (!sta ||
		    wpa_ft_action_rx(sta->wpa_sm, (u8 *) &mgmt->u.action,
				     len - IEEE80211_HDRLEN))
			break;
		return 1;
#endif /* CONFIG_IEEE80211R_AP */
	case WLAN_ACTION_WMM:
		hostapd_wmm_action(hapd, mgmt, len);
		return 1;
	case WLAN_ACTION_SA_QUERY:
		ieee802_11_sa_query_action(hapd, mgmt, len);
		return 1;
#ifdef CONFIG_WNM_AP
	case WLAN_ACTION_WNM:
		ieee802_11_rx_wnm_action_ap(hapd, mgmt, len);
		return 1;
#endif /* CONFIG_WNM_AP */
#ifdef CONFIG_FST
	case WLAN_ACTION_FST:
		if (hapd->iface->fst)
			fst_rx_action(hapd->iface->fst, mgmt, len);
		else
			wpa_printf(MSG_DEBUG,
				   "FST: Ignore FST Action frame - no FST attached");
		return 1;
#endif /* CONFIG_FST */
	case WLAN_ACTION_PUBLIC:
	case WLAN_ACTION_PROTECTED_DUAL:
		if (len >= IEEE80211_HDRLEN + 2 &&
		    mgmt->u.action.u.public_action.action ==
		    WLAN_PA_20_40_BSS_COEX) {
			hostapd_2040_coex_action(hapd, mgmt, len);
			return 1;
		}
#ifdef CONFIG_DPP
		if (len >= IEEE80211_HDRLEN + 6 &&
		    mgmt->u.action.u.vs_public_action.action ==
		    WLAN_PA_VENDOR_SPECIFIC &&
		    WPA_GET_BE24(mgmt->u.action.u.vs_public_action.oui) ==
		    OUI_WFA &&
		    mgmt->u.action.u.vs_public_action.variable[0] ==
		    DPP_OUI_TYPE) {
			const u8 *pos, *end;

			pos = mgmt->u.action.u.vs_public_action.oui;
			end = ((const u8 *) mgmt) + len;
			hostapd_dpp_rx_action(hapd, mgmt->sa, pos, end - pos,
					      freq);
			return 1;
		}
		if (len >= IEEE80211_HDRLEN + 2 &&
		    (mgmt->u.action.u.public_action.action ==
		     WLAN_PA_GAS_INITIAL_RESP ||
		     mgmt->u.action.u.public_action.action ==
		     WLAN_PA_GAS_COMEBACK_RESP)) {
			const u8 *pos, *end;

			pos = &mgmt->u.action.u.public_action.action;
			end = ((const u8 *) mgmt) + len;
			if (gas_query_ap_rx(hapd->gas, mgmt->sa,
					    mgmt->u.action.category,
					    pos, end - pos, freq) == 0)
				return 1;
		}
#endif /* CONFIG_DPP */
#ifdef CONFIG_NAN_USD
		if (mgmt->u.action.category == WLAN_ACTION_PUBLIC &&
		    len >= IEEE80211_HDRLEN + 5 &&
		    mgmt->u.action.u.vs_public_action.action ==
		    WLAN_PA_VENDOR_SPECIFIC &&
		    WPA_GET_BE24(mgmt->u.action.u.vs_public_action.oui) ==
		    OUI_WFA &&
		    mgmt->u.action.u.vs_public_action.variable[0] ==
		    NAN_OUI_TYPE) {
			const u8 *pos, *end;

			pos = mgmt->u.action.u.vs_public_action.variable;
			end = ((const u8 *) mgmt) + len;
			pos++;
			hostapd_nan_usd_rx_sdf(hapd, mgmt->sa, freq,
					       pos, end - pos);
			return 1;
		}
#endif /* CONFIG_NAN_USD */
		if (hapd->public_action_cb) {
			hapd->public_action_cb(hapd->public_action_cb_ctx,
					       (u8 *) mgmt, len, freq);
		}
		if (hapd->public_action_cb2) {
			hapd->public_action_cb2(hapd->public_action_cb2_ctx,
						(u8 *) mgmt, len, freq);
		}
		if (hapd->public_action_cb || hapd->public_action_cb2)
			return 1;
		break;
	case WLAN_ACTION_VENDOR_SPECIFIC:
		if (hapd->vendor_action_cb) {
			if (hapd->vendor_action_cb(hapd->vendor_action_cb_ctx,
						   (u8 *) mgmt, len, freq) == 0)
				return 1;
		}
		break;
#ifndef CONFIG_NO_RRM
	case WLAN_ACTION_RADIO_MEASUREMENT:
		hostapd_handle_radio_measurement(hapd, (const u8 *) mgmt, len);
		return 1;
#endif /* CONFIG_NO_RRM */
	}

	hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_DEBUG,
		       "handle_action - unknown action category %d or invalid "
		       "frame",
		       mgmt->u.action.category);
	if (!is_multicast_ether_addr(mgmt->da) &&
	    !(mgmt->u.action.category & 0x80) &&
	    !is_multicast_ether_addr(mgmt->sa)) {
		struct ieee80211_mgmt *resp;

		/*
		 * IEEE 802.11-REVma/D9.0 - 7.3.1.11
		 * Return the Action frame to the source without change
		 * except that MSB of the Category set to 1.
		 */
		wpa_printf(MSG_DEBUG, "IEEE 802.11: Return unknown Action "
			   "frame back to sender");
		resp = os_memdup(mgmt, len);
		if (resp == NULL)
			return 0;
		os_memcpy(resp->da, resp->sa, ETH_ALEN);
		os_memcpy(resp->sa, hapd->own_addr, ETH_ALEN);
		os_memcpy(resp->bssid, hapd->own_addr, ETH_ALEN);
		resp->u.action.category |= 0x80;

		if (hostapd_drv_send_mlme(hapd, resp, len, 0, NULL, 0, 0) < 0) {
			wpa_printf(MSG_ERROR, "IEEE 802.11: Failed to send "
				   "Action frame");
		}
		os_free(resp);
	}

	return 1;
}


/**
 * notify_mgmt_frame - Notify of Management frames on the control interface
 * @hapd: hostapd BSS data structure (the BSS to which the Management frame was
 * sent to)
 * @buf: Management frame data (starting from the IEEE 802.11 header)
 * @len: Length of frame data in octets
 *
 * Notify the control interface of any received Management frame.
 */
static void notify_mgmt_frame(struct hostapd_data *hapd, const u8 *buf,
			      size_t len)
{

	int hex_len = len * 2 + 1;
	char *hex = os_malloc(hex_len);

	if (hex) {
		wpa_snprintf_hex(hex, hex_len, buf, len);
		wpa_msg_ctrl(hapd->msg_ctx, MSG_INFO,
			     AP_MGMT_FRAME_RECEIVED "buf=%s", hex);
		os_free(hex);
	}
}


/**
 * ieee802_11_mgmt - process incoming IEEE 802.11 management frames
 * @hapd: hostapd BSS data structure (the BSS to which the management frame was
 * sent to)
 * @buf: management frame data (starting from IEEE 802.11 header)
 * @len: length of frame data in octets
 * @fi: meta data about received frame (signal level, etc.)
 *
 * Process all incoming IEEE 802.11 management frames. This will be called for
 * each frame received from the kernel driver through wlan#ap interface. In
 * addition, it can be called to re-inserted pending frames (e.g., when using
 * external RADIUS server as an MAC ACL).
 */
int ieee802_11_mgmt(struct hostapd_data *hapd, const u8 *buf, size_t len,
		    struct hostapd_frame_info *fi)
{
	struct ieee80211_mgmt *mgmt;
	u16 fc, stype;
	int ret = 0;
	unsigned int freq;
	int ssi_signal = fi ? fi->ssi_signal : 0;
#ifdef CONFIG_NAN_USD
	static const u8 nan_network_id[ETH_ALEN] =
		{ 0x51, 0x6f, 0x9a, 0x01, 0x00, 0x00 };
#endif /* CONFIG_NAN_USD */

	if (len < 24)
		return 0;

	if (fi && fi->freq)
		freq = fi->freq;
	else
		freq = hapd->iface->freq;

	mgmt = (struct ieee80211_mgmt *) buf;
	fc = le_to_host16(mgmt->frame_control);
	stype = WLAN_FC_GET_STYPE(fc);

	if (is_multicast_ether_addr(mgmt->sa) ||
	    is_zero_ether_addr(mgmt->sa) ||
	    ether_addr_equal(mgmt->sa, hapd->own_addr)) {
		/* Do not process any frames with unexpected/invalid SA so that
		 * we do not add any state for unexpected STA addresses or end
		 * up sending out frames to unexpected destination. */
		wpa_printf(MSG_DEBUG, "MGMT: Invalid SA=" MACSTR
			   " in received frame - ignore this frame silently",
			   MAC2STR(mgmt->sa));
		return 0;
	}

	if (stype == WLAN_FC_STYPE_BEACON) {
		handle_beacon(hapd, mgmt, len, fi);
		return 1;
	}

	if (!is_broadcast_ether_addr(mgmt->bssid) &&
#ifdef CONFIG_P2P
	    /* Invitation responses can be sent with the peer MAC as BSSID */
	    !((hapd->conf->p2p & P2P_GROUP_OWNER) &&
	      stype == WLAN_FC_STYPE_ACTION) &&
#endif /* CONFIG_P2P */
#ifdef CONFIG_MESH
	    !(hapd->conf->mesh & MESH_ENABLED) &&
#endif /* CONFIG_MESH */
#ifdef CONFIG_IEEE80211BE
	    !(hapd->conf->mld_ap &&
	      ether_addr_equal(hapd->mld->mld_addr, mgmt->bssid)) &&
#endif /* CONFIG_IEEE80211BE */
	    !ether_addr_equal(mgmt->bssid, hapd->own_addr)) {
		wpa_printf(MSG_INFO, "MGMT: BSSID=" MACSTR " not our address",
			   MAC2STR(mgmt->bssid));
		return 0;
	}

	if (hapd->iface->state != HAPD_IFACE_ENABLED) {
		wpa_printf(MSG_DEBUG, "MGMT: Ignore management frame while interface is not enabled (SA=" MACSTR " DA=" MACSTR " subtype=%u)",
			   MAC2STR(mgmt->sa), MAC2STR(mgmt->da), stype);
		return 1;
	}

	if (stype == WLAN_FC_STYPE_PROBE_REQ) {
		handle_probe_req(hapd, mgmt, len, ssi_signal);
		return 1;
	}

	if ((!is_broadcast_ether_addr(mgmt->da) ||
	     stype != WLAN_FC_STYPE_ACTION) &&
#ifdef CONFIG_IEEE80211BE
	    !(hapd->conf->mld_ap &&
	      ether_addr_equal(hapd->mld->mld_addr, mgmt->bssid)) &&
#endif /* CONFIG_IEEE80211BE */
#ifdef CONFIG_NAN_USD
	    !ether_addr_equal(mgmt->da, nan_network_id) &&
#endif /* CONFIG_NAN_USD */
	    !ether_addr_equal(mgmt->da, hapd->own_addr)) {
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "MGMT: DA=" MACSTR " not our address",
			       MAC2STR(mgmt->da));
		return 0;
	}

	if (hapd->iconf->track_sta_max_num)
		sta_track_add(hapd->iface, mgmt->sa, ssi_signal);

	if (hapd->conf->notify_mgmt_frames)
		notify_mgmt_frame(hapd, buf, len);

	switch (stype) {
	case WLAN_FC_STYPE_AUTH:
		wpa_printf(MSG_DEBUG, "mgmt::auth");
		handle_auth(hapd, mgmt, len, ssi_signal, 0);
		ret = 1;
		break;
	case WLAN_FC_STYPE_ASSOC_REQ:
		wpa_printf(MSG_DEBUG, "mgmt::assoc_req");
		handle_assoc(hapd, mgmt, len, 0, ssi_signal);
		ret = 1;
		break;
	case WLAN_FC_STYPE_REASSOC_REQ:
		wpa_printf(MSG_DEBUG, "mgmt::reassoc_req");
		handle_assoc(hapd, mgmt, len, 1, ssi_signal);
		ret = 1;
		break;
	case WLAN_FC_STYPE_DISASSOC:
		wpa_printf(MSG_DEBUG, "mgmt::disassoc");
		handle_disassoc(hapd, mgmt, len);
		ret = 1;
		break;
	case WLAN_FC_STYPE_DEAUTH:
		wpa_msg(hapd->msg_ctx, MSG_DEBUG, "mgmt::deauth");
		handle_deauth(hapd, mgmt, len);
		ret = 1;
		break;
	case WLAN_FC_STYPE_ACTION:
		wpa_printf(MSG_DEBUG, "mgmt::action");
		ret = handle_action(hapd, mgmt, len, freq);
		break;
	default:
		hostapd_logger(hapd, mgmt->sa, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "unknown mgmt frame subtype %d", stype);
		break;
	}

	return ret;
}


static void handle_auth_cb(struct hostapd_data *hapd,
			   const struct ieee80211_mgmt *mgmt,
			   size_t len, int ok)
{
	u16 auth_alg, auth_transaction, status_code;
	struct sta_info *sta;
	bool success_status;

	sta = ap_get_sta(hapd, mgmt->da);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "handle_auth_cb: STA " MACSTR
			   " not found",
			   MAC2STR(mgmt->da));
		return;
	}

	if (len < IEEE80211_HDRLEN + sizeof(mgmt->u.auth)) {
		wpa_printf(MSG_INFO, "handle_auth_cb - too short payload (len=%lu)",
			   (unsigned long) len);
		auth_alg = 0;
		auth_transaction = 0;
		status_code = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto fail;
	}

	auth_alg = le_to_host16(mgmt->u.auth.auth_alg);
	auth_transaction = le_to_host16(mgmt->u.auth.auth_transaction);
	status_code = le_to_host16(mgmt->u.auth.status_code);

	if (!ok) {
		hostapd_logger(hapd, mgmt->da, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_NOTICE,
			       "did not acknowledge authentication response");
		goto fail;
	}

	if (status_code == WLAN_STATUS_SUCCESS &&
	    ((auth_alg == WLAN_AUTH_OPEN && auth_transaction == 2) ||
	     (auth_alg == WLAN_AUTH_SHARED_KEY && auth_transaction == 4))) {
		hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_INFO, "authenticated");
		sta->flags |= WLAN_STA_AUTH;
		if (sta->added_unassoc)
			hostapd_set_sta_flags(hapd, sta);
		return;
	}

fail:
	success_status = status_code == WLAN_STATUS_SUCCESS;
#ifdef CONFIG_SAE
	if (auth_alg == WLAN_AUTH_SAE && auth_transaction == 1)
		success_status = sae_status_success(hapd, status_code);
#endif /* CONFIG_SAE */
	if (!success_status && sta->added_unassoc) {
		hostapd_drv_sta_remove(hapd, sta->addr);
		sta->added_unassoc = 0;
	}
}


static void hostapd_set_wds_encryption(struct hostapd_data *hapd,
				       struct sta_info *sta,
				       char *ifname_wds)
{
#ifdef CONFIG_WEP
	int i;
	struct hostapd_ssid *ssid = &hapd->conf->ssid;

	if (hapd->conf->ieee802_1x || hapd->conf->wpa)
		return;

	for (i = 0; i < 4; i++) {
		if (ssid->wep.key[i] &&
		    hostapd_drv_set_key(ifname_wds, hapd, WPA_ALG_WEP, NULL, i,
					0, i == ssid->wep.idx, NULL, 0,
					ssid->wep.key[i], ssid->wep.len[i],
					i == ssid->wep.idx ?
					KEY_FLAG_GROUP_RX_TX_DEFAULT :
					KEY_FLAG_GROUP_RX_TX)) {
			wpa_printf(MSG_WARNING,
				   "Could not set WEP keys for WDS interface; %s",
				   ifname_wds);
			break;
		}
	}
#endif /* CONFIG_WEP */
}


#ifdef CONFIG_IEEE80211BE
static void ieee80211_ml_link_sta_assoc_cb(struct hostapd_data *hapd,
					   struct sta_info *sta,
					   struct mld_link_info *link,
					   bool ok)
{
	bool updated = false;

	if (!ok) {
		hostapd_logger(hapd, link->peer_addr, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "did not acknowledge association response");
		sta->flags &= ~WLAN_STA_ASSOC_REQ_OK;

		/* The STA is added only in case of SUCCESS */
		if (link->status == WLAN_STATUS_SUCCESS)
			hostapd_drv_sta_remove(hapd, sta->addr);

		return;
	}

	if (link->status != WLAN_STATUS_SUCCESS)
		return;

	sta->flags |= WLAN_STA_ASSOC;
	sta->flags &= ~WLAN_STA_WNM_SLEEP_MODE;

	if (!hapd->conf->ieee802_1x && !hapd->conf->wpa)
		updated = ap_sta_set_authorized_flag(hapd, sta, 1);

	hostapd_set_sta_flags(hapd, sta);
	if (updated)
		ap_sta_set_authorized_event(hapd, sta, 1);

	/*
	 * TODOs:
	 * - IEEE 802.1X port enablement is not needed as done on the station
	 *     doing the connection.
	 * - Not handling accounting
	 * - Need to handle VLAN configuration
	 */
}
#endif /* CONFIG_IEEE80211BE */


static void hostapd_ml_handle_assoc_cb(struct hostapd_data *hapd,
				       struct sta_info *sta, bool ok)
{
#ifdef CONFIG_IEEE80211BE
	struct hostapd_data *tmp_hapd;

	if (!hostapd_is_mld_ap(hapd))
		return;

	for_each_mld_link(tmp_hapd, hapd) {
		struct mld_link_info *link;
		struct sta_info *tmp_sta;

		if (tmp_hapd == hapd)
			continue;

		link = &sta->mld_info.links[tmp_hapd->mld_link_id];
		if (!link->valid)
			continue;

		for (tmp_sta = tmp_hapd->sta_list; tmp_sta;
		     tmp_sta = tmp_sta->next) {
			if (tmp_sta == sta ||
			    tmp_sta->mld_assoc_link_id !=
			    sta->mld_assoc_link_id ||
			    tmp_sta->aid != sta->aid)
				continue;

			ieee80211_ml_link_sta_assoc_cb(tmp_hapd, tmp_sta, link,
						       ok);
			break;
		}
	}
#endif /* CONFIG_IEEE80211BE */
}


static void handle_assoc_cb(struct hostapd_data *hapd,
			    const struct ieee80211_mgmt *mgmt,
			    size_t len, int reassoc, int ok)
{
	u16 status;
	struct sta_info *sta;
	int new_assoc = 1;

	sta = ap_get_sta(hapd, mgmt->da);
	if (!sta) {
		wpa_printf(MSG_INFO, "handle_assoc_cb: STA " MACSTR " not found",
			   MAC2STR(mgmt->da));
		return;
	}

#ifdef CONFIG_IEEE80211BE
	if (ap_sta_is_mld(hapd, sta) &&
	    hapd->mld_link_id != sta->mld_assoc_link_id) {
		/* See ieee80211_ml_link_sta_assoc_cb() for the MLD case */
		wpa_printf(MSG_DEBUG,
			   "%s: MLD: ignore on link station (%d != %d)",
			   __func__, hapd->mld_link_id, sta->mld_assoc_link_id);
		return;
	}
#endif /* CONFIG_IEEE80211BE */

	if (len < IEEE80211_HDRLEN + (reassoc ? sizeof(mgmt->u.reassoc_resp) :
				      sizeof(mgmt->u.assoc_resp))) {
		wpa_printf(MSG_INFO,
			   "handle_assoc_cb(reassoc=%d) - too short payload (len=%lu)",
			   reassoc, (unsigned long) len);
		hostapd_drv_sta_remove(hapd, sta->addr);
		return;
	}

	if (reassoc)
		status = le_to_host16(mgmt->u.reassoc_resp.status_code);
	else
		status = le_to_host16(mgmt->u.assoc_resp.status_code);

	if (!ok) {
		hostapd_logger(hapd, mgmt->da, HOSTAPD_MODULE_IEEE80211,
			       HOSTAPD_LEVEL_DEBUG,
			       "did not acknowledge association response");
		sta->flags &= ~WLAN_STA_ASSOC_REQ_OK;
		/* The STA is added only in case of SUCCESS */
		if (status == WLAN_STATUS_SUCCESS)
			hostapd_drv_sta_remove(hapd, sta->addr);

		goto handle_ml;
	}

	if (status != WLAN_STATUS_SUCCESS)
		goto handle_ml;

	/* Stop previous accounting session, if one is started, and allocate
	 * new session id for the new session. */
	accounting_sta_stop(hapd, sta);

	hostapd_logger(hapd, sta->addr, HOSTAPD_MODULE_IEEE80211,
		       HOSTAPD_LEVEL_INFO,
		       "associated (aid %d)",
		       sta->aid);

	if (sta->flags & WLAN_STA_ASSOC)
		new_assoc = 0;
	sta->flags |= WLAN_STA_ASSOC;
	sta->flags &= ~WLAN_STA_WNM_SLEEP_MODE;
	if ((!hapd->conf->ieee802_1x && !hapd->conf->wpa &&
	     !hapd->conf->osen) ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK ||
	    sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	    sta->auth_alg == WLAN_AUTH_FILS_PK ||
	    sta->auth_alg == WLAN_AUTH_FT) {
		/*
		 * Open, static WEP, FT protocol, or FILS; no separate
		 * authorization step.
		 */
		ap_sta_set_authorized(hapd, sta, 1);
	}

	if (reassoc)
		mlme_reassociate_indication(hapd, sta);
	else
		mlme_associate_indication(hapd, sta);

	sta->sa_query_timed_out = 0;

	if (sta->eapol_sm == NULL) {
		/*
		 * This STA does not use RADIUS server for EAP authentication,
		 * so bind it to the selected VLAN interface now, since the
		 * interface selection is not going to change anymore.
		 */
		if (ap_sta_bind_vlan(hapd, sta) < 0)
			goto handle_ml;
	} else if (sta->vlan_id) {
		/* VLAN ID already set (e.g., by PMKSA caching), so bind STA */
		if (ap_sta_bind_vlan(hapd, sta) < 0)
			goto handle_ml;
	}

	hostapd_set_sta_flags(hapd, sta);

	if (!(sta->flags & WLAN_STA_WDS) && sta->pending_wds_enable) {
		wpa_printf(MSG_DEBUG, "Enable 4-address WDS mode for STA "
			   MACSTR " based on pending request",
			   MAC2STR(sta->addr));
		sta->pending_wds_enable = 0;
		sta->flags |= WLAN_STA_WDS;
	}

	/* WPS not supported on backhaul BSS. Disable 4addr mode on fronthaul */
	if ((sta->flags & WLAN_STA_WDS) ||
	    (sta->flags & WLAN_STA_MULTI_AP &&
	     (hapd->conf->multi_ap & BACKHAUL_BSS) &&
	     hapd->conf->wds_sta &&
	     !(sta->flags & WLAN_STA_WPS))) {
		int ret;
		char ifname_wds[IFNAMSIZ + 1];

		wpa_printf(MSG_DEBUG, "Reenable 4-address WDS mode for STA "
			   MACSTR " (aid %u)",
			   MAC2STR(sta->addr), sta->aid);
		ret = hostapd_set_wds_sta(hapd, ifname_wds, sta->addr,
					  sta->aid, 1);
		if (!ret)
			hostapd_set_wds_encryption(hapd, sta, ifname_wds);
	}

	if (sta->auth_alg == WLAN_AUTH_FT)
		wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC_FT);
	else
		wpa_auth_sm_event(sta->wpa_sm, WPA_ASSOC);
	hapd->new_assoc_sta_cb(hapd, sta, !new_assoc);
	ieee802_1x_notify_port_enabled(sta->eapol_sm, 1);

#ifdef CONFIG_FILS
	if ((sta->auth_alg == WLAN_AUTH_FILS_SK ||
	     sta->auth_alg == WLAN_AUTH_FILS_SK_PFS ||
	     sta->auth_alg == WLAN_AUTH_FILS_PK) &&
	    fils_set_tk(sta->wpa_sm) < 0) {
		wpa_printf(MSG_DEBUG, "FILS: TK configuration failed");
		ap_sta_disconnect(hapd, sta, sta->addr,
				  WLAN_REASON_UNSPECIFIED);
		return;
	}
#endif /* CONFIG_FILS */

	if (sta->pending_eapol_rx) {
		struct os_reltime now, age;

		os_get_reltime(&now);
		os_reltime_sub(&now, &sta->pending_eapol_rx->rx_time, &age);
		if (age.sec == 0 && age.usec < 200000) {
			wpa_printf(MSG_DEBUG,
				   "Process pending EAPOL frame that was received from " MACSTR " just before association notification",
				   MAC2STR(sta->addr));
			ieee802_1x_receive(
				hapd, mgmt->da,
				wpabuf_head(sta->pending_eapol_rx->buf),
				wpabuf_len(sta->pending_eapol_rx->buf),
				sta->pending_eapol_rx->encrypted);
		}
		wpabuf_free(sta->pending_eapol_rx->buf);
		os_free(sta->pending_eapol_rx);
		sta->pending_eapol_rx = NULL;
	}

handle_ml:
	hostapd_ml_handle_assoc_cb(hapd, sta, ok);
}


static void handle_deauth_cb(struct hostapd_data *hapd,
			     const struct ieee80211_mgmt *mgmt,
			     size_t len, int ok)
{
	struct sta_info *sta;
	if (is_multicast_ether_addr(mgmt->da))
		return;
	sta = ap_get_sta(hapd, mgmt->da);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "handle_deauth_cb: STA " MACSTR
			   " not found", MAC2STR(mgmt->da));
		return;
	}
	if (ok)
		wpa_printf(MSG_DEBUG, "STA " MACSTR " acknowledged deauth",
			   MAC2STR(sta->addr));
	else
		wpa_printf(MSG_DEBUG, "STA " MACSTR " did not acknowledge "
			   "deauth", MAC2STR(sta->addr));

	ap_sta_deauth_cb(hapd, sta);
}


static void handle_disassoc_cb(struct hostapd_data *hapd,
			       const struct ieee80211_mgmt *mgmt,
			       size_t len, int ok)
{
	struct sta_info *sta;
	if (is_multicast_ether_addr(mgmt->da))
		return;
	sta = ap_get_sta(hapd, mgmt->da);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "handle_disassoc_cb: STA " MACSTR
			   " not found", MAC2STR(mgmt->da));
		return;
	}
	if (ok)
		wpa_printf(MSG_DEBUG, "STA " MACSTR " acknowledged disassoc",
			   MAC2STR(sta->addr));
	else
		wpa_printf(MSG_DEBUG, "STA " MACSTR " did not acknowledge "
			   "disassoc", MAC2STR(sta->addr));

	ap_sta_disassoc_cb(hapd, sta);
}


static void handle_action_cb(struct hostapd_data *hapd,
			     const struct ieee80211_mgmt *mgmt,
			     size_t len, int ok)
{
	struct sta_info *sta;
#ifndef CONFIG_NO_RRM
	const struct rrm_measurement_report_element *report;
#endif /* CONFIG_NO_RRM */

#ifdef CONFIG_DPP
	if (len >= IEEE80211_HDRLEN + 6 &&
	    mgmt->u.action.category == WLAN_ACTION_PUBLIC &&
	    mgmt->u.action.u.vs_public_action.action ==
	    WLAN_PA_VENDOR_SPECIFIC &&
	    WPA_GET_BE24(mgmt->u.action.u.vs_public_action.oui) ==
	    OUI_WFA &&
	    mgmt->u.action.u.vs_public_action.variable[0] ==
	    DPP_OUI_TYPE) {
		const u8 *pos, *end;

		pos = &mgmt->u.action.u.vs_public_action.variable[1];
		end = ((const u8 *) mgmt) + len;
		hostapd_dpp_tx_status(hapd, mgmt->da, pos, end - pos, ok);
		return;
	}
	if (len >= IEEE80211_HDRLEN + 2 &&
	    mgmt->u.action.category == WLAN_ACTION_PUBLIC &&
	    (mgmt->u.action.u.public_action.action ==
	     WLAN_PA_GAS_INITIAL_REQ ||
	     mgmt->u.action.u.public_action.action ==
	     WLAN_PA_GAS_COMEBACK_REQ)) {
		const u8 *pos, *end;

		pos = mgmt->u.action.u.public_action.variable;
		end = ((const u8 *) mgmt) + len;
		gas_query_ap_tx_status(hapd->gas, mgmt->da, pos, end - pos, ok);
		return;
	}
#endif /* CONFIG_DPP */
	if (is_multicast_ether_addr(mgmt->da))
		return;
	sta = ap_get_sta(hapd, mgmt->da);
	if (!sta) {
		wpa_printf(MSG_DEBUG, "handle_action_cb: STA " MACSTR
			   " not found", MAC2STR(mgmt->da));
		return;
	}

#ifdef CONFIG_HS20
	if (ok && len >= IEEE80211_HDRLEN + 2 &&
	    mgmt->u.action.category == WLAN_ACTION_WNM &&
	    mgmt->u.action.u.vs_public_action.action == WNM_NOTIFICATION_REQ &&
	    sta->hs20_deauth_on_ack) {
		wpa_printf(MSG_DEBUG, "HS 2.0: Deauthenticate STA " MACSTR
			   " on acknowledging the WNM-Notification",
			   MAC2STR(sta->addr));
		ap_sta_session_timeout(hapd, sta, 0);
		return;
	}
#endif /* CONFIG_HS20 */

#ifndef CONFIG_NO_RRM
	if (len < 24 + 5 + sizeof(*report))
		return;
	report = (const struct rrm_measurement_report_element *)
		&mgmt->u.action.u.rrm.variable[2];
	if (mgmt->u.action.category == WLAN_ACTION_RADIO_MEASUREMENT &&
	    mgmt->u.action.u.rrm.action == WLAN_RRM_RADIO_MEASUREMENT_REQUEST &&
	    report->eid == WLAN_EID_MEASURE_REQUEST &&
	    report->len >= 3 &&
	    report->type == MEASURE_TYPE_BEACON)
		hostapd_rrm_beacon_req_tx_status(hapd, mgmt, len, ok);
#endif /* CONFIG_NO_RRM */
}


/**
 * ieee802_11_mgmt_cb - Process management frame TX status callback
 * @hapd: hostapd BSS data structure (the BSS from which the management frame
 * was sent from)
 * @buf: management frame data (starting from IEEE 802.11 header)
 * @len: length of frame data in octets
 * @stype: management frame subtype from frame control field
 * @ok: Whether the frame was ACK'ed
 */
void ieee802_11_mgmt_cb(struct hostapd_data *hapd, const u8 *buf, size_t len,
			u16 stype, int ok)
{
	const struct ieee80211_mgmt *mgmt;
	mgmt = (const struct ieee80211_mgmt *) buf;

#ifdef CONFIG_TESTING_OPTIONS
	if (hapd->ext_mgmt_frame_handling) {
		size_t hex_len = 2 * len + 1;
		char *hex = os_malloc(hex_len);

		if (hex) {
			wpa_snprintf_hex(hex, hex_len, buf, len);
			wpa_msg(hapd->msg_ctx, MSG_INFO,
				"MGMT-TX-STATUS stype=%u ok=%d buf=%s",
				stype, ok, hex);
			os_free(hex);
		}
		return;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	switch (stype) {
	case WLAN_FC_STYPE_AUTH:
		wpa_printf(MSG_DEBUG, "mgmt::auth cb");
		handle_auth_cb(hapd, mgmt, len, ok);
		break;
	case WLAN_FC_STYPE_ASSOC_RESP:
		wpa_printf(MSG_DEBUG, "mgmt::assoc_resp cb");
		handle_assoc_cb(hapd, mgmt, len, 0, ok);
		break;
	case WLAN_FC_STYPE_REASSOC_RESP:
		wpa_printf(MSG_DEBUG, "mgmt::reassoc_resp cb");
		handle_assoc_cb(hapd, mgmt, len, 1, ok);
		break;
	case WLAN_FC_STYPE_PROBE_RESP:
		wpa_printf(MSG_EXCESSIVE, "mgmt::proberesp cb ok=%d", ok);
		break;
	case WLAN_FC_STYPE_DEAUTH:
		wpa_printf(MSG_DEBUG, "mgmt::deauth cb");
		handle_deauth_cb(hapd, mgmt, len, ok);
		break;
	case WLAN_FC_STYPE_DISASSOC:
		wpa_printf(MSG_DEBUG, "mgmt::disassoc cb");
		handle_disassoc_cb(hapd, mgmt, len, ok);
		break;
	case WLAN_FC_STYPE_ACTION:
		wpa_printf(MSG_DEBUG, "mgmt::action cb ok=%d", ok);
		handle_action_cb(hapd, mgmt, len, ok);
		break;
	default:
		wpa_printf(MSG_INFO, "unknown mgmt cb frame subtype %d", stype);
		break;
	}
}


int ieee802_11_get_mib(struct hostapd_data *hapd, char *buf, size_t buflen)
{
	/* TODO */
	return 0;
}


int ieee802_11_get_mib_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   char *buf, size_t buflen)
{
	/* TODO */
	return 0;
}


void hostapd_tx_status(struct hostapd_data *hapd, const u8 *addr,
		       const u8 *buf, size_t len, int ack)
{
	struct sta_info *sta;
	struct hostapd_iface *iface = hapd->iface;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL && iface->num_bss > 1) {
		size_t j;
		for (j = 0; j < iface->num_bss; j++) {
			hapd = iface->bss[j];
			sta = ap_get_sta(hapd, addr);
			if (sta)
				break;
		}
	}
	if (sta == NULL || !(sta->flags & WLAN_STA_ASSOC))
		return;
	if (sta->flags & WLAN_STA_PENDING_POLL) {
		wpa_printf(MSG_DEBUG, "STA " MACSTR " %s pending "
			   "activity poll", MAC2STR(sta->addr),
			   ack ? "ACKed" : "did not ACK");
		if (ack)
			sta->flags &= ~WLAN_STA_PENDING_POLL;
	}

	ieee802_1x_tx_status(hapd, sta, buf, len, ack);
}


void hostapd_client_poll_ok(struct hostapd_data *hapd, const u8 *addr)
{
	struct sta_info *sta;
	struct hostapd_iface *iface = hapd->iface;

	sta = ap_get_sta(hapd, addr);
	if (sta == NULL && iface->num_bss > 1) {
		size_t j;
		for (j = 0; j < iface->num_bss; j++) {
			hapd = iface->bss[j];
			sta = ap_get_sta(hapd, addr);
			if (sta)
				break;
		}
	}
	if (sta == NULL)
		return;
	wpa_msg(hapd->msg_ctx, MSG_INFO, AP_STA_POLL_OK MACSTR,
		MAC2STR(sta->addr));
	if (!(sta->flags & WLAN_STA_PENDING_POLL))
		return;

	wpa_printf(MSG_DEBUG, "STA " MACSTR " ACKed pending "
		   "activity poll", MAC2STR(sta->addr));
	sta->flags &= ~WLAN_STA_PENDING_POLL;
}


void ieee802_11_rx_from_unknown(struct hostapd_data *hapd, const u8 *src,
				int wds)
{
	struct sta_info *sta;

	sta = ap_get_sta(hapd, src);
	if (sta &&
	    ((sta->flags & WLAN_STA_ASSOC) ||
	     ((sta->flags & WLAN_STA_ASSOC_REQ_OK) && wds))) {
		if (!hapd->conf->wds_sta)
			return;

		if ((sta->flags & (WLAN_STA_ASSOC | WLAN_STA_ASSOC_REQ_OK)) ==
		    WLAN_STA_ASSOC_REQ_OK) {
			wpa_printf(MSG_DEBUG,
				   "Postpone 4-address WDS mode enabling for STA "
				   MACSTR " since TX status for AssocResp is not yet known",
				   MAC2STR(sta->addr));
			sta->pending_wds_enable = 1;
			return;
		}

		if (wds && !(sta->flags & WLAN_STA_WDS)) {
			int ret;
			char ifname_wds[IFNAMSIZ + 1];

			wpa_printf(MSG_DEBUG, "Enable 4-address WDS mode for "
				   "STA " MACSTR " (aid %u)",
				   MAC2STR(sta->addr), sta->aid);
			sta->flags |= WLAN_STA_WDS;
			ret = hostapd_set_wds_sta(hapd, ifname_wds,
						  sta->addr, sta->aid, 1);
			if (!ret)
				hostapd_set_wds_encryption(hapd, sta,
							   ifname_wds);
		}
		return;
	}

	wpa_printf(MSG_DEBUG, "Data/PS-poll frame from not associated STA "
		   MACSTR, MAC2STR(src));
	if (is_multicast_ether_addr(src) || is_zero_ether_addr(src) ||
	    ether_addr_equal(src, hapd->own_addr)) {
		/* Broadcast bit set in SA or unexpected SA?! Ignore the frame
		 * silently. */
		return;
	}

	if (sta && (sta->flags & WLAN_STA_ASSOC_REQ_OK)) {
		wpa_printf(MSG_DEBUG, "Association Response to the STA has "
			   "already been sent, but no TX status yet known - "
			   "ignore Class 3 frame issue with " MACSTR,
			   MAC2STR(src));
		return;
	}

	if (sta && (sta->flags & WLAN_STA_AUTH))
		hostapd_drv_sta_disassoc(
			hapd, src,
			WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
	else
		hostapd_drv_sta_deauth(
			hapd, src,
			WLAN_REASON_CLASS3_FRAME_FROM_NONASSOC_STA);
}


static u8 * hostapd_add_tpe_info(u8 *eid, u8 tx_pwr_count,
				 enum max_tx_pwr_interpretation tx_pwr_intrpn,
				 u8 tx_pwr_cat, u8 tx_pwr)
{
	int i;

	*eid++ = WLAN_EID_TRANSMIT_POWER_ENVELOPE; /* Element ID */
	*eid++ = 2 + tx_pwr_count; /* Length */

	/*
	 * Transmit Power Information field
	 *	bits 0-2 : Maximum Transmit Power Count
	 *	bits 3-5 : Maximum Transmit Power Interpretation
	 *	bits 6-7 : Maximum Transmit Power Category
	 */
	*eid++ = tx_pwr_count | (tx_pwr_intrpn << 3) | (tx_pwr_cat << 6);

	/* Maximum Transmit Power field */
	for (i = 0; i <= tx_pwr_count; i++)
		*eid++ = tx_pwr;

	return eid;
}


/*
 * TODO: Extract power limits from channel data after 6G regulatory
 *	support.
 */
#define REG_PSD_MAX_TXPOWER_FOR_DEFAULT_CLIENT      (-1) /* dBm/MHz */
#define REG_PSD_MAX_TXPOWER_FOR_SUBORDINATE_CLIENT  5    /* dBm/MHz */

u8 * hostapd_eid_txpower_envelope(struct hostapd_data *hapd, u8 *eid)
{
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_config *iconf = iface->conf;
	struct hostapd_hw_modes *mode = iface->current_mode;
	struct hostapd_channel_data *chan;
	int dfs, i;
	u8 channel, tx_pwr_count, local_pwr_constraint;
	int max_tx_power;
	u8 tx_pwr;

	if (!mode)
		return eid;

	if (ieee80211_freq_to_chan(iface->freq, &channel) == NUM_HOSTAPD_MODES)
		return eid;

	for (i = 0; i < mode->num_channels; i++) {
		if (mode->channels[i].freq == iface->freq)
			break;
	}
	if (i == mode->num_channels)
		return eid;

#ifdef CONFIG_IEEE80211AX
	/* IEEE Std 802.11ax-2021, Annex E.2.7 (6 GHz band in the United
	 * States): An AP that is an Indoor Access Point per regulatory rules
	 * shall send at least two Transmit Power Envelope elements in Beacon
	 * and Probe Response frames as follows:
	 *  - Maximum Transmit Power Category subfield = Default;
	 *	Unit interpretation = Regulatory client EIRP PSD
	 *  - Maximum Transmit Power Category subfield = Subordinate Device;
	 *	Unit interpretation = Regulatory client EIRP PSD
	 */
	if (is_6ghz_op_class(iconf->op_class)) {
		enum max_tx_pwr_interpretation tx_pwr_intrpn;

		/* Same Maximum Transmit Power for all 20 MHz bands */
		tx_pwr_count = 0;
		tx_pwr_intrpn = REGULATORY_CLIENT_EIRP_PSD;

		/* Default Transmit Power Envelope for Global Operating Class */
		if (hapd->iconf->reg_def_cli_eirp_psd != -1)
			tx_pwr = hapd->iconf->reg_def_cli_eirp_psd;
		else
			tx_pwr = REG_PSD_MAX_TXPOWER_FOR_DEFAULT_CLIENT * 2;

		eid = hostapd_add_tpe_info(eid, tx_pwr_count, tx_pwr_intrpn,
					   REG_DEFAULT_CLIENT, tx_pwr);

		/* Indoor Access Point must include an additional TPE for
		 * subordinate devices */
		if (he_reg_is_indoor(iconf->he_6ghz_reg_pwr_type)) {
			/* TODO: Extract PSD limits from channel data */
			if (hapd->iconf->reg_sub_cli_eirp_psd != -1)
				tx_pwr = hapd->iconf->reg_sub_cli_eirp_psd;
			else
				tx_pwr = REG_PSD_MAX_TXPOWER_FOR_SUBORDINATE_CLIENT * 2;
			eid = hostapd_add_tpe_info(eid, tx_pwr_count,
						   tx_pwr_intrpn,
						   REG_SUBORDINATE_CLIENT,
						   tx_pwr);
		}

		if (iconf->reg_def_cli_eirp != -1 &&
		    he_reg_is_sp(iconf->he_6ghz_reg_pwr_type))
			eid = hostapd_add_tpe_info(
				eid, tx_pwr_count, REGULATORY_CLIENT_EIRP,
				REG_DEFAULT_CLIENT,
				hapd->iconf->reg_def_cli_eirp);

		return eid;
	}
#endif /* CONFIG_IEEE80211AX */

	switch (hostapd_get_oper_chwidth(iconf)) {
	case CONF_OPER_CHWIDTH_USE_HT:
		if (iconf->secondary_channel == 0) {
			/* Max Transmit Power count = 0 (20 MHz) */
			tx_pwr_count = 0;
		} else {
			/* Max Transmit Power count = 1 (20, 40 MHz) */
			tx_pwr_count = 1;
		}
		break;
	case CONF_OPER_CHWIDTH_80MHZ:
		/* Max Transmit Power count = 2 (20, 40, and 80 MHz) */
		tx_pwr_count = 2;
		break;
	case CONF_OPER_CHWIDTH_80P80MHZ:
	case CONF_OPER_CHWIDTH_160MHZ:
		/* Max Transmit Power count = 3 (20, 40, 80, 160/80+80 MHz) */
		tx_pwr_count = 3;
		break;
	default:
		return eid;
	}

	/*
	 * Below local_pwr_constraint logic is referred from
	 * hostapd_eid_pwr_constraint.
	 *
	 * Check if DFS is required by regulatory.
	 */
	dfs = hostapd_is_dfs_required(hapd->iface);
	if (dfs < 0)
		dfs = 0;

	/*
	 * In order to meet regulations when TPC is not implemented using
	 * a transmit power that is below the legal maximum (including any
	 * mitigation factor) should help. In this case, indicate 3 dB below
	 * maximum allowed transmit power.
	 */
	if (hapd->iconf->local_pwr_constraint == -1)
		local_pwr_constraint = (dfs == 0) ? 0 : 3;
	else
		local_pwr_constraint = hapd->iconf->local_pwr_constraint;

	/*
	 * A STA that is not an AP shall use a transmit power less than or
	 * equal to the local maximum transmit power level for the channel.
	 * The local maximum transmit power can be calculated from the formula:
	 * local max TX pwr = max TX pwr - local pwr constraint
	 * Where max TX pwr is maximum transmit power level specified for
	 * channel in Country element and local pwr constraint is specified
	 * for channel in this Power Constraint element.
	 */
	chan = &mode->channels[i];
	max_tx_power = chan->max_tx_power - local_pwr_constraint;

	/*
	 * Local Maximum Transmit power is encoded as two's complement
	 * with a 0.5 dB step.
	 */
	max_tx_power *= 2; /* in 0.5 dB steps */
	if (max_tx_power > 127) {
		/* 63.5 has special meaning of 63.5 dBm or higher */
		max_tx_power = 127;
	}
	if (max_tx_power < -128)
		max_tx_power = -128;
	if (max_tx_power < 0)
		tx_pwr = 0x80 + max_tx_power + 128;
	else
		tx_pwr = max_tx_power;

	return hostapd_add_tpe_info(eid, tx_pwr_count, LOCAL_EIRP,
				    0 /* Reserved for bands other than 6 GHz */,
				    tx_pwr);
}


u8 * hostapd_eid_wb_chsw_wrapper(struct hostapd_data *hapd, u8 *eid)
{
	u8 bw, chan1 = 0, chan2 = 0;
	int freq1;

	if (!hapd->cs_freq_params.channel ||
	    (!hapd->cs_freq_params.vht_enabled &&
	     !hapd->cs_freq_params.he_enabled &&
	     !hapd->cs_freq_params.eht_enabled))
		return eid;

	/* bandwidth: 0: 40, 1: 80, 160, 80+80, 4: 320 as per
	 * IEEE P802.11-REVme/D4.0, 9.4.2.159 and Table 9-314. */
	switch (hapd->cs_freq_params.bandwidth) {
	case 40:
		bw = 0;
		break;
	case 80:
		bw = 1;
		break;
	case 160:
		bw = 1;
		break;
	case 320:
		bw = 4;
		break;
	default:
		/* not valid VHT bandwidth or not in CSA */
		return eid;
	}

	freq1 = hapd->cs_freq_params.center_freq1 ?
		hapd->cs_freq_params.center_freq1 :
		hapd->cs_freq_params.freq;
	if (ieee80211_freq_to_chan(freq1, &chan1) !=
	    HOSTAPD_MODE_IEEE80211A)
		return eid;

	if (hapd->cs_freq_params.center_freq2 &&
	    ieee80211_freq_to_chan(hapd->cs_freq_params.center_freq2,
				   &chan2) != HOSTAPD_MODE_IEEE80211A)
		return eid;

	*eid++ = WLAN_EID_CHANNEL_SWITCH_WRAPPER;
	*eid++ = 5; /* Length of Channel Switch Wrapper */
	*eid++ = WLAN_EID_WIDE_BW_CHSWITCH;
	*eid++ = 3; /* Length of Wide Bandwidth Channel Switch element */
	*eid++ = bw; /* New Channel Width */
	if (hapd->cs_freq_params.bandwidth == 160) {
		/* Update the CCFS0 and CCFS1 values in the element based on
		 * IEEE P802.11-REVme/D4.0, Table 9-314 */

		/* CCFS1 - The channel center frequency index of the 160 MHz
		 * channel. */
		chan2 = chan1;

		/* CCFS0 - The channel center frequency index of the 80 MHz
		 * channel segment that contains the primary channel. */
		if (hapd->cs_freq_params.channel < chan1)
			chan1 -= 8;
		else
			chan1 += 8;
	}
	*eid++ = chan1; /* New Channel Center Frequency Segment 0 */
	*eid++ = chan2; /* New Channel Center Frequency Segment 1 */

	return eid;
}


static size_t hostapd_eid_nr_db_len(struct hostapd_data *hapd,
				    size_t *current_len)
{
	struct hostapd_neighbor_entry *nr;
	size_t total_len = 0, len = *current_len;

	dl_list_for_each(nr, &hapd->nr_db, struct hostapd_neighbor_entry,
			 list) {
		if (!nr->nr || wpabuf_len(nr->nr) < 12)
			continue;

		if (nr->short_ssid == hapd->conf->ssid.short_ssid)
			continue;

		/* Start a new element */
		if (!len ||
		    len + RNR_TBTT_HEADER_LEN + RNR_TBTT_INFO_LEN > 255) {
			len = RNR_HEADER_LEN;
			total_len += RNR_HEADER_LEN;
		}

		len += RNR_TBTT_HEADER_LEN + RNR_TBTT_INFO_LEN;
		total_len += RNR_TBTT_HEADER_LEN + RNR_TBTT_INFO_LEN;
	}

	*current_len = len;
	return total_len;
}


struct mbssid_ie_profiles {
	u8 start;
	u8 end;
};

static bool hostapd_skip_rnr(size_t i, struct mbssid_ie_profiles *skip_profiles,
			     bool ap_mld, u8 tbtt_info_len, bool mld_update,
			     struct hostapd_data *reporting_hapd,
			     struct hostapd_data *bss)
{
	if (skip_profiles &&
	    i >= skip_profiles->start && i < skip_profiles->end)
		return true;

	/* No need to report if length is for normal TBTT and the BSS is
	 * affiliated with an AP MLD. MLD TBTT will include this. */
	if (tbtt_info_len == RNR_TBTT_INFO_LEN && ap_mld)
		return true;

	/* No need to report if length is for MLD TBTT and the BSS is not
	 * affiliated with an aP MLD. Normal TBTT will include this. */
	if (tbtt_info_len == RNR_TBTT_INFO_MLD_LEN && !ap_mld)
		return true;

#ifdef CONFIG_IEEE80211BE
	/* If building for co-location and they are ML partners, no need to
	 * include since the ML RNR will carry this. */
	if (!mld_update && hostapd_is_ml_partner(reporting_hapd, bss))
		return true;

	/* If building for ML RNR and they are not ML partners, don't include.
	 */
	if (mld_update && !hostapd_is_ml_partner(reporting_hapd, bss))
		return true;
#endif /* CONFIG_IEEE80211BE */

	return false;
}


static size_t
hostapd_eid_rnr_iface_len(struct hostapd_data *hapd,
			  struct hostapd_data *reporting_hapd,
			  size_t *current_len,
			  struct mbssid_ie_profiles *skip_profiles,
			  bool mld_update)
{
	size_t total_len = 0, len = *current_len;
	int tbtt_count, total_tbtt_count = 0;
	size_t i, start;
	u8 tbtt_info_len = mld_update ? RNR_TBTT_INFO_MLD_LEN :
		RNR_TBTT_INFO_LEN;

repeat_rnr_len:
	start = 0;
	tbtt_count = 0;

	while (start < hapd->iface->num_bss) {
		if (!len ||
		    len + RNR_TBTT_HEADER_LEN + tbtt_info_len > 255 ||
		    tbtt_count >= RNR_TBTT_INFO_COUNT_MAX) {
			len = RNR_HEADER_LEN;
			total_len += RNR_HEADER_LEN;
			tbtt_count = 0;
		}

		len += RNR_TBTT_HEADER_LEN;
		total_len += RNR_TBTT_HEADER_LEN;

		for (i = start; i < hapd->iface->num_bss; i++) {
			struct hostapd_data *bss = hapd->iface->bss[i];
			bool ap_mld = false;

			if (!bss || !bss->conf || !bss->started)
				continue;

#ifdef CONFIG_IEEE80211BE
			ap_mld = bss->conf->mld_ap;
#endif /* CONFIG_IEEE80211BE */

			if (bss == reporting_hapd ||
			    bss->conf->ignore_broadcast_ssid)
				continue;

			if (hostapd_skip_rnr(i, skip_profiles, ap_mld,
					     tbtt_info_len, mld_update,
					     reporting_hapd, bss))
				continue;

			if (len + tbtt_info_len > 255 ||
			    tbtt_count >= RNR_TBTT_INFO_COUNT_MAX)
				break;

			len += tbtt_info_len;
			total_len += tbtt_info_len;
			tbtt_count++;
		}
		start = i;
	}

	total_tbtt_count += tbtt_count;

	/* If building for co-location, re-build again but this time include
	 * ML TBTTs.
	 */
	if (!mld_update && tbtt_info_len == RNR_TBTT_INFO_LEN) {
		tbtt_info_len = RNR_TBTT_INFO_MLD_LEN;

		/* If no TBTT was found, adjust the len and total_len since it
		 * would have incremented before we checked all BSSs. */
		if (!tbtt_count) {
			len -= RNR_TBTT_HEADER_LEN;
			total_len -= RNR_TBTT_HEADER_LEN;
		}

		goto repeat_rnr_len;
	}

	/* This is possible when in the re-built case and no suitable TBTT was
	 * found. Adjust the length accordingly. */
	if (!tbtt_count && total_tbtt_count) {
		len -= RNR_TBTT_HEADER_LEN;
		total_len -= RNR_TBTT_HEADER_LEN;
	}

	if (!total_tbtt_count)
		total_len = 0;
	else
		*current_len = len;

	return total_len;
}


enum colocation_mode {
	NO_COLOCATED_6GHZ,
	STANDALONE_6GHZ,
	COLOCATED_6GHZ,
	COLOCATED_LOWER_BAND,
};

static enum colocation_mode get_colocation_mode(struct hostapd_data *hapd)
{
	u8 i;
	bool is_6ghz = is_6ghz_op_class(hapd->iconf->op_class);

	if (!hapd->iface || !hapd->iface->interfaces)
		return NO_COLOCATED_6GHZ;

	if (is_6ghz && hapd->iface->interfaces->count == 1)
		return STANDALONE_6GHZ;

	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		struct hostapd_iface *iface;
		bool is_colocated_6ghz;

		iface = hapd->iface->interfaces->iface[i];
		if (iface == hapd->iface || !iface || !iface->conf)
			continue;

		is_colocated_6ghz = is_6ghz_op_class(iface->conf->op_class);
		if (!is_6ghz && is_colocated_6ghz)
			return COLOCATED_LOWER_BAND;
		if (is_6ghz && !is_colocated_6ghz)
			return COLOCATED_6GHZ;
	}

	if (is_6ghz)
		return STANDALONE_6GHZ;

	return NO_COLOCATED_6GHZ;
}


static size_t hostapd_eid_rnr_colocation_len(struct hostapd_data *hapd,
					     size_t *current_len)
{
	struct hostapd_iface *iface;
	size_t len = 0;
	size_t i;

	if (!hapd->iface || !hapd->iface->interfaces)
		return 0;

	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		iface = hapd->iface->interfaces->iface[i];

		if (!iface || iface == hapd->iface ||
		    iface->state != HAPD_IFACE_ENABLED ||
		    !is_6ghz_op_class(iface->conf->op_class))
			continue;

		len += hostapd_eid_rnr_iface_len(iface->bss[0], hapd,
						 current_len, NULL, false);
	}

	return len;
}


static size_t hostapd_eid_rnr_mlo_len(struct hostapd_data *hapd, u32 type,
				      size_t *current_len)
{
	size_t len = 0;
#ifdef CONFIG_IEEE80211BE
	struct hostapd_iface *iface;
	size_t i;

	if (!hapd->iface || !hapd->iface->interfaces || !hapd->conf->mld_ap)
		return 0;

	/* TODO: Allow for FILS/Action as well */
	if (type != WLAN_FC_STYPE_BEACON && type != WLAN_FC_STYPE_PROBE_RESP)
		return 0;

	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		iface = hapd->iface->interfaces->iface[i];

		if (!iface || iface == hapd->iface ||
		    hapd->iface->freq == iface->freq)
			continue;

		len += hostapd_eid_rnr_iface_len(iface->bss[0], hapd,
						 current_len, NULL, true);
	}
#endif /* CONFIG_IEEE80211BE */

	return len;
}


size_t hostapd_eid_rnr_len(struct hostapd_data *hapd, u32 type,
			   bool include_mld_params)
{
	size_t total_len = 0, current_len = 0;
	enum colocation_mode mode = get_colocation_mode(hapd);

	switch (type) {
	case WLAN_FC_STYPE_BEACON:
		if (hapd->conf->rnr)
			total_len += hostapd_eid_nr_db_len(hapd, &current_len);
		/* fallthrough */
	case WLAN_FC_STYPE_PROBE_RESP:
		if (mode == COLOCATED_LOWER_BAND)
			total_len +=
				hostapd_eid_rnr_colocation_len(hapd,
							       &current_len);

		if (hapd->conf->rnr && hapd->iface->num_bss > 1 &&
		    !hapd->iconf->mbssid)
			total_len += hostapd_eid_rnr_iface_len(hapd, hapd,
							       &current_len,
							       NULL, false);
		break;
	case WLAN_FC_STYPE_ACTION:
		if (hapd->iface->num_bss > 1 && mode == STANDALONE_6GHZ)
			total_len += hostapd_eid_rnr_iface_len(hapd, hapd,
							       &current_len,
							       NULL, false);
		break;
	}

	/* For EMA Beacons, MLD neighbor repoting is added as part of
	 * MBSSID RNR. */
	if (include_mld_params &&
	    (type != WLAN_FC_STYPE_BEACON ||
	     hapd->iconf->mbssid != ENHANCED_MBSSID_ENABLED))
		total_len += hostapd_eid_rnr_mlo_len(hapd, type, &current_len);

	return total_len;
}


static u8 * hostapd_eid_nr_db(struct hostapd_data *hapd, u8 *eid,
			      size_t *current_len)
{
	struct hostapd_neighbor_entry *nr;
	size_t len = *current_len;
	u8 *size_offset = (eid - len) + 1;

	dl_list_for_each(nr, &hapd->nr_db, struct hostapd_neighbor_entry,
			 list) {
		if (!nr->nr || wpabuf_len(nr->nr) < 12)
			continue;

		if (nr->short_ssid == hapd->conf->ssid.short_ssid)
			continue;

		/* Start a new element */
		if (!len ||
		    len + RNR_TBTT_HEADER_LEN + RNR_TBTT_INFO_LEN > 255) {
			*eid++ = WLAN_EID_REDUCED_NEIGHBOR_REPORT;
			size_offset = eid++;
			len = RNR_HEADER_LEN;
		}

		/* TBTT Information Header subfield (2 octets) */
		*eid++ = 0;
		/* TBTT Information Length */
		*eid++ = RNR_TBTT_INFO_LEN;
		/* Operating Class */
		*eid++ = wpabuf_head_u8(nr->nr)[10];
		/* Channel Number */
		*eid++ = wpabuf_head_u8(nr->nr)[11];
		len += RNR_TBTT_HEADER_LEN;
		/* TBTT Information Set */
		/* TBTT Information field */
		/* Neighbor AP TBTT Offset */
		*eid++ = RNR_NEIGHBOR_AP_OFFSET_UNKNOWN;
		/* BSSID */
		os_memcpy(eid, nr->bssid, ETH_ALEN);
		eid += ETH_ALEN;
		/* Short SSID */
		os_memcpy(eid, &nr->short_ssid, 4);
		eid += 4;
		/* BSS parameters */
		*eid++ = nr->bss_parameters;
		/* 20 MHz PSD */
		*eid++ = RNR_20_MHZ_PSD_MAX_TXPOWER;
		len += RNR_TBTT_INFO_LEN;
		*size_offset = (eid - size_offset) - 1;
	}

	*current_len = len;
	return eid;
}


static bool hostapd_eid_rnr_bss(struct hostapd_data *hapd,
				struct hostapd_data *reporting_hapd,
				struct mbssid_ie_profiles *skip_profiles,
				size_t i, u8 *tbtt_count, size_t *len,
				u8 **pos, u8 **tbtt_count_pos, u8 tbtt_info_len,
				u8 op_class, bool mld_update)
{
	struct hostapd_iface *iface = hapd->iface;
	struct hostapd_data *bss = iface->bss[i];
	u8 bss_param = 0;
	bool ap_mld = false;
	u8 *eid = *pos;

#ifdef CONFIG_IEEE80211BE
	ap_mld = !!hapd->conf->mld_ap;
#endif /* CONFIG_IEEE80211BE */

	if (!bss || !bss->conf || !bss->started ||
	    bss == reporting_hapd || bss->conf->ignore_broadcast_ssid)
		return false;

	if (hostapd_skip_rnr(i, skip_profiles, ap_mld, tbtt_info_len,
			     mld_update, reporting_hapd, bss))
	    return false;

	if (*len + RNR_TBTT_INFO_LEN > 255 ||
	    *tbtt_count >= RNR_TBTT_INFO_COUNT_MAX)
		return true;

	if (!(*tbtt_count)) {
		/* Add neighbor report header info only if there is at least
		 * one TBTT info available. */
		*tbtt_count_pos = eid++;
		*eid++ = tbtt_info_len;
		*eid++ = op_class;
		*eid++ = bss->iconf->channel;
		*len += RNR_TBTT_HEADER_LEN;
	}

	*eid++ = RNR_NEIGHBOR_AP_OFFSET_UNKNOWN;
	os_memcpy(eid, bss->own_addr, ETH_ALEN);
	eid += ETH_ALEN;
	os_memcpy(eid, &bss->conf->ssid.short_ssid, 4);
	eid += 4;
	if (bss->conf->ssid.short_ssid == reporting_hapd->conf->ssid.short_ssid)
		bss_param |= RNR_BSS_PARAM_SAME_SSID;

	if (iface->conf->mbssid != MBSSID_DISABLED && iface->num_bss > 1) {
		bss_param |= RNR_BSS_PARAM_MULTIPLE_BSSID;
		if (bss == hostapd_mbssid_get_tx_bss(hapd))
			bss_param |= RNR_BSS_PARAM_TRANSMITTED_BSSID;
	}

	if (is_6ghz_op_class(hapd->iconf->op_class) &&
	    bss->conf->unsol_bcast_probe_resp_interval)
		bss_param |= RNR_BSS_PARAM_UNSOLIC_PROBE_RESP_ACTIVE;

	bss_param |= RNR_BSS_PARAM_CO_LOCATED;

	*eid++ = bss_param;
	*eid++ = RNR_20_MHZ_PSD_MAX_TXPOWER;

#ifdef CONFIG_IEEE80211BE
	if (ap_mld) {
		u8 param_ch = bss->eht_mld_bss_param_change;
		bool is_partner;

		/* If BSS is not a partner of the reporting_hapd
		 *  a) MLD ID advertised shall be 255.
		 *  b) Link ID advertised shall be 15.
		 *  c) BPCC advertised shall be 255 */
		is_partner = hostapd_is_ml_partner(bss, reporting_hapd);
		/* MLD ID */
		*eid++ = is_partner ? hostapd_get_mld_id(bss) : 0xFF;
		/* Link ID (Bit 3 to Bit 0)
		 * BPCC (Bit 4 to Bit 7) */
		*eid++ = is_partner ?
			bss->mld_link_id | ((param_ch & 0xF) << 4) :
			(MAX_NUM_MLD_LINKS | 0xF0);
		/* BPCC (Bit 3 to Bit 0) */
		*eid = is_partner ? ((param_ch & 0xF0) >> 4) : 0x0F;
#ifdef CONFIG_TESTING_OPTIONS
		if (bss->conf->mld_indicate_disabled)
			*eid |= RNR_TBTT_INFO_MLD_PARAM2_LINK_DISABLED;
#endif /* CONFIG_TESTING_OPTIONS */
		eid++;
	}
#endif /* CONFIG_IEEE80211BE */

	*len += tbtt_info_len;
	(*tbtt_count)++;
	*pos = eid;

	return false;
}


static u8 * hostapd_eid_rnr_iface(struct hostapd_data *hapd,
				  struct hostapd_data *reporting_hapd,
				  u8 *eid, size_t *current_len,
				  struct mbssid_ie_profiles *skip_profiles,
				  bool mld_update)
{
	struct hostapd_iface *iface = hapd->iface;
	size_t i, start;
	size_t len = *current_len;
	u8 *eid_start = eid, *size_offset = (eid - len) + 1;
	u8 *tbtt_count_pos = size_offset + 1;
	u8 tbtt_count, total_tbtt_count = 0, op_class, channel;
	u8 tbtt_info_len = mld_update ? RNR_TBTT_INFO_MLD_LEN :
		RNR_TBTT_INFO_LEN;

	if (!(iface->drv_flags & WPA_DRIVER_FLAGS_AP_CSA) || !iface->freq)
		return eid;

	if (ieee80211_freq_to_channel_ext(iface->freq,
					  hapd->iconf->secondary_channel,
					  hostapd_get_oper_chwidth(hapd->iconf),
					  &op_class, &channel) ==
	    NUM_HOSTAPD_MODES)
		return eid;

repeat_rnr:
	start = 0;
	tbtt_count = 0;
	while (start < iface->num_bss) {
		if (!len ||
		    len + RNR_TBTT_HEADER_LEN + tbtt_info_len > 255 ||
		    tbtt_count >= RNR_TBTT_INFO_COUNT_MAX) {
			eid_start = eid;
			*eid++ = WLAN_EID_REDUCED_NEIGHBOR_REPORT;
			size_offset = eid++;
			len = RNR_HEADER_LEN;
			tbtt_count = 0;
		}

		for (i = start; i < iface->num_bss; i++) {
			if (hostapd_eid_rnr_bss(hapd, reporting_hapd,
						skip_profiles, i,
						&tbtt_count, &len, &eid,
						&tbtt_count_pos, tbtt_info_len,
						op_class, mld_update))
				break;
		}

		start = i;

		if (tbtt_count) {
			*tbtt_count_pos = RNR_TBTT_INFO_COUNT(tbtt_count - 1);
			*size_offset = (eid - size_offset) - 1;
		}
	}

	total_tbtt_count += tbtt_count;

	/* If building for co-location, re-build again but this time include
	 * ML TBTTs.
	 */
	if (!mld_update && tbtt_info_len == RNR_TBTT_INFO_LEN) {
		tbtt_info_len = RNR_TBTT_INFO_MLD_LEN;
		goto repeat_rnr;
	}

	if (!total_tbtt_count)
		return eid_start;

	*current_len = len;
	return eid;
}


u8 * hostapd_eid_rnr_colocation(struct hostapd_data *hapd, u8 *eid,
				size_t *current_len)
{
	struct hostapd_iface *iface;
	size_t i;

	if (!hapd->iface || !hapd->iface->interfaces)
		return eid;

	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		iface = hapd->iface->interfaces->iface[i];

		if (!iface || iface == hapd->iface ||
		    iface->state != HAPD_IFACE_ENABLED ||
		    !is_6ghz_op_class(iface->conf->op_class))
			continue;

		eid = hostapd_eid_rnr_iface(iface->bss[0], hapd, eid,
					    current_len, NULL, false);
	}

	return eid;
}


u8 * hostapd_eid_rnr_mlo(struct hostapd_data *hapd, u32 type,
			 u8 *eid, size_t *current_len)
{
#ifdef CONFIG_IEEE80211BE
	struct hostapd_iface *iface;
	size_t i;

	if (!hapd->iface || !hapd->iface->interfaces || !hapd->conf->mld_ap)
		return eid;

	/* TODO: Allow for FILS/Action as well */
	if (type != WLAN_FC_STYPE_BEACON && type != WLAN_FC_STYPE_PROBE_RESP)
		return eid;

	for (i = 0; i < hapd->iface->interfaces->count; i++) {
		iface = hapd->iface->interfaces->iface[i];

		if (!iface || iface == hapd->iface ||
		    hapd->iface->freq == iface->freq)
			continue;

		eid = hostapd_eid_rnr_iface(iface->bss[0], hapd, eid,
					    current_len, NULL, true);
	}
#endif /* CONFIG_IEEE80211BE */

	return eid;
}


u8 * hostapd_eid_rnr(struct hostapd_data *hapd, u8 *eid, u32 type,
		     bool include_mld_params)
{
	u8 *eid_start = eid;
	size_t current_len = 0;
	enum colocation_mode mode = get_colocation_mode(hapd);

	switch (type) {
	case WLAN_FC_STYPE_BEACON:
		if (hapd->conf->rnr)
			eid = hostapd_eid_nr_db(hapd, eid, &current_len);
		/* fallthrough */
	case WLAN_FC_STYPE_PROBE_RESP:
		if (mode == COLOCATED_LOWER_BAND)
			eid = hostapd_eid_rnr_colocation(hapd, eid,
							 &current_len);

		if (hapd->conf->rnr && hapd->iface->num_bss > 1 &&
		    !hapd->iconf->mbssid)
			eid = hostapd_eid_rnr_iface(hapd, hapd, eid,
						    &current_len, NULL, false);
		break;
	case WLAN_FC_STYPE_ACTION:
		if (hapd->iface->num_bss > 1 && mode == STANDALONE_6GHZ)
			eid = hostapd_eid_rnr_iface(hapd, hapd, eid,
						    &current_len, NULL, false);
		break;
	default:
		return eid_start;
	}

	/* For EMA Beacons, MLD neighbor repoting is added as part of
	 * MBSSID RNR. */
	if (include_mld_params &&
	    (type != WLAN_FC_STYPE_BEACON ||
	     hapd->iconf->mbssid != ENHANCED_MBSSID_ENABLED))
		eid = hostapd_eid_rnr_mlo(hapd, type, eid, &current_len);

	if (eid == eid_start + 2)
		return eid_start;

	return eid;
}


static bool mbssid_known_bss(unsigned int i, const u8 *known_bss,
			     size_t known_bss_len)
{
	if (!known_bss || known_bss_len <= i / 8)
		return false;
	known_bss = &known_bss[i / 8];
	return *known_bss & (u8) (BIT(i % 8));
}


static size_t hostapd_mbssid_ext_capa(struct hostapd_data *bss,
				      struct hostapd_data *tx_bss, u8 *buf)
{
	u8 ext_capa_tx[20], *ext_capa_tx_end, ext_capa[20], *ext_capa_end;
	size_t ext_capa_len, ext_capa_tx_len;

	ext_capa_tx_end = hostapd_eid_ext_capab(tx_bss, ext_capa_tx,
						true);
	ext_capa_tx_len = ext_capa_tx_end - ext_capa_tx;
	ext_capa_end = hostapd_eid_ext_capab(bss, ext_capa, true);
	ext_capa_len = ext_capa_end - ext_capa;
	if (ext_capa_tx_len != ext_capa_len ||
	    os_memcmp(ext_capa_tx, ext_capa, ext_capa_len) != 0) {
		os_memcpy(buf, ext_capa, ext_capa_len);
		return ext_capa_len;
	}

	return 0;
}


static size_t hostapd_eid_mbssid_elem_len(struct hostapd_data *hapd,
					  u32 frame_type, size_t *bss_index,
					  const u8 *known_bss,
					  size_t known_bss_len)
{
	struct hostapd_data *tx_bss = hostapd_mbssid_get_tx_bss(hapd);
	size_t len, i;
	u8 ext_capa[20];

	/* Element ID: 1 octet
	 * Length: 1 octet
	 * MaxBSSID Indicator: 1 octet
	 * Optional Subelements: vatiable
	 *
	 * Total fixed length: 3 octets
	 *
	 * 1 octet in len for the MaxBSSID Indicator field.
	 */
	len = 1;

	for (i = *bss_index; i < hapd->iface->num_bss; i++) {
		struct hostapd_data *bss = hapd->iface->bss[i];
		const u8 *auth, *rsn = NULL, *rsnx = NULL;
		size_t nontx_profile_len, auth_len;
		u8 ie_count = 0;

		if (!bss || !bss->conf || !bss->started ||
		    mbssid_known_bss(i, known_bss, known_bss_len))
			continue;

		/*
		 * Sublement ID: 1 octet
		 * Length: 1 octet
		 * Nontransmitted capabilities: 4 octets
		 * SSID element: 2 + variable
		 * Multiple BSSID Index Element: 3 octets (+2 octets in beacons)
		 * Fixed length = 1 + 1 + 4 + 2 + 3 = 11
		 */
		nontx_profile_len = 11 + bss->conf->ssid.ssid_len;

		if (frame_type == WLAN_FC_STYPE_BEACON)
			nontx_profile_len += 2;

		auth = wpa_auth_get_wpa_ie(bss->wpa_auth, &auth_len);
		if (auth) {
			rsn = get_ie(auth, auth_len, WLAN_EID_RSN);
			if (rsn)
				nontx_profile_len += 2 + rsn[1];

			rsnx = get_ie(auth, auth_len, WLAN_EID_RSNX);
			if (rsnx)
				nontx_profile_len += 2 + rsnx[1];
		}

		nontx_profile_len += hostapd_mbssid_ext_capa(bss, tx_bss,
							     ext_capa);

		if (!rsn && hostapd_wpa_ie(tx_bss, WLAN_EID_RSN))
			ie_count++;
		if (!rsnx && hostapd_wpa_ie(tx_bss, WLAN_EID_RSNX))
			ie_count++;
		if (bss->conf->xrates_supported)
			nontx_profile_len += 8;
		else if (hapd->conf->xrates_supported)
			ie_count++;
		if (ie_count)
			nontx_profile_len += 4 + ie_count;

		if (len + nontx_profile_len > 255)
			break;

		len += nontx_profile_len;
	}

	*bss_index = i;

	/* Add 2 octets to get the full size of the element */
	return len + 2;
}


size_t hostapd_eid_mbssid_len(struct hostapd_data *hapd, u32 frame_type,
			      u8 *elem_count, const u8 *known_bss,
			      size_t known_bss_len, size_t *rnr_len)
{
	size_t len = 0, bss_index = 1;
	bool ap_mld = false;

#ifdef CONFIG_IEEE80211BE
	ap_mld = hapd->conf->mld_ap;
#endif /* CONFIG_IEEE80211BE */

	if (!hapd->iconf->mbssid || hapd->iface->num_bss <= 1 ||
	    (frame_type != WLAN_FC_STYPE_BEACON &&
	     frame_type != WLAN_FC_STYPE_PROBE_RESP))
		return 0;

	if (frame_type == WLAN_FC_STYPE_BEACON) {
		if (!elem_count) {
			wpa_printf(MSG_INFO,
				   "MBSSID: Insufficient data for Beacon frames");
			return 0;
		}
		*elem_count = 0;
	}

	while (bss_index < hapd->iface->num_bss) {
		size_t rnr_count = bss_index;

		len += hostapd_eid_mbssid_elem_len(hapd, frame_type,
						   &bss_index, known_bss,
						   known_bss_len);

		if (frame_type == WLAN_FC_STYPE_BEACON)
			*elem_count += 1;
		if (hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED && rnr_len) {
			size_t rnr_cur_len = 0;
			struct mbssid_ie_profiles skip_profiles = {
				rnr_count, bss_index
			};

			*rnr_len += hostapd_eid_rnr_iface_len(
				hapd, hostapd_mbssid_get_tx_bss(hapd),
				&rnr_cur_len, &skip_profiles, ap_mld);
		}
	}

	if (hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED && rnr_len)
		*rnr_len += hostapd_eid_rnr_len(hapd, frame_type, false);

	return len;
}


static u8 * hostapd_eid_mbssid_elem(struct hostapd_data *hapd, u8 *eid, u8 *end,
				    u32 frame_type, u8 max_bssid_indicator,
				    size_t *bss_index, u8 elem_count,
				    const u8 *known_bss, size_t known_bss_len)
{
	struct hostapd_data *tx_bss = hostapd_mbssid_get_tx_bss(hapd);
	size_t i;
	u8 *eid_len_offset, *max_bssid_indicator_offset;

	*eid++ = WLAN_EID_MULTIPLE_BSSID;
	eid_len_offset = eid++;
	max_bssid_indicator_offset = eid++;

	for (i = *bss_index; i < hapd->iface->num_bss; i++) {
		struct hostapd_data *bss = hapd->iface->bss[i];
		struct hostapd_bss_config *conf;
		u8 *eid_len_pos, *nontx_bss_start = eid;
		const u8 *auth, *rsn = NULL, *rsnx = NULL;
		u8 ie_count = 0, non_inherit_ie[3];
		size_t auth_len = 0;
		u16 capab_info;

		if (!bss || !bss->conf || !bss->started ||
		    mbssid_known_bss(i, known_bss, known_bss_len))
			continue;
		conf = bss->conf;

		*eid++ = WLAN_MBSSID_SUBELEMENT_NONTRANSMITTED_BSSID_PROFILE;
		eid_len_pos = eid++;

		capab_info = hostapd_own_capab_info(bss);
		*eid++ = WLAN_EID_NONTRANSMITTED_BSSID_CAPA;
		*eid++ = sizeof(capab_info);
		WPA_PUT_LE16(eid, capab_info);
		eid += sizeof(capab_info);

		*eid++ = WLAN_EID_SSID;
		*eid++ = conf->ssid.ssid_len;
		os_memcpy(eid, conf->ssid.ssid, conf->ssid.ssid_len);
		eid += conf->ssid.ssid_len;

		*eid++ = WLAN_EID_MULTIPLE_BSSID_INDEX;
		if (frame_type == WLAN_FC_STYPE_BEACON) {
			*eid++ = 3;
			*eid++ = i; /* BSSID Index */
			if (hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED &&
			    (conf->dtim_period % elem_count))
				conf->dtim_period = elem_count;
			*eid++ = conf->dtim_period;
			/* The driver is expected to update the DTIM Count
			 * field for each BSS that corresponds to a
			 * nontransmitted BSSID. The value is initialized to
			 * 0 here so that the DTIM count would be somewhat
			 * functional even if the driver were not to update
			 * this. */
			*eid++ = 0; /* DTIM Count */
		} else {
			/* Probe Request frame does not include DTIM Period and
			 * DTIM Count fields. */
			*eid++ = 1;
			*eid++ = i; /* BSSID Index */
		}

		auth = wpa_auth_get_wpa_ie(bss->wpa_auth, &auth_len);
		if (auth) {
			rsn = get_ie(auth, auth_len, WLAN_EID_RSN);
			if (rsn) {
				os_memcpy(eid, rsn, 2 + rsn[1]);
				eid += 2 + rsn[1];
			}

			rsnx = get_ie(auth, auth_len, WLAN_EID_RSNX);
			if (rsnx) {
				os_memcpy(eid, rsnx, 2 + rsnx[1]);
				eid += 2 + rsnx[1];
			}
		}

		eid += hostapd_mbssid_ext_capa(bss, tx_bss, eid);

		/* List of Element ID values in increasing order */
		if (!rsn && hostapd_wpa_ie(tx_bss, WLAN_EID_RSN))
			non_inherit_ie[ie_count++] = WLAN_EID_RSN;
		if (hapd->conf->xrates_supported &&
		    !bss->conf->xrates_supported)
			non_inherit_ie[ie_count++] = WLAN_EID_EXT_SUPP_RATES;
		if (!rsnx && hostapd_wpa_ie(tx_bss, WLAN_EID_RSNX))
			non_inherit_ie[ie_count++] = WLAN_EID_RSNX;
		if (ie_count) {
			*eid++ = WLAN_EID_EXTENSION;
			*eid++ = 2 + ie_count + 1;
			*eid++ = WLAN_EID_EXT_NON_INHERITANCE;
			*eid++ = ie_count;
			os_memcpy(eid, non_inherit_ie, ie_count);
			eid += ie_count;
			*eid++ = 0; /* No Element ID Extension List */
		}

		*eid_len_pos = (eid - eid_len_pos) - 1;

		if (((eid - eid_len_offset) - 1) > 255) {
			eid = nontx_bss_start;
			break;
		}
	}

	*bss_index = i;
	*max_bssid_indicator_offset = max_bssid_indicator;
	if (*max_bssid_indicator_offset < 1)
		*max_bssid_indicator_offset = 1;
	*eid_len_offset = (eid - eid_len_offset) - 1;
	return eid;
}


u8 * hostapd_eid_mbssid(struct hostapd_data *hapd, u8 *eid, u8 *end,
			unsigned int frame_stype, u8 elem_count,
			u8 **elem_offset,
			const u8 *known_bss, size_t known_bss_len, u8 *rnr_eid,
			u8 *rnr_count, u8 **rnr_offset, size_t rnr_len)
{
	size_t bss_index = 1, cur_len = 0;
	u8 elem_index = 0, *rnr_start_eid = rnr_eid;
	bool add_rnr, ap_mld = false;

#ifdef CONFIG_IEEE80211BE
	ap_mld = hapd->conf->mld_ap;
#endif /* CONFIG_IEEE80211BE */

	if (!hapd->iconf->mbssid || hapd->iface->num_bss <= 1 ||
	    (frame_stype != WLAN_FC_STYPE_BEACON &&
	     frame_stype != WLAN_FC_STYPE_PROBE_RESP))
		return eid;

	if (frame_stype == WLAN_FC_STYPE_BEACON && !elem_offset) {
		wpa_printf(MSG_INFO,
			   "MBSSID: Insufficient data for Beacon frames");
		return eid;
	}

	add_rnr = hapd->iconf->mbssid == ENHANCED_MBSSID_ENABLED &&
		frame_stype == WLAN_FC_STYPE_BEACON &&
		rnr_eid && rnr_count && rnr_offset && rnr_len;

	while (bss_index < hapd->iface->num_bss) {
		unsigned int rnr_start_count = bss_index;

		if (frame_stype == WLAN_FC_STYPE_BEACON) {
			if (elem_index == elem_count) {
				wpa_printf(MSG_WARNING,
					   "MBSSID: Larger number of elements than there is room in the provided array");
				break;
			}

			elem_offset[elem_index] = eid;
			elem_index = elem_index + 1;
		}
		eid = hostapd_eid_mbssid_elem(hapd, eid, end, frame_stype,
					      hostapd_max_bssid_indicator(hapd),
					      &bss_index, elem_count,
					      known_bss, known_bss_len);

		if (add_rnr) {
			struct mbssid_ie_profiles skip_profiles = {
				rnr_start_count, bss_index
			};

			rnr_offset[*rnr_count] = rnr_eid;
			*rnr_count = *rnr_count + 1;
			cur_len = 0;
			rnr_eid = hostapd_eid_rnr_iface(
				hapd, hostapd_mbssid_get_tx_bss(hapd),
				rnr_eid, &cur_len, &skip_profiles, ap_mld);
		}
	}

	if (add_rnr && (size_t) (rnr_eid - rnr_start_eid) < rnr_len) {
		rnr_offset[*rnr_count] = rnr_eid;
		*rnr_count = *rnr_count + 1;
		cur_len = 0;

		if (hapd->conf->rnr)
			rnr_eid = hostapd_eid_nr_db(hapd, rnr_eid, &cur_len);
		if (get_colocation_mode(hapd) == COLOCATED_LOWER_BAND)
			rnr_eid = hostapd_eid_rnr_colocation(hapd, rnr_eid,
							     &cur_len);
	}

	return eid;
}

#endif /* CONFIG_NATIVE_WINDOWS */
