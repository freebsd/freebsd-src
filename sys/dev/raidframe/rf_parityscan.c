/*	$FreeBSD$ */
/*	$NetBSD: rf_parityscan.c,v 1.9 2000/05/28 03:00:31 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
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

/*****************************************************************************
 *
 * rf_parityscan.c -- misc utilities related to parity verification
 *
 *****************************************************************************/

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_dagfuncs.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_mcpair.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_engine.h>
#include <dev/raidframe/rf_parityscan.h>
#include <dev/raidframe/rf_map.h>
#include <dev/raidframe/rf_kintf.h>

/*****************************************************************************************
 *
 * walk through the entire arry and write new parity.
 * This works by creating two DAGs, one to read a stripe of data and one to
 * write new parity.  The first is executed, the data is xored together, and
 * then the second is executed.  To avoid constantly building and tearing down
 * the DAGs, we create them a priori and fill them in with the mapping
 * information as we go along.
 *
 * there should never be more than one thread running this.
 *
 ****************************************************************************************/

int 
rf_RewriteParity(raidPtr)
	RF_Raid_t *raidPtr;
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_AccessStripeMapHeader_t *asm_h;
	int ret_val;
	int rc;
	RF_PhysDiskAddr_t pda;
	RF_SectorNum_t i;

	if (raidPtr->Layout.map->faultsTolerated == 0) {
		/* There isn't any parity. Call it "okay." */
		return (RF_PARITY_OKAY);
	}
	if (raidPtr->status[0] != rf_rs_optimal) {
		/*
		 * We're in degraded mode.  Don't try to verify parity now! 
		 * XXX: this should be a "we don't want to", not a 
		 * "we can't" error. 
		 */
		return (RF_PARITY_COULD_NOT_VERIFY);
	}

	ret_val = 0;

	pda.startSector = 0;
	pda.numSector = raidPtr->Layout.sectorsPerStripeUnit;
	rc = RF_PARITY_OKAY;

	for (i = 0; i < raidPtr->totalSectors && 
		     rc <= RF_PARITY_CORRECTED; 
	     i += layoutPtr->dataSectorsPerStripe) {
		if (raidPtr->waitShutdown) {
			/* Someone is pulling the plug on this set...
			   abort the re-write */
			return (1);
		}
		asm_h = rf_MapAccess(raidPtr, i, 
				     layoutPtr->dataSectorsPerStripe, 
				     NULL, RF_DONT_REMAP);
		raidPtr->parity_rewrite_stripes_done = 
			i / layoutPtr->dataSectorsPerStripe ;
		rc = rf_VerifyParity(raidPtr, asm_h->stripeMap, 1, 0);

		switch (rc) {
		case RF_PARITY_OKAY:
		case RF_PARITY_CORRECTED:
			break;
		case RF_PARITY_BAD:
			printf("Parity bad during correction\n");
			ret_val = 1;
			break;
		case RF_PARITY_COULD_NOT_CORRECT:
			printf("Could not correct bad parity\n");
			ret_val = 1;
			break;
		case RF_PARITY_COULD_NOT_VERIFY:
			printf("Could not verify parity\n");
			ret_val = 1;
			break;
		default:
			printf("Bad rc=%d from VerifyParity in RewriteParity\n", rc);
			ret_val = 1;
		}
		rf_FreeAccessStripeMap(asm_h);
	}
	return (ret_val);
}
/*****************************************************************************************
 *
 * verify that the parity in a particular stripe is correct.
 * we validate only the range of parity defined by parityPDA, since
 * this is all we have locked.  The way we do this is to create an asm
 * that maps the whole stripe and then range-restrict it to the parity
 * region defined by the parityPDA.
 *
 ****************************************************************************************/
int 
rf_VerifyParity(raidPtr, aasm, correct_it, flags)
	RF_Raid_t *raidPtr;
	RF_AccessStripeMap_t *aasm;
	int     correct_it;
	RF_RaidAccessFlags_t flags;
{
	RF_PhysDiskAddr_t *parityPDA;
	RF_AccessStripeMap_t *doasm;
	RF_LayoutSW_t *lp;
	int     lrc, rc;

	lp = raidPtr->Layout.map;
	if (lp->faultsTolerated == 0) {
		/*
	         * There isn't any parity. Call it "okay."
	         */
		return (RF_PARITY_OKAY);
	}
	rc = RF_PARITY_OKAY;
	if (lp->VerifyParity) {
		for (doasm = aasm; doasm; doasm = doasm->next) {
			for (parityPDA = doasm->parityInfo; parityPDA; 
			     parityPDA = parityPDA->next) {
				lrc = lp->VerifyParity(raidPtr, 
						       doasm->raidAddress, 
						       parityPDA,
						       correct_it, flags);
				if (lrc > rc) {
					/* see rf_parityscan.h for why this
					 * works */
					rc = lrc;
				}
			}
		}
	} else {
		rc = RF_PARITY_COULD_NOT_VERIFY;
	}
	return (rc);
}

