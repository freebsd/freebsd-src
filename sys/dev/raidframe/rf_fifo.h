/*	$FreeBSD$ */
/*	$NetBSD: rf_fifo.h,v 1.3 1999/02/05 00:06:11 oster Exp $	*/
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

/*
 * rf_fifo.h --  prioritized FIFO queue code.
 *
 * 4-9-93 Created (MCH)
 */


#ifndef _RF__RF_FIFO_H_
#define _RF__RF_FIFO_H_

#include <dev/raidframe/rf_archs.h>
#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_diskqueue.h>

typedef struct RF_FifoHeader_s {
	RF_DiskQueueData_t *hq_head, *hq_tail;	/* high priority requests */
	RF_DiskQueueData_t *lq_head, *lq_tail;	/* low priority requests */
	int     hq_count, lq_count;	/* debug only */
}       RF_FifoHeader_t;

extern void *
rf_FifoCreate(RF_SectorCount_t sectPerDisk,
    RF_AllocListElem_t * clList, RF_ShutdownList_t ** listp);
extern void 
rf_FifoEnqueue(void *q_in, RF_DiskQueueData_t * elem,
    int priority);
extern RF_DiskQueueData_t *rf_FifoDequeue(void *q_in);
extern RF_DiskQueueData_t *rf_FifoPeek(void *q_in);
extern int 
rf_FifoPromote(void *q_in, RF_StripeNum_t parityStripeID,
    RF_ReconUnitNum_t which_ru);

#endif				/* !_RF__RF_FIFO_H_ */
