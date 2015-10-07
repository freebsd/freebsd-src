/*-
 * Copyright (c) 2015 SRI International
 * Copyright (c) 2001 Doug Rabson
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
 *
 * $FreeBSD$
 */

#ifndef _COMPAT_CHERIABI_CHERIABI_H_
#define _COMPAT_CHERIABI_CHERIABI_H_

#include <machine/cheri.h>

static inline void *
__cheri_cap_to_ptr(struct chericap *c)
{
	void *ptr;

	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, c, 0);
	CHERI_CTOPTR(ptr, CHERI_CR_CTEMP0, CHERI_CR_KDC);

	return (ptr);
}

#define PTRIN(v)        __cheri_cap_to_ptr(&v)

#define CP(src,dst,fld) do { (dst).fld = (src).fld; } while (0)
#define PTRIN_CP(src,dst,fld) \
	do { (dst).fld = PTRIN((src).fld); } while (0)

struct kevent_c {
	uintptr_t	ident;		/* identifier for this event */
	short		filter;		/* filter for event */
	u_short		flags;
	u_int		fflags;
	intptr_t	data;
	struct chericap	udata;		/* opaque user data identifier */
};

struct iovec_c {
	struct chericap	iov_base;
	size_t		iov_len;
};

struct msghdr_c {
	struct chericap	msg_name;
	socklen_t	msg_namelen;
	struct chericap	msg_iov;
	int		msg_iovlen;
	struct chericap	msg_control;
	socklen_t	msg_controllen;
	int		msg_flags;
};

struct jail_c {
	uint32_t	version;
	struct chericap	path;
	struct chericap	hostname;
	struct chericap	jailname;
	uint32_t	ip4s;
	uint32_t	ip6s;
	struct chericap	ip4;
	struct chericap ip6;
};

struct sigaction_c {
	struct chericap	sa_u;
	int		sa_flags;
	sigset_t	sa_mask;
};

struct thr_param_c {
	uintptr_t	start_func;
	struct chericap	arg;
	struct chericap	stack_base;
	size_t		stack_size;
	struct chericap	tls_base;
	size_t		tls_size;
	struct chericap	child_tid;
	struct chericap	parent_tid;
	int		flags;
	struct chericap	rtp;
	struct chericap	spare[3];
};

#endif /* !_COMPAT_CHERIABI_CHERIABI_H_ */
