/*-
 * Copyright (c) 1984, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)ptrace.h	8.2 (Berkeley) 1/4/94
 * $FreeBSD$
 */

#ifndef	_SYS_PTRACE_H_
#define	_SYS_PTRACE_H_

#define	PT_TRACE_ME	0	/* child declares it's being traced */
#define	PT_READ_I	1	/* read word in child's I space */
#define	PT_READ_D	2	/* read word in child's D space */
/* was	PT_READ_U	3	 * read word in child's user structure */
#define	PT_WRITE_I	4	/* write word in child's I space */
#define	PT_WRITE_D	5	/* write word in child's D space */
/* was	PT_WRITE_U	6	 * write word in child's user structure */
#define	PT_CONTINUE	7	/* continue the child */
#define	PT_KILL		8	/* kill the child process */
#define	PT_STEP		9	/* single step the child */

#define	PT_ATTACH	10	/* trace some running process */
#define	PT_DETACH	11	/* stop tracing a process */
#define	PT_IO		12	/* do I/O to/from stopped process. */

#define	PT_FIRSTMACH	32	/* for machine-specific requests */
#include <machine/ptrace.h>	/* machine-specific requests, if any */

struct ptrace_io_desc {
	int	piod_op;	/* I/O operation */
	void	*piod_offs;	/* child offset */
	void	*piod_addr;	/* parent offset */
	size_t	piod_len;	/* request length */
};

/*
 *  * Operations in piod_op.
 *   */
#define	PIOD_READ_D	1	/* Read from D space */
#define	PIOD_WRITE_D	2	/* Write to D space */
#define	PIOD_READ_I	3	/* Read from I space */
#define	PIOD_WRITE_I	4	/* Write to I space */

#ifdef _KERNEL
void	proc_reparent __P((struct proc *child, struct proc *newparent));
int	ptrace_set_pc __P((struct proc *p, unsigned long addr));
int	ptrace_single_step __P((struct proc *p));
int	ptrace_write_u __P((struct proc *p, vm_offset_t off, long data));
int	kern_ptrace __P((struct proc *p, int req, pid_t pid, void *addr,
		int data));
#else /* !_KERNEL */

#include <sys/cdefs.h>

__BEGIN_DECLS
int	ptrace __P((int _request, pid_t _pid, caddr_t _addr, int _data));
__END_DECLS

#endif /* !_KERNEL */

#endif	/* !_SYS_PTRACE_H_ */
