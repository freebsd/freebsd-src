/*	$NetBSD: rf_cvscan.c,v 1.5 1999/08/13 03:41:53 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
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

/*******************************************************************************
 *
 * cvscan.c --  prioritized cvscan disk queueing code.
 *
 * Nov 9, 1994, adapted from raidSim version (MCH)
 *
 ******************************************************************************/

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_alloclist.h>
#include <dev/raidframe/rf_stripelocks.h>
#include <dev/raidframe/rf_layout.h>
#include <dev/raidframe/rf_diskqueue.h>
#include <dev/raidframe/rf_cvscan.h>
#include <dev/raidframe/rf_debugMem.h>
#include <dev/raidframe/rf_general.h>

#define DO_CHECK_STATE(_hdr_) CheckCvscanState((_hdr_), __FILE__, __LINE__)

#define pri_ok(p)  ( ((p) == RF_IO_NORMAL_PRIORITY) || ((p) == RF_IO_LOW_PRIORITY))

static void 
CheckCvscanState(RF_CvscanHeader_t * hdr, char *file, int line)
{
	long    i, key;
	RF_DiskQueueData_t *tmp;

	if (hdr->left != (RF_DiskQueueData_t *) NULL)
		RF_ASSERT(hdr->left->sectorOffset < hdr->cur_block);
	for (key = hdr->cur_block, i = 0, tmp = hdr->left;
	    tmp != (RF_DiskQueueData_t *) NULL;
	    key = tmp->sectorOffset, i++, tmp = tmp->next)
		RF_ASSERT(tmp->sectorOffset <= key
		    && tmp->priority == hdr->nxt_priority && pri_ok(tmp->priority));
	RF_ASSERT(i == hdr->left_cnt);

	for (key = hdr->cur_block, i = 0, tmp = hdr->right;
	    tmp != (RF_DiskQueueData_t *) NULL;
	    key = tmp->sectorOffset, i++, tmp = tmp->next) {
		RF_ASSERT(key <= tmp->sectorOffset);
		RF_ASSERT(tmp->priority == hdr->nxt_priority);
		RF_ASSERT(pri_ok(tmp->priority));
	}
	RF_ASSERT(i == hdr->right_cnt);

	for (key = hdr->nxt_priority - 1, tmp = hdr->burner;
	    tmp != (RF_DiskQueueData_t *) NULL;
	    key = tmp->priority, tmp = tmp->next) {
		RF_ASSERT(tmp);
		RF_ASSERT(hdr);
		RF_ASSERT(pri_ok(tmp->priority));
		RF_ASSERT(key >= tmp->priority);
		RF_ASSERT(tmp->priority < hdr->nxt_priority);
	}
}



static void 
PriorityInsert(RF_DiskQueueData_t ** list_ptr, RF_DiskQueueData_t * req)
{
	/* * insert block pointed to by req in to list whose first * entry is
	 * pointed to by the pointer that list_ptr points to * ie., list_ptr
	 * is a grandparent of the first entry */

	for (; (*list_ptr) != (RF_DiskQueueData_t *) NULL &&
	    (*list_ptr)->priority > req->priority;
	    list_ptr = &((*list_ptr)->next)) {
	}
	req->next = (*list_ptr);
	(*list_ptr) = req;
}



static void 
ReqInsert(RF_DiskQueueData_t ** list_ptr, RF_DiskQueueData_t * req, RF_CvscanArmDir_t order)
{
	/* * insert block pointed to by req in to list whose first * entry is
	 * pointed to by the pointer that list_ptr points to * ie., list_ptr
	 * is a grandparent of the first entry */

	for (; (*list_ptr) != (RF_DiskQueueData_t *) NULL &&

	    ((order == rf_cvscan_RIGHT && (*list_ptr)->sectorOffset <= req->sectorOffset)
		|| (order == rf_cvscan_LEFT && (*list_ptr)->sectorOffset > req->sectorOffset));
	    list_ptr = &((*list_ptr)->next)) {
	}
	req->next = (*list_ptr);
	(*list_ptr) = req;
}



