/*	$FreeBSD$ */
/*	$NetBSD: rf_dagdegwr.c,v 1.6 2001/01/26 04:05:08 oster Exp $	*/
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

/*
 * rf_dagdegwr.c
 *
 * code for creating degraded write DAGs
 *
 */

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_dagfuncs.h>
#include <dev/raidframe/rf_debugMem.h>
#include <dev/raidframe/rf_memchunk.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_dagdegwr.h>


/******************************************************************************
 *
 * General comments on DAG creation:
 *
 * All DAGs in this file use roll-away error recovery.  Each DAG has a single
 * commit node, usually called "Cmt."  If an error occurs before the Cmt node
 * is reached, the execution engine will halt forward execution and work
 * backward through the graph, executing the undo functions.  Assuming that
 * each node in the graph prior to the Cmt node are undoable and atomic - or -
 * does not make changes to permanent state, the graph will fail atomically.
 * If an error occurs after the Cmt node executes, the engine will roll-forward
 * through the graph, blindly executing nodes until it reaches the end.
 * If a graph reaches the end, it is assumed to have completed successfully.
 *
 * A graph has only 1 Cmt node.
 *
 */


/******************************************************************************
 *
 * The following wrappers map the standard DAG creation interface to the
 * DAG creation routines.  Additionally, these wrappers enable experimentation
 * with new DAG structures by providing an extra level of indirection, allowing
 * the DAG creation routines to be replaced at this single point.
 */

static 
RF_CREATE_DAG_FUNC_DECL(rf_CreateSimpleDegradedWriteDAG)
{
	rf_CommonCreateSimpleDegradedWriteDAG(raidPtr, asmap, dag_h, bp,
	    flags, allocList, 1, rf_RecoveryXorFunc, RF_TRUE);
}

void 
rf_CreateDegradedWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList)
	RF_Raid_t *raidPtr;
	RF_AccessStripeMap_t *asmap;
	RF_DagHeader_t *dag_h;
	void   *bp;
	RF_RaidAccessFlags_t flags;
	RF_AllocListElem_t *allocList;
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_PhysDiskAddr_t *failedPDA = asmap->failedPDAs[0];

	RF_ASSERT(asmap->numDataFailed == 1);
	dag_h->creator = "DegradedWriteDAG";

	/* if the access writes only a portion of the failed unit, and also
	 * writes some portion of at least one surviving unit, we create two
	 * DAGs, one for the failed component and one for the non-failed
	 * component, and do them sequentially.  Note that the fact that we're
	 * accessing only a portion of the failed unit indicates that the
	 * access either starts or ends in the failed unit, and hence we need
	 * create only two dags.  This is inefficient in that the same data or
	 * parity can get read and written twice using this structure.  I need
	 * to fix this to do the access all at once. */
	RF_ASSERT(!(asmap->numStripeUnitsAccessed != 1 && failedPDA->numSector != layoutPtr->sectorsPerStripeUnit));
	rf_CreateSimpleDegradedWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList);
}



/******************************************************************************
 *
 * DAG creation code begins here
 */



/******************************************************************************
 *
 * CommonCreateSimpleDegradedWriteDAG -- creates a DAG to do a degraded-mode
 * write, which is as follows
 *
 *                                        / {Wnq} --\
 * hdr -> blockNode ->  Rod -> Xor -> Cmt -> Wnp ----> unblock -> term
 *                  \  {Rod} /            \  Wnd ---/
 *                                        \ {Wnd} -/
 *
 * commit nodes: Xor, Wnd
 *
 * IMPORTANT:
 * This DAG generator does not work for double-degraded archs since it does not
 * generate Q
 *
 * This dag is essentially identical to the large-write dag, except that the
 * write to the failed data unit is suppressed.
 *
 * IMPORTANT:  this dag does not work in the case where the access writes only
 * a portion of the failed unit, and also writes some portion of at least one
 * surviving SU.  this case is handled in CreateDegradedWriteDAG above.
 *
 * The block & unblock nodes are leftovers from a previous version.  They
 * do nothing, but I haven't deleted them because it would be a tremendous
 * effort to put them back in.
 *
 * This dag is used whenever a one of the data units in a write has failed.
 * If it is the parity unit that failed, the nonredundant write dag (below)
 * is used.
 *****************************************************************************/

