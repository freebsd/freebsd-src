/*	$FreeBSD$ */
/*	$NetBSD: rf_types.h,v 1.6 1999/09/05 03:05:55 oster Exp $	*/
/*
 * rf_types.h
 */
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
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
/***********************************************************
 *
 * rf_types.h -- standard types for RAIDframe
 *
 ***********************************************************/

#ifndef _RF__RF_TYPES_H_
#define _RF__RF_TYPES_H_


#include <dev/raidframe/rf_archs.h>

#include <sys/errno.h>
#include <sys/types.h>

#include <sys/uio.h>
#include <sys/param.h>
#include <sys/lock.h>

/*
 * First, define system-dependent types and constants.
 *
 * If the machine is big-endian, RF_BIG_ENDIAN should be 1.
 * Otherwise, it should be 0.
 *
 * The various integer types should be self-explanatory; we
 * use these elsewhere to avoid size confusion.
 *
 * LONGSHIFT is lg(sizeof(long)) (that is, log base two of sizeof(long)
 *
 */

#include <sys/types.h>
#include <machine/endian.h>
#include <machine/limits.h>

#if BYTE_ORDER == BIG_ENDIAN
#define RF_IS_BIG_ENDIAN    1
#elif BYTE_ORDER == LITTLE_ENDIAN
#define RF_IS_BIG_ENDIAN    0
#else
#error byte order not defined
#endif
typedef int8_t RF_int8;
typedef u_int8_t RF_uint8;
typedef int16_t RF_int16;
typedef u_int16_t RF_uint16;
typedef int32_t RF_int32;
typedef u_int32_t RF_uint32;
typedef int64_t RF_int64;
typedef u_int64_t RF_uint64;
#if LONG_BIT == 32
#define RF_LONGSHIFT        2
#elif LONG_BIT == 64
#define RF_LONGSHIFT        3
#elif defined(__i386__)
#define RF_LONGSHIFT	    2
#elif defined(__alpha__)
#define RF_LONGSHIFT	    3
#else
#error word size not defined
#endif

/*
 * These are just zero and non-zero. We don't use "TRUE"
 * and "FALSE" because there's too much nonsense trying
 * to get them defined exactly once on every platform, given
 * the different places they may be defined in system header
 * files.
 */
#define RF_TRUE  1
#define RF_FALSE 0

/*
 * Now, some generic types
 */
typedef RF_uint64 RF_IoCount_t;
typedef RF_uint64 RF_Offset_t;
typedef RF_uint32 RF_PSSFlags_t;
typedef RF_uint64 RF_SectorCount_t;
typedef RF_uint64 RF_StripeCount_t;
typedef RF_int64 RF_SectorNum_t;/* these are unsigned so we can set them to
				 * (-1) for "uninitialized" */
typedef RF_int64 RF_StripeNum_t;
typedef RF_int64 RF_RaidAddr_t;
typedef int RF_RowCol_t;	/* unsigned so it can be (-1) */
typedef RF_int64 RF_HeadSepLimit_t;
typedef RF_int64 RF_ReconUnitCount_t;
typedef int RF_ReconUnitNum_t;

typedef char RF_ParityConfig_t;

typedef char RF_DiskQueueType_t[1024];
#define RF_DISK_QUEUE_TYPE_NONE ""

/* values for the 'type' field in a reconstruction buffer */
typedef int RF_RbufType_t;
#define RF_RBUF_TYPE_EXCLUSIVE   0	/* this buf assigned exclusively to
					 * one disk */
#define RF_RBUF_TYPE_FLOATING    1	/* this is a floating recon buf */
#define RF_RBUF_TYPE_FORCED      2	/* this rbuf was allocated to complete
					 * a forced recon */

typedef char RF_IoType_t;
#define RF_IO_TYPE_READ          'r'
#define RF_IO_TYPE_WRITE         'w'
#define RF_IO_TYPE_NOP           'n'
#define RF_IO_IS_R_OR_W(_type_) (((_type_) == RF_IO_TYPE_READ) \
                                || ((_type_) == RF_IO_TYPE_WRITE))

typedef void (*RF_VoidFuncPtr) (void *,...);

typedef RF_uint32 RF_AccessStripeMapFlags_t;
typedef RF_uint32 RF_DiskQueueDataFlags_t;
typedef RF_uint32 RF_DiskQueueFlags_t;
typedef RF_uint32 RF_RaidAccessFlags_t;

#define RF_DISKQUEUE_DATA_FLAGS_NONE ((RF_DiskQueueDataFlags_t)0)

