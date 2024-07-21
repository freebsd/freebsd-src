/*
 * WPA Supplicant - IEEE 802.11r - Fast BSS Transition
 * Copyright (c) 2006-2018, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "crypto/aes_wrap.h"
#include "crypto/sha384.h"
#include "crypto/sha512.h"
#include "crypto/random.h"
#include "common/ieee802_11_defs.h"
#include "common/ieee802_11_common.h"
#include "common/ocv.h"
#include "common/wpa_ctrl.h"
#include "drivers/driver.h"
#include "wpa.h"
#include "wpa_i.h"
#include "wpa_ie.h"
#include "pmksa_cache.h"

#ifdef CONFIG_IEEE80211R

#ifdef CONFIG_PASN
static void wpa_ft_pasn_store_r1kh(struct wpa_sm *sm, const u8 *bssid);
#else /* CONFIG_PASN */
static void wpa_ft_pasn_store_r1kh(struct wpa_sm *sm, const u8 *bssid)
{
}
#endif /* CONFIG_PASN */


int wpa_derive_ptk_ft(struct wpa_sm *sm, const unsigned char *src_addr,
		      const struct wpa_eapol_key *key, struct wpa_ptk *ptk)
{
	u8 ptk_name[WPA_PMK_NAME_LEN];
	const u8 *anonce = key->key_nonce;
	int use_sha384 = wpa_key_mgmt_sha384(sm->key_mgmt);
	const u8 *mpmk;
	size_t mpmk_len, kdk_len;
	int ret = 0;

	if (sm->xxkey_len > 0) {
		mpmk = sm->xxkey;
		mpmk_len = sm->xxkey_len;
	} else if (sm->cur_pmksa) {
		mpmk = sm->cur_pmksa->pmk;
		mpmk_len = sm->cur_pmksa->pmk_len;
	} else {
		wpa_printf(MSG_DEBUG, "FT: XXKey not available for key "
			   "derivation");
		return -1;
	}

	if (wpa_key_mgmt_sae_ext_key(sm->key_mgmt))
		sm->pmk_r0_len = mpmk_len;
	else
		sm->pmk_r0_len = use_sha384 ? SHA384_MAC_LEN : PMK_LEN;
	if (wpa_derive_pmk_r0(mpmk, mpmk_len, sm->ssid,
			      sm->ssid_len, sm->mobility_domain,
			      sm->r0kh_id, sm->r0kh_id_len, sm->own_addr,
			      sm->pmk_r0, sm->pmk_r0_name, sm->key_mgmt) < 0)
		return -1;
	sm->pmk_r1_len = sm->pmk_r0_len;
	if (wpa_derive_pmk_r1(sm->pmk_r0, sm->pmk_r0_len, sm->pmk_r0_name,
			      sm->r1kh_id, sm->own_addr, sm->pmk_r1,
			      sm->pmk_r1_name) < 0)
		return -1;

	wpa_ft_pasn_store_r1kh(sm, src_addr);

	if (sm->force_kdk_derivation ||
	    (sm->secure_ltf &&
	     ieee802_11_rsnx_capab(sm->ap_rsnxe, WLAN_RSNX_CAPAB_SECURE_LTF)))
		kdk_len = WPA_KDK_MAX_LEN;
	else
		kdk_len = 0;

	ret = wpa_pmk_r1_to_ptk(sm->pmk_r1, sm->pmk_r1_len, sm->snonce,
				anonce, sm->own_addr, wpa_sm_get_auth_addr(sm),
				sm->pmk_r1_name, ptk, ptk_name, sm->key_mgmt,
				sm->pairwise_cipher, kdk_len);
	if (ret) {
		wpa_printf(MSG_ERROR, "FT: PTK derivation failed");
		return ret;
	}

	os_memcpy(sm->key_mobility_domain, sm->mobility_domain,
		  MOBILITY_DOMAIN_ID_LEN);

#ifdef CONFIG_PASN
	if (sm->secure_ltf &&
	    ieee802_11_rsnx_capab(sm->ap_rsnxe, WLAN_RSNX_CAPAB_SECURE_LTF))
		ret = wpa_ltf_keyseed(ptk, sm->key_mgmt, sm->pairwise_cipher);
#endif /* CONFIG_PASN */

	return ret;
}


/**
 * wpa_sm_set_ft_params - Set FT (IEEE 802.11r) parameters
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @ies: Association Response IEs or %NULL to clear FT parameters
 * @ies_len: Length of ies buffer in octets
 * Returns: 0 on success, -1 on failure
 */
int wpa_sm_set_ft_params(struct wpa_sm *sm, const u8 *ies, size_t ies_len)
{
	struct wpa_ft_ies ft;

	if (sm == NULL)
		return 0;

	if (!get_ie(ies, ies_len, WLAN_EID_MOBILITY_DOMAIN)) {
		os_free(sm->assoc_resp_ies);
		sm->assoc_resp_ies = NULL;
		sm->assoc_resp_ies_len = 0;
		os_memset(sm->mobility_domain, 0, MOBILITY_DOMAIN_ID_LEN);
		os_memset(sm->r0kh_id, 0, FT_R0KH_ID_MAX_LEN);
		sm->r0kh_id_len = 0;
		os_memset(sm->r1kh_id, 0, FT_R1KH_ID_LEN);
		return 0;
	}

	if (wpa_ft_parse_ies(ies, ies_len, &ft, sm->key_mgmt, false) < 0)
		return -1;

	if (ft.mdie_len < MOBILITY_DOMAIN_ID_LEN + 1) {
		wpa_ft_parse_ies_free(&ft);
		return -1;
	}

	wpa_hexdump(MSG_DEBUG, "FT: Mobility domain",
		    ft.mdie, MOBILITY_DOMAIN_ID_LEN);
	os_memcpy(sm->mobility_domain, ft.mdie, MOBILITY_DOMAIN_ID_LEN);
	sm->mdie_ft_capab = ft.mdie[MOBILITY_DOMAIN_ID_LEN];
	wpa_printf(MSG_DEBUG, "FT: Capability and Policy: 0x%02x",
		   sm->mdie_ft_capab);

	if (ft.r0kh_id) {
		wpa_hexdump(MSG_DEBUG, "FT: R0KH-ID",
			    ft.r0kh_id, ft.r0kh_id_len);
		os_memcpy(sm->r0kh_id, ft.r0kh_id, ft.r0kh_id_len);
		sm->r0kh_id_len = ft.r0kh_id_len;
	} else {
		/* FIX: When should R0KH-ID be cleared? We need to keep the
		 * old R0KH-ID in order to be able to use this during FT. */
		/*
		 * os_memset(sm->r0kh_id, 0, FT_R0KH_ID_LEN);
		 * sm->r0kh_id_len = 0;
		 */
	}

	if (ft.r1kh_id) {
		wpa_hexdump(MSG_DEBUG, "FT: R1KH-ID",
			    ft.r1kh_id, FT_R1KH_ID_LEN);
		os_memcpy(sm->r1kh_id, ft.r1kh_id, FT_R1KH_ID_LEN);
	} else
		os_memset(sm->r1kh_id, 0, FT_R1KH_ID_LEN);

	os_free(sm->assoc_resp_ies);
	sm->assoc_resp_ies = os_malloc(ft.mdie_len + 2 + ft.ftie_len + 2);
	if (sm->assoc_resp_ies) {
		u8 *pos = sm->assoc_resp_ies;

		os_memcpy(pos, ft.mdie - 2, ft.mdie_len + 2);
		pos += ft.mdie_len + 2;

		if (ft.ftie) {
			os_memcpy(pos, ft.ftie - 2, ft.ftie_len + 2);
			pos += ft.ftie_len + 2;
		}
		sm->assoc_resp_ies_len = pos - sm->assoc_resp_ies;
		wpa_hexdump(MSG_DEBUG, "FT: Stored MDIE and FTIE from "
			    "(Re)Association Response",
			    sm->assoc_resp_ies, sm->assoc_resp_ies_len);
	}

	wpa_ft_parse_ies_free(&ft);
	return 0;
}


