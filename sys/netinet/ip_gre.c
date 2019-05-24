/*-
 * SPDX-License-Identifier: BSD-2-Clause-NetBSD
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
 * $NetBSD: ip_gre.c,v 1.29 2003/09/05 23:02:43 itojun Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_encap.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif

#include <net/if_gre.h>
#include <machine/in_cksum.h>

#define	GRE_TTL			30
VNET_DEFINE(int, ip_gre_ttl) = GRE_TTL;
#define	V_ip_gre_ttl		VNET(ip_gre_ttl)
SYSCTL_INT(_net_inet_ip, OID_AUTO, grettl, CTLFLAG_VNET | CTLFLAG_RW,
    &VNET_NAME(ip_gre_ttl), 0, "Default TTL value for encapsulated packets");

struct in_gre_socket {
	struct gre_socket		base;
	in_addr_t			addr;
};
VNET_DEFINE_STATIC(struct gre_sockets *, ipv4_sockets) = NULL;
VNET_DEFINE_STATIC(struct gre_list *, ipv4_hashtbl) = NULL;
VNET_DEFINE_STATIC(struct gre_list *, ipv4_srchashtbl) = NULL;
#define	V_ipv4_sockets		VNET(ipv4_sockets)
#define	V_ipv4_hashtbl		VNET(ipv4_hashtbl)
#define	V_ipv4_srchashtbl	VNET(ipv4_srchashtbl)
#define	GRE_HASH(src, dst)	(V_ipv4_hashtbl[\
    in_gre_hashval((src), (dst)) & (GRE_HASH_SIZE - 1)])
#define	GRE_SRCHASH(src)	(V_ipv4_srchashtbl[\
    fnv_32_buf(&(src), sizeof(src), FNV1_32_INIT) & (GRE_HASH_SIZE - 1)])
#define	GRE_SOCKHASH(src)	(V_ipv4_sockets[\
    fnv_32_buf(&(src), sizeof(src), FNV1_32_INIT) & (GRE_HASH_SIZE - 1)])
#define	GRE_HASH_SC(sc)		GRE_HASH((sc)->gre_oip.ip_src.s_addr,\
    (sc)->gre_oip.ip_dst.s_addr)

static uint32_t
in_gre_hashval(in_addr_t src, in_addr_t dst)
{
	uint32_t ret;

	ret = fnv_32_buf(&src, sizeof(src), FNV1_32_INIT);
	return (fnv_32_buf(&dst, sizeof(dst), ret));
}

static struct gre_socket*
in_gre_lookup_socket(in_addr_t addr)
{
	struct gre_socket *gs;
	struct in_gre_socket *s;

	CK_LIST_FOREACH(gs, &GRE_SOCKHASH(addr), chain) {
		s = __containerof(gs, struct in_gre_socket, base);
		if (s->addr == addr)
			break;
	}
	return (gs);
}

static int
in_gre_checkdup(const struct gre_softc *sc, in_addr_t src, in_addr_t dst,
    uint32_t opts)
{
	struct gre_list *head;
	struct gre_softc *tmp;
	struct gre_socket *gs;

	if (sc->gre_family == AF_INET &&
	    sc->gre_oip.ip_src.s_addr == src &&
	    sc->gre_oip.ip_dst.s_addr == dst &&
	    (sc->gre_options & GRE_UDPENCAP) == (opts & GRE_UDPENCAP))
		return (EEXIST);

	if (opts & GRE_UDPENCAP) {
		gs = in_gre_lookup_socket(src);
		if (gs == NULL)
			return (0);
		head = &gs->list;
	} else
		head = &GRE_HASH(src, dst);

	CK_LIST_FOREACH(tmp, head, chain) {
		if (tmp == sc)
			continue;
		if (tmp->gre_oip.ip_src.s_addr == src &&
		    tmp->gre_oip.ip_dst.s_addr == dst)
			return (EADDRNOTAVAIL);
	}
	return (0);
}

static int
in_gre_lookup(const struct mbuf *m, int off, int proto, void **arg)
{
	const struct ip *ip;
	struct gre_softc *sc;

	if (V_ipv4_hashtbl == NULL)
		return (0);

	MPASS(in_epoch(net_epoch_preempt));
	ip = mtod(m, const struct ip *);
	CK_LIST_FOREACH(sc, &GRE_HASH(ip->ip_dst.s_addr,
	    ip->ip_src.s_addr), chain) {
		/*
		 * This is an inbound packet, its ip_dst is source address
		 * in softc.
		 */
		if (sc->gre_oip.ip_src.s_addr == ip->ip_dst.s_addr &&
		    sc->gre_oip.ip_dst.s_addr == ip->ip_src.s_addr) {
			if ((GRE2IFP(sc)->if_flags & IFF_UP) == 0)
				return (0);
			*arg = sc;
			return (ENCAP_DRV_LOOKUP);
		}
	}
	return (0);
}

