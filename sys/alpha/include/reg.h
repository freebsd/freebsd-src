/* $FreeBSD$ */
/* From: NetBSD: reg.h,v 1.3 1997/04/06 08:47:40 cgd Exp */

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef _ALPHA_REG_H_
#define	_ALPHA_REG_H_

/*
 * XXX where did this info come from?
 */

/*
 * Struct reg, used for procfs and in signal contexts
 * Note that in signal contexts, it's represented as an array.
 * That array has to look exactly like 'struct reg' though.
 */
#define	R_V0	0
#define	R_T0	1
#define	R_T1	2
#define	R_T2	3
#define	R_T3	4
#define	R_T4	5
#define	R_T5	6
#define	R_T6	7
#define	R_T7	8
#define	R_S0	9
#define	R_S1	10
#define	R_S2	11
#define	R_S3	12
#define	R_S4	13
#define	R_S5	14
#define	R_S6	15
#define	R_A0	16
#define	R_A1	17
#define	R_A2	18
#define	R_A3	19
#define	R_A4	20
#define	R_A5	21
#define	R_T8	22
#define	R_T9	23
#define	R_T10	24
#define	R_T11	25
#define	R_RA	26
#define	R_T12	27
#define	R_AT	28
#define	R_GP	29
#define	R_SP	30
#define	R_ZERO	31

/*
 * Register extensions used in mcontext_t
 */
#define	R_PS		32
#define	R_PC		33
#define	R_TRAPARG_A0	34
#define	R_TRAPARG_A1	35
#define	R_TRAPARG_A2	36

struct reg {
	u_int64_t	r_regs[32];
};

/*
 * Floating point unit state. (also, register set used for ptrace.)
 *
 * The floating point registers for a process, saved only when
 * necessary.
 *
 * Note that in signal contexts, it's represented as an array.
 * That array has to look exactly like 'struct reg' though.
 */
struct fpreg {
	u_int64_t	fpr_regs[32];
	u_int64_t	fpr_cr;
};

/*
 * Placeholder.
 */
struct dbreg {
	unsigned long junk;
};

#ifdef _KERNEL
void	restorefpstate __P((struct fpreg *));
void	savefpstate __P((struct fpreg *));
void	setregs __P((struct proc *, u_long, u_long, u_long));
#endif

#endif /* _ALPHA_REG_H_ */
