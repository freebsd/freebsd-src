/*	$NetBSD: rf_dagdegrd.c,v 1.7 2001/01/26 14:06:16 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
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
 * rf_dagdegrd.c
 *
 * code for creating degraded read DAGs
 */

#include <dev/raidframe/rf_archs.h>
#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_dagfuncs.h>
#include <dev/raidframe/rf_debugMem.h>
#include <dev/raidframe/rf_memchunk.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_dagdegrd.h>


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

void 
rf_CreateRaidFiveDegradedReadDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList)
{
	rf_CreateDegradedReadDAG(raidPtr, asmap, dag_h, bp, flags, allocList,
	    &rf_xorRecoveryFuncs);
}


/******************************************************************************
 *
 * DAG creation code begins here
 */


/******************************************************************************
 * Create a degraded read DAG for RAID level 1
 *
 * Hdr -> Nil -> R(p/s)d -> Commit -> Trm
 *
 * The "Rd" node reads data from the surviving disk in the mirror pair
 *   Rpd - read of primary copy
 *   Rsd - read of secondary copy
 *
 * Parameters:  raidPtr   - description of the physical array
 *              asmap     - logical & physical addresses for this access
 *              bp        - buffer ptr (for holding write data)
 *              flags     - general flags (e.g. disk locking)
 *              allocList - list of memory allocated in DAG creation
 *****************************************************************************/