/**
 * wpa_ft_gen_req_ies - Generate FT (IEEE 802.11r) IEs for Auth/ReAssoc Request
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @len: Buffer for returning the length of the IEs
 * @anonce: ANonce or %NULL if not yet available
 * @pmk_name: PMKR0Name or PMKR1Name to be added into the RSN IE PMKID List
 * @kck: KCK for MIC or %NULL if no MIC is used
 * @kck_len: KCK length in octets
 * @target_ap: Target AP address
 * @ric_ies: Optional IE(s), e.g., WMM TSPEC(s), for RIC-Request or %NULL
 * @ric_ies_len: Length of ric_ies buffer in octets
 * @ap_mdie: Mobility Domain IE from the target AP
 * @omit_rsnxe: Whether RSNXE is omitted from Reassociation Request frame
 * Returns: Pointer to buffer with IEs or %NULL on failure
 *
 * Caller is responsible for freeing the returned buffer with os_free();
 */
static u8 * wpa_ft_gen_req_ies(struct wpa_sm *sm, size_t *len,
			       const u8 *anonce, const u8 *pmk_name,
			       const u8 *kck, size_t kck_len,
			       const u8 *target_ap,
			       const u8 *ric_ies, size_t ric_ies_len,
			       const u8 *ap_mdie, int omit_rsnxe)
{
	size_t buf_len;
	u8 *buf, *pos, *ftie_len, *ftie_pos, *fte_mic, *elem_count;
	struct rsn_mdie *mdie;
	struct rsn_ie_hdr *rsnie;
	int mdie_len;
	u8 rsnxe[10];
	size_t rsnxe_len;
	int rsnxe_used;
	int res;
	u8 mic_control;

	sm->ft_completed = 0;
	sm->ft_reassoc_completed = 0;

	buf_len = 2 + sizeof(struct rsn_mdie) + 2 +
		sizeof(struct rsn_ftie_sha512) +
		2 + sm->r0kh_id_len + ric_ies_len + 100;
	buf = os_zalloc(buf_len);
	if (buf == NULL)
		return NULL;
	pos = buf;

	/* RSNIE[PMKR0Name/PMKR1Name] */
	rsnie = (struct rsn_ie_hdr *) pos;
	rsnie->elem_id = WLAN_EID_RSN;
	WPA_PUT_LE16(rsnie->version, RSN_VERSION);
	pos = (u8 *) (rsnie + 1);

	/* Group Suite Selector */
	if (!wpa_cipher_valid_group(sm->group_cipher)) {
		wpa_printf(MSG_WARNING, "FT: Invalid group cipher (%d)",
			   sm->group_cipher);
		os_free(buf);
		return NULL;
	}
	RSN_SELECTOR_PUT(pos, wpa_cipher_to_suite(WPA_PROTO_RSN,
						  sm->group_cipher));
	pos += RSN_SELECTOR_LEN;

	/* Pairwise Suite Count */
	WPA_PUT_LE16(pos, 1);
	pos += 2;

	/* Pairwise Suite List */
	if (!wpa_cipher_valid_pairwise(sm->pairwise_cipher)) {
		wpa_printf(MSG_WARNING, "FT: Invalid pairwise cipher (%d)",
			   sm->pairwise_cipher);
		os_free(buf);
		return NULL;
	}
	RSN_SELECTOR_PUT(pos, wpa_cipher_to_suite(WPA_PROTO_RSN,
						  sm->pairwise_cipher));
	pos += RSN_SELECTOR_LEN;

	/* Authenticated Key Management Suite Count */
	WPA_PUT_LE16(pos, 1);
	pos += 2;

	/* Authenticated Key Management Suite List */
	if (sm->key_mgmt == WPA_KEY_MGMT_FT_IEEE8021X)
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_802_1X);
#ifdef CONFIG_SHA384
	else if (sm->key_mgmt == WPA_KEY_MGMT_FT_IEEE8021X_SHA384)
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_802_1X_SHA384);
#endif /* CONFIG_SHA384 */
	else if (sm->key_mgmt == WPA_KEY_MGMT_FT_PSK)
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_PSK);
	else if (sm->key_mgmt == WPA_KEY_MGMT_FT_SAE)
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_SAE);
	else if (sm->key_mgmt == WPA_KEY_MGMT_FT_SAE_EXT_KEY)
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_SAE_EXT_KEY);
#ifdef CONFIG_FILS
	else if (sm->key_mgmt == WPA_KEY_MGMT_FT_FILS_SHA256)
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_FILS_SHA256);
	else if (sm->key_mgmt == WPA_KEY_MGMT_FT_FILS_SHA384)
		RSN_SELECTOR_PUT(pos, RSN_AUTH_KEY_MGMT_FT_FILS_SHA384);
#endif /* CONFIG_FILS */
	else {
		wpa_printf(MSG_WARNING, "FT: Invalid key management type (%d)",
			   sm->key_mgmt);
		os_free(buf);
		return NULL;
	}
	pos += RSN_SELECTOR_LEN;

	/* RSN Capabilities */
	WPA_PUT_LE16(pos, rsn_supp_capab(sm));
	pos += 2;

	/* PMKID Count */
	WPA_PUT_LE16(pos, 1);
	pos += 2;

	/* PMKID List [PMKR0Name/PMKR1Name] */
	os_memcpy(pos, pmk_name, WPA_PMK_NAME_LEN);
	pos += WPA_PMK_NAME_LEN;

	/* Management Group Cipher Suite */
	switch (sm->mgmt_group_cipher) {
	case WPA_CIPHER_AES_128_CMAC:
		RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_AES_128_CMAC);
		pos += RSN_SELECTOR_LEN;
		break;
	case WPA_CIPHER_BIP_GMAC_128:
		RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_BIP_GMAC_128);
		pos += RSN_SELECTOR_LEN;
		break;
	case WPA_CIPHER_BIP_GMAC_256:
		RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_BIP_GMAC_256);
		pos += RSN_SELECTOR_LEN;
		break;
	case WPA_CIPHER_BIP_CMAC_256:
		RSN_SELECTOR_PUT(pos, RSN_CIPHER_SUITE_BIP_CMAC_256);
		pos += RSN_SELECTOR_LEN;
		break;
	}

	rsnie->len = (pos - (u8 *) rsnie) - 2;

	/* MDIE */
	mdie_len = wpa_ft_add_mdie(sm, pos, buf_len - (pos - buf), ap_mdie);
	if (mdie_len <= 0) {
		os_free(buf);
		return NULL;
	}
	mdie = (struct rsn_mdie *) (pos + 2);
	pos += mdie_len;

	/* FTIE[SNonce, [R1KH-ID,] R0KH-ID ] */
	ftie_pos = pos;
	*pos++ = WLAN_EID_FAST_BSS_TRANSITION;
	ftie_len = pos++;
	rsnxe_used = wpa_key_mgmt_sae(sm->key_mgmt) && anonce &&
		(sm->sae_pwe == SAE_PWE_HASH_TO_ELEMENT ||
		 sm->sae_pwe == SAE_PWE_BOTH);
