/*	$NetBSD: rf_map.c,v 1.5 2000/06/29 00:22:27 oster Exp $	*/

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

/**************************************************************************
 *
 * map.c -- main code for mapping RAID addresses to physical disk addresses
 *
 **************************************************************************/

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_threadstuff.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_map.h>
#include <dev/raidframe/rf_freelist.h>
#include <dev/raidframe/rf_shutdown.h>

static void rf_FreePDAList(RF_PhysDiskAddr_t * start, RF_PhysDiskAddr_t * end, int count);
static void 
rf_FreeASMList(RF_AccessStripeMap_t * start, RF_AccessStripeMap_t * end,
    int count);

/*****************************************************************************************
 *
 * MapAccess -- main 1st order mapping routine.
 *
 * Maps an access in the RAID address space to the corresponding set of physical disk
 * addresses.  The result is returned as a list of AccessStripeMap structures, one per
 * stripe accessed.  Each ASM structure contains a pointer to a list of PhysDiskAddr
 * structures, which describe the physical locations touched by the user access.  Note
 * that this routine returns only static mapping information, i.e. the list of physical
 * addresses returned does not necessarily identify the set of physical locations that
 * will actually be read or written.
 *
 * The routine also maps the parity.  The physical disk location returned always
 * indicates the entire parity unit, even when only a subset of it is being accessed.
 * This is because an access that is not stripe unit aligned but that spans a stripe
 * unit boundary may require access two distinct portions of the parity unit, and we
 * can't yet tell which portion(s) we'll actually need.  We leave it up to the algorithm
 * selection code to decide what subset of the parity unit to access.
 *
 * Note that addresses in the RAID address space must always be maintained as
 * longs, instead of ints.
 *
 * This routine returns NULL if numBlocks is 0
 *
 ****************************************************************************************/

