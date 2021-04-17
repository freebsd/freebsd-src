/*
 * DPP authentication exchange
 * Copyright (c) 2017, Qualcomm Atheros, Inc.
 * Copyright (c) 2018-2020, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/ieee802_11_common.h"
#include "common/wpa_ctrl.h"
#include "crypto/aes.h"
#include "crypto/aes_siv.h"
#include "crypto/random.h"
#include "dpp.h"
#include "dpp_i.h"


#ifdef CONFIG_TESTING_OPTIONS
u8 dpp_protocol_key_override[600];
size_t dpp_protocol_key_override_len = 0;
u8 dpp_nonce_override[DPP_MAX_NONCE_LEN];
size_t dpp_nonce_override_len = 0;
#endif /* CONFIG_TESTING_OPTIONS */


static void dpp_build_attr_i_bootstrap_key_hash(struct wpabuf *msg,
						const u8 *hash)
{
	if (hash) {
		wpa_printf(MSG_DEBUG, "DPP: I-Bootstrap Key Hash");
		wpabuf_put_le16(msg, DPP_ATTR_I_BOOTSTRAP_KEY_HASH);
		wpabuf_put_le16(msg, SHA256_MAC_LEN);
		wpabuf_put_data(msg, hash, SHA256_MAC_LEN);
	}
}


static void dpp_auth_success(struct dpp_authentication *auth)
{
	wpa_printf(MSG_DEBUG,
		   "DPP: Authentication success - clear temporary keys");
	os_memset(auth->Mx, 0, sizeof(auth->Mx));
	auth->Mx_len = 0;
	os_memset(auth->Nx, 0, sizeof(auth->Nx));
	auth->Nx_len = 0;
	os_memset(auth->Lx, 0, sizeof(auth->Lx));
	auth->Lx_len = 0;
	os_memset(auth->k1, 0, sizeof(auth->k1));
	os_memset(auth->k2, 0, sizeof(auth->k2));

	auth->auth_success = 1;
}


static struct wpabuf * dpp_auth_build_req(struct dpp_authentication *auth,
					  const struct wpabuf *pi,
					  size_t nonce_len,
					  const u8 *r_pubkey_hash,
					  const u8 *i_pubkey_hash,
					  unsigned int neg_freq)
{
	struct wpabuf *msg;
	u8 clear[4 + DPP_MAX_NONCE_LEN + 4 + 1];
	u8 wrapped_data[4 + DPP_MAX_NONCE_LEN + 4 + 1 + AES_BLOCK_SIZE];
	u8 *pos;
	const u8 *addr[2];
	size_t len[2], siv_len, attr_len;
	u8 *attr_start, *attr_end;

	/* Build DPP Authentication Request frame attributes */
	attr_len = 2 * (4 + SHA256_MAC_LEN) + 4 + (pi ? wpabuf_len(pi) : 0) +
		4 + sizeof(wrapped_data);
	if (neg_freq > 0)
		attr_len += 4 + 2;
#ifdef CONFIG_DPP2
	attr_len += 5;
#endif /* CONFIG_DPP2 */
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_AFTER_WRAPPED_DATA_AUTH_REQ)
		attr_len += 5;
#endif /* CONFIG_TESTING_OPTIONS */
	msg = dpp_alloc_msg(DPP_PA_AUTHENTICATION_REQ, attr_len);
	if (!msg)
		return NULL;

	attr_start = wpabuf_put(msg, 0);

	/* Responder Bootstrapping Key Hash */
	dpp_build_attr_r_bootstrap_key_hash(msg, r_pubkey_hash);

	/* Initiator Bootstrapping Key Hash */
	dpp_build_attr_i_bootstrap_key_hash(msg, i_pubkey_hash);

	/* Initiator Protocol Key */
	if (pi) {
		wpabuf_put_le16(msg, DPP_ATTR_I_PROTOCOL_KEY);
		wpabuf_put_le16(msg, wpabuf_len(pi));
		wpabuf_put_buf(msg, pi);
	}

	/* Channel */
	if (neg_freq > 0) {
		u8 op_class, channel;

		if (ieee80211_freq_to_channel_ext(neg_freq, 0, 0, &op_class,
						  &channel) ==
		    NUM_HOSTAPD_MODES) {
			wpa_printf(MSG_INFO,
				   "DPP: Unsupported negotiation frequency request: %d",
				   neg_freq);
			wpabuf_free(msg);
			return NULL;
		}
		wpabuf_put_le16(msg, DPP_ATTR_CHANNEL);
		wpabuf_put_le16(msg, 2);
		wpabuf_put_u8(msg, op_class);
		wpabuf_put_u8(msg, channel);
	}

#ifdef CONFIG_DPP2
	/* Protocol Version */
	if (DPP_VERSION > 1) {
		wpabuf_put_le16(msg, DPP_ATTR_PROTOCOL_VERSION);
		wpabuf_put_le16(msg, 1);
		wpabuf_put_u8(msg, DPP_VERSION);
	}
#endif /* CONFIG_DPP2 */

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_WRAPPED_DATA_AUTH_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Wrapped Data");
		goto skip_wrapped_data;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* Wrapped data ({I-nonce, I-capabilities}k1) */
	pos = clear;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_I_NONCE_AUTH_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no I-nonce");
		goto skip_i_nonce;
	}
	if (dpp_test == DPP_TEST_INVALID_I_NONCE_AUTH_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - invalid I-nonce");
		WPA_PUT_LE16(pos, DPP_ATTR_I_NONCE);
		pos += 2;
		WPA_PUT_LE16(pos, nonce_len - 1);
		pos += 2;
		os_memcpy(pos, auth->i_nonce, nonce_len - 1);
		pos += nonce_len - 1;
		goto skip_i_nonce;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* I-nonce */
	WPA_PUT_LE16(pos, DPP_ATTR_I_NONCE);
	pos += 2;
	WPA_PUT_LE16(pos, nonce_len);
	pos += 2;
	os_memcpy(pos, auth->i_nonce, nonce_len);
	pos += nonce_len;

#ifdef CONFIG_TESTING_OPTIONS
skip_i_nonce:
	if (dpp_test == DPP_TEST_NO_I_CAPAB_AUTH_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no I-capab");
		goto skip_i_capab;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* I-capabilities */
	WPA_PUT_LE16(pos, DPP_ATTR_I_CAPABILITIES);
	pos += 2;
	WPA_PUT_LE16(pos, 1);
	pos += 2;
	auth->i_capab = auth->allowed_roles;
	*pos++ = auth->i_capab;
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_ZERO_I_CAPAB) {
		wpa_printf(MSG_INFO, "DPP: TESTING - zero I-capabilities");
		pos[-1] = 0;
	}
skip_i_capab:
#endif /* CONFIG_TESTING_OPTIONS */

	attr_end = wpabuf_put(msg, 0);

	/* OUI, OUI type, Crypto Suite, DPP frame type */
	addr[0] = wpabuf_head_u8(msg) + 2;
	len[0] = 3 + 1 + 1 + 1;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);

	/* Attributes before Wrapped Data */
	addr[1] = attr_start;
	len[1] = attr_end - attr_start;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);

	siv_len = pos - clear;
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV cleartext", clear, siv_len);
	if (aes_siv_encrypt(auth->k1, auth->curve->hash_len, clear, siv_len,
			    2, addr, len, wrapped_data) < 0) {
		wpabuf_free(msg);
		return NULL;
	}
	siv_len += AES_BLOCK_SIZE;
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped_data, siv_len);

	wpabuf_put_le16(msg, DPP_ATTR_WRAPPED_DATA);
	wpabuf_put_le16(msg, siv_len);
	wpabuf_put_data(msg, wrapped_data, siv_len);

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_AFTER_WRAPPED_DATA_AUTH_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - attr after Wrapped Data");
		dpp_build_attr_status(msg, DPP_STATUS_OK);
	}
skip_wrapped_data:
#endif /* CONFIG_TESTING_OPTIONS */

	wpa_hexdump_buf(MSG_DEBUG,
			"DPP: Authentication Request frame attributes", msg);

	return msg;
}


static struct wpabuf * dpp_auth_build_resp(struct dpp_authentication *auth,
					   enum dpp_status_error status,
					   const struct wpabuf *pr,
					   size_t nonce_len,
					   const u8 *r_pubkey_hash,
					   const u8 *i_pubkey_hash,
					   const u8 *r_nonce, const u8 *i_nonce,
					   const u8 *wrapped_r_auth,
					   size_t wrapped_r_auth_len,
					   const u8 *siv_key)
{
	struct wpabuf *msg;
#define DPP_AUTH_RESP_CLEAR_LEN 2 * (4 + DPP_MAX_NONCE_LEN) + 4 + 1 + \
		4 + 4 + DPP_MAX_HASH_LEN + AES_BLOCK_SIZE
	u8 clear[DPP_AUTH_RESP_CLEAR_LEN];
	u8 wrapped_data[DPP_AUTH_RESP_CLEAR_LEN + AES_BLOCK_SIZE];
	const u8 *addr[2];
	size_t len[2], siv_len, attr_len;
	u8 *attr_start, *attr_end, *pos;

	auth->waiting_auth_conf = 1;
	auth->auth_resp_status = status;
	auth->auth_resp_tries = 0;

	/* Build DPP Authentication Response frame attributes */
	attr_len = 4 + 1 + 2 * (4 + SHA256_MAC_LEN) +
		4 + (pr ? wpabuf_len(pr) : 0) + 4 + sizeof(wrapped_data);
#ifdef CONFIG_DPP2
	attr_len += 5;
#endif /* CONFIG_DPP2 */
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_AFTER_WRAPPED_DATA_AUTH_RESP)
		attr_len += 5;
