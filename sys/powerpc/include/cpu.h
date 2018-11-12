/*-
 * Copyright (C) 1995-1997 Wolfgang Solfrank.
 * Copyright (C) 1995-1997 TooLs GmbH.
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
 *	$NetBSD: cpu.h,v 1.11 2000/05/26 21:19:53 thorpej Exp $
 * $FreeBSD$
 */

#ifndef _MACHINE_CPU_H_
#define	_MACHINE_CPU_H_

#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/psl.h>

/*
 * CPU Feature Attributes
 *
 * These are defined in the PowerPC ELF ABI for the AT_HWCAP vector,
 * and are exported to userland via the machdep.cpu_features
 * sysctl.
 */

extern int cpu_features;
extern int cpu_features2;

#define	PPC_FEATURE_32		0x80000000	/* Always true */
#define	PPC_FEATURE_64		0x40000000	/* Defined on a 64-bit CPU */
#define	PPC_FEATURE_HAS_ALTIVEC	0x10000000	
#define	PPC_FEATURE_HAS_FPU	0x08000000
#define	PPC_FEATURE_HAS_MMU	0x04000000
#define	PPC_FEATURE_UNIFIED_CACHE 0x01000000
#define	PPC_FEATURE_BOOKE	0x00008000
#define	PPC_FEATURE_SMT		0x00004000
#define	PPC_FEATURE_ARCH_2_05	0x00001000
#define	PPC_FEATURE_HAS_DFP	0x00000400
#define	PPC_FEATURE_ARCH_2_06	0x00000100
#define	PPC_FEATURE_HAS_VSX	0x00000080

#define	PPC_FEATURE2_ARCH_2_07	0x80000000
#define	PPC_FEATURE2_HAS_HTM	0x40000000
#define	PPC_FEATURE2_HAS_VCRYPTO 0x02000000

#define	PPC_FEATURE_BITMASK						\
	"\20"								\
	"\040PPC32\037PPC64\035ALTIVEC\034FPU\033MMU\031UNIFIEDCACHE"	\
	"\020BOOKE\017SMT\015ARCH205\013DFP\011ARCH206\010VSX"
#define	PPC_FEATURE2_BITMASK						\
	"\20"								\
	"\040ARCH207\037HTM\032VCRYPTO"

#define	TRAPF_USERMODE(frame)	(((frame)->srr1 & PSL_PR) != 0)
#define	TRAPF_PC(frame)		((frame)->srr0)

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CACHELINE	1

static __inline u_int64_t
get_cyclecount(void)
{
	u_int32_t _upper, _lower;
	u_int64_t _time;

	__asm __volatile(
		"mftb %0\n"
		"mftbu %1"
		: "=r" (_lower), "=r" (_upper));

	_time = (u_int64_t)_upper;
	_time = (_time << 32) + _lower;
	return (_time);
}

#define	cpu_getstack(td)	((td)->td_frame->fixreg[1])
#define	cpu_spinwait()		__asm __volatile("or 27,27,27") /* yield */

extern char btext[];
extern char etext[];

void	cpu_halt(void);
void	cpu_reset(void);
void	cpu_sleep(void);
void	flush_disable_caches(void);
void	fork_trampoline(void);
void	swi_vm(void *);

#endif	/* _MACHINE_CPU_H_ */
