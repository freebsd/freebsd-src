/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
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
#include <signal.h>
#include <stdio.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

void
_thread_flockfile(FILE * fp, char *fname, int lineno)
{
	int             fd, flags;
	int             status;

	/* Block signals: */
	_thread_kern_sig_block(&status);

	if ((fd = fileno(fp)) >= 0) {
		if (fp->_flags & __SRW) {
			flags = FD_READ | FD_WRITE;
		} else {
			if (fp->_flags & __SWR) {
				flags = FD_WRITE;
			} else {
				flags = FD_READ;
			}
		}

		/* This might fail but POSIX doesn't give a damn. */
		_thread_fd_lock(fd, flags, NULL, fname, lineno);
	}
	/* Unblock signals: */
	_thread_kern_sig_unblock(status);
	return;
}

int
_thread_ftrylockfile(FILE * fp)
{
	int             fd = 0;
	int             flags;
	int             status;

	if ((fd = fileno(fp)) >= 0) {
		/* Block signals: */
		_thread_kern_sig_block(&status);

		if (fp->_flags & __SRW) {
			flags = FD_READ | FD_WRITE;
		} else {
			if (fp->_flags & __SWR) {
				flags = FD_WRITE;
			} else {
				flags = FD_READ;
			}
		}
		if (!(_thread_fd_table[fd]->r_owner && _thread_fd_table[fd]->w_owner)) {
			_thread_fd_lock(fd, flags, NULL, __FILE__, __LINE__);
			fd = 0;
		} else {
			fd = -1;
		}

		/* Unblock signals: */
		_thread_kern_sig_unblock(status);
	}
	return (fd);
}

void 
_thread_funlockfile(FILE * fp)
{
	int             fd, flags;
	int             status;

	if ((fd = fileno(fp)) >= 0) {
		/* Block signals: */
		_thread_kern_sig_block(&status);

		if (fp->_flags & __SRW) {
			flags = FD_READ | FD_WRITE;
		} else if (fp->_flags & __SWR) {
			flags = FD_WRITE;
		} else {
			flags = FD_READ;
		}
		_thread_fd_unlock(fd, flags);

		/* Unblock signals: */
		_thread_kern_sig_unblock(status);
	}
	return;
}

#endif
