/*	$NetBSD: rf_reconbuffer.c,v 1.5 2001/01/27 20:10:49 oster Exp $	*/

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
 * rf_reconbuffer.c -- reconstruction buffer manager
 *
 ***************************************************/

#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_reconbuffer.h>
#include <dev/raidframe/rf_acctrace.h>
#include <dev/raidframe/rf_etimer.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_debugprint.h>
#include <dev/raidframe/rf_revent.h>
#include <dev/raidframe/rf_reconutil.h>
#include <dev/raidframe/rf_nwayxor.h>

#define Dprintf1(s,a) if (rf_reconbufferDebug) printf(s,a)
#define Dprintf2(s,a,b) if (rf_reconbufferDebug) printf(s,a,b)
#define Dprintf3(s,a,b,c) if (rf_reconbufferDebug) printf(s,a,b,c)
#define Dprintf4(s,a,b,c,d) if (rf_reconbufferDebug) printf(s,a,b,c,d)
#define Dprintf5(s,a,b,c,d,e) if (rf_reconbufferDebug) printf(s,a,b,c,d,e)

/*****************************************************************************
 *
 * Submit a reconstruction buffer to the manager for XOR.  We can only
 * submit a buffer if (1) we can xor into an existing buffer, which
 * means we don't have to acquire a new one, (2) we can acquire a
 * floating recon buffer, or (3) the caller has indicated that we are
 * allowed to keep the submitted buffer.
 *
 * Returns non-zero if and only if we were not able to submit.
 * In this case, we append the current disk ID to the wait list on the
 * indicated RU, so that it will be re-enabled when we acquire a buffer 
 * for this RU.
 *
 ****************************************************************************/

/*
 * nWayXorFuncs[i] is a pointer to a function that will xor "i"
 * bufs into the accumulating sum.
 */
static RF_VoidFuncPtr nWayXorFuncs[] = {
	NULL,
	(RF_VoidFuncPtr) rf_nWayXor1,
	(RF_VoidFuncPtr) rf_nWayXor2,
	(RF_VoidFuncPtr) rf_nWayXor3,
	(RF_VoidFuncPtr) rf_nWayXor4,
	(RF_VoidFuncPtr) rf_nWayXor5,
	(RF_VoidFuncPtr) rf_nWayXor6,
	(RF_VoidFuncPtr) rf_nWayXor7,
	(RF_VoidFuncPtr) rf_nWayXor8,
	(RF_VoidFuncPtr) rf_nWayXor9
};

int 
rf_SubmitReconBuffer(rbuf, keep_it, use_committed)
	RF_ReconBuffer_t *rbuf;	/* the recon buffer to submit */
	int     keep_it;	/* whether we can keep this buffer or we have
				 * to return it */
	int     use_committed;	/* whether to use a committed or an available
				 * recon buffer */
{
	RF_LayoutSW_t *lp;
	int     rc;

	lp = rbuf->raidPtr->Layout.map;
	rc = lp->SubmitReconBuffer(rbuf, keep_it, use_committed);
	return (rc);
}

