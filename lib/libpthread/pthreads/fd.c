/* ==== fd.c ============================================================
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
 * Description : All the syscalls dealing with fds.
 *
 *  1.00 93/08/14 proven
 *      -Started coding this file.
 *
 *	1.01 93/11/13 proven
 *		-The functions readv() and writev() added.
 */

#include <pthread.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread/posix.h>

/*
 * These first functions really should not be called by the user.
 *
 * I really should dynamically figure out what the table size is.
 */
int dtablesize = 64;
static struct fd_table_entry fd_entry[64];

/* ==========================================================================
 * fd_init()
 */
void fd_init(void)
{
	int i;

	for (i = 0; i < dtablesize; i++) {
		fd_table[i] = &fd_entry[i];

		fd_table[i]->ops 	= NULL;
		fd_table[i]->type 	= FD_NT;
		fd_table[i]->fd.i 	= NOTOK;
		fd_table[i]->flags 	= 0;
		fd_table[i]->count 	= 0;

		pthread_queue_init(&(fd_table[i]->r_queue));
		pthread_queue_init(&(fd_table[i]->w_queue));

		fd_table[i]->r_owner 	= NULL;
		fd_table[i]->w_owner 	= NULL;
		fd_table[i]->lock 		= SEMAPHORE_CLEAR;
		fd_table[i]->next 		= NULL;
		fd_table[i]->lockcount	= 0;
	}

	/* Currently only initialize first 3 fds. */
	fd_kern_init(0);
	fd_kern_init(1);
	fd_kern_init(2);
	printf ("Warning: threaded process may have changed open file descriptors\n");
}

/* ==========================================================================
 * fd_allocate()
 */
int fd_allocate()
{
	semaphore *lock;
	int i;

	for (i = 0; i < dtablesize; i++) {
		lock = &(fd_table[i]->lock);
		while (SEMAPHORE_TEST_AND_SET(lock)) {
			continue;
		}
		if (fd_table[i]->count || fd_table[i]->r_owner
		  || fd_table[i]->w_owner) {
			SEMAPHORE_RESET(lock);
			continue;
		}
		if (fd_table[i]->type == FD_NT) {
			/* Test to see if the kernel version is in use */
			/* If so continue; */
		}
		fd_table[i]->count++;
		SEMAPHORE_RESET(lock);
		return(i);
	}
	pthread_run->error = ENFILE;
	return(NOTOK);
}

/* ==========================================================================
 * fd_free()
 *
 * Assumes fd is locked and owner by pthread_run
 * Don't clear the queues, fd_unlock will do that.
 */
int fd_free(int fd)
{
	struct fd_table_entry *fd_valid;
	int ret;

	if (ret = --fd_table[fd]->count) {
		/* Separate pthread queue into two distinct queues. */
		fd_valid = fd_table[fd];
		fd_table[fd] = fd_table[fd]->next;
		fd_valid->next = fd_table[fd]->next;
	}

	fd_table[fd]->type 	= FD_NIU;
	fd_table[fd]->fd.i 	= NOTOK;
	fd_table[fd]->next 	= NULL;
	fd_table[fd]->flags = 0;
	fd_table[fd]->count = 0;
	return(ret);
}

/* ==========================================================================
 * fd_basic_unlock()
 * 
 * The real work of unlock without the locking of fd_table[fd].lock.
 */
