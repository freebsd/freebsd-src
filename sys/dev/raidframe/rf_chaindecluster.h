/*	$FreeBSD$ */
/*	$NetBSD: rf_chaindecluster.h,v 1.4 2001/01/26 04:14:14 oster Exp $	*/
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

/* rf_chaindecluster.h
 * header file for Chained Declustering
 */


#ifndef _RF__RF_CHAINDECLUSTER_H_
#define _RF__RF_CHAINDECLUSTER_H_

int 
rf_ConfigureChainDecluster(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr);
RF_ReconUnitCount_t rf_GetNumSpareRUsChainDecluster(RF_Raid_t * raidPtr);
void 
rf_MapSectorChainDecluster(RF_Raid_t * raidPtr, RF_RaidAddr_t raidSector,
    RF_RowCol_t * row, RF_RowCol_t * col, RF_SectorNum_t * diskSector, int remap);
void 
rf_MapParityChainDecluster(RF_Raid_t * raidPtr, RF_RaidAddr_t raidSector,
    RF_RowCol_t * row, RF_RowCol_t * col, RF_SectorNum_t * diskSector, int remap);
void 
rf_IdentifyStripeChainDecluster(RF_Raid_t * raidPtr, RF_RaidAddr_t addr,
    RF_RowCol_t ** diskids, RF_RowCol_t * outRow);
void 
rf_MapSIDToPSIDChainDecluster(RF_RaidLayout_t * layoutPtr,
    RF_StripeNum_t stripeID, RF_StripeNum_t * psID,
    RF_ReconUnitNum_t * which_ru);
void 
rf_RAIDCDagSelect(RF_Raid_t * raidPtr, RF_IoType_t type,
    RF_AccessStripeMap_t * asmap,
    RF_VoidFuncPtr *);
#if 0
void    (**createFunc) (RF_Raid_t *,
            RF_AccessStripeMap_t *,
            RF_DagHeader_t *,
            void *,
            RF_RaidAccessFlags_t,
            RF_AllocListElem_t *);
#endif

#endif				/* !_RF__RF_CHAINDECLUSTER_H_ */
