/*-
 * Copyright (c) 2010-2012 Citrix Inc.
 * Copyright (c) 2009-2012 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 2004-2006 Kip Macy
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet6.h"
#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/sx.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip6.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/frame.h>
#include <machine/vmparam.h>

#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/mutex.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <machine/atomic.h>

#include <machine/intr_machdep.h>

#include <machine/in_cksum.h>

#include <dev/hyperv/include/hyperv.h>
#include "hv_net_vsc.h"
#include "hv_rndis.h"
#include "hv_rndis_filter.h"


/* Short for Hyper-V network interface */
#define NETVSC_DEVNAME    "hn"

/*
 * It looks like offset 0 of buf is reserved to hold the softc pointer.
 * The sc pointer evidently not needed, and is not presently populated.
 * The packet offset is where the netvsc_packet starts in the buffer.
 */
#define HV_NV_SC_PTR_OFFSET_IN_BUF         0
#define HV_NV_PACKET_OFFSET_IN_BUF         16


/*
 * Data types
 */

struct hv_netvsc_driver_context {
	uint32_t		drv_inited;
};

/*
 * Be aware that this sleepable mutex will exhibit WITNESS errors when
 * certain TCP and ARP code paths are taken.  This appears to be a
 * well-known condition, as all other drivers checked use a sleeping
 * mutex to protect their transmit paths.
 * Also Be aware that mutexes do not play well with semaphores, and there
 * is a conflicting semaphore in a certain channel code path.
 */
#define NV_LOCK_INIT(_sc, _name) \
	    mtx_init(&(_sc)->hn_lock, _name, MTX_NETWORK_LOCK, MTX_DEF)
#define NV_LOCK(_sc)		mtx_lock(&(_sc)->hn_lock)
#define NV_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->hn_lock, MA_OWNED)
#define NV_UNLOCK(_sc)		mtx_unlock(&(_sc)->hn_lock)
#define NV_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->hn_lock)


/*
 * Globals
 */

int hv_promisc_mode = 0;    /* normal mode by default */

/* The one and only one */
static struct hv_netvsc_driver_context g_netvsc_drv;


/*
 * Forward declarations
 */
static void hn_stop(hn_softc_t *sc);
static void hn_ifinit_locked(hn_softc_t *sc);
static void hn_ifinit(void *xsc);
static int  hn_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static int  hn_start_locked(struct ifnet *ifp);
static void hn_start(struct ifnet *ifp);

/*
 * NetVsc get message transport protocol type 
 */
static uint32_t get_transport_proto_type(struct mbuf *m_head)
{
	uint32_t ret_val = TRANSPORT_TYPE_NOT_IP;
	uint16_t ether_type = 0;
	int ether_len = 0;
	struct ether_vlan_header *eh;
#ifdef INET
	struct ip *iph;
#endif
#ifdef INET6
	struct ip6_hdr *ip6;
#endif

	eh = mtod(m_head, struct ether_vlan_header*);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		ether_len = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		ether_type = eh->evl_proto;
	} else {
		ether_len = ETHER_HDR_LEN;
		ether_type = eh->evl_encap_proto;
	}

	switch (ntohs(ether_type)) {
#ifdef INET6
	case ETHERTYPE_IPV6:
		ip6 = (struct ip6_hdr *)(m_head->m_data + ether_len);

		if (IPPROTO_TCP == ip6->ip6_nxt) {
			ret_val = TRANSPORT_TYPE_IPV6_TCP;
		} else if (IPPROTO_UDP == ip6->ip6_nxt) {
			ret_val = TRANSPORT_TYPE_IPV6_UDP;
		}
		break;
#endif
#ifdef INET
	case ETHERTYPE_IP:
		iph = (struct ip *)(m_head->m_data + ether_len);

		if (IPPROTO_TCP == iph->ip_p) {
			ret_val = TRANSPORT_TYPE_IPV4_TCP;
		} else if (IPPROTO_UDP == iph->ip_p) {
			ret_val = TRANSPORT_TYPE_IPV4_UDP;
		}
		break;
#endif
	default:
		ret_val = TRANSPORT_TYPE_NOT_IP;
		break;
	}

	return (ret_val);
}

/*
 * NetVsc driver initialization
 * Note:  Filter init is no longer required
 */
static int
netvsc_drv_init(void)
{
	return (0);
}

/*
 * NetVsc global initialization entry point
 */
