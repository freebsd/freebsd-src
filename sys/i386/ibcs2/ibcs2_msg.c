/*
 * Copyright (c) 1995 Steven Wallace
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
 * $Id: ibcs2_msg.c,v 1.4 1997/07/20 09:39:44 bde Exp $
 */

/*
 * IBCS2 message compatibility module.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>

#include <i386/ibcs2/ibcs2_types.h>
#include <i386/ibcs2/ibcs2_signal.h>
#include <i386/ibcs2/ibcs2_util.h>
#include <i386/ibcs2/ibcs2_poll.h>
#include <i386/ibcs2/ibcs2_proto.h>


int
ibcs2_getmsg(p, uap)
	struct proc *p;
	struct ibcs2_getmsg_args *uap;
{
	return 0; /* fake */
}

int
ibcs2_putmsg(p, uap)
	struct proc *p;
	struct ibcs2_putmsg_args *uap;
{
	return 0; /* fake */
}


int
ibcs2_poll(p, uap)
	struct proc *p;
	struct ibcs2_poll_args *uap;
{
	int error, i;
	fd_set *readfds, *writefds, *exceptfds;
	struct timeval *timeout;
	struct ibcs2_poll conv;
	struct select_args tmp_select;
	caddr_t sg = stackgap_init();

	if (uap->nfds > FD_SETSIZE)
		return EINVAL;
	readfds   = stackgap_alloc(&sg, sizeof(fd_set *));
	writefds  = stackgap_alloc(&sg, sizeof(fd_set *));
	exceptfds = stackgap_alloc(&sg, sizeof(fd_set *));
	timeout   = stackgap_alloc(&sg, sizeof(struct timeval *));

	FD_ZERO(readfds);
	FD_ZERO(writefds);
	FD_ZERO(exceptfds);
	if (uap->timeout == -1)
		timeout = NULL;
	else {
		timeout->tv_usec = (uap->timeout % 1000)*1000;
		timeout->tv_sec  = uap->timeout / 1000;
	}

	tmp_select.nd = 0;
	tmp_select.in = readfds;
	tmp_select.ou = writefds;
	tmp_select.ex = exceptfds;
	tmp_select.tv = timeout;

	for (i = 0; i < uap->nfds; i++) {
		if (error = copyin(uap->fds + i*sizeof(struct ibcs2_poll),
				   &conv, sizeof(conv)))
			return error;
		conv.revents = 0;
		if (conv.fd < 0 || conv.fd > FD_SETSIZE)
			continue;
		if (conv.fd >= tmp_select.nd)
			tmp_select.nd = conv.fd + 1;
		if (conv.events & IBCS2_READPOLL)
			FD_SET(conv.fd, readfds);
		if (conv.events & IBCS2_WRITEPOLL)
			FD_SET(conv.fd, writefds);
		FD_SET(conv.fd, exceptfds);
	}
	if (error = select(p, &tmp_select))
		return error;
	if (p->p_retval[0] == 0)
		return 0;
	p->p_retval[0] = 0;
	for (p->p_retval[0] = 0, i = 0; i < uap->nfds; i++) {
		copyin(uap->fds + i*sizeof(struct ibcs2_poll),
		       &conv, sizeof(conv));
		conv.revents = 0;
		if (conv.fd < 0 || conv.fd > FD_SETSIZE)
			/* should check for open as well */
			conv.revents |= IBCS2_POLLNVAL;
		else {
			if (FD_ISSET(conv.fd, readfds))
				conv.revents |= IBCS2_POLLIN;
			if (FD_ISSET(conv.fd, writefds))
				conv.revents |= IBCS2_POLLOUT;
			if (FD_ISSET(conv.fd, exceptfds))
				conv.revents |= IBCS2_POLLERR;
			if (conv.revents)
				++p->p_retval[0];
		}
		if (error = copyout(&conv,
				    uap->fds + i*sizeof(struct ibcs2_poll),
				    sizeof(conv)))
			return error;
	}
	return 0;
}
