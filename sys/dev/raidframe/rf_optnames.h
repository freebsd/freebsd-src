/*	$FreeBSD$ */
/*	$NetBSD: rf_optnames.h,v 1.6 1999/12/07 02:54:08 oster Exp $	*/
/*
 * rf_optnames.h
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

/*
 * Don't protect against multiple inclusion here- we actually want this.
 */

RF_DBG_OPTION(accessDebug, 0)
RF_DBG_OPTION(accessTraceBufSize, 0)
RF_DBG_OPTION(cscanDebug, 0)	/* debug CSCAN sorting */
RF_DBG_OPTION(dagDebug, 0)
RF_DBG_OPTION(debugPrintUseBuffer, 0)
RF_DBG_OPTION(degDagDebug, 0)
RF_DBG_OPTION(disableAsyncAccs, 0)
RF_DBG_OPTION(diskDebug, 0)
RF_DBG_OPTION(enableAtomicRMW, 0)	/* this debug var enables locking of
					 * the disk arm during small-write
					 * operations.  Setting this variable
					 * to anything other than 0 will
					 * result in deadlock.  (wvcii) */
RF_DBG_OPTION(engineDebug, 0)
RF_DBG_OPTION(fifoDebug, 0)	/* debug fifo queueing */
RF_DBG_OPTION(floatingRbufDebug, 0)
RF_DBG_OPTION(forceHeadSepLimit, -1)
RF_DBG_OPTION(forceNumFloatingReconBufs, -1)		/* wire down number of
							 * extra recon buffers
							 * to use */
RF_DBG_OPTION(keepAccTotals, 0)		/* turn on keep_acc_totals */
RF_DBG_OPTION(lockTableSize, RF_DEFAULT_LOCK_TABLE_SIZE)
RF_DBG_OPTION(mapDebug, 0)
RF_DBG_OPTION(maxNumTraces, -1)

RF_DBG_OPTION(memChunkDebug, 0)
RF_DBG_OPTION(memDebug, 0)
RF_DBG_OPTION(memDebugAddress, 0)
RF_DBG_OPTION(numBufsToAccumulate, 1)		/* number of buffers to
						 * accumulate before doing XOR */
RF_DBG_OPTION(prReconSched, 0)
RF_DBG_OPTION(printDAGsDebug, 0)
RF_DBG_OPTION(printStatesDebug, 0)
RF_DBG_OPTION(protectedSectors, 64L)		/* # of sectors at start of
						 * disk to exclude from RAID
						 * address space */
RF_DBG_OPTION(pssDebug, 0)
RF_DBG_OPTION(queueDebug, 0)
RF_DBG_OPTION(quiesceDebug, 0)
RF_DBG_OPTION(raidSectorOffset, 0)	/* added to all incoming sectors to
					 * debug alignment problems */
RF_DBG_OPTION(reconDebug, 0)
RF_DBG_OPTION(reconbufferDebug, 0)
RF_DBG_OPTION(scanDebug, 0)	/* debug SCAN sorting */
RF_DBG_OPTION(showXorCallCounts, 0)	/* show n-way Xor call counts */
RF_DBG_OPTION(shutdownDebug, 0)		/* show shutdown calls */
RF_DBG_OPTION(sizePercentage, 100)
RF_DBG_OPTION(sstfDebug, 0)	/* turn on debugging info for sstf queueing */
RF_DBG_OPTION(stripeLockDebug, 0)
RF_DBG_OPTION(suppressLocksAndLargeWrites, 0)
RF_DBG_OPTION(suppressTraceDelays, 0)
RF_DBG_OPTION(useMemChunks, 1)
RF_DBG_OPTION(validateDAGDebug, 0)
RF_DBG_OPTION(validateVisitedDebug, 1)		/* XXX turn to zero by
						 * default? */
RF_DBG_OPTION(verifyParityDebug, 0)
RF_DBG_OPTION(debugKernelAccess, 0)	/* DoAccessKernel debugging */

#if RF_INCLUDE_PARITYLOGGING > 0
RF_DBG_OPTION(forceParityLogReint, 0)
RF_DBG_OPTION(numParityRegions, 0)	/* number of regions in the array */
RF_DBG_OPTION(numReintegrationThreads, 1)
RF_DBG_OPTION(parityLogDebug, 0)	/* if nonzero, enables debugging of
					 * parity logging */
RF_DBG_OPTION(totalInCoreLogCapacity, 1024 * 1024)	/* target bytes
							 * available for in-core
							 * logs */
#endif				/* RF_INCLUDE_PARITYLOGGING > 0 */

