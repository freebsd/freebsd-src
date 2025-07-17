/*-
 * Copyright (C) 2015 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>

#include "opt_platform.h"

GLOBAL(opal_entrypoint)
	.llong	0
GLOBAL(opal_data)
	.llong	0
GLOBAL(opal_msr)
	.llong	0

TOC_ENTRY(opal_entrypoint)
TOC_ENTRY(opal_data)
TOC_ENTRY(opal_msr)

ASENTRY(opal_call)
	/* Args:
	 * r3: opal token
	 * r4-r10 opal arguments
	 */

	/* Save call stuff on stack */
	mflr	%r0
	std	%r0,16(%r1)
	std	%r2,-16(%r1)
	mfcr	%r0
	std	%r0,8(%r1)

	/* Load OPAL entry information */
	mr	%r0,%r3
	addis	%r3,%r2,TOC_REF(opal_entrypoint)@ha
	ld	%r3,TOC_REF(opal_entrypoint)@l(%r3)
	ld	%r3,0(%r3)
	mtctr	%r3

	/* Save MSR in non-volatile scratch register and turn off translation */
	std	%r31,-8(%r1)
	mfmsr	%r31

	/* Load last bits from the TOC */
	addis	%r3,%r2,TOC_REF(opal_msr)@ha
	ld	%r3,TOC_REF(opal_msr)@l(%r3)
	ld	%r3,0(%r3)
	addis	%r2,%r2,TOC_REF(opal_data)@ha
	ld	%r2,TOC_REF(opal_data)@l(%r2)
	ld	%r2,0(%r2)


#if defined(__LITTLE_ENDIAN__) && defined(QEMU)
	/* QEMU hack: qemu does not emulate mtmsrd correctly! */
	ori     %r3,%r3,1       /* Leave PSR_LE set */
#endif
	mtmsrd	%r3
	isync
#if defined(__LITTLE_ENDIAN__) && defined(QEMU)
	/* Clean up from qemu hack */
	xori	%r3,%r3,1
#endif

#ifdef __LITTLE_ENDIAN__
	mtsrr1	%r3
#endif

	/* Shift registers over */
	mr	%r3,%r4
	mr	%r4,%r5
	mr	%r5,%r6
	mr	%r6,%r7
	mr	%r7,%r8
	mr	%r8,%r9
	mr	%r9,%r10

#ifdef __LITTLE_ENDIAN__
	/* We need to rfid to switch endian. */
	mfctr	%r11
	mtsrr0	%r11
	LOAD_LR_NIA
1:
	mflr	%r11
	addi	%r11, %r11, (2f-1b)
	mtlr	%r11
	/* Call OPAL */
	rfid
2:
	RETURN_TO_NATIVE_ENDIAN
#else
	/* Call OPAL */
	bctrl
#endif

	/* Restore MSR */
	mtmsrd	%r31
	isync
	ld	%r31,-8(%r1)
	
	/* Restore call stuff from stack */
	ld	%r0,16(%r1)
	mtlr	%r0
	ld	%r2,-16(%r1)
	ld	%r0,8(%r1)
	mtcr	%r0

	/* And return */
	blr
ASEND(opal_call)
