/*	$FreeBSD$ */
/*	$NetBSD: rf_paritylog.h,v 1.3 1999/02/05 00:06:14 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: William V. Courtright II
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

/* header file for parity log
 *
 */

#ifndef _RF__RF_PARITYLOG_H_
#define _RF__RF_PARITYLOG_H_

#include <dev/raidframe/rf_types.h>

#define RF_DEFAULT_NUM_SECTORS_PER_LOG 64

typedef int RF_RegionId_t;

typedef enum RF_ParityRecordType_e {
	RF_STOP,
	RF_UPDATE,
	RF_OVERWRITE
}       RF_ParityRecordType_t;

struct RF_CommonLogData_s {
	RF_DECLARE_MUTEX(mutex)	/* protects cnt */
	int     cnt;		/* when 0, time to call wakeFunc */
	RF_Raid_t *raidPtr;
/*   int                    (*wakeFunc)(RF_Buf_t); */
	int     (*wakeFunc) (RF_DagNode_t * node, int status);
	void   *wakeArg;
	RF_AccTraceEntry_t *tracerec;
	RF_Etimer_t startTime;
	caddr_t bufPtr;
	RF_ParityRecordType_t operation;
	RF_CommonLogData_t *next;
};

struct RF_ParityLogData_s {
	RF_RegionId_t regionID;	/* this struct guaranteed to span a single
				 * region */
	int     bufOffset;	/* offset from common->bufPtr */
	RF_PhysDiskAddr_t diskAddress;
	RF_CommonLogData_t *common;	/* info shared by one or more
					 * parityLogData structs */
	RF_ParityLogData_t *next;
	RF_ParityLogData_t *prev;
};

struct RF_ParityLogAppendQueue_s {
	RF_DECLARE_MUTEX(mutex)
};

struct RF_ParityLogRecord_s {
	RF_PhysDiskAddr_t parityAddr;
	RF_ParityRecordType_t operation;
};

struct RF_ParityLog_s {
	RF_RegionId_t regionID;
	int     numRecords;
	int     diskOffset;
	RF_ParityLogRecord_t *records;
	caddr_t bufPtr;
	RF_ParityLog_t *next;
};

struct RF_ParityLogQueue_s {
	RF_DECLARE_MUTEX(mutex)
	RF_ParityLog_t *parityLogs;
};

struct RF_RegionBufferQueue_s {
	RF_DECLARE_MUTEX(mutex)
	RF_DECLARE_COND(cond)
	int     bufferSize;
	int     totalBuffers;	/* size of array 'buffers' */
	int     availableBuffers;	/* num available 'buffers' */
	int     emptyBuffersIndex;	/* stick next freed buffer here */
	int     availBuffersIndex;	/* grab next buffer from here */
	caddr_t *buffers;	/* array buffers used to hold parity */
};
#define RF_PLOG_CREATED   (1<<0)/* thread is created */
#define RF_PLOG_RUNNING   (1<<1)/* thread is running */
#define RF_PLOG_TERMINATE (1<<2)/* thread is terminated (should exit) */
#define RF_PLOG_SHUTDOWN  (1<<3)/* thread is aware and exiting/exited */

struct RF_ParityLogDiskQueue_s {
	RF_DECLARE_MUTEX(mutex)	/* protects all vars in this struct */
	RF_DECLARE_COND(cond)
	int     threadState;	/* is thread running, should it shutdown  (see
				 * above) */
	RF_ParityLog_t *flushQueue;	/* list of parity logs to be flushed
					 * to log disk */
	RF_ParityLog_t *reintQueue;	/* list of parity logs waiting to be
					 * reintegrated */
	RF_ParityLogData_t *bufHead;	/* head of FIFO list of log data,
					 * waiting on a buffer */
	RF_ParityLogData_t *bufTail;	/* tail of FIFO list of log data,
					 * waiting on a buffer */
	RF_ParityLogData_t *reintHead;	/* head of FIFO list of log data,
					 * waiting on reintegration */
	RF_ParityLogData_t *reintTail;	/* tail of FIFO list of log data,
					 * waiting on reintegration */
	RF_ParityLogData_t *logBlockHead;	/* queue of work, blocked
						 * until a log is available */
	RF_ParityLogData_t *logBlockTail;
	RF_ParityLogData_t *reintBlockHead;	/* queue of work, blocked
						 * until reintegration is
						 * complete */
	RF_ParityLogData_t *reintBlockTail;
	RF_CommonLogData_t *freeCommonList;	/* list of unused common data
						 * structs */
	RF_ParityLogData_t *freeDataList;	/* list of unused log data
						 * structs */
};

struct RF_DiskMap_s {
	RF_PhysDiskAddr_t parityAddr;
	RF_ParityRecordType_t operation;
};

struct RF_RegionInfo_s {
	RF_DECLARE_MUTEX(mutex)	/* protects: diskCount, diskMap,
				 * loggingEnabled, coreLog */
	RF_DECLARE_MUTEX(reintMutex)	/* protects: reintInProgress */
	int     reintInProgress;/* flag used to suspend flushing operations */
	RF_SectorCount_t capacity;	/* capacity of this region in sectors */
	RF_SectorNum_t regionStartAddr;	/* starting disk address for this
					 * region */
	RF_SectorNum_t parityStartAddr;	/* starting disk address for this
					 * region */
	RF_SectorCount_t numSectorsParity;	/* number of parity sectors
						 * protected by this region */
	RF_SectorCount_t diskCount;	/* num of sectors written to this
					 * region's disk log */
	RF_DiskMap_t *diskMap;	/* in-core map of what's in this region's disk
				 * log */
	int     loggingEnabled;	/* logging enable for this region */
	RF_ParityLog_t *coreLog;/* in-core log for this region */
};

RF_ParityLogData_t *
rf_CreateParityLogData(RF_ParityRecordType_t operation,
    RF_PhysDiskAddr_t * pda, caddr_t bufPtr, RF_Raid_t * raidPtr,
    int (*wakeFunc) (RF_DagNode_t * node, int status),
    void *wakeArg, RF_AccTraceEntry_t * tracerec,
    RF_Etimer_t startTime);
	RF_ParityLogData_t *rf_SearchAndDequeueParityLogData(RF_Raid_t * raidPtr,
            RF_RegionId_t regionID, RF_ParityLogData_t ** head,
            RF_ParityLogData_t ** tail, int ignoreLocks);
	void    rf_ReleaseParityLogs(RF_Raid_t * raidPtr, RF_ParityLog_t * firstLog);
	int     rf_ParityLogAppend(RF_ParityLogData_t * logData, int finish,
            RF_ParityLog_t ** incomingLog, int clearReintFlag);
	void    rf_EnableParityLogging(RF_Raid_t * raidPtr);

#endif				/* !_RF__RF_PARITYLOG_H_ */
