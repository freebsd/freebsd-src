/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * Copyright (c) 2014, 2018 Andrey V. Elsukov <ae@FreeBSD.org>
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Heiko W.Rupp <hwr@pilhuhn.de>
 *
 * IPv6-over-GRE contributed by Gert Doering <gert@greenie.muc.de>
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $NetBSD: if_gre.c,v 1.49 2003/12/11 00:22:29 itojun Exp $
 */

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_rss.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/vnet.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#ifdef INET
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#ifdef RSS
#include <netinet/in_rss.h>
#endif
#endif

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#ifdef RSS
#include <netinet6/in6_rss.h>
#endif
#endif

#include <netinet/ip_encap.h>
#include <netinet/udp.h>
#include <net/bpf.h>
#include <net/if_gre.h>

#include <netlink/netlink.h>
#include <netlink/netlink_ctl.h>
#include <netlink/netlink_var.h>
#include <netlink/netlink_route.h>
#include <netlink/route/route_var.h>

#include <machine/in_cksum.h>
#include <security/mac/mac_framework.h>

#define	GREMTU			1476

static const char grename[] = "gre";
MALLOC_DEFINE(M_GRE, grename, "Generic Routing Encapsulation");

static struct sx gre_ioctl_sx;
SX_SYSINIT(gre_ioctl_sx, &gre_ioctl_sx, "gre_ioctl");
#define GRE_LOCK_ASSERT() sx_assert(&gre_ioctl_sx, SA_XLOCKED);

static int	gre_clone_create(struct if_clone *, char *, size_t,
		    struct ifc_data *, struct ifnet **);
static int	gre_clone_destroy(struct if_clone *, struct ifnet *,
		    uint32_t);
static int	gre_clone_create_nl(struct if_clone *, char *, size_t,
		    struct ifc_data_nl *);
static int	gre_clone_modify_nl(struct ifnet *, struct ifc_data_nl *);
static void	gre_clone_dump_nl(struct ifnet *, struct nl_writer *);
VNET_DEFINE_STATIC(struct if_clone *, gre_cloner);
#define	V_gre_cloner	VNET(gre_cloner)

#ifdef VIMAGE
static void	gre_reassign(struct ifnet *, struct vnet *, char *);
#endif
static void	gre_qflush(struct ifnet *);
static int	gre_transmit(struct ifnet *, struct mbuf *);
static int	gre_ioctl(struct ifnet *, u_long, caddr_t);
static int	gre_output(struct ifnet *, struct mbuf *,
		    const struct sockaddr *, struct route *);
static void	gre_delete_tunnel(struct gre_softc *);
static int	gre_set_addr_nl(struct gre_softc *, struct nl_pstate *,
		    struct sockaddr *, struct sockaddr *);

static int	gre_set_flags(struct gre_softc *, uint32_t);
static int	gre_set_key(struct gre_softc *, uint32_t);
static int	gre_set_udp_sport(struct gre_softc *, uint16_t);
static int	gre_setopts(struct gre_softc *, u_long, uint32_t);

static int	gre_set_flags_nl(struct gre_softc *, struct nl_pstate *, uint32_t);
static int	gre_set_key_nl(struct gre_softc *, struct nl_pstate *, uint32_t);
static int	gre_set_encap_nl(struct gre_softc *, struct nl_pstate *, uint32_t);
static int	gre_set_udp_sport_nl(struct gre_softc *, struct nl_pstate *, uint16_t);

