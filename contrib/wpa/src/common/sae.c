/*
 * Simultaneous authentication of equals
 * Copyright (c) 2012-2016, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "utils/const_time.h"
#include "crypto/crypto.h"
#include "crypto/sha256.h"
#include "crypto/random.h"
#include "crypto/dh_groups.h"
#include "ieee802_11_defs.h"
#include "sae.h"


static int sae_suitable_group(int group)
{
#ifdef CONFIG_TESTING_OPTIONS
	/* Allow all groups for testing purposes in non-production builds. */
	return 1;
#else /* CONFIG_TESTING_OPTIONS */
	/* Enforce REVmd rules on which SAE groups are suitable for production
	 * purposes: FFC groups whose prime is >= 3072 bits and ECC groups
	 * defined over a prime field whose prime is >= 256 bits. Furthermore,
	 * ECC groups defined over a characteristic 2 finite field and ECC
	 * groups with a co-factor greater than 1 are not suitable. */
	return group == 19 || group == 20 || group == 21 ||
		group == 28 || group == 29 || group == 30 ||
		group == 15 || group == 16 || group == 17 || group == 18;
#endif /* CONFIG_TESTING_OPTIONS */
}


int sae_set_group(struct sae_data *sae, int group)
{
	struct sae_temporary_data *tmp;

	if (!sae_suitable_group(group)) {
		wpa_printf(MSG_DEBUG, "SAE: Reject unsuitable group %d", group);
		return -1;
	}

	sae_clear_data(sae);
	tmp = sae->tmp = os_zalloc(sizeof(*tmp));
	if (tmp == NULL)
		return -1;

	/* First, check if this is an ECC group */
	tmp->ec = crypto_ec_init(group);
	if (tmp->ec) {
		wpa_printf(MSG_DEBUG, "SAE: Selecting supported ECC group %d",
			   group);
		sae->group = group;
		tmp->prime_len = crypto_ec_prime_len(tmp->ec);
		tmp->prime = crypto_ec_get_prime(tmp->ec);
		tmp->order = crypto_ec_get_order(tmp->ec);
		return 0;
	}

	/* Not an ECC group, check FFC */
	tmp->dh = dh_groups_get(group);
	if (tmp->dh) {
		wpa_printf(MSG_DEBUG, "SAE: Selecting supported FFC group %d",
			   group);
		sae->group = group;
		tmp->prime_len = tmp->dh->prime_len;
		if (tmp->prime_len > SAE_MAX_PRIME_LEN) {
			sae_clear_data(sae);
			return -1;
		}

		tmp->prime_buf = crypto_bignum_init_set(tmp->dh->prime,
							tmp->prime_len);
		if (tmp->prime_buf == NULL) {
			sae_clear_data(sae);
			return -1;
		}
		tmp->prime = tmp->prime_buf;

		tmp->order_buf = crypto_bignum_init_set(tmp->dh->order,
							tmp->dh->order_len);
		if (tmp->order_buf == NULL) {
			sae_clear_data(sae);
			return -1;
		}
		tmp->order = tmp->order_buf;

		return 0;
	}

	/* Unsupported group */
	wpa_printf(MSG_DEBUG,
		   "SAE: Group %d not supported by the crypto library", group);
	return -1;
}


void sae_clear_temp_data(struct sae_data *sae)
{
	struct sae_temporary_data *tmp;
	if (sae == NULL || sae->tmp == NULL)
		return;
	tmp = sae->tmp;
	crypto_ec_deinit(tmp->ec);
	crypto_bignum_deinit(tmp->prime_buf, 0);
	crypto_bignum_deinit(tmp->order_buf, 0);
	crypto_bignum_deinit(tmp->sae_rand, 1);
	crypto_bignum_deinit(tmp->pwe_ffc, 1);
	crypto_bignum_deinit(tmp->own_commit_scalar, 0);
	crypto_bignum_deinit(tmp->own_commit_element_ffc, 0);
	crypto_bignum_deinit(tmp->peer_commit_element_ffc, 0);
	crypto_ec_point_deinit(tmp->pwe_ecc, 1);
	crypto_ec_point_deinit(tmp->own_commit_element_ecc, 0);
	crypto_ec_point_deinit(tmp->peer_commit_element_ecc, 0);
	wpabuf_free(tmp->anti_clogging_token);
	os_free(tmp->pw_id);
	bin_clear_free(tmp, sizeof(*tmp));
	sae->tmp = NULL;
}


void sae_clear_data(struct sae_data *sae)
{
	if (sae == NULL)
		return;
	sae_clear_temp_data(sae);
	crypto_bignum_deinit(sae->peer_commit_scalar, 0);
	os_memset(sae, 0, sizeof(*sae));
}


static void buf_shift_right(u8 *buf, size_t len, size_t bits)
{
	size_t i;
	for (i = len - 1; i > 0; i--)
		buf[i] = (buf[i - 1] << (8 - bits)) | (buf[i] >> bits);
	buf[0] >>= bits;
}


static struct crypto_bignum * sae_get_rand(struct sae_data *sae)
{
	u8 val[SAE_MAX_PRIME_LEN];
	int iter = 0;
	struct crypto_bignum *bn = NULL;
	int order_len_bits = crypto_bignum_bits(sae->tmp->order);
	size_t order_len = (order_len_bits + 7) / 8;

	if (order_len > sizeof(val))
		return NULL;

	for (;;) {
		if (iter++ > 100 || random_get_bytes(val, order_len) < 0)
			return NULL;
		if (order_len_bits % 8)
			buf_shift_right(val, order_len, 8 - order_len_bits % 8);
		bn = crypto_bignum_init_set(val, order_len);
		if (bn == NULL)
			return NULL;
		if (crypto_bignum_is_zero(bn) ||
		    crypto_bignum_is_one(bn) ||
		    crypto_bignum_cmp(bn, sae->tmp->order) >= 0) {
			crypto_bignum_deinit(bn, 0);
			continue;
		}
		break;
	}

	os_memset(val, 0, order_len);
	return bn;
}


static struct crypto_bignum * sae_get_rand_and_mask(struct sae_data *sae)
{
	crypto_bignum_deinit(sae->tmp->sae_rand, 1);
	sae->tmp->sae_rand = sae_get_rand(sae);
	if (sae->tmp->sae_rand == NULL)
		return NULL;
	return sae_get_rand(sae);
}


static void sae_pwd_seed_key(const u8 *addr1, const u8 *addr2, u8 *key)
{
	wpa_printf(MSG_DEBUG, "SAE: PWE derivation - addr1=" MACSTR
		   " addr2=" MACSTR, MAC2STR(addr1), MAC2STR(addr2));
	if (os_memcmp(addr1, addr2, ETH_ALEN) > 0) {
		os_memcpy(key, addr1, ETH_ALEN);
		os_memcpy(key + ETH_ALEN, addr2, ETH_ALEN);
	} else {
		os_memcpy(key, addr2, ETH_ALEN);
		os_memcpy(key + ETH_ALEN, addr1, ETH_ALEN);
	}
}


static struct crypto_bignum *
get_rand_1_to_p_1(const u8 *prime, size_t prime_len, size_t prime_bits,
		  int *r_odd)
{
	for (;;) {
		struct crypto_bignum *r;
		u8 tmp[SAE_MAX_ECC_PRIME_LEN];

		if (random_get_bytes(tmp, prime_len) < 0)
			break;
		if (prime_bits % 8)
			buf_shift_right(tmp, prime_len, 8 - prime_bits % 8);
		if (os_memcmp(tmp, prime, prime_len) >= 0)
			continue;
		r = crypto_bignum_init_set(tmp, prime_len);
		if (!r)
			break;
		if (crypto_bignum_is_zero(r)) {
			crypto_bignum_deinit(r, 0);
			continue;
		}

		*r_odd = tmp[prime_len - 1] & 0x01;
		return r;
	}

	return NULL;
}


