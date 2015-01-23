/*-
 * Copyright (c) 2006 Roman Divacky
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _LINUX_MISC_H_
#define	_LINUX_MISC_H_

/*
 * Miscellaneous
 */
#define	LINUX_NAME_MAX		255
#define	LINUX_MAX_UTSNAME	65

#define	LINUX_CTL_MAXNAME	10

/* defines for prctl */
#define	LINUX_PR_SET_PDEATHSIG  1	/* Second arg is a signal. */
#define	LINUX_PR_GET_PDEATHSIG  2	/*
					 * Second arg is a ptr to return the
					 * signal.
					 */
#define	LINUX_PR_GET_KEEPCAPS	7	/* Get drop capabilities on setuid */
#define	LINUX_PR_SET_KEEPCAPS	8	/* Set drop capabilities on setuid */
#define	LINUX_PR_SET_NAME	15	/* Set process name. */
#define	LINUX_PR_GET_NAME	16	/* Get process name. */

#define	LINUX_MAX_COMM_LEN	16	/* Maximum length of the process name. */

#define	LINUX_MREMAP_MAYMOVE	1
#define	LINUX_MREMAP_FIXED	2

extern const char *linux_platform;

/*
 * Non-standard aux entry types used in Linux ELF binaries.
 */

#define	LINUX_AT_PLATFORM	15	/* String identifying CPU */
#define	LINUX_AT_HWCAP		16	/* CPU capabilities */
#define	LINUX_AT_CLKTCK		17	/* frequency at which times() increments */
#define	LINUX_AT_SECURE		23	/* secure mode boolean */
#define	LINUX_AT_BASE_PLATFORM	24	/* string identifying real platform, may
					 * differ from AT_PLATFORM.
					 */
#define	LINUX_AT_EXECFN		31	/* filename of program */

/* Linux sets the i387 to extended precision. */
#if defined(__i386__) || defined(__amd64__)
#define	__LINUX_NPXCW__		0x37f
#endif

#define	LINUX_CLONE_VM			0x00000100
#define	LINUX_CLONE_FS			0x00000200
#define	LINUX_CLONE_FILES		0x00000400
#define	LINUX_CLONE_SIGHAND		0x00000800
#define	LINUX_CLONE_PID			0x00001000	/* No longer exist in Linux */
#define	LINUX_CLONE_VFORK		0x00004000
#define	LINUX_CLONE_PARENT		0x00008000
#define	LINUX_CLONE_THREAD		0x00010000
#define	LINUX_CLONE_SETTLS		0x00080000
#define	LINUX_CLONE_PARENT_SETTID	0x00100000
#define	LINUX_CLONE_CHILD_CLEARTID	0x00200000
#define	LINUX_CLONE_CHILD_SETTID	0x01000000

#define	LINUX_THREADING_FLAGS					\
	(LINUX_CLONE_VM | LINUX_CLONE_FS | LINUX_CLONE_FILES |	\
	LINUX_CLONE_SIGHAND | LINUX_CLONE_THREAD)

/* Scheduling policies */
#define	LINUX_SCHED_OTHER	0
#define	LINUX_SCHED_FIFO	1
#define	LINUX_SCHED_RR		2

struct l_new_utsname {
	char	sysname[LINUX_MAX_UTSNAME];
	char	nodename[LINUX_MAX_UTSNAME];
	char	release[LINUX_MAX_UTSNAME];
	char	version[LINUX_MAX_UTSNAME];
	char	machine[LINUX_MAX_UTSNAME];
	char	domainname[LINUX_MAX_UTSNAME];
};

#define	LINUX_CLOCK_REALTIME		0
#define	LINUX_CLOCK_MONOTONIC		1
#define	LINUX_CLOCK_PROCESS_CPUTIME_ID	2
#define	LINUX_CLOCK_THREAD_CPUTIME_ID	3
#define	LINUX_CLOCK_REALTIME_HR		4
#define	LINUX_CLOCK_MONOTONIC_HR	5

extern int stclohz;

#define __WCLONE 0x80000000

int linux_common_wait(struct thread *td, int pid, int *status,
			int options, struct rusage *ru);
int linux_set_upcall_kse(struct thread *td, register_t stack);
int linux_set_cloned_tls(struct thread *td, void *desc);

#endif	/* _LINUX_MISC_H_ */
