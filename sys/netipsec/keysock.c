/*	$KAME: keysock.c,v 1.25 2001/08/13 20:07:41 itojun Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_ipsec.h"

/* This code has derived from sys/net/rtsock.c on FreeBSD2.2.5 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/domain.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/protosw.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/vnet.h>

#include <netinet/in.h>

#include <net/pfkeyv2.h>
#include <netipsec/key.h>
#include <netipsec/keysock.h>
#include <netipsec/key_debug.h>
#include <netipsec/ipsec.h>

#include <machine/stdarg.h>

static struct mtx keysock_mtx;
MTX_SYSINIT(keysock, &keysock_mtx, "key socket pcb list", MTX_DEF);

#define	KEYSOCK_LOCK()		mtx_lock(&keysock_mtx)
#define	KEYSOCK_UNLOCK()	mtx_unlock(&keysock_mtx)

VNET_DEFINE_STATIC(LIST_HEAD(, keycb), keycb_list) =
    LIST_HEAD_INITIALIZER(keycb_list);
#define	V_keycb_list		VNET(keycb_list)

static struct sockaddr key_src = { 2, PF_KEY, };

static int key_sendup0(struct keycb *, struct mbuf *, int);

VNET_PCPUSTAT_DEFINE(struct pfkeystat, pfkeystat);
VNET_PCPUSTAT_SYSINIT(pfkeystat);

#ifdef VIMAGE
VNET_PCPUSTAT_SYSUNINIT(pfkeystat);
#endif /* VIMAGE */

static int
key_send(struct socket *so, int flags, struct mbuf *m,
    struct sockaddr *nam, struct mbuf *control, struct thread *td)
{
	struct sadb_msg *msg;
	int len, error = 0;

	if ((flags & PRUS_OOB) || control != NULL) {
		m_freem(m);
		if (control != NULL)
			m_freem(control);
		return (EOPNOTSUPP);
	}

	PFKEYSTAT_INC(out_total);
	PFKEYSTAT_ADD(out_bytes, m->m_pkthdr.len);

	len = m->m_pkthdr.len;
	if (len < sizeof(struct sadb_msg)) {
		PFKEYSTAT_INC(out_tooshort);
		error = EINVAL;
		goto end;
	}

	if (m->m_len < sizeof(struct sadb_msg)) {
		if ((m = m_pullup(m, sizeof(struct sadb_msg))) == NULL) {
			PFKEYSTAT_INC(out_nomem);
			error = ENOBUFS;
			goto end;
		}
	}

	M_ASSERTPKTHDR(m);

	KEYDBG(KEY_DUMP, kdebug_mbuf(m));

	msg = mtod(m, struct sadb_msg *);
	PFKEYSTAT_INC(out_msgtype[msg->sadb_msg_type]);
	if (len != PFKEY_UNUNIT64(msg->sadb_msg_len)) {
		PFKEYSTAT_INC(out_invlen);
		error = EINVAL;
		goto end;
	}

	error = key_parse(m, so);
	m = NULL;
end:
	if (m != NULL)
		m_freem(m);
	return (error);
}

/*
 * send message to the socket.
 */
static int
key_sendup0(struct keycb *kp, struct mbuf *m, int promisc)
{

	if (promisc) {
		struct sadb_msg *pmsg;

		M_PREPEND(m, sizeof(struct sadb_msg), M_NOWAIT);
		if (m == NULL) {
			PFKEYSTAT_INC(in_nomem);
			return (ENOBUFS);
		}
		pmsg = mtod(m, struct sadb_msg *);
		bzero(pmsg, sizeof(*pmsg));
		pmsg->sadb_msg_version = PF_KEY_V2;
		pmsg->sadb_msg_type = SADB_X_PROMISC;
		pmsg->sadb_msg_len = PFKEY_UNIT64(m->m_pkthdr.len);
		/* pid and seq? */

		PFKEYSTAT_INC(in_msgtype[pmsg->sadb_msg_type]);
	}

	if (!sbappendaddr(&kp->kp_socket->so_rcv, &key_src, m, NULL)) {
		PFKEYSTAT_INC(in_nomem);
		m_freem(m);
		soroverflow(kp->kp_socket);
		return (ENOBUFS);
	}

	sorwakeup(kp->kp_socket);
	return (0);
}