int 
rf_VerifyParityBasic(raidPtr, raidAddr, parityPDA, correct_it, flags)
	RF_Raid_t *raidPtr;
	RF_RaidAddr_t raidAddr;
	RF_PhysDiskAddr_t *parityPDA;
	int     correct_it;
	RF_RaidAccessFlags_t flags;
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_RaidAddr_t startAddr = rf_RaidAddressOfPrevStripeBoundary(layoutPtr,
								     raidAddr);
	RF_SectorCount_t numsector = parityPDA->numSector;
	int     numbytes = rf_RaidAddressToByte(raidPtr, numsector);
	int     bytesPerStripe = numbytes * layoutPtr->numDataCol;
	RF_DagHeader_t *rd_dag_h, *wr_dag_h;	/* read, write dag */
	RF_DagNode_t *blockNode, *unblockNode, *wrBlock, *wrUnblock;
	RF_AccessStripeMapHeader_t *asm_h;
	RF_AccessStripeMap_t *asmap;
	RF_AllocListElem_t *alloclist;
	RF_PhysDiskAddr_t *pda;
	char   *pbuf, *buf, *end_p, *p;
	int     i, retcode;
	RF_ReconUnitNum_t which_ru;
	RF_StripeNum_t psID = rf_RaidAddressToParityStripeID(layoutPtr, 
							     raidAddr, 
							     &which_ru);
	int     stripeWidth = layoutPtr->numDataCol + layoutPtr->numParityCol;
	RF_AccTraceEntry_t tracerec;
	RF_MCPair_t *mcpair;

	retcode = RF_PARITY_OKAY;

	mcpair = rf_AllocMCPair();
	rf_MakeAllocList(alloclist);
	RF_MallocAndAdd(buf, numbytes * (layoutPtr->numDataCol + layoutPtr->numParityCol), (char *), alloclist);
	RF_CallocAndAdd(pbuf, 1, numbytes, (char *), alloclist);	/* use calloc to make
									 * sure buffer is zeroed */
	end_p = buf + bytesPerStripe;

	rd_dag_h = rf_MakeSimpleDAG(raidPtr, stripeWidth, numbytes, buf, rf_DiskReadFunc, rf_DiskReadUndoFunc,
	    "Rod", alloclist, flags, RF_IO_NORMAL_PRIORITY);
	blockNode = rd_dag_h->succedents[0];
	unblockNode = blockNode->succedents[0]->succedents[0];

	/* map the stripe and fill in the PDAs in the dag */
	asm_h = rf_MapAccess(raidPtr, startAddr, layoutPtr->dataSectorsPerStripe, buf, RF_DONT_REMAP);
	asmap = asm_h->stripeMap;

	for (pda = asmap->physInfo, i = 0; i < layoutPtr->numDataCol; i++, pda = pda->next) {
		RF_ASSERT(pda);
		rf_RangeRestrictPDA(raidPtr, parityPDA, pda, 0, 1);
		RF_ASSERT(pda->numSector != 0);
		if (rf_TryToRedirectPDA(raidPtr, pda, 0))
			goto out;	/* no way to verify parity if disk is
					 * dead.  return w/ good status */
		blockNode->succedents[i]->params[0].p = pda;
		blockNode->succedents[i]->params[2].v = psID;
		blockNode->succedents[i]->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
	}

	RF_ASSERT(!asmap->parityInfo->next);
	rf_RangeRestrictPDA(raidPtr, parityPDA, asmap->parityInfo, 0, 1);
	RF_ASSERT(asmap->parityInfo->numSector != 0);
	if (rf_TryToRedirectPDA(raidPtr, asmap->parityInfo, 1))
		goto out;
	blockNode->succedents[layoutPtr->numDataCol]->params[0].p = asmap->parityInfo;

	/* fire off the DAG */
	bzero((char *) &tracerec, sizeof(tracerec));
	rd_dag_h->tracerec = &tracerec;

	if (rf_verifyParityDebug) {
		printf("Parity verify read dag:\n");
		rf_PrintDAGList(rd_dag_h);
	}
	RF_LOCK_MUTEX(mcpair->mutex);
	mcpair->flag = 0;
	rf_DispatchDAG(rd_dag_h, (void (*) (void *)) rf_MCPairWakeupFunc,
	    (void *) mcpair);
	while (!mcpair->flag)
		RF_WAIT_COND(mcpair->cond, mcpair->mutex);
	RF_UNLOCK_MUTEX(mcpair->mutex);
	if (rd_dag_h->status != rf_enable) {
		RF_ERRORMSG("Unable to verify parity:  can't read the stripe\n");
		retcode = RF_PARITY_COULD_NOT_VERIFY;
		goto out;
	}
	for (p = buf; p < end_p; p += numbytes) {
		rf_bxor(p, pbuf, numbytes, NULL);
	}
	for (i = 0; i < numbytes; i++) {
#if 0
		if (pbuf[i] != 0 || buf[bytesPerStripe + i] != 0) {
			printf("Bytes: %d %d %d\n", i, pbuf[i], buf[bytesPerStripe + i]);
		}
#endif
		if (pbuf[i] != buf[bytesPerStripe + i]) {
			if (!correct_it)
				RF_ERRORMSG3("Parity verify error: byte %d of parity is 0x%x should be 0x%x\n",
				    i, (u_char) buf[bytesPerStripe + i], (u_char) pbuf[i]);
			retcode = RF_PARITY_BAD;
			break;
		}
	}

	if (retcode && correct_it) {
		wr_dag_h = rf_MakeSimpleDAG(raidPtr, 1, numbytes, pbuf, rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
		    "Wnp", alloclist, flags, RF_IO_NORMAL_PRIORITY);
		wrBlock = wr_dag_h->succedents[0];
		wrUnblock = wrBlock->succedents[0]->succedents[0];
		wrBlock->succedents[0]->params[0].p = asmap->parityInfo;
		wrBlock->succedents[0]->params[2].v = psID;
		wrBlock->succedents[0]->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
		bzero((char *) &tracerec, sizeof(tracerec));
		wr_dag_h->tracerec = &tracerec;
		if (rf_verifyParityDebug) {
			printf("Parity verify write dag:\n");
			rf_PrintDAGList(wr_dag_h);
		}
		RF_LOCK_MUTEX(mcpair->mutex);
		mcpair->flag = 0;
		rf_DispatchDAG(wr_dag_h, (void (*) (void *)) rf_MCPairWakeupFunc,
		    (void *) mcpair);
		while (!mcpair->flag)
			RF_WAIT_COND(mcpair->cond, mcpair->mutex);
		RF_UNLOCK_MUTEX(mcpair->mutex);
		if (wr_dag_h->status != rf_enable) {
			RF_ERRORMSG("Unable to correct parity in VerifyParity:  can't write the stripe\n");
			retcode = RF_PARITY_COULD_NOT_CORRECT;
		}
		rf_FreeDAG(wr_dag_h);
		if (retcode == RF_PARITY_BAD)
			retcode = RF_PARITY_CORRECTED;
	}