void fd_basic_unlock(int fd, int lock_type)
{
	struct pthread *pthread;
	semaphore *plock;

	if (fd_table[fd]->r_owner == pthread_run) {
		 if (pthread = pthread_queue_get(&fd_table[fd]->r_queue)) {

            plock = &(pthread->lock);
            while (SEMAPHORE_TEST_AND_SET(plock)) {
                 pthread_yield();
            }
			pthread_queue_deq(&fd_table[fd]->r_queue);
			fd_table[fd]->r_owner = pthread;
            pthread->state = PS_RUNNING;
            SEMAPHORE_RESET(plock);
		} else {
			fd_table[fd]->r_owner = NULL;
        }
	}

	if (fd_table[fd]->w_owner == pthread_run) {
		 if (pthread = pthread_queue_get(&fd_table[fd]->w_queue)) {
            plock = &(pthread->lock);
            while (SEMAPHORE_TEST_AND_SET(plock)) {
                 pthread_yield();
            }
			pthread_queue_deq(&fd_table[fd]->r_queue);
			fd_table[fd]->w_owner = pthread;
            pthread->state = PS_RUNNING;
            SEMAPHORE_RESET(plock);
		} else {
			fd_table[fd]->w_owner = NULL;
        }
	}
}

/* ==========================================================================
 * fd_unlock()
 * If there is a lock count then the function fileunlock will do
 * the unlocking, just return.
 */
void fd_unlock(int fd, int lock_type)
{
	semaphore *lock;

	if (!(fd_table[fd]->lockcount)) {
		lock = &(fd_table[fd]->lock);
		while (SEMAPHORE_TEST_AND_SET(lock)) {
			pthread_yield();
		}
		fd_basic_unlock(fd, lock_type);
		SEMAPHORE_RESET(lock);
	}
}

/* ==========================================================================
 * fd_basic_lock()
 * 
 * The real work of lock without the locking of fd_table[fd].lock.
 * Be sure to leave the lock the same way you found it. i.e. locked.
 */
int fd_basic_lock(unsigned int fd, int lock_type, semaphore * lock)
{
	semaphore *plock;

	/* If not in use return EBADF error */
	if (fd_table[fd]->type == FD_NIU) {
		return(NOTOK);
	}

	/* If not tested, test it and see if it is valid */
	if (fd_table[fd]->type == FD_NT) {
		/* If not ok return EBADF error */
		if (fd_kern_init(fd) != OK) {
			return(NOTOK);
		}
	}
	if ((fd_table[fd]->type == FD_HALF_DUPLEX) ||
	  (lock_type & FD_READ)) {
		if (fd_table[fd]->r_owner) {
			if (fd_table[fd]->r_owner != pthread_run) {
				plock = &(pthread_run->lock);
   	    		while (SEMAPHORE_TEST_AND_SET(plock)) {
   	        		pthread_yield();
   	    		}
   	    		pthread_queue_enq(&fd_table[fd]->r_queue, pthread_run);
   	    		SEMAPHORE_RESET(lock);
	
				/* Reschedule will unlock pthread_run */
				reschedule(PS_FDLR_WAIT);
	
				while(SEMAPHORE_TEST_AND_SET(lock)) {
   	    			pthread_yield();
				}
			} else {
				if (!fd_table[fd]->lockcount) {
					PANIC();
				}
			}
		}
		fd_table[fd]->r_owner = pthread_run;
	}
	if ((fd_table[fd]->type != FD_HALF_DUPLEX) &&
	  (lock_type & FD_WRITE)) {
		if (fd_table[fd]->w_owner) {
			if (fd_table[fd]->w_owner != pthread_run) {
				plock = &(pthread_run->lock);
   	    		while (SEMAPHORE_TEST_AND_SET(plock)) {
   	        		pthread_yield();
   	    		}
   	    		pthread_queue_enq(&fd_table[fd]->w_queue, pthread_run);
   	    		SEMAPHORE_RESET(lock);

				/* Reschedule will unlock pthread_run */
				reschedule(PS_FDLW_WAIT);

				while(SEMAPHORE_TEST_AND_SET(lock)) {
   	    			pthread_yield();
				}
			}
		}
		fd_table[fd]->w_owner = pthread_run;
	}
	if (!fd_table[fd]->count) {
		fd_basic_unlock(fd, lock_type);
        return(NOTOK);
    }
	return(OK);
}

/* ==========================================================================
 * fd_lock()
 */
