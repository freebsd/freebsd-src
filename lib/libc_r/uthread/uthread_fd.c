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
#include <errno.h>
#include <stdlib.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

int
_thread_fd_table_init(int fd)
{
	int	ret = 0;
	int	status;

	/* Block signals: */
	_thread_kern_sig_block(&status);

	/* Check if the file descriptor is out of range: */
	if (fd < 0 || fd >= _thread_dtablesize) {
		/* Return a bad file descriptor error: */
		_thread_seterrno(_thread_run, EBADF);
		ret = -1;
	}
	/*
	 * Check if memory has already been allocated for this file
	 * descriptor: 
	 */
	else if (_thread_fd_table[fd] != NULL) {
	}
	/* Allocate memory for the file descriptor table entry: */
	else if ((_thread_fd_table[fd] = (struct fd_table_entry *) malloc(sizeof(struct fd_table_entry))) == NULL) {
		/* Return a bad file descriptor error: */
		_thread_seterrno(_thread_run, EBADF);
		ret = -1;
	} else {
		/* Initialise the file locks: */
		_thread_fd_table[fd]->r_owner = NULL;
		_thread_fd_table[fd]->w_owner = NULL;
		_thread_fd_table[fd]->r_fname = NULL;
		_thread_fd_table[fd]->w_fname = NULL;
		_thread_fd_table[fd]->r_lineno = 0;;
		_thread_fd_table[fd]->w_lineno = 0;;
		_thread_fd_table[fd]->r_lockcount = 0;;
		_thread_fd_table[fd]->w_lockcount = 0;;

		/* Default the flags: */
		_thread_fd_table[fd]->flags = 0;

		/* Initialise the read/write queues: */
		_thread_queue_init(&_thread_fd_table[fd]->r_queue);
		_thread_queue_init(&_thread_fd_table[fd]->w_queue);
	}

	/* Unblock signals: */
	_thread_kern_sig_unblock(status);

	/* Return the completion status: */
	return (ret);
}

void
_thread_fd_unlock(int fd, int lock_type)
{
	int	ret;
	int	status;

	/* Block signals while the file descriptor lock is tested: */
	_thread_kern_sig_block(&status);

	/*
	 * Check that the file descriptor table is initialised for this
	 * entry: 
	 */
	if ((ret = _thread_fd_table_init(fd)) != 0) {
	} else {
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
					_thread_fd_table[fd]->r_owner->state = PS_RUNNING;

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
					_thread_fd_table[fd]->w_owner->state = PS_RUNNING;

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
	}

	/* Unblock signals again: */
	_thread_kern_sig_unblock(status);

	/* Nothing to return.                                                   */
	return;
}

int
_thread_fd_lock(int fd, int lock_type, struct timespec * timeout,
		char *fname, int lineno)
{
	int	ret;
	int	status;

	/* Block signals while the file descriptor lock is tested: */
	_thread_kern_sig_block(&status);

	/*
	 * Check that the file descriptor table is initialised for this
	 * entry: 
	 */
	if ((ret = _thread_fd_table_init(fd)) != 0) {
	} else {
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
					 * Block signals so that the file
					 * descriptor lock can   again be
					 * tested: 
					 */
					_thread_kern_sig_block(NULL);
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
					 * Schedule this thread to wait on
					 * the write lock. It will only be
					 * woken when it becomes the next in
					 * the queue and is granted access to
					 * the lock by the thread that is
					 * unlocking the file descriptor.        
					 */
					_thread_kern_sched_state(PS_FDLW_WAIT, __FILE__, __LINE__);

					/*
					 * Block signals so that the file
					 * descriptor lock can again be
					 * tested: 
					 */
					_thread_kern_sig_block(NULL);
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
	}

	/* Unblock signals again: */
	_thread_kern_sig_unblock(status);

	/* Return the completion status: */
	return (ret);
}
#endif
