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

/*
 * Forward thread structure definition. This is opaque to the user.
 */
struct pthread;

/*
 * Queue definitions.
 */
struct pthread_queue {
	struct pthread	*q_next;
	struct pthread	*q_last;
	void		*q_data;
};

/*
 * Static queue initialization values. 
 */
#define PTHREAD_QUEUE_INITIALIZER { NULL, NULL, NULL }

/* 
 * Mutex definitions.
 */
enum pthread_mutextype {
	MUTEX_TYPE_FAST		= 1,
	MUTEX_TYPE_COUNTING_FAST	= 2,	/* Recursive */
	MUTEX_TYPE_MAX
};

union pthread_mutex_data {
	void	*m_ptr;
	int	m_count;
};

struct pthread_mutex {
	enum pthread_mutextype		m_type;
	struct pthread_queue		m_queue;
	struct pthread			*m_owner;
	union pthread_mutex_data	m_data;
	long				m_flags;
};

/*
 * Flags for mutexes. 
 */
#define MUTEX_FLAGS_PRIVATE	0x01
#define MUTEX_FLAGS_INITED	0x02
#define MUTEX_FLAGS_BUSY	0x04

/*
 * Static mutex initialization values. 
 */
#define PTHREAD_MUTEX_INITIALIZER   \
	{ MUTEX_TYPE_FAST, PTHREAD_QUEUE_INITIALIZER, \
	NULL, { NULL }, MUTEX_FLAGS_INITED }

struct pthread_mutex_attr {
	enum pthread_mutextype	m_type;
	long			m_flags;
};

/* 
 * Condition variable definitions.
 */
enum pthread_cond_type {
	COND_TYPE_FAST,
	COND_TYPE_MAX
};

struct pthread_cond {
	enum pthread_cond_type	c_type;
	struct pthread_queue	c_queue;
	void			*c_data;
	long			c_flags;
};

struct pthread_cond_attr {
	enum pthread_cond_type	c_type;
	long			c_flags;
};

/*
 * Flags for condition variables.
 */
#define COND_FLAGS_PRIVATE	0x01
#define COND_FLAGS_INITED	0x02
#define COND_FLAGS_BUSY		0x04

/*
 * Static cond initialization values. 
 */
#define PTHREAD_COND_INITIALIZER    \
	{ COND_TYPE_FAST, PTHREAD_QUEUE_INITIALIZER, NULL, COND_FLAGS_INITED }

/*
 * Cleanup definitions.
 */
struct pthread_cleanup {
	struct pthread_cleanup	*next;
	void			(*routine) ();
	void			*routine_arg;
};

/*
 * Scheduling definitions.
 */
enum schedparam_policy {
	SCHED_RR,
	SCHED_IO,
	SCHED_FIFO,
	SCHED_OTHER
};

struct pthread_attr {
	enum schedparam_policy	schedparam_policy;
	int			prio;
	int			suspend;
	int			flags;
	void			*arg_attr;
	void			(*cleanup_attr) ();
	void			*stackaddr_attr;
	size_t			stacksize_attr;
};

struct sched_param {
	int	prio;
	void	*no_data;
};

/*
 * Once definitions.
 */
struct pthread_once {
	int			state;
	struct pthread_mutex	mutex;
};

/*
 * Flags for once initialization.
 */
#define PTHREAD_NEEDS_INIT  0
#define PTHREAD_DONE_INIT   1

/*
 * Static once initialization values. 
 */
#define PTHREAD_ONCE_INIT   { PTHREAD_NEEDS_INIT, PTHREAD_MUTEX_INITIALIZER }

/*
 * Type definitions.
 */
typedef int     pthread_key_t;
typedef struct	pthread			*pthread_t;
typedef struct	pthread_attr		pthread_attr_t;
typedef struct	pthread_cond		pthread_cond_t;
typedef struct	pthread_cond_attr	pthread_condattr_t;
typedef struct	pthread_mutex		pthread_mutex_t;
typedef struct	pthread_mutex_attr	pthread_mutexattr_t;
typedef struct	pthread_once		pthread_once_t;
typedef void	*pthread_addr_t;
typedef void	*(*pthread_startroutine_t) (void *);