static int is_quadratic_residue_blind(struct sae_data *sae,
				      const u8 *prime, size_t bits,
				      const u8 *qr, const u8 *qnr,
				      const struct crypto_bignum *y_sqr)
{
	struct crypto_bignum *r, *num, *qr_or_qnr = NULL;
	int r_odd, check, res = -1;
	u8 qr_or_qnr_bin[SAE_MAX_ECC_PRIME_LEN];
	size_t prime_len = sae->tmp->prime_len;
	unsigned int mask;

	/*
	 * Use the blinding technique to mask y_sqr while determining
	 * whether it is a quadratic residue modulo p to avoid leaking
	 * timing information while determining the Legendre symbol.
	 *
	 * v = y_sqr
	 * r = a random number between 1 and p-1, inclusive
	 * num = (v * r * r) modulo p
	 */
	r = get_rand_1_to_p_1(prime, prime_len, bits, &r_odd);
	if (!r)
		return -1;

	num = crypto_bignum_init();
	if (!num ||
	    crypto_bignum_mulmod(y_sqr, r, sae->tmp->prime, num) < 0 ||
	    crypto_bignum_mulmod(num, r, sae->tmp->prime, num) < 0)
		goto fail;

	/*
	 * Need to minimize differences in handling different cases, so try to
	 * avoid branches and timing differences.
	 *
	 * If r_odd:
	 * num = (num * qr) module p
	 * LGR(num, p) = 1 ==> quadratic residue
	 * else:
	 * num = (num * qnr) module p
	 * LGR(num, p) = -1 ==> quadratic residue
	 */
	mask = const_time_is_zero(r_odd);
	const_time_select_bin(mask, qnr, qr, prime_len, qr_or_qnr_bin);
	qr_or_qnr = crypto_bignum_init_set(qr_or_qnr_bin, prime_len);
	if (!qr_or_qnr ||
	    crypto_bignum_mulmod(num, qr_or_qnr, sae->tmp->prime, num) < 0)
		goto fail;
	/* r_odd is 0 or 1; branchless version of check = r_odd ? 1 : -1, */
	check = const_time_select_int(mask, -1, 1);

	res = crypto_bignum_legendre(num, sae->tmp->prime);
	if (res == -2) {
		res = -1;
		goto fail;
	}
	/* branchless version of res = res == check
	 * (res is -1, 0, or 1; check is -1 or 1) */
	mask = const_time_eq(res, check);
	res = const_time_select_int(mask, 1, 0);
fail:
	crypto_bignum_deinit(num, 1);
	crypto_bignum_deinit(r, 1);
	crypto_bignum_deinit(qr_or_qnr, 1);
	return res;
}


static int sae_test_pwd_seed_ecc(struct sae_data *sae, const u8 *pwd_seed,
				 const u8 *prime, const u8 *qr, const u8 *qnr,
				 u8 *pwd_value)
{
	struct crypto_bignum *y_sqr, *x_cand;
	int res;
	size_t bits;

	wpa_hexdump_key(MSG_DEBUG, "SAE: pwd-seed", pwd_seed, SHA256_MAC_LEN);

	/* pwd-value = KDF-z(pwd-seed, "SAE Hunting and Pecking", p) */
	bits = crypto_ec_prime_len_bits(sae->tmp->ec);
	if (sha256_prf_bits(pwd_seed, SHA256_MAC_LEN, "SAE Hunting and Pecking",
			    prime, sae->tmp->prime_len, pwd_value, bits) < 0)
		return -1;
	if (bits % 8)
		buf_shift_right(pwd_value, sae->tmp->prime_len, 8 - bits % 8);
	wpa_hexdump_key(MSG_DEBUG, "SAE: pwd-value",
			pwd_value, sae->tmp->prime_len);

	if (os_memcmp(pwd_value, prime, sae->tmp->prime_len) >= 0)
		return 0;

	x_cand = crypto_bignum_init_set(pwd_value, sae->tmp->prime_len);
	if (!x_cand)
		return -1;
	y_sqr = crypto_ec_point_compute_y_sqr(sae->tmp->ec, x_cand);
	crypto_bignum_deinit(x_cand, 1);
	if (!y_sqr)
		return -1;

	res = is_quadratic_residue_blind(sae, prime, bits, qr, qnr, y_sqr);
	crypto_bignum_deinit(y_sqr, 1);
	return res;
}


/* Returns -1 on fatal failure, 0 if PWE cannot be derived from the provided
 * pwd-seed, or 1 if a valid PWE was derived from pwd-seed. */
static int sae_test_pwd_seed_ffc(struct sae_data *sae, const u8 *pwd_seed,
				 struct crypto_bignum *pwe)
{
	u8 pwd_value[SAE_MAX_PRIME_LEN];
	size_t bits = sae->tmp->prime_len * 8;
	u8 exp[1];
	struct crypto_bignum *a, *b = NULL;
	int res, is_val;
	u8 pwd_value_valid;

	wpa_hexdump_key(MSG_DEBUG, "SAE: pwd-seed", pwd_seed, SHA256_MAC_LEN);

	/* pwd-value = KDF-z(pwd-seed, "SAE Hunting and Pecking", p) */
	if (sha256_prf_bits(pwd_seed, SHA256_MAC_LEN, "SAE Hunting and Pecking",
			    sae->tmp->dh->prime, sae->tmp->prime_len, pwd_value,
			    bits) < 0)
		return -1;
	wpa_hexdump_key(MSG_DEBUG, "SAE: pwd-value", pwd_value,
			sae->tmp->prime_len);

	/* Check whether pwd-value < p */
	res = const_time_memcmp(pwd_value, sae->tmp->dh->prime,
				sae->tmp->prime_len);
	/* pwd-value >= p is invalid, so res is < 0 for the valid cases and
	 * the negative sign can be used to fill the mask for constant time
	 * selection */
	pwd_value_valid = const_time_fill_msb(res);

	/* If pwd-value >= p, force pwd-value to be < p and perform the
	 * calculations anyway to hide timing difference. The derived PWE will
	 * be ignored in that case. */
	pwd_value[0] = const_time_select_u8(pwd_value_valid, pwd_value[0], 0);

	/* PWE = pwd-value^((p-1)/r) modulo p */

	res = -1;
	a = crypto_bignum_init_set(pwd_value, sae->tmp->prime_len);
	if (!a)
		goto fail;

	/* This is an optimization based on the used group that does not depend
	 * on the password in any way, so it is fine to use separate branches
	 * for this step without constant time operations. */
	if (sae->tmp->dh->safe_prime) {
		/*
		 * r = (p-1)/2 for the group used here, so this becomes:
		 * PWE = pwd-value^2 modulo p
		 */
		exp[0] = 2;
		b = crypto_bignum_init_set(exp, sizeof(exp));
	} else {
		/* Calculate exponent: (p-1)/r */
		exp[0] = 1;
		b = crypto_bignum_init_set(exp, sizeof(exp));
		if (b == NULL ||
		    crypto_bignum_sub(sae->tmp->prime, b, b) < 0 ||
		    crypto_bignum_div(b, sae->tmp->order, b) < 0)
			goto fail;
	}

