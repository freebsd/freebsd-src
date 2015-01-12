/*-
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
 *	@(#)if_loop.c	8.2 (Berkeley) 1/9/95
 * $FreeBSD$
 */

/*
 * Loopback interface driver for protocol testing and timing.
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_types.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/bpf.h>
#include <net/vnet.h>

#ifdef	INET
#include <netinet/in.h>
#endif

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet/ip6.h>
#endif

#include <security/mac/mac_framework.h>

#ifdef TINY_LOMTU
#define	LOMTU	(1024+512)
#elif defined(LARGE_LOMTU)
#define LOMTU	131072
#else
#define LOMTU	16384
#endif

#define	LO_CSUM_FEATURES	(CSUM_IP | CSUM_TCP | CSUM_UDP | CSUM_SCTP)
#define	LO_CSUM_FEATURES6	(CSUM_TCP_IPV6 | CSUM_UDP_IPV6 | CSUM_SCTP_IPV6)
#define	LO_CSUM_SET		(CSUM_DATA_VALID | CSUM_DATA_VALID_IPV6 | \
				    CSUM_PSEUDO_HDR | \
				    CSUM_IP_CHECKED | CSUM_IP_VALID | \
				    CSUM_SCTP_VALID)
int		if_simloop(if_t, struct mbuf *, int, int);
static int	loioctl(if_t, u_long, caddr_t);
static int	looutput(if_t, struct mbuf *,
		    const struct sockaddr *, struct route *);
static int	lo_clone_create(struct if_clone *, int, caddr_t);
static void	lo_clone_destroy(if_t);

VNET_DEFINE(if_t, loif);	/* Used externally. */
#define	V_loif	VNET(loif)

#ifdef VIMAGE
static VNET_DEFINE(struct if_clone *, lo_cloner);
#define	V_lo_cloner		VNET(lo_cloner)
#endif

static struct if_clone *lo_cloner;
static const char loname[] = "lo";

static struct ifdriver lo_ifdrv = {
	.ifdrv_ops = {
		.ifop_origin = IFOP_ORIGIN_DRIVER,
		.ifop_ioctl = loioctl,
		.ifop_output = looutput,
	},
	.ifdrv_name = loname,
	.ifdrv_type = IFT_LOOP,
	.ifdrv_dlt = DLT_NULL,
	.ifdrv_dlt_hdrlen = sizeof(uint32_t),
};

static void
lo_clone_destroy(if_t ifp)
{

#ifndef VIMAGE
	/* XXX: destroying lo0 will lead to panics. */
	KASSERT(V_loif != ifp, ("%s: destroying lo0", __func__));
#endif

	if_detach(ifp);
}

static int
lo_clone_create(struct if_clone *ifc, int unit, caddr_t params)
{
	struct if_attach_args ifat = {
		.ifat_version = IF_ATTACH_VERSION,
		.ifat_drv = &lo_ifdrv,
		.ifat_dunit = unit,
		.ifat_mtu = LOMTU,
		.ifat_flags = IFF_LOOPBACK | IFF_MULTICAST,
		.ifat_capabilities = IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6,
		.ifat_capenable = IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6,
		.ifat_hwassist = LO_CSUM_FEATURES | LO_CSUM_FEATURES6,
	};
	if_t ifp;

	ifp = if_attach(&ifat);
	if (V_loif == NULL)
		V_loif = ifp;

	return (0);
}

static void
vnet_loif_init(const void *unused __unused)
{

#ifdef VIMAGE
	lo_cloner = if_clone_simple(loname, lo_clone_create, lo_clone_destroy,
	    1);
	V_lo_cloner = lo_cloner;
#else
	lo_cloner = if_clone_simple(loname, lo_clone_create, lo_clone_destroy,
	    1);
#endif
}
VNET_SYSINIT(vnet_loif_init, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_loif_init, NULL);

#ifdef VIMAGE
static void
vnet_loif_uninit(const void *unused __unused)
{

	if_clone_detach(V_lo_cloner);
	V_loif = NULL;
}
VNET_SYSUNINIT(vnet_loif_uninit, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY,
    vnet_loif_uninit, NULL);
