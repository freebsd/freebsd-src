/*	$FreeBSD$ */
/*	$NetBSD: rf_options.h,v 1.3 1999/02/05 00:06:13 oster Exp $	*/
/*
 * rf_options.h
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

#ifndef _RF__RF_OPTIONS_H_
#define _RF__RF_OPTIONS_H_

#define RF_DEFAULT_LOCK_TABLE_SIZE 256

typedef struct RF_DebugNames_s {
	char   *name;
	long   *ptr;
}       RF_DebugName_t;

extern RF_DebugName_t rf_debugNames[];

#ifdef RF_DBG_OPTION
#undef RF_DBG_OPTION
#endif				/* RF_DBG_OPTION */

#ifdef __STDC__
#define RF_DBG_OPTION(_option_,_defval_) extern long rf_##_option_;
#else				/* __STDC__ */
#define RF_DBG_OPTION(_option_,_defval_) extern long rf_/**/_option_;
#endif				/* __STDC__ */
#include <dev/raidframe/rf_optnames.h>

void    rf_ResetDebugOptions(void);

#endif				/* !_RF__RF_OPTIONS_H_ */