#endif /* CONFIG_TESTING_OPTIONS */
	msg = dpp_alloc_msg(DPP_PA_AUTHENTICATION_RESP, attr_len);
	if (!msg)
		return NULL;

	attr_start = wpabuf_put(msg, 0);

	/* DPP Status */
	if (status != 255)
		dpp_build_attr_status(msg, status);

	/* Responder Bootstrapping Key Hash */
	dpp_build_attr_r_bootstrap_key_hash(msg, r_pubkey_hash);

	/* Initiator Bootstrapping Key Hash (mutual authentication) */
	dpp_build_attr_i_bootstrap_key_hash(msg, i_pubkey_hash);

	/* Responder Protocol Key */
	if (pr) {
		wpabuf_put_le16(msg, DPP_ATTR_R_PROTOCOL_KEY);
		wpabuf_put_le16(msg, wpabuf_len(pr));
		wpabuf_put_buf(msg, pr);
	}

#ifdef CONFIG_DPP2
	/* Protocol Version */
	if (auth->peer_version >= 2) {
		wpabuf_put_le16(msg, DPP_ATTR_PROTOCOL_VERSION);
		wpabuf_put_le16(msg, 1);
		wpabuf_put_u8(msg, DPP_VERSION);
	}
#endif /* CONFIG_DPP2 */

	attr_end = wpabuf_put(msg, 0);

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_WRAPPED_DATA_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Wrapped Data");
		goto skip_wrapped_data;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* Wrapped data ({R-nonce, I-nonce, R-capabilities, {R-auth}ke}k2) */
	pos = clear;

	if (r_nonce) {
		/* R-nonce */
		WPA_PUT_LE16(pos, DPP_ATTR_R_NONCE);
		pos += 2;
		WPA_PUT_LE16(pos, nonce_len);
		pos += 2;
		os_memcpy(pos, r_nonce, nonce_len);
		pos += nonce_len;
	}

	if (i_nonce) {
		/* I-nonce */
		WPA_PUT_LE16(pos, DPP_ATTR_I_NONCE);
		pos += 2;
		WPA_PUT_LE16(pos, nonce_len);
		pos += 2;
		os_memcpy(pos, i_nonce, nonce_len);
#ifdef CONFIG_TESTING_OPTIONS
		if (dpp_test == DPP_TEST_I_NONCE_MISMATCH_AUTH_RESP) {
			wpa_printf(MSG_INFO, "DPP: TESTING - I-nonce mismatch");
			pos[nonce_len / 2] ^= 0x01;
		}
#endif /* CONFIG_TESTING_OPTIONS */
		pos += nonce_len;
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_R_CAPAB_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no R-capab");
		goto skip_r_capab;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* R-capabilities */
	WPA_PUT_LE16(pos, DPP_ATTR_R_CAPABILITIES);
	pos += 2;
	WPA_PUT_LE16(pos, 1);
	pos += 2;
	auth->r_capab = auth->configurator ? DPP_CAPAB_CONFIGURATOR :
		DPP_CAPAB_ENROLLEE;
	*pos++ = auth->r_capab;
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_ZERO_R_CAPAB) {
		wpa_printf(MSG_INFO, "DPP: TESTING - zero R-capabilities");
		pos[-1] = 0;
	} else if (dpp_test == DPP_TEST_INCOMPATIBLE_R_CAPAB_AUTH_RESP) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - incompatible R-capabilities");
		if ((auth->i_capab & DPP_CAPAB_ROLE_MASK) ==
		    (DPP_CAPAB_CONFIGURATOR | DPP_CAPAB_ENROLLEE))
			pos[-1] = 0;
		else
			pos[-1] = auth->configurator ? DPP_CAPAB_ENROLLEE :
				DPP_CAPAB_CONFIGURATOR;
	}
skip_r_capab:
#endif /* CONFIG_TESTING_OPTIONS */

	if (wrapped_r_auth) {
		/* {R-auth}ke */
		WPA_PUT_LE16(pos, DPP_ATTR_WRAPPED_DATA);
		pos += 2;
		WPA_PUT_LE16(pos, wrapped_r_auth_len);
		pos += 2;
		os_memcpy(pos, wrapped_r_auth, wrapped_r_auth_len);
		pos += wrapped_r_auth_len;
	}

	/* OUI, OUI type, Crypto Suite, DPP frame type */
	addr[0] = wpabuf_head_u8(msg) + 2;
	len[0] = 3 + 1 + 1 + 1;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);

	/* Attributes before Wrapped Data */
	addr[1] = attr_start;
	len[1] = attr_end - attr_start;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);

	siv_len = pos - clear;
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV cleartext", clear, siv_len);
	if (aes_siv_encrypt(siv_key, auth->curve->hash_len, clear, siv_len,
			    2, addr, len, wrapped_data) < 0) {
		wpabuf_free(msg);
		return NULL;
	}
	siv_len += AES_BLOCK_SIZE;
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped_data, siv_len);

	wpabuf_put_le16(msg, DPP_ATTR_WRAPPED_DATA);
	wpabuf_put_le16(msg, siv_len);
	wpabuf_put_data(msg, wrapped_data, siv_len);

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_AFTER_WRAPPED_DATA_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - attr after Wrapped Data");
		dpp_build_attr_status(msg, DPP_STATUS_OK);
	}
skip_wrapped_data:
#endif /* CONFIG_TESTING_OPTIONS */

	wpa_hexdump_buf(MSG_DEBUG,
			"DPP: Authentication Response frame attributes", msg);
	return msg;
}


static int dpp_auth_build_resp_ok(struct dpp_authentication *auth)
{
	size_t nonce_len;
	size_t secret_len;
	struct wpabuf *msg, *pr = NULL;
	u8 r_auth[4 + DPP_MAX_HASH_LEN];
	u8 wrapped_r_auth[4 + DPP_MAX_HASH_LEN + AES_BLOCK_SIZE], *w_r_auth;
	size_t wrapped_r_auth_len;
	int ret = -1;
	const u8 *r_pubkey_hash, *i_pubkey_hash, *r_nonce, *i_nonce;
	enum dpp_status_error status = DPP_STATUS_OK;
#ifdef CONFIG_TESTING_OPTIONS
	u8 test_hash[SHA256_MAC_LEN];
#endif /* CONFIG_TESTING_OPTIONS */

	wpa_printf(MSG_DEBUG, "DPP: Build Authentication Response");
	if (!auth->own_bi)
		return -1;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_nonce_override_len > 0) {
		wpa_printf(MSG_INFO, "DPP: TESTING - override R-nonce");
		nonce_len = dpp_nonce_override_len;
		os_memcpy(auth->r_nonce, dpp_nonce_override, nonce_len);
	} else {
		nonce_len = auth->curve->nonce_len;
		if (random_get_bytes(auth->r_nonce, nonce_len)) {
			wpa_printf(MSG_ERROR,
				   "DPP: Failed to generate R-nonce");
			goto fail;
		}
	}
#else /* CONFIG_TESTING_OPTIONS */
	nonce_len = auth->curve->nonce_len;
	if (random_get_bytes(auth->r_nonce, nonce_len)) {
		wpa_printf(MSG_ERROR, "DPP: Failed to generate R-nonce");
		goto fail;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	wpa_hexdump(MSG_DEBUG, "DPP: R-nonce", auth->r_nonce, nonce_len);

	EVP_PKEY_free(auth->own_protocol_key);
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_protocol_key_override_len) {
		const struct dpp_curve_params *tmp_curve;

		wpa_printf(MSG_INFO,
			   "DPP: TESTING - override protocol key");
		auth->own_protocol_key = dpp_set_keypair(
			&tmp_curve, dpp_protocol_key_override,
			dpp_protocol_key_override_len);
	} else {
		auth->own_protocol_key = dpp_gen_keypair(auth->curve);
	}
#else /* CONFIG_TESTING_OPTIONS */
	auth->own_protocol_key = dpp_gen_keypair(auth->curve);
