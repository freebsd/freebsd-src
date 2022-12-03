/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
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
 *	@(#)rtsock.c	8.7 (Berkeley) 10/12/95
 * $FreeBSD$
 */
#include "opt_ddb.h"
#include "opt_route.h"
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/kernel.h>
#include <sys/eventhandler.h>
#include <sys/domain.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/protosw.h>
#include <sys/rmlock.h>
#include <sys/rwlock.h>
#include <sys/signalvar.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/if_llatbl.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/raw_cb.h>
#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip_carp.h>
#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#endif
#include <net/route/nhop.h>

#define	DEBUG_MOD_NAME	rtsock
#define	DEBUG_MAX_LEVEL	LOG_DEBUG
#include <net/route/route_debug.h>
_DECLARE_DEBUG(LOG_INFO);

#ifdef COMPAT_FREEBSD32
#include <sys/mount.h>
#include <compat/freebsd32/freebsd32.h>

struct if_msghdr32 {
	uint16_t ifm_msglen;
	uint8_t	ifm_version;
	uint8_t	ifm_type;
	int32_t	ifm_addrs;
	int32_t	ifm_flags;
	uint16_t ifm_index;
	uint16_t _ifm_spare1;
	struct	if_data ifm_data;
};

struct if_msghdrl32 {
	uint16_t ifm_msglen;
	uint8_t	ifm_version;
	uint8_t	ifm_type;
	int32_t	ifm_addrs;
	int32_t	ifm_flags;
	uint16_t ifm_index;
	uint16_t _ifm_spare1;
	uint16_t ifm_len;
	uint16_t ifm_data_off;
	uint32_t _ifm_spare2;
	struct	if_data ifm_data;
};

struct ifa_msghdrl32 {
	uint16_t ifam_msglen;
	uint8_t	ifam_version;
	uint8_t	ifam_type;
	int32_t	ifam_addrs;
	int32_t	ifam_flags;
	uint16_t ifam_index;
	uint16_t _ifam_spare1;
	uint16_t ifam_len;
	uint16_t ifam_data_off;
	int32_t	ifam_metric;
	struct	if_data ifam_data;
};

#define SA_SIZE32(sa)						\
    (  (((struct sockaddr *)(sa))->sa_len == 0) ?		\
	sizeof(int)		:				\
	1 + ( (((struct sockaddr *)(sa))->sa_len - 1) | (sizeof(int) - 1) ) )

#endif /* COMPAT_FREEBSD32 */

struct linear_buffer {
	char		*base;	/* Base allocated memory pointer */
	uint32_t	offset;	/* Currently used offset */
	uint32_t	size;	/* Total buffer size */
};
#define	SCRATCH_BUFFER_SIZE	1024

#define	RTS_PID_LOG(_l, _fmt, ...)	RT_LOG_##_l(_l, "PID %d: " _fmt, curproc ? curproc->p_pid : 0, ## __VA_ARGS__)

MALLOC_DEFINE(M_RTABLE, "routetbl", "routing tables");

/* NB: these are not modified */
static struct	sockaddr route_src = { 2, PF_ROUTE, };
static struct	sockaddr sa_zero   = { sizeof(sa_zero), AF_INET, };

/* These are external hooks for CARP. */
int	(*carp_get_vhid_p)(struct ifaddr *);

/*
 * Used by rtsock/raw_input callback code to decide whether to filter the update
 * notification to a socket bound to a particular FIB.
 */
#define	RTS_FILTER_FIB	M_PROTO8

typedef struct {
	int	ip_count;	/* attached w/ AF_INET */
	int	ip6_count;	/* attached w/ AF_INET6 */
	int	any_count;	/* total attached */
} route_cb_t;
VNET_DEFINE_STATIC(route_cb_t, route_cb);
#define	V_route_cb VNET(route_cb)

struct mtx rtsock_mtx;
MTX_SYSINIT(rtsock, &rtsock_mtx, "rtsock route_cb lock", MTX_DEF);

#define	RTSOCK_LOCK()	mtx_lock(&rtsock_mtx)
#define	RTSOCK_UNLOCK()	mtx_unlock(&rtsock_mtx)
#define	RTSOCK_LOCK_ASSERT()	mtx_assert(&rtsock_mtx, MA_OWNED)

SYSCTL_NODE(_net, OID_AUTO, route, CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "");

struct walkarg {
	int	family;
	int	w_tmemsize;
	int	w_op, w_arg;
	caddr_t	w_tmem;
	struct sysctl_req *w_req;
	struct sockaddr *dst;
	struct sockaddr *mask;
};

static void	rts_input(struct mbuf *m);
static struct mbuf *rtsock_msg_mbuf(int type, struct rt_addrinfo *rtinfo);
static int	rtsock_msg_buffer(int type, struct rt_addrinfo *rtinfo,
			struct walkarg *w, int *plen);
static int	rt_xaddrs(caddr_t cp, caddr_t cplim,
			struct rt_addrinfo *rtinfo);
static int	cleanup_xaddrs(struct rt_addrinfo *info, struct linear_buffer *lb);
static int	sysctl_dumpentry(struct rtentry *rt, void *vw);
static int	sysctl_dumpnhop(struct rtentry *rt, struct nhop_object *nh,
			uint32_t weight, struct walkarg *w);
static int	sysctl_iflist(int af, struct walkarg *w);
static int	sysctl_ifmalist(int af, struct walkarg *w);
static int	route_output(struct mbuf *m, struct socket *so, ...);
static void	rt_getmetrics(const struct rtentry *rt,
			const struct nhop_object *nh, struct rt_metrics *out);
static void	rt_dispatch(struct mbuf *, sa_family_t);
static void	rt_ifannouncemsg(struct ifnet *ifp, int what);
static int	handle_rtm_get(struct rt_addrinfo *info, u_int fibnum,
			struct rt_msghdr *rtm, struct rib_cmd_info *rc);
static int	update_rtm_from_rc(struct rt_addrinfo *info,
			struct rt_msghdr **prtm, int alloc_len,
			struct rib_cmd_info *rc, struct nhop_object *nh);
static void	send_rtm_reply(struct socket *so, struct rt_msghdr *rtm,
			struct mbuf *m, sa_family_t saf, u_int fibnum,
			int rtm_errno);
static bool	can_export_rte(struct ucred *td_ucred, bool rt_is_host,
			const struct sockaddr *rt_dst);
static void	rtsock_notify_event(uint32_t fibnum, const struct rib_cmd_info *rc);
static void	rtsock_ifmsg(struct ifnet *ifp, int if_flags_mask);

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
SYSCTL_PROC(_net_route, OID_AUTO, netisr_maxqlen,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
    0, 0, sysctl_route_netisr_maxqlen, "I",
    "maximum routing socket dispatch queue length");

static void
vnet_rts_init(void)
{
	int tmp;

	if (IS_DEFAULT_VNET(curvnet)) {
		if (TUNABLE_INT_FETCH("net.route.netisr_maxqlen", &tmp))
			rtsock_nh.nh_qlimit = tmp;
		netisr_register(&rtsock_nh);
	}
#ifdef VIMAGE
	 else
		netisr_register_vnet(&rtsock_nh);
#endif
}
VNET_SYSINIT(vnet_rtsock, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD,
    vnet_rts_init, 0);

#ifdef VIMAGE
static void
vnet_rts_uninit(void)
{

	netisr_unregister_vnet(&rtsock_nh);
}
VNET_SYSUNINIT(vnet_rts_uninit, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD,
    vnet_rts_uninit, 0);
#endif

static void
report_route_event(const struct rib_cmd_info *rc, void *_cbdata)
{
	uint32_t fibnum = (uint32_t)(uintptr_t)_cbdata;
	struct nhop_object *nh;

	nh = rc->rc_cmd == RTM_DELETE ? rc->rc_nh_old : rc->rc_nh_new;
	rt_routemsg(rc->rc_cmd, rc->rc_rt, nh, fibnum);
}

static void
rts_handle_route_event(uint32_t fibnum, const struct rib_cmd_info *rc)
{
#ifdef ROUTE_MPATH
	if ((rc->rc_nh_new && NH_IS_NHGRP(rc->rc_nh_new)) ||
	    (rc->rc_nh_old && NH_IS_NHGRP(rc->rc_nh_old))) {
		rib_decompose_notification(rc, report_route_event,
		    (void *)(uintptr_t)fibnum);
	} else
#endif
		report_route_event(rc, (void *)(uintptr_t)fibnum);
}
static struct rtbridge rtsbridge = {
	.route_f = rts_handle_route_event,
	.ifmsg_f = rtsock_ifmsg,
};
static struct rtbridge *rtsbridge_orig_p;

static void
rtsock_notify_event(uint32_t fibnum, const struct rib_cmd_info *rc)
{
	netlink_callback_p->route_f(fibnum, rc);
}

static void
rtsock_init(void)
{
	rtsbridge_orig_p = rtsock_callback_p;
	rtsock_callback_p = &rtsbridge;
}
SYSINIT(rtsock_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, rtsock_init, NULL);

static void
rts_handle_ifnet_arrival(void *arg __unused, struct ifnet *ifp)
{
	rt_ifannouncemsg(ifp, IFAN_ARRIVAL);
}
EVENTHANDLER_DEFINE(ifnet_arrival_event, rts_handle_ifnet_arrival, NULL, 0);

static void
rts_handle_ifnet_departure(void *arg __unused, struct ifnet *ifp)
{
	rt_ifannouncemsg(ifp, IFAN_DEPARTURE);
}
EVENTHANDLER_DEFINE(ifnet_departure_event, rts_handle_ifnet_departure, NULL, 0);

static int
raw_input_rts_cb(struct mbuf *m, struct sockproto *proto, struct sockaddr *src,
    struct rawcb *rp)
{
	int fibnum;

	KASSERT(m != NULL, ("%s: m is NULL", __func__));
	KASSERT(proto != NULL, ("%s: proto is NULL", __func__));
	KASSERT(rp != NULL, ("%s: rp is NULL", __func__));

	/* No filtering requested. */
	if ((m->m_flags & RTS_FILTER_FIB) == 0)
		return (0);

	/* Check if it is a rts and the fib matches the one of the socket. */
	fibnum = M_GETFIB(m);
	if (proto->sp_family != PF_ROUTE ||
	    rp->rcb_socket == NULL ||
	    rp->rcb_socket->so_fibnum == fibnum)
		return (0);

	/* Filtering requested and no match, the socket shall be skipped. */
	return (1);
}

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

	raw_input_ext(m, &route_proto, &route_src, raw_input_rts_cb);
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
	int error;

	KASSERT(so->so_pcb == NULL, ("rts_attach: so_pcb != NULL"));

	/* XXX */
	rp = malloc(sizeof *rp, M_PCB, M_WAITOK | M_ZERO);

	so->so_pcb = (caddr_t)rp;
	so->so_fibnum = td->td_proc->p_fibnum;
	error = raw_attach(so, proto);
	rp = sotorawcb(so);
	if (error) {
		so->so_pcb = NULL;
		free(rp, M_PCB);
		return error;
	}
	RTSOCK_LOCK();
	switch(rp->rcb_proto.sp_protocol) {
	case AF_INET:
		V_route_cb.ip_count++;
		break;
	case AF_INET6:
		V_route_cb.ip6_count++;
		break;
	}
	V_route_cb.any_count++;
	RTSOCK_UNLOCK();
	soisconnected(so);
	so->so_options |= SO_USELOOPBACK;
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
		V_route_cb.ip_count--;
		break;
	case AF_INET6:
		V_route_cb.ip6_count--;
		break;
	}
	V_route_cb.any_count--;
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
    struct nhop_object *nh, union sockaddr_union *saun, struct ucred *cred)
{
#if defined(INET) || defined(INET6)
	struct epoch_tracker et;
#endif

