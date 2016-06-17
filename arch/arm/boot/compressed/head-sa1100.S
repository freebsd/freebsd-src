/* 
 * linux/arch/arm/boot/compressed/head-sa1100.S
 * 
 * Copyright (C) 1999 Nicolas Pitre <nico@cam.org>
 * 
 * SA1100 specific tweaks.  This is merged into head.S by the linker.
 *
 */

#include <linux/config.h>
#include <linux/linkage.h>
#include <asm/mach-types.h>

		.section        ".start", "ax"

__SA1100_start:

		@ Preserve r8/r7 i.e. kernel entry values

#if defined(CONFIG_SA1100_GRAPHICSCLIENT) && !defined(CONFIG_ANGELBOOT)
		mov	r7, #MACH_TYPE_GRAPHICSCLIENT
		mov	r8, #0
#endif
#if defined(CONFIG_SA1100_GRAPHICSMASTER) && !defined(CONFIG_ANGELBOOT)
		mov	r7, #MACH_TYPE_GRAPHICSMASTER
		mov	r8, #0
#endif
#if defined(CONFIG_SA1100_ADSBITSY) && !defined(CONFIG_ANGELBOOT)
		mov	r7, #MACH_TYPE_ADSBITSY
		mov	r8, #0
#endif

#ifdef CONFIG_SA1100_PFS168
		@ REVISIT_PFS168: Temporary until firmware updated to use assigned machine number
		mov	r7, #MACH_TYPE_PFS168
#endif

#ifdef CONFIG_SA1100_VICTOR
		teq	r7, #MACH_TYPE_VICTOR
		bne	10f

		@ Copy cmdline to 0xc0000000
		mov	r1, #0xc0000000
		cmp	r0, #0
		moveq	r2, #0
1:		ldrneb	r2, [r0], #1
		cmpne	r2, #0
		strb	r2, [r1], #1
		bne	1b
10:
#endif

		@ Data cache might be active.
		@ Be sure to flush kernel binary out of the cache,
		@ whatever state it is, before it is turned off.
		@ This is done by fetching through currently executed
		@ memory to be sure we hit the same cache.
		bic	r2, pc, #0x1f
		add	r3, r2, #0x4000		@ 16 kb is quite enough...
1:		ldr	r0, [r2], #32
		teq	r2, r3
		bne	1b
		mcr	p15, 0, r0, c7, c10, 4	@ drain WB
		mcr	p15, 0, r0, c7, c7, 0	@ flush I & D caches

		@ disabling MMU and caches
		mrc	p15, 0, r0, c1, c0, 0	@ read control reg
		bic	r0, r0, #0x0d		@ clear WB, DC, MMU
		bic	r0, r0, #0x1000		@ clear Icache
		mcr	p15, 0, r0, c1, c0, 0

/*
 * Pause for a short time so that we give enough time
 * for the host to start a terminal up.
 */
		mov	r0, #0x00200000
1:		subs	r0, r0, #1
		bne	1b

