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

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/clock.h>
#include <machine/md_var.h>

#include <i386/isa/isa_device.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <sys/kernel.h>
#include <net/iso88025.h>

void
iso88025_ifattach(ifp)
    register struct ifnet *ifp;
{
    register struct ifaddr *ifa = NULL;
    register struct sockaddr_dl *sdl;

    ifp->if_type = IFT_ISO88025;
    ifp->if_addrlen = 6;
    ifp->if_hdrlen=18;
    if (ifp->if_baudrate == 0)
        ifp->if_baudrate = 16000000; /* 1, 4, or 16Mbit default? */
    if (ifp->if_mtu == 0)
        ifp->if_mtu = ISO88025_DEFAULT_MTU;

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
                        arp_ifinit((struct arpcom *)ifp, ifa);
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
                if (ifr->ifr_mtu > ISO88025MTU) {
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
iso88025_output(ifp, m0, dst, rt0)
	register struct ifnet *ifp;
	struct mbuf *m0;
	struct sockaddr *dst;
	struct rtentry *rt0;
{
	register struct iso88025_header *th;
	struct iso88025_header gen_th;
	register struct iso88025_sockaddr_data *sd = (struct iso88025_sockaddr_data *)dst->sa_data;
        register struct llc *l;
	register struct sockaddr_dl *sdl = NULL;
        int s, error = 0, rif_len = 0;
 	u_char edst[6];
	register struct mbuf *m = m0;
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
			rif_len = (ntohs(sdl->sdl_rcf) & 0x1f00) >> 8;

	/* Generate a generic 802.5 header for the packet */
	gen_th.ac = 0x10;
	gen_th.fc = 0x40;
	memcpy(gen_th.iso88025_shost, ac->ac_enaddr, sizeof(ac->ac_enaddr));
	if (rif_len) {
		gen_th.iso88025_shost[0] |= 0x80;
		if (rif_len > 2) {
			gen_th.rcf = sdl->sdl_rcf;
			memcpy(gen_th.rseg, sdl->sdl_route, rif_len - 2);
		}
	}
	

	switch (dst->sa_family) {
#ifdef INET
	case AF_INET:
		if (!arpresolve(ac, rt, m, dst, edst, rt0))
			return (0);	/* if not yet resolved */
		/* Add LLC and SNAP headers */
		M_PREPEND(m, 8, M_DONTWAIT);
		if (m == 0)
			senderr(ENOBUFS);
		l = mtod(m, struct llc *);
	        l->llc_un.type_snap.ether_type = htons(ETHERTYPE_IP);
	        l->llc_dsap = 0xaa;
		l->llc_ssap = 0xaa;
		l->llc_un.type_snap.control = 0x3;
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
                        /*printf("iso88025_output: if_simloop broadcast.\n");*/
                        (void) if_simloop(ifp, n, dst, ISO88025_HDR_LEN);
                } else if (bcmp(th->iso88025_dhost,
                    th->iso88025_shost, ETHER_ADDR_LEN) == 0) {
                        /*printf("iso88025_output: if_simloop to ourselves.\n");*/
                        (void) if_simloop(ifp, m, dst, ISO88025_HDR_LEN);
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
	IF_ENQUEUE(&ifp->if_snd, m);
        /*printf("iso88025_output: packet queued.\n");*/
        if ((ifp->if_flags & IFF_OACTIVE) == 0)
		(*ifp->if_start)(ifp);
	splx(s);
	ifp->if_obytes += len + ISO88025_HDR_LEN + 8;
	if (m->m_flags & M_MCAST)
		ifp->if_omcasts++;
	return (error);

bad:
	if (m)
		m_freem(m);
        /*printf("iso88025_output: something went wrong, bailing to bad.\n");*/
	return (error);
}

/*
 * ISO 88025 de-encapsulation
 */
void
iso88025_input(ifp, th, m)
	struct ifnet *ifp;
	register struct iso88025_header *th;
	struct mbuf *m;
{
	register struct ifqueue *inq;
	u_short ether_type;
	int s;
	register struct llc *l = mtod(m, struct llc *);

        /*printf("iso88025_input: entered.\n");*/

        /*m->m_pkthdr.len = m->m_len = m->m_len - 8;*/ /* Length of LLC header in our case */
        m->m_pkthdr.len -= 8;
        m->m_len -= 8;
        m->m_data += 8; /* Length of LLC header in our case */

	if ((ifp->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return;
	}
	ifp->if_ibytes += m->m_pkthdr.len + sizeof (*th);
	if (th->iso88025_dhost[0] & 1) {
		if (bcmp((caddr_t)etherbroadcastaddr, (caddr_t)th->iso88025_dhost,
			 sizeof(etherbroadcastaddr)) == 0)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
	} 
	if (m->m_flags & (M_BCAST|M_MCAST))
		ifp->if_imcasts++;

	ether_type = ntohs(l->llc_un.type_snap.ether_type);

        /*printf("iso88025_input: source %6D dest %6D ethertype %x\n", th->iso88025_shost, ":", th->iso88025_dhost, ":", ether_type);*/

	switch (ether_type) {
#ifdef INET
	case ETHERTYPE_IP:
            /*printf("iso88025_input: IP Packet\n");*/
		th->iso88025_shost[0] &= ~(0x80); /* Turn off source route bit XXX */
		if (ipflow_fastforward(m))
			return;
		schednetisr(NETISR_IP);
		inq = &ipintrq;
		break;

	case ETHERTYPE_ARP:
            /*printf("iso88025_input: ARP Packet\n");*/
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
                /*printf("iso88025_input: Packet queued.\n");*/
	splx(s);
}