int 
rf_SubmitReconBufferBasic(rbuf, keep_it, use_committed)
	RF_ReconBuffer_t *rbuf;	/* the recon buffer to submit */
	int     keep_it;	/* whether we can keep this buffer or we have
				 * to return it */
	int     use_committed;	/* whether to use a committed or an available
				 * recon buffer */
{
	RF_Raid_t *raidPtr = rbuf->raidPtr;
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_ReconCtrl_t *reconCtrlPtr = raidPtr->reconControl[rbuf->row];
	RF_ReconParityStripeStatus_t *pssPtr;
	RF_ReconBuffer_t *targetRbuf, *t = NULL;	/* temporary rbuf
							 * pointers */
	caddr_t ta;		/* temporary data buffer pointer */
	RF_CallbackDesc_t *cb, *p;
	int     retcode = 0, created = 0;

	RF_Etimer_t timer;

	/* makes no sense to have a submission from the failed disk */
	RF_ASSERT(rbuf);
	RF_ASSERT(rbuf->col != reconCtrlPtr->fcol);

	Dprintf5("RECON: submission by row %d col %d for psid %ld ru %d (failed offset %ld)\n",
	    rbuf->row, rbuf->col, (long) rbuf->parityStripeID, rbuf->which_ru, (long) rbuf->failedDiskSectorOffset);

	RF_LOCK_PSS_MUTEX(raidPtr, rbuf->row, rbuf->parityStripeID);

	RF_LOCK_MUTEX(reconCtrlPtr->rb_mutex);

	pssPtr = rf_LookupRUStatus(raidPtr, reconCtrlPtr->pssTable, rbuf->parityStripeID, rbuf->which_ru, RF_PSS_NONE, &created);
	RF_ASSERT(pssPtr);	/* if it didn't exist, we wouldn't have gotten
				 * an rbuf for it */

	/* check to see if enough buffers have accumulated to do an XOR.  If
	 * so, there's no need to acquire a floating rbuf.  Before we can do
	 * any XORing, we must have acquired a destination buffer.  If we
	 * have, then we can go ahead and do the XOR if (1) including this
	 * buffer, enough bufs have accumulated, or (2) this is the last
	 * submission for this stripe. Otherwise, we have to go acquire a
	 * floating rbuf. */

	targetRbuf = (RF_ReconBuffer_t *) pssPtr->rbuf;
	if ((targetRbuf != NULL) &&
	    ((pssPtr->xorBufCount == rf_numBufsToAccumulate - 1) || (targetRbuf->count + pssPtr->xorBufCount + 1 == layoutPtr->numDataCol))) {
		pssPtr->rbufsForXor[pssPtr->xorBufCount++] = rbuf;	/* install this buffer */
		Dprintf3("RECON: row %d col %d invoking a %d-way XOR\n", rbuf->row, rbuf->col, pssPtr->xorBufCount);
		RF_ETIMER_START(timer);
		rf_MultiWayReconXor(raidPtr, pssPtr);
		RF_ETIMER_STOP(timer);
		RF_ETIMER_EVAL(timer);
		raidPtr->accumXorTimeUs += RF_ETIMER_VAL_US(timer);
		if (!keep_it) {
			raidPtr->recon_tracerecs[rbuf->col].xor_us = RF_ETIMER_VAL_US(timer);
			RF_ETIMER_STOP(raidPtr->recon_tracerecs[rbuf->col].recon_timer);
			RF_ETIMER_EVAL(raidPtr->recon_tracerecs[rbuf->col].recon_timer);
			raidPtr->recon_tracerecs[rbuf->col].specific.recon.recon_return_to_submit_us +=
			    RF_ETIMER_VAL_US(raidPtr->recon_tracerecs[rbuf->col].recon_timer);
			RF_ETIMER_START(raidPtr->recon_tracerecs[rbuf->col].recon_timer);

			rf_LogTraceRec(raidPtr, &raidPtr->recon_tracerecs[rbuf->col]);
		}
		rf_CheckForFullRbuf(raidPtr, reconCtrlPtr, pssPtr, layoutPtr->numDataCol);

		/* if use_committed is on, we _must_ consume a buffer off the
		 * committed list. */
		if (use_committed) {
			t = reconCtrlPtr->committedRbufs;
			RF_ASSERT(t);
			reconCtrlPtr->committedRbufs = t->next;
			rf_ReleaseFloatingReconBuffer(raidPtr, rbuf->row, t);
		}
		if (keep_it) {
			RF_UNLOCK_PSS_MUTEX(raidPtr, rbuf->row, rbuf->parityStripeID);
			RF_UNLOCK_MUTEX(reconCtrlPtr->rb_mutex);
			rf_FreeReconBuffer(rbuf);
			return (retcode);
		}
		goto out;
	}
	/* set the value of "t", which we'll use as the rbuf from here on */
	if (keep_it) {
		t = rbuf;
	} else {
		if (use_committed) {	/* if a buffer has been committed to
					 * us, use it */
			t = reconCtrlPtr->committedRbufs;
			RF_ASSERT(t);
			reconCtrlPtr->committedRbufs = t->next;
			t->next = NULL;
		} else
			if (reconCtrlPtr->floatingRbufs) {
				t = reconCtrlPtr->floatingRbufs;
				reconCtrlPtr->floatingRbufs = t->next;
				t->next = NULL;
			}
	}

	/* If we weren't able to acquire a buffer, append to the end of the
	 * buf list in the recon ctrl struct. */
	if (!t) {
		RF_ASSERT(!keep_it && !use_committed);
		Dprintf2("RECON: row %d col %d failed to acquire floating rbuf\n", rbuf->row, rbuf->col);

		raidPtr->procsInBufWait++;
		if ((raidPtr->procsInBufWait == raidPtr->numCol - 1) && (raidPtr->numFullReconBuffers == 0)) {
			printf("Buffer wait deadlock detected.  Exiting.\n");
			rf_PrintPSStatusTable(raidPtr, rbuf->row);
			RF_PANIC();
		}
		pssPtr->flags |= RF_PSS_BUFFERWAIT;
		cb = rf_AllocCallbackDesc();	/* append to buf wait list in
						 * recon ctrl structure */
		cb->row = rbuf->row;
		cb->col = rbuf->col;
		cb->callbackArg.v = rbuf->parityStripeID;
		cb->callbackArg2.v = rbuf->which_ru;
		cb->next = NULL;
		if (!reconCtrlPtr->bufferWaitList)
			reconCtrlPtr->bufferWaitList = cb;
		else {		/* might want to maintain head/tail pointers
				 * here rather than search for end of list */
			for (p = reconCtrlPtr->bufferWaitList; p->next; p = p->next);
			p->next = cb;
		}
		retcode = 1;
		goto out;
	}
	Dprintf2("RECON: row %d col %d acquired rbuf\n", rbuf->row, rbuf->col);
	RF_ETIMER_STOP(raidPtr->recon_tracerecs[rbuf->col].recon_timer);
	RF_ETIMER_EVAL(raidPtr->recon_tracerecs[rbuf->col].recon_timer);
	raidPtr->recon_tracerecs[rbuf->col].specific.recon.recon_return_to_submit_us +=
	    RF_ETIMER_VAL_US(raidPtr->recon_tracerecs[rbuf->col].recon_timer);
	RF_ETIMER_START(raidPtr->recon_tracerecs[rbuf->col].recon_timer);

	rf_LogTraceRec(raidPtr, &raidPtr->recon_tracerecs[rbuf->col]);

	/* initialize the buffer */
	if (t != rbuf) {
		t->row = rbuf->row;
		t->col = reconCtrlPtr->fcol;
		t->parityStripeID = rbuf->parityStripeID;
		t->which_ru = rbuf->which_ru;
		t->failedDiskSectorOffset = rbuf->failedDiskSectorOffset;
		t->spRow = rbuf->spRow;
		t->spCol = rbuf->spCol;
		t->spOffset = rbuf->spOffset;

		ta = t->buffer;
		t->buffer = rbuf->buffer;
		rbuf->buffer = ta;	/* swap buffers */
	}
	/* the first installation always gets installed as the destination
	 * buffer. subsequent installations get stacked up to allow for
	 * multi-way XOR */
	if (!pssPtr->rbuf) {
		pssPtr->rbuf = t;
		t->count = 1;
	} else
		pssPtr->rbufsForXor[pssPtr->xorBufCount++] = t;	/* install this buffer */

	rf_CheckForFullRbuf(raidPtr, reconCtrlPtr, pssPtr, layoutPtr->numDataCol);	/* the buffer is full if
											 * G=2 */

out:
	RF_UNLOCK_PSS_MUTEX(raidPtr, rbuf->row, rbuf->parityStripeID);
	RF_UNLOCK_MUTEX(reconCtrlPtr->rb_mutex);
	return (retcode);
}

