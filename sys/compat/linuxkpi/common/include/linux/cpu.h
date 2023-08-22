/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2021 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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
 */

#ifndef	_LINUXKPI_LINUX_CPU_H
#define	_LINUXKPI_LINUX_CPU_H

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cpuset.h>
#include <sys/smp.h>
#include <linux/compiler.h>
#include <linux/slab.h>

typedef	cpuset_t	cpumask_t;

extern cpumask_t cpu_online_mask;

cpumask_t *lkpi_get_static_single_cpu_mask(int);

static __inline int
cpumask_next(int cpuid, cpumask_t mask)
{

	/*
	 * -1 can be an input to cpuid according to logic in drivers
	 * but is never a valid cpuid in a set!
	 */
	KASSERT((cpuid >= -1 && cpuid <= MAXCPU), ("%s: invalid cpuid %d\n",
	    __func__, cpuid));
	KASSERT(!CPU_EMPTY(&mask), ("%s: empty CPU mask", __func__));

	do {
		cpuid++;
#ifdef SMP
		if (cpuid > mp_maxid)
#endif
			cpuid = 0;
	} while (!CPU_ISSET(cpuid, &mask));
	return (cpuid);
}

static __inline void
cpumask_set_cpu(int cpu, cpumask_t *mask)
{

	CPU_SET(cpu, mask);
}

#define	cpumask_of(_cpu)	(lkpi_get_static_single_cpu_mask(_cpu))

#endif	/* _LINUXKPI_LINUX_CPU_H */
