/*-
 * Copyright (c) 2004-2005 Robert N. M. Watson
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
 * $FreeBSD: src/sys/netatalk/ddp_usrreq.c,v 1.55 2007/05/11 10:20:49 rwatson Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/protosw.h>
#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>

#include <netatalk/at.h>
#include <netatalk/at_var.h>
#include <netatalk/ddp_var.h>
#include <netatalk/ddp_pcb.h>
#include <netatalk/at_extern.h>

static u_long	ddp_sendspace = DDP_MAXSZ; /* Max ddp size + 1 (ddp_type) */
static u_long	ddp_recvspace = 10 * (587 + sizeof(struct sockaddr_at));

static struct ifqueue atintrq1, atintrq2, aarpintrq;

static int
ddp_attach(struct socket *so, int proto, struct thread *td)
{
	struct ddpcb *ddp;
	int error = 0;
	
	ddp = sotoddpcb(so);
	KASSERT(ddp == NULL, ("ddp_attach: ddp != NULL"));

	/*
	 * Allocate socket buffer space first so that it's present
	 * before first use.
	 */
	error = soreserve(so, ddp_sendspace, ddp_recvspace);
	if (error)
		return (error);

	DDP_LIST_XLOCK();
	error = at_pcballoc(so);
	DDP_LIST_XUNLOCK();
	return (error);
}

static void
ddp_detach(struct socket *so)
{
	struct ddpcb *ddp;
	
	ddp = sotoddpcb(so);
	KASSERT(ddp != NULL, ("ddp_detach: ddp == NULL"));

	DDP_LIST_XLOCK();
	DDP_LOCK(ddp);
	at_pcbdetach(so, ddp);
	DDP_LIST_XUNLOCK();
}

static int      
ddp_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct ddpcb *ddp;
	int error = 0;
	
	ddp = sotoddpcb(so);
	KASSERT(ddp != NULL, ("ddp_bind: ddp == NULL"));

	DDP_LIST_XLOCK();
	DDP_LOCK(ddp);
	error = at_pcbsetaddr(ddp, nam, td);
	DDP_UNLOCK(ddp);
	DDP_LIST_XUNLOCK();
	return (error);
}
    
static int
ddp_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct ddpcb *ddp;
	int error = 0;
	
	ddp = sotoddpcb(so);
	KASSERT(ddp != NULL, ("ddp_connect: ddp == NULL"));

	DDP_LIST_XLOCK();
	DDP_LOCK(ddp);
	if (ddp->ddp_fsat.sat_port != ATADDR_ANYPORT) {
		DDP_UNLOCK(ddp);
		DDP_LIST_XUNLOCK();
		return (EISCONN);
	}

	error = at_pcbconnect( ddp, nam, td );
	DDP_UNLOCK(ddp);
	DDP_LIST_XUNLOCK();
	if (error == 0)
		soisconnected(so);
	return (error);
}

static int
ddp_disconnect(struct socket *so)
{
	struct ddpcb *ddp;
	
	ddp = sotoddpcb(so);
	KASSERT(ddp != NULL, ("ddp_disconnect: ddp == NULL"));

	DDP_LOCK(ddp);
	if (ddp->ddp_fsat.sat_addr.s_node == ATADDR_ANYNODE) {
		DDP_UNLOCK(ddp);
		return (ENOTCONN);
	}

	at_pcbdisconnect(ddp);
	ddp->ddp_fsat.sat_addr.s_node = ATADDR_ANYNODE;
	DDP_UNLOCK(ddp);
	soisdisconnected(so);
	return (0);
}

static int
ddp_shutdown(struct socket *so)
{
	struct ddpcb	*ddp;

	ddp = sotoddpcb(so);
	KASSERT(ddp != NULL, ("ddp_shutdown: ddp == NULL"));

	socantsendmore(so);
	return (0);
}

static int
ddp_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
    struct mbuf *control, struct thread *td)
{
	struct ddpcb *ddp;
	int error = 0;
	
	ddp = sotoddpcb(so);
	KASSERT(ddp != NULL, ("ddp_send: ddp == NULL"));