#endif /* CONFIG_TESTING_OPTIONS */
	if (!auth->own_protocol_key)
		goto fail;

	pr = dpp_get_pubkey_point(auth->own_protocol_key, 0);
	if (!pr)
		goto fail;

	/* ECDH: N = pR * PI */
	if (dpp_ecdh(auth->own_protocol_key, auth->peer_protocol_key,
		     auth->Nx, &secret_len) < 0)
		goto fail;

	wpa_hexdump_key(MSG_DEBUG, "DPP: ECDH shared secret (N.x)",
			auth->Nx, auth->secret_len);
	auth->Nx_len = auth->secret_len;

	if (dpp_derive_k2(auth->Nx, auth->secret_len, auth->k2,
			  auth->curve->hash_len) < 0)
		goto fail;

	if (auth->own_bi && auth->peer_bi) {
		/* Mutual authentication */
		if (dpp_auth_derive_l_responder(auth) < 0)
			goto fail;
	}

	if (dpp_derive_bk_ke(auth) < 0)
		goto fail;

	/* R-auth = H(I-nonce | R-nonce | PI.x | PR.x | [BI.x |] BR.x | 0) */
	WPA_PUT_LE16(r_auth, DPP_ATTR_R_AUTH_TAG);
	WPA_PUT_LE16(&r_auth[2], auth->curve->hash_len);
	if (dpp_gen_r_auth(auth, r_auth + 4) < 0)
		goto fail;
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_R_AUTH_MISMATCH_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - R-auth mismatch");
		r_auth[4 + auth->curve->hash_len / 2] ^= 0x01;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	if (aes_siv_encrypt(auth->ke, auth->curve->hash_len,
			    r_auth, 4 + auth->curve->hash_len,
			    0, NULL, NULL, wrapped_r_auth) < 0)
		goto fail;
	wrapped_r_auth_len = 4 + auth->curve->hash_len + AES_BLOCK_SIZE;
	wpa_hexdump(MSG_DEBUG, "DPP: {R-auth}ke",
		    wrapped_r_auth, wrapped_r_auth_len);
	w_r_auth = wrapped_r_auth;

	r_pubkey_hash = auth->own_bi->pubkey_hash;
	if (auth->peer_bi)
		i_pubkey_hash = auth->peer_bi->pubkey_hash;
	else
		i_pubkey_hash = NULL;

	i_nonce = auth->i_nonce;
	r_nonce = auth->r_nonce;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_R_BOOTSTRAP_KEY_HASH_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no R-Bootstrap Key Hash");
		r_pubkey_hash = NULL;
	} else if (dpp_test ==
		   DPP_TEST_INVALID_R_BOOTSTRAP_KEY_HASH_AUTH_RESP) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - invalid R-Bootstrap Key Hash");
		os_memcpy(test_hash, r_pubkey_hash, SHA256_MAC_LEN);
		test_hash[SHA256_MAC_LEN - 1] ^= 0x01;
		r_pubkey_hash = test_hash;
	} else if (dpp_test == DPP_TEST_NO_I_BOOTSTRAP_KEY_HASH_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no I-Bootstrap Key Hash");
		i_pubkey_hash = NULL;
	} else if (dpp_test ==
		   DPP_TEST_INVALID_I_BOOTSTRAP_KEY_HASH_AUTH_RESP) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - invalid I-Bootstrap Key Hash");
		if (i_pubkey_hash)
			os_memcpy(test_hash, i_pubkey_hash, SHA256_MAC_LEN);
		else
			os_memset(test_hash, 0, SHA256_MAC_LEN);
		test_hash[SHA256_MAC_LEN - 1] ^= 0x01;
		i_pubkey_hash = test_hash;
	} else if (dpp_test == DPP_TEST_NO_R_PROTO_KEY_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no R-Proto Key");
		wpabuf_free(pr);
		pr = NULL;
	} else if (dpp_test == DPP_TEST_INVALID_R_PROTO_KEY_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - invalid R-Proto Key");
		wpabuf_free(pr);
		pr = wpabuf_alloc(2 * auth->curve->prime_len);
		if (!pr || dpp_test_gen_invalid_key(pr, auth->curve) < 0)
			goto fail;
	} else if (dpp_test == DPP_TEST_NO_R_AUTH_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no R-Auth");
		w_r_auth = NULL;
		wrapped_r_auth_len = 0;
	} else if (dpp_test == DPP_TEST_NO_STATUS_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Status");
		status = 255;
	} else if (dpp_test == DPP_TEST_INVALID_STATUS_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - invalid Status");
		status = 254;
	} else if (dpp_test == DPP_TEST_NO_R_NONCE_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no R-nonce");
		r_nonce = NULL;
	} else if (dpp_test == DPP_TEST_NO_I_NONCE_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no I-nonce");
		i_nonce = NULL;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	msg = dpp_auth_build_resp(auth, status, pr, nonce_len,
				  r_pubkey_hash, i_pubkey_hash,
				  r_nonce, i_nonce,
				  w_r_auth, wrapped_r_auth_len,
				  auth->k2);
	if (!msg)
		goto fail;
	wpabuf_free(auth->resp_msg);
	auth->resp_msg = msg;
	ret = 0;
fail:
	wpabuf_free(pr);
	return ret;
}


static int dpp_auth_build_resp_status(struct dpp_authentication *auth,
				      enum dpp_status_error status)
{
	struct wpabuf *msg;
	const u8 *r_pubkey_hash, *i_pubkey_hash, *i_nonce;
#ifdef CONFIG_TESTING_OPTIONS
	u8 test_hash[SHA256_MAC_LEN];
#endif /* CONFIG_TESTING_OPTIONS */

	if (!auth->own_bi)
		return -1;
	wpa_printf(MSG_DEBUG, "DPP: Build Authentication Response");

	r_pubkey_hash = auth->own_bi->pubkey_hash;
	if (auth->peer_bi)
		i_pubkey_hash = auth->peer_bi->pubkey_hash;
	else
		i_pubkey_hash = NULL;

	i_nonce = auth->i_nonce;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_R_BOOTSTRAP_KEY_HASH_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no R-Bootstrap Key Hash");
		r_pubkey_hash = NULL;
	} else if (dpp_test ==
		   DPP_TEST_INVALID_R_BOOTSTRAP_KEY_HASH_AUTH_RESP) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - invalid R-Bootstrap Key Hash");
		os_memcpy(test_hash, r_pubkey_hash, SHA256_MAC_LEN);
		test_hash[SHA256_MAC_LEN - 1] ^= 0x01;
		r_pubkey_hash = test_hash;
	} else if (dpp_test == DPP_TEST_NO_I_BOOTSTRAP_KEY_HASH_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no I-Bootstrap Key Hash");
		i_pubkey_hash = NULL;
	} else if (dpp_test ==
		   DPP_TEST_INVALID_I_BOOTSTRAP_KEY_HASH_AUTH_RESP) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - invalid I-Bootstrap Key Hash");
		if (i_pubkey_hash)
			os_memcpy(test_hash, i_pubkey_hash, SHA256_MAC_LEN);
		else
			os_memset(test_hash, 0, SHA256_MAC_LEN);
		test_hash[SHA256_MAC_LEN - 1] ^= 0x01;
		i_pubkey_hash = test_hash;
	} else if (dpp_test == DPP_TEST_NO_STATUS_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Status");
		status = 255;
	} else if (dpp_test == DPP_TEST_NO_I_NONCE_AUTH_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no I-nonce");
		i_nonce = NULL;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	msg = dpp_auth_build_resp(auth, status, NULL, auth->curve->nonce_len,
				  r_pubkey_hash, i_pubkey_hash,
				  NULL, i_nonce, NULL, 0, auth->k1);
	if (!msg)
		return -1;
	wpabuf_free(auth->resp_msg);
	auth->resp_msg = msg;
	return 0;
}


struct dpp_authentication *
dpp_auth_req_rx(struct dpp_global *dpp, void *msg_ctx, u8 dpp_allowed_roles,
		int qr_mutual, struct dpp_bootstrap_info *peer_bi,
		struct dpp_bootstrap_info *own_bi,
		unsigned int freq, const u8 *hdr, const u8 *attr_start,
		size_t attr_len)
{
	EVP_PKEY *pi = NULL;
	EVP_PKEY_CTX *ctx = NULL;
	size_t secret_len;
	const u8 *addr[2];
	size_t len[2];
	u8 *unwrapped = NULL;
	size_t unwrapped_len = 0;
	const u8 *wrapped_data, *i_proto, *i_nonce, *i_capab, *i_bootstrap,
		*channel;
	u16 wrapped_data_len, i_proto_len, i_nonce_len, i_capab_len,
		i_bootstrap_len, channel_len;
	struct dpp_authentication *auth = NULL;
#ifdef CONFIG_DPP2
	const u8 *version;
	u16 version_len;
#endif /* CONFIG_DPP2 */

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_STOP_AT_AUTH_REQ) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - stop at Authentication Request");
		return NULL;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	wrapped_data = dpp_get_attr(attr_start, attr_len, DPP_ATTR_WRAPPED_DATA,
				    &wrapped_data_len);
	if (!wrapped_data || wrapped_data_len < AES_BLOCK_SIZE) {
		wpa_msg(msg_ctx, MSG_INFO, DPP_EVENT_FAIL
			"Missing or invalid required Wrapped Data attribute");
		return NULL;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: Wrapped Data",
		    wrapped_data, wrapped_data_len);
	attr_len = wrapped_data - 4 - attr_start;

	auth = dpp_alloc_auth(dpp, msg_ctx);
	if (!auth)
		goto fail;
	if (peer_bi && peer_bi->configurator_params &&
	    dpp_set_configurator(auth, peer_bi->configurator_params) < 0)
		goto fail;
	auth->peer_bi = peer_bi;
	auth->own_bi = own_bi;
	auth->curve = own_bi->curve;
	auth->curr_freq = freq;

	auth->peer_version = 1; /* default to the first version */
#ifdef CONFIG_DPP2
	version = dpp_get_attr(attr_start, attr_len, DPP_ATTR_PROTOCOL_VERSION,
			       &version_len);
	if (version && DPP_VERSION > 1) {
		if (version_len < 1 || version[0] == 0) {
			dpp_auth_fail(auth,
				      "Invalid Protocol Version attribute");
			goto fail;
		}
		auth->peer_version = version[0];
		wpa_printf(MSG_DEBUG, "DPP: Peer protocol version %u",
			   auth->peer_version);
	}
