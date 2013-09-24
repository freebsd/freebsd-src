/*-
 * Copyright (c) 2003 Peter Wemm.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)pcb.h	5.10 (Berkeley) 5/12/91
 * $FreeBSD$
 */

#ifndef _AMD64_PCB_H_
#define _AMD64_PCB_H_

/*
 * AMD64 process control block
 */
#include <machine/fpu.h>
#include <machine/segments.h>

struct pcb {
	register_t	pcb_r15;
	register_t	pcb_r14;
	register_t	pcb_r13;
	register_t	pcb_r12;
	register_t	pcb_rbp;
	register_t	pcb_rsp;
	register_t	pcb_rbx;
	register_t	pcb_rip;
	register_t	pcb_fsbase;
	register_t	pcb_gsbase;
	register_t	pcb_kgsbase;
	register_t	pcb_cr0;
	register_t	pcb_cr2;
	register_t	pcb_cr3;
	register_t	pcb_cr4;
	register_t	pcb_dr0;
	register_t	pcb_dr1;
	register_t	pcb_dr2;
	register_t	pcb_dr3;
	register_t	pcb_dr6;
	register_t	pcb_dr7;

	struct region_descriptor pcb_gdt;
	struct region_descriptor pcb_idt;
	struct region_descriptor pcb_ldt;
	uint16_t	pcb_tr;

	u_int		pcb_flags;
#define	PCB_FULL_IRET	0x01	/* full iret is required */
#define	PCB_DBREGS	0x02	/* process using debug registers */
#define	PCB_KERNFPU	0x04	/* kernel uses fpu */
#define	PCB_FPUINITDONE	0x08	/* fpu state is initialized */
#define	PCB_USERFPUINITDONE 0x10 /* fpu user state is initialized */
#define	PCB_32BIT	0x40	/* process has 32 bit context (segs etc) */

	uint16_t	pcb_initial_fpucw;

	/* copyin/out fault recovery */
	caddr_t		pcb_onfault;

	/* 32-bit segment descriptor */
	struct user_segment_descriptor pcb_gs32sd;

	/* local tss, with i/o bitmap; NULL for common */
	struct amd64tss *pcb_tssp;

	/* model specific registers */
	register_t	pcb_efer;
	register_t	pcb_star;
	register_t	pcb_lstar;
	register_t	pcb_cstar;
	register_t	pcb_sfmask;
	register_t	pcb_xsmask;

	/* fpu context for suspend/resume */
	void		*pcb_fpususpend;

	struct savefpu	*pcb_save;

	uint64_t	pcb_pad[3];
};

#ifdef _KERNEL
struct trapframe;

/*
 * The pcb_flags is only modified by current thread, or by other threads
 * when current thread is stopped.  However, current thread may change it
 * from the interrupt context in cpu_switch(), or in the trap handler.
 * When we read-modify-write pcb_flags from C sources, compiler may generate
 * code that is not atomic regarding the interrupt handler.  If a trap or
 * interrupt happens and any flag is modified from the handler, it can be
 * clobbered with the cached value later.  Therefore, we implement setting
 * and clearing flags with single-instruction functions, which do not race
 * with possible modification of the flags from the trap or interrupt context,
 * because traps and interrupts are executed only on instruction boundary.
 */
static __inline void
set_pcb_flags(struct pcb *pcb, const u_int flags)
{

	__asm __volatile("orl %1,%0"
	    : "=m" (pcb->pcb_flags) : "ir" (flags), "m" (pcb->pcb_flags)
	    : "cc");
}

static __inline void
clear_pcb_flags(struct pcb *pcb, const u_int flags)
{

	__asm __volatile("andl %1,%0"
	    : "=m" (pcb->pcb_flags) : "ir" (~flags), "m" (pcb->pcb_flags)
	    : "cc");
}

void	makectx(struct trapframe *, struct pcb *);
int	savectx(struct pcb *) __returns_twice;
void	resumectx(struct pcb *);

#endif

#endif /* _AMD64_PCB_H_ */
