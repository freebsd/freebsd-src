/*-
 * Copyright (c) 1999 Luoqi Chen <luoqi@freebsd.org>
 * Copyright (c) Peter Wemm <peter@netplex.com.au>
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

#ifndef	_MACHINE_PCPU_H_
#define	_MACHINE_PCPU_H_

#include <sys/sysctl.h>
#include <machine/pcb.h>

struct pcpu_stats {
	u_long		pcs_nasts;		/* IPI_AST counter. */
	u_long		pcs_nclks;		/* Clock interrupt counter. */
	u_long		pcs_nextints;		/* ExtINT counter. */
	u_long		pcs_nhardclocks;	/* IPI_HARDCLOCK counter. */
	u_long		pcs_nhighfps;		/* IPI_HIGH_FP counter. */
	u_long		pcs_nhwints;		/* Hardware int. counter. */
	u_long		pcs_npreempts;		/* IPI_PREEMPT counter. */
	u_long		pcs_nrdvs;		/* IPI_RENDEZVOUS counter. */
	u_long		pcs_nstops;		/* IPI_STOP counter. */
	u_long		pcs_nstrays;		/* Stray interrupt counter. */
};

struct pcpu_md {
	struct pcb	pcb;			/* Used by IPI_STOP */
	struct pmap	*current_pmap;		/* active pmap */
	vm_offset_t	vhpt;			/* Address of VHPT */
	uint64_t	lid;			/* local CPU ID */
	uint64_t	clock;			/* Clock counter. */
	uint64_t	clock_load;		/* Clock reload value. */
	uint32_t	clock_mode;		/* Clock ET mode */
	uint32_t	awake:1;		/* CPU is awake? */
	struct pcpu_stats stats;		/* Interrupt stats. */
#ifdef _KERNEL
	struct sysctl_ctx_list sysctl_ctx;
	struct sysctl_oid *sysctl_tree;
#endif
};

#define	PCPU_MD_FIELDS							\
	uint32_t	pc_acpi_id;		/* ACPI CPU id. */	\
	struct pcpu_md	pc_md			/* MD fields. */

#ifdef _KERNEL

struct pcpu;

register struct pcpu * volatile pcpup __asm__("r13");

static __inline __pure2 struct thread *
__curthread(void)
{
	struct thread *td;

	__asm("ld8.acq %0=[r13]" : "=r"(td));
	return (td);
}
#define	curthread	(__curthread())

#define	PCPU_GET(member)	(pcpup->pc_ ## member)

/*
 * XXX The implementation of this operation should be made atomic
 * with respect to preemption.
 */
#define	PCPU_ADD(member, value)	(pcpup->pc_ ## member += (value))
#define	PCPU_INC(member)	PCPU_ADD(member, 1)
#define	PCPU_PTR(member)	(&pcpup->pc_ ## member)
#define	PCPU_SET(member,value)	(pcpup->pc_ ## member = (value))

#endif	/* _KERNEL */

#endif	/* !_MACHINE_PCPU_H_ */
