/*	$FreeBSD$ */
/*	$NetBSD: rf_decluster.c,v 1.6 2001/01/26 04:40:03 oster Exp $	*/
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

/*----------------------------------------------------------------------
 *
 * rf_decluster.c -- code related to the declustered layout
 *
 * Created 10-21-92 (MCH)
 *
 * Nov 93:  adding support for distributed sparing.  This code is a little
 *          complex:  the basic layout used is as follows:
 *          let F = (v-1)/GCD(r,v-1).  The spare space for each set of
 *          F consecutive fulltables is grouped together and placed after
 *          that set of tables.
 *                   +------------------------------+
 *                   |        F fulltables          |
 *                   |        Spare Space           |
 *                   |        F fulltables          |
 *                   |        Spare Space           |
 *                   |            ...               |
 *                   +------------------------------+
 *
 *--------------------------------------------------------------------*/

#include <dev/raidframe/rf_archs.h>
#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_raidframe.h>
#include <dev/raidframe/rf_configure.h>
#include <dev/raidframe/rf_decluster.h>
#include <dev/raidframe/rf_debugMem.h>
#include <dev/raidframe/rf_utils.h>
#include <dev/raidframe/rf_alloclist.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_shutdown.h>


extern int rf_copyback_in_progress;	/* debug only */

/* found in rf_kintf.c */
int     rf_GetSpareTableFromDaemon(RF_SparetWait_t * req);

#if (RF_INCLUDE_PARITY_DECLUSTERING > 0) || (RF_INCLUDE_PARITY_DECLUSTERING_PQ > 0)

/* configuration code */

