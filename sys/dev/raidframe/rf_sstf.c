/*	$FreeBSD$ */
/*	$NetBSD: rf_sstf.c,v 1.6 2001/01/27 20:18:55 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jim Zelenka
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

/*******************************************************************************
 *
 * sstf.c --  prioritized shortest seek time first disk queueing code
 *
 ******************************************************************************/

#include <dev/raidframe/rf_alloclist.h>
#include <dev/raidframe/rf_stripelocks.h>
#include <dev/raidframe/rf_layout.h>
#include <dev/raidframe/rf_diskqueue.h>
#include <dev/raidframe/rf_sstf.h>
#include <dev/raidframe/rf_debugMem.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_options.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_types.h>

#define DIR_LEFT   1
#define DIR_RIGHT  2
#define DIR_EITHER 3

#define SNUM_DIFF(_a_,_b_) (((_a_)>(_b_))?((_a_)-(_b_)):((_b_)-(_a_)))

#define QSUM(_sstfq_) (((_sstfq_)->lopri.qlen)+((_sstfq_)->left.qlen)+((_sstfq_)->right.qlen))


static void 
do_sstf_ord_q(RF_DiskQueueData_t **,
    RF_DiskQueueData_t **,
    RF_DiskQueueData_t *);

static RF_DiskQueueData_t *
closest_to_arm(RF_SstfQ_t *,
    RF_SectorNum_t,
    int *,
    int);
static void do_dequeue(RF_SstfQ_t *, RF_DiskQueueData_t *);


static void 
do_sstf_ord_q(queuep, tailp, req)
	RF_DiskQueueData_t **queuep;
	RF_DiskQueueData_t **tailp;
	RF_DiskQueueData_t *req;
{
	RF_DiskQueueData_t *r, *s;

	if (*queuep == NULL) {
		*queuep = req;
		*tailp = req;
		req->next = NULL;
		req->prev = NULL;
		return;
	}
	if (req->sectorOffset <= (*queuep)->sectorOffset) {
		req->next = *queuep;
		req->prev = NULL;
		(*queuep)->prev = req;
		*queuep = req;
		return;
	}
	if (req->sectorOffset > (*tailp)->sectorOffset) {
		/* optimization */
		r = NULL;
		s = *tailp;
		goto q_at_end;
	}
	for (s = NULL, r = *queuep; r; s = r, r = r->next) {
		if (r->sectorOffset >= req->sectorOffset) {
			/* insert after s, before r */
			RF_ASSERT(s);
			req->next = r;
			r->prev = req;
			s->next = req;
			req->prev = s;
			return;
		}
	}
q_at_end:
	/* insert after s, at end of queue */
	RF_ASSERT(r == NULL);
	RF_ASSERT(s);
	RF_ASSERT(s == (*tailp));
	req->next = NULL;
	req->prev = s;
	s->next = req;
	*tailp = req;
}
/* for removing from head-of-queue */
#define DO_HEAD_DEQ(_r_,_q_) { \
	_r_ = (_q_)->queue; \
	RF_ASSERT((_r_) != NULL); \
	(_q_)->queue = (_r_)->next; \
	(_q_)->qlen--; \
	if ((_q_)->qlen == 0) { \
		RF_ASSERT((_r_) == (_q_)->qtail); \
		RF_ASSERT((_q_)->queue == NULL); \
		(_q_)->qtail = NULL; \
	} \
	else { \
		RF_ASSERT((_q_)->queue->prev == (_r_)); \
		(_q_)->queue->prev = NULL; \
	} \
}

/* for removing from end-of-queue */
#define DO_TAIL_DEQ(_r_,_q_) { \
	_r_ = (_q_)->qtail; \
	RF_ASSERT((_r_) != NULL); \
	(_q_)->qtail = (_r_)->prev; \
	(_q_)->qlen--; \
	if ((_q_)->qlen == 0) { \
		RF_ASSERT((_r_) == (_q_)->queue); \
		RF_ASSERT((_q_)->qtail == NULL); \
		(_q_)->queue = NULL; \
	} \
	else { \
		RF_ASSERT((_q_)->qtail->next == (_r_)); \
		(_q_)->qtail->next = NULL; \
	} \
}

#define DO_BEST_DEQ(_l_,_r_,_q_) { \
	if (SNUM_DIFF((_q_)->queue->sectorOffset,_l_) \
		< SNUM_DIFF((_q_)->qtail->sectorOffset,_l_)) \
	{ \
		DO_HEAD_DEQ(_r_,_q_); \
	} \
	else { \
		DO_TAIL_DEQ(_r_,_q_); \
	} \
}

