/*
 * Copyright (c) 1993, 1994 by Chris Provenzano, proven@mit.edu
 * Copyright (c) 1995-1998 by John Birrell <jb@cimlogic.com.au>
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
 * $FreeBSD$
 */
#ifndef _PTHREAD_H_
#define _PTHREAD_H_

/*
 * Header files.
 */
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <limits.h>
#include <sched.h>

/*
 * Run-time invariant values:
 */
#define PTHREAD_DESTRUCTOR_ITERATIONS		4
#define PTHREAD_KEYS_MAX			256
#define PTHREAD_STACK_MIN			1024
#define PTHREAD_THREADS_MAX			ULONG_MAX

/*
 * Flags for threads and thread attributes.
 */
#define PTHREAD_DETACHED            0x1
#define PTHREAD_SCOPE_SYSTEM        0x2
#define PTHREAD_INHERIT_SCHED       0x4
#define PTHREAD_NOFLOAT             0x8

#define PTHREAD_CREATE_DETACHED     PTHREAD_DETACHED
#define PTHREAD_CREATE_JOINABLE     0
#define PTHREAD_SCOPE_PROCESS       0
#define PTHREAD_EXPLICIT_SCHED      0

/*
 * Flags for read/write lock attributes
 */
#define PTHREAD_PROCESS_PRIVATE     0
#define PTHREAD_PROCESS_SHARED      1	

/*
 * Flags for cancelling threads
 */
#define PTHREAD_CANCEL_ENABLE		0
#define PTHREAD_CANCEL_DISABLE		1
#define PTHREAD_CANCEL_DEFERRED		0
#define PTHREAD_CANCEL_ASYNCHRONOUS	2
#define PTHREAD_CANCELED		((void *) 1)

/*
 * Forward structure definitions.
 *
 * These are mostly opaque to the user.
 */
struct pthread;
struct pthread_attr;
struct pthread_cond;
struct pthread_cond_attr;
struct pthread_mutex;
struct pthread_mutex_attr;
struct pthread_once;
struct pthread_rwlock;
struct pthread_rwlockattr;

/*
 * Primitive system data type definitions required by P1003.1c.
 *
 * Note that P1003.1c specifies that there are no defined comparison
 * or assignment operators for the types pthread_attr_t, pthread_cond_t,
 * pthread_condattr_t, pthread_mutex_t, pthread_mutexattr_t.
 */
typedef struct	pthread			*pthread_t;
typedef struct	pthread_attr		*pthread_attr_t;
typedef struct	pthread_mutex		*pthread_mutex_t;
typedef struct	pthread_mutex_attr	*pthread_mutexattr_t;
typedef struct	pthread_cond		*pthread_cond_t;
typedef struct	pthread_cond_attr	*pthread_condattr_t;
typedef int     			pthread_key_t;
typedef struct	pthread_once		pthread_once_t;
typedef struct	pthread_rwlock		*pthread_rwlock_t;
typedef struct	pthread_rwlockattr	*pthread_rwlockattr_t;

/*
 * Additional type definitions:
 *
 * Note that P1003.1c reserves the prefixes pthread_ and PTHREAD_ for
 * use in header symbols.
 */
typedef void	*pthread_addr_t;
typedef void	*(*pthread_startroutine_t) __P((void *));

/*
 * Once definitions.
 */
struct pthread_once {
	int		state;
	pthread_mutex_t	mutex;
};

/*
 * Flags for once initialization.
 */
#define PTHREAD_NEEDS_INIT  0
#define PTHREAD_DONE_INIT   1

/*
 * Static once initialization values. 
 */
#define PTHREAD_ONCE_INIT   { PTHREAD_NEEDS_INIT, NULL }

/*
 * Static initialization values. 
 */
#define PTHREAD_MUTEX_INITIALIZER	NULL
#define PTHREAD_COND_INITIALIZER	NULL
#define PTHREAD_RWLOCK_INITIALIZER	NULL

/*
 * Default attribute arguments (draft 4, deprecated).
 */
