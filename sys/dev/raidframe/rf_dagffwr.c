/*	$FreeBSD$ */
/*	$NetBSD: rf_dagffwr.c,v 1.5 2000/01/07 03:40:58 oster Exp $	*/
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
 * rf_dagff.c
 *
 * code for creating fault-free DAGs
 *
 */

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_dagfuncs.h>
#include <dev/raidframe/rf_debugMem.h>
#include <dev/raidframe/rf_dagffrd.h>
#include <dev/raidframe/rf_memchunk.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_dagffwr.h>

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
rf_CreateNonRedundantWriteDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList,
    RF_IoType_t type)
{
	rf_CreateNonredundantDAG(raidPtr, asmap, dag_h, bp, flags, allocList,
	    RF_IO_TYPE_WRITE);
}

void 
rf_CreateRAID0WriteDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList,
    RF_IoType_t type)
{
	rf_CreateNonredundantDAG(raidPtr, asmap, dag_h, bp, flags, allocList,
	    RF_IO_TYPE_WRITE);
}

void 
rf_CreateSmallWriteDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList)
{
	/* "normal" rollaway */
	rf_CommonCreateSmallWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList,
	    &rf_xorFuncs, NULL);
}

void 
rf_CreateLargeWriteDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList)
{
	/* "normal" rollaway */
	rf_CommonCreateLargeWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList,
	    1, rf_RegularXorFunc, RF_TRUE);
}


/******************************************************************************
 *
 * DAG creation code begins here
 */


/******************************************************************************
 *
 * creates a DAG to perform a large-write operation:
 *
 *           / Rod \           / Wnd \
 * H -- block- Rod - Xor - Cmt - Wnd --- T
 *           \ Rod /          \  Wnp /
 *                             \[Wnq]/
 *
 * The XOR node also does the Q calculation in the P+Q architecture.
 * All nodes are before the commit node (Cmt) are assumed to be atomic and
 * undoable - or - they make no changes to permanent state.
 *
 * Rod = read old data
 * Cmt = commit node
 * Wnp = write new parity
 * Wnd = write new data
 * Wnq = write new "q"
 * [] denotes optional segments in the graph
 *
 * Parameters:  raidPtr   - description of the physical array
 *              asmap     - logical & physical addresses for this access
 *              bp        - buffer ptr (holds write data)
 *              flags     - general flags (e.g. disk locking)
 *              allocList - list of memory allocated in DAG creation
 *              nfaults   - number of faults array can tolerate
 *                          (equal to # redundancy units in stripe)
 *              redfuncs  - list of redundancy generating functions
 *
 *****************************************************************************/

void 
rf_CommonCreateLargeWriteDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList,
    int nfaults,
    int (*redFunc) (RF_DagNode_t *),
    int allowBufferRecycle)
{
	RF_DagNode_t *nodes, *wndNodes, *rodNodes, *xorNode, *wnpNode;
	RF_DagNode_t *wnqNode, *blockNode, *commitNode, *termNode;
	int     nWndNodes, nRodNodes, i, nodeNum, asmNum;
	RF_AccessStripeMapHeader_t *new_asm_h[2];
	RF_StripeNum_t parityStripeID;
	char   *sosBuffer, *eosBuffer;
	RF_ReconUnitNum_t which_ru;
	RF_RaidLayout_t *layoutPtr;
	RF_PhysDiskAddr_t *pda;

	layoutPtr = &(raidPtr->Layout);
	parityStripeID = rf_RaidAddressToParityStripeID(layoutPtr, asmap->raidAddress,
	    &which_ru);

	if (rf_dagDebug) {
		printf("[Creating large-write DAG]\n");
	}
	dag_h->creator = "LargeWriteDAG";

	dag_h->numCommitNodes = 1;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	/* alloc the nodes: Wnd, xor, commit, block, term, and  Wnp */
	nWndNodes = asmap->numStripeUnitsAccessed;
	RF_CallocAndAdd(nodes, nWndNodes + 4 + nfaults, sizeof(RF_DagNode_t),
	    (RF_DagNode_t *), allocList);
	i = 0;
	wndNodes = &nodes[i];
	i += nWndNodes;
	xorNode = &nodes[i];
	i += 1;
	wnpNode = &nodes[i];
	i += 1;
	blockNode = &nodes[i];
	i += 1;
	commitNode = &nodes[i];
	i += 1;
	termNode = &nodes[i];
	i += 1;
	if (nfaults == 2) {
		wnqNode = &nodes[i];
		i += 1;
	} else {
		wnqNode = NULL;
	}
	rf_MapUnaccessedPortionOfStripe(raidPtr, layoutPtr, asmap, dag_h, new_asm_h,
	    &nRodNodes, &sosBuffer, &eosBuffer, allocList);
	if (nRodNodes > 0) {
		RF_CallocAndAdd(rodNodes, nRodNodes, sizeof(RF_DagNode_t),
		    (RF_DagNode_t *), allocList);
	} else {
		rodNodes = NULL;
	}

	/* begin node initialization */
	if (nRodNodes > 0) {
		rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
		    NULL, nRodNodes, 0, 0, 0, dag_h, "Nil", allocList);
	} else {
		rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
		    NULL, 1, 0, 0, 0, dag_h, "Nil", allocList);
	}

	rf_InitNode(commitNode, rf_wait, RF_TRUE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL,
	    nWndNodes + nfaults, 1, 0, 0, dag_h, "Cmt", allocList);
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc, NULL,
	    0, nWndNodes + nfaults, 0, 0, dag_h, "Trm", allocList);

	/* initialize the Rod nodes */
	for (nodeNum = asmNum = 0; asmNum < 2; asmNum++) {
		if (new_asm_h[asmNum]) {
			pda = new_asm_h[asmNum]->stripeMap->physInfo;
			while (pda) {
				rf_InitNode(&rodNodes[nodeNum], rf_wait, RF_FALSE, rf_DiskReadFunc,
				    rf_DiskReadUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h,
				    "Rod", allocList);
				rodNodes[nodeNum].params[0].p = pda;
				rodNodes[nodeNum].params[1].p = pda->bufPtr;
				rodNodes[nodeNum].params[2].v = parityStripeID;
				rodNodes[nodeNum].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
				    0, 0, which_ru);
				nodeNum++;
				pda = pda->next;
			}
		}
	}
	RF_ASSERT(nodeNum == nRodNodes);

	/* initialize the wnd nodes */
	pda = asmap->physInfo;
	for (i = 0; i < nWndNodes; i++) {
		rf_InitNode(&wndNodes[i], rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
		    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Wnd", allocList);
		RF_ASSERT(pda != NULL);
		wndNodes[i].params[0].p = pda;
		wndNodes[i].params[1].p = pda->bufPtr;
		wndNodes[i].params[2].v = parityStripeID;
		wndNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
		pda = pda->next;
	}

	/* initialize the redundancy node */
	if (nRodNodes > 0) {
		rf_InitNode(xorNode, rf_wait, RF_FALSE, redFunc, rf_NullNodeUndoFunc, NULL, 1,
		    nRodNodes, 2 * (nWndNodes + nRodNodes) + 1, nfaults, dag_h,
		    "Xr ", allocList);
	} else {
		rf_InitNode(xorNode, rf_wait, RF_FALSE, redFunc, rf_NullNodeUndoFunc, NULL, 1,
		    1, 2 * (nWndNodes + nRodNodes) + 1, nfaults, dag_h, "Xr ", allocList);
	}
	xorNode->flags |= RF_DAGNODE_FLAG_YIELD;
	for (i = 0; i < nWndNodes; i++) {
		xorNode->params[2 * i + 0] = wndNodes[i].params[0];	/* pda */
		xorNode->params[2 * i + 1] = wndNodes[i].params[1];	/* buf ptr */
	}
	for (i = 0; i < nRodNodes; i++) {
		xorNode->params[2 * (nWndNodes + i) + 0] = rodNodes[i].params[0];	/* pda */
		xorNode->params[2 * (nWndNodes + i) + 1] = rodNodes[i].params[1];	/* buf ptr */
	}
	/* xor node needs to get at RAID information */
	xorNode->params[2 * (nWndNodes + nRodNodes)].p = raidPtr;

	/*
         * Look for an Rod node that reads a complete SU. If none, alloc a buffer
         * to receive the parity info. Note that we can't use a new data buffer
         * because it will not have gotten written when the xor occurs.
         */
	if (allowBufferRecycle) {
		for (i = 0; i < nRodNodes; i++) {
			if (((RF_PhysDiskAddr_t *) rodNodes[i].params[0].p)->numSector == raidPtr->Layout.sectorsPerStripeUnit)
				break;
		}
	}
	if ((!allowBufferRecycle) || (i == nRodNodes)) {
		RF_CallocAndAdd(xorNode->results[0], 1,
		    rf_RaidAddressToByte(raidPtr, raidPtr->Layout.sectorsPerStripeUnit),
		    (void *), allocList);
	} else {
		xorNode->results[0] = rodNodes[i].params[1].p;
	}

	/* initialize the Wnp node */
	rf_InitNode(wnpNode, rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
	    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Wnp", allocList);
	wnpNode->params[0].p = asmap->parityInfo;
	wnpNode->params[1].p = xorNode->results[0];
	wnpNode->params[2].v = parityStripeID;
	wnpNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
	/* parityInfo must describe entire parity unit */
	RF_ASSERT(asmap->parityInfo->next == NULL);

	if (nfaults == 2) {
		/*
	         * We never try to recycle a buffer for the Q calcuation
	         * in addition to the parity. This would cause two buffers
	         * to get smashed during the P and Q calculation, guaranteeing
	         * one would be wrong.
	         */
		RF_CallocAndAdd(xorNode->results[1], 1,
		    rf_RaidAddressToByte(raidPtr, raidPtr->Layout.sectorsPerStripeUnit),
		    (void *), allocList);
		rf_InitNode(wnqNode, rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
		    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Wnq", allocList);
		wnqNode->params[0].p = asmap->qInfo;
		wnqNode->params[1].p = xorNode->results[1];
		wnqNode->params[2].v = parityStripeID;
		wnqNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
		/* parityInfo must describe entire parity unit */
		RF_ASSERT(asmap->parityInfo->next == NULL);
	}
	/*
         * Connect nodes to form graph.
         */

	/* connect dag header to block node */
	RF_ASSERT(blockNode->numAntecedents == 0);
	dag_h->succedents[0] = blockNode;

	if (nRodNodes > 0) {
		/* connect the block node to the Rod nodes */
		RF_ASSERT(blockNode->numSuccedents == nRodNodes);
		RF_ASSERT(xorNode->numAntecedents == nRodNodes);
		for (i = 0; i < nRodNodes; i++) {
			RF_ASSERT(rodNodes[i].numAntecedents == 1);
			blockNode->succedents[i] = &rodNodes[i];
			rodNodes[i].antecedents[0] = blockNode;
			rodNodes[i].antType[0] = rf_control;

			/* connect the Rod nodes to the Xor node */
			RF_ASSERT(rodNodes[i].numSuccedents == 1);
			rodNodes[i].succedents[0] = xorNode;
			xorNode->antecedents[i] = &rodNodes[i];
			xorNode->antType[i] = rf_trueData;
		}
	} else {
		/* connect the block node to the Xor node */
		RF_ASSERT(blockNode->numSuccedents == 1);
		RF_ASSERT(xorNode->numAntecedents == 1);
		blockNode->succedents[0] = xorNode;
		xorNode->antecedents[0] = blockNode;
		xorNode->antType[0] = rf_control;
	}

	/* connect the xor node to the commit node */
	RF_ASSERT(xorNode->numSuccedents == 1);
	RF_ASSERT(commitNode->numAntecedents == 1);
	xorNode->succedents[0] = commitNode;
	commitNode->antecedents[0] = xorNode;
	commitNode->antType[0] = rf_control;

	/* connect the commit node to the write nodes */
	RF_ASSERT(commitNode->numSuccedents == nWndNodes + nfaults);
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(wndNodes->numAntecedents == 1);
		commitNode->succedents[i] = &wndNodes[i];
		wndNodes[i].antecedents[0] = commitNode;
		wndNodes[i].antType[0] = rf_control;
	}
	RF_ASSERT(wnpNode->numAntecedents == 1);
	commitNode->succedents[nWndNodes] = wnpNode;
	wnpNode->antecedents[0] = commitNode;
	wnpNode->antType[0] = rf_trueData;
	if (nfaults == 2) {
		RF_ASSERT(wnqNode->numAntecedents == 1);
		commitNode->succedents[nWndNodes + 1] = wnqNode;
		wnqNode->antecedents[0] = commitNode;
		wnqNode->antType[0] = rf_trueData;
	}
	/* connect the write nodes to the term node */
	RF_ASSERT(termNode->numAntecedents == nWndNodes + nfaults);
	RF_ASSERT(termNode->numSuccedents == 0);
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(wndNodes->numSuccedents == 1);
		wndNodes[i].succedents[0] = termNode;
		termNode->antecedents[i] = &wndNodes[i];
		termNode->antType[i] = rf_control;
	}
	RF_ASSERT(wnpNode->numSuccedents == 1);
	wnpNode->succedents[0] = termNode;
	termNode->antecedents[nWndNodes] = wnpNode;
	termNode->antType[nWndNodes] = rf_control;
	if (nfaults == 2) {
		RF_ASSERT(wnqNode->numSuccedents == 1);
		wnqNode->succedents[0] = termNode;
		termNode->antecedents[nWndNodes + 1] = wnqNode;
		termNode->antType[nWndNodes + 1] = rf_control;
	}
}
/******************************************************************************
 *
 * creates a DAG to perform a small-write operation (either raid 5 or pq),
 * which is as follows:
 *
 * Hdr -> Nil -> Rop -> Xor -> Cmt ----> Wnp [Unp] --> Trm
 *            \- Rod X      /     \----> Wnd [Und]-/
 *           [\- Rod X     /       \---> Wnd [Und]-/]
 *           [\- Roq -> Q /         \--> Wnq [Unq]-/]
 *
 * Rop = read old parity
 * Rod = read old data
 * Roq = read old "q"
 * Cmt = commit node
 * Und = unlock data disk
 * Unp = unlock parity disk
 * Unq = unlock q disk
 * Wnp = write new parity
 * Wnd = write new data
 * Wnq = write new "q"
 * [ ] denotes optional segments in the graph
 *
 * Parameters:  raidPtr   - description of the physical array
 *              asmap     - logical & physical addresses for this access
 *              bp        - buffer ptr (holds write data)
 *              flags     - general flags (e.g. disk locking)
 *              allocList - list of memory allocated in DAG creation
 *              pfuncs    - list of parity generating functions
 *              qfuncs    - list of q generating functions
 *
 * A null qfuncs indicates single fault tolerant
 *****************************************************************************/

