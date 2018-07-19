/*
 * WPA Supplicant - WPA state machine and EAPOL-Key processing
 * Copyright (c) 2003-2015, Jouni Malinen <j@w1.fi>
 * Copyright(c) 2015 Intel Deutschland GmbH
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "crypto/aes_wrap.h"
#include "crypto/crypto.h"
#include "crypto/random.h"
#include "common/ieee802_11_defs.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "wpa.h"
#include "eloop.h"
#include "preauth.h"
#include "pmksa_cache.h"
#include "wpa_i.h"
#include "wpa_ie.h"
#include "peerkey.h"


static const u8 null_rsc[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };


/**
 * wpa_eapol_key_send - Send WPA/RSN EAPOL-Key message
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @kck: Key Confirmation Key (KCK, part of PTK)
 * @kck_len: KCK length in octets
 * @ver: Version field from Key Info
 * @dest: Destination address for the frame
 * @proto: Ethertype (usually ETH_P_EAPOL)
 * @msg: EAPOL-Key message
 * @msg_len: Length of message
 * @key_mic: Pointer to the buffer to which the EAPOL-Key MIC is written
 * Returns: >= 0 on success, < 0 on failure
 */
int wpa_eapol_key_send(struct wpa_sm *sm, const u8 *kck, size_t kck_len,
		       int ver, const u8 *dest, u16 proto,
		       u8 *msg, size_t msg_len, u8 *key_mic)
{
	int ret = -1;
	size_t mic_len = wpa_mic_len(sm->key_mgmt);

	if (is_zero_ether_addr(dest) && is_zero_ether_addr(sm->bssid)) {
		/*
		 * Association event was not yet received; try to fetch
		 * BSSID from the driver.
		 */
		if (wpa_sm_get_bssid(sm, sm->bssid) < 0) {
			wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
				"WPA: Failed to read BSSID for "
				"EAPOL-Key destination address");
		} else {
			dest = sm->bssid;
			wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
				"WPA: Use BSSID (" MACSTR
				") as the destination for EAPOL-Key",
				MAC2STR(dest));
		}
	}
	if (key_mic &&
	    wpa_eapol_key_mic(kck, kck_len, sm->key_mgmt, ver, msg, msg_len,
			      key_mic)) {
		wpa_msg(sm->ctx->msg_ctx, MSG_ERROR,
			"WPA: Failed to generate EAPOL-Key version %d key_mgmt 0x%x MIC",
			ver, sm->key_mgmt);
		goto out;
	}
	wpa_hexdump_key(MSG_DEBUG, "WPA: KCK", kck, kck_len);
	wpa_hexdump(MSG_DEBUG, "WPA: Derived Key MIC", key_mic, mic_len);
	wpa_hexdump(MSG_MSGDUMP, "WPA: TX EAPOL-Key", msg, msg_len);
	ret = wpa_sm_ether_send(sm, dest, proto, msg, msg_len);
	eapol_sm_notify_tx_eapol_key(sm->eapol);
out:
	os_free(msg);
	return ret;
}


/**
 * wpa_sm_key_request - Send EAPOL-Key Request
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @error: Indicate whether this is an Michael MIC error report
 * @pairwise: 1 = error report for pairwise packet, 0 = for group packet
 *
 * Send an EAPOL-Key Request to the current authenticator. This function is
 * used to request rekeying and it is usually called when a local Michael MIC
 * failure is detected.
 */
void wpa_sm_key_request(struct wpa_sm *sm, int error, int pairwise)
{
	size_t mic_len, hdrlen, rlen;
	struct wpa_eapol_key *reply;
	struct wpa_eapol_key_192 *reply192;
	int key_info, ver;
	u8 bssid[ETH_ALEN], *rbuf, *key_mic;

	if (sm->key_mgmt == WPA_KEY_MGMT_OSEN ||
	    wpa_key_mgmt_suite_b(sm->key_mgmt))
		ver = WPA_KEY_INFO_TYPE_AKM_DEFINED;
	else if (wpa_key_mgmt_ft(sm->key_mgmt) ||
		 wpa_key_mgmt_sha256(sm->key_mgmt))
		ver = WPA_KEY_INFO_TYPE_AES_128_CMAC;
	else if (sm->pairwise_cipher != WPA_CIPHER_TKIP)
		ver = WPA_KEY_INFO_TYPE_HMAC_SHA1_AES;
	else
		ver = WPA_KEY_INFO_TYPE_HMAC_MD5_RC4;

	if (wpa_sm_get_bssid(sm, bssid) < 0) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"Failed to read BSSID for EAPOL-Key request");
		return;
	}

	mic_len = wpa_mic_len(sm->key_mgmt);
	hdrlen = mic_len == 24 ? sizeof(*reply192) : sizeof(*reply);
	rbuf = wpa_sm_alloc_eapol(sm, IEEE802_1X_TYPE_EAPOL_KEY, NULL,
				  hdrlen, &rlen, (void *) &reply);
	if (rbuf == NULL)
		return;
	reply192 = (struct wpa_eapol_key_192 *) reply;

	reply->type = (sm->proto == WPA_PROTO_RSN ||
		       sm->proto == WPA_PROTO_OSEN) ?
		EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
	key_info = WPA_KEY_INFO_REQUEST | ver;
	if (sm->ptk_set)
		key_info |= WPA_KEY_INFO_MIC | WPA_KEY_INFO_SECURE;
	if (error)
		key_info |= WPA_KEY_INFO_ERROR;
	if (pairwise)
		key_info |= WPA_KEY_INFO_KEY_TYPE;
	WPA_PUT_BE16(reply->key_info, key_info);
	WPA_PUT_BE16(reply->key_length, 0);
	os_memcpy(reply->replay_counter, sm->request_counter,
		  WPA_REPLAY_COUNTER_LEN);
	inc_byte_array(sm->request_counter, WPA_REPLAY_COUNTER_LEN);

	if (mic_len == 24)
		WPA_PUT_BE16(reply192->key_data_length, 0);
	else
		WPA_PUT_BE16(reply->key_data_length, 0);
	if (!(key_info & WPA_KEY_INFO_MIC))
		key_mic = NULL;
	else
		key_mic = reply192->key_mic; /* same offset in reply */

	wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
		"WPA: Sending EAPOL-Key Request (error=%d "
		"pairwise=%d ptk_set=%d len=%lu)",
		error, pairwise, sm->ptk_set, (unsigned long) rlen);
	wpa_eapol_key_send(sm, sm->ptk.kck, sm->ptk.kck_len, ver, bssid,
			   ETH_P_EAPOL, rbuf, rlen, key_mic);
}


static void wpa_supplicant_key_mgmt_set_pmk(struct wpa_sm *sm)
{
#ifdef CONFIG_IEEE80211R
	if (sm->key_mgmt == WPA_KEY_MGMT_FT_IEEE8021X) {
		if (wpa_sm_key_mgmt_set_pmk(sm, sm->xxkey, sm->xxkey_len))
			wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
				"RSN: Cannot set low order 256 bits of MSK for key management offload");
	} else {
#endif /* CONFIG_IEEE80211R */
		if (wpa_sm_key_mgmt_set_pmk(sm, sm->pmk, sm->pmk_len))
			wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
				"RSN: Cannot set PMK for key management offload");
#ifdef CONFIG_IEEE80211R
	}
#endif /* CONFIG_IEEE80211R */
}


static int wpa_supplicant_get_pmk(struct wpa_sm *sm,
				  const unsigned char *src_addr,
				  const u8 *pmkid)
{
	int abort_cached = 0;

	if (pmkid && !sm->cur_pmksa) {
		/* When using drivers that generate RSN IE, wpa_supplicant may
		 * not have enough time to get the association information
		 * event before receiving this 1/4 message, so try to find a
		 * matching PMKSA cache entry here. */
		sm->cur_pmksa = pmksa_cache_get(sm->pmksa, src_addr, pmkid,
						NULL);
		if (sm->cur_pmksa) {
			wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
				"RSN: found matching PMKID from PMKSA cache");
		} else {
			wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
				"RSN: no matching PMKID found");
			abort_cached = 1;
		}
	}

	if (pmkid && sm->cur_pmksa &&
	    os_memcmp_const(pmkid, sm->cur_pmksa->pmkid, PMKID_LEN) == 0) {
		wpa_hexdump(MSG_DEBUG, "RSN: matched PMKID", pmkid, PMKID_LEN);
		wpa_sm_set_pmk_from_pmksa(sm);
		wpa_hexdump_key(MSG_DEBUG, "RSN: PMK from PMKSA cache",
				sm->pmk, sm->pmk_len);
		eapol_sm_notify_cached(sm->eapol);
#ifdef CONFIG_IEEE80211R
		sm->xxkey_len = 0;
#endif /* CONFIG_IEEE80211R */
	} else if (wpa_key_mgmt_wpa_ieee8021x(sm->key_mgmt) && sm->eapol) {
		int res, pmk_len;

		if (sm->key_mgmt & WPA_KEY_MGMT_IEEE8021X_SUITE_B_192)
			pmk_len = PMK_LEN_SUITE_B_192;
		else
			pmk_len = PMK_LEN;
		res = eapol_sm_get_key(sm->eapol, sm->pmk, pmk_len);
		if (res) {
			if (pmk_len == PMK_LEN) {
				/*
				 * EAP-LEAP is an exception from other EAP
				 * methods: it uses only 16-byte PMK.
				 */
				res = eapol_sm_get_key(sm->eapol, sm->pmk, 16);
				pmk_len = 16;
			}
		} else {
#ifdef CONFIG_IEEE80211R
			u8 buf[2 * PMK_LEN];
			if (eapol_sm_get_key(sm->eapol, buf, 2 * PMK_LEN) == 0)
			{
				os_memcpy(sm->xxkey, buf + PMK_LEN, PMK_LEN);
				sm->xxkey_len = PMK_LEN;
				os_memset(buf, 0, sizeof(buf));
			}
#endif /* CONFIG_IEEE80211R */
		}
		if (res == 0) {
			struct rsn_pmksa_cache_entry *sa = NULL;
			wpa_hexdump_key(MSG_DEBUG, "WPA: PMK from EAPOL state "
					"machines", sm->pmk, pmk_len);
			sm->pmk_len = pmk_len;
			wpa_supplicant_key_mgmt_set_pmk(sm);
			if (sm->proto == WPA_PROTO_RSN &&
			    !wpa_key_mgmt_suite_b(sm->key_mgmt) &&
			    !wpa_key_mgmt_ft(sm->key_mgmt)) {
				sa = pmksa_cache_add(sm->pmksa,
						     sm->pmk, pmk_len, NULL,
						     NULL, 0,
						     src_addr, sm->own_addr,
						     sm->network_ctx,
						     sm->key_mgmt);
			}
			if (!sm->cur_pmksa && pmkid &&
			    pmksa_cache_get(sm->pmksa, src_addr, pmkid, NULL))
			{
				wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
					"RSN: the new PMK matches with the "
					"PMKID");
				abort_cached = 0;
			} else if (sa && !sm->cur_pmksa && pmkid) {
				/*
				 * It looks like the authentication server
				 * derived mismatching MSK. This should not
				 * really happen, but bugs happen.. There is not
				 * much we can do here without knowing what
				 * exactly caused the server to misbehave.
				 */
				wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
					"RSN: PMKID mismatch - authentication server may have derived different MSK?!");
				return -1;
			}

			if (!sm->cur_pmksa)
				sm->cur_pmksa = sa;
		} else {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: Failed to get master session key from "
				"EAPOL state machines - key handshake "
				"aborted");
			if (sm->cur_pmksa) {
				wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
					"RSN: Cancelled PMKSA caching "
					"attempt");
				sm->cur_pmksa = NULL;
				abort_cached = 1;
			} else if (!abort_cached) {
				return -1;
			}
		}
	}

	if (abort_cached && wpa_key_mgmt_wpa_ieee8021x(sm->key_mgmt) &&
	    !wpa_key_mgmt_suite_b(sm->key_mgmt) &&
	    !wpa_key_mgmt_ft(sm->key_mgmt) && sm->key_mgmt != WPA_KEY_MGMT_OSEN)
	{
		/* Send EAPOL-Start to trigger full EAP authentication. */
		u8 *buf;
		size_t buflen;

		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"RSN: no PMKSA entry found - trigger "
			"full EAP authentication");
		buf = wpa_sm_alloc_eapol(sm, IEEE802_1X_TYPE_EAPOL_START,
					 NULL, 0, &buflen, NULL);
		if (buf) {
			wpa_sm_ether_send(sm, sm->bssid, ETH_P_EAPOL,
					  buf, buflen);
			os_free(buf);
			return -2;
		}

		return -1;
	}

	return 0;
}


/**
 * wpa_supplicant_send_2_of_4 - Send message 2 of WPA/RSN 4-Way Handshake
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @dst: Destination address for the frame
 * @key: Pointer to the EAPOL-Key frame header
 * @ver: Version bits from EAPOL-Key Key Info
 * @nonce: Nonce value for the EAPOL-Key frame
 * @wpa_ie: WPA/RSN IE
 * @wpa_ie_len: Length of the WPA/RSN IE
 * @ptk: PTK to use for keyed hash and encryption
 * Returns: >= 0 on success, < 0 on failure
 */
int wpa_supplicant_send_2_of_4(struct wpa_sm *sm, const unsigned char *dst,
			       const struct wpa_eapol_key *key,
			       int ver, const u8 *nonce,
			       const u8 *wpa_ie, size_t wpa_ie_len,
			       struct wpa_ptk *ptk)
{
	size_t mic_len, hdrlen, rlen;
	struct wpa_eapol_key *reply;
	struct wpa_eapol_key_192 *reply192;
	u8 *rbuf, *key_mic;
	u8 *rsn_ie_buf = NULL;

	if (wpa_ie == NULL) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING, "WPA: No wpa_ie set - "
			"cannot generate msg 2/4");
		return -1;
	}

#ifdef CONFIG_IEEE80211R
	if (wpa_key_mgmt_ft(sm->key_mgmt)) {
		int res;

		/*
		 * Add PMKR1Name into RSN IE (PMKID-List) and add MDIE and
		 * FTIE from (Re)Association Response.
		 */
		rsn_ie_buf = os_malloc(wpa_ie_len + 2 + 2 + PMKID_LEN +
				       sm->assoc_resp_ies_len);
		if (rsn_ie_buf == NULL)
			return -1;
		os_memcpy(rsn_ie_buf, wpa_ie, wpa_ie_len);
		res = wpa_insert_pmkid(rsn_ie_buf, &wpa_ie_len,
				       sm->pmk_r1_name);
		if (res < 0) {
			os_free(rsn_ie_buf);
			return -1;
		}

		if (sm->assoc_resp_ies) {
			os_memcpy(rsn_ie_buf + wpa_ie_len, sm->assoc_resp_ies,
				  sm->assoc_resp_ies_len);
			wpa_ie_len += sm->assoc_resp_ies_len;
		}

		wpa_ie = rsn_ie_buf;
	}
