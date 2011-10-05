/*
 * Copyright (c) 2009 Kungliga Tekniska Högskolan
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

#ifndef HEIM_EC_H
#define HEIM_EC_H 1

#define EC_GROUP_get_degree hc_EC_GROUP_get_degree
#define EC_KEY_get0_group hc_EC_KEY_get0_group
#define EC_GROUP_get_order hc_EC_GROUP_get_order
#define o2i_ECPublicKey hc_o2i_ECPublicKey
#define EC_KEY_free hc_EC_KEY_free
#define EC_GROUP_new_by_curve_name hc_EC_GROUP_new_by_curve_name
#define EC_KEY_set_group hc_EC_KEY_set_group
#define EC_GROUP_free hc_EC_GROUP_free
#define EC_KEY_check_key hc_EC_KEY_check_key
#define EC_KEY_get0_private_key hc_EC_KEY_get0_private_key
#define EC_KEY_set_private_key hc_EC_KEY_set_private_key

#include <hcrypto/bn.h>
#include <hcrypto/engine.h>

typedef struct EC_KEY EC_KEY;
typedef struct EC_GROUP EC_GROUP;
typedef struct EC_GROUP_ID_s *EC_GROUP_ID;

unsigned long
EC_GROUP_get_degree(EC_GROUP *);

EC_GROUP *
EC_KEY_get0_group(EC_KEY *);

int
EC_GROUP_get_order(EC_GROUP *, BIGNUM *, BN_CTX *);

EC_KEY *
o2i_ECPublicKey(EC_KEY **key, unsigned char **, size_t);

EC_KEY *
EC_KEY_new_by_curve_name(EC_GROUP_ID);

int
EC_KEY_generate_key(EC_KEY *);

void
EC_KEY_free(EC_KEY *);

EC_GROUP *
EC_GROUP_new_by_curve_name(int nid);

void
EC_KEY_set_group(EC_KEY *, EC_GROUP *);

void
EC_GROUP_free(EC_GROUP *);

int
EC_KEY_check_key(const EC_KEY *);

const BIGNUM *EC_KEY_get0_private_key(const EC_KEY *);

int EC_KEY_set_private_key(EC_KEY *, const BIGNUM *);

#endif /* HEIM_EC_H */
