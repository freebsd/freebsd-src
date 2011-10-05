/*
 * Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
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
#include <krb5-types.h>
#include <rfc2459_asn1.h>

#include <dh.h>

#include <roken.h>

/**
 * @page page_dh DH - Diffie-Hellman key exchange
 *
 * Diffie-Hellman key exchange is a protocol that allows two parties
 * to establish a shared secret key.
 *
 * Include and example how to use DH_new() and friends here.
 *
 * See the library functions here: @ref hcrypto_dh
 */

/**
 * Create a new DH object using DH_new_method(NULL), see DH_new_method().
 *
 * @return a newly allocated DH object.
 *
 * @ingroup hcrypto_dh
 */

DH *
DH_new(void)
{
    return DH_new_method(NULL);
}

/**
 * Create a new DH object from the given engine, if the NULL is used,
 * the default engine is used. Free the DH object with DH_free().
 *
 * @param engine The engine to use to allocate the DH object.
 *
 * @return a newly allocated DH object.
 *
 * @ingroup hcrypto_dh
 */

DH *
DH_new_method(ENGINE *engine)
{
    DH *dh;

    dh = calloc(1, sizeof(*dh));
    if (dh == NULL)
	return NULL;

    dh->references = 1;

    if (engine) {
	ENGINE_up_ref(engine);
	dh->engine = engine;
    } else {
	dh->engine = ENGINE_get_default_DH();
    }

    if (dh->engine) {
	dh->meth = ENGINE_get_DH(dh->engine);
	if (dh->meth == NULL) {
	    ENGINE_finish(engine);
	    free(dh);
	    return 0;
	}
    }

    if (dh->meth == NULL)
	dh->meth = DH_get_default_method();

    (*dh->meth->init)(dh);

    return dh;
}

/**
 * Free a DH object and release related resources, like ENGINE, that
 * the object was using.
 *
 * @param dh object to be freed.
 *
 * @ingroup hcrypto_dh
 */

void
DH_free(DH *dh)
{
    if (dh->references <= 0)
	abort();

    if (--dh->references > 0)
	return;

    (*dh->meth->finish)(dh);

    if (dh->engine)
	ENGINE_finish(dh->engine);

#define free_if(f) if (f) { BN_free(f); }
    free_if(dh->p);
    free_if(dh->g);
    free_if(dh->pub_key);
    free_if(dh->priv_key);
    free_if(dh->q);
    free_if(dh->j);
    free_if(dh->counter);
#undef free_if

    memset(dh, 0, sizeof(*dh));
    free(dh);
}

/**
 * Add a reference to the DH object. The object should be free with
 * DH_free() to drop the reference.
 *
 * @param dh the object to increase the reference count too.
 *
 * @return the updated reference count, can't safely be used except
 * for debug printing.
 *
 * @ingroup hcrypto_dh
 */

int
DH_up_ref(DH *dh)
{
    return ++dh->references;
}

/**
 * The maximum output size of the DH_compute_key() function.
 *
 * @param dh The DH object to get the size from.
 *
 * @return the maximum size in bytes of the out data.
 *
 * @ingroup hcrypto_dh
 */

int
DH_size(const DH *dh)
{
    return BN_num_bytes(dh->p);
}

/**
 * Set the data index idx in the DH object to data.
 *
 * @param dh DH object.
 * @param idx index to set the data for.
 * @param data data to store for the index idx.
 *
 * @return 1 on success.
 *
 * @ingroup hcrypto_dh
 */

int
DH_set_ex_data(DH *dh, int idx, void *data)
{
    dh->ex_data.sk = data;
    return 1;
}

/**
 * Get the data for index idx in the DH object.
 *
 * @param dh DH object.
 * @param idx index to get the data for.
 *
 * @return the object store in index idx
 *
 * @ingroup hcrypto_dh
 */

void *
DH_get_ex_data(DH *dh, int idx)
{
    return dh->ex_data.sk;
}

/**
 * Generate DH parameters for the DH object give parameters.
 *
 * @param dh The DH object to generate parameters for.
 * @param prime_len length of the prime
 * @param generator generator, g
 * @param cb Callback parameters to show progress, can be NULL.
 *
 * @return the maximum size in bytes of the out data.
 *
 * @ingroup hcrypto_dh
 */

int
DH_generate_parameters_ex(DH *dh, int prime_len, int generator, BN_GENCB *cb)
{
    if (dh->meth->generate_params)
	return dh->meth->generate_params(dh, prime_len, generator, cb);
    return 0;
}

/**
 * Check that the public key is sane.
 *
 * @param dh the local peer DH parameters.
 * @param pub_key the remote peer public key parameters.
 * @param codes return that the failures of the pub_key are.
 *
 * @return 1 on success, 0 on failure and *codes is set the the
 * combined fail check for the public key
 *
 * @ingroup hcrypto_dh
 */

