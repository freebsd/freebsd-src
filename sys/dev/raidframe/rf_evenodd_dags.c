/*	$FreeBSD$ */
/*	$NetBSD: rf_evenodd_dags.c,v 1.2 1999/02/05 00:06:11 oster Exp $	*/
/*
 * rf_evenodd_dags.c
 */
/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chang-Ming Wu
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

#include <dev/raidframe/rf_archs.h>

#if RF_INCLUDE_EVENODD > 0

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_dagfuncs.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_etimer.h>
#include <dev/raidframe/rf_acctrace.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_evenodd_dags.h>
#include <dev/raidframe/rf_evenodd.h>
#include <dev/raidframe/rf_evenodd_dagfuncs.h>
#include <dev/raidframe/rf_pq.h>
#include <dev/raidframe/rf_dagdegrd.h>
#include <dev/raidframe/rf_dagdegwr.h>
#include <dev/raidframe/rf_dagffwr.h>


/*
 * Lost one data.
 * Use P to reconstruct missing data.
 */
RF_CREATE_DAG_FUNC_DECL(rf_EO_100_CreateReadDAG)
{
	rf_CreateDegradedReadDAG(raidPtr, asmap, dag_h, bp, flags, allocList, &rf_eoPRecoveryFuncs);
}
/*
 * Lost data + E.
 * Use P to reconstruct missing data.
 */
RF_CREATE_DAG_FUNC_DECL(rf_EO_101_CreateReadDAG)
{
	rf_CreateDegradedReadDAG(raidPtr, asmap, dag_h, bp, flags, allocList, &rf_eoPRecoveryFuncs);
}
/*
 * Lost data + P.
 * Make E look like P, and use Eor for Xor, and we can
 * use degraded read DAG.
 */
RF_CREATE_DAG_FUNC_DECL(rf_EO_110_CreateReadDAG)
{
	RF_PhysDiskAddr_t *temp;
	/* swap P and E pointers to fake out the DegradedReadDAG code */
	temp = asmap->parityInfo;
	asmap->parityInfo = asmap->qInfo;
	asmap->qInfo = temp;
	rf_CreateDegradedReadDAG(raidPtr, asmap, dag_h, bp, flags, allocList, &rf_eoERecoveryFuncs);
}
/*
 * Lost two data.
 */
RF_CREATE_DAG_FUNC_DECL(rf_EOCreateDoubleDegradedReadDAG)
{
	rf_EO_DoubleDegRead(raidPtr, asmap, dag_h, bp, flags, allocList);
}
/*
 * Lost two data.
 */
RF_CREATE_DAG_FUNC_DECL(rf_EO_200_CreateReadDAG)
{
	rf_EOCreateDoubleDegradedReadDAG(raidPtr, asmap, dag_h, bp, flags, allocList);
}
RF_CREATE_DAG_FUNC_DECL(rf_EO_100_CreateWriteDAG)
{
	if (asmap->numStripeUnitsAccessed != 1 &&
	    asmap->failedPDAs[0]->numSector != raidPtr->Layout.sectorsPerStripeUnit)
		RF_PANIC();
	rf_CommonCreateSimpleDegradedWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList, 2, (int (*) (RF_DagNode_t *)) rf_Degraded_100_EOFunc, RF_TRUE);
}
/*
 * E is dead. Small write.
 */
RF_CREATE_DAG_FUNC_DECL(rf_EO_001_CreateSmallWriteDAG)
{
	rf_CommonCreateSmallWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList, &rf_EOSmallWritePFuncs, NULL);
}
/*
 * E is dead. Large write.
 */
RF_CREATE_DAG_FUNC_DECL(rf_EO_001_CreateLargeWriteDAG)
{
	rf_CommonCreateLargeWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList, 1, rf_RegularPFunc, RF_TRUE);
}
/*
 * P is dead. Small write.
 * Swap E + P, use single-degraded stuff.
 */
RF_CREATE_DAG_FUNC_DECL(rf_EO_010_CreateSmallWriteDAG)
{
	RF_PhysDiskAddr_t *temp;
	/* swap P and E pointers to fake out the DegradedReadDAG code */
	temp = asmap->parityInfo;
	asmap->parityInfo = asmap->qInfo;
	asmap->qInfo = temp;
	rf_CommonCreateSmallWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList, &rf_EOSmallWriteEFuncs, NULL);
}
/*
 * P is dead. Large write.
 * Swap E + P, use single-degraded stuff.
 */
RF_CREATE_DAG_FUNC_DECL(rf_EO_010_CreateLargeWriteDAG)
{
	RF_PhysDiskAddr_t *temp;
	/* swap P and E pointers to fake out the code */
	temp = asmap->parityInfo;
	asmap->parityInfo = asmap->qInfo;
	asmap->qInfo = temp;
	rf_CommonCreateLargeWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList, 1, rf_RegularEFunc, RF_FALSE);
}
RF_CREATE_DAG_FUNC_DECL(rf_EO_011_CreateWriteDAG)
{
	rf_CreateNonRedundantWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList,
	    RF_IO_TYPE_WRITE);
}
RF_CREATE_DAG_FUNC_DECL(rf_EO_110_CreateWriteDAG)
{
	RF_PhysDiskAddr_t *temp;

	if (asmap->numStripeUnitsAccessed != 1 &&
	    asmap->failedPDAs[0]->numSector != raidPtr->Layout.sectorsPerStripeUnit) {
		RF_PANIC();
	}
	/* swap P and E to fake out parity code */
	temp = asmap->parityInfo;
	asmap->parityInfo = asmap->qInfo;
	asmap->qInfo = temp;
	rf_CommonCreateSimpleDegradedWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList, 1, (int (*) (RF_DagNode_t *)) rf_EO_DegradedWriteEFunc, RF_FALSE);
	/* is the regular E func the right one to call? */
}
RF_CREATE_DAG_FUNC_DECL(rf_EO_101_CreateWriteDAG)
{
	if (asmap->numStripeUnitsAccessed != 1 &&
	    asmap->failedPDAs[0]->numSector != raidPtr->Layout.sectorsPerStripeUnit)
		RF_PANIC();
	rf_CommonCreateSimpleDegradedWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList, 1, rf_RecoveryXorFunc, RF_TRUE);
}
RF_CREATE_DAG_FUNC_DECL(rf_EO_DoubleDegRead)
{
	rf_DoubleDegRead(raidPtr, asmap, dag_h, bp, flags, allocList,
	    "Re", "EvenOddRecovery", rf_EvenOddDoubleRecoveryFunc);
}
RF_CREATE_DAG_FUNC_DECL(rf_EOCreateSmallWriteDAG)
{
	rf_CommonCreateSmallWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList, &rf_pFuncs, &rf_EOSmallWriteEFuncs);
}
RF_CREATE_DAG_FUNC_DECL(rf_EOCreateLargeWriteDAG)
{
	rf_CommonCreateLargeWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList, 2, rf_RegularPEFunc, RF_FALSE);
}
RF_CREATE_DAG_FUNC_DECL(rf_EO_200_CreateWriteDAG)
{
	rf_DoubleDegSmallWrite(raidPtr, asmap, dag_h, bp, flags, allocList, "Re", "We", "EOWrDDRecovery", rf_EOWriteDoubleRecoveryFunc);
}
#endif				/* RF_INCLUDE_EVENODD > 0 */