#endif /* CONFIG_IEEE80211R */

	wpa_hexdump(MSG_DEBUG, "WPA: WPA IE for msg 2/4", wpa_ie, wpa_ie_len);

	mic_len = wpa_mic_len(sm->key_mgmt);
	hdrlen = mic_len == 24 ? sizeof(*reply192) : sizeof(*reply);
	rbuf = wpa_sm_alloc_eapol(sm, IEEE802_1X_TYPE_EAPOL_KEY,
				  NULL, hdrlen + wpa_ie_len,
				  &rlen, (void *) &reply);
	if (rbuf == NULL) {
		os_free(rsn_ie_buf);
		return -1;
	}
	reply192 = (struct wpa_eapol_key_192 *) reply;

	reply->type = (sm->proto == WPA_PROTO_RSN ||
		       sm->proto == WPA_PROTO_OSEN) ?
		EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
	WPA_PUT_BE16(reply->key_info,
		     ver | WPA_KEY_INFO_KEY_TYPE | WPA_KEY_INFO_MIC);
	if (sm->proto == WPA_PROTO_RSN || sm->proto == WPA_PROTO_OSEN)
		WPA_PUT_BE16(reply->key_length, 0);
	else
		os_memcpy(reply->key_length, key->key_length, 2);
	os_memcpy(reply->replay_counter, key->replay_counter,
		  WPA_REPLAY_COUNTER_LEN);
	wpa_hexdump(MSG_DEBUG, "WPA: Replay Counter", reply->replay_counter,
		    WPA_REPLAY_COUNTER_LEN);

	key_mic = reply192->key_mic; /* same offset for reply and reply192 */
	if (mic_len == 24) {
		WPA_PUT_BE16(reply192->key_data_length, wpa_ie_len);
		os_memcpy(reply192 + 1, wpa_ie, wpa_ie_len);
	} else {
		WPA_PUT_BE16(reply->key_data_length, wpa_ie_len);
		os_memcpy(reply + 1, wpa_ie, wpa_ie_len);
	}
	os_free(rsn_ie_buf);

	os_memcpy(reply->key_nonce, nonce, WPA_NONCE_LEN);

	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: Sending EAPOL-Key 2/4");
	return wpa_eapol_key_send(sm, ptk->kck, ptk->kck_len, ver, dst,
				  ETH_P_EAPOL, rbuf, rlen, key_mic);
}


static int wpa_derive_ptk(struct wpa_sm *sm, const unsigned char *src_addr,
			  const struct wpa_eapol_key *key, struct wpa_ptk *ptk)
{
#ifdef CONFIG_IEEE80211R
	if (wpa_key_mgmt_ft(sm->key_mgmt))
		return wpa_derive_ptk_ft(sm, src_addr, key, ptk);
#endif /* CONFIG_IEEE80211R */

	return wpa_pmk_to_ptk(sm->pmk, sm->pmk_len, "Pairwise key expansion",
			      sm->own_addr, sm->bssid, sm->snonce,
			      key->key_nonce, ptk, sm->key_mgmt,
			      sm->pairwise_cipher);
}


static void wpa_supplicant_process_1_of_4(struct wpa_sm *sm,
					  const unsigned char *src_addr,
					  const struct wpa_eapol_key *key,
					  u16 ver, const u8 *key_data,
					  size_t key_data_len)
{
	struct wpa_eapol_ie_parse ie;
	struct wpa_ptk *ptk;
	int res;
	u8 *kde, *kde_buf = NULL;
	size_t kde_len;

	if (wpa_sm_get_network_ctx(sm) == NULL) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING, "WPA: No SSID info "
			"found (msg 1 of 4)");
		return;
	}

	wpa_sm_set_state(sm, WPA_4WAY_HANDSHAKE);
	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: RX message 1 of 4-Way "
		"Handshake from " MACSTR " (ver=%d)", MAC2STR(src_addr), ver);

	os_memset(&ie, 0, sizeof(ie));

	if (sm->proto == WPA_PROTO_RSN || sm->proto == WPA_PROTO_OSEN) {
		/* RSN: msg 1/4 should contain PMKID for the selected PMK */
		wpa_hexdump(MSG_DEBUG, "RSN: msg 1/4 key data",
			    key_data, key_data_len);
		if (wpa_supplicant_parse_ies(key_data, key_data_len, &ie) < 0)
			goto failed;
		if (ie.pmkid) {
			wpa_hexdump(MSG_DEBUG, "RSN: PMKID from "
				    "Authenticator", ie.pmkid, PMKID_LEN);
		}
	}

	res = wpa_supplicant_get_pmk(sm, src_addr, ie.pmkid);
	if (res == -2) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "RSN: Do not reply to "
			"msg 1/4 - requesting full EAP authentication");
		return;
	}
	if (res)
		goto failed;

	if (sm->renew_snonce) {
		if (random_get_bytes(sm->snonce, WPA_NONCE_LEN)) {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: Failed to get random data for SNonce");
			goto failed;
		}
		sm->renew_snonce = 0;
		wpa_hexdump(MSG_DEBUG, "WPA: Renewed SNonce",
			    sm->snonce, WPA_NONCE_LEN);
	}

	/* Calculate PTK which will be stored as a temporary PTK until it has
	 * been verified when processing message 3/4. */
	ptk = &sm->tptk;
	wpa_derive_ptk(sm, src_addr, key, ptk);
	if (sm->pairwise_cipher == WPA_CIPHER_TKIP) {
		u8 buf[8];
		/* Supplicant: swap tx/rx Mic keys */
		os_memcpy(buf, &ptk->tk[16], 8);
		os_memcpy(&ptk->tk[16], &ptk->tk[24], 8);
		os_memcpy(&ptk->tk[24], buf, 8);
		os_memset(buf, 0, sizeof(buf));
	}
	sm->tptk_set = 1;

	kde = sm->assoc_wpa_ie;
	kde_len = sm->assoc_wpa_ie_len;

#ifdef CONFIG_P2P
	if (sm->p2p) {
		kde_buf = os_malloc(kde_len + 2 + RSN_SELECTOR_LEN + 1);
		if (kde_buf) {
			u8 *pos;
			wpa_printf(MSG_DEBUG, "P2P: Add IP Address Request KDE "
				   "into EAPOL-Key 2/4");
			os_memcpy(kde_buf, kde, kde_len);
			kde = kde_buf;
			pos = kde + kde_len;
			*pos++ = WLAN_EID_VENDOR_SPECIFIC;
			*pos++ = RSN_SELECTOR_LEN + 1;
			RSN_SELECTOR_PUT(pos, WFA_KEY_DATA_IP_ADDR_REQ);
			pos += RSN_SELECTOR_LEN;
			*pos++ = 0x01;
			kde_len = pos - kde;
		}
	}
#endif /* CONFIG_P2P */

	if (wpa_supplicant_send_2_of_4(sm, sm->bssid, key, ver, sm->snonce,
				       kde, kde_len, ptk) < 0)
		goto failed;

	os_free(kde_buf);
	os_memcpy(sm->anonce, key->key_nonce, WPA_NONCE_LEN);
	return;

failed:
	os_free(kde_buf);
	wpa_sm_deauthenticate(sm, WLAN_REASON_UNSPECIFIED);
}


static void wpa_sm_start_preauth(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_sm *sm = eloop_ctx;
	rsn_preauth_candidate_process(sm);
}


static void wpa_supplicant_key_neg_complete(struct wpa_sm *sm,
					    const u8 *addr, int secure)
{
	wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
		"WPA: Key negotiation completed with "
		MACSTR " [PTK=%s GTK=%s]", MAC2STR(addr),
		wpa_cipher_txt(sm->pairwise_cipher),
		wpa_cipher_txt(sm->group_cipher));
	wpa_sm_cancel_auth_timeout(sm);
	wpa_sm_set_state(sm, WPA_COMPLETED);

	if (secure) {
		wpa_sm_mlme_setprotection(
			sm, addr, MLME_SETPROTECTION_PROTECT_TYPE_RX_TX,
			MLME_SETPROTECTION_KEY_TYPE_PAIRWISE);
		eapol_sm_notify_portValid(sm->eapol, TRUE);
		if (wpa_key_mgmt_wpa_psk(sm->key_mgmt))
			eapol_sm_notify_eap_success(sm->eapol, TRUE);
		/*
		 * Start preauthentication after a short wait to avoid a
		 * possible race condition between the data receive and key
		 * configuration after the 4-Way Handshake. This increases the
		 * likelihood of the first preauth EAPOL-Start frame getting to
		 * the target AP.
		 */
		eloop_register_timeout(1, 0, wpa_sm_start_preauth, sm, NULL);
	}

	if (sm->cur_pmksa && sm->cur_pmksa->opportunistic) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"RSN: Authenticator accepted "
			"opportunistic PMKSA entry - marking it valid");
		sm->cur_pmksa->opportunistic = 0;
	}

#ifdef CONFIG_IEEE80211R
	if (wpa_key_mgmt_ft(sm->key_mgmt)) {
		/* Prepare for the next transition */
		wpa_ft_prepare_auth_request(sm, NULL);
	}
#endif /* CONFIG_IEEE80211R */
}


static void wpa_sm_rekey_ptk(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_sm *sm = eloop_ctx;
	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: Request PTK rekeying");
	wpa_sm_key_request(sm, 0, 1);
}


static int wpa_supplicant_install_ptk(struct wpa_sm *sm,
				      const struct wpa_eapol_key *key)
{
	int keylen, rsclen;
	enum wpa_alg alg;
	const u8 *key_rsc;

	if (sm->ptk.installed) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"WPA: Do not re-install same PTK to the driver");
		return 0;
	}

	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
		"WPA: Installing PTK to the driver");

	if (sm->pairwise_cipher == WPA_CIPHER_NONE) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: Pairwise Cipher "
			"Suite: NONE - do not use pairwise keys");
		return 0;
	}

	if (!wpa_cipher_valid_pairwise(sm->pairwise_cipher)) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: Unsupported pairwise cipher %d",
			sm->pairwise_cipher);
		return -1;
	}

	alg = wpa_cipher_to_alg(sm->pairwise_cipher);
	keylen = wpa_cipher_key_len(sm->pairwise_cipher);
	rsclen = wpa_cipher_rsc_len(sm->pairwise_cipher);

	if (sm->proto == WPA_PROTO_RSN || sm->proto == WPA_PROTO_OSEN) {
		key_rsc = null_rsc;
	} else {
		key_rsc = key->key_rsc;
		wpa_hexdump(MSG_DEBUG, "WPA: RSC", key_rsc, rsclen);
	}

	if (wpa_sm_set_key(sm, alg, sm->bssid, 0, 1, key_rsc, rsclen,
			   sm->ptk.tk, keylen) < 0) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: Failed to set PTK to the "
			"driver (alg=%d keylen=%d bssid=" MACSTR ")",
			alg, keylen, MAC2STR(sm->bssid));
		return -1;
	}

	/* TK is not needed anymore in supplicant */
	os_memset(sm->ptk.tk, 0, WPA_TK_MAX_LEN);
	sm->ptk.installed = 1;

	if (sm->wpa_ptk_rekey) {
		eloop_cancel_timeout(wpa_sm_rekey_ptk, sm, NULL);
		eloop_register_timeout(sm->wpa_ptk_rekey, 0, wpa_sm_rekey_ptk,
				       sm, NULL);
	}

	return 0;
}


static int wpa_supplicant_check_group_cipher(struct wpa_sm *sm,
					     int group_cipher,
					     int keylen, int maxkeylen,
					     int *key_rsc_len,
					     enum wpa_alg *alg)
{
	int klen;

	*alg = wpa_cipher_to_alg(group_cipher);
	if (*alg == WPA_ALG_NONE) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: Unsupported Group Cipher %d",
			group_cipher);
		return -1;
	}
	*key_rsc_len = wpa_cipher_rsc_len(group_cipher);

	klen = wpa_cipher_key_len(group_cipher);
	if (keylen != klen || maxkeylen < klen) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: Unsupported %s Group Cipher key length %d (%d)",
			wpa_cipher_txt(group_cipher), keylen, maxkeylen);
		return -1;
	}
	return 0;
}


struct wpa_gtk_data {
	enum wpa_alg alg;
	int tx, key_rsc_len, keyidx;
	u8 gtk[32];
	int gtk_len;
};


static int wpa_supplicant_install_gtk(struct wpa_sm *sm,
				      const struct wpa_gtk_data *gd,
				      const u8 *key_rsc, int wnm_sleep)
{
	const u8 *_gtk = gd->gtk;
	u8 gtk_buf[32];

	/* Detect possible key reinstallation */
	if ((sm->gtk.gtk_len == (size_t) gd->gtk_len &&
	     os_memcmp(sm->gtk.gtk, gd->gtk, sm->gtk.gtk_len) == 0) ||
	    (sm->gtk_wnm_sleep.gtk_len == (size_t) gd->gtk_len &&
	     os_memcmp(sm->gtk_wnm_sleep.gtk, gd->gtk,
		       sm->gtk_wnm_sleep.gtk_len) == 0)) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"WPA: Not reinstalling already in-use GTK to the driver (keyidx=%d tx=%d len=%d)",
			gd->keyidx, gd->tx, gd->gtk_len);
		return 0;
	}

	wpa_hexdump_key(MSG_DEBUG, "WPA: Group Key", gd->gtk, gd->gtk_len);
	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
		"WPA: Installing GTK to the driver (keyidx=%d tx=%d len=%d)",
		gd->keyidx, gd->tx, gd->gtk_len);
	wpa_hexdump(MSG_DEBUG, "WPA: RSC", key_rsc, gd->key_rsc_len);
	if (sm->group_cipher == WPA_CIPHER_TKIP) {
		/* Swap Tx/Rx keys for Michael MIC */
		os_memcpy(gtk_buf, gd->gtk, 16);
		os_memcpy(gtk_buf + 16, gd->gtk + 24, 8);
		os_memcpy(gtk_buf + 24, gd->gtk + 16, 8);
		_gtk = gtk_buf;
	}
	if (sm->pairwise_cipher == WPA_CIPHER_NONE) {
		if (wpa_sm_set_key(sm, gd->alg, NULL,
				   gd->keyidx, 1, key_rsc, gd->key_rsc_len,
				   _gtk, gd->gtk_len) < 0) {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: Failed to set GTK to the driver "
				"(Group only)");
			os_memset(gtk_buf, 0, sizeof(gtk_buf));
			return -1;
		}
	} else if (wpa_sm_set_key(sm, gd->alg, broadcast_ether_addr,
				  gd->keyidx, gd->tx, key_rsc, gd->key_rsc_len,
				  _gtk, gd->gtk_len) < 0) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: Failed to set GTK to "
			"the driver (alg=%d keylen=%d keyidx=%d)",
			gd->alg, gd->gtk_len, gd->keyidx);
		os_memset(gtk_buf, 0, sizeof(gtk_buf));
		return -1;
	}
	os_memset(gtk_buf, 0, sizeof(gtk_buf));

	if (wnm_sleep) {
		sm->gtk_wnm_sleep.gtk_len = gd->gtk_len;
		os_memcpy(sm->gtk_wnm_sleep.gtk, gd->gtk,
			  sm->gtk_wnm_sleep.gtk_len);
	} else {
		sm->gtk.gtk_len = gd->gtk_len;
		os_memcpy(sm->gtk.gtk, gd->gtk, sm->gtk.gtk_len);
	}

	return 0;
}