static void
netvsc_init(void)
{
	if (bootverbose)
		printf("Netvsc initializing... ");

	/*
	 * XXXKYS: cleanup initialization
	 */
	if (!cold && !g_netvsc_drv.drv_inited) {
		g_netvsc_drv.drv_inited = 1;
		netvsc_drv_init();
		if (bootverbose)
			printf("done!\n");
	} else if (bootverbose)
		printf("Already initialized!\n");
}

/* {F8615163-DF3E-46c5-913F-F2D2F965ED0E} */
static const hv_guid g_net_vsc_device_type = {
	.data = {0x63, 0x51, 0x61, 0xF8, 0x3E, 0xDF, 0xc5, 0x46,
		0x91, 0x3F, 0xF2, 0xD2, 0xF9, 0x65, 0xED, 0x0E}
};

/*
 * Standard probe entry point.
 *
 */
static int
netvsc_probe(device_t dev)
{
	const char *p;

	p = vmbus_get_type(dev);
	if (!memcmp(p, &g_net_vsc_device_type.data, sizeof(hv_guid))) {
		device_set_desc(dev, "Synthetic Network Interface");
		if (bootverbose)
			printf("Netvsc probe... DONE \n");

		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

/*
 * Standard attach entry point.
 *
 * Called when the driver is loaded.  It allocates needed resources,
 * and initializes the "hardware" and software.
 */
static int
netvsc_attach(device_t dev)
{
	struct hv_device *device_ctx = vmbus_get_devctx(dev);
	netvsc_device_info device_info;
	hn_softc_t *sc;
	int unit = device_get_unit(dev);
	struct ifnet *ifp;
	int ret;

	netvsc_init();

	sc = device_get_softc(dev);
	if (sc == NULL) {
		return (ENOMEM);
	}

	bzero(sc, sizeof(hn_softc_t));
	sc->hn_unit = unit;
	sc->hn_dev = dev;

	NV_LOCK_INIT(sc, "NetVSCLock");

	sc->hn_dev_obj = device_ctx;

	ifp = sc->hn_ifp = if_alloc(IFT_ETHER);
	ifp->if_softc = sc;

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_dunit = unit;
	ifp->if_dname = NETVSC_DEVNAME;

	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = hn_ioctl;
	ifp->if_start = hn_start;
	ifp->if_init = hn_ifinit;
	/* needed by hv_rf_on_device_add() code */
	ifp->if_mtu = ETHERMTU;
	IFQ_SET_MAXLEN(&ifp->if_snd, 512);
	ifp->if_snd.ifq_drv_maxlen = 511;
	IFQ_SET_READY(&ifp->if_snd);

	/*
	 * Tell upper layers that we support full VLAN capability.
	 */
	ifp->if_hdrlen = sizeof(struct ether_vlan_header);
	ifp->if_capabilities |=
	    IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU | IFCAP_HWCSUM | IFCAP_TSO;
	ifp->if_capenable |=
	    IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU | IFCAP_HWCSUM | IFCAP_TSO;
	ifp->if_hwassist = CSUM_TCP | CSUM_UDP | CSUM_TSO;

	ret = hv_rf_on_device_add(device_ctx, &device_info);
	if (ret != 0) {
		if_free(ifp);

		return (ret);
	}
	if (device_info.link_state == 0) {
		sc->hn_carrier = 1;
	}

	ether_ifattach(ifp, device_info.mac_addr);

	return (0);
}

/*
 * Standard detach entry point
 */
static int
netvsc_detach(device_t dev)
{
	struct hv_device *hv_device = vmbus_get_devctx(dev); 

	if (bootverbose)
		printf("netvsc_detach\n");

	/*
	 * XXXKYS:  Need to clean up all our
	 * driver state; this is the driver
	 * unloading.
	 */

	/*
	 * XXXKYS:  Need to stop outgoing traffic and unregister
	 * the netdevice.
	 */

	hv_rf_on_device_remove(hv_device, HV_RF_NV_DESTROY_CHANNEL);

	return (0);
}

/*
 * Standard shutdown entry point
 */
static int
netvsc_shutdown(device_t dev)
{
	return (0);
}

/*
 * Send completion processing
 *
 * Note:  It looks like offset 0 of buf is reserved to hold the softc
 * pointer.  The sc pointer is not currently needed in this function, and
 * it is not presently populated by the TX function.
 */
void
netvsc_xmit_completion(void *context)
{
	netvsc_packet *packet = (netvsc_packet *)context;
	struct mbuf *mb;
	uint8_t *buf;

	mb = (struct mbuf *)(uintptr_t)packet->compl.send.send_completion_tid;
	buf = ((uint8_t *)packet) - HV_NV_PACKET_OFFSET_IN_BUF;

	free(buf, M_NETVSC);

	if (mb != NULL) {
		m_freem(mb);
	}
}

/*
 * Start a transmit of one or more packets
 */
static int
hn_start_locked(struct ifnet *ifp)
{
	hn_softc_t *sc = ifp->if_softc;
	struct hv_device *device_ctx = vmbus_get_devctx(sc->hn_dev);
	netvsc_dev *net_dev = sc->net_dev;
	device_t dev = device_ctx->device;
	uint8_t *buf;
	netvsc_packet *packet;
	struct mbuf *m_head, *m;
	struct mbuf *mc_head = NULL;
	struct ether_vlan_header *eh;
	rndis_msg *rndis_mesg;
	rndis_packet *rndis_pkt;
	rndis_per_packet_info *rppi;
	ndis_8021q_info *rppi_vlan_info;
	rndis_tcp_ip_csum_info *csum_info;
	rndis_tcp_tso_info *tso_info;	
	int ether_len;
	int i;
	int num_frags;
	int len;
	int retries = 0;
	int ret = 0;	
	uint32_t rndis_msg_size = 0;
	uint32_t trans_proto_type;
	uint32_t send_buf_section_idx =
	    NVSP_1_CHIMNEY_SEND_INVALID_SECTION_INDEX;

	while (!IFQ_DRV_IS_EMPTY(&sc->hn_ifp->if_snd)) {
		IFQ_DRV_DEQUEUE(&sc->hn_ifp->if_snd, m_head);
		if (m_head == NULL) {
			break;
		}

		len = 0;
		num_frags = 0;

		/* Walk the mbuf list computing total length and num frags */
		for (m = m_head; m != NULL; m = m->m_next) {
			if (m->m_len != 0) {
				num_frags++;
				len += m->m_len;
			}
		}

		/*
		 * Reserve the number of pages requested.  Currently,
		 * one page is reserved for the message in the RNDIS
		 * filter packet
		 */
		num_frags += HV_RF_NUM_TX_RESERVED_PAGE_BUFS;

		/* If exceeds # page_buffers in netvsc_packet */
		if (num_frags > NETVSC_PACKET_MAXPAGE) {
			device_printf(dev, "exceed max page buffers,%d,%d\n",
			    num_frags, NETVSC_PACKET_MAXPAGE);
			m_freem(m_head);
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			return (EINVAL);
		}

		/*
		 * Allocate a buffer with space for a netvsc packet plus a
		 * number of reserved areas.  First comes a (currently 16
		 * bytes, currently unused) reserved data area.  Second is
		 * the netvsc_packet. Third is an area reserved for an 
		 * rndis_filter_packet struct. Fourth (optional) is a 
		 * rndis_per_packet_info struct.
		 * Changed malloc to M_NOWAIT to avoid sleep under spin lock.
		 * No longer reserving extra space for page buffers, as they
		 * are already part of the netvsc_packet.
		 */
		buf = malloc(HV_NV_PACKET_OFFSET_IN_BUF +
			sizeof(netvsc_packet) + 
			sizeof(rndis_msg) +
			RNDIS_VLAN_PPI_SIZE +
			RNDIS_TSO_PPI_SIZE +
			RNDIS_CSUM_PPI_SIZE,
			M_NETVSC, M_ZERO | M_NOWAIT);
		if (buf == NULL) {
			device_printf(dev, "hn:malloc packet failed\n");
			m_freem(m_head);
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			return (ENOMEM);
		}

		packet = (netvsc_packet *)(buf + HV_NV_PACKET_OFFSET_IN_BUF);
		*(vm_offset_t *)buf = HV_NV_SC_PTR_OFFSET_IN_BUF;

		packet->is_data_pkt = TRUE;

		/* Set up the rndis header */
		packet->page_buf_count = num_frags;

		/* Initialize it from the mbuf */
		packet->tot_data_buf_len = len;

		/*
		 * extension points to the area reserved for the
		 * rndis_filter_packet, which is placed just after
		 * the netvsc_packet (and rppi struct, if present;
		 * length is updated later).
		 */
		packet->rndis_mesg = packet + 1;
		rndis_mesg = (rndis_msg *)packet->rndis_mesg;
		rndis_mesg->ndis_msg_type = REMOTE_NDIS_PACKET_MSG;

		rndis_pkt = &rndis_mesg->msg.packet;
		rndis_pkt->data_offset = sizeof(rndis_packet);
		rndis_pkt->data_length = packet->tot_data_buf_len;
		rndis_pkt->per_pkt_info_offset = sizeof(rndis_packet);

		rndis_msg_size = RNDIS_MESSAGE_SIZE(rndis_packet);

		/*
		 * If the Hyper-V infrastructure needs to embed a VLAN tag,
		 * initialize netvsc_packet and rppi struct values as needed.
		 */
		if (m_head->m_flags & M_VLANTAG) {
			/*
			 * set up some additional fields so the Hyper-V infrastructure will stuff the VLAN tag
			 * into the frame.
			 */
			packet->vlan_tci = m_head->m_pkthdr.ether_vtag;

			rndis_msg_size += RNDIS_VLAN_PPI_SIZE;

			rppi = hv_set_rppi_data(rndis_mesg, RNDIS_VLAN_PPI_SIZE,
			    ieee_8021q_info);
		
			/* VLAN info immediately follows rppi struct */
			rppi_vlan_info = (ndis_8021q_info *)((char*)rppi + 
			    rppi->per_packet_info_offset);
			/* FreeBSD does not support CFI or priority */
			rppi_vlan_info->u1.s1.vlan_id =
			    packet->vlan_tci & 0xfff;
		}

		if (0 == m_head->m_pkthdr.csum_flags) {
			goto pre_send;
		}

		eh = mtod(m_head, struct ether_vlan_header*);
		if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
			ether_len = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		} else {
			ether_len = ETHER_HDR_LEN;
		}

		trans_proto_type = get_transport_proto_type(m_head);
		if (TRANSPORT_TYPE_NOT_IP == trans_proto_type) {
			goto pre_send;
		}

		/*
		 * TSO packet needless to setup the send side checksum
		 * offload.
		 */
		if (m_head->m_pkthdr.csum_flags & CSUM_TSO) {
			goto do_tso;
		}

		/* setup checksum offload */
		rndis_msg_size += RNDIS_CSUM_PPI_SIZE;
		rppi = hv_set_rppi_data(rndis_mesg, RNDIS_CSUM_PPI_SIZE,
		    tcpip_chksum_info);
		csum_info = (rndis_tcp_ip_csum_info *)((char*)rppi +
		    rppi->per_packet_info_offset);

		if (trans_proto_type & (TYPE_IPV4 << 16)) {
			csum_info->xmit.is_ipv4 = 1;
		} else {
			csum_info->xmit.is_ipv6 = 1;
		}

		if (trans_proto_type & TYPE_TCP) {
			csum_info->xmit.tcp_csum = 1;
			csum_info->xmit.tcp_header_offset = 0;
		} else if (trans_proto_type & TYPE_UDP) {
			csum_info->xmit.udp_csum = 1;
		}

		goto pre_send;

do_tso:
		/* setup TCP segmentation offload */
		rndis_msg_size += RNDIS_TSO_PPI_SIZE;
		rppi = hv_set_rppi_data(rndis_mesg, RNDIS_TSO_PPI_SIZE,
		    tcp_large_send_info);
		
		tso_info = (rndis_tcp_tso_info *)((char *)rppi +
		    rppi->per_packet_info_offset);
		tso_info->lso_v2_xmit.type =
		    RNDIS_TCP_LARGE_SEND_OFFLOAD_V2_TYPE;
		
#ifdef INET
		if (trans_proto_type & (TYPE_IPV4 << 16)) {
			struct ip *ip =
			    (struct ip *)(m_head->m_data + ether_len);
			unsigned long iph_len = ip->ip_hl << 2;
			struct tcphdr *th =
			    (struct tcphdr *)((caddr_t)ip + iph_len);
		
			tso_info->lso_v2_xmit.ip_version =
			    RNDIS_TCP_LARGE_SEND_OFFLOAD_IPV4;
			ip->ip_len = 0;
			ip->ip_sum = 0;
		
			th->th_sum = in_pseudo(ip->ip_src.s_addr,
			    ip->ip_dst.s_addr,
			    htons(IPPROTO_TCP));
		}
#endif
#if defined(INET6) && defined(INET)
		else
#endif
#ifdef INET6
		{
			struct ip6_hdr *ip6 =
			    (struct ip6_hdr *)(m_head->m_data + ether_len);
			struct tcphdr *th = (struct tcphdr *)(ip6 + 1);

			tso_info->lso_v2_xmit.ip_version =
			    RNDIS_TCP_LARGE_SEND_OFFLOAD_IPV6;
			ip6->ip6_plen = 0;
			th->th_sum = in6_cksum_pseudo(ip6, 0, IPPROTO_TCP, 0);
		}
#endif
		tso_info->lso_v2_xmit.tcp_header_offset = 0;
		tso_info->lso_v2_xmit.mss = m_head->m_pkthdr.tso_segsz;

pre_send:
		rndis_mesg->msg_len = packet->tot_data_buf_len + rndis_msg_size;
		packet->tot_data_buf_len = rndis_mesg->msg_len;

		/* send packet with send buffer */
		if (packet->tot_data_buf_len < net_dev->send_section_size) {
			send_buf_section_idx =
			    hv_nv_get_next_send_section(net_dev);
			if (send_buf_section_idx !=
			    NVSP_1_CHIMNEY_SEND_INVALID_SECTION_INDEX) {
				char *dest = ((char *)net_dev->send_buf +
				    send_buf_section_idx *
				    net_dev->send_section_size);

				memcpy(dest, rndis_mesg, rndis_msg_size);
				dest += rndis_msg_size;
				for (m = m_head; m != NULL; m = m->m_next) {
					if (m->m_len) {
						memcpy(dest,
						    (void *)mtod(m, vm_offset_t),
						    m->m_len);
						dest += m->m_len;
					}
				}

				packet->send_buf_section_idx =
				    send_buf_section_idx;
				packet->send_buf_section_size =
				    packet->tot_data_buf_len;
				packet->page_buf_count = 0;
				goto do_send;
			}
		}

		/* send packet with page buffer */
		packet->page_buffers[0].pfn =
		    atop(hv_get_phys_addr(rndis_mesg));
		packet->page_buffers[0].offset =
		    (unsigned long)rndis_mesg & PAGE_MASK;
		packet->page_buffers[0].length = rndis_msg_size;

		/*
		 * Fill the page buffers with mbuf info starting at index
		 * HV_RF_NUM_TX_RESERVED_PAGE_BUFS.
		 */
		i = HV_RF_NUM_TX_RESERVED_PAGE_BUFS;
		for (m = m_head; m != NULL; m = m->m_next) {
			if (m->m_len) {
				vm_offset_t paddr =
				    vtophys(mtod(m, vm_offset_t));
				packet->page_buffers[i].pfn =
				    paddr >> PAGE_SHIFT;
				packet->page_buffers[i].offset =
				    paddr & (PAGE_SIZE - 1);
				packet->page_buffers[i].length = m->m_len;
				i++;
			}
		}

		packet->send_buf_section_idx = 
		    NVSP_1_CHIMNEY_SEND_INVALID_SECTION_INDEX;
		packet->send_buf_section_size = 0;

do_send:

		/*
		 * If bpf, copy the mbuf chain.  This is less expensive than
		 * it appears; the mbuf clusters are not copied, only their
		 * reference counts are incremented.
		 * Needed to avoid a race condition where the completion
		 * callback is invoked, freeing the mbuf chain, before the
		 * bpf_mtap code has a chance to run.
		 */
		if (ifp->if_bpf) {
			mc_head = m_copypacket(m_head, M_NOWAIT);
		}
retry_send:
		/* Set the completion routine */
		packet->compl.send.on_send_completion = netvsc_xmit_completion;
		packet->compl.send.send_completion_context = packet;
		packet->compl.send.send_completion_tid = (uint64_t)(uintptr_t)m_head;

		/* Removed critical_enter(), does not appear necessary */
		ret = hv_nv_on_send(device_ctx, packet);
		if (ret == 0) {
			if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
			/* if bpf && mc_head, call bpf_mtap code */
			if (mc_head) {
				ETHER_BPF_MTAP(ifp, mc_head);
			}
		} else {
			retries++;
			if (retries < 4) {
				goto retry_send;
			}

			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;

			/*
			 * Null the mbuf pointer so the completion function
			 * does not free the mbuf chain.  We just pushed the
			 * mbuf chain back on the if_snd queue.
			 */
			packet->compl.send.send_completion_tid = 0;

			/*
			 * Release the resources since we will not get any
			 * send completion
			 */
			netvsc_xmit_completion(packet);
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		}

		/* if bpf && mc_head, free the mbuf chain copy */
		if (mc_head) {
			m_freem(mc_head);
		}
	}

	return (ret);
}

/*
 * Link up/down notification
 */
void
netvsc_linkstatus_callback(struct hv_device *device_obj, uint32_t status)
{
	hn_softc_t *sc = device_get_softc(device_obj->device);

	if (sc == NULL) {
		return;
	}

	if (status == 1) {
		sc->hn_carrier = 1;
	} else {
		sc->hn_carrier = 0;
	}
}

/*
 * Append the specified data to the indicated mbuf chain,
 * Extend the mbuf chain if the new data does not fit in
 * existing space.
 *
 * This is a minor rewrite of m_append() from sys/kern/uipc_mbuf.c.
 * There should be an equivalent in the kernel mbuf code,
 * but there does not appear to be one yet.
 *
 * Differs from m_append() in that additional mbufs are
 * allocated with cluster size MJUMPAGESIZE, and filled
 * accordingly.
 *
 * Return 1 if able to complete the job; otherwise 0.
 */
static int
hv_m_append(struct mbuf *m0, int len, c_caddr_t cp)
{
	struct mbuf *m, *n;
	int remainder, space;

	for (m = m0; m->m_next != NULL; m = m->m_next)
		;
	remainder = len;
	space = M_TRAILINGSPACE(m);
	if (space > 0) {
		/*
		 * Copy into available space.
		 */
		if (space > remainder)
			space = remainder;
		bcopy(cp, mtod(m, caddr_t) + m->m_len, space);
		m->m_len += space;
		cp += space;
		remainder -= space;
	}
	while (remainder > 0) {
		/*
		 * Allocate a new mbuf; could check space
		 * and allocate a cluster instead.
		 */
		n = m_getjcl(M_NOWAIT, m->m_type, 0, MJUMPAGESIZE);
		if (n == NULL)
			break;
		n->m_len = min(MJUMPAGESIZE, remainder);
		bcopy(cp, mtod(n, caddr_t), n->m_len);
		cp += n->m_len;
		remainder -= n->m_len;
		m->m_next = n;
		m = n;
	}
	if (m0->m_flags & M_PKTHDR)
		m0->m_pkthdr.len += len - remainder;

	return (remainder == 0);
}


/*
 * Called when we receive a data packet from the "wire" on the
 * specified device
 *
 * Note:  This is no longer used as a callback
 */
int
netvsc_recv(struct hv_device *device_ctx, netvsc_packet *packet,
    rndis_tcp_ip_csum_info *csum_info)
{
	hn_softc_t *sc = (hn_softc_t *)device_get_softc(device_ctx->device);
	struct mbuf *m_new;
	struct ifnet *ifp;
	device_t dev = device_ctx->device;
	int size;

	if (sc == NULL) {
		return (0); /* TODO: KYS how can this be! */
	}

	ifp = sc->hn_ifp;

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		return (0);
	}

	/*
	 * Bail out if packet contains more data than configured MTU.
	 */
	if (packet->tot_data_buf_len > (ifp->if_mtu + ETHER_HDR_LEN)) {
		return (0);
	}

	/*
	 * Get an mbuf with a cluster.  For packets 2K or less,
	 * get a standard 2K cluster.  For anything larger, get a
	 * 4K cluster.  Any buffers larger than 4K can cause problems
	 * if looped around to the Hyper-V TX channel, so avoid them.
	 */
	size = MCLBYTES;

	if (packet->tot_data_buf_len > MCLBYTES) {
		/* 4096 */
		size = MJUMPAGESIZE;
	}

	m_new = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, size);

	if (m_new == NULL) {
		device_printf(dev, "alloc mbuf failed.\n");
		return (0);
	}

	hv_m_append(m_new, packet->tot_data_buf_len,
			packet->data);

	m_new->m_pkthdr.rcvif = ifp;

	/* receive side checksum offload */
	m_new->m_pkthdr.csum_flags = 0;
	if (NULL != csum_info) {
		/* IP csum offload */
		if (csum_info->receive.ip_csum_succeeded) {
			m_new->m_pkthdr.csum_flags |=
			    (CSUM_IP_CHECKED | CSUM_IP_VALID);
		}

		/* TCP csum offload */
		if (csum_info->receive.tcp_csum_succeeded) {
			m_new->m_pkthdr.csum_flags |=
			    (CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
			m_new->m_pkthdr.csum_data = 0xffff;
		}
	}

	if ((packet->vlan_tci != 0) &&
	    (ifp->if_capenable & IFCAP_VLAN_HWTAGGING) != 0) {
		m_new->m_pkthdr.ether_vtag = packet->vlan_tci;
		m_new->m_flags |= M_VLANTAG;
	}

	/*
	 * Note:  Moved RX completion back to hv_nv_on_receive() so all
	 * messages (not just data messages) will trigger a response.
	 */

	if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);

	/* We're not holding the lock here, so don't release it */
	(*ifp->if_input)(ifp, m_new);

	return (0);
}

