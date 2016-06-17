/*
 * asmmacro.h: Assembler macros to make things easier to read.
 *
 * Copyright (C) 1996 David S. Miller (dm@engr.sgi.com)
 * Copyright (C) 1998, 1999 Ralf Baechle
 * Copyright (C) 1999 Silicon Graphics, Inc.
 */
#ifndef _ASM_ASMMACRO_H
#define _ASM_ASMMACRO_H

#include <linux/config.h>
#include <asm/offset.h>

#ifdef CONFIG_CPU_SB1
#define FPU_ENABLE_HAZARD		\
	.set    push;			\
	.set	noreorder;		\
	.set    mips2;			\
	SSNOP;				\
	bnezl   $0, .+4;		\
	 SSNOP;				\
	.set    pop
#else
#define FPU_ENABLE_HAZARD
#endif

	.macro	fpu_save_16even thread tmp
	cfc1	\tmp, fcr31
	sdc1	$f2,  (THREAD_FPU + 0x010)(\thread)
	sdc1	$f4,  (THREAD_FPU + 0x020)(\thread)
	sdc1	$f6,  (THREAD_FPU + 0x030)(\thread)
	sdc1	$f8,  (THREAD_FPU + 0x040)(\thread)
	sdc1	$f10, (THREAD_FPU + 0x050)(\thread)
	sdc1	$f12, (THREAD_FPU + 0x060)(\thread)
	sdc1	$f14, (THREAD_FPU + 0x070)(\thread)
	sdc1	$f16, (THREAD_FPU + 0x080)(\thread)
	sdc1	$f18, (THREAD_FPU + 0x090)(\thread)
	sdc1	$f20, (THREAD_FPU + 0x0a0)(\thread)
	sdc1	$f22, (THREAD_FPU + 0x0b0)(\thread)
	sdc1	$f24, (THREAD_FPU + 0x0c0)(\thread)
	sdc1	$f26, (THREAD_FPU + 0x0d0)(\thread)
	sdc1	$f28, (THREAD_FPU + 0x0e0)(\thread)
	sdc1	$f30, (THREAD_FPU + 0x0f0)(\thread)
	sw	\tmp, (THREAD_FPU + 0x100)(\thread)
	.endm

	.macro	fpu_save_16odd thread
	sdc1	$f1,  (THREAD_FPU + 0x08)(\thread)
	sdc1	$f3,  (THREAD_FPU + 0x18)(\thread)
	sdc1	$f5,  (THREAD_FPU + 0x28)(\thread)
	sdc1	$f7,  (THREAD_FPU + 0x38)(\thread)
	sdc1	$f9,  (THREAD_FPU + 0x48)(\thread)
	sdc1	$f11, (THREAD_FPU + 0x58)(\thread)
	sdc1	$f13, (THREAD_FPU + 0x68)(\thread)
	sdc1	$f15, (THREAD_FPU + 0x78)(\thread)
	sdc1	$f17, (THREAD_FPU + 0x88)(\thread)
	sdc1	$f19, (THREAD_FPU + 0x98)(\thread)
	sdc1	$f21, (THREAD_FPU + 0xa8)(\thread)
	sdc1	$f23, (THREAD_FPU + 0xb8)(\thread)
	sdc1	$f25, (THREAD_FPU + 0xc8)(\thread)
	sdc1	$f27, (THREAD_FPU + 0xd8)(\thread)
	sdc1	$f29, (THREAD_FPU + 0xe8)(\thread)
	sdc1	$f31, (THREAD_FPU + 0xf8)(\thread)
	.endm

	.macro	fpu_save thread tmp
	cfc1	\tmp,  fcr31
	swc1	$f0,  (THREAD_FPU + 0x000)(\thread)
	swc1	$f1,  (THREAD_FPU + 0x008)(\thread)
	swc1	$f2,  (THREAD_FPU + 0x010)(\thread)
	swc1	$f3,  (THREAD_FPU + 0x018)(\thread)
	swc1	$f4,  (THREAD_FPU + 0x020)(\thread)
	swc1	$f5,  (THREAD_FPU + 0x028)(\thread)
	swc1	$f6,  (THREAD_FPU + 0x030)(\thread)
	swc1	$f7,  (THREAD_FPU + 0x038)(\thread)
	swc1	$f8,  (THREAD_FPU + 0x040)(\thread)
	swc1	$f9,  (THREAD_FPU + 0x048)(\thread)
	swc1	$f10, (THREAD_FPU + 0x050)(\thread)
	swc1	$f11, (THREAD_FPU + 0x058)(\thread)
	swc1	$f12, (THREAD_FPU + 0x060)(\thread)
	swc1	$f13, (THREAD_FPU + 0x068)(\thread)
	swc1	$f14, (THREAD_FPU + 0x070)(\thread)
	swc1	$f15, (THREAD_FPU + 0x078)(\thread)
	swc1	$f16, (THREAD_FPU + 0x080)(\thread)
	swc1	$f17, (THREAD_FPU + 0x088)(\thread)
	swc1	$f18, (THREAD_FPU + 0x090)(\thread)
	swc1	$f19, (THREAD_FPU + 0x098)(\thread)
	swc1	$f20, (THREAD_FPU + 0x0a0)(\thread)
	swc1	$f21, (THREAD_FPU + 0x0a8)(\thread)
	swc1	$f22, (THREAD_FPU + 0x0b0)(\thread)
	swc1	$f23, (THREAD_FPU + 0x0b8)(\thread)
	swc1	$f24, (THREAD_FPU + 0x0c0)(\thread)
	swc1	$f25, (THREAD_FPU + 0x0c8)(\thread)
	swc1	$f26, (THREAD_FPU + 0x0d0)(\thread)
	swc1	$f27, (THREAD_FPU + 0x0d8)(\thread)
	swc1	$f28, (THREAD_FPU + 0x0e0)(\thread)
	swc1	$f29, (THREAD_FPU + 0x0e8)(\thread)
	swc1	$f30, (THREAD_FPU + 0x0f0)(\thread)
	swc1	$f31, (THREAD_FPU + 0x0f8)(\thread)
	sw	\tmp, (THREAD_FPU + 0x100)(\thread)
	.endm

	.macro	fpu_restore_16even thread tmp
	lw	\tmp, (THREAD_FPU + 0x100)(\thread)
	ldc1	$f2,  (THREAD_FPU + 0x010)(\thread)
	ldc1	$f4,  (THREAD_FPU + 0x020)(\thread)
	ldc1	$f6,  (THREAD_FPU + 0x030)(\thread)
	ldc1	$f8,  (THREAD_FPU + 0x040)(\thread)
	ldc1	$f10, (THREAD_FPU + 0x050)(\thread)
	ldc1	$f12, (THREAD_FPU + 0x060)(\thread)
	ldc1	$f14, (THREAD_FPU + 0x070)(\thread)
	ldc1	$f16, (THREAD_FPU + 0x080)(\thread)
	ldc1	$f18, (THREAD_FPU + 0x090)(\thread)
	ldc1	$f20, (THREAD_FPU + 0x0a0)(\thread)
	ldc1	$f22, (THREAD_FPU + 0x0b0)(\thread)
	ldc1	$f24, (THREAD_FPU + 0x0c0)(\thread)
	ldc1	$f26, (THREAD_FPU + 0x0d0)(\thread)
	ldc1	$f28, (THREAD_FPU + 0x0e0)(\thread)
	ldc1	$f30, (THREAD_FPU + 0x0f0)(\thread)
	ctc1	\tmp,  fcr31
	.endm

	.macro	fpu_restore_16odd thread
	ldc1	$f1,  (THREAD_FPU + 0x08)(\thread)
	ldc1	$f3,  (THREAD_FPU + 0x18)(\thread)
	ldc1	$f5,  (THREAD_FPU + 0x28)(\thread)
	ldc1	$f7,  (THREAD_FPU + 0x38)(\thread)
	ldc1	$f9,  (THREAD_FPU + 0x48)(\thread)
	ldc1	$f11, (THREAD_FPU + 0x58)(\thread)
	ldc1	$f13, (THREAD_FPU + 0x68)(\thread)
	ldc1	$f15, (THREAD_FPU + 0x78)(\thread)
	ldc1	$f17, (THREAD_FPU + 0x88)(\thread)
	ldc1	$f19, (THREAD_FPU + 0x98)(\thread)
	ldc1	$f21, (THREAD_FPU + 0xa8)(\thread)
	ldc1	$f23, (THREAD_FPU + 0xb8)(\thread)
	ldc1	$f25, (THREAD_FPU + 0xc8)(\thread)
	ldc1	$f27, (THREAD_FPU + 0xd8)(\thread)
	ldc1	$f29, (THREAD_FPU + 0xe8)(\thread)
	ldc1	$f31, (THREAD_FPU + 0xf8)(\thread)
	.endm

	.macro	fpu_restore thread tmp
	lw	\tmp, (THREAD_FPU + 0x100)(\thread)
	lwc1	$f0,  (THREAD_FPU + 0x000)(\thread)
	lwc1	$f1,  (THREAD_FPU + 0x008)(\thread)
	lwc1	$f2,  (THREAD_FPU + 0x010)(\thread)
	lwc1	$f3,  (THREAD_FPU + 0x018)(\thread)
	lwc1	$f4,  (THREAD_FPU + 0x020)(\thread)
	lwc1	$f5,  (THREAD_FPU + 0x028)(\thread)
	lwc1	$f6,  (THREAD_FPU + 0x030)(\thread)
	lwc1	$f7,  (THREAD_FPU + 0x038)(\thread)
	lwc1	$f8,  (THREAD_FPU + 0x040)(\thread)
	lwc1	$f9,  (THREAD_FPU + 0x048)(\thread)
	lwc1	$f10, (THREAD_FPU + 0x050)(\thread)
	lwc1	$f11, (THREAD_FPU + 0x058)(\thread)
	lwc1	$f12, (THREAD_FPU + 0x060)(\thread)
	lwc1	$f13, (THREAD_FPU + 0x068)(\thread)
	lwc1	$f14, (THREAD_FPU + 0x070)(\thread)
	lwc1	$f15, (THREAD_FPU + 0x078)(\thread)
	lwc1	$f16, (THREAD_FPU + 0x080)(\thread)
	lwc1	$f17, (THREAD_FPU + 0x088)(\thread)
	lwc1	$f18, (THREAD_FPU + 0x090)(\thread)
	lwc1	$f19, (THREAD_FPU + 0x098)(\thread)
	lwc1	$f20, (THREAD_FPU + 0x0a0)(\thread)
	lwc1	$f21, (THREAD_FPU + 0x0a8)(\thread)
	lwc1	$f22, (THREAD_FPU + 0x0b0)(\thread)
	lwc1	$f23, (THREAD_FPU + 0x0b8)(\thread)
	lwc1	$f24, (THREAD_FPU + 0x0c0)(\thread)
	lwc1	$f25, (THREAD_FPU + 0x0c8)(\thread)
	lwc1	$f26, (THREAD_FPU + 0x0d0)(\thread)
	lwc1	$f27, (THREAD_FPU + 0x0d8)(\thread)
	lwc1	$f28, (THREAD_FPU + 0x0e0)(\thread)
	lwc1	$f29, (THREAD_FPU + 0x0e8)(\thread)
	lwc1	$f30, (THREAD_FPU + 0x0f0)(\thread)
	lwc1	$f31, (THREAD_FPU + 0x0f8)(\thread)
	ctc1	\tmp,  fcr31
	.endm

	.macro	cpu_save_nonscratch thread
	sd	s0, THREAD_REG16(\thread)
	sd	s1, THREAD_REG17(\thread)
	sd	s2, THREAD_REG18(\thread)
	sd	s3, THREAD_REG19(\thread)
	sd	s4, THREAD_REG20(\thread)
	sd	s5, THREAD_REG21(\thread)
	sd	s6, THREAD_REG22(\thread)
	sd	s7, THREAD_REG23(\thread)
	sd	sp, THREAD_REG29(\thread)
	sd	fp, THREAD_REG30(\thread)
	.endm

	.macro	cpu_restore_nonscratch thread
	ld	s0, THREAD_REG16(\thread)
	ld	s1, THREAD_REG17(\thread)
	ld	s2, THREAD_REG18(\thread)
	ld	s3, THREAD_REG19(\thread)
	ld	s4, THREAD_REG20(\thread)
	ld	s5, THREAD_REG21(\thread)
	ld	s6, THREAD_REG22(\thread)
	ld	s7, THREAD_REG23(\thread)
	ld	sp, THREAD_REG29(\thread)
	ld	fp, THREAD_REG30(\thread)
	ld	ra, THREAD_REG31(\thread)
	.endm

#endif /* !(_ASM_ASMMACRO_H) */