void 
rf_CommonCreateSimpleDegradedWriteDAG(raidPtr, asmap, dag_h, bp, flags,
    allocList, nfaults, redFunc, allowBufferRecycle)
	RF_Raid_t *raidPtr;
	RF_AccessStripeMap_t *asmap;
	RF_DagHeader_t *dag_h;
	void   *bp;
	RF_RaidAccessFlags_t flags;
	RF_AllocListElem_t *allocList;
	int     nfaults;
	int     (*redFunc) (RF_DagNode_t *);
	int     allowBufferRecycle;
{
	int     nNodes, nRrdNodes, nWndNodes, nXorBufs, i, j, paramNum,
	        rdnodesFaked;
	RF_DagNode_t *blockNode, *unblockNode, *wnpNode, *wnqNode, *termNode;
	RF_DagNode_t *nodes, *wndNodes, *rrdNodes, *xorNode, *commitNode;
	RF_SectorCount_t sectorsPerSU;
	RF_ReconUnitNum_t which_ru;
	char   *xorTargetBuf = NULL;	/* the target buffer for the XOR
					 * operation */
	char   *overlappingPDAs;/* a temporary array of flags */
	RF_AccessStripeMapHeader_t *new_asm_h[2];
	RF_PhysDiskAddr_t *pda, *parityPDA;
	RF_StripeNum_t parityStripeID;
	RF_PhysDiskAddr_t *failedPDA;
	RF_RaidLayout_t *layoutPtr;

	layoutPtr = &(raidPtr->Layout);
	parityStripeID = rf_RaidAddressToParityStripeID(layoutPtr, asmap->raidAddress,
	    &which_ru);
	sectorsPerSU = layoutPtr->sectorsPerStripeUnit;
	/* failedPDA points to the pda within the asm that targets the failed
	 * disk */
	failedPDA = asmap->failedPDAs[0];

	if (rf_dagDebug)
		printf("[Creating degraded-write DAG]\n");

	RF_ASSERT(asmap->numDataFailed == 1);
	dag_h->creator = "SimpleDegradedWriteDAG";

	/*
         * Generate two ASMs identifying the surviving data
         * we need in order to recover the lost data.
         */
	/* overlappingPDAs array must be zero'd */
	RF_Calloc(overlappingPDAs, asmap->numStripeUnitsAccessed, sizeof(char), (char *));
	rf_GenerateFailedAccessASMs(raidPtr, asmap, failedPDA, dag_h, new_asm_h,
	    &nXorBufs, NULL, overlappingPDAs, allocList);

	/* create all the nodes at once */
	nWndNodes = asmap->numStripeUnitsAccessed - 1;	/* no access is
							 * generated for the
							 * failed pda */

	nRrdNodes = ((new_asm_h[0]) ? new_asm_h[0]->stripeMap->numStripeUnitsAccessed : 0) +
	    ((new_asm_h[1]) ? new_asm_h[1]->stripeMap->numStripeUnitsAccessed : 0);
	/*
         * XXX
         *
         * There's a bug with a complete stripe overwrite- that means 0 reads
         * of old data, and the rest of the DAG generation code doesn't like
         * that. A release is coming, and I don't wanna risk breaking a critical
         * DAG generator, so here's what I'm gonna do- if there's no read nodes,
         * I'm gonna fake there being a read node, and I'm gonna swap in a
         * no-op node in its place (to make all the link-up code happy).
         * This should be fixed at some point.  --jimz
         */
	if (nRrdNodes == 0) {
		nRrdNodes = 1;
		rdnodesFaked = 1;
	} else {
		rdnodesFaked = 0;
	}
	/* lock, unlock, xor, Wnd, Rrd, W(nfaults) */
	nNodes = 5 + nfaults + nWndNodes + nRrdNodes;
	RF_CallocAndAdd(nodes, nNodes, sizeof(RF_DagNode_t),
	    (RF_DagNode_t *), allocList);
	i = 0;
	blockNode = &nodes[i];
	i += 1;
	commitNode = &nodes[i];
	i += 1;
	unblockNode = &nodes[i];
	i += 1;
	termNode = &nodes[i];
	i += 1;
	xorNode = &nodes[i];
	i += 1;
	wnpNode = &nodes[i];
	i += 1;
	wndNodes = &nodes[i];
	i += nWndNodes;
	rrdNodes = &nodes[i];
	i += nRrdNodes;
	if (nfaults == 2) {
		wnqNode = &nodes[i];
		i += 1;
	} else {
		wnqNode = NULL;
	}
	RF_ASSERT(i == nNodes);

	/* this dag can not commit until all rrd and xor Nodes have completed */
	dag_h->numCommitNodes = 1;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	RF_ASSERT(nRrdNodes > 0);
	rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
	    NULL, nRrdNodes, 0, 0, 0, dag_h, "Nil", allocList);
	rf_InitNode(commitNode, rf_wait, RF_TRUE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
	    NULL, nWndNodes + nfaults, 1, 0, 0, dag_h, "Cmt", allocList);
	rf_InitNode(unblockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
	    NULL, 1, nWndNodes + nfaults, 0, 0, dag_h, "Nil", allocList);
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc,
	    NULL, 0, 1, 0, 0, dag_h, "Trm", allocList);
	rf_InitNode(xorNode, rf_wait, RF_FALSE, redFunc, rf_NullNodeUndoFunc, NULL, 1,
	    nRrdNodes, 2 * nXorBufs + 2, nfaults, dag_h, "Xrc", allocList);

	/*
         * Fill in the Rrd nodes. If any of the rrd buffers are the same size as
         * the failed buffer, save a pointer to it so we can use it as the target
         * of the XOR. The pdas in the rrd nodes have been range-restricted, so if
         * a buffer is the same size as the failed buffer, it must also be at the
         * same alignment within the SU.
         */
	i = 0;
	if (new_asm_h[0]) {
		for (i = 0, pda = new_asm_h[0]->stripeMap->physInfo;
		    i < new_asm_h[0]->stripeMap->numStripeUnitsAccessed;
		    i++, pda = pda->next) {
			rf_InitNode(&rrdNodes[i], rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc,
			    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Rrd", allocList);
			RF_ASSERT(pda);
			rrdNodes[i].params[0].p = pda;
			rrdNodes[i].params[1].p = pda->bufPtr;
			rrdNodes[i].params[2].v = parityStripeID;
			rrdNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
		}
	}
	/* i now equals the number of stripe units accessed in new_asm_h[0] */
	if (new_asm_h[1]) {
		for (j = 0, pda = new_asm_h[1]->stripeMap->physInfo;
		    j < new_asm_h[1]->stripeMap->numStripeUnitsAccessed;
		    j++, pda = pda->next) {
			rf_InitNode(&rrdNodes[i + j], rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc,
			    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Rrd", allocList);
			RF_ASSERT(pda);
			rrdNodes[i + j].params[0].p = pda;
			rrdNodes[i + j].params[1].p = pda->bufPtr;
			rrdNodes[i + j].params[2].v = parityStripeID;
			rrdNodes[i + j].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
			if (allowBufferRecycle && (pda->numSector == failedPDA->numSector))
				xorTargetBuf = pda->bufPtr;
		}
	}
	if (rdnodesFaked) {
		/*
	         * This is where we'll init that fake noop read node
	         * (XXX should the wakeup func be different?)
	         */
		rf_InitNode(&rrdNodes[0], rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
		    NULL, 1, 1, 0, 0, dag_h, "RrN", allocList);
	}
	/*
         * Make a PDA for the parity unit.  The parity PDA should start at
         * the same offset into the SU as the failed PDA.
         */
	/* Danner comment: I don't think this copy is really necessary. We are
	 * in one of two cases here. (1) The entire failed unit is written.
	 * Then asmap->parityInfo will describe the entire parity. (2) We are
	 * only writing a subset of the failed unit and nothing else. Then the
	 * asmap->parityInfo describes the failed unit and the copy can also
	 * be avoided. */

	RF_MallocAndAdd(parityPDA, sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t *), allocList);
	parityPDA->row = asmap->parityInfo->row;
	parityPDA->col = asmap->parityInfo->col;
	parityPDA->startSector = ((asmap->parityInfo->startSector / sectorsPerSU)
	    * sectorsPerSU) + (failedPDA->startSector % sectorsPerSU);
	parityPDA->numSector = failedPDA->numSector;

	if (!xorTargetBuf) {
		RF_CallocAndAdd(xorTargetBuf, 1,
		    rf_RaidAddressToByte(raidPtr, failedPDA->numSector), (char *), allocList);
	}
	/* init the Wnp node */
	rf_InitNode(wnpNode, rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
	    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Wnp", allocList);
	wnpNode->params[0].p = parityPDA;
	wnpNode->params[1].p = xorTargetBuf;
	wnpNode->params[2].v = parityStripeID;
	wnpNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);

	/* fill in the Wnq Node */
	if (nfaults == 2) {
		{
			RF_MallocAndAdd(parityPDA, sizeof(RF_PhysDiskAddr_t),
			    (RF_PhysDiskAddr_t *), allocList);
			parityPDA->row = asmap->qInfo->row;
			parityPDA->col = asmap->qInfo->col;
			parityPDA->startSector = ((asmap->qInfo->startSector / sectorsPerSU)
			    * sectorsPerSU) + (failedPDA->startSector % sectorsPerSU);
			parityPDA->numSector = failedPDA->numSector;

			rf_InitNode(wnqNode, rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
			    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Wnq", allocList);
			wnqNode->params[0].p = parityPDA;
			RF_CallocAndAdd(xorNode->results[1], 1,
			    rf_RaidAddressToByte(raidPtr, failedPDA->numSector), (char *), allocList);
			wnqNode->params[1].p = xorNode->results[1];
			wnqNode->params[2].v = parityStripeID;
			wnqNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
		}
	}
	/* fill in the Wnd nodes */
	for (pda = asmap->physInfo, i = 0; i < nWndNodes; i++, pda = pda->next) {
		if (pda == failedPDA) {
			i--;
			continue;
		}
		rf_InitNode(&wndNodes[i], rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
		    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Wnd", allocList);
		RF_ASSERT(pda);
		wndNodes[i].params[0].p = pda;
		wndNodes[i].params[1].p = pda->bufPtr;
		wndNodes[i].params[2].v = parityStripeID;
		wndNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
	}

	/* fill in the results of the xor node */
	xorNode->results[0] = xorTargetBuf;

	/* fill in the params of the xor node */

	paramNum = 0;
	if (rdnodesFaked == 0) {
		for (i = 0; i < nRrdNodes; i++) {
			/* all the Rrd nodes need to be xored together */
			xorNode->params[paramNum++] = rrdNodes[i].params[0];
			xorNode->params[paramNum++] = rrdNodes[i].params[1];
		}
	}
	for (i = 0; i < nWndNodes; i++) {
		/* any Wnd nodes that overlap the failed access need to be
		 * xored in */
		if (overlappingPDAs[i]) {
			RF_MallocAndAdd(pda, sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t *), allocList);
			bcopy((char *) wndNodes[i].params[0].p, (char *) pda, sizeof(RF_PhysDiskAddr_t));
			rf_RangeRestrictPDA(raidPtr, failedPDA, pda, RF_RESTRICT_DOBUFFER, 0);
			xorNode->params[paramNum++].p = pda;
			xorNode->params[paramNum++].p = pda->bufPtr;
		}
	}
	RF_Free(overlappingPDAs, asmap->numStripeUnitsAccessed * sizeof(char));

	/*
         * Install the failed PDA into the xor param list so that the
         * new data gets xor'd in.
         */
	xorNode->params[paramNum++].p = failedPDA;
	xorNode->params[paramNum++].p = failedPDA->bufPtr;

	/*
         * The last 2 params to the recovery xor node are always the failed
         * PDA and the raidPtr. install the failedPDA even though we have just
         * done so above. This allows us to use the same XOR function for both
         * degraded reads and degraded writes.
         */
	xorNode->params[paramNum++].p = failedPDA;
	xorNode->params[paramNum++].p = raidPtr;
	RF_ASSERT(paramNum == 2 * nXorBufs + 2);

	/*
         * Code to link nodes begins here
         */

	/* link header to block node */
	RF_ASSERT(blockNode->numAntecedents == 0);
	dag_h->succedents[0] = blockNode;

	/* link block node to rd nodes */
	RF_ASSERT(blockNode->numSuccedents == nRrdNodes);
	for (i = 0; i < nRrdNodes; i++) {
		RF_ASSERT(rrdNodes[i].numAntecedents == 1);
		blockNode->succedents[i] = &rrdNodes[i];
		rrdNodes[i].antecedents[0] = blockNode;
		rrdNodes[i].antType[0] = rf_control;
	}

	/* link read nodes to xor node */
	RF_ASSERT(xorNode->numAntecedents == nRrdNodes);
	for (i = 0; i < nRrdNodes; i++) {
		RF_ASSERT(rrdNodes[i].numSuccedents == 1);
		rrdNodes[i].succedents[0] = xorNode;
		xorNode->antecedents[i] = &rrdNodes[i];
		xorNode->antType[i] = rf_trueData;
	}

	/* link xor node to commit node */
	RF_ASSERT(xorNode->numSuccedents == 1);
	RF_ASSERT(commitNode->numAntecedents == 1);
	xorNode->succedents[0] = commitNode;
	commitNode->antecedents[0] = xorNode;
	commitNode->antType[0] = rf_control;

	/* link commit node to wnd nodes */
	RF_ASSERT(commitNode->numSuccedents == nfaults + nWndNodes);
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(wndNodes[i].numAntecedents == 1);
		commitNode->succedents[i] = &wndNodes[i];
		wndNodes[i].antecedents[0] = commitNode;
		wndNodes[i].antType[0] = rf_control;
	}

	/* link the commit node to wnp, wnq nodes */
	RF_ASSERT(wnpNode->numAntecedents == 1);
	commitNode->succedents[nWndNodes] = wnpNode;
	wnpNode->antecedents[0] = commitNode;
	wnpNode->antType[0] = rf_control;
	if (nfaults == 2) {
		RF_ASSERT(wnqNode->numAntecedents == 1);
		commitNode->succedents[nWndNodes + 1] = wnqNode;
		wnqNode->antecedents[0] = commitNode;
		wnqNode->antType[0] = rf_control;
	}
	/* link write new data nodes to unblock node */
	RF_ASSERT(unblockNode->numAntecedents == (nWndNodes + nfaults));
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(wndNodes[i].numSuccedents == 1);
		wndNodes[i].succedents[0] = unblockNode;
		unblockNode->antecedents[i] = &wndNodes[i];
		unblockNode->antType[i] = rf_control;
	}

	/* link write new parity node to unblock node */
	RF_ASSERT(wnpNode->numSuccedents == 1);
	wnpNode->succedents[0] = unblockNode;
	unblockNode->antecedents[nWndNodes] = wnpNode;
	unblockNode->antType[nWndNodes] = rf_control;

	/* link write new q node to unblock node */
	if (nfaults == 2) {
		RF_ASSERT(wnqNode->numSuccedents == 1);
		wnqNode->succedents[0] = unblockNode;
		unblockNode->antecedents[nWndNodes + 1] = wnqNode;
		unblockNode->antType[nWndNodes + 1] = rf_control;
	}
	/* link unblock node to term node */
	RF_ASSERT(unblockNode->numSuccedents == 1);
	RF_ASSERT(termNode->numAntecedents == 1);
	RF_ASSERT(termNode->numSuccedents == 0);
	unblockNode->succedents[0] = termNode;
	termNode->antecedents[0] = unblockNode;
	termNode->antType[0] = rf_control;
}
#define CONS_PDA(if,start,num) \
  pda_p->row = asmap->if->row;    pda_p->col = asmap->if->col; \
  pda_p->startSector = ((asmap->if->startSector / secPerSU) * secPerSU) + start; \
  pda_p->numSector = num; \
  pda_p->next = NULL; \
  RF_MallocAndAdd(pda_p->bufPtr,rf_RaidAddressToByte(raidPtr,num),(char *), allocList)