static int wpa_supplicant_gtk_tx_bit_workaround(const struct wpa_sm *sm,
						int tx)
{
	if (tx && sm->pairwise_cipher != WPA_CIPHER_NONE) {
		/* Ignore Tx bit for GTK if a pairwise key is used. One AP
		 * seemed to set this bit (incorrectly, since Tx is only when
		 * doing Group Key only APs) and without this workaround, the
		 * data connection does not work because wpa_supplicant
		 * configured non-zero keyidx to be used for unicast. */
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"WPA: Tx bit set for GTK, but pairwise "
			"keys are used - ignore Tx bit");
		return 0;
	}
	return tx;
}


static int wpa_supplicant_rsc_relaxation(const struct wpa_sm *sm,
					 const u8 *rsc)
{
	int rsclen;

	if (!sm->wpa_rsc_relaxation)
		return 0;

	rsclen = wpa_cipher_rsc_len(sm->group_cipher);

	/*
	 * Try to detect RSC (endian) corruption issue where the AP sends
	 * the RSC bytes in EAPOL-Key message in the wrong order, both if
	 * it's actually a 6-byte field (as it should be) and if it treats
	 * it as an 8-byte field.
	 * An AP model known to have this bug is the Sapido RB-1632.
	 */
	if (rsclen == 6 && ((rsc[5] && !rsc[0]) || rsc[6] || rsc[7])) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"RSC %02x%02x%02x%02x%02x%02x%02x%02x is likely bogus, using 0",
			rsc[0], rsc[1], rsc[2], rsc[3],
			rsc[4], rsc[5], rsc[6], rsc[7]);

		return 1;
	}

	return 0;
}


static int wpa_supplicant_pairwise_gtk(struct wpa_sm *sm,
				       const struct wpa_eapol_key *key,
				       const u8 *gtk, size_t gtk_len,
				       int key_info)
{
	struct wpa_gtk_data gd;
	const u8 *key_rsc;

	/*
	 * IEEE Std 802.11i-2004 - 8.5.2 EAPOL-Key frames - Figure 43x
	 * GTK KDE format:
	 * KeyID[bits 0-1], Tx [bit 2], Reserved [bits 3-7]
	 * Reserved [bits 0-7]
	 * GTK
	 */

	os_memset(&gd, 0, sizeof(gd));
	wpa_hexdump_key(MSG_DEBUG, "RSN: received GTK in pairwise handshake",
			gtk, gtk_len);

	if (gtk_len < 2 || gtk_len - 2 > sizeof(gd.gtk))
		return -1;

	gd.keyidx = gtk[0] & 0x3;
	gd.tx = wpa_supplicant_gtk_tx_bit_workaround(sm,
						     !!(gtk[0] & BIT(2)));
	gtk += 2;
	gtk_len -= 2;

	os_memcpy(gd.gtk, gtk, gtk_len);
	gd.gtk_len = gtk_len;

	key_rsc = key->key_rsc;
	if (wpa_supplicant_rsc_relaxation(sm, key->key_rsc))
		key_rsc = null_rsc;

	if (sm->group_cipher != WPA_CIPHER_GTK_NOT_USED &&
	    (wpa_supplicant_check_group_cipher(sm, sm->group_cipher,
					       gtk_len, gtk_len,
					       &gd.key_rsc_len, &gd.alg) ||
	     wpa_supplicant_install_gtk(sm, &gd, key->key_rsc, 0))) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"RSN: Failed to install GTK");
		os_memset(&gd, 0, sizeof(gd));
		return -1;
	}
	os_memset(&gd, 0, sizeof(gd));

	wpa_supplicant_key_neg_complete(sm, sm->bssid,
					key_info & WPA_KEY_INFO_SECURE);
	return 0;
}


#ifdef CONFIG_IEEE80211W
static int wpa_supplicant_install_igtk(struct wpa_sm *sm,
				       const struct wpa_igtk_kde *igtk,
				       int wnm_sleep)
{
	size_t len = wpa_cipher_key_len(sm->mgmt_group_cipher);
	u16 keyidx = WPA_GET_LE16(igtk->keyid);

	/* Detect possible key reinstallation */
	if ((sm->igtk.igtk_len == len &&
	     os_memcmp(sm->igtk.igtk, igtk->igtk, sm->igtk.igtk_len) == 0) ||
	    (sm->igtk_wnm_sleep.igtk_len == len &&
	     os_memcmp(sm->igtk_wnm_sleep.igtk, igtk->igtk,
		       sm->igtk_wnm_sleep.igtk_len) == 0)) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"WPA: Not reinstalling already in-use IGTK to the driver (keyidx=%d)",
			keyidx);
		return  0;
	}

	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
		"WPA: IGTK keyid %d pn %02x%02x%02x%02x%02x%02x",
		keyidx, MAC2STR(igtk->pn));
	wpa_hexdump_key(MSG_DEBUG, "WPA: IGTK", igtk->igtk, len);
	if (keyidx > 4095) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: Invalid IGTK KeyID %d", keyidx);
		return -1;
	}
	if (wpa_sm_set_key(sm, wpa_cipher_to_alg(sm->mgmt_group_cipher),
			   broadcast_ether_addr,
			   keyidx, 0, igtk->pn, sizeof(igtk->pn),
			   igtk->igtk, len) < 0) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: Failed to configure IGTK to the driver");
		return -1;
	}

	if (wnm_sleep) {
		sm->igtk_wnm_sleep.igtk_len = len;
		os_memcpy(sm->igtk_wnm_sleep.igtk, igtk->igtk,
			  sm->igtk_wnm_sleep.igtk_len);
	} else {
		sm->igtk.igtk_len = len;
		os_memcpy(sm->igtk.igtk, igtk->igtk, sm->igtk.igtk_len);
	}

	return 0;
}
#endif /* CONFIG_IEEE80211W */


static int ieee80211w_set_keys(struct wpa_sm *sm,
			       struct wpa_eapol_ie_parse *ie)
{
#ifdef CONFIG_IEEE80211W
	if (!wpa_cipher_valid_mgmt_group(sm->mgmt_group_cipher))
		return 0;

	if (ie->igtk) {
		size_t len;
		const struct wpa_igtk_kde *igtk;

		len = wpa_cipher_key_len(sm->mgmt_group_cipher);
		if (ie->igtk_len != WPA_IGTK_KDE_PREFIX_LEN + len)
			return -1;

		igtk = (const struct wpa_igtk_kde *) ie->igtk;
		keyidx = WPA_GET_LE16(igtk->keyid);
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: IGTK keyid %d "
			"pn %02x%02x%02x%02x%02x%02x",
			keyidx, MAC2STR(igtk->pn));
		wpa_hexdump_key(MSG_DEBUG, "WPA: IGTK",
				igtk->igtk, len);
		if (keyidx > 4095) {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: Invalid IGTK KeyID %d", keyidx);
		if (wpa_supplicant_install_igtk(sm, igtk, 0) < 0)
			return -1;
	}

	return 0;
#else /* CONFIG_IEEE80211W */
	return 0;
#endif /* CONFIG_IEEE80211W */
}


static void wpa_report_ie_mismatch(struct wpa_sm *sm,
				   const char *reason, const u8 *src_addr,
				   const u8 *wpa_ie, size_t wpa_ie_len,
				   const u8 *rsn_ie, size_t rsn_ie_len)
{
	wpa_msg(sm->ctx->msg_ctx, MSG_WARNING, "WPA: %s (src=" MACSTR ")",
		reason, MAC2STR(src_addr));

	if (sm->ap_wpa_ie) {
		wpa_hexdump(MSG_INFO, "WPA: WPA IE in Beacon/ProbeResp",
			    sm->ap_wpa_ie, sm->ap_wpa_ie_len);
	}
	if (wpa_ie) {
		if (!sm->ap_wpa_ie) {
			wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
				"WPA: No WPA IE in Beacon/ProbeResp");
		}
		wpa_hexdump(MSG_INFO, "WPA: WPA IE in 3/4 msg",
			    wpa_ie, wpa_ie_len);
	}

	if (sm->ap_rsn_ie) {
		wpa_hexdump(MSG_INFO, "WPA: RSN IE in Beacon/ProbeResp",
			    sm->ap_rsn_ie, sm->ap_rsn_ie_len);
	}
	if (rsn_ie) {
		if (!sm->ap_rsn_ie) {
			wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
				"WPA: No RSN IE in Beacon/ProbeResp");
		}
		wpa_hexdump(MSG_INFO, "WPA: RSN IE in 3/4 msg",
			    rsn_ie, rsn_ie_len);
	}

	wpa_sm_deauthenticate(sm, WLAN_REASON_IE_IN_4WAY_DIFFERS);
}


#ifdef CONFIG_IEEE80211R

static int ft_validate_mdie(struct wpa_sm *sm,
			    const unsigned char *src_addr,
			    struct wpa_eapol_ie_parse *ie,
			    const u8 *assoc_resp_mdie)
{
	struct rsn_mdie *mdie;

	mdie = (struct rsn_mdie *) (ie->mdie + 2);
	if (ie->mdie == NULL || ie->mdie_len < 2 + sizeof(*mdie) ||
	    os_memcmp(mdie->mobility_domain, sm->mobility_domain,
		      MOBILITY_DOMAIN_ID_LEN) != 0) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "FT: MDIE in msg 3/4 did "
			"not match with the current mobility domain");
		return -1;
	}

	if (assoc_resp_mdie &&
	    (assoc_resp_mdie[1] != ie->mdie[1] ||
	     os_memcmp(assoc_resp_mdie, ie->mdie, 2 + ie->mdie[1]) != 0)) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "FT: MDIE mismatch");
		wpa_hexdump(MSG_DEBUG, "FT: MDIE in EAPOL-Key msg 3/4",
			    ie->mdie, 2 + ie->mdie[1]);
		wpa_hexdump(MSG_DEBUG, "FT: MDIE in (Re)Association Response",
			    assoc_resp_mdie, 2 + assoc_resp_mdie[1]);
		return -1;
	}

	return 0;
}


static int ft_validate_ftie(struct wpa_sm *sm,
			    const unsigned char *src_addr,
			    struct wpa_eapol_ie_parse *ie,
			    const u8 *assoc_resp_ftie)
{
	if (ie->ftie == NULL) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"FT: No FTIE in EAPOL-Key msg 3/4");
		return -1;
	}

	if (assoc_resp_ftie == NULL)
		return 0;

	if (assoc_resp_ftie[1] != ie->ftie[1] ||
	    os_memcmp(assoc_resp_ftie, ie->ftie, 2 + ie->ftie[1]) != 0) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "FT: FTIE mismatch");
		wpa_hexdump(MSG_DEBUG, "FT: FTIE in EAPOL-Key msg 3/4",
			    ie->ftie, 2 + ie->ftie[1]);
		wpa_hexdump(MSG_DEBUG, "FT: FTIE in (Re)Association Response",
			    assoc_resp_ftie, 2 + assoc_resp_ftie[1]);
		return -1;
	}

	return 0;
}


static int ft_validate_rsnie(struct wpa_sm *sm,
			     const unsigned char *src_addr,
			     struct wpa_eapol_ie_parse *ie)
{
	struct wpa_ie_data rsn;

	if (!ie->rsn_ie)
		return 0;

	/*
	 * Verify that PMKR1Name from EAPOL-Key message 3/4
	 * matches with the value we derived.
	 */
	if (wpa_parse_wpa_ie_rsn(ie->rsn_ie, ie->rsn_ie_len, &rsn) < 0 ||
	    rsn.num_pmkid != 1 || rsn.pmkid == NULL) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "FT: No PMKR1Name in "
			"FT 4-way handshake message 3/4");
		return -1;
	}

	if (os_memcmp_const(rsn.pmkid, sm->pmk_r1_name, WPA_PMK_NAME_LEN) != 0)
	{
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"FT: PMKR1Name mismatch in "
			"FT 4-way handshake message 3/4");
		wpa_hexdump(MSG_DEBUG, "FT: PMKR1Name from Authenticator",
			    rsn.pmkid, WPA_PMK_NAME_LEN);
		wpa_hexdump(MSG_DEBUG, "FT: Derived PMKR1Name",
			    sm->pmk_r1_name, WPA_PMK_NAME_LEN);
		return -1;
	}

	return 0;
}


static int wpa_supplicant_validate_ie_ft(struct wpa_sm *sm,
					 const unsigned char *src_addr,
					 struct wpa_eapol_ie_parse *ie)
{
	const u8 *pos, *end, *mdie = NULL, *ftie = NULL;

	if (sm->assoc_resp_ies) {
		pos = sm->assoc_resp_ies;
		end = pos + sm->assoc_resp_ies_len;
		while (end - pos > 2) {
			if (2 + pos[1] > end - pos)
				break;
			switch (*pos) {
			case WLAN_EID_MOBILITY_DOMAIN:
				mdie = pos;
				break;
			case WLAN_EID_FAST_BSS_TRANSITION:
				ftie = pos;
				break;
			}
			pos += 2 + pos[1];
		}
	}

	if (ft_validate_mdie(sm, src_addr, ie, mdie) < 0 ||
	    ft_validate_ftie(sm, src_addr, ie, ftie) < 0 ||
	    ft_validate_rsnie(sm, src_addr, ie) < 0)
		return -1;

	return 0;
}

#endif /* CONFIG_IEEE80211R */


static int wpa_supplicant_validate_ie(struct wpa_sm *sm,
				      const unsigned char *src_addr,
				      struct wpa_eapol_ie_parse *ie)
{
	if (sm->ap_wpa_ie == NULL && sm->ap_rsn_ie == NULL) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"WPA: No WPA/RSN IE for this AP known. "
			"Trying to get from scan results");
		if (wpa_sm_get_beacon_ie(sm) < 0) {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: Could not find AP from "
				"the scan results");
		} else {
			wpa_msg(sm->ctx->msg_ctx, MSG_DEBUG,
				"WPA: Found the current AP from "
				"updated scan results");
		}
	}

	if (ie->wpa_ie == NULL && ie->rsn_ie == NULL &&
	    (sm->ap_wpa_ie || sm->ap_rsn_ie)) {
		wpa_report_ie_mismatch(sm, "IE in 3/4 msg does not match "
				       "with IE in Beacon/ProbeResp (no IE?)",
				       src_addr, ie->wpa_ie, ie->wpa_ie_len,
				       ie->rsn_ie, ie->rsn_ie_len);
		return -1;
	}

	if ((ie->wpa_ie && sm->ap_wpa_ie &&
	     (ie->wpa_ie_len != sm->ap_wpa_ie_len ||
	      os_memcmp(ie->wpa_ie, sm->ap_wpa_ie, ie->wpa_ie_len) != 0)) ||
	    (ie->rsn_ie && sm->ap_rsn_ie &&
	     wpa_compare_rsn_ie(wpa_key_mgmt_ft(sm->key_mgmt),
				sm->ap_rsn_ie, sm->ap_rsn_ie_len,
				ie->rsn_ie, ie->rsn_ie_len))) {
		wpa_report_ie_mismatch(sm, "IE in 3/4 msg does not match "
				       "with IE in Beacon/ProbeResp",
				       src_addr, ie->wpa_ie, ie->wpa_ie_len,
				       ie->rsn_ie, ie->rsn_ie_len);
		return -1;
	}

	if (sm->proto == WPA_PROTO_WPA &&
	    ie->rsn_ie && sm->ap_rsn_ie == NULL && sm->rsn_enabled) {
		wpa_report_ie_mismatch(sm, "Possible downgrade attack "
				       "detected - RSN was enabled and RSN IE "
				       "was in msg 3/4, but not in "
				       "Beacon/ProbeResp",
				       src_addr, ie->wpa_ie, ie->wpa_ie_len,
				       ie->rsn_ie, ie->rsn_ie_len);
		return -1;
	}

