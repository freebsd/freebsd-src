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

#include <sys/_sigset.h>
#include <machine/reg.h>

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
#define PT_IO		12	/* do I/O to/from stopped process. */
#define	PT_LWPINFO	13	/* Info about the LWP that stopped. */
#define PT_GETNUMLWPS	14	/* get total number of threads */
#define PT_GETLWPLIST	15	/* get thread list */
#define PT_CLEARSTEP	16	/* turn off single step */
#define PT_SETSTEP	17	/* turn on single step */
#define PT_SUSPEND	18	/* suspend a thread */
#define PT_RESUME	19	/* resume a thread */

#define	PT_TO_SCE	20
#define	PT_TO_SCX	21
#define	PT_SYSCALL	22

#define PT_GETREGS      33	/* get general-purpose registers */
#define PT_SETREGS      34	/* set general-purpose registers */
#define PT_GETFPREGS    35	/* get floating-point registers */
#define PT_SETFPREGS    36	/* set floating-point registers */
#define PT_GETDBREGS    37	/* get debugging registers */
#define PT_SETDBREGS    38	/* set debugging registers */

#define	PT_VM_TIMESTAMP	40	/* Get VM version (timestamp) */
#define	PT_VM_ENTRY	41	/* Get VM map (entry) */

#define PT_FIRSTMACH    64	/* for machine-specific requests */
#include <machine/ptrace.h>	/* machine-specific requests, if any */

struct ptrace_io_desc {
	int	piod_op;	/* I/O operation */
	void	*piod_offs;	/* child offset */
	void	*piod_addr;	/* parent offset */
	size_t	piod_len;	/* request length */
};

/*
 * Operations in piod_op.
 */
#define PIOD_READ_D	1	/* Read from D space */
#define PIOD_WRITE_D	2	/* Write to D space */
#define PIOD_READ_I	3	/* Read from I space */
#define PIOD_WRITE_I	4	/* Write to I space */

/* Argument structure for PT_LWPINFO. */
struct ptrace_lwpinfo {
	lwpid_t	pl_lwpid;	/* LWP described. */
	int	pl_event;	/* Event that stopped the LWP. */
#define	PL_EVENT_NONE	0
#define	PL_EVENT_SIGNAL	1
	int	pl_flags;	/* LWP flags. */
#define	PL_FLAG_SA	0x01	/* M:N thread */
#define	PL_FLAG_BOUND	0x02	/* M:N bound thread */
	sigset_t	pl_sigmask;	/* LWP signal mask */
	sigset_t	pl_siglist;	/* LWP pending signal */
};

/* Argument structure for PT_VM_ENTRY. */
struct ptrace_vm_entry {
	int		pve_entry;	/* Entry number used for iteration. */
	int		pve_timestamp;	/* Generation number of VM map. */
	u_long		pve_start;	/* Start VA of range. */
	u_long		pve_end;	/* End VA of range (incl). */
	u_long		pve_offset;	/* Offset in backing object. */
	u_int		pve_prot;	/* Protection of memory range. */
	u_int		pve_pathlen;	/* Size of path. */
	long		pve_fileid;	/* File ID. */
	uint32_t	pve_fsid;	/* File system ID. */
	char		*pve_path;	/* Path name of object. */
};

#ifdef _KERNEL

#define	PTRACESTOP_SC(p, td, flag)				\
	if ((p)->p_flag & P_TRACED && (p)->p_stops & (flag)) {	\
		PROC_LOCK(p);					\
		ptracestop((td), SIGTRAP);			\
		PROC_UNLOCK(p);					\
	}
/*
 * The flags below are used for ptrace(2) tracing and have no relation
 * to procfs.  They are stored in struct proc's p_stops member.
 */
#define	S_PT_SCE	0x000010000
#define	S_PT_SCX	0x000020000

int	ptrace_set_pc(struct thread *_td, unsigned long _addr);
int	ptrace_single_step(struct thread *_td);
int	ptrace_clear_single_step(struct thread *_td);

#ifdef __HAVE_PTRACE_MACHDEP
int	cpu_ptrace(struct thread *_td, int _req, void *_addr, int _data);
#endif

/*
 * These are prototypes for functions that implement some of the
 * debugging functionality exported by procfs / linprocfs and by the
 * ptrace(2) syscall.  They used to be part of procfs, but they don't
 * really belong there.
 */
struct reg;
struct fpreg;
struct dbreg;
struct uio;
int	proc_read_regs(struct thread *_td, struct reg *_reg);
int	proc_write_regs(struct thread *_td, struct reg *_reg);
int	proc_read_fpregs(struct thread *_td, struct fpreg *_fpreg);
int	proc_write_fpregs(struct thread *_td, struct fpreg *_fpreg);
int	proc_read_dbregs(struct thread *_td, struct dbreg *_dbreg);
int	proc_write_dbregs(struct thread *_td, struct dbreg *_dbreg);
int	proc_sstep(struct thread *_td);
int	proc_rwmem(struct proc *_p, struct uio *_uio);
#ifdef COMPAT_FREEBSD32
struct reg32;
struct fpreg32;
struct dbreg32;
int	proc_read_regs32(struct thread *_td, struct reg32 *_reg32);
int	proc_write_regs32(struct thread *_td, struct reg32 *_reg32);
int	proc_read_fpregs32(struct thread *_td, struct fpreg32 *_fpreg32);
int	proc_write_fpregs32(struct thread *_td, struct fpreg32 *_fpreg32);
int	proc_read_dbregs32(struct thread *_td, struct dbreg32 *_dbreg32);
int	proc_write_dbregs32(struct thread *_td, struct dbreg32 *_dbreg32);
#endif
#else /* !_KERNEL */

#include <sys/cdefs.h>

__BEGIN_DECLS
int	ptrace(int _request, pid_t _pid, caddr_t _addr, int _data);
__END_DECLS

#endif /* !_KERNEL */

#endif	/* !_SYS_PTRACE_H_ */
