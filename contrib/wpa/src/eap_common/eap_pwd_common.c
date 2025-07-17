/*
 * EAP server/peer: EAP-pwd shared routines
 * Copyright (c) 2010, Dan Harkins <dharkins@lounge.org>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include "common.h"
#include "utils/const_time.h"
#include "common/dragonfly.h"
#include "crypto/sha256.h"
#include "crypto/crypto.h"
#include "eap_defs.h"
#include "eap_pwd_common.h"

#define MAX_ECC_PRIME_LEN 66


/* The random function H(x) = HMAC-SHA256(0^32, x) */
struct crypto_hash * eap_pwd_h_init(void)
{
	u8 allzero[SHA256_MAC_LEN];
	os_memset(allzero, 0, SHA256_MAC_LEN);
	return crypto_hash_init(CRYPTO_HASH_ALG_HMAC_SHA256, allzero,
				SHA256_MAC_LEN);
}


void eap_pwd_h_update(struct crypto_hash *hash, const u8 *data, size_t len)
{
	crypto_hash_update(hash, data, len);
}


void eap_pwd_h_final(struct crypto_hash *hash, u8 *digest)
{
	size_t len = SHA256_MAC_LEN;
	crypto_hash_finish(hash, digest, &len);
}


/* a counter-based KDF based on NIST SP800-108 */
static int eap_pwd_kdf(const u8 *key, size_t keylen, const u8 *label,
		       size_t labellen, u8 *result, size_t resultbitlen)
{
	struct crypto_hash *hash;
	u8 digest[SHA256_MAC_LEN];
	u16 i, ctr, L;
	size_t resultbytelen, len = 0, mdlen;

	resultbytelen = (resultbitlen + 7) / 8;
	ctr = 0;
	L = htons(resultbitlen);
	while (len < resultbytelen) {
		ctr++;
		i = htons(ctr);
		hash = crypto_hash_init(CRYPTO_HASH_ALG_HMAC_SHA256,
					key, keylen);
		if (hash == NULL)
			return -1;
		if (ctr > 1)
			crypto_hash_update(hash, digest, SHA256_MAC_LEN);
		crypto_hash_update(hash, (u8 *) &i, sizeof(u16));
		crypto_hash_update(hash, label, labellen);
		crypto_hash_update(hash, (u8 *) &L, sizeof(u16));
		mdlen = SHA256_MAC_LEN;
		if (crypto_hash_finish(hash, digest, &mdlen) < 0)
			return -1;
		if ((len + mdlen) > resultbytelen)
			os_memcpy(result + len, digest, resultbytelen - len);
		else
			os_memcpy(result + len, digest, mdlen);
		len += mdlen;
	}

	/* since we're expanding to a bit length, mask off the excess */
	if (resultbitlen % 8) {
		u8 mask = 0xff;
		mask <<= (8 - (resultbitlen % 8));
		result[resultbytelen - 1] &= mask;
	}

	return 0;
}


EAP_PWD_group * get_eap_pwd_group(u16 num)
{
	EAP_PWD_group *grp;

	if (!dragonfly_suitable_group(num, 1)) {
		wpa_printf(MSG_INFO, "EAP-pwd: unsuitable group %u", num);
		return NULL;
	}
	grp = os_zalloc(sizeof(EAP_PWD_group));
	if (!grp)
		return NULL;
	grp->group = crypto_ec_init(num);
	if (!grp->group) {
		wpa_printf(MSG_INFO, "EAP-pwd: unable to create EC group");
		os_free(grp);
		return NULL;
	}

	grp->group_num = num;
	wpa_printf(MSG_INFO, "EAP-pwd: provisioned group %d", num);

	return grp;
}


/*
 * compute a "random" secret point on an elliptic curve based
 * on the password and identities.
 */