#ifdef CONFIG_IEEE80211R
	if (wpa_key_mgmt_ft(sm->key_mgmt) &&
	    wpa_supplicant_validate_ie_ft(sm, src_addr, ie) < 0)
		return -1;
#endif /* CONFIG_IEEE80211R */

	return 0;
}


/**
 * wpa_supplicant_send_4_of_4 - Send message 4 of WPA/RSN 4-Way Handshake
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @dst: Destination address for the frame
 * @key: Pointer to the EAPOL-Key frame header
 * @ver: Version bits from EAPOL-Key Key Info
 * @key_info: Key Info
 * @ptk: PTK to use for keyed hash and encryption
 * Returns: >= 0 on success, < 0 on failure
 */
int wpa_supplicant_send_4_of_4(struct wpa_sm *sm, const unsigned char *dst,
			       const struct wpa_eapol_key *key,
			       u16 ver, u16 key_info,
			       struct wpa_ptk *ptk)
{
	size_t mic_len, hdrlen, rlen;
	struct wpa_eapol_key *reply;
	struct wpa_eapol_key_192 *reply192;
	u8 *rbuf, *key_mic;

	mic_len = wpa_mic_len(sm->key_mgmt);
	hdrlen = mic_len == 24 ? sizeof(*reply192) : sizeof(*reply);
	rbuf = wpa_sm_alloc_eapol(sm, IEEE802_1X_TYPE_EAPOL_KEY, NULL,
				  hdrlen, &rlen, (void *) &reply);
	if (rbuf == NULL)
		return -1;
	reply192 = (struct wpa_eapol_key_192 *) reply;

	reply->type = (sm->proto == WPA_PROTO_RSN ||
		       sm->proto == WPA_PROTO_OSEN) ?
		EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
	key_info &= WPA_KEY_INFO_SECURE;
	key_info |= ver | WPA_KEY_INFO_KEY_TYPE | WPA_KEY_INFO_MIC;
	WPA_PUT_BE16(reply->key_info, key_info);
	if (sm->proto == WPA_PROTO_RSN || sm->proto == WPA_PROTO_OSEN)
		WPA_PUT_BE16(reply->key_length, 0);
	else
		os_memcpy(reply->key_length, key->key_length, 2);
	os_memcpy(reply->replay_counter, key->replay_counter,
		  WPA_REPLAY_COUNTER_LEN);

	key_mic = reply192->key_mic; /* same offset for reply and reply192 */
	if (mic_len == 24)
		WPA_PUT_BE16(reply192->key_data_length, 0);
	else
		WPA_PUT_BE16(reply->key_data_length, 0);

	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: Sending EAPOL-Key 4/4");
	return wpa_eapol_key_send(sm, ptk->kck, ptk->kck_len, ver, dst,
				  ETH_P_EAPOL, rbuf, rlen, key_mic);
}


static void wpa_supplicant_process_3_of_4(struct wpa_sm *sm,
					  const struct wpa_eapol_key *key,
					  u16 ver, const u8 *key_data,
					  size_t key_data_len)
{
	u16 key_info, keylen;
	struct wpa_eapol_ie_parse ie;

	wpa_sm_set_state(sm, WPA_4WAY_HANDSHAKE);
	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: RX message 3 of 4-Way "
		"Handshake from " MACSTR " (ver=%d)", MAC2STR(sm->bssid), ver);

	key_info = WPA_GET_BE16(key->key_info);

	wpa_hexdump(MSG_DEBUG, "WPA: IE KeyData", key_data, key_data_len);
	if (wpa_supplicant_parse_ies(key_data, key_data_len, &ie) < 0)
		goto failed;
	if (ie.gtk && !(key_info & WPA_KEY_INFO_ENCR_KEY_DATA)) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: GTK IE in unencrypted key data");
		goto failed;
	}
#ifdef CONFIG_IEEE80211W
	if (ie.igtk && !(key_info & WPA_KEY_INFO_ENCR_KEY_DATA)) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: IGTK KDE in unencrypted key data");
		goto failed;
	}

	if (ie.igtk &&
	    wpa_cipher_valid_mgmt_group(sm->mgmt_group_cipher) &&
	    ie.igtk_len != WPA_IGTK_KDE_PREFIX_LEN +
	    (unsigned int) wpa_cipher_key_len(sm->mgmt_group_cipher)) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: Invalid IGTK KDE length %lu",
			(unsigned long) ie.igtk_len);
		goto failed;
	}
#endif /* CONFIG_IEEE80211W */

	if (wpa_supplicant_validate_ie(sm, sm->bssid, &ie) < 0)
		goto failed;

	if (os_memcmp(sm->anonce, key->key_nonce, WPA_NONCE_LEN) != 0) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: ANonce from message 1 of 4-Way Handshake "
			"differs from 3 of 4-Way Handshake - drop packet (src="
			MACSTR ")", MAC2STR(sm->bssid));
		goto failed;
	}

	keylen = WPA_GET_BE16(key->key_length);
	if (keylen != wpa_cipher_key_len(sm->pairwise_cipher)) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: Invalid %s key length %d (src=" MACSTR
			")", wpa_cipher_txt(sm->pairwise_cipher), keylen,
			MAC2STR(sm->bssid));
		goto failed;
	}

#ifdef CONFIG_P2P
	if (ie.ip_addr_alloc) {
		os_memcpy(sm->p2p_ip_addr, ie.ip_addr_alloc, 3 * 4);
		wpa_hexdump(MSG_DEBUG, "P2P: IP address info",
			    sm->p2p_ip_addr, sizeof(sm->p2p_ip_addr));
	}
#endif /* CONFIG_P2P */

	if (wpa_supplicant_send_4_of_4(sm, sm->bssid, key, ver, key_info,
				       &sm->ptk) < 0) {
		goto failed;
	}

	/* SNonce was successfully used in msg 3/4, so mark it to be renewed
	 * for the next 4-Way Handshake. If msg 3 is received again, the old
	 * SNonce will still be used to avoid changing PTK. */
	sm->renew_snonce = 1;

	if (key_info & WPA_KEY_INFO_INSTALL) {
		if (wpa_supplicant_install_ptk(sm, key))
			goto failed;
	}

	if (key_info & WPA_KEY_INFO_SECURE) {
		wpa_sm_mlme_setprotection(
			sm, sm->bssid, MLME_SETPROTECTION_PROTECT_TYPE_RX,
			MLME_SETPROTECTION_KEY_TYPE_PAIRWISE);
		eapol_sm_notify_portValid(sm->eapol, TRUE);
	}
	wpa_sm_set_state(sm, WPA_GROUP_HANDSHAKE);

	if (sm->group_cipher == WPA_CIPHER_GTK_NOT_USED) {
		wpa_supplicant_key_neg_complete(sm, sm->bssid,
						key_info & WPA_KEY_INFO_SECURE);
	} else if (ie.gtk &&
	    wpa_supplicant_pairwise_gtk(sm, key,
					ie.gtk, ie.gtk_len, key_info) < 0) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"RSN: Failed to configure GTK");
		goto failed;
	}

	if (ieee80211w_set_keys(sm, &ie) < 0) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"RSN: Failed to configure IGTK");
		goto failed;
	}

	if (ie.gtk)
		wpa_sm_set_rekey_offload(sm);

	if (sm->proto == WPA_PROTO_RSN && wpa_key_mgmt_suite_b(sm->key_mgmt)) {
		struct rsn_pmksa_cache_entry *sa;

		sa = pmksa_cache_add(sm->pmksa, sm->pmk, sm->pmk_len, NULL,
				     sm->ptk.kck, sm->ptk.kck_len,
				     sm->bssid, sm->own_addr,
				     sm->network_ctx, sm->key_mgmt);
		if (!sm->cur_pmksa)
			sm->cur_pmksa = sa;
	}

	sm->msg_3_of_4_ok = 1;
	return;

failed:
	wpa_sm_deauthenticate(sm, WLAN_REASON_UNSPECIFIED);
}


static int wpa_supplicant_process_1_of_2_rsn(struct wpa_sm *sm,
					     const u8 *keydata,
					     size_t keydatalen,
					     u16 key_info,
					     struct wpa_gtk_data *gd)
{
	int maxkeylen;
	struct wpa_eapol_ie_parse ie;

	wpa_hexdump(MSG_DEBUG, "RSN: msg 1/2 key data", keydata, keydatalen);
	if (wpa_supplicant_parse_ies(keydata, keydatalen, &ie) < 0)
		return -1;
	if (ie.gtk && !(key_info & WPA_KEY_INFO_ENCR_KEY_DATA)) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: GTK IE in unencrypted key data");
		return -1;
	}
	if (ie.gtk == NULL) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"WPA: No GTK IE in Group Key msg 1/2");
		return -1;
	}
	maxkeylen = gd->gtk_len = ie.gtk_len - 2;

	if (wpa_supplicant_check_group_cipher(sm, sm->group_cipher,
					      gd->gtk_len, maxkeylen,
					      &gd->key_rsc_len, &gd->alg))
		return -1;

	wpa_hexdump_key(MSG_DEBUG, "RSN: received GTK in group key handshake",
			ie.gtk, ie.gtk_len);
	gd->keyidx = ie.gtk[0] & 0x3;
	gd->tx = wpa_supplicant_gtk_tx_bit_workaround(sm,
						      !!(ie.gtk[0] & BIT(2)));
	if (ie.gtk_len - 2 > sizeof(gd->gtk)) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"RSN: Too long GTK in GTK IE (len=%lu)",
			(unsigned long) ie.gtk_len - 2);
		return -1;
	}
	os_memcpy(gd->gtk, ie.gtk + 2, ie.gtk_len - 2);

	if (ieee80211w_set_keys(sm, &ie) < 0)
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"RSN: Failed to configure IGTK");

	return 0;
}


static int wpa_supplicant_process_1_of_2_wpa(struct wpa_sm *sm,
					     const struct wpa_eapol_key *key,
					     const u8 *key_data,
					     size_t key_data_len, u16 key_info,
					     u16 ver, struct wpa_gtk_data *gd)
{
	size_t maxkeylen;
	u16 gtk_len;

	gtk_len = WPA_GET_BE16(key->key_length);
	maxkeylen = key_data_len;
	if (ver == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
		if (maxkeylen < 8) {
			wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
				"WPA: Too short maxkeylen (%lu)",
				(unsigned long) maxkeylen);
			return -1;
		}
		maxkeylen -= 8;
	}

	if (gtk_len > maxkeylen ||
	    wpa_supplicant_check_group_cipher(sm, sm->group_cipher,
					      gtk_len, maxkeylen,
					      &gd->key_rsc_len, &gd->alg))
		return -1;

	gd->gtk_len = gtk_len;
	gd->keyidx = (key_info & WPA_KEY_INFO_KEY_INDEX_MASK) >>
		WPA_KEY_INFO_KEY_INDEX_SHIFT;
	if (ver == WPA_KEY_INFO_TYPE_HMAC_MD5_RC4 && sm->ptk.kek_len == 16) {
#ifdef CONFIG_NO_RC4
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: RC4 not supported in the build");
		return -1;
#else /* CONFIG_NO_RC4 */
		u8 ek[32];
		if (key_data_len > sizeof(gd->gtk)) {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: RC4 key data too long (%lu)",
				(unsigned long) key_data_len);
			return -1;
		}
		os_memcpy(ek, key->key_iv, 16);
		os_memcpy(ek + 16, sm->ptk.kek, sm->ptk.kek_len);
		os_memcpy(gd->gtk, key_data, key_data_len);
		if (rc4_skip(ek, 32, 256, gd->gtk, key_data_len)) {
			os_memset(ek, 0, sizeof(ek));
			wpa_msg(sm->ctx->msg_ctx, MSG_ERROR,
				"WPA: RC4 failed");
			return -1;
		}
		os_memset(ek, 0, sizeof(ek));
#endif /* CONFIG_NO_RC4 */
	} else if (ver == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
		if (maxkeylen % 8) {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: Unsupported AES-WRAP len %lu",
				(unsigned long) maxkeylen);
			return -1;
		}
		if (maxkeylen > sizeof(gd->gtk)) {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: AES-WRAP key data "
				"too long (keydatalen=%lu maxkeylen=%lu)",
				(unsigned long) key_data_len,
				(unsigned long) maxkeylen);
			return -1;
		}
		if (aes_unwrap(sm->ptk.kek, sm->ptk.kek_len, maxkeylen / 8,
			       key_data, gd->gtk)) {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: AES unwrap failed - could not decrypt "
				"GTK");
			return -1;
		}
	} else {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: Unsupported key_info type %d", ver);
		return -1;
	}
	gd->tx = wpa_supplicant_gtk_tx_bit_workaround(
		sm, !!(key_info & WPA_KEY_INFO_TXRX));
	return 0;
}


static int wpa_supplicant_send_2_of_2(struct wpa_sm *sm,
				      const struct wpa_eapol_key *key,
				      int ver, u16 key_info)
{
	size_t mic_len, hdrlen, rlen;
	struct wpa_eapol_key *reply;
	struct wpa_eapol_key_192 *reply192;
	u8 *rbuf, *key_mic;

	mic_len = wpa_mic_len(sm->key_mgmt);
	hdrlen = mic_len == 24 ? sizeof(*reply192) : sizeof(*reply);
	rbuf = wpa_sm_alloc_eapol(sm, IEEE802_1X_TYPE_EAPOL_KEY, NULL,
				  hdrlen, &rlen, (void *) &reply);
	if (rbuf == NULL)
		return -1;
	reply192 = (struct wpa_eapol_key_192 *) reply;

	reply->type = (sm->proto == WPA_PROTO_RSN ||
		       sm->proto == WPA_PROTO_OSEN) ?
		EAPOL_KEY_TYPE_RSN : EAPOL_KEY_TYPE_WPA;
	key_info &= WPA_KEY_INFO_KEY_INDEX_MASK;
	key_info |= ver | WPA_KEY_INFO_MIC | WPA_KEY_INFO_SECURE;
	WPA_PUT_BE16(reply->key_info, key_info);
	if (sm->proto == WPA_PROTO_RSN || sm->proto == WPA_PROTO_OSEN)
		WPA_PUT_BE16(reply->key_length, 0);
	else
		os_memcpy(reply->key_length, key->key_length, 2);
	os_memcpy(reply->replay_counter, key->replay_counter,
		  WPA_REPLAY_COUNTER_LEN);

	key_mic = reply192->key_mic; /* same offset for reply and reply192 */
	if (mic_len == 24)
		WPA_PUT_BE16(reply192->key_data_length, 0);
	else
		WPA_PUT_BE16(reply->key_data_length, 0);

	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: Sending EAPOL-Key 2/2");
	return wpa_eapol_key_send(sm, sm->ptk.kck, sm->ptk.kck_len, ver,
				  sm->bssid, ETH_P_EAPOL, rbuf, rlen, key_mic);
}


