/*	$FreeBSD$ */
/*	$NetBSD: rf_pq.c,v 1.7 2000/01/07 03:41:02 oster Exp $	*/
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

/*
 * Code for RAID level 6 (P + Q) disk array architecture.
 */

#include <dev/raidframe/rf_archs.h>
#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_dagffrd.h>
#include <dev/raidframe/rf_dagffwr.h>
#include <dev/raidframe/rf_dagdegrd.h>
#include <dev/raidframe/rf_dagdegwr.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_dagfuncs.h>
#include <dev/raidframe/rf_etimer.h>
#include <dev/raidframe/rf_pqdeg.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_map.h>
#include <dev/raidframe/rf_pq.h>

RF_RedFuncs_t rf_pFuncs = {rf_RegularONPFunc, "Regular Old-New P", rf_SimpleONPFunc, "Simple Old-New P"};
RF_RedFuncs_t rf_pRecoveryFuncs = {rf_RecoveryPFunc, "Recovery P Func", rf_RecoveryPFunc, "Recovery P Func"};

int 
rf_RegularONPFunc(node)
	RF_DagNode_t *node;
{
	return (rf_RegularXorFunc(node));
}
/*
   same as simpleONQ func, but the coefficient is always 1
*/

int 
rf_SimpleONPFunc(node)
	RF_DagNode_t *node;
{
	return (rf_SimpleXorFunc(node));
}

int 
rf_RecoveryPFunc(node)
	RF_DagNode_t *node;
{
	return (rf_RecoveryXorFunc(node));
}

int 
rf_RegularPFunc(node)
	RF_DagNode_t *node;
{
	return (rf_RegularXorFunc(node));
}
#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0)

static void 
QDelta(char *dest, char *obuf, char *nbuf, unsigned length,
    unsigned char coeff);
static void 
rf_InvertQ(unsigned long *qbuf, unsigned long *abuf,
    unsigned length, unsigned coeff);

RF_RedFuncs_t rf_qFuncs = {rf_RegularONQFunc, "Regular Old-New Q", rf_SimpleONQFunc, "Simple Old-New Q"};
RF_RedFuncs_t rf_qRecoveryFuncs = {rf_RecoveryQFunc, "Recovery Q Func", rf_RecoveryQFunc, "Recovery Q Func"};
RF_RedFuncs_t rf_pqRecoveryFuncs = {rf_RecoveryPQFunc, "Recovery PQ Func", rf_RecoveryPQFunc, "Recovery PQ Func"};