#endif /* CONFIG_DPP2 */

	channel = dpp_get_attr(attr_start, attr_len, DPP_ATTR_CHANNEL,
			       &channel_len);
	if (channel) {
		int neg_freq;

		if (channel_len < 2) {
			dpp_auth_fail(auth, "Too short Channel attribute");
			goto fail;
		}

		neg_freq = ieee80211_chan_to_freq(NULL, channel[0], channel[1]);
		wpa_printf(MSG_DEBUG,
			   "DPP: Initiator requested different channel for negotiation: op_class=%u channel=%u --> freq=%d",
			   channel[0], channel[1], neg_freq);
		if (neg_freq < 0) {
			dpp_auth_fail(auth,
				      "Unsupported Channel attribute value");
			goto fail;
		}

		if (auth->curr_freq != (unsigned int) neg_freq) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Changing negotiation channel from %u MHz to %u MHz",
				   freq, neg_freq);
			auth->curr_freq = neg_freq;
		}
	}

	i_proto = dpp_get_attr(attr_start, attr_len, DPP_ATTR_I_PROTOCOL_KEY,
			       &i_proto_len);
	if (!i_proto) {
		dpp_auth_fail(auth,
			      "Missing required Initiator Protocol Key attribute");
		goto fail;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: Initiator Protocol Key",
		    i_proto, i_proto_len);

	/* M = bR * PI */
	pi = dpp_set_pubkey_point(own_bi->pubkey, i_proto, i_proto_len);
	if (!pi) {
		dpp_auth_fail(auth, "Invalid Initiator Protocol Key");
		goto fail;
	}
	dpp_debug_print_key("Peer (Initiator) Protocol Key", pi);

	if (dpp_ecdh(own_bi->pubkey, pi, auth->Mx, &secret_len) < 0)
		goto fail;
	auth->secret_len = secret_len;

	wpa_hexdump_key(MSG_DEBUG, "DPP: ECDH shared secret (M.x)",
			auth->Mx, auth->secret_len);
	auth->Mx_len = auth->secret_len;

	if (dpp_derive_k1(auth->Mx, auth->secret_len, auth->k1,
			  auth->curve->hash_len) < 0)
		goto fail;

	addr[0] = hdr;
	len[0] = DPP_HDR_LEN;
	addr[1] = attr_start;
	len[1] = attr_len;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped_data, wrapped_data_len);
	unwrapped_len = wrapped_data_len - AES_BLOCK_SIZE;
	unwrapped = os_malloc(unwrapped_len);
	if (!unwrapped)
		goto fail;
	if (aes_siv_decrypt(auth->k1, auth->curve->hash_len,
			    wrapped_data, wrapped_data_len,
			    2, addr, len, unwrapped) < 0) {
		dpp_auth_fail(auth, "AES-SIV decryption failed");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV cleartext",
		    unwrapped, unwrapped_len);

	if (dpp_check_attrs(unwrapped, unwrapped_len) < 0) {
		dpp_auth_fail(auth, "Invalid attribute in unwrapped data");
		goto fail;
	}

	i_nonce = dpp_get_attr(unwrapped, unwrapped_len, DPP_ATTR_I_NONCE,
			       &i_nonce_len);
	if (!i_nonce || i_nonce_len != auth->curve->nonce_len) {
		dpp_auth_fail(auth, "Missing or invalid I-nonce");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: I-nonce", i_nonce, i_nonce_len);
	os_memcpy(auth->i_nonce, i_nonce, i_nonce_len);

	i_capab = dpp_get_attr(unwrapped, unwrapped_len,
			       DPP_ATTR_I_CAPABILITIES,
			       &i_capab_len);
	if (!i_capab || i_capab_len < 1) {
		dpp_auth_fail(auth, "Missing or invalid I-capabilities");
		goto fail;
	}
	auth->i_capab = i_capab[0];
	wpa_printf(MSG_DEBUG, "DPP: I-capabilities: 0x%02x", auth->i_capab);

	bin_clear_free(unwrapped, unwrapped_len);
	unwrapped = NULL;

	switch (auth->i_capab & DPP_CAPAB_ROLE_MASK) {
	case DPP_CAPAB_ENROLLEE:
		if (!(dpp_allowed_roles & DPP_CAPAB_CONFIGURATOR)) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Local policy does not allow Configurator role");
			goto not_compatible;
		}
		wpa_printf(MSG_DEBUG, "DPP: Acting as Configurator");
		auth->configurator = 1;
		break;
	case DPP_CAPAB_CONFIGURATOR:
		if (!(dpp_allowed_roles & DPP_CAPAB_ENROLLEE)) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Local policy does not allow Enrollee role");
			goto not_compatible;
		}
		wpa_printf(MSG_DEBUG, "DPP: Acting as Enrollee");
		auth->configurator = 0;
		break;
	case DPP_CAPAB_CONFIGURATOR | DPP_CAPAB_ENROLLEE:
		if (dpp_allowed_roles & DPP_CAPAB_ENROLLEE) {
			wpa_printf(MSG_DEBUG, "DPP: Acting as Enrollee");
			auth->configurator = 0;
		} else if (dpp_allowed_roles & DPP_CAPAB_CONFIGURATOR) {
			wpa_printf(MSG_DEBUG, "DPP: Acting as Configurator");
			auth->configurator = 1;
		} else {
			wpa_printf(MSG_DEBUG,
				   "DPP: Local policy does not allow Configurator/Enrollee role");
			goto not_compatible;
		}
		break;
	default:
		wpa_printf(MSG_DEBUG, "DPP: Unexpected role in I-capabilities");
		wpa_msg(auth->msg_ctx, MSG_INFO,
			DPP_EVENT_FAIL "Invalid role in I-capabilities 0x%02x",
			auth->i_capab & DPP_CAPAB_ROLE_MASK);
		goto fail;
	}

	auth->peer_protocol_key = pi;
	pi = NULL;
	if (qr_mutual && !peer_bi && own_bi->type == DPP_BOOTSTRAP_QR_CODE) {
		char hex[SHA256_MAC_LEN * 2 + 1];

		wpa_printf(MSG_DEBUG,
			   "DPP: Mutual authentication required with QR Codes, but peer info is not yet available - request more time");
		if (dpp_auth_build_resp_status(auth,
					       DPP_STATUS_RESPONSE_PENDING) < 0)
			goto fail;
		i_bootstrap = dpp_get_attr(attr_start, attr_len,
					   DPP_ATTR_I_BOOTSTRAP_KEY_HASH,
					   &i_bootstrap_len);
		if (i_bootstrap && i_bootstrap_len == SHA256_MAC_LEN) {
			auth->response_pending = 1;
			os_memcpy(auth->waiting_pubkey_hash,
				  i_bootstrap, i_bootstrap_len);
			wpa_snprintf_hex(hex, sizeof(hex), i_bootstrap,
					 i_bootstrap_len);
		} else {
			hex[0] = '\0';
		}

		wpa_msg(auth->msg_ctx, MSG_INFO, DPP_EVENT_SCAN_PEER_QR_CODE
			"%s", hex);
		return auth;
	}
	if (dpp_auth_build_resp_ok(auth) < 0)
		goto fail;

	return auth;

not_compatible:
	wpa_msg(auth->msg_ctx, MSG_INFO, DPP_EVENT_NOT_COMPATIBLE
		"i-capab=0x%02x", auth->i_capab);
	if (dpp_allowed_roles & DPP_CAPAB_CONFIGURATOR)
		auth->configurator = 1;
	else
		auth->configurator = 0;
	auth->peer_protocol_key = pi;
	pi = NULL;
	if (dpp_auth_build_resp_status(auth, DPP_STATUS_NOT_COMPATIBLE) < 0)
		goto fail;

	auth->remove_on_tx_status = 1;
	return auth;
fail:
	bin_clear_free(unwrapped, unwrapped_len);
	EVP_PKEY_free(pi);
	EVP_PKEY_CTX_free(ctx);
	dpp_auth_deinit(auth);
	return NULL;
}


int dpp_notify_new_qr_code(struct dpp_authentication *auth,
			   struct dpp_bootstrap_info *peer_bi)
{
	if (!auth || !auth->response_pending ||
	    os_memcmp(auth->waiting_pubkey_hash, peer_bi->pubkey_hash,
		      SHA256_MAC_LEN) != 0)
		return 0;

	wpa_printf(MSG_DEBUG,
		   "DPP: New scanned QR Code has matching public key that was needed to continue DPP Authentication exchange with "
		   MACSTR, MAC2STR(auth->peer_mac_addr));
	auth->peer_bi = peer_bi;

	if (dpp_auth_build_resp_ok(auth) < 0)
		return -1;

	return 1;
}


static struct wpabuf * dpp_auth_build_conf(struct dpp_authentication *auth,
					   enum dpp_status_error status)
{
	struct wpabuf *msg;
	u8 i_auth[4 + DPP_MAX_HASH_LEN];
	size_t i_auth_len;
	u8 r_nonce[4 + DPP_MAX_NONCE_LEN];
	size_t r_nonce_len;
	const u8 *addr[2];
	size_t len[2], attr_len;
	u8 *wrapped_i_auth;
	u8 *wrapped_r_nonce;
	u8 *attr_start, *attr_end;
	const u8 *r_pubkey_hash, *i_pubkey_hash;
#ifdef CONFIG_TESTING_OPTIONS
	u8 test_hash[SHA256_MAC_LEN];
#endif /* CONFIG_TESTING_OPTIONS */

	wpa_printf(MSG_DEBUG, "DPP: Build Authentication Confirmation");

	i_auth_len = 4 + auth->curve->hash_len;
	r_nonce_len = 4 + auth->curve->nonce_len;
	/* Build DPP Authentication Confirmation frame attributes */
	attr_len = 4 + 1 + 2 * (4 + SHA256_MAC_LEN) +
		4 + i_auth_len + r_nonce_len + AES_BLOCK_SIZE;
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_AFTER_WRAPPED_DATA_AUTH_CONF)
		attr_len += 5;
#endif /* CONFIG_TESTING_OPTIONS */
	msg = dpp_alloc_msg(DPP_PA_AUTHENTICATION_CONF, attr_len);
	if (!msg)
		goto fail;

	attr_start = wpabuf_put(msg, 0);

	r_pubkey_hash = auth->peer_bi->pubkey_hash;
	if (auth->own_bi)
		i_pubkey_hash = auth->own_bi->pubkey_hash;
	else
		i_pubkey_hash = NULL;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_STATUS_AUTH_CONF) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Status");
		goto skip_status;
	} else if (dpp_test == DPP_TEST_INVALID_STATUS_AUTH_CONF) {
		wpa_printf(MSG_INFO, "DPP: TESTING - invalid Status");
		status = 254;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* DPP Status */
	dpp_build_attr_status(msg, status);

#ifdef CONFIG_TESTING_OPTIONS
skip_status:
	if (dpp_test == DPP_TEST_NO_R_BOOTSTRAP_KEY_HASH_AUTH_CONF) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no R-Bootstrap Key Hash");
		r_pubkey_hash = NULL;
	} else if (dpp_test ==
		   DPP_TEST_INVALID_R_BOOTSTRAP_KEY_HASH_AUTH_CONF) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - invalid R-Bootstrap Key Hash");
		os_memcpy(test_hash, r_pubkey_hash, SHA256_MAC_LEN);
		test_hash[SHA256_MAC_LEN - 1] ^= 0x01;
		r_pubkey_hash = test_hash;
	} else if (dpp_test == DPP_TEST_NO_I_BOOTSTRAP_KEY_HASH_AUTH_CONF) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no I-Bootstrap Key Hash");
		i_pubkey_hash = NULL;
	} else if (dpp_test ==
		   DPP_TEST_INVALID_I_BOOTSTRAP_KEY_HASH_AUTH_CONF) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - invalid I-Bootstrap Key Hash");
		if (i_pubkey_hash)
			os_memcpy(test_hash, i_pubkey_hash, SHA256_MAC_LEN);
		else
			os_memset(test_hash, 0, SHA256_MAC_LEN);
		test_hash[SHA256_MAC_LEN - 1] ^= 0x01;
		i_pubkey_hash = test_hash;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* Responder Bootstrapping Key Hash */
	dpp_build_attr_r_bootstrap_key_hash(msg, r_pubkey_hash);

	/* Initiator Bootstrapping Key Hash (mutual authentication) */
	dpp_build_attr_i_bootstrap_key_hash(msg, i_pubkey_hash);

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_WRAPPED_DATA_AUTH_CONF)
		goto skip_wrapped_data;
	if (dpp_test == DPP_TEST_NO_I_AUTH_AUTH_CONF)
		i_auth_len = 0;
