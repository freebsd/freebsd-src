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
#include <asm/page.h>
#include <asm/regdef.h>
#include <asm/stackframe.h>
#include <asm/war.h>

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

	.macro	tlb_handler_m3 name interruptible writebit
	NESTED(__\name, PT_SIZE, sp)
	dmfc0	k0, CP0_BADVADDR
	dmfc0	k1, CP0_ENTRYHI
	xor	k0, k1
	dsrl	k0, k0, PAGE_SHIFT + 1
	bnez	k0, 1f
	SAVE_ALL
	dmfc0	a2, CP0_BADVADDR
	__BUILD_\interruptible
	li	a1, \writebit
	sd	a2, PT_BVADDR(sp)
	move	a0, sp
	jal	do_page_fault
1:
	j	ret_from_exception
	END(__\name)
	.endm

	tlb_handler	xtlb_mod kmode 1
#if BCM1250_M3_WAR
	tlb_handler_m3	xtlb_tlbl kmode 0
#else
	tlb_handler	xtlb_tlbl kmode 0
#endif
	tlb_handler	xtlb_tlbs kmode 1