void 
rf_PQDagSelect(
    RF_Raid_t * raidPtr,
    RF_IoType_t type,
    RF_AccessStripeMap_t * asmap,
    RF_VoidFuncPtr * createFunc)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	unsigned ndfail = asmap->numDataFailed;
	unsigned npfail = asmap->numParityFailed;
	unsigned ntfail = npfail + ndfail;

	RF_ASSERT(RF_IO_IS_R_OR_W(type));
	if (ntfail > 2) {
		RF_ERRORMSG("more than two disks failed in a single group!  Aborting I/O operation.\n");
		 /* *infoFunc = */ *createFunc = NULL;
		return;
	}
	/* ok, we can do this I/O */
	if (type == RF_IO_TYPE_READ) {
		switch (ndfail) {
		case 0:
			/* fault free read */
			*createFunc = (RF_VoidFuncPtr) rf_CreateFaultFreeReadDAG;	/* same as raid 5 */
			break;
		case 1:
			/* lost a single data unit */
			/* two cases: (1) parity is not lost. do a normal raid
			 * 5 reconstruct read. (2) parity is lost. do a
			 * reconstruct read using "q". */
			if (ntfail == 2) {	/* also lost redundancy */
				if (asmap->failedPDAs[1]->type == RF_PDA_TYPE_PARITY)
					*createFunc = (RF_VoidFuncPtr) rf_PQ_110_CreateReadDAG;
				else
					*createFunc = (RF_VoidFuncPtr) rf_PQ_101_CreateReadDAG;
			} else {
				/* P and Q are ok. But is there a failure in
				 * some unaccessed data unit? */
				if (rf_NumFailedDataUnitsInStripe(raidPtr, asmap) == 2)
					*createFunc = (RF_VoidFuncPtr) rf_PQ_200_CreateReadDAG;
				else
					*createFunc = (RF_VoidFuncPtr) rf_PQ_100_CreateReadDAG;
			}
			break;
		case 2:
			/* lost two data units */
			/* *infoFunc = PQOneTwo; */
			*createFunc = (RF_VoidFuncPtr) rf_PQ_200_CreateReadDAG;
			break;
		}
		return;
	}
	/* a write */
	switch (ntfail) {
	case 0:		/* fault free */
		if (rf_suppressLocksAndLargeWrites ||
		    (((asmap->numStripeUnitsAccessed <= (layoutPtr->numDataCol / 2)) && (layoutPtr->numDataCol != 1)) ||
			(asmap->parityInfo->next != NULL) || (asmap->qInfo->next != NULL) || rf_CheckStripeForFailures(raidPtr, asmap))) {

			*createFunc = (RF_VoidFuncPtr) rf_PQCreateSmallWriteDAG;
		} else {
			*createFunc = (RF_VoidFuncPtr) rf_PQCreateLargeWriteDAG;
		}
		break;

	case 1:		/* single disk fault */
		if (npfail == 1) {
			RF_ASSERT((asmap->failedPDAs[0]->type == RF_PDA_TYPE_PARITY) || (asmap->failedPDAs[0]->type == RF_PDA_TYPE_Q));
			if (asmap->failedPDAs[0]->type == RF_PDA_TYPE_Q) {	/* q died, treat like
										 * normal mode raid5
										 * write. */
				if (((asmap->numStripeUnitsAccessed <= (layoutPtr->numDataCol / 2)) || (asmap->numStripeUnitsAccessed == 1))
				    || rf_NumFailedDataUnitsInStripe(raidPtr, asmap))
					*createFunc = (RF_VoidFuncPtr) rf_PQ_001_CreateSmallWriteDAG;
				else
					*createFunc = (RF_VoidFuncPtr) rf_PQ_001_CreateLargeWriteDAG;
			} else {/* parity died, small write only updating Q */
				if (((asmap->numStripeUnitsAccessed <= (layoutPtr->numDataCol / 2)) || (asmap->numStripeUnitsAccessed == 1))
				    || rf_NumFailedDataUnitsInStripe(raidPtr, asmap))
					*createFunc = (RF_VoidFuncPtr) rf_PQ_010_CreateSmallWriteDAG;
				else
					*createFunc = (RF_VoidFuncPtr) rf_PQ_010_CreateLargeWriteDAG;
			}
		} else {	/* data missing. Do a P reconstruct write if
				 * only a single data unit is lost in the
				 * stripe, otherwise a PQ reconstruct write. */
			if (rf_NumFailedDataUnitsInStripe(raidPtr, asmap) == 2)
				*createFunc = (RF_VoidFuncPtr) rf_PQ_200_CreateWriteDAG;
			else
				*createFunc = (RF_VoidFuncPtr) rf_PQ_100_CreateWriteDAG;
		}
		break;

	case 2:		/* two disk faults */
		switch (npfail) {
		case 2:	/* both p and q dead */
			*createFunc = (RF_VoidFuncPtr) rf_PQ_011_CreateWriteDAG;
			break;
		case 1:	/* either p or q and dead data */
			RF_ASSERT(asmap->failedPDAs[0]->type == RF_PDA_TYPE_DATA);
			RF_ASSERT((asmap->failedPDAs[1]->type == RF_PDA_TYPE_PARITY) || (asmap->failedPDAs[1]->type == RF_PDA_TYPE_Q));
			if (asmap->failedPDAs[1]->type == RF_PDA_TYPE_Q)
				*createFunc = (RF_VoidFuncPtr) rf_PQ_101_CreateWriteDAG;
			else
				*createFunc = (RF_VoidFuncPtr) rf_PQ_110_CreateWriteDAG;
			break;
		case 0:	/* double data loss */
			*createFunc = (RF_VoidFuncPtr) rf_PQ_200_CreateWriteDAG;
			break;
		}
		break;

	default:		/* more than 2 disk faults */
		*createFunc = NULL;
		RF_PANIC();
	}
	return;
}
/*
   Used as a stop gap info function
*/
#if 0
static void 
PQOne(raidPtr, nSucc, nAnte, asmap)
	RF_Raid_t *raidPtr;
	int    *nSucc;
	int    *nAnte;
	RF_AccessStripeMap_t *asmap;
{
	*nSucc = *nAnte = 1;
}