	if (!b)
		goto fail;

	res = crypto_bignum_exptmod(a, b, sae->tmp->prime, pwe);
	if (res < 0)
		goto fail;

	/* There were no fatal errors in calculations, so determine the return
	 * value using constant time operations. We get here for number of
	 * invalid cases which are cleared here after having performed all the
	 * computation. PWE is valid if pwd-value was less than prime and
	 * PWE > 1. Start with pwd-value check first and then use constant time
	 * operations to clear res to 0 if PWE is 0 or 1.
	 */
	res = const_time_select_u8(pwd_value_valid, 1, 0);
	is_val = crypto_bignum_is_zero(pwe);
	res = const_time_select_u8(const_time_is_zero(is_val), res, 0);
	is_val = crypto_bignum_is_one(pwe);
	res = const_time_select_u8(const_time_is_zero(is_val), res, 0);

fail:
	crypto_bignum_deinit(a, 1);
	crypto_bignum_deinit(b, 1);
	return res;
}


static int get_random_qr_qnr(const u8 *prime, size_t prime_len,
			     const struct crypto_bignum *prime_bn,
			     size_t prime_bits, struct crypto_bignum **qr,
			     struct crypto_bignum **qnr)
{
	*qr = NULL;
	*qnr = NULL;

	while (!(*qr) || !(*qnr)) {
		u8 tmp[SAE_MAX_ECC_PRIME_LEN];
		struct crypto_bignum *q;
		int res;

		if (random_get_bytes(tmp, prime_len) < 0)
			break;
		if (prime_bits % 8)
			buf_shift_right(tmp, prime_len, 8 - prime_bits % 8);
		if (os_memcmp(tmp, prime, prime_len) >= 0)
			continue;
		q = crypto_bignum_init_set(tmp, prime_len);
		if (!q)
			break;
		res = crypto_bignum_legendre(q, prime_bn);

		if (res == 1 && !(*qr))
			*qr = q;
		else if (res == -1 && !(*qnr))
			*qnr = q;
		else
			crypto_bignum_deinit(q, 0);
	}

	return (*qr && *qnr) ? 0 : -1;
}


static int sae_derive_pwe_ecc(struct sae_data *sae, const u8 *addr1,
			      const u8 *addr2, const u8 *password,
			      size_t password_len, const char *identifier)
{
	u8 counter, k = 40;
	u8 addrs[2 * ETH_ALEN];
	const u8 *addr[3];
	size_t len[3];
	size_t num_elem;
	u8 *dummy_password, *tmp_password;
	int pwd_seed_odd = 0;
	u8 prime[SAE_MAX_ECC_PRIME_LEN];
	size_t prime_len;
	struct crypto_bignum *x = NULL, *qr = NULL, *qnr = NULL;
	u8 x_bin[SAE_MAX_ECC_PRIME_LEN];
	u8 x_cand_bin[SAE_MAX_ECC_PRIME_LEN];
	u8 qr_bin[SAE_MAX_ECC_PRIME_LEN];
	u8 qnr_bin[SAE_MAX_ECC_PRIME_LEN];
	size_t bits;
	int res = -1;
	u8 found = 0; /* 0 (false) or 0xff (true) to be used as const_time_*
		       * mask */

	os_memset(x_bin, 0, sizeof(x_bin));

	dummy_password = os_malloc(password_len);
	tmp_password = os_malloc(password_len);
	if (!dummy_password || !tmp_password ||
	    random_get_bytes(dummy_password, password_len) < 0)
		goto fail;

	prime_len = sae->tmp->prime_len;
	if (crypto_bignum_to_bin(sae->tmp->prime, prime, sizeof(prime),
				 prime_len) < 0)
		goto fail;
	bits = crypto_ec_prime_len_bits(sae->tmp->ec);

	/*
	 * Create a random quadratic residue (qr) and quadratic non-residue
	 * (qnr) modulo p for blinding purposes during the loop.
	 */
	if (get_random_qr_qnr(prime, prime_len, sae->tmp->prime, bits,
			      &qr, &qnr) < 0 ||
	    crypto_bignum_to_bin(qr, qr_bin, sizeof(qr_bin), prime_len) < 0 ||
	    crypto_bignum_to_bin(qnr, qnr_bin, sizeof(qnr_bin), prime_len) < 0)
		goto fail;

	wpa_hexdump_ascii_key(MSG_DEBUG, "SAE: password",
			      password, password_len);
	if (identifier)
		wpa_printf(MSG_DEBUG, "SAE: password identifier: %s",
			   identifier);

	/*
	 * H(salt, ikm) = HMAC-SHA256(salt, ikm)
	 * base = password [|| identifier]
	 * pwd-seed = H(MAX(STA-A-MAC, STA-B-MAC) || MIN(STA-A-MAC, STA-B-MAC),
	 *              base || counter)
	 */
	sae_pwd_seed_key(addr1, addr2, addrs);

	addr[0] = tmp_password;
	len[0] = password_len;
	num_elem = 1;
	if (identifier) {
		addr[num_elem] = (const u8 *) identifier;
		len[num_elem] = os_strlen(identifier);
		num_elem++;
	}
	addr[num_elem] = &counter;
	len[num_elem] = sizeof(counter);
	num_elem++;

	/*
	 * Continue for at least k iterations to protect against side-channel
	 * attacks that attempt to determine the number of iterations required
	 * in the loop.
	 */
	for (counter = 1; counter <= k || !found; counter++) {
		u8 pwd_seed[SHA256_MAC_LEN];

		if (counter > 200) {
			/* This should not happen in practice */
			wpa_printf(MSG_DEBUG, "SAE: Failed to derive PWE");
			break;
		}

		wpa_printf(MSG_DEBUG, "SAE: counter = %03u", counter);
		const_time_select_bin(found, dummy_password, password,
				      password_len, tmp_password);
		if (hmac_sha256_vector(addrs, sizeof(addrs), num_elem,
				       addr, len, pwd_seed) < 0)
			break;

		res = sae_test_pwd_seed_ecc(sae, pwd_seed,
					    prime, qr_bin, qnr_bin, x_cand_bin);
		const_time_select_bin(found, x_bin, x_cand_bin, prime_len,
				      x_bin);
		pwd_seed_odd = const_time_select_u8(
			found, pwd_seed_odd,
			pwd_seed[SHA256_MAC_LEN - 1] & 0x01);
		os_memset(pwd_seed, 0, sizeof(pwd_seed));
		if (res < 0)
			goto fail;
		/* Need to minimize differences in handling res == 0 and 1 here
		 * to avoid differences in timing and instruction cache access,
		 * so use const_time_select_*() to make local copies of the
		 * values based on whether this loop iteration was the one that
		 * found the pwd-seed/x. */

		/* found is 0 or 0xff here and res is 0 or 1. Bitwise OR of them
		 * (with res converted to 0/0xff) handles this in constant time.
		 */
		found |= res * 0xff;
		wpa_printf(MSG_DEBUG, "SAE: pwd-seed result %d found=0x%02x",
			   res, found);
	}

	if (!found) {
		wpa_printf(MSG_DEBUG, "SAE: Could not generate PWE");
		res = -1;
		goto fail;
	}

	x = crypto_bignum_init_set(x_bin, prime_len);
	if (!x) {
		res = -1;
		goto fail;
	}

	if (!sae->tmp->pwe_ecc)
		sae->tmp->pwe_ecc = crypto_ec_point_init(sae->tmp->ec);
	if (!sae->tmp->pwe_ecc)
		res = -1;
	else
		res = crypto_ec_point_solve_y_coord(sae->tmp->ec,
						    sae->tmp->pwe_ecc, x,
						    pwd_seed_odd);
	if (res < 0) {
		/*
		 * This should not happen since we already checked that there
		 * is a result.
		 */
		wpa_printf(MSG_DEBUG, "SAE: Could not solve y");
	}

fail:
	crypto_bignum_deinit(qr, 0);
	crypto_bignum_deinit(qnr, 0);
	os_free(dummy_password);
	bin_clear_free(tmp_password, password_len);
	crypto_bignum_deinit(x, 1);
	os_memset(x_bin, 0, sizeof(x_bin));
	os_memset(x_cand_bin, 0, sizeof(x_cand_bin));

	return res;
}


