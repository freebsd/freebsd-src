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
 * $Id: uthread_fd.c,v 1.5 1998/02/13 01:27:32 julian Exp $
 *
 */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/* Static variables: */
static	long	fd_table_lock	= 0;

/*
 * This function *must* return -1 and set the thread specific errno
 * as a system call. This is because the error return from this
 * function is propagated directly back from thread-wrapped system
 * calls.
 */

int
_thread_fd_table_init(int fd)
{
	int	ret = 0;

	/* Lock the file descriptor table: */
	_spinlock(&fd_table_lock);

	/* Check if the file descriptor is out of range: */
	if (fd < 0 || fd >= _thread_dtablesize) {
		/* Return a bad file descriptor error: */
		errno = EBADF;
		ret = -1;
	}

	/*
	 * Check if memory has already been allocated for this file
	 * descriptor: 
	 */
	else if (_thread_fd_table[fd] != NULL) {
		/* Memory has already been allocated. */
	}
	/* Allocate memory for the file descriptor table entry: */
	else if ((_thread_fd_table[fd] = (struct fd_table_entry *)
	    malloc(sizeof(struct fd_table_entry))) == NULL) {
		/* Return a bad file descriptor error: */
		errno = EBADF;
		ret = -1;
	} else {
		/* Assume that the operation will succeed: */
		ret = 0;

		/* Initialise the file locks: */
		_thread_fd_table[fd]->access_lock = 0;
		_thread_fd_table[fd]->r_owner = NULL;
		_thread_fd_table[fd]->w_owner = NULL;
		_thread_fd_table[fd]->r_fname = NULL;
		_thread_fd_table[fd]->w_fname = NULL;
		_thread_fd_table[fd]->r_lineno = 0;;
		_thread_fd_table[fd]->w_lineno = 0;;
		_thread_fd_table[fd]->r_lockcount = 0;;
		_thread_fd_table[fd]->w_lockcount = 0;;

		/* Initialise the read/write queues: */
		_thread_queue_init(&_thread_fd_table[fd]->r_queue);
		_thread_queue_init(&_thread_fd_table[fd]->w_queue);

		/* Get the flags for the file: */
		if (fd >= 3 && (_thread_fd_table[fd]->flags =
		    _thread_sys_fcntl(fd, F_GETFL, 0)) == -1) {
			ret = -1;
		    }
		else {
			/* Check if a stdio descriptor: */
			if (fd < 3)
				/*
				 * Use the stdio flags read by
				 * _pthread_init() to avoid
				 * mistaking the non-blocking
				 * flag that, when set on one
				 * stdio fd, is set on all stdio
				 * fds.
				 */
				_thread_fd_table[fd]->flags =
				    _pthread_stdio_flags[fd];

			/* Make the file descriptor non-blocking: */
			if (_thread_sys_fcntl(fd, F_SETFL,
			    _thread_fd_table[fd]->flags | O_NONBLOCK) == -1) {
				/*
				 * Some devices don't support
				 * non-blocking calls (sigh):
				 */
				if (errno != ENODEV) {
				   ret = -1;
				}
			}
		}

		/* Check if one of the fcntl calls failed: */
		if (ret != 0) {
			/* Free the file descriptor table entry: */
			free(_thread_fd_table[fd]);
			_thread_fd_table[fd] = NULL;
		}
	}

	/* Unlock the file descriptor table: */
	_atomic_unlock(&fd_table_lock);

	/* Return the completion status: */
	return (ret);
}