/*
 * Rules for using sc->temp_unusable:
 * 1.  sc->temp_unusable can only be read or written while holding NV_LOCK()
 * 2.  code reading sc->temp_unusable under NV_LOCK(), and finding 
 *     sc->temp_unusable set, must release NV_LOCK() and exit
 * 3.  to retain exclusive control of the interface,
 *     sc->temp_unusable must be set by code before releasing NV_LOCK()
 * 4.  only code setting sc->temp_unusable can clear sc->temp_unusable
 * 5.  code setting sc->temp_unusable must eventually clear sc->temp_unusable
 */

/*
 * Standard ioctl entry point.  Called when the user wants to configure
 * the interface.
 */
static int
hn_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	hn_softc_t *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
#ifdef INET
	struct ifaddr *ifa = (struct ifaddr *)data;
#endif
	netvsc_device_info device_info;
	struct hv_device *hn_dev;
	int mask, error = 0;
	int retry_cnt = 500;
	
	switch(cmd) {

	case SIOCSIFADDR:
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET) {
			ifp->if_flags |= IFF_UP;
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
				hn_ifinit(sc);
			arp_ifinit(ifp, ifa);
		} else
#endif
		error = ether_ioctl(ifp, cmd, data);
		break;
	case SIOCSIFMTU:
		hn_dev = vmbus_get_devctx(sc->hn_dev);

		/* Check MTU value change */
		if (ifp->if_mtu == ifr->ifr_mtu)
			break;

		if (ifr->ifr_mtu > NETVSC_MAX_CONFIGURABLE_MTU) {
			error = EINVAL;
			break;
		}

		/* Obtain and record requested MTU */
		ifp->if_mtu = ifr->ifr_mtu;
 		
		do {
			NV_LOCK(sc);
			if (!sc->temp_unusable) {
				sc->temp_unusable = TRUE;
				retry_cnt = -1;
			}
			NV_UNLOCK(sc);
			if (retry_cnt > 0) {
				retry_cnt--;
				DELAY(5 * 1000);
			}
		} while (retry_cnt > 0);

		if (retry_cnt == 0) {
			error = EINVAL;
			break;
		}

		/* We must remove and add back the device to cause the new
		 * MTU to take effect.  This includes tearing down, but not
		 * deleting the channel, then bringing it back up.
		 */
		error = hv_rf_on_device_remove(hn_dev, HV_RF_NV_RETAIN_CHANNEL);
		if (error) {
			NV_LOCK(sc);
			sc->temp_unusable = FALSE;
			NV_UNLOCK(sc);
			break;
		}
		error = hv_rf_on_device_add(hn_dev, &device_info);
		if (error) {
			NV_LOCK(sc);
			sc->temp_unusable = FALSE;
			NV_UNLOCK(sc);
			break;
		}

		hn_ifinit_locked(sc);

		NV_LOCK(sc);
		sc->temp_unusable = FALSE;
		NV_UNLOCK(sc);
		break;
	case SIOCSIFFLAGS:
		do {
                       NV_LOCK(sc);
                       if (!sc->temp_unusable) {
                               sc->temp_unusable = TRUE;
                               retry_cnt = -1;
                       }
                       NV_UNLOCK(sc);
                       if (retry_cnt > 0) {
                      	        retry_cnt--;
                        	DELAY(5 * 1000);
                       }
                } while (retry_cnt > 0);

                if (retry_cnt == 0) {
                       error = EINVAL;
                       break;
                }

		if (ifp->if_flags & IFF_UP) {
			/*
			 * If only the state of the PROMISC flag changed,
			 * then just use the 'set promisc mode' command
			 * instead of reinitializing the entire NIC. Doing
			 * a full re-init means reloading the firmware and
			 * waiting for it to start up, which may take a
			 * second or two.
			 */
#ifdef notyet
			/* Fixme:  Promiscuous mode? */
			if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->hn_if_flags & IFF_PROMISC)) {
				/* do something here for Hyper-V */
			} else if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->hn_if_flags & IFF_PROMISC) {
				/* do something here for Hyper-V */
			} else
