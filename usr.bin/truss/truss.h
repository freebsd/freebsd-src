/*
 * Copyright 2001 Jamey Wood
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

#include <sys/linker_set.h>
#include <sys/queue.h>

#define	FOLLOWFORKS		0x00000001
#define	RELATIVETIMESTAMPS	0x00000002
#define	ABSOLUTETIMESTAMPS	0x00000004
#define	NOSIGS			0x00000008
#define	EXECVEARGS		0x00000010
#define	EXECVEENVS		0x00000020
#define	COUNTONLY		0x00000040

struct procinfo;
struct trussinfo;

struct procabi {
	const char *type;
	const char **syscallnames;
	int nsyscalls;
	int (*fetch_args)(struct trussinfo *, u_int);
	int (*fetch_retval)(struct trussinfo *, long *, int *);
};

#define	PROCABI(abi)	DATA_SET(procabi, abi)

/*
 * This is confusingly named.  It holds per-thread state about the
 * currently executing system call.  syscalls.h defines a struct
 * syscall that holds metadata used to format system call arguments.
 *
 * NB: args[] stores the raw argument values (e.g. from registers)
 * passed to the system call.  s_args[] stores a string representation
 * of a system call's arguments.  These do not necessarily map one to
 * one.  A system call description may omit individual arguments
 * (padding) or combine adjacent arguments (e.g. when passing an off_t
 * argument on a 32-bit system).  The nargs member contains the count
 * of valid pointers in s_args[], not args[].
 */
struct current_syscall {
	struct syscall *sc;
	const char *name;
	int number;
	unsigned long args[10];
	unsigned int nargs;
	char *s_args[10];	/* the printable arguments */
};

struct threadinfo
{
	SLIST_ENTRY(threadinfo) entries;
	struct procinfo *proc;
	lwpid_t tid;
	int in_syscall;
	struct current_syscall cs;
	struct timespec before;
	struct timespec after;
};

struct procinfo {
	LIST_ENTRY(procinfo) entries;
	pid_t pid;
	struct procabi *abi;

	SLIST_HEAD(, threadinfo) threadlist;
};

struct trussinfo
{
	int flags;
	int strsize;
	FILE *outfile;

	struct timespec start_time;

	struct threadinfo *curthread;

	LIST_HEAD(, procinfo) proclist;
};

#define	timespecsubt(tvp, uvp, vvp)					\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_nsec = (tvp)->tv_nsec - (uvp)->tv_nsec;	\
		if ((vvp)->tv_nsec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_nsec += 1000000000;			\
		}							\
	} while (0)

#define	timespecadd(tvp, uvp, vvp)					\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_nsec = (tvp)->tv_nsec + (uvp)->tv_nsec;	\
		if ((vvp)->tv_nsec > 1000000000) {				\
			(vvp)->tv_sec++;				\
			(vvp)->tv_nsec -= 1000000000;			\
		}							\
	} while (0)