void
_thread_fd_unlock(int fd, int lock_type)
{
	int	ret;

	/*
	 * Check that the file descriptor table is initialised for this
	 * entry: 
	 */
	if ((ret = _thread_fd_table_init(fd)) == 0) {
		/*
		 * Lock the file descriptor table entry to prevent
		 * other threads for clashing with the current
		 * thread's accesses:
		 */
		_spinlock(&_thread_fd_table[fd]->access_lock);

		/* Check if the running thread owns the read lock: */
		if (_thread_fd_table[fd]->r_owner == _thread_run) {
			/* Check the file descriptor and lock types: */
			if (lock_type == FD_READ || lock_type == FD_RDWR) {
				/*
				 * Decrement the read lock count for the
				 * running thread: 
				 */
				_thread_fd_table[fd]->r_lockcount--;

				/*
				 * Check if the running thread still has read
				 * locks on this file descriptor: 
				 */
				if (_thread_fd_table[fd]->r_lockcount != 0) {
				}
				/*
				 * Get the next thread in the queue for a
				 * read lock on this file descriptor: 
				 */
				else if ((_thread_fd_table[fd]->r_owner = _thread_queue_deq(&_thread_fd_table[fd]->r_queue)) == NULL) {
				} else {
					/*
					 * Set the state of the new owner of
					 * the thread to  running: 
					 */
					PTHREAD_NEW_STATE(_thread_fd_table[fd]->r_owner,PS_RUNNING);

					/*
					 * Reset the number of read locks.
					 * This will be incremented by the
					 * new owner of the lock when it sees
					 * that it has the lock.                           
					 */
					_thread_fd_table[fd]->r_lockcount = 0;
				}
			}
		}
		/* Check if the running thread owns the write lock: */
		if (_thread_fd_table[fd]->w_owner == _thread_run) {
			/* Check the file descriptor and lock types: */
			if (lock_type == FD_WRITE || lock_type == FD_RDWR) {
				/*
				 * Decrement the write lock count for the
				 * running thread: 
				 */
				_thread_fd_table[fd]->w_lockcount--;

				/*
				 * Check if the running thread still has
				 * write locks on this file descriptor: 
				 */
				if (_thread_fd_table[fd]->w_lockcount != 0) {
				}
				/*
				 * Get the next thread in the queue for a
				 * write lock on this file descriptor: 
				 */
				else if ((_thread_fd_table[fd]->w_owner = _thread_queue_deq(&_thread_fd_table[fd]->w_queue)) == NULL) {
				} else {
					/*
					 * Set the state of the new owner of
					 * the thread to running: 
					 */
					PTHREAD_NEW_STATE(_thread_fd_table[fd]->w_owner,PS_RUNNING);

					/*
					 * Reset the number of write locks.
					 * This will be incremented by the
					 * new owner of the lock when it  
					 * sees that it has the lock.                           
					 */
					_thread_fd_table[fd]->w_lockcount = 0;
				}
			}
		}

		/* Unlock the file descriptor table entry: */
		_atomic_unlock(&_thread_fd_table[fd]->access_lock);

	}
	/* Nothing to return. */
	return;
}