/* so can be NULL if target != KEY_SENDUP_ONE */
int
key_sendup_mbuf(struct socket *so, struct mbuf *m, int target)
{
	struct mbuf *n;
	struct keycb *kp;
	int error = 0;

	KASSERT(m != NULL, ("NULL mbuf pointer was passed."));
	KASSERT(so != NULL || target != KEY_SENDUP_ONE,
	    ("NULL socket pointer was passed."));
	KASSERT(target == KEY_SENDUP_ONE || target == KEY_SENDUP_ALL ||
	    target == KEY_SENDUP_REGISTERED, ("Wrong target %d", target));

	PFKEYSTAT_INC(in_total);
	PFKEYSTAT_ADD(in_bytes, m->m_pkthdr.len);
	if (m->m_len < sizeof(struct sadb_msg)) {
		m = m_pullup(m, sizeof(struct sadb_msg));
		if (m == NULL) {
			PFKEYSTAT_INC(in_nomem);
			return (ENOBUFS);
		}
	}
	if (m->m_len >= sizeof(struct sadb_msg)) {
		struct sadb_msg *msg;
		msg = mtod(m, struct sadb_msg *);
		PFKEYSTAT_INC(in_msgtype[msg->sadb_msg_type]);
	}
	KEYSOCK_LOCK();
	LIST_FOREACH(kp, &V_keycb_list, kp_next) {
		/*
		 * If you are in promiscuous mode, and when you get broadcasted
		 * reply, you'll get two PF_KEY messages.
		 * (based on pf_key@inner.net message on 14 Oct 1998)
		 */
		if (kp->kp_promisc) {
			n = m_copym(m, 0, M_COPYALL, M_NOWAIT);
			if (n != NULL)
				key_sendup0(kp, n, 1);
			else
				PFKEYSTAT_INC(in_nomem);
		}

		/* the exact target will be processed later */
		if (so != NULL && so->so_pcb == kp)
			continue;

		if (target == KEY_SENDUP_ONE || (target ==
		    KEY_SENDUP_REGISTERED && kp->kp_registered == 0))
			continue;

		/* KEY_SENDUP_ALL + KEY_SENDUP_REGISTERED */
		n = m_copym(m, 0, M_COPYALL, M_NOWAIT);
		if (n == NULL) {
			PFKEYSTAT_INC(in_nomem);
			/* Try send to another socket */
			continue;
		}

		if (key_sendup0(kp, n, 0) == 0)
			PFKEYSTAT_INC(in_msgtarget[target]);
	}

	if (so)	{ /* KEY_SENDUP_ONE */
		error = key_sendup0(so->so_pcb, m, 0);
		if (error == 0)
			PFKEYSTAT_INC(in_msgtarget[KEY_SENDUP_ONE]);
	} else {
		error = 0;
		m_freem(m);
	}
	KEYSOCK_UNLOCK();
	return (error);
}

static u_long key_sendspace = 8192;
SYSCTL_ULONG(_net_key, OID_AUTO, sendspace, CTLFLAG_RW, &key_sendspace, 0,
    "Default key socket send space");
static u_long key_recvspace = 8192;
SYSCTL_ULONG(_net_key, OID_AUTO, recvspace, CTLFLAG_RW, &key_recvspace, 0,
    "Default key socket receive space");

static int
key_attach(struct socket *so, int proto, struct thread *td)
{
	struct keycb *kp;
	int error;

	KASSERT(so->so_pcb == NULL, ("key_attach: so_pcb != NULL"));

	if (td != NULL) {
		error = priv_check(td, PRIV_NET_RAW);
		if (error != 0)
			return (error);
	}

	error = soreserve(so, key_sendspace, key_recvspace);
	if (error != 0)
		return (error);

	kp = malloc(sizeof(*kp), M_PCB, M_WAITOK);
	kp->kp_socket = so;
	kp->kp_promisc = kp->kp_registered = 0;

	so->so_pcb = kp;
	so->so_options |= SO_USELOOPBACK;

	KEYSOCK_LOCK();
	LIST_INSERT_HEAD(&V_keycb_list, kp, kp_next);
	KEYSOCK_UNLOCK();
	soisconnected(so);

	return (0);
}

static void
key_close(struct socket *so)
{

	soisdisconnected(so);
}

static void
key_detach(struct socket *so)
{
	struct keycb *kp = so->so_pcb;

	key_freereg(so);
	KEYSOCK_LOCK();
	LIST_REMOVE(kp, kp_next);
	KEYSOCK_UNLOCK();
	free(kp, M_PCB);
	so->so_pcb = NULL;
}

static int
key_shutdown(struct socket *so)
{
	socantsendmore(so);
	return (0);
}

SYSCTL_NODE(_net, PF_KEY, key, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Key Family");

static struct protosw keysw = {
	.pr_type =		SOCK_RAW,
	.pr_protocol =		PF_KEY_V2,
	.pr_flags =		PR_ATOMIC | PR_ADDR,
	.pr_abort =		key_close,
	.pr_attach =		key_attach,
	.pr_detach =		key_detach,
	.pr_send =		key_send,
	.pr_shutdown =		key_shutdown,
	.pr_close =		key_close,
};

static struct domain keydomain = {
	.dom_family =		PF_KEY,
	.dom_name =		"key",
	.dom_nprotosw =		1,
	.dom_protosw =		{ &keysw },
};
DOMAIN_SET(key);