/*
 * Check that ingress address belongs to local host.
 */
static void
in_gre_set_running(struct gre_softc *sc)
{

	if (in_localip(sc->gre_oip.ip_src))
		GRE2IFP(sc)->if_drv_flags |= IFF_DRV_RUNNING;
	else
		GRE2IFP(sc)->if_drv_flags &= ~IFF_DRV_RUNNING;
}

/*
 * ifaddr_event handler.
 * Clear IFF_DRV_RUNNING flag when ingress address disappears to prevent
 * source address spoofing.
 */
static void
in_gre_srcaddr(void *arg __unused, const struct sockaddr *sa,
    int event __unused)
{
	const struct sockaddr_in *sin;
	struct gre_softc *sc;

	/* Check that VNET is ready */
	if (V_ipv4_hashtbl == NULL)
		return;

	MPASS(in_epoch(net_epoch_preempt));
	sin = (const struct sockaddr_in *)sa;
	CK_LIST_FOREACH(sc, &GRE_SRCHASH(sin->sin_addr.s_addr), srchash) {
		if (sc->gre_oip.ip_src.s_addr != sin->sin_addr.s_addr)
			continue;
		in_gre_set_running(sc);
	}
}

static void
in_gre_udp_input(struct mbuf *m, int off, struct inpcb *inp,
    const struct sockaddr *sa, void *ctx)
{
	struct epoch_tracker et;
	struct gre_socket *gs;
	struct gre_softc *sc;
	in_addr_t dst;

	NET_EPOCH_ENTER(et);
	/*
	 * udp_append() holds reference to inp, it is safe to check
	 * inp_flags2 without INP_RLOCK().
	 * If socket was closed before we have entered NET_EPOCH section,
	 * INP_FREED flag should be set. Otherwise it should be safe to
	 * make access to ctx data, because gre_so will be freed by
	 * gre_sofree() via epoch_call().
	 */
	if (__predict_false(inp->inp_flags2 & INP_FREED)) {
		NET_EPOCH_EXIT(et);
		m_freem(m);
		return;
	}

	gs = (struct gre_socket *)ctx;
	dst = ((const struct sockaddr_in *)sa)->sin_addr.s_addr;
	CK_LIST_FOREACH(sc, &gs->list, chain) {
		if (sc->gre_oip.ip_dst.s_addr == dst)
			break;
	}
	if (sc != NULL && (GRE2IFP(sc)->if_flags & IFF_UP) != 0){
		gre_input(m, off + sizeof(struct udphdr), IPPROTO_UDP, sc);
		NET_EPOCH_EXIT(et);
		return;
	}
	m_freem(m);
	NET_EPOCH_EXIT(et);
}