int
_thread_fd_lock(int fd, int lock_type, struct timespec * timeout,
		char *fname, int lineno)
{
	int	ret;

	/*
	 * Check that the file descriptor table is initialised for this
	 * entry: 
	 */
	if ((ret = _thread_fd_table_init(fd)) == 0) {
		/*
		 * Lock the file descriptor table entry to prevent
		 * other threads for clashing with the current
		 * thread's accesses:
		 */
		_spinlock(&_thread_fd_table[fd]->access_lock);

		/* Check the file descriptor and lock types: */
		if (lock_type == FD_READ || lock_type == FD_RDWR) {
			/*
			 * Enter a loop to wait for the file descriptor to be
			 * locked    for read for the current thread: 
			 */
			while (_thread_fd_table[fd]->r_owner != _thread_run) {
				/*
				 * Check if the file descriptor is locked by
				 * another thread: 
				 */
				if (_thread_fd_table[fd]->r_owner != NULL) {
					/*
					 * Another thread has locked the file
					 * descriptor for read, so join the
					 * queue of threads waiting for a  
					 * read lock on this file descriptor: 
					 */
					_thread_queue_enq(&_thread_fd_table[fd]->r_queue, _thread_run);

					/*
					 * Save the file descriptor details
					 * in the thread structure for the
					 * running thread: 
					 */
					_thread_run->data.fd.fd = fd;
					_thread_run->data.fd.branch = lineno;
					_thread_run->data.fd.fname = fname;

					/* Set the timeout: */
					_thread_kern_set_timeout(timeout);

					/*
					 * Unlock the file descriptor
					 * table entry:
					 */
					_atomic_unlock(&_thread_fd_table[fd]->access_lock);

					/*
					 * Schedule this thread to wait on
					 * the read lock. It will only be
					 * woken when it becomes the next in
					 * the   queue and is granted access
					 * to the lock by the       thread
					 * that is unlocking the file
					 * descriptor.        
					 */
					_thread_kern_sched_state(PS_FDLR_WAIT, __FILE__, __LINE__);

					/*
					 * Lock the file descriptor
					 * table entry again:
					 */
					_spinlock(&_thread_fd_table[fd]->access_lock);

				} else {
					/*
					 * The running thread now owns the
					 * read lock on this file descriptor: 
					 */
					_thread_fd_table[fd]->r_owner = _thread_run;

					/*
					 * Reset the number of read locks for
					 * this file descriptor: 
					 */
					_thread_fd_table[fd]->r_lockcount = 0;

					/*
					 * Save the source file details for
					 * debugging: 
					 */
					_thread_fd_table[fd]->r_fname = fname;
					_thread_fd_table[fd]->r_lineno = lineno;
				}
			}

			/* Increment the read lock count: */
			_thread_fd_table[fd]->r_lockcount++;
		}
		/* Check the file descriptor and lock types: */
		if (lock_type == FD_WRITE || lock_type == FD_RDWR) {
			/*
			 * Enter a loop to wait for the file descriptor to be
			 * locked for write for the current thread: 
			 */
			while (_thread_fd_table[fd]->w_owner != _thread_run) {
				/*
				 * Check if the file descriptor is locked by
				 * another thread: 
				 */
				if (_thread_fd_table[fd]->w_owner != NULL) {
					/*
					 * Another thread has locked the file
					 * descriptor for write, so join the
					 * queue of threads waiting for a 
					 * write lock on this file
					 * descriptor: 
					 */
					_thread_queue_enq(&_thread_fd_table[fd]->w_queue, _thread_run);

					/*
					 * Save the file descriptor details
					 * in the thread structure for the
					 * running thread: 
					 */
					_thread_run->data.fd.fd = fd;
					_thread_run->data.fd.branch = lineno;
					_thread_run->data.fd.fname = fname;

					/* Set the timeout: */
					_thread_kern_set_timeout(timeout);

					/*
					 * Unlock the file descriptor
					 * table entry:
					 */
					_atomic_unlock(&_thread_fd_table[fd]->access_lock);

					/*
					 * Schedule this thread to wait on
					 * the write lock. It will only be
					 * woken when it becomes the next in
					 * the queue and is granted access to
					 * the lock by the thread that is
					 * unlocking the file descriptor.        
					 */
					_thread_kern_sched_state(PS_FDLW_WAIT, __FILE__, __LINE__);

					/*
					 * Lock the file descriptor
					 * table entry again:
					 */
					_spinlock(&_thread_fd_table[fd]->access_lock);
				} else {
					/*
					 * The running thread now owns the
					 * write lock on this   file
					 * descriptor: 
					 */
					_thread_fd_table[fd]->w_owner = _thread_run;

					/*
					 * Reset the number of write locks
					 * for this file descriptor: 
					 */
					_thread_fd_table[fd]->w_lockcount = 0;

					/*
					 * Save the source file details for
					 * debugging: 
					 */
					_thread_fd_table[fd]->w_fname = fname;
					_thread_fd_table[fd]->w_lineno = lineno;
				}
			}

			/* Increment the write lock count: */
			_thread_fd_table[fd]->w_lockcount++;
		}

		/* Unlock the file descriptor table entry: */
		_atomic_unlock(&_thread_fd_table[fd]->access_lock);
	}

	/* Return the completion status: */
	return (ret);
}
#endif
