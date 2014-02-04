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

#include <machine/cpuconf.h>

#define	ALT_STACK_SIZE	128

struct vmspace;

#endif	/* _KERNEL */

#ifdef VFP
#define PCPU_MD_FIELDS							\
	unsigned int pc_cpu;						\
	unsigned int pc_vfpsid;						\
	unsigned int pc_vfpmvfr0;					\
	unsigned int pc_vfpmvfr1;					\
	struct thread *pc_vfpcthread;					\
	struct pmap *pc_curpmap;					\
	char __pad[133]
#else
#define PCPU_MD_FIELDS							\
	char __pad[157]
#endif

#ifdef _KERNEL

struct pcb;
struct pcpu;

extern struct pcpu *pcpup;
#if ARM_ARCH_6 || ARM_ARCH_7A
/* or ARM_TP_ADDRESS 	mark REMOVE ME NOTE */
static inline struct pcpu *
get_pcpu(void)
{
	void *pcpu;

	__asm __volatile("mrc p15, 0, %0, c13, c0, 4" : "=r" (pcpu));
	return (pcpu);
}

static inline void
set_pcpu(void *pcpu)
{

	__asm __volatile("mcr p15, 0, %0, c13, c0, 4" : : "r" (pcpu));
}

static inline void *
get_tls(void)
{
	void *tls;

	__asm __volatile("mrc p15, 0, %0, c13, c0, 3" : "=r" (tls));
	return (tls);
}

static inline void
set_tls(void *tls)
{

	__asm __volatile("mcr p15, 0, %0, c13, c0, 3" : : "r" (tls));
}
#else
#define get_pcpu()	pcpup
#endif

#define	PCPU_GET(member)	(get_pcpu()->pc_ ## member)
#define	PCPU_ADD(member, value)	(get_pcpu()->pc_ ## member += (value))
#define	PCPU_INC(member)	PCPU_ADD(member, 1)
#define	PCPU_PTR(member)	(&get_pcpu()->pc_ ## member)
#define	PCPU_SET(member,value)	(get_pcpu()->pc_ ## member = (value))

void pcpu0_init(void);
#endif	/* _KERNEL */

#endif	/* !_MACHINE_PCPU_H_ */
