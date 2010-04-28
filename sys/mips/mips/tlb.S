/*	$OpenBSD: locore.S,v 1.18 1998/09/15 10:58:53 pefo Exp $	*/
/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Digital Equipment Corporation and Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (C) 1989 Digital Equipment Corporation.
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies.
 * Digital Equipment Corporation makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/loMem.s,
 *	v 1.1 89/07/11 17:55:04 nelson Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/mach/ds3100.md/RCS/machAsm.s,
 *	v 9.2 90/01/29 18:00:39 shirriff Exp  SPRITE (DECWRL)
 * from: Header: /sprite/src/kernel/vm/ds3100.md/vmPmaxAsm.s,
 *	v 1.1 89/07/10 14:27:41 nelson Exp  SPRITE (DECWRL)
 *
 *	from: @(#)locore.s	8.5 (Berkeley) 1/4/94
 *	JNPR: tlb.S,v 1.1.4.2 2007/09/10 09:02:05 girish
 * $FreeBSD$
 */

/*
 *	Contains code that is the first executed at boot time plus
 *	assembly language support routines.
 */

#include "opt_cputype.h"

#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/cpuregs.h>
#include <machine/regnum.h>
#include <machine/pte.h>

#include "assym.s"

#if defined(ISA_MIPS32)
#undef WITH_64BIT_CP0
#elif defined(ISA_MIPS64)
#define WITH_64BIT_CP0
#elif defined(ISA_MIPS3)
#define WITH_64BIT_CP0
#else
#error "Please write the code for this ISA"
#endif

#ifdef WITH_64BIT_CP0
#define _SLL	dsll
#define	_SRL	dsrl
#define	_MFC0	dmfc0
#define	_MTC0	dmtc0
#define WIRED_SHIFT 34
#else
#define _SLL	sll
#define	_SRL	srl
#define	_MFC0	mfc0
#define	_MTC0	mtc0
#define WIRED_SHIFT 2
#endif
	.set	noreorder			# Noreorder is default style!
#if defined(ISA_MIPS32)
	.set	mips32
#elif defined(ISA_MIPS64)
	.set	mips64
#elif defined(ISA_MIPS3)
	.set	mips3
#endif

#define	ITLBNOPFIX	nop;nop;nop;nop;nop;nop;nop;nop;nop;nop;

/*
 * FREEBSD_DEVELOPERS_FIXME
 * Some MIPS CPU may need delays using nops between executing CP0 Instructions
 */
#define	MIPS_CPU_NOP_DELAY	nop;nop;nop;nop;

/*--------------------------------------------------------------------------
 *
 * Mips_TLBWriteIndexed(unsigned index, tlb *tlb);
 *
 *	Write the given entry into the TLB at the given index.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	TLB entry set.
 *
 *--------------------------------------------------------------------------
 */
LEAF(Mips_TLBWriteIndexed)
	mfc0	v1, COP_0_STATUS_REG		# Save the status register.
	mtc0	zero, COP_0_STATUS_REG		# Disable interrupts
	ITLBNOPFIX
	lw	a2, 8(a1)
	lw	a3, 12(a1)
	_MFC0	t0, COP_0_TLB_HI		# Save the current PID.

	_MTC0	a2, COP_0_TLB_LO0		# Set up entry low0.
	_MTC0	a3, COP_0_TLB_LO1		# Set up entry low1.
	lw	a2, 0(a1)
	lw	a3, 4(a1)
	mtc0	a0, COP_0_TLB_INDEX		# Set the index.
	_MTC0	a2, COP_0_TLB_PG_MASK		# Set up entry mask.
	_MTC0	a3, COP_0_TLB_HI		# Set up entry high.
	MIPS_CPU_NOP_DELAY
	tlbwi					# Write the TLB
	MIPS_CPU_NOP_DELAY

	_MTC0	t0, COP_0_TLB_HI		# Restore the PID.
	nop
	_MTC0	zero, COP_0_TLB_PG_MASK		# Default mask value.
	mtc0	v1, COP_0_STATUS_REG		# Restore the status register
	ITLBNOPFIX
	j	ra
	nop
END(Mips_TLBWriteIndexed)

/*--------------------------------------------------------------------------
 *
 * Mips_SetPID(int pid);
 *
 *	Write the given pid into the TLB pid reg.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	PID set in the entry hi register.
 *
 *--------------------------------------------------------------------------
 */
LEAF(Mips_SetPID)
	_MTC0	a0, COP_0_TLB_HI		# Write the hi reg value
	nop					# required for QED5230
	nop					# required for QED5230
	j	ra
	nop
END(Mips_SetPID)

/*--------------------------------------------------------------------------
 *
 * Mips_SetWIRED(int wired);
 *
 *	Write the given value into the TLB wired reg.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	WIRED set in the wired register.
 *
 *--------------------------------------------------------------------------
 */
LEAF(Mips_SetWIRED)
	mtc0	a0, COP_0_TLB_WIRED
	j	ra
	nop
END(Mips_SetWIRED)

/*--------------------------------------------------------------------------
 *
 * Mips_GetWIRED(void);
 *
 *	Get the value from the TLB wired reg.
 *
 * Results:
 *	Value of wired reg.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */
LEAF(Mips_GetWIRED)
	mfc0	v0, COP_0_TLB_WIRED
	j	ra
	nop
END(Mips_GetWIRED)

/*--------------------------------------------------------------------------
 *
 * Mips_TLBFlush(tlbsize);
 *
 *	Flush the "random" entries from the TLB.
 *	Uses "wired" register to determine what register to start with.
 *	Arg "tlbsize" is the number of entries to flush.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The TLB is flushed.
 *
 *--------------------------------------------------------------------------
 */
LEAF(Mips_TLBFlush)
	mfc0	v1, COP_0_STATUS_REG		# Save the status register.
	mtc0	zero, COP_0_STATUS_REG		# Disable interrupts
	ITLBNOPFIX
	mfc0	t1, COP_0_TLB_WIRED
	_MFC0	t0, COP_0_TLB_HI		# Save the PID
	_MTC0	zero, COP_0_TLB_LO0		# Zero out low entry0.
	_MTC0	zero, COP_0_TLB_LO1		# Zero out low entry1.
	mtc0	zero, COP_0_TLB_PG_MASK 	# Zero out mask entry.
	#
	# Load invalid entry, each TLB entry should have it's own bogus 
	# address calculated by following expression:
	# MIPS_KSEG0_START + 2 * i * PAGE_SIZE;
	# One bogus value for every TLB entry might cause MCHECK exception
	#
	sll	t3, t1, PAGE_SHIFT + 1
	li	v0, MIPS_KSEG0_START		# invalid address
	addu	v0, t3
/*
 * Align the starting value (t1) and the upper bound (a0).
 */
1:
	mtc0	t1, COP_0_TLB_INDEX		# Set the index register.
	ITLBNOPFIX
	_MTC0	v0, COP_0_TLB_HI		# Mark entry high as invalid
	addu	t1, t1, 1			# Increment index.
	addu	v0, v0, 8 * 1024
	MIPS_CPU_NOP_DELAY
	tlbwi					# Write the TLB entry.
	MIPS_CPU_NOP_DELAY
	bne	t1, a0, 1b
	nop
	_MTC0	t0, COP_0_TLB_HI		# Restore the PID
	mtc0	v1, COP_0_STATUS_REG		# Restore the status register
	ITLBNOPFIX
	j	ra
	nop
END(Mips_TLBFlush)


/*--------------------------------------------------------------------------
 *
 * Mips_TLBFlushAddr(unsigned TLBhi);
 *
 *	Flush any TLB entries for the given address and TLB PID.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The process's page is flushed from the TLB.
 *
 *--------------------------------------------------------------------------
 */
LEAF(Mips_TLBFlushAddr)
	mfc0	v1, COP_0_STATUS_REG		# Save the status register.
	mtc0	zero, COP_0_STATUS_REG		# Disable interrupts
	ITLBNOPFIX
	li	v0, (PTE_HVPN | PTE_ASID)
	and	a0, a0, v0			# Make shure valid hi value.
	_MFC0	t0, COP_0_TLB_HI		# Get current PID
	mfc0	t3, COP_0_TLB_PG_MASK		# Save current pgMask
	_MTC0	a0, COP_0_TLB_HI		# look for addr & PID
	MIPS_CPU_NOP_DELAY
	tlbp					# Probe for the entry.
	MIPS_CPU_NOP_DELAY
	mfc0	v0, COP_0_TLB_INDEX		# See what we got
	li	t1, MIPS_KSEG0_START
	bltz	v0, 1f				# index < 0 => !found
	nop
	# Load invalid entry, each TLB entry should have it's own bogus 
	# address calculated by following expression:
	# MIPS_KSEG0_START + 2 * i * PAGE_SIZE;
	# One bogus value for every TLB entry might cause MCHECK exception
	sll	v0, PAGE_SHIFT + 1
	addu	t1, v0
	_MTC0	t1, COP_0_TLB_HI		# Mark entry high as invalid

	_MTC0	zero, COP_0_TLB_LO0		# Zero out low entry.
	_MTC0	zero, COP_0_TLB_LO1		# Zero out low entry.
	MIPS_CPU_NOP_DELAY
	tlbwi
	MIPS_CPU_NOP_DELAY
1:
	_MTC0	t0, COP_0_TLB_HI		# restore PID
	mtc0	t3, COP_0_TLB_PG_MASK		# Restore pgMask
	mtc0	v1, COP_0_STATUS_REG		# Restore the status register
	ITLBNOPFIX
	j	ra
	nop
END(Mips_TLBFlushAddr)

/*--------------------------------------------------------------------------
 *
 * Mips_TLBUpdate(unsigned virpageadr, lowregx);
 *
 *	Update the TLB if highreg is found; otherwise, enter the data.
 *
 * Results:
 *	< 0 if loaded >= 0 if updated.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */
LEAF(Mips_TLBUpdate)
	mfc0	v1, COP_0_STATUS_REG		# Save the status register.
	mtc0	zero, COP_0_STATUS_REG		# Disable interrupts
	ITLBNOPFIX
	and	t1, a0, 0x1000			# t1 = Even/Odd flag
	li	v0, (PTE_HVPN | PTE_ASID)
	and	a0, a0, v0
	_MFC0	t0, COP_0_TLB_HI		# Save current PID
	_MTC0	a0, COP_0_TLB_HI		# Init high reg
	and	a2, a1, PTE_G			# Copy global bit
	MIPS_CPU_NOP_DELAY
	tlbp					# Probe for the entry.
	_SLL	a1, a1, WIRED_SHIFT
	_SRL	a1, a1, WIRED_SHIFT
	nop
	mfc0	v0, COP_0_TLB_INDEX		# See what we got
	bne	t1, zero, 2f			# Decide even odd
# EVEN
	nop
	bltz	v0, 1f				# index < 0 => !found
	MIPS_CPU_NOP_DELAY

	tlbr					# update, read entry first
	MIPS_CPU_NOP_DELAY
	_MTC0	a1, COP_0_TLB_LO0		# init low reg0.
	MIPS_CPU_NOP_DELAY
	tlbwi					# update slot found
	b	4f
	nop
1:
	mtc0	zero, COP_0_TLB_PG_MASK 	# init mask.
	_MTC0	a0, COP_0_TLB_HI		# init high reg.
	_MTC0	a1, COP_0_TLB_LO0		# init low reg0.
	_MTC0	a2, COP_0_TLB_LO1		# init low reg1.
	MIPS_CPU_NOP_DELAY
	tlbwr					# enter into a random slot
	MIPS_CPU_NOP_DELAY
	b	4f
	nop
# ODD
2:
	nop
	bltz	v0, 3f				# index < 0 => !found
	MIPS_CPU_NOP_DELAY

	tlbr					# read the entry first
	MIPS_CPU_NOP_DELAY
	_MTC0	a1, COP_0_TLB_LO1		# init low reg1.
	MIPS_CPU_NOP_DELAY
	tlbwi					# update slot found
	MIPS_CPU_NOP_DELAY
	b	4f
	nop
3:
	mtc0	zero, COP_0_TLB_PG_MASK		# init mask.
	_MTC0	a0, COP_0_TLB_HI		# init high reg.
	_MTC0	a2, COP_0_TLB_LO0		# init low reg0.
	_MTC0	a1, COP_0_TLB_LO1		# init low reg1.
	MIPS_CPU_NOP_DELAY
	tlbwr					# enter into a random slot

4:						# Make shure pipeline
	MIPS_CPU_NOP_DELAY
	_MTC0	t0, COP_0_TLB_HI		# restore PID
	mtc0	v1, COP_0_STATUS_REG		# Restore the status register
	ITLBNOPFIX
	j	ra
	nop
END(Mips_TLBUpdate)

/*--------------------------------------------------------------------------
 *
 * Mips_TLBRead(unsigned entry, struct tlb *tlb);
 *
 *	Read the TLB entry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	tlb will contain the TLB entry found.
 *
 *--------------------------------------------------------------------------
 */
LEAF(Mips_TLBRead)
	mfc0	v1, COP_0_STATUS_REG		# Save the status register.
	mtc0	zero, COP_0_STATUS_REG		# Disable interrupts
	ITLBNOPFIX
	_MFC0	t0, COP_0_TLB_HI		# Get current PID

	mtc0	a0, COP_0_TLB_INDEX		# Set the index register
	MIPS_CPU_NOP_DELAY
	tlbr					# Read from the TLB
	MIPS_CPU_NOP_DELAY
	mfc0	t2, COP_0_TLB_PG_MASK		# fetch the hi entry
	_MFC0	t3, COP_0_TLB_HI		# fetch the hi entry
	_MFC0	ta0, COP_0_TLB_LO0		# See what we got
	_MFC0	ta1, COP_0_TLB_LO1		# See what we got
	_MTC0	t0, COP_0_TLB_HI		# restore PID
	MIPS_CPU_NOP_DELAY
	mtc0	v1, COP_0_STATUS_REG		# Restore the status register
	ITLBNOPFIX
	sw	t2, 0(a1)
	sw	t3, 4(a1)
	sw	ta0, 8(a1)
	j	ra
	sw	ta1, 12(a1)
END(Mips_TLBRead)

/*--------------------------------------------------------------------------
 *
 * Mips_TLBGetPID(void);
 *
 * Results:
 *	Returns the current TLB pid reg.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------------------
 */
LEAF(Mips_TLBGetPID)
	_MFC0	v0, COP_0_TLB_HI		# get PID
	j	ra
	and	v0, v0, VMTLB_PID		# mask off PID
END(Mips_TLBGetPID)



/*--------------------------------------------------------------------------
 *
 * void mips_TBIAP(int sizeofTLB);
 *
 * Invalidate TLB entries belong to per process user spaces while
 * leaving entries for kernel space marked global intact.
 *
 *--------------------------------------------------------------------------
 */
LEAF(mips_TBIAP)
	mfc0	v1, COP_0_STATUS_REG		# save status register
	mtc0	zero, COP_0_STATUS_REG		# disable interrupts

	_MFC0	ta0, COP_0_TLB_HI		# Get current PID
	move	t2, a0
	mfc0	t1, COP_0_TLB_WIRED
	#
	# Load invalid entry, each TLB entry should have it's own bogus 
	# address calculated by following expression:
	# MIPS_KSEG0_START + 2 * i * PAGE_SIZE;
	# One bogus value for every TLB entry might cause MCHECK exception
	#
	sll	t3, t1, PAGE_SHIFT + 1
	li	v0, MIPS_KSEG0_START		# invalid address
	addu	v0, t3

	mfc0	t3, COP_0_TLB_PG_MASK		# save current pgMask

	# do {} while (t1 < t2)
1:
	mtc0	t1, COP_0_TLB_INDEX		# set index
	MIPS_CPU_NOP_DELAY
	tlbr					# obtain an entry
	MIPS_CPU_NOP_DELAY
	_MFC0	a0, COP_0_TLB_LO1
	and	a0, a0, PTE_G			# check to see it has G bit
	bnez	a0, 2f
	nop

	_MTC0	v0, COP_0_TLB_HI		# make entryHi invalid
	_MTC0	zero, COP_0_TLB_LO0		# zero out entryLo0
	_MTC0	zero, COP_0_TLB_LO1		# zero out entryLo1
	mtc0	zero, COP_0_TLB_PG_MASK		# zero out mask entry
	MIPS_CPU_NOP_DELAY
	tlbwi					# invalidate the TLB entry
2:
	addu	t1, t1, 1
	addu	v0, 1 << (PAGE_SHIFT + 1)
	bne	t1, t2, 1b
	nop

	_MTC0	ta0, COP_0_TLB_HI		# restore PID
	mtc0	t3, COP_0_TLB_PG_MASK		# restore pgMask
	MIPS_CPU_NOP_DELAY
	mtc0	v1, COP_0_STATUS_REG		# restore status register
	j	ra				# new ASID will be set soon
	nop
	.set	mips2
END(mips_TBIAP)
