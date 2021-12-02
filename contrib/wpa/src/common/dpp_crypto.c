/*
 * DPP crypto functionality
 * Copyright (c) 2017, Qualcomm Atheros, Inc.
 * Copyright (c) 2018-2020, The Linux Foundation
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"
#include <openssl/opensslv.h>
#include <openssl/err.h>
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/pem.h>

#include "utils/common.h"
#include "utils/base64.h"
#include "utils/json.h"
#include "common/ieee802_11_defs.h"
#include "crypto/crypto.h"
#include "crypto/random.h"
#include "crypto/sha384.h"
#include "crypto/sha512.h"
#include "dpp.h"
#include "dpp_i.h"


#if OPENSSL_VERSION_NUMBER < 0x10100000L || \
	(defined(LIBRESSL_VERSION_NUMBER) && \
	 LIBRESSL_VERSION_NUMBER < 0x20700000L)
/* Compatibility wrappers for older versions. */

static int ECDSA_SIG_set0(ECDSA_SIG *sig, BIGNUM *r, BIGNUM *s)
{
	sig->r = r;
	sig->s = s;
	return 1;
}


static void ECDSA_SIG_get0(const ECDSA_SIG *sig, const BIGNUM **pr,
			   const BIGNUM **ps)
{
	if (pr)
		*pr = sig->r;
	if (ps)
		*ps = sig->s;
}


static EC_KEY * EVP_PKEY_get0_EC_KEY(EVP_PKEY *pkey)
{
	if (pkey->type != EVP_PKEY_EC)
		return NULL;
	return pkey->pkey.ec;
}

#endif

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


static const struct dpp_curve_params *
dpp_get_curve_oid(const ASN1_OBJECT *poid)
{
	ASN1_OBJECT *oid;
	int i;

	for (i = 0; dpp_curves[i].name; i++) {
		oid = OBJ_txt2obj(dpp_curves[i].name, 0);
		if (oid && OBJ_cmp(poid, oid) == 0)
			return &dpp_curves[i];
	}
	return NULL;
}


