/*	$FreeBSD$ */
/*	$NetBSD: rf_diskqueue.c,v 1.13 2000/03/04 04:22:34 oster Exp $	*/
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

/****************************************************************************
 *
 * rf_diskqueue.c -- higher-level disk queue code
 *
 * the routines here are a generic wrapper around the actual queueing
 * routines.  The code here implements thread scheduling, synchronization,
 * and locking ops (see below) on top of the lower-level queueing code.
 *
 * to support atomic RMW, we implement "locking operations".  When a
 * locking op is dispatched to the lower levels of the driver, the
 * queue is locked, and no further I/Os are dispatched until the queue
 * receives & completes a corresponding "unlocking operation".  This
 * code relies on the higher layers to guarantee that a locking op
 * will always be eventually followed by an unlocking op.  The model
 * is that the higher layers are structured so locking and unlocking
 * ops occur in pairs, i.e.  an unlocking op cannot be generated until
 * after a locking op reports completion.  There is no good way to
 * check to see that an unlocking op "corresponds" to the op that
 * currently has the queue locked, so we make no such attempt.  Since
 * by definition there can be only one locking op outstanding on a
 * disk, this should not be a problem.
 *
 * In the kernel, we allow multiple I/Os to be concurrently dispatched
 * to the disk driver.  In order to support locking ops in this
 * environment, when we decide to do a locking op, we stop dispatching
 * new I/Os and wait until all dispatched I/Os have completed before
 * dispatching the locking op.
 *
 * Unfortunately, the code is different in the 3 different operating
 * states (user level, kernel, simulator).  In the kernel, I/O is
 * non-blocking, and we have no disk threads to dispatch for us.
 * Therefore, we have to dispatch new I/Os to the scsi driver at the
 * time of enqueue, and also at the time of completion.  At user
 * level, I/O is blocking, and so only the disk threads may dispatch
 * I/Os.  Thus at user level, all we can do at enqueue time is enqueue
 * and wake up the disk thread to do the dispatch.
 *
 ****************************************************************************/

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_threadstuff.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_diskqueue.h>
#include <dev/raidframe/rf_alloclist.h>
#include <dev/raidframe/rf_acctrace.h>
#include <dev/raidframe/rf_etimer.h>
#include <dev/raidframe/rf_configure.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_freelist.h>
#include <dev/raidframe/rf_debugprint.h>
#include <dev/raidframe/rf_shutdown.h>
#include <dev/raidframe/rf_cvscan.h>
#include <dev/raidframe/rf_sstf.h>
#include <dev/raidframe/rf_fifo.h>
#include <dev/raidframe/rf_kintf.h>

static int init_dqd(RF_DiskQueueData_t *);
static void clean_dqd(RF_DiskQueueData_t *);
static void rf_ShutdownDiskQueueSystem(void *);

#define Dprintf1(s,a)         if (rf_queueDebug) rf_debug_printf(s,(void *)((unsigned long)a),NULL,NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf2(s,a,b)       if (rf_queueDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf3(s,a,b,c)     if (rf_queueDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),NULL,NULL,NULL,NULL,NULL)

/*****************************************************************************
 *
 * the disk queue switch defines all the functions used in the
 * different queueing disciplines queue ID, init routine, enqueue
 * routine, dequeue routine
 *
 ****************************************************************************/

static RF_DiskQueueSW_t diskqueuesw[] = {
	{"fifo",		/* FIFO */
		rf_FifoCreate,
		rf_FifoEnqueue,
		rf_FifoDequeue,
		rf_FifoPeek,
	rf_FifoPromote},

	{"cvscan",		/* cvscan */
		rf_CvscanCreate,
		rf_CvscanEnqueue,
		rf_CvscanDequeue,
		rf_CvscanPeek,
	rf_CvscanPromote},

	{"sstf",		/* shortest seek time first */
		rf_SstfCreate,
		rf_SstfEnqueue,
		rf_SstfDequeue,
		rf_SstfPeek,
	rf_SstfPromote},

	{"scan",		/* SCAN (two-way elevator) */
		rf_ScanCreate,
		rf_SstfEnqueue,
		rf_ScanDequeue,
		rf_ScanPeek,
	rf_SstfPromote},

	{"cscan",		/* CSCAN (one-way elevator) */
		rf_CscanCreate,
		rf_SstfEnqueue,
		rf_CscanDequeue,
		rf_CscanPeek,
	rf_SstfPromote},

};
#define NUM_DISK_QUEUE_TYPES (sizeof(diskqueuesw)/sizeof(RF_DiskQueueSW_t))

