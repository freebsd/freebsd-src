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
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <net/if_types.h>
#include <net/if_vlan_var.h>
#include <net/if.h>

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

/* YYY should get it from the underlying channel */
#define HN_TX_DESC_CNT			512

#define HN_LROENT_CNT_DEF		128

#define HN_RNDIS_MSG_LEN		\
    (sizeof(rndis_msg) +		\
     RNDIS_VLAN_PPI_SIZE +		\
     RNDIS_TSO_PPI_SIZE +		\
     RNDIS_CSUM_PPI_SIZE)
#define HN_RNDIS_MSG_BOUNDARY		PAGE_SIZE
#define HN_RNDIS_MSG_ALIGN		CACHE_LINE_SIZE

#define HN_TX_DATA_BOUNDARY		PAGE_SIZE
#define HN_TX_DATA_MAXSIZE		IP_MAXPACKET
#define HN_TX_DATA_SEGSIZE		PAGE_SIZE
#define HN_TX_DATA_SEGCNT_MAX		\
    (NETVSC_PACKET_MAXPAGE - HV_RF_NUM_TX_RESERVED_PAGE_BUFS)

#define HN_DIRECT_TX_SIZE_DEF		128

struct hn_txdesc {
	SLIST_ENTRY(hn_txdesc) link;
	struct mbuf	*m;
	struct hn_tx_ring *txr;
	int		refs;
	uint32_t	flags;		/* HN_TXD_FLAG_ */
	netvsc_packet	netvsc_pkt;	/* XXX to be removed */

	bus_dmamap_t	data_dmap;

	bus_addr_t	rndis_msg_paddr;
	rndis_msg	*rndis_msg;
	bus_dmamap_t	rndis_msg_dmap;
};

#define HN_TXD_FLAG_ONLIST	0x1
#define HN_TXD_FLAG_DMAMAP	0x2

/*
 * Only enable UDP checksum offloading when it is on 2012R2 or
 * later.  UDP checksum offloading doesn't work on earlier
 * Windows releases.
 */
#define HN_CSUM_ASSIST_WIN8	(CSUM_TCP)
#define HN_CSUM_ASSIST		(CSUM_IP | CSUM_UDP | CSUM_TCP)

#define HN_LRO_LENLIM_DEF		(25 * ETHERMTU)
/* YYY 2*MTU is a bit rough, but should be good enough. */
#define HN_LRO_LENLIM_MIN(ifp)		(2 * (ifp)->if_mtu)

#define HN_LRO_ACKCNT_DEF		1

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

SYSCTL_NODE(_hw, OID_AUTO, hn, CTLFLAG_RD, NULL, "Hyper-V network interface");

/* Trust tcp segements verification on host side. */
static int hn_trust_hosttcp = 1;
SYSCTL_INT(_hw_hn, OID_AUTO, trust_hosttcp, CTLFLAG_RDTUN,
    &hn_trust_hosttcp, 0,
    "Trust tcp segement verification on host side, "
    "when csum info is missing (global setting)");

/* Trust udp datagrams verification on host side. */
static int hn_trust_hostudp = 1;
SYSCTL_INT(_hw_hn, OID_AUTO, trust_hostudp, CTLFLAG_RDTUN,
    &hn_trust_hostudp, 0,
    "Trust udp datagram verification on host side, "
    "when csum info is missing (global setting)");

/* Trust ip packets verification on host side. */
static int hn_trust_hostip = 1;
SYSCTL_INT(_hw_hn, OID_AUTO, trust_hostip, CTLFLAG_RDTUN,
    &hn_trust_hostip, 0,
    "Trust ip packet verification on host side, "
    "when csum info is missing (global setting)");

#if __FreeBSD_version >= 1100045
/* Limit TSO burst size */
static int hn_tso_maxlen = 0;
SYSCTL_INT(_hw_hn, OID_AUTO, tso_maxlen, CTLFLAG_RDTUN,
    &hn_tso_maxlen, 0, "TSO burst limit");
#endif

/* Limit chimney send size */
static int hn_tx_chimney_size = 0;
SYSCTL_INT(_hw_hn, OID_AUTO, tx_chimney_size, CTLFLAG_RDTUN,
    &hn_tx_chimney_size, 0, "Chimney send packet size limit");

/* Limit the size of packet for direct transmission */
static int hn_direct_tx_size = HN_DIRECT_TX_SIZE_DEF;
SYSCTL_INT(_hw_hn, OID_AUTO, direct_tx_size, CTLFLAG_RDTUN,
    &hn_direct_tx_size, 0, "Size of the packet for direct transmission");

#if defined(INET) || defined(INET6)
#if __FreeBSD_version >= 1100095
static int hn_lro_entry_count = HN_LROENT_CNT_DEF;
SYSCTL_INT(_hw_hn, OID_AUTO, lro_entry_count, CTLFLAG_RDTUN,
    &hn_lro_entry_count, 0, "LRO entry count");
#endif
#endif

static int hn_share_tx_taskq = 0;
SYSCTL_INT(_hw_hn, OID_AUTO, share_tx_taskq, CTLFLAG_RDTUN,
    &hn_share_tx_taskq, 0, "Enable shared TX taskqueue");

static struct taskqueue	*hn_tx_taskq;

/*
 * Forward declarations
 */
static void hn_stop(hn_softc_t *sc);
static void hn_ifinit_locked(hn_softc_t *sc);
static void hn_ifinit(void *xsc);
static int  hn_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static int hn_start_locked(struct hn_tx_ring *txr, int len);
static void hn_start(struct ifnet *ifp);
static void hn_start_txeof(struct hn_tx_ring *);
static int hn_ifmedia_upd(struct ifnet *ifp);
static void hn_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr);
#if __FreeBSD_version >= 1100099
static int hn_lro_lenlim_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_lro_ackcnt_sysctl(SYSCTL_HANDLER_ARGS);
#endif
static int hn_trust_hcsum_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_tx_chimney_size_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_rx_stat_ulong_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_rx_stat_u64_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_tx_stat_ulong_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_tx_conf_int_sysctl(SYSCTL_HANDLER_ARGS);
static int hn_check_iplen(const struct mbuf *, int);
static int hn_create_tx_ring(struct hn_softc *, int);
static void hn_destroy_tx_ring(struct hn_tx_ring *);
static int hn_create_tx_data(struct hn_softc *);
static void hn_destroy_tx_data(struct hn_softc *);
static void hn_start_taskfunc(void *xsc, int pending);
static void hn_txeof_taskfunc(void *xsc, int pending);
static void hn_stop_tx_tasks(struct hn_softc *);
static int hn_encap(struct hn_tx_ring *, struct hn_txdesc *, struct mbuf **);
static void hn_create_rx_data(struct hn_softc *sc);
static void hn_destroy_rx_data(struct hn_softc *sc);
static void hn_set_tx_chimney_size(struct hn_softc *, int);

static int
hn_ifmedia_upd(struct ifnet *ifp __unused)
{

	return EOPNOTSUPP;
}

static void
hn_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct hn_softc *sc = ifp->if_softc;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!sc->hn_carrier) {
		ifmr->ifm_active |= IFM_NONE;
		return;
	}
	ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active |= IFM_10G_T | IFM_FDX;
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
	struct ifnet *ifp = NULL;
	int error;
#if __FreeBSD_version >= 1100045
	int tso_maxlen;