#endif

static int
loop_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		break;

	case MOD_UNLOAD:
		printf("loop module unload - not possible for this module type\n");
		return (EINVAL);

	default:
		return (EOPNOTSUPP);
	}
	return (0);
}

static moduledata_t loop_mod = {
	"if_lo",
	loop_modevent,
	0
};

DECLARE_MODULE(if_lo, loop_mod, SI_SUB_PROTO_IFATTACHDOMAIN, SI_ORDER_ANY);

int
looutput(if_t ifp, struct mbuf *m, const struct sockaddr *dst,
    struct route *ro)
{
	u_int32_t af;
	struct rtentry *rt = NULL;
#ifdef MAC
	int error;
#endif

	M_ASSERTPKTHDR(m); /* check if we have the packet header */

	if (ro != NULL)
		rt = ro->ro_rt;
#ifdef MAC
	error = mac_ifnet_check_transmit(ifp, m);
	if (error) {
		m_freem(m);
		return (error);
	}
#endif

	if (rt && rt->rt_flags & (RTF_REJECT|RTF_BLACKHOLE)) {
		m_freem(m);
		return (rt->rt_flags & RTF_BLACKHOLE ? 0 :
		        rt->rt_flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
	}

	if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
	if_inc_counter(ifp, IFCOUNTER_OBYTES, m->m_pkthdr.len);

	/* BPF writes need to be handled specially. */
	if (dst->sa_family == AF_UNSPEC)
		bcopy(dst->sa_data, &af, sizeof(af));
	else
		af = dst->sa_family;

#if 1	/* XXX */
	switch (af) {
	case AF_INET:
		if (if_get(ifp, IF_CAPENABLE) & IFCAP_RXCSUM) {
			m->m_pkthdr.csum_data = 0xffff;
			m->m_pkthdr.csum_flags = LO_CSUM_SET;
		}
		m->m_pkthdr.csum_flags &= ~LO_CSUM_FEATURES;
		break;
	case AF_INET6:
#if 0
		/*
		 * XXX-BZ for now always claim the checksum is good despite
		 * any interface flags.   This is a workaround for 9.1-R and
		 * a proper solution ought to be sought later.
		 */
		if (ifp->if_capenable & IFCAP_RXCSUM_IPV6) {
			m->m_pkthdr.csum_data = 0xffff;
			m->m_pkthdr.csum_flags = LO_CSUM_SET;
		}
#else
		m->m_pkthdr.csum_data = 0xffff;
		m->m_pkthdr.csum_flags = LO_CSUM_SET;
#endif
		m->m_pkthdr.csum_flags &= ~LO_CSUM_FEATURES6;
		break;
	default:
		printf("looutput: af=%d unexpected\n", af);
		m_freem(m);
		return (EAFNOSUPPORT);
	}
#endif
	return (if_simloop(ifp, m, af, 0));
}

/*
 * if_simloop()
 *
 * This function is to support software emulation of hardware loopback,
 * i.e., for interfaces with the IFF_SIMPLEX attribute. Since they can't
 * hear their own broadcasts, we create a copy of the packet that we
 * would normally receive via a hardware loopback.
 *
 * This function expects the packet to include the media header of length hlen.
 */
int
if_simloop(if_t ifp, struct mbuf *m, int af, int hlen)
{
	int isr;

	M_ASSERTPKTHDR(m);
	m_tag_delete_nonpersistent(m);
	m->m_pkthdr.rcvif = ifp;

#ifdef MAC
	mac_ifnet_create_mbuf(ifp, m);
#endif

	/*
	 * Let BPF see incoming packet in the following manner:
	 *  - Emulated packet loopback for a simplex interface
	 *    (net/if_ethersubr.c)
	 *	-> passes it to ifp's BPF
	 *  - IPv4/v6 multicast packet loopback (netinet(6)/ip(6)_output.c)
	 *	-> not passes it to any BPF
	 *  - Normal packet loopback from myself to myself (net/if_loop.c)
	 *	-> passes to lo0's BPF (even in case of IPv6, where ifp!=lo0)
	 */
	if (hlen > 0)
		if_mtap(ifp, m, NULL, 0);
	else if ((m->m_flags & M_MCAST) == 0 || V_loif == ifp)
		if_mtap(V_loif, m, &af, sizeof(af));

	/* Strip away media header */
	if (hlen > 0) {
		m_adj(m, hlen);
#ifndef __NO_STRICT_ALIGNMENT
		/*
		 * Some archs do not like unaligned data, so
		 * we move data down in the first mbuf.
		 */
		if (mtod(m, vm_offset_t) & 3) {
			KASSERT(hlen >= 3, ("if_simloop: hlen too small"));
			bcopy(m->m_data,
			    (char *)(mtod(m, vm_offset_t)
				- (mtod(m, vm_offset_t) & 3)),
			    m->m_len);
			m->m_data -= (mtod(m,vm_offset_t) & 3);
		}
#endif
	}

	/* Deliver to upper layer protocol */
	switch (af) {
#ifdef INET
	case AF_INET:
		isr = NETISR_IP;
		break;
#endif
#ifdef INET6
	case AF_INET6:
		m->m_flags |= M_LOOP;
		isr = NETISR_IPV6;
		break;
#endif
	default:
		printf("if_simloop: can't handle af=%d\n", af);
		m_freem(m);
		return (EAFNOSUPPORT);
	}
	if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);
	if_inc_counter(ifp, IFCOUNTER_IBYTES, m->m_pkthdr.len);
	netisr_queue(isr, m);	/* mbuf is free'd on failure. */
	return (0);
}

