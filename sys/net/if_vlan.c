/*
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: if_vlan.c,v 1.1 1998/03/18 01:40:12 wollman Exp $
 */

/*
 * if_vlan.c - pseudo-device driver for IEEE 802.1Q virtual LANs.
 * Might be extended some day to also handle IEEE 802.1p priority
 * tagging.  This is sort of sneaky in the implementation, since
 * we need to pretend to be enough of an Ethernet implementation
 * to make arp work.  The way we do this is by telling everyone
 * that we are an Ethernet, and then catch the packets that
 * ether_output() left on our output queue queue when it calls
 * if_start(), rewrite them for use by the real outgoing interface,
 * and ask it to send them.
 */

#include "vlan.h"
#if NVLAN > 0
#include "opt_inet.h"
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/systm.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

SYSCTL_NODE(_net_link, IFT_8021_VLAN, vlan, CTLFLAG_RW, 0, "IEEE 802.1Q VLAN");
SYSCTL_NODE(_net_link_vlan, PF_LINK, link, CTLFLAG_RW, 0, "for consistency");

u_int	vlan_proto = ETHERTYPE_VLAN;
SYSCTL_INT(_net_link_vlan_link, VLANCTL_PROTO, proto, CTLFLAG_RW, &vlan_proto,
	   0, "Ethernet protocol used for VLAN encapsulation");

static	struct ifvlan ifv_softc[NVLAN];

static	void vlan_start(struct ifnet *ifp);
static	void vlan_ifinit(void *foo);
static	int vlan_ioctl(struct ifnet *ifp, int cmd, caddr_t addr);

static void
vlaninit(void *dummy)
{
	int i;

	for (i = 0; i < NVLAN; i++) {
		struct ifnet *ifp = &ifv_softc[i].ifv_if;

		ifp->if_softc = &ifv_softc[i];
		ifp->if_name = "vlan";
		ifp->if_unit = i;
		/* NB: flags are not set here */
		ifp->if_linkmib = &ifv_softc[i].ifv_mib;
		ifp->if_linkmiblen = sizeof ifv_softc[i].ifv_mib;
		/* NB: mtu is not set here */

		ifp->if_init = vlan_ifinit;
		ifp->if_start = vlan_start;
		ifp->if_ioctl = vlan_ioctl;
		ifp->if_output = ether_output;
		if_attach(ifp);
		ether_ifattach(ifp);
#if NBPFILTER > 0
		bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif
		/* Now undo some of the damage... */
		ifp->if_data.ifi_type = IFT_8021_VLAN;
		ifp->if_data.ifi_hdrlen = EVL_ENCAPLEN;
		ifp->if_resolvemulti = 0;
	}
}
PSEUDO_SET(vlaninit, if_vlan);

static void
vlan_ifinit(void *foo)
{
	;
}

static void
vlan_start(struct ifnet *ifp)
{
	struct ifvlan *ifv;
	struct ifnet *p;
	struct ether_vlan_header *evl;
	struct mbuf *m;

	ifv = ifp->if_softc;
	p = ifv->ifv_p;

	ifp->if_flags |= IFF_OACTIVE;
	for (;;) {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == 0)
			break;
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp, m);
#endif /* NBPFILTER > 0 */

		M_PREPEND(m, EVL_ENCAPLEN, M_DONTWAIT);
		if (m == 0)
			continue;
		/* M_PREPEND takes care of m_len, m_pkthdr.len for us */

		/*
		 * Transform the Ethernet header into an Ethernet header
		 * with 802.1Q encapsulation.
		 */
		bcopy(mtod(m, char *) + EVL_ENCAPLEN, mtod(m, char *),
		      sizeof(struct ether_header));
		evl = mtod(m, struct ether_vlan_header *);
		evl->evl_proto = evl->evl_encap_proto;
		evl->evl_encap_proto = htons(vlan_proto);
		evl->evl_tag = htons(ifv->ifv_tag);
		printf("vlan_start: %*D\n", sizeof *evl, (char *)evl, ":");

		/*
		 * Send it, precisely as ether_output() would have.
		 * We are already running at splimp.
		 */
		if (IF_QFULL(&p->if_snd)) {
			IF_DROP(&p->if_snd);
				/* XXX stats */
		}
		IF_ENQUEUE(&p->if_snd, m);
		if ((p->if_flags & IFF_OACTIVE) == 0)
			p->if_start(p);
	}
	ifp->if_flags &= ~IFF_OACTIVE;
}

