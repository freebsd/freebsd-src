/*-
 * Copyright (c) 2021 Dmitry Chagin <dchagin@FreeBSD.org>
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

#ifndef _LINUX_FORK_H_
#define _LINUX_FORK_H_

#define	LINUX_CLONE_VM			0x00000100
#define	LINUX_CLONE_FS			0x00000200
#define	LINUX_CLONE_FILES		0x00000400
#define	LINUX_CLONE_SIGHAND		0x00000800
#define	LINUX_CLONE_PIDFD		0x00001000	/* since Linux 5.2 */
#define	LINUX_CLONE_PTRACE		0x00002000
#define	LINUX_CLONE_VFORK		0x00004000
#define	LINUX_CLONE_PARENT		0x00008000
#define	LINUX_CLONE_THREAD		0x00010000
#define	LINUX_CLONE_NEWNS		0x00020000	/* New mount NS */
#define	LINUX_CLONE_SYSVSEM		0x00040000
#define	LINUX_CLONE_SETTLS		0x00080000
#define	LINUX_CLONE_PARENT_SETTID	0x00100000
#define	LINUX_CLONE_CHILD_CLEARTID	0x00200000
#define	LINUX_CLONE_DETACHED		0x00400000	/* Unused */
#define	LINUX_CLONE_UNTRACED		0x00800000
#define	LINUX_CLONE_CHILD_SETTID	0x01000000
#define	LINUX_CLONE_NEWCGROUP		0x02000000	/* New cgroup NS */
#define	LINUX_CLONE_NEWUTS		0x04000000
#define	LINUX_CLONE_NEWIPC		0x08000000
#define	LINUX_CLONE_NEWUSER		0x10000000
#define	LINUX_CLONE_NEWPID		0x20000000
#define	LINUX_CLONE_NEWNET		0x40000000
#define	LINUX_CLONE_IO			0x80000000

/* Flags for the clone3() syscall. */
#define	LINUX_CLONE_CLEAR_SIGHAND	0x100000000ULL
#define	LINUX_CLONE_INTO_CGROUP		0x200000000ULL
#define	LINUX_CLONE_NEWTIME		0x00000080

#define	LINUX_CLONE_LEGACY_FLAGS	0xffffffffULL

#define	LINUX_CSIGNAL			0x000000ff

/*
 * User-space clone3 args layout.
 */
struct l_user_clone_args {
	uint64_t flags;
	uint64_t pidfd;
	uint64_t child_tid;
	uint64_t parent_tid;
	uint64_t exit_signal;
	uint64_t stack;
	uint64_t stack_size;
	uint64_t tls;
	uint64_t set_tid;
	uint64_t set_tid_size;
	uint64_t cgroup;
};

/*
 * Kernel clone3 args layout.
 */
struct l_clone_args {
	uint64_t flags;
	l_int *child_tid;
	l_int *parent_tid;
	l_int exit_signal;
	l_ulong stack;
	l_ulong stack_size;
	l_ulong tls;
};

#define	LINUX_CLONE_ARGS_SIZE_VER0	64

int linux_set_upcall(struct thread *, register_t);
int linux_set_cloned_tls(struct thread *, void *);
void linux_thread_detach(struct thread *);

#endif /* _LINUX_FORK_H_ */