int 
rf_ConfigureDeclustered(
    RF_ShutdownList_t ** listp,
    RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	int     b, v, k, r, lambda;	/* block design params */
	int     i, j;
	RF_RowCol_t *first_avail_slot;
	RF_StripeCount_t complete_FT_count, numCompleteFullTablesPerDisk;
	RF_DeclusteredConfigInfo_t *info;
	RF_StripeCount_t PUsPerDisk, spareRegionDepthInPUs, numCompleteSpareRegionsPerDisk,
	        extraPUsPerDisk;
	RF_StripeCount_t totSparePUsPerDisk;
	RF_SectorNum_t diskOffsetOfLastFullTableInSUs;
	RF_SectorCount_t SpareSpaceInSUs;
	char   *cfgBuf = (char *) (cfgPtr->layoutSpecific);
	RF_StripeNum_t l, SUID;

	SUID = l = 0;
	numCompleteSpareRegionsPerDisk = 0;

	/* 1. create layout specific structure */
	RF_MallocAndAdd(info, sizeof(RF_DeclusteredConfigInfo_t), (RF_DeclusteredConfigInfo_t *), raidPtr->cleanupList);
	if (info == NULL)
		return (ENOMEM);
	layoutPtr->layoutSpecificInfo = (void *) info;
	info->SpareTable = NULL;

	/* 2. extract parameters from the config structure */
	if (layoutPtr->map->flags & RF_DISTRIBUTE_SPARE) {
		(void) bcopy(cfgBuf, info->sparemap_fname, RF_SPAREMAP_NAME_LEN);
	}
	cfgBuf += RF_SPAREMAP_NAME_LEN;

	b = *((int *) cfgBuf);
	cfgBuf += sizeof(int);
	v = *((int *) cfgBuf);
	cfgBuf += sizeof(int);
	k = *((int *) cfgBuf);
	cfgBuf += sizeof(int);
	r = *((int *) cfgBuf);
	cfgBuf += sizeof(int);
	lambda = *((int *) cfgBuf);
	cfgBuf += sizeof(int);
	raidPtr->noRotate = *((int *) cfgBuf);
	cfgBuf += sizeof(int);

	/* the sparemaps are generated assuming that parity is rotated, so we
	 * issue a warning if both distributed sparing and no-rotate are on at
	 * the same time */
	if ((layoutPtr->map->flags & RF_DISTRIBUTE_SPARE) && raidPtr->noRotate) {
		RF_ERRORMSG("Warning:  distributed sparing specified without parity rotation.\n");
	}
	if (raidPtr->numCol != v) {
		RF_ERRORMSG2("RAID: config error: table element count (%d) not equal to no. of cols (%d)\n", v, raidPtr->numCol);
		return (EINVAL);
	}
	/* 3.  set up the values used in the mapping code */
	info->BlocksPerTable = b;
	info->Lambda = lambda;
	info->NumParityReps = info->groupSize = k;
	info->SUsPerTable = b * (k - 1) * layoutPtr->SUsPerPU;	/* b blks, k-1 SUs each */
	info->SUsPerFullTable = k * info->SUsPerTable;	/* rot k times */
	info->PUsPerBlock = k - 1;
	info->SUsPerBlock = info->PUsPerBlock * layoutPtr->SUsPerPU;
	info->TableDepthInPUs = (b * k) / v;
	info->FullTableDepthInPUs = info->TableDepthInPUs * k;	/* k repetitions */

	/* used only in distributed sparing case */
	info->FullTablesPerSpareRegion = (v - 1) / rf_gcd(r, v - 1);	/* (v-1)/gcd fulltables */
	info->TablesPerSpareRegion = k * info->FullTablesPerSpareRegion;
	info->SpareSpaceDepthPerRegionInSUs = (r * info->TablesPerSpareRegion / (v - 1)) * layoutPtr->SUsPerPU;

	/* check to make sure the block design is sufficiently small */
	if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
		if (info->FullTableDepthInPUs * layoutPtr->SUsPerPU + info->SpareSpaceDepthPerRegionInSUs > layoutPtr->stripeUnitsPerDisk) {
			RF_ERRORMSG3("RAID: config error: Full Table depth (%d) + Spare Space (%d) larger than disk size (%d) (BD too big)\n",
			    (int) info->FullTableDepthInPUs,
			    (int) info->SpareSpaceDepthPerRegionInSUs,
			    (int) layoutPtr->stripeUnitsPerDisk);
			return (EINVAL);
		}
	} else {
		if (info->TableDepthInPUs * layoutPtr->SUsPerPU > layoutPtr->stripeUnitsPerDisk) {
			RF_ERRORMSG2("RAID: config error: Table depth (%d) larger than disk size (%d) (BD too big)\n",
			    (int) (info->TableDepthInPUs * layoutPtr->SUsPerPU), \
			    (int) layoutPtr->stripeUnitsPerDisk);
			return (EINVAL);
		}
	}


	/* compute the size of each disk, and the number of tables in the last
	 * fulltable (which need not be complete) */
	if (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) {

		PUsPerDisk = layoutPtr->stripeUnitsPerDisk / layoutPtr->SUsPerPU;
		spareRegionDepthInPUs = (info->TablesPerSpareRegion * info->TableDepthInPUs +
		    (info->TablesPerSpareRegion * info->TableDepthInPUs) / (v - 1));
		info->SpareRegionDepthInSUs = spareRegionDepthInPUs * layoutPtr->SUsPerPU;

		numCompleteSpareRegionsPerDisk = PUsPerDisk / spareRegionDepthInPUs;
		info->NumCompleteSRs = numCompleteSpareRegionsPerDisk;
		extraPUsPerDisk = PUsPerDisk % spareRegionDepthInPUs;

		/* assume conservatively that we need the full amount of spare
		 * space in one region in order to provide spares for the
		 * partial spare region at the end of the array.  We set "i"
		 * to the number of tables in the partial spare region.  This
		 * may actually include some fulltables. */
		extraPUsPerDisk -= (info->SpareSpaceDepthPerRegionInSUs / layoutPtr->SUsPerPU);
		if (extraPUsPerDisk <= 0)
			i = 0;
		else
			i = extraPUsPerDisk / info->TableDepthInPUs;

		complete_FT_count = raidPtr->numRow * (numCompleteSpareRegionsPerDisk * (info->TablesPerSpareRegion / k) + i / k);
		info->FullTableLimitSUID = complete_FT_count * info->SUsPerFullTable;
		info->ExtraTablesPerDisk = i % k;

		/* note that in the last spare region, the spare space is
		 * complete even though data/parity space is not */
		totSparePUsPerDisk = (numCompleteSpareRegionsPerDisk + 1) * (info->SpareSpaceDepthPerRegionInSUs / layoutPtr->SUsPerPU);
		info->TotSparePUsPerDisk = totSparePUsPerDisk;

		layoutPtr->stripeUnitsPerDisk =
		    ((complete_FT_count / raidPtr->numRow) * info->FullTableDepthInPUs +	/* data & parity space */
		    info->ExtraTablesPerDisk * info->TableDepthInPUs +
		    totSparePUsPerDisk	/* spare space */
		    ) * layoutPtr->SUsPerPU;
		layoutPtr->dataStripeUnitsPerDisk =
		    (complete_FT_count * info->FullTableDepthInPUs + info->ExtraTablesPerDisk * info->TableDepthInPUs)
		    * layoutPtr->SUsPerPU * (k - 1) / k;

	} else {
		/* non-dist spare case:  force each disk to contain an
		 * integral number of tables */
		layoutPtr->stripeUnitsPerDisk /= (info->TableDepthInPUs * layoutPtr->SUsPerPU);
		layoutPtr->stripeUnitsPerDisk *= (info->TableDepthInPUs * layoutPtr->SUsPerPU);

		/* compute the number of tables in the last fulltable, which
		 * need not be complete */
		complete_FT_count =
		    ((layoutPtr->stripeUnitsPerDisk / layoutPtr->SUsPerPU) / info->FullTableDepthInPUs) * raidPtr->numRow;

		info->FullTableLimitSUID = complete_FT_count * info->SUsPerFullTable;
		info->ExtraTablesPerDisk =
		    ((layoutPtr->stripeUnitsPerDisk / layoutPtr->SUsPerPU) / info->TableDepthInPUs) % k;
	}

	raidPtr->sectorsPerDisk = layoutPtr->stripeUnitsPerDisk * layoutPtr->sectorsPerStripeUnit;

	/* find the disk offset of the stripe unit where the last fulltable
	 * starts */
	numCompleteFullTablesPerDisk = complete_FT_count / raidPtr->numRow;
	diskOffsetOfLastFullTableInSUs = numCompleteFullTablesPerDisk * info->FullTableDepthInPUs * layoutPtr->SUsPerPU;
	if (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) {
		SpareSpaceInSUs = numCompleteSpareRegionsPerDisk * info->SpareSpaceDepthPerRegionInSUs;
		diskOffsetOfLastFullTableInSUs += SpareSpaceInSUs;
		info->DiskOffsetOfLastSpareSpaceChunkInSUs =
		    diskOffsetOfLastFullTableInSUs + info->ExtraTablesPerDisk * info->TableDepthInPUs * layoutPtr->SUsPerPU;
	}
	info->DiskOffsetOfLastFullTableInSUs = diskOffsetOfLastFullTableInSUs;
	info->numCompleteFullTablesPerDisk = numCompleteFullTablesPerDisk;

	/* 4.  create and initialize the lookup tables */
	info->LayoutTable = rf_make_2d_array(b, k, raidPtr->cleanupList);
	if (info->LayoutTable == NULL)
		return (ENOMEM);
	info->OffsetTable = rf_make_2d_array(b, k, raidPtr->cleanupList);
	if (info->OffsetTable == NULL)
		return (ENOMEM);
	info->BlockTable = rf_make_2d_array(info->TableDepthInPUs * layoutPtr->SUsPerPU, raidPtr->numCol, raidPtr->cleanupList);
	if (info->BlockTable == NULL)
		return (ENOMEM);

	first_avail_slot = rf_make_1d_array(v, NULL);
	if (first_avail_slot == NULL)
		return (ENOMEM);

	for (i = 0; i < b; i++)
		for (j = 0; j < k; j++)
			info->LayoutTable[i][j] = *cfgBuf++;

	/* initialize offset table */
	for (i = 0; i < b; i++)
		for (j = 0; j < k; j++) {
			info->OffsetTable[i][j] = first_avail_slot[info->LayoutTable[i][j]];
			first_avail_slot[info->LayoutTable[i][j]]++;
		}

	/* initialize block table */
	for (SUID = l = 0; l < layoutPtr->SUsPerPU; l++) {
		for (i = 0; i < b; i++) {
			for (j = 0; j < k; j++) {
				info->BlockTable[(info->OffsetTable[i][j] * layoutPtr->SUsPerPU) + l]
				    [info->LayoutTable[i][j]] = SUID;
			}
			SUID++;
		}
	}

	rf_free_1d_array(first_avail_slot, v);

	/* 5.  set up the remaining redundant-but-useful parameters */

	raidPtr->totalSectors = (k * complete_FT_count + raidPtr->numRow * info->ExtraTablesPerDisk) *
	    info->SUsPerTable * layoutPtr->sectorsPerStripeUnit;
	layoutPtr->numStripe = (raidPtr->totalSectors / layoutPtr->sectorsPerStripeUnit) / (k - 1);

	/* strange evaluation order below to try and minimize overflow
	 * problems */

	layoutPtr->dataSectorsPerStripe = (k - 1) * layoutPtr->sectorsPerStripeUnit;
	layoutPtr->bytesPerStripeUnit = layoutPtr->sectorsPerStripeUnit << raidPtr->logBytesPerSector;
	layoutPtr->numDataCol = k - 1;
	layoutPtr->numParityCol = 1;

	return (0);
}
/* declustering with distributed sparing */
static void rf_ShutdownDeclusteredDS(RF_ThreadArg_t);
static void 
rf_ShutdownDeclusteredDS(arg)
	RF_ThreadArg_t arg;
{
	RF_DeclusteredConfigInfo_t *info;
	RF_Raid_t *raidPtr;

	raidPtr = (RF_Raid_t *) arg;
	info = (RF_DeclusteredConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;
	if (info->SpareTable)
		rf_FreeSpareTable(raidPtr);
}

int 
rf_ConfigureDeclusteredDS(
    RF_ShutdownList_t ** listp,
    RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr)
{
	int     rc;

	rc = rf_ConfigureDeclustered(listp, raidPtr, cfgPtr);
	if (rc)
		return (rc);
	rc = rf_ShutdownCreate(listp, rf_ShutdownDeclusteredDS, raidPtr);
	if (rc) {
		RF_ERRORMSG1("Got %d adding shutdown event for DeclusteredDS\n", rc);
		rf_ShutdownDeclusteredDS(raidPtr);
		return (rc);
	}
	return (0);
}

void 
rf_MapSectorDeclustered(raidPtr, raidSector, row, col, diskSector, remap)
	RF_Raid_t *raidPtr;
	RF_RaidAddr_t raidSector;
	RF_RowCol_t *row;
	RF_RowCol_t *col;
	RF_SectorNum_t *diskSector;
	int     remap;
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_DeclusteredConfigInfo_t *info = (RF_DeclusteredConfigInfo_t *) layoutPtr->layoutSpecificInfo;
	RF_StripeNum_t SUID = raidSector / layoutPtr->sectorsPerStripeUnit;
	RF_StripeNum_t FullTableID, FullTableOffset, TableID, TableOffset;
	RF_StripeNum_t BlockID, BlockOffset, RepIndex;
	RF_StripeCount_t sus_per_fulltable = info->SUsPerFullTable;
	RF_StripeCount_t fulltable_depth = info->FullTableDepthInPUs * layoutPtr->SUsPerPU;
	RF_StripeNum_t base_suid = 0, outSU, SpareRegion = 0, SpareSpace = 0;

	rf_decluster_adjust_params(layoutPtr, &SUID, &sus_per_fulltable, &fulltable_depth, &base_suid);

	FullTableID = SUID / sus_per_fulltable;	/* fulltable ID within array
						 * (across rows) */
	if (raidPtr->numRow == 1)
		*row = 0;	/* avoid a mod and a div in the common case */
	else {
		*row = FullTableID % raidPtr->numRow;
		FullTableID /= raidPtr->numRow;	/* convert to fulltable ID on
						 * this disk */
	}
	if (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) {
		SpareRegion = FullTableID / info->FullTablesPerSpareRegion;
		SpareSpace = SpareRegion * info->SpareSpaceDepthPerRegionInSUs;
	}
	FullTableOffset = SUID % sus_per_fulltable;
	TableID = FullTableOffset / info->SUsPerTable;
	TableOffset = FullTableOffset - TableID * info->SUsPerTable;
	BlockID = TableOffset / info->PUsPerBlock;
	BlockOffset = TableOffset - BlockID * info->PUsPerBlock;
	BlockID %= info->BlocksPerTable;
	RepIndex = info->PUsPerBlock - TableID;
	if (!raidPtr->noRotate)
		BlockOffset += ((BlockOffset >= RepIndex) ? 1 : 0);
	*col = info->LayoutTable[BlockID][BlockOffset];

	/* remap to distributed spare space if indicated */
	if (remap) {
		RF_ASSERT(raidPtr->Disks[*row][*col].status == rf_ds_reconstructing || raidPtr->Disks[*row][*col].status == rf_ds_dist_spared ||
		    (rf_copyback_in_progress && raidPtr->Disks[*row][*col].status == rf_ds_optimal));
		rf_remap_to_spare_space(layoutPtr, info, *row, FullTableID, TableID, BlockID, (base_suid) ? 1 : 0, SpareRegion, col, &outSU);
	} else {

		outSU = base_suid;
		outSU += FullTableID * fulltable_depth;	/* offs to strt of FT */
		outSU += SpareSpace;	/* skip rsvd spare space */
		outSU += TableID * info->TableDepthInPUs * layoutPtr->SUsPerPU;	/* offs to strt of tble */
		outSU += info->OffsetTable[BlockID][BlockOffset] * layoutPtr->SUsPerPU;	/* offs to the PU */
	}
	outSU += TableOffset / (info->BlocksPerTable * info->PUsPerBlock);	/* offs to the SU within
										 * a PU */

	/* convert SUs to sectors, and, if not aligned to SU boundary, add in
	 * offset to sector.  */
	*diskSector = outSU * layoutPtr->sectorsPerStripeUnit + (raidSector % layoutPtr->sectorsPerStripeUnit);

	RF_ASSERT(*col != -1);
}


