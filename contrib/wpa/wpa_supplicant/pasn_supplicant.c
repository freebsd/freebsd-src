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
#include "config.h"

static const int dot11RSNAConfigPMKLifetime = 43200;

struct wpa_pasn_auth_work {
	u8 bssid[ETH_ALEN];
	int akmp;
	int cipher;
	u16 group;
	int network_id;
	struct wpabuf *comeback;
};


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
}


static void wpas_pasn_cancel_auth_work(struct wpa_supplicant *wpa_s)
{
	wpa_printf(MSG_DEBUG, "PASN: Cancel pasn-start-auth work");

	/* Remove pending/started work */
	radio_remove_works(wpa_s, "pasn-start-auth", 0);
}


static void wpas_pasn_auth_status(struct wpa_supplicant *wpa_s, const u8 *bssid,
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
				MAC2STR(bssid),
				wpa_key_mgmt_txt(akmp, WPA_PROTO_RSN),
				status, comeback_after, comeback_txt);

			os_free(comeback_txt);
			return;
		}
	}

	wpa_msg(wpa_s, MSG_INFO,
		PASN_AUTH_STATUS MACSTR " akmp=%s, status=%u",
		MAC2STR(bssid), wpa_key_mgmt_txt(akmp, WPA_PROTO_RSN),
		status);
}


#ifdef CONFIG_SAE

static struct wpabuf * wpas_pasn_wd_sae_commit(struct wpa_supplicant *wpa_s)
{
	struct wpas_pasn *pasn = &wpa_s->pasn;
	struct wpabuf *buf = NULL;
	int ret;

	ret = sae_set_group(&pasn->sae, pasn->group);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to set SAE group");
		return NULL;
	}

	ret = sae_prepare_commit_pt(&pasn->sae, pasn->ssid->pt,
				    wpa_s->own_addr, pasn->bssid,
				    NULL, NULL);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to prepare SAE commit");
		return NULL;
	}

	/* Need to add the entire Authentication frame body */
	buf = wpabuf_alloc(6 + SAE_COMMIT_MAX_LEN);
	if (!buf) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to allocate SAE buffer");
		return NULL;
	}

	wpabuf_put_le16(buf, WLAN_AUTH_SAE);
	wpabuf_put_le16(buf, 1);
	wpabuf_put_le16(buf, WLAN_STATUS_SAE_HASH_TO_ELEMENT);

	sae_write_commit(&pasn->sae, buf, NULL, 0);
	pasn->sae.state = SAE_COMMITTED;

	return buf;
}


static int wpas_pasn_wd_sae_rx(struct wpa_supplicant *wpa_s, struct wpabuf *wd)
{
	struct wpas_pasn *pasn = &wpa_s->pasn;
	const u8 *data;
	size_t buf_len;
	u16 len, res, alg, seq, status;
	int groups[] = { pasn->group, 0 };
	int ret;

	if (!wd)
		return -1;

	data = wpabuf_head_u8(wd);
	buf_len = wpabuf_len(wd);

	/* first handle the commit message */
	if (buf_len < 2) {
		wpa_printf(MSG_DEBUG, "PASN: SAE buffer too short (commit)");
		return -1;
	}

	len = WPA_GET_LE16(data);
	if (len < 6 || buf_len - 2 < len) {
		wpa_printf(MSG_DEBUG, "PASN: SAE buffer too short for commit");
		return -1;
	}

	buf_len -= 2;
	data += 2;

	alg = WPA_GET_LE16(data);
	seq = WPA_GET_LE16(data + 2);
	status = WPA_GET_LE16(data + 4);

	wpa_printf(MSG_DEBUG, "PASN: SAE: commit: alg=%u, seq=%u, status=%u",
		   alg, seq, status);

	if (alg != WLAN_AUTH_SAE || seq != 1 ||
	    status != WLAN_STATUS_SAE_HASH_TO_ELEMENT) {
		wpa_printf(MSG_DEBUG, "PASN: SAE: dropping peer commit");
		return -1;
	}

	res = sae_parse_commit(&pasn->sae, data + 6, len - 6, NULL, 0, groups,
			       1);
	if (res != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG, "PASN: SAE failed parsing commit");
		return -1;
	}

	/* Process the commit message and derive the PMK */
	ret = sae_process_commit(&pasn->sae);
	if (ret) {
		wpa_printf(MSG_DEBUG, "SAE: Failed to process peer commit");
		return -1;
	}

	buf_len -= len;
	data += len;

	/* Handle the confirm message */
	if (buf_len < 2) {
		wpa_printf(MSG_DEBUG, "PASN: SAE buffer too short (confirm)");
		return -1;
	}

	len = WPA_GET_LE16(data);
	if (len < 6 || buf_len - 2 < len) {
		wpa_printf(MSG_DEBUG, "PASN: SAE buffer too short for confirm");
		return -1;
	}

	buf_len -= 2;
	data += 2;

	alg = WPA_GET_LE16(data);
	seq = WPA_GET_LE16(data + 2);
	status = WPA_GET_LE16(data + 4);

	wpa_printf(MSG_DEBUG, "PASN: SAE confirm: alg=%u, seq=%u, status=%u",
		   alg, seq, status);

	if (alg != WLAN_AUTH_SAE || seq != 2 || status != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG, "PASN: Dropping peer SAE confirm");
		return -1;
	}

	res = sae_check_confirm(&pasn->sae, data + 6, len - 6);
	if (res != WLAN_STATUS_SUCCESS) {
		wpa_printf(MSG_DEBUG, "PASN: SAE failed checking confirm");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "PASN: SAE completed successfully");
	pasn->sae.state = SAE_ACCEPTED;

	return 0;
}


static struct wpabuf * wpas_pasn_wd_sae_confirm(struct wpa_supplicant *wpa_s)
{
	struct wpas_pasn *pasn = &wpa_s->pasn;
	struct wpabuf *buf = NULL;

	/* Need to add the entire authentication frame body */
	buf = wpabuf_alloc(6 + SAE_CONFIRM_MAX_LEN);
	if (!buf) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to allocate SAE buffer");
		return NULL;
	}

	wpabuf_put_le16(buf, WLAN_AUTH_SAE);
	wpabuf_put_le16(buf, 2);
	wpabuf_put_le16(buf, WLAN_STATUS_SUCCESS);

	sae_write_confirm(&pasn->sae, buf);
	pasn->sae.state = SAE_CONFIRMED;

	return buf;
}


static int wpas_pasn_sae_setup_pt(struct wpa_supplicant *wpa_s,
				  struct wpa_ssid *ssid, int group)
{
	const char *password = ssid->sae_password;
	int groups[2] = { group, 0 };

