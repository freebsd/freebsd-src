/*	$FreeBSD$ */
/*	$NetBSD: rf_desc.h,v 1.5 2000/01/09 00:00:18 oster Exp $	*/
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

#ifndef _RF__RF_DESC_H_
#define _RF__RF_DESC_H_

#include <dev/raidframe/rf_archs.h>
#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_etimer.h>
#include <dev/raidframe/rf_dag.h>

struct RF_RaidReconDesc_s {
	RF_Raid_t *raidPtr;	/* raid device descriptor */
	RF_RowCol_t row;	/* row of failed disk */
	RF_RowCol_t col;	/* col of failed disk */
	int     state;		/* how far along the reconstruction operation
				 * has gotten */
	RF_RaidDisk_t *spareDiskPtr;	/* describes target disk for recon
					 * (not used in dist sparing) */
	int     numDisksDone;	/* the number of surviving disks that have
				 * completed their work */
	RF_RowCol_t srow;	/* row ID of the spare disk (not used in dist
				 * sparing) */
	RF_RowCol_t scol;	/* col ID of the spare disk (not used in dist
				 * sparing) */
	/*
         * Prevent recon from hogging CPU
         */
	RF_Etimer_t recon_exec_timer;
	RF_uint64 reconExecTimerRunning;
	RF_uint64 reconExecTicks;
	RF_uint64 maxReconExecTicks;

#if RF_RECON_STATS > 0
	RF_uint64 hsStallCount;	/* head sep stall count */
	RF_uint64 numReconExecDelays;
	RF_uint64 numReconEventWaits;
#endif				/* RF_RECON_STATS > 0 */
	RF_RaidReconDesc_t *next;
};

struct RF_RaidAccessDesc_s {
	RF_Raid_t *raidPtr;	/* raid device descriptor */
	RF_IoType_t type;	/* read or write */
	RF_RaidAddr_t raidAddress;	/* starting address in raid address
					 * space */
	RF_SectorCount_t numBlocks;	/* number of blocks (sectors) to
					 * transfer */
	RF_StripeCount_t numStripes;	/* number of stripes involved in
					 * access */
	caddr_t bufPtr;		/* pointer to data buffer */
	RF_RaidAccessFlags_t flags;	/* flags controlling operation */
	int     state;		/* index into states telling how far along the
				 * RAID operation has gotten */
	RF_AccessState_t *states;	/* array of states to be run */
	int     status;		/* pass/fail status of the last operation */
	RF_DagList_t *dagArray;	/* array of dag lists, one list per stripe */
	RF_AccessStripeMapHeader_t *asmap;	/* the asm for this I/O */
	void   *bp;		/* buf pointer for this RAID acc.  ignored
				 * outside the kernel */
	RF_DagHeader_t **paramDAG;	/* allows the DAG to be returned to
					 * the caller after I/O completion */
	RF_AccessStripeMapHeader_t **paramASM;	/* allows the ASM to be
						 * returned to the caller
						 * after I/O completion */
	RF_AccTraceEntry_t tracerec;	/* perf monitoring information for a
					 * user access (not for dag stats) */
	void    (*callbackFunc) (RF_CBParam_t);	/* callback function for this
						 * I/O */
	void   *callbackArg;	/* arg to give to callback func */

	RF_AllocListElem_t *cleanupList;	/* memory to be freed at the
						 * end of the access */

	RF_RaidAccessDesc_t *next;
	RF_RaidAccessDesc_t *head;

	int     numPending;

	        RF_DECLARE_MUTEX(mutex)	/* these are used to implement
					 * blocking I/O */
	        RF_DECLARE_COND(cond)
	int     async_flag;

	RF_Etimer_t timer;	/* used for timing this access */
};
#endif				/* !_RF__RF_DESC_H_ */
