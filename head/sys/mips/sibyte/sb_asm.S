/*-
 * Copyright (c) 2009 Neelkanth Natu
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
 */

#include <machine/asm.h>

/*
 * We compile a 32-bit kernel to run on the SB-1 processor which is a 64-bit
 * processor. It has some registers that must be accessed using 64-bit load
 * and store instructions.
 *
 * So we have to resort to assembly because the compiler does not emit the
 * 'ld' and 'sd' instructions since it thinks that it is compiling for a
 * 32-bit mips processor.
 */

.set	mips64
.set	noat
.set	noreorder

/*
 * return (MIPS_PHYS_TO_KSEG1(0x10020008))
 * Parameters: none
 */
LEAF(sb_read_syscfg)
	lui     v0, 0xb002
	ori     v0, v0, 0x8
	ld      v1, 0(v0)	/* syscfg = MIPS_PHYS_TO_KSEG1(0x10020008) */
	move	v0, v1
	dsll32	v0, v0, 0
	dsrl32	v0, v0, 0	/* v0 = lower_uint32(mask) */
	jr	ra
	dsrl32	v1, v1, 0	/* v1 = upper_uint32(mask) */
END(sb_read_syscfg)

/*
 * MIPS_PHYS_TO_KSEG1(0x10020008) = (uint64_t)val
 * Parameters:
 * - lower_uint32(val): a0
 * - upper_uint32(val): a1
 */
LEAF(sb_write_syscfg)
	lui     v0, 0xb002
	ori     v0, v0, 0x8
	dsll32	a1, a1, 0	/* clear lower 32 bits of a1 */
	dsll32	a0, a0, 0
	dsrl32	a0, a0, 0	/* clear upper 32 bits of a0 */
	or	a1, a1, a0
	sd	a1, 0(v0)	/* MIPS_PHYS_TO_KSEG1(0x10020008) = val */
	jr	ra
	nop
	nop
END(sb_write_syscfg)

/*
 * MIPS_PHYS_TO_KSEG1(0x10020028) |= (1 << intsrc)
 *
 * Parameters:
 * - intsrc (a0)
 */
LEAF(sb_disable_intsrc)
	lui     v0, 0xb002
	ori     v0, v0, 0x28
	ld      v1, 0(v0)	/* mask = MIPS_PHYS_TO_KSEG1(0x10020028) */
	li      a1, 1
	dsllv   a1, a1, a0
	or      a1, a1, v1	/* mask |= (1 << intsrc) */
	jr	ra
	sd      a1, 0(v0)	/* MIPS_PHYS_TO_KSEG1(0x10020028) = mask */
END(sb_disable_intsrc)

/*
 * MIPS_PHYS_TO_KSEG1(0x10020028) &= ~(1 << intsrc)
 *
 * Parameters:
 * - intsrc (a0)
 */
LEAF(sb_enable_intsrc)
	lui     v0, 0xb002
	ori     v0, v0, 0x28
	ld      v1, 0(v0)	/* mask = MIPS_PHYS_TO_KSEG1(0x10020028) */
	li      a2, 1
	dsllv   a2, a2, a0
	nor     a2, zero, a2
	and     a2, a2, v1	/* mask &= ~(1 << intsrc) */
	sd      a2, 0(v0)	/* MIPS_PHYS_TO_KSEG1(0x10020028) = mask */
	jr      ra
	nop
END(sb_enable_intsrc)

/*
 * return ((uint64_t)MIPS_PHYS_TO_KSEG1(0x10020028))
 * Parameters: none
 */
LEAF(sb_read_intsrc_mask)
	lui     v0, 0xb002
	ori     v0, v0, 0x28
	ld      v1, 0(v0)	/* mask = MIPS_PHYS_TO_KSEG1(0x10020028) */
	move	v0, v1
	dsll32	v0, v0, 0
	dsrl32	v0, v0, 0	/* v0 = lower_uint32(mask) */
	jr	ra
	dsrl32	v1, v1, 0	/* v1 = upper_uint32(mask) */
END(sb_read_intsrc_mask)

/*
 * return ((uint64_t *)MIPS_PHYS_TO_KSEG1(0x10020200) + intsrc)
 * Parameters:
 * - intsrc (a0)
 */
LEAF(sb_read_intmap)
	sll	a0, a0, 3	/* compute the offset of the intmap register */
	lui     v0, 0xb002
	addu    a0, a0, v0
	ld      v0, 512(a0)	/* v0 = MIPS_PHYS_TO_KSEG1(0x10020200) + off */
	jr      ra
	nop
END(sb_read_intmap)

/*
 * (uint64_t *)MIPS_PHYS_TO_KSEG1(0x10020200) + intsrc = irq
 * Parameters:
 * - intsrc (a0)
 * - irq    (a1)
 */
LEAF(sb_write_intmap)
	sll     a0, a0, 0x3 /* compute the offset of the intmap register */
	lui     v0, 0xb002
	addu    a0, a0, v0
	sd      a1, 512(a0) /* MIPS_PHYS_TO_KSEG1(0x10020200) + off = irq */
	jr      ra
	nop
END(sb_write_intmap)