	if (!password)
		password = ssid->passphrase;

	if (!password) {
		wpa_printf(MSG_DEBUG, "PASN: SAE without a password");
		return -1;
	}

	if (ssid->pt)
		return 0; /* PT already derived */

	ssid->pt = sae_derive_pt(groups, ssid->ssid, ssid->ssid_len,
				 (const u8 *) password, os_strlen(password),
				 ssid->sae_password_id);

	return ssid->pt ? 0 : -1;
}

#endif /* CONFIG_SAE */


#ifdef CONFIG_FILS

static struct wpabuf * wpas_pasn_fils_build_auth(struct wpa_supplicant *wpa_s)
{
	struct wpas_pasn *pasn = &wpa_s->pasn;
	struct wpabuf *buf = NULL;
	struct wpabuf *erp_msg;
	int ret;

	erp_msg = eapol_sm_build_erp_reauth_start(wpa_s->eapol);
	if (!erp_msg) {
		wpa_printf(MSG_DEBUG,
			   "PASN: FILS: ERP EAP-Initiate/Re-auth unavailable");
		return NULL;
	}

	if (random_get_bytes(pasn->fils.nonce, FILS_NONCE_LEN) < 0 ||
	    random_get_bytes(pasn->fils.session, FILS_SESSION_LEN) < 0)
		goto fail;

	wpa_hexdump(MSG_DEBUG, "PASN: FILS: Nonce", pasn->fils.nonce,
		    FILS_NONCE_LEN);

	wpa_hexdump(MSG_DEBUG, "PASN: FILS: Session", pasn->fils.session,
		    FILS_SESSION_LEN);

	buf = wpabuf_alloc(1500);
	if (!buf)
		goto fail;

	/* Add the authentication algorithm */
	wpabuf_put_le16(buf, WLAN_AUTH_FILS_SK);

	/* Authentication Transaction seq# */
	wpabuf_put_le16(buf, 1);

	/* Status Code */
	wpabuf_put_le16(buf, WLAN_STATUS_SUCCESS);

	/* Own RSNE */
	wpa_pasn_add_rsne(buf, NULL, pasn->akmp, pasn->cipher);

	/* FILS Nonce */
	wpabuf_put_u8(buf, WLAN_EID_EXTENSION);
	wpabuf_put_u8(buf, 1 + FILS_NONCE_LEN);
	wpabuf_put_u8(buf, WLAN_EID_EXT_FILS_NONCE);
	wpabuf_put_data(buf, pasn->fils.nonce, FILS_NONCE_LEN);

	/* FILS Session */
	wpabuf_put_u8(buf, WLAN_EID_EXTENSION);
	wpabuf_put_u8(buf, 1 + FILS_SESSION_LEN);
	wpabuf_put_u8(buf, WLAN_EID_EXT_FILS_SESSION);
	wpabuf_put_data(buf, pasn->fils.session, FILS_SESSION_LEN);

	/* Wrapped Data (ERP) */
	wpabuf_put_u8(buf, WLAN_EID_EXTENSION);
	wpabuf_put_u8(buf, 1 + wpabuf_len(erp_msg));
	wpabuf_put_u8(buf, WLAN_EID_EXT_WRAPPED_DATA);
	wpabuf_put_buf(buf, erp_msg);

	/*
	 * Calculate pending PMKID here so that we do not need to maintain a
	 * copy of the EAP-Initiate/Reauth message.
	 */
	ret = fils_pmkid_erp(pasn->akmp, wpabuf_head(erp_msg),
			     wpabuf_len(erp_msg),
			     pasn->fils.erp_pmkid);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Failed to get ERP PMKID");
		goto fail;
	}

	wpabuf_free(erp_msg);
	erp_msg = NULL;

	wpa_hexdump_buf(MSG_DEBUG, "PASN: FILS: Authentication frame", buf);
	return buf;
fail:
	wpabuf_free(erp_msg);
	wpabuf_free(buf);
	return NULL;
}


static void wpas_pasn_initiate_eapol(struct wpa_supplicant *wpa_s)
{
	struct wpas_pasn *pasn = &wpa_s->pasn;
	struct eapol_config eapol_conf;
	struct wpa_ssid *ssid = pasn->ssid;

	wpa_printf(MSG_DEBUG, "PASN: FILS: Initiating EAPOL");

	eapol_sm_notify_eap_success(wpa_s->eapol, false);
	eapol_sm_notify_eap_fail(wpa_s->eapol, false);
	eapol_sm_notify_portControl(wpa_s->eapol, Auto);

	os_memset(&eapol_conf, 0, sizeof(eapol_conf));
	eapol_conf.fast_reauth = wpa_s->conf->fast_reauth;
	eapol_conf.workaround = ssid->eap_workaround;

	eapol_sm_notify_config(wpa_s->eapol, &ssid->eap, &eapol_conf);
}


static struct wpabuf * wpas_pasn_wd_fils_auth(struct wpa_supplicant *wpa_s)
{
	struct wpas_pasn *pasn = &wpa_s->pasn;
	struct wpa_bss *bss;
	const u8 *indic;
	u16 fils_info;

	wpa_printf(MSG_DEBUG, "PASN: FILS: wrapped data - completed=%u",
		   pasn->fils.completed);

	/* Nothing to add as we are done */
	if (pasn->fils.completed)
		return NULL;

	if (!pasn->ssid) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: No network block");
		return NULL;
	}

	bss = wpa_bss_get_bssid(wpa_s, pasn->bssid);
	if (!bss) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: BSS not found");
		return NULL;
	}

	indic = wpa_bss_get_ie(bss, WLAN_EID_FILS_INDICATION);
	if (!indic || indic[1] < 2) {
		wpa_printf(MSG_DEBUG, "PASN: Missing FILS Indication IE");
		return NULL;
	}

	fils_info = WPA_GET_LE16(indic + 2);
	if (!(fils_info & BIT(9))) {
		wpa_printf(MSG_DEBUG,
			   "PASN: FILS auth without PFS not supported");
		return NULL;
	}

	wpas_pasn_initiate_eapol(wpa_s);

	return wpas_pasn_fils_build_auth(wpa_s);
}