static RF_DiskQueueData_t *
closest_to_arm(queue, arm_pos, dir, allow_reverse)
	RF_SstfQ_t *queue;
	RF_SectorNum_t arm_pos;
	int    *dir;
	int     allow_reverse;
{
	RF_SectorNum_t best_pos_l = 0, this_pos_l = 0, last_pos = 0;
	RF_SectorNum_t best_pos_r = 0, this_pos_r = 0;
	RF_DiskQueueData_t *r, *best_l, *best_r;

	best_r = best_l = NULL;
	for (r = queue->queue; r; r = r->next) {
		if (r->sectorOffset < arm_pos) {
			if (best_l == NULL) {
				best_l = r;
				last_pos = best_pos_l = this_pos_l;
			} else {
				this_pos_l = arm_pos - r->sectorOffset;
				if (this_pos_l < best_pos_l) {
					best_l = r;
					last_pos = best_pos_l = this_pos_l;
				} else {
					last_pos = this_pos_l;
				}
			}
		} else {
			if (best_r == NULL) {
				best_r = r;
				last_pos = best_pos_r = this_pos_r;
			} else {
				this_pos_r = r->sectorOffset - arm_pos;
				if (this_pos_r < best_pos_r) {
					best_r = r;
					last_pos = best_pos_r = this_pos_r;
				} else {
					last_pos = this_pos_r;
				}
				if (this_pos_r > last_pos) {
					/* getting farther away */
					break;
				}
			}
		}
	}
	if ((best_r == NULL) && (best_l == NULL))
		return (NULL);
	if ((*dir == DIR_RIGHT) && best_r)
		return (best_r);
	if ((*dir == DIR_LEFT) && best_l)
		return (best_l);
	if (*dir == DIR_EITHER) {
		if (best_l == NULL)
			return (best_r);
		if (best_r == NULL)
			return (best_l);
		if (best_pos_r < best_pos_l)
			return (best_r);
		else
			return (best_l);
	}
	/*
	 * Nothing in the direction we want to go. Reverse or
	 * reset the arm. We know we have an I/O in the other
	 * direction.
	 */
	if (allow_reverse) {
		if (*dir == DIR_RIGHT) {
			*dir = DIR_LEFT;
			return (best_l);
		} else {
			*dir = DIR_RIGHT;
			return (best_r);
		}
	}
	/*
	 * Reset (beginning of queue).
	 */
	RF_ASSERT(*dir == DIR_RIGHT);
	return (queue->queue);
}

void   *
rf_SstfCreate(sect_per_disk, cl_list, listp)
	RF_SectorCount_t sect_per_disk;
	RF_AllocListElem_t *cl_list;
	RF_ShutdownList_t **listp;
{
	RF_Sstf_t *sstfq;

	RF_CallocAndAdd(sstfq, 1, sizeof(RF_Sstf_t), (RF_Sstf_t *), cl_list);
	sstfq->dir = DIR_EITHER;
	sstfq->allow_reverse = 1;
	return ((void *) sstfq);
}

void   *
rf_ScanCreate(sect_per_disk, cl_list, listp)
	RF_SectorCount_t sect_per_disk;
	RF_AllocListElem_t *cl_list;
	RF_ShutdownList_t **listp;
{
	RF_Sstf_t *scanq;

	RF_CallocAndAdd(scanq, 1, sizeof(RF_Sstf_t), (RF_Sstf_t *), cl_list);
	scanq->dir = DIR_RIGHT;
	scanq->allow_reverse = 1;
	return ((void *) scanq);
}

void   *
rf_CscanCreate(sect_per_disk, cl_list, listp)
	RF_SectorCount_t sect_per_disk;
	RF_AllocListElem_t *cl_list;
	RF_ShutdownList_t **listp;
{
	RF_Sstf_t *cscanq;

	RF_CallocAndAdd(cscanq, 1, sizeof(RF_Sstf_t), (RF_Sstf_t *), cl_list);
	cscanq->dir = DIR_RIGHT;
	return ((void *) cscanq);
}

