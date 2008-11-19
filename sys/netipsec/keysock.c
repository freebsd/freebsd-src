/*	$FreeBSD$	*/
/*	$KAME: keysock.c,v 1.25 2001/08/13 20:07:41 itojun Exp $	*/

/*-
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
#include <sys/vimage.h>

#include <net/if.h>
#include <net/raw_cb.h>
#include <net/route.h>

#include <netinet/in.h>

#include <net/pfkeyv2.h>
#include <netipsec/key.h>
#include <netipsec/keysock.h>
#include <netipsec/key_debug.h>
#include <netipsec/ipsec.h>

#include <machine/stdarg.h>

struct key_cb {
	int key_count;
	int any_count;
};

#ifdef VIMAGE_GLOBALS
static struct key_cb key_cb;
struct pfkeystat pfkeystat;
#endif

static struct sockaddr key_src = { 2, PF_KEY, };

static int key_sendup0 __P((struct rawcb *, struct mbuf *, int));

/*
 * key_output()
 */
int
key_output(struct mbuf *m, struct socket *so)
{
	INIT_VNET_IPSEC(curvnet);
	struct sadb_msg *msg;
	int len, error = 0;

	if (m == 0)
		panic("%s: NULL pointer was passed.\n", __func__);

	V_pfkeystat.out_total++;
	V_pfkeystat.out_bytes += m->m_pkthdr.len;

	len = m->m_pkthdr.len;
	if (len < sizeof(struct sadb_msg)) {
		V_pfkeystat.out_tooshort++;
		error = EINVAL;
		goto end;
	}

	if (m->m_len < sizeof(struct sadb_msg)) {
		if ((m = m_pullup(m, sizeof(struct sadb_msg))) == 0) {
			V_pfkeystat.out_nomem++;
			error = ENOBUFS;
			goto end;
		}
	}

	M_ASSERTPKTHDR(m);

	KEYDEBUG(KEYDEBUG_KEY_DUMP, kdebug_mbuf(m));

	msg = mtod(m, struct sadb_msg *);
	V_pfkeystat.out_msgtype[msg->sadb_msg_type]++;
	if (len != PFKEY_UNUNIT64(msg->sadb_msg_len)) {
		V_pfkeystat.out_invlen++;
		error = EINVAL;
		goto end;
	}

	error = key_parse(m, so);
	m = NULL;
end:
	if (m)
		m_freem(m);
	return error;
}

/*
 * send message to the socket.
 */
static int
key_sendup0(rp, m, promisc)
	struct rawcb *rp;
	struct mbuf *m;
	int promisc;
{
	INIT_VNET_IPSEC(curvnet);
	int error;

	if (promisc) {
		struct sadb_msg *pmsg;

		M_PREPEND(m, sizeof(struct sadb_msg), M_DONTWAIT);
		if (m && m->m_len < sizeof(struct sadb_msg))
			m = m_pullup(m, sizeof(struct sadb_msg));
		if (!m) {
			V_pfkeystat.in_nomem++;
			m_freem(m);
			return ENOBUFS;
		}
		m->m_pkthdr.len += sizeof(*pmsg);

		pmsg = mtod(m, struct sadb_msg *);
		bzero(pmsg, sizeof(*pmsg));
		pmsg->sadb_msg_version = PF_KEY_V2;
		pmsg->sadb_msg_type = SADB_X_PROMISC;
		pmsg->sadb_msg_len = PFKEY_UNIT64(m->m_pkthdr.len);
		/* pid and seq? */

		V_pfkeystat.in_msgtype[pmsg->sadb_msg_type]++;
	}

	if (!sbappendaddr(&rp->rcb_socket->so_rcv, (struct sockaddr *)&V_key_src,
	    m, NULL)) {
		V_pfkeystat.in_nomem++;
		m_freem(m);
		error = ENOBUFS;
	} else
		error = 0;
	sorwakeup(rp->rcb_socket);
	return error;
}