static void 
PQOneTwo(raidPtr, nSucc, nAnte, asmap)
	RF_Raid_t *raidPtr;
	int    *nSucc;
	int    *nAnte;
	RF_AccessStripeMap_t *asmap;
{
	*nSucc = 1;
	*nAnte = 2;
}
#endif

RF_CREATE_DAG_FUNC_DECL(rf_PQCreateLargeWriteDAG)
{
	rf_CommonCreateLargeWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList, 2,
	    rf_RegularPQFunc, RF_FALSE);
}

int 
rf_RegularONQFunc(node)
	RF_DagNode_t *node;
{
	int     np = node->numParams;
	int     d;
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[np - 1].p;
	int     i;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	RF_Etimer_t timer;
	char   *qbuf, *qpbuf;
	char   *obuf, *nbuf;
	RF_PhysDiskAddr_t *old, *new;
	unsigned long coeff;
	unsigned secPerSU = raidPtr->Layout.sectorsPerStripeUnit;

	RF_ETIMER_START(timer);

	d = (np - 3) / 4;
	RF_ASSERT(4 * d + 3 == np);
	qbuf = (char *) node->params[2 * d + 1].p;	/* q buffer */
	for (i = 0; i < d; i++) {
		old = (RF_PhysDiskAddr_t *) node->params[2 * i].p;
		obuf = (char *) node->params[2 * i + 1].p;
		new = (RF_PhysDiskAddr_t *) node->params[2 * (d + 1 + i)].p;
		nbuf = (char *) node->params[2 * (d + 1 + i) + 1].p;
		RF_ASSERT(new->numSector == old->numSector);
		RF_ASSERT(new->raidAddress == old->raidAddress);
		/* the stripe unit within the stripe tells us the coefficient
		 * to use for the multiply. */
		coeff = rf_RaidAddressToStripeUnitID(&(raidPtr->Layout), new->raidAddress);
		/* compute the data unit offset within the column, then add
		 * one */
		coeff = (coeff % raidPtr->Layout.numDataCol);
		qpbuf = qbuf + rf_RaidAddressToByte(raidPtr, old->startSector % secPerSU);
		QDelta(qpbuf, obuf, nbuf, rf_RaidAddressToByte(raidPtr, old->numSector), coeff);
	}

	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->q_us += RF_ETIMER_VAL_US(timer);
	rf_GenericWakeupFunc(node, 0);	/* call wake func explicitly since no
					 * I/O in this node */
	return (0);
}
/*
   See the SimpleXORFunc for the difference between a simple and regular func.
   These Q functions should be used for

         new q = Q(data,old data,old q)

   style updates and not for

         q = ( new data, new data, .... )

   computations.

   The simple q takes 2(2d+1)+1 params, where d is the number
   of stripes written. The order of params is
   old data pda_0, old data buffer_0, old data pda_1, old data buffer_1, ... old data pda_d, old data buffer_d
   [2d] old q pda_0, old q buffer
   [2d_2] new data pda_0, new data buffer_0, ...                                    new data pda_d, new data buffer_d
   raidPtr
*/