void 
rf_SstfEnqueue(qptr, req, priority)
	void   *qptr;
	RF_DiskQueueData_t *req;
	int     priority;
{
	RF_Sstf_t *sstfq;

	sstfq = (RF_Sstf_t *) qptr;

	if (priority == RF_IO_LOW_PRIORITY) {
		if (rf_sstfDebug || rf_scanDebug || rf_cscanDebug) {
			RF_DiskQueue_t *dq;
			dq = (RF_DiskQueue_t *) req->queue;
			printf("raid%d: ENQ lopri %d,%d queues are %d,%d,%d\n",
			       req->raidPtr->raidid,
			       dq->row, dq->col, 
			       sstfq->left.qlen, sstfq->right.qlen,
			       sstfq->lopri.qlen);
		}
		do_sstf_ord_q(&sstfq->lopri.queue, &sstfq->lopri.qtail, req);
		sstfq->lopri.qlen++;
	} else {
		if (req->sectorOffset < sstfq->last_sector) {
			do_sstf_ord_q(&sstfq->left.queue, &sstfq->left.qtail, req);
			sstfq->left.qlen++;
		} else {
			do_sstf_ord_q(&sstfq->right.queue, &sstfq->right.qtail, req);
			sstfq->right.qlen++;
		}
	}
}

static void 
do_dequeue(queue, req)
	RF_SstfQ_t *queue;
	RF_DiskQueueData_t *req;
{
	RF_DiskQueueData_t *req2;

	if (rf_sstfDebug || rf_scanDebug || rf_cscanDebug) {
		printf("raid%d: do_dequeue\n", req->raidPtr->raidid);
	}
	if (req == queue->queue) {
		DO_HEAD_DEQ(req2, queue);
		RF_ASSERT(req2 == req);
	} else
		if (req == queue->qtail) {
			DO_TAIL_DEQ(req2, queue);
			RF_ASSERT(req2 == req);
		} else {
			/* dequeue from middle of list */
			RF_ASSERT(req->next);
			RF_ASSERT(req->prev);
			queue->qlen--;
			req->next->prev = req->prev;
			req->prev->next = req->next;
			req->next = req->prev = NULL;
		}
}

RF_DiskQueueData_t *
rf_SstfDequeue(qptr)
	void   *qptr;
{
	RF_DiskQueueData_t *req = NULL;
	RF_Sstf_t *sstfq;

	sstfq = (RF_Sstf_t *) qptr;

	if (rf_sstfDebug) {
		RF_DiskQueue_t *dq;
		dq = (RF_DiskQueue_t *) req->queue;
		RF_ASSERT(QSUM(sstfq) == dq->queueLength);
		printf("raid%d: sstf: Dequeue %d,%d queues are %d,%d,%d\n",
		       req->raidPtr->raidid, dq->row, dq->col, 
		       sstfq->left.qlen, sstfq->right.qlen, sstfq->lopri.qlen);
	}
	if (sstfq->left.queue == NULL) {
		RF_ASSERT(sstfq->left.qlen == 0);
		if (sstfq->right.queue == NULL) {
			RF_ASSERT(sstfq->right.qlen == 0);
			if (sstfq->lopri.queue == NULL) {
				RF_ASSERT(sstfq->lopri.qlen == 0);
				return (NULL);
			}
			if (rf_sstfDebug) {
				printf("raid%d: sstf: check for close lopri",
				       req->raidPtr->raidid);
			}
			req = closest_to_arm(&sstfq->lopri, sstfq->last_sector,
			    &sstfq->dir, sstfq->allow_reverse);
			if (rf_sstfDebug) {
				printf("raid%d: sstf: closest_to_arm said %lx",
				       req->raidPtr->raidid, (long) req);
			}
			if (req == NULL)
				return (NULL);
			do_dequeue(&sstfq->lopri, req);
		} else {
			DO_BEST_DEQ(sstfq->last_sector, req, &sstfq->right);
		}
	} else {
		if (sstfq->right.queue == NULL) {
			RF_ASSERT(sstfq->right.qlen == 0);
			DO_BEST_DEQ(sstfq->last_sector, req, &sstfq->left);
		} else {
			if (SNUM_DIFF(sstfq->last_sector, sstfq->right.queue->sectorOffset)
			    < SNUM_DIFF(sstfq->last_sector, sstfq->left.qtail->sectorOffset)) {
				DO_HEAD_DEQ(req, &sstfq->right);
			} else {
				DO_TAIL_DEQ(req, &sstfq->left);
			}
		}
	}
	RF_ASSERT(req);
	sstfq->last_sector = req->sectorOffset;
	return (req);
}

