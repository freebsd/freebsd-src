/*	$FreeBSD$ */
/*	$NetBSD: rf_evenodd_dagfuncs.c,v 1.7 2001/01/26 03:50:53 oster Exp $	*/
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: ChangMing Wu
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

/*
 * Code for RAID-EVENODD  architecture.
 */

#include <dev/raidframe/rf_archs.h>

#if RF_INCLUDE_EVENODD > 0

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_raid.h>
#include <dev/raidframe/rf_dag.h>
#include <dev/raidframe/rf_dagffrd.h>
#include <dev/raidframe/rf_dagffwr.h>
#include <dev/raidframe/rf_dagdegrd.h>
#include <dev/raidframe/rf_dagdegwr.h>
#include <dev/raidframe/rf_dagutils.h>
#include <dev/raidframe/rf_dagfuncs.h>
#include <dev/raidframe/rf_etimer.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_configure.h>
#include <dev/raidframe/rf_parityscan.h>
#include <dev/raidframe/rf_evenodd.h>
#include <dev/raidframe/rf_evenodd_dagfuncs.h>

/* These redundant functions are for small write */
RF_RedFuncs_t rf_EOSmallWritePFuncs = {rf_RegularXorFunc, "Regular Old-New P", rf_SimpleXorFunc, "Simple Old-New P"};
RF_RedFuncs_t rf_EOSmallWriteEFuncs = {rf_RegularONEFunc, "Regular Old-New E", rf_SimpleONEFunc, "Regular Old-New E"};
/* These redundant functions are for degraded read */
RF_RedFuncs_t rf_eoPRecoveryFuncs = {rf_RecoveryXorFunc, "Recovery Xr", rf_RecoveryXorFunc, "Recovery Xr"};
RF_RedFuncs_t rf_eoERecoveryFuncs = {rf_RecoveryEFunc, "Recovery E Func", rf_RecoveryEFunc, "Recovery E Func"};
/**********************************************************************************************
 *   the following encoding node functions is used in  EO_000_CreateLargeWriteDAG
 **********************************************************************************************/
int 
rf_RegularPEFunc(node)
	RF_DagNode_t *node;
{
	rf_RegularESubroutine(node, node->results[1]);
	rf_RegularXorFunc(node);/* does the wakeup here! */
#if 1
	return (0);		/* XXX This was missing... GO */
#endif
}


/************************************************************************************************
 *  For EO_001_CreateSmallWriteDAG, there are (i)RegularONEFunc() and (ii)SimpleONEFunc() to
 *  be used. The previous case is when write access at least sectors of full stripe unit.
 *  The later function is used when the write access two stripe units but with total sectors
 *  less than sectors per SU. In this case, the access of parity and 'E' are shown as disconnected
 *  areas in their stripe unit and  parity write and 'E' write are both devided into two distinct
 *  writes( totally four). This simple old-new write and regular old-new write happen as in RAID-5
 ************************************************************************************************/

/* Algorithm:
     1. Store the difference of old data and new data in the Rod buffer.
     2. then encode this buffer into the buffer which already have old 'E' information inside it,
	the result can be shown to be the new 'E' information.
     3. xor the Wnd buffer into the difference buffer to recover the  original old data.
   Here we have another alternative: to allocate a temporary buffer for storing the difference of
   old data and new data, then encode temp buf into old 'E' buf to form new 'E', but this approach
   take the same speed as the previous, and need more memory.
*/
int 
rf_RegularONEFunc(node)
	RF_DagNode_t *node;
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[node->numParams - 1].p;
	RF_RaidLayout_t *layoutPtr = (RF_RaidLayout_t *) & raidPtr->Layout;
	int     EpdaIndex = (node->numParams - 1) / 2 - 1;	/* the parameter of node
								 * where you can find
								 * e-pda */
	int     i, k, retcode = 0;
	int     suoffset, length;
	RF_RowCol_t scol;
	char   *srcbuf, *destbuf;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	RF_Etimer_t timer;
	RF_PhysDiskAddr_t *pda, *EPDA = (RF_PhysDiskAddr_t *) node->params[EpdaIndex].p;
	int     ESUOffset = rf_StripeUnitOffset(layoutPtr, EPDA->startSector);	/* generally zero  */

	RF_ASSERT(EPDA->type == RF_PDA_TYPE_Q);
	RF_ASSERT(ESUOffset == 0);

	RF_ETIMER_START(timer);

	/* Xor the Wnd buffer into Rod buffer, the difference of old data and
	 * new data is stored in Rod buffer */
	for (k = 0; k < EpdaIndex; k += 2) {
		length = rf_RaidAddressToByte(raidPtr, ((RF_PhysDiskAddr_t *) node->params[k].p)->numSector);
		retcode = rf_bxor(node->params[k + EpdaIndex + 3].p, node->params[k + 1].p, length, node->dagHdr->bp);
	}
	/* Start to encoding the buffer storing the difference of old data and
	 * new data into 'E' buffer  */
	for (i = 0; i < EpdaIndex; i += 2)
		if (node->params[i + 1].p != node->results[0]) {	/* results[0] is buf ptr
									 * of E */
			pda = (RF_PhysDiskAddr_t *) node->params[i].p;
			srcbuf = (char *) node->params[i + 1].p;
			scol = rf_EUCol(layoutPtr, pda->raidAddress);
			suoffset = rf_StripeUnitOffset(layoutPtr, pda->startSector);
			destbuf = ((char *) node->results[0]) + rf_RaidAddressToByte(raidPtr, suoffset);
			rf_e_encToBuf(raidPtr, scol, srcbuf, RF_EO_MATRIX_DIM - 2, destbuf, pda->numSector);
		}
	/* Recover the original old data to be used by parity encoding
	 * function in XorNode */
	for (k = 0; k < EpdaIndex; k += 2) {
		length = rf_RaidAddressToByte(raidPtr, ((RF_PhysDiskAddr_t *) node->params[k].p)->numSector);
		retcode = rf_bxor(node->params[k + EpdaIndex + 3].p, node->params[k + 1].p, length, node->dagHdr->bp);
	}
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->q_us += RF_ETIMER_VAL_US(timer);
	rf_GenericWakeupFunc(node, 0);