#endif /* CONFIG_TESTING_OPTIONS */

	attr_end = wpabuf_put(msg, 0);

	/* OUI, OUI type, Crypto Suite, DPP frame type */
	addr[0] = wpabuf_head_u8(msg) + 2;
	len[0] = 3 + 1 + 1 + 1;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);

	/* Attributes before Wrapped Data */
	addr[1] = attr_start;
	len[1] = attr_end - attr_start;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);

	if (status == DPP_STATUS_OK) {
		/* I-auth wrapped with ke */
		wpabuf_put_le16(msg, DPP_ATTR_WRAPPED_DATA);
		wpabuf_put_le16(msg, i_auth_len + AES_BLOCK_SIZE);
		wrapped_i_auth = wpabuf_put(msg, i_auth_len + AES_BLOCK_SIZE);

#ifdef CONFIG_TESTING_OPTIONS
		if (dpp_test == DPP_TEST_NO_I_AUTH_AUTH_CONF)
			goto skip_i_auth;
#endif /* CONFIG_TESTING_OPTIONS */

		/* I-auth = H(R-nonce | I-nonce | PR.x | PI.x | BR.x | [BI.x |]
		 *	      1) */
		WPA_PUT_LE16(i_auth, DPP_ATTR_I_AUTH_TAG);
		WPA_PUT_LE16(&i_auth[2], auth->curve->hash_len);
		if (dpp_gen_i_auth(auth, i_auth + 4) < 0)
			goto fail;

#ifdef CONFIG_TESTING_OPTIONS
		if (dpp_test == DPP_TEST_I_AUTH_MISMATCH_AUTH_CONF) {
			wpa_printf(MSG_INFO, "DPP: TESTING - I-auth mismatch");
			i_auth[4 + auth->curve->hash_len / 2] ^= 0x01;
		}
skip_i_auth:
#endif /* CONFIG_TESTING_OPTIONS */
		if (aes_siv_encrypt(auth->ke, auth->curve->hash_len,
				    i_auth, i_auth_len,
				    2, addr, len, wrapped_i_auth) < 0)
			goto fail;
		wpa_hexdump(MSG_DEBUG, "DPP: {I-auth}ke",
			    wrapped_i_auth, i_auth_len + AES_BLOCK_SIZE);
	} else {
		/* R-nonce wrapped with k2 */
		wpabuf_put_le16(msg, DPP_ATTR_WRAPPED_DATA);
		wpabuf_put_le16(msg, r_nonce_len + AES_BLOCK_SIZE);
		wrapped_r_nonce = wpabuf_put(msg, r_nonce_len + AES_BLOCK_SIZE);

		WPA_PUT_LE16(r_nonce, DPP_ATTR_R_NONCE);
		WPA_PUT_LE16(&r_nonce[2], auth->curve->nonce_len);
		os_memcpy(r_nonce + 4, auth->r_nonce, auth->curve->nonce_len);

		if (aes_siv_encrypt(auth->k2, auth->curve->hash_len,
				    r_nonce, r_nonce_len,
				    2, addr, len, wrapped_r_nonce) < 0)
			goto fail;
		wpa_hexdump(MSG_DEBUG, "DPP: {R-nonce}k2",
			    wrapped_r_nonce, r_nonce_len + AES_BLOCK_SIZE);
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_AFTER_WRAPPED_DATA_AUTH_CONF) {
		wpa_printf(MSG_INFO, "DPP: TESTING - attr after Wrapped Data");
		dpp_build_attr_status(msg, DPP_STATUS_OK);
	}
skip_wrapped_data:
#endif /* CONFIG_TESTING_OPTIONS */

	wpa_hexdump_buf(MSG_DEBUG,
			"DPP: Authentication Confirmation frame attributes",
			msg);
	if (status == DPP_STATUS_OK)
		dpp_auth_success(auth);

	return msg;

fail:
	wpabuf_free(msg);
	return NULL;
}


static int dpp_autogen_bootstrap_key(struct dpp_authentication *auth)
{
	struct dpp_bootstrap_info *bi;

	if (auth->own_bi)
		return 0; /* already generated */

	bi = os_zalloc(sizeof(*bi));
	if (!bi)
		return -1;
	bi->type = DPP_BOOTSTRAP_QR_CODE;
	if (dpp_keygen(bi, auth->peer_bi->curve->name, NULL, 0) < 0 ||
	    dpp_gen_uri(bi) < 0)
		goto fail;
	wpa_printf(MSG_DEBUG,
		   "DPP: Auto-generated own bootstrapping key info: URI %s",
		   bi->uri);

	auth->tmp_own_bi = auth->own_bi = bi;

	return 0;
fail:
	dpp_bootstrap_info_free(bi);
	return -1;
}


struct dpp_authentication * dpp_auth_init(struct dpp_global *dpp, void *msg_ctx,
					  struct dpp_bootstrap_info *peer_bi,
					  struct dpp_bootstrap_info *own_bi,
					  u8 dpp_allowed_roles,
					  unsigned int neg_freq,
					  struct hostapd_hw_modes *own_modes,
					  u16 num_modes)
{
	struct dpp_authentication *auth;
	size_t nonce_len;
	size_t secret_len;
	struct wpabuf *pi = NULL;
	const u8 *r_pubkey_hash, *i_pubkey_hash;
#ifdef CONFIG_TESTING_OPTIONS
	u8 test_hash[SHA256_MAC_LEN];
#endif /* CONFIG_TESTING_OPTIONS */

	auth = dpp_alloc_auth(dpp, msg_ctx);
	if (!auth)
		return NULL;
	if (peer_bi->configurator_params &&
	    dpp_set_configurator(auth, peer_bi->configurator_params) < 0)
		goto fail;
	auth->initiator = 1;
	auth->waiting_auth_resp = 1;
	auth->allowed_roles = dpp_allowed_roles;
	auth->configurator = !!(dpp_allowed_roles & DPP_CAPAB_CONFIGURATOR);
	auth->peer_bi = peer_bi;
	auth->own_bi = own_bi;
	auth->curve = peer_bi->curve;

	if (dpp_autogen_bootstrap_key(auth) < 0 ||
	    dpp_prepare_channel_list(auth, neg_freq, own_modes, num_modes) < 0)
		goto fail;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_nonce_override_len > 0) {
		wpa_printf(MSG_INFO, "DPP: TESTING - override I-nonce");
		nonce_len = dpp_nonce_override_len;
		os_memcpy(auth->i_nonce, dpp_nonce_override, nonce_len);
	} else {
		nonce_len = auth->curve->nonce_len;
		if (random_get_bytes(auth->i_nonce, nonce_len)) {
			wpa_printf(MSG_ERROR,
				   "DPP: Failed to generate I-nonce");
			goto fail;
		}
	}
#else /* CONFIG_TESTING_OPTIONS */
	nonce_len = auth->curve->nonce_len;
	if (random_get_bytes(auth->i_nonce, nonce_len)) {
		wpa_printf(MSG_ERROR, "DPP: Failed to generate I-nonce");
		goto fail;
	}
#endif /* CONFIG_TESTING_OPTIONS */
	wpa_hexdump(MSG_DEBUG, "DPP: I-nonce", auth->i_nonce, nonce_len);

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_protocol_key_override_len) {
		const struct dpp_curve_params *tmp_curve;

		wpa_printf(MSG_INFO,
			   "DPP: TESTING - override protocol key");
		auth->own_protocol_key = dpp_set_keypair(
			&tmp_curve, dpp_protocol_key_override,
			dpp_protocol_key_override_len);
	} else {
		auth->own_protocol_key = dpp_gen_keypair(auth->curve);
	}
#else /* CONFIG_TESTING_OPTIONS */
	auth->own_protocol_key = dpp_gen_keypair(auth->curve);
