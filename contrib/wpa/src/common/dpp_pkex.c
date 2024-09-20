/*
 * DPP PKEX functionality
 * Copyright (c) 2017, Qualcomm Atheros, Inc.
 * Copyright (c) 2018-2020, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "common/wpa_ctrl.h"
#include "crypto/aes.h"
#include "crypto/aes_siv.h"
#include "crypto/crypto.h"
#include "dpp.h"
#include "dpp_i.h"


#ifdef CONFIG_TESTING_OPTIONS
u8 dpp_pkex_own_mac_override[ETH_ALEN] = { 0, 0, 0, 0, 0, 0 };
u8 dpp_pkex_peer_mac_override[ETH_ALEN] = { 0, 0, 0, 0, 0, 0 };
u8 dpp_pkex_ephemeral_key_override[600];
size_t dpp_pkex_ephemeral_key_override_len = 0;
#endif /* CONFIG_TESTING_OPTIONS */


static struct wpabuf * dpp_pkex_build_exchange_req(struct dpp_pkex *pkex,
						   bool v2)
{
	struct crypto_ec *ec = NULL;
	struct crypto_ec_point *Qi = NULL, *M = NULL, *X = NULL;
	u8 *Mx, *My;
	struct wpabuf *msg = NULL;
	size_t attr_len;
	const struct dpp_curve_params *curve = pkex->own_bi->curve;

	wpa_printf(MSG_DEBUG, "DPP: Build PKEX %sExchange Request",
		   v2 ? "" : "Version 1 ");

	/* Qi = H([MAC-Initiator |] [identifier |] code) * Pi */
	Qi = dpp_pkex_derive_Qi(curve, v2 ? NULL : pkex->own_mac, pkex->code,
				pkex->code_len, pkex->identifier, &ec);
	if (!Qi)
		goto fail;

	/* Generate a random ephemeral keypair x/X */
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_pkex_ephemeral_key_override_len) {
		const struct dpp_curve_params *tmp_curve;

		wpa_printf(MSG_INFO,
			   "DPP: TESTING - override ephemeral key x/X");
		pkex->x = dpp_set_keypair(&tmp_curve,
					  dpp_pkex_ephemeral_key_override,
					  dpp_pkex_ephemeral_key_override_len);
	} else {
		pkex->x = dpp_gen_keypair(curve);
	}
#else /* CONFIG_TESTING_OPTIONS */
	pkex->x = dpp_gen_keypair(curve);
#endif /* CONFIG_TESTING_OPTIONS */
	if (!pkex->x)
		goto fail;

	/* M = X + Qi */
	X = crypto_ec_key_get_public_key(pkex->x);
	M = crypto_ec_point_init(ec);
	if (!X || !M)
		goto fail;
	crypto_ec_point_debug_print(ec, X, "DPP: X");

	if (crypto_ec_point_add(ec, X, Qi, M))
		goto fail;
	crypto_ec_point_debug_print(ec, M, "DPP: M");

	/* Initiator -> Responder: group, [identifier,] M */
	attr_len = 4 + 2;
#ifdef CONFIG_DPP2
	if (v2)
		attr_len += 4 + 1;
#endif /* CONFIG_DPP2 */
	if (pkex->identifier)
		attr_len += 4 + os_strlen(pkex->identifier);
	attr_len += 4 + 2 * curve->prime_len;
	msg = dpp_alloc_msg(v2 ? DPP_PA_PKEX_EXCHANGE_REQ :
			    DPP_PA_PKEX_V1_EXCHANGE_REQ, attr_len);
	if (!msg)
		goto fail;

#ifdef CONFIG_DPP2
	if (v2) {
		/* Protocol Version */
		wpabuf_put_le16(msg, DPP_ATTR_PROTOCOL_VERSION);
		wpabuf_put_le16(msg, 1);
		wpabuf_put_u8(msg, DPP_VERSION);
	}
#endif /* CONFIG_DPP2 */

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_FINITE_CYCLIC_GROUP_PKEX_EXCHANGE_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Finite Cyclic Group");
		goto skip_finite_cyclic_group;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* Finite Cyclic Group attribute */
	wpabuf_put_le16(msg, DPP_ATTR_FINITE_CYCLIC_GROUP);
	wpabuf_put_le16(msg, 2);
	wpabuf_put_le16(msg, curve->ike_group);

#ifdef CONFIG_TESTING_OPTIONS
skip_finite_cyclic_group:
#endif /* CONFIG_TESTING_OPTIONS */

	/* Code Identifier attribute */
	if (pkex->identifier) {
		wpabuf_put_le16(msg, DPP_ATTR_CODE_IDENTIFIER);
		wpabuf_put_le16(msg, os_strlen(pkex->identifier));
		wpabuf_put_str(msg, pkex->identifier);
	}

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_ENCRYPTED_KEY_PKEX_EXCHANGE_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Encrypted Key");
		goto out;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* M in Encrypted Key attribute */
	wpabuf_put_le16(msg, DPP_ATTR_ENCRYPTED_KEY);
	wpabuf_put_le16(msg, 2 * curve->prime_len);

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_INVALID_ENCRYPTED_KEY_PKEX_EXCHANGE_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - invalid Encrypted Key");
		if (dpp_test_gen_invalid_key(msg, curve) < 0)
			goto fail;
		goto out;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	Mx = wpabuf_put(msg, curve->prime_len);
	My = wpabuf_put(msg, curve->prime_len);
	if (crypto_ec_point_to_bin(ec, M, Mx, My))
		goto fail;
	wpabuf_free(pkex->enc_key);
	pkex->enc_key = wpabuf_alloc_copy(Mx, 2 * curve->prime_len);

	os_memcpy(pkex->Mx, Mx, curve->prime_len);

out:
	crypto_ec_point_deinit(X, 1);
	crypto_ec_point_deinit(M, 1);
	crypto_ec_point_deinit(Qi, 1);
	crypto_ec_deinit(ec);
	return msg;
fail:
	wpa_printf(MSG_INFO, "DPP: Failed to build PKEX Exchange Request");
	wpabuf_free(msg);
	msg = NULL;
	goto out;
}


static void dpp_pkex_fail(struct dpp_pkex *pkex, const char *txt)
{
	wpa_msg(pkex->msg_ctx, MSG_INFO, DPP_EVENT_FAIL "%s", txt);
}


