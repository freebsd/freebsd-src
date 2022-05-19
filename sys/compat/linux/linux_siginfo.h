/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2004 Tim J. Robbins
 * Copyright (c) 2001 Doug Rabson
 * Copyright (c) 1994-1996 Søren Schmidt
 * All rights reserved.
 * Copyright (c) 2022 Dmitry Chagin <dchagin@FreeBSD.org>
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

#ifndef _LINUX_SIGINFO_H_
#define _LINUX_SIGINFO_H_

/*
 * si_code values
 */
#define	LINUX_SI_USER		0	/* sent by kill, sigsend, raise */
#define	LINUX_SI_KERNEL		0x80	/* sent by the kernel from somewhere */
#define	LINUX_SI_QUEUE		-1	/* sent by sigqueue */
#define	LINUX_SI_TIMER		-2	/* sent by timer expiration */
#define	LINUX_SI_MESGQ		-3	/* sent by real time mesq state change */
#define	LINUX_SI_ASYNCIO	-4	/* sent by AIO completion */
#define	LINUX_SI_SIGIO		-5	/* sent by queued SIGIO */
#define	LINUX_SI_TKILL		-6	/* sent by tkill system call */

/*
 * SIGILL si_codes
 */
#define	LINUX_ILL_ILLOPC	1	/* illegal opcode */
#define	LINUX_ILL_ILLOPN	2	/* illegal operand */
#define	LINUX_ILL_ILLADR	3	/* illegal addressing mode */
#define	LINUX_ILL_ILLTRP	4	/* illegal trap */
#define	LINUX_ILL_PRVOPC	5	/* privileged opcode */
#define	LINUX_ILL_PRVREG	6	/* privileged register */
#define	LINUX_ILL_COPROC	7	/* coprocessor error */
#define	LINUX_ILL_BADSTK	8	/* internal stack error */
#define	LINUX_ILL_BADIADDR	9	/* unimplemented instruction address */
#define	LINUX___ILL_BREAK	10	/* (ia64) illegal break */
#define	LINUX___ILL_BNDMOD	11	/* (ia64) bundle-update (modification)
					 * in progress
					 */

/*
 * SIGFPE si_codes
 */
#define	LINUX_FPE_INTDIV	1	/* integer divide by zero */
#define	LINUX_FPE_INTOVF	2	/* integer overflow */
#define	LINUX_FPE_FLTDIV	3	/* floating point divide by zero */
#define	LINUX_FPE_FLTOVF	4	/* floating point overflow */
#define	LINUX_FPE_FLTUND	5	/* floating point underflow */
#define	LINUX_FPE_FLTRES	6	/* floating point inexact result */
#define	LINUX_FPE_FLTINV	7	/* floating point invalid operation */
#define	LINUX_FPE_FLTSUB	8	/* (ia64) subscript out of range */
#define	LINUX___FPE_DECOVF	9	/* (ia64) decimal overflow */
#define	LINUX___FPE_DECDIV	10	/* (ia64) decimal division by zero */
#define	LINUX___FPE_DECERR	11	/* (ia64) packed decimal error */
#define	LINUX___FPE_INVASC	12	/* (ia64) invalid ASCII digit */
#define	LINUX___FPE_INVDEC	13	/* (ia64) invalid decimal digit */
#define	LINUX_FPE_FLTUNK	14	/* undiagnosed floating-point exception */
#define	LINUX_FPE_CONDTRAP	15	/* trap on condition */

/*
 * SIGSEGV si_codes
 */
#define	LINUX_SEGV_MAPERR	1	/* address not mapped to object */
#define	LINUX_SEGV_ACCERR	2	/* invalid permissions for mapped object */
#define	LINUX_SEGV_BNDERR	3	/* failed address bound checks */
#ifdef __ia64__
#define	LINUX___SEGV_PSTKOVF	4	/* paragraph stack overflow */
#else
#define	LINUX_SEGV_PKUERR	4	/* failed protection key checks */
#endif
#define	LINUX_SEGV_ACCADI	5	/* ADI not enabled for mapped object */
#define	LINUX_SEGV_ADIDERR	6	/* Disrupting MCD error */
#define	LINUX_SEGV_ADIPERR	7	/* Precise MCD exception */
#define	LINUX_SEGV_MTEAERR	8	/* Asynchronous ARM MTE error */
#define	LINUX_SEGV_MTESERR	9	/* Synchronous ARM MTE exception */

/*
 * SIGBUS si_codes
 */
#define	LINUX_BUS_ADRALN	1	/* invalid address alignment */
#define	LINUX_BUS_ADRERR	2	/* non-existent physical address */
#define	LINUX_BUS_OBJERR	3	/* object specific hardware error */