/* prototyping this inexplicably causes the compile of the layout table (rf_layout.c) to fail */
void 
rf_MapParityDeclustered(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t raidSector,
    RF_RowCol_t * row,
    RF_RowCol_t * col,
    RF_SectorNum_t * diskSector,
    int remap)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_DeclusteredConfigInfo_t *info = (RF_DeclusteredConfigInfo_t *) layoutPtr->layoutSpecificInfo;
	RF_StripeNum_t SUID = raidSector / layoutPtr->sectorsPerStripeUnit;
	RF_StripeNum_t FullTableID, FullTableOffset, TableID, TableOffset;
	RF_StripeNum_t BlockID, BlockOffset, RepIndex;
	RF_StripeCount_t sus_per_fulltable = info->SUsPerFullTable;
	RF_StripeCount_t fulltable_depth = info->FullTableDepthInPUs * layoutPtr->SUsPerPU;
	RF_StripeNum_t base_suid = 0, outSU, SpareRegion = 0, SpareSpace = 0;

	rf_decluster_adjust_params(layoutPtr, &SUID, &sus_per_fulltable, &fulltable_depth, &base_suid);

	/* compute row & (possibly) spare space exactly as before */
	FullTableID = SUID / sus_per_fulltable;
	if (raidPtr->numRow == 1)
		*row = 0;	/* avoid a mod and a div in the common case */
	else {
		*row = FullTableID % raidPtr->numRow;
		FullTableID /= raidPtr->numRow;	/* convert to fulltable ID on
						 * this disk */
	}
	if ((raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
		SpareRegion = FullTableID / info->FullTablesPerSpareRegion;
		SpareSpace = SpareRegion * info->SpareSpaceDepthPerRegionInSUs;
	}
	/* compute BlockID and RepIndex exactly as before */
	FullTableOffset = SUID % sus_per_fulltable;
	TableID = FullTableOffset / info->SUsPerTable;
	TableOffset = FullTableOffset - TableID * info->SUsPerTable;
	/* TableOffset     = FullTableOffset % info->SUsPerTable; */
	/* BlockID         = (TableOffset / info->PUsPerBlock) %
	 * info->BlocksPerTable; */
	BlockID = TableOffset / info->PUsPerBlock;
	/* BlockOffset     = TableOffset % info->PUsPerBlock; */
	BlockOffset = TableOffset - BlockID * info->PUsPerBlock;
	BlockID %= info->BlocksPerTable;

	/* the parity block is in the position indicated by RepIndex */
	RepIndex = (raidPtr->noRotate) ? info->PUsPerBlock : info->PUsPerBlock - TableID;
	*col = info->LayoutTable[BlockID][RepIndex];

	if (remap) {
		RF_ASSERT(raidPtr->Disks[*row][*col].status == rf_ds_reconstructing || raidPtr->Disks[*row][*col].status == rf_ds_dist_spared ||
		    (rf_copyback_in_progress && raidPtr->Disks[*row][*col].status == rf_ds_optimal));
		rf_remap_to_spare_space(layoutPtr, info, *row, FullTableID, TableID, BlockID, (base_suid) ? 1 : 0, SpareRegion, col, &outSU);
	} else {

		/* compute sector as before, except use RepIndex instead of
		 * BlockOffset */
		outSU = base_suid;
		outSU += FullTableID * fulltable_depth;
		outSU += SpareSpace;	/* skip rsvd spare space */
		outSU += TableID * info->TableDepthInPUs * layoutPtr->SUsPerPU;
		outSU += info->OffsetTable[BlockID][RepIndex] * layoutPtr->SUsPerPU;
	}

	outSU += TableOffset / (info->BlocksPerTable * info->PUsPerBlock);
	*diskSector = outSU * layoutPtr->sectorsPerStripeUnit + (raidSector % layoutPtr->sectorsPerStripeUnit);

	RF_ASSERT(*col != -1);
}
/* returns an array of ints identifying the disks that comprise the stripe containing the indicated address.
 * the caller must _never_ attempt to modify this array.
 */