static int
in_gre_setup_socket(struct gre_softc *sc)
{
	struct sockopt sopt;
	struct sockaddr_in sin;
	struct in_gre_socket *s;
	struct gre_socket *gs;
	in_addr_t addr;
	int error, value;

	/*
	 * NOTE: we are protected with gre_ioctl_sx lock.
	 *
	 * First check that socket is already configured.
	 * If so, check that source addres was not changed.
	 * If address is different, check that there are no other tunnels
	 * and close socket.
	 */
	addr = sc->gre_oip.ip_src.s_addr;
	gs = sc->gre_so;
	if (gs != NULL) {
		s = __containerof(gs, struct in_gre_socket, base);
		if (s->addr != addr) {
			if (CK_LIST_EMPTY(&gs->list)) {
				CK_LIST_REMOVE(gs, chain);
				soclose(gs->so);
				epoch_call(net_epoch_preempt, &gs->epoch_ctx,
				    gre_sofree);
			}
			gs = sc->gre_so = NULL;
		}
	}

	if (gs == NULL) {
		/*
		 * Check that socket for given address is already
		 * configured.
		 */
		gs = in_gre_lookup_socket(addr);
		if (gs == NULL) {
			s = malloc(sizeof(*s), M_GRE, M_WAITOK | M_ZERO);
			s->addr = addr;
			gs = &s->base;

			error = socreate(sc->gre_family, &gs->so,
			    SOCK_DGRAM, IPPROTO_UDP, curthread->td_ucred,
			    curthread);
			if (error != 0) {
				if_printf(GRE2IFP(sc),
				    "cannot create socket: %d\n", error);
				free(s, M_GRE);
				return (error);
			}

			error = udp_set_kernel_tunneling(gs->so,
			    in_gre_udp_input, NULL, gs);
			if (error != 0) {
				if_printf(GRE2IFP(sc),
				    "cannot set UDP tunneling: %d\n", error);
				goto fail;
			}

			memset(&sopt, 0, sizeof(sopt));
			sopt.sopt_dir = SOPT_SET;
			sopt.sopt_level = IPPROTO_IP;
			sopt.sopt_name = IP_BINDANY;
			sopt.sopt_val = &value;
			sopt.sopt_valsize = sizeof(value);
			value = 1;
			error = sosetopt(gs->so, &sopt);
			if (error != 0) {
				if_printf(GRE2IFP(sc),
				    "cannot set IP_BINDANY opt: %d\n", error);
				goto fail;
			}

			memset(&sin, 0, sizeof(sin));
			sin.sin_family = AF_INET;
			sin.sin_len = sizeof(sin);
			sin.sin_addr.s_addr = addr;
			sin.sin_port = htons(GRE_UDPPORT);
			error = sobind(gs->so, (struct sockaddr *)&sin,
			    curthread);
			if (error != 0) {
				if_printf(GRE2IFP(sc),
				    "cannot bind socket: %d\n", error);
				goto fail;
			}
			/* Add socket to the chain */
			CK_LIST_INSERT_HEAD(&GRE_SOCKHASH(addr), gs, chain);
		}
	}

	/* Add softc to the socket's list */
	CK_LIST_INSERT_HEAD(&gs->list, sc, chain);
	sc->gre_so = gs;
	return (0);
fail:
	soclose(gs->so);
	free(s, M_GRE);
	return (error);
}

static int
in_gre_attach(struct gre_softc *sc)
{
	struct grehdr *gh;
	int error;

	if (sc->gre_options & GRE_UDPENCAP) {
		sc->gre_csumflags = CSUM_UDP;
		sc->gre_hlen = sizeof(struct greudp);
		sc->gre_oip.ip_p = IPPROTO_UDP;
		gh = &sc->gre_udphdr->gi_gre;
		gre_update_udphdr(sc, &sc->gre_udp,
		    in_pseudo(sc->gre_oip.ip_src.s_addr,
		    sc->gre_oip.ip_dst.s_addr, 0));
	} else {
		sc->gre_hlen = sizeof(struct greip);
		sc->gre_oip.ip_p = IPPROTO_GRE;
		gh = &sc->gre_iphdr->gi_gre;
	}
	sc->gre_oip.ip_v = IPVERSION;
	sc->gre_oip.ip_hl = sizeof(struct ip) >> 2;
	gre_update_hdr(sc, gh);

	/*
	 * If we return error, this means that sc is not linked,
	 * and caller should reset gre_family and free(sc->gre_hdr).
	 */
	if (sc->gre_options & GRE_UDPENCAP) {
		error = in_gre_setup_socket(sc);
		if (error != 0)
			return (error);
	} else
		CK_LIST_INSERT_HEAD(&GRE_HASH_SC(sc), sc, chain);
	CK_LIST_INSERT_HEAD(&GRE_SRCHASH(sc->gre_oip.ip_src.s_addr),
	    sc, srchash);

	/* Set IFF_DRV_RUNNING if interface is ready */
	in_gre_set_running(sc);
	return (0);
}

