/*	$FreeBSD$ */
/*	$NetBSD: rf_engine.h,v 1.3 1999/02/05 00:06:11 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: William V. Courtright II, Mark Holland, Jim Zelenka
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

/**********************************************************
 *                                                        *
 * engine.h -- header file for execution engine functions *
 *                                                        *
 **********************************************************/

#ifndef _RF__RF_ENGINE_H_
#define _RF__RF_ENGINE_H_

int 
rf_ConfigureEngine(RF_ShutdownList_t ** listp,
    RF_Raid_t * raidPtr, RF_Config_t * cfgPtr);

int     rf_FinishNode(RF_DagNode_t * node, int context);	/* return finished node
								 * to engine */

int     rf_DispatchDAG(RF_DagHeader_t * dag, void (*cbFunc) (void *), void *cbArg);	/* execute dag */

#endif				/* !_RF__RF_ENGINE_H_ */
