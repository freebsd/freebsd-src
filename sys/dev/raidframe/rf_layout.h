/*	$FreeBSD$ */
/*	$NetBSD: rf_layout.h,v 1.5 2001/01/26 04:14:14 oster Exp $	*/
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

/* rf_layout.h -- header file defining layout data structures
 */

#ifndef _RF__RF_LAYOUT_H_
#define _RF__RF_LAYOUT_H_

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_archs.h>
#include <dev/raidframe/rf_alloclist.h>

#ifndef _KERNEL
#include <stdio.h>
#endif

/*****************************************************************************************
 *
 * This structure identifies all layout-specific operations and parameters.
 *
 ****************************************************************************************/

typedef struct RF_LayoutSW_s {
	RF_ParityConfig_t parityConfig;
	const char *configName;

#ifndef _KERNEL
	/* layout-specific parsing */
	int     (*MakeLayoutSpecific) (FILE * fp, RF_Config_t * cfgPtr, void *arg);
	void   *makeLayoutSpecificArg;
#endif				/* !KERNEL */

#if RF_UTILITY == 0
	/* initialization routine */
	int     (*Configure) (RF_ShutdownList_t ** shutdownListp, RF_Raid_t * raidPtr, RF_Config_t * cfgPtr);

	/* routine to map RAID sector address -> physical (row, col, offset) */
	void    (*MapSector) (RF_Raid_t * raidPtr, RF_RaidAddr_t raidSector,
	            RF_RowCol_t * row, RF_RowCol_t * col, RF_SectorNum_t * diskSector, int remap);

	/* routine to map RAID sector address -> physical (r,c,o) of parity
	 * unit */
	void    (*MapParity) (RF_Raid_t * raidPtr, RF_RaidAddr_t raidSector,
	            RF_RowCol_t * row, RF_RowCol_t * col, RF_SectorNum_t * diskSector, int remap);

	/* routine to map RAID sector address -> physical (r,c,o) of Q unit */
	void    (*MapQ) (RF_Raid_t * raidPtr, RF_RaidAddr_t raidSector, RF_RowCol_t * row,
	            RF_RowCol_t * col, RF_SectorNum_t * diskSector, int remap);

	/* routine to identify the disks comprising a stripe */
	void    (*IdentifyStripe) (RF_Raid_t * raidPtr, RF_RaidAddr_t addr,
	            RF_RowCol_t ** diskids, RF_RowCol_t * outRow);

	/* routine to select a dag */
	void    (*SelectionFunc) (RF_Raid_t * raidPtr, RF_IoType_t type,
	            RF_AccessStripeMap_t * asmap,
	            RF_VoidFuncPtr *);
#if 0
	void    (**createFunc) (RF_Raid_t *,
	            RF_AccessStripeMap_t *,
	            RF_DagHeader_t *, void *,
	            RF_RaidAccessFlags_t,
	            RF_AllocListElem_t *);

#endif

	/* map a stripe ID to a parity stripe ID.  This is typically the
	 * identity mapping */
	void    (*MapSIDToPSID) (RF_RaidLayout_t * layoutPtr, RF_StripeNum_t stripeID,
	            RF_StripeNum_t * psID, RF_ReconUnitNum_t * which_ru);

	/* get default head separation limit (may be NULL) */
	        RF_HeadSepLimit_t(*GetDefaultHeadSepLimit) (RF_Raid_t * raidPtr);

	/* get default num recon buffers (may be NULL) */
	int     (*GetDefaultNumFloatingReconBuffers) (RF_Raid_t * raidPtr);

	/* get number of spare recon units (may be NULL) */
	        RF_ReconUnitCount_t(*GetNumSpareRUs) (RF_Raid_t * raidPtr);

	/* spare table installation (may be NULL) */
	int     (*InstallSpareTable) (RF_Raid_t * raidPtr, RF_RowCol_t frow, RF_RowCol_t fcol);

	/* recon buffer submission function */
	int     (*SubmitReconBuffer) (RF_ReconBuffer_t * rbuf, int keep_it,
	            int use_committed);

	/*
         * verify that parity information for a stripe is correct
         * see rf_parityscan.h for return vals
         */
	int     (*VerifyParity) (RF_Raid_t * raidPtr, RF_RaidAddr_t raidAddr,
	            RF_PhysDiskAddr_t * parityPDA, int correct_it, RF_RaidAccessFlags_t flags);

	/* number of faults tolerated by this mapping */
	int     faultsTolerated;

	/* states to step through in an access. Must end with "LastState". The
	 * default is DefaultStates in rf_layout.c */
	RF_AccessState_t *states;

	RF_AccessStripeMapFlags_t flags;
#endif				/* RF_UTILITY == 0 */
}       RF_LayoutSW_t;
/* enables remapping to spare location under dist sparing */
#define RF_REMAP       1
#define RF_DONT_REMAP  0