int
in_gre_setopts(struct gre_softc *sc, u_long cmd, uint32_t value)
{
	int error;

	/* NOTE: we are protected with gre_ioctl_sx lock */
	MPASS(cmd == GRESKEY || cmd == GRESOPTS || cmd == GRESPORT);
	MPASS(sc->gre_family == AF_INET);

	/*
	 * If we are going to change encapsulation protocol, do check
	 * for duplicate tunnels. Return EEXIST here to do not confuse
	 * user.
	 */
	if (cmd == GRESOPTS &&
	    (sc->gre_options & GRE_UDPENCAP) != (value & GRE_UDPENCAP) &&
	    in_gre_checkdup(sc, sc->gre_oip.ip_src.s_addr,
		sc->gre_oip.ip_dst.s_addr, value) == EADDRNOTAVAIL)
		return (EEXIST);

	CK_LIST_REMOVE(sc, chain);
	CK_LIST_REMOVE(sc, srchash);
	GRE_WAIT();
	switch (cmd) {
	case GRESKEY:
		sc->gre_key = value;
		break;
	case GRESOPTS:
		sc->gre_options = value;
		break;
	case GRESPORT:
		sc->gre_port = value;
		break;
	}
	error = in_gre_attach(sc);
	if (error != 0) {
		sc->gre_family = 0;
		free(sc->gre_hdr, M_GRE);
	}
	return (error);
}

int
in_gre_ioctl(struct gre_softc *sc, u_long cmd, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	struct sockaddr_in *dst, *src;
	struct ip *ip;
	int error;

	/* NOTE: we are protected with gre_ioctl_sx lock */
	error = EINVAL;
	switch (cmd) {
	case SIOCSIFPHYADDR:
		src = &((struct in_aliasreq *)data)->ifra_addr;
		dst = &((struct in_aliasreq *)data)->ifra_dstaddr;

		/* sanity checks */
		if (src->sin_family != dst->sin_family ||
		    src->sin_family != AF_INET ||
		    src->sin_len != dst->sin_len ||
		    src->sin_len != sizeof(*src))
			break;
		if (src->sin_addr.s_addr == INADDR_ANY ||
		    dst->sin_addr.s_addr == INADDR_ANY) {
			error = EADDRNOTAVAIL;
			break;
		}
		if (V_ipv4_hashtbl == NULL) {
			V_ipv4_hashtbl = gre_hashinit();
			V_ipv4_srchashtbl = gre_hashinit();
			V_ipv4_sockets = (struct gre_sockets *)gre_hashinit();
		}
		error = in_gre_checkdup(sc, src->sin_addr.s_addr,
		    dst->sin_addr.s_addr, sc->gre_options);
		if (error == EADDRNOTAVAIL)
			break;
		if (error == EEXIST) {
			/* Addresses are the same. Just return. */
			error = 0;
			break;
		}
		ip = malloc(sizeof(struct greudp) + 3 * sizeof(uint32_t),
		    M_GRE, M_WAITOK | M_ZERO);
		ip->ip_src.s_addr = src->sin_addr.s_addr;
		ip->ip_dst.s_addr = dst->sin_addr.s_addr;
		if (sc->gre_family != 0) {
			/* Detach existing tunnel first */
			CK_LIST_REMOVE(sc, chain);
			CK_LIST_REMOVE(sc, srchash);
			GRE_WAIT();
			free(sc->gre_hdr, M_GRE);
			/* XXX: should we notify about link state change? */
		}
		sc->gre_family = AF_INET;
		sc->gre_hdr = ip;
		sc->gre_oseq = 0;
		sc->gre_iseq = UINT32_MAX;
		error = in_gre_attach(sc);
		if (error != 0) {
			sc->gre_family = 0;
			free(sc->gre_hdr, M_GRE);
		}
		break;
	case SIOCGIFPSRCADDR:
	case SIOCGIFPDSTADDR:
		if (sc->gre_family != AF_INET) {
			error = EADDRNOTAVAIL;
			break;
		}
		src = (struct sockaddr_in *)&ifr->ifr_addr;
		memset(src, 0, sizeof(*src));
		src->sin_family = AF_INET;
		src->sin_len = sizeof(*src);
		src->sin_addr = (cmd == SIOCGIFPSRCADDR) ?
		    sc->gre_oip.ip_src: sc->gre_oip.ip_dst;
		error = prison_if(curthread->td_ucred, (struct sockaddr *)src);
		if (error != 0)
			memset(src, 0, sizeof(*src));
		break;
	}
	return (error);
}

