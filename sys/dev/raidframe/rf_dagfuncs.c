/*	$NetBSD: rf_dagfuncs.c,v 1.7 2001/02/03 12:51:10 mrg Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, William V. Courtright II
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

/*
 * dagfuncs.c -- DAG node execution routines
 *
 * Rules:
 * 1. Every DAG execution function must eventually cause node->status to
 *    get set to "good" or "bad", and "FinishNode" to be called. In the
 *    case of nodes that complete immediately (xor, NullNodeFunc, etc),
 *    the node execution function can do these two things directly. In
 *    the case of nodes that have to wait for some event (a disk read to
 *    complete, a lock to be released, etc) to occur before they can
 *    complete, this is typically achieved by having whatever module
 *    is doing the operation call GenericWakeupFunc upon completion.
 * 2. DAG execution functions should check the status in the DAG header
 *    and NOP out their operations if the status is not "enable". However,
 *    execution functions that release resources must be sure to release
 *    them even when they NOP out the function that would use them.
 *    Functions that acquire resources should go ahead and acquire them
 *    even when they NOP, so that a downstream release node will not have
 *    to check to find out whether or not the acquire was suppressed.
 */

#include <sys/param.h>
#if defined(__NetBSD__)
#include <sys/ioctl.h>
#elif defined(__FreeBSD__)
#include <sys/ioccom.h>
#include <sys/filio.h>
#endif

#include <dev/raidframe/rf_archs.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_layout.h>
#include <dev/raidframe/rf_etimer.h>
#include <dev/raidframe/rf_acctrace.h>
#include <dev/raidframe/rf_diskqueue.h>
#include <dev/raidframe/rf_dagfuncs.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_engine.h>
#include <dev/raidframe/rf_dagutils.h>

#include <dev/raidframe/rf_kintf.h>

#if RF_INCLUDE_PARITYLOGGING > 0
#include <dev/raidframe/rf_paritylog.h>
#endif				/* RF_INCLUDE_PARITYLOGGING > 0 */

int     (*rf_DiskReadFunc) (RF_DagNode_t *);
int     (*rf_DiskWriteFunc) (RF_DagNode_t *);
int     (*rf_DiskReadUndoFunc) (RF_DagNode_t *);
int     (*rf_DiskWriteUndoFunc) (RF_DagNode_t *);
int     (*rf_DiskUnlockFunc) (RF_DagNode_t *);
int     (*rf_DiskUnlockUndoFunc) (RF_DagNode_t *);
int     (*rf_RegularXorUndoFunc) (RF_DagNode_t *);
int     (*rf_SimpleXorUndoFunc) (RF_DagNode_t *);
int     (*rf_RecoveryXorUndoFunc) (RF_DagNode_t *);

/*****************************************************************************************
 * main (only) configuration routine for this module
 ****************************************************************************************/
int 
rf_ConfigureDAGFuncs(listp)
	RF_ShutdownList_t **listp;
{
	RF_ASSERT(((sizeof(long) == 8) && RF_LONGSHIFT == 3) || ((sizeof(long) == 4) && RF_LONGSHIFT == 2));
	rf_DiskReadFunc = rf_DiskReadFuncForThreads;
	rf_DiskReadUndoFunc = rf_DiskUndoFunc;
	rf_DiskWriteFunc = rf_DiskWriteFuncForThreads;
	rf_DiskWriteUndoFunc = rf_DiskUndoFunc;
	rf_DiskUnlockFunc = rf_DiskUnlockFuncForThreads;
	rf_DiskUnlockUndoFunc = rf_NullNodeUndoFunc;
	rf_RegularXorUndoFunc = rf_NullNodeUndoFunc;
	rf_SimpleXorUndoFunc = rf_NullNodeUndoFunc;
	rf_RecoveryXorUndoFunc = rf_NullNodeUndoFunc;
	return (0);
}



/*****************************************************************************************
 * the execution function associated with a terminate node
 ****************************************************************************************/
int 
rf_TerminateFunc(node)
	RF_DagNode_t *node;
{
	RF_ASSERT(node->dagHdr->numCommits == node->dagHdr->numCommitNodes);
	node->status = rf_good;
	return (rf_FinishNode(node, RF_THREAD_CONTEXT));
}

