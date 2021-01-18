/*-
 * Copyright (c) 2010 Konstantin Belousov <kib@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _AESNI_H_
#define _AESNI_H_

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/queue.h>

#include <opencrypto/cryptodev.h>

#if defined(__amd64__) || defined(__i386__)
#include <machine/cpufunc.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>
#include <machine/fpu.h>
#endif

#define	AES128_ROUNDS	10
#define	AES192_ROUNDS	12
#define	AES256_ROUNDS	14
#define	AES_SCHED_LEN	((AES256_ROUNDS + 1) * AES_BLOCK_LEN)
#define	AES_SCHED_ALIGN	16

struct aesni_session {
	uint8_t schedules[3 * AES_SCHED_LEN + AES_SCHED_ALIGN];
	uint8_t *enc_schedule;
	uint8_t *dec_schedule;
	uint8_t *xts_schedule;
	int rounds;
	/* uint8_t *ses_ictx; */
	/* uint8_t *ses_octx; */
	int used;
	int mlen;
	int hash_len;
	void (*hash_init)(void *);
	int (*hash_update)(void *, const void *, u_int);
	void (*hash_finalize)(void *, void *);
	bool hmac;
};

/*
 * Internal functions, implemented in assembler.
 */
void aesni_set_enckey(const uint8_t *userkey,
    uint8_t *encrypt_schedule /*__aligned(16)*/, int number_of_rounds);
void aesni_set_deckey(const uint8_t *encrypt_schedule /*__aligned(16)*/,
    uint8_t *decrypt_schedule /*__aligned(16)*/, int number_of_rounds);

/*
 * Slightly more public interfaces.
 */
void aesni_encrypt_cbc(int rounds, const void *key_schedule /*__aligned(16)*/,
    size_t len, const uint8_t *from, uint8_t *to,
    const uint8_t iv[__min_size(AES_BLOCK_LEN)]);
void aesni_decrypt_cbc(int rounds, const void *key_schedule /*__aligned(16)*/,
    size_t len, uint8_t *buf, const uint8_t iv[__min_size(AES_BLOCK_LEN)]);
void aesni_encrypt_ecb(int rounds, const void *key_schedule /*__aligned(16)*/,
    size_t len, const uint8_t *from, uint8_t *to);
void aesni_decrypt_ecb(int rounds, const void *key_schedule /*__aligned(16)*/,
    size_t len, const uint8_t *from, uint8_t *to);
void aesni_encrypt_icm(int rounds, const void *key_schedule /*__aligned(16)*/,
    size_t len, const uint8_t *from, uint8_t *to,
    const uint8_t iv[__min_size(AES_BLOCK_LEN)]);

void aesni_encrypt_xts(int rounds, const void *data_schedule /*__aligned(16)*/,
    const void *tweak_schedule /*__aligned(16)*/, size_t len,
    const uint8_t *from, uint8_t *to,
    const uint8_t iv[__min_size(AES_BLOCK_LEN)]);
void aesni_decrypt_xts(int rounds, const void *data_schedule /*__aligned(16)*/,
    const void *tweak_schedule /*__aligned(16)*/, size_t len,
    const uint8_t *from, uint8_t *to,
    const uint8_t iv[__min_size(AES_BLOCK_LEN)]);

/* GCM & GHASH functions */
void AES_GCM_encrypt(const unsigned char *in, unsigned char *out,
    const unsigned char *addt, const unsigned char *ivec,
    unsigned char *tag, uint32_t nbytes, uint32_t abytes, int ibytes,
    const unsigned char *key, int nr);
int AES_GCM_decrypt(const unsigned char *in, unsigned char *out,
    const unsigned char *addt, const unsigned char *ivec,
    const unsigned char *tag, uint32_t nbytes, uint32_t abytes, int ibytes,
    const unsigned char *key, int nr);

/* CCM + CBC-MAC functions */
void AES_CCM_encrypt(const unsigned char *in, unsigned char *out,
    const unsigned char *addt, const unsigned char *ivec,
    unsigned char *tag, uint32_t nbytes, uint32_t abytes, int ibytes,
    const unsigned char *key, int nr);
int AES_CCM_decrypt(const unsigned char *in, unsigned char *out,
    const unsigned char *addt, const unsigned char *ivec,
    const unsigned char *tag, uint32_t nbytes, uint32_t abytes, int ibytes,
    const unsigned char *key, int nr);
void aesni_cipher_setup_common(struct aesni_session *ses,
    const struct crypto_session_params *csp, const uint8_t *key, int keylen);

#endif /* _AESNI_H_ */