#endif

	sc = device_get_softc(dev);
	if (sc == NULL) {
		return (ENOMEM);
	}

	bzero(sc, sizeof(hn_softc_t));
	sc->hn_unit = unit;
	sc->hn_dev = dev;

	if (hn_tx_taskq == NULL) {
		sc->hn_tx_taskq = taskqueue_create("hn_tx", M_WAITOK,
		    taskqueue_thread_enqueue, &sc->hn_tx_taskq);
		taskqueue_start_threads(&sc->hn_tx_taskq, 1, PI_NET, "%s tx",
		    device_get_nameunit(dev));
	} else {
		sc->hn_tx_taskq = hn_tx_taskq;
	}
	NV_LOCK_INIT(sc, "NetVSCLock");

	sc->hn_dev_obj = device_ctx;

	ifp = sc->hn_ifp = sc->arpcom.ac_ifp = if_alloc(IFT_ETHER);
	ifp->if_softc = sc;

	error = hn_create_tx_data(sc);
	if (error)
		goto failed;

	hn_create_rx_data(sc);

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

	ifmedia_init(&sc->hn_media, 0, hn_ifmedia_upd, hn_ifmedia_sts);
	ifmedia_add(&sc->hn_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->hn_media, IFM_ETHER | IFM_AUTO);
	/* XXX ifmedia_set really should do this for us */
	sc->hn_media.ifm_media = sc->hn_media.ifm_cur->ifm_media;

	/*
	 * Tell upper layers that we support full VLAN capability.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);
	ifp->if_capabilities |=
	    IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU | IFCAP_HWCSUM | IFCAP_TSO |
	    IFCAP_LRO;
	ifp->if_capenable |=
	    IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU | IFCAP_HWCSUM | IFCAP_TSO |
	    IFCAP_LRO;
	ifp->if_hwassist = sc->hn_tx_ring[0].hn_csum_assist | CSUM_TSO;

	error = hv_rf_on_device_add(device_ctx, &device_info);
	if (error)
		goto failed;

	if (device_info.link_state == 0) {
		sc->hn_carrier = 1;
	}

#if __FreeBSD_version >= 1100045
	tso_maxlen = hn_tso_maxlen;
	if (tso_maxlen <= 0 || tso_maxlen > IP_MAXPACKET)
		tso_maxlen = IP_MAXPACKET;

	ifp->if_hw_tsomaxsegcount = HN_TX_DATA_SEGCNT_MAX;
	ifp->if_hw_tsomaxsegsize = PAGE_SIZE;
	ifp->if_hw_tsomax = tso_maxlen -
	    (ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN);
#endif

	ether_ifattach(ifp, device_info.mac_addr);

#if __FreeBSD_version >= 1100045
	if_printf(ifp, "TSO: %u/%u/%u\n", ifp->if_hw_tsomax,
	    ifp->if_hw_tsomaxsegcount, ifp->if_hw_tsomaxsegsize);
#endif

	sc->hn_tx_chimney_max = sc->net_dev->send_section_size;
	hn_set_tx_chimney_size(sc, sc->hn_tx_chimney_max);
	if (hn_tx_chimney_size > 0 &&
	    hn_tx_chimney_size < sc->hn_tx_chimney_max)
		hn_set_tx_chimney_size(sc, hn_tx_chimney_size);

	return (0);
failed:
	hn_destroy_tx_data(sc);
	if (ifp != NULL)
		if_free(ifp);
	return (error);
}

/*
 * Standard detach entry point
 */