#ifdef CONFIG_TESTING_OPTIONS
	if (anonce && sm->ft_rsnxe_used) {
		rsnxe_used = sm->ft_rsnxe_used == 1;
		wpa_printf(MSG_DEBUG, "TESTING: FT: Force RSNXE Used %d",
			   rsnxe_used);
	}
#endif /* CONFIG_TESTING_OPTIONS */
	mic_control = rsnxe_used ? FTE_MIC_CTRL_RSNXE_USED : 0;
	if (sm->key_mgmt == WPA_KEY_MGMT_FT_SAE_EXT_KEY &&
	    sm->pmk_r0_len == SHA512_MAC_LEN) {
		struct rsn_ftie_sha512 *ftie;

		ftie = (struct rsn_ftie_sha512 *) pos;
		mic_control |= FTE_MIC_LEN_32 << FTE_MIC_CTRL_MIC_LEN_SHIFT;
		ftie->mic_control[0] = mic_control;
		fte_mic = ftie->mic;
		elem_count = &ftie->mic_control[1];
		pos += sizeof(*ftie);
		os_memcpy(ftie->snonce, sm->snonce, WPA_NONCE_LEN);
		if (anonce)
			os_memcpy(ftie->anonce, anonce, WPA_NONCE_LEN);
	} else if ((sm->key_mgmt == WPA_KEY_MGMT_FT_SAE_EXT_KEY &&
		    sm->pmk_r0_len == SHA384_MAC_LEN) ||
		   wpa_key_mgmt_sha384(sm->key_mgmt)) {
		struct rsn_ftie_sha384 *ftie;

		ftie = (struct rsn_ftie_sha384 *) pos;
		mic_control |= FTE_MIC_LEN_24 << FTE_MIC_CTRL_MIC_LEN_SHIFT;
		ftie->mic_control[0] = mic_control;
		fte_mic = ftie->mic;
		elem_count = &ftie->mic_control[1];
		pos += sizeof(*ftie);
		os_memcpy(ftie->snonce, sm->snonce, WPA_NONCE_LEN);
		if (anonce)
			os_memcpy(ftie->anonce, anonce, WPA_NONCE_LEN);
	} else {
		struct rsn_ftie *ftie;

		ftie = (struct rsn_ftie *) pos;
		mic_control |= FTE_MIC_LEN_16 << FTE_MIC_CTRL_MIC_LEN_SHIFT;
		ftie->mic_control[0] = mic_control;
		fte_mic = ftie->mic;
		elem_count = &ftie->mic_control[1];
		pos += sizeof(*ftie);
		os_memcpy(ftie->snonce, sm->snonce, WPA_NONCE_LEN);
		if (anonce)
			os_memcpy(ftie->anonce, anonce, WPA_NONCE_LEN);
	}
	if (kck) {
		/* R1KH-ID sub-element in third FT message */
		*pos++ = FTIE_SUBELEM_R1KH_ID;
		*pos++ = FT_R1KH_ID_LEN;
		os_memcpy(pos, sm->r1kh_id, FT_R1KH_ID_LEN);
		pos += FT_R1KH_ID_LEN;
	}
	/* R0KH-ID sub-element */
	*pos++ = FTIE_SUBELEM_R0KH_ID;
	*pos++ = sm->r0kh_id_len;
	os_memcpy(pos, sm->r0kh_id, sm->r0kh_id_len);
	pos += sm->r0kh_id_len;
#ifdef CONFIG_OCV
	if (kck && wpa_sm_ocv_enabled(sm)) {
		/* OCI sub-element in the third FT message */
		struct wpa_channel_info ci;

		if (wpa_sm_channel_info(sm, &ci) != 0) {
			wpa_printf(MSG_WARNING,
				   "Failed to get channel info for OCI element in FTE");
			os_free(buf);
			return NULL;
		}
#ifdef CONFIG_TESTING_OPTIONS
		if (sm->oci_freq_override_ft_assoc) {
			wpa_printf(MSG_INFO,
				   "TEST: Override OCI KDE frequency %d -> %d MHz",
				   ci.frequency, sm->oci_freq_override_ft_assoc);
			ci.frequency = sm->oci_freq_override_ft_assoc;
		}
#endif /* CONFIG_TESTING_OPTIONS */

		*pos++ = FTIE_SUBELEM_OCI;
		*pos++ = OCV_OCI_LEN;
		if (ocv_insert_oci(&ci, &pos) < 0) {
			os_free(buf);
			return NULL;
		}
	}
#endif /* CONFIG_OCV */
	*ftie_len = pos - ftie_len - 1;

	if (ric_ies) {
		/* RIC Request */
		os_memcpy(pos, ric_ies, ric_ies_len);
		pos += ric_ies_len;
	}

	if (omit_rsnxe) {
		rsnxe_len = 0;
	} else {
		res = wpa_gen_rsnxe(sm, rsnxe, sizeof(rsnxe));
		if (res < 0) {
			os_free(buf);
			return NULL;
		}
		rsnxe_len = res;
	}

	if (kck) {
		/*
		 * IEEE Std 802.11r-2008, 11A.8.4
		 * MIC shall be calculated over:
		 * non-AP STA MAC address
		 * Target AP MAC address
		 * Transaction seq number (5 for ReassocReq, 3 otherwise)
		 * RSN IE
		 * MDIE
		 * FTIE (with MIC field set to 0)
		 * RIC-Request (if present)
		 * RSNXE (if present)
		 */
		/* Information element count */
		*elem_count = 3 + ieee802_11_ie_count(ric_ies, ric_ies_len);
		if (rsnxe_len)
			*elem_count += 1;
		if (wpa_ft_mic(sm->key_mgmt, kck, kck_len,
			       sm->own_addr, target_ap, 5,
			       ((u8 *) mdie) - 2, 2 + sizeof(*mdie),
			       ftie_pos, 2 + *ftie_len,
			       (u8 *) rsnie, 2 + rsnie->len, ric_ies,
			       ric_ies_len, rsnxe_len ? rsnxe : NULL, rsnxe_len,
			       NULL,
			       fte_mic) < 0) {
			wpa_printf(MSG_INFO, "FT: Failed to calculate MIC");
			os_free(buf);
			return NULL;
		}
	}

	*len = pos - buf;

	return buf;
}


static int wpa_ft_install_ptk(struct wpa_sm *sm, const u8 *bssid)
{
	int keylen;
	enum wpa_alg alg;
	u8 null_rsc[6] = { 0, 0, 0, 0, 0, 0 };

	wpa_printf(MSG_DEBUG, "FT: Installing PTK to the driver.");

	if (!wpa_cipher_valid_pairwise(sm->pairwise_cipher)) {
		wpa_printf(MSG_WARNING, "FT: Unsupported pairwise cipher %d",
			   sm->pairwise_cipher);
		return -1;
	}

	alg = wpa_cipher_to_alg(sm->pairwise_cipher);
	keylen = wpa_cipher_key_len(sm->pairwise_cipher);

	/* TODO: AP MLD address for MLO */
	if (wpa_sm_set_key(sm, -1, alg, bssid, 0, 1, null_rsc, sizeof(null_rsc),
			   (u8 *) sm->ptk.tk, keylen,
			   KEY_FLAG_PAIRWISE_RX_TX) < 0) {
		wpa_printf(MSG_WARNING, "FT: Failed to set PTK to the driver");
		return -1;
	}
	sm->tk_set = true;

	wpa_sm_store_ptk(sm, bssid, sm->pairwise_cipher,
			 sm->dot11RSNAConfigPMKLifetime, &sm->ptk);
	return 0;
}