/* XXX this interface should be obsoleted. */
int
key_sendup(so, msg, len, target)
	struct socket *so;
	struct sadb_msg *msg;
	u_int len;
	int target;	/*target of the resulting message*/
{
	INIT_VNET_IPSEC(curvnet);
	struct mbuf *m, *n, *mprev;
	int tlen;

	/* sanity check */
	if (so == 0 || msg == 0)
		panic("%s: NULL pointer was passed.\n", __func__);

	KEYDEBUG(KEYDEBUG_KEY_DUMP,
		printf("%s: \n", __func__);
		kdebug_sadb(msg));

	/*
	 * we increment statistics here, just in case we have ENOBUFS
	 * in this function.
	 */
	V_pfkeystat.in_total++;
	V_pfkeystat.in_bytes += len;
	V_pfkeystat.in_msgtype[msg->sadb_msg_type]++;

	/*
	 * Get mbuf chain whenever possible (not clusters),
	 * to save socket buffer.  We'll be generating many SADB_ACQUIRE
	 * messages to listening key sockets.  If we simply allocate clusters,
	 * sbappendaddr() will raise ENOBUFS due to too little sbspace().
	 * sbspace() computes # of actual data bytes AND mbuf region.
	 *
	 * TODO: SADB_ACQUIRE filters should be implemented.
	 */
	tlen = len;
	m = mprev = NULL;
	while (tlen > 0) {
		if (tlen == len) {
			MGETHDR(n, M_DONTWAIT, MT_DATA);
			if (n == NULL) {
				V_pfkeystat.in_nomem++;
				return ENOBUFS;
			}
			n->m_len = MHLEN;
		} else {
			MGET(n, M_DONTWAIT, MT_DATA);
			if (n == NULL) {
				V_pfkeystat.in_nomem++;
				return ENOBUFS;
			}
			n->m_len = MLEN;
		}
		if (tlen >= MCLBYTES) {	/*XXX better threshold? */
			MCLGET(n, M_DONTWAIT);
			if ((n->m_flags & M_EXT) == 0) {
				m_free(n);
				m_freem(m);
				V_pfkeystat.in_nomem++;
				return ENOBUFS;
			}
			n->m_len = MCLBYTES;
		}

		if (tlen < n->m_len)
			n->m_len = tlen;
		n->m_next = NULL;
		if (m == NULL)
			m = mprev = n;
		else {
			mprev->m_next = n;
			mprev = n;
		}
		tlen -= n->m_len;
		n = NULL;
	}
	m->m_pkthdr.len = len;
	m->m_pkthdr.rcvif = NULL;
	m_copyback(m, 0, len, (caddr_t)msg);

	/* avoid duplicated statistics */
	V_pfkeystat.in_total--;
	V_pfkeystat.in_bytes -= len;
	V_pfkeystat.in_msgtype[msg->sadb_msg_type]--;

	return key_sendup_mbuf(so, m, target);
}

/* so can be NULL if target != KEY_SENDUP_ONE */
int
key_sendup_mbuf(so, m, target)
	struct socket *so;
	struct mbuf *m;
	int target;
{
	INIT_VNET_NET(curvnet);
	INIT_VNET_IPSEC(curvnet);
	struct mbuf *n;
	struct keycb *kp;
	int sendup;
	struct rawcb *rp;
	int error = 0;

	if (m == NULL)
		panic("key_sendup_mbuf: NULL pointer was passed.\n");
	if (so == NULL && target == KEY_SENDUP_ONE)
		panic("%s: NULL pointer was passed.\n", __func__);

	V_pfkeystat.in_total++;
	V_pfkeystat.in_bytes += m->m_pkthdr.len;
	if (m->m_len < sizeof(struct sadb_msg)) {
		m = m_pullup(m, sizeof(struct sadb_msg));
		if (m == NULL) {
			V_pfkeystat.in_nomem++;
			return ENOBUFS;
		}
	}
	if (m->m_len >= sizeof(struct sadb_msg)) {
		struct sadb_msg *msg;
		msg = mtod(m, struct sadb_msg *);
		V_pfkeystat.in_msgtype[msg->sadb_msg_type]++;
	}
	mtx_lock(&rawcb_mtx);
	LIST_FOREACH(rp, &V_rawcb_list, list)
	{
		if (rp->rcb_proto.sp_family != PF_KEY)
			continue;
		if (rp->rcb_proto.sp_protocol
		 && rp->rcb_proto.sp_protocol != PF_KEY_V2) {
			continue;
		}

		kp = (struct keycb *)rp;

		/*
		 * If you are in promiscuous mode, and when you get broadcasted
		 * reply, you'll get two PF_KEY messages.
		 * (based on pf_key@inner.net message on 14 Oct 1998)
		 */
		if (((struct keycb *)rp)->kp_promisc) {
			if ((n = m_copy(m, 0, (int)M_COPYALL)) != NULL) {
				(void)key_sendup0(rp, n, 1);
				n = NULL;
			}
		}

		/* the exact target will be processed later */
		if (so && sotorawcb(so) == rp)
			continue;

		sendup = 0;
		switch (target) {
		case KEY_SENDUP_ONE:
			/* the statement has no effect */
			if (so && sotorawcb(so) == rp)
				sendup++;
			break;
		case KEY_SENDUP_ALL:
			sendup++;
			break;
		case KEY_SENDUP_REGISTERED:
			if (kp->kp_registered)
				sendup++;
			break;
		}
		V_pfkeystat.in_msgtarget[target]++;

		if (!sendup)
			continue;

		if ((n = m_copy(m, 0, (int)M_COPYALL)) == NULL) {
			m_freem(m);
			V_pfkeystat.in_nomem++;
			mtx_unlock(&rawcb_mtx);
			return ENOBUFS;
		}

		if ((error = key_sendup0(rp, n, 0)) != 0) {
			m_freem(m);
			mtx_unlock(&rawcb_mtx);
			return error;
		}

		n = NULL;
	}

	if (so) {
		error = key_sendup0(sotorawcb(so), m, 0);
		m = NULL;
	} else {
		error = 0;
		m_freem(m);
	}
	mtx_unlock(&rawcb_mtx);
	return error;
}

/*
 * key_abort()
 * derived from net/rtsock.c:rts_abort()
 */
