/*	$NetBSD: rf_options.c,v 1.3 1999/02/05 00:06:13 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*
 * rf_options.c
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


#include <dev/raidframe/rf_threadstuff.h>
#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_archs.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_options.h>

#ifdef RF_DBG_OPTION
#undef RF_DBG_OPTION
#endif				/* RF_DBG_OPTION */

#ifdef __STDC__
#define RF_DBG_OPTION(_option_,_defval_) long rf_##_option_ = _defval_;
#else				/* __STDC__ */
#define RF_DBG_OPTION(_option_,_defval_) long rf_/**/_option_ = _defval_;
#endif				/* __STDC__ */

#include <dev/raidframe/rf_optnames.h>

#undef RF_DBG_OPTION

#ifdef __STDC__
#define RF_DBG_OPTION(_option_,_defval_) { RF_STRING(_option_), &rf_##_option_ },
#else				/* __STDC__ */
#define RF_DBG_OPTION(_option_,_defval_) { RF_STRING(_option_), &rf_/**/_option_ },
#endif				/* __STDC__ */

RF_DebugName_t rf_debugNames[] = {
#include <dev/raidframe/rf_optnames.h>
	{NULL, NULL}
};
#undef RF_DBG_OPTION

#ifdef __STDC__
#define RF_DBG_OPTION(_option_,_defval_) rf_##_option_  = _defval_ ;
#else				/* __STDC__ */
#define RF_DBG_OPTION(_option_,_defval_) rf_/**/_option_ = _defval_ ;
#endif				/* __STDC__ */

void 
rf_ResetDebugOptions()
{
#include <dev/raidframe/rf_optnames.h>
}