#if (RF_INCLUDE_PQ > 0) || (RF_INCLUDE_EVENODD > 0)
void 
rf_WriteGenerateFailedAccessASMs(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_PhysDiskAddr_t ** pdap,
    int *nNodep,
    RF_PhysDiskAddr_t ** pqpdap,
    int *nPQNodep,
    RF_AllocListElem_t * allocList)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	int     PDAPerDisk, i;
	RF_SectorCount_t secPerSU = layoutPtr->sectorsPerStripeUnit;
	int     numDataCol = layoutPtr->numDataCol;
	int     state;
	unsigned napdas;
	RF_SectorNum_t fone_start, fone_end, ftwo_start = 0, ftwo_end;
	RF_PhysDiskAddr_t *fone = asmap->failedPDAs[0], *ftwo = asmap->failedPDAs[1];
	RF_PhysDiskAddr_t *pda_p;
	RF_RaidAddr_t sosAddr;

	/* determine how many pda's we will have to generate per unaccess
	 * stripe. If there is only one failed data unit, it is one; if two,
	 * possibly two, depending wether they overlap. */

	fone_start = rf_StripeUnitOffset(layoutPtr, fone->startSector);
	fone_end = fone_start + fone->numSector;

	if (asmap->numDataFailed == 1) {
		PDAPerDisk = 1;
		state = 1;
		RF_MallocAndAdd(*pqpdap, 2 * sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t *), allocList);
		pda_p = *pqpdap;
		/* build p */
		CONS_PDA(parityInfo, fone_start, fone->numSector);
		pda_p->type = RF_PDA_TYPE_PARITY;
		pda_p++;
		/* build q */
		CONS_PDA(qInfo, fone_start, fone->numSector);
		pda_p->type = RF_PDA_TYPE_Q;
	} else {
		ftwo_start = rf_StripeUnitOffset(layoutPtr, ftwo->startSector);
		ftwo_end = ftwo_start + ftwo->numSector;
		if (fone->numSector + ftwo->numSector > secPerSU) {
			PDAPerDisk = 1;
			state = 2;
			RF_MallocAndAdd(*pqpdap, 2 * sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t *), allocList);
			pda_p = *pqpdap;
			CONS_PDA(parityInfo, 0, secPerSU);
			pda_p->type = RF_PDA_TYPE_PARITY;
			pda_p++;
			CONS_PDA(qInfo, 0, secPerSU);
			pda_p->type = RF_PDA_TYPE_Q;
		} else {
			PDAPerDisk = 2;
			state = 3;
			/* four of them, fone, then ftwo */
			RF_MallocAndAdd(*pqpdap, 4 * sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t *), allocList);
			pda_p = *pqpdap;
			CONS_PDA(parityInfo, fone_start, fone->numSector);
			pda_p->type = RF_PDA_TYPE_PARITY;
			pda_p++;
			CONS_PDA(qInfo, fone_start, fone->numSector);
			pda_p->type = RF_PDA_TYPE_Q;
			pda_p++;
			CONS_PDA(parityInfo, ftwo_start, ftwo->numSector);
			pda_p->type = RF_PDA_TYPE_PARITY;
			pda_p++;
			CONS_PDA(qInfo, ftwo_start, ftwo->numSector);
			pda_p->type = RF_PDA_TYPE_Q;
		}
	}
	/* figure out number of nonaccessed pda */
	napdas = PDAPerDisk * (numDataCol - 2);
	*nPQNodep = PDAPerDisk;

	*nNodep = napdas;
	if (napdas == 0)
		return;		/* short circuit */

	/* allocate up our list of pda's */

	RF_CallocAndAdd(pda_p, napdas, sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t *), allocList);
	*pdap = pda_p;

	/* linkem together */
	for (i = 0; i < (napdas - 1); i++)
		pda_p[i].next = pda_p + (i + 1);

	sosAddr = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, asmap->raidAddress);
	for (i = 0; i < numDataCol; i++) {
		if ((pda_p - (*pdap)) == napdas)
			continue;
		pda_p->type = RF_PDA_TYPE_DATA;
		pda_p->raidAddress = sosAddr + (i * secPerSU);
		(raidPtr->Layout.map->MapSector) (raidPtr, pda_p->raidAddress, &(pda_p->row), &(pda_p->col), &(pda_p->startSector), 0);
		/* skip over dead disks */
		if (RF_DEAD_DISK(raidPtr->Disks[pda_p->row][pda_p->col].status))
			continue;
		switch (state) {
		case 1:	/* fone */
			pda_p->numSector = fone->numSector;
			pda_p->raidAddress += fone_start;
			pda_p->startSector += fone_start;
			RF_MallocAndAdd(pda_p->bufPtr, rf_RaidAddressToByte(raidPtr, pda_p->numSector), (char *), allocList);
			break;
		case 2:	/* full stripe */
			pda_p->numSector = secPerSU;
			RF_MallocAndAdd(pda_p->bufPtr, rf_RaidAddressToByte(raidPtr, secPerSU), (char *), allocList);
			break;
		case 3:	/* two slabs */
			pda_p->numSector = fone->numSector;
			pda_p->raidAddress += fone_start;
			pda_p->startSector += fone_start;
			RF_MallocAndAdd(pda_p->bufPtr, rf_RaidAddressToByte(raidPtr, pda_p->numSector), (char *), allocList);
			pda_p++;
			pda_p->type = RF_PDA_TYPE_DATA;
			pda_p->raidAddress = sosAddr + (i * secPerSU);
			(raidPtr->Layout.map->MapSector) (raidPtr, pda_p->raidAddress, &(pda_p->row), &(pda_p->col), &(pda_p->startSector), 0);
			pda_p->numSector = ftwo->numSector;
			pda_p->raidAddress += ftwo_start;
			pda_p->startSector += ftwo_start;
			RF_MallocAndAdd(pda_p->bufPtr, rf_RaidAddressToByte(raidPtr, pda_p->numSector), (char *), allocList);
			break;
		default:
			RF_PANIC();
		}
		pda_p++;
	}

	RF_ASSERT(pda_p - *pdap == napdas);
	return;
}
#define DISK_NODE_PDA(node)  ((node)->params[0].p)

