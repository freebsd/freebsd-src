/*-
 * Copyright (c) 2015-2016 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract FA8750-10-C-0237
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

#ifndef _COMPAT_CHERIABI_CHERIABI_SIGNAL_H_
#define _COMPAT_CHERIABI_CHERIABI_SIGNAL_H_

struct sigaltstack_c {
	struct chericap	ss_sp;		/* signal stack base */
	size_t		ss_size;	/* signal stack length */
	int		ss_flags;	/* SS_DISABLE and/or SS_ONSTACK */
};

union sigval_c {
	int			sival_int;
	struct chericap		sival_ptr;
	/* XXX: no 6.0 compatibility (sigval_*) */
};

struct siginfo_c {
	int		si_signo;
	int		si_errno;
	int		si_code;
	__pid_t		si_pid;
	__uid_t		si_uid;
	int		si_status;
	struct chericap	si_addr;	/* faulting instruction */
	union sigval_c	si_value;
	union   {
		struct {
			int     _trapno;	/* machine specific trap code */
		} _fault;
		struct {
			int     _timerid;
			int     _overrun;
		} _timer;
		struct {
			int     _mqd;
		} _mesgq;
		struct {
			long    _band;		/* band event for SIGPOLL */
		} _poll;			/* was this ever used ? */
		struct {
			long    __spare1__;
			int     __spare2__[7];  
		} __spare__;
	} _reason;
};

struct sigevent_c {
	int	sigev_notify;
	int	sigev_signo;
	union sigval_c sigev_value;
	union {
		__lwpid_t	_threadid;
		struct {
			struct chericap _function;	/* void (*)(union sigval); */
			struct chericap	*_attribute;	/* void * */
		} _sigev_thread;
		unsigned short _kevent_flags;
		long __spare__[8];
	} _sigev_un;
};

typedef struct {
	struct chericap ss_sp;
	size_t		ss_size;
	int		ss_flag;
} cheriabi_stack_t;

struct sigevent;
int convert_sigevent_c(struct sigevent_c *sig_c, struct sigevent *sig);
void siginfo_to_siginfo_c(const siginfo_t *src, struct siginfo_c *dst);

#endif /* _COMPAT_CHERIABI_CHERIABI_SIGNAL_H_ */
