/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/crypto/builtin/sha2/sha2.h - SHA-256 declarations */
/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
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

#ifndef SHA2_H
#define SHA2_H 1

#include <k5-int.h>

#define SHA256_DIGEST_LENGTH 32
#define SHA384_DIGEST_LENGTH 48
#define SHA512_DIGEST_LENGTH 64

#define SHA256_BLOCK_SIZE 64
#define SHA384_BLOCK_SIZE 128
#define SHA512_BLOCK_SIZE 128

struct sha256state {
  unsigned int sz[2];
  uint32_t counter[8];
  unsigned char save[64];
};

struct sha512state {
  uint64_t sz[2];
  uint64_t counter[8];
  unsigned char save[128];
};

/* SHA-384 and SHA-512 use the same state object. */
typedef struct sha256state SHA256_CTX;
typedef struct sha512state SHA384_CTX;
typedef struct sha512state SHA512_CTX;

void k5_sha256_init(SHA256_CTX *);
void k5_sha256_update(SHA256_CTX *, const void *, size_t);
void k5_sha256_final(void *, SHA256_CTX *);

void k5_sha384_init(SHA384_CTX *);
void k5_sha384_update(SHA384_CTX *, const void *, size_t);
void k5_sha384_final(void *, SHA384_CTX *);

void k5_sha512_init(SHA512_CTX *);
void k5_sha512_update(SHA512_CTX *, const void *, size_t);
void k5_sha512_final(void *, SHA512_CTX *);

#endif /* SHA2_H */
