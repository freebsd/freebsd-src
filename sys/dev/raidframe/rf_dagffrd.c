/*	$NetBSD: rf_dagffrd.c,v 1.4 2000/01/07 03:40:58 oster Exp $	*/

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
 * rf_dagffrd.c
 *
 * code for creating fault-free read DAGs
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
#include <dev/raidframe/rf_dagffrd.h>

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
rf_CreateFaultFreeReadDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList)
{
	rf_CreateNonredundantDAG(raidPtr, asmap, dag_h, bp, flags, allocList,
	    RF_IO_TYPE_READ);
}


/******************************************************************************
 *
 * DAG creation code begins here
 */

/******************************************************************************
 *
 * creates a DAG to perform a nonredundant read or write of data within one
 * stripe.
 * For reads, this DAG is as follows:
 *
 *                   /---- read ----\
 *    Header -- Block ---- read ---- Commit -- Terminate
 *                   \---- read ----/
 *
 * For writes, this DAG is as follows:
 *
 *                    /---- write ----\
 *    Header -- Commit ---- write ---- Block -- Terminate
 *                    \---- write ----/
 *
 * There is one disk node per stripe unit accessed, and all disk nodes are in
 * parallel.
 *
 * Tricky point here:  The first disk node (read or write) is created
 * normally.  Subsequent disk nodes are created by copying the first one,
 * and modifying a few params.  The "succedents" and "antecedents" fields are
 * _not_ re-created in each node, but rather left pointing to the same array
 * that was malloc'd when the first node was created.  Thus, it's essential
 * that when this DAG is freed, the succedents and antecedents fields be freed
 * in ONLY ONE of the read nodes.  This does not apply to the "params" field
 * because it is recreated for each READ node.
 *
 * Note that normal-priority accesses do not need to be tagged with their
 * parity stripe ID, because they will never be promoted.  Hence, I've
 * commented-out the code to do this, and marked it with UNNEEDED.
 *
 *****************************************************************************/