/**
 * wpa_ft_prepare_auth_request - Generate over-the-air auth request
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @mdie: Target AP MDIE
 * Returns: 0 on success, -1 on failure
 */
int wpa_ft_prepare_auth_request(struct wpa_sm *sm, const u8 *mdie)
{
	u8 *ft_ies;
	size_t ft_ies_len;

	/* Generate a new SNonce */
	if (random_get_bytes(sm->snonce, WPA_NONCE_LEN)) {
		wpa_printf(MSG_INFO, "FT: Failed to generate a new SNonce");
		return -1;
	}

	ft_ies = wpa_ft_gen_req_ies(sm, &ft_ies_len, NULL, sm->pmk_r0_name,
				    NULL, 0, sm->bssid, NULL, 0, mdie, 0);
	if (ft_ies) {
		wpa_sm_update_ft_ies(sm, sm->mobility_domain,
				     ft_ies, ft_ies_len);
		os_free(ft_ies);
	}

	return 0;
}


int wpa_ft_add_mdie(struct wpa_sm *sm, u8 *buf, size_t buf_len,
		    const u8 *ap_mdie)
{
	u8 *pos = buf;
	struct rsn_mdie *mdie;

	if (buf_len < 2 + sizeof(*mdie)) {
		wpa_printf(MSG_INFO,
			   "FT: Failed to add MDIE: short buffer, length=%zu",
			   buf_len);
		return 0;
	}

	*pos++ = WLAN_EID_MOBILITY_DOMAIN;
	*pos++ = sizeof(*mdie);
	mdie = (struct rsn_mdie *) pos;
	os_memcpy(mdie->mobility_domain, sm->mobility_domain,
		  MOBILITY_DOMAIN_ID_LEN);
	mdie->ft_capab = ap_mdie && ap_mdie[1] >= 3 ? ap_mdie[4] :
		sm->mdie_ft_capab;

	return 2 + sizeof(*mdie);
}


const u8 * wpa_sm_get_ft_md(struct wpa_sm *sm)
{
	return sm->mobility_domain;
}


int wpa_ft_process_response(struct wpa_sm *sm, const u8 *ies, size_t ies_len,
			    int ft_action, const u8 *target_ap,
			    const u8 *ric_ies, size_t ric_ies_len)
{
	u8 *ft_ies;
	size_t ft_ies_len;
	struct wpa_ft_ies parse;
	struct rsn_mdie *mdie;
	u8 ptk_name[WPA_PMK_NAME_LEN];
	int ret = -1, res;
	const u8 *bssid;
	const u8 *kck;
	size_t kck_len, kdk_len;

	os_memset(&parse, 0, sizeof(parse));

	wpa_hexdump(MSG_DEBUG, "FT: Response IEs", ies, ies_len);
	wpa_hexdump(MSG_DEBUG, "FT: RIC IEs", ric_ies, ric_ies_len);

	if (ft_action) {
		if (!sm->over_the_ds_in_progress) {
			wpa_printf(MSG_DEBUG, "FT: No over-the-DS in progress "
				   "- drop FT Action Response");
			goto fail;
		}

		if (!ether_addr_equal(target_ap, sm->target_ap)) {
			wpa_printf(MSG_DEBUG, "FT: No over-the-DS in progress "
				   "with this Target AP - drop FT Action "
				   "Response");
			goto fail;
		}
	}

	if (!wpa_key_mgmt_ft(sm->key_mgmt)) {
		wpa_printf(MSG_DEBUG, "FT: Reject FT IEs since FT is not "
			   "enabled for this connection");
		goto fail;
	}

	if (wpa_ft_parse_ies(ies, ies_len, &parse, sm->key_mgmt,
			     !ft_action) < 0) {
		wpa_printf(MSG_DEBUG, "FT: Failed to parse IEs");
		goto fail;
	}

	mdie = (struct rsn_mdie *) parse.mdie;
	if (mdie == NULL || parse.mdie_len < sizeof(*mdie) ||
	    os_memcmp(mdie->mobility_domain, sm->mobility_domain,
		      MOBILITY_DOMAIN_ID_LEN) != 0) {
		wpa_printf(MSG_DEBUG, "FT: Invalid MDIE");
		goto fail;
	}

	if (!parse.ftie || !parse.fte_anonce || !parse.fte_snonce) {
		wpa_printf(MSG_DEBUG, "FT: Invalid FTE");
		goto fail;
	}

	if (os_memcmp(parse.fte_snonce, sm->snonce, WPA_NONCE_LEN) != 0) {
		wpa_printf(MSG_DEBUG, "FT: SNonce mismatch in FTIE");
		wpa_hexdump(MSG_DEBUG, "FT: Received SNonce",
			    parse.fte_snonce, WPA_NONCE_LEN);
		wpa_hexdump(MSG_DEBUG, "FT: Expected SNonce",
			    sm->snonce, WPA_NONCE_LEN);
		goto fail;
	}

	if (parse.r0kh_id == NULL) {
		wpa_printf(MSG_DEBUG, "FT: No R0KH-ID subelem in FTIE");
		goto fail;
	}

	if (parse.r0kh_id_len != sm->r0kh_id_len ||
	    os_memcmp_const(parse.r0kh_id, sm->r0kh_id, parse.r0kh_id_len) != 0)
	{
		wpa_printf(MSG_DEBUG, "FT: R0KH-ID in FTIE did not match with "
			   "the current R0KH-ID");
		wpa_hexdump(MSG_DEBUG, "FT: R0KH-ID in FTIE",
			    parse.r0kh_id, parse.r0kh_id_len);
		wpa_hexdump(MSG_DEBUG, "FT: The current R0KH-ID",
			    sm->r0kh_id, sm->r0kh_id_len);
		goto fail;
	}

	if (parse.r1kh_id == NULL) {
		wpa_printf(MSG_DEBUG, "FT: No R1KH-ID subelem in FTIE");
		goto fail;
	}

	if (parse.rsn_pmkid == NULL ||
	    os_memcmp_const(parse.rsn_pmkid, sm->pmk_r0_name, WPA_PMK_NAME_LEN))
	{
		wpa_printf(MSG_DEBUG, "FT: No matching PMKR0Name (PMKID) in "
			   "RSNIE");
		goto fail;
	}

	if (sm->mfp == 2 && !(parse.rsn_capab & WPA_CAPABILITY_MFPC)) {
		wpa_printf(MSG_INFO,
			   "FT: Target AP does not support PMF, but local configuration requires that");
		goto fail;
	}

	os_memcpy(sm->r1kh_id, parse.r1kh_id, FT_R1KH_ID_LEN);
	wpa_hexdump(MSG_DEBUG, "FT: R1KH-ID", sm->r1kh_id, FT_R1KH_ID_LEN);
	wpa_hexdump(MSG_DEBUG, "FT: SNonce", sm->snonce, WPA_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "FT: ANonce", parse.fte_anonce, WPA_NONCE_LEN);
	os_memcpy(sm->anonce, parse.fte_anonce, WPA_NONCE_LEN);
	if (wpa_derive_pmk_r1(sm->pmk_r0, sm->pmk_r0_len, sm->pmk_r0_name,
			      sm->r1kh_id, sm->own_addr, sm->pmk_r1,
			      sm->pmk_r1_name) < 0)
		goto fail;
	sm->pmk_r1_len = sm->pmk_r0_len;

	bssid = target_ap;

	wpa_ft_pasn_store_r1kh(sm, bssid);

	if (sm->force_kdk_derivation ||
	    (sm->secure_ltf &&
	     ieee802_11_rsnx_capab(sm->ap_rsnxe, WLAN_RSNX_CAPAB_SECURE_LTF)))
		kdk_len = WPA_KDK_MAX_LEN;
	else
		kdk_len = 0;

	/* TODO: AP MLD address for MLO */
	if (wpa_pmk_r1_to_ptk(sm->pmk_r1, sm->pmk_r1_len, sm->snonce,
			      parse.fte_anonce, sm->own_addr, bssid,
			      sm->pmk_r1_name, &sm->ptk, ptk_name, sm->key_mgmt,
			      sm->pairwise_cipher,
			      kdk_len) < 0)
		goto fail;

	os_memcpy(sm->key_mobility_domain, sm->mobility_domain,
		  MOBILITY_DOMAIN_ID_LEN);

#ifdef CONFIG_PASN
	if (sm->secure_ltf &&
	    ieee802_11_rsnx_capab(sm->ap_rsnxe, WLAN_RSNX_CAPAB_SECURE_LTF) &&
	    wpa_ltf_keyseed(&sm->ptk, sm->key_mgmt, sm->pairwise_cipher)) {
		wpa_printf(MSG_DEBUG, "FT: Failed to derive LTF keyseed");
		goto fail;
	}
#endif /* CONFIG_PASN */

	if (wpa_key_mgmt_fils(sm->key_mgmt)) {
		kck = sm->ptk.kck2;
		kck_len = sm->ptk.kck2_len;
	} else {
		kck = sm->ptk.kck;
		kck_len = sm->ptk.kck_len;
	}
	ft_ies = wpa_ft_gen_req_ies(sm, &ft_ies_len, parse.fte_anonce,
				    sm->pmk_r1_name,
				    kck, kck_len, bssid,
				    ric_ies, ric_ies_len,
				    parse.mdie ? parse.mdie - 2 : NULL,
				    !sm->ap_rsnxe);
	if (ft_ies) {
		wpa_sm_update_ft_ies(sm, sm->mobility_domain,
				     ft_ies, ft_ies_len);
		os_free(ft_ies);
	}

	wpa_sm_mark_authenticated(sm, bssid);
	res = wpa_ft_install_ptk(sm, bssid);
	if (res) {
		/*
		 * Some drivers do not support key configuration when we are
		 * not associated with the target AP. Work around this by
		 * trying again after the following reassociation gets
		 * completed.
		 */
		wpa_printf(MSG_DEBUG, "FT: Failed to set PTK prior to "
			   "association - try again after reassociation");
		sm->set_ptk_after_assoc = 1;
	} else
		sm->set_ptk_after_assoc = 0;

	sm->ft_completed = 1;
	if (ft_action) {
		/*
		 * The caller is expected trigger re-association with the
		 * Target AP.
		 */
		os_memcpy(sm->bssid, target_ap, ETH_ALEN);
	}

	ret = 0;
fail:
	wpa_ft_parse_ies_free(&parse);
	return ret;
}


