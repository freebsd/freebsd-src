/*	$FreeBSD$ */
/*	$NetBSD: rf_raid5.c,v 1.4 2000/01/08 22:57:30 oster Exp $	*/
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

/******************************************************************************
 *
 * rf_raid5.c -- implements RAID Level 5
 *
 *****************************************************************************/

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_raid5.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_dagffrd.h>
#include <dev/raidframe/rf_dagffwr.h>
#include <dev/raidframe/rf_dagdegrd.h>
#include <dev/raidframe/rf_dagdegwr.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_map.h>
#include <dev/raidframe/rf_utils.h>

typedef struct RF_Raid5ConfigInfo_s {
	RF_RowCol_t **stripeIdentifier;	/* filled in at config time and used
					 * by IdentifyStripe */
}       RF_Raid5ConfigInfo_t;

int 
rf_ConfigureRAID5(
    RF_ShutdownList_t ** listp,
    RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr)
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_Raid5ConfigInfo_t *info;
	RF_RowCol_t i, j, startdisk;

	/* create a RAID level 5 configuration structure */
	RF_MallocAndAdd(info, sizeof(RF_Raid5ConfigInfo_t), (RF_Raid5ConfigInfo_t *), raidPtr->cleanupList);
	if (info == NULL)
		return (ENOMEM);
	layoutPtr->layoutSpecificInfo = (void *) info;

	RF_ASSERT(raidPtr->numRow == 1);

	/* the stripe identifier must identify the disks in each stripe, IN
	 * THE ORDER THAT THEY APPEAR IN THE STRIPE. */
	info->stripeIdentifier = rf_make_2d_array(raidPtr->numCol, raidPtr->numCol, raidPtr->cleanupList);
	if (info->stripeIdentifier == NULL)
		return (ENOMEM);
	startdisk = 0;
	for (i = 0; i < raidPtr->numCol; i++) {
		for (j = 0; j < raidPtr->numCol; j++) {
			info->stripeIdentifier[i][j] = (startdisk + j) % raidPtr->numCol;
		}
		if ((--startdisk) < 0)
			startdisk = raidPtr->numCol - 1;
	}

	/* fill in the remaining layout parameters */
	layoutPtr->numStripe = layoutPtr->stripeUnitsPerDisk;
	layoutPtr->bytesPerStripeUnit = layoutPtr->sectorsPerStripeUnit << raidPtr->logBytesPerSector;
	layoutPtr->numDataCol = raidPtr->numCol - 1;
	layoutPtr->dataSectorsPerStripe = layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;
	layoutPtr->numParityCol = 1;
	layoutPtr->dataStripeUnitsPerDisk = layoutPtr->stripeUnitsPerDisk;

	raidPtr->totalSectors = layoutPtr->stripeUnitsPerDisk * layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;

	return (0);
}

int 
rf_GetDefaultNumFloatingReconBuffersRAID5(RF_Raid_t * raidPtr)
{
	return (20);
}

RF_HeadSepLimit_t 
rf_GetDefaultHeadSepLimitRAID5(RF_Raid_t * raidPtr)
{
	return (10);
}
#if !defined(__NetBSD__) && !defined(__FreeBSD__) && !defined(_KERNEL)
/* not currently used */
int 
rf_ShutdownRAID5(RF_Raid_t * raidPtr)
{
	return (0);
}
#endif

void 
rf_MapSectorRAID5(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t raidSector,
    RF_RowCol_t * row,
    RF_RowCol_t * col,
    RF_SectorNum_t * diskSector,
    int remap)
{
	RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
	*row = 0;
	*col = (SUID % raidPtr->numCol);
	*diskSector = (SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
	    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void 
rf_MapParityRAID5(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t raidSector,
    RF_RowCol_t * row,
    RF_RowCol_t * col,
    RF_SectorNum_t * diskSector,
    int remap)
{
	RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;

	*row = 0;
	*col = raidPtr->Layout.numDataCol - (SUID / raidPtr->Layout.numDataCol) % raidPtr->numCol;
	*diskSector = (SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
	    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void 
rf_IdentifyStripeRAID5(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t addr,
    RF_RowCol_t ** diskids,
    RF_RowCol_t * outRow)
{
	RF_StripeNum_t stripeID = rf_RaidAddressToStripeID(&raidPtr->Layout, addr);
	RF_Raid5ConfigInfo_t *info = (RF_Raid5ConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;

	*outRow = 0;
	*diskids = info->stripeIdentifier[stripeID % raidPtr->numCol];
}

void 
rf_MapSIDToPSIDRAID5(
    RF_RaidLayout_t * layoutPtr,
    RF_StripeNum_t stripeID,
    RF_StripeNum_t * psID,
    RF_ReconUnitNum_t * which_ru)
{
	*which_ru = 0;
	*psID = stripeID;
}
/* select an algorithm for performing an access.  Returns two pointers,
 * one to a function that will return information about the DAG, and
 * another to a function that will create the dag.
 */
void 
rf_RaidFiveDagSelect(
    RF_Raid_t * raidPtr,
    RF_IoType_t type,
    RF_AccessStripeMap_t * asmap,
    RF_VoidFuncPtr * createFunc)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_PhysDiskAddr_t *failedPDA = NULL;
	RF_RowCol_t frow, fcol;
	RF_RowStatus_t rstat;
	int     prior_recon;

	RF_ASSERT(RF_IO_IS_R_OR_W(type));

	if (asmap->numDataFailed + asmap->numParityFailed > 1) {
		RF_ERRORMSG("Multiple disks failed in a single group!  Aborting I/O operation.\n");
		 /* *infoFunc = */ *createFunc = NULL;
		return;
	} else
		if (asmap->numDataFailed + asmap->numParityFailed == 1) {

			/* if under recon & already reconstructed, redirect
			 * the access to the spare drive and eliminate the
			 * failure indication */
			failedPDA = asmap->failedPDAs[0];
			frow = failedPDA->row;
			fcol = failedPDA->col;
			rstat = raidPtr->status[failedPDA->row];
			prior_recon = (rstat == rf_rs_reconfigured) || (
			    (rstat == rf_rs_reconstructing) ?
			    rf_CheckRUReconstructed(raidPtr->reconControl[frow]->reconMap, failedPDA->startSector) : 0
			    );
			if (prior_recon) {
				RF_RowCol_t or = failedPDA->row, oc = failedPDA->col;
				RF_SectorNum_t oo = failedPDA->startSector;

				if (layoutPtr->map->flags & RF_DISTRIBUTE_SPARE) {	/* redirect to dist
											 * spare space */

					if (failedPDA == asmap->parityInfo) {

						/* parity has failed */
						(layoutPtr->map->MapParity) (raidPtr, failedPDA->raidAddress, &failedPDA->row,
						    &failedPDA->col, &failedPDA->startSector, RF_REMAP);

						if (asmap->parityInfo->next) {	/* redir 2nd component,
										 * if any */
							RF_PhysDiskAddr_t *p = asmap->parityInfo->next;
							RF_SectorNum_t SUoffs = p->startSector % layoutPtr->sectorsPerStripeUnit;
							p->row = failedPDA->row;
							p->col = failedPDA->col;
							p->startSector = rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, failedPDA->startSector) +
							    SUoffs;	/* cheating:
									 * startSector is not
									 * really a RAID address */
						}
					} else
						if (asmap->parityInfo->next && failedPDA == asmap->parityInfo->next) {
							RF_ASSERT(0);	/* should not ever
									 * happen */
						} else {

							/* data has failed */
							(layoutPtr->map->MapSector) (raidPtr, failedPDA->raidAddress, &failedPDA->row,
							    &failedPDA->col, &failedPDA->startSector, RF_REMAP);

						}

				} else {	/* redirect to dedicated spare
						 * space */

					failedPDA->row = raidPtr->Disks[frow][fcol].spareRow;
					failedPDA->col = raidPtr->Disks[frow][fcol].spareCol;

					/* the parity may have two distinct
					 * components, both of which may need
					 * to be redirected */
					if (asmap->parityInfo->next) {
						if (failedPDA == asmap->parityInfo) {
							failedPDA->next->row = failedPDA->row;
							failedPDA->next->col = failedPDA->col;
						} else
							if (failedPDA == asmap->parityInfo->next) {	/* paranoid:  should
													 * never occur */
								asmap->parityInfo->row = failedPDA->row;
								asmap->parityInfo->col = failedPDA->col;
							}
					}
				}

				RF_ASSERT(failedPDA->col != -1);

				if (rf_dagDebug || rf_mapDebug) {
					printf("raid%d: Redirected type '%c' r %d c %d o %ld -> r %d c %d o %ld\n",
					       raidPtr->raidid, type, or, oc, 
					       (long) oo, failedPDA->row, 
					       failedPDA->col,
					       (long) failedPDA->startSector);
				}
				asmap->numDataFailed = asmap->numParityFailed = 0;
			}
		}
	/* all dags begin/end with block/unblock node therefore, hdrSucc &
	 * termAnt counts should always be 1 also, these counts should not be
	 * visible outside dag creation routines - manipulating the counts
	 * here should be removed */
	if (type == RF_IO_TYPE_READ) {
		if (asmap->numDataFailed == 0)
			*createFunc = (RF_VoidFuncPtr) rf_CreateFaultFreeReadDAG;
		else
			*createFunc = (RF_VoidFuncPtr) rf_CreateRaidFiveDegradedReadDAG;
	} else {


		/* if mirroring, always use large writes.  If the access
		 * requires two distinct parity updates, always do a small
		 * write.  If the stripe contains a failure but the access
		 * does not, do a small write. The first conditional
		 * (numStripeUnitsAccessed <= numDataCol/2) uses a
		 * less-than-or-equal rather than just a less-than because
		 * when G is 3 or 4, numDataCol/2 is 1, and I want
		 * single-stripe-unit updates to use just one disk. */
		if ((asmap->numDataFailed + asmap->numParityFailed) == 0) {
			if (rf_suppressLocksAndLargeWrites ||
			    (((asmap->numStripeUnitsAccessed <= (layoutPtr->numDataCol / 2)) && (layoutPtr->numDataCol != 1)) ||
				(asmap->parityInfo->next != NULL) || rf_CheckStripeForFailures(raidPtr, asmap))) {
				*createFunc = (RF_VoidFuncPtr) rf_CreateSmallWriteDAG;
			} else
				*createFunc = (RF_VoidFuncPtr) rf_CreateLargeWriteDAG;
		} else {
			if (asmap->numParityFailed == 1)
				*createFunc = (RF_VoidFuncPtr) rf_CreateNonRedundantWriteDAG;
			else
				if (asmap->numStripeUnitsAccessed != 1 && failedPDA->numSector != layoutPtr->sectorsPerStripeUnit)
					*createFunc = NULL;
				else
					*createFunc = (RF_VoidFuncPtr) rf_CreateDegradedWriteDAG;
		}
	}
}