static RF_DiskQueueData_t *
ReqDequeue(RF_DiskQueueData_t ** list_ptr)
{
	RF_DiskQueueData_t *ret = (*list_ptr);
	if ((*list_ptr) != (RF_DiskQueueData_t *) NULL) {
		(*list_ptr) = (*list_ptr)->next;
	}
	return (ret);
}



static void 
ReBalance(RF_CvscanHeader_t * hdr)
{
	/* DO_CHECK_STATE(hdr); */
	while (hdr->right != (RF_DiskQueueData_t *) NULL
	    && hdr->right->sectorOffset < hdr->cur_block) {
		hdr->right_cnt--;
		hdr->left_cnt++;
		ReqInsert(&hdr->left, ReqDequeue(&hdr->right), rf_cvscan_LEFT);
	}
	/* DO_CHECK_STATE(hdr); */
}



static void 
Transfer(RF_DiskQueueData_t ** to_list_ptr, RF_DiskQueueData_t ** from_list_ptr)
{
	RF_DiskQueueData_t *gp;
	for (gp = (*from_list_ptr); gp != (RF_DiskQueueData_t *) NULL;) {
		RF_DiskQueueData_t *p = gp->next;
		PriorityInsert(to_list_ptr, gp);
		gp = p;
	}
	(*from_list_ptr) = (RF_DiskQueueData_t *) NULL;
}



static void 
RealEnqueue(RF_CvscanHeader_t * hdr, RF_DiskQueueData_t * req)
{
	RF_ASSERT(req->priority == RF_IO_NORMAL_PRIORITY || req->priority == RF_IO_LOW_PRIORITY);

	DO_CHECK_STATE(hdr);
	if (hdr->left_cnt == 0 && hdr->right_cnt == 0) {
		hdr->nxt_priority = req->priority;
	}
	if (req->priority > hdr->nxt_priority) {
		/*
		** dump all other outstanding requests on the back burner
		*/
		Transfer(&hdr->burner, &hdr->left);
		Transfer(&hdr->burner, &hdr->right);
		hdr->left_cnt = 0;
		hdr->right_cnt = 0;
		hdr->nxt_priority = req->priority;
	}
	if (req->priority < hdr->nxt_priority) {
		/*
		** yet another low priority task!
		*/
		PriorityInsert(&hdr->burner, req);
	} else {
		if (req->sectorOffset < hdr->cur_block) {
			/* this request is to the left of the current arms */
			ReqInsert(&hdr->left, req, rf_cvscan_LEFT);
			hdr->left_cnt++;
		} else {
			/* this request is to the right of the current arms */
			ReqInsert(&hdr->right, req, rf_cvscan_RIGHT);
			hdr->right_cnt++;
		}
	}
	DO_CHECK_STATE(hdr);
}



void 
rf_CvscanEnqueue(void *q_in, RF_DiskQueueData_t * elem, int priority)
{
	RF_CvscanHeader_t *hdr = (RF_CvscanHeader_t *) q_in;
	RealEnqueue(hdr, elem /* req */ );
}



