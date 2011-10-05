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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <pkcs12.h>
#include <bn.h>

#include <roken.h>

int
PKCS12_key_gen(const void *key, size_t keylen,
	       const void *salt, size_t saltlen,
	       int id, int iteration, size_t outkeysize,
	       void *out, const EVP_MD *md)
{
    unsigned char *v, *I, hash[EVP_MAX_MD_SIZE];
    unsigned int size, size_I = 0;
    unsigned char idc = id;
    EVP_MD_CTX *ctx;
    unsigned char *outp = out;
    int i, vlen;

    /**
     * The argument key is pointing to an utf16 string, and thus
     * keylen that is no a multiple of 2 is invalid.
     */
    if (keylen & 1)
	return 0;

    ctx = EVP_MD_CTX_create();
    if (ctx == NULL)
	return 0;

    vlen = EVP_MD_block_size(md);
    v = malloc(vlen + 1);
    if (v == NULL) {
	EVP_MD_CTX_destroy(ctx);
	return 0;
    }

    I = calloc(1, vlen * 2);
    if (I == NULL) {
	EVP_MD_CTX_destroy(ctx);
	free(v);
	return 0;
    }

    if (salt && saltlen > 0) {
	for (i = 0; i < vlen; i++)
	    I[i] = ((unsigned char*)salt)[i % saltlen];
	size_I += vlen;
    }
    /*
     * There is a diffrence between the no password string and the
     * empty string, in the empty string the UTF16 NUL terminator is
     * included into the string.
     */
    if (key) {
	for (i = 0; i < vlen / 2; i++) {
	    I[(i * 2) + size_I] = 0;
	    I[(i * 2) + size_I + 1] = ((unsigned char*)key)[i % (keylen + 1)];
	}
	size_I += vlen;
    }

    while (1) {
	BIGNUM *bnB, *bnOne;

	if (!EVP_DigestInit_ex(ctx, md, NULL)) {
	    EVP_MD_CTX_destroy(ctx);
	    free(I);
	    free(v);
	    return 0;
	}
	for (i = 0; i < vlen; i++)
	    EVP_DigestUpdate(ctx, &idc, 1);
	EVP_DigestUpdate(ctx, I, size_I);
	EVP_DigestFinal_ex(ctx, hash, &size);

	for (i = 1; i < iteration; i++)
	    EVP_Digest(hash, size, hash, &size, md, NULL);

	memcpy(outp, hash, min(outkeysize, size));
	if (outkeysize < size)
	    break;
	outkeysize -= size;
	outp += size;

	for (i = 0; i < vlen; i++)
	    v[i] = hash[i % size];

	bnB = BN_bin2bn(v, vlen, NULL);
	bnOne = BN_new();
	BN_set_word(bnOne, 1);

	BN_uadd(bnB, bnB, bnOne);

	for (i = 0; i < vlen * 2; i += vlen) {
	    BIGNUM *bnI;
	    int j;

	    bnI = BN_bin2bn(I + i, vlen, NULL);

	    BN_uadd(bnI, bnI, bnB);

	    j = BN_num_bytes(bnI);
	    if (j > vlen) {
		assert(j == vlen + 1);
		BN_bn2bin(bnI, v);
		memcpy(I + i, v + 1, vlen);
	    } else {
		memset(I + i, 0, vlen - j);
		BN_bn2bin(bnI, I + i + vlen - j);
	    }
	    BN_free(bnI);
	}
	BN_free(bnB);
	BN_free(bnOne);
	size_I = vlen * 2;
    }

    EVP_MD_CTX_destroy(ctx);
    free(I);
    free(v);

    return 1;
}
