/*
 * Copyright (c) 1998, Larry Lile
 * All rights reserved.
 *
 * For latest sources and information on this driver, please
 * go to http://anarchy.stdio.com.
 *
 * Questions, comments or suggestions should be directed to
 * Larry Lile <lile@stdio.com>.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *
 */

/*
 *
 * General ISO 802.5 (Token Ring) support routines
 * 
 */

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h> 
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/if_llc.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <net/if_arp.h>

#include <net/iso88025.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#endif

#include <net/bpf.h>

#include <machine/clock.h>
#include <machine/md_var.h>

#include <i386/isa/isa_device.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <sys/kernel.h>
#include <net/iso88025.h>

void
iso88025_ifattach(struct ifnet *ifp)
{
    register struct ifaddr *ifa = NULL;
    register struct sockaddr_dl *sdl;

    ifp->if_type = IFT_ISO88025;
    ifp->if_addrlen = ISO88025_ADDR_LEN;
    ifp->if_hdrlen = ISO88025_HDR_LEN;
    if (ifp->if_baudrate == 0)
        ifp->if_baudrate = TR_16MBPS; /* 16Mbit should be a safe default */
    if (ifp->if_mtu == 0)
        ifp->if_mtu = ISO88025_DEFAULT_MTU;
    ifp->if_broadcastaddr = etherbroadcastaddr;

        ifa = ifnet_addrs[ifp->if_index - 1];
        if (ifa == 0) {
                printf("iso88025_ifattach: no lladdr!\n");
                return;
        }
        sdl = (struct sockaddr_dl *)ifa->ifa_addr;
        sdl->sdl_type = IFT_ISO88025;
        sdl->sdl_alen = ifp->if_addrlen;
        bcopy(((struct arpcom *)ifp)->ac_enaddr, LLADDR(sdl), ifp->if_addrlen);
}

int
iso88025_ioctl(struct ifnet *ifp, int command, caddr_t data)
{
        struct ifaddr *ifa = (struct ifaddr *) data;
        struct ifreq *ifr = (struct ifreq *) data;
        int error = 0;

        switch (command) {
        case SIOCSIFADDR:
                ifp->if_flags |= IFF_UP;

                switch (ifa->ifa_addr->sa_family) {
#ifdef INET
                case AF_INET:
                        ifp->if_init(ifp->if_softc);    /* before arpwhohas */
                        arp_ifinit(ifp, ifa);
                        break;
#endif
                default:
                        ifp->if_init(ifp->if_softc);
                        break;
                }
                break;

        case SIOCGIFADDR:
                {
                        struct sockaddr *sa;

                        sa = (struct sockaddr *) & ifr->ifr_data;
                        bcopy(((struct arpcom *)ifp->if_softc)->ac_enaddr,
                              (caddr_t) sa->sa_data, ISO88025_ADDR_LEN);
                }
                break;

        case SIOCSIFMTU:
                /*
                 * Set the interface MTU.
                 */
                if (ifr->ifr_mtu > ISO88025_MAX_MTU) {
                        error = EINVAL;
                } else {
                        ifp->if_mtu = ifr->ifr_mtu;
                }
                break;
        }
        return (error);
}

/*
 * ISO88025 encapsulation
 */
