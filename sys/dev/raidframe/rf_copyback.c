/*	$NetBSD: rf_copyback.c,v 1.15 2001/01/26 02:16:24 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
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

/*****************************************************************************************
 *
 * copyback.c -- code to copy reconstructed data back from spare space to
 *               the replaced disk.
 *
 * the code operates using callbacks on the I/Os to continue with the next
 * unit to be copied back.  We do this because a simple loop containing blocking I/Os
 * will not work in the simulator.
 *
 ****************************************************************************************/

#include <dev/raidframe/rf_types.h>

#if defined(__FreeBSD__)
#include <sys/types.h>
#include <sys/systm.h>
#if __FreeBSD_version > 500005
#include <sys/bio.h>
#endif
#endif

#include <sys/time.h>
#include <sys/buf.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_mcpair.h>
#include <dev/raidframe/rf_acctrace.h>
#include <dev/raidframe/rf_etimer.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_utils.h>
#include <dev/raidframe/rf_copyback.h>
#include <dev/raidframe/rf_decluster.h>
#include <dev/raidframe/rf_driver.h>
#include <dev/raidframe/rf_shutdown.h>
#include <dev/raidframe/rf_kintf.h>

#define RF_COPYBACK_DATA   0
#define RF_COPYBACK_PARITY 1

int     rf_copyback_in_progress;

static int rf_CopybackReadDoneProc(RF_CopybackDesc_t * desc, int status);
static int rf_CopybackWriteDoneProc(RF_CopybackDesc_t * desc, int status);
static void rf_CopybackOne(RF_CopybackDesc_t * desc, int typ,
			   RF_RaidAddr_t addr, RF_RowCol_t testRow, 
			   RF_RowCol_t testCol,
			   RF_SectorNum_t testOffs);
static void rf_CopybackComplete(RF_CopybackDesc_t * desc, int status);

int 
rf_ConfigureCopyback(listp)
	RF_ShutdownList_t **listp;
{
	rf_copyback_in_progress = 0;
	return (0);
}
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#if defined(__NetBSD__)
#include <sys/ioctl.h>
#elif defined(__FreeBSD__)
#include <sys/ioccom.h>
#include <sys/filio.h>
#endif
#include <sys/fcntl.h>
#include <sys/vnode.h>