#endif
				hn_ifinit_locked(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				hn_stop(sc);
			}
		}
		NV_LOCK(sc);
		sc->temp_unusable = FALSE;
		NV_UNLOCK(sc);
		sc->hn_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCSIFCAP:
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		if (mask & IFCAP_TXCSUM) {
			if (IFCAP_TXCSUM & ifp->if_capenable) {
				ifp->if_capenable &= ~IFCAP_TXCSUM;
				ifp->if_hwassist &= ~(CSUM_TCP | CSUM_UDP);
			} else {
				ifp->if_capenable |= IFCAP_TXCSUM;
				ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
			}
		}

		if (mask & IFCAP_RXCSUM) {
			if (IFCAP_RXCSUM & ifp->if_capenable) {
				ifp->if_capenable &= ~IFCAP_RXCSUM;
			} else {
				ifp->if_capenable |= IFCAP_RXCSUM;
			}
		}

		if (mask & IFCAP_TSO4) {
			ifp->if_capenable ^= IFCAP_TSO4;
			ifp->if_hwassist ^= CSUM_IP_TSO;
		}

		if (mask & IFCAP_TSO6) {
			ifp->if_capenable ^= IFCAP_TSO6;
			ifp->if_hwassist ^= CSUM_IP6_TSO;
		}

		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
