/*	$FreeBSD$ */
/*	$NetBSD: rf_layout.c,v 1.9 2001/01/27 19:34:43 oster Exp $	*/
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

/* rf_layout.c -- driver code dealing with layout and mapping issues
 */

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_archs.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_configure.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_desc.h>
#include <dev/raidframe/rf_decluster.h>
#include <dev/raidframe/rf_pq.h>
#include <dev/raidframe/rf_declusterPQ.h>
#include <dev/raidframe/rf_raid0.h>
#include <dev/raidframe/rf_raid1.h>
#include <dev/raidframe/rf_raid4.h>
#include <dev/raidframe/rf_raid5.h>
#include <dev/raidframe/rf_states.h>
#if RF_INCLUDE_RAID5_RS > 0
#include <dev/raidframe/rf_raid5_rotatedspare.h>
#endif				/* RF_INCLUDE_RAID5_RS > 0 */
#if RF_INCLUDE_CHAINDECLUSTER > 0
#include <dev/raidframe/rf_chaindecluster.h>
#endif				/* RF_INCLUDE_CHAINDECLUSTER > 0 */
#if RF_INCLUDE_INTERDECLUSTER > 0
#include <dev/raidframe/rf_interdecluster.h>
#endif				/* RF_INCLUDE_INTERDECLUSTER > 0 */
#if RF_INCLUDE_PARITYLOGGING > 0
#include <dev/raidframe/rf_paritylogging.h>
#endif				/* RF_INCLUDE_PARITYLOGGING > 0 */
#if RF_INCLUDE_EVENODD > 0
#include <dev/raidframe/rf_evenodd.h>
#endif				/* RF_INCLUDE_EVENODD > 0 */
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_driver.h>
#include <dev/raidframe/rf_parityscan.h>
#include <dev/raidframe/rf_reconbuffer.h>
#include <dev/raidframe/rf_reconutil.h>

/***********************************************************************
 *
 * the layout switch defines all the layouts that are supported.
 *    fields are: layout ID, init routine, shutdown routine, map
 *    sector, map parity, identify stripe, dag selection, map stripeid
 *    to parity stripe id (optional), num faults tolerated, special
 *    flags.
 *
 ***********************************************************************/

static RF_AccessState_t DefaultStates[] = {rf_QuiesceState,
					   rf_IncrAccessesCountState, 
					   rf_MapState, 
					   rf_LockState, 
					   rf_CreateDAGState,
					   rf_ExecuteDAGState, 
					   rf_ProcessDAGState, 
					   rf_DecrAccessesCountState,
					   rf_CleanupState, 
					   rf_LastState};

#define RF_NU(a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p) a,b,c,d,e,f,g,h,i,j,k,l,m,n,o,p

/* Note that if you add any new RAID types to this list, that you must
   also update the mapsw[] table in the raidctl sources */

