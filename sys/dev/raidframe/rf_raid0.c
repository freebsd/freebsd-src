/*	$NetBSD: rf_raid0.c,v 1.4 2000/01/07 03:41:02 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
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

/***************************************
 *
 * rf_raid0.c -- implements RAID Level 0
 *
 ***************************************/

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_raid0.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_dagffrd.h>
#include <dev/raidframe/rf_dagffwr.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_dagfuncs.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_configure.h>
#include <dev/raidframe/rf_parityscan.h>

typedef struct RF_Raid0ConfigInfo_s {
	RF_RowCol_t *stripeIdentifier;
}       RF_Raid0ConfigInfo_t;

int 
rf_ConfigureRAID0(
    RF_ShutdownList_t ** listp,
    RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr)
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_Raid0ConfigInfo_t *info;
	RF_RowCol_t i;

	/* create a RAID level 0 configuration structure */
	RF_MallocAndAdd(info, sizeof(RF_Raid0ConfigInfo_t), (RF_Raid0ConfigInfo_t *), raidPtr->cleanupList);
	if (info == NULL)
		return (ENOMEM);
	layoutPtr->layoutSpecificInfo = (void *) info;

	RF_MallocAndAdd(info->stripeIdentifier, raidPtr->numCol * sizeof(RF_RowCol_t), (RF_RowCol_t *), raidPtr->cleanupList);
	if (info->stripeIdentifier == NULL)
		return (ENOMEM);
	for (i = 0; i < raidPtr->numCol; i++)
		info->stripeIdentifier[i] = i;

	RF_ASSERT(raidPtr->numRow == 1);
	raidPtr->totalSectors = layoutPtr->stripeUnitsPerDisk * raidPtr->numCol * layoutPtr->sectorsPerStripeUnit;
	layoutPtr->numStripe = layoutPtr->stripeUnitsPerDisk;
	layoutPtr->dataSectorsPerStripe = raidPtr->numCol * layoutPtr->sectorsPerStripeUnit;
	layoutPtr->bytesPerStripeUnit = layoutPtr->sectorsPerStripeUnit << raidPtr->logBytesPerSector;
	layoutPtr->numDataCol = raidPtr->numCol;
	layoutPtr->numParityCol = 0;
	return (0);
}

void 
rf_MapSectorRAID0(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t raidSector,
    RF_RowCol_t * row,
    RF_RowCol_t * col,
    RF_SectorNum_t * diskSector,
    int remap)
{
	RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
	*row = 0;
	*col = SUID % raidPtr->numCol;
	*diskSector = (SUID / raidPtr->numCol) * raidPtr->Layout.sectorsPerStripeUnit +
	    (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
}

void 
rf_MapParityRAID0(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t raidSector,
    RF_RowCol_t * row,
    RF_RowCol_t * col,
    RF_SectorNum_t * diskSector,
    int remap)
{
	*row = *col = 0;
	*diskSector = 0;
}

void 
rf_IdentifyStripeRAID0(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t addr,
    RF_RowCol_t ** diskids,
    RF_RowCol_t * outRow)
{
	RF_Raid0ConfigInfo_t *info;

	info = raidPtr->Layout.layoutSpecificInfo;
	*diskids = info->stripeIdentifier;
	*outRow = 0;
}

void 
rf_MapSIDToPSIDRAID0(
    RF_RaidLayout_t * layoutPtr,
    RF_StripeNum_t stripeID,
    RF_StripeNum_t * psID,
    RF_ReconUnitNum_t * which_ru)
{
	*which_ru = 0;
	*psID = stripeID;
}

void 
rf_RAID0DagSelect(
    RF_Raid_t * raidPtr,
    RF_IoType_t type,
    RF_AccessStripeMap_t * asmap,
    RF_VoidFuncPtr * createFunc)
{
	*createFunc = ((type == RF_IO_TYPE_READ) ?
	    (RF_VoidFuncPtr) rf_CreateFaultFreeReadDAG : (RF_VoidFuncPtr) rf_CreateRAID0WriteDAG);
}

int 
rf_VerifyParityRAID0(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t raidAddr,
    RF_PhysDiskAddr_t * parityPDA,
    int correct_it,
    RF_RaidAccessFlags_t flags)
{
	/*
         * No parity is always okay.
         */
	return (RF_PARITY_OKAY);
}