#ifndef PTHREAD_KERNEL
#define pthread_condattr_default    NULL
#define pthread_mutexattr_default   NULL
#define pthread_attr_default        NULL
#endif

#define PTHREAD_PRIO_NONE	0
#define PTHREAD_PRIO_INHERIT	1
#define PTHREAD_PRIO_PROTECT	2

/*
 * Mutex types (Single UNIX Specification, Version 2, 1997).
 *
 * Note that a mutex attribute with one of the following types:
 *
 *	PTHREAD_MUTEX_NORMAL
 *	PTHREAD_MUTEX_RECURSIVE
 *      MUTEX_TYPE_FAST (deprecated)
 *	MUTEX_TYPE_COUNTING_FAST (deprecated)
 *
 * will deviate from POSIX specified semantics.
 */
enum pthread_mutextype {
	PTHREAD_MUTEX_ERRORCHECK	= 1,	/* Default POSIX mutex */
	PTHREAD_MUTEX_RECURSIVE		= 2,	/* Recursive mutex */
	PTHREAD_MUTEX_NORMAL		= 3,	/* No error checking */
	MUTEX_TYPE_MAX
};

#define PTHREAD_MUTEX_DEFAULT		PTHREAD_MUTEX_ERRORCHECK
#define MUTEX_TYPE_FAST			PTHREAD_MUTEX_NORMAL
#define MUTEX_TYPE_COUNTING_FAST	PTHREAD_MUTEX_RECURSIVE

/*
 * Thread function prototype definitions:
 */
__BEGIN_DECLS
int		pthread_attr_destroy __P((pthread_attr_t *));
int		pthread_attr_getstack __P((const pthread_attr_t * __restrict,
			void ** __restrict stackaddr,
			size_t * __restrict stacksize));
int		pthread_attr_getstacksize __P((const pthread_attr_t *,
			size_t *));
int		pthread_attr_getstackaddr __P((const pthread_attr_t *,
			void **));
int		pthread_attr_getdetachstate __P((const pthread_attr_t *,
			int *));
int		pthread_attr_init __P((pthread_attr_t *));
int		pthread_attr_setstack __P((pthread_attr_t *, void *, size_t));
int		pthread_attr_setstacksize __P((pthread_attr_t *, size_t));
int		pthread_attr_setstackaddr __P((pthread_attr_t *, void *));
int		pthread_attr_setdetachstate __P((pthread_attr_t *, int));
void		pthread_cleanup_pop __P((int));
void		pthread_cleanup_push __P((void (*) (void *),
			void *routine_arg));
int		pthread_condattr_destroy __P((pthread_condattr_t *));
int		pthread_condattr_init __P((pthread_condattr_t *));

#if defined(_POSIX_THREAD_PROCESS_SHARED)
int		pthread_condattr_getpshared __P((pthread_condattr_t *,
			int *));
int		pthread_condattr_setpshared __P((pthread_condattr_t *,
			int));
#endif

int		pthread_cond_broadcast __P((pthread_cond_t *));
int		pthread_cond_destroy __P((pthread_cond_t *));
int		pthread_cond_init __P((pthread_cond_t *,
			const pthread_condattr_t *));
int		pthread_cond_signal __P((pthread_cond_t *));
int		pthread_cond_timedwait __P((pthread_cond_t *,
			pthread_mutex_t *, const struct timespec *));
int		pthread_cond_wait __P((pthread_cond_t *, pthread_mutex_t *));
int		pthread_create __P((pthread_t *, const pthread_attr_t *,
			void *(*) (void *), void *));
int		pthread_detach __P((pthread_t));
int		pthread_equal __P((pthread_t, pthread_t));
void		pthread_exit __P((void *)) __dead2;
void		*pthread_getspecific __P((pthread_key_t));
int		pthread_join __P((pthread_t, void **));
int		pthread_key_create __P((pthread_key_t *,
			void (*) (void *)));
