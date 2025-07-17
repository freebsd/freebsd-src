/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022, Jake Freeland <jfree@freebsd.org>
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

#ifndef	_LINUXKPI_LINUX_EVENTPOLL_H_
#define	_LINUXKPI_LINUX_EVENTPOLL_H_

#include <sys/poll.h>

#define EPOLLIN		POLLIN
#define EPOLLPRI	POLLPRI
#define EPOLLOUT	POLLOUT
#define EPOLLERR	POLLERR
#define EPOLLHUP	POLLHUP
#define EPOLLNVAL	POLLNVAL
#define EPOLLRDNORM	POLLRDNORM
#define EPOLLRDBAND	POLLRDBAND
#define EPOLLWRNORM	POLLWRNORM
#define EPOLLWRBAND	POLLWRBAND
#define EPOLLRDHUP	POLLRDHUP

#endif	/* _LINUXKPI_LINUX_EVENTPOLL_H_ */