out:
	rf_FreeAccessStripeMap(asm_h);
	rf_FreeAllocList(alloclist);
	rf_FreeDAG(rd_dag_h);
	rf_FreeMCPair(mcpair);
	return (retcode);
}

int 
rf_TryToRedirectPDA(raidPtr, pda, parity)
	RF_Raid_t *raidPtr;
	RF_PhysDiskAddr_t *pda;
	int     parity;
{
	if (raidPtr->Disks[pda->row][pda->col].status == rf_ds_reconstructing) {
		if (rf_CheckRUReconstructed(raidPtr->reconControl[pda->row]->reconMap, pda->startSector)) {
			if (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) {
				RF_RowCol_t or = pda->row, oc = pda->col;
				RF_SectorNum_t os = pda->startSector;
				if (parity) {
					(raidPtr->Layout.map->MapParity) (raidPtr, pda->raidAddress, &pda->row, &pda->col, &pda->startSector, RF_REMAP);
					if (rf_verifyParityDebug)
						printf("VerifyParity: Redir P r %d c %d sect %ld -> r %d c %d sect %ld\n",
						    or, oc, (long) os, pda->row, pda->col, (long) pda->startSector);
				} else {
					(raidPtr->Layout.map->MapSector) (raidPtr, pda->raidAddress, &pda->row, &pda->col, &pda->startSector, RF_REMAP);
					if (rf_verifyParityDebug)
						printf("VerifyParity: Redir D r %d c %d sect %ld -> r %d c %d sect %ld\n",
						    or, oc, (long) os, pda->row, pda->col, (long) pda->startSector);
				}
			} else {
				RF_RowCol_t spRow = raidPtr->Disks[pda->row][pda->col].spareRow;
				RF_RowCol_t spCol = raidPtr->Disks[pda->row][pda->col].spareCol;
				pda->row = spRow;
				pda->col = spCol;
			}
		}
	}
	if (RF_DEAD_DISK(raidPtr->Disks[pda->row][pda->col].status))
		return (1);
	return (0);
}
/*****************************************************************************************
 *
 * currently a stub.
 *
 * takes as input an ASM describing a write operation and containing one failure, and
 * verifies that the parity was correctly updated to reflect the write.
 *
 * if it's a data unit that's failed, we read the other data units in the stripe and
 * the parity unit, XOR them together, and verify that we get the data intended for
 * the failed disk.  Since it's easy, we also validate that the right data got written
 * to the surviving data disks.
 *
 * If it's the parity that failed, there's really no validation we can do except the
 * above verification that the right data got written to all disks.  This is because
 * the new data intended for the failed disk is supplied in the ASM, but this is of
 * course not the case for the new parity.
 *
 ****************************************************************************************/