/*
 * Default attribute arguments.
 */
#define pthread_condattr_default    NULL
#define pthread_mutexattr_default   NULL
#ifndef PTHREAD_KERNEL
#define pthread_attr_default        NULL
#endif

/*
 * Thread function prototype definitions:
 */
__BEGIN_DECLS
int  pthread_create __P((pthread_t *, const pthread_attr_t *,
     void *(*start_routine) (void *), void *));
void pthread_exit __P((void *));
pthread_t pthread_self __P((void));
int  pthread_equal __P((pthread_t, pthread_t));
int  pthread_getprio __P((pthread_t));
int  pthread_setprio __P((pthread_t, int));
int  pthread_join __P((pthread_t, void **));
int  pthread_detach __P((pthread_t *));
int  pthread_resume __P((pthread_t));
int  pthread_suspend __P((pthread_t));
void pthread_yield __P((void));
int  pthread_setschedparam __P((pthread_t pthread, int policy,
     struct sched_param * param));
int  pthread_getschedparam __P((pthread_t pthread, int *policy,
     struct sched_param * param));
int  pthread_kill __P((struct pthread *, int));
int  pthread_cleanup_push __P((void (*routine) (void *), void *routine_arg));
void pthread_cleanup_pop __P((int execute));
int  pthread_cond_init __P((pthread_cond_t *, const pthread_condattr_t *));
int  pthread_cond_timedwait __P((pthread_cond_t *, pthread_mutex_t *,
     const struct timespec * abstime));
int  pthread_cond_wait __P((pthread_cond_t *, pthread_mutex_t *));
int  pthread_cond_signal __P((pthread_cond_t *));
int  pthread_cond_broadcast __P((pthread_cond_t *));
int  pthread_cond_destroy __P((pthread_cond_t *));
int  pthread_mutex_init __P((pthread_mutex_t *, const pthread_mutexattr_t *));
int  pthread_mutex_lock __P((pthread_mutex_t *));
int  pthread_mutex_unlock __P((pthread_mutex_t *));
int  pthread_mutex_trylock __P((pthread_mutex_t *));
int  pthread_mutex_destroy __P((pthread_mutex_t *));
int  pthread_attr_init __P((pthread_attr_t *));
int  pthread_attr_destroy __P((pthread_attr_t *));
int  pthread_attr_setstacksize __P((pthread_attr_t *, size_t));
int  pthread_attr_getstacksize __P((pthread_attr_t *, size_t *));
int  pthread_attr_setstackaddr __P((pthread_attr_t *, void *));
int  pthread_attr_getstackaddr __P((pthread_attr_t *, void **));
int  pthread_attr_setdetachstate __P((pthread_attr_t *, int));
int  pthread_attr_getdetachstate __P((pthread_attr_t *, int *));
int  pthread_attr_setscope __P((pthread_attr_t *, int));
int  pthread_attr_getscope __P((pthread_attr_t *, int *));
int  pthread_attr_setinheritsched __P((pthread_attr_t *, int));
int  pthread_attr_getinheritsched __P((pthread_attr_t *, int *));
int  pthread_attr_setschedpolicy __P((pthread_attr_t *, int));
int  pthread_attr_getschedpolicy __P((pthread_attr_t *, int *));
int  pthread_attr_setschedparam __P((pthread_attr_t *, struct sched_param *));
int  pthread_attr_getschedparam __P((pthread_attr_t *, struct sched_param *));
int  pthread_attr_setfloatstate __P((pthread_attr_t *, int));
int  pthread_attr_getfloatstate __P((pthread_attr_t *, int *));
int  pthread_attr_setcleanup __P((pthread_attr_t *, void (*routine) (void *), void *));
int  pthread_attr_setcreatesuspend __P((pthread_attr_t *));
int  pthread_once __P((pthread_once_t *, void (*init_routine) (void)));
int  pthread_keycreate __P((pthread_key_t *, void (*routine) (void *)));
int  pthread_setspecific __P((pthread_key_t, const void *));
int  pthread_getspecific __P((pthread_key_t, void **));
int  pthread_key_delete __P((pthread_key_t));
__END_DECLS

#endif