#ifdef notyet
		/* Fixme:  Multicast mode? */
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			NV_LOCK(sc);
			netvsc_setmulti(sc);
			NV_UNLOCK(sc);
			error = 0;
		}
#endif
		/* FALLTHROUGH */
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = EINVAL;
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (error);
}

/*
 *
 */
static void
hn_stop(hn_softc_t *sc)
{
	struct ifnet *ifp;
	int ret;
	struct hv_device *device_ctx = vmbus_get_devctx(sc->hn_dev);

	ifp = sc->hn_ifp;

	if (bootverbose)
		printf(" Closing Device ...\n");

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	if_link_state_change(ifp, LINK_STATE_DOWN);
	sc->hn_initdone = 0;

	ret = hv_rf_on_close(device_ctx);
}

/*
 * FreeBSD transmit entry point
 */
static void
hn_start(struct ifnet *ifp)
{
	hn_softc_t *sc;

	sc = ifp->if_softc;
	NV_LOCK(sc);
	if (sc->temp_unusable) {
		NV_UNLOCK(sc);
		return;
	}
	hn_start_locked(ifp);
	NV_UNLOCK(sc);
}

/*
 *
 */
static void
hn_ifinit_locked(hn_softc_t *sc)
{
	struct ifnet *ifp;
	struct hv_device *device_ctx = vmbus_get_devctx(sc->hn_dev);
	int ret;

	ifp = sc->hn_ifp;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		return;
	}

	hv_promisc_mode = 1;

	ret = hv_rf_on_open(device_ctx);
	if (ret != 0) {
		return;
	} else {
		sc->hn_initdone = 1;
	}
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	if_link_state_change(ifp, LINK_STATE_UP);
}