int 
rf_VerifyDegrModeWrite(raidPtr, asmh)
	RF_Raid_t *raidPtr;
	RF_AccessStripeMapHeader_t *asmh;
{
	return (0);
}
/* creates a simple DAG with a header, a block-recon node at level 1,
 * nNodes nodes at level 2, an unblock-recon node at level 3, and
 * a terminator node at level 4.  The stripe address field in
 * the block and unblock nodes are not touched, nor are the pda
 * fields in the second-level nodes, so they must be filled in later.
 *
 * commit point is established at unblock node - this means that any
 * failure during dag execution causes the dag to fail
 */
RF_DagHeader_t *
rf_MakeSimpleDAG(raidPtr, nNodes, bytesPerSU, databuf, doFunc, undoFunc, name, alloclist, flags, priority)
	RF_Raid_t *raidPtr;
	int     nNodes;
	int     bytesPerSU;
	char   *databuf;
	int     (*doFunc) (RF_DagNode_t * node);
	int     (*undoFunc) (RF_DagNode_t * node);
	char   *name;		/* node names at the second level */
	RF_AllocListElem_t *alloclist;
	RF_RaidAccessFlags_t flags;
	int     priority;
{
	RF_DagHeader_t *dag_h;
	RF_DagNode_t *nodes, *termNode, *blockNode, *unblockNode;
	int     i;

	/* create the nodes, the block & unblock nodes, and the terminator
	 * node */
	RF_CallocAndAdd(nodes, nNodes + 3, sizeof(RF_DagNode_t), (RF_DagNode_t *), alloclist);
	blockNode = &nodes[nNodes];
	unblockNode = blockNode + 1;
	termNode = unblockNode + 1;

	dag_h = rf_AllocDAGHeader();
	dag_h->raidPtr = (void *) raidPtr;
	dag_h->allocList = NULL;/* we won't use this alloc list */
	dag_h->status = rf_enable;
	dag_h->numSuccedents = 1;
	dag_h->creator = "SimpleDAG";

	/* this dag can not commit until the unblock node is reached errors
	 * prior to the commit point imply the dag has failed */
	dag_h->numCommitNodes = 1;
	dag_h->numCommits = 0;

	dag_h->succedents[0] = blockNode;
	rf_InitNode(blockNode, rf_wait, RF_FALSE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, nNodes, 0, 0, 0, dag_h, "Nil", alloclist);
	rf_InitNode(unblockNode, rf_wait, RF_TRUE, rf_NullNodeFunc, rf_NullNodeUndoFunc, NULL, 1, nNodes, 0, 0, dag_h, "Nil", alloclist);
	unblockNode->succedents[0] = termNode;
	for (i = 0; i < nNodes; i++) {
		blockNode->succedents[i] = unblockNode->antecedents[i] = &nodes[i];
		unblockNode->antType[i] = rf_control;
		rf_InitNode(&nodes[i], rf_wait, RF_FALSE, doFunc, undoFunc, rf_GenericWakeupFunc, 1, 1, 4, 0, dag_h, name, alloclist);
		nodes[i].succedents[0] = unblockNode;
		nodes[i].antecedents[0] = blockNode;
		nodes[i].antType[0] = rf_control;
		nodes[i].params[1].p = (databuf + (i * bytesPerSU));
	}
	rf_InitNode(termNode, rf_wait, RF_FALSE, rf_TerminateFunc, rf_TerminateUndoFunc, NULL, 0, 1, 0, 0, dag_h, "Trm", alloclist);
	termNode->antecedents[0] = unblockNode;
	termNode->antType[0] = rf_control;
	return (dag_h);
}