struct dpp_pkex * dpp_pkex_init(void *msg_ctx, struct dpp_bootstrap_info *bi,
				const u8 *own_mac,
				const char *identifier, const char *code,
				size_t code_len, bool v2)
{
	struct dpp_pkex *pkex;

#ifdef CONFIG_TESTING_OPTIONS
	if (!is_zero_ether_addr(dpp_pkex_own_mac_override)) {
		wpa_printf(MSG_INFO, "DPP: TESTING - own_mac override " MACSTR,
			   MAC2STR(dpp_pkex_own_mac_override));
		own_mac = dpp_pkex_own_mac_override;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	pkex = os_zalloc(sizeof(*pkex));
	if (!pkex)
		return NULL;
	pkex->msg_ctx = msg_ctx;
	pkex->initiator = 1;
	pkex->v2 = v2;
	pkex->own_bi = bi;
	os_memcpy(pkex->own_mac, own_mac, ETH_ALEN);
	if (identifier) {
		pkex->identifier = os_strdup(identifier);
		if (!pkex->identifier)
			goto fail;
	}
	pkex->code = os_memdup(code, code_len);
	if (!pkex->code)
		goto fail;
	pkex->code_len = code_len;
	pkex->exchange_req = dpp_pkex_build_exchange_req(pkex, v2);
	if (!pkex->exchange_req)
		goto fail;
	return pkex;
fail:
	dpp_pkex_free(pkex);
	return NULL;
}


static struct wpabuf *
dpp_pkex_build_exchange_resp(struct dpp_pkex *pkex,
			     enum dpp_status_error status,
			     const u8 *Nx, const u8 *Ny)
{
	struct wpabuf *msg = NULL;
	size_t attr_len;
	const struct dpp_curve_params *curve = pkex->own_bi->curve;

	/* Initiator -> Responder: DPP Status, [Protocol Version,] [identifier,]
	 * N */
	attr_len = 4 + 1;
#ifdef CONFIG_DPP2
	if (pkex->v2)
		attr_len += 4 + 1;
#endif /* CONFIG_DPP2 */
	if (pkex->identifier)
		attr_len += 4 + os_strlen(pkex->identifier);
	attr_len += 4 + 2 * curve->prime_len;
	msg = dpp_alloc_msg(DPP_PA_PKEX_EXCHANGE_RESP, attr_len);
	if (!msg)
		goto fail;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_STATUS_PKEX_EXCHANGE_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Status");
		goto skip_status;
	}

	if (dpp_test == DPP_TEST_INVALID_STATUS_PKEX_EXCHANGE_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - invalid Status");
		status = 255;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* DPP Status */
	dpp_build_attr_status(msg, status);

#ifdef CONFIG_TESTING_OPTIONS
skip_status:
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_DPP2
	if (pkex->v2) {
		/* Protocol Version */
		wpabuf_put_le16(msg, DPP_ATTR_PROTOCOL_VERSION);
		wpabuf_put_le16(msg, 1);
		wpabuf_put_u8(msg, DPP_VERSION);
	}
#endif /* CONFIG_DPP2 */

	/* Code Identifier attribute */
	if (pkex->identifier) {
		wpabuf_put_le16(msg, DPP_ATTR_CODE_IDENTIFIER);
		wpabuf_put_le16(msg, os_strlen(pkex->identifier));
		wpabuf_put_str(msg, pkex->identifier);
	}

	if (status != DPP_STATUS_OK)
		goto skip_encrypted_key;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_ENCRYPTED_KEY_PKEX_EXCHANGE_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Encrypted Key");
		goto skip_encrypted_key;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* N in Encrypted Key attribute */
	wpabuf_put_le16(msg, DPP_ATTR_ENCRYPTED_KEY);
	wpabuf_put_le16(msg, 2 * curve->prime_len);

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_INVALID_ENCRYPTED_KEY_PKEX_EXCHANGE_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - invalid Encrypted Key");
		if (dpp_test_gen_invalid_key(msg, curve) < 0)
			goto fail;
		goto skip_encrypted_key;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	wpabuf_put_data(msg, Nx, curve->prime_len);
	wpabuf_put_data(msg, Ny, curve->prime_len);
	os_memcpy(pkex->Nx, Nx, curve->prime_len);

skip_encrypted_key:
	if (status == DPP_STATUS_BAD_GROUP) {
		/* Finite Cyclic Group attribute */
		wpabuf_put_le16(msg, DPP_ATTR_FINITE_CYCLIC_GROUP);
		wpabuf_put_le16(msg, 2);
		wpabuf_put_le16(msg, curve->ike_group);
	}

	return msg;
fail:
	wpabuf_free(msg);
	return NULL;
}


static int dpp_pkex_identifier_match(const u8 *attr_id, u16 attr_id_len,
				     const char *identifier)
{
	if (!attr_id && identifier) {
		wpa_printf(MSG_DEBUG,
			   "DPP: No PKEX code identifier received, but expected one");
		return 0;
	}

	if (attr_id && !identifier) {
		wpa_printf(MSG_DEBUG,
			   "DPP: PKEX code identifier received, but not expecting one");
		return 0;
	}

	if (attr_id && identifier &&
	    (os_strlen(identifier) != attr_id_len ||
	     os_memcmp(identifier, attr_id, attr_id_len) != 0)) {
		wpa_printf(MSG_DEBUG, "DPP: PKEX code identifier mismatch");
		return 0;
	}

	return 1;
}


struct dpp_pkex * dpp_pkex_rx_exchange_req(void *msg_ctx,
					   struct dpp_bootstrap_info *bi,
					   const u8 *own_mac,
					   const u8 *peer_mac,
					   const char *identifier,
					   const char *code, size_t code_len,
					   const u8 *buf, size_t len, bool v2)
{
	const u8 *attr_group, *attr_id, *attr_key;
	u16 attr_group_len, attr_id_len, attr_key_len;
	const struct dpp_curve_params *curve = bi->curve;
	u16 ike_group;
	struct dpp_pkex *pkex = NULL;
	struct crypto_ec_point *Qi = NULL, *Qr = NULL, *M = NULL, *X = NULL,
		*N = NULL, *Y = NULL;
	struct crypto_ec *ec = NULL;
	u8 *x_coord = NULL, *y_coord = NULL;
	u8 Kx[DPP_MAX_SHARED_SECRET_LEN];
	size_t Kx_len;
	int res;
	u8 peer_version = 0;

	if (bi->pkex_t >= PKEX_COUNTER_T_LIMIT) {
		wpa_msg(msg_ctx, MSG_INFO, DPP_EVENT_FAIL
			"PKEX counter t limit reached - ignore message");
		return NULL;
	}

#ifdef CONFIG_DPP2
	if (v2) {
		const u8 *version;
		u16 version_len;

		version = dpp_get_attr(buf, len, DPP_ATTR_PROTOCOL_VERSION,
				       &version_len);
		if (!version || version_len < 1 || version[0] == 0) {
			wpa_msg(msg_ctx, MSG_INFO,
				"Missing or invalid Protocol Version attribute");
			return NULL;
		}
		peer_version = version[0];
		wpa_printf(MSG_DEBUG, "DPP: Peer protocol version %u",
			   peer_version);
	}
#endif /* CONFIG_DPP2 */

#ifdef CONFIG_TESTING_OPTIONS
	if (!is_zero_ether_addr(dpp_pkex_peer_mac_override)) {
		wpa_printf(MSG_INFO, "DPP: TESTING - peer_mac override " MACSTR,
			   MAC2STR(dpp_pkex_peer_mac_override));
		peer_mac = dpp_pkex_peer_mac_override;
	}
	if (!is_zero_ether_addr(dpp_pkex_own_mac_override)) {
		wpa_printf(MSG_INFO, "DPP: TESTING - own_mac override " MACSTR,
			   MAC2STR(dpp_pkex_own_mac_override));
		own_mac = dpp_pkex_own_mac_override;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	attr_id_len = 0;
	attr_id = dpp_get_attr(buf, len, DPP_ATTR_CODE_IDENTIFIER,
			       &attr_id_len);
	if (!dpp_pkex_identifier_match(attr_id, attr_id_len, identifier))
		return NULL;

	attr_group = dpp_get_attr(buf, len, DPP_ATTR_FINITE_CYCLIC_GROUP,
				  &attr_group_len);
	if (!attr_group || attr_group_len != 2) {
		wpa_msg(msg_ctx, MSG_INFO, DPP_EVENT_FAIL
			"Missing or invalid Finite Cyclic Group attribute");
		return NULL;
	}
	ike_group = WPA_GET_LE16(attr_group);
	if (ike_group != curve->ike_group) {
		wpa_msg(msg_ctx, MSG_INFO, DPP_EVENT_FAIL
			"Mismatching PKEX curve: peer=%u own=%u",
			ike_group, curve->ike_group);
		pkex = os_zalloc(sizeof(*pkex));
		if (!pkex)
			goto fail;
		pkex->v2 = v2;
		pkex->peer_version = peer_version;
		pkex->own_bi = bi;
		pkex->failed = 1;
		pkex->exchange_resp = dpp_pkex_build_exchange_resp(
			pkex, DPP_STATUS_BAD_GROUP, NULL, NULL);
		if (!pkex->exchange_resp)
			goto fail;
		return pkex;
	}

	/* M in Encrypted Key attribute */
	attr_key = dpp_get_attr(buf, len, DPP_ATTR_ENCRYPTED_KEY,
				&attr_key_len);
	if (!attr_key || attr_key_len & 0x01 || attr_key_len < 2 ||
	    attr_key_len / 2 > DPP_MAX_SHARED_SECRET_LEN) {
		wpa_msg(msg_ctx, MSG_INFO, DPP_EVENT_FAIL
			"Missing Encrypted Key attribute");
		return NULL;
	}

	/* Qi = H([MAC-Initiator |] [identifier |] code) * Pi */
	Qi = dpp_pkex_derive_Qi(curve, v2 ? NULL : peer_mac, code, code_len,
				identifier, &ec);
	if (!Qi)
		goto fail;

	/* X' = M - Qi */
	X = crypto_ec_point_init(ec);
	M = crypto_ec_point_from_bin(ec, attr_key);
	if (!X || !M ||
	    crypto_ec_point_is_at_infinity(ec, M) ||
	    !crypto_ec_point_is_on_curve(ec, M) ||
	    crypto_ec_point_invert(ec, Qi) ||
	    crypto_ec_point_add(ec, M, Qi, X) ||
	    crypto_ec_point_is_at_infinity(ec, X) ||
	    !crypto_ec_point_is_on_curve(ec, X)) {
		wpa_msg(msg_ctx, MSG_INFO, DPP_EVENT_FAIL
			"Invalid Encrypted Key value");
		bi->pkex_t++;
		goto fail;
	}
	crypto_ec_point_debug_print(ec, M, "DPP: M");
	crypto_ec_point_debug_print(ec, X, "DPP: X'");

	pkex = os_zalloc(sizeof(*pkex));
	if (!pkex)
		goto fail;
	pkex->v2 = v2;
	pkex->peer_version = peer_version;
	pkex->t = bi->pkex_t;
	pkex->msg_ctx = msg_ctx;
	pkex->own_bi = bi;
	if (own_mac)
		os_memcpy(pkex->own_mac, own_mac, ETH_ALEN);
	if (peer_mac)
		os_memcpy(pkex->peer_mac, peer_mac, ETH_ALEN);
	if (identifier) {
		pkex->identifier = os_strdup(identifier);
		if (!pkex->identifier)
			goto fail;
	}
	pkex->code = os_memdup(code, code_len);
	if (!pkex->code)
		goto fail;
	pkex->code_len = code_len;

	os_memcpy(pkex->Mx, attr_key, attr_key_len / 2);

	x_coord = os_malloc(curve->prime_len);
	y_coord = os_malloc(curve->prime_len);
	if (!x_coord || !y_coord ||
	    crypto_ec_point_to_bin(ec, X, x_coord, y_coord))
		goto fail;

	pkex->x = crypto_ec_key_set_pub(curve->ike_group, x_coord,
					y_coord, crypto_ec_prime_len(ec));
	if (!pkex->x)
		goto fail;

	/* Qr = H([MAC-Responder |] [identifier |] code) * Pr */
	Qr = dpp_pkex_derive_Qr(curve, v2 ? NULL : own_mac, code, code_len,
				identifier, NULL);
	if (!Qr)
		goto fail;

	/* Generate a random ephemeral keypair y/Y */
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_pkex_ephemeral_key_override_len) {
		const struct dpp_curve_params *tmp_curve;

		wpa_printf(MSG_INFO,
			   "DPP: TESTING - override ephemeral key y/Y");
		pkex->y = dpp_set_keypair(&tmp_curve,
					  dpp_pkex_ephemeral_key_override,
					  dpp_pkex_ephemeral_key_override_len);
	} else {
		pkex->y = dpp_gen_keypair(curve);
	}
#else /* CONFIG_TESTING_OPTIONS */
	pkex->y = dpp_gen_keypair(curve);
#endif /* CONFIG_TESTING_OPTIONS */
	if (!pkex->y)
		goto fail;

	/* N = Y + Qr */
	Y = crypto_ec_key_get_public_key(pkex->y);
	if (!Y)
		goto fail;
	crypto_ec_point_debug_print(ec, Y, "DPP: Y");

	N = crypto_ec_point_init(ec);
	if (!N ||
	    crypto_ec_point_add(ec, Y, Qr, N) ||
	    crypto_ec_point_to_bin(ec, N, x_coord, y_coord))
		goto fail;
	crypto_ec_point_debug_print(ec, N, "DPP: N");

	pkex->exchange_resp = dpp_pkex_build_exchange_resp(pkex, DPP_STATUS_OK,
							   x_coord, y_coord);
	if (!pkex->exchange_resp)
		goto fail;

	/* K = y * X' */
	if (dpp_ecdh(pkex->y, pkex->x, Kx, &Kx_len) < 0)
		goto fail;

	wpa_hexdump_key(MSG_DEBUG, "DPP: ECDH shared secret (K.x)",
			Kx, Kx_len);

	/* z = HKDF(<>, info | M.x | N.x | code, K.x) */
	res = dpp_pkex_derive_z(pkex->v2 ? NULL : pkex->peer_mac,
				pkex->v2 ? NULL : pkex->own_mac,
				pkex->peer_version, DPP_VERSION,
				pkex->Mx, curve->prime_len,
				pkex->Nx, curve->prime_len, pkex->code,
				pkex->code_len,	Kx, Kx_len, pkex->z,
				curve->hash_len);
	os_memset(Kx, 0, Kx_len);
	if (res < 0)
		goto fail;

	pkex->exchange_done = 1;

out:
	os_free(x_coord);
	os_free(y_coord);
	crypto_ec_point_deinit(Qi, 1);
	crypto_ec_point_deinit(Qr, 1);
	crypto_ec_point_deinit(M, 1);
	crypto_ec_point_deinit(N, 1);
	crypto_ec_point_deinit(X, 1);
	crypto_ec_point_deinit(Y, 1);
	crypto_ec_deinit(ec);
	return pkex;
fail:
	wpa_printf(MSG_DEBUG, "DPP: PKEX Exchange Request processing failed");
	dpp_pkex_free(pkex);
	pkex = NULL;
	goto out;
}


static struct wpabuf *
dpp_pkex_build_commit_reveal_req(struct dpp_pkex *pkex,
				 const struct wpabuf *A_pub, const u8 *u)
{
	const struct dpp_curve_params *curve = pkex->own_bi->curve;
	struct wpabuf *msg = NULL;
	size_t clear_len, attr_len;
	struct wpabuf *clear = NULL;
	u8 *wrapped;
	u8 octet;
	const u8 *addr[2];
	size_t len[2];

	/* {A, u, [bootstrapping info]}z */
	clear_len = 4 + 2 * curve->prime_len + 4 + curve->hash_len;
	clear = wpabuf_alloc(clear_len);
	attr_len = 4 + clear_len + AES_BLOCK_SIZE;
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_AFTER_WRAPPED_DATA_PKEX_CR_REQ)
		attr_len += 5;
#endif /* CONFIG_TESTING_OPTIONS */
	msg = dpp_alloc_msg(DPP_PA_PKEX_COMMIT_REVEAL_REQ, attr_len);
	if (!clear || !msg)
		goto fail;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_BOOTSTRAP_KEY_PKEX_CR_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Bootstrap Key");
		goto skip_bootstrap_key;
	}
	if (dpp_test == DPP_TEST_INVALID_BOOTSTRAP_KEY_PKEX_CR_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - invalid Bootstrap Key");
		wpabuf_put_le16(clear, DPP_ATTR_BOOTSTRAP_KEY);
		wpabuf_put_le16(clear, 2 * curve->prime_len);
		if (dpp_test_gen_invalid_key(clear, curve) < 0)
			goto fail;
		goto skip_bootstrap_key;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* A in Bootstrap Key attribute */
	wpabuf_put_le16(clear, DPP_ATTR_BOOTSTRAP_KEY);
	wpabuf_put_le16(clear, wpabuf_len(A_pub));
	wpabuf_put_buf(clear, A_pub);

#ifdef CONFIG_TESTING_OPTIONS
skip_bootstrap_key:
	if (dpp_test == DPP_TEST_NO_I_AUTH_TAG_PKEX_CR_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no I-Auth tag");
		goto skip_i_auth_tag;
	}
	if (dpp_test == DPP_TEST_I_AUTH_TAG_MISMATCH_PKEX_CR_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - I-Auth tag mismatch");
		wpabuf_put_le16(clear, DPP_ATTR_I_AUTH_TAG);
		wpabuf_put_le16(clear, curve->hash_len);
		wpabuf_put_data(clear, u, curve->hash_len - 1);
		wpabuf_put_u8(clear, u[curve->hash_len - 1] ^ 0x01);
		goto skip_i_auth_tag;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* u in I-Auth tag attribute */
	wpabuf_put_le16(clear, DPP_ATTR_I_AUTH_TAG);
	wpabuf_put_le16(clear, curve->hash_len);
	wpabuf_put_data(clear, u, curve->hash_len);

#ifdef CONFIG_TESTING_OPTIONS
skip_i_auth_tag:
	if (dpp_test == DPP_TEST_NO_WRAPPED_DATA_PKEX_CR_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Wrapped Data");
		goto skip_wrapped_data;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	addr[0] = wpabuf_head_u8(msg) + 2;
	len[0] = DPP_HDR_LEN;
	octet = 0;
	addr[1] = &octet;
	len[1] = sizeof(octet);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);

	wpabuf_put_le16(msg, DPP_ATTR_WRAPPED_DATA);
	wpabuf_put_le16(msg, wpabuf_len(clear) + AES_BLOCK_SIZE);
	wrapped = wpabuf_put(msg, wpabuf_len(clear) + AES_BLOCK_SIZE);

	wpa_hexdump_buf(MSG_DEBUG, "DPP: AES-SIV cleartext", clear);
	if (aes_siv_encrypt(pkex->z, curve->hash_len,
			    wpabuf_head(clear), wpabuf_len(clear),
			    2, addr, len, wrapped) < 0)
		goto fail;
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped, wpabuf_len(clear) + AES_BLOCK_SIZE);

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_AFTER_WRAPPED_DATA_PKEX_CR_REQ) {
		wpa_printf(MSG_INFO, "DPP: TESTING - attr after Wrapped Data");
		dpp_build_attr_status(msg, DPP_STATUS_OK);
	}
