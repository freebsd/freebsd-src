/*
 * Copyright (c) 2017 Alexandru Elisei <alexandru.elisei@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/elf_common.h>

#include <machine/asm.h>

.macro vempty
	.align 7
	1: b 	1b
.endm

/*
 * Install a new exception vector table with the base address supplied by the
 * parameter in register x0.
 */
.macro vector_stub_el1h_sync
	.align 7
	msr	vbar_el2, x0
	ERET
.endm

	.align 11
	.globl hyp_stub_vectors
hyp_stub_vectors:
	vempty			/* Synchronous EL2t */
	vempty			/* IRQ EL2t */
	vempty			/* FIQ EL2t */
	vempty			/* SError EL2t */

	vempty			/* Synchronous EL2h */
	vempty			/* IRQ EL2h */
	vempty			/* FIQ EL2h */
	vempty			/* SError EL2h */

	vector_stub_el1h_sync	/* Synchronous 64-bit EL1 */
	vempty			/* IRQ 64-bit EL1 */
	vempty			/* FIQ 64-bit EL1 */
	vempty			/* SError 64-bit EL1 */

	vempty			/* Synchronous 32-bit EL1 */
	vempty			/* IRQ 32-bit EL1 */
	vempty			/* FIQ 32-bit EL1 */
	vempty			/* SError 32-bit EL1 */

GNU_PROPERTY_AARCH64_FEATURE_1_NOTE(GNU_PROPERTY_AARCH64_FEATURE_1_VAL)
