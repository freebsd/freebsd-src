/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1994-1996 Søren Schmidt
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

#ifndef _I386_LINUX_H_
#define	_I386_LINUX_H_

#include <sys/abi_compat.h>

#include <compat/linux/linux.h>
#include <i386/linux/linux_syscall.h>

#define LINUX_LEGACY_SYSCALLS

#define	LINUX_DTRACE	linuxulator

/*
 * Provide a separate set of types for the Linux types.
 */
typedef int		l_int;
typedef int32_t		l_long;
typedef int64_t		l_longlong;
typedef short		l_short;
typedef unsigned int	l_uint;
typedef uint32_t	l_ulong;
typedef uint64_t	l_ulonglong;
typedef unsigned short	l_ushort;

typedef char		*l_caddr_t;
typedef l_ulong		l_uintptr_t;
typedef l_long		l_clock_t;
typedef l_int		l_daddr_t;
typedef l_uint		l_gid_t;
typedef l_ushort	l_gid16_t;
typedef l_ulong		l_ino_t;
typedef l_int		l_key_t;
typedef l_longlong	l_loff_t;
typedef l_ushort	l_mode_t;
typedef l_long		l_off_t;
typedef l_int		l_pid_t;
typedef l_uint		l_size_t;
typedef l_long		l_suseconds_t;
typedef l_long		l_time_t;
typedef l_longlong	l_time64_t;
typedef l_uint		l_uid_t;
typedef l_ushort	l_uid16_t;
typedef l_int		l_timer_t;
typedef l_int		l_mqd_t;
typedef	l_ulong		l_fd_mask;

#include <compat/linux/linux_siginfo.h>

typedef struct {
	l_int		val[2];
} l_fsid_t;

typedef struct {
	l_time_t	tv_sec;
	l_suseconds_t	tv_usec;
} l_timeval;

typedef struct {
	l_time64_t	tv_sec;
	l_time64_t	tv_usec;
} l_sock_timeval;

#define	l_fd_set	fd_set

/*
 * Miscellaneous
 */
#define LINUX_AT_COUNT		22	/* Count of used aux entry types.
					 * Keep this synchronized with
					 * linux_copyout_auxargs() code.
					 */
struct l___sysctl_args
{
	l_int		*name;
	l_int		nlen;
	void		*oldval;
	l_size_t	*oldlenp;
	void		*newval;
	l_size_t	newlen;
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
	l_ulong rlim_cur;
	l_ulong rlim_max;
};

struct l_mmap_argv {
	l_uintptr_t	addr;
	l_size_t	len;
	l_int		prot;
	l_int		flags;
	l_int		fd;
	l_off_t		pgoff;
};

/*
 * stat family of syscalls
 */
struct l_timespec {
	l_time_t	tv_sec;
	l_long		tv_nsec;
};

/* __kernel_timespec */
struct l_timespec64 {
	l_time64_t	tv_sec;
	l_longlong	tv_nsec;
};

struct l_newstat {
	l_ulong		st_dev;
	l_ulong		st_ino;
	l_ushort	st_mode;
	l_ushort	st_nlink;
	l_ushort	st_uid;
	l_ushort	st_gid;
	l_ulong		st_rdev;
	l_ulong		st_size;
	l_ulong		st_blksize;
	l_ulong		st_blocks;
	struct l_timespec	st_atim;
	struct l_timespec	st_mtim;
	struct l_timespec	st_ctim;
	l_ulong		__unused4;
	l_ulong		__unused5;
};

/* __old_kernel_stat now */
struct l_old_stat {
	l_ushort	st_dev;
	l_ulong		st_ino;
	l_ushort	st_mode;
	l_ushort	st_nlink;
	l_ushort	st_uid;
	l_ushort	st_gid;
	l_ushort	st_rdev;
	l_long		st_size;
	struct l_timespec	st_atim;
	struct l_timespec	st_mtim;
	struct l_timespec	st_ctim;
	l_long		st_blksize;
	l_long		st_blocks;
	l_ulong		st_flags;
	l_ulong		st_gen;
};

