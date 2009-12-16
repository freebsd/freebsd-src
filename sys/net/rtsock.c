/*-
 * Copyright (c) 1988, 1991, 1993
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
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)rtsock.c	8.7 (Berkeley) 10/12/95
 * $FreeBSD$
 */
#include "opt_sctp.h"
#include "opt_mpath.h"
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/domain.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/rwlock.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_llatbl.h>
#include <net/netisr.h>
#include <net/raw_cb.h>
#include <net/route.h>
#include <net/vnet.h>

#include <netinet/in.h>
#ifdef INET6
#include <netinet6/scope6_var.h>
#endif

#if defined(INET) || defined(INET6)
#ifdef SCTP
extern void sctp_addr_change(struct ifaddr *ifa, int cmd);
#endif /* SCTP */
#endif

MALLOC_DEFINE(M_RTABLE, "routetbl", "routing tables");

/* NB: these are not modified */
static struct	sockaddr route_src = { 2, PF_ROUTE, };
static struct	sockaddr sa_zero   = { sizeof(sa_zero), AF_INET, };

static struct {
	int	ip_count;	/* attached w/ AF_INET */
	int	ip6_count;	/* attached w/ AF_INET6 */
	int	ipx_count;	/* attached w/ AF_IPX */
	int	any_count;	/* total attached */
} route_cb;

struct mtx rtsock_mtx;
MTX_SYSINIT(rtsock, &rtsock_mtx, "rtsock route_cb lock", MTX_DEF);

#define	RTSOCK_LOCK()	mtx_lock(&rtsock_mtx)
#define	RTSOCK_UNLOCK()	mtx_unlock(&rtsock_mtx)
#define	RTSOCK_LOCK_ASSERT()	mtx_assert(&rtsock_mtx, MA_OWNED)

SYSCTL_NODE(_net, OID_AUTO, route, CTLFLAG_RD, 0, "");

struct walkarg {
	int	w_tmemsize;
	int	w_op, w_arg;
	caddr_t	w_tmem;
	struct sysctl_req *w_req;
};

static void	rts_input(struct mbuf *m);
static struct mbuf *rt_msg1(int type, struct rt_addrinfo *rtinfo);
static int	rt_msg2(int type, struct rt_addrinfo *rtinfo,
			caddr_t cp, struct walkarg *w);
static int	rt_xaddrs(caddr_t cp, caddr_t cplim,
			struct rt_addrinfo *rtinfo);
static int	sysctl_dumpentry(struct radix_node *rn, void *vw);
static int	sysctl_iflist(int af, struct walkarg *w);
static int	sysctl_ifmalist(int af, struct walkarg *w);
static int	route_output(struct mbuf *m, struct socket *so);
static void	rt_setmetrics(u_long which, const struct rt_metrics *in,
			struct rt_metrics_lite *out);
static void	rt_getmetrics(const struct rt_metrics_lite *in,
			struct rt_metrics *out);
static void	rt_dispatch(struct mbuf *, const struct sockaddr *);

static struct netisr_handler rtsock_nh = {
	.nh_name = "rtsock",
	.nh_handler = rts_input,
	.nh_proto = NETISR_ROUTE,
	.nh_policy = NETISR_POLICY_SOURCE,
};

static int
sysctl_route_netisr_maxqlen(SYSCTL_HANDLER_ARGS)
{
	int error, qlimit;

	netisr_getqlimit(&rtsock_nh, &qlimit);
	error = sysctl_handle_int(oidp, &qlimit, 0, req);
        if (error || !req->newptr)
                return (error);
	if (qlimit < 1)
		return (EINVAL);
	return (netisr_setqlimit(&rtsock_nh, qlimit));
}
SYSCTL_PROC(_net_route, OID_AUTO, netisr_maxqlen, CTLTYPE_INT|CTLFLAG_RW,
    0, 0, sysctl_route_netisr_maxqlen, "I",
    "maximum routing socket dispatch queue length");

static void
rts_init(void)
{
	int tmp;

	if (TUNABLE_INT_FETCH("net.route.netisr_maxqlen", &tmp))
		rtsock_nh.nh_qlimit = tmp;
	netisr_register(&rtsock_nh);
}
SYSINIT(rtsock, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, rts_init, 0);

static void
rts_input(struct mbuf *m)
{
	struct sockproto route_proto;
	unsigned short *family;
	struct m_tag *tag;

	route_proto.sp_family = PF_ROUTE;
	tag = m_tag_find(m, PACKET_TAG_RTSOCKFAM, NULL);
	if (tag != NULL) {
		family = (unsigned short *)(tag + 1);
		route_proto.sp_protocol = *family;
		m_tag_delete(m, tag);
	} else
		route_proto.sp_protocol = 0;

	raw_input(m, &route_proto, &route_src);
}

/*
 * It really doesn't make any sense at all for this code to share much
 * with raw_usrreq.c, since its functionality is so restricted.  XXX
 */
static void
rts_abort(struct socket *so)
{

	raw_usrreqs.pru_abort(so);
}

static void
rts_close(struct socket *so)
{

	raw_usrreqs.pru_close(so);
}

/* pru_accept is EOPNOTSUPP */

static int
rts_attach(struct socket *so, int proto, struct thread *td)
{
	struct rawcb *rp;
	int s, error;

	KASSERT(so->so_pcb == NULL, ("rts_attach: so_pcb != NULL"));

	/* XXX */
	rp = malloc(sizeof *rp, M_PCB, M_WAITOK | M_ZERO);
	if (rp == NULL)
		return ENOBUFS;

	/*
	 * The splnet() is necessary to block protocols from sending
	 * error notifications (like RTM_REDIRECT or RTM_LOSING) while
	 * this PCB is extant but incompletely initialized.
	 * Probably we should try to do more of this work beforehand and
	 * eliminate the spl.
	 */
	s = splnet();
	so->so_pcb = (caddr_t)rp;
	so->so_fibnum = td->td_proc->p_fibnum;
	error = raw_attach(so, proto);
	rp = sotorawcb(so);
	if (error) {
		splx(s);
		so->so_pcb = NULL;
		free(rp, M_PCB);
		return error;
	}
	RTSOCK_LOCK();
	switch(rp->rcb_proto.sp_protocol) {
	case AF_INET:
		route_cb.ip_count++;
		break;
	case AF_INET6:
		route_cb.ip6_count++;
		break;
	case AF_IPX:
		route_cb.ipx_count++;
		break;
	}
	route_cb.any_count++;
	RTSOCK_UNLOCK();
	soisconnected(so);
	so->so_options |= SO_USELOOPBACK;
	splx(s);
	return 0;
}

