/*-
 * Copyright (C) 2008 Damien Miller <djm@mindrot.org>
 * Copyright (c) 2010 Konstantin Belousov <kib@FreeBSD.org>
 * Copyright (c) 2010-2011 Pawel Jakub Dawidek <pawel@dawidek.net>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/libkern.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <crypto/aesni/aesni.h>

MALLOC_DECLARE(M_AESNI);

void
aesni_encrypt_cbc(int rounds, const void *key_schedule, size_t len,
    const uint8_t *from, uint8_t *to, const uint8_t iv[AES_BLOCK_LEN])
{
	const uint8_t *ivp;
	size_t i;

	len /= AES_BLOCK_LEN;
	ivp = iv;
	for (i = 0; i < len; i++) {
		aesni_enc(rounds - 1, key_schedule, from, to, ivp);
		ivp = to;
		from += AES_BLOCK_LEN;
		to += AES_BLOCK_LEN;
	}
}

void
aesni_encrypt_ecb(int rounds, const void *key_schedule, size_t len,
    const uint8_t from[AES_BLOCK_LEN], uint8_t to[AES_BLOCK_LEN])
{
	size_t i;

	len /= AES_BLOCK_LEN;
	for (i = 0; i < len; i++) {
		aesni_enc(rounds - 1, key_schedule, from, to, NULL);
		from += AES_BLOCK_LEN;
		to += AES_BLOCK_LEN;
	}
}

void
aesni_decrypt_ecb(int rounds, const void *key_schedule, size_t len,
    const uint8_t from[AES_BLOCK_LEN], uint8_t to[AES_BLOCK_LEN])
{
	size_t i;

	len /= AES_BLOCK_LEN;
	for (i = 0; i < len; i++) {
		aesni_dec(rounds - 1, key_schedule, from, to, NULL);
		from += AES_BLOCK_LEN;
		to += AES_BLOCK_LEN;
	}
}

#define	AES_XTS_BLOCKSIZE	16
#define	AES_XTS_IVSIZE		8
#define	AES_XTS_ALPHA		0x87	/* GF(2^128) generator polynomial */

static void
aesni_crypt_xts_block(int rounds, const void *key_schedule, uint64_t *tweak,
    const uint64_t *from, uint64_t *to, uint64_t *block, int do_encrypt)
{
	int carry;

	block[0] = from[0] ^ tweak[0];
	block[1] = from[1] ^ tweak[1];

	if (do_encrypt)
		aesni_enc(rounds - 1, key_schedule, (uint8_t *)block, (uint8_t *)to, NULL);
	else
		aesni_dec(rounds - 1, key_schedule, (uint8_t *)block, (uint8_t *)to, NULL);

	to[0] ^= tweak[0];
	to[1] ^= tweak[1];

	/* Exponentiate tweak. */
	carry = ((tweak[0] & 0x8000000000000000ULL) > 0);
	tweak[0] <<= 1;
	if (tweak[1] & 0x8000000000000000ULL) {
		uint8_t *twk = (uint8_t *)tweak;

		twk[0] ^= AES_XTS_ALPHA;
	}
	tweak[1] <<= 1;
	if (carry)
		tweak[1] |= 1;
}

static void
aesni_crypt_xts(int rounds, const void *data_schedule,
    const void *tweak_schedule, size_t len, const uint8_t *from, uint8_t *to,
    const uint8_t iv[AES_BLOCK_LEN], int do_encrypt)
{
	uint64_t block[AES_XTS_BLOCKSIZE / 8];
	uint8_t tweak[AES_XTS_BLOCKSIZE];
	size_t i;

	/*
	 * Prepare tweak as E_k2(IV). IV is specified as LE representation
	 * of a 64-bit block number which we allow to be passed in directly.
	 */
#if BYTE_ORDER == LITTLE_ENDIAN
	bcopy(iv, tweak, AES_XTS_IVSIZE);
	/* Last 64 bits of IV are always zero. */
	bzero(tweak + AES_XTS_IVSIZE, AES_XTS_IVSIZE);
#else
#error Only LITTLE_ENDIAN architectures are supported.
#endif
	aesni_enc(rounds - 1, tweak_schedule, tweak, tweak, NULL);

	len /= AES_XTS_BLOCKSIZE;
	for (i = 0; i < len; i++) {
		aesni_crypt_xts_block(rounds, data_schedule, (uint64_t *)tweak,
		    (const uint64_t *)from, (uint64_t *)to, block, do_encrypt);
		from += AES_XTS_BLOCKSIZE;
		to += AES_XTS_BLOCKSIZE;
	}

	bzero(tweak, sizeof(tweak));
	bzero(block, sizeof(block));
}

static void
aesni_encrypt_xts(int rounds, const void *data_schedule,
    const void *tweak_schedule, size_t len, const uint8_t *from, uint8_t *to,
    const uint8_t iv[AES_BLOCK_LEN])
{

	aesni_crypt_xts(rounds, data_schedule, tweak_schedule, len, from, to,
	    iv, 1);
}

static void
aesni_decrypt_xts(int rounds, const void *data_schedule,
    const void *tweak_schedule, size_t len, const uint8_t *from, uint8_t *to,
    const uint8_t iv[AES_BLOCK_LEN])
{

	aesni_crypt_xts(rounds, data_schedule, tweak_schedule, len, from, to,
	    iv, 0);
}

