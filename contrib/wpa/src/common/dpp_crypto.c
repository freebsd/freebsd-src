/*
 * DPP crypto functionality
 * Copyright (c) 2017, Qualcomm Atheros, Inc.
 * Copyright (c) 2018-2020, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/base64.h"
#include "utils/json.h"
#include "common/ieee802_11_defs.h"
#include "crypto/crypto.h"
#include "crypto/random.h"
#include "crypto/sha384.h"
#include "crypto/sha512.h"
#include "tls/asn1.h"
#include "dpp.h"
#include "dpp_i.h"


static const struct dpp_curve_params dpp_curves[] = {
	/* The mandatory to support and the default NIST P-256 curve needs to
	 * be the first entry on this list. */
	{ "prime256v1", 32, 32, 16, 32, "P-256", 19, "ES256" },
	{ "secp384r1", 48, 48, 24, 48, "P-384", 20, "ES384" },
	{ "secp521r1", 64, 64, 32, 66, "P-521", 21, "ES512" },
	{ "brainpoolP256r1", 32, 32, 16, 32, "BP-256", 28, "BS256" },
	{ "brainpoolP384r1", 48, 48, 24, 48, "BP-384", 29, "BS384" },
	{ "brainpoolP512r1", 64, 64, 32, 64, "BP-512", 30, "BS512" },
	{ NULL, 0, 0, 0, 0, NULL, 0, NULL }
};


const struct dpp_curve_params * dpp_get_curve_name(const char *name)
{
	int i;

	if (!name)
		return &dpp_curves[0];

	for (i = 0; dpp_curves[i].name; i++) {
		if (os_strcmp(name, dpp_curves[i].name) == 0 ||
		    (dpp_curves[i].jwk_crv &&
		     os_strcmp(name, dpp_curves[i].jwk_crv) == 0))
			return &dpp_curves[i];
	}
	return NULL;
}


const struct dpp_curve_params * dpp_get_curve_jwk_crv(const char *name)
{
	int i;

	for (i = 0; dpp_curves[i].name; i++) {
		if (dpp_curves[i].jwk_crv &&
		    os_strcmp(name, dpp_curves[i].jwk_crv) == 0)
			return &dpp_curves[i];
	}
	return NULL;
}


const struct dpp_curve_params * dpp_get_curve_ike_group(u16 group)
{
	int i;

	for (i = 0; dpp_curves[i].name; i++) {
		if (dpp_curves[i].ike_group == group)
			return &dpp_curves[i];
	}
	return NULL;
}


void dpp_debug_print_key(const char *title, struct crypto_ec_key *key)
{
	struct wpabuf *der = NULL;

	crypto_ec_key_debug_print(key, title);

	der = crypto_ec_key_get_ecprivate_key(key, true);
	if (der) {
		wpa_hexdump_buf_key(MSG_DEBUG, "DPP: ECPrivateKey", der);
	} else {
		der = crypto_ec_key_get_subject_public_key(key);
		if (der)
			wpa_hexdump_buf_key(MSG_DEBUG, "DPP: EC_PUBKEY", der);
	}

	wpabuf_clear_free(der);
}


static int dpp_hash_vector(const struct dpp_curve_params *curve,
			   size_t num_elem, const u8 *addr[], const size_t *len,
			   u8 *mac)
{
	if (curve->hash_len == 32)
		return sha256_vector(num_elem, addr, len, mac);
	if (curve->hash_len == 48)
		return sha384_vector(num_elem, addr, len, mac);
	if (curve->hash_len == 64)
		return sha512_vector(num_elem, addr, len, mac);
	return -1;
}


int dpp_hkdf_expand(size_t hash_len, const u8 *secret, size_t secret_len,
		    const char *label, u8 *out, size_t outlen)
{
	if (hash_len == 32)
		return hmac_sha256_kdf(secret, secret_len, NULL,
				       (const u8 *) label, os_strlen(label),
				       out, outlen);
	if (hash_len == 48)
		return hmac_sha384_kdf(secret, secret_len, NULL,
				       (const u8 *) label, os_strlen(label),
				       out, outlen);
	if (hash_len == 64)
		return hmac_sha512_kdf(secret, secret_len, NULL,
				       (const u8 *) label, os_strlen(label),
				       out, outlen);
	return -1;
}


int dpp_hmac_vector(size_t hash_len, const u8 *key, size_t key_len,
		    size_t num_elem, const u8 *addr[], const size_t *len,
		    u8 *mac)
{
	if (hash_len == 32)
		return hmac_sha256_vector(key, key_len, num_elem, addr, len,
					  mac);
	if (hash_len == 48)
		return hmac_sha384_vector(key, key_len, num_elem, addr, len,
					  mac);
	if (hash_len == 64)
		return hmac_sha512_vector(key, key_len, num_elem, addr, len,
					  mac);
	return -1;
}


static int dpp_hmac(size_t hash_len, const u8 *key, size_t key_len,
		    const u8 *data, size_t data_len, u8 *mac)
{
	if (hash_len == 32)
		return hmac_sha256(key, key_len, data, data_len, mac);
	if (hash_len == 48)
		return hmac_sha384(key, key_len, data, data_len, mac);
	if (hash_len == 64)
		return hmac_sha512(key, key_len, data, data_len, mac);
	return -1;
}


#ifdef CONFIG_DPP2

static int dpp_pbkdf2_f(size_t hash_len,
			const u8 *password, size_t password_len,
			const u8 *salt, size_t salt_len,
			unsigned int iterations, unsigned int count, u8 *digest)
{
	unsigned char tmp[DPP_MAX_HASH_LEN], tmp2[DPP_MAX_HASH_LEN];
	unsigned int i;
	size_t j;
	u8 count_buf[4];
	const u8 *addr[2];
	size_t len[2];

	addr[0] = salt;
	len[0] = salt_len;
	addr[1] = count_buf;
	len[1] = 4;

	/* F(P, S, c, i) = U1 xor U2 xor ... Uc
	 * U1 = PRF(P, S || i)
	 * U2 = PRF(P, U1)
	 * Uc = PRF(P, Uc-1)
	 */

	WPA_PUT_BE32(count_buf, count);
	if (dpp_hmac_vector(hash_len, password, password_len, 2, addr, len,
			    tmp))
		return -1;
	os_memcpy(digest, tmp, hash_len);

	for (i = 1; i < iterations; i++) {
		if (dpp_hmac(hash_len, password, password_len, tmp, hash_len,
			     tmp2))
			return -1;
		os_memcpy(tmp, tmp2, hash_len);
		for (j = 0; j < hash_len; j++)
			digest[j] ^= tmp2[j];
	}

	return 0;
}


int dpp_pbkdf2(size_t hash_len, const u8 *password, size_t password_len,
	       const u8 *salt, size_t salt_len, unsigned int iterations,
	       u8 *buf, size_t buflen)
{
	unsigned int count = 0;
	unsigned char *pos = buf;
	size_t left = buflen, plen;
	unsigned char digest[DPP_MAX_HASH_LEN];

	while (left > 0) {
		count++;
		if (dpp_pbkdf2_f(hash_len, password, password_len,
				 salt, salt_len, iterations, count, digest))
			return -1;
		plen = left > hash_len ? hash_len : left;
		os_memcpy(pos, digest, plen);
		pos += plen;
		left -= plen;
	}

	return 0;
}

#endif /* CONFIG_DPP2 */


struct crypto_ec_key * dpp_set_pubkey_point(struct crypto_ec_key *group_key,
					    const u8 *buf, size_t len)
{
	int ike_group = crypto_ec_key_group(group_key);

	if (len & 1)
		return NULL;

	if (ike_group < 0) {
		wpa_printf(MSG_ERROR, "DPP: Could not get EC group");
		return NULL;
	}

	return crypto_ec_key_set_pub(ike_group, buf, buf + len / 2, len / 2);
}


struct crypto_ec_key * dpp_gen_keypair(const struct dpp_curve_params *curve)
{
	struct crypto_ec_key *key;

	wpa_printf(MSG_DEBUG, "DPP: Generating a keypair");

	key = crypto_ec_key_gen(curve->ike_group);
	if (key && wpa_debug_show_keys)
	    dpp_debug_print_key("Own generated key", key);

	return key;
}


struct crypto_ec_key * dpp_set_keypair(const struct dpp_curve_params **curve,
				       const u8 *privkey, size_t privkey_len)
{
	struct crypto_ec_key *key;
	int group;

	key = crypto_ec_key_parse_priv(privkey, privkey_len);
	if (!key) {
		wpa_printf(MSG_INFO, "DPP: Failed to parse private key");
		return NULL;
	}

	group = crypto_ec_key_group(key);
	if (group < 0) {
		crypto_ec_key_deinit(key);
		return NULL;
	}

	*curve = dpp_get_curve_ike_group(group);
	if (!*curve) {
		wpa_printf(MSG_INFO,
			   "DPP: Unsupported curve (group=%d) in pre-assigned key",
			   group);
		crypto_ec_key_deinit(key);
		return NULL;
	}

	return key;
}


int dpp_bootstrap_key_hash(struct dpp_bootstrap_info *bi)
{
	struct wpabuf *der;
	int res;

	der = crypto_ec_key_get_subject_public_key(bi->pubkey);
	if (!der)
		return -1;
	wpa_hexdump_buf(MSG_DEBUG, "DPP: Compressed public key (DER)",
			der);
	res = dpp_bi_pubkey_hash(bi, wpabuf_head(der), wpabuf_len(der));
	if (res < 0)
		wpa_printf(MSG_DEBUG, "DPP: Failed to hash public key");
	wpabuf_free(der);
	return res;
}


int dpp_keygen(struct dpp_bootstrap_info *bi, const char *curve,
	       const u8 *privkey, size_t privkey_len)
{
	char *base64 = NULL;
	char *pos, *end;
	size_t len;
	struct wpabuf *der = NULL;

	bi->curve = dpp_get_curve_name(curve);
	if (!bi->curve) {
		wpa_printf(MSG_INFO, "DPP: Unsupported curve: %s", curve);
		return -1;
	}

	if (privkey)
		bi->pubkey = dpp_set_keypair(&bi->curve, privkey, privkey_len);
	else
		bi->pubkey = dpp_gen_keypair(bi->curve);
	if (!bi->pubkey)
		goto fail;
	bi->own = 1;

	der = crypto_ec_key_get_subject_public_key(bi->pubkey);
	if (!der)
		goto fail;
	wpa_hexdump_buf(MSG_DEBUG, "DPP: Compressed public key (DER)",
			der);

	if (dpp_bi_pubkey_hash(bi, wpabuf_head(der), wpabuf_len(der)) < 0) {
		wpa_printf(MSG_DEBUG, "DPP: Failed to hash public key");
		goto fail;
	}

	base64 = base64_encode(wpabuf_head(der), wpabuf_len(der), &len);
	wpabuf_free(der);
	der = NULL;
	if (!base64)
		goto fail;
	pos = base64;
	end = pos + len;
	for (;;) {
		pos = os_strchr(pos, '\n');
		if (!pos)
			break;
		os_memmove(pos, pos + 1, end - pos);
	}
	os_free(bi->pk);
	bi->pk = base64;
	return 0;
fail:
	os_free(base64);
	wpabuf_free(der);
	return -1;
}


int dpp_derive_k1(const u8 *Mx, size_t Mx_len, u8 *k1, unsigned int hash_len)
{
	u8 salt[DPP_MAX_HASH_LEN], prk[DPP_MAX_HASH_LEN];
	const char *info = "first intermediate key";
	int res;

	/* k1 = HKDF(<>, "first intermediate key", M.x) */

	/* HKDF-Extract(<>, M.x) */
	os_memset(salt, 0, hash_len);
	if (dpp_hmac(hash_len, salt, hash_len, Mx, Mx_len, prk) < 0)
		return -1;
	wpa_hexdump_key(MSG_DEBUG, "DPP: PRK = HKDF-Extract(<>, IKM=M.x)",
			prk, hash_len);

	/* HKDF-Expand(PRK, info, L) */
	res = dpp_hkdf_expand(hash_len, prk, hash_len, info, k1, hash_len);
	os_memset(prk, 0, hash_len);
	if (res < 0)
		return -1;

	wpa_hexdump_key(MSG_DEBUG, "DPP: k1 = HKDF-Expand(PRK, info, L)",
			k1, hash_len);
	return 0;
}