void 
rf_CommonCreateSmallWriteDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList,
    RF_RedFuncs_t * pfuncs,
    RF_RedFuncs_t * qfuncs)
{
	RF_DagNode_t *readDataNodes, *readParityNodes, *readQNodes, *termNode;
	RF_DagNode_t *unlockDataNodes, *unlockParityNodes, *unlockQNodes;
	RF_DagNode_t *xorNodes, *qNodes, *blockNode, *commitNode, *nodes;
	RF_DagNode_t *writeDataNodes, *writeParityNodes, *writeQNodes;
	int     i, j, nNodes, totalNumNodes, lu_flag;
	RF_ReconUnitNum_t which_ru;
	int     (*func) (RF_DagNode_t *), (*undoFunc) (RF_DagNode_t *);
	int     (*qfunc) (RF_DagNode_t *);
	int     numDataNodes, numParityNodes;
	RF_StripeNum_t parityStripeID;
	RF_PhysDiskAddr_t *pda;
	char   *name, *qname;
	long    nfaults;

	nfaults = qfuncs ? 2 : 1;
	lu_flag = (rf_enableAtomicRMW) ? 1 : 0;	/* lock/unlock flag */

	parityStripeID = rf_RaidAddressToParityStripeID(&(raidPtr->Layout),
	    asmap->raidAddress, &which_ru);
	pda = asmap->physInfo;
	numDataNodes = asmap->numStripeUnitsAccessed;
	numParityNodes = (asmap->parityInfo->next) ? 2 : 1;

	if (rf_dagDebug) {
		printf("[Creating small-write DAG]\n");
	}
	RF_ASSERT(numDataNodes > 0);
	dag_h->creator = "SmallWriteDAG";

	dag_h->numCommitNodes = 1;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	/*
         * DAG creation occurs in four steps:
         * 1. count the number of nodes in the DAG
         * 2. create the nodes
         * 3. initialize the nodes
         * 4. connect the nodes
         */

	/*
         * Step 1. compute number of nodes in the graph
         */

	/* number of nodes: a read and write for each data unit a redundancy
	 * computation node for each parity node (nfaults * nparity) a read
	 * and write for each parity unit a block and commit node (2) a
	 * terminate node if atomic RMW an unlock node for each data unit,
	 * redundancy unit */
	totalNumNodes = (2 * numDataNodes) + (nfaults * numParityNodes)
	    + (nfaults * 2 * numParityNodes) + 3;
	if (lu_flag) {
		totalNumNodes += (numDataNodes + (nfaults * numParityNodes));
	}
	/*
         * Step 2. create the nodes
         */
	RF_CallocAndAdd(nodes, totalNumNodes, sizeof(RF_DagNode_t),
	    (RF_DagNode_t *), allocList);
	i = 0;
	blockNode = &nodes[i];
	i += 1;
	commitNode = &nodes[i];
	i += 1;
	readDataNodes = &nodes[i];
	i += numDataNodes;
	readParityNodes = &nodes[i];
	i += numParityNodes;
	writeDataNodes = &nodes[i];
	i += numDataNodes;
	writeParityNodes = &nodes[i];
	i += numParityNodes;
	xorNodes = &nodes[i];
	i += numParityNodes;
	termNode = &nodes[i];
	i += 1;
	if (lu_flag) {
		unlockDataNodes = &nodes[i];
		i += numDataNodes;
		unlockParityNodes = &nodes[i];
		i += numParityNodes;
	} else {
		unlockDataNodes = unlockParityNodes = NULL;
	}
	if (nfaults == 2) {
		readQNodes = &nodes[i];
		i += numParityNodes;
		writeQNodes = &nodes[i];
		i += numParityNodes;
		qNodes = &nodes[i];
		i += numParityNodes;
		if (lu_flag) {
			unlockQNodes = &nodes[i];
			i += numParityNodes;
		} else {
			unlockQNodes = NULL;
		}
	} else {
		readQNodes = writeQNodes = qNodes = unlockQNodes = NULL;
	}
	RF_ASSERT(i == totalNumNodes);

	/*
         * Step 3. initialize the nodes
         */
	/* initialize block node (Nil) */
	nNodes = numDataNodes + (nfaults * numParityNodes);
	rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
	    NULL, nNodes, 0, 0, 0, dag_h, "Nil", allocList);

	/* initialize commit node (Cmt) */
	rf_InitNode(commitNode, rf_wait, RF_TRUE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
	    NULL, nNodes, (nfaults * numParityNodes), 0, 0, dag_h, "Cmt", allocList);

	/* initialize terminate node (Trm) */
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc,
	    NULL, 0, nNodes, 0, 0, dag_h, "Trm", allocList);

	/* initialize nodes which read old data (Rod) */
	for (i = 0; i < numDataNodes; i++) {
		rf_InitNode(&readDataNodes[i], rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc,
		    rf_GenericWakeupFunc, (nfaults * numParityNodes), 1, 4, 0, dag_h,
		    "Rod", allocList);
		RF_ASSERT(pda != NULL);
		/* physical disk addr desc */
		readDataNodes[i].params[0].p = pda;
		/* buffer to hold old data */
		readDataNodes[i].params[1].p = rf_AllocBuffer(raidPtr,
		    dag_h, pda, allocList);
		readDataNodes[i].params[2].v = parityStripeID;
		readDataNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
		    lu_flag, 0, which_ru);
		pda = pda->next;
		for (j = 0; j < readDataNodes[i].numSuccedents; j++) {
			readDataNodes[i].propList[j] = NULL;
		}
	}

	/* initialize nodes which read old parity (Rop) */
	pda = asmap->parityInfo;
	i = 0;
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(pda != NULL);
		rf_InitNode(&readParityNodes[i], rf_wait, RF_FALSE, rf_DiskReadFunc,
		    rf_DiskReadUndoFunc, rf_GenericWakeupFunc, numParityNodes, 1, 4,
		    0, dag_h, "Rop", allocList);
		readParityNodes[i].params[0].p = pda;
		/* buffer to hold old parity */
		readParityNodes[i].params[1].p = rf_AllocBuffer(raidPtr,
		    dag_h, pda, allocList);
		readParityNodes[i].params[2].v = parityStripeID;
		readParityNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
		    lu_flag, 0, which_ru);
		pda = pda->next;
		for (j = 0; j < readParityNodes[i].numSuccedents; j++) {
			readParityNodes[i].propList[0] = NULL;
		}
	}

	/* initialize nodes which read old Q (Roq) */
	if (nfaults == 2) {
		pda = asmap->qInfo;
		for (i = 0; i < numParityNodes; i++) {
			RF_ASSERT(pda != NULL);
			rf_InitNode(&readQNodes[i], rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc,
			    rf_GenericWakeupFunc, numParityNodes, 1, 4, 0, dag_h, "Roq", allocList);
			readQNodes[i].params[0].p = pda;
			/* buffer to hold old Q */
			readQNodes[i].params[1].p = rf_AllocBuffer(raidPtr, dag_h, pda,
			    allocList);
			readQNodes[i].params[2].v = parityStripeID;
			readQNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
			    lu_flag, 0, which_ru);
			pda = pda->next;
			for (j = 0; j < readQNodes[i].numSuccedents; j++) {
				readQNodes[i].propList[0] = NULL;
			}
		}
	}
	/* initialize nodes which write new data (Wnd) */
	pda = asmap->physInfo;
	for (i = 0; i < numDataNodes; i++) {
		RF_ASSERT(pda != NULL);
		rf_InitNode(&writeDataNodes[i], rf_wait, RF_FALSE, rf_DiskWriteFunc,
		    rf_DiskWriteUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h,
		    "Wnd", allocList);
		/* physical disk addr desc */
		writeDataNodes[i].params[0].p = pda;
		/* buffer holding new data to be written */
		writeDataNodes[i].params[1].p = pda->bufPtr;
		writeDataNodes[i].params[2].v = parityStripeID;
		writeDataNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
		    0, 0, which_ru);
		if (lu_flag) {
			/* initialize node to unlock the disk queue */
			rf_InitNode(&unlockDataNodes[i], rf_wait, RF_FALSE, rf_DiskUnlockFunc,
			    rf_DiskUnlockUndoFunc, rf_GenericWakeupFunc, 1, 1, 2, 0, dag_h,
			    "Und", allocList);
			/* physical disk addr desc */
			unlockDataNodes[i].params[0].p = pda;
			unlockDataNodes[i].params[1].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
			    0, lu_flag, which_ru);
		}
		pda = pda->next;
	}

	/*
         * Initialize nodes which compute new parity and Q.
         */
	/*
         * We use the simple XOR func in the double-XOR case, and when
         * we're accessing only a portion of one stripe unit. The distinction
         * between the two is that the regular XOR func assumes that the targbuf
         * is a full SU in size, and examines the pda associated with the buffer
         * to decide where within the buffer to XOR the data, whereas
         * the simple XOR func just XORs the data into the start of the buffer.
         */
	if ((numParityNodes == 2) || ((numDataNodes == 1)
		&& (asmap->totalSectorsAccessed < raidPtr->Layout.sectorsPerStripeUnit))) {
		func = pfuncs->simple;
		undoFunc = rf_NullNodeUndoFunc;
		name = pfuncs->SimpleName;
		if (qfuncs) {
			qfunc = qfuncs->simple;
			qname = qfuncs->SimpleName;
		} else {
			qfunc = NULL;
			qname = NULL;
		}
	} else {
		func = pfuncs->regular;
		undoFunc = rf_NullNodeUndoFunc;
		name = pfuncs->RegularName;
		if (qfuncs) {
			qfunc = qfuncs->regular;
			qname = qfuncs->RegularName;
		} else {
			qfunc = NULL;
			qname = NULL;
		}
	}
	/*
         * Initialize the xor nodes: params are {pda,buf}
         * from {Rod,Wnd,Rop} nodes, and raidPtr
         */
	if (numParityNodes == 2) {
		/* double-xor case */
		for (i = 0; i < numParityNodes; i++) {
			/* note: no wakeup func for xor */
			rf_InitNode(&xorNodes[i], rf_wait, RF_FALSE, func, undoFunc, NULL,
			    1, (numDataNodes + numParityNodes), 7, 1, dag_h, name, allocList);
			xorNodes[i].flags |= RF_DAGNODE_FLAG_YIELD;
			xorNodes[i].params[0] = readDataNodes[i].params[0];
			xorNodes[i].params[1] = readDataNodes[i].params[1];
			xorNodes[i].params[2] = readParityNodes[i].params[0];
			xorNodes[i].params[3] = readParityNodes[i].params[1];
			xorNodes[i].params[4] = writeDataNodes[i].params[0];
			xorNodes[i].params[5] = writeDataNodes[i].params[1];
			xorNodes[i].params[6].p = raidPtr;
			/* use old parity buf as target buf */
			xorNodes[i].results[0] = readParityNodes[i].params[1].p;
			if (nfaults == 2) {
				/* note: no wakeup func for qor */
				rf_InitNode(&qNodes[i], rf_wait, RF_FALSE, qfunc, undoFunc, NULL, 1,
				    (numDataNodes + numParityNodes), 7, 1, dag_h, qname, allocList);
				qNodes[i].params[0] = readDataNodes[i].params[0];
				qNodes[i].params[1] = readDataNodes[i].params[1];
				qNodes[i].params[2] = readQNodes[i].params[0];
				qNodes[i].params[3] = readQNodes[i].params[1];
				qNodes[i].params[4] = writeDataNodes[i].params[0];
				qNodes[i].params[5] = writeDataNodes[i].params[1];
				qNodes[i].params[6].p = raidPtr;
				/* use old Q buf as target buf */
				qNodes[i].results[0] = readQNodes[i].params[1].p;
			}
		}
	} else {
		/* there is only one xor node in this case */
		rf_InitNode(&xorNodes[0], rf_wait, RF_FALSE, func, undoFunc, NULL, 1,
		    (numDataNodes + numParityNodes),
		    (2 * (numDataNodes + numDataNodes + 1) + 1), 1, dag_h, name, allocList);
		xorNodes[0].flags |= RF_DAGNODE_FLAG_YIELD;
		for (i = 0; i < numDataNodes + 1; i++) {
			/* set up params related to Rod and Rop nodes */
			xorNodes[0].params[2 * i + 0] = readDataNodes[i].params[0];	/* pda */
			xorNodes[0].params[2 * i + 1] = readDataNodes[i].params[1];	/* buffer ptr */
		}
		for (i = 0; i < numDataNodes; i++) {
			/* set up params related to Wnd and Wnp nodes */
			xorNodes[0].params[2 * (numDataNodes + 1 + i) + 0] =	/* pda */
			    writeDataNodes[i].params[0];
			xorNodes[0].params[2 * (numDataNodes + 1 + i) + 1] =	/* buffer ptr */
			    writeDataNodes[i].params[1];
		}
		/* xor node needs to get at RAID information */
		xorNodes[0].params[2 * (numDataNodes + numDataNodes + 1)].p = raidPtr;
		xorNodes[0].results[0] = readParityNodes[0].params[1].p;
		if (nfaults == 2) {
			rf_InitNode(&qNodes[0], rf_wait, RF_FALSE, qfunc, undoFunc, NULL, 1,
			    (numDataNodes + numParityNodes),
			    (2 * (numDataNodes + numDataNodes + 1) + 1), 1, dag_h,
			    qname, allocList);
			for (i = 0; i < numDataNodes; i++) {
				/* set up params related to Rod */
				qNodes[0].params[2 * i + 0] = readDataNodes[i].params[0];	/* pda */
				qNodes[0].params[2 * i + 1] = readDataNodes[i].params[1];	/* buffer ptr */
			}
			/* and read old q */
			qNodes[0].params[2 * numDataNodes + 0] =	/* pda */
			    readQNodes[0].params[0];
			qNodes[0].params[2 * numDataNodes + 1] =	/* buffer ptr */
			    readQNodes[0].params[1];
			for (i = 0; i < numDataNodes; i++) {
				/* set up params related to Wnd nodes */
				qNodes[0].params[2 * (numDataNodes + 1 + i) + 0] =	/* pda */
				    writeDataNodes[i].params[0];
				qNodes[0].params[2 * (numDataNodes + 1 + i) + 1] =	/* buffer ptr */
				    writeDataNodes[i].params[1];
			}
			/* xor node needs to get at RAID information */
			qNodes[0].params[2 * (numDataNodes + numDataNodes + 1)].p = raidPtr;
			qNodes[0].results[0] = readQNodes[0].params[1].p;
		}
	}

	/* initialize nodes which write new parity (Wnp) */
	pda = asmap->parityInfo;
	for (i = 0; i < numParityNodes; i++) {
		rf_InitNode(&writeParityNodes[i], rf_wait, RF_FALSE, rf_DiskWriteFunc,
		    rf_DiskWriteUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h,
		    "Wnp", allocList);
		RF_ASSERT(pda != NULL);
		writeParityNodes[i].params[0].p = pda;	/* param 1 (bufPtr)
							 * filled in by xor node */
		writeParityNodes[i].params[1].p = xorNodes[i].results[0];	/* buffer pointer for
										 * parity write
										 * operation */
		writeParityNodes[i].params[2].v = parityStripeID;
		writeParityNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
		    0, 0, which_ru);
		if (lu_flag) {
			/* initialize node to unlock the disk queue */
			rf_InitNode(&unlockParityNodes[i], rf_wait, RF_FALSE, rf_DiskUnlockFunc,
			    rf_DiskUnlockUndoFunc, rf_GenericWakeupFunc, 1, 1, 2, 0, dag_h,
			    "Unp", allocList);
			unlockParityNodes[i].params[0].p = pda;	/* physical disk addr
								 * desc */
			unlockParityNodes[i].params[1].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
			    0, lu_flag, which_ru);
		}
		pda = pda->next;
	}

	/* initialize nodes which write new Q (Wnq) */
	if (nfaults == 2) {
		pda = asmap->qInfo;
		for (i = 0; i < numParityNodes; i++) {
			rf_InitNode(&writeQNodes[i], rf_wait, RF_FALSE, rf_DiskWriteFunc,
			    rf_DiskWriteUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h,
			    "Wnq", allocList);
			RF_ASSERT(pda != NULL);
			writeQNodes[i].params[0].p = pda;	/* param 1 (bufPtr)
								 * filled in by xor node */
			writeQNodes[i].params[1].p = qNodes[i].results[0];	/* buffer pointer for
										 * parity write
										 * operation */
			writeQNodes[i].params[2].v = parityStripeID;
			writeQNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
			    0, 0, which_ru);
			if (lu_flag) {
				/* initialize node to unlock the disk queue */
				rf_InitNode(&unlockQNodes[i], rf_wait, RF_FALSE, rf_DiskUnlockFunc,
				    rf_DiskUnlockUndoFunc, rf_GenericWakeupFunc, 1, 1, 2, 0, dag_h,
				    "Unq", allocList);
				unlockQNodes[i].params[0].p = pda;	/* physical disk addr
									 * desc */
				unlockQNodes[i].params[1].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY,
				    0, lu_flag, which_ru);
			}
			pda = pda->next;
		}
	}
	/*
         * Step 4. connect the nodes.
         */

	/* connect header to block node */
	dag_h->succedents[0] = blockNode;

	/* connect block node to read old data nodes */
	RF_ASSERT(blockNode->numSuccedents == (numDataNodes + (numParityNodes * nfaults)));
	for (i = 0; i < numDataNodes; i++) {
		blockNode->succedents[i] = &readDataNodes[i];
		RF_ASSERT(readDataNodes[i].numAntecedents == 1);
		readDataNodes[i].antecedents[0] = blockNode;
		readDataNodes[i].antType[0] = rf_control;
	}

	/* connect block node to read old parity nodes */
	for (i = 0; i < numParityNodes; i++) {
		blockNode->succedents[numDataNodes + i] = &readParityNodes[i];
		RF_ASSERT(readParityNodes[i].numAntecedents == 1);
		readParityNodes[i].antecedents[0] = blockNode;
		readParityNodes[i].antType[0] = rf_control;
	}

	/* connect block node to read old Q nodes */
	if (nfaults == 2) {
		for (i = 0; i < numParityNodes; i++) {
			blockNode->succedents[numDataNodes + numParityNodes + i] = &readQNodes[i];
			RF_ASSERT(readQNodes[i].numAntecedents == 1);
			readQNodes[i].antecedents[0] = blockNode;
			readQNodes[i].antType[0] = rf_control;
		}
	}
	/* connect read old data nodes to xor nodes */
	for (i = 0; i < numDataNodes; i++) {
		RF_ASSERT(readDataNodes[i].numSuccedents == (nfaults * numParityNodes));
		for (j = 0; j < numParityNodes; j++) {
			RF_ASSERT(xorNodes[j].numAntecedents == numDataNodes + numParityNodes);
			readDataNodes[i].succedents[j] = &xorNodes[j];
			xorNodes[j].antecedents[i] = &readDataNodes[i];
			xorNodes[j].antType[i] = rf_trueData;
		}
	}

	/* connect read old data nodes to q nodes */
	if (nfaults == 2) {
		for (i = 0; i < numDataNodes; i++) {
			for (j = 0; j < numParityNodes; j++) {
				RF_ASSERT(qNodes[j].numAntecedents == numDataNodes + numParityNodes);
				readDataNodes[i].succedents[numParityNodes + j] = &qNodes[j];
				qNodes[j].antecedents[i] = &readDataNodes[i];
				qNodes[j].antType[i] = rf_trueData;
			}
		}
	}
	/* connect read old parity nodes to xor nodes */
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(readParityNodes[i].numSuccedents == numParityNodes);
		for (j = 0; j < numParityNodes; j++) {
			readParityNodes[i].succedents[j] = &xorNodes[j];
			xorNodes[j].antecedents[numDataNodes + i] = &readParityNodes[i];
			xorNodes[j].antType[numDataNodes + i] = rf_trueData;
		}
	}

	/* connect read old q nodes to q nodes */
	if (nfaults == 2) {
		for (i = 0; i < numParityNodes; i++) {
			RF_ASSERT(readParityNodes[i].numSuccedents == numParityNodes);
			for (j = 0; j < numParityNodes; j++) {
				readQNodes[i].succedents[j] = &qNodes[j];
				qNodes[j].antecedents[numDataNodes + i] = &readQNodes[i];
				qNodes[j].antType[numDataNodes + i] = rf_trueData;
			}
		}
	}
	/* connect xor nodes to commit node */
	RF_ASSERT(commitNode->numAntecedents == (nfaults * numParityNodes));
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(xorNodes[i].numSuccedents == 1);
		xorNodes[i].succedents[0] = commitNode;
		commitNode->antecedents[i] = &xorNodes[i];
		commitNode->antType[i] = rf_control;
	}

	/* connect q nodes to commit node */
	if (nfaults == 2) {
		for (i = 0; i < numParityNodes; i++) {
			RF_ASSERT(qNodes[i].numSuccedents == 1);
			qNodes[i].succedents[0] = commitNode;
			commitNode->antecedents[i + numParityNodes] = &qNodes[i];
			commitNode->antType[i + numParityNodes] = rf_control;
		}
	}
	/* connect commit node to write nodes */
	RF_ASSERT(commitNode->numSuccedents == (numDataNodes + (nfaults * numParityNodes)));
	for (i = 0; i < numDataNodes; i++) {
		RF_ASSERT(writeDataNodes[i].numAntecedents == 1);
		commitNode->succedents[i] = &writeDataNodes[i];
		writeDataNodes[i].antecedents[0] = commitNode;
		writeDataNodes[i].antType[0] = rf_trueData;
	}
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(writeParityNodes[i].numAntecedents == 1);
		commitNode->succedents[i + numDataNodes] = &writeParityNodes[i];
		writeParityNodes[i].antecedents[0] = commitNode;
		writeParityNodes[i].antType[0] = rf_trueData;
	}
	if (nfaults == 2) {
		for (i = 0; i < numParityNodes; i++) {
			RF_ASSERT(writeQNodes[i].numAntecedents == 1);
			commitNode->succedents[i + numDataNodes + numParityNodes] = &writeQNodes[i];
			writeQNodes[i].antecedents[0] = commitNode;
			writeQNodes[i].antType[0] = rf_trueData;
		}
	}
	RF_ASSERT(termNode->numAntecedents == (numDataNodes + (nfaults * numParityNodes)));
	RF_ASSERT(termNode->numSuccedents == 0);
	for (i = 0; i < numDataNodes; i++) {
		if (lu_flag) {
			/* connect write new data nodes to unlock nodes */
			RF_ASSERT(writeDataNodes[i].numSuccedents == 1);
			RF_ASSERT(unlockDataNodes[i].numAntecedents == 1);
			writeDataNodes[i].succedents[0] = &unlockDataNodes[i];
			unlockDataNodes[i].antecedents[0] = &writeDataNodes[i];
			unlockDataNodes[i].antType[0] = rf_control;

			/* connect unlock nodes to term node */
			RF_ASSERT(unlockDataNodes[i].numSuccedents == 1);
			unlockDataNodes[i].succedents[0] = termNode;
			termNode->antecedents[i] = &unlockDataNodes[i];
			termNode->antType[i] = rf_control;
		} else {
			/* connect write new data nodes to term node */
			RF_ASSERT(writeDataNodes[i].numSuccedents == 1);
			RF_ASSERT(termNode->numAntecedents == (numDataNodes + (nfaults * numParityNodes)));
			writeDataNodes[i].succedents[0] = termNode;
			termNode->antecedents[i] = &writeDataNodes[i];
			termNode->antType[i] = rf_control;
		}
	}

	for (i = 0; i < numParityNodes; i++) {
		if (lu_flag) {
			/* connect write new parity nodes to unlock nodes */
			RF_ASSERT(writeParityNodes[i].numSuccedents == 1);
			RF_ASSERT(unlockParityNodes[i].numAntecedents == 1);
			writeParityNodes[i].succedents[0] = &unlockParityNodes[i];
			unlockParityNodes[i].antecedents[0] = &writeParityNodes[i];
			unlockParityNodes[i].antType[0] = rf_control;

			/* connect unlock nodes to term node */
			RF_ASSERT(unlockParityNodes[i].numSuccedents == 1);
			unlockParityNodes[i].succedents[0] = termNode;
			termNode->antecedents[numDataNodes + i] = &unlockParityNodes[i];
			termNode->antType[numDataNodes + i] = rf_control;
		} else {
			RF_ASSERT(writeParityNodes[i].numSuccedents == 1);
			writeParityNodes[i].succedents[0] = termNode;
			termNode->antecedents[numDataNodes + i] = &writeParityNodes[i];
			termNode->antType[numDataNodes + i] = rf_control;
		}
	}

	if (nfaults == 2) {
		for (i = 0; i < numParityNodes; i++) {
			if (lu_flag) {
				/* connect write new Q nodes to unlock nodes */
				RF_ASSERT(writeQNodes[i].numSuccedents == 1);
				RF_ASSERT(unlockQNodes[i].numAntecedents == 1);
				writeQNodes[i].succedents[0] = &unlockQNodes[i];
				unlockQNodes[i].antecedents[0] = &writeQNodes[i];
				unlockQNodes[i].antType[0] = rf_control;

				/* connect unlock nodes to unblock node */
				RF_ASSERT(unlockQNodes[i].numSuccedents == 1);
				unlockQNodes[i].succedents[0] = termNode;
				termNode->antecedents[numDataNodes + numParityNodes + i] = &unlockQNodes[i];
				termNode->antType[numDataNodes + numParityNodes + i] = rf_control;
			} else {
				RF_ASSERT(writeQNodes[i].numSuccedents == 1);
				writeQNodes[i].succedents[0] = termNode;
				termNode->antecedents[numDataNodes + numParityNodes + i] = &writeQNodes[i];
				termNode->antType[numDataNodes + numParityNodes + i] = rf_control;
			}
		}
	}
}


