/*	$FreeBSD$ */
/*	$NetBSD: rf_evenodd.c,v 1.4 2000/01/07 03:40:59 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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

/*****************************************************************************************
 *
 * rf_evenodd.c -- implements EVENODD array architecture
 *
 ****************************************************************************************/

#include <dev/raidframe/rf_archs.h>

#if RF_INCLUDE_EVENODD > 0

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
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_evenodd.h>
#include <dev/raidframe/rf_configure.h>
#include <dev/raidframe/rf_parityscan.h>
#include <dev/raidframe/rf_utils.h>
#include <dev/raidframe/rf_map.h>
#include <dev/raidframe/rf_pq.h>
#include <dev/raidframe/rf_mcpair.h>
#include <dev/raidframe/rf_evenodd.h>
#include <dev/raidframe/rf_evenodd_dagfuncs.h>
#include <dev/raidframe/rf_evenodd_dags.h>
#include <dev/raidframe/rf_engine.h>
#include <dev/raidframe/rf_kintf.h>

typedef struct RF_EvenOddConfigInfo_s {
	RF_RowCol_t **stripeIdentifier;	/* filled in at config time & used by
					 * IdentifyStripe */
}       RF_EvenOddConfigInfo_t;

int 
rf_ConfigureEvenOdd(listp, raidPtr, cfgPtr)
	RF_ShutdownList_t **listp;
	RF_Raid_t *raidPtr;
	RF_Config_t *cfgPtr;
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_EvenOddConfigInfo_t *info;
	RF_RowCol_t i, j, startdisk;

	RF_MallocAndAdd(info, sizeof(RF_EvenOddConfigInfo_t), (RF_EvenOddConfigInfo_t *), raidPtr->cleanupList);
	layoutPtr->layoutSpecificInfo = (void *) info;

	RF_ASSERT(raidPtr->numRow == 1);

	info->stripeIdentifier = rf_make_2d_array(raidPtr->numCol, raidPtr->numCol, raidPtr->cleanupList);
	startdisk = 0;
	for (i = 0; i < raidPtr->numCol; i++) {
		for (j = 0; j < raidPtr->numCol; j++) {
			info->stripeIdentifier[i][j] = (startdisk + j) % raidPtr->numCol;
		}
		if ((startdisk -= 2) < 0)
			startdisk += raidPtr->numCol;
	}

	/* fill in the remaining layout parameters */
	layoutPtr->numStripe = layoutPtr->stripeUnitsPerDisk;
	layoutPtr->bytesPerStripeUnit = layoutPtr->sectorsPerStripeUnit << raidPtr->logBytesPerSector;
	layoutPtr->numDataCol = raidPtr->numCol - 2;	/* ORIG:
							 * layoutPtr->numDataCol
							 * = raidPtr->numCol-1;  */
#if RF_EO_MATRIX_DIM > 17
	if (raidPtr->numCol <= 17) {
		printf("Number of stripe units in a parity stripe is smaller than 17. Please\n");
		printf("define the macro RF_EO_MATRIX_DIM in file rf_evenodd_dagfuncs.h to \n");
		printf("be 17 to increase performance. \n");
		return (EINVAL);
	}
#elif RF_EO_MATRIX_DIM == 17
	if (raidPtr->numCol > 17) {
		printf("Number of stripe units in a parity stripe is bigger than 17. Please\n");
		printf("define the macro RF_EO_MATRIX_DIM in file rf_evenodd_dagfuncs.h to \n");
		printf("be 257 for encoding and decoding functions to work. \n");
		return (EINVAL);
	}
#endif
	layoutPtr->dataSectorsPerStripe = layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;
	layoutPtr->numParityCol = 2;
	layoutPtr->dataStripeUnitsPerDisk = layoutPtr->stripeUnitsPerDisk;
	raidPtr->sectorsPerDisk = layoutPtr->stripeUnitsPerDisk * layoutPtr->sectorsPerStripeUnit;

	raidPtr->totalSectors = layoutPtr->stripeUnitsPerDisk * layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;

	return (0);
}

int 
rf_GetDefaultNumFloatingReconBuffersEvenOdd(RF_Raid_t * raidPtr)
{
	return (20);
}

RF_HeadSepLimit_t 
rf_GetDefaultHeadSepLimitEvenOdd(RF_Raid_t * raidPtr)
{
	return (10);
}