static RF_FreeList_t *rf_dqd_freelist;

#define RF_MAX_FREE_DQD 256
#define RF_DQD_INC       16
#define RF_DQD_INITIAL   64

#if defined(__FreeBSD__) && __FreeBSD_version > 500005
#include <sys/bio.h>
#endif

#include <sys/buf.h>

static int 
init_dqd(dqd)
	RF_DiskQueueData_t *dqd;
{

	dqd->bp = (RF_Buf_t) malloc(sizeof(RF_Buf_t), M_RAIDFRAME, M_NOWAIT);
	if (dqd->bp == NULL) {
		return (ENOMEM);
	}
	memset(dqd->bp, 0, sizeof(RF_Buf_t));	/* if you don't do it, nobody
						 * else will.. */
	return (0);
}

static void 
clean_dqd(dqd)
	RF_DiskQueueData_t *dqd;
{
	free(dqd->bp, M_RAIDFRAME);
}
/* configures a single disk queue */

int 
rf_ConfigureDiskQueue(
      RF_Raid_t * raidPtr,
      RF_DiskQueue_t * diskqueue,
      RF_RowCol_t r,		/* row & col -- debug only.  BZZT not any
				 * more... */
      RF_RowCol_t c,
      RF_DiskQueueSW_t * p,
      RF_SectorCount_t sectPerDisk,
      dev_t dev,
      int maxOutstanding,
      RF_ShutdownList_t ** listp,
      RF_AllocListElem_t * clList)
{
	int     rc;

	diskqueue->row = r;
	diskqueue->col = c;
	diskqueue->qPtr = p;
	diskqueue->qHdr = (p->Create) (sectPerDisk, clList, listp);
	diskqueue->dev = dev;
	diskqueue->numOutstanding = 0;
	diskqueue->queueLength = 0;
	diskqueue->maxOutstanding = maxOutstanding;
	diskqueue->curPriority = RF_IO_NORMAL_PRIORITY;
	diskqueue->nextLockingOp = NULL;
	diskqueue->unlockingOp = NULL;
	diskqueue->numWaiting = 0;
	diskqueue->flags = 0;
	diskqueue->raidPtr = raidPtr;
	diskqueue->rf_cinfo = &raidPtr->raid_cinfo[r][c];
	rc = rf_create_managed_mutex(listp, &diskqueue->mutex);
	if (rc) {
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		return (rc);
	}
	rc = rf_create_managed_cond(listp, &diskqueue->cond);
	if (rc) {
		RF_ERRORMSG3("Unable to init cond file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		return (rc);
	}
	return (0);
}

static void 
rf_ShutdownDiskQueueSystem(ignored)
	void   *ignored;
{
	RF_FREELIST_DESTROY_CLEAN(rf_dqd_freelist, next, (RF_DiskQueueData_t *), clean_dqd);
}

int 
rf_ConfigureDiskQueueSystem(listp)
	RF_ShutdownList_t **listp;
{
	int     rc;

	RF_FREELIST_CREATE(rf_dqd_freelist, RF_MAX_FREE_DQD,
	    RF_DQD_INC, sizeof(RF_DiskQueueData_t));
	if (rf_dqd_freelist == NULL)
		return (ENOMEM);
	rc = rf_ShutdownCreate(listp, rf_ShutdownDiskQueueSystem, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n",
		    __FILE__, __LINE__, rc);
		rf_ShutdownDiskQueueSystem(NULL);
		return (rc);
	}
	RF_FREELIST_PRIME_INIT(rf_dqd_freelist, RF_DQD_INITIAL, next,
	    (RF_DiskQueueData_t *), init_dqd);
	return (0);
}