#if 1
	return (0);		/* XXX this was missing.. GO */
#endif
}

int 
rf_SimpleONEFunc(node)
	RF_DagNode_t *node;
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[node->numParams - 1].p;
	RF_RaidLayout_t *layoutPtr = (RF_RaidLayout_t *) & raidPtr->Layout;
	RF_PhysDiskAddr_t *pda = (RF_PhysDiskAddr_t *) node->params[0].p;
	int     retcode = 0;
	char   *srcbuf, *destbuf;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	int     length;
	RF_RowCol_t scol;
	RF_Etimer_t timer;

	RF_ASSERT(((RF_PhysDiskAddr_t *) node->params[2].p)->type == RF_PDA_TYPE_Q);
	if (node->dagHdr->status == rf_enable) {
		RF_ETIMER_START(timer);
		length = rf_RaidAddressToByte(raidPtr, ((RF_PhysDiskAddr_t *) node->params[4].p)->numSector);	/* this is a pda of
														 * writeDataNodes */
		/* bxor to buffer of readDataNodes */
		retcode = rf_bxor(node->params[5].p, node->params[1].p, length, node->dagHdr->bp);
		/* find out the corresponding colume in encoding matrix for
		 * write colume to be encoded into redundant disk 'E' */
		scol = rf_EUCol(layoutPtr, pda->raidAddress);
		srcbuf = node->params[1].p;
		destbuf = node->params[3].p;
		/* Start encoding process */
		rf_e_encToBuf(raidPtr, scol, srcbuf, RF_EO_MATRIX_DIM - 2, destbuf, pda->numSector);
		rf_bxor(node->params[5].p, node->params[1].p, length, node->dagHdr->bp);
		RF_ETIMER_STOP(timer);
		RF_ETIMER_EVAL(timer);
		tracerec->q_us += RF_ETIMER_VAL_US(timer);

	}
	return (rf_GenericWakeupFunc(node, retcode));	/* call wake func
							 * explicitly since no
							 * I/O in this node */
}


/****** called by rf_RegularPEFunc(node) and rf_RegularEFunc(node) in f.f. large write  ********/
void 
rf_RegularESubroutine(node, ebuf)
	RF_DagNode_t *node;
	char   *ebuf;
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[node->numParams - 1].p;
	RF_RaidLayout_t *layoutPtr = (RF_RaidLayout_t *) & raidPtr->Layout;
	RF_PhysDiskAddr_t *pda;
	int     i, suoffset;
	RF_RowCol_t scol;
	char   *srcbuf, *destbuf;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	RF_Etimer_t timer;

	RF_ETIMER_START(timer);
	for (i = 0; i < node->numParams - 2; i += 2) {
		RF_ASSERT(node->params[i + 1].p != ebuf);
		pda = (RF_PhysDiskAddr_t *) node->params[i].p;
		suoffset = rf_StripeUnitOffset(layoutPtr, pda->startSector);
		scol = rf_EUCol(layoutPtr, pda->raidAddress);
		srcbuf = (char *) node->params[i + 1].p;
		destbuf = ebuf + rf_RaidAddressToByte(raidPtr, suoffset);
		rf_e_encToBuf(raidPtr, scol, srcbuf, RF_EO_MATRIX_DIM - 2, destbuf, pda->numSector);
	}
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->xor_us += RF_ETIMER_VAL_US(timer);
}


/*******************************************************************************************
 *			 Used in  EO_001_CreateLargeWriteDAG
 ******************************************************************************************/
int 
rf_RegularEFunc(node)
	RF_DagNode_t *node;
{
	rf_RegularESubroutine(node, node->results[0]);
	rf_GenericWakeupFunc(node, 0);
#if 1
	return (0);		/* XXX this was missing?.. GO */
#endif
}
/*******************************************************************************************
 * This degraded function allow only two case:
 *  1. when write access the full failed stripe unit, then the access can be more than
 *     one tripe units.
 *  2. when write access only part of the failed SU, we assume accesses of more than
 *     one stripe unit is not allowed so that the write can be dealt with like a
 *     large write.
 *  The following function is based on these assumptions. So except in the second case,
 *  it looks the same as a large write encodeing function. But this is not exactly the
 *  normal way for doing a degraded write, since raidframe have to break cases of access
 *  other than the above two into smaller accesses. We may have to change
 *  DegrESubroutin in the future.
 *******************************************************************************************/
void 
rf_DegrESubroutine(node, ebuf)
	RF_DagNode_t *node;
	char   *ebuf;
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[node->numParams - 1].p;
	RF_RaidLayout_t *layoutPtr = (RF_RaidLayout_t *) & raidPtr->Layout;
	RF_PhysDiskAddr_t *failedPDA = (RF_PhysDiskAddr_t *) node->params[node->numParams - 2].p;
	RF_PhysDiskAddr_t *pda;
	int     i, suoffset, failedSUOffset = rf_StripeUnitOffset(layoutPtr, failedPDA->startSector);
	RF_RowCol_t scol;
	char   *srcbuf, *destbuf;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	RF_Etimer_t timer;

	RF_ETIMER_START(timer);
	for (i = 0; i < node->numParams - 2; i += 2) {
		RF_ASSERT(node->params[i + 1].p != ebuf);
		pda = (RF_PhysDiskAddr_t *) node->params[i].p;
		suoffset = rf_StripeUnitOffset(layoutPtr, pda->startSector);
		scol = rf_EUCol(layoutPtr, pda->raidAddress);
		srcbuf = (char *) node->params[i + 1].p;
		destbuf = ebuf + rf_RaidAddressToByte(raidPtr, suoffset - failedSUOffset);
		rf_e_encToBuf(raidPtr, scol, srcbuf, RF_EO_MATRIX_DIM - 2, destbuf, pda->numSector);
	}

	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	tracerec->q_us += RF_ETIMER_VAL_US(timer);
}