const struct dpp_curve_params * dpp_get_curve_nid(int nid)
{
	int i, tmp;

	if (!nid)
		return NULL;
	for (i = 0; dpp_curves[i].name; i++) {
		tmp = OBJ_txt2nid(dpp_curves[i].name);
		if (tmp == nid)
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


void dpp_debug_print_point(const char *title, const EC_GROUP *group,
			   const EC_POINT *point)
{
	BIGNUM *x, *y;
	BN_CTX *ctx;
	char *x_str = NULL, *y_str = NULL;

	if (!wpa_debug_show_keys)
		return;

	ctx = BN_CTX_new();
	x = BN_new();
	y = BN_new();
	if (!ctx || !x || !y ||
	    EC_POINT_get_affine_coordinates_GFp(group, point, x, y, ctx) != 1)
		goto fail;

	x_str = BN_bn2hex(x);
	y_str = BN_bn2hex(y);
	if (!x_str || !y_str)
		goto fail;

	wpa_printf(MSG_DEBUG, "%s (%s,%s)", title, x_str, y_str);

fail:
	OPENSSL_free(x_str);
	OPENSSL_free(y_str);
	BN_free(x);
	BN_free(y);
	BN_CTX_free(ctx);
}


void dpp_debug_print_key(const char *title, EVP_PKEY *key)
{
	EC_KEY *eckey;
	BIO *out;
	size_t rlen;
	char *txt;
	int res;
	unsigned char *der = NULL;
	int der_len;
	const EC_GROUP *group;
	const EC_POINT *point;

	out = BIO_new(BIO_s_mem());
	if (!out)
		return;

	EVP_PKEY_print_private(out, key, 0, NULL);
	rlen = BIO_ctrl_pending(out);
	txt = os_malloc(rlen + 1);
	if (txt) {
		res = BIO_read(out, txt, rlen);
		if (res > 0) {
			txt[res] = '\0';
			wpa_printf(MSG_DEBUG, "%s: %s", title, txt);
		}
		os_free(txt);
	}
	BIO_free(out);

	eckey = EVP_PKEY_get1_EC_KEY(key);
	if (!eckey)
		return;

	group = EC_KEY_get0_group(eckey);
	point = EC_KEY_get0_public_key(eckey);
	if (group && point)
		dpp_debug_print_point(title, group, point);

	der_len = i2d_ECPrivateKey(eckey, &der);
	if (der_len > 0)
		wpa_hexdump_key(MSG_DEBUG, "DPP: ECPrivateKey", der, der_len);
	OPENSSL_free(der);
	if (der_len <= 0) {
		der = NULL;
		der_len = i2d_EC_PUBKEY(eckey, &der);
		if (der_len > 0)
			wpa_hexdump(MSG_DEBUG, "DPP: EC_PUBKEY", der, der_len);
		OPENSSL_free(der);
	}

	EC_KEY_free(eckey);
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


int dpp_bn2bin_pad(const BIGNUM *bn, u8 *pos, size_t len)
{
	int num_bytes, offset;

	num_bytes = BN_num_bytes(bn);
	if ((size_t) num_bytes > len)
		return -1;
	offset = len - num_bytes;
	os_memset(pos, 0, offset);
	BN_bn2bin(bn, pos + offset);
	return 0;
}


struct wpabuf * dpp_get_pubkey_point(EVP_PKEY *pkey, int prefix)
{
	int len, res;
	EC_KEY *eckey;
	struct wpabuf *buf;
	unsigned char *pos;

	eckey = EVP_PKEY_get1_EC_KEY(pkey);
	if (!eckey)
		return NULL;
	EC_KEY_set_conv_form(eckey, POINT_CONVERSION_UNCOMPRESSED);
	len = i2o_ECPublicKey(eckey, NULL);
	if (len <= 0) {
		wpa_printf(MSG_ERROR,
			   "DDP: Failed to determine public key encoding length");
		EC_KEY_free(eckey);
		return NULL;
	}

	buf = wpabuf_alloc(len);
	if (!buf) {
		EC_KEY_free(eckey);
		return NULL;
	}

	pos = wpabuf_put(buf, len);
	res = i2o_ECPublicKey(eckey, &pos);
	EC_KEY_free(eckey);
	if (res != len) {
		wpa_printf(MSG_ERROR,
			   "DDP: Failed to encode public key (res=%d/%d)",
			   res, len);
		wpabuf_free(buf);
		return NULL;
	}

	if (!prefix) {
		/* Remove 0x04 prefix to match DPP definition */
		pos = wpabuf_mhead(buf);
		os_memmove(pos, pos + 1, len - 1);
		buf->used--;
	}

	return buf;
}


EVP_PKEY * dpp_set_pubkey_point_group(const EC_GROUP *group,
				      const u8 *buf_x, const u8 *buf_y,
				      size_t len)
{
	EC_KEY *eckey = NULL;
	BN_CTX *ctx;
	EC_POINT *point = NULL;
	BIGNUM *x = NULL, *y = NULL;
	EVP_PKEY *pkey = NULL;

	ctx = BN_CTX_new();
	if (!ctx) {
		wpa_printf(MSG_ERROR, "DPP: Out of memory");
		return NULL;
	}

	point = EC_POINT_new(group);
	x = BN_bin2bn(buf_x, len, NULL);
	y = BN_bin2bn(buf_y, len, NULL);
	if (!point || !x || !y) {
		wpa_printf(MSG_ERROR, "DPP: Out of memory");
		goto fail;
	}

	if (!EC_POINT_set_affine_coordinates_GFp(group, point, x, y, ctx)) {
		wpa_printf(MSG_ERROR,
			   "DPP: OpenSSL: EC_POINT_set_affine_coordinates_GFp failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	if (!EC_POINT_is_on_curve(group, point, ctx) ||
	    EC_POINT_is_at_infinity(group, point)) {
		wpa_printf(MSG_ERROR, "DPP: Invalid point");
		goto fail;
	}
	dpp_debug_print_point("DPP: dpp_set_pubkey_point_group", group, point);

	eckey = EC_KEY_new();
	if (!eckey ||
	    EC_KEY_set_group(eckey, group) != 1 ||
	    EC_KEY_set_public_key(eckey, point) != 1) {
		wpa_printf(MSG_ERROR,
			   "DPP: Failed to set EC_KEY: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
	EC_KEY_set_asn1_flag(eckey, OPENSSL_EC_NAMED_CURVE);

	pkey = EVP_PKEY_new();
	if (!pkey || EVP_PKEY_set1_EC_KEY(pkey, eckey) != 1) {
		wpa_printf(MSG_ERROR, "DPP: Could not create EVP_PKEY");
		goto fail;
	}

out:
	BN_free(x);
	BN_free(y);
	EC_KEY_free(eckey);
	EC_POINT_free(point);
	BN_CTX_free(ctx);
	return pkey;
fail:
	EVP_PKEY_free(pkey);
	pkey = NULL;
	goto out;
}


EVP_PKEY * dpp_set_pubkey_point(EVP_PKEY *group_key, const u8 *buf, size_t len)
{
	const EC_KEY *eckey;
	const EC_GROUP *group;
	EVP_PKEY *pkey = NULL;

	if (len & 1)
		return NULL;

	eckey = EVP_PKEY_get0_EC_KEY(group_key);
	if (!eckey) {
		wpa_printf(MSG_ERROR,
			   "DPP: Could not get EC_KEY from group_key");
		return NULL;
	}

	group = EC_KEY_get0_group(eckey);
	if (group)
		pkey = dpp_set_pubkey_point_group(group, buf, buf + len / 2,
						  len / 2);
	else
		wpa_printf(MSG_ERROR, "DPP: Could not get EC group");

	return pkey;
}


EVP_PKEY * dpp_gen_keypair(const struct dpp_curve_params *curve)
{
	EVP_PKEY_CTX *kctx = NULL;
	EC_KEY *ec_params = NULL;
	EVP_PKEY *params = NULL, *key = NULL;
	int nid;

	wpa_printf(MSG_DEBUG, "DPP: Generating a keypair");

	nid = OBJ_txt2nid(curve->name);
	if (nid == NID_undef) {
		wpa_printf(MSG_INFO, "DPP: Unsupported curve %s", curve->name);
		return NULL;
	}

	ec_params = EC_KEY_new_by_curve_name(nid);
	if (!ec_params) {
		wpa_printf(MSG_ERROR,
			   "DPP: Failed to generate EC_KEY parameters");
		goto fail;
	}
	EC_KEY_set_asn1_flag(ec_params, OPENSSL_EC_NAMED_CURVE);
	params = EVP_PKEY_new();
	if (!params || EVP_PKEY_set1_EC_KEY(params, ec_params) != 1) {
		wpa_printf(MSG_ERROR,
			   "DPP: Failed to generate EVP_PKEY parameters");
		goto fail;
	}

	kctx = EVP_PKEY_CTX_new(params, NULL);
	if (!kctx ||
	    EVP_PKEY_keygen_init(kctx) != 1 ||
	    EVP_PKEY_keygen(kctx, &key) != 1) {
		wpa_printf(MSG_ERROR, "DPP: Failed to generate EC key");
		key = NULL;
		goto fail;
	}

	if (wpa_debug_show_keys)
		dpp_debug_print_key("Own generated key", key);

fail:
	EC_KEY_free(ec_params);
	EVP_PKEY_free(params);
	EVP_PKEY_CTX_free(kctx);
	return key;
}


EVP_PKEY * dpp_set_keypair(const struct dpp_curve_params **curve,
			   const u8 *privkey, size_t privkey_len)
{
	EVP_PKEY *pkey;
	EC_KEY *eckey;
	const EC_GROUP *group;
	int nid;

	pkey = EVP_PKEY_new();
	if (!pkey)
		return NULL;
	eckey = d2i_ECPrivateKey(NULL, &privkey, privkey_len);
	if (!eckey) {
		wpa_printf(MSG_INFO,
			   "DPP: OpenSSL: d2i_ECPrivateKey() failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		EVP_PKEY_free(pkey);
		return NULL;
	}
	group = EC_KEY_get0_group(eckey);
	if (!group) {
		EC_KEY_free(eckey);
		EVP_PKEY_free(pkey);
		return NULL;
	}
	nid = EC_GROUP_get_curve_name(group);
	*curve = dpp_get_curve_nid(nid);
	if (!*curve) {
		wpa_printf(MSG_INFO,
			   "DPP: Unsupported curve (nid=%d) in pre-assigned key",
			   nid);
		EC_KEY_free(eckey);
		EVP_PKEY_free(pkey);
		return NULL;
	}

	if (EVP_PKEY_assign_EC_KEY(pkey, eckey) != 1) {
		EC_KEY_free(eckey);
		EVP_PKEY_free(pkey);
		return NULL;
	}
	return pkey;
}


typedef struct {
	/* AlgorithmIdentifier ecPublicKey with optional parameters present
	 * as an OID identifying the curve */
	X509_ALGOR *alg;
	/* Compressed format public key per ANSI X9.63 */
	ASN1_BIT_STRING *pub_key;
} DPP_BOOTSTRAPPING_KEY;

ASN1_SEQUENCE(DPP_BOOTSTRAPPING_KEY) = {
	ASN1_SIMPLE(DPP_BOOTSTRAPPING_KEY, alg, X509_ALGOR),
	ASN1_SIMPLE(DPP_BOOTSTRAPPING_KEY, pub_key, ASN1_BIT_STRING)
} ASN1_SEQUENCE_END(DPP_BOOTSTRAPPING_KEY);

IMPLEMENT_ASN1_FUNCTIONS(DPP_BOOTSTRAPPING_KEY);


static struct wpabuf * dpp_bootstrap_key_der(EVP_PKEY *key)
{
	unsigned char *der = NULL;
	int der_len;
	const EC_KEY *eckey;
	struct wpabuf *ret = NULL;
	size_t len;
	const EC_GROUP *group;
	const EC_POINT *point;
	BN_CTX *ctx;
	DPP_BOOTSTRAPPING_KEY *bootstrap = NULL;
	int nid;

	ctx = BN_CTX_new();
	eckey = EVP_PKEY_get0_EC_KEY(key);
	if (!ctx || !eckey)
		goto fail;

	group = EC_KEY_get0_group(eckey);
	point = EC_KEY_get0_public_key(eckey);
	if (!group || !point)
		goto fail;
	dpp_debug_print_point("DPP: bootstrap public key", group, point);
	nid = EC_GROUP_get_curve_name(group);

	bootstrap = DPP_BOOTSTRAPPING_KEY_new();
	if (!bootstrap ||
	    X509_ALGOR_set0(bootstrap->alg, OBJ_nid2obj(EVP_PKEY_EC),
			    V_ASN1_OBJECT, (void *) OBJ_nid2obj(nid)) != 1)
		goto fail;

	len = EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED,
				 NULL, 0, ctx);
	if (len == 0)
		goto fail;

	der = OPENSSL_malloc(len);
	if (!der)
		goto fail;
	len = EC_POINT_point2oct(group, point, POINT_CONVERSION_COMPRESSED,
				 der, len, ctx);

	OPENSSL_free(bootstrap->pub_key->data);
	bootstrap->pub_key->data = der;
	der = NULL;
	bootstrap->pub_key->length = len;
	/* No unused bits */
	bootstrap->pub_key->flags &= ~(ASN1_STRING_FLAG_BITS_LEFT | 0x07);
	bootstrap->pub_key->flags |= ASN1_STRING_FLAG_BITS_LEFT;

	der_len = i2d_DPP_BOOTSTRAPPING_KEY(bootstrap, &der);
	if (der_len <= 0) {
		wpa_printf(MSG_ERROR,
			   "DDP: Failed to build DER encoded public key");
		goto fail;
	}

	ret = wpabuf_alloc_copy(der, der_len);
fail:
	DPP_BOOTSTRAPPING_KEY_free(bootstrap);
	OPENSSL_free(der);
	BN_CTX_free(ctx);
	return ret;
}


int dpp_bootstrap_key_hash(struct dpp_bootstrap_info *bi)
{
	struct wpabuf *der;
	int res;

	der = dpp_bootstrap_key_der(bi->pubkey);
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

	der = dpp_bootstrap_key_der(bi->pubkey);
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


int dpp_ecdh(EVP_PKEY *own, EVP_PKEY *peer, u8 *secret, size_t *secret_len)
{
	EVP_PKEY_CTX *ctx;
	int ret = -1;

	ERR_clear_error();
	*secret_len = 0;

	ctx = EVP_PKEY_CTX_new(own, NULL);
	if (!ctx) {
		wpa_printf(MSG_ERROR, "DPP: EVP_PKEY_CTX_new failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		return -1;
	}

	if (EVP_PKEY_derive_init(ctx) != 1) {
		wpa_printf(MSG_ERROR, "DPP: EVP_PKEY_derive_init failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	if (EVP_PKEY_derive_set_peer(ctx, peer) != 1) {
		wpa_printf(MSG_ERROR,
			   "DPP: EVP_PKEY_derive_set_peet failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	if (EVP_PKEY_derive(ctx, NULL, secret_len) != 1) {
		wpa_printf(MSG_ERROR, "DPP: EVP_PKEY_derive(NULL) failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	if (*secret_len > DPP_MAX_SHARED_SECRET_LEN) {
		u8 buf[200];
		int level = *secret_len > 200 ? MSG_ERROR : MSG_DEBUG;

		/* It looks like OpenSSL can return unexpectedly large buffer
		 * need for shared secret from EVP_PKEY_derive(NULL) in some
		 * cases. For example, group 19 has shown cases where secret_len
		 * is set to 72 even though the actual length ends up being
		 * updated to 32 when EVP_PKEY_derive() is called with a buffer
		 * for the value. Work around this by trying to fetch the value
		 * and continue if it is within supported range even when the
		 * initial buffer need is claimed to be larger. */
		wpa_printf(level,
			   "DPP: Unexpected secret_len=%d from EVP_PKEY_derive()",
			   (int) *secret_len);
		if (*secret_len > 200)
			goto fail;
		if (EVP_PKEY_derive(ctx, buf, secret_len) != 1) {
			wpa_printf(MSG_ERROR, "DPP: EVP_PKEY_derive failed: %s",
				   ERR_error_string(ERR_get_error(), NULL));
			goto fail;
		}
		if (*secret_len > DPP_MAX_SHARED_SECRET_LEN) {
			wpa_printf(MSG_ERROR,
				   "DPP: Unexpected secret_len=%d from EVP_PKEY_derive()",
				   (int) *secret_len);
			goto fail;
		}
		wpa_hexdump_key(MSG_DEBUG, "DPP: Unexpected secret_len change",
				buf, *secret_len);
		os_memcpy(secret, buf, *secret_len);
		forced_memzero(buf, sizeof(buf));
		goto done;
	}

	if (EVP_PKEY_derive(ctx, secret, secret_len) != 1) {
		wpa_printf(MSG_ERROR, "DPP: EVP_PKEY_derive failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

done:
	ret = 0;

fail:
	EVP_PKEY_CTX_free(ctx);
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
	EVP_PKEY *pkey;
	const unsigned char *p;
	int res;
	X509_PUBKEY *pub = NULL;
	ASN1_OBJECT *ppkalg;
	const unsigned char *pk;
	int ppklen;
	X509_ALGOR *pa;
#if OPENSSL_VERSION_NUMBER < 0x10100000L || \
	(defined(LIBRESSL_VERSION_NUMBER) && \
	 LIBRESSL_VERSION_NUMBER < 0x20800000L)
	ASN1_OBJECT *pa_oid;
#else
	const ASN1_OBJECT *pa_oid;
#endif
	const void *pval;
	int ptype;
	const ASN1_OBJECT *poid;
	char buf[100];

	if (dpp_bi_pubkey_hash(bi, data, data_len) < 0) {
		wpa_printf(MSG_DEBUG, "DPP: Failed to hash public key");
		return -1;
	}

	/* DER encoded ASN.1 SubjectPublicKeyInfo
	 *
	 * SubjectPublicKeyInfo  ::=  SEQUENCE  {
	 *      algorithm            AlgorithmIdentifier,
	 *      subjectPublicKey     BIT STRING  }
	 *
	 * AlgorithmIdentifier  ::=  SEQUENCE  {
	 *      algorithm               OBJECT IDENTIFIER,
	 *      parameters              ANY DEFINED BY algorithm OPTIONAL  }
	 *
	 * subjectPublicKey = compressed format public key per ANSI X9.63
	 * algorithm = ecPublicKey (1.2.840.10045.2.1)
	 * parameters = shall be present and shall be OBJECT IDENTIFIER; e.g.,
	 *       prime256v1 (1.2.840.10045.3.1.7)
	 */

	p = data;
	pkey = d2i_PUBKEY(NULL, &p, data_len);

	if (!pkey) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Could not parse URI public-key SubjectPublicKeyInfo");
		return -1;
	}

	if (EVP_PKEY_type(EVP_PKEY_id(pkey)) != EVP_PKEY_EC) {
		wpa_printf(MSG_DEBUG,
			   "DPP: SubjectPublicKeyInfo does not describe an EC key");
		EVP_PKEY_free(pkey);
		return -1;
	}

	res = X509_PUBKEY_set(&pub, pkey);
	if (res != 1) {
		wpa_printf(MSG_DEBUG, "DPP: Could not set pubkey");
		goto fail;
	}

	res = X509_PUBKEY_get0_param(&ppkalg, &pk, &ppklen, &pa, pub);
	if (res != 1) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Could not extract SubjectPublicKeyInfo parameters");
		goto fail;
	}
	res = OBJ_obj2txt(buf, sizeof(buf), ppkalg, 0);
	if (res < 0 || (size_t) res >= sizeof(buf)) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Could not extract SubjectPublicKeyInfo algorithm");
		goto fail;
	}
	wpa_printf(MSG_DEBUG, "DPP: URI subjectPublicKey algorithm: %s", buf);
	if (os_strcmp(buf, "id-ecPublicKey") != 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unsupported SubjectPublicKeyInfo algorithm");
		goto fail;
	}

	X509_ALGOR_get0(&pa_oid, &ptype, (void *) &pval, pa);
	if (ptype != V_ASN1_OBJECT) {
		wpa_printf(MSG_DEBUG,
			   "DPP: SubjectPublicKeyInfo parameters did not contain an OID");
		goto fail;
	}
	poid = pval;
	res = OBJ_obj2txt(buf, sizeof(buf), poid, 0);
	if (res < 0 || (size_t) res >= sizeof(buf)) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Could not extract SubjectPublicKeyInfo parameters OID");
		goto fail;
	}
	wpa_printf(MSG_DEBUG, "DPP: URI subjectPublicKey parameters: %s", buf);
	bi->curve = dpp_get_curve_oid(poid);
	if (!bi->curve) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unsupported SubjectPublicKeyInfo curve: %s",
			   buf);
		goto fail;
	}

	wpa_hexdump(MSG_DEBUG, "DPP: URI subjectPublicKey", pk, ppklen);

	X509_PUBKEY_free(pub);
	bi->pubkey = pkey;
	return 0;
fail:
	X509_PUBKEY_free(pub);
	EVP_PKEY_free(pkey);
	return -1;
}


static struct wpabuf *
dpp_parse_jws_prot_hdr(const struct dpp_curve_params *curve,
		       const u8 *prot_hdr, u16 prot_hdr_len,
		       const EVP_MD **ret_md)
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
	    os_strcmp(token->string, "BS256") == 0)
		*ret_md = EVP_sha256();
	else if (os_strcmp(token->string, "ES384") == 0 ||
		 os_strcmp(token->string, "BS384") == 0)
		*ret_md = EVP_sha384();
	else if (os_strcmp(token->string, "ES512") == 0 ||
		 os_strcmp(token->string, "BS512") == 0)
		*ret_md = EVP_sha512();
	else
		*ret_md = NULL;
	if (!*ret_md) {
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


static int dpp_check_pubkey_match(EVP_PKEY *pub, struct wpabuf *r_hash)
{
	struct wpabuf *uncomp;
	int res;
	u8 hash[SHA256_MAC_LEN];
	const u8 *addr[1];
	size_t len[1];

	if (wpabuf_len(r_hash) != SHA256_MAC_LEN)
		return -1;
	uncomp = dpp_get_pubkey_point(pub, 1);
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
			     EVP_PKEY *csign_pub, const char *connector)
{
	enum dpp_status_error ret = 255;
	const char *pos, *end, *signed_start, *signed_end;
	struct wpabuf *kid = NULL;
	unsigned char *prot_hdr = NULL, *signature = NULL;
	size_t prot_hdr_len = 0, signature_len = 0;
	const EVP_MD *sign_md = NULL;
	unsigned char *der = NULL;
	int der_len;
	int res;
	EVP_MD_CTX *md_ctx = NULL;
	ECDSA_SIG *sig = NULL;
	BIGNUM *r = NULL, *s = NULL;
	const struct dpp_curve_params *curve;
	const EC_KEY *eckey;
	const EC_GROUP *group;
	int nid;

	eckey = EVP_PKEY_get0_EC_KEY(csign_pub);
	if (!eckey)
		goto fail;
	group = EC_KEY_get0_group(eckey);
	if (!group)
		goto fail;
	nid = EC_GROUP_get_curve_name(group);
	curve = dpp_get_curve_nid(nid);
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
	kid = dpp_parse_jws_prot_hdr(curve, prot_hdr, prot_hdr_len, &sign_md);
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

	/* JWS Signature encodes the signature (r,s) as two octet strings. Need
	 * to convert that to DER encoded ECDSA_SIG for OpenSSL EVP routines. */
	r = BN_bin2bn(signature, signature_len / 2, NULL);
	s = BN_bin2bn(signature + signature_len / 2, signature_len / 2, NULL);
	sig = ECDSA_SIG_new();
	if (!r || !s || !sig || ECDSA_SIG_set0(sig, r, s) != 1)
		goto fail;
	r = NULL;
	s = NULL;

	der_len = i2d_ECDSA_SIG(sig, &der);
	if (der_len <= 0) {
		wpa_printf(MSG_DEBUG, "DPP: Could not DER encode signature");
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: DER encoded signature", der, der_len);
	md_ctx = EVP_MD_CTX_create();
	if (!md_ctx)
		goto fail;

	ERR_clear_error();
	if (EVP_DigestVerifyInit(md_ctx, NULL, sign_md, NULL, csign_pub) != 1) {
		wpa_printf(MSG_DEBUG, "DPP: EVP_DigestVerifyInit failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
	if (EVP_DigestVerifyUpdate(md_ctx, signed_start,
				   signed_end - signed_start + 1) != 1) {
		wpa_printf(MSG_DEBUG, "DPP: EVP_DigestVerifyUpdate failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
	res = EVP_DigestVerifyFinal(md_ctx, der, der_len);
	if (res != 1) {
		wpa_printf(MSG_DEBUG,
			   "DPP: EVP_DigestVerifyFinal failed (res=%d): %s",
			   res, ERR_error_string(ERR_get_error(), NULL));
		ret = DPP_STATUS_INVALID_CONNECTOR;
		goto fail;
	}

	ret = DPP_STATUS_OK;
fail:
	EVP_MD_CTX_destroy(md_ctx);
	os_free(prot_hdr);
	wpabuf_free(kid);
	os_free(signature);
	ECDSA_SIG_free(sig);
	BN_free(r);
	BN_free(s);
	OPENSSL_free(der);
	return ret;
}


enum dpp_status_error
dpp_check_signed_connector(struct dpp_signed_connector_info *info,
			   const u8 *csign_key, size_t csign_key_len,
			   const u8 *peer_connector, size_t peer_connector_len)
{
	const unsigned char *p;
	EVP_PKEY *csign = NULL;
	char *signed_connector = NULL;
	enum dpp_status_error res = DPP_STATUS_INVALID_CONNECTOR;

	p = csign_key;
	csign = d2i_PUBKEY(NULL, &p, csign_key_len);
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
	EVP_PKEY_free(csign);
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
		pix = dpp_get_pubkey_point(auth->own_protocol_key, 0);
		prx = dpp_get_pubkey_point(auth->peer_protocol_key, 0);
		if (auth->own_bi)
			bix = dpp_get_pubkey_point(auth->own_bi->pubkey, 0);
		else
			bix = NULL;
		brx = dpp_get_pubkey_point(auth->peer_bi->pubkey, 0);
	} else {
		pix = dpp_get_pubkey_point(auth->peer_protocol_key, 0);
		prx = dpp_get_pubkey_point(auth->own_protocol_key, 0);
		if (auth->peer_bi)
			bix = dpp_get_pubkey_point(auth->peer_bi->pubkey, 0);
		else
			bix = NULL;
		brx = dpp_get_pubkey_point(auth->own_bi->pubkey, 0);
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
		pix = dpp_get_pubkey_point(auth->own_protocol_key, 0);
		prx = dpp_get_pubkey_point(auth->peer_protocol_key, 0);
		if (auth->own_bi)
			bix = dpp_get_pubkey_point(auth->own_bi->pubkey, 0);
		else
			bix = NULL;
		if (!auth->peer_bi)
			goto fail;
		brx = dpp_get_pubkey_point(auth->peer_bi->pubkey, 0);
	} else {
		pix = dpp_get_pubkey_point(auth->peer_protocol_key, 0);
		prx = dpp_get_pubkey_point(auth->own_protocol_key, 0);
		if (auth->peer_bi)
			bix = dpp_get_pubkey_point(auth->peer_bi->pubkey, 0);
		else
			bix = NULL;
		if (!auth->own_bi)
			goto fail;
		brx = dpp_get_pubkey_point(auth->own_bi->pubkey, 0);
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
	const EC_GROUP *group;
	EC_POINT *l = NULL;
	const EC_KEY *BI, *bR, *pR;
	const EC_POINT *BI_point;
	BN_CTX *bnctx;
	BIGNUM *lx, *sum, *q;
	const BIGNUM *bR_bn, *pR_bn;
	int ret = -1;

	/* L = ((bR + pR) modulo q) * BI */

	bnctx = BN_CTX_new();
	sum = BN_new();
	q = BN_new();
	lx = BN_new();
	if (!bnctx || !sum || !q || !lx)
		goto fail;
	BI = EVP_PKEY_get0_EC_KEY(auth->peer_bi->pubkey);
	if (!BI)
		goto fail;
	BI_point = EC_KEY_get0_public_key(BI);
	group = EC_KEY_get0_group(BI);
	if (!group)
		goto fail;

	bR = EVP_PKEY_get0_EC_KEY(auth->own_bi->pubkey);
	pR = EVP_PKEY_get0_EC_KEY(auth->own_protocol_key);
	if (!bR || !pR)
		goto fail;
	bR_bn = EC_KEY_get0_private_key(bR);
	pR_bn = EC_KEY_get0_private_key(pR);
	if (!bR_bn || !pR_bn)
		goto fail;
	if (EC_GROUP_get_order(group, q, bnctx) != 1 ||
	    BN_mod_add(sum, bR_bn, pR_bn, q, bnctx) != 1)
		goto fail;
	l = EC_POINT_new(group);
	if (!l ||
	    EC_POINT_mul(group, l, NULL, BI_point, sum, bnctx) != 1 ||
	    EC_POINT_get_affine_coordinates_GFp(group, l, lx, NULL,
						bnctx) != 1) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	if (dpp_bn2bin_pad(lx, auth->Lx, auth->secret_len) < 0)
		goto fail;
	wpa_hexdump_key(MSG_DEBUG, "DPP: L.x", auth->Lx, auth->secret_len);
	auth->Lx_len = auth->secret_len;
	ret = 0;
fail:
	EC_POINT_clear_free(l);
	BN_clear_free(lx);
	BN_clear_free(sum);
	BN_free(q);
	BN_CTX_free(bnctx);
	return ret;
}


int dpp_auth_derive_l_initiator(struct dpp_authentication *auth)
{
	const EC_GROUP *group;
	EC_POINT *l = NULL, *sum = NULL;
	const EC_KEY *bI, *BR, *PR;
	const EC_POINT *BR_point, *PR_point;
	BN_CTX *bnctx;
	BIGNUM *lx;
	const BIGNUM *bI_bn;
	int ret = -1;

	/* L = bI * (BR + PR) */

	bnctx = BN_CTX_new();
	lx = BN_new();
	if (!bnctx || !lx)
		goto fail;
	BR = EVP_PKEY_get0_EC_KEY(auth->peer_bi->pubkey);
	PR = EVP_PKEY_get0_EC_KEY(auth->peer_protocol_key);
	if (!BR || !PR)
		goto fail;
	BR_point = EC_KEY_get0_public_key(BR);
	PR_point = EC_KEY_get0_public_key(PR);

	bI = EVP_PKEY_get0_EC_KEY(auth->own_bi->pubkey);
	if (!bI)
		goto fail;
	group = EC_KEY_get0_group(bI);
	bI_bn = EC_KEY_get0_private_key(bI);
	if (!group || !bI_bn)
		goto fail;
	sum = EC_POINT_new(group);
	l = EC_POINT_new(group);
	if (!sum || !l ||
	    EC_POINT_add(group, sum, BR_point, PR_point, bnctx) != 1 ||
	    EC_POINT_mul(group, l, NULL, sum, bI_bn, bnctx) != 1 ||
	    EC_POINT_get_affine_coordinates_GFp(group, l, lx, NULL,
						bnctx) != 1) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	if (dpp_bn2bin_pad(lx, auth->Lx, auth->secret_len) < 0)
		goto fail;
	wpa_hexdump_key(MSG_DEBUG, "DPP: L.x", auth->Lx, auth->secret_len);
	auth->Lx_len = auth->secret_len;
	ret = 0;
fail:
	EC_POINT_clear_free(l);
	EC_POINT_clear_free(sum);
	BN_clear_free(lx);
	BN_CTX_free(bnctx);
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
		     EVP_PKEY *own_key, EVP_PKEY *peer_key, u8 *pmkid)
{
	struct wpabuf *nkx, *pkx;
	int ret = -1, res;
	const u8 *addr[2];
	size_t len[2];
	u8 hash[SHA256_MAC_LEN];

	/* PMKID = Truncate-128(H(min(NK.x, PK.x) | max(NK.x, PK.x))) */
	nkx = dpp_get_pubkey_point(own_key, 0);
	pkx = dpp_get_pubkey_point(peer_key, 0);
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


static EVP_PKEY * dpp_pkex_get_role_elem(const struct dpp_curve_params *curve,
					 int init)
{
	EC_GROUP *group;
	size_t len = curve->prime_len;
	const u8 *x, *y;
	EVP_PKEY *res;

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

	group = EC_GROUP_new_by_curve_name(OBJ_txt2nid(curve->name));
	if (!group)
		return NULL;
	res = dpp_set_pubkey_point_group(group, x, y, len);
	EC_GROUP_free(group);
	return res;
}


EC_POINT * dpp_pkex_derive_Qi(const struct dpp_curve_params *curve,
			      const u8 *mac_init, const char *code,
			      const char *identifier, BN_CTX *bnctx,
			      EC_GROUP **ret_group)
{
	u8 hash[DPP_MAX_HASH_LEN];
	const u8 *addr[3];
	size_t len[3];
	unsigned int num_elem = 0;
	EC_POINT *Qi = NULL;
	EVP_PKEY *Pi = NULL;
	const EC_KEY *Pi_ec;
	const EC_POINT *Pi_point;
	BIGNUM *hash_bn = NULL;
	const EC_GROUP *group = NULL;
	EC_GROUP *group2 = NULL;

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
	Pi = dpp_pkex_get_role_elem(curve, 1);
	if (!Pi)
		goto fail;
	dpp_debug_print_key("DPP: Pi", Pi);
	Pi_ec = EVP_PKEY_get0_EC_KEY(Pi);
	if (!Pi_ec)
		goto fail;
	Pi_point = EC_KEY_get0_public_key(Pi_ec);

	group = EC_KEY_get0_group(Pi_ec);
	if (!group)
		goto fail;
	group2 = EC_GROUP_dup(group);
	if (!group2)
		goto fail;
	Qi = EC_POINT_new(group2);
	if (!Qi) {
		EC_GROUP_free(group2);
		goto fail;
	}
	hash_bn = BN_bin2bn(hash, curve->hash_len, NULL);
	if (!hash_bn ||
	    EC_POINT_mul(group2, Qi, NULL, Pi_point, hash_bn, bnctx) != 1)
		goto fail;
	if (EC_POINT_is_at_infinity(group, Qi)) {
		wpa_printf(MSG_INFO, "DPP: Qi is the point-at-infinity");
		goto fail;
	}
	dpp_debug_print_point("DPP: Qi", group, Qi);
out:
	EVP_PKEY_free(Pi);
	BN_clear_free(hash_bn);
	if (ret_group && Qi)
		*ret_group = group2;
	else
		EC_GROUP_free(group2);
	return Qi;
fail:
	EC_POINT_free(Qi);
	Qi = NULL;
	goto out;
}


EC_POINT * dpp_pkex_derive_Qr(const struct dpp_curve_params *curve,
			      const u8 *mac_resp, const char *code,
			      const char *identifier, BN_CTX *bnctx,
			      EC_GROUP **ret_group)
{
	u8 hash[DPP_MAX_HASH_LEN];
	const u8 *addr[3];
	size_t len[3];
	unsigned int num_elem = 0;
	EC_POINT *Qr = NULL;
	EVP_PKEY *Pr = NULL;
	const EC_KEY *Pr_ec;
	const EC_POINT *Pr_point;
	BIGNUM *hash_bn = NULL;
	const EC_GROUP *group = NULL;
	EC_GROUP *group2 = NULL;

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
	Pr = dpp_pkex_get_role_elem(curve, 0);
	if (!Pr)
		goto fail;
	dpp_debug_print_key("DPP: Pr", Pr);
	Pr_ec = EVP_PKEY_get0_EC_KEY(Pr);
	if (!Pr_ec)
		goto fail;
	Pr_point = EC_KEY_get0_public_key(Pr_ec);

	group = EC_KEY_get0_group(Pr_ec);
	if (!group)
		goto fail;
	group2 = EC_GROUP_dup(group);
	if (!group2)
		goto fail;
	Qr = EC_POINT_new(group2);
	if (!Qr) {
		EC_GROUP_free(group2);
		goto fail;
	}
	hash_bn = BN_bin2bn(hash, curve->hash_len, NULL);
	if (!hash_bn ||
	    EC_POINT_mul(group2, Qr, NULL, Pr_point, hash_bn, bnctx) != 1)
		goto fail;
	if (EC_POINT_is_at_infinity(group, Qr)) {
		wpa_printf(MSG_INFO, "DPP: Qr is the point-at-infinity");
		goto fail;
	}
	dpp_debug_print_point("DPP: Qr", group, Qr);
out:
	EVP_PKEY_free(Pr);
	BN_clear_free(hash_bn);
	if (ret_group && Qr)
		*ret_group = group2;
	else
		EC_GROUP_free(group2);
	return Qr;
fail:
	EC_POINT_free(Qr);
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
	BN_CTX *bnctx = NULL;
	EVP_PKEY *own_key = NULL, *peer_key = NULL;
	BIGNUM *sum = NULL, *q = NULL, *mx = NULL;
	EC_POINT *m = NULL;
	const EC_KEY *cR, *pR;
	const EC_GROUP *group;
	const BIGNUM *cR_bn, *pR_bn;
	const EC_POINT *CI_point;
	const EC_KEY *CI;
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
	cR = EVP_PKEY_get0_EC_KEY(own_key);
	pR = EVP_PKEY_get0_EC_KEY(auth->own_protocol_key);
	if (!pR)
		goto fail;
	group = EC_KEY_get0_group(pR);
	bnctx = BN_CTX_new();
	sum = BN_new();
	mx = BN_new();
	q = BN_new();
	m = EC_POINT_new(group);
	if (!cR || !bnctx || !sum || !mx || !q || !m)
		goto fail;
	cR_bn = EC_KEY_get0_private_key(cR);
	pR_bn = EC_KEY_get0_private_key(pR);
	if (!cR_bn || !pR_bn)
		goto fail;
	CI = EVP_PKEY_get0_EC_KEY(peer_key);
	CI_point = EC_KEY_get0_public_key(CI);
	if (EC_GROUP_get_order(group, q, bnctx) != 1 ||
	    BN_mod_add(sum, cR_bn, pR_bn, q, bnctx) != 1 ||
	    EC_POINT_mul(group, m, NULL, CI_point, sum, bnctx) != 1 ||
	    EC_POINT_get_affine_coordinates_GFp(group, m, mx, NULL,
						bnctx) != 1) {
		wpa_printf(MSG_ERROR,
			   "OpenSSL: failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	if (dpp_bn2bin_pad(mx, Mx, curve->prime_len) < 0)
		goto fail;
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
	EVP_PKEY_free(auth->reconfig_old_protocol_key);
	auth->reconfig_old_protocol_key = own_key;
	own_key = NULL;
fail:
	forced_memzero(prk, sizeof(prk));
	forced_memzero(Mx, sizeof(Mx));
	EC_POINT_clear_free(m);
	BN_free(q);
	BN_clear_free(mx);
	BN_clear_free(sum);
	EVP_PKEY_free(own_key);
	EVP_PKEY_free(peer_key);
	BN_CTX_free(bnctx);
	return res;
}


int dpp_reconfig_derive_ke_initiator(struct dpp_authentication *auth,
				     const u8 *r_proto, u16 r_proto_len,
				     struct json_token *net_access_key)
{
	BN_CTX *bnctx = NULL;
	EVP_PKEY *pr = NULL, *peer_key = NULL;
	EC_POINT *sum = NULL, *m = NULL;
	BIGNUM *mx = NULL;
	const EC_KEY *cI, *CR, *PR;
	const EC_GROUP *group;
	const EC_POINT *CR_point, *PR_point;
	const BIGNUM *cI_bn;
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
	EVP_PKEY_free(auth->peer_protocol_key);
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
	cI = EVP_PKEY_get0_EC_KEY(auth->conf->connector_key);
	cI_bn = EC_KEY_get0_private_key(cI);
	group = EC_KEY_get0_group(cI);
	bnctx = BN_CTX_new();
	sum = EC_POINT_new(group);
	m = EC_POINT_new(group);
	mx = BN_new();
	CR = EVP_PKEY_get0_EC_KEY(peer_key);
	PR = EVP_PKEY_get0_EC_KEY(auth->peer_protocol_key);
	CR_point = EC_KEY_get0_public_key(CR);
	PR_point = EC_KEY_get0_public_key(PR);
	if (!bnctx || !sum || !m || !mx ||
	    EC_POINT_add(group, sum, CR_point, PR_point, bnctx) != 1 ||
	    EC_POINT_mul(group, m, NULL, sum, cI_bn, bnctx) != 1 ||
	    EC_POINT_get_affine_coordinates_GFp(group, m, mx, NULL,
						bnctx) != 1 ||
	    dpp_bn2bin_pad(mx, Mx, curve->prime_len) < 0)
		goto fail;

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
	EVP_PKEY_free(pr);
	EVP_PKEY_free(peer_key);
	EC_POINT_clear_free(sum);
	EC_POINT_clear_free(m);
	BN_clear_free(mx);
	BN_CTX_free(bnctx);
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
	char *signed3 = NULL;
	unsigned char *signature = NULL;
	const unsigned char *p;
	size_t signature_len;
	EVP_MD_CTX *md_ctx = NULL;
	ECDSA_SIG *sig = NULL;
	char *dot = ".";
	const EVP_MD *sign_md;
	const BIGNUM *r, *s;

	curve = conf->curve;
	if (curve->hash_len == SHA256_MAC_LEN) {
		sign_md = EVP_sha256();
	} else if (curve->hash_len == SHA384_MAC_LEN) {
		sign_md = EVP_sha384();
	} else if (curve->hash_len == SHA512_MAC_LEN) {
		sign_md = EVP_sha512();
	} else {
		wpa_printf(MSG_DEBUG, "DPP: Unknown signature algorithm");
		goto fail;
	}

	md_ctx = EVP_MD_CTX_create();
	if (!md_ctx)
		goto fail;

	ERR_clear_error();
	if (EVP_DigestSignInit(md_ctx, NULL, sign_md, NULL, conf->csign) != 1) {
		wpa_printf(MSG_DEBUG, "DPP: EVP_DigestSignInit failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
	if (EVP_DigestSignUpdate(md_ctx, signed1, signed1_len) != 1 ||
	    EVP_DigestSignUpdate(md_ctx, dot, 1) != 1 ||
	    EVP_DigestSignUpdate(md_ctx, signed2, signed2_len) != 1) {
		wpa_printf(MSG_DEBUG, "DPP: EVP_DigestSignUpdate failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
	if (EVP_DigestSignFinal(md_ctx, NULL, &signature_len) != 1) {
		wpa_printf(MSG_DEBUG, "DPP: EVP_DigestSignFinal failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
	signature = os_malloc(signature_len);
	if (!signature)
		goto fail;
	if (EVP_DigestSignFinal(md_ctx, signature, &signature_len) != 1) {
		wpa_printf(MSG_DEBUG, "DPP: EVP_DigestSignFinal failed: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
	wpa_hexdump(MSG_DEBUG, "DPP: signedConnector ECDSA signature (DER)",
		    signature, signature_len);
	/* Convert to raw coordinates r,s */
	p = signature;
	sig = d2i_ECDSA_SIG(NULL, &p, signature_len);
	if (!sig)
		goto fail;
	ECDSA_SIG_get0(sig, &r, &s);
	if (dpp_bn2bin_pad(r, signature, curve->prime_len) < 0 ||
	    dpp_bn2bin_pad(s, signature + curve->prime_len,
			   curve->prime_len) < 0)
		goto fail;
	signature_len = 2 * curve->prime_len;
	wpa_hexdump(MSG_DEBUG, "DPP: signedConnector ECDSA signature (raw r,s)",
		    signature, signature_len);
	signed3 = base64_url_encode(signature, signature_len, signed3_len);
fail:
	EVP_MD_CTX_destroy(md_ctx);
	ECDSA_SIG_free(sig);
	os_free(signature);
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
	EVP_PKEY *own_key;
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
	EVP_PKEY_free(own_key);

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
	X509_REQ *req = NULL;
	struct wpabuf *buf = NULL;
	unsigned char *der;
	int der_len;
	EVP_PKEY *key;
	const EVP_MD *sign_md;
	unsigned int hash_len = auth->curve->hash_len;
	EC_KEY *eckey;
	BIO *out = NULL;
	u8 cp[DPP_CP_LEN];
	char *password;
	size_t password_len;
	int res;

	/* TODO: use auth->csrattrs */

	/* TODO: support generation of a new private key if csrAttrs requests
	 * a specific group to be used */
	key = auth->own_protocol_key;

	eckey = EVP_PKEY_get1_EC_KEY(key);
	if (!eckey)
		goto fail;
	der = NULL;
	der_len = i2d_ECPrivateKey(eckey, &der);
	if (der_len <= 0)
		goto fail;
	wpabuf_free(auth->priv_key);
	auth->priv_key = wpabuf_alloc_copy(der, der_len);
	OPENSSL_free(der);
	if (!auth->priv_key)
		goto fail;

	req = X509_REQ_new();
	if (!req || !X509_REQ_set_pubkey(req, key))
		goto fail;

	if (name) {
		X509_NAME *n;

		n = X509_REQ_get_subject_name(req);
		if (!n)
			goto fail;

		if (X509_NAME_add_entry_by_txt(
			    n, "CN", MBSTRING_UTF8,
			    (const unsigned char *) name, -1, -1, 0) != 1)
			goto fail;
	}

	/* cp = HKDF-Expand(bk, "CSR challengePassword", 64) */
	if (dpp_hkdf_expand(hash_len, auth->bk, hash_len,
			    "CSR challengePassword", cp, DPP_CP_LEN) < 0)
		goto fail;
	wpa_hexdump_key(MSG_DEBUG,
			"DPP: cp = HKDF-Expand(bk, \"CSR challengePassword\", 64)",
			cp, DPP_CP_LEN);
	password = base64_encode_no_lf(cp, DPP_CP_LEN, &password_len);
	forced_memzero(cp, DPP_CP_LEN);
	if (!password)
		goto fail;

	res = X509_REQ_add1_attr_by_NID(req, NID_pkcs9_challengePassword,
					V_ASN1_UTF8STRING,
					(const unsigned char *) password,
					password_len);
	bin_clear_free(password, password_len);
	if (!res)
		goto fail;

	/* TODO */

	/* TODO: hash func selection based on csrAttrs */
	if (hash_len == SHA256_MAC_LEN) {
		sign_md = EVP_sha256();
	} else if (hash_len == SHA384_MAC_LEN) {
		sign_md = EVP_sha384();
	} else if (hash_len == SHA512_MAC_LEN) {
		sign_md = EVP_sha512();
	} else {
		wpa_printf(MSG_DEBUG, "DPP: Unknown signature algorithm");
		goto fail;
	}

	if (!X509_REQ_sign(req, key, sign_md))
		goto fail;

	der = NULL;
	der_len = i2d_X509_REQ(req, &der);
	if (der_len < 0)
		goto fail;
	buf = wpabuf_alloc_copy(der, der_len);
	OPENSSL_free(der);

	wpa_hexdump_buf(MSG_DEBUG, "DPP: CSR", buf);

fail:
	BIO_free_all(out);
	X509_REQ_free(req);
	return buf;
}


struct wpabuf * dpp_pkcs7_certs(const struct wpabuf *pkcs7)
{
#ifdef OPENSSL_IS_BORINGSSL
	CBS pkcs7_cbs;
#else /* OPENSSL_IS_BORINGSSL */
	PKCS7 *p7 = NULL;
	const unsigned char *p = wpabuf_head(pkcs7);
#endif /* OPENSSL_IS_BORINGSSL */
	STACK_OF(X509) *certs;
	int i, num;
	BIO *out = NULL;
	size_t rlen;
	struct wpabuf *pem = NULL;
	int res;

#ifdef OPENSSL_IS_BORINGSSL
	certs = sk_X509_new_null();
	if (!certs)
		goto fail;
	CBS_init(&pkcs7_cbs, wpabuf_head(pkcs7), wpabuf_len(pkcs7));
	if (!PKCS7_get_certificates(certs, &pkcs7_cbs)) {
		wpa_printf(MSG_INFO, "DPP: Could not parse PKCS#7 object: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
#else /* OPENSSL_IS_BORINGSSL */
	p7 = d2i_PKCS7(NULL, &p, wpabuf_len(pkcs7));
	if (!p7) {
		wpa_printf(MSG_INFO, "DPP: Could not parse PKCS#7 object: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}

	switch (OBJ_obj2nid(p7->type)) {
	case NID_pkcs7_signed:
		certs = p7->d.sign->cert;
		break;
	case NID_pkcs7_signedAndEnveloped:
		certs = p7->d.signed_and_enveloped->cert;
		break;
	default:
		certs = NULL;
		break;
	}
#endif /* OPENSSL_IS_BORINGSSL */

	if (!certs || ((num = sk_X509_num(certs)) == 0)) {
		wpa_printf(MSG_INFO,
			   "DPP: No certificates found in PKCS#7 object");
		goto fail;
	}

	out = BIO_new(BIO_s_mem());
	if (!out)
		goto fail;

	for (i = 0; i < num; i++) {
		X509 *cert = sk_X509_value(certs, i);

		PEM_write_bio_X509(out, cert);
	}

	rlen = BIO_ctrl_pending(out);
	pem = wpabuf_alloc(rlen);
	if (!pem)
		goto fail;
	res = BIO_read(out, wpabuf_put(pem, 0), rlen);
	if (res <= 0) {
		wpabuf_free(pem);
		pem = NULL;
		goto fail;
	}
	wpabuf_put(pem, res);

fail:
#ifdef OPENSSL_IS_BORINGSSL
	if (certs)
		sk_X509_pop_free(certs, X509_free);
#else /* OPENSSL_IS_BORINGSSL */
	PKCS7_free(p7);
#endif /* OPENSSL_IS_BORINGSSL */
	if (out)
		BIO_free_all(out);

	return pem;
}


int dpp_validate_csr(struct dpp_authentication *auth, const struct wpabuf *csr)
{
	X509_REQ *req;
	const unsigned char *pos;
	EVP_PKEY *pkey;
	int res, loc, ret = -1;
	X509_ATTRIBUTE *attr;
	ASN1_TYPE *type;
	ASN1_STRING *str;
	unsigned char *utf8 = NULL;
	unsigned char *cp = NULL;
	size_t cp_len;
	u8 exp_cp[DPP_CP_LEN];
	unsigned int hash_len = auth->curve->hash_len;

	pos = wpabuf_head(csr);
	req = d2i_X509_REQ(NULL, &pos, wpabuf_len(csr));
	if (!req) {
		wpa_printf(MSG_DEBUG, "DPP: Failed to parse CSR");
		return -1;
	}

	pkey = X509_REQ_get_pubkey(req);
	if (!pkey) {
		wpa_printf(MSG_DEBUG, "DPP: Failed to get public key from CSR");
		goto fail;
	}

	res = X509_REQ_verify(req, pkey);
	EVP_PKEY_free(pkey);
	if (res != 1) {
		wpa_printf(MSG_DEBUG,
			   "DPP: CSR does not have a valid signature");
		goto fail;
	}

	loc = X509_REQ_get_attr_by_NID(req, NID_pkcs9_challengePassword, -1);
	if (loc < 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: CSR does not include challengePassword");
		goto fail;
	}

	attr = X509_REQ_get_attr(req, loc);
	if (!attr) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Could not get challengePassword attribute");
		goto fail;
	}

	type = X509_ATTRIBUTE_get0_type(attr, 0);
	if (!type) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Could not get challengePassword attribute type");
		goto fail;
	}

	res = ASN1_TYPE_get(type);
	/* This is supposed to be UTF8String, but allow other strings as well
	 * since challengePassword is using ASCII (base64 encoded). */
	if (res != V_ASN1_UTF8STRING && res != V_ASN1_PRINTABLESTRING &&
	    res != V_ASN1_IA5STRING) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Unexpected challengePassword attribute type %d",
			   res);
		goto fail;
	}

	str = X509_ATTRIBUTE_get0_data(attr, 0, res, NULL);
	if (!str) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Could not get ASN.1 string for challengePassword");
		goto fail;
	}

	res = ASN1_STRING_to_UTF8(&utf8, str);
	if (res < 0) {
		wpa_printf(MSG_DEBUG,
			   "DPP: Could not get UTF8 version of challengePassword");
		goto fail;
	}

	cp = base64_decode((const char *) utf8, res, &cp_len);
	OPENSSL_free(utf8);
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
	X509_REQ_free(req);
	return ret;
}


struct dpp_reconfig_id * dpp_gen_reconfig_id(const u8 *csign_key,
					     size_t csign_key_len,
					     const u8 *pp_key,
					     size_t pp_key_len)
{
	const unsigned char *p;
	EVP_PKEY *csign = NULL, *ppkey = NULL;
	struct dpp_reconfig_id *id = NULL;
	BN_CTX *ctx = NULL;
	BIGNUM *bn = NULL, *q = NULL;
	const EC_KEY *eckey;
	const EC_GROUP *group;
	EC_POINT *e_id = NULL;

	p = csign_key;
	csign = d2i_PUBKEY(NULL, &p, csign_key_len);
	if (!csign)
		goto fail;

	if (!pp_key)
		goto fail;
	p = pp_key;
	ppkey = d2i_PUBKEY(NULL, &p, pp_key_len);
	if (!ppkey)
		goto fail;

	eckey = EVP_PKEY_get0_EC_KEY(csign);
	if (!eckey)
		goto fail;
	group = EC_KEY_get0_group(eckey);
	if (!group)
		goto fail;

	e_id = EC_POINT_new(group);
	ctx = BN_CTX_new();
	bn = BN_new();
	q = BN_new();
	if (!e_id || !ctx || !bn || !q ||
	    !EC_GROUP_get_order(group, q, ctx) ||
	    !BN_rand_range(bn, q) ||
	    !EC_POINT_mul(group, e_id, bn, NULL, NULL, ctx))
		goto fail;

	dpp_debug_print_point("DPP: Generated random point E-id", group, e_id);

	id = os_zalloc(sizeof(*id));
	if (!id)
		goto fail;
	id->group = group;
	id->e_id = e_id;
	e_id = NULL;
	id->csign = csign;
	csign = NULL;
	id->pp_key = ppkey;
	ppkey = NULL;
fail:
	EC_POINT_free(e_id);
	EVP_PKEY_free(csign);
	EVP_PKEY_free(ppkey);
	BN_clear_free(bn);
	BN_CTX_free(ctx);
	return id;
}


static EVP_PKEY * dpp_pkey_from_point(const EC_GROUP *group,
				      const EC_POINT *point)
{
	EC_KEY *eckey;
	EVP_PKEY *pkey = NULL;

	eckey = EC_KEY_new();
	if (!eckey ||
	    EC_KEY_set_group(eckey, group) != 1 ||
	    EC_KEY_set_public_key(eckey, point) != 1) {
		wpa_printf(MSG_ERROR,
			   "DPP: Failed to set EC_KEY: %s",
			   ERR_error_string(ERR_get_error(), NULL));
		goto fail;
	}
	EC_KEY_set_asn1_flag(eckey, OPENSSL_EC_NAMED_CURVE);

	pkey = EVP_PKEY_new();
	if (!pkey || EVP_PKEY_set1_EC_KEY(pkey, eckey) != 1) {
		wpa_printf(MSG_ERROR, "DPP: Could not create EVP_PKEY");
		EVP_PKEY_free(pkey);
		pkey = NULL;
		goto fail;
	}

fail:
	EC_KEY_free(eckey);
	return pkey;
}


int dpp_update_reconfig_id(struct dpp_reconfig_id *id)
{
	BN_CTX *ctx = NULL;
	BIGNUM *bn = NULL, *q = NULL;
	EC_POINT *e_prime_id = NULL, *a_nonce = NULL;
	int ret = -1;
	const EC_KEY *pp;
	const EC_POINT *pp_point;

	pp = EVP_PKEY_get0_EC_KEY(id->pp_key);
	if (!pp)
		goto fail;
	pp_point = EC_KEY_get0_public_key(pp);
	e_prime_id = EC_POINT_new(id->group);
	a_nonce = EC_POINT_new(id->group);
	ctx = BN_CTX_new();
	bn = BN_new();
	q = BN_new();
	/* Generate random 0 <= a-nonce < q
	 * A-NONCE = a-nonce * G
	 * E'-id = E-id + a-nonce * P_pk */
	if (!pp_point || !e_prime_id || !a_nonce || !ctx || !bn || !q ||
	    !EC_GROUP_get_order(id->group, q, ctx) ||
	    !BN_rand_range(bn, q) || /* bn = a-nonce */
	    !EC_POINT_mul(id->group, a_nonce, bn, NULL, NULL, ctx) ||
	    !EC_POINT_mul(id->group, e_prime_id, NULL, pp_point, bn, ctx) ||
	    !EC_POINT_add(id->group, e_prime_id, id->e_id, e_prime_id, ctx))
		goto fail;

	dpp_debug_print_point("DPP: Generated A-NONCE", id->group, a_nonce);
	dpp_debug_print_point("DPP: Encrypted E-id to E'-id",
			      id->group, e_prime_id);

	EVP_PKEY_free(id->a_nonce);
	EVP_PKEY_free(id->e_prime_id);
	id->a_nonce = dpp_pkey_from_point(id->group, a_nonce);
	id->e_prime_id = dpp_pkey_from_point(id->group, e_prime_id);
	if (!id->a_nonce || !id->e_prime_id)
		goto fail;

	ret = 0;

fail:
	EC_POINT_free(e_prime_id);
	EC_POINT_free(a_nonce);
	BN_clear_free(bn);
	BN_CTX_free(ctx);
	return ret;
}


void dpp_free_reconfig_id(struct dpp_reconfig_id *id)
{
	if (id) {
		EC_POINT_clear_free(id->e_id);
		EVP_PKEY_free(id->csign);
		EVP_PKEY_free(id->a_nonce);
		EVP_PKEY_free(id->e_prime_id);
		EVP_PKEY_free(id->pp_key);
		os_free(id);
	}
}


EC_POINT * dpp_decrypt_e_id(EVP_PKEY *ppkey, EVP_PKEY *a_nonce,
			    EVP_PKEY *e_prime_id)
{
	const EC_KEY *pp_ec, *a_nonce_ec, *e_prime_id_ec;
	const BIGNUM *pp_bn;
	const EC_GROUP *group;
	EC_POINT *e_id = NULL;
	const EC_POINT *a_nonce_point, *e_prime_id_point;
	BN_CTX *ctx = NULL;

	if (!ppkey)
		return NULL;

	/* E-id = E'-id - s_C * A-NONCE */
	pp_ec = EVP_PKEY_get0_EC_KEY(ppkey);
	a_nonce_ec = EVP_PKEY_get0_EC_KEY(a_nonce);
	e_prime_id_ec = EVP_PKEY_get0_EC_KEY(e_prime_id);
	if (!pp_ec || !a_nonce_ec || !e_prime_id_ec)
		return NULL;
	pp_bn = EC_KEY_get0_private_key(pp_ec);
	group = EC_KEY_get0_group(pp_ec);
	a_nonce_point = EC_KEY_get0_public_key(a_nonce_ec);
	e_prime_id_point = EC_KEY_get0_public_key(e_prime_id_ec);
	ctx = BN_CTX_new();
	if (!pp_bn || !group || !a_nonce_point || !e_prime_id_point || !ctx)
		goto fail;
	e_id = EC_POINT_new(group);
	if (!e_id ||
	    !EC_POINT_mul(group, e_id, NULL, a_nonce_point, pp_bn, ctx) ||
	    !EC_POINT_invert(group, e_id, ctx) ||
	    !EC_POINT_add(group, e_id, e_prime_id_point, e_id, ctx)) {
		EC_POINT_clear_free(e_id);
		goto fail;
	}

	dpp_debug_print_point("DPP: Decrypted E-id", group, e_id);

fail:
	BN_CTX_free(ctx);
	return e_id;
}

#endif /* CONFIG_DPP2 */


#ifdef CONFIG_TESTING_OPTIONS

int dpp_test_gen_invalid_key(struct wpabuf *msg,
			     const struct dpp_curve_params *curve)
{
	BN_CTX *ctx;
	BIGNUM *x, *y;
	int ret = -1;
	EC_GROUP *group;
	EC_POINT *point;

	group = EC_GROUP_new_by_curve_name(OBJ_txt2nid(curve->name));
	if (!group)
		return -1;

	ctx = BN_CTX_new();
	point = EC_POINT_new(group);
	x = BN_new();
	y = BN_new();
	if (!ctx || !point || !x || !y)
		goto fail;

	if (BN_rand(x, curve->prime_len * 8, 0, 0) != 1)
		goto fail;

	/* Generate a random y coordinate that results in a point that is not
	 * on the curve. */
	for (;;) {
		if (BN_rand(y, curve->prime_len * 8, 0, 0) != 1)
			goto fail;

		if (EC_POINT_set_affine_coordinates_GFp(group, point, x, y,
							ctx) != 1) {
#if OPENSSL_VERSION_NUMBER >= 0x10100000L || defined(OPENSSL_IS_BORINGSSL)
		/* Unlike older OpenSSL versions, OpenSSL 1.1.1 and BoringSSL
		 * return an error from EC_POINT_set_affine_coordinates_GFp()
		 * when the point is not on the curve. */
			break;
#else /* >=1.1.0 or OPENSSL_IS_BORINGSSL */
			goto fail;
#endif /* >= 1.1.0 or OPENSSL_IS_BORINGSSL */
		}

		if (!EC_POINT_is_on_curve(group, point, ctx))
			break;
	}

	if (dpp_bn2bin_pad(x, wpabuf_put(msg, curve->prime_len),
			   curve->prime_len) < 0 ||
	    dpp_bn2bin_pad(y, wpabuf_put(msg, curve->prime_len),
			   curve->prime_len) < 0)
		goto fail;

	ret = 0;
fail:
	if (ret < 0)
		wpa_printf(MSG_INFO, "DPP: Failed to generate invalid key");
	BN_free(x);
	BN_free(y);
	EC_POINT_free(point);
	BN_CTX_free(ctx);
	EC_GROUP_free(group);

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