/*
 * Flags values for RF_AccessStripeMapFlags_t
 */
#define RF_NO_STRIPE_LOCKS   0x0001	/* suppress stripe locks */
#define RF_DISTRIBUTE_SPARE  0x0002	/* distribute spare space in archs
					 * that support it */
#define RF_BD_DECLUSTERED    0x0004	/* declustering uses block designs */

/*************************************************************************
 *
 * this structure forms the layout component of the main Raid
 * structure.  It describes everything needed to define and perform
 * the mapping of logical RAID addresses <-> physical disk addresses.
 *
 *************************************************************************/
struct RF_RaidLayout_s {
	/* configuration parameters */
	RF_SectorCount_t sectorsPerStripeUnit;	/* number of sectors in one
						 * stripe unit */
	RF_StripeCount_t SUsPerPU;	/* stripe units per parity unit */
	RF_StripeCount_t SUsPerRU;	/* stripe units per reconstruction
					 * unit */

	/* redundant-but-useful info computed from the above, used in all
	 * layouts */
	RF_StripeCount_t numStripe;	/* total number of stripes in the
					 * array */
	RF_SectorCount_t dataSectorsPerStripe;
	RF_StripeCount_t dataStripeUnitsPerDisk;
	u_int   bytesPerStripeUnit;
	u_int   dataBytesPerStripe;
	RF_StripeCount_t numDataCol;	/* number of SUs of data per stripe
					 * (name here is a la RAID4) */
	RF_StripeCount_t numParityCol;	/* number of SUs of parity per stripe.
					 * Always 1 for now */
	RF_StripeCount_t numParityLogCol;	/* number of SUs of parity log
						 * per stripe.  Always 1 for
						 * now */
	RF_StripeCount_t stripeUnitsPerDisk;

	RF_LayoutSW_t *map;	/* ptr to struct holding mapping fns and
				 * information */
	void   *layoutSpecificInfo;	/* ptr to a structure holding
					 * layout-specific params */
};
/*****************************************************************************************
 *
 * The mapping code returns a pointer to a list of AccessStripeMap structures, which
 * describes all the mapping information about an access.  The list contains one
 * AccessStripeMap structure per stripe touched by the access.  Each element in the list
 * contains a stripe identifier and a pointer to a list of PhysDiskAddr structuress.  Each
 * element in this latter list describes the physical location of a stripe unit accessed
 * within the corresponding stripe.
 *
 ****************************************************************************************/

#define RF_PDA_TYPE_DATA   0
#define RF_PDA_TYPE_PARITY 1
#define RF_PDA_TYPE_Q      2

struct RF_PhysDiskAddr_s {
	RF_RowCol_t row, col;	/* disk identifier */
	RF_SectorNum_t startSector;	/* sector offset into the disk */
	RF_SectorCount_t numSector;	/* number of sectors accessed */
	int     type;		/* used by higher levels: currently, data,
				 * parity, or q */
	caddr_t bufPtr;		/* pointer to buffer supplying/receiving data */
	RF_RaidAddr_t raidAddress;	/* raid address corresponding to this
					 * physical disk address */
	RF_PhysDiskAddr_t *next;
};
#define RF_MAX_FAILED_PDA RF_MAXCOL