	/* First, see if the returned address is part of the jail. */
	if (prison_if(cred, nh->nh_ifa->ifa_addr) == 0) {
		info->rti_info[RTAX_IFA] = nh->nh_ifa->ifa_addr;
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
		NET_EPOCH_ENTER(et);
		CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
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
		NET_EPOCH_EXIT(et);
		if (!found) {
			/*
			 * As a last resort return the 'default' jail address.
			 */
			ia = ((struct sockaddr_in *)nh->nh_ifa->ifa_addr)->
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
		NET_EPOCH_ENTER(et);
		CK_STAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
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
		NET_EPOCH_EXIT(et);
		if (!found) {
			/*
			 * As a last resort return the 'default' jail address.
			 */
			ia6 = ((struct sockaddr_in6 *)nh->nh_ifa->ifa_addr)->
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

static int
fill_blackholeinfo(struct rt_addrinfo *info, union sockaddr_union *saun)
{
	struct ifaddr *ifa;
	sa_family_t saf;

	if (V_loif == NULL) {
		RTS_PID_LOG(LOG_INFO, "Unable to add blackhole/reject nhop without loopback");
		return (ENOTSUP);
	}
	info->rti_ifp = V_loif;

	saf = info->rti_info[RTAX_DST]->sa_family;

	CK_STAILQ_FOREACH(ifa, &info->rti_ifp->if_addrhead, ifa_link) {
		if (ifa->ifa_addr->sa_family == saf) {
			info->rti_ifa = ifa;
			break;
		}
	}
	if (info->rti_ifa == NULL) {
		RTS_PID_LOG(LOG_INFO, "Unable to find ifa for blackhole/reject nhop");
		return (ENOTSUP);
	}

	bzero(saun, sizeof(union sockaddr_union));
	switch (saf) {
#ifdef INET
	case AF_INET:
		saun->sin.sin_family = AF_INET;
		saun->sin.sin_len = sizeof(struct sockaddr_in);
		saun->sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		saun->sin6.sin6_family = AF_INET6;
		saun->sin6.sin6_len = sizeof(struct sockaddr_in6);
		saun->sin6.sin6_addr = in6addr_loopback;
		break;
#endif
	default:
		RTS_PID_LOG(LOG_INFO, "unsupported family: %d", saf);
		return (ENOTSUP);
	}
	info->rti_info[RTAX_GATEWAY] = &saun->sa;
	info->rti_flags |= RTF_GATEWAY;

	return (0);
}

/*
 * Fills in @info based on userland-provided @rtm message.
 *
 * Returns 0 on success.
 */
static int
fill_addrinfo(struct rt_msghdr *rtm, int len, struct linear_buffer *lb, u_int fibnum,
    struct rt_addrinfo *info)
{
	int error;
	sa_family_t saf;

	rtm->rtm_pid = curproc->p_pid;
	info->rti_addrs = rtm->rtm_addrs;

	info->rti_mflags = rtm->rtm_inits;
	info->rti_rmx = &rtm->rtm_rmx;

	/*
	 * rt_xaddrs() performs s6_addr[2] := sin6_scope_id for AF_INET6
	 * link-local address because rtrequest requires addresses with
	 * embedded scope id.
	 */
	if (rt_xaddrs((caddr_t)(rtm + 1), len + (caddr_t)rtm, info))
		return (EINVAL);

	info->rti_flags = rtm->rtm_flags;
	error = cleanup_xaddrs(info, lb);
	if (error != 0)
		return (error);
	saf = info->rti_info[RTAX_DST]->sa_family;
	/*
	 * Verify that the caller has the appropriate privilege; RTM_GET
	 * is the only operation the non-superuser is allowed.
	 */
	if (rtm->rtm_type != RTM_GET) {
		error = priv_check(curthread, PRIV_NET_ROUTE);
		if (error != 0)
			return (error);
	}

	/*
	 * The given gateway address may be an interface address.
	 * For example, issuing a "route change" command on a route
	 * entry that was created from a tunnel, and the gateway
	 * address given is the local end point. In this case the 
	 * RTF_GATEWAY flag must be cleared or the destination will
	 * not be reachable even though there is no error message.
	 */
	if (info->rti_info[RTAX_GATEWAY] != NULL &&
	    info->rti_info[RTAX_GATEWAY]->sa_family != AF_LINK) {
		struct rt_addrinfo ginfo;
		struct sockaddr *gdst;
		struct sockaddr_storage ss;

		bzero(&ginfo, sizeof(ginfo));
		bzero(&ss, sizeof(ss));
		ss.ss_len = sizeof(ss);

		ginfo.rti_info[RTAX_GATEWAY] = (struct sockaddr *)&ss;
		gdst = info->rti_info[RTAX_GATEWAY];

		/* 
		 * A host route through the loopback interface is 
		 * installed for each interface adddress. In pre 8.0
		 * releases the interface address of a PPP link type
		 * is not reachable locally. This behavior is fixed as 
		 * part of the new L2/L3 redesign and rewrite work. The
		 * signature of this interface address route is the
		 * AF_LINK sa_family type of the gateway, and the
		 * rt_ifp has the IFF_LOOPBACK flag set.
		 */
		if (rib_lookup_info(fibnum, gdst, NHR_REF, 0, &ginfo) == 0) {
			if (ss.ss_family == AF_LINK &&
			    ginfo.rti_ifp->if_flags & IFF_LOOPBACK) {
				info->rti_flags &= ~RTF_GATEWAY;
				info->rti_flags |= RTF_GWFLAG_COMPAT;
			}
			rib_free_info(&ginfo);
		}
	}

	return (0);
}

static struct nhop_object *
select_nhop(struct nhop_object *nh, const struct sockaddr *gw)
{
	if (!NH_IS_NHGRP(nh))
		return (nh);
#ifdef ROUTE_MPATH
	const struct weightened_nhop *wn;
	uint32_t num_nhops;
	wn = nhgrp_get_nhops((struct nhgrp_object *)nh, &num_nhops);
	if (gw == NULL)
		return (wn[0].nh);
	for (int i = 0; i < num_nhops; i++) {
		if (match_nhop_gw(wn[i].nh, gw))
			return (wn[i].nh);
	}
#endif
	return (NULL);
}

/*
 * Handles RTM_GET message from routing socket, returning matching rt.
 *
 * Returns:
 * 0 on success, with locked and referenced matching rt in @rt_nrt
 * errno of failure
 */
static int
handle_rtm_get(struct rt_addrinfo *info, u_int fibnum,
    struct rt_msghdr *rtm, struct rib_cmd_info *rc)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rnh;
	struct nhop_object *nh;
	sa_family_t saf;

	saf = info->rti_info[RTAX_DST]->sa_family;

	rnh = rt_tables_get_rnh(fibnum, saf);
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	RIB_RLOCK(rnh);

	/*
	 * By (implicit) convention host route (one without netmask)
	 * means longest-prefix-match request and the route with netmask
	 * means exact-match lookup.
	 * As cleanup_xaddrs() cleans up info flags&addrs for the /32,/128
	 * prefixes, use original data to check for the netmask presence.
	 */
	if ((rtm->rtm_addrs & RTA_NETMASK) == 0) {
		/*
		 * Provide longest prefix match for
		 * address lookup (no mask).
		 * 'route -n get addr'
		 */
		rc->rc_rt = (struct rtentry *) rnh->rnh_matchaddr(
		    info->rti_info[RTAX_DST], &rnh->head);
	} else
		rc->rc_rt = (struct rtentry *) rnh->rnh_lookup(
		    info->rti_info[RTAX_DST],
		    info->rti_info[RTAX_NETMASK], &rnh->head);

	if (rc->rc_rt == NULL) {
		RIB_RUNLOCK(rnh);
		return (ESRCH);
	}

	nh = select_nhop(rt_get_raw_nhop(rc->rc_rt), info->rti_info[RTAX_GATEWAY]);
	if (nh == NULL) {
		RIB_RUNLOCK(rnh);
		return (ESRCH);
	}
	/*
	 * If performing proxied L2 entry insertion, and
	 * the actual PPP host entry is found, perform
	 * another search to retrieve the prefix route of
	 * the local end point of the PPP link.
	 * TODO: move this logic to userland.
	 */
	if (rtm->rtm_flags & RTF_ANNOUNCE) {
		struct sockaddr_storage laddr;

		if (nh->nh_ifp != NULL &&
		    nh->nh_ifp->if_type == IFT_PROPVIRTUAL) {
			struct ifaddr *ifa;

			ifa = ifa_ifwithnet(info->rti_info[RTAX_DST], 1,
					RT_ALL_FIBS);
			if (ifa != NULL)
				rt_maskedcopy(ifa->ifa_addr,
					      (struct sockaddr *)&laddr,
					      ifa->ifa_netmask);
		} else
			rt_maskedcopy(nh->nh_ifa->ifa_addr,
				      (struct sockaddr *)&laddr,
				      nh->nh_ifa->ifa_netmask);
		/* 
		 * refactor rt and no lock operation necessary
		 */
		rc->rc_rt = (struct rtentry *)rnh->rnh_matchaddr(
		    (struct sockaddr *)&laddr, &rnh->head);
		if (rc->rc_rt == NULL) {
			RIB_RUNLOCK(rnh);
			return (ESRCH);
		}
		nh = select_nhop(rt_get_raw_nhop(rc->rc_rt), info->rti_info[RTAX_GATEWAY]);
		if (nh == NULL) {
			RIB_RUNLOCK(rnh);
			return (ESRCH);
		}
	}
	rc->rc_nh_new = nh;
	rc->rc_nh_weight = rc->rc_rt->rt_weight;
	RIB_RUNLOCK(rnh);

	return (0);
}

static void
init_sockaddrs_family(int family, struct sockaddr *dst, struct sockaddr *mask)
{
#ifdef INET
	if (family == AF_INET) {
		struct sockaddr_in *dst4 = (struct sockaddr_in *)dst;
		struct sockaddr_in *mask4 = (struct sockaddr_in *)mask;

		bzero(dst4, sizeof(struct sockaddr_in));
		bzero(mask4, sizeof(struct sockaddr_in));

		dst4->sin_family = AF_INET;
		dst4->sin_len = sizeof(struct sockaddr_in);
		mask4->sin_family = AF_INET;
		mask4->sin_len = sizeof(struct sockaddr_in);
	}
#endif
#ifdef INET6
	if (family == AF_INET6) {
		struct sockaddr_in6 *dst6 = (struct sockaddr_in6 *)dst;
		struct sockaddr_in6 *mask6 = (struct sockaddr_in6 *)mask;

		bzero(dst6, sizeof(struct sockaddr_in6));
		bzero(mask6, sizeof(struct sockaddr_in6));

		dst6->sin6_family = AF_INET6;
		dst6->sin6_len = sizeof(struct sockaddr_in6);
		mask6->sin6_family = AF_INET6;
		mask6->sin6_len = sizeof(struct sockaddr_in6);
	}
#endif
}

static void
export_rtaddrs(const struct rtentry *rt, struct sockaddr *dst,
    struct sockaddr *mask)
{
#ifdef INET
	if (dst->sa_family == AF_INET) {
		struct sockaddr_in *dst4 = (struct sockaddr_in *)dst;
		struct sockaddr_in *mask4 = (struct sockaddr_in *)mask;
		uint32_t scopeid = 0;
		rt_get_inet_prefix_pmask(rt, &dst4->sin_addr, &mask4->sin_addr,
		    &scopeid);
		return;
	}
#endif
#ifdef INET6
	if (dst->sa_family == AF_INET6) {
		struct sockaddr_in6 *dst6 = (struct sockaddr_in6 *)dst;
		struct sockaddr_in6 *mask6 = (struct sockaddr_in6 *)mask;
		uint32_t scopeid = 0;
		rt_get_inet6_prefix_pmask(rt, &dst6->sin6_addr,
		    &mask6->sin6_addr, &scopeid);
		dst6->sin6_scope_id = scopeid;
		return;
	}
#endif
}

static int
update_rtm_from_info(struct rt_addrinfo *info, struct rt_msghdr **prtm,
    int alloc_len)
{
	struct rt_msghdr *rtm, *orig_rtm = NULL;
	struct walkarg w;
	int len;

	rtm = *prtm;
	/* Check if we need to realloc storage */
	rtsock_msg_buffer(rtm->rtm_type, info, NULL, &len);
	if (len > alloc_len) {
		struct rt_msghdr *tmp_rtm;

		tmp_rtm = malloc(len, M_TEMP, M_NOWAIT);
		if (tmp_rtm == NULL)
			return (ENOBUFS);
		bcopy(rtm, tmp_rtm, rtm->rtm_msglen);
		orig_rtm = rtm;
		rtm = tmp_rtm;
		alloc_len = len;

		/*
		 * Delay freeing original rtm as info contains
		 * data referencing it.
		 */
	}

	w.w_tmem = (caddr_t)rtm;
	w.w_tmemsize = alloc_len;
	rtsock_msg_buffer(rtm->rtm_type, info, &w, &len);
	rtm->rtm_addrs = info->rti_addrs;

	if (orig_rtm != NULL)
		free(orig_rtm, M_TEMP);
	*prtm = rtm;
	return (0);
}


/*
 * Update sockaddrs, flags, etc in @prtm based on @rc data.
 * rtm can be reallocated.
 *
 * Returns 0 on success, along with pointer to (potentially reallocated)
 *  rtm.
 *
 */
static int
update_rtm_from_rc(struct rt_addrinfo *info, struct rt_msghdr **prtm,
    int alloc_len, struct rib_cmd_info *rc, struct nhop_object *nh)
{
	union sockaddr_union saun;
	struct rt_msghdr *rtm;
	struct ifnet *ifp;
	int error;

	rtm = *prtm;
	union sockaddr_union sa_dst, sa_mask;
	int family = info->rti_info[RTAX_DST]->sa_family;
	init_sockaddrs_family(family, &sa_dst.sa, &sa_mask.sa);
	export_rtaddrs(rc->rc_rt, &sa_dst.sa, &sa_mask.sa);

	info->rti_info[RTAX_DST] = &sa_dst.sa;
	info->rti_info[RTAX_NETMASK] = rt_is_host(rc->rc_rt) ? NULL : &sa_mask.sa;
	info->rti_info[RTAX_GATEWAY] = &nh->gw_sa;
	info->rti_info[RTAX_GENMASK] = 0;
	ifp = nh->nh_ifp;
	if (rtm->rtm_addrs & (RTA_IFP | RTA_IFA)) {
		if (ifp) {
			info->rti_info[RTAX_IFP] =
			    ifp->if_addr->ifa_addr;
			error = rtm_get_jailed(info, ifp, nh,
			    &saun, curthread->td_ucred);
			if (error != 0)
				return (error);
			if (ifp->if_flags & IFF_POINTOPOINT)
				info->rti_info[RTAX_BRD] =
				    nh->nh_ifa->ifa_dstaddr;
			rtm->rtm_index = ifp->if_index;
		} else {
			info->rti_info[RTAX_IFP] = NULL;
			info->rti_info[RTAX_IFA] = NULL;
		}
	} else if (ifp != NULL)
		rtm->rtm_index = ifp->if_index;

	if ((error = update_rtm_from_info(info, prtm, alloc_len)) != 0)
		return (error);

	rtm = *prtm;
	rtm->rtm_flags = rc->rc_rt->rte_flags | nhop_get_rtflags(nh);
	if (rtm->rtm_flags & RTF_GWFLAG_COMPAT)
		rtm->rtm_flags = RTF_GATEWAY | 
			(rtm->rtm_flags & ~RTF_GWFLAG_COMPAT);
	rt_getmetrics(rc->rc_rt, nh, &rtm->rtm_rmx);
	rtm->rtm_rmx.rmx_weight = rc->rc_nh_weight;

	return (0);
}

#ifdef ROUTE_MPATH
static void
save_del_notification(const struct rib_cmd_info *rc, void *_cbdata)
{
	struct rib_cmd_info *rc_new = (struct rib_cmd_info *)_cbdata;

	if (rc->rc_cmd == RTM_DELETE)
		*rc_new = *rc;
}

static void
save_add_notification(const struct rib_cmd_info *rc, void *_cbdata)
{
	struct rib_cmd_info *rc_new = (struct rib_cmd_info *)_cbdata;

	if (rc->rc_cmd == RTM_ADD)
		*rc_new = *rc;
}
#endif

static struct sockaddr *
alloc_sockaddr_aligned(struct linear_buffer *lb, int len)
{
	len = roundup2(len, sizeof(uint64_t));
	if (lb->offset + len > lb->size)
		return (NULL);
	struct sockaddr *sa = (struct sockaddr *)(lb->base + lb->offset);
	lb->offset += len;
	return (sa);
}

/*ARGSUSED*/
static int
route_output(struct mbuf *m, struct socket *so, ...)
{
	struct rt_msghdr *rtm = NULL;
	struct rtentry *rt = NULL;
	struct rt_addrinfo info;
	struct epoch_tracker et;
#ifdef INET6
	struct sockaddr_storage ss;
	struct sockaddr_in6 *sin6;
	int i, rti_need_deembed = 0;
#endif
	int alloc_len = 0, len, error = 0, fibnum;
	sa_family_t saf = AF_UNSPEC;
	struct rib_cmd_info rc;
	struct nhop_object *nh;

	fibnum = so->so_fibnum;
#define senderr(e) { error = e; goto flush;}
	if (m == NULL || ((m->m_len < sizeof(long)) &&
		       (m = m_pullup(m, sizeof(long))) == NULL))
		return (ENOBUFS);
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("route_output");
	NET_EPOCH_ENTER(et);
	len = m->m_pkthdr.len;
	if (len < sizeof(*rtm) ||
	    len != mtod(m, struct rt_msghdr *)->rtm_msglen)
		senderr(EINVAL);

	/*
	 * Most of current messages are in range 200-240 bytes,
	 * minimize possible re-allocation on reply using larger size
	 * buffer aligned on 1k boundaty.
	 */
	alloc_len = roundup2(len, 1024);
	int total_len = alloc_len + SCRATCH_BUFFER_SIZE;
	if ((rtm = malloc(total_len, M_TEMP, M_NOWAIT)) == NULL)
		senderr(ENOBUFS);

	m_copydata(m, 0, len, (caddr_t)rtm);
	bzero(&info, sizeof(info));
	nh = NULL;
	struct linear_buffer lb = {
		.base = (char *)rtm + alloc_len,
		.size = SCRATCH_BUFFER_SIZE,
	};

	if (rtm->rtm_version != RTM_VERSION) {
		/* Do not touch message since format is unknown */
		free(rtm, M_TEMP);
		rtm = NULL;
		senderr(EPROTONOSUPPORT);
	}

	/*
	 * Starting from here, it is possible
	 * to alter original message and insert
	 * caller PID and error value.
	 */

	if ((error = fill_addrinfo(rtm, len, &lb, fibnum, &info)) != 0) {
		senderr(error);
	}
	/* fill_addringo() embeds scope into IPv6 addresses */
#ifdef INET6
	rti_need_deembed = 1;
#endif

	saf = info.rti_info[RTAX_DST]->sa_family;

	/* support for new ARP code */
	if (rtm->rtm_flags & RTF_LLDATA) {
		error = lla_rt_output(rtm, &info);
		goto flush;
	}

	union sockaddr_union gw_saun;
	int blackhole_flags = rtm->rtm_flags & (RTF_BLACKHOLE|RTF_REJECT);
	if (blackhole_flags != 0) {
		if (blackhole_flags != (RTF_BLACKHOLE | RTF_REJECT))
			error = fill_blackholeinfo(&info, &gw_saun);
		else {
			RTS_PID_LOG(LOG_DEBUG, "both BLACKHOLE and REJECT flags specifiied");
			error = EINVAL;
		}
		if (error != 0)
			senderr(error);
	}

	switch (rtm->rtm_type) {
	case RTM_ADD:
	case RTM_CHANGE:
		if (rtm->rtm_type == RTM_ADD) {
			if (info.rti_info[RTAX_GATEWAY] == NULL) {
				RTS_PID_LOG(LOG_DEBUG, "RTM_ADD w/o gateway");
				senderr(EINVAL);
			}
		}
		error = rib_action(fibnum, rtm->rtm_type, &info, &rc);
		if (error == 0) {
			rtsock_notify_event(fibnum, &rc);
#ifdef ROUTE_MPATH
			if (NH_IS_NHGRP(rc.rc_nh_new) ||
			    (rc.rc_nh_old && NH_IS_NHGRP(rc.rc_nh_old))) {
				struct rib_cmd_info rc_simple = {};
				rib_decompose_notification(&rc,
				    save_add_notification, (void *)&rc_simple);
				rc = rc_simple;
			}
#endif
			/* nh MAY be empty if RTM_CHANGE request is no-op */
			nh = rc.rc_nh_new;
			if (nh != NULL) {
				rtm->rtm_index = nh->nh_ifp->if_index;
				rtm->rtm_flags = rc.rc_rt->rte_flags | nhop_get_rtflags(nh);
			}
		}
		break;

	case RTM_DELETE:
		error = rib_action(fibnum, RTM_DELETE, &info, &rc);
		if (error == 0) {
			rtsock_notify_event(fibnum, &rc);
#ifdef ROUTE_MPATH
			if (NH_IS_NHGRP(rc.rc_nh_old) ||
			    (rc.rc_nh_new && NH_IS_NHGRP(rc.rc_nh_new))) {
				struct rib_cmd_info rc_simple = {};
				rib_decompose_notification(&rc,
				    save_del_notification, (void *)&rc_simple);
				rc = rc_simple;
			}
#endif
			nh = rc.rc_nh_old;
		}
		break;

	case RTM_GET:
		error = handle_rtm_get(&info, fibnum, rtm, &rc);
		if (error != 0)
			senderr(error);
		nh = rc.rc_nh_new;

		if (!can_export_rte(curthread->td_ucred,
		    info.rti_info[RTAX_NETMASK] == NULL,
		    info.rti_info[RTAX_DST])) {
			senderr(ESRCH);
		}
		break;

	default:
		senderr(EOPNOTSUPP);
	}

	if (error == 0 && nh != NULL) {
		error = update_rtm_from_rc(&info, &rtm, alloc_len, &rc, nh);
		/*
		 * Note that some sockaddr pointers may have changed to
		 * point to memory outsize @rtm. Some may be pointing
		 * to the on-stack variables.
		 * Given that, any pointer in @info CANNOT BE USED.
		 */

		/*
		 * scopeid deembedding has been performed while
		 * writing updated rtm in rtsock_msg_buffer().
		 * With that in mind, skip deembedding procedure below.
		 */
#ifdef INET6
		rti_need_deembed = 0;
#endif
	}

flush:
	NET_EPOCH_EXIT(et);
	rt = NULL;

#ifdef INET6
	if (rtm != NULL) {
		if (rti_need_deembed) {
			/* sin6_scope_id is recovered before sending rtm. */
			sin6 = (struct sockaddr_in6 *)&ss;
			for (i = 0; i < RTAX_MAX; i++) {
				if (info.rti_info[i] == NULL)
					continue;
				if (info.rti_info[i]->sa_family != AF_INET6)
					continue;
				bcopy(info.rti_info[i], sin6, sizeof(*sin6));
				if (sa6_recoverscope(sin6) == 0)
					bcopy(sin6, info.rti_info[i],
						    sizeof(*sin6));
			}
			if (update_rtm_from_info(&info, &rtm, alloc_len) != 0) {
				if (error != 0)
					error = ENOBUFS;
			}
		}
	}
#endif
	send_rtm_reply(so, rtm, m, saf, fibnum, error);

	return (error);
}

/*
 * Sends the prepared reply message in @rtm to all rtsock clients.
 * Frees @m and @rtm.
 *
 */
static void
send_rtm_reply(struct socket *so, struct rt_msghdr *rtm, struct mbuf *m,
    sa_family_t saf, u_int fibnum, int rtm_errno)
{
	struct rawcb *rp = NULL;

	/*
	 * Check to see if we don't want our own messages.
	 */
	if ((so->so_options & SO_USELOOPBACK) == 0) {
		if (V_route_cb.any_count <= 1) {
			if (rtm != NULL)
				free(rtm, M_TEMP);
			m_freem(m);
			return;
		}
		/* There is another listener, so construct message */
		rp = sotorawcb(so);
	}

	if (rtm != NULL) {
		if (rtm_errno!= 0)
			rtm->rtm_errno = rtm_errno;
		else
			rtm->rtm_flags |= RTF_DONE;

		m_copyback(m, 0, rtm->rtm_msglen, (caddr_t)rtm);
		if (m->m_pkthdr.len < rtm->rtm_msglen) {
			m_freem(m);
			m = NULL;
		} else if (m->m_pkthdr.len > rtm->rtm_msglen)
			m_adj(m, rtm->rtm_msglen - m->m_pkthdr.len);

		free(rtm, M_TEMP);
	}
	if (m != NULL) {
		M_SETFIB(m, fibnum);
		m->m_flags |= RTS_FILTER_FIB;
		if (rp) {
			/*
			 * XXX insure we don't get a copy by
			 * invalidating our protocol
			 */
			unsigned short family = rp->rcb_proto.sp_family;
			rp->rcb_proto.sp_family = 0;
			rt_dispatch(m, saf);
			rp->rcb_proto.sp_family = family;
		} else
			rt_dispatch(m, saf);
	}
}

static void
rt_getmetrics(const struct rtentry *rt, const struct nhop_object *nh,
    struct rt_metrics *out)
{

	bzero(out, sizeof(*out));
	out->rmx_mtu = nh->nh_mtu;
	out->rmx_weight = rt->rt_weight;
	out->rmx_nhidx = nhop_get_idx(nh);
	/* Kernel -> userland timebase conversion. */
	out->rmx_expire = nhop_get_expire(nh) ?
	    nhop_get_expire(nh) - time_uptime + time_second : 0;
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
		if (cp + sa->sa_len > cplim) {
			RTS_PID_LOG(LOG_DEBUG, "sa_len too big for sa type %d", i);
			return (EINVAL);
		}
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
#ifdef INET6
		if (sa->sa_family == AF_INET6)
			sa6_embedscope((struct sockaddr_in6 *)sa,
			    V_ip6_use_defzone);
#endif
		rtinfo->rti_info[i] = sa;
		cp += SA_SIZE(sa);
	}
	return (0);
}

#ifdef INET
static inline void
fill_sockaddr_inet(struct sockaddr_in *sin, struct in_addr addr)
{

	const struct sockaddr_in nsin = {
		.sin_family = AF_INET,
		.sin_len = sizeof(struct sockaddr_in),
		.sin_addr = addr,
	};
	*sin = nsin;
}
#endif

#ifdef INET6
static inline void
fill_sockaddr_inet6(struct sockaddr_in6 *sin6, const struct in6_addr *addr6,
    uint32_t scopeid)
{

	const struct sockaddr_in6 nsin6 = {
		.sin6_family = AF_INET6,
		.sin6_len = sizeof(struct sockaddr_in6),
		.sin6_addr = *addr6,
		.sin6_scope_id = scopeid,
	};
	*sin6 = nsin6;
}
#endif

/*
 * Checks if gateway is suitable for lltable operations.
 * Lltable code requires AF_LINK gateway with ifindex
 *  and mac address specified.
 * Returns 0 on success.
 */
static int
cleanup_xaddrs_lladdr(struct rt_addrinfo *info)
{
	struct sockaddr_dl *sdl = (struct sockaddr_dl *)info->rti_info[RTAX_GATEWAY];

	if (sdl->sdl_family != AF_LINK)
		return (EINVAL);

	if (sdl->sdl_index == 0) {
		RTS_PID_LOG(LOG_DEBUG, "AF_LINK gateway w/o ifindex");
		return (EINVAL);
	}

	if (offsetof(struct sockaddr_dl, sdl_data) + sdl->sdl_nlen + sdl->sdl_alen > sdl->sdl_len) {
		RTS_PID_LOG(LOG_DEBUG, "AF_LINK gw: sdl_nlen/sdl_alen too large");
		return (EINVAL);
	}

	return (0);
}

static int
cleanup_xaddrs_gateway(struct rt_addrinfo *info, struct linear_buffer *lb)
{
	struct sockaddr *gw = info->rti_info[RTAX_GATEWAY];
	struct sockaddr *sa;

	if (info->rti_flags & RTF_LLDATA)
		return (cleanup_xaddrs_lladdr(info));

	switch (gw->sa_family) {
#ifdef INET
	case AF_INET:
		{
			struct sockaddr_in *gw_sin = (struct sockaddr_in *)gw;

			/* Ensure reads do not go beyoud SA boundary */
			if (SA_SIZE(gw) < offsetof(struct sockaddr_in, sin_zero)) {
				RTS_PID_LOG(LOG_DEBUG, "gateway sin_len too small: %d",
				    gw->sa_len);
				return (EINVAL);
			}
			sa = alloc_sockaddr_aligned(lb, sizeof(struct sockaddr_in));
			if (sa == NULL)
				return (ENOBUFS);
			fill_sockaddr_inet((struct sockaddr_in *)sa, gw_sin->sin_addr);
			info->rti_info[RTAX_GATEWAY] = sa;
		}
		break;
#endif
#ifdef INET6
	case AF_INET6:
		{
			struct sockaddr_in6 *gw_sin6 = (struct sockaddr_in6 *)gw;
			if (gw_sin6->sin6_len < sizeof(struct sockaddr_in6)) {
				RTS_PID_LOG(LOG_DEBUG, "gateway sin6_len too small: %d",
				    gw->sa_len);
				return (EINVAL);
			}
			fill_sockaddr_inet6(gw_sin6, &gw_sin6->sin6_addr, 0);
			break;
		}
#endif
	case AF_LINK:
		{
			struct sockaddr_dl *gw_sdl;

			size_t sdl_min_len = offsetof(struct sockaddr_dl, sdl_data);
			gw_sdl = (struct sockaddr_dl *)gw;
			if (gw_sdl->sdl_len < sdl_min_len) {
				RTS_PID_LOG(LOG_DEBUG, "gateway sdl_len too small: %d",
				    gw_sdl->sdl_len);
				return (EINVAL);
			}
			sa = alloc_sockaddr_aligned(lb, sizeof(struct sockaddr_dl_short));
			if (sa == NULL)
				return (ENOBUFS);

			const struct sockaddr_dl_short sdl = {
				.sdl_family = AF_LINK,
				.sdl_len = sizeof(struct sockaddr_dl_short),
				.sdl_index = gw_sdl->sdl_index,
			};
			*((struct sockaddr_dl_short *)sa) = sdl;
			info->rti_info[RTAX_GATEWAY] = sa;
			break;
		}
	}

	return (0);
}

static void
remove_netmask(struct rt_addrinfo *info)
{
	info->rti_info[RTAX_NETMASK] = NULL;
	info->rti_flags |= RTF_HOST;
	info->rti_addrs &= ~RTA_NETMASK;
}

#ifdef INET
static int
cleanup_xaddrs_inet(struct rt_addrinfo *info, struct linear_buffer *lb)
{
	struct sockaddr_in *dst_sa, *mask_sa;
	const int sa_len = sizeof(struct sockaddr_in);
	struct in_addr dst, mask;

	/* Check & fixup dst/netmask combination first */
	dst_sa = (struct sockaddr_in *)info->rti_info[RTAX_DST];
	mask_sa = (struct sockaddr_in *)info->rti_info[RTAX_NETMASK];

	/* Ensure reads do not go beyound the buffer size */
	if (SA_SIZE(dst_sa) < offsetof(struct sockaddr_in, sin_zero)) {
		RTS_PID_LOG(LOG_DEBUG, "prefix dst sin_len too small: %d",
		    dst_sa->sin_len);
		return (EINVAL);
	}

	if ((mask_sa != NULL) && mask_sa->sin_len < sizeof(struct sockaddr_in)) {
		/*
		 * Some older routing software encode mask length into the
		 * sin_len, thus resulting in "truncated" sockaddr.
		 */
		int len = mask_sa->sin_len - offsetof(struct sockaddr_in, sin_addr);
		if (len >= 0) {
			mask.s_addr = 0;
			if (len > sizeof(struct in_addr))
				len = sizeof(struct in_addr);
			memcpy(&mask, &mask_sa->sin_addr, len);
		} else {
			RTS_PID_LOG(LOG_DEBUG, "prefix mask sin_len too small: %d",
			    mask_sa->sin_len);
			return (EINVAL);
		}
	} else
		mask.s_addr = mask_sa ? mask_sa->sin_addr.s_addr : INADDR_BROADCAST;

	dst.s_addr = htonl(ntohl(dst_sa->sin_addr.s_addr) & ntohl(mask.s_addr));

	/* Construct new "clean" dst/mask sockaddresses */
	if ((dst_sa = (struct sockaddr_in *)alloc_sockaddr_aligned(lb, sa_len)) == NULL)
		return (ENOBUFS);
	fill_sockaddr_inet(dst_sa, dst);
	info->rti_info[RTAX_DST] = (struct sockaddr *)dst_sa;

	if (mask.s_addr != INADDR_BROADCAST) {
		if ((mask_sa = (struct sockaddr_in *)alloc_sockaddr_aligned(lb, sa_len)) == NULL)
			return (ENOBUFS);
		fill_sockaddr_inet(mask_sa, mask);
		info->rti_info[RTAX_NETMASK] = (struct sockaddr *)mask_sa;
		info->rti_flags &= ~RTF_HOST;
	} else
		remove_netmask(info);

	/* Check gateway */
	if (info->rti_info[RTAX_GATEWAY] != NULL)
		return (cleanup_xaddrs_gateway(info, lb));

	return (0);
}
#endif

#ifdef INET6
static int
cleanup_xaddrs_inet6(struct rt_addrinfo *info, struct linear_buffer *lb)
{
	struct sockaddr *sa;
	struct sockaddr_in6 *dst_sa, *mask_sa;
	struct in6_addr mask, *dst;
	const int sa_len = sizeof(struct sockaddr_in6);

	/* Check & fixup dst/netmask combination first */
	dst_sa = (struct sockaddr_in6 *)info->rti_info[RTAX_DST];
	mask_sa = (struct sockaddr_in6 *)info->rti_info[RTAX_NETMASK];

	if (dst_sa->sin6_len < sizeof(struct sockaddr_in6)) {
		RTS_PID_LOG(LOG_DEBUG, "prefix dst sin6_len too small: %d",
		    dst_sa->sin6_len);
		return (EINVAL);
	}

	if (mask_sa && mask_sa->sin6_len < sizeof(struct sockaddr_in6)) {
		/*
		 * Some older routing software encode mask length into the
		 * sin6_len, thus resulting in "truncated" sockaddr.
		 */
		int len = mask_sa->sin6_len - offsetof(struct sockaddr_in6, sin6_addr);
		if (len >= 0) {
			bzero(&mask, sizeof(mask));
			if (len > sizeof(struct in6_addr))
				len = sizeof(struct in6_addr);
			memcpy(&mask, &mask_sa->sin6_addr, len);
		} else {
			RTS_PID_LOG(LOG_DEBUG, "rtsock: prefix mask sin6_len too small: %d",
			    mask_sa->sin6_len);
			return (EINVAL);
		}
	} else
		mask = mask_sa ? mask_sa->sin6_addr : in6mask128;

	dst = &dst_sa->sin6_addr;
	IN6_MASK_ADDR(dst, &mask);

	if ((sa = alloc_sockaddr_aligned(lb, sa_len)) == NULL)
		return (ENOBUFS);
	fill_sockaddr_inet6((struct sockaddr_in6 *)sa, dst, 0);
	info->rti_info[RTAX_DST] = sa;

	if (!IN6_ARE_ADDR_EQUAL(&mask, &in6mask128)) {
		if ((sa = alloc_sockaddr_aligned(lb, sa_len)) == NULL)
			return (ENOBUFS);
		fill_sockaddr_inet6((struct sockaddr_in6 *)sa, &mask, 0);
		info->rti_info[RTAX_NETMASK] = sa;
		info->rti_flags &= ~RTF_HOST;
	} else
		remove_netmask(info);

	/* Check gateway */
	if (info->rti_info[RTAX_GATEWAY] != NULL)
		return (cleanup_xaddrs_gateway(info, lb));

	return (0);
}
#endif

static int
cleanup_xaddrs(struct rt_addrinfo *info, struct linear_buffer *lb)
{
	int error = EAFNOSUPPORT;

	if (info->rti_info[RTAX_DST] == NULL) {
		RTS_PID_LOG(LOG_DEBUG, "prefix dst is not set");
		return (EINVAL);
	}

	if (info->rti_flags & RTF_LLDATA) {
		/*
		 * arp(8)/ndp(8) sends RTA_NETMASK for the associated
		 * prefix along with the actual address in RTA_DST.
		 * Remove netmask to avoid unnecessary address masking.
		 */
		remove_netmask(info);
	}

	switch (info->rti_info[RTAX_DST]->sa_family) {
#ifdef INET
	case AF_INET:
		error = cleanup_xaddrs_inet(info, lb);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		error = cleanup_xaddrs_inet6(info, lb);
		break;
#endif
	}

	return (error);
}

/*
 * Fill in @dmask with valid netmask leaving original @smask
 * intact. Mostly used with radix netmasks.
 */
struct sockaddr *
rtsock_fix_netmask(const struct sockaddr *dst, const struct sockaddr *smask,
    struct sockaddr_storage *dmask)
{
	if (dst == NULL || smask == NULL)
		return (NULL);

	memset(dmask, 0, dst->sa_len);
	memcpy(dmask, smask, smask->sa_len);
	dmask->ss_len = dst->sa_len;
	dmask->ss_family = dst->sa_family;

	return ((struct sockaddr *)dmask);
}

/*
 * Writes information related to @rtinfo object to newly-allocated mbuf.
 * Assumes MCLBYTES is enough to construct any message.
 * Used for OS notifications of vaious events (if/ifa announces,etc)
 *
 * Returns allocated mbuf or NULL on failure.
 */
static struct mbuf *
rtsock_msg_mbuf(int type, struct rt_addrinfo *rtinfo)
{
	struct sockaddr_storage ss;
	struct rt_msghdr *rtm;
	struct mbuf *m;
	int i;
	struct sockaddr *sa;
#ifdef INET6
	struct sockaddr_in6 *sin6;
#endif
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

	/* XXXGL: can we use MJUMPAGESIZE cluster here? */
	KASSERT(len <= MCLBYTES, ("%s: message too big", __func__));
	if (len > MHLEN)
		m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	else
		m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m == NULL)
		return (m);

	m->m_pkthdr.len = m->m_len = len;
	rtm = mtod(m, struct rt_msghdr *);
	bzero((caddr_t)rtm, len);
	for (i = 0; i < RTAX_MAX; i++) {
		if ((sa = rtinfo->rti_info[i]) == NULL)
			continue;
		rtinfo->rti_addrs |= (1 << i);

		dlen = SA_SIZE(sa);
		KASSERT(dlen <= sizeof(ss),
		    ("%s: sockaddr size overflow", __func__));
		bzero(&ss, sizeof(ss));
		bcopy(sa, &ss, sa->sa_len);
		sa = (struct sockaddr *)&ss;
#ifdef INET6
		if (sa->sa_family == AF_INET6) {
			sin6 = (struct sockaddr_in6 *)sa;
			(void)sa6_recoverscope(sin6);
		}
#endif
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

/*
 * Writes information related to @rtinfo object to preallocated buffer.
 * Stores needed size in @plen. If @w is NULL, calculates size without
 * writing.
 * Used for sysctl dumps and rtsock answers (RTM_DEL/RTM_GET) generation.
 *
 * Returns 0 on success.
 *
 */
static int
rtsock_msg_buffer(int type, struct rt_addrinfo *rtinfo, struct walkarg *w, int *plen)
{
	struct sockaddr_storage ss;
	int len, buflen = 0, dlen, i;
	caddr_t cp = NULL;
	struct rt_msghdr *rtm = NULL;
#ifdef INET6
	struct sockaddr_in6 *sin6;
#endif
#ifdef COMPAT_FREEBSD32
	bool compat32 = false;
#endif

	switch (type) {
	case RTM_DELADDR:
	case RTM_NEWADDR:
		if (w != NULL && w->w_op == NET_RT_IFLISTL) {
#ifdef COMPAT_FREEBSD32
			if (w->w_req->flags & SCTL_MASK32) {
				len = sizeof(struct ifa_msghdrl32);
				compat32 = true;
			} else
#endif
				len = sizeof(struct ifa_msghdrl);
		} else
			len = sizeof(struct ifa_msghdr);
		break;

	case RTM_IFINFO:
#ifdef COMPAT_FREEBSD32
		if (w != NULL && w->w_req->flags & SCTL_MASK32) {
			if (w->w_op == NET_RT_IFLISTL)
				len = sizeof(struct if_msghdrl32);
			else
				len = sizeof(struct if_msghdr32);
			compat32 = true;
			break;
		}
#endif
		if (w != NULL && w->w_op == NET_RT_IFLISTL)
			len = sizeof(struct if_msghdrl);
		else
			len = sizeof(struct if_msghdr);
		break;

	case RTM_NEWMADDR:
		len = sizeof(struct ifma_msghdr);
		break;

	default:
		len = sizeof(struct rt_msghdr);
	}

	if (w != NULL) {
		rtm = (struct rt_msghdr *)w->w_tmem;
		buflen = w->w_tmemsize - len;
		cp = (caddr_t)w->w_tmem + len;
	}

	rtinfo->rti_addrs = 0;
	for (i = 0; i < RTAX_MAX; i++) {
		struct sockaddr *sa;

		if ((sa = rtinfo->rti_info[i]) == NULL)
			continue;
		rtinfo->rti_addrs |= (1 << i);
#ifdef COMPAT_FREEBSD32
		if (compat32)
			dlen = SA_SIZE32(sa);
		else
#endif
			dlen = SA_SIZE(sa);
		if (cp != NULL && buflen >= dlen) {
			KASSERT(dlen <= sizeof(ss),
			    ("%s: sockaddr size overflow", __func__));
			bzero(&ss, sizeof(ss));
			bcopy(sa, &ss, sa->sa_len);
			sa = (struct sockaddr *)&ss;
#ifdef INET6
			if (sa->sa_family == AF_INET6) {
				sin6 = (struct sockaddr_in6 *)sa;
				(void)sa6_recoverscope(sin6);
			}
#endif
			bcopy((caddr_t)sa, cp, (unsigned)dlen);
			cp += dlen;
			buflen -= dlen;
		} else if (cp != NULL) {
			/*
			 * Buffer too small. Count needed size
			 * and return with error.
			 */
			cp = NULL;
		}

		len += dlen;
	}

	if (cp != NULL) {
		dlen = ALIGN(len) - len;
		if (buflen < dlen)
			cp = NULL;
		else {
			bzero(cp, dlen);
			cp += dlen;
			buflen -= dlen;
		}
	}
	len = ALIGN(len);

	if (cp != NULL) {
		/* fill header iff buffer is large enough */
		rtm->rtm_version = RTM_VERSION;
		rtm->rtm_type = type;
		rtm->rtm_msglen = len;
	}

	*plen = len;

	if (w != NULL && cp == NULL)
		return (ENOBUFS);

	return (0);
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that a redirect has occurred, a routing lookup
 * has failed, or that a protocol has detected timeouts to a particular
 * destination.
 */
void
rt_missmsg_fib(int type, struct rt_addrinfo *rtinfo, int flags, int error,
    int fibnum)
{
	struct rt_msghdr *rtm;
	struct mbuf *m;
	struct sockaddr *sa = rtinfo->rti_info[RTAX_DST];

	if (V_route_cb.any_count == 0)
		return;
	m = rtsock_msg_mbuf(type, rtinfo);
	if (m == NULL)
		return;

	if (fibnum != RT_ALL_FIBS) {
		KASSERT(fibnum >= 0 && fibnum < rt_numfibs, ("%s: fibnum out "
		    "of range 0 <= %d < %d", __func__, fibnum, rt_numfibs));
		M_SETFIB(m, fibnum);
		m->m_flags |= RTS_FILTER_FIB;
	}

	rtm = mtod(m, struct rt_msghdr *);
	rtm->rtm_flags = RTF_DONE | flags;
	rtm->rtm_errno = error;
	rtm->rtm_addrs = rtinfo->rti_addrs;
	rt_dispatch(m, sa ? sa->sa_family : AF_UNSPEC);
}

void
rt_missmsg(int type, struct rt_addrinfo *rtinfo, int flags, int error)
{

	rt_missmsg_fib(type, rtinfo, flags, error, RT_ALL_FIBS);
}

/*
 * This routine is called to generate a message from the routing
 * socket indicating that the status of a network interface has changed.
 */
static void
rtsock_ifmsg(struct ifnet *ifp, int if_flags_mask __unused)
{
	struct if_msghdr *ifm;
	struct mbuf *m;
	struct rt_addrinfo info;

	if (V_route_cb.any_count == 0)
		return;
	bzero((caddr_t)&info, sizeof(info));
	m = rtsock_msg_mbuf(RTM_IFINFO, &info);
	if (m == NULL)
		return;
	ifm = mtod(m, struct if_msghdr *);
	ifm->ifm_index = ifp->if_index;
	ifm->ifm_flags = ifp->if_flags | ifp->if_drv_flags;
	if_data_copy(ifp, &ifm->ifm_data);
	ifm->ifm_addrs = 0;
	rt_dispatch(m, AF_UNSPEC);
}

/*
 * Announce interface address arrival/withdraw.
 * Please do not call directly, use rt_addrmsg().
 * Assume input data to be valid.
 * Returns 0 on success.
 */
int
rtsock_addrmsg(int cmd, struct ifaddr *ifa, int fibnum)
{
	struct rt_addrinfo info;
	struct sockaddr *sa;
	int ncmd;
	struct mbuf *m;
	struct ifa_msghdr *ifam;
	struct ifnet *ifp = ifa->ifa_ifp;
	struct sockaddr_storage ss;

	if (V_route_cb.any_count == 0)
		return (0);

	ncmd = cmd == RTM_ADD ? RTM_NEWADDR : RTM_DELADDR;

	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_IFA] = sa = ifa->ifa_addr;
	info.rti_info[RTAX_IFP] = ifp->if_addr->ifa_addr;
	info.rti_info[RTAX_NETMASK] = rtsock_fix_netmask(
	    info.rti_info[RTAX_IFA], ifa->ifa_netmask, &ss);
	info.rti_info[RTAX_BRD] = ifa->ifa_dstaddr;
	if ((m = rtsock_msg_mbuf(ncmd, &info)) == NULL)
		return (ENOBUFS);
	ifam = mtod(m, struct ifa_msghdr *);
	ifam->ifam_index = ifp->if_index;
	ifam->ifam_metric = ifa->ifa_ifp->if_metric;
	ifam->ifam_flags = ifa->ifa_flags;
	ifam->ifam_addrs = info.rti_addrs;

	if (fibnum != RT_ALL_FIBS) {
		M_SETFIB(m, fibnum);
		m->m_flags |= RTS_FILTER_FIB;
	}

	rt_dispatch(m, sa ? sa->sa_family : AF_UNSPEC);

	return (0);
}

/*
 * Announce route addition/removal to rtsock based on @rt data.
 * Callers are advives to use rt_routemsg() instead of using this
 *  function directly.
 * Assume @rt data is consistent.
 *
 * Returns 0 on success.
 */
int
rtsock_routemsg(int cmd, struct rtentry *rt, struct nhop_object *nh,
    int fibnum)
{
	union sockaddr_union dst, mask;
	struct rt_addrinfo info;

	if (V_route_cb.any_count == 0)
		return (0);

	int family = rt_get_family(rt);
	init_sockaddrs_family(family, &dst.sa, &mask.sa);
	export_rtaddrs(rt, &dst.sa, &mask.sa);

	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_DST] = &dst.sa;
	info.rti_info[RTAX_NETMASK] = &mask.sa;
	info.rti_info[RTAX_GATEWAY] = &nh->gw_sa;
	info.rti_flags = rt->rte_flags | nhop_get_rtflags(nh);
	info.rti_ifp = nh->nh_ifp;

	return (rtsock_routemsg_info(cmd, &info, fibnum));
}

int
rtsock_routemsg_info(int cmd, struct rt_addrinfo *info, int fibnum)
{
	struct rt_msghdr *rtm;
	struct sockaddr *sa;
	struct mbuf *m;

	if (V_route_cb.any_count == 0)
		return (0);

	if (info->rti_flags & RTF_HOST)
		info->rti_info[RTAX_NETMASK] = NULL;

	m = rtsock_msg_mbuf(cmd, info);
	if (m == NULL)
		return (ENOBUFS);

	if (fibnum != RT_ALL_FIBS) {
		KASSERT(fibnum >= 0 && fibnum < rt_numfibs, ("%s: fibnum out "
		    "of range 0 <= %d < %d", __func__, fibnum, rt_numfibs));
		M_SETFIB(m, fibnum);
		m->m_flags |= RTS_FILTER_FIB;
	}

	rtm = mtod(m, struct rt_msghdr *);
	rtm->rtm_addrs = info->rti_addrs;
	if (info->rti_ifp != NULL)
		rtm->rtm_index = info->rti_ifp->if_index;
	/* Add RTF_DONE to indicate command 'completion' required by API */
	info->rti_flags |= RTF_DONE;
	/* Reported routes has to be up */
	if (cmd == RTM_ADD || cmd == RTM_CHANGE)
		info->rti_flags |= RTF_UP;
	rtm->rtm_flags = info->rti_flags;

	sa = info->rti_info[RTAX_DST];
	rt_dispatch(m, sa ? sa->sa_family : AF_UNSPEC);

	return (0);
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

	if (V_route_cb.any_count == 0)
		return;

	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_IFA] = ifma->ifma_addr;
	if (ifp && ifp->if_addr)
		info.rti_info[RTAX_IFP] = ifp->if_addr->ifa_addr;
	else
		info.rti_info[RTAX_IFP] = NULL;
	/*
	 * If a link-layer address is present, present it as a ``gateway''
	 * (similarly to how ARP entries, e.g., are presented).
	 */
	info.rti_info[RTAX_GATEWAY] = ifma->ifma_lladdr;
	m = rtsock_msg_mbuf(cmd, &info);
	if (m == NULL)
		return;
	ifmam = mtod(m, struct ifma_msghdr *);
	KASSERT(ifp != NULL, ("%s: link-layer multicast address w/o ifp\n",
	    __func__));
	ifmam->ifmam_index = ifp->if_index;
	ifmam->ifmam_addrs = info.rti_addrs;
	rt_dispatch(m, ifma->ifma_addr ? ifma->ifma_addr->sa_family : AF_UNSPEC);
}

static struct mbuf *
rt_makeifannouncemsg(struct ifnet *ifp, int type, int what,
	struct rt_addrinfo *info)
{
	struct if_announcemsghdr *ifan;
	struct mbuf *m;

