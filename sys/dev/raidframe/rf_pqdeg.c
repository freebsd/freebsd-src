/*	$NetBSD: rf_pqdeg.c,v 1.5 2000/01/07 03:41:04 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky
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

#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_dagfuncs.h>
#include <dev/raidframe/rf_dagffrd.h>
#include <dev/raidframe/rf_dagffwr.h>
#include <dev/raidframe/rf_dagdegrd.h>
#include <dev/raidframe/rf_dagdegwr.h>
#include <dev/raidframe/rf_etimer.h>
#include <dev/raidframe/rf_pqdeg.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_pqdegdags.h>
#include <dev/raidframe/rf_pq.h>

/*
   Degraded mode dag functions for P+Q calculations.

   The following nomenclature is used.

   PQ_<D><P><Q>_Create{Large,Small}<Write|Read>DAG

   where <D><P><Q> are single digits representing the number of failed
   data units <D> (0,1,2), parity units <P> (0,1), and Q units <Q>, effecting
   the I/O. The reads have only  PQ_<D><P><Q>_CreateReadDAG variants, while
   the single fault writes have both large and small write versions. (Single fault
   PQ is equivalent to normal mode raid 5 in many aspects.

   Some versions degenerate into the same case, and are grouped together below.
*/

/* Reads, single failure

   we have parity, so we can do a raid 5
   reconstruct read.
*/

RF_CREATE_DAG_FUNC_DECL(rf_PQ_100_CreateReadDAG)
{
	rf_CreateDegradedReadDAG(raidPtr, asmap, dag_h, bp, flags, allocList, &rf_pRecoveryFuncs);
}
/* Reads double failure  */

/*
   Q is lost, but not parity
   so we can a raid 5 reconstruct read.
*/

RF_CREATE_DAG_FUNC_DECL(rf_PQ_101_CreateReadDAG)
{
	rf_CreateDegradedReadDAG(raidPtr, asmap, dag_h, bp, flags, allocList, &rf_pRecoveryFuncs);
}
/*
  parity is lost, so we need to
  do a reconstruct read and recompute
  the data with Q.
*/

RF_CREATE_DAG_FUNC_DECL(rf_PQ_110_CreateReadDAG)
{
	RF_PhysDiskAddr_t *temp;
	/* swap P and Q pointers to fake out the DegradedReadDAG code */
	temp = asmap->parityInfo;
	asmap->parityInfo = asmap->qInfo;
	asmap->qInfo = temp;
	rf_CreateDegradedReadDAG(raidPtr, asmap, dag_h, bp, flags, allocList, &rf_qRecoveryFuncs);
}
/*
  Two data units are dead in this stripe, so we will need read
  both P and Q to reconstruct the data. Note that only
  one data unit we are reading may actually be missing.
*/
RF_CREATE_DAG_FUNC_DECL(rf_CreateDoubleDegradedReadDAG);
RF_CREATE_DAG_FUNC_DECL(rf_CreateDoubleDegradedReadDAG)
{
	rf_PQ_DoubleDegRead(raidPtr, asmap, dag_h, bp, flags, allocList);
}
RF_CREATE_DAG_FUNC_DECL(rf_PQ_200_CreateReadDAG);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_200_CreateReadDAG)
{
	rf_CreateDoubleDegradedReadDAG(raidPtr, asmap, dag_h, bp, flags, allocList);
}
/* Writes, single failure */