int 
rf_ConfigureDiskQueues(
    RF_ShutdownList_t ** listp,
    RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr)
{
	RF_DiskQueue_t **diskQueues, *spareQueues;
	RF_DiskQueueSW_t *p;
	RF_RowCol_t r, c;
	int     rc, i;

	raidPtr->maxQueueDepth = cfgPtr->maxOutstandingDiskReqs;

	for (p = NULL, i = 0; i < NUM_DISK_QUEUE_TYPES; i++) {
		if (!strcmp(diskqueuesw[i].queueType, cfgPtr->diskQueueType)) {
			p = &diskqueuesw[i];
			break;
		}
	}
	if (p == NULL) {
		RF_ERRORMSG2("Unknown queue type \"%s\".  Using %s\n", cfgPtr->diskQueueType, diskqueuesw[0].queueType);
		p = &diskqueuesw[0];
	}
	raidPtr->qType = p;
	RF_CallocAndAdd(diskQueues, raidPtr->numRow, sizeof(RF_DiskQueue_t *), (RF_DiskQueue_t **), raidPtr->cleanupList);
	if (diskQueues == NULL) {
		return (ENOMEM);
	}
	raidPtr->Queues = diskQueues;
	for (r = 0; r < raidPtr->numRow; r++) {
		RF_CallocAndAdd(diskQueues[r], raidPtr->numCol + 
				 ((r == 0) ? RF_MAXSPARE : 0), 
				sizeof(RF_DiskQueue_t), (RF_DiskQueue_t *), 
				raidPtr->cleanupList);
		if (diskQueues[r] == NULL)
			return (ENOMEM);
		for (c = 0; c < raidPtr->numCol; c++) {
			rc = rf_ConfigureDiskQueue(raidPtr, &diskQueues[r][c],
						   r, c, p,
						   raidPtr->sectorsPerDisk, 
						   raidPtr->Disks[r][c].dev,
						   cfgPtr->maxOutstandingDiskReqs, 
						   listp, raidPtr->cleanupList);
			if (rc)
				return (rc);
		}
	}

	spareQueues = &raidPtr->Queues[0][raidPtr->numCol];
	for (r = 0; r < raidPtr->numSpare; r++) {
		rc = rf_ConfigureDiskQueue(raidPtr, &spareQueues[r],
		    0, raidPtr->numCol + r, p,
		    raidPtr->sectorsPerDisk,
		    raidPtr->Disks[0][raidPtr->numCol + r].dev,
		    cfgPtr->maxOutstandingDiskReqs, listp,
		    raidPtr->cleanupList);
		if (rc)
			return (rc);
	}
	return (0);
}
/* Enqueue a disk I/O
 *
 * Unfortunately, we have to do things differently in the different
 * environments (simulator, user-level, kernel).
 * At user level, all I/O is blocking, so we have 1 or more threads/disk
 * and the thread that enqueues is different from the thread that dequeues.
 * In the kernel, I/O is non-blocking and so we'd like to have multiple
 * I/Os outstanding on the physical disks when possible.
 *
 * when any request arrives at a queue, we have two choices:
 *    dispatch it to the lower levels
 *    queue it up
 *
 * kernel rules for when to do what:
 *    locking request:  queue empty => dispatch and lock queue,
 *                      else queue it
 *    unlocking req  :  always dispatch it
 *    normal req     :  queue empty => dispatch it & set priority
 *                      queue not full & priority is ok => dispatch it
 *                      else queue it
 *
 * user-level rules:
 *    always enqueue.  In the special case of an unlocking op, enqueue
 *    in a special way that will cause the unlocking op to be the next
 *    thing dequeued.
 *
 * simulator rules:
 *    Do the same as at user level, with the sleeps and wakeups suppressed.
 */
void 
rf_DiskIOEnqueue(queue, req, pri)
	RF_DiskQueue_t *queue;
	RF_DiskQueueData_t *req;
	int     pri;
{
	RF_ETIMER_START(req->qtime);
	RF_ASSERT(req->type == RF_IO_TYPE_NOP || req->numSector);
	req->priority = pri;

	if (rf_queueDebug && (req->numSector == 0)) {
		printf("Warning: Enqueueing zero-sector access\n");
	}
	/*
         * kernel
         */
	RF_LOCK_QUEUE_MUTEX(queue, "DiskIOEnqueue");
	/* locking request */
	if (RF_LOCKING_REQ(req)) {
		if (RF_QUEUE_EMPTY(queue)) {
			Dprintf3("Dispatching pri %d locking op to r %d c %d (queue empty)\n", pri, queue->row, queue->col);
			RF_LOCK_QUEUE(queue);
			rf_DispatchKernelIO(queue, req);
		} else {
			queue->queueLength++;	/* increment count of number
						 * of requests waiting in this
						 * queue */
			Dprintf3("Enqueueing pri %d locking op to r %d c %d (queue not empty)\n", pri, queue->row, queue->col);
			req->queue = (void *) queue;
			(queue->qPtr->Enqueue) (queue->qHdr, req, pri);
		}
	}
	/* unlocking request */
	else
		if (RF_UNLOCKING_REQ(req)) {	/* we'll do the actual unlock
						 * when this I/O completes */
			Dprintf3("Dispatching pri %d unlocking op to r %d c %d\n", pri, queue->row, queue->col);
			RF_ASSERT(RF_QUEUE_LOCKED(queue));
			rf_DispatchKernelIO(queue, req);
		}
	/* normal request */
		else
			if (RF_OK_TO_DISPATCH(queue, req)) {
				Dprintf3("Dispatching pri %d regular op to r %d c %d (ok to dispatch)\n", pri, queue->row, queue->col);
				rf_DispatchKernelIO(queue, req);
			} else {
				queue->queueLength++;	/* increment count of
							 * number of requests
							 * waiting in this queue */
				Dprintf3("Enqueueing pri %d regular op to r %d c %d (not ok to dispatch)\n", pri, queue->row, queue->col);
				req->queue = (void *) queue;
				(queue->qPtr->Enqueue) (queue->qHdr, req, pri);
			}
	RF_UNLOCK_QUEUE_MUTEX(queue, "DiskIOEnqueue");
}