int fd_lock(unsigned int fd, int lock_type)
{
	semaphore *lock;
	int error;

	if (fd < dtablesize) { 
		lock = &(fd_table[fd]->lock);
		while (SEMAPHORE_TEST_AND_SET(lock)) {
			pthread_yield();
		}
		error = fd_basic_lock(fd, lock_type, lock);
		SEMAPHORE_RESET(lock);
		return(error);
	}
	return(NOTOK);
}


/* ==========================================================================
 * ======================================================================= */

/* ==========================================================================
 * read()
 */
ssize_t read(int fd, void *buf, size_t nbytes)
{
	int ret;

	if ((ret = fd_lock(fd, FD_READ)) == OK) {
     	ret = fd_table[fd]->ops->read(fd_table[fd]->fd,
		  fd_table[fd]->flags, buf, nbytes); 
		fd_unlock(fd, FD_READ);
	} 
	return(ret);
}

/* ==========================================================================
 * readv()
 */
int readv(int fd, const struct iovec *iov, int iovcnt)
{
	int ret;

	if ((ret = fd_lock(fd, FD_READ)) == OK) {
     	ret = fd_table[fd]->ops->readv(fd_table[fd]->fd,
		  fd_table[fd]->flags, iov, iovcnt); 
		fd_unlock(fd, FD_READ);
	} 
	return(ret);
}

/* ==========================================================================
 * write()
 */
ssize_t write(int fd, const void *buf, size_t nbytes)
{
	int ret;

	 if ((ret = fd_lock(fd, FD_WRITE)) == OK) {
     	ret = fd_table[fd]->ops->write(fd_table[fd]->fd,
		  fd_table[fd]->flags, buf, nbytes); 
        fd_unlock(fd, FD_WRITE);
    }
    return(ret);
}

/* ==========================================================================
 * writev()
 */
int writev(int fd, const struct iovec *iov, int iovcnt)
{
	int ret;

	 if ((ret = fd_lock(fd, FD_WRITE)) == OK) {
     	ret = fd_table[fd]->ops->writev(fd_table[fd]->fd,
		  fd_table[fd]->flags, iov, iovcnt); 
        fd_unlock(fd, FD_WRITE);
    }
    return(ret);
}

/* ==========================================================================
 * lseek()
 */
off_t lseek(int fd, off_t offset, int whence)
{
	int ret;

	 if ((ret = fd_lock(fd, FD_RDWR)) == OK) {
     	ret = fd_table[fd]->ops->seek(fd_table[fd]->fd,
		  fd_table[fd]->flags, offset, whence); 
        fd_unlock(fd, FD_RDWR);
    }
    return(ret);
}

/* ==========================================================================
 * close()
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
int close(int fd)
{
	union fd_data realfd;
	int ret, flags;

	if ((ret = fd_lock(fd, FD_RDWR)) == OK) {
		flags = fd_table[fd]->flags;
		realfd = fd_table[fd]->fd;
		if (fd_free(fd) == OK) {
     		ret = fd_table[fd]->ops->close(realfd, flags);
		}
		fd_unlock(fd, FD_RDWR);
	}
	return(ret);
}

/* ==========================================================================
 * fd_basic_dup()
 *
 * Might need to do more than just what's below.
 */
static inline void fd_basic_dup(int fd, int newfd)
{
	fd_table[newfd]->next = fd_table[fd]->next;
	fd_table[fd]->next = fd_table[newfd];
	fd_table[fd]->count++;
}

/* ==========================================================================
 * dup2()
 *
 * Always lock the lower number fd first to avoid deadlocks.
 * newfd must be locked by hand so it can be closed if it is open,
 * or it won't be opened while dup is in progress.
 */