int
DH_check_pubkey(const DH *dh, const BIGNUM *pub_key, int *codes)
{
    BIGNUM *bn = NULL, *sum = NULL;
    int ret = 0;

    *codes = 0;

    /**
     * Checks that the function performs are:
     * - pub_key is not negative
     */

    if (BN_is_negative(pub_key))
	goto out;

    /**
     * - pub_key > 1    and    pub_key < p - 1,
     *    to avoid small subgroups attack.
     */

    bn = BN_new();
    if (bn == NULL)
	goto out;

    if (!BN_set_word(bn, 1))
	goto out;

    if (BN_cmp(bn, pub_key) >= 0)
	*codes |= DH_CHECK_PUBKEY_TOO_SMALL;

    sum = BN_new();
    if (sum == NULL)
	goto out;

    BN_uadd(sum, pub_key, bn);

    if (BN_cmp(sum, dh->p) >= 0)
	*codes |= DH_CHECK_PUBKEY_TOO_LARGE;

    /**
     * - if g == 2, pub_key have more then one bit set,
     *   if bits set is 1, log_2(pub_key) is trival
     */

    if (!BN_set_word(bn, 2))
	goto out;

    if (BN_cmp(bn, dh->g) == 0) {
	unsigned i, n = BN_num_bits(pub_key);
	unsigned bits = 0;

	for (i = 0; i <= n; i++)
	    if (BN_is_bit_set(pub_key, i))
		bits++;

	if (bits < 2) {
	    *codes |= DH_CHECK_PUBKEY_TOO_SMALL;
	    goto out;
	}
    }

    ret = 1;
out:
    if (bn)
	BN_free(bn);
    if (sum)
	BN_free(sum);

    return ret;
}

/**
 * Generate a new DH private-public key pair. The dh parameter must be
 * allocted first with DH_new(). dh->p and dp->g must be set.
 *
 * @param dh dh parameter.
 *
 * @return 1 on success.
 *
 * @ingroup hcrypto_dh
 */

int
DH_generate_key(DH *dh)
{
    return dh->meth->generate_key(dh);
}

/**
 * Complute the shared secret key.
 *
 * @param shared_key the resulting shared key, need to be at least
 * DH_size() large.
 * @param peer_pub_key the peer's public key.
 * @param dh the dh key pair.
 *
 * @return 1 on success.
 *
 * @ingroup hcrypto_dh
 */

int
DH_compute_key(unsigned char *shared_key,
	       const BIGNUM *peer_pub_key, DH *dh)
{
    int codes;

    /**
     * Checks that the pubkey passed in is valid using
     * DH_check_pubkey().
     */

    if (!DH_check_pubkey(dh, peer_pub_key, &codes) || codes != 0)
	return -1;

    return dh->meth->compute_key(shared_key, peer_pub_key, dh);
}

/**
 * Set a new method for the DH keypair.
 *
 * @param dh dh parameter.
 * @param method the new method for the DH parameter.
 *
 * @return 1 on success.
 *
 * @ingroup hcrypto_dh
 */

int
DH_set_method(DH *dh, const DH_METHOD *method)
{
    (*dh->meth->finish)(dh);
    if (dh->engine) {
	ENGINE_finish(dh->engine);
	dh->engine = NULL;
    }
    dh->meth = method;
    (*dh->meth->init)(dh);
    return 1;
}

/*
 *
 */

static int
dh_null_generate_key(DH *dh)
{
    return 0;
}

static int
dh_null_compute_key(unsigned char *shared,const BIGNUM *pub, DH *dh)
{
    return 0;
}

static int
dh_null_init(DH *dh)
{
    return 1;
}

static int
dh_null_finish(DH *dh)
{
    return 1;
}

static int
dh_null_generate_params(DH *dh, int prime_num, int len, BN_GENCB *cb)
{
    return 0;
}

static const DH_METHOD dh_null_method = {
    "hcrypto null DH",
    dh_null_generate_key,
    dh_null_compute_key,
    NULL,
    dh_null_init,
    dh_null_finish,
    0,
    NULL,
    dh_null_generate_params
};

extern const DH_METHOD _hc_dh_ltm_method;
static const DH_METHOD *dh_default_method = &_hc_dh_ltm_method;

/**
 * Return the dummy DH implementation.
 *
 * @return pointer to a DH_METHOD.
 *
 * @ingroup hcrypto_dh
 */

const DH_METHOD *
DH_null_method(void)
{
    return &dh_null_method;
}

/**
 * Set the default DH implementation.
 *
 * @param meth pointer to a DH_METHOD.
 *
 * @ingroup hcrypto_dh
 */

void
DH_set_default_method(const DH_METHOD *meth)
{
    dh_default_method = meth;
}

/**
 * Return the default DH implementation.
 *
 * @return pointer to a DH_METHOD.
 *
 * @ingroup hcrypto_dh
 */

const DH_METHOD *
DH_get_default_method(void)
{
    return dh_default_method;
}

/*
 *
 */

static int
bn2heim_int(BIGNUM *bn, heim_integer *integer)
{
    integer->length = BN_num_bytes(bn);
    integer->data = malloc(integer->length);
    if (integer->data == NULL) {
	integer->length = 0;
	return ENOMEM;
    }
    BN_bn2bin(bn, integer->data);
    integer->negative = BN_is_negative(bn);
    return 0;
}

/**
 *
 */

int
i2d_DHparams(DH *dh, unsigned char **pp)
{
    DHParameter data;
    size_t size;
    int ret;

    memset(&data, 0, sizeof(data));

    if (bn2heim_int(dh->p, &data.prime) ||
	bn2heim_int(dh->g, &data.base))
    {
	free_DHParameter(&data);
	return -1;
    }

    if (pp == NULL) {
	size = length_DHParameter(&data);
	free_DHParameter(&data);
    } else {
	void *p;
	size_t len;

	ASN1_MALLOC_ENCODE(DHParameter, p, len, &data, &size, ret);
	free_DHParameter(&data);
	if (ret)
	    return -1;
	if (len != size) {
	    abort();
            return -1;
        }

	memcpy(*pp, p, size);
	free(p);

	*pp += size;
    }

    return size;
}
