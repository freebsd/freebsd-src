/*	$NetBSD: rf_psstatus.c,v 1.5 2000/01/08 22:57:31 oster Exp $	*/

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

/*****************************************************************************
 *
 * psstatus.c
 *
 * The reconstruction code maintains a bunch of status related to the parity
 * stripes that are currently under reconstruction.  This header file defines
 * the status structures.
 *
 *****************************************************************************/

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_debugprint.h>
#include <dev/raidframe/rf_freelist.h>
#include <dev/raidframe/rf_psstatus.h>
#include <dev/raidframe/rf_shutdown.h>

#define Dprintf1(s,a)         if (rf_pssDebug) rf_debug_printf(s,(void *)((unsigned long)a),NULL,NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf2(s,a,b)       if (rf_pssDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),NULL,NULL,NULL,NULL,NULL,NULL)
#define Dprintf3(s,a,b,c)     if (rf_pssDebug) rf_debug_printf(s,(void *)((unsigned long)a),(void *)((unsigned long)b),(void *)((unsigned long)c),NULL,NULL,NULL,NULL,NULL)

static void 
RealPrintPSStatusTable(RF_Raid_t * raidPtr,
    RF_PSStatusHeader_t * pssTable);

#define RF_MAX_FREE_PSS  32
#define RF_PSS_INC        8
#define RF_PSS_INITIAL    4

static int init_pss(RF_ReconParityStripeStatus_t *, RF_Raid_t *);
static void clean_pss(RF_ReconParityStripeStatus_t *, RF_Raid_t *);
static void rf_ShutdownPSStatus(void *);

static int 
init_pss(p, raidPtr)
	RF_ReconParityStripeStatus_t *p;
	RF_Raid_t *raidPtr;
{
	RF_Calloc(p->issued, raidPtr->numCol, sizeof(char), (char *));
	if (p->issued == NULL)
		return (ENOMEM);
	return (0);
}

static void 
clean_pss(p, raidPtr)
	RF_ReconParityStripeStatus_t *p;
	RF_Raid_t *raidPtr;
{
	RF_Free(p->issued, raidPtr->numCol * sizeof(char));
}

static void 
rf_ShutdownPSStatus(arg)
	void   *arg;
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) arg;

	RF_FREELIST_DESTROY_CLEAN_ARG(raidPtr->pss_freelist, next, (RF_ReconParityStripeStatus_t *), clean_pss, raidPtr);
}

int 
rf_ConfigurePSStatus(
    RF_ShutdownList_t ** listp,
    RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr)
{
	int     rc;

	raidPtr->pssTableSize = RF_PSS_DEFAULT_TABLESIZE;
	RF_FREELIST_CREATE(raidPtr->pss_freelist, RF_MAX_FREE_PSS,
	    RF_PSS_INC, sizeof(RF_ReconParityStripeStatus_t));
	if (raidPtr->pss_freelist == NULL)
		return (ENOMEM);
	rc = rf_ShutdownCreate(listp, rf_ShutdownPSStatus, raidPtr);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n",
		    __FILE__, __LINE__, rc);
		rf_ShutdownPSStatus(raidPtr);
		return (rc);
	}
	RF_FREELIST_PRIME_INIT_ARG(raidPtr->pss_freelist, RF_PSS_INITIAL, next,
	    (RF_ReconParityStripeStatus_t *), init_pss, raidPtr);
	return (0);
}
/*****************************************************************************************
 * sets up the pss table
 * We pre-allocate a bunch of entries to avoid as much as possible having to
 * malloc up hash chain entries.
 ****************************************************************************************/
RF_PSStatusHeader_t *
rf_MakeParityStripeStatusTable(raidPtr)
	RF_Raid_t *raidPtr;
{
	RF_PSStatusHeader_t *pssTable;
	int     i, j, rc;

	RF_Calloc(pssTable, raidPtr->pssTableSize, sizeof(RF_PSStatusHeader_t), (RF_PSStatusHeader_t *));
	for (i = 0; i < raidPtr->pssTableSize; i++) {
		rc = rf_mutex_init(&pssTable[i].mutex, __FUNCTION__);
		if (rc) {
			RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
			    __LINE__, rc);
			/* fail and deallocate */
			for (j = 0; j < i; j++) {
				rf_mutex_destroy(&pssTable[i].mutex);
			}
			RF_Free(pssTable, raidPtr->pssTableSize * sizeof(RF_PSStatusHeader_t));
			return (NULL);
		}
	}
	return (pssTable);
}