int dpp_derive_k2(const u8 *Nx, size_t Nx_len, u8 *k2, unsigned int hash_len)
{
	u8 salt[DPP_MAX_HASH_LEN], prk[DPP_MAX_HASH_LEN];
	const char *info = "second intermediate key";
	int res;

	/* k2 = HKDF(<>, "second intermediate key", N.x) */

	/* HKDF-Extract(<>, N.x) */
	os_memset(salt, 0, hash_len);
	res = dpp_hmac(hash_len, salt, hash_len, Nx, Nx_len, prk);
	if (res < 0)
		return -1;
	wpa_hexdump_key(MSG_DEBUG, "DPP: PRK = HKDF-Extract(<>, IKM=N.x)",
			prk, hash_len);

	/* HKDF-Expand(PRK, info, L) */
	res = dpp_hkdf_expand(hash_len, prk, hash_len, info, k2, hash_len);
	os_memset(prk, 0, hash_len);
	if (res < 0)
		return -1;

	wpa_hexdump_key(MSG_DEBUG, "DPP: k2 = HKDF-Expand(PRK, info, L)",
			k2, hash_len);
	return 0;
}


int dpp_derive_bk_ke(struct dpp_authentication *auth)
{
	unsigned int hash_len = auth->curve->hash_len;
	size_t nonce_len = auth->curve->nonce_len;
	u8 nonces[2 * DPP_MAX_NONCE_LEN];
	const char *info_ke = "DPP Key";
	int res;
	const u8 *addr[3];
	size_t len[3];
	size_t num_elem = 0;

	if (!auth->Mx_len || !auth->Nx_len) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Mx/Nx not available - cannot derive ke");
		return -1;
	}

	/* bk = HKDF-Extract(I-nonce | R-nonce, M.x | N.x [| L.x]) */
	os_memcpy(nonces, auth->i_nonce, nonce_len);
	os_memcpy(&nonces[nonce_len], auth->r_nonce, nonce_len);
	addr[num_elem] = auth->Mx;
	len[num_elem] = auth->Mx_len;
	num_elem++;
	addr[num_elem] = auth->Nx;
	len[num_elem] = auth->Nx_len;
	num_elem++;
	if (auth->peer_bi && auth->own_bi) {
		if (!auth->Lx_len) {
			wpa_printf(MSG_DEBUG,
				   "DPP: Lx not available - cannot derive ke");
			return -1;
		}
		addr[num_elem] = auth->Lx;
		len[num_elem] = auth->secret_len;
		num_elem++;
	}
	res = dpp_hmac_vector(hash_len, nonces, 2 * nonce_len,
			      num_elem, addr, len, auth->bk);
	if (res < 0)
		return -1;
	wpa_hexdump_key(MSG_DEBUG,
			"DPP: bk = HKDF-Extract(I-nonce | R-nonce, M.x | N.x [| L.x])",
			auth->bk, hash_len);

	/* ke = HKDF-Expand(bk, "DPP Key", length) */
	res = dpp_hkdf_expand(hash_len, auth->bk, hash_len, info_ke, auth->ke,
			      hash_len);
	if (res < 0)
		return -1;

	wpa_hexdump_key(MSG_DEBUG,
			"DPP: ke = HKDF-Expand(bk, \"DPP Key\", length)",
			auth->ke, hash_len);

	return 0;
}


int dpp_ecdh(struct crypto_ec_key *own, struct crypto_ec_key *peer,
	     u8 *secret, size_t *secret_len)
{
	struct crypto_ecdh *ecdh;
	struct wpabuf *peer_pub, *secret_buf = NULL;
	int ret = -1;

	*secret_len = 0;

	ecdh = crypto_ecdh_init2(crypto_ec_key_group(own), own);
	if (!ecdh) {
		wpa_printf(MSG_ERROR, "DPP: crypto_ecdh_init2() failed");
		return -1;
	}

	peer_pub = crypto_ec_key_get_pubkey_point(peer, 0);
	if (!peer_pub) {
		wpa_printf(MSG_ERROR,
			   "DPP: crypto_ec_key_get_pubkey_point() failed");
		goto fail;
	}

	secret_buf = crypto_ecdh_set_peerkey(ecdh, 1, wpabuf_head(peer_pub),
					     wpabuf_len(peer_pub));
	if (!secret_buf) {
		wpa_printf(MSG_ERROR, "DPP: crypto_ecdh_set_peerkey() failed");
		goto fail;
	}

	if (wpabuf_len(secret_buf) > DPP_MAX_SHARED_SECRET_LEN) {
		wpa_printf(MSG_ERROR, "DPP: ECDH secret longer than expected");
		goto fail;
	}

	*secret_len = wpabuf_len(secret_buf);
	os_memcpy(secret, wpabuf_head(secret_buf), wpabuf_len(secret_buf));
	ret = 0;

fail:
	wpabuf_clear_free(secret_buf);
	wpabuf_free(peer_pub);
	crypto_ecdh_deinit(ecdh);
	return ret;
}


int dpp_bi_pubkey_hash(struct dpp_bootstrap_info *bi,
		       const u8 *data, size_t data_len)
{
	const u8 *addr[2];
	size_t len[2];

	addr[0] = data;
	len[0] = data_len;
	if (sha256_vector(1, addr, len, bi->pubkey_hash) < 0)
		return -1;
	wpa_hexdump(MSG_DEBUG, "DPP: Public key hash",
		    bi->pubkey_hash, SHA256_MAC_LEN);

	addr[0] = (const u8 *) "chirp";
	len[0] = 5;
	addr[1] = data;
	len[1] = data_len;
	if (sha256_vector(2, addr, len, bi->pubkey_hash_chirp) < 0)
		return -1;
	wpa_hexdump(MSG_DEBUG, "DPP: Public key hash (chirp)",
		    bi->pubkey_hash_chirp, SHA256_MAC_LEN);

	return 0;
}


int dpp_get_subject_public_key(struct dpp_bootstrap_info *bi,
			       const u8 *data, size_t data_len)
{
	struct crypto_ec_key *key;

	if (dpp_bi_pubkey_hash(bi, data, data_len) < 0) {
		wpa_printf(MSG_DEBUG, "DPP: Failed to hash public key");
		return -1;
	}

	key = crypto_ec_key_parse_pub(data, data_len);
	if (!key) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Could not parse URI public-key SubjectPublicKeyInfo");
		return -1;
	}

	bi->curve = dpp_get_curve_ike_group(crypto_ec_key_group(key));
	if (!bi->curve) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unsupported SubjectPublicKeyInfo curve: group %d",
			   crypto_ec_key_group(key));
		goto fail;
	}

	bi->pubkey = key;
	return 0;
fail:
	crypto_ec_key_deinit(key);
	return -1;
}


static struct wpabuf *
dpp_parse_jws_prot_hdr(const struct dpp_curve_params *curve,
		       const u8 *prot_hdr, u16 prot_hdr_len,
		       int *hash_func)
{
	struct json_token *root, *token;
	struct wpabuf *kid = NULL;

	root = json_parse((const char *) prot_hdr, prot_hdr_len);
	if (!root) {
		wpa_printf(MSG_DEBUG,
			   "DPP: JSON parsing failed for JWS Protected Header");
		goto fail;
	}

	if (root->type != JSON_OBJECT) {
		wpa_printf(MSG_DEBUG,
			   "DPP: JWS Protected Header root is not an object");
		goto fail;
	}

	token = json_get_member(root, "typ");
	if (!token || token->type != JSON_STRING) {
		wpa_printf(MSG_DEBUG, "DPP: No typ string value found");
		goto fail;
	}
	wpa_printf(MSG_DEBUG, "DPP: JWS Protected Header typ=%s",
		   token->string);
	if (os_strcmp(token->string, "dppCon") != 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unsupported JWS Protected Header typ=%s",
			   token->string);
		goto fail;
	}

	token = json_get_member(root, "alg");
	if (!token || token->type != JSON_STRING) {
		wpa_printf(MSG_DEBUG, "DPP: No alg string value found");
		goto fail;
	}
	wpa_printf(MSG_DEBUG, "DPP: JWS Protected Header alg=%s",
		   token->string);
	if (os_strcmp(token->string, curve->jws_alg) != 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unexpected JWS Protected Header alg=%s (expected %s based on C-sign-key)",
			   token->string, curve->jws_alg);
		goto fail;
	}
	if (os_strcmp(token->string, "ES256") == 0 ||
	    os_strcmp(token->string, "BS256") == 0) {
		*hash_func = CRYPTO_HASH_ALG_SHA256;
	} else if (os_strcmp(token->string, "ES384") == 0 ||
		   os_strcmp(token->string, "BS384") == 0) {
		*hash_func = CRYPTO_HASH_ALG_SHA384;
	} else if (os_strcmp(token->string, "ES512") == 0 ||
		   os_strcmp(token->string, "BS512") == 0) {
		*hash_func = CRYPTO_HASH_ALG_SHA512;
	} else {
		*hash_func = -1;
		wpa_printf(MSG_DEBUG,
			   "DPP: Unsupported JWS Protected Header alg=%s",
			   token->string);
		goto fail;
	}

	kid = json_get_member_base64url(root, "kid");
	if (!kid) {
		wpa_printf(MSG_DEBUG, "DPP: No kid string value found");
		goto fail;
	}
	wpa_hexdump_buf(MSG_DEBUG, "DPP: JWS Protected Header kid (decoded)",
			kid);

fail:
	json_free(root);
	return kid;
}


static int dpp_check_pubkey_match(struct crypto_ec_key *pub,
				  struct wpabuf *r_hash)
{
	struct wpabuf *uncomp;
	int res;
	u8 hash[SHA256_MAC_LEN];
	const u8 *addr[1];
	size_t len[1];

	if (wpabuf_len(r_hash) != SHA256_MAC_LEN)
		return -1;
	uncomp = crypto_ec_key_get_pubkey_point(pub, 1);
	if (!uncomp)
		return -1;
	addr[0] = wpabuf_head(uncomp);
	len[0] = wpabuf_len(uncomp);
	wpa_hexdump(MSG_DEBUG, "DPP: Uncompressed public key",
		    addr[0], len[0]);
	res = sha256_vector(1, addr, len, hash);
	wpabuf_free(uncomp);
	if (res < 0)
		return -1;
	if (os_memcmp(hash, wpabuf_head(r_hash), SHA256_MAC_LEN) != 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Received hash value does not match calculated public key hash value");
		wpa_hexdump(MSG_DEBUG, "DPP: Calculated hash",
			    hash, SHA256_MAC_LEN);
		return -1;
	}
	return 0;
}


