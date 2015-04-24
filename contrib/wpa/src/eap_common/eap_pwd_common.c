/*
 * EAP server/peer: EAP-pwd shared routines
 * Copyright (c) 2010, Dan Harkins <dharkins@lounge.org>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include "common.h"
#include "crypto/sha256.h"
#include "crypto/crypto.h"
#include "eap_defs.h"
#include "eap_pwd_common.h"

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


/*
 * compute a "random" secret point on an elliptic curve based
 * on the password and identities.
 */
int compute_password_element(EAP_PWD_group *grp, u16 num,
			     u8 *password, int password_len,
			     u8 *id_server, int id_server_len,
			     u8 *id_peer, int id_peer_len, u8 *token)
{
	BIGNUM *x_candidate = NULL, *rnd = NULL, *cofactor = NULL;
	struct crypto_hash *hash;
	unsigned char pwe_digest[SHA256_MAC_LEN], *prfbuf = NULL, ctr;
	int nid, is_odd, ret = 0;
	size_t primebytelen, primebitlen;

	switch (num) { /* from IANA registry for IKE D-H groups */
        case 19:
		nid = NID_X9_62_prime256v1;
		break;
        case 20:
		nid = NID_secp384r1;
		break;
        case 21:
		nid = NID_secp521r1;
		break;
#ifndef OPENSSL_IS_BORINGSSL
        case 25:
		nid = NID_X9_62_prime192v1;
		break;
#endif /* OPENSSL_IS_BORINGSSL */
        case 26:
		nid = NID_secp224r1;
		break;
        default:
		wpa_printf(MSG_INFO, "EAP-pwd: unsupported group %d", num);
		return -1;
	}

	grp->pwe = NULL;
	grp->order = NULL;
	grp->prime = NULL;

	if ((grp->group = EC_GROUP_new_by_curve_name(nid)) == NULL) {
		wpa_printf(MSG_INFO, "EAP-pwd: unable to create EC_GROUP");
		goto fail;
	}

	if (((rnd = BN_new()) == NULL) ||
	    ((cofactor = BN_new()) == NULL) ||
	    ((grp->pwe = EC_POINT_new(grp->group)) == NULL) ||
	    ((grp->order = BN_new()) == NULL) ||
	    ((grp->prime = BN_new()) == NULL) ||
	    ((x_candidate = BN_new()) == NULL)) {
		wpa_printf(MSG_INFO, "EAP-pwd: unable to create bignums");
		goto fail;
	}

	if (!EC_GROUP_get_curve_GFp(grp->group, grp->prime, NULL, NULL, NULL))
	{
		wpa_printf(MSG_INFO, "EAP-pwd: unable to get prime for GFp "
			   "curve");
		goto fail;
	}
	if (!EC_GROUP_get_order(grp->group, grp->order, NULL)) {
		wpa_printf(MSG_INFO, "EAP-pwd: unable to get order for curve");
		goto fail;
	}
	if (!EC_GROUP_get_cofactor(grp->group, cofactor, NULL)) {
		wpa_printf(MSG_INFO, "EAP-pwd: unable to get cofactor for "
			   "curve");
		goto fail;
	}
	primebitlen = BN_num_bits(grp->prime);
	primebytelen = BN_num_bytes(grp->prime);
	if ((prfbuf = os_malloc(primebytelen)) == NULL) {
		wpa_printf(MSG_INFO, "EAP-pwd: unable to malloc space for prf "
			   "buffer");
		goto fail;
	}
	os_memset(prfbuf, 0, primebytelen);
	ctr = 0;
	while (1) {
		if (ctr > 30) {
			wpa_printf(MSG_INFO, "EAP-pwd: unable to find random "
				   "point on curve for group %d, something's "
				   "fishy", num);
			goto fail;
		}
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

		BN_bin2bn(pwe_digest, SHA256_MAC_LEN, rnd);

		if (eap_pwd_kdf(pwe_digest, SHA256_MAC_LEN,
				(u8 *) "EAP-pwd Hunting And Pecking",
				os_strlen("EAP-pwd Hunting And Pecking"),
				prfbuf, primebitlen) < 0)
			goto fail;

		BN_bin2bn(prfbuf, primebytelen, x_candidate);

		/*
		 * eap_pwd_kdf() returns a string of bits 0..primebitlen but
		 * BN_bin2bn will treat that string of bits as a big endian
		 * number. If the primebitlen is not an even multiple of 8
		 * then excessive bits-- those _after_ primebitlen-- so now
		 * we have to shift right the amount we masked off.
		 */
		if (primebitlen % 8)
			BN_rshift(x_candidate, x_candidate,
				  (8 - (primebitlen % 8)));

		if (BN_ucmp(x_candidate, grp->prime) >= 0)
			continue;

		wpa_hexdump(MSG_DEBUG, "EAP-pwd: x_candidate",
			    prfbuf, primebytelen);

		/*
		 * need to unambiguously identify the solution, if there is
		 * one...
		 */
		if (BN_is_odd(rnd))
			is_odd = 1;
		else
			is_odd = 0;

		/*
		 * solve the quadratic equation, if it's not solvable then we
		 * don't have a point
		 */
		if (!EC_POINT_set_compressed_coordinates_GFp(grp->group,
							     grp->pwe,
							     x_candidate,
							     is_odd, NULL))
			continue;
		/*
		 * If there's a solution to the equation then the point must be
		 * on the curve so why check again explicitly? OpenSSL code
		 * says this is required by X9.62. We're not X9.62 but it can't
		 * hurt just to be sure.
		 */
		if (!EC_POINT_is_on_curve(grp->group, grp->pwe, NULL)) {
			wpa_printf(MSG_INFO, "EAP-pwd: point is not on curve");
			continue;
		}

		if (BN_cmp(cofactor, BN_value_one())) {
			/* make sure the point is not in a small sub-group */
			if (!EC_POINT_mul(grp->group, grp->pwe, NULL, grp->pwe,
					  cofactor, NULL)) {
				wpa_printf(MSG_INFO, "EAP-pwd: cannot "
					   "multiply generator by order");
				continue;
			}
			if (EC_POINT_is_at_infinity(grp->group, grp->pwe)) {
				wpa_printf(MSG_INFO, "EAP-pwd: point is at "
					   "infinity");
				continue;
			}
		}
		/* if we got here then we have a new generator. */
		break;
	}
	wpa_printf(MSG_DEBUG, "EAP-pwd: found a PWE in %d tries", ctr);
	grp->group_num = num;
	if (0) {
 fail:
		EC_GROUP_free(grp->group);
		grp->group = NULL;
		EC_POINT_clear_free(grp->pwe);
		grp->pwe = NULL;
		BN_clear_free(grp->order);
		grp->order = NULL;
		BN_clear_free(grp->prime);
		grp->prime = NULL;
		ret = 1;
	}
	/* cleanliness and order.... */
	BN_clear_free(cofactor);
	BN_clear_free(x_candidate);
	BN_clear_free(rnd);
	os_free(prfbuf);

	return ret;
}


