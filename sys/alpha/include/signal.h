/* $FreeBSD: src/sys/alpha/include/signal.h,v 1.5 1999/10/04 19:33:49 marcel Exp $ */
/* From: NetBSD: signal.h,v 1.3 1997/04/06 08:47:43 cgd Exp */

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

#ifndef _ALPHA_SIGNAL_H_
#define	_ALPHA_SIGNAL_H_

typedef long	sig_atomic_t;

#ifndef _ANSI_SOURCE

/*
 * Information pushed on stack when a signal is delivered.
 * This is used by the kernel to restore state following
 * execution of the signal handler.  It is also made available
 * to the handler to allow it to restore state properly if
 * a non-standard exit is performed.
 *
 * Note that sc_regs[] and sc_fpregs[]+sc_fpcr are inline
 * representations of 'struct reg' and 'struct fpreg', respectively.
 */
typedef unsigned int osigset_t;

struct  osigcontext {
	long	sc_onstack;             /* sigstack state to restore */
	long	sc_mask;                /* signal mask to restore */
	long	sc_pc;                  /* pc to restore */
	long	sc_ps;			/* ps to restore */
	unsigned long sc_regs[32];	/* integer register set (see above) */
	long	sc_ownedfp;		/* fp has been used */
	unsigned long sc_fpregs[32];	/* FP register set (see above) */
	unsigned long sc_fpcr;		/* FP control register (see above) */
	unsigned long sc_fp_control;	/* FP software control word */
	long	sc_reserved[2];		/* XXX */
	long	sc_xxx1[2];		/* sc_ssize, sc_sbase on DUX */
	unsigned long sc_traparg_a0;	/* a0 argument to trap at exception */
	unsigned long sc_traparg_a1;	/* a1 argument to trap at exception */
	unsigned long sc_traparg_a2;	/* a2 argument to trap at exception */
	long	sc_xxx2[3];		/* sc_fp_trap_pc, sc_fp_trigger_sum, sc_fp_trigger_inst */
};

/*
 * The sequence of the fields should match those in
 * mcontext_t. Keep them in sync!
 */
struct	sigcontext {
	sigset_t sc_mask;		/* signal mask to restore */
	long	sc_onstack;             /* sigstack state to restore */
	unsigned long sc_regs[32];	/* integer register set (see above) */
	long	sc_ps;			/* ps to restore */
	long	sc_pc;                  /* pc to restore */
	unsigned long sc_traparg_a0;	/* a0 argument to trap at exception */
	unsigned long sc_traparg_a1;	/* a1 argument to trap at exception */
	unsigned long sc_traparg_a2;	/* a2 argument to trap at exception */
	unsigned long sc_fpregs[32];	/* FP register set (see above) */
	unsigned long sc_fpcr;		/* FP control register (see above) */
	unsigned long sc_fp_control;	/* FP software control word */
	long	sc_ownedfp;		/* fp has been used */
	long	sc_xxx1[2];		/* sc_ssize, sc_sbase on DUX */
	long	sc_xxx2[3];		/* sc_fp_trap_pc, sc_fp_trigger_sum, sc_fp_trigger_inst */
	long	sc_reserved[2];		/* XXX */
};

#define	sc_sp	sc_regs[R_SP]

#endif /* !_ANSI_SOURCE */
#endif /* !_ALPHA_SIGNAL_H_*/