int
in_gre_output(struct mbuf *m, int af, int hlen)
{
	struct greip *gi;

	gi = mtod(m, struct greip *);
	switch (af) {
	case AF_INET:
		/*
		 * gre_transmit() has used M_PREPEND() that doesn't guarantee
		 * m_data is contiguous more than hlen bytes. Use m_copydata()
		 * here to avoid m_pullup().
		 */
		m_copydata(m, hlen + offsetof(struct ip, ip_tos),
		    sizeof(u_char), &gi->gi_ip.ip_tos);
		m_copydata(m, hlen + offsetof(struct ip, ip_id),
		    sizeof(u_short), (caddr_t)&gi->gi_ip.ip_id);
		break;
#ifdef INET6
	case AF_INET6:
		gi->gi_ip.ip_tos = 0; /* XXX */
		ip_fillid(&gi->gi_ip);
		break;
#endif
	}
	gi->gi_ip.ip_ttl = V_ip_gre_ttl;
	gi->gi_ip.ip_len = htons(m->m_pkthdr.len);
	return (ip_output(m, NULL, NULL, IP_FORWARDING, NULL, NULL));
}

static const struct srcaddrtab *ipv4_srcaddrtab = NULL;
static const struct encaptab *ecookie = NULL;
static const struct encap_config ipv4_encap_cfg = {
	.proto = IPPROTO_GRE,
	.min_length = sizeof(struct greip) + sizeof(struct ip),
	.exact_match = ENCAP_DRV_LOOKUP,
	.lookup = in_gre_lookup,
	.input = gre_input
};

void
in_gre_init(void)
{

	if (!IS_DEFAULT_VNET(curvnet))
		return;
	ipv4_srcaddrtab = ip_encap_register_srcaddr(in_gre_srcaddr,
	    NULL, M_WAITOK);
	ecookie = ip_encap_attach(&ipv4_encap_cfg, NULL, M_WAITOK);
}

void
in_gre_uninit(void)
{

	if (IS_DEFAULT_VNET(curvnet)) {
		ip_encap_detach(ecookie);
		ip_encap_unregister_srcaddr(ipv4_srcaddrtab);
	}
	if (V_ipv4_hashtbl != NULL) {
		gre_hashdestroy(V_ipv4_hashtbl);
		V_ipv4_hashtbl = NULL;
		GRE_WAIT();
		gre_hashdestroy(V_ipv4_srchashtbl);
		gre_hashdestroy((struct gre_list *)V_ipv4_sockets);
	}
}