static int
aesni_cipher_setup_common(struct aesni_session *ses, const uint8_t *key,
    int keylen)
{

	switch (ses->algo) {
	case CRYPTO_AES_CBC:
		switch (keylen) {
		case 128:
			ses->rounds = AES128_ROUNDS;
			break;
		case 192:
			ses->rounds = AES192_ROUNDS;
			break;
		case 256:
			ses->rounds = AES256_ROUNDS;
			break;
		default:
			return (EINVAL);
		}
		break;
	case CRYPTO_AES_XTS:
		switch (keylen) {
		case 256:
			ses->rounds = AES128_ROUNDS;
			break;
		case 512:
			ses->rounds = AES256_ROUNDS;
			break;
		default:
			return (EINVAL);
		}
		break;
	default:
		return (EINVAL);
	}

	aesni_set_enckey(key, ses->enc_schedule, ses->rounds);
	aesni_set_deckey(ses->enc_schedule, ses->dec_schedule, ses->rounds);
	if (ses->algo == CRYPTO_AES_CBC)
		arc4rand(ses->iv, sizeof(ses->iv), 0);
	else /* if (ses->algo == CRYPTO_AES_XTS) */ {
		aesni_set_enckey(key + keylen / 16, ses->xts_schedule,
		    ses->rounds);
	}

	return (0);
}

int
aesni_cipher_setup(struct aesni_session *ses, struct cryptoini *encini)
{
	struct thread *td;
	int error, saved_ctx;

	td = curthread;
	if (!is_fpu_kern_thread(0)) {
		error = fpu_kern_enter(td, ses->fpu_ctx, FPU_KERN_NORMAL);
		saved_ctx = 1;
	} else {
		error = 0;
		saved_ctx = 0;
	}
	if (error == 0) {
		error = aesni_cipher_setup_common(ses, encini->cri_key,
		    encini->cri_klen);
		if (saved_ctx)
			fpu_kern_leave(td, ses->fpu_ctx);
	}
	return (error);
}

int
aesni_cipher_process(struct aesni_session *ses, struct cryptodesc *enccrd,
    struct cryptop *crp)
{
	struct thread *td;
	uint8_t *buf;
	int error, allocated, saved_ctx;

	buf = aesni_cipher_alloc(enccrd, crp, &allocated);
	if (buf == NULL)
		return (ENOMEM);

	td = curthread;
	if (!is_fpu_kern_thread(0)) {
		error = fpu_kern_enter(td, ses->fpu_ctx, FPU_KERN_NORMAL);
		if (error != 0)
			goto out;
		saved_ctx = 1;
	} else {
		saved_ctx = 0;
		error = 0;
	}

	if ((enccrd->crd_flags & CRD_F_KEY_EXPLICIT) != 0) {
		error = aesni_cipher_setup_common(ses, enccrd->crd_key,
		    enccrd->crd_klen);
		if (error != 0)
			goto out;
	}

	if ((enccrd->crd_flags & CRD_F_ENCRYPT) != 0) {
		if ((enccrd->crd_flags & CRD_F_IV_EXPLICIT) != 0)
			bcopy(enccrd->crd_iv, ses->iv, AES_BLOCK_LEN);
		if ((enccrd->crd_flags & CRD_F_IV_PRESENT) == 0)
			crypto_copyback(crp->crp_flags, crp->crp_buf,
			    enccrd->crd_inject, AES_BLOCK_LEN, ses->iv);
		if (ses->algo == CRYPTO_AES_CBC) {
			aesni_encrypt_cbc(ses->rounds, ses->enc_schedule,
			    enccrd->crd_len, buf, buf, ses->iv);
		} else /* if (ses->algo == CRYPTO_AES_XTS) */ {
			aesni_encrypt_xts(ses->rounds, ses->enc_schedule,
			    ses->xts_schedule, enccrd->crd_len, buf, buf,
			    ses->iv);
		}
	} else {
		if ((enccrd->crd_flags & CRD_F_IV_EXPLICIT) != 0)
			bcopy(enccrd->crd_iv, ses->iv, AES_BLOCK_LEN);
		else
			crypto_copydata(crp->crp_flags, crp->crp_buf,
			    enccrd->crd_inject, AES_BLOCK_LEN, ses->iv);
		if (ses->algo == CRYPTO_AES_CBC) {
			aesni_decrypt_cbc(ses->rounds, ses->dec_schedule,
			    enccrd->crd_len, buf, ses->iv);
		} else /* if (ses->algo == CRYPTO_AES_XTS) */ {
			aesni_decrypt_xts(ses->rounds, ses->dec_schedule,
			    ses->xts_schedule, enccrd->crd_len, buf, buf,
			    ses->iv);
		}
	}
	if (saved_ctx)
		fpu_kern_leave(td, ses->fpu_ctx);
	if (allocated)
		crypto_copyback(crp->crp_flags, crp->crp_buf, enccrd->crd_skip,
		    enccrd->crd_len, buf);
	if ((enccrd->crd_flags & CRD_F_ENCRYPT) != 0)
		crypto_copydata(crp->crp_flags, crp->crp_buf,
		    enccrd->crd_skip + enccrd->crd_len - AES_BLOCK_LEN,
		    AES_BLOCK_LEN, ses->iv);
 out:
	if (allocated) {
		bzero(buf, enccrd->crd_len);
		free(buf, M_AESNI);
	}
	return (error);
}
