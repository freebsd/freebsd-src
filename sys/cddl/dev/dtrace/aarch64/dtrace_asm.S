/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#define _ASM
#define _LOCORE

#include <sys/cpuvar_defs.h>
#include <sys/dtrace.h>
#include <sys/elf_common.h>

#include <machine/armreg.h>
#include <machine/asm.h>

#include "assym.inc"

/*
void dtrace_membar_producer(void)
*/
ENTRY(dtrace_membar_producer)
	RET
END(dtrace_membar_producer)

/*
void dtrace_membar_consumer(void)
*/
ENTRY(dtrace_membar_consumer)
	RET
END(dtrace_membar_consumer)

/*
dtrace_icookie_t dtrace_interrupt_disable(void)
*/
ENTRY(dtrace_interrupt_disable)
	mrs	x0, daif
	msr	daifset, #2
	RET
END(dtrace_interrupt_disable)

/*
void dtrace_interrupt_enable(dtrace_icookie_t cookie)
*/
ENTRY(dtrace_interrupt_enable)
	msr	daif, x0
	RET
END(dtrace_interrupt_enable)
/*
uint8_t
dtrace_fuword8_nocheck(void *addr)
*/
ENTRY(dtrace_fuword8_nocheck)
	ldtrb	w0, [x0]
	RET
END(dtrace_fuword8_nocheck)

/*
uint16_t
dtrace_fuword16_nocheck(void *addr)
*/
ENTRY(dtrace_fuword16_nocheck)
	ldtrh	w0, [x0]
	RET
END(dtrace_fuword16_nocheck)

/*
uint32_t
dtrace_fuword32_nocheck(void *addr)
*/
ENTRY(dtrace_fuword32_nocheck)
	ldtr	w0, [x0]
	RET
END(dtrace_fuword32_nocheck)

/*
uint64_t
dtrace_fuword64_nocheck(void *addr)
*/
ENTRY(dtrace_fuword64_nocheck)
	ldtr	x0, [x0]
	RET
END(dtrace_fuword64_nocheck)

/*
void
dtrace_copyin_pan(uintptr_t uaddr, uintptr_t kaddr, size_t size)
*/
ENTRY(dtrace_copyin_pan)
	cbz	x2, 2f		/* If len == 0 then skip loop */
1:
	mvn	x4, xzr		/* Set all bits */
	ldtrb	w4, [x0]	/* Load from uaddr or raise data abort */
	add	x0, x0, #1
	lsr	x3, x4, #32	/* Extract zero-extended high bits */
	cbnz	x3, 2f		/* If high bits != 0 then break */
	strb	w4, [x1], #1	/* Store in kaddr */
	sub	x2, x2, #1	/* len-- */
	cbnz	x2, 1b
2:
	RET
END(dtrace_copyin_pan)

/*
void
dtrace_copyinstr_pan(uintptr_t uaddr, uintptr_t kaddr, size_t size)
*/
ENTRY(dtrace_copyinstr_pan)
	cbz     x2, 2f          /* If len == 0 then skip loop */
1:
	mvn	x4, xzr		/* Set all bits */
	ldtrb	w4, [x0]	/* Load from uaddr or raise data abort */
	add	x0, x0, #1
	lsr	x3, x4, #32	/* Extract zero-extended high bits */
	cbnz	x3, 2f		/* If high bits != 0 then break */
	strb    w4, [x1], #1    /* Store in kaddr */
	cbz     w4, 2f          /* If == 0 then break */
	sub     x2, x2, #1      /* len-- */
	cbnz    x2, 1b
2:
	RET
END(dtrace_copyinstr_pan)

/*
void
dtrace_copyout_pan(uintptr_t kaddr, uintptr_t uaddr, size_t size,
    volatile uint16_t *flags)
*/
ENTRY(dtrace_copyout_pan)
	cbz	x2, 4f		/* If len == 0 then skip loop */
	ldrb	w4, [x0], #1	/* Load from kaddr */
	sttrb	w4, [x1]	/* Store in uaddr or raise data abort */
	b 2f			/* Check fault */
1:
	ldrb	w4, [x0], #1	/* Load from kaddr */
	sttrb	w4, [x1]	/* Store in uaddr or raise data abort */
	tst	x1, #0xfff	/* uaddr & 2^12-1 */
	bne	3f		/* If not 4k aligned then skip check */
2:
	ldrh	w5, [x3]	/* Load cpu flags */
	tst	w5, #CPU_DTRACE_BADADDR
	bne	4f		/* If flags&CPU_DTRACE_BADADDR != 0 then break */
3:
	add	x1, x1, #1	/* uaddr++ */
	sub	x2, x2, #1	/* len-- */
	cbnz	x2, 1b
4:
	RET
END(dtrace_copyout_pan)


/*
void
dtrace_copyoutstr_pan(uintptr_t kaddr, uintptr_t uaddr, size_t size,
    volatile uint16_t *flags)
*/
ENTRY(dtrace_copyoutstr_pan)
	cbz	x2, 4f		/* If len == 0 then skip loop */
	ldrb	w4, [x0], #1	/* Load from kaddr */
	sttrb	w4, [x1]	/* Store in uaddr or raise data abort */
	b 2f			/* Check fault */
1:
	ldrb	w4, [x0], #1	/* Load from kaddr */
	sttrb	w4, [x1]	/* Store in uaddr or raise data abort */
	tst	x1, #0xfff	/* uaddr & 2^12-1 */
	bne	3f		/* If not 4k aligned then skip check */
2:
	ldrh	w5, [x3]	/* Load cpu flags */
	tst	w5, #CPU_DTRACE_BADADDR
	bne	4f		/* If flags&CPU_DTRACE_BADADDR != 0 then break */
3:
	cbz     w4, 4f          /* If == 0 then break */
	add	x1, x1, #1	/* uaddr++ */
	sub	x2, x2, #1	/* len-- */
	cbnz	x2, 1b
4:
	RET
END(dtrace_copyoutstr_pan)

/*
uintptr_t
dtrace_caller(int aframes)
*/
ENTRY(dtrace_caller)
	mov	x0, #-1
	RET
END(dtrace_caller)

/*
uint32_t
dtrace_cas32(uint32_t *target, uint32_t cmp, uint32_t new)
*/
ENTRY(dtrace_cas32)
1:	ldxr	w3, [x0]	/* Load target */
	cmp	w3, w1		/* Check if *target == cmp */
	bne	2f		/* No, return */
	stxr	w12, w2, [x0]	/* Store new to target */
	cbnz	w12, 1b		/* Try again if store not succeed */
2:	mov	w0, w3		/* Return the value loaded from target */
	RET
END(dtrace_cas32)

/*
void *
dtrace_casptr(volatile void *target, volatile void *cmp, volatile void *new)
*/
ENTRY(dtrace_casptr)
1:	ldxr	x3, [x0]	/* Load target */
	cmp	x3, x1		/* Check if *target == cmp */
	bne	2f		/* No, return */
	stxr	w12, x2, [x0]	/* Store new to target */
	cbnz	w12, 1b		/* Try again if store not succeed */
2:	mov	x0, x3		/* Return the value loaded from target */
	RET
END(dtrace_casptr)

GNU_PROPERTY_AARCH64_FEATURE_1_NOTE(GNU_PROPERTY_AARCH64_FEATURE_1_VAL)