static int wpas_pasn_wd_fils_rx(struct wpa_supplicant *wpa_s, struct wpabuf *wd)
{
	struct wpas_pasn *pasn = &wpa_s->pasn;
	struct ieee802_11_elems elems;
	struct wpa_ie_data rsne_data;
	u8 rmsk[ERP_MAX_KEY_LEN];
	size_t rmsk_len;
	u8 anonce[FILS_NONCE_LEN];
	const u8 *data;
	size_t buf_len;
	struct wpabuf *fils_wd = NULL;
	u16 alg, seq, status;
	int ret;

	if (!wd)
		return -1;

	data = wpabuf_head(wd);
	buf_len = wpabuf_len(wd);

	wpa_hexdump(MSG_DEBUG, "PASN: FILS: Authentication frame len=%zu",
		    data, buf_len);

	/* first handle the header */
	if (buf_len < 6) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Buffer too short");
		return -1;
	}

	alg = WPA_GET_LE16(data);
	seq = WPA_GET_LE16(data + 2);
	status = WPA_GET_LE16(data + 4);

	wpa_printf(MSG_DEBUG, "PASN: FILS: commit: alg=%u, seq=%u, status=%u",
		   alg, seq, status);

	if (alg != WLAN_AUTH_FILS_SK || seq != 2 ||
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
	    !elems.wrapped_data) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Missing IEs");
		return -1;
	}

	ret = wpa_parse_wpa_ie(elems.rsn_ie - 2, elems.rsn_ie_len + 2,
			       &rsne_data);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Failed parsing RNSE");
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

	wpa_hexdump(MSG_DEBUG, "PASN: FILS: ANonce", elems.fils_nonce,
		    FILS_NONCE_LEN);
	os_memcpy(anonce, elems.fils_nonce, FILS_NONCE_LEN);

	wpa_hexdump(MSG_DEBUG, "PASN: FILS: FILS Session", elems.fils_session,
		    FILS_SESSION_LEN);

	if (os_memcmp(pasn->fils.session, elems.fils_session,
		      FILS_SESSION_LEN)) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Session mismatch");
		return -1;
	}

	fils_wd = ieee802_11_defrag(&elems, WLAN_EID_EXTENSION,
				    WLAN_EID_EXT_WRAPPED_DATA);

	if (!fils_wd) {
		wpa_printf(MSG_DEBUG,
			   "PASN: FILS: Failed getting wrapped data");
		return -1;
	}

	eapol_sm_process_erp_finish(wpa_s->eapol, wpabuf_head(fils_wd),
				    wpabuf_len(fils_wd));

	wpabuf_free(fils_wd);
	fils_wd = NULL;

	if (eapol_sm_failed(wpa_s->eapol)) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: ERP finish failed");
		return -1;
	}

	rmsk_len = ERP_MAX_KEY_LEN;
	ret = eapol_sm_get_key(wpa_s->eapol, rmsk, rmsk_len);

	if (ret == PMK_LEN) {
		rmsk_len = PMK_LEN;
		ret = eapol_sm_get_key(wpa_s->eapol, rmsk, rmsk_len);
	}

	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Failed getting RMSK");
		return -1;
	}

	ret = fils_rmsk_to_pmk(pasn->akmp, rmsk, rmsk_len,
			       pasn->fils.nonce, anonce, NULL, 0,
			       pasn->pmk, &pasn->pmk_len);

	forced_memzero(rmsk, sizeof(rmsk));

	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: FILS: Failed to derive PMK");
		return -1;
	}

	wpa_hexdump(MSG_DEBUG, "PASN: FILS: PMKID", pasn->fils.erp_pmkid,
		    PMKID_LEN);

	wpa_printf(MSG_DEBUG, "PASN: FILS: ERP processing succeeded");

	wpa_pasn_pmksa_cache_add(wpa_s->wpa, pasn->pmk,
				 pasn->pmk_len, pasn->fils.erp_pmkid,
				 pasn->bssid, pasn->akmp);

	pasn->fils.completed = true;
	return 0;
}

#endif /* CONFIG_FILS */


static struct wpabuf * wpas_pasn_get_wrapped_data(struct wpa_supplicant *wpa_s)
{
	struct wpas_pasn *pasn = &wpa_s->pasn;

	if (pasn->using_pmksa)
		return NULL;

	switch (pasn->akmp) {
	case WPA_KEY_MGMT_PASN:
		/* no wrapped data */
		return NULL;
	case WPA_KEY_MGMT_SAE:
#ifdef CONFIG_SAE
		if (pasn->trans_seq == 0)
			return wpas_pasn_wd_sae_commit(wpa_s);
		if (pasn->trans_seq == 2)
			return wpas_pasn_wd_sae_confirm(wpa_s);
#endif /* CONFIG_SAE */
		wpa_printf(MSG_ERROR,
			   "PASN: SAE: Cannot derive wrapped data");
		return NULL;
	case WPA_KEY_MGMT_FILS_SHA256:
	case WPA_KEY_MGMT_FILS_SHA384:
#ifdef CONFIG_FILS
		return wpas_pasn_wd_fils_auth(wpa_s);
#endif /* CONFIG_FILS */
	case WPA_KEY_MGMT_FT_PSK:
	case WPA_KEY_MGMT_FT_IEEE8021X:
	case WPA_KEY_MGMT_FT_IEEE8021X_SHA384:
		/*
		 * Wrapped data with these AKMs is optional and is only needed
		 * for further validation of FT security parameters. For now do
		 * not use them.
		 */
		return NULL;
	default:
		wpa_printf(MSG_ERROR,
			   "PASN: TODO: Wrapped data for akmp=0x%x",
			   pasn->akmp);
		return NULL;
	}
}


static u8 wpas_pasn_get_wrapped_data_format(struct wpas_pasn *pasn)
{
	if (pasn->using_pmksa)
		return WPA_PASN_WRAPPED_DATA_NO;

	/* Note: Valid AKMP is expected to already be validated */
	switch (pasn->akmp) {
	case WPA_KEY_MGMT_SAE:
		return WPA_PASN_WRAPPED_DATA_SAE;
	case WPA_KEY_MGMT_FILS_SHA256:
	case WPA_KEY_MGMT_FILS_SHA384:
		return WPA_PASN_WRAPPED_DATA_FILS_SK;
	case WPA_KEY_MGMT_FT_PSK:
	case WPA_KEY_MGMT_FT_IEEE8021X:
	case WPA_KEY_MGMT_FT_IEEE8021X_SHA384:
		/*
		 * Wrapped data with these AKMs is optional and is only needed
		 * for further validation of FT security parameters. For now do
		 * not use them.
		 */
		return WPA_PASN_WRAPPED_DATA_NO;
	case WPA_KEY_MGMT_PASN:
	default:
		return WPA_PASN_WRAPPED_DATA_NO;
	}
}