    	if (control && control->m_len)
		return (EINVAL);

	if (addr != NULL) {
		DDP_LIST_XLOCK();
		DDP_LOCK(ddp);
		if (ddp->ddp_fsat.sat_port != ATADDR_ANYPORT) {
			error = EISCONN;
			goto out;
		}

		error = at_pcbconnect(ddp, addr, td);
		if (error == 0) {
			error = ddp_output(m, so);
			at_pcbdisconnect(ddp);
		}
out:
		DDP_UNLOCK(ddp);
		DDP_LIST_XUNLOCK();
	} else {
		DDP_LOCK(ddp);
		if (ddp->ddp_fsat.sat_port == ATADDR_ANYPORT)
			error = ENOTCONN;
		else
			error = ddp_output(m, so);
		DDP_UNLOCK(ddp);
	}
	return (error);
}

/*
 * XXXRW: This is never called because we only invoke abort on stream
 * protocols.
 */
static void
ddp_abort(struct socket *so)
{
	struct ddpcb	*ddp;
	
	ddp = sotoddpcb(so);
	KASSERT(ddp != NULL, ("ddp_abort: ddp == NULL"));

	DDP_LOCK(ddp);
	at_pcbdisconnect(ddp);
	DDP_UNLOCK(ddp);
	soisdisconnected(so);
}

static void
ddp_close(struct socket *so)
{
	struct ddpcb	*ddp;
	
	ddp = sotoddpcb(so);
	KASSERT(ddp != NULL, ("ddp_close: ddp == NULL"));

	DDP_LOCK(ddp);
	at_pcbdisconnect(ddp);
	DDP_UNLOCK(ddp);
	soisdisconnected(so);
}

void 
ddp_init(void)
{

	atintrq1.ifq_maxlen = IFQ_MAXLEN;
	atintrq2.ifq_maxlen = IFQ_MAXLEN;
	aarpintrq.ifq_maxlen = IFQ_MAXLEN;
	mtx_init(&atintrq1.ifq_mtx, "at1_inq", NULL, MTX_DEF);
	mtx_init(&atintrq2.ifq_mtx, "at2_inq", NULL, MTX_DEF);
	mtx_init(&aarpintrq.ifq_mtx, "aarp_inq", NULL, MTX_DEF);
	DDP_LIST_LOCK_INIT();
	netisr_register(NETISR_ATALK1, at1intr, &atintrq1, NETISR_MPSAFE);
	netisr_register(NETISR_ATALK2, at2intr, &atintrq2, NETISR_MPSAFE);
	netisr_register(NETISR_AARP, aarpintr, &aarpintrq, NETISR_MPSAFE);
}

#if 0
static void 
ddp_clean(void)
{
	struct ddpcp	*ddp;

	for (ddp = ddpcb_list; ddp != NULL; ddp = ddp->ddp_next)
		at_pcbdetach(ddp->ddp_socket, ddp);
	DDP_LIST_LOCK_DESTROY();
}
#endif

static int
at_getpeeraddr(struct socket *so, struct sockaddr **nam)
{

	return (EOPNOTSUPP);
}

static int
at_getsockaddr(struct socket *so, struct sockaddr **nam)
{
	struct ddpcb	*ddp;

	ddp = sotoddpcb(so);
	KASSERT(ddp != NULL, ("at_getsockaddr: ddp == NULL"));

	DDP_LOCK(ddp);
	at_sockaddr(ddp, nam);
	DDP_UNLOCK(ddp);
	return (0);
}

struct pr_usrreqs ddp_usrreqs = {
	.pru_abort =		ddp_abort,
	.pru_attach =		ddp_attach,
	.pru_bind =		ddp_bind,
	.pru_connect =		ddp_connect,
	.pru_control =		at_control,
	.pru_detach =		ddp_detach,
	.pru_disconnect =	ddp_disconnect,
	.pru_peeraddr =		at_getpeeraddr,
	.pru_send =		ddp_send,
	.pru_shutdown =		ddp_shutdown,
	.pru_sockaddr =		at_getsockaddr,
	.pru_close =		ddp_close,
};
