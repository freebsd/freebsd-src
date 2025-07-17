/*
 * PASN responder processing
 *
 * Copyright (C) 2019, Intel Corporation
 * Copyright (C) 2022, Qualcomm Innovation Center, Inc.
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/wpa_common.h"
#include "common/sae.h"
#include "common/ieee802_11_common.h"
#include "common/ieee802_11_defs.h"
#include "crypto/sha384.h"
#include "crypto/sha256.h"
#include "crypto/random.h"
#include "crypto/crypto.h"
#include "ap/hostapd.h"
#include "ap/comeback_token.h"
#include "ap/ieee802_1x.h"
#include "ap/pmksa_cache_auth.h"
#include "pasn_common.h"


void pasn_set_responder_pmksa(struct pasn_data *pasn,
			      struct rsn_pmksa_cache *pmksa)
{
	if (pasn)
		pasn->pmksa = pmksa;
}


#ifdef CONFIG_PASN
#ifdef CONFIG_SAE

static int pasn_wd_handle_sae_commit(struct pasn_data *pasn,
				     const u8 *own_addr, const u8 *peer_addr,
				     struct wpabuf *wd)
{
	const u8 *data;
	size_t buf_len;
	u16 res, alg, seq, status;
	int groups[] = { pasn->group, 0 };
	int ret;

	if (!wd)
		return -1;

	data = wpabuf_head_u8(wd);
	buf_len = wpabuf_len(wd);

	if (buf_len < 6) {
		wpa_printf(MSG_DEBUG, "PASN: SAE buffer too short. len=%zu",
			   buf_len);
		return -1;
	}

	alg = WPA_GET_LE16(data);
	seq = WPA_GET_LE16(data + 2);
	status = WPA_GET_LE16(data + 4);

	wpa_printf(MSG_DEBUG, "PASN: SAE commit: alg=%u, seq=%u, status=%u",
		   alg, seq, status);

	if (alg != WLAN_AUTH_SAE || seq != 1 ||
	    status != WLAN_STATUS_SAE_HASH_TO_ELEMENT) {
		wpa_printf(MSG_DEBUG, "PASN: Dropping peer SAE commit");
		return -1;
	}

	sae_clear_data(&pasn->sae);
	pasn->sae.state = SAE_NOTHING;

	ret = sae_set_group(&pasn->sae, pasn->group);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to set SAE group");
		return -1;
	}

	if (!pasn->password || !pasn->pt) {
		wpa_printf(MSG_DEBUG, "PASN: No SAE PT found");
		return -1;
	}

	ret = sae_prepare_commit_pt(&pasn->sae, pasn->pt, own_addr, peer_addr,
				    NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to prepare SAE commit");
		return -1;
	}

	res = sae_parse_commit(&pasn->sae, data + 6, buf_len - 6, NULL, 0,
			       groups, 0, NULL);
	if (res != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG, "PASN: Failed parsing SAE commit");
		return -1;
	}

	/* Process the commit message and derive the PMK */
	ret = sae_process_commit(&pasn->sae);
	if (ret) {
		wpa_printf(MSG_DEBUG, "SAE: Failed to process peer commit");
		return -1;
	}

	pasn->sae.state = SAE_COMMITTED;

	return 0;
}


static int pasn_wd_handle_sae_confirm(struct pasn_data *pasn,
				      const u8 *peer_addr, struct wpabuf *wd)
{
	const u8 *data;
	size_t buf_len;
	u16 res, alg, seq, status;

	if (!wd)
		return -1;

	data = wpabuf_head_u8(wd);
	buf_len = wpabuf_len(wd);

	if (buf_len < 6) {
		wpa_printf(MSG_DEBUG, "PASN: SAE buffer too short. len=%zu",
			   buf_len);
		return -1;
	}

	alg = WPA_GET_LE16(data);
	seq = WPA_GET_LE16(data + 2);
	status = WPA_GET_LE16(data + 4);

	wpa_printf(MSG_DEBUG, "PASN: SAE confirm: alg=%u, seq=%u, status=%u",
		   alg, seq, status);

	if (alg != WLAN_AUTH_SAE || seq != 2 || status != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG, "PASN: Dropping peer SAE confirm");
		return -1;
	}

	res = sae_check_confirm(&pasn->sae, data + 6, buf_len - 6, NULL);
	if (res != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG, "PASN: SAE failed checking confirm");
		return -1;
	}

	pasn->sae.state = SAE_ACCEPTED;

	/*
	 * TODO: Based on on IEEE P802.11az/D2.6, the PMKSA derived with
	 * PASN/SAE should only be allowed with future PASN only. For now do not
	 * restrict this only for PASN.
	 */
	if (pasn->disable_pmksa_caching)
		return 0;

	wpa_hexdump_key(MSG_DEBUG, "RSN: Cache PMK from SAE",
			pasn->sae.pmk, pasn->sae.pmk_len);
	if (!pasn->sae.akmp)
		pasn->sae.akmp = WPA_KEY_MGMT_SAE;

	pmksa_cache_auth_add(pasn->pmksa, pasn->sae.pmk, pasn->sae.pmk_len,
			     pasn->sae.pmkid, NULL, 0, pasn->own_addr,
			     peer_addr, 0, NULL, pasn->sae.akmp);
	return 0;
}


