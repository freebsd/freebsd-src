/*	$OpenBSD: proc.h,v 1.2 1998/09/15 10:50:12 pefo Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	@(#)proc.h	8.1 (Berkeley) 6/10/93
 *	JNPR: proc.h,v 1.7.2.1 2007/09/10 06:25:24 girish
 * $FreeBSD$
 */

#ifndef _MACHINE_PROC_H_
#define	_MACHINE_PROC_H_

/*
 * Machine-dependent part of the proc structure.
 */
struct mdthread {
	int	md_flags;		/* machine-dependent flags */
	int	md_upte[KSTACK_PAGES];	/* ptes for mapping u pcb */
	int	md_ss_addr;		/* single step address for ptrace */
	int	md_ss_instr;		/* single step instruction for ptrace */
	register_t	md_saved_intr;
	u_int	md_spinlock_count;
/* The following is CPU dependent, but kept in for compatibility */
	int	md_pc_ctrl;		/* performance counter control */
	int	md_pc_count;		/* performance counter */
	int	md_pc_spill;		/* performance counter spill */
	vm_offset_t	md_realstack;
	void	*md_tls;
};

/* md_flags */
#define	MDTD_FPUSED	0x0001		/* Process used the FPU */

struct mdproc {
	/* empty */
};

struct thread;

void	mips_cpu_switch(struct thread *, struct thread *, struct mtx *);
void	mips_cpu_throw(struct thread *, struct thread *);

#endif	/* !_MACHINE_PROC_H_ */