static void wpa_supplicant_process_1_of_2(struct wpa_sm *sm,
					  const unsigned char *src_addr,
					  const struct wpa_eapol_key *key,
					  const u8 *key_data,
					  size_t key_data_len, u16 ver)
{
	u16 key_info;
	int rekey, ret;
	struct wpa_gtk_data gd;
	const u8 *key_rsc;

	if (!sm->msg_3_of_4_ok) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"WPA: Group Key Handshake started prior to completion of 4-way handshake");
		goto failed;
	}

	os_memset(&gd, 0, sizeof(gd));

	rekey = wpa_sm_get_state(sm) == WPA_COMPLETED;
	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: RX message 1 of Group Key "
		"Handshake from " MACSTR " (ver=%d)", MAC2STR(src_addr), ver);

	key_info = WPA_GET_BE16(key->key_info);

	if (sm->proto == WPA_PROTO_RSN || sm->proto == WPA_PROTO_OSEN) {
		ret = wpa_supplicant_process_1_of_2_rsn(sm, key_data,
							key_data_len, key_info,
							&gd);
	} else {
		ret = wpa_supplicant_process_1_of_2_wpa(sm, key, key_data,
							key_data_len,
							key_info, ver, &gd);
	}

	wpa_sm_set_state(sm, WPA_GROUP_HANDSHAKE);

	if (ret)
		goto failed;

	key_rsc = key->key_rsc;
	if (wpa_supplicant_rsc_relaxation(sm, key->key_rsc))
		key_rsc = null_rsc;

	if (wpa_supplicant_install_gtk(sm, &gd, key->key_rsc, 0) ||
	    wpa_supplicant_send_2_of_2(sm, key, ver, key_info))
		goto failed;
	os_memset(&gd, 0, sizeof(gd));

	if (rekey) {
		wpa_msg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: Group rekeying "
			"completed with " MACSTR " [GTK=%s]",
			MAC2STR(sm->bssid), wpa_cipher_txt(sm->group_cipher));
		wpa_sm_cancel_auth_timeout(sm);
		wpa_sm_set_state(sm, WPA_COMPLETED);
	} else {
		wpa_supplicant_key_neg_complete(sm, sm->bssid,
						key_info &
						WPA_KEY_INFO_SECURE);
	}

	wpa_sm_set_rekey_offload(sm);

	return;

failed:
	os_memset(&gd, 0, sizeof(gd));
	wpa_sm_deauthenticate(sm, WLAN_REASON_UNSPECIFIED);
}


static int wpa_supplicant_verify_eapol_key_mic(struct wpa_sm *sm,
					       struct wpa_eapol_key_192 *key,
					       u16 ver,
					       const u8 *buf, size_t len)
{
	u8 mic[WPA_EAPOL_KEY_MIC_MAX_LEN];
	int ok = 0;
	size_t mic_len = wpa_mic_len(sm->key_mgmt);

	os_memcpy(mic, key->key_mic, mic_len);
	if (sm->tptk_set) {
		os_memset(key->key_mic, 0, mic_len);
		wpa_eapol_key_mic(sm->tptk.kck, sm->tptk.kck_len, sm->key_mgmt,
				  ver, buf, len, key->key_mic);
		if (os_memcmp_const(mic, key->key_mic, mic_len) != 0) {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: Invalid EAPOL-Key MIC "
				"when using TPTK - ignoring TPTK");
		} else {
			ok = 1;
			sm->tptk_set = 0;
			sm->ptk_set = 1;
			os_memcpy(&sm->ptk, &sm->tptk, sizeof(sm->ptk));
			os_memset(&sm->tptk, 0, sizeof(sm->tptk));
		}
	}

	if (!ok && sm->ptk_set) {
		os_memset(key->key_mic, 0, mic_len);
		wpa_eapol_key_mic(sm->ptk.kck, sm->ptk.kck_len, sm->key_mgmt,
				  ver, buf, len, key->key_mic);
		if (os_memcmp_const(mic, key->key_mic, mic_len) != 0) {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: Invalid EAPOL-Key MIC - "
				"dropping packet");
			return -1;
		}
		ok = 1;
	}

	if (!ok) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: Could not verify EAPOL-Key MIC - "
			"dropping packet");
		return -1;
	}

	os_memcpy(sm->rx_replay_counter, key->replay_counter,
		  WPA_REPLAY_COUNTER_LEN);
	sm->rx_replay_counter_set = 1;
	return 0;
}


/* Decrypt RSN EAPOL-Key key data (RC4 or AES-WRAP) */
static int wpa_supplicant_decrypt_key_data(struct wpa_sm *sm,
					   struct wpa_eapol_key *key, u16 ver,
					   u8 *key_data, size_t *key_data_len)
{
	wpa_hexdump(MSG_DEBUG, "RSN: encrypted key data",
		    key_data, *key_data_len);
	if (!sm->ptk_set) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: PTK not available, cannot decrypt EAPOL-Key Key "
			"Data");
		return -1;
	}

	/* Decrypt key data here so that this operation does not need
	 * to be implemented separately for each message type. */
	if (ver == WPA_KEY_INFO_TYPE_HMAC_MD5_RC4 && sm->ptk.kek_len == 16) {
#ifdef CONFIG_NO_RC4
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: RC4 not supported in the build");
		return -1;
#else /* CONFIG_NO_RC4 */
		u8 ek[32];
		os_memcpy(ek, key->key_iv, 16);
		os_memcpy(ek + 16, sm->ptk.kek, sm->ptk.kek_len);
		if (rc4_skip(ek, 32, 256, key_data, *key_data_len)) {
			os_memset(ek, 0, sizeof(ek));
			wpa_msg(sm->ctx->msg_ctx, MSG_ERROR,
				"WPA: RC4 failed");
			return -1;
		}
		os_memset(ek, 0, sizeof(ek));
#endif /* CONFIG_NO_RC4 */
	} else if (ver == WPA_KEY_INFO_TYPE_HMAC_SHA1_AES ||
		   ver == WPA_KEY_INFO_TYPE_AES_128_CMAC ||
		   sm->key_mgmt == WPA_KEY_MGMT_OSEN ||
		   wpa_key_mgmt_suite_b(sm->key_mgmt)) {
		u8 *buf;
		if (*key_data_len < 8 || *key_data_len % 8) {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: Unsupported AES-WRAP len %u",
				(unsigned int) *key_data_len);
			return -1;
		}
		*key_data_len -= 8; /* AES-WRAP adds 8 bytes */
		buf = os_malloc(*key_data_len);
		if (buf == NULL) {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: No memory for AES-UNWRAP buffer");
			return -1;
		}
		if (aes_unwrap(sm->ptk.kek, sm->ptk.kek_len, *key_data_len / 8,
			       key_data, buf)) {
			bin_clear_free(buf, *key_data_len);
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: AES unwrap failed - "
				"could not decrypt EAPOL-Key key data");
			return -1;
		}
		os_memcpy(key_data, buf, *key_data_len);
		bin_clear_free(buf, *key_data_len);
		WPA_PUT_BE16(key->key_data_length, *key_data_len);
	} else {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: Unsupported key_info type %d", ver);
		return -1;
	}
	wpa_hexdump_key(MSG_DEBUG, "WPA: decrypted EAPOL-Key key data",
			key_data, *key_data_len);
	return 0;
}


/**
 * wpa_sm_aborted_cached - Notify WPA that PMKSA caching was aborted
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 */
void wpa_sm_aborted_cached(struct wpa_sm *sm)
{
	if (sm && sm->cur_pmksa) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"RSN: Cancelling PMKSA caching attempt");
		sm->cur_pmksa = NULL;
	}
}


static void wpa_eapol_key_dump(struct wpa_sm *sm,
			       const struct wpa_eapol_key *key,
			       unsigned int key_data_len,
			       const u8 *mic, unsigned int mic_len)
{
#ifndef CONFIG_NO_STDOUT_DEBUG
	u16 key_info = WPA_GET_BE16(key->key_info);

	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "  EAPOL-Key type=%d", key->type);
	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
		"  key_info 0x%x (ver=%d keyidx=%d rsvd=%d %s%s%s%s%s%s%s%s)",
		key_info, key_info & WPA_KEY_INFO_TYPE_MASK,
		(key_info & WPA_KEY_INFO_KEY_INDEX_MASK) >>
		WPA_KEY_INFO_KEY_INDEX_SHIFT,
		(key_info & (BIT(13) | BIT(14) | BIT(15))) >> 13,
		key_info & WPA_KEY_INFO_KEY_TYPE ? "Pairwise" : "Group",
		key_info & WPA_KEY_INFO_INSTALL ? " Install" : "",
		key_info & WPA_KEY_INFO_ACK ? " Ack" : "",
		key_info & WPA_KEY_INFO_MIC ? " MIC" : "",
		key_info & WPA_KEY_INFO_SECURE ? " Secure" : "",
		key_info & WPA_KEY_INFO_ERROR ? " Error" : "",
		key_info & WPA_KEY_INFO_REQUEST ? " Request" : "",
		key_info & WPA_KEY_INFO_ENCR_KEY_DATA ? " Encr" : "");
	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
		"  key_length=%u key_data_length=%u",
		WPA_GET_BE16(key->key_length), key_data_len);
	wpa_hexdump(MSG_DEBUG, "  replay_counter",
		    key->replay_counter, WPA_REPLAY_COUNTER_LEN);
	wpa_hexdump(MSG_DEBUG, "  key_nonce", key->key_nonce, WPA_NONCE_LEN);
	wpa_hexdump(MSG_DEBUG, "  key_iv", key->key_iv, 16);
	wpa_hexdump(MSG_DEBUG, "  key_rsc", key->key_rsc, 8);
	wpa_hexdump(MSG_DEBUG, "  key_id (reserved)", key->key_id, 8);
	wpa_hexdump(MSG_DEBUG, "  key_mic", mic, mic_len);
#endif /* CONFIG_NO_STDOUT_DEBUG */
}


/**
 * wpa_sm_rx_eapol - Process received WPA EAPOL frames
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @src_addr: Source MAC address of the EAPOL packet
 * @buf: Pointer to the beginning of the EAPOL data (EAPOL header)
 * @len: Length of the EAPOL frame
 * Returns: 1 = WPA EAPOL-Key processed, 0 = not a WPA EAPOL-Key, -1 failure
 *
 * This function is called for each received EAPOL frame. Other than EAPOL-Key
 * frames can be skipped if filtering is done elsewhere. wpa_sm_rx_eapol() is
 * only processing WPA and WPA2 EAPOL-Key frames.
 *
 * The received EAPOL-Key packets are validated and valid packets are replied
 * to. In addition, key material (PTK, GTK) is configured at the end of a
 * successful key handshake.
 */
int wpa_sm_rx_eapol(struct wpa_sm *sm, const u8 *src_addr,
		    const u8 *buf, size_t len)
{
	size_t plen, data_len, key_data_len;
	const struct ieee802_1x_hdr *hdr;
	struct wpa_eapol_key *key;
	struct wpa_eapol_key_192 *key192;
	u16 key_info, ver;
	u8 *tmp = NULL;
	int ret = -1;
	struct wpa_peerkey *peerkey = NULL;
	u8 *key_data;
	size_t mic_len, keyhdrlen;

#ifdef CONFIG_IEEE80211R
	sm->ft_completed = 0;
#endif /* CONFIG_IEEE80211R */

	mic_len = wpa_mic_len(sm->key_mgmt);
	keyhdrlen = mic_len == 24 ? sizeof(*key192) : sizeof(*key);

	if (len < sizeof(*hdr) + keyhdrlen) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"WPA: EAPOL frame too short to be a WPA "
			"EAPOL-Key (len %lu, expecting at least %lu)",
			(unsigned long) len,
			(unsigned long) sizeof(*hdr) + keyhdrlen);
		return 0;
	}

	hdr = (const struct ieee802_1x_hdr *) buf;
	plen = be_to_host16(hdr->length);
	data_len = plen + sizeof(*hdr);
	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
		"IEEE 802.1X RX: version=%d type=%d length=%lu",
		hdr->version, hdr->type, (unsigned long) plen);

	if (hdr->version < EAPOL_VERSION) {
		/* TODO: backwards compatibility */
	}
	if (hdr->type != IEEE802_1X_TYPE_EAPOL_KEY) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"WPA: EAPOL frame (type %u) discarded, "
			"not a Key frame", hdr->type);
		ret = 0;
		goto out;
	}
	wpa_hexdump(MSG_MSGDUMP, "WPA: RX EAPOL-Key", buf, len);
	if (plen > len - sizeof(*hdr) || plen < keyhdrlen) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"WPA: EAPOL frame payload size %lu "
			"invalid (frame size %lu)",
			(unsigned long) plen, (unsigned long) len);
		ret = 0;
		goto out;
	}
	if (data_len < len) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"WPA: ignoring %lu bytes after the IEEE 802.1X data",
			(unsigned long) len - data_len);
	}

	/*
	 * Make a copy of the frame since we need to modify the buffer during
	 * MAC validation and Key Data decryption.
	 */
	tmp = os_malloc(data_len);
	if (tmp == NULL)
		goto out;
	os_memcpy(tmp, buf, data_len);
	key = (struct wpa_eapol_key *) (tmp + sizeof(struct ieee802_1x_hdr));
	key192 = (struct wpa_eapol_key_192 *)
		(tmp + sizeof(struct ieee802_1x_hdr));
	if (mic_len == 24)
		key_data = (u8 *) (key192 + 1);
	else
		key_data = (u8 *) (key + 1);

	if (key->type != EAPOL_KEY_TYPE_WPA && key->type != EAPOL_KEY_TYPE_RSN)
	{
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"WPA: EAPOL-Key type (%d) unknown, discarded",
			key->type);
		ret = 0;
		goto out;
	}

	if (mic_len == 24)
		key_data_len = WPA_GET_BE16(key192->key_data_length);
	else
		key_data_len = WPA_GET_BE16(key->key_data_length);
	wpa_eapol_key_dump(sm, key, key_data_len, key192->key_mic, mic_len);

	if (key_data_len > plen - keyhdrlen) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO, "WPA: Invalid EAPOL-Key "
			"frame - key_data overflow (%u > %u)",
			(unsigned int) key_data_len,
			(unsigned int) (plen - keyhdrlen));
		goto out;
	}

	eapol_sm_notify_lower_layer_success(sm->eapol, 0);
	key_info = WPA_GET_BE16(key->key_info);
	ver = key_info & WPA_KEY_INFO_TYPE_MASK;
	if (ver != WPA_KEY_INFO_TYPE_HMAC_MD5_RC4 &&
#if defined(CONFIG_IEEE80211R) || defined(CONFIG_IEEE80211W)
	    ver != WPA_KEY_INFO_TYPE_AES_128_CMAC &&
#endif /* CONFIG_IEEE80211R || CONFIG_IEEE80211W */
	    ver != WPA_KEY_INFO_TYPE_HMAC_SHA1_AES &&
	    !wpa_key_mgmt_suite_b(sm->key_mgmt) &&
	    sm->key_mgmt != WPA_KEY_MGMT_OSEN) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"WPA: Unsupported EAPOL-Key descriptor version %d",
			ver);
		goto out;
	}

	if (sm->key_mgmt == WPA_KEY_MGMT_OSEN &&
	    ver != WPA_KEY_INFO_TYPE_AKM_DEFINED) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"OSEN: Unsupported EAPOL-Key descriptor version %d",
			ver);
		goto out;
	}

	if (wpa_key_mgmt_suite_b(sm->key_mgmt) &&
	    ver != WPA_KEY_INFO_TYPE_AKM_DEFINED) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"RSN: Unsupported EAPOL-Key descriptor version %d (expected AKM defined = 0)",
			ver);
		goto out;
	}

