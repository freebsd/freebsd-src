/*	$NetBSD: rf_parityloggingdags.c,v 1.4 2000/01/07 03:41:04 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
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

#include <dev/raidframe/rf_archs.h>

#if RF_INCLUDE_PARITYLOGGING > 0

/*
  DAGs specific to parity logging are created here
 */

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_dagfuncs.h>
#include <dev/raidframe/rf_debugMem.h>
#include <dev/raidframe/rf_paritylog.h>
#include <dev/raidframe/rf_memchunk.h>
#include <dev/raidframe/rf_general.h>

#include <dev/raidframe/rf_parityloggingdags.h>

/******************************************************************************
 *
 * creates a DAG to perform a large-write operation:
 *
 *         / Rod \     / Wnd \
 * H -- NIL- Rod - NIL - Wnd ------ NIL - T
 *         \ Rod /     \ Xor - Lpo /
 *
 * The writes are not done until the reads complete because if they were done in
 * parallel, a failure on one of the reads could leave the parity in an inconsistent
 * state, so that the retry with a new DAG would produce erroneous parity.
 *
 * Note:  this DAG has the nasty property that none of the buffers allocated for reading
 *        old data can be freed until the XOR node fires.  Need to fix this.
 *
 * The last two arguments are the number of faults tolerated, and function for the
 * redundancy calculation. The undo for the redundancy calc is assumed to be null
 *
 *****************************************************************************/

