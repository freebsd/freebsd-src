/*
 * Copyright (c) 1993, 1994 by Chris Provenzano, proven@mit.edu
 * Copyright (c) 1995 by John Birrell <jb@cimlogic.com.au>
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

/*
 * Run-time invariant values:
 */
#define PTHREAD_DESTRUCTOR_ITERATIONS		4
#define PTHREAD_KEYS_MAX			256
#define PTHREAD_STACK_MIN			1024
#define PTHREAD_THREADS_MAX			ULONG_MAX

/*
 * Compile time symbolic constants for portability specifications:
 *
 * Note that those commented out are not currently supported by the
 * implementation.
 */
#define _POSIX_THREADS
#define _POSIX_THREAD_ATTR_STACKADDR
#define _POSIX_THREAD_ATTR_STACKSIZE
#define _POSIX_THREAD_PRIORITY_SCHEDULING
/* #define _POSIX_THREAD_PRIO_INHERIT   */
/* #define _POSIX_THREAD_PRIO_PROTECT   */
/* #define _POSIX_THREAD_PROCESS_SHARED */
#define _POSIX_THREAD_SAFE_FUNCTIONS

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
struct sched_param;

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

/*
 * Additional type definitions:
 *
 * Note that P1003.1c reserves the prefixes pthread_ and PTHREAD_ for
 * use in header symbols.
 */
typedef void	*pthread_addr_t;
typedef void	*(*pthread_startroutine_t) (void *);

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
 * Default attribute arguments.
 */
#define pthread_condattr_default    NULL
#ifndef PTHREAD_KERNEL
#define pthread_mutexattr_default   NULL
#define pthread_attr_default        NULL
#endif

enum pthread_mutextype {
	MUTEX_TYPE_FAST			= 1,
	MUTEX_TYPE_COUNTING_FAST	= 2,	/* Recursive */
	MUTEX_TYPE_MAX
};

/*
 * Thread function prototype definitions:
 */
__BEGIN_DECLS
int		pthread_attr_destroy __P((pthread_attr_t *));
int		pthread_attr_getinheritsched __P((pthread_attr_t *, int *));
int		pthread_attr_getschedparam __P((pthread_attr_t *,
			struct sched_param *));
int		pthread_attr_getschedpolicy __P((pthread_attr_t *, int *));
int		pthread_attr_getscope __P((pthread_attr_t *, int *));
int		pthread_attr_getstacksize __P((pthread_attr_t *, size_t *));
int		pthread_attr_getstackaddr __P((pthread_attr_t *, void **));
int		pthread_attr_getdetachstate __P((pthread_attr_t *, int *));
int		pthread_attr_init __P((pthread_attr_t *));
int		pthread_attr_setinheritsched __P((pthread_attr_t *, int));
int		pthread_attr_setschedparam __P((pthread_attr_t *,
			struct sched_param *));
int		pthread_attr_setschedpolicy __P((pthread_attr_t *, int));
int		pthread_attr_setscope __P((pthread_attr_t *, int));
int		pthread_attr_setstacksize __P((pthread_attr_t *, size_t));
int		pthread_attr_setstackaddr __P((pthread_attr_t *, void *));
int		pthread_attr_setdetachstate __P((pthread_attr_t *, int));
void		pthread_cleanup_pop __P((int execute));
int		pthread_cleanup_push __P((void (*routine) (void *),
			void *routine_arg));
int		pthread_condattr_destroy __P((pthread_condattr_t *attr));
int		pthread_condattr_init __P((pthread_condattr_t *attr));
int		pthread_condattr_getpshared __P((pthread_condattr_t *attr,
			int *pshared));
int		pthread_condattr_setpshared __P((pthread_condattr_t *attr,
			int pshared));
int		pthread_cond_broadcast __P((pthread_cond_t *));
int		pthread_cond_destroy __P((pthread_cond_t *));
int		pthread_cond_init __P((pthread_cond_t *,
			const pthread_condattr_t *));
int		pthread_cond_signal __P((pthread_cond_t *));
int		pthread_cond_timedwait __P((pthread_cond_t *,
			pthread_mutex_t *, const struct timespec * abstime));
int		pthread_cond_wait __P((pthread_cond_t *, pthread_mutex_t *));
int		pthread_create __P((pthread_t *, const pthread_attr_t *,
			void *(*start_routine) (void *), void *));
int		pthread_detach __P((pthread_t *));
int		pthread_equal __P((pthread_t, pthread_t));
void		pthread_exit __P((void *));
void		*pthread_getspecific __P((pthread_key_t));
int		pthread_join __P((pthread_t, void **));
int		pthread_key_create __P((pthread_key_t *,
			void (*routine) (void *)));
int		pthread_key_delete __P((pthread_key_t));
int		pthread_kill __P((struct pthread *, int));
int		pthread_mutexattr_destroy __P((pthread_mutexattr_t *));
int		pthread_mutexattr_getprioceiling __P((pthread_mutexattr_t *,
			int *prioceiling));
int		pthread_mutexattr_getprotocol __P((pthread_mutexattr_t *,
			int *protocol));
int		pthread_mutexattr_getpshared __P((pthread_mutexattr_t *,
			int *pshared));
int		pthread_mutexattr_init __P((pthread_mutexattr_t *));
int		pthread_mutexattr_setprioceiling __P((pthread_mutexattr_t *,
			int prioceiling));
int		pthread_mutexattr_setprotocol __P((pthread_mutexattr_t *,
			int protocol));
int		pthread_mutexattr_setpshared __P((pthread_mutexattr_t *,
			int pshared));
int		pthread_mutex_destroy __P((pthread_mutex_t *));
int		pthread_mutex_getprioceiling __P((pthread_mutex_t *));
int		pthread_mutex_init __P((pthread_mutex_t *,
			const pthread_mutexattr_t *));
int		pthread_mutex_lock __P((pthread_mutex_t *));
int		pthread_mutex_setprioceiling __P((pthread_mutex_t *));
int		pthread_mutex_trylock __P((pthread_mutex_t *));
int		pthread_mutex_unlock __P((pthread_mutex_t *));
int		pthread_once __P((pthread_once_t *,
			void (*init_routine) (void)));
pthread_t	pthread_self __P((void));
int		pthread_setcancelstate __P((int, int *));
int		pthread_setcanceltype __P((int, int *));
int		pthread_setspecific __P((pthread_key_t, const void *));
int		pthread_sigmask __P((int, const sigset_t *, sigset_t *));
int		pthread_testcancel __P((void));


int		pthread_getprio __P((pthread_t));
int		pthread_setprio __P((pthread_t, int));
void		pthread_yield __P((void));
int		pthread_setschedparam __P((pthread_t pthread, int policy,
			struct sched_param * param));
int		pthread_getschedparam __P((pthread_t pthread, int *policy,
			struct sched_param * param));
int		pthread_attr_setfloatstate __P((pthread_attr_t *, int));
int		pthread_attr_getfloatstate __P((pthread_attr_t *, int *));
int		pthread_attr_setcleanup __P((pthread_attr_t *,
			void (*routine) (void *), void *));
__END_DECLS

#endif
