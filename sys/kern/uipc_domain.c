/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/eventhandler.h>
#include <sys/epoch.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rmlock.h>
#include <sys/socketvar.h>
#include <sys/systm.h>

#include <machine/atomic.h>

#include <net/vnet.h>

struct domainhead domains = SLIST_HEAD_INITIALIZER(&domains);
int domain_init_status = 1;
static struct mtx dom_mtx;		/* domain list lock */
MTX_SYSINIT(domain, &dom_mtx, "domain list", MTX_DEF);

static int
pr_accept_notsupp(struct socket *so, struct sockaddr *sa)
{
	return (EOPNOTSUPP);
}

static int
pr_aio_queue_notsupp(struct socket *so, struct kaiocb *job)
{
	return (EOPNOTSUPP);
}

static int
pr_bind_notsupp(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	return (EOPNOTSUPP);
}

static int
pr_bindat_notsupp(int fd, struct socket *so, struct sockaddr *nam,
    struct thread *td)
{
	return (EOPNOTSUPP);
}

static int
pr_connect_notsupp(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	return (EOPNOTSUPP);
}

static int
pr_connectat_notsupp(int fd, struct socket *so, struct sockaddr *nam,
    struct thread *td)
{
	return (EOPNOTSUPP);
}

static int
pr_connect2_notsupp(struct socket *so1, struct socket *so2)
{
	return (EOPNOTSUPP);
}

static int
pr_control_notsupp(struct socket *so, u_long cmd, void *data,
    struct ifnet *ifp, struct thread *td)
{
	return (EOPNOTSUPP);
}

static int
pr_disconnect_notsupp(struct socket *so)
{
	return (EOPNOTSUPP);
}

static int
pr_listen_notsupp(struct socket *so, int backlog, struct thread *td)
{
	return (EOPNOTSUPP);
}

static int
pr_peeraddr_notsupp(struct socket *so, struct sockaddr *nam)
{
	return (EOPNOTSUPP);
}

static int
pr_rcvd_notsupp(struct socket *so, int flags)
{
	return (EOPNOTSUPP);
}

static int
pr_rcvoob_notsupp(struct socket *so, struct mbuf *m, int flags)
{
	return (EOPNOTSUPP);
}

static int
pr_send_notsupp(struct socket *so, int flags, struct mbuf *m,
    struct sockaddr *addr, struct mbuf *control, struct thread *td)
{
	if (control != NULL)
		m_freem(control);
	if ((flags & PRUS_NOTREADY) == 0)
		m_freem(m);
	return (EOPNOTSUPP);
}

static int
pr_ready_notsupp(struct socket *so, struct mbuf *m, int count)
{
	return (EOPNOTSUPP);
}

static int
pr_shutdown_notsupp(struct socket *so)
{
	return (EOPNOTSUPP);
}

static int
pr_sockaddr_notsupp(struct socket *so, struct sockaddr *nam)
{
	return (EOPNOTSUPP);
}

static int
pr_sosend_notsupp(struct socket *so, struct sockaddr *addr, struct uio *uio,
    struct mbuf *top, struct mbuf *control, int flags, struct thread *td)
{
	return (EOPNOTSUPP);
}

static int
pr_soreceive_notsupp(struct socket *so, struct sockaddr **paddr,
    struct uio *uio, struct mbuf **mp0, struct mbuf **controlp, int *flagsp)
{
	return (EOPNOTSUPP);
}

static int
pr_sopoll_notsupp(struct socket *so, int events, struct ucred *cred,
    struct thread *td)
{
	return (EOPNOTSUPP);
}

static void
pr_init(struct domain *dom, struct protosw *pr)
{

	KASSERT(pr->pr_attach != NULL,
	    ("%s: protocol doesn't have pr_attach", __func__));

	pr->pr_domain = dom;

#define	DEFAULT(foo, bar)	if (pr->foo == NULL) pr->foo = bar
	DEFAULT(pr_sosend, sosend_generic);
	DEFAULT(pr_soreceive, soreceive_generic);
	DEFAULT(pr_sopoll, sopoll_generic);
	DEFAULT(pr_setsbopt, sbsetopt);

#define NOTSUPP(foo)	if (pr->foo == NULL)  pr->foo = foo ## _notsupp
	NOTSUPP(pr_accept);
	NOTSUPP(pr_aio_queue);
	NOTSUPP(pr_bind);
	NOTSUPP(pr_bindat);
	NOTSUPP(pr_connect);
	NOTSUPP(pr_connect2);
	NOTSUPP(pr_connectat);
	NOTSUPP(pr_control);
	NOTSUPP(pr_disconnect);
	NOTSUPP(pr_listen);
	NOTSUPP(pr_peeraddr);
	NOTSUPP(pr_rcvd);
	NOTSUPP(pr_rcvoob);
	NOTSUPP(pr_send);
	NOTSUPP(pr_shutdown);
	NOTSUPP(pr_sockaddr);
	NOTSUPP(pr_sosend);
	NOTSUPP(pr_soreceive);
	NOTSUPP(pr_sopoll);
	NOTSUPP(pr_ready);
}