void 
rf_CommonCreateParityLoggingLargeWriteDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList,
    int nfaults,
    int (*redFunc) (RF_DagNode_t *))
{
	RF_DagNode_t *nodes, *wndNodes, *rodNodes = NULL, *syncNode, *xorNode,
	       *lpoNode, *blockNode, *unblockNode, *termNode;
	int     nWndNodes, nRodNodes, i;
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_AccessStripeMapHeader_t *new_asm_h[2];
	int     nodeNum, asmNum;
	RF_ReconUnitNum_t which_ru;
	char   *sosBuffer, *eosBuffer;
	RF_PhysDiskAddr_t *pda;
	RF_StripeNum_t parityStripeID = rf_RaidAddressToParityStripeID(&(raidPtr->Layout), asmap->raidAddress, &which_ru);

	if (rf_dagDebug)
		printf("[Creating parity-logging large-write DAG]\n");
	RF_ASSERT(nfaults == 1);/* this arch only single fault tolerant */
	dag_h->creator = "ParityLoggingLargeWriteDAG";

	/* alloc the Wnd nodes, the xor node, and the Lpo node */
	nWndNodes = asmap->numStripeUnitsAccessed;
	RF_CallocAndAdd(nodes, nWndNodes + 6, sizeof(RF_DagNode_t), (RF_DagNode_t *), allocList);
	i = 0;
	wndNodes = &nodes[i];
	i += nWndNodes;
	xorNode = &nodes[i];
	i += 1;
	lpoNode = &nodes[i];
	i += 1;
	blockNode = &nodes[i];
	i += 1;
	syncNode = &nodes[i];
	i += 1;
	unblockNode = &nodes[i];
	i += 1;
	termNode = &nodes[i];
	i += 1;

	dag_h->numCommitNodes = nWndNodes + 1;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	rf_MapUnaccessedPortionOfStripe(raidPtr, layoutPtr, asmap, dag_h, new_asm_h, &nRodNodes, &sosBuffer, &eosBuffer, allocList);
	if (nRodNodes > 0)
		RF_CallocAndAdd(rodNodes, nRodNodes, sizeof(RF_DagNode_t), (RF_DagNode_t *), allocList);

	/* begin node initialization */
	rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, nRodNodes + 1, 0, 0, 0, dag_h, "Nil", allocList);
	rf_InitNode(unblockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, 1, nWndNodes + 1, 0, 0, dag_h, "Nil", allocList);
	rf_InitNode(syncNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, nWndNodes + 1, nRodNodes + 1, 0, 0, dag_h, "Nil", allocList);
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc, NULL, 0, 1, 0, 0, dag_h, "Trm", allocList);

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
		rf_InitNode(&wndNodes[i], rf_wait, RF_TRUE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, "Wnd", allocList);
		RF_ASSERT(pda != NULL);
		wndNodes[i].params[0].p = pda;
		wndNodes[i].params[1].p = pda->bufPtr;
		wndNodes[i].params[2].v = parityStripeID;
		wndNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
		pda = pda->next;
	}

	/* initialize the redundancy node */
	rf_InitNode(xorNode, rf_wait, RF_TRUE, redFunc, rf_NullNodeUndoFunc, NULL, 1, 1, 2 * (nWndNodes + nRodNodes) + 1, 1, dag_h, "Xr ", allocList);
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
	for (i = 0; i < nRodNodes; i++)
		if (((RF_PhysDiskAddr_t *) rodNodes[i].params[0].p)->numSector == raidPtr->Layout.sectorsPerStripeUnit)
			break;
	if (i == nRodNodes) {
		RF_CallocAndAdd(xorNode->results[0], 1, rf_RaidAddressToByte(raidPtr, raidPtr->Layout.sectorsPerStripeUnit), (void *), allocList);
	} else {
		xorNode->results[0] = rodNodes[i].params[1].p;
	}

	/* initialize the Lpo node */
	rf_InitNode(lpoNode, rf_wait, RF_FALSE, rf_ParityLogOverwriteFunc, rf_ParityLogOverwriteUndoFunc, rf_GenericWakeupFunc, 1, 1, 2, 0, dag_h, "Lpo", allocList);

	lpoNode->params[0].p = asmap->parityInfo;
	lpoNode->params[1].p = xorNode->results[0];
	RF_ASSERT(asmap->parityInfo->next == NULL);	/* parityInfo must
							 * describe entire
							 * parity unit */

	/* connect nodes to form graph */

	/* connect dag header to block node */
	RF_ASSERT(dag_h->numSuccedents == 1);
	RF_ASSERT(blockNode->numAntecedents == 0);
	dag_h->succedents[0] = blockNode;

	/* connect the block node to the Rod nodes */
	RF_ASSERT(blockNode->numSuccedents == nRodNodes + 1);
	for (i = 0; i < nRodNodes; i++) {
		RF_ASSERT(rodNodes[i].numAntecedents == 1);
		blockNode->succedents[i] = &rodNodes[i];
		rodNodes[i].antecedents[0] = blockNode;
		rodNodes[i].antType[0] = rf_control;
	}

	/* connect the block node to the sync node */
	/* necessary if nRodNodes == 0 */
	RF_ASSERT(syncNode->numAntecedents == nRodNodes + 1);
	blockNode->succedents[nRodNodes] = syncNode;
	syncNode->antecedents[0] = blockNode;
	syncNode->antType[0] = rf_control;

	/* connect the Rod nodes to the syncNode */
	for (i = 0; i < nRodNodes; i++) {
		rodNodes[i].succedents[0] = syncNode;
		syncNode->antecedents[1 + i] = &rodNodes[i];
		syncNode->antType[1 + i] = rf_control;
	}

	/* connect the sync node to the xor node */
	RF_ASSERT(syncNode->numSuccedents == nWndNodes + 1);
	RF_ASSERT(xorNode->numAntecedents == 1);
	syncNode->succedents[0] = xorNode;
	xorNode->antecedents[0] = syncNode;
	xorNode->antType[0] = rf_trueData;	/* carry forward from sync */

	/* connect the sync node to the Wnd nodes */
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(wndNodes->numAntecedents == 1);
		syncNode->succedents[1 + i] = &wndNodes[i];
		wndNodes[i].antecedents[0] = syncNode;
		wndNodes[i].antType[0] = rf_control;
	}

	/* connect the xor node to the Lpo node */
	RF_ASSERT(xorNode->numSuccedents == 1);
	RF_ASSERT(lpoNode->numAntecedents == 1);
	xorNode->succedents[0] = lpoNode;
	lpoNode->antecedents[0] = xorNode;
	lpoNode->antType[0] = rf_trueData;

	/* connect the Wnd nodes to the unblock node */
	RF_ASSERT(unblockNode->numAntecedents == nWndNodes + 1);
	for (i = 0; i < nWndNodes; i++) {
		RF_ASSERT(wndNodes->numSuccedents == 1);
		wndNodes[i].succedents[0] = unblockNode;
		unblockNode->antecedents[i] = &wndNodes[i];
		unblockNode->antType[i] = rf_control;
	}

	/* connect the Lpo node to the unblock node */
	RF_ASSERT(lpoNode->numSuccedents == 1);
	lpoNode->succedents[0] = unblockNode;
	unblockNode->antecedents[nWndNodes] = lpoNode;
	unblockNode->antType[nWndNodes] = rf_control;

	/* connect unblock node to terminator */
	RF_ASSERT(unblockNode->numSuccedents == 1);
	RF_ASSERT(termNode->numAntecedents == 1);
	RF_ASSERT(termNode->numSuccedents == 0);
	unblockNode->succedents[0] = termNode;
	termNode->antecedents[0] = unblockNode;
	termNode->antType[0] = rf_control;
}




