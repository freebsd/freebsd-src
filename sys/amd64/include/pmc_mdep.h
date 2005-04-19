/*-
 * Copyright (c) 2003, Joseph Koshy
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

#ifndef _MACHINE_PMC_MDEP_H
#define	_MACHINE_PMC_MDEP_H 1

#include <sys/pmc.h>

/* AMD K7 PMCs */

#define	K8_NPMCS		5 /* 1 TSC + 4 PMCs */

#define	K8_PMC_COUNTERMASK	0xFF000000
#define	K8_PMC_TO_COUNTER(x)	(((x) << 24) & K8_PMC_COUNTERMASK)
#define	K8_PMC_INVERT		(1 << 23)
#define	K8_PMC_ENABLE		(1 << 22)
#define	K8_PMC_INT		(1 << 20)
#define	K8_PMC_PC		(1 << 19)
#define	K8_PMC_EDGE		(1 << 18)
#define	K8_PMC_OS		(1 << 17)
#define	K8_PMC_USR		(1 << 16)

#define	K8_PMC_UNITMASK_M	0x10
#define	K8_PMC_UNITMASK_O	0x08
#define	K8_PMC_UNITMASK_E	0x04
#define	K8_PMC_UNITMASK_S	0x02
#define	K8_PMC_UNITMASK_I	0x01
#define	K8_PMC_UNITMASK_MOESI	0x1F

#define	K8_PMC_UNITMASK		0xFF00
#define	K8_PMC_EVENTMASK 	0x00FF
#define	K8_PMC_TO_UNITMASK(x)	(((x) << 8) & K8_PMC_UNITMASK)
#define	K8_PMC_TO_EVENTMASK(x)	((x) & 0xFF)
#define	K8_VALID_BITS		(K8_PMC_COUNTERMASK | K8_PMC_INVERT |      \
	K8_PMC_ENABLE | K8_PMC_INT | K8_PMC_PC | K8_PMC_EDGE | K8_PMC_OS | \
	K8_PMC_USR | K8_PMC_UNITMASK | K8_PMC_EVENTMASK)

#ifdef _KERNEL

/*
 * Prototypes
 */

#if defined(__amd64__)
struct pmc_mdep *pmc_amd_initialize(void);
#endif /* defined(__i386__) */

#endif /* _KERNEL */
#endif /* _MACHINE_PMC_MDEP_H */
