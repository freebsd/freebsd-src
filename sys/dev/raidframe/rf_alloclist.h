/*	$FreeBSD$ */
/*	$NetBSD: rf_alloclist.h,v 1.3 1999/02/05 00:06:06 oster Exp $	*/
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
 * alloclist.h -- header file for alloclist.c
 *
 ***************************************************************************/

#ifndef _RF__RF_ALLOCLIST_H_
#define _RF__RF_ALLOCLIST_H_

#include <dev/raidframe/rf_types.h>

#define RF_POINTERS_PER_ALLOC_LIST_ELEMENT 20

struct RF_AllocListElem_s {
	void   *pointers[RF_POINTERS_PER_ALLOC_LIST_ELEMENT];
	int     sizes[RF_POINTERS_PER_ALLOC_LIST_ELEMENT];
	int     numPointers;
	RF_AllocListElem_t *next;
};
#define rf_MakeAllocList(_ptr_) _ptr_ = rf_real_MakeAllocList(1);
#define rf_AddToAllocList(_l_,_ptr_,_sz_) rf_real_AddToAllocList((_l_), (_ptr_), (_sz_), 1)

int     rf_ConfigureAllocList(RF_ShutdownList_t ** listp);

#if RF_UTILITY == 0
void    rf_real_AddToAllocList(RF_AllocListElem_t * l, void *p, int size, int lockflag);
void    rf_FreeAllocList(RF_AllocListElem_t * l);
RF_AllocListElem_t *rf_real_MakeAllocList(int lockflag);
#endif				/* RF_UTILITY == 0 */

#endif				/* !_RF__RF_ALLOCLIST_H_ */