struct RF_AccessStripeMap_s {
	RF_StripeNum_t stripeID;/* the stripe index */
	RF_RaidAddr_t raidAddress;	/* the starting raid address within
					 * this stripe */
	RF_RaidAddr_t endRaidAddress;	/* raid address one sector past the
					 * end of the access */
	RF_SectorCount_t totalSectorsAccessed;	/* total num sectors
						 * identified in physInfo list */
	RF_StripeCount_t numStripeUnitsAccessed;	/* total num elements in
							 * physInfo list */
	int     numDataFailed;	/* number of failed data disks accessed */
	int     numParityFailed;/* number of failed parity disks accessed (0
				 * or 1) */
	int     numQFailed;	/* number of failed Q units accessed (0 or 1) */
	RF_AccessStripeMapFlags_t flags;	/* various flags */
#if 0
	RF_PhysDiskAddr_t *failedPDA;	/* points to the PDA that has failed */
	RF_PhysDiskAddr_t *failedPDAtwo;	/* points to the second PDA
						 * that has failed, if any */
#else
	int     numFailedPDAs;	/* number of failed phys addrs */
	RF_PhysDiskAddr_t *failedPDAs[RF_MAX_FAILED_PDA];	/* array of failed phys
								 * addrs */
#endif
	RF_PhysDiskAddr_t *physInfo;	/* a list of PhysDiskAddr structs */
	RF_PhysDiskAddr_t *parityInfo;	/* list of physical addrs for the
					 * parity (P of P + Q ) */
	RF_PhysDiskAddr_t *qInfo;	/* list of physical addrs for the Q of
					 * P + Q */
	RF_LockReqDesc_t lockReqDesc;	/* used for stripe locking */
	RF_RowCol_t origRow;	/* the original row:  we may redirect the acc
				 * to a different row */
	RF_AccessStripeMap_t *next;
};
/* flag values */
#define RF_ASM_REDIR_LARGE_WRITE   0x00000001	/* allows large-write creation
						 * code to redirect failed
						 * accs */
#define RF_ASM_BAILOUT_DAG_USED    0x00000002	/* allows us to detect
						 * recursive calls to the
						 * bailout write dag */
#define RF_ASM_FLAGS_LOCK_TRIED    0x00000004	/* we've acquired the lock on
						 * the first parity range in
						 * this parity stripe */
#define RF_ASM_FLAGS_LOCK_TRIED2   0x00000008	/* we've acquired the lock on
						 * the 2nd   parity range in
						 * this parity stripe */
#define RF_ASM_FLAGS_FORCE_TRIED   0x00000010	/* we've done the force-recon
						 * call on this parity stripe */
#define RF_ASM_FLAGS_RECON_BLOCKED 0x00000020	/* we blocked recon => we must
						 * unblock it later */

struct RF_AccessStripeMapHeader_s {
	RF_StripeCount_t numStripes;	/* total number of stripes touched by
					 * this acc */
	RF_AccessStripeMap_t *stripeMap;	/* pointer to the actual map.
						 * Also used for making lists */
	RF_AccessStripeMapHeader_t *next;
};
/*****************************************************************************************
 *
 * various routines mapping addresses in the RAID address space.  These work across
 * all layouts.  DON'T PUT ANY LAYOUT-SPECIFIC CODE HERE.
 *
 ****************************************************************************************/

/* return the identifier of the stripe containing the given address */
#define rf_RaidAddressToStripeID(_layoutPtr_, _addr_) \
  ( ((_addr_) / (_layoutPtr_)->sectorsPerStripeUnit) / (_layoutPtr_)->numDataCol )

/* return the raid address of the start of the indicates stripe ID */
#define rf_StripeIDToRaidAddress(_layoutPtr_, _sid_) \
  ( ((_sid_) * (_layoutPtr_)->sectorsPerStripeUnit) * (_layoutPtr_)->numDataCol )

/* return the identifier of the stripe containing the given stripe unit id */
#define rf_StripeUnitIDToStripeID(_layoutPtr_, _addr_) \
  ( (_addr_) / (_layoutPtr_)->numDataCol )

