/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017 Edward Tomasz Napierala <trasz@FreeBSD.org>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/sx.h>
#include <sys/syscallsubr.h>

#include <machine/../linux/linux.h>
#include <machine/../linux/linux_proto.h>
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_errno.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_signal.h>
#include <compat/linux/linux_util.h>

#define	LINUX_PTRACE_TRACEME		0
#define	LINUX_PTRACE_PEEKTEXT		1
#define	LINUX_PTRACE_PEEKDATA		2
#define	LINUX_PTRACE_PEEKUSER		3
#define	LINUX_PTRACE_POKETEXT		4
#define	LINUX_PTRACE_POKEDATA		5
#define	LINUX_PTRACE_POKEUSER		6
#define	LINUX_PTRACE_CONT		7
#define	LINUX_PTRACE_KILL		8
#define	LINUX_PTRACE_SINGLESTEP		9
#define	LINUX_PTRACE_GETREGS		12
#define	LINUX_PTRACE_SETREGS		13
#define	LINUX_PTRACE_GETFPREGS		14
#define	LINUX_PTRACE_SETFPREGS		15
#define	LINUX_PTRACE_ATTACH		16
#define	LINUX_PTRACE_DETACH		17
#define	LINUX_PTRACE_SYSCALL		24
#define	LINUX_PTRACE_SETOPTIONS		0x4200
#define	LINUX_PTRACE_GETEVENTMSG	0x4201
#define	LINUX_PTRACE_GETSIGINFO		0x4202
#define	LINUX_PTRACE_GETREGSET		0x4204
#define	LINUX_PTRACE_SEIZE		0x4206
#define	LINUX_PTRACE_GET_SYSCALL_INFO	0x420e

#define	LINUX_PTRACE_EVENT_EXEC		4
#define	LINUX_PTRACE_EVENT_EXIT		6

#define	LINUX_PTRACE_O_TRACESYSGOOD	1
#define	LINUX_PTRACE_O_TRACEFORK	2
#define	LINUX_PTRACE_O_TRACEVFORK	4
#define	LINUX_PTRACE_O_TRACECLONE	8
#define	LINUX_PTRACE_O_TRACEEXEC	16
#define	LINUX_PTRACE_O_TRACEVFORKDONE	32
#define	LINUX_PTRACE_O_TRACEEXIT	64
#define	LINUX_PTRACE_O_TRACESECCOMP	128
#define	LINUX_PTRACE_O_EXITKILL		1048576
#define	LINUX_PTRACE_O_SUSPEND_SECCOMP	2097152

#define	LINUX_NT_PRSTATUS		0x1
#define	LINUX_NT_PRFPREG		0x2
#define	LINUX_NT_X86_XSTATE		0x202

#define	LINUX_PTRACE_O_MASK	(LINUX_PTRACE_O_TRACESYSGOOD |	\
    LINUX_PTRACE_O_TRACEFORK | LINUX_PTRACE_O_TRACEVFORK |	\
    LINUX_PTRACE_O_TRACECLONE | LINUX_PTRACE_O_TRACEEXEC |	\
    LINUX_PTRACE_O_TRACEVFORKDONE | LINUX_PTRACE_O_TRACEEXIT |	\
    LINUX_PTRACE_O_TRACESECCOMP | LINUX_PTRACE_O_EXITKILL |	\
    LINUX_PTRACE_O_SUSPEND_SECCOMP)

#define	LINUX_PTRACE_SYSCALL_INFO_NONE	0
#define	LINUX_PTRACE_SYSCALL_INFO_ENTRY	1
#define	LINUX_PTRACE_SYSCALL_INFO_EXIT	2

static int
map_signum(int lsig, int *bsigp)
{
	int bsig;

	if (lsig == 0) {
		*bsigp = 0;
		return (0);
	}

	if (lsig < 0 || lsig > LINUX_SIGRTMAX)
		return (EINVAL);

	bsig = linux_to_bsd_signal(lsig);
	if (bsig == SIGSTOP)
		bsig = 0;

	*bsigp = bsig;
	return (0);
}