void 
rf_CreateNonredundantDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList,
    RF_IoType_t type)
{
	RF_DagNode_t *nodes, *diskNodes, *blockNode, *commitNode, *termNode;
	RF_PhysDiskAddr_t *pda = asmap->physInfo;
	int     (*doFunc) (RF_DagNode_t *), (*undoFunc) (RF_DagNode_t *);
	int     i, n, totalNumNodes;
	char   *name;

	n = asmap->numStripeUnitsAccessed;
	dag_h->creator = "NonredundantDAG";

	RF_ASSERT(RF_IO_IS_R_OR_W(type));
	switch (type) {
	case RF_IO_TYPE_READ:
		doFunc = rf_DiskReadFunc;
		undoFunc = rf_DiskReadUndoFunc;
		name = "R  ";
		if (rf_dagDebug)
			printf("[Creating non-redundant read DAG]\n");
		break;
	case RF_IO_TYPE_WRITE:
		doFunc = rf_DiskWriteFunc;
		undoFunc = rf_DiskWriteUndoFunc;
		name = "W  ";
		if (rf_dagDebug)
			printf("[Creating non-redundant write DAG]\n");
		break;
	default:
		RF_PANIC();
	}

	/*
         * For reads, the dag can not commit until the block node is reached.
         * for writes, the dag commits immediately.
         */
	dag_h->numCommitNodes = 1;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	/*
         * Node count:
         * 1 block node
         * n data reads (or writes)
         * 1 commit node
         * 1 terminator node
         */
	RF_ASSERT(n > 0);
	totalNumNodes = n + 3;
	RF_CallocAndAdd(nodes, totalNumNodes, sizeof(RF_DagNode_t),
	    (RF_DagNode_t *), allocList);
	i = 0;
	diskNodes = &nodes[i];
	i += n;
	blockNode = &nodes[i];
	i += 1;
	commitNode = &nodes[i];
	i += 1;
	termNode = &nodes[i];
	i += 1;
	RF_ASSERT(i == totalNumNodes);

	/* initialize nodes */
	switch (type) {
	case RF_IO_TYPE_READ:
		rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
		    NULL, n, 0, 0, 0, dag_h, "Nil", allocList);
		rf_InitNode(commitNode, rf_wait, RF_TRUE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
		    NULL, 1, n, 0, 0, dag_h, "Cmt", allocList);
		rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc,
		    NULL, 0, 1, 0, 0, dag_h, "Trm", allocList);
		break;
	case RF_IO_TYPE_WRITE:
		rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
		    NULL, 1, 0, 0, 0, dag_h, "Nil", allocList);
		rf_InitNode(commitNode, rf_wait, RF_TRUE, rf_NullNodeFunc, rf_NullNodeUndoFunc,
		    NULL, n, 1, 0, 0, dag_h, "Cmt", allocList);
		rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc,
		    NULL, 0, n, 0, 0, dag_h, "Trm", allocList);
		break;
	default:
		RF_PANIC();
	}

	for (i = 0; i < n; i++) {
		RF_ASSERT(pda != NULL);
		rf_InitNode(&diskNodes[i], rf_wait, RF_FALSE, doFunc, undoFunc, rf_GenericWakeupFunc,
		    1, 1, 4, 0, dag_h, name, allocList);
		diskNodes[i].params[0].p = pda;
		diskNodes[i].params[1].p = pda->bufPtr;
		/* parity stripe id is not necessary */
		diskNodes[i].params[2].v = 0;
		diskNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, 0);
		pda = pda->next;
	}

	/*
         * Connect nodes.
         */

	/* connect hdr to block node */
	RF_ASSERT(blockNode->numAntecedents == 0);
	dag_h->succedents[0] = blockNode;

	if (type == RF_IO_TYPE_READ) {
		/* connecting a nonredundant read DAG */
		RF_ASSERT(blockNode->numSuccedents == n);
		RF_ASSERT(commitNode->numAntecedents == n);
		for (i = 0; i < n; i++) {
			/* connect block node to each read node */
			RF_ASSERT(diskNodes[i].numAntecedents == 1);
			blockNode->succedents[i] = &diskNodes[i];
			diskNodes[i].antecedents[0] = blockNode;
			diskNodes[i].antType[0] = rf_control;

			/* connect each read node to the commit node */
			RF_ASSERT(diskNodes[i].numSuccedents == 1);
			diskNodes[i].succedents[0] = commitNode;
			commitNode->antecedents[i] = &diskNodes[i];
			commitNode->antType[i] = rf_control;
		}
		/* connect the commit node to the term node */
		RF_ASSERT(commitNode->numSuccedents == 1);
		RF_ASSERT(termNode->numAntecedents == 1);
		RF_ASSERT(termNode->numSuccedents == 0);
		commitNode->succedents[0] = termNode;
		termNode->antecedents[0] = commitNode;
		termNode->antType[0] = rf_control;
	} else {
		/* connecting a nonredundant write DAG */
		/* connect the block node to the commit node */
		RF_ASSERT(blockNode->numSuccedents == 1);
		RF_ASSERT(commitNode->numAntecedents == 1);
		blockNode->succedents[0] = commitNode;
		commitNode->antecedents[0] = blockNode;
		commitNode->antType[0] = rf_control;

		RF_ASSERT(commitNode->numSuccedents == n);
		RF_ASSERT(termNode->numAntecedents == n);
		RF_ASSERT(termNode->numSuccedents == 0);
		for (i = 0; i < n; i++) {
			/* connect the commit node to each write node */
			RF_ASSERT(diskNodes[i].numAntecedents == 1);
			commitNode->succedents[i] = &diskNodes[i];
			diskNodes[i].antecedents[0] = commitNode;
			diskNodes[i].antType[0] = rf_control;

			/* connect each write node to the term node */
			RF_ASSERT(diskNodes[i].numSuccedents == 1);
			diskNodes[i].succedents[0] = termNode;
			termNode->antecedents[i] = &diskNodes[i];
			termNode->antType[i] = rf_control;
		}
	}
}
/******************************************************************************
 * Create a fault-free read DAG for RAID level 1
 *
 * Hdr -> Nil -> Rmir -> Cmt -> Trm
 *
 * The "Rmir" node schedules a read from the disk in the mirror pair with the
 * shortest disk queue.  the proper queue is selected at Rmir execution.  this
 * deferred mapping is unlike other archs in RAIDframe which generally fix
 * mapping at DAG creation time.
 *
 * Parameters:  raidPtr   - description of the physical array
 *              asmap     - logical & physical addresses for this access
 *              bp        - buffer ptr (for holding read data)
 *              flags     - general flags (e.g. disk locking)
 *              allocList - list of memory allocated in DAG creation
 *****************************************************************************/