#define DISK_NODE_PARAMS(_node_,_p_) \
  (_node_).params[0].p = _p_ ; \
  (_node_).params[1].p = (_p_)->bufPtr; \
  (_node_).params[2].v = parityStripeID; \
  (_node_).params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru)

void 
rf_DoubleDegSmallWrite(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList,
    char *redundantReadNodeName,
    char *redundantWriteNodeName,
    char *recoveryNodeName,
    int (*recovFunc) (RF_DagNode_t *))
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_DagNode_t *nodes, *wudNodes, *rrdNodes, *recoveryNode, *blockNode,
	       *unblockNode, *rpNodes, *rqNodes, *wpNodes, *wqNodes, *termNode;
	RF_PhysDiskAddr_t *pda, *pqPDAs;
	RF_PhysDiskAddr_t *npdas;
	int     nWriteNodes, nNodes, nReadNodes, nRrdNodes, nWudNodes, i;
	RF_ReconUnitNum_t which_ru;
	int     nPQNodes;
	RF_StripeNum_t parityStripeID = rf_RaidAddressToParityStripeID(layoutPtr, asmap->raidAddress, &which_ru);

	/* simple small write case - First part looks like a reconstruct-read
	 * of the failed data units. Then a write of all data units not
	 * failed. */


	/* Hdr | ------Block- /  /         \   Rrd  Rrd ...  Rrd  Rp Rq \  \
	 * /  -------PQ----- /   \   \ Wud   Wp  WQ	     \    |   /
	 * --Unblock- | T
	 * 
	 * Rrd = read recovery data  (potentially none) Wud = write user data
	 * (not incl. failed disks) Wp = Write P (could be two) Wq = Write Q
	 * (could be two)
	 * 
	 */

	rf_WriteGenerateFailedAccessASMs(raidPtr, asmap, &npdas, &nRrdNodes, &pqPDAs, &nPQNodes, allocList);

	RF_ASSERT(asmap->numDataFailed == 1);

	nWudNodes = asmap->numStripeUnitsAccessed - (asmap->numDataFailed);
	nReadNodes = nRrdNodes + 2 * nPQNodes;
	nWriteNodes = nWudNodes + 2 * nPQNodes;
	nNodes = 4 + nReadNodes + nWriteNodes;

	RF_CallocAndAdd(nodes, nNodes, sizeof(RF_DagNode_t), (RF_DagNode_t *), allocList);
	blockNode = nodes;
	unblockNode = blockNode + 1;
	termNode = unblockNode + 1;
	recoveryNode = termNode + 1;
	rrdNodes = recoveryNode + 1;
	rpNodes = rrdNodes + nRrdNodes;
	rqNodes = rpNodes + nPQNodes;
	wudNodes = rqNodes + nPQNodes;
	wpNodes = wudNodes + nWudNodes;
	wqNodes = wpNodes + nPQNodes;

	dag_h->creator = "PQ_DDSimpleSmallWrite";
	dag_h->numSuccedents = 1;
	dag_h->succedents[0] = blockNode;
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc, NULL, 0, 1, 0, 0, dag_h, "Trm", allocList);
	termNode->antecedents[0] = unblockNode;
	termNode->antType[0] = rf_control;

	/* init the block and unblock nodes */
	/* The block node has all the read nodes as successors */
	rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, nReadNodes, 0, 0, 0, dag_h, "Nil", allocList);
	for (i = 0; i < nReadNodes; i++)
		blockNode->succedents[i] = rrdNodes + i;

	/* The unblock node has all the writes as successors */
	rf_InitNode(unblockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, 1, nWriteNodes, 0, 0, dag_h, "Nil", allocList);
	for (i = 0; i < nWriteNodes; i++) {
		unblockNode->antecedents[i] = wudNodes + i;
		unblockNode->antType[i] = rf_control;
	}
	unblockNode->succedents[0] = termNode;

