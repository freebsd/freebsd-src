/* $OpenBSD: dh.c,v 1.57 2015/05/27 23:39:18 dtucker Exp $ */
/*
 * Copyright (c) 2000 Niels Provos.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "includes.h"

#include <sys/param.h>	/* MIN */

#include <openssl/bn.h>
#include <openssl/dh.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "dh.h"
#include "pathnames.h"
#include "log.h"
#include "misc.h"
#include "ssherr.h"

static int
parse_prime(int linenum, char *line, struct dhgroup *dhg)
{
	char *cp, *arg;
	char *strsize, *gen, *prime;
	const char *errstr = NULL;
	long long n;

	dhg->p = dhg->g = NULL;
	cp = line;
	if ((arg = strdelim(&cp)) == NULL)
		return 0;
	/* Ignore leading whitespace */
	if (*arg == '\0')
		arg = strdelim(&cp);
	if (!arg || !*arg || *arg == '#')
		return 0;

	/* time */
	if (cp == NULL || *arg == '\0')
		goto truncated;
	arg = strsep(&cp, " "); /* type */
	if (cp == NULL || *arg == '\0')
		goto truncated;
	/* Ensure this is a safe prime */
	n = strtonum(arg, 0, 5, &errstr);
	if (errstr != NULL || n != MODULI_TYPE_SAFE) {
		error("moduli:%d: type is not %d", linenum, MODULI_TYPE_SAFE);
		goto fail;
	}
	arg = strsep(&cp, " "); /* tests */
	if (cp == NULL || *arg == '\0')
		goto truncated;
	/* Ensure prime has been tested and is not composite */
	n = strtonum(arg, 0, 0x1f, &errstr);
	if (errstr != NULL ||
	    (n & MODULI_TESTS_COMPOSITE) || !(n & ~MODULI_TESTS_COMPOSITE)) {
		error("moduli:%d: invalid moduli tests flag", linenum);
		goto fail;
	}
	arg = strsep(&cp, " "); /* tries */
	if (cp == NULL || *arg == '\0')
		goto truncated;
	n = strtonum(arg, 0, 1<<30, &errstr);
	if (errstr != NULL || n == 0) {
		error("moduli:%d: invalid primality trial count", linenum);
		goto fail;
	}
	strsize = strsep(&cp, " "); /* size */
	if (cp == NULL || *strsize == '\0' ||
	    (dhg->size = (int)strtonum(strsize, 0, 64*1024, &errstr)) == 0 ||
	    errstr) {
		error("moduli:%d: invalid prime length", linenum);
		goto fail;
	}
	/* The whole group is one bit larger */
	dhg->size++;
	gen = strsep(&cp, " "); /* gen */
	if (cp == NULL || *gen == '\0')
		goto truncated;
	prime = strsep(&cp, " "); /* prime */
	if (cp != NULL || *prime == '\0') {
 truncated:
		error("moduli:%d: truncated", linenum);
		goto fail;
	}

	if ((dhg->g = BN_new()) == NULL ||
	    (dhg->p = BN_new()) == NULL) {
		error("parse_prime: BN_new failed");
		goto fail;
	}
	if (BN_hex2bn(&dhg->g, gen) == 0) {
		error("moduli:%d: could not parse generator value", linenum);
		goto fail;
	}
	if (BN_hex2bn(&dhg->p, prime) == 0) {
		error("moduli:%d: could not parse prime value", linenum);
		goto fail;
	}
	if (BN_num_bits(dhg->p) != dhg->size) {
		error("moduli:%d: prime has wrong size: actual %d listed %d",
		    linenum, BN_num_bits(dhg->p), dhg->size - 1);
		goto fail;
	}
	if (BN_cmp(dhg->g, BN_value_one()) <= 0) {
		error("moduli:%d: generator is invalid", linenum);
		goto fail;
	}
	return 1;

 fail:
	if (dhg->g != NULL)
		BN_clear_free(dhg->g);
	if (dhg->p != NULL)
		BN_clear_free(dhg->p);
	dhg->g = dhg->p = NULL;
	return 0;
}

