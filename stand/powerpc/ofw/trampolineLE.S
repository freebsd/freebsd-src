/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Brandon Bergren <bdragon@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <machine/asm.h>

/**
 * int openfirmware_trampoline(void *buf, int (*cb)(void *));
 */
ASENTRY_NOPROF(openfirmware_trampoline)
	mflr	%r0
	stw	%r0, 4(%r1)
	stwu	%r1, -16(%r1)
	stw	%r30, 8(%r1)
	/* Save current MSR for restoration post-call. */
	mfmsr	%r30
	mr	%r5, %r30
	/* Remove LE bit from MSR. */
	clrrwi	%r5, %r5, 1
	mtsrr0	%r4
	mtsrr1	%r5
	LOAD_LR_NIA
1:
	mflr	%r4
	addi	%r4, %r4, (2f - 1b)
	mtlr	%r4
	/* Switch to BE and transfer control to OF entry */
	rfid
2:
	/* Control is returned here, but in BE. */
	.long	0x05009f42	/* LOAD_LR_NIA				*/
				/* 0:					*/
	.long	0xa603db7f	/* mtsrr1 	%r30			*/
	.long	0xa602c87f	/* mflr		%r30			*/
	.long	0x1400de3b	/* addi		%r30, %r30, (1f - 0b)	*/
	.long	0xa603da7f	/* mtsrr0	%r30			*/
	.long	0x2400004c	/* rfid					*/
				/* 1:					*/
1:
	/* Back to normal. Tidy up for return. */
	lwz	%r30, 8(%r1)
	lwz	%r0, 20(%r1)
	addi	%r1, %r1, 16
	mtlr	%r0
	blr
ASEND(openfirmware_trampoline)
