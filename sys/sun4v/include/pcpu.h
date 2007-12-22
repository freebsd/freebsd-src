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

#ifndef	_MACHINE_PCPU_H_
#define	_MACHINE_PCPU_H_

#ifdef _KERNEL

#include <machine/asmacros.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>

#define	ALT_STACK_SIZE	128

struct pmap;

/*
 * Inside the kernel, the globally reserved register g7 is used to
 * point at the globaldata structure.
 */
#define	PCPU_MD_FIELDS							\
	struct	intr_request pc_irpool[IR_FREE];			\
	struct	intr_request *pc_irhead;				\
	struct	intr_request **pc_irtail;				\
	struct	intr_request *pc_irfree;				\
	struct 	pmap *pc_curpmap;					\
	vm_offset_t pc_addr;						\
	vm_offset_t *pc_mondo_data;                                     \
        vm_offset_t *pc_cpu_list;                                       \
	vm_offset_t *pc_cpu_q;                                          \
	vm_offset_t *pc_dev_q;                                          \
	vm_offset_t *pc_rq;                                             \
	vm_offset_t *pc_nrq;                                            \
	vm_paddr_t pc_mondo_data_ra;                                    \
        vm_paddr_t pc_cpu_list_ra;                                      \
	vm_paddr_t pc_cpu_q_ra;                                         \
	uint64_t pc_cpu_q_size;                                         \
	vm_paddr_t pc_dev_q_ra;                                         \
	uint64_t pc_dev_q_size;                                         \
	vm_paddr_t pc_rq_ra;                                            \
	uint64_t pc_rq_size;                                            \
	vm_paddr_t pc_nrq_ra;                                           \
	uint64_t pc_nrq_size;                                           \
	u_long	pc_tickref;						\
	u_long	pc_tickadj;						\
	struct rwindow pc_kwbuf;                                        \
	u_long  pc_kwbuf_sp;                                            \
	u_int   pc_kwbuf_full;                                          \
	struct rwindow pc_tsbwbuf[2];                                   \
        uint16_t pc_cpulist[MAXCPU];                                    \
        uint64_t pad[11];

	/* XXX SUN4V_FIXME - as we access the *_ra and *_size fields in quick
	 * succession we _really_ want them to be L1 cache line size aligned
	 * and it is quite possible that we want all of ASI_QUEUE fields to
	 * be L2 cache aligned - they're surrounded by per-cpu data, so there is
	 * no possibility of false sharing, but this might help in reducing misses
	 */
struct pcpu;

register struct pcpu *pcpup __asm__(__XSTRING(PCPU_REG));

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
