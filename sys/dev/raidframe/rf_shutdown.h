/*	$FreeBSD$ */
/*	$NetBSD: rf_shutdown.h,v 1.2 1999/02/05 00:06:17 oster Exp $	*/
/*
 * rf_shutdown.h
 */
/*
 * Copyright (c) 1996 Carnegie-Mellon University.
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
/*
 * Maintain lists of cleanup functions. Also, mechanisms for coordinating
 * thread startup and shutdown.
 */

#ifndef _RF__RF_SHUTDOWN_H_
#define _RF__RF_SHUTDOWN_H_

#include <dev/raidframe/rf_types.h>
#include <dev/raidframe/rf_threadstuff.h>

/*
 * Important note: the shutdown list is run like a stack, new
 * entries pushed on top. Therefore, the most recently added
 * entry (last started) is the first removed (stopped). This
 * should handle system-dependencies pretty nicely- if a system
 * is there when you start another, it'll be there when you
 * shut down another. Hopefully, this subsystem will remove
 * more complexity than it introduces.
 */

struct RF_ShutdownList_s {
	void    (*cleanup) (void *arg);
	void   *arg;
	char   *file;
	int     line;
	RF_ShutdownList_t *next;
};
#define rf_ShutdownCreate(_listp_,_func_,_arg_) \
  _rf_ShutdownCreate(_listp_,_func_,_arg_,__FILE__,__LINE__)

int     _rf_ShutdownCreate(RF_ShutdownList_t ** listp, void (*cleanup) (void *arg),
            void *arg, char *file, int line);
int     rf_ShutdownList(RF_ShutdownList_t ** listp);

#endif				/* !_RF__RF_SHUTDOWN_H_ */