static void 
CreateMirrorReadDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList,
    int (*readfunc) (RF_DagNode_t * node))
{
	RF_DagNode_t *readNodes, *nodes, *blockNode, *commitNode, *termNode;
	RF_PhysDiskAddr_t *data_pda = asmap->physInfo;
	RF_PhysDiskAddr_t *parity_pda = asmap->parityInfo;
	int     i, n, totalNumNodes;

	n = asmap->numStripeUnitsAccessed;
	dag_h->creator = "RaidOneReadDAG";
	if (rf_dagDebug) {
		printf("[Creating RAID level 1 read DAG]\n");
	}
	/*
         * This dag can not commit until the commit node is reached
         * errors prior to the commit point imply the dag has failed.
         */
	dag_h->numCommitNodes = 1;
	dag_h->numCommits = 0;
	dag_h->numSuccedents = 1;

	/*
         * Node count:
         * n data reads
         * 1 block node
         * 1 commit node
         * 1 terminator node
         */
	RF_ASSERT(n > 0);
	totalNumNodes = n + 3;
	RF_CallocAndAdd(nodes, totalNumNodes, sizeof(RF_DagNode_t),
	    (RF_DagNode_t *), allocList);
	i = 0;
	readNodes = &nodes[i];
	i += n;
	blockNode = &nodes[i];
	i += 1;
	commitNode = &nodes[i];
	i += 1;
	termNode = &nodes[i];
	i += 1;
	RF_ASSERT(i == totalNumNodes);

	/* initialize nodes */
	rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc,
	    rf_NullNodeUndoFunc, NULL, n, 0, 0, 0, dag_h, "Nil", allocList);
	rf_InitNode(commitNode, rf_wait, RF_TRUE, rf_NullNodeFunc,
	    rf_NullNodeUndoFunc, NULL, 1, n, 0, 0, dag_h, "Cmt", allocList);
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc,
	    rf_TerminateUndoFunc, NULL, 0, 1, 0, 0, dag_h, "Trm", allocList);

	for (i = 0; i < n; i++) {
		RF_ASSERT(data_pda != NULL);
		RF_ASSERT(parity_pda != NULL);
		rf_InitNode(&readNodes[i], rf_wait, RF_FALSE, readfunc,
		    rf_DiskReadMirrorUndoFunc, rf_GenericWakeupFunc, 1, 1, 5, 0, dag_h,
		    "Rmir", allocList);
		readNodes[i].params[0].p = data_pda;
		readNodes[i].params[1].p = data_pda->bufPtr;
		/* parity stripe id is not necessary */
		readNodes[i].params[2].p = 0;
		readNodes[i].params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, 0);
		readNodes[i].params[4].p = parity_pda;
		data_pda = data_pda->next;
		parity_pda = parity_pda->next;
	}

	/*
         * Connect nodes
         */

	/* connect hdr to block node */
	RF_ASSERT(blockNode->numAntecedents == 0);
	dag_h->succedents[0] = blockNode;

	/* connect block node to read nodes */
	RF_ASSERT(blockNode->numSuccedents == n);
	for (i = 0; i < n; i++) {
		RF_ASSERT(readNodes[i].numAntecedents == 1);
		blockNode->succedents[i] = &readNodes[i];
		readNodes[i].antecedents[0] = blockNode;
		readNodes[i].antType[0] = rf_control;
	}

	/* connect read nodes to commit node */
	RF_ASSERT(commitNode->numAntecedents == n);
	for (i = 0; i < n; i++) {
		RF_ASSERT(readNodes[i].numSuccedents == 1);
		readNodes[i].succedents[0] = commitNode;
		commitNode->antecedents[i] = &readNodes[i];
		commitNode->antType[i] = rf_control;
	}

	/* connect commit node to term node */
	RF_ASSERT(commitNode->numSuccedents == 1);
	RF_ASSERT(termNode->numAntecedents == 1);
	RF_ASSERT(termNode->numSuccedents == 0);
	commitNode->succedents[0] = termNode;
	termNode->antecedents[0] = commitNode;
	termNode->antType[0] = rf_control;
}

void 
rf_CreateMirrorIdleReadDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList)
{
	CreateMirrorReadDAG(raidPtr, asmap, dag_h, bp, flags, allocList,
	    rf_DiskReadMirrorIdleFunc);
}

void 
rf_CreateMirrorPartitionReadDAG(
    RF_Raid_t * raidPtr,
    RF_AccessStripeMap_t * asmap,
    RF_DagHeader_t * dag_h,
    void *bp,
    RF_RaidAccessFlags_t flags,
    RF_AllocListElem_t * allocList)
{
	CreateMirrorReadDAG(raidPtr, asmap, dag_h, bp, flags, allocList,
	    rf_DiskReadMirrorPartitionFunc);
}