int wpa_ft_is_completed(struct wpa_sm *sm)
{
	if (sm == NULL)
		return 0;

	if (!wpa_key_mgmt_ft(sm->key_mgmt))
		return 0;

	return sm->ft_completed;
}


void wpa_reset_ft_completed(struct wpa_sm *sm)
{
	if (sm != NULL)
		sm->ft_completed = 0;
}


static int wpa_ft_process_gtk_subelem(struct wpa_sm *sm, const u8 *gtk_elem,
				      size_t gtk_elem_len)
{
	u8 gtk[32];
	int keyidx;
	enum wpa_alg alg;
	size_t gtk_len, keylen, rsc_len;
	const u8 *kek;
	size_t kek_len;

	if (wpa_key_mgmt_fils(sm->key_mgmt)) {
		kek = sm->ptk.kek2;
		kek_len = sm->ptk.kek2_len;
	} else {
		kek = sm->ptk.kek;
		kek_len = sm->ptk.kek_len;
	}

	if (gtk_elem == NULL) {
		wpa_printf(MSG_DEBUG, "FT: No GTK included in FTIE");
		return 0;
	}

	wpa_hexdump_key(MSG_DEBUG, "FT: Received GTK in Reassoc Resp",
			gtk_elem, gtk_elem_len);

	if (gtk_elem_len < 11 + 24 || (gtk_elem_len - 11) % 8 ||
	    gtk_elem_len - 19 > sizeof(gtk)) {
		wpa_printf(MSG_DEBUG, "FT: Invalid GTK sub-elem "
			   "length %lu", (unsigned long) gtk_elem_len);
		return -1;
	}
	gtk_len = gtk_elem_len - 19;
	if (aes_unwrap(kek, kek_len, gtk_len / 8, gtk_elem + 11, gtk)) {
		wpa_printf(MSG_WARNING, "FT: AES unwrap failed - could not "
			   "decrypt GTK");
		return -1;
	}

	keylen = wpa_cipher_key_len(sm->group_cipher);
	rsc_len = wpa_cipher_rsc_len(sm->group_cipher);
	alg = wpa_cipher_to_alg(sm->group_cipher);
	if (alg == WPA_ALG_NONE) {
		wpa_printf(MSG_WARNING, "WPA: Unsupported Group Cipher %d",
			   sm->group_cipher);
		return -1;
	}

	if (gtk_len < keylen) {
		wpa_printf(MSG_DEBUG, "FT: Too short GTK in FTIE");
		return -1;
	}

	/* Key Info[2] | Key Length[1] | RSC[8] | Key[5..32]. */

	keyidx = WPA_GET_LE16(gtk_elem) & 0x03;

	if (gtk_elem[2] != keylen) {
		wpa_printf(MSG_DEBUG, "FT: GTK length mismatch: received %d "
			   "negotiated %lu",
			   gtk_elem[2], (unsigned long) keylen);
		return -1;
	}

	wpa_hexdump_key(MSG_DEBUG, "FT: GTK from Reassoc Resp", gtk, keylen);
	if (sm->group_cipher == WPA_CIPHER_TKIP) {
		/* Swap Tx/Rx keys for Michael MIC */
		u8 tmp[8];
		os_memcpy(tmp, gtk + 16, 8);
		os_memcpy(gtk + 16, gtk + 24, 8);
		os_memcpy(gtk + 24, tmp, 8);
	}
	if (wpa_sm_set_key(sm, -1, alg, broadcast_ether_addr, keyidx, 0,
			   gtk_elem + 3, rsc_len, gtk, keylen,
			   KEY_FLAG_GROUP_RX) < 0) {
		wpa_printf(MSG_WARNING, "WPA: Failed to set GTK to the "
			   "driver.");
		return -1;
	}

	return 0;
}


