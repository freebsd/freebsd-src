/*-
 * Copyright (c) 2010 George V. Neville-Neil <gnn@freebsd.org>
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

#ifndef _DEV_HWPMC_MIPS24K_H_
#define _DEV_HWPMC_MIPS24K_H_

#define	MIPS24K_PMC_CAPS	(PMC_CAP_INTERRUPT | PMC_CAP_USER |     \
				 PMC_CAP_SYSTEM | PMC_CAP_EDGE |	\
				 PMC_CAP_THRESHOLD | PMC_CAP_READ |	\
				 PMC_CAP_WRITE | PMC_CAP_INVERT |	\
				 PMC_CAP_QUALIFIER)

#define MIPS24K_PMC_INTERRUPT_ENABLE      0x10 /* Enable interrupts */
#define MIPS24K_PMC_USER_ENABLE           0x08 /* Count in USER mode */
#define MIPS24K_PMC_SUPER_ENABLE          0x04 /* Count in SUPERVISOR mode */
#define MIPS24K_PMC_KERNEL_ENABLE         0x02 /* Count in KERNEL mode */
#define MIPS24K_PMC_ENABLE (MIPS24K_PMC_USER_ENABLE |	   \
			    MIPS24K_PMC_SUPER_ENABLE |	   \
			    MIPS24K_PMC_KERNEL_ENABLE)

/*
 * Interrupts are posted when bit 31 of the relevant
 * counter is set.
 */
#define	MIPS24K_RELOAD_COUNT_TO_PERFCTR_VALUE(R)	(0x80000000 - (R))
#define	MIPS24K_PERFCTR_VALUE_TO_RELOAD_COUNT(P)	((P) - 0x80000000)

#define MIPS24K_PMC_SELECT 0x4 /* Which bit position the event starts at. */
#define MIPS24K_PMC_OFFSET 2   /* Control registers are 0, 2, 4, etc. */
#define MIPS24K_PMC_MORE 0x800000 /* Test for more PMCs (bit 31) */

#ifdef _KERNEL
/* MD extension for 'struct pmc' */
struct pmc_md_mips24k_pmc {
	uint32_t	pm_mips24k_evsel;
};
#endif /* _KERNEL */

#endif /* _DEV_HWPMC_MIPS_H_ */