#endif /* CONFIG_TESTING_OPTIONS */
	if (!auth->own_protocol_key)
		goto fail;

	pi = dpp_get_pubkey_point(auth->own_protocol_key, 0);
	if (!pi)
		goto fail;

	/* ECDH: M = pI * BR */
	if (dpp_ecdh(auth->own_protocol_key, auth->peer_bi->pubkey,
		     auth->Mx, &secret_len) < 0)
		goto fail;
	auth->secret_len = secret_len;

	wpa_hexdump_key(MSG_DEBUG, "DPP: ECDH shared secret (M.x)",
			auth->Mx, auth->secret_len);
	auth->Mx_len = auth->secret_len;

	if (dpp_derive_k1(auth->Mx, auth->secret_len, auth->k1,
			  auth->curve->hash_len) < 0)
		goto fail;

	r_pubkey_hash = auth->peer_bi->pubkey_hash;
	i_pubkey_hash = auth->own_bi->pubkey_hash;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_R_BOOTSTRAP_KEY_HASH_AUTH_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no R-Bootstrap Key Hash");
		r_pubkey_hash = NULL;
	} else if (dpp_test == DPP_TEST_INVALID_R_BOOTSTRAP_KEY_HASH_AUTH_REQ) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - invalid R-Bootstrap Key Hash");
		os_memcpy(test_hash, r_pubkey_hash, SHA256_MAC_LEN);
		test_hash[SHA256_MAC_LEN - 1] ^= 0x01;
		r_pubkey_hash = test_hash;
	} else if (dpp_test == DPP_TEST_NO_I_BOOTSTRAP_KEY_HASH_AUTH_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no I-Bootstrap Key Hash");
		i_pubkey_hash = NULL;
	} else if (dpp_test == DPP_TEST_INVALID_I_BOOTSTRAP_KEY_HASH_AUTH_REQ) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - invalid I-Bootstrap Key Hash");
		os_memcpy(test_hash, i_pubkey_hash, SHA256_MAC_LEN);
		test_hash[SHA256_MAC_LEN - 1] ^= 0x01;
		i_pubkey_hash = test_hash;
	} else if (dpp_test == DPP_TEST_NO_I_PROTO_KEY_AUTH_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no I-Proto Key");
		wpabuf_free(pi);
		pi = NULL;
	} else if (dpp_test == DPP_TEST_INVALID_I_PROTO_KEY_AUTH_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - invalid I-Proto Key");
		wpabuf_free(pi);
		pi = wpabuf_alloc(2 * auth->curve->prime_len);
		if (!pi || dpp_test_gen_invalid_key(pi, auth->curve) < 0)
			goto fail;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (neg_freq && auth->num_freq == 1 && auth->freq[0] == neg_freq)
		neg_freq = 0;
	auth->req_msg = dpp_auth_build_req(auth, pi, nonce_len, r_pubkey_hash,
					   i_pubkey_hash, neg_freq);
	if (!auth->req_msg)
		goto fail;

out:
	wpabuf_free(pi);
	return auth;
fail:
	dpp_auth_deinit(auth);
	auth = NULL;
	goto out;
}
static void
dpp_auth_resp_rx_status(struct dpp_authentication *auth, const u8 *hdr,
			const u8 *attr_start, size_t attr_len,
			const u8 *wrapped_data, u16 wrapped_data_len,
			enum dpp_status_error status)
{
	const u8 *addr[2];
	size_t len[2];
	u8 *unwrapped = NULL;
	size_t unwrapped_len = 0;
	const u8 *i_nonce, *r_capab;
	u16 i_nonce_len, r_capab_len;

	if (status == DPP_STATUS_NOT_COMPATIBLE) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Responder reported incompatible roles");
	} else if (status == DPP_STATUS_RESPONSE_PENDING) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Responder reported more time needed");
	} else {
		wpa_printf(MSG_DEBUG,
			   "DPP: Responder reported failure (status %d)",
			   status);
		dpp_auth_fail(auth, "Responder reported failure");
		return;
	}

	addr[0] = hdr;
	len[0] = DPP_HDR_LEN;
	addr[1] = attr_start;
	len[1] = attr_len;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped_data, wrapped_data_len);
	unwrapped_len = wrapped_data_len - AES_BLOCK_SIZE;
	unwrapped = os_malloc(unwrapped_len);
	if (!unwrapped)
		goto fail;
	if (aes_siv_decrypt(auth->k1, auth->curve->hash_len,
			    wrapped_data, wrapped_data_len,
			    2, addr, len, unwrapped) < 0) {
		dpp_auth_fail(auth, "AES-SIV decryption failed");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV cleartext",
		    unwrapped, unwrapped_len);

	if (dpp_check_attrs(unwrapped, unwrapped_len) < 0) {
		dpp_auth_fail(auth, "Invalid attribute in unwrapped data");
		goto fail;
	}

	i_nonce = dpp_get_attr(unwrapped, unwrapped_len, DPP_ATTR_I_NONCE,
			       &i_nonce_len);
	if (!i_nonce || i_nonce_len != auth->curve->nonce_len) {
		dpp_auth_fail(auth, "Missing or invalid I-nonce");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: I-nonce", i_nonce, i_nonce_len);
	if (os_memcmp(auth->i_nonce, i_nonce, i_nonce_len) != 0) {
		dpp_auth_fail(auth, "I-nonce mismatch");
		goto fail;
	}

	r_capab = dpp_get_attr(unwrapped, unwrapped_len,
			       DPP_ATTR_R_CAPABILITIES,
			       &r_capab_len);
	if (!r_capab || r_capab_len < 1) {
		dpp_auth_fail(auth, "Missing or invalid R-capabilities");
		goto fail;
	}
	auth->r_capab = r_capab[0];
	wpa_printf(MSG_DEBUG, "DPP: R-capabilities: 0x%02x", auth->r_capab);
	if (status == DPP_STATUS_NOT_COMPATIBLE) {
		wpa_msg(auth->msg_ctx, MSG_INFO, DPP_EVENT_NOT_COMPATIBLE
			"r-capab=0x%02x", auth->r_capab);
	} else if (status == DPP_STATUS_RESPONSE_PENDING) {
		u8 role = auth->r_capab & DPP_CAPAB_ROLE_MASK;

		if ((auth->configurator && role != DPP_CAPAB_ENROLLEE) ||
		    (!auth->configurator && role != DPP_CAPAB_CONFIGURATOR)) {
			wpa_msg(auth->msg_ctx, MSG_INFO,
				DPP_EVENT_FAIL "Unexpected role in R-capabilities 0x%02x",
				role);
		} else {
			wpa_printf(MSG_DEBUG,
				   "DPP: Continue waiting for full DPP Authentication Response");
			wpa_msg(auth->msg_ctx, MSG_INFO,
				DPP_EVENT_RESPONSE_PENDING "%s",
				auth->tmp_own_bi ? auth->tmp_own_bi->uri : "");
		}
	}
fail:
	bin_clear_free(unwrapped, unwrapped_len);
}


struct wpabuf *
dpp_auth_resp_rx(struct dpp_authentication *auth, const u8 *hdr,
		 const u8 *attr_start, size_t attr_len)
{
	EVP_PKEY *pr;
	size_t secret_len;
	const u8 *addr[2];
	size_t len[2];
	u8 *unwrapped = NULL, *unwrapped2 = NULL;
	size_t unwrapped_len = 0, unwrapped2_len = 0;
	const u8 *r_bootstrap, *i_bootstrap, *wrapped_data, *status, *r_proto,
		*r_nonce, *i_nonce, *r_capab, *wrapped2, *r_auth;
	u16 r_bootstrap_len, i_bootstrap_len, wrapped_data_len, status_len,
		r_proto_len, r_nonce_len, i_nonce_len, r_capab_len,
		wrapped2_len, r_auth_len;
	u8 r_auth2[DPP_MAX_HASH_LEN];
	u8 role;
#ifdef CONFIG_DPP2
	const u8 *version;
	u16 version_len;
#endif /* CONFIG_DPP2 */

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_STOP_AT_AUTH_RESP) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - stop at Authentication Response");
		return NULL;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (!auth->initiator || !auth->peer_bi || auth->reconfig) {
		dpp_auth_fail(auth, "Unexpected Authentication Response");
		return NULL;
	}

	auth->waiting_auth_resp = 0;

	wrapped_data = dpp_get_attr(attr_start, attr_len, DPP_ATTR_WRAPPED_DATA,
				    &wrapped_data_len);
	if (!wrapped_data || wrapped_data_len < AES_BLOCK_SIZE) {
		dpp_auth_fail(auth,
			      "Missing or invalid required Wrapped Data attribute");
		return NULL;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: Wrapped data",
		    wrapped_data, wrapped_data_len);

	attr_len = wrapped_data - 4 - attr_start;

	r_bootstrap = dpp_get_attr(attr_start, attr_len,
				   DPP_ATTR_R_BOOTSTRAP_KEY_HASH,
				   &r_bootstrap_len);
	if (!r_bootstrap || r_bootstrap_len != SHA256_MAC_LEN) {
		dpp_auth_fail(auth,
			      "Missing or invalid required Responder Bootstrapping Key Hash attribute");
		return NULL;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: Responder Bootstrapping Key Hash",
		    r_bootstrap, r_bootstrap_len);
	if (os_memcmp(r_bootstrap, auth->peer_bi->pubkey_hash,
		      SHA256_MAC_LEN) != 0) {
		dpp_auth_fail(auth,
			      "Unexpected Responder Bootstrapping Key Hash value");
		wpa_hexdump(MSG_DEBUG,
			    "DPP: Expected Responder Bootstrapping Key Hash",
			    auth->peer_bi->pubkey_hash, SHA256_MAC_LEN);
		return NULL;
	}

	i_bootstrap = dpp_get_attr(attr_start, attr_len,
				   DPP_ATTR_I_BOOTSTRAP_KEY_HASH,
				   &i_bootstrap_len);
	if (i_bootstrap) {
		if (i_bootstrap_len != SHA256_MAC_LEN) {
			dpp_auth_fail(auth,
				      "Invalid Initiator Bootstrapping Key Hash attribute");
			return NULL;
		}
		wpa_hexdump(MSG_MSGDUMP,
			    "DPP: Initiator Bootstrapping Key Hash",
			    i_bootstrap, i_bootstrap_len);
		if (!auth->own_bi ||
		    os_memcmp(i_bootstrap, auth->own_bi->pubkey_hash,
			      SHA256_MAC_LEN) != 0) {
			dpp_auth_fail(auth,
				      "Initiator Bootstrapping Key Hash attribute did not match");
			return NULL;
		}
	} else if (auth->own_bi && auth->own_bi->type == DPP_BOOTSTRAP_PKEX) {
		/* PKEX bootstrapping mandates use of mutual authentication */
		dpp_auth_fail(auth,
			      "Missing Initiator Bootstrapping Key Hash attribute");
		return NULL;
	} else if (auth->own_bi &&
		   auth->own_bi->type == DPP_BOOTSTRAP_NFC_URI &&
		   auth->own_bi->nfc_negotiated) {
		/* NFC negotiated connection handover bootstrapping mandates
		 * use of mutual authentication */
		dpp_auth_fail(auth,
			      "Missing Initiator Bootstrapping Key Hash attribute");
		return NULL;
	}

	auth->peer_version = 1; /* default to the first version */