int 
rf_SimpleONQFunc(node)
	RF_DagNode_t *node;
{
	int     np = node->numParams;
	int     d;
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[np - 1].p;
	int     i;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	RF_Etimer_t timer;
	char   *qbuf;
	char   *obuf, *nbuf;
	RF_PhysDiskAddr_t *old, *new;
	unsigned long coeff;

	RF_ETIMER_START(timer);

	d = (np - 3) / 4;
	RF_ASSERT(4 * d + 3 == np);
	qbuf = (char *) node->params[2 * d + 1].p;	/* q buffer */
	for (i = 0; i < d; i++) {
		old = (RF_PhysDiskAddr_t *) node->params[2 * i].p;
		obuf = (char *) node->params[2 * i + 1].p;
		new = (RF_PhysDiskAddr_t *) node->params[2 * (d + 1 + i)].p;
		nbuf = (char *) node->params[2 * (d + 1 + i) + 1].p;
		RF_ASSERT(new->numSector == old->numSector);
		RF_ASSERT(new->raidAddress == old->raidAddress);
		/* the stripe unit within the stripe tells us the coefficient
		 * to use for the multiply. */
		coeff = rf_RaidAddressToStripeUnitID(&(raidPtr->Layout), new->raidAddress);
		/* compute the data unit offset within the column, then add
		 * one */
		coeff = (coeff % raidPtr->Layout.numDataCol);
		QDelta(qbuf, obuf, nbuf, rf_RaidAddressToByte(raidPtr, old->numSector), coeff);
	}

	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->q_us += RF_ETIMER_VAL_US(timer);
	rf_GenericWakeupFunc(node, 0);	/* call wake func explicitly since no
					 * I/O in this node */
	return (0);
}
RF_CREATE_DAG_FUNC_DECL(rf_PQCreateSmallWriteDAG)
{
	rf_CommonCreateSmallWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList, &rf_pFuncs, &rf_qFuncs);
}

static void RegularQSubr(RF_DagNode_t *node, char   *qbuf);

static void 
RegularQSubr(node, qbuf)
	RF_DagNode_t *node;
	char   *qbuf;
{
	int     np = node->numParams;
	int     d;
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[np - 1].p;
	unsigned secPerSU = raidPtr->Layout.sectorsPerStripeUnit;
	int     i;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	RF_Etimer_t timer;
	char   *obuf, *qpbuf;
	RF_PhysDiskAddr_t *old;
	unsigned long coeff;

	RF_ETIMER_START(timer);

	d = (np - 1) / 2;
	RF_ASSERT(2 * d + 1 == np);
	for (i = 0; i < d; i++) {
		old = (RF_PhysDiskAddr_t *) node->params[2 * i].p;
		obuf = (char *) node->params[2 * i + 1].p;
		coeff = rf_RaidAddressToStripeUnitID(&(raidPtr->Layout), old->raidAddress);
		/* compute the data unit offset within the column, then add
		 * one */
		coeff = (coeff % raidPtr->Layout.numDataCol);
		/* the input buffers may not all be aligned with the start of
		 * the stripe. so shift by their sector offset within the
		 * stripe unit */
		qpbuf = qbuf + rf_RaidAddressToByte(raidPtr, old->startSector % secPerSU);
		rf_IncQ((unsigned long *) qpbuf, (unsigned long *) obuf, rf_RaidAddressToByte(raidPtr, old->numSector), coeff);
	}

	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->q_us += RF_ETIMER_VAL_US(timer);
}
/*
   used in degraded writes.
*/

static void DegrQSubr(RF_DagNode_t *node);

static void 
DegrQSubr(node)
	RF_DagNode_t *node;
{
	int     np = node->numParams;
	int     d;
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[np - 1].p;
	unsigned secPerSU = raidPtr->Layout.sectorsPerStripeUnit;
	int     i;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	RF_Etimer_t timer;
	char   *qbuf = node->results[1];
	char   *obuf, *qpbuf;
	RF_PhysDiskAddr_t *old;
	unsigned long coeff;
	unsigned fail_start;
	int     j;

	old = (RF_PhysDiskAddr_t *) node->params[np - 2].p;
	fail_start = old->startSector % secPerSU;

	RF_ETIMER_START(timer);

	d = (np - 2) / 2;
	RF_ASSERT(2 * d + 2 == np);
	for (i = 0; i < d; i++) {
		old = (RF_PhysDiskAddr_t *) node->params[2 * i].p;
		obuf = (char *) node->params[2 * i + 1].p;
		coeff = rf_RaidAddressToStripeUnitID(&(raidPtr->Layout), old->raidAddress);
		/* compute the data unit offset within the column, then add
		 * one */
		coeff = (coeff % raidPtr->Layout.numDataCol);
		/* the input buffers may not all be aligned with the start of
		 * the stripe. so shift by their sector offset within the
		 * stripe unit */
		j = old->startSector % secPerSU;
		RF_ASSERT(j >= fail_start);
		qpbuf = qbuf + rf_RaidAddressToByte(raidPtr, j - fail_start);
		rf_IncQ((unsigned long *) qpbuf, (unsigned long *) obuf, rf_RaidAddressToByte(raidPtr, old->numSector), coeff);
	}

	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->q_us += RF_ETIMER_VAL_US(timer);
}
/*
   Called by large write code to compute the new parity and the new q.

   structure of the params:

   pda_0, buffer_0, pda_1 , buffer_1, ... , pda_d, buffer_d ( d = numDataCol
   raidPtr

   for a total of 2d+1 arguments.
   The result buffers results[0], results[1] are the buffers for the p and q,
   respectively.

   We compute Q first, then compute P. The P calculation may try to reuse
   one of the input buffers for its output, so if we computed P first, we would
   corrupt the input for the q calculation.
*/