SYSCTL_DECL(_net_link);
static SYSCTL_NODE(_net_link, IFT_TUNNEL, gre, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Generic Routing Encapsulation");
#ifndef MAX_GRE_NEST
/*
 * This macro controls the default upper limitation on nesting of gre tunnels.
 * Since, setting a large value to this macro with a careless configuration
 * may introduce system crash, we don't allow any nestings by default.
 * If you need to configure nested gre tunnels, you can define this macro
 * in your kernel configuration file.  However, if you do so, please be
 * careful to configure the tunnels so that it won't make a loop.
 */
#define MAX_GRE_NEST 1
#endif

VNET_DEFINE_STATIC(int, max_gre_nesting) = MAX_GRE_NEST;
#define	V_max_gre_nesting	VNET(max_gre_nesting)
SYSCTL_INT(_net_link_gre, OID_AUTO, max_nesting, CTLFLAG_RW | CTLFLAG_VNET,
    &VNET_NAME(max_gre_nesting), 0, "Max nested tunnels");

struct nl_parsed_gre {
	struct sockaddr		*ifla_local;
	struct sockaddr		*ifla_remote;
	uint32_t		ifla_flags;
	uint32_t		ifla_okey;
	uint32_t		ifla_encap_type;
	uint16_t		ifla_encap_sport;
};

#define _OUT(_field)	offsetof(struct nl_parsed_gre, _field)
static const struct nlattr_parser nla_p_gre[] = {
	{ .type = IFLA_GRE_LOCAL, .off = _OUT(ifla_local), .cb = nlattr_get_ip },
	{ .type = IFLA_GRE_REMOTE, .off = _OUT(ifla_remote), .cb = nlattr_get_ip },
	{ .type = IFLA_GRE_FLAGS, .off = _OUT(ifla_flags), .cb = nlattr_get_uint32 },
	{ .type = IFLA_GRE_OKEY, .off = _OUT(ifla_okey), .cb = nlattr_get_uint32 },
	{ .type = IFLA_GRE_ENCAP_TYPE, .off = _OUT(ifla_encap_type), .cb = nlattr_get_uint32 },
	{ .type = IFLA_GRE_ENCAP_SPORT, .off = _OUT(ifla_encap_sport), .cb = nlattr_get_uint16 },
};
#undef _OUT
NL_DECLARE_ATTR_PARSER(gre_modify_parser, nla_p_gre);

static const struct nlhdr_parser *all_parsers[] = {
	&gre_modify_parser,
};


static void
vnet_gre_init(const void *unused __unused)
{
	struct if_clone_addreq_v2 req = {
		.version = 2,
		.flags = IFC_F_AUTOUNIT,
		.match_f = NULL,
		.create_f = gre_clone_create,
		.destroy_f = gre_clone_destroy,
		.create_nl_f = gre_clone_create_nl,
		.modify_nl_f = gre_clone_modify_nl,
		.dump_nl_f = gre_clone_dump_nl,
	};
	V_gre_cloner = ifc_attach_cloner(grename, (struct if_clone_addreq *)&req);
#ifdef INET
	in_gre_init();
#endif
#ifdef INET6
	in6_gre_init();
#endif
}
VNET_SYSINIT(vnet_gre_init, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_gre_init, NULL);

static void
vnet_gre_uninit(const void *unused __unused)
{

	ifc_detach_cloner(V_gre_cloner);
#ifdef INET
	in_gre_uninit();
#endif
#ifdef INET6
	in6_gre_uninit();
#endif
	/* XXX: epoch_call drain */
}
VNET_SYSUNINIT(vnet_gre_uninit, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_gre_uninit, NULL);

static int
gre_clone_create_nl(struct if_clone *ifc, char *name, size_t len,
    struct ifc_data_nl *ifd)
{
	struct ifc_data ifd_new = {
		.flags = IFC_F_SYSSPACE,
		.unit = ifd->unit,
	};

	return (gre_clone_create(ifc, name, len, &ifd_new, &ifd->ifp));
}

static int
gre_clone_modify_nl(struct ifnet *ifp, struct ifc_data_nl *ifd)
{
	struct gre_softc *sc = ifp->if_softc;
	struct nl_parsed_link *lattrs = ifd->lattrs;
	struct nl_pstate *npt = ifd->npt;
	struct nl_parsed_gre params;
	struct nlattr *attrs = lattrs->ifla_idata;
	struct nlattr_bmask bm;
	int error = 0;

	if ((attrs == NULL) ||
	    (nl_has_attr(ifd->bm, IFLA_LINKINFO) == 0)) {
		error = nl_modify_ifp_generic(ifp, lattrs, ifd->bm, npt);
		return (error);
	}

	error = priv_check(curthread, PRIV_NET_GRE);
	if (error)
		return (error);

	/* make sure ignored attributes by nl_parse will not cause panics */
	memset(&params, 0, sizeof(params));

	nl_get_attrs_bmask_raw(NLA_DATA(attrs), NLA_DATA_LEN(attrs), &bm);
	if ((error = nl_parse_nested(attrs, &gre_modify_parser, npt, &params)) != 0)
		return (error);

	if (nl_has_attr(&bm, IFLA_GRE_LOCAL) && nl_has_attr(&bm, IFLA_GRE_REMOTE))
		error = gre_set_addr_nl(sc, npt, params.ifla_local, params.ifla_remote);
	else if (nl_has_attr(&bm, IFLA_GRE_LOCAL) || nl_has_attr(&bm, IFLA_GRE_REMOTE)) {
		error = EINVAL;
		nlmsg_report_err_msg(npt, "Specify both remote and local address together");
	}

	if (error == 0 && nl_has_attr(&bm, IFLA_GRE_FLAGS))
		error = gre_set_flags_nl(sc, npt, params.ifla_flags);

	if (error == 0 && nl_has_attr(&bm, IFLA_GRE_OKEY))
		error = gre_set_key_nl(sc, npt, params.ifla_okey);

	if (error == 0 && nl_has_attr(&bm, IFLA_GRE_ENCAP_TYPE))
		error = gre_set_encap_nl(sc, npt, params.ifla_encap_type);

	if (error == 0 && nl_has_attr(&bm, IFLA_GRE_ENCAP_SPORT))
		error = gre_set_udp_sport_nl(sc, npt, params.ifla_encap_sport);

	if (error == 0)
		error = nl_modify_ifp_generic(ifp, ifd->lattrs, ifd->bm, ifd->npt);

	return (error);
}

static void
gre_clone_dump_nl(struct ifnet *ifp, struct nl_writer *nw)
{
	GRE_RLOCK_TRACKER;
	struct gre_softc *sc;
	struct ifreq ifr;

	nlattr_add_u32(nw, IFLA_LINK, ifp->if_index);
	nlattr_add_string(nw, IFLA_IFNAME, ifp->if_xname);

	int off = nlattr_add_nested(nw, IFLA_LINKINFO);
	if (off == 0)
		return;

	nlattr_add_string(nw, IFLA_INFO_KIND, "gre");
	int off2 = nlattr_add_nested(nw, IFLA_INFO_DATA);
	if (off2 == 0) {
		nlattr_set_len(nw, off);
		return;
	}

	sc = ifp->if_softc;
	GRE_RLOCK();

	if (sc->gre_family == AF_INET) {
#ifdef INET
		if (in_gre_ioctl(sc, SIOCGIFPSRCADDR, (caddr_t)&ifr) == 0)
			nlattr_add_in_addr(nw, IFLA_GRE_LOCAL,
			    (const struct in_addr *)&ifr.ifr_addr);
		if (in_gre_ioctl(sc, SIOCGIFPDSTADDR, (caddr_t)&ifr) == 0)
			nlattr_add_in_addr(nw, IFLA_GRE_LOCAL,
			    (const struct in_addr *)&ifr.ifr_dstaddr);
#endif
	} else if (sc->gre_family == AF_INET6) {
#ifdef INET6
		if (in6_gre_ioctl(sc, SIOCGIFPSRCADDR_IN6, (caddr_t)&ifr) == 0)
			nlattr_add_in6_addr(nw, IFLA_GRE_LOCAL,
			    (const struct in6_addr *)&ifr.ifr_addr);
		if (in6_gre_ioctl(sc, SIOCGIFPDSTADDR_IN6, (caddr_t)&ifr) == 0)
			nlattr_add_in6_addr(nw, IFLA_GRE_LOCAL,
			    (const struct in6_addr *)&ifr.ifr_dstaddr);
#endif
	}

	nlattr_add_u32(nw, IFLA_GRE_FLAGS, sc->gre_options);
	nlattr_add_u32(nw, IFLA_GRE_OKEY, sc->gre_key);
	nlattr_add_u32(nw, IFLA_GRE_ENCAP_TYPE,
	    sc->gre_options & GRE_UDPENCAP ? IFLA_TUNNEL_GRE_UDP : IFLA_TUNNEL_NONE);
	nlattr_add_u16(nw, IFLA_GRE_ENCAP_SPORT, sc->gre_port);

	nlattr_set_len(nw, off2);
	nlattr_set_len(nw, off);

	GRE_RUNLOCK();
}

static int
gre_clone_create(struct if_clone *ifc, char *name, size_t len,
    struct ifc_data *ifd, struct ifnet **ifpp)
{
	struct gre_softc *sc;

	sc = malloc(sizeof(struct gre_softc), M_GRE, M_WAITOK | M_ZERO);
	sc->gre_fibnum = curthread->td_proc->p_fibnum;
	GRE2IFP(sc) = if_alloc(IFT_TUNNEL);
	GRE2IFP(sc)->if_softc = sc;
	if_initname(GRE2IFP(sc), grename, ifd->unit);

	GRE2IFP(sc)->if_mtu = GREMTU;
	GRE2IFP(sc)->if_flags = IFF_POINTOPOINT|IFF_MULTICAST;
	GRE2IFP(sc)->if_output = gre_output;
	GRE2IFP(sc)->if_ioctl = gre_ioctl;
	GRE2IFP(sc)->if_transmit = gre_transmit;
	GRE2IFP(sc)->if_qflush = gre_qflush;
#ifdef VIMAGE
	GRE2IFP(sc)->if_reassign = gre_reassign;
#endif
	GRE2IFP(sc)->if_capabilities |= IFCAP_LINKSTATE;
	GRE2IFP(sc)->if_capenable |= IFCAP_LINKSTATE;
	if_attach(GRE2IFP(sc));
	bpfattach(GRE2IFP(sc), DLT_NULL, sizeof(u_int32_t));
	*ifpp = GRE2IFP(sc);

	return (0);
}

#ifdef VIMAGE
static void
gre_reassign(struct ifnet *ifp, struct vnet *new_vnet __unused,
    char *unused __unused)
{
	struct gre_softc *sc;

	sx_xlock(&gre_ioctl_sx);
	sc = ifp->if_softc;
	if (sc != NULL)
		gre_delete_tunnel(sc);
	sx_xunlock(&gre_ioctl_sx);
}
#endif /* VIMAGE */

static int
gre_clone_destroy(struct if_clone *ifc, struct ifnet *ifp, uint32_t flags)
{
	struct gre_softc *sc;

	sx_xlock(&gre_ioctl_sx);
	sc = ifp->if_softc;
	gre_delete_tunnel(sc);
	bpfdetach(ifp);
	if_detach(ifp);
	ifp->if_softc = NULL;
	sx_xunlock(&gre_ioctl_sx);

	GRE_WAIT();
	if_free(ifp);
	free(sc, M_GRE);

	return (0);
}

static int
gre_set_key(struct gre_softc *sc, uint32_t key)
{
	int error = 0;

	GRE_LOCK_ASSERT();

	if (sc->gre_key == key)
		return (0);
	error = gre_setopts(sc, GRESKEY, key);

	return (error);
}

static int
gre_set_flags(struct gre_softc *sc, uint32_t opt)
{
	int error = 0;

	GRE_LOCK_ASSERT();

	if (opt & ~GRE_OPTMASK)
		return (EINVAL);
	if (sc->gre_options == opt)
		return (0);
	error = gre_setopts(sc, GRESOPTS, opt);

	return (error);
}

static int
gre_set_udp_sport(struct gre_softc *sc, uint16_t port)
{
	int error = 0;

	GRE_LOCK_ASSERT();

	if (port != 0 && (port < V_ipport_hifirstauto ||
	    port > V_ipport_hilastauto))
		return (EINVAL);
	if (sc->gre_port == port)
		return (0);
	if ((sc->gre_options & GRE_UDPENCAP) == 0) {
		/*
		 * UDP encapsulation is not enabled, thus
		 * there is no need to reattach softc.
		 */
		sc->gre_port = port;
		return (0);
	}
	error = gre_setopts(sc, GRESPORT, port);

	return (error);
}

static int
gre_setopts(struct gre_softc *sc, u_long cmd, uint32_t opt)
{
	int error = 0;

	GRE_LOCK_ASSERT();

	switch (sc->gre_family) {
#ifdef INET
	case AF_INET:
		error = in_gre_setopts(sc, cmd, opt);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		error = in6_gre_setopts(sc, cmd, opt);
		break;
#endif
	default:
		/*
		 * Tunnel is not yet configured.
		 * We can just change any parameters.
		 */
		if (cmd == GRESKEY)
			sc->gre_key = opt;
		if (cmd == GRESOPTS)
			sc->gre_options = opt;
		if (cmd == GRESPORT)
			sc->gre_port = opt;
		break;
	}
	/*
	 * XXX: Do we need to initiate change of interface
	 * state here?
	 */
	return (error);
};

static int
gre_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	struct gre_softc *sc;
	uint32_t opt;
	int error;

	switch (cmd) {
	case SIOCSIFMTU:
		 /* XXX: */
		if (ifr->ifr_mtu < 576)
			return (EINVAL);
		ifp->if_mtu = ifr->ifr_mtu;
		return (0);
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
	case SIOCSIFFLAGS:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		return (0);
	case GRESADDRS:
	case GRESADDRD:
	case GREGADDRS:
	case GREGADDRD:
	case GRESPROTO:
	case GREGPROTO:
		return (EOPNOTSUPP);
	}
	sx_xlock(&gre_ioctl_sx);
	sc = ifp->if_softc;
	if (sc == NULL) {
		error = ENXIO;
		goto end;
	}
	error = 0;
	switch (cmd) {
	case SIOCDIFPHYADDR:
		if (sc->gre_family == 0)
			break;
		gre_delete_tunnel(sc);
		break;
#ifdef INET
	case SIOCSIFPHYADDR:
	case SIOCGIFPSRCADDR:
	case SIOCGIFPDSTADDR:
		error = in_gre_ioctl(sc, cmd, data);
		break;
#endif
#ifdef INET6
	case SIOCSIFPHYADDR_IN6:
	case SIOCGIFPSRCADDR_IN6:
	case SIOCGIFPDSTADDR_IN6:
		error = in6_gre_ioctl(sc, cmd, data);
		break;
#endif
	case SIOCGTUNFIB:
		ifr->ifr_fib = sc->gre_fibnum;
		break;
	case SIOCSTUNFIB:
		if ((error = priv_check(curthread, PRIV_NET_GRE)) != 0)
			break;
		if (ifr->ifr_fib >= rt_numfibs)
			error = EINVAL;
		else
			sc->gre_fibnum = ifr->ifr_fib;
		break;
	case GRESKEY:
	case GRESOPTS:
	case GRESPORT:
		if ((error = priv_check(curthread, PRIV_NET_GRE)) != 0)
			break;
		if ((error = copyin(ifr_data_get_ptr(ifr), &opt,
		    sizeof(opt))) != 0)
			break;
		if (cmd == GRESKEY)
			error = gre_set_key(sc, opt);
		else if (cmd == GRESOPTS)
			error = gre_set_flags(sc, opt);
		else if (cmd == GRESPORT)
			error = gre_set_udp_sport(sc, opt);
		break;
	case GREGKEY:
		error = copyout(&sc->gre_key, ifr_data_get_ptr(ifr),
		    sizeof(sc->gre_key));
		break;
	case GREGOPTS:
		error = copyout(&sc->gre_options, ifr_data_get_ptr(ifr),
		    sizeof(sc->gre_options));
		break;
	case GREGPORT:
		error = copyout(&sc->gre_port, ifr_data_get_ptr(ifr),
		    sizeof(sc->gre_port));
		break;
	default:
		error = EINVAL;
		break;
	}
	if (error == 0 && sc->gre_family != 0) {
		if (
#ifdef INET
		    cmd == SIOCSIFPHYADDR ||
#endif
#ifdef INET6
		    cmd == SIOCSIFPHYADDR_IN6 ||
#endif
		    0) {
			if_link_state_change(ifp, LINK_STATE_UP);
		}
	}
end:
	sx_xunlock(&gre_ioctl_sx);
	return (error);
}

