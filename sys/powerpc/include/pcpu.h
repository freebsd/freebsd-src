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
 * $FreeBSD$
 */

#ifndef	_MACHINE_GLOBALDATA_H_
#define	_MACHINE_GLOBALDATA_H_

#ifdef _KERNEL

#include <sys/queue.h>

/*
 * This structure maps out the global data that needs to be kept on a
 * per-cpu basis.  genassym uses this to generate offsets for the assembler
 * code, which also provides external symbols so that C can get at them as
 * though they were really globals. This structure is pointed to by
 * the per-cpu system value (see alpha_pal_rdval() and alpha_pal_wrval()).
 * Inside the kernel, the globally reserved register t7 is used to
 * point at the globaldata structure.
 */
struct globaldata {
	struct alpha_pcb gd_idlepcb;		/* pcb for idling */
	struct proc	*gd_curproc;		/* current process */
	struct proc	*gd_idleproc;		/* idle process */
	struct proc	*gd_fpcurproc;		/* fp state owner */
	struct pcb	*gd_curpcb;		/* current pcb */
	struct timeval	gd_switchtime;	
	int		gd_switchticks;
	u_int		gd_cpuno;		/* this cpu number */
	u_int		gd_other_cpus;		/* all other cpus */
	int		gd_inside_intr;
	u_int64_t	gd_idlepcbphys;		/* pa of gd_idlepcb */
	u_int64_t	gd_pending_ipis;	/* pending IPI events */
	u_int32_t	gd_next_asn;		/* next ASN to allocate */
	u_int32_t	gd_current_asngen;	/* ASN rollover check */
	u_int32_t	gd_intr_nesting_level;  /* interrupt recursion */

	u_int		gd_astpending;
	SLIST_ENTRY(globaldata) gd_allcpu;
	int		gd_witness_spin_check;
#ifdef KTR_PERCPU
	volatile int	gd_ktr_idx;		/* Index into trace table */
	char		*gd_ktr_buf;
	char		gd_ktr_buf_data[0];
#endif
};

SLIST_HEAD(cpuhead, globaldata);
extern struct cpuhead cpuhead;

void globaldata_init(struct globaldata *pcpu, int cpuno, size_t sz);
struct globaldata *globaldata_find(int cpuno);

#endif	/* _KERNEL */

#endif	/* !_MACHINE_GLOBALDATA_H_ */