static void
key_abort(struct socket *so)
{
	raw_usrreqs.pru_abort(so);
}

/*
 * key_attach()
 * derived from net/rtsock.c:rts_attach()
 */
static int
key_attach(struct socket *so, int proto, struct thread *td)
{
	INIT_VNET_IPSEC(curvnet);
	struct keycb *kp;
	int error;

	KASSERT(so->so_pcb == NULL, ("key_attach: so_pcb != NULL"));

	if (td != NULL) {
		error = priv_check(td, PRIV_NET_RAW);
		if (error)
			return error;
	}

	/* XXX */
	kp = malloc(sizeof *kp, M_PCB, M_WAITOK | M_ZERO); 
	if (kp == 0)
		return ENOBUFS;

	so->so_pcb = (caddr_t)kp;
	error = raw_attach(so, proto);
	kp = (struct keycb *)sotorawcb(so);
	if (error) {
		free(kp, M_PCB);
		so->so_pcb = (caddr_t) 0;
		return error;
	}

	kp->kp_promisc = kp->kp_registered = 0;

	if (kp->kp_raw.rcb_proto.sp_protocol == PF_KEY) /* XXX: AF_KEY */
		V_key_cb.key_count++;
	V_key_cb.any_count++;
	soisconnected(so);
	so->so_options |= SO_USELOOPBACK;

	return 0;
}

/*
 * key_bind()
 * derived from net/rtsock.c:rts_bind()
 */
static int
key_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{
  return EINVAL;
}

/*
 * key_close()
 * derived from net/rtsock.c:rts_close().
 */
static void
key_close(struct socket *so)
{

	raw_usrreqs.pru_close(so);
}

/*
 * key_connect()
 * derived from net/rtsock.c:rts_connect()
 */
static int
key_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{
	return EINVAL;
}

/*
 * key_detach()
 * derived from net/rtsock.c:rts_detach()
 */
static void
key_detach(struct socket *so)
{
	INIT_VNET_IPSEC(curvnet);
	struct keycb *kp = (struct keycb *)sotorawcb(so);

	KASSERT(kp != NULL, ("key_detach: kp == NULL"));
	if (kp->kp_raw.rcb_proto.sp_protocol
	    == PF_KEY) /* XXX: AF_KEY */
		V_key_cb.key_count--;
	V_key_cb.any_count--;

	key_freereg(so);
	raw_usrreqs.pru_detach(so);
}

/*
 * key_disconnect()
 * derived from net/rtsock.c:key_disconnect()
 */
static int
key_disconnect(struct socket *so)
{
	return(raw_usrreqs.pru_disconnect(so));
}

/*
 * key_peeraddr()
 * derived from net/rtsock.c:rts_peeraddr()
 */
static int
key_peeraddr(struct socket *so, struct sockaddr **nam)
{
	return(raw_usrreqs.pru_peeraddr(so, nam));
}

/*
 * key_send()
 * derived from net/rtsock.c:rts_send()
 */
static int
key_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
	 struct mbuf *control, struct thread *td)
{
	return(raw_usrreqs.pru_send(so, flags, m, nam, control, td));
}

/*
 * key_shutdown()
 * derived from net/rtsock.c:rts_shutdown()
 */
static int
key_shutdown(struct socket *so)
{
	return(raw_usrreqs.pru_shutdown(so));
}

/*
 * key_sockaddr()
 * derived from net/rtsock.c:rts_sockaddr()
 */
static int
key_sockaddr(struct socket *so, struct sockaddr **nam)
{
	return(raw_usrreqs.pru_sockaddr(so, nam));
}

struct pr_usrreqs key_usrreqs = {
	.pru_abort =		key_abort,
	.pru_attach =		key_attach,
	.pru_bind =		key_bind,
	.pru_connect =		key_connect,
	.pru_detach =		key_detach,
	.pru_disconnect =	key_disconnect,
	.pru_peeraddr =		key_peeraddr,
	.pru_send =		key_send,
	.pru_shutdown =		key_shutdown,
	.pru_sockaddr =		key_sockaddr,
	.pru_close =		key_close,
};

/* sysctl */
SYSCTL_NODE(_net, PF_KEY, key, CTLFLAG_RW, 0, "Key Family");

/*
 * Definitions of protocols supported in the KEY domain.
 */

extern struct domain keydomain;

struct protosw keysw[] = {
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&keydomain,
	.pr_protocol =		PF_KEY_V2,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_output =		key_output,
	.pr_ctlinput =		raw_ctlinput,
	.pr_init =		raw_init,
	.pr_usrreqs =		&key_usrreqs
}
};

static void
key_init0(void)
{
	INIT_VNET_IPSEC(curvnet);

	bzero((caddr_t)&V_key_cb, sizeof(V_key_cb));
	ipsec_init();
	key_init();
}

struct domain keydomain = {
	.dom_family =		PF_KEY,
	.dom_name =		"key",
	.dom_init =		key_init0,
	.dom_protosw =		keysw,
	.dom_protoswNPROTOSW =	&keysw[sizeof(keysw)/sizeof(keysw[0])]
};

DOMAIN_SET(key);
