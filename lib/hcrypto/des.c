/*
 * Copyright (c) 2005 Kungliga Tekniska Högskolan
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

/**
 * @page page_des DES - Data Encryption Standard crypto interface
 *
 * See the library functions here: @ref hcrypto_des
 *
 * DES was created by IBM, modififed by NSA and then adopted by NBS
 * (now NIST) and published ad FIPS PUB 46 (updated by FIPS 46-1).
 *
 * Since the 19th May 2005 DES was withdrawn by NIST and should no
 * longer be used. See @ref page_evp for replacement encryption
 * algorithms and interfaces.
 *
 * Read more the iteresting history of DES on Wikipedia
 * http://www.wikipedia.org/wiki/Data_Encryption_Standard .
 *
 * @section des_keygen DES key generation
 *
 * To generate a DES key safely you have to use the code-snippet
 * below. This is because the DES_random_key() can fail with an
 * abort() in case of and failure to start the random generator.
 *
 * There is a replacement function DES_new_random_key(), however that
 * function does not exists in OpenSSL.
 *
 * @code
 * DES_cblock key;
 * do {
 *     if (RAND_rand(&key, sizeof(key)) != 1)
 *          goto failure;
 *     DES_set_odd_parity(key);
 * } while (DES_is_weak_key(&key));
 * @endcode
 *
 * @section des_impl DES implementation history
 *
 * There was no complete BSD licensed, fast, GPL compatible
 * implementation of DES, so Love wrote the part that was missing,
 * fast key schedule setup and adapted the interface to the orignal
 * libdes.
 *
 * The document that got me started for real was "Efficient
 * Implementation of the Data Encryption Standard" by Dag Arne Osvik.
 * I never got to the PC1 transformation was working, instead I used
 * table-lookup was used for all key schedule setup. The document was
 * very useful since it de-mystified other implementations for me.
 *
 * The core DES function (SBOX + P transformation) is from Richard
 * Outerbridge public domain DES implementation. My sanity is saved
 * thanks to his work. Thank you Richard.
 */

#include <config.h>

#define HC_DEPRECATED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <krb5-types.h>
#include <assert.h>

#include <roken.h>

#include "des.h"
#include "ui.h"

static void desx(uint32_t [2], DES_key_schedule *, int);
static void IP(uint32_t [2]);
static void FP(uint32_t [2]);

#include "des-tables.h"

#define ROTATE_LEFT28(x,one)				\
    if (one) {						\
	x = ( ((x)<<(1)) & 0xffffffe) | ((x) >> 27);	\
    } else {						\
	x = ( ((x)<<(2)) & 0xffffffc) | ((x) >> 26);	\
    }

/**
 * Set the parity of the key block, used to generate a des key from a
 * random key. See @ref des_keygen.
 *
 * @param key key to fixup the parity for.
 * @ingroup hcrypto_des
 */

void
DES_set_odd_parity(DES_cblock *key)
{
    unsigned int i;
    for (i = 0; i < DES_CBLOCK_LEN; i++)
	(*key)[i] = odd_parity[(*key)[i]];
}

/**
 * Check if the key have correct parity.
 *
 * @param key key to check the parity.
 * @return 1 on success, 0 on failure.
 * @ingroup hcrypto_des
 */

int HC_DEPRECATED
DES_check_key_parity(DES_cblock *key)
{
    unsigned int i;

    for (i = 0; i <  DES_CBLOCK_LEN; i++)
	if ((*key)[i] != odd_parity[(*key)[i]])
	    return 0;
    return 1;
}

/*
 *
 */

/* FIPS 74 */
static DES_cblock weak_keys[] = {
    {0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01}, /* weak keys */
    {0xFE,0xFE,0xFE,0xFE,0xFE,0xFE,0xFE,0xFE},
    {0x1F,0x1F,0x1F,0x1F,0x0E,0x0E,0x0E,0x0E},
    {0xE0,0xE0,0xE0,0xE0,0xF1,0xF1,0xF1,0xF1},
    {0x01,0xFE,0x01,0xFE,0x01,0xFE,0x01,0xFE}, /* semi-weak keys */
    {0xFE,0x01,0xFE,0x01,0xFE,0x01,0xFE,0x01},
    {0x1F,0xE0,0x1F,0xE0,0x0E,0xF1,0x0E,0xF1},
    {0xE0,0x1F,0xE0,0x1F,0xF1,0x0E,0xF1,0x0E},
    {0x01,0xE0,0x01,0xE0,0x01,0xF1,0x01,0xF1},
    {0xE0,0x01,0xE0,0x01,0xF1,0x01,0xF1,0x01},
    {0x1F,0xFE,0x1F,0xFE,0x0E,0xFE,0x0E,0xFE},
    {0xFE,0x1F,0xFE,0x1F,0xFE,0x0E,0xFE,0x0E},
    {0x01,0x1F,0x01,0x1F,0x01,0x0E,0x01,0x0E},
    {0x1F,0x01,0x1F,0x01,0x0E,0x01,0x0E,0x01},
    {0xE0,0xFE,0xE0,0xFE,0xF1,0xFE,0xF1,0xFE},
    {0xFE,0xE0,0xFE,0xE0,0xFE,0xF1,0xFE,0xF1}
};

/**
 * Checks if the key is any of the weaks keys that makes DES attacks
 * trival.
 *
 * @param key key to check.
 *
 * @return 1 if the key is weak, 0 otherwise.
 * @ingroup hcrypto_des
 */

int
DES_is_weak_key(DES_cblock *key)
{
    int weak = 0;
    int i;

    for (i = 0; i < sizeof(weak_keys)/sizeof(weak_keys[0]); i++)
	weak ^= (ct_memcmp(weak_keys[i], key, DES_CBLOCK_LEN) == 0);

    return !!weak;
}