int
iso88025_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst, struct rtentry *rt0)
{
	register struct iso88025_header *th;
	struct iso88025_header gen_th;
	register struct iso88025_sockaddr_data *sd = (struct iso88025_sockaddr_data *)dst->sa_data;
        register struct llc *l;
	register struct sockaddr_dl *sdl = NULL;
        int s, error = 0, rif_len = 0;
 	u_char edst[6];
	register struct rtentry *rt;
	int len = m->m_pkthdr.len, loop_copy = 0;
	struct arpcom *ac = (struct arpcom *)ifp;

	if ((ifp->if_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING))
		senderr(ENETDOWN);
	rt = rt0;
	if (rt) {
		if ((rt->rt_flags & RTF_UP) == 0) {
			rt0 = rt = rtalloc1(dst, 1, 0UL);
			if (rt0)
				rt->rt_refcnt--;
			else
				senderr(EHOSTUNREACH);
		}
		if (rt->rt_flags & RTF_GATEWAY) {
			if (rt->rt_gwroute == 0)
				goto lookup;
			if (((rt = rt->rt_gwroute)->rt_flags & RTF_UP) == 0) {
				rtfree(rt); rt = rt0;
			lookup: rt->rt_gwroute = rtalloc1(rt->rt_gateway, 1,
							  0UL);
				if ((rt = rt->rt_gwroute) == 0)
					senderr(EHOSTUNREACH);
			}
		}
		if (rt->rt_flags & RTF_REJECT)
			if (rt->rt_rmx.rmx_expire == 0 ||
			    time_second < rt->rt_rmx.rmx_expire)
				senderr(rt == rt0 ? EHOSTDOWN : EHOSTUNREACH);
	}

	/* Calculate routing info length based on arp table entry */
	if (rt && (sdl = (struct sockaddr_dl *)rt->rt_gateway))
		if (sdl->sdl_rcf != NULL)
			rif_len = TR_RCF_RIFLEN(sdl->sdl_rcf);

	/* Generate a generic 802.5 header for the packet */
	gen_th.ac = TR_AC;
	gen_th.fc = TR_LLC_FRAME;
	memcpy(gen_th.iso88025_shost, ac->ac_enaddr, sizeof(ac->ac_enaddr));
	if (rif_len) {
		gen_th.iso88025_shost[0] |= TR_RII;
		if (rif_len > 2) {
			gen_th.rcf = sdl->sdl_rcf;
			memcpy(gen_th.rd, sdl->sdl_route, rif_len - 2);
		}
	}
	

	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
		if (!arpresolve(ifp, rt, m, dst, edst, rt0))
			return (0);	/* if not yet resolved */
		/* Add LLC and SNAP headers */
		M_PREPEND(m, 8, M_DONTWAIT);
		if (m == 0)
			senderr(ENOBUFS);
		l = mtod(m, struct llc *);
	        l->llc_un.type_snap.ether_type = htons(ETHERTYPE_IP);
	        l->llc_dsap = l->llc_ssap = LLC_SNAP_LSAP;
		l->llc_un.type_snap.control = LLC_UI;
		l->llc_un.type_snap.org_code[0] = 0x0;
		l->llc_un.type_snap.org_code[1] = 0x0;
		l->llc_un.type_snap.org_code[2] = 0x0;
		memcpy(gen_th.iso88025_dhost, edst, sizeof(edst));
		break;
#endif

	case AF_UNSPEC:
		/*
		 * For AF_UNSPEC sockaddr.sa_data must contain all of the
		 * mac information needed to send the packet.  This allows
		 * full mac, llc, and source routing function to be controlled.
		 * llc and source routing information must already be in the
		 * mbuf provided, ac/fc are set in sa_data.  sockaddr.sa_data
		 * should be a iso88025_sockaddr_data structure see iso88025.h
		 */
                loop_copy = -1;
		sd = (struct iso88025_sockaddr_data *)dst->sa_data;
		gen_th.ac = sd->ac;
		gen_th.fc = sd->fc;
		memcpy(gen_th.iso88025_dhost, sd->ether_dhost, sizeof(sd->ether_dhost));
		memcpy(gen_th.iso88025_shost, sd->ether_shost, sizeof(sd->ether_shost));
		rif_len = 0;
		break;

	default:
		printf("%s%d: can't handle af%d\n", ifp->if_name, ifp->if_unit,
			dst->sa_family);
		senderr(EAFNOSUPPORT);
	}

	/*
	 * Add local net header.  If no space in first mbuf,
	 * allocate another.
	 */
	
	M_PREPEND(m, ISO88025_HDR_LEN + rif_len, M_DONTWAIT);
	if (m == 0)
		senderr(ENOBUFS);

	/* Copy as much of the generic header as is needed into the mbuf */
	th = mtod(m, struct iso88025_header *);
	memcpy(th, &gen_th, ISO88025_HDR_LEN + rif_len);

        /*
         * If a simplex interface, and the packet is being sent to our
         * Ethernet address or a broadcast address, loopback a copy.
         * XXX To make a simplex device behave exactly like a duplex
         * device, we should copy in the case of sending to our own
         * ethernet address (thus letting the original actually appear
         * on the wire). However, we don't do that here for security
         * reasons and compatibility with the original behavior.
         */     
        if ((ifp->if_flags & IFF_SIMPLEX) &&
           (loop_copy != -1)) {
                if ((m->m_flags & M_BCAST) || (loop_copy > 0)) { 
                        struct mbuf *n = m_copy(m, 0, (int)M_COPYALL);
                        (void) if_simloop(ifp,
			    n, dst->sa_family, ISO88025_HDR_LEN);
                } else if (bcmp(th->iso88025_dhost,
                    th->iso88025_shost, ETHER_ADDR_LEN) == 0) {
                        (void) if_simloop(ifp,
			    m, dst->sa_family, ISO88025_HDR_LEN);
                        return(0);      /* XXX */
                }       
        }      

        s = splimp();
	/*
	 * Queue message on interface, and start output if interface
	 * not yet active.
	 */
	if (IF_QFULL(&ifp->if_snd)) {
            printf("iso88025_output: packet dropped QFULL.\n");
		IF_DROP(&ifp->if_snd);
		splx(s);
		senderr(ENOBUFS);
	}
	if (m->m_flags & M_MCAST)
		ifp->if_omcasts++;
	IF_ENQUEUE(&ifp->if_snd, m);
        if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
	splx(s);
	ifp->if_obytes += len + ISO88025_HDR_LEN + 8;
	return (error);

