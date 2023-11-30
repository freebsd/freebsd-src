/*-
 * Copyright (c) 2010 Isilon Systems, Inc.
 * Copyright (c) 2010 iX Systems, Inc.
 * Copyright (c) 2010 Panasas, Inc.
 * Copyright (c) 2013, 2014 Mellanox Technologies, Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */
#ifndef	_LINUXKPI_LINUX_NET_H_
#define	_LINUXKPI_LINUX_NET_H_

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>

static inline int
sock_create_kern(int family, int type, int proto, struct socket **res)
{
	return -socreate(family, res, type, proto, curthread->td_ucred,
	    curthread);
}

static inline int
sock_getname(struct socket *so, struct sockaddr *sa, int *sockaddr_len,
    int peer)
{
	int error;

	/*
	 * XXXGL: we can't use sopeeraddr()/sosockaddr() here since with
	 * INVARIANTS they would check if supplied sockaddr has enough
	 * length.  Such notion doesn't even exist in Linux KPI.
	 */
	if (peer) {
		if ((so->so_state & (SS_ISCONNECTED|SS_ISCONFIRMING)) == 0)
			return (-ENOTCONN);

		error = so->so_proto->pr_peeraddr(so, sa);
	} else
		error = so->so_proto->pr_sockaddr(so, sa);
	if (error)
		return (-error);
	*sockaddr_len = sa->sa_len;

	return (0);
}

static inline void
sock_release(struct socket *so)
{
	soclose(so);
}


int linuxkpi_net_ratelimit(void);

static inline int
net_ratelimit(void)
{

	return (linuxkpi_net_ratelimit());
}

#endif	/* _LINUXKPI_LINUX_NET_H_ */