static int
rts_bind(struct socket *so, struct sockaddr *nam, struct thread *td)
{

	return (raw_usrreqs.pru_bind(so, nam, td)); /* xxx just EINVAL */
}

static int
rts_connect(struct socket *so, struct sockaddr *nam, struct thread *td)
{

	return (raw_usrreqs.pru_connect(so, nam, td)); /* XXX just EINVAL */
}

/* pru_connect2 is EOPNOTSUPP */
/* pru_control is EOPNOTSUPP */

static void
rts_detach(struct socket *so)
{
	struct rawcb *rp = sotorawcb(so);

	KASSERT(rp != NULL, ("rts_detach: rp == NULL"));

	RTSOCK_LOCK();
	switch(rp->rcb_proto.sp_protocol) {
	case AF_INET:
		route_cb.ip_count--;
		break;
	case AF_INET6:
		route_cb.ip6_count--;
		break;
	case AF_IPX:
		route_cb.ipx_count--;
		break;
	}
	route_cb.any_count--;
	RTSOCK_UNLOCK();
	raw_usrreqs.pru_detach(so);
}

static int
rts_disconnect(struct socket *so)
{

	return (raw_usrreqs.pru_disconnect(so));
}

/* pru_listen is EOPNOTSUPP */

static int
rts_peeraddr(struct socket *so, struct sockaddr **nam)
{

	return (raw_usrreqs.pru_peeraddr(so, nam));
}

/* pru_rcvd is EOPNOTSUPP */
/* pru_rcvoob is EOPNOTSUPP */

static int
rts_send(struct socket *so, int flags, struct mbuf *m, struct sockaddr *nam,
	 struct mbuf *control, struct thread *td)
{

	return (raw_usrreqs.pru_send(so, flags, m, nam, control, td));
}

/* pru_sense is null */

static int
rts_shutdown(struct socket *so)
{

	return (raw_usrreqs.pru_shutdown(so));
}

static int
rts_sockaddr(struct socket *so, struct sockaddr **nam)
{

	return (raw_usrreqs.pru_sockaddr(so, nam));
}

static struct pr_usrreqs route_usrreqs = {
	.pru_abort =		rts_abort,
	.pru_attach =		rts_attach,
	.pru_bind =		rts_bind,
	.pru_connect =		rts_connect,
	.pru_detach =		rts_detach,
	.pru_disconnect =	rts_disconnect,
	.pru_peeraddr =		rts_peeraddr,
	.pru_send =		rts_send,
	.pru_shutdown =		rts_shutdown,
	.pru_sockaddr =		rts_sockaddr,
	.pru_close =		rts_close,
};

#ifndef _SOCKADDR_UNION_DEFINED
#define	_SOCKADDR_UNION_DEFINED
/*
 * The union of all possible address formats we handle.
 */
union sockaddr_union {
	struct sockaddr		sa;
	struct sockaddr_in	sin;
	struct sockaddr_in6	sin6;
};
#endif /* _SOCKADDR_UNION_DEFINED */

static int
rtm_get_jailed(struct rt_addrinfo *info, struct ifnet *ifp,
    struct rtentry *rt, union sockaddr_union *saun, struct ucred *cred)
{

	/* First, see if the returned address is part of the jail. */
	if (prison_if(cred, rt->rt_ifa->ifa_addr) == 0) {
		info->rti_info[RTAX_IFA] = rt->rt_ifa->ifa_addr;
		return (0);
	}

	switch (info->rti_info[RTAX_DST]->sa_family) {
#ifdef INET
	case AF_INET:
	{
		struct in_addr ia;
		struct ifaddr *ifa;
		int found;

		found = 0;
		/*
		 * Try to find an address on the given outgoing interface
		 * that belongs to the jail.
		 */
		IF_ADDR_LOCK(ifp);
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			struct sockaddr *sa;
			sa = ifa->ifa_addr;
			if (sa->sa_family != AF_INET)
				continue;
			ia = ((struct sockaddr_in *)sa)->sin_addr;
			if (prison_check_ip4(cred, &ia) == 0) {
				found = 1;
				break;
			}
		}
		IF_ADDR_UNLOCK(ifp);
		if (!found) {
			/*
			 * As a last resort return the 'default' jail address.
			 */
			ia = ((struct sockaddr_in *)rt->rt_ifa->ifa_addr)->
			    sin_addr;
			if (prison_get_ip4(cred, &ia) != 0)
				return (ESRCH);
		}
		bzero(&saun->sin, sizeof(struct sockaddr_in));
		saun->sin.sin_len = sizeof(struct sockaddr_in);
		saun->sin.sin_family = AF_INET;
		saun->sin.sin_addr.s_addr = ia.s_addr;
		info->rti_info[RTAX_IFA] = (struct sockaddr *)&saun->sin;
		break;
	}
#endif
#ifdef INET6
	case AF_INET6:
	{
		struct in6_addr ia6;
		struct ifaddr *ifa;
		int found;

		found = 0;
		/*
		 * Try to find an address on the given outgoing interface
		 * that belongs to the jail.
		 */
		IF_ADDR_LOCK(ifp);
		TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
			struct sockaddr *sa;
			sa = ifa->ifa_addr;
			if (sa->sa_family != AF_INET6)
				continue;
			bcopy(&((struct sockaddr_in6 *)sa)->sin6_addr,
			    &ia6, sizeof(struct in6_addr));
			if (prison_check_ip6(cred, &ia6) == 0) {
				found = 1;
				break;
			}
		}
		IF_ADDR_UNLOCK(ifp);
		if (!found) {
			/*
			 * As a last resort return the 'default' jail address.
			 */
			ia6 = ((struct sockaddr_in6 *)rt->rt_ifa->ifa_addr)->
			    sin6_addr;
			if (prison_get_ip6(cred, &ia6) != 0)
				return (ESRCH);
		}
		bzero(&saun->sin6, sizeof(struct sockaddr_in6));
		saun->sin6.sin6_len = sizeof(struct sockaddr_in6);
		saun->sin6.sin6_family = AF_INET6;
		bcopy(&ia6, &saun->sin6.sin6_addr, sizeof(struct in6_addr));
		if (sa6_recoverscope(&saun->sin6) != 0)
			return (ESRCH);
		info->rti_info[RTAX_IFA] = (struct sockaddr *)&saun->sin6;
		break;
	}
#endif
	default:
		return (ESRCH);
	}
	return (0);
}