static int
netvsc_detach(device_t dev)
{
	struct hn_softc *sc = device_get_softc(dev);
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

	hn_stop_tx_tasks(sc);

	ifmedia_removeall(&sc->hn_media);
	hn_destroy_rx_data(sc);
	hn_destroy_tx_data(sc);

	if (sc->hn_tx_taskq != hn_tx_taskq)
		taskqueue_free(sc->hn_tx_taskq);

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

static __inline int
hn_txdesc_dmamap_load(struct hn_tx_ring *txr, struct hn_txdesc *txd,
    struct mbuf **m_head, bus_dma_segment_t *segs, int *nsegs)
{
	struct mbuf *m = *m_head;
	int error;

	error = bus_dmamap_load_mbuf_sg(txr->hn_tx_data_dtag, txd->data_dmap,
	    m, segs, nsegs, BUS_DMA_NOWAIT);
	if (error == EFBIG) {
		struct mbuf *m_new;

		m_new = m_collapse(m, M_NOWAIT, HN_TX_DATA_SEGCNT_MAX);
		if (m_new == NULL)
			return ENOBUFS;
		else
			*m_head = m = m_new;
		txr->hn_tx_collapsed++;

		error = bus_dmamap_load_mbuf_sg(txr->hn_tx_data_dtag,
		    txd->data_dmap, m, segs, nsegs, BUS_DMA_NOWAIT);
	}
	if (!error) {
		bus_dmamap_sync(txr->hn_tx_data_dtag, txd->data_dmap,
		    BUS_DMASYNC_PREWRITE);
		txd->flags |= HN_TXD_FLAG_DMAMAP;
	}
	return error;
}

static __inline void
hn_txdesc_dmamap_unload(struct hn_tx_ring *txr, struct hn_txdesc *txd)
{

	if (txd->flags & HN_TXD_FLAG_DMAMAP) {
		bus_dmamap_sync(txr->hn_tx_data_dtag,
		    txd->data_dmap, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(txr->hn_tx_data_dtag,
		    txd->data_dmap);
		txd->flags &= ~HN_TXD_FLAG_DMAMAP;
	}
}

static __inline int
hn_txdesc_put(struct hn_tx_ring *txr, struct hn_txdesc *txd)
{

	KASSERT((txd->flags & HN_TXD_FLAG_ONLIST) == 0,
	    ("put an onlist txd %#x", txd->flags));

	KASSERT(txd->refs > 0, ("invalid txd refs %d", txd->refs));
	if (atomic_fetchadd_int(&txd->refs, -1) != 1)
		return 0;

	hn_txdesc_dmamap_unload(txr, txd);
	if (txd->m != NULL) {
		m_freem(txd->m);
		txd->m = NULL;
	}

	txd->flags |= HN_TXD_FLAG_ONLIST;

	mtx_lock_spin(&txr->hn_txlist_spin);
	KASSERT(txr->hn_txdesc_avail >= 0 &&
	    txr->hn_txdesc_avail < txr->hn_txdesc_cnt,
	    ("txdesc_put: invalid txd avail %d", txr->hn_txdesc_avail));
	txr->hn_txdesc_avail++;
	SLIST_INSERT_HEAD(&txr->hn_txlist, txd, link);
	mtx_unlock_spin(&txr->hn_txlist_spin);

	return 1;
}

static __inline struct hn_txdesc *
hn_txdesc_get(struct hn_tx_ring *txr)
{
	struct hn_txdesc *txd;

	mtx_lock_spin(&txr->hn_txlist_spin);
	txd = SLIST_FIRST(&txr->hn_txlist);
	if (txd != NULL) {
		KASSERT(txr->hn_txdesc_avail > 0,
		    ("txdesc_get: invalid txd avail %d", txr->hn_txdesc_avail));
		txr->hn_txdesc_avail--;
		SLIST_REMOVE_HEAD(&txr->hn_txlist, link);
	}
	mtx_unlock_spin(&txr->hn_txlist_spin);

	if (txd != NULL) {
		KASSERT(txd->m == NULL && txd->refs == 0 &&
		    (txd->flags & HN_TXD_FLAG_ONLIST), ("invalid txd"));
		txd->flags &= ~HN_TXD_FLAG_ONLIST;
		txd->refs = 1;
	}
	return txd;
}

static __inline void
hn_txdesc_hold(struct hn_txdesc *txd)
{

	/* 0->1 transition will never work */
	KASSERT(txd->refs > 0, ("invalid refs %d", txd->refs));
	atomic_add_int(&txd->refs, 1);
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
	netvsc_packet *packet = context;
	struct hn_txdesc *txd;
	struct hn_tx_ring *txr;

	txd = (struct hn_txdesc *)(uintptr_t)
	    packet->compl.send.send_completion_tid;

	txr = txd->txr;
	txr->hn_txeof = 1;
	hn_txdesc_put(txr, txd);
}

void
netvsc_channel_rollup(struct hv_device *device_ctx)
{
	struct hn_softc *sc = device_get_softc(device_ctx->device);
	struct hn_tx_ring *txr = &sc->hn_tx_ring[0]; /* TODO: vRSS */
#if defined(INET) || defined(INET6)
	struct hn_rx_ring *rxr = &sc->hn_rx_ring[0]; /* TODO: vRSS */
	struct lro_ctrl *lro = &rxr->hn_lro;
	struct lro_entry *queued;

	while ((queued = SLIST_FIRST(&lro->lro_active)) != NULL) {
		SLIST_REMOVE_HEAD(&lro->lro_active, next);
		tcp_lro_flush(lro, queued);
	}
#endif

	if (!txr->hn_txeof)
		return;

	txr->hn_txeof = 0;
	hn_start_txeof(txr);
}

/*
 * NOTE:
 * If this function fails, then both txd and m_head0 will be freed.
 */
static int
hn_encap(struct hn_tx_ring *txr, struct hn_txdesc *txd, struct mbuf **m_head0)
{
	bus_dma_segment_t segs[HN_TX_DATA_SEGCNT_MAX];
	int error, nsegs, i;
	struct mbuf *m_head = *m_head0;
	netvsc_packet *packet;
	rndis_msg *rndis_mesg;
	rndis_packet *rndis_pkt;
	rndis_per_packet_info *rppi;
	uint32_t rndis_msg_size;

	packet = &txd->netvsc_pkt;
	packet->is_data_pkt = TRUE;
	packet->tot_data_buf_len = m_head->m_pkthdr.len;

	/*
	 * extension points to the area reserved for the
	 * rndis_filter_packet, which is placed just after
	 * the netvsc_packet (and rppi struct, if present;
	 * length is updated later).
	 */
	rndis_mesg = txd->rndis_msg;
	/* XXX not necessary */
	memset(rndis_mesg, 0, HN_RNDIS_MSG_LEN);
	rndis_mesg->ndis_msg_type = REMOTE_NDIS_PACKET_MSG;

	rndis_pkt = &rndis_mesg->msg.packet;
	rndis_pkt->data_offset = sizeof(rndis_packet);
	rndis_pkt->data_length = packet->tot_data_buf_len;
	rndis_pkt->per_pkt_info_offset = sizeof(rndis_packet);

	rndis_msg_size = RNDIS_MESSAGE_SIZE(rndis_packet);

	if (m_head->m_flags & M_VLANTAG) {
		ndis_8021q_info *rppi_vlan_info;

		rndis_msg_size += RNDIS_VLAN_PPI_SIZE;
		rppi = hv_set_rppi_data(rndis_mesg, RNDIS_VLAN_PPI_SIZE,
		    ieee_8021q_info);

		rppi_vlan_info = (ndis_8021q_info *)((uint8_t *)rppi +
		    rppi->per_packet_info_offset);
		rppi_vlan_info->u1.s1.vlan_id =
		    m_head->m_pkthdr.ether_vtag & 0xfff;
	}

	if (m_head->m_pkthdr.csum_flags & CSUM_TSO) {
		rndis_tcp_tso_info *tso_info;	
		struct ether_vlan_header *eh;
		int ether_len;

		/*
		 * XXX need m_pullup and use mtodo
		 */
		eh = mtod(m_head, struct ether_vlan_header*);
		if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN))
			ether_len = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		else
			ether_len = ETHER_HDR_LEN;

		rndis_msg_size += RNDIS_TSO_PPI_SIZE;
		rppi = hv_set_rppi_data(rndis_mesg, RNDIS_TSO_PPI_SIZE,
		    tcp_large_send_info);

		tso_info = (rndis_tcp_tso_info *)((uint8_t *)rppi +
		    rppi->per_packet_info_offset);
		tso_info->lso_v2_xmit.type =
		    RNDIS_TCP_LARGE_SEND_OFFLOAD_V2_TYPE;

#ifdef INET
		if (m_head->m_pkthdr.csum_flags & CSUM_IP_TSO) {
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
			    ip->ip_dst.s_addr, htons(IPPROTO_TCP));
		}
#endif
#if defined(INET6) && defined(INET)
		else
#endif
#ifdef INET6
		{
			struct ip6_hdr *ip6 = (struct ip6_hdr *)
			    (m_head->m_data + ether_len);
			struct tcphdr *th = (struct tcphdr *)(ip6 + 1);

			tso_info->lso_v2_xmit.ip_version =
			    RNDIS_TCP_LARGE_SEND_OFFLOAD_IPV6;
			ip6->ip6_plen = 0;
			th->th_sum = in6_cksum_pseudo(ip6, 0, IPPROTO_TCP, 0);
		}
#endif
		tso_info->lso_v2_xmit.tcp_header_offset = 0;
		tso_info->lso_v2_xmit.mss = m_head->m_pkthdr.tso_segsz;
	} else if (m_head->m_pkthdr.csum_flags & txr->hn_csum_assist) {
		rndis_tcp_ip_csum_info *csum_info;

		rndis_msg_size += RNDIS_CSUM_PPI_SIZE;
		rppi = hv_set_rppi_data(rndis_mesg, RNDIS_CSUM_PPI_SIZE,
		    tcpip_chksum_info);
		csum_info = (rndis_tcp_ip_csum_info *)((uint8_t *)rppi +
		    rppi->per_packet_info_offset);

		csum_info->xmit.is_ipv4 = 1;
		if (m_head->m_pkthdr.csum_flags & CSUM_IP)
			csum_info->xmit.ip_header_csum = 1;

		if (m_head->m_pkthdr.csum_flags & CSUM_TCP) {
			csum_info->xmit.tcp_csum = 1;
			csum_info->xmit.tcp_header_offset = 0;
		} else if (m_head->m_pkthdr.csum_flags & CSUM_UDP) {
			csum_info->xmit.udp_csum = 1;
		}
	}

	rndis_mesg->msg_len = packet->tot_data_buf_len + rndis_msg_size;
	packet->tot_data_buf_len = rndis_mesg->msg_len;

	/*
	 * Chimney send, if the packet could fit into one chimney buffer.
	 */
	if (packet->tot_data_buf_len < txr->hn_tx_chimney_size) {
		netvsc_dev *net_dev = txr->hn_sc->net_dev;
		uint32_t send_buf_section_idx;

		send_buf_section_idx =
		    hv_nv_get_next_send_section(net_dev);
		if (send_buf_section_idx !=
		    NVSP_1_CHIMNEY_SEND_INVALID_SECTION_INDEX) {
			uint8_t *dest = ((uint8_t *)net_dev->send_buf +
			    (send_buf_section_idx *
			     net_dev->send_section_size));

			memcpy(dest, rndis_mesg, rndis_msg_size);
			dest += rndis_msg_size;
			m_copydata(m_head, 0, m_head->m_pkthdr.len, dest);

			packet->send_buf_section_idx = send_buf_section_idx;
			packet->send_buf_section_size =
			    packet->tot_data_buf_len;
			packet->page_buf_count = 0;
			txr->hn_tx_chimney++;
			goto done;
		}
	}

	error = hn_txdesc_dmamap_load(txr, txd, &m_head, segs, &nsegs);
	if (error) {
		int freed;

		/*
		 * This mbuf is not linked w/ the txd yet, so free it now.
		 */
		m_freem(m_head);
		*m_head0 = NULL;

		freed = hn_txdesc_put(txr, txd);
		KASSERT(freed != 0,
		    ("fail to free txd upon txdma error"));

		txr->hn_txdma_failed++;
		if_inc_counter(txr->hn_sc->hn_ifp, IFCOUNTER_OERRORS, 1);
		return error;
	}
	*m_head0 = m_head;

	packet->page_buf_count = nsegs + HV_RF_NUM_TX_RESERVED_PAGE_BUFS;

	/* send packet with page buffer */
	packet->page_buffers[0].pfn = atop(txd->rndis_msg_paddr);
	packet->page_buffers[0].offset = txd->rndis_msg_paddr & PAGE_MASK;
	packet->page_buffers[0].length = rndis_msg_size;

	/*
	 * Fill the page buffers with mbuf info starting at index
	 * HV_RF_NUM_TX_RESERVED_PAGE_BUFS.
	 */
	for (i = 0; i < nsegs; ++i) {
		hv_vmbus_page_buffer *pb = &packet->page_buffers[
		    i + HV_RF_NUM_TX_RESERVED_PAGE_BUFS];

		pb->pfn = atop(segs[i].ds_addr);
		pb->offset = segs[i].ds_addr & PAGE_MASK;
		pb->length = segs[i].ds_len;
	}

	packet->send_buf_section_idx =
	    NVSP_1_CHIMNEY_SEND_INVALID_SECTION_INDEX;
	packet->send_buf_section_size = 0;
done:
	txd->m = m_head;

	/* Set the completion routine */
	packet->compl.send.on_send_completion = netvsc_xmit_completion;
	packet->compl.send.send_completion_context = packet;
	packet->compl.send.send_completion_tid = (uint64_t)(uintptr_t)txd;

	return 0;
}