void 
rf_FreeParityStripeStatusTable(raidPtr, pssTable)
	RF_Raid_t *raidPtr;
	RF_PSStatusHeader_t *pssTable;
{
	int     i;

	if (rf_pssDebug)
		RealPrintPSStatusTable(raidPtr, pssTable);
	for (i = 0; i < raidPtr->pssTableSize; i++) {
		if (pssTable[i].chain) {
			printf("ERROR: pss hash chain not null at recon shutdown\n");
		}
		rf_mutex_destroy(&pssTable[i].mutex);
	}
	RF_Free(pssTable, raidPtr->pssTableSize * sizeof(RF_PSStatusHeader_t));
}


/* looks up the status structure for a parity stripe.
 * if the create_flag is on, creates and returns the status structure it it doesn't exist
 * otherwise returns NULL if the status structure does not exist
 *
 * ASSUMES THE PSS DESCRIPTOR IS LOCKED UPON ENTRY
 */
RF_ReconParityStripeStatus_t *
rf_LookupRUStatus(
    RF_Raid_t * raidPtr,
    RF_PSStatusHeader_t * pssTable,
    RF_StripeNum_t psID,
    RF_ReconUnitNum_t which_ru,
    RF_PSSFlags_t flags,	/* whether or not to create it if it doesn't
				 * exist + what flags to set initially */
    int *created)
{
	RF_PSStatusHeader_t *hdr = &pssTable[RF_HASH_PSID(raidPtr, psID)];
	RF_ReconParityStripeStatus_t *p, *pssPtr = hdr->chain;

	*created = 0;
	for (p = pssPtr; p; p = p->next) {
		if (p->parityStripeID == psID && p->which_ru == which_ru)
			break;
	}

	if (!p && (flags & RF_PSS_CREATE)) {
		Dprintf2("PSS: creating pss for psid %ld ru %d\n", psID, which_ru);
		p = rf_AllocPSStatus(raidPtr);
		p->next = hdr->chain;
		hdr->chain = p;

		p->parityStripeID = psID;
		p->which_ru = which_ru;
		p->flags = flags;
		p->rbuf = NULL;
		p->writeRbuf = NULL;
		p->blockCount = 0;
		p->procWaitList = NULL;
		p->blockWaitList = NULL;
		p->bufWaitList = NULL;
		*created = 1;
	} else
		if (p) {	/* we didn't create, but we want to specify
				 * some new status */
			p->flags |= flags;	/* add in whatever flags we're
						 * specifying */
		}
	if (p && (flags & RF_PSS_RECON_BLOCKED)) {
		p->blockCount++;/* if we're asking to block recon, bump the
				 * count */
		Dprintf3("raid%d: Blocked recon on psid %ld.  count now %d\n",
			 raidPtr->raidid, psID, p->blockCount);
	}
	return (p);
}
/* deletes an entry from the parity stripe status table.  typically used
 * when an entry has been allocated solely to block reconstruction, and
 * no recon was requested while recon was blocked.  Assumes the hash
 * chain is ALREADY LOCKED.
 */
void 
rf_PSStatusDelete(raidPtr, pssTable, pssPtr)
	RF_Raid_t *raidPtr;
	RF_PSStatusHeader_t *pssTable;
	RF_ReconParityStripeStatus_t *pssPtr;
{
	RF_PSStatusHeader_t *hdr = &(pssTable[RF_HASH_PSID(raidPtr, pssPtr->parityStripeID)]);
	RF_ReconParityStripeStatus_t *p = hdr->chain, *pt = NULL;

	while (p) {
		if (p == pssPtr) {
			if (pt)
				pt->next = p->next;
			else
				hdr->chain = p->next;
			p->next = NULL;
			rf_FreePSStatus(raidPtr, p);
			return;
		}
		pt = p;
		p = p->next;
	}
	RF_ASSERT(0);		/* we must find it here */
}
/* deletes an entry from the ps status table after reconstruction has completed */
void 
rf_RemoveFromActiveReconTable(raidPtr, row, psid, which_ru)
	RF_Raid_t *raidPtr;
	RF_RowCol_t row;
	RF_ReconUnitNum_t which_ru;
	RF_StripeNum_t psid;
{
	RF_PSStatusHeader_t *hdr = &(raidPtr->reconControl[row]->pssTable[RF_HASH_PSID(raidPtr, psid)]);
	RF_ReconParityStripeStatus_t *p, *pt;
	RF_CallbackDesc_t *cb, *cb1;

	RF_LOCK_MUTEX(hdr->mutex);
	for (pt = NULL, p = hdr->chain; p; pt = p, p = p->next) {
		if ((p->parityStripeID == psid) && (p->which_ru == which_ru))
			break;
	}
	if (p == NULL) {
		rf_PrintPSStatusTable(raidPtr, row);
	}
	RF_ASSERT(p);		/* it must be there */

	Dprintf2("PSS: deleting pss for psid %ld ru %d\n", psid, which_ru);

	/* delete this entry from the hash chain */
	if (pt)
		pt->next = p->next;
	else
		hdr->chain = p->next;
	p->next = NULL;

	RF_UNLOCK_MUTEX(hdr->mutex);

	/* wakup anyone waiting on the parity stripe ID */
	cb = p->procWaitList;
	p->procWaitList = NULL;
	while (cb) {
		Dprintf1("Waking up access waiting on parity stripe ID %ld\n", p->parityStripeID);
		cb1 = cb->next;
		(cb->callbackFunc) (cb->callbackArg);

		/* THIS IS WHAT THE ORIGINAL CODE HAD... the extra 0 is bogus,
		 * IMHO */
		/* (cb->callbackFunc)(cb->callbackArg, 0); */
		rf_FreeCallbackDesc(cb);
		cb = cb1;
	}

	rf_FreePSStatus(raidPtr, p);
}

