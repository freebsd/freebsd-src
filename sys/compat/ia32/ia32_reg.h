/*-
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
 *	from: @(#)reg.h	5.5 (Berkeley) 1/18/91
 * $FreeBSD: src/sys/compat/ia32/ia32_reg.h,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _COMPAT_IA32_IA32_REG_H_
#define	_COMPAT_IA32_IA32_REG_H_

/*
 * Register set accessible via /proc/$pid/regs and PT_{SET,GET}REGS.
 */
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

/*
 * Register set accessible via /proc/$pid/fpregs.
 */
struct fpreg32 {
	unsigned int	fpr_env[7];
	unsigned char	fpr_acc[8][10];
	unsigned int	fpr_ex_sw;
	unsigned char	fpr_pad[64];
};

/*
 * Register set accessible via /proc/$pid/dbregs.
 */
struct dbreg32 {
	unsigned int  dr[8];	/* debug registers */
};

/* Environment information of floating point unit */
struct env87 {
	int	en_cw;		/* control word (16bits) */
	int	en_sw;		/* status word (16bits) */
	int	en_tw;		/* tag word (16bits) */
	int	en_fip;		/* floating point instruction pointer */
	u_short	en_fcs;		/* floating code segment selector */
	u_short	en_opcode;	/* opcode last executed (11 bits ) */
	int	en_foo;		/* floating operand offset */
	int	en_fos;		/* floating operand segment selector */
};

#ifdef __ia64__
/* Layout of an x87 fpu register (amd64 gets this elsewhere) */
struct fpacc87 {
	u_char  fp_bytes[10];
};
#endif

/* Floating point context */
struct save87 {
	struct	env87 sv_env;	/* floating point control/status */
	struct	fpacc87 sv_ac[8];	/* accumulator contents, 0-7 */
	u_char	sv_pad0[4];	/* padding for (now unused) saved status word */
	u_char	sv_pad[64];	/* padding; used by emulators */
};


/*
 * Alternative layouts for <sys/procfs.h>
 * Used in core dumps, the reason for this file existing.
 */
struct prstatus32 {
	int	pr_version;
	u_int	pr_statussz;
	u_int	pr_gregsetsz;
	u_int	pr_fpregsetsz;
	int	pr_osreldate;
	int	pr_cursig;
	pid_t	pr_pid;
	struct reg32 pr_reg;
};

struct prpsinfo32 {
	int	pr_version;
	u_int	pr_psinfosz;
	char	pr_fname[PRFNAMESZ+1];
	char	pr_psargs[PRARGSZ+1];
};

/*
 * Wrappers and converters.
 */
int	fill_regs32(struct thread *, struct reg32 *);
int	set_regs32(struct thread *, struct reg32 *);
int	fill_fpregs32(struct thread *, struct fpreg32 *);
int	set_fpregs32(struct thread *, struct fpreg32 *);
int	fill_dbregs32(struct thread *, struct dbreg32 *);
int	set_dbregs32(struct thread *, struct dbreg32 *);

#endif /* !_COMPAT_IA32_IA32_REG_H_ */