/*ARGSUSED*/
static int
route_output(struct mbuf *m, struct socket *so)
{
#define	sa_equal(a1, a2) (bcmp((a1), (a2), (a1)->sa_len) == 0)
	struct rt_msghdr *rtm = NULL;
	struct rtentry *rt = NULL;
	struct radix_node_head *rnh;
	struct rt_addrinfo info;
	int len, error = 0;
	struct ifnet *ifp = NULL;
	union sockaddr_union saun;

#define senderr(e) { error = e; goto flush;}
	if (m == NULL || ((m->m_len < sizeof(long)) &&
		       (m = m_pullup(m, sizeof(long))) == NULL))
		return (ENOBUFS);
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("route_output");
	len = m->m_pkthdr.len;
	if (len < sizeof(*rtm) ||
	    len != mtod(m, struct rt_msghdr *)->rtm_msglen) {
		info.rti_info[RTAX_DST] = NULL;
		senderr(EINVAL);
	}
	R_Malloc(rtm, struct rt_msghdr *, len);
	if (rtm == NULL) {
		info.rti_info[RTAX_DST] = NULL;
		senderr(ENOBUFS);
	}
	m_copydata(m, 0, len, (caddr_t)rtm);
	if (rtm->rtm_version != RTM_VERSION) {
		info.rti_info[RTAX_DST] = NULL;
		senderr(EPROTONOSUPPORT);
	}
	rtm->rtm_pid = curproc->p_pid;
	bzero(&info, sizeof(info));
	info.rti_addrs = rtm->rtm_addrs;
	if (rt_xaddrs((caddr_t)(rtm + 1), len + (caddr_t)rtm, &info)) {
		info.rti_info[RTAX_DST] = NULL;
		senderr(EINVAL);
	}
	info.rti_flags = rtm->rtm_flags;
	if (info.rti_info[RTAX_DST] == NULL ||
	    info.rti_info[RTAX_DST]->sa_family >= AF_MAX ||
	    (info.rti_info[RTAX_GATEWAY] != NULL &&
	     info.rti_info[RTAX_GATEWAY]->sa_family >= AF_MAX))
		senderr(EINVAL);
	/*
	 * Verify that the caller has the appropriate privilege; RTM_GET
	 * is the only operation the non-superuser is allowed.
	 */
	if (rtm->rtm_type != RTM_GET) {
		error = priv_check(curthread, PRIV_NET_ROUTE);
		if (error)
			senderr(error);
	}

	/*
	 * The given gateway address may be an interface address.
	 * For example, issuing a "route change" command on a route
	 * entry that was created from a tunnel, and the gateway
	 * address given is the local end point. In this case the 
	 * RTF_GATEWAY flag must be cleared or the destination will
	 * not be reachable even though there is no error message.
	 */
	if (info.rti_info[RTAX_GATEWAY] != NULL &&
	    info.rti_info[RTAX_GATEWAY]->sa_family != AF_LINK) {
		struct route gw_ro;

		bzero(&gw_ro, sizeof(gw_ro));
		gw_ro.ro_dst = *info.rti_info[RTAX_GATEWAY];
		rtalloc_ign_fib(&gw_ro, 0, so->so_fibnum);
		/* 
		 * A host route through the loopback interface is 
		 * installed for each interface adddress. In pre 8.0
		 * releases the interface address of a PPP link type
		 * is not reachable locally. This behavior is fixed as 
		 * part of the new L2/L3 redesign and rewrite work. The
		 * signature of this interface address route is the
		 * AF_LINK sa_family type of the rt_gateway, and the
		 * rt_ifp has the IFF_LOOPBACK flag set.
		 */
		if (gw_ro.ro_rt != NULL &&
		    gw_ro.ro_rt->rt_gateway->sa_family == AF_LINK &&
		    gw_ro.ro_rt->rt_ifp->if_flags & IFF_LOOPBACK)
			info.rti_flags &= ~RTF_GATEWAY;
		if (gw_ro.ro_rt != NULL)
			RTFREE(gw_ro.ro_rt);
	}

	switch (rtm->rtm_type) {
		struct rtentry *saved_nrt;

	case RTM_ADD:
		if (info.rti_info[RTAX_GATEWAY] == NULL)
			senderr(EINVAL);
		saved_nrt = NULL;

		/* support for new ARP code */
		if (info.rti_info[RTAX_GATEWAY]->sa_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLDATA) != 0) {
			error = lla_rt_output(rtm, &info);
			break;
		}
		error = rtrequest1_fib(RTM_ADD, &info, &saved_nrt,
		    so->so_fibnum);
		if (error == 0 && saved_nrt) {
			RT_LOCK(saved_nrt);
			rt_setmetrics(rtm->rtm_inits,
				&rtm->rtm_rmx, &saved_nrt->rt_rmx);
			rtm->rtm_index = saved_nrt->rt_ifp->if_index;
			RT_REMREF(saved_nrt);
			RT_UNLOCK(saved_nrt);
		}
		break;

	case RTM_DELETE:
		saved_nrt = NULL;
		/* support for new ARP code */
		if (info.rti_info[RTAX_GATEWAY] && 
		    (info.rti_info[RTAX_GATEWAY]->sa_family == AF_LINK) &&
		    (rtm->rtm_flags & RTF_LLDATA) != 0) {
			error = lla_rt_output(rtm, &info);
			break;
		}
		error = rtrequest1_fib(RTM_DELETE, &info, &saved_nrt,
		    so->so_fibnum);
		if (error == 0) {
			RT_LOCK(saved_nrt);
			rt = saved_nrt;
			goto report;
		}
		break;

	case RTM_GET:
	case RTM_CHANGE:
	case RTM_LOCK:
		rnh = rt_tables_get_rnh(so->so_fibnum,
		    info.rti_info[RTAX_DST]->sa_family);
		if (rnh == NULL)
			senderr(EAFNOSUPPORT);
		RADIX_NODE_HEAD_RLOCK(rnh);
		rt = (struct rtentry *) rnh->rnh_lookup(info.rti_info[RTAX_DST],
			info.rti_info[RTAX_NETMASK], rnh);
		if (rt == NULL) {	/* XXX looks bogus */
			RADIX_NODE_HEAD_RUNLOCK(rnh);
			senderr(ESRCH);
		}
#ifdef RADIX_MPATH
		/*
		 * for RTM_CHANGE/LOCK, if we got multipath routes,
		 * we require users to specify a matching RTAX_GATEWAY.
		 *
		 * for RTM_GET, gate is optional even with multipath.
		 * if gate == NULL the first match is returned.
		 * (no need to call rt_mpath_matchgate if gate == NULL)
		 */
		if (rn_mpath_capable(rnh) &&
		    (rtm->rtm_type != RTM_GET || info.rti_info[RTAX_GATEWAY])) {
			rt = rt_mpath_matchgate(rt, info.rti_info[RTAX_GATEWAY]);
			if (!rt) {
				RADIX_NODE_HEAD_RUNLOCK(rnh);
				senderr(ESRCH);
			}
		}
#endif
		RT_LOCK(rt);
		RT_ADDREF(rt);
		RADIX_NODE_HEAD_RUNLOCK(rnh);

		/* 
		 * Fix for PR: 82974
		 *
		 * RTM_CHANGE/LOCK need a perfect match, rn_lookup()
		 * returns a perfect match in case a netmask is
		 * specified.  For host routes only a longest prefix
		 * match is returned so it is necessary to compare the
		 * existence of the netmask.  If both have a netmask
		 * rnh_lookup() did a perfect match and if none of them
		 * have a netmask both are host routes which is also a
		 * perfect match.
		 */

		if (rtm->rtm_type != RTM_GET && 
		    (!rt_mask(rt) != !info.rti_info[RTAX_NETMASK])) {
			RT_UNLOCK(rt);
			senderr(ESRCH);
		}

		switch(rtm->rtm_type) {

		case RTM_GET:
		report:
			RT_LOCK_ASSERT(rt);
			if ((rt->rt_flags & RTF_HOST) == 0
			    ? jailed_without_vnet(curthread->td_ucred)
			    : prison_if(curthread->td_ucred,
			    rt_key(rt)) != 0) {
				RT_UNLOCK(rt);
				senderr(ESRCH);
			}
			info.rti_info[RTAX_DST] = rt_key(rt);
			info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
			info.rti_info[RTAX_NETMASK] = rt_mask(rt);
			info.rti_info[RTAX_GENMASK] = 0;
			if (rtm->rtm_addrs & (RTA_IFP | RTA_IFA)) {
				ifp = rt->rt_ifp;
				if (ifp) {
					info.rti_info[RTAX_IFP] =
					    ifp->if_addr->ifa_addr;
					error = rtm_get_jailed(&info, ifp, rt,
					    &saun, curthread->td_ucred);
					if (error != 0) {
						RT_UNLOCK(rt);
						senderr(error);
					}
					if (ifp->if_flags & IFF_POINTOPOINT)
						info.rti_info[RTAX_BRD] =
						    rt->rt_ifa->ifa_dstaddr;
					rtm->rtm_index = ifp->if_index;
				} else {
					info.rti_info[RTAX_IFP] = NULL;
					info.rti_info[RTAX_IFA] = NULL;
				}
			} else if ((ifp = rt->rt_ifp) != NULL) {
				rtm->rtm_index = ifp->if_index;
			}
			len = rt_msg2(rtm->rtm_type, &info, NULL, NULL);
			if (len > rtm->rtm_msglen) {
				struct rt_msghdr *new_rtm;
				R_Malloc(new_rtm, struct rt_msghdr *, len);
				if (new_rtm == NULL) {
					RT_UNLOCK(rt);
					senderr(ENOBUFS);
				}
				bcopy(rtm, new_rtm, rtm->rtm_msglen);
				Free(rtm); rtm = new_rtm;
			}
			(void)rt_msg2(rtm->rtm_type, &info, (caddr_t)rtm, NULL);
			rtm->rtm_flags = rt->rt_flags;
			rt_getmetrics(&rt->rt_rmx, &rtm->rtm_rmx);
			rtm->rtm_addrs = info.rti_addrs;
			break;

		case RTM_CHANGE:
			/*
			 * New gateway could require new ifaddr, ifp;
			 * flags may also be different; ifp may be specified
			 * by ll sockaddr when protocol address is ambiguous
			 */
			if (((rt->rt_flags & RTF_GATEWAY) &&
			     info.rti_info[RTAX_GATEWAY] != NULL) ||
			    info.rti_info[RTAX_IFP] != NULL ||
			    (info.rti_info[RTAX_IFA] != NULL &&
			     !sa_equal(info.rti_info[RTAX_IFA],
				       rt->rt_ifa->ifa_addr))) {
				RT_UNLOCK(rt);
				RADIX_NODE_HEAD_LOCK(rnh);
				error = rt_getifa_fib(&info, rt->rt_fibnum);
				/*
				 * XXXRW: Really we should release this
				 * reference later, but this maintains
				 * historical behavior.
				 */
				if (info.rti_ifa != NULL)
					ifa_free(info.rti_ifa);
				RADIX_NODE_HEAD_UNLOCK(rnh);
				if (error != 0)
					senderr(error);
				RT_LOCK(rt);
			}
			if (info.rti_ifa != NULL &&
			    info.rti_ifa != rt->rt_ifa &&
			    rt->rt_ifa != NULL &&
			    rt->rt_ifa->ifa_rtrequest != NULL) {
				rt->rt_ifa->ifa_rtrequest(RTM_DELETE, rt,
				    &info);
				ifa_free(rt->rt_ifa);
			}
			if (info.rti_info[RTAX_GATEWAY] != NULL) {
				RT_UNLOCK(rt);
				RADIX_NODE_HEAD_LOCK(rnh);
				RT_LOCK(rt);
				
				error = rt_setgate(rt, rt_key(rt),
				    info.rti_info[RTAX_GATEWAY]);
				RADIX_NODE_HEAD_UNLOCK(rnh);
				if (error != 0) {
					RT_UNLOCK(rt);
					senderr(error);
				}
				rt->rt_flags |= (RTF_GATEWAY & info.rti_flags);
			}
			if (info.rti_ifa != NULL &&
			    info.rti_ifa != rt->rt_ifa) {
				ifa_ref(info.rti_ifa);
				rt->rt_ifa = info.rti_ifa;
				rt->rt_ifp = info.rti_ifp;
			}
			/* Allow some flags to be toggled on change. */
			rt->rt_flags = (rt->rt_flags & ~RTF_FMASK) |
				    (rtm->rtm_flags & RTF_FMASK);
			rt_setmetrics(rtm->rtm_inits, &rtm->rtm_rmx,
					&rt->rt_rmx);
			rtm->rtm_index = rt->rt_ifp->if_index;
			if (rt->rt_ifa && rt->rt_ifa->ifa_rtrequest)
			       rt->rt_ifa->ifa_rtrequest(RTM_ADD, rt, &info);
			/* FALLTHROUGH */
		case RTM_LOCK:
			/* We don't support locks anymore */
			break;
		}
		RT_UNLOCK(rt);
		break;

	default:
		senderr(EOPNOTSUPP);
	}

