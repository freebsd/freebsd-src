/*	$NetBSD: rf_chaindecluster.c,v 1.6 2001/01/26 04:27:16 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
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

/******************************************************************************
 *
 * rf_chaindecluster.c -- implements chained declustering
 *
 *****************************************************************************/

#include <dev/raidframe/rf_archs.h>

#if (RF_INCLUDE_CHAINDECLUSTER > 0) 

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_chaindecluster.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_dagffrd.h>
#include <dev/raidframe/rf_dagffwr.h>
#include <dev/raidframe/rf_dagdegrd.h>
#include <dev/raidframe/rf_dagfuncs.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_utils.h>

typedef struct RF_ChaindeclusterConfigInfo_s {
	RF_RowCol_t **stripeIdentifier;	/* filled in at config time and used
					 * by IdentifyStripe */
	RF_StripeCount_t numSparingRegions;
	RF_StripeCount_t stripeUnitsPerSparingRegion;
	RF_SectorNum_t mirrorStripeOffset;
}       RF_ChaindeclusterConfigInfo_t;

int 
rf_ConfigureChainDecluster(
    RF_ShutdownList_t ** listp,
    RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr)
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_StripeCount_t num_used_stripeUnitsPerDisk;
	RF_ChaindeclusterConfigInfo_t *info;
	RF_RowCol_t i;

	/* create a Chained Declustering configuration structure */
	RF_MallocAndAdd(info, sizeof(RF_ChaindeclusterConfigInfo_t), (RF_ChaindeclusterConfigInfo_t *), raidPtr->cleanupList);
	if (info == NULL)
		return (ENOMEM);
	layoutPtr->layoutSpecificInfo = (void *) info;

	/* fill in the config structure.  */
	info->stripeIdentifier = rf_make_2d_array(raidPtr->numCol, 2, raidPtr->cleanupList);
	if (info->stripeIdentifier == NULL)
		return (ENOMEM);
	for (i = 0; i < raidPtr->numCol; i++) {
		info->stripeIdentifier[i][0] = i % raidPtr->numCol;
		info->stripeIdentifier[i][1] = (i + 1) % raidPtr->numCol;
	}

	RF_ASSERT(raidPtr->numRow == 1);

	/* fill in the remaining layout parameters */
	num_used_stripeUnitsPerDisk = layoutPtr->stripeUnitsPerDisk - (layoutPtr->stripeUnitsPerDisk %
	    (2 * raidPtr->numCol - 2));
	info->numSparingRegions = num_used_stripeUnitsPerDisk / (2 * raidPtr->numCol - 2);
	info->stripeUnitsPerSparingRegion = raidPtr->numCol * (raidPtr->numCol - 1);
	info->mirrorStripeOffset = info->numSparingRegions * (raidPtr->numCol - 1);
	layoutPtr->numStripe = info->numSparingRegions * info->stripeUnitsPerSparingRegion;
	layoutPtr->bytesPerStripeUnit = layoutPtr->sectorsPerStripeUnit << raidPtr->logBytesPerSector;
	layoutPtr->numDataCol = 1;
	layoutPtr->dataSectorsPerStripe = layoutPtr->numDataCol * layoutPtr->sectorsPerStripeUnit;
	layoutPtr->numParityCol = 1;

	layoutPtr->dataStripeUnitsPerDisk = num_used_stripeUnitsPerDisk;

	raidPtr->sectorsPerDisk =
	    num_used_stripeUnitsPerDisk * layoutPtr->sectorsPerStripeUnit;

	raidPtr->totalSectors =
	    (layoutPtr->numStripe) * layoutPtr->sectorsPerStripeUnit;

	layoutPtr->stripeUnitsPerDisk = raidPtr->sectorsPerDisk / layoutPtr->sectorsPerStripeUnit;

	return (0);
}

