/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000 Silicon Graphics, Inc.
 * Written by Ulf Carlsson (ulfc@engr.sgi.com)
 * Copyright (C) 2002  Maciej W. Rozycki
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/threads.h>

#include <asm/asm.h>
#include <asm/hazards.h>
#include <asm/regdef.h>
#include <asm/mipsregs.h>
#include <asm/pgtable.h>
#include <asm/stackframe.h>
#include <asm/war.h>

#define PGD_INDX_MASK	((_PTRS_PER_PGD - 1) << _PGD_T_LOG2)
#define PMD_INDX_MASK	((_PTRS_PER_PMD - 1) << _PMD_T_LOG2)
#define PTE_INDX_MASK	((_PTRS_PER_PTE - 1) << _PTE_T_LOG2)
#define PTEP_INDX_MASK	((_PTRS_PER_PTE >> 1 - 1) << (_PTE_T_LOG2 + 1))

	.data
	.comm	pgd_current, NR_CPUS * 8, 8

	/*
	 * After this macro runs we have a pointer to the pte of the address
	 * that caused the fault in PTR.
	 */
	.macro	LOAD_PTE2, ptr, tmp, kaddr
#ifdef CONFIG_SMP
	dmfc0	\ptr, CP0_CONTEXT
	dmfc0	\tmp, CP0_BADVADDR
	dsra	\ptr, 23			# get pgd_current[cpu]
#else
	dmfc0	\tmp, CP0_BADVADDR
	dla	\ptr, pgd_current
#endif
	bltz	\tmp, \kaddr
	 ld	\ptr, (\ptr)
	dsrl	\tmp, PGDIR_SHIFT - 3		# get pgd offset in bytes
	andi	\tmp, PGD_INDX_MASK
	daddu	\ptr, \tmp			# add in pgd offset
	dmfc0	\tmp, CP0_BADVADDR
	ld	\ptr, (\ptr)			# get pmd pointer
	dsrl	\tmp, PMD_SHIFT - 3		# get pmd offset in bytes
	andi	\tmp, PMD_INDX_MASK
	daddu	\ptr, \tmp			# add in pmd offset
	dmfc0	\tmp, CP0_XCONTEXT
	ld	\ptr, (\ptr)			# get pte pointer
	andi	\tmp, PTEP_INDX_MASK		# get pte offset
	daddu	\ptr, \tmp
	.endm


	/*
	 * Ditto for the kernel table.
	 */
	.macro	LOAD_KPTE2, ptr, tmp, not_vmalloc
	/*
	 * First, determine that the address is in/above vmalloc range.
	 */
	dmfc0	\tmp, CP0_BADVADDR
	dli	\ptr, VMALLOC_START

	/*
	 * Now find offset into kptbl.
	 */
	dsubu	\tmp, \tmp, \ptr
	dla	\ptr, kptbl
	dsrl	\tmp, _PAGE_SHIFT + 1		# get vpn2
	dsll	\tmp, 4				# byte offset of pte
	daddu	\ptr, \ptr, \tmp

	/*
	 * Determine that fault address is within vmalloc range.
	 */
	dla	\tmp, ekptbl
	slt	\tmp, \ptr, \tmp
	beqz	\tmp, \not_vmalloc		# not vmalloc
	 nop
	.endm


	/*
	 * This places the even/odd pte pair in the page table at the pte
	 * entry pointed to by PTE into ENTRYLO0 and ENTRYLO1.
	 */
	.macro	PTE_RELOAD, pte0, pte1
	dsrl	\pte0, 6			# convert to entrylo0
	dmtc0	\pte0, CP0_ENTRYLO0		# load it
	dsrl	\pte1, 6			# convert to entrylo1
	dmtc0	\pte1, CP0_ENTRYLO1		# load it
	.endm


	.text
	.set	noreorder
	.set	mips3

	__INIT

	.align	5
LEAF(except_vec0_generic)
	.set	noat
	PANIC("Unused vector called")
1:	b	1b
	 nop
END(except_vec0_generic)


	/*
	 * TLB refill handlers for the R4000 and SB1.
	 * Attention:  We may only use 32 instructions / 128 bytes.
	 */
	.align  5
LEAF(except_vec1_r4k)
	.set    noat
	dla     k0, handle_vec1_r4k
	jr      k0
	 nop
END(except_vec1_r4k)

LEAF(except_vec1_sb1)
#if BCM1250_M3_WAR
	dmfc0	k0, CP0_BADVADDR
	dmfc0	k1, CP0_ENTRYHI
	xor	k0, k1
	dsrl	k0, k0, _PAGE_SHIFT + 1
	bnez	k0, 1f
#endif
	.set    noat
	dla     k0, handle_vec1_r4k
	jr      k0
	 nop

1:	eret
	nop
END(except_vec1_sb1)

	__FINIT

	.align  5
LEAF(handle_vec1_r4k)
	.set    noat
	LOAD_PTE2 k1 k0 9f
	ld	k0, 0(k1)			# get even pte
	ld	k1, 8(k1)			# get odd pte
	PTE_RELOAD k0 k1
	rm9000_tlb_hazard
	b	1f
	 tlbwr
1:	nop
	rm9000_tlb_hazard
	eret

9:						# handle the vmalloc range
	LOAD_KPTE2 k1 k0 invalid_vmalloc_address
	ld	k0, 0(k1)			# get even pte
	ld	k1, 8(k1)			# get odd pte
	PTE_RELOAD k0 k1
	rm9000_tlb_hazard
	b	1f
	 tlbwr
1:	nop
	rm9000_tlb_hazard
	eret
END(handle_vec1_r4k)


	__INIT

	/*
	 * TLB refill handler for the R10000.
	 * Attention:  We may only use 32 instructions / 128 bytes.
	 */
	.align	5
LEAF(except_vec1_r10k)
	.set    noat
	dla     k0, handle_vec1_r10k
	jr      k0
	 nop
END(except_vec1_r10k)

	__FINIT

	.align	5
LEAF(handle_vec1_r10k)
	.set	noat
	LOAD_PTE2 k1 k0 9f
	ld	k0, 0(k1)			# get even pte
	ld	k1, 8(k1)			# get odd pte
	PTE_RELOAD k0 k1
	rm9000_tlb_hazard
	nop
	tlbwr
	rm9000_tlb_hazard
	eret

9:						# handle the vmalloc range
	LOAD_KPTE2 k1 k0 invalid_vmalloc_address
	ld	k0, 0(k1)			# get even pte
	ld	k1, 8(k1)			# get odd pte
	PTE_RELOAD k0 k1
	rm9000_tlb_hazard
	nop
	tlbwr
	rm9000_tlb_hazard
	eret
END(handle_vec1_r10k)


	.align	5
LEAF(invalid_vmalloc_address)
	.set	noat
	PANIC("Invalid kernel address")
1:	b	1b
	 nop
END(invalid_vmalloc_address)