flush:
	if (rtm) {
		if (error)
			rtm->rtm_errno = error;
		else
			rtm->rtm_flags |= RTF_DONE;
	}
	if (rt)		/* XXX can this be true? */
		RTFREE(rt);
    {
	struct rawcb *rp = NULL;
	/*
	 * Check to see if we don't want our own messages.
	 */
	if ((so->so_options & SO_USELOOPBACK) == 0) {
		if (route_cb.any_count <= 1) {
			if (rtm)
				Free(rtm);
			m_freem(m);
			return (error);
		}
		/* There is another listener, so construct message */
		rp = sotorawcb(so);
	}
	if (rtm) {
		m_copyback(m, 0, rtm->rtm_msglen, (caddr_t)rtm);
		if (m->m_pkthdr.len < rtm->rtm_msglen) {
			m_freem(m);
			m = NULL;
		} else if (m->m_pkthdr.len > rtm->rtm_msglen)
			m_adj(m, rtm->rtm_msglen - m->m_pkthdr.len);
		Free(rtm);
	}
	if (m) {
		if (rp) {
			/*
			 * XXX insure we don't get a copy by
			 * invalidating our protocol
			 */
			unsigned short family = rp->rcb_proto.sp_family;
			rp->rcb_proto.sp_family = 0;
			rt_dispatch(m, info.rti_info[RTAX_DST]);
			rp->rcb_proto.sp_family = family;
		} else
			rt_dispatch(m, info.rti_info[RTAX_DST]);
	}
    }
	return (error);