static struct wpabuf * pasn_get_sae_wd(struct pasn_data *pasn)
{
	struct wpabuf *buf = NULL;
	u8 *len_ptr;
	size_t len;

	/* Need to add the entire Authentication frame body */
	buf = wpabuf_alloc(8 + SAE_COMMIT_MAX_LEN + 8 + SAE_CONFIRM_MAX_LEN);
	if (!buf) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to allocate SAE buffer");
		return NULL;
	}

	/* Need to add the entire authentication frame body for the commit */
	len_ptr = wpabuf_put(buf, 2);
	wpabuf_put_le16(buf, WLAN_AUTH_SAE);
	wpabuf_put_le16(buf, 1);
	wpabuf_put_le16(buf, WLAN_STATUS_SAE_HASH_TO_ELEMENT);

	/* Write the actual commit and update the length accordingly */
	sae_write_commit(&pasn->sae, buf, NULL, 0);
	len = wpabuf_len(buf);
	WPA_PUT_LE16(len_ptr, len - 2);

	/* Need to add the entire Authentication frame body for the confirm */
	len_ptr = wpabuf_put(buf, 2);
	wpabuf_put_le16(buf, WLAN_AUTH_SAE);
	wpabuf_put_le16(buf, 2);
	wpabuf_put_le16(buf, WLAN_STATUS_SUCCESS);

	sae_write_confirm(&pasn->sae, buf);
	WPA_PUT_LE16(len_ptr, wpabuf_len(buf) - len - 2);

	pasn->sae.state = SAE_CONFIRMED;

	return buf;
}

#endif /* CONFIG_SAE */


#ifdef CONFIG_FILS

static struct wpabuf * pasn_get_fils_wd(struct pasn_data *pasn)
{
	struct pasn_fils *fils = &pasn->fils;
	struct wpabuf *buf = NULL;

	if (!fils->erp_resp) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Missing erp_resp");
		return NULL;
	}

	buf = wpabuf_alloc(1500);
	if (!buf)
		return NULL;

	/* Add the authentication algorithm */
	wpabuf_put_le16(buf, WLAN_AUTH_FILS_SK);

	/* Authentication Transaction seq# */
	wpabuf_put_le16(buf, 2);

	/* Status Code */
	wpabuf_put_le16(buf, WLAN_STATUS_SUCCESS);

	/* Own RSNE */
	wpa_pasn_add_rsne(buf, NULL, pasn->akmp, pasn->cipher);

	/* FILS Nonce */
	wpabuf_put_u8(buf, WLAN_EID_EXTENSION);
	wpabuf_put_u8(buf, 1 + FILS_NONCE_LEN);
	wpabuf_put_u8(buf, WLAN_EID_EXT_FILS_NONCE);
	wpabuf_put_data(buf, fils->anonce, FILS_NONCE_LEN);

	/* FILS Session */
	wpabuf_put_u8(buf, WLAN_EID_EXTENSION);
	wpabuf_put_u8(buf, 1 + FILS_SESSION_LEN);
	wpabuf_put_u8(buf, WLAN_EID_EXT_FILS_SESSION);
	wpabuf_put_data(buf, fils->session, FILS_SESSION_LEN);

	/* Wrapped Data */
	wpabuf_put_u8(buf, WLAN_EID_EXTENSION);
	wpabuf_put_u8(buf, 1 + wpabuf_len(fils->erp_resp));
	wpabuf_put_u8(buf, WLAN_EID_EXT_WRAPPED_DATA);
	wpabuf_put_buf(buf, fils->erp_resp);

	return buf;
}

#endif /* CONFIG_FILS */