int 
rf_TerminateUndoFunc(node)
	RF_DagNode_t *node;
{
	return (0);
}


/*****************************************************************************************
 * execution functions associated with a mirror node
 *
 * parameters:
 *
 * 0 - physical disk addres of data
 * 1 - buffer for holding read data
 * 2 - parity stripe ID
 * 3 - flags
 * 4 - physical disk address of mirror (parity)
 *
 ****************************************************************************************/

int 
rf_DiskReadMirrorIdleFunc(node)
	RF_DagNode_t *node;
{
	/* select the mirror copy with the shortest queue and fill in node
	 * parameters with physical disk address */

	rf_SelectMirrorDiskIdle(node);
	return (rf_DiskReadFunc(node));
}

int 
rf_DiskReadMirrorPartitionFunc(node)
	RF_DagNode_t *node;
{
	/* select the mirror copy with the shortest queue and fill in node
	 * parameters with physical disk address */

	rf_SelectMirrorDiskPartition(node);
	return (rf_DiskReadFunc(node));
}

int 
rf_DiskReadMirrorUndoFunc(node)
	RF_DagNode_t *node;
{
	return (0);
}



#if RF_INCLUDE_PARITYLOGGING > 0
/*****************************************************************************************
 * the execution function associated with a parity log update node
 ****************************************************************************************/
int 
rf_ParityLogUpdateFunc(node)
	RF_DagNode_t *node;
{
	RF_PhysDiskAddr_t *pda = (RF_PhysDiskAddr_t *) node->params[0].p;
	caddr_t buf = (caddr_t) node->params[1].p;
	RF_ParityLogData_t *logData;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	RF_Etimer_t timer;

	if (node->dagHdr->status == rf_enable) {
		RF_ETIMER_START(timer);
		logData = rf_CreateParityLogData(RF_UPDATE, pda, buf,
		    (RF_Raid_t *) (node->dagHdr->raidPtr),
		    node->wakeFunc, (void *) node,
		    node->dagHdr->tracerec, timer);
		if (logData)
			rf_ParityLogAppend(logData, RF_FALSE, NULL, RF_FALSE);
		else {
			RF_ETIMER_STOP(timer);
			RF_ETIMER_EVAL(timer);
			tracerec->plog_us += RF_ETIMER_VAL_US(timer);
			(node->wakeFunc) (node, ENOMEM);
		}
	}
	return (0);
}


/*****************************************************************************************
 * the execution function associated with a parity log overwrite node
 ****************************************************************************************/
int 
rf_ParityLogOverwriteFunc(node)
	RF_DagNode_t *node;
{
	RF_PhysDiskAddr_t *pda = (RF_PhysDiskAddr_t *) node->params[0].p;
	caddr_t buf = (caddr_t) node->params[1].p;
	RF_ParityLogData_t *logData;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	RF_Etimer_t timer;

	if (node->dagHdr->status == rf_enable) {
		RF_ETIMER_START(timer);
		logData = rf_CreateParityLogData(RF_OVERWRITE, pda, buf, (RF_Raid_t *) (node->dagHdr->raidPtr),
		    node->wakeFunc, (void *) node, node->dagHdr->tracerec, timer);
		if (logData)
			rf_ParityLogAppend(logData, RF_FALSE, NULL, RF_FALSE);
		else {
			RF_ETIMER_STOP(timer);
			RF_ETIMER_EVAL(timer);
			tracerec->plog_us += RF_ETIMER_VAL_US(timer);
			(node->wakeFunc) (node, ENOMEM);
		}
	}
	return (0);
}
#else				/* RF_INCLUDE_PARITYLOGGING > 0 */

int 
rf_ParityLogUpdateFunc(node)
	RF_DagNode_t *node;
{
	return (0);
}
int 
rf_ParityLogOverwriteFunc(node)
	RF_DagNode_t *node;
{
	return (0);
}
#endif				/* RF_INCLUDE_PARITYLOGGING > 0 */

int 
rf_ParityLogUpdateUndoFunc(node)
	RF_DagNode_t *node;
{
	return (0);
}