	if (V_route_cb.any_count == 0)
		return NULL;
	bzero((caddr_t)info, sizeof(*info));
	m = rtsock_msg_mbuf(type, info);
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
		rt_dispatch(m, AF_UNSPEC);
	}
}

/*
 * This is called to generate routing socket messages indicating
 * network interface arrival and departure.
 */
static void
rt_ifannouncemsg(struct ifnet *ifp, int what)
{
	struct mbuf *m;
	struct rt_addrinfo info;

	m = rt_makeifannouncemsg(ifp, RTM_IFANNOUNCE, what, &info);
	if (m != NULL)
		rt_dispatch(m, AF_UNSPEC);
}

static void
rt_dispatch(struct mbuf *m, sa_family_t saf)
{
	struct m_tag *tag;

	/*
	 * Preserve the family from the sockaddr, if any, in an m_tag for
	 * use when injecting the mbuf into the routing socket buffer from
	 * the netisr.
	 */
	if (saf != AF_UNSPEC) {
		tag = m_tag_get(PACKET_TAG_RTSOCKFAM, sizeof(unsigned short),
		    M_NOWAIT);
		if (tag == NULL) {
			m_freem(m);
			return;
		}
		*(unsigned short *)(tag + 1) = saf;
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
 * Checks if rte can be exported w.r.t jails/vnets.
 *
 * Returns true if it can, false otherwise.
 */
static bool
can_export_rte(struct ucred *td_ucred, bool rt_is_host,
    const struct sockaddr *rt_dst)
{

	if ((!rt_is_host) ? jailed_without_vnet(td_ucred)
	    : prison_if(td_ucred, rt_dst) != 0)
		return (false);
	return (true);
}


/*
 * This is used in dumping the kernel table via sysctl().
 */
static int
sysctl_dumpentry(struct rtentry *rt, void *vw)
{
	struct walkarg *w = vw;
	struct nhop_object *nh;

	NET_EPOCH_ASSERT();

	export_rtaddrs(rt, w->dst, w->mask);
	if (!can_export_rte(w->w_req->td->td_ucred, rt_is_host(rt), w->dst))
		return (0);
	nh = rt_get_raw_nhop(rt);
#ifdef ROUTE_MPATH
	if (NH_IS_NHGRP(nh)) {
		const struct weightened_nhop *wn;
		uint32_t num_nhops;
		int error;
		wn = nhgrp_get_nhops((struct nhgrp_object *)nh, &num_nhops);
		for (int i = 0; i < num_nhops; i++) {
			error = sysctl_dumpnhop(rt, wn[i].nh, wn[i].weight, w);
			if (error != 0)
				return (error);
		}
	} else
#endif
		sysctl_dumpnhop(rt, nh, rt->rt_weight, w);

	return (0);
}


static int
sysctl_dumpnhop(struct rtentry *rt, struct nhop_object *nh, uint32_t weight,
    struct walkarg *w)
{
	struct rt_addrinfo info;
	int error = 0, size;
	uint32_t rtflags;

	rtflags = nhop_get_rtflags(nh);

	if (w->w_op == NET_RT_FLAGS && !(rtflags & w->w_arg))
		return (0);

	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_DST] = w->dst;
	info.rti_info[RTAX_GATEWAY] = &nh->gw_sa;
	info.rti_info[RTAX_NETMASK] = (rtflags & RTF_HOST) ? NULL : w->mask;
	info.rti_info[RTAX_GENMASK] = 0;
	if (nh->nh_ifp && !(nh->nh_ifp->if_flags & IFF_DYING)) {
		info.rti_info[RTAX_IFP] = nh->nh_ifp->if_addr->ifa_addr;
		info.rti_info[RTAX_IFA] = nh->nh_ifa->ifa_addr;
		if (nh->nh_ifp->if_flags & IFF_POINTOPOINT)
			info.rti_info[RTAX_BRD] = nh->nh_ifa->ifa_dstaddr;
	}
	if ((error = rtsock_msg_buffer(RTM_GET, &info, w, &size)) != 0)
		return (error);
	if (w->w_req && w->w_tmem) {
		struct rt_msghdr *rtm = (struct rt_msghdr *)w->w_tmem;

		bzero(&rtm->rtm_index,
		    sizeof(*rtm) - offsetof(struct rt_msghdr, rtm_index));

		/*
		 * rte flags may consist of RTF_HOST (duplicated in nhop rtflags)
		 * and RTF_UP (if entry is linked, which is always true here).
		 * Given that, use nhop rtflags & add RTF_UP.
		 */
		rtm->rtm_flags = rtflags | RTF_UP;
		if (rtm->rtm_flags & RTF_GWFLAG_COMPAT)
			rtm->rtm_flags = RTF_GATEWAY | 
				(rtm->rtm_flags & ~RTF_GWFLAG_COMPAT);
		rt_getmetrics(rt, nh, &rtm->rtm_rmx);
		rtm->rtm_rmx.rmx_weight = weight;
		rtm->rtm_index = nh->nh_ifp->if_index;
		rtm->rtm_addrs = info.rti_addrs;
		error = SYSCTL_OUT(w->w_req, (caddr_t)rtm, size);
		return (error);
	}
	return (error);
}

static int
sysctl_iflist_ifml(struct ifnet *ifp, const struct if_data *src_ifd,
    struct rt_addrinfo *info, struct walkarg *w, int len)
{
	struct if_msghdrl *ifm;
	struct if_data *ifd;

	ifm = (struct if_msghdrl *)w->w_tmem;

#ifdef COMPAT_FREEBSD32
	if (w->w_req->flags & SCTL_MASK32) {
		struct if_msghdrl32 *ifm32;

		ifm32 = (struct if_msghdrl32 *)ifm;
		ifm32->ifm_addrs = info->rti_addrs;
		ifm32->ifm_flags = ifp->if_flags | ifp->if_drv_flags;
		ifm32->ifm_index = ifp->if_index;
		ifm32->_ifm_spare1 = 0;
		ifm32->ifm_len = sizeof(*ifm32);
		ifm32->ifm_data_off = offsetof(struct if_msghdrl32, ifm_data);
		ifm32->_ifm_spare2 = 0;
		ifd = &ifm32->ifm_data;
	} else
#endif
	{
		ifm->ifm_addrs = info->rti_addrs;
		ifm->ifm_flags = ifp->if_flags | ifp->if_drv_flags;
		ifm->ifm_index = ifp->if_index;
		ifm->_ifm_spare1 = 0;
		ifm->ifm_len = sizeof(*ifm);
		ifm->ifm_data_off = offsetof(struct if_msghdrl, ifm_data);
		ifm->_ifm_spare2 = 0;
		ifd = &ifm->ifm_data;
	}

	memcpy(ifd, src_ifd, sizeof(*ifd));

	return (SYSCTL_OUT(w->w_req, (caddr_t)ifm, len));
}

static int
sysctl_iflist_ifm(struct ifnet *ifp, const struct if_data *src_ifd,
    struct rt_addrinfo *info, struct walkarg *w, int len)
{
	struct if_msghdr *ifm;
	struct if_data *ifd;

	ifm = (struct if_msghdr *)w->w_tmem;

#ifdef COMPAT_FREEBSD32
	if (w->w_req->flags & SCTL_MASK32) {
		struct if_msghdr32 *ifm32;

		ifm32 = (struct if_msghdr32 *)ifm;
		ifm32->ifm_addrs = info->rti_addrs;
		ifm32->ifm_flags = ifp->if_flags | ifp->if_drv_flags;
		ifm32->ifm_index = ifp->if_index;
		ifm32->_ifm_spare1 = 0;
		ifd = &ifm32->ifm_data;
	} else
#endif
	{
		ifm->ifm_addrs = info->rti_addrs;
		ifm->ifm_flags = ifp->if_flags | ifp->if_drv_flags;
		ifm->ifm_index = ifp->if_index;
		ifm->_ifm_spare1 = 0;
		ifd = &ifm->ifm_data;
	}

	memcpy(ifd, src_ifd, sizeof(*ifd));

	return (SYSCTL_OUT(w->w_req, (caddr_t)ifm, len));
}

static int
sysctl_iflist_ifaml(struct ifaddr *ifa, struct rt_addrinfo *info,
    struct walkarg *w, int len)
{
	struct ifa_msghdrl *ifam;
	struct if_data *ifd;

	ifam = (struct ifa_msghdrl *)w->w_tmem;

#ifdef COMPAT_FREEBSD32
	if (w->w_req->flags & SCTL_MASK32) {
		struct ifa_msghdrl32 *ifam32;

		ifam32 = (struct ifa_msghdrl32 *)ifam;
		ifam32->ifam_addrs = info->rti_addrs;
		ifam32->ifam_flags = ifa->ifa_flags;
		ifam32->ifam_index = ifa->ifa_ifp->if_index;
		ifam32->_ifam_spare1 = 0;
		ifam32->ifam_len = sizeof(*ifam32);
		ifam32->ifam_data_off =
		    offsetof(struct ifa_msghdrl32, ifam_data);
		ifam32->ifam_metric = ifa->ifa_ifp->if_metric;
		ifd = &ifam32->ifam_data;
	} else
#endif
	{
		ifam->ifam_addrs = info->rti_addrs;
		ifam->ifam_flags = ifa->ifa_flags;
		ifam->ifam_index = ifa->ifa_ifp->if_index;
		ifam->_ifam_spare1 = 0;
		ifam->ifam_len = sizeof(*ifam);
		ifam->ifam_data_off = offsetof(struct ifa_msghdrl, ifam_data);
		ifam->ifam_metric = ifa->ifa_ifp->if_metric;
		ifd = &ifam->ifam_data;
	}

	bzero(ifd, sizeof(*ifd));
	ifd->ifi_datalen = sizeof(struct if_data);
	ifd->ifi_ipackets = counter_u64_fetch(ifa->ifa_ipackets);
	ifd->ifi_opackets = counter_u64_fetch(ifa->ifa_opackets);
	ifd->ifi_ibytes = counter_u64_fetch(ifa->ifa_ibytes);
	ifd->ifi_obytes = counter_u64_fetch(ifa->ifa_obytes);

	/* Fixup if_data carp(4) vhid. */
	if (carp_get_vhid_p != NULL)
		ifd->ifi_vhid = (*carp_get_vhid_p)(ifa);

	return (SYSCTL_OUT(w->w_req, w->w_tmem, len));
}

static int
sysctl_iflist_ifam(struct ifaddr *ifa, struct rt_addrinfo *info,
    struct walkarg *w, int len)
{
	struct ifa_msghdr *ifam;

	ifam = (struct ifa_msghdr *)w->w_tmem;
	ifam->ifam_addrs = info->rti_addrs;
	ifam->ifam_flags = ifa->ifa_flags;
	ifam->ifam_index = ifa->ifa_ifp->if_index;
	ifam->_ifam_spare1 = 0;
	ifam->ifam_metric = ifa->ifa_ifp->if_metric;

	return (SYSCTL_OUT(w->w_req, w->w_tmem, len));
}

static int
sysctl_iflist(int af, struct walkarg *w)
{
	struct ifnet *ifp;
	struct ifaddr *ifa;
	struct if_data ifd;
	struct rt_addrinfo info;
	int len, error = 0;
	struct sockaddr_storage ss;

	bzero((caddr_t)&info, sizeof(info));
	bzero(&ifd, sizeof(ifd));
	CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		if (w->w_arg && w->w_arg != ifp->if_index)
			continue;
		if_data_copy(ifp, &ifd);
		ifa = ifp->if_addr;
		info.rti_info[RTAX_IFP] = ifa->ifa_addr;
		error = rtsock_msg_buffer(RTM_IFINFO, &info, w, &len);
		if (error != 0)
			goto done;
		info.rti_info[RTAX_IFP] = NULL;
		if (w->w_req && w->w_tmem) {
			if (w->w_op == NET_RT_IFLISTL)
				error = sysctl_iflist_ifml(ifp, &ifd, &info, w,
				    len);
			else
				error = sysctl_iflist_ifm(ifp, &ifd, &info, w,
				    len);
			if (error)
				goto done;
		}
		while ((ifa = CK_STAILQ_NEXT(ifa, ifa_link)) != NULL) {
			if (af && af != ifa->ifa_addr->sa_family)
				continue;
			if (prison_if(w->w_req->td->td_ucred,
			    ifa->ifa_addr) != 0)
				continue;
			info.rti_info[RTAX_IFA] = ifa->ifa_addr;
			info.rti_info[RTAX_NETMASK] = rtsock_fix_netmask(
			    ifa->ifa_addr, ifa->ifa_netmask, &ss);
			info.rti_info[RTAX_BRD] = ifa->ifa_dstaddr;
			error = rtsock_msg_buffer(RTM_NEWADDR, &info, w, &len);
			if (error != 0)
				goto done;
			if (w->w_req && w->w_tmem) {
				if (w->w_op == NET_RT_IFLISTL)
					error = sysctl_iflist_ifaml(ifa, &info,
					    w, len);
				else
					error = sysctl_iflist_ifam(ifa, &info,
					    w, len);
				if (error)
					goto done;
			}
		}
		info.rti_info[RTAX_IFA] = NULL;
		info.rti_info[RTAX_NETMASK] = NULL;
		info.rti_info[RTAX_BRD] = NULL;
	}
done:
	return (error);
}