/* do a complete copyback */
void 
rf_CopybackReconstructedData(raidPtr)
	RF_Raid_t *raidPtr;
{
	RF_ComponentLabel_t *c_label;
	int     done, retcode;
	RF_CopybackDesc_t *desc;
	RF_RowCol_t frow, fcol;
	RF_RaidDisk_t *badDisk;
	struct vnode *vp;
	char   *databuf;
	int ac;

	RF_Malloc(c_label, sizeof(RF_ComponentLabel_t), (RF_ComponentLabel_t *));
	if (c_label == NULL) {
		printf("rf_CopybackReconstructedData: Out of memory?\n");
		return;
	}

	done = 0;
	fcol = 0;
	for (frow = 0; frow < raidPtr->numRow; frow++) {
		for (fcol = 0; fcol < raidPtr->numCol; fcol++) {
			if (raidPtr->Disks[frow][fcol].status == rf_ds_dist_spared
			    || raidPtr->Disks[frow][fcol].status == rf_ds_spared) {
				done = 1;
				break;
			}
		}
		if (done)
			break;
	}

	if (frow == raidPtr->numRow) {
		printf("COPYBACK:  no disks need copyback\n");
		return;
	}
	badDisk = &raidPtr->Disks[frow][fcol];

	/* This device may have been opened successfully the first time. Close
	 * it before trying to open it again.. */

	if (raidPtr->raid_cinfo[frow][fcol].ci_vp != NULL) {
		printf("Closed the open device: %s\n",
		    raidPtr->Disks[frow][fcol].devname);
		vp = raidPtr->raid_cinfo[frow][fcol].ci_vp;
		ac = raidPtr->Disks[frow][fcol].auto_configured;
		rf_close_component(raidPtr, vp, ac);
		raidPtr->raid_cinfo[frow][fcol].ci_vp = NULL;

	}
	/* note that this disk was *not* auto_configured (any longer) */
	raidPtr->Disks[frow][fcol].auto_configured = 0;

	printf("About to (re-)open the device: %s\n",
	    raidPtr->Disks[frow][fcol].devname);

	retcode = raid_getcomponentsize(raidPtr, frow, fcol);

	if (retcode) {
		printf("COPYBACK: raidlookup on device: %s failed: %d!\n",
		    raidPtr->Disks[frow][fcol].devname, retcode);

		/* XXX the component isn't responding properly... must be
		 * still dead :-( */
		return;

	}
#if 0
	/* This is the way it was done before the CAM stuff was removed */

	if (rf_extract_ids(badDisk->devname, &bus, &targ, &lun)) {
		printf("COPYBACK: unable to extract bus, target, lun from devname %s\n",
		    badDisk->devname);
		return;
	}
	/* TUR the disk that's marked as bad to be sure that it's actually
	 * alive */
	rf_SCSI_AllocTUR(&tur_op);
	retcode = rf_SCSI_DoTUR(tur_op, bus, targ, lun, badDisk->dev);
	rf_SCSI_FreeDiskOp(tur_op, 0);
#endif

	if (retcode) {
		printf("COPYBACK: target disk failed TUR\n");
		return;
	}
	/* get a buffer to hold one SU  */
	RF_Malloc(databuf, rf_RaidAddressToByte(raidPtr, raidPtr->Layout.sectorsPerStripeUnit), (char *));

	/* create a descriptor */
	RF_Malloc(desc, sizeof(*desc), (RF_CopybackDesc_t *));
	desc->raidPtr = raidPtr;
	desc->status = 0;
	desc->frow = frow;
	desc->fcol = fcol;
	desc->spRow = badDisk->spareRow;
	desc->spCol = badDisk->spareCol;
	desc->stripeAddr = 0;
	desc->sectPerSU = raidPtr->Layout.sectorsPerStripeUnit;
	desc->sectPerStripe = raidPtr->Layout.sectorsPerStripeUnit * raidPtr->Layout.numDataCol;
	desc->databuf = databuf;
	desc->mcpair = rf_AllocMCPair();

	printf("COPYBACK: Quiescing the array\n");
	/* quiesce the array, since we don't want to code support for user
	 * accs here */
	rf_SuspendNewRequestsAndWait(raidPtr);

	/* adjust state of the array and of the disks */
	RF_LOCK_MUTEX(raidPtr->mutex);
	raidPtr->Disks[desc->frow][desc->fcol].status = rf_ds_optimal;
	raidPtr->status[desc->frow] = rf_rs_optimal;
	rf_copyback_in_progress = 1;	/* debug only */
	RF_UNLOCK_MUTEX(raidPtr->mutex);

	printf("COPYBACK: Beginning\n");
	RF_GETTIME(desc->starttime);
	rf_ContinueCopyback(desc);

	/* Data has been restored.  Fix up the component label. */
	/* Don't actually need the read here.. */
	raidread_component_label( raidPtr->raid_cinfo[frow][fcol].ci_dev,
				  raidPtr->raid_cinfo[frow][fcol].ci_vp,
				  c_label);
	
	raid_init_component_label( raidPtr, c_label );

	c_label->row = frow;
	c_label->column = fcol;
	c_label->partitionSize = raidPtr->Disks[frow][fcol].partitionSize;

	raidwrite_component_label( raidPtr->raid_cinfo[frow][fcol].ci_dev,
				   raidPtr->raid_cinfo[frow][fcol].ci_vp,
				   c_label);
	RF_Free(c_label, sizeof(RF_ComponentLabel_t));
}


/*
 * invoked via callback after a copyback I/O has completed to
 * continue on with the next one
 */
