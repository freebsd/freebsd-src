/*
 * Copyright (c) 1995, 1996, 1997, 1999 Kungliga Tekniska Högskolan
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

#include "config.h"


#define HC_DEPRECATED

#ifdef KRB5
#include <krb5-types.h>
#endif
#include <stdlib.h>

#include <des.h>
#include <rand.h>

#undef __attribute__
#define __attribute__(X)

void HC_DEPRECATED
DES_rand_data(void *outdata, int size)
{
    RAND_bytes(outdata, size);
}

void HC_DEPRECATED
DES_generate_random_block(DES_cblock *block)
{
    RAND_bytes(block, sizeof(*block));
}

#define DES_rand_data_key hc_DES_rand_data_key

void HC_DEPRECATED
DES_rand_data_key(DES_cblock *key);

/*
 * Generate a random DES key.
 */

void HC_DEPRECATED
DES_rand_data_key(DES_cblock *key)
{
    DES_new_random_key(key);
}

void HC_DEPRECATED
DES_set_sequence_number(void *ll)
{
}

void HC_DEPRECATED
DES_set_random_generator_seed(DES_cblock *seed)
{
    RAND_seed(seed, sizeof(*seed));
}

/**
 * Generate a random des key using a random block, fixup parity and
 * skip weak keys.
 *
 * @param key is set to a random key.
 *
 * @return 0 on success, non zero on random number generator failure.
 *
 * @ingroup hcrypto_des
 */

int HC_DEPRECATED
DES_new_random_key(DES_cblock *key)
{
    do {
	if (RAND_bytes(key, sizeof(*key)) != 1)
	    return 1;
	DES_set_odd_parity(key);
    } while(DES_is_weak_key(key));

    return(0);
}

/**
 * Seed the random number generator. Deprecated, use @ref page_rand
 *
 * @param seed a seed to seed that random number generate with.
 *
 * @ingroup hcrypto_des
 */

void HC_DEPRECATED
DES_init_random_number_generator(DES_cblock *seed)
{
    RAND_seed(seed, sizeof(*seed));
}

/**
 * Generate a random key, deprecated since it doesn't return an error
 * code, use DES_new_random_key().
 *
 * @param key is set to a random key.
 *
 * @ingroup hcrypto_des
 */

void HC_DEPRECATED
DES_random_key(DES_cblock *key)
{
    if (DES_new_random_key(key))
	abort();
}
