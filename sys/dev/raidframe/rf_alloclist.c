/*	$FreeBSD$ */
/*	$NetBSD: rf_alloclist.c,v 1.4 1999/08/13 03:41:53 oster Exp $	*/
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

/****************************************************************************
 *
 * Alloclist.c -- code to manipulate allocation lists
 *
 * an allocation list is just a list of AllocListElem structures.  Each
 * such structure contains a fixed-size array of pointers.  Calling
 * FreeAList() causes each pointer to be freed.
 *
 ***************************************************************************/

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_threadstuff.h>
#include <dev/raidframe/rf_alloclist.h>
#include <dev/raidframe/rf_debugMem.h>
#include <dev/raidframe/rf_etimer.h>
#include <dev/raidframe/rf_general.h>
#include <dev/raidframe/rf_shutdown.h>

RF_DECLARE_STATIC_MUTEX(alist_mutex)
	static unsigned int fl_hit_count, fl_miss_count;

	static RF_AllocListElem_t *al_free_list = NULL;
	static int al_free_list_count;

#define RF_AL_FREELIST_MAX 256

#define DO_FREE(_p,_sz) RF_Free((_p),(_sz))

	static void rf_ShutdownAllocList(void *);

	static void rf_ShutdownAllocList(ignored)
	void   *ignored;
{
	RF_AllocListElem_t *p, *pt;

	for (p = al_free_list; p;) {
		pt = p;
		p = p->next;
		DO_FREE(pt, sizeof(*pt));
	}
	rf_mutex_destroy(&alist_mutex);
	/*
        printf("Alloclist: Free list hit count %lu (%lu %%) miss count %lu (%lu %%)\n",
	       fl_hit_count, (100*fl_hit_count)/(fl_hit_count+fl_miss_count),
	       fl_miss_count, (100*fl_miss_count)/(fl_hit_count+fl_miss_count));
        */
}

int 
rf_ConfigureAllocList(listp)
	RF_ShutdownList_t **listp;
{
	int     rc;

	rc = rf_mutex_init(&alist_mutex, __FUNCTION__);
	if (rc) {
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		return (rc);
	}
	al_free_list = NULL;
	fl_hit_count = fl_miss_count = al_free_list_count = 0;
	rc = rf_ShutdownCreate(listp, rf_ShutdownAllocList, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n",
		    __FILE__, __LINE__, rc);
		rf_mutex_destroy(&alist_mutex);
		return (rc);
	}
	return (0);
}


/* we expect the lists to have at most one or two elements, so we're willing
 * to search for the end.  If you ever observe the lists growing longer,
 * increase POINTERS_PER_ALLOC_LIST_ELEMENT.
 */
void 
rf_real_AddToAllocList(l, p, size, lockflag)
	RF_AllocListElem_t *l;
	void   *p;
	int     size;
	int     lockflag;
{
	RF_AllocListElem_t *newelem;

	for (; l->next; l = l->next)
		RF_ASSERT(l->numPointers == RF_POINTERS_PER_ALLOC_LIST_ELEMENT);	/* find end of list */

	RF_ASSERT(l->numPointers >= 0 && l->numPointers <= RF_POINTERS_PER_ALLOC_LIST_ELEMENT);
	if (l->numPointers == RF_POINTERS_PER_ALLOC_LIST_ELEMENT) {
		newelem = rf_real_MakeAllocList(lockflag);
		l->next = newelem;
		l = newelem;
	}
	l->pointers[l->numPointers] = p;
	l->sizes[l->numPointers] = size;
	l->numPointers++;

}


/* we use the debug_mem_mutex here because we need to lock it anyway to call free.
 * this is probably a bug somewhere else in the code, but when I call malloc/free
 * outside of any lock I have endless trouble with malloc appearing to return the
 * same pointer twice.  Since we have to lock it anyway, we might as well use it
 * as the lock around the al_free_list.  Note that we can't call Free with the
 * debug_mem_mutex locked.
 */
void 
rf_FreeAllocList(l)
	RF_AllocListElem_t *l;
{
	int     i;
	RF_AllocListElem_t *temp, *p;

	for (p = l; p; p = p->next) {
		RF_ASSERT(p->numPointers >= 0 && p->numPointers <= RF_POINTERS_PER_ALLOC_LIST_ELEMENT);
		for (i = 0; i < p->numPointers; i++) {
			RF_ASSERT(p->pointers[i]);
			RF_Free(p->pointers[i], p->sizes[i]);
		}
	}
	while (l) {
		temp = l;
		l = l->next;
		if (al_free_list_count > RF_AL_FREELIST_MAX) {
			DO_FREE(temp, sizeof(*temp));
		} else {
			temp->next = al_free_list;
			al_free_list = temp;
			al_free_list_count++;
		}
	}
}

RF_AllocListElem_t *
rf_real_MakeAllocList(lockflag)
	int     lockflag;
{
	RF_AllocListElem_t *p;

	if (al_free_list) {
		fl_hit_count++;
		p = al_free_list;
		al_free_list = p->next;
		al_free_list_count--;
	} else {
		fl_miss_count++;
		RF_Malloc(p, sizeof(RF_AllocListElem_t), (RF_AllocListElem_t *));	/* no allocation locking
											 * in kernel, so this is
											 * fine */
	}
	if (p == NULL) {
		return (NULL);
	}
	bzero((char *) p, sizeof(RF_AllocListElem_t));
	return (p);
}