static void
gre_delete_tunnel(struct gre_softc *sc)
{
	struct gre_socket *gs;

	sx_assert(&gre_ioctl_sx, SA_XLOCKED);
	if (sc->gre_family != 0) {
		CK_LIST_REMOVE(sc, chain);
		CK_LIST_REMOVE(sc, srchash);
		GRE_WAIT();
		free(sc->gre_hdr, M_GRE);
		sc->gre_family = 0;
	}
	/*
	 * If this Tunnel was the last one that could use UDP socket,
	 * we should unlink socket from hash table and close it.
	 */
	if ((gs = sc->gre_so) != NULL && CK_LIST_EMPTY(&gs->list)) {
		CK_LIST_REMOVE(gs, chain);
		soclose(gs->so);
		NET_EPOCH_CALL(gre_sofree, &gs->epoch_ctx);
		sc->gre_so = NULL;
	}
	GRE2IFP(sc)->if_drv_flags &= ~IFF_DRV_RUNNING;
	if_link_state_change(GRE2IFP(sc), LINK_STATE_DOWN);
}

struct gre_list *
gre_hashinit(void)
{
	struct gre_list *hash;
	int i;

	hash = malloc(sizeof(struct gre_list) * GRE_HASH_SIZE,
	    M_GRE, M_WAITOK);
	for (i = 0; i < GRE_HASH_SIZE; i++)
		CK_LIST_INIT(&hash[i]);

	return (hash);
}

