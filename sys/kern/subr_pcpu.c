/*
 * Copyright (c) 2001 Wind River Systems, Inc.
 * All rights reserved.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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

/*
 * This module provides MI support for per-cpu data.
 *
 * Each architecture determines the mapping of logical CPU IDs to physical
 * CPUs.  The requirements of this mapping are as follows:
 *  - Logical CPU IDs must reside in the range 0 ... MAXCPU - 1.
 *  - The mapping is not required to be dense.  That is, there may be
 *    gaps in the mappings.
 *  - The platform sets the value of MAXCPU in <machine/param.h>.
 *  - It is suggested, but not required, that in the non-SMP case, the
 *    platform define MAXCPU to be 1 and define the logical ID of the
 *    sole CPU as 0.
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/linker_set.h>
#include <sys/lock.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <ddb/ddb.h>

static struct pcpu *cpuid_to_pcpu[MAXCPU];
struct cpuhead cpuhead = SLIST_HEAD_INITIALIZER(cpuhead);

/*
 * Initialize the MI portions of a struct pcpu.
 */
void
pcpu_init(struct pcpu *pcpu, int cpuid, size_t size)
{

	bzero(pcpu, size);
	KASSERT(cpuid >= 0 && cpuid < MAXCPU,
	    ("pcpu_init: invalid cpuid %d", cpuid));
	pcpu->pc_cpuid = cpuid;
	cpuid_to_pcpu[cpuid] = pcpu;
	SLIST_INSERT_HEAD(&cpuhead, pcpu, pc_allcpu);
	cpu_pcpu_init(pcpu, cpuid, size);
}

/*
 * Destroy a struct pcpu.
 */
void
pcpu_destroy(struct pcpu *pcpu)
{

	SLIST_REMOVE(&cpuhead, pcpu, pcpu, pc_allcpu);
	cpuid_to_pcpu[pcpu->pc_cpuid] = NULL;
}

/*
 * Locate a struct pcpu by cpu id.
 */
struct pcpu *
pcpu_find(u_int cpuid)
{

	return (cpuid_to_pcpu[cpuid]);
}

#ifdef DDB
DB_SHOW_COMMAND(pcpu, db_show_pcpu)
{
	struct pcpu *pc;
	struct thread *td;
	int id;

	if (have_addr)
		id = ((addr >> 4) % 16) * 10 + (addr % 16);
	else
		id = PCPU_GET(cpuid);
	pc = pcpu_find(id);
	if (pc == NULL) {
		db_printf("CPU %d not found\n", id);
		return;
	}
	db_printf("cpuid        = %d\n", pc->pc_cpuid);
	db_printf("curthread    = ");
	td = pc->pc_curthread;
	if (td != NULL)
		db_printf("%p: pid %d \"%s\"\n", td, td->td_proc->p_pid,
		    td->td_proc->p_comm);
	else
		db_printf("none\n");
	db_printf("curpcb       = %p\n", pc->pc_curpcb);
	db_printf("fpcurthread  = ");
	td = pc->pc_fpcurthread;
	if (td != NULL)
		db_printf("%p: pid %d \"%s\"\n", td, td->td_proc->p_pid,
		    td->td_proc->p_comm);
	else
		db_printf("none\n");
	db_printf("idlethread   = ");
	td = pc->pc_idlethread;
	if (td != NULL)
		db_printf("%p: pid %d \"%s\"\n", td, td->td_proc->p_pid,
		    td->td_proc->p_comm);
	else
		db_printf("none\n");
	db_show_mdpcpu(pc);
		
#ifdef WITNESS
	db_printf("spin locks held:\n");
	witness_list_locks(&pc->pc_spinlocks);
#endif
}
#endif