/**
 * Setup a des key schedule from a key. Deprecated function, use
 * DES_set_key_unchecked() or DES_set_key_checked() instead.
 *
 * @param key a key to initialize the key schedule with.
 * @param ks a key schedule to initialize.
 *
 * @return 0 on success
 * @ingroup hcrypto_des
 */

int HC_DEPRECATED
DES_set_key(DES_cblock *key, DES_key_schedule *ks)
{
    return DES_set_key_checked(key, ks);
}

/**
 * Setup a des key schedule from a key. The key is no longer needed
 * after this transaction and can cleared.
 *
 * Does NOT check that the key is weak for or have wrong parity.
 *
 * @param key a key to initialize the key schedule with.
 * @param ks a key schedule to initialize.
 *
 * @return 0 on success
 * @ingroup hcrypto_des
 */

int
DES_set_key_unchecked(DES_cblock *key, DES_key_schedule *ks)
{
    uint32_t t1, t2;
    uint32_t c, d;
    int shifts[16] = { 1, 1, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1 };
    uint32_t *k = &ks->ks[0];
    int i;

    t1 = (*key)[0] << 24 | (*key)[1] << 16 | (*key)[2] << 8 | (*key)[3];
    t2 = (*key)[4] << 24 | (*key)[5] << 16 | (*key)[6] << 8 | (*key)[7];

    c =   (pc1_c_3[(t1 >> (5            )) & 0x7] << 3)
	| (pc1_c_3[(t1 >> (5 + 8        )) & 0x7] << 2)
	| (pc1_c_3[(t1 >> (5 + 8 + 8    )) & 0x7] << 1)
	| (pc1_c_3[(t1 >> (5 + 8 + 8 + 8)) & 0x7] << 0)
	| (pc1_c_4[(t2 >> (4            )) & 0xf] << 3)
	| (pc1_c_4[(t2 >> (4 + 8        )) & 0xf] << 2)
	| (pc1_c_4[(t2 >> (4 + 8 + 8    )) & 0xf] << 1)
	| (pc1_c_4[(t2 >> (4 + 8 + 8 + 8)) & 0xf] << 0);


    d =   (pc1_d_3[(t2 >> (1            )) & 0x7] << 3)
	| (pc1_d_3[(t2 >> (1 + 8        )) & 0x7] << 2)
	| (pc1_d_3[(t2 >> (1 + 8 + 8    )) & 0x7] << 1)
	| (pc1_d_3[(t2 >> (1 + 8 + 8 + 8)) & 0x7] << 0)
	| (pc1_d_4[(t1 >> (1            )) & 0xf] << 3)
	| (pc1_d_4[(t1 >> (1 + 8        )) & 0xf] << 2)
	| (pc1_d_4[(t1 >> (1 + 8 + 8    )) & 0xf] << 1)
	| (pc1_d_4[(t1 >> (1 + 8 + 8 + 8)) & 0xf] << 0);

    for (i = 0; i < 16; i++) {
	uint32_t kc, kd;

	ROTATE_LEFT28(c, shifts[i]);
	ROTATE_LEFT28(d, shifts[i]);

	kc = pc2_c_1[(c >> 22) & 0x3f] |
	    pc2_c_2[((c >> 16) & 0x30) | ((c >> 15) & 0xf)] |
	    pc2_c_3[((c >> 9 ) & 0x3c) | ((c >> 8 ) & 0x3)] |
	    pc2_c_4[((c >> 2 ) & 0x20) | ((c >> 1) & 0x18) | (c & 0x7)];
	kd = pc2_d_1[(d >> 22) & 0x3f] |
	    pc2_d_2[((d >> 15) & 0x30) | ((d >> 14) & 0xf)] |
	    pc2_d_3[ (d >> 7 ) & 0x3f] |
	    pc2_d_4[((d >> 1 ) & 0x3c) | ((d      ) & 0x3)];

	/* Change to byte order used by the S boxes */
	*k  =    (kc & 0x00fc0000L) << 6;
	*k |=    (kc & 0x00000fc0L) << 10;
	*k |=    (kd & 0x00fc0000L) >> 10;
	*k++  |= (kd & 0x00000fc0L) >> 6;
	*k  =    (kc & 0x0003f000L) << 12;
	*k |=    (kc & 0x0000003fL) << 16;
	*k |=    (kd & 0x0003f000L) >> 4;
	*k++  |= (kd & 0x0000003fL);
    }

    return 0;
}

/**
 * Just like DES_set_key_unchecked() except checking that the key is
 * not weak for or have correct parity.
 *
 * @param key a key to initialize the key schedule with.
 * @param ks a key schedule to initialize.
 *
 * @return 0 on success, -1 on invalid parity, -2 on weak key.
 * @ingroup hcrypto_des
 */

int
DES_set_key_checked(DES_cblock *key, DES_key_schedule *ks)
{
    if (!DES_check_key_parity(key)) {
	memset(ks, 0, sizeof(*ks));
	return -1;
    }
    if (DES_is_weak_key(key)) {
	memset(ks, 0, sizeof(*ks));
	return -2;
    }
    return DES_set_key_unchecked(key, ks);
}

/**
 * Compatibility function for eay libdes, works just like
 * DES_set_key_checked().
 *
 * @param key a key to initialize the key schedule with.
 * @param ks a key schedule to initialize.
 *
 * @return 0 on success, -1 on invalid parity, -2 on weak key.
 * @ingroup hcrypto_des
 */

int
DES_key_sched(DES_cblock *key, DES_key_schedule *ks)
{
    return DES_set_key_checked(key, ks);
}

/*
 *
 */