#undef	sa_equal
}

static void
rt_setmetrics(u_long which, const struct rt_metrics *in,
	struct rt_metrics_lite *out)
{
#define metric(f, e) if (which & (f)) out->e = in->e;
	/*
	 * Only these are stored in the routing entry since introduction
	 * of tcp hostcache. The rest is ignored.
	 */
	metric(RTV_MTU, rmx_mtu);
	metric(RTV_WEIGHT, rmx_weight);
	/* Userland -> kernel timebase conversion. */
	if (which & RTV_EXPIRE)
		out->rmx_expire = in->rmx_expire ?
		    in->rmx_expire - time_second + time_uptime : 0;
#undef metric
}

static void
rt_getmetrics(const struct rt_metrics_lite *in, struct rt_metrics *out)
{
#define metric(e) out->e = in->e;
	bzero(out, sizeof(*out));
	metric(rmx_mtu);
	metric(rmx_weight);
	/* Kernel -> userland timebase conversion. */
	out->rmx_expire = in->rmx_expire ?
	    in->rmx_expire - time_uptime + time_second : 0;
#undef metric
}

/*
 * Extract the addresses of the passed sockaddrs.
 * Do a little sanity checking so as to avoid bad memory references.
 * This data is derived straight from userland.
 */
static int
rt_xaddrs(caddr_t cp, caddr_t cplim, struct rt_addrinfo *rtinfo)
{
	struct sockaddr *sa;
	int i;

	for (i = 0; i < RTAX_MAX && cp < cplim; i++) {
		if ((rtinfo->rti_addrs & (1 << i)) == 0)
			continue;
		sa = (struct sockaddr *)cp;
		/*
		 * It won't fit.
		 */
		if (cp + sa->sa_len > cplim)
			return (EINVAL);
		/*
		 * there are no more.. quit now
		 * If there are more bits, they are in error.
		 * I've seen this. route(1) can evidently generate these. 
		 * This causes kernel to core dump.
		 * for compatibility, If we see this, point to a safe address.
		 */
		if (sa->sa_len == 0) {
			rtinfo->rti_info[i] = &sa_zero;
			return (0); /* should be EINVAL but for compat */
		}
		/* accept it */
		rtinfo->rti_info[i] = sa;
		cp += SA_SIZE(sa);
	}
	return (0);
}

static struct mbuf *
rt_msg1(int type, struct rt_addrinfo *rtinfo)
{
	struct rt_msghdr *rtm;
	struct mbuf *m;
	int i;
	struct sockaddr *sa;
	int len, dlen;

	switch (type) {

	case RTM_DELADDR:
	case RTM_NEWADDR:
		len = sizeof(struct ifa_msghdr);
		break;

	case RTM_DELMADDR:
	case RTM_NEWMADDR:
		len = sizeof(struct ifma_msghdr);
		break;

	case RTM_IFINFO:
		len = sizeof(struct if_msghdr);
		break;

	case RTM_IFANNOUNCE:
	case RTM_IEEE80211:
		len = sizeof(struct if_announcemsghdr);
		break;

	default:
		len = sizeof(struct rt_msghdr);
	}
	if (len > MCLBYTES)
		panic("rt_msg1");
	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m && len > MHLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_free(m);
			m = NULL;
		}
	}
	if (m == NULL)
		return (m);
	m->m_pkthdr.len = m->m_len = len;
	m->m_pkthdr.rcvif = NULL;
	rtm = mtod(m, struct rt_msghdr *);
	bzero((caddr_t)rtm, len);
	for (i = 0; i < RTAX_MAX; i++) {
		if ((sa = rtinfo->rti_info[i]) == NULL)
			continue;
		rtinfo->rti_addrs |= (1 << i);
		dlen = SA_SIZE(sa);
		m_copyback(m, len, dlen, (caddr_t)sa);
		len += dlen;
	}
	if (m->m_pkthdr.len != len) {
		m_freem(m);
		return (NULL);
	}
	rtm->rtm_msglen = len;
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = type;
	return (m);
}