int 
rf_MultiWayReconXor(raidPtr, pssPtr)
	RF_Raid_t *raidPtr;
	RF_ReconParityStripeStatus_t *pssPtr;	/* the pss descriptor for this
						 * parity stripe */
{
	int     i, numBufs = pssPtr->xorBufCount;
	int     numBytes = rf_RaidAddressToByte(raidPtr, raidPtr->Layout.sectorsPerStripeUnit * raidPtr->Layout.SUsPerRU);
	RF_ReconBuffer_t **rbufs = (RF_ReconBuffer_t **) pssPtr->rbufsForXor;
	RF_ReconBuffer_t *targetRbuf = (RF_ReconBuffer_t *) pssPtr->rbuf;

	RF_ASSERT(pssPtr->rbuf != NULL);
	RF_ASSERT(numBufs > 0 && numBufs < RF_PS_MAX_BUFS);
#ifdef _KERNEL
#ifndef __NetBSD__
#ifndef __FreeBSD__
	thread_block();		/* yield the processor before doing a big XOR */
#endif
#endif
#endif				/* _KERNEL */
	/*
         * XXX
         *
         * What if more than 9 bufs?
         */
	nWayXorFuncs[numBufs] (pssPtr->rbufsForXor, targetRbuf, numBytes / sizeof(long));

	/* release all the reconstruction buffers except the last one, which
	 * belongs to the disk whose submission caused this XOR to take place */
	for (i = 0; i < numBufs - 1; i++) {
		if (rbufs[i]->type == RF_RBUF_TYPE_FLOATING)
			rf_ReleaseFloatingReconBuffer(raidPtr, rbufs[i]->row, rbufs[i]);
		else
			if (rbufs[i]->type == RF_RBUF_TYPE_FORCED)
				rf_FreeReconBuffer(rbufs[i]);
			else
				RF_ASSERT(0);
	}
	targetRbuf->count += pssPtr->xorBufCount;
	pssPtr->xorBufCount = 0;
	return (0);
}
/* removes one full buffer from one of the full-buffer lists and returns it.
 *
 * ASSUMES THE RB_MUTEX IS UNLOCKED AT ENTRY.
 */
