/*-
 * Copyright (c) 2004-2005 Robert N. M. Watson
 * Copyright (c) 1990,1994 Regents of The University of Michigan.
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
 * $FreeBSD$
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
	struct ddpcb	*ddp;
	int		error = 0;
	
	ddp = sotoddpcb(so);
	if (ddp != NULL)
		return (EINVAL);

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

static int
ddp_detach(struct socket *so)
{
	struct ddpcb	*ddp;
	
	ddp = sotoddpcb(so);
	if (ddp == NULL)
	    return (EINVAL);

	DDP_LIST_XLOCK();
	DDP_LOCK(ddp);
	at_pcbdetach(so, ddp);
	DDP_LIST_XUNLOCK();
	return (0);
}

static int      
ddp_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	struct ddpcb	*ddp;
	int		error = 0;
	
	ddp = sotoddpcb(so);
	if (ddp == NULL) {
	    return (EINVAL);
	}
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
	struct ddpcb	*ddp;
	int		error = 0;
	
	ddp = sotoddpcb(so);
	if (ddp == NULL) {
	    return (EINVAL);
	}

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

	struct ddpcb	*ddp;
	
	ddp = sotoddpcb(so);
	if (ddp == NULL) {
	    return (EINVAL);
	}
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
	if (ddp == NULL) {
		return (EINVAL);
	}
	socantsendmore(so);
	return (0);
}

static int
ddp_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *addr,
            struct mbuf *control, struct thread *td)
{
	struct ddpcb	*ddp;
	int		error = 0;
	
	ddp = sotoddpcb(so);
	if (ddp == NULL) {
		return (EINVAL);
	}

    	if (control && control->m_len) {
		return (EINVAL);
    	}

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

static int
ddp_abort(struct socket *so)
{
	struct ddpcb	*ddp;
	
	ddp = sotoddpcb(so);
	if (ddp == NULL) {
		return (EINVAL);
	}
	DDP_LIST_XLOCK();
	DDP_LOCK(ddp);
	at_pcbdetach(so, ddp);
	DDP_LIST_XUNLOCK();
	return (0);
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
    struct ddpcb	*ddp;

    for (ddp = ddpcb_list; ddp != NULL; ddp = ddp->ddp_next) {
	at_pcbdetach(ddp->ddp_socket, ddp);
    }
    DDP_LIST_LOCK_DESTROY();
}
#endif

static int
at_setpeeraddr(struct socket *so, struct sockaddr **nam)
{
	return (EOPNOTSUPP);
}

static int
at_setsockaddr(struct socket *so, struct sockaddr **nam)
{
	struct ddpcb	*ddp;

	ddp = sotoddpcb(so);
	if (ddp == NULL) {
	    return (EINVAL);
	}
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
	.pru_peeraddr =		at_setpeeraddr,
	.pru_send =		ddp_send,
	.pru_shutdown =		ddp_shutdown,
	.pru_sockaddr =		at_setsockaddr,
};
