/*	$NetBSD: rf_mcpair.c,v 1.4 2000/09/11 02:23:14 oster Exp $	*/

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

/* rf_mcpair.c
 * an mcpair is a structure containing a mutex and a condition variable.
 * it's used to block the current thread until some event occurs.
 */

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_threadstuff.h>
#include <dev/raidframe/rf_mcpair.h>
#include <dev/raidframe/rf_debugMem.h>
#include <dev/raidframe/rf_freelist.h>
#include <dev/raidframe/rf_shutdown.h>

#include <sys/proc.h>

static RF_FreeList_t *rf_mcpair_freelist;

#define RF_MAX_FREE_MCPAIR 128
#define RF_MCPAIR_INC       16
#define RF_MCPAIR_INITIAL   24

static int init_mcpair(RF_MCPair_t *);
static void clean_mcpair(RF_MCPair_t *);
static void rf_ShutdownMCPair(void *);



static int 
init_mcpair(t)
	RF_MCPair_t *t;
{
	int     rc;

	rc = rf_mutex_init(&t->mutex, __FUNCTION__);
	if (rc) {
		RF_ERRORMSG3("Unable to init mutex file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		return (rc);
	}
	rc = rf_cond_init(&t->cond);
	if (rc) {
		RF_ERRORMSG3("Unable to init cond file %s line %d rc=%d\n", __FILE__,
		    __LINE__, rc);
		rf_mutex_destroy(&t->mutex);
		return (rc);
	}
	return (0);
}

static void 
clean_mcpair(t)
	RF_MCPair_t *t;
{
	rf_mutex_destroy(&t->mutex);
	rf_cond_destroy(&t->cond);
}

static void 
rf_ShutdownMCPair(ignored)
	void   *ignored;
{
	RF_FREELIST_DESTROY_CLEAN(rf_mcpair_freelist, next, (RF_MCPair_t *), clean_mcpair);
}

int 
rf_ConfigureMCPair(listp)
	RF_ShutdownList_t **listp;
{
	int     rc;

	RF_FREELIST_CREATE(rf_mcpair_freelist, RF_MAX_FREE_MCPAIR,
	    RF_MCPAIR_INC, sizeof(RF_MCPair_t));
	rc = rf_ShutdownCreate(listp, rf_ShutdownMCPair, NULL);
	if (rc) {
		RF_ERRORMSG3("Unable to add to shutdown list file %s line %d rc=%d\n",
		    __FILE__, __LINE__, rc);
		rf_ShutdownMCPair(NULL);
		return (rc);
	}
	RF_FREELIST_PRIME_INIT(rf_mcpair_freelist, RF_MCPAIR_INITIAL, next,
	    (RF_MCPair_t *), init_mcpair);
	return (0);
}

RF_MCPair_t *
rf_AllocMCPair()
{
	RF_MCPair_t *t;

	RF_FREELIST_GET_INIT(rf_mcpair_freelist, t, next, (RF_MCPair_t *), init_mcpair);
	if (t) {
		t->flag = 0;
		t->next = NULL;
	}
	return (t);
}

void 
rf_FreeMCPair(t)
	RF_MCPair_t *t;
{
	RF_FREELIST_FREE_CLEAN(rf_mcpair_freelist, t, next, clean_mcpair);
}
/* the callback function used to wake you up when you use an mcpair to wait for something */
void 
rf_MCPairWakeupFunc(mcpair)
	RF_MCPair_t *mcpair;
{
	RF_LOCK_MUTEX(mcpair->mutex);
	mcpair->flag = 1;
	wakeup(&(mcpair->cond));
	RF_UNLOCK_MUTEX(mcpair->mutex);
}