/******************************************************************************
 *
 * creates a DAG to perform a small-write operation (either raid 5 or pq), which is as follows:
 *
 *                                     Header
 *                                       |
 *                                     Block
 *                                 / |  ... \   \
 *                                /  |       \   \
 *                             Rod  Rod      Rod  Rop
 *                             | \ /| \    / |  \/ |
 *                             |    |        |  /\ |
 *                             Wnd  Wnd      Wnd   X
 *                              |    \       /     |
 *                              |     \     /      |
 *                               \     \   /      Lpo
 *                                \     \ /       /
 *                                 +-> Unblock <-+
 *                                       |
 *                                       T
 *
 *
 * R = Read, W = Write, X = Xor, o = old, n = new, d = data, p = parity.
 * When the access spans a stripe unit boundary and is less than one SU in size, there will
 * be two Rop -- X -- Wnp branches.  I call this the "double-XOR" case.
 * The second output from each Rod node goes to the X node.  In the double-XOR
 * case, there are exactly 2 Rod nodes, and each sends one output to one X node.
 * There is one Rod -- Wnd -- T branch for each stripe unit being updated.
 *
 * The block and unblock nodes are unused.  See comment above CreateFaultFreeReadDAG.
 *
 * Note:  this DAG ignores all the optimizations related to making the RMWs atomic.
 *        it also has the nasty property that none of the buffers allocated for reading
 *        old data & parity can be freed until the XOR node fires.  Need to fix this.
 *
 * A null qfuncs indicates single fault tolerant
 *****************************************************************************/