RF_ReconBuffer_t *
rf_GetFullReconBuffer(reconCtrlPtr)
	RF_ReconCtrl_t *reconCtrlPtr;
{
	RF_ReconBuffer_t *p;

	RF_LOCK_MUTEX(reconCtrlPtr->rb_mutex);

	if ((p = reconCtrlPtr->priorityList) != NULL) {
		reconCtrlPtr->priorityList = p->next;
		p->next = NULL;
		goto out;
	}
	if ((p = reconCtrlPtr->fullBufferList) != NULL) {
		reconCtrlPtr->fullBufferList = p->next;
		p->next = NULL;
		goto out;
	}
out:
	RF_UNLOCK_MUTEX(reconCtrlPtr->rb_mutex);
	return (p);
}


/* if the reconstruction buffer is full, move it to the full list,
 * which is maintained sorted by failed disk sector offset
 *
 * ASSUMES THE RB_MUTEX IS LOCKED AT ENTRY.  */
int 
rf_CheckForFullRbuf(raidPtr, reconCtrl, pssPtr, numDataCol)
	RF_Raid_t *raidPtr;
	RF_ReconCtrl_t *reconCtrl;
	RF_ReconParityStripeStatus_t *pssPtr;
	int     numDataCol;
{
	RF_ReconBuffer_t *p, *pt, *rbuf = (RF_ReconBuffer_t *) pssPtr->rbuf;

	if (rbuf->count == numDataCol) {
		raidPtr->numFullReconBuffers++;
		Dprintf2("RECON: rbuf for psid %ld ru %d has filled\n",
		    (long) rbuf->parityStripeID, rbuf->which_ru);
		if (!reconCtrl->fullBufferList || (rbuf->failedDiskSectorOffset < reconCtrl->fullBufferList->failedDiskSectorOffset)) {
			Dprintf2("RECON: rbuf for psid %ld ru %d is head of list\n",
			    (long) rbuf->parityStripeID, rbuf->which_ru);
			rbuf->next = reconCtrl->fullBufferList;
			reconCtrl->fullBufferList = rbuf;
		} else {
			for (pt = reconCtrl->fullBufferList, p = pt->next; p && p->failedDiskSectorOffset < rbuf->failedDiskSectorOffset; pt = p, p = p->next);
			rbuf->next = p;
			pt->next = rbuf;
			Dprintf2("RECON: rbuf for psid %ld ru %d is in list\n",
			    (long) rbuf->parityStripeID, rbuf->which_ru);
		}
#if 0
		pssPtr->writeRbuf = pssPtr->rbuf;	/* DEBUG ONLY:  we like
							 * to be able to find
							 * this rbuf while it's
							 * awaiting write */
#else
		rbuf->pssPtr = pssPtr;
#endif
		pssPtr->rbuf = NULL;
		rf_CauseReconEvent(raidPtr, rbuf->row, rbuf->col, NULL, RF_REVENT_BUFREADY);
	}
	return (0);
}