static RF_LayoutSW_t mapsw[] = {
#if RF_INCLUDE_PARITY_DECLUSTERING > 0
	/* parity declustering */
	{'T', "Parity declustering",
		RF_NU(
		    rf_ConfigureDeclustered,
		    rf_MapSectorDeclustered, rf_MapParityDeclustered, NULL,
		    rf_IdentifyStripeDeclustered,
		    rf_RaidFiveDagSelect,
		    rf_MapSIDToPSIDDeclustered,
		    rf_GetDefaultHeadSepLimitDeclustered,
		    rf_GetDefaultNumFloatingReconBuffersDeclustered,
		    NULL, NULL,
		    rf_SubmitReconBufferBasic,
		    rf_VerifyParityBasic,
		    1,
		    DefaultStates,
		    0)
	},
#endif

#if RF_INCLUDE_PARITY_DECLUSTERING_DS > 0
	/* parity declustering with distributed sparing */
	{'D', "Distributed sparing parity declustering",
		RF_NU(
		    rf_ConfigureDeclusteredDS,
		    rf_MapSectorDeclustered, rf_MapParityDeclustered, NULL,
		    rf_IdentifyStripeDeclustered,
		    rf_RaidFiveDagSelect,
		    rf_MapSIDToPSIDDeclustered,
		    rf_GetDefaultHeadSepLimitDeclustered,
		    rf_GetDefaultNumFloatingReconBuffersDeclustered,
		    rf_GetNumSpareRUsDeclustered, rf_InstallSpareTable,
		    rf_SubmitReconBufferBasic,
		    rf_VerifyParityBasic,
		    1,
		    DefaultStates,
		    RF_DISTRIBUTE_SPARE | RF_BD_DECLUSTERED)
	},
#endif

#if RF_INCLUDE_DECL_PQ > 0
	/* declustered P+Q */
	{'Q', "Declustered P+Q",
		RF_NU(
		    rf_ConfigureDeclusteredPQ,
		    rf_MapSectorDeclusteredPQ, rf_MapParityDeclusteredPQ, rf_MapQDeclusteredPQ,
		    rf_IdentifyStripeDeclusteredPQ,
		    rf_PQDagSelect,
		    rf_MapSIDToPSIDDeclustered,
		    rf_GetDefaultHeadSepLimitDeclustered,
		    rf_GetDefaultNumFloatingReconBuffersPQ,
		    NULL, NULL,
		    NULL,
		    rf_VerifyParityBasic,
		    2,
		    DefaultStates,
		    0)
	},
#endif				/* RF_INCLUDE_DECL_PQ > 0 */

#if RF_INCLUDE_RAID5_RS > 0
	/* RAID 5 with rotated sparing */
	{'R', "RAID Level 5 rotated sparing",
		RF_NU(
		    rf_ConfigureRAID5_RS,
		    rf_MapSectorRAID5_RS, rf_MapParityRAID5_RS, NULL,
		    rf_IdentifyStripeRAID5_RS,
		    rf_RaidFiveDagSelect,
		    rf_MapSIDToPSIDRAID5_RS,
		    rf_GetDefaultHeadSepLimitRAID5,
		    rf_GetDefaultNumFloatingReconBuffersRAID5,
		    rf_GetNumSpareRUsRAID5_RS, NULL,
		    rf_SubmitReconBufferBasic,
		    rf_VerifyParityBasic,
		    1,
		    DefaultStates,
		    RF_DISTRIBUTE_SPARE)
	},
#endif				/* RF_INCLUDE_RAID5_RS > 0 */

#if RF_INCLUDE_CHAINDECLUSTER > 0
	/* Chained Declustering */
	{'C', "Chained Declustering",
		RF_NU(
		    rf_ConfigureChainDecluster,
		    rf_MapSectorChainDecluster, rf_MapParityChainDecluster, NULL,
		    rf_IdentifyStripeChainDecluster,
		    rf_RAIDCDagSelect,
		    rf_MapSIDToPSIDChainDecluster,
		    NULL,
		    NULL,
		    rf_GetNumSpareRUsChainDecluster, NULL,
		    rf_SubmitReconBufferBasic,
		    rf_VerifyParityBasic,
		    1,
		    DefaultStates,
		    0)
	},
#endif				/* RF_INCLUDE_CHAINDECLUSTER > 0 */

#if RF_INCLUDE_INTERDECLUSTER > 0
	/* Interleaved Declustering */
	{'I', "Interleaved Declustering",
		RF_NU(
		    rf_ConfigureInterDecluster,
		    rf_MapSectorInterDecluster, rf_MapParityInterDecluster, NULL,
		    rf_IdentifyStripeInterDecluster,
		    rf_RAIDIDagSelect,
		    rf_MapSIDToPSIDInterDecluster,
		    rf_GetDefaultHeadSepLimitInterDecluster,
		    rf_GetDefaultNumFloatingReconBuffersInterDecluster,
		    rf_GetNumSpareRUsInterDecluster, NULL,
		    rf_SubmitReconBufferBasic,
		    rf_VerifyParityBasic,
		    1,
		    DefaultStates,
		    RF_DISTRIBUTE_SPARE)
	},
#endif				/* RF_INCLUDE_INTERDECLUSTER > 0 */

#if RF_INCLUDE_RAID0 > 0
	/* RAID level 0 */
	{'0', "RAID Level 0",
		RF_NU(
		    rf_ConfigureRAID0,
		    rf_MapSectorRAID0, rf_MapParityRAID0, NULL,
		    rf_IdentifyStripeRAID0,
		    rf_RAID0DagSelect,
		    rf_MapSIDToPSIDRAID0,
		    NULL,
		    NULL,
		    NULL, NULL,
		    NULL,
		    rf_VerifyParityRAID0,
		    0,
		    DefaultStates,
		    0)
	},
#endif				/* RF_INCLUDE_RAID0 > 0 */

#if RF_INCLUDE_RAID1 > 0
	/* RAID level 1 */
	{'1', "RAID Level 1",
		RF_NU(
		    rf_ConfigureRAID1,
		    rf_MapSectorRAID1, rf_MapParityRAID1, NULL,
		    rf_IdentifyStripeRAID1,
		    rf_RAID1DagSelect,
		    rf_MapSIDToPSIDRAID1,
		    NULL,
		    NULL,
		    NULL, NULL,
		    rf_SubmitReconBufferRAID1,
		    rf_VerifyParityRAID1,
		    1,
		    DefaultStates,
		    0)
	},
#endif				/* RF_INCLUDE_RAID1 > 0 */

#if RF_INCLUDE_RAID4 > 0
	/* RAID level 4 */
	{'4', "RAID Level 4",
		RF_NU(
		    rf_ConfigureRAID4,
		    rf_MapSectorRAID4, rf_MapParityRAID4, NULL,
		    rf_IdentifyStripeRAID4,
		    rf_RaidFiveDagSelect,
		    rf_MapSIDToPSIDRAID4,
		    rf_GetDefaultHeadSepLimitRAID4,
		    rf_GetDefaultNumFloatingReconBuffersRAID4,
		    NULL, NULL,
		    rf_SubmitReconBufferBasic,
		    rf_VerifyParityBasic,
		    1,
		    DefaultStates,
		    0)
	},
#endif				/* RF_INCLUDE_RAID4 > 0 */

#if RF_INCLUDE_RAID5 > 0
	/* RAID level 5 */
	{'5', "RAID Level 5",
		RF_NU(
		    rf_ConfigureRAID5,
		    rf_MapSectorRAID5, rf_MapParityRAID5, NULL,
		    rf_IdentifyStripeRAID5,
		    rf_RaidFiveDagSelect,
		    rf_MapSIDToPSIDRAID5,
		    rf_GetDefaultHeadSepLimitRAID5,
		    rf_GetDefaultNumFloatingReconBuffersRAID5,
		    NULL, NULL,
		    rf_SubmitReconBufferBasic,
		    rf_VerifyParityBasic,
		    1,
		    DefaultStates,
		    0)
	},
#endif				/* RF_INCLUDE_RAID5 > 0 */

#if RF_INCLUDE_EVENODD > 0
	/* Evenodd */
	{'E', "EvenOdd",
		RF_NU(
		    rf_ConfigureEvenOdd,
		    rf_MapSectorRAID5, rf_MapParityEvenOdd, rf_MapEEvenOdd,
		    rf_IdentifyStripeEvenOdd,
		    rf_EODagSelect,
		    rf_MapSIDToPSIDRAID5,
		    NULL,
		    NULL,
		    NULL, NULL,
		    NULL,	/* no reconstruction, yet */
		    rf_VerifyParityEvenOdd,
		    2,
		    DefaultStates,
		    0)
	},
#endif				/* RF_INCLUDE_EVENODD > 0 */

#if RF_INCLUDE_EVENODD > 0
	/* Declustered Evenodd */
	{'e', "Declustered EvenOdd",
		RF_NU(
		    rf_ConfigureDeclusteredPQ,
		    rf_MapSectorDeclusteredPQ, rf_MapParityDeclusteredPQ, rf_MapQDeclusteredPQ,
		    rf_IdentifyStripeDeclusteredPQ,
		    rf_EODagSelect,
		    rf_MapSIDToPSIDRAID5,
		    rf_GetDefaultHeadSepLimitDeclustered,
		    rf_GetDefaultNumFloatingReconBuffersPQ,
		    NULL, NULL,
		    NULL,	/* no reconstruction, yet */
		    rf_VerifyParityEvenOdd,
		    2,
		    DefaultStates,
		    0)
	},
#endif				/* RF_INCLUDE_EVENODD > 0 */

#if RF_INCLUDE_PARITYLOGGING > 0
	/* parity logging */
	{'L', "Parity logging",
		RF_NU(
		    rf_ConfigureParityLogging,
		    rf_MapSectorParityLogging, rf_MapParityParityLogging, NULL,
		    rf_IdentifyStripeParityLogging,
		    rf_ParityLoggingDagSelect,
		    rf_MapSIDToPSIDParityLogging,
		    rf_GetDefaultHeadSepLimitParityLogging,
		    rf_GetDefaultNumFloatingReconBuffersParityLogging,
		    NULL, NULL,
		    rf_SubmitReconBufferBasic,
		    NULL,
		    1,
		    DefaultStates,
		    0)
	},
#endif				/* RF_INCLUDE_PARITYLOGGING > 0 */

	/* end-of-list marker */
	{'\0', NULL,
		RF_NU(
		    NULL,
		    NULL, NULL, NULL,
		    NULL,
		    NULL,
		    NULL,
		    NULL,
		    NULL,
		    NULL, NULL,
		    NULL,
		    NULL,
		    0,
		    NULL,
		    0)
	}
};