/**************************************************************************************
 * This function is used in case where one data disk failed and both redundant disks
 * alive. It is used in the EO_100_CreateWriteDAG. Note: if there is another disk
 * failed in the stripe but not accessed at this time, then we should, instead, use
 * the rf_EOWriteDoubleRecoveryFunc().
 **************************************************************************************/
int 
rf_Degraded_100_EOFunc(node)
	RF_DagNode_t *node;
{
	rf_DegrESubroutine(node, node->results[1]);
	rf_RecoveryXorFunc(node);	/* does the wakeup here! */
#if 1
	return (0);		/* XXX this was missing... SHould these be
				 * void functions??? GO */
#endif
}
/**************************************************************************************
 * This function is to encode one sector in one of the data disks to the E disk.
 * However, in evenodd this function can also be used as decoding function to recover
 * data from dead disk in the case of parity failure and a single data failure.
 **************************************************************************************/
void 
rf_e_EncOneSect(
    RF_RowCol_t srcLogicCol,
    char *srcSecbuf,
    RF_RowCol_t destLogicCol,
    char *destSecbuf,
    int bytesPerSector)
{
	int     S_index;	/* index of the EU in the src col which need
				 * be Xored into all EUs in a dest sector */
	int     numRowInEncMatix = (RF_EO_MATRIX_DIM) - 1;
	RF_RowCol_t j, indexInDest,	/* row index of an encoding unit in
					 * the destination colume of encoding
					 * matrix */
	        indexInSrc;	/* row index of an encoding unit in the source
				 * colume used for recovery */
	int     bytesPerEU = bytesPerSector / numRowInEncMatix;

#if RF_EO_MATRIX_DIM > 17
	int     shortsPerEU = bytesPerEU / sizeof(short);
	short  *destShortBuf, *srcShortBuf1, *srcShortBuf2;
	short temp1;
#elif RF_EO_MATRIX_DIM == 17
	int     longsPerEU = bytesPerEU / sizeof(long);
	long   *destLongBuf, *srcLongBuf1, *srcLongBuf2;
	long temp1;
#endif

#if RF_EO_MATRIX_DIM > 17
	RF_ASSERT(sizeof(short) == 2 || sizeof(short) == 1);
	RF_ASSERT(bytesPerEU % sizeof(short) == 0);
#elif RF_EO_MATRIX_DIM == 17
	RF_ASSERT(sizeof(long) == 8 || sizeof(long) == 4);
	RF_ASSERT(bytesPerEU % sizeof(long) == 0);
#endif

	S_index = rf_EO_Mod((RF_EO_MATRIX_DIM - 1 + destLogicCol - srcLogicCol), RF_EO_MATRIX_DIM);
#if RF_EO_MATRIX_DIM > 17
	srcShortBuf1 = (short *) (srcSecbuf + S_index * bytesPerEU);
#elif RF_EO_MATRIX_DIM == 17
	srcLongBuf1 = (long *) (srcSecbuf + S_index * bytesPerEU);
#endif

	for (indexInDest = 0; indexInDest < numRowInEncMatix; indexInDest++) {
		indexInSrc = rf_EO_Mod((indexInDest + destLogicCol - srcLogicCol), RF_EO_MATRIX_DIM);

#if RF_EO_MATRIX_DIM > 17
		destShortBuf = (short *) (destSecbuf + indexInDest * bytesPerEU);
		srcShortBuf2 = (short *) (srcSecbuf + indexInSrc * bytesPerEU);
		for (j = 0; j < shortsPerEU; j++) {
			temp1 = destShortBuf[j] ^ srcShortBuf1[j];
			/* note: S_index won't be at the end row for any src
			 * col! */
			if (indexInSrc != RF_EO_MATRIX_DIM - 1)
				destShortBuf[j] = (srcShortBuf2[j]) ^ temp1;
			/* if indexInSrc is at the end row, ie.
			 * RF_EO_MATRIX_DIM -1, then all elements are zero! */
			else
				destShortBuf[j] = temp1;
		}

#elif RF_EO_MATRIX_DIM == 17
		destLongBuf = (long *) (destSecbuf + indexInDest * bytesPerEU);
		srcLongBuf2 = (long *) (srcSecbuf + indexInSrc * bytesPerEU);
		for (j = 0; j < longsPerEU; j++) {
			temp1 = destLongBuf[j] ^ srcLongBuf1[j];
			if (indexInSrc != RF_EO_MATRIX_DIM - 1)
				destLongBuf[j] = (srcLongBuf2[j]) ^ temp1;
			else
				destLongBuf[j] = temp1;
		}
#endif
	}
}

void 
rf_e_encToBuf(
    RF_Raid_t * raidPtr,
    RF_RowCol_t srcLogicCol,
    char *srcbuf,
    RF_RowCol_t destLogicCol,
    char *destbuf,
    int numSector)
{
	int     i, bytesPerSector = rf_RaidAddressToByte(raidPtr, 1);

	for (i = 0; i < numSector; i++) {
		rf_e_EncOneSect(srcLogicCol, srcbuf, destLogicCol, destbuf, bytesPerSector);
		srcbuf += bytesPerSector;
		destbuf += bytesPerSector;
	}
}
/**************************************************************************************
 * when parity die and one data die, We use second redundant information, 'E',
 * to recover the data in dead disk. This function is used in the recovery node of
 * for EO_110_CreateReadDAG
 **************************************************************************************/
