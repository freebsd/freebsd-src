/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Turing Robotic Industries Inc.
 * Copyright (c) 2000 Marcel Moolenaar
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/fcntl.h>
#include <sys/imgact.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/sdt.h>

#include <security/audit/audit.h>

#include <arm64/linux/linux.h>
#include <arm64/linux/linux_proto.h>
#include <compat/linux/linux_dtrace.h>
#include <compat/linux/linux_emul.h>
#include <compat/linux/linux_fork.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_mmap.h>
#include <compat/linux/linux_util.h>

#define	LINUX_ARCH_AARCH64		0xc00000b7

/* DTrace init */
LIN_SDT_PROVIDER_DECLARE(LINUX_DTRACE);

/* DTrace probes */
LIN_SDT_PROBE_DEFINE0(machdep, linux_mmap2, todo);

/*
 * LINUXTODO: deduplicate; linux_execve is common across archs, except that on
 * amd64 compat linuxulator it calls freebsd32_exec_copyin_args.
 */
int
linux_execve(struct thread *td, struct linux_execve_args *uap)
{
	struct image_args eargs;
	char *path;
	int error;

	if (!LUSECONVPATH(td)) {
		error = exec_copyin_args(&eargs, uap->path, UIO_USERSPACE,
		    uap->argp, uap->envp);
	} else {
		LCONVPATHEXIST(td, uap->path, &path);
		error = exec_copyin_args(&eargs, path, UIO_SYSSPACE,
		    uap->argp, uap->envp);
		LFREEPATH(path);
	}
	if (error == 0)
		error = linux_common_execve(td, &eargs);
	AUDIT_SYSCALL_EXIT(error == EJUSTRETURN ? 0 : error, td);
	return (error);
}

int
linux_set_upcall(struct thread *td, register_t stack)
{

	if (stack)
		td->td_frame->tf_sp = stack;

	/*
	 * The newly created Linux thread returns
	 * to the user space by the same path that a parent does.
	 */
	td->td_frame->tf_x[0] = 0;
	return (0);
}

/* LINUXTODO: deduplicate arm64 linux_mmap2 */
int
linux_mmap2(struct thread *td, struct linux_mmap2_args *uap)
{

	LIN_SDT_PROBE0(machdep, linux_mmap2, todo);
	return (linux_mmap_common(td, PTROUT(uap->addr), uap->len, uap->prot,
	    uap->flags, uap->fd, uap->pgoff));
}

int
linux_mprotect(struct thread *td, struct linux_mprotect_args *uap)
{

	return (linux_mprotect_common(td, PTROUT(uap->addr), uap->len,
	    uap->prot));
}

int
linux_madvise(struct thread *td, struct linux_madvise_args *uap)
{

	return (linux_madvise_common(td, PTROUT(uap->addr), uap->len, uap->behav));
}

int
linux_set_cloned_tls(struct thread *td, void *desc)
{

	if ((uint64_t)desc >= VM_MAXUSER_ADDRESS)
		return (EPERM);

	return (cpu_set_user_tls(td, desc));
}

void
bsd_to_linux_regset(const struct reg *b_reg, struct linux_pt_regset *l_regset)
{

	KASSERT(sizeof(l_regset->x) == sizeof(b_reg->x) + sizeof(l_ulong),
	    ("%s: size mismatch\n", __func__));
	memcpy(l_regset->x, b_reg->x, sizeof(b_reg->x));

	l_regset->x[30] = b_reg->lr;
	l_regset->sp = b_reg->sp;
	l_regset->pc = b_reg->elr;
	l_regset->cpsr = b_reg->spsr;
}

void
linux_to_bsd_regset(struct reg *b_reg, const struct linux_pt_regset *l_regset)
{

	KASSERT(sizeof(l_regset->x) == sizeof(b_reg->x) + sizeof(l_ulong),
	    ("%s: size mismatch\n", __func__));

	memcpy(b_reg->x, l_regset->x, sizeof(b_reg->x));
	b_reg->sp = l_regset->sp;
	b_reg->elr = l_regset->pc;
	b_reg->spsr = l_regset->cpsr;
}

void
linux_ptrace_get_syscall_info_machdep(const struct reg *reg,
    struct syscall_info *si)
{

	si->arch = LINUX_ARCH_AARCH64;
	si->instruction_pointer = reg->lr;
	si->stack_pointer = reg->sp;
}

int
linux_ptrace_getregs_machdep(struct thread *td __unused, pid_t pid __unused,
    struct linux_pt_regset *l_regset __unused)
{

	return (0);
}

