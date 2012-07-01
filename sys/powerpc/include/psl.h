/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
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
 *	$NetBSD: psl.h,v 1.5 2000/11/19 19:52:37 matt Exp $
 * $FreeBSD$
 */

#ifndef	_MACHINE_PSL_H_
#define	_MACHINE_PSL_H_

#if defined(BOOKE_E500)
/*
 * Machine State Register (MSR) - e500 core
 *
 * The PowerPC e500 does not implement the following bits:
 *
 * FP, FE0, FE1 - reserved, always cleared, setting has no effect.
 *
 */
#define PSL_UCLE	0x04000000UL	/* User mode cache lock enable */
#define PSL_SPE		0x02000000UL	/* SPE enable */
#define PSL_WE		0x00040000UL	/* Wait state enable */
#define PSL_CE		0x00020000UL	/* Critical interrupt enable */
#define PSL_EE		0x00008000UL	/* External interrupt enable */
#define PSL_PR		0x00004000UL	/* User mode */
#define PSL_FP		0x00002000UL	/* Floating point available */
#define PSL_ME		0x00001000UL	/* Machine check interrupt enable */
#define PSL_FE0		0x00000800UL	/* Floating point exception mode 0 */
#define PSL_UBLE	0x00000400UL	/* BTB lock enable */
#define PSL_DE		0x00000200UL	/* Debug interrupt enable */
#define PSL_FE1		0x00000100UL	/* Floating point exception mode 1 */
#define PSL_IS		0x00000020UL	/* Instruction address space */
#define PSL_DS		0x00000010UL	/* Data address space */
#define PSL_PMM		0x00000004UL	/* Performance monitor mark */

#define PSL_FE_DFLT	0x00000000UL	/* default == none */

/* Initial kernel MSR, use IS=1 ad DS=1. */
#define PSL_KERNSET_INIT	(PSL_IS | PSL_DS)
#define PSL_KERNSET		(PSL_CE | PSL_ME | PSL_EE)
#define PSL_USERSET		(PSL_KERNSET | PSL_PR)

#elif defined(BOOKE_PPC4XX)
/*
 * Machine State Register (MSR) - PPC4xx core
 */
#define PSL_WE		(0x80000000 >> 13) /* Wait State Enable */
#define PSL_CE		(0x80000000 >> 14) /* Critical Interrupt Enable */
#define PSL_EE		(0x80000000 >> 16) /* External Interrupt Enable */
#define PSL_PR		(0x80000000 >> 17) /* Problem State */
#define PSL_FP		(0x80000000 >> 18) /* Floating Point Available */
#define PSL_ME		(0x80000000 >> 19) /* Machine Check Enable */
#define PSL_FE0		(0x80000000 >> 20) /* Floating-point exception mode 0 */
#define PSL_DWE		(0x80000000 >> 21) /* Debug Wait Enable */
#define PSL_DE		(0x80000000 >> 22) /* Debug interrupt Enable */
#define PSL_FE1		(0x80000000 >> 23) /* Floating-point exception mode 1 */
#define PSL_IS		(0x80000000 >> 26) /* Instruction Address Space */
#define PSL_DS		(0x80000000 >> 27) /* Data Address Space */

#define PSL_KERNSET	(PSL_CE | PSL_ME | PSL_EE | PSL_FP)
#define PSL_USERSET	(PSL_KERNSET | PSL_PR)

#define PSL_FE_DFLT	0x00000000UL	/* default == none */

#else	/* if defined(BOOKE_*) */
/*
 * Machine State Register (MSR)
 *
 * The PowerPC 601 does not implement the following bits:
 *
 *	VEC, POW, ILE, BE, RI, LE[*]
 *
 * [*] Little-endian mode on the 601 is implemented in the HID0 register.
 */

#ifdef __powerpc64__
#define PSL_SF		0x8000000000000000UL	/* 64-bit addressing */
#define PSL_HV		0x1000000000000000UL	/* hyper-privileged mode */
#endif

#define	PSL_VEC		0x02000000UL	/* AltiVec vector unit available */
#define	PSL_POW		0x00040000UL	/* power management */
#define	PSL_ILE		0x00010000UL	/* interrupt endian mode (1 == le) */
#define	PSL_EE		0x00008000UL	/* external interrupt enable */
#define	PSL_PR		0x00004000UL	/* privilege mode (1 == user) */
#define	PSL_FP		0x00002000UL	/* floating point enable */
#define	PSL_ME		0x00001000UL	/* machine check enable */
#define	PSL_FE0		0x00000800UL	/* floating point interrupt mode 0 */
#define	PSL_SE		0x00000400UL	/* single-step trace enable */
#define	PSL_BE		0x00000200UL	/* branch trace enable */
#define	PSL_FE1		0x00000100UL	/* floating point interrupt mode 1 */
#define	PSL_IP		0x00000040UL	/* interrupt prefix */
#define	PSL_IR		0x00000020UL	/* instruction address relocation */
#define	PSL_DR		0x00000010UL	/* data address relocation */
#define	PSL_PMM		0x00000004UL	/* performance monitor mark */
#define	PSL_RI		0x00000002UL	/* recoverable interrupt */
#define	PSL_LE		0x00000001UL	/* endian mode (1 == le) */

#define	PSL_601_MASK	~(PSL_POW|PSL_ILE|PSL_BE|PSL_RI|PSL_LE)

/*
 * Floating-point exception modes:
 */
#define	PSL_FE_DIS	0		/* none */
#define	PSL_FE_NONREC	PSL_FE1		/* imprecise non-recoverable */
#define	PSL_FE_REC	PSL_FE0		/* imprecise recoverable */
#define	PSL_FE_PREC	(PSL_FE0 | PSL_FE1) /* precise */
#define	PSL_FE_DFLT	PSL_FE_DIS	/* default == none */

/*
 * Note that PSL_POW and PSL_ILE are not in the saved copy of the MSR
 */
#define	PSL_MBO		0
#define	PSL_MBZ		0

#ifdef __powerpc64__
#define	PSL_KERNSET	(PSL_SF | PSL_EE | PSL_ME | PSL_IR | PSL_DR | PSL_RI)
#else
#define	PSL_KERNSET	(PSL_EE | PSL_ME | PSL_IR | PSL_DR | PSL_RI)
#endif
#define	PSL_USERSET	(PSL_KERNSET | PSL_PR)

#define	PSL_USERSTATIC	(PSL_USERSET | PSL_IP | 0x87c0008c)

#endif	/* if defined(BOOKE_E500) */
#endif	/* _MACHINE_PSL_H_ */