int
linux_ptrace_status(struct thread *td, pid_t pid, int status)
{
	struct ptrace_lwpinfo lwpinfo;
	struct linux_pemuldata *pem;
	register_t saved_retval;
	int error;

	saved_retval = td->td_retval[0];
	error = kern_ptrace(td, PT_LWPINFO, pid, &lwpinfo, sizeof(lwpinfo));
	td->td_retval[0] = saved_retval;
	if (error != 0) {
		linux_msg(td, "PT_LWPINFO failed with error %d", error);
		return (status);
	}

	pem = pem_find(td->td_proc);
	KASSERT(pem != NULL, ("%s: proc emuldata not found.\n", __func__));

	LINUX_PEM_SLOCK(pem);
	if ((pem->ptrace_flags & LINUX_PTRACE_O_TRACESYSGOOD) &&
	    lwpinfo.pl_flags & PL_FLAG_SCE)
		status |= (LINUX_SIGTRAP | 0x80) << 8;
	if ((pem->ptrace_flags & LINUX_PTRACE_O_TRACESYSGOOD) &&
	    lwpinfo.pl_flags & PL_FLAG_SCX) {
		if (lwpinfo.pl_flags & PL_FLAG_EXEC)
			status |= (LINUX_SIGTRAP | LINUX_PTRACE_EVENT_EXEC << 8) << 8;
		else
			status |= (LINUX_SIGTRAP | 0x80) << 8;
	}
	if ((pem->ptrace_flags & LINUX_PTRACE_O_TRACEEXIT) &&
	    lwpinfo.pl_flags & PL_FLAG_EXITED)
		status |= (LINUX_SIGTRAP | LINUX_PTRACE_EVENT_EXIT << 8) << 8;
	LINUX_PEM_SUNLOCK(pem);

	return (status);
}

static int
linux_ptrace_peek(struct thread *td, pid_t pid, void *addr, void *data)
{
	int error;

	error = kern_ptrace(td, PT_READ_I, pid, addr, 0);
	if (error == 0)
		error = copyout(td->td_retval, data, sizeof(l_int));
	else if (error == ENOMEM)
		error = EIO;
	td->td_retval[0] = error;

	return (error);
}

static int
linux_ptrace_setoptions(struct thread *td, pid_t pid, l_ulong data)
{
	struct linux_pemuldata *pem;
	int mask;

	mask = 0;

	if (data & ~LINUX_PTRACE_O_MASK) {
		linux_msg(td, "unknown ptrace option %lx set; "
		    "returning EINVAL",
		    data & ~LINUX_PTRACE_O_MASK);
		return (EINVAL);
	}

	pem = pem_find(td->td_proc);
	KASSERT(pem != NULL, ("%s: proc emuldata not found.\n", __func__));

	/*
	 * PTRACE_O_EXITKILL is ignored, we do that by default.
	 */

	LINUX_PEM_XLOCK(pem);
	if (data & LINUX_PTRACE_O_TRACESYSGOOD) {
		pem->ptrace_flags |= LINUX_PTRACE_O_TRACESYSGOOD;
	} else {
		pem->ptrace_flags &= ~LINUX_PTRACE_O_TRACESYSGOOD;
	}
	LINUX_PEM_XUNLOCK(pem);

	if (data & LINUX_PTRACE_O_TRACEFORK)
		mask |= PTRACE_FORK;

	if (data & LINUX_PTRACE_O_TRACEVFORK)
		mask |= PTRACE_VFORK;

	if (data & LINUX_PTRACE_O_TRACECLONE)
		mask |= PTRACE_VFORK;

	if (data & LINUX_PTRACE_O_TRACEEXEC)
		mask |= PTRACE_EXEC;

	if (data & LINUX_PTRACE_O_TRACEVFORKDONE)
		mask |= PTRACE_VFORK; /* XXX: Close enough? */

	if (data & LINUX_PTRACE_O_TRACEEXIT) {
		pem->ptrace_flags |= LINUX_PTRACE_O_TRACEEXIT;
	} else {
		pem->ptrace_flags &= ~LINUX_PTRACE_O_TRACEEXIT;
	}

	return (kern_ptrace(td, PT_SET_EVENT_MASK, pid, &mask, sizeof(mask)));
}

