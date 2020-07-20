/*-
 * Copyright (c) 2005-2006 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * Copyright (c) 2004 Mark R V Murray
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

/*	$OpenBSD: via.c,v 1.3 2004/06/15 23:36:55 deraadt Exp $	*/
/*-
 * Copyright (c) 2003 Jason Wright
 * Copyright (c) 2003, 2004 Theo de Raadt
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/libkern.h>
#include <sys/pcpu.h>
#include <sys/uio.h>

#include <opencrypto/cryptodev.h>
#include <crypto/rijndael/rijndael.h>

#include <crypto/via/padlock.h>

#define	PADLOCK_ROUND_COUNT_AES128	10
#define	PADLOCK_ROUND_COUNT_AES192	12
#define	PADLOCK_ROUND_COUNT_AES256	14

#define	PADLOCK_ALGORITHM_TYPE_AES	0

#define	PADLOCK_KEY_GENERATION_HW	0
#define	PADLOCK_KEY_GENERATION_SW	1

#define	PADLOCK_DIRECTION_ENCRYPT	0
#define	PADLOCK_DIRECTION_DECRYPT	1

#define	PADLOCK_KEY_SIZE_128	0
#define	PADLOCK_KEY_SIZE_192	1
#define	PADLOCK_KEY_SIZE_256	2

MALLOC_DECLARE(M_PADLOCK);

static __inline void
padlock_cbc(void *in, void *out, size_t count, void *key, union padlock_cw *cw,
    void *iv)
{
#ifdef __GNUCLIKE_ASM
	/* The .byte line is really VIA C3 "xcrypt-cbc" instruction */
	__asm __volatile(
		"pushf				\n\t"
		"popf				\n\t"
		"rep				\n\t"
		".byte	0x0f, 0xa7, 0xd0"
			: "+a" (iv), "+c" (count), "+D" (out), "+S" (in)
			: "b" (key), "d" (cw)
			: "cc", "memory"
		);
#endif
}

static void
padlock_cipher_key_setup(struct padlock_session *ses, const void *key, int klen)
{
	union padlock_cw *cw;
	int i;

	cw = &ses->ses_cw;
	if (cw->cw_key_generation == PADLOCK_KEY_GENERATION_SW) {
		/* Build expanded keys for both directions */
		rijndaelKeySetupEnc(ses->ses_ekey, key, klen * 8);
		rijndaelKeySetupDec(ses->ses_dkey, key, klen * 8);
		for (i = 0; i < 4 * (RIJNDAEL_MAXNR + 1); i++) {
			ses->ses_ekey[i] = ntohl(ses->ses_ekey[i]);
			ses->ses_dkey[i] = ntohl(ses->ses_dkey[i]);
		}
	} else {
		bcopy(key, ses->ses_ekey, klen);
		bcopy(key, ses->ses_dkey, klen);
	}
}

int
padlock_cipher_setup(struct padlock_session *ses,
    const struct crypto_session_params *csp)
{
	union padlock_cw *cw;

	if (csp->csp_cipher_klen != 16 && csp->csp_cipher_klen != 24 &&
	    csp->csp_cipher_klen != 32) {
		return (EINVAL);
	}

	cw = &ses->ses_cw;
	bzero(cw, sizeof(*cw));
	cw->cw_algorithm_type = PADLOCK_ALGORITHM_TYPE_AES;
	cw->cw_key_generation = PADLOCK_KEY_GENERATION_SW;
	cw->cw_intermediate = 0;
	switch (csp->csp_cipher_klen * 8) {
	case 128:
		cw->cw_round_count = PADLOCK_ROUND_COUNT_AES128;
		cw->cw_key_size = PADLOCK_KEY_SIZE_128;
#ifdef HW_KEY_GENERATION
		/* This doesn't buy us much, that's why it is commented out. */
		cw->cw_key_generation = PADLOCK_KEY_GENERATION_HW;
#endif
		break;
	case 192:
		cw->cw_round_count = PADLOCK_ROUND_COUNT_AES192;
		cw->cw_key_size = PADLOCK_KEY_SIZE_192;
		break;
	case 256:
		cw->cw_round_count = PADLOCK_ROUND_COUNT_AES256;
		cw->cw_key_size = PADLOCK_KEY_SIZE_256;
		break;
	}
	if (csp->csp_cipher_key != NULL) {
		padlock_cipher_key_setup(ses, csp->csp_cipher_key,
		    csp->csp_cipher_klen);
	}
	return (0);
}

/*
 * Function checks if the given buffer is already 16 bytes aligned.
 * If it is there is no need to allocate new buffer.
 * If it isn't, new buffer is allocated.
 */
static u_char *
padlock_cipher_alloc(struct cryptop *crp, int *allocated)
{
	u_char *addr;

	addr = crypto_contiguous_subsegment(crp, crp->crp_payload_start,
	    crp->crp_payload_length);
	if (((uintptr_t)addr & 0xf) == 0) { /* 16 bytes aligned? */
		*allocated = 0;
		return (addr);
	}

	*allocated = 1;
	addr = malloc(crp->crp_payload_length + 16, M_PADLOCK, M_NOWAIT);
	return (addr);
}

int
padlock_cipher_process(struct padlock_session *ses, struct cryptop *crp,
    const struct crypto_session_params *csp)
{
	union padlock_cw *cw;
	struct thread *td;
	u_char *buf, *abuf;
	uint32_t *key;
	uint8_t iv[AES_BLOCK_LEN] __aligned(16);
	int allocated;

	buf = padlock_cipher_alloc(crp, &allocated);
	if (buf == NULL)
		return (ENOMEM);
	/* Buffer has to be 16 bytes aligned. */
	abuf = PADLOCK_ALIGN(buf);

	if (crp->crp_cipher_key != NULL) {
		padlock_cipher_key_setup(ses, crp->crp_cipher_key,
		    csp->csp_cipher_klen);
	}

	cw = &ses->ses_cw;
	cw->cw_filler0 = 0;
	cw->cw_filler1 = 0;
	cw->cw_filler2 = 0;
	cw->cw_filler3 = 0;

	crypto_read_iv(crp, iv);

	if (CRYPTO_OP_IS_ENCRYPT(crp->crp_op)) {
		cw->cw_direction = PADLOCK_DIRECTION_ENCRYPT;
		key = ses->ses_ekey;
	} else {
		cw->cw_direction = PADLOCK_DIRECTION_DECRYPT;
		key = ses->ses_dkey;
	}

	if (allocated) {
		crypto_copydata(crp, crp->crp_payload_start,
		    crp->crp_payload_length, abuf);
	}

	td = curthread;
	fpu_kern_enter(td, ses->ses_fpu_ctx, FPU_KERN_NORMAL | FPU_KERN_KTHR);
	padlock_cbc(abuf, abuf, crp->crp_payload_length / AES_BLOCK_LEN, key,
	    cw, iv);
	fpu_kern_leave(td, ses->ses_fpu_ctx);

	if (allocated) {
		crypto_copyback(crp, crp->crp_payload_start,
		    crp->crp_payload_length, abuf);

		zfree(buf, M_PADLOCK);
	}
	return (0);
}