static int sae_modp_group_require_masking(int group)
{
	/* Groups for which pwd-value is likely to be >= p frequently */
	return group == 22 || group == 23 || group == 24;
}


static int sae_derive_pwe_ffc(struct sae_data *sae, const u8 *addr1,
			      const u8 *addr2, const u8 *password,
			      size_t password_len, const char *identifier)
{
	u8 counter, k, sel_counter = 0;
	u8 addrs[2 * ETH_ALEN];
	const u8 *addr[3];
	size_t len[3];
	size_t num_elem;
	u8 found = 0; /* 0 (false) or 0xff (true) to be used as const_time_*
		       * mask */
	u8 mask;
	struct crypto_bignum *pwe;
	size_t prime_len = sae->tmp->prime_len * 8;
	u8 *pwe_buf;

	crypto_bignum_deinit(sae->tmp->pwe_ffc, 1);
	sae->tmp->pwe_ffc = NULL;

	/* Allocate a buffer to maintain selected and candidate PWE for constant
	 * time selection. */
	pwe_buf = os_zalloc(prime_len * 2);
	pwe = crypto_bignum_init();
	if (!pwe_buf || !pwe)
		goto fail;

	wpa_hexdump_ascii_key(MSG_DEBUG, "SAE: password",
			      password, password_len);

	/*
	 * H(salt, ikm) = HMAC-SHA256(salt, ikm)
	 * pwd-seed = H(MAX(STA-A-MAC, STA-B-MAC) || MIN(STA-A-MAC, STA-B-MAC),
	 *              password [|| identifier] || counter)
	 */
	sae_pwd_seed_key(addr1, addr2, addrs);

	addr[0] = password;
	len[0] = password_len;
	num_elem = 1;
	if (identifier) {
		addr[num_elem] = (const u8 *) identifier;
		len[num_elem] = os_strlen(identifier);
		num_elem++;
	}
	addr[num_elem] = &counter;
	len[num_elem] = sizeof(counter);
	num_elem++;

	k = sae_modp_group_require_masking(sae->group) ? 40 : 1;

	for (counter = 1; counter <= k || !found; counter++) {
		u8 pwd_seed[SHA256_MAC_LEN];
		int res;

		if (counter > 200) {
			/* This should not happen in practice */
			wpa_printf(MSG_DEBUG, "SAE: Failed to derive PWE");
			break;
		}

		wpa_printf(MSG_DEBUG, "SAE: counter = %02u", counter);
		if (hmac_sha256_vector(addrs, sizeof(addrs), num_elem,
				       addr, len, pwd_seed) < 0)
			break;
		res = sae_test_pwd_seed_ffc(sae, pwd_seed, pwe);
		/* res is -1 for fatal failure, 0 if a valid PWE was not found,
		 * or 1 if a valid PWE was found. */
		if (res < 0)
			break;
		/* Store the candidate PWE into the second half of pwe_buf and
		 * the selected PWE in the beginning of pwe_buf using constant
		 * time selection. */
		if (crypto_bignum_to_bin(pwe, pwe_buf + prime_len, prime_len,
					 prime_len) < 0)
			break;
		const_time_select_bin(found, pwe_buf, pwe_buf + prime_len,
				      prime_len, pwe_buf);
		sel_counter = const_time_select_u8(found, sel_counter, counter);
		mask = const_time_eq_u8(res, 1);
		found = const_time_select_u8(found, found, mask);
	}

	if (!found)
		goto fail;

	wpa_printf(MSG_DEBUG, "SAE: Use PWE from counter = %02u", sel_counter);
	sae->tmp->pwe_ffc = crypto_bignum_init_set(pwe_buf, prime_len);
fail:
	crypto_bignum_deinit(pwe, 1);
	bin_clear_free(pwe_buf, prime_len * 2);
	return sae->tmp->pwe_ffc ? 0 : -1;
}


static int sae_derive_commit_element_ecc(struct sae_data *sae,
					 struct crypto_bignum *mask)
{
	/* COMMIT-ELEMENT = inverse(scalar-op(mask, PWE)) */
	if (!sae->tmp->own_commit_element_ecc) {
		sae->tmp->own_commit_element_ecc =
			crypto_ec_point_init(sae->tmp->ec);
		if (!sae->tmp->own_commit_element_ecc)
			return -1;
	}

	if (crypto_ec_point_mul(sae->tmp->ec, sae->tmp->pwe_ecc, mask,
				sae->tmp->own_commit_element_ecc) < 0 ||
	    crypto_ec_point_invert(sae->tmp->ec,
				   sae->tmp->own_commit_element_ecc) < 0) {
		wpa_printf(MSG_DEBUG, "SAE: Could not compute commit-element");
		return -1;
	}

	return 0;
}


static int sae_derive_commit_element_ffc(struct sae_data *sae,
					 struct crypto_bignum *mask)
{
	/* COMMIT-ELEMENT = inverse(scalar-op(mask, PWE)) */
	if (!sae->tmp->own_commit_element_ffc) {
		sae->tmp->own_commit_element_ffc = crypto_bignum_init();
		if (!sae->tmp->own_commit_element_ffc)
			return -1;
	}

	if (crypto_bignum_exptmod(sae->tmp->pwe_ffc, mask, sae->tmp->prime,
				  sae->tmp->own_commit_element_ffc) < 0 ||
	    crypto_bignum_inverse(sae->tmp->own_commit_element_ffc,
				  sae->tmp->prime,
				  sae->tmp->own_commit_element_ffc) < 0) {
		wpa_printf(MSG_DEBUG, "SAE: Could not compute commit-element");
		return -1;
	}

	return 0;
}


static int sae_derive_commit(struct sae_data *sae)
{
	struct crypto_bignum *mask;
	int ret = -1;
	unsigned int counter = 0;

	do {
		counter++;
		if (counter > 100) {
			/*
			 * This cannot really happen in practice if the random
			 * number generator is working. Anyway, to avoid even a
			 * theoretical infinite loop, break out after 100
			 * attemps.
			 */
			return -1;
		}

		mask = sae_get_rand_and_mask(sae);
		if (mask == NULL) {
			wpa_printf(MSG_DEBUG, "SAE: Could not get rand/mask");
			return -1;
		}

		/* commit-scalar = (rand + mask) modulo r */
		if (!sae->tmp->own_commit_scalar) {
			sae->tmp->own_commit_scalar = crypto_bignum_init();
			if (!sae->tmp->own_commit_scalar)
				goto fail;
		}
		crypto_bignum_add(sae->tmp->sae_rand, mask,
				  sae->tmp->own_commit_scalar);
		crypto_bignum_mod(sae->tmp->own_commit_scalar, sae->tmp->order,
				  sae->tmp->own_commit_scalar);
	} while (crypto_bignum_is_zero(sae->tmp->own_commit_scalar) ||
		 crypto_bignum_is_one(sae->tmp->own_commit_scalar));

	if ((sae->tmp->ec && sae_derive_commit_element_ecc(sae, mask) < 0) ||
	    (sae->tmp->dh && sae_derive_commit_element_ffc(sae, mask) < 0))
		goto fail;

	ret = 0;
fail:
	crypto_bignum_deinit(mask, 1);
	return ret;
}