void
gre_hashdestroy(struct gre_list *hash)
{

	free(hash, M_GRE);
}

void
gre_sofree(epoch_context_t ctx)
{
	struct gre_socket *gs;

	gs = __containerof(ctx, struct gre_socket, epoch_ctx);
	free(gs, M_GRE);
}

static __inline uint16_t
gre_cksum_add(uint16_t sum, uint16_t a)
{
	uint16_t res;

	res = sum + a;
	return (res + (res < a));
}

void
gre_update_udphdr(struct gre_softc *sc, struct udphdr *udp, uint16_t csum)
{

	sx_assert(&gre_ioctl_sx, SA_XLOCKED);
	MPASS(sc->gre_options & GRE_UDPENCAP);

	udp->uh_dport = htons(GRE_UDPPORT);
	udp->uh_sport = htons(sc->gre_port);
	udp->uh_sum = csum;
	udp->uh_ulen = 0;
}

void
gre_update_hdr(struct gre_softc *sc, struct grehdr *gh)
{
	uint32_t *opts;
	uint16_t flags;

	sx_assert(&gre_ioctl_sx, SA_XLOCKED);

	flags = 0;
	opts = gh->gre_opts;
	if (sc->gre_options & GRE_ENABLE_CSUM) {
		flags |= GRE_FLAGS_CP;
		sc->gre_hlen += 2 * sizeof(uint16_t);
		*opts++ = 0;
	}
	if (sc->gre_key != 0) {
		flags |= GRE_FLAGS_KP;
		sc->gre_hlen += sizeof(uint32_t);
		*opts++ = htonl(sc->gre_key);
	}
	if (sc->gre_options & GRE_ENABLE_SEQ) {
		flags |= GRE_FLAGS_SP;
		sc->gre_hlen += sizeof(uint32_t);
		*opts++ = 0;
	} else
		sc->gre_oseq = 0;
	gh->gre_flags = htons(flags);
}

