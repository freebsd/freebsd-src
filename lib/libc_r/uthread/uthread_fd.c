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
 * $FreeBSD$
 *
 */
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

#define FDQ_INSERT(q,p)					\
do {							\
	TAILQ_INSERT_TAIL(q,p,qe);			\
	p->flags |= PTHREAD_FLAGS_IN_FDQ;		\
} while (0)

#define FDQ_REMOVE(q,p)					\
do {							\
	if ((p->flags & PTHREAD_FLAGS_IN_FDQ) != 0) {	\
		TAILQ_REMOVE(q,p,qe);			\
		p->flags &= ~PTHREAD_FLAGS_IN_FDQ;	\
	}						\
} while (0)


/* Static variables: */
static	spinlock_t	fd_table_lock	= _SPINLOCK_INITIALIZER;

/* Prototypes: */
static inline pthread_t fd_next_reader(int fd);
static inline pthread_t fd_next_writer(int fd);


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
	struct fd_table_entry *entry;
	int	saved_errno;

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

	/* Allocate memory for the file descriptor table entry: */
	} else if ((entry = (struct fd_table_entry *)
	    malloc(sizeof(struct fd_table_entry))) == NULL) {
		/* Return an insufficient memory error: */
		errno = ENOMEM;
		ret = -1;
	} else {
		/* Initialise the file locks: */
		memset(&entry->lock, 0, sizeof(entry->lock));
		entry->r_owner = NULL;
		entry->w_owner = NULL;
		entry->r_fname = NULL;
		entry->w_fname = NULL;
		entry->r_lineno = 0;
		entry->w_lineno = 0;
		entry->r_lockcount = 0;
		entry->w_lockcount = 0;

		/* Initialise the read/write queues: */
		TAILQ_INIT(&entry->r_queue);
		TAILQ_INIT(&entry->w_queue);

		/* Get the flags for the file: */
		if (((fd >= 3) || (_pthread_stdio_flags[fd] == -1)) &&
		    (entry->flags = _thread_sys_fcntl(fd, F_GETFL, 0)) == -1) {
			ret = -1;
		}
		else {
			/* Check if a stdio descriptor: */
			if ((fd < 3) && (_pthread_stdio_flags[fd] != -1))
				/*
				 * Use the stdio flags read by
				 * _pthread_init() to avoid
				 * mistaking the non-blocking
				 * flag that, when set on one
				 * stdio fd, is set on all stdio
				 * fds.
				 */
				entry->flags = _pthread_stdio_flags[fd];

			/*
			 * Make the file descriptor non-blocking.
			 * This might fail if the device driver does
			 * not support non-blocking calls, or if the
			 * driver is naturally non-blocking.
			 */
			saved_errno = errno;
			_thread_sys_fcntl(fd, F_SETFL,
			    entry->flags | O_NONBLOCK);
			errno = saved_errno;

			/* Lock the file descriptor table: */
			_SPINLOCK(&fd_table_lock);

			/*
			 * Check if another thread allocated the
			 * file descriptor entry while this thread
			 * was doing the same thing. The table wasn't
			 * kept locked during this operation because
			 * it has the potential to recurse.
			 */
			if (_thread_fd_table[fd] == NULL) {
				/* This thread wins: */
				_thread_fd_table[fd] = entry;
				entry = NULL;
			}

			/* Unlock the file descriptor table: */
			_SPINUNLOCK(&fd_table_lock);
		}

		/*
		 * Check if another thread initialised the table entry
		 * before this one could:
		 */
		if (entry != NULL)
			/*
			 * Throw away the table entry that this thread
			 * prepared. The other thread wins.
			 */
			free(entry);
	}

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
		 * Defer signals to protect the scheduling queues from
		 * access by the signal handler:
		 */
		_thread_kern_sig_defer();

		/*
		 * Lock the file descriptor table entry to prevent
		 * other threads for clashing with the current
		 * thread's accesses:
		 */
		_SPINLOCK(&_thread_fd_table[fd]->lock);

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
				else if ((_thread_fd_table[fd]->r_owner = fd_next_reader(fd)) == NULL) {
				} else {
					/* Remove this thread from the queue: */
					FDQ_REMOVE(&_thread_fd_table[fd]->r_queue,
					    _thread_fd_table[fd]->r_owner);

					/*
					 * Set the state of the new owner of
					 * the thread to running: 
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
				else if ((_thread_fd_table[fd]->w_owner = fd_next_writer(fd)) == NULL) {
				} else {
					/* Remove this thread from the queue: */
					FDQ_REMOVE(&_thread_fd_table[fd]->w_queue,
					    _thread_fd_table[fd]->w_owner);

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
		_SPINUNLOCK(&_thread_fd_table[fd]->lock);

		/*
		 * Undefer and handle pending signals, yielding if
		 * necessary:
		 */
		_thread_kern_sig_undefer();
	}
}