typedef struct RF_AccessStripeMap_s RF_AccessStripeMap_t;
typedef struct RF_AccessStripeMapHeader_s RF_AccessStripeMapHeader_t;
typedef struct RF_AllocListElem_s RF_AllocListElem_t;
typedef struct RF_CallbackDesc_s RF_CallbackDesc_t;
typedef struct RF_ChunkDesc_s RF_ChunkDesc_t;
typedef struct RF_CommonLogData_s RF_CommonLogData_t;
typedef struct RF_Config_s RF_Config_t;
typedef struct RF_CumulativeStats_s RF_CumulativeStats_t;
typedef struct RF_DagHeader_s RF_DagHeader_t;
typedef struct RF_DagList_s RF_DagList_t;
typedef struct RF_DagNode_s RF_DagNode_t;
typedef struct RF_DeclusteredConfigInfo_s RF_DeclusteredConfigInfo_t;
typedef struct RF_DiskId_s RF_DiskId_t;
typedef struct RF_DiskMap_s RF_DiskMap_t;
typedef struct RF_DiskQueue_s RF_DiskQueue_t;
typedef struct RF_DiskQueueData_s RF_DiskQueueData_t;
typedef struct RF_DiskQueueSW_s RF_DiskQueueSW_t;
typedef struct RF_Etimer_s RF_Etimer_t;
typedef struct RF_EventCreate_s RF_EventCreate_t;
typedef struct RF_FreeList_s RF_FreeList_t;
typedef struct RF_LockReqDesc_s RF_LockReqDesc_t;
typedef struct RF_LockTableEntry_s RF_LockTableEntry_t;
typedef struct RF_MCPair_s RF_MCPair_t;
typedef struct RF_OwnerInfo_s RF_OwnerInfo_t;
typedef struct RF_ParityLog_s RF_ParityLog_t;
typedef struct RF_ParityLogAppendQueue_s RF_ParityLogAppendQueue_t;
typedef struct RF_ParityLogData_s RF_ParityLogData_t;
typedef struct RF_ParityLogDiskQueue_s RF_ParityLogDiskQueue_t;
typedef struct RF_ParityLogQueue_s RF_ParityLogQueue_t;
typedef struct RF_ParityLogRecord_s RF_ParityLogRecord_t;
typedef struct RF_PerDiskReconCtrl_s RF_PerDiskReconCtrl_t;
typedef struct RF_PSStatusHeader_s RF_PSStatusHeader_t;
typedef struct RF_PhysDiskAddr_s RF_PhysDiskAddr_t;
typedef struct RF_PropHeader_s RF_PropHeader_t;
typedef struct RF_Raid_s RF_Raid_t;
typedef struct RF_RaidAccessDesc_s RF_RaidAccessDesc_t;
typedef struct RF_RaidDisk_s RF_RaidDisk_t;
typedef struct RF_RaidLayout_s RF_RaidLayout_t;
typedef struct RF_RaidReconDesc_s RF_RaidReconDesc_t;
typedef struct RF_ReconBuffer_s RF_ReconBuffer_t;
typedef struct RF_ReconConfig_s RF_ReconConfig_t;
typedef struct RF_ReconCtrl_s RF_ReconCtrl_t;
typedef struct RF_ReconDoneProc_s RF_ReconDoneProc_t;
typedef struct RF_ReconEvent_s RF_ReconEvent_t;
typedef struct RF_ReconMap_s RF_ReconMap_t;
typedef struct RF_ReconMapListElem_s RF_ReconMapListElem_t;
typedef struct RF_ReconParityStripeStatus_s RF_ReconParityStripeStatus_t;
typedef struct RF_RedFuncs_s RF_RedFuncs_t;
typedef struct RF_RegionBufferQueue_s RF_RegionBufferQueue_t;
typedef struct RF_RegionInfo_s RF_RegionInfo_t;
typedef struct RF_ShutdownList_s RF_ShutdownList_t;
typedef struct RF_SpareTableEntry_s RF_SpareTableEntry_t;
typedef struct RF_SparetWait_s RF_SparetWait_t;
typedef struct RF_StripeLockDesc_s RF_StripeLockDesc_t;
typedef struct RF_ThreadGroup_s RF_ThreadGroup_t;
typedef struct RF_ThroughputStats_s RF_ThroughputStats_t;

/*
 * Important assumptions regarding ordering of the states in this list
 * have been made!!!
 * Before disturbing this ordering, look at code in rf_states.c
 */
typedef enum RF_AccessState_e {
	/* original states */
	rf_QuiesceState,	/* handles queisence for reconstruction */
	rf_IncrAccessesCountState,	/* count accesses in flight */
	rf_DecrAccessesCountState,
	rf_MapState,		/* map access to disk addresses */
	rf_LockState,		/* take stripe locks */
	rf_CreateDAGState,	/* create DAGs */
	rf_ExecuteDAGState,	/* execute DAGs */
	rf_ProcessDAGState,	/* DAGs are completing- check if correct, or
				 * if we need to retry */
	rf_CleanupState,	/* release stripe locks, clean up */
	rf_LastState		/* must be the last state */
}       RF_AccessState_t;
#define RF_MAXROW    10		/* these are arbitrary and can be modified at
				 * will */
#define RF_MAXCOL    40
#define RF_MAXSPARE  10
#define RF_MAXDBGV   75		/* max number of debug variables */

union RF_GenericParam_u {
	void   *p;
	RF_uint64 v;
};
typedef union RF_GenericParam_u RF_DagParam_t;
typedef union RF_GenericParam_u RF_CBParam_t;

#if defined(__FreeBSD__) && __FreeBSD_version > 500005
typedef struct bio	*RF_Buf_t;
#else
typedef struct buf	*RF_Buf_t;
#endif
#endif				/* _RF__RF_TYPES_H_ */