RF_AccessStripeMapHeader_t *
rf_MapAccess(raidPtr, raidAddress, numBlocks, buffer, remap)
	RF_Raid_t *raidPtr;
	RF_RaidAddr_t raidAddress;	/* starting address in RAID address
					 * space */
	RF_SectorCount_t numBlocks;	/* number of blocks in RAID address
					 * space to access */
	caddr_t buffer;		/* buffer to supply/receive data */
	int     remap;		/* 1 => remap addresses to spare space */
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_AccessStripeMapHeader_t *asm_hdr = NULL;
	RF_AccessStripeMap_t *asm_list = NULL, *asm_p = NULL;
	int     faultsTolerated = layoutPtr->map->faultsTolerated;
	RF_RaidAddr_t startAddress = raidAddress;	/* we'll change
							 * raidAddress along the
							 * way */
	RF_RaidAddr_t endAddress = raidAddress + numBlocks;
	RF_RaidDisk_t **disks = raidPtr->Disks;

	RF_PhysDiskAddr_t *pda_p, *pda_q;
	RF_StripeCount_t numStripes = 0;
	RF_RaidAddr_t stripeRealEndAddress, stripeEndAddress, nextStripeUnitAddress;
	RF_RaidAddr_t startAddrWithinStripe, lastRaidAddr;
	RF_StripeCount_t totStripes;
	RF_StripeNum_t stripeID, lastSID, SUID, lastSUID;
	RF_AccessStripeMap_t *asmList, *t_asm;
	RF_PhysDiskAddr_t *pdaList, *t_pda;

	/* allocate all the ASMs and PDAs up front */
	lastRaidAddr = raidAddress + numBlocks - 1;
	stripeID = rf_RaidAddressToStripeID(layoutPtr, raidAddress);
	lastSID = rf_RaidAddressToStripeID(layoutPtr, lastRaidAddr);
	totStripes = lastSID - stripeID + 1;
	SUID = rf_RaidAddressToStripeUnitID(layoutPtr, raidAddress);
	lastSUID = rf_RaidAddressToStripeUnitID(layoutPtr, lastRaidAddr);

	asmList = rf_AllocASMList(totStripes);
	pdaList = rf_AllocPDAList(lastSUID - SUID + 1 + faultsTolerated * totStripes);	/* may also need pda(s)
											 * per stripe for parity */

	if (raidAddress + numBlocks > raidPtr->totalSectors) {
		RF_ERRORMSG1("Unable to map access because offset (%d) was invalid\n",
		    (int) raidAddress);
		return (NULL);
	}
	if (rf_mapDebug)
		rf_PrintRaidAddressInfo(raidPtr, raidAddress, numBlocks);
	for (; raidAddress < endAddress;) {
		/* make the next stripe structure */
		RF_ASSERT(asmList);
		t_asm = asmList;
		asmList = asmList->next;
		bzero((char *) t_asm, sizeof(RF_AccessStripeMap_t));
		if (!asm_p)
			asm_list = asm_p = t_asm;
		else {
			asm_p->next = t_asm;
			asm_p = asm_p->next;
		}
		numStripes++;

		/* map SUs from current location to the end of the stripe */
		asm_p->stripeID =	/* rf_RaidAddressToStripeID(layoutPtr,
		        raidAddress) */ stripeID++;
		stripeRealEndAddress = rf_RaidAddressOfNextStripeBoundary(layoutPtr, raidAddress);
		stripeEndAddress = RF_MIN(endAddress, stripeRealEndAddress);
		asm_p->raidAddress = raidAddress;
		asm_p->endRaidAddress = stripeEndAddress;

		/* map each stripe unit in the stripe */
		pda_p = NULL;
		startAddrWithinStripe = raidAddress;	/* Raid addr of start of
							 * portion of access
							 * that is within this
							 * stripe */
		for (; raidAddress < stripeEndAddress;) {
			RF_ASSERT(pdaList);
			t_pda = pdaList;
			pdaList = pdaList->next;
			bzero((char *) t_pda, sizeof(RF_PhysDiskAddr_t));
			if (!pda_p)
				asm_p->physInfo = pda_p = t_pda;
			else {
				pda_p->next = t_pda;
				pda_p = pda_p->next;
			}

			pda_p->type = RF_PDA_TYPE_DATA;
			(layoutPtr->map->MapSector) (raidPtr, raidAddress, &(pda_p->row), &(pda_p->col), &(pda_p->startSector), remap);

			/* mark any failures we find.  failedPDA is don't-care
			 * if there is more than one failure */
			pda_p->raidAddress = raidAddress;	/* the RAID address
								 * corresponding to this
								 * physical disk address */
			nextStripeUnitAddress = rf_RaidAddressOfNextStripeUnitBoundary(layoutPtr, raidAddress);
			pda_p->numSector = RF_MIN(endAddress, nextStripeUnitAddress) - raidAddress;
			RF_ASSERT(pda_p->numSector != 0);
			rf_ASMCheckStatus(raidPtr, pda_p, asm_p, disks, 0);
			pda_p->bufPtr = buffer + rf_RaidAddressToByte(raidPtr, (raidAddress - startAddress));
			asm_p->totalSectorsAccessed += pda_p->numSector;
			asm_p->numStripeUnitsAccessed++;
			asm_p->origRow = pda_p->row;	/* redundant but
							 * harmless to do this
							 * in every loop
							 * iteration */

			raidAddress = RF_MIN(endAddress, nextStripeUnitAddress);
		}

		/* Map the parity. At this stage, the startSector and
		 * numSector fields for the parity unit are always set to
		 * indicate the entire parity unit. We may modify this after
		 * mapping the data portion. */
		switch (faultsTolerated) {
		case 0:
			break;
		case 1:	/* single fault tolerant */
			RF_ASSERT(pdaList);
			t_pda = pdaList;
			pdaList = pdaList->next;
			bzero((char *) t_pda, sizeof(RF_PhysDiskAddr_t));
			pda_p = asm_p->parityInfo = t_pda;
			pda_p->type = RF_PDA_TYPE_PARITY;
			(layoutPtr->map->MapParity) (raidPtr, rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, startAddrWithinStripe),
			    &(pda_p->row), &(pda_p->col), &(pda_p->startSector), remap);
			pda_p->numSector = layoutPtr->sectorsPerStripeUnit;
			/* raidAddr may be needed to find unit to redirect to */
			pda_p->raidAddress = rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, startAddrWithinStripe);
			rf_ASMCheckStatus(raidPtr, pda_p, asm_p, disks, 1);
			rf_ASMParityAdjust(asm_p->parityInfo, startAddrWithinStripe, endAddress, layoutPtr, asm_p);

			break;
		case 2:	/* two fault tolerant */
			RF_ASSERT(pdaList && pdaList->next);
			t_pda = pdaList;
			pdaList = pdaList->next;
			bzero((char *) t_pda, sizeof(RF_PhysDiskAddr_t));
			pda_p = asm_p->parityInfo = t_pda;
			pda_p->type = RF_PDA_TYPE_PARITY;
			t_pda = pdaList;
			pdaList = pdaList->next;
			bzero((char *) t_pda, sizeof(RF_PhysDiskAddr_t));
			pda_q = asm_p->qInfo = t_pda;
			pda_q->type = RF_PDA_TYPE_Q;
			(layoutPtr->map->MapParity) (raidPtr, rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, startAddrWithinStripe),
			    &(pda_p->row), &(pda_p->col), &(pda_p->startSector), remap);
			(layoutPtr->map->MapQ) (raidPtr, rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, startAddrWithinStripe),
			    &(pda_q->row), &(pda_q->col), &(pda_q->startSector), remap);
			pda_q->numSector = pda_p->numSector = layoutPtr->sectorsPerStripeUnit;
			/* raidAddr may be needed to find unit to redirect to */
			pda_p->raidAddress = rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, startAddrWithinStripe);
			pda_q->raidAddress = rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, startAddrWithinStripe);
			/* failure mode stuff */
			rf_ASMCheckStatus(raidPtr, pda_p, asm_p, disks, 1);
			rf_ASMCheckStatus(raidPtr, pda_q, asm_p, disks, 1);
			rf_ASMParityAdjust(asm_p->parityInfo, startAddrWithinStripe, endAddress, layoutPtr, asm_p);
			rf_ASMParityAdjust(asm_p->qInfo, startAddrWithinStripe, endAddress, layoutPtr, asm_p);
			break;
		}
	}
	RF_ASSERT(asmList == NULL && pdaList == NULL);
	/* make the header structure */
	asm_hdr = rf_AllocAccessStripeMapHeader();
	RF_ASSERT(numStripes == totStripes);
	asm_hdr->numStripes = numStripes;
	asm_hdr->stripeMap = asm_list;

	if (rf_mapDebug)
		rf_PrintAccessStripeMap(asm_hdr);
	return (asm_hdr);
}
/*****************************************************************************************
 * This routine walks through an ASM list and marks the PDAs that have failed.
 * It's called only when a disk failure causes an in-flight DAG to fail.
 * The parity may consist of two components, but we want to use only one failedPDA
 * pointer.  Thus we set failedPDA to point to the first parity component, and rely
 * on the rest of the code to do the right thing with this.
 ****************************************************************************************/