static int
rt_msg2(int type, struct rt_addrinfo *rtinfo, caddr_t cp, struct walkarg *w)
{
	int i;
	int len, dlen, second_time = 0;
	caddr_t cp0;

	rtinfo->rti_addrs = 0;
again:
	switch (type) {

	case RTM_DELADDR:
	case RTM_NEWADDR:
		len = sizeof(struct ifa_msghdr);
		break;

	case RTM_IFINFO:
		len = sizeof(struct if_msghdr);
		break;

	case RTM_NEWMADDR:
		len = sizeof(struct ifma_msghdr);
		break;

	default:
		len = sizeof(struct rt_msghdr);
	}
	cp0 = cp;
	if (cp0)
		cp += len;
	for (i = 0; i < RTAX_MAX; i++) {
		struct sockaddr *sa;

		if ((sa = rtinfo->rti_info[i]) == NULL)
			continue;
		rtinfo->rti_addrs |= (1 << i);
		dlen = SA_SIZE(sa);
		if (cp) {
			bcopy((caddr_t)sa, cp, (unsigned)dlen);
			cp += dlen;
		}
		len += dlen;
	}
	len = ALIGN(len);
	if (cp == NULL && w != NULL && !second_time) {
		struct walkarg *rw = w;

		if (rw->w_req) {
			if (rw->w_tmemsize < len) {
				if (rw->w_tmem)
					free(rw->w_tmem, M_RTABLE);
				rw->w_tmem = (caddr_t)
					malloc(len, M_RTABLE, M_NOWAIT);
				if (rw->w_tmem)
					rw->w_tmemsize = len;
			}
			if (rw->w_tmem) {
				cp = rw->w_tmem;
				second_time = 1;
				goto again;
			}
		}
	}
	if (cp) {
		struct rt_msghdr *rtm = (struct rt_msghdr *)cp0;

		rtm->rtm_version = RTM_VERSION;
		rtm->rtm_type = type;
		rtm->rtm_msglen = len;
	}
	return (len);
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that a redirect has occured, a routing lookup
 * has failed, or that a protocol has detected timeouts to a particular
 * destination.
 */
void
rt_missmsg(int type, struct rt_addrinfo *rtinfo, int flags, int error)
{
	struct rt_msghdr *rtm;
	struct mbuf *m;
	struct sockaddr *sa = rtinfo->rti_info[RTAX_DST];

	if (route_cb.any_count == 0)
		return;
	m = rt_msg1(type, rtinfo);
	if (m == NULL)
		return;
	rtm = mtod(m, struct rt_msghdr *);
	rtm->rtm_flags = RTF_DONE | flags;
	rtm->rtm_errno = error;
	rtm->rtm_addrs = rtinfo->rti_addrs;
	rt_dispatch(m, sa);
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that the status of a network interface has changed.
 */
void
rt_ifmsg(struct ifnet *ifp)
{
	struct if_msghdr *ifm;
	struct mbuf *m;
	struct rt_addrinfo info;

	if (route_cb.any_count == 0)
		return;
	bzero((caddr_t)&info, sizeof(info));
	m = rt_msg1(RTM_IFINFO, &info);
	if (m == NULL)
		return;
	ifm = mtod(m, struct if_msghdr *);
	ifm->ifm_index = ifp->if_index;
	ifm->ifm_flags = ifp->if_flags | ifp->if_drv_flags;
	ifm->ifm_data = ifp->if_data;
	ifm->ifm_addrs = 0;
	rt_dispatch(m, NULL);
}

/*
 * This is called to generate messages from the routing socket
 * indicating a network interface has had addresses associated with it.
 * if we ever reverse the logic and replace messages TO the routing
 * socket indicate a request to configure interfaces, then it will
 * be unnecessary as the routing socket will automatically generate
 * copies of it.
 */
void
rt_newaddrmsg(int cmd, struct ifaddr *ifa, int error, struct rtentry *rt)
{
	struct rt_addrinfo info;
	struct sockaddr *sa = NULL;
	int pass;
	struct mbuf *m = NULL;
	struct ifnet *ifp = ifa->ifa_ifp;

	KASSERT(cmd == RTM_ADD || cmd == RTM_DELETE,
		("unexpected cmd %u", cmd));
#if defined(INET) || defined(INET6)
#ifdef SCTP
	/*
	 * notify the SCTP stack
	 * this will only get called when an address is added/deleted
	 * XXX pass the ifaddr struct instead if ifa->ifa_addr...
	 */
	sctp_addr_change(ifa, cmd);
#endif /* SCTP */
#endif
	if (route_cb.any_count == 0)
		return;
	for (pass = 1; pass < 3; pass++) {
		bzero((caddr_t)&info, sizeof(info));
		if ((cmd == RTM_ADD && pass == 1) ||
		    (cmd == RTM_DELETE && pass == 2)) {
			struct ifa_msghdr *ifam;
			int ncmd = cmd == RTM_ADD ? RTM_NEWADDR : RTM_DELADDR;

			info.rti_info[RTAX_IFA] = sa = ifa->ifa_addr;
			info.rti_info[RTAX_IFP] = ifp->if_addr->ifa_addr;
			info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;
			info.rti_info[RTAX_BRD] = ifa->ifa_dstaddr;
			if ((m = rt_msg1(ncmd, &info)) == NULL)
				continue;
			ifam = mtod(m, struct ifa_msghdr *);
			ifam->ifam_index = ifp->if_index;
			ifam->ifam_metric = ifa->ifa_metric;
			ifam->ifam_flags = ifa->ifa_flags;
			ifam->ifam_addrs = info.rti_addrs;
		}
		if ((cmd == RTM_ADD && pass == 2) ||
		    (cmd == RTM_DELETE && pass == 1)) {
			struct rt_msghdr *rtm;

			if (rt == NULL)
				continue;
			info.rti_info[RTAX_NETMASK] = rt_mask(rt);
			info.rti_info[RTAX_DST] = sa = rt_key(rt);
			info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
			if ((m = rt_msg1(cmd, &info)) == NULL)
				continue;
			rtm = mtod(m, struct rt_msghdr *);
			rtm->rtm_index = ifp->if_index;
			rtm->rtm_flags |= rt->rt_flags;
			rtm->rtm_errno = error;
			rtm->rtm_addrs = info.rti_addrs;
		}
		rt_dispatch(m, sa);
	}
}

/*
 * This is the analogue to the rt_newaddrmsg which performs the same
 * function but for multicast group memberhips.  This is easier since
 * there is no route state to worry about.
 */
void
rt_newmaddrmsg(int cmd, struct ifmultiaddr *ifma)
{
	struct rt_addrinfo info;
	struct mbuf *m = NULL;
	struct ifnet *ifp = ifma->ifma_ifp;
	struct ifma_msghdr *ifmam;

	if (route_cb.any_count == 0)
		return;

	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_IFA] = ifma->ifma_addr;
	info.rti_info[RTAX_IFP] = ifp ? ifp->if_addr->ifa_addr : NULL;
	/*
	 * If a link-layer address is present, present it as a ``gateway''
	 * (similarly to how ARP entries, e.g., are presented).
	 */
	info.rti_info[RTAX_GATEWAY] = ifma->ifma_lladdr;
	m = rt_msg1(cmd, &info);
	if (m == NULL)
		return;
	ifmam = mtod(m, struct ifma_msghdr *);
	KASSERT(ifp != NULL, ("%s: link-layer multicast address w/o ifp\n",
	    __func__));
	ifmam->ifmam_index = ifp->if_index;
	ifmam->ifmam_addrs = info.rti_addrs;
	rt_dispatch(m, ifma->ifma_addr);
}

static struct mbuf *
rt_makeifannouncemsg(struct ifnet *ifp, int type, int what,
	struct rt_addrinfo *info)
{
	struct if_announcemsghdr *ifan;
	struct mbuf *m;

	if (route_cb.any_count == 0)
		return NULL;
	bzero((caddr_t)info, sizeof(*info));
	m = rt_msg1(type, info);
	if (m != NULL) {
		ifan = mtod(m, struct if_announcemsghdr *);
		ifan->ifan_index = ifp->if_index;
		strlcpy(ifan->ifan_name, ifp->if_xname,
			sizeof(ifan->ifan_name));
		ifan->ifan_what = what;
	}
	return m;
}

/*
 * This is called to generate routing socket messages indicating
 * IEEE80211 wireless events.
 * XXX we piggyback on the RTM_IFANNOUNCE msg format in a clumsy way.
 */
void
rt_ieee80211msg(struct ifnet *ifp, int what, void *data, size_t data_len)
{
	struct mbuf *m;
	struct rt_addrinfo info;

	m = rt_makeifannouncemsg(ifp, RTM_IEEE80211, what, &info);
	if (m != NULL) {
		/*
		 * Append the ieee80211 data.  Try to stick it in the
		 * mbuf containing the ifannounce msg; otherwise allocate
		 * a new mbuf and append.
		 *
		 * NB: we assume m is a single mbuf.
		 */
		if (data_len > M_TRAILINGSPACE(m)) {
			struct mbuf *n = m_get(M_NOWAIT, MT_DATA);
			if (n == NULL) {
				m_freem(m);
				return;
			}
			bcopy(data, mtod(n, void *), data_len);
			n->m_len = data_len;
			m->m_next = n;
		} else if (data_len > 0) {
			bcopy(data, mtod(m, u_int8_t *) + m->m_len, data_len);
			m->m_len += data_len;
		}
		if (m->m_flags & M_PKTHDR)
			m->m_pkthdr.len += data_len;
		mtod(m, struct if_announcemsghdr *)->ifan_msglen += data_len;
		rt_dispatch(m, NULL);
	}
}

/*
 * This is called to generate routing socket messages indicating
 * network interface arrival and departure.
 */
void
rt_ifannouncemsg(struct ifnet *ifp, int what)
{
	struct mbuf *m;
	struct rt_addrinfo info;

	m = rt_makeifannouncemsg(ifp, RTM_IFANNOUNCE, what, &info);
	if (m != NULL)
		rt_dispatch(m, NULL);
}

static void
rt_dispatch(struct mbuf *m, const struct sockaddr *sa)
{
	struct m_tag *tag;

	/*
	 * Preserve the family from the sockaddr, if any, in an m_tag for
	 * use when injecting the mbuf into the routing socket buffer from
	 * the netisr.
	 */
	if (sa != NULL) {
		tag = m_tag_get(PACKET_TAG_RTSOCKFAM, sizeof(unsigned short),
		    M_NOWAIT);
		if (tag == NULL) {
			m_freem(m);
			return;
		}
		*(unsigned short *)(tag + 1) = sa->sa_family;
		m_tag_prepend(m, tag);
	}
#ifdef VIMAGE
	if (V_loif)
		m->m_pkthdr.rcvif = V_loif;
	else {
		m_freem(m);
		return;
	}
#endif
	netisr_queue(NETISR_ROUTE, m);	/* mbuf is free'd on failure. */
}

/*
 * This is used in dumping the kernel table via sysctl().
 */
static int
sysctl_dumpentry(struct radix_node *rn, void *vw)
{
	struct walkarg *w = vw;
	struct rtentry *rt = (struct rtentry *)rn;
	int error = 0, size;
	struct rt_addrinfo info;

	if (w->w_op == NET_RT_FLAGS && !(rt->rt_flags & w->w_arg))
		return 0;
	if ((rt->rt_flags & RTF_HOST) == 0
	    ? jailed_without_vnet(w->w_req->td->td_ucred)
	    : prison_if(w->w_req->td->td_ucred, rt_key(rt)) != 0)
		return (0);
	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_DST] = rt_key(rt);
	info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
	info.rti_info[RTAX_NETMASK] = rt_mask(rt);
	info.rti_info[RTAX_GENMASK] = 0;
	if (rt->rt_ifp) {
		info.rti_info[RTAX_IFP] = rt->rt_ifp->if_addr->ifa_addr;
		info.rti_info[RTAX_IFA] = rt->rt_ifa->ifa_addr;
		if (rt->rt_ifp->if_flags & IFF_POINTOPOINT)
			info.rti_info[RTAX_BRD] = rt->rt_ifa->ifa_dstaddr;
	}
	size = rt_msg2(RTM_GET, &info, NULL, w);
	if (w->w_req && w->w_tmem) {
		struct rt_msghdr *rtm = (struct rt_msghdr *)w->w_tmem;

		rtm->rtm_flags = rt->rt_flags;
		/*
		 * let's be honest about this being a retarded hack
		 */
		rtm->rtm_fmask = rt->rt_rmx.rmx_pksent;
		rt_getmetrics(&rt->rt_rmx, &rtm->rtm_rmx);
		rtm->rtm_index = rt->rt_ifp->if_index;
		rtm->rtm_errno = rtm->rtm_pid = rtm->rtm_seq = 0;
		rtm->rtm_addrs = info.rti_addrs;
		error = SYSCTL_OUT(w->w_req, (caddr_t)rtm, size);
		return (error);
	}
	return (error);
}

