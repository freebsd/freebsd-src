/*	$FreeBSD$ */
/*	$NetBSD: rf_hist.h,v 1.3 1999/02/05 00:06:12 oster Exp $	*/
/*
 * rf_hist.h
 *
 * Histgram operations for RAIDframe stats
 */
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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

#ifndef _RF__RF_HIST_H_
#define _RF__RF_HIST_H_

#include <dev/raidframe/rf_types.h>

#define RF_HIST_RESOLUTION   5
#define RF_HIST_MIN_VAL      0
#define RF_HIST_MAX_VAL      1000
#define RF_HIST_RANGE        (RF_HIST_MAX_VAL - RF_HIST_MIN_VAL)
#define RF_HIST_NUM_BUCKETS  (RF_HIST_RANGE / RF_HIST_RESOLUTION + 1)

typedef RF_uint32 RF_Hist_t;

#define RF_HIST_ADD(_hist_,_val_) { \
	RF_Hist_t val; \
	val = ((RF_Hist_t)(_val_)) / 1000; \
	if (val >= RF_HIST_MAX_VAL) \
		_hist_[RF_HIST_NUM_BUCKETS-1]++; \
	else \
		_hist_[(val - RF_HIST_MIN_VAL) / RF_HIST_RESOLUTION]++; \
}

#endif				/* !_RF__RF_HIST_H_ */
