/*	$NetBSD: rf_fifo.c,v 1.5 2000/03/04 03:27:13 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/***************************************************
 *
 * rf_fifo.c --  prioritized fifo queue code.
 * There are only two priority levels: hi and lo.
 *
 * Aug 4, 1994, adapted from raidSim version (MCH)
 *
 ***************************************************/

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_alloclist.h>
#include <dev/raidframe/rf_stripelocks.h>
#include <dev/raidframe/rf_layout.h>
#include <dev/raidframe/rf_diskqueue.h>
#include <dev/raidframe/rf_fifo.h>
#include <dev/raidframe/rf_debugMem.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_options.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_types.h>

/* just malloc a header, zero it (via calloc), and return it */
/*ARGSUSED*/
void   *
rf_FifoCreate(sectPerDisk, clList, listp)
	RF_SectorCount_t sectPerDisk;
	RF_AllocListElem_t *clList;
	RF_ShutdownList_t **listp;
{
	RF_FifoHeader_t *q;

	RF_CallocAndAdd(q, 1, sizeof(RF_FifoHeader_t), (RF_FifoHeader_t *), clList);
	q->hq_count = q->lq_count = 0;
	return ((void *) q);
}

void 
rf_FifoEnqueue(q_in, elem, priority)
	void   *q_in;
	RF_DiskQueueData_t *elem;
	int     priority;
{
	RF_FifoHeader_t *q = (RF_FifoHeader_t *) q_in;

	RF_ASSERT(priority == RF_IO_NORMAL_PRIORITY || priority == RF_IO_LOW_PRIORITY);

	elem->next = NULL;
	if (priority == RF_IO_NORMAL_PRIORITY) {
		if (!q->hq_tail) {
			RF_ASSERT(q->hq_count == 0 && q->hq_head == NULL);
			q->hq_head = q->hq_tail = elem;
		} else {
			RF_ASSERT(q->hq_count != 0 && q->hq_head != NULL);
			q->hq_tail->next = elem;
			q->hq_tail = elem;
		}
		q->hq_count++;
	} else {
		RF_ASSERT(elem->next == NULL);
		if (rf_fifoDebug) {
			printf("raid%d: fifo: ENQ lopri\n", 
			       elem->raidPtr->raidid);
		}
		if (!q->lq_tail) {
			RF_ASSERT(q->lq_count == 0 && q->lq_head == NULL);
			q->lq_head = q->lq_tail = elem;
		} else {
			RF_ASSERT(q->lq_count != 0 && q->lq_head != NULL);
			q->lq_tail->next = elem;
			q->lq_tail = elem;
		}
		q->lq_count++;
	}
	if ((q->hq_count + q->lq_count) != elem->queue->queueLength) {
		printf("Queue lengths differ!: %d %d %d\n",
		    q->hq_count, q->lq_count, (int) elem->queue->queueLength);
		printf("%d %d %d %d\n",
		    (int) elem->queue->numOutstanding,
		    (int) elem->queue->maxOutstanding,
		    (int) elem->queue->row,
		    (int) elem->queue->col);
	}
	RF_ASSERT((q->hq_count + q->lq_count) == elem->queue->queueLength);
}

RF_DiskQueueData_t *
rf_FifoDequeue(q_in)
	void   *q_in;
{
	RF_FifoHeader_t *q = (RF_FifoHeader_t *) q_in;
	RF_DiskQueueData_t *nd;

	RF_ASSERT(q);
	if (q->hq_head) {
		RF_ASSERT(q->hq_count != 0 && q->hq_tail != NULL);
		nd = q->hq_head;
		q->hq_head = q->hq_head->next;
		if (!q->hq_head)
			q->hq_tail = NULL;
		nd->next = NULL;
		q->hq_count--;
	} else
		if (q->lq_head) {
			RF_ASSERT(q->lq_count != 0 && q->lq_tail != NULL);
			nd = q->lq_head;
			q->lq_head = q->lq_head->next;
			if (!q->lq_head)
				q->lq_tail = NULL;
			nd->next = NULL;
			q->lq_count--;
			if (rf_fifoDebug) {
				printf("raid%d: fifo: DEQ lopri %lx\n", 
				       nd->raidPtr->raidid, (long) nd);
			}
		} else {
			RF_ASSERT(q->hq_count == 0 && q->lq_count == 0 && q->hq_tail == NULL && q->lq_tail == NULL);
			nd = NULL;
		}
	return (nd);
}

/* Return ptr to item at head of queue.  Used to examine request
 * info without actually dequeueing the request.
 */
RF_DiskQueueData_t *
rf_FifoPeek(void *q_in)
{
	RF_DiskQueueData_t *headElement = NULL;
	RF_FifoHeader_t *q = (RF_FifoHeader_t *) q_in;

	RF_ASSERT(q);
	if (q->hq_head)
		headElement = q->hq_head;
	else
		if (q->lq_head)
			headElement = q->lq_head;
	return (headElement);
}
/* We sometimes need to promote a low priority access to a regular priority access.
 * Currently, this is only used when the user wants to write a stripe which is currently
 * under reconstruction.
 * This routine will promote all accesses tagged with the indicated parityStripeID from
 * the low priority queue to the end of the normal priority queue.
 * We assume the queue is locked upon entry.
 */
int 
rf_FifoPromote(q_in, parityStripeID, which_ru)
	void   *q_in;
	RF_StripeNum_t parityStripeID;
	RF_ReconUnitNum_t which_ru;
{
	RF_FifoHeader_t *q = (RF_FifoHeader_t *) q_in;
	RF_DiskQueueData_t *lp = q->lq_head, *pt = NULL;	/* lp = lo-pri queue
								 * pointer, pt = trailer */
	int     retval = 0;

	while (lp) {

		/* search for the indicated parity stripe in the low-pri queue */
		if (lp->parityStripeID == parityStripeID && lp->which_ru == which_ru) {
			/* printf("FifoPromote:  promoting access for psid
			 * %ld\n",parityStripeID); */
			if (pt)
				pt->next = lp->next;	/* delete an entry other
							 * than the first */
			else
				q->lq_head = lp->next;	/* delete the head entry */

			if (!q->lq_head)
				q->lq_tail = NULL;	/* we deleted the only
							 * entry */
			else
				if (lp == q->lq_tail)
					q->lq_tail = pt;	/* we deleted the tail
								 * entry */

			lp->next = NULL;
			q->lq_count--;

			if (q->hq_tail) {
				q->hq_tail->next = lp;
				q->hq_tail = lp;
			}
			 /* append to hi-priority queue */ 
			else {
				q->hq_head = q->hq_tail = lp;
			}
			q->hq_count++;

			/* UpdateShortestSeekFinishTimeForced(lp->requestPtr,
			 * lp->diskState); *//* deal with this later, if ever */

			lp = (pt) ? pt->next : q->lq_head;	/* reset low-pri pointer
								 * and continue */
			retval++;

		} else {
			pt = lp;
			lp = lp->next;
		}
	}

	/* sanity check.  delete this if you ever put more than one entry in
	 * the low-pri queue */
	RF_ASSERT(retval == 0 || retval == 1);
	return (retval);
}
