/*
 * Copyright (c) 2004 Kungliga Tekniska Högskolan
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

/* symbol renaming */
#define RC2_set_key hc_RC2_set_key
#define RC2_encryptc hc_RC2_encryptc
#define RC2_decryptc hc_RC2_decryptc
#define RC2_cbc_encrypt hc_RC2_cbc_encrypt

/*
 *
 */

#define RC2_ENCRYPT	1
#define RC2_DECRYPT	0

#define RC2_BLOCK_SIZE	8
#define RC2_BLOCK	RC2_BLOCK_SIZE
#define RC2_KEY_LENGTH	16

typedef struct rc2_key {
    unsigned int data[64];
} RC2_KEY;

#ifdef __cplusplus
extern "C" {
#endif

void RC2_set_key(RC2_KEY *, int, const unsigned char *,int);

void RC2_encryptc(unsigned char *, unsigned char *, const RC2_KEY *);
void RC2_decryptc(unsigned char *, unsigned char *, const RC2_KEY *);

void RC2_cbc_encrypt(const unsigned char *, unsigned char *, long,
		     RC2_KEY *, unsigned char *, int);

#ifdef  __cplusplus
}
#endif