RF_ReconParityStripeStatus_t *
rf_AllocPSStatus(raidPtr)
	RF_Raid_t *raidPtr;
{
	RF_ReconParityStripeStatus_t *p;

	RF_FREELIST_GET_INIT_ARG(raidPtr->pss_freelist, p, next, (RF_ReconParityStripeStatus_t *), init_pss, raidPtr);
	if (p) {
		bzero(p->issued, raidPtr->numCol);
	}
	p->next = NULL;
	/* no need to initialize here b/c the only place we're called from is
	 * the above Lookup */
	return (p);
}

void 
rf_FreePSStatus(raidPtr, p)
	RF_Raid_t *raidPtr;
	RF_ReconParityStripeStatus_t *p;
{
	RF_ASSERT(p->procWaitList == NULL);
	RF_ASSERT(p->blockWaitList == NULL);
	RF_ASSERT(p->bufWaitList == NULL);

	RF_FREELIST_FREE_CLEAN_ARG(raidPtr->pss_freelist, p, next, clean_pss, raidPtr);
}

static void 
RealPrintPSStatusTable(raidPtr, pssTable)
	RF_Raid_t *raidPtr;
	RF_PSStatusHeader_t *pssTable;
{
	int     i, j, procsWaiting, blocksWaiting, bufsWaiting;
	RF_ReconParityStripeStatus_t *p;
	RF_CallbackDesc_t *cb;

	printf("\nParity Stripe Status Table\n");
	for (i = 0; i < raidPtr->pssTableSize; i++) {
		for (p = pssTable[i].chain; p; p = p->next) {
			procsWaiting = blocksWaiting = bufsWaiting = 0;
			for (cb = p->procWaitList; cb; cb = cb->next)
				procsWaiting++;
			for (cb = p->blockWaitList; cb; cb = cb->next)
				blocksWaiting++;
			for (cb = p->bufWaitList; cb; cb = cb->next)
				bufsWaiting++;
			printf("PSID %ld RU %d : blockCount %d %d/%d/%d proc/block/buf waiting, issued ",
			    (long) p->parityStripeID, p->which_ru, p->blockCount, procsWaiting, blocksWaiting, bufsWaiting);
			for (j = 0; j < raidPtr->numCol; j++)
				printf("%c", (p->issued[j]) ? '1' : '0');
			if (!p->flags)
				printf(" flags: (none)");
			else {
				if (p->flags & RF_PSS_UNDER_RECON)
					printf(" under-recon");
				if (p->flags & RF_PSS_FORCED_ON_WRITE)
					printf(" forced-w");
				if (p->flags & RF_PSS_FORCED_ON_READ)
					printf(" forced-r");
				if (p->flags & RF_PSS_RECON_BLOCKED)
					printf(" blocked");
				if (p->flags & RF_PSS_BUFFERWAIT)
					printf(" bufwait");
			}
			printf("\n");
		}
	}
}

void 
rf_PrintPSStatusTable(raidPtr, row)
	RF_Raid_t *raidPtr;
	RF_RowCol_t row;
{
	RF_PSStatusHeader_t *pssTable = raidPtr->reconControl[row]->pssTable;
	RealPrintPSStatusTable(raidPtr, pssTable);
}