void 
rf_MarkFailuresInASMList(raidPtr, asm_h)
	RF_Raid_t *raidPtr;
	RF_AccessStripeMapHeader_t *asm_h;
{
	RF_RaidDisk_t **disks = raidPtr->Disks;
	RF_AccessStripeMap_t *asmap;
	RF_PhysDiskAddr_t *pda;

	for (asmap = asm_h->stripeMap; asmap; asmap = asmap->next) {
		asmap->numDataFailed = asmap->numParityFailed = asmap->numQFailed = 0;
		asmap->numFailedPDAs = 0;
		bzero((char *) asmap->failedPDAs,
		    RF_MAX_FAILED_PDA * sizeof(RF_PhysDiskAddr_t *));
		for (pda = asmap->physInfo; pda; pda = pda->next) {
			if (RF_DEAD_DISK(disks[pda->row][pda->col].status)) {
				asmap->numDataFailed++;
				asmap->failedPDAs[asmap->numFailedPDAs] = pda;
				asmap->numFailedPDAs++;
			}
		}
		pda = asmap->parityInfo;
		if (pda && RF_DEAD_DISK(disks[pda->row][pda->col].status)) {
			asmap->numParityFailed++;
			asmap->failedPDAs[asmap->numFailedPDAs] = pda;
			asmap->numFailedPDAs++;
		}
		pda = asmap->qInfo;
		if (pda && RF_DEAD_DISK(disks[pda->row][pda->col].status)) {
			asmap->numQFailed++;
			asmap->failedPDAs[asmap->numFailedPDAs] = pda;
			asmap->numFailedPDAs++;
		}
	}
}
/*****************************************************************************************
 *
 * DuplicateASM -- duplicates an ASM and returns the new one
 *
 ****************************************************************************************/
RF_AccessStripeMap_t *
rf_DuplicateASM(asmap)
	RF_AccessStripeMap_t *asmap;
{
	RF_AccessStripeMap_t *new_asm;
	RF_PhysDiskAddr_t *pda, *new_pda, *t_pda;

	new_pda = NULL;
	new_asm = rf_AllocAccessStripeMapComponent();
	bcopy((char *) asmap, (char *) new_asm, sizeof(RF_AccessStripeMap_t));
	new_asm->numFailedPDAs = 0;	/* ??? */
	new_asm->failedPDAs[0] = NULL;
	new_asm->physInfo = NULL;
	new_asm->parityInfo = NULL;
	new_asm->next = NULL;

	for (pda = asmap->physInfo; pda; pda = pda->next) {	/* copy the physInfo
								 * list */
		t_pda = rf_AllocPhysDiskAddr();
		bcopy((char *) pda, (char *) t_pda, sizeof(RF_PhysDiskAddr_t));
		t_pda->next = NULL;
		if (!new_asm->physInfo) {
			new_asm->physInfo = t_pda;
			new_pda = t_pda;
		} else {
			new_pda->next = t_pda;
			new_pda = new_pda->next;
		}
		if (pda == asmap->failedPDAs[0])
			new_asm->failedPDAs[0] = t_pda;
	}
	for (pda = asmap->parityInfo; pda; pda = pda->next) {	/* copy the parityInfo
								 * list */
		t_pda = rf_AllocPhysDiskAddr();
		bcopy((char *) pda, (char *) t_pda, sizeof(RF_PhysDiskAddr_t));
		t_pda->next = NULL;
		if (!new_asm->parityInfo) {
			new_asm->parityInfo = t_pda;
			new_pda = t_pda;
		} else {
			new_pda->next = t_pda;
			new_pda = new_pda->next;
		}
		if (pda == asmap->failedPDAs[0])
			new_asm->failedPDAs[0] = t_pda;
	}
	return (new_asm);
}
/*****************************************************************************************
 *
 * DuplicatePDA -- duplicates a PDA and returns the new one
 *
 ****************************************************************************************/