int 
rf_ParityLogOverwriteUndoFunc(node)
	RF_DagNode_t *node;
{
	return (0);
}
/*****************************************************************************************
 * the execution function associated with a NOP node
 ****************************************************************************************/
int 
rf_NullNodeFunc(node)
	RF_DagNode_t *node;
{
	node->status = rf_good;
	return (rf_FinishNode(node, RF_THREAD_CONTEXT));
}

int 
rf_NullNodeUndoFunc(node)
	RF_DagNode_t *node;
{
	node->status = rf_undone;
	return (rf_FinishNode(node, RF_THREAD_CONTEXT));
}


/*****************************************************************************************
 * the execution function associated with a disk-read node
 ****************************************************************************************/
int 
rf_DiskReadFuncForThreads(node)
	RF_DagNode_t *node;
{
	RF_DiskQueueData_t *req;
	RF_PhysDiskAddr_t *pda = (RF_PhysDiskAddr_t *) node->params[0].p;
	caddr_t buf = (caddr_t) node->params[1].p;
	RF_StripeNum_t parityStripeID = (RF_StripeNum_t) node->params[2].v;
	unsigned priority = RF_EXTRACT_PRIORITY(node->params[3].v);
	unsigned lock = RF_EXTRACT_LOCK_FLAG(node->params[3].v);
	unsigned unlock = RF_EXTRACT_UNLOCK_FLAG(node->params[3].v);
	unsigned which_ru = RF_EXTRACT_RU(node->params[3].v);
	RF_DiskQueueDataFlags_t flags = 0;
	RF_IoType_t iotype = (node->dagHdr->status == rf_enable) ? RF_IO_TYPE_READ : RF_IO_TYPE_NOP;
	RF_DiskQueue_t **dqs = ((RF_Raid_t *) (node->dagHdr->raidPtr))->Queues;
	void   *b_proc = NULL;

#if defined(__NetBSD__)
	if (node->dagHdr->bp)
		b_proc = (void *) ((RF_Buf_t) node->dagHdr->bp)->b_proc;
#endif

	RF_ASSERT(!(lock && unlock));
	flags |= (lock) ? RF_LOCK_DISK_QUEUE : 0;
	flags |= (unlock) ? RF_UNLOCK_DISK_QUEUE : 0;

	req = rf_CreateDiskQueueData(iotype, pda->startSector, pda->numSector,
	    buf, parityStripeID, which_ru,
	    (int (*) (void *, int)) node->wakeFunc,
	    node, NULL, node->dagHdr->tracerec,
	    (void *) (node->dagHdr->raidPtr), flags, b_proc);
	if (!req) {
		(node->wakeFunc) (node, ENOMEM);
	} else {
		node->dagFuncData = (void *) req;
		rf_DiskIOEnqueue(&(dqs[pda->row][pda->col]), req, priority);
	}
	return (0);
}


/*****************************************************************************************
 * the execution function associated with a disk-write node
 ****************************************************************************************/