RF_DiskQueueData_t *
rf_ScanDequeue(qptr)
	void   *qptr;
{
	RF_DiskQueueData_t *req = NULL;
	RF_Sstf_t *scanq;

	scanq = (RF_Sstf_t *) qptr;

	if (rf_scanDebug) {
		RF_DiskQueue_t *dq;
		dq = (RF_DiskQueue_t *) req->queue;
		RF_ASSERT(QSUM(scanq) == dq->queueLength);
		printf("raid%d: scan: Dequeue %d,%d queues are %d,%d,%d\n", 
		       req->raidPtr->raidid, dq->row, dq->col, 
		       scanq->left.qlen, scanq->right.qlen, scanq->lopri.qlen);
	}
	if (scanq->left.queue == NULL) {
		RF_ASSERT(scanq->left.qlen == 0);
		if (scanq->right.queue == NULL) {
			RF_ASSERT(scanq->right.qlen == 0);
			if (scanq->lopri.queue == NULL) {
				RF_ASSERT(scanq->lopri.qlen == 0);
				return (NULL);
			}
			req = closest_to_arm(&scanq->lopri, scanq->last_sector,
			    &scanq->dir, scanq->allow_reverse);
			if (req == NULL)
				return (NULL);
			do_dequeue(&scanq->lopri, req);
		} else {
			scanq->dir = DIR_RIGHT;
			DO_HEAD_DEQ(req, &scanq->right);
		}
	} else
		if (scanq->right.queue == NULL) {
			RF_ASSERT(scanq->right.qlen == 0);
			RF_ASSERT(scanq->left.queue);
			scanq->dir = DIR_LEFT;
			DO_TAIL_DEQ(req, &scanq->left);
		} else {
			RF_ASSERT(scanq->right.queue);
			RF_ASSERT(scanq->left.queue);
			if (scanq->dir == DIR_RIGHT) {
				DO_HEAD_DEQ(req, &scanq->right);
			} else {
				DO_TAIL_DEQ(req, &scanq->left);
			}
		}
	RF_ASSERT(req);
	scanq->last_sector = req->sectorOffset;
	return (req);
}

RF_DiskQueueData_t *
rf_CscanDequeue(qptr)
	void   *qptr;
{
	RF_DiskQueueData_t *req = NULL;
	RF_Sstf_t *cscanq;

	cscanq = (RF_Sstf_t *) qptr;

	RF_ASSERT(cscanq->dir == DIR_RIGHT);
	if (rf_cscanDebug) {
		RF_DiskQueue_t *dq;
		dq = (RF_DiskQueue_t *) req->queue;
		RF_ASSERT(QSUM(cscanq) == dq->queueLength);
		printf("raid%d: scan: Dequeue %d,%d queues are %d,%d,%d\n", 
		       req->raidPtr->raidid, dq->row, dq->col,
		       cscanq->left.qlen, cscanq->right.qlen,
		       cscanq->lopri.qlen);
	}
	if (cscanq->right.queue) {
		DO_HEAD_DEQ(req, &cscanq->right);
	} else {
		RF_ASSERT(cscanq->right.qlen == 0);
		if (cscanq->left.queue == NULL) {
			RF_ASSERT(cscanq->left.qlen == 0);
			if (cscanq->lopri.queue == NULL) {
				RF_ASSERT(cscanq->lopri.qlen == 0);
				return (NULL);
			}
			req = closest_to_arm(&cscanq->lopri, cscanq->last_sector,
			    &cscanq->dir, cscanq->allow_reverse);
			if (req == NULL)
				return (NULL);
			do_dequeue(&cscanq->lopri, req);
		} else {
			/*
			 * There's I/Os to the left of the arm. Swing
			 * on back (swap queues).
			 */
			cscanq->right = cscanq->left;
			cscanq->left.qlen = 0;
			cscanq->left.queue = cscanq->left.qtail = NULL;
			DO_HEAD_DEQ(req, &cscanq->right);
		}
	}
	RF_ASSERT(req);
	cscanq->last_sector = req->sectorOffset;
	return (req);
}