int
gre_input(struct mbuf *m, int off, int proto, void *arg)
{
	struct gre_softc *sc = arg;
	struct grehdr *gh;
	struct ifnet *ifp;
	uint32_t *opts;
#ifdef notyet
	uint32_t key;
#endif
	uint16_t flags;
	int hlen, isr, af;

	ifp = GRE2IFP(sc);
	hlen = off + sizeof(struct grehdr) + 4 * sizeof(uint32_t);
	if (m->m_pkthdr.len < hlen)
		goto drop;
	if (m->m_len < hlen) {
		m = m_pullup(m, hlen);
		if (m == NULL)
			goto drop;
	}
	gh = (struct grehdr *)mtodo(m, off);
	flags = ntohs(gh->gre_flags);
	if (flags & ~GRE_FLAGS_MASK)
		goto drop;
	opts = gh->gre_opts;
	hlen = 2 * sizeof(uint16_t);
	if (flags & GRE_FLAGS_CP) {
		/* reserved1 field must be zero */
		if (((uint16_t *)opts)[1] != 0)
			goto drop;
		if (in_cksum_skip(m, m->m_pkthdr.len, off) != 0)
			goto drop;
		hlen += 2 * sizeof(uint16_t);
		opts++;
	}
	if (flags & GRE_FLAGS_KP) {
#ifdef notyet
        /*
         * XXX: The current implementation uses the key only for outgoing
         * packets. But we can check the key value here, or even in the
         * encapcheck function.
         */
		key = ntohl(*opts);
#endif
		hlen += sizeof(uint32_t);
		opts++;
    }
#ifdef notyet
	} else
		key = 0;

	if (sc->gre_key != 0 && (key != sc->gre_key || key != 0))
		goto drop;
