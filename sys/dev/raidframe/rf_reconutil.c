/*	$NetBSD: rf_reconutil.c,v 1.3 1999/02/05 00:06:17 oster Exp $	*/

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

/********************************************
 * rf_reconutil.c -- reconstruction utilities
 ********************************************/

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_desc.h>
#include <dev/raidframe/rf_reconutil.h>
#include <dev/raidframe/rf_reconbuffer.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_decluster.h>
#include <dev/raidframe/rf_raid5_rotatedspare.h>
#include <dev/raidframe/rf_interdecluster.h>
#include <dev/raidframe/rf_chaindecluster.h>

/*******************************************************************
 * allocates/frees the reconstruction control information structures
 *******************************************************************/
RF_ReconCtrl_t *
rf_MakeReconControl(reconDesc, frow, fcol, srow, scol)
	RF_RaidReconDesc_t *reconDesc;
	RF_RowCol_t frow;	/* failed row and column */
	RF_RowCol_t fcol;
	RF_RowCol_t srow;	/* identifies which spare we're using */
	RF_RowCol_t scol;
{
	RF_Raid_t *raidPtr = reconDesc->raidPtr;
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_ReconUnitCount_t RUsPerPU = layoutPtr->SUsPerPU / layoutPtr->SUsPerRU;
	RF_ReconUnitCount_t numSpareRUs;
	RF_ReconCtrl_t *reconCtrlPtr;
	RF_ReconBuffer_t *rbuf;
	RF_LayoutSW_t *lp;
	int     retcode, rc;
	RF_RowCol_t i;

	lp = raidPtr->Layout.map;

	/* make and zero the global reconstruction structure and the per-disk
	 * structure */
	RF_Calloc(reconCtrlPtr, 1, sizeof(RF_ReconCtrl_t), (RF_ReconCtrl_t *));
	RF_Calloc(reconCtrlPtr->perDiskInfo, raidPtr->numCol, sizeof(RF_PerDiskReconCtrl_t), (RF_PerDiskReconCtrl_t *));	/* this zeros it */
	reconCtrlPtr->reconDesc = reconDesc;
	reconCtrlPtr->fcol = fcol;
	reconCtrlPtr->spareRow = srow;
	reconCtrlPtr->spareCol = scol;
	reconCtrlPtr->lastPSID = layoutPtr->numStripe / layoutPtr->SUsPerPU;
	reconCtrlPtr->percentComplete = 0;

	/* initialize each per-disk recon information structure */
	for (i = 0; i < raidPtr->numCol; i++) {
		reconCtrlPtr->perDiskInfo[i].reconCtrl = reconCtrlPtr;
		reconCtrlPtr->perDiskInfo[i].row = frow;
		reconCtrlPtr->perDiskInfo[i].col = i;
		reconCtrlPtr->perDiskInfo[i].curPSID = -1;	/* make it appear as if
								 * we just finished an
								 * RU */
		reconCtrlPtr->perDiskInfo[i].ru_count = RUsPerPU - 1;
	}

	/* Get the number of spare units per disk and the sparemap in case
	 * spare is distributed  */

	if (lp->GetNumSpareRUs) {
		numSpareRUs = lp->GetNumSpareRUs(raidPtr);
	} else {
		numSpareRUs = 0;
	}

	/*
         * Not all distributed sparing archs need dynamic mappings
         */
	if (lp->InstallSpareTable) {
		retcode = rf_InstallSpareTable(raidPtr, frow, fcol);
		if (retcode) {
			RF_PANIC();	/* XXX fix this */
		}
	}
	/* make the reconstruction map */
	reconCtrlPtr->reconMap = rf_MakeReconMap(raidPtr, (int) (layoutPtr->SUsPerRU * layoutPtr->sectorsPerStripeUnit),
	    raidPtr->sectorsPerDisk, numSpareRUs);

	/* make the per-disk reconstruction buffers */
	for (i = 0; i < raidPtr->numCol; i++) {
		reconCtrlPtr->perDiskInfo[i].rbuf = (i == fcol) ? NULL : rf_MakeReconBuffer(raidPtr, frow, i, RF_RBUF_TYPE_EXCLUSIVE);
	}

	/* initialize the event queue */
	rc = rf_mutex_init(&reconCtrlPtr->eq_mutex, __FUNCTION__);
	if (rc) {
		/* XXX deallocate, cleanup */
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		return (NULL);
	}
	rc = rf_cond_init(&reconCtrlPtr->eq_cond);
	if (rc) {
		/* XXX deallocate, cleanup */
		RF_ERRORMSG3("Unable to init cond file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		return (NULL);
	}
	reconCtrlPtr->eventQueue = NULL;
	reconCtrlPtr->eq_count = 0;

	/* make the floating recon buffers and append them to the free list */
	rc = rf_mutex_init(&reconCtrlPtr->rb_mutex, __FUNCTION__);
	if (rc) {
		/* XXX deallocate, cleanup */
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		return (NULL);
	}
	reconCtrlPtr->fullBufferList = NULL;
	reconCtrlPtr->priorityList = NULL;
	reconCtrlPtr->floatingRbufs = NULL;
	reconCtrlPtr->committedRbufs = NULL;
	for (i = 0; i < raidPtr->numFloatingReconBufs; i++) {
		rbuf = rf_MakeReconBuffer(raidPtr, frow, fcol, RF_RBUF_TYPE_FLOATING);
		rbuf->next = reconCtrlPtr->floatingRbufs;
		reconCtrlPtr->floatingRbufs = rbuf;
	}

	/* create the parity stripe status table */
	reconCtrlPtr->pssTable = rf_MakeParityStripeStatusTable(raidPtr);

	/* set the initial min head sep counter val */
	reconCtrlPtr->minHeadSepCounter = 0;

	return (reconCtrlPtr);
}

void 
rf_FreeReconControl(raidPtr, row)
	RF_Raid_t *raidPtr;
	RF_RowCol_t row;
{
	RF_ReconCtrl_t *reconCtrlPtr = raidPtr->reconControl[row];
	RF_ReconBuffer_t *t;
	RF_ReconUnitNum_t i;

	RF_ASSERT(reconCtrlPtr);
	for (i = 0; i < raidPtr->numCol; i++)
		if (reconCtrlPtr->perDiskInfo[i].rbuf)
			rf_FreeReconBuffer(reconCtrlPtr->perDiskInfo[i].rbuf);
	for (i = 0; i < raidPtr->numFloatingReconBufs; i++) {
		t = reconCtrlPtr->floatingRbufs;
		RF_ASSERT(t);
		reconCtrlPtr->floatingRbufs = t->next;
		rf_FreeReconBuffer(t);
	}
	rf_mutex_destroy(&reconCtrlPtr->rb_mutex);
	rf_mutex_destroy(&reconCtrlPtr->eq_mutex);
	rf_cond_destroy(&reconCtrlPtr->eq_cond);
	rf_FreeReconMap(reconCtrlPtr->reconMap);
	rf_FreeParityStripeStatusTable(raidPtr, reconCtrlPtr->pssTable);
	RF_Free(reconCtrlPtr->perDiskInfo, raidPtr->numCol * sizeof(RF_PerDiskReconCtrl_t));
	RF_Free(reconCtrlPtr, sizeof(*reconCtrlPtr));
}


/******************************************************************************
 * computes the default head separation limit
 *****************************************************************************/
RF_HeadSepLimit_t 
rf_GetDefaultHeadSepLimit(raidPtr)
	RF_Raid_t *raidPtr;
{
	RF_HeadSepLimit_t hsl;
	RF_LayoutSW_t *lp;

	lp = raidPtr->Layout.map;
	if (lp->GetDefaultHeadSepLimit == NULL)
		return (-1);
	hsl = lp->GetDefaultHeadSepLimit(raidPtr);
	return (hsl);
}


/******************************************************************************
 * computes the default number of floating recon buffers
 *****************************************************************************/
int 
rf_GetDefaultNumFloatingReconBuffers(raidPtr)
	RF_Raid_t *raidPtr;
{
	RF_LayoutSW_t *lp;
	int     nrb;

	lp = raidPtr->Layout.map;
	if (lp->GetDefaultNumFloatingReconBuffers == NULL)
		return (3 * raidPtr->numCol);
	nrb = lp->GetDefaultNumFloatingReconBuffers(raidPtr);
	return (nrb);
}


/******************************************************************************
 * creates and initializes a reconstruction buffer
 *****************************************************************************/
RF_ReconBuffer_t *
rf_MakeReconBuffer(
    RF_Raid_t * raidPtr,
    RF_RowCol_t row,
    RF_RowCol_t col,
    RF_RbufType_t type)
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_ReconBuffer_t *t;
	u_int   recon_buffer_size = rf_RaidAddressToByte(raidPtr, layoutPtr->SUsPerRU * layoutPtr->sectorsPerStripeUnit);

	RF_Malloc(t, sizeof(RF_ReconBuffer_t), (RF_ReconBuffer_t *));
	RF_Malloc(t->buffer, recon_buffer_size, (caddr_t));
	RF_Malloc(t->arrived, raidPtr->numCol * sizeof(char), (char *));
	t->raidPtr = raidPtr;
	t->row = row;
	t->col = col;
	t->priority = RF_IO_RECON_PRIORITY;
	t->type = type;
	t->pssPtr = NULL;
	t->next = NULL;
	return (t);
}
/******************************************************************************
 * frees a reconstruction buffer
 *****************************************************************************/