int 
rf_DiskWriteFuncForThreads(node)
	RF_DagNode_t *node;
{
	RF_DiskQueueData_t *req;
	RF_PhysDiskAddr_t *pda = (RF_PhysDiskAddr_t *) node->params[0].p;
	caddr_t buf = (caddr_t) node->params[1].p;
	RF_StripeNum_t parityStripeID = (RF_StripeNum_t) node->params[2].v;
	unsigned priority = RF_EXTRACT_PRIORITY(node->params[3].v);
	unsigned lock = RF_EXTRACT_LOCK_FLAG(node->params[3].v);
	unsigned unlock = RF_EXTRACT_UNLOCK_FLAG(node->params[3].v);
	unsigned which_ru = RF_EXTRACT_RU(node->params[3].v);
	RF_DiskQueueDataFlags_t flags = 0;
	RF_IoType_t iotype = (node->dagHdr->status == rf_enable) ? RF_IO_TYPE_WRITE : RF_IO_TYPE_NOP;
	RF_DiskQueue_t **dqs = ((RF_Raid_t *) (node->dagHdr->raidPtr))->Queues;
	void   *b_proc = NULL;

#if defined(__NetBSD__)
	if (node->dagHdr->bp)
		b_proc = (void *) ((RF_Buf_t) node->dagHdr->bp)->b_proc;
#endif

	/* normal processing (rollaway or forward recovery) begins here */
	RF_ASSERT(!(lock && unlock));
	flags |= (lock) ? RF_LOCK_DISK_QUEUE : 0;
	flags |= (unlock) ? RF_UNLOCK_DISK_QUEUE : 0;
	req = rf_CreateDiskQueueData(iotype, pda->startSector, pda->numSector,
	    buf, parityStripeID, which_ru,
	    (int (*) (void *, int)) node->wakeFunc,
	    (void *) node, NULL,
	    node->dagHdr->tracerec,
	    (void *) (node->dagHdr->raidPtr),
	    flags, b_proc);

	if (!req) {
		(node->wakeFunc) (node, ENOMEM);
	} else {
		node->dagFuncData = (void *) req;
		rf_DiskIOEnqueue(&(dqs[pda->row][pda->col]), req, priority);
	}

	return (0);
}
/*****************************************************************************************
 * the undo function for disk nodes
 * Note:  this is not a proper undo of a write node, only locks are released.
 *        old data is not restored to disk!
 ****************************************************************************************/
int 
rf_DiskUndoFunc(node)
	RF_DagNode_t *node;
{
	RF_DiskQueueData_t *req;
	RF_PhysDiskAddr_t *pda = (RF_PhysDiskAddr_t *) node->params[0].p;
	RF_DiskQueue_t **dqs = ((RF_Raid_t *) (node->dagHdr->raidPtr))->Queues;

	req = rf_CreateDiskQueueData(RF_IO_TYPE_NOP,
	    0L, 0, NULL, 0L, 0,
	    (int (*) (void *, int)) node->wakeFunc,
	    (void *) node,
	    NULL, node->dagHdr->tracerec,
	    (void *) (node->dagHdr->raidPtr),
	    RF_UNLOCK_DISK_QUEUE, NULL);
	if (!req)
		(node->wakeFunc) (node, ENOMEM);
	else {
		node->dagFuncData = (void *) req;
		rf_DiskIOEnqueue(&(dqs[pda->row][pda->col]), req, RF_IO_NORMAL_PRIORITY);
	}

	return (0);
}
/*****************************************************************************************
 * the execution function associated with an "unlock disk queue" node
 ****************************************************************************************/
int 
rf_DiskUnlockFuncForThreads(node)
	RF_DagNode_t *node;
{
	RF_DiskQueueData_t *req;
	RF_PhysDiskAddr_t *pda = (RF_PhysDiskAddr_t *) node->params[0].p;
	RF_DiskQueue_t **dqs = ((RF_Raid_t *) (node->dagHdr->raidPtr))->Queues;

	req = rf_CreateDiskQueueData(RF_IO_TYPE_NOP,
	    0L, 0, NULL, 0L, 0,
	    (int (*) (void *, int)) node->wakeFunc,
	    (void *) node,
	    NULL, node->dagHdr->tracerec,
	    (void *) (node->dagHdr->raidPtr),
	    RF_UNLOCK_DISK_QUEUE, NULL);
	if (!req)
		(node->wakeFunc) (node, ENOMEM);
	else {
		node->dagFuncData = (void *) req;
		rf_DiskIOEnqueue(&(dqs[pda->row][pda->col]), req, RF_IO_NORMAL_PRIORITY);
	}

	return (0);
}
/*****************************************************************************************
 * Callback routine for DiskRead and DiskWrite nodes.  When the disk op completes,
 * the routine is called to set the node status and inform the execution engine that
 * the node has fired.
 ****************************************************************************************/