/* release a floating recon buffer for someone else to use.
 * assumes the rb_mutex is LOCKED at entry
 */
void 
rf_ReleaseFloatingReconBuffer(raidPtr, row, rbuf)
	RF_Raid_t *raidPtr;
	RF_RowCol_t row;
	RF_ReconBuffer_t *rbuf;
{
	RF_ReconCtrl_t *rcPtr = raidPtr->reconControl[row];
	RF_CallbackDesc_t *cb;

	Dprintf2("RECON: releasing rbuf for psid %ld ru %d\n",
	    (long) rbuf->parityStripeID, rbuf->which_ru);

	/* if anyone is waiting on buffers, wake one of them up.  They will
	 * subsequently wake up anyone else waiting on their RU */
	if (rcPtr->bufferWaitList) {
		rbuf->next = rcPtr->committedRbufs;
		rcPtr->committedRbufs = rbuf;
		cb = rcPtr->bufferWaitList;
		rcPtr->bufferWaitList = cb->next;
		rf_CauseReconEvent(raidPtr, cb->row, cb->col, (void *) 1, RF_REVENT_BUFCLEAR);	/* arg==1 => we've
												 * committed a buffer */
		rf_FreeCallbackDesc(cb);
		raidPtr->procsInBufWait--;
	} else {
		rbuf->next = rcPtr->floatingRbufs;
		rcPtr->floatingRbufs = rbuf;
	}
}
/* release any disk that is waiting on a buffer for the indicated RU.
 * assumes the rb_mutex is LOCKED at entry
 */
void 
rf_ReleaseBufferWaiters(raidPtr, pssPtr)
	RF_Raid_t *raidPtr;
	RF_ReconParityStripeStatus_t *pssPtr;
{
	RF_CallbackDesc_t *cb1, *cb = pssPtr->bufWaitList;

	Dprintf2("RECON: releasing buf waiters for psid %ld ru %d\n",
	    (long) pssPtr->parityStripeID, pssPtr->which_ru);
	pssPtr->flags &= ~RF_PSS_BUFFERWAIT;
	while (cb) {
		cb1 = cb->next;
		cb->next = NULL;
		rf_CauseReconEvent(raidPtr, cb->row, cb->col, (void *) 0, RF_REVENT_BUFCLEAR);	/* arg==0 => we haven't
												 * committed a buffer */
		rf_FreeCallbackDesc(cb);
		cb = cb1;
	}
	pssPtr->bufWaitList = NULL;
}
/* when reconstruction is forced on an RU, there may be some disks waiting to
 * acquire a buffer for that RU.  Since we allocate a new buffer as part of
 * the forced-reconstruction process, we no longer have to wait for any
 * buffers, so we wakeup any waiter that we find in the bufferWaitList
 *
 * assumes the rb_mutex is LOCKED at entry
 */
void 
rf_ReleaseBufferWaiter(rcPtr, rbuf)
	RF_ReconCtrl_t *rcPtr;
	RF_ReconBuffer_t *rbuf;
{
	RF_CallbackDesc_t *cb, *cbt;

	for (cbt = NULL, cb = rcPtr->bufferWaitList; cb; cbt = cb, cb = cb->next) {
		if ((cb->callbackArg.v == rbuf->parityStripeID) && (cb->callbackArg2.v == rbuf->which_ru)) {
			Dprintf2("RECON: Dropping row %d col %d from buffer wait list\n", cb->row, cb->col);
			if (cbt)
				cbt->next = cb->next;
			else
				rcPtr->bufferWaitList = cb->next;
			rf_CauseReconEvent((RF_Raid_t *) rbuf->raidPtr, cb->row, cb->col, (void *) 0, RF_REVENT_BUFREADY);	/* arg==0 => no
																 * committed buffer */
			rf_FreeCallbackDesc(cb);
			return;
		}
	}
}
