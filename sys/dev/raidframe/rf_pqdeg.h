/*	$FreeBSD$ */
/*	$NetBSD: rf_pqdeg.h,v 1.3 1999/02/05 00:06:15 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Daniel Stodolsky
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

#ifndef _RF__RF_PQDEG_H_
#define _RF__RF_PQDEG_H_

#include <dev/raidframe/rf_types.h>

#if RF_UTILITY == 0
#include <dev/raidframe/rf_dag.h>

/* extern decl's of the failure mode PQ functions.
 * See pddeg.c for nomenclature discussion.
 */

/* reads, single failure  */
RF_CREATE_DAG_FUNC_DECL(rf_PQ_100_CreateReadDAG);
/* reads, two failure */
RF_CREATE_DAG_FUNC_DECL(rf_PQ_110_CreateReadDAG);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_101_CreateReadDAG);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_200_CreateReadDAG);

/* writes, single failure */
RF_CREATE_DAG_FUNC_DECL(rf_PQ_100_CreateWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_010_CreateSmallWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_010_CreateLargeWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_001_CreateSmallWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_001_CreateLargeWriteDAG);

/* writes, double failure */
RF_CREATE_DAG_FUNC_DECL(rf_PQ_011_CreateWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_110_CreateWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_101_CreateWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_PQ_200_CreateWriteDAG);
#endif				/* RF_UTILITY == 0 */

typedef RF_uint32 RF_ua32_t[32];
typedef RF_uint8 RF_ua1024_t[1024];

extern RF_ua32_t rf_rn;
extern RF_ua32_t rf_qfor[32];
#ifndef _KERNEL			/* we don't support PQ in the kernel yet, so
				 * don't link in this monster table */
extern RF_ua1024_t rf_qinv[29 * 29];
#else				/* !_KERNEL */
extern RF_ua1024_t rf_qinv[1];
#endif				/* !_KERNEL */

#endif				/* !_RF__RF_PQDEG_H_ */