enum dpp_status_error
dpp_process_signed_connector(struct dpp_signed_connector_info *info,
			     struct crypto_ec_key *csign_pub,
			     const char *connector)
{
	enum dpp_status_error ret = 255;
	const char *pos, *end, *signed_start, *signed_end;
	struct wpabuf *kid = NULL;
	unsigned char *prot_hdr = NULL, *signature = NULL;
	size_t prot_hdr_len = 0, signature_len = 0, signed_len;
	int res, hash_func = -1;
	const struct dpp_curve_params *curve;
	u8 *hash = NULL;

	curve = dpp_get_curve_ike_group(crypto_ec_key_group(csign_pub));
	if (!curve)
		goto fail;
	wpa_printf(MSG_DEBUG, "DPP: C-sign-key group: %s", curve->jwk_crv);
	os_memset(info, 0, sizeof(*info));

	signed_start = pos = connector;
	end = os_strchr(pos, '.');
	if (!end) {
		wpa_printf(MSG_DEBUG, "DPP: Missing dot(1) in signedConnector");
		ret = DPP_STATUS_INVALID_CONNECTOR;
		goto fail;
	}
	prot_hdr = base64_url_decode(pos, end - pos, &prot_hdr_len);
	if (!prot_hdr) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Failed to base64url decode signedConnector JWS Protected Header");
		ret = DPP_STATUS_INVALID_CONNECTOR;
		goto fail;
	}
	wpa_hexdump_ascii(MSG_DEBUG,
			  "DPP: signedConnector - JWS Protected Header",
			  prot_hdr, prot_hdr_len);
	kid = dpp_parse_jws_prot_hdr(curve, prot_hdr, prot_hdr_len, &hash_func);
	if (!kid) {
		ret = DPP_STATUS_INVALID_CONNECTOR;
		goto fail;
	}
	if (wpabuf_len(kid) != SHA256_MAC_LEN) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unexpected signedConnector JWS Protected Header kid length: %u (expected %u)",
			   (unsigned int) wpabuf_len(kid), SHA256_MAC_LEN);
		ret = DPP_STATUS_INVALID_CONNECTOR;
		goto fail;
	}

	pos = end + 1;
	end = os_strchr(pos, '.');
	if (!end) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Missing dot(2) in signedConnector");
		ret = DPP_STATUS_INVALID_CONNECTOR;
		goto fail;
	}
	signed_end = end - 1;
	info->payload = base64_url_decode(pos, end - pos, &info->payload_len);
	if (!info->payload) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Failed to base64url decode signedConnector JWS Payload");
		ret = DPP_STATUS_INVALID_CONNECTOR;
		goto fail;
	}
	wpa_hexdump_ascii(MSG_DEBUG,
			  "DPP: signedConnector - JWS Payload",
			  info->payload, info->payload_len);
	pos = end + 1;
	signature = base64_url_decode(pos, os_strlen(pos), &signature_len);
	if (!signature) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Failed to base64url decode signedConnector signature");
		ret = DPP_STATUS_INVALID_CONNECTOR;
		goto fail;
		}
	wpa_hexdump(MSG_DEBUG, "DPP: signedConnector - signature",
		    signature, signature_len);

	if (dpp_check_pubkey_match(csign_pub, kid) < 0) {
		ret = DPP_STATUS_NO_MATCH;
		goto fail;
	}

	if (signature_len & 0x01) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unexpected signedConnector signature length (%d)",
			   (int) signature_len);
		ret = DPP_STATUS_INVALID_CONNECTOR;
		goto fail;
	}

	hash = os_malloc(curve->hash_len);
	if (!hash)
		goto fail;

	signed_len = signed_end - signed_start + 1;
	if (hash_func == CRYPTO_HASH_ALG_SHA256)
		res = sha256_vector(1, (const u8 **) &signed_start, &signed_len,
				    hash);
	else if (hash_func == CRYPTO_HASH_ALG_SHA384)
		res = sha384_vector(1, (const u8 **) &signed_start, &signed_len,
				    hash);
	else if (hash_func == CRYPTO_HASH_ALG_SHA512)
		res = sha512_vector(1, (const u8 **) &signed_start, &signed_len,
				    hash);
	else
		goto fail;

	if (res)
		goto fail;

	res = crypto_ec_key_verify_signature_r_s(csign_pub,
						 hash, curve->hash_len,
						 signature, signature_len / 2,
						 signature + signature_len / 2,
						 signature_len / 2);
	if (res != 1) {
		wpa_printf(MSG_DEBUG,
			   "DPP: signedConnector signature check failed (res=%d)",
			   res);
		ret = DPP_STATUS_INVALID_CONNECTOR;
		goto fail;
	}

	ret = DPP_STATUS_OK;
fail:
	os_free(hash);
	os_free(prot_hdr);
	wpabuf_free(kid);
	os_free(signature);
	return ret;
}


enum dpp_status_error
dpp_check_signed_connector(struct dpp_signed_connector_info *info,
			   const u8 *csign_key, size_t csign_key_len,
			   const u8 *peer_connector, size_t peer_connector_len)
{
	struct crypto_ec_key *csign;
	char *signed_connector = NULL;
	enum dpp_status_error res = DPP_STATUS_INVALID_CONNECTOR;

	csign = crypto_ec_key_parse_pub(csign_key, csign_key_len);
	if (!csign) {
		wpa_printf(MSG_ERROR,
			   "DPP: Failed to parse local C-sign-key information");
		goto fail;
	}

	wpa_hexdump_ascii(MSG_DEBUG, "DPP: Peer signedConnector",
			  peer_connector, peer_connector_len);
	signed_connector = os_malloc(peer_connector_len + 1);
	if (!signed_connector)
		goto fail;
	os_memcpy(signed_connector, peer_connector, peer_connector_len);
	signed_connector[peer_connector_len] = '\0';
	res = dpp_process_signed_connector(info, csign, signed_connector);
fail:
	os_free(signed_connector);
	crypto_ec_key_deinit(csign);
	return res;
}


int dpp_gen_r_auth(struct dpp_authentication *auth, u8 *r_auth)
{
	struct wpabuf *pix, *prx, *bix, *brx;
	const u8 *addr[7];
	size_t len[7];
	size_t i, num_elem = 0;
	size_t nonce_len;
	u8 zero = 0;
	int res = -1;

	/* R-auth = H(I-nonce | R-nonce | PI.x | PR.x | [BI.x |] BR.x | 0) */
	nonce_len = auth->curve->nonce_len;

	if (auth->initiator) {
		pix = crypto_ec_key_get_pubkey_point(auth->own_protocol_key, 0);
		prx = crypto_ec_key_get_pubkey_point(auth->peer_protocol_key,
						     0);
		if (auth->own_bi)
			bix = crypto_ec_key_get_pubkey_point(
				auth->own_bi->pubkey, 0);
		else
			bix = NULL;
		brx = crypto_ec_key_get_pubkey_point(auth->peer_bi->pubkey, 0);
	} else {
		pix = crypto_ec_key_get_pubkey_point(auth->peer_protocol_key,
						     0);
		prx = crypto_ec_key_get_pubkey_point(auth->own_protocol_key, 0);
		if (auth->peer_bi)
			bix = crypto_ec_key_get_pubkey_point(
				auth->peer_bi->pubkey, 0);
		else
			bix = NULL;
		brx = crypto_ec_key_get_pubkey_point(auth->own_bi->pubkey, 0);
	}
	if (!pix || !prx || !brx)
		goto fail;

	addr[num_elem] = auth->i_nonce;
	len[num_elem] = nonce_len;
	num_elem++;

	addr[num_elem] = auth->r_nonce;
	len[num_elem] = nonce_len;
	num_elem++;

	addr[num_elem] = wpabuf_head(pix);
	len[num_elem] = wpabuf_len(pix) / 2;
	num_elem++;

	addr[num_elem] = wpabuf_head(prx);
	len[num_elem] = wpabuf_len(prx) / 2;
	num_elem++;

	if (bix) {
		addr[num_elem] = wpabuf_head(bix);
		len[num_elem] = wpabuf_len(bix) / 2;
		num_elem++;
	}

	addr[num_elem] = wpabuf_head(brx);
	len[num_elem] = wpabuf_len(brx) / 2;
	num_elem++;

	addr[num_elem] = &zero;
	len[num_elem] = 1;
	num_elem++;

	wpa_printf(MSG_DEBUG, "DPP: R-auth hash components");
	for (i = 0; i < num_elem; i++)
		wpa_hexdump(MSG_DEBUG, "DPP: hash component", addr[i], len[i]);
	res = dpp_hash_vector(auth->curve, num_elem, addr, len, r_auth);
	if (res == 0)
		wpa_hexdump(MSG_DEBUG, "DPP: R-auth", r_auth,
			    auth->curve->hash_len);
fail:
	wpabuf_free(pix);
	wpabuf_free(prx);
	wpabuf_free(bix);
	wpabuf_free(brx);
	return res;
}


int dpp_gen_i_auth(struct dpp_authentication *auth, u8 *i_auth)
{
	struct wpabuf *pix = NULL, *prx = NULL, *bix = NULL, *brx = NULL;
	const u8 *addr[7];
	size_t len[7];
	size_t i, num_elem = 0;
	size_t nonce_len;
	u8 one = 1;
	int res = -1;

	/* I-auth = H(R-nonce | I-nonce | PR.x | PI.x | BR.x | [BI.x |] 1) */
	nonce_len = auth->curve->nonce_len;

	if (auth->initiator) {
		pix = crypto_ec_key_get_pubkey_point(auth->own_protocol_key, 0);
		prx = crypto_ec_key_get_pubkey_point(auth->peer_protocol_key,
						     0);
		if (auth->own_bi)
			bix = crypto_ec_key_get_pubkey_point(
				auth->own_bi->pubkey, 0);
		else
			bix = NULL;
		if (!auth->peer_bi)
			goto fail;
		brx = crypto_ec_key_get_pubkey_point(auth->peer_bi->pubkey, 0);
	} else {
		pix = crypto_ec_key_get_pubkey_point(auth->peer_protocol_key,
						     0);
		prx = crypto_ec_key_get_pubkey_point(auth->own_protocol_key, 0);
		if (auth->peer_bi)
			bix = crypto_ec_key_get_pubkey_point(
				auth->peer_bi->pubkey, 0);
		else
			bix = NULL;
		if (!auth->own_bi)
			goto fail;
		brx = crypto_ec_key_get_pubkey_point(auth->own_bi->pubkey, 0);
	}
	if (!pix || !prx || !brx)
		goto fail;

	addr[num_elem] = auth->r_nonce;
	len[num_elem] = nonce_len;
	num_elem++;

	addr[num_elem] = auth->i_nonce;
	len[num_elem] = nonce_len;
	num_elem++;

	addr[num_elem] = wpabuf_head(prx);
	len[num_elem] = wpabuf_len(prx) / 2;
	num_elem++;

	addr[num_elem] = wpabuf_head(pix);
	len[num_elem] = wpabuf_len(pix) / 2;
	num_elem++;

	addr[num_elem] = wpabuf_head(brx);
	len[num_elem] = wpabuf_len(brx) / 2;
	num_elem++;

	if (bix) {
		addr[num_elem] = wpabuf_head(bix);
		len[num_elem] = wpabuf_len(bix) / 2;
		num_elem++;
	}

	addr[num_elem] = &one;
	len[num_elem] = 1;
	num_elem++;

	wpa_printf(MSG_DEBUG, "DPP: I-auth hash components");
	for (i = 0; i < num_elem; i++)
		wpa_hexdump(MSG_DEBUG, "DPP: hash component", addr[i], len[i]);
	res = dpp_hash_vector(auth->curve, num_elem, addr, len, i_auth);
	if (res == 0)
		wpa_hexdump(MSG_DEBUG, "DPP: I-auth", i_auth,
			    auth->curve->hash_len);
fail:
	wpabuf_free(pix);
	wpabuf_free(prx);
	wpabuf_free(bix);
	wpabuf_free(brx);
	return res;
}


int dpp_auth_derive_l_responder(struct dpp_authentication *auth)
{
	struct crypto_ec *ec;
	struct crypto_ec_point *L = NULL;
	const struct crypto_ec_point *BI;
	const struct crypto_bignum *bR, *pR, *q;
	struct crypto_bignum *sum = NULL, *lx = NULL;
	int ret = -1;

	/* L = ((bR + pR) modulo q) * BI */

	ec = crypto_ec_init(crypto_ec_key_group(auth->peer_bi->pubkey));
	if (!ec)
		goto fail;

	q = crypto_ec_get_order(ec);
	BI = crypto_ec_key_get_public_key(auth->peer_bi->pubkey);
	bR = crypto_ec_key_get_private_key(auth->own_bi->pubkey);
	pR = crypto_ec_key_get_private_key(auth->own_protocol_key);
	sum = crypto_bignum_init();
	L = crypto_ec_point_init(ec);
	lx = crypto_bignum_init();
	if (!q || !BI || !bR || !pR || !sum || !L || !lx ||
	    crypto_bignum_addmod(bR, pR, q, sum) ||
	    crypto_ec_point_mul(ec, BI, sum, L) ||
	    crypto_ec_point_x(ec, L, lx) ||
	    crypto_bignum_to_bin(lx, auth->Lx, sizeof(auth->Lx),
				 auth->secret_len) < 0)
		goto fail;

	wpa_hexdump_key(MSG_DEBUG, "DPP: L.x", auth->Lx, auth->secret_len);
	auth->Lx_len = auth->secret_len;
	ret = 0;
fail:
	crypto_bignum_deinit(lx, 1);
	crypto_bignum_deinit(sum, 1);
	crypto_ec_point_deinit(L, 1);
	crypto_ec_deinit(ec);
	return ret;
}


