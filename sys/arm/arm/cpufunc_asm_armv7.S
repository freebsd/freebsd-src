/*-
 * Copyright (c) 2010 Per Odlund <per.odlund@armagedon.se>
 * Copyright (C) 2011 MARVELL INTERNATIONAL LTD.
 * All rights reserved.
 *
 * Developed by Semihalf.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of MARVELL nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

#include <machine/asm.h>
__FBSDID("$FreeBSD$");

#include <machine/sysreg.h>

	.cpu cortex-a8

#ifdef ELF_TRAMPOLINE
.Lcoherency_level:
	.word	_C_LABEL(arm_cache_loc)
.Lcache_type:
	.word	_C_LABEL(arm_cache_type)
.Lway_mask:
	.word	0x3ff
.Lmax_index:
	.word	0x7fff

/* Based on algorithm from ARM Architecture Reference Manual */
ENTRY(armv7_dcache_wbinv_all)
	stmdb	sp!, {r4, r5, r6, r7, r8, r9}

	/* Get cache level */
	ldr	r0, .Lcoherency_level
	ldr	r3, [r0]
	cmp	r3, #0
	beq	Finished
	/* For each cache level */
	mov	r8, #0
Loop1:
	/* Get cache type for given level */
	mov	r2, r8, lsl #2
	add	r2, r2, r2
	ldr	r0, .Lcache_type
	ldr	r1, [r0, r2]

	/* Get line size */
	and	r2, r1, #7
	add	r2, r2, #4

	/* Get number of ways */
	ldr	r4, .Lway_mask
	ands	r4, r4, r1, lsr #3
	clz	r5, r4

	/* Get max index */
	ldr	r7, .Lmax_index
	ands	r7, r7, r1, lsr #13
Loop2:
	mov	r9, r4
Loop3:
	mov	r6, r8, lsl #1
	orr	r6, r6, r9, lsl r5
	orr	r6, r6, r7, lsl r2

	/* Clean and invalidate data cache by way/index */
	mcr	CP15_DCCISW(r6)
	subs	r9, r9, #1
	bge	Loop3
	subs	r7, r7, #1
	bge	Loop2
Skip:
	add	r8, r8, #1
	cmp	r3, r8
	bne Loop1
Finished:
	dsb
	ldmia	sp!, {r4, r5, r6, r7, r8, r9}
	RET
END(armv7_dcache_wbinv_all)

ENTRY(armv7_idcache_wbinv_all)
	stmdb	sp!, {lr}
	bl armv7_dcache_wbinv_all
#ifdef SMP
	mcr	CP15_ICIALLUIS
#else
	mcr	CP15_ICIALLU
#endif
	dsb
	isb
	ldmia	sp!, {lr}
	RET
END(armv7_idcache_wbinv_all)
#endif

ENTRY(armv7_cpu_sleep)
	dsb				/* data synchronization barrier */
	wfi  				/* wait for interrupt */
	RET
END(armv7_cpu_sleep)

ENTRY(armv7_drain_writebuf)
	dsb
	RET
END(armv7_drain_writebuf)
