/*	$FreeBSD$ */
/*	$NetBSD: rf_diskqueue.h,v 1.5 2000/02/13 04:53:57 oster Exp $	*/
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

/*****************************************************************************************
 *
 * rf_diskqueue.h -- header file for disk queues
 *
 * see comments in rf_diskqueue.c
 *
 ****************************************************************************************/


#ifndef _RF__RF_DISKQUEUE_H_
#define _RF__RF_DISKQUEUE_H_

#include <dev/raidframe/rf_threadstuff.h>
#include <dev/raidframe/rf_acctrace.h>
#include <dev/raidframe/rf_alloclist.h>
#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_etimer.h>

#include <dev/raidframe/rf_bsd.h>

#define RF_IO_NORMAL_PRIORITY 1
#define RF_IO_LOW_PRIORITY    0

/* the data held by a disk queue entry */
struct RF_DiskQueueData_s {
	RF_SectorNum_t sectorOffset;	/* sector offset into the disk */
	RF_SectorCount_t numSector;	/* number of sectors to read/write */
	RF_IoType_t type;	/* read/write/nop */
	caddr_t buf;		/* buffer pointer */
	RF_StripeNum_t parityStripeID;	/* the RAID parity stripe ID this
					 * access is for */
	RF_ReconUnitNum_t which_ru;	/* which RU within this parity stripe */
	int     priority;	/* the priority of this request */
	int     (*CompleteFunc) (void *, int);	/* function to be called upon
						 * completion */
	int     (*AuxFunc) (void *,...);	/* function called upon
						 * completion of the first I/O
						 * of a Read_Op_Write pair */
	void   *argument;	/* argument to be passed to CompleteFunc */
	RF_Raid_t *raidPtr;	/* needed for simulation */
	RF_AccTraceEntry_t *tracerec;	/* perf mon only */
	RF_Etimer_t qtime;	/* perf mon only - time request is in queue */
	long    entryTime;
	RF_DiskQueueData_t *next;
	RF_DiskQueueData_t *prev;
	caddr_t buf2;		/* for read-op-write */
	dev_t   dev;		/* the device number for in-kernel version */
	RF_DiskQueue_t *queue;	/* the disk queue to which this req is
				 * targeted */
	RF_DiskQueueDataFlags_t flags;	/* flags controlling operation */

	struct proc *b_proc;	/* the b_proc from the original bp passed into
				 * the driver for this I/O */
				/* XXX Should this be changed to the opaque
				 * RF_Thread_t ? */
	RF_Buf_t bp;		/* a bp to use to get this I/O done */
};
#define RF_LOCK_DISK_QUEUE   0x01
#define RF_UNLOCK_DISK_QUEUE 0x02

/* note: "Create" returns type-specific queue header pointer cast to (void *) */
struct RF_DiskQueueSW_s {
	RF_DiskQueueType_t queueType;
	void   *(*Create) (RF_SectorCount_t, RF_AllocListElem_t *, RF_ShutdownList_t **);	/* creation routine --
												 * one call per queue in
												 * system */
	void    (*Enqueue) (void *, RF_DiskQueueData_t *, int);	/* enqueue routine */
	RF_DiskQueueData_t *(*Dequeue) (void *);	/* dequeue routine */
	RF_DiskQueueData_t *(*Peek) (void *);	/* peek at head of queue */

	/* the rest are optional:  they improve performance, but the driver
	 * will deal with it if they don't exist */
	int     (*Promote) (void *, RF_StripeNum_t, RF_ReconUnitNum_t);	/* promotes priority of
									 * tagged accesses */
};

struct RF_DiskQueue_s {
	RF_DiskQueueSW_t *qPtr;	/* access point to queue functions */
	void   *qHdr;		/* queue header, of whatever type */
	        RF_DECLARE_MUTEX(mutex)	/* mutex locking data structures */
	        RF_DECLARE_COND(cond)	/* condition variable for
					 * synchronization */
	long    numOutstanding;	/* number of I/Os currently outstanding on
				 * disk */
	long    maxOutstanding;	/* max # of I/Os that can be outstanding on a
				 * disk (in-kernel only) */
	int     curPriority;	/* the priority of accs all that are currently
				 * outstanding */
	long    queueLength;	/* number of requests in queue */
	RF_DiskQueueData_t *nextLockingOp;	/* a locking op that has
						 * arrived at the head of the
						 * queue & is waiting for
						 * drainage */
	RF_DiskQueueData_t *unlockingOp;	/* used at user level to
						 * communicate unlocking op
						 * b/w user (or dag exec) &
						 * disk threads */
	int     numWaiting;	/* number of threads waiting on this variable.
				 * user-level only */
	RF_DiskQueueFlags_t flags;	/* terminate, locked */
	RF_Raid_t *raidPtr;	/* associated array */
	dev_t   dev;		/* device number for kernel version */
	RF_SectorNum_t last_deq_sector;	/* last sector number dequeued or
					 * dispatched */
	int     row, col;	/* debug only */
	struct raidcinfo *rf_cinfo;	/* disks component info.. */
};
#define RF_DQ_LOCKED  0x02	/* no new accs allowed until queue is
				 * explicitly unlocked */