RF_PhysDiskAddr_t *
rf_DuplicatePDA(pda)
	RF_PhysDiskAddr_t *pda;
{
	RF_PhysDiskAddr_t *new;

	new = rf_AllocPhysDiskAddr();
	bcopy((char *) pda, (char *) new, sizeof(RF_PhysDiskAddr_t));
	return (new);
}
/*****************************************************************************************
 *
 * routines to allocate and free list elements.  All allocation routines zero the
 * structure before returning it.
 *
 * FreePhysDiskAddr is static.  It should never be called directly, because
 * FreeAccessStripeMap takes care of freeing the PhysDiskAddr list.
 *
 ****************************************************************************************/

static RF_FreeList_t *rf_asmhdr_freelist;
#define RF_MAX_FREE_ASMHDR 128
#define RF_ASMHDR_INC       16
#define RF_ASMHDR_INITIAL   32

static RF_FreeList_t *rf_asm_freelist;
#define RF_MAX_FREE_ASM 192
#define RF_ASM_INC       24
#define RF_ASM_INITIAL   64

static RF_FreeList_t *rf_pda_freelist;
#define RF_MAX_FREE_PDA 192
#define RF_PDA_INC       24
#define RF_PDA_INITIAL   64

/* called at shutdown time.  So far, all that is necessary is to release all the free lists */
static void rf_ShutdownMapModule(void *);
static void 
rf_ShutdownMapModule(ignored)
	void   *ignored;
{
	RF_FREELIST_DESTROY(rf_asmhdr_freelist, next, (RF_AccessStripeMapHeader_t *));
	RF_FREELIST_DESTROY(rf_pda_freelist, next, (RF_PhysDiskAddr_t *));
	RF_FREELIST_DESTROY(rf_asm_freelist, next, (RF_AccessStripeMap_t *));
}

int 
rf_ConfigureMapModule(listp)
	RF_ShutdownList_t **listp;
{
	int     rc;

	RF_FREELIST_CREATE(rf_asmhdr_freelist, RF_MAX_FREE_ASMHDR,
	    RF_ASMHDR_INC, sizeof(RF_AccessStripeMapHeader_t));
	if (rf_asmhdr_freelist == NULL) {
		return (ENOMEM);
	}
	RF_FREELIST_CREATE(rf_asm_freelist, RF_MAX_FREE_ASM,
	    RF_ASM_INC, sizeof(RF_AccessStripeMap_t));
	if (rf_asm_freelist == NULL) {
		RF_FREELIST_DESTROY(rf_asmhdr_freelist, next, (RF_AccessStripeMapHeader_t *));
		return (ENOMEM);
	}
	RF_FREELIST_CREATE(rf_pda_freelist, RF_MAX_FREE_PDA,
	    RF_PDA_INC, sizeof(RF_PhysDiskAddr_t));
	if (rf_pda_freelist == NULL) {
		RF_FREELIST_DESTROY(rf_asmhdr_freelist, next, (RF_AccessStripeMapHeader_t *));
		RF_FREELIST_DESTROY(rf_pda_freelist, next, (RF_PhysDiskAddr_t *));
		return (ENOMEM);
	}
	rc = rf_ShutdownCreate(listp, rf_ShutdownMapModule, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		rf_ShutdownMapModule(NULL);
		return (rc);
	}
	RF_FREELIST_PRIME(rf_asmhdr_freelist, RF_ASMHDR_INITIAL, next,
	    (RF_AccessStripeMapHeader_t *));
	RF_FREELIST_PRIME(rf_asm_freelist, RF_ASM_INITIAL, next,
	    (RF_AccessStripeMap_t *));
	RF_FREELIST_PRIME(rf_pda_freelist, RF_PDA_INITIAL, next,
	    (RF_PhysDiskAddr_t *));

	return (0);
}