int 
rf_RecoveryEFunc(node)
	RF_DagNode_t *node;
{
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[node->numParams - 1].p;
	RF_RaidLayout_t *layoutPtr = (RF_RaidLayout_t *) & raidPtr->Layout;
	RF_PhysDiskAddr_t *failedPDA = (RF_PhysDiskAddr_t *) node->params[node->numParams - 2].p;
	RF_RowCol_t scol,	/* source logical column */
	        fcol = rf_EUCol(layoutPtr, failedPDA->raidAddress);	/* logical column of
									 * failed SU */
	int     i;
	RF_PhysDiskAddr_t *pda;
	int     suoffset, failedSUOffset = rf_StripeUnitOffset(layoutPtr, failedPDA->startSector);
	char   *srcbuf, *destbuf;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;
	RF_Etimer_t timer;

	bzero((char *) node->results[0], rf_RaidAddressToByte(raidPtr, failedPDA->numSector));
	if (node->dagHdr->status == rf_enable) {
		RF_ETIMER_START(timer);
		for (i = 0; i < node->numParams - 2; i += 2)
			if (node->params[i + 1].p != node->results[0]) {
				pda = (RF_PhysDiskAddr_t *) node->params[i].p;
				if (i == node->numParams - 4)
					scol = RF_EO_MATRIX_DIM - 2;	/* the colume of
									 * redundant E */
				else
					scol = rf_EUCol(layoutPtr, pda->raidAddress);
				srcbuf = (char *) node->params[i + 1].p;
				suoffset = rf_StripeUnitOffset(layoutPtr, pda->startSector);
				destbuf = ((char *) node->results[0]) + rf_RaidAddressToByte(raidPtr, suoffset - failedSUOffset);
				rf_e_encToBuf(raidPtr, scol, srcbuf, fcol, destbuf, pda->numSector);
			}
		RF_ETIMER_STOP(timer);
		RF_ETIMER_EVAL(timer);
		tracerec->xor_us += RF_ETIMER_VAL_US(timer);
	}
	return (rf_GenericWakeupFunc(node, 0));	/* node execute successfully */
}
/**************************************************************************************
 * This function is used in the case where one data and the parity have filed.
 * (in EO_110_CreateWriteDAG )
 **************************************************************************************/
int 
rf_EO_DegradedWriteEFunc(RF_DagNode_t * node)
{
	rf_DegrESubroutine(node, node->results[0]);
	rf_GenericWakeupFunc(node, 0);
#if 1
	return (0);		/* XXX Yet another one!! GO */
#endif
}



/**************************************************************************************
 *  		THE FUNCTION IS FOR DOUBLE DEGRADED READ AND WRITE CASES
 **************************************************************************************/

