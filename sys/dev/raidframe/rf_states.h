/*	$FreeBSD$ */
/*	$NetBSD: rf_states.h,v 1.3 1999/02/05 00:06:17 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, William V. Courtright II, Robby Findler
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

#ifndef _RF__RF_STATES_H_
#define _RF__RF_STATES_H_

#include <dev/raidframe/rf_types.h>

void    rf_ContinueRaidAccess(RF_RaidAccessDesc_t * desc);
void    rf_ContinueDagAccess(RF_DagList_t * dagList);
int     rf_State_LastState(RF_RaidAccessDesc_t * desc);
int     rf_State_IncrAccessCount(RF_RaidAccessDesc_t * desc);
int     rf_State_DecrAccessCount(RF_RaidAccessDesc_t * desc);
int     rf_State_Quiesce(RF_RaidAccessDesc_t * desc);
int     rf_State_Map(RF_RaidAccessDesc_t * desc);
int     rf_State_Lock(RF_RaidAccessDesc_t * desc);
int     rf_State_CreateDAG(RF_RaidAccessDesc_t * desc);
int     rf_State_ExecuteDAG(RF_RaidAccessDesc_t * desc);
int     rf_State_ProcessDAG(RF_RaidAccessDesc_t * desc);
int     rf_State_Cleanup(RF_RaidAccessDesc_t * desc);

#endif				/* !_RF__RF_STATES_H_ */
