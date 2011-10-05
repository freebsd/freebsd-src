/*
 * Copyright (c) 2006 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <dh.h>

#include <roken.h>

#include "tommath.h"

static void
BN2mpz(mp_int *s, const BIGNUM *bn)
{
    size_t len;
    void *p;

    len = BN_num_bytes(bn);
    p = malloc(len);
    BN_bn2bin(bn, p);
    mp_read_unsigned_bin(s, p, len);
    free(p);
}


static BIGNUM *
mpz2BN(mp_int *s)
{
    size_t size;
    BIGNUM *bn;
    void *p;

    size = mp_unsigned_bin_size(s);
    p = malloc(size);
    if (p == NULL && size != 0)
	return NULL;
    mp_to_unsigned_bin(s, p);

    bn = BN_bin2bn(p, size, NULL);
    free(p);
    return bn;
}

/*
 *
 */

#define DH_NUM_TRIES 10

static int
ltm_dh_generate_key(DH *dh)
{
    mp_int pub, priv_key, g, p;
    int have_private_key = (dh->priv_key != NULL);
    int codes, times = 0;
    int res;

    if (dh->p == NULL || dh->g == NULL)
	return 0;

    while (times++ < DH_NUM_TRIES) {
	if (!have_private_key) {
	    size_t bits = BN_num_bits(dh->p);

	    if (dh->priv_key)
		BN_free(dh->priv_key);

	    dh->priv_key = BN_new();
	    if (dh->priv_key == NULL)
		return 0;
	    if (!BN_rand(dh->priv_key, bits - 1, 0, 0)) {
		BN_clear_free(dh->priv_key);
		dh->priv_key = NULL;
		return 0;
	    }
	}
	if (dh->pub_key)
	    BN_free(dh->pub_key);

	mp_init_multi(&pub, &priv_key, &g, &p, NULL);

	BN2mpz(&priv_key, dh->priv_key);
	BN2mpz(&g, dh->g);
	BN2mpz(&p, dh->p);

	res = mp_exptmod(&g, &priv_key, &p, &pub);

	mp_clear_multi(&priv_key, &g, &p, NULL);
	if (res != 0)
	    continue;

	dh->pub_key = mpz2BN(&pub);
	mp_clear(&pub);
	if (dh->pub_key == NULL)
	    return 0;

	if (DH_check_pubkey(dh, dh->pub_key, &codes) && codes == 0)
	    break;
	if (have_private_key)
	    return 0;
    }

    if (times >= DH_NUM_TRIES) {
	if (!have_private_key && dh->priv_key) {
	    BN_free(dh->priv_key);
	    dh->priv_key = NULL;
	}
	if (dh->pub_key) {
	    BN_free(dh->pub_key);
	    dh->pub_key = NULL;
	}
	return 0;
    }

    return 1;
}

static int
ltm_dh_compute_key(unsigned char *shared, const BIGNUM * pub, DH *dh)
{
    mp_int s, priv_key, p, peer_pub;
    int ret;

    if (dh->pub_key == NULL || dh->g == NULL || dh->priv_key == NULL)
	return -1;

    mp_init_multi(&s, &priv_key, &p, &peer_pub, NULL);
    BN2mpz(&p, dh->p);
    BN2mpz(&peer_pub, pub);

    /* check if peers pubkey is reasonable */
    if (mp_isneg(&peer_pub)
	|| mp_cmp(&peer_pub, &p) >= 0
	|| mp_cmp_d(&peer_pub, 1) <= 0)
    {
	ret = -1;
	goto out;
    }

    BN2mpz(&priv_key, dh->priv_key);

    ret = mp_exptmod(&peer_pub, &priv_key, &p, &s);

    if (ret != 0) {
	ret = -1;
	goto out;
    }

    ret = mp_unsigned_bin_size(&s);
    mp_to_unsigned_bin(&s, shared);

 out:
    mp_clear_multi(&s, &priv_key, &p, &peer_pub, NULL);

    return ret;
}

static int
ltm_dh_generate_params(DH *dh, int a, int b, BN_GENCB *callback)
{
    /* groups should already be known, we don't care about this */
    return 0;
}

static int
ltm_dh_init(DH *dh)
{
    return 1;
}

static int
ltm_dh_finish(DH *dh)
{
    return 1;
}


/*
 *
 */

const DH_METHOD _hc_dh_ltm_method = {
    "hcrypto ltm DH",
    ltm_dh_generate_key,
    ltm_dh_compute_key,
    NULL,
    ltm_dh_init,
    ltm_dh_finish,
    0,
    NULL,
    ltm_dh_generate_params
};

/**
 * DH implementation using libtommath.
 *
 * @return the DH_METHOD for the DH implementation using libtommath.
 *
 * @ingroup hcrypto_dh
 */

const DH_METHOD *
DH_ltm_method(void)
{
    return &_hc_dh_ltm_method;
}