#ifdef CONFIG_IEEE80211R
	if (wpa_key_mgmt_ft(sm->key_mgmt)) {
		/* IEEE 802.11r uses a new key_info type (AES-128-CMAC). */
		if (ver != WPA_KEY_INFO_TYPE_AES_128_CMAC) {
			wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
				"FT: AP did not use AES-128-CMAC");
			goto out;
		}
	} else
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_IEEE80211W
	if (wpa_key_mgmt_sha256(sm->key_mgmt)) {
		if (ver != WPA_KEY_INFO_TYPE_AES_128_CMAC &&
		    sm->key_mgmt != WPA_KEY_MGMT_OSEN &&
		    !wpa_key_mgmt_suite_b(sm->key_mgmt)) {
			wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
				"WPA: AP did not use the "
				"negotiated AES-128-CMAC");
			goto out;
		}
	} else
#endif /* CONFIG_IEEE80211W */
	if (sm->pairwise_cipher == WPA_CIPHER_CCMP &&
	    !wpa_key_mgmt_suite_b(sm->key_mgmt) &&
	    ver != WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"WPA: CCMP is used, but EAPOL-Key "
			"descriptor version (%d) is not 2", ver);
		if (sm->group_cipher != WPA_CIPHER_CCMP &&
		    !(key_info & WPA_KEY_INFO_KEY_TYPE)) {
			/* Earlier versions of IEEE 802.11i did not explicitly
			 * require version 2 descriptor for all EAPOL-Key
			 * packets, so allow group keys to use version 1 if
			 * CCMP is not used for them. */
			wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
				"WPA: Backwards compatibility: allow invalid "
				"version for non-CCMP group keys");
		} else if (ver == WPA_KEY_INFO_TYPE_AES_128_CMAC) {
			wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
				"WPA: Interoperability workaround: allow incorrect (should have been HMAC-SHA1), but stronger (is AES-128-CMAC), descriptor version to be used");
		} else
			goto out;
	} else if (sm->pairwise_cipher == WPA_CIPHER_GCMP &&
		   !wpa_key_mgmt_suite_b(sm->key_mgmt) &&
		   ver != WPA_KEY_INFO_TYPE_HMAC_SHA1_AES) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"WPA: GCMP is used, but EAPOL-Key "
			"descriptor version (%d) is not 2", ver);
		goto out;
	}

#ifdef CONFIG_PEERKEY
	for (peerkey = sm->peerkey; peerkey; peerkey = peerkey->next) {
		if (os_memcmp(peerkey->addr, src_addr, ETH_ALEN) == 0)
			break;
	}

	if (!(key_info & WPA_KEY_INFO_SMK_MESSAGE) && peerkey) {
		if (!peerkey->initiator && peerkey->replay_counter_set &&
		    os_memcmp(key->replay_counter, peerkey->replay_counter,
			      WPA_REPLAY_COUNTER_LEN) <= 0) {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"RSN: EAPOL-Key Replay Counter did not "
				"increase (STK) - dropping packet");
			goto out;
		} else if (peerkey->initiator) {
			u8 _tmp[WPA_REPLAY_COUNTER_LEN];
			os_memcpy(_tmp, key->replay_counter,
				  WPA_REPLAY_COUNTER_LEN);
			inc_byte_array(_tmp, WPA_REPLAY_COUNTER_LEN);
			if (os_memcmp(_tmp, peerkey->replay_counter,
				      WPA_REPLAY_COUNTER_LEN) != 0) {
				wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
					"RSN: EAPOL-Key Replay "
					"Counter did not match (STK) - "
					"dropping packet");
				goto out;
			}
		}
	}

	if (peerkey && peerkey->initiator && (key_info & WPA_KEY_INFO_ACK)) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"RSN: Ack bit in key_info from STK peer");
		goto out;
	}
#endif /* CONFIG_PEERKEY */

	if (!peerkey && sm->rx_replay_counter_set &&
	    os_memcmp(key->replay_counter, sm->rx_replay_counter,
		      WPA_REPLAY_COUNTER_LEN) <= 0) {
		wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
			"WPA: EAPOL-Key Replay Counter did not increase - "
			"dropping packet");
		goto out;
	}

	if (!(key_info & (WPA_KEY_INFO_ACK | WPA_KEY_INFO_SMK_MESSAGE))
#ifdef CONFIG_PEERKEY
	    && (peerkey == NULL || !peerkey->initiator)
#endif /* CONFIG_PEERKEY */
		) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"WPA: No Ack bit in key_info");
		goto out;
	}

	if (key_info & WPA_KEY_INFO_REQUEST) {
		wpa_msg(sm->ctx->msg_ctx, MSG_INFO,
			"WPA: EAPOL-Key with Request bit - dropped");
		goto out;
	}

	if ((key_info & WPA_KEY_INFO_MIC) && !peerkey &&
	    wpa_supplicant_verify_eapol_key_mic(sm, key192, ver, tmp, data_len))
		goto out;

#ifdef CONFIG_PEERKEY
	if ((key_info & WPA_KEY_INFO_MIC) && peerkey &&
	    peerkey_verify_eapol_key_mic(sm, peerkey, key192, ver, tmp,
					 data_len))
		goto out;
#endif /* CONFIG_PEERKEY */

	if ((sm->proto == WPA_PROTO_RSN || sm->proto == WPA_PROTO_OSEN) &&
	    (key_info & WPA_KEY_INFO_ENCR_KEY_DATA)) {
		if (wpa_supplicant_decrypt_key_data(sm, key, ver, key_data,
						    &key_data_len))
			goto out;
	}

	if (key_info & WPA_KEY_INFO_KEY_TYPE) {
		if (key_info & WPA_KEY_INFO_KEY_INDEX_MASK) {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: Ignored EAPOL-Key (Pairwise) with "
				"non-zero key index");
			goto out;
		}
		if (peerkey) {
			/* PeerKey 4-Way Handshake */
			peerkey_rx_eapol_4way(sm, peerkey, key, key_info, ver,
					      key_data, key_data_len);
		} else if (key_info & WPA_KEY_INFO_MIC) {
			/* 3/4 4-Way Handshake */
			wpa_supplicant_process_3_of_4(sm, key, ver, key_data,
						      key_data_len);
		} else {
			/* 1/4 4-Way Handshake */
			wpa_supplicant_process_1_of_4(sm, src_addr, key,
						      ver, key_data,
						      key_data_len);
		}
	} else if (key_info & WPA_KEY_INFO_SMK_MESSAGE) {
		/* PeerKey SMK Handshake */
		peerkey_rx_eapol_smk(sm, src_addr, key, key_data_len, key_info,
				     ver);
	} else {
		if (key_info & WPA_KEY_INFO_MIC) {
			/* 1/2 Group Key Handshake */
			wpa_supplicant_process_1_of_2(sm, src_addr, key,
						      key_data, key_data_len,
						      ver);
		} else {
			wpa_msg(sm->ctx->msg_ctx, MSG_WARNING,
				"WPA: EAPOL-Key (Group) without Mic bit - "
				"dropped");
		}
	}

	ret = 1;

out:
	bin_clear_free(tmp, data_len);
	return ret;
}


#ifdef CONFIG_CTRL_IFACE
static u32 wpa_key_mgmt_suite(struct wpa_sm *sm)
{
	switch (sm->key_mgmt) {
	case WPA_KEY_MGMT_IEEE8021X:
		return ((sm->proto == WPA_PROTO_RSN ||
			 sm->proto == WPA_PROTO_OSEN) ?
			RSN_AUTH_KEY_MGMT_UNSPEC_802_1X :
			WPA_AUTH_KEY_MGMT_UNSPEC_802_1X);
	case WPA_KEY_MGMT_PSK:
		return (sm->proto == WPA_PROTO_RSN ?
			RSN_AUTH_KEY_MGMT_PSK_OVER_802_1X :
			WPA_AUTH_KEY_MGMT_PSK_OVER_802_1X);
#ifdef CONFIG_IEEE80211R
	case WPA_KEY_MGMT_FT_IEEE8021X:
		return RSN_AUTH_KEY_MGMT_FT_802_1X;
	case WPA_KEY_MGMT_FT_PSK:
		return RSN_AUTH_KEY_MGMT_FT_PSK;
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_IEEE80211W
	case WPA_KEY_MGMT_IEEE8021X_SHA256:
		return RSN_AUTH_KEY_MGMT_802_1X_SHA256;
	case WPA_KEY_MGMT_PSK_SHA256:
		return RSN_AUTH_KEY_MGMT_PSK_SHA256;
#endif /* CONFIG_IEEE80211W */
	case WPA_KEY_MGMT_CCKM:
		return (sm->proto == WPA_PROTO_RSN ?
			RSN_AUTH_KEY_MGMT_CCKM:
			WPA_AUTH_KEY_MGMT_CCKM);
	case WPA_KEY_MGMT_WPA_NONE:
		return WPA_AUTH_KEY_MGMT_NONE;
	case WPA_KEY_MGMT_IEEE8021X_SUITE_B:
		return RSN_AUTH_KEY_MGMT_802_1X_SUITE_B;
	case WPA_KEY_MGMT_IEEE8021X_SUITE_B_192:
		return RSN_AUTH_KEY_MGMT_802_1X_SUITE_B_192;
	default:
		return 0;
	}
}


#define RSN_SUITE "%02x-%02x-%02x-%d"
#define RSN_SUITE_ARG(s) \
((s) >> 24) & 0xff, ((s) >> 16) & 0xff, ((s) >> 8) & 0xff, (s) & 0xff

/**
 * wpa_sm_get_mib - Dump text list of MIB entries
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @buf: Buffer for the list
 * @buflen: Length of the buffer
 * Returns: Number of bytes written to buffer
 *
 * This function is used fetch dot11 MIB variables.
 */
int wpa_sm_get_mib(struct wpa_sm *sm, char *buf, size_t buflen)
{
	char pmkid_txt[PMKID_LEN * 2 + 1];
	int rsna, ret;
	size_t len;

	if (sm->cur_pmksa) {
		wpa_snprintf_hex(pmkid_txt, sizeof(pmkid_txt),
				 sm->cur_pmksa->pmkid, PMKID_LEN);
	} else
		pmkid_txt[0] = '\0';

	if ((wpa_key_mgmt_wpa_psk(sm->key_mgmt) ||
	     wpa_key_mgmt_wpa_ieee8021x(sm->key_mgmt)) &&
	    sm->proto == WPA_PROTO_RSN)
		rsna = 1;
	else
		rsna = 0;

	ret = os_snprintf(buf, buflen,
			  "dot11RSNAOptionImplemented=TRUE\n"
			  "dot11RSNAPreauthenticationImplemented=TRUE\n"
			  "dot11RSNAEnabled=%s\n"
			  "dot11RSNAPreauthenticationEnabled=%s\n"
			  "dot11RSNAConfigVersion=%d\n"
			  "dot11RSNAConfigPairwiseKeysSupported=5\n"
			  "dot11RSNAConfigGroupCipherSize=%d\n"
			  "dot11RSNAConfigPMKLifetime=%d\n"
			  "dot11RSNAConfigPMKReauthThreshold=%d\n"
			  "dot11RSNAConfigNumberOfPTKSAReplayCounters=1\n"
			  "dot11RSNAConfigSATimeout=%d\n",
			  rsna ? "TRUE" : "FALSE",
			  rsna ? "TRUE" : "FALSE",
			  RSN_VERSION,
			  wpa_cipher_key_len(sm->group_cipher) * 8,
			  sm->dot11RSNAConfigPMKLifetime,
			  sm->dot11RSNAConfigPMKReauthThreshold,
			  sm->dot11RSNAConfigSATimeout);
	if (os_snprintf_error(buflen, ret))
		return 0;
	len = ret;

	ret = os_snprintf(
		buf + len, buflen - len,
		"dot11RSNAAuthenticationSuiteSelected=" RSN_SUITE "\n"
		"dot11RSNAPairwiseCipherSelected=" RSN_SUITE "\n"
		"dot11RSNAGroupCipherSelected=" RSN_SUITE "\n"
		"dot11RSNAPMKIDUsed=%s\n"
		"dot11RSNAAuthenticationSuiteRequested=" RSN_SUITE "\n"
		"dot11RSNAPairwiseCipherRequested=" RSN_SUITE "\n"
		"dot11RSNAGroupCipherRequested=" RSN_SUITE "\n"
		"dot11RSNAConfigNumberOfGTKSAReplayCounters=0\n"
		"dot11RSNA4WayHandshakeFailures=%u\n",
		RSN_SUITE_ARG(wpa_key_mgmt_suite(sm)),
		RSN_SUITE_ARG(wpa_cipher_to_suite(sm->proto,
						  sm->pairwise_cipher)),
		RSN_SUITE_ARG(wpa_cipher_to_suite(sm->proto,
						  sm->group_cipher)),
		pmkid_txt,
		RSN_SUITE_ARG(wpa_key_mgmt_suite(sm)),
		RSN_SUITE_ARG(wpa_cipher_to_suite(sm->proto,
						  sm->pairwise_cipher)),
		RSN_SUITE_ARG(wpa_cipher_to_suite(sm->proto,
						  sm->group_cipher)),
		sm->dot11RSNA4WayHandshakeFailures);
	if (!os_snprintf_error(buflen - len, ret))
		len += ret;

	return (int) len;
}
#endif /* CONFIG_CTRL_IFACE */