/*
 * Process an ioctl request.
 */
/* ARGSUSED */
int
loioctl(if_t ifp, u_long cmd, caddr_t data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0, mask;

	switch (cmd) {
	case SIOCSIFADDR:
		if_addflags(ifp, IF_FLAGS, IFF_UP);
		/*
		 * Everything else is done at a higher level.
		 */
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifr == 0) {
			error = EAFNOSUPPORT;		/* XXX */
			break;
		}
		switch (ifr->ifr_addr.sa_family) {

#ifdef INET
		case AF_INET:
			break;
#endif
#ifdef INET6
		case AF_INET6:
			break;
#endif

		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCSIFMTU:
		if_set(ifp, IF_MTU, ifr->ifr_mtu);
		break;

	case SIOCSIFFLAGS:
		break;

	case SIOCSIFCAP:
		mask = if_get(ifp, IF_CAPENABLE) ^ ifr->ifr_reqcap;
		if ((mask & IFCAP_RXCSUM) != 0)
			if_xorflags(ifp, IF_CAPENABLE, IFCAP_RXCSUM);
		if ((mask & IFCAP_TXCSUM) != 0)
			if_xorflags(ifp, IF_CAPENABLE, IFCAP_TXCSUM);
		if ((mask & IFCAP_RXCSUM_IPV6) != 0) {
#if 0
			if_xorflags(ifp, IF_CAPENABLE, IFCAP_RXCSUM_IPV6);
#else
			error = EOPNOTSUPP;
			break;
#endif
		}
		if ((mask & IFCAP_TXCSUM_IPV6) != 0) {
#if 0
			if_xorflags(ifp, IF_CAPENABLE, IFCAP_TXCSUM_IPV6);
#else
			error = EOPNOTSUPP;
			break;
#endif
		}
		if_set(ifp, IF_HWASSIST, 0);
		if (if_get(ifp, IF_CAPENABLE) & IFCAP_TXCSUM)
			if_set(ifp, IF_HWASSIST, LO_CSUM_FEATURES);
#if 0
		if (if_get(ifp, IF_CAPENABLE) & IFCAP_TXCSUM_IPV6)
			if_addflags(ifp, IF_HWASSIST, LO_CSUM_FEATURES6);
#endif
		break;

	default:
		error = EINVAL;
	}
	return (error);
}