RF_ReconUnitCount_t 
rf_GetNumSpareRUsChainDecluster(raidPtr)
	RF_Raid_t *raidPtr;
{
	RF_ChaindeclusterConfigInfo_t *info = (RF_ChaindeclusterConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;

	/*
         * The layout uses two stripe units per disk as spare within each
         * sparing region.
         */
	return (2 * info->numSparingRegions);
}


/* Maps to the primary copy of the data, i.e. the first mirror pair */
void 
rf_MapSectorChainDecluster(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t raidSector,
    RF_RowCol_t * row,
    RF_RowCol_t * col,
    RF_SectorNum_t * diskSector,
    int remap)
{
	RF_ChaindeclusterConfigInfo_t *info = (RF_ChaindeclusterConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;
	RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
	RF_SectorNum_t index_within_region, index_within_disk;
	RF_StripeNum_t sparing_region_id;
	int     col_before_remap;

	*row = 0;
	sparing_region_id = SUID / info->stripeUnitsPerSparingRegion;
	index_within_region = SUID % info->stripeUnitsPerSparingRegion;
	index_within_disk = index_within_region / raidPtr->numCol;
	col_before_remap = SUID % raidPtr->numCol;

	if (!remap) {
		*col = col_before_remap;
		*diskSector = (index_within_disk + ((raidPtr->numCol - 1) * sparing_region_id)) *
		    raidPtr->Layout.sectorsPerStripeUnit;
		*diskSector += (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
	} else {
		/* remap sector to spare space... */
		*diskSector = sparing_region_id * (raidPtr->numCol + 1) * raidPtr->Layout.sectorsPerStripeUnit;
		*diskSector += (raidPtr->numCol - 1) * raidPtr->Layout.sectorsPerStripeUnit;
		*diskSector += (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
		index_within_disk = index_within_region / raidPtr->numCol;
		if (index_within_disk < col_before_remap)
			*col = index_within_disk;
		else
			if (index_within_disk == raidPtr->numCol - 2) {
				*col = (col_before_remap + raidPtr->numCol - 1) % raidPtr->numCol;
				*diskSector += raidPtr->Layout.sectorsPerStripeUnit;
			} else
				*col = (index_within_disk + 2) % raidPtr->numCol;
	}

}



/* Maps to the second copy of the mirror pair, which is chain declustered. The second copy is contained
   in the next disk (mod numCol) after the disk containing the primary copy.
   The offset into the disk is one-half disk down */
void 
rf_MapParityChainDecluster(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t raidSector,
    RF_RowCol_t * row,
    RF_RowCol_t * col,
    RF_SectorNum_t * diskSector,
    int remap)
{
	RF_ChaindeclusterConfigInfo_t *info = (RF_ChaindeclusterConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;
	RF_StripeNum_t SUID = raidSector / raidPtr->Layout.sectorsPerStripeUnit;
	RF_SectorNum_t index_within_region, index_within_disk;
	RF_StripeNum_t sparing_region_id;
	int     col_before_remap;

	*row = 0;
	if (!remap) {
		*col = SUID % raidPtr->numCol;
		*col = (*col + 1) % raidPtr->numCol;
		*diskSector = info->mirrorStripeOffset * raidPtr->Layout.sectorsPerStripeUnit;
		*diskSector += (SUID / raidPtr->numCol) * raidPtr->Layout.sectorsPerStripeUnit;
		*diskSector += (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
	} else {
		/* remap parity to spare space ... */
		sparing_region_id = SUID / info->stripeUnitsPerSparingRegion;
		index_within_region = SUID % info->stripeUnitsPerSparingRegion;
		index_within_disk = index_within_region / raidPtr->numCol;
		*diskSector = sparing_region_id * (raidPtr->numCol + 1) * raidPtr->Layout.sectorsPerStripeUnit;
		*diskSector += (raidPtr->numCol) * raidPtr->Layout.sectorsPerStripeUnit;
		*diskSector += (raidSector % raidPtr->Layout.sectorsPerStripeUnit);
		col_before_remap = SUID % raidPtr->numCol;
		if (index_within_disk < col_before_remap)
			*col = index_within_disk;
		else
			if (index_within_disk == raidPtr->numCol - 2) {
				*col = (col_before_remap + 2) % raidPtr->numCol;
				*diskSector -= raidPtr->Layout.sectorsPerStripeUnit;
			} else
				*col = (index_within_disk + 2) % raidPtr->numCol;
	}

}

void 
rf_IdentifyStripeChainDecluster(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t addr,
    RF_RowCol_t ** diskids,
    RF_RowCol_t * outRow)
{
	RF_ChaindeclusterConfigInfo_t *info = (RF_ChaindeclusterConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;
	RF_StripeNum_t SUID;
	RF_RowCol_t col;

	SUID = addr / raidPtr->Layout.sectorsPerStripeUnit;
	col = SUID % raidPtr->numCol;
	*outRow = 0;
	*diskids = info->stripeIdentifier[col];
}

void 
rf_MapSIDToPSIDChainDecluster(
    RF_RaidLayout_t * layoutPtr,
    RF_StripeNum_t stripeID,
    RF_StripeNum_t * psID,
    RF_ReconUnitNum_t * which_ru)
{
	*which_ru = 0;
	*psID = stripeID;
}
/******************************************************************************
 * select a graph to perform a single-stripe access
 *
 * Parameters:  raidPtr    - description of the physical array
 *              type       - type of operation (read or write) requested
 *              asmap      - logical & physical addresses for this access
 *              createFunc - function to use to create the graph (return value)
 *****************************************************************************/

void 
rf_RAIDCDagSelect(
    RF_Raid_t * raidPtr,
    RF_IoType_t type,
    RF_AccessStripeMap_t * asmap,
    RF_VoidFuncPtr * createFunc)
#if 0
	void    (**createFunc) (RF_Raid_t *, RF_AccessStripeMap_t *,
            RF_DagHeader_t *, void *, RF_RaidAccessFlags_t,
            RF_AllocListElem_t *)
#endif
{
	RF_ASSERT(RF_IO_IS_R_OR_W(type));
	RF_ASSERT(raidPtr->numRow == 1);

	if (asmap->numDataFailed + asmap->numParityFailed > 1) {
		RF_ERRORMSG("Multiple disks failed in a single group!  Aborting I/O operation.\n");
		*createFunc = NULL;
		return;
	}
	*createFunc = (type == RF_IO_TYPE_READ) ? (RF_VoidFuncPtr) rf_CreateFaultFreeReadDAG : (RF_VoidFuncPtr) rf_CreateRaidOneWriteDAG;

	if (type == RF_IO_TYPE_READ) {
		if ((raidPtr->status[0] == rf_rs_degraded) || (raidPtr->status[0] == rf_rs_reconstructing))
			*createFunc = (RF_VoidFuncPtr) rf_CreateRaidCDegradedReadDAG;	/* array status is
											 * degraded, implement
											 * workload shifting */
		else
			*createFunc = (RF_VoidFuncPtr) rf_CreateMirrorPartitionReadDAG;	/* array status not
											 * degraded, so use
											 * mirror partition dag */
	} else
		*createFunc = (RF_VoidFuncPtr) rf_CreateRaidOneWriteDAG;
}
#endif /* (RF_INCLUDE_CHAINDECLUSTER > 0) */