static struct wpabuf * wpas_pasn_build_auth_1(struct wpa_supplicant *wpa_s,
					      const struct wpabuf *comeback)
{
	struct wpas_pasn *pasn = &wpa_s->pasn;
	struct wpabuf *buf, *pubkey = NULL, *wrapped_data_buf = NULL;
	const u8 *pmkid;
	u8 wrapped_data;
	int ret;
	u16 capab;

	wpa_printf(MSG_DEBUG, "PASN: Building frame 1");

	if (pasn->trans_seq)
		return NULL;

	buf = wpabuf_alloc(1500);
	if (!buf)
		goto fail;

	/* Get public key */
	pubkey = crypto_ecdh_get_pubkey(pasn->ecdh, 0);
	pubkey = wpabuf_zeropad(pubkey, crypto_ecdh_prime_len(pasn->ecdh));
	if (!pubkey) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to get pubkey");
		goto fail;
	}

	wrapped_data = wpas_pasn_get_wrapped_data_format(pasn);

	wpa_pasn_build_auth_header(buf, pasn->bssid,
				   wpa_s->own_addr, pasn->bssid,
				   pasn->trans_seq + 1, WLAN_STATUS_SUCCESS);

	pmkid = NULL;
	if (wpa_key_mgmt_ft(pasn->akmp)) {
		ret = wpa_pasn_ft_derive_pmk_r1(wpa_s->wpa, pasn->akmp,
						pasn->bssid,
						pasn->pmk_r1,
						&pasn->pmk_r1_len,
						pasn->pmk_r1_name);
		if (ret) {
			wpa_printf(MSG_DEBUG,
				   "PASN: FT: Failed to derive keys");
			goto fail;
		}

		pmkid = pasn->pmk_r1_name;
	} else if (wrapped_data != WPA_PASN_WRAPPED_DATA_NO) {
		struct rsn_pmksa_cache_entry *pmksa;

		pmksa = wpa_sm_pmksa_cache_get(wpa_s->wpa, pasn->bssid,
					       NULL, NULL, pasn->akmp);
		if (pmksa)
			pmkid = pmksa->pmkid;

		/*
		 * Note: Even when PMKSA is available, also add wrapped data as
		 * it is possible that the PMKID is no longer valid at the AP.
		 */
		wrapped_data_buf = wpas_pasn_get_wrapped_data(wpa_s);
	}

	if (wpa_pasn_add_rsne(buf, pmkid, pasn->akmp, pasn->cipher) < 0)
		goto fail;

	if (!wrapped_data_buf)
		wrapped_data = WPA_PASN_WRAPPED_DATA_NO;

	wpa_pasn_add_parameter_ie(buf, pasn->group, wrapped_data,
				  pubkey, true, comeback, -1);

	if (wpa_pasn_add_wrapped_data(buf, wrapped_data_buf) < 0)
		goto fail;

	/* Add own RNSXE */
	capab = 0;
	capab |= BIT(WLAN_RSNX_CAPAB_SAE_H2E);
	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_LTF)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_LTF);
	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_RTT)
		capab |= BIT(WLAN_RSNX_CAPAB_SECURE_RTT);
	if (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_PROT_RANGE_NEG)
		capab |= BIT(WLAN_RSNX_CAPAB_PROT_RANGE_NEG);
	wpa_pasn_add_rsnxe(buf, capab);

	ret = pasn_auth_frame_hash(pasn->akmp, pasn->cipher,
				   wpabuf_head_u8(buf) + IEEE80211_HDRLEN,
				   wpabuf_len(buf) - IEEE80211_HDRLEN,
				   pasn->hash);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to compute hash");
		goto fail;
	}

	pasn->trans_seq++;

	wpabuf_free(wrapped_data_buf);
	wpabuf_free(pubkey);

	wpa_printf(MSG_DEBUG, "PASN: Frame 1: Success");
	return buf;
fail:
	pasn->status = WLAN_STATUS_UNSPECIFIED_FAILURE;
	wpabuf_free(wrapped_data_buf);
	wpabuf_free(pubkey);
	wpabuf_free(buf);
	return NULL;
}


static struct wpabuf * wpas_pasn_build_auth_3(struct wpa_supplicant *wpa_s)
{
	struct wpas_pasn *pasn = &wpa_s->pasn;
	struct wpabuf *buf, *wrapped_data_buf = NULL;
	u8 mic[WPA_PASN_MAX_MIC_LEN];
	u8 mic_len, data_len;
	const u8 *data;
	u8 *ptr;
	u8 wrapped_data;
	int ret;

	wpa_printf(MSG_DEBUG, "PASN: Building frame 3");

	if (pasn->trans_seq != 2)
		return NULL;

	buf = wpabuf_alloc(1500);
	if (!buf)
		goto fail;

	wrapped_data = wpas_pasn_get_wrapped_data_format(pasn);

	wpa_pasn_build_auth_header(buf, pasn->bssid,
				   wpa_s->own_addr, pasn->bssid,
				   pasn->trans_seq + 1, WLAN_STATUS_SUCCESS);

	wrapped_data_buf = wpas_pasn_get_wrapped_data(wpa_s);

	if (!wrapped_data_buf)
		wrapped_data = WPA_PASN_WRAPPED_DATA_NO;

	wpa_pasn_add_parameter_ie(buf, pasn->group, wrapped_data,
				  NULL, false, NULL, -1);

	if (wpa_pasn_add_wrapped_data(buf, wrapped_data_buf) < 0)
		goto fail;
	wpabuf_free(wrapped_data_buf);
	wrapped_data_buf = NULL;

	/* Add the MIC */
	mic_len = pasn_mic_len(pasn->akmp, pasn->cipher);
	wpabuf_put_u8(buf, WLAN_EID_MIC);
	wpabuf_put_u8(buf, mic_len);
	ptr = wpabuf_put(buf, mic_len);

	os_memset(ptr, 0, mic_len);

	data = wpabuf_head_u8(buf) + IEEE80211_HDRLEN;
	data_len = wpabuf_len(buf) - IEEE80211_HDRLEN;

	ret = pasn_mic(pasn->ptk.kck, pasn->akmp, pasn->cipher,
		       wpa_s->own_addr, pasn->bssid,
		       pasn->hash, mic_len * 2, data, data_len, mic);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: frame 3: Failed MIC calculation");
		goto fail;
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (wpa_s->conf->pasn_corrupt_mic) {
		wpa_printf(MSG_DEBUG, "PASN: frame 3: Corrupt MIC");
		mic[0] = ~mic[0];
	}
#endif /* CONFIG_TESTING_OPTIONS */

	os_memcpy(ptr, mic, mic_len);

	pasn->trans_seq++;

	wpa_printf(MSG_DEBUG, "PASN: frame 3: Success");
	return buf;
fail:
	pasn->status = WLAN_STATUS_UNSPECIFIED_FAILURE;
	wpabuf_free(wrapped_data_buf);
	wpabuf_free(buf);
	return NULL;
}