/* get the next set of I/Os started, kernel version only */
void 
rf_DiskIOComplete(queue, req, status)
	RF_DiskQueue_t *queue;
	RF_DiskQueueData_t *req;
	int     status;
{
	int     done = 0;

	RF_LOCK_QUEUE_MUTEX(queue, "DiskIOComplete");

	/* unlock the queue: (1) after an unlocking req completes (2) after a
	 * locking req fails */
	if (RF_UNLOCKING_REQ(req) || (RF_LOCKING_REQ(req) && status)) {
		Dprintf2("DiskIOComplete: unlocking queue at r %d c %d\n", queue->row, queue->col);
		RF_ASSERT(RF_QUEUE_LOCKED(queue) && (queue->unlockingOp == NULL));
		RF_UNLOCK_QUEUE(queue);
	}
	queue->numOutstanding--;
	RF_ASSERT(queue->numOutstanding >= 0);

	/* dispatch requests to the disk until we find one that we can't. */
	/* no reason to continue once we've filled up the queue */
	/* no reason to even start if the queue is locked */

	while (!done && !RF_QUEUE_FULL(queue) && !RF_QUEUE_LOCKED(queue)) {
		if (queue->nextLockingOp) {
			req = queue->nextLockingOp;
			queue->nextLockingOp = NULL;
			Dprintf3("DiskIOComplete: a pri %d locking req was pending at r %d c %d\n", req->priority, queue->row, queue->col);
		} else {
			req = (queue->qPtr->Dequeue) (queue->qHdr);
			if (req != NULL) {
				Dprintf3("DiskIOComplete: extracting pri %d req from queue at r %d c %d\n", req->priority, queue->row, queue->col);
			} else {
				Dprintf1("DiskIOComplete: no more requests to extract.\n", "");
			}
		}
		if (req) {
			queue->queueLength--;	/* decrement count of number
						 * of requests waiting in this
						 * queue */
			RF_ASSERT(queue->queueLength >= 0);
		}
		if (!req)
			done = 1;
		else
			if (RF_LOCKING_REQ(req)) {
				if (RF_QUEUE_EMPTY(queue)) {	/* dispatch it */
					Dprintf3("DiskIOComplete: dispatching pri %d locking req to r %d c %d (queue empty)\n", req->priority, queue->row, queue->col);
					RF_LOCK_QUEUE(queue);
					rf_DispatchKernelIO(queue, req);
					done = 1;
				} else {	/* put it aside to wait for
						 * the queue to drain */
					Dprintf3("DiskIOComplete: postponing pri %d locking req to r %d c %d\n", req->priority, queue->row, queue->col);
					RF_ASSERT(queue->nextLockingOp == NULL);
					queue->nextLockingOp = req;
					done = 1;
				}
			} else
				if (RF_UNLOCKING_REQ(req)) {	/* should not happen:
								 * unlocking ops should
								 * not get queued */
					RF_ASSERT(RF_QUEUE_LOCKED(queue));	/* support it anyway for
										 * the future */
					Dprintf3("DiskIOComplete: dispatching pri %d unl req to r %d c %d (SHOULD NOT SEE THIS)\n", req->priority, queue->row, queue->col);
					rf_DispatchKernelIO(queue, req);
					done = 1;
				} else
					if (RF_OK_TO_DISPATCH(queue, req)) {
						Dprintf3("DiskIOComplete: dispatching pri %d regular req to r %d c %d (ok to dispatch)\n", req->priority, queue->row, queue->col);
						rf_DispatchKernelIO(queue, req);
					} else {	/* we can't dispatch it,
							 * so just re-enqueue
							 * it.  */
						/* potential trouble here if
						 * disk queues batch reqs */
						Dprintf3("DiskIOComplete: re-enqueueing pri %d regular req to r %d c %d\n", req->priority, queue->row, queue->col);
						queue->queueLength++;
						(queue->qPtr->Enqueue) (queue->qHdr, req, req->priority);
						done = 1;
					}
	}

	RF_UNLOCK_QUEUE_MUTEX(queue, "DiskIOComplete");
}
/* promotes accesses tagged with the given parityStripeID from low priority
 * to normal priority.  This promotion is optional, meaning that a queue
 * need not implement it.  If there is no promotion routine associated with
 * a queue, this routine does nothing and returns -1.
 */
