/*	$NetBSD: rf_callback.c,v 1.3 1999/02/05 00:06:06 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
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

/*****************************************************************************************
 *
 * callback.c -- code to manipulate callback descriptor
 *
 ****************************************************************************************/


#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_threadstuff.h>
#include <dev/raidframe/rf_callback.h>
#include <dev/raidframe/rf_debugMem.h>
#include <dev/raidframe/rf_freelist.h>
#include <dev/raidframe/rf_shutdown.h>

static RF_FreeList_t *rf_callback_freelist;

#define RF_MAX_FREE_CALLBACK 64
#define RF_CALLBACK_INC       4
#define RF_CALLBACK_INITIAL   4

static void rf_ShutdownCallback(void *);
static void 
rf_ShutdownCallback(ignored)
	void   *ignored;
{
	RF_FREELIST_DESTROY(rf_callback_freelist, next, (RF_CallbackDesc_t *));
}

int 
rf_ConfigureCallback(listp)
	RF_ShutdownList_t **listp;
{
	int     rc;

	RF_FREELIST_CREATE(rf_callback_freelist, RF_MAX_FREE_CALLBACK,
	    RF_CALLBACK_INC, sizeof(RF_CallbackDesc_t));
	if (rf_callback_freelist == NULL)
		return (ENOMEM);
	rc = rf_ShutdownCreate(listp, rf_ShutdownCallback, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		rf_ShutdownCallback(NULL);
		return (rc);
	}
	RF_FREELIST_PRIME(rf_callback_freelist, RF_CALLBACK_INITIAL, next,
	    (RF_CallbackDesc_t *));
	return (0);
}

RF_CallbackDesc_t *
rf_AllocCallbackDesc()
{
	RF_CallbackDesc_t *p;

	RF_FREELIST_GET(rf_callback_freelist, p, next, (RF_CallbackDesc_t *));
	return (p);
}

void 
rf_FreeCallbackDesc(p)
	RF_CallbackDesc_t *p;
{
	RF_FREELIST_FREE(rf_callback_freelist, p, next);
}