int
_thread_fd_lock(int fd, int lock_type, struct timespec * timeout)
{
	int	ret;

	/*
	 * Check that the file descriptor table is initialised for this
	 * entry: 
	 */
	if ((ret = _thread_fd_table_init(fd)) == 0) {
		/* Clear the interrupted flag: */
		_thread_run->interrupted = 0;

		/*
		 * Lock the file descriptor table entry to prevent
		 * other threads for clashing with the current
		 * thread's accesses:
		 */
		_SPINLOCK(&_thread_fd_table[fd]->lock);

		/* Check the file descriptor and lock types: */
		if (lock_type == FD_READ || lock_type == FD_RDWR) {
			/*
			 * Wait for the file descriptor to be locked
			 * for read for the current thread: 
			 */
			while ((_thread_fd_table[fd]->r_owner != _thread_run) &&
			    (_thread_run->interrupted == 0)) {
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
					FDQ_INSERT(&_thread_fd_table[fd]->r_queue, _thread_run);

					/*
					 * Save the file descriptor details
					 * in the thread structure for the
					 * running thread: 
					 */
					_thread_run->data.fd.fd = fd;

					/* Set the timeout: */
					_thread_kern_set_timeout(timeout);

					/*
					 * Unlock the file descriptor
					 * table entry:
					 */
					_SPINUNLOCK(&_thread_fd_table[fd]->lock);

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
					_SPINLOCK(&_thread_fd_table[fd]->lock);

					if (_thread_run->interrupted != 0) {
						FDQ_REMOVE(&_thread_fd_table[fd]->r_queue,
						    _thread_run);
					}
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
				}
			}

			if (_thread_fd_table[fd]->r_owner == _thread_run)
				/* Increment the read lock count: */
				_thread_fd_table[fd]->r_lockcount++;
		}

		/* Check the file descriptor and lock types: */
		if (_thread_run->interrupted == 0 &&
		    (lock_type == FD_WRITE || lock_type == FD_RDWR)) {
			/*
			 * Wait for the file descriptor to be locked
			 * for write for the current thread: 
			 */
			while ((_thread_fd_table[fd]->w_owner != _thread_run) &&
			    (_thread_run->interrupted == 0)) {
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
					FDQ_INSERT(&_thread_fd_table[fd]->w_queue, _thread_run);

					/*
					 * Save the file descriptor details
					 * in the thread structure for the
					 * running thread: 
					 */
					_thread_run->data.fd.fd = fd;

					/* Set the timeout: */
					_thread_kern_set_timeout(timeout);

					/*
					 * Unlock the file descriptor
					 * table entry:
					 */
					_SPINUNLOCK(&_thread_fd_table[fd]->lock);

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
					_SPINLOCK(&_thread_fd_table[fd]->lock);

					if (_thread_run->interrupted != 0) {
						FDQ_REMOVE(&_thread_fd_table[fd]->w_queue,
						    _thread_run);
					}
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
				}
			}

			if (_thread_fd_table[fd]->w_owner == _thread_run)
				/* Increment the write lock count: */
				_thread_fd_table[fd]->w_lockcount++;
		}

		/* Unlock the file descriptor table entry: */
		_SPINUNLOCK(&_thread_fd_table[fd]->lock);

		if (_thread_run->interrupted != 0) {
			ret = -1;
			errno = EINTR;
			if (_thread_run->continuation != NULL)
				_thread_run->continuation((void *)_thread_run);
		}
	}

	/* Return the completion status: */
	return (ret);
}