void 
rf_IdentifyStripeDeclustered(
    RF_Raid_t * raidPtr,
    RF_RaidAddr_t addr,
    RF_RowCol_t ** diskids,
    RF_RowCol_t * outRow)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_DeclusteredConfigInfo_t *info = (RF_DeclusteredConfigInfo_t *) layoutPtr->layoutSpecificInfo;
	RF_StripeCount_t sus_per_fulltable = info->SUsPerFullTable;
	RF_StripeCount_t fulltable_depth = info->FullTableDepthInPUs * layoutPtr->SUsPerPU;
	RF_StripeNum_t base_suid = 0;
	RF_StripeNum_t SUID = rf_RaidAddressToStripeUnitID(layoutPtr, addr);
	RF_StripeNum_t stripeID, FullTableID;
	int     tableOffset;

	rf_decluster_adjust_params(layoutPtr, &SUID, &sus_per_fulltable, &fulltable_depth, &base_suid);
	FullTableID = SUID / sus_per_fulltable;	/* fulltable ID within array
						 * (across rows) */
	*outRow = FullTableID % raidPtr->numRow;
	stripeID = rf_StripeUnitIDToStripeID(layoutPtr, SUID);	/* find stripe offset
								 * into array */
	tableOffset = (stripeID % info->BlocksPerTable);	/* find offset into
								 * block design table */
	*diskids = info->LayoutTable[tableOffset];
}
/* This returns the default head-separation limit, which is measured
 * in "required units for reconstruction".  Each time a disk fetches
 * a unit, it bumps a counter.  The head-sep code prohibits any disk
 * from getting more than headSepLimit counter values ahead of any
 * other.
 *
 * We assume here that the number of floating recon buffers is already
 * set.  There are r stripes to be reconstructed in each table, and so
 * if we have a total of B buffers, we can have at most B/r tables
 * under recon at any one time.  In each table, lambda units are required
 * from each disk, so given B buffers, the head sep limit has to be
 * (lambda*B)/r units.  We subtract one to avoid weird boundary cases.
 *
 * for example, suppose were given 50 buffers, r=19, and lambda=4 as in
 * the 20.5 design.  There are 19 stripes/table to be reconstructed, so
 * we can have 50/19 tables concurrently under reconstruction, which means
 * we can allow the fastest disk to get 50/19 tables ahead of the slower
 * disk.  There are lambda "required units" for each disk, so the fastest
 * disk can get 4*50/19 = 10 counter values ahead of the slowest.
 *
 * If numBufsToAccumulate is not 1, we need to limit the head sep further
 * because multiple bufs will be required for each stripe under recon.
 */
