/*	$FreeBSD$ */
/*	$NetBSD: rf_evenodd_dags.h,v 1.2 1999/02/05 00:06:11 oster Exp $	*/
/*
 * rf_evenodd_dags.h
 */
/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chang-Ming Wu
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

#ifndef _RF__RF_EVENODD_DAGS_H_
#define _RF__RF_EVENODD_DAGS_H_

#include <dev/raidframe/rf_types.h>

#if RF_UTILITY == 0
#include <dev/raidframe/rf_dag.h>

/* extern decl's of the failure mode EO functions.
 * swiped from rf_pqdeg.h
 */

RF_CREATE_DAG_FUNC_DECL(rf_EO_100_CreateReadDAG);
RF_CREATE_DAG_FUNC_DECL(rf_EO_101_CreateReadDAG);
RF_CREATE_DAG_FUNC_DECL(rf_EO_110_CreateReadDAG);
RF_CREATE_DAG_FUNC_DECL(rf_EO_200_CreateReadDAG);
RF_CREATE_DAG_FUNC_DECL(rf_EOCreateDoubleDegradedReadDAG);
RF_CREATE_DAG_FUNC_DECL(rf_EO_100_CreateWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_EO_010_CreateSmallWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_EO_001_CreateSmallWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_EO_010_CreateLargeWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_EO_001_CreateLargeWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_EO_011_CreateWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_EO_110_CreateWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_EO_101_CreateWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_EO_DoubleDegRead);
RF_CREATE_DAG_FUNC_DECL(rf_EOCreateSmallWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_EOCreateLargeWriteDAG);
RF_CREATE_DAG_FUNC_DECL(rf_EO_200_CreateWriteDAG);
#endif				/* RF_UTILITY == 0 */

#endif				/* !_RF__RF_EVENODD_DAGS_H_ */