#endif
	if (flags & GRE_FLAGS_SP) {
#ifdef notyet
		seq = ntohl(*opts);
#endif
		hlen += sizeof(uint32_t);
	}
	switch (ntohs(gh->gre_proto)) {
	case ETHERTYPE_WCCP:
		/*
		 * For WCCP skip an additional 4 bytes if after GRE header
		 * doesn't follow an IP header.
		 */
		if (flags == 0 && (*(uint8_t *)gh->gre_opts & 0xF0) != 0x40)
			hlen += sizeof(uint32_t);
		/* FALLTHROUGH */
	case ETHERTYPE_IP:
		isr = NETISR_IP;
		af = AF_INET;
		break;
	case ETHERTYPE_IPV6:
		isr = NETISR_IPV6;
		af = AF_INET6;
		break;
	default:
		goto drop;
	}
	m_adj(m, off + hlen);
	m_clrprotoflags(m);
	m->m_pkthdr.rcvif = ifp;
	M_SETFIB(m, ifp->if_fib);
#ifdef MAC
	mac_ifnet_create_mbuf(ifp, m);
#endif
	BPF_MTAP2(ifp, &af, sizeof(af), m);
	if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);
	if_inc_counter(ifp, IFCOUNTER_IBYTES, m->m_pkthdr.len);
	if ((ifp->if_flags & IFF_MONITOR) != 0)
		m_freem(m);
	else
		netisr_dispatch(isr, m);
	return (IPPROTO_DONE);
drop:
	if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
	m_freem(m);
	return (IPPROTO_DONE);
}

static int
gre_output(struct ifnet *ifp, struct mbuf *m, const struct sockaddr *dst,
   struct route *ro)
{
	uint32_t af;

	/* BPF writes need to be handled specially. */
	if (dst->sa_family == AF_UNSPEC || dst->sa_family == pseudo_AF_HDRCMPLT)
		bcopy(dst->sa_data, &af, sizeof(af));
	else
		af = RO_GET_FAMILY(ro, dst);
	/*
	 * Now save the af in the inbound pkt csum data, this is a cheat since
	 * we are using the inbound csum_data field to carry the af over to
	 * the gre_transmit() routine, avoiding using yet another mtag.
	 */
	m->m_pkthdr.csum_data = af;
	return (ifp->if_transmit(ifp, m));
}

static void
gre_setseqn(struct grehdr *gh, uint32_t seq)
{
	uint32_t *opts;
	uint16_t flags;

	opts = gh->gre_opts;
	flags = ntohs(gh->gre_flags);
	KASSERT((flags & GRE_FLAGS_SP) != 0,
	    ("gre_setseqn called, but GRE_FLAGS_SP isn't set "));
	if (flags & GRE_FLAGS_CP)
		opts++;
	if (flags & GRE_FLAGS_KP)
		opts++;
	*opts = htonl(seq);
}