struct l_stat64 {
	l_ulonglong	st_dev;
	u_char		__pad0[4];
	l_ulong		__st_ino;
	l_uint		st_mode;
	l_uint		st_nlink;
	l_ulong		st_uid;
	l_ulong		st_gid;
	l_ulonglong	st_rdev;
	u_char		__pad3[4];
	l_longlong	st_size;
	l_ulong		st_blksize;
	l_ulonglong	st_blocks;
	struct l_timespec	st_atim;
	struct l_timespec	st_mtim;
	struct l_timespec	st_ctim;
	l_ulonglong	st_ino;
};

struct l_statfs64 {
	l_int		f_type;
	l_int		f_bsize;
	uint64_t	f_blocks;
	uint64_t	f_bfree;
	uint64_t	f_bavail;
	uint64_t	f_files;
	uint64_t	f_ffree;
	l_fsid_t	f_fsid;
	l_int		f_namelen;
	l_int		f_frsize;
	l_int		f_flags;
	l_int		f_spare[4];
};

/* sigaction flags */
#define	LINUX_SA_NOCLDSTOP	0x00000001
#define	LINUX_SA_NOCLDWAIT	0x00000002
#define	LINUX_SA_SIGINFO	0x00000004
#define	LINUX_SA_RESTORER	0x04000000
#define	LINUX_SA_ONSTACK	0x08000000
#define	LINUX_SA_RESTART	0x10000000
#define	LINUX_SA_INTERRUPT	0x20000000
#define	LINUX_SA_NOMASK		0x40000000
#define	LINUX_SA_ONESHOT	0x80000000

/* sigaltstack */
#define	LINUX_MINSIGSTKSZ	2048

typedef void	(*l_handler_t)(l_int);
typedef l_ulong	l_osigset_t;

typedef struct {
	l_handler_t	lsa_handler;
	l_osigset_t	lsa_mask;
	l_ulong		lsa_flags;
	void	(*lsa_restorer)(void);
} l_osigaction_t;

typedef struct {
	l_handler_t	lsa_handler;
	l_ulong		lsa_flags;
	void	(*lsa_restorer)(void);
	l_sigset_t	lsa_mask;
} l_sigaction_t;

typedef struct {
	l_uintptr_t	ss_sp;
	l_int		ss_flags;
	l_size_t	ss_size;
} l_stack_t;

extern struct sysentvec linux_sysvec;

/*
 * arch specific open/fcntl flags
 */
#define	LINUX_F_GETLK64		12
#define	LINUX_F_SETLK64		13
#define	LINUX_F_SETLKW64	14

union l_semun {
	l_int		val;
	l_uintptr_t	buf;
	l_ushort	*array;
	l_uintptr_t	__buf;
	l_uintptr_t	__pad;
};

struct l_user_desc {
	l_uint		entry_number;
	l_uint		base_addr;
	l_uint		limit;
	l_uint		seg_32bit:1;
	l_uint		contents:2;
	l_uint		read_exec_only:1;
	l_uint		limit_in_pages:1;
	l_uint		seg_not_present:1;
	l_uint		useable:1;
};

struct l_desc_struct {
	unsigned long	a, b;
};

#define	LINUX_LOWERWORD	0x0000ffff

/*
 * Macros which does the same thing as those in Linux include/asm-um/ldt-i386.h.
 * These convert Linux user space descriptor to machine one.
 */
#define	LINUX_LDT_entry_a(info)					\
	((((info)->base_addr & LINUX_LOWERWORD) << 16) |	\
	((info)->limit & LINUX_LOWERWORD))