int
vlan_input(struct ether_header *eh, struct mbuf *m)
{
	int i;
	struct ifvlan *ifv;

	for (i = 0; i < NVLAN; i++) {
		ifv = &ifv_softc[i];
		if (m->m_pkthdr.rcvif == ifv->ifv_p
		    && (EVL_VLANOFTAG(ntohs(*mtod(m, u_int16_t *)))
			== ifv->ifv_tag))
			break;
	}

	if (i >= NVLAN || (ifv->ifv_if.if_flags & IFF_UP) == 0) {
		m_freem(m);
		return -1;	/* so ether_input can take note */
	}

	/*
	 * Having found a valid vlan interface corresponding to
	 * the given source interface and vlan tag, remove the
	 * encapsulation, and run the real packet through
	 * ether_input() a second time (it had better be
	 * reentrant!).
	 */
	m->m_pkthdr.rcvif = &ifv->ifv_if;
	eh->ether_type = mtod(m, u_int16_t *)[1];
	m->m_data += EVL_ENCAPLEN;
	m->m_len -= EVL_ENCAPLEN;
	m->m_pkthdr.len -= EVL_ENCAPLEN;

#if NBPFILTER > 0
	if (ifv->ifv_if.if_bpf) {
		/*
		 * Do the usual BPF fakery.  Note that we don't support
		 * promiscuous mode here, since it would require the
		 * drivers to know about VLANs and we're not ready for
		 * that yet.
		 */
		struct mbuf m0;
		m0.m_next = m;
		m0.m_len = sizeof(struct ether_header);
		m0.m_data = (char *)eh;
		bpf_mtap(&ifv->ifv_if, &m0);
	}
#endif
	ether_input(&ifv->ifv_if, eh, m);
	return 0;
}

static int
vlan_config(struct ifvlan *ifv, struct ifnet *p)
{
	struct ifaddr *ifa1, *ifa2;
	struct sockaddr_dl *sdl1, *sdl2;

	if (p->if_data.ifi_type != IFT_ETHER)
		return EPROTONOSUPPORT;
	if (ifv->ifv_p)
		return EBUSY;
	ifv->ifv_p = p;
	if (p->if_data.ifi_hdrlen == sizeof(struct ether_vlan_header))
		ifv->ifv_if.if_mtu = p->if_mtu;
	else
		ifv->ifv_if.if_mtu = p->if_data.ifi_mtu - EVL_ENCAPLEN;

	/*
	 * NB: we don't support multicast at this point.
	 */
	ifv->ifv_if.if_flags = (p->if_flags & ~IFF_MULTICAST); /* XXX */

	/*
	 * Set up our ``Ethernet address'' to reflect the underlying
	 * physical interface's.
	 */
	ifa1 = ifnet_addrs[ifv->ifv_if.if_index - 1];
	ifa2 = ifnet_addrs[p->if_index - 1];
	sdl1 = (struct sockaddr_dl *)ifa1->ifa_addr;
	sdl2 = (struct sockaddr_dl *)ifa2->ifa_addr;
	sdl1->sdl_type = IFT_ETHER;
	sdl1->sdl_alen = ETHER_ADDR_LEN;
	bcopy(LLADDR(sdl2), LLADDR(sdl1), ETHER_ADDR_LEN);
	bcopy(LLADDR(sdl2), ifv->ifv_ac.ac_enaddr, ETHER_ADDR_LEN);
	return 0;
}

static int
vlan_ioctl(struct ifnet *ifp, int cmd, caddr_t data)
{
	struct ifaddr *ifa;
	struct ifnet *p;
	struct ifreq *ifr;
	struct ifvlan *ifv;
	struct vlanreq vlr;
	int error = 0;

	ifr = (struct ifreq *)data;
	ifa = (struct ifaddr *)data;
	ifv = ifp->if_softc;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit(&ifv->ifv_ac, ifa);
			break;
#endif
		default:
			break;
		}
		break;

	case SIOCGIFADDR:
		{
			struct sockaddr *sa;

			sa = (struct sockaddr *) &ifr->ifr_data;
			bcopy(((struct arpcom *)ifp->if_softc)->ac_enaddr,
			      (caddr_t) sa->sa_data, ETHER_ADDR_LEN);
		}
		break;

	case SIOCSIFMTU:
		/*
		 * Set the interface MTU.
		 */
		if (ifr->ifr_mtu > ETHERMTU) {
			error = EINVAL;
		} else {
			ifp->if_mtu = ifr->ifr_mtu;
		}
		break;

	case SIOCSETVLAN:
		error = copyin(ifr->ifr_data, &vlr, sizeof vlr);
		if (error)
			break;
		if (vlr.vlr_parent[0] == '\0') {
			ifv->ifv_p = 0;
			if_down(ifp);
			break;
		}
		p = ifunit(vlr.vlr_parent);
		if (p == 0) {
			error = ENOENT;
			break;
		}
		error = vlan_config(ifv, p);
		if (error)
			break;
		ifv->ifv_tag = vlr.vlr_tag;
		break;
		
	case SIOCGETVLAN:
		bzero(&vlr, sizeof vlr);
		if (ifv->ifv_p) {
			sprintf(vlr.vlr_parent, "%s%d", ifv->ifv_p->if_name,
				ifv->ifv_p->if_unit);
			vlr.vlr_tag = ifv->ifv_tag;
		}
		error = copyout(&vlr, ifr->ifr_data, sizeof vlr);
		break;
		
	case SIOCSIFFLAGS:
		/*
		 * We don't support all-multicast or promiscuous modes
		 * right now because it would require help from the
		 * underlying drivers, which hasn't been implemented.
		 */
		if (ifr->ifr_flags & (IFF_PROMISC|IFF_ALLMULTI)) {
			ifp->if_flags &= ~(IFF_PROMISC|IFF_ALLMULTI);
			error = EINVAL;
		}
		break;

		/* NB: this will reject multicast state changes */
	default:
		error = EINVAL;
	}
	return error;
}

#endif /* NVLAN > 0 */