/*
 * Start a transmit of one or more packets
 */
static int
hn_start_locked(struct hn_tx_ring *txr, int len)
{
	struct hn_softc *sc = txr->hn_sc;
	struct ifnet *ifp = sc->hn_ifp;
	struct hv_device *device_ctx = vmbus_get_devctx(sc->hn_dev);

	KASSERT(txr == &sc->hn_tx_ring[0], ("not the first TX ring"));
	mtx_assert(&txr->hn_tx_lock, MA_OWNED);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return 0;

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		int error, send_failed = 0;
		struct hn_txdesc *txd;
		struct mbuf *m_head;

		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (len > 0 && m_head->m_pkthdr.len > len) {
			/*
			 * This sending could be time consuming; let callers
			 * dispatch this packet sending (and sending of any
			 * following up packets) to tx taskqueue.
			 */
			IF_PREPEND(&ifp->if_snd, m_head);
			return 1;
		}

		txd = hn_txdesc_get(txr);
		if (txd == NULL) {
			txr->hn_no_txdescs++;
			IF_PREPEND(&ifp->if_snd, m_head);
			atomic_set_int(&ifp->if_drv_flags, IFF_DRV_OACTIVE);
			break;
		}

		error = hn_encap(txr, txd, &m_head);
		if (error) {
			/* Both txd and m_head are freed */
			continue;
		}
again:
		/*
		 * Make sure that txd is not freed before ETHER_BPF_MTAP.
		 */
		hn_txdesc_hold(txd);
		error = hv_nv_on_send(device_ctx, &txd->netvsc_pkt);
		if (!error) {
			ETHER_BPF_MTAP(ifp, m_head);
			if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
		}
		hn_txdesc_put(txr, txd);

		if (__predict_false(error)) {
			int freed;

			/*
			 * This should "really rarely" happen.
			 *
			 * XXX Too many RX to be acked or too many sideband
			 * commands to run?  Ask netvsc_channel_rollup()
			 * to kick start later.
			 */
			txr->hn_txeof = 1;
			if (!send_failed) {
				txr->hn_send_failed++;
				send_failed = 1;
				/*
				 * Try sending again after set hn_txeof;
				 * in case that we missed the last
				 * netvsc_channel_rollup().
				 */
				goto again;
			}
			if_printf(ifp, "send failed\n");

			/*
			 * This mbuf will be prepended, don't free it
			 * in hn_txdesc_put(); only unload it from the
			 * DMA map in hn_txdesc_put(), if it was loaded.
			 */
			txd->m = NULL;
			freed = hn_txdesc_put(txr, txd);
			KASSERT(freed != 0,
			    ("fail to free txd upon send error"));

			txr->hn_send_failed++;
			IF_PREPEND(&ifp->if_snd, m_head);
			atomic_set_int(&ifp->if_drv_flags, IFF_DRV_OACTIVE);
			break;
		}
	}
	return 0;
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
		n = m_getjcl(M_DONTWAIT, m->m_type, 0, MJUMPAGESIZE);
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
	struct hn_softc *sc = device_get_softc(device_ctx->device);
	struct hn_rx_ring *rxr = &sc->hn_rx_ring[0]; /* TODO: vRSS */
	struct mbuf *m_new;
	struct ifnet *ifp;
	int size, do_lro = 0, do_csum = 1;

	if (sc == NULL) {
		return (0); /* TODO: KYS how can this be! */
	}

	ifp = sc->hn_ifp;
	
	ifp = sc->arpcom.ac_ifp;

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		return (0);
	}

	/*
	 * Bail out if packet contains more data than configured MTU.
	 */
	if (packet->tot_data_buf_len > (ifp->if_mtu + ETHER_HDR_LEN)) {
		return (0);
	} else if (packet->tot_data_buf_len <= MHLEN) {
		m_new = m_gethdr(M_NOWAIT, MT_DATA);
		if (m_new == NULL)
			return (0);
		memcpy(mtod(m_new, void *), packet->data,
		    packet->tot_data_buf_len);
		m_new->m_pkthdr.len = m_new->m_len = packet->tot_data_buf_len;
		rxr->hn_small_pkts++;
	} else {
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
			if_printf(ifp, "alloc mbuf failed.\n");
			return (0);
		}

		hv_m_append(m_new, packet->tot_data_buf_len, packet->data);
	}
	m_new->m_pkthdr.rcvif = ifp;

	if (__predict_false((ifp->if_capenable & IFCAP_RXCSUM) == 0))
		do_csum = 0;

	/* receive side checksum offload */
	if (csum_info != NULL) {
		/* IP csum offload */
		if (csum_info->receive.ip_csum_succeeded && do_csum) {
			m_new->m_pkthdr.csum_flags |=
			    (CSUM_IP_CHECKED | CSUM_IP_VALID);
			rxr->hn_csum_ip++;
		}

		/* TCP/UDP csum offload */
		if ((csum_info->receive.tcp_csum_succeeded ||
		     csum_info->receive.udp_csum_succeeded) && do_csum) {
			m_new->m_pkthdr.csum_flags |=
			    (CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
			m_new->m_pkthdr.csum_data = 0xffff;
			if (csum_info->receive.tcp_csum_succeeded)
				rxr->hn_csum_tcp++;
			else
				rxr->hn_csum_udp++;
		}

		if (csum_info->receive.ip_csum_succeeded &&
		    csum_info->receive.tcp_csum_succeeded)
			do_lro = 1;
	} else {
		const struct ether_header *eh;
		uint16_t etype;
		int hoff;

		hoff = sizeof(*eh);
		if (m_new->m_len < hoff)
			goto skip;
		eh = mtod(m_new, struct ether_header *);
		etype = ntohs(eh->ether_type);
		if (etype == ETHERTYPE_VLAN) {
			const struct ether_vlan_header *evl;

			hoff = sizeof(*evl);
			if (m_new->m_len < hoff)
				goto skip;
			evl = mtod(m_new, struct ether_vlan_header *);
			etype = ntohs(evl->evl_proto);
		}

		if (etype == ETHERTYPE_IP) {
			int pr;

			pr = hn_check_iplen(m_new, hoff);
			if (pr == IPPROTO_TCP) {
				if (do_csum &&
				    (rxr->hn_trust_hcsum &
				     HN_TRUST_HCSUM_TCP)) {
					rxr->hn_csum_trusted++;
					m_new->m_pkthdr.csum_flags |=
					   (CSUM_IP_CHECKED | CSUM_IP_VALID |
					    CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
					m_new->m_pkthdr.csum_data = 0xffff;
				}
				/* Rely on SW csum verification though... */
				do_lro = 1;
			} else if (pr == IPPROTO_UDP) {
				if (do_csum &&
				    (rxr->hn_trust_hcsum &
				     HN_TRUST_HCSUM_UDP)) {
					rxr->hn_csum_trusted++;
					m_new->m_pkthdr.csum_flags |=
					   (CSUM_IP_CHECKED | CSUM_IP_VALID |
					    CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
					m_new->m_pkthdr.csum_data = 0xffff;
				}
			} else if (pr != IPPROTO_DONE && do_csum &&
			    (rxr->hn_trust_hcsum & HN_TRUST_HCSUM_IP)) {
				rxr->hn_csum_trusted++;
				m_new->m_pkthdr.csum_flags |=
				    (CSUM_IP_CHECKED | CSUM_IP_VALID);
			}
		}
	}
skip:
	if ((packet->vlan_tci != 0) &&
	    (ifp->if_capenable & IFCAP_VLAN_HWTAGGING) != 0) {
		m_new->m_pkthdr.ether_vtag = packet->vlan_tci;
		m_new->m_flags |= M_VLANTAG;
	}

	/*
	 * Note:  Moved RX completion back to hv_nv_on_receive() so all
	 * messages (not just data messages) will trigger a response.
	 */

	ifp->if_ipackets++;

	if ((ifp->if_capenable & IFCAP_LRO) && do_lro) {
#if defined(INET) || defined(INET6)
		struct lro_ctrl *lro = &rxr->hn_lro;

		if (lro->lro_cnt) {
			rxr->hn_lro_tried++;
			if (tcp_lro_rx(lro, m_new, 0) == 0) {
				/* DONE! */
				return 0;
			}
		}
#endif
	}

	/* We're not holding the lock here, so don't release it */
	(*ifp->if_input)(ifp, m_new);

	return (0);
}

void
netvsc_recv_rollup(struct hv_device *device_ctx __unused)
{
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

#if __FreeBSD_version >= 1100099
		/*
		 * Make sure that LRO aggregation length limit is still
		 * valid, after the MTU change.
		 */
		NV_LOCK(sc);
		if (sc->hn_rx_ring[0].hn_lro.lro_length_lim <
		    HN_LRO_LENLIM_MIN(ifp)) {
			int i;
			for (i = 0; i < sc->hn_rx_ring_cnt; ++i) {
				sc->hn_rx_ring[i].hn_lro.lro_length_lim =
				    HN_LRO_LENLIM_MIN(ifp);
			}
		}
		NV_UNLOCK(sc);
#endif

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

		sc->hn_tx_chimney_max = sc->net_dev->send_section_size;
		if (sc->hn_tx_ring[0].hn_tx_chimney_size >
		    sc->hn_tx_chimney_max)
			hn_set_tx_chimney_size(sc, sc->hn_tx_chimney_max);

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
		NV_LOCK(sc);

		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		if (mask & IFCAP_TXCSUM) {
			ifp->if_capenable ^= IFCAP_TXCSUM;
			if (ifp->if_capenable & IFCAP_TXCSUM) {
				ifp->if_hwassist |=
				    sc->hn_tx_ring[0].hn_csum_assist;
			} else {
				ifp->if_hwassist &=
				    ~sc->hn_tx_ring[0].hn_csum_assist;
			}
		}

		if (mask & IFCAP_RXCSUM)
			ifp->if_capenable ^= IFCAP_RXCSUM;

		if (mask & IFCAP_LRO)
			ifp->if_capenable ^= IFCAP_LRO;

		if (mask & IFCAP_TSO4) {
			ifp->if_capenable ^= IFCAP_TSO4;
			if (ifp->if_capenable & IFCAP_TSO4)
				ifp->if_hwassist |= CSUM_IP_TSO;
			else
				ifp->if_hwassist &= ~CSUM_IP_TSO;
		}

		if (mask & IFCAP_TSO6) {
			ifp->if_capenable ^= IFCAP_TSO6;
			if (ifp->if_capenable & IFCAP_TSO6)
				ifp->if_hwassist |= CSUM_IP6_TSO;
			else
				ifp->if_hwassist &= ~CSUM_IP6_TSO;
		}

		NV_UNLOCK(sc);
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
		error = EINVAL;
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->hn_media, cmd);
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

	atomic_clear_int(&ifp->if_drv_flags,
	    (IFF_DRV_RUNNING | IFF_DRV_OACTIVE));
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
	struct hn_softc *sc = ifp->if_softc;
	struct hn_tx_ring *txr = &sc->hn_tx_ring[0];

	if (txr->hn_sched_tx)
		goto do_sched;

	if (mtx_trylock(&txr->hn_tx_lock)) {
		int sched;

		sched = hn_start_locked(txr, txr->hn_direct_tx_size);
		mtx_unlock(&txr->hn_tx_lock);
		if (!sched)
			return;
	}
do_sched:
	taskqueue_enqueue(txr->hn_tx_taskq, &txr->hn_start_task);
}

static void
hn_start_txeof(struct hn_tx_ring *txr)
{
	struct hn_softc *sc = txr->hn_sc;
	struct ifnet *ifp = sc->hn_ifp;

	KASSERT(txr == &sc->hn_tx_ring[0], ("not the first TX ring"));

	if (txr->hn_sched_tx)
		goto do_sched;

	if (mtx_trylock(&txr->hn_tx_lock)) {
		int sched;

		atomic_clear_int(&ifp->if_drv_flags, IFF_DRV_OACTIVE);
		sched = hn_start_locked(txr, txr->hn_direct_tx_size);
		mtx_unlock(&txr->hn_tx_lock);
		if (sched) {
			taskqueue_enqueue(txr->hn_tx_taskq,
			    &txr->hn_start_task);
		}
	} else {
do_sched:
		/*
		 * Release the OACTIVE earlier, with the hope, that
		 * others could catch up.  The task will clear the
		 * flag again with the hn_tx_lock to avoid possible
		 * races.
		 */
		atomic_clear_int(&ifp->if_drv_flags, IFF_DRV_OACTIVE);
		taskqueue_enqueue(txr->hn_tx_taskq, &txr->hn_txeof_task);
	}
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
	atomic_clear_int(&ifp->if_drv_flags, IFF_DRV_OACTIVE);
	atomic_set_int(&ifp->if_drv_flags, IFF_DRV_RUNNING);
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
	ifp->if_oerrors++;
}
#endif

#if __FreeBSD_version >= 1100099

static int
hn_lro_lenlim_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	unsigned int lenlim;
	int error, i;

	lenlim = sc->hn_rx_ring[0].hn_lro.lro_length_lim;
	error = sysctl_handle_int(oidp, &lenlim, 0, req);
	if (error || req->newptr == NULL)
		return error;

	if (lenlim < HN_LRO_LENLIM_MIN(sc->hn_ifp) ||
	    lenlim > TCP_LRO_LENGTH_MAX)
		return EINVAL;

	NV_LOCK(sc);
	for (i = 0; i < sc->hn_rx_ring_cnt; ++i)
		sc->hn_rx_ring[i].hn_lro.lro_length_lim = lenlim;
	NV_UNLOCK(sc);
	return 0;
}