static struct wpabuf * pasn_get_wrapped_data(struct pasn_data *pasn)
{
	switch (pasn->akmp) {
	case WPA_KEY_MGMT_PASN:
		/* no wrapped data */
		return NULL;
	case WPA_KEY_MGMT_SAE:
#ifdef CONFIG_SAE
		return pasn_get_sae_wd(pasn);
#else /* CONFIG_SAE */
		wpa_printf(MSG_ERROR,
			   "PASN: SAE: Cannot derive wrapped data");
		return NULL;
#endif /* CONFIG_SAE */
	case WPA_KEY_MGMT_FILS_SHA256:
	case WPA_KEY_MGMT_FILS_SHA384:
#ifdef CONFIG_FILS
		return pasn_get_fils_wd(pasn);
#endif /* CONFIG_FILS */
		/* fall through */
	case WPA_KEY_MGMT_FT_PSK:
	case WPA_KEY_MGMT_FT_IEEE8021X:
	case WPA_KEY_MGMT_FT_IEEE8021X_SHA384:
	default:
		wpa_printf(MSG_ERROR,
			   "PASN: TODO: Wrapped data for akmp=0x%x",
			   pasn->akmp);
		return NULL;
	}
}


static int
pasn_derive_keys(struct pasn_data *pasn,
		 const u8 *own_addr, const u8 *peer_addr,
		 const u8 *cached_pmk, size_t cached_pmk_len,
		 struct wpa_pasn_params_data *pasn_data,
		 struct wpabuf *wrapped_data,
		 struct wpabuf *secret)
{
	static const u8 pasn_default_pmk[] = {'P', 'M', 'K', 'z'};
	u8 pmk[PMK_LEN_MAX];
	u8 pmk_len;
	int ret;

	os_memset(pmk, 0, sizeof(pmk));
	pmk_len = 0;

	if (!cached_pmk || !cached_pmk_len)
		wpa_printf(MSG_DEBUG, "PASN: No valid PMKSA entry");

	if (pasn->akmp == WPA_KEY_MGMT_PASN) {
		wpa_printf(MSG_DEBUG, "PASN: Using default PMK");

		pmk_len = WPA_PASN_PMK_LEN;
		os_memcpy(pmk, pasn_default_pmk, sizeof(pasn_default_pmk));
	} else if (cached_pmk && cached_pmk_len) {
		wpa_printf(MSG_DEBUG, "PASN: Using PMKSA entry");

		pmk_len = cached_pmk_len;
		os_memcpy(pmk, cached_pmk, cached_pmk_len);
	} else {
		switch (pasn->akmp) {
#ifdef CONFIG_SAE
		case WPA_KEY_MGMT_SAE:
			if (pasn->sae.state == SAE_COMMITTED) {
				pmk_len = PMK_LEN;
				os_memcpy(pmk, pasn->sae.pmk, PMK_LEN);
				break;
			}
#endif /* CONFIG_SAE */
			/* fall through */
		default:
			/* TODO: Derive PMK based on wrapped data */
			wpa_printf(MSG_DEBUG,
				   "PASN: Missing PMK derivation");
			return -1;
		}
	}

	pasn->pmk_len = pmk_len;
	os_memcpy(pasn->pmk, pmk, pmk_len);
	ret = pasn_pmk_to_ptk(pmk, pmk_len, peer_addr, own_addr,
			      wpabuf_head(secret), wpabuf_len(secret),
			      &pasn->ptk, pasn->akmp,
			      pasn->cipher, pasn->kdk_len);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to derive PTK");
		return -1;
	}

	if (pasn->secure_ltf) {
		ret = wpa_ltf_keyseed(&pasn->ptk, pasn->akmp,
				      pasn->cipher);
		if (ret) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Failed to derive LTF keyseed");
			return -1;
		}
	}

	wpa_printf(MSG_DEBUG, "PASN: PTK successfully derived");
	return 0;
}


static void handle_auth_pasn_comeback(struct pasn_data *pasn,
				      const u8 *own_addr, const u8 *peer_addr,
				      u16 group)
{
	struct wpabuf *buf, *comeback;
	int ret;

	wpa_printf(MSG_DEBUG,
		   "PASN: Building comeback frame 2. Comeback after=%u",
		   pasn->comeback_after);

	buf = wpabuf_alloc(1500);
	if (!buf)
		return;

	wpa_pasn_build_auth_header(buf, pasn->bssid, own_addr, peer_addr, 2,
				   WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY);

	/*
	 * Do not include the group as a part of the token since it is not going
	 * to be used.
	 */
	comeback = auth_build_token_req(&pasn->last_comeback_key_update,
					pasn->comeback_key, pasn->comeback_idx,
					pasn->comeback_pending_idx,
					sizeof(u16) * COMEBACK_PENDING_IDX_SIZE,
					0, peer_addr, 0);
	if (!comeback) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Failed sending auth with comeback");
		wpabuf_free(buf);
		return;
	}

	wpa_pasn_add_parameter_ie(buf, group,
				  WPA_PASN_WRAPPED_DATA_NO,
				  NULL, 0, comeback,
				  pasn->comeback_after);
	wpabuf_free(comeback);

	wpa_printf(MSG_DEBUG,
		   "PASN: comeback: STA=" MACSTR, MAC2STR(peer_addr));

	ret = pasn->send_mgmt(pasn->cb_ctx, wpabuf_head_u8(buf),
			      wpabuf_len(buf), 0, 0, 0);
	if (ret)
		wpa_printf(MSG_INFO, "PASN: Failed to send comeback frame 2");

	wpabuf_free(buf);
}