int dpp_auth_derive_l_initiator(struct dpp_authentication *auth)
{
	struct crypto_ec *ec;
	struct crypto_ec_point *L = NULL, *sum = NULL;
	const struct crypto_ec_point *BR, *PR;
	const struct crypto_bignum *bI;
	struct crypto_bignum *lx = NULL;
	int ret = -1;

	/* L = bI * (BR + PR) */

	ec = crypto_ec_init(crypto_ec_key_group(auth->peer_bi->pubkey));
	if (!ec)
		goto fail;

	BR = crypto_ec_key_get_public_key(auth->peer_bi->pubkey);
	PR = crypto_ec_key_get_public_key(auth->peer_protocol_key);
	bI = crypto_ec_key_get_private_key(auth->own_bi->pubkey);
	sum = crypto_ec_point_init(ec);
	L = crypto_ec_point_init(ec);
	lx = crypto_bignum_init();
	if (!BR || !PR || !bI || !sum || !L || !lx ||
	    crypto_ec_point_add(ec, BR, PR, sum) ||
	    crypto_ec_point_mul(ec, sum, bI, L) ||
	    crypto_ec_point_x(ec, L, lx) ||
	    crypto_bignum_to_bin(lx, auth->Lx, sizeof(auth->Lx),
				 auth->secret_len) < 0)
		goto fail;

	wpa_hexdump_key(MSG_DEBUG, "DPP: L.x", auth->Lx, auth->secret_len);
	auth->Lx_len = auth->secret_len;
	ret = 0;
fail:
	crypto_bignum_deinit(lx, 1);
	crypto_ec_point_deinit(sum, 1);
	crypto_ec_point_deinit(L, 1);
	crypto_ec_deinit(ec);
	return ret;
}


int dpp_derive_pmk(const u8 *Nx, size_t Nx_len, u8 *pmk, unsigned int hash_len)
{
	u8 salt[DPP_MAX_HASH_LEN], prk[DPP_MAX_HASH_LEN];
	const char *info = "DPP PMK";
	int res;

	/* PMK = HKDF(<>, "DPP PMK", N.x) */

	/* HKDF-Extract(<>, N.x) */
	os_memset(salt, 0, hash_len);
	if (dpp_hmac(hash_len, salt, hash_len, Nx, Nx_len, prk) < 0)
		return -1;
	wpa_hexdump_key(MSG_DEBUG, "DPP: PRK = HKDF-Extract(<>, IKM=N.x)",
			prk, hash_len);

	/* HKDF-Expand(PRK, info, L) */
	res = dpp_hkdf_expand(hash_len, prk, hash_len, info, pmk, hash_len);
	os_memset(prk, 0, hash_len);
	if (res < 0)
		return -1;

	wpa_hexdump_key(MSG_DEBUG, "DPP: PMK = HKDF-Expand(PRK, info, L)",
			pmk, hash_len);
	return 0;
}


int dpp_derive_pmkid(const struct dpp_curve_params *curve,
		     struct crypto_ec_key *own_key,
		     struct crypto_ec_key *peer_key, u8 *pmkid)
{
	struct wpabuf *nkx, *pkx;
	int ret = -1, res;
	const u8 *addr[2];
	size_t len[2];
	u8 hash[SHA256_MAC_LEN];

	/* PMKID = Truncate-128(H(min(NK.x, PK.x) | max(NK.x, PK.x))) */
	nkx = crypto_ec_key_get_pubkey_point(own_key, 0);
	pkx = crypto_ec_key_get_pubkey_point(peer_key, 0);
	if (!nkx || !pkx)
		goto fail;
	addr[0] = wpabuf_head(nkx);
	len[0] = wpabuf_len(nkx) / 2;
	addr[1] = wpabuf_head(pkx);
	len[1] = wpabuf_len(pkx) / 2;
	if (len[0] != len[1])
		goto fail;
	if (os_memcmp(addr[0], addr[1], len[0]) > 0) {
		addr[0] = wpabuf_head(pkx);
		addr[1] = wpabuf_head(nkx);
	}
	wpa_hexdump(MSG_DEBUG, "DPP: PMKID hash payload 1", addr[0], len[0]);
	wpa_hexdump(MSG_DEBUG, "DPP: PMKID hash payload 2", addr[1], len[1]);
	res = sha256_vector(2, addr, len, hash);
	if (res < 0)
		goto fail;
	wpa_hexdump(MSG_DEBUG, "DPP: PMKID hash output", hash, SHA256_MAC_LEN);
	os_memcpy(pmkid, hash, PMKID_LEN);
	wpa_hexdump(MSG_DEBUG, "DPP: PMKID", pmkid, PMKID_LEN);
	ret = 0;
fail:
	wpabuf_free(nkx);
	wpabuf_free(pkx);
	return ret;
}


/* Role-specific elements for PKEX */

/* NIST P-256 */
static const u8 pkex_init_x_p256[32] = {
	0x56, 0x26, 0x12, 0xcf, 0x36, 0x48, 0xfe, 0x0b,
	0x07, 0x04, 0xbb, 0x12, 0x22, 0x50, 0xb2, 0x54,
	0xb1, 0x94, 0x64, 0x7e, 0x54, 0xce, 0x08, 0x07,
	0x2e, 0xec, 0xca, 0x74, 0x5b, 0x61, 0x2d, 0x25
 };
static const u8 pkex_init_y_p256[32] = {
	0x3e, 0x44, 0xc7, 0xc9, 0x8c, 0x1c, 0xa1, 0x0b,
	0x20, 0x09, 0x93, 0xb2, 0xfd, 0xe5, 0x69, 0xdc,
	0x75, 0xbc, 0xad, 0x33, 0xc1, 0xe7, 0xc6, 0x45,
	0x4d, 0x10, 0x1e, 0x6a, 0x3d, 0x84, 0x3c, 0xa4
 };
static const u8 pkex_resp_x_p256[32] = {
	0x1e, 0xa4, 0x8a, 0xb1, 0xa4, 0xe8, 0x42, 0x39,
	0xad, 0x73, 0x07, 0xf2, 0x34, 0xdf, 0x57, 0x4f,
	0xc0, 0x9d, 0x54, 0xbe, 0x36, 0x1b, 0x31, 0x0f,
	0x59, 0x91, 0x52, 0x33, 0xac, 0x19, 0x9d, 0x76
};
static const u8 pkex_resp_y_p256[32] = {
	0xd9, 0xfb, 0xf6, 0xb9, 0xf5, 0xfa, 0xdf, 0x19,
	0x58, 0xd8, 0x3e, 0xc9, 0x89, 0x7a, 0x35, 0xc1,
	0xbd, 0xe9, 0x0b, 0x77, 0x7a, 0xcb, 0x91, 0x2a,
	0xe8, 0x21, 0x3f, 0x47, 0x52, 0x02, 0x4d, 0x67
};

/* NIST P-384 */
static const u8 pkex_init_x_p384[48] = {
	0x95, 0x3f, 0x42, 0x9e, 0x50, 0x7f, 0xf9, 0xaa,
	0xac, 0x1a, 0xf2, 0x85, 0x2e, 0x64, 0x91, 0x68,
	0x64, 0xc4, 0x3c, 0xb7, 0x5c, 0xf8, 0xc9, 0x53,
	0x6e, 0x58, 0x4c, 0x7f, 0xc4, 0x64, 0x61, 0xac,
	0x51, 0x8a, 0x6f, 0xfe, 0xab, 0x74, 0xe6, 0x12,
	0x81, 0xac, 0x38, 0x5d, 0x41, 0xe6, 0xb9, 0xa3
};
static const u8 pkex_init_y_p384[48] = {
	0x76, 0x2f, 0x68, 0x84, 0xa6, 0xb0, 0x59, 0x29,
	0x83, 0xa2, 0x6c, 0xa4, 0x6c, 0x3b, 0xf8, 0x56,
	0x76, 0x11, 0x2a, 0x32, 0x90, 0xbd, 0x07, 0xc7,
	0x37, 0x39, 0x9d, 0xdb, 0x96, 0xf3, 0x2b, 0xb6,
	0x27, 0xbb, 0x29, 0x3c, 0x17, 0x33, 0x9d, 0x94,
	0xc3, 0xda, 0xac, 0x46, 0xb0, 0x8e, 0x07, 0x18
};
static const u8 pkex_resp_x_p384[48] = {
	0xad, 0xbe, 0xd7, 0x1d, 0x3a, 0x71, 0x64, 0x98,
	0x5f, 0xb4, 0xd6, 0x4b, 0x50, 0xd0, 0x84, 0x97,
	0x4b, 0x7e, 0x57, 0x70, 0xd2, 0xd9, 0xf4, 0x92,
	0x2a, 0x3f, 0xce, 0x99, 0xc5, 0x77, 0x33, 0x44,
	0x14, 0x56, 0x92, 0xcb, 0xae, 0x46, 0x64, 0xdf,
	0xe0, 0xbb, 0xd7, 0xb1, 0x29, 0x20, 0x72, 0xdf
};
static const u8 pkex_resp_y_p384[48] = {
	0xab, 0xa7, 0xdf, 0x52, 0xaa, 0xe2, 0x35, 0x0c,
	0xe3, 0x75, 0x32, 0xe6, 0xbf, 0x06, 0xc8, 0x7c,
	0x38, 0x29, 0x4c, 0xec, 0x82, 0xac, 0xd7, 0xa3,
	0x09, 0xd2, 0x0e, 0x22, 0x5a, 0x74, 0x52, 0xa1,
	0x7e, 0x54, 0x4e, 0xfe, 0xc6, 0x29, 0x33, 0x63,
	0x15, 0xe1, 0x7b, 0xe3, 0x40, 0x1c, 0xca, 0x06
};

/* NIST P-521 */
static const u8 pkex_init_x_p521[66] = {
	0x00, 0x16, 0x20, 0x45, 0x19, 0x50, 0x95, 0x23,
	0x0d, 0x24, 0xbe, 0x00, 0x87, 0xdc, 0xfa, 0xf0,
	0x58, 0x9a, 0x01, 0x60, 0x07, 0x7a, 0xca, 0x76,
	0x01, 0xab, 0x2d, 0x5a, 0x46, 0xcd, 0x2c, 0xb5,
	0x11, 0x9a, 0xff, 0xaa, 0x48, 0x04, 0x91, 0x38,
	0xcf, 0x86, 0xfc, 0xa4, 0xa5, 0x0f, 0x47, 0x01,
	0x80, 0x1b, 0x30, 0xa3, 0xae, 0xe8, 0x1c, 0x2e,
	0xea, 0xcc, 0xf0, 0x03, 0x9f, 0x77, 0x4c, 0x8d,
	0x97, 0x76
};
static const u8 pkex_init_y_p521[66] = {
	0x00, 0xb3, 0x8e, 0x02, 0xe4, 0x2a, 0x63, 0x59,
	0x12, 0xc6, 0x10, 0xba, 0x3a, 0xf9, 0x02, 0x99,
	0x3f, 0x14, 0xf0, 0x40, 0xde, 0x5c, 0xc9, 0x8b,
	0x02, 0x55, 0xfa, 0x91, 0xb1, 0xcc, 0x6a, 0xbd,
	0xe5, 0x62, 0xc0, 0xc5, 0xe3, 0xa1, 0x57, 0x9f,
	0x08, 0x1a, 0xa6, 0xe2, 0xf8, 0x55, 0x90, 0xbf,
	0xf5, 0xa6, 0xc3, 0xd8, 0x52, 0x1f, 0xb7, 0x02,
	0x2e, 0x7c, 0xc8, 0xb3, 0x20, 0x1e, 0x79, 0x8d,
	0x03, 0xa8
};
static const u8 pkex_resp_x_p521[66] = {
	0x00, 0x79, 0xe4, 0x4d, 0x6b, 0x5e, 0x12, 0x0a,
	0x18, 0x2c, 0xb3, 0x05, 0x77, 0x0f, 0xc3, 0x44,
	0x1a, 0xcd, 0x78, 0x46, 0x14, 0xee, 0x46, 0x3f,
	0xab, 0xc9, 0x59, 0x7c, 0x85, 0xa0, 0xc2, 0xfb,
	0x02, 0x32, 0x99, 0xde, 0x5d, 0xe1, 0x0d, 0x48,
	0x2d, 0x71, 0x7d, 0x8d, 0x3f, 0x61, 0x67, 0x9e,
	0x2b, 0x8b, 0x12, 0xde, 0x10, 0x21, 0x55, 0x0a,
	0x5b, 0x2d, 0xe8, 0x05, 0x09, 0xf6, 0x20, 0x97,
	0x84, 0xb4
};
static const u8 pkex_resp_y_p521[66] = {
	0x00, 0x46, 0x63, 0x39, 0xbe, 0xcd, 0xa4, 0x2d,
	0xca, 0x27, 0x74, 0xd4, 0x1b, 0x91, 0x33, 0x20,
	0x83, 0xc7, 0x3b, 0xa4, 0x09, 0x8b, 0x8e, 0xa3,
	0x88, 0xe9, 0x75, 0x7f, 0x56, 0x7b, 0x38, 0x84,
	0x62, 0x02, 0x7c, 0x90, 0x51, 0x07, 0xdb, 0xe9,
	0xd0, 0xde, 0xda, 0x9a, 0x5d, 0xe5, 0x94, 0xd2,
	0xcf, 0x9d, 0x4c, 0x33, 0x91, 0xa6, 0xc3, 0x80,
	0xa7, 0x6e, 0x7e, 0x8d, 0xf8, 0x73, 0x6e, 0x53,
	0xce, 0xe1
};