RF_LayoutSW_t *
rf_GetLayout(RF_ParityConfig_t parityConfig)
{
	RF_LayoutSW_t *p;

	/* look up the specific layout */
	for (p = &mapsw[0]; p->parityConfig; p++)
		if (p->parityConfig == parityConfig)
			break;
	if (!p->parityConfig)
		return (NULL);
	RF_ASSERT(p->parityConfig == parityConfig);
	return (p);
}

/*****************************************************************************
 *
 * ConfigureLayout --
 *
 * read the configuration file and set up the RAID layout parameters.
 * After reading common params, invokes the layout-specific
 * configuration routine to finish the configuration.
 *
 ****************************************************************************/
int 
rf_ConfigureLayout(
    RF_ShutdownList_t ** listp,
    RF_Raid_t * raidPtr,
    RF_Config_t * cfgPtr)
{
	RF_RaidLayout_t *layoutPtr = &(raidPtr->Layout);
	RF_ParityConfig_t parityConfig;
	RF_LayoutSW_t *p;
	int     retval;

	layoutPtr->sectorsPerStripeUnit = cfgPtr->sectPerSU;
	layoutPtr->SUsPerPU = cfgPtr->SUsPerPU;
	layoutPtr->SUsPerRU = cfgPtr->SUsPerRU;
	parityConfig = cfgPtr->parityConfig;

	if (layoutPtr->sectorsPerStripeUnit <= 0) {
		RF_ERRORMSG2("raid%d: Invalid sectorsPerStripeUnit: %d\n",
			     raidPtr->raidid, 
			     (int)layoutPtr->sectorsPerStripeUnit );
		return (EINVAL); 
	}

	layoutPtr->stripeUnitsPerDisk = raidPtr->sectorsPerDisk / layoutPtr->sectorsPerStripeUnit;

	p = rf_GetLayout(parityConfig);
	if (p == NULL) {
		RF_ERRORMSG1("Unknown parity configuration '%c'", parityConfig);
		return (EINVAL);
	}
	RF_ASSERT(p->parityConfig == parityConfig);
	layoutPtr->map = p;

	/* initialize the specific layout */

	retval = (p->Configure) (listp, raidPtr, cfgPtr);

	if (retval)
		return (retval);

	layoutPtr->dataBytesPerStripe = layoutPtr->dataSectorsPerStripe << raidPtr->logBytesPerSector;
	raidPtr->sectorsPerDisk = layoutPtr->stripeUnitsPerDisk * layoutPtr->sectorsPerStripeUnit;

	if (rf_forceNumFloatingReconBufs >= 0) {
		raidPtr->numFloatingReconBufs = rf_forceNumFloatingReconBufs;
	} else {
		raidPtr->numFloatingReconBufs = rf_GetDefaultNumFloatingReconBuffers(raidPtr);
	}

	if (rf_forceHeadSepLimit >= 0) {
		raidPtr->headSepLimit = rf_forceHeadSepLimit;
	} else {
		raidPtr->headSepLimit = rf_GetDefaultHeadSepLimit(raidPtr);
	}

	printf("RAIDFRAME: Configure (%s): total number of sectors is %lu (%lu MB)\n",
	    layoutPtr->map->configName,
	    (unsigned long) raidPtr->totalSectors,
	    (unsigned long) (raidPtr->totalSectors / 1024 * (1 << raidPtr->logBytesPerSector) / 1024));
	if (raidPtr->headSepLimit >= 0) {
		printf("RAIDFRAME(%s): Using %ld floating recon bufs with head sep limit %ld\n",
		    layoutPtr->map->configName, (long) raidPtr->numFloatingReconBufs, (long) raidPtr->headSepLimit);
	} else {
		printf("RAIDFRAME(%s): Using %ld floating recon bufs with no head sep limit\n",
		    layoutPtr->map->configName, (long) raidPtr->numFloatingReconBufs);
	}

	return (0);
}
/* typically there is a 1-1 mapping between stripes and parity stripes.
 * however, the declustering code supports packing multiple stripes into
 * a single parity stripe, so as to increase the size of the reconstruction
 * unit without affecting the size of the stripe unit.  This routine finds
 * the parity stripe identifier associated with a stripe ID.  There is also
 * a RaidAddressToParityStripeID macro in layout.h
 */
RF_StripeNum_t 
rf_MapStripeIDToParityStripeID(layoutPtr, stripeID, which_ru)
	RF_RaidLayout_t *layoutPtr;
	RF_StripeNum_t stripeID;
	RF_ReconUnitNum_t *which_ru;
{
	RF_StripeNum_t parityStripeID;

	/* quick exit in the common case of SUsPerPU==1 */
	if ((layoutPtr->SUsPerPU == 1) || !layoutPtr->map->MapSIDToPSID) {
		*which_ru = 0;
		return (stripeID);
	} else {
		(layoutPtr->map->MapSIDToPSID) (layoutPtr, stripeID, &parityStripeID, which_ru);
	}
	return (parityStripeID);
}