RF_CREATE_DAG_FUNC_DECL(rf_PQ_100_CreateWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_100_CreateWriteDAG)
{
	if (asmap->numStripeUnitsAccessed != 1 &&
	    asmap->failedPDAs[0]->numSector != 
	    raidPtr->Layout.sectorsPerStripeUnit)
		RF_PANIC();
	rf_CommonCreateSimpleDegradedWriteDAG(raidPtr, asmap, dag_h, bp, 
		      flags, allocList, 2, 
		      (int (*) (RF_DagNode_t *)) rf_Degraded_100_PQFunc, 
		      RF_FALSE);
}
/* Dead  P - act like a RAID 5 small write with parity = Q */
RF_CREATE_DAG_FUNC_DECL(rf_PQ_010_CreateSmallWriteDAG)
{
	RF_PhysDiskAddr_t *temp;
	/* swap P and Q pointers to fake out the DegradedReadDAG code */
	temp = asmap->parityInfo;
	asmap->parityInfo = asmap->qInfo;
	asmap->qInfo = temp;
	rf_CommonCreateSmallWriteDAG(raidPtr, asmap, dag_h, bp, flags, 
				     allocList, &rf_qFuncs, NULL);
}
/* Dead Q - act like a RAID 5 small write */
RF_CREATE_DAG_FUNC_DECL(rf_PQ_001_CreateSmallWriteDAG)
{
	rf_CommonCreateSmallWriteDAG(raidPtr, asmap, dag_h, bp, flags, 
				     allocList, &rf_pFuncs, NULL);
}
/* Dead P - act like a RAID 5 large write but for Q */
RF_CREATE_DAG_FUNC_DECL(rf_PQ_010_CreateLargeWriteDAG)
{
	RF_PhysDiskAddr_t *temp;
	/* swap P and Q pointers to fake out the code */
	temp = asmap->parityInfo;
	asmap->parityInfo = asmap->qInfo;
	asmap->qInfo = temp;
	rf_CommonCreateLargeWriteDAG(raidPtr, asmap, dag_h, bp, flags, 
				     allocList, 1, rf_RegularQFunc, RF_FALSE);
}
/* Dead Q - act like a RAID 5 large write */
RF_CREATE_DAG_FUNC_DECL(rf_PQ_001_CreateLargeWriteDAG)
{
	rf_CommonCreateLargeWriteDAG(raidPtr, asmap, dag_h, bp, flags, 
				     allocList, 1, rf_RegularPFunc, RF_FALSE);
}


/*
 * writes, double failure
 */

/*
 * Lost P & Q - do a nonredundant write
 */
RF_CREATE_DAG_FUNC_DECL(rf_PQ_011_CreateWriteDAG)
{
	rf_CreateNonRedundantWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList,
	    RF_IO_TYPE_WRITE);
}
/*
   In the two cases below,
   A nasty case arises when the write a (strict) portion of a failed stripe unit
   and parts of another su. For now, we do not support this.
*/

/*
  Lost Data and  P - do a Q write.
*/
RF_CREATE_DAG_FUNC_DECL(rf_PQ_110_CreateWriteDAG)
{
	RF_PhysDiskAddr_t *temp;

	if (asmap->numStripeUnitsAccessed != 1 &&
	    asmap->failedPDAs[0]->numSector != raidPtr->Layout.sectorsPerStripeUnit) {
		RF_PANIC();
	}
	/* swap P and Q to fake out parity code */
	temp = asmap->parityInfo;
	asmap->parityInfo = asmap->qInfo;
	asmap->qInfo = temp;
	rf_CommonCreateSimpleDegradedWriteDAG(raidPtr, asmap, dag_h, bp, flags,
		      allocList, 1, 
		      (int (*) (RF_DagNode_t *)) rf_PQ_DegradedWriteQFunc,
		      RF_FALSE);
	/* is the regular Q func the right one to call? */
}
/*
   Lost Data and Q - do degraded mode P write
*/
RF_CREATE_DAG_FUNC_DECL(rf_PQ_101_CreateWriteDAG)
{
	if (asmap->numStripeUnitsAccessed != 1 &&
	    asmap->failedPDAs[0]->numSector != raidPtr->Layout.sectorsPerStripeUnit)
		RF_PANIC();
	rf_CommonCreateSimpleDegradedWriteDAG(raidPtr, asmap, dag_h, bp, flags,
	    allocList, 1, rf_RecoveryXorFunc, RF_FALSE);
}
#endif				/* (RF_INCLUDE_DECL_PQ > 0) ||
				 * (RF_INCLUDE_RAID6 > 0) */