int 
rf_GenericWakeupFunc(node, status)
	RF_DagNode_t *node;
	int     status;
{
	switch (node->status) {
	case rf_bwd1:
		node->status = rf_bwd2;
		if (node->dagFuncData)
			rf_FreeDiskQueueData((RF_DiskQueueData_t *) node->dagFuncData);
		return (rf_DiskWriteFuncForThreads(node));
		break;
	case rf_fired:
		if (status)
			node->status = rf_bad;
		else
			node->status = rf_good;
		break;
	case rf_recover:
		/* probably should never reach this case */
		if (status)
			node->status = rf_panic;
		else
			node->status = rf_undone;
		break;
	default:
		printf("rf_GenericWakeupFunc:");
		printf("node->status is %d,", node->status);
		printf("status is %d \n", status);
		RF_PANIC();
		break;
	}
	if (node->dagFuncData)
		rf_FreeDiskQueueData((RF_DiskQueueData_t *) node->dagFuncData);
	return (rf_FinishNode(node, RF_INTR_CONTEXT));
}


/*****************************************************************************************
 * there are three distinct types of xor nodes
 * A "regular xor" is used in the fault-free case where the access spans a complete
 * stripe unit.  It assumes that the result buffer is one full stripe unit in size,
 * and uses the stripe-unit-offset values that it computes from the PDAs to determine
 * where within the stripe unit to XOR each argument buffer.
 *
 * A "simple xor" is used in the fault-free case where the access touches only a portion
 * of one (or two, in some cases) stripe unit(s).  It assumes that all the argument
 * buffers are of the same size and have the same stripe unit offset.
 *
 * A "recovery xor" is used in the degraded-mode case.  It's similar to the regular
 * xor function except that it takes the failed PDA as an additional parameter, and
 * uses it to determine what portions of the argument buffers need to be xor'd into
 * the result buffer, and where in the result buffer they should go.
 ****************************************************************************************/

/* xor the params together and store the result in the result field.
 * assume the result field points to a buffer that is the size of one SU,
 * and use the pda params to determine where within the buffer to XOR
 * the input buffers.
 */
int 
rf_RegularXorFunc(node)
	RF_DagNode_t *node;
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[node->numParams - 1].p;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	RF_Etimer_t timer;
	int     i, retcode;

	retcode = 0;
	if (node->dagHdr->status == rf_enable) {
		/* don't do the XOR if the input is the same as the output */
		RF_ETIMER_START(timer);
		for (i = 0; i < node->numParams - 1; i += 2)
			if (node->params[i + 1].p != node->results[0]) {
				retcode = rf_XorIntoBuffer(raidPtr, (RF_PhysDiskAddr_t *) node->params[i].p,
				    (char *) node->params[i + 1].p, (char *) node->results[0], node->dagHdr->bp);
			}
		RF_ETIMER_STOP(timer);
		RF_ETIMER_EVAL(timer);
		tracerec->xor_us += RF_ETIMER_VAL_US(timer);
	}
	return (rf_GenericWakeupFunc(node, retcode));	/* call wake func
							 * explicitly since no
							 * I/O in this node */
}
/* xor the inputs into the result buffer, ignoring placement issues */
int 
rf_SimpleXorFunc(node)
	RF_DagNode_t *node;
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[node->numParams - 1].p;
	int     i, retcode = 0;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	RF_Etimer_t timer;

	if (node->dagHdr->status == rf_enable) {
		RF_ETIMER_START(timer);
		/* don't do the XOR if the input is the same as the output */
		for (i = 0; i < node->numParams - 1; i += 2)
			if (node->params[i + 1].p != node->results[0]) {
				retcode = rf_bxor((char *)node->params[i + 1].p,
				    (char *)node->results[0],
				    rf_RaidAddressToByte(raidPtr,
				    ((RF_PhysDiskAddr_t *)node->params[i].p)->
				    numSector), (RF_Buf_t)node->dagHdr->bp);
			}
		RF_ETIMER_STOP(timer);
		RF_ETIMER_EVAL(timer);
		tracerec->xor_us += RF_ETIMER_VAL_US(timer);
	}
	return (rf_GenericWakeupFunc(node, retcode));	/* call wake func
							 * explicitly since no
							 * I/O in this node */
}
/* this xor is used by the degraded-mode dag functions to recover lost data.
 * the second-to-last parameter is the PDA for the failed portion of the access.
 * the code here looks at this PDA and assumes that the xor target buffer is
 * equal in size to the number of sectors in the failed PDA.  It then uses
 * the other PDAs in the parameter list to determine where within the target
 * buffer the corresponding data should be xored.
 */
