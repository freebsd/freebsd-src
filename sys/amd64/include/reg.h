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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	from: @(#)reg.h	5.5 (Berkeley) 1/18/91
 * $FreeBSD$
 */

#ifndef _MACHINE_REG_H_
#define	_MACHINE_REG_H_

/*
 * Register set accessible via /proc/$pid/regs and PT_{SET,GET}REGS.
 */
struct reg {
	register_t	r_r15;
	register_t	r_r14;
	register_t	r_r13;
	register_t	r_r12;
	register_t	r_r11;
	register_t	r_r10;
	register_t	r_r9;
	register_t	r_r8;
	register_t	r_rdi;
	register_t	r_rsi;
	register_t	r_rbp;
	register_t	r_rbx;
	register_t	r_rdx;
	register_t	r_rcx;
	register_t	r_rax;
	register_t	r_trapno;
	register_t	r_err;
	register_t	r_rip;
	register_t	r_cs;
	register_t	r_rflags;
	register_t	r_rsp;
	register_t	r_ss;
};

/*
 * Register set accessible via /proc/$pid/fpregs.
 */
struct fpreg {
	/*
	 * XXX should get struct from fpu.h.  Here we give a slightly
	 * simplified struct.  This may be too much detail.  Perhaps
	 * an array of unsigned longs is best.
	 */
	unsigned long	fpr_env[4];
	unsigned char	fpr_acc[8][16];
	unsigned char	fpr_xacc[16][16];
	unsigned long	fpr_spare[12];
};

struct dbreg {
	unsigned long grrr;
};

#ifdef _KERNEL
/*
 * XXX these interfaces are MI, so they should be declared in a MI place.
 */
int	fill_regs(struct thread *, struct reg *);
int	set_regs(struct thread *, struct reg *);
int	fill_fpregs(struct thread *, struct fpreg *);
int	set_fpregs(struct thread *, struct fpreg *);
int	fill_dbregs(struct thread *, struct dbreg *);
int	set_dbregs(struct thread *, struct dbreg *);
#endif

#endif /* !_MACHINE_REG_H_ */