void 
rf_doubleEOdecode(
    RF_Raid_t * raidPtr,
    char **rrdbuf,
    char **dest,
    RF_RowCol_t * fcol,
    char *pbuf,
    char *ebuf)
{
	RF_RaidLayout_t *layoutPtr = (RF_RaidLayout_t *) & (raidPtr->Layout);
	int     i, j, k, f1, f2, row;
	int     rrdrow, erow, count = 0;
	int     bytesPerSector = rf_RaidAddressToByte(raidPtr, 1);
	int     numRowInEncMatix = (RF_EO_MATRIX_DIM) - 1;
#if 0
	int     pcol = (RF_EO_MATRIX_DIM) - 1;
#endif
	int     ecol = (RF_EO_MATRIX_DIM) - 2;
	int     bytesPerEU = bytesPerSector / numRowInEncMatix;
	int     numDataCol = layoutPtr->numDataCol;
#if RF_EO_MATRIX_DIM > 17
	int     shortsPerEU = bytesPerEU / sizeof(short);
	short  *rrdbuf_current, *pbuf_current, *ebuf_current;
	short  *dest_smaller, *dest_smaller_current, *dest_larger, *dest_larger_current;
	short *temp;
	short  *P;

	RF_ASSERT(bytesPerEU % sizeof(short) == 0);
	RF_Malloc(P, bytesPerEU, (short *));
	RF_Malloc(temp, bytesPerEU, (short *));
#elif RF_EO_MATRIX_DIM == 17
	int     longsPerEU = bytesPerEU / sizeof(long);
	long   *rrdbuf_current, *pbuf_current, *ebuf_current;
	long   *dest_smaller, *dest_smaller_current, *dest_larger, *dest_larger_current;
	long *temp;
	long   *P;

	RF_ASSERT(bytesPerEU % sizeof(long) == 0);
	RF_Malloc(P, bytesPerEU, (long *));
	RF_Malloc(temp, bytesPerEU, (long *));
#endif
	RF_ASSERT(*((long *) dest[0]) == 0);
	RF_ASSERT(*((long *) dest[1]) == 0);
	bzero((char *) P, bytesPerEU);
	bzero((char *) temp, bytesPerEU);
	RF_ASSERT(*P == 0);
	/* calculate the 'P' parameter, which, not parity, is the Xor of all
	 * elements in the last two column, ie. 'E' and 'parity' colume, see
	 * the Ref. paper by Blaum, et al 1993  */
	for (i = 0; i < numRowInEncMatix; i++)
		for (k = 0; k < longsPerEU; k++) {
#if RF_EO_MATRIX_DIM > 17
			ebuf_current = ((short *) ebuf) + i * shortsPerEU + k;
			pbuf_current = ((short *) pbuf) + i * shortsPerEU + k;
#elif RF_EO_MATRIX_DIM == 17
			ebuf_current = ((long *) ebuf) + i * longsPerEU + k;
			pbuf_current = ((long *) pbuf) + i * longsPerEU + k;
#endif
			P[k] ^= *ebuf_current;
			P[k] ^= *pbuf_current;
		}
	RF_ASSERT(fcol[0] != fcol[1]);
	if (fcol[0] < fcol[1]) {
#if RF_EO_MATRIX_DIM > 17
		dest_smaller = (short *) (dest[0]);
		dest_larger = (short *) (dest[1]);
#elif RF_EO_MATRIX_DIM == 17
		dest_smaller = (long *) (dest[0]);
		dest_larger = (long *) (dest[1]);
#endif
		f1 = fcol[0];
		f2 = fcol[1];
	} else {
#if RF_EO_MATRIX_DIM > 17
		dest_smaller = (short *) (dest[1]);
		dest_larger = (short *) (dest[0]);
#elif RF_EO_MATRIX_DIM == 17
		dest_smaller = (long *) (dest[1]);
		dest_larger = (long *) (dest[0]);
#endif
		f1 = fcol[1];
		f2 = fcol[0];
	}
	row = (RF_EO_MATRIX_DIM) - 1;
	while ((row = rf_EO_Mod((row + f1 - f2), RF_EO_MATRIX_DIM)) != ((RF_EO_MATRIX_DIM) - 1)) {
#if RF_EO_MATRIX_DIM > 17
		dest_larger_current = dest_larger + row * shortsPerEU;
		dest_smaller_current = dest_smaller + row * shortsPerEU;
#elif RF_EO_MATRIX_DIM == 17
		dest_larger_current = dest_larger + row * longsPerEU;
		dest_smaller_current = dest_smaller + row * longsPerEU;
#endif
		/**    Do the diagonal recovery. Initially, temp[k] = (failed 1),
		       which is the failed data in the colume which has smaller col index. **/
		/* step 1:  ^(SUM of nonfailed in-diagonal A(rrdrow,0..m-3))         */
		for (j = 0; j < numDataCol; j++) {
			if (j == f1 || j == f2)
				continue;
			rrdrow = rf_EO_Mod((row + f2 - j), RF_EO_MATRIX_DIM);
			if (rrdrow != (RF_EO_MATRIX_DIM) - 1) {
#if RF_EO_MATRIX_DIM > 17
				rrdbuf_current = (short *) (rrdbuf[j]) + rrdrow * shortsPerEU;
				for (k = 0; k < shortsPerEU; k++)
					temp[k] ^= *(rrdbuf_current + k);
#elif RF_EO_MATRIX_DIM == 17
				rrdbuf_current = (long *) (rrdbuf[j]) + rrdrow * longsPerEU;
				for (k = 0; k < longsPerEU; k++)
					temp[k] ^= *(rrdbuf_current + k);
#endif
			}
		}
		/* step 2:  ^E(erow,m-2), If erow is at the buttom row, don't
		 * Xor into it  E(erow,m-2) = (principle diagonal) ^ (failed
		 * 1) ^ (failed 2) ^ ( SUM of nonfailed in-diagonal
		 * A(rrdrow,0..m-3) ) After this step, temp[k] = (principle
		 * diagonal) ^ (failed 2)       */

		erow = rf_EO_Mod((row + f2 - ecol), (RF_EO_MATRIX_DIM));
		if (erow != (RF_EO_MATRIX_DIM) - 1) {
#if RF_EO_MATRIX_DIM > 17
			ebuf_current = (short *) ebuf + shortsPerEU * erow;
			for (k = 0; k < shortsPerEU; k++)
				temp[k] ^= *(ebuf_current + k);
#elif RF_EO_MATRIX_DIM == 17
			ebuf_current = (long *) ebuf + longsPerEU * erow;
			for (k = 0; k < longsPerEU; k++)
				temp[k] ^= *(ebuf_current + k);
#endif
		}
		/* step 3: ^P to obtain the failed data (failed 2).  P can be
		 * proved to be actually  (principle diagonal)  After this
		 * step, temp[k] = (failed 2), the failed data to be recovered */
#if RF_EO_MATRIX_DIM > 17
		for (k = 0; k < shortsPerEU; k++)
			temp[k] ^= P[k];
		/* Put the data to the destination buffer                              */
		for (k = 0; k < shortsPerEU; k++)
			dest_larger_current[k] = temp[k];
#elif RF_EO_MATRIX_DIM == 17
		for (k = 0; k < longsPerEU; k++)
			temp[k] ^= P[k];
		/* Put the data to the destination buffer                              */
		for (k = 0; k < longsPerEU; k++)
			dest_larger_current[k] = temp[k];
#endif

		/**          THE FOLLOWING DO THE HORIZONTAL XOR                **/
		/* step 1:  ^(SUM of A(row,0..m-3)), ie. all nonfailed data
		 * columes    */
		for (j = 0; j < numDataCol; j++) {
			if (j == f1 || j == f2)
				continue;
#if RF_EO_MATRIX_DIM > 17
			rrdbuf_current = (short *) (rrdbuf[j]) + row * shortsPerEU;
			for (k = 0; k < shortsPerEU; k++)
				temp[k] ^= *(rrdbuf_current + k);
#elif RF_EO_MATRIX_DIM == 17
			rrdbuf_current = (long *) (rrdbuf[j]) + row * longsPerEU;
			for (k = 0; k < longsPerEU; k++)
				temp[k] ^= *(rrdbuf_current + k);
#endif
		}
		/* step 2: ^A(row,m-1) */
		/* step 3: Put the data to the destination buffer                             	 */
#if RF_EO_MATRIX_DIM > 17
		pbuf_current = (short *) pbuf + shortsPerEU * row;
		for (k = 0; k < shortsPerEU; k++)
			temp[k] ^= *(pbuf_current + k);
		for (k = 0; k < shortsPerEU; k++)
			dest_smaller_current[k] = temp[k];
#elif RF_EO_MATRIX_DIM == 17
		pbuf_current = (long *) pbuf + longsPerEU * row;
		for (k = 0; k < longsPerEU; k++)
			temp[k] ^= *(pbuf_current + k);
		for (k = 0; k < longsPerEU; k++)
			dest_smaller_current[k] = temp[k];
#endif
		count++;
	}
	/* Check if all Encoding Unit in the data buffer have been decoded,
	 * according EvenOdd theory, if "RF_EO_MATRIX_DIM" is a prime number,
	 * this algorithm will covered all buffer 				 */
	RF_ASSERT(count == numRowInEncMatix);
	RF_Free((char *) P, bytesPerEU);
	RF_Free((char *) temp, bytesPerEU);
}


