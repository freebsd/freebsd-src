/*
 * Copyright (c) 2003-2004 Kungliga Tekniska Högskolan
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

/* $Id$ */

#ifndef HEIM_AES_H
#define HEIM_AES_H 1

/* symbol renaming */
#define AES_set_encrypt_key hc_AES_set_encrypt_key
#define AES_set_decrypt_key hc_AES_decrypt_key
#define AES_encrypt hc_AES_encrypt
#define AES_decrypt hc_AES_decrypt
#define AES_cbc_encrypt hc_AES_cbc_encrypt
#define AES_cfb8_encrypt hc_AES_cfb8_encrypt

/*
 *
 */

#define AES_BLOCK_SIZE 16
#define AES_MAXNR 14

#define AES_ENCRYPT 1
#define AES_DECRYPT 0

typedef struct aes_key {
    uint32_t key[(AES_MAXNR+1)*4];
    int rounds;
} AES_KEY;

#ifdef __cplusplus
extern "C" {
#endif

int AES_set_encrypt_key(const unsigned char *, const int, AES_KEY *);
int AES_set_decrypt_key(const unsigned char *, const int, AES_KEY *);

void AES_encrypt(const unsigned char *, unsigned char *, const AES_KEY *);
void AES_decrypt(const unsigned char *, unsigned char *, const AES_KEY *);

void AES_cbc_encrypt(const unsigned char *, unsigned char *,
		     unsigned long, const AES_KEY *,
		     unsigned char *, int);
void AES_cfb8_encrypt(const unsigned char *, unsigned char *,
		      unsigned long, const AES_KEY *,
		      unsigned char *, int);

#ifdef  __cplusplus
}
#endif

#endif /* HEIM_AES_H */