/* macros setting & returning information about queues and requests */
#define RF_QUEUE_LOCKED(_q)                 ((_q)->flags & RF_DQ_LOCKED)
#define RF_QUEUE_EMPTY(_q)                  (((_q)->numOutstanding == 0) && ((_q)->nextLockingOp == NULL) && !RF_QUEUE_LOCKED(_q))
#define RF_QUEUE_FULL(_q)                   ((_q)->numOutstanding == (_q)->maxOutstanding)

#define RF_LOCK_QUEUE(_q)                   (_q)->flags |= RF_DQ_LOCKED
#define RF_UNLOCK_QUEUE(_q)                 (_q)->flags &= ~RF_DQ_LOCKED

#define RF_LOCK_QUEUE_MUTEX(_q_,_wh_)   RF_LOCK_MUTEX((_q_)->mutex)
#define RF_UNLOCK_QUEUE_MUTEX(_q_,_wh_) RF_UNLOCK_MUTEX((_q_)->mutex)

#define RF_LOCKING_REQ(_r)                  ((_r)->flags & RF_LOCK_DISK_QUEUE)
#define RF_UNLOCKING_REQ(_r)                ((_r)->flags & RF_UNLOCK_DISK_QUEUE)

/* whether it is ok to dispatch a regular request */
#define RF_OK_TO_DISPATCH(_q_,_r_) \
  (RF_QUEUE_EMPTY(_q_) || \
    (!RF_QUEUE_FULL(_q_) && ((_r_)->priority >= (_q_)->curPriority)))

int     rf_ConfigureDiskQueueSystem(RF_ShutdownList_t ** listp);

void    rf_TerminateDiskQueues(RF_Raid_t * raidPtr);

int 
rf_ConfigureDiskQueues(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr);

void    rf_DiskIOEnqueue(RF_DiskQueue_t * queue, RF_DiskQueueData_t * req, int pri);


void    rf_DiskIOComplete(RF_DiskQueue_t * queue, RF_DiskQueueData_t * req, int status);

int 
rf_DiskIOPromote(RF_DiskQueue_t * queue, RF_StripeNum_t parityStripeID,
    RF_ReconUnitNum_t which_ru);

RF_DiskQueueData_t *
rf_CreateDiskQueueData(RF_IoType_t typ, RF_SectorNum_t ssect, 
		       RF_SectorCount_t nsect, caddr_t buf,
		       RF_StripeNum_t parityStripeID, 
		       RF_ReconUnitNum_t which_ru,
		       int (*wakeF) (void *, int),
		       void *arg, RF_DiskQueueData_t * next, 
		       RF_AccTraceEntry_t * tracerec,
		       void *raidPtr, RF_DiskQueueDataFlags_t flags, 
		       void *kb_proc);

RF_DiskQueueData_t *
rf_CreateDiskQueueDataFull(RF_IoType_t typ, RF_SectorNum_t ssect, 
			   RF_SectorCount_t nsect, caddr_t buf,
			   RF_StripeNum_t parityStripeID, 
			   RF_ReconUnitNum_t which_ru,
			   int (*wakeF) (void *, int),
			   void *arg, RF_DiskQueueData_t * next, 
			   RF_AccTraceEntry_t * tracerec,
			   int priority, int (*AuxFunc) (void *,...), 
			   caddr_t buf2, void *raidPtr, 
			   RF_DiskQueueDataFlags_t flags, void *kb_proc);

void    
rf_FreeDiskQueueData(RF_DiskQueueData_t * p);

int 
rf_ConfigureDiskQueue(RF_Raid_t *, RF_DiskQueue_t *, RF_RowCol_t, 
		      RF_RowCol_t, RF_DiskQueueSW_t *,
		      RF_SectorCount_t, dev_t, int, 
		      RF_ShutdownList_t **,
		      RF_AllocListElem_t *);
#endif				/* !_RF__RF_DISKQUEUE_H_ */
