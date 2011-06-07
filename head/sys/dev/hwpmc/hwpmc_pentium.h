/*-
 * Copyright (c) 2005, Joseph Koshy
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* Machine dependent interfaces */

#ifndef _DEV_HWPMC_PENTIUM_H_
#define	_DEV_HWPMC_PENTIUM_H_ 1

/* Intel Pentium PMCs */

#define	PENTIUM_NPMCS	2
#define	PENTIUM_CESR_PC1		(1 << 25)
#define	PENTIUM_CESR_CC1_MASK		0x01C00000
#define	PENTIUM_CESR_TO_CC1(C)		(((C) & 0x07) << 22)
#define	PENTIUM_CESR_ES1_MASK		0x003F0000
#define	PENTIUM_CESR_TO_ES1(E)		(((E) & 0x3F) << 16)
#define	PENTIUM_CESR_PC0		(1 << 9)
#define	PENTIUM_CESR_CC0_MASK		0x000001C0
#define	PENTIUM_CESR_TO_CC0(C)		(((C) & 0x07) << 6)
#define	PENTIUM_CESR_ES0_MASK		0x0000003F
#define	PENTIUM_CESR_TO_ES0(E)		((E) & 0x3F)
#define	PENTIUM_CESR_RESERVED		0xFC00FC00

#define	PENTIUM_MSR_CESR		0x11
#define	PENTIUM_MSR_CTR0		0x12
#define	PENTIUM_MSR_CTR1		0x13

struct pmc_md_pentium_op_pmcallocate {
	uint32_t	pm_pentium_config;
};

#ifdef _KERNEL

/* MD extension for 'struct pmc' */
struct pmc_md_pentium_pmc {
	uint32_t	pm_pentium_cesr;
};


/*
 * Prototypes
 */

int	pmc_p5_initialize(struct pmc_mdep *_md, int _ncpus);
void	pmc_p5_finalize(struct pmc_mdep *_md);

#endif /* _KERNEL */
#endif /* _DEV_HWPMC_PENTIUM_H_ */
