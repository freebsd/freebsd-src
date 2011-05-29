/* $OpenBSD: dh.c,v 1.48 2009/10/01 11:37:33 grunk Exp $ */
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

#include <sys/param.h>

#include <openssl/bn.h>
#include <openssl/dh.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dh.h"
#include "pathnames.h"
#include "log.h"
#include "misc.h"

static int
parse_prime(int linenum, char *line, struct dhgroup *dhg)
{
	char *cp, *arg;
	char *strsize, *gen, *prime;
	const char *errstr = NULL;
	long long n;

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
		goto fail;
	arg = strsep(&cp, " "); /* type */
	if (cp == NULL || *arg == '\0')
		goto fail;
	/* Ensure this is a safe prime */
	n = strtonum(arg, 0, 5, &errstr);
	if (errstr != NULL || n != MODULI_TYPE_SAFE)
		goto fail;
	arg = strsep(&cp, " "); /* tests */
	if (cp == NULL || *arg == '\0')
		goto fail;
	/* Ensure prime has been tested and is not composite */
	n = strtonum(arg, 0, 0x1f, &errstr);
	if (errstr != NULL ||
	    (n & MODULI_TESTS_COMPOSITE) || !(n & ~MODULI_TESTS_COMPOSITE))
		goto fail;
	arg = strsep(&cp, " "); /* tries */
	if (cp == NULL || *arg == '\0')
		goto fail;
	n = strtonum(arg, 0, 1<<30, &errstr);
	if (errstr != NULL || n == 0)
		goto fail;
	strsize = strsep(&cp, " "); /* size */
	if (cp == NULL || *strsize == '\0' ||
	    (dhg->size = (int)strtonum(strsize, 0, 64*1024, &errstr)) == 0 ||
	    errstr)
		goto fail;
	/* The whole group is one bit larger */
	dhg->size++;
	gen = strsep(&cp, " "); /* gen */
	if (cp == NULL || *gen == '\0')
		goto fail;
	prime = strsep(&cp, " "); /* prime */
	if (cp != NULL || *prime == '\0')
		goto fail;

	if ((dhg->g = BN_new()) == NULL)
		fatal("parse_prime: BN_new failed");
	if ((dhg->p = BN_new()) == NULL)
		fatal("parse_prime: BN_new failed");
	if (BN_hex2bn(&dhg->g, gen) == 0)
		goto failclean;

	if (BN_hex2bn(&dhg->p, prime) == 0)
		goto failclean;

	if (BN_num_bits(dhg->p) != dhg->size)
		goto failclean;

	if (BN_is_zero(dhg->g) || BN_is_one(dhg->g))
		goto failclean;

	return (1);

 failclean:
	BN_clear_free(dhg->g);
	BN_clear_free(dhg->p);
 fail:
	error("Bad prime description in line %d", linenum);
	return (0);
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
		return (dh_new_group14());
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
		return (dh_new_group14());
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
	if (linenum != which+1)
		fatal("WARNING: line %d disappeared in %s, giving up",
		    which, _PATH_DH_PRIMES);

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

void
dh_gen_key(DH *dh, int need)
{
	int i, bits_set, tries = 0;

	if (dh->p == NULL)
		fatal("dh_gen_key: dh->p == NULL");
	if (need > INT_MAX / 2 || 2 * need >= BN_num_bits(dh->p))
		fatal("dh_gen_key: group too small: %d (2*need %d)",
		    BN_num_bits(dh->p), 2*need);
	do {
		if (dh->priv_key != NULL)
			BN_clear_free(dh->priv_key);
		if ((dh->priv_key = BN_new()) == NULL)
			fatal("dh_gen_key: BN_new failed");
		/* generate a 2*need bits random private exponent */
		if (!BN_rand(dh->priv_key, 2*need, 0, 0))
			fatal("dh_gen_key: BN_rand failed");
		if (DH_generate_key(dh) == 0)
			fatal("DH_generate_key");
		for (i = 0, bits_set = 0; i <= BN_num_bits(dh->priv_key); i++)
			if (BN_is_bit_set(dh->priv_key, i))
				bits_set++;
		debug2("dh_gen_key: priv key bits set: %d/%d",
		    bits_set, BN_num_bits(dh->priv_key));
		if (tries++ > 10)
			fatal("dh_gen_key: too many bad keys: giving up");
	} while (!dh_pub_is_valid(dh, dh->pub_key));
}

DH *
dh_new_group_asc(const char *gen, const char *modulus)
{
	DH *dh;

	if ((dh = DH_new()) == NULL)
		fatal("dh_new_group_asc: DH_new");

	if (BN_hex2bn(&dh->p, modulus) == 0)
		fatal("BN_hex2bn p");
	if (BN_hex2bn(&dh->g, gen) == 0)
		fatal("BN_hex2bn g");

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
		fatal("dh_new_group: DH_new");
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
 * Estimates the group order for a Diffie-Hellman group that has an
 * attack complexity approximately the same as O(2**bits).  Estimate
 * with:  O(exp(1.9223 * (ln q)^(1/3) (ln ln q)^(2/3)))
 */

int
dh_estimate(int bits)
{

	if (bits <= 128)
		return (1024);	/* O(2**86) */
	if (bits <= 192)
		return (2048);	/* O(2**116) */
	return (4096);		/* O(2**156) */
}
