/*
 * Copyright (c) 2003 Kungliga Tekniska Högskolan
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

#include "rijndael-alg-fst.h"
#include "aes.h"

int
AES_set_encrypt_key(const unsigned char *userkey, const int bits, AES_KEY *key)
{
    key->rounds = rijndaelKeySetupEnc(key->key, userkey, bits);
    if (key->rounds == 0)
	return -1;
    return 0;
}

int
AES_set_decrypt_key(const unsigned char *userkey, const int bits, AES_KEY *key)
{
    key->rounds = rijndaelKeySetupDec(key->key, userkey, bits);
    if (key->rounds == 0)
	return -1;
    return 0;
}

void
AES_encrypt(const unsigned char *in, unsigned char *out, const AES_KEY *key)
{
    rijndaelEncrypt(key->key, key->rounds, in, out);
}

void
AES_decrypt(const unsigned char *in, unsigned char *out, const AES_KEY *key)
{
    rijndaelDecrypt(key->key, key->rounds, in, out);
}

void
AES_cbc_encrypt(const unsigned char *in, unsigned char *out,
		unsigned long size, const AES_KEY *key,
		unsigned char *iv, int forward_encrypt)
{
    unsigned char tmp[AES_BLOCK_SIZE];
    int i;

    if (forward_encrypt) {
	while (size >= AES_BLOCK_SIZE) {
	    for (i = 0; i < AES_BLOCK_SIZE; i++)
		tmp[i] = in[i] ^ iv[i];
	    AES_encrypt(tmp, out, key);
	    memcpy(iv, out, AES_BLOCK_SIZE);
	    size -= AES_BLOCK_SIZE;
	    in += AES_BLOCK_SIZE;
	    out += AES_BLOCK_SIZE;
	}
	if (size) {
	    for (i = 0; i < size; i++)
		tmp[i] = in[i] ^ iv[i];
	    for (i = size; i < AES_BLOCK_SIZE; i++)
		tmp[i] = iv[i];
	    AES_encrypt(tmp, out, key);
	    memcpy(iv, out, AES_BLOCK_SIZE);
	}
    } else {
	while (size >= AES_BLOCK_SIZE) {
	    memcpy(tmp, in, AES_BLOCK_SIZE);
	    AES_decrypt(tmp, out, key);
	    for (i = 0; i < AES_BLOCK_SIZE; i++)
		out[i] ^= iv[i];
	    memcpy(iv, tmp, AES_BLOCK_SIZE);
	    size -= AES_BLOCK_SIZE;
	    in += AES_BLOCK_SIZE;
	    out += AES_BLOCK_SIZE;
	}
	if (size) {
	    memcpy(tmp, in, AES_BLOCK_SIZE);
	    AES_decrypt(tmp, out, key);
	    for (i = 0; i < size; i++)
		out[i] ^= iv[i];
	    memcpy(iv, tmp, AES_BLOCK_SIZE);
	}
    }
}

void
AES_cfb8_encrypt(const unsigned char *in, unsigned char *out,
                 unsigned long size, const AES_KEY *key,
                 unsigned char *iv, int forward_encrypt)
{
    int i;

    for (i = 0; i < size; i++) {
        unsigned char tmp[AES_BLOCK_SIZE + 1];

        memcpy(tmp, iv, AES_BLOCK_SIZE);
        AES_encrypt(iv, iv, key);
        if (!forward_encrypt) {
            tmp[AES_BLOCK_SIZE] = in[i];
        }
        out[i] = in[i] ^ iv[0];
        if (forward_encrypt) {
            tmp[AES_BLOCK_SIZE] = out[i];
        }
        memcpy(iv, &tmp[1], AES_BLOCK_SIZE);
    }
}