void 
rf_CreateRaidOneDegradedReadDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList)
{
	RF_DagNode_t *nodes, *rdNode, *blockNode, *commitNode, *termNode;
	RF_StripeNum_t parityStripeID;
	RF_ReconUnitNum_t which_ru;
	RF_PhysDiskAddr_t *pda;
	int     useMirror, i;

	useMirror = 0;
	parityStripeID = rf_RaidAddressToParityStripeID(&(raidPtr->Layout),
	    asmap->raidAddress, &which_ru);
	if (rf_dagDebug) {
		printf("[Creating RAID level 1 degraded read DAG]\n");
	}
	dag_h->creator = "RaidOneDegradedReadDAG";
	/* alloc the Wnd nodes and the Wmir node */
	if (asmap->numDataFailed == 0)
		useMirror = RF_FALSE;
	else
		useMirror = RF_TRUE;

	/* total number of nodes = 1 + (block + commit + terminator) */
	RF_CallocAndAdd(nodes, 4, sizeof(RF_DagNode_t), (RF_DagNode_t *), allocList);
	i = 0;
	rdNode = &nodes[i];
	i++;
	blockNode = &nodes[i];
	i++;
	commitNode = &nodes[i];
	i++;
	termNode = &nodes[i];
	i++;

	/* this dag can not commit until the commit node is reached.   errors
	 * prior to the commit point imply the dag has failed and must be
	 * retried */
	dag_h->numCommitNodes = 1;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	/* initialize the block, commit, and terminator nodes */
	rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
	    NULL, 1, 0, 0, 0, dag_h, "Nil", allocList);
	rf_InitNode(commitNode, rf_wait, RF_TRUE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
	    NULL, 1, 1, 0, 0, dag_h, "Cmt", allocList);
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc,
	    NULL, 0, 1, 0, 0, dag_h, "Trm", allocList);

	pda = asmap->physInfo;
	RF_ASSERT(pda != NULL);
	/* parityInfo must describe entire parity unit */
	RF_ASSERT(asmap->parityInfo->next == NULL);

	/* initialize the data node */
	if (!useMirror) {
		/* read primary copy of data */
		rf_InitNode(rdNode, rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc,
		    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Rpd", allocList);
		rdNode->params[0].p = pda;
		rdNode->params[1].p = pda->bufPtr;
		rdNode->params[2].v = parityStripeID;
		rdNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
	} else {
		/* read secondary copy of data */
		rf_InitNode(rdNode, rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc,
		    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Rsd", allocList);
		rdNode->params[0].p = asmap->parityInfo;
		rdNode->params[1].p = pda->bufPtr;
		rdNode->params[2].v = parityStripeID;
		rdNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
	}

	/* connect header to block node */
	RF_ASSERT(dag_h->numSuccedents == 1);
	RF_ASSERT(blockNode->numAntecedents == 0);
	dag_h->succedents[0] = blockNode;

	/* connect block node to rdnode */
	RF_ASSERT(blockNode->numSuccedents == 1);
	RF_ASSERT(rdNode->numAntecedents == 1);
	blockNode->succedents[0] = rdNode;
	rdNode->antecedents[0] = blockNode;
	rdNode->antType[0] = rf_control;

	/* connect rdnode to commit node */
	RF_ASSERT(rdNode->numSuccedents == 1);
	RF_ASSERT(commitNode->numAntecedents == 1);
	rdNode->succedents[0] = commitNode;
	commitNode->antecedents[0] = rdNode;
	commitNode->antType[0] = rf_control;

	/* connect commit node to terminator */
	RF_ASSERT(commitNode->numSuccedents == 1);
	RF_ASSERT(termNode->numAntecedents == 1);
	RF_ASSERT(termNode->numSuccedents == 0);
	commitNode->succedents[0] = termNode;
	termNode->antecedents[0] = commitNode;
	termNode->antType[0] = rf_control;
}



/******************************************************************************
 *
 * creates a DAG to perform a degraded-mode read of data within one stripe.
 * This DAG is as follows:
 *
 * Hdr -> Block -> Rud -> Xor -> Cmt -> T
 *              -> Rrd ->
 *              -> Rp -->
 *
 * Each R node is a successor of the L node
 * One successor arc from each R node goes to C, and the other to X
 * There is one Rud for each chunk of surviving user data requested by the
 * user, and one Rrd for each chunk of surviving user data _not_ being read by
 * the user
 * R = read, ud = user data, rd = recovery (surviving) data, p = parity
 * X = XOR, C = Commit, T = terminate
 *
 * The block node guarantees a single source node.
 *
 * Note:  The target buffer for the XOR node is set to the actual user buffer
 * where the failed data is supposed to end up.  This buffer is zero'd by the
 * code here.  Thus, if you create a degraded read dag, use it, and then
 * re-use, you have to be sure to zero the target buffer prior to the re-use.
 *
 * The recfunc argument at the end specifies the name and function used for
 * the redundancy
 * recovery function.
 *
 *****************************************************************************/

void 
rf_CreateDegradedReadDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList,
    RF_RedFuncs_t * recFunc)
{
	RF_DagNode_t *nodes, *rudNodes, *rrdNodes, *xorNode, *blockNode;
	RF_DagNode_t *commitNode, *rpNode, *termNode;
	int     nNodes, nRrdNodes, nRudNodes, nXorBufs, i;
	int     j, paramNum;
	RF_SectorCount_t sectorsPerSU;
	RF_ReconUnitNum_t which_ru;
	char   *overlappingPDAs;/* a temporary array of flags */
	RF_AccessStripeMapHeader_t *new_asm_h[2];
	RF_PhysDiskAddr_t *pda, *parityPDA;
	RF_StripeNum_t parityStripeID;
	RF_PhysDiskAddr_t *failedPDA;
	RF_RaidLayout_t *layoutPtr;
	char   *rpBuf;

	layoutPtr = &(raidPtr->Layout);
	/* failedPDA points to the pda within the asm that targets the failed
	 * disk */
	failedPDA = asmap->failedPDAs[0];
	parityStripeID = rf_RaidAddressToParityStripeID(layoutPtr,
	    asmap->raidAddress, &which_ru);
	sectorsPerSU = layoutPtr->sectorsPerStripeUnit;

	if (rf_dagDebug) {
		printf("[Creating degraded read DAG]\n");
	}
	RF_ASSERT(asmap->numDataFailed == 1);
	dag_h->creator = "DegradedReadDAG";

	/*
         * generate two ASMs identifying the surviving data we need
         * in order to recover the lost data
         */

	/* overlappingPDAs array must be zero'd */
	RF_Calloc(overlappingPDAs, asmap->numStripeUnitsAccessed, sizeof(char), (char *));
	rf_GenerateFailedAccessASMs(raidPtr, asmap, failedPDA, dag_h, new_asm_h, &nXorBufs,
	    &rpBuf, overlappingPDAs, allocList);

	/*
         * create all the nodes at once
         *
         * -1 because no access is generated for the failed pda
         */
	nRudNodes = asmap->numStripeUnitsAccessed - 1;
	nRrdNodes = ((new_asm_h[0]) ? new_asm_h[0]->stripeMap->numStripeUnitsAccessed : 0) +
	    ((new_asm_h[1]) ? new_asm_h[1]->stripeMap->numStripeUnitsAccessed : 0);
	nNodes = 5 + nRudNodes + nRrdNodes;	/* lock, unlock, xor, Rp, Rud,
						 * Rrd */
	RF_CallocAndAdd(nodes, nNodes, sizeof(RF_DagNode_t), (RF_DagNode_t *),
	    allocList);
	i = 0;
	blockNode = &nodes[i];
	i++;
	commitNode = &nodes[i];
	i++;
	xorNode = &nodes[i];
	i++;
	rpNode = &nodes[i];
	i++;
	termNode = &nodes[i];
	i++;
	rudNodes = &nodes[i];
	i += nRudNodes;
	rrdNodes = &nodes[i];
	i += nRrdNodes;
	RF_ASSERT(i == nNodes);

	/* initialize nodes */
	dag_h->numCommitNodes = 1;
	dag_h->numCommits = 0;
	/* this dag can not commit until the commit node is reached errors
	 * prior to the commit point imply the dag has failed */
	dag_h->numSuccedents = 1;

	rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
	    NULL, nRudNodes + nRrdNodes + 1, 0, 0, 0, dag_h, "Nil", allocList);
	rf_InitNode(commitNode, rf_wait, RF_TRUE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
	    NULL, 1, 1, 0, 0, dag_h, "Cmt", allocList);
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc,
	    NULL, 0, 1, 0, 0, dag_h, "Trm", allocList);
	rf_InitNode(xorNode, rf_wait, RF_FALSE, recFunc->simple, rf_NullNodeUndoFunc,
	    NULL, 1, nRudNodes + nRrdNodes + 1, 2 * nXorBufs + 2, 1, dag_h,
	    recFunc->SimpleName, allocList);

	/* fill in the Rud nodes */
	for (pda = asmap->physInfo, i = 0; i < nRudNodes; i++, pda = pda->next) {
		if (pda == failedPDA) {
			i--;
			continue;
		}
		rf_InitNode(&rudNodes[i], rf_wait, RF_FALSE, rf_DiskReadFunc,
		    rf_DiskReadUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h,
		    "Rud", allocList);
		RF_ASSERT(pda);
		rudNodes[i].params[0].p = pda;
		rudNodes[i].params[1].p = pda->bufPtr;
		rudNodes[i].params[2].v = parityStripeID;
		rudNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
	}

	/* fill in the Rrd nodes */
	i = 0;
	if (new_asm_h[0]) {
		for (pda = new_asm_h[0]->stripeMap->physInfo;
		    i < new_asm_h[0]->stripeMap->numStripeUnitsAccessed;
		    i++, pda = pda->next) {
			rf_InitNode(&rrdNodes[i], rf_wait, RF_FALSE, rf_DiskReadFunc,
			    rf_DiskReadUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0,
			    dag_h, "Rrd", allocList);
			RF_ASSERT(pda);
			rrdNodes[i].params[0].p = pda;
			rrdNodes[i].params[1].p = pda->bufPtr;
			rrdNodes[i].params[2].v = parityStripeID;
			rrdNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
		}
	}
	if (new_asm_h[1]) {
		for (j = 0, pda = new_asm_h[1]->stripeMap->physInfo;
		    j < new_asm_h[1]->stripeMap->numStripeUnitsAccessed;
		    j++, pda = pda->next) {
			rf_InitNode(&rrdNodes[i + j], rf_wait, RF_FALSE, rf_DiskReadFunc,
			    rf_DiskReadUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0,
			    dag_h, "Rrd", allocList);
			RF_ASSERT(pda);
			rrdNodes[i + j].params[0].p = pda;
			rrdNodes[i + j].params[1].p = pda->bufPtr;
			rrdNodes[i + j].params[2].v = parityStripeID;
			rrdNodes[i + j].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
		}
	}
	/* make a PDA for the parity unit */
	RF_MallocAndAdd(parityPDA, sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t *), allocList);
	parityPDA->row = asmap->parityInfo->row;
	parityPDA->col = asmap->parityInfo->col;
	parityPDA->startSector = ((asmap->parityInfo->startSector / sectorsPerSU)
	    * sectorsPerSU) + (failedPDA->startSector % sectorsPerSU);
	parityPDA->numSector = failedPDA->numSector;

	/* initialize the Rp node */
	rf_InitNode(rpNode, rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc,
	    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Rp ", allocList);
	rpNode->params[0].p = parityPDA;
	rpNode->params[1].p = rpBuf;
	rpNode->params[2].v = parityStripeID;
	rpNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);

	/*
         * the last and nastiest step is to assign all
         * the parameters of the Xor node
         */
	paramNum = 0;
	for (i = 0; i < nRrdNodes; i++) {
		/* all the Rrd nodes need to be xored together */
		xorNode->params[paramNum++] = rrdNodes[i].params[0];
		xorNode->params[paramNum++] = rrdNodes[i].params[1];
	}
	for (i = 0; i < nRudNodes; i++) {
		/* any Rud nodes that overlap the failed access need to be
		 * xored in */
		if (overlappingPDAs[i]) {
			RF_MallocAndAdd(pda, sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t *), allocList);
			bcopy((char *) rudNodes[i].params[0].p, (char *) pda, sizeof(RF_PhysDiskAddr_t));
			rf_RangeRestrictPDA(raidPtr, failedPDA, pda, RF_RESTRICT_DOBUFFER, 0);
			xorNode->params[paramNum++].p = pda;
			xorNode->params[paramNum++].p = pda->bufPtr;
		}
	}
	RF_Free(overlappingPDAs, asmap->numStripeUnitsAccessed * sizeof(char));

	/* install parity pda as last set of params to be xor'd */
	xorNode->params[paramNum++].p = parityPDA;
	xorNode->params[paramNum++].p = rpBuf;

	/*
         * the last 2 params to the recovery xor node are
         * the failed PDA and the raidPtr
         */
	xorNode->params[paramNum++].p = failedPDA;
	xorNode->params[paramNum++].p = raidPtr;
	RF_ASSERT(paramNum == 2 * nXorBufs + 2);

	/*
         * The xor node uses results[0] as the target buffer.
         * Set pointer and zero the buffer. In the kernel, this
         * may be a user buffer in which case we have to remap it.
         */
	xorNode->results[0] = failedPDA->bufPtr;
	RF_BZERO(bp, failedPDA->bufPtr, rf_RaidAddressToByte(raidPtr,
		failedPDA->numSector));

	/* connect nodes to form graph */
	/* connect the header to the block node */
	RF_ASSERT(dag_h->numSuccedents == 1);
	RF_ASSERT(blockNode->numAntecedents == 0);
	dag_h->succedents[0] = blockNode;

	/* connect the block node to the read nodes */
	RF_ASSERT(blockNode->numSuccedents == (1 + nRrdNodes + nRudNodes));
	RF_ASSERT(rpNode->numAntecedents == 1);
	blockNode->succedents[0] = rpNode;
	rpNode->antecedents[0] = blockNode;
	rpNode->antType[0] = rf_control;
	for (i = 0; i < nRrdNodes; i++) {
		RF_ASSERT(rrdNodes[i].numSuccedents == 1);
		blockNode->succedents[1 + i] = &rrdNodes[i];
		rrdNodes[i].antecedents[0] = blockNode;
		rrdNodes[i].antType[0] = rf_control;
	}
	for (i = 0; i < nRudNodes; i++) {
		RF_ASSERT(rudNodes[i].numSuccedents == 1);
		blockNode->succedents[1 + nRrdNodes + i] = &rudNodes[i];
		rudNodes[i].antecedents[0] = blockNode;
		rudNodes[i].antType[0] = rf_control;
	}

	/* connect the read nodes to the xor node */
	RF_ASSERT(xorNode->numAntecedents == (1 + nRrdNodes + nRudNodes));
	RF_ASSERT(rpNode->numSuccedents == 1);
	rpNode->succedents[0] = xorNode;
	xorNode->antecedents[0] = rpNode;
	xorNode->antType[0] = rf_trueData;
	for (i = 0; i < nRrdNodes; i++) {
		RF_ASSERT(rrdNodes[i].numSuccedents == 1);
		rrdNodes[i].succedents[0] = xorNode;
		xorNode->antecedents[1 + i] = &rrdNodes[i];
		xorNode->antType[1 + i] = rf_trueData;
	}
	for (i = 0; i < nRudNodes; i++) {
		RF_ASSERT(rudNodes[i].numSuccedents == 1);
		rudNodes[i].succedents[0] = xorNode;
		xorNode->antecedents[1 + nRrdNodes + i] = &rudNodes[i];
		xorNode->antType[1 + nRrdNodes + i] = rf_trueData;
	}

	/* connect the xor node to the commit node */
	RF_ASSERT(xorNode->numSuccedents == 1);
	RF_ASSERT(commitNode->numAntecedents == 1);
	xorNode->succedents[0] = commitNode;
	commitNode->antecedents[0] = xorNode;
	commitNode->antType[0] = rf_control;

	/* connect the termNode to the commit node */
	RF_ASSERT(commitNode->numSuccedents == 1);
	RF_ASSERT(termNode->numAntecedents == 1);
	RF_ASSERT(termNode->numSuccedents == 0);
	commitNode->succedents[0] = termNode;
	termNode->antType[0] = rf_control;
	termNode->antecedents[0] = commitNode;
}