void 
rf_IdentifyStripeEvenOdd(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t addr,
    RF_RowCol_t ** diskids,
    RF_RowCol_t * outRow)
{
	RF_StripeNum_t stripeID = rf_RaidAddressToStripeID(&raidPtr->Layout, addr);
	RF_EvenOddConfigInfo_t *info = (RF_EvenOddConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;

	*outRow = 0;
	*diskids = info->stripeIdentifier[stripeID % raidPtr->numCol];
}
/* The layout of stripe unit on the disks are:      c0 c1 c2 c3 c4

 						     0  1  2  E  P
						     5  E  P  3  4
						     P  6  7  8  E
	 					    10 11  E  P  9
						     E  P 12 13 14
						     ....

  We use the MapSectorRAID5 to map data information because the routine can be shown to map exactly
  the layout of data stripe unit as shown above although we have 2 redundant information now.
  But for E and P, we use rf_MapEEvenOdd and rf_MapParityEvenOdd which are different method from raid-5.
*/


void 
rf_MapParityEvenOdd(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t raidSector,
    RF_RowCol_t * row,
    RF_RowCol_t * col,
    RF_SectorNum_t * diskSector,
    int remap)
{
	RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
	RF_StripeNum_t endSUIDofthisStrip = (SUID / raidPtr->Layout.numDataCol + 1) * raidPtr->Layout.numDataCol - 1;

	*row = 0;
	*col = (endSUIDofthisStrip + 2) % raidPtr->numCol;
	*diskSector = (SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
	    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void 
rf_MapEEvenOdd(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t raidSector,
    RF_RowCol_t * row,
    RF_RowCol_t * col,
    RF_SectorNum_t * diskSector,
    int remap)
{
	RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
	RF_StripeNum_t endSUIDofthisStrip = (SUID / raidPtr->Layout.numDataCol + 1) * raidPtr->Layout.numDataCol - 1;

	*row = 0;
	*col = (endSUIDofthisStrip + 1) % raidPtr->numCol;
	*diskSector = (SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
	    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void 
rf_EODagSelect(
    RF_Raid_t * raidPtr,
    RF_IoType_t type,
    RF_AccessStripeMap_t * asmap,
    RF_VoidFuncPtr * createFunc)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	unsigned ndfail = asmap->numDataFailed;
	unsigned npfail = asmap->numParityFailed + asmap->numQFailed;
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
			 * reconstruct read using "e". */
			if (ntfail == 2) {	/* also lost redundancy */
				if (asmap->failedPDAs[1]->type == RF_PDA_TYPE_PARITY)
					*createFunc = (RF_VoidFuncPtr) rf_EO_110_CreateReadDAG;
				else
					*createFunc = (RF_VoidFuncPtr) rf_EO_101_CreateReadDAG;
			} else {
				/* P and E are ok. But is there a failure in
				 * some unaccessed data unit? */
				if (rf_NumFailedDataUnitsInStripe(raidPtr, asmap) == 2)
					*createFunc = (RF_VoidFuncPtr) rf_EO_200_CreateReadDAG;
				else
					*createFunc = (RF_VoidFuncPtr) rf_EO_100_CreateReadDAG;
			}
			break;
		case 2:
			/* *createFunc = rf_EO_200_CreateReadDAG; */
			*createFunc = NULL;
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

			*createFunc = (RF_VoidFuncPtr) rf_EOCreateSmallWriteDAG;
		} else {
			*createFunc = (RF_VoidFuncPtr) rf_EOCreateLargeWriteDAG;
		}
		break;

	case 1:		/* single disk fault */
		if (npfail == 1) {
			RF_ASSERT((asmap->failedPDAs[0]->type == RF_PDA_TYPE_PARITY) || (asmap->failedPDAs[0]->type == RF_PDA_TYPE_Q));
			if (asmap->failedPDAs[0]->type == RF_PDA_TYPE_Q) {	/* q died, treat like
										 * normal mode raid5
										 * write. */
				if (((asmap->numStripeUnitsAccessed <= (layoutPtr->numDataCol / 2)) || (asmap->numStripeUnitsAccessed == 1))
				    || (asmap->parityInfo->next != NULL) || rf_NumFailedDataUnitsInStripe(raidPtr, asmap))
					*createFunc = (RF_VoidFuncPtr) rf_EO_001_CreateSmallWriteDAG;
				else
					*createFunc = (RF_VoidFuncPtr) rf_EO_001_CreateLargeWriteDAG;
			} else {/* parity died, small write only updating Q */
				if (((asmap->numStripeUnitsAccessed <= (layoutPtr->numDataCol / 2)) || (asmap->numStripeUnitsAccessed == 1))
				    || (asmap->qInfo->next != NULL) || rf_NumFailedDataUnitsInStripe(raidPtr, asmap))
					*createFunc = (RF_VoidFuncPtr) rf_EO_010_CreateSmallWriteDAG;
				else
					*createFunc = (RF_VoidFuncPtr) rf_EO_010_CreateLargeWriteDAG;
			}
		} else {	/* data missing. Do a P reconstruct write if
				 * only a single data unit is lost in the
				 * stripe, otherwise a reconstruct write which
				 * employnig both P and E units. */
			if (rf_NumFailedDataUnitsInStripe(raidPtr, asmap) == 2) {
				if (asmap->numStripeUnitsAccessed == 1)
					*createFunc = (RF_VoidFuncPtr) rf_EO_200_CreateWriteDAG;
				else
					*createFunc = NULL;	/* No direct support for
								 * this case now, like
								 * that in Raid-5  */
			} else {
				if (asmap->numStripeUnitsAccessed != 1 && asmap->failedPDAs[0]->numSector != layoutPtr->sectorsPerStripeUnit)
					*createFunc = NULL;	/* No direct support for
								 * this case now, like
								 * that in Raid-5  */
				else
					*createFunc = (RF_VoidFuncPtr) rf_EO_100_CreateWriteDAG;
			}
		}
		break;

	case 2:		/* two disk faults */
		switch (npfail) {
		case 2:	/* both p and q dead */
			*createFunc = (RF_VoidFuncPtr) rf_EO_011_CreateWriteDAG;
			break;
		case 1:	/* either p or q and dead data */
			RF_ASSERT(asmap->failedPDAs[0]->type == RF_PDA_TYPE_DATA);
			RF_ASSERT((asmap->failedPDAs[1]->type == RF_PDA_TYPE_PARITY) || (asmap->failedPDAs[1]->type == RF_PDA_TYPE_Q));
			if (asmap->failedPDAs[1]->type == RF_PDA_TYPE_Q) {
				if (asmap->numStripeUnitsAccessed != 1 && asmap->failedPDAs[0]->numSector != layoutPtr->sectorsPerStripeUnit)
					*createFunc = NULL;	/* In both PQ and
								 * EvenOdd, no direct
								 * support for this case
								 * now, like that in
								 * Raid-5  */
				else
					*createFunc = (RF_VoidFuncPtr) rf_EO_101_CreateWriteDAG;
			} else {
				if (asmap->numStripeUnitsAccessed != 1 && asmap->failedPDAs[0]->numSector != layoutPtr->sectorsPerStripeUnit)
					*createFunc = NULL;	/* No direct support for
								 * this case, like that
								 * in Raid-5  */
				else
					*createFunc = (RF_VoidFuncPtr) rf_EO_110_CreateWriteDAG;
			}
			break;
		case 0:	/* double data loss */
			/* if(asmap->failedPDAs[0]->numSector +
			 * asmap->failedPDAs[1]->numSector == 2 *
			 * layoutPtr->sectorsPerStripeUnit ) createFunc =
			 * rf_EOCreateLargeWriteDAG; else    							 */
			*createFunc = NULL;	/* currently, in Evenodd, No
						 * support for simultaneous
						 * access of both failed SUs */
			break;
		}
		break;

	default:		/* more than 2 disk faults */
		*createFunc = NULL;
		RF_PANIC();
	}
	return;
}


int 
rf_VerifyParityEvenOdd(raidPtr, raidAddr, parityPDA, correct_it, flags)
	RF_Raid_t *raidPtr;
	RF_RaidAddr_t raidAddr;
	RF_PhysDiskAddr_t *parityPDA;
	int     correct_it;
	RF_RaidAccessFlags_t flags;
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_RaidAddr_t startAddr = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, raidAddr);
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
	char   *redundantbuf2;
	int     redundantTwoErr = 0, redundantOneErr = 0;
	int     parity_cant_correct = RF_FALSE, red2_cant_correct = RF_FALSE,
	        parity_corrected = RF_FALSE, red2_corrected = RF_FALSE;
	int     i, retcode;
	RF_ReconUnitNum_t which_ru;
	RF_StripeNum_t psID = rf_RaidAddressToParityStripeID(layoutPtr, raidAddr, &which_ru);
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
	RF_CallocAndAdd(redundantbuf2, 1, numbytes, (char *), alloclist);	/* use calloc to make
										 * sure buffer is zeroed */

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

	RF_ASSERT(!asmap->qInfo->next);
	rf_RangeRestrictPDA(raidPtr, parityPDA, asmap->qInfo, 0, 1);
	RF_ASSERT(asmap->qInfo->numSector != 0);
	if (rf_TryToRedirectPDA(raidPtr, asmap->qInfo, 1))
		goto out;
	/* if disk is dead, b/c no reconstruction is implemented right now,
	 * the function "rf_TryToRedirectPDA" always return one, which cause
	 * go to out and return w/ good status   */
	blockNode->succedents[layoutPtr->numDataCol + 1]->params[0].p = asmap->qInfo;

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
	for (p = buf, i = 0; p < end_p; p += numbytes, i++) {
		rf_e_encToBuf(raidPtr, i, p, RF_EO_MATRIX_DIM - 2, redundantbuf2, numsector);
		/* the corresponding columes in EvenOdd encoding Matrix for
		 * these p pointers which point to the databuffer in a full
		 * stripe are sequentially from 0 to layoutPtr->numDataCol-1 */
		rf_bxor(p, pbuf, numbytes, NULL);
	}
	RF_ASSERT(i == layoutPtr->numDataCol);

	for (i = 0; i < numbytes; i++) {
		if (pbuf[i] != buf[bytesPerStripe + i]) {
			if (!correct_it) {
				RF_ERRORMSG3("Parity verify error: byte %d of parity is 0x%x should be 0x%x\n",
				    i, (u_char) buf[bytesPerStripe + i], (u_char) pbuf[i]);
			}
		}
		redundantOneErr = 1;
		break;
	}

	for (i = 0; i < numbytes; i++) {
		if (redundantbuf2[i] != buf[bytesPerStripe + numbytes + i]) {
			if (!correct_it) {
				RF_ERRORMSG3("Parity verify error: byte %d of second redundant information is 0x%x should be 0x%x\n",
				    i, (u_char) buf[bytesPerStripe + numbytes + i], (u_char) redundantbuf2[i]);
			}
			redundantTwoErr = 1;
			break;
		}
	}
	if (redundantOneErr || redundantTwoErr)
		retcode = RF_PARITY_BAD;

	/* correct the first redundant disk, ie parity if it is error    */
	if (redundantOneErr && correct_it) {
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
			parity_cant_correct = RF_TRUE;
		} else {
			parity_corrected = RF_TRUE;
		}
		rf_FreeDAG(wr_dag_h);
	}
	if (redundantTwoErr && correct_it) {
		wr_dag_h = rf_MakeSimpleDAG(raidPtr, 1, numbytes, redundantbuf2, rf_DiskWriteFunc, rf_DiskWriteUndoFunc,
		    "Wnred2", alloclist, flags, RF_IO_NORMAL_PRIORITY);
		wrBlock = wr_dag_h->succedents[0];
		wrUnblock = wrBlock->succedents[0]->succedents[0];
		wrBlock->succedents[0]->params[0].p = asmap->qInfo;
		wrBlock->succedents[0]->params[2].v = psID;
		wrBlock->succedents[0]->params[3].v = RF_CREATE_PARAM3(RF_IO_NORMAL_PRIORITY, 0, 0, which_ru);
		bzero((char *) &tracerec, sizeof(tracerec));
		wr_dag_h->tracerec = &tracerec;
		if (rf_verifyParityDebug) {
			printf("Dag of write new second redundant information in parity verify :\n");
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
			RF_ERRORMSG("Unable to correct second redundant information in VerifyParity:  can't write the stripe\n");
			red2_cant_correct = RF_TRUE;
		} else {
			red2_corrected = RF_TRUE;
		}
		rf_FreeDAG(wr_dag_h);
	}
	if ((redundantOneErr && parity_cant_correct) ||
	    (redundantTwoErr && red2_cant_correct))
		retcode = RF_PARITY_COULD_NOT_CORRECT;
	if ((retcode = RF_PARITY_BAD) && parity_corrected && red2_corrected)
		retcode = RF_PARITY_CORRECTED;


out:
	rf_FreeAccessStripeMap(asm_h);
	rf_FreeAllocList(alloclist);
	rf_FreeDAG(rd_dag_h);
	rf_FreeMCPair(mcpair);
	return (retcode);
}
#endif				/* RF_INCLUDE_EVENODD > 0 */