int 
rf_DiskIOPromote(queue, parityStripeID, which_ru)
	RF_DiskQueue_t *queue;
	RF_StripeNum_t parityStripeID;
	RF_ReconUnitNum_t which_ru;
{
	int     retval;

	if (!queue->qPtr->Promote)
		return (-1);
	RF_LOCK_QUEUE_MUTEX(queue, "DiskIOPromote");
	retval = (queue->qPtr->Promote) (queue->qHdr, parityStripeID, which_ru);
	RF_UNLOCK_QUEUE_MUTEX(queue, "DiskIOPromote");
	return (retval);
}

RF_DiskQueueData_t *
rf_CreateDiskQueueData(
    RF_IoType_t typ,
    RF_SectorNum_t ssect,
    RF_SectorCount_t nsect,
    caddr_t buf,
    RF_StripeNum_t parityStripeID,
    RF_ReconUnitNum_t which_ru,
    int (*wakeF) (void *, int),
    void *arg,
    RF_DiskQueueData_t * next,
    RF_AccTraceEntry_t * tracerec,
    void *raidPtr,
    RF_DiskQueueDataFlags_t flags,
    void *kb_proc)
{
	RF_DiskQueueData_t *p;

	RF_FREELIST_GET_INIT(rf_dqd_freelist, p, next, (RF_DiskQueueData_t *), init_dqd);

	p->sectorOffset = ssect + rf_protectedSectors;
	p->numSector = nsect;
	p->type = typ;
	p->buf = buf;
	p->parityStripeID = parityStripeID;
	p->which_ru = which_ru;
	p->CompleteFunc = wakeF;
	p->argument = arg;
	p->next = next;
	p->tracerec = tracerec;
	p->priority = RF_IO_NORMAL_PRIORITY;
	p->AuxFunc = NULL;
	p->buf2 = NULL;
	p->raidPtr = raidPtr;
	p->flags = flags;
	p->b_proc = kb_proc;
	return (p);
}

RF_DiskQueueData_t *
rf_CreateDiskQueueDataFull(
    RF_IoType_t typ,
    RF_SectorNum_t ssect,
    RF_SectorCount_t nsect,
    caddr_t buf,
    RF_StripeNum_t parityStripeID,
    RF_ReconUnitNum_t which_ru,
    int (*wakeF) (void *, int),
    void *arg,
    RF_DiskQueueData_t * next,
    RF_AccTraceEntry_t * tracerec,
    int priority,
    int (*AuxFunc) (void *,...),
    caddr_t buf2,
    void *raidPtr,
    RF_DiskQueueDataFlags_t flags,
    void *kb_proc)
{
	RF_DiskQueueData_t *p;

	RF_FREELIST_GET_INIT(rf_dqd_freelist, p, next, (RF_DiskQueueData_t *), init_dqd);

	p->sectorOffset = ssect + rf_protectedSectors;
	p->numSector = nsect;
	p->type = typ;
	p->buf = buf;
	p->parityStripeID = parityStripeID;
	p->which_ru = which_ru;
	p->CompleteFunc = wakeF;
	p->argument = arg;
	p->next = next;
	p->tracerec = tracerec;
	p->priority = priority;
	p->AuxFunc = AuxFunc;
	p->buf2 = buf2;
	p->raidPtr = raidPtr;
	p->flags = flags;
	p->b_proc = kb_proc;
	return (p);
}

void 
rf_FreeDiskQueueData(p)
	RF_DiskQueueData_t *p;
{
	RF_FREELIST_FREE_CLEAN(rf_dqd_freelist, p, next, clean_dqd);
}