/***************************************************************************************
* 	This function is called by double degragded read
* 	EO_200_CreateReadDAG
*
***************************************************************************************/
int 
rf_EvenOddDoubleRecoveryFunc(node)
	RF_DagNode_t *node;
{
	int     ndataParam = 0;
	int     np = node->numParams;
	RF_AccessStripeMap_t *asmap = (RF_AccessStripeMap_t *) node->params[np - 1].p;
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[np - 2].p;
	RF_RaidLayout_t *layoutPtr = (RF_RaidLayout_t *) & (raidPtr->Layout);
	int     i, prm, sector, nresults = node->numResults;
	RF_SectorCount_t secPerSU = layoutPtr->sectorsPerStripeUnit;
	unsigned sosAddr;
	int     two = 0, mallc_one = 0, mallc_two = 0;	/* flags to indicate if
							 * memory is allocated */
	int     bytesPerSector = rf_RaidAddressToByte(raidPtr, 1);
	RF_PhysDiskAddr_t *ppda, *ppda2, *epda, *epda2, *pda, *pda0, *pda1,
	        npda;
	RF_RowCol_t fcol[2], fsuoff[2], fsuend[2], numDataCol = layoutPtr->numDataCol;
	char  **buf, *ebuf, *pbuf, *dest[2];
	long   *suoff = NULL, *suend = NULL, *prmToCol = NULL, psuoff, esuoff;
	RF_SectorNum_t startSector, endSector;
	RF_Etimer_t timer;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;

	RF_ETIMER_START(timer);

	/* Find out the number of parameters which are pdas for data
	 * information */
	for (i = 0; i <= np; i++)
		if (((RF_PhysDiskAddr_t *) node->params[i].p)->type != RF_PDA_TYPE_DATA) {
			ndataParam = i;
			break;
		}
	RF_Malloc(buf, numDataCol * sizeof(char *), (char **));
	if (ndataParam != 0) {
		RF_Malloc(suoff, ndataParam * sizeof(long), (long *));
		RF_Malloc(suend, ndataParam * sizeof(long), (long *));
		RF_Malloc(prmToCol, ndataParam * sizeof(long), (long *));
	}
	if (asmap->failedPDAs[1] &&
	    (asmap->failedPDAs[1]->numSector + asmap->failedPDAs[0]->numSector < secPerSU)) {
		RF_ASSERT(0);	/* currently, no support for this situation */
		ppda = node->params[np - 6].p;
		ppda2 = node->params[np - 5].p;
		RF_ASSERT(ppda2->type == RF_PDA_TYPE_PARITY);
		epda = node->params[np - 4].p;
		epda2 = node->params[np - 3].p;
		RF_ASSERT(epda2->type == RF_PDA_TYPE_Q);
		two = 1;
	} else {
		ppda = node->params[np - 4].p;
		epda = node->params[np - 3].p;
		psuoff = rf_StripeUnitOffset(layoutPtr, ppda->startSector);
		esuoff = rf_StripeUnitOffset(layoutPtr, epda->startSector);
		RF_ASSERT(psuoff == esuoff);
	}
	/*
            the followings have three goals:
            1. determine the startSector to begin decoding and endSector to end decoding.
            2. determine the colume numbers of the two failed disks.
            3. determine the offset and end offset of the access within each failed stripe unit.
         */
	if (nresults == 1) {
		/* find the startSector to begin decoding */
		pda = node->results[0];
		bzero(pda->bufPtr, bytesPerSector * pda->numSector);
		fsuoff[0] = rf_StripeUnitOffset(layoutPtr, pda->startSector);
		fsuend[0] = fsuoff[0] + pda->numSector;
		startSector = fsuoff[0];
		endSector = fsuend[0];

		/* find out the column of failed disk being accessed */
		fcol[0] = rf_EUCol(layoutPtr, pda->raidAddress);

		/* find out the other failed colume not accessed */
		sosAddr = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, asmap->raidAddress);
		for (i = 0; i < numDataCol; i++) {
			npda.raidAddress = sosAddr + (i * secPerSU);
			(raidPtr->Layout.map->MapSector) (raidPtr, npda.raidAddress, &(npda.row), &(npda.col), &(npda.startSector), 0);
			/* skip over dead disks */
			if (RF_DEAD_DISK(raidPtr->Disks[npda.row][npda.col].status))
				if (i != fcol[0])
					break;
		}
		RF_ASSERT(i < numDataCol);
		fcol[1] = i;
	} else {
		RF_ASSERT(nresults == 2);
		pda0 = node->results[0];
		bzero(pda0->bufPtr, bytesPerSector * pda0->numSector);
		pda1 = node->results[1];
		bzero(pda1->bufPtr, bytesPerSector * pda1->numSector);
		/* determine the failed colume numbers of the two failed
		 * disks. */
		fcol[0] = rf_EUCol(layoutPtr, pda0->raidAddress);
		fcol[1] = rf_EUCol(layoutPtr, pda1->raidAddress);
		/* determine the offset and end offset of the access within
		 * each failed stripe unit. */
		fsuoff[0] = rf_StripeUnitOffset(layoutPtr, pda0->startSector);
		fsuend[0] = fsuoff[0] + pda0->numSector;
		fsuoff[1] = rf_StripeUnitOffset(layoutPtr, pda1->startSector);
		fsuend[1] = fsuoff[1] + pda1->numSector;
		/* determine the startSector to begin decoding */
		startSector = RF_MIN(pda0->startSector, pda1->startSector);
		/* determine the endSector to end decoding */
		endSector = RF_MAX(fsuend[0], fsuend[1]);
	}
	/*
	      assign the beginning sector and the end sector for each parameter
	      find out the corresponding colume # for each parameter
        */
	for (prm = 0; prm < ndataParam; prm++) {
		pda = node->params[prm].p;
		suoff[prm] = rf_StripeUnitOffset(layoutPtr, pda->startSector);
		suend[prm] = suoff[prm] + pda->numSector;
		prmToCol[prm] = rf_EUCol(layoutPtr, pda->raidAddress);
	}
	/* 'sector' is the sector for the current decoding algorithm. For each
	 * sector in the failed SU, find out the corresponding parameters that
	 * cover the current sector and that are needed for decoding of this
	 * sector in failed SU. 2.  Find out if sector is in the shadow of any
	 * accessed failed SU. If not, malloc a temporary space of a sector in
	 * size. */
	for (sector = startSector; sector < endSector; sector++) {
		if (nresults == 2)
			if (!(fsuoff[0] <= sector && sector < fsuend[0]) && !(fsuoff[1] <= sector && sector < fsuend[1]))
				continue;
		for (prm = 0; prm < ndataParam; prm++)
			if (suoff[prm] <= sector && sector < suend[prm])
				buf[(prmToCol[prm])] = ((RF_PhysDiskAddr_t *) node->params[prm].p)->bufPtr +
				    rf_RaidAddressToByte(raidPtr, sector - suoff[prm]);
		/* find out if sector is in the shadow of any accessed failed
		 * SU. If yes, assign dest[0], dest[1] to point at suitable
		 * position of the buffer corresponding to failed SUs. if no,
		 * malloc a temporary space of a sector in size for
		 * destination of decoding. */
		RF_ASSERT(nresults == 1 || nresults == 2);
		if (nresults == 1) {
			dest[0] = ((RF_PhysDiskAddr_t *) node->results[0])->bufPtr + rf_RaidAddressToByte(raidPtr, sector - fsuoff[0]);
			/* Always malloc temp buffer to dest[1]  */
			RF_Malloc(dest[1], bytesPerSector, (char *));
			bzero(dest[1], bytesPerSector);
			mallc_two = 1;
		} else {
			if (fsuoff[0] <= sector && sector < fsuend[0])
				dest[0] = ((RF_PhysDiskAddr_t *) node->results[0])->bufPtr + rf_RaidAddressToByte(raidPtr, sector - fsuoff[0]);
			else {
				RF_Malloc(dest[0], bytesPerSector, (char *));
				bzero(dest[0], bytesPerSector);
				mallc_one = 1;
			}
			if (fsuoff[1] <= sector && sector < fsuend[1])
				dest[1] = ((RF_PhysDiskAddr_t *) node->results[1])->bufPtr + rf_RaidAddressToByte(raidPtr, sector - fsuoff[1]);
			else {
				RF_Malloc(dest[1], bytesPerSector, (char *));
				bzero(dest[1], bytesPerSector);
				mallc_two = 1;
			}
			RF_ASSERT(mallc_one == 0 || mallc_two == 0);
		}
		pbuf = ppda->bufPtr + rf_RaidAddressToByte(raidPtr, sector - psuoff);
		ebuf = epda->bufPtr + rf_RaidAddressToByte(raidPtr, sector - esuoff);
		/*
	         * After finish finding all needed sectors, call doubleEOdecode function for decoding
	         * one sector to destination.
	         */
		rf_doubleEOdecode(raidPtr, buf, dest, fcol, pbuf, ebuf);
		/* free all allocated memory, and mark flag to indicate no
		 * memory is being allocated */
		if (mallc_one == 1)
			RF_Free(dest[0], bytesPerSector);
		if (mallc_two == 1)
			RF_Free(dest[1], bytesPerSector);
		mallc_one = mallc_two = 0;
	}
	RF_Free(buf, numDataCol * sizeof(char *));
	if (ndataParam != 0) {
		RF_Free(suoff, ndataParam * sizeof(long));
		RF_Free(suend, ndataParam * sizeof(long));
		RF_Free(prmToCol, ndataParam * sizeof(long));
	}
	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	if (tracerec) {
		tracerec->q_us += RF_ETIMER_VAL_US(timer);
	}
	rf_GenericWakeupFunc(node, 0);
