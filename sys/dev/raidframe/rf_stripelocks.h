/*	$FreeBSD$ */
/*	$NetBSD: rf_stripelocks.h,v 1.3 1999/02/05 00:06:18 oster Exp $	*/
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

/*****************************************************************************
 *
 * stripelocks.h -- header file for locking stripes
 *
 * Note that these functions are called from the execution routines of certain
 * DAG Nodes, and so they must be NON-BLOCKING to assure maximum parallelism
 * in the DAG.  Accordingly, when a node wants to acquire a lock, it calls
 * AcquireStripeLock, supplying a pointer to a callback function.  If the lock
 * is free at the time of the call, 0 is returned, indicating that the lock
 * has been acquired.  If the lock is not free, 1 is returned, and a copy of
 * the function pointer and argument are held in the lock table.  When the
 * lock becomes free, the callback function is invoked.
 *
 *****************************************************************************/

#ifndef _RF__RF_STRIPELOCKS_H_
#define _RF__RF_STRIPELOCKS_H_

#if defined(__FreeBSD__)
#include <sys/types.h>
#if __FreeBSD_version > 500005
#include <sys/bio.h>
#endif
#if _KERNEL
#include <sys/systm.h>
#endif
#endif
#include <sys/buf.h>

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_threadstuff.h>
#include <dev/raidframe/rf_general.h>

struct RF_LockReqDesc_s {
	RF_IoType_t type;	/* read or write */
	RF_int64 start, stop;	/* start and end of range to be locked */
	RF_int64 start2, stop2;	/* start and end of 2nd range to be locked */
	void    (*cbFunc) (RF_Buf_t);	/* callback function */
	void   *cbArg;		/* argument to callback function */
	RF_LockReqDesc_t *next;	/* next element in chain */
	RF_LockReqDesc_t *templink;	/* for making short-lived lists of
					 * request descriptors */
};
#define RF_ASSERT_VALID_LOCKREQ(_lr_) { \
	RF_ASSERT(RF_IO_IS_R_OR_W((_lr_)->type)); \
}

struct RF_StripeLockDesc_s {
	RF_StripeNum_t stripeID;/* the stripe ID */
	RF_LockReqDesc_t *granted;	/* unordered list of granted requests */
	RF_LockReqDesc_t *waitersH;	/* FIFO queue of all waiting reqs,
					 * both read and write (Head and Tail) */
	RF_LockReqDesc_t *waitersT;
	int     nWriters;	/* number of writers either granted or waiting */
	RF_StripeLockDesc_t *next;	/* for hash table collision resolution */
};

struct RF_LockTableEntry_s {
	RF_DECLARE_MUTEX(mutex)	/* mutex on this hash chain */
	RF_StripeLockDesc_t *descList;	/* hash chain of lock descriptors */
};
/*
 * Initializes a stripe lock descriptor.  _defSize is the number of sectors
 * that we lock when there is no parity information in the ASM (e.g. RAID0).
 */

#define RF_INIT_LOCK_REQ_DESC(_lrd, _typ, _cbf, _cba, _asm, _defSize)                                           \
  {                                                                                                          \
    (_lrd).type    = _typ;                                                                                   \
    (_lrd).start2  = -1;                                                                                     \
    (_lrd).stop2   = -1;                                                                                     \
    if ((_asm)->parityInfo) {                                                                                \
      (_lrd).start = (_asm)->parityInfo->startSector;                                                        \
      (_lrd).stop  = (_asm)->parityInfo->startSector + (_asm)->parityInfo->numSector-1;                      \
      if ((_asm)->parityInfo->next) {                                                                        \
        (_lrd).start2  = (_asm)->parityInfo->next->startSector;                                              \
        (_lrd).stop2   = (_asm)->parityInfo->next->startSector + (_asm)->parityInfo->next->numSector-1;      \
      }                                                                                                      \
    } else {                                                                                                 \
      (_lrd).start = 0;                                                                                      \
      (_lrd).stop  = (_defSize);                                                                             \
    }													     \
    (_lrd).templink= NULL;                                                                                   \
    (_lrd).cbFunc  = (_cbf);                                                                                 \
    (_lrd).cbArg   = (void *) (_cba);                                                                        \
  }

int     rf_ConfigureStripeLockFreeList(RF_ShutdownList_t ** listp);
RF_LockTableEntry_t *rf_MakeLockTable(void);
void    rf_ShutdownStripeLocks(RF_LockTableEntry_t * lockTable);
int 
rf_ConfigureStripeLocks(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr);
int 
rf_AcquireStripeLock(RF_LockTableEntry_t * lockTable,
    RF_StripeNum_t stripeID, RF_LockReqDesc_t * lockReqDesc);
void 
rf_ReleaseStripeLock(RF_LockTableEntry_t * lockTable,
    RF_StripeNum_t stripeID, RF_LockReqDesc_t * lockReqDesc);

#endif				/* !_RF__RF_STRIPELOCKS_H_ */