int handle_auth_pasn_resp(struct pasn_data *pasn, const u8 *own_addr,
			  const u8 *peer_addr,
			  struct rsn_pmksa_cache_entry *pmksa, u16 status)
{
	struct wpabuf *buf, *pubkey = NULL, *wrapped_data_buf = NULL;
	struct wpabuf *rsn_buf = NULL;
	u8 mic[WPA_PASN_MAX_MIC_LEN];
	u8 mic_len;
	u8 *ptr;
	const u8 *frame, *data, *rsn_ie, *rsnxe_ie;
	u8 *data_buf = NULL;
	size_t frame_len, data_len;
	int ret;
	const u8 *pmkid = NULL;

	wpa_printf(MSG_DEBUG, "PASN: Building frame 2: status=%u", status);

	buf = wpabuf_alloc(1500);
	if (!buf)
		goto fail;

	wpa_pasn_build_auth_header(buf, pasn->bssid, own_addr, peer_addr, 2,
				   status);

	if (status != WLAN_STATUS_SUCCESS)
		goto done;

	if (pmksa && pasn->custom_pmkid_valid)
		pmkid = pasn->custom_pmkid;
	else if (pmksa) {
		pmkid = pmksa->pmkid;
#ifdef CONFIG_SAE
	} else if (pasn->akmp == WPA_KEY_MGMT_SAE) {
		wpa_printf(MSG_DEBUG, "PASN: Use SAE PMKID");
		pmkid = pasn->sae.pmkid;
#endif /* CONFIG_SAE */
#ifdef CONFIG_FILS
	} else if (pasn->akmp == WPA_KEY_MGMT_FILS_SHA256 ||
		   pasn->akmp == WPA_KEY_MGMT_FILS_SHA384) {
		wpa_printf(MSG_DEBUG, "PASN: Use FILS ERP PMKID");
		pmkid = pasn->fils.erp_pmkid;
#endif /* CONFIG_FILS */
	}

	if (wpa_pasn_add_rsne(buf, pmkid,
			      pasn->akmp, pasn->cipher) < 0)
		goto fail;

	/* No need to derive PMK if PMKSA is given */
	if (!pmksa)
		wrapped_data_buf = pasn_get_wrapped_data(pasn);
	else
		pasn->wrapped_data_format = WPA_PASN_WRAPPED_DATA_NO;