int 
rf_RegularPQFunc(node)
	RF_DagNode_t *node;
{
	RegularQSubr(node, node->results[1]);
	return (rf_RegularXorFunc(node));	/* does the wakeup */
}

int 
rf_RegularQFunc(node)
	RF_DagNode_t *node;
{
	/* Almost ... adjust Qsubr args */
	RegularQSubr(node, node->results[0]);
	rf_GenericWakeupFunc(node, 0);	/* call wake func explicitly since no
					 * I/O in this node */
	return (0);
}
/*
   Called by singly degraded write code to compute the new parity and the new q.

   structure of the params:

   pda_0, buffer_0, pda_1 , buffer_1, ... , pda_d, buffer_d
   failedPDA raidPtr

   for a total of 2d+2 arguments.
   The result buffers results[0], results[1] are the buffers for the parity and q,
   respectively.

   We compute Q first, then compute parity. The parity calculation may try to reuse
   one of the input buffers for its output, so if we computed parity first, we would
   corrupt the input for the q calculation.

   We treat this identically to the regularPQ case, ignoring the failedPDA extra argument.
*/

void 
rf_Degraded_100_PQFunc(node)
	RF_DagNode_t *node;
{
	int     np = node->numParams;

	RF_ASSERT(np >= 2);
	DegrQSubr(node);
	rf_RecoveryXorFunc(node);
}


/*
   The two below are used when reading a stripe with a single lost data unit.
   The parameters are

   pda_0, buffer_0, .... pda_n, buffer_n, P pda, P buffer, failedPDA, raidPtr

   and results[0] contains the data buffer. Which is originally zero-filled.

*/

/* this Q func is used by the degraded-mode dag functions to recover lost data.
 * the second-to-last parameter is the PDA for the failed portion of the access.
 * the code here looks at this PDA and assumes that the xor target buffer is
 * equal in size to the number of sectors in the failed PDA.  It then uses
 * the other PDAs in the parameter list to determine where within the target
 * buffer the corresponding data should be xored.
 *
 * Recall the basic equation is
 *
 *     Q = ( data_1 + 2 * data_2 ... + k * data_k  ) mod 256
 *
 * so to recover data_j we need
 *
 *    J data_j = (Q - data_1 - 2 data_2 ....- k* data_k) mod 256
 *
 * So the coefficient for each buffer is (255 - data_col), and j should be initialized by
 * copying Q into it. Then we need to do a table lookup to convert to solve
 *   data_j /= J
 *
 *
 */
