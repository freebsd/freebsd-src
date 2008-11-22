/*	$NetBSD: natm.c,v 1.5 1996/11/09 03:26:26 chuck Exp $	*/
/*-
 *
 * Copyright (c) 1996 Charles D. Cranor and Washington University.
 * Copyright (c) 2005 Robert N. M. Watson
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Charles D. Cranor and
 *      Washington University.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * natm.c: native mode ATM access (both aal0 and aal5).
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/sx.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_atm.h>
#include <net/netisr.h>

#include <netinet/in.h>

#include <netnatm/natm.h>

static const u_long natm5_sendspace = 16*1024;
static const u_long natm5_recvspace = 16*1024;

static const u_long natm0_sendspace = 16*1024;
static const u_long natm0_recvspace = 16*1024;

struct mtx natm_mtx;

/*
 * user requests
 */
static int natm_usr_attach(struct socket *, int, d_thread_t *);
static int natm_usr_detach(struct socket *);
static int natm_usr_connect(struct socket *, struct sockaddr *, d_thread_t *);
static int natm_usr_disconnect(struct socket *);
static int natm_usr_shutdown(struct socket *);
static int natm_usr_send(struct socket *, int, struct mbuf *,
    struct sockaddr *, struct mbuf *, d_thread_t *);
static int natm_usr_peeraddr(struct socket *, struct sockaddr **);
static int natm_usr_control(struct socket *, u_long, caddr_t,
    struct ifnet *, d_thread_t *);
static int natm_usr_abort(struct socket *);
static int natm_usr_bind(struct socket *, struct sockaddr *, d_thread_t *);
static int natm_usr_sockaddr(struct socket *, struct sockaddr **);

static int
natm_usr_attach(struct socket *so, int proto, d_thread_t *p)
{
    struct natmpcb *npcb;
    int error = 0;

    npcb = (struct natmpcb *)so->so_pcb;

    KASSERT(npcb == NULL, ("natm_usr_attach: so_pcb != NULL"));

    if (so->so_snd.sb_hiwat == 0 || so->so_rcv.sb_hiwat == 0) {
	if (proto == PROTO_NATMAAL5) 
	    error = soreserve(so, natm5_sendspace, natm5_recvspace);
	else
	    error = soreserve(so, natm0_sendspace, natm0_recvspace);
        if (error)
          goto out;
    }

    so->so_pcb = (caddr_t) (npcb = npcb_alloc(M_WAITOK));
    npcb->npcb_socket = so;
out:
    return (error);
}

static int
natm_usr_detach(struct socket *so)
{
    struct natmpcb *npcb;
    int error = 0;

    NATM_LOCK();
    npcb = (struct natmpcb *)so->so_pcb;
    if (npcb == NULL) {
	/* XXXRW: Does this case ever actually happen? */
	error = EINVAL;
	goto out;
    }

    /*
     * we turn on 'drain' *before* we sofree.
     */
    npcb_free(npcb, NPCB_DESTROY);	/* drain */
    ACCEPT_LOCK();
    SOCK_LOCK(so);
    so->so_pcb = NULL;
    sotryfree(so);
 out:
    NATM_UNLOCK();
    return (error);
}