bad:
	if (m)
		m_freem(m);
	return (error);
}

/*
 * ISO 88025 de-encapsulation
 */
void
iso88025_input(struct ifnet *ifp, struct iso88025_header *th, struct mbuf *m)
{
	register struct ifqueue *inq;
	u_short ether_type;
	int s;
	register struct llc *l = mtod(m, struct llc *);

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}

	switch (l->llc_control) {
	case LLC_UI:
		break;
	case LLC_TEST:
	case LLC_TEST_P:
	{
		struct sockaddr sa;
		struct arpcom *ac = (struct arpcom *)ifp;
		struct iso88025_sockaddr_data *th2;
		int i;
		u_char c = l->llc_dsap;

		if (th->iso88025_shost[0] & TR_RII) { /* XXX */
			printf("iso88025_input: dropping source routed LLC_TEST\n");
			m_free(m);
			return;
		}
		l->llc_dsap = l->llc_ssap;
		l->llc_ssap = c;
		if (m->m_flags & (M_BCAST | M_MCAST))
			bcopy((caddr_t)ac->ac_enaddr, 
			      (caddr_t)th->iso88025_dhost, ISO88025_ADDR_LEN);
		sa.sa_family = AF_UNSPEC;
		sa.sa_len = sizeof(sa);
		th2 = (struct iso88025_sockaddr_data *)sa.sa_data;
		for (i = 0; i < ISO88025_ADDR_LEN; i++) {
			th2->ether_shost[i] = c = th->iso88025_dhost[i];
			th2->ether_dhost[i] = th->iso88025_dhost[i] = th->iso88025_shost[i];
			th->iso88025_shost[i] = c;
		}
		th2->ac = TR_AC;
		th2->fc = TR_LLC_FRAME;
		ifp->if_output(ifp, m, &sa, NULL);
		return;
	}
	default:
		printf("iso88025_input: unexpected llc control 0x%02x\n", l->llc_control);
		m_freem(m);
		return;
	}

        m->m_pkthdr.len -= 8;
        m->m_len -= 8;
        m->m_data += 8; /* Length of LLC header in our case */

	ifp->if_ibytes += m->m_pkthdr.len + sizeof(*th);
	if (th->iso88025_dhost[0] & 1) {
		if (bcmp((caddr_t)etherbroadcastaddr, (caddr_t)th->iso88025_dhost, sizeof(etherbroadcastaddr)) == 0)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
	} 
	if (m->m_flags & (M_BCAST|M_MCAST))
		ifp->if_imcasts++;

	ether_type = ntohs(l->llc_un.type_snap.ether_type);

	switch (ether_type) {
#ifdef INET
	case ETHERTYPE_IP:
		th->iso88025_shost[0] &= ~(TR_RII); 
		if (ipflow_fastforward(m))
			return;
		schednetisr(NETISR_IP);
		inq = &ipintrq;
		break;

	case ETHERTYPE_ARP:
		if (ifp->if_flags & IFF_NOARP) {
			m_freem(m);
			return;
		}
		schednetisr(NETISR_ARP);
		inq = &arpintrq;
                break;
#endif
	default:
	    m_freem(m);
	    return;
	}

	s = splimp();
	if (IF_QFULL(inq)) {
		IF_DROP(inq);
		m_freem(m);
                printf("iso88025_input: Packet dropped (Queue full).\n");
	} else
		IF_ENQUEUE(inq, m);
	splx(s);
}