static void wpas_pasn_reset(struct wpa_supplicant *wpa_s)
{
	struct wpas_pasn *pasn = &wpa_s->pasn;

	wpa_printf(MSG_DEBUG, "PASN: Reset");

	crypto_ecdh_deinit(pasn->ecdh);
	pasn->ecdh = NULL;

	wpas_pasn_cancel_auth_work(wpa_s);
	wpa_s->pasn_auth_work = NULL;

	eloop_cancel_timeout(wpas_pasn_auth_work_timeout, wpa_s, NULL);

	pasn->akmp = 0;
	pasn->cipher = 0;
	pasn->group = 0;
	pasn->trans_seq = 0;
	pasn->pmk_len = 0;
	pasn->using_pmksa = false;

	forced_memzero(pasn->pmk, sizeof(pasn->pmk));
	forced_memzero(&pasn->ptk, sizeof(pasn->ptk));
	forced_memzero(&pasn->hash, sizeof(pasn->hash));

	wpabuf_free(pasn->beacon_rsne_rsnxe);
	pasn->beacon_rsne_rsnxe = NULL;

	wpabuf_free(pasn->comeback);
	pasn->comeback = NULL;
	pasn->comeback_after = 0;

#ifdef CONFIG_SAE
	sae_clear_data(&pasn->sae);
#endif /* CONFIG_SAE */

#ifdef CONFIG_FILS
	os_memset(&pasn->fils, 0, sizeof(pasn->fils));
#endif /* CONFIG_FILS*/

#ifdef CONFIG_IEEE80211R
	forced_memzero(pasn->pmk_r1, sizeof(pasn->pmk_r1));
	pasn->pmk_r1_len = 0;
	os_memset(pasn->pmk_r1_name, 0, sizeof(pasn->pmk_r1_name));
#endif /* CONFIG_IEEE80211R */
	pasn->status = WLAN_STATUS_UNSPECIFIED_FAILURE;
}


static int wpas_pasn_set_pmk(struct wpa_supplicant *wpa_s,
			     struct wpa_ie_data *rsn_data,
			     struct wpa_pasn_params_data *pasn_data,
			     struct wpabuf *wrapped_data)
{
	static const u8 pasn_default_pmk[] = {'P', 'M', 'K', 'z'};
	struct wpas_pasn *pasn = &wpa_s->pasn;

	os_memset(pasn->pmk, 0, sizeof(pasn->pmk));
	pasn->pmk_len = 0;

	if (pasn->akmp == WPA_KEY_MGMT_PASN) {
		wpa_printf(MSG_DEBUG, "PASN: Using default PMK");

		pasn->pmk_len = WPA_PASN_PMK_LEN;
		os_memcpy(pasn->pmk, pasn_default_pmk,
			  sizeof(pasn_default_pmk));
		return 0;
	}

	if (wpa_key_mgmt_ft(pasn->akmp)) {
#ifdef CONFIG_IEEE80211R
		wpa_printf(MSG_DEBUG, "PASN: FT: Using PMK-R1");
		pasn->pmk_len = pasn->pmk_r1_len;
		os_memcpy(pasn->pmk, pasn->pmk_r1, pasn->pmk_r1_len);
		pasn->using_pmksa = true;
		return 0;
#else /* CONFIG_IEEE80211R */
		wpa_printf(MSG_DEBUG, "PASN: FT: Not supported");
		return -1;
#endif /* CONFIG_IEEE80211R */
	}

	if (rsn_data->num_pmkid) {
		struct rsn_pmksa_cache_entry *pmksa;

		pmksa = wpa_sm_pmksa_cache_get(wpa_s->wpa, pasn->bssid,
					       rsn_data->pmkid, NULL,
					       pasn->akmp);
		if (pmksa) {
			wpa_printf(MSG_DEBUG, "PASN: Using PMKSA");

			pasn->pmk_len = pmksa->pmk_len;
			os_memcpy(pasn->pmk, pmksa->pmk, pmksa->pmk_len);
			pasn->using_pmksa = true;

			return 0;
		}
	}

#ifdef CONFIG_SAE
	if (pasn->akmp == WPA_KEY_MGMT_SAE) {
		int ret;

		ret = wpas_pasn_wd_sae_rx(wpa_s, wrapped_data);
		if (ret) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Failed processing SAE wrapped data");
			pasn->status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			return -1;
		}

		wpa_printf(MSG_DEBUG, "PASN: Success deriving PMK with SAE");
		pasn->pmk_len = PMK_LEN;
		os_memcpy(pasn->pmk, pasn->sae.pmk, PMK_LEN);

		wpa_pasn_pmksa_cache_add(wpa_s->wpa, pasn->pmk,
					 pasn->pmk_len, pasn->sae.pmkid,
					 pasn->bssid, pasn->akmp);
		return 0;
	}
#endif /* CONFIG_SAE */

#ifdef CONFIG_FILS
	if (pasn->akmp == WPA_KEY_MGMT_FILS_SHA256 ||
	    pasn->akmp == WPA_KEY_MGMT_FILS_SHA384) {
		int ret;

		ret = wpas_pasn_wd_fils_rx(wpa_s, wrapped_data);
		if (ret) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Failed processing FILS wrapped data");
			pasn->status = WLAN_STATUS_UNSPECIFIED_FAILURE;
			return -1;
		}

		return 0;
	}
#endif	/* CONFIG_FILS */

	/* TODO: Derive PMK based on wrapped data */
	wpa_printf(MSG_DEBUG, "PASN: Missing implementation to derive PMK");
	pasn->status = WLAN_STATUS_UNSPECIFIED_FAILURE;
	return -1;
}


static int wpas_pasn_start(struct wpa_supplicant *wpa_s, const u8 *bssid,
			   int akmp, int cipher, u16 group, int freq,
			   const u8 *beacon_rsne, u8 beacon_rsne_len,
			   const u8 *beacon_rsnxe, u8 beacon_rsnxe_len,
			   int network_id, struct wpabuf *comeback)
{
	struct wpas_pasn *pasn = &wpa_s->pasn;
	struct wpa_ssid *ssid = NULL;
	struct wpabuf *frame;
	int ret;

	/* TODO: Currently support only ECC groups */
	if (!dragonfly_suitable_group(group, 1)) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Reject unsuitable group %u", group);
		return -1;
	}

	ssid = wpa_config_get_network(wpa_s->conf, network_id);

	switch (akmp) {
	case WPA_KEY_MGMT_PASN:
		break;
#ifdef CONFIG_SAE
	case WPA_KEY_MGMT_SAE:
		if (!ssid) {
			wpa_printf(MSG_DEBUG,
				   "PASN: No network profile found for SAE");
			return -1;
		}

		if (!ieee802_11_rsnx_capab(beacon_rsnxe,
					   WLAN_RSNX_CAPAB_SAE_H2E)) {
			wpa_printf(MSG_DEBUG,
				   "PASN: AP does not support SAE H2E");
			return -1;
		}

		if (wpas_pasn_sae_setup_pt(wpa_s, ssid, group) < 0) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Failed to derive PT");
			return -1;
		}

		pasn->sae.state = SAE_NOTHING;
		pasn->sae.send_confirm = 0;
		pasn->ssid = ssid;
		break;