int compute_password_element(EAP_PWD_group *grp, u16 num,
			     const u8 *password, size_t password_len,
			     const u8 *id_server, size_t id_server_len,
			     const u8 *id_peer, size_t id_peer_len,
			     const u8 *token)
{
	struct crypto_bignum *qr = NULL, *qnr = NULL;
	u8 qr_bin[MAX_ECC_PRIME_LEN];
	u8 qnr_bin[MAX_ECC_PRIME_LEN];
	u8 qr_or_qnr_bin[MAX_ECC_PRIME_LEN];
	u8 x_bin[MAX_ECC_PRIME_LEN];
	u8 prime_bin[MAX_ECC_PRIME_LEN];
	u8 x_y[2 * MAX_ECC_PRIME_LEN];
	struct crypto_bignum *tmp2 = NULL, *y = NULL;
	struct crypto_hash *hash;
	unsigned char pwe_digest[SHA256_MAC_LEN], *prfbuf = NULL, ctr;
	int ret = 0, res;
	u8 found = 0; /* 0 (false) or 0xff (true) to be used as const_time_*
		       * mask */
	size_t primebytelen = 0, primebitlen;
	struct crypto_bignum *x_candidate = NULL;
	const struct crypto_bignum *prime;
	u8 found_ctr = 0, is_odd = 0;
	int cmp_prime;
	unsigned int in_range;
	unsigned int is_eq;

	if (grp->pwe)
		return -1;

	os_memset(x_bin, 0, sizeof(x_bin));

	prime = crypto_ec_get_prime(grp->group);
	primebitlen = crypto_ec_prime_len_bits(grp->group);
	primebytelen = crypto_ec_prime_len(grp->group);
	if (crypto_bignum_to_bin(prime, prime_bin, sizeof(prime_bin),
				 primebytelen) < 0)
		return -1;

	if ((prfbuf = os_malloc(primebytelen)) == NULL) {
		wpa_printf(MSG_INFO, "EAP-pwd: unable to malloc space for prf "
			   "buffer");
		goto fail;
	}

	/* get a random quadratic residue and nonresidue */
	if (dragonfly_get_random_qr_qnr(prime, &qr, &qnr) < 0 ||
	    crypto_bignum_to_bin(qr, qr_bin, sizeof(qr_bin),
				 primebytelen) < 0 ||
	    crypto_bignum_to_bin(qnr, qnr_bin, sizeof(qnr_bin),
				 primebytelen) < 0)
		goto fail;

	os_memset(prfbuf, 0, primebytelen);
	ctr = 0;

	/*
	 * Run through the hunting-and-pecking loop 40 times to mask the time
	 * necessary to find PWE. The odds of PWE not being found in 40 loops is
	 * roughly 1 in 1 trillion.
	 */
	while (ctr < 40) {
		ctr++;

		/*
		 * compute counter-mode password value and stretch to prime
		 *    pwd-seed = H(token | peer-id | server-id | password |
		 *		   counter)
		 */
		hash = eap_pwd_h_init();
		if (hash == NULL)
			goto fail;
		eap_pwd_h_update(hash, token, sizeof(u32));
		eap_pwd_h_update(hash, id_peer, id_peer_len);
		eap_pwd_h_update(hash, id_server, id_server_len);
		eap_pwd_h_update(hash, password, password_len);
		eap_pwd_h_update(hash, &ctr, sizeof(ctr));
		eap_pwd_h_final(hash, pwe_digest);

		is_odd = const_time_select_u8(
			found, is_odd, pwe_digest[SHA256_MAC_LEN - 1] & 0x01);
		if (eap_pwd_kdf(pwe_digest, SHA256_MAC_LEN,
				(u8 *) "EAP-pwd Hunting And Pecking",
				os_strlen("EAP-pwd Hunting And Pecking"),
				prfbuf, primebitlen) < 0)
			goto fail;
		if (primebitlen % 8)
			buf_shift_right(prfbuf, primebytelen,
					8 - primebitlen % 8);
		cmp_prime = const_time_memcmp(prfbuf, prime_bin, primebytelen);
		/* Create a const_time mask for selection based on prf result
		 * being smaller than prime. */
		in_range = const_time_fill_msb((unsigned int) cmp_prime);
		/* The algorithm description would skip the next steps if
		 * cmp_prime >= 0, but go through them regardless to minimize
		 * externally observable differences in behavior. */

		crypto_bignum_deinit(x_candidate, 1);
		x_candidate = crypto_bignum_init_set(prfbuf, primebytelen);
		if (!x_candidate) {
			wpa_printf(MSG_INFO,
				   "EAP-pwd: unable to create x_candidate");
			goto fail;
		}

		wpa_hexdump_key(MSG_DEBUG, "EAP-pwd: x_candidate",
				prfbuf, primebytelen);
		const_time_select_bin(found, x_bin, prfbuf, primebytelen,
				      x_bin);

		/*
		 * compute y^2 using the equation of the curve
		 *
		 *      y^2 = x^3 + ax + b
		 */
		crypto_bignum_deinit(tmp2, 1);
		tmp2 = crypto_ec_point_compute_y_sqr(grp->group, x_candidate);
		if (!tmp2)
			goto fail;

		res = dragonfly_is_quadratic_residue_blind(grp->group, qr_bin,
							   qnr_bin, tmp2);
		if (res < 0)
			goto fail;
		found_ctr = const_time_select_u8(found, found_ctr, ctr);
		/* found is 0 or 0xff here and res is 0 or 1. Bitwise OR of them
		 * (with res converted to 0/0xff and masked with prf being below
		 * prime) handles this in constant time.
		 */
		found |= (res & in_range) * 0xff;
	}
	if (found == 0) {
		wpa_printf(MSG_INFO,
			   "EAP-pwd: unable to find random point on curve for group %d, something's fishy",
			   num);
		goto fail;
	}

	/*
	 * We know x_candidate is a quadratic residue so set it here.
	 */
	crypto_bignum_deinit(x_candidate, 1);
	x_candidate = crypto_bignum_init_set(x_bin, primebytelen);
	if (!x_candidate)
		goto fail;

	/* y = sqrt(x^3 + ax + b) mod p
	 * if LSB(y) == LSB(pwd-seed): PWE = (x, y)
	 * else: PWE = (x, p - y)
	 *
	 * Calculate y and the two possible values for PWE and after that,
	 * use constant time selection to copy the correct alternative.
	 */
	y = crypto_ec_point_compute_y_sqr(grp->group, x_candidate);
	if (!y ||
	    dragonfly_sqrt(grp->group, y, y) < 0 ||
	    crypto_bignum_to_bin(y, x_y, MAX_ECC_PRIME_LEN, primebytelen) < 0 ||
	    crypto_bignum_sub(prime, y, y) < 0 ||
	    crypto_bignum_to_bin(y, x_y + MAX_ECC_PRIME_LEN,
				 MAX_ECC_PRIME_LEN, primebytelen) < 0) {
		wpa_printf(MSG_DEBUG, "EAP-pwd: Could not solve y");
		goto fail;
	}

	/* Constant time selection of the y coordinate from the two
	 * options */
	is_eq = const_time_eq(is_odd, x_y[primebytelen - 1] & 0x01);
	const_time_select_bin(is_eq, x_y, x_y + MAX_ECC_PRIME_LEN,
			      primebytelen, x_y + primebytelen);
	os_memcpy(x_y, x_bin, primebytelen);
	wpa_hexdump_key(MSG_DEBUG, "EAP-pwd: PWE", x_y, 2 * primebytelen);
	grp->pwe = crypto_ec_point_from_bin(grp->group, x_y);
	if (!grp->pwe) {
		wpa_printf(MSG_DEBUG, "EAP-pwd: Could not generate PWE");
		goto fail;
	}

	/*
	 * If there's a solution to the equation then the point must be on the
	 * curve so why check again explicitly? OpenSSL code says this is
	 * required by X9.62. We're not X9.62 but it can't hurt just to be sure.
	 */
	if (!crypto_ec_point_is_on_curve(grp->group, grp->pwe)) {
		wpa_printf(MSG_INFO, "EAP-pwd: point is not on curve");
		goto fail;
	}

	wpa_printf(MSG_DEBUG, "EAP-pwd: found a PWE in %02d tries", found_ctr);

	if (0) {
 fail:
		crypto_ec_point_deinit(grp->pwe, 1);
		grp->pwe = NULL;
		ret = 1;
	}
	/* cleanliness and order.... */
	crypto_bignum_deinit(x_candidate, 1);
	crypto_bignum_deinit(tmp2, 1);
	crypto_bignum_deinit(y, 1);
	crypto_bignum_deinit(qr, 1);
	crypto_bignum_deinit(qnr, 1);
	bin_clear_free(prfbuf, primebytelen);
	os_memset(qr_bin, 0, sizeof(qr_bin));
	os_memset(qnr_bin, 0, sizeof(qnr_bin));
	os_memset(qr_or_qnr_bin, 0, sizeof(qr_or_qnr_bin));
	os_memset(pwe_digest, 0, sizeof(pwe_digest));
	forced_memzero(x_y, sizeof(x_y));

	return ret;
}


