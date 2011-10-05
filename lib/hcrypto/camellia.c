/*
 * Copyright (c) 2007 Kungliga Tekniska Högskolan
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

#ifdef KRB5
#include <krb5-types.h>
#endif

#include <string.h>

#include "camellia-ntt.h"
#include "camellia.h"

#include <roken.h>

int
CAMELLIA_set_key(const unsigned char *userkey,
		 const int bits, CAMELLIA_KEY *key)
{
    key->bits = bits;
    Camellia_Ekeygen(bits, userkey, key->key);
    return 1;
}

void
CAMELLIA_encrypt(const unsigned char *in, unsigned char *out,
		 const CAMELLIA_KEY *key)
{
    Camellia_EncryptBlock(key->bits, in, key->key, out);

}

void
CAMELLIA_decrypt(const unsigned char *in, unsigned char *out,
		 const CAMELLIA_KEY *key)
{
    Camellia_DecryptBlock(key->bits, in, key->key, out);
}

void
CAMELLIA_cbc_encrypt(const unsigned char *in, unsigned char *out,
		     unsigned long size, const CAMELLIA_KEY *key,
		     unsigned char *iv, int mode_encrypt)
{
    unsigned char tmp[CAMELLIA_BLOCK_SIZE];
    int i;

    if (mode_encrypt) {
	while (size >= CAMELLIA_BLOCK_SIZE) {
	    for (i = 0; i < CAMELLIA_BLOCK_SIZE; i++)
		tmp[i] = in[i] ^ iv[i];
	    CAMELLIA_encrypt(tmp, out, key);
	    memcpy(iv, out, CAMELLIA_BLOCK_SIZE);
	    size -= CAMELLIA_BLOCK_SIZE;
	    in += CAMELLIA_BLOCK_SIZE;
	    out += CAMELLIA_BLOCK_SIZE;
	}
	if (size) {
	    for (i = 0; i < size; i++)
		tmp[i] = in[i] ^ iv[i];
	    for (i = size; i < CAMELLIA_BLOCK_SIZE; i++)
		tmp[i] = iv[i];
	    CAMELLIA_encrypt(tmp, out, key);
	    memcpy(iv, out, CAMELLIA_BLOCK_SIZE);
	}
    } else {
	while (size >= CAMELLIA_BLOCK_SIZE) {
	    memcpy(tmp, in, CAMELLIA_BLOCK_SIZE);
	    CAMELLIA_decrypt(tmp, out, key);
	    for (i = 0; i < CAMELLIA_BLOCK_SIZE; i++)
		out[i] ^= iv[i];
	    memcpy(iv, tmp, CAMELLIA_BLOCK_SIZE);
	    size -= CAMELLIA_BLOCK_SIZE;
	    in += CAMELLIA_BLOCK_SIZE;
	    out += CAMELLIA_BLOCK_SIZE;
	}
	if (size) {
	    memcpy(tmp, in, CAMELLIA_BLOCK_SIZE);
	    CAMELLIA_decrypt(tmp, out, key);
	    for (i = 0; i < size; i++)
		out[i] ^= iv[i];
	    memcpy(iv, tmp, CAMELLIA_BLOCK_SIZE);
	}
    }
}
