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
#include <stdio.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

void
_thread_queue_init(struct pthread_queue * queue)
{
	/* Initialise the pointers in the queue structure: */
	queue->q_next = NULL;
	queue->q_last = NULL;
	queue->q_data = NULL;
	return;
}

void
_thread_queue_enq(struct pthread_queue * queue, struct pthread * thread)
{
	if (queue->q_last) {
		queue->q_last->qnxt = thread;
	} else {
		queue->q_next = thread;
	}
	queue->q_last = thread;
	thread->queue = queue;
	thread->qnxt = NULL;
	return;
}

struct pthread *
_thread_queue_get(struct pthread_queue * queue)
{
	/* Return the pointer to the next thread in the queue: */
	return (queue->q_next);
}

struct pthread *
_thread_queue_deq(struct pthread_queue * queue)
{
	struct pthread *thread = NULL;

	if (queue->q_next) {
		thread = queue->q_next;
		if (!(queue->q_next = queue->q_next->qnxt)) {
			queue->q_last = NULL;
		}
		thread->queue = NULL;
		thread->qnxt = NULL;
	}
	return (thread);
}

int
_thread_queue_remove(struct pthread_queue * queue, struct pthread * thread)
{
	struct pthread **current = &(queue->q_next);
	struct pthread *prev = NULL;
	int             ret = -1;

	while (*current) {
		if (*current == thread) {
			if ((*current)->qnxt) {
				*current = (*current)->qnxt;
			} else {
				queue->q_last = prev;
				*current = NULL;
			}
			ret = 0;
			break;
		}
		prev = *current;
		current = &((*current)->qnxt);
	}
	thread->queue = NULL;
	thread->qnxt = NULL;
	return (ret);
}

int
pthread_llist_remove(struct pthread ** llist, struct pthread * thread)
{
	while (*llist) {
		if (*llist == thread) {
			*llist = thread->qnxt;
			return (0);
		}
		llist = &(*llist)->qnxt;
	}
	return (-1);
}

#endif
