/*	$FreeBSD$ */
/*	$NetBSD: rf_dagffwr.h,v 1.3 1999/02/05 00:06:08 oster Exp $	*/
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

#ifndef _RF__RF_DAGFFWR_H_
#define _RF__RF_DAGFFWR_H_

#include <dev/raidframe/rf_types.h>

/* fault-free write DAG creation routines */
void 
rf_CreateNonRedundantWriteDAG(RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h, void *bp,
    RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList,
    RF_IoType_t type);
void 
rf_CreateRAID0WriteDAG(RF_Raid_t * raidPtr, RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h, void *bp, RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList, RF_IoType_t type);
void 
rf_CreateSmallWriteDAG(RF_Raid_t * raidPtr, RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h, void *bp, RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList);
void 
rf_CreateLargeWriteDAG(RF_Raid_t * raidPtr, RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h, void *bp, RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList);
void 
rf_CommonCreateLargeWriteDAG(RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h, void *bp,
    RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList, int nfaults,
    int (*redFunc) (RF_DagNode_t *), int allowBufferRecycle);
	void    rf_CommonCreateLargeWriteDAGFwd(RF_Raid_t * raidPtr,
            RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h, void *bp,
            RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList, int nfaults,
            int (*redFunc) (RF_DagNode_t *), int allowBufferRecycle);
	void    rf_CommonCreateSmallWriteDAG(RF_Raid_t * raidPtr,
            RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h, void *bp,
            RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList,
            RF_RedFuncs_t * pfuncs, RF_RedFuncs_t * qfuncs);
	void    rf_CommonCreateSmallWriteDAGFwd(RF_Raid_t * raidPtr,
            RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h, void *bp,
            RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList,
            RF_RedFuncs_t * pfuncs, RF_RedFuncs_t * qfuncs);
	void    rf_CreateRaidOneWriteDAG(RF_Raid_t * raidPtr, RF_AccessStripeMap_t * asmap,
            RF_DagHeader_t * dag_h, void *bp, RF_RaidAccessFlags_t flags,
            RF_AllocListElem_t * allocList);
	void    rf_CreateRaidOneWriteDAGFwd(RF_Raid_t * raidPtr,
            RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h, void *bp,
            RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList);

#endif				/* !_RF__RF_DAGFFWR_H_ */
