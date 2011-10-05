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

/* $Id$ */

#ifndef HEIM_EVP_HCRYPTO_H
#define HEIM_EVP_HCRYPTO_H 1

/* symbol renaming */
#define EVP_hcrypto_md2 hc_EVP_hcrypto_md2
#define EVP_hcrypto_md4 hc_EVP_hcrypto_md4
#define EVP_hcrypto_md5 hc_EVP_hcrypto_md5
#define EVP_hcrypto_sha1 hc_EVP_hcrypto_sha1
#define EVP_hcrypto_sha256 hc_EVP_hcrypto_sha256
#define EVP_hcrypto_sha384 hc_EVP_hcrypto_sha384
#define EVP_hcrypto_sha512 hc_EVP_hcrypto_sha512
#define EVP_hcrypto_des_cbc hc_EVP_hcrypto_des_cbc
#define EVP_hcrypto_des_ede3_cbc hc_EVP_hcrypto_des_ede3_cbc
#define EVP_hcrypto_aes_128_cbc hc_EVP_hcrypto_aes_128_cbc
#define EVP_hcrypto_aes_192_cbc hc_EVP_hcrypto_aes_192_cbc
#define EVP_hcrypto_aes_256_cbc hc_EVP_hcrypto_aes_256_cbc
#define EVP_hcrypto_aes_128_cfb8 hc_EVP_hcrypto_aes_128_cfb8
#define EVP_hcrypto_aes_192_cfb8 hc_EVP_hcrypto_aes_192_cfb8
#define EVP_hcrypto_aes_256_cfb8 hc_EVP_hcrypto_aes_256_cfb8
#define EVP_hcrypto_rc4 hc_EVP_hcrypto_rc4
#define EVP_hcrypto_rc4_40 hc_EVP_hcrypto_rc4_40
#define EVP_hcrypto_rc2_40_cbc hc_EVP_hcrypto_rc2_40_cbc
#define EVP_hcrypto_rc2_64_cbc hc_EVP_hcrypto_rc2_64_cbc
#define EVP_hcrypto_rc2_cbc hc_EVP_hcrypto_rc2_cbc
#define EVP_hcrypto_camellia_128_cbc hc_EVP_hcrypto_camellia_128_cbc
#define EVP_hcrypto_camellia_192_cbc hc_EVP_hcrypto_camellia_192_cbc
#define EVP_hcrypto_camellia_256_cbc hc_EVP_hcrypto_camellia_256_cbc

/*
 *
 */

HC_CPP_BEGIN

const EVP_MD * EVP_hcrypto_md2(void);
const EVP_MD * EVP_hcrypto_md4(void);
const EVP_MD * EVP_hcrypto_md5(void);
const EVP_MD * EVP_hcrypto_sha1(void);
const EVP_MD * EVP_hcrypto_sha256(void);
const EVP_MD * EVP_hcrypto_sha384(void);
const EVP_MD * EVP_hcrypto_sha512(void);

const EVP_CIPHER * EVP_hcrypto_rc4(void);
const EVP_CIPHER * EVP_hcrypto_rc4_40(void);

const EVP_CIPHER * EVP_hcrypto_rc2_cbc(void);
const EVP_CIPHER * EVP_hcrypto_rc2_40_cbc(void);
const EVP_CIPHER * EVP_hcrypto_rc2_64_cbc(void);

const EVP_CIPHER * EVP_hcrypto_des_cbc(void);
const EVP_CIPHER * EVP_hcrypto_des_ede3_cbc(void);

const EVP_CIPHER * EVP_hcrypto_aes_128_cbc(void);
const EVP_CIPHER * EVP_hcrypto_aes_192_cbc(void);
const EVP_CIPHER * EVP_hcrypto_aes_256_cbc(void);

const EVP_CIPHER * EVP_hcrypto_aes_128_cfb8(void);
const EVP_CIPHER * EVP_hcrypto_aes_192_cfb8(void);
const EVP_CIPHER * EVP_hcrypto_aes_256_cfb8(void);

const EVP_CIPHER * EVP_hcrypto_camellia_128_cbc(void);
const EVP_CIPHER * EVP_hcrypto_camellia_192_cbc(void);
const EVP_CIPHER * EVP_hcrypto_camellia_256_cbc(void);


HC_CPP_END

#endif /* HEIM_EVP_HCRYPTO_H */
