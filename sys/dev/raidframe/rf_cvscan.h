/*	$FreeBSD$ */
/*	$NetBSD: rf_cvscan.h,v 1.3 1999/02/05 00:06:07 oster Exp $	*/
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
**	Disk scheduling by CVSCAN( N, r )
**
**	Given a set of requests, partition them into one set on each
**	side of the current arm position.  The trick is to pick which
**	side you are going to service next; once a side is picked you will
**	service the closest request.
**	Let there be n1 requests on one side and n2 requests on the other
**	side.  If one of n1 or n2 is zero, select the other side.
**	If both n1 and n2 are nonzero, select a "range" for examination
**	that is N' = min( n1, n2, N ).  Average the distance from the
**	current position to the nearest N' requests on each side giving
**	d1 and d2.
**	Suppose the last decision was to move toward set 2, then the
**	current direction is toward set 2, and you will only switch to set
**	1 if d1+R < d2 where R is r*(total number of cylinders), r in [0,1].
**
**	I extend this by applying only to the set of requests that all
**	share the same, highest priority level.
*/

#ifndef _RF__RF_CVSCAN_H_
#define _RF__RF_CVSCAN_H_

#include <dev/raidframe/rf_diskqueue.h>

typedef enum RF_CvscanArmDir_e {
	rf_cvscan_LEFT,
	rf_cvscan_RIGHT
}       RF_CvscanArmDir_t;

typedef struct RF_CvscanHeader_s {
	long    range_for_avg;	/* CVSCAN param N */
	long    change_penalty;	/* CVSCAN param R */
	RF_CvscanArmDir_t direction;
	RF_SectorNum_t cur_block;
	int     nxt_priority;
	RF_DiskQueueData_t *left;
	int     left_cnt;
	RF_DiskQueueData_t *right;
	int     right_cnt;
	RF_DiskQueueData_t *burner;
}       RF_CvscanHeader_t;

int     rf_CvscanConfigure(void);
void   *
rf_CvscanCreate(RF_SectorCount_t sect_per_disk,
    RF_AllocListElem_t * cl_list, RF_ShutdownList_t ** listp);
void    rf_CvscanEnqueue(void *qptr, RF_DiskQueueData_t * req, int priority);
RF_DiskQueueData_t *rf_CvscanDequeue(void *qptr);
RF_DiskQueueData_t *rf_CvscanPeek(void *qptr);
int 
rf_CvscanPromote(void *qptr, RF_StripeNum_t parityStripeID,
    RF_ReconUnitNum_t which_ru);

#endif				/* !_RF__RF_CVSCAN_H_ */