#ifdef CONFIG_DPP2
	version = dpp_get_attr(attr_start, attr_len, DPP_ATTR_PROTOCOL_VERSION,
			       &version_len);
	if (version && DPP_VERSION > 1) {
		if (version_len < 1 || version[0] == 0) {
			dpp_auth_fail(auth,
				      "Invalid Protocol Version attribute");
			return NULL;
		}
		auth->peer_version = version[0];
		wpa_printf(MSG_DEBUG, "DPP: Peer protocol version %u",
			   auth->peer_version);
	}
#endif /* CONFIG_DPP2 */

	status = dpp_get_attr(attr_start, attr_len, DPP_ATTR_STATUS,
			      &status_len);
	if (!status || status_len < 1) {
		dpp_auth_fail(auth,
			      "Missing or invalid required DPP Status attribute");
		return NULL;
	}
	wpa_printf(MSG_DEBUG, "DPP: Status %u", status[0]);
	auth->auth_resp_status = status[0];
	if (status[0] != DPP_STATUS_OK) {
		dpp_auth_resp_rx_status(auth, hdr, attr_start,
					attr_len, wrapped_data,
					wrapped_data_len, status[0]);
		return NULL;
	}

	if (!i_bootstrap && auth->own_bi) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Responder decided not to use mutual authentication");
		auth->own_bi = NULL;
	}

	wpa_msg(auth->msg_ctx, MSG_INFO, DPP_EVENT_AUTH_DIRECTION "mutual=%d",
		auth->own_bi != NULL);

	r_proto = dpp_get_attr(attr_start, attr_len, DPP_ATTR_R_PROTOCOL_KEY,
			       &r_proto_len);
	if (!r_proto) {
		dpp_auth_fail(auth,
			      "Missing required Responder Protocol Key attribute");
		return NULL;
	}
	wpa_hexdump(MSG_MSGDUMP, "DPP: Responder Protocol Key",
		    r_proto, r_proto_len);

	/* N = pI * PR */
	pr = dpp_set_pubkey_point(auth->own_protocol_key, r_proto, r_proto_len);
	if (!pr) {
		dpp_auth_fail(auth, "Invalid Responder Protocol Key");
		return NULL;
	}
	dpp_debug_print_key("Peer (Responder) Protocol Key", pr);

	if (dpp_ecdh(auth->own_protocol_key, pr, auth->Nx, &secret_len) < 0) {
		dpp_auth_fail(auth, "Failed to derive ECDH shared secret");
		goto fail;
	}
	EVP_PKEY_free(auth->peer_protocol_key);
	auth->peer_protocol_key = pr;
	pr = NULL;

	wpa_hexdump_key(MSG_DEBUG, "DPP: ECDH shared secret (N.x)",
			auth->Nx, auth->secret_len);
	auth->Nx_len = auth->secret_len;

	if (dpp_derive_k2(auth->Nx, auth->secret_len, auth->k2,
			  auth->curve->hash_len) < 0)
		goto fail;

	addr[0] = hdr;
	len[0] = DPP_HDR_LEN;
	addr[1] = attr_start;
	len[1] = attr_len;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped_data, wrapped_data_len);
	unwrapped_len = wrapped_data_len - AES_BLOCK_SIZE;
	unwrapped = os_malloc(unwrapped_len);
	if (!unwrapped)
		goto fail;
	if (aes_siv_decrypt(auth->k2, auth->curve->hash_len,
			    wrapped_data, wrapped_data_len,
			    2, addr, len, unwrapped) < 0) {
		dpp_auth_fail(auth, "AES-SIV decryption failed");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV cleartext",
		    unwrapped, unwrapped_len);

	if (dpp_check_attrs(unwrapped, unwrapped_len) < 0) {
		dpp_auth_fail(auth, "Invalid attribute in unwrapped data");
		goto fail;
	}

	r_nonce = dpp_get_attr(unwrapped, unwrapped_len, DPP_ATTR_R_NONCE,
			       &r_nonce_len);
	if (!r_nonce || r_nonce_len != auth->curve->nonce_len) {
		dpp_auth_fail(auth, "DPP: Missing or invalid R-nonce");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: R-nonce", r_nonce, r_nonce_len);
	os_memcpy(auth->r_nonce, r_nonce, r_nonce_len);

	i_nonce = dpp_get_attr(unwrapped, unwrapped_len, DPP_ATTR_I_NONCE,
			       &i_nonce_len);
	if (!i_nonce || i_nonce_len != auth->curve->nonce_len) {
		dpp_auth_fail(auth, "Missing or invalid I-nonce");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: I-nonce", i_nonce, i_nonce_len);
	if (os_memcmp(auth->i_nonce, i_nonce, i_nonce_len) != 0) {
		dpp_auth_fail(auth, "I-nonce mismatch");
		goto fail;
	}

	if (auth->own_bi) {
		/* Mutual authentication */
		if (dpp_auth_derive_l_initiator(auth) < 0)
			goto fail;
	}

	r_capab = dpp_get_attr(unwrapped, unwrapped_len,
			       DPP_ATTR_R_CAPABILITIES,
			       &r_capab_len);
	if (!r_capab || r_capab_len < 1) {
		dpp_auth_fail(auth, "Missing or invalid R-capabilities");
		goto fail;
	}
	auth->r_capab = r_capab[0];
	wpa_printf(MSG_DEBUG, "DPP: R-capabilities: 0x%02x", auth->r_capab);
	role = auth->r_capab & DPP_CAPAB_ROLE_MASK;
	if ((auth->allowed_roles ==
	     (DPP_CAPAB_CONFIGURATOR | DPP_CAPAB_ENROLLEE)) &&
	    (role == DPP_CAPAB_CONFIGURATOR || role == DPP_CAPAB_ENROLLEE)) {
		/* Peer selected its role, so move from "either role" to the
		 * role that is compatible with peer's selection. */
		auth->configurator = role == DPP_CAPAB_ENROLLEE;
		wpa_printf(MSG_DEBUG, "DPP: Acting as %s",
			   auth->configurator ? "Configurator" : "Enrollee");
	} else if ((auth->configurator && role != DPP_CAPAB_ENROLLEE) ||
		   (!auth->configurator && role != DPP_CAPAB_CONFIGURATOR)) {
		wpa_printf(MSG_DEBUG, "DPP: Incompatible role selection");
		wpa_msg(auth->msg_ctx, MSG_INFO, DPP_EVENT_FAIL
			"Unexpected role in R-capabilities 0x%02x",
			role);
		if (role != DPP_CAPAB_ENROLLEE &&
		    role != DPP_CAPAB_CONFIGURATOR)
			goto fail;
		bin_clear_free(unwrapped, unwrapped_len);
		auth->remove_on_tx_status = 1;
		return dpp_auth_build_conf(auth, DPP_STATUS_NOT_COMPATIBLE);
	}

	wrapped2 = dpp_get_attr(unwrapped, unwrapped_len,
				DPP_ATTR_WRAPPED_DATA, &wrapped2_len);
	if (!wrapped2 || wrapped2_len < AES_BLOCK_SIZE) {
		dpp_auth_fail(auth,
			      "Missing or invalid Secondary Wrapped Data");
		goto fail;
	}

	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped2, wrapped2_len);

	if (dpp_derive_bk_ke(auth) < 0)
		goto fail;

	unwrapped2_len = wrapped2_len - AES_BLOCK_SIZE;
	unwrapped2 = os_malloc(unwrapped2_len);
	if (!unwrapped2)
		goto fail;
	if (aes_siv_decrypt(auth->ke, auth->curve->hash_len,
			    wrapped2, wrapped2_len,
			    0, NULL, NULL, unwrapped2) < 0) {
		dpp_auth_fail(auth, "AES-SIV decryption failed");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV cleartext",
		    unwrapped2, unwrapped2_len);

	if (dpp_check_attrs(unwrapped2, unwrapped2_len) < 0) {
		dpp_auth_fail(auth,
			      "Invalid attribute in secondary unwrapped data");
		goto fail;
	}

	r_auth = dpp_get_attr(unwrapped2, unwrapped2_len, DPP_ATTR_R_AUTH_TAG,
			       &r_auth_len);
	if (!r_auth || r_auth_len != auth->curve->hash_len) {
		dpp_auth_fail(auth,
			      "Missing or invalid Responder Authenticating Tag");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: Received Responder Authenticating Tag",
		    r_auth, r_auth_len);
	/* R-auth' = H(I-nonce | R-nonce | PI.x | PR.x | [BI.x |] BR.x | 0) */
	if (dpp_gen_r_auth(auth, r_auth2) < 0)
		goto fail;
	wpa_hexdump(MSG_DEBUG, "DPP: Calculated Responder Authenticating Tag",
		    r_auth2, r_auth_len);
	if (os_memcmp(r_auth, r_auth2, r_auth_len) != 0) {
		dpp_auth_fail(auth, "Mismatching Responder Authenticating Tag");
		bin_clear_free(unwrapped, unwrapped_len);
		bin_clear_free(unwrapped2, unwrapped2_len);
		auth->remove_on_tx_status = 1;
		return dpp_auth_build_conf(auth, DPP_STATUS_AUTH_FAILURE);
	}

	bin_clear_free(unwrapped, unwrapped_len);
	bin_clear_free(unwrapped2, unwrapped2_len);

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_AUTH_RESP_IN_PLACE_OF_CONF) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - Authentication Response in place of Confirm");
		if (dpp_auth_build_resp_ok(auth) < 0)
			return NULL;
		return wpabuf_dup(auth->resp_msg);
	}
