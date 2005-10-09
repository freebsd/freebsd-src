/*
 * Copyright (c) 2005 David Xu <davidxu@freebsd.org>
 * Copyright (C) 2003 Daniel M. Eischen <deischen@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/queue.h>

#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "thr_private.h"
#include "libc_private.h"

/*#define DEBUG_THREAD_LIST */
#ifdef DEBUG_THREAD_LIST
#define DBG_MSG		stdout_debug
#else
#define DBG_MSG(x...)
#endif

/*
 * Define a high water mark for the maximum number of threads that
 * will be cached.  Once this level is reached, any extra threads
 * will be free()'d.
 */
#define	MAX_CACHED_THREADS	100

/*
 * We've got to keep track of everything that is allocated, not only
 * to have a speedy free list, but also so they can be deallocated
 * after a fork().
 */
static TAILQ_HEAD(, pthread)	free_threadq;
static umtx_t			free_thread_lock;
static umtx_t			tcb_lock;
static int			free_thread_count = 0;
static int			inited = 0;

LIST_HEAD(thread_hash_head, pthread);
#define HASH_QUEUES	128
static struct thread_hash_head	thr_hashtable[HASH_QUEUES];
#define	THREAD_HASH(thrd)	(((unsigned long)thrd >> 8) % HASH_QUEUES)

static void thr_destroy(struct pthread *curthread, struct pthread *thread);

void
_thr_list_init(void)
{
	int i;

	_gc_count = 0;
	_thr_umtx_init(&_thr_list_lock);
	TAILQ_INIT(&_thread_list);
	TAILQ_INIT(&free_threadq);
	_thr_umtx_init(&free_thread_lock);
	_thr_umtx_init(&tcb_lock);
	if (inited) {
		for (i = 0; i < HASH_QUEUES; ++i)
			LIST_INIT(&thr_hashtable[i]);
	}
	inited = 1;
}

void
_thr_gc(struct pthread *curthread)
{
	struct pthread *td, *td_next;
	TAILQ_HEAD(, pthread) worklist;

	TAILQ_INIT(&worklist);
	THREAD_LIST_LOCK(curthread);

	/* Check the threads waiting for GC. */
	for (td = TAILQ_FIRST(&_thread_gc_list); td != NULL; td = td_next) {
		td_next = TAILQ_NEXT(td, gcle);
		if (td->tid != TID_TERMINATED) {
			/* make sure we are not still in userland */
			continue;
		}
		_thr_stack_free(&td->attr);
		if (((td->tlflags & TLFLAGS_DETACHED) != 0) &&
		    (td->refcount == 0)) {
			THR_GCLIST_REMOVE(td);
			/*
			 * The thread has detached and is no longer
			 * referenced.  It is safe to remove all
			 * remnants of the thread.
			 */
			THR_LIST_REMOVE(td);
			TAILQ_INSERT_HEAD(&worklist, td, gcle);
		}
	}
	THREAD_LIST_UNLOCK(curthread);

	while ((td = TAILQ_FIRST(&worklist)) != NULL) {
		TAILQ_REMOVE(&worklist, td, gcle);
		/*
		 * XXX we don't free initial thread, because there might
		 * have some code referencing initial thread.
		 */
		if (td == _thr_initial) {
			DBG_MSG("Initial thread won't be freed\n");
			continue;
		}

		DBG_MSG("Freeing thread %p\n", td);
		_thr_free(curthread, td);
	}
}

struct pthread *
_thr_alloc(struct pthread *curthread)
{
	struct pthread	*thread = NULL;
	struct tcb	*tcb;

	if (curthread != NULL) {
		if (GC_NEEDED())
			_thr_gc(curthread);
		if (free_thread_count > 0) {
			THR_LOCK_ACQUIRE(curthread, &free_thread_lock);
			if ((thread = TAILQ_FIRST(&free_threadq)) != NULL) {
				TAILQ_REMOVE(&free_threadq, thread, tle);
				free_thread_count--;
			}
			THR_LOCK_RELEASE(curthread, &free_thread_lock);
		}
	}
	if (thread == NULL) {
		thread = malloc(sizeof(struct pthread));
		if (thread == NULL)
			return (NULL);
	}
	if (curthread != NULL) {
		THR_LOCK_ACQUIRE(curthread, &tcb_lock);
		tcb = _tcb_ctor(thread, 0 /* not initial tls */);
		THR_LOCK_RELEASE(curthread, &tcb_lock);
	} else {
		tcb = _tcb_ctor(thread, 1 /* initial tls */);
	}
	if (tcb != NULL) {
		memset(thread, 0, sizeof(*thread));
		thread->tcb = tcb;
	} else {
		thr_destroy(curthread, thread);
		thread = NULL;
	}
	return (thread);
}