skip_wrapped_data:
#endif /* CONFIG_TESTING_OPTIONS */

out:
	wpabuf_free(clear);
	return msg;

fail:
	wpabuf_free(msg);
	msg = NULL;
	goto out;
}


struct wpabuf * dpp_pkex_rx_exchange_resp(struct dpp_pkex *pkex,
					  const u8 *peer_mac,
					  const u8 *buf, size_t buflen)
{
	const u8 *attr_status, *attr_id, *attr_key, *attr_group;
	u16 attr_status_len, attr_id_len, attr_key_len, attr_group_len;
	struct crypto_ec *ec = NULL;
	struct wpabuf *msg = NULL, *A_pub = NULL, *X_pub = NULL, *Y_pub = NULL;
	const struct dpp_curve_params *curve = pkex->own_bi->curve;
	struct crypto_ec_point *Qr = NULL, *Y = NULL, *N = NULL;
	u8 *x_coord = NULL, *y_coord = NULL;
	size_t Jx_len, Kx_len;
	u8 Jx[DPP_MAX_SHARED_SECRET_LEN], Kx[DPP_MAX_SHARED_SECRET_LEN];
	const u8 *addr[4];
	size_t len[4];
	size_t num_elem;
	u8 u[DPP_MAX_HASH_LEN];
	int res;

	if (pkex->failed || pkex->t >= PKEX_COUNTER_T_LIMIT || !pkex->initiator)
		return NULL;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_STOP_AT_PKEX_EXCHANGE_RESP) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - stop at PKEX Exchange Response");
		pkex->failed = 1;
		return NULL;
	}

	if (!is_zero_ether_addr(dpp_pkex_peer_mac_override)) {
		wpa_printf(MSG_INFO, "DPP: TESTING - peer_mac override " MACSTR,
			   MAC2STR(dpp_pkex_peer_mac_override));
		peer_mac = dpp_pkex_peer_mac_override;
	}