static int wpa_ft_process_igtk_subelem(struct wpa_sm *sm, const u8 *igtk_elem,
				       size_t igtk_elem_len)
{
	u8 igtk[WPA_IGTK_MAX_LEN];
	size_t igtk_len;
	u16 keyidx;
	const u8 *kek;
	size_t kek_len;

	if (wpa_key_mgmt_fils(sm->key_mgmt)) {
		kek = sm->ptk.kek2;
		kek_len = sm->ptk.kek2_len;
	} else {
		kek = sm->ptk.kek;
		kek_len = sm->ptk.kek_len;
	}

	if (sm->mgmt_group_cipher != WPA_CIPHER_AES_128_CMAC &&
	    sm->mgmt_group_cipher != WPA_CIPHER_BIP_GMAC_128 &&
	    sm->mgmt_group_cipher != WPA_CIPHER_BIP_GMAC_256 &&
	    sm->mgmt_group_cipher != WPA_CIPHER_BIP_CMAC_256)
		return 0;

	if (igtk_elem == NULL) {
		wpa_printf(MSG_DEBUG, "FT: No IGTK included in FTIE");
		return 0;
	}

	wpa_hexdump_key(MSG_DEBUG, "FT: Received IGTK in Reassoc Resp",
			igtk_elem, igtk_elem_len);

	igtk_len = wpa_cipher_key_len(sm->mgmt_group_cipher);
	if (igtk_elem_len != 2 + 6 + 1 + igtk_len + 8) {
		wpa_printf(MSG_DEBUG, "FT: Invalid IGTK sub-elem "
			   "length %lu", (unsigned long) igtk_elem_len);
		return -1;
	}
	if (igtk_elem[8] != igtk_len) {
		wpa_printf(MSG_DEBUG, "FT: Invalid IGTK sub-elem Key Length "
			   "%d", igtk_elem[8]);
		return -1;
	}

	if (aes_unwrap(kek, kek_len, igtk_len / 8, igtk_elem + 9, igtk)) {
		wpa_printf(MSG_WARNING, "FT: AES unwrap failed - could not "
			   "decrypt IGTK");
		return -1;
	}

	/* KeyID[2] | IPN[6] | Key Length[1] | Key[16+8] */

	keyidx = WPA_GET_LE16(igtk_elem);

	wpa_hexdump_key(MSG_DEBUG, "FT: IGTK from Reassoc Resp", igtk,
			igtk_len);
	if (wpa_sm_set_key(sm, -1, wpa_cipher_to_alg(sm->mgmt_group_cipher),
			   broadcast_ether_addr, keyidx, 0,
			   igtk_elem + 2, 6, igtk, igtk_len,
			   KEY_FLAG_GROUP_RX) < 0) {
		wpa_printf(MSG_WARNING, "WPA: Failed to set IGTK to the "
			   "driver.");
		forced_memzero(igtk, sizeof(igtk));
		return -1;
	}
	forced_memzero(igtk, sizeof(igtk));

	return 0;
}


static int wpa_ft_process_bigtk_subelem(struct wpa_sm *sm, const u8 *bigtk_elem,
				       size_t bigtk_elem_len)
{
	u8 bigtk[WPA_BIGTK_MAX_LEN];
	size_t bigtk_len;
	u16 keyidx;
	const u8 *kek;
	size_t kek_len;

	if (!sm->beacon_prot || !bigtk_elem ||
	    (sm->mgmt_group_cipher != WPA_CIPHER_AES_128_CMAC &&
	     sm->mgmt_group_cipher != WPA_CIPHER_BIP_GMAC_128 &&
	     sm->mgmt_group_cipher != WPA_CIPHER_BIP_GMAC_256 &&
	     sm->mgmt_group_cipher != WPA_CIPHER_BIP_CMAC_256))
		return 0;

	if (wpa_key_mgmt_fils(sm->key_mgmt)) {
		kek = sm->ptk.kek2;
		kek_len = sm->ptk.kek2_len;
	} else {
		kek = sm->ptk.kek;
		kek_len = sm->ptk.kek_len;
	}

	wpa_hexdump_key(MSG_DEBUG, "FT: Received BIGTK in Reassoc Resp",
			bigtk_elem, bigtk_elem_len);

	bigtk_len = wpa_cipher_key_len(sm->mgmt_group_cipher);
	if (bigtk_elem_len != 2 + 6 + 1 + bigtk_len + 8) {
		wpa_printf(MSG_DEBUG,
			   "FT: Invalid BIGTK sub-elem length %lu",
			   (unsigned long) bigtk_elem_len);
		return -1;
	}
	if (bigtk_elem[8] != bigtk_len) {
		wpa_printf(MSG_DEBUG,
			   "FT: Invalid BIGTK sub-elem Key Length %d",
			   bigtk_elem[8]);
		return -1;
	}

	if (aes_unwrap(kek, kek_len, bigtk_len / 8, bigtk_elem + 9, bigtk)) {
		wpa_printf(MSG_WARNING,
			   "FT: AES unwrap failed - could not decrypt BIGTK");
		return -1;
	}

	/* KeyID[2] | IPN[6] | Key Length[1] | Key[16+8] */

	keyidx = WPA_GET_LE16(bigtk_elem);

	wpa_hexdump_key(MSG_DEBUG, "FT: BIGTK from Reassoc Resp", bigtk,
			bigtk_len);
	if (wpa_sm_set_key(sm, -1, wpa_cipher_to_alg(sm->mgmt_group_cipher),
			   broadcast_ether_addr, keyidx, 0,
			   bigtk_elem + 2, 6, bigtk, bigtk_len,
			   KEY_FLAG_GROUP_RX) < 0) {
		wpa_printf(MSG_WARNING,
			   "WPA: Failed to set BIGTK to the driver");
		forced_memzero(bigtk, sizeof(bigtk));
		return -1;
	}
	forced_memzero(bigtk, sizeof(bigtk));

	return 0;
}


