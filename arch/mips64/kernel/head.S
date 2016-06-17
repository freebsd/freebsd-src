/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Head.S contains the MIPS exception handler and startup code.
 *
 * Copyright (C) 1994, 1995 Waldorf Electronics
 * Written by Ralf Baechle and Andreas Busse
 * Copyright (C) 1994, 1995, 1996, 1997, 1998, 1999 Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <asm/asm.h>
#include <asm/regdef.h>
#include <asm/processor.h>
#include <asm/mipsregs.h>
#include <asm/stackframe.h>
#include <asm/pgtable.h>
#include <asm/sn/addrs.h>
#include <asm/sn/sn0/hubni.h>
#include <asm/sn/klkernvars.h>

	.macro	ARC64_TWIDDLE_PC
#if defined(CONFIG_ARC64) || defined(CONFIG_MAPPED_KERNEL)
	/* We get launched at a XKPHYS address but the kernel is linked to
	   run at a KSEG0 address, so jump there.  */
	PTR_LA	t0, \@f
	jr	t0
\@:
#endif
	.endm

#ifdef CONFIG_SGI_IP27
	/*
	 * outputs the local nasid into res.  IP27 stuff.
	 */
	.macro GET_NASID_ASM res
	dli	\res, LOCAL_HUB_ADDR(NI_STATUS_REV_ID)
	ld	\res, (\res)
	and	\res, NSRI_NODEID_MASK
	dsrl	\res, NSRI_NODEID_SHFT
	.endm
#endif /* CONFIG_SGI_IP27 */

	/*
	 * inputs are the text nasid in t1, data nasid in t2.
	 */
	.macro MAPPED_KERNEL_SETUP_TLB
#ifdef CONFIG_MAPPED_KERNEL
	/*
	 * This needs to read the nasid - assume 0 for now.
	 * Drop in 0xffffffffc0000000 in tlbhi, 0+VG in tlblo_0,
	 * 0+DVG in tlblo_1.
	 */
	dli	t0, 0xffffffffc0000000
	dmtc0	t0, CP0_ENTRYHI
	li	t0, 0x1c000		# Offset of text into node memory
	dsll	t1, NASID_SHFT		# Shift text nasid into place
	dsll	t2, NASID_SHFT		# Same for data nasid
	or	t1, t1, t0		# Physical load address of kernel text
	or	t2, t2, t0		# Physical load address of kernel data
	dsrl	t1, 12			# 4K pfn
	dsrl	t2, 12			# 4K pfn
	dsll	t1, 6			# Get pfn into place
	dsll	t2, 6			# Get pfn into place
	li	t0, ((_PAGE_GLOBAL|_PAGE_VALID| _CACHE_CACHABLE_COW) >> 6)
	or	t0, t0, t1
	mtc0	t0, CP0_ENTRYLO0	# physaddr, VG, cach exlwr
	li	t0, ((_PAGE_GLOBAL|_PAGE_VALID| _PAGE_DIRTY|_CACHE_CACHABLE_COW) >> 6)
	or	t0, t0, t2
	mtc0	t0, CP0_ENTRYLO1	# physaddr, DVG, cach exlwr
	li	t0, 0x1ffe000		# MAPPED_KERN_TLBMASK, TLBPGMASK_16M
	mtc0	t0, CP0_PAGEMASK
	li	t0, 0			# KMAP_INX
	mtc0	t0, CP0_INDEX
	li	t0, 1
	mtc0	t0, CP0_WIRED
	tlbwi
#else
	mtc0	zero, CP0_WIRED
#endif
	.endm

	.text

EXPORT(stext)					# used for profiling
EXPORT(_stext)

	__INIT

NESTED(kernel_entry, 16, sp)			# kernel entry point

	ori	sp, 0xf				# align stack on 16 byte.
	xori	sp, 0xf

#ifdef CONFIG_SGI_IP27
	GET_NASID_ASM	t1
	move	t2, t1				# text and data are here
	MAPPED_KERNEL_SETUP_TLB
#endif /* IP27 */

	ARC64_TWIDDLE_PC

	CLI					# disable interrupts

	/*
	 * The firmware/bootloader passes argc/argp/envp
	 * to us as arguments.  But clear bss first because
	 * the romvec and other important info is stored there
	 * by prom_init().
	 */
	PTR_LA	t0, _edata
	sd	zero, (t0)
	PTR_LA	t1, (_end - 8)
1:
	daddiu	t0, 8
	sd	zero, (t0)
	bne	t0, t1, 1b

	PTR_LA	$28, init_task_union		# init current pointer
	daddiu	sp, $28, KERNEL_STACK_SIZE-32
	set_saved_sp	sp, t0, t1
	dsubu	sp, 4*SZREG			# init stack pointer

	j	init_arch
	END(kernel_entry)

#ifdef CONFIG_SMP
/*
 * SMP slave cpus entry point.  Board specific code for bootstrap calls this
 * function after setting up the stack and gp registers.
 */
NESTED(smp_bootstrap, 16, sp)
#ifdef CONFIG_SGI_IP27
	GET_NASID_ASM	t1
	dli	t0, KLDIR_OFFSET + (KLI_KERN_VARS * KLDIR_ENT_SIZE) + \
		    KLDIR_OFF_POINTER + K0BASE
	dsll	t1, NASID_SHFT
	or	t0, t0, t1
	ld	t0, 0(t0)			# t0 points to kern_vars struct
	lh	t1, KV_RO_NASID_OFFSET(t0)
	lh	t2, KV_RW_NASID_OFFSET(t0)
	MAPPED_KERNEL_SETUP_TLB
	ARC64_TWIDDLE_PC
#endif /* CONFIG_SGI_IP27 */

	CLI

	/*
	 * For the moment set ST0_KU so the CPU will not spit fire when
	 * executing 64-bit instructions.  The full initialization of the
	 * CPU's status register is done later in per_cpu_trap_init().
	 */
        mfc0	t0, CP0_STATUS
        or	t0, ST0_KX
	mtc0	t0, CP0_STATUS

	jal	start_secondary			# XXX: IP27: cboot

	END(smp_bootstrap)
#endif /* CONFIG_SMP */

	__FINIT

	declare_saved_sp

	.macro	page name, order=0
	.globl	\name
\name:	.size	\name, (_PAGE_SIZE << \order)
	.org	. + (_PAGE_SIZE << \order)
	.type	\name, @object
	.endm

	.data
	.align	12

	page	swapper_pg_dir, _PGD_ORDER
	page	invalid_pmd_table, _PMD_ORDER
	page	invalid_pte_table, _PTE_ORDER
	page	kptbl, _PGD_ORDER
	.globl	ekptbl
	page	kpmdtbl, 0
ekptbl:
