/*	$FreeBSD$ */
/*	$NetBSD: rf_raid5_rotatedspare.c,v 1.5 2001/01/26 05:16:58 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Khalil Amiri
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

/**************************************************************************
 *
 * rf_raid5_rotated_spare.c -- implements RAID Level 5 with rotated sparing
 *
 **************************************************************************/

#include <dev/raidframe/rf_archs.h>

#if RF_INCLUDE_RAID5_RS > 0

#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_raid5.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_dagfuncs.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_utils.h>
#include <dev/raidframe/rf_raid5_rotatedspare.h>

typedef struct RF_Raid5RSConfigInfo_s {
	RF_RowCol_t **stripeIdentifier;	/* filled in at config time & used by
					 * IdentifyStripe */
}       RF_Raid5RSConfigInfo_t;

int 
rf_ConfigureRAID5_RS(
    RF_ShutdownList_t ** listp,
    RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr)
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_Raid5RSConfigInfo_t *info;
	RF_RowCol_t i, j, startdisk;

	/* create a RAID level 5 configuration structure */
	RF_MallocAndAdd(info, sizeof(RF_Raid5RSConfigInfo_t), (RF_Raid5RSConfigInfo_t *), raidPtr->cleanupList);
	if (info == NULL)
		return (ENOMEM);
	layoutPtr->layoutSpecificInfo = (void *) info;

	RF_ASSERT(raidPtr->numRow == 1);
	RF_ASSERT(raidPtr->numCol >= 3);

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
	layoutPtr->numDataCol = raidPtr->numCol - 2;
	layoutPtr->dataSectorsPerStripe = layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;
	layoutPtr->numParityCol = 1;
	layoutPtr->dataStripeUnitsPerDisk = layoutPtr->stripeUnitsPerDisk;
	raidPtr->sectorsPerDisk = layoutPtr->stripeUnitsPerDisk * layoutPtr->sectorsPerStripeUnit;

	raidPtr->totalSectors = layoutPtr->stripeUnitsPerDisk * layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;

	return (0);
}

RF_ReconUnitCount_t 
rf_GetNumSpareRUsRAID5_RS(raidPtr)
	RF_Raid_t *raidPtr;
{
	return (raidPtr->Layout.stripeUnitsPerDisk / raidPtr->numCol);
}

void 
rf_MapSectorRAID5_RS(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t raidSector,
    RF_RowCol_t * row,
    RF_RowCol_t * col,
    RF_SectorNum_t * diskSector,
    int remap)
{
	RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;

	*row = 0;
	if (remap) {
		*col = raidPtr->numCol - 1 - (1 + SUID / raidPtr->Layout.numDataCol) % raidPtr->numCol;
		*col = (*col + 1) % raidPtr->numCol;	/* spare unit is rotated
							 * with parity; line
							 * above maps to parity */
	} else {
		*col = (SUID + (SUID / raidPtr->Layout.numDataCol)) % raidPtr->numCol;
	}
	*diskSector = (SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
	    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void 
rf_MapParityRAID5_RS(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t raidSector,
    RF_RowCol_t * row,
    RF_RowCol_t * col,
    RF_SectorNum_t * diskSector,
    int remap)
{
	RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;

	*row = 0;
	*col = raidPtr->numCol - 1 - (1 + SUID / raidPtr->Layout.numDataCol) % raidPtr->numCol;
	*diskSector = (SUID / (raidPtr->Layout.numDataCol)) * raidPtr->Layout.sectorsPerStripeUnit +
	    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
	if (remap)
		*col = (*col + 1) % raidPtr->numCol;
}

void 
rf_IdentifyStripeRAID5_RS(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t addr,
    RF_RowCol_t ** diskids,
    RF_RowCol_t * outRow)
{
	RF_StripeNum_t stripeID = rf_RaidAddressToStripeID(&raidPtr->Layout, addr);
	RF_Raid5RSConfigInfo_t *info = (RF_Raid5RSConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;
	*outRow = 0;
	*diskids = info->stripeIdentifier[stripeID % raidPtr->numCol];

}

void 
rf_MapSIDToPSIDRAID5_RS(
    RF_RaidLayout_t * layoutPtr,
    RF_StripeNum_t stripeID,
    RF_StripeNum_t * psID,
    RF_ReconUnitNum_t * which_ru)
{
	*which_ru = 0;
	*psID = stripeID;
}
#endif /* RF_INCLUDE_RAID5_RS > 0 */