int 
rf_RecoveryQFunc(node)
	RF_DagNode_t *node;
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[node->numParams - 1].p;
	RF_RaidLayout_t *layoutPtr = (RF_RaidLayout_t *) & raidPtr->Layout;
	RF_PhysDiskAddr_t *failedPDA = (RF_PhysDiskAddr_t *) node->params[node->numParams - 2].p;
	int     i;
	RF_PhysDiskAddr_t *pda;
	RF_RaidAddr_t suoffset, failedSUOffset = rf_StripeUnitOffset(layoutPtr, failedPDA->startSector);
	char   *srcbuf, *destbuf;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	RF_Etimer_t timer;
	unsigned long coeff;

	RF_ETIMER_START(timer);
	/* start by copying Q into the buffer */
	bcopy(node->params[node->numParams - 3].p, node->results[0],
	    rf_RaidAddressToByte(raidPtr, failedPDA->numSector));
	for (i = 0; i < node->numParams - 4; i += 2) {
		RF_ASSERT(node->params[i + 1].p != node->results[0]);
		pda = (RF_PhysDiskAddr_t *) node->params[i].p;
		srcbuf = (char *) node->params[i + 1].p;
		suoffset = rf_StripeUnitOffset(layoutPtr, pda->startSector);
		destbuf = ((char *) node->results[0]) + rf_RaidAddressToByte(raidPtr, suoffset - failedSUOffset);
		coeff = rf_RaidAddressToStripeUnitID(&(raidPtr->Layout), pda->raidAddress);
		/* compute the data unit offset within the column */
		coeff = (coeff % raidPtr->Layout.numDataCol);
		rf_IncQ((unsigned long *) destbuf, (unsigned long *) srcbuf, rf_RaidAddressToByte(raidPtr, pda->numSector), coeff);
	}
	/* Do the nasty inversion now */
	coeff = (rf_RaidAddressToStripeUnitID(&(raidPtr->Layout), failedPDA->startSector) % raidPtr->Layout.numDataCol);
	rf_InvertQ(node->results[0], node->results[0], rf_RaidAddressToByte(raidPtr, pda->numSector), coeff);
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->q_us += RF_ETIMER_VAL_US(timer);
	rf_GenericWakeupFunc(node, 0);
	return (0);
}

int 
rf_RecoveryPQFunc(node)
	RF_DagNode_t *node;
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[node->numParams - 1].p;
	printf("raid%d: Recovery from PQ not implemented.\n",raidPtr->raidid);
	return (1);
}
/*
   Degraded write Q subroutine.
   Used when P is dead.
   Large-write style Q computation.
   Parameters

   (pda,buf),(pda,buf),.....,(failedPDA,bufPtr),failedPDA,raidPtr.

   We ignore failedPDA.

   This is a "simple style" recovery func.
*/

void 
rf_PQ_DegradedWriteQFunc(node)
	RF_DagNode_t *node;
{
	int     np = node->numParams;
	int     d;
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[np - 1].p;
	unsigned secPerSU = raidPtr->Layout.sectorsPerStripeUnit;
	int     i;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	RF_Etimer_t timer;
	char   *qbuf = node->results[0];
	char   *obuf, *qpbuf;
	RF_PhysDiskAddr_t *old;
	unsigned long coeff;
	int     fail_start, j;

	old = (RF_PhysDiskAddr_t *) node->params[np - 2].p;
	fail_start = old->startSector % secPerSU;

	RF_ETIMER_START(timer);

	d = (np - 2) / 2;
	RF_ASSERT(2 * d + 2 == np);

	for (i = 0; i < d; i++) {
		old = (RF_PhysDiskAddr_t *) node->params[2 * i].p;
		obuf = (char *) node->params[2 * i + 1].p;
		coeff = rf_RaidAddressToStripeUnitID(&(raidPtr->Layout), old->raidAddress);
		/* compute the data unit offset within the column, then add
		 * one */
		coeff = (coeff % raidPtr->Layout.numDataCol);
		j = old->startSector % secPerSU;
		RF_ASSERT(j >= fail_start);
		qpbuf = qbuf + rf_RaidAddressToByte(raidPtr, j - fail_start);
		rf_IncQ((unsigned long *) qpbuf, (unsigned long *) obuf, rf_RaidAddressToByte(raidPtr, old->numSector), coeff);
	}

	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->q_us += RF_ETIMER_VAL_US(timer);
	rf_GenericWakeupFunc(node, 0);
}




/* Q computations */

/*
   coeff - colummn;

   compute  dest ^= qfor[28-coeff][rn[coeff+1] a]

   on 5-bit basis;
   length in bytes;
*/

