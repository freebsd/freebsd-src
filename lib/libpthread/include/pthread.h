/* ==== pthread.h ============================================================
 * Copyright (c) 1993 by Chris Provenzano, proven@mit.edu
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
 * Description : Basic pthread header.
 *
 *  1.00 93/07/20 proven
 *      -Started coding this file.
 */

#include <pthread/engine.h>
#include <pthread/kernel.h>
#include <pthread/queue.h>
#include <pthread/mutex.h>
#include <pthread/cond.h>
#include <pthread/fd.h>

#include <pthread/util.h>
#include <errno.h>

/* More includes */
#include <pthread/pthread_once.h>

/* More includes, that need size_t */
#include <pthread/pthread_attr.h>

enum pthread_state {
	PS_RUNNING,
	PS_MUTEX_WAIT,
	PS_COND_WAIT,
	PS_FDLR_WAIT,
	PS_FDLW_WAIT,
	PS_FDR_WAIT,
	PS_FDW_WAIT,
    PS_SLEEP_WAIT,
	PS_JOIN,
	PS_DEAD
};

#define PF_DETACHED			0x00000001
	
struct pthread {
	struct machdep_pthread	machdep_data;
	enum pthread_state		state;
	pthread_attr_t			attr;

	/* Other flags */
	int						flags;

	/* Time until timeout */
	int 					time_sec;
	int						time_usec;

	/* Join queue for waiting threads */
	struct pthread_queue	join_queue;

	/* Queue thread is waiting on, (mutexes, cond. etc.) */
	struct pthread_queue	*queue;

	/*
	 * Thread implementations are just multiple queue type implemenations,
	 * Below are the various link lists currently necessary
	 * It is possible for a thread to be on multiple, or even all the
	 * queues at once, much care must be taken during queue manipulation.
	 *
     * The pthread structure must be locked before you can even look at
	 * the link lists.
	 */ 

	struct pthread			*pll;		/* ALL threads, in any state */
	/* struct pthread		*rll;		 Current run queue, before resced */
	struct pthread			*sll;		/* For sleeping threads */
	struct pthread			*next;		/* Standard for mutexes, etc ... */
	/* struct pthread			*fd_next;	 For kernel fd operations */

	int						fd;			/* Used when thread waiting on fd */

	semaphore				lock;
	void 					*ret;
	int						error;
};

typedef struct pthread*			pthread_t;

/*
 * Globals
 */
extern	struct pthread 			*pthread_run;
extern	struct pthread 			*pthread_initial;
extern	struct pthread 			*pthread_link_list;
extern	pthread_attr_t			pthread_default_attr;
extern	struct pthread_queue 	pthread_current_queue;
extern	struct fd_table_entry 	*fd_table[];

/*
 * New functions
 */

__BEGIN_DECLS

void		pthread_init		__P((void));
int			pthread_create		__P((pthread_t *, const pthread_attr_t *,
							   	  void * (*start_routine)(void *), void *));
void		pthread_exit		__P((void *));
pthread_t	pthread_self		__P((void));
int			pthread_equal		__P((pthread_t, pthread_t));
int			pthread_join		__P((pthread_t, void **));
int			pthread_detach		__P((pthread_t));
		
#if defined(PTHREAD_KERNEL)

void		pthread_yield		__P((void));

/* Not valid, but I can't spell so this will be caught at compile time */
#define		pthread_yeild(notvalid)

#endif

__END_DECLS
