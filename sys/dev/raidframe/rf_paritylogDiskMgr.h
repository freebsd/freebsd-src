/*	$FreeBSD$ */
/*	$NetBSD: rf_paritylogDiskMgr.h,v 1.3 1999/02/05 00:06:14 oster Exp $	*/
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

/* header file for parity log disk mgr code
 *
 */

#ifndef _RF__RF_PARITYLOGDISKMGR_H_
#define _RF__RF_PARITYLOGDISKMGR_H_

#include <dev/raidframe/rf_types.h>

int     rf_ShutdownLogging(RF_Raid_t * raidPtr);
int     rf_ParityLoggingDiskManager(RF_Raid_t * raidPtr);

#endif				/* !_RF__RF_PARITYLOGDISKMGR_H_ */
