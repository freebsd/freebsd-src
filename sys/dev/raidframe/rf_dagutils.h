/*	$FreeBSD$ */
/*	$NetBSD: rf_dagutils.h,v 1.3 1999/02/05 00:06:08 oster Exp $	*/
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

/*************************************************************************
 *
 * rf_dagutils.h -- header file for utility routines for manipulating DAGs
 *
 *************************************************************************/


#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_dagfuncs.h>
#include <dev/raidframe/rf_general.h>

#ifndef _RF__RF_DAGUTILS_H_
#define _RF__RF_DAGUTILS_H_

struct RF_RedFuncs_s {
	int     (*regular) (RF_DagNode_t *);
	char   *RegularName;
	int     (*simple) (RF_DagNode_t *);
	char   *SimpleName;
};

extern RF_RedFuncs_t rf_xorFuncs;
extern RF_RedFuncs_t rf_xorRecoveryFuncs;

void 
rf_InitNode(RF_DagNode_t * node, RF_NodeStatus_t initstatus,
    int commit,
    int (*doFunc) (RF_DagNode_t * node),
    int (*undoFunc) (RF_DagNode_t * node),
    int (*wakeFunc) (RF_DagNode_t * node, int status),
    int nSucc, int nAnte, int nParam, int nResult,
    RF_DagHeader_t * hdr, char *name, RF_AllocListElem_t * alist);

	void    rf_FreeDAG(RF_DagHeader_t * dag_h);

	RF_PropHeader_t *rf_MakePropListEntry(RF_DagHeader_t * dag_h, int resultNum,
            int paramNum, RF_PropHeader_t * next, RF_AllocListElem_t * allocList);

	int     rf_ConfigureDAGs(RF_ShutdownList_t ** listp);

	RF_DagHeader_t *rf_AllocDAGHeader(void);

	void    rf_FreeDAGHeader(RF_DagHeader_t * dh);

	void   *rf_AllocBuffer(RF_Raid_t * raidPtr, RF_DagHeader_t * dag_h,
            RF_PhysDiskAddr_t * pda, RF_AllocListElem_t * allocList);

	char   *rf_NodeStatusString(RF_DagNode_t * node);

	void    rf_PrintNodeInfoString(RF_DagNode_t * node);

	int     rf_AssignNodeNums(RF_DagHeader_t * dag_h);

	int     rf_RecurAssignNodeNums(RF_DagNode_t * node, int num, int unvisited);

	void    rf_ResetDAGHeaderPointers(RF_DagHeader_t * dag_h, RF_DagHeader_t * newptr);

	void    rf_RecurResetDAGHeaderPointers(RF_DagNode_t * node, RF_DagHeader_t * newptr);

	void    rf_PrintDAGList(RF_DagHeader_t * dag_h);

	int     rf_ValidateDAG(RF_DagHeader_t * dag_h);

	void    rf_redirect_asm(RF_Raid_t * raidPtr, RF_AccessStripeMap_t * asmap);

	void    rf_MapUnaccessedPortionOfStripe(RF_Raid_t * raidPtr,
            RF_RaidLayout_t * layoutPtr,
            RF_AccessStripeMap_t * asmap, RF_DagHeader_t * dag_h,
            RF_AccessStripeMapHeader_t ** new_asm_h, int *nRodNodes, char **sosBuffer,
            char **eosBuffer, RF_AllocListElem_t * allocList);

	int     rf_PDAOverlap(RF_RaidLayout_t * layoutPtr, RF_PhysDiskAddr_t * src,
            RF_PhysDiskAddr_t * dest);

	void    rf_GenerateFailedAccessASMs(RF_Raid_t * raidPtr,
            RF_AccessStripeMap_t * asmap, RF_PhysDiskAddr_t * failedPDA,
            RF_DagHeader_t * dag_h, RF_AccessStripeMapHeader_t ** new_asm_h,
            int *nXorBufs, char **rpBufPtr, char *overlappingPDAs,
            RF_AllocListElem_t * allocList);

/* flags used by RangeRestrictPDA */
#define RF_RESTRICT_NOBUFFER 0
#define RF_RESTRICT_DOBUFFER 1

	void    rf_RangeRestrictPDA(RF_Raid_t * raidPtr, RF_PhysDiskAddr_t * src,
            RF_PhysDiskAddr_t * dest, int dobuffer, int doraidaddr);

	int     rf_compute_workload_shift(RF_Raid_t * raidPtr, RF_PhysDiskAddr_t * pda);
	void    rf_SelectMirrorDiskIdle(RF_DagNode_t * node);
	void    rf_SelectMirrorDiskPartition(RF_DagNode_t * node);

#endif				/* !_RF__RF_DAGUTILS_H_ */
