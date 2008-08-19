/*-
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

#ifndef _SYS_PCPU_H_
#define	_SYS_PCPU_H_

#ifdef LOCORE
#error "no assembler-serviceable parts inside"
#endif

#include <sys/queue.h>
#include <sys/vmmeter.h>
#include <sys/resource.h>
#include <machine/pcpu.h>

struct pcb;
struct thread;

/* 
 * XXXUPS remove as soon as we have per cpu variable
 * linker sets and  can define rm_queue in _rm_lock.h
*/
struct rm_queue {
	struct rm_queue* volatile rmq_next;
	struct rm_queue* volatile rmq_prev;
};

/*
 * This structure maps out the global data that needs to be kept on a
 * per-cpu basis.  The members are accessed via the PCPU_GET/SET/PTR
 * macros defined in <machine/pcpu.h>.  Machine dependent fields are
 * defined in the PCPU_MD_FIELDS macro defined in <machine/pcpu.h>.
 */
struct pcpu {
	struct thread	*pc_curthread;		/* Current thread */
	struct thread	*pc_idlethread;		/* Idle thread */
	struct thread	*pc_fpcurthread;	/* Fp state owner */
	struct thread	*pc_deadthread;		/* Zombie thread or NULL */
	struct pcb	*pc_curpcb;		/* Current pcb */
	uint64_t	pc_switchtime;
	int		pc_switchticks;
	u_int		pc_cpuid;		/* This cpu number */
	cpumask_t	pc_cpumask;		/* This cpu mask */
	cpumask_t	pc_other_cpus;		/* Mask of all other cpus */
	SLIST_ENTRY(pcpu) pc_allcpu;
	struct lock_list_entry *pc_spinlocks;
#ifdef KTR_PERCPU
	int		pc_ktr_idx;		/* Index into trace table */
	char		*pc_ktr_buf;
#endif
	struct vmmeter	pc_cnt;			/* VM stats counters */
	long		pc_cp_time[CPUSTATES];	/* statclock ticks */
	struct device	*pc_device;

	/* 
	 * Stuff for read mostly lock
	 * 
	 * XXXUPS remove as soon as we have per cpu variable
	 * linker sets.
	 */
	struct rm_queue  pc_rm_queue; 

	/*
	 * Keep MD fields last, so that CPU-specific variations on a
	 * single architecture don't result in offset variations of
	 * the machine-independent fields of the pcpu. Even though
	 * the pcpu structure is private to the kernel, some ports
	 * (e.g. lsof, part of gtop) define _KERNEL and include this
	 * header. While strictly speaking this is wrong, there's no
	 * reason not to keep the offsets of the MI fields contants.
	 * If only to make kernel debugging easier...
	 */
	PCPU_MD_FIELDS;
};

#ifdef _KERNEL

SLIST_HEAD(cpuhead, pcpu);

extern struct cpuhead cpuhead;

#define	curcpu		PCPU_GET(cpuid)
#define	curproc		(curthread->td_proc)
#ifndef curthread
#define	curthread	PCPU_GET(curthread)
#endif

/*
 * Machine dependent callouts.  cpu_pcpu_init() is responsible for
 * initializing machine dependent fields of struct pcpu, and
 * db_show_mdpcpu() is responsible for handling machine dependent
 * fields for the DDB 'show pcpu' command.
 */

extern struct pcpu *cpuid_to_pcpu[MAXCPU];


void	cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size);
void	db_show_mdpcpu(struct pcpu *pcpu);

void	pcpu_destroy(struct pcpu *pcpu);
struct	pcpu *pcpu_find(u_int cpuid);
void	pcpu_init(struct pcpu *pcpu, int cpuid, size_t size);

#endif	/* _KERNEL */

#endif /* !_SYS_PCPU_H_ */
