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

#include <sys/queue.h>

#define FOLLOWFORKS        0x00000001
#define RELATIVETIMESTAMPS 0x00000002
#define ABSOLUTETIMESTAMPS 0x00000004
#define NOSIGS             0x00000008
#define EXECVEARGS         0x00000010
#define EXECVEENVS         0x00000020
#define COUNTONLY          0x00000040

struct threadinfo
{
	SLIST_ENTRY(threadinfo) entries;
	lwpid_t tid;
	int in_syscall;
	int in_fork;
};

struct trussinfo
{
	int pid;
	int flags;
	int pr_why;
	int pr_data;
	int strsize;
	FILE *outfile;

	struct timespec start_time;
	struct timespec before;
	struct timespec after;

	struct threadinfo *curthread;
	
	SLIST_HEAD(, threadinfo) threadlist;
};

#define timespecsubt(tvp, uvp, vvp)					\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_nsec = (tvp)->tv_nsec - (uvp)->tv_nsec;	\
		if ((vvp)->tv_nsec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_nsec += 1000000000;			\
		}							\
	} while (0)

#define timespecadd(tvp, uvp, vvp)					\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec + (uvp)->tv_sec;		\
		(vvp)->tv_nsec = (tvp)->tv_nsec + (uvp)->tv_nsec;	\
		if ((vvp)->tv_nsec > 1000000000) {				\
			(vvp)->tv_sec++;				\
			(vvp)->tv_nsec -= 1000000000;			\
		}							\
	} while (0)

#define S_NONE  0
#define S_SCE   1
#define S_SCX   2
#define S_EXIT  3
#define S_SIG   4
#define S_EXEC  5
