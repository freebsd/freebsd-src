/*	$FreeBSD$ */
/*	$NetBSD: rf_copyback.h,v 1.3 1999/02/05 00:06:06 oster Exp $	*/
/*
 * rf_copyback.h
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

#ifndef _RF__RF_COPYBACK_H_
#define _RF__RF_COPYBACK_H_

#include <dev/raidframe/rf_types.h>

typedef struct RF_CopybackDesc_s {
	RF_Raid_t *raidPtr;
	RF_RowCol_t frow;
	RF_RowCol_t fcol;
	RF_RowCol_t spRow;
	RF_RowCol_t spCol;
	int     status;
	RF_StripeNum_t stripeAddr;
	RF_SectorCount_t sectPerSU;
	RF_SectorCount_t sectPerStripe;
	char   *databuf;
	RF_DiskQueueData_t *readreq;
	RF_DiskQueueData_t *writereq;
	struct timeval starttime;
	RF_MCPair_t *mcpair;
}       RF_CopybackDesc_t;

extern int rf_copyback_in_progress;

int     rf_ConfigureCopyback(RF_ShutdownList_t ** listp);
void    rf_CopybackReconstructedData(RF_Raid_t * raidPtr);
void    rf_ContinueCopyback(RF_CopybackDesc_t * desc);

#endif				/* !_RF__RF_COPYBACK_H_ */