#define INIT_READ_NODE(node,name) \
  rf_InitNode(node, rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, name, allocList); \
  (node)->succedents[0] = recoveryNode; \
  (node)->antecedents[0] = blockNode; \
  (node)->antType[0] = rf_control;

	/* build the read nodes */
	pda = npdas;
	for (i = 0; i < nRrdNodes; i++, pda = pda->next) {
		INIT_READ_NODE(rrdNodes + i, "rrd");
		DISK_NODE_PARAMS(rrdNodes[i], pda);
	}

	/* read redundancy pdas */
	pda = pqPDAs;
	INIT_READ_NODE(rpNodes, "Rp");
	RF_ASSERT(pda);
	DISK_NODE_PARAMS(rpNodes[0], pda);
	pda++;
	INIT_READ_NODE(rqNodes, redundantReadNodeName);
	RF_ASSERT(pda);
	DISK_NODE_PARAMS(rqNodes[0], pda);
	if (nPQNodes == 2) {
		pda++;
		INIT_READ_NODE(rpNodes + 1, "Rp");
		RF_ASSERT(pda);
		DISK_NODE_PARAMS(rpNodes[1], pda);
		pda++;
		INIT_READ_NODE(rqNodes + 1, redundantReadNodeName);
		RF_ASSERT(pda);
		DISK_NODE_PARAMS(rqNodes[1], pda);
	}
	/* the recovery node has all reads as precedessors and all writes as
	 * successors. It generates a result for every write P or write Q
	 * node. As parameters, it takes a pda per read and a pda per stripe
	 * of user data written. It also takes as the last params the raidPtr
	 * and asm. For results, it takes PDA for P & Q. */


	rf_InitNode(recoveryNode, rf_wait, RF_FALSE, recovFunc, rf_NullNodeUndoFunc, NULL,
	    nWriteNodes,	/* succesors */
	    nReadNodes,		/* preds */
	    nReadNodes + nWudNodes + 3,	/* params */
	    2 * nPQNodes,	/* results */
	    dag_h, recoveryNodeName, allocList);



	for (i = 0; i < nReadNodes; i++) {
		recoveryNode->antecedents[i] = rrdNodes + i;
		recoveryNode->antType[i] = rf_control;
		recoveryNode->params[i].p = DISK_NODE_PDA(rrdNodes + i);
	}
	for (i = 0; i < nWudNodes; i++) {
		recoveryNode->succedents[i] = wudNodes + i;
	}
	recoveryNode->params[nReadNodes + nWudNodes].p = asmap->failedPDAs[0];
	recoveryNode->params[nReadNodes + nWudNodes + 1].p = raidPtr;
	recoveryNode->params[nReadNodes + nWudNodes + 2].p = asmap;

	for (; i < nWriteNodes; i++)
		recoveryNode->succedents[i] = wudNodes + i;

	pda = pqPDAs;
	recoveryNode->results[0] = pda;
	pda++;
	recoveryNode->results[1] = pda;
	if (nPQNodes == 2) {
		pda++;
		recoveryNode->results[2] = pda;
		pda++;
		recoveryNode->results[3] = pda;
	}
	/* fill writes */
