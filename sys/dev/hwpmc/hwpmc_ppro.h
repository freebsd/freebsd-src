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
 * $FreeBSD: src/sys/dev/hwpmc/hwpmc_ppro.h,v 1.1 2005/06/09 19:45:07 jkoshy Exp $
 */

/* Machine dependent interfaces */

#ifndef _DEV_HWPMC_PPRO_H_
#define	_DEV_HWPMC_PPRO_H_

/* Intel PPro, Celeron, P-II, P-III, Pentium-M PMCS */

#define	P6_NPMCS	3		/* 1 TSC + 2 PMCs */

#define	P6_EVSEL_CMASK_MASK		0xFF000000
#define	P6_EVSEL_TO_CMASK(C)		(((C) & 0xFF) << 24)
#define	P6_EVSEL_INV			(1 << 23)
#define	P6_EVSEL_EN			(1 << 22)
#define	P6_EVSEL_INT			(1 << 20)
#define	P6_EVSEL_PC			(1 << 19)
#define	P6_EVSEL_E			(1 << 18)
#define	P6_EVSEL_OS			(1 << 17)
#define	P6_EVSEL_USR			(1 << 16)
#define	P6_EVSEL_UMASK_MASK		0x0000FF00
#define	P6_EVSEL_TO_UMASK(U)		(((U) & 0xFF) << 8)
#define	P6_EVSEL_EVENT_SELECT(ES)	((ES) & 0xFF)
#define	P6_EVSEL_RESERVED		(1 << 21)

#define	P6_MSR_EVSEL0			0x0186
#define	P6_MSR_EVSEL1			0x0187
#define	P6_MSR_PERFCTR0			0x00C1
#define	P6_MSR_PERFCTR1			0x00C2

#define	P6_PERFCTR_READ_MASK		0xFFFFFFFFFFLL	/* 40 bits */
#define	P6_PERFCTR_WRITE_MASK		0xFFFFFFFFU	/* 32 bits */

#define	P6_RELOAD_COUNT_TO_PERFCTR_VALUE(R)	(-(R))
#define	P6_PERFCTR_VALUE_TO_RELOAD_COUNT(P)	(-(P))

#define	P6_PMC_HAS_OVERFLOWED(P)	((rdpmc(P) & (1LL << 39)) == 0)

struct pmc_md_ppro_op_pmcallocate {
	uint32_t	pm_ppro_config;
};

#ifdef _KERNEL

/* MD extension for 'struct pmc' */
struct pmc_md_ppro_pmc {
	uint32_t	pm_ppro_evsel;
};

/*
 * Prototypes
 */

int	pmc_initialize_p6(struct pmc_mdep *);		/* Pentium Pro PMCs */

#endif /* _KERNEL */
#endif /* _DEV_HWPMC_PPRO_H_ */