int 
rf_RecoveryXorFunc(node)
	RF_DagNode_t *node;
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[node->numParams - 1].p;
	RF_RaidLayout_t *layoutPtr = (RF_RaidLayout_t *) & raidPtr->Layout;
	RF_PhysDiskAddr_t *failedPDA = (RF_PhysDiskAddr_t *) node->params[node->numParams - 2].p;
	int     i, retcode = 0;
	RF_PhysDiskAddr_t *pda;
	int     suoffset, failedSUOffset = rf_StripeUnitOffset(layoutPtr, failedPDA->startSector);
	char   *srcbuf, *destbuf;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	RF_Etimer_t timer;

	if (node->dagHdr->status == rf_enable) {
		RF_ETIMER_START(timer);
		for (i = 0; i < node->numParams - 2; i += 2)
			if (node->params[i + 1].p != node->results[0]) {
				pda = (RF_PhysDiskAddr_t *) node->params[i].p;
				srcbuf = (char *) node->params[i + 1].p;
				suoffset = rf_StripeUnitOffset(layoutPtr, pda->startSector);
				destbuf = ((char *) node->results[0]) + rf_RaidAddressToByte(raidPtr, suoffset - failedSUOffset);
				retcode = rf_bxor(srcbuf, destbuf, rf_RaidAddressToByte(raidPtr, pda->numSector), node->dagHdr->bp);
			}
		RF_ETIMER_STOP(timer);
		RF_ETIMER_EVAL(timer);
		tracerec->xor_us += RF_ETIMER_VAL_US(timer);
	}
	return (rf_GenericWakeupFunc(node, retcode));
}
/*****************************************************************************************
 * The next three functions are utilities used by the above xor-execution functions.
 ****************************************************************************************/


/*
 * this is just a glorified buffer xor.  targbuf points to a buffer that is one full stripe unit
 * in size.  srcbuf points to a buffer that may be less than 1 SU, but never more.  When the
 * access described by pda is one SU in size (which by implication means it's SU-aligned),
 * all that happens is (targbuf) <- (srcbuf ^ targbuf).  When the access is less than one
 * SU in size the XOR occurs on only the portion of targbuf identified in the pda.
 */

int 
rf_XorIntoBuffer(raidPtr, pda, srcbuf, targbuf, bp)
	RF_Raid_t *raidPtr;
	RF_PhysDiskAddr_t *pda;
	char   *srcbuf;
	char   *targbuf;
	void   *bp;
{
	char   *targptr;
	int     sectPerSU = raidPtr->Layout.sectorsPerStripeUnit;
	int     SUOffset = pda->startSector % sectPerSU;
	int     length, retcode = 0;

	RF_ASSERT(pda->numSector <= sectPerSU);

	targptr = targbuf + rf_RaidAddressToByte(raidPtr, SUOffset);
	length = rf_RaidAddressToByte(raidPtr, pda->numSector);
	retcode = rf_bxor(srcbuf, targptr, length, bp);
	return (retcode);
}
/* it really should be the case that the buffer pointers (returned by malloc)
 * are aligned to the natural word size of the machine, so this is the only
 * case we optimize for.  The length should always be a multiple of the sector
 * size, so there should be no problem with leftover bytes at the end.
 */
int 
rf_bxor(src, dest, len, bp)
	char   *src;
	char   *dest;
	int     len;
	void   *bp;
{
	unsigned mask = sizeof(long) - 1, retcode = 0;

	if (!(((unsigned long) src) & mask) && !(((unsigned long) dest) & mask) && !(len & mask)) {
		retcode = rf_longword_bxor((unsigned long *) src, (unsigned long *) dest, len >> RF_LONGSHIFT, bp);
	} else {
		RF_ASSERT(0);
	}
	return (retcode);
}
/* map a user buffer into kernel space, if necessary */
#define REMAP_VA(_bp,x,y) (y) = (x)

/* When XORing in kernel mode, we need to map each user page to kernel space before we can access it.
 * We don't want to assume anything about which input buffers are in kernel/user
 * space, nor about their alignment, so in each loop we compute the maximum number
 * of bytes that we can xor without crossing any page boundaries, and do only this many
 * bytes before the next remap.
 */