#endif /* CONFIG_SAE */
#ifdef CONFIG_FILS
	case WPA_KEY_MGMT_FILS_SHA256:
	case WPA_KEY_MGMT_FILS_SHA384:
		pasn->ssid = ssid;
		break;
#endif /* CONFIG_FILS */
#ifdef CONFIG_IEEE80211R
	case WPA_KEY_MGMT_FT_PSK:
	case WPA_KEY_MGMT_FT_IEEE8021X:
	case WPA_KEY_MGMT_FT_IEEE8021X_SHA384:
		break;
#endif /* CONFIG_IEEE80211R */
	default:
		wpa_printf(MSG_ERROR, "PASN: Unsupported AKMP=0x%x", akmp);
		return -1;
	}

	pasn->ecdh = crypto_ecdh_init(group);
	if (!pasn->ecdh) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to init ECDH");
		goto fail;
	}

	pasn->beacon_rsne_rsnxe = wpabuf_alloc(beacon_rsne_len +
					       beacon_rsnxe_len);
	if (!pasn->beacon_rsne_rsnxe) {
		wpa_printf(MSG_DEBUG, "PASN: Failed storing beacon RSNE/RSNXE");
		goto fail;
	}

	wpabuf_put_data(pasn->beacon_rsne_rsnxe, beacon_rsne, beacon_rsne_len);
	if (beacon_rsnxe && beacon_rsnxe_len)
		wpabuf_put_data(pasn->beacon_rsne_rsnxe, beacon_rsnxe,
				beacon_rsnxe_len);

	pasn->akmp = akmp;
	pasn->cipher = cipher;
	pasn->group = group;
	pasn->freq = freq;

	if (wpa_s->conf->force_kdk_derivation ||
	    (wpa_s->drv_flags2 & WPA_DRIVER_FLAGS2_SEC_LTF &&
	     ieee802_11_rsnx_capab(beacon_rsnxe, WLAN_RSNX_CAPAB_SECURE_LTF)))
		pasn->kdk_len = WPA_KDK_MAX_LEN;
	else
		pasn->kdk_len = 0;
	wpa_printf(MSG_DEBUG, "PASN: kdk_len=%zu", pasn->kdk_len);

	os_memcpy(pasn->bssid, bssid, ETH_ALEN);

	wpa_printf(MSG_DEBUG,
		   "PASN: Init: " MACSTR " akmp=0x%x, cipher=0x%x, group=%u",
		   MAC2STR(pasn->bssid), pasn->akmp, pasn->cipher,
		   pasn->group);

	frame = wpas_pasn_build_auth_1(wpa_s, comeback);
	if (!frame) {
		wpa_printf(MSG_DEBUG, "PASN: Failed building 1st auth frame");
		goto fail;
	}

	ret = wpa_drv_send_mlme(wpa_s, wpabuf_head(frame), wpabuf_len(frame), 0,
				pasn->freq, 1000);

	wpabuf_free(frame);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed sending 1st auth frame");
		goto fail;
	}

	eloop_register_timeout(2, 0, wpas_pasn_auth_work_timeout, wpa_s, NULL);
	return 0;

fail:
	return -1;
}


static struct wpa_bss * wpas_pasn_allowed(struct wpa_supplicant *wpa_s,
					  const u8 *bssid, int akmp, int cipher)
{
	struct wpa_bss *bss;
	const u8 *rsne;
	struct wpa_ie_data rsne_data;
	int ret;

	if (os_memcmp(wpa_s->bssid, bssid, ETH_ALEN) == 0) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Not doing authentication with current BSS");
		return NULL;
	}

	bss = wpa_bss_get_bssid(wpa_s, bssid);
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
	struct wpa_bss *bss;
	const u8 *rsne, *rsnxe;
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
	bss = wpas_pasn_allowed(wpa_s, awork->bssid, awork->akmp,
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

	ret = wpas_pasn_start(wpa_s, awork->bssid, awork->akmp, awork->cipher,
			      awork->group, bss->freq, rsne, *(rsne + 1) + 2,
			      rsnxe, rsnxe ? *(rsnxe + 1) + 2 : 0,
			      awork->network_id, awork->comeback);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Failed to start PASN authentication");
		goto fail;
	}

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


int wpas_pasn_auth_start(struct wpa_supplicant *wpa_s, const u8 *bssid,
			 int akmp, int cipher, u16 group, int network_id,
			 const u8 *comeback, size_t comeback_len)
{
	struct wpa_pasn_auth_work *awork;
	struct wpa_bss *bss;

	wpa_printf(MSG_DEBUG, "PASN: Start: " MACSTR " akmp=0x%x, cipher=0x%x",
		   MAC2STR(bssid), akmp, cipher);

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

	bss = wpas_pasn_allowed(wpa_s, bssid, akmp, cipher);
	if (!bss)
		return -1;

	wpas_pasn_reset(wpa_s);

	awork = os_zalloc(sizeof(*awork));
	if (!awork)
		return -1;

	os_memcpy(awork->bssid, bssid, ETH_ALEN);
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
	struct wpas_pasn *pasn = &wpa_s->pasn;

	if (!wpa_s->pasn.ecdh)
		return;

	wpa_printf(MSG_DEBUG, "PASN: Stopping authentication");

	wpas_pasn_auth_status(wpa_s, pasn->bssid, pasn->akmp, pasn->cipher,
			      pasn->status, pasn->comeback,
			      pasn->comeback_after);

	wpas_pasn_reset(wpa_s);
}


static int wpas_pasn_immediate_retry(struct wpa_supplicant *wpa_s,
				     struct wpas_pasn *pasn,
				     struct wpa_pasn_params_data *params)
{
	int akmp = pasn->akmp;
	int cipher = pasn->cipher;
	u16 group = pasn->group;
	u8 bssid[ETH_ALEN];
	int network_id = pasn->ssid ? pasn->ssid->id : 0;