#endif /* CONFIG_TESTING_OPTIONS */

#ifdef CONFIG_DPP2
	if (pkex->v2) {
		const u8 *version;
		u16 version_len;

		version = dpp_get_attr(buf, buflen, DPP_ATTR_PROTOCOL_VERSION,
				       &version_len);
		if (!version || version_len < 1 || version[0] == 0) {
		dpp_pkex_fail(pkex,
			      "Missing or invalid Protocol Version attribute");
			return NULL;
		}
		pkex->peer_version = version[0];
		wpa_printf(MSG_DEBUG, "DPP: Peer protocol version %u",
			   pkex->peer_version);
	}
#endif /* CONFIG_DPP2 */

	if (peer_mac)
		os_memcpy(pkex->peer_mac, peer_mac, ETH_ALEN);

	attr_status = dpp_get_attr(buf, buflen, DPP_ATTR_STATUS,
				   &attr_status_len);
	if (!attr_status || attr_status_len != 1) {
		dpp_pkex_fail(pkex, "No DPP Status attribute");
		return NULL;
	}
	wpa_printf(MSG_DEBUG, "DPP: Status %u", attr_status[0]);

	if (attr_status[0] == DPP_STATUS_BAD_GROUP) {
		attr_group = dpp_get_attr(buf, buflen,
					  DPP_ATTR_FINITE_CYCLIC_GROUP,
					  &attr_group_len);
		if (attr_group && attr_group_len == 2) {
			wpa_msg(pkex->msg_ctx, MSG_INFO, DPP_EVENT_FAIL
				"Peer indicated mismatching PKEX group - proposed %u",
				WPA_GET_LE16(attr_group));
			return NULL;
		}
	}

	if (attr_status[0] != DPP_STATUS_OK) {
		dpp_pkex_fail(pkex, "PKEX failed (peer indicated failure)");
		return NULL;
	}

	attr_id_len = 0;
	attr_id = dpp_get_attr(buf, buflen, DPP_ATTR_CODE_IDENTIFIER,
			       &attr_id_len);
	if (!dpp_pkex_identifier_match(attr_id, attr_id_len,
				       pkex->identifier)) {
		dpp_pkex_fail(pkex, "PKEX code identifier mismatch");
		return NULL;
	}

	/* N in Encrypted Key attribute */
	attr_key = dpp_get_attr(buf, buflen, DPP_ATTR_ENCRYPTED_KEY,
				&attr_key_len);
	if (!attr_key || attr_key_len & 0x01 || attr_key_len < 2) {
		dpp_pkex_fail(pkex, "Missing Encrypted Key attribute");
		return NULL;
	}

	/* Qr = H([MAC-Responder |] [identifier |] code) * Pr */
	Qr = dpp_pkex_derive_Qr(curve, pkex->v2 ? NULL : pkex->peer_mac,
				pkex->code, pkex->code_len, pkex->identifier,
				&ec);
	if (!Qr)
		goto fail;

	/* Y' = N - Qr */
	Y = crypto_ec_point_init(ec);
	N = crypto_ec_point_from_bin(ec, attr_key);
	if (!Y || !N ||
	    crypto_ec_point_is_at_infinity(ec, N) ||
	    !crypto_ec_point_is_on_curve(ec, N) ||
	    crypto_ec_point_invert(ec, Qr) ||
	    crypto_ec_point_add(ec, N, Qr, Y) ||
	    crypto_ec_point_is_at_infinity(ec, Y) ||
	    !crypto_ec_point_is_on_curve(ec, Y)) {
		dpp_pkex_fail(pkex, "Invalid Encrypted Key value");
		pkex->t++;
		goto fail;
	}
	crypto_ec_point_debug_print(ec, N, "DPP: N");
	crypto_ec_point_debug_print(ec, Y, "DPP: Y'");

	pkex->exchange_done = 1;

	/* ECDH: J = a * Y' */
	x_coord = os_malloc(curve->prime_len);
	y_coord = os_malloc(curve->prime_len);
	if (!x_coord || !y_coord ||
	    crypto_ec_point_to_bin(ec, Y, x_coord, y_coord))
		goto fail;
	pkex->y = crypto_ec_key_set_pub(curve->ike_group, x_coord, y_coord,
					curve->prime_len);
	if (!pkex->y)
		goto fail;
	if (dpp_ecdh(pkex->own_bi->pubkey, pkex->y, Jx, &Jx_len) < 0)
		goto fail;

	wpa_hexdump_key(MSG_DEBUG, "DPP: ECDH shared secret (J.x)",
			Jx, Jx_len);

	/* u = HMAC(J.x, [MAC-Initiator |] A.x | Y'.x | X.x) */
	A_pub = crypto_ec_key_get_pubkey_point(pkex->own_bi->pubkey, 0);
	Y_pub = crypto_ec_key_get_pubkey_point(pkex->y, 0);
	X_pub = crypto_ec_key_get_pubkey_point(pkex->x, 0);
	if (!A_pub || !Y_pub || !X_pub)
		goto fail;
	num_elem = 0;
	if (!pkex->v2) {
		addr[num_elem] = pkex->own_mac;
		len[num_elem] = ETH_ALEN;
		num_elem++;
	}
	addr[num_elem] = wpabuf_head(A_pub);
	len[num_elem] = wpabuf_len(A_pub) / 2;
	num_elem++;
	addr[num_elem] = wpabuf_head(Y_pub);
	len[num_elem] = wpabuf_len(Y_pub) / 2;
	num_elem++;
	addr[num_elem] = wpabuf_head(X_pub);
	len[num_elem] = wpabuf_len(X_pub) / 2;
	num_elem++;
	if (dpp_hmac_vector(curve->hash_len, Jx, Jx_len, num_elem, addr, len, u)
	    < 0)
		goto fail;
	wpa_hexdump(MSG_DEBUG, "DPP: u", u, curve->hash_len);

	/* K = x * Y' */
	if (dpp_ecdh(pkex->x, pkex->y, Kx, &Kx_len) < 0)
		goto fail;

	wpa_hexdump_key(MSG_DEBUG, "DPP: ECDH shared secret (K.x)",
			Kx, Kx_len);

	/* z = HKDF(<>, info | M.x | N.x | code, K.x) */
	res = dpp_pkex_derive_z(pkex->v2 ? NULL : pkex->own_mac,
				pkex->v2 ? NULL : pkex->peer_mac,
				DPP_VERSION, pkex->peer_version,
				pkex->Mx, curve->prime_len,
				attr_key /* N.x */, attr_key_len / 2,
				pkex->code, pkex->code_len, Kx, Kx_len,
				pkex->z, curve->hash_len);
	os_memset(Kx, 0, Kx_len);
	if (res < 0)
		goto fail;

	msg = dpp_pkex_build_commit_reveal_req(pkex, A_pub, u);
	if (!msg)
		goto fail;

