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
 *	from: @(#)frame.h	5.2 (Berkeley) 1/18/91
 *	$Id: frame.h,v 1.11 1995/12/14 08:21:33 phk Exp $
 */

#ifndef _MACHINE_FRAME_H_
#define _MACHINE_FRAME_H_ 1

#include <sys/signal.h>

/*
 * System stack frames.
 */

/*
 * Exception/Trap Stack Frame
 */

struct trapframe {
	int	tf_es;
	int	tf_ds;
	int	tf_edi;
	int	tf_esi;
	int	tf_ebp;
	int	tf_isp;
	int	tf_ebx;
	int	tf_edx;
	int	tf_ecx;
	int	tf_eax;
	int	tf_trapno;
	/* below portion defined in 386 hardware */
	int	tf_err;
	int	tf_eip;
	int	tf_cs;
	int	tf_eflags;
	/* below only when crossing rings (e.g. user to kernel) */
	int	tf_esp;
	int	tf_ss;
};

/* Interrupt stack frame */

struct intrframe {
	int	if_vec;
	int	if_ppl;
	int	if_es;
	int	if_ds;
	int	if_edi;
	int	if_esi;
	int	if_ebp;
	int	:32;
	int	if_ebx;
	int	if_edx;
	int	if_ecx;
	int	if_eax;
	int	:32;		/* for compat with trap frame - trapno */
	int	:32;		/* for compat with trap frame - err */
	/* below portion defined in 386 hardware */
	int	if_eip;
	int	if_cs;
	int	if_eflags;
	/* below only when crossing rings (e.g. user to kernel) */
	int	if_esp;
	int	if_ss;
};

/* frame of clock (same as interrupt frame) */

struct clockframe {
	int	cf_vec;
	int	cf_ppl;
	int	cf_es;
	int	cf_ds;
	int	cf_edi;
	int	cf_esi;
	int	cf_ebp;
	int	:32;
	int	cf_ebx;
	int	cf_edx;
	int	cf_ecx;
	int	cf_eax;
	int	:32;		/* for compat with trap frame - trapno */
	int	:32;		/* for compat with trap frame - err */
	/* below portion defined in 386 hardware */
	int	cf_eip;
	int	cf_cs;
	int	cf_eflags;
	/* below only when crossing rings (e.g. user to kernel) */
	int	cf_esp;
	int	cf_ss;
};

/*
 * Signal frame
 */
struct sigframe {
	int	sf_signum;
	int	sf_code;
	struct	sigcontext *sf_scp;
	char	*sf_addr;
	sig_t	sf_handler;
	struct	sigcontext sf_sc;
};

int	kdb_trap __P((int, int, struct trapframe *));
extern  int (*pmath_emulate) __P((struct trapframe *));

#endif /* _MACHINE_FRAME_H_ */
