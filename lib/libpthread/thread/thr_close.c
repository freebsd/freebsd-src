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
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
close(int fd)
{
	int		flags;
	int		ret;
	int		status;
	struct stat	sb;

	/* Lock the file descriptor while the file is closed: */
	if ((ret = _thread_fd_lock(fd, FD_RDWR, NULL, __FILE__, __LINE__)) == 0) {
		/* Block signals: */
		_thread_kern_sig_block(&status);

		/* Get file descriptor status. */
		fstat(fd, &sb);

		/*
		 * Check if the file should be left as blocking.
		 *
		 * This is so that the file descriptors shared with a parent
		 * process aren't left set to non-blocking if the child
		 * closes them prior to exit.  An example where this causes
		 * problems with /bin/sh is when a child closes stdin.
		 *
		 * Setting a file as blocking causes problems if a threaded
		 * parent accesses the file descriptor before the child exits.
		 * Once the threaded parent receives a SIGCHLD then it resets
		 * all of its files to non-blocking, and so it is then safe
		 * to access them.
		 *
		 * Pipes are not set to blocking when they are closed, as
		 * the parent and child will normally close the file
		 * descriptor of the end of the pipe that they are not
		 * using, which would then cause any reads to block
		 * indefinitely.
		 */
		if ((S_ISREG(sb.st_mode) || S_ISCHR(sb.st_mode)) && (_thread_fd_table[fd]->flags & O_NONBLOCK) == 0) {
			/* Get the current flags: */
			flags = _thread_sys_fcntl(fd, F_GETFL, NULL);
			/* Clear the nonblocking file descriptor flag: */
			_thread_sys_fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
		}

		/* Close the file descriptor: */
		ret = _thread_sys_close(fd);

		/* Free the file descriptor table entry: */
		free(_thread_fd_table[fd]);
		_thread_fd_table[fd] = NULL;

		/* Unblock signals again: */
		_thread_kern_sig_unblock(status);
	}
	return (ret);
}
#endif