out:
	wpabuf_free(A_pub);
	wpabuf_free(X_pub);
	wpabuf_free(Y_pub);
	os_free(x_coord);
	os_free(y_coord);
	crypto_ec_point_deinit(Qr, 1);
	crypto_ec_point_deinit(Y, 1);
	crypto_ec_point_deinit(N, 1);
	crypto_ec_deinit(ec);
	return msg;
fail:
	wpa_printf(MSG_DEBUG, "DPP: PKEX Exchange Response processing failed");
	goto out;
}


static struct wpabuf *
dpp_pkex_build_commit_reveal_resp(struct dpp_pkex *pkex,
				  const struct wpabuf *B_pub, const u8 *v)
{
	const struct dpp_curve_params *curve = pkex->own_bi->curve;
	struct wpabuf *msg = NULL;
	const u8 *addr[2];
	size_t len[2];
	u8 octet;
	u8 *wrapped;
	struct wpabuf *clear = NULL;
	size_t clear_len, attr_len;

	/* {B, v [bootstrapping info]}z */
	clear_len = 4 + 2 * curve->prime_len + 4 + curve->hash_len;
	clear = wpabuf_alloc(clear_len);
	attr_len = 4 + clear_len + AES_BLOCK_SIZE;
#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_AFTER_WRAPPED_DATA_PKEX_CR_RESP)
		attr_len += 5;
#endif /* CONFIG_TESTING_OPTIONS */
	msg = dpp_alloc_msg(DPP_PA_PKEX_COMMIT_REVEAL_RESP, attr_len);
	if (!clear || !msg)
		goto fail;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_NO_BOOTSTRAP_KEY_PKEX_CR_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Bootstrap Key");
		goto skip_bootstrap_key;
	}
	if (dpp_test == DPP_TEST_INVALID_BOOTSTRAP_KEY_PKEX_CR_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - invalid Bootstrap Key");
		wpabuf_put_le16(clear, DPP_ATTR_BOOTSTRAP_KEY);
		wpabuf_put_le16(clear, 2 * curve->prime_len);
		if (dpp_test_gen_invalid_key(clear, curve) < 0)
			goto fail;
		goto skip_bootstrap_key;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* B in Bootstrap Key attribute */
	wpabuf_put_le16(clear, DPP_ATTR_BOOTSTRAP_KEY);
	wpabuf_put_le16(clear, wpabuf_len(B_pub));
	wpabuf_put_buf(clear, B_pub);

#ifdef CONFIG_TESTING_OPTIONS
skip_bootstrap_key:
	if (dpp_test == DPP_TEST_NO_R_AUTH_TAG_PKEX_CR_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no R-Auth tag");
		goto skip_r_auth_tag;
	}
	if (dpp_test == DPP_TEST_R_AUTH_TAG_MISMATCH_PKEX_CR_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - R-Auth tag mismatch");
		wpabuf_put_le16(clear, DPP_ATTR_R_AUTH_TAG);
		wpabuf_put_le16(clear, curve->hash_len);
		wpabuf_put_data(clear, v, curve->hash_len - 1);
		wpabuf_put_u8(clear, v[curve->hash_len - 1] ^ 0x01);
		goto skip_r_auth_tag;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	/* v in R-Auth tag attribute */
	wpabuf_put_le16(clear, DPP_ATTR_R_AUTH_TAG);
	wpabuf_put_le16(clear, curve->hash_len);
	wpabuf_put_data(clear, v, curve->hash_len);

#ifdef CONFIG_TESTING_OPTIONS
skip_r_auth_tag:
	if (dpp_test == DPP_TEST_NO_WRAPPED_DATA_PKEX_CR_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - no Wrapped Data");
		goto skip_wrapped_data;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	addr[0] = wpabuf_head_u8(msg) + 2;
	len[0] = DPP_HDR_LEN;
	octet = 1;
	addr[1] = &octet;
	len[1] = sizeof(octet);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);

	wpabuf_put_le16(msg, DPP_ATTR_WRAPPED_DATA);
	wpabuf_put_le16(msg, wpabuf_len(clear) + AES_BLOCK_SIZE);
	wrapped = wpabuf_put(msg, wpabuf_len(clear) + AES_BLOCK_SIZE);

	wpa_hexdump_buf(MSG_DEBUG, "DPP: AES-SIV cleartext", clear);
	if (aes_siv_encrypt(pkex->z, curve->hash_len,
			    wpabuf_head(clear), wpabuf_len(clear),
			    2, addr, len, wrapped) < 0)
		goto fail;
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped, wpabuf_len(clear) + AES_BLOCK_SIZE);

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_AFTER_WRAPPED_DATA_PKEX_CR_RESP) {
		wpa_printf(MSG_INFO, "DPP: TESTING - attr after Wrapped Data");
		dpp_build_attr_status(msg, DPP_STATUS_OK);
	}