static int
sysctl_ifmalist(int af, struct walkarg *w)
{
	struct rt_addrinfo info;
	struct ifaddr *ifa;
	struct ifmultiaddr *ifma;
	struct ifnet *ifp;
	int error, len;

	NET_EPOCH_ASSERT();

	error = 0;
	bzero((caddr_t)&info, sizeof(info));

	CK_STAILQ_FOREACH(ifp, &V_ifnet, if_link) {
		if (w->w_arg && w->w_arg != ifp->if_index)
			continue;
		ifa = ifp->if_addr;
		info.rti_info[RTAX_IFP] = ifa ? ifa->ifa_addr : NULL;
		CK_STAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (af && af != ifma->ifma_addr->sa_family)
				continue;
			if (prison_if(w->w_req->td->td_ucred,
			    ifma->ifma_addr) != 0)
				continue;
			info.rti_info[RTAX_IFA] = ifma->ifma_addr;
			info.rti_info[RTAX_GATEWAY] =
			    (ifma->ifma_addr->sa_family != AF_LINK) ?
			    ifma->ifma_lladdr : NULL;
			error = rtsock_msg_buffer(RTM_NEWMADDR, &info, w, &len);
			if (error != 0)
				break;
			if (w->w_req && w->w_tmem) {
				struct ifma_msghdr *ifmam;

				ifmam = (struct ifma_msghdr *)w->w_tmem;
				ifmam->ifmam_index = ifma->ifma_ifp->if_index;
				ifmam->ifmam_flags = 0;
				ifmam->ifmam_addrs = info.rti_addrs;
				ifmam->_ifmam_spare1 = 0;
				error = SYSCTL_OUT(w->w_req, w->w_tmem, len);
				if (error != 0)
					break;
			}
		}
		if (error != 0)
			break;
	}
	return (error);
}