DH *
choose_dh(int min, int wantbits, int max)
{
	FILE *f;
	char line[4096];
	int best, bestcount, which;
	int linenum;
	struct dhgroup dhg;

	if ((f = fopen(_PATH_DH_MODULI, "r")) == NULL &&
	    (f = fopen(_PATH_DH_PRIMES, "r")) == NULL) {
		logit("WARNING: %s does not exist, using fixed modulus",
		    _PATH_DH_MODULI);
		return (dh_new_group_fallback(max));
	}

	linenum = 0;
	best = bestcount = 0;
	while (fgets(line, sizeof(line), f)) {
		linenum++;
		if (!parse_prime(linenum, line, &dhg))
			continue;
		BN_clear_free(dhg.g);
		BN_clear_free(dhg.p);

		if (dhg.size > max || dhg.size < min)
			continue;

		if ((dhg.size > wantbits && dhg.size < best) ||
		    (dhg.size > best && best < wantbits)) {
			best = dhg.size;
			bestcount = 0;
		}
		if (dhg.size == best)
			bestcount++;
	}
	rewind(f);

	if (bestcount == 0) {
		fclose(f);
		logit("WARNING: no suitable primes in %s", _PATH_DH_PRIMES);
		return (dh_new_group_fallback(max));
	}

	linenum = 0;
	which = arc4random_uniform(bestcount);
	while (fgets(line, sizeof(line), f)) {
		if (!parse_prime(linenum, line, &dhg))
			continue;
		if ((dhg.size > max || dhg.size < min) ||
		    dhg.size != best ||
		    linenum++ != which) {
			BN_clear_free(dhg.g);
			BN_clear_free(dhg.p);
			continue;
		}
		break;
	}
	fclose(f);
	if (linenum != which+1) {
		logit("WARNING: line %d disappeared in %s, giving up",
		    which, _PATH_DH_PRIMES);
		return (dh_new_group_fallback(max));
	}

	return (dh_new_group(dhg.g, dhg.p));
}

/* diffie-hellman-groupN-sha1 */

int
dh_pub_is_valid(DH *dh, BIGNUM *dh_pub)
{
	int i;
	int n = BN_num_bits(dh_pub);
	int bits_set = 0;
	BIGNUM *tmp;

	if (dh_pub->neg) {
		logit("invalid public DH value: negative");
		return 0;
	}
	if (BN_cmp(dh_pub, BN_value_one()) != 1) {	/* pub_exp <= 1 */
		logit("invalid public DH value: <= 1");
		return 0;
	}

	if ((tmp = BN_new()) == NULL) {
		error("%s: BN_new failed", __func__);
		return 0;
	}
	if (!BN_sub(tmp, dh->p, BN_value_one()) ||
	    BN_cmp(dh_pub, tmp) != -1) {		/* pub_exp > p-2 */
		BN_clear_free(tmp);
		logit("invalid public DH value: >= p-1");
		return 0;
	}
	BN_clear_free(tmp);

	for (i = 0; i <= n; i++)
		if (BN_is_bit_set(dh_pub, i))
			bits_set++;
	debug2("bits set: %d/%d", bits_set, BN_num_bits(dh->p));

	/* if g==2 and bits_set==1 then computing log_g(dh_pub) is trivial */
	if (bits_set > 1)
		return 1;

	logit("invalid public DH value (%d/%d)", bits_set, BN_num_bits(dh->p));
	return 0;
}

int
dh_gen_key(DH *dh, int need)
{
	int pbits;

	if (need < 0 || dh->p == NULL ||
	    (pbits = BN_num_bits(dh->p)) <= 0 ||
	    need > INT_MAX / 2 || 2 * need > pbits)
		return SSH_ERR_INVALID_ARGUMENT;
	dh->length = MIN(need * 2, pbits - 1);
	if (DH_generate_key(dh) == 0 ||
	    !dh_pub_is_valid(dh, dh->pub_key)) {
		BN_clear_free(dh->priv_key);
		return SSH_ERR_LIBCRYPTO_ERROR;
	}
	return 0;
}

DH *
dh_new_group_asc(const char *gen, const char *modulus)
{
	DH *dh;

	if ((dh = DH_new()) == NULL)
		return NULL;
	if (BN_hex2bn(&dh->p, modulus) == 0 ||
	    BN_hex2bn(&dh->g, gen) == 0) {
		DH_free(dh);
		return NULL;
	}
	return (dh);
}

/*
 * This just returns the group, we still need to generate the exchange
 * value.
 */

DH *
dh_new_group(BIGNUM *gen, BIGNUM *modulus)
{
	DH *dh;

	if ((dh = DH_new()) == NULL)
		return NULL;
	dh->p = modulus;
	dh->g = gen;

	return (dh);
}

DH *
dh_new_group1(void)
{
	static char *gen = "2", *group1 =
	    "FFFFFFFF" "FFFFFFFF" "C90FDAA2" "2168C234" "C4C6628B" "80DC1CD1"
	    "29024E08" "8A67CC74" "020BBEA6" "3B139B22" "514A0879" "8E3404DD"
	    "EF9519B3" "CD3A431B" "302B0A6D" "F25F1437" "4FE1356D" "6D51C245"
	    "E485B576" "625E7EC6" "F44C42E9" "A637ED6B" "0BFF5CB6" "F406B7ED"
	    "EE386BFB" "5A899FA5" "AE9F2411" "7C4B1FE6" "49286651" "ECE65381"
	    "FFFFFFFF" "FFFFFFFF";

	return (dh_new_group_asc(gen, group1));
}