int sae_prepare_commit(const u8 *addr1, const u8 *addr2,
		       const u8 *password, size_t password_len,
		       const char *identifier, struct sae_data *sae)
{
	if (sae->tmp == NULL ||
	    (sae->tmp->ec && sae_derive_pwe_ecc(sae, addr1, addr2, password,
						password_len,
						identifier) < 0) ||
	    (sae->tmp->dh && sae_derive_pwe_ffc(sae, addr1, addr2, password,
						password_len,
						identifier) < 0) ||
	    sae_derive_commit(sae) < 0)
		return -1;
	return 0;
}


static int sae_derive_k_ecc(struct sae_data *sae, u8 *k)
{
	struct crypto_ec_point *K;
	int ret = -1;

	K = crypto_ec_point_init(sae->tmp->ec);
	if (K == NULL)
		goto fail;

	/*
	 * K = scalar-op(rand, (elem-op(scalar-op(peer-commit-scalar, PWE),
	 *                                        PEER-COMMIT-ELEMENT)))
	 * If K is identity element (point-at-infinity), reject
	 * k = F(K) (= x coordinate)
	 */

	if (crypto_ec_point_mul(sae->tmp->ec, sae->tmp->pwe_ecc,
				sae->peer_commit_scalar, K) < 0 ||
	    crypto_ec_point_add(sae->tmp->ec, K,
				sae->tmp->peer_commit_element_ecc, K) < 0 ||
	    crypto_ec_point_mul(sae->tmp->ec, K, sae->tmp->sae_rand, K) < 0 ||
	    crypto_ec_point_is_at_infinity(sae->tmp->ec, K) ||
	    crypto_ec_point_to_bin(sae->tmp->ec, K, k, NULL) < 0) {
		wpa_printf(MSG_DEBUG, "SAE: Failed to calculate K and k");
		goto fail;
	}

	wpa_hexdump_key(MSG_DEBUG, "SAE: k", k, sae->tmp->prime_len);

	ret = 0;
fail:
	crypto_ec_point_deinit(K, 1);
	return ret;
}


static int sae_derive_k_ffc(struct sae_data *sae, u8 *k)
{
	struct crypto_bignum *K;
	int ret = -1;

	K = crypto_bignum_init();
	if (K == NULL)
		goto fail;

	/*
	 * K = scalar-op(rand, (elem-op(scalar-op(peer-commit-scalar, PWE),
	 *                                        PEER-COMMIT-ELEMENT)))
	 * If K is identity element (one), reject.
	 * k = F(K) (= x coordinate)
	 */

	if (crypto_bignum_exptmod(sae->tmp->pwe_ffc, sae->peer_commit_scalar,
				  sae->tmp->prime, K) < 0 ||
	    crypto_bignum_mulmod(K, sae->tmp->peer_commit_element_ffc,
				 sae->tmp->prime, K) < 0 ||
	    crypto_bignum_exptmod(K, sae->tmp->sae_rand, sae->tmp->prime, K) < 0
	    ||
	    crypto_bignum_is_one(K) ||
	    crypto_bignum_to_bin(K, k, SAE_MAX_PRIME_LEN, sae->tmp->prime_len) <
	    0) {
		wpa_printf(MSG_DEBUG, "SAE: Failed to calculate K and k");
		goto fail;
	}

	wpa_hexdump_key(MSG_DEBUG, "SAE: k", k, sae->tmp->prime_len);

	ret = 0;
fail:
	crypto_bignum_deinit(K, 1);
	return ret;
}


static int sae_derive_keys(struct sae_data *sae, const u8 *k)
{
	u8 null_key[SAE_KEYSEED_KEY_LEN], val[SAE_MAX_PRIME_LEN];
	u8 keyseed[SHA256_MAC_LEN];
	u8 keys[SAE_KCK_LEN + SAE_PMK_LEN];
	struct crypto_bignum *tmp;
	int ret = -1;

	tmp = crypto_bignum_init();
	if (tmp == NULL)
		goto fail;

	/* keyseed = H(<0>32, k)
	 * KCK || PMK = KDF-512(keyseed, "SAE KCK and PMK",
	 *                      (commit-scalar + peer-commit-scalar) modulo r)
	 * PMKID = L((commit-scalar + peer-commit-scalar) modulo r, 0, 128)
	 */

	os_memset(null_key, 0, sizeof(null_key));
	hmac_sha256(null_key, sizeof(null_key), k, sae->tmp->prime_len,
		    keyseed);
	wpa_hexdump_key(MSG_DEBUG, "SAE: keyseed", keyseed, sizeof(keyseed));

	crypto_bignum_add(sae->tmp->own_commit_scalar, sae->peer_commit_scalar,
			  tmp);
	crypto_bignum_mod(tmp, sae->tmp->order, tmp);
	crypto_bignum_to_bin(tmp, val, sizeof(val), sae->tmp->prime_len);
	wpa_hexdump(MSG_DEBUG, "SAE: PMKID", val, SAE_PMKID_LEN);
	if (sha256_prf(keyseed, sizeof(keyseed), "SAE KCK and PMK",
		       val, sae->tmp->prime_len, keys, sizeof(keys)) < 0)
		goto fail;
	os_memset(keyseed, 0, sizeof(keyseed));
	os_memcpy(sae->tmp->kck, keys, SAE_KCK_LEN);
	os_memcpy(sae->pmk, keys + SAE_KCK_LEN, SAE_PMK_LEN);
	os_memcpy(sae->pmkid, val, SAE_PMKID_LEN);
	os_memset(keys, 0, sizeof(keys));
	wpa_hexdump_key(MSG_DEBUG, "SAE: KCK", sae->tmp->kck, SAE_KCK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "SAE: PMK", sae->pmk, SAE_PMK_LEN);

	ret = 0;
fail:
	crypto_bignum_deinit(tmp, 0);
	return ret;
}


int sae_process_commit(struct sae_data *sae)
{
	u8 k[SAE_MAX_PRIME_LEN];
	if (sae->tmp == NULL ||
	    (sae->tmp->ec && sae_derive_k_ecc(sae, k) < 0) ||
	    (sae->tmp->dh && sae_derive_k_ffc(sae, k) < 0) ||
	    sae_derive_keys(sae, k) < 0)
		return -1;
	return 0;
}