static void
rtable_sysctl_dump(uint32_t fibnum, int family, struct walkarg *w)
{
	union sockaddr_union sa_dst, sa_mask;

	w->family = family;
	w->dst = (struct sockaddr *)&sa_dst;
	w->mask = (struct sockaddr *)&sa_mask;

	init_sockaddrs_family(family, w->dst, w->mask);

	rib_walk(fibnum, family, false, sysctl_dumpentry, w);
}

static int
sysctl_rtsock(SYSCTL_HANDLER_ARGS)
{
	struct epoch_tracker et;
	int	*name = (int *)arg1;
	u_int	namelen = arg2;
	struct rib_head *rnh = NULL; /* silence compiler. */
	int	i, lim, error = EINVAL;
	int	fib = 0;
	u_char	af;
	struct	walkarg w;

	if (namelen < 3)
		return (EINVAL);

	name++;
	namelen--;
	if (req->newptr)
		return (EPERM);
	if (name[1] == NET_RT_DUMP || name[1] == NET_RT_NHOP || name[1] == NET_RT_NHGRP) {
		if (namelen == 3)
			fib = req->td->td_proc->p_fibnum;
		else if (namelen == 4)
			fib = (name[3] == RT_ALL_FIBS) ?
			    req->td->td_proc->p_fibnum : name[3];
		else
			return ((namelen < 3) ? EISDIR : ENOTDIR);
		if (fib < 0 || fib >= rt_numfibs)
			return (EINVAL);
	} else if (namelen != 3)
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

	/*
	 * Allocate reply buffer in advance.
	 * All rtsock messages has maximum length of u_short.
	 */
	w.w_tmemsize = 65536;
	w.w_tmem = malloc(w.w_tmemsize, M_TEMP, M_WAITOK);

	NET_EPOCH_ENTER(et);
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
			rnh = rt_tables_get_rnh(fib, i);
			if (rnh != NULL) {
				rtable_sysctl_dump(fib, i, &w);
			} else if (af != 0)
				error = EAFNOSUPPORT;
		}
		break;
	case NET_RT_NHOP:
	case NET_RT_NHGRP:
		/* Allow dumping one specific af/fib at a time */
		if (namelen < 4) {
			error = EINVAL;
			break;
		}
		fib = name[3];
		if (fib < 0 || fib > rt_numfibs) {
			error = EINVAL;
			break;
		}
		rnh = rt_tables_get_rnh(fib, af);
		if (rnh == NULL) {
			error = EAFNOSUPPORT;
			break;
		}
		if (w.w_op == NET_RT_NHOP)
			error = nhops_dump_sysctl(rnh, w.w_req);
		else
#ifdef ROUTE_MPATH
			error = nhgrp_dump_sysctl(rnh, w.w_req);
#else
			error = ENOTSUP;
#endif
		break;
	case NET_RT_IFLIST:
	case NET_RT_IFLISTL:
		error = sysctl_iflist(af, &w);
		break;

	case NET_RT_IFMALIST:
		error = sysctl_ifmalist(af, &w);
		break;
	}
	NET_EPOCH_EXIT(et);

	free(w.w_tmem, M_TEMP);
	return (error);
}

static SYSCTL_NODE(_net, PF_ROUTE, routetable, CTLFLAG_RD | CTLFLAG_MPSAFE,
    sysctl_rtsock, "Return route tables and interface/address lists");

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
	.dom_name =		"route",
	.dom_protosw =		routesw,
	.dom_protoswNPROTOSW =	&routesw[nitems(routesw)]
};

VNET_DOMAIN_SET(route);