static int
natm_usr_connect(struct socket *so, struct sockaddr *nam, d_thread_t *p)
{
    struct natmpcb *npcb;
    struct sockaddr_natm *snatm;
    struct atmio_openvcc op;
    struct ifnet *ifp;
    int error = 0;
    int proto = so->so_proto->pr_protocol;

    NATM_LOCK();
    npcb = (struct natmpcb *)so->so_pcb;
    if (npcb == NULL) {
	/* XXXRW: Does this case ever actually happen? */
	error = EINVAL;
	goto out;
    }

    /*
     * validate nam and npcb
     */
    snatm = (struct sockaddr_natm *)nam;
    if (snatm->snatm_len != sizeof(*snatm) ||
	(npcb->npcb_flags & NPCB_FREE) == 0) {
	error = EINVAL;
	goto out;
    }
    if (snatm->snatm_family != AF_NATM) {
	error = EAFNOSUPPORT;
	goto out;
    }

    snatm->snatm_if[IFNAMSIZ - 1] = '\0';	/* XXX ensure null termination
						   since ifunit() uses strcmp */

    /*
     * convert interface string to ifp, validate.
     */
    ifp = ifunit(snatm->snatm_if);
    if (ifp == NULL || (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
	error = ENXIO;
	goto out;
    }
    if (ifp->if_output != atm_output) {
	error = EAFNOSUPPORT;
	goto out;
    }

    /*
     * register us with the NATM PCB layer
     */
    if (npcb_add(npcb, ifp, snatm->snatm_vci, snatm->snatm_vpi) != npcb) {
        error = EADDRINUSE;
        goto out;
    }
    NATM_UNLOCK();

    /*
     * open the channel
     */
    bzero(&op, sizeof(op));
    op.rxhand = npcb;
    op.param.flags = ATMIO_FLAG_PVC;
    op.param.vpi = npcb->npcb_vpi;
    op.param.vci = npcb->npcb_vci;
    op.param.rmtu = op.param.tmtu = ifp->if_mtu;
    op.param.aal = (proto == PROTO_NATMAAL5) ? ATMIO_AAL_5 : ATMIO_AAL_0;
    op.param.traffic = ATMIO_TRAFFIC_UBR;

    IFF_LOCKGIANT(ifp);
    if (ifp->if_ioctl == NULL || 
	ifp->if_ioctl(ifp, SIOCATMOPENVCC, (caddr_t)&op) != 0) {
	IFF_UNLOCKGIANT(ifp);
	NATM_LOCK();
	npcb_free(npcb, NPCB_REMOVE);
        error = EIO;
	goto out;
    }
    IFF_UNLOCKGIANT(ifp);

    NATM_LOCK();
    soisconnected(so);

 out:
    NATM_UNLOCK();
    return (error);
}

static int
natm_usr_disconnect(struct socket *so)
{
    struct natmpcb *npcb;
    struct atmio_closevcc cl;
    struct ifnet *ifp;
    int error = 0;

    NATM_LOCK();
    npcb = (struct natmpcb *)so->so_pcb;
    if (npcb == NULL) {
	/* XXXRW: Does this case ever actually happen? */
	error = EINVAL;
	goto out;
    }

    if ((npcb->npcb_flags & NPCB_CONNECTED) == 0) {
        printf("natm: disconnected check\n");
        error = EIO;
	goto out;
    }
    ifp = npcb->npcb_ifp;

    /*
     * disable rx
     */
    cl.vpi = npcb->npcb_vpi;
    cl.vci = npcb->npcb_vci;
    NATM_UNLOCK();
    if (ifp->if_ioctl != NULL) {
	IFF_LOCKGIANT(ifp);
	ifp->if_ioctl(ifp, SIOCATMCLOSEVCC, (caddr_t)&cl);
	IFF_UNLOCKGIANT(ifp);
    }
    NATM_LOCK();

    npcb_free(npcb, NPCB_REMOVE);
    soisdisconnected(so);

 out:
    NATM_UNLOCK();
    return (error);
}

static int
natm_usr_shutdown(struct socket *so)
{
    socantsendmore(so);
    return (0);
}

static int
natm_usr_send(struct socket *so, int flags, struct mbuf *m, 
    struct sockaddr *nam, struct mbuf *control, d_thread_t *p)
{
    struct natmpcb *npcb;
    struct atm_pseudohdr *aph;
    int error = 0;
    int proto = so->so_proto->pr_protocol;

    NATM_LOCK();
    npcb = (struct natmpcb *)so->so_pcb;
    if (npcb == NULL) {
	/* XXXRW: Does this case ever actually happen? */
	error = EINVAL;
	goto out;
    }

    if (control && control->m_len) {
	m_freem(control);
	m_freem(m);
	error = EINVAL;
	goto out;
    }

    /*
     * send the data.   we must put an atm_pseudohdr on first
     */
    M_PREPEND(m, sizeof(*aph), M_DONTWAIT);
    if (m == NULL) {
        error = ENOBUFS;
	goto out;
    }
    aph = mtod(m, struct atm_pseudohdr *);
    ATM_PH_VPI(aph) = npcb->npcb_vpi;
    ATM_PH_SETVCI(aph, npcb->npcb_vci);
    ATM_PH_FLAGS(aph) = (proto == PROTO_NATMAAL5) ? ATM_PH_AAL5 : 0;

    error = atm_output(npcb->npcb_ifp, m, NULL, NULL);

 out:
    NATM_UNLOCK();
    return (error);
}

static int
natm_usr_peeraddr(struct socket *so, struct sockaddr **nam)
{
    struct natmpcb *npcb;
    struct sockaddr_natm *snatm, ssnatm;

    NATM_LOCK();
    npcb = (struct natmpcb *)so->so_pcb;
    if (npcb == NULL) {
	/* XXXRW: Does this case ever actually happen? */
    	NATM_UNLOCK();
	return (EINVAL);
    }

    snatm = &ssnatm;
    bzero(snatm, sizeof(*snatm));
    snatm->snatm_len = sizeof(*snatm);
    snatm->snatm_family = AF_NATM;
    strlcpy(snatm->snatm_if, npcb->npcb_ifp->if_xname,
        sizeof(snatm->snatm_if));
    snatm->snatm_vci = npcb->npcb_vci;
    snatm->snatm_vpi = npcb->npcb_vpi;
    NATM_UNLOCK();
    *nam = sodupsockaddr((struct sockaddr *)snatm, M_WAITOK);
    return (0);
}

static int
natm_usr_control(struct socket *so, u_long cmd, caddr_t arg,
    struct ifnet *ifp, d_thread_t *p)
{
    struct natmpcb *npcb;
    int error;

    /*
     * XXXRW: Does this case ever actually happen?  And does it even matter
     * given that npcb is unused?
     */
    npcb = (struct natmpcb *)so->so_pcb;
    if (npcb == NULL)
	return (EINVAL);

    if (ifp == NULL || ifp->if_ioctl == NULL)
	return (EOPNOTSUPP);

    IFF_LOCKGIANT(ifp);
    error = ((*ifp->if_ioctl)(ifp, cmd, arg));
    IFF_UNLOCKGIANT(ifp);
    return (error);
}

static int
natm_usr_abort(struct socket *so)
{
    return (natm_usr_shutdown(so));
}

static int
natm_usr_bind(struct socket *so, struct sockaddr *nam, d_thread_t *p)
{
    return (EOPNOTSUPP);
}

static int
natm_usr_sockaddr(struct socket *so, struct sockaddr **nam)
{
    return (EOPNOTSUPP);
}

/* xxx - should be const */
struct pr_usrreqs natm_usrreqs = {
	.pru_abort =		natm_usr_abort,
	.pru_attach =		natm_usr_attach,
	.pru_bind =		natm_usr_bind,
	.pru_connect =		natm_usr_connect,
	.pru_control =		natm_usr_control,
	.pru_detach =		natm_usr_detach,
	.pru_disconnect =	natm_usr_disconnect,
	.pru_peeraddr =		natm_usr_peeraddr,
	.pru_send =		natm_usr_send,
	.pru_shutdown =		natm_usr_shutdown,
	.pru_sockaddr =		natm_usr_sockaddr,
};

/*
 * natmintr: interrupt
 *
 * note: we expect a socket pointer in rcvif rather than an interface
 * pointer.    we can get the interface pointer from the so's PCB if
 * we really need it.
 */
void
natmintr(struct mbuf *m)
{
	struct socket *so;
	struct natmpcb *npcb;

#ifdef DIAGNOSTIC
	M_ASSERTPKTHDR(m);
#endif

	NATM_LOCK();
	npcb = (struct natmpcb *)m->m_pkthdr.rcvif;	/* XXX: overloaded */
	so = npcb->npcb_socket;

	npcb->npcb_inq--;

	if (npcb->npcb_flags & NPCB_DRAIN) {
		if (npcb->npcb_inq == 0)
			FREE(npcb, M_PCB);			/* done! */
		NATM_UNLOCK();
		m_freem(m);
		return;
	}

	if (npcb->npcb_flags & NPCB_FREE) {
		NATM_UNLOCK();
		m_freem(m);					/* drop */
		return;
	}

#ifdef NEED_TO_RESTORE_IFP
	m->m_pkthdr.rcvif = npcb->npcb_ifp;
#else
#ifdef DIAGNOSTIC
	m->m_pkthdr.rcvif = NULL;	/* null it out to be safe */
#endif
#endif

	if (sbspace(&so->so_rcv) > m->m_pkthdr.len) {
#ifdef NATM_STAT
		natm_sookcnt++;
		natm_sookbytes += m->m_pkthdr.len;
#endif
		sbappendrecord(&so->so_rcv, m);
		sorwakeup(so);
		NATM_UNLOCK();
	} else {
#ifdef NATM_STAT
		natm_sodropcnt++;
		natm_sodropbytes += m->m_pkthdr.len;
#endif
		NATM_UNLOCK();
		m_freem(m);
	}
}

/* 
 * natm0_sysctl: not used, but here in case we want to add something
 * later...
 */
int
natm0_sysctl(SYSCTL_HANDLER_ARGS)
{
	/* All sysctl names at this level are terminal. */
	return (ENOENT);
}

/* 
 * natm5_sysctl: not used, but here in case we want to add something
 * later...
 */
int
natm5_sysctl(SYSCTL_HANDLER_ARGS)
{
	/* All sysctl names at this level are terminal. */
	return (ENOENT);
}