#define	LINUX_ENTRY_B_READ_EXEC_ONLY	9
#define	LINUX_ENTRY_B_CONTENTS		10
#define	LINUX_ENTRY_B_SEG_NOT_PRESENT	15
#define	LINUX_ENTRY_B_BASE_ADDR		16
#define	LINUX_ENTRY_B_USEABLE		20
#define	LINUX_ENTRY_B_SEG32BIT		22
#define	LINUX_ENTRY_B_LIMIT		23

#define	LINUX_LDT_entry_b(info)							\
	(((info)->base_addr & 0xff000000) |					\
	((info)->limit & 0xf0000) |						\
	((info)->contents << LINUX_ENTRY_B_CONTENTS) |				\
	(((info)->seg_not_present == 0) << LINUX_ENTRY_B_SEG_NOT_PRESENT) |	\
	(((info)->base_addr & 0x00ff0000) >> LINUX_ENTRY_B_BASE_ADDR) |		\
	(((info)->read_exec_only == 0) << LINUX_ENTRY_B_READ_EXEC_ONLY) |	\
	((info)->seg_32bit << LINUX_ENTRY_B_SEG32BIT) |				\
	((info)->useable << LINUX_ENTRY_B_USEABLE) |				\
	((info)->limit_in_pages << LINUX_ENTRY_B_LIMIT) | 0x7000)

#define	LINUX_LDT_empty(info)		\
	((info)->base_addr == 0 &&	\
	(info)->limit == 0 &&		\
	(info)->contents == 0 &&	\
	(info)->seg_not_present == 1 &&	\
	(info)->read_exec_only == 1 &&	\
	(info)->seg_32bit == 0 &&	\
	(info)->limit_in_pages == 0 &&	\
	(info)->useable == 0)

/*
 * Macros for converting segments.
 * They do the same as those in arch/i386/kernel/process.c in Linux.
 */
#define	LINUX_GET_BASE(desc)				\
	((((desc)->a >> 16) & LINUX_LOWERWORD) |	\
	(((desc)->b << 16) & 0x00ff0000) |		\
	((desc)->b & 0xff000000))

#define	LINUX_GET_LIMIT(desc)			\
	(((desc)->a & LINUX_LOWERWORD) |	\
	((desc)->b & 0xf0000))

#define	LINUX_GET_32BIT(desc)		\
	(((desc)->b >> LINUX_ENTRY_B_SEG32BIT) & 1)
#define	LINUX_GET_CONTENTS(desc)	\
	(((desc)->b >> LINUX_ENTRY_B_CONTENTS) & 3)
#define	LINUX_GET_WRITABLE(desc)	\
	(((desc)->b >> LINUX_ENTRY_B_READ_EXEC_ONLY) & 1)
#define	LINUX_GET_LIMIT_PAGES(desc)	\
	(((desc)->b >> LINUX_ENTRY_B_LIMIT) & 1)
#define	LINUX_GET_PRESENT(desc)		\
	(((desc)->b >> LINUX_ENTRY_B_SEG_NOT_PRESENT) & 1)
#define	LINUX_GET_USEABLE(desc)		\
	(((desc)->b >> LINUX_ENTRY_B_USEABLE) & 1)

#define	linux_copyout_rusage(r, u)	copyout(r, u, sizeof(*r))

/* This corresponds to 'struct user_regs_struct' in Linux. */
struct linux_pt_regset {
	l_uint ebx;
	l_uint ecx;
	l_uint edx;
	l_uint esi;
	l_uint edi;
	l_uint ebp;
	l_uint eax;
	l_uint ds;
	l_uint es;
	l_uint fs;
	l_uint gs;
	l_uint orig_eax;
	l_uint eip;
	l_uint cs;
	l_uint eflags;
	l_uint esp;
	l_uint ss;
};

#ifdef _KERNEL
struct reg;

void	bsd_to_linux_regset(const struct reg *b_reg,
	    struct linux_pt_regset *l_regset);
#endif /* _KERNEL */

#endif /* !_I386_LINUX_H_ */
