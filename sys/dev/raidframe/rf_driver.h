/*	$FreeBSD$ */
/*	$NetBSD: rf_driver.h,v 1.4 2000/02/13 04:53:57 oster Exp $	*/
/*
 * rf_driver.h
 */
/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Jim Zelenka
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

#ifndef _RF__RF_DRIVER_H_
#define _RF__RF_DRIVER_H_

#include <dev/raidframe/rf_threadstuff.h>
#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_bsd.h>

#if _KERNEL
RF_DECLARE_EXTERN_MUTEX(rf_printf_mutex)
int     rf_BootRaidframe(void);
int     rf_UnbootRaidframe(void);
int     rf_Shutdown(RF_Raid_t * raidPtr);
int     rf_Configure(RF_Raid_t * raidPtr, RF_Config_t * cfgPtr,
		     RF_AutoConfig_t *ac);
RF_RaidAccessDesc_t *rf_AllocRaidAccDesc(RF_Raid_t * raidPtr, RF_IoType_t type,
					 RF_RaidAddr_t raidAddress, 
					 RF_SectorCount_t numBlocks, 
					 caddr_t bufPtr,
					 void *bp, RF_DagHeader_t ** paramDAG,
					 RF_AccessStripeMapHeader_t ** paramASM,
					 RF_RaidAccessFlags_t flags, 
					 void (*cbF) (RF_Buf_t), 
					 void *cbA,
					 RF_AccessState_t * states);
void    rf_FreeRaidAccDesc(RF_RaidAccessDesc_t * desc);
int     rf_DoAccess(RF_Raid_t * raidPtr, RF_IoType_t type, int async_flag,
		    RF_RaidAddr_t raidAddress, RF_SectorCount_t numBlocks, 
		    caddr_t bufPtr, void *bp_in, RF_DagHeader_t ** paramDAG,
		    RF_AccessStripeMapHeader_t ** paramASM, 
		    RF_RaidAccessFlags_t flags, 
		    RF_RaidAccessDesc_t ** paramDesc, 
		    void (*cbF) (RF_Buf_t), void *cbA);
int     rf_SetReconfiguredMode(RF_Raid_t * raidPtr, RF_RowCol_t row,
			       RF_RowCol_t col);
int     rf_FailDisk(RF_Raid_t * raidPtr, RF_RowCol_t frow, RF_RowCol_t fcol,
		    int initRecon);
void    rf_SignalQuiescenceLock(RF_Raid_t * raidPtr, 
				RF_RaidReconDesc_t * reconDesc);
int     rf_SuspendNewRequestsAndWait(RF_Raid_t * raidPtr);
void    rf_ResumeNewRequests(RF_Raid_t * raidPtr);
void    rf_StartThroughputStats(RF_Raid_t * raidPtr);
void    rf_StartUserStats(RF_Raid_t * raidPtr);
void    rf_StopUserStats(RF_Raid_t * raidPtr);
void    rf_UpdateUserStats(RF_Raid_t * raidPtr, int rt, int numsect);
void    rf_PrintUserStats(RF_Raid_t * raidPtr);
#endif /* _KERNEL */
#endif				/* !_RF__RF_DRIVER_H_ */