static int
sysctl_iflist(int af, struct walkarg *w)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct rt_addrinfo info;
	int len, error = 0;

	bzero((caddr_t)&info, sizeof(info));
	IFNET_RLOCK();
	TAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		if (w->w_arg && w->w_arg != ifp->if_index)
			continue;
		ifa = ifp->if_addr;
		info.rti_info[RTAX_IFP] = ifa->ifa_addr;
		len = rt_msg2(RTM_IFINFO, &info, NULL, w);
		info.rti_info[RTAX_IFP] = NULL;
		if (w->w_req && w->w_tmem) {
			struct if_msghdr *ifm;

			ifm = (struct if_msghdr *)w->w_tmem;
			ifm->ifm_index = ifp->if_index;
			ifm->ifm_flags = ifp->if_flags | ifp->if_drv_flags;
			ifm->ifm_data = ifp->if_data;
			ifm->ifm_addrs = info.rti_addrs;
			error = SYSCTL_OUT(w->w_req,(caddr_t)ifm, len);
			if (error)
				goto done;
		}
		while ((ifa = TAILQ_NEXT(ifa, ifa_link)) != NULL) {
			if (af && af != ifa->ifa_addr->sa_family)
				continue;
			if (prison_if(w->w_req->td->td_ucred,
			    ifa->ifa_addr) != 0)
				continue;
			info.rti_info[RTAX_IFA] = ifa->ifa_addr;
			info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;
			info.rti_info[RTAX_BRD] = ifa->ifa_dstaddr;
			len = rt_msg2(RTM_NEWADDR, &info, NULL, w);
			if (w->w_req && w->w_tmem) {
				struct ifa_msghdr *ifam;

				ifam = (struct ifa_msghdr *)w->w_tmem;
				ifam->ifam_index = ifa->ifa_ifp->if_index;
				ifam->ifam_flags = ifa->ifa_flags;
				ifam->ifam_metric = ifa->ifa_metric;
				ifam->ifam_addrs = info.rti_addrs;
				error = SYSCTL_OUT(w->w_req, w->w_tmem, len);
				if (error)
					goto done;
			}
		}
		info.rti_info[RTAX_IFA] = info.rti_info[RTAX_NETMASK] =
			info.rti_info[RTAX_BRD] = NULL;
	}