skip_wrapped_data:
#endif /* CONFIG_TESTING_OPTIONS */

out:
	wpabuf_free(clear);
	return msg;

fail:
	wpabuf_free(msg);
	msg = NULL;
	goto out;
}


struct wpabuf * dpp_pkex_rx_commit_reveal_req(struct dpp_pkex *pkex,
					      const u8 *hdr,
					      const u8 *buf, size_t buflen)
{
	const struct dpp_curve_params *curve = pkex->own_bi->curve;
	size_t Jx_len, Lx_len;
	u8 Jx[DPP_MAX_SHARED_SECRET_LEN];
	u8 Lx[DPP_MAX_SHARED_SECRET_LEN];
	const u8 *wrapped_data, *b_key, *peer_u;
	u16 wrapped_data_len, b_key_len, peer_u_len = 0;
	const u8 *addr[4];
	size_t len[4];
	size_t num_elem;
	u8 octet;
	u8 *unwrapped = NULL;
	size_t unwrapped_len = 0;
	struct wpabuf *msg = NULL, *A_pub = NULL, *X_pub = NULL, *Y_pub = NULL;
	struct wpabuf *B_pub = NULL;
	u8 u[DPP_MAX_HASH_LEN], v[DPP_MAX_HASH_LEN];

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_STOP_AT_PKEX_CR_REQ) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - stop at PKEX CR Request");
		pkex->failed = 1;
		return NULL;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (!pkex->exchange_done || pkex->failed ||
	    pkex->t >= PKEX_COUNTER_T_LIMIT || pkex->initiator)
		goto fail;

	wrapped_data = dpp_get_attr(buf, buflen, DPP_ATTR_WRAPPED_DATA,
				    &wrapped_data_len);
	if (!wrapped_data || wrapped_data_len < AES_BLOCK_SIZE) {
		dpp_pkex_fail(pkex,
			      "Missing or invalid required Wrapped Data attribute");
		goto fail;
	}

	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped_data, wrapped_data_len);
	unwrapped_len = wrapped_data_len - AES_BLOCK_SIZE;
	unwrapped = os_malloc(unwrapped_len);
	if (!unwrapped)
		goto fail;

	addr[0] = hdr;
	len[0] = DPP_HDR_LEN;
	octet = 0;
	addr[1] = &octet;
	len[1] = sizeof(octet);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);

	if (aes_siv_decrypt(pkex->z, curve->hash_len,
			    wrapped_data, wrapped_data_len,
			    2, addr, len, unwrapped) < 0) {
		dpp_pkex_fail(pkex,
			      "AES-SIV decryption failed - possible PKEX code mismatch");
		pkex->failed = 1;
		pkex->t++;
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV cleartext",
		    unwrapped, unwrapped_len);

	if (dpp_check_attrs(unwrapped, unwrapped_len) < 0) {
		dpp_pkex_fail(pkex, "Invalid attribute in unwrapped data");
		goto fail;
	}

	b_key = dpp_get_attr(unwrapped, unwrapped_len, DPP_ATTR_BOOTSTRAP_KEY,
			     &b_key_len);
	if (!b_key || b_key_len != 2 * curve->prime_len) {
		dpp_pkex_fail(pkex, "No valid peer bootstrapping key found");
		goto fail;
	}
	pkex->peer_bootstrap_key = dpp_set_pubkey_point(pkex->x, b_key,
							b_key_len);
	if (!pkex->peer_bootstrap_key) {
		dpp_pkex_fail(pkex, "Peer bootstrapping key is invalid");
		goto fail;
	}
	dpp_debug_print_key("DPP: Peer bootstrap public key",
			    pkex->peer_bootstrap_key);

	/* ECDH: J' = y * A' */
	if (dpp_ecdh(pkex->y, pkex->peer_bootstrap_key, Jx, &Jx_len) < 0)
		goto fail;

	wpa_hexdump_key(MSG_DEBUG, "DPP: ECDH shared secret (J.x)",
			Jx, Jx_len);

	/* u' = HMAC(J'.x, [MAC-Initiator |] A'.x | Y.x | X'.x) */
	A_pub = crypto_ec_key_get_pubkey_point(pkex->peer_bootstrap_key, 0);
	Y_pub = crypto_ec_key_get_pubkey_point(pkex->y, 0);
	X_pub = crypto_ec_key_get_pubkey_point(pkex->x, 0);
	if (!A_pub || !Y_pub || !X_pub)
		goto fail;
	num_elem = 0;
	if (!pkex->v2) {
		addr[num_elem] = pkex->peer_mac;
		len[num_elem] = ETH_ALEN;
		num_elem++;
	}
	addr[num_elem] = wpabuf_head(A_pub);
	len[num_elem] = wpabuf_len(A_pub) / 2;
	num_elem++;
	addr[num_elem] = wpabuf_head(Y_pub);
	len[num_elem] = wpabuf_len(Y_pub) / 2;
	num_elem++;
	addr[num_elem] = wpabuf_head(X_pub);
	len[num_elem] = wpabuf_len(X_pub) / 2;
	num_elem++;
	if (dpp_hmac_vector(curve->hash_len, Jx, Jx_len, num_elem, addr, len, u)
	    < 0)
		goto fail;

	peer_u = dpp_get_attr(unwrapped, unwrapped_len, DPP_ATTR_I_AUTH_TAG,
			      &peer_u_len);
	if (!peer_u || peer_u_len != curve->hash_len ||
	    os_memcmp(peer_u, u, curve->hash_len) != 0) {
		dpp_pkex_fail(pkex, "No valid u (I-Auth tag) found");
		wpa_hexdump(MSG_DEBUG, "DPP: Calculated u'",
			    u, curve->hash_len);
		wpa_hexdump(MSG_DEBUG, "DPP: Received u", peer_u, peer_u_len);
		pkex->t++;
		goto fail;
	}
	wpa_printf(MSG_DEBUG, "DPP: Valid u (I-Auth tag) received");

	/* ECDH: L = b * X' */
	if (dpp_ecdh(pkex->own_bi->pubkey, pkex->x, Lx, &Lx_len) < 0)
		goto fail;

	wpa_hexdump_key(MSG_DEBUG, "DPP: ECDH shared secret (L.x)",
			Lx, Lx_len);

	/* v = HMAC(L.x, [MAC-Responder |] B.x | X'.x | Y.x) */
	B_pub = crypto_ec_key_get_pubkey_point(pkex->own_bi->pubkey, 0);
	if (!B_pub)
		goto fail;
	num_elem = 0;
	if (!pkex->v2) {
		addr[num_elem] = pkex->own_mac;
		len[num_elem] = ETH_ALEN;
		num_elem++;
	}
	addr[num_elem] = wpabuf_head(B_pub);
	len[num_elem] = wpabuf_len(B_pub) / 2;
	num_elem++;
	addr[num_elem] = wpabuf_head(X_pub);
	len[num_elem] = wpabuf_len(X_pub) / 2;
	num_elem++;
	addr[num_elem] = wpabuf_head(Y_pub);
	len[num_elem] = wpabuf_len(Y_pub) / 2;
	num_elem++;
	if (dpp_hmac_vector(curve->hash_len, Lx, Lx_len, num_elem, addr, len, v)
	    < 0)
		goto fail;
	wpa_hexdump(MSG_DEBUG, "DPP: v", v, curve->hash_len);

	msg = dpp_pkex_build_commit_reveal_resp(pkex, B_pub, v);
	if (!msg)
		goto fail;