/* Brainpool P-256r1 */
static const u8 pkex_init_x_bp_p256r1[32] = {
	0x46, 0x98, 0x18, 0x6c, 0x27, 0xcd, 0x4b, 0x10,
	0x7d, 0x55, 0xa3, 0xdd, 0x89, 0x1f, 0x9f, 0xca,
	0xc7, 0x42, 0x5b, 0x8a, 0x23, 0xed, 0xf8, 0x75,
	0xac, 0xc7, 0xe9, 0x8d, 0xc2, 0x6f, 0xec, 0xd8
};
static const u8 pkex_init_y_bp_p256r1[32] = {
	0x93, 0xca, 0xef, 0xa9, 0x66, 0x3e, 0x87, 0xcd,
	0x52, 0x6e, 0x54, 0x13, 0xef, 0x31, 0x67, 0x30,
	0x15, 0x13, 0x9d, 0x6d, 0xc0, 0x95, 0x32, 0xbe,
	0x4f, 0xab, 0x5d, 0xf7, 0xbf, 0x5e, 0xaa, 0x0b
};
static const u8 pkex_resp_x_bp_p256r1[32] = {
	0x90, 0x18, 0x84, 0xc9, 0xdc, 0xcc, 0xb5, 0x2f,
	0x4a, 0x3f, 0x4f, 0x18, 0x0a, 0x22, 0x56, 0x6a,
	0xa9, 0xef, 0xd4, 0xe6, 0xc3, 0x53, 0xc2, 0x1a,
	0x23, 0x54, 0xdd, 0x08, 0x7e, 0x10, 0xd8, 0xe3
};
static const u8 pkex_resp_y_bp_p256r1[32] = {
	0x2a, 0xfa, 0x98, 0x9b, 0xe3, 0xda, 0x30, 0xfd,
	0x32, 0x28, 0xcb, 0x66, 0xfb, 0x40, 0x7f, 0xf2,
	0xb2, 0x25, 0x80, 0x82, 0x44, 0x85, 0x13, 0x7e,
	0x4b, 0xb5, 0x06, 0xc0, 0x03, 0x69, 0x23, 0x64
};

/* Brainpool P-384r1 */
static const u8 pkex_init_x_bp_p384r1[48] = {
	0x0a, 0x2c, 0xeb, 0x49, 0x5e, 0xb7, 0x23, 0xbd,
	0x20, 0x5b, 0xe0, 0x49, 0xdf, 0xcf, 0xcf, 0x19,
	0x37, 0x36, 0xe1, 0x2f, 0x59, 0xdb, 0x07, 0x06,
	0xb5, 0xeb, 0x2d, 0xae, 0xc2, 0xb2, 0x38, 0x62,
	0xa6, 0x73, 0x09, 0xa0, 0x6c, 0x0a, 0xa2, 0x30,
	0x99, 0xeb, 0xf7, 0x1e, 0x47, 0xb9, 0x5e, 0xbe
};
static const u8 pkex_init_y_bp_p384r1[48] = {
	0x54, 0x76, 0x61, 0x65, 0x75, 0x5a, 0x2f, 0x99,
	0x39, 0x73, 0xca, 0x6c, 0xf9, 0xf7, 0x12, 0x86,
	0x54, 0xd5, 0xd4, 0xad, 0x45, 0x7b, 0xbf, 0x32,
	0xee, 0x62, 0x8b, 0x9f, 0x52, 0xe8, 0xa0, 0xc9,
	0xb7, 0x9d, 0xd1, 0x09, 0xb4, 0x79, 0x1c, 0x3e,
	0x1a, 0xbf, 0x21, 0x45, 0x66, 0x6b, 0x02, 0x52
};
static const u8 pkex_resp_x_bp_p384r1[48] = {
	0x03, 0xa2, 0x57, 0xef, 0xe8, 0x51, 0x21, 0xa0,
	0xc8, 0x9e, 0x21, 0x02, 0xb5, 0x9a, 0x36, 0x25,
	0x74, 0x22, 0xd1, 0xf2, 0x1b, 0xa8, 0x9a, 0x9b,
	0x97, 0xbc, 0x5a, 0xeb, 0x26, 0x15, 0x09, 0x71,
	0x77, 0x59, 0xec, 0x8b, 0xb7, 0xe1, 0xe8, 0xce,
	0x65, 0xb8, 0xaf, 0xf8, 0x80, 0xae, 0x74, 0x6c
};
static const u8 pkex_resp_y_bp_p384r1[48] = {
	0x2f, 0xd9, 0x6a, 0xc7, 0x3e, 0xec, 0x76, 0x65,
	0x2d, 0x38, 0x7f, 0xec, 0x63, 0x26, 0x3f, 0x04,
	0xd8, 0x4e, 0xff, 0xe1, 0x0a, 0x51, 0x74, 0x70,
	0xe5, 0x46, 0x63, 0x7f, 0x5c, 0xc0, 0xd1, 0x7c,
	0xfb, 0x2f, 0xea, 0xe2, 0xd8, 0x0f, 0x84, 0xcb,
	0xe9, 0x39, 0x5c, 0x64, 0xfe, 0xcb, 0x2f, 0xf1
};

/* Brainpool P-512r1 */
static const u8 pkex_init_x_bp_p512r1[64] = {
	0x4c, 0xe9, 0xb6, 0x1c, 0xe2, 0x00, 0x3c, 0x9c,
	0xa9, 0xc8, 0x56, 0x52, 0xaf, 0x87, 0x3e, 0x51,
	0x9c, 0xbb, 0x15, 0x31, 0x1e, 0xc1, 0x05, 0xfc,
	0x7c, 0x77, 0xd7, 0x37, 0x61, 0x27, 0xd0, 0x95,
	0x98, 0xee, 0x5d, 0xa4, 0x3d, 0x09, 0xdb, 0x3d,
	0xfa, 0x89, 0x9e, 0x7f, 0xa6, 0xa6, 0x9c, 0xff,
	0x83, 0x5c, 0x21, 0x6c, 0x3e, 0xf2, 0xfe, 0xdc,
	0x63, 0xe4, 0xd1, 0x0e, 0x75, 0x45, 0x69, 0x0f
};
static const u8 pkex_init_y_bp_p512r1[64] = {
	0x50, 0xb5, 0x9b, 0xfa, 0x45, 0x67, 0x75, 0x94,
	0x44, 0xe7, 0x68, 0xb0, 0xeb, 0x3e, 0xb3, 0xb8,
	0xf9, 0x99, 0x05, 0xef, 0xae, 0x6c, 0xbc, 0xe3,
	0xe1, 0xd2, 0x51, 0x54, 0xdf, 0x59, 0xd4, 0x45,
	0x41, 0x3a, 0xa8, 0x0b, 0x76, 0x32, 0x44, 0x0e,
	0x07, 0x60, 0x3a, 0x6e, 0xbe, 0xfe, 0xe0, 0x58,
	0x52, 0xa0, 0xaa, 0x8b, 0xd8, 0x5b, 0xf2, 0x71,
	0x11, 0x9a, 0x9e, 0x8f, 0x1a, 0xd1, 0xc9, 0x99
};
static const u8 pkex_resp_x_bp_p512r1[64] = {
	0x2a, 0x60, 0x32, 0x27, 0xa1, 0xe6, 0x94, 0x72,
	0x1c, 0x48, 0xbe, 0xc5, 0x77, 0x14, 0x30, 0x76,
	0xe4, 0xbf, 0xf7, 0x7b, 0xc5, 0xfd, 0xdf, 0x19,
	0x1e, 0x0f, 0xdf, 0x1c, 0x40, 0xfa, 0x34, 0x9e,
	0x1f, 0x42, 0x24, 0xa3, 0x2c, 0xd5, 0xc7, 0xc9,
	0x7b, 0x47, 0x78, 0x96, 0xf1, 0x37, 0x0e, 0x88,
	0xcb, 0xa6, 0x52, 0x29, 0xd7, 0xa8, 0x38, 0x29,
	0x8e, 0x6e, 0x23, 0x47, 0xd4, 0x4b, 0x70, 0x3e
};
static const u8 pkex_resp_y_bp_p512r1[64] = {
	0x80, 0x1f, 0x43, 0xd2, 0x17, 0x35, 0xec, 0x81,
	0xd9, 0x4b, 0xdc, 0x81, 0x19, 0xd9, 0x5f, 0x68,
	0x16, 0x84, 0xfe, 0x63, 0x4b, 0x8d, 0x5d, 0xaa,
	0x88, 0x4a, 0x47, 0x48, 0xd4, 0xea, 0xab, 0x7d,
	0x6a, 0xbf, 0xe1, 0x28, 0x99, 0x6a, 0x87, 0x1c,
	0x30, 0xb4, 0x44, 0x2d, 0x75, 0xac, 0x35, 0x09,
	0x73, 0x24, 0x3d, 0xb4, 0x43, 0xb1, 0xc1, 0x56,
	0x56, 0xad, 0x30, 0x87, 0xf4, 0xc3, 0x00, 0xc7
};


static struct crypto_ec_key *
dpp_pkex_get_role_elem(const struct dpp_curve_params *curve, int init)
{
	const u8 *x, *y;

	switch (curve->ike_group) {
	case 19:
		x = init ? pkex_init_x_p256 : pkex_resp_x_p256;
		y = init ? pkex_init_y_p256 : pkex_resp_y_p256;
		break;
	case 20:
		x = init ? pkex_init_x_p384 : pkex_resp_x_p384;
		y = init ? pkex_init_y_p384 : pkex_resp_y_p384;
		break;
	case 21:
		x = init ? pkex_init_x_p521 : pkex_resp_x_p521;
		y = init ? pkex_init_y_p521 : pkex_resp_y_p521;
		break;
	case 28:
		x = init ? pkex_init_x_bp_p256r1 : pkex_resp_x_bp_p256r1;
		y = init ? pkex_init_y_bp_p256r1 : pkex_resp_y_bp_p256r1;
		break;
	case 29:
		x = init ? pkex_init_x_bp_p384r1 : pkex_resp_x_bp_p384r1;
		y = init ? pkex_init_y_bp_p384r1 : pkex_resp_y_bp_p384r1;
		break;
	case 30:
		x = init ? pkex_init_x_bp_p512r1 : pkex_resp_x_bp_p512r1;
		y = init ? pkex_init_y_bp_p512r1 : pkex_resp_y_bp_p512r1;
		break;
	default:
		return NULL;
	}

	return crypto_ec_key_set_pub(curve->ike_group, x, y, curve->prime_len);
}


struct crypto_ec_point *
dpp_pkex_derive_Qi(const struct dpp_curve_params *curve, const u8 *mac_init,
		   const char *code, const char *identifier,
		   struct crypto_ec **ret_ec)
{
	u8 hash[DPP_MAX_HASH_LEN];
	const u8 *addr[3];
	size_t len[3];
	unsigned int num_elem = 0;
	struct crypto_ec_point *Qi = NULL;
	struct crypto_ec_key *Pi_key = NULL;
	const struct crypto_ec_point *Pi = NULL;
	struct crypto_bignum *hash_bn = NULL;
	struct crypto_ec *ec = NULL;

	/* Qi = H(MAC-Initiator | [identifier |] code) * Pi */