void 
rf_CommonCreateParityLoggingSmallWriteDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList,
    RF_RedFuncs_t * pfuncs,
    RF_RedFuncs_t * qfuncs)
{
	RF_DagNode_t *xorNodes, *blockNode, *unblockNode, *nodes;
	RF_DagNode_t *readDataNodes, *readParityNodes;
	RF_DagNode_t *writeDataNodes, *lpuNodes;
	RF_DagNode_t *unlockDataNodes = NULL, *termNode;
	RF_PhysDiskAddr_t *pda = asmap->physInfo;
	int     numDataNodes = asmap->numStripeUnitsAccessed;
	int     numParityNodes = (asmap->parityInfo->next) ? 2 : 1;
	int     i, j, nNodes, totalNumNodes;
	RF_ReconUnitNum_t which_ru;
	int     (*func) (RF_DagNode_t * node), (*undoFunc) (RF_DagNode_t * node);
	int     (*qfunc) (RF_DagNode_t * node);
	char   *name, *qname;
	RF_StripeNum_t parityStripeID = rf_RaidAddressToParityStripeID(&(raidPtr->Layout), asmap->raidAddress, &which_ru);
	long    nfaults = qfuncs ? 2 : 1;
	int     lu_flag = (rf_enableAtomicRMW) ? 1 : 0;	/* lock/unlock flag */

	if (rf_dagDebug)
		printf("[Creating parity-logging small-write DAG]\n");
	RF_ASSERT(numDataNodes > 0);
	RF_ASSERT(nfaults == 1);
	dag_h->creator = "ParityLoggingSmallWriteDAG";

	/* DAG creation occurs in three steps: 1. count the number of nodes in
	 * the DAG 2. create the nodes 3. initialize the nodes 4. connect the
	 * nodes */

	/* Step 1. compute number of nodes in the graph */

	/* number of nodes: a read and write for each data unit a redundancy
	 * computation node for each parity node a read and Lpu for each
	 * parity unit a block and unblock node (2) a terminator node if
	 * atomic RMW an unlock node for each data unit, redundancy unit */
	totalNumNodes = (2 * numDataNodes) + numParityNodes + (2 * numParityNodes) + 3;
	if (lu_flag)
		totalNumNodes += numDataNodes;

	nNodes = numDataNodes + numParityNodes;

	dag_h->numCommitNodes = numDataNodes + numParityNodes;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	/* Step 2. create the nodes */
	RF_CallocAndAdd(nodes, totalNumNodes, sizeof(RF_DagNode_t), (RF_DagNode_t *), allocList);
	i = 0;
	blockNode = &nodes[i];
	i += 1;
	unblockNode = &nodes[i];
	i += 1;
	readDataNodes = &nodes[i];
	i += numDataNodes;
	readParityNodes = &nodes[i];
	i += numParityNodes;
	writeDataNodes = &nodes[i];
	i += numDataNodes;
	lpuNodes = &nodes[i];
	i += numParityNodes;
	xorNodes = &nodes[i];
	i += numParityNodes;
	termNode = &nodes[i];
	i += 1;
	if (lu_flag) {
		unlockDataNodes = &nodes[i];
		i += numDataNodes;
	}
	RF_ASSERT(i == totalNumNodes);

	/* Step 3. initialize the nodes */
	/* initialize block node (Nil) */
	rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, nNodes, 0, 0, 0, dag_h, "Nil", allocList);

	/* initialize unblock node (Nil) */
	rf_InitNode(unblockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, 1, nNodes, 0, 0, dag_h, "Nil", allocList);

	/* initialize terminatory node (Trm) */
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc, NULL, 0, 1, 0, 0, dag_h, "Trm", allocList);

	/* initialize nodes which read old data (Rod) */
	for (i = 0; i < numDataNodes; i++) {
		rf_InitNode(&readDataNodes[i], rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc, rf_GenericWakeupFunc, nNodes, 1, 4, 0, dag_h, "Rod", allocList);
		RF_ASSERT(pda != NULL);
		readDataNodes[i].params[0].p = pda;	/* physical disk addr
							 * desc */
		readDataNodes[i].params[1].p = rf_AllocBuffer(raidPtr, dag_h, pda, allocList);	/* buffer to hold old
												 * data */
		readDataNodes[i].params[2].v = parityStripeID;
		readDataNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, lu_flag, 0, which_ru);
		pda = pda->next;
		readDataNodes[i].propList[0] = NULL;
		readDataNodes[i].propList[1] = NULL;
	}

	/* initialize nodes which read old parity (Rop) */
	pda = asmap->parityInfo;
	i = 0;
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(pda != NULL);
		rf_InitNode(&readParityNodes[i], rf_wait, RF_FALSE, rf_DiskReadFunc, rf_DiskReadUndoFunc, rf_GenericWakeupFunc, nNodes, 1, 4, 0, dag_h, "Rop", allocList);
		readParityNodes[i].params[0].p = pda;
		readParityNodes[i].params[1].p = rf_AllocBuffer(raidPtr, dag_h, pda, allocList);	/* buffer to hold old
													 * parity */
		readParityNodes[i].params[2].v = parityStripeID;
		readParityNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
		readParityNodes[i].propList[0] = NULL;
		pda = pda->next;
	}

	/* initialize nodes which write new data (Wnd) */
	pda = asmap->physInfo;
	for (i = 0; i < numDataNodes; i++) {
		RF_ASSERT(pda != NULL);
		rf_InitNode(&writeDataNodes[i], rf_wait, RF_TRUE, rf_DiskWriteFunc, rf_DiskWriteUndoFunc, rf_GenericWakeupFunc, 1, nNodes, 4, 0, dag_h, "Wnd", allocList);
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


	/* initialize nodes which compute new parity */
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
			rf_InitNode(&xorNodes[i], rf_wait, RF_TRUE, func, undoFunc, NULL, 1, nNodes, 7, 1, dag_h, name, allocList);	/* no wakeup func for
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
		}
	} else {
		/* there is only one xor node in this case */
		rf_InitNode(&xorNodes[0], rf_wait, RF_TRUE, func, undoFunc, NULL, 1, nNodes, (2 * (numDataNodes + numDataNodes + 1) + 1), 1, dag_h, name, allocList);
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
	}

	/* initialize the log node(s) */
	pda = asmap->parityInfo;
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(pda);
		rf_InitNode(&lpuNodes[i], rf_wait, RF_FALSE, rf_ParityLogUpdateFunc, rf_ParityLogUpdateUndoFunc, rf_GenericWakeupFunc, 1, 1, 2, 0, dag_h, "Lpu", allocList);
		lpuNodes[i].params[0].p = pda;	/* PhysDiskAddr of parity */
		lpuNodes[i].params[1].p = xorNodes[i].results[0];	/* buffer pointer to
									 * parity */
		pda = pda->next;
	}


	/* Step 4. connect the nodes */

	/* connect header to block node */
	RF_ASSERT(dag_h->numSuccedents == 1);
	RF_ASSERT(blockNode->numAntecedents == 0);
	dag_h->succedents[0] = blockNode;

	/* connect block node to read old data nodes */
	RF_ASSERT(blockNode->numSuccedents == (numDataNodes + numParityNodes));
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

	/* connect read old data nodes to write new data nodes */
	for (i = 0; i < numDataNodes; i++) {
		RF_ASSERT(readDataNodes[i].numSuccedents == numDataNodes + numParityNodes);
		for (j = 0; j < numDataNodes; j++) {
			RF_ASSERT(writeDataNodes[j].numAntecedents == numDataNodes + numParityNodes);
			readDataNodes[i].succedents[j] = &writeDataNodes[j];
			writeDataNodes[j].antecedents[i] = &readDataNodes[i];
			if (i == j)
				writeDataNodes[j].antType[i] = rf_antiData;
			else
				writeDataNodes[j].antType[i] = rf_control;
		}
	}

	/* connect read old data nodes to xor nodes */
	for (i = 0; i < numDataNodes; i++)
		for (j = 0; j < numParityNodes; j++) {
			RF_ASSERT(xorNodes[j].numAntecedents == numDataNodes + numParityNodes);
			readDataNodes[i].succedents[numDataNodes + j] = &xorNodes[j];
			xorNodes[j].antecedents[i] = &readDataNodes[i];
			xorNodes[j].antType[i] = rf_trueData;
		}

	/* connect read old parity nodes to write new data nodes */
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(readParityNodes[i].numSuccedents == numDataNodes + numParityNodes);
		for (j = 0; j < numDataNodes; j++) {
			readParityNodes[i].succedents[j] = &writeDataNodes[j];
			writeDataNodes[j].antecedents[numDataNodes + i] = &readParityNodes[i];
			writeDataNodes[j].antType[numDataNodes + i] = rf_control;
		}
	}

	/* connect read old parity nodes to xor nodes */
	for (i = 0; i < numParityNodes; i++)
		for (j = 0; j < numParityNodes; j++) {
			readParityNodes[i].succedents[numDataNodes + j] = &xorNodes[j];
			xorNodes[j].antecedents[numDataNodes + i] = &readParityNodes[i];
			xorNodes[j].antType[numDataNodes + i] = rf_trueData;
		}

	/* connect xor nodes to write new parity nodes */
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(xorNodes[i].numSuccedents == 1);
		RF_ASSERT(lpuNodes[i].numAntecedents == 1);
		xorNodes[i].succedents[0] = &lpuNodes[i];
		lpuNodes[i].antecedents[0] = &xorNodes[i];
		lpuNodes[i].antType[0] = rf_trueData;
	}

	for (i = 0; i < numDataNodes; i++) {
		if (lu_flag) {
			/* connect write new data nodes to unlock nodes */
			RF_ASSERT(writeDataNodes[i].numSuccedents == 1);
			RF_ASSERT(unlockDataNodes[i].numAntecedents == 1);
			writeDataNodes[i].succedents[0] = &unlockDataNodes[i];
			unlockDataNodes[i].antecedents[0] = &writeDataNodes[i];
			unlockDataNodes[i].antType[0] = rf_control;

			/* connect unlock nodes to unblock node */
			RF_ASSERT(unlockDataNodes[i].numSuccedents == 1);
			RF_ASSERT(unblockNode->numAntecedents == (numDataNodes + (nfaults * numParityNodes)));
			unlockDataNodes[i].succedents[0] = unblockNode;
			unblockNode->antecedents[i] = &unlockDataNodes[i];
			unblockNode->antType[i] = rf_control;
		} else {
			/* connect write new data nodes to unblock node */
			RF_ASSERT(writeDataNodes[i].numSuccedents == 1);
			RF_ASSERT(unblockNode->numAntecedents == (numDataNodes + (nfaults * numParityNodes)));
			writeDataNodes[i].succedents[0] = unblockNode;
			unblockNode->antecedents[i] = &writeDataNodes[i];
			unblockNode->antType[i] = rf_control;
		}
	}

	/* connect write new parity nodes to unblock node */
	for (i = 0; i < numParityNodes; i++) {
		RF_ASSERT(lpuNodes[i].numSuccedents == 1);
		lpuNodes[i].succedents[0] = unblockNode;
		unblockNode->antecedents[numDataNodes + i] = &lpuNodes[i];
		unblockNode->antType[numDataNodes + i] = rf_control;
	}

	/* connect unblock node to terminator */
	RF_ASSERT(unblockNode->numSuccedents == 1);
	RF_ASSERT(termNode->numAntecedents == 1);
	RF_ASSERT(termNode->numSuccedents == 0);
	unblockNode->succedents[0] = termNode;
	termNode->antecedents[0] = unblockNode;
	termNode->antType[0] = rf_control;
}


void 
rf_CreateParityLoggingSmallWriteDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList,
    RF_RedFuncs_t * pfuncs,
    RF_RedFuncs_t * qfuncs)
{
	dag_h->creator = "ParityLoggingSmallWriteDAG";
	rf_CommonCreateParityLoggingSmallWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList, &rf_xorFuncs, NULL);
}


void 
rf_CreateParityLoggingLargeWriteDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList,
    int nfaults,
    int (*redFunc) (RF_DagNode_t *))
{
	dag_h->creator = "ParityLoggingSmallWriteDAG";
	rf_CommonCreateParityLoggingLargeWriteDAG(raidPtr, asmap, dag_h, bp, flags, allocList, 1, rf_RegularXorFunc);
}
#endif				/* RF_INCLUDE_PARITYLOGGING > 0 */