out:
	os_free(unwrapped);
	wpabuf_free(A_pub);
	wpabuf_free(B_pub);
	wpabuf_free(X_pub);
	wpabuf_free(Y_pub);
	return msg;
fail:
	wpa_printf(MSG_DEBUG,
		   "DPP: PKEX Commit-Reveal Request processing failed");
	goto out;
}


int dpp_pkex_rx_commit_reveal_resp(struct dpp_pkex *pkex, const u8 *hdr,
				   const u8 *buf, size_t buflen)
{
	const struct dpp_curve_params *curve = pkex->own_bi->curve;
	const u8 *wrapped_data, *b_key, *peer_v;
	u16 wrapped_data_len, b_key_len, peer_v_len = 0;
	const u8 *addr[4];
	size_t len[4];
	size_t num_elem;
	u8 octet;
	u8 *unwrapped = NULL;
	size_t unwrapped_len = 0;
	int ret = -1;
	u8 v[DPP_MAX_HASH_LEN];
	size_t Lx_len;
	u8 Lx[DPP_MAX_SHARED_SECRET_LEN];
	struct wpabuf *B_pub = NULL, *X_pub = NULL, *Y_pub = NULL;

#ifdef CONFIG_TESTING_OPTIONS
	if (dpp_test == DPP_TEST_STOP_AT_PKEX_CR_RESP) {
		wpa_printf(MSG_INFO,
			   "DPP: TESTING - stop at PKEX CR Response");
		pkex->failed = 1;
		goto fail;
	}
#endif /* CONFIG_TESTING_OPTIONS */

	if (!pkex->exchange_done || pkex->failed ||
	    pkex->t >= PKEX_COUNTER_T_LIMIT || !pkex->initiator)
		goto fail;

	wrapped_data = dpp_get_attr(buf, buflen, DPP_ATTR_WRAPPED_DATA,
				    &wrapped_data_len);
	if (!wrapped_data || wrapped_data_len < AES_BLOCK_SIZE) {
		dpp_pkex_fail(pkex,
			      "Missing or invalid required Wrapped Data attribute");
		goto fail;
	}

	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV ciphertext",
		    wrapped_data, wrapped_data_len);
	unwrapped_len = wrapped_data_len - AES_BLOCK_SIZE;
	unwrapped = os_malloc(unwrapped_len);
	if (!unwrapped)
		goto fail;

	addr[0] = hdr;
	len[0] = DPP_HDR_LEN;
	octet = 1;
	addr[1] = &octet;
	len[1] = sizeof(octet);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[0]", addr[0], len[0]);
	wpa_hexdump(MSG_DEBUG, "DDP: AES-SIV AD[1]", addr[1], len[1]);

	if (aes_siv_decrypt(pkex->z, curve->hash_len,
			    wrapped_data, wrapped_data_len,
			    2, addr, len, unwrapped) < 0) {
		dpp_pkex_fail(pkex,
			      "AES-SIV decryption failed - possible PKEX code mismatch");
		pkex->t++;
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: AES-SIV cleartext",
		    unwrapped, unwrapped_len);

	if (dpp_check_attrs(unwrapped, unwrapped_len) < 0) {
		dpp_pkex_fail(pkex, "Invalid attribute in unwrapped data");
		goto fail;
	}

	b_key = dpp_get_attr(unwrapped, unwrapped_len, DPP_ATTR_BOOTSTRAP_KEY,
			     &b_key_len);
	if (!b_key || b_key_len != 2 * curve->prime_len) {
		dpp_pkex_fail(pkex, "No valid peer bootstrapping key found");
		goto fail;
	}
	pkex->peer_bootstrap_key = dpp_set_pubkey_point(pkex->x, b_key,
							b_key_len);
	if (!pkex->peer_bootstrap_key) {
		dpp_pkex_fail(pkex, "Peer bootstrapping key is invalid");
		goto fail;
	}
	dpp_debug_print_key("DPP: Peer bootstrap public key",
			    pkex->peer_bootstrap_key);

	/* ECDH: L' = x * B' */
	if (dpp_ecdh(pkex->x, pkex->peer_bootstrap_key, Lx, &Lx_len) < 0)
		goto fail;

	wpa_hexdump_key(MSG_DEBUG, "DPP: ECDH shared secret (L.x)",
			Lx, Lx_len);

	/* v' = HMAC(L.x, [MAC-Responder |] B'.x | X.x | Y'.x) */
	B_pub = crypto_ec_key_get_pubkey_point(pkex->peer_bootstrap_key, 0);
	X_pub = crypto_ec_key_get_pubkey_point(pkex->x, 0);
	Y_pub = crypto_ec_key_get_pubkey_point(pkex->y, 0);
	if (!B_pub || !X_pub || !Y_pub)
		goto fail;
	num_elem = 0;
	if (!pkex->v2) {
		addr[num_elem] = pkex->peer_mac;
		len[num_elem] = ETH_ALEN;
		num_elem++;
	}
	addr[num_elem] = wpabuf_head(B_pub);
	len[num_elem] = wpabuf_len(B_pub) / 2;
	num_elem++;
	addr[num_elem] = wpabuf_head(X_pub);
	len[num_elem] = wpabuf_len(X_pub) / 2;
	num_elem++;
	addr[num_elem] = wpabuf_head(Y_pub);
	len[num_elem] = wpabuf_len(Y_pub) / 2;
	num_elem++;
	if (dpp_hmac_vector(curve->hash_len, Lx, Lx_len, num_elem, addr, len, v)
	    < 0)
		goto fail;

	peer_v = dpp_get_attr(unwrapped, unwrapped_len, DPP_ATTR_R_AUTH_TAG,
			      &peer_v_len);
	if (!peer_v || peer_v_len != curve->hash_len ||
	    os_memcmp(peer_v, v, curve->hash_len) != 0) {
		dpp_pkex_fail(pkex, "No valid v (R-Auth tag) found");
		wpa_hexdump(MSG_DEBUG, "DPP: Calculated v'",
			    v, curve->hash_len);
		wpa_hexdump(MSG_DEBUG, "DPP: Received v", peer_v, peer_v_len);
		pkex->t++;
		goto fail;
	}
	wpa_printf(MSG_DEBUG, "DPP: Valid v (R-Auth tag) received");

	ret = 0;