DH *
dh_new_group14(void)
{
	static char *gen = "2", *group14 =
	    "FFFFFFFF" "FFFFFFFF" "C90FDAA2" "2168C234" "C4C6628B" "80DC1CD1"
	    "29024E08" "8A67CC74" "020BBEA6" "3B139B22" "514A0879" "8E3404DD"
	    "EF9519B3" "CD3A431B" "302B0A6D" "F25F1437" "4FE1356D" "6D51C245"
	    "E485B576" "625E7EC6" "F44C42E9" "A637ED6B" "0BFF5CB6" "F406B7ED"
	    "EE386BFB" "5A899FA5" "AE9F2411" "7C4B1FE6" "49286651" "ECE45B3D"
	    "C2007CB8" "A163BF05" "98DA4836" "1C55D39A" "69163FA8" "FD24CF5F"
	    "83655D23" "DCA3AD96" "1C62F356" "208552BB" "9ED52907" "7096966D"
	    "670C354E" "4ABC9804" "F1746C08" "CA18217C" "32905E46" "2E36CE3B"
	    "E39E772C" "180E8603" "9B2783A2" "EC07A28F" "B5C55DF0" "6F4C52C9"
	    "DE2BCBF6" "95581718" "3995497C" "EA956AE5" "15D22618" "98FA0510"
	    "15728E5A" "8AACAA68" "FFFFFFFF" "FFFFFFFF";

	return (dh_new_group_asc(gen, group14));
}

/*
 * 4k bit fallback group used by DH-GEX if moduli file cannot be read.
 * Source: MODP group 16 from RFC3526.
 */
DH *
dh_new_group_fallback(int max)
{
	static char *gen = "2", *group16 =
	    "FFFFFFFF" "FFFFFFFF" "C90FDAA2" "2168C234" "C4C6628B" "80DC1CD1"
	    "29024E08" "8A67CC74" "020BBEA6" "3B139B22" "514A0879" "8E3404DD"
	    "EF9519B3" "CD3A431B" "302B0A6D" "F25F1437" "4FE1356D" "6D51C245"
	    "E485B576" "625E7EC6" "F44C42E9" "A637ED6B" "0BFF5CB6" "F406B7ED"
	    "EE386BFB" "5A899FA5" "AE9F2411" "7C4B1FE6" "49286651" "ECE45B3D"
	    "C2007CB8" "A163BF05" "98DA4836" "1C55D39A" "69163FA8" "FD24CF5F"
	    "83655D23" "DCA3AD96" "1C62F356" "208552BB" "9ED52907" "7096966D"
	    "670C354E" "4ABC9804" "F1746C08" "CA18217C" "32905E46" "2E36CE3B"
	    "E39E772C" "180E8603" "9B2783A2" "EC07A28F" "B5C55DF0" "6F4C52C9"
	    "DE2BCBF6" "95581718" "3995497C" "EA956AE5" "15D22618" "98FA0510"
	    "15728E5A" "8AAAC42D" "AD33170D" "04507A33" "A85521AB" "DF1CBA64"
	    "ECFB8504" "58DBEF0A" "8AEA7157" "5D060C7D" "B3970F85" "A6E1E4C7"
	    "ABF5AE8C" "DB0933D7" "1E8C94E0" "4A25619D" "CEE3D226" "1AD2EE6B"
	    "F12FFA06" "D98A0864" "D8760273" "3EC86A64" "521F2B18" "177B200C"
	    "BBE11757" "7A615D6C" "770988C0" "BAD946E2" "08E24FA0" "74E5AB31"
	    "43DB5BFC" "E0FD108E" "4B82D120" "A9210801" "1A723C12" "A787E6D7"
	    "88719A10" "BDBA5B26" "99C32718" "6AF4E23C" "1A946834" "B6150BDA"
	    "2583E9CA" "2AD44CE8" "DBBBC2DB" "04DE8EF9" "2E8EFC14" "1FBECAA6"
	    "287C5947" "4E6BC05D" "99B2964F" "A090C3A2" "233BA186" "515BE7ED"
	    "1F612970" "CEE2D7AF" "B81BDD76" "2170481C" "D0069127" "D5B05AA9"
	    "93B4EA98" "8D8FDDC1" "86FFB7DC" "90A6C08F" "4DF435C9" "34063199"
	    "FFFFFFFF" "FFFFFFFF";

	if (max < 4096) {
		debug3("requested max size %d, using 2k bit group 14", max);
		return dh_new_group14();
	}
	debug3("using 4k bit group 16");
	return (dh_new_group_asc(gen, group16));
}

/*
 * Estimates the group order for a Diffie-Hellman group that has an
 * attack complexity approximately the same as O(2**bits).
 * Values from NIST Special Publication 800-57: Recommendation for Key
 * Management Part 1 (rev 3) limited by the recommended maximum value
 * from RFC4419 section 3.
 */

u_int
dh_estimate(int bits)
{
	if (bits <= 112)
		return 2048;
	if (bits <= 128)
		return 3072;
	if (bits <= 192)
		return 7680;
	return 8192;
}
