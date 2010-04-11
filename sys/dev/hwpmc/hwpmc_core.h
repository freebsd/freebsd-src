/*-
 * Copyright (c) 2008 Joseph Koshy
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

#ifndef _DEV_HWPMC_CORE_H_
#define	_DEV_HWPMC_CORE_H_ 1

/*
 * Fixed-function PMCs.
 */
struct pmc_md_iaf_op_pmcallocate {
	uint16_t	pm_iaf_flags;	/* additional flags */
};

#define	IAF_OS		0x1
#define	IAF_USR		0x2
#define	IAF_ANY		0x4
#define	IAF_PMI		0x8

/*
 * Programmable PMCs.
 */
struct pmc_md_iap_op_pmcallocate {
	uint32_t	pm_iap_config;
	uint32_t	pm_iap_rsp;
};

#define	IAP_EVSEL(C)	((C) & 0xFF)
#define	IAP_UMASK(C)	((C) & 0xFF00)
#define	IAP_USR		(1 << 16)
#define	IAP_OS		(1 << 17)
#define	IAP_EDGE	(1 << 18)
#define	IAP_INT		(1 << 20)
#define	IAP_ANY		(1 << 21)
#define	IAP_EN		(1 << 22)
#define	IAP_INV		(1 << 23)
#define	IAP_CMASK(C)	(((C) & 0xFF) << 24)

#define	IA_OFFCORE_RSP_MASK	0xF7FF

#ifdef	_KERNEL

/*
 * Fixed-function counters.
 */
#define	IAF_MASK				0xF

#define	IAF_CTR0				0x309
#define	IAF_CTR1				0x30A
#define	IAF_CTR2				0x30B

#define	IAF_OFFSET				32
#define	IAF_CTRL				0x38D

/*
 * Programmable counters.
 */

#define	IAP_PMC0				0x0C1
#define	IAP_EVSEL0				0x186

/*
 * Simplified programming interface in Intel Performance Architecture
 * v2 and later.
 */

#define	IA_GLOBAL_STATUS			0x38E
#define	IA_GLOBAL_CTRL				0x38F
#define	IA_GLOBAL_OVF_CTRL			0x390

#define	IA_GLOBAL_STATUS_FLAG_CONDCHG		(1ULL << 63)
#define	IA_GLOBAL_STATUS_FLAG_OVFBUF		(1ULL << 62)

/*
 * Offcore response configuration.
 */
#define	IA_OFFCORE_RSP0				0x1A6
#define	IA_OFFCORE_RSP1				0x1A7

struct pmc_md_iaf_pmc {
	uint64_t	pm_iaf_ctrl;
};

struct pmc_md_iap_pmc {
	uint32_t	pm_iap_evsel;
	uint32_t	pm_iap_rsp;
};

/*
 * Prototypes.
 */

int	pmc_core_initialize(struct pmc_mdep *_md, int _maxcpu);
void	pmc_core_finalize(struct pmc_mdep *_md);

void	pmc_core_mark_started(int _cpu, int _pmc);

int	pmc_iaf_initialize(struct pmc_mdep *_md, int _maxcpu, int _npmc, int _width);
void	pmc_iaf_finalize(struct pmc_mdep *_md);

int	pmc_iap_initialize(struct pmc_mdep *_md, int _maxcpu, int _npmc, int _width,
	    int _flags);
void	pmc_iap_finalize(struct pmc_mdep *_md);

#endif	/* _KERNEL */
#endif	/* _DEV_HWPMC_CORE_H */