int 
rf_longword_bxor(src, dest, len, bp)
	unsigned long *src;
	unsigned long *dest;
	int     len;		/* longwords */
	void   *bp;
{
	unsigned long *end = src + len;
	unsigned long d0, d1, d2, d3, s0, s1, s2, s3;	/* temps */
	unsigned long *pg_src, *pg_dest;	/* per-page source/dest
							 * pointers */
	int     longs_this_time;/* # longwords to xor in the current iteration */

	REMAP_VA(bp, src, pg_src);
	REMAP_VA(bp, dest, pg_dest);
	if (!pg_src || !pg_dest)
		return (EFAULT);

	while (len >= 4) {
		longs_this_time = RF_MIN(len, RF_MIN(RF_BLIP(pg_src), RF_BLIP(pg_dest)) >> RF_LONGSHIFT);	/* note len in longwords */
		src += longs_this_time;
		dest += longs_this_time;
		len -= longs_this_time;
		while (longs_this_time >= 4) {
			d0 = pg_dest[0];
			d1 = pg_dest[1];
			d2 = pg_dest[2];
			d3 = pg_dest[3];
			s0 = pg_src[0];
			s1 = pg_src[1];
			s2 = pg_src[2];
			s3 = pg_src[3];
			pg_dest[0] = d0 ^ s0;
			pg_dest[1] = d1 ^ s1;
			pg_dest[2] = d2 ^ s2;
			pg_dest[3] = d3 ^ s3;
			pg_src += 4;
			pg_dest += 4;
			longs_this_time -= 4;
		}
		while (longs_this_time > 0) {	/* cannot cross any page
						 * boundaries here */
			*pg_dest++ ^= *pg_src++;
			longs_this_time--;
		}

		/* either we're done, or we've reached a page boundary on one
		 * (or possibly both) of the pointers */
		if (len) {
			if (RF_PAGE_ALIGNED(src))
				REMAP_VA(bp, src, pg_src);
			if (RF_PAGE_ALIGNED(dest))
				REMAP_VA(bp, dest, pg_dest);
			if (!pg_src || !pg_dest)
				return (EFAULT);
		}
	}
	while (src < end) {
		*pg_dest++ ^= *pg_src++;
		src++;
		dest++;
		len--;
		if (RF_PAGE_ALIGNED(src))
			REMAP_VA(bp, src, pg_src);
		if (RF_PAGE_ALIGNED(dest))
			REMAP_VA(bp, dest, pg_dest);
	}
	RF_ASSERT(len == 0);
	return (0);
}


