/*	$NetBSD: cpuconf.h,v 1.8 2003/09/06 08:55:42 rearnsha Exp $	*/

/*-
 * Copyright (c) 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

#ifndef _MACHINE_CPUCONF_H_
#define	_MACHINE_CPUCONF_H_

/*
 * IF YOU CHANGE THIS FILE, MAKE SURE TO UPDATE THE DEFINITION OF
 * "PMAP_NEEDS_PTE_SYNC" IN <arm/arm32/pmap.h> FOR THE CPU TYPE
 * YOU ARE ADDING SUPPORT FOR.
 */

/*
 * Step 1: Count the number of CPU types configured into the kernel.
 */
#define	CPU_NTYPES	2

/*
 * Step 2: Determine which ARM architecture versions are configured.
 */

#if (defined(CPU_ARM7TDMI) || defined(CPU_ARM8) || defined(CPU_ARM9) ||	\
     defined(CPU_ARM10) || defined(CPU_SA110) || defined(CPU_SA1100) || \
     defined(CPU_SA1110) || defined(CPU_IXP12X0) || defined(CPU_XSCALE_IXP425))
#define	ARM_ARCH_4	1
#else
#define	ARM_ARCH_4	0
#endif

#if (defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) ||		\
     defined(CPU_XSCALE_PXA2X0))
#define	ARM_ARCH_5	1
#else
#define	ARM_ARCH_5	0
#endif

#define	ARM_NARCH	(ARM_ARCH_4 + ARM_ARCH_5)
#if ARM_NARCH == 0 && !defined(KLD_MODULE)
#error ARM_NARCH is 0
#endif

/*
 * Step 3: Define which MMU classes are configured:
 *
 *	ARM_MMU_MEMC		Prehistoric, external memory controller
 *				and MMU for ARMv2 CPUs.
 *
 *	ARM_MMU_GENERIC		Generic ARM MMU, compatible with ARM6.
 *
 *	ARM_MMU_SA1		StrongARM SA-1 MMU.  Compatible with generic
 *				ARM MMU, but has no write-through cache mode.
 *
 *	ARM_MMU_XSCALE		XScale MMU.  Compatible with generic ARM
 *				MMU, but also has several extensions which
 *				require different PTE layout to use.
 */
#if (defined(CPU_ARM2) || defined(CPU_ARM250) || defined(CPU_ARM3))
#define	ARM_MMU_MEMC		1
#else
#define	ARM_MMU_MEMC		0
#endif

#if (defined(CPU_ARM6) || defined(CPU_ARM7) || defined(CPU_ARM7TDMI) ||	\
     defined(CPU_ARM8) || defined(CPU_ARM9) || defined(CPU_ARM10))
#define	ARM_MMU_GENERIC		1
#else
#define	ARM_MMU_GENERIC		0
#endif

#if (defined(CPU_SA110) || defined(CPU_SA1100) || defined(CPU_SA1110) ||\
     defined(CPU_IXP12X0))
#define	ARM_MMU_SA1		1
#else
#define	ARM_MMU_SA1		0
#endif

#if(defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321) ||		\
     defined(CPU_XSCALE_PXA2X0) || defined(CPU_XSCALE_IXP425))
#define	ARM_MMU_XSCALE		1
#else
#define	ARM_MMU_XSCALE		0
#endif

#define	ARM_NMMUS		(ARM_MMU_MEMC + ARM_MMU_GENERIC +	\
				 ARM_MMU_SA1 + ARM_MMU_XSCALE)
#if ARM_NMMUS == 0 && !defined(KLD_MODULE)
#error ARM_NMMUS is 0
#endif

/*
 * Step 4: Define features that may be present on a subset of CPUs
 *
 *	ARM_XSCALE_PMU		Performance Monitoring Unit on 80200 and 80321
 */

#if (defined(CPU_XSCALE_80200) || defined(CPU_XSCALE_80321))
#define ARM_XSCALE_PMU	1
#else
#define ARM_XSCALE_PMU	0
#endif

#endif /* _MACHINE_CPUCONF_H_ */