void 
rf_IncQ(dest, buf, length, coeff)
	unsigned long *dest;
	unsigned long *buf;
	unsigned length;
	unsigned coeff;
{
	unsigned long a, d, new;
	unsigned long a1, a2;
	unsigned int *q = &(rf_qfor[28 - coeff][0]);
	unsigned r = rf_rn[coeff + 1];

#define EXTRACT(a,i) ((a >> (5L*i)) & 0x1f)
#define INSERT(a,i) (a << (5L*i))

	length /= 8;
	/* 13 5 bit quants in a 64 bit word */
	while (length) {
		a = *buf++;
		d = *dest;
		a1 = EXTRACT(a, 0) ^ r;
		a2 = EXTRACT(a, 1) ^ r;
		new = INSERT(a2, 1) | a1;
		a1 = EXTRACT(a, 2) ^ r;
		a2 = EXTRACT(a, 3) ^ r;
		a1 = q[a1];
		a2 = q[a2];
		new = new | INSERT(a1, 2) | INSERT(a2, 3);
		a1 = EXTRACT(a, 4) ^ r;
		a2 = EXTRACT(a, 5) ^ r;
		a1 = q[a1];
		a2 = q[a2];
		new = new | INSERT(a1, 4) | INSERT(a2, 5);
		a1 = EXTRACT(a, 5) ^ r;
		a2 = EXTRACT(a, 6) ^ r;
		a1 = q[a1];
		a2 = q[a2];
		new = new | INSERT(a1, 5) | INSERT(a2, 6);
#if RF_LONGSHIFT > 2
		a1 = EXTRACT(a, 7) ^ r;
		a2 = EXTRACT(a, 8) ^ r;
		a1 = q[a1];
		a2 = q[a2];
		new = new | INSERT(a1, 7) | INSERT(a2, 8);
		a1 = EXTRACT(a, 9) ^ r;
		a2 = EXTRACT(a, 10) ^ r;
		a1 = q[a1];
		a2 = q[a2];
		new = new | INSERT(a1, 9) | INSERT(a2, 10);
		a1 = EXTRACT(a, 11) ^ r;
		a2 = EXTRACT(a, 12) ^ r;
		a1 = q[a1];
		a2 = q[a2];
		new = new | INSERT(a1, 11) | INSERT(a2, 12);
#endif				/* RF_LONGSHIFT > 2 */
		d ^= new;
		*dest++ = d;
		length--;
	}
}
/*
   compute

   dest ^= rf_qfor[28-coeff][rf_rn[coeff+1] (old^new) ]

   on a five bit basis.
   optimization: compute old ^ new on 64 bit basis.

   length in bytes.
*/

static void 
QDelta(
    char *dest,
    char *obuf,
    char *nbuf,
    unsigned length,
    unsigned char coeff)
{
	unsigned long a, d, new;
	unsigned long a1, a2;
	unsigned int *q = &(rf_qfor[28 - coeff][0]);
	unsigned int r = rf_rn[coeff + 1];

	r = a1 = a2 = new = d = a = 0; /* XXX for now... */
	q = NULL; /* XXX for now */

#ifdef _KERNEL
	/* PQ in kernel currently not supported because the encoding/decoding
	 * table is not present */
	bzero(dest, length);
#else				/* KERNEL */
	/* this code probably doesn't work and should be rewritten  -wvcii */
	/* 13 5 bit quants in a 64 bit word */
	length /= 8;
	while (length) {
		a = *obuf++;	/* XXX need to reorg to avoid cache conflicts */
		a ^= *nbuf++;
		d = *dest;
		a1 = EXTRACT(a, 0) ^ r;
		a2 = EXTRACT(a, 1) ^ r;
		a1 = q[a1];
		a2 = q[a2];
		new = INSERT(a2, 1) | a1;
		a1 = EXTRACT(a, 2) ^ r;
		a2 = EXTRACT(a, 3) ^ r;
		a1 = q[a1];
		a2 = q[a2];
		new = new | INSERT(a1, 2) | INSERT(a2, 3);
		a1 = EXTRACT(a, 4) ^ r;
		a2 = EXTRACT(a, 5) ^ r;
		a1 = q[a1];
		a2 = q[a2];
		new = new | INSERT(a1, 4) | INSERT(a2, 5);
		a1 = EXTRACT(a, 5) ^ r;
		a2 = EXTRACT(a, 6) ^ r;
		a1 = q[a1];
		a2 = q[a2];
		new = new | INSERT(a1, 5) | INSERT(a2, 6);
#if RF_LONGSHIFT > 2
		a1 = EXTRACT(a, 7) ^ r;
		a2 = EXTRACT(a, 8) ^ r;
		a1 = q[a1];
		a2 = q[a2];
		new = new | INSERT(a1, 7) | INSERT(a2, 8);
		a1 = EXTRACT(a, 9) ^ r;
		a2 = EXTRACT(a, 10) ^ r;
		a1 = q[a1];
		a2 = q[a2];
		new = new | INSERT(a1, 9) | INSERT(a2, 10);
		a1 = EXTRACT(a, 11) ^ r;
		a2 = EXTRACT(a, 12) ^ r;
		a1 = q[a1];
		a2 = q[a2];
		new = new | INSERT(a1, 11) | INSERT(a2, 12);
#endif				/* RF_LONGSHIFT > 2 */
		d ^= new;
		*dest++ = d;
		length--;
	}
#endif				/* _KERNEL */
}
/*
   recover columns a and b from the given p and q into
   bufs abuf and bbuf. All bufs are word aligned.
   Length is in bytes.
*/