#define INIT_WRITE_NODE(node,name) \
  rf_InitNode(node, rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, name, allocList); \
    (node)->succedents[0] = unblockNode; \
    (node)->antecedents[0] = recoveryNode; \
    (node)->antType[0] = rf_control;

	pda = asmap->physInfo;
	for (i = 0; i < nWudNodes; i++) {
		INIT_WRITE_NODE(wudNodes + i, "Wd");
		DISK_NODE_PARAMS(wudNodes[i], pda);
		recoveryNode->params[nReadNodes + i].p = DISK_NODE_PDA(wudNodes + i);
		pda = pda->next;
	}
	/* write redundancy pdas */
	pda = pqPDAs;
	INIT_WRITE_NODE(wpNodes, "Wp");
	RF_ASSERT(pda);
	DISK_NODE_PARAMS(wpNodes[0], pda);
	pda++;
	INIT_WRITE_NODE(wqNodes, "Wq");
	RF_ASSERT(pda);
	DISK_NODE_PARAMS(wqNodes[0], pda);
	if (nPQNodes == 2) {
		pda++;
		INIT_WRITE_NODE(wpNodes + 1, "Wp");
		RF_ASSERT(pda);
		DISK_NODE_PARAMS(wpNodes[1], pda);
		pda++;
		INIT_WRITE_NODE(wqNodes + 1, "Wq");
		RF_ASSERT(pda);
		DISK_NODE_PARAMS(wqNodes[1], pda);
	}
}
#endif   /* (RF_INCLUDE_PQ > 0) || (RF_INCLUDE_EVENODD > 0) */
