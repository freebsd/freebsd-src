/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1999 Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */
#include <linux/init.h>
#include <asm/mipsregs.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>

	.macro	__BUILD_cli
	CLI
	.endm

	.macro	__BUILD_sti
	STI
	.endm

	.macro	__BUILD_kmode
	KMODE
	.endm

	.macro	tlb_handler name interruptible writebit
	NESTED(__\name, PT_SIZE, sp)
	SAVE_ALL
	dmfc0	a2, CP0_BADVADDR
	__BUILD_\interruptible
	li	a1, \writebit
	sd	a2, PT_BVADDR(sp)
	move	a0, sp
	jal	do_page_fault
	j	ret_from_exception
	END(__\name)
	.endm

	tlb_handler	xtlb_mod kmode 1
	tlb_handler	xtlb_tlbl kmode 0
	tlb_handler	xtlb_tlbs kmode 1