static int
hn_lro_ackcnt_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int ackcnt, error, i;

	/*
	 * lro_ackcnt_lim is append count limit,
	 * +1 to turn it into aggregation limit.
	 */
	ackcnt = sc->hn_rx_ring[0].hn_lro.lro_ackcnt_lim + 1;
	error = sysctl_handle_int(oidp, &ackcnt, 0, req);
	if (error || req->newptr == NULL)
		return error;

	if (ackcnt < 2 || ackcnt > (TCP_LRO_ACKCNT_MAX + 1))
		return EINVAL;

	/*
	 * Convert aggregation limit back to append
	 * count limit.
	 */
	--ackcnt;
	NV_LOCK(sc);
	for (i = 0; i < sc->hn_rx_ring_cnt; ++i)
		sc->hn_rx_ring[i].hn_lro.lro_ackcnt_lim = ackcnt;
	NV_UNLOCK(sc);
	return 0;
}

#endif

static int
hn_trust_hcsum_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int hcsum = arg2;
	int on, error, i;

	on = 0;
	if (sc->hn_rx_ring[0].hn_trust_hcsum & hcsum)
		on = 1;

	error = sysctl_handle_int(oidp, &on, 0, req);
	if (error || req->newptr == NULL)
		return error;

	NV_LOCK(sc);
	for (i = 0; i < sc->hn_rx_ring_cnt; ++i) {
		struct hn_rx_ring *rxr = &sc->hn_rx_ring[i];

		if (on)
			rxr->hn_trust_hcsum |= hcsum;
		else
			rxr->hn_trust_hcsum &= ~hcsum;
	}
	NV_UNLOCK(sc);
	return 0;
}

static int
hn_tx_chimney_size_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int chimney_size, error;

	chimney_size = sc->hn_tx_ring[0].hn_tx_chimney_size;
	error = sysctl_handle_int(oidp, &chimney_size, 0, req);
	if (error || req->newptr == NULL)
		return error;

	if (chimney_size > sc->hn_tx_chimney_max || chimney_size <= 0)
		return EINVAL;

	hn_set_tx_chimney_size(sc, chimney_size);
	return 0;
}

static int
hn_rx_stat_ulong_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int ofs = arg2, i, error;
	struct hn_rx_ring *rxr;
	u_long stat;

	stat = 0;
	for (i = 0; i < sc->hn_rx_ring_cnt; ++i) {
		rxr = &sc->hn_rx_ring[i];
		stat += *((u_long *)((uint8_t *)rxr + ofs));
	}

	error = sysctl_handle_long(oidp, &stat, 0, req);
	if (error || req->newptr == NULL)
		return error;

	/* Zero out this stat. */
	for (i = 0; i < sc->hn_rx_ring_cnt; ++i) {
		rxr = &sc->hn_rx_ring[i];
		*((u_long *)((uint8_t *)rxr + ofs)) = 0;
	}
	return 0;
}

