/*	$NetBSD: rf_driver.c,v 1.39 2000/12/15 02:12:58 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Greg Oster
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, Khalil Amiri, Claudson Bornstein, William V. Courtright II,
 *         Robby Findler, Daniel Stodolsky, Rachad Youssef, Jim Zelenka
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

/******************************************************************************
 *
 * rf_driver.c -- main setup, teardown, and access routines for the RAID driver
 *
 * all routines are prefixed with rf_ (raidframe), to avoid conficts.
 *
 ******************************************************************************/


#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#if defined(__NetBSD__)
#include <sys/ioctl.h>
#elif defined(__FreeBSD__)
#include <sys/ioccom.h>
#include <sys/filio.h>
#endif
#include <sys/fcntl.h>
#include <sys/vnode.h>


#include <dev/raidframe/rf_archs.h>
#include <dev/raidframe/rf_threadstuff.h>

#include <sys/errno.h>

#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_aselect.h>
#include <dev/raidframe/rf_diskqueue.h>
#include <dev/raidframe/rf_parityscan.h>
#include <dev/raidframe/rf_alloclist.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_utils.h>
#include <dev/raidframe/rf_etimer.h>
#include <dev/raidframe/rf_acctrace.h>
#include <dev/raidframe/rf_configure.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_desc.h>
#include <dev/raidframe/rf_states.h>
#include <dev/raidframe/rf_freelist.h>
#include <dev/raidframe/rf_decluster.h>
#include <dev/raidframe/rf_map.h>
#include <dev/raidframe/rf_revent.h>
#include <dev/raidframe/rf_callback.h>
#include <dev/raidframe/rf_engine.h>
#include <dev/raidframe/rf_memchunk.h>
#include <dev/raidframe/rf_mcpair.h>
#include <dev/raidframe/rf_nwayxor.h>
#include <dev/raidframe/rf_debugprint.h>
#include <dev/raidframe/rf_copyback.h>
#include <dev/raidframe/rf_driver.h>
#include <dev/raidframe/rf_options.h>
#include <dev/raidframe/rf_shutdown.h>
#include <dev/raidframe/rf_kintf.h>

#if defined(__FreeBSD__) && __FreeBSD_version > 500005
#include <sys/bio.h>
#endif

#include <sys/buf.h>

/* rad == RF_RaidAccessDesc_t */
static RF_FreeList_t *rf_rad_freelist;
#define RF_MAX_FREE_RAD 128
#define RF_RAD_INC       16
#define RF_RAD_INITIAL   32

/* debug variables */
char    rf_panicbuf[2048];	/* a buffer to hold an error msg when we panic */

/* main configuration routines */
static int raidframe_booted = 0;

static void rf_ConfigureDebug(RF_Config_t * cfgPtr);
static void set_debug_option(char *name, long val);
static void rf_UnconfigureArray(void);
static int init_rad(RF_RaidAccessDesc_t *);
static void clean_rad(RF_RaidAccessDesc_t *);
static void rf_ShutdownRDFreeList(void *);
static int rf_ConfigureRDFreeList(RF_ShutdownList_t **);

RF_DECLARE_MUTEX(rf_printf_mutex)	/* debug only:  avoids interleaved
					 * printfs by different stripes */

#define SIGNAL_QUIESCENT_COND(_raid_)  wakeup(&((_raid_)->accesses_suspended))
#define WAIT_FOR_QUIESCENCE(_raid_) \
	RF_LTSLEEP(&((_raid_)->accesses_suspended), PRIBIO, \
		"raidframe quiesce", 0, &((_raid_)->access_suspend_mutex))

#if defined(__FreeBSD__) && __FreeBSD_version > 500005
#define IO_BUF_ERR(bp, err) { \
	bp->bio_flags |= BIO_ERROR; \
	bp->bio_resid = bp->bio_bcount; \
	bp->bio_error = err; \
	biodone(bp); \
};
#else
#define IO_BUF_ERR(bp, err) { \
	bp->b_flags |= B_ERROR; \
	bp->b_resid = bp->b_bcount; \
	bp->b_error = err; \
	biodone(bp); \
}
#endif

static int configureCount = 0;	/* number of active configurations */
static int configInProgress = 0; /* configuration is in progress and code
				  * needs to be serialized. */
static int isconfigged = 0;	/* is basic raidframe (non per-array)
				 * stuff configged */
RF_DECLARE_STATIC_MUTEX(configureMutex)	/* used to lock the configuration
					 * stuff */
