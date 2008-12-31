/*-
 * Copyright (c) 2004 Robert N. M. Watson
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Copyright (c) 1990, 1994 Regents of The University of Michigan.
 * All Rights Reserved.
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear
 * in supporting documentation, and that the name of The University
 * of Michigan not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission. This software is supplied as is without expressed or
 * implied warranties of any kind.
 *
 * This product includes software developed by the University of
 * California, Berkeley and its contributors.
 *
 *	Research Systems Unix Group
 *	The University of Michigan
 *	c/o Wesley Craig
 *	535 W. William Street
 *	Ann Arbor, Michigan
 *	+1-313-764-2278
 *	netatalk@umich.edu
 *
 * $FreeBSD: src/sys/netatalk/ddp_pcb.h,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _NETATALK_DDP_PCB_H_
#define	_NETATALK_DDP_PCB_H_

int	at_pcballoc(struct socket *so);
int	at_pcbconnect(struct ddpcb *ddp, struct sockaddr *addr, 
	    struct thread *td);
void	at_pcbdetach(struct socket *so, struct ddpcb *ddp);
void	at_pcbdisconnect(struct ddpcb *ddp);
int	at_pcbsetaddr(struct ddpcb *ddp, struct sockaddr *addr,
	    struct thread *td);
void	at_sockaddr(struct ddpcb *ddp, struct sockaddr **addr);

/* Lock macros for per-pcb locks. */
#define	DDP_LOCK_INIT(ddp)	mtx_init(&(ddp)->ddp_mtx, "ddp_mtx",	\
				    NULL, MTX_DEF)
#define	DDP_LOCK_DESTROY(ddp)	mtx_destroy(&(ddp)->ddp_mtx)
#define	DDP_LOCK(ddp)		mtx_lock(&(ddp)->ddp_mtx)
#define	DDP_UNLOCK(ddp)		mtx_unlock(&(ddp)->ddp_mtx)
#define	DDP_LOCK_ASSERT(ddp)	mtx_assert(&(ddp)->ddp_mtx, MA_OWNED)

/* Lock macros for global pcb list lock. */
#define	DDP_LIST_LOCK_INIT()	mtx_init(&ddp_list_mtx, "ddp_list_mtx",	\
				    NULL, MTX_DEF)
#define	DDP_LIST_LOCK_DESTROY()	mtx_destroy(&ddp_list_mtx)
#define	DDP_LIST_XLOCK()	mtx_lock(&ddp_list_mtx)
#define	DDP_LIST_XUNLOCK()	mtx_unlock(&ddp_list_mtx)
#define	DDP_LIST_XLOCK_ASSERT()	mtx_assert(&ddp_list_mtx, MA_OWNED)
#define	DDP_LIST_SLOCK()	mtx_lock(&ddp_list_mtx)
#define	DDP_LIST_SUNLOCK()	mtx_unlock(&ddp_list_mtx)
#define	DDP_LIST_SLOCK_ASSERT()	mtx_assert(&ddp_list_mtx, MA_OWNED)

#endif /* !_NETATALK_DDP_PCB_H_ */