static int
linux_ptrace_geteventmsg(struct thread *td, pid_t pid, l_ulong data)
{

	linux_msg(td, "PTRACE_GETEVENTMSG not implemented; returning EINVAL");
	return (EINVAL);
}

static int
linux_ptrace_getsiginfo(struct thread *td, pid_t pid, l_ulong data)
{
	struct ptrace_lwpinfo lwpinfo;
	l_siginfo_t l_siginfo;
	int error, sig;

	error = kern_ptrace(td, PT_LWPINFO, pid, &lwpinfo, sizeof(lwpinfo));
	if (error != 0) {
		linux_msg(td, "PT_LWPINFO failed with error %d", error);
		return (error);
	}

	if ((lwpinfo.pl_flags & PL_FLAG_SI) == 0) {
		error = EINVAL;
		linux_msg(td, "no PL_FLAG_SI, returning %d", error);
		return (error);
	}

	sig = bsd_to_linux_signal(lwpinfo.pl_siginfo.si_signo);
	memset(&l_siginfo, 0, sizeof(l_siginfo));
	siginfo_to_lsiginfo(&lwpinfo.pl_siginfo, &l_siginfo, sig);
	error = copyout(&l_siginfo, (void *)data, sizeof(l_siginfo));
	return (error);
}

static int
linux_ptrace_getregs(struct thread *td, pid_t pid, void *data)
{
	struct reg b_reg;
	struct linux_pt_regset l_regset;
	int error;

	error = kern_ptrace(td, PT_GETREGS, pid, &b_reg, 0);
	if (error != 0)
		return (error);

	bsd_to_linux_regset(&b_reg, &l_regset);
	error = linux_ptrace_getregs_machdep(td, pid, &l_regset);
	if (error != 0)
		return (error);

	error = copyout(&l_regset, (void *)data, sizeof(l_regset));
	return (error);
}

static int
linux_ptrace_setregs(struct thread *td, pid_t pid, void *data)
{
	struct reg b_reg;
	struct linux_pt_regset l_regset;
	int error;

	error = copyin(data, &l_regset, sizeof(l_regset));
	if (error != 0)
		return (error);
	linux_to_bsd_regset(&b_reg, &l_regset);
	error = kern_ptrace(td, PT_SETREGS, pid, &b_reg, 0);
	return (error);
}

static int
linux_ptrace_getregset_prstatus(struct thread *td, pid_t pid, l_ulong data)
{
	struct reg b_reg;
	struct linux_pt_regset l_regset;
	struct iovec iov;
	size_t len;
	int error;

	error = copyin((const void *)data, &iov, sizeof(iov));
	if (error != 0) {
		linux_msg(td, "copyin error %d", error);
		return (error);
	}

	error = kern_ptrace(td, PT_GETREGS, pid, &b_reg, 0);
	if (error != 0)
		return (error);

	bsd_to_linux_regset(&b_reg, &l_regset);
	error = linux_ptrace_getregs_machdep(td, pid, &l_regset);
	if (error != 0)
		return (error);

	len = MIN(iov.iov_len, sizeof(l_regset));
	error = copyout(&l_regset, (void *)iov.iov_base, len);
	if (error != 0) {
		linux_msg(td, "copyout error %d", error);
		return (error);
	}

	iov.iov_len = len;
	error = copyout(&iov, (void *)data, sizeof(iov));
	if (error != 0) {
		linux_msg(td, "iov copyout error %d", error);
		return (error);
	}

	return (error);
}