int compute_keys(EAP_PWD_group *grp, BN_CTX *bnctx, BIGNUM *k,
		 BIGNUM *peer_scalar, BIGNUM *server_scalar,
		 u8 *confirm_peer, u8 *confirm_server,
		 u32 *ciphersuite, u8 *msk, u8 *emsk, u8 *session_id)
{
	struct crypto_hash *hash;
	u8 mk[SHA256_MAC_LEN], *cruft;
	u8 msk_emsk[EAP_MSK_LEN + EAP_EMSK_LEN];
	int offset;

	if ((cruft = os_malloc(BN_num_bytes(grp->prime))) == NULL)
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
	eap_pwd_h_update(hash, (u8 *) ciphersuite, sizeof(u32));
	offset = BN_num_bytes(grp->order) - BN_num_bytes(peer_scalar);
	os_memset(cruft, 0, BN_num_bytes(grp->prime));
	BN_bn2bin(peer_scalar, cruft + offset);
	eap_pwd_h_update(hash, cruft, BN_num_bytes(grp->order));
	offset = BN_num_bytes(grp->order) - BN_num_bytes(server_scalar);
	os_memset(cruft, 0, BN_num_bytes(grp->prime));
	BN_bn2bin(server_scalar, cruft + offset);
	eap_pwd_h_update(hash, cruft, BN_num_bytes(grp->order));
	eap_pwd_h_final(hash, &session_id[1]);

	/* then compute MK = H(k | confirm-peer | confirm-server) */
	hash = eap_pwd_h_init();
	if (hash == NULL) {
		os_free(cruft);
		return -1;
	}
	offset = BN_num_bytes(grp->prime) - BN_num_bytes(k);
	os_memset(cruft, 0, BN_num_bytes(grp->prime));
	BN_bn2bin(k, cruft + offset);
	eap_pwd_h_update(hash, cruft, BN_num_bytes(grp->prime));
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
