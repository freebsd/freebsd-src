/*-
 * Copyright (c) 2003-2007 Joseph Koshy
 * Copyright (c) 2007 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by A. Joseph Koshy under
 * sponsorship from the FreeBSD Foundation and Google, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_hwpmc_hooks.h"

#include <sys/types.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/smp.h>

#ifdef	HWPMC_HOOKS
#define	PMC_KERNEL_VERSION	PMC_VERSION
#else
#define	PMC_KERNEL_VERSION	0
#endif

const int pmc_kernel_version = PMC_KERNEL_VERSION;

/* Hook variable. */
int (*pmc_hook)(struct thread *td, int function, void *arg) = NULL;

/* Interrupt handler */
int (*pmc_intr)(int cpu, struct trapframe *tf) = NULL;

/* Bitmask of CPUs requiring servicing at hardclock time */
volatile cpumask_t pmc_cpumask;

/*
 * A global count of SS mode PMCs.  When non-zero, this means that
 * we have processes that are sampling the system as a whole.
 */
volatile int pmc_ss_count;

/*
 * Since PMC(4) may not be loaded in the current kernel, the
 * convention followed is that a non-NULL value of 'pmc_hook' implies
 * the presence of this kernel module.
 *
 * This requires us to protect 'pmc_hook' with a
 * shared (sx) lock -- thus making the process of calling into PMC(4)
 * somewhat more expensive than a simple 'if' check and indirect call.
 */
struct sx pmc_sx;

static void
pmc_init_sx(void)
{
	sx_init_flags(&pmc_sx, "pmc-sx", SX_NOWITNESS);
}

SYSINIT(pmcsx, SI_SUB_LOCK, SI_ORDER_MIDDLE, pmc_init_sx, NULL);

/*
 * Helper functions
 */

int
pmc_cpu_is_disabled(int cpu)
{
#ifdef	SMP
	return ((hlt_cpus_mask & (1 << cpu)) != 0);
#else
	return 0;
#endif
}

int
pmc_cpu_is_logical(int cpu)
{
#ifdef	SMP
	return ((logical_cpus_mask & (1 << cpu)) != 0);
#else
	return 0;
#endif
}
