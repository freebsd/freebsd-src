/*
 * Copyright (c) 2022, Netflix, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * This is the trampoline that starts the FreeBSD kernel. Since the Linux kernel
 * calls this routine with no args, and has a different environment than the boot
 * loader provides and that the kernel expects, this code is responsible for setting
 * all that up and calling the normal kernel entry point. It's analogous ot the
 * "purgatory" code in the linux kernel. Details about these operations are
 * contained in comments below. On aarch64, the kernel will start all the APs so
 * we don't have to worry about them here.
 */

/*
 * Keep in sync with exec.c. Kexec starts aarch64_tramp w/o any
 * parameters, so store them here.
 *
 * struct trampoline_data {
 *	uint64_t	entry;			//  0 (PA where kernel loaded)
 *	uint64_t	modulep;		//  8 module metadata
 * };
 *
 * The aarch64 _start routine assumes:
 *  MMU      on with an identity map, or off
 *  D-Cache: off
 *  I-Cache: on or off
 *  We are loaded at a 2MiB aligned address
 *  Module data (modulep) pointer in x0
 *
 * Unlike EFI, we don't support copying the staging area. We tell Linunx to land
 * the kernel in its final location with the needed alignment, etc.
 *
 * This trampoline installs sets up the arguments the kernel expects, flushes
 * the cache lines and jumps to the kernel _start address. We pass the modulep
 * pointer in x0, as _start expects.
 */
	.text
	.globl	aarch64_tramp
aarch64_tramp:
	b	1f		/* skip over our saved args */
	.p2align	3
trampoline_data:
#define TRAMP_ENTRY	0
#define TRAMP_MODULEP	8
#define TRAMP_TOTAL	16
	.space TRAMP_TOTAL
#define TMPSTACKSIZE	48	/* 16 bytes for args +8 for pushq/popfq + 24 spare */
1:
	adr	x2, trampoline_data
	ldr	x1, [x2, #TRAMP_ENTRY]
	ldr	x0, [x2, #TRAMP_MODULEP]
	br	x1

	.p2align 4
	.space	TMPSTACKSIZE
aarch64_tramp_end:			/* padding doubles as stack */

	.data
	.globl	aarch64_tramp_size
aarch64_tramp_size:
	.long	aarch64_tramp_end-aarch64_tramp
	.globl	aarch64_tramp_data_offset
aarch64_tramp_data_offset:
	.long	trampoline_data-aarch64_tramp
