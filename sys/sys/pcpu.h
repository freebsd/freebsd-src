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

/*
 * Define a set for pcpu data.
 * 
 * We don't use SET_DECLARE because it defines the set as 'a' when we
 * want 'aw'.  gcc considers uninitialized data in a separate section
 * writable, and there is no generic zero initializer that works for
 * structs and scalars.
 */
extern uintptr_t *__start_set_pcpu;
extern uintptr_t *__stop_set_pcpu;

__asm__(
#ifdef __arm__
	".section set_pcpu, \"aw\", %progbits\n"
#else
	".section set_pcpu, \"aw\", @progbits\n"
#endif
	"\t.p2align " __XSTRING(CACHE_LINE_SHIFT) "\n"
	"\t.previous");

/*
 * Array of dynamic pcpu base offsets.  Indexed by id.
 */
extern uintptr_t dpcpu_off[];

/*
 * Convenience defines.
 */
#define	DPCPU_START		((uintptr_t)&__start_set_pcpu)
#define	DPCPU_STOP		((uintptr_t)&__stop_set_pcpu)
#define	DPCPU_BYTES		(DPCPU_STOP - DPCPU_START)
#define	DPCPU_MODMIN		2048
#define	DPCPU_SIZE		roundup2(DPCPU_BYTES, PAGE_SIZE)
#define	DPCPU_MODSIZE		(DPCPU_SIZE - (DPCPU_BYTES - DPCPU_MODMIN))

/*
 * Declaration and definition.
 */
#define	DPCPU_NAME(n)		pcpu_entry_##n
#define	DPCPU_DECLARE(t, n)	extern t DPCPU_NAME(n)
#define	DPCPU_DEFINE(t, n)	t DPCPU_NAME(n) __section("set_pcpu") __used

/*
 * Accessors with a given base.
 */
#define	_DPCPU_PTR(b, n)						\
    (__typeof(DPCPU_NAME(n))*)((b) + (uintptr_t)&DPCPU_NAME(n))
#define	_DPCPU_GET(b, n)	(*_DPCPU_PTR(b, n))
#define	_DPCPU_SET(b, n, v)	(*_DPCPU_PTR(b, n) = v)

/*
 * Accessors for the current cpu.
 */
#define	DPCPU_PTR(n)		_DPCPU_PTR(PCPU_GET(dynamic), n)
#define	DPCPU_GET(n)		(*DPCPU_PTR(n))
#define	DPCPU_SET(n, v)		(*DPCPU_PTR(n) = v)

/*
 * Accessors for remote cpus.
 */
#define	DPCPU_ID_PTR(i, n)	_DPCPU_PTR(dpcpu_off[(i)], n)
#define	DPCPU_ID_GET(i, n)	(*DPCPU_ID_PTR(i, n))
#define	DPCPU_ID_SET(i, n, v)	(*DPCPU_ID_PTR(i, n) = v)

/* 
 * XXXUPS remove as soon as we have per cpu variable
 * linker sets and can define rm_queue in _rm_lock.h
 */
struct rm_queue {
	struct rm_queue* volatile rmq_next;
	struct rm_queue* volatile rmq_prev;
};

#define	PCPU_NAME_LEN (sizeof("CPU ") + sizeof(__XSTRING(MAXCPU) + 1))

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
	uint64_t	pc_switchtime;		/* cpu_ticks() at last csw */
	int		pc_switchticks;		/* `ticks' at last csw */
	u_int		pc_cpuid;		/* This cpu number */
	cpumask_t	pc_cpumask;		/* This cpu mask */
	cpumask_t	pc_other_cpus;		/* Mask of all other cpus */
	SLIST_ENTRY(pcpu) pc_allcpu;
	struct lock_list_entry *pc_spinlocks;
#ifdef KTR
	char		pc_name[PCPU_NAME_LEN];	/* String name for KTR */
#endif
	struct vmmeter	pc_cnt;			/* VM stats counters */
	long		pc_cp_time[CPUSTATES];	/* statclock ticks */
	struct device	*pc_device;
	void		*pc_netisr;		/* netisr SWI cookie */
	int		pc_dnweight;		/* vm_page_dontneed() */

	/*
	 * Stuff for read mostly lock
	 *
	 * XXXUPS remove as soon as we have per cpu variable
	 * linker sets.
	 */
	struct rm_queue	pc_rm_queue;

	uintptr_t	pc_dynamic;		/* Dynamic per-cpu data area */

	/*
	 * Keep MD fields last, so that CPU-specific variations on a
	 * single architecture don't result in offset variations of
	 * the machine-independent fields of the pcpu.  Even though
	 * the pcpu structure is private to the kernel, some ports
	 * (e.g., lsof, part of gtop) define _KERNEL and include this
	 * header.  While strictly speaking this is wrong, there's no
	 * reason not to keep the offsets of the MI fields constant
	 * if only to make kernel debugging easier.
	 */
	PCPU_MD_FIELDS;
} __aligned(CACHE_LINE_SIZE);

#ifdef _KERNEL

SLIST_HEAD(cpuhead, pcpu);

extern struct cpuhead cpuhead;
extern struct pcpu *cpuid_to_pcpu[MAXCPU];

#define	curcpu		PCPU_GET(cpuid)
#define	curproc		(curthread->td_proc)
#ifndef curthread
#define	curthread	PCPU_GET(curthread)
#endif
#define	curvidata	PCPU_GET(vidata)

/*
 * Machine dependent callouts.  cpu_pcpu_init() is responsible for
 * initializing machine dependent fields of struct pcpu, and
 * db_show_mdpcpu() is responsible for handling machine dependent
 * fields for the DDB 'show pcpu' command.
 */
void	cpu_pcpu_init(struct pcpu *pcpu, int cpuid, size_t size);
void	db_show_mdpcpu(struct pcpu *pcpu);

void	*dpcpu_alloc(int size);
void	dpcpu_copy(void *s, int size);
void	dpcpu_free(void *s, int size);
void	dpcpu_init(void *dpcpu, int cpuid);
void	pcpu_destroy(struct pcpu *pcpu);
struct	pcpu *pcpu_find(u_int cpuid);
void	pcpu_init(struct pcpu *pcpu, int cpuid, size_t size);

#endif /* _KERNEL */

#endif /* !_SYS_PCPU_H_ */
