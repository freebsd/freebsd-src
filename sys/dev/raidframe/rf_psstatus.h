/*	$FreeBSD$ */
/*	$NetBSD: rf_psstatus.h,v 1.3 1999/02/05 00:06:15 oster Exp $	*/
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
 * psstatus.h
 *
 * The reconstruction code maintains a bunch of status related to the parity
 * stripes that are currently under reconstruction.  This header file defines
 * the status structures.
 *
 *****************************************************************************/

#ifndef _RF__RF_PSSTATUS_H_
#define _RF__RF_PSSTATUS_H_

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_threadstuff.h>
#include <dev/raidframe/rf_callback.h>

#define RF_PS_MAX_BUFS 10	/* max number of bufs we'll accumulate before
				 * we do an XOR */

#define RF_PSS_DEFAULT_TABLESIZE 200

/*
 * Macros to acquire/release the mutex lock on a parity stripe status
 * descriptor. Note that we use just one lock for the whole hash chain.
 */
#define RF_HASH_PSID(_raid_,_psid_) ( (_psid_) % ((_raid_)->pssTableSize) )	/* simple hash function */
#define RF_LOCK_PSS_MUTEX(_raidPtr, _row, _psid) \
  RF_LOCK_MUTEX((_raidPtr)->reconControl[_row]->pssTable[ RF_HASH_PSID(_raidPtr,_psid) ].mutex)
#define RF_UNLOCK_PSS_MUTEX(_raidPtr, _row, _psid) \
  RF_UNLOCK_MUTEX((_raidPtr)->reconControl[_row]->pssTable[ RF_HASH_PSID(_raidPtr,_psid) ].mutex)

struct RF_ReconParityStripeStatus_s {
	RF_StripeNum_t parityStripeID;	/* the parity stripe ID */
	RF_ReconUnitNum_t which_ru;	/* which reconstruction unit with the
					 * indicated parity stripe */
	RF_PSSFlags_t flags;	/* flags indicating various conditions */
	void   *rbuf;		/* this is the accumulating xor sum */
	void   *writeRbuf;	/* DEBUG ONLY:  a pointer to the rbuf after it
				 * has filled & been sent to disk */
	void   *rbufsForXor[RF_PS_MAX_BUFS];	/* these are buffers still to
						 * be xored into the
						 * accumulating sum */
	int     xorBufCount;	/* num buffers waiting to be xored */
	int     blockCount;	/* count of # proc that have blocked recon on
				 * this parity stripe */
	char   *issued;		/* issued[i]==1 <=> column i has already
				 * issued a read request for the indicated RU */
	RF_CallbackDesc_t *procWaitList;	/* list of user procs waiting
						 * for recon to be done */
	RF_CallbackDesc_t *blockWaitList;	/* list of disks blocked
						 * waiting for user write to
						 * complete */
	RF_CallbackDesc_t *bufWaitList;	/* list of disks blocked waiting to
					 * acquire a buffer for this RU */
	RF_ReconParityStripeStatus_t *next;
};

struct RF_PSStatusHeader_s {
	RF_DECLARE_MUTEX(mutex)	/* mutex for this hash chain */
	RF_ReconParityStripeStatus_t *chain;	/* the hash chain */
};
/* masks for the "flags" field above */
#define RF_PSS_NONE            0x00000000	/* no flags */
#define RF_PSS_UNDER_RECON     0x00000001	/* this parity stripe is
						 * currently under
						 * reconstruction */
#define RF_PSS_FORCED_ON_WRITE 0x00000002	/* indicates a recon was
						 * forced due to a user-write
						 * operation */
#define RF_PSS_FORCED_ON_READ  0x00000004	/* ditto for read, but not
						 * currently implemented */
#define RF_PSS_RECON_BLOCKED   0x00000008	/* reconstruction is currently
						 * blocked due to a pending
						 * user I/O */
#define RF_PSS_CREATE          0x00000010	/* tells LookupRUStatus to
						 * create the entry */
#define RF_PSS_BUFFERWAIT      0x00000020	/* someone is waiting for a
						 * buffer for this RU */

int 
rf_ConfigurePSStatus(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr);

RF_PSStatusHeader_t *rf_MakeParityStripeStatusTable(RF_Raid_t * raidPtr);
void 
rf_FreeParityStripeStatusTable(RF_Raid_t * raidPtr,
    RF_PSStatusHeader_t * pssTable);
RF_ReconParityStripeStatus_t *
rf_LookupRUStatus(RF_Raid_t * raidPtr,
    RF_PSStatusHeader_t * pssTable, RF_StripeNum_t psID,
    RF_ReconUnitNum_t which_ru, RF_PSSFlags_t flags, int *created);
void 
rf_PSStatusDelete(RF_Raid_t * raidPtr, RF_PSStatusHeader_t * pssTable,
    RF_ReconParityStripeStatus_t * pssPtr);
void 
rf_RemoveFromActiveReconTable(RF_Raid_t * raidPtr, RF_RowCol_t row,
    RF_StripeNum_t psid, RF_ReconUnitNum_t which_ru);
RF_ReconParityStripeStatus_t *rf_AllocPSStatus(RF_Raid_t * raidPtr);
void    rf_FreePSStatus(RF_Raid_t * raidPtr, RF_ReconParityStripeStatus_t * p);
void    rf_PrintPSStatusTable(RF_Raid_t * raidPtr, RF_RowCol_t row);

#endif				/* !_RF__RF_PSSTATUS_H_ */
