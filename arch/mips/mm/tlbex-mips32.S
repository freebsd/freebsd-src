/*
 * TLB exception handling code for MIPS32 CPUs.
 *
 * Copyright (C) 1994, 1995, 1996 by Ralf Baechle and Andreas Busse
 *
 * Multi-cpu abstraction and reworking:
 * Copyright (C) 1996 David S. Miller (dm@engr.sgi.com)
 *
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 *
 * Pete Popov, ppopov@pacbell.net
 * Added 36 bit phys address support.
 * Copyright (C) 2002 MontaVista Software, Inc.
 */
#include <linux/init.h>

#include <asm/asm.h>
#include <asm/current.h>
#include <asm/offset.h>
#include <asm/cachectl.h>
#include <asm/fpregdef.h>
#include <asm/mipsregs.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/processor.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

#define TLB_OPTIMIZE /* If you are paranoid, disable this. */

#ifdef CONFIG_64BIT_PHYS_ADDR

/* We really only support 36 bit physical addresses on MIPS32 */
#define PTE_L		lw
#define PTE_S		sw
#define PTE_SRL		srl
#define P_MTC0		mtc0
#define PTE_HALF        4 /* pte_high contains pre-shifted, ready to go entry */
#define PTE_SIZE        8
#define PTEP_INDX_MSK	0xff0
#define PTE_INDX_MSK	0xff8
#define PTE_INDX_SHIFT 9
#define CONVERT_PTE(pte)
#define PTE_MAKEWRITE_HIGH(pte, ptr) \
	lw	pte, 4(ptr); \
	ori	pte, (_PAGE_VALID | _PAGE_DIRTY); \
	sw	pte, 4(ptr); \
	lw	pte, 0(ptr);

#define PTE_MAKEVALID_HIGH(pte, ptr) \
	lw	pte, 4(ptr); \
	ori	pte, pte, _PAGE_VALID; \
	sw	pte, 4(ptr); \
	lw	pte, 0(ptr);

#else

#define PTE_L		lw
#define PTE_S		sw
#define PTE_SRL		srl
#define P_MTC0		mtc0
#define PTE_HALF        0
#define PTE_SIZE	4
#define PTEP_INDX_MSK	0xff8
#define PTE_INDX_MSK	0xffc
#define PTE_INDX_SHIFT	10
#define CONVERT_PTE(pte) srl pte, pte, 6
#define PTE_MAKEWRITE_HIGH(pte, ptr)
#define PTE_MAKEVALID_HIGH(pte, ptr)

#endif  /* CONFIG_64BIT_PHYS_ADDR */

	__INIT

#ifdef CONFIG_64BIT_PHYS_ADDR
#define GET_PTE_OFF(reg)
#else
#define GET_PTE_OFF(reg)	srl	reg, reg, 1
#endif

/*	
 * These handlers much be written in a relocatable manner
 * because based upon the cpu type an arbitrary one of the
 * following pieces of code will be copied to the KSEG0
 * vector location.
 */
	/* TLB refill, EXL == 0, MIPS32 version */
	.set	noreorder
	.set	noat
	LEAF(except_vec0_r4000)
	.set	mips3
#ifdef CONFIG_SMP
	mfc0	k1, CP0_CONTEXT
	la	k0, pgd_current
	srl	k1, 23
	sll	k1, 2				# log2(sizeof(pgd_t)
	addu	k1, k0, k1
	lw	k1, (k1)
#else 
	lw	k1, pgd_current			# get pgd pointer
#endif	
	nop
	mfc0	k0, CP0_BADVADDR		# Get faulting address
	srl	k0, k0, _PGDIR_SHIFT		# get pgd only bits

	sll	k0, k0, 2
	addu	k1, k1, k0			# add in pgd offset
	mfc0	k0, CP0_CONTEXT			# get context reg
	lw	k1, (k1)
	GET_PTE_OFF(k0)				# get pte offset
	and	k0, k0, PTEP_INDX_MSK
	addu	k1, k1, k0			# add in offset

	PTE_L	k0, PTE_HALF(k1)		# get even pte
	CONVERT_PTE(k0)
	P_MTC0	k0, CP0_ENTRYLO0		# load it
	PTE_L	k1, (PTE_HALF+PTE_SIZE)(k1)	# get odd pte
	CONVERT_PTE(k1)
	P_MTC0	k1, CP0_ENTRYLO1		# load it
	b	1f
	tlbwr					# write random tlb entry
1:
	nop
	eret					# return from trap
	END(except_vec0_r4000)

/*
 * These are here to avoid putting ifdefs in tlb-r4k.c
 */
	.set	noreorder
	.set	noat
	LEAF(except_vec0_nevada)
	.set	mips3
	PANIC("Nevada Exception Vec 0 called")
	END(except_vec0_nevada)

	.set	noreorder
	.set	noat
	LEAF(except_vec0_r4600)
	.set	mips3
	PANIC("R4600 Exception Vec 0 called")
	END(except_vec0_r4600)

	__FINIT

/*
 * ABUSE of CPP macros 101.
 *
 * After this macro runs, the pte faulted on is
 * in register PTE, a ptr into the table in which
 * the pte belongs is in PTR.
 */

#ifdef CONFIG_SMP
#define GET_PGD(scratch, ptr)        \
	mfc0    ptr, CP0_CONTEXT;    \
	la      scratch, pgd_current;\
	srl     ptr, 23;             \
	sll     ptr, 2;              \
	addu    ptr, scratch, ptr;   \
	lw      ptr, (ptr);          
#else
#define GET_PGD(scratch, ptr)    \
	lw	ptr, pgd_current;
