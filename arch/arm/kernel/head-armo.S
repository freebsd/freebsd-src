/*
 *  linux/arch/arm/kernel/head-armo.S
 *
 *  Copyright (C) 1994-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  26-bit kernel startup code
 */
#include <linux/config.h>
#include <linux/linkage.h>
#include <asm/mach-types.h>

		.globl	SYMBOL_NAME(swapper_pg_dir)
		.equ	SYMBOL_NAME(swapper_pg_dir),	0x0207d000

/*
 * Entry point.
 */
		.section ".text.init",#alloc,#execinstr
ENTRY(stext)
__entry:	cmp	pc, #0x02000000
		ldrlt	pc, LC0			@ if 0x01800000, call at 0x02080000
		teq	r0, #0			@ Check for old calling method
		blne	oldparams		@ Move page if old
		adr	r0, LC0
		ldmib	r0, {r2-r5, sp}		@ Setup stack
		mov	r0, #0
1:		cmp	r2, r3			@ Clear BSS
		strcc	r0, [r2], #4
		bcc	1b

		bl	detect_proc_type
		str	r0, [r4]
		bl	detect_arch_type
		str	r0, [r5]

		mov	fp, #0
		b	SYMBOL_NAME(start_kernel)

LC0:		.word	SYMBOL_NAME(_stext)
		.word	SYMBOL_NAME(__bss_start)		@ r2
		.word	SYMBOL_NAME(_end)			@ r3
		.word	SYMBOL_NAME(processor_id)		@ r4
		.word	SYMBOL_NAME(__machine_arch_type)	@ r5
		.word	SYMBOL_NAME(init_task_union)+8192	@ sp
arm2_id:	.long	0x41560200
arm250_id:	.long	0x41560250
		.align

oldparams:	mov	r4, #0x02000000
		add	r3, r4, #0x00080000
		add	r4, r4, #0x0007c000
1:		ldmia	r0!, {r5 - r12}
		stmia	r4!, {r5 - r12}
		cmp	r4, r3
		blt	1b
		mov	pc, lr

/*
 * We need some way to automatically detect the difference between
 * these two machines.  Unfortunately, it is not possible to detect
 * the presence of the SuperIO chip, because that will hang the old
 * Archimedes machines solid.
 */
/* DAG: Outdated, these have been combined !!!!!!! */
detect_arch_type:
#if defined(CONFIG_ARCH_ARC)
		mov	r0, #MACH_TYPE_ARCHIMEDES
#elif defined(CONFIG_ARCH_A5K)
		mov	r0, #MACH_TYPE_A5K
#endif
		mov	pc, lr

detect_proc_type:
		mov	ip, lr
		mov	r2, #0xea000000		@ Point undef instr to continuation
		adr	r0, continue - 12
		orr	r0, r2, r0, lsr #2
		mov	r1, #0
		str	r0, [r1, #4]
		ldr	r0, arm2_id
		swp	r2, r2, [r1]		@ check for swp (ARM2 cant)
		ldr	r0, arm250_id
		mrc	15, 0, r3, c0, c0	@ check for CP#15 (ARM250 cant)
		mov	r0, r3
continue:	mov	r2, #0xeb000000		@ Make undef vector loop
		sub	r2, r2, #2
		str	r2, [r1, #4]
		mov	pc, ip
