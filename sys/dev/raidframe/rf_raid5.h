/*	$FreeBSD$ */
/*	$NetBSD: rf_raid5.h,v 1.3 1999/02/05 00:06:16 oster Exp $	*/
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

/* rf_raid5.h - header file for RAID Level 5 */

#ifndef _RF__RF_RAID5_H_
#define _RF__RF_RAID5_H_

int 
rf_ConfigureRAID5(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr);
int     rf_GetDefaultNumFloatingReconBuffersRAID5(RF_Raid_t * raidPtr);
RF_HeadSepLimit_t rf_GetDefaultHeadSepLimitRAID5(RF_Raid_t * raidPtr);
void 
rf_MapSectorRAID5(RF_Raid_t * raidPtr, RF_RaidAddr_t raidSector,
    RF_RowCol_t * row, RF_RowCol_t * col, RF_SectorNum_t * diskSector, int remap);
void 
rf_MapParityRAID5(RF_Raid_t * raidPtr, RF_RaidAddr_t raidSector,
    RF_RowCol_t * row, RF_RowCol_t * col, RF_SectorNum_t * diskSector, int remap);
void 
rf_IdentifyStripeRAID5(RF_Raid_t * raidPtr, RF_RaidAddr_t addr,
    RF_RowCol_t ** diskids, RF_RowCol_t * outRow);
void 
rf_MapSIDToPSIDRAID5(RF_RaidLayout_t * layoutPtr,
    RF_StripeNum_t stripeID, RF_StripeNum_t * psID,
    RF_ReconUnitNum_t * which_ru);
void 
rf_RaidFiveDagSelect(RF_Raid_t * raidPtr, RF_IoType_t type,
    RF_AccessStripeMap_t * asmap, RF_VoidFuncPtr * createFunc);

#endif				/* !_RF__RF_RAID5_H_ */
