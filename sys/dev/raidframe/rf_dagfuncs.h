/*	$FreeBSD$ */
/*	$NetBSD: rf_dagfuncs.h,v 1.4 2000/03/30 13:39:07 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland, William V. Courtright II, Jim Zelenka
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
 * dagfuncs.h -- header file for DAG node execution routines
 *
 ****************************************************************************************/

#ifndef _RF__RF_DAGFUNCS_H_
#define _RF__RF_DAGFUNCS_H_

int     rf_ConfigureDAGFuncs(RF_ShutdownList_t ** listp);
int     rf_TerminateFunc(RF_DagNode_t * node);
int     rf_TerminateUndoFunc(RF_DagNode_t * node);
int     rf_DiskReadMirrorIdleFunc(RF_DagNode_t * node);
int     rf_DiskReadMirrorPartitionFunc(RF_DagNode_t * node);
int     rf_DiskReadMirrorUndoFunc(RF_DagNode_t * node);
int     rf_ParityLogUpdateFunc(RF_DagNode_t * node);
int     rf_ParityLogOverwriteFunc(RF_DagNode_t * node);
int     rf_ParityLogUpdateUndoFunc(RF_DagNode_t * node);
int     rf_ParityLogOverwriteUndoFunc(RF_DagNode_t * node);
int     rf_NullNodeFunc(RF_DagNode_t * node);
int     rf_NullNodeUndoFunc(RF_DagNode_t * node);
int     rf_DiskReadFuncForThreads(RF_DagNode_t * node);
int     rf_DiskWriteFuncForThreads(RF_DagNode_t * node);
int     rf_DiskUndoFunc(RF_DagNode_t * node);
int     rf_DiskUnlockFuncForThreads(RF_DagNode_t * node);
int     rf_GenericWakeupFunc(RF_DagNode_t * node, int status);
int     rf_RegularXorFunc(RF_DagNode_t * node);
int     rf_SimpleXorFunc(RF_DagNode_t * node);
int     rf_RecoveryXorFunc(RF_DagNode_t * node);
int 
rf_XorIntoBuffer(RF_Raid_t * raidPtr, RF_PhysDiskAddr_t * pda, char *srcbuf,
    char *targbuf, void *bp);
int     rf_bxor(char *src, char *dest, int len, void *bp);
int 
rf_longword_bxor(unsigned long *src, unsigned long *dest, int len, void *bp);
int 
rf_longword_bxor3(unsigned long *dest, unsigned long *a, unsigned long *b, 
		  unsigned long *c, int len, void *bp);
int 
rf_bxor3(unsigned char *dst, unsigned char *a, unsigned char *b,
    unsigned char *c, unsigned long len, void *bp);

/* function ptrs defined in ConfigureDAGFuncs() */
extern int (*rf_DiskReadFunc) (RF_DagNode_t *);
extern int (*rf_DiskWriteFunc) (RF_DagNode_t *);
extern int (*rf_DiskReadUndoFunc) (RF_DagNode_t *);
extern int (*rf_DiskWriteUndoFunc) (RF_DagNode_t *);
extern int (*rf_DiskUnlockFunc) (RF_DagNode_t *);
extern int (*rf_DiskUnlockUndoFunc) (RF_DagNode_t *);
extern int (*rf_SimpleXorUndoFunc) (RF_DagNode_t *);
extern int (*rf_RegularXorUndoFunc) (RF_DagNode_t *);
extern int (*rf_RecoveryXorUndoFunc) (RF_DagNode_t *);

/* macros for manipulating the param[3] in a read or write node */
#define RF_CREATE_PARAM3(pri, lk, unlk, wru) (((RF_uint64)(((wru&0xFFFFFF)<<8)|((lk)?0x10:0)|((unlk)?0x20:0)|((pri)&0xF)) ))
#define RF_EXTRACT_PRIORITY(_x_)     ((((unsigned) ((unsigned long)(_x_))) >> 0) & 0x0F)
#define RF_EXTRACT_LOCK_FLAG(_x_)    ((((unsigned) ((unsigned long)(_x_))) >> 4) & 0x1)
#define RF_EXTRACT_UNLOCK_FLAG(_x_)  ((((unsigned) ((unsigned long)(_x_))) >> 5) & 0x1)
#define RF_EXTRACT_RU(_x_)           ((((unsigned) ((unsigned long)(_x_))) >> 8) & 0xFFFFFF)

#endif				/* !_RF__RF_DAGFUNCS_H_ */