static int
hn_rx_stat_u64_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int ofs = arg2, i, error;
	struct hn_rx_ring *rxr;
	uint64_t stat;

	stat = 0;
	for (i = 0; i < sc->hn_rx_ring_cnt; ++i) {
		rxr = &sc->hn_rx_ring[i];
		stat += *((uint64_t *)((uint8_t *)rxr + ofs));
	}

	error = sysctl_handle_64(oidp, &stat, 0, req);
	if (error || req->newptr == NULL)
		return error;

	/* Zero out this stat. */
	for (i = 0; i < sc->hn_rx_ring_cnt; ++i) {
		rxr = &sc->hn_rx_ring[i];
		*((uint64_t *)((uint8_t *)rxr + ofs)) = 0;
	}
	return 0;
}

static int
hn_tx_stat_ulong_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int ofs = arg2, i, error;
	struct hn_tx_ring *txr;
	u_long stat;

	stat = 0;
	for (i = 0; i < sc->hn_tx_ring_cnt; ++i) {
		txr = &sc->hn_tx_ring[i];
		stat += *((u_long *)((uint8_t *)txr + ofs));
	}

	error = sysctl_handle_long(oidp, &stat, 0, req);
	if (error || req->newptr == NULL)
		return error;

	/* Zero out this stat. */
	for (i = 0; i < sc->hn_tx_ring_cnt; ++i) {
		txr = &sc->hn_tx_ring[i];
		*((u_long *)((uint8_t *)txr + ofs)) = 0;
	}
	return 0;
}

static int
hn_tx_conf_int_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct hn_softc *sc = arg1;
	int ofs = arg2, i, error, conf;
	struct hn_tx_ring *txr;

	txr = &sc->hn_tx_ring[0];
	conf = *((int *)((uint8_t *)txr + ofs));

	error = sysctl_handle_int(oidp, &conf, 0, req);
	if (error || req->newptr == NULL)
		return error;

	NV_LOCK(sc);
	for (i = 0; i < sc->hn_tx_ring_cnt; ++i) {
		txr = &sc->hn_tx_ring[i];
		*((int *)((uint8_t *)txr + ofs)) = conf;
	}
	NV_UNLOCK(sc);

	return 0;
}

static int
hn_check_iplen(const struct mbuf *m, int hoff)
{
	const struct ip *ip;
	int len, iphlen, iplen;
	const struct tcphdr *th;
	int thoff;				/* TCP data offset */

	len = hoff + sizeof(struct ip);

	/* The packet must be at least the size of an IP header. */
	if (m->m_pkthdr.len < len)
		return IPPROTO_DONE;

	/* The fixed IP header must reside completely in the first mbuf. */
	if (m->m_len < len)
		return IPPROTO_DONE;

	ip = mtodo(m, hoff);

	/* Bound check the packet's stated IP header length. */
	iphlen = ip->ip_hl << 2;
	if (iphlen < sizeof(struct ip))		/* minimum header length */
		return IPPROTO_DONE;

	/* The full IP header must reside completely in the one mbuf. */
	if (m->m_len < hoff + iphlen)
		return IPPROTO_DONE;

	iplen = ntohs(ip->ip_len);

	/*
	 * Check that the amount of data in the buffers is as
	 * at least much as the IP header would have us expect.
	 */
	if (m->m_pkthdr.len < hoff + iplen)
		return IPPROTO_DONE;

	/*
	 * Ignore IP fragments.
	 */
	if (ntohs(ip->ip_off) & (IP_OFFMASK | IP_MF))
		return IPPROTO_DONE;

	/*
	 * The TCP/IP or UDP/IP header must be entirely contained within
	 * the first fragment of a packet.
	 */
	switch (ip->ip_p) {
	case IPPROTO_TCP:
		if (iplen < iphlen + sizeof(struct tcphdr))
			return IPPROTO_DONE;
		if (m->m_len < hoff + iphlen + sizeof(struct tcphdr))
			return IPPROTO_DONE;
		th = (const struct tcphdr *)((const uint8_t *)ip + iphlen);
		thoff = th->th_off << 2;
		if (thoff < sizeof(struct tcphdr) || thoff + iphlen > iplen)
			return IPPROTO_DONE;
		if (m->m_len < hoff + iphlen + thoff)
			return IPPROTO_DONE;
		break;
	case IPPROTO_UDP:
		if (iplen < iphlen + sizeof(struct udphdr))
			return IPPROTO_DONE;
		if (m->m_len < hoff + iphlen + sizeof(struct udphdr))
			return IPPROTO_DONE;
		break;
	default:
		if (iplen < iphlen)
			return IPPROTO_DONE;
		break;
	}
	return ip->ip_p;
}

static void
hn_dma_map_paddr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *paddr = arg;

	if (error)
		return;

	KASSERT(nseg == 1, ("too many segments %d!", nseg));
	*paddr = segs->ds_addr;
}

static void
hn_create_rx_data(struct hn_softc *sc)
{
	struct sysctl_oid_list *child;
	struct sysctl_ctx_list *ctx;
	device_t dev = sc->hn_dev;
#if defined(INET) || defined(INET6)
#if __FreeBSD_version >= 1100095
	int lroent_cnt;
#endif
#endif
	int i;

	sc->hn_rx_ring_cnt = 1; /* TODO: vRSS */
	sc->hn_rx_ring = malloc(sizeof(struct hn_rx_ring) * sc->hn_rx_ring_cnt,
	    M_NETVSC, M_WAITOK | M_ZERO);

#if defined(INET) || defined(INET6)
#if __FreeBSD_version >= 1100095
	lroent_cnt = hn_lro_entry_count;
	if (lroent_cnt < TCP_LRO_ENTRIES)
		lroent_cnt = TCP_LRO_ENTRIES;
	device_printf(dev, "LRO: entry count %d\n", lroent_cnt);
#endif
#endif	/* INET || INET6 */

	for (i = 0; i < sc->hn_rx_ring_cnt; ++i) {
		struct hn_rx_ring *rxr = &sc->hn_rx_ring[i];

		if (hn_trust_hosttcp)
			rxr->hn_trust_hcsum |= HN_TRUST_HCSUM_TCP;
		if (hn_trust_hostudp)
			rxr->hn_trust_hcsum |= HN_TRUST_HCSUM_UDP;
		if (hn_trust_hostip)
			rxr->hn_trust_hcsum |= HN_TRUST_HCSUM_IP;

		/*
		 * Initialize LRO.
		 */
#if defined(INET) || defined(INET6)
#if __FreeBSD_version >= 1100095
		tcp_lro_init_args(&rxr->hn_lro, sc->hn_ifp, lroent_cnt, 0);
#else
		tcp_lro_init(&rxr->hn_lro);
		rxr->hn_lro.ifp = sc->hn_ifp;
#endif
#if __FreeBSD_version >= 1100099
		rxr->hn_lro.lro_length_lim = HN_LRO_LENLIM_DEF;
		rxr->hn_lro.lro_ackcnt_lim = HN_LRO_ACKCNT_DEF;
#endif
#endif	/* INET || INET6 */
	}

	ctx = device_get_sysctl_ctx(dev);
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));

	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "lro_queued",
	    CTLTYPE_U64 | CTLFLAG_RW, sc,
	    __offsetof(struct hn_rx_ring, hn_lro.lro_queued),
	    hn_rx_stat_u64_sysctl, "LU", "LRO queued");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "lro_flushed",
	    CTLTYPE_U64 | CTLFLAG_RW, sc,
	    __offsetof(struct hn_rx_ring, hn_lro.lro_flushed),
	    hn_rx_stat_u64_sysctl, "LU", "LRO flushed");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "lro_tried",
	    CTLTYPE_ULONG | CTLFLAG_RW, sc,
	    __offsetof(struct hn_rx_ring, hn_lro_tried),
	    hn_rx_stat_ulong_sysctl, "LU", "# of LRO tries");