static RF_ShutdownList_t *globalShutdown;	/* non array-specific
						 * stuff */

/* called at system boot time */
int     
rf_BootRaidframe()
{
	int     rc;

	if (raidframe_booted)
		return (EBUSY);
	raidframe_booted = 1;

	rc = rf_mutex_init(&configureMutex, __FUNCTION__);
	if (rc) {
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		RF_PANIC();
	}
	configureCount = 0;
	isconfigged = 0;
	globalShutdown = NULL;
	return (0);
}
/*
 * This function is really just for debugging user-level stuff: it
 * frees up all memory, other RAIDframe resources which might otherwise
 * be kept around. This is used with systems like "sentinel" to detect
 * memory leaks.
 */
int 
rf_UnbootRaidframe()
{
	int     rc;

	RF_LOCK_MUTEX(configureMutex);
	if (configureCount) {
		RF_UNLOCK_MUTEX(configureMutex);
		return (EBUSY);
	}
	raidframe_booted = 0;
	RF_UNLOCK_MUTEX(configureMutex);
	rc = rf_mutex_destroy(&configureMutex);
	if (rc) {
		RF_ERRORMSG3("Unable to destroy mutex file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		RF_PANIC();
	}
	return (0);
}
/*
 * Called whenever an array is shutdown
 */
static void 
rf_UnconfigureArray()
{
	int     rc;

	RF_LOCK_MUTEX(configureMutex);
	if (--configureCount == 0) {	/* if no active configurations, shut
					 * everything down */
		isconfigged = 0;

		rc = rf_ShutdownList(&globalShutdown);
		if (rc) {
			RF_ERRORMSG1("RAIDFRAME: unable to do global shutdown, rc=%d\n", rc);
		}

		/*
	         * We must wait until now, because the AllocList module
	         * uses the DebugMem module.
	         */
		if (rf_memDebug)
			rf_print_unfreed();
	}
	RF_UNLOCK_MUTEX(configureMutex);
}

/*
 * Called to shut down an array.
 */
int 
rf_Shutdown(raidPtr)
	RF_Raid_t *raidPtr;
{

	if (!raidPtr->valid) {
		RF_ERRORMSG("Attempt to shut down unconfigured RAIDframe driver.  Aborting shutdown\n");
		return (EINVAL);
	}
	/*
         * wait for outstanding IOs to land
         * As described in rf_raid.h, we use the rad_freelist lock
         * to protect the per-array info about outstanding descs
         * since we need to do freelist locking anyway, and this
         * cuts down on the amount of serialization we've got going
         * on.
         */
	RF_FREELIST_DO_LOCK(rf_rad_freelist);
	if (raidPtr->waitShutdown) {
		RF_FREELIST_DO_UNLOCK(rf_rad_freelist);
		return (EBUSY);
	}
	raidPtr->waitShutdown = 1;
	while (raidPtr->nAccOutstanding) {
		RF_WAIT_COND(raidPtr->outstandingCond, RF_FREELIST_MUTEX_OF(rf_rad_freelist));
	}
	RF_FREELIST_DO_UNLOCK(rf_rad_freelist);

	/* Wait for any parity re-writes to stop... */
	while (raidPtr->parity_rewrite_in_progress) {
		printf("Waiting for parity re-write to exit...\n");
		tsleep(&raidPtr->parity_rewrite_in_progress, PRIBIO,
		       "rfprwshutdown", 0);
	}

	raidPtr->valid = 0;

	rf_update_component_labels(raidPtr, RF_FINAL_COMPONENT_UPDATE);

	rf_UnconfigureVnodes(raidPtr);

	rf_ShutdownList(&raidPtr->shutdownList);

	rf_UnconfigureArray();

	return (0);
}


#define DO_INIT_CONFIGURE(f) { \
	rc = f (&globalShutdown); \
	if (rc) { \
		RF_ERRORMSG2("RAIDFRAME: failed %s with %d\n", RF_STRING(f), rc); \
		rf_ShutdownList(&globalShutdown); \
		RF_LOCK_MUTEX(configureMutex); \
		configInProgress = 0; \
		configureCount--; \
		RF_UNLOCK_MUTEX(configureMutex); \
		return(rc); \
	} \
}

#define DO_RAID_FAIL() { \
	rf_UnconfigureVnodes(raidPtr); \
	rf_ShutdownList(&raidPtr->shutdownList); \
	rf_UnconfigureArray(); \
}

