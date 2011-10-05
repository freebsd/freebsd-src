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

/* $Id$ */

#ifndef HEIM_HMAC_H
#define HEIM_HMAC_H 1

#include <hcrypto/evp.h>

/* symbol renaming */
#define HMAC_CTX_init hc_HMAC_CTX_init
#define HMAC_CTX_cleanup hc_HMAC_CTX_cleanup
#define HMAC_size hc_HMAC_size
#define HMAC_Init_ex hc_HMAC_Init_ex
#define HMAC_Update hc_HMAC_Update
#define HMAC_Final hc_HMAC_Final
#define HMAC hc_HMAC

/*
 *
 */

#define HMAC_MAX_MD_CBLOCK	64

typedef struct hc_HMAC_CTX HMAC_CTX;

struct hc_HMAC_CTX {
    const EVP_MD *md;
    ENGINE *engine;
    EVP_MD_CTX *ctx;
    size_t key_length;
    void *opad;
    void *ipad;
    void *buf;
};


void	HMAC_CTX_init(HMAC_CTX *);
void	HMAC_CTX_cleanup(HMAC_CTX *ctx);

size_t	HMAC_size(const HMAC_CTX *ctx);

void	HMAC_Init_ex(HMAC_CTX *, const void *, size_t,
		     const EVP_MD *, ENGINE *);
void	HMAC_Update(HMAC_CTX *ctx, const void *data, size_t len);
void	HMAC_Final(HMAC_CTX *ctx, void *md, unsigned int *len);

void *	HMAC(const EVP_MD *evp_md, const void *key, size_t key_len,
	     const void *data, size_t n, void *md, unsigned int *md_len);

#endif /* HEIM_HMAC_H */
