/*	$FreeBSD$ */
/*	$NetBSD: rf_parityloggingdags.h,v 1.3 1999/02/05 00:06:14 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: William V. Courtright II
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
 *                                                                          *
 * rf_parityloggingdags.h -- header file for parity logging dags            *
 *                                                                          *
 ****************************************************************************/

#ifndef _RF__RF_PARITYLOGGINGDAGS_H_
#define _RF__RF_PARITYLOGGINGDAGS_H_

/* routines that create DAGs */
void 
rf_CommonCreateParityLoggingLargeWriteDAG(RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h,
    void *bp, RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList,
    int nfaults, int (*redFunc) (RF_DagNode_t *));
	void    rf_CommonCreateParityLoggingSmallWriteDAG(RF_Raid_t * raidPtr,
            RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h,
            void *bp, RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList,
            RF_RedFuncs_t * pfuncs, RF_RedFuncs_t * qfuncs);

	void    rf_CreateParityLoggingLargeWriteDAG(RF_Raid_t * raidPtr,
            RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h,
            void *bp, RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList,
            int nfaults, int (*redFunc) (RF_DagNode_t *));
	void    rf_CreateParityLoggingSmallWriteDAG(RF_Raid_t * raidPtr,
            RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h,
            void *bp, RF_RaidAccessFlags_t flags, RF_AllocListElem_t * allocList,
            RF_RedFuncs_t * pfuncs, RF_RedFuncs_t * qfuncs);

#endif				/* !_RF__RF_PARITYLOGGINGDAGS_H_ */