/******************************************************************************
 * create a write graph (fault-free or degraded) for RAID level 1
 *
 * Hdr -> Commit -> Wpd -> Nil -> Trm
 *               -> Wsd ->
 *
 * The "Wpd" node writes data to the primary copy in the mirror pair
 * The "Wsd" node writes data to the secondary copy in the mirror pair
 *
 * Parameters:  raidPtr   - description of the physical array
 *              asmap     - logical & physical addresses for this access
 *              bp        - buffer ptr (holds write data)
 *              flags     - general flags (e.g. disk locking)
 *              allocList - list of memory allocated in DAG creation
 *****************************************************************************/

void 
rf_CreateRaidOneWriteDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList)
{
	RF_DagNode_t *unblockNode, *termNode, *commitNode;
	RF_DagNode_t *nodes, *wndNode, *wmirNode;
	int     nWndNodes, nWmirNodes, i;
	RF_ReconUnitNum_t which_ru;
	RF_PhysDiskAddr_t *pda, *pdaP;
	RF_StripeNum_t parityStripeID;

	parityStripeID = rf_RaidAddressToParityStripeID(&(raidPtr->Layout),
	    asmap->raidAddress, &which_ru);
	if (rf_dagDebug) {
		printf("[Creating RAID level 1 write DAG]\n");
	}
	dag_h->creator = "RaidOneWriteDAG";

	/* 2 implies access not SU aligned */
	nWmirNodes = (asmap->parityInfo->next) ? 2 : 1;
	nWndNodes = (asmap->physInfo->next) ? 2 : 1;

	/* alloc the Wnd nodes and the Wmir node */
	if (asmap->numDataFailed == 1)
		nWndNodes--;
	if (asmap->numParityFailed == 1)
		nWmirNodes--;

	/* total number of nodes = nWndNodes + nWmirNodes + (commit + unblock
	 * + terminator) */
	RF_CallocAndAdd(nodes, nWndNodes + nWmirNodes + 3, sizeof(RF_DagNode_t),
	    (RF_DagNode_t *), allocList);
	i = 0;
	wndNode = &nodes[i];
	i += nWndNodes;
	wmirNode = &nodes[i];
	i += nWmirNodes;
	commitNode = &nodes[i];
	i += 1;
	unblockNode = &nodes[i];
	i += 1;
	termNode = &nodes[i];
	i += 1;
	RF_ASSERT(i == (nWndNodes + nWmirNodes + 3));

	/* this dag can commit immediately */
	dag_h->numCommitNodes = 1;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	/* initialize the commit, unblock, and term nodes */
	rf_InitNode(commitNode, rf_wait, RF_TRUE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
	    NULL, (nWndNodes + nWmirNodes), 0, 0, 0, dag_h, "Cmt", allocList);
	rf_InitNode(unblockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
	    NULL, 1, (nWndNodes + nWmirNodes), 0, 0, dag_h, "Nil", allocList);
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc,
	    NULL, 0, 1, 0, 0, dag_h, "Trm", allocList);

	/* initialize the wnd nodes */
	if (nWndNodes > 0) {
		pda = asmap->physInfo;
		for (i = 0; i < nWndNodes; i++) {
			rf_InitNode(&wndNode[i], rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
			    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Wpd", allocList);
			RF_ASSERT(pda != NULL);
			wndNode[i].params[0].p = pda;
			wndNode[i].params[1].p = pda->bufPtr;
			wndNode[i].params[2].v = parityStripeID;
			wndNode[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
			pda = pda->next;
		}
		RF_ASSERT(pda == NULL);
	}
	/* initialize the mirror nodes */
	if (nWmirNodes > 0) {
		pda = asmap->physInfo;
		pdaP = asmap->parityInfo;
		for (i = 0; i < nWmirNodes; i++) {
			rf_InitNode(&wmirNode[i], rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
			    rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Wsd", allocList);
			RF_ASSERT(pda != NULL);
			wmirNode[i].params[0].p = pdaP;
			wmirNode[i].params[1].p = pda->bufPtr;
			wmirNode[i].params[2].v = parityStripeID;
			wmirNode[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
			pda = pda->next;
			pdaP = pdaP->next;
		}
		RF_ASSERT(pda == NULL);
		RF_ASSERT(pdaP == NULL);
	}
	/* link the header node to the commit node */
	RF_ASSERT(dag_h->numSuccedents == 1);
	RF_ASSERT(commitNode->numAntecedents == 0);
	dag_h->succedents[0] = commitNode;

	/* link the commit node to the write nodes */
	RF_ASSERT(commitNode->numSuccedents == (nWndNodes + nWmirNodes));
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(wndNode[i].numAntecedents == 1);
		commitNode->succedents[i] = &wndNode[i];
		wndNode[i].antecedents[0] = commitNode;
		wndNode[i].antType[0] = rf_control;
	}
	for (i = 0; i < nWmirNodes; i++) {
		RF_ASSERT(wmirNode[i].numAntecedents == 1);
		commitNode->succedents[i + nWndNodes] = &wmirNode[i];
		wmirNode[i].antecedents[0] = commitNode;
		wmirNode[i].antType[0] = rf_control;
	}

	/* link the write nodes to the unblock node */
	RF_ASSERT(unblockNode->numAntecedents == (nWndNodes + nWmirNodes));
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(wndNode[i].numSuccedents == 1);
		wndNode[i].succedents[0] = unblockNode;
		unblockNode->antecedents[i] = &wndNode[i];
		unblockNode->antType[i] = rf_control;
	}
	for (i = 0; i < nWmirNodes; i++) {
		RF_ASSERT(wmirNode[i].numSuccedents == 1);
		wmirNode[i].succedents[0] = unblockNode;
		unblockNode->antecedents[i + nWndNodes] = &wmirNode[i];
		unblockNode->antType[i + nWndNodes] = rf_control;
	}

	/* link the unblock node to the term node */
	RF_ASSERT(unblockNode->numSuccedents == 1);
	RF_ASSERT(termNode->numAntecedents == 1);
	RF_ASSERT(termNode->numSuccedents == 0);
	unblockNode->succedents[0] = termNode;
	termNode->antecedents[0] = unblockNode;
	termNode->antType[0] = rf_control;
}



/* DAGs which have no commit points.
 *
 * The following DAGs are used in forward and backward error recovery experiments.
 * They are identical to the DAGs above this comment with the exception that the
 * the commit points have been removed.
 */



void 
rf_CommonCreateLargeWriteDAGFwd(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList,
    int nfaults,
    int (*redFunc) (RF_DagNode_t *),
    int allowBufferRecycle)
{
	RF_DagNode_t *nodes, *wndNodes, *rodNodes, *xorNode, *wnpNode;
	RF_DagNode_t *wnqNode, *blockNode, *syncNode, *termNode;
	int     nWndNodes, nRodNodes, i, nodeNum, asmNum;
	RF_AccessStripeMapHeader_t *new_asm_h[2];
	RF_StripeNum_t parityStripeID;
	char   *sosBuffer, *eosBuffer;
	RF_ReconUnitNum_t which_ru;
	RF_RaidLayout_t *layoutPtr;
	RF_PhysDiskAddr_t *pda;

	layoutPtr = &(raidPtr->Layout);
	parityStripeID = rf_RaidAddressToParityStripeID(&(raidPtr->Layout), asmap->raidAddress, &which_ru);

	if (rf_dagDebug)
		printf("[Creating large-write DAG]\n");
	dag_h->creator = "LargeWriteDAGFwd";

	dag_h->numCommitNodes = 0;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	/* alloc the nodes: Wnd, xor, commit, block, term, and  Wnp */
	nWndNodes = asmap->numStripeUnitsAccessed;
	RF_CallocAndAdd(nodes, nWndNodes + 4 + nfaults, sizeof(RF_DagNode_t), (RF_DagNode_t *), allocList);
	i = 0;
	wndNodes = &nodes[i];
	i += nWndNodes;
	xorNode = &nodes[i];
	i += 1;
	wnpNode = &nodes[i];
	i += 1;
	blockNode = &nodes[i];
	i += 1;
	syncNode = &nodes[i];
	i += 1;
	termNode = &nodes[i];
	i += 1;
	if (nfaults == 2) {
		wnqNode = &nodes[i];
		i += 1;
	} else {
		wnqNode = NULL;
	}
	rf_MapUnaccessedPortionOfStripe(raidPtr, layoutPtr, asmap, dag_h, new_asm_h, &nRodNodes, &sosBuffer, &eosBuffer, allocList);
	if (nRodNodes > 0) {
		RF_CallocAndAdd(rodNodes, nRodNodes, sizeof(RF_DagNode_t), (RF_DagNode_t *), allocList);
	} else {
		rodNodes = NULL;
	}

	/* begin node initialization */
	if (nRodNodes > 0) {
		rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, nRodNodes, 0, 0, 0, dag_h, "Nil", allocList);
		rf_InitNode(syncNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, nWndNodes + 1, nRodNodes, 0, 0, dag_h, "Nil", allocList);
	} else {
		rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, 1, 0, 0, 0, dag_h, "Nil", allocList);
		rf_InitNode(syncNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, nWndNodes + 1, 1, 0, 0, dag_h, "Nil", allocList);
	}

	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc, NULL, 0, nWndNodes + nfaults, 0, 0, dag_h, "Trm", allocList);

	/* initialize the Rod nodes */
	for (nodeNum = asmNum = 0; asmNum < 2; asmNum++) {
		if (new_asm_h[asmNum]) {
			pda = new_asm_h[asmNum]->stripeMap->physInfo;
			while (pda) {
				rf_InitNode(&rodNodes[nodeNum], rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Rod", allocList);
				rodNodes[nodeNum].params[0].p = pda;
				rodNodes[nodeNum].params[1].p = pda->bufPtr;
				rodNodes[nodeNum].params[2].v = parityStripeID;
				rodNodes[nodeNum].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
				nodeNum++;
				pda = pda->next;
			}
		}
	}
	RF_ASSERT(nodeNum == nRodNodes);

	/* initialize the wnd nodes */
	pda = asmap->physInfo;
	for (i = 0; i < nWndNodes; i++) {
		rf_InitNode(&wndNodes[i], rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Wnd", allocList);
		RF_ASSERT(pda != NULL);
		wndNodes[i].params[0].p = pda;
		wndNodes[i].params[1].p = pda->bufPtr;
		wndNodes[i].params[2].v = parityStripeID;
		wndNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
		pda = pda->next;
	}

	/* initialize the redundancy node */
	rf_InitNode(xorNode, rf_wait, RF_FALSE, redFunc, rf_NullNodeUndoFunc, NULL, 1, nfaults, 2 * (nWndNodes + nRodNodes) + 1, nfaults, dag_h, "Xr ", allocList);
	xorNode->flags |= RF_DAGNODE_FLAG_YIELD;
	for (i = 0; i < nWndNodes; i++) {
		xorNode->params[2 * i + 0] = wndNodes[i].params[0];	/* pda */
		xorNode->params[2 * i + 1] = wndNodes[i].params[1];	/* buf ptr */
	}
	for (i = 0; i < nRodNodes; i++) {
		xorNode->params[2 * (nWndNodes + i) + 0] = rodNodes[i].params[0];	/* pda */
		xorNode->params[2 * (nWndNodes + i) + 1] = rodNodes[i].params[1];	/* buf ptr */
	}
	xorNode->params[2 * (nWndNodes + nRodNodes)].p = raidPtr;	/* xor node needs to get
									 * at RAID information */

	/* look for an Rod node that reads a complete SU.  If none, alloc a
	 * buffer to receive the parity info. Note that we can't use a new
	 * data buffer because it will not have gotten written when the xor
	 * occurs. */
	if (allowBufferRecycle) {
		for (i = 0; i < nRodNodes; i++)
			if (((RF_PhysDiskAddr_t *) rodNodes[i].params[0].p)->numSector == raidPtr->Layout.sectorsPerStripeUnit)
				break;
	}
	if ((!allowBufferRecycle) || (i == nRodNodes)) {
		RF_CallocAndAdd(xorNode->results[0], 1, rf_RaidAddressToByte(raidPtr, raidPtr->Layout.sectorsPerStripeUnit), (void *), allocList);
	} else
		xorNode->results[0] = rodNodes[i].params[1].p;

	/* initialize the Wnp node */
	rf_InitNode(wnpNode, rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Wnp", allocList);
	wnpNode->params[0].p = asmap->parityInfo;
	wnpNode->params[1].p = xorNode->results[0];
	wnpNode->params[2].v = parityStripeID;
	wnpNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
	RF_ASSERT(asmap->parityInfo->next == NULL);	/* parityInfo must
							 * describe entire
							 * parity unit */

	if (nfaults == 2) {
		/* we never try to recycle a buffer for the Q calcuation in
		 * addition to the parity. This would cause two buffers to get
		 * smashed during the P and Q calculation, guaranteeing one
		 * would be wrong. */
		RF_CallocAndAdd(xorNode->results[1], 1, rf_RaidAddressToByte(raidPtr, raidPtr->Layout.sectorsPerStripeUnit), (void *), allocList);
		rf_InitNode(wnqNode, rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Wnq", allocList);
		wnqNode->params[0].p = asmap->qInfo;
		wnqNode->params[1].p = xorNode->results[1];
		wnqNode->params[2].v = parityStripeID;
		wnqNode->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
		RF_ASSERT(asmap->parityInfo->next == NULL);	/* parityInfo must
								 * describe entire
								 * parity unit */
	}
	/* connect nodes to form graph */

	/* connect dag header to block node */
	RF_ASSERT(blockNode->numAntecedents == 0);
	dag_h->succedents[0] = blockNode;

	if (nRodNodes > 0) {
		/* connect the block node to the Rod nodes */
		RF_ASSERT(blockNode->numSuccedents == nRodNodes);
		RF_ASSERT(syncNode->numAntecedents == nRodNodes);
		for (i = 0; i < nRodNodes; i++) {
			RF_ASSERT(rodNodes[i].numAntecedents == 1);
			blockNode->succedents[i] = &rodNodes[i];
			rodNodes[i].antecedents[0] = blockNode;
			rodNodes[i].antType[0] = rf_control;

			/* connect the Rod nodes to the Nil node */
			RF_ASSERT(rodNodes[i].numSuccedents == 1);
			rodNodes[i].succedents[0] = syncNode;
			syncNode->antecedents[i] = &rodNodes[i];
			syncNode->antType[i] = rf_trueData;
		}
	} else {
		/* connect the block node to the Nil node */
		RF_ASSERT(blockNode->numSuccedents == 1);
		RF_ASSERT(syncNode->numAntecedents == 1);
		blockNode->succedents[0] = syncNode;
		syncNode->antecedents[0] = blockNode;
		syncNode->antType[0] = rf_control;
	}

	/* connect the sync node to the Wnd nodes */
	RF_ASSERT(syncNode->numSuccedents == (1 + nWndNodes));
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(wndNodes->numAntecedents == 1);
		syncNode->succedents[i] = &wndNodes[i];
		wndNodes[i].antecedents[0] = syncNode;
		wndNodes[i].antType[0] = rf_control;
	}

	/* connect the sync node to the Xor node */
	RF_ASSERT(xorNode->numAntecedents == 1);
	syncNode->succedents[nWndNodes] = xorNode;
	xorNode->antecedents[0] = syncNode;
	xorNode->antType[0] = rf_control;

	/* connect the xor node to the write parity node */
	RF_ASSERT(xorNode->numSuccedents == nfaults);
	RF_ASSERT(wnpNode->numAntecedents == 1);
	xorNode->succedents[0] = wnpNode;
	wnpNode->antecedents[0] = xorNode;
	wnpNode->antType[0] = rf_trueData;
	if (nfaults == 2) {
		RF_ASSERT(wnqNode->numAntecedents == 1);
		xorNode->succedents[1] = wnqNode;
		wnqNode->antecedents[0] = xorNode;
		wnqNode->antType[0] = rf_trueData;
	}
	/* connect the write nodes to the term node */
	RF_ASSERT(termNode->numAntecedents == nWndNodes + nfaults);
	RF_ASSERT(termNode->numSuccedents == 0);
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(wndNodes->numSuccedents == 1);
		wndNodes[i].succedents[0] = termNode;
		termNode->antecedents[i] = &wndNodes[i];
		termNode->antType[i] = rf_control;
	}
	RF_ASSERT(wnpNode->numSuccedents == 1);
	wnpNode->succedents[0] = termNode;
	termNode->antecedents[nWndNodes] = wnpNode;
	termNode->antType[nWndNodes] = rf_control;
	if (nfaults == 2) {
		RF_ASSERT(wnqNode->numSuccedents == 1);
		wnqNode->succedents[0] = termNode;
		termNode->antecedents[nWndNodes + 1] = wnqNode;
		termNode->antType[nWndNodes + 1] = rf_control;
	}
}


/******************************************************************************
 *
 * creates a DAG to perform a small-write operation (either raid 5 or pq),
 * which is as follows:
 *
 * Hdr -> Nil -> Rop - Xor - Wnp [Unp] -- Trm
 *            \- Rod X- Wnd [Und] -------/
 *           [\- Rod X- Wnd [Und] ------/]
 *           [\- Roq - Q --> Wnq [Unq]-/]
 *
 * Rop = read old parity
 * Rod = read old data
 * Roq = read old "q"
 * Cmt = commit node
 * Und = unlock data disk
 * Unp = unlock parity disk
 * Unq = unlock q disk
 * Wnp = write new parity
 * Wnd = write new data
 * Wnq = write new "q"
 * [ ] denotes optional segments in the graph
 *
 * Parameters:  raidPtr   - description of the physical array
 *              asmap     - logical & physical addresses for this access
 *              bp        - buffer ptr (holds write data)
 *              flags     - general flags (e.g. disk locking)
 *              allocList - list of memory allocated in DAG creation
 *              pfuncs    - list of parity generating functions
 *              qfuncs    - list of q generating functions
 *
 * A null qfuncs indicates single fault tolerant
 *****************************************************************************/

void 
rf_CommonCreateSmallWriteDAGFwd(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList,
    RF_RedFuncs_t * pfuncs,
    RF_RedFuncs_t * qfuncs)
{
	RF_DagNode_t *readDataNodes, *readParityNodes, *readQNodes, *termNode;
	RF_DagNode_t *unlockDataNodes, *unlockParityNodes, *unlockQNodes;
	RF_DagNode_t *xorNodes, *qNodes, *blockNode, *nodes;
	RF_DagNode_t *writeDataNodes, *writeParityNodes, *writeQNodes;
	int     i, j, nNodes, totalNumNodes, lu_flag;
	RF_ReconUnitNum_t which_ru;
	int     (*func) (RF_DagNode_t *), (*undoFunc) (RF_DagNode_t *);
	int     (*qfunc) (RF_DagNode_t *);
	int     numDataNodes, numParityNodes;
	RF_StripeNum_t parityStripeID;
	RF_PhysDiskAddr_t *pda;
	char   *name, *qname;
	long    nfaults;

	nfaults = qfuncs ? 2 : 1;
	lu_flag = (rf_enableAtomicRMW) ? 1 : 0;	/* lock/unlock flag */

	parityStripeID = rf_RaidAddressToParityStripeID(&(raidPtr->Layout), asmap->raidAddress, &which_ru);
	pda = asmap->physInfo;
	numDataNodes = asmap->numStripeUnitsAccessed;
	numParityNodes = (asmap->parityInfo->next) ? 2 : 1;

	if (rf_dagDebug)
		printf("[Creating small-write DAG]\n");
	RF_ASSERT(numDataNodes > 0);
	dag_h->creator = "SmallWriteDAGFwd";

	dag_h->numCommitNodes = 0;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	qfunc = NULL;
	qname = NULL;

	/* DAG creation occurs in four steps: 1. count the number of nodes in
	 * the DAG 2. create the nodes 3. initialize the nodes 4. connect the
	 * nodes */

	/* Step 1. compute number of nodes in the graph */

	/* number of nodes: a read and write for each data unit a redundancy
	 * computation node for each parity node (nfaults * nparity) a read
	 * and write for each parity unit a block node a terminate node if
	 * atomic RMW an unlock node for each data unit, redundancy unit */
	totalNumNodes = (2 * numDataNodes) + (nfaults * numParityNodes) + (nfaults * 2 * numParityNodes) + 2;
	if (lu_flag)
		totalNumNodes += (numDataNodes + (nfaults * numParityNodes));


	/* Step 2. create the nodes */
	RF_CallocAndAdd(nodes, totalNumNodes, sizeof(RF_DagNode_t), (RF_DagNode_t *), allocList);
	i = 0;
	blockNode = &nodes[i];
	i += 1;
	readDataNodes = &nodes[i];
	i += numDataNodes;
	readParityNodes = &nodes[i];
	i += numParityNodes;
	writeDataNodes = &nodes[i];
	i += numDataNodes;
	writeParityNodes = &nodes[i];
	i += numParityNodes;
	xorNodes = &nodes[i];
	i += numParityNodes;
	termNode = &nodes[i];
	i += 1;
	if (lu_flag) {
		unlockDataNodes = &nodes[i];
		i += numDataNodes;
		unlockParityNodes = &nodes[i];
		i += numParityNodes;
	} else {
		unlockDataNodes = unlockParityNodes = NULL;
	}
	if (nfaults == 2) {
		readQNodes = &nodes[i];
		i += numParityNodes;
		writeQNodes = &nodes[i];
		i += numParityNodes;
		qNodes = &nodes[i];
		i += numParityNodes;
		if (lu_flag) {
			unlockQNodes = &nodes[i];
			i += numParityNodes;
		} else {
			unlockQNodes = NULL;
		}
	} else {
		readQNodes = writeQNodes = qNodes = unlockQNodes = NULL;
	}
	RF_ASSERT(i == totalNumNodes);

	/* Step 3. initialize the nodes */
	/* initialize block node (Nil) */
	nNodes = numDataNodes + (nfaults * numParityNodes);
	rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, nNodes, 0, 0, 0, dag_h, "Nil", allocList);

	/* initialize terminate node (Trm) */
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc, NULL, 0, nNodes, 0, 0, dag_h, "Trm", allocList);

	/* initialize nodes which read old data (Rod) */
	for (i = 0; i < numDataNodes; i++) {
		rf_InitNode(&readDataNodes[i], rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc, rf_GenericWakeupFunc, (numParityNodes * nfaults) + 1, 1, 4, 0, dag_h, "Rod", allocList);
		RF_ASSERT(pda != NULL);
		readDataNodes[i].params[0].p = pda;	/* physical disk addr
							 * desc */
		readDataNodes[i].params[1].p = rf_AllocBuffer(raidPtr, dag_h, pda, allocList);	/* buffer to hold old
												 * data */
		readDataNodes[i].params[2].v = parityStripeID;
		readDataNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, lu_flag, 0, which_ru);
		pda = pda->next;
		for (j = 0; j < readDataNodes[i].numSuccedents; j++)
			readDataNodes[i].propList[j] = NULL;
	}

	/* initialize nodes which read old parity (Rop) */
	pda = asmap->parityInfo;
	i = 0;
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(pda != NULL);
		rf_InitNode(&readParityNodes[i], rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc, rf_GenericWakeupFunc, numParityNodes, 1, 4, 0, dag_h, "Rop", allocList);
		readParityNodes[i].params[0].p = pda;
		readParityNodes[i].params[1].p = rf_AllocBuffer(raidPtr, dag_h, pda, allocList);	/* buffer to hold old
													 * parity */
		readParityNodes[i].params[2].v = parityStripeID;
		readParityNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, lu_flag, 0, which_ru);
		for (j = 0; j < readParityNodes[i].numSuccedents; j++)
			readParityNodes[i].propList[0] = NULL;
		pda = pda->next;
	}

	/* initialize nodes which read old Q (Roq) */
	if (nfaults == 2) {
		pda = asmap->qInfo;
		for (i = 0; i < numParityNodes; i++) {
			RF_ASSERT(pda != NULL);
			rf_InitNode(&readQNodes[i], rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc, rf_GenericWakeupFunc, numParityNodes, 1, 4, 0, dag_h, "Roq", allocList);
			readQNodes[i].params[0].p = pda;
			readQNodes[i].params[1].p = rf_AllocBuffer(raidPtr, dag_h, pda, allocList);	/* buffer to hold old Q */
			readQNodes[i].params[2].v = parityStripeID;
			readQNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, lu_flag, 0, which_ru);
			for (j = 0; j < readQNodes[i].numSuccedents; j++)
				readQNodes[i].propList[0] = NULL;
			pda = pda->next;
		}
	}
	/* initialize nodes which write new data (Wnd) */
	pda = asmap->physInfo;
	for (i = 0; i < numDataNodes; i++) {
		RF_ASSERT(pda != NULL);
		rf_InitNode(&writeDataNodes[i], rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Wnd", allocList);
		writeDataNodes[i].params[0].p = pda;	/* physical disk addr
							 * desc */
		writeDataNodes[i].params[1].p = pda->bufPtr;	/* buffer holding new
								 * data to be written */
		writeDataNodes[i].params[2].v = parityStripeID;
		writeDataNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);

		if (lu_flag) {
			/* initialize node to unlock the disk queue */
			rf_InitNode(&unlockDataNodes[i], rf_wait, RF_FALSE, rf_DiskUnlockFunc, rf_DiskUnlockUndoFunc, rf_GenericWakeupFunc, 1, 1, 2, 0, dag_h, "Und", allocList);
			unlockDataNodes[i].params[0].p = pda;	/* physical disk addr
								 * desc */
			unlockDataNodes[i].params[1].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, lu_flag, which_ru);
		}
		pda = pda->next;
	}


	/* initialize nodes which compute new parity and Q */
	/* we use the simple XOR func in the double-XOR case, and when we're
	 * accessing only a portion of one stripe unit. the distinction
	 * between the two is that the regular XOR func assumes that the
	 * targbuf is a full SU in size, and examines the pda associated with
	 * the buffer to decide where within the buffer to XOR the data,
	 * whereas the simple XOR func just XORs the data into the start of
	 * the buffer. */
	if ((numParityNodes == 2) || ((numDataNodes == 1) && (asmap->totalSectorsAccessed < raidPtr->Layout.sectorsPerStripeUnit))) {
		func = pfuncs->simple;
		undoFunc = rf_NullNodeUndoFunc;
		name = pfuncs->SimpleName;
		if (qfuncs) {
			qfunc = qfuncs->simple;
			qname = qfuncs->SimpleName;
		}
	} else {
		func = pfuncs->regular;
		undoFunc = rf_NullNodeUndoFunc;
		name = pfuncs->RegularName;
		if (qfuncs) {
			qfunc = qfuncs->regular;
			qname = qfuncs->RegularName;
		}
	}
	/* initialize the xor nodes: params are {pda,buf} from {Rod,Wnd,Rop}
	 * nodes, and raidPtr  */
	if (numParityNodes == 2) {	/* double-xor case */
		for (i = 0; i < numParityNodes; i++) {
			rf_InitNode(&xorNodes[i], rf_wait, RF_FALSE, func, undoFunc, NULL, numParityNodes, numParityNodes + numDataNodes, 7, 1, dag_h, name, allocList);	/* no wakeup func for
																						 * xor */
			xorNodes[i].flags |= RF_DAGNODE_FLAG_YIELD;
			xorNodes[i].params[0] = readDataNodes[i].params[0];
			xorNodes[i].params[1] = readDataNodes[i].params[1];
			xorNodes[i].params[2] = readParityNodes[i].params[0];
			xorNodes[i].params[3] = readParityNodes[i].params[1];
			xorNodes[i].params[4] = writeDataNodes[i].params[0];
			xorNodes[i].params[5] = writeDataNodes[i].params[1];
			xorNodes[i].params[6].p = raidPtr;
			xorNodes[i].results[0] = readParityNodes[i].params[1].p;	/* use old parity buf as
											 * target buf */
			if (nfaults == 2) {
				rf_InitNode(&qNodes[i], rf_wait, RF_FALSE, qfunc, undoFunc, NULL, numParityNodes, numParityNodes + numDataNodes, 7, 1, dag_h, qname, allocList);	/* no wakeup func for
																							 * xor */
				qNodes[i].params[0] = readDataNodes[i].params[0];
				qNodes[i].params[1] = readDataNodes[i].params[1];
				qNodes[i].params[2] = readQNodes[i].params[0];
				qNodes[i].params[3] = readQNodes[i].params[1];
				qNodes[i].params[4] = writeDataNodes[i].params[0];
				qNodes[i].params[5] = writeDataNodes[i].params[1];
				qNodes[i].params[6].p = raidPtr;
				qNodes[i].results[0] = readQNodes[i].params[1].p;	/* use old Q buf as
											 * target buf */
			}
		}
	} else {
		/* there is only one xor node in this case */
		rf_InitNode(&xorNodes[0], rf_wait, RF_FALSE, func, undoFunc, NULL, numParityNodes, numParityNodes + numDataNodes, (2 * (numDataNodes + numDataNodes + 1) + 1), 1, dag_h, name, allocList);
		xorNodes[0].flags |= RF_DAGNODE_FLAG_YIELD;
		for (i = 0; i < numDataNodes + 1; i++) {
			/* set up params related to Rod and Rop nodes */
			xorNodes[0].params[2 * i + 0] = readDataNodes[i].params[0];	/* pda */
			xorNodes[0].params[2 * i + 1] = readDataNodes[i].params[1];	/* buffer pointer */
		}
		for (i = 0; i < numDataNodes; i++) {
			/* set up params related to Wnd and Wnp nodes */
			xorNodes[0].params[2 * (numDataNodes + 1 + i) + 0] = writeDataNodes[i].params[0];	/* pda */
			xorNodes[0].params[2 * (numDataNodes + 1 + i) + 1] = writeDataNodes[i].params[1];	/* buffer pointer */
		}
		xorNodes[0].params[2 * (numDataNodes + numDataNodes + 1)].p = raidPtr;	/* xor node needs to get
											 * at RAID information */
		xorNodes[0].results[0] = readParityNodes[0].params[1].p;
		if (nfaults == 2) {
			rf_InitNode(&qNodes[0], rf_wait, RF_FALSE, qfunc, undoFunc, NULL, numParityNodes, numParityNodes + numDataNodes, (2 * (numDataNodes + numDataNodes + 1) + 1), 1, dag_h, qname, allocList);
			for (i = 0; i < numDataNodes; i++) {
				/* set up params related to Rod */
				qNodes[0].params[2 * i + 0] = readDataNodes[i].params[0];	/* pda */
				qNodes[0].params[2 * i + 1] = readDataNodes[i].params[1];	/* buffer pointer */
			}
			/* and read old q */
			qNodes[0].params[2 * numDataNodes + 0] = readQNodes[0].params[0];	/* pda */
			qNodes[0].params[2 * numDataNodes + 1] = readQNodes[0].params[1];	/* buffer pointer */
			for (i = 0; i < numDataNodes; i++) {
				/* set up params related to Wnd nodes */
				qNodes[0].params[2 * (numDataNodes + 1 + i) + 0] = writeDataNodes[i].params[0];	/* pda */
				qNodes[0].params[2 * (numDataNodes + 1 + i) + 1] = writeDataNodes[i].params[1];	/* buffer pointer */
			}
			qNodes[0].params[2 * (numDataNodes + numDataNodes + 1)].p = raidPtr;	/* xor node needs to get
												 * at RAID information */
			qNodes[0].results[0] = readQNodes[0].params[1].p;
		}
	}

	/* initialize nodes which write new parity (Wnp) */
	pda = asmap->parityInfo;
	for (i = 0; i < numParityNodes; i++) {
		rf_InitNode(&writeParityNodes[i], rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc, rf_GenericWakeupFunc, 1, numParityNodes, 4, 0, dag_h, "Wnp", allocList);
		RF_ASSERT(pda != NULL);
		writeParityNodes[i].params[0].p = pda;	/* param 1 (bufPtr)
							 * filled in by xor node */
		writeParityNodes[i].params[1].p = xorNodes[i].results[0];	/* buffer pointer for
										 * parity write
										 * operation */
		writeParityNodes[i].params[2].v = parityStripeID;
		writeParityNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);

		if (lu_flag) {
			/* initialize node to unlock the disk queue */
			rf_InitNode(&unlockParityNodes[i], rf_wait, RF_FALSE, rf_DiskUnlockFunc, rf_DiskUnlockUndoFunc, rf_GenericWakeupFunc, 1, 1, 2, 0, dag_h, "Unp", allocList);
			unlockParityNodes[i].params[0].p = pda;	/* physical disk addr
								 * desc */
			unlockParityNodes[i].params[1].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, lu_flag, which_ru);
		}
		pda = pda->next;
	}

	/* initialize nodes which write new Q (Wnq) */
	if (nfaults == 2) {
		pda = asmap->qInfo;
		for (i = 0; i < numParityNodes; i++) {
			rf_InitNode(&writeQNodes[i], rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc, rf_GenericWakeupFunc, 1, numParityNodes, 4, 0, dag_h, "Wnq", allocList);
			RF_ASSERT(pda != NULL);
			writeQNodes[i].params[0].p = pda;	/* param 1 (bufPtr)
								 * filled in by xor node */
			writeQNodes[i].params[1].p = qNodes[i].results[0];	/* buffer pointer for
										 * parity write
										 * operation */
			writeQNodes[i].params[2].v = parityStripeID;
			writeQNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);

			if (lu_flag) {
				/* initialize node to unlock the disk queue */
				rf_InitNode(&unlockQNodes[i], rf_wait, RF_FALSE, rf_DiskUnlockFunc, rf_DiskUnlockUndoFunc, rf_GenericWakeupFunc, 1, 1, 2, 0, dag_h, "Unq", allocList);
				unlockQNodes[i].params[0].p = pda;	/* physical disk addr
									 * desc */
				unlockQNodes[i].params[1].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, lu_flag, which_ru);
			}
			pda = pda->next;
		}
	}
	/* Step 4. connect the nodes */

	/* connect header to block node */
	dag_h->succedents[0] = blockNode;

	/* connect block node to read old data nodes */
	RF_ASSERT(blockNode->numSuccedents == (numDataNodes + (numParityNodes * nfaults)));
	for (i = 0; i < numDataNodes; i++) {
		blockNode->succedents[i] = &readDataNodes[i];
		RF_ASSERT(readDataNodes[i].numAntecedents == 1);
		readDataNodes[i].antecedents[0] = blockNode;
		readDataNodes[i].antType[0] = rf_control;
	}

	/* connect block node to read old parity nodes */
	for (i = 0; i < numParityNodes; i++) {
		blockNode->succedents[numDataNodes + i] = &readParityNodes[i];
		RF_ASSERT(readParityNodes[i].numAntecedents == 1);
		readParityNodes[i].antecedents[0] = blockNode;
		readParityNodes[i].antType[0] = rf_control;
	}

	/* connect block node to read old Q nodes */
	if (nfaults == 2)
		for (i = 0; i < numParityNodes; i++) {
			blockNode->succedents[numDataNodes + numParityNodes + i] = &readQNodes[i];
			RF_ASSERT(readQNodes[i].numAntecedents == 1);
			readQNodes[i].antecedents[0] = blockNode;
			readQNodes[i].antType[0] = rf_control;
		}

	/* connect read old data nodes to write new data nodes */
	for (i = 0; i < numDataNodes; i++) {
		RF_ASSERT(readDataNodes[i].numSuccedents == ((nfaults * numParityNodes) + 1));
		RF_ASSERT(writeDataNodes[i].numAntecedents == 1);
		readDataNodes[i].succedents[0] = &writeDataNodes[i];
		writeDataNodes[i].antecedents[0] = &readDataNodes[i];
		writeDataNodes[i].antType[0] = rf_antiData;
	}

	/* connect read old data nodes to xor nodes */
	for (i = 0; i < numDataNodes; i++) {
		for (j = 0; j < numParityNodes; j++) {
			RF_ASSERT(xorNodes[j].numAntecedents == numDataNodes + numParityNodes);
			readDataNodes[i].succedents[1 + j] = &xorNodes[j];
			xorNodes[j].antecedents[i] = &readDataNodes[i];
			xorNodes[j].antType[i] = rf_trueData;
		}
	}

	/* connect read old data nodes to q nodes */
	if (nfaults == 2)
		for (i = 0; i < numDataNodes; i++)
			for (j = 0; j < numParityNodes; j++) {
				RF_ASSERT(qNodes[j].numAntecedents == numDataNodes + numParityNodes);
				readDataNodes[i].succedents[1 + numParityNodes + j] = &qNodes[j];
				qNodes[j].antecedents[i] = &readDataNodes[i];
				qNodes[j].antType[i] = rf_trueData;
			}

	/* connect read old parity nodes to xor nodes */
	for (i = 0; i < numParityNodes; i++) {
		for (j = 0; j < numParityNodes; j++) {
			RF_ASSERT(readParityNodes[i].numSuccedents == numParityNodes);
			readParityNodes[i].succedents[j] = &xorNodes[j];
			xorNodes[j].antecedents[numDataNodes + i] = &readParityNodes[i];
			xorNodes[j].antType[numDataNodes + i] = rf_trueData;
		}
	}

	/* connect read old q nodes to q nodes */
	if (nfaults == 2)
		for (i = 0; i < numParityNodes; i++) {
			for (j = 0; j < numParityNodes; j++) {
				RF_ASSERT(readQNodes[i].numSuccedents == numParityNodes);
				readQNodes[i].succedents[j] = &qNodes[j];
				qNodes[j].antecedents[numDataNodes + i] = &readQNodes[i];
				qNodes[j].antType[numDataNodes + i] = rf_trueData;
			}
		}

	/* connect xor nodes to the write new parity nodes */
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(writeParityNodes[i].numAntecedents == numParityNodes);
		for (j = 0; j < numParityNodes; j++) {
			RF_ASSERT(xorNodes[j].numSuccedents == numParityNodes);
			xorNodes[i].succedents[j] = &writeParityNodes[j];
			writeParityNodes[j].antecedents[i] = &xorNodes[i];
			writeParityNodes[j].antType[i] = rf_trueData;
		}
	}

	/* connect q nodes to the write new q nodes */
	if (nfaults == 2)
		for (i = 0; i < numParityNodes; i++) {
			RF_ASSERT(writeQNodes[i].numAntecedents == numParityNodes);
			for (j = 0; j < numParityNodes; j++) {
				RF_ASSERT(qNodes[j].numSuccedents == 1);
				qNodes[i].succedents[j] = &writeQNodes[j];
				writeQNodes[j].antecedents[i] = &qNodes[i];
				writeQNodes[j].antType[i] = rf_trueData;
			}
		}

	RF_ASSERT(termNode->numAntecedents == (numDataNodes + (nfaults * numParityNodes)));
	RF_ASSERT(termNode->numSuccedents == 0);
	for (i = 0; i < numDataNodes; i++) {
		if (lu_flag) {
			/* connect write new data nodes to unlock nodes */
			RF_ASSERT(writeDataNodes[i].numSuccedents == 1);
			RF_ASSERT(unlockDataNodes[i].numAntecedents == 1);
			writeDataNodes[i].succedents[0] = &unlockDataNodes[i];
			unlockDataNodes[i].antecedents[0] = &writeDataNodes[i];
			unlockDataNodes[i].antType[0] = rf_control;

			/* connect unlock nodes to term node */
			RF_ASSERT(unlockDataNodes[i].numSuccedents == 1);
			unlockDataNodes[i].succedents[0] = termNode;
			termNode->antecedents[i] = &unlockDataNodes[i];
			termNode->antType[i] = rf_control;
		} else {
			/* connect write new data nodes to term node */
			RF_ASSERT(writeDataNodes[i].numSuccedents == 1);
			RF_ASSERT(termNode->numAntecedents == (numDataNodes + (nfaults * numParityNodes)));
			writeDataNodes[i].succedents[0] = termNode;
			termNode->antecedents[i] = &writeDataNodes[i];
			termNode->antType[i] = rf_control;
		}
	}

	for (i = 0; i < numParityNodes; i++) {
		if (lu_flag) {
			/* connect write new parity nodes to unlock nodes */
			RF_ASSERT(writeParityNodes[i].numSuccedents == 1);
			RF_ASSERT(unlockParityNodes[i].numAntecedents == 1);
			writeParityNodes[i].succedents[0] = &unlockParityNodes[i];
			unlockParityNodes[i].antecedents[0] = &writeParityNodes[i];
			unlockParityNodes[i].antType[0] = rf_control;

			/* connect unlock nodes to term node */
			RF_ASSERT(unlockParityNodes[i].numSuccedents == 1);
			unlockParityNodes[i].succedents[0] = termNode;
			termNode->antecedents[numDataNodes + i] = &unlockParityNodes[i];
			termNode->antType[numDataNodes + i] = rf_control;
		} else {
			RF_ASSERT(writeParityNodes[i].numSuccedents == 1);
			writeParityNodes[i].succedents[0] = termNode;
			termNode->antecedents[numDataNodes + i] = &writeParityNodes[i];
			termNode->antType[numDataNodes + i] = rf_control;
		}
	}

	if (nfaults == 2)
		for (i = 0; i < numParityNodes; i++) {
			if (lu_flag) {
				/* connect write new Q nodes to unlock nodes */
				RF_ASSERT(writeQNodes[i].numSuccedents == 1);
				RF_ASSERT(unlockQNodes[i].numAntecedents == 1);
				writeQNodes[i].succedents[0] = &unlockQNodes[i];
				unlockQNodes[i].antecedents[0] = &writeQNodes[i];
				unlockQNodes[i].antType[0] = rf_control;

				/* connect unlock nodes to unblock node */
				RF_ASSERT(unlockQNodes[i].numSuccedents == 1);
				unlockQNodes[i].succedents[0] = termNode;
				termNode->antecedents[numDataNodes + numParityNodes + i] = &unlockQNodes[i];
				termNode->antType[numDataNodes + numParityNodes + i] = rf_control;
			} else {
				RF_ASSERT(writeQNodes[i].numSuccedents == 1);
				writeQNodes[i].succedents[0] = termNode;
				termNode->antecedents[numDataNodes + numParityNodes + i] = &writeQNodes[i];
				termNode->antType[numDataNodes + numParityNodes + i] = rf_control;
			}
		}
}