void 
rf_FreeReconBuffer(rbuf)
	RF_ReconBuffer_t *rbuf;
{
	RF_Raid_t *raidPtr = rbuf->raidPtr;
	u_int   recon_buffer_size = rf_RaidAddressToByte(raidPtr, raidPtr->Layout.SUsPerRU * raidPtr->Layout.sectorsPerStripeUnit);

	RF_Free(rbuf->arrived, raidPtr->numCol * sizeof(char));
	RF_Free(rbuf->buffer, recon_buffer_size);
	RF_Free(rbuf, sizeof(*rbuf));
}


/******************************************************************************
 * debug only:  sanity check the number of floating recon bufs in use
 *****************************************************************************/
void 
rf_CheckFloatingRbufCount(raidPtr, dolock)
	RF_Raid_t *raidPtr;
	int     dolock;
{
	RF_ReconParityStripeStatus_t *p;
	RF_PSStatusHeader_t *pssTable;
	RF_ReconBuffer_t *rbuf;
	int     i, j, sum = 0;
	RF_RowCol_t frow = 0;

	for (i = 0; i < raidPtr->numRow; i++)
		if (raidPtr->reconControl[i]) {
			frow = i;
			break;
		}
	RF_ASSERT(frow >= 0);

	if (dolock)
		RF_LOCK_MUTEX(raidPtr->reconControl[frow]->rb_mutex);
	pssTable = raidPtr->reconControl[frow]->pssTable;

	for (i = 0; i < raidPtr->pssTableSize; i++) {
		RF_LOCK_MUTEX(pssTable[i].mutex);
		for (p = pssTable[i].chain; p; p = p->next) {
			rbuf = (RF_ReconBuffer_t *) p->rbuf;
			if (rbuf && rbuf->type == RF_RBUF_TYPE_FLOATING)
				sum++;

			rbuf = (RF_ReconBuffer_t *) p->writeRbuf;
			if (rbuf && rbuf->type == RF_RBUF_TYPE_FLOATING)
				sum++;

			for (j = 0; j < p->xorBufCount; j++) {
				rbuf = (RF_ReconBuffer_t *) p->rbufsForXor[j];
				RF_ASSERT(rbuf);
				if (rbuf->type == RF_RBUF_TYPE_FLOATING)
					sum++;
			}
		}
		RF_UNLOCK_MUTEX(pssTable[i].mutex);
	}

	for (rbuf = raidPtr->reconControl[frow]->floatingRbufs; rbuf; rbuf = rbuf->next) {
		if (rbuf->type == RF_RBUF_TYPE_FLOATING)
			sum++;
	}
	for (rbuf = raidPtr->reconControl[frow]->committedRbufs; rbuf; rbuf = rbuf->next) {
		if (rbuf->type == RF_RBUF_TYPE_FLOATING)
			sum++;
	}
	for (rbuf = raidPtr->reconControl[frow]->fullBufferList; rbuf; rbuf = rbuf->next) {
		if (rbuf->type == RF_RBUF_TYPE_FLOATING)
			sum++;
	}
	for (rbuf = raidPtr->reconControl[frow]->priorityList; rbuf; rbuf = rbuf->next) {
		if (rbuf->type == RF_RBUF_TYPE_FLOATING)
			sum++;
	}

	RF_ASSERT(sum == raidPtr->numFloatingReconBufs);

	if (dolock)
		RF_UNLOCK_MUTEX(raidPtr->reconControl[frow]->rb_mutex);
}