void
_thr_free(struct pthread *curthread, struct pthread *thread)
{
	DBG_MSG("Freeing thread %p\n", thread);
	if (thread->name) {
		free(thread->name);
		thread->name = NULL;
	}
	/*
	 * Always free tcb, as we only know it is part of RTLD TLS
	 * block, but don't know its detail and can not assume how
	 * it works, so better to avoid caching it here.
	 */
	if (curthread != NULL) {
		THR_LOCK_ACQUIRE(curthread, &tcb_lock);
		_tcb_dtor(thread->tcb);
		THR_LOCK_RELEASE(curthread, &tcb_lock);
	} else {
		_tcb_dtor(thread->tcb);
	}
	thread->tcb = NULL;
	if ((curthread == NULL) || (free_thread_count >= MAX_CACHED_THREADS)) {
		thr_destroy(curthread, thread);
	} else {
		/*
		 * Add the thread to the free thread list, this also avoids
		 * pthread id is reused too quickly, may help some buggy apps.
		 */
		THR_LOCK_ACQUIRE(curthread, &free_thread_lock);
		TAILQ_INSERT_TAIL(&free_threadq, thread, tle);
		free_thread_count++;
		THR_LOCK_RELEASE(curthread, &free_thread_lock);
	}
}

static void
thr_destroy(struct pthread *curthread __unused, struct pthread *thread)
{
	free(thread);
}

/*
 * Add the thread to the list of all threads and increment
 * number of active threads.
 */
void
_thr_link(struct pthread *curthread, struct pthread *thread)
{
	THREAD_LIST_LOCK(curthread);
	THR_LIST_ADD(thread);
	if (thread->attr.flags & PTHREAD_DETACHED)
		thread->tlflags |= TLFLAGS_DETACHED;
	_thread_active_threads++;
	THREAD_LIST_UNLOCK(curthread);
}

/*
 * Remove an active thread.
 */
void
_thr_unlink(struct pthread *curthread, struct pthread *thread)
{
	THREAD_LIST_LOCK(curthread);
	THR_LIST_REMOVE(thread);
	_thread_active_threads--;
	THREAD_LIST_UNLOCK(curthread);
}

void
_thr_hash_add(struct pthread *thread)
{
	struct thread_hash_head *head;

	head = &thr_hashtable[THREAD_HASH(thread)];
	LIST_INSERT_HEAD(head, thread, hle);
}

void
_thr_hash_remove(struct pthread *thread)
{
	LIST_REMOVE(thread, hle);
}

struct pthread *
_thr_hash_find(struct pthread *thread)
{
	struct pthread *td;
	struct thread_hash_head *head;

	head = &thr_hashtable[THREAD_HASH(thread)];
	LIST_FOREACH(td, head, hle) {
		if (td == thread)
			return (thread);
	}
	return (NULL);
}

/*
 * Find a thread in the linked list of active threads and add a reference
 * to it.  Threads with positive reference counts will not be deallocated
 * until all references are released.
 */
int
_thr_ref_add(struct pthread *curthread, struct pthread *thread,
    int include_dead)
{
	int ret;

	if (thread == NULL)
		/* Invalid thread: */
		return (EINVAL);

	THREAD_LIST_LOCK(curthread);
	if ((ret = _thr_find_thread(curthread, thread, include_dead)) == 0) {
		thread->refcount++;
	}
	THREAD_LIST_UNLOCK(curthread);

	/* Return zero if the thread exists: */
	return (ret);
}

void
_thr_ref_delete(struct pthread *curthread, struct pthread *thread)
{
	if (thread != NULL) {
		THREAD_LIST_LOCK(curthread);
		thread->refcount--;
		if ((thread->refcount == 0) &&
		    (thread->tlflags & TLFLAGS_GC_SAFE) != 0)
			THR_GCLIST_ADD(thread);
		THREAD_LIST_UNLOCK(curthread);
	}
}

int
_thr_find_thread(struct pthread *curthread, struct pthread *thread,
    int include_dead)
{
	struct pthread *pthread;

	if (thread == NULL)
		/* Invalid thread: */
		return (EINVAL);

	pthread = _thr_hash_find(thread);
	if (pthread) {
		if (include_dead == 0 && pthread->state == PS_DEAD) {
			pthread = NULL;
		}	
	}

	/* Return zero if the thread exists: */
	return ((pthread != NULL) ? 0 : ESRCH);
}