RF_AccessStripeMapHeader_t *
rf_AllocAccessStripeMapHeader()
{
	RF_AccessStripeMapHeader_t *p;

	RF_FREELIST_GET(rf_asmhdr_freelist, p, next, (RF_AccessStripeMapHeader_t *));
	bzero((char *) p, sizeof(RF_AccessStripeMapHeader_t));

	return (p);
}


void 
rf_FreeAccessStripeMapHeader(p)
	RF_AccessStripeMapHeader_t *p;
{
	RF_FREELIST_FREE(rf_asmhdr_freelist, p, next);
}

RF_PhysDiskAddr_t *
rf_AllocPhysDiskAddr()
{
	RF_PhysDiskAddr_t *p;

	RF_FREELIST_GET(rf_pda_freelist, p, next, (RF_PhysDiskAddr_t *));
	bzero((char *) p, sizeof(RF_PhysDiskAddr_t));

	return (p);
}
/* allocates a list of PDAs, locking the free list only once
 * when we have to call calloc, we do it one component at a time to simplify
 * the process of freeing the list at program shutdown.  This should not be
 * much of a performance hit, because it should be very infrequently executed.
 */
RF_PhysDiskAddr_t *
rf_AllocPDAList(count)
	int     count;
{
	RF_PhysDiskAddr_t *p = NULL;

	RF_FREELIST_GET_N(rf_pda_freelist, p, next, (RF_PhysDiskAddr_t *), count);
	return (p);
}

void 
rf_FreePhysDiskAddr(p)
	RF_PhysDiskAddr_t *p;
{
	RF_FREELIST_FREE(rf_pda_freelist, p, next);
}

static void 
rf_FreePDAList(l_start, l_end, count)
	RF_PhysDiskAddr_t *l_start, *l_end;	/* pointers to start and end
						 * of list */
	int     count;		/* number of elements in list */
{
	RF_FREELIST_FREE_N(rf_pda_freelist, l_start, next, (RF_PhysDiskAddr_t *), count);
}

RF_AccessStripeMap_t *
rf_AllocAccessStripeMapComponent()
{
	RF_AccessStripeMap_t *p;

	RF_FREELIST_GET(rf_asm_freelist, p, next, (RF_AccessStripeMap_t *));
	bzero((char *) p, sizeof(RF_AccessStripeMap_t));

	return (p);
}
/* this is essentially identical to AllocPDAList.  I should combine the two.
 * when we have to call calloc, we do it one component at a time to simplify
 * the process of freeing the list at program shutdown.  This should not be
 * much of a performance hit, because it should be very infrequently executed.
 */
RF_AccessStripeMap_t *
rf_AllocASMList(count)
	int     count;
{
	RF_AccessStripeMap_t *p = NULL;

	RF_FREELIST_GET_N(rf_asm_freelist, p, next, (RF_AccessStripeMap_t *), count);
	return (p);
}

void 
rf_FreeAccessStripeMapComponent(p)
	RF_AccessStripeMap_t *p;
{
	RF_FREELIST_FREE(rf_asm_freelist, p, next);
}

static void 
rf_FreeASMList(l_start, l_end, count)
	RF_AccessStripeMap_t *l_start, *l_end;
	int     count;
{
	RF_FREELIST_FREE_N(rf_asm_freelist, l_start, next, (RF_AccessStripeMap_t *), count);
}

void 
rf_FreeAccessStripeMap(hdr)
	RF_AccessStripeMapHeader_t *hdr;
{
	RF_AccessStripeMap_t *p, *pt = NULL;
	RF_PhysDiskAddr_t *pdp, *trailer, *pdaList = NULL, *pdaEnd = NULL;
	int     count = 0, t, asm_count = 0;

	for (p = hdr->stripeMap; p; p = p->next) {

		/* link the 3 pda lists into the accumulating pda list */

		if (!pdaList)
			pdaList = p->qInfo;
		else
			pdaEnd->next = p->qInfo;
		for (trailer = NULL, pdp = p->qInfo; pdp;) {
			trailer = pdp;
			pdp = pdp->next;
			count++;
		}
		if (trailer)
			pdaEnd = trailer;

		if (!pdaList)
			pdaList = p->parityInfo;
		else
			pdaEnd->next = p->parityInfo;
		for (trailer = NULL, pdp = p->parityInfo; pdp;) {
			trailer = pdp;
			pdp = pdp->next;
			count++;
		}
		if (trailer)
			pdaEnd = trailer;

		if (!pdaList)
			pdaList = p->physInfo;
		else
			pdaEnd->next = p->physInfo;
		for (trailer = NULL, pdp = p->physInfo; pdp;) {
			trailer = pdp;
			pdp = pdp->next;
			count++;
		}
		if (trailer)
			pdaEnd = trailer;

		pt = p;
		asm_count++;
	}

	/* debug only */
	for (t = 0, pdp = pdaList; pdp; pdp = pdp->next)
		t++;
	RF_ASSERT(t == count);

	if (pdaList)
		rf_FreePDAList(pdaList, pdaEnd, count);
	rf_FreeASMList(hdr->stripeMap, pt, asm_count);
	rf_FreeAccessStripeMapHeader(hdr);
}
/* We can't use the large write optimization if there are any failures in the stripe.
 * In the declustered layout, there is no way to immediately determine what disks
 * constitute a stripe, so we actually have to hunt through the stripe looking for failures.
 * The reason we map the parity instead of just using asm->parityInfo->col is because
 * the latter may have been already redirected to a spare drive, which would
 * mess up the computation of the stripe offset.
 *
 * ASSUMES AT MOST ONE FAILURE IN THE STRIPE.
 */