	/* Get public key */
	pubkey = crypto_ecdh_get_pubkey(pasn->ecdh, 0);
	pubkey = wpabuf_zeropad(pubkey,
				crypto_ecdh_prime_len(pasn->ecdh));
	if (!pubkey) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to get pubkey");
		goto fail;
	}

	wpa_pasn_add_parameter_ie(buf, pasn->group,
				  pasn->wrapped_data_format,
				  pubkey, true, NULL, 0);

	if (wpa_pasn_add_wrapped_data(buf, wrapped_data_buf) < 0)
		goto fail;

	wpabuf_free(wrapped_data_buf);
	wrapped_data_buf = NULL;
	wpabuf_free(pubkey);
	pubkey = NULL;

	/* Add RSNXE if needed */
	rsnxe_ie = pasn->rsnxe_ie;
	if (rsnxe_ie)
		wpabuf_put_data(buf, rsnxe_ie, 2 + rsnxe_ie[1]);

	wpa_pasn_add_extra_ies(buf, pasn->extra_ies, pasn->extra_ies_len);

	/* Add the mic */
	mic_len = pasn_mic_len(pasn->akmp, pasn->cipher);
	wpabuf_put_u8(buf, WLAN_EID_MIC);
	wpabuf_put_u8(buf, mic_len);
	ptr = wpabuf_put(buf, mic_len);

	os_memset(ptr, 0, mic_len);

	frame = wpabuf_head_u8(buf) + IEEE80211_HDRLEN;
	frame_len = wpabuf_len(buf) - IEEE80211_HDRLEN;

	if (pasn->rsn_ie && pasn->rsn_ie_len) {
		rsn_ie = pasn->rsn_ie;
	} else {
		/*
		 * Note: when pasn->rsn_ie is NULL, it is likely that Beacon
		 * frame RSNE is not initialized. This is possible in case of
		 * PASN authentication used for Wi-Fi Aware for which Beacon
		 * frame RSNE and RSNXE are same as RSNE and RSNXE in the
		 * Authentication frame.
		 */
		rsn_buf = wpabuf_alloc(500);
		if (!rsn_buf)
			goto fail;

		if (wpa_pasn_add_rsne(rsn_buf, pmkid,
				      pasn->akmp, pasn->cipher) < 0)
			goto fail;

		rsn_ie = wpabuf_head_u8(rsn_buf);
	}

	/*
	 * Note: wpa_auth_get_wpa_ie() might return not only the RSNE but also
	 * MDE, etc. Thus, do not use the returned length but instead use the
	 * length specified in the IE header.
	 */
	data_len = rsn_ie[1] + 2;
	if (rsnxe_ie) {
		data_buf = os_zalloc(rsn_ie[1] + 2 + rsnxe_ie[1] + 2);
		if (!data_buf)
			goto fail;

		os_memcpy(data_buf, rsn_ie, rsn_ie[1] + 2);
		os_memcpy(data_buf + rsn_ie[1] + 2, rsnxe_ie, rsnxe_ie[1] + 2);
		data_len += rsnxe_ie[1] + 2;
		data = data_buf;
	} else {
		data = rsn_ie;
	}

	ret = pasn_mic(pasn->ptk.kck, pasn->akmp, pasn->cipher,
		       own_addr, peer_addr, data, data_len,
		       frame, frame_len, mic);
	os_free(data_buf);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Frame 3: Failed MIC calculation");
		goto fail;
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (pasn->corrupt_mic) {
		wpa_printf(MSG_DEBUG, "PASN: frame 2: Corrupt MIC");
		mic[0] = ~mic[0];
	}
#endif /* CONFIG_TESTING_OPTIONS */

	os_memcpy(ptr, mic, mic_len);

done:
	wpa_printf(MSG_DEBUG,
		   "PASN: Building frame 2: success; resp STA=" MACSTR,
		   MAC2STR(peer_addr));

	ret = pasn->send_mgmt(pasn->cb_ctx, wpabuf_head_u8(buf),
			      wpabuf_len(buf), 0, 0, 0);
	if (ret)
		wpa_printf(MSG_INFO, "send_auth_reply: Send failed");

	wpabuf_free(rsn_buf);
	wpabuf_free(buf);
	return ret;
fail:
	wpabuf_free(wrapped_data_buf);
	wpabuf_free(pubkey);
	wpabuf_free(rsn_buf);
	wpabuf_free(buf);
	return -1;
}


