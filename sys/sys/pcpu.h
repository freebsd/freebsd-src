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
 * $FreeBSD: src/sys/sys/pcpu.h,v 1.10 2002/12/10 02:33:45 julian Exp $
 */

#ifndef _SYS_PCPU_H_
#define	_SYS_PCPU_H_

#ifndef _KERNEL
#error "no user-serviceable parts inside"
#endif

#ifdef LOCORE
#error "no assembler-serviceable parts inside"
#endif

#include <sys/queue.h>
#include <sys/vmmeter.h>
#include <machine/pcpu.h>

struct pcb;
struct thread;

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
	struct bintime	pc_switchtime;	
	int		pc_switchticks;
	u_int		pc_cpuid;		/* This cpu number */
	u_int		pc_cpumask;		/* This cpu mask */
	u_int		pc_other_cpus;		/* Mask of all other cpus */
	SLIST_ENTRY(pcpu) pc_allcpu;
	struct lock_list_entry *pc_spinlocks;
#ifdef KTR_PERCPU
	int		pc_ktr_idx;		/* Index into trace table */
	char		*pc_ktr_buf;
#endif
	PCPU_MD_FIELDS;
	struct vmmeter	pc_cnt;			/* VM stats counters */
};

SLIST_HEAD(cpuhead, pcpu);

extern struct cpuhead cpuhead;

#define	CURPROC		(curthread->td_proc)
#define	curkse		(curthread->td_kse)
#define	curksegrp	(curthread->td_ksegrp)
#define	curproc		(curthread->td_proc)
#define	curthread	PCPU_GET(curthread)

/*
 * MI PCPU support functions
 *
 * PCPU_LAZY_INC() -	Lazily increment a per-cpu stats counter, without
 *			guarenteeing atomicity or even necessarily consistency.
 *
 *			XXX we need to create MD primitives to support
 *			this to guarentee at least some level of consistency,
 *			i.e., to prevent us from totally corrupting the 
 *			counters due to preemption in a multi-instruction
 *			increment sequence for architectures that do not
 *			support single-instruction memory increments.
 */
#define PCPU_LAZY_INC(var)	(++*PCPU_PTR(var))

/*
 * Machine dependent callouts.  cpu_pcpu_init() is responsible for
 * initializing machine dependent fields of struct pcpu, and
 * db_show_mdpcpu() is responsible for handling machine dependent
 * fields for the DDB 'show pcpu' command.
 */
void	cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size);
void	db_show_mdpcpu(struct pcpu *pcpu);

void	pcpu_destroy(struct pcpu *pcpu);
struct	pcpu *pcpu_find(u_int cpuid);
void	pcpu_init(struct pcpu *pcpu, int cpuid, size_t size);

#endif /* !_SYS_PCPU_H_ */
