/*	$FreeBSD$ */
/*	$NetBSD: rf_callback.h,v 1.3 1999/02/05 00:06:06 oster Exp $	*/
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

/*****************************************************************************************
 *
 * callback.h -- header file for callback.c
 *
 * the reconstruction code must manage concurrent I/Os on multiple drives.
 * it sometimes needs to suspend operation on a particular drive until some
 * condition occurs.  we can't block the thread, of course, or we wouldn't
 * be able to manage our other outstanding I/Os.  Instead we just suspend
 * new activity on the indicated disk, and create a callback descriptor and
 * put it someplace where it will get invoked when the condition that's
 * stalling us has cleared.  When the descriptor is invoked, it will call
 * a function that will restart operation on the indicated disk.
 *
 ****************************************************************************************/

#ifndef _RF__RF_CALLBACK_H_
#define _RF__RF_CALLBACK_H_

#include <dev/raidframe/rf_types.h>

struct RF_CallbackDesc_s {
	void    (*callbackFunc) (RF_CBParam_t);	/* function to call */
	RF_CBParam_t callbackArg;	/* args to give to function, or just
					 * info about this callback  */
	RF_CBParam_t callbackArg2;
	RF_RowCol_t row;	/* disk row and column IDs to give to the
				 * callback func */
	RF_RowCol_t col;
	RF_CallbackDesc_t *next;/* next entry in list */
};

int     rf_ConfigureCallback(RF_ShutdownList_t ** listp);
RF_CallbackDesc_t *rf_AllocCallbackDesc(void);
void    rf_FreeCallbackDesc(RF_CallbackDesc_t * p);

#endif				/* !_RF__RF_CALLBACK_H_ */