int wpa_ft_validate_reassoc_resp(struct wpa_sm *sm, const u8 *ies,
				 size_t ies_len, const u8 *src_addr)
{
	struct wpa_ft_ies parse;
	struct rsn_mdie *mdie;
	unsigned int count;
	u8 mic[WPA_EAPOL_KEY_MIC_MAX_LEN];
	const u8 *kck;
	size_t kck_len;
	int own_rsnxe_used;
	size_t mic_len;
	int ret = -1;

	os_memset(&parse, 0, sizeof(parse));

	wpa_hexdump(MSG_DEBUG, "FT: Response IEs", ies, ies_len);

	if (!wpa_key_mgmt_ft(sm->key_mgmt)) {
		wpa_printf(MSG_DEBUG, "FT: Reject FT IEs since FT is not "
			   "enabled for this connection");
		goto fail;
	}

	if (sm->ft_reassoc_completed) {
		wpa_printf(MSG_DEBUG, "FT: Reassociation has already been completed for this FT protocol instance - ignore unexpected retransmission");
		return 0;
	}

	if (wpa_ft_parse_ies(ies, ies_len, &parse, sm->key_mgmt, true) < 0) {
		wpa_printf(MSG_DEBUG, "FT: Failed to parse IEs");
		goto fail;
	}

	mdie = (struct rsn_mdie *) parse.mdie;
	if (mdie == NULL || parse.mdie_len < sizeof(*mdie) ||
	    os_memcmp(mdie->mobility_domain, sm->mobility_domain,
		      MOBILITY_DOMAIN_ID_LEN) != 0) {
		wpa_printf(MSG_DEBUG, "FT: Invalid MDIE");
		goto fail;
	}

	if (sm->key_mgmt == WPA_KEY_MGMT_FT_SAE_EXT_KEY &&
	    sm->pmk_r1_len == SHA512_MAC_LEN)
		mic_len = 32;
	else if ((sm->key_mgmt == WPA_KEY_MGMT_FT_SAE_EXT_KEY &&
		  sm->pmk_r1_len == SHA384_MAC_LEN) ||
		 wpa_key_mgmt_sha384(sm->key_mgmt))
		mic_len = 24;
	else
		mic_len = 16;

	if (!parse.ftie || !parse.fte_anonce || !parse.fte_snonce ||
	    parse.fte_mic_len != mic_len) {
		wpa_printf(MSG_DEBUG,
			   "FT: Invalid FTE (fte_mic_len=%zu mic_len=%zu)",
			   parse.fte_mic_len, mic_len);
		goto fail;
	}

	if (os_memcmp(parse.fte_snonce, sm->snonce, WPA_NONCE_LEN) != 0) {
		wpa_printf(MSG_DEBUG, "FT: SNonce mismatch in FTIE");
		wpa_hexdump(MSG_DEBUG, "FT: Received SNonce",
			    parse.fte_snonce, WPA_NONCE_LEN);
		wpa_hexdump(MSG_DEBUG, "FT: Expected SNonce",
			    sm->snonce, WPA_NONCE_LEN);
		goto fail;
	}

	if (os_memcmp(parse.fte_anonce, sm->anonce, WPA_NONCE_LEN) != 0) {
		wpa_printf(MSG_DEBUG, "FT: ANonce mismatch in FTIE");
		wpa_hexdump(MSG_DEBUG, "FT: Received ANonce",
			    parse.fte_anonce, WPA_NONCE_LEN);
		wpa_hexdump(MSG_DEBUG, "FT: Expected ANonce",
			    sm->anonce, WPA_NONCE_LEN);
		goto fail;
	}

	if (parse.r0kh_id == NULL) {
		wpa_printf(MSG_DEBUG, "FT: No R0KH-ID subelem in FTIE");
		goto fail;
	}

	if (parse.r0kh_id_len != sm->r0kh_id_len ||
	    os_memcmp_const(parse.r0kh_id, sm->r0kh_id, parse.r0kh_id_len) != 0)
	{
		wpa_printf(MSG_DEBUG, "FT: R0KH-ID in FTIE did not match with "
			   "the current R0KH-ID");
		wpa_hexdump(MSG_DEBUG, "FT: R0KH-ID in FTIE",
			    parse.r0kh_id, parse.r0kh_id_len);
		wpa_hexdump(MSG_DEBUG, "FT: The current R0KH-ID",
			    sm->r0kh_id, sm->r0kh_id_len);
		goto fail;
	}

	if (parse.r1kh_id == NULL) {
		wpa_printf(MSG_DEBUG, "FT: No R1KH-ID subelem in FTIE");
		goto fail;
	}

	if (os_memcmp_const(parse.r1kh_id, sm->r1kh_id, FT_R1KH_ID_LEN) != 0) {
		wpa_printf(MSG_DEBUG, "FT: Unknown R1KH-ID used in "
			   "ReassocResp");
		goto fail;
	}

	if (parse.rsn_pmkid == NULL ||
	    os_memcmp_const(parse.rsn_pmkid, sm->pmk_r1_name, WPA_PMK_NAME_LEN))
	{
		wpa_printf(MSG_DEBUG, "FT: No matching PMKR1Name (PMKID) in "
			   "RSNIE (pmkid=%d)", !!parse.rsn_pmkid);
		goto fail;
	}

	count = 3;
	if (parse.ric)
		count += ieee802_11_ie_count(parse.ric, parse.ric_len);
	if (parse.rsnxe)
		count++;
	if (parse.fte_elem_count != count) {
		wpa_printf(MSG_DEBUG, "FT: Unexpected IE count in MIC "
			   "Control: received %u expected %u",
			   parse.fte_elem_count, count);
		goto fail;
	}

	if (wpa_key_mgmt_fils(sm->key_mgmt)) {
		kck = sm->ptk.kck2;
		kck_len = sm->ptk.kck2_len;
	} else {
		kck = sm->ptk.kck;
		kck_len = sm->ptk.kck_len;
	}

	if (wpa_ft_mic(sm->key_mgmt, kck, kck_len, sm->own_addr, src_addr, 6,
		       parse.mdie - 2, parse.mdie_len + 2,
		       parse.ftie - 2, parse.ftie_len + 2,
		       parse.rsn - 2, parse.rsn_len + 2,
		       parse.ric, parse.ric_len,
		       parse.rsnxe ? parse.rsnxe - 2 : NULL,
		       parse.rsnxe ? parse.rsnxe_len + 2 : 0,
		       NULL,
		       mic) < 0) {
		wpa_printf(MSG_DEBUG, "FT: Failed to calculate MIC");
		goto fail;
	}

	if (os_memcmp_const(mic, parse.fte_mic, mic_len) != 0) {
		wpa_printf(MSG_DEBUG, "FT: Invalid MIC in FTIE");
		wpa_hexdump(MSG_MSGDUMP, "FT: Received MIC",
			    parse.fte_mic, mic_len);
		wpa_hexdump(MSG_MSGDUMP, "FT: Calculated MIC", mic, mic_len);
		goto fail;
	}

	if (parse.fte_rsnxe_used && !sm->ap_rsnxe) {
		wpa_printf(MSG_INFO,
			   "FT: FTE indicated that AP uses RSNXE, but RSNXE was not included in Beacon/Probe Response frames");
		goto fail;
	}

	if (!sm->ap_rsn_ie) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"FT: No RSNE for this AP known - trying to get from scan results");
		if (wpa_sm_get_beacon_ie(sm) < 0) {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"FT: Could not find AP from the scan results");
			goto fail;
		}
		wpa_msg(sm->ctx->msg_ctx, MSG_DEBUG,
			"FT: Found the current AP from updated scan results");
	}

	if (sm->ap_rsn_ie &&
	    wpa_compare_rsn_ie(wpa_key_mgmt_ft(sm->key_mgmt),
			       sm->ap_rsn_ie, sm->ap_rsn_ie_len,
			       parse.rsn - 2, parse.rsn_len + 2)) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"FT: RSNE mismatch between Beacon/ProbeResp and FT protocol Reassociation Response frame");
		wpa_hexdump(MSG_INFO, "RSNE in Beacon/ProbeResp",
			    sm->ap_rsn_ie, sm->ap_rsn_ie_len);
		wpa_hexdump(MSG_INFO,
			    "RSNE in FT protocol Reassociation Response frame",
			    parse.rsn ? parse.rsn - 2 : NULL,
			    parse.rsn ? parse.rsn_len + 2 : 0);
		goto fail;
	}

	own_rsnxe_used = wpa_key_mgmt_sae(sm->key_mgmt) &&
		(sm->sae_pwe == SAE_PWE_HASH_TO_ELEMENT ||
		 sm->sae_pwe == SAE_PWE_BOTH);
	if ((sm->ap_rsnxe && !parse.rsnxe && own_rsnxe_used) ||
	    (!sm->ap_rsnxe && parse.rsnxe) ||
	    (sm->ap_rsnxe && parse.rsnxe &&
	     (sm->ap_rsnxe_len != 2 + parse.rsnxe_len ||
	      os_memcmp(sm->ap_rsnxe, parse.rsnxe - 2,
			sm->ap_rsnxe_len) != 0))) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"FT: RSNXE mismatch between Beacon/ProbeResp and FT protocol Reassociation Response frame");
		wpa_hexdump(MSG_INFO, "RSNXE in Beacon/ProbeResp",
			    sm->ap_rsnxe, sm->ap_rsnxe_len);
		wpa_hexdump(MSG_INFO,
			    "RSNXE in FT protocol Reassociation Response frame",
			    parse.rsnxe ? parse.rsnxe - 2 : NULL,
			    parse.rsnxe ? parse.rsnxe_len + 2 : 0);
		goto fail;
	}