static int
linux_ptrace_getregset(struct thread *td, pid_t pid, l_ulong addr, l_ulong data)
{

	switch (addr) {
	case LINUX_NT_PRSTATUS:
		return (linux_ptrace_getregset_prstatus(td, pid, data));
	case LINUX_NT_PRFPREG:
		linux_msg(td, "PTRAGE_GETREGSET NT_PRFPREG not implemented; "
		    "returning EINVAL");
		return (EINVAL);
	case LINUX_NT_X86_XSTATE:
		linux_msg(td, "PTRAGE_GETREGSET NT_X86_XSTATE not implemented; "
		    "returning EINVAL");
		return (EINVAL);
	default:
		linux_msg(td, "PTRACE_GETREGSET request %#lx not implemented; "
		    "returning EINVAL", addr);
		return (EINVAL);
	}
}

static int
linux_ptrace_seize(struct thread *td, pid_t pid, l_ulong addr, l_ulong data)
{

	linux_msg(td, "PTRACE_SEIZE not implemented; returning EINVAL");
	return (EINVAL);
}

static int
linux_ptrace_get_syscall_info(struct thread *td, pid_t pid,
    l_ulong len, l_ulong data)
{
	struct ptrace_lwpinfo lwpinfo;
	struct ptrace_sc_ret sr;
	struct reg b_reg;
	struct syscall_info si;
	int error;

	error = kern_ptrace(td, PT_LWPINFO, pid, &lwpinfo, sizeof(lwpinfo));
	if (error != 0) {
		linux_msg(td, "PT_LWPINFO failed with error %d", error);
		return (error);
	}

	memset(&si, 0, sizeof(si));

	if (lwpinfo.pl_flags & PL_FLAG_SCE) {
		si.op = LINUX_PTRACE_SYSCALL_INFO_ENTRY;
		si.entry.nr = lwpinfo.pl_syscall_code;
		/*
		 * The use of PT_GET_SC_ARGS there is special,
		 * implementation of PT_GET_SC_ARGS for Linux-ABI
		 * callers emulates Linux bug which strace(1) depends
		 * on: at initialization it tests whether ptrace works
		 * by calling close(2), or some other single-argument
		 * syscall, _with six arguments_, and then verifies
		 * whether it can fetch them all using this API;
		 * otherwise it bails out.
		 */
		error = kern_ptrace(td, PT_GET_SC_ARGS, pid,
		    &si.entry.args, sizeof(si.entry.args));
		if (error != 0) {
			linux_msg(td, "PT_GET_SC_ARGS failed with error %d",
			    error);
			return (error);
		}
	} else if (lwpinfo.pl_flags & PL_FLAG_SCX) {
		si.op = LINUX_PTRACE_SYSCALL_INFO_EXIT;
		error = kern_ptrace(td, PT_GET_SC_RET, pid, &sr, sizeof(sr));

		if (error != 0) {
			linux_msg(td, "PT_GET_SC_RET failed with error %d",
			    error);
			return (error);
		}

		if (sr.sr_error == 0) {
			si.exit.rval = sr.sr_retval[0];
			si.exit.is_error = 0;
		} else if (sr.sr_error == EJUSTRETURN) {
			/*
			 * EJUSTRETURN means the actual value to return
			 * has already been put into td_frame; instead
			 * of extracting it and trying to determine whether
			 * it's an error or not just bail out and let
			 * the ptracing process fall back to another method.
			 */
			si.op = LINUX_PTRACE_SYSCALL_INFO_NONE;
		} else if (sr.sr_error == ERESTART) {
			si.exit.rval = -LINUX_ERESTARTSYS;
			si.exit.is_error = 1;
		} else {
			si.exit.rval = bsd_to_linux_errno(sr.sr_error);
			si.exit.is_error = 1;
		}
	} else {
		si.op = LINUX_PTRACE_SYSCALL_INFO_NONE;
	}

	error = kern_ptrace(td, PT_GETREGS, pid, &b_reg, 0);
	if (error != 0)
		return (error);

	linux_ptrace_get_syscall_info_machdep(&b_reg, &si);

	len = MIN(len, sizeof(si));
	error = copyout(&si, (void *)data, len);
	if (error == 0)
		td->td_retval[0] = sizeof(si);

	return (error);
}

