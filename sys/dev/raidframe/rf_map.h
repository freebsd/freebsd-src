/*	$FreeBSD$ */
/*	$NetBSD: rf_map.h,v 1.3 1999/02/05 00:06:12 oster Exp $	*/
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

/* rf_map.h */

#ifndef _RF__RF_MAP_H_
#define _RF__RF_MAP_H_

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_alloclist.h>
#include <dev/raidframe/rf_raid.h>

/* mapping structure allocation and free routines */
RF_AccessStripeMapHeader_t *
rf_MapAccess(RF_Raid_t * raidPtr,
    RF_RaidAddr_t raidAddress, RF_SectorCount_t numBlocks,
    caddr_t buffer, int remap);

void 
rf_MarkFailuresInASMList(RF_Raid_t * raidPtr,
    RF_AccessStripeMapHeader_t * asm_h);

RF_AccessStripeMap_t *rf_DuplicateASM(RF_AccessStripeMap_t * asmap);

RF_PhysDiskAddr_t *rf_DuplicatePDA(RF_PhysDiskAddr_t * pda);

int     rf_ConfigureMapModule(RF_ShutdownList_t ** listp);

RF_AccessStripeMapHeader_t *rf_AllocAccessStripeMapHeader(void);

void    rf_FreeAccessStripeMapHeader(RF_AccessStripeMapHeader_t * p);

RF_PhysDiskAddr_t *rf_AllocPhysDiskAddr(void);

RF_PhysDiskAddr_t *rf_AllocPDAList(int count);

void    rf_FreePhysDiskAddr(RF_PhysDiskAddr_t * p);

RF_AccessStripeMap_t *rf_AllocAccessStripeMapComponent(void);

RF_AccessStripeMap_t *rf_AllocASMList(int count);

void    rf_FreeAccessStripeMapComponent(RF_AccessStripeMap_t * p);

void    rf_FreeAccessStripeMap(RF_AccessStripeMapHeader_t * hdr);

int     rf_CheckStripeForFailures(RF_Raid_t * raidPtr, RF_AccessStripeMap_t * asmap);

int     rf_NumFailedDataUnitsInStripe(RF_Raid_t * raidPtr, RF_AccessStripeMap_t * asmap);

void    rf_PrintAccessStripeMap(RF_AccessStripeMapHeader_t * asm_h);

void    rf_PrintFullAccessStripeMap(RF_AccessStripeMapHeader_t * asm_h, int prbuf);

void 
rf_PrintRaidAddressInfo(RF_Raid_t * raidPtr, RF_RaidAddr_t raidAddr,
    RF_SectorCount_t numBlocks);

void 
rf_ASMParityAdjust(RF_PhysDiskAddr_t * toAdjust,
    RF_StripeNum_t startAddrWithinStripe, RF_SectorNum_t endAddress,
    RF_RaidLayout_t * layoutPtr, RF_AccessStripeMap_t * asm_p);

void 
rf_ASMCheckStatus(RF_Raid_t * raidPtr, RF_PhysDiskAddr_t * pda_p,
    RF_AccessStripeMap_t * asm_p, RF_RaidDisk_t ** disks, int parity);

#endif				/* !_RF__RF_MAP_H_ */