#if 1
	return (0);		/* XXX is this even close!!?!?!!? GO */
#endif
}


/* currently, only access of one of the two failed SU is allowed in this function.
 * also, asmap->numStripeUnitsAccessed is limited to be one, the RaidFrame will break large access into
 * many accesses of single stripe unit.
 */

int 
rf_EOWriteDoubleRecoveryFunc(node)
	RF_DagNode_t *node;
{
	int     np = node->numParams;
	RF_AccessStripeMap_t *asmap = (RF_AccessStripeMap_t *) node->params[np - 1].p;
	RF_Raid_t *raidPtr = (RF_Raid_t *) node->params[np - 2].p;
	RF_RaidLayout_t *layoutPtr = (RF_RaidLayout_t *) & (raidPtr->Layout);
	RF_SectorNum_t sector;
	RF_RowCol_t col, scol;
	int     prm, i, j;
	RF_SectorCount_t secPerSU = layoutPtr->sectorsPerStripeUnit;
	unsigned sosAddr;
	unsigned bytesPerSector = rf_RaidAddressToByte(raidPtr, 1);
	RF_int64 numbytes;
	RF_SectorNum_t startSector, endSector;
	RF_PhysDiskAddr_t *ppda, *epda, *pda, *fpda, npda;
	RF_RowCol_t fcol[2], numDataCol = layoutPtr->numDataCol;
	char  **buf;		/* buf[0], buf[1], buf[2], ...etc. point to
				 * buffer storing data read from col0, col1,
				 * col2 */
	char   *ebuf, *pbuf, *dest[2], *olddata[2];
	RF_Etimer_t timer;
	RF_AccTraceEntry_t *tracerec = node->dagHdr->tracerec;

	RF_ASSERT(asmap->numDataFailed == 1);	/* currently only support this
						 * case, the other failed SU
						 * is not being accessed */
	RF_ETIMER_START(timer);
	RF_Malloc(buf, numDataCol * sizeof(char *), (char **));

	ppda = node->results[0];/* Instead of being buffers, node->results[0]
				 * and [1] are Ppda and Epda  */
	epda = node->results[1];
	fpda = asmap->failedPDAs[0];

	/* First, recovery the failed old SU using EvenOdd double decoding      */
	/* determine the startSector and endSector for decoding */
	startSector = rf_StripeUnitOffset(layoutPtr, fpda->startSector);
	endSector = startSector + fpda->numSector;
	/* Assign buf[col] pointers to point to each non-failed colume  and
	 * initialize the pbuf and ebuf to point at the beginning of each
	 * source buffers and destination buffers */
	for (prm = 0; prm < numDataCol - 2; prm++) {
		pda = (RF_PhysDiskAddr_t *) node->params[prm].p;
		col = rf_EUCol(layoutPtr, pda->raidAddress);
		buf[col] = pda->bufPtr;
	}
	/* pbuf and ebuf:  they will change values as double recovery decoding
	 * goes on */
	pbuf = ppda->bufPtr;
	ebuf = epda->bufPtr;
	/* find out the logical colume numbers in the encoding matrix of the
	 * two failed columes */
	fcol[0] = rf_EUCol(layoutPtr, fpda->raidAddress);

	/* find out the other failed colume not accessed this time */
	sosAddr = rf_RaidAddressOfPrevStripeBoundary(layoutPtr, asmap->raidAddress);
	for (i = 0; i < numDataCol; i++) {
		npda.raidAddress = sosAddr + (i * secPerSU);
		(raidPtr->Layout.map->MapSector) (raidPtr, npda.raidAddress, &(npda.row), &(npda.col), &(npda.startSector), 0);
		/* skip over dead disks */
		if (RF_DEAD_DISK(raidPtr->Disks[npda.row][npda.col].status))
			if (i != fcol[0])
				break;
	}
	RF_ASSERT(i < numDataCol);
	fcol[1] = i;
	/* assign temporary space to put recovered failed SU */
	numbytes = fpda->numSector * bytesPerSector;
	RF_Malloc(olddata[0], numbytes, (char *));
	RF_Malloc(olddata[1], numbytes, (char *));
	dest[0] = olddata[0];
	dest[1] = olddata[1];
	bzero(olddata[0], numbytes);
	bzero(olddata[1], numbytes);
	/* Begin the recovery decoding, initially buf[j],  ebuf, pbuf, dest[j]
	 * have already pointed at the beginning of each source buffers and
	 * destination buffers */
	for (sector = startSector, i = 0; sector < endSector; sector++, i++) {
		rf_doubleEOdecode(raidPtr, buf, dest, fcol, pbuf, ebuf);
		for (j = 0; j < numDataCol; j++)
			if ((j != fcol[0]) && (j != fcol[1]))
				buf[j] += bytesPerSector;
		dest[0] += bytesPerSector;
		dest[1] += bytesPerSector;
		ebuf += bytesPerSector;
		pbuf += bytesPerSector;
	}
	/* after recovery, the buffer pointed by olddata[0] is the old failed
	 * data. With new writing data and this old data, use small write to
	 * calculate the new redundant informations */
	/* node->params[ 0, ... PDAPerDisk * (numDataCol - 2)-1 ] are Pdas of
	 * Rrd; params[ PDAPerDisk*(numDataCol - 2), ... PDAPerDisk*numDataCol
	 * -1 ] are Pdas of Rp, ( Rp2 ), Re, ( Re2 ) ; params[
	 * PDAPerDisk*numDataCol, ... PDAPerDisk*numDataCol
	 * +asmap->numStripeUnitsAccessed -asmap->numDataFailed-1] are Pdas of
	 * wudNodes; For current implementation, we assume the simplest case:
	 * asmap->numStripeUnitsAccessed == 1 and asmap->numDataFailed == 1
	 * ie. PDAPerDisk = 1 then node->params[numDataCol] must be the new
	 * data to be writen to the failed disk. We first bxor the new data
	 * into the old recovered data, then do the same things as small
	 * write. */

	rf_bxor(((RF_PhysDiskAddr_t *) node->params[numDataCol].p)->bufPtr, olddata[0], numbytes, node->dagHdr->bp);
	/* do new 'E' calculation  */
	/* find out the corresponding colume in encoding matrix for write
	 * colume to be encoded into redundant disk 'E' */
	scol = rf_EUCol(layoutPtr, fpda->raidAddress);
	/* olddata[0] now is source buffer pointer; epda->bufPtr is the dest
	 * buffer pointer               */
	rf_e_encToBuf(raidPtr, scol, olddata[0], RF_EO_MATRIX_DIM - 2, epda->bufPtr, fpda->numSector);

	/* do new 'P' calculation  */
	rf_bxor(olddata[0], ppda->bufPtr, numbytes, node->dagHdr->bp);
	/* Free the allocated buffer  */
	RF_Free(olddata[0], numbytes);
	RF_Free(olddata[1], numbytes);
	RF_Free(buf, numDataCol * sizeof(char *));

	RF_ETIMER_STOP(timer);
	RF_ETIMER_EVAL(timer);
	if (tracerec) {
		tracerec->q_us += RF_ETIMER_VAL_US(timer);
	}
	rf_GenericWakeupFunc(node, 0);
	return (0);
}
#endif				/* RF_INCLUDE_EVENODD > 0 */