RF_DiskQueueData_t *
rf_SstfPeek(qptr)
	void   *qptr;
{
	RF_DiskQueueData_t *req;
	RF_Sstf_t *sstfq;

	sstfq = (RF_Sstf_t *) qptr;

	if ((sstfq->left.queue == NULL) && (sstfq->right.queue == NULL)) {
		req = closest_to_arm(&sstfq->lopri, sstfq->last_sector, &sstfq->dir,
		    sstfq->allow_reverse);
	} else {
		if (sstfq->left.queue == NULL)
			req = sstfq->right.queue;
		else {
			if (sstfq->right.queue == NULL)
				req = sstfq->left.queue;
			else {
				if (SNUM_DIFF(sstfq->last_sector, sstfq->right.queue->sectorOffset)
				    < SNUM_DIFF(sstfq->last_sector, sstfq->left.qtail->sectorOffset)) {
					req = sstfq->right.queue;
				} else {
					req = sstfq->left.qtail;
				}
			}
		}
	}
	if (req == NULL) {
		RF_ASSERT(QSUM(sstfq) == 0);
	}
	return (req);
}

RF_DiskQueueData_t *
rf_ScanPeek(qptr)
	void   *qptr;
{
	RF_DiskQueueData_t *req;
	RF_Sstf_t *scanq;
	int     dir;

	scanq = (RF_Sstf_t *) qptr;
	dir = scanq->dir;

	if (scanq->left.queue == NULL) {
		RF_ASSERT(scanq->left.qlen == 0);
		if (scanq->right.queue == NULL) {
			RF_ASSERT(scanq->right.qlen == 0);
			if (scanq->lopri.queue == NULL) {
				RF_ASSERT(scanq->lopri.qlen == 0);
				return (NULL);
			}
			req = closest_to_arm(&scanq->lopri, scanq->last_sector,
			    &dir, scanq->allow_reverse);
		} else {
			req = scanq->right.queue;
		}
	} else
		if (scanq->right.queue == NULL) {
			RF_ASSERT(scanq->right.qlen == 0);
			RF_ASSERT(scanq->left.queue);
			req = scanq->left.qtail;
		} else {
			RF_ASSERT(scanq->right.queue);
			RF_ASSERT(scanq->left.queue);
			if (scanq->dir == DIR_RIGHT) {
				req = scanq->right.queue;
			} else {
				req = scanq->left.qtail;
			}
		}
	if (req == NULL) {
		RF_ASSERT(QSUM(scanq) == 0);
	}
	return (req);
}

RF_DiskQueueData_t *
rf_CscanPeek(qptr)
	void   *qptr;
{
	RF_DiskQueueData_t *req;
	RF_Sstf_t *cscanq;

	cscanq = (RF_Sstf_t *) qptr;

	RF_ASSERT(cscanq->dir == DIR_RIGHT);
	if (cscanq->right.queue) {
		req = cscanq->right.queue;
	} else {
		RF_ASSERT(cscanq->right.qlen == 0);
		if (cscanq->left.queue == NULL) {
			RF_ASSERT(cscanq->left.qlen == 0);
			if (cscanq->lopri.queue == NULL) {
				RF_ASSERT(cscanq->lopri.qlen == 0);
				return (NULL);
			}
			req = closest_to_arm(&cscanq->lopri, cscanq->last_sector,
			    &cscanq->dir, cscanq->allow_reverse);
		} else {
			/*
			 * There's I/Os to the left of the arm. We'll end
			 * up swinging on back.
			 */
			req = cscanq->left.queue;
		}
	}
	if (req == NULL) {
		RF_ASSERT(QSUM(cscanq) == 0);
	}
	return (req);
}

int 
rf_SstfPromote(qptr, parityStripeID, which_ru)
	void   *qptr;
	RF_StripeNum_t parityStripeID;
	RF_ReconUnitNum_t which_ru;
{
	RF_DiskQueueData_t *r, *next;
	RF_Sstf_t *sstfq;
	int     n;

	sstfq = (RF_Sstf_t *) qptr;

	n = 0;
	for (r = sstfq->lopri.queue; r; r = next) {
		next = r->next;
		if (rf_sstfDebug || rf_scanDebug || rf_cscanDebug) {
			printf("raid%d: check promote %lx\n",
			       r->raidPtr->raidid, (long) r);
		}
		if ((r->parityStripeID == parityStripeID)
		    && (r->which_ru == which_ru)) {
			do_dequeue(&sstfq->lopri, r);
			rf_SstfEnqueue(qptr, r, RF_IO_NORMAL_PRIORITY);
			n++;
		}
	}
	if (rf_sstfDebug || rf_scanDebug || rf_cscanDebug) {
		printf("raid%d: promoted %d matching I/Os queues are %d,%d,%d\n",
		       r->raidPtr->raidid, n, sstfq->left.qlen, 
		       sstfq->right.qlen, sstfq->lopri.qlen);
	}
	return (n);
}
