/*	$FreeBSD$ */
/*	$NetBSD: rf_mcpair.h,v 1.6 2000/09/21 01:45:46 oster Exp $	*/
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

/* rf_mcpair.h
 * see comments in rf_mcpair.c
 */

#ifndef _RF__RF_MCPAIR_H_
#define _RF__RF_MCPAIR_H_

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_threadstuff.h>

struct RF_MCPair_s {
	RF_DECLARE_MUTEX(mutex)
	RF_DECLARE_COND(cond)
	int     flag;
	RF_MCPair_t *next;
};
#define RF_WAIT_MCPAIR(_mcp) \
	RF_LTSLEEP(&((_mcp)->cond), PRIBIO, "mcpair", 0, &((_mcp)->mutex))

int     rf_ConfigureMCPair(RF_ShutdownList_t ** listp);
RF_MCPair_t *rf_AllocMCPair(void);
void    rf_FreeMCPair(RF_MCPair_t * t);
void    rf_MCPairWakeupFunc(RF_MCPair_t * t);

#endif				/* !_RF__RF_MCPAIR_H_ */
