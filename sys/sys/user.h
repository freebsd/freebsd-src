/*
 * Copyright (c) 1982, 1986, 1989, 1991 Regents of the University of California.
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
 *	from: @(#)user.h	7.19 (Berkeley) 5/4/91
 *	$Id: user.h,v 1.3 1993/11/07 17:53:17 wollman Exp $
 */

#ifndef _SYS_USER_H_
#define _SYS_USER_H_ 1

#include <machine/pcb.h>
#ifndef KERNEL
/* stuff that *used* to be included by user.h, or is now needed */
#include <errno.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/ucred.h>
#include <sys/uio.h>
#endif
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <vm/vm.h>		/* XXX */
#include <sys/kinfo_proc.h>


/*
 * Per process structure containing data that isn't needed in core
 * when the process isn't running (esp. when swapped out).
 * This structure may or may not be at the same kernel address
 * in all processes.
 */
 
struct	user {
	struct	pcb u_pcb;

	struct	sigacts u_sigacts;	/* p_sigacts points here (use it!) */
	struct	pstats u_stats;		/* p_stats points here (use it!) */

	/*
	 * Remaining fields only for core dump and/or ptrace--
	 * not valid at other times!
	 */
	struct	kinfo_proc u_kproc;	/* proc + eproc */
};

/*
 * Redefinitions to make the debuggers happy for now...
 * This subterfuge brought to you by coredump() and procxmt().
 * These fields are *only* valid at those times!
 */
#define	U_ar0	u_kproc.kp_proc.p_regs	/* copy of curproc->p_regs */
#define	U_tsize	u_kproc.kp_eproc.e_vm.vm_tsize
#define	U_dsize	u_kproc.kp_eproc.e_vm.vm_dsize
#define	U_ssize	u_kproc.kp_eproc.e_vm.vm_ssize
#define	U_sig	u_sigacts.ps_sig
#define	U_code	u_sigacts.ps_code

#ifndef KERNEL
#define	u_ar0	U_ar0
#define	u_tsize	U_tsize
#define	u_dsize	U_dsize
#define	u_ssize	U_ssize
#define	u_sig	U_sig
#define	u_code	U_code
#endif /* KERNEL */
#endif /* _SYS_USER_H_ */