	wpa_printf(MSG_DEBUG, "DPP: MAC-Initiator: " MACSTR, MAC2STR(mac_init));
	addr[num_elem] = mac_init;
	len[num_elem] = ETH_ALEN;
	num_elem++;
	if (identifier) {
		wpa_printf(MSG_DEBUG, "DPP: code identifier: %s",
			   identifier);
		addr[num_elem] = (const u8 *) identifier;
		len[num_elem] = os_strlen(identifier);
		num_elem++;
	}
	wpa_hexdump_ascii_key(MSG_DEBUG, "DPP: code", code, os_strlen(code));
	addr[num_elem] = (const u8 *) code;
	len[num_elem] = os_strlen(code);
	num_elem++;
	if (dpp_hash_vector(curve, num_elem, addr, len, hash) < 0)
		goto fail;
	wpa_hexdump_key(MSG_DEBUG,
			"DPP: H(MAC-Initiator | [identifier |] code)",
			hash, curve->hash_len);
	Pi_key = dpp_pkex_get_role_elem(curve, 1);
	if (!Pi_key)
		goto fail;
	dpp_debug_print_key("DPP: Pi", Pi_key);

	ec = crypto_ec_init(curve->ike_group);
	if (!ec)
		goto fail;

	Pi = crypto_ec_key_get_public_key(Pi_key);
	Qi = crypto_ec_point_init(ec);
	hash_bn = crypto_bignum_init_set(hash, curve->hash_len);
	if (!Pi || !Qi || !hash_bn || crypto_ec_point_mul(ec, Pi, hash_bn, Qi))
		goto fail;

	if (crypto_ec_point_is_at_infinity(ec, Qi)) {
		wpa_printf(MSG_INFO, "DPP: Qi is the point-at-infinity");
		goto fail;
	}
	crypto_ec_point_debug_print(ec, Qi, "DPP: Qi");
out:
	crypto_ec_key_deinit(Pi_key);
	crypto_bignum_deinit(hash_bn, 1);
	if (ret_ec && Qi)
		*ret_ec = ec;
	else
		crypto_ec_deinit(ec);
	return Qi;
fail:
	crypto_ec_point_deinit(Qi, 1);
	Qi = NULL;
	goto out;
}


struct crypto_ec_point *
dpp_pkex_derive_Qr(const struct dpp_curve_params *curve, const u8 *mac_resp,
		   const char *code, const char *identifier,
		   struct crypto_ec **ret_ec)
{
	u8 hash[DPP_MAX_HASH_LEN];
	const u8 *addr[3];
	size_t len[3];
	unsigned int num_elem = 0;
	struct crypto_ec_point *Qr = NULL;
	struct crypto_ec_key *Pr_key = NULL;
	const struct crypto_ec_point *Pr = NULL;
	struct crypto_bignum *hash_bn = NULL;
	struct crypto_ec *ec = NULL;

	/* Qr = H(MAC-Responder | | [identifier | ] code) * Pr */

	wpa_printf(MSG_DEBUG, "DPP: MAC-Responder: " MACSTR, MAC2STR(mac_resp));
	addr[num_elem] = mac_resp;
	len[num_elem] = ETH_ALEN;
	num_elem++;
	if (identifier) {
		wpa_printf(MSG_DEBUG, "DPP: code identifier: %s",
			   identifier);
		addr[num_elem] = (const u8 *) identifier;
		len[num_elem] = os_strlen(identifier);
		num_elem++;
	}
	wpa_hexdump_ascii_key(MSG_DEBUG, "DPP: code", code, os_strlen(code));
	addr[num_elem] = (const u8 *) code;
	len[num_elem] = os_strlen(code);
	num_elem++;
	if (dpp_hash_vector(curve, num_elem, addr, len, hash) < 0)
		goto fail;
	wpa_hexdump_key(MSG_DEBUG,
			"DPP: H(MAC-Responder | [identifier |] code)",
			hash, curve->hash_len);
	Pr_key = dpp_pkex_get_role_elem(curve, 0);
	if (!Pr_key)
		goto fail;
	dpp_debug_print_key("DPP: Pr", Pr_key);

	ec = crypto_ec_init(curve->ike_group);
	if (!ec)
		goto fail;

	Pr = crypto_ec_key_get_public_key(Pr_key);
	Qr = crypto_ec_point_init(ec);
	hash_bn = crypto_bignum_init_set(hash, curve->hash_len);
	if (!Pr || !Qr || !hash_bn || crypto_ec_point_mul(ec, Pr, hash_bn, Qr))
		goto fail;

	if (crypto_ec_point_is_at_infinity(ec, Qr)) {
		wpa_printf(MSG_INFO, "DPP: Qr is the point-at-infinity");
		goto fail;
	}
	crypto_ec_point_debug_print(ec, Qr, "DPP: Qr");

out:
	crypto_ec_key_deinit(Pr_key);
	crypto_bignum_deinit(hash_bn, 1);
	if (ret_ec && Qr)
		*ret_ec = ec;
	else
		crypto_ec_deinit(ec);
	return Qr;
fail:
	crypto_ec_point_deinit(Qr, 1);
	Qr = NULL;
	goto out;
}


int dpp_pkex_derive_z(const u8 *mac_init, const u8 *mac_resp,
		      const u8 *Mx, size_t Mx_len,
		      const u8 *Nx, size_t Nx_len,
		      const char *code,
		      const u8 *Kx, size_t Kx_len,
		      u8 *z, unsigned int hash_len)
{
	u8 salt[DPP_MAX_HASH_LEN], prk[DPP_MAX_HASH_LEN];
	int res;
	u8 *info, *pos;
	size_t info_len;

	/* z = HKDF(<>, MAC-Initiator | MAC-Responder | M.x | N.x | code, K.x)
	 */

	/* HKDF-Extract(<>, IKM=K.x) */
	os_memset(salt, 0, hash_len);
	if (dpp_hmac(hash_len, salt, hash_len, Kx, Kx_len, prk) < 0)
		return -1;
	wpa_hexdump_key(MSG_DEBUG, "DPP: PRK = HKDF-Extract(<>, IKM)",
			prk, hash_len);
	info_len = 2 * ETH_ALEN + Mx_len + Nx_len + os_strlen(code);
	info = os_malloc(info_len);
	if (!info)
		return -1;
	pos = info;
	os_memcpy(pos, mac_init, ETH_ALEN);
	pos += ETH_ALEN;
	os_memcpy(pos, mac_resp, ETH_ALEN);
	pos += ETH_ALEN;
	os_memcpy(pos, Mx, Mx_len);
	pos += Mx_len;
	os_memcpy(pos, Nx, Nx_len);
	pos += Nx_len;
	os_memcpy(pos, code, os_strlen(code));

	/* HKDF-Expand(PRK, info, L) */
	if (hash_len == 32)
		res = hmac_sha256_kdf(prk, hash_len, NULL, info, info_len,
				      z, hash_len);
	else if (hash_len == 48)
		res = hmac_sha384_kdf(prk, hash_len, NULL, info, info_len,
				      z, hash_len);
	else if (hash_len == 64)
		res = hmac_sha512_kdf(prk, hash_len, NULL, info, info_len,
				      z, hash_len);
	else
		res = -1;
	os_free(info);
	os_memset(prk, 0, hash_len);
	if (res < 0)
		return -1;

	wpa_hexdump_key(MSG_DEBUG, "DPP: z = HKDF-Expand(PRK, info, L)",
			z, hash_len);
	return 0;
}


int dpp_reconfig_derive_ke_responder(struct dpp_authentication *auth,
				     const u8 *net_access_key,
				     size_t net_access_key_len,
				     struct json_token *peer_net_access_key)
{
	struct crypto_ec_key *own_key = NULL, *peer_key = NULL;
	struct crypto_bignum *sum = NULL;
	const struct crypto_bignum *q, *cR, *pR;
	struct crypto_ec *ec = NULL;
	struct crypto_ec_point *M = NULL;
	const struct crypto_ec_point *CI;
	u8 Mx[DPP_MAX_SHARED_SECRET_LEN];
	u8 prk[DPP_MAX_HASH_LEN];
	const struct dpp_curve_params *curve;
	int res = -1;
	u8 nonces[2 * DPP_MAX_NONCE_LEN];

	own_key = dpp_set_keypair(&auth->curve, net_access_key,
				  net_access_key_len);
	if (!own_key) {
		dpp_auth_fail(auth, "Failed to parse own netAccessKey");
		goto fail;
	}

	peer_key = dpp_parse_jwk(peer_net_access_key, &curve);
	if (!peer_key)
		goto fail;
	dpp_debug_print_key("DPP: Received netAccessKey", peer_key);

	if (auth->curve != curve) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Mismatching netAccessKey curves (own=%s != peer=%s)",
			   auth->curve->name, curve->name);
		goto fail;
	}

	auth->own_protocol_key = dpp_gen_keypair(curve);
	if (!auth->own_protocol_key)
		goto fail;

	if (random_get_bytes(auth->e_nonce, auth->curve->nonce_len)) {
		wpa_printf(MSG_ERROR, "DPP: Failed to generate E-nonce");
		goto fail;
	}
	wpa_hexdump_key(MSG_DEBUG, "DPP: E-nonce",
			auth->e_nonce, auth->curve->nonce_len);

	/* M = { cR + pR } * CI */
	ec = crypto_ec_init(curve->ike_group);
	if (!ec)
		goto fail;

	sum = crypto_bignum_init();
	q = crypto_ec_get_order(ec);
	M = crypto_ec_point_init(ec);
	cR = crypto_ec_key_get_private_key(own_key);
	pR = crypto_ec_key_get_private_key(auth->own_protocol_key);
	CI = crypto_ec_key_get_public_key(peer_key);
	if (!sum || !q || !M || !cR || !pR || !CI ||
	    crypto_bignum_addmod(cR, pR, q, sum) ||
	    crypto_ec_point_mul(ec, CI, sum, M) ||
	    crypto_ec_point_to_bin(ec, M, Mx, NULL)) {
		wpa_printf(MSG_ERROR, "DPP: Error during M computation");
		goto fail;
	}
	wpa_hexdump_key(MSG_DEBUG, "DPP: M.x", Mx, curve->prime_len);

	/* ke = HKDF(C-nonce | E-nonce, "dpp reconfig key", M.x) */

	/* HKDF-Extract(C-nonce | E-nonce, M.x) */
	os_memcpy(nonces, auth->c_nonce, curve->nonce_len);
	os_memcpy(&nonces[curve->nonce_len], auth->e_nonce, curve->nonce_len);
	if (dpp_hmac(curve->hash_len, nonces, 2 * curve->nonce_len,
		     Mx, curve->prime_len, prk) < 0)
		goto fail;
	wpa_hexdump_key(MSG_DEBUG, "DPP: PRK", prk, curve->hash_len);

	/* HKDF-Expand(PRK, "dpp reconfig key", L) */
	if (dpp_hkdf_expand(curve->hash_len, prk, curve->hash_len,
			    "dpp reconfig key", auth->ke, curve->hash_len) < 0)
		goto fail;
	wpa_hexdump_key(MSG_DEBUG,
			"DPP: ke = HKDF(C-nonce | E-nonce, \"dpp reconfig key\", M.x)",
			auth->ke, curve->hash_len);

	res = 0;
	crypto_ec_key_deinit(auth->reconfig_old_protocol_key);
	auth->reconfig_old_protocol_key = own_key;
	own_key = NULL;
fail:
	forced_memzero(prk, sizeof(prk));
	forced_memzero(Mx, sizeof(Mx));
	crypto_ec_point_deinit(M, 1);
	crypto_bignum_deinit(sum, 1);
	crypto_ec_key_deinit(own_key);
	crypto_ec_key_deinit(peer_key);
	crypto_ec_deinit(ec);
	return res;
}


int dpp_reconfig_derive_ke_initiator(struct dpp_authentication *auth,
				     const u8 *r_proto, u16 r_proto_len,
				     struct json_token *net_access_key)
{
	struct crypto_ec_key *pr = NULL, *peer_key = NULL;
	const struct crypto_ec_point *CR, *PR;
	const struct crypto_bignum *cI;
	struct crypto_ec *ec = NULL;
	struct crypto_ec_point *sum = NULL, *M = NULL;
	u8 Mx[DPP_MAX_SHARED_SECRET_LEN];
	u8 prk[DPP_MAX_HASH_LEN];
	int res = -1;
	const struct dpp_curve_params *curve;
	u8 nonces[2 * DPP_MAX_NONCE_LEN];

