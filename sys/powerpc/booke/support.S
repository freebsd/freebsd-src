/*-
 * Copyright (C) 2006 Semihalf, Marian Balakowicz <m8@semihalf.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "assym.s"

#include <machine/param.h>
#include <machine/asm.h>
#include <machine/spr.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/trap.h>
#include <machine/vmparam.h>
#include <machine/tlb.h>

	.text
/*
 * void remap_ccsrbar(vm_offset_t old_ccsrbar_va, vm_offset_t new_ccsrbar_va,
 *     vm_offset_t new_ccsrbar_pa)
 *
 * r3 - old_ccsrbar_va
 * r4 - new_ccsrbar_va
 * r5 - new_ccsrbar_pa
 */
ENTRY(remap_ccsrbar)
	/*
	 * CCSRBAR updating sequence according
	 * to section 4.3.1.1.1 of MPC8555E RM.
	 */

	/* Read current value of CCSRBAR */
	lwz	%r6, 0(%r3)
	isync

	/* Write new value */
	rlwinm	%r6, %r5, 20, 12, 23
	stw	%r6, 0(%r3)

	/*
	 * Read from address that is outside of CCSRBAR space.
	 * We have RAM locations available at KERNBASE.
	 */
	lis	%r7, KERNBASE@ha
	addi	%r7, %r7, KERNBASE@l
	lwz	%r6, 0(%r7)
	isync

	/* Read value of CCSRBAR from new location */
	lwz	%r6, 0(%r4)
	isync
	blr

/*
 * void switch_to_as0(void)
 */
ENTRY(switch_to_as0)
	mflr	%r5			/* Save LR */

	mfmsr	%r3
	lis	%r6, (PSL_IS | PSL_DS)@ha
	ori	%r6, %r6, (PSL_IS | PSL_DS)@l
	not	%r6, %r6
	and	%r3, %r3, %r6		/* Clear IS/DS bits */

	bl	1f
1:	mflr	%r4			/* Use current address */
	addi	%r4, %r4, 20		/* Increment to instruction after rfi  */
	mtspr   SPR_SRR0, %r4
	mtspr   SPR_SRR1, %r3
	rfi

	mtlr	%r5			/* Restore LR */
	blr

/*
 * void load_pid0(tlbtid_t)
 */
ENTRY(load_pid0)
	mtspr	SPR_PID0, %r3
	isync
	blr