#if __FreeBSD_version >= 1100099
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "lro_length_lim",
	    CTLTYPE_UINT | CTLFLAG_RW, sc, 0, hn_lro_lenlim_sysctl, "IU",
	    "Max # of data bytes to be aggregated by LRO");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "lro_ackcnt_lim",
	    CTLTYPE_INT | CTLFLAG_RW, sc, 0, hn_lro_ackcnt_sysctl, "I",
	    "Max # of ACKs to be aggregated by LRO");
#endif
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "trust_hosttcp",
	    CTLTYPE_INT | CTLFLAG_RW, sc, HN_TRUST_HCSUM_TCP,
	    hn_trust_hcsum_sysctl, "I",
	    "Trust tcp segement verification on host side, "
	    "when csum info is missing");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "trust_hostudp",
	    CTLTYPE_INT | CTLFLAG_RW, sc, HN_TRUST_HCSUM_UDP,
	    hn_trust_hcsum_sysctl, "I",
	    "Trust udp datagram verification on host side, "
	    "when csum info is missing");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "trust_hostip",
	    CTLTYPE_INT | CTLFLAG_RW, sc, HN_TRUST_HCSUM_IP,
	    hn_trust_hcsum_sysctl, "I",
	    "Trust ip packet verification on host side, "
	    "when csum info is missing");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "csum_ip",
	    CTLTYPE_ULONG | CTLFLAG_RW, sc,
	    __offsetof(struct hn_rx_ring, hn_csum_ip),
	    hn_rx_stat_ulong_sysctl, "LU", "RXCSUM IP");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "csum_tcp",
	    CTLTYPE_ULONG | CTLFLAG_RW, sc,
	    __offsetof(struct hn_rx_ring, hn_csum_tcp),
	    hn_rx_stat_ulong_sysctl, "LU", "RXCSUM TCP");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "csum_udp",
	    CTLTYPE_ULONG | CTLFLAG_RW, sc,
	    __offsetof(struct hn_rx_ring, hn_csum_udp),
	    hn_rx_stat_ulong_sysctl, "LU", "RXCSUM UDP");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "csum_trusted",
	    CTLTYPE_ULONG | CTLFLAG_RW, sc,
	    __offsetof(struct hn_rx_ring, hn_csum_trusted),
	    hn_rx_stat_ulong_sysctl, "LU",
	    "# of packets that we trust host's csum verification");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "small_pkts",
	    CTLTYPE_ULONG | CTLFLAG_RW, sc,
	    __offsetof(struct hn_rx_ring, hn_small_pkts),
	    hn_rx_stat_ulong_sysctl, "LU", "# of small packets received");
}

static void
hn_destroy_rx_data(struct hn_softc *sc)
{
#if defined(INET) || defined(INET6)
	int i;
#endif

	if (sc->hn_rx_ring_cnt == 0)
		return;

#if defined(INET) || defined(INET6)
	for (i = 0; i < sc->hn_rx_ring_cnt; ++i)
		tcp_lro_free(&sc->hn_rx_ring[i].hn_lro);
#endif
	free(sc->hn_rx_ring, M_NETVSC);
	sc->hn_rx_ring = NULL;

	sc->hn_rx_ring_cnt = 0;
}

static int
hn_create_tx_ring(struct hn_softc *sc, int id)
{
	struct hn_tx_ring *txr = &sc->hn_tx_ring[id];
	bus_dma_tag_t parent_dtag;
	int error, i;

	txr->hn_sc = sc;

	mtx_init(&txr->hn_txlist_spin, "hn txlist", NULL, MTX_SPIN);
	mtx_init(&txr->hn_tx_lock, "hn tx", NULL, MTX_DEF);

	txr->hn_txdesc_cnt = HN_TX_DESC_CNT;
	txr->hn_txdesc = malloc(sizeof(struct hn_txdesc) * txr->hn_txdesc_cnt,
	    M_NETVSC, M_WAITOK | M_ZERO);
	SLIST_INIT(&txr->hn_txlist);

	txr->hn_tx_taskq = sc->hn_tx_taskq;
	TASK_INIT(&txr->hn_start_task, 0, hn_start_taskfunc, txr);
	TASK_INIT(&txr->hn_txeof_task, 0, hn_txeof_taskfunc, txr);

	txr->hn_direct_tx_size = hn_direct_tx_size;
	if (hv_vmbus_protocal_version >= HV_VMBUS_VERSION_WIN8_1)
		txr->hn_csum_assist = HN_CSUM_ASSIST;
	else
		txr->hn_csum_assist = HN_CSUM_ASSIST_WIN8;

	/*
	 * Always schedule transmission instead of trying to do direct
	 * transmission.  This one gives the best performance so far.
	 */
	txr->hn_sched_tx = 1;

	parent_dtag = bus_get_dma_tag(sc->hn_dev);

	/* DMA tag for RNDIS messages. */
	error = bus_dma_tag_create(parent_dtag, /* parent */
	    HN_RNDIS_MSG_ALIGN,		/* alignment */
	    HN_RNDIS_MSG_BOUNDARY,	/* boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    HN_RNDIS_MSG_LEN,		/* maxsize */
	    1,				/* nsegments */
	    HN_RNDIS_MSG_LEN,		/* maxsegsize */
	    0,				/* flags */
	    NULL,			/* lockfunc */
	    NULL,			/* lockfuncarg */
	    &txr->hn_tx_rndis_dtag);
	if (error) {
		device_printf(sc->hn_dev, "failed to create rndis dmatag\n");
		return error;
	}

	/* DMA tag for data. */
	error = bus_dma_tag_create(parent_dtag, /* parent */
	    1,				/* alignment */
	    HN_TX_DATA_BOUNDARY,	/* boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    HN_TX_DATA_MAXSIZE,		/* maxsize */
	    HN_TX_DATA_SEGCNT_MAX,	/* nsegments */
	    HN_TX_DATA_SEGSIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL,			/* lockfunc */
	    NULL,			/* lockfuncarg */
	    &txr->hn_tx_data_dtag);
	if (error) {
		device_printf(sc->hn_dev, "failed to create data dmatag\n");
		return error;
	}

	for (i = 0; i < txr->hn_txdesc_cnt; ++i) {
		struct hn_txdesc *txd = &txr->hn_txdesc[i];

		txd->txr = txr;

		/*
		 * Allocate and load RNDIS messages.
		 */
        	error = bus_dmamem_alloc(txr->hn_tx_rndis_dtag,
		    (void **)&txd->rndis_msg,
		    BUS_DMA_WAITOK | BUS_DMA_COHERENT,
		    &txd->rndis_msg_dmap);
		if (error) {
			device_printf(sc->hn_dev,
			    "failed to allocate rndis_msg, %d\n", i);
			return error;
		}

		error = bus_dmamap_load(txr->hn_tx_rndis_dtag,
		    txd->rndis_msg_dmap,
		    txd->rndis_msg, HN_RNDIS_MSG_LEN,
		    hn_dma_map_paddr, &txd->rndis_msg_paddr,
		    BUS_DMA_NOWAIT);
		if (error) {
			device_printf(sc->hn_dev,
			    "failed to load rndis_msg, %d\n", i);
			bus_dmamem_free(txr->hn_tx_rndis_dtag,
			    txd->rndis_msg, txd->rndis_msg_dmap);
			return error;
		}

		/* DMA map for TX data. */
		error = bus_dmamap_create(txr->hn_tx_data_dtag, 0,
		    &txd->data_dmap);
		if (error) {
			device_printf(sc->hn_dev,
			    "failed to allocate tx data dmamap\n");
			bus_dmamap_unload(txr->hn_tx_rndis_dtag,
			    txd->rndis_msg_dmap);
			bus_dmamem_free(txr->hn_tx_rndis_dtag,
			    txd->rndis_msg, txd->rndis_msg_dmap);
			return error;
		}

		/* All set, put it to list */
		txd->flags |= HN_TXD_FLAG_ONLIST;
		SLIST_INSERT_HEAD(&txr->hn_txlist, txd, link);
	}
	txr->hn_txdesc_avail = txr->hn_txdesc_cnt;

	if (sc->hn_tx_sysctl_tree != NULL) {
		struct sysctl_oid_list *child;
		struct sysctl_ctx_list *ctx;
		char name[16];

		/*
		 * Create per TX ring sysctl tree:
		 * dev.hn.UNIT.tx.RINGID
		 */
		ctx = device_get_sysctl_ctx(sc->hn_dev);
		child = SYSCTL_CHILDREN(sc->hn_tx_sysctl_tree);

		snprintf(name, sizeof(name), "%d", id);
		txr->hn_tx_sysctl_tree = SYSCTL_ADD_NODE(ctx, child, OID_AUTO,
		    name, CTLFLAG_RD, 0, "");

		if (txr->hn_tx_sysctl_tree != NULL) {
			child = SYSCTL_CHILDREN(txr->hn_tx_sysctl_tree);

			SYSCTL_ADD_INT(ctx, child, OID_AUTO, "txdesc_avail",
			    CTLFLAG_RD, &txr->hn_txdesc_avail, 0,
			    "# of available TX descs");
		}
	}

	return 0;
}