static uint32_t
gre_flowid(struct gre_softc *sc, struct mbuf *m, uint32_t af)
{
	uint32_t flowid = 0;

	if ((sc->gre_options & GRE_UDPENCAP) == 0 || sc->gre_port != 0)
		return (flowid);
	switch (af) {
#ifdef INET
	case AF_INET:
#ifdef RSS
		flowid = rss_hash_ip4_2tuple(mtod(m, struct ip *)->ip_src,
		    mtod(m, struct ip *)->ip_dst);
		break;
#endif
		flowid = mtod(m, struct ip *)->ip_src.s_addr ^
		    mtod(m, struct ip *)->ip_dst.s_addr;
		break;
#endif
#ifdef INET6
	case AF_INET6:
#ifdef RSS
		flowid = rss_hash_ip6_2tuple(
		    &mtod(m, struct ip6_hdr *)->ip6_src,
		    &mtod(m, struct ip6_hdr *)->ip6_dst);
		break;
#endif
		flowid = mtod(m, struct ip6_hdr *)->ip6_src.s6_addr32[3] ^
		    mtod(m, struct ip6_hdr *)->ip6_dst.s6_addr32[3];
		break;
#endif
	default:
		break;
	}
	return (flowid);
}

#define	MTAG_GRE	1307983903
static int
gre_transmit(struct ifnet *ifp, struct mbuf *m)
{
	GRE_RLOCK_TRACKER;
	struct gre_softc *sc;
	struct grehdr *gh;
	struct udphdr *uh;
	uint32_t af, flowid;
	int error, len;
	uint16_t proto;

	len = 0;
	GRE_RLOCK();
#ifdef MAC
	error = mac_ifnet_check_transmit(ifp, m);
	if (error) {
		m_freem(m);
		goto drop;
	}
#endif
	error = ENETDOWN;
	sc = ifp->if_softc;
	if ((ifp->if_flags & IFF_MONITOR) != 0 ||
	    (ifp->if_flags & IFF_UP) == 0 ||
	    (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0 ||
	    sc->gre_family == 0 ||
	    (error = if_tunnel_check_nesting(ifp, m, MTAG_GRE,
		V_max_gre_nesting)) != 0) {
		m_freem(m);
		goto drop;
	}
	af = m->m_pkthdr.csum_data;
	BPF_MTAP2(ifp, &af, sizeof(af), m);
	m->m_flags &= ~(M_BCAST|M_MCAST);
	flowid = gre_flowid(sc, m, af);
	M_SETFIB(m, sc->gre_fibnum);
	M_PREPEND(m, sc->gre_hlen, M_NOWAIT);
	if (m == NULL) {
		error = ENOBUFS;
		goto drop;
	}
	bcopy(sc->gre_hdr, mtod(m, void *), sc->gre_hlen);
	/* Determine GRE proto */
	switch (af) {
#ifdef INET
	case AF_INET:
		proto = htons(ETHERTYPE_IP);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		proto = htons(ETHERTYPE_IPV6);
		break;
#endif
	default:
		m_freem(m);
		error = ENETDOWN;
		goto drop;
	}
	/* Determine offset of GRE header */
	switch (sc->gre_family) {
#ifdef INET
	case AF_INET:
		len = sizeof(struct ip);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		len = sizeof(struct ip6_hdr);
		break;
#endif
	default:
		m_freem(m);
		error = ENETDOWN;
		goto drop;
	}
	if (sc->gre_options & GRE_UDPENCAP) {
		uh = (struct udphdr *)mtodo(m, len);
		uh->uh_sport |= htons(V_ipport_hifirstauto) |
		    (flowid >> 16) | (flowid & 0xFFFF);
		uh->uh_sport = htons(ntohs(uh->uh_sport) %
		    V_ipport_hilastauto);
		uh->uh_ulen = htons(m->m_pkthdr.len - len);
		uh->uh_sum = gre_cksum_add(uh->uh_sum,
		    htons(m->m_pkthdr.len - len + IPPROTO_UDP));
		m->m_pkthdr.csum_flags = sc->gre_csumflags;
		m->m_pkthdr.csum_data = offsetof(struct udphdr, uh_sum);
		len += sizeof(struct udphdr);
	}
	gh = (struct grehdr *)mtodo(m, len);
	gh->gre_proto = proto;
	if (sc->gre_options & GRE_ENABLE_SEQ)
		gre_setseqn(gh, sc->gre_oseq++);
	if (sc->gre_options & GRE_ENABLE_CSUM) {
		*(uint16_t *)gh->gre_opts = in_cksum_skip(m,
		    m->m_pkthdr.len, len);
	}
	len = m->m_pkthdr.len - len;
	switch (sc->gre_family) {
#ifdef INET
	case AF_INET:
		error = in_gre_output(m, af, sc->gre_hlen);
		break;
#endif
#ifdef INET6
	case AF_INET6:
		error = in6_gre_output(m, af, sc->gre_hlen, flowid);
		break;
#endif
	default:
		m_freem(m);
		error = ENETDOWN;
	}
drop:
	if (error)
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
	else {
		if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
		if_inc_counter(ifp, IFCOUNTER_OBYTES, len);
	}
	GRE_RUNLOCK();
	return (error);
}

static void
gre_qflush(struct ifnet *ifp __unused)
{

}

static int
gre_set_addr_nl(struct gre_softc *sc, struct nl_pstate *npt,
    struct sockaddr *src, struct sockaddr *dst)
{
#if defined(INET) || defined(INET6)
	union {
#ifdef INET
		struct in_aliasreq in;
#endif
#ifdef INET6
		struct in6_aliasreq in6;
#endif
	} aliasreq;
#endif
	int error;

	/* XXX: this sanity check runs again in in[6]_gre_ioctl */
	if (src->sa_family != dst->sa_family)
		error = EADDRNOTAVAIL;
#ifdef INET
	else if (src->sa_family == AF_INET) {
		memcpy(&aliasreq.in.ifra_addr, src, sizeof(struct sockaddr_in));
		memcpy(&aliasreq.in.ifra_dstaddr, dst, sizeof(struct sockaddr_in));
		sx_xlock(&gre_ioctl_sx);
		error = in_gre_ioctl(sc, SIOCSIFPHYADDR, (caddr_t)&aliasreq.in);
		sx_xunlock(&gre_ioctl_sx);
	}
#endif
#ifdef INET6
	else if (src->sa_family == AF_INET6) {
		memcpy(&aliasreq.in6.ifra_addr, src, sizeof(struct sockaddr_in6));
		memcpy(&aliasreq.in6.ifra_dstaddr, dst, sizeof(struct sockaddr_in6));
		sx_xlock(&gre_ioctl_sx);
		error = in6_gre_ioctl(sc, SIOCSIFPHYADDR_IN6, (caddr_t)&aliasreq.in6);
		sx_xunlock(&gre_ioctl_sx);
	}
#endif
	else
		error = EAFNOSUPPORT;

	if (error == EADDRNOTAVAIL)
		nlmsg_report_err_msg(npt, "address is invalid");
	if (error == EEXIST)
		nlmsg_report_err_msg(npt, "remote and local addresses are the same");
	if (error == EAFNOSUPPORT)
		nlmsg_report_err_msg(npt, "address family is not supported");

	return (error);
}

static int
gre_set_flags_nl(struct gre_softc *sc, struct nl_pstate *npt, uint32_t opt)
{
	int error = 0;

	sx_xlock(&gre_ioctl_sx);
	error = gre_set_flags(sc, opt);
	sx_xunlock(&gre_ioctl_sx);

	if (error == EINVAL)
		nlmsg_report_err_msg(npt, "gre flags are invalid");

	return (error);
}

static int
gre_set_key_nl(struct gre_softc *sc, struct nl_pstate *npt, uint32_t key)
{
	int error = 0;

	sx_xlock(&gre_ioctl_sx);
	error = gre_set_key(sc, key);
	sx_xunlock(&gre_ioctl_sx);

	if (error == EINVAL)
		nlmsg_report_err_msg(npt, "gre key is invalid: %u", key);

	return (error);
}

static int
gre_set_encap_nl(struct gre_softc *sc, struct nl_pstate *npt, uint32_t type)
{
	uint32_t opt;
	int error = 0;

	sx_xlock(&gre_ioctl_sx);
	opt = sc->gre_options;
	if (type & IFLA_TUNNEL_GRE_UDP)
		opt |= GRE_UDPENCAP;
	else
		opt &= ~GRE_UDPENCAP;
	error = gre_set_flags(sc, opt);
	sx_xunlock(&gre_ioctl_sx);

	if (error == EEXIST)
		nlmsg_report_err_msg(npt, "same gre tunnel exist");

	return (error);
}


static int
gre_set_udp_sport_nl(struct gre_softc *sc, struct nl_pstate *npt, uint16_t port)
{
	int error = 0;

	sx_xlock(&gre_ioctl_sx);
	error = gre_set_udp_sport(sc, port);
	sx_xunlock(&gre_ioctl_sx);

	if (error == EINVAL)
		nlmsg_report_err_msg(npt, "source port is invalid: %u", port);

	return (error);
}


static int
gremodevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		NL_VERIFY_PARSERS(all_parsers);
		break;
	case MOD_UNLOAD:
		break;
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t gre_mod = {
	"if_gre",
	gremodevent,
	0
};

DECLARE_MODULE(if_gre, gre_mod, SI_SUB_PSEUDO, SI_ORDER_ANY);
MODULE_VERSION(if_gre, 1);
