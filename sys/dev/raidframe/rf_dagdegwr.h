/*	$FreeBSD$ */
/*	$NetBSD: rf_dagdegwr.h,v 1.4 1999/08/15 02:36:03 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, Daniel Stodolsky, William V. Courtright II
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


#ifndef _RF__RF_DAGDEGWR_H_
#define _RF__RF_DAGDEGWR_H_

/* degraded write DAG creation routines */
void rf_CreateDegradedWriteDAG(RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h, void *bp,
    RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList);

void rf_CommonCreateSimpleDegradedWriteDAG(RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h, void *bp,
    RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList,
    int nfaults, int (*redFunc) (RF_DagNode_t *), int allowBufferRecycle);

void rf_WriteGenerateFailedAccessASMs(RF_Raid_t * raidPtr,
      RF_AccessStripeMap_t * asmap, RF_PhysDiskAddr_t ** pdap,
      int *nNodep, RF_PhysDiskAddr_t ** pqpdap,
      int *nPQNodep, RF_AllocListElem_t * allocList);

void rf_DoubleDegSmallWrite(RF_Raid_t * raidPtr, RF_AccessStripeMap_t * asmap,
            RF_DagHeader_t * dag_h, void *bp, RF_RaidAccessFlags_t flags,
            RF_AllocListElem_t * allocList, char *redundantReadNodeName,
            char *redundantWriteNodeName, char *recoveryNodeName,
            int (*recovFunc) (RF_DagNode_t *));

#endif				/* !_RF__RF_DAGDEGWR_H_ */
