/*	$FreeBSD$ */
/*	$NetBSD: rf_parityscan.h,v 1.3 1999/02/05 00:06:14 oster Exp $	*/
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

#ifndef _RF__RF_PARITYSCAN_H_
#define _RF__RF_PARITYSCAN_H_

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_alloclist.h>

int     rf_RewriteParity(RF_Raid_t * raidPtr);
int 
rf_VerifyParityBasic(RF_Raid_t * raidPtr, RF_RaidAddr_t raidAddr,
    RF_PhysDiskAddr_t * parityPDA, int correct_it, RF_RaidAccessFlags_t flags);
int 
rf_VerifyParity(RF_Raid_t * raidPtr, RF_AccessStripeMap_t * stripeMap,
    int correct_it, RF_RaidAccessFlags_t flags);
int     rf_TryToRedirectPDA(RF_Raid_t * raidPtr, RF_PhysDiskAddr_t * pda, int parity);
int     rf_VerifyDegrModeWrite(RF_Raid_t * raidPtr, RF_AccessStripeMapHeader_t * asmh);
RF_DagHeader_t *
rf_MakeSimpleDAG(RF_Raid_t * raidPtr, int nNodes,
    int bytesPerSU, char *databuf,
    int (*doFunc) (RF_DagNode_t *),
    int (*undoFunc) (RF_DagNode_t *),
    char *name, RF_AllocListElem_t * alloclist,
    RF_RaidAccessFlags_t flags, int priority);

#define RF_DO_CORRECT_PARITY   1
#define RF_DONT_CORRECT_PARITY 0

/*
 * Return vals for VerifyParity operation
 *
 * Ordering is important here.
 */
#define RF_PARITY_OKAY               0	/* or no parity information */
#define RF_PARITY_CORRECTED          1
#define RF_PARITY_BAD                2
#define RF_PARITY_COULD_NOT_CORRECT  3
#define RF_PARITY_COULD_NOT_VERIFY   4

#endif				/* !_RF__RF_PARITYSCAN_H_ */
