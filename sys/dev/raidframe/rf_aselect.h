/*	$FreeBSD$ */
/*	$NetBSD: rf_aselect.h,v 1.3 1999/02/05 00:06:06 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, William V. Courtright II
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

/*****************************************************************************
 *
 * aselect.h -- header file for algorithm selection code
 *
 *****************************************************************************/

#ifndef _RF__RF_ASELECT_H_
#define _RF__RF_ASELECT_H_

#include <dev/raidframe/rf_desc.h>

int     rf_SelectAlgorithm(RF_RaidAccessDesc_t * desc, RF_RaidAccessFlags_t flags);

#endif				/* !_RF__RF_ASELECT_H_ */