done:
	IFNET_RUNLOCK();
	return (error);
}

static int
sysctl_ifmalist(int af, struct walkarg *w)
{
	struct ifnet *ifp;
	struct ifmultiaddr *ifma;
	struct	rt_addrinfo info;
	int	len, error = 0;
	struct ifaddr *ifa;

	bzero((caddr_t)&info, sizeof(info));
	IFNET_RLOCK();
	TAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		if (w->w_arg && w->w_arg != ifp->if_index)
			continue;
		ifa = ifp->if_addr;
		info.rti_info[RTAX_IFP] = ifa ? ifa->ifa_addr : NULL;
		IF_ADDR_LOCK(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (af && af != ifma->ifma_addr->sa_family)
				continue;
			if (prison_if(w->w_req->td->td_ucred,
			    ifma->ifma_addr) != 0)
				continue;
			info.rti_info[RTAX_IFA] = ifma->ifma_addr;
			info.rti_info[RTAX_GATEWAY] =
			    (ifma->ifma_addr->sa_family != AF_LINK) ?
			    ifma->ifma_lladdr : NULL;
			len = rt_msg2(RTM_NEWMADDR, &info, NULL, w);
			if (w->w_req && w->w_tmem) {
				struct ifma_msghdr *ifmam;

				ifmam = (struct ifma_msghdr *)w->w_tmem;
				ifmam->ifmam_index = ifma->ifma_ifp->if_index;
				ifmam->ifmam_flags = 0;
				ifmam->ifmam_addrs = info.rti_addrs;
				error = SYSCTL_OUT(w->w_req, w->w_tmem, len);
				if (error) {
					IF_ADDR_UNLOCK(ifp);
					goto done;
				}
			}
		}
		IF_ADDR_UNLOCK(ifp);
	}
done:
	IFNET_RUNLOCK();
	return (error);
}

static int
sysctl_rtsock(SYSCTL_HANDLER_ARGS)
{
	int	*name = (int *)arg1;
	u_int	namelen = arg2;
	struct radix_node_head *rnh = NULL; /* silence compiler. */
	int	i, lim, error = EINVAL;
	u_char	af;
	struct	walkarg w;

	name ++;
	namelen--;
	if (req->newptr)
		return (EPERM);
	if (namelen != 3)
		return ((namelen < 3) ? EISDIR : ENOTDIR);
	af = name[0];
	if (af > AF_MAX)
		return (EINVAL);
	bzero(&w, sizeof(w));
	w.w_op = name[1];
	w.w_arg = name[2];
	w.w_req = req;

	error = sysctl_wire_old_buffer(req, 0);
	if (error)
		return (error);
	switch (w.w_op) {

	case NET_RT_DUMP:
	case NET_RT_FLAGS:
		if (af == 0) {			/* dump all tables */
			i = 1;
			lim = AF_MAX;
		} else				/* dump only one table */
			i = lim = af;

		/*
		 * take care of llinfo entries, the caller must
		 * specify an AF
		 */
		if (w.w_op == NET_RT_FLAGS &&
		    (w.w_arg == 0 || w.w_arg & RTF_LLINFO)) {
			if (af != 0)
				error = lltable_sysctl_dumparp(af, w.w_req);
			else
				error = EINVAL;
			break;
		}
		/*
		 * take care of routing entries
		 */
		for (error = 0; error == 0 && i <= lim; i++) {
			rnh = rt_tables_get_rnh(req->td->td_proc->p_fibnum, i);
			if (rnh != NULL) {
				RADIX_NODE_HEAD_LOCK(rnh); 
			    	error = rnh->rnh_walktree(rnh,
				    sysctl_dumpentry, &w);
				RADIX_NODE_HEAD_UNLOCK(rnh);
			} else if (af != 0)
				error = EAFNOSUPPORT;
		}
		break;

	case NET_RT_IFLIST:
		error = sysctl_iflist(af, &w);
		break;

	case NET_RT_IFMALIST:
		error = sysctl_ifmalist(af, &w);
		break;
	}
	if (w.w_tmem)
		free(w.w_tmem, M_RTABLE);
	return (error);
}

SYSCTL_NODE(_net, PF_ROUTE, routetable, CTLFLAG_RD, sysctl_rtsock, "");

/*
 * Definitions of protocols supported in the ROUTE domain.
 */

static struct domain routedomain;		/* or at least forward */

static struct protosw routesw[] = {
{
	.pr_type =		SOCK_RAW,
	.pr_domain =		&routedomain,
	.pr_flags =		PR_ATOMIC|PR_ADDR,
	.pr_output =		route_output,
	.pr_ctlinput =		raw_ctlinput,
	.pr_init =		raw_init,
	.pr_usrreqs =		&route_usrreqs
}
};

static struct domain routedomain = {
	.dom_family =		PF_ROUTE,
	.dom_name =		 "route",
	.dom_protosw =		routesw,
	.dom_protoswNPROTOSW =	&routesw[sizeof(routesw)/sizeof(routesw[0])]
};

VNET_DOMAIN_SET(route);