	wpa_printf(MSG_DEBUG, "PASN: Immediate retry");
	os_memcpy(bssid, pasn->bssid, ETH_ALEN);
	wpas_pasn_reset(wpa_s);

	return wpas_pasn_auth_start(wpa_s, bssid, akmp, cipher, group,
				    network_id,
				    params->comeback, params->comeback_len);
}


int wpas_pasn_auth_rx(struct wpa_supplicant *wpa_s,
		      const struct ieee80211_mgmt *mgmt, size_t len)
{
	struct wpas_pasn *pasn = &wpa_s->pasn;
	struct ieee802_11_elems elems;
	struct wpa_ie_data rsn_data;
	struct wpa_pasn_params_data pasn_params;
	struct wpabuf *wrapped_data = NULL, *secret = NULL, *frame = NULL;
	u8 mic[WPA_PASN_MAX_MIC_LEN], out_mic[WPA_PASN_MAX_MIC_LEN];
	u8 mic_len;
	u16 status;
	int ret, inc_y;
	u16 fc = host_to_le16((WLAN_FC_TYPE_MGMT << 2) |
			      (WLAN_FC_STYPE_AUTH << 4));

	if (!wpa_s->pasn_auth_work || !mgmt ||
	    len < offsetof(struct ieee80211_mgmt, u.auth.variable))
		return -2;

	/* Not an Authentication frame; do nothing */
	if ((mgmt->frame_control & fc) != fc)
		return -2;

	/* Not our frame; do nothing */
	if (os_memcmp(mgmt->da, wpa_s->own_addr, ETH_ALEN) != 0 ||
	    os_memcmp(mgmt->sa, pasn->bssid, ETH_ALEN) != 0 ||
	    os_memcmp(mgmt->bssid, pasn->bssid, ETH_ALEN) != 0)
		return -2;

	/* Not PASN; do nothing */
	if (mgmt->u.auth.auth_alg != host_to_le16(WLAN_AUTH_PASN))
		return -2;

	if (mgmt->u.auth.auth_transaction !=
	    host_to_le16(pasn->trans_seq + 1)) {
		wpa_printf(MSG_DEBUG,
			   "PASN: RX: Invalid transaction sequence: (%u != %u)",
			   le_to_host16(mgmt->u.auth.auth_transaction),
			   pasn->trans_seq + 1);
		return -1;
	}

	status = le_to_host16(mgmt->u.auth.status_code);

	if (status != WLAN_STATUS_SUCCESS &&
	    status != WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Authentication rejected - status=%u", status);
		pasn->status = status;
		wpas_pasn_auth_stop(wpa_s);
		return -1;
	}

	if (ieee802_11_parse_elems(mgmt->u.auth.variable,
				   len - offsetof(struct ieee80211_mgmt,
						  u.auth.variable),
				   &elems, 0) == ParseFailed) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Failed parsing Authentication frame");
		goto fail;
	}

	/* Check that the MIC IE exists. Save it and zero out the memory */
	mic_len = pasn_mic_len(pasn->akmp, pasn->cipher);
	if (status == WLAN_STATUS_SUCCESS) {
		if (!elems.mic || elems.mic_len != mic_len) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Invalid MIC. Expecting len=%u",
				   mic_len);
			goto fail;
		} else {
			os_memcpy(mic, elems.mic, mic_len);
			/* TODO: Clean this up.. Should not be modifying the
			 * received message buffer. */
			os_memset((u8 *) elems.mic, 0, mic_len);
		}
	}

	if (!elems.pasn_params || !elems.pasn_params_len) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Missing PASN Parameters IE");
		goto fail;
	}

	ret = wpa_pasn_parse_parameter_ie(elems.pasn_params - 3,
					  elems.pasn_params_len + 3,
					  true, &pasn_params);
	if (ret) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Failed validation PASN of Parameters IE");
		goto fail;
	}

	if (status == WLAN_STATUS_ASSOC_REJECTED_TEMPORARILY) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Authentication temporarily rejected");

		if (pasn_params.comeback && pasn_params.comeback_len) {
			wpa_printf(MSG_DEBUG,
				   "PASN: Comeback token available. After=%u",
				   pasn_params.after);

			if (!pasn_params.after)
				return wpas_pasn_immediate_retry(wpa_s, pasn,
								 &pasn_params);

			pasn->comeback = wpabuf_alloc_copy(
				pasn_params.comeback, pasn_params.comeback_len);
			if (pasn->comeback)
				pasn->comeback_after = pasn_params.after;
		}

		pasn->status = status;
		goto fail;
	}

	ret = wpa_parse_wpa_ie(elems.rsn_ie - 2, elems.rsn_ie_len + 2,
			       &rsn_data);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed parsing RNSE");
		goto fail;
	}

	ret = wpa_pasn_validate_rsne(&rsn_data);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed validating RSNE");
		goto fail;
	}

	if (pasn->akmp != rsn_data.key_mgmt ||
	    pasn->cipher != rsn_data.pairwise_cipher) {
		wpa_printf(MSG_DEBUG, "PASN: Mismatch in AKMP/cipher");
		goto fail;
	}

	if (pasn->group != pasn_params.group) {
		wpa_printf(MSG_DEBUG, "PASN: Mismatch in group");
		goto fail;
	}

	if (!pasn_params.pubkey || !pasn_params.pubkey_len) {
		wpa_printf(MSG_DEBUG, "PASN: Invalid public key");
		goto fail;
	}

	if (pasn_params.pubkey[0] == WPA_PASN_PUBKEY_UNCOMPRESSED) {
		inc_y = 1;
	} else if (pasn_params.pubkey[0] == WPA_PASN_PUBKEY_COMPRESSED_0 ||
		   pasn_params.pubkey[0] == WPA_PASN_PUBKEY_COMPRESSED_1) {
		inc_y = 0;
	} else {
		wpa_printf(MSG_DEBUG,
			   "PASN: Invalid first octet in pubkey=0x%x",
			   pasn_params.pubkey[0]);
		goto fail;
	}

	secret = crypto_ecdh_set_peerkey(pasn->ecdh, inc_y,
					 pasn_params.pubkey + 1,
					 pasn_params.pubkey_len - 1);

	if (!secret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to derive shared secret");
		goto fail;
	}

	if (pasn_params.wrapped_data_format != WPA_PASN_WRAPPED_DATA_NO) {
		wrapped_data = ieee802_11_defrag(&elems,
						 WLAN_EID_EXTENSION,
						 WLAN_EID_EXT_WRAPPED_DATA);

		if (!wrapped_data) {
			wpa_printf(MSG_DEBUG, "PASN: Missing wrapped data");
			goto fail;
		}
	}

	ret = wpas_pasn_set_pmk(wpa_s, &rsn_data, &pasn_params, wrapped_data);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to set PMK");
		goto fail;
	}

	ret = pasn_pmk_to_ptk(pasn->pmk, pasn->pmk_len,
			      wpa_s->own_addr, pasn->bssid,
			      wpabuf_head(secret), wpabuf_len(secret),
			      &pasn->ptk, pasn->akmp, pasn->cipher,
			      pasn->kdk_len);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed to derive PTK");
		goto fail;
	}

	wpabuf_free(wrapped_data);
	wrapped_data = NULL;
	wpabuf_free(secret);
	secret = NULL;

	/* Verify the MIC */
	ret = pasn_mic(pasn->ptk.kck, pasn->akmp, pasn->cipher,
		       pasn->bssid, wpa_s->own_addr,
		       wpabuf_head(pasn->beacon_rsne_rsnxe),
		       wpabuf_len(pasn->beacon_rsne_rsnxe),
		       (u8 *) &mgmt->u.auth,
		       len - offsetof(struct ieee80211_mgmt, u.auth),
		       out_mic);

	wpa_hexdump_key(MSG_DEBUG, "PASN: Frame MIC", mic, mic_len);
	if (ret || os_memcmp(mic, out_mic, mic_len) != 0) {
		wpa_printf(MSG_DEBUG, "PASN: Failed MIC verification");
		goto fail;
	}

	pasn->trans_seq++;

	wpa_printf(MSG_DEBUG, "PASN: Success verifying Authentication frame");

	frame = wpas_pasn_build_auth_3(wpa_s);
	if (!frame) {
		wpa_printf(MSG_DEBUG, "PASN: Failed building 3rd auth frame");
		goto fail;
	}

	ret = wpa_drv_send_mlme(wpa_s, wpabuf_head(frame), wpabuf_len(frame), 0,
				pasn->freq, 100);
	wpabuf_free(frame);
	if (ret) {
		wpa_printf(MSG_DEBUG, "PASN: Failed sending 3st auth frame");
		goto fail;
	}

	wpa_printf(MSG_DEBUG, "PASN: Success sending last frame. Store PTK");

	ptksa_cache_add(wpa_s->ptksa, pasn->bssid, pasn->cipher,
			dot11RSNAConfigPMKLifetime, &pasn->ptk);

	forced_memzero(&pasn->ptk, sizeof(pasn->ptk));

	pasn->status = WLAN_STATUS_SUCCESS;
	return 0;
