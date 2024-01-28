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
 *	uint64_t	memmap_src;		// 16 Linux-provided memory map PA
 *	uint64_t	memmap_dst;		// 24 Module data copy PA
 *	uint64_t	memmap_len;		// 32 Length to copy
 * };
 *
 * FreeBSD's arm64 entry point is _start which assumes:
 *  MMU      on with an identity map, or off
 *  D-Cache: off
 *  I-Cache: on or off
 *  We are loaded at a 2MiB aligned address
 *  Module data (modulep) pointer in x0
 *
 * The rest of the boot loader tells Linux to land the kernel in its final
 * location with the needed alignment, etc. It does this, and then we take over.
 *
 * The linux kernel will helpfully turn off the MMU, flush the caches, disables
 * them, etc. It calls the tramp with two args: FDT blob address in x0 and the
 * EL2 vectors in x1. Currently, we make use of neither of these parameters: we
 * pass whatever dtb we think we need as part of the module data and we're a bit
 * weak on hypervisor support at the moment. _start's requirements are all
 * satisifed.
 *
 * This trampoline sets up the arguments the kernel expects and jumps to the
 * kernel _start address. We pass the modulep pointer in x0, as _start
 * expects. We assume that the various cache flushing, invalidation, etc that
 * linux did during or after copying the data down is sufficient, though we may
 * need to be mindful of cache flushing if we run in EL2 (TBD).
 *
 * Note, if TRAMP_MEMMAP_SRC is non-zero, then we have to copy the Linux
 * provided UEFI memory map. It's easier to do that here. In kboot we couldn't
 * access the physical memory, and it's a chicken and egg problem later in the
 * kernel.
 */

#define TRAMP_ENTRY		0
#define TRAMP_MODULEP		8
#define TRAMP_MEMMAP_SRC	16
#define TRAMP_MEMMAP_DST	24
#define TRAMP_MEMMAP_LEN	32
#define TRAMP_TOTAL		40

	.text
	.globl	tramp
tramp:
	adr	x8, trampoline_data
	ldr	x10, [x8, #TRAMP_MEMMAP_SRC]
	cmp	x10, xzr
	b.eq	9f

	/*
	 * Copy over the memory map into area we have reserved for it. Assume
	 * the copy is a multiple of 8, since we know table entries are made up
	 * of several 64-bit quantities.
	 */
	ldp	x11, x12, [x8, #TRAMP_MEMMAP_DST]	/* x12 = len */
1:
	ldr	x13, [x10], #8
	str	x13, [x11], #8
	subs	x12, x12, #8
	b.hi	1b
9:
	ldp	x9, x0, [x8, #TRAMP_ENTRY]		/* x0 = modulep */
	br	x9

	.p2align 4
trampoline_data:
	.space TRAMP_TOTAL
#define TMPSTACKSIZE	48	/* 16 bytes for args +8 for pushq/popfq + 24 spare */
	.space	TMPSTACKSIZE
tramp_end:			/* padding doubles as stack */

	.data
	.globl	tramp_size
tramp_size:
	.long	tramp_end-tramp
	.globl	tramp_data_offset
tramp_data_offset:
	.long	trampoline_data-tramp