#endif /* CONFIG_TESTING_OPTIONS */

	return dpp_auth_build_conf(auth, DPP_STATUS_OK);

fail:
	bin_clear_free(unwrapped, unwrapped_len);
	bin_clear_free(unwrapped2, unwrapped2_len);
	EVP_PKEY_free(pr);
	return NULL;
}


static int dpp_auth_conf_rx_failure(struct dpp_authentication *auth,
				    const u8 *hdr,
				    const u8 *attr_start, size_t attr_len,
				    const u8 *wrapped_data,
				    u16 wrapped_data_len,
				    enum dpp_status_error status)
{
	const u8 *addr[2];
	size_t len[2];
	u8 *unwrapped = NULL;
	size_t unwrapped_len = 0;
	const u8 *r_nonce;
	u16 r_nonce_len;

	/* Authentication Confirm failure cases are expected to include
	 * {R-nonce}k2 in the Wrapped Data attribute. */

	addr[0] = hdr;
	len[0] = DPP_HDR_LEN;
	addr[1] = attr_start;
	len[1] = attr_len;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped_data, wrapped_data_len);
	unwrapped_len = wrapped_data_len - AES_BLOCK_SIZE;
	unwrapped = os_malloc(unwrapped_len);
	if (!unwrapped) {
		dpp_auth_fail(auth, "Authentication failed");
		goto fail;
	}
	if (aes_siv_decrypt(auth->k2, auth->curve->hash_len,
			    wrapped_data, wrapped_data_len,
			    2, addr, len, unwrapped) < 0) {
		dpp_auth_fail(auth, "AES-SIV decryption failed");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV cleartext",
		    unwrapped, unwrapped_len);

	if (dpp_check_attrs(unwrapped, unwrapped_len) < 0) {
		dpp_auth_fail(auth, "Invalid attribute in unwrapped data");
		goto fail;
	}

	r_nonce = dpp_get_attr(unwrapped, unwrapped_len, DPP_ATTR_R_NONCE,
			       &r_nonce_len);
	if (!r_nonce || r_nonce_len != auth->curve->nonce_len) {
		dpp_auth_fail(auth, "DPP: Missing or invalid R-nonce");
		goto fail;
	}
	if (os_memcmp(r_nonce, auth->r_nonce, r_nonce_len) != 0) {
		wpa_hexdump(MSG_DEBUG, "DPP: Received R-nonce",
			    r_nonce, r_nonce_len);
		wpa_hexdump(MSG_DEBUG, "DPP: Expected R-nonce",
			    auth->r_nonce, r_nonce_len);
		dpp_auth_fail(auth, "R-nonce mismatch");
		goto fail;
	}

	if (status == DPP_STATUS_NOT_COMPATIBLE)
		dpp_auth_fail(auth, "Peer reported incompatible R-capab role");
	else if (status == DPP_STATUS_AUTH_FAILURE)
		dpp_auth_fail(auth, "Peer reported authentication failure)");

fail:
	bin_clear_free(unwrapped, unwrapped_len);
	return -1;
}


int dpp_auth_conf_rx(struct dpp_authentication *auth, const u8 *hdr,
		     const u8 *attr_start, size_t attr_len)
{
	const u8 *r_bootstrap, *i_bootstrap, *wrapped_data, *status, *i_auth;
	u16 r_bootstrap_len, i_bootstrap_len, wrapped_data_len, status_len,
		i_auth_len;
	const u8 *addr[2];
	size_t len[2];
	u8 *unwrapped = NULL;
	size_t unwrapped_len = 0;
	u8 i_auth2[DPP_MAX_HASH_LEN];

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_STOP_AT_AUTH_CONF) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - stop at Authentication Confirm");
		return -1;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (auth->initiator || !auth->own_bi || !auth->waiting_auth_conf ||
	    auth->reconfig) {
		wpa_printf(MSG_DEBUG,
			   "DPP: initiator=%d own_bi=%d waiting_auth_conf=%d",
			   auth->initiator, !!auth->own_bi,
			   auth->waiting_auth_conf);
		dpp_auth_fail(auth, "Unexpected Authentication Confirm");
		return -1;
	}

	auth->waiting_auth_conf = 0;

	wrapped_data = dpp_get_attr(attr_start, attr_len, DPP_ATTR_WRAPPED_DATA,
				    &wrapped_data_len);
	if (!wrapped_data || wrapped_data_len < AES_BLOCK_SIZE) {
		dpp_auth_fail(auth,
			      "Missing or invalid required Wrapped Data attribute");
		return -1;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: Wrapped data",
		    wrapped_data, wrapped_data_len);

	attr_len = wrapped_data - 4 - attr_start;

	r_bootstrap = dpp_get_attr(attr_start, attr_len,
				   DPP_ATTR_R_BOOTSTRAP_KEY_HASH,
				   &r_bootstrap_len);
	if (!r_bootstrap || r_bootstrap_len != SHA256_MAC_LEN) {
		dpp_auth_fail(auth,
			      "Missing or invalid required Responder Bootstrapping Key Hash attribute");
		return -1;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: Responder Bootstrapping Key Hash",
		    r_bootstrap, r_bootstrap_len);
	if (os_memcmp(r_bootstrap, auth->own_bi->pubkey_hash,
		      SHA256_MAC_LEN) != 0) {
		wpa_hexdump(MSG_DEBUG,
			    "DPP: Expected Responder Bootstrapping Key Hash",
			    auth->peer_bi->pubkey_hash, SHA256_MAC_LEN);
		dpp_auth_fail(auth,
			      "Responder Bootstrapping Key Hash mismatch");
		return -1;
	}

	i_bootstrap = dpp_get_attr(attr_start, attr_len,
				   DPP_ATTR_I_BOOTSTRAP_KEY_HASH,
				   &i_bootstrap_len);
	if (i_bootstrap) {
		if (i_bootstrap_len != SHA256_MAC_LEN) {
			dpp_auth_fail(auth,
				      "Invalid Initiator Bootstrapping Key Hash attribute");
			return -1;
		}
		wpa_hexdump(MSG_MSGDUMP,
			    "DPP: Initiator Bootstrapping Key Hash",
			    i_bootstrap, i_bootstrap_len);
		if (!auth->peer_bi ||
		    os_memcmp(i_bootstrap, auth->peer_bi->pubkey_hash,
			      SHA256_MAC_LEN) != 0) {
			dpp_auth_fail(auth,
				      "Initiator Bootstrapping Key Hash mismatch");
			return -1;
		}
	} else if (auth->peer_bi) {
		/* Mutual authentication and peer did not include its
		 * Bootstrapping Key Hash attribute. */
		dpp_auth_fail(auth,
			      "Missing Initiator Bootstrapping Key Hash attribute");
		return -1;
	}

	status = dpp_get_attr(attr_start, attr_len, DPP_ATTR_STATUS,
			      &status_len);
	if (!status || status_len < 1) {
		dpp_auth_fail(auth,
			      "Missing or invalid required DPP Status attribute");
		return -1;
	}
	wpa_printf(MSG_DEBUG, "DPP: Status %u", status[0]);
	if (status[0] == DPP_STATUS_NOT_COMPATIBLE ||
	    status[0] == DPP_STATUS_AUTH_FAILURE)
		return dpp_auth_conf_rx_failure(auth, hdr, attr_start,
						attr_len, wrapped_data,
						wrapped_data_len, status[0]);

	if (status[0] != DPP_STATUS_OK) {
		dpp_auth_fail(auth, "Authentication failed");
		return -1;
	}

	addr[0] = hdr;
	len[0] = DPP_HDR_LEN;
	addr[1] = attr_start;
	len[1] = attr_len;
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped_data, wrapped_data_len);
	unwrapped_len = wrapped_data_len - AES_BLOCK_SIZE;
	unwrapped = os_malloc(unwrapped_len);
	if (!unwrapped)
		return -1;
	if (aes_siv_decrypt(auth->ke, auth->curve->hash_len,
			    wrapped_data, wrapped_data_len,
			    2, addr, len, unwrapped) < 0) {
		dpp_auth_fail(auth, "AES-SIV decryption failed");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV cleartext",
		    unwrapped, unwrapped_len);

	if (dpp_check_attrs(unwrapped, unwrapped_len) < 0) {
		dpp_auth_fail(auth, "Invalid attribute in unwrapped data");
		goto fail;
	}

	i_auth = dpp_get_attr(unwrapped, unwrapped_len, DPP_ATTR_I_AUTH_TAG,
			      &i_auth_len);
	if (!i_auth || i_auth_len != auth->curve->hash_len) {
		dpp_auth_fail(auth,
			      "Missing or invalid Initiator Authenticating Tag");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: Received Initiator Authenticating Tag",
		    i_auth, i_auth_len);
	/* I-auth' = H(R-nonce | I-nonce | PR.x | PI.x | BR.x | [BI.x |] 1) */
	if (dpp_gen_i_auth(auth, i_auth2) < 0)
		goto fail;
	wpa_hexdump(MSG_DEBUG, "DPP: Calculated Initiator Authenticating Tag",
		    i_auth2, i_auth_len);
	if (os_memcmp(i_auth, i_auth2, i_auth_len) != 0) {
		dpp_auth_fail(auth, "Mismatching Initiator Authenticating Tag");
		goto fail;
	}

	bin_clear_free(unwrapped, unwrapped_len);
	dpp_auth_success(auth);
	return 0;
fail:
	bin_clear_free(unwrapped, unwrapped_len);
	return -1;
}