static void wpa_sm_pmksa_free_cb(struct rsn_pmksa_cache_entry *entry,
				 void *ctx, enum pmksa_free_reason reason)
{
	struct wpa_sm *sm = ctx;
	int deauth = 0;

	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "RSN: PMKSA cache entry free_cb: "
		MACSTR " reason=%d", MAC2STR(entry->aa), reason);

	if (sm->cur_pmksa == entry) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"RSN: %s current PMKSA entry",
			reason == PMKSA_REPLACE ? "replaced" : "removed");
		pmksa_cache_clear_current(sm);

		/*
		 * If an entry is simply being replaced, there's no need to
		 * deauthenticate because it will be immediately re-added.
		 * This happens when EAP authentication is completed again
		 * (reauth or failed PMKSA caching attempt).
		 */
		if (reason != PMKSA_REPLACE)
			deauth = 1;
	}

	if (reason == PMKSA_EXPIRE &&
	    (sm->pmk_len == entry->pmk_len &&
	     os_memcmp(sm->pmk, entry->pmk, sm->pmk_len) == 0)) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"RSN: deauthenticating due to expired PMK");
		pmksa_cache_clear_current(sm);
		deauth = 1;
	}

	if (deauth) {
		os_memset(sm->pmk, 0, sizeof(sm->pmk));
		wpa_sm_deauthenticate(sm, WLAN_REASON_UNSPECIFIED);
	}
}


/**
 * wpa_sm_init - Initialize WPA state machine
 * @ctx: Context pointer for callbacks; this needs to be an allocated buffer
 * Returns: Pointer to the allocated WPA state machine data
 *
 * This function is used to allocate a new WPA state machine and the returned
 * value is passed to all WPA state machine calls.
 */
struct wpa_sm * wpa_sm_init(struct wpa_sm_ctx *ctx)
{
	struct wpa_sm *sm;

	sm = os_zalloc(sizeof(*sm));
	if (sm == NULL)
		return NULL;
	dl_list_init(&sm->pmksa_candidates);
	sm->renew_snonce = 1;
	sm->ctx = ctx;

	sm->dot11RSNAConfigPMKLifetime = 43200;
	sm->dot11RSNAConfigPMKReauthThreshold = 70;
	sm->dot11RSNAConfigSATimeout = 60;

	sm->pmksa = pmksa_cache_init(wpa_sm_pmksa_free_cb, sm, sm);
	if (sm->pmksa == NULL) {
		wpa_msg(sm->ctx->msg_ctx, MSG_ERROR,
			"RSN: PMKSA cache initialization failed");
		os_free(sm);
		return NULL;
	}

	return sm;
}


/**
 * wpa_sm_deinit - Deinitialize WPA state machine
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 */
void wpa_sm_deinit(struct wpa_sm *sm)
{
	if (sm == NULL)
		return;
	pmksa_cache_deinit(sm->pmksa);
	eloop_cancel_timeout(wpa_sm_start_preauth, sm, NULL);
	eloop_cancel_timeout(wpa_sm_rekey_ptk, sm, NULL);
	os_free(sm->assoc_wpa_ie);
	os_free(sm->ap_wpa_ie);
	os_free(sm->ap_rsn_ie);
	wpa_sm_drop_sa(sm);
	os_free(sm->ctx);
	peerkey_deinit(sm);
#ifdef CONFIG_IEEE80211R
	os_free(sm->assoc_resp_ies);
#endif /* CONFIG_IEEE80211R */
#ifdef CONFIG_TESTING_OPTIONS
	wpabuf_free(sm->test_assoc_ie);
#endif /* CONFIG_TESTING_OPTIONS */
	os_free(sm);
}


/**
 * wpa_sm_notify_assoc - Notify WPA state machine about association
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @bssid: The BSSID of the new association
 *
 * This function is called to let WPA state machine know that the connection
 * was established.
 */
void wpa_sm_notify_assoc(struct wpa_sm *sm, const u8 *bssid)
{
	int clear_keys = 1;

	if (sm == NULL)
		return;

	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
		"WPA: Association event - clear replay counter");
	os_memcpy(sm->bssid, bssid, ETH_ALEN);
	os_memset(sm->rx_replay_counter, 0, WPA_REPLAY_COUNTER_LEN);
	sm->rx_replay_counter_set = 0;
	sm->renew_snonce = 1;
	if (os_memcmp(sm->preauth_bssid, bssid, ETH_ALEN) == 0)
		rsn_preauth_deinit(sm);

#ifdef CONFIG_IEEE80211R
	if (wpa_ft_is_completed(sm)) {
		/*
		 * Clear portValid to kick EAPOL state machine to re-enter
		 * AUTHENTICATED state to get the EAPOL port Authorized.
		 */
		eapol_sm_notify_portValid(sm->eapol, FALSE);
		wpa_supplicant_key_neg_complete(sm, sm->bssid, 1);

		/* Prepare for the next transition */
		wpa_ft_prepare_auth_request(sm, NULL);

		clear_keys = 0;
	}
#endif /* CONFIG_IEEE80211R */

	if (clear_keys) {
		/*
		 * IEEE 802.11, 8.4.10: Delete PTK SA on (re)association if
		 * this is not part of a Fast BSS Transition.
		 */
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: Clear old PTK");
		sm->ptk_set = 0;
		os_memset(&sm->ptk, 0, sizeof(sm->ptk));
		sm->tptk_set = 0;
		os_memset(&sm->tptk, 0, sizeof(sm->tptk));
		os_memset(&sm->gtk, 0, sizeof(sm->gtk));
		os_memset(&sm->gtk_wnm_sleep, 0, sizeof(sm->gtk_wnm_sleep));
#ifdef CONFIG_IEEE80211W
		os_memset(&sm->igtk, 0, sizeof(sm->igtk));
		os_memset(&sm->igtk_wnm_sleep, 0, sizeof(sm->igtk_wnm_sleep));
#endif /* CONFIG_IEEE80211W */
	}

#ifdef CONFIG_TDLS
	wpa_tdls_assoc(sm);
#endif /* CONFIG_TDLS */

#ifdef CONFIG_P2P
	os_memset(sm->p2p_ip_addr, 0, sizeof(sm->p2p_ip_addr));
#endif /* CONFIG_P2P */
}


/**
 * wpa_sm_notify_disassoc - Notify WPA state machine about disassociation
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 *
 * This function is called to let WPA state machine know that the connection
 * was lost. This will abort any existing pre-authentication session.
 */
void wpa_sm_notify_disassoc(struct wpa_sm *sm)
{
	eloop_cancel_timeout(wpa_sm_start_preauth, sm, NULL);
	eloop_cancel_timeout(wpa_sm_rekey_ptk, sm, NULL);
	peerkey_deinit(sm);
	rsn_preauth_deinit(sm);
	pmksa_cache_clear_current(sm);
	if (wpa_sm_get_state(sm) == WPA_4WAY_HANDSHAKE)
		sm->dot11RSNA4WayHandshakeFailures++;
#ifdef CONFIG_TDLS
	wpa_tdls_disassoc(sm);
#endif /* CONFIG_TDLS */
#ifdef CONFIG_IEEE80211R
	sm->ft_reassoc_completed = 0;
#endif /* CONFIG_IEEE80211R */

	/* Keys are not needed in the WPA state machine anymore */
	wpa_sm_drop_sa(sm);

	sm->msg_3_of_4_ok = 0;
}


/**
 * wpa_sm_set_pmk - Set PMK
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @pmk: The new PMK
 * @pmk_len: The length of the new PMK in bytes
 * @pmkid: Calculated PMKID
 * @bssid: AA to add into PMKSA cache or %NULL to not cache the PMK
 *
 * Configure the PMK for WPA state machine.
 */
void wpa_sm_set_pmk(struct wpa_sm *sm, const u8 *pmk, size_t pmk_len,
		    const u8 *pmkid, const u8 *bssid)
{
	if (sm == NULL)
		return;

	sm->pmk_len = pmk_len;
	os_memcpy(sm->pmk, pmk, pmk_len);

#ifdef CONFIG_IEEE80211R
	/* Set XXKey to be PSK for FT key derivation */
	sm->xxkey_len = pmk_len;
	os_memcpy(sm->xxkey, pmk, pmk_len);
#endif /* CONFIG_IEEE80211R */

	if (bssid) {
		pmksa_cache_add(sm->pmksa, pmk, pmk_len, pmkid, NULL, 0,
				bssid, sm->own_addr,
				sm->network_ctx, sm->key_mgmt);
	}
}


/**
 * wpa_sm_set_pmk_from_pmksa - Set PMK based on the current PMKSA
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 *
 * Take the PMK from the current PMKSA into use. If no PMKSA is active, the PMK
 * will be cleared.
 */
void wpa_sm_set_pmk_from_pmksa(struct wpa_sm *sm)
{
	if (sm == NULL)
		return;

	if (sm->cur_pmksa) {
		sm->pmk_len = sm->cur_pmksa->pmk_len;
		os_memcpy(sm->pmk, sm->cur_pmksa->pmk, sm->pmk_len);
	} else {
		sm->pmk_len = PMK_LEN;
		os_memset(sm->pmk, 0, PMK_LEN);
	}
}


/**
 * wpa_sm_set_fast_reauth - Set fast reauthentication (EAP) enabled/disabled
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @fast_reauth: Whether fast reauthentication (EAP) is allowed
 */
void wpa_sm_set_fast_reauth(struct wpa_sm *sm, int fast_reauth)
{
	if (sm)
		sm->fast_reauth = fast_reauth;
}


/**
 * wpa_sm_set_scard_ctx - Set context pointer for smartcard callbacks
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @scard_ctx: Context pointer for smartcard related callback functions
 */
void wpa_sm_set_scard_ctx(struct wpa_sm *sm, void *scard_ctx)
{
	if (sm == NULL)
		return;
	sm->scard_ctx = scard_ctx;
	if (sm->preauth_eapol)
		eapol_sm_register_scard_ctx(sm->preauth_eapol, scard_ctx);
}


/**
 * wpa_sm_set_config - Notification of current configration change
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @config: Pointer to current network configuration
 *
 * Notify WPA state machine that configuration has changed. config will be
 * stored as a backpointer to network configuration. This can be %NULL to clear
 * the stored pointed.
 */
void wpa_sm_set_config(struct wpa_sm *sm, struct rsn_supp_config *config)
{
	if (!sm)
		return;

	if (config) {
		sm->network_ctx = config->network_ctx;
		sm->peerkey_enabled = config->peerkey_enabled;
		sm->allowed_pairwise_cipher = config->allowed_pairwise_cipher;
		sm->proactive_key_caching = config->proactive_key_caching;
		sm->eap_workaround = config->eap_workaround;
		sm->eap_conf_ctx = config->eap_conf_ctx;
		if (config->ssid) {
			os_memcpy(sm->ssid, config->ssid, config->ssid_len);
			sm->ssid_len = config->ssid_len;
		} else
			sm->ssid_len = 0;
		sm->wpa_ptk_rekey = config->wpa_ptk_rekey;
		sm->p2p = config->p2p;
		sm->wpa_rsc_relaxation = config->wpa_rsc_relaxation;
	} else {
		sm->network_ctx = NULL;
		sm->peerkey_enabled = 0;
		sm->allowed_pairwise_cipher = 0;
		sm->proactive_key_caching = 0;
		sm->eap_workaround = 0;
		sm->eap_conf_ctx = NULL;
		sm->ssid_len = 0;
		sm->wpa_ptk_rekey = 0;
		sm->p2p = 0;
		sm->wpa_rsc_relaxation = 0;
	}
}


/**
 * wpa_sm_set_own_addr - Set own MAC address
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @addr: Own MAC address
 */
void wpa_sm_set_own_addr(struct wpa_sm *sm, const u8 *addr)
{
	if (sm)
		os_memcpy(sm->own_addr, addr, ETH_ALEN);
}


/**
 * wpa_sm_set_ifname - Set network interface name
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @ifname: Interface name
 * @bridge_ifname: Optional bridge interface name (for pre-auth)
 */
void wpa_sm_set_ifname(struct wpa_sm *sm, const char *ifname,
		       const char *bridge_ifname)
{
	if (sm) {
		sm->ifname = ifname;
		sm->bridge_ifname = bridge_ifname;
	}
}


/**
 * wpa_sm_set_eapol - Set EAPOL state machine pointer
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @eapol: Pointer to EAPOL state machine allocated with eapol_sm_init()
 */
void wpa_sm_set_eapol(struct wpa_sm *sm, struct eapol_sm *eapol)
{
	if (sm)
		sm->eapol = eapol;
}


/**
 * wpa_sm_set_param - Set WPA state machine parameters
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @param: Parameter field
 * @value: Parameter value
 * Returns: 0 on success, -1 on failure
 */
int wpa_sm_set_param(struct wpa_sm *sm, enum wpa_sm_conf_params param,
		     unsigned int value)
{
	int ret = 0;

	if (sm == NULL)
		return -1;

	switch (param) {
	case RSNA_PMK_LIFETIME:
		if (value > 0)
			sm->dot11RSNAConfigPMKLifetime = value;
		else
			ret = -1;
		break;
	case RSNA_PMK_REAUTH_THRESHOLD:
		if (value > 0 && value <= 100)
			sm->dot11RSNAConfigPMKReauthThreshold = value;
		else
			ret = -1;
		break;
	case RSNA_SA_TIMEOUT:
		if (value > 0)
			sm->dot11RSNAConfigSATimeout = value;
		else
			ret = -1;
		break;
	case WPA_PARAM_PROTO:
		sm->proto = value;
		break;
	case WPA_PARAM_PAIRWISE:
		sm->pairwise_cipher = value;
		break;
	case WPA_PARAM_GROUP:
		sm->group_cipher = value;
		break;
	case WPA_PARAM_KEY_MGMT:
		sm->key_mgmt = value;
		break;
#ifdef CONFIG_IEEE80211W
	case WPA_PARAM_MGMT_GROUP:
		sm->mgmt_group_cipher = value;
		break;
#endif /* CONFIG_IEEE80211W */
	case WPA_PARAM_RSN_ENABLED:
		sm->rsn_enabled = value;
		break;
	case WPA_PARAM_MFP:
		sm->mfp = value;
		break;
	default:
		break;
	}

	return ret;
}


/**
 * wpa_sm_get_status - Get WPA state machine
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @buf: Buffer for status information
 * @buflen: Maximum buffer length
 * @verbose: Whether to include verbose status information
 * Returns: Number of bytes written to buf.
 *
 * Query WPA state machine for status information. This function fills in
 * a text area with current status information. If the buffer (buf) is not
 * large enough, status information will be truncated to fit the buffer.
 */
int wpa_sm_get_status(struct wpa_sm *sm, char *buf, size_t buflen,
		      int verbose)
{
	char *pos = buf, *end = buf + buflen;
	int ret;

	ret = os_snprintf(pos, end - pos,
			  "pairwise_cipher=%s\n"
			  "group_cipher=%s\n"
			  "key_mgmt=%s\n",
			  wpa_cipher_txt(sm->pairwise_cipher),
			  wpa_cipher_txt(sm->group_cipher),
			  wpa_key_mgmt_txt(sm->key_mgmt, sm->proto));
	if (os_snprintf_error(end - pos, ret))
		return pos - buf;
	pos += ret;

	if (sm->mfp != NO_MGMT_FRAME_PROTECTION && sm->ap_rsn_ie) {
		struct wpa_ie_data rsn;
		if (wpa_parse_wpa_ie_rsn(sm->ap_rsn_ie, sm->ap_rsn_ie_len, &rsn)
		    >= 0 &&
		    rsn.capabilities & (WPA_CAPABILITY_MFPR |
					WPA_CAPABILITY_MFPC)) {
			ret = os_snprintf(pos, end - pos, "pmf=%d\n",
					  (rsn.capabilities &
					   WPA_CAPABILITY_MFPR) ? 2 : 1);
			if (os_snprintf_error(end - pos, ret))
				return pos - buf;
			pos += ret;
		}
	}

	return pos - buf;
}