fail:
	wpa_printf(MSG_DEBUG, "PASN: Failed RX processing - terminating");
	wpabuf_free(wrapped_data);
	wpabuf_free(secret);

	/*
	 * TODO: In case of an error the standard allows to silently drop
	 * the frame and terminate the authentication exchange. However, better
	 * reply to the AP with an error status.
	 */
	if (status == WLAN_STATUS_SUCCESS)
		pasn->status = WLAN_STATUS_UNSPECIFIED_FAILURE;
	else
		pasn->status = status;

	wpas_pasn_auth_stop(wpa_s);
	return -1;
}


int wpas_pasn_auth_tx_status(struct wpa_supplicant *wpa_s,
			     const u8 *data, size_t data_len, u8 acked)

{
	struct wpas_pasn *pasn = &wpa_s->pasn;
	const struct ieee80211_mgmt *mgmt = (struct ieee80211_mgmt *) data;
	u16 fc = host_to_le16((WLAN_FC_TYPE_MGMT << 2) |
			      (WLAN_FC_STYPE_AUTH << 4));

	wpa_printf(MSG_DEBUG, "PASN: auth_tx_status: acked=%u", acked);

	if (!wpa_s->pasn_auth_work) {
		wpa_printf(MSG_DEBUG,
			   "PASN: auth_tx_status: no work in progress");
		return -1;
	}

	if (!mgmt ||
	    data_len < offsetof(struct ieee80211_mgmt, u.auth.variable))
		return -1;

	/* Not an authentication frame; do nothing */
	if ((mgmt->frame_control & fc) != fc)
		return -1;

	/* Not our frame; do nothing */
	if (os_memcmp(mgmt->da, pasn->bssid, ETH_ALEN) ||
	    os_memcmp(mgmt->sa, wpa_s->own_addr, ETH_ALEN) ||
	    os_memcmp(mgmt->bssid, pasn->bssid, ETH_ALEN))
		return -1;

	/* Not PASN; do nothing */
	if (mgmt->u.auth.auth_alg !=  host_to_le16(WLAN_AUTH_PASN))
		return -1;

	if (mgmt->u.auth.auth_transaction != host_to_le16(pasn->trans_seq)) {
		wpa_printf(MSG_ERROR,
			   "PASN: Invalid transaction sequence: (%u != %u)",
			   pasn->trans_seq,
			   le_to_host16(mgmt->u.auth.auth_transaction));
		return 0;
	}

	wpa_printf(MSG_ERROR,
		   "PASN: auth with trans_seq=%u, acked=%u", pasn->trans_seq,
		   acked);

	/*
	 * Even if the frame was not acked, do not treat this is an error, and
	 * try to complete the flow, relying on the PASN timeout callback to
	 * clean up.
	 */
	if (pasn->trans_seq == 3) {
		wpa_printf(MSG_DEBUG, "PASN: auth complete with: " MACSTR,
			   MAC2STR(pasn->bssid));
		/*
		 * Either frame was not ACKed or it was ACKed but the trans_seq
		 * != 1, i.e., not expecting an RX frame, so we are done.
		 */
		wpas_pasn_auth_stop(wpa_s);
	}

	return 0;
}


int wpas_pasn_deauthenticate(struct wpa_supplicant *wpa_s, const u8 *bssid)
{
	struct wpa_bss *bss;
	struct wpabuf *buf;
	struct ieee80211_mgmt *deauth;
	int ret;

	if (os_memcmp(wpa_s->bssid, bssid, ETH_ALEN) == 0) {
		wpa_printf(MSG_DEBUG,
			   "PASN: Cannot deauthenticate from current BSS");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "PASN: deauth: Flushing all PTKSA entries for "
		   MACSTR, MAC2STR(bssid));
	ptksa_cache_flush(wpa_s->ptksa, bssid, WPA_CIPHER_NONE);

	bss = wpa_bss_get_bssid(wpa_s, bssid);
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

	os_memcpy(deauth->da, bssid, ETH_ALEN);
	os_memcpy(deauth->sa, wpa_s->own_addr, ETH_ALEN);
	os_memcpy(deauth->bssid, bssid, ETH_ALEN);
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
