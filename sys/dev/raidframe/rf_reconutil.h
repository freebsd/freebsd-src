/*	$FreeBSD$ */
/*	$NetBSD: rf_reconutil.h,v 1.3 1999/02/05 00:06:17 oster Exp $	*/
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

/************************************************************
 * rf_reconutil.h -- header file for reconstruction utilities
 ************************************************************/

#ifndef _RF__RF_RECONUTIL_H_
#define _RF__RF_RECONUTIL_H_

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_reconstruct.h>

RF_ReconCtrl_t *
rf_MakeReconControl(RF_RaidReconDesc_t * reconDesc,
    RF_RowCol_t frow, RF_RowCol_t fcol, RF_RowCol_t srow, RF_RowCol_t scol);
void    rf_FreeReconControl(RF_Raid_t * raidPtr, RF_RowCol_t row);
RF_HeadSepLimit_t rf_GetDefaultHeadSepLimit(RF_Raid_t * raidPtr);
int     rf_GetDefaultNumFloatingReconBuffers(RF_Raid_t * raidPtr);
RF_ReconBuffer_t *
rf_MakeReconBuffer(RF_Raid_t * raidPtr, RF_RowCol_t row,
    RF_RowCol_t col, RF_RbufType_t type);
void    rf_FreeReconBuffer(RF_ReconBuffer_t * rbuf);
void    rf_CheckFloatingRbufCount(RF_Raid_t * raidPtr, int dolock);

#endif				/* !_RF__RF_RECONUTIL_H_ */