/*
 * XXX
 *
 * Everything about this seems wrong.
 */
void 
rf_PQ_recover(pbuf, qbuf, abuf, bbuf, length, coeff_a, coeff_b)
	unsigned long *pbuf;
	unsigned long *qbuf;
	unsigned long *abuf;
	unsigned long *bbuf;
	unsigned length;
	unsigned coeff_a;
	unsigned coeff_b;
{
	unsigned long p, q, a, a0, a1;
	int     col = (29 * coeff_a) + coeff_b;
	unsigned char *q0 = &(rf_qinv[col][0]);

	length /= 8;
	while (length) {
		p = *pbuf++;
		q = *qbuf++;
		a0 = EXTRACT(p, 0);
		a1 = EXTRACT(q, 0);
		a = q0[a0 << 5 | a1];
#define MF(i) \
      a0 = EXTRACT(p,i); \
      a1 = EXTRACT(q,i); \
      a  = a | INSERT(q0[a0<<5 | a1],i)

		MF(1);
		MF(2);
		MF(3);
		MF(4);
		MF(5);
		MF(6);
#if 0
		MF(7);
		MF(8);
		MF(9);
		MF(10);
		MF(11);
		MF(12);
#endif				/* 0 */
		*abuf++ = a;
		*bbuf++ = a ^ p;
		length--;
	}
}
/*
   Lost parity and a data column. Recover that data column.
   Assume col coeff is lost. Let q the contents of Q after
   all surviving data columns have been q-xored out of it.
   Then we have the equation

   q[28-coeff][a_i ^ r_i+1] = q

   but q is cyclic with period 31.
   So q[3+coeff][q[28-coeff][a_i ^ r_{i+1}]] =
      q[31][a_i ^ r_{i+1}] = a_i ^ r_{i+1} .

   so a_i = r_{coeff+1} ^ q[3+coeff][q]

   The routine is passed q buffer and the buffer
   the data is to be recoverd into. They can be the same.
*/



static void 
rf_InvertQ(
    unsigned long *qbuf,
    unsigned long *abuf,
    unsigned length,
    unsigned coeff)
{
	unsigned long a, new;
	unsigned long a1, a2;
	unsigned int *q = &(rf_qfor[3 + coeff][0]);
	unsigned r = rf_rn[coeff + 1];

	/* 13 5 bit quants in a 64 bit word */
	length /= 8;
	while (length) {
		a = *qbuf++;
		a1 = EXTRACT(a, 0);
		a2 = EXTRACT(a, 1);
		a1 = r ^ q[a1];
		a2 = r ^ q[a2];
		new = INSERT(a2, 1) | a1;
#define M(i,j) \
      a1 = EXTRACT(a,i); \
      a2 = EXTRACT(a,j); \
      a1 = r ^ q[a1]; \
      a2 = r ^ q[a2]; \
      new = new | INSERT(a1,i) | INSERT(a2,j)

		M(2, 3);
		M(4, 5);
		M(5, 6);
#if RF_LONGSHIFT > 2
		M(7, 8);
		M(9, 10);
		M(11, 12);
#endif				/* RF_LONGSHIFT > 2 */
		*abuf++ = new;
		length--;
	}
}
#endif				/* (RF_INCLUDE_DECL_PQ > 0) ||
				 * (RF_INCLUDE_RAID6 > 0) */
