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

#ifdef DEBUG
static void
ps_len(const char *string, const uint8_t *data, int length)
{
	int i;

	printf("%-12s[0x", string);
	for(i = 0; i < length; i++) {
		if (i % AES_BLOCK_LEN == 0 && i > 0)
			printf("+");
		printf("%02x", data[i]);
	}
	printf("]\n");
}
#endif

void
aesni_encrypt_cbc(int rounds, const void *key_schedule, size_t len,
    const uint8_t *from, uint8_t *to, const uint8_t iv[AES_BLOCK_LEN])
{
	const uint8_t *ivp;
	size_t i;

#ifdef DEBUG
	ps_len("AES CBC encrypt iv:", iv, AES_BLOCK_LEN);
	ps_len("from:", from, len);
#endif

	len /= AES_BLOCK_LEN;
	ivp = iv;
	for (i = 0; i < len; i++) {
		aesni_enc(rounds - 1, key_schedule, from, to, ivp);
		ivp = to;
		from += AES_BLOCK_LEN;
		to += AES_BLOCK_LEN;
	}
#ifdef DEBUG
	ps_len("to:", to - len * AES_BLOCK_LEN, len * AES_BLOCK_LEN);
#endif
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

static int
aesni_cipher_setup_common(struct aesni_session *ses, const uint8_t *key,
    int keylen)
{

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
 
	aesni_set_enckey(key, ses->enc_schedule, ses->rounds);
	aesni_set_deckey(ses->enc_schedule, ses->dec_schedule, ses->rounds);
	arc4rand(ses->iv, sizeof(ses->iv), 0);

	return (0);
}

int
aesni_cipher_setup(struct aesni_session *ses, struct cryptoini *encini)
{
	struct thread *td;
	int error;

 	td = curthread;
 	error = fpu_kern_enter(td, &ses->fpu_ctx, FPU_KERN_NORMAL);
 	if (error == 0) {
		error = aesni_cipher_setup_common(ses, encini->cri_key,
		    encini->cri_klen);
 		fpu_kern_leave(td, &ses->fpu_ctx);
 	}
 	return (error);
}

int
aesni_cipher_process(struct aesni_session *ses, struct cryptodesc *enccrd,
    struct cryptop *crp)
{
	struct thread *td;
	uint8_t *buf;
	int error, allocated;

	buf = aesni_cipher_alloc(enccrd, crp, &allocated);
	if (buf == NULL)
		return (ENOMEM);

	td = curthread;
	error = fpu_kern_enter(td, &ses->fpu_ctx, FPU_KERN_NORMAL);
	if (error != 0)
		goto out;
 
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

		aesni_encrypt_cbc(ses->rounds, ses->enc_schedule,
		    enccrd->crd_len, buf, buf, ses->iv);
	} else {
		if ((enccrd->crd_flags & CRD_F_IV_EXPLICIT) != 0)
			bcopy(enccrd->crd_iv, ses->iv, AES_BLOCK_LEN);
		else
			crypto_copydata(crp->crp_flags, crp->crp_buf,
			    enccrd->crd_inject, AES_BLOCK_LEN, ses->iv);
		aesni_decrypt_cbc(ses->rounds, ses->dec_schedule,
		    enccrd->crd_len, buf, ses->iv);
	}
	fpu_kern_leave(td, &ses->fpu_ctx);
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
