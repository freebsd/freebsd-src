/*	$FreeBSD$ */
/*	$NetBSD: rf_dagdegrd.h,v 1.3 1999/02/05 00:06:07 oster Exp $	*/
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

#ifndef _RF__RF_DAGDEGRD_H_
#define _RF__RF_DAGDEGRD_H_

#include <dev/raidframe/rf_types.h>

/* degraded read DAG creation routines */
void 
rf_CreateRaidFiveDegradedReadDAG(RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h, void *bp,
    RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList);
void 
rf_CreateRaidOneDegradedReadDAG(RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h, void *bp,
    RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList);
void 
rf_CreateDegradedReadDAG(RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h, void *bp,
    RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList,
    RF_RedFuncs_t * recFunc);
void 
rf_CreateRaidCDegradedReadDAG(RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h, void *bp,
    RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList);
void 
rf_DD_GenerateFailedAccessASMs(RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap, RF_PhysDiskAddr_t ** pdap,
    int *nNodep, RF_PhysDiskAddr_t ** pqpdap, int *nPQNodep,
    RF_AllocListElem_t * allocList);
void 
rf_DoubleDegRead(RF_Raid_t * raidPtr, RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h, void *bp, RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList, char *redundantReadNodeName,
    char *recoveryNodeName, int (*recovFunc) (RF_DagNode_t *));

#endif				/* !_RF__RF_DAGDEGRD_H_ */