/*
 *
 */
static void
hn_ifinit(void *xsc)
{
	hn_softc_t *sc = xsc;

	NV_LOCK(sc);
	if (sc->temp_unusable) {
		NV_UNLOCK(sc);
		return;
	}
	sc->temp_unusable = TRUE;
	NV_UNLOCK(sc);

	hn_ifinit_locked(sc);

	NV_LOCK(sc);
	sc->temp_unusable = FALSE;
	NV_UNLOCK(sc);
}

#ifdef LATER
/*
 *
 */
static void
hn_watchdog(struct ifnet *ifp)
{
	hn_softc_t *sc;
	sc = ifp->if_softc;

	printf("hn%d: watchdog timeout -- resetting\n", sc->hn_unit);
	hn_ifinit(sc);    /*???*/
	if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
}
#endif

static device_method_t netvsc_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe,         netvsc_probe),
        DEVMETHOD(device_attach,        netvsc_attach),
        DEVMETHOD(device_detach,        netvsc_detach),
        DEVMETHOD(device_shutdown,      netvsc_shutdown),

        { 0, 0 }
};

static driver_t netvsc_driver = {
        NETVSC_DEVNAME,
        netvsc_methods,
        sizeof(hn_softc_t)
};

static devclass_t netvsc_devclass;

DRIVER_MODULE(hn, vmbus, netvsc_driver, netvsc_devclass, 0, 0);
MODULE_VERSION(hn, 1);
MODULE_DEPEND(hn, vmbus, 1, 1, 1);
SYSINIT(netvsc_initx, SI_SUB_KTHREAD_IDLE, SI_ORDER_MIDDLE + 1, netvsc_init,
     NULL);