#define DO_RAID_INIT_CONFIGURE(f) { \
	rc = f (&raidPtr->shutdownList, raidPtr, cfgPtr); \
	if (rc) { \
		RF_ERRORMSG2("RAIDFRAME: failed %s with %d\n", RF_STRING(f), rc); \
		DO_RAID_FAIL(); \
		return(rc); \
	} \
}

#define DO_RAID_MUTEX(_m_) { \
	rc = rf_create_managed_mutex(&raidPtr->shutdownList, (_m_)); \
	if (rc) { \
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", \
			__FILE__, __LINE__, rc); \
		DO_RAID_FAIL(); \
		return(rc); \
	} \
}

#define DO_RAID_COND(_c_) { \
	rc = rf_create_managed_cond(&raidPtr->shutdownList, (_c_)); \
	if (rc) { \
		RF_ERRORMSG3("Unable to init cond file %s line %d rc=%d\n", \
			__FILE__, __LINE__, rc); \
		DO_RAID_FAIL(); \
		return(rc); \
	} \
}

int 
rf_Configure(raidPtr, cfgPtr, ac)
	RF_Raid_t *raidPtr;
	RF_Config_t *cfgPtr;
	RF_AutoConfig_t *ac;
{
	RF_RowCol_t row, col;
	int     i, rc;

	/* XXX This check can probably be removed now, since 
	   RAIDFRAME_CONFIGURE now checks to make sure that the
	   RAID set is not already valid
	*/
	if (raidPtr->valid) {
		RF_ERRORMSG("RAIDframe configuration not shut down.  Aborting configure.\n");
		return (EINVAL);
	}
	RF_LOCK_MUTEX(configureMutex);
	if (configInProgress == 1) {
		RF_UNLOCK_MUTEX(configureMutex);
		return (EBUSY);
	}
	configureCount++;
	if (isconfigged == 0) {
		configInProgress = 1;
		RF_UNLOCK_MUTEX(configureMutex);
		rc = rf_create_managed_mutex(&globalShutdown, &rf_printf_mutex);
		if (rc) {
			RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
			    __LINE__, rc);
			rf_ShutdownList(&globalShutdown);
			return (rc);
		}
		/* initialize globals */
		printf("RAIDFRAME: protectedSectors is %ld\n", 
		       rf_protectedSectors);

		rf_clear_debug_print_buffer();

		DO_INIT_CONFIGURE(rf_ConfigureAllocList);

		/*
	         * Yes, this does make debugging general to the whole
	         * system instead of being array specific. Bummer, drag.  
		 */
		rf_ConfigureDebug(cfgPtr);
		DO_INIT_CONFIGURE(rf_ConfigureDebugMem);
		DO_INIT_CONFIGURE(rf_ConfigureAccessTrace);
		DO_INIT_CONFIGURE(rf_ConfigureMapModule);
		DO_INIT_CONFIGURE(rf_ConfigureReconEvent);
		DO_INIT_CONFIGURE(rf_ConfigureCallback);
		DO_INIT_CONFIGURE(rf_ConfigureMemChunk);
		DO_INIT_CONFIGURE(rf_ConfigureRDFreeList);
		DO_INIT_CONFIGURE(rf_ConfigureNWayXor);
		DO_INIT_CONFIGURE(rf_ConfigureStripeLockFreeList);
		DO_INIT_CONFIGURE(rf_ConfigureMCPair);
		DO_INIT_CONFIGURE(rf_ConfigureDAGs);
		DO_INIT_CONFIGURE(rf_ConfigureDAGFuncs);
		DO_INIT_CONFIGURE(rf_ConfigureDebugPrint);
		DO_INIT_CONFIGURE(rf_ConfigureReconstruction);
		DO_INIT_CONFIGURE(rf_ConfigureCopyback);
		DO_INIT_CONFIGURE(rf_ConfigureDiskQueueSystem);

		RF_LOCK_MUTEX(configureMutex);
		isconfigged = 1;
		configInProgress = 0;
	}
	RF_UNLOCK_MUTEX(configureMutex);

	DO_RAID_MUTEX(&raidPtr->mutex);
	/* set up the cleanup list.  Do this after ConfigureDebug so that
	 * value of memDebug will be set */

	rf_MakeAllocList(raidPtr->cleanupList);
	if (raidPtr->cleanupList == NULL) {
		DO_RAID_FAIL();
		return (ENOMEM);
	}
	rc = rf_ShutdownCreate(&raidPtr->shutdownList,
	    (void (*) (void *)) rf_FreeAllocList,
	    raidPtr->cleanupList);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n",
		    __FILE__, __LINE__, rc);
		DO_RAID_FAIL();
		return (rc);
	}
	raidPtr->numRow = cfgPtr->numRow;
	raidPtr->numCol = cfgPtr->numCol;
	raidPtr->numSpare = cfgPtr->numSpare;

	/* XXX we don't even pretend to support more than one row in the
	 * kernel... */
	if (raidPtr->numRow != 1) {
		RF_ERRORMSG("Only one row supported in kernel.\n");
		DO_RAID_FAIL();
		return (EINVAL);
	}
	RF_CallocAndAdd(raidPtr->status, raidPtr->numRow, sizeof(RF_RowStatus_t),
	    (RF_RowStatus_t *), raidPtr->cleanupList);
	if (raidPtr->status == NULL) {
		DO_RAID_FAIL();
		return (ENOMEM);
	}
	RF_CallocAndAdd(raidPtr->reconControl, raidPtr->numRow,
	    sizeof(RF_ReconCtrl_t *), (RF_ReconCtrl_t **), raidPtr->cleanupList);
	if (raidPtr->reconControl == NULL) {
		DO_RAID_FAIL();
		return (ENOMEM);
	}
	for (i = 0; i < raidPtr->numRow; i++) {
		raidPtr->status[i] = rf_rs_optimal;
		raidPtr->reconControl[i] = NULL;
	}

	DO_RAID_INIT_CONFIGURE(rf_ConfigureEngine);
	DO_RAID_INIT_CONFIGURE(rf_ConfigureStripeLocks);

	DO_RAID_COND(&raidPtr->outstandingCond);

	raidPtr->nAccOutstanding = 0;
	raidPtr->waitShutdown = 0;

	DO_RAID_MUTEX(&raidPtr->access_suspend_mutex);
	DO_RAID_COND(&raidPtr->quiescent_cond);

	DO_RAID_COND(&raidPtr->waitForReconCond);

	DO_RAID_MUTEX(&raidPtr->recon_done_proc_mutex);

	if (ac!=NULL) {
		/* We have an AutoConfig structure..  Don't do the
		   normal disk configuration... call the auto config
		   stuff */
		rf_AutoConfigureDisks(raidPtr, cfgPtr, ac);
	} else {
		DO_RAID_INIT_CONFIGURE(rf_ConfigureDisks);
		DO_RAID_INIT_CONFIGURE(rf_ConfigureSpareDisks);
	}
	/* do this after ConfigureDisks & ConfigureSpareDisks to be sure dev
	 * no. is set */
	DO_RAID_INIT_CONFIGURE(rf_ConfigureDiskQueues);

	DO_RAID_INIT_CONFIGURE(rf_ConfigureLayout);

	DO_RAID_INIT_CONFIGURE(rf_ConfigurePSStatus);

	for (row = 0; row < raidPtr->numRow; row++) {
		for (col = 0; col < raidPtr->numCol; col++) {
			/*
		         * XXX better distribution
		         */
			raidPtr->hist_diskreq[row][col] = 0;
		}
	}

	raidPtr->numNewFailures = 0;
	raidPtr->copyback_in_progress = 0;
	raidPtr->parity_rewrite_in_progress = 0;
	raidPtr->recon_in_progress = 0;
	raidPtr->maxOutstanding = cfgPtr->maxOutstandingDiskReqs;

	/* autoconfigure and root_partition will actually get filled in 
	   after the config is done */
	raidPtr->autoconfigure = 0;
	raidPtr->root_partition = 0;
	raidPtr->last_unit = raidPtr->raidid;
	raidPtr->config_order = 0;

	if (rf_keepAccTotals) {
		raidPtr->keep_acc_totals = 1;
	}
	rf_StartUserStats(raidPtr);

	raidPtr->valid = 1;
	return (0);
}