#endif

#define LOAD_PTE(pte, ptr) \
	GET_PGD(pte, ptr)          \
	mfc0	pte, CP0_BADVADDR; \
	srl	pte, pte, _PGDIR_SHIFT; \
	sll	pte, pte, 2; \
	addu	ptr, ptr, pte; \
	mfc0	pte, CP0_BADVADDR; \
	lw	ptr, (ptr); \
	srl	pte, pte, PTE_INDX_SHIFT; \
	and	pte, pte, PTE_INDX_MSK; \
	addu	ptr, ptr, pte; \
	PTE_L	pte, (ptr);

	/* This places the even/odd pte pair in the page
	 * table at PTR into ENTRYLO0 and ENTRYLO1 using
	 * TMP as a scratch register.
	 */
#define PTE_RELOAD(ptr, tmp) \
	ori	ptr, ptr, PTE_SIZE; \
	xori	ptr, ptr, PTE_SIZE; \
	PTE_L	tmp, (PTE_HALF+PTE_SIZE)(ptr); \
	CONVERT_PTE(tmp); \
	P_MTC0	tmp, CP0_ENTRYLO1; \
	PTE_L	ptr, PTE_HALF(ptr); \
	CONVERT_PTE(ptr); \
	P_MTC0	ptr, CP0_ENTRYLO0;

#define DO_FAULT(write) \
	.set	noat; \
	SAVE_ALL; \
	mfc0	a2, CP0_BADVADDR; \
	STI; \
	.set	at; \
	move	a0, sp; \
	jal	do_page_fault; \
	 li	a1, write; \
	j	ret_from_exception; \
	 nop; \
	.set	noat;

	/* Check is PTE is present, if not then jump to LABEL.
	 * PTR points to the page table where this PTE is located,
	 * when the macro is done executing PTE will be restored
	 * with it's original value.
	 */
#define PTE_PRESENT(pte, ptr, label) \
	andi	pte, pte, (_PAGE_PRESENT | _PAGE_READ); \
	xori	pte, pte, (_PAGE_PRESENT | _PAGE_READ); \
	bnez	pte, label; \
	PTE_L	pte, (ptr);

	/* Make PTE valid, store result in PTR. */
#define PTE_MAKEVALID(pte, ptr) \
	ori	pte, pte, (_PAGE_VALID | _PAGE_ACCESSED); \
	PTE_S	pte, (ptr);

	/* Check if PTE can be written to, if not branch to LABEL.
	 * Regardless restore PTE with value from PTR when done.
	 */
#define PTE_WRITABLE(pte, ptr, label) \
	andi	pte, pte, (_PAGE_PRESENT | _PAGE_WRITE); \
	xori	pte, pte, (_PAGE_PRESENT | _PAGE_WRITE); \
	bnez	pte, label; \
	PTE_L	pte, (ptr);

	/* Make PTE writable, update software status bits as well,
	 * then store at PTR.
	 */
#define PTE_MAKEWRITE(pte, ptr) \
	ori	pte, pte, (_PAGE_ACCESSED | _PAGE_MODIFIED | \
			   _PAGE_VALID | _PAGE_DIRTY); \
	PTE_S	pte, (ptr);

	.set	noreorder

#define R5K_HAZARD nop

	.align	5
	NESTED(handle_tlbl, PT_SIZE, sp)
	.set	noat
invalid_tlbl:
#ifdef TLB_OPTIMIZE
	/* Test present bit in entry. */
	LOAD_PTE(k0, k1)
	R5K_HAZARD
	tlbp
	PTE_PRESENT(k0, k1, nopage_tlbl)
	PTE_MAKEVALID_HIGH(k0, k1)
	PTE_MAKEVALID(k0, k1)
	PTE_RELOAD(k1, k0)
	nop
	b	1f
	 tlbwi
1:
	nop
	.set	mips3	
	eret
	.set	mips0
#endif

nopage_tlbl:
	DO_FAULT(0)
	END(handle_tlbl)

	.align	5
	NESTED(handle_tlbs, PT_SIZE, sp)
	.set	noat
#ifdef TLB_OPTIMIZE
	.set	mips3
        li      k0,0
	LOAD_PTE(k0, k1)
	R5K_HAZARD
	tlbp				# find faulting entry
	PTE_WRITABLE(k0, k1, nopage_tlbs)
	PTE_MAKEWRITE(k0, k1)
	PTE_MAKEWRITE_HIGH(k0, k1)
	PTE_RELOAD(k1, k0)
	nop
	b	1f
	 tlbwi
1:
	nop
	.set	mips3
	eret
	.set	mips0
#endif

nopage_tlbs:
	DO_FAULT(1)
	END(handle_tlbs)

	.align	5
	NESTED(handle_mod, PT_SIZE, sp)
	.set	noat
#ifdef TLB_OPTIMIZE
	.set	mips3
	LOAD_PTE(k0, k1)
	R5K_HAZARD
	tlbp					# find faulting entry
	andi	k0, k0, _PAGE_WRITE
	beqz	k0, nowrite_mod
	PTE_L	k0, (k1)

	/* Present and writable bits set, set accessed and dirty bits. */
	PTE_MAKEWRITE(k0, k1)
	PTE_MAKEWRITE_HIGH(k0, k1)
	/* Now reload the entry into the tlb. */
	PTE_RELOAD(k1, k0)
	nop
	b	1f
	 tlbwi
1:
	nop
	.set	mips3
	eret
	.set	mips0
#endif

nowrite_mod:
	DO_FAULT(1)
	END(handle_mod)