#if (RF_INCLUDE_CHAINDECLUSTER > 0)
/******************************************************************************
 * Create a degraded read DAG for Chained Declustering
 *
 * Hdr -> Nil -> R(p/s)d -> Cmt -> Trm
 *
 * The "Rd" node reads data from the surviving disk in the mirror pair
 *   Rpd - read of primary copy
 *   Rsd - read of secondary copy
 *
 * Parameters:  raidPtr   - description of the physical array
 *              asmap     - logical & physical addresses for this access
 *              bp        - buffer ptr (for holding write data)
 *              flags     - general flags (e.g. disk locking)
 *              allocList - list of memory allocated in DAG creation
 *****************************************************************************/

void 
rf_CreateRaidCDegradedReadDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList)
{
	RF_DagNode_t *nodes, *rdNode, *blockNode, *commitNode, *termNode;
	RF_StripeNum_t parityStripeID;
	int     useMirror, i, shiftable;
	RF_ReconUnitNum_t which_ru;
	RF_PhysDiskAddr_t *pda;

	if ((asmap->numDataFailed + asmap->numParityFailed) == 0) {
		shiftable = RF_TRUE;
	} else {
		shiftable = RF_FALSE;
	}
	useMirror = 0;
	parityStripeID = rf_RaidAddressToParityStripeID(&(raidPtr->Layout),
	    asmap->raidAddress, &which_ru);

	if (rf_dagDebug) {
		printf("[Creating RAID C degraded read DAG]\n");
	}
	dag_h->creator = "RaidCDegradedReadDAG";
	/* alloc the Wnd nodes and the Wmir node */
	if (asmap->numDataFailed == 0)
		useMirror = RF_FALSE;
	else
		useMirror = RF_TRUE;

	/* total number of nodes = 1 + (block + commit + terminator) */
	RF_CallocAndAdd(nodes, 4, sizeof(RF_DagNode_t), (RF_DagNode_t *), allocList);
	i = 0;
	rdNode = &nodes[i];
	i++;
	blockNode = &nodes[i];
	i++;
	commitNode = &nodes[i];
	i++;
	termNode = &nodes[i];
	i++;

	/*
         * This dag can not commit until the commit node is reached.
         * Errors prior to the commit point imply the dag has failed
         * and must be retried.
         */
	dag_h->numCommitNodes = 1;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	/* initialize the block, commit, and terminator nodes */
	rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
	    NULL, 1, 0, 0, 0, dag_h, "Nil", allocList);
	rf_InitNode(commitNode, rf_wait, RF_TRUE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
	    NULL, 1, 1, 0, 0, dag_h, "Cmt", allocList);
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc,
	    NULL, 0, 1, 0, 0, dag_h, "Trm", allocList);

	pda = asmap->physInfo;
	RF_ASSERT(pda != NULL);
	/* parityInfo must describe entire parity unit */
	RF_ASSERT(asmap->parityInfo->next == NULL);

	/* initialize the data node */
	if (!useMirror) {
		rf_InitNode(rdNode, rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc,
		    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Rpd", allocList);
		if (shiftable && rf_compute_workload_shift(raidPtr, pda)) {
			/* shift this read to the next disk in line */
			rdNode->params[0].p = asmap->parityInfo;
			rdNode->params[1].p = pda->bufPtr;
			rdNode->params[2].v = parityStripeID;
			rdNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
		} else {
			/* read primary copy */
			rdNode->params[0].p = pda;
			rdNode->params[1].p = pda->bufPtr;
			rdNode->params[2].v = parityStripeID;
			rdNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
		}
	} else {
		/* read secondary copy of data */
		rf_InitNode(rdNode, rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc,
		    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Rsd", allocList);
		rdNode->params[0].p = asmap->parityInfo;
		rdNode->params[1].p = pda->bufPtr;
		rdNode->params[2].v = parityStripeID;
		rdNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
	}

	/* connect header to block node */
	RF_ASSERT(dag_h->numSuccedents == 1);
	RF_ASSERT(blockNode->numAntecedents == 0);
	dag_h->succedents[0] = blockNode;

	/* connect block node to rdnode */
	RF_ASSERT(blockNode->numSuccedents == 1);
	RF_ASSERT(rdNode->numAntecedents == 1);
	blockNode->succedents[0] = rdNode;
	rdNode->antecedents[0] = blockNode;
	rdNode->antType[0] = rf_control;

	/* connect rdnode to commit node */
	RF_ASSERT(rdNode->numSuccedents == 1);
	RF_ASSERT(commitNode->numAntecedents == 1);
	rdNode->succedents[0] = commitNode;
	commitNode->antecedents[0] = rdNode;
	commitNode->antType[0] = rf_control;

	/* connect commit node to terminator */
	RF_ASSERT(commitNode->numSuccedents == 1);
	RF_ASSERT(termNode->numAntecedents == 1);
	RF_ASSERT(termNode->numSuccedents == 0);
	commitNode->succedents[0] = termNode;
	termNode->antecedents[0] = commitNode;
	termNode->antType[0] = rf_control;
}
#endif /* (RF_INCLUDE_CHAINDECLUSTER > 0) */

#if (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0) || (RF_INCLUDE_EVENODD > 0)
/*
 * XXX move this elsewhere?
 */
void 
rf_DD_GenerateFailedAccessASMs(
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
	RF_SectorNum_t suoff, suend;
	unsigned firstDataCol, napdas, count;
	RF_SectorNum_t fone_start, fone_end, ftwo_start = 0, ftwo_end = 0;
	RF_PhysDiskAddr_t *fone = asmap->failedPDAs[0], *ftwo = asmap->failedPDAs[1];
	RF_PhysDiskAddr_t *pda_p;
	RF_PhysDiskAddr_t *phys_p;
	RF_RaidAddr_t sosAddr;

	/* determine how many pda's we will have to generate per unaccess
	 * stripe. If there is only one failed data unit, it is one; if two,
	 * possibly two, depending wether they overlap. */

	fone_start = rf_StripeUnitOffset(layoutPtr, fone->startSector);
	fone_end = fone_start + fone->numSector;

#define CONS_PDA(if,start,num) \
  pda_p->row = asmap->if->row;    pda_p->col = asmap->if->col; \
  pda_p->startSector = ((asmap->if->startSector / secPerSU) * secPerSU) + start; \
  pda_p->numSector = num; \
  pda_p->next = NULL; \
  RF_MallocAndAdd(pda_p->bufPtr,rf_RaidAddressToByte(raidPtr,num),(char *), allocList)

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
	napdas = PDAPerDisk * (numDataCol - asmap->numStripeUnitsAccessed - (ftwo == NULL ? 1 : 0));
	*nPQNodep = PDAPerDisk;

	/* sweep over the over accessed pda's, figuring out the number of
	 * additional pda's to generate. Of course, skip the failed ones */

	count = 0;
	for (pda_p = asmap->physInfo; pda_p; pda_p = pda_p->next) {
		if ((pda_p == fone) || (pda_p == ftwo))
			continue;
		suoff = rf_StripeUnitOffset(layoutPtr, pda_p->startSector);
		suend = suoff + pda_p->numSector;
		switch (state) {
		case 1:	/* one failed PDA to overlap */
			/* if a PDA doesn't contain the failed unit, it can
			 * only miss the start or end, not both */
			if ((suoff > fone_start) || (suend < fone_end))
				count++;
			break;
		case 2:	/* whole stripe */
			if (suoff)	/* leak at begining */
				count++;
			if (suend < numDataCol)	/* leak at end */
				count++;
			break;
		case 3:	/* two disjoint units */
			if ((suoff > fone_start) || (suend < fone_end))
				count++;
			if ((suoff > ftwo_start) || (suend < ftwo_end))
				count++;
			break;
		default:
			RF_PANIC();
		}
	}

	napdas += count;
	*nNodep = napdas;
	if (napdas == 0)
		return;		/* short circuit */

	/* allocate up our list of pda's */

	RF_CallocAndAdd(pda_p, napdas, sizeof(RF_PhysDiskAddr_t), (RF_PhysDiskAddr_t *), allocList);
	*pdap = pda_p;

	/* linkem together */
	for (i = 0; i < (napdas - 1); i++)
		pda_p[i].next = pda_p + (i + 1);

	/* march through the one's up to the first accessed disk */
	firstDataCol = rf_RaidAddressToStripeUnitID(&(raidPtr->Layout), asmap->physInfo->raidAddress) % numDataCol;
	sosAddr = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, asmap->raidAddress);
	for (i = 0; i < firstDataCol; i++) {
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

	/* march through the touched stripe units */
	for (phys_p = asmap->physInfo; phys_p; phys_p = phys_p->next, i++) {
		if ((phys_p == asmap->failedPDAs[0]) || (phys_p == asmap->failedPDAs[1]))
			continue;
		suoff = rf_StripeUnitOffset(layoutPtr, phys_p->startSector);
		suend = suoff + phys_p->numSector;
		switch (state) {
		case 1:	/* single buffer */
			if (suoff > fone_start) {
				RF_ASSERT(suend >= fone_end);
				/* The data read starts after the mapped
				 * access, snip off the begining */
				pda_p->numSector = suoff - fone_start;
				pda_p->raidAddress = sosAddr + (i * secPerSU) + fone_start;
				(raidPtr->Layout.map->MapSector) (raidPtr, pda_p->raidAddress, &(pda_p->row), &(pda_p->col), &(pda_p->startSector), 0);
				RF_MallocAndAdd(pda_p->bufPtr, rf_RaidAddressToByte(raidPtr, pda_p->numSector), (char *), allocList);
				pda_p++;
			}
			if (suend < fone_end) {
				RF_ASSERT(suoff <= fone_start);
				/* The data read stops before the end of the
				 * failed access, extend */
				pda_p->numSector = fone_end - suend;
				pda_p->raidAddress = sosAddr + (i * secPerSU) + suend;	/* off by one? */
				(raidPtr->Layout.map->MapSector) (raidPtr, pda_p->raidAddress, &(pda_p->row), &(pda_p->col), &(pda_p->startSector), 0);
				RF_MallocAndAdd(pda_p->bufPtr, rf_RaidAddressToByte(raidPtr, pda_p->numSector), (char *), allocList);
				pda_p++;
			}
			break;
		case 2:	/* whole stripe unit */
			RF_ASSERT((suoff == 0) || (suend == secPerSU));
			if (suend < secPerSU) {	/* short read, snip from end
						 * on */
				pda_p->numSector = secPerSU - suend;
				pda_p->raidAddress = sosAddr + (i * secPerSU) + suend;	/* off by one? */
				(raidPtr->Layout.map->MapSector) (raidPtr, pda_p->raidAddress, &(pda_p->row), &(pda_p->col), &(pda_p->startSector), 0);
				RF_MallocAndAdd(pda_p->bufPtr, rf_RaidAddressToByte(raidPtr, pda_p->numSector), (char *), allocList);
				pda_p++;
			} else
				if (suoff > 0) {	/* short at front */
					pda_p->numSector = suoff;
					pda_p->raidAddress = sosAddr + (i * secPerSU);
					(raidPtr->Layout.map->MapSector) (raidPtr, pda_p->raidAddress, &(pda_p->row), &(pda_p->col), &(pda_p->startSector), 0);
					RF_MallocAndAdd(pda_p->bufPtr, rf_RaidAddressToByte(raidPtr, pda_p->numSector), (char *), allocList);
					pda_p++;
				}
			break;
		case 3:	/* two nonoverlapping failures */
			if ((suoff > fone_start) || (suend < fone_end)) {
				if (suoff > fone_start) {
					RF_ASSERT(suend >= fone_end);
					/* The data read starts after the
					 * mapped access, snip off the
					 * begining */
					pda_p->numSector = suoff - fone_start;
					pda_p->raidAddress = sosAddr + (i * secPerSU) + fone_start;
					(raidPtr->Layout.map->MapSector) (raidPtr, pda_p->raidAddress, &(pda_p->row), &(pda_p->col), &(pda_p->startSector), 0);
					RF_MallocAndAdd(pda_p->bufPtr, rf_RaidAddressToByte(raidPtr, pda_p->numSector), (char *), allocList);
					pda_p++;
				}
				if (suend < fone_end) {
					RF_ASSERT(suoff <= fone_start);
					/* The data read stops before the end
					 * of the failed access, extend */
					pda_p->numSector = fone_end - suend;
					pda_p->raidAddress = sosAddr + (i * secPerSU) + suend;	/* off by one? */
					(raidPtr->Layout.map->MapSector) (raidPtr, pda_p->raidAddress, &(pda_p->row), &(pda_p->col), &(pda_p->startSector), 0);
					RF_MallocAndAdd(pda_p->bufPtr, rf_RaidAddressToByte(raidPtr, pda_p->numSector), (char *), allocList);
					pda_p++;
				}
			}
			if ((suoff > ftwo_start) || (suend < ftwo_end)) {
				if (suoff > ftwo_start) {
					RF_ASSERT(suend >= ftwo_end);
					/* The data read starts after the
					 * mapped access, snip off the
					 * begining */
					pda_p->numSector = suoff - ftwo_start;
					pda_p->raidAddress = sosAddr + (i * secPerSU) + ftwo_start;
					(raidPtr->Layout.map->MapSector) (raidPtr, pda_p->raidAddress, &(pda_p->row), &(pda_p->col), &(pda_p->startSector), 0);
					RF_MallocAndAdd(pda_p->bufPtr, rf_RaidAddressToByte(raidPtr, pda_p->numSector), (char *), allocList);
					pda_p++;
				}
				if (suend < ftwo_end) {
					RF_ASSERT(suoff <= ftwo_start);
					/* The data read stops before the end
					 * of the failed access, extend */
					pda_p->numSector = ftwo_end - suend;
					pda_p->raidAddress = sosAddr + (i * secPerSU) + suend;	/* off by one? */
					(raidPtr->Layout.map->MapSector) (raidPtr, pda_p->raidAddress, &(pda_p->row), &(pda_p->col), &(pda_p->startSector), 0);
					RF_MallocAndAdd(pda_p->bufPtr, rf_RaidAddressToByte(raidPtr, pda_p->numSector), (char *), allocList);
					pda_p++;
				}
			}
			break;
		default:
			RF_PANIC();
		}
	}

	/* after the last accessed disk */
	for (; i < numDataCol; i++) {
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
#define INIT_DISK_NODE(node,name) \
rf_InitNode(node, rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc, rf_GenericWakeupFunc, 2,1,4,0, dag_h, name, allocList); \
(node)->succedents[0] = unblockNode; \
(node)->succedents[1] = recoveryNode; \
(node)->antecedents[0] = blockNode; \
(node)->antType[0] = rf_control

#define DISK_NODE_PARAMS(_node_,_p_) \
  (_node_).params[0].p = _p_ ; \
  (_node_).params[1].p = (_p_)->bufPtr; \
  (_node_).params[2].v = parityStripeID; \
  (_node_).params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru)

void 
rf_DoubleDegRead(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList,
    char *redundantReadNodeName,
    char *recoveryNodeName,
    int (*recovFunc) (RF_DagNode_t *))
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_DagNode_t *nodes, *rudNodes, *rrdNodes, *recoveryNode, *blockNode,
	       *unblockNode, *rpNodes, *rqNodes, *termNode;
	RF_PhysDiskAddr_t *pda, *pqPDAs;
	RF_PhysDiskAddr_t *npdas;
	int     nNodes, nRrdNodes, nRudNodes, i;
	RF_ReconUnitNum_t which_ru;
	int     nReadNodes, nPQNodes;
	RF_PhysDiskAddr_t *failedPDA = asmap->failedPDAs[0];
	RF_PhysDiskAddr_t *failedPDAtwo = asmap->failedPDAs[1];
	RF_StripeNum_t parityStripeID = rf_RaidAddressToParityStripeID(layoutPtr, asmap->raidAddress, &which_ru);

	if (rf_dagDebug)
		printf("[Creating Double Degraded Read DAG]\n");
	rf_DD_GenerateFailedAccessASMs(raidPtr, asmap, &npdas, &nRrdNodes, &pqPDAs, &nPQNodes, allocList);

	nRudNodes = asmap->numStripeUnitsAccessed - (asmap->numDataFailed);
	nReadNodes = nRrdNodes + nRudNodes + 2 * nPQNodes;
	nNodes = 4 /* block, unblock, recovery, term */ + nReadNodes;

	RF_CallocAndAdd(nodes, nNodes, sizeof(RF_DagNode_t), (RF_DagNode_t *), allocList);
	i = 0;
	blockNode = &nodes[i];
	i += 1;
	unblockNode = &nodes[i];
	i += 1;
	recoveryNode = &nodes[i];
	i += 1;
	termNode = &nodes[i];
	i += 1;
	rudNodes = &nodes[i];
	i += nRudNodes;
	rrdNodes = &nodes[i];
	i += nRrdNodes;
	rpNodes = &nodes[i];
	i += nPQNodes;
	rqNodes = &nodes[i];
	i += nPQNodes;
	RF_ASSERT(i == nNodes);

	dag_h->numSuccedents = 1;
	dag_h->succedents[0] = blockNode;
	dag_h->creator = "DoubleDegRead";
	dag_h->numCommits = 0;
	dag_h->numCommitNodes = 1;	/* unblock */

	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc, NULL, 0, 2, 0, 0, dag_h, "Trm", allocList);
	termNode->antecedents[0] = unblockNode;
	termNode->antType[0] = rf_control;
	termNode->antecedents[1] = recoveryNode;
	termNode->antType[1] = rf_control;

	/* init the block and unblock nodes */
	/* The block node has all nodes except itself, unblock and recovery as
	 * successors. Similarly for predecessors of the unblock. */
	rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, nReadNodes, 0, 0, 0, dag_h, "Nil", allocList);
	rf_InitNode(unblockNode, rf_wait, RF_TRUE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, 1, nReadNodes, 0, 0, dag_h, "Nil", allocList);

	for (i = 0; i < nReadNodes; i++) {
		blockNode->succedents[i] = rudNodes + i;
		unblockNode->antecedents[i] = rudNodes + i;
		unblockNode->antType[i] = rf_control;
	}
	unblockNode->succedents[0] = termNode;

	/* The recovery node has all the reads as predecessors, and the term
	 * node as successors. It gets a pda as a param from each of the read
	 * nodes plus the raidPtr. For each failed unit is has a result pda. */
	rf_InitNode(recoveryNode, rf_wait, RF_FALSE, recovFunc, rf_NullNodeUndoFunc, NULL,
	    1,			/* succesors */
	    nReadNodes,		/* preds */
	    nReadNodes + 2,	/* params */
	    asmap->numDataFailed,	/* results */
	    dag_h, recoveryNodeName, allocList);

	recoveryNode->succedents[0] = termNode;
	for (i = 0; i < nReadNodes; i++) {
		recoveryNode->antecedents[i] = rudNodes + i;
		recoveryNode->antType[i] = rf_trueData;
	}

	/* build the read nodes, then come back and fill in recovery params
	 * and results */
	pda = asmap->physInfo;
	for (i = 0; i < nRudNodes; pda = pda->next) {
		if ((pda == failedPDA) || (pda == failedPDAtwo))
			continue;
		INIT_DISK_NODE(rudNodes + i, "Rud");
		RF_ASSERT(pda);
		DISK_NODE_PARAMS(rudNodes[i], pda);
		i++;
	}

	pda = npdas;
	for (i = 0; i < nRrdNodes; i++, pda = pda->next) {
		INIT_DISK_NODE(rrdNodes + i, "Rrd");
		RF_ASSERT(pda);
		DISK_NODE_PARAMS(rrdNodes[i], pda);
	}

	/* redundancy pdas */
	pda = pqPDAs;
	INIT_DISK_NODE(rpNodes, "Rp");
	RF_ASSERT(pda);
	DISK_NODE_PARAMS(rpNodes[0], pda);
	pda++;
	INIT_DISK_NODE(rqNodes, redundantReadNodeName);
	RF_ASSERT(pda);
	DISK_NODE_PARAMS(rqNodes[0], pda);
	if (nPQNodes == 2) {
		pda++;
		INIT_DISK_NODE(rpNodes + 1, "Rp");
		RF_ASSERT(pda);
		DISK_NODE_PARAMS(rpNodes[1], pda);
		pda++;
		INIT_DISK_NODE(rqNodes + 1, redundantReadNodeName);
		RF_ASSERT(pda);
		DISK_NODE_PARAMS(rqNodes[1], pda);
	}
	/* fill in recovery node params */
	for (i = 0; i < nReadNodes; i++)
		recoveryNode->params[i] = rudNodes[i].params[0];	/* pda */
	recoveryNode->params[i++].p = (void *) raidPtr;
	recoveryNode->params[i++].p = (void *) asmap;
	recoveryNode->results[0] = failedPDA;
	if (asmap->numDataFailed == 2)
		recoveryNode->results[1] = failedPDAtwo;

	/* zero fill the target data buffers? */
}

#endif /* (RF_INCLUDE_DECL_PQ > 0) || (RF_INCLUDE_RAID6 > 0) || (RF_INCLUDE_EVENODD > 0) */