static void
load(const unsigned char *b, uint32_t v[2])
{
    v[0] =  b[0] << 24;
    v[0] |= b[1] << 16;
    v[0] |= b[2] << 8;
    v[0] |= b[3] << 0;
    v[1] =  b[4] << 24;
    v[1] |= b[5] << 16;
    v[1] |= b[6] << 8;
    v[1] |= b[7] << 0;
}

static void
store(const uint32_t v[2], unsigned char *b)
{
    b[0] = (v[0] >> 24) & 0xff;
    b[1] = (v[0] >> 16) & 0xff;
    b[2] = (v[0] >>  8) & 0xff;
    b[3] = (v[0] >>  0) & 0xff;
    b[4] = (v[1] >> 24) & 0xff;
    b[5] = (v[1] >> 16) & 0xff;
    b[6] = (v[1] >>  8) & 0xff;
    b[7] = (v[1] >>  0) & 0xff;
}

/**
 * Encrypt/decrypt a block using DES. Also called ECB mode
 *
 * @param u data to encrypt
 * @param ks key schedule to use
 * @param encp if non zero, encrypt. if zero, decrypt.
 *
 * @ingroup hcrypto_des
 */

void
DES_encrypt(uint32_t u[2], DES_key_schedule *ks, int encp)
{
    IP(u);
    desx(u, ks, encp);
    FP(u);
}

/**
 * Encrypt/decrypt a block using DES.
 *
 * @param input data to encrypt
 * @param output data to encrypt
 * @param ks key schedule to use
 * @param encp if non zero, encrypt. if zero, decrypt.
 *
 * @ingroup hcrypto_des
 */

void
DES_ecb_encrypt(DES_cblock *input, DES_cblock *output,
		DES_key_schedule *ks, int encp)
{
    uint32_t u[2];
    load(*input, u);
    DES_encrypt(u, ks, encp);
    store(u, *output);
}

/**
 * Encrypt/decrypt a block using DES in Chain Block Cipher mode (cbc).
 *
 * The IV must always be diffrent for diffrent input data blocks.
 *
 * @param in data to encrypt
 * @param out data to encrypt
 * @param length length of data
 * @param ks key schedule to use
 * @param iv initial vector to use
 * @param encp if non zero, encrypt. if zero, decrypt.
 *
 * @ingroup hcrypto_des
 */

void
DES_cbc_encrypt(const void *in, void *out, long length,
		DES_key_schedule *ks, DES_cblock *iv, int encp)
{
    const unsigned char *input = in;
    unsigned char *output = out;
    uint32_t u[2];
    uint32_t uiv[2];

    load(*iv, uiv);

    if (encp) {
	while (length >= DES_CBLOCK_LEN) {
	    load(input, u);
	    u[0] ^= uiv[0]; u[1] ^= uiv[1];
	    DES_encrypt(u, ks, 1);
	    uiv[0] = u[0]; uiv[1] = u[1];
	    store(u, output);

	    length -= DES_CBLOCK_LEN;
	    input += DES_CBLOCK_LEN;
	    output += DES_CBLOCK_LEN;
	}
	if (length) {
	    unsigned char tmp[DES_CBLOCK_LEN];
	    memcpy(tmp, input, length);
	    memset(tmp + length, 0, DES_CBLOCK_LEN - length);
	    load(tmp, u);
	    u[0] ^= uiv[0]; u[1] ^= uiv[1];
	    DES_encrypt(u, ks, 1);
	    store(u, output);
	}
    } else {
	uint32_t t[2];
	while (length >= DES_CBLOCK_LEN) {
	    load(input, u);
	    t[0] = u[0]; t[1] = u[1];
	    DES_encrypt(u, ks, 0);
	    u[0] ^= uiv[0]; u[1] ^= uiv[1];
	    store(u, output);
	    uiv[0] = t[0]; uiv[1] = t[1];

	    length -= DES_CBLOCK_LEN;
	    input += DES_CBLOCK_LEN;
	    output += DES_CBLOCK_LEN;
	}
	if (length) {
	    unsigned char tmp[DES_CBLOCK_LEN];
	    memcpy(tmp, input, length);
	    memset(tmp + length, 0, DES_CBLOCK_LEN - length);
	    load(tmp, u);
	    DES_encrypt(u, ks, 0);
	    u[0] ^= uiv[0]; u[1] ^= uiv[1];
	    store(u, output);
	}
    }
    uiv[0] = 0; u[0] = 0; uiv[1] = 0; u[1] = 0;
}

/**
 * Encrypt/decrypt a block using DES in Propagating Cipher Block
 * Chaining mode. This mode is only used for Kerberos 4, and it should
 * stay that way.
 *
 * The IV must always be diffrent for diffrent input data blocks.
 *
 * @param in data to encrypt
 * @param out data to encrypt
 * @param length length of data
 * @param ks key schedule to use
 * @param iv initial vector to use
 * @param encp if non zero, encrypt. if zero, decrypt.
 *
 * @ingroup hcrypto_des
 */