int 
rf_CheckStripeForFailures(raidPtr, asmap)
	RF_Raid_t *raidPtr;
	RF_AccessStripeMap_t *asmap;
{
	RF_RowCol_t trow, tcol, prow, pcol, *diskids, row, i;
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_StripeCount_t stripeOffset;
	int     numFailures;
	RF_RaidAddr_t sosAddr;
	RF_SectorNum_t diskOffset, poffset;
	RF_RowCol_t testrow;

	/* quick out in the fault-free case.  */
	RF_LOCK_MUTEX(raidPtr->mutex);
	numFailures = raidPtr->numFailures;
	RF_UNLOCK_MUTEX(raidPtr->mutex);
	if (numFailures == 0)
		return (0);

	sosAddr = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, asmap->raidAddress);
	row = asmap->physInfo->row;
	(layoutPtr->map->IdentifyStripe) (raidPtr, asmap->raidAddress, &diskids, &testrow);
	(layoutPtr->map->MapParity) (raidPtr, asmap->raidAddress, &prow, &pcol, &poffset, 0);	/* get pcol */

	/* this need not be true if we've redirected the access to a spare in
	 * another row RF_ASSERT(row == testrow); */
	stripeOffset = 0;
	for (i = 0; i < layoutPtr->numDataCol + layoutPtr->numParityCol; i++) {
		if (diskids[i] != pcol) {
			if (RF_DEAD_DISK(raidPtr->Disks[testrow][diskids[i]].status)) {
				if (raidPtr->status[testrow] != rf_rs_reconstructing)
					return (1);
				RF_ASSERT(raidPtr->reconControl[testrow]->fcol == diskids[i]);
				layoutPtr->map->MapSector(raidPtr,
				    sosAddr + stripeOffset * layoutPtr->sectorsPerStripeUnit,
				    &trow, &tcol, &diskOffset, 0);
				RF_ASSERT((trow == testrow) && (tcol == diskids[i]));
				if (!rf_CheckRUReconstructed(raidPtr->reconControl[testrow]->reconMap, diskOffset))
					return (1);
				asmap->flags |= RF_ASM_REDIR_LARGE_WRITE;
				return (0);
			}
			stripeOffset++;
		}
	}
	return (0);
}
/*
   return the number of failed data units in the stripe.
*/

int 
rf_NumFailedDataUnitsInStripe(raidPtr, asmap)
	RF_Raid_t *raidPtr;
	RF_AccessStripeMap_t *asmap;
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_RowCol_t trow, tcol, row, i;
	RF_SectorNum_t diskOffset;
	RF_RaidAddr_t sosAddr;
	int     numFailures;

	/* quick out in the fault-free case.  */
	RF_LOCK_MUTEX(raidPtr->mutex);
	numFailures = raidPtr->numFailures;
	RF_UNLOCK_MUTEX(raidPtr->mutex);
	if (numFailures == 0)
		return (0);
	numFailures = 0;

	sosAddr = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, asmap->raidAddress);
	row = asmap->physInfo->row;
	for (i = 0; i < layoutPtr->numDataCol; i++) {
		(layoutPtr->map->MapSector) (raidPtr, sosAddr + i * layoutPtr->sectorsPerStripeUnit,
		    &trow, &tcol, &diskOffset, 0);
		if (RF_DEAD_DISK(raidPtr->Disks[trow][tcol].status))
			numFailures++;
	}

	return numFailures;
}


/*****************************************************************************************
 *
 * debug routines
 *
 ****************************************************************************************/

void 
rf_PrintAccessStripeMap(asm_h)
	RF_AccessStripeMapHeader_t *asm_h;
{
	rf_PrintFullAccessStripeMap(asm_h, 0);
}