int handle_auth_pasn_1(struct pasn_data *pasn,
		       const u8 *own_addr, const u8 *peer_addr,
		       const struct ieee80211_mgmt *mgmt, size_t len)
{
	struct ieee802_11_elems elems;
	struct wpa_ie_data rsn_data;
	struct wpa_pasn_params_data pasn_params;
	struct rsn_pmksa_cache_entry *pmksa = NULL;
	const u8 *cached_pmk = NULL;
	size_t cached_pmk_len = 0;
	struct wpabuf *wrapped_data = NULL, *secret = NULL;
	const int *groups = pasn->pasn_groups;
	static const int default_groups[] = { 19, 0 };
	u16 status = WLAN_STATUS_SUCCESS;
	int ret, inc_y;
	bool derive_keys;
	u32 i;

	if (!groups)
		groups = default_groups;

	if (ieee802_11_parse_elems(mgmt->u.auth.variable,
				   len - offsetof(struct ieee80211_mgmt,
						  u.auth.variable),
				   &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Failed parsing Authentication frame");
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto send_resp;
	}

	if (!elems.rsn_ie) {
		wpa_printf(MSG_DEBUG, "PASN: No RSNE");
		status = WLAN_STATUS_INVALID_RSNIE;
		goto send_resp;
	}

	ret = wpa_parse_wpa_ie_rsn(elems.rsn_ie - 2, elems.rsn_ie_len + 2,
				   &rsn_data);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed parsing RSNE");
		status = WLAN_STATUS_INVALID_RSNIE;
		goto send_resp;
	}

	ret = wpa_pasn_validate_rsne(&rsn_data);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed validating RSNE");
		status = WLAN_STATUS_INVALID_RSNIE;
		goto send_resp;
	}

	if (!(rsn_data.key_mgmt & pasn->wpa_key_mgmt) ||
	    !(rsn_data.pairwise_cipher & pasn->rsn_pairwise)) {
		wpa_printf(MSG_DEBUG, "PASN: Mismatch in AKMP/cipher");
		status = WLAN_STATUS_INVALID_RSNIE;
		goto send_resp;
	}

	pasn->akmp = rsn_data.key_mgmt;
	pasn->cipher = rsn_data.pairwise_cipher;

	if (pasn->derive_kdk &&
	    ieee802_11_rsnx_capab_len(elems.rsnxe, elems.rsnxe_len,
				      WLAN_RSNX_CAPAB_SECURE_LTF))
		pasn->secure_ltf = true;

	if (pasn->derive_kdk)
		pasn->kdk_len = WPA_KDK_MAX_LEN;
	else
		pasn->kdk_len = 0;

	wpa_printf(MSG_DEBUG, "PASN: kdk_len=%zu", pasn->kdk_len);

	if (!elems.pasn_params || !elems.pasn_params_len) {
		wpa_printf(MSG_DEBUG,
			   "PASN: No PASN Parameters element found");
		status = WLAN_STATUS_INVALID_PARAMETERS;
		goto send_resp;
	}

	ret = wpa_pasn_parse_parameter_ie(elems.pasn_params - 3,
					  elems.pasn_params_len + 3,
					  false, &pasn_params);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Failed validation of PASN Parameters IE");
		status = WLAN_STATUS_INVALID_PARAMETERS;
		goto send_resp;
	}

	for (i = 0; groups[i] > 0 && groups[i] != pasn_params.group; i++)
		;

	if (!pasn_params.group || groups[i] != pasn_params.group) {
		wpa_printf(MSG_DEBUG, "PASN: Requested group=%hu not allowed",
			   pasn_params.group);
		status = WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;
		goto send_resp;
	}

	if (!pasn_params.pubkey || !pasn_params.pubkey_len) {
		wpa_printf(MSG_DEBUG, "PASN: Invalid public key");
		status = WLAN_STATUS_INVALID_PARAMETERS;
		goto send_resp;
	}

	if (pasn_params.comeback) {
		wpa_printf(MSG_DEBUG, "PASN: Checking peer comeback token");

		ret = check_comeback_token(pasn->comeback_key,
					   pasn->comeback_pending_idx,
					   peer_addr,
					   pasn_params.comeback,
					   pasn_params.comeback_len);

		if (ret) {
			wpa_printf(MSG_DEBUG, "PASN: Invalid comeback token");
			status = WLAN_STATUS_INVALID_PARAMETERS;
			goto send_resp;
		}
	} else if (pasn->use_anti_clogging) {
		wpa_printf(MSG_DEBUG, "PASN: Respond with comeback");
		handle_auth_pasn_comeback(pasn, own_addr, peer_addr,
					  pasn_params.group);
		return -1;
	}

	pasn->ecdh = crypto_ecdh_init(pasn_params.group);
	if (!pasn->ecdh) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to init ECDH");
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto send_resp;
	}

	pasn->group = pasn_params.group;

	if (pasn_params.pubkey[0] == WPA_PASN_PUBKEY_UNCOMPRESSED) {
		inc_y = 1;
	} else if (pasn_params.pubkey[0] == WPA_PASN_PUBKEY_COMPRESSED_0 ||
		   pasn_params.pubkey[0] == WPA_PASN_PUBKEY_COMPRESSED_1) {
		inc_y = 0;
	} else {
		wpa_printf(MSG_DEBUG,
			   "PASN: Invalid first octet in pubkey=0x%x",
			   pasn_params.pubkey[0]);
		status = WLAN_STATUS_INVALID_PUBLIC_KEY;
		goto send_resp;
	}

	secret = crypto_ecdh_set_peerkey(pasn->ecdh, inc_y,
					 pasn_params.pubkey + 1,
					 pasn_params.pubkey_len - 1);
	if (!secret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to derive shared secret");
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto send_resp;
	}

	if (!pasn->noauth && pasn->akmp == WPA_KEY_MGMT_PASN) {
		wpa_printf(MSG_DEBUG, "PASN: Refuse PASN-UNAUTH");
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto send_resp;
	}

	derive_keys = true;
	if (pasn_params.wrapped_data_format != WPA_PASN_WRAPPED_DATA_NO) {
		wrapped_data = ieee802_11_defrag(elems.wrapped_data,
						 elems.wrapped_data_len, true);
		if (!wrapped_data) {
			wpa_printf(MSG_DEBUG, "PASN: Missing wrapped data");
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto send_resp;
		}

#ifdef CONFIG_SAE
		if (pasn->akmp == WPA_KEY_MGMT_SAE) {
			ret = pasn_wd_handle_sae_commit(pasn, own_addr,
							peer_addr,
							wrapped_data);
			if (ret) {
				wpa_printf(MSG_DEBUG,
					   "PASN: Failed processing SAE commit");
				status = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto send_resp;
			}
		}
#endif /* CONFIG_SAE */
#ifdef CONFIG_FILS
		if (pasn->akmp == WPA_KEY_MGMT_FILS_SHA256 ||
		    pasn->akmp == WPA_KEY_MGMT_FILS_SHA384) {
			if (!pasn->fils_wd_valid) {
				wpa_printf(MSG_DEBUG,
					   "PASN: Invalid FILS wrapped data");
				status = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto send_resp;
			}

			wpa_printf(MSG_DEBUG,
				   "PASN: FILS: Pending AS response");

			/*
			 * With PASN/FILS, keys can be derived only after a
			 * response from the AS is processed.
			 */
			derive_keys = false;
		}
#endif /* CONFIG_FILS */
	}

	pasn->wrapped_data_format = pasn_params.wrapped_data_format;

	ret = pasn_auth_frame_hash(pasn->akmp, pasn->cipher,
				   ((const u8 *) mgmt) + IEEE80211_HDRLEN,
				   len - IEEE80211_HDRLEN, pasn->hash);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to compute hash");
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
		goto send_resp;
	}

	if (!derive_keys) {
		wpa_printf(MSG_DEBUG, "PASN: Storing secret");
		pasn->secret = secret;
		wpabuf_free(wrapped_data);
		return 0;
	}

	if (rsn_data.num_pmkid) {
		if (wpa_key_mgmt_ft(pasn->akmp)) {
#ifdef CONFIG_IEEE80211R_AP
			wpa_printf(MSG_DEBUG, "PASN: FT: Fetch PMK-R1");

			if (!pasn->pmk_r1_len) {
				wpa_printf(MSG_DEBUG,
					   "PASN: FT: Failed getting PMK-R1");
				status = WLAN_STATUS_UNSPECIFIED_FAILURE;
				goto send_resp;
			}
			cached_pmk = pasn->pmk_r1;
			cached_pmk_len = pasn->pmk_r1_len;
#else /* CONFIG_IEEE80211R_AP */
			wpa_printf(MSG_DEBUG, "PASN: FT: Not supported");
			status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			goto send_resp;
#endif /* CONFIG_IEEE80211R_AP */
		} else {
			wpa_printf(MSG_DEBUG, "PASN: Try to find PMKSA entry");

			if (pasn->pmksa) {
				const u8 *pmkid = NULL;

				if (pasn->custom_pmkid_valid) {
					ret = pasn->validate_custom_pmkid(
						pasn->cb_ctx, peer_addr,
						rsn_data.pmkid);
					if (ret) {
						wpa_printf(MSG_DEBUG,
							   "PASN: Failed custom PMKID validation");
						status = WLAN_STATUS_UNSPECIFIED_FAILURE;
						goto send_resp;
					}
				} else {
					pmkid = rsn_data.pmkid;
				}

				pmksa = pmksa_cache_auth_get(pasn->pmksa,
							     peer_addr,
							     pmkid);
				if (pmksa) {
					cached_pmk = pmksa->pmk;
					cached_pmk_len = pmksa->pmk_len;
				}
			}
		}
	} else {
		wpa_printf(MSG_DEBUG, "PASN: No PMKID specified");
	}

	ret = pasn_derive_keys(pasn, own_addr, peer_addr,
			       cached_pmk, cached_pmk_len,
			       &pasn_params, wrapped_data, secret);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to derive keys");
		status = WLAN_STATUS_PASN_BASE_AKMP_FAILED;
		goto send_resp;
	}

	ret = pasn_auth_frame_hash(pasn->akmp, pasn->cipher,
				   ((const u8 *) mgmt) + IEEE80211_HDRLEN,
				   len - IEEE80211_HDRLEN, pasn->hash);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to compute hash");
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