int compute_keys(EAP_PWD_group *grp, const struct crypto_bignum *k,
		 const struct crypto_bignum *peer_scalar,
		 const struct crypto_bignum *server_scalar,
		 const u8 *confirm_peer, const u8 *confirm_server,
		 const u32 *ciphersuite, u8 *msk, u8 *emsk, u8 *session_id)
{
	struct crypto_hash *hash;
	u8 mk[SHA256_MAC_LEN], *cruft;
	u8 msk_emsk[EAP_MSK_LEN + EAP_EMSK_LEN];
	size_t prime_len, order_len;

	prime_len = crypto_ec_prime_len(grp->group);
	order_len = crypto_ec_order_len(grp->group);

	cruft = os_malloc(prime_len);
	if (!cruft)
		return -1;

	/*
	 * first compute the session-id = TypeCode | H(ciphersuite | scal_p |
	 *	scal_s)
	 */
	session_id[0] = EAP_TYPE_PWD;
	hash = eap_pwd_h_init();
	if (hash == NULL) {
		os_free(cruft);
		return -1;
	}
	eap_pwd_h_update(hash, (const u8 *) ciphersuite, sizeof(u32));
	if (crypto_bignum_to_bin(peer_scalar, cruft, order_len,
				 order_len) < 0) {
		os_free(cruft);
		return -1;
	}

	eap_pwd_h_update(hash, cruft, order_len);
	if (crypto_bignum_to_bin(server_scalar, cruft, order_len,
				 order_len) < 0) {
		os_free(cruft);
		return -1;
	}

	eap_pwd_h_update(hash, cruft, order_len);
	eap_pwd_h_final(hash, &session_id[1]);

	/* then compute MK = H(k | confirm-peer | confirm-server) */
	hash = eap_pwd_h_init();
	if (hash == NULL) {
		os_free(cruft);
		return -1;
	}

	if (crypto_bignum_to_bin(k, cruft, prime_len, prime_len) < 0) {
		os_free(cruft);
		return -1;
	}

	eap_pwd_h_update(hash, cruft, prime_len);
	os_free(cruft);
	eap_pwd_h_update(hash, confirm_peer, SHA256_MAC_LEN);
	eap_pwd_h_update(hash, confirm_server, SHA256_MAC_LEN);
	eap_pwd_h_final(hash, mk);

	/* stretch the mk with the session-id to get MSK | EMSK */
	if (eap_pwd_kdf(mk, SHA256_MAC_LEN,
			session_id, SHA256_MAC_LEN + 1,
			msk_emsk, (EAP_MSK_LEN + EAP_EMSK_LEN) * 8) < 0) {
		return -1;
	}

	os_memcpy(msk, msk_emsk, EAP_MSK_LEN);
	os_memcpy(emsk, msk_emsk + EAP_MSK_LEN, EAP_EMSK_LEN);

	return 1;
}