void 
rf_ContinueCopyback(desc)
	RF_CopybackDesc_t *desc;
{
	RF_SectorNum_t testOffs, stripeAddr;
	RF_Raid_t *raidPtr = desc->raidPtr;
	RF_RaidAddr_t addr;
	RF_RowCol_t testRow, testCol;
	int     old_pctg, new_pctg, done;
	struct timeval t, diff;

	old_pctg = (-1);
	while (1) {
		stripeAddr = desc->stripeAddr;
		desc->raidPtr->copyback_stripes_done = stripeAddr
			/ desc->sectPerStripe;
		if (rf_prReconSched) {
			old_pctg = 100 * desc->stripeAddr / raidPtr->totalSectors;
		}
		desc->stripeAddr += desc->sectPerStripe;
		if (rf_prReconSched) {
			new_pctg = 100 * desc->stripeAddr / raidPtr->totalSectors;
			if (new_pctg != old_pctg) {
				RF_GETTIME(t);
				RF_TIMEVAL_DIFF(&desc->starttime, &t, &diff);
				printf("%d %d.%06d\n", new_pctg, (int) diff.tv_sec, (int) diff.tv_usec);
			}
		}
		if (stripeAddr >= raidPtr->totalSectors) {
			rf_CopybackComplete(desc, 0);
			return;
		}
		/* walk through the current stripe, su-by-su */
		for (done = 0, addr = stripeAddr; addr < stripeAddr + desc->sectPerStripe; addr += desc->sectPerSU) {

			/* map the SU, disallowing remap to spare space */
			(raidPtr->Layout.map->MapSector) (raidPtr, addr, &testRow, &testCol, &testOffs, RF_DONT_REMAP);

			if (testRow == desc->frow && testCol == desc->fcol) {
				rf_CopybackOne(desc, RF_COPYBACK_DATA, addr, testRow, testCol, testOffs);
				done = 1;
				break;
			}
		}

		if (!done) {
			/* we didn't find the failed disk in the data part.
			 * check parity. */

			/* map the parity for this stripe, disallowing remap
			 * to spare space */
			(raidPtr->Layout.map->MapParity) (raidPtr, stripeAddr, &testRow, &testCol, &testOffs, RF_DONT_REMAP);

			if (testRow == desc->frow && testCol == desc->fcol) {
				rf_CopybackOne(desc, RF_COPYBACK_PARITY, stripeAddr, testRow, testCol, testOffs);
			}
		}
		/* check to see if the last read/write pair failed */
		if (desc->status) {
			rf_CopybackComplete(desc, 1);
			return;
		}
		/* we didn't find any units to copy back in this stripe.
		 * Continue with the next one */
	}
}