#define	LINUX_BUS_MCEERR_AR	4	/* hardware memory error consumed
					 * on a machine check:
					 * action required
					 */
#define	LINUX_BUS_MCEERR_AO	5	/* hardware memory error detected
					 * in process but not consumed:
					 * action optional
					 */

/*
 * SIGTRAP si_codes
 */
#define	LINUX_TRAP_BRKPT	1	/* process breakpoint */
#define	LINUX_TRAP_TRACE	2	/* process trace trap */
#define	LINUX_TRAP_BRANCH	3	/* process taken branch trap */
#define	LINUX_TRAP_HWBKPT	4	/* hardware breakpoint/watchpoint */
#define	LINUX_TRAP_UNK		5	/* undiagnosed trap */
#define	LINUX_TRAP_PERF		6	/* perf event with sigtrap=1 */

/*
 * SIGCHLD si_codes
 */
#define	LINUX_CLD_EXITED	1	/* child has exited */
#define	LINUX_CLD_KILLED	2	/* child was killed */
#define	LINUX_CLD_DUMPED	3	/* child terminated abnormally */
#define	LINUX_CLD_TRAPPED	4	/* traced child has trapped */
#define	LINUX_CLD_STOPPED	5	/* child has stopped */
#define	LINUX_CLD_CONTINUED	6	/* stopped child has continued */

/*
 * SIGPOLL (or any other signal without signal specific si_codes) si_codes
 */
#define	LINUX_POLL_IN		1	/* data input available */
#define	LINUX_POLL_OUT		2	/* output buffers available */
#define	LINUX_POLL_MSG		3	/* input message available */
#define	LINUX_POLL_ERR		4	/* i/o error */
#define	LINUX_POLL_PRI		5	/* high priority input available */
#define	LINUX_POLL_HUP		6	/* device disconnected */

/*
 * SIGSYS si_codes
 */
#define	LINUX_SYS_SECCOMP	1	/* seccomp triggered */
#define	LINUX_SYS_USER_DISPATCH	2	/* syscall user dispatch triggered */

/*
 * SIGEMT si_codes
 */
#define	LINUX_EMT_TAGOVF	1	/* tag overflow */

typedef union l_sigval {
	l_int		sival_int;
	l_uintptr_t	sival_ptr;
} l_sigval_t;

#define	LINUX_SI_MAX_SIZE		128

union __sifields {
	struct {
		l_pid_t		_pid;
		l_uid_t		_uid;
	} _kill;

	struct {
		l_timer_t	_tid;
		l_int		_overrun;
		char		_pad[sizeof(l_uid_t) - sizeof(int)];
		union l_sigval	_sigval;
		l_uint		_sys_private;
	} _timer;

	struct {
		l_pid_t		_pid;		/* sender's pid */
		l_uid_t		_uid;		/* sender's uid */
		union l_sigval	_sigval;
	} _rt;

	struct {
		l_pid_t		_pid;		/* which child */
		l_uid_t		_uid;		/* sender's uid */
		l_int		_status;	/* exit code */
		l_clock_t	_utime;
		l_clock_t	_stime;
	} _sigchld;

	struct {
		l_uintptr_t	_addr;	/* Faulting insn/memory ref. */
	} _sigfault;

	struct {
		l_long		_band;	/* POLL_IN,POLL_OUT,POLL_MSG */
		l_int		_fd;
	} _sigpoll;
};

typedef struct l_siginfo {
	union {
		struct {
			l_int		lsi_signo;
			l_int		lsi_errno;
			l_int		lsi_code;
			union __sifields _sifields;
		};
		l_int	_pad[LINUX_SI_MAX_SIZE/sizeof(l_int)];
	};
} l_siginfo_t;

_Static_assert(sizeof(l_siginfo_t) == LINUX_SI_MAX_SIZE, "l_siginfo_t size");

#define	lsi_pid		_sifields._kill._pid
#define	lsi_uid		_sifields._kill._uid
#define	lsi_tid		_sifields._timer._tid
#define	lsi_overrun	_sifields._timer._overrun
#define	lsi_sys_private	_sifields._timer._sys_private
#define	lsi_status	_sifields._sigchld._status
#define	lsi_utime	_sifields._sigchld._utime
#define	lsi_stime	_sifields._sigchld._stime
#define	lsi_value	_sifields._rt._sigval
#define	lsi_int		_sifields._rt._sigval.sival_int
#define	lsi_ptr		_sifields._rt._sigval.sival_ptr
#define	lsi_addr	_sifields._sigfault._addr
#define	lsi_band	_sifields._sigpoll._band
#define	lsi_fd		_sifields._sigpoll._fd

#endif /* _LINUX_SIGINFO_H_ */

