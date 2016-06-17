/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1994, 1995 Waldorf Electronics
 * Written by Ralf Baechle and Andreas Busse
 * Copyright (C) 1995 - 1999 Ralf Baechle
 * Copyright (C) 1996 Paul M. Antoine
 * Modified for DECStation and hence R3000 support by Paul M. Antoine
 * Further modifications by David S. Miller and Harald Koerfgen
 * Copyright (C) 1999 Silicon Graphics, Inc.
 *
 * Kevin Kissell, kevink@mips.com and Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/threads.h>

#include <asm/asm.h>
#include <asm/current.h>
#include <asm/offset.h>
#include <asm/pgtable-bits.h>
#include <asm/processor.h>
#include <asm/regdef.h>
#include <asm/cachectl.h>
#include <asm/mipsregs.h>
#include <asm/stackframe.h>

		.text
		/*
		 * Reserved space for exception handlers.
		 * Necessary for machines which link their kernels at KSEG0.
		 */
		.fill	0x400

		/* The following two symbols are used for kernel profiling. */
		EXPORT(stext)
		EXPORT(_stext)

		__INIT

		/* Cache Error */
		LEAF(except_vec2_generic)
		.set	noreorder
		.set	noat
		.set    mips0
		/*
		 * This is a very bad place to be.  Our cache error
		 * detection has triggered.  If we have write-back data
		 * in the cache, we may not be able to recover.  As a
		 * first-order desperate measure, turn off KSEG0 cacheing.
		 */
		mfc0	k0,CP0_CONFIG
		li	k1,~CONF_CM_CMASK
		and	k0,k0,k1
		ori	k0,k0,CONF_CM_UNCACHED
		mtc0	k0,CP0_CONFIG
		/* Give it a few cycles to sink in... */
		nop
		nop
		nop

		j	cache_parity_error
		nop
		END(except_vec2_generic)

		.set	at

		/*
		 * Special interrupt vector for embedded MIPS.  This is a
		 * dedicated interrupt vector which reduces interrupt processing
		 * overhead.  The jump instruction will be inserted here at
		 * initialization time.  This handler may only be 8 bytes in
		 * size!
		 */
		NESTED(except_vec4, 0, sp)
1:		j	1b			/* Dummy, will be replaced */
		 nop
		END(except_vec4)

		/*
		 * EJTAG debug exception handler.
		 * The EJTAG debug exception entry point is 0xbfc00480, which
		 * normally is in the boot PROM, so the boot PROM must do a
		 * unconditional jump to this vector.
		 */
		NESTED(except_vec_ejtag_debug, 0, sp)
		j	ejtag_debug_handler
		 nop
		END(except_vec_ejtag_debug)

		__FINIT

		/*
		 * EJTAG debug exception handler.
		 */
		NESTED(ejtag_debug_handler, PT_SIZE, sp)
		.set	noat
		.set	noreorder
		mtc0	k0, CP0_DESAVE
		mfc0	k0, CP0_DEBUG

		sll	k0, k0, 30	# Check for SDBBP.
		bgez	k0, ejtag_return

		la	k0, ejtag_debug_buffer
		sw	k1, 0(k0)
		SAVE_ALL
		jal	ejtag_exception_handler
		 move	a0, sp
		RESTORE_ALL
		la	k0, ejtag_debug_buffer
		lw	k1, 0(k0)

ejtag_return:
		mfc0	k0, CP0_DESAVE
		.set	mips32
		deret
		.set	mips0
		 nop
		.set	at
		END(ejtag_debug_handler)

		__INIT

		/*
		* NMI debug exception handler for MIPS reference boards.
		* The NMI debug exception entry point is 0xbfc00000, which
		* normally is in the boot PROM, so the boot PROM must do a
		* unconditional jump to this vector.
		*/
		NESTED(except_vec_nmi, 0, sp)
		j       nmi_handler
		 nop
		END(except_vec_nmi)

		__FINIT

		NESTED(nmi_handler, PT_SIZE, sp)
		.set    noat
		.set    noreorder
		.set    mips3
		SAVE_ALL
		jal     nmi_exception_handler
		 move   a0, sp
		RESTORE_ALL
		eret
		.set    at
		.set    mips0
		END(nmi_handler)

		__INIT

		/*
		 * Kernel entry point
		 */
		NESTED(kernel_entry, 16, sp)
		.set	noreorder

		/*
		 * The firmware/bootloader passes argc/argp/envp
		 * to us as arguments.  But clear bss first because
		 * the romvec and other important info is stored there
		 * by prom_init().
		 */
		la	t0, _edata
		sw	zero, (t0)
		la	t1, (_end - 4)
1:
		addiu	t0, 4
		bne	t0, t1, 1b
		 sw	zero, (t0)

		/*
		 * Stack for kernel and init, current variable
		 */
		la	$28, init_task_union
		addiu	t0, $28, KERNEL_STACK_SIZE-32
		subu	sp, t0, 4*SZREG
		sw	t0, kernelsp

		jal	init_arch
		 nop
		END(kernel_entry)


#ifdef CONFIG_SMP

/*
 * SMP slave cpus entry point.  Board specific code for bootstrap calls this
 * function after setting up the stack and gp registers.
 */
		LEAF(smp_bootstrap)
		.set push
		.set noreorder
		mtc0	zero, CP0_WIRED
		CLI
		mfc0	t0, CP0_STATUS
		li	t1, ~(ST0_CU1|ST0_CU2|ST0_CU3|ST0_KX|ST0_SX|ST0_UX)
		and	t0, t1
		or	t0, (ST0_CU0);
		jal	start_secondary
		mtc0	t0, CP0_STATUS
		.set pop
		END(smp_bootstrap)
#endif

		__FINIT

		/*
		 * This buffer is reserved for the use of the EJTAG debug
		 * handler.
		 */
		.data
		EXPORT(ejtag_debug_buffer)
		.fill	4

		.comm	kernelsp,    NR_CPUS * 8, 8
		.comm	pgd_current, NR_CPUS * 8, 8

	.macro	page name, order=0
	.globl	\name
\name:	.size	\name, (_PAGE_SIZE << \order)
	.org	. + (_PAGE_SIZE << \order)
	.type	\name, @object
	.endm

	.data
	.align	12

	page	swapper_pg_dir, _PGD_ORDER
	page	empty_bad_page, 0
	page	empty_bad_page_table, 0
	page	invalid_pte_table, 0