RF_HeadSepLimit_t 
rf_GetDefaultHeadSepLimitDeclustered(
    RF_Raid_t * raidPtr)
{
	RF_DeclusteredConfigInfo_t *info = (RF_DeclusteredConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;

	return (info->Lambda * raidPtr->numFloatingReconBufs / info->TableDepthInPUs / rf_numBufsToAccumulate);
}
/* returns the default number of recon buffers to use.  The value
 * is somewhat arbitrary...it's intended to be large enough to allow
 * for a reasonably large head-sep limit, but small enough that you
 * don't use up all your system memory with buffers.
 */
int 
rf_GetDefaultNumFloatingReconBuffersDeclustered(RF_Raid_t * raidPtr)
{
	return (100 * rf_numBufsToAccumulate);
}
/* sectors in the last fulltable of the array need to be handled
 * specially since this fulltable can be incomplete.  this function
 * changes the values of certain params to handle this.
 *
 * the idea here is that MapSector et. al. figure out which disk the
 * addressed unit lives on by computing the modulos of the unit number
 * with the number of units per fulltable, table, etc.  In the last
 * fulltable, there are fewer units per fulltable, so we need to adjust
 * the number of user data units per fulltable to reflect this.
 *
 * so, we (1) convert the fulltable size and depth parameters to
 * the size of the partial fulltable at the end, (2) compute the
 * disk sector offset where this fulltable starts, and (3) convert
 * the users stripe unit number from an offset into the array to
 * an offset into the last fulltable.
 */
void 
rf_decluster_adjust_params(
    RF_RaidLayout_t * layoutPtr,
    RF_StripeNum_t * SUID,
    RF_StripeCount_t * sus_per_fulltable,
    RF_StripeCount_t * fulltable_depth,
    RF_StripeNum_t * base_suid)
{
	RF_DeclusteredConfigInfo_t *info = (RF_DeclusteredConfigInfo_t *) layoutPtr->layoutSpecificInfo;

	if (*SUID >= info->FullTableLimitSUID) {
		/* new full table size is size of last full table on disk */
		*sus_per_fulltable = info->ExtraTablesPerDisk * info->SUsPerTable;

		/* new full table depth is corresponding depth */
		*fulltable_depth = info->ExtraTablesPerDisk * info->TableDepthInPUs * layoutPtr->SUsPerPU;

		/* set up the new base offset */
		*base_suid = info->DiskOffsetOfLastFullTableInSUs;

		/* convert users array address to an offset into the last
		 * fulltable */
		*SUID -= info->FullTableLimitSUID;
	}
}
/*
 * map a stripe ID to a parity stripe ID.
 * See comment above RaidAddressToParityStripeID in layout.c.
 */
void 
rf_MapSIDToPSIDDeclustered(
    RF_RaidLayout_t * layoutPtr,
    RF_StripeNum_t stripeID,
    RF_StripeNum_t * psID,
    RF_ReconUnitNum_t * which_ru)
{
	RF_DeclusteredConfigInfo_t *info;

	info = (RF_DeclusteredConfigInfo_t *) layoutPtr->layoutSpecificInfo;

	*psID = (stripeID / (layoutPtr->SUsPerPU * info->BlocksPerTable))
	    * info->BlocksPerTable + (stripeID % info->BlocksPerTable);
	*which_ru = (stripeID % (info->BlocksPerTable * layoutPtr->SUsPerPU))
	    / info->BlocksPerTable;
	RF_ASSERT((*which_ru) < layoutPtr->SUsPerPU / layoutPtr->SUsPerRU);
}
/*
 * Called from MapSector and MapParity to retarget an access at the spare unit.
 * Modifies the "col" and "outSU" parameters only.
 */
void 
rf_remap_to_spare_space(
    RF_RaidLayout_t * layoutPtr,
    RF_DeclusteredConfigInfo_t * info,
    RF_RowCol_t row,
    RF_StripeNum_t FullTableID,
    RF_StripeNum_t TableID,
    RF_SectorNum_t BlockID,
    RF_StripeNum_t base_suid,
    RF_StripeNum_t SpareRegion,
    RF_RowCol_t * outCol,
    RF_StripeNum_t * outSU)
{
	RF_StripeNum_t ftID, spareTableStartSU, TableInSpareRegion, lastSROffset,
	        which_ft;

	/*
         * note that FullTableID and hence SpareRegion may have gotten
         * tweaked by rf_decluster_adjust_params. We detect this by
         * noticing that base_suid is not 0.
         */
	if (base_suid == 0) {
		ftID = FullTableID;
	} else {
		/*
	         * There may be > 1.0 full tables in the last (i.e. partial)
	         * spare region.  find out which of these we're in.
	         */
		lastSROffset = info->NumCompleteSRs * info->SpareRegionDepthInSUs;
		which_ft = (info->DiskOffsetOfLastFullTableInSUs - lastSROffset) / (info->FullTableDepthInPUs * layoutPtr->SUsPerPU);

		/* compute the actual full table ID */
		ftID = info->DiskOffsetOfLastFullTableInSUs / (info->FullTableDepthInPUs * layoutPtr->SUsPerPU) + which_ft;
		SpareRegion = info->NumCompleteSRs;
	}
	TableInSpareRegion = (ftID * info->NumParityReps + TableID) % info->TablesPerSpareRegion;

	*outCol = info->SpareTable[TableInSpareRegion][BlockID].spareDisk;
	RF_ASSERT(*outCol != -1);

	spareTableStartSU = (SpareRegion == info->NumCompleteSRs) ?
	    info->DiskOffsetOfLastFullTableInSUs + info->ExtraTablesPerDisk * info->TableDepthInPUs * layoutPtr->SUsPerPU :
	    (SpareRegion + 1) * info->SpareRegionDepthInSUs - info->SpareSpaceDepthPerRegionInSUs;
	*outSU = spareTableStartSU + info->SpareTable[TableInSpareRegion][BlockID].spareBlockOffsetInSUs;
	if (*outSU >= layoutPtr->stripeUnitsPerDisk) {
		printf("rf_remap_to_spare_space: invalid remapped disk SU offset %ld\n", (long) *outSU);
	}
}

#endif /* (RF_INCLUDE_PARITY_DECLUSTERING > 0)  || (RF_INCLUDE_PARITY_DECLUSTERING_PQ > 0) */


int 
rf_InstallSpareTable(
    RF_Raid_t * raidPtr,
    RF_RowCol_t frow,
    RF_RowCol_t fcol)
{
	RF_DeclusteredConfigInfo_t *info = (RF_DeclusteredConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;
	RF_SparetWait_t *req;
	int     retcode;

	RF_Malloc(req, sizeof(*req), (RF_SparetWait_t *));
	req->C = raidPtr->numCol;
	req->G = raidPtr->Layout.numDataCol + raidPtr->Layout.numParityCol;
	req->fcol = fcol;
	req->SUsPerPU = raidPtr->Layout.SUsPerPU;
	req->TablesPerSpareRegion = info->TablesPerSpareRegion;
	req->BlocksPerTable = info->BlocksPerTable;
	req->TableDepthInPUs = info->TableDepthInPUs;
	req->SpareSpaceDepthPerRegionInSUs = info->SpareSpaceDepthPerRegionInSUs;

	retcode = rf_GetSpareTableFromDaemon(req);
	RF_ASSERT(!retcode);	/* XXX -- fix this to recover gracefully --
				 * XXX */
	return (retcode);
}
/*
 * Invoked via ioctl to install a spare table in the kernel.
 */
int 
rf_SetSpareTable(raidPtr, data)
	RF_Raid_t *raidPtr;
	void   *data;
{
	RF_DeclusteredConfigInfo_t *info = (RF_DeclusteredConfigInfo_t *) raidPtr->Layout.layoutSpecificInfo;
	RF_SpareTableEntry_t **ptrs;
	int     i, retcode;

	/* what we need to copyin is a 2-d array, so first copyin the user
	 * pointers to the rows in the table */
	RF_Malloc(ptrs, info->TablesPerSpareRegion * sizeof(RF_SpareTableEntry_t *), (RF_SpareTableEntry_t **));
	retcode = copyin((caddr_t) data, (caddr_t) ptrs, info->TablesPerSpareRegion * sizeof(RF_SpareTableEntry_t *));

	if (retcode)
		return (retcode);

	/* now allocate kernel space for the row pointers */
	RF_Malloc(info->SpareTable, info->TablesPerSpareRegion * sizeof(RF_SpareTableEntry_t *), (RF_SpareTableEntry_t **));

	/* now allocate kernel space for each row in the table, and copy it in
	 * from user space */
	for (i = 0; i < info->TablesPerSpareRegion; i++) {
		RF_Malloc(info->SpareTable[i], info->BlocksPerTable * sizeof(RF_SpareTableEntry_t), (RF_SpareTableEntry_t *));
		retcode = copyin(ptrs[i], info->SpareTable[i], info->BlocksPerTable * sizeof(RF_SpareTableEntry_t));
		if (retcode) {
			info->SpareTable = NULL;	/* blow off the memory
							 * we've allocated */
			return (retcode);
		}
	}

	/* free up the temporary array we used */
	RF_Free(ptrs, info->TablesPerSpareRegion * sizeof(RF_SpareTableEntry_t *));

	return (0);
}

RF_ReconUnitCount_t 
rf_GetNumSpareRUsDeclustered(raidPtr)
	RF_Raid_t *raidPtr;
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;

	return (((RF_DeclusteredConfigInfo_t *) layoutPtr->layoutSpecificInfo)->TotSparePUsPerDisk);
}

void 
rf_FreeSpareTable(raidPtr)
	RF_Raid_t *raidPtr;
{
	long    i;
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_DeclusteredConfigInfo_t *info = (RF_DeclusteredConfigInfo_t *) layoutPtr->layoutSpecificInfo;
	RF_SpareTableEntry_t **table = info->SpareTable;

	for (i = 0; i < info->TablesPerSpareRegion; i++) {
		RF_Free(table[i], info->BlocksPerTable * sizeof(RF_SpareTableEntry_t));
	}
	RF_Free(table, info->TablesPerSpareRegion * sizeof(RF_SpareTableEntry_t *));
	info->SpareTable = (RF_SpareTableEntry_t **) NULL;
}