void
_thread_fd_unlock_debug(int fd, int lock_type, char *fname, int lineno)
{
	int	ret;

	/*
	 * Check that the file descriptor table is initialised for this
	 * entry: 
	 */
	if ((ret = _thread_fd_table_init(fd)) == 0) {
		/*
		 * Defer signals to protect the scheduling queues from
		 * access by the signal handler:
		 */
		_thread_kern_sig_defer();

		/*
		 * Lock the file descriptor table entry to prevent
		 * other threads for clashing with the current
		 * thread's accesses:
		 */
		_spinlock_debug(&_thread_fd_table[fd]->lock, fname, lineno);

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
				else if ((_thread_fd_table[fd]->r_owner = fd_next_reader(fd)) == NULL) {
				} else {
					/* Remove this thread from the queue: */
					FDQ_REMOVE(&_thread_fd_table[fd]->r_queue,
					    _thread_fd_table[fd]->r_owner);

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
				else if ((_thread_fd_table[fd]->w_owner = fd_next_writer(fd)) == NULL) {
				} else {
					/* Remove this thread from the queue: */
					FDQ_REMOVE(&_thread_fd_table[fd]->w_queue,
					    _thread_fd_table[fd]->w_owner);

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
		_SPINUNLOCK(&_thread_fd_table[fd]->lock);

		/*
		 * Undefer and handle pending signals, yielding if
		 * necessary.
		 */
		_thread_kern_sig_undefer();
	}
}

int
_thread_fd_lock_debug(int fd, int lock_type, struct timespec * timeout,
		char *fname, int lineno)
{
	int	ret;

	/*
	 * Check that the file descriptor table is initialised for this
	 * entry: 
	 */
	if ((ret = _thread_fd_table_init(fd)) == 0) {
		/* Clear the interrupted flag: */
		_thread_run->interrupted = 0;

		/*
		 * Lock the file descriptor table entry to prevent
		 * other threads for clashing with the current
		 * thread's accesses:
		 */
		_spinlock_debug(&_thread_fd_table[fd]->lock, fname, lineno);

		/* Check the file descriptor and lock types: */
		if (lock_type == FD_READ || lock_type == FD_RDWR) {
			/*
			 * Wait for the file descriptor to be locked
			 * for read for the current thread: 
			 */
			while ((_thread_fd_table[fd]->r_owner != _thread_run) &&
			    (_thread_run->interrupted == 0)) {
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
					FDQ_INSERT(&_thread_fd_table[fd]->r_queue, _thread_run);

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
					_SPINUNLOCK(&_thread_fd_table[fd]->lock);

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
					_SPINLOCK(&_thread_fd_table[fd]->lock);

					if (_thread_run->interrupted != 0) {
						FDQ_REMOVE(&_thread_fd_table[fd]->r_queue,
						    _thread_run);
					}
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

			if (_thread_fd_table[fd]->r_owner == _thread_run)
				/* Increment the read lock count: */
				_thread_fd_table[fd]->r_lockcount++;
		}

		/* Check the file descriptor and lock types: */
		if (_thread_run->interrupted == 0 &&
		    (lock_type == FD_WRITE || lock_type == FD_RDWR)) {
			/*
			 * Wait for the file descriptor to be locked
			 * for write for the current thread: 
			 */
			while ((_thread_fd_table[fd]->w_owner != _thread_run) &&
			    (_thread_run->interrupted == 0)) {
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
					FDQ_INSERT(&_thread_fd_table[fd]->w_queue, _thread_run);

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
					_SPINUNLOCK(&_thread_fd_table[fd]->lock);

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
					_SPINLOCK(&_thread_fd_table[fd]->lock);

					if (_thread_run->interrupted != 0) {
						FDQ_REMOVE(&_thread_fd_table[fd]->w_queue,
						    _thread_run);
					}
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

			if (_thread_fd_table[fd]->w_owner == _thread_run)
				/* Increment the write lock count: */
				_thread_fd_table[fd]->w_lockcount++;
		}

		/* Unlock the file descriptor table entry: */
		_SPINUNLOCK(&_thread_fd_table[fd]->lock);

		if (_thread_run->interrupted != 0) {
			ret = -1;
			errno = EINTR;
			if (_thread_run->continuation != NULL)
				_thread_run->continuation((void *)_thread_run);
		}
	}

	/* Return the completion status: */
	return (ret);
}

void
_thread_fd_unlock_owned(pthread_t pthread)
{
	int fd;

	for (fd = 0; fd < _thread_dtablesize; fd++) {
		if ((_thread_fd_table[fd] != NULL) &&
		    ((_thread_fd_table[fd]->r_owner == pthread) ||
		    (_thread_fd_table[fd]->w_owner == pthread))) {
			/*
			 * Defer signals to protect the scheduling queues
			 * from access by the signal handler:
			 */
			_thread_kern_sig_defer();

			/*
			 * Lock the file descriptor table entry to prevent
			 * other threads for clashing with the current
			 * thread's accesses:
			 */
			_SPINLOCK(&_thread_fd_table[fd]->lock);

			/* Check if the thread owns the read lock: */
			if (_thread_fd_table[fd]->r_owner == pthread) {
				/* Clear the read lock count: */
				_thread_fd_table[fd]->r_lockcount = 0;

				/*
				 * Get the next thread in the queue for a
				 * read lock on this file descriptor: 
				 */
				if ((_thread_fd_table[fd]->r_owner = fd_next_reader(fd)) != NULL) {
					/* Remove this thread from the queue: */
					FDQ_REMOVE(&_thread_fd_table[fd]->r_queue,
					    _thread_fd_table[fd]->r_owner);

					/*
					 * Set the state of the new owner of
					 * the thread to running: 
					 */
					PTHREAD_NEW_STATE(_thread_fd_table[fd]->r_owner,PS_RUNNING);
				}
			}

			/* Check if the thread owns the write lock: */
			if (_thread_fd_table[fd]->w_owner == pthread) {
				/* Clear the write lock count: */
				_thread_fd_table[fd]->w_lockcount = 0;

				/*
				 * Get the next thread in the queue for a
				 * write lock on this file descriptor: 
				 */
				if ((_thread_fd_table[fd]->w_owner = fd_next_writer(fd)) != NULL) {
					/* Remove this thread from the queue: */
					FDQ_REMOVE(&_thread_fd_table[fd]->w_queue,
					    _thread_fd_table[fd]->w_owner);

					/*
					 * Set the state of the new owner of
					 * the thread to running: 
					 */
					PTHREAD_NEW_STATE(_thread_fd_table[fd]->w_owner,PS_RUNNING);

				}
			}

			/* Unlock the file descriptor table entry: */
			_SPINUNLOCK(&_thread_fd_table[fd]->lock);

			/*
			 * Undefer and handle pending signals, yielding if
			 * necessary.
			 */
			_thread_kern_sig_undefer();
		}
	}
}

void
_fd_lock_backout(pthread_t pthread)
{
	int	fd;

	/*
	 * Defer signals to protect the scheduling queues
	 * from access by the signal handler:
	 */
	_thread_kern_sig_defer();

	switch (pthread->state) {

	case PS_FDLR_WAIT:
		fd = pthread->data.fd.fd;

		/*
		 * Lock the file descriptor table entry to prevent
		 * other threads for clashing with the current
		 * thread's accesses:
		 */
		_SPINLOCK(&_thread_fd_table[fd]->lock);

		/* Remove the thread from the waiting queue: */
		FDQ_REMOVE(&_thread_fd_table[fd]->r_queue, pthread);
		break;

	case PS_FDLW_WAIT:
		fd = pthread->data.fd.fd;

		/*
		 * Lock the file descriptor table entry to prevent
		 * other threads from clashing with the current
		 * thread's accesses:
		 */
		_SPINLOCK(&_thread_fd_table[fd]->lock);

		/* Remove the thread from the waiting queue: */
		FDQ_REMOVE(&_thread_fd_table[fd]->w_queue, pthread);
		break;

	default:
		break;
	}

	/*
	 * Undefer and handle pending signals, yielding if
	 * necessary.
	 */
	_thread_kern_sig_undefer();
}

static inline pthread_t
fd_next_reader(int fd)
{
	pthread_t pthread;

	while (((pthread = TAILQ_FIRST(&_thread_fd_table[fd]->r_queue)) != NULL) &&
	    (pthread->interrupted != 0)) {
		/*
		 * This thread has either been interrupted by a signal or
		 * it has been canceled.  Remove it from the queue.
		 */
		FDQ_REMOVE(&_thread_fd_table[fd]->r_queue, pthread);
	}

	return (pthread);
}

static inline pthread_t
fd_next_writer(int fd)
{
	pthread_t pthread;

	while (((pthread = TAILQ_FIRST(&_thread_fd_table[fd]->w_queue)) != NULL) &&
	    (pthread->interrupted != 0)) {
		/*
		 * This thread has either been interrupted by a signal or
		 * it has been canceled.  Remove it from the queue.
		 */
		FDQ_REMOVE(&_thread_fd_table[fd]->w_queue, pthread);
	}

	return (pthread);
}
#endif