	pr = dpp_set_pubkey_point(auth->conf->connector_key,
				  r_proto, r_proto_len);
	if (!pr) {
		dpp_auth_fail(auth, "Invalid Responder Protocol Key");
		goto fail;
	}
	dpp_debug_print_key("Peer (Responder) Protocol Key", pr);
	crypto_ec_key_deinit(auth->peer_protocol_key);
	auth->peer_protocol_key = pr;
	pr = NULL;

	peer_key = dpp_parse_jwk(net_access_key, &curve);
	if (!peer_key)
		goto fail;
	dpp_debug_print_key("DPP: Received netAccessKey", peer_key);
	if (auth->curve != curve) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Mismatching netAccessKey curves (own=%s != peer=%s)",
			   auth->curve->name, curve->name);
		goto fail;
	}

	/* M = cI * { CR + PR } */
	ec = crypto_ec_init(curve->ike_group);
	if (!ec)
		goto fail;

	cI = crypto_ec_key_get_private_key(auth->conf->connector_key);
	sum = crypto_ec_point_init(ec);
	M = crypto_ec_point_init(ec);
	CR = crypto_ec_key_get_public_key(peer_key);
	PR = crypto_ec_key_get_public_key(auth->peer_protocol_key);
	if (!cI || !sum || !M || !CR || !PR ||
	    crypto_ec_point_add(ec, CR, PR, sum) ||
	    crypto_ec_point_mul(ec, sum, cI, M) ||
	    crypto_ec_point_to_bin(ec, M, Mx, NULL)) {
		wpa_printf(MSG_ERROR, "DPP: Error during M computation");
		goto fail;
	}

	wpa_hexdump_key(MSG_DEBUG, "DPP: M.x", Mx, curve->prime_len);

	/* ke = HKDF(C-nonce | E-nonce, "dpp reconfig key", M.x) */

	/* HKDF-Extract(C-nonce | E-nonce, M.x) */
	os_memcpy(nonces, auth->c_nonce, curve->nonce_len);
	os_memcpy(&nonces[curve->nonce_len], auth->e_nonce, curve->nonce_len);
	if (dpp_hmac(curve->hash_len, nonces, 2 * curve->nonce_len,
		     Mx, curve->prime_len, prk) < 0)
		goto fail;
	wpa_hexdump_key(MSG_DEBUG, "DPP: PRK", prk, curve->hash_len);

	/* HKDF-Expand(PRK, "dpp reconfig key", L) */
	if (dpp_hkdf_expand(curve->hash_len, prk, curve->hash_len,
			    "dpp reconfig key", auth->ke, curve->hash_len) < 0)
		goto fail;
	wpa_hexdump_key(MSG_DEBUG,
			"DPP: ke = HKDF(C-nonce | E-nonce, \"dpp reconfig key\", M.x)",
			auth->ke, curve->hash_len);

	res = 0;
fail:
	forced_memzero(prk, sizeof(prk));
	forced_memzero(Mx, sizeof(Mx));
	crypto_ec_key_deinit(pr);
	crypto_ec_key_deinit(peer_key);
	crypto_ec_point_deinit(sum, 1);
	crypto_ec_point_deinit(M, 1);
	crypto_ec_deinit(ec);
	return res;
}


static char *
dpp_build_jws_prot_hdr(struct dpp_configurator *conf, size_t *signed1_len)
{
	struct wpabuf *jws_prot_hdr;
	char *signed1;

	jws_prot_hdr = wpabuf_alloc(100);
	if (!jws_prot_hdr)
		return NULL;
	json_start_object(jws_prot_hdr, NULL);
	json_add_string(jws_prot_hdr, "typ", "dppCon");
	json_value_sep(jws_prot_hdr);
	json_add_string(jws_prot_hdr, "kid", conf->kid);
	json_value_sep(jws_prot_hdr);
	json_add_string(jws_prot_hdr, "alg", conf->curve->jws_alg);
	json_end_object(jws_prot_hdr);
	signed1 = base64_url_encode(wpabuf_head(jws_prot_hdr),
				    wpabuf_len(jws_prot_hdr),
				    signed1_len);
	wpabuf_free(jws_prot_hdr);
	return signed1;
}


static char *
dpp_build_conn_signature(struct dpp_configurator *conf,
			 const char *signed1, size_t signed1_len,
			 const char *signed2, size_t signed2_len,
			 size_t *signed3_len)
{
	const struct dpp_curve_params *curve;
	struct wpabuf *sig = NULL;
	char *signed3 = NULL;
	char *dot = ".";
	const u8 *vector[3];
	size_t vector_len[3];
	u8 *hash;
	int ret;

	vector[0] = (const u8 *) signed1;
	vector[1] = (const u8 *) dot;
	vector[2] = (const u8 *) signed2;
	vector_len[0] = signed1_len;
	vector_len[1] = 1;
	vector_len[2] = signed2_len;

	curve = conf->curve;
	hash = os_malloc(curve->hash_len);
	if (!hash)
		goto fail;
	if (curve->hash_len == SHA256_MAC_LEN) {
		ret = sha256_vector(3, vector, vector_len, hash);
	} else if (curve->hash_len == SHA384_MAC_LEN) {
		ret = sha384_vector(3, vector, vector_len, hash);
	} else if (curve->hash_len == SHA512_MAC_LEN) {
		ret = sha512_vector(3, vector, vector_len, hash);
	} else {
		wpa_printf(MSG_DEBUG, "DPP: Unknown signature algorithm");
		goto fail;
	}
	if (ret) {
		wpa_printf(MSG_DEBUG, "DPP: Hash computation failed");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: Hash value for Connector signature",
		    hash, curve->hash_len);

	sig = crypto_ec_key_sign_r_s(conf->csign, hash, curve->hash_len);
	if (!sig) {
		wpa_printf(MSG_ERROR, "DPP: Signature computation failed");
		goto fail;
	}

	wpa_hexdump(MSG_DEBUG, "DPP: signedConnector ECDSA signature (raw r,s)",
		    wpabuf_head(sig), wpabuf_len(sig));
	signed3 = base64_url_encode(wpabuf_head(sig), wpabuf_len(sig),
				    signed3_len);

fail:
	os_free(hash);
	wpabuf_free(sig);
	return signed3;
}

char * dpp_sign_connector(struct dpp_configurator *conf,
			  const struct wpabuf *dppcon)
{
	char *signed1 = NULL, *signed2 = NULL, *signed3 = NULL;
	char *signed_conn = NULL, *pos;
	size_t signed1_len, signed2_len, signed3_len;

	signed1 = dpp_build_jws_prot_hdr(conf, &signed1_len);
	signed2 = base64_url_encode(wpabuf_head(dppcon), wpabuf_len(dppcon),
				    &signed2_len);
	if (!signed1 || !signed2)
		goto fail;

	signed3 = dpp_build_conn_signature(conf, signed1, signed1_len,
					   signed2, signed2_len, &signed3_len);
	if (!signed3)
		goto fail;

	signed_conn = os_malloc(signed1_len + signed2_len + signed3_len + 3);
	if (!signed_conn)
		goto fail;
	pos = signed_conn;
	os_memcpy(pos, signed1, signed1_len);
	pos += signed1_len;
	*pos++ = '.';
	os_memcpy(pos, signed2, signed2_len);
	pos += signed2_len;
	*pos++ = '.';
	os_memcpy(pos, signed3, signed3_len);
	pos += signed3_len;
	*pos = '\0';

fail:
	os_free(signed1);
	os_free(signed2);
	os_free(signed3);
	return signed_conn;
}


#ifdef CONFIG_DPP2

struct dpp_pfs * dpp_pfs_init(const u8 *net_access_key,
			      size_t net_access_key_len)
{
	struct wpabuf *pub = NULL;
	struct crypto_ec_key *own_key;
	struct dpp_pfs *pfs;

	pfs = os_zalloc(sizeof(*pfs));
	if (!pfs)
		return NULL;

	own_key = dpp_set_keypair(&pfs->curve, net_access_key,
				  net_access_key_len);
	if (!own_key) {
		wpa_printf(MSG_ERROR, "DPP: Failed to parse own netAccessKey");
		goto fail;
	}
	crypto_ec_key_deinit(own_key);

	pfs->ecdh = crypto_ecdh_init(pfs->curve->ike_group);
	if (!pfs->ecdh)
		goto fail;

	pub = crypto_ecdh_get_pubkey(pfs->ecdh, 0);
	pub = wpabuf_zeropad(pub, pfs->curve->prime_len);
	if (!pub)
		goto fail;

	pfs->ie = wpabuf_alloc(5 + wpabuf_len(pub));
	if (!pfs->ie)
		goto fail;
	wpabuf_put_u8(pfs->ie, WLAN_EID_EXTENSION);
	wpabuf_put_u8(pfs->ie, 1 + 2 + wpabuf_len(pub));
	wpabuf_put_u8(pfs->ie, WLAN_EID_EXT_OWE_DH_PARAM);
	wpabuf_put_le16(pfs->ie, pfs->curve->ike_group);
	wpabuf_put_buf(pfs->ie, pub);
	wpabuf_free(pub);
	wpa_hexdump_buf(MSG_DEBUG, "DPP: Diffie-Hellman Parameter element",
			pfs->ie);

	return pfs;
fail:
	wpabuf_free(pub);
	dpp_pfs_free(pfs);
	return NULL;
}


int dpp_pfs_process(struct dpp_pfs *pfs, const u8 *peer_ie, size_t peer_ie_len)
{
	if (peer_ie_len < 2)
		return -1;
	if (WPA_GET_LE16(peer_ie) != pfs->curve->ike_group) {
		wpa_printf(MSG_DEBUG, "DPP: Peer used different group for PFS");
		return -1;
	}

	pfs->secret = crypto_ecdh_set_peerkey(pfs->ecdh, 0, peer_ie + 2,
					      peer_ie_len - 2);
	pfs->secret = wpabuf_zeropad(pfs->secret, pfs->curve->prime_len);
	if (!pfs->secret) {
		wpa_printf(MSG_DEBUG, "DPP: Invalid peer DH public key");
		return -1;
	}
	wpa_hexdump_buf_key(MSG_DEBUG, "DPP: DH shared secret", pfs->secret);
	return 0;
}


void dpp_pfs_free(struct dpp_pfs *pfs)
{
	if (!pfs)
		return;
	crypto_ecdh_deinit(pfs->ecdh);
	wpabuf_free(pfs->ie);
	wpabuf_clear_free(pfs->secret);
	os_free(pfs);
}


struct wpabuf * dpp_build_csr(struct dpp_authentication *auth, const char *name)
{
	struct crypto_csr *csr = NULL;
	struct wpabuf *buf = NULL;
	struct crypto_ec_key *key;
	unsigned int hash_len = auth->curve->hash_len;
	struct wpabuf *priv_key;
	u8 cp[DPP_CP_LEN];
	char *password = NULL;
	size_t password_len;
	int hash_sign_algo;

	/* TODO: use auth->csrattrs */

	/* TODO: support generation of a new private key if csrAttrs requests
	 * a specific group to be used */
	key = auth->own_protocol_key;

	priv_key = crypto_ec_key_get_ecprivate_key(key, true);
	if (!priv_key)
		goto fail;
	wpabuf_free(auth->priv_key);
	auth->priv_key = priv_key;

	csr = crypto_csr_init();
	if (!csr || crypto_csr_set_ec_public_key(csr, key))
		goto fail;

	if (name && crypto_csr_set_name(csr, CSR_NAME_CN, name))
		goto fail;

	/* cp = HKDF-Expand(bk, "CSR challengePassword", 64) */
	if (dpp_hkdf_expand(hash_len, auth->bk, hash_len,
			    "CSR challengePassword", cp, DPP_CP_LEN) < 0)
		goto fail;
	wpa_hexdump_key(MSG_DEBUG,
			"DPP: cp = HKDF-Expand(bk, \"CSR challengePassword\", 64)",
			cp, DPP_CP_LEN);
	password = base64_encode_no_lf(cp, DPP_CP_LEN, &password_len);
	forced_memzero(cp, DPP_CP_LEN);
	if (!password ||
	    crypto_csr_set_attribute(csr, CSR_ATTR_CHALLENGE_PASSWORD,
				     ASN1_TAG_UTF8STRING, (const u8 *) password,
				     password_len))
		goto fail;

