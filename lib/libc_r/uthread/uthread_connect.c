/*
 * Copyright (c) 1995-1998 John Birrell <jb@cimlogic.com.au>
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
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
 * $FreeBSD: src/lib/libc_r/uthread/uthread_connect.c,v 1.10 2000/01/29 22:53:41 jasone Exp $
 */
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
_connect(int fd, const struct sockaddr * name, socklen_t namelen)
{
	struct sockaddr tmpname;
	int             errnolen, ret, tmpnamelen;

	if ((ret = _FD_LOCK(fd, FD_RDWR, NULL)) == 0) {
		if ((ret = _thread_sys_connect(fd, name, namelen)) < 0) {
			if (!(_thread_fd_table[fd]->flags & O_NONBLOCK) &&
			((errno == EWOULDBLOCK) || (errno == EINPROGRESS) ||
			 (errno == EALREADY) || (errno == EAGAIN))) {
				_thread_run->data.fd.fd = fd;

				/* Set the timeout: */
				_thread_kern_set_timeout(NULL);
				_thread_kern_sched_state(PS_FDW_WAIT, __FILE__, __LINE__);

				tmpnamelen = sizeof(tmpname);
				/* 0 now lets see if it really worked */
				if (((ret = _thread_sys_getpeername(fd, &tmpname, &tmpnamelen)) < 0) && (errno == ENOTCONN)) {

					/*
					 * Get the error, this function
					 * should not fail 
					 */
					errnolen = sizeof(errno);
					_thread_sys_getsockopt(fd, SOL_SOCKET, SO_ERROR, &errno, &errnolen);
				}
			} else {
				ret = -1;
			}
		}
		_FD_UNLOCK(fd, FD_RDWR);
	}
	return (ret);
}

__strong_reference(_connect, connect);
#endif