void 
rf_PrintFullAccessStripeMap(asm_h, prbuf)
	RF_AccessStripeMapHeader_t *asm_h;
	int     prbuf;		/* flag to print buffer pointers */
{
	int     i;
	RF_AccessStripeMap_t *asmap = asm_h->stripeMap;
	RF_PhysDiskAddr_t *p;
	printf("%d stripes total\n", (int) asm_h->numStripes);
	for (; asmap; asmap = asmap->next) {
		/* printf("Num failures: %d\n",asmap->numDataFailed); */
		/* printf("Num sectors:
		 * %d\n",(int)asmap->totalSectorsAccessed); */
		printf("Stripe %d (%d sectors), failures: %d data, %d parity: ",
		    (int) asmap->stripeID,
		    (int) asmap->totalSectorsAccessed,
		    (int) asmap->numDataFailed,
		    (int) asmap->numParityFailed);
		if (asmap->parityInfo) {
			printf("Parity [r%d c%d s%d-%d", asmap->parityInfo->row, asmap->parityInfo->col,
			    (int) asmap->parityInfo->startSector,
			    (int) (asmap->parityInfo->startSector +
				asmap->parityInfo->numSector - 1));
			if (prbuf)
				printf(" b0x%lx", (unsigned long) asmap->parityInfo->bufPtr);
			if (asmap->parityInfo->next) {
				printf(", r%d c%d s%d-%d", asmap->parityInfo->next->row,
				    asmap->parityInfo->next->col,
				    (int) asmap->parityInfo->next->startSector,
				    (int) (asmap->parityInfo->next->startSector +
					asmap->parityInfo->next->numSector - 1));
				if (prbuf)
					printf(" b0x%lx", (unsigned long) asmap->parityInfo->next->bufPtr);
				RF_ASSERT(asmap->parityInfo->next->next == NULL);
			}
			printf("]\n\t");
		}
		for (i = 0, p = asmap->physInfo; p; p = p->next, i++) {
			printf("SU r%d c%d s%d-%d ", p->row, p->col, (int) p->startSector,
			    (int) (p->startSector + p->numSector - 1));
			if (prbuf)
				printf("b0x%lx ", (unsigned long) p->bufPtr);
			if (i && !(i & 1))
				printf("\n\t");
		}
		printf("\n");
		p = asm_h->stripeMap->failedPDAs[0];
		if (asm_h->stripeMap->numDataFailed + asm_h->stripeMap->numParityFailed > 1)
			printf("[multiple failures]\n");
		else
			if (asm_h->stripeMap->numDataFailed + asm_h->stripeMap->numParityFailed > 0)
				printf("\t[Failed PDA: r%d c%d s%d-%d]\n", p->row, p->col,
				    (int) p->startSector, (int) (p->startSector + p->numSector - 1));
	}
}