out:
	wpabuf_free(B_pub);
	wpabuf_free(X_pub);
	wpabuf_free(Y_pub);
	os_free(unwrapped);
	return ret;
fail:
	goto out;
}


struct dpp_bootstrap_info *
dpp_pkex_finish(struct dpp_global *dpp, struct dpp_pkex *pkex, const u8 *peer,
		unsigned int freq)
{
	struct dpp_bootstrap_info *bi;

	bi = os_zalloc(sizeof(*bi));
	if (!bi)
		return NULL;
	bi->id = dpp_next_id(dpp);
	bi->type = DPP_BOOTSTRAP_PKEX;
	if (peer)
		os_memcpy(bi->mac_addr, peer, ETH_ALEN);
	if (freq) {
		bi->num_freq = 1;
		bi->freq[0] = freq;
	}
	bi->curve = pkex->own_bi->curve;
	bi->pubkey = pkex->peer_bootstrap_key;
	pkex->peer_bootstrap_key = NULL;
	if (dpp_bootstrap_key_hash(bi) < 0) {
		dpp_bootstrap_info_free(bi);
		return NULL;
	}
	os_memcpy(pkex->own_bi->peer_pubkey_hash, bi->pubkey_hash,
		  SHA256_MAC_LEN);
	dpp_pkex_free(pkex);
	dl_list_add(&dpp->bootstrap, &bi->list);
	return bi;
}


void dpp_pkex_free(struct dpp_pkex *pkex)
{
	if (!pkex)
		return;

	os_free(pkex->identifier);
	os_free(pkex->code);
	crypto_ec_key_deinit(pkex->x);
	crypto_ec_key_deinit(pkex->y);
	crypto_ec_key_deinit(pkex->peer_bootstrap_key);
	wpabuf_free(pkex->exchange_req);
	wpabuf_free(pkex->exchange_resp);
	wpabuf_free(pkex->enc_key);
	os_free(pkex);
}
