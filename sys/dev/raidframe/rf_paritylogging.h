/*	$FreeBSD$ */
/*	$NetBSD: rf_paritylogging.h,v 1.3 1999/02/05 00:06:14 oster Exp $	*/
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

/* header file for Parity Logging */

#ifndef _RF__RF_PARITYLOGGING_H_
#define _RF__RF_PARITYLOGGING_H_

int 
rf_ConfigureParityLogging(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr);
int     rf_GetDefaultNumFloatingReconBuffersParityLogging(RF_Raid_t * raidPtr);
RF_HeadSepLimit_t rf_GetDefaultHeadSepLimitParityLogging(RF_Raid_t * raidPtr);
RF_RegionId_t 
rf_MapRegionIDParityLogging(RF_Raid_t * raidPtr,
    RF_SectorNum_t address);
void 
rf_MapSectorParityLogging(RF_Raid_t * raidPtr, RF_RaidAddr_t raidSector,
    RF_RowCol_t * row, RF_RowCol_t * col, RF_SectorNum_t * diskSector,
    int remap);
void 
rf_MapParityParityLogging(RF_Raid_t * raidPtr, RF_RaidAddr_t raidSector,
    RF_RowCol_t * row, RF_RowCol_t * col, RF_SectorNum_t * diskSector,
    int remap);
void 
rf_MapLogParityLogging(RF_Raid_t * raidPtr, RF_RegionId_t regionID,
    RF_SectorNum_t regionOffset, RF_RowCol_t * row, RF_RowCol_t * col,
    RF_SectorNum_t * startSector);
void 
rf_MapRegionParity(RF_Raid_t * raidPtr, RF_RegionId_t regionID,
    RF_RowCol_t * row, RF_RowCol_t * col, RF_SectorNum_t * startSector,
    RF_SectorCount_t * numSector);
void 
rf_IdentifyStripeParityLogging(RF_Raid_t * raidPtr, RF_RaidAddr_t addr,
    RF_RowCol_t ** diskids, RF_RowCol_t * outRow);
void 
rf_MapSIDToPSIDParityLogging(RF_RaidLayout_t * layoutPtr,
    RF_StripeNum_t stripeID, RF_StripeNum_t * psID,
    RF_ReconUnitNum_t * which_ru);
void 
rf_ParityLoggingDagSelect(RF_Raid_t * raidPtr, RF_IoType_t type,
    RF_AccessStripeMap_t * asmap, RF_VoidFuncPtr * createFunc);

#endif				/* !_RF__RF_PARITYLOGGING_H_ */
