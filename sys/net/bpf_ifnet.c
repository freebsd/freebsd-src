/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1990, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 2019 Andrey V. Elsukov <ae@FreeBSD.org>
 * Copyright (c) 2025 Gleb Smirnoff <glebius@FreeBSD.org>
 *
 * This code is derived from the Stanford/CMU enet packet filter,
 * (net/enet.c) distributed as part of 4.3BSD, and code contributed
 * to Berkeley by Steven McCanne and Van Jacobson both of Lawrence
 * Berkeley Laboratory.
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

#include <sys/mbuf.h>
#include <sys/socket.h>
#include <net/bpf.h>
#include <net/bpfdesc.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/if_types.h>
#include <net/route.h>

/* We need to know all the ifnets we support. */
#include <net/if_dl.h>
#include <net/ethernet.h>
#include <net/firewire.h>
#include <net/if_pflog.h>
#include <net/if_pfsync.h>

#include <security/mac/mac_framework.h>

static int
bpf_ifnet_write(void *arg, struct mbuf *m, struct mbuf *mc, int flags)
{
	struct ifnet *ifp = arg;
	struct route ro = {};
	struct sockaddr dst = {
		.sa_family = AF_UNSPEC,
	};
	u_int hlen;
	int error;

	NET_EPOCH_ASSERT();

	if (__predict_false((ifp->if_flags & IFF_UP) == 0)) {
		m_freem(m);
		m_freem(mc);
		return (ENETDOWN);
	}

	switch (ifp->if_type) {
	/* DLT_RAW */
	case IFT_MBIM:		/* umb(4) */
	case IFT_OTHER:		/* uhso(4), usie */
		hlen = 0;
		break;

	/* DLT_ENC */
	case IFT_ENC:
		hlen = 12; /* XXXGL: sizeof(struct enchdr); */
		break;

	/* DLT_EN10MB */
	case IFT_ETHER:		/* if_ethersubr.c */
	case IFT_L2VLAN:	/* vlan(4) */
	case IFT_BRIDGE:	/* if_bridge(4) */
	case IFT_IEEE8023ADLAG:	/* lagg(4) */
	case IFT_INFINIBAND:	/* if_infiniband.c */
	{
		struct ether_header *eh;

		eh = mtod(m, struct ether_header *);
		if (ETHER_IS_MULTICAST(eh->ether_dhost)) {
			if (bcmp(ifp->if_broadcastaddr, eh->ether_dhost,
			    ETHER_ADDR_LEN) == 0)
				m->m_flags |= M_BCAST;
			else
				m->m_flags |= M_MCAST;
		}
		if (!(flags & BPFD_HDRCMPLT)) {
			memcpy(eh->ether_shost, IF_LLADDR(ifp),
			    sizeof(eh->ether_shost));
		}
		hlen = ETHER_HDR_LEN;
		break;
	}
	/* DLT_APPLE_IP_OVER_IEEE1394 */
	case IFT_IEEE1394:	/* fwip(4) */
		hlen = sizeof(struct fw_hwaddr);
		break;

	/* DLT_NULL */
	case IFT_GIF:		/* gif(4) */
	case IFT_LOOP:		/* lo(4), disc(4) */
	case IFT_PARA:		/* plip(4), iic */
	case IFT_PPP:		/* tun(4) */
	case IFT_PROPVIRTUAL:	/* ng_iface(4) */
	case IFT_WIREGUARD:	/* wg(4) */
	case IFT_STF:		/* stf(4) */
	case IFT_TUNNEL:	/* ipsec(4), me(4), gre(4), ovpn(4) */
		hlen = sizeof(uint32_t);
		break;

	/* DLT_PFLOG */
	case IFT_PFLOG:
		hlen = PFLOG_HDRLEN;
		break;

	/* DLT_PFSYNC */
	case IFT_PFSYNC:
		hlen = PFSYNC_HDRLEN;
		break;

	default:
		hlen = 0;	/* pacify compiler */
		KASSERT(0, ("%s: ifp %p type %u not supported", __func__,
		    ifp, ifp->if_type));
	}

	if (__predict_false(hlen > m->m_len)) {
		m_freem(m);
		m_freem(mc);
		return (EMSGSIZE);
	};

	if (hlen != 0) {
		bcopy(mtod(m, const void *), &dst.sa_data, hlen);
		ro.ro_prepend = (char *)&dst.sa_data;
		ro.ro_plen = hlen;
		ro.ro_flags = RT_HAS_HEADER;
		m->m_pkthdr.len -= hlen;
		m->m_len -= hlen;
		m->m_data += hlen;
	};

	CURVNET_SET(ifp->if_vnet);
	error = ifp->if_output(ifp, m, &dst, &ro);
	if (error != 0) {
		m_freem(mc);
	} else if (mc != NULL) {
		mc->m_pkthdr.rcvif = ifp;
		(void)ifp->if_input(ifp, mc);
	}
	CURVNET_RESTORE();

	return (error);
}

static bool
bpf_ifnet_chkdir(void *arg, const struct mbuf *m, int dir)
{
	struct ifnet *ifp = arg;
	struct ifnet *rcvif = m_rcvif(m);

	return ((dir == BPF_D_IN && ifp != rcvif) ||
	    (dir == BPF_D_OUT && ifp == rcvif));
}

uint32_t
bpf_ifnet_wrsize(void *arg)
{
	struct ifnet *ifp = arg;

	return (ifp->if_mtu);
}

int
bpf_ifnet_promisc(void *arg, bool on)
{
	struct ifnet *ifp = arg;
	int error;

	CURVNET_SET(ifp->if_vnet);
	if ((error = ifpromisc(ifp, on ? 1 : 0)) != 0)
		if_printf(ifp, "%s: ifpromisc failed (%d)\n", __func__, error);
	CURVNET_RESTORE();

	return (error);
}

#ifdef MAC
static int
bpf_ifnet_mac_check_receive(void *arg, struct bpf_d *d)
{
	struct ifnet *ifp = arg;

	return (mac_bpfdesc_check_receive(d, ifp));
}
#endif

static const struct bif_methods bpf_ifnet_methods = {
	.bif_chkdir = bpf_ifnet_chkdir,
	.bif_promisc = bpf_ifnet_promisc,
	.bif_wrsize = bpf_ifnet_wrsize,
	.bif_write = bpf_ifnet_write,
#ifdef MAC
	.bif_mac_check_receive = bpf_ifnet_mac_check_receive,
#endif
};

/*
 * Attach an interface to bpf.  dlt is the link layer type; hdrlen is the
 * fixed size of the link header (variable length headers not yet supported).
 * Legacy KPI to be obsoleted soon.
 */
void
bpfattach(struct ifnet *ifp, u_int dlt, u_int hdrlen)
{

	ifp->if_bpf = bpf_attach(ifp->if_xname, dlt, hdrlen,
	    &bpf_ifnet_methods, ifp);
	if_ref(ifp);
	if (bootverbose && IS_DEFAULT_VNET(curvnet))
		if_printf(ifp, "bpf attached\n");
}

/*
 * The dead_bpf_if is an ugly plug against races at ifnet destroy time that
 * still exist and are not properly covered by epoch(9).
 * Legacy KPI to be obsoleted soon.
 */
void
bpfdetach(struct ifnet *ifp)
{
	static const struct bpfd_list dead_bpf_if = CK_LIST_HEAD_INITIALIZER();
	struct bpf_if *bif;

	bif = ifp->if_bpf;
	ifp->if_bpf = __DECONST(struct bpf_if *, &dead_bpf_if);
	bpf_detach(bif);
	if_rele(ifp);
}