void
DES_pcbc_encrypt(const void *in, void *out, long length,
		 DES_key_schedule *ks, DES_cblock *iv, int encp)
{
    const unsigned char *input = in;
    unsigned char *output = out;
    uint32_t u[2];
    uint32_t uiv[2];

    load(*iv, uiv);

    if (encp) {
	uint32_t t[2];
	while (length >= DES_CBLOCK_LEN) {
	    load(input, u);
	    t[0] = u[0]; t[1] = u[1];
	    u[0] ^= uiv[0]; u[1] ^= uiv[1];
	    DES_encrypt(u, ks, 1);
	    uiv[0] = u[0] ^ t[0]; uiv[1] = u[1] ^ t[1];
	    store(u, output);

	    length -= DES_CBLOCK_LEN;
	    input += DES_CBLOCK_LEN;
	    output += DES_CBLOCK_LEN;
	}
	if (length) {
	    unsigned char tmp[DES_CBLOCK_LEN];
	    memcpy(tmp, input, length);
	    memset(tmp + length, 0, DES_CBLOCK_LEN - length);
	    load(tmp, u);
	    u[0] ^= uiv[0]; u[1] ^= uiv[1];
	    DES_encrypt(u, ks, 1);
	    store(u, output);
	}
    } else {
	uint32_t t[2];
	while (length >= DES_CBLOCK_LEN) {
	    load(input, u);
	    t[0] = u[0]; t[1] = u[1];
	    DES_encrypt(u, ks, 0);
	    u[0] ^= uiv[0]; u[1] ^= uiv[1];
	    store(u, output);
	    uiv[0] = t[0] ^ u[0]; uiv[1] = t[1] ^ u[1];

	    length -= DES_CBLOCK_LEN;
	    input += DES_CBLOCK_LEN;
	    output += DES_CBLOCK_LEN;
	}
	if (length) {
	    unsigned char tmp[DES_CBLOCK_LEN];
	    memcpy(tmp, input, length);
	    memset(tmp + length, 0, DES_CBLOCK_LEN - length);
	    load(tmp, u);
	    DES_encrypt(u, ks, 0);
	    u[0] ^= uiv[0]; u[1] ^= uiv[1];
	}
    }
    uiv[0] = 0; u[0] = 0; uiv[1] = 0; u[1] = 0;
}

/*
 *
 */

static void
_des3_encrypt(uint32_t u[2], DES_key_schedule *ks1, DES_key_schedule *ks2,
	      DES_key_schedule *ks3, int encp)
{
    IP(u);
    if (encp) {
	desx(u, ks1, 1); /* IP + FP cancel out each other */
	desx(u, ks2, 0);
	desx(u, ks3, 1);
    } else {
	desx(u, ks3, 0);
	desx(u, ks2, 1);
	desx(u, ks1, 0);
    }
    FP(u);
}

/**
 * Encrypt/decrypt a block using triple DES using EDE mode,
 * encrypt/decrypt/encrypt.
 *
 * @param input data to encrypt
 * @param output data to encrypt
 * @param ks1 key schedule to use
 * @param ks2 key schedule to use
 * @param ks3 key schedule to use
 * @param encp if non zero, encrypt. if zero, decrypt.
 *
 * @ingroup hcrypto_des
 */

void
DES_ecb3_encrypt(DES_cblock *input,
		 DES_cblock *output,
		 DES_key_schedule *ks1,
		 DES_key_schedule *ks2,
		 DES_key_schedule *ks3,
		 int encp)
{
    uint32_t u[2];
    load(*input, u);
    _des3_encrypt(u, ks1, ks2, ks3, encp);
    store(u, *output);
    return;
}

/**
 * Encrypt/decrypt using Triple DES in Chain Block Cipher mode (cbc).
 *
 * The IV must always be diffrent for diffrent input data blocks.
 *
 * @param in data to encrypt
 * @param out data to encrypt
 * @param length length of data
 * @param ks1 key schedule to use
 * @param ks2 key schedule to use
 * @param ks3 key schedule to use
 * @param iv initial vector to use
 * @param encp if non zero, encrypt. if zero, decrypt.
 *
 * @ingroup hcrypto_des
 */

void
DES_ede3_cbc_encrypt(const void *in, void *out,
		     long length, DES_key_schedule *ks1,
		     DES_key_schedule *ks2, DES_key_schedule *ks3,
		     DES_cblock *iv, int encp)
{
    const unsigned char *input = in;
    unsigned char *output = out;
    uint32_t u[2];
    uint32_t uiv[2];

    load(*iv, uiv);

    if (encp) {
	while (length >= DES_CBLOCK_LEN) {
	    load(input, u);
	    u[0] ^= uiv[0]; u[1] ^= uiv[1];
	    _des3_encrypt(u, ks1, ks2, ks3, 1);
	    uiv[0] = u[0]; uiv[1] = u[1];
	    store(u, output);

	    length -= DES_CBLOCK_LEN;
	    input += DES_CBLOCK_LEN;
	    output += DES_CBLOCK_LEN;
	}
	if (length) {
	    unsigned char tmp[DES_CBLOCK_LEN];
	    memcpy(tmp, input, length);
	    memset(tmp + length, 0, DES_CBLOCK_LEN - length);
	    load(tmp, u);
	    u[0] ^= uiv[0]; u[1] ^= uiv[1];
	    _des3_encrypt(u, ks1, ks2, ks3, 1);
	    store(u, output);
	}
    } else {
	uint32_t t[2];
	while (length >= DES_CBLOCK_LEN) {
	    load(input, u);
	    t[0] = u[0]; t[1] = u[1];
	    _des3_encrypt(u, ks1, ks2, ks3, 0);
	    u[0] ^= uiv[0]; u[1] ^= uiv[1];
	    store(u, output);
	    uiv[0] = t[0]; uiv[1] = t[1];

	    length -= DES_CBLOCK_LEN;
	    input += DES_CBLOCK_LEN;
	    output += DES_CBLOCK_LEN;
	}
	if (length) {
	    unsigned char tmp[DES_CBLOCK_LEN];
	    memcpy(tmp, input, length);
	    memset(tmp + length, 0, DES_CBLOCK_LEN - length);
	    load(tmp, u);
	    _des3_encrypt(u, ks1, ks2, ks3, 0);
	    u[0] ^= uiv[0]; u[1] ^= uiv[1];
	    store(u, output);
	}
    }
    store(uiv, *iv);
    uiv[0] = 0; u[0] = 0; uiv[1] = 0; u[1] = 0;
}

