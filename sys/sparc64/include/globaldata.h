/*-
 * Copyright (c) 1999 Luoqi Chen <luoqi@freebsd.org>
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
 *	from: FreeBSD: src/sys/i386/include/globaldata.h,v 1.27 2001/04/27
 * $FreeBSD$
 */

#ifndef	_MACHINE_GLOBALDATA_H_
#define	_MACHINE_GLOBALDATA_H_

#ifdef _KERNEL

#include <sys/queue.h>

#include <machine/frame.h>
#include <machine/intr_machdep.h>

#define	ALT_STACK_SIZE	128

/*
 * This structure maps out the global data that needs to be kept on a
 * per-cpu basis.  genassym uses this to generate offsets for the assembler
 * code, which also provides external symbols so that C can get at them as
 * though they were really globals. This structure is pointed to by
 * the per-cpu system value.
 * Inside the kernel, the globally reserved register g7 is used to
 * point at the globaldata structure.
 */
struct globaldata {
	struct	thread *gd_curthread;		/* current thread */
	struct	thread *gd_idlethread;		/* idle thread */
	struct	pcb *gd_curpcb;			/* current pcb */
	struct	timeval gd_switchtime;	
	int	gd_switchticks;
	u_int	gd_cpuid;			/* this cpu number */
	u_int	gd_other_cpus;			/* all other cpus */
	SLIST_ENTRY(globaldata) gd_allcpu;
	struct	lock_list_entry *gd_spinlocks;

	struct	intr_queue gd_iq;		/* interrupt queuq */
	u_long	gd_alt_stack[ALT_STACK_SIZE];	/* alternate global stack */
	u_int	gd_wp_insn;			/* watch point support */
	u_long	gd_wp_pstate;
	u_long	gd_wp_va;
	int	gd_wp_mask;

#ifdef KTR_PERCPU
	int	gd_ktr_idx;			/* index into trace table */
	char	*gd_ktr_buf;
	char	gd_ktr_buf_data[0];
#endif
};

#endif	/* _KERNEL */

#endif	/* !_MACHINE_GLOBALDATA_H_ */