void sae_write_commit(struct sae_data *sae, struct wpabuf *buf,
		      const struct wpabuf *token, const char *identifier)
{
	u8 *pos;

	if (sae->tmp == NULL)
		return;

	wpabuf_put_le16(buf, sae->group); /* Finite Cyclic Group */
	if (token) {
		wpabuf_put_buf(buf, token);
		wpa_hexdump(MSG_DEBUG, "SAE: Anti-clogging token",
			    wpabuf_head(token), wpabuf_len(token));
	}
	pos = wpabuf_put(buf, sae->tmp->prime_len);
	crypto_bignum_to_bin(sae->tmp->own_commit_scalar, pos,
			     sae->tmp->prime_len, sae->tmp->prime_len);
	wpa_hexdump(MSG_DEBUG, "SAE: own commit-scalar",
		    pos, sae->tmp->prime_len);
	if (sae->tmp->ec) {
		pos = wpabuf_put(buf, 2 * sae->tmp->prime_len);
		crypto_ec_point_to_bin(sae->tmp->ec,
				       sae->tmp->own_commit_element_ecc,
				       pos, pos + sae->tmp->prime_len);
		wpa_hexdump(MSG_DEBUG, "SAE: own commit-element(x)",
			    pos, sae->tmp->prime_len);
		wpa_hexdump(MSG_DEBUG, "SAE: own commit-element(y)",
			    pos + sae->tmp->prime_len, sae->tmp->prime_len);
	} else {
		pos = wpabuf_put(buf, sae->tmp->prime_len);
		crypto_bignum_to_bin(sae->tmp->own_commit_element_ffc, pos,
				     sae->tmp->prime_len, sae->tmp->prime_len);
		wpa_hexdump(MSG_DEBUG, "SAE: own commit-element",
			    pos, sae->tmp->prime_len);
	}

	if (identifier) {
		/* Password Identifier element */
		wpabuf_put_u8(buf, WLAN_EID_EXTENSION);
		wpabuf_put_u8(buf, 1 + os_strlen(identifier));
		wpabuf_put_u8(buf, WLAN_EID_EXT_PASSWORD_IDENTIFIER);
		wpabuf_put_str(buf, identifier);
		wpa_printf(MSG_DEBUG, "SAE: own Password Identifier: %s",
			   identifier);
	}
}


