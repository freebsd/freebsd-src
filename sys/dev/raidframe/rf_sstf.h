/*	$FreeBSD$ */
/*	$NetBSD: rf_sstf.h,v 1.3 1999/02/05 00:06:17 oster Exp $	*/
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

#ifndef _RF__RF_SSTF_H_
#define _RF__RF_SSTF_H_

#include <dev/raidframe/rf_diskqueue.h>

typedef struct RF_SstfQ_s {
	RF_DiskQueueData_t *queue;
	RF_DiskQueueData_t *qtail;
	int     qlen;
}       RF_SstfQ_t;

typedef struct RF_Sstf_s {
	RF_SstfQ_t left;
	RF_SstfQ_t right;
	RF_SstfQ_t lopri;
	RF_SectorNum_t last_sector;
	int     dir;
	int     allow_reverse;
}       RF_Sstf_t;

void   *
rf_SstfCreate(RF_SectorCount_t sect_per_disk,
    RF_AllocListElem_t * cl_list, RF_ShutdownList_t ** listp);
void   *
rf_ScanCreate(RF_SectorCount_t sect_per_disk,
    RF_AllocListElem_t * cl_list, RF_ShutdownList_t ** listp);
void   *
rf_CscanCreate(RF_SectorCount_t sect_per_disk,
    RF_AllocListElem_t * cl_list, RF_ShutdownList_t ** listp);
void    rf_SstfEnqueue(void *qptr, RF_DiskQueueData_t * req, int priority);
RF_DiskQueueData_t *rf_SstfDequeue(void *qptr);
RF_DiskQueueData_t *rf_SstfPeek(void *qptr);
int 
rf_SstfPromote(void *qptr, RF_StripeNum_t parityStripeID,
    RF_ReconUnitNum_t which_ru);
RF_DiskQueueData_t *rf_ScanDequeue(void *qptr);
RF_DiskQueueData_t *rf_ScanPeek(void *qptr);
RF_DiskQueueData_t *rf_CscanDequeue(void *qptr);
RF_DiskQueueData_t *rf_CscanPeek(void *qptr);

#endif				/* !_RF__RF_SSTF_H_ */
