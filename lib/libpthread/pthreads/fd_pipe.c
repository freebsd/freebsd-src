/* ==== fd_pipe.c ============================================================
 * Copyright (c) 1993, 1994 by Chris Provenzano, proven@mit.edu
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
 *  This product includes software developed by Chris Provenzano.
 * 4. The name of Chris Provenzano may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * Description : The new fast ITC pipe routines.
 *
 *  1.00 93/08/14 proven
 *      -Started coding this file.
 *
 *	1.01 93/11/13 proven
 *		-The functions readv() and writev() added.
 */

#include <pthread.h>
#include <pthread/fd_pipe.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread/posix.h>

/* ==========================================================================
 * The pipe lock is never unlocked until all pthreads waiting are done with it
 * read()
 */
ssize_t __pipe_read(struct __pipe *fd, int flags, void *buf, size_t nbytes)
{
	semaphore *lock, *plock;
	int ret = 0;

	if (flags & O_ACCMODE) { return(NOTOK); }

	lock = &(fd->lock);
	while (SEMAPHORE_TEST_AND_SET(lock)) {
		pthread_yield();
	}
	/* If there is nothing to read, go to sleep */
	if (fd->count == 0) {
		if (flags == WR_CLOSED) {
			SEMAPHORE_RESET(lock);
			return(0);
		} /* Lock pthread */
		plock = &(pthread_run->lock);
		while (SEMAPHORE_TEST_AND_SET(plock)) {
			pthread_yield();
		}

		/* queue pthread for a FDR_WAIT */
		pthread_run->next = NULL;
		fd->wait = pthread_run;
		SEMAPHORE_RESET(lock);
		reschedule(PS_FDR_WAIT);
		ret = fd->size;
	} else {
		ret = MIN(nbytes, fd->count);
		memcpy(buf, fd->buf + fd->offset, ret);
		if (!(fd->count -= ret)) {
			fd->offset = 0;
		}

		/* Should try to read more from the waiting writer */
	
		if (fd->wait) {
			plock = &(fd->wait->lock);
			while (SEMAPHORE_TEST_AND_SET(plock)) {
				pthread_yield();
			}
			fd->wait->state = PS_RUNNING;
			SEMAPHORE_RESET(plock);
		} else {
			SEMAPHORE_RESET(lock);
		}
	}
	return(ret);
}

/* ==========================================================================
 * __pipe_write()
 *
 * First check to see if the read side is still open, then
 * check to see if there is a thread in a read wait for this pipe, if so
 * copy as much data as possible directly into the read waiting threads
 * buffer. The write thread(whether or not there was a read thread)
 * copies as much data as it can into the pipe buffer and it there
 * is still data it goes to sleep.
 */
ssize_t __pipe_write(struct __pipe *fd, int flags, const void *buf, size_t nbytes) {
	semaphore *lock, *plock;
	int ret, count;

	if (!(flags & O_ACCMODE)) { return(NOTOK); }

	lock = &(fd->lock);
	while (SEMAPHORE_TEST_AND_SET(lock)) {
		pthread_yield();
	}
	while (fd->flags != RD_CLOSED) {
		if (fd->wait) {
			/* Lock pthread */
			plock = &(fd->wait->lock);
			while (SEMAPHORE_TEST_AND_SET(plock)) {
				pthread_yield();
			}
	
			/* Copy data directly into waiting pthreads buf */
			fd->wait_size = MIN(nbytes, fd->wait_size);
			memcpy(fd->wait_buf, buf, fd->wait_size);
			buf = (const char *)buf + fd->wait_size;
			nbytes -= fd->wait_size;
			ret = fd->wait_size;

			/* Wake up waiting pthread */	
			fd->wait->state = PS_RUNNING;
			SEMAPHORE_RESET(plock);
			fd->wait = NULL;
		}

		if (count = MIN(nbytes, fd->size - (fd->offset + fd->count))) {
			memcpy(fd->buf + (fd->offset + fd->count), buf, count);
			buf = (const char *)buf + count;
			nbytes -= count;
			ret += count;
		}
		if (nbytes) {
			/* Lock pthread */
			plock = &(fd->wait->lock);
			while (SEMAPHORE_TEST_AND_SET(plock)) {
				pthread_yield();
			}
	
			fd->wait = pthread_run;
			SEMAPHORE_RESET(lock);
			reschedule(PS_FDW_WAIT);
		} else {		
	   	    return(ret);
		}
	}
	return(NOTOK);
}

/* ==========================================================================
 * __pipe_close()
 *
 * The whole close procedure is a bit odd and needs a bit of a rethink.
 * For now close() locks the fd, calls fd_free() which checks to see if
 * there are any other fd values poinging to the same real fd. If so
 * It breaks the wait queue into two sections those that are waiting on fd
 * and those waiting on other fd's. Those that are waiting on fd are connected
 * to the fd_table[fd] queue, and the count is set to zero, (BUT THE LOCK IS NOT
 * RELEASED). close() then calls fd_unlock which give the fd to the next queued
 * element which determins that the fd is closed and then calls fd_unlock etc...
 */
int __pipe_close(struct __pipe *fd, int flags)
{
	semaphore *lock, *plock;

	lock = &(fd->lock);
	while (SEMAPHORE_TEST_AND_SET(lock)) {
		pthread_yield();
	}
	if (!(fd->flags)) {
		if (fd->wait) {
			if (flags & O_ACCMODE) {
				fd->flags |= WR_CLOSED;
				/* Lock pthread */
				/* Write side closed, wake read side and return EOF */
				plock = &((fd->wait)->lock);
				while (SEMAPHORE_TEST_AND_SET(plock)) {
					pthread_yield();
				}

				fd->count = 0;

				/* Wake up waiting pthread */	
				fd->wait->state = PS_RUNNING;
				SEMAPHORE_RESET(plock);
				fd->wait = NULL;
			} else {
				/* Should send a signal */
				fd->flags |= RD_CLOSED;
			}
		}
	} else {
		free(fd);
		return(OK);
	}
	SEMAPHORE_RESET(lock);
}

/* ==========================================================================
 * For those function that aren't implemented yet
 * __pipe_enosys()
 */
static int __pipe_enosys()
{
	pthread_run->error = ENOSYS;
	return(NOTOK);
}

/*
 * File descriptor operations
 */
struct fd_ops fd_ops[] = {
{	NULL, NULL, },		/* Non operations */
{	__pipe_write, __pipe_read, __pipe_close, __pipe_enosys, __pipe_enosys,
	__pipe_enosys },
};

/* ==========================================================================
 * open()
 */
/* int __pipe_open(const char *path, int flags, ...) */
int newpipe(int fd[2])
{
	struct __pipe *fd_data;

	if ((!((fd[0] = fd_allocate()) < OK)) && (!((fd[1] = fd_allocate()) < OK))) {
		fd_data = malloc(sizeof(struct __pipe));
		fd_data->buf = malloc(4096);
		fd_data->size = 4096;
		fd_data->count = 0;
		fd_data->offset = 0;

		fd_data->wait = NULL;
		fd_data->flags = 0;

		fd_table[fd[0]]->fd.ptr = fd_data;
		fd_table[fd[0]]->flags = O_RDONLY;
		fd_table[fd[1]]->fd.ptr = fd_data;
		fd_table[fd[1]]->flags = O_WRONLY;

		return(OK);
	}
	return(NOTOK);
}

