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
 *	$Id: reg.h,v 1.10 1995/01/14 10:41:41 bde Exp $
 */

#ifndef _MACHINE_REG_H_
#define	_MACHINE_REG_H_

/*
 * Location of the users' stored
 * registers within appropriate frame of 'trap' and 'syscall', relative to
 * base of stack frame.
 * Normal usage is u.u_ar0[XX] in kernel.
 */

/* When referenced during a trap/exception, registers are at these offsets */

#define	tES	(0)
#define	tDS	(1)
#define	tEDI	(2)
#define	tESI	(3)
#define	tEBP	(4)
#define	tISP	(5)
#define	tEBX	(6)
#define	tEDX	(7)
#define	tECX	(8)
#define	tEAX	(9)

#define	tERR	(11)

#define	tEIP	(12)
#define	tCS	(13)
#define	tEFLAGS	(14)
#define	tESP	(15)
#define	tSS	(16)

/*
 * Registers accessible to ptrace(2) syscall for debugger
 * The machine-dependent code for PT_{SET,GET}REGS needs to
 * use whichever order, defined above, is correct, so that it
 * is all invisible to the user.
 */
struct reg {
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
};

/*
 * Register set accessible via /proc/$pid/fpreg
 */
struct fpreg {
	/*
	 * XXX should get struct from npx.h.  Here we give a slightly
	 * simplified struct.  This may be too much detail.  Perhaps
	 * an array of unsigned longs is best.
	 */
	unsigned long	fpr_env[7];
	unsigned char	fpr_acc[8][10];
	unsigned long	fpr_ex_sw;
	unsigned char	fpr_pad[64];
};

#ifdef KERNEL
int	set_regs __P((struct proc *p, struct reg *regs));
void	setregs __P((struct proc *, u_long, u_long));
#endif

#endif /* !_MACHINE_REG_H_ */