/******************************************************************************
 * create a write graph (fault-free or degraded) for RAID level 1
 *
 * Hdr  Nil -> Wpd -> Nil -> Trm
 *      Nil -> Wsd ->
 *
 * The "Wpd" node writes data to the primary copy in the mirror pair
 * The "Wsd" node writes data to the secondary copy in the mirror pair
 *
 * Parameters:  raidPtr   - description of the physical array
 *              asmap     - logical & physical addresses for this access
 *              bp        - buffer ptr (holds write data)
 *              flags     - general flags (e.g. disk locking)
 *              allocList - list of memory allocated in DAG creation
 *****************************************************************************/

void 
rf_CreateRaidOneWriteDAGFwd(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList)
{
	RF_DagNode_t *blockNode, *unblockNode, *termNode;
	RF_DagNode_t *nodes, *wndNode, *wmirNode;
	int     nWndNodes, nWmirNodes, i;
	RF_ReconUnitNum_t which_ru;
	RF_PhysDiskAddr_t *pda, *pdaP;
	RF_StripeNum_t parityStripeID;

	parityStripeID = rf_RaidAddressToParityStripeID(&(raidPtr->Layout),
	    asmap->raidAddress, &which_ru);
	if (rf_dagDebug) {
		printf("[Creating RAID level 1 write DAG]\n");
	}
	nWmirNodes = (asmap->parityInfo->next) ? 2 : 1;	/* 2 implies access not
							 * SU aligned */
	nWndNodes = (asmap->physInfo->next) ? 2 : 1;

	/* alloc the Wnd nodes and the Wmir node */
	if (asmap->numDataFailed == 1)
		nWndNodes--;
	if (asmap->numParityFailed == 1)
		nWmirNodes--;

	/* total number of nodes = nWndNodes + nWmirNodes + (block + unblock +
	 * terminator) */
	RF_CallocAndAdd(nodes, nWndNodes + nWmirNodes + 3, sizeof(RF_DagNode_t), (RF_DagNode_t *), allocList);
	i = 0;
	wndNode = &nodes[i];
	i += nWndNodes;
	wmirNode = &nodes[i];
	i += nWmirNodes;
	blockNode = &nodes[i];
	i += 1;
	unblockNode = &nodes[i];
	i += 1;
	termNode = &nodes[i];
	i += 1;
	RF_ASSERT(i == (nWndNodes + nWmirNodes + 3));

	/* this dag can commit immediately */
	dag_h->numCommitNodes = 0;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	/* initialize the unblock and term nodes */
	rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, (nWndNodes + nWmirNodes), 0, 0, 0, dag_h, "Nil", allocList);
	rf_InitNode(unblockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, 1, (nWndNodes + nWmirNodes), 0, 0, dag_h, "Nil", allocList);
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc, NULL, 0, 1, 0, 0, dag_h, "Trm", allocList);

	/* initialize the wnd nodes */
	if (nWndNodes > 0) {
		pda = asmap->physInfo;
		for (i = 0; i < nWndNodes; i++) {
			rf_InitNode(&wndNode[i], rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Wpd", allocList);
			RF_ASSERT(pda != NULL);
			wndNode[i].params[0].p = pda;
			wndNode[i].params[1].p = pda->bufPtr;
			wndNode[i].params[2].v = parityStripeID;
			wndNode[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
			pda = pda->next;
		}
		RF_ASSERT(pda == NULL);
	}
	/* initialize the mirror nodes */
	if (nWmirNodes > 0) {
		pda = asmap->physInfo;
		pdaP = asmap->parityInfo;
		for (i = 0; i < nWmirNodes; i++) {
			rf_InitNode(&wmirNode[i], rf_wait, RF_FALSE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Wsd", allocList);
			RF_ASSERT(pda != NULL);
			wmirNode[i].params[0].p = pdaP;
			wmirNode[i].params[1].p = pda->bufPtr;
			wmirNode[i].params[2].v = parityStripeID;
			wmirNode[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
			pda = pda->next;
			pdaP = pdaP->next;
		}
		RF_ASSERT(pda == NULL);
		RF_ASSERT(pdaP == NULL);
	}
	/* link the header node to the block node */
	RF_ASSERT(dag_h->numSuccedents == 1);
	RF_ASSERT(blockNode->numAntecedents == 0);
	dag_h->succedents[0] = blockNode;

	/* link the block node to the write nodes */
	RF_ASSERT(blockNode->numSuccedents == (nWndNodes + nWmirNodes));
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(wndNode[i].numAntecedents == 1);
		blockNode->succedents[i] = &wndNode[i];
		wndNode[i].antecedents[0] = blockNode;
		wndNode[i].antType[0] = rf_control;
	}
	for (i = 0; i < nWmirNodes; i++) {
		RF_ASSERT(wmirNode[i].numAntecedents == 1);
		blockNode->succedents[i + nWndNodes] = &wmirNode[i];
		wmirNode[i].antecedents[0] = blockNode;
		wmirNode[i].antType[0] = rf_control;
	}

	/* link the write nodes to the unblock node */
	RF_ASSERT(unblockNode->numAntecedents == (nWndNodes + nWmirNodes));
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(wndNode[i].numSuccedents == 1);
		wndNode[i].succedents[0] = unblockNode;
		unblockNode->antecedents[i] = &wndNode[i];
		unblockNode->antType[i] = rf_control;
	}
	for (i = 0; i < nWmirNodes; i++) {
		RF_ASSERT(wmirNode[i].numSuccedents == 1);
		wmirNode[i].succedents[0] = unblockNode;
		unblockNode->antecedents[i + nWndNodes] = &wmirNode[i];
		unblockNode->antType[i + nWndNodes] = rf_control;
	}

	/* link the unblock node to the term node */
	RF_ASSERT(unblockNode->numSuccedents == 1);
	RF_ASSERT(termNode->numAntecedents == 1);
	RF_ASSERT(termNode->numSuccedents == 0);
	unblockNode->succedents[0] = termNode;
	termNode->antecedents[0] = unblockNode;
	termNode->antType[0] = rf_control;

	return;
}
