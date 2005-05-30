/*-
 * Copyright (c) 2003-2005, Joseph Koshy
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

#include <sys/pmckern.h>
#include <sys/smp.h>

struct sx pmc_sx;

/* Hook variable. */
int (*pmc_hook)(struct thread *td, int function, void *arg) = NULL;

/* Interrupt handler */
int (*pmc_intr)(int cpu, uintptr_t pc, int usermode) = NULL;

cpumask_t pmc_cpumask;

/*
 * Since PMC(4) may not be loaded in the current kernel, the
 * convention followed is that a non-NULL value of 'pmc_hook' implies
 * the presence of this kernel module.
 *
 * This requires us to protect 'pmc_hook' with a
 * shared (sx) lock -- thus making the process of calling into PMC(4)
 * somewhat more expensive than a simple 'if' check and indirect call.
 */


SX_SYSINIT(pmc, &pmc_sx, "pmc shared lock");

/*
 * pmc_cpu_is_disabled
 *
 * return TRUE if the cpu specified has been disabled.
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