RF_DiskQueueData_t *
rf_CvscanDequeue(void *q_in)
{
	RF_CvscanHeader_t *hdr = (RF_CvscanHeader_t *) q_in;
	long    range, i, sum_dist_left, sum_dist_right;
	RF_DiskQueueData_t *ret;
	RF_DiskQueueData_t *tmp;

	DO_CHECK_STATE(hdr);

	if (hdr->left_cnt == 0 && hdr->right_cnt == 0)
		return ((RF_DiskQueueData_t *) NULL);

	range = RF_MIN(hdr->range_for_avg, RF_MIN(hdr->left_cnt, hdr->right_cnt));
	for (i = 0, tmp = hdr->left, sum_dist_left =
	    ((hdr->direction == rf_cvscan_RIGHT) ? range * hdr->change_penalty : 0);
	    tmp != (RF_DiskQueueData_t *) NULL && i < range;
	    tmp = tmp->next, i++) {
		sum_dist_left += hdr->cur_block - tmp->sectorOffset;
	}
	for (i = 0, tmp = hdr->right, sum_dist_right =
	    ((hdr->direction == rf_cvscan_LEFT) ? range * hdr->change_penalty : 0);
	    tmp != (RF_DiskQueueData_t *) NULL && i < range;
	    tmp = tmp->next, i++) {
		sum_dist_right += tmp->sectorOffset - hdr->cur_block;
	}

	if (hdr->right_cnt == 0 || sum_dist_left < sum_dist_right) {
		hdr->direction = rf_cvscan_LEFT;
		hdr->cur_block = hdr->left->sectorOffset + hdr->left->numSector;
		hdr->left_cnt = RF_MAX(hdr->left_cnt - 1, 0);
		tmp = hdr->left;
		ret = (ReqDequeue(&hdr->left)) /*->parent*/ ;
	} else {
		hdr->direction = rf_cvscan_RIGHT;
		hdr->cur_block = hdr->right->sectorOffset + hdr->right->numSector;
		hdr->right_cnt = RF_MAX(hdr->right_cnt - 1, 0);
		tmp = hdr->right;
		ret = (ReqDequeue(&hdr->right)) /*->parent*/ ;
	}
	ReBalance(hdr);

	if (hdr->left_cnt == 0 && hdr->right_cnt == 0
	    && hdr->burner != (RF_DiskQueueData_t *) NULL) {
		/*
		** restore low priority requests for next dequeue
		*/
		RF_DiskQueueData_t *burner = hdr->burner;
		hdr->nxt_priority = burner->priority;
		while (burner != (RF_DiskQueueData_t *) NULL
		    && burner->priority == hdr->nxt_priority) {
			RF_DiskQueueData_t *next = burner->next;
			RealEnqueue(hdr, burner);
			burner = next;
		}
		hdr->burner = burner;
	}
	DO_CHECK_STATE(hdr);
	return (ret);
}



RF_DiskQueueData_t *
rf_CvscanPeek(void *q_in)
{
	RF_CvscanHeader_t *hdr = (RF_CvscanHeader_t *) q_in;
	long    range, i, sum_dist_left, sum_dist_right;
	RF_DiskQueueData_t *tmp, *headElement;

	DO_CHECK_STATE(hdr);

	if (hdr->left_cnt == 0 && hdr->right_cnt == 0)
		headElement = NULL;
	else {
		range = RF_MIN(hdr->range_for_avg, RF_MIN(hdr->left_cnt, hdr->right_cnt));
		for (i = 0, tmp = hdr->left, sum_dist_left =
		    ((hdr->direction == rf_cvscan_RIGHT) ? range * hdr->change_penalty : 0);
		    tmp != (RF_DiskQueueData_t *) NULL && i < range;
		    tmp = tmp->next, i++) {
			sum_dist_left += hdr->cur_block - tmp->sectorOffset;
		}
		for (i = 0, tmp = hdr->right, sum_dist_right =
		    ((hdr->direction == rf_cvscan_LEFT) ? range * hdr->change_penalty : 0);
		    tmp != (RF_DiskQueueData_t *) NULL && i < range;
		    tmp = tmp->next, i++) {
			sum_dist_right += tmp->sectorOffset - hdr->cur_block;
		}

		if (hdr->right_cnt == 0 || sum_dist_left < sum_dist_right)
			headElement = hdr->left;
		else
			headElement = hdr->right;
	}
	return (headElement);
}



/*
** CVSCAN( 1, 0 ) is Shortest Seek Time First (SSTF)
**				lowest average response time
** CVSCAN( 1, infinity ) is SCAN
**				lowest response time standard deviation
*/


int 
rf_CvscanConfigure()
{
	return (0);
}



void   *
rf_CvscanCreate(RF_SectorCount_t sectPerDisk,
    RF_AllocListElem_t * clList,
    RF_ShutdownList_t ** listp)
{
	RF_CvscanHeader_t *hdr;
	long    range = 2;	/* Currently no mechanism to change these */
	long    penalty = sectPerDisk / 5;

	RF_MallocAndAdd(hdr, sizeof(RF_CvscanHeader_t), (RF_CvscanHeader_t *), clList);
	bzero((char *) hdr, sizeof(RF_CvscanHeader_t));
	hdr->range_for_avg = RF_MAX(range, 1);
	hdr->change_penalty = RF_MAX(penalty, 0);
	hdr->direction = rf_cvscan_RIGHT;
	hdr->cur_block = 0;
	hdr->left_cnt = hdr->right_cnt = 0;
	hdr->left = hdr->right = (RF_DiskQueueData_t *) NULL;
	hdr->burner = (RF_DiskQueueData_t *) NULL;
	DO_CHECK_STATE(hdr);

	return ((void *) hdr);
}