/* copyback one unit */
static void 
rf_CopybackOne(desc, typ, addr, testRow, testCol, testOffs)
	RF_CopybackDesc_t *desc;
	int     typ;
	RF_RaidAddr_t addr;
	RF_RowCol_t testRow;
	RF_RowCol_t testCol;
	RF_SectorNum_t testOffs;
{
	RF_SectorCount_t sectPerSU = desc->sectPerSU;
	RF_Raid_t *raidPtr = desc->raidPtr;
	RF_RowCol_t spRow = desc->spRow;
	RF_RowCol_t spCol = desc->spCol;
	RF_SectorNum_t spOffs;

	/* find the spare spare location for this SU */
	if (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) {
		if (typ == RF_COPYBACK_DATA)
			raidPtr->Layout.map->MapSector(raidPtr, addr, &spRow, &spCol, &spOffs, RF_REMAP);
		else
			raidPtr->Layout.map->MapParity(raidPtr, addr, &spRow, &spCol, &spOffs, RF_REMAP);
	} else {
		spOffs = testOffs;
	}

	/* create reqs to read the old location & write the new */
	desc->readreq = rf_CreateDiskQueueData(RF_IO_TYPE_READ, spOffs,
	    sectPerSU, desc->databuf, 0L, 0,
	    (int (*) (void *, int)) rf_CopybackReadDoneProc, desc,
	    NULL, NULL, (void *) raidPtr, RF_DISKQUEUE_DATA_FLAGS_NONE, NULL);
	desc->writereq = rf_CreateDiskQueueData(RF_IO_TYPE_WRITE, testOffs,
	    sectPerSU, desc->databuf, 0L, 0,
	    (int (*) (void *, int)) rf_CopybackWriteDoneProc, desc,
	    NULL, NULL, (void *) raidPtr, RF_DISKQUEUE_DATA_FLAGS_NONE, NULL);
	desc->frow = testRow;
	desc->fcol = testCol;

	/* enqueue the read.  the write will go out as part of the callback on
	 * the read. at user-level & in the kernel, wait for the read-write
	 * pair to complete. in the simulator, just return, since everything
	 * will happen as callbacks */

	RF_LOCK_MUTEX(desc->mcpair->mutex);
	desc->mcpair->flag = 0;

	rf_DiskIOEnqueue(&raidPtr->Queues[spRow][spCol], desc->readreq, RF_IO_NORMAL_PRIORITY);

	while (!desc->mcpair->flag) {
		RF_WAIT_MCPAIR(desc->mcpair);
	}
	RF_UNLOCK_MUTEX(desc->mcpair->mutex);
	rf_FreeDiskQueueData(desc->readreq);
	rf_FreeDiskQueueData(desc->writereq);

}


/* called at interrupt context when the read has completed.  just send out the write */
static int 
rf_CopybackReadDoneProc(desc, status)
	RF_CopybackDesc_t *desc;
	int     status;
{
	if (status) {		/* invoke the callback with bad status */
		printf("COPYBACK: copyback read failed.  Aborting.\n");
		(desc->writereq->CompleteFunc) (desc, -100);
	} else {
		rf_DiskIOEnqueue(&(desc->raidPtr->Queues[desc->frow][desc->fcol]), desc->writereq, RF_IO_NORMAL_PRIORITY);
	}
	return (0);
}
/* called at interrupt context when the write has completed.
 * at user level & in the kernel, wake up the copyback thread.
 * in the simulator, invoke the next copyback directly.
 * can't free diskqueuedata structs in the kernel b/c we're at interrupt context.
 */
static int 
rf_CopybackWriteDoneProc(desc, status)
	RF_CopybackDesc_t *desc;
	int     status;
{
	if (status && status != -100) {
		printf("COPYBACK: copyback write failed.  Aborting.\n");
	}
	desc->status = status;
	rf_MCPairWakeupFunc(desc->mcpair);
	return (0);
}
/* invoked when the copyback has completed */
static void 
rf_CopybackComplete(desc, status)
	RF_CopybackDesc_t *desc;
	int     status;
{
	RF_Raid_t *raidPtr = desc->raidPtr;
	struct timeval t, diff;

	if (!status) {
		RF_LOCK_MUTEX(raidPtr->mutex);
		if (raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE) {
			RF_ASSERT(raidPtr->Layout.map->parityConfig == 'D');
			rf_FreeSpareTable(raidPtr);
		} else {
			raidPtr->Disks[desc->spRow][desc->spCol].status = rf_ds_spare;
		}
		RF_UNLOCK_MUTEX(raidPtr->mutex);

		RF_GETTIME(t);
		RF_TIMEVAL_DIFF(&desc->starttime, &t, &diff);
		printf("Copyback time was %d.%06d seconds\n",
		    (int) diff.tv_sec, (int) diff.tv_usec);
	} else
		printf("COPYBACK: Failure.\n");

	RF_Free(desc->databuf, rf_RaidAddressToByte(raidPtr, desc->sectPerSU));
	rf_FreeMCPair(desc->mcpair);
	RF_Free(desc, sizeof(*desc));

	rf_copyback_in_progress = 0;
	rf_ResumeNewRequests(raidPtr);
}
