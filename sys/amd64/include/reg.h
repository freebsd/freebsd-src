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
 * $FreeBSD$
 */

#ifndef _MACHINE_REG_H_
#define	_MACHINE_REG_H_

/*
 * Indices for registers in `struct trapframe' and `struct regs'.
 *
 * This interface is deprecated.  In the kernel, it is only used in FPU
 * emulators to convert from register numbers encoded in instructions to
 * register values.  Everything else just accesses the relevant struct
 * members.  In userland, debuggers tend to abuse this interface since
 * they don't understand that `struct regs' is a struct.  I hope they have
 * stopped accessing the registers in the trap frame via PT_{READ,WRITE}_U
 * and we can stop supporting the user area soon.
 */
#define	tFS	(0)
#define	tES	(1)
#define	tDS	(2)
#define	tEDI	(3)
#define	tESI	(4)
#define	tEBP	(5)
#define	tISP	(6)
#define	tEBX	(7)
#define	tEDX	(8)
#define	tECX	(9)
#define	tEAX	(10)
#define	tERR	(12)
#define	tEIP	(13)
#define	tCS	(14)
#define	tEFLAGS	(15)
#define	tESP	(16)
#define	tSS	(17)

/*
 * Indices for registers in `struct regs' only.
 *
 * Some registers live in the pcb and are only in an "array" with the
 * other registers in application interfaces that copy all the registers
 * to or from a `struct regs'.
 */
#define	tGS	(18)

/*
 * Register set accessible via /proc/$pid/regs and PT_{SET,GET}REGS.
 */
struct reg {
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

/*
 * Register set accessible via /proc/$pid/dbregs.
 */
struct dbreg {
	unsigned int  dr0;	/* debug address register 0 */
	unsigned int  dr1;	/* debug address register 1 */
	unsigned int  dr2;	/* debug address register 2 */
	unsigned int  dr3;	/* debug address register 3 */
	unsigned int  dr4;	/* reserved */
	unsigned int  dr5;	/* reserved */
	unsigned int  dr6;	/* debug status register */
	unsigned int  dr7;	/* debug control register */
};

#define DBREG_DR7_EXEC      0x00      /* break on execute       */
#define DBREG_DR7_WRONLY    0x01      /* break on write         */
#define DBREG_DR7_RDWR      0x03      /* break on read or write */
#define DBREG_DRX(d,x) ((&d->dr0)[x]) /* reference dr0 - dr7 by
                                         register number */


#ifdef _KERNEL
/*
 * XXX these interfaces are MI, so they should be declared in a MI place.
 */
void	setregs __P((struct thread *, u_long, u_long, u_long));
int	fill_regs __P((struct thread *, struct reg *));
int	set_regs __P((struct thread *, struct reg *));
int	fill_fpregs __P((struct thread *, struct fpreg *));
int	set_fpregs __P((struct thread *, struct fpreg *));
int	fill_dbregs __P((struct thread *, struct dbreg *));
int	set_dbregs __P((struct thread *, struct dbreg *));
#endif

#endif /* !_MACHINE_REG_H_ */