static int 
init_rad(desc)
	RF_RaidAccessDesc_t *desc;
{
	int     rc;

	rc = rf_mutex_init(&desc->mutex, __FUNCTION__);
	if (rc) {
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		return (rc);
	}
	rc = rf_cond_init(&desc->cond);
	if (rc) {
		RF_ERRORMSG3("Unable to init cond file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		rf_mutex_destroy(&desc->mutex);
		return (rc);
	}
	return (0);
}

static void 
clean_rad(desc)
	RF_RaidAccessDesc_t *desc;
{
	rf_mutex_destroy(&desc->mutex);
	rf_cond_destroy(&desc->cond);
}

static void 
rf_ShutdownRDFreeList(ignored)
	void   *ignored;
{
	RF_FREELIST_DESTROY_CLEAN(rf_rad_freelist, next, (RF_RaidAccessDesc_t *), clean_rad);
}

static int 
rf_ConfigureRDFreeList(listp)
	RF_ShutdownList_t **listp;
{
	int     rc;

	RF_FREELIST_CREATE(rf_rad_freelist, RF_MAX_FREE_RAD,
	    RF_RAD_INC, sizeof(RF_RaidAccessDesc_t));
	if (rf_rad_freelist == NULL) {
		return (ENOMEM);
	}
	rc = rf_ShutdownCreate(listp, rf_ShutdownRDFreeList, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		rf_ShutdownRDFreeList(NULL);
		return (rc);
	}
	RF_FREELIST_PRIME_INIT(rf_rad_freelist, RF_RAD_INITIAL, next,
	    (RF_RaidAccessDesc_t *), init_rad);
	return (0);
}

RF_RaidAccessDesc_t *
rf_AllocRaidAccDesc(
    RF_Raid_t * raidPtr,
    RF_IoType_t type,
    RF_RaidAddr_t raidAddress,
    RF_SectorCount_t numBlocks,
    caddr_t bufPtr,
    void *bp,
    RF_DagHeader_t ** paramDAG,
    RF_AccessStripeMapHeader_t ** paramASM,
    RF_RaidAccessFlags_t flags,
    void (*cbF) (RF_Buf_t),
    void *cbA,
    RF_AccessState_t * states)
{
	RF_RaidAccessDesc_t *desc;

	RF_FREELIST_GET_INIT_NOUNLOCK(rf_rad_freelist, desc, next, (RF_RaidAccessDesc_t *), init_rad);
	if (raidPtr->waitShutdown) {
		/*
	         * Actually, we're shutting the array down. Free the desc
	         * and return NULL.
	         */
		RF_FREELIST_DO_UNLOCK(rf_rad_freelist);
		RF_FREELIST_FREE_CLEAN(rf_rad_freelist, desc, next, clean_rad);
		return (NULL);
	}
	raidPtr->nAccOutstanding++;
	RF_FREELIST_DO_UNLOCK(rf_rad_freelist);

	desc->raidPtr = (void *) raidPtr;
	desc->type = type;
	desc->raidAddress = raidAddress;
	desc->numBlocks = numBlocks;
	desc->bufPtr = bufPtr;
	desc->bp = bp;
	desc->paramDAG = paramDAG;
	desc->paramASM = paramASM;
	desc->flags = flags;
	desc->states = states;
	desc->state = 0;

	desc->status = 0;
	bzero((char *) &desc->tracerec, sizeof(RF_AccTraceEntry_t));
	desc->callbackFunc = (void (*) (RF_CBParam_t)) cbF;	/* XXX */
	desc->callbackArg = cbA;
	desc->next = NULL;
	desc->head = desc;
	desc->numPending = 0;
	desc->cleanupList = NULL;
	rf_MakeAllocList(desc->cleanupList);
	return (desc);
}

void 
rf_FreeRaidAccDesc(RF_RaidAccessDesc_t * desc)
{
	RF_Raid_t *raidPtr = desc->raidPtr;

	RF_ASSERT(desc);

	rf_FreeAllocList(desc->cleanupList);
	RF_FREELIST_FREE_CLEAN_NOUNLOCK(rf_rad_freelist, desc, next, clean_rad);
	raidPtr->nAccOutstanding--;
	if (raidPtr->waitShutdown) {
		RF_SIGNAL_COND(raidPtr->outstandingCond);
	}
	RF_FREELIST_DO_UNLOCK(rf_rad_freelist);
}
/*********************************************************************
 * Main routine for performing an access.
 * Accesses are retried until a DAG can not be selected.  This occurs
 * when either the DAG library is incomplete or there are too many
 * failures in a parity group.
 ********************************************************************/
int 
rf_DoAccess(
    RF_Raid_t * raidPtr,
    RF_IoType_t type,
    int async_flag,
    RF_RaidAddr_t raidAddress,
    RF_SectorCount_t numBlocks,
    caddr_t bufPtr,
    void *bp_in,
    RF_DagHeader_t ** paramDAG,
    RF_AccessStripeMapHeader_t ** paramASM,
    RF_RaidAccessFlags_t flags,
    RF_RaidAccessDesc_t ** paramDesc,
    void (*cbF) (RF_Buf_t),
    void *cbA)
/*
type should be read or write
async_flag should be RF_TRUE or RF_FALSE
bp_in is a buf pointer.  void * to facilitate ignoring it outside the kernel
*/
{
	RF_RaidAccessDesc_t *desc;
	caddr_t lbufPtr = bufPtr;
	RF_Buf_t bp = (RF_Buf_t) bp_in;

	raidAddress += rf_raidSectorOffset;

	if (!raidPtr->valid) {
		RF_ERRORMSG("RAIDframe driver not successfully configured.  Rejecting access.\n");
		IO_BUF_ERR(bp, EINVAL);
		return (EINVAL);
	}

	if (rf_accessDebug) {

		printf("logBytes is: %d %d %d\n", raidPtr->raidid,
		    raidPtr->logBytesPerSector,
		    (int) rf_RaidAddressToByte(raidPtr, numBlocks));
		printf("raid%d: %s raidAddr %d (stripeid %d-%d) numBlocks %d (%d bytes) buf 0x%lx\n", raidPtr->raidid,
		    (type == RF_IO_TYPE_READ) ? "READ" : "WRITE", (int) raidAddress,
		    (int) rf_RaidAddressToStripeID(&raidPtr->Layout, raidAddress),
		    (int) rf_RaidAddressToStripeID(&raidPtr->Layout, raidAddress + numBlocks - 1),
		    (int) numBlocks,
		    (int) rf_RaidAddressToByte(raidPtr, numBlocks),
		    (long) bufPtr);
	}
	if (raidAddress + numBlocks > raidPtr->totalSectors) {

		printf("DoAccess: raid addr %lu too large to access %lu sectors.  Max legal addr is %lu\n",
		    (u_long) raidAddress, (u_long) numBlocks, (u_long) raidPtr->totalSectors);

		IO_BUF_ERR(bp, ENOSPC);
		return (ENOSPC);
	}
	desc = rf_AllocRaidAccDesc(raidPtr, type, raidAddress,
	    numBlocks, lbufPtr, bp, paramDAG, paramASM,
	    flags, cbF, cbA, raidPtr->Layout.map->states);

	if (desc == NULL) {
		return (ENOMEM);
	}
	RF_ETIMER_START(desc->tracerec.tot_timer);

	desc->async_flag = async_flag;

	rf_ContinueRaidAccess(desc);

	return (0);
}
/* force the array into reconfigured mode without doing reconstruction */
int 
rf_SetReconfiguredMode(raidPtr, row, col)
	RF_Raid_t *raidPtr;
	int     row;
	int     col;
{
	if (!(raidPtr->Layout.map->flags & RF_DISTRIBUTE_SPARE)) {
		printf("Can't set reconfigured mode in dedicated-spare array\n");
		RF_PANIC();
	}
	RF_LOCK_MUTEX(raidPtr->mutex);
	raidPtr->numFailures++;
	raidPtr->Disks[row][col].status = rf_ds_dist_spared;
	raidPtr->status[row] = rf_rs_reconfigured;
	rf_update_component_labels(raidPtr, RF_NORMAL_COMPONENT_UPDATE);
	/* install spare table only if declustering + distributed sparing
	 * architecture. */
	if (raidPtr->Layout.map->flags & RF_BD_DECLUSTERED)
		rf_InstallSpareTable(raidPtr, row, col);
	RF_UNLOCK_MUTEX(raidPtr->mutex);
	return (0);
}

extern int fail_row, fail_col, fail_time;
extern int delayed_recon;

int 
rf_FailDisk(
    RF_Raid_t * raidPtr,
    int frow,
    int fcol,
    int initRecon)
{
	printf("raid%d: Failing disk r%d c%d\n", raidPtr->raidid, frow, fcol);
	RF_LOCK_MUTEX(raidPtr->mutex);
	raidPtr->numFailures++;
	raidPtr->Disks[frow][fcol].status = rf_ds_failed;
	raidPtr->status[frow] = rf_rs_degraded;
	rf_update_component_labels(raidPtr, RF_NORMAL_COMPONENT_UPDATE);
	RF_UNLOCK_MUTEX(raidPtr->mutex);
	if (initRecon)
		rf_ReconstructFailedDisk(raidPtr, frow, fcol);
	return (0);
}
/* releases a thread that is waiting for the array to become quiesced.
 * access_suspend_mutex should be locked upon calling this
 */
void 
rf_SignalQuiescenceLock(raidPtr, reconDesc)
	RF_Raid_t *raidPtr;
	RF_RaidReconDesc_t *reconDesc;
{
	if (rf_quiesceDebug) {
		printf("raid%d: Signalling quiescence lock\n", 
		       raidPtr->raidid);
	}
	raidPtr->access_suspend_release = 1;

	if (raidPtr->waiting_for_quiescence) {
		SIGNAL_QUIESCENT_COND(raidPtr);
	}
}
/* suspends all new requests to the array.  No effect on accesses that are in flight.  */
int 
rf_SuspendNewRequestsAndWait(raidPtr)
	RF_Raid_t *raidPtr;
{
	if (rf_quiesceDebug)
		printf("Suspending new reqs\n");

	RF_LOCK_MUTEX(raidPtr->access_suspend_mutex);
	raidPtr->accesses_suspended++;
	raidPtr->waiting_for_quiescence = (raidPtr->accs_in_flight == 0) ? 0 : 1;

	if (raidPtr->waiting_for_quiescence) {
		raidPtr->access_suspend_release = 0;
		while (!raidPtr->access_suspend_release) {
			printf("Suspending: Waiting for Quiescence\n");
			WAIT_FOR_QUIESCENCE(raidPtr);
			raidPtr->waiting_for_quiescence = 0;
		}
	}
	printf("Quiescence reached..\n");

	RF_UNLOCK_MUTEX(raidPtr->access_suspend_mutex);
	return (raidPtr->waiting_for_quiescence);
}
/* wake up everyone waiting for quiescence to be released */
void 
rf_ResumeNewRequests(raidPtr)
	RF_Raid_t *raidPtr;
{
	RF_CallbackDesc_t *t, *cb;

	if (rf_quiesceDebug)
		printf("Resuming new reqs\n");

	RF_LOCK_MUTEX(raidPtr->access_suspend_mutex);
	raidPtr->accesses_suspended--;
	if (raidPtr->accesses_suspended == 0)
		cb = raidPtr->quiesce_wait_list;
	else
		cb = NULL;
	raidPtr->quiesce_wait_list = NULL;
	RF_UNLOCK_MUTEX(raidPtr->access_suspend_mutex);

	while (cb) {
		t = cb;
		cb = cb->next;
		(t->callbackFunc) (t->callbackArg);
		rf_FreeCallbackDesc(t);
	}
}
/*****************************************************************************************
 *
 * debug routines
 *
 ****************************************************************************************/

static void 
set_debug_option(name, val)
	char   *name;
	long    val;
{
	RF_DebugName_t *p;

	for (p = rf_debugNames; p->name; p++) {
		if (!strcmp(p->name, name)) {
			*(p->ptr) = val;
			printf("[Set debug variable %s to %ld]\n", name, val);
			return;
		}
	}
	RF_ERRORMSG1("Unknown debug string \"%s\"\n", name);
}


/* would like to use sscanf here, but apparently not available in kernel */
/*ARGSUSED*/
static void 
rf_ConfigureDebug(cfgPtr)
	RF_Config_t *cfgPtr;
{
	char   *val_p, *name_p, *white_p;
	long    val;
	int     i;

	rf_ResetDebugOptions();
	for (i = 0; cfgPtr->debugVars[i][0] && i < RF_MAXDBGV; i++) {
		name_p = rf_find_non_white(&cfgPtr->debugVars[i][0]);
		white_p = rf_find_white(name_p);	/* skip to start of 2nd
							 * word */
		val_p = rf_find_non_white(white_p);
		if (*val_p == '0' && *(val_p + 1) == 'x')
			val = rf_htoi(val_p + 2);
		else
			val = rf_atoi(val_p);
		*white_p = '\0';
		set_debug_option(name_p, val);
	}
}
/* performance monitoring stuff */

#define TIMEVAL_TO_US(t) (((long) t.tv_sec) * 1000000L + (long) t.tv_usec)

#if !defined(_KERNEL) && !defined(SIMULATE)

/*
 * Throughput stats currently only used in user-level RAIDframe
 */

static int 
rf_InitThroughputStats(
    RF_ShutdownList_t ** listp,
    RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr)
{
	int     rc;

	/* these used by user-level raidframe only */
	rc = rf_create_managed_mutex(listp, &raidPtr->throughputstats.mutex);
	if (rc) {
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		return (rc);
	}
	raidPtr->throughputstats.sum_io_us = 0;
	raidPtr->throughputstats.num_ios = 0;
	raidPtr->throughputstats.num_out_ios = 0;
	return (0);
}

void 
rf_StartThroughputStats(RF_Raid_t * raidPtr)
{
	RF_LOCK_MUTEX(raidPtr->throughputstats.mutex);
	raidPtr->throughputstats.num_ios++;
	raidPtr->throughputstats.num_out_ios++;
	if (raidPtr->throughputstats.num_out_ios == 1)
		RF_GETTIME(raidPtr->throughputstats.start);
	RF_UNLOCK_MUTEX(raidPtr->throughputstats.mutex);
}

static void 
rf_StopThroughputStats(RF_Raid_t * raidPtr)
{
	struct timeval diff;

	RF_LOCK_MUTEX(raidPtr->throughputstats.mutex);
	raidPtr->throughputstats.num_out_ios--;
	if (raidPtr->throughputstats.num_out_ios == 0) {
		RF_GETTIME(raidPtr->throughputstats.stop);
		RF_TIMEVAL_DIFF(&raidPtr->throughputstats.start, &raidPtr->throughputstats.stop, &diff);
		raidPtr->throughputstats.sum_io_us += TIMEVAL_TO_US(diff);
	}
	RF_UNLOCK_MUTEX(raidPtr->throughputstats.mutex);
}

static void 
rf_PrintThroughputStats(RF_Raid_t * raidPtr)
{
	RF_ASSERT(raidPtr->throughputstats.num_out_ios == 0);
	if (raidPtr->throughputstats.sum_io_us != 0) {
		printf("[Througphut: %8.2f IOs/second]\n", raidPtr->throughputstats.num_ios
		    / (raidPtr->throughputstats.sum_io_us / 1000000.0));
	}
}
#endif				/* !KERNEL && !SIMULATE */

void 
rf_StartUserStats(RF_Raid_t * raidPtr)
{
	RF_GETTIME(raidPtr->userstats.start);
	raidPtr->userstats.sum_io_us = 0;
	raidPtr->userstats.num_ios = 0;
	raidPtr->userstats.num_sect_moved = 0;
}

void 
rf_StopUserStats(RF_Raid_t * raidPtr)
{
	RF_GETTIME(raidPtr->userstats.stop);
}

void 
rf_UpdateUserStats(raidPtr, rt, numsect)
	RF_Raid_t *raidPtr;
	int     rt;		/* resp time in us */
	int     numsect;	/* number of sectors for this access */
{
	raidPtr->userstats.sum_io_us += rt;
	raidPtr->userstats.num_ios++;
	raidPtr->userstats.num_sect_moved += numsect;
}

void 
rf_PrintUserStats(RF_Raid_t * raidPtr)
{
	long    elapsed_us, mbs, mbs_frac;
	struct timeval diff;

	RF_TIMEVAL_DIFF(&raidPtr->userstats.start, &raidPtr->userstats.stop, &diff);
	elapsed_us = TIMEVAL_TO_US(diff);

	/* 2000 sectors per megabyte, 10000000 microseconds per second */
	if (elapsed_us)
		mbs = (raidPtr->userstats.num_sect_moved / 2000) / (elapsed_us / 1000000);
	else
		mbs = 0;

	/* this computes only the first digit of the fractional mb/s moved */
	if (elapsed_us) {
		mbs_frac = ((raidPtr->userstats.num_sect_moved / 200) / (elapsed_us / 1000000))
		    - (mbs * 10);
	} else {
		mbs_frac = 0;
	}

	printf("Number of I/Os:             %ld\n", raidPtr->userstats.num_ios);
	printf("Elapsed time (us):          %ld\n", elapsed_us);
	printf("User I/Os per second:       %ld\n", RF_DB0_CHECK(raidPtr->userstats.num_ios, (elapsed_us / 1000000)));
	printf("Average user response time: %ld us\n", RF_DB0_CHECK(raidPtr->userstats.sum_io_us, raidPtr->userstats.num_ios));
	printf("Total sectors moved:        %ld\n", raidPtr->userstats.num_sect_moved);
	printf("Average access size (sect): %ld\n", RF_DB0_CHECK(raidPtr->userstats.num_sect_moved, raidPtr->userstats.num_ios));
	printf("Achieved data rate:         %ld.%ld MB/sec\n", mbs, mbs_frac);
}


void
rf_print_panic_message(line,file)
	int line;
	char *file;
{
	sprintf(rf_panicbuf,"raidframe error at line %d file %s",
		line, file);
}

void
rf_print_assert_panic_message(line,file,condition)
	int line;
	char *file;
	char *condition;
{
	sprintf(rf_panicbuf,
		"raidframe error at line %d file %s (failed asserting %s)\n",
		line, file, condition);
}
