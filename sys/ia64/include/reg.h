/*-
 * Copyright (c) 2000 Doug Rabson
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
 *	$FreeBSD$
 */

#ifndef _MACHINE_REG_H_
#define _MACHINE_REG_H_

#include <machine/_regset.h>

struct reg32 {
	unsigned int	r_fs;
	unsigned int	r_es;
	unsigned int	r_ds;
	unsigned int	r_edi;
	unsigned int	r_esi;
	unsigned int	r_ebp;
	unsigned int	r_isp;
	unsigned int	r_ebx;
	unsigned int	r_edx;
	unsigned int	r_ecx;
	unsigned int	r_eax;
	unsigned int	r_trapno;
	unsigned int	r_err;
	unsigned int	r_eip;
	unsigned int	r_cs;
	unsigned int	r_eflags;
	unsigned int	r_esp;
	unsigned int	r_ss;
	unsigned int	r_gs;
};

struct reg {
	struct _special		r_special;
	struct _callee_saved	r_preserved;
	struct _caller_saved	r_scratch;
};

struct fpreg32 {
	unsigned int	fpr_env[7];
	unsigned char	fpr_acc[8][10];
	unsigned int	fpr_ex_sw;
	unsigned char	fpr_pad[64];
};

struct fpreg {
	struct _callee_saved_fp	fpr_preserved;
	struct _caller_saved_fp	fpr_scratch;
	struct _high_fp		fpr_high;
};

struct dbreg32 {
	unsigned int	dr[8];
};

struct dbreg {
	unsigned long	dbr_data[8];
	unsigned long	dbr_inst[8];
};

#ifdef _KERNEL
struct thread;

/* XXX these interfaces are MI, so they should be declared in a MI place. */
int	fill_regs(struct thread *, struct reg *);
int	set_regs(struct thread *, struct reg *);
int	fill_fpregs(struct thread *, struct fpreg *);
int	set_fpregs(struct thread *, struct fpreg *);
int	fill_dbregs(struct thread *, struct dbreg *);
int	set_dbregs(struct thread *, struct dbreg *);
#ifdef COMPAT_FREEBSD32
int	fill_regs32(struct thread *, struct reg32 *);
int	set_regs32(struct thread *, struct reg32 *);
int	fill_fpregs32(struct thread *, struct fpreg32 *);
int	set_fpregs32(struct thread *, struct fpreg32 *);
int	fill_dbregs32(struct thread *, struct dbreg32 *);
int	set_dbregs32(struct thread *, struct dbreg32 *);
#endif
#endif

#endif /* _MACHINE_REG_H_ */