static int eap_pwd_element_coord_ok(const struct crypto_bignum *prime,
				    const u8 *buf, size_t len)
{
	struct crypto_bignum *val;
	int ok = 1;

	val = crypto_bignum_init_set(buf, len);
	if (!val || crypto_bignum_is_zero(val) ||
	    crypto_bignum_cmp(val, prime) >= 0)
		ok = 0;
	crypto_bignum_deinit(val, 0);
	return ok;
}


struct crypto_ec_point * eap_pwd_get_element(EAP_PWD_group *group,
					     const u8 *buf)
{
	struct crypto_ec_point *element;
	const struct crypto_bignum *prime;
	size_t prime_len;

	prime = crypto_ec_get_prime(group->group);
	prime_len = crypto_ec_prime_len(group->group);

	/* RFC 5931, 2.8.5.2.2: 0 < x,y < p */
	if (!eap_pwd_element_coord_ok(prime, buf, prime_len) ||
	    !eap_pwd_element_coord_ok(prime, buf + prime_len, prime_len)) {
		wpa_printf(MSG_INFO, "EAP-pwd: Invalid coordinate in element");
		return NULL;
	}

	element = crypto_ec_point_from_bin(group->group, buf);
	if (!element) {
		wpa_printf(MSG_INFO, "EAP-pwd: EC point from element failed");
		return NULL;
	}

	/* RFC 5931, 2.8.5.2.2: on curve and not the point at infinity */
	if (!crypto_ec_point_is_on_curve(group->group, element) ||
	    crypto_ec_point_is_at_infinity(group->group, element)) {
		wpa_printf(MSG_INFO, "EAP-pwd: Invalid element");
		goto fail;
	}

out:
	return element;
fail:
	crypto_ec_point_deinit(element, 0);
	element = NULL;
	goto out;
}


struct crypto_bignum * eap_pwd_get_scalar(EAP_PWD_group *group, const u8 *buf)
{
	struct crypto_bignum *scalar;
	const struct crypto_bignum *order;
	size_t order_len;

	order = crypto_ec_get_order(group->group);
	order_len = crypto_ec_order_len(group->group);

	/* RFC 5931, 2.8.5.2: 1 < scalar < r */
	scalar = crypto_bignum_init_set(buf, order_len);
	if (!scalar || crypto_bignum_is_zero(scalar) ||
	    crypto_bignum_is_one(scalar) ||
	    crypto_bignum_cmp(scalar, order) >= 0) {
		wpa_printf(MSG_INFO, "EAP-pwd: received scalar is invalid");
		crypto_bignum_deinit(scalar, 0);
		scalar = NULL;
	}

	return scalar;
}


int eap_pwd_get_rand_mask(EAP_PWD_group *group, struct crypto_bignum *_rand,
			  struct crypto_bignum *_mask,
			  struct crypto_bignum *scalar)
{
	return dragonfly_generate_scalar(crypto_ec_get_order(group->group),
					 _rand, _mask, scalar);
}