/*
 * Add a new protocol domain to the list of supported domains
 * Note: you cant unload it again because a socket may be using it.
 * XXX can't fail at this time.
 */
void
domain_add(struct domain *dp)
{
	struct protosw *pr;

	MPASS(IS_DEFAULT_VNET(curvnet));

	if (dp->dom_probe != NULL && (*dp->dom_probe)() != 0)
		return;

	for (int i = 0; i < dp->dom_nprotosw; i++)
		if ((pr = dp->dom_protosw[i]) != NULL)
			pr_init(dp, pr);

	mtx_lock(&dom_mtx);
#ifdef INVARIANTS
	struct domain *tmp;
	SLIST_FOREACH(tmp, &domains, dom_next)
		MPASS(tmp->dom_family != dp->dom_family);
#endif
	SLIST_INSERT_HEAD(&domains, dp, dom_next);
	mtx_unlock(&dom_mtx);
}

void
domain_remove(struct domain *dp)
{

	if ((dp->dom_flags & DOMF_UNLOADABLE) == 0)
		return;

	mtx_lock(&dom_mtx);
	SLIST_REMOVE(&domains, dp, domain, dom_next);
	mtx_unlock(&dom_mtx);
}

static void
domainfinalize(void *dummy)
{

	mtx_lock(&dom_mtx);
	KASSERT(domain_init_status == 1, ("domainfinalize called too late!"));
	domain_init_status = 2;
	mtx_unlock(&dom_mtx);	
}
SYSINIT(domainfin, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_FIRST, domainfinalize,
    NULL);

struct domain *
pffinddomain(int family)
{
	struct domain *dp;

	SLIST_FOREACH(dp, &domains, dom_next)
		if (dp->dom_family == family)
			return (dp);
	return (NULL);
}

struct protosw *
pffindproto(int family, int type, int proto)
{
	struct domain *dp;
	struct protosw *pr;

	dp = pffinddomain(family);
	if (dp == NULL)
		return (NULL);

	for (int i = 0; i < dp->dom_nprotosw; i++)
		if ((pr = dp->dom_protosw[i]) != NULL && pr->pr_type == type &&
		    (pr->pr_protocol == 0 || proto == 0 ||
		     pr->pr_protocol == proto))
			return (pr);

	return (NULL);
}

/*
 * The caller must make sure that the new protocol is fully set up and ready to
 * accept requests before it is registered.
 */
int
protosw_register(struct domain *dp, struct protosw *npr)
{
	struct protosw **prp;

	MPASS(dp);
	MPASS(npr && npr->pr_type > 0 && npr->pr_protocol > 0);

	prp = NULL;
	/*
	 * Protect us against races when two protocol registrations for
	 * the same protocol happen at the same time.
	 */
	mtx_lock(&dom_mtx);
	for (int i = 0; i < dp->dom_nprotosw; i++) {
		if (dp->dom_protosw[i] == NULL) {
			/* Remember the first free spacer. */
			if (prp == NULL)
				prp = &dp->dom_protosw[i];
		} else {
			/*
			 * The new protocol must not yet exist.
			 * XXXAO: Check only protocol?
			 * XXXGL: Maybe assert that it doesn't exist?
			 */
			if ((dp->dom_protosw[i]->pr_type == npr->pr_type) &&
			    (dp->dom_protosw[i]->pr_protocol ==
			    npr->pr_protocol)) {
				mtx_unlock(&dom_mtx);
				return (EEXIST);
			}

		}
	}

	/* If no free spacer is found we can't add the new protocol. */
	if (prp == NULL) {
		mtx_unlock(&dom_mtx);
		return (ENOMEM);
	}

	pr_init(dp, npr);
	*prp = npr;
	mtx_unlock(&dom_mtx);

	return (0);
}

/*
 * The caller must make sure the protocol and its functions correctly shut down
 * all sockets and release all locks and memory references.
 */
int
protosw_unregister(struct protosw *pr)
{
	struct domain *dp;
	struct protosw **prp;

	dp = pr->pr_domain;
	prp = NULL;

	mtx_lock(&dom_mtx);
	/* The protocol must exist and only once. */
	for (int i = 0; i < dp->dom_nprotosw; i++) {
		if (dp->dom_protosw[i] == pr) {
			KASSERT(prp == NULL,
			    ("%s: domain %p protocol %p registered twice\n",
			    __func__, dp, pr));
			prp = &dp->dom_protosw[i];
		}
	}

	/* Protocol does not exist.  XXXGL: assert that it does? */
	if (prp == NULL) {
		mtx_unlock(&dom_mtx);
		return (EPROTONOSUPPORT);
	}

	/* De-orbit the protocol and make the slot available again. */
	*prp = NULL;
	mtx_unlock(&dom_mtx);

	return (0);
}