void 
rf_PrintRaidAddressInfo(raidPtr, raidAddr, numBlocks)
	RF_Raid_t *raidPtr;
	RF_RaidAddr_t raidAddr;
	RF_SectorCount_t numBlocks;
{
	RF_RaidLayout_t *layoutPtr = &raidPtr->Layout;
	RF_RaidAddr_t ra, sosAddr = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, raidAddr);

	printf("Raid addrs of SU boundaries from start of stripe to end of access:\n\t");
	for (ra = sosAddr; ra <= raidAddr + numBlocks; ra += layoutPtr->sectorsPerStripeUnit) {
		printf("%d (0x%x), ", (int) ra, (int) ra);
	}
	printf("\n");
	printf("Offset into stripe unit: %d (0x%x)\n",
	    (int) (raidAddr % layoutPtr->sectorsPerStripeUnit),
	    (int) (raidAddr % layoutPtr->sectorsPerStripeUnit));
}
/*
   given a parity descriptor and the starting address within a stripe,
   range restrict the parity descriptor to touch only the correct stuff.
*/
void 
rf_ASMParityAdjust(
    RF_PhysDiskAddr_t * toAdjust,
    RF_StripeNum_t startAddrWithinStripe,
    RF_SectorNum_t endAddress,
    RF_RaidLayout_t * layoutPtr,
    RF_AccessStripeMap_t * asm_p)
{
	RF_PhysDiskAddr_t *new_pda;

	/* when we're accessing only a portion of one stripe unit, we want the
	 * parity descriptor to identify only the chunk of parity associated
	 * with the data.  When the access spans exactly one stripe unit
	 * boundary and is less than a stripe unit in size, it uses two
	 * disjoint regions of the parity unit.  When an access spans more
	 * than one stripe unit boundary, it uses all of the parity unit.
	 * 
	 * To better handle the case where stripe units are small, we may
	 * eventually want to change the 2nd case so that if the SU size is
	 * below some threshold, we just read/write the whole thing instead of
	 * breaking it up into two accesses. */
	if (asm_p->numStripeUnitsAccessed == 1) {
		int     x = (startAddrWithinStripe % layoutPtr->sectorsPerStripeUnit);
		toAdjust->startSector += x;
		toAdjust->raidAddress += x;
		toAdjust->numSector = asm_p->physInfo->numSector;
		RF_ASSERT(toAdjust->numSector != 0);
	} else
		if (asm_p->numStripeUnitsAccessed == 2 && asm_p->totalSectorsAccessed < layoutPtr->sectorsPerStripeUnit) {
			int     x = (startAddrWithinStripe % layoutPtr->sectorsPerStripeUnit);

			/* create a second pda and copy the parity map info
			 * into it */
			RF_ASSERT(toAdjust->next == NULL);
			new_pda = toAdjust->next = rf_AllocPhysDiskAddr();
			*new_pda = *toAdjust;	/* structure assignment */
			new_pda->next = NULL;

			/* adjust the start sector & number of blocks for the
			 * first parity pda */
			toAdjust->startSector += x;
			toAdjust->raidAddress += x;
			toAdjust->numSector = rf_RaidAddressOfNextStripeUnitBoundary(layoutPtr, startAddrWithinStripe) - startAddrWithinStripe;
			RF_ASSERT(toAdjust->numSector != 0);

			/* adjust the second pda */
			new_pda->numSector = endAddress - rf_RaidAddressOfPrevStripeUnitBoundary(layoutPtr, endAddress);
			/* new_pda->raidAddress =
			 * rf_RaidAddressOfNextStripeUnitBoundary(layoutPtr,
			 * toAdjust->raidAddress); */
			RF_ASSERT(new_pda->numSector != 0);
		}
}
/*
   Check if a disk has been spared or failed. If spared,
   redirect the I/O.
   If it has been failed, record it in the asm pointer.
   Fourth arg is whether data or parity.
*/
void 
rf_ASMCheckStatus(
    RF_Raid_t * raidPtr,
    RF_PhysDiskAddr_t * pda_p,
    RF_AccessStripeMap_t * asm_p,
    RF_RaidDisk_t ** disks,
    int parity)
{
	RF_DiskStatus_t dstatus;
	RF_RowCol_t frow, fcol;

	dstatus = disks[pda_p->row][pda_p->col].status;

	if (dstatus == rf_ds_spared) {
		/* if the disk has been spared, redirect access to the spare */
		frow = pda_p->row;
		fcol = pda_p->col;
		pda_p->row = disks[frow][fcol].spareRow;
		pda_p->col = disks[frow][fcol].spareCol;
	} else
		if (dstatus == rf_ds_dist_spared) {
			/* ditto if disk has been spared to dist spare space */
			RF_RowCol_t or = pda_p->row, oc = pda_p->col;
			RF_SectorNum_t oo = pda_p->startSector;

			if (pda_p->type == RF_PDA_TYPE_DATA)
				raidPtr->Layout.map->MapSector(raidPtr, pda_p->raidAddress, &pda_p->row, &pda_p->col, &pda_p->startSector, RF_REMAP);
			else
				raidPtr->Layout.map->MapParity(raidPtr, pda_p->raidAddress, &pda_p->row, &pda_p->col, &pda_p->startSector, RF_REMAP);

			if (rf_mapDebug) {
				printf("Redirected r %d c %d o %d -> r%d c %d o %d\n", or, oc, (int) oo,
				    pda_p->row, pda_p->col, (int) pda_p->startSector);
			}
		} else
			if (RF_DEAD_DISK(dstatus)) {
				/* if the disk is inaccessible, mark the
				 * failure */
				if (parity)
					asm_p->numParityFailed++;
				else {
					asm_p->numDataFailed++;
#if 0
					/* XXX Do we really want this spewing
					 * out on the console? GO */
					printf("DATA_FAILED!\n");
#endif
				}
				asm_p->failedPDAs[asm_p->numFailedPDAs] = pda_p;
				asm_p->numFailedPDAs++;
#if 0
				switch (asm_p->numParityFailed + asm_p->numDataFailed) {
				case 1:
					asm_p->failedPDAs[0] = pda_p;
					break;
				case 2:
					asm_p->failedPDAs[1] = pda_p;
				default:
					break;
				}
#endif
			}
	/* the redirected access should never span a stripe unit boundary */
	RF_ASSERT(rf_RaidAddressToStripeUnitID(&raidPtr->Layout, pda_p->raidAddress) ==
	    rf_RaidAddressToStripeUnitID(&raidPtr->Layout, pda_p->raidAddress + pda_p->numSector - 1));
	RF_ASSERT(pda_p->col != -1);
}