u16 sae_group_allowed(struct sae_data *sae, int *allowed_groups, u16 group)
{
	if (allowed_groups) {
		int i;
		for (i = 0; allowed_groups[i] > 0; i++) {
			if (allowed_groups[i] == group)
				break;
		}
		if (allowed_groups[i] != group) {
			wpa_printf(MSG_DEBUG, "SAE: Proposed group %u not "
				   "enabled in the current configuration",
				   group);
			return WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;
		}
	}

	if (sae->state == SAE_COMMITTED && group != sae->group) {
		wpa_printf(MSG_DEBUG, "SAE: Do not allow group to be changed");
		return WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;
	}

	if (group != sae->group && sae_set_group(sae, group) < 0) {
		wpa_printf(MSG_DEBUG, "SAE: Unsupported Finite Cyclic Group %u",
			   group);
		return WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;
	}

	if (sae->tmp == NULL) {
		wpa_printf(MSG_DEBUG, "SAE: Group information not yet initialized");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	if (sae->tmp->dh && !allowed_groups) {
		wpa_printf(MSG_DEBUG, "SAE: Do not allow FFC group %u without "
			   "explicit configuration enabling it", group);
		return WLAN_STATUS_FINITE_CYCLIC_GROUP_NOT_SUPPORTED;
	}

	return WLAN_STATUS_SUCCESS;
}


static int sae_is_password_id_elem(const u8 *pos, const u8 *end)
{
	return end - pos >= 3 &&
		pos[0] == WLAN_EID_EXTENSION &&
		pos[1] >= 1 &&
		end - pos - 2 >= pos[1] &&
		pos[2] == WLAN_EID_EXT_PASSWORD_IDENTIFIER;
}


static void sae_parse_commit_token(struct sae_data *sae, const u8 **pos,
				   const u8 *end, const u8 **token,
				   size_t *token_len)
{
	size_t scalar_elem_len, tlen;
	const u8 *elem;

	if (token)
		*token = NULL;
	if (token_len)
		*token_len = 0;

	scalar_elem_len = (sae->tmp->ec ? 3 : 2) * sae->tmp->prime_len;
	if (scalar_elem_len >= (size_t) (end - *pos))
		return; /* No extra data beyond peer scalar and element */

	/* It is a bit difficult to parse this now that there is an
	 * optional variable length Anti-Clogging Token field and
	 * optional variable length Password Identifier element in the
	 * frame. We are sending out fixed length Anti-Clogging Token
	 * fields, so use that length as a requirement for the received
	 * token and check for the presence of possible Password
	 * Identifier element based on the element header information.
	 */
	tlen = end - (*pos + scalar_elem_len);

	if (tlen < SHA256_MAC_LEN) {
		wpa_printf(MSG_DEBUG,
			   "SAE: Too short optional data (%u octets) to include our Anti-Clogging Token",
			   (unsigned int) tlen);
		return;
	}

	elem = *pos + scalar_elem_len;
	if (sae_is_password_id_elem(elem, end)) {
		 /* Password Identifier element takes out all available
		  * extra octets, so there can be no Anti-Clogging token in
		  * this frame. */
		return;
	}

	elem += SHA256_MAC_LEN;
	if (sae_is_password_id_elem(elem, end)) {
		 /* Password Identifier element is included in the end, so
		  * remove its length from the Anti-Clogging token field. */
		tlen -= 2 + elem[1];
	}

	wpa_hexdump(MSG_DEBUG, "SAE: Anti-Clogging Token", *pos, tlen);
	if (token)
		*token = *pos;
	if (token_len)
		*token_len = tlen;
	*pos += tlen;
}


static u16 sae_parse_commit_scalar(struct sae_data *sae, const u8 **pos,
				   const u8 *end)
{
	struct crypto_bignum *peer_scalar;

	if (sae->tmp->prime_len > end - *pos) {
		wpa_printf(MSG_DEBUG, "SAE: Not enough data for scalar");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	peer_scalar = crypto_bignum_init_set(*pos, sae->tmp->prime_len);
	if (peer_scalar == NULL)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	/*
	 * IEEE Std 802.11-2012, 11.3.8.6.1: If there is a protocol instance for
	 * the peer and it is in Authenticated state, the new Commit Message
	 * shall be dropped if the peer-scalar is identical to the one used in
	 * the existing protocol instance.
	 */
	if (sae->state == SAE_ACCEPTED && sae->peer_commit_scalar &&
	    crypto_bignum_cmp(sae->peer_commit_scalar, peer_scalar) == 0) {
		wpa_printf(MSG_DEBUG, "SAE: Do not accept re-use of previous "
			   "peer-commit-scalar");
		crypto_bignum_deinit(peer_scalar, 0);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	/* 1 < scalar < r */
	if (crypto_bignum_is_zero(peer_scalar) ||
	    crypto_bignum_is_one(peer_scalar) ||
	    crypto_bignum_cmp(peer_scalar, sae->tmp->order) >= 0) {
		wpa_printf(MSG_DEBUG, "SAE: Invalid peer scalar");
		crypto_bignum_deinit(peer_scalar, 0);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}


	crypto_bignum_deinit(sae->peer_commit_scalar, 0);
	sae->peer_commit_scalar = peer_scalar;
	wpa_hexdump(MSG_DEBUG, "SAE: Peer commit-scalar",
		    *pos, sae->tmp->prime_len);
	*pos += sae->tmp->prime_len;

	return WLAN_STATUS_SUCCESS;
}


static u16 sae_parse_commit_element_ecc(struct sae_data *sae, const u8 **pos,
					const u8 *end)
{
	u8 prime[SAE_MAX_ECC_PRIME_LEN];

	if (2 * sae->tmp->prime_len > end - *pos) {
		wpa_printf(MSG_DEBUG, "SAE: Not enough data for "
			   "commit-element");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	if (crypto_bignum_to_bin(sae->tmp->prime, prime, sizeof(prime),
				 sae->tmp->prime_len) < 0)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	/* element x and y coordinates < p */
	if (os_memcmp(*pos, prime, sae->tmp->prime_len) >= 0 ||
	    os_memcmp(*pos + sae->tmp->prime_len, prime,
		      sae->tmp->prime_len) >= 0) {
		wpa_printf(MSG_DEBUG, "SAE: Invalid coordinates in peer "
			   "element");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	wpa_hexdump(MSG_DEBUG, "SAE: Peer commit-element(x)",
		    *pos, sae->tmp->prime_len);
	wpa_hexdump(MSG_DEBUG, "SAE: Peer commit-element(y)",
		    *pos + sae->tmp->prime_len, sae->tmp->prime_len);

	crypto_ec_point_deinit(sae->tmp->peer_commit_element_ecc, 0);
	sae->tmp->peer_commit_element_ecc =
		crypto_ec_point_from_bin(sae->tmp->ec, *pos);
	if (sae->tmp->peer_commit_element_ecc == NULL)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;

	if (!crypto_ec_point_is_on_curve(sae->tmp->ec,
					 sae->tmp->peer_commit_element_ecc)) {
		wpa_printf(MSG_DEBUG, "SAE: Peer element is not on curve");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}

	*pos += 2 * sae->tmp->prime_len;

	return WLAN_STATUS_SUCCESS;
}


static u16 sae_parse_commit_element_ffc(struct sae_data *sae, const u8 **pos,
					const u8 *end)
{
	struct crypto_bignum *res, *one;
	const u8 one_bin[1] = { 0x01 };

	if (sae->tmp->prime_len > end - *pos) {
		wpa_printf(MSG_DEBUG, "SAE: Not enough data for "
			   "commit-element");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
	wpa_hexdump(MSG_DEBUG, "SAE: Peer commit-element", *pos,
		    sae->tmp->prime_len);

	crypto_bignum_deinit(sae->tmp->peer_commit_element_ffc, 0);
	sae->tmp->peer_commit_element_ffc =
		crypto_bignum_init_set(*pos, sae->tmp->prime_len);
	if (sae->tmp->peer_commit_element_ffc == NULL)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	/* 1 < element < p - 1 */
	res = crypto_bignum_init();
	one = crypto_bignum_init_set(one_bin, sizeof(one_bin));
	if (!res || !one ||
	    crypto_bignum_sub(sae->tmp->prime, one, res) ||
	    crypto_bignum_is_zero(sae->tmp->peer_commit_element_ffc) ||
	    crypto_bignum_is_one(sae->tmp->peer_commit_element_ffc) ||
	    crypto_bignum_cmp(sae->tmp->peer_commit_element_ffc, res) >= 0) {
		crypto_bignum_deinit(res, 0);
		crypto_bignum_deinit(one, 0);
		wpa_printf(MSG_DEBUG, "SAE: Invalid peer element");
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
	crypto_bignum_deinit(one, 0);

	/* scalar-op(r, ELEMENT) = 1 modulo p */
	if (crypto_bignum_exptmod(sae->tmp->peer_commit_element_ffc,
				  sae->tmp->order, sae->tmp->prime, res) < 0 ||
	    !crypto_bignum_is_one(res)) {
		wpa_printf(MSG_DEBUG, "SAE: Invalid peer element (scalar-op)");
		crypto_bignum_deinit(res, 0);
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	}
	crypto_bignum_deinit(res, 0);

	*pos += sae->tmp->prime_len;

	return WLAN_STATUS_SUCCESS;
}


static u16 sae_parse_commit_element(struct sae_data *sae, const u8 **pos,
				    const u8 *end)
{
	if (sae->tmp->dh)
		return sae_parse_commit_element_ffc(sae, pos, end);
	return sae_parse_commit_element_ecc(sae, pos, end);
}


static int sae_parse_password_identifier(struct sae_data *sae,
					 const u8 *pos, const u8 *end)
{
	wpa_hexdump(MSG_DEBUG, "SAE: Possible elements at the end of the frame",
		    pos, end - pos);
	if (!sae_is_password_id_elem(pos, end)) {
		if (sae->tmp->pw_id) {
			wpa_printf(MSG_DEBUG,
				   "SAE: No Password Identifier included, but expected one (%s)",
				   sae->tmp->pw_id);
			return WLAN_STATUS_UNKNOWN_PASSWORD_IDENTIFIER;
		}
		os_free(sae->tmp->pw_id);
		sae->tmp->pw_id = NULL;
		return WLAN_STATUS_SUCCESS; /* No Password Identifier */
	}

	if (sae->tmp->pw_id &&
	    (pos[1] - 1 != (int) os_strlen(sae->tmp->pw_id) ||
	     os_memcmp(sae->tmp->pw_id, pos + 3, pos[1] - 1) != 0)) {
		wpa_printf(MSG_DEBUG,
			   "SAE: The included Password Identifier does not match the expected one (%s)",
			   sae->tmp->pw_id);
		return WLAN_STATUS_UNKNOWN_PASSWORD_IDENTIFIER;
	}

	os_free(sae->tmp->pw_id);
	sae->tmp->pw_id = os_malloc(pos[1]);
	if (!sae->tmp->pw_id)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	os_memcpy(sae->tmp->pw_id, pos + 3, pos[1] - 1);
	sae->tmp->pw_id[pos[1] - 1] = '\0';
	wpa_hexdump_ascii(MSG_DEBUG, "SAE: Received Password Identifier",
			  sae->tmp->pw_id, pos[1] -  1);
	return WLAN_STATUS_SUCCESS;
}


u16 sae_parse_commit(struct sae_data *sae, const u8 *data, size_t len,
		     const u8 **token, size_t *token_len, int *allowed_groups)
{
	const u8 *pos = data, *end = data + len;
	u16 res;

	/* Check Finite Cyclic Group */
	if (end - pos < 2)
		return WLAN_STATUS_UNSPECIFIED_FAILURE;
	res = sae_group_allowed(sae, allowed_groups, WPA_GET_LE16(pos));
	if (res != WLAN_STATUS_SUCCESS)
		return res;
	pos += 2;

	/* Optional Anti-Clogging Token */
	sae_parse_commit_token(sae, &pos, end, token, token_len);

	/* commit-scalar */
	res = sae_parse_commit_scalar(sae, &pos, end);
	if (res != WLAN_STATUS_SUCCESS)
		return res;

	/* commit-element */
	res = sae_parse_commit_element(sae, &pos, end);
	if (res != WLAN_STATUS_SUCCESS)
		return res;

	/* Optional Password Identifier element */
	res = sae_parse_password_identifier(sae, pos, end);
	if (res != WLAN_STATUS_SUCCESS)
		return res;

	/*
	 * Check whether peer-commit-scalar and PEER-COMMIT-ELEMENT are same as
	 * the values we sent which would be evidence of a reflection attack.
	 */
	if (!sae->tmp->own_commit_scalar ||
	    crypto_bignum_cmp(sae->tmp->own_commit_scalar,
			      sae->peer_commit_scalar) != 0 ||
	    (sae->tmp->dh &&
	     (!sae->tmp->own_commit_element_ffc ||
	      crypto_bignum_cmp(sae->tmp->own_commit_element_ffc,
				sae->tmp->peer_commit_element_ffc) != 0)) ||
	    (sae->tmp->ec &&
	     (!sae->tmp->own_commit_element_ecc ||
	      crypto_ec_point_cmp(sae->tmp->ec,
				  sae->tmp->own_commit_element_ecc,
				  sae->tmp->peer_commit_element_ecc) != 0)))
		return WLAN_STATUS_SUCCESS; /* scalars/elements are different */

	/*
	 * This is a reflection attack - return special value to trigger caller
	 * to silently discard the frame instead of replying with a specific
	 * status code.
	 */
	return SAE_SILENTLY_DISCARD;
}


static void sae_cn_confirm(struct sae_data *sae, const u8 *sc,
			   const struct crypto_bignum *scalar1,
			   const u8 *element1, size_t element1_len,
			   const struct crypto_bignum *scalar2,
			   const u8 *element2, size_t element2_len,
			   u8 *confirm)
{
	const u8 *addr[5];
	size_t len[5];
	u8 scalar_b1[SAE_MAX_PRIME_LEN], scalar_b2[SAE_MAX_PRIME_LEN];

	/* Confirm
	 * CN(key, X, Y, Z, ...) =
	 *    HMAC-SHA256(key, D2OS(X) || D2OS(Y) || D2OS(Z) | ...)
	 * confirm = CN(KCK, send-confirm, commit-scalar, COMMIT-ELEMENT,
	 *              peer-commit-scalar, PEER-COMMIT-ELEMENT)
	 * verifier = CN(KCK, peer-send-confirm, peer-commit-scalar,
	 *               PEER-COMMIT-ELEMENT, commit-scalar, COMMIT-ELEMENT)
	 */
	addr[0] = sc;
	len[0] = 2;
	crypto_bignum_to_bin(scalar1, scalar_b1, sizeof(scalar_b1),
			     sae->tmp->prime_len);
	addr[1] = scalar_b1;
	len[1] = sae->tmp->prime_len;
	addr[2] = element1;
	len[2] = element1_len;
	crypto_bignum_to_bin(scalar2, scalar_b2, sizeof(scalar_b2),
			     sae->tmp->prime_len);
	addr[3] = scalar_b2;
	len[3] = sae->tmp->prime_len;
	addr[4] = element2;
	len[4] = element2_len;
	hmac_sha256_vector(sae->tmp->kck, sizeof(sae->tmp->kck), 5, addr, len,
			   confirm);
}


static void sae_cn_confirm_ecc(struct sae_data *sae, const u8 *sc,
			       const struct crypto_bignum *scalar1,
			       const struct crypto_ec_point *element1,
			       const struct crypto_bignum *scalar2,
			       const struct crypto_ec_point *element2,
			       u8 *confirm)
{
	u8 element_b1[2 * SAE_MAX_ECC_PRIME_LEN];
	u8 element_b2[2 * SAE_MAX_ECC_PRIME_LEN];

	crypto_ec_point_to_bin(sae->tmp->ec, element1, element_b1,
			       element_b1 + sae->tmp->prime_len);
	crypto_ec_point_to_bin(sae->tmp->ec, element2, element_b2,
			       element_b2 + sae->tmp->prime_len);

	sae_cn_confirm(sae, sc, scalar1, element_b1, 2 * sae->tmp->prime_len,
		       scalar2, element_b2, 2 * sae->tmp->prime_len, confirm);
}


static void sae_cn_confirm_ffc(struct sae_data *sae, const u8 *sc,
			       const struct crypto_bignum *scalar1,
			       const struct crypto_bignum *element1,
			       const struct crypto_bignum *scalar2,
			       const struct crypto_bignum *element2,
			       u8 *confirm)
{
	u8 element_b1[SAE_MAX_PRIME_LEN];
	u8 element_b2[SAE_MAX_PRIME_LEN];

	crypto_bignum_to_bin(element1, element_b1, sizeof(element_b1),
			     sae->tmp->prime_len);
	crypto_bignum_to_bin(element2, element_b2, sizeof(element_b2),
			     sae->tmp->prime_len);

	sae_cn_confirm(sae, sc, scalar1, element_b1, sae->tmp->prime_len,
		       scalar2, element_b2, sae->tmp->prime_len, confirm);
}


void sae_write_confirm(struct sae_data *sae, struct wpabuf *buf)
{
	const u8 *sc;

	if (sae->tmp == NULL)
		return;

	/* Send-Confirm */
	sc = wpabuf_put(buf, 0);
	wpabuf_put_le16(buf, sae->send_confirm);
	if (sae->send_confirm < 0xffff)
		sae->send_confirm++;

	if (sae->tmp->ec)
		sae_cn_confirm_ecc(sae, sc, sae->tmp->own_commit_scalar,
				   sae->tmp->own_commit_element_ecc,
				   sae->peer_commit_scalar,
				   sae->tmp->peer_commit_element_ecc,
				   wpabuf_put(buf, SHA256_MAC_LEN));
	else
		sae_cn_confirm_ffc(sae, sc, sae->tmp->own_commit_scalar,
				   sae->tmp->own_commit_element_ffc,
				   sae->peer_commit_scalar,
				   sae->tmp->peer_commit_element_ffc,
				   wpabuf_put(buf, SHA256_MAC_LEN));
}


int sae_check_confirm(struct sae_data *sae, const u8 *data, size_t len)
{
	u8 verifier[SHA256_MAC_LEN];

	if (len < 2 + SHA256_MAC_LEN) {
		wpa_printf(MSG_DEBUG, "SAE: Too short confirm message");
		return -1;
	}

	wpa_printf(MSG_DEBUG, "SAE: peer-send-confirm %u", WPA_GET_LE16(data));

	if (!sae->tmp || !sae->peer_commit_scalar ||
	    !sae->tmp->own_commit_scalar) {
		wpa_printf(MSG_DEBUG, "SAE: Temporary data not yet available");
		return -1;
	}

	if (sae->tmp->ec) {
		if (!sae->tmp->peer_commit_element_ecc ||
		    !sae->tmp->own_commit_element_ecc)
			return -1;
		sae_cn_confirm_ecc(sae, data, sae->peer_commit_scalar,
				   sae->tmp->peer_commit_element_ecc,
				   sae->tmp->own_commit_scalar,
				   sae->tmp->own_commit_element_ecc,
				   verifier);
	} else {
		if (!sae->tmp->peer_commit_element_ffc ||
		    !sae->tmp->own_commit_element_ffc)
			return -1;
		sae_cn_confirm_ffc(sae, data, sae->peer_commit_scalar,
				   sae->tmp->peer_commit_element_ffc,
				   sae->tmp->own_commit_scalar,
				   sae->tmp->own_commit_element_ffc,
				   verifier);
	}

	if (os_memcmp_const(verifier, data + 2, SHA256_MAC_LEN) != 0) {
		wpa_printf(MSG_DEBUG, "SAE: Confirm mismatch");
		wpa_hexdump(MSG_DEBUG, "SAE: Received confirm",
			    data + 2, SHA256_MAC_LEN);
		wpa_hexdump(MSG_DEBUG, "SAE: Calculated verifier",
			    verifier, SHA256_MAC_LEN);
		return -1;
	}

	return 0;
}


const char * sae_state_txt(enum sae_state state)
{
	switch (state) {
	case SAE_NOTHING:
		return "Nothing";
	case SAE_COMMITTED:
		return "Committed";
	case SAE_CONFIRMED:
		return "Confirmed";
	case SAE_ACCEPTED:
		return "Accepted";
	}
	return "?";
}