#if defined(__NetBSD__) || defined(__FreeBSD__) && defined(_KERNEL)
/* PrintCvscanQueue is not used, so we ignore it... */
#else
static void 
PrintCvscanQueue(RF_CvscanHeader_t * hdr)
{
	RF_DiskQueueData_t *tmp;

	printf("CVSCAN(%d,%d) at %d going %s\n",
	    (int) hdr->range_for_avg,
	    (int) hdr->change_penalty,
	    (int) hdr->cur_block,
	    (hdr->direction == rf_cvscan_LEFT) ? "LEFT" : "RIGHT");
	printf("\tLeft(%d): ", hdr->left_cnt);
	for (tmp = hdr->left; tmp != (RF_DiskQueueData_t *) NULL; tmp = tmp->next)
		printf("(%d,%ld,%d) ",
		    (int) tmp->sectorOffset,
		    (long) (tmp->sectorOffset + tmp->numSector),
		    tmp->priority);
	printf("\n");
	printf("\tRight(%d): ", hdr->right_cnt);
	for (tmp = hdr->right; tmp != (RF_DiskQueueData_t *) NULL; tmp = tmp->next)
		printf("(%d,%ld,%d) ",
		    (int) tmp->sectorOffset,
		    (long) (tmp->sectorOffset + tmp->numSector),
		    tmp->priority);
	printf("\n");
	printf("\tBurner: ");
	for (tmp = hdr->burner; tmp != (RF_DiskQueueData_t *) NULL; tmp = tmp->next)
		printf("(%d,%ld,%d) ",
		    (int) tmp->sectorOffset,
		    (long) (tmp->sectorOffset + tmp->numSector),
		    tmp->priority);
	printf("\n");
}
#endif


/* promotes reconstruction accesses for the given stripeID to normal priority.
 * returns 1 if an access was found and zero otherwise.  Normally, we should
 * only have one or zero entries in the burner queue, so execution time should
 * be short.
 */
int 
rf_CvscanPromote(void *q_in, RF_StripeNum_t parityStripeID, RF_ReconUnitNum_t which_ru)
{
	RF_CvscanHeader_t *hdr = (RF_CvscanHeader_t *) q_in;
	RF_DiskQueueData_t *trailer = NULL, *tmp = hdr->burner, *tlist = NULL;
	int     retval = 0;

	DO_CHECK_STATE(hdr);
	while (tmp) {		/* handle entries at the front of the list */
		if (tmp->parityStripeID == parityStripeID && tmp->which_ru == which_ru) {
			hdr->burner = tmp->next;
			tmp->priority = RF_IO_NORMAL_PRIORITY;
			tmp->next = tlist;
			tlist = tmp;
			tmp = hdr->burner;
		} else
			break;
	}
	if (tmp) {
		trailer = tmp;
		tmp = tmp->next;
	}
	while (tmp) {		/* handle entries on the rest of the list */
		if (tmp->parityStripeID == parityStripeID && tmp->which_ru == which_ru) {
			trailer->next = tmp->next;
			tmp->priority = RF_IO_NORMAL_PRIORITY;
			tmp->next = tlist;
			tlist = tmp;	/* insert on a temp queue */
			tmp = trailer->next;
		} else {
			trailer = tmp;
			tmp = tmp->next;
		}
	}
	while (tlist) {
		retval++;
		tmp = tlist->next;
		RealEnqueue(hdr, tlist);
		tlist = tmp;
	}
	RF_ASSERT(retval == 0 || retval == 1);
	DO_CHECK_STATE((RF_CvscanHeader_t *) q_in);
	return (retval);
}