/**
 * Encrypt/decrypt using DES in cipher feedback mode with 64 bit
 * feedback.
 *
 * The IV must always be diffrent for diffrent input data blocks.
 *
 * @param in data to encrypt
 * @param out data to encrypt
 * @param length length of data
 * @param ks key schedule to use
 * @param iv initial vector to use
 * @param num offset into in cipher block encryption/decryption stop last time.
 * @param encp if non zero, encrypt. if zero, decrypt.
 *
 * @ingroup hcrypto_des
 */

void
DES_cfb64_encrypt(const void *in, void *out,
		  long length, DES_key_schedule *ks, DES_cblock *iv,
		  int *num, int encp)
{
    const unsigned char *input = in;
    unsigned char *output = out;
    unsigned char tmp[DES_CBLOCK_LEN];
    uint32_t uiv[2];

    load(*iv, uiv);

    assert(*num >= 0 && *num < DES_CBLOCK_LEN);

    if (encp) {
	int i = *num;

	while (length > 0) {
	    if (i == 0)
		DES_encrypt(uiv, ks, 1);
	    store(uiv, tmp);
	    for (; i < DES_CBLOCK_LEN && i < length; i++) {
		output[i] = tmp[i] ^ input[i];
	    }
	    if (i == DES_CBLOCK_LEN)
		load(output, uiv);
	    output += i;
	    input += i;
	    length -= i;
	    if (i == DES_CBLOCK_LEN)
		i = 0;
	}
	store(uiv, *iv);
	*num = i;
    } else {
	int i = *num;
	unsigned char c;

	while (length > 0) {
	    if (i == 0) {
		DES_encrypt(uiv, ks, 1);
		store(uiv, tmp);
	    }
	    for (; i < DES_CBLOCK_LEN && i < length; i++) {
		c = input[i];
		output[i] = tmp[i] ^ input[i];
		(*iv)[i] = c;
	    }
	    output += i;
	    input += i;
	    length -= i;
	    if (i == DES_CBLOCK_LEN) {
		i = 0;
		load(*iv, uiv);
	    }
	}
	store(uiv, *iv);
	*num = i;
    }
}

/**
 * Crete a checksum using DES in CBC encryption mode. This mode is
 * only used for Kerberos 4, and it should stay that way.
 *
 * The IV must always be diffrent for diffrent input data blocks.
 *
 * @param in data to checksum
 * @param output the checksum
 * @param length length of data
 * @param ks key schedule to use
 * @param iv initial vector to use
 *
 * @ingroup hcrypto_des
 */

uint32_t
DES_cbc_cksum(const void *in, DES_cblock *output,
	      long length, DES_key_schedule *ks, DES_cblock *iv)
{
    const unsigned char *input = in;
    uint32_t uiv[2];
    uint32_t u[2] = { 0, 0 };

    load(*iv, uiv);

    while (length >= DES_CBLOCK_LEN) {
	load(input, u);
	u[0] ^= uiv[0]; u[1] ^= uiv[1];
	DES_encrypt(u, ks, 1);
	uiv[0] = u[0]; uiv[1] = u[1];

	length -= DES_CBLOCK_LEN;
	input += DES_CBLOCK_LEN;
    }
    if (length) {
	unsigned char tmp[DES_CBLOCK_LEN];
	memcpy(tmp, input, length);
	memset(tmp + length, 0, DES_CBLOCK_LEN - length);
	load(tmp, u);
	u[0] ^= uiv[0]; u[1] ^= uiv[1];
	DES_encrypt(u, ks, 1);
    }
    if (output)
	store(u, *output);

    uiv[0] = 0; u[0] = 0; uiv[1] = 0;
    return u[1];
}

/*
 *
 */

static unsigned char
bitswap8(unsigned char b)
{
    unsigned char r = 0;
    int i;
    for (i = 0; i < 8; i++) {
	r = r << 1 | (b & 1);
	b = b >> 1;
    }
    return r;
}

/**
 * Convert a string to a DES key. Use something like
 * PKCS5_PBKDF2_HMAC_SHA1() to create key from passwords.
 *
 * @param str The string to convert to a key
 * @param key the resulting key
 *
 * @ingroup hcrypto_des
 */

void
DES_string_to_key(const char *str, DES_cblock *key)
{
    const unsigned char *s;
    unsigned char *k;
    DES_key_schedule ks;
    size_t i, len;

    memset(key, 0, sizeof(*key));
    k = *key;
    s = (const unsigned char *)str;

    len = strlen(str);
    for (i = 0; i < len; i++) {
	if ((i % 16) < 8)
	    k[i % 8] ^= s[i] << 1;
	else
	    k[7 - (i % 8)] ^= bitswap8(s[i]);
    }
    DES_set_odd_parity(key);
    if (DES_is_weak_key(key))
	k[7] ^= 0xF0;
    DES_set_key(key, &ks);
    DES_cbc_cksum(s, key, len, &ks, key);
    memset(&ks, 0, sizeof(ks));
    DES_set_odd_parity(key);
    if (DES_is_weak_key(key))
	k[7] ^= 0xF0;
}

/**
 * Read password from prompt and create a DES key. Internal uses
 * DES_string_to_key(). Really, go use a really string2key function
 * like PKCS5_PBKDF2_HMAC_SHA1().
 *
 * @param key key to convert to
 * @param prompt prompt to display user
 * @param verify prompt twice.
 *
 * @return 1 on success, non 1 on failure.
 */

int
DES_read_password(DES_cblock *key, char *prompt, int verify)
{
    char buf[512];
    int ret;

    ret = UI_UTIL_read_pw_string(buf, sizeof(buf) - 1, prompt, verify);
    if (ret == 1)
	DES_string_to_key(buf, key);
    return ret;
}

