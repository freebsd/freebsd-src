/*	$FreeBSD$ */
/*	$NetBSD: rf_evenodd_dagfuncs.h,v 1.2 1999/02/05 00:06:11 oster Exp $	*/
/*
 * rf_evenodd_dagfuncs.h
 */
/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chang-Ming Wu
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

#ifndef _RF__RF_EVENODD_DAGFUNCS_H_
#define _RF__RF_EVENODD_DAGFUNCS_H_

extern RF_RedFuncs_t rf_EOSmallWriteEFuncs;
extern RF_RedFuncs_t rf_EOSmallWritePFuncs;
extern RF_RedFuncs_t rf_eoERecoveryFuncs;
extern RF_RedFuncs_t rf_eoPRecoveryFuncs;

int     rf_RegularPEFunc(RF_DagNode_t * node);
int     rf_RegularONEFunc(RF_DagNode_t * node);
int     rf_SimpleONEFunc(RF_DagNode_t * node);
void    rf_RegularESubroutine(RF_DagNode_t * node, char *ebuf);
int     rf_RegularEFunc(RF_DagNode_t * node);
void    rf_DegrESubroutine(RF_DagNode_t * node, char *ebuf);
int     rf_Degraded_100_EOFunc(RF_DagNode_t * node);
void 
rf_e_EncOneSect(RF_RowCol_t srcLogicCol, char *srcSecbuf,
    RF_RowCol_t destLogicCol, char *destSecbuf, int bytesPerSector);
void 
rf_e_encToBuf(RF_Raid_t * raidPtr, RF_RowCol_t srcLogicCol,
    char *srcbuf, RF_RowCol_t destLogicCol, char *destbuf, int numSector);
int     rf_RecoveryEFunc(RF_DagNode_t * node);
int     rf_EO_DegradedWriteEFunc(RF_DagNode_t * node);
void 
rf_doubleEOdecode(RF_Raid_t * raidPtr, char **rrdbuf, char **dest,
    RF_RowCol_t * fcol, char *pbuf, char *ebuf);
int     rf_EvenOddDoubleRecoveryFunc(RF_DagNode_t * node);
int     rf_EOWriteDoubleRecoveryFunc(RF_DagNode_t * node);

#define rf_EUCol(_layoutPtr_, _addr_ ) \
( (_addr_)%( (_layoutPtr_)->dataSectorsPerStripe ) )/((_layoutPtr_)->sectorsPerStripeUnit)

#define rf_EO_Mod( _int1_, _int2_ ) \
( ((_int1_) < 0)? (((_int1_)+(_int2_))%(_int2_)) : (_int1_)%(_int2_) )

#define rf_OffsetOfNextEUBoundary(_offset_, sec_per_eu) ((_offset_)/(sec_per_eu) + 1)*(sec_per_eu)

#define RF_EO_MATRIX_DIM 17

/*
 * RF_EO_MATRIX_DIM should be a prime number: and "bytesPerSector" should be
 * dividable by ( RF_EO_MATRIX_DIM - 1) to fully encode and utilize the space
 * in a sector, this number could also be 17. Tha later case doesn't apply
 * for disk array larger than 17 columns totally.
 */

#endif				/* !_RF__RF_EVENODD_DAGFUNCS_H_ */
