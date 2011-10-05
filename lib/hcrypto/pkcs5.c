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

#ifdef KRB5
#include <krb5-types.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include <evp.h>
#include <hmac.h>

#include <roken.h>

/**
 * As descriped in PKCS5, convert a password, salt, and iteration counter into a crypto key.
 *
 * @param password Password.
 * @param password_len Length of password.
 * @param salt Salt
 * @param salt_len Length of salt.
 * @param iter iteration counter.
 * @param keylen the output key length.
 * @param key the output key.
 *
 * @return 1 on success, non 1 on failure.
 *
 * @ingroup hcrypto_misc
 */

int
PKCS5_PBKDF2_HMAC_SHA1(const void * password, size_t password_len,
		       const void * salt, size_t salt_len,
		       unsigned long iter,
		       size_t keylen, void *key)
{
    size_t datalen, leftofkey, checksumsize;
    char *data, *tmpcksum;
    uint32_t keypart;
    const EVP_MD *md;
    unsigned long i;
    int j;
    char *p;
    unsigned int hmacsize;

    md = EVP_sha1();
    checksumsize = EVP_MD_size(md);
    datalen = salt_len + 4;

    tmpcksum = malloc(checksumsize + datalen);
    if (tmpcksum == NULL)
	return 0;

    data = &tmpcksum[checksumsize];

    memcpy(data, salt, salt_len);

    keypart = 1;
    leftofkey = keylen;
    p = key;

    while (leftofkey) {
	int len;

	if (leftofkey > checksumsize)
	    len = checksumsize;
	else
	    len = leftofkey;

	data[datalen - 4] = (keypart >> 24) & 0xff;
	data[datalen - 3] = (keypart >> 16) & 0xff;
	data[datalen - 2] = (keypart >> 8)  & 0xff;
	data[datalen - 1] = (keypart)       & 0xff;

	HMAC(md, password, password_len, data, datalen,
	     tmpcksum, &hmacsize);

	memcpy(p, tmpcksum, len);
	for (i = 1; i < iter; i++) {
	    HMAC(md, password, password_len, tmpcksum, checksumsize,
		 tmpcksum, &hmacsize);

	    for (j = 0; j < len; j++)
		p[j] ^= tmpcksum[j];
	}

	p += len;
	leftofkey -= len;
	keypart++;
    }

    free(tmpcksum);

    return 1;
}