/*
 *
 */


void
_DES_ipfp_test(void)
{
    DES_cblock k = "\x01\x02\x04\x08\x10\x20\x40\x80", k2;
    uint32_t u[2] = { 1, 0 };
    IP(u);
    FP(u);
    IP(u);
    FP(u);
    if (u[0] != 1 || u[1] != 0)
	abort();

    load(k, u);
    store(u, k2);
    if (memcmp(k, k2, 8) != 0)
	abort();
}

/* D3DES (V5.09) -
 *
 * A portable, public domain, version of the Data Encryption Standard.
 *
 * Written with Symantec's THINK (Lightspeed) C by Richard Outerbridge.
 * Thanks to: Dan Hoey for his excellent Initial and Inverse permutation
 * code;  Jim Gillogly & Phil Karn for the DES key schedule code; Dennis
 * Ferguson, Eric Young and Dana How for comparing notes; and Ray Lau,
 * for humouring me on.
 *
 * Copyright (c) 1988,1989,1990,1991,1992 by Richard Outerbridge.
 * (GEnie : OUTER; CIS : [71755,204]) Graven Imagery, 1992.
 */

static uint32_t SP1[64] = {
    0x01010400L, 0x00000000L, 0x00010000L, 0x01010404L,
    0x01010004L, 0x00010404L, 0x00000004L, 0x00010000L,
    0x00000400L, 0x01010400L, 0x01010404L, 0x00000400L,
    0x01000404L, 0x01010004L, 0x01000000L, 0x00000004L,
    0x00000404L, 0x01000400L, 0x01000400L, 0x00010400L,
    0x00010400L, 0x01010000L, 0x01010000L, 0x01000404L,
    0x00010004L, 0x01000004L, 0x01000004L, 0x00010004L,
    0x00000000L, 0x00000404L, 0x00010404L, 0x01000000L,
    0x00010000L, 0x01010404L, 0x00000004L, 0x01010000L,
    0x01010400L, 0x01000000L, 0x01000000L, 0x00000400L,
    0x01010004L, 0x00010000L, 0x00010400L, 0x01000004L,
    0x00000400L, 0x00000004L, 0x01000404L, 0x00010404L,
    0x01010404L, 0x00010004L, 0x01010000L, 0x01000404L,
    0x01000004L, 0x00000404L, 0x00010404L, 0x01010400L,
    0x00000404L, 0x01000400L, 0x01000400L, 0x00000000L,
    0x00010004L, 0x00010400L, 0x00000000L, 0x01010004L };

static uint32_t SP2[64] = {
    0x80108020L, 0x80008000L, 0x00008000L, 0x00108020L,
    0x00100000L, 0x00000020L, 0x80100020L, 0x80008020L,
    0x80000020L, 0x80108020L, 0x80108000L, 0x80000000L,
    0x80008000L, 0x00100000L, 0x00000020L, 0x80100020L,
    0x00108000L, 0x00100020L, 0x80008020L, 0x00000000L,
    0x80000000L, 0x00008000L, 0x00108020L, 0x80100000L,
    0x00100020L, 0x80000020L, 0x00000000L, 0x00108000L,
    0x00008020L, 0x80108000L, 0x80100000L, 0x00008020L,
    0x00000000L, 0x00108020L, 0x80100020L, 0x00100000L,
    0x80008020L, 0x80100000L, 0x80108000L, 0x00008000L,
    0x80100000L, 0x80008000L, 0x00000020L, 0x80108020L,
    0x00108020L, 0x00000020L, 0x00008000L, 0x80000000L,
    0x00008020L, 0x80108000L, 0x00100000L, 0x80000020L,
    0x00100020L, 0x80008020L, 0x80000020L, 0x00100020L,
    0x00108000L, 0x00000000L, 0x80008000L, 0x00008020L,
    0x80000000L, 0x80100020L, 0x80108020L, 0x00108000L };

static uint32_t SP3[64] = {
    0x00000208L, 0x08020200L, 0x00000000L, 0x08020008L,
    0x08000200L, 0x00000000L, 0x00020208L, 0x08000200L,
    0x00020008L, 0x08000008L, 0x08000008L, 0x00020000L,
    0x08020208L, 0x00020008L, 0x08020000L, 0x00000208L,
    0x08000000L, 0x00000008L, 0x08020200L, 0x00000200L,
    0x00020200L, 0x08020000L, 0x08020008L, 0x00020208L,
    0x08000208L, 0x00020200L, 0x00020000L, 0x08000208L,
    0x00000008L, 0x08020208L, 0x00000200L, 0x08000000L,
    0x08020200L, 0x08000000L, 0x00020008L, 0x00000208L,
    0x00020000L, 0x08020200L, 0x08000200L, 0x00000000L,
    0x00000200L, 0x00020008L, 0x08020208L, 0x08000200L,
    0x08000008L, 0x00000200L, 0x00000000L, 0x08020008L,
    0x08000208L, 0x00020000L, 0x08000000L, 0x08020208L,
    0x00000008L, 0x00020208L, 0x00020200L, 0x08000008L,
    0x08020000L, 0x08000208L, 0x00000208L, 0x08020000L,
    0x00020208L, 0x00000008L, 0x08020008L, 0x00020200L };

