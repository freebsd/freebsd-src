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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
fcntl(int fd, int cmd,...)
{
	int             flags = 0;
	int		nonblock;
	int             oldfd;
	int             ret;
	int             status;
	va_list         ap;

	/* Lock the file descriptor: */
	if ((ret = _FD_LOCK(fd, FD_RDWR, NULL)) == 0) {
		/* Initialise the variable argument list: */
		va_start(ap, cmd);

		/* Process according to file control command type: */
		switch (cmd) {
		/* Duplicate a file descriptor: */
		case F_DUPFD:
			/*
			 * Get the file descriptor that the caller wants to
			 * use: 
			 */
			oldfd = va_arg(ap, int);

			/* Initialise the file descriptor table entry: */
			if ((ret = _thread_sys_fcntl(fd, cmd, oldfd)) < 0) {
			}
			/* Initialise the file descriptor table entry: */
			else if (_thread_fd_table_init(ret) != 0) {
				/* Quietly close the file: */
				_thread_sys_close(ret);

				/* Reset the file descriptor: */
				ret = -1;
			} else {
				/*
				 * Save the file open flags so that they can
				 * be         checked later: 
				 */
				_thread_fd_table[ret]->flags = _thread_fd_table[fd]->flags;
			}
			break;
		case F_SETFD:
			flags = va_arg(ap, int);
			ret = _thread_sys_fcntl(fd, cmd, flags);
			break;
		case F_GETFD:
			ret = _thread_sys_fcntl(fd, cmd, 0);
			break;
		case F_GETFL:
			ret = _thread_fd_table[fd]->flags;
			break;
		case F_SETFL:
			/*
			 * Get the file descriptor flags passed by the
			 * caller:
			 */
			flags = va_arg(ap, int);

			/*
			 * Check if the user wants a non-blocking file
			 * descriptor:
			 */
			nonblock = flags & O_NONBLOCK;

			/* Set the file descriptor flags: */
			if ((ret = _thread_sys_fcntl(fd, cmd, flags | O_NONBLOCK)) != 0) {

			/* Get the flags so that we behave like the kernel: */
			} else if ((flags = _thread_sys_fcntl(fd,
			    F_GETFL, 0)) == -1) {
				/* Error getting flags: */
				ret = -1;

			/*
			 * Check if the file descriptor is non-blocking
			 * with respect to the user:
			 */
			} else if (nonblock)
				/* A non-blocking descriptor: */
				_thread_fd_table[fd]->flags = flags | O_NONBLOCK;
			else
				/* Save the flags: */
				_thread_fd_table[fd]->flags = flags & ~O_NONBLOCK;
			break;
		default:
			/* Might want to make va_arg use a union */
			ret = _thread_sys_fcntl(fd, cmd, va_arg(ap, void *));
			break;
		}

		/* Free variable arguments: */
		va_end(ap);

		/* Unlock the file descriptor: */
		_FD_UNLOCK(fd, FD_RDWR);
	}

	/* Return the completion status: */
	return (ret);
}
#endif
