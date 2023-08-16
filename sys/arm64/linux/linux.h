/*-
 * Copyright (c) 1994-1996 Søren Schmidt
 * Copyright (c) 2013 Dmitry Chagin <dchagin@FreeBSD.org>
 * Copyright (c) 2018 Turing Robotic Industries Inc.
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

/*
 */
#ifndef _ARM64_LINUX_H_
#define	_ARM64_LINUX_H_

#include <sys/abi_compat.h>

#include <compat/linux/linux.h>
#include <arm64/linux/linux_syscall.h>

#define	LINUX_DTRACE	linuxulator

/* Provide a separate set of types for the Linux types */
typedef int32_t		l_int;
typedef int64_t		l_long;
typedef int16_t		l_short;
typedef uint32_t	l_uint;
typedef uint64_t	l_ulong;
typedef uint16_t	l_ushort;

typedef l_ulong		l_uintptr_t;
typedef l_long		l_clock_t;
typedef l_int		l_daddr_t;
typedef l_uint		l_gid_t;
typedef l_ushort	l_gid16_t;	/* XXX */
typedef l_uint		l_uid_t;
typedef l_ushort	l_uid16_t;	/* XXX */
typedef l_ulong		l_ino_t;
typedef l_int		l_key_t;
typedef l_long		l_loff_t;
typedef l_uint		l_mode_t;
typedef l_long		l_off_t;
typedef l_int		l_pid_t;
typedef l_ulong		l_size_t;
typedef l_long		l_suseconds_t;
typedef l_long		l_time_t;
typedef l_int		l_timer_t;	/* XXX */
typedef l_int		l_mqd_t;
typedef l_ulong		l_fd_mask;

#include <compat/linux/linux_siginfo.h>

typedef struct {
	l_int		val[2];
} l_fsid_t;

typedef struct {
	l_time_t	tv_sec;
	l_suseconds_t	tv_usec;
} l_timeval;

#define	l_fd_set	fd_set

/* Miscellaneous */
#define	LINUX_AT_COUNT		21	/* Count of used aux entry types.
					 * Keep this synchronized with
					 * linux_copyout_auxargs() code.
					 */

struct l___sysctl_args
{
	l_uintptr_t	name;
	l_int		nlen;
	l_uintptr_t	oldval;
	l_uintptr_t	oldlenp;
	l_uintptr_t	newval;
	l_uintptr_t	newlen;
	l_ulong		__spare[4];
};

/* Resource limits */
#define	LINUX_RLIMIT_CPU	0
#define	LINUX_RLIMIT_FSIZE	1
#define	LINUX_RLIMIT_DATA	2
#define	LINUX_RLIMIT_STACK	3
#define	LINUX_RLIMIT_CORE	4
#define	LINUX_RLIMIT_RSS	5
#define	LINUX_RLIMIT_NPROC	6
#define	LINUX_RLIMIT_NOFILE	7
#define	LINUX_RLIMIT_MEMLOCK	8
#define	LINUX_RLIMIT_AS		9	/* Address space limit */

#define	LINUX_RLIM_NLIMITS	10

struct l_rlimit {
	l_ulong		rlim_cur;
	l_ulong		rlim_max;
};

/* stat family of syscalls */
struct l_timespec {
	l_time_t	tv_sec;
	l_long		tv_nsec;
};

#define	LINUX_O_DIRECTORY	000040000	/* Must be a directory */
#define	LINUX_O_NOFOLLOW	000100000	/* Do not follow links */
#define	LINUX_O_DIRECT		000200000	/* Direct disk access hint */
#define	LINUX_O_LARGEFILE	000400000

struct l_newstat {
	l_ulong		st_dev;
	l_ino_t		st_ino;
	l_uint		st_mode;
	l_uint		st_nlink;

	l_uid_t		st_uid;
	l_gid_t		st_gid;

	l_ulong		st_rdev;
	l_ulong		__st_pad1;
	l_off_t		st_size;
	l_int		st_blksize;
	l_int		__st_pad2;
	l_long		st_blocks;

	struct l_timespec	st_atim;
	struct l_timespec	st_mtim;
	struct l_timespec	st_ctim;
	l_uint		__unused1;
	l_uint		__unused2;
};

/* sigaction flags */
#define	LINUX_SA_NOCLDSTOP	0x00000001
#define	LINUX_SA_NOCLDWAIT	0x00000002
#define	LINUX_SA_SIGINFO	0x00000004
#define	LINUX_SA_RESTORER	0x04000000
#define	LINUX_SA_ONSTACK	0x08000000
#define	LINUX_SA_RESTART	0x10000000
#define	LINUX_SA_INTERRUPT	0x20000000	/* XXX */
#define	LINUX_SA_NOMASK		0x40000000	/* SA_NODEFER */
#define	LINUX_SA_ONESHOT	0x80000000	/* SA_RESETHAND */

typedef void	(*l_handler_t)(l_int);

typedef struct {
	l_handler_t	lsa_handler;
	l_ulong		lsa_flags;
	l_uintptr_t	lsa_restorer;
	l_sigset_t	lsa_mask;
} l_sigaction_t;				/* XXX */

typedef struct {
	l_uintptr_t	ss_sp;
	l_int		ss_flags;
	l_size_t	ss_size;
} l_stack_t;

union l_semun {
	l_int		val;
	l_uintptr_t	buf;
	l_uintptr_t	array;
	l_uintptr_t	__buf;
	l_uintptr_t	__pad;
};

#define	linux_copyout_rusage(r, u)	copyout(r, u, sizeof(*r))

struct linux_pt_regset {
	l_ulong x[31];
	l_ulong sp;
	l_ulong pc;
	l_ulong cpsr;
};

#ifdef _KERNEL
struct reg;
struct syscall_info;

void	bsd_to_linux_regset(const struct reg *b_reg,
	    struct linux_pt_regset *l_regset);
void	linux_to_bsd_regset(struct reg *b_reg,
	    const struct linux_pt_regset *l_regset);
void	linux_ptrace_get_syscall_info_machdep(const struct reg *reg,
	    struct syscall_info *si);
int	linux_ptrace_getregs_machdep(struct thread *td, pid_t pid,
	    struct linux_pt_regset *l_regset);
int	linux_ptrace_peekuser(struct thread *td, pid_t pid,
	    void *addr, void *data);
int	linux_ptrace_pokeuser(struct thread *td, pid_t pid,
	    void *addr, void *data);
#endif /* _KERNEL */

#endif /* _ARM64_LINUX_H_ */