static uint32_t SP4[64] = {
    0x00802001L, 0x00002081L, 0x00002081L, 0x00000080L,
    0x00802080L, 0x00800081L, 0x00800001L, 0x00002001L,
    0x00000000L, 0x00802000L, 0x00802000L, 0x00802081L,
    0x00000081L, 0x00000000L, 0x00800080L, 0x00800001L,
    0x00000001L, 0x00002000L, 0x00800000L, 0x00802001L,
    0x00000080L, 0x00800000L, 0x00002001L, 0x00002080L,
    0x00800081L, 0x00000001L, 0x00002080L, 0x00800080L,
    0x00002000L, 0x00802080L, 0x00802081L, 0x00000081L,
    0x00800080L, 0x00800001L, 0x00802000L, 0x00802081L,
    0x00000081L, 0x00000000L, 0x00000000L, 0x00802000L,
    0x00002080L, 0x00800080L, 0x00800081L, 0x00000001L,
    0x00802001L, 0x00002081L, 0x00002081L, 0x00000080L,
    0x00802081L, 0x00000081L, 0x00000001L, 0x00002000L,
    0x00800001L, 0x00002001L, 0x00802080L, 0x00800081L,
    0x00002001L, 0x00002080L, 0x00800000L, 0x00802001L,
    0x00000080L, 0x00800000L, 0x00002000L, 0x00802080L };

static uint32_t SP5[64] = {
    0x00000100L, 0x02080100L, 0x02080000L, 0x42000100L,
    0x00080000L, 0x00000100L, 0x40000000L, 0x02080000L,
    0x40080100L, 0x00080000L, 0x02000100L, 0x40080100L,
    0x42000100L, 0x42080000L, 0x00080100L, 0x40000000L,
    0x02000000L, 0x40080000L, 0x40080000L, 0x00000000L,
    0x40000100L, 0x42080100L, 0x42080100L, 0x02000100L,
    0x42080000L, 0x40000100L, 0x00000000L, 0x42000000L,
    0x02080100L, 0x02000000L, 0x42000000L, 0x00080100L,
    0x00080000L, 0x42000100L, 0x00000100L, 0x02000000L,
    0x40000000L, 0x02080000L, 0x42000100L, 0x40080100L,
    0x02000100L, 0x40000000L, 0x42080000L, 0x02080100L,
    0x40080100L, 0x00000100L, 0x02000000L, 0x42080000L,
    0x42080100L, 0x00080100L, 0x42000000L, 0x42080100L,
    0x02080000L, 0x00000000L, 0x40080000L, 0x42000000L,
    0x00080100L, 0x02000100L, 0x40000100L, 0x00080000L,
    0x00000000L, 0x40080000L, 0x02080100L, 0x40000100L };

static uint32_t SP6[64] = {
    0x20000010L, 0x20400000L, 0x00004000L, 0x20404010L,
    0x20400000L, 0x00000010L, 0x20404010L, 0x00400000L,
    0x20004000L, 0x00404010L, 0x00400000L, 0x20000010L,
    0x00400010L, 0x20004000L, 0x20000000L, 0x00004010L,
    0x00000000L, 0x00400010L, 0x20004010L, 0x00004000L,
    0x00404000L, 0x20004010L, 0x00000010L, 0x20400010L,
    0x20400010L, 0x00000000L, 0x00404010L, 0x20404000L,
    0x00004010L, 0x00404000L, 0x20404000L, 0x20000000L,
    0x20004000L, 0x00000010L, 0x20400010L, 0x00404000L,
    0x20404010L, 0x00400000L, 0x00004010L, 0x20000010L,
    0x00400000L, 0x20004000L, 0x20000000L, 0x00004010L,
    0x20000010L, 0x20404010L, 0x00404000L, 0x20400000L,
    0x00404010L, 0x20404000L, 0x00000000L, 0x20400010L,
    0x00000010L, 0x00004000L, 0x20400000L, 0x00404010L,
    0x00004000L, 0x00400010L, 0x20004010L, 0x00000000L,
    0x20404000L, 0x20000000L, 0x00400010L, 0x20004010L };

static uint32_t SP7[64] = {
    0x00200000L, 0x04200002L, 0x04000802L, 0x00000000L,
    0x00000800L, 0x04000802L, 0x00200802L, 0x04200800L,
    0x04200802L, 0x00200000L, 0x00000000L, 0x04000002L,
    0x00000002L, 0x04000000L, 0x04200002L, 0x00000802L,
    0x04000800L, 0x00200802L, 0x00200002L, 0x04000800L,
    0x04000002L, 0x04200000L, 0x04200800L, 0x00200002L,
    0x04200000L, 0x00000800L, 0x00000802L, 0x04200802L,
    0x00200800L, 0x00000002L, 0x04000000L, 0x00200800L,
    0x04000000L, 0x00200800L, 0x00200000L, 0x04000802L,
    0x04000802L, 0x04200002L, 0x04200002L, 0x00000002L,
    0x00200002L, 0x04000000L, 0x04000800L, 0x00200000L,
    0x04200800L, 0x00000802L, 0x00200802L, 0x04200800L,
    0x00000802L, 0x04000002L, 0x04200802L, 0x04200000L,
    0x00200800L, 0x00000000L, 0x00000002L, 0x04200802L,
    0x00000000L, 0x00200802L, 0x04200000L, 0x00000800L,
    0x04000002L, 0x04000800L, 0x00000800L, 0x00200002L };

static uint32_t SP8[64] = {
    0x10001040L, 0x00001000L, 0x00040000L, 0x10041040L,
    0x10000000L, 0x10001040L, 0x00000040L, 0x10000000L,
    0x00040040L, 0x10040000L, 0x10041040L, 0x00041000L,
    0x10041000L, 0x00041040L, 0x00001000L, 0x00000040L,
    0x10040000L, 0x10000040L, 0x10001000L, 0x00001040L,
    0x00041000L, 0x00040040L, 0x10040040L, 0x10041000L,
    0x00001040L, 0x00000000L, 0x00000000L, 0x10040040L,
    0x10000040L, 0x10001000L, 0x00041040L, 0x00040000L,
    0x00041040L, 0x00040000L, 0x10041000L, 0x00001000L,
    0x00000040L, 0x10040040L, 0x00001000L, 0x00041040L,
    0x10001000L, 0x00000040L, 0x10000040L, 0x10040000L,
    0x10040040L, 0x10000000L, 0x00040000L, 0x10001040L,
    0x00000000L, 0x10041040L, 0x00040040L, 0x10000040L,
    0x10040000L, 0x10001000L, 0x10001040L, 0x00000000L,
    0x10041040L, 0x00041000L, 0x00041000L, 0x00001040L,
    0x00001040L, 0x00040040L, 0x10000000L, 0x10041000L };