int
linux_ptrace(struct thread *td, struct linux_ptrace_args *uap)
{
	void *addr;
	pid_t pid;
	int error, sig;

	if (!allow_ptrace)
		return (ENOSYS);

	pid  = (pid_t)uap->pid;
	addr = (void *)uap->addr;

	switch (uap->req) {
	case LINUX_PTRACE_TRACEME:
		error = kern_ptrace(td, PT_TRACE_ME, 0, 0, 0);
		break;
	case LINUX_PTRACE_PEEKTEXT:
	case LINUX_PTRACE_PEEKDATA:
		error = linux_ptrace_peek(td, pid, addr, (void *)uap->data);
		if (error != 0)
			goto out;
		/*
		 * Linux expects this syscall to read 64 bits, not 32.
		 */
		error = linux_ptrace_peek(td, pid,
		    (void *)(uap->addr + 4), (void *)(uap->data + 4));
		break;
	case LINUX_PTRACE_PEEKUSER:
		error = linux_ptrace_peekuser(td, pid, addr, (void *)uap->data);
		break;
	case LINUX_PTRACE_POKETEXT:
	case LINUX_PTRACE_POKEDATA:
		error = kern_ptrace(td, PT_WRITE_D, pid, addr, uap->data);
		if (error != 0)
			goto out;
		/*
		 * Linux expects this syscall to write 64 bits, not 32.
		 */
		error = kern_ptrace(td, PT_WRITE_D, pid,
		    (void *)(uap->addr + 4), uap->data >> 32);
		break;
	case LINUX_PTRACE_POKEUSER:
		error = linux_ptrace_pokeuser(td, pid, addr, (void *)uap->data);
		break;
	case LINUX_PTRACE_CONT:
		error = map_signum(uap->data, &sig);
		if (error != 0)
			break;
		error = kern_ptrace(td, PT_CONTINUE, pid, (void *)1, sig);
		break;
	case LINUX_PTRACE_KILL:
		error = kern_ptrace(td, PT_KILL, pid, addr, uap->data);
		break;
	case LINUX_PTRACE_SINGLESTEP:
		error = map_signum(uap->data, &sig);
		if (error != 0)
			break;
		error = kern_ptrace(td, PT_STEP, pid, (void *)1, sig);
		break;
	case LINUX_PTRACE_GETREGS:
		error = linux_ptrace_getregs(td, pid, (void *)uap->data);
		break;
	case LINUX_PTRACE_SETREGS:
		error = linux_ptrace_setregs(td, pid, (void *)uap->data);
		break;
	case LINUX_PTRACE_ATTACH:
		error = kern_ptrace(td, PT_ATTACH, pid, addr, uap->data);
		break;
	case LINUX_PTRACE_DETACH:
		error = map_signum(uap->data, &sig);
		if (error != 0)
			break;
		error = kern_ptrace(td, PT_DETACH, pid, (void *)1, sig);
		break;
	case LINUX_PTRACE_SYSCALL:
		error = map_signum(uap->data, &sig);
		if (error != 0)
			break;
		error = kern_ptrace(td, PT_SYSCALL, pid, (void *)1, sig);
		break;
	case LINUX_PTRACE_SETOPTIONS:
		error = linux_ptrace_setoptions(td, pid, uap->data);
		break;
	case LINUX_PTRACE_GETEVENTMSG:
		error = linux_ptrace_geteventmsg(td, pid, uap->data);
		break;
	case LINUX_PTRACE_GETSIGINFO:
		error = linux_ptrace_getsiginfo(td, pid, uap->data);
		break;
	case LINUX_PTRACE_GETREGSET:
		error = linux_ptrace_getregset(td, pid, uap->addr, uap->data);
		break;
	case LINUX_PTRACE_SEIZE:
		error = linux_ptrace_seize(td, pid, uap->addr, uap->data);
		break;
	case LINUX_PTRACE_GET_SYSCALL_INFO:
		error = linux_ptrace_get_syscall_info(td, pid, uap->addr, uap->data);
		break;
	default:
		linux_msg(td, "ptrace(%ld, ...) not implemented; "
		    "returning EINVAL", uap->req);
		error = EINVAL;
		break;
	}

out:
	if (error == EBUSY)
		error = ESRCH;

	return (error);
}