int dup2(fd, newfd)
{
	union fd_data realfd;
	semaphore *lock;
	int ret, flags;

	if (newfd < dtablesize) {
		if (fd < newfd) {
			if ((ret = fd_lock(fd, FD_RDWR)) == OK) {
				/* Need to lock the newfd by hand */
				lock = &(fd_table[newfd]->lock);
				while(SEMAPHORE_TEST_AND_SET(lock)) {
					pthread_yield();
				}

				/* Is it inuse */
				if (fd_basic_lock(newfd, FD_RDWR, lock) == OK) {
					/* free it and check close status */
					flags = fd_table[fd]->flags;
					realfd = fd_table[fd]->fd;
					if (fd_free(fd) == OK) {
     					ret = fd_table[fd]->ops->close(realfd, flags);
					} else {
						/* Lots of work to do */
					}
				}
				fd_basic_dup(fd, newfd);
			}
			fd_unlock(fd, FD_RDWR);
		} else {
			/* Need to lock the newfd by hand */
			lock = &(fd_table[newfd]->lock);
			while(SEMAPHORE_TEST_AND_SET(lock)) {
				pthread_yield();
			}
			if ((ret = fd_lock(fd, FD_RDWR)) == OK) {
			}
			/* Is it inuse */
			if ((ret = fd_basic_lock(newfd, FD_RDWR, lock)) == OK) {
				/* free it and check close status */
				flags = fd_table[fd]->flags;
				realfd = fd_table[fd]->fd;
				if (fd_free(fd) == OK) {
   		  			ret = fd_table[fd]->ops->close(realfd, flags);
				} else {
					/* Lots of work to do */
				}

				fd_basic_dup(fd, newfd);
				fd_unlock(fd, FD_RDWR);
			}
			SEMAPHORE_RESET(lock);
		}
	} else {
		ret = NOTOK;
	}
	return(ret);
			
}

/* ==========================================================================
 * dup()
 */
int dup(int fd)
{
	int ret;

	if ((ret = fd_lock(fd, FD_RDWR)) == OK) {
		ret = fd_allocate();
		fd_basic_dup(fd, ret);
		fd_unlock(fd, FD_RDWR);
	}
	return(ret);
}

/* ==========================================================================
 * fcntl()
 */
int fcntl(int fd, int cmd, ...)
{
	int ret, realfd, flags;
	struct flock *flock;
	semaphore *plock;
	va_list ap;

	flags = 0;
	if ((ret = fd_lock(fd, FD_RDWR)) == OK) {
		va_start(ap, cmd);
		switch(cmd) {
		case F_DUPFD:
			ret = fd_allocate();
			fd_basic_dup(va_arg(ap, int), ret);
			break;
		case F_SETFD:
			flags = va_arg(ap, int);
		case F_GETFD:
     		ret = fd_table[fd]->ops->fcntl(fd_table[fd]->fd,
			  fd_table[fd]->flags, cmd, flags | __FD_NONBLOCK);
			break;
		case F_GETFL:
			ret = fd_table[fd]->flags;
			break;
		case F_SETFL:
			flags = va_arg(ap, int);
     		if ((ret = fd_table[fd]->ops->fcntl(fd_table[fd]->fd,
			  fd_table[fd]->flags, cmd, flags | __FD_NONBLOCK)) == OK) {
				fd_table[fd]->flags = flags;
			}
			break;
/*		case F_SETLKW: */
			/*
			 * Do the same as SETLK but if it fails with EACCES or EAGAIN
			 * block the thread and try again later, not implemented yet
			 */
/*		case F_SETLK: */
/*		case F_GETLK: 
			flock = va_arg(ap, struct flock*);
     		ret = fd_table[fd]->ops->fcntl(fd_table[fd]->fd,
			  fd_table[fd]->flags, cmd, flock);
			break; */
		default:
			/* Might want to make va_arg use a union */
     		ret = fd_table[fd]->ops->fcntl(fd_table[fd]->fd,
			  fd_table[fd]->flags, cmd, va_arg(ap, void*));
			break;
		}
		va_end(ap);
		fd_unlock(fd, FD_RDWR);
	}
	return(ret);
}
