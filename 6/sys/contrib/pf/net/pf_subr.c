/*	$FreeBSD$ */
/*	from $OpenBSD: kern_subr.c,v 1.26 2003/10/31 11:10:41 markus Exp $	*/
/*	$NetBSD: kern_subr.c,v 1.15 1996/04/09 17:21:56 ragge Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_subr.c	8.3 (Berkeley) 1/21/94
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/resourcevar.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_var.h>

#include <net/pfvar.h>

/*
 * This implements additional functions used by pf which can not be ported
 * easyly. At this point it boils down to mostly the Net/OpenBSD hook
 * implementation.
 *
 * BEWARE: this is not locked! Required locking is done by the caller.
 */

void *
hook_establish(struct hook_desc_head *head, int tail, void (*fn)(void *),
    void *arg)
{
	struct hook_desc *hdp;

	hdp = (struct hook_desc *)malloc(sizeof (*hdp), M_DEVBUF, M_NOWAIT);
	if (hdp == NULL)
		return (NULL);

	hdp->hd_fn = fn;
	hdp->hd_arg = arg;
	if (tail)
		TAILQ_INSERT_TAIL(head, hdp, hd_list);
	else
		TAILQ_INSERT_HEAD(head, hdp, hd_list);

	return (hdp);
}

void
hook_disestablish(struct hook_desc_head *head, void *vhook)
{
	struct hook_desc *hdp;

#ifdef DIAGNOSTIC
	for (hdp = TAILQ_FIRST(head); hdp != NULL;
	    hdp = TAILQ_NEXT(hdp, hd_list))
                if (hdp == vhook)
			break;
	if (hdp == NULL)
		panic("hook_disestablish: hook not established");
#endif
	hdp = vhook;
	TAILQ_REMOVE(head, hdp, hd_list);
	free(hdp, M_DEVBUF);
}

/*
 * Run hooks.  Startup hooks are invoked right after scheduler_start but
 * before root is mounted.  Shutdown hooks are invoked immediately before the
 * system is halted or rebooted, i.e. after file systems unmounted,
 * after crash dump done, etc.
 */
void
dohooks(struct hook_desc_head *head, int flags)
{
	struct hook_desc *hdp;

	if ((flags & HOOK_REMOVE) == 0) {
		TAILQ_FOREACH(hdp, head, hd_list) {
			(*hdp->hd_fn)(hdp->hd_arg);
		}
	} else {
		while ((hdp = TAILQ_FIRST(head)) != NULL) {
			TAILQ_REMOVE(head, hdp, hd_list);
			(*hdp->hd_fn)(hdp->hd_arg);
			if ((flags & HOOK_FREE) != 0)
				free(hdp, M_DEVBUF);
		}
	}
}
