/*	$FreeBSD$ */
/*	$NetBSD: rf_dagflags.h,v 1.3 1999/02/05 00:06:08 oster Exp $	*/
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

/**************************************************************************************
 *
 * dagflags.h -- flags that can be given to DoAccess
 * I pulled these out of dag.h because routines that call DoAccess may need these flags,
 * but certainly do not need the declarations related to the DAG data structures.
 *
 **************************************************************************************/


#ifndef _RF__RF_DAGFLAGS_H_
#define _RF__RF_DAGFLAGS_H_

/*
 * Bitmasks for the "flags" parameter (RF_RaidAccessFlags_t) used
 * by DoAccess, SelectAlgorithm, and the DAG creation routines.
 *
 * If USE_DAG or USE_ASM is specified, neither the DAG nor the ASM
 * will be modified, which means that you can't SUPRESS if you
 * specify USE_DAG.
 */

#define RF_DAG_FLAGS_NONE             0	/* no flags */
#define RF_DAG_SUPPRESS_LOCKS     (1<<0)	/* supress all stripe locks in
						 * the DAG */
#define RF_DAG_RETURN_ASM         (1<<1)	/* create an ASM and return it
						 * instead of freeing it */
#define RF_DAG_RETURN_DAG         (1<<2)	/* create a DAG and return it
						 * instead of freeing it */
#define RF_DAG_NONBLOCKING_IO     (1<<3)	/* cause DoAccess to be
						 * non-blocking */
#define RF_DAG_ACCESS_COMPLETE    (1<<4)	/* the access is complete */
#define RF_DAG_DISPATCH_RETURNED  (1<<5)	/* used to handle the case
						 * where the dag invokes no
						 * I/O */
#define RF_DAG_TEST_ACCESS        (1<<6)	/* this access came through
						 * rf_ioctl instead of
						 * rf_strategy */

#endif				/* !_RF__RF_DAGFLAGS_H_ */