#ifdef CONFIG_OCV
	if (wpa_sm_ocv_enabled(sm)) {
		struct wpa_channel_info ci;

		if (wpa_sm_channel_info(sm, &ci) != 0) {
			wpa_printf(MSG_WARNING,
				   "Failed to get channel info to validate received OCI in (Re)Assoc Response");
			goto fail;
		}

		if (ocv_verify_tx_params(parse.oci, parse.oci_len, &ci,
					 channel_width_to_int(ci.chanwidth),
					 ci.seg1_idx) != OCI_SUCCESS) {
			wpa_msg(sm->ctx->msg_ctx, MSG_INFO, OCV_FAILURE
				"addr=" MACSTR " frame=ft-assoc error=%s",
				MAC2STR(src_addr), ocv_errorstr);
			goto fail;
		}
	}
#endif /* CONFIG_OCV */

	sm->ft_reassoc_completed = 1;

	if (wpa_ft_process_gtk_subelem(sm, parse.gtk, parse.gtk_len) < 0 ||
	    wpa_ft_process_igtk_subelem(sm, parse.igtk, parse.igtk_len) < 0 ||
	    wpa_ft_process_bigtk_subelem(sm, parse.bigtk, parse.bigtk_len) < 0)
		goto fail;

	if (sm->set_ptk_after_assoc) {
		wpa_printf(MSG_DEBUG, "FT: Try to set PTK again now that we "
			   "are associated");
		if (wpa_ft_install_ptk(sm, src_addr) < 0)
			goto fail;
		sm->set_ptk_after_assoc = 0;
	}

	if (parse.ric) {
		wpa_hexdump(MSG_MSGDUMP, "FT: RIC Response",
			    parse.ric, parse.ric_len);
		/* TODO: parse response and inform driver about results when
		 * using wpa_supplicant SME */
	}

	wpa_printf(MSG_DEBUG, "FT: Completed successfully");

	ret = 0;
fail:
	wpa_ft_parse_ies_free(&parse);
	return ret;
}


/**
 * wpa_ft_start_over_ds - Generate over-the-DS auth request
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @target_ap: Target AP Address
 * @mdie: Mobility Domain IE from the target AP
 * @force: Whether to force the request and ignore mobility domain differences
 *	(only for testing purposes)
 * Returns: 0 on success, -1 on failure
 */
int wpa_ft_start_over_ds(struct wpa_sm *sm, const u8 *target_ap,
			 const u8 *mdie, bool force)
{
	u8 *ft_ies;
	size_t ft_ies_len;

	if (!force &&
	    (!mdie || mdie[1] < 3 || !wpa_sm_has_ft_keys(sm, mdie + 2))) {
		wpa_printf(MSG_DEBUG, "FT: Cannot use over-the DS with " MACSTR
			   " - no keys matching the mobility domain",
			   MAC2STR(target_ap));
		return -1;
	}

	wpa_printf(MSG_DEBUG, "FT: Request over-the-DS with " MACSTR,
		   MAC2STR(target_ap));

	/* Generate a new SNonce */
	if (random_get_bytes(sm->snonce, WPA_NONCE_LEN)) {
		wpa_printf(MSG_INFO, "FT: Failed to generate a new SNonce");
		return -1;
	}

	ft_ies = wpa_ft_gen_req_ies(sm, &ft_ies_len, NULL, sm->pmk_r0_name,
				    NULL, 0, target_ap, NULL, 0, mdie, 0);
	if (ft_ies) {
		sm->over_the_ds_in_progress = 1;
		os_memcpy(sm->target_ap, target_ap, ETH_ALEN);
		wpa_sm_send_ft_action(sm, 1, target_ap, ft_ies, ft_ies_len);
		os_free(ft_ies);
	}

	return 0;
}


#ifdef CONFIG_PASN

static struct pasn_ft_r1kh * wpa_ft_pasn_get_r1kh(struct wpa_sm *sm,
						  const u8 *bssid)
{
	size_t i;

	for (i = 0; i < sm->n_pasn_r1kh; i++)
		if (ether_addr_equal(sm->pasn_r1kh[i].bssid, bssid))
			return &sm->pasn_r1kh[i];

	return NULL;
}


static void wpa_ft_pasn_store_r1kh(struct wpa_sm *sm, const u8 *bssid)
{
	struct pasn_ft_r1kh *tmp = wpa_ft_pasn_get_r1kh(sm, bssid);

	if (tmp)
		return;

	tmp = os_realloc_array(sm->pasn_r1kh, sm->n_pasn_r1kh + 1,
			       sizeof(*tmp));
	if (!tmp) {
		wpa_printf(MSG_DEBUG, "PASN: FT: Failed to store R1KH");
		return;
	}

	sm->pasn_r1kh = tmp;
	tmp = &sm->pasn_r1kh[sm->n_pasn_r1kh];

	wpa_printf(MSG_DEBUG, "PASN: FT: Store R1KH for " MACSTR,
		   MAC2STR(bssid));

	os_memcpy(tmp->bssid, bssid, ETH_ALEN);
	os_memcpy(tmp->r1kh_id, sm->r1kh_id, FT_R1KH_ID_LEN);

	sm->n_pasn_r1kh++;
}


int wpa_pasn_ft_derive_pmk_r1(struct wpa_sm *sm, int akmp, const u8 *bssid,
			      u8 *pmk_r1, size_t *pmk_r1_len, u8 *pmk_r1_name)
{
	struct pasn_ft_r1kh *r1kh_entry;

	if (sm->key_mgmt != (unsigned int) akmp) {
		wpa_printf(MSG_DEBUG,
			   "PASN: FT: Key management mismatch: %u != %u",
			   sm->key_mgmt, akmp);
		return -1;
	}

	r1kh_entry = wpa_ft_pasn_get_r1kh(sm, bssid);
	if (!r1kh_entry) {
		wpa_printf(MSG_DEBUG,
			   "PASN: FT: Cannot find R1KH-ID for " MACSTR,
			   MAC2STR(bssid));
		return -1;
	}

	/*
	 * Note: PMK R0 etc. were already derived and are maintained by the
	 * state machine, and as the same key hierarchy is used, there is no
	 * need to derive them again, so only derive PMK R1 etc.
	 */
	if (wpa_derive_pmk_r1(sm->pmk_r0, sm->pmk_r0_len, sm->pmk_r0_name,
			      r1kh_entry->r1kh_id, sm->own_addr, pmk_r1,
			      pmk_r1_name) < 0)
		return -1;

	*pmk_r1_len = sm->pmk_r0_len;

	wpa_hexdump_key(MSG_DEBUG, "PASN: FT: PMK-R1", pmk_r1, sm->pmk_r0_len);
	wpa_hexdump(MSG_DEBUG, "PASN: FT: PMKR1Name", pmk_r1_name,
		    WPA_PMK_NAME_LEN);

	return 0;
}

#endif /* CONFIG_PASN */

#endif /* CONFIG_IEEE80211R */