/* return the identifier of the stripe unit containing the given address */
#define rf_RaidAddressToStripeUnitID(_layoutPtr_, _addr_) \
  ( ((_addr_) / (_layoutPtr_)->sectorsPerStripeUnit) )

/* return the RAID address of next stripe boundary beyond the given address */
#define rf_RaidAddressOfNextStripeBoundary(_layoutPtr_, _addr_) \
  ( (((_addr_)/(_layoutPtr_)->dataSectorsPerStripe)+1) * (_layoutPtr_)->dataSectorsPerStripe )

/* return the RAID address of the start of the stripe containing the given address */
#define rf_RaidAddressOfPrevStripeBoundary(_layoutPtr_, _addr_) \
  ( (((_addr_)/(_layoutPtr_)->dataSectorsPerStripe)+0) * (_layoutPtr_)->dataSectorsPerStripe )

/* return the RAID address of next stripe unit boundary beyond the given address */
#define rf_RaidAddressOfNextStripeUnitBoundary(_layoutPtr_, _addr_) \
  ( (((_addr_)/(_layoutPtr_)->sectorsPerStripeUnit)+1L)*(_layoutPtr_)->sectorsPerStripeUnit )

/* return the RAID address of the start of the stripe unit containing RAID address _addr_ */
#define rf_RaidAddressOfPrevStripeUnitBoundary(_layoutPtr_, _addr_) \
  ( (((_addr_)/(_layoutPtr_)->sectorsPerStripeUnit)+0)*(_layoutPtr_)->sectorsPerStripeUnit )

/* returns the offset into the stripe.  used by RaidAddressStripeAligned */
#define rf_RaidAddressStripeOffset(_layoutPtr_, _addr_) \
  ( (_addr_) % ((_layoutPtr_)->dataSectorsPerStripe) )

/* returns the offset into the stripe unit.  */
#define rf_StripeUnitOffset(_layoutPtr_, _addr_) \
  ( (_addr_) % ((_layoutPtr_)->sectorsPerStripeUnit) )

/* returns nonzero if the given RAID address is stripe-aligned */
#define rf_RaidAddressStripeAligned( __layoutPtr__, __addr__ ) \
  ( rf_RaidAddressStripeOffset(__layoutPtr__, __addr__) == 0 )

/* returns nonzero if the given address is stripe-unit aligned */
#define rf_StripeUnitAligned( __layoutPtr__, __addr__ ) \
  ( rf_StripeUnitOffset(__layoutPtr__, __addr__) == 0 )

/* convert an address expressed in RAID blocks to/from an addr expressed in bytes */
#define rf_RaidAddressToByte(_raidPtr_, _addr_) \
  ( (_addr_) << ( (_raidPtr_)->logBytesPerSector ) )

#define rf_ByteToRaidAddress(_raidPtr_, _addr_) \
  ( (_addr_) >> ( (_raidPtr_)->logBytesPerSector ) )

/* convert a raid address to/from a parity stripe ID.  Conversion to raid address is easy,
 * since we're asking for the address of the first sector in the parity stripe.  Conversion to a
 * parity stripe ID is more complex, since stripes are not contiguously allocated in
 * parity stripes.
 */
#define rf_RaidAddressToParityStripeID(_layoutPtr_, _addr_, _ru_num_) \
  rf_MapStripeIDToParityStripeID( (_layoutPtr_), rf_RaidAddressToStripeID( (_layoutPtr_), (_addr_) ), (_ru_num_) )

#define rf_ParityStripeIDToRaidAddress(_layoutPtr_, _psid_) \
  ( (_psid_) * (_layoutPtr_)->SUsPerPU * (_layoutPtr_)->numDataCol * (_layoutPtr_)->sectorsPerStripeUnit )

RF_LayoutSW_t *rf_GetLayout(RF_ParityConfig_t parityConfig);
int 
rf_ConfigureLayout(RF_ShutdownList_t ** listp, RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr);
RF_StripeNum_t 
rf_MapStripeIDToParityStripeID(RF_RaidLayout_t * layoutPtr,
    RF_StripeNum_t stripeID, RF_ReconUnitNum_t * which_ru);

#endif				/* !_RF__RF_LAYOUT_H_ */
