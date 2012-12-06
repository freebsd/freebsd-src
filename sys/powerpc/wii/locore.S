/*-
 * Copyright (C) 2012 Margarida Gouveia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <machine/bat.h>
/*
 * When we are invoked from Wii loaders, the state of the MMU and the BAT
 * mappings can vary.  In this file we try to reset the MMU to a state
 * that lets us boot FreeBSD.
 *
 * N.B.: keep the BAT0 in sync with mmu_oea.c and never touch BAT1 later.
 *
 * This file is being included from aim/locore32.S.
 */

#define	MMU_REALMODE()				\
	mfmsr	%r12;				\
	rlwinm	%r12, %r12, 0, ~(PSL_DR|PSL_IR);\
	sync;					\
	bl	1f;				\
1:						\
	mflr    %r11;				\
	clrlwi  %r11, %r11, 3;	/* XXX why? */	\
	addi    %r11, %r11, 2f - 1b;		\
	mtsrr0  %r11;				\
	mtsrr1	%r12;	/* Disables the MMU */	\
	isync;					\
	rfi;					\
2:

#define	MMU_VIRTUALMODE()			\
	bl	3f;				\
3:						\
	mflr	%r11;				\
	addi	%r11, %r11, 4f - 3b;		\
	mfmsr	%r12;				\
	ori	%r12, %r12, PSL_DR|PSL_IR;	\
	mtsrr0	%r11;				\
	mtsrr1	%r12;	/* Enables the MMU */	\
	isync;					\
	rfi;					\
4:

	MMU_REALMODE()

	/* Reset standard BATs */
	li	%r11, 0
	mtibatu 0, %r11
	mtibatl 0, %r11
	mtdbatu 0, %r11
	mtdbatl 0, %r11
	mtibatu 1, %r11
	mtibatl 1, %r11
	mtdbatu 1, %r11
	mtdbatl 1, %r11
	mtibatu 2, %r11
	mtibatl 2, %r11
	mtdbatu 2, %r11
	mtdbatl 2, %r11
	mtibatu 3, %r11
	mtibatl 3, %r11
	mtdbatu 3, %r11
	mtdbatl 3, %r11

	/* Reset high BATs. IBAT[4-7][UL] + DBAT[4-7][UL] */
	mtspr	560, %r11
	mtspr	561, %r11
	mtspr	562, %r11
	mtspr	563, %r11
	mtspr	564, %r11
	mtspr	565, %r11
	mtspr	566, %r11
	mtspr	567, %r11
	mtspr	568, %r11
	mtspr	569, %r11
	mtspr	570, %r11
	mtspr	571, %r11
	mtspr	572, %r11
	mtspr	573, %r11
	mtspr	574, %r11
	mtspr	575, %r11

	/*
	 * We need to setup BAT0 as in mmu_oea.c.
	 */
	li	%r11, BATU(0x00000000, BAT_BL_256M, BAT_Vs)
	li	%r12, BATL(0x00000000, BAT_M, BAT_PP_RW)
	mtdbatu	0, %r11
	mtdbatl	0, %r12
	mtibatu	0, %r11
	mtibatl	0, %r12
	isync

	/*
	 * We use BAT1 to be able to write I/O memory, including the
	 * framebuffer registers.
	 */
	/* BATU(0x0c000000, BAT_BL_32M, BAT_Vs) */
	lis	%r11, 0x0c00
	ori	%r11, %r11, BAT_BL_32M|BAT_Vs
	/* BATL(0x0c000000, BAT_I|BAT_G, BAT_PP_RW) */
	lis	%r12, 0x0c00
	ori	%r12, %r12, BAT_I|BAT_G|BAT_PP_RW
	mtdbatu	1, %r11
	mtdbatl	1, %r12
	isync

	MMU_VIRTUALMODE()