	/* TODO: hash func selection based on csrAttrs */
	if (hash_len == SHA256_MAC_LEN) {
		hash_sign_algo = CRYPTO_HASH_ALG_SHA256;
	} else if (hash_len == SHA384_MAC_LEN) {
		hash_sign_algo = CRYPTO_HASH_ALG_SHA384;
	} else if (hash_len == SHA512_MAC_LEN) {
		hash_sign_algo = CRYPTO_HASH_ALG_SHA512;
	} else {
		wpa_printf(MSG_DEBUG, "DPP: Unknown signature algorithm");
		goto fail;
	}

	buf = crypto_csr_sign(csr, key, hash_sign_algo);
	if (!buf)
		goto fail;
	wpa_hexdump_buf(MSG_DEBUG, "DPP: CSR", buf);

fail:
	bin_clear_free(password, password_len);
	crypto_csr_deinit(csr);
	return buf;
}


int dpp_validate_csr(struct dpp_authentication *auth,
		     const struct wpabuf *csrbuf)
{
	struct crypto_csr *csr;
	const u8 *attr;
	size_t attr_len;
	int attr_type;
	unsigned char *cp = NULL;
	size_t cp_len;
	u8 exp_cp[DPP_CP_LEN];
	unsigned int hash_len = auth->curve->hash_len;
	int ret = -1;

	csr = crypto_csr_verify(csrbuf);
	if (!csr) {
		wpa_printf(MSG_DEBUG,
			   "DPP: CSR invalid or invalid signature");
		goto fail;
	}

	attr = crypto_csr_get_attribute(csr, CSR_ATTR_CHALLENGE_PASSWORD,
					&attr_len, &attr_type);
	if (!attr) {
		wpa_printf(MSG_DEBUG,
			   "DPP: CSR does not include challengePassword");
		goto fail;
	}
	/* This is supposed to be UTF8String, but allow other strings as well
	 * since challengePassword is using ASCII (base64 encoded). */
	if (attr_type != ASN1_TAG_UTF8STRING &&
	    attr_type != ASN1_TAG_PRINTABLESTRING &&
	    attr_type != ASN1_TAG_IA5STRING) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unexpected challengePassword attribute type %d",
			   attr_type);
		goto fail;
	}

	cp = base64_decode((const char *) attr, attr_len, &cp_len);
	if (!cp) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Could not base64 decode challengePassword");
		goto fail;
	}
	if (cp_len != DPP_CP_LEN) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unexpected cp length (%zu) in CSR challengePassword",
			   cp_len);
		goto fail;
	}
	wpa_hexdump_key(MSG_DEBUG, "DPP: cp from CSR challengePassword",
			cp, cp_len);

	/* cp = HKDF-Expand(bk, "CSR challengePassword", 64) */
	if (dpp_hkdf_expand(hash_len, auth->bk, hash_len,
			    "CSR challengePassword", exp_cp, DPP_CP_LEN) < 0)
		goto fail;
	wpa_hexdump_key(MSG_DEBUG,
			"DPP: cp = HKDF-Expand(bk, \"CSR challengePassword\", 64)",
			exp_cp, DPP_CP_LEN);
	if (os_memcmp_const(cp, exp_cp, DPP_CP_LEN) != 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: CSR challengePassword does not match calculated cp");
		goto fail;
	}

	ret = 0;
fail:
	os_free(cp);
	crypto_csr_deinit(csr);
	return ret;
}


struct dpp_reconfig_id * dpp_gen_reconfig_id(const u8 *csign_key,
					     size_t csign_key_len,
					     const u8 *pp_key,
					     size_t pp_key_len)
{
	struct crypto_ec_key *csign = NULL, *ppkey = NULL;
	struct dpp_reconfig_id *id = NULL;
	struct crypto_ec *ec = NULL;
	const struct crypto_bignum *q;
	struct crypto_bignum *bn = NULL;
	struct crypto_ec_point *e_id = NULL;
	const struct crypto_ec_point *generator;

	csign = crypto_ec_key_parse_pub(csign_key, csign_key_len);
	if (!csign)
		goto fail;

	if (!pp_key)
		goto fail;
	ppkey = crypto_ec_key_parse_pub(pp_key, pp_key_len);
	if (!ppkey)
		goto fail;

	ec = crypto_ec_init(crypto_ec_key_group(csign));
	if (!ec)
		goto fail;

	e_id = crypto_ec_point_init(ec);
	bn = crypto_bignum_init();
	q = crypto_ec_get_order(ec);
	generator = crypto_ec_get_generator(ec);
	if (!e_id || !bn || !q || !generator ||
	    crypto_bignum_rand(bn, q) ||
	    crypto_ec_point_mul(ec, generator, bn, e_id))
		goto fail;

	crypto_ec_point_debug_print(ec, e_id,
				    "DPP: Generated random point E-id");

	id = os_zalloc(sizeof(*id));
	if (!id)
		goto fail;

	id->ec = ec;
	ec = NULL;
	id->e_id = e_id;
	e_id = NULL;
	id->csign = csign;
	csign = NULL;
	id->pp_key = ppkey;
	ppkey = NULL;
fail:
	crypto_ec_point_deinit(e_id, 1);
	crypto_ec_key_deinit(csign);
	crypto_ec_key_deinit(ppkey);
	crypto_bignum_deinit(bn, 1);
	crypto_ec_deinit(ec);
	return id;
}


int dpp_update_reconfig_id(struct dpp_reconfig_id *id)
{
	const struct crypto_bignum *q;
	struct crypto_bignum *bn;
	const struct crypto_ec_point *pp, *generator;
	struct crypto_ec_point *e_prime_id, *a_nonce;
	int ret = -1;

	pp = crypto_ec_key_get_public_key(id->pp_key);
	e_prime_id = crypto_ec_point_init(id->ec);
	a_nonce = crypto_ec_point_init(id->ec);
	bn = crypto_bignum_init();
	q = crypto_ec_get_order(id->ec);
	generator = crypto_ec_get_generator(id->ec);

	/* Generate random 0 <= a-nonce < q
	 * A-NONCE = a-nonce * G
	 * E'-id = E-id + a-nonce * P_pk */
	if (!pp || !e_prime_id || !a_nonce || !bn || !q || !generator ||
	    crypto_bignum_rand(bn, q) || /* bn = a-nonce */
	    crypto_ec_point_mul(id->ec, generator, bn, a_nonce) ||
	    crypto_ec_point_mul(id->ec, pp, bn, e_prime_id) ||
	    crypto_ec_point_add(id->ec, id->e_id, e_prime_id, e_prime_id))
		goto fail;

	crypto_ec_point_debug_print(id->ec, a_nonce,
				    "DPP: Generated A-NONCE");
	crypto_ec_point_debug_print(id->ec, e_prime_id,
				    "DPP: Encrypted E-id to E'-id");

	crypto_ec_key_deinit(id->a_nonce);
	crypto_ec_key_deinit(id->e_prime_id);
	id->a_nonce = crypto_ec_key_set_pub_point(id->ec, a_nonce);
	id->e_prime_id = crypto_ec_key_set_pub_point(id->ec, e_prime_id);
	if (!id->a_nonce || !id->e_prime_id)
		goto fail;

	ret = 0;

fail:
	crypto_ec_point_deinit(e_prime_id, 1);
	crypto_ec_point_deinit(a_nonce, 1);
	crypto_bignum_deinit(bn, 1);
	return ret;
}


void dpp_free_reconfig_id(struct dpp_reconfig_id *id)
{
	if (id) {
		crypto_ec_point_deinit(id->e_id, 1);
		crypto_ec_key_deinit(id->csign);
		crypto_ec_key_deinit(id->a_nonce);
		crypto_ec_key_deinit(id->e_prime_id);
		crypto_ec_key_deinit(id->pp_key);
		crypto_ec_deinit(id->ec);
		os_free(id);
	}
}


struct crypto_ec_point * dpp_decrypt_e_id(struct crypto_ec_key *ppkey,
					  struct crypto_ec_key *a_nonce,
					  struct crypto_ec_key *e_prime_id)
{
	struct crypto_ec *ec;
	const struct crypto_bignum *pp;
	struct crypto_ec_point *e_id = NULL;
	const struct crypto_ec_point *a_nonce_point, *e_prime_id_point;

	if (!ppkey)
		return NULL;

	/* E-id = E'-id - s_C * A-NONCE */
	ec = crypto_ec_init(crypto_ec_key_group(ppkey));
	if (!ec)
		return NULL;

	pp = crypto_ec_key_get_private_key(ppkey);
	a_nonce_point = crypto_ec_key_get_public_key(a_nonce);
	e_prime_id_point = crypto_ec_key_get_public_key(e_prime_id);
	e_id = crypto_ec_point_init(ec);
	if (!pp || !a_nonce_point || !e_prime_id_point || !e_id ||
	    crypto_ec_point_mul(ec, a_nonce_point, pp, e_id) ||
	    crypto_ec_point_invert(ec, e_id) ||
	    crypto_ec_point_add(ec, e_id, e_prime_id_point, e_id)) {
		crypto_ec_point_deinit(e_id, 1);
		goto fail;
	}

	crypto_ec_point_debug_print(ec, e_id, "DPP: Decrypted E-id");

fail:
	crypto_ec_deinit(ec);
	return e_id;
}

#endif /* CONFIG_DPP2 */


#ifdef CONFIG_TESTING_OPTIONS

int dpp_test_gen_invalid_key(struct wpabuf *msg,
			     const struct dpp_curve_params *curve)
{
	struct crypto_ec *ec;
	struct crypto_ec_key *key = NULL;
	const struct crypto_ec_point *pub_key;
	struct crypto_ec_point *p = NULL;
	u8 *x, *y;
	int ret = -1;

	ec = crypto_ec_init(curve->ike_group);
	x = wpabuf_put(msg, curve->prime_len);
	y = wpabuf_put(msg, curve->prime_len);
	if (!ec)
		goto fail;

retry:
	/* Generate valid key pair */
	key = crypto_ec_key_gen(curve->ike_group);
	if (!key)
		goto fail;

	/* Retrieve public key coordinates */
	pub_key = crypto_ec_key_get_public_key(key);
	if (!pub_key)
		goto fail;

	crypto_ec_point_to_bin(ec, pub_key, x, y);

	/* And corrupt them */
	y[curve->prime_len - 1] ^= 0x01;
	p = crypto_ec_point_from_bin(ec, x);
	if (p && crypto_ec_point_is_on_curve(ec, p)) {
		crypto_ec_point_deinit(p, 0);
		p = NULL;
		goto retry;
	}

	ret = 0;
fail:
	crypto_ec_point_deinit(p, 0);
	crypto_ec_key_deinit(key);
	crypto_ec_deinit(ec);
	return ret;
}


char * dpp_corrupt_connector_signature(const char *connector)
{
	char *tmp, *pos, *signed3 = NULL;
	unsigned char *signature = NULL;
	size_t signature_len = 0, signed3_len;

	tmp = os_zalloc(os_strlen(connector) + 5);
	if (!tmp)
		goto fail;
	os_memcpy(tmp, connector, os_strlen(connector));

	pos = os_strchr(tmp, '.');
	if (!pos)
		goto fail;

	pos = os_strchr(pos + 1, '.');
	if (!pos)
		goto fail;
	pos++;

	wpa_printf(MSG_DEBUG, "DPP: Original base64url encoded signature: %s",
		   pos);
	signature = base64_url_decode(pos, os_strlen(pos), &signature_len);
	if (!signature || signature_len == 0)
		goto fail;
	wpa_hexdump(MSG_DEBUG, "DPP: Original Connector signature",
		    signature, signature_len);
	signature[signature_len - 1] ^= 0x01;
	wpa_hexdump(MSG_DEBUG, "DPP: Corrupted Connector signature",
		    signature, signature_len);
	signed3 = base64_url_encode(signature, signature_len, &signed3_len);
	if (!signed3)
		goto fail;
	os_memcpy(pos, signed3, signed3_len);
	pos[signed3_len] = '\0';
	wpa_printf(MSG_DEBUG, "DPP: Corrupted base64url encoded signature: %s",
		   pos);

out:
	os_free(signature);
	os_free(signed3);
	return tmp;
fail:
	os_free(tmp);
	tmp = NULL;
	goto out;
}

#endif /* CONFIG_TESTING_OPTIONS */