send_resp:
	ret = handle_auth_pasn_resp(pasn, own_addr, peer_addr, pmksa, status);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to send response");
		status = WLAN_STATUS_UNSPECIFIED_FAILURE;
	} else {
		wpa_printf(MSG_DEBUG,
			   "PASN: Success handling transaction == 1");
	}

	wpabuf_free(secret);
	wpabuf_free(wrapped_data);

	if (status != WLAN_STATUS_SUCCESS)
		return -1;

	return 0;
}


int handle_auth_pasn_3(struct pasn_data *pasn, const u8 *own_addr,
		       const u8 *peer_addr,
		       const struct ieee80211_mgmt *mgmt, size_t len)
{
	struct ieee802_11_elems elems;
	struct wpa_pasn_params_data pasn_params;
	struct wpabuf *wrapped_data = NULL;
	u8 mic[WPA_PASN_MAX_MIC_LEN], out_mic[WPA_PASN_MAX_MIC_LEN];
	u8 mic_len;
	int ret;
	u8 *copy = NULL;
	size_t copy_len, mic_offset;

	if (ieee802_11_parse_elems(mgmt->u.auth.variable,
				   len - offsetof(struct ieee80211_mgmt,
						  u.auth.variable),
				   &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Failed parsing Authentication frame");
		goto fail;
	}

	/* Check that the MIC IE exists. Save it and zero out the memory. */
	mic_len = pasn_mic_len(pasn->akmp, pasn->cipher);
	if (!elems.mic || elems.mic_len != mic_len) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Invalid MIC. Expecting len=%u", mic_len);
		goto fail;
	}
	os_memcpy(mic, elems.mic, mic_len);

	if (!elems.pasn_params || !elems.pasn_params_len) {
		wpa_printf(MSG_DEBUG,
			   "PASN: No PASN Parameters element found");
		goto fail;
	}

	ret = wpa_pasn_parse_parameter_ie(elems.pasn_params - 3,
					  elems.pasn_params_len + 3,
					  false, &pasn_params);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Failed validation of PASN Parameters IE");
		goto fail;
	}

	if (pasn_params.pubkey || pasn_params.pubkey_len) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Public key should not be included");
		goto fail;
	}

	/* Verify the MIC */
	copy_len = len - offsetof(struct ieee80211_mgmt, u.auth);
	mic_offset = elems.mic - (const u8 *) &mgmt->u.auth;
	copy_len = len - offsetof(struct ieee80211_mgmt, u.auth);
	if (mic_offset + mic_len > copy_len)
		goto fail;
	copy = os_memdup(&mgmt->u.auth, copy_len);
	if (!copy)
		goto fail;
	os_memset(copy + mic_offset, 0, mic_len);
	ret = pasn_mic(pasn->ptk.kck, pasn->akmp, pasn->cipher,
		       peer_addr, own_addr,
		       pasn->hash, mic_len * 2,
		       copy, copy_len, out_mic);
	os_free(copy);
	copy = NULL;

	wpa_hexdump_key(MSG_DEBUG, "PASN: Frame MIC", mic, mic_len);
	if (ret || os_memcmp(mic, out_mic, mic_len) != 0) {
		wpa_printf(MSG_DEBUG, "PASN: Failed MIC verification");
		goto fail;
	}

	if (pasn_params.wrapped_data_format != WPA_PASN_WRAPPED_DATA_NO) {
		wrapped_data = ieee802_11_defrag(elems.wrapped_data,
						 elems.wrapped_data_len,
						 true);

		if (!wrapped_data) {
			wpa_printf(MSG_DEBUG, "PASN: Missing wrapped data");
			goto fail;
		}

#ifdef CONFIG_SAE
		if (pasn->akmp == WPA_KEY_MGMT_SAE) {
			ret = pasn_wd_handle_sae_confirm(pasn, peer_addr,
							 wrapped_data);
			if (ret) {
				wpa_printf(MSG_DEBUG,
					   "PASN: Failed processing SAE confirm");
				wpabuf_free(wrapped_data);
				goto fail;
			}
		}
#endif /* CONFIG_SAE */
#ifdef CONFIG_FILS
		if (pasn->akmp == WPA_KEY_MGMT_FILS_SHA256 ||
		    pasn->akmp == WPA_KEY_MGMT_FILS_SHA384) {
			if (wrapped_data) {
				wpa_printf(MSG_DEBUG,
					   "PASN: FILS: Ignore wrapped data");
			}
		}
#endif /* CONFIG_FILS */
		wpabuf_free(wrapped_data);
	}

	wpa_printf(MSG_INFO,
		   "PASN: Success handling transaction == 3. Store PTK");
	return 0;

fail:
	os_free(copy);
	return -1;
}

#endif /* CONFIG_PASN */