static void
hn_destroy_tx_ring(struct hn_tx_ring *txr)
{
	struct hn_txdesc *txd;

	if (txr->hn_txdesc == NULL)
		return;

	while ((txd = SLIST_FIRST(&txr->hn_txlist)) != NULL) {
		KASSERT(txd->m == NULL, ("still has mbuf installed"));
		KASSERT((txd->flags & HN_TXD_FLAG_DMAMAP) == 0,
		    ("still dma mapped"));
		SLIST_REMOVE_HEAD(&txr->hn_txlist, link);

		bus_dmamap_unload(txr->hn_tx_rndis_dtag,
		    txd->rndis_msg_dmap);
		bus_dmamem_free(txr->hn_tx_rndis_dtag,
		    txd->rndis_msg, txd->rndis_msg_dmap);

		bus_dmamap_destroy(txr->hn_tx_data_dtag, txd->data_dmap);
	}

	if (txr->hn_tx_data_dtag != NULL)
		bus_dma_tag_destroy(txr->hn_tx_data_dtag);
	if (txr->hn_tx_rndis_dtag != NULL)
		bus_dma_tag_destroy(txr->hn_tx_rndis_dtag);
	free(txr->hn_txdesc, M_NETVSC);
	txr->hn_txdesc = NULL;

	mtx_destroy(&txr->hn_txlist_spin);
	mtx_destroy(&txr->hn_tx_lock);
}

static int
hn_create_tx_data(struct hn_softc *sc)
{
	struct sysctl_oid_list *child;
	struct sysctl_ctx_list *ctx;
	int i;

	sc->hn_tx_ring_cnt = 1; /* TODO: vRSS */
	sc->hn_tx_ring = malloc(sizeof(struct hn_tx_ring) * sc->hn_tx_ring_cnt,
	    M_NETVSC, M_WAITOK | M_ZERO);

	ctx = device_get_sysctl_ctx(sc->hn_dev);
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(sc->hn_dev));

	/* Create dev.hn.UNIT.tx sysctl tree */
	sc->hn_tx_sysctl_tree = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "tx",
	    CTLFLAG_RD, 0, "");

	for (i = 0; i < sc->hn_tx_ring_cnt; ++i) {
		int error;

		error = hn_create_tx_ring(sc, i);
		if (error)
			return error;
	}

	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "no_txdescs",
	    CTLTYPE_ULONG | CTLFLAG_RW, sc,
	    __offsetof(struct hn_tx_ring, hn_no_txdescs),
	    hn_tx_stat_ulong_sysctl, "LU", "# of times short of TX descs");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "send_failed",
	    CTLTYPE_ULONG | CTLFLAG_RW, sc,
	    __offsetof(struct hn_tx_ring, hn_send_failed),
	    hn_tx_stat_ulong_sysctl, "LU", "# of hyper-v sending failure");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "txdma_failed",
	    CTLTYPE_ULONG | CTLFLAG_RW, sc,
	    __offsetof(struct hn_tx_ring, hn_txdma_failed),
	    hn_tx_stat_ulong_sysctl, "LU", "# of TX DMA failure");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "tx_collapsed",
	    CTLTYPE_ULONG | CTLFLAG_RW, sc,
	    __offsetof(struct hn_tx_ring, hn_tx_collapsed),
	    hn_tx_stat_ulong_sysctl, "LU", "# of TX mbuf collapsed");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "tx_chimney",
	    CTLTYPE_ULONG | CTLFLAG_RW, sc,
	    __offsetof(struct hn_tx_ring, hn_tx_chimney),
	    hn_tx_stat_ulong_sysctl, "LU", "# of chimney send");
	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "txdesc_cnt",
	    CTLFLAG_RD, &sc->hn_tx_ring[0].hn_txdesc_cnt, 0,
	    "# of total TX descs");
	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "tx_chimney_max",
	    CTLFLAG_RD, &sc->hn_tx_chimney_max, 0,
	    "Chimney send packet size upper boundary");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "tx_chimney_size",
	    CTLTYPE_INT | CTLFLAG_RW, sc, 0, hn_tx_chimney_size_sysctl,
	    "I", "Chimney send packet size limit");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "direct_tx_size",
	    CTLTYPE_INT | CTLFLAG_RW, sc,
	    __offsetof(struct hn_tx_ring, hn_direct_tx_size),
	    hn_tx_conf_int_sysctl, "I",
	    "Size of the packet for direct transmission");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "sched_tx",
	    CTLTYPE_INT | CTLFLAG_RW, sc,
	    __offsetof(struct hn_tx_ring, hn_sched_tx),
	    hn_tx_conf_int_sysctl, "I",
	    "Always schedule transmission "
	    "instead of doing direct transmission");

	return 0;
}

static void
hn_set_tx_chimney_size(struct hn_softc *sc, int chimney_size)
{
	int i;

	NV_LOCK(sc);
	for (i = 0; i < sc->hn_tx_ring_cnt; ++i)
		sc->hn_tx_ring[i].hn_tx_chimney_size = chimney_size;
	NV_UNLOCK(sc);
}

static void
hn_destroy_tx_data(struct hn_softc *sc)
{
	int i;

	if (sc->hn_tx_ring_cnt == 0)
		return;

	for (i = 0; i < sc->hn_tx_ring_cnt; ++i)
		hn_destroy_tx_ring(&sc->hn_tx_ring[i]);

	free(sc->hn_tx_ring, M_NETVSC);
	sc->hn_tx_ring = NULL;

	sc->hn_tx_ring_cnt = 0;
}

static void
hn_start_taskfunc(void *xtxr, int pending __unused)
{
	struct hn_tx_ring *txr = xtxr;

	mtx_lock(&txr->hn_tx_lock);
	hn_start_locked(txr, 0);
	mtx_unlock(&txr->hn_tx_lock);
}

static void
hn_txeof_taskfunc(void *xtxr, int pending __unused)
{
	struct hn_tx_ring *txr = xtxr;

	mtx_lock(&txr->hn_tx_lock);
	atomic_clear_int(&txr->hn_sc->hn_ifp->if_drv_flags, IFF_DRV_OACTIVE);
	hn_start_locked(txr, 0);
	mtx_unlock(&txr->hn_tx_lock);
}

static void
hn_stop_tx_tasks(struct hn_softc *sc)
{
	int i;

	for (i = 0; i < sc->hn_tx_ring_cnt; ++i) {
		struct hn_tx_ring *txr = &sc->hn_tx_ring[i];

		taskqueue_drain(txr->hn_tx_taskq, &txr->hn_start_task);
		taskqueue_drain(txr->hn_tx_taskq, &txr->hn_txeof_task);
	}
}

static void
hn_tx_taskq_create(void *arg __unused)
{
	if (!hn_share_tx_taskq)
		return;

	hn_tx_taskq = taskqueue_create("hn_tx", M_WAITOK,
	    taskqueue_thread_enqueue, &hn_tx_taskq);
	taskqueue_start_threads(&hn_tx_taskq, 1, PI_NET, "hn tx");
}
SYSINIT(hn_txtq_create, SI_SUB_DRIVERS, SI_ORDER_FIRST,
    hn_tx_taskq_create, NULL);

static void
hn_tx_taskq_destroy(void *arg __unused)
{
	if (hn_tx_taskq != NULL)
		taskqueue_free(hn_tx_taskq);
}
SYSUNINIT(hn_txtq_destroy, SI_SUB_DRIVERS, SI_ORDER_FIRST,
    hn_tx_taskq_destroy, NULL);

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
