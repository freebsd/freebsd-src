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