int wpa_sm_pmf_enabled(struct wpa_sm *sm)
{
	struct wpa_ie_data rsn;

	if (sm->mfp == NO_MGMT_FRAME_PROTECTION || !sm->ap_rsn_ie)
		return 0;

	if (wpa_parse_wpa_ie_rsn(sm->ap_rsn_ie, sm->ap_rsn_ie_len, &rsn) >= 0 &&
	    rsn.capabilities & (WPA_CAPABILITY_MFPR | WPA_CAPABILITY_MFPC))
		return 1;

	return 0;
}


/**
 * wpa_sm_set_assoc_wpa_ie_default - Generate own WPA/RSN IE from configuration
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @wpa_ie: Pointer to buffer for WPA/RSN IE
 * @wpa_ie_len: Pointer to the length of the wpa_ie buffer
 * Returns: 0 on success, -1 on failure
 */
int wpa_sm_set_assoc_wpa_ie_default(struct wpa_sm *sm, u8 *wpa_ie,
				    size_t *wpa_ie_len)
{
	int res;

	if (sm == NULL)
		return -1;

#ifdef CONFIG_TESTING_OPTIONS
	if (sm->test_assoc_ie) {
		wpa_printf(MSG_DEBUG,
			   "TESTING: Replace association WPA/RSN IE");
		if (*wpa_ie_len < wpabuf_len(sm->test_assoc_ie))
			return -1;
		os_memcpy(wpa_ie, wpabuf_head(sm->test_assoc_ie),
			  wpabuf_len(sm->test_assoc_ie));
		res = wpabuf_len(sm->test_assoc_ie);
	} else
#endif /* CONFIG_TESTING_OPTIONS */
	res = wpa_gen_wpa_ie(sm, wpa_ie, *wpa_ie_len);
	if (res < 0)
		return -1;
	*wpa_ie_len = res;

	wpa_hexdump(MSG_DEBUG, "WPA: Set own WPA IE default",
		    wpa_ie, *wpa_ie_len);

	if (sm->assoc_wpa_ie == NULL) {
		/*
		 * Make a copy of the WPA/RSN IE so that 4-Way Handshake gets
		 * the correct version of the IE even if PMKSA caching is
		 * aborted (which would remove PMKID from IE generation).
		 */
		sm->assoc_wpa_ie = os_malloc(*wpa_ie_len);
		if (sm->assoc_wpa_ie == NULL)
			return -1;

		os_memcpy(sm->assoc_wpa_ie, wpa_ie, *wpa_ie_len);
		sm->assoc_wpa_ie_len = *wpa_ie_len;
	}

	return 0;
}


/**
 * wpa_sm_set_assoc_wpa_ie - Set own WPA/RSN IE from (Re)AssocReq
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @ie: Pointer to IE data (starting from id)
 * @len: IE length
 * Returns: 0 on success, -1 on failure
 *
 * Inform WPA state machine about the WPA/RSN IE used in (Re)Association
 * Request frame. The IE will be used to override the default value generated
 * with wpa_sm_set_assoc_wpa_ie_default().
 */
int wpa_sm_set_assoc_wpa_ie(struct wpa_sm *sm, const u8 *ie, size_t len)
{
	if (sm == NULL)
		return -1;

	os_free(sm->assoc_wpa_ie);
	if (ie == NULL || len == 0) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"WPA: clearing own WPA/RSN IE");
		sm->assoc_wpa_ie = NULL;
		sm->assoc_wpa_ie_len = 0;
	} else {
		wpa_hexdump(MSG_DEBUG, "WPA: set own WPA/RSN IE", ie, len);
		sm->assoc_wpa_ie = os_malloc(len);
		if (sm->assoc_wpa_ie == NULL)
			return -1;

		os_memcpy(sm->assoc_wpa_ie, ie, len);
		sm->assoc_wpa_ie_len = len;
	}

	return 0;
}


/**
 * wpa_sm_set_ap_wpa_ie - Set AP WPA IE from Beacon/ProbeResp
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @ie: Pointer to IE data (starting from id)
 * @len: IE length
 * Returns: 0 on success, -1 on failure
 *
 * Inform WPA state machine about the WPA IE used in Beacon / Probe Response
 * frame.
 */
int wpa_sm_set_ap_wpa_ie(struct wpa_sm *sm, const u8 *ie, size_t len)
{
	if (sm == NULL)
		return -1;

	os_free(sm->ap_wpa_ie);
	if (ie == NULL || len == 0) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"WPA: clearing AP WPA IE");
		sm->ap_wpa_ie = NULL;
		sm->ap_wpa_ie_len = 0;
	} else {
		wpa_hexdump(MSG_DEBUG, "WPA: set AP WPA IE", ie, len);
		sm->ap_wpa_ie = os_malloc(len);
		if (sm->ap_wpa_ie == NULL)
			return -1;

		os_memcpy(sm->ap_wpa_ie, ie, len);
		sm->ap_wpa_ie_len = len;
	}

	return 0;
}


/**
 * wpa_sm_set_ap_rsn_ie - Set AP RSN IE from Beacon/ProbeResp
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @ie: Pointer to IE data (starting from id)
 * @len: IE length
 * Returns: 0 on success, -1 on failure
 *
 * Inform WPA state machine about the RSN IE used in Beacon / Probe Response
 * frame.
 */
int wpa_sm_set_ap_rsn_ie(struct wpa_sm *sm, const u8 *ie, size_t len)
{
	if (sm == NULL)
		return -1;

	os_free(sm->ap_rsn_ie);
	if (ie == NULL || len == 0) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"WPA: clearing AP RSN IE");
		sm->ap_rsn_ie = NULL;
		sm->ap_rsn_ie_len = 0;
	} else {
		wpa_hexdump(MSG_DEBUG, "WPA: set AP RSN IE", ie, len);
		sm->ap_rsn_ie = os_malloc(len);
		if (sm->ap_rsn_ie == NULL)
			return -1;

		os_memcpy(sm->ap_rsn_ie, ie, len);
		sm->ap_rsn_ie_len = len;
	}

	return 0;
}


/**
 * wpa_sm_parse_own_wpa_ie - Parse own WPA/RSN IE
 * @sm: Pointer to WPA state machine data from wpa_sm_init()
 * @data: Pointer to data area for parsing results
 * Returns: 0 on success, -1 if IE is not known, or -2 on parsing failure
 *
 * Parse the contents of the own WPA or RSN IE from (Re)AssocReq and write the
 * parsed data into data.
 */
int wpa_sm_parse_own_wpa_ie(struct wpa_sm *sm, struct wpa_ie_data *data)
{
	if (sm == NULL)
		return -1;

	if (sm->assoc_wpa_ie == NULL) {
		wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG,
			"WPA: No WPA/RSN IE available from association info");
		return -1;
	}
	if (wpa_parse_wpa_ie(sm->assoc_wpa_ie, sm->assoc_wpa_ie_len, data))
		return -2;
	return 0;
}


int wpa_sm_pmksa_cache_list(struct wpa_sm *sm, char *buf, size_t len)
{
	return pmksa_cache_list(sm->pmksa, buf, len);
}


void wpa_sm_drop_sa(struct wpa_sm *sm)
{
	wpa_dbg(sm->ctx->msg_ctx, MSG_DEBUG, "WPA: Clear old PMK and PTK");
	sm->ptk_set = 0;
	sm->tptk_set = 0;
	os_memset(sm->pmk, 0, sizeof(sm->pmk));
	os_memset(&sm->ptk, 0, sizeof(sm->ptk));
	os_memset(&sm->tptk, 0, sizeof(sm->tptk));
	os_memset(&sm->gtk, 0, sizeof(sm->gtk));
	os_memset(&sm->gtk_wnm_sleep, 0, sizeof(sm->gtk_wnm_sleep));
#ifdef CONFIG_IEEE80211W
	os_memset(&sm->igtk, 0, sizeof(sm->igtk));
	os_memset(&sm->igtk_wnm_sleep, 0, sizeof(sm->igtk_wnm_sleep));
#endif /* CONFIG_IEEE80211W */
#ifdef CONFIG_IEEE80211R
	os_memset(sm->xxkey, 0, sizeof(sm->xxkey));
	os_memset(sm->pmk_r0, 0, sizeof(sm->pmk_r0));
	os_memset(sm->pmk_r1, 0, sizeof(sm->pmk_r1));
#endif /* CONFIG_IEEE80211R */
}


int wpa_sm_has_ptk(struct wpa_sm *sm)
{
	if (sm == NULL)
		return 0;
	return sm->ptk_set;
}


void wpa_sm_update_replay_ctr(struct wpa_sm *sm, const u8 *replay_ctr)
{
	os_memcpy(sm->rx_replay_counter, replay_ctr, WPA_REPLAY_COUNTER_LEN);
}


void wpa_sm_pmksa_cache_flush(struct wpa_sm *sm, void *network_ctx)
{
	pmksa_cache_flush(sm->pmksa, network_ctx, NULL, 0);
}


#ifdef CONFIG_WNM
int wpa_wnmsleep_install_key(struct wpa_sm *sm, u8 subelem_id, u8 *buf)
{
	u16 keyinfo;
	u8 keylen;  /* plaintext key len */
	u8 *key_rsc;

	if (subelem_id == WNM_SLEEP_SUBELEM_GTK) {
		struct wpa_gtk_data gd;

		os_memset(&gd, 0, sizeof(gd));
		keylen = wpa_cipher_key_len(sm->group_cipher);
		gd.key_rsc_len = wpa_cipher_rsc_len(sm->group_cipher);
		gd.alg = wpa_cipher_to_alg(sm->group_cipher);
		if (gd.alg == WPA_ALG_NONE) {
			wpa_printf(MSG_DEBUG, "Unsupported group cipher suite");
			return -1;
		}

		key_rsc = buf + 5;
		keyinfo = WPA_GET_LE16(buf + 2);
		gd.gtk_len = keylen;
		if (gd.gtk_len != buf[4]) {
			wpa_printf(MSG_DEBUG, "GTK len mismatch len %d vs %d",
				   gd.gtk_len, buf[4]);
			return -1;
		}
		gd.keyidx = keyinfo & 0x03; /* B0 - B1 */
		gd.tx = wpa_supplicant_gtk_tx_bit_workaround(
		         sm, !!(keyinfo & WPA_KEY_INFO_TXRX));

		os_memcpy(gd.gtk, buf + 13, gd.gtk_len);

		wpa_hexdump_key(MSG_DEBUG, "Install GTK (WNM SLEEP)",
				gd.gtk, gd.gtk_len);
		if (wpa_supplicant_install_gtk(sm, &gd, key_rsc, 1)) {
			os_memset(&gd, 0, sizeof(gd));
			wpa_printf(MSG_DEBUG, "Failed to install the GTK in "
				   "WNM mode");
			return -1;
		}
		os_memset(&gd, 0, sizeof(gd));
#ifdef CONFIG_IEEE80211W
	} else if (subelem_id == WNM_SLEEP_SUBELEM_IGTK) {
		const struct wpa_igtk_kde *igtk;

		os_memset(&igd, 0, sizeof(igd));
		keylen = wpa_cipher_key_len(sm->mgmt_group_cipher);
		os_memcpy(igd.keyid, buf + 2, 2);
		os_memcpy(igd.pn, buf + 4, 6);

		keyidx = WPA_GET_LE16(igd.keyid);
		os_memcpy(igd.igtk, buf + 10, keylen);

		wpa_hexdump_key(MSG_DEBUG, "Install IGTK (WNM SLEEP)",
				igd.igtk, keylen);
		if (wpa_sm_set_key(sm, wpa_cipher_to_alg(sm->mgmt_group_cipher),
				   broadcast_ether_addr,
				   keyidx, 0, igd.pn, sizeof(igd.pn),
				   igd.igtk, keylen) < 0) {
			wpa_printf(MSG_DEBUG, "Failed to install the IGTK in "
				   "WNM mode");
			os_memset(&igd, 0, sizeof(igd));
		igtk = (const struct wpa_igtk_kde *) (buf + 2);
		if (wpa_supplicant_install_igtk(sm, igtk, 1) < 0)
			return -1;
#endif /* CONFIG_IEEE80211W */
	} else {
		wpa_printf(MSG_DEBUG, "Unknown element id");
		return -1;
	}

	return 0;
}
#endif /* CONFIG_WNM */


#ifdef CONFIG_PEERKEY
int wpa_sm_rx_eapol_peerkey(struct wpa_sm *sm, const u8 *src_addr,
			    const u8 *buf, size_t len)
{
	struct wpa_peerkey *peerkey;

	for (peerkey = sm->peerkey; peerkey; peerkey = peerkey->next) {
		if (os_memcmp(peerkey->addr, src_addr, ETH_ALEN) == 0)
			break;
	}

	if (!peerkey)
		return 0;

	wpa_sm_rx_eapol(sm, src_addr, buf, len);

	return 1;
}
#endif /* CONFIG_PEERKEY */


#ifdef CONFIG_P2P

int wpa_sm_get_p2p_ip_addr(struct wpa_sm *sm, u8 *buf)
{
	if (sm == NULL || WPA_GET_BE32(sm->p2p_ip_addr) == 0)
		return -1;
	os_memcpy(buf, sm->p2p_ip_addr, 3 * 4);
	return 0;
}

#endif /* CONFIG_P2P */


void wpa_sm_set_rx_replay_ctr(struct wpa_sm *sm, const u8 *rx_replay_counter)
{
	if (rx_replay_counter == NULL)
		return;

	os_memcpy(sm->rx_replay_counter, rx_replay_counter,
		  WPA_REPLAY_COUNTER_LEN);
	sm->rx_replay_counter_set = 1;
	wpa_printf(MSG_DEBUG, "Updated key replay counter");
}


void wpa_sm_set_ptk_kck_kek(struct wpa_sm *sm,
			    const u8 *ptk_kck, size_t ptk_kck_len,
			    const u8 *ptk_kek, size_t ptk_kek_len)
{
	if (ptk_kck && ptk_kck_len <= WPA_KCK_MAX_LEN) {
		os_memcpy(sm->ptk.kck, ptk_kck, ptk_kck_len);
		sm->ptk.kck_len = ptk_kck_len;
		wpa_printf(MSG_DEBUG, "Updated PTK KCK");
	}
	if (ptk_kek && ptk_kek_len <= WPA_KEK_MAX_LEN) {
		os_memcpy(sm->ptk.kek, ptk_kek, ptk_kek_len);
		sm->ptk.kek_len = ptk_kek_len;
		wpa_printf(MSG_DEBUG, "Updated PTK KEK");
	}
	sm->ptk_set = 1;
}


#ifdef CONFIG_TESTING_OPTIONS
void wpa_sm_set_test_assoc_ie(struct wpa_sm *sm, struct wpabuf *buf)
{
	wpabuf_free(sm->test_assoc_ie);
	sm->test_assoc_ie = buf;
}
#endif /* CONFIG_TESTING_OPTIONS */
