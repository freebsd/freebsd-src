/*-
 * Copyright (c) 2007 Roman Divacky
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
 *
 * $FreeBSD$
 */

#ifndef _LINUX_EPOLL_H_
#define	_LINUX_EPOLL_H_

#ifdef __amd64__
#define	EPOLL_PACKED	__packed
#else
#define	EPOLL_PACKED
#endif

struct linux_epoll_event {
	uint32_t	events;
	uint64_t	data;	
} EPOLL_PACKED;

#define	LINUX_EPOLLIN		0x001
#define	LINUX_EPOLLPRI		0x002
#define	LINUX_EPOLLOUT		0x004
#define	LINUX_EPOLLRDNORM	0x040
#define	LINUX_EPOLLRDBAND	0x080
#define	LINUX_EPOLLWRNORM	0x100
#define	LINUX_EPOLLWRBAND	0x200
#define	LINUX_EPOLLMSG		0x400
#define	LINUX_EPOLLERR		0x008
#define	LINUX_EPOLLHUP		0x010
#define	LINUX_EPOLLRDHUP	0x2000
#define	LINUX_EPOLLWAKEUP	1u<<29
#define	LINUX_EPOLLONESHOT	1u<<30
#define	LINUX_EPOLLET		1u<<31

#define	LINUX_EPOLL_CTL_ADD	1
#define	LINUX_EPOLL_CTL_DEL	2
#define	LINUX_EPOLL_CTL_MOD	3

#define	LINUX_EPOLL_CLOEXEC	02000000
#define	LINUX_EPOLL_NONBLOCK	00004000

#define	LINUX_MAX_EVENTS	(INT_MAX / sizeof(struct linux_epoll_event))

#endif	/* !_LINUX_EPOLL_H_ */