/*
   dst = a ^ b ^ c;
   a may equal dst
   see comment above longword_bxor
*/
int 
rf_longword_bxor3(dst, a, b, c, len, bp)
	unsigned long *dst;
	unsigned long *a;
	unsigned long *b;
	unsigned long *c;
	int     len;		/* length in longwords */
	void   *bp;
{
	unsigned long a0, a1, a2, a3, b0, b1, b2, b3;
	unsigned long *pg_a, *pg_b, *pg_c, *pg_dst;	/* per-page source/dest
								 * pointers */
	int     longs_this_time;/* # longs to xor in the current iteration */
	char    dst_is_a = 0;

	REMAP_VA(bp, a, pg_a);
	REMAP_VA(bp, b, pg_b);
	REMAP_VA(bp, c, pg_c);
	if (a == dst) {
		pg_dst = pg_a;
		dst_is_a = 1;
	} else {
		REMAP_VA(bp, dst, pg_dst);
	}

	/* align dest to cache line.  Can't cross a pg boundary on dst here. */
	while ((((unsigned long) pg_dst) & 0x1f)) {
		*pg_dst++ = *pg_a++ ^ *pg_b++ ^ *pg_c++;
		dst++;
		a++;
		b++;
		c++;
		if (RF_PAGE_ALIGNED(a)) {
			REMAP_VA(bp, a, pg_a);
			if (!pg_a)
				return (EFAULT);
		}
		if (RF_PAGE_ALIGNED(b)) {
			REMAP_VA(bp, a, pg_b);
			if (!pg_b)
				return (EFAULT);
		}
		if (RF_PAGE_ALIGNED(c)) {
			REMAP_VA(bp, a, pg_c);
			if (!pg_c)
				return (EFAULT);
		}
		len--;
	}

	while (len > 4) {
		longs_this_time = RF_MIN(len, RF_MIN(RF_BLIP(a), RF_MIN(RF_BLIP(b), RF_MIN(RF_BLIP(c), RF_BLIP(dst)))) >> RF_LONGSHIFT);
		a += longs_this_time;
		b += longs_this_time;
		c += longs_this_time;
		dst += longs_this_time;
		len -= longs_this_time;
		while (longs_this_time >= 4) {
			a0 = pg_a[0];
			longs_this_time -= 4;

			a1 = pg_a[1];
			a2 = pg_a[2];

			a3 = pg_a[3];
			pg_a += 4;

			b0 = pg_b[0];
			b1 = pg_b[1];

			b2 = pg_b[2];
			b3 = pg_b[3];
			/* start dual issue */
			a0 ^= b0;
			b0 = pg_c[0];

			pg_b += 4;
			a1 ^= b1;

			a2 ^= b2;
			a3 ^= b3;

			b1 = pg_c[1];
			a0 ^= b0;

			b2 = pg_c[2];
			a1 ^= b1;

			b3 = pg_c[3];
			a2 ^= b2;

			pg_dst[0] = a0;
			a3 ^= b3;
			pg_dst[1] = a1;
			pg_c += 4;
			pg_dst[2] = a2;
			pg_dst[3] = a3;
			pg_dst += 4;
		}
		while (longs_this_time > 0) {	/* cannot cross any page
						 * boundaries here */
			*pg_dst++ = *pg_a++ ^ *pg_b++ ^ *pg_c++;
			longs_this_time--;
		}

		if (len) {
			if (RF_PAGE_ALIGNED(a)) {
				REMAP_VA(bp, a, pg_a);
				if (!pg_a)
					return (EFAULT);
				if (dst_is_a)
					pg_dst = pg_a;
			}
			if (RF_PAGE_ALIGNED(b)) {
				REMAP_VA(bp, b, pg_b);
				if (!pg_b)
					return (EFAULT);
			}
			if (RF_PAGE_ALIGNED(c)) {
				REMAP_VA(bp, c, pg_c);
				if (!pg_c)
					return (EFAULT);
			}
			if (!dst_is_a)
				if (RF_PAGE_ALIGNED(dst)) {
					REMAP_VA(bp, dst, pg_dst);
					if (!pg_dst)
						return (EFAULT);
				}
		}
	}
	while (len) {
		*pg_dst++ = *pg_a++ ^ *pg_b++ ^ *pg_c++;
		dst++;
		a++;
		b++;
		c++;
		if (RF_PAGE_ALIGNED(a)) {
			REMAP_VA(bp, a, pg_a);
			if (!pg_a)
				return (EFAULT);
			if (dst_is_a)
				pg_dst = pg_a;
		}
		if (RF_PAGE_ALIGNED(b)) {
			REMAP_VA(bp, b, pg_b);
			if (!pg_b)
				return (EFAULT);
		}
		if (RF_PAGE_ALIGNED(c)) {
			REMAP_VA(bp, c, pg_c);
			if (!pg_c)
				return (EFAULT);
		}
		if (!dst_is_a)
			if (RF_PAGE_ALIGNED(dst)) {
				REMAP_VA(bp, dst, pg_dst);
				if (!pg_dst)
					return (EFAULT);
			}
		len--;
	}
	return (0);
}

int 
rf_bxor3(dst, a, b, c, len, bp)
	unsigned char *dst;
	unsigned char *a;
	unsigned char *b;
	unsigned char *c;
	unsigned long len;
	void   *bp;
{
	RF_ASSERT(((RF_UL(dst) | RF_UL(a) | RF_UL(b) | RF_UL(c) | len) & 0x7) == 0);

	return (rf_longword_bxor3((unsigned long *) dst, (unsigned long *) a,
		(unsigned long *) b, (unsigned long *) c, len >> RF_LONGSHIFT, bp));
}
