/*
 * Copyright (c) 1998 Daniel Eischen <eischen@vigrid.com>.
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
 *	This product includes software developed by Daniel Eischen.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY DANIEL EISCHEN AND CONTRIBUTORS ``AS IS'' AND
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
 */
#include <stdlib.h>
#include <sys/queue.h>
#include <string.h>
#ifdef _THREAD_SAFE
#include <pthread.h>
#include "pthread_private.h"

/* Prototypes: */
static void pq_insert_prio_list(pq_queue_t *pq, int prio);


int
_pq_init(pq_queue_t *pq, int minprio, int maxprio)
{
	int i, ret = 0;
	int prioslots = maxprio - minprio + 1;

	if (pq == NULL)
		ret = -1;

	/* Create the priority queue with (maxprio - minprio + 1) slots: */
	else if	((pq->pq_lists =
	    (pq_list_t *) malloc(sizeof(pq_list_t) * prioslots)) == NULL)
		ret = -1;

	else {
		/* Initialize the queue for each priority slot: */
		for (i = 0; i < prioslots; i++) {
			TAILQ_INIT(&pq->pq_lists[i].pl_head);
			pq->pq_lists[i].pl_prio = i;
			pq->pq_lists[i].pl_queued = 0;
		}

		/* Initialize the priority queue: */
		TAILQ_INIT(&pq->pq_queue);

		/* Remember the queue size: */
		pq->pq_size = prioslots;
	}
	return (ret);
}

void
_pq_remove(pq_queue_t *pq, pthread_t pthread)
{
	int prio = pthread->active_priority;

	TAILQ_REMOVE(&pq->pq_lists[prio].pl_head, pthread, pqe);
}


void
_pq_insert_head(pq_queue_t *pq, pthread_t pthread)
{
	int prio = pthread->active_priority;

	TAILQ_INSERT_HEAD(&pq->pq_lists[prio].pl_head, pthread, pqe);
	if (pq->pq_lists[prio].pl_queued == 0)
		/* Insert the list into the priority queue: */
		pq_insert_prio_list(pq, prio);
}


void
_pq_insert_tail(pq_queue_t *pq, pthread_t pthread)
{
	int prio = pthread->active_priority;

	TAILQ_INSERT_TAIL(&pq->pq_lists[prio].pl_head, pthread, pqe);
	if (pq->pq_lists[prio].pl_queued == 0)
		/* Insert the list into the priority queue: */
		pq_insert_prio_list(pq, prio);
}


pthread_t
_pq_first(pq_queue_t *pq)
{
	pq_list_t *pql;
	pthread_t pthread = NULL;

	while (((pql = TAILQ_FIRST(&pq->pq_queue)) != NULL) &&
	    (pthread == NULL)) {
		if ((pthread = TAILQ_FIRST(&pql->pl_head)) == NULL) {
			/*
			 * The priority list is empty; remove the list
			 * from the queue.
			 */
			TAILQ_REMOVE(&pq->pq_queue, pql, pl_link);

			/* Mark the list as not being in the queue: */
			pql->pl_queued = 0;
		}
	}
	return (pthread);
}


static void
pq_insert_prio_list(pq_queue_t *pq, int prio)
{
	pq_list_t *pql;

	/*
	 * The priority queue is in descending priority order.  Start at
	 * the beginning of the queue and find the list before which the
	 * new list should to be inserted.
	 */
	pql = TAILQ_FIRST(&pq->pq_queue);
	while ((pql != NULL) && (pql->pl_prio > prio))
		pql = TAILQ_NEXT(pql, pl_link);

	/* Insert the list: */
	if (pql == NULL)
		TAILQ_INSERT_TAIL(&pq->pq_queue, &pq->pq_lists[prio], pl_link);
	else
		TAILQ_INSERT_BEFORE(pql, &pq->pq_lists[prio], pl_link);

	/* Mark this list as being in the queue: */
	pq->pq_lists[prio].pl_queued = 1;
}

#endif