int		pthread_key_delete __P((pthread_key_t));
int		pthread_kill __P((pthread_t, int));
int		pthread_mutexattr_init __P((pthread_mutexattr_t *));
int		pthread_mutexattr_destroy __P((pthread_mutexattr_t *));
int		pthread_mutexattr_gettype __P((pthread_mutexattr_t *, int *));
int		pthread_mutexattr_settype __P((pthread_mutexattr_t *, int));
int		pthread_mutex_destroy __P((pthread_mutex_t *));
int		pthread_mutex_init __P((pthread_mutex_t *,
			const pthread_mutexattr_t *));
int		pthread_mutex_lock __P((pthread_mutex_t *));
int		pthread_mutex_trylock __P((pthread_mutex_t *));
int		pthread_mutex_unlock __P((pthread_mutex_t *));
int		pthread_once __P((pthread_once_t *,
			void (*) (void)));
int		pthread_rwlock_destroy __P((pthread_rwlock_t *));
int		pthread_rwlock_init __P((pthread_rwlock_t *,
			const pthread_rwlockattr_t *));
int		pthread_rwlock_rdlock __P((pthread_rwlock_t *));
int		pthread_rwlock_tryrdlock __P((pthread_rwlock_t *));
int		pthread_rwlock_trywrlock __P((pthread_rwlock_t *));
int		pthread_rwlock_unlock __P((pthread_rwlock_t *));
int		pthread_rwlock_wrlock __P((pthread_rwlock_t *));
int		pthread_rwlockattr_init __P((pthread_rwlockattr_t *));
int		pthread_rwlockattr_getpshared __P((const pthread_rwlockattr_t *,
			int *));
int		pthread_rwlockattr_setpshared __P((pthread_rwlockattr_t *,
			int));
int		pthread_rwlockattr_destroy __P((pthread_rwlockattr_t *));
pthread_t	pthread_self __P((void));
int		pthread_setspecific __P((pthread_key_t, const void *));
int		pthread_sigmask __P((int, const sigset_t *, sigset_t *));

int		pthread_cancel __P((pthread_t));
int		pthread_setcancelstate __P((int, int *));
int		pthread_setcanceltype __P((int, int *));
void		pthread_testcancel __P((void));

int		pthread_getprio __P((pthread_t));
int		pthread_setprio __P((pthread_t, int));
void		pthread_yield __P((void));

#if defined(_POSIX_THREAD_PROCESS_SHARED)
int		pthread_mutexattr_getpshared __P((pthread_mutexattr_t *,
			int *pshared));
int		pthread_mutexattr_setpshared __P((pthread_mutexattr_t *,
			int pshared));
#endif

int		pthread_mutexattr_getprioceiling __P((pthread_mutexattr_t *,
			int *));
int		pthread_mutexattr_setprioceiling __P((pthread_mutexattr_t *,
			int));
int		pthread_mutex_getprioceiling __P((pthread_mutex_t *, int *));
int		pthread_mutex_setprioceiling __P((pthread_mutex_t *, int, int *));

int		pthread_mutexattr_getprotocol __P((pthread_mutexattr_t *,
			int *));
int		pthread_mutexattr_setprotocol __P((pthread_mutexattr_t *,
			int));

int		pthread_attr_getinheritsched __P((const pthread_attr_t *, int *));
int		pthread_attr_getschedparam __P((const pthread_attr_t *,
			struct sched_param *));
int		pthread_attr_getschedpolicy __P((const pthread_attr_t *, int *));
int		pthread_attr_getscope __P((const pthread_attr_t *, int *));
int		pthread_attr_setinheritsched __P((pthread_attr_t *, int));
int		pthread_attr_setschedparam __P((pthread_attr_t *,
			const struct sched_param *));
int		pthread_attr_setschedpolicy __P((pthread_attr_t *, int));
int		pthread_attr_setscope __P((pthread_attr_t *, int));
int		pthread_getschedparam __P((pthread_t pthread, int *,
			struct sched_param *));
int		pthread_setschedparam __P((pthread_t, int,
			const struct sched_param *));
int		pthread_getconcurrency __P((void));
int		pthread_setconcurrency __P((int));

int		pthread_attr_setfloatstate __P((pthread_attr_t *, int));
int		pthread_attr_getfloatstate __P((pthread_attr_t *, int *));
__END_DECLS

#endif