static void
IP(uint32_t v[2])
{
    uint32_t work;

    work = ((v[0] >> 4) ^ v[1]) & 0x0f0f0f0fL;
    v[1] ^= work;
    v[0] ^= (work << 4);
    work = ((v[0] >> 16) ^ v[1]) & 0x0000ffffL;
    v[1] ^= work;
    v[0] ^= (work << 16);
    work = ((v[1] >> 2) ^ v[0]) & 0x33333333L;
    v[0] ^= work;
    v[1] ^= (work << 2);
    work = ((v[1] >> 8) ^ v[0]) & 0x00ff00ffL;
    v[0] ^= work;
    v[1] ^= (work << 8);
    v[1] = ((v[1] << 1) | ((v[1] >> 31) & 1L)) & 0xffffffffL;
    work = (v[0] ^ v[1]) & 0xaaaaaaaaL;
    v[0] ^= work;
    v[1] ^= work;
    v[0] = ((v[0] << 1) | ((v[0] >> 31) & 1L)) & 0xffffffffL;
}

static void
FP(uint32_t v[2])
{
    uint32_t work;

    v[0] = (v[0] << 31) | (v[0] >> 1);
    work = (v[1] ^ v[0]) & 0xaaaaaaaaL;
    v[1] ^= work;
    v[0] ^= work;
    v[1] = (v[1] << 31) | (v[1] >> 1);
    work = ((v[1] >> 8) ^ v[0]) & 0x00ff00ffL;
    v[0] ^= work;
    v[1] ^= (work << 8);
    work = ((v[1] >> 2) ^ v[0]) & 0x33333333L;
    v[0] ^= work;
    v[1] ^= (work << 2);
    work = ((v[0] >> 16) ^ v[1]) & 0x0000ffffL;
    v[1] ^= work;
    v[0] ^= (work << 16);
    work = ((v[0] >> 4) ^ v[1]) & 0x0f0f0f0fL;
    v[1] ^= work;
    v[0] ^= (work << 4);
}

static void
desx(uint32_t block[2], DES_key_schedule *ks, int encp)
{
    uint32_t *keys;
    uint32_t fval, work, right, left;
    int round;

    left = block[0];
    right = block[1];

    if (encp) {
	keys = &ks->ks[0];

	for( round = 0; round < 8; round++ ) {
	    work  = (right << 28) | (right >> 4);
	    work ^= *keys++;
	    fval  = SP7[ work     & 0x3fL];
	    fval |= SP5[(work >>  8) & 0x3fL];
	    fval |= SP3[(work >> 16) & 0x3fL];
	    fval |= SP1[(work >> 24) & 0x3fL];
	    work  = right ^ *keys++;
	    fval |= SP8[ work     & 0x3fL];
	    fval |= SP6[(work >>  8) & 0x3fL];
	    fval |= SP4[(work >> 16) & 0x3fL];
	    fval |= SP2[(work >> 24) & 0x3fL];
	    left ^= fval;
	    work  = (left << 28) | (left >> 4);
	    work ^= *keys++;
	    fval  = SP7[ work     & 0x3fL];
	    fval |= SP5[(work >>  8) & 0x3fL];
	    fval |= SP3[(work >> 16) & 0x3fL];
	    fval |= SP1[(work >> 24) & 0x3fL];
	    work  = left ^ *keys++;
	    fval |= SP8[ work     & 0x3fL];
	    fval |= SP6[(work >>  8) & 0x3fL];
	    fval |= SP4[(work >> 16) & 0x3fL];
	    fval |= SP2[(work >> 24) & 0x3fL];
	    right ^= fval;
	}
    } else {
	keys = &ks->ks[30];

	for( round = 0; round < 8; round++ ) {
	    work  = (right << 28) | (right >> 4);
	    work ^= *keys++;
	    fval  = SP7[ work     & 0x3fL];
	    fval |= SP5[(work >>  8) & 0x3fL];
	    fval |= SP3[(work >> 16) & 0x3fL];
	    fval |= SP1[(work >> 24) & 0x3fL];
	    work  = right ^ *keys++;
	    fval |= SP8[ work     & 0x3fL];
	    fval |= SP6[(work >>  8) & 0x3fL];
	    fval |= SP4[(work >> 16) & 0x3fL];
	    fval |= SP2[(work >> 24) & 0x3fL];
	    left ^= fval;
	    work  = (left << 28) | (left >> 4);
	    keys -= 4;
	    work ^= *keys++;
	    fval  = SP7[ work     & 0x3fL];
	    fval |= SP5[(work >>  8) & 0x3fL];
	    fval |= SP3[(work >> 16) & 0x3fL];
	    fval |= SP1[(work >> 24) & 0x3fL];
	    work  = left ^ *keys++;
	    fval |= SP8[ work     & 0x3fL];
	    fval |= SP6[(work >>  8) & 0x3fL];
	    fval |= SP4[(work >> 16) & 0x3fL];
	    fval |= SP2[(work >> 24) & 0x3fL];
	    right ^= fval;
	    keys -= 4;
	}
    }
    block[0] = right;
    block[1] = left;
}
