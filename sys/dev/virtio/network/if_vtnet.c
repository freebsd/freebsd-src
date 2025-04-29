/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011, Bryan Venteicher <bryanv@FreeBSD.org>
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

/* Driver for VirtIO network devices. */

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sockio.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/msan.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/random.h>
#include <sys/sglist.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/taskqueue.h>
#include <sys/smp.h>
#include <machine/smp.h>

#include <vm/uma.h>

#include <net/debugnet.h>
#include <net/ethernet.h>
#include <net/pfil.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/if_vlan_var.h>

#include <net/bpf.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/tcp_lro.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/virtio/virtio.h>
#include <dev/virtio/virtqueue.h>
#include <dev/virtio/network/virtio_net.h>
#include <dev/virtio/network/if_vtnetvar.h>
#include "virtio_if.h"

#include "opt_inet.h"
#include "opt_inet6.h"

#if defined(INET) || defined(INET6)
#include <machine/in_cksum.h>
#endif

#ifdef __NO_STRICT_ALIGNMENT
#define VTNET_ETHER_ALIGN 0
#else /* Strict alignment */
#define VTNET_ETHER_ALIGN ETHER_ALIGN
#endif

static int	vtnet_modevent(module_t, int, void *);

static int	vtnet_probe(device_t);
static int	vtnet_attach(device_t);
static int	vtnet_detach(device_t);
static int	vtnet_suspend(device_t);
static int	vtnet_resume(device_t);
static int	vtnet_shutdown(device_t);
static int	vtnet_attach_completed(device_t);
static int	vtnet_config_change(device_t);

static int	vtnet_negotiate_features(struct vtnet_softc *);
static int	vtnet_setup_features(struct vtnet_softc *);
static int	vtnet_init_rxq(struct vtnet_softc *, int);
static int	vtnet_init_txq(struct vtnet_softc *, int);
static int	vtnet_alloc_rxtx_queues(struct vtnet_softc *);
static void	vtnet_free_rxtx_queues(struct vtnet_softc *);
static int	vtnet_alloc_rx_filters(struct vtnet_softc *);
static void	vtnet_free_rx_filters(struct vtnet_softc *);
static int	vtnet_alloc_virtqueues(struct vtnet_softc *);
static void	vtnet_alloc_interface(struct vtnet_softc *);
static int	vtnet_setup_interface(struct vtnet_softc *);
static int	vtnet_ioctl_mtu(struct vtnet_softc *, u_int);
static int	vtnet_ioctl_ifflags(struct vtnet_softc *);
static int	vtnet_ioctl_multi(struct vtnet_softc *);
static int	vtnet_ioctl_ifcap(struct vtnet_softc *, struct ifreq *);
static int	vtnet_ioctl(if_t, u_long, caddr_t);
static uint64_t	vtnet_get_counter(if_t, ift_counter);

static int	vtnet_rxq_populate(struct vtnet_rxq *);
static void	vtnet_rxq_free_mbufs(struct vtnet_rxq *);
static struct mbuf *
		vtnet_rx_alloc_buf(struct vtnet_softc *, int , struct mbuf **);
static int	vtnet_rxq_replace_lro_nomrg_buf(struct vtnet_rxq *,
		    struct mbuf *, int);
static int	vtnet_rxq_replace_buf(struct vtnet_rxq *, struct mbuf *, int);
static int	vtnet_rxq_enqueue_buf(struct vtnet_rxq *, struct mbuf *);
static int	vtnet_rxq_new_buf(struct vtnet_rxq *);
static int	vtnet_rxq_csum_needs_csum(struct vtnet_rxq *, struct mbuf *,
		     uint16_t, int, struct virtio_net_hdr *);
static int	vtnet_rxq_csum_data_valid(struct vtnet_rxq *, struct mbuf *,
		     uint16_t, int, struct virtio_net_hdr *);
static int	vtnet_rxq_csum(struct vtnet_rxq *, struct mbuf *,
		     struct virtio_net_hdr *);
static void	vtnet_rxq_discard_merged_bufs(struct vtnet_rxq *, int);
static void	vtnet_rxq_discard_buf(struct vtnet_rxq *, struct mbuf *);
static int	vtnet_rxq_merged_eof(struct vtnet_rxq *, struct mbuf *, int);
static void	vtnet_rxq_input(struct vtnet_rxq *, struct mbuf *,
		    struct virtio_net_hdr *);
static int	vtnet_rxq_eof(struct vtnet_rxq *);
static void	vtnet_rx_vq_process(struct vtnet_rxq *rxq, int tries);
static void	vtnet_rx_vq_intr(void *);
static void	vtnet_rxq_tq_intr(void *, int);

static int	vtnet_txq_intr_threshold(struct vtnet_txq *);
static int	vtnet_txq_below_threshold(struct vtnet_txq *);
static int	vtnet_txq_notify(struct vtnet_txq *);
static void	vtnet_txq_free_mbufs(struct vtnet_txq *);
static int	vtnet_txq_offload_ctx(struct vtnet_txq *, struct mbuf *,
		    int *, int *, int *);
static int	vtnet_txq_offload_tso(struct vtnet_txq *, struct mbuf *, int,
		    int, struct virtio_net_hdr *);
static struct mbuf *
		vtnet_txq_offload(struct vtnet_txq *, struct mbuf *,
		    struct virtio_net_hdr *);
static int	vtnet_txq_enqueue_buf(struct vtnet_txq *, struct mbuf **,
		    struct vtnet_tx_header *);
static int	vtnet_txq_encap(struct vtnet_txq *, struct mbuf **, int);

/* Required for ALTQ */
static void	vtnet_start_locked(struct vtnet_txq *, if_t);
static void	vtnet_start(if_t);

/* Required for MQ */
static int	vtnet_txq_mq_start_locked(struct vtnet_txq *, struct mbuf *);
static int	vtnet_txq_mq_start(if_t, struct mbuf *);
static void	vtnet_txq_tq_deferred(void *, int);
static void	vtnet_qflush(if_t);


static void	vtnet_txq_start(struct vtnet_txq *);
static void	vtnet_txq_tq_intr(void *, int);
static int	vtnet_txq_eof(struct vtnet_txq *);
static void	vtnet_tx_vq_intr(void *);
static void	vtnet_tx_start_all(struct vtnet_softc *);

static int	vtnet_watchdog(struct vtnet_txq *);
static void	vtnet_accum_stats(struct vtnet_softc *,
		    struct vtnet_rxq_stats *, struct vtnet_txq_stats *);
static void	vtnet_tick(void *);

static void	vtnet_start_taskqueues(struct vtnet_softc *);
static void	vtnet_free_taskqueues(struct vtnet_softc *);
static void	vtnet_drain_taskqueues(struct vtnet_softc *);

static void	vtnet_drain_rxtx_queues(struct vtnet_softc *);
static void	vtnet_stop_rendezvous(struct vtnet_softc *);
static void	vtnet_stop(struct vtnet_softc *);
static int	vtnet_virtio_reinit(struct vtnet_softc *);
static void	vtnet_init_rx_filters(struct vtnet_softc *);
static int	vtnet_init_rx_queues(struct vtnet_softc *);
static int	vtnet_init_tx_queues(struct vtnet_softc *);
static int	vtnet_init_rxtx_queues(struct vtnet_softc *);
static void	vtnet_set_active_vq_pairs(struct vtnet_softc *);
static void	vtnet_update_rx_offloads(struct vtnet_softc *);
static int	vtnet_reinit(struct vtnet_softc *);
static void	vtnet_init_locked(struct vtnet_softc *, int);
static void	vtnet_init(void *);

static void	vtnet_free_ctrl_vq(struct vtnet_softc *);
static void	vtnet_exec_ctrl_cmd(struct vtnet_softc *, void *,
		    struct sglist *, int, int);
static int	vtnet_ctrl_mac_cmd(struct vtnet_softc *, uint8_t *);
static int	vtnet_ctrl_guest_offloads(struct vtnet_softc *, uint64_t);
static int	vtnet_ctrl_mq_cmd(struct vtnet_softc *, uint16_t);
static int	vtnet_ctrl_rx_cmd(struct vtnet_softc *, uint8_t, bool);
static int	vtnet_set_promisc(struct vtnet_softc *, bool);
static int	vtnet_set_allmulti(struct vtnet_softc *, bool);
static void	vtnet_rx_filter(struct vtnet_softc *);
static void	vtnet_rx_filter_mac(struct vtnet_softc *);
static int	vtnet_exec_vlan_filter(struct vtnet_softc *, int, uint16_t);
static void	vtnet_rx_filter_vlan(struct vtnet_softc *);
static void	vtnet_update_vlan_filter(struct vtnet_softc *, int, uint16_t);
static void	vtnet_register_vlan(void *, if_t, uint16_t);
static void	vtnet_unregister_vlan(void *, if_t, uint16_t);

static void	vtnet_update_speed_duplex(struct vtnet_softc *);
static int	vtnet_is_link_up(struct vtnet_softc *);
static void	vtnet_update_link_status(struct vtnet_softc *);
static int	vtnet_ifmedia_upd(if_t);
static void	vtnet_ifmedia_sts(if_t, struct ifmediareq *);
static void	vtnet_get_macaddr(struct vtnet_softc *);
static void	vtnet_set_macaddr(struct vtnet_softc *);
static void	vtnet_attached_set_macaddr(struct vtnet_softc *);
static void	vtnet_vlan_tag_remove(struct mbuf *);
static void	vtnet_set_rx_process_limit(struct vtnet_softc *);

static void	vtnet_setup_rxq_sysctl(struct sysctl_ctx_list *,
		    struct sysctl_oid_list *, struct vtnet_rxq *);
static void	vtnet_setup_txq_sysctl(struct sysctl_ctx_list *,
		    struct sysctl_oid_list *, struct vtnet_txq *);
static void	vtnet_setup_queue_sysctl(struct vtnet_softc *);
static void	vtnet_load_tunables(struct vtnet_softc *);
static void	vtnet_setup_sysctl(struct vtnet_softc *);

static int	vtnet_rxq_enable_intr(struct vtnet_rxq *);
static void	vtnet_rxq_disable_intr(struct vtnet_rxq *);
static int	vtnet_txq_enable_intr(struct vtnet_txq *);
static void	vtnet_txq_disable_intr(struct vtnet_txq *);
static void	vtnet_enable_rx_interrupts(struct vtnet_softc *);
static void	vtnet_enable_tx_interrupts(struct vtnet_softc *);
static void	vtnet_enable_interrupts(struct vtnet_softc *);
static void	vtnet_disable_rx_interrupts(struct vtnet_softc *);
static void	vtnet_disable_tx_interrupts(struct vtnet_softc *);
static void	vtnet_disable_interrupts(struct vtnet_softc *);

static int	vtnet_tunable_int(struct vtnet_softc *, const char *, int);

DEBUGNET_DEFINE(vtnet);

#define vtnet_htog16(_sc, _val)	virtio_htog16(vtnet_modern(_sc), _val)
#define vtnet_htog32(_sc, _val)	virtio_htog32(vtnet_modern(_sc), _val)
#define vtnet_htog64(_sc, _val)	virtio_htog64(vtnet_modern(_sc), _val)
#define vtnet_gtoh16(_sc, _val)	virtio_gtoh16(vtnet_modern(_sc), _val)
#define vtnet_gtoh32(_sc, _val)	virtio_gtoh32(vtnet_modern(_sc), _val)
#define vtnet_gtoh64(_sc, _val)	virtio_gtoh64(vtnet_modern(_sc), _val)

/* Tunables. */
static SYSCTL_NODE(_hw, OID_AUTO, vtnet, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "VirtIO Net driver parameters");

static int vtnet_csum_disable = 0;
SYSCTL_INT(_hw_vtnet, OID_AUTO, csum_disable, CTLFLAG_RDTUN,
    &vtnet_csum_disable, 0, "Disables receive and send checksum offload");

static int vtnet_fixup_needs_csum = 0;
SYSCTL_INT(_hw_vtnet, OID_AUTO, fixup_needs_csum, CTLFLAG_RDTUN,
    &vtnet_fixup_needs_csum, 0,
    "Calculate valid checksum for NEEDS_CSUM packets");

static int vtnet_tso_disable = 0;
SYSCTL_INT(_hw_vtnet, OID_AUTO, tso_disable, CTLFLAG_RDTUN,
    &vtnet_tso_disable, 0, "Disables TSO");

static int vtnet_lro_disable = 0;
SYSCTL_INT(_hw_vtnet, OID_AUTO, lro_disable, CTLFLAG_RDTUN,
    &vtnet_lro_disable, 0, "Disables hardware LRO");

static int vtnet_mq_disable = 0;
SYSCTL_INT(_hw_vtnet, OID_AUTO, mq_disable, CTLFLAG_RDTUN,
    &vtnet_mq_disable, 0, "Disables multiqueue support");

static int vtnet_mq_max_pairs = VTNET_MAX_QUEUE_PAIRS;
SYSCTL_INT(_hw_vtnet, OID_AUTO, mq_max_pairs, CTLFLAG_RDTUN,
    &vtnet_mq_max_pairs, 0, "Maximum number of multiqueue pairs");

static int vtnet_tso_maxlen = IP_MAXPACKET;
SYSCTL_INT(_hw_vtnet, OID_AUTO, tso_maxlen, CTLFLAG_RDTUN,
    &vtnet_tso_maxlen, 0, "TSO burst limit");

static int vtnet_rx_process_limit = 1024;
SYSCTL_INT(_hw_vtnet, OID_AUTO, rx_process_limit, CTLFLAG_RDTUN,
    &vtnet_rx_process_limit, 0,
    "Number of RX segments processed in one pass");

static int vtnet_lro_entry_count = 128;
SYSCTL_INT(_hw_vtnet, OID_AUTO, lro_entry_count, CTLFLAG_RDTUN,
    &vtnet_lro_entry_count, 0, "Software LRO entry count");

/* Enable sorted LRO, and the depth of the mbuf queue. */
static int vtnet_lro_mbufq_depth = 0;
SYSCTL_UINT(_hw_vtnet, OID_AUTO, lro_mbufq_depth, CTLFLAG_RDTUN,
    &vtnet_lro_mbufq_depth, 0, "Depth of software LRO mbuf queue");

/* Deactivate ALTQ Support */
static int vtnet_altq_disable = 0;
SYSCTL_INT(_hw_vtnet, OID_AUTO, altq_disable, CTLFLAG_RDTUN,
    &vtnet_altq_disable, 0, "Disables ALTQ Support");

/*
 * For the driver to be considered as having altq enabled,
 * it must be compiled with an ALTQ capable kernel,
 * and the tunable hw.vtnet.altq_disable must be zero
 */
#define VTNET_ALTQ_ENABLED (VTNET_ALTQ_CAPABLE && (!vtnet_altq_disable))


static uma_zone_t vtnet_tx_header_zone;

static struct virtio_feature_desc vtnet_feature_desc[] = {
	{ VIRTIO_NET_F_CSUM,			"TxChecksum"		},
	{ VIRTIO_NET_F_GUEST_CSUM,		"RxChecksum"		},
	{ VIRTIO_NET_F_CTRL_GUEST_OFFLOADS,	"CtrlRxOffloads"	},
	{ VIRTIO_NET_F_MAC,			"MAC"			},
	{ VIRTIO_NET_F_GSO,			"TxGSO"			},
	{ VIRTIO_NET_F_GUEST_TSO4,		"RxLROv4"		},
	{ VIRTIO_NET_F_GUEST_TSO6,		"RxLROv6"		},
	{ VIRTIO_NET_F_GUEST_ECN,		"RxLROECN"		},
	{ VIRTIO_NET_F_GUEST_UFO,		"RxUFO"			},
	{ VIRTIO_NET_F_HOST_TSO4,		"TxTSOv4"		},
	{ VIRTIO_NET_F_HOST_TSO6,		"TxTSOv6"		},
	{ VIRTIO_NET_F_HOST_ECN,		"TxTSOECN"		},
	{ VIRTIO_NET_F_HOST_UFO,		"TxUFO"			},
	{ VIRTIO_NET_F_MRG_RXBUF,		"MrgRxBuf"		},
	{ VIRTIO_NET_F_STATUS,			"Status"		},
	{ VIRTIO_NET_F_CTRL_VQ,			"CtrlVq"		},
	{ VIRTIO_NET_F_CTRL_RX,			"CtrlRxMode"		},
	{ VIRTIO_NET_F_CTRL_VLAN,		"CtrlVLANFilter"	},
	{ VIRTIO_NET_F_CTRL_RX_EXTRA,		"CtrlRxModeExtra"	},
	{ VIRTIO_NET_F_GUEST_ANNOUNCE,		"GuestAnnounce"		},
	{ VIRTIO_NET_F_MQ,			"Multiqueue"		},
	{ VIRTIO_NET_F_CTRL_MAC_ADDR,		"CtrlMacAddr"		},
	{ VIRTIO_NET_F_SPEED_DUPLEX,		"SpeedDuplex"		},

	{ 0, NULL }
};

static device_method_t vtnet_methods[] = {
	/* Device methods. */
	DEVMETHOD(device_probe,			vtnet_probe),
	DEVMETHOD(device_attach,		vtnet_attach),
	DEVMETHOD(device_detach,		vtnet_detach),
	DEVMETHOD(device_suspend,		vtnet_suspend),
	DEVMETHOD(device_resume,		vtnet_resume),
	DEVMETHOD(device_shutdown,		vtnet_shutdown),

	/* VirtIO methods. */
	DEVMETHOD(virtio_attach_completed,	vtnet_attach_completed),
	DEVMETHOD(virtio_config_change,		vtnet_config_change),

	DEVMETHOD_END
};

#ifdef DEV_NETMAP
#include <dev/netmap/if_vtnet_netmap.h>
#endif

static driver_t vtnet_driver = {
    .name = "vtnet",
    .methods = vtnet_methods,
    .size = sizeof(struct vtnet_softc)
};
VIRTIO_DRIVER_MODULE(vtnet, vtnet_driver, vtnet_modevent, NULL);
MODULE_VERSION(vtnet, 1);
MODULE_DEPEND(vtnet, virtio, 1, 1, 1);
#ifdef DEV_NETMAP
MODULE_DEPEND(vtnet, netmap, 1, 1, 1);
#endif

VIRTIO_SIMPLE_PNPINFO(vtnet, VIRTIO_ID_NETWORK, "VirtIO Networking Adapter");

static int
vtnet_modevent(module_t mod __unused, int type, void *unused __unused)
{
	int error = 0;
	static int loaded = 0;

	switch (type) {
	case MOD_LOAD:
		if (loaded++ == 0) {
			vtnet_tx_header_zone = uma_zcreate("vtnet_tx_hdr",
				sizeof(struct vtnet_tx_header),
				NULL, NULL, NULL, NULL, 0, 0);
#ifdef DEBUGNET
			/*
			 * We need to allocate from this zone in the transmit path, so ensure
			 * that we have at least one item per header available.
			 * XXX add a separate zone like we do for mbufs? otherwise we may alloc
			 * buckets
			 */
			uma_zone_reserve(vtnet_tx_header_zone, DEBUGNET_MAX_IN_FLIGHT * 2);
			uma_prealloc(vtnet_tx_header_zone, DEBUGNET_MAX_IN_FLIGHT * 2);
#endif
		}
		break;
	case MOD_QUIESCE:
		if (uma_zone_get_cur(vtnet_tx_header_zone) > 0)
			error = EBUSY;
		break;
	case MOD_UNLOAD:
		if (--loaded == 0) {
			uma_zdestroy(vtnet_tx_header_zone);
			vtnet_tx_header_zone = NULL;
		}
		break;
	case MOD_SHUTDOWN:
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static int
vtnet_probe(device_t dev)
{
	return (VIRTIO_SIMPLE_PROBE(dev, vtnet));
}

static int
vtnet_attach(device_t dev)
{
	struct vtnet_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->vtnet_dev = dev;
	virtio_set_feature_desc(dev, vtnet_feature_desc);

	VTNET_CORE_LOCK_INIT(sc);
	callout_init_mtx(&sc->vtnet_tick_ch, VTNET_CORE_MTX(sc), 0);
	vtnet_load_tunables(sc);

	vtnet_alloc_interface(sc);
	vtnet_setup_sysctl(sc);

	error = vtnet_setup_features(sc);
	if (error) {
		device_printf(dev, "cannot setup features\n");
		goto fail;
	}

	error = vtnet_alloc_rx_filters(sc);
	if (error) {
		device_printf(dev, "cannot allocate Rx filters\n");
		goto fail;
	}

	error = vtnet_alloc_rxtx_queues(sc);
	if (error) {
		device_printf(dev, "cannot allocate queues\n");
		goto fail;
	}

	error = vtnet_alloc_virtqueues(sc);
	if (error) {
		device_printf(dev, "cannot allocate virtqueues\n");
		goto fail;
	}

	error = vtnet_setup_interface(sc);
	if (error) {
		device_printf(dev, "cannot setup interface\n");
		goto fail;
	}

	error = virtio_setup_intr(dev, INTR_TYPE_NET);
	if (error) {
		device_printf(dev, "cannot setup interrupts\n");
		ether_ifdetach(sc->vtnet_ifp);
		goto fail;
	}

#ifdef DEV_NETMAP
	vtnet_netmap_attach(sc);
#endif
	vtnet_start_taskqueues(sc);

fail:
	if (error)
		vtnet_detach(dev);

	return (error);
}

static int
vtnet_detach(device_t dev)
{
	struct vtnet_softc *sc;
	if_t ifp;

	sc = device_get_softc(dev);
	ifp = sc->vtnet_ifp;

	if (device_is_attached(dev)) {
		VTNET_CORE_LOCK(sc);
		vtnet_stop(sc);
		VTNET_CORE_UNLOCK(sc);

		callout_drain(&sc->vtnet_tick_ch);
		vtnet_drain_taskqueues(sc);

		ether_ifdetach(ifp);
	}

#ifdef DEV_NETMAP
	netmap_detach(ifp);
#endif

	if (sc->vtnet_pfil != NULL) {
		pfil_head_unregister(sc->vtnet_pfil);
		sc->vtnet_pfil = NULL;
	}

	vtnet_free_taskqueues(sc);

	if (sc->vtnet_vlan_attach != NULL) {
		EVENTHANDLER_DEREGISTER(vlan_config, sc->vtnet_vlan_attach);
		sc->vtnet_vlan_attach = NULL;
	}
	if (sc->vtnet_vlan_detach != NULL) {
		EVENTHANDLER_DEREGISTER(vlan_unconfig, sc->vtnet_vlan_detach);
		sc->vtnet_vlan_detach = NULL;
	}

	ifmedia_removeall(&sc->vtnet_media);

	if (ifp != NULL) {
		if_free(ifp);
		sc->vtnet_ifp = NULL;
	}

	vtnet_free_rxtx_queues(sc);
	vtnet_free_rx_filters(sc);

	if (sc->vtnet_ctrl_vq != NULL)
		vtnet_free_ctrl_vq(sc);

	VTNET_CORE_LOCK_DESTROY(sc);

	return (0);
}

static int
vtnet_suspend(device_t dev)
{
	struct vtnet_softc *sc;

	sc = device_get_softc(dev);

	VTNET_CORE_LOCK(sc);
	vtnet_stop(sc);
	sc->vtnet_flags |= VTNET_FLAG_SUSPENDED;
	VTNET_CORE_UNLOCK(sc);

	return (0);
}

static int
vtnet_resume(device_t dev)
{
	struct vtnet_softc *sc;
	if_t ifp;

	sc = device_get_softc(dev);
	ifp = sc->vtnet_ifp;

	VTNET_CORE_LOCK(sc);
	if (if_getflags(ifp) & IFF_UP)
		vtnet_init_locked(sc, 0);
	sc->vtnet_flags &= ~VTNET_FLAG_SUSPENDED;
	VTNET_CORE_UNLOCK(sc);

	return (0);
}

static int
vtnet_shutdown(device_t dev)
{
	/*
	 * Suspend already does all of what we need to
	 * do here; we just never expect to be resumed.
	 */
	return (vtnet_suspend(dev));
}

static int
vtnet_attach_completed(device_t dev)
{
	struct vtnet_softc *sc;

	sc = device_get_softc(dev);

	VTNET_CORE_LOCK(sc);
	vtnet_attached_set_macaddr(sc);
	VTNET_CORE_UNLOCK(sc);

	return (0);
}

static int
vtnet_config_change(device_t dev)
{
	struct vtnet_softc *sc;

	sc = device_get_softc(dev);

	VTNET_CORE_LOCK(sc);
	vtnet_update_link_status(sc);
	if (sc->vtnet_link_active != 0)
		vtnet_tx_start_all(sc);
	VTNET_CORE_UNLOCK(sc);

	return (0);
}

static int
vtnet_negotiate_features(struct vtnet_softc *sc)
{
	device_t dev;
	uint64_t features, negotiated_features;
	int no_csum;

	dev = sc->vtnet_dev;
	features = virtio_bus_is_modern(dev) ? VTNET_MODERN_FEATURES :
	    VTNET_LEGACY_FEATURES;

	/*
	 * TSO and LRO are only available when their corresponding checksum
	 * offload feature is also negotiated.
	 */
	no_csum = vtnet_tunable_int(sc, "csum_disable", vtnet_csum_disable);
	if (no_csum)
		features &= ~(VIRTIO_NET_F_CSUM | VIRTIO_NET_F_GUEST_CSUM);
	if (no_csum || vtnet_tunable_int(sc, "tso_disable", vtnet_tso_disable))
		features &= ~VTNET_TSO_FEATURES;
	if (no_csum || vtnet_tunable_int(sc, "lro_disable", vtnet_lro_disable))
		features &= ~VTNET_LRO_FEATURES;

	/* Deactivate MQ Feature flag, if driver has ALTQ enabled, or MQ is explicitly disabled */
	if (VTNET_ALTQ_ENABLED || vtnet_tunable_int(sc, "mq_disable", vtnet_mq_disable))
		features &= ~VIRTIO_NET_F_MQ;

	negotiated_features = virtio_negotiate_features(dev, features);

	if (virtio_with_feature(dev, VIRTIO_NET_F_MTU)) {
		uint16_t mtu;

		mtu = virtio_read_dev_config_2(dev,
		    offsetof(struct virtio_net_config, mtu));
		if (mtu < VTNET_MIN_MTU) {
			device_printf(dev, "Invalid MTU value: %d. "
			    "MTU feature disabled.\n", mtu);
			features &= ~VIRTIO_NET_F_MTU;
			negotiated_features =
			    virtio_negotiate_features(dev, features);
		}
	}

	if (virtio_with_feature(dev, VIRTIO_NET_F_MQ)) {
		uint16_t npairs;

		npairs = virtio_read_dev_config_2(dev,
		    offsetof(struct virtio_net_config, max_virtqueue_pairs));
		if (npairs < VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MIN ||
		    npairs > VIRTIO_NET_CTRL_MQ_VQ_PAIRS_MAX) {
			device_printf(dev, "Invalid max_virtqueue_pairs value: "
			    "%d. Multiqueue feature disabled.\n", npairs);
			features &= ~VIRTIO_NET_F_MQ;
			negotiated_features =
			    virtio_negotiate_features(dev, features);
		}
	}

	if (virtio_with_feature(dev, VTNET_LRO_FEATURES) &&
	    virtio_with_feature(dev, VIRTIO_NET_F_MRG_RXBUF) == 0) {
		/*
		 * LRO without mergeable buffers requires special care. This
		 * is not ideal because every receive buffer must be large
		 * enough to hold the maximum TCP packet, the Ethernet header,
		 * and the header. This requires up to 34 descriptors with
		 * MCLBYTES clusters. If we do not have indirect descriptors,
		 * LRO is disabled since the virtqueue will not contain very
		 * many receive buffers.
		 */
		if (!virtio_with_feature(dev, VIRTIO_RING_F_INDIRECT_DESC)) {
			device_printf(dev,
			    "Host LRO disabled since both mergeable buffers "
			    "and indirect descriptors were not negotiated\n");
			features &= ~VTNET_LRO_FEATURES;
			negotiated_features =
			    virtio_negotiate_features(dev, features);
		} else
			sc->vtnet_flags |= VTNET_FLAG_LRO_NOMRG;
	}

	sc->vtnet_features = negotiated_features;
	sc->vtnet_negotiated_features = negotiated_features;

	return (virtio_finalize_features(dev));
}

static int
vtnet_setup_features(struct vtnet_softc *sc)
{
	device_t dev;
	int error;

	dev = sc->vtnet_dev;

	error = vtnet_negotiate_features(sc);
	if (error)
		return (error);

	if (virtio_with_feature(dev, VIRTIO_F_VERSION_1))
		sc->vtnet_flags |= VTNET_FLAG_MODERN;
	if (virtio_with_feature(dev, VIRTIO_RING_F_INDIRECT_DESC))
		sc->vtnet_flags |= VTNET_FLAG_INDIRECT;
	if (virtio_with_feature(dev, VIRTIO_RING_F_EVENT_IDX))
		sc->vtnet_flags |= VTNET_FLAG_EVENT_IDX;

	if (virtio_with_feature(dev, VIRTIO_NET_F_MAC)) {
		/* This feature should always be negotiated. */
		sc->vtnet_flags |= VTNET_FLAG_MAC;
	}

	if (virtio_with_feature(dev, VIRTIO_NET_F_MTU)) {
		sc->vtnet_max_mtu = virtio_read_dev_config_2(dev,
		    offsetof(struct virtio_net_config, mtu));
	} else
		sc->vtnet_max_mtu = VTNET_MAX_MTU;

	if (virtio_with_feature(dev, VIRTIO_NET_F_MRG_RXBUF)) {
		sc->vtnet_flags |= VTNET_FLAG_MRG_RXBUFS;
		sc->vtnet_hdr_size = sizeof(struct virtio_net_hdr_mrg_rxbuf);
	} else if (vtnet_modern(sc)) {
		/* This is identical to the mergeable header. */
		sc->vtnet_hdr_size = sizeof(struct virtio_net_hdr_v1);
	} else
		sc->vtnet_hdr_size = sizeof(struct virtio_net_hdr);

	if (vtnet_modern(sc) || sc->vtnet_flags & VTNET_FLAG_MRG_RXBUFS)
		sc->vtnet_rx_nsegs = VTNET_RX_SEGS_HDR_INLINE;
	else if (sc->vtnet_flags & VTNET_FLAG_LRO_NOMRG)
		sc->vtnet_rx_nsegs = VTNET_RX_SEGS_LRO_NOMRG;
	else
		sc->vtnet_rx_nsegs = VTNET_RX_SEGS_HDR_SEPARATE;

	/*
	 * Favor "hardware" LRO if negotiated, but support software LRO as
	 * a fallback; there is usually little benefit (or worse) with both.
	 */
	if (virtio_with_feature(dev, VIRTIO_NET_F_GUEST_TSO4) == 0 &&
	    virtio_with_feature(dev, VIRTIO_NET_F_GUEST_TSO6) == 0)
		sc->vtnet_flags |= VTNET_FLAG_SW_LRO;

	if (virtio_with_feature(dev, VIRTIO_NET_F_GSO) ||
	    virtio_with_feature(dev, VIRTIO_NET_F_HOST_TSO4) ||
	    virtio_with_feature(dev, VIRTIO_NET_F_HOST_TSO6))
		sc->vtnet_tx_nsegs = VTNET_TX_SEGS_MAX;
	else
		sc->vtnet_tx_nsegs = VTNET_TX_SEGS_MIN;

	sc->vtnet_req_vq_pairs = 1;
	sc->vtnet_max_vq_pairs = 1;

	if (virtio_with_feature(dev, VIRTIO_NET_F_CTRL_VQ)) {
		sc->vtnet_flags |= VTNET_FLAG_CTRL_VQ;

		if (virtio_with_feature(dev, VIRTIO_NET_F_CTRL_RX))
			sc->vtnet_flags |= VTNET_FLAG_CTRL_RX;
		if (virtio_with_feature(dev, VIRTIO_NET_F_CTRL_VLAN))
			sc->vtnet_flags |= VTNET_FLAG_VLAN_FILTER;
		if (virtio_with_feature(dev, VIRTIO_NET_F_CTRL_MAC_ADDR))
			sc->vtnet_flags |= VTNET_FLAG_CTRL_MAC;

		if (virtio_with_feature(dev, VIRTIO_NET_F_MQ)) {
			sc->vtnet_max_vq_pairs = virtio_read_dev_config_2(dev,
			    offsetof(struct virtio_net_config,
			    max_virtqueue_pairs));
		}
	}

	if (sc->vtnet_max_vq_pairs > 1) {
		int req;

		/*
		 * Limit the maximum number of requested queue pairs to the
		 * number of CPUs and the configured maximum.
		 */
		req = vtnet_tunable_int(sc, "mq_max_pairs", vtnet_mq_max_pairs);
		if (req < 0)
			req = 1;
		if (req == 0)
			req = mp_ncpus;
		if (req > sc->vtnet_max_vq_pairs)
			req = sc->vtnet_max_vq_pairs;
		if (req > mp_ncpus)
			req = mp_ncpus;
		if (req > 1) {
			sc->vtnet_req_vq_pairs = req;
			sc->vtnet_flags |= VTNET_FLAG_MQ;
		}
	}

	return (0);
}

static int
vtnet_init_rxq(struct vtnet_softc *sc, int id)
{
	struct vtnet_rxq *rxq;

	rxq = &sc->vtnet_rxqs[id];

	snprintf(rxq->vtnrx_name, sizeof(rxq->vtnrx_name), "%s-rx%d",
	    device_get_nameunit(sc->vtnet_dev), id);
	mtx_init(&rxq->vtnrx_mtx, rxq->vtnrx_name, NULL, MTX_DEF);

	rxq->vtnrx_sc = sc;
	rxq->vtnrx_id = id;

	rxq->vtnrx_sg = sglist_alloc(sc->vtnet_rx_nsegs, M_NOWAIT);
	if (rxq->vtnrx_sg == NULL)
		return (ENOMEM);

#if defined(INET) || defined(INET6)
	if (vtnet_software_lro(sc)) {
		if (tcp_lro_init_args(&rxq->vtnrx_lro, sc->vtnet_ifp,
		    sc->vtnet_lro_entry_count, sc->vtnet_lro_mbufq_depth) != 0)
			return (ENOMEM);
	}
#endif

	NET_TASK_INIT(&rxq->vtnrx_intrtask, 0, vtnet_rxq_tq_intr, rxq);
	rxq->vtnrx_tq = taskqueue_create(rxq->vtnrx_name, M_NOWAIT,
	    taskqueue_thread_enqueue, &rxq->vtnrx_tq);

	return (rxq->vtnrx_tq == NULL ? ENOMEM : 0);
}

static int
vtnet_init_txq(struct vtnet_softc *sc, int id)
{
	struct vtnet_txq *txq;

	txq = &sc->vtnet_txqs[id];

	snprintf(txq->vtntx_name, sizeof(txq->vtntx_name), "%s-tx%d",
	    device_get_nameunit(sc->vtnet_dev), id);
	mtx_init(&txq->vtntx_mtx, txq->vtntx_name, NULL, MTX_DEF);

	txq->vtntx_sc = sc;
	txq->vtntx_id = id;

	txq->vtntx_sg = sglist_alloc(sc->vtnet_tx_nsegs, M_NOWAIT);
	if (txq->vtntx_sg == NULL)
		return (ENOMEM);

	if (!VTNET_ALTQ_ENABLED) {
		txq->vtntx_br = buf_ring_alloc(VTNET_DEFAULT_BUFRING_SIZE, M_DEVBUF,
		    M_NOWAIT, &txq->vtntx_mtx);
		if (txq->vtntx_br == NULL)
			return (ENOMEM);

		TASK_INIT(&txq->vtntx_defrtask, 0, vtnet_txq_tq_deferred, txq);
	}
	TASK_INIT(&txq->vtntx_intrtask, 0, vtnet_txq_tq_intr, txq);
	txq->vtntx_tq = taskqueue_create(txq->vtntx_name, M_NOWAIT,
	    taskqueue_thread_enqueue, &txq->vtntx_tq);
	if (txq->vtntx_tq == NULL)
		return (ENOMEM);

	return (0);
}

static int
vtnet_alloc_rxtx_queues(struct vtnet_softc *sc)
{
	int i, npairs, error;

	npairs = sc->vtnet_max_vq_pairs;

	sc->vtnet_rxqs = malloc(sizeof(struct vtnet_rxq) * npairs, M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	sc->vtnet_txqs = malloc(sizeof(struct vtnet_txq) * npairs, M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (sc->vtnet_rxqs == NULL || sc->vtnet_txqs == NULL)
		return (ENOMEM);

	for (i = 0; i < npairs; i++) {
		error = vtnet_init_rxq(sc, i);
		if (error)
			return (error);
		error = vtnet_init_txq(sc, i);
		if (error)
			return (error);
	}

	vtnet_set_rx_process_limit(sc);
	vtnet_setup_queue_sysctl(sc);

	return (0);
}

static void
vtnet_destroy_rxq(struct vtnet_rxq *rxq)
{

	rxq->vtnrx_sc = NULL;
	rxq->vtnrx_id = -1;

#if defined(INET) || defined(INET6)
	tcp_lro_free(&rxq->vtnrx_lro);
#endif

	if (rxq->vtnrx_sg != NULL) {
		sglist_free(rxq->vtnrx_sg);
		rxq->vtnrx_sg = NULL;
	}

	if (mtx_initialized(&rxq->vtnrx_mtx) != 0)
		mtx_destroy(&rxq->vtnrx_mtx);
}

static void
vtnet_destroy_txq(struct vtnet_txq *txq)
{

	txq->vtntx_sc = NULL;
	txq->vtntx_id = -1;

	if (txq->vtntx_sg != NULL) {
		sglist_free(txq->vtntx_sg);
		txq->vtntx_sg = NULL;
	}

	if (!VTNET_ALTQ_ENABLED) {
		if (txq->vtntx_br != NULL) {
			buf_ring_free(txq->vtntx_br, M_DEVBUF);
			txq->vtntx_br = NULL;
		}
	}

	if (mtx_initialized(&txq->vtntx_mtx) != 0)
		mtx_destroy(&txq->vtntx_mtx);
}

static void
vtnet_free_rxtx_queues(struct vtnet_softc *sc)
{
	int i;

	if (sc->vtnet_rxqs != NULL) {
		for (i = 0; i < sc->vtnet_max_vq_pairs; i++)
			vtnet_destroy_rxq(&sc->vtnet_rxqs[i]);
		free(sc->vtnet_rxqs, M_DEVBUF);
		sc->vtnet_rxqs = NULL;
	}

	if (sc->vtnet_txqs != NULL) {
		for (i = 0; i < sc->vtnet_max_vq_pairs; i++)
			vtnet_destroy_txq(&sc->vtnet_txqs[i]);
		free(sc->vtnet_txqs, M_DEVBUF);
		sc->vtnet_txqs = NULL;
	}
}

static int
vtnet_alloc_rx_filters(struct vtnet_softc *sc)
{

	if (sc->vtnet_flags & VTNET_FLAG_CTRL_RX) {
		sc->vtnet_mac_filter = malloc(sizeof(struct vtnet_mac_filter),
		    M_DEVBUF, M_NOWAIT | M_ZERO);
		if (sc->vtnet_mac_filter == NULL)
			return (ENOMEM);
	}

	if (sc->vtnet_flags & VTNET_FLAG_VLAN_FILTER) {
		sc->vtnet_vlan_filter = malloc(sizeof(uint32_t) *
		    VTNET_VLAN_FILTER_NWORDS, M_DEVBUF, M_NOWAIT | M_ZERO);
		if (sc->vtnet_vlan_filter == NULL)
			return (ENOMEM);
	}

	return (0);
}

static void
vtnet_free_rx_filters(struct vtnet_softc *sc)
{

	if (sc->vtnet_mac_filter != NULL) {
		free(sc->vtnet_mac_filter, M_DEVBUF);
		sc->vtnet_mac_filter = NULL;
	}

	if (sc->vtnet_vlan_filter != NULL) {
		free(sc->vtnet_vlan_filter, M_DEVBUF);
		sc->vtnet_vlan_filter = NULL;
	}
}

static int
vtnet_alloc_virtqueues(struct vtnet_softc *sc)
{
	device_t dev;
	struct vq_alloc_info *info;
	struct vtnet_rxq *rxq;
	struct vtnet_txq *txq;
	int i, idx, nvqs, error;

	dev = sc->vtnet_dev;

	nvqs = sc->vtnet_max_vq_pairs * 2;
	if (sc->vtnet_flags & VTNET_FLAG_CTRL_VQ)
		nvqs++;

	info = malloc(sizeof(struct vq_alloc_info) * nvqs, M_TEMP, M_NOWAIT);
	if (info == NULL)
		return (ENOMEM);

	for (i = 0, idx = 0; i < sc->vtnet_req_vq_pairs; i++, idx += 2) {
		rxq = &sc->vtnet_rxqs[i];
		VQ_ALLOC_INFO_INIT(&info[idx], sc->vtnet_rx_nsegs,
		    vtnet_rx_vq_intr, rxq, &rxq->vtnrx_vq,
		    "%s-rx%d", device_get_nameunit(dev), rxq->vtnrx_id);

		txq = &sc->vtnet_txqs[i];
		VQ_ALLOC_INFO_INIT(&info[idx + 1], sc->vtnet_tx_nsegs,
		    vtnet_tx_vq_intr, txq, &txq->vtntx_vq,
		    "%s-tx%d", device_get_nameunit(dev), txq->vtntx_id);
	}

	/* These queues will not be used so allocate the minimum resources. */
	for (; i < sc->vtnet_max_vq_pairs; i++, idx += 2) {
		rxq = &sc->vtnet_rxqs[i];
		VQ_ALLOC_INFO_INIT(&info[idx], 0, NULL, rxq, &rxq->vtnrx_vq,
		    "%s-rx%d", device_get_nameunit(dev), rxq->vtnrx_id);

		txq = &sc->vtnet_txqs[i];
		VQ_ALLOC_INFO_INIT(&info[idx + 1], 0, NULL, txq, &txq->vtntx_vq,
		    "%s-tx%d", device_get_nameunit(dev), txq->vtntx_id);
	}

	if (sc->vtnet_flags & VTNET_FLAG_CTRL_VQ) {
		VQ_ALLOC_INFO_INIT(&info[idx], 0, NULL, NULL,
		    &sc->vtnet_ctrl_vq, "%s ctrl", device_get_nameunit(dev));
	}

	error = virtio_alloc_virtqueues(dev, nvqs, info);
	free(info, M_TEMP);

	return (error);
}

static void
vtnet_alloc_interface(struct vtnet_softc *sc)
{
	device_t dev;
	if_t ifp;

	dev = sc->vtnet_dev;

	ifp = if_alloc(IFT_ETHER);
	sc->vtnet_ifp = ifp;
	if_setsoftc(ifp, sc);
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
}

static int
vtnet_setup_interface(struct vtnet_softc *sc)
{
	device_t dev;
	struct pfil_head_args pa;
	if_t ifp;

	dev = sc->vtnet_dev;
	ifp = sc->vtnet_ifp;

	if_setflags(ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
	if_setbaudrate(ifp, IF_Gbps(10));
	if_setinitfn(ifp, vtnet_init);
	if_setioctlfn(ifp, vtnet_ioctl);
	if_setgetcounterfn(ifp, vtnet_get_counter);

	if (!VTNET_ALTQ_ENABLED) {
		if_settransmitfn(ifp, vtnet_txq_mq_start);
		if_setqflushfn(ifp, vtnet_qflush);
	} else {
		struct virtqueue *vq = sc->vtnet_txqs[0].vtntx_vq;
		if_setstartfn(ifp, vtnet_start);
		if_setsendqlen(ifp, virtqueue_size(vq) - 1);
		if_setsendqready(ifp);
	}

	vtnet_get_macaddr(sc);

	if (virtio_with_feature(dev, VIRTIO_NET_F_STATUS))
		if_setcapabilitiesbit(ifp, IFCAP_LINKSTATE, 0);

	ifmedia_init(&sc->vtnet_media, 0, vtnet_ifmedia_upd, vtnet_ifmedia_sts);
	ifmedia_add(&sc->vtnet_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->vtnet_media, IFM_ETHER | IFM_AUTO);

	if (virtio_with_feature(dev, VIRTIO_NET_F_CSUM)) {
		int gso;

		if_setcapabilitiesbit(ifp, IFCAP_TXCSUM | IFCAP_TXCSUM_IPV6, 0);

		gso = virtio_with_feature(dev, VIRTIO_NET_F_GSO);
		if (gso || virtio_with_feature(dev, VIRTIO_NET_F_HOST_TSO4))
			if_setcapabilitiesbit(ifp, IFCAP_TSO4, 0);
		if (gso || virtio_with_feature(dev, VIRTIO_NET_F_HOST_TSO6))
			if_setcapabilitiesbit(ifp, IFCAP_TSO6, 0);
		if (gso || virtio_with_feature(dev, VIRTIO_NET_F_HOST_ECN))
			sc->vtnet_flags |= VTNET_FLAG_TSO_ECN;

		if (if_getcapabilities(ifp) & (IFCAP_TSO4 | IFCAP_TSO6)) {
			int tso_maxlen;

			if_setcapabilitiesbit(ifp, IFCAP_VLAN_HWTSO, 0);

			tso_maxlen = vtnet_tunable_int(sc, "tso_maxlen",
			    vtnet_tso_maxlen);
			if_sethwtsomax(ifp, tso_maxlen -
			    (ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN));
			if_sethwtsomaxsegcount(ifp, sc->vtnet_tx_nsegs - 1);
			if_sethwtsomaxsegsize(ifp, PAGE_SIZE);
		}
	}

	if (virtio_with_feature(dev, VIRTIO_NET_F_GUEST_CSUM)) {
		if_setcapabilitiesbit(ifp, IFCAP_RXCSUM, 0);
#ifdef notyet
		/* BMV: Rx checksums not distinguished between IPv4 and IPv6. */
		if_setcapabilitiesbit(ifp, IFCAP_RXCSUM_IPV6, 0);
#endif

		if (vtnet_tunable_int(sc, "fixup_needs_csum",
		    vtnet_fixup_needs_csum) != 0)
			sc->vtnet_flags |= VTNET_FLAG_FIXUP_NEEDS_CSUM;

		/* Support either "hardware" or software LRO. */
		if_setcapabilitiesbit(ifp, IFCAP_LRO, 0);
	}

	if (if_getcapabilities(ifp) & (IFCAP_HWCSUM | IFCAP_HWCSUM_IPV6)) {
		/*
		 * VirtIO does not support VLAN tagging, but we can fake
		 * it by inserting and removing the 802.1Q header during
		 * transmit and receive. We are then able to do checksum
		 * offloading of VLAN frames.
		 */
		if_setcapabilitiesbit(ifp, IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_HWCSUM, 0);
	}

	if (sc->vtnet_max_mtu >= ETHERMTU_JUMBO)
		if_setcapabilitiesbit(ifp, IFCAP_JUMBO_MTU, 0);
	if_setcapabilitiesbit(ifp, IFCAP_VLAN_MTU, 0);

	/*
	 * Capabilities after here are not enabled by default.
	 */
	if_setcapenable(ifp, if_getcapabilities(ifp));

	if (sc->vtnet_flags & VTNET_FLAG_VLAN_FILTER) {
		if_setcapabilitiesbit(ifp, IFCAP_VLAN_HWFILTER, 0);

		sc->vtnet_vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
		    vtnet_register_vlan, sc, EVENTHANDLER_PRI_FIRST);
		sc->vtnet_vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
		    vtnet_unregister_vlan, sc, EVENTHANDLER_PRI_FIRST);
	}

	ether_ifattach(ifp, sc->vtnet_hwaddr);

	/* Tell the upper layer(s) we support long frames. */
	if_setifheaderlen(ifp, sizeof(struct ether_vlan_header));

	DEBUGNET_SET(ifp, vtnet);

	pa.pa_version = PFIL_VERSION;
	pa.pa_flags = PFIL_IN;
	pa.pa_type = PFIL_TYPE_ETHERNET;
	pa.pa_headname = if_name(ifp);
	sc->vtnet_pfil = pfil_head_register(&pa);

	return (0);
}

static int
vtnet_rx_cluster_size(struct vtnet_softc *sc, int mtu)
{
	int framesz;

	if (sc->vtnet_flags & VTNET_FLAG_MRG_RXBUFS)
		return (MJUMPAGESIZE);
	else if (sc->vtnet_flags & VTNET_FLAG_LRO_NOMRG)
		return (MCLBYTES);

	/*
	 * Try to scale the receive mbuf cluster size from the MTU. We
	 * could also use the VQ size to influence the selected size,
	 * but that would only matter for very small queues.
	 */
	if (vtnet_modern(sc)) {
		MPASS(sc->vtnet_hdr_size == sizeof(struct virtio_net_hdr_v1));
		framesz = sizeof(struct virtio_net_hdr_v1);
	} else
		framesz = sizeof(struct vtnet_rx_header);
	framesz += sizeof(struct ether_vlan_header) + mtu;
	/*
	 * Account for the offsetting we'll do elsewhere so we allocate the
	 * right size for the mtu.
	 */
	if (VTNET_ETHER_ALIGN != 0 && sc->vtnet_hdr_size % 4 == 0) {
		framesz += VTNET_ETHER_ALIGN;
	}

	if (framesz <= MCLBYTES)
		return (MCLBYTES);
	else if (framesz <= MJUMPAGESIZE)
		return (MJUMPAGESIZE);
	else if (framesz <= MJUM9BYTES)
		return (MJUM9BYTES);

	/* Sane default; avoid 16KB clusters. */
	return (MCLBYTES);
}

static int
vtnet_ioctl_mtu(struct vtnet_softc *sc, u_int mtu)
{
	if_t ifp;
	int clustersz;

	ifp = sc->vtnet_ifp;
	VTNET_CORE_LOCK_ASSERT(sc);

	if (if_getmtu(ifp) == mtu)
		return (0);
	else if (mtu < ETHERMIN || mtu > sc->vtnet_max_mtu)
		return (EINVAL);

	if_setmtu(ifp, mtu);
	clustersz = vtnet_rx_cluster_size(sc, mtu);

	if (clustersz != sc->vtnet_rx_clustersz &&
	    if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
		if_setdrvflagbits(ifp, 0, IFF_DRV_RUNNING);
		vtnet_init_locked(sc, 0);
	}

	return (0);
}

static int
vtnet_ioctl_ifflags(struct vtnet_softc *sc)
{
	if_t ifp;
	int drv_running;

	ifp = sc->vtnet_ifp;
	drv_running = (if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0;

	VTNET_CORE_LOCK_ASSERT(sc);

	if ((if_getflags(ifp) & IFF_UP) == 0) {
		if (drv_running)
			vtnet_stop(sc);
		goto out;
	}

	if (!drv_running) {
		vtnet_init_locked(sc, 0);
		goto out;
	}

	if ((if_getflags(ifp) ^ sc->vtnet_if_flags) &
	    (IFF_PROMISC | IFF_ALLMULTI)) {
		if (sc->vtnet_flags & VTNET_FLAG_CTRL_RX)
			vtnet_rx_filter(sc);
		else {
			/*
			 * We don't support filtering out multicast, so
			 * ALLMULTI is always set.
			 */
			if_setflagbits(ifp, IFF_ALLMULTI, 0);
			if_setflagbits(ifp, IFF_PROMISC, 0);
		}
	}

out:
	sc->vtnet_if_flags = if_getflags(ifp);
	return (0);
}

static int
vtnet_ioctl_multi(struct vtnet_softc *sc)
{
	if_t ifp;

	ifp = sc->vtnet_ifp;

	VTNET_CORE_LOCK_ASSERT(sc);

	if (sc->vtnet_flags & VTNET_FLAG_CTRL_RX &&
	    if_getdrvflags(ifp) & IFF_DRV_RUNNING)
		vtnet_rx_filter_mac(sc);

	return (0);
}

static int
vtnet_ioctl_ifcap(struct vtnet_softc *sc, struct ifreq *ifr)
{
	if_t ifp;
	int mask, reinit, update;

	ifp = sc->vtnet_ifp;
	mask = (ifr->ifr_reqcap & if_getcapabilities(ifp)) ^ if_getcapenable(ifp);
	reinit = update = 0;

	VTNET_CORE_LOCK_ASSERT(sc);

	if (mask & IFCAP_TXCSUM)
		if_togglecapenable(ifp, IFCAP_TXCSUM);
	if (mask & IFCAP_TXCSUM_IPV6)
		if_togglecapenable(ifp, IFCAP_TXCSUM_IPV6);
	if (mask & IFCAP_TSO4)
		if_togglecapenable(ifp, IFCAP_TSO4);
	if (mask & IFCAP_TSO6)
		if_togglecapenable(ifp, IFCAP_TSO6);

	if (mask & (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6 | IFCAP_LRO)) {
		/*
		 * These Rx features require the negotiated features to
		 * be updated. Avoid a full reinit if possible.
		 */
		if (sc->vtnet_features & VIRTIO_NET_F_CTRL_GUEST_OFFLOADS)
			update = 1;
		else
			reinit = 1;

		/* BMV: Avoid needless renegotiation for just software LRO. */
		if ((mask & (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6 | IFCAP_LRO)) ==
		    IFCAP_LRO && vtnet_software_lro(sc))
			reinit = update = 0;

		if (mask & IFCAP_RXCSUM)
			if_togglecapenable(ifp, IFCAP_RXCSUM);
		if (mask & IFCAP_RXCSUM_IPV6)
			if_togglecapenable(ifp, IFCAP_RXCSUM_IPV6);
		if (mask & IFCAP_LRO)
			if_togglecapenable(ifp, IFCAP_LRO);

		/*
		 * VirtIO does not distinguish between IPv4 and IPv6 checksums
		 * so treat them as a pair. Guest TSO (LRO) requires receive
		 * checksums.
		 */
		if (if_getcapenable(ifp) & (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6)) {
			if_setcapenablebit(ifp, IFCAP_RXCSUM, 0);
#ifdef notyet
			if_setcapenablebit(ifp, IFCAP_RXCSUM_IPV6, 0);
#endif
		} else
			if_setcapenablebit(ifp, 0,
			    (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6 | IFCAP_LRO));
	}

	if (mask & IFCAP_VLAN_HWFILTER) {
		/* These Rx features require renegotiation. */
		reinit = 1;

		if (mask & IFCAP_VLAN_HWFILTER)
			if_togglecapenable(ifp, IFCAP_VLAN_HWFILTER);
	}

	if (mask & IFCAP_VLAN_HWTSO)
		if_togglecapenable(ifp, IFCAP_VLAN_HWTSO);
	if (mask & IFCAP_VLAN_HWTAGGING)
		if_togglecapenable(ifp, IFCAP_VLAN_HWTAGGING);

	if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
		if (reinit) {
			if_setdrvflagbits(ifp, 0, IFF_DRV_RUNNING);
			vtnet_init_locked(sc, 0);
		} else if (update)
			vtnet_update_rx_offloads(sc);
	}

	return (0);
}

static int
vtnet_ioctl(if_t ifp, u_long cmd, caddr_t data)
{
	struct vtnet_softc *sc;
	struct ifreq *ifr;
	int error;

	sc = if_getsoftc(ifp);
	ifr = (struct ifreq *) data;
	error = 0;

	switch (cmd) {
	case SIOCSIFMTU:
		VTNET_CORE_LOCK(sc);
		error = vtnet_ioctl_mtu(sc, ifr->ifr_mtu);
		VTNET_CORE_UNLOCK(sc);
		break;

	case SIOCSIFFLAGS:
		VTNET_CORE_LOCK(sc);
		error = vtnet_ioctl_ifflags(sc);
		VTNET_CORE_UNLOCK(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		VTNET_CORE_LOCK(sc);
		error = vtnet_ioctl_multi(sc);
		VTNET_CORE_UNLOCK(sc);
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->vtnet_media, cmd);
		break;

	case SIOCSIFCAP:
		VTNET_CORE_LOCK(sc);
		error = vtnet_ioctl_ifcap(sc, ifr);
		VTNET_CORE_UNLOCK(sc);
		VLAN_CAPABILITIES(ifp);
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	VTNET_CORE_LOCK_ASSERT_NOTOWNED(sc);

	return (error);
}

static int
vtnet_rxq_populate(struct vtnet_rxq *rxq)
{
	struct virtqueue *vq;
	int nbufs, error;

#ifdef DEV_NETMAP
	error = vtnet_netmap_rxq_populate(rxq);
	if (error >= 0)
		return (error);
#endif  /* DEV_NETMAP */

	vq = rxq->vtnrx_vq;
	error = ENOSPC;

	for (nbufs = 0; !virtqueue_full(vq); nbufs++) {
		error = vtnet_rxq_new_buf(rxq);
		if (error)
			break;
	}

	if (nbufs > 0) {
		virtqueue_notify(vq);
		/*
		 * EMSGSIZE signifies the virtqueue did not have enough
		 * entries available to hold the last mbuf. This is not
		 * an error.
		 */
		if (error == EMSGSIZE)
			error = 0;
	}

	return (error);
}

static void
vtnet_rxq_free_mbufs(struct vtnet_rxq *rxq)
{
	struct virtqueue *vq;
	struct mbuf *m;
	int last;
#ifdef DEV_NETMAP
	struct netmap_kring *kring = netmap_kring_on(NA(rxq->vtnrx_sc->vtnet_ifp),
							rxq->vtnrx_id, NR_RX);
#else  /* !DEV_NETMAP */
	void *kring = NULL;
#endif /* !DEV_NETMAP */

	vq = rxq->vtnrx_vq;
	last = 0;

	while ((m = virtqueue_drain(vq, &last)) != NULL) {
		if (kring == NULL)
			m_freem(m);
	}

	KASSERT(virtqueue_empty(vq),
	    ("%s: mbufs remaining in rx queue %p", __func__, rxq));
}

static struct mbuf *
vtnet_rx_alloc_buf(struct vtnet_softc *sc, int nbufs, struct mbuf **m_tailp)
{
	struct mbuf *m_head, *m_tail, *m;
	int i, size;

	m_head = NULL;
	size = sc->vtnet_rx_clustersz;

	KASSERT(nbufs == 1 || sc->vtnet_flags & VTNET_FLAG_LRO_NOMRG,
	    ("%s: mbuf %d chain requested without LRO_NOMRG", __func__, nbufs));

	for (i = 0; i < nbufs; i++) {
		m = m_getjcl(M_NOWAIT, MT_DATA, i == 0 ? M_PKTHDR : 0, size);
		if (m == NULL) {
			sc->vtnet_stats.mbuf_alloc_failed++;
			m_freem(m_head);
			return (NULL);
		}

		m->m_len = size;
		/*
		 * Need to offset the mbuf if the header we're going to add
		 * will misalign.
		 */
		if (VTNET_ETHER_ALIGN != 0 && sc->vtnet_hdr_size % 4 == 0) {
			m_adj(m, VTNET_ETHER_ALIGN);
		}
		if (m_head != NULL) {
			m_tail->m_next = m;
			m_tail = m;
		} else
			m_head = m_tail = m;
	}

	if (m_tailp != NULL)
		*m_tailp = m_tail;

	return (m_head);
}

/*
 * Slow path for when LRO without mergeable buffers is negotiated.
 */
static int
vtnet_rxq_replace_lro_nomrg_buf(struct vtnet_rxq *rxq, struct mbuf *m0,
    int len0)
{
	struct vtnet_softc *sc;
	struct mbuf *m, *m_prev, *m_new, *m_tail;
	int len, clustersz, nreplace, error;

	sc = rxq->vtnrx_sc;
	clustersz = sc->vtnet_rx_clustersz;
	/*
	 * Need to offset the mbuf if the header we're going to add will
	 * misalign, account for that here.
	 */
	if (VTNET_ETHER_ALIGN != 0 && sc->vtnet_hdr_size % 4 == 0)
		clustersz -= VTNET_ETHER_ALIGN;

	m_prev = NULL;
	m_tail = NULL;
	nreplace = 0;

	m = m0;
	len = len0;

	/*
	 * Since these mbuf chains are so large, avoid allocating a complete
	 * replacement when the received frame did not consume the entire
	 * chain. Unused mbufs are moved to the tail of the replacement mbuf.
	 */
	while (len > 0) {
		if (m == NULL) {
			sc->vtnet_stats.rx_frame_too_large++;
			return (EMSGSIZE);
		}

		/*
		 * Every mbuf should have the expected cluster size since that
		 * is also used to allocate the replacements.
		 */
		KASSERT(m->m_len == clustersz,
		    ("%s: mbuf size %d not expected cluster size %d", __func__,
		    m->m_len, clustersz));

		m->m_len = MIN(m->m_len, len);
		len -= m->m_len;

		m_prev = m;
		m = m->m_next;
		nreplace++;
	}

	KASSERT(nreplace > 0 && nreplace <= sc->vtnet_rx_nmbufs,
	    ("%s: invalid replacement mbuf count %d max %d", __func__,
	    nreplace, sc->vtnet_rx_nmbufs));

	m_new = vtnet_rx_alloc_buf(sc, nreplace, &m_tail);
	if (m_new == NULL) {
		m_prev->m_len = clustersz;
		return (ENOBUFS);
	}

	/*
	 * Move any unused mbufs from the received mbuf chain onto the
	 * end of the replacement chain.
	 */
	if (m_prev->m_next != NULL) {
		m_tail->m_next = m_prev->m_next;
		m_prev->m_next = NULL;
	}

	error = vtnet_rxq_enqueue_buf(rxq, m_new);
	if (error) {
		/*
		 * The replacement is suppose to be an copy of the one
		 * dequeued so this is a very unexpected error.
		 *
		 * Restore the m0 chain to the original state if it was
		 * modified so we can then discard it.
		 */
		if (m_tail->m_next != NULL) {
			m_prev->m_next = m_tail->m_next;
			m_tail->m_next = NULL;
		}
		m_prev->m_len = clustersz;
		sc->vtnet_stats.rx_enq_replacement_failed++;
		m_freem(m_new);
	}

	return (error);
}

static int
vtnet_rxq_replace_buf(struct vtnet_rxq *rxq, struct mbuf *m, int len)
{
	struct vtnet_softc *sc;
	struct mbuf *m_new;
	int error;

	sc = rxq->vtnrx_sc;

	if (sc->vtnet_flags & VTNET_FLAG_LRO_NOMRG)
		return (vtnet_rxq_replace_lro_nomrg_buf(rxq, m, len));

	MPASS(m->m_next == NULL);
	if (m->m_len < len)
		return (EMSGSIZE);

	m_new = vtnet_rx_alloc_buf(sc, 1, NULL);
	if (m_new == NULL)
		return (ENOBUFS);

	error = vtnet_rxq_enqueue_buf(rxq, m_new);
	if (error) {
		sc->vtnet_stats.rx_enq_replacement_failed++;
		m_freem(m_new);
	} else
		m->m_len = len;

	return (error);
}

static int
vtnet_rxq_enqueue_buf(struct vtnet_rxq *rxq, struct mbuf *m)
{
	struct vtnet_softc *sc;
	struct sglist *sg;
	int header_inlined, error;

	sc = rxq->vtnrx_sc;
	sg = rxq->vtnrx_sg;

	KASSERT(m->m_next == NULL || sc->vtnet_flags & VTNET_FLAG_LRO_NOMRG,
	    ("%s: mbuf chain without LRO_NOMRG", __func__));
	VTNET_RXQ_LOCK_ASSERT(rxq);

	sglist_reset(sg);
	header_inlined = vtnet_modern(sc) ||
	    (sc->vtnet_flags & VTNET_FLAG_MRG_RXBUFS) != 0; /* TODO: ANY_LAYOUT */

	/*
	 * Note: The mbuf has been already adjusted when we allocate it if we
	 * have to do strict alignment.
	 */
	if (header_inlined)
		error = sglist_append_mbuf(sg, m);
	else {
		struct vtnet_rx_header *rxhdr =
		    mtod(m, struct vtnet_rx_header *);
		MPASS(sc->vtnet_hdr_size == sizeof(struct virtio_net_hdr));

		/* Append the header and remaining mbuf data. */
		error = sglist_append(sg, &rxhdr->vrh_hdr, sc->vtnet_hdr_size);
		if (error)
			return (error);
		error = sglist_append(sg, &rxhdr[1],
		    m->m_len - sizeof(struct vtnet_rx_header));
		if (error)
			return (error);

		if (m->m_next != NULL)
			error = sglist_append_mbuf(sg, m->m_next);
	}

	if (error)
		return (error);

	return (virtqueue_enqueue(rxq->vtnrx_vq, m, sg, 0, sg->sg_nseg));
}

static int
vtnet_rxq_new_buf(struct vtnet_rxq *rxq)
{
	struct vtnet_softc *sc;
	struct mbuf *m;
	int error;

	sc = rxq->vtnrx_sc;

	m = vtnet_rx_alloc_buf(sc, sc->vtnet_rx_nmbufs, NULL);
	if (m == NULL)
		return (ENOBUFS);

	error = vtnet_rxq_enqueue_buf(rxq, m);
	if (error)
		m_freem(m);

	return (error);
}

static int
vtnet_rxq_csum_needs_csum(struct vtnet_rxq *rxq, struct mbuf *m, uint16_t etype,
    int hoff, struct virtio_net_hdr *hdr)
{
	struct vtnet_softc *sc;
	int error;

	sc = rxq->vtnrx_sc;

	/*
	 * NEEDS_CSUM corresponds to Linux's CHECKSUM_PARTIAL, but FreeBSD does
	 * not have an analogous CSUM flag. The checksum has been validated,
	 * but is incomplete (TCP/UDP pseudo header).
	 *
	 * The packet is likely from another VM on the same host that itself
	 * performed checksum offloading so Tx/Rx is basically a memcpy and
	 * the checksum has little value.
	 *
	 * Default to receiving the packet as-is for performance reasons, but
	 * this can cause issues if the packet is to be forwarded because it
	 * does not contain a valid checksum. This patch may be helpful:
	 * https://reviews.freebsd.org/D6611. In the meantime, have the driver
	 * compute the checksum if requested.
	 *
	 * BMV: Need to add an CSUM_PARTIAL flag?
	 */
	if ((sc->vtnet_flags & VTNET_FLAG_FIXUP_NEEDS_CSUM) == 0) {
		error = vtnet_rxq_csum_data_valid(rxq, m, etype, hoff, hdr);
		return (error);
	}

	/*
	 * Compute the checksum in the driver so the packet will contain a
	 * valid checksum. The checksum is at csum_offset from csum_start.
	 */
	switch (etype) {
#if defined(INET) || defined(INET6)
	case ETHERTYPE_IP:
	case ETHERTYPE_IPV6: {
		int csum_off, csum_end;
		uint16_t csum;

		csum_off = hdr->csum_start + hdr->csum_offset;
		csum_end = csum_off + sizeof(uint16_t);

		/* Assume checksum will be in the first mbuf. */
		if (m->m_len < csum_end || m->m_pkthdr.len < csum_end)
			return (1);

		/*
		 * Like in_delayed_cksum()/in6_delayed_cksum(), compute the
		 * checksum and write it at the specified offset. We could
		 * try to verify the packet: csum_start should probably
		 * correspond to the start of the TCP/UDP header.
		 *
		 * BMV: Need to properly handle UDP with zero checksum. Is
		 * the IPv4 header checksum implicitly validated?
		 */
		csum = in_cksum_skip(m, m->m_pkthdr.len, hdr->csum_start);
		*(uint16_t *)(mtodo(m, csum_off)) = csum;
		m->m_pkthdr.csum_flags |= CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
		m->m_pkthdr.csum_data = 0xFFFF;
		break;
	}
#endif
	default:
		sc->vtnet_stats.rx_csum_bad_ethtype++;
		return (1);
	}

	return (0);
}

static int
vtnet_rxq_csum_data_valid(struct vtnet_rxq *rxq, struct mbuf *m,
    uint16_t etype, int hoff, struct virtio_net_hdr *hdr __unused)
{
#if 0
	struct vtnet_softc *sc;
#endif
	int protocol;

#if 0
	sc = rxq->vtnrx_sc;
#endif

	switch (etype) {
#if defined(INET)
	case ETHERTYPE_IP:
		if (__predict_false(m->m_len < hoff + sizeof(struct ip)))
			protocol = IPPROTO_DONE;
		else {
			struct ip *ip = (struct ip *)(m->m_data + hoff);
			protocol = ip->ip_p;
		}
		break;
#endif
#if defined(INET6)
	case ETHERTYPE_IPV6:
		if (__predict_false(m->m_len < hoff + sizeof(struct ip6_hdr))
		    || ip6_lasthdr(m, hoff, IPPROTO_IPV6, &protocol) < 0)
			protocol = IPPROTO_DONE;
		break;
#endif
	default:
		protocol = IPPROTO_DONE;
		break;
	}

	switch (protocol) {
	case IPPROTO_TCP:
	case IPPROTO_UDP:
		m->m_pkthdr.csum_flags |= CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
		m->m_pkthdr.csum_data = 0xFFFF;
		break;
	default:
		/*
		 * FreeBSD does not support checksum offloading of this
		 * protocol. Let the stack re-verify the checksum later
		 * if the protocol is supported.
		 */
#if 0
		if_printf(sc->vtnet_ifp,
		    "%s: checksum offload of unsupported protocol "
		    "etype=%#x protocol=%d csum_start=%d csum_offset=%d\n",
		    __func__, etype, protocol, hdr->csum_start,
		    hdr->csum_offset);
#endif
		break;
	}

	return (0);
}

static int
vtnet_rxq_csum(struct vtnet_rxq *rxq, struct mbuf *m,
    struct virtio_net_hdr *hdr)
{
	const struct ether_header *eh;
	int hoff;
	uint16_t etype;

	eh = mtod(m, const struct ether_header *);
	etype = ntohs(eh->ether_type);
	if (etype == ETHERTYPE_VLAN) {
		/* TODO BMV: Handle QinQ. */
		const struct ether_vlan_header *evh =
		    mtod(m, const struct ether_vlan_header *);
		etype = ntohs(evh->evl_proto);
		hoff = sizeof(struct ether_vlan_header);
	} else
		hoff = sizeof(struct ether_header);

	if (hdr->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM)
		return (vtnet_rxq_csum_needs_csum(rxq, m, etype, hoff, hdr));
	else /* VIRTIO_NET_HDR_F_DATA_VALID */
		return (vtnet_rxq_csum_data_valid(rxq, m, etype, hoff, hdr));
}

static void
vtnet_rxq_discard_merged_bufs(struct vtnet_rxq *rxq, int nbufs)
{
	struct mbuf *m;

	while (--nbufs > 0) {
		m = virtqueue_dequeue(rxq->vtnrx_vq, NULL);
		if (m == NULL)
			break;
		vtnet_rxq_discard_buf(rxq, m);
	}
}

static void
vtnet_rxq_discard_buf(struct vtnet_rxq *rxq, struct mbuf *m)
{
	int error __diagused;

	/*
	 * Requeue the discarded mbuf. This should always be successful
	 * since it was just dequeued.
	 */
	error = vtnet_rxq_enqueue_buf(rxq, m);
	KASSERT(error == 0,
	    ("%s: cannot requeue discarded mbuf %d", __func__, error));
}

static int
vtnet_rxq_merged_eof(struct vtnet_rxq *rxq, struct mbuf *m_head, int nbufs)
{
	struct vtnet_softc *sc;
	struct virtqueue *vq;
	struct mbuf *m_tail;

	sc = rxq->vtnrx_sc;
	vq = rxq->vtnrx_vq;
	m_tail = m_head;

	while (--nbufs > 0) {
		struct mbuf *m;
		uint32_t len;

		m = virtqueue_dequeue(vq, &len);
		if (m == NULL) {
			rxq->vtnrx_stats.vrxs_ierrors++;
			goto fail;
		}

		if (vtnet_rxq_new_buf(rxq) != 0) {
			rxq->vtnrx_stats.vrxs_iqdrops++;
			vtnet_rxq_discard_buf(rxq, m);
			if (nbufs > 1)
				vtnet_rxq_discard_merged_bufs(rxq, nbufs);
			goto fail;
		}

		if (m->m_len < len)
			len = m->m_len;

		m->m_len = len;
		m->m_flags &= ~M_PKTHDR;

		m_head->m_pkthdr.len += len;
		m_tail->m_next = m;
		m_tail = m;
	}

	return (0);

fail:
	sc->vtnet_stats.rx_mergeable_failed++;
	m_freem(m_head);

	return (1);
}

#if defined(INET) || defined(INET6)
static int
vtnet_lro_rx(struct vtnet_rxq *rxq, struct mbuf *m)
{
	struct lro_ctrl *lro;

	lro = &rxq->vtnrx_lro;

	if (lro->lro_mbuf_max != 0) {
		tcp_lro_queue_mbuf(lro, m);
		return (0);
	}

	return (tcp_lro_rx(lro, m, 0));
}
#endif

static void
vtnet_rxq_input(struct vtnet_rxq *rxq, struct mbuf *m,
    struct virtio_net_hdr *hdr)
{
	struct vtnet_softc *sc;
	if_t ifp;

	sc = rxq->vtnrx_sc;
	ifp = sc->vtnet_ifp;

	if (if_getcapenable(ifp) & IFCAP_VLAN_HWTAGGING) {
		struct ether_header *eh = mtod(m, struct ether_header *);
		if (eh->ether_type == htons(ETHERTYPE_VLAN)) {
			vtnet_vlan_tag_remove(m);
			/*
			 * With the 802.1Q header removed, update the
			 * checksum starting location accordingly.
			 */
			if (hdr->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM)
				hdr->csum_start -= ETHER_VLAN_ENCAP_LEN;
		}
	}

	m->m_pkthdr.flowid = rxq->vtnrx_id;
	M_HASHTYPE_SET(m, M_HASHTYPE_OPAQUE);

	if (hdr->flags &
	    (VIRTIO_NET_HDR_F_NEEDS_CSUM | VIRTIO_NET_HDR_F_DATA_VALID)) {
		if (vtnet_rxq_csum(rxq, m, hdr) == 0)
			rxq->vtnrx_stats.vrxs_csum++;
		else
			rxq->vtnrx_stats.vrxs_csum_failed++;
	}

	if (hdr->gso_size != 0) {
		switch (hdr->gso_type & ~VIRTIO_NET_HDR_GSO_ECN) {
		case VIRTIO_NET_HDR_GSO_TCPV4:
		case VIRTIO_NET_HDR_GSO_TCPV6:
			m->m_pkthdr.lro_nsegs =
			    howmany(m->m_pkthdr.len, hdr->gso_size);
			rxq->vtnrx_stats.vrxs_host_lro++;
			break;
		}
	}

	rxq->vtnrx_stats.vrxs_ipackets++;
	rxq->vtnrx_stats.vrxs_ibytes += m->m_pkthdr.len;

#if defined(INET) || defined(INET6)
	if (vtnet_software_lro(sc) && if_getcapenable(ifp) & IFCAP_LRO) {
		if (vtnet_lro_rx(rxq, m) == 0)
			return;
	}
#endif

	if_input(ifp, m);
}

static int
vtnet_rxq_eof(struct vtnet_rxq *rxq)
{
	struct virtio_net_hdr lhdr, *hdr;
	struct vtnet_softc *sc;
	if_t ifp;
	struct virtqueue *vq;
	int deq, count;

	sc = rxq->vtnrx_sc;
	vq = rxq->vtnrx_vq;
	ifp = sc->vtnet_ifp;
	deq = 0;
	count = sc->vtnet_rx_process_limit;

	VTNET_RXQ_LOCK_ASSERT(rxq);

	CURVNET_SET(if_getvnet(ifp));
	while (count-- > 0) {
		struct mbuf *m;
		uint32_t len, nbufs, adjsz;

		m = virtqueue_dequeue(vq, &len);
		if (m == NULL)
			break;
		deq++;

		if (len < sc->vtnet_hdr_size + ETHER_HDR_LEN) {
			rxq->vtnrx_stats.vrxs_ierrors++;
			vtnet_rxq_discard_buf(rxq, m);
			continue;
		}

		if (sc->vtnet_flags & VTNET_FLAG_MRG_RXBUFS) {
			struct virtio_net_hdr_mrg_rxbuf *mhdr =
			    mtod(m, struct virtio_net_hdr_mrg_rxbuf *);
			kmsan_mark(mhdr, sizeof(*mhdr), KMSAN_STATE_INITED);
			nbufs = vtnet_htog16(sc, mhdr->num_buffers);
			adjsz = sizeof(struct virtio_net_hdr_mrg_rxbuf);
		} else if (vtnet_modern(sc)) {
			nbufs = 1; /* num_buffers is always 1 */
			adjsz = sizeof(struct virtio_net_hdr_v1);
		} else {
			nbufs = 1;
			adjsz = sizeof(struct vtnet_rx_header);
			/*
			 * Account for our gap between the header and start of
			 * data to keep the segments separated.
			 */
			len += VTNET_RX_HEADER_PAD;
		}

		if (vtnet_rxq_replace_buf(rxq, m, len) != 0) {
			rxq->vtnrx_stats.vrxs_iqdrops++;
			vtnet_rxq_discard_buf(rxq, m);
			if (nbufs > 1)
				vtnet_rxq_discard_merged_bufs(rxq, nbufs);
			continue;
		}

		m->m_pkthdr.len = len;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.csum_flags = 0;

		if (nbufs > 1) {
			/* Dequeue the rest of chain. */
			if (vtnet_rxq_merged_eof(rxq, m, nbufs) != 0)
				continue;
		}

		kmsan_mark_mbuf(m, KMSAN_STATE_INITED);

		/*
		 * Save an endian swapped version of the header prior to it
		 * being stripped. The header is always at the start of the
		 * mbuf data. num_buffers was already saved (and not needed)
		 * so use the standard header.
		 */
		hdr = mtod(m, struct virtio_net_hdr *);
		lhdr.flags = hdr->flags;
		lhdr.gso_type = hdr->gso_type;
		lhdr.hdr_len = vtnet_htog16(sc, hdr->hdr_len);
		lhdr.gso_size = vtnet_htog16(sc, hdr->gso_size);
		lhdr.csum_start = vtnet_htog16(sc, hdr->csum_start);
		lhdr.csum_offset = vtnet_htog16(sc, hdr->csum_offset);
		m_adj(m, adjsz);

		if (PFIL_HOOKED_IN(sc->vtnet_pfil)) {
			pfil_return_t pfil;

			pfil = pfil_mbuf_in(sc->vtnet_pfil, &m, ifp, NULL);
			switch (pfil) {
			case PFIL_DROPPED:
			case PFIL_CONSUMED:
				continue;
			default:
				KASSERT(pfil == PFIL_PASS,
				    ("Filter returned %d!", pfil));
			}
		}

		vtnet_rxq_input(rxq, m, &lhdr);
	}

	if (deq > 0) {
#if defined(INET) || defined(INET6)
		if (vtnet_software_lro(sc))
			tcp_lro_flush_all(&rxq->vtnrx_lro);
#endif
		virtqueue_notify(vq);
	}
	CURVNET_RESTORE();

	return (count > 0 ? 0 : EAGAIN);
}

static void
vtnet_rx_vq_process(struct vtnet_rxq *rxq, int tries)
{
	struct vtnet_softc *sc;
	if_t ifp;
	u_int more;
#ifdef DEV_NETMAP
	int nmirq;
#endif /* DEV_NETMAP */

	sc = rxq->vtnrx_sc;
	ifp = sc->vtnet_ifp;

	if (__predict_false(rxq->vtnrx_id >= sc->vtnet_act_vq_pairs)) {
		/*
		 * Ignore this interrupt. Either this is a spurious interrupt
		 * or multiqueue without per-VQ MSIX so every queue needs to
		 * be polled (a brain dead configuration we could try harder
		 * to avoid).
		 */
		vtnet_rxq_disable_intr(rxq);
		return;
	}

	VTNET_RXQ_LOCK(rxq);

#ifdef DEV_NETMAP
	/*
	 * We call netmap_rx_irq() under lock to prevent concurrent calls.
	 * This is not necessary to serialize the access to the RX vq, but
	 * rather to avoid races that may happen if this interface is
	 * attached to a VALE switch, which would cause received packets
	 * to stall in the RX queue (nm_kr_tryget() could find the kring
	 * busy when called from netmap_bwrap_intr_notify()).
	 */
	nmirq = netmap_rx_irq(ifp, rxq->vtnrx_id, &more);
	if (nmirq != NM_IRQ_PASS) {
		VTNET_RXQ_UNLOCK(rxq);
		if (nmirq == NM_IRQ_RESCHED) {
			taskqueue_enqueue(rxq->vtnrx_tq, &rxq->vtnrx_intrtask);
		}
		return;
	}
#endif /* DEV_NETMAP */

again:
	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0) {
		VTNET_RXQ_UNLOCK(rxq);
		return;
	}

	more = vtnet_rxq_eof(rxq);
	if (more || vtnet_rxq_enable_intr(rxq) != 0) {
		if (!more)
			vtnet_rxq_disable_intr(rxq);
		/*
		 * This is an occasional condition or race (when !more),
		 * so retry a few times before scheduling the taskqueue.
		 */
		if (tries-- > 0)
			goto again;

		rxq->vtnrx_stats.vrxs_rescheduled++;
		VTNET_RXQ_UNLOCK(rxq);
		taskqueue_enqueue(rxq->vtnrx_tq, &rxq->vtnrx_intrtask);
	} else
		VTNET_RXQ_UNLOCK(rxq);
}

static void
vtnet_rx_vq_intr(void *xrxq)
{
	struct vtnet_rxq *rxq;

	rxq = xrxq;
	vtnet_rx_vq_process(rxq, VTNET_INTR_DISABLE_RETRIES);
}

static void
vtnet_rxq_tq_intr(void *xrxq, int pending __unused)
{
	struct vtnet_rxq *rxq;

	rxq = xrxq;
	vtnet_rx_vq_process(rxq, 0);
}

static int
vtnet_txq_intr_threshold(struct vtnet_txq *txq)
{
	struct vtnet_softc *sc;
	int threshold;

	sc = txq->vtntx_sc;

	/*
	 * The Tx interrupt is disabled until the queue free count falls
	 * below our threshold. Completed frames are drained from the Tx
	 * virtqueue before transmitting new frames and in the watchdog
	 * callout, so the frequency of Tx interrupts is greatly reduced,
	 * at the cost of not freeing mbufs as quickly as they otherwise
	 * would be.
	 */
	threshold = virtqueue_size(txq->vtntx_vq) / 4;

	/*
	 * Without indirect descriptors, leave enough room for the most
	 * segments we handle.
	 */
	if ((sc->vtnet_flags & VTNET_FLAG_INDIRECT) == 0 &&
	    threshold < sc->vtnet_tx_nsegs)
		threshold = sc->vtnet_tx_nsegs;

	return (threshold);
}

static int
vtnet_txq_below_threshold(struct vtnet_txq *txq)
{
	struct virtqueue *vq;

	vq = txq->vtntx_vq;

	return (virtqueue_nfree(vq) <= txq->vtntx_intr_threshold);
}

static int
vtnet_txq_notify(struct vtnet_txq *txq)
{
	struct virtqueue *vq;

	vq = txq->vtntx_vq;

	txq->vtntx_watchdog = VTNET_TX_TIMEOUT;
	virtqueue_notify(vq);

	if (vtnet_txq_enable_intr(txq) == 0)
		return (0);

	/*
	 * Drain frames that were completed since last checked. If this
	 * causes the queue to go above the threshold, the caller should
	 * continue transmitting.
	 */
	if (vtnet_txq_eof(txq) != 0 && vtnet_txq_below_threshold(txq) == 0) {
		virtqueue_disable_intr(vq);
		return (1);
	}

	return (0);
}

static void
vtnet_txq_free_mbufs(struct vtnet_txq *txq)
{
	struct virtqueue *vq;
	struct vtnet_tx_header *txhdr;
	int last;
#ifdef DEV_NETMAP
	struct netmap_kring *kring = netmap_kring_on(NA(txq->vtntx_sc->vtnet_ifp),
							txq->vtntx_id, NR_TX);
#else  /* !DEV_NETMAP */
	void *kring = NULL;
#endif /* !DEV_NETMAP */

	vq = txq->vtntx_vq;
	last = 0;

	while ((txhdr = virtqueue_drain(vq, &last)) != NULL) {
		if (kring == NULL) {
			m_freem(txhdr->vth_mbuf);
			uma_zfree(vtnet_tx_header_zone, txhdr);
		}
	}

	KASSERT(virtqueue_empty(vq),
	    ("%s: mbufs remaining in tx queue %p", __func__, txq));
}

/*
 * BMV: This can go away once we finally have offsets in the mbuf header.
 */
static int
vtnet_txq_offload_ctx(struct vtnet_txq *txq, struct mbuf *m, int *etype,
    int *proto, int *start)
{
	struct vtnet_softc *sc;
	struct ether_vlan_header *evh;
#if defined(INET) || defined(INET6)
	int offset;
#endif

	sc = txq->vtntx_sc;

	evh = mtod(m, struct ether_vlan_header *);
	if (evh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		/* BMV: We should handle nested VLAN tags too. */
		*etype = ntohs(evh->evl_proto);
#if defined(INET) || defined(INET6)
		offset = sizeof(struct ether_vlan_header);
#endif
	} else {
		*etype = ntohs(evh->evl_encap_proto);
#if defined(INET) || defined(INET6)
		offset = sizeof(struct ether_header);
#endif
	}

	switch (*etype) {
#if defined(INET)
	case ETHERTYPE_IP: {
		struct ip *ip, iphdr;
		if (__predict_false(m->m_len < offset + sizeof(struct ip))) {
			m_copydata(m, offset, sizeof(struct ip),
			    (caddr_t) &iphdr);
			ip = &iphdr;
		} else
			ip = (struct ip *)(m->m_data + offset);
		*proto = ip->ip_p;
		*start = offset + (ip->ip_hl << 2);
		break;
	}
#endif
#if defined(INET6)
	case ETHERTYPE_IPV6:
		*proto = -1;
		*start = ip6_lasthdr(m, offset, IPPROTO_IPV6, proto);
		/* Assert the network stack sent us a valid packet. */
		KASSERT(*start > offset,
		    ("%s: mbuf %p start %d offset %d proto %d", __func__, m,
		    *start, offset, *proto));
		break;
#endif
	default:
		sc->vtnet_stats.tx_csum_unknown_ethtype++;
		return (EINVAL);
	}

	return (0);
}

static int
vtnet_txq_offload_tso(struct vtnet_txq *txq, struct mbuf *m, int eth_type,
    int offset, struct virtio_net_hdr *hdr)
{
	static struct timeval lastecn;
	static int curecn;
	struct vtnet_softc *sc;
	struct tcphdr *tcp, tcphdr;

	sc = txq->vtntx_sc;

	if (__predict_false(m->m_len < offset + sizeof(struct tcphdr))) {
		m_copydata(m, offset, sizeof(struct tcphdr), (caddr_t) &tcphdr);
		tcp = &tcphdr;
	} else
		tcp = (struct tcphdr *)(m->m_data + offset);

	hdr->hdr_len = vtnet_gtoh16(sc, offset + (tcp->th_off << 2));
	hdr->gso_size = vtnet_gtoh16(sc, m->m_pkthdr.tso_segsz);
	hdr->gso_type = eth_type == ETHERTYPE_IP ? VIRTIO_NET_HDR_GSO_TCPV4 :
	    VIRTIO_NET_HDR_GSO_TCPV6;

	if (__predict_false(tcp_get_flags(tcp) & TH_CWR)) {
		/*
		 * Drop if VIRTIO_NET_F_HOST_ECN was not negotiated. In
		 * FreeBSD, ECN support is not on a per-interface basis,
		 * but globally via the net.inet.tcp.ecn.enable sysctl
		 * knob. The default is off.
		 */
		if ((sc->vtnet_flags & VTNET_FLAG_TSO_ECN) == 0) {
			if (ppsratecheck(&lastecn, &curecn, 1))
				if_printf(sc->vtnet_ifp,
				    "TSO with ECN not negotiated with host\n");
			return (ENOTSUP);
		}
		hdr->gso_type |= VIRTIO_NET_HDR_GSO_ECN;
	}

	txq->vtntx_stats.vtxs_tso++;

	return (0);
}

static struct mbuf *
vtnet_txq_offload(struct vtnet_txq *txq, struct mbuf *m,
    struct virtio_net_hdr *hdr)
{
	struct vtnet_softc *sc;
	int flags, etype, csum_start, proto, error;

	sc = txq->vtntx_sc;
	flags = m->m_pkthdr.csum_flags;

	error = vtnet_txq_offload_ctx(txq, m, &etype, &proto, &csum_start);
	if (error)
		goto drop;

	if (flags & (VTNET_CSUM_OFFLOAD | VTNET_CSUM_OFFLOAD_IPV6)) {
		/* Sanity check the parsed mbuf matches the offload flags. */
		if (__predict_false((flags & VTNET_CSUM_OFFLOAD &&
		    etype != ETHERTYPE_IP) || (flags & VTNET_CSUM_OFFLOAD_IPV6
		    && etype != ETHERTYPE_IPV6))) {
			sc->vtnet_stats.tx_csum_proto_mismatch++;
			goto drop;
		}

		hdr->flags |= VIRTIO_NET_HDR_F_NEEDS_CSUM;
		hdr->csum_start = vtnet_gtoh16(sc, csum_start);
		hdr->csum_offset = vtnet_gtoh16(sc, m->m_pkthdr.csum_data);
		txq->vtntx_stats.vtxs_csum++;
	}

	if (flags & (CSUM_IP_TSO | CSUM_IP6_TSO)) {
		/*
		 * Sanity check the parsed mbuf IP protocol is TCP, and
		 * VirtIO TSO reqires the checksum offloading above.
		 */
		if (__predict_false(proto != IPPROTO_TCP)) {
			sc->vtnet_stats.tx_tso_not_tcp++;
			goto drop;
		} else if (__predict_false((hdr->flags &
		    VIRTIO_NET_HDR_F_NEEDS_CSUM) == 0)) {
			sc->vtnet_stats.tx_tso_without_csum++;
			goto drop;
		}

		error = vtnet_txq_offload_tso(txq, m, etype, csum_start, hdr);
		if (error)
			goto drop;
	}

	return (m);

drop:
	m_freem(m);
	return (NULL);
}

static int
vtnet_txq_enqueue_buf(struct vtnet_txq *txq, struct mbuf **m_head,
    struct vtnet_tx_header *txhdr)
{
	struct vtnet_softc *sc;
	struct virtqueue *vq;
	struct sglist *sg;
	struct mbuf *m;
	int error;

	sc = txq->vtntx_sc;
	vq = txq->vtntx_vq;
	sg = txq->vtntx_sg;
	m = *m_head;

	sglist_reset(sg);
	error = sglist_append(sg, &txhdr->vth_uhdr, sc->vtnet_hdr_size);
	if (error != 0 || sg->sg_nseg != 1) {
		KASSERT(0, ("%s: cannot add header to sglist error %d nseg %d",
		    __func__, error, sg->sg_nseg));
		goto fail;
	}

	error = sglist_append_mbuf(sg, m);
	if (error) {
		m = m_defrag(m, M_NOWAIT);
		if (m == NULL)
			goto fail;

		*m_head = m;
		sc->vtnet_stats.tx_defragged++;

		error = sglist_append_mbuf(sg, m);
		if (error)
			goto fail;
	}

	txhdr->vth_mbuf = m;
	error = virtqueue_enqueue(vq, txhdr, sg, sg->sg_nseg, 0);

	return (error);

fail:
	sc->vtnet_stats.tx_defrag_failed++;
	m_freem(*m_head);
	*m_head = NULL;

	return (ENOBUFS);
}

static int
vtnet_txq_encap(struct vtnet_txq *txq, struct mbuf **m_head, int flags)
{
	struct vtnet_tx_header *txhdr;
	struct virtio_net_hdr *hdr;
	struct mbuf *m;
	int error;

	m = *m_head;
	M_ASSERTPKTHDR(m);

	txhdr = uma_zalloc(vtnet_tx_header_zone, flags | M_ZERO);
	if (txhdr == NULL) {
		m_freem(m);
		*m_head = NULL;
		return (ENOMEM);
	}

	/*
	 * Always use the non-mergeable header, regardless if mergable headers
	 * were negotiated, because for transmit num_buffers is always zero.
	 * The vtnet_hdr_size is used to enqueue the right header size segment.
	 */
	hdr = &txhdr->vth_uhdr.hdr;

	if (m->m_flags & M_VLANTAG) {
		m = ether_vlanencap(m, m->m_pkthdr.ether_vtag);
		if ((*m_head = m) == NULL) {
			error = ENOBUFS;
			goto fail;
		}
		m->m_flags &= ~M_VLANTAG;
	}

	if (m->m_pkthdr.csum_flags & VTNET_CSUM_ALL_OFFLOAD) {
		m = vtnet_txq_offload(txq, m, hdr);
		if ((*m_head = m) == NULL) {
			error = ENOBUFS;
			goto fail;
		}
	}

	error = vtnet_txq_enqueue_buf(txq, m_head, txhdr);
fail:
	if (error)
		uma_zfree(vtnet_tx_header_zone, txhdr);

	return (error);
}


static void
vtnet_start_locked(struct vtnet_txq *txq, if_t ifp)
{
	struct vtnet_softc *sc;
	struct virtqueue *vq;
	struct mbuf *m0;
	int tries, enq;

	sc = txq->vtntx_sc;
	vq = txq->vtntx_vq;
	tries = 0;

	VTNET_TXQ_LOCK_ASSERT(txq);

	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0 ||
	    sc->vtnet_link_active == 0)
		return;

	vtnet_txq_eof(txq);

again:
	enq = 0;

	while (!if_sendq_empty(ifp)) {
		if (virtqueue_full(vq))
			break;

		m0 = if_dequeue(ifp);
		if (m0 == NULL)
			break;

		if (vtnet_txq_encap(txq, &m0, M_NOWAIT) != 0) {
			if (m0 != NULL)
				if_sendq_prepend(ifp, m0);
			break;
		}

		enq++;
		ETHER_BPF_MTAP(ifp, m0);
	}

	if (enq > 0 && vtnet_txq_notify(txq) != 0) {
		if (tries++ < VTNET_NOTIFY_RETRIES)
			goto again;

		txq->vtntx_stats.vtxs_rescheduled++;
		taskqueue_enqueue(txq->vtntx_tq, &txq->vtntx_intrtask);
	}
}

static void
vtnet_start(if_t ifp)
{
	struct vtnet_softc *sc;
	struct vtnet_txq *txq;

	sc = if_getsoftc(ifp);
	txq = &sc->vtnet_txqs[0];

	VTNET_TXQ_LOCK(txq);
	vtnet_start_locked(txq, ifp);
	VTNET_TXQ_UNLOCK(txq);
}


static int
vtnet_txq_mq_start_locked(struct vtnet_txq *txq, struct mbuf *m)
{
	struct vtnet_softc *sc;
	struct virtqueue *vq;
	struct buf_ring *br;
	if_t ifp;
	int enq, tries, error;

	sc = txq->vtntx_sc;
	vq = txq->vtntx_vq;
	br = txq->vtntx_br;
	ifp = sc->vtnet_ifp;
	tries = 0;
	error = 0;

	VTNET_TXQ_LOCK_ASSERT(txq);

	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0 ||
	    sc->vtnet_link_active == 0) {
		if (m != NULL)
			error = drbr_enqueue(ifp, br, m);
		return (error);
	}

	if (m != NULL) {
		error = drbr_enqueue(ifp, br, m);
		if (error)
			return (error);
	}

	vtnet_txq_eof(txq);

again:
	enq = 0;

	while ((m = drbr_peek(ifp, br)) != NULL) {
		if (virtqueue_full(vq)) {
			drbr_putback(ifp, br, m);
			break;
		}

		if (vtnet_txq_encap(txq, &m, M_NOWAIT) != 0) {
			if (m != NULL)
				drbr_putback(ifp, br, m);
			else
				drbr_advance(ifp, br);
			break;
		}
		drbr_advance(ifp, br);

		enq++;
		ETHER_BPF_MTAP(ifp, m);
	}

	if (enq > 0 && vtnet_txq_notify(txq) != 0) {
		if (tries++ < VTNET_NOTIFY_RETRIES)
			goto again;

		txq->vtntx_stats.vtxs_rescheduled++;
		taskqueue_enqueue(txq->vtntx_tq, &txq->vtntx_intrtask);
	}

	return (0);
}

static int
vtnet_txq_mq_start(if_t ifp, struct mbuf *m)
{
	struct vtnet_softc *sc;
	struct vtnet_txq *txq;
	int i, npairs, error;

	sc = if_getsoftc(ifp);
	npairs = sc->vtnet_act_vq_pairs;

	if (M_HASHTYPE_GET(m) != M_HASHTYPE_NONE)
		i = m->m_pkthdr.flowid % npairs;
	else
		i = curcpu % npairs;

	txq = &sc->vtnet_txqs[i];

	if (VTNET_TXQ_TRYLOCK(txq) != 0) {
		error = vtnet_txq_mq_start_locked(txq, m);
		VTNET_TXQ_UNLOCK(txq);
	} else {
		error = drbr_enqueue(ifp, txq->vtntx_br, m);
		taskqueue_enqueue(txq->vtntx_tq, &txq->vtntx_defrtask);
	}

	return (error);
}

static void
vtnet_txq_tq_deferred(void *xtxq, int pending __unused)
{
	struct vtnet_softc *sc;
	struct vtnet_txq *txq;

	txq = xtxq;
	sc = txq->vtntx_sc;

	VTNET_TXQ_LOCK(txq);
	if (!drbr_empty(sc->vtnet_ifp, txq->vtntx_br))
		vtnet_txq_mq_start_locked(txq, NULL);
	VTNET_TXQ_UNLOCK(txq);
}


static void
vtnet_txq_start(struct vtnet_txq *txq)
{
	struct vtnet_softc *sc;
	if_t ifp;

	sc = txq->vtntx_sc;
	ifp = sc->vtnet_ifp;

	if (!VTNET_ALTQ_ENABLED) {
		if (!drbr_empty(ifp, txq->vtntx_br))
			vtnet_txq_mq_start_locked(txq, NULL);
	} else {
		if (!if_sendq_empty(ifp))
			vtnet_start_locked(txq, ifp);

	}
}

static void
vtnet_txq_tq_intr(void *xtxq, int pending __unused)
{
	struct vtnet_softc *sc;
	struct vtnet_txq *txq;
	if_t ifp;

	txq = xtxq;
	sc = txq->vtntx_sc;
	ifp = sc->vtnet_ifp;

	VTNET_TXQ_LOCK(txq);

	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0) {
		VTNET_TXQ_UNLOCK(txq);
		return;
	}

	vtnet_txq_eof(txq);
	vtnet_txq_start(txq);

	VTNET_TXQ_UNLOCK(txq);
}

static int
vtnet_txq_eof(struct vtnet_txq *txq)
{
	struct virtqueue *vq;
	struct vtnet_tx_header *txhdr;
	struct mbuf *m;
	int deq;

	vq = txq->vtntx_vq;
	deq = 0;
	VTNET_TXQ_LOCK_ASSERT(txq);

	while ((txhdr = virtqueue_dequeue(vq, NULL)) != NULL) {
		m = txhdr->vth_mbuf;
		deq++;

		txq->vtntx_stats.vtxs_opackets++;
		txq->vtntx_stats.vtxs_obytes += m->m_pkthdr.len;
		if (m->m_flags & M_MCAST)
			txq->vtntx_stats.vtxs_omcasts++;

		m_freem(m);
		uma_zfree(vtnet_tx_header_zone, txhdr);
	}

	if (virtqueue_empty(vq))
		txq->vtntx_watchdog = 0;

	return (deq);
}

static void
vtnet_tx_vq_intr(void *xtxq)
{
	struct vtnet_softc *sc;
	struct vtnet_txq *txq;
	if_t ifp;

	txq = xtxq;
	sc = txq->vtntx_sc;
	ifp = sc->vtnet_ifp;

	if (__predict_false(txq->vtntx_id >= sc->vtnet_act_vq_pairs)) {
		/*
		 * Ignore this interrupt. Either this is a spurious interrupt
		 * or multiqueue without per-VQ MSIX so every queue needs to
		 * be polled (a brain dead configuration we could try harder
		 * to avoid).
		 */
		vtnet_txq_disable_intr(txq);
		return;
	}

#ifdef DEV_NETMAP
	if (netmap_tx_irq(ifp, txq->vtntx_id) != NM_IRQ_PASS)
		return;
#endif /* DEV_NETMAP */

	VTNET_TXQ_LOCK(txq);

	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0) {
		VTNET_TXQ_UNLOCK(txq);
		return;
	}

	vtnet_txq_eof(txq);
	vtnet_txq_start(txq);

	VTNET_TXQ_UNLOCK(txq);
}

static void
vtnet_tx_start_all(struct vtnet_softc *sc)
{
	struct vtnet_txq *txq;
	int i;

	VTNET_CORE_LOCK_ASSERT(sc);

	for (i = 0; i < sc->vtnet_act_vq_pairs; i++) {
		txq = &sc->vtnet_txqs[i];

		VTNET_TXQ_LOCK(txq);
		vtnet_txq_start(txq);
		VTNET_TXQ_UNLOCK(txq);
	}
}

static void
vtnet_qflush(if_t ifp)
{
	struct vtnet_softc *sc;
	struct vtnet_txq *txq;
	struct mbuf *m;
	int i;

	sc = if_getsoftc(ifp);

	for (i = 0; i < sc->vtnet_act_vq_pairs; i++) {
		txq = &sc->vtnet_txqs[i];

		VTNET_TXQ_LOCK(txq);
		while ((m = buf_ring_dequeue_sc(txq->vtntx_br)) != NULL)
			m_freem(m);
		VTNET_TXQ_UNLOCK(txq);
	}

	if_qflush(ifp);
}

static int
vtnet_watchdog(struct vtnet_txq *txq)
{
	if_t ifp;

	ifp = txq->vtntx_sc->vtnet_ifp;

	VTNET_TXQ_LOCK(txq);
	if (txq->vtntx_watchdog == 1) {
		/*
		 * Only drain completed frames if the watchdog is about to
		 * expire. If any frames were drained, there may be enough
		 * free descriptors now available to transmit queued frames.
		 * In that case, the timer will immediately be decremented
		 * below, but the timeout is generous enough that should not
		 * be a problem.
		 */
		if (vtnet_txq_eof(txq) != 0)
			vtnet_txq_start(txq);
	}

	if (txq->vtntx_watchdog == 0 || --txq->vtntx_watchdog) {
		VTNET_TXQ_UNLOCK(txq);
		return (0);
	}
	VTNET_TXQ_UNLOCK(txq);

	if_printf(ifp, "watchdog timeout on queue %d\n", txq->vtntx_id);
	return (1);
}

static void
vtnet_accum_stats(struct vtnet_softc *sc, struct vtnet_rxq_stats *rxacc,
    struct vtnet_txq_stats *txacc)
{

	bzero(rxacc, sizeof(struct vtnet_rxq_stats));
	bzero(txacc, sizeof(struct vtnet_txq_stats));

	for (int i = 0; i < sc->vtnet_max_vq_pairs; i++) {
		struct vtnet_rxq_stats *rxst;
		struct vtnet_txq_stats *txst;

		rxst = &sc->vtnet_rxqs[i].vtnrx_stats;
		rxacc->vrxs_ipackets += rxst->vrxs_ipackets;
		rxacc->vrxs_ibytes += rxst->vrxs_ibytes;
		rxacc->vrxs_iqdrops += rxst->vrxs_iqdrops;
		rxacc->vrxs_csum += rxst->vrxs_csum;
		rxacc->vrxs_csum_failed += rxst->vrxs_csum_failed;
		rxacc->vrxs_rescheduled += rxst->vrxs_rescheduled;

		txst = &sc->vtnet_txqs[i].vtntx_stats;
		txacc->vtxs_opackets += txst->vtxs_opackets;
		txacc->vtxs_obytes += txst->vtxs_obytes;
		txacc->vtxs_csum += txst->vtxs_csum;
		txacc->vtxs_tso += txst->vtxs_tso;
		txacc->vtxs_rescheduled += txst->vtxs_rescheduled;
	}
}

static uint64_t
vtnet_get_counter(if_t ifp, ift_counter cnt)
{
	struct vtnet_softc *sc;
	struct vtnet_rxq_stats rxaccum;
	struct vtnet_txq_stats txaccum;

	sc = if_getsoftc(ifp);
	vtnet_accum_stats(sc, &rxaccum, &txaccum);

	switch (cnt) {
	case IFCOUNTER_IPACKETS:
		return (rxaccum.vrxs_ipackets);
	case IFCOUNTER_IQDROPS:
		return (rxaccum.vrxs_iqdrops);
	case IFCOUNTER_IERRORS:
		return (rxaccum.vrxs_ierrors);
	case IFCOUNTER_OPACKETS:
		return (txaccum.vtxs_opackets);
	case IFCOUNTER_OBYTES:
		if (!VTNET_ALTQ_ENABLED)
			return (txaccum.vtxs_obytes);
		/* FALLTHROUGH */
	case IFCOUNTER_OMCASTS:
		if (!VTNET_ALTQ_ENABLED)
			return (txaccum.vtxs_omcasts);
		/* FALLTHROUGH */
	default:
		return (if_get_counter_default(ifp, cnt));
	}
}

static void
vtnet_tick(void *xsc)
{
	struct vtnet_softc *sc;
	if_t ifp;
	int i, timedout;

	sc = xsc;
	ifp = sc->vtnet_ifp;
	timedout = 0;

	VTNET_CORE_LOCK_ASSERT(sc);

	for (i = 0; i < sc->vtnet_act_vq_pairs; i++)
		timedout |= vtnet_watchdog(&sc->vtnet_txqs[i]);

	if (timedout != 0) {
		if_setdrvflagbits(ifp, 0, IFF_DRV_RUNNING);
		vtnet_init_locked(sc, 0);
	} else
		callout_schedule(&sc->vtnet_tick_ch, hz);
}

static void
vtnet_start_taskqueues(struct vtnet_softc *sc)
{
	device_t dev;
	struct vtnet_rxq *rxq;
	struct vtnet_txq *txq;
	int i, error;

	dev = sc->vtnet_dev;

	/*
	 * Errors here are very difficult to recover from - we cannot
	 * easily fail because, if this is during boot, we will hang
	 * when freeing any successfully started taskqueues because
	 * the scheduler isn't up yet.
	 *
	 * Most drivers just ignore the return value - it only fails
	 * with ENOMEM so an error is not likely.
	 */
	for (i = 0; i < sc->vtnet_req_vq_pairs; i++) {
		rxq = &sc->vtnet_rxqs[i];
		error = taskqueue_start_threads(&rxq->vtnrx_tq, 1, PI_NET,
		    "%s rxq %d", device_get_nameunit(dev), rxq->vtnrx_id);
		if (error) {
			device_printf(dev, "failed to start rx taskq %d\n",
			    rxq->vtnrx_id);
		}

		txq = &sc->vtnet_txqs[i];
		error = taskqueue_start_threads(&txq->vtntx_tq, 1, PI_NET,
		    "%s txq %d", device_get_nameunit(dev), txq->vtntx_id);
		if (error) {
			device_printf(dev, "failed to start tx taskq %d\n",
			    txq->vtntx_id);
		}
	}
}

static void
vtnet_free_taskqueues(struct vtnet_softc *sc)
{
	struct vtnet_rxq *rxq;
	struct vtnet_txq *txq;
	int i;

	for (i = 0; i < sc->vtnet_max_vq_pairs; i++) {
		rxq = &sc->vtnet_rxqs[i];
		if (rxq->vtnrx_tq != NULL) {
			taskqueue_free(rxq->vtnrx_tq);
			rxq->vtnrx_tq = NULL;
		}

		txq = &sc->vtnet_txqs[i];
		if (txq->vtntx_tq != NULL) {
			taskqueue_free(txq->vtntx_tq);
			txq->vtntx_tq = NULL;
		}
	}
}

static void
vtnet_drain_taskqueues(struct vtnet_softc *sc)
{
	struct vtnet_rxq *rxq;
	struct vtnet_txq *txq;
	int i;

	for (i = 0; i < sc->vtnet_max_vq_pairs; i++) {
		rxq = &sc->vtnet_rxqs[i];
		if (rxq->vtnrx_tq != NULL)
			taskqueue_drain(rxq->vtnrx_tq, &rxq->vtnrx_intrtask);

		txq = &sc->vtnet_txqs[i];
		if (txq->vtntx_tq != NULL) {
			taskqueue_drain(txq->vtntx_tq, &txq->vtntx_intrtask);
			if (!VTNET_ALTQ_ENABLED)
				taskqueue_drain(txq->vtntx_tq, &txq->vtntx_defrtask);
		}
	}
}

static void
vtnet_drain_rxtx_queues(struct vtnet_softc *sc)
{
	struct vtnet_rxq *rxq;
	struct vtnet_txq *txq;
	int i;

	for (i = 0; i < sc->vtnet_max_vq_pairs; i++) {
		rxq = &sc->vtnet_rxqs[i];
		vtnet_rxq_free_mbufs(rxq);

		txq = &sc->vtnet_txqs[i];
		vtnet_txq_free_mbufs(txq);
	}
}

static void
vtnet_stop_rendezvous(struct vtnet_softc *sc)
{
	struct vtnet_rxq *rxq;
	struct vtnet_txq *txq;
	int i;

	VTNET_CORE_LOCK_ASSERT(sc);

	/*
	 * Lock and unlock the per-queue mutex so we known the stop
	 * state is visible. Doing only the active queues should be
	 * sufficient, but it does not cost much extra to do all the
	 * queues.
	 */
	for (i = 0; i < sc->vtnet_max_vq_pairs; i++) {
		rxq = &sc->vtnet_rxqs[i];
		VTNET_RXQ_LOCK(rxq);
		VTNET_RXQ_UNLOCK(rxq);

		txq = &sc->vtnet_txqs[i];
		VTNET_TXQ_LOCK(txq);
		VTNET_TXQ_UNLOCK(txq);
	}
}

static void
vtnet_stop(struct vtnet_softc *sc)
{
	device_t dev;
	if_t ifp;

	dev = sc->vtnet_dev;
	ifp = sc->vtnet_ifp;

	VTNET_CORE_LOCK_ASSERT(sc);

	if_setdrvflagbits(ifp, 0, IFF_DRV_RUNNING);
	sc->vtnet_link_active = 0;
	callout_stop(&sc->vtnet_tick_ch);

	/* Only advisory. */
	vtnet_disable_interrupts(sc);

#ifdef DEV_NETMAP
	/* Stop any pending txsync/rxsync and disable them. */
	netmap_disable_all_rings(ifp);
#endif /* DEV_NETMAP */

	/*
	 * Stop the host adapter. This resets it to the pre-initialized
	 * state. It will not generate any interrupts until after it is
	 * reinitialized.
	 */
	virtio_stop(dev);
	vtnet_stop_rendezvous(sc);

	vtnet_drain_rxtx_queues(sc);
	sc->vtnet_act_vq_pairs = 1;
}

static int
vtnet_virtio_reinit(struct vtnet_softc *sc)
{
	device_t dev;
	if_t ifp;
	uint64_t features;
	int error;

	dev = sc->vtnet_dev;
	ifp = sc->vtnet_ifp;
	features = sc->vtnet_negotiated_features;

	/*
	 * Re-negotiate with the host, removing any disabled receive
	 * features. Transmit features are disabled only on our side
	 * via if_capenable and if_hwassist.
	 */

	if ((if_getcapenable(ifp) & (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6)) == 0)
		features &= ~(VIRTIO_NET_F_GUEST_CSUM | VTNET_LRO_FEATURES);

	if ((if_getcapenable(ifp) & IFCAP_LRO) == 0)
		features &= ~VTNET_LRO_FEATURES;

	if ((if_getcapenable(ifp) & IFCAP_VLAN_HWFILTER) == 0)
		features &= ~VIRTIO_NET_F_CTRL_VLAN;

	error = virtio_reinit(dev, features);
	if (error) {
		device_printf(dev, "virtio reinit error %d\n", error);
		return (error);
	}

	sc->vtnet_features = features;
	virtio_reinit_complete(dev);

	return (0);
}

static void
vtnet_init_rx_filters(struct vtnet_softc *sc)
{
	if_t ifp;

	ifp = sc->vtnet_ifp;

	if (sc->vtnet_flags & VTNET_FLAG_CTRL_RX) {
		vtnet_rx_filter(sc);
		vtnet_rx_filter_mac(sc);
	}

	if (if_getcapenable(ifp) & IFCAP_VLAN_HWFILTER)
		vtnet_rx_filter_vlan(sc);
}

static int
vtnet_init_rx_queues(struct vtnet_softc *sc)
{
	device_t dev;
	if_t ifp;
	struct vtnet_rxq *rxq;
	int i, clustersz, error;

	dev = sc->vtnet_dev;
	ifp = sc->vtnet_ifp;

	clustersz = vtnet_rx_cluster_size(sc, if_getmtu(ifp));
	sc->vtnet_rx_clustersz = clustersz;

	if (sc->vtnet_flags & VTNET_FLAG_LRO_NOMRG) {
		sc->vtnet_rx_nmbufs = howmany(sizeof(struct vtnet_rx_header) +
		    VTNET_MAX_RX_SIZE, clustersz);
		KASSERT(sc->vtnet_rx_nmbufs < sc->vtnet_rx_nsegs,
		    ("%s: too many rx mbufs %d for %d segments", __func__,
		    sc->vtnet_rx_nmbufs, sc->vtnet_rx_nsegs));
	} else
		sc->vtnet_rx_nmbufs = 1;

	for (i = 0; i < sc->vtnet_act_vq_pairs; i++) {
		rxq = &sc->vtnet_rxqs[i];

		/* Hold the lock to satisfy asserts. */
		VTNET_RXQ_LOCK(rxq);
		error = vtnet_rxq_populate(rxq);
		VTNET_RXQ_UNLOCK(rxq);

		if (error) {
			device_printf(dev, "cannot populate Rx queue %d\n", i);
			return (error);
		}
	}

	return (0);
}

static int
vtnet_init_tx_queues(struct vtnet_softc *sc)
{
	struct vtnet_txq *txq;
	int i;

	for (i = 0; i < sc->vtnet_act_vq_pairs; i++) {
		txq = &sc->vtnet_txqs[i];
		txq->vtntx_watchdog = 0;
		txq->vtntx_intr_threshold = vtnet_txq_intr_threshold(txq);
#ifdef DEV_NETMAP
		netmap_reset(NA(sc->vtnet_ifp), NR_TX, i, 0);
#endif /* DEV_NETMAP */
	}

	return (0);
}

static int
vtnet_init_rxtx_queues(struct vtnet_softc *sc)
{
	int error;

	error = vtnet_init_rx_queues(sc);
	if (error)
		return (error);

	error = vtnet_init_tx_queues(sc);
	if (error)
		return (error);

	return (0);
}

static void
vtnet_set_active_vq_pairs(struct vtnet_softc *sc)
{
	device_t dev;
	int npairs;

	dev = sc->vtnet_dev;

	if ((sc->vtnet_flags & VTNET_FLAG_MQ) == 0) {
		sc->vtnet_act_vq_pairs = 1;
		return;
	}

	npairs = sc->vtnet_req_vq_pairs;

	if (vtnet_ctrl_mq_cmd(sc, npairs) != 0) {
		device_printf(dev, "cannot set active queue pairs to %d, "
		    "falling back to 1 queue pair\n", npairs);
		npairs = 1;
	}

	sc->vtnet_act_vq_pairs = npairs;
}

static void
vtnet_update_rx_offloads(struct vtnet_softc *sc)
{
	if_t ifp;
	uint64_t features;
	int error;

	ifp = sc->vtnet_ifp;
	features = sc->vtnet_features;

	VTNET_CORE_LOCK_ASSERT(sc);

	if (if_getcapabilities(ifp) & (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6)) {
		if (if_getcapenable(ifp) & (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6))
			features |= VIRTIO_NET_F_GUEST_CSUM;
		else
			features &= ~VIRTIO_NET_F_GUEST_CSUM;
	}

	if (if_getcapabilities(ifp) & IFCAP_LRO && !vtnet_software_lro(sc)) {
		if (if_getcapenable(ifp) & IFCAP_LRO)
			features |= VTNET_LRO_FEATURES;
		else
			features &= ~VTNET_LRO_FEATURES;
	}

	error = vtnet_ctrl_guest_offloads(sc,
	    features & (VIRTIO_NET_F_GUEST_CSUM | VIRTIO_NET_F_GUEST_TSO4 |
		        VIRTIO_NET_F_GUEST_TSO6 | VIRTIO_NET_F_GUEST_ECN  |
			VIRTIO_NET_F_GUEST_UFO));
	if (error) {
		device_printf(sc->vtnet_dev,
		    "%s: cannot update Rx features\n", __func__);
		if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
			if_setdrvflagbits(ifp, 0, IFF_DRV_RUNNING);
			vtnet_init_locked(sc, 0);
		}
	} else
		sc->vtnet_features = features;
}

static int
vtnet_reinit(struct vtnet_softc *sc)
{
	if_t ifp;
	int error;

	ifp = sc->vtnet_ifp;

	bcopy(if_getlladdr(ifp), sc->vtnet_hwaddr, ETHER_ADDR_LEN);

	error = vtnet_virtio_reinit(sc);
	if (error)
		return (error);

	vtnet_set_macaddr(sc);
	vtnet_set_active_vq_pairs(sc);

	if (sc->vtnet_flags & VTNET_FLAG_CTRL_VQ)
		vtnet_init_rx_filters(sc);

	if_sethwassist(ifp, 0);
	if (if_getcapenable(ifp) & IFCAP_TXCSUM)
		if_sethwassistbits(ifp, VTNET_CSUM_OFFLOAD, 0);
	if (if_getcapenable(ifp) & IFCAP_TXCSUM_IPV6)
		if_sethwassistbits(ifp, VTNET_CSUM_OFFLOAD_IPV6, 0);
	if (if_getcapenable(ifp) & IFCAP_TSO4)
		if_sethwassistbits(ifp, CSUM_IP_TSO, 0);
	if (if_getcapenable(ifp) & IFCAP_TSO6)
		if_sethwassistbits(ifp, CSUM_IP6_TSO, 0);

	error = vtnet_init_rxtx_queues(sc);
	if (error)
		return (error);

	return (0);
}

static void
vtnet_init_locked(struct vtnet_softc *sc, int init_mode)
{
	if_t ifp;

	ifp = sc->vtnet_ifp;

	VTNET_CORE_LOCK_ASSERT(sc);

	if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
		return;

	vtnet_stop(sc);

#ifdef DEV_NETMAP
	/* Once stopped we can update the netmap flags, if necessary. */
	switch (init_mode) {
	case VTNET_INIT_NETMAP_ENTER:
		nm_set_native_flags(NA(ifp));
		break;
	case VTNET_INIT_NETMAP_EXIT:
		nm_clear_native_flags(NA(ifp));
		break;
	}
#endif /* DEV_NETMAP */

	if (vtnet_reinit(sc) != 0) {
		vtnet_stop(sc);
		return;
	}

	if_setdrvflagbits(ifp, IFF_DRV_RUNNING, 0);
	vtnet_update_link_status(sc);
	vtnet_enable_interrupts(sc);
	callout_reset(&sc->vtnet_tick_ch, hz, vtnet_tick, sc);

#ifdef DEV_NETMAP
	/* Re-enable txsync/rxsync. */
	netmap_enable_all_rings(ifp);
#endif /* DEV_NETMAP */
}

static void
vtnet_init(void *xsc)
{
	struct vtnet_softc *sc;

	sc = xsc;

	VTNET_CORE_LOCK(sc);
	vtnet_init_locked(sc, 0);
	VTNET_CORE_UNLOCK(sc);
}

static void
vtnet_free_ctrl_vq(struct vtnet_softc *sc)
{

	/*
	 * The control virtqueue is only polled and therefore it should
	 * already be empty.
	 */
	KASSERT(virtqueue_empty(sc->vtnet_ctrl_vq),
	    ("%s: ctrl vq %p not empty", __func__, sc->vtnet_ctrl_vq));
}

static void
vtnet_exec_ctrl_cmd(struct vtnet_softc *sc, void *cookie,
    struct sglist *sg, int readable, int writable)
{
	struct virtqueue *vq;

	vq = sc->vtnet_ctrl_vq;

	MPASS(sc->vtnet_flags & VTNET_FLAG_CTRL_VQ);
	VTNET_CORE_LOCK_ASSERT(sc);

	if (!virtqueue_empty(vq))
		return;

	/*
	 * Poll for the response, but the command is likely completed before
	 * returning from the notify.
	 */
	if (virtqueue_enqueue(vq, cookie, sg, readable, writable) == 0)  {
		virtqueue_notify(vq);
		virtqueue_poll(vq, NULL);
	}
}

static int
vtnet_ctrl_mac_cmd(struct vtnet_softc *sc, uint8_t *hwaddr)
{
	struct sglist_seg segs[3];
	struct sglist sg;
	struct {
		struct virtio_net_ctrl_hdr hdr __aligned(2);
		uint8_t pad1;
		uint8_t addr[ETHER_ADDR_LEN] __aligned(8);
		uint8_t pad2;
		uint8_t ack;
	} s;
	int error;

	error = 0;
	MPASS(sc->vtnet_flags & VTNET_FLAG_CTRL_MAC);

	s.hdr.class = VIRTIO_NET_CTRL_MAC;
	s.hdr.cmd = VIRTIO_NET_CTRL_MAC_ADDR_SET;
	bcopy(hwaddr, &s.addr[0], ETHER_ADDR_LEN);
	s.ack = VIRTIO_NET_ERR;

	sglist_init(&sg, nitems(segs), segs);
	error |= sglist_append(&sg, &s.hdr, sizeof(struct virtio_net_ctrl_hdr));
	error |= sglist_append(&sg, &s.addr[0], ETHER_ADDR_LEN);
	error |= sglist_append(&sg, &s.ack, sizeof(uint8_t));
	MPASS(error == 0 && sg.sg_nseg == nitems(segs));

	if (error == 0)
		vtnet_exec_ctrl_cmd(sc, &s.ack, &sg, sg.sg_nseg - 1, 1);

	return (s.ack == VIRTIO_NET_OK ? 0 : EIO);
}

static int
vtnet_ctrl_guest_offloads(struct vtnet_softc *sc, uint64_t offloads)
{
	struct sglist_seg segs[3];
	struct sglist sg;
	struct {
		struct virtio_net_ctrl_hdr hdr __aligned(2);
		uint8_t pad1;
		uint64_t offloads __aligned(8);
		uint8_t pad2;
		uint8_t ack;
	} s;
	int error;

	error = 0;
	MPASS(sc->vtnet_features & VIRTIO_NET_F_CTRL_GUEST_OFFLOADS);

	s.hdr.class = VIRTIO_NET_CTRL_GUEST_OFFLOADS;
	s.hdr.cmd = VIRTIO_NET_CTRL_GUEST_OFFLOADS_SET;
	s.offloads = vtnet_gtoh64(sc, offloads);
	s.ack = VIRTIO_NET_ERR;

	sglist_init(&sg, nitems(segs), segs);
	error |= sglist_append(&sg, &s.hdr, sizeof(struct virtio_net_ctrl_hdr));
	error |= sglist_append(&sg, &s.offloads, sizeof(uint64_t));
	error |= sglist_append(&sg, &s.ack, sizeof(uint8_t));
	MPASS(error == 0 && sg.sg_nseg == nitems(segs));

	if (error == 0)
		vtnet_exec_ctrl_cmd(sc, &s.ack, &sg, sg.sg_nseg - 1, 1);

	return (s.ack == VIRTIO_NET_OK ? 0 : EIO);
}

static int
vtnet_ctrl_mq_cmd(struct vtnet_softc *sc, uint16_t npairs)
{
	struct sglist_seg segs[3];
	struct sglist sg;
	struct {
		struct virtio_net_ctrl_hdr hdr __aligned(2);
		uint8_t pad1;
		struct virtio_net_ctrl_mq mq __aligned(2);
		uint8_t pad2;
		uint8_t ack;
	} s;
	int error;

	error = 0;
	MPASS(sc->vtnet_flags & VTNET_FLAG_MQ);

	s.hdr.class = VIRTIO_NET_CTRL_MQ;
	s.hdr.cmd = VIRTIO_NET_CTRL_MQ_VQ_PAIRS_SET;
	s.mq.virtqueue_pairs = vtnet_gtoh16(sc, npairs);
	s.ack = VIRTIO_NET_ERR;

	sglist_init(&sg, nitems(segs), segs);
	error |= sglist_append(&sg, &s.hdr, sizeof(struct virtio_net_ctrl_hdr));
	error |= sglist_append(&sg, &s.mq, sizeof(struct virtio_net_ctrl_mq));
	error |= sglist_append(&sg, &s.ack, sizeof(uint8_t));
	MPASS(error == 0 && sg.sg_nseg == nitems(segs));

	if (error == 0)
		vtnet_exec_ctrl_cmd(sc, &s.ack, &sg, sg.sg_nseg - 1, 1);

	return (s.ack == VIRTIO_NET_OK ? 0 : EIO);
}

static int
vtnet_ctrl_rx_cmd(struct vtnet_softc *sc, uint8_t cmd, bool on)
{
	struct sglist_seg segs[3];
	struct sglist sg;
	struct {
		struct virtio_net_ctrl_hdr hdr __aligned(2);
		uint8_t pad1;
		uint8_t onoff;
		uint8_t pad2;
		uint8_t ack;
	} s;
	int error;

	error = 0;
	MPASS(sc->vtnet_flags & VTNET_FLAG_CTRL_RX);

	s.hdr.class = VIRTIO_NET_CTRL_RX;
	s.hdr.cmd = cmd;
	s.onoff = on;
	s.ack = VIRTIO_NET_ERR;

	sglist_init(&sg, nitems(segs), segs);
	error |= sglist_append(&sg, &s.hdr, sizeof(struct virtio_net_ctrl_hdr));
	error |= sglist_append(&sg, &s.onoff, sizeof(uint8_t));
	error |= sglist_append(&sg, &s.ack, sizeof(uint8_t));
	MPASS(error == 0 && sg.sg_nseg == nitems(segs));

	if (error == 0)
		vtnet_exec_ctrl_cmd(sc, &s.ack, &sg, sg.sg_nseg - 1, 1);

	return (s.ack == VIRTIO_NET_OK ? 0 : EIO);
}

static int
vtnet_set_promisc(struct vtnet_softc *sc, bool on)
{
	return (vtnet_ctrl_rx_cmd(sc, VIRTIO_NET_CTRL_RX_PROMISC, on));
}

static int
vtnet_set_allmulti(struct vtnet_softc *sc, bool on)
{
	return (vtnet_ctrl_rx_cmd(sc, VIRTIO_NET_CTRL_RX_ALLMULTI, on));
}

static void
vtnet_rx_filter(struct vtnet_softc *sc)
{
	device_t dev;
	if_t ifp;

	dev = sc->vtnet_dev;
	ifp = sc->vtnet_ifp;

	VTNET_CORE_LOCK_ASSERT(sc);

	if (vtnet_set_promisc(sc, if_getflags(ifp) & IFF_PROMISC) != 0) {
		device_printf(dev, "cannot %s promiscuous mode\n",
		    if_getflags(ifp) & IFF_PROMISC ? "enable" : "disable");
	}

	if (vtnet_set_allmulti(sc, if_getflags(ifp) & IFF_ALLMULTI) != 0) {
		device_printf(dev, "cannot %s all-multicast mode\n",
		    if_getflags(ifp) & IFF_ALLMULTI ? "enable" : "disable");
	}
}

static u_int
vtnet_copy_ifaddr(void *arg, struct sockaddr_dl *sdl, u_int ucnt)
{
	struct vtnet_softc *sc = arg;

	if (memcmp(LLADDR(sdl), sc->vtnet_hwaddr, ETHER_ADDR_LEN) == 0)
		return (0);

	if (ucnt < VTNET_MAX_MAC_ENTRIES)
		bcopy(LLADDR(sdl),
		    &sc->vtnet_mac_filter->vmf_unicast.macs[ucnt],
		    ETHER_ADDR_LEN);

	return (1);
}

static u_int
vtnet_copy_maddr(void *arg, struct sockaddr_dl *sdl, u_int mcnt)
{
	struct vtnet_mac_filter *filter = arg;

	if (mcnt < VTNET_MAX_MAC_ENTRIES)
		bcopy(LLADDR(sdl), &filter->vmf_multicast.macs[mcnt],
		    ETHER_ADDR_LEN);

	return (1);
}

static void
vtnet_rx_filter_mac(struct vtnet_softc *sc)
{
	struct virtio_net_ctrl_hdr hdr __aligned(2);
	struct vtnet_mac_filter *filter;
	struct sglist_seg segs[4];
	struct sglist sg;
	if_t ifp;
	bool promisc, allmulti;
	u_int ucnt, mcnt;
	int error;
	uint8_t ack;

	ifp = sc->vtnet_ifp;
	filter = sc->vtnet_mac_filter;
	error = 0;

	MPASS(sc->vtnet_flags & VTNET_FLAG_CTRL_RX);
	VTNET_CORE_LOCK_ASSERT(sc);

	/* Unicast MAC addresses: */
	ucnt = if_foreach_lladdr(ifp, vtnet_copy_ifaddr, sc);
	promisc = (ucnt > VTNET_MAX_MAC_ENTRIES);

	if (promisc) {
		ucnt = 0;
		if_printf(ifp, "more than %d MAC addresses assigned, "
		    "falling back to promiscuous mode\n",
		    VTNET_MAX_MAC_ENTRIES);
	}

	/* Multicast MAC addresses: */
	mcnt = if_foreach_llmaddr(ifp, vtnet_copy_maddr, filter);
	allmulti = (mcnt > VTNET_MAX_MAC_ENTRIES);

	if (allmulti) {
		mcnt = 0;
		if_printf(ifp, "more than %d multicast MAC addresses "
		    "assigned, falling back to all-multicast mode\n",
		    VTNET_MAX_MAC_ENTRIES);
	}

	if (promisc && allmulti)
		goto out;

	filter->vmf_unicast.nentries = vtnet_gtoh32(sc, ucnt);
	filter->vmf_multicast.nentries = vtnet_gtoh32(sc, mcnt);

	hdr.class = VIRTIO_NET_CTRL_MAC;
	hdr.cmd = VIRTIO_NET_CTRL_MAC_TABLE_SET;
	ack = VIRTIO_NET_ERR;

	sglist_init(&sg, nitems(segs), segs);
	error |= sglist_append(&sg, &hdr, sizeof(struct virtio_net_ctrl_hdr));
	error |= sglist_append(&sg, &filter->vmf_unicast,
	    sizeof(uint32_t) + ucnt * ETHER_ADDR_LEN);
	error |= sglist_append(&sg, &filter->vmf_multicast,
	    sizeof(uint32_t) + mcnt * ETHER_ADDR_LEN);
	error |= sglist_append(&sg, &ack, sizeof(uint8_t));
	MPASS(error == 0 && sg.sg_nseg == nitems(segs));

	if (error == 0)
		vtnet_exec_ctrl_cmd(sc, &ack, &sg, sg.sg_nseg - 1, 1);
	if (ack != VIRTIO_NET_OK)
		if_printf(ifp, "error setting host MAC filter table\n");

out:
	if (promisc != 0 && vtnet_set_promisc(sc, true) != 0)
		if_printf(ifp, "cannot enable promiscuous mode\n");
	if (allmulti != 0 && vtnet_set_allmulti(sc, true) != 0)
		if_printf(ifp, "cannot enable all-multicast mode\n");
}

static int
vtnet_exec_vlan_filter(struct vtnet_softc *sc, int add, uint16_t tag)
{
	struct sglist_seg segs[3];
	struct sglist sg;
	struct {
		struct virtio_net_ctrl_hdr hdr __aligned(2);
		uint8_t pad1;
		uint16_t tag __aligned(2);
		uint8_t pad2;
		uint8_t ack;
	} s;
	int error;

	error = 0;
	MPASS(sc->vtnet_flags & VTNET_FLAG_VLAN_FILTER);

	s.hdr.class = VIRTIO_NET_CTRL_VLAN;
	s.hdr.cmd = add ? VIRTIO_NET_CTRL_VLAN_ADD : VIRTIO_NET_CTRL_VLAN_DEL;
	s.tag = vtnet_gtoh16(sc, tag);
	s.ack = VIRTIO_NET_ERR;

	sglist_init(&sg, nitems(segs), segs);
	error |= sglist_append(&sg, &s.hdr, sizeof(struct virtio_net_ctrl_hdr));
	error |= sglist_append(&sg, &s.tag, sizeof(uint16_t));
	error |= sglist_append(&sg, &s.ack, sizeof(uint8_t));
	MPASS(error == 0 && sg.sg_nseg == nitems(segs));

	if (error == 0)
		vtnet_exec_ctrl_cmd(sc, &s.ack, &sg, sg.sg_nseg - 1, 1);

	return (s.ack == VIRTIO_NET_OK ? 0 : EIO);
}

static void
vtnet_rx_filter_vlan(struct vtnet_softc *sc)
{
	int i, bit;
	uint32_t w;
	uint16_t tag;

	MPASS(sc->vtnet_flags & VTNET_FLAG_VLAN_FILTER);
	VTNET_CORE_LOCK_ASSERT(sc);

	/* Enable the filter for each configured VLAN. */
	for (i = 0; i < VTNET_VLAN_FILTER_NWORDS; i++) {
		w = sc->vtnet_vlan_filter[i];

		while ((bit = ffs(w) - 1) != -1) {
			w &= ~(1 << bit);
			tag = sizeof(w) * CHAR_BIT * i + bit;

			if (vtnet_exec_vlan_filter(sc, 1, tag) != 0) {
				device_printf(sc->vtnet_dev,
				    "cannot enable VLAN %d filter\n", tag);
			}
		}
	}
}

static void
vtnet_update_vlan_filter(struct vtnet_softc *sc, int add, uint16_t tag)
{
	if_t ifp;
	int idx, bit;

	ifp = sc->vtnet_ifp;
	idx = (tag >> 5) & 0x7F;
	bit = tag & 0x1F;

	if (tag == 0 || tag > 4095)
		return;

	VTNET_CORE_LOCK(sc);

	if (add)
		sc->vtnet_vlan_filter[idx] |= (1 << bit);
	else
		sc->vtnet_vlan_filter[idx] &= ~(1 << bit);

	if (if_getcapenable(ifp) & IFCAP_VLAN_HWFILTER &&
	    if_getdrvflags(ifp) & IFF_DRV_RUNNING &&
	    vtnet_exec_vlan_filter(sc, add, tag) != 0) {
		device_printf(sc->vtnet_dev,
		    "cannot %s VLAN %d %s the host filter table\n",
		    add ? "add" : "remove", tag, add ? "to" : "from");
	}

	VTNET_CORE_UNLOCK(sc);
}

static void
vtnet_register_vlan(void *arg, if_t ifp, uint16_t tag)
{

	if (if_getsoftc(ifp) != arg)
		return;

	vtnet_update_vlan_filter(arg, 1, tag);
}

static void
vtnet_unregister_vlan(void *arg, if_t ifp, uint16_t tag)
{

	if (if_getsoftc(ifp) != arg)
		return;

	vtnet_update_vlan_filter(arg, 0, tag);
}

static void
vtnet_update_speed_duplex(struct vtnet_softc *sc)
{
	if_t ifp;
	uint32_t speed;

	ifp = sc->vtnet_ifp;

	if ((sc->vtnet_features & VIRTIO_NET_F_SPEED_DUPLEX) == 0)
		return;

	/* BMV: Ignore duplex. */
	speed = virtio_read_dev_config_4(sc->vtnet_dev,
	    offsetof(struct virtio_net_config, speed));
	if (speed != UINT32_MAX)
		if_setbaudrate(ifp, IF_Mbps(speed));
}

static int
vtnet_is_link_up(struct vtnet_softc *sc)
{
	uint16_t status;

	if ((sc->vtnet_features & VIRTIO_NET_F_STATUS) == 0)
		return (1);

	status = virtio_read_dev_config_2(sc->vtnet_dev,
	    offsetof(struct virtio_net_config, status));

	return ((status & VIRTIO_NET_S_LINK_UP) != 0);
}

static void
vtnet_update_link_status(struct vtnet_softc *sc)
{
	if_t ifp;
	int link;

	ifp = sc->vtnet_ifp;
	VTNET_CORE_LOCK_ASSERT(sc);
	link = vtnet_is_link_up(sc);

	/* Notify if the link status has changed. */
	if (link != 0 && sc->vtnet_link_active == 0) {
		vtnet_update_speed_duplex(sc);
		sc->vtnet_link_active = 1;
		if_link_state_change(ifp, LINK_STATE_UP);
	} else if (link == 0 && sc->vtnet_link_active != 0) {
		sc->vtnet_link_active = 0;
		if_link_state_change(ifp, LINK_STATE_DOWN);
	}
}

static int
vtnet_ifmedia_upd(if_t ifp __unused)
{
	return (EOPNOTSUPP);
}

static void
vtnet_ifmedia_sts(if_t ifp, struct ifmediareq *ifmr)
{
	struct vtnet_softc *sc;

	sc = if_getsoftc(ifp);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	VTNET_CORE_LOCK(sc);
	if (vtnet_is_link_up(sc) != 0) {
		ifmr->ifm_status |= IFM_ACTIVE;
		ifmr->ifm_active |= IFM_10G_T | IFM_FDX;
	} else
		ifmr->ifm_active |= IFM_NONE;
	VTNET_CORE_UNLOCK(sc);
}

static void
vtnet_get_macaddr(struct vtnet_softc *sc)
{

	if (sc->vtnet_flags & VTNET_FLAG_MAC) {
		virtio_read_device_config_array(sc->vtnet_dev,
		    offsetof(struct virtio_net_config, mac),
		    &sc->vtnet_hwaddr[0], sizeof(uint8_t), ETHER_ADDR_LEN);
	} else {
		/* Generate a random locally administered unicast address. */
		sc->vtnet_hwaddr[0] = 0xB2;
		arc4rand(&sc->vtnet_hwaddr[1], ETHER_ADDR_LEN - 1, 0);
	}
}

static void
vtnet_set_macaddr(struct vtnet_softc *sc)
{
	device_t dev;
	int error;

	dev = sc->vtnet_dev;

	if (sc->vtnet_flags & VTNET_FLAG_CTRL_MAC) {
		error = vtnet_ctrl_mac_cmd(sc, sc->vtnet_hwaddr);
		if (error)
			device_printf(dev, "unable to set MAC address\n");
		return;
	}

	/* MAC in config is read-only in modern VirtIO. */
	if (!vtnet_modern(sc) && sc->vtnet_flags & VTNET_FLAG_MAC) {
		for (int i = 0; i < ETHER_ADDR_LEN; i++) {
			virtio_write_dev_config_1(dev,
			    offsetof(struct virtio_net_config, mac) + i,
			    sc->vtnet_hwaddr[i]);
		}
	}
}

static void
vtnet_attached_set_macaddr(struct vtnet_softc *sc)
{

	/* Assign MAC address if it was generated. */
	if ((sc->vtnet_flags & VTNET_FLAG_MAC) == 0)
		vtnet_set_macaddr(sc);
}

static void
vtnet_vlan_tag_remove(struct mbuf *m)
{
	struct ether_vlan_header *evh;

	evh = mtod(m, struct ether_vlan_header *);
	m->m_pkthdr.ether_vtag = ntohs(evh->evl_tag);
	m->m_flags |= M_VLANTAG;

	/* Strip the 802.1Q header. */
	bcopy((char *) evh, (char *) evh + ETHER_VLAN_ENCAP_LEN,
	    ETHER_HDR_LEN - ETHER_TYPE_LEN);
	m_adj(m, ETHER_VLAN_ENCAP_LEN);
}

static void
vtnet_set_rx_process_limit(struct vtnet_softc *sc)
{
	int limit;

	limit = vtnet_tunable_int(sc, "rx_process_limit",
	    vtnet_rx_process_limit);
	if (limit < 0)
		limit = INT_MAX;
	sc->vtnet_rx_process_limit = limit;
}

static void
vtnet_setup_rxq_sysctl(struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child, struct vtnet_rxq *rxq)
{
	struct sysctl_oid *node;
	struct sysctl_oid_list *list;
	struct vtnet_rxq_stats *stats;
	char namebuf[16];

	snprintf(namebuf, sizeof(namebuf), "rxq%d", rxq->vtnrx_id);
	node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Receive Queue");
	list = SYSCTL_CHILDREN(node);

	stats = &rxq->vtnrx_stats;

	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "ipackets", CTLFLAG_RD,
	    &stats->vrxs_ipackets, "Receive packets");
	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "ibytes", CTLFLAG_RD,
	    &stats->vrxs_ibytes, "Receive bytes");
	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "iqdrops", CTLFLAG_RD,
	    &stats->vrxs_iqdrops, "Receive drops");
	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "ierrors", CTLFLAG_RD,
	    &stats->vrxs_ierrors, "Receive errors");
	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "csum", CTLFLAG_RD,
	    &stats->vrxs_csum, "Receive checksum offloaded");
	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "csum_failed", CTLFLAG_RD,
	    &stats->vrxs_csum_failed, "Receive checksum offload failed");
	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "host_lro", CTLFLAG_RD,
	    &stats->vrxs_host_lro, "Receive host segmentation offloaded");
	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "rescheduled", CTLFLAG_RD,
	    &stats->vrxs_rescheduled,
	    "Receive interrupt handler rescheduled");
}

static void
vtnet_setup_txq_sysctl(struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child, struct vtnet_txq *txq)
{
	struct sysctl_oid *node;
	struct sysctl_oid_list *list;
	struct vtnet_txq_stats *stats;
	char namebuf[16];

	snprintf(namebuf, sizeof(namebuf), "txq%d", txq->vtntx_id);
	node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Transmit Queue");
	list = SYSCTL_CHILDREN(node);

	stats = &txq->vtntx_stats;

	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "opackets", CTLFLAG_RD,
	    &stats->vtxs_opackets, "Transmit packets");
	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "obytes", CTLFLAG_RD,
	    &stats->vtxs_obytes, "Transmit bytes");
	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "omcasts", CTLFLAG_RD,
	    &stats->vtxs_omcasts, "Transmit multicasts");
	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "csum", CTLFLAG_RD,
	    &stats->vtxs_csum, "Transmit checksum offloaded");
	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "tso", CTLFLAG_RD,
	    &stats->vtxs_tso, "Transmit TCP segmentation offloaded");
	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "rescheduled", CTLFLAG_RD,
	    &stats->vtxs_rescheduled,
	    "Transmit interrupt handler rescheduled");
}

static void
vtnet_setup_queue_sysctl(struct vtnet_softc *sc)
{
	device_t dev;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;
	int i;

	dev = sc->vtnet_dev;
	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	for (i = 0; i < sc->vtnet_req_vq_pairs; i++) {
		vtnet_setup_rxq_sysctl(ctx, child, &sc->vtnet_rxqs[i]);
		vtnet_setup_txq_sysctl(ctx, child, &sc->vtnet_txqs[i]);
	}
}

static void
vtnet_setup_stat_sysctl(struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child, struct vtnet_softc *sc)
{
	struct vtnet_statistics *stats;
	struct vtnet_rxq_stats rxaccum;
	struct vtnet_txq_stats txaccum;

	vtnet_accum_stats(sc, &rxaccum, &txaccum);

	stats = &sc->vtnet_stats;
	stats->rx_csum_offloaded = rxaccum.vrxs_csum;
	stats->rx_csum_failed = rxaccum.vrxs_csum_failed;
	stats->rx_task_rescheduled = rxaccum.vrxs_rescheduled;
	stats->tx_csum_offloaded = txaccum.vtxs_csum;
	stats->tx_tso_offloaded = txaccum.vtxs_tso;
	stats->tx_task_rescheduled = txaccum.vtxs_rescheduled;

	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "mbuf_alloc_failed",
	    CTLFLAG_RD, &stats->mbuf_alloc_failed,
	    "Mbuf cluster allocation failures");

	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "rx_frame_too_large",
	    CTLFLAG_RD, &stats->rx_frame_too_large,
	    "Received frame larger than the mbuf chain");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "rx_enq_replacement_failed",
	    CTLFLAG_RD, &stats->rx_enq_replacement_failed,
	    "Enqueuing the replacement receive mbuf failed");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "rx_mergeable_failed",
	    CTLFLAG_RD, &stats->rx_mergeable_failed,
	    "Mergeable buffers receive failures");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "rx_csum_bad_ethtype",
	    CTLFLAG_RD, &stats->rx_csum_bad_ethtype,
	    "Received checksum offloaded buffer with unsupported "
	    "Ethernet type");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "rx_csum_bad_ipproto",
	    CTLFLAG_RD, &stats->rx_csum_bad_ipproto,
	    "Received checksum offloaded buffer with incorrect IP protocol");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "rx_csum_bad_offset",
	    CTLFLAG_RD, &stats->rx_csum_bad_offset,
	    "Received checksum offloaded buffer with incorrect offset");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "rx_csum_bad_proto",
	    CTLFLAG_RD, &stats->rx_csum_bad_proto,
	    "Received checksum offloaded buffer with incorrect protocol");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "rx_csum_failed",
	    CTLFLAG_RD, &stats->rx_csum_failed,
	    "Received buffer checksum offload failed");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "rx_csum_offloaded",
	    CTLFLAG_RD, &stats->rx_csum_offloaded,
	    "Received buffer checksum offload succeeded");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "rx_task_rescheduled",
	    CTLFLAG_RD, &stats->rx_task_rescheduled,
	    "Times the receive interrupt task rescheduled itself");

	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "tx_csum_unknown_ethtype",
	    CTLFLAG_RD, &stats->tx_csum_unknown_ethtype,
	    "Aborted transmit of checksum offloaded buffer with unknown "
	    "Ethernet type");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "tx_csum_proto_mismatch",
	    CTLFLAG_RD, &stats->tx_csum_proto_mismatch,
	    "Aborted transmit of checksum offloaded buffer because mismatched "
	    "protocols");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "tx_tso_not_tcp",
	    CTLFLAG_RD, &stats->tx_tso_not_tcp,
	    "Aborted transmit of TSO buffer with non TCP protocol");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "tx_tso_without_csum",
	    CTLFLAG_RD, &stats->tx_tso_without_csum,
	    "Aborted transmit of TSO buffer without TCP checksum offload");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "tx_defragged",
	    CTLFLAG_RD, &stats->tx_defragged,
	    "Transmit mbufs defragged");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "tx_defrag_failed",
	    CTLFLAG_RD, &stats->tx_defrag_failed,
	    "Aborted transmit of buffer because defrag failed");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "tx_csum_offloaded",
	    CTLFLAG_RD, &stats->tx_csum_offloaded,
	    "Offloaded checksum of transmitted buffer");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "tx_tso_offloaded",
	    CTLFLAG_RD, &stats->tx_tso_offloaded,
	    "Segmentation offload of transmitted buffer");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "tx_task_rescheduled",
	    CTLFLAG_RD, &stats->tx_task_rescheduled,
	    "Times the transmit interrupt task rescheduled itself");
}

static void
vtnet_setup_sysctl(struct vtnet_softc *sc)
{
	device_t dev;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;

	dev = sc->vtnet_dev;
	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "max_vq_pairs",
	    CTLFLAG_RD, &sc->vtnet_max_vq_pairs, 0,
	    "Number of maximum supported virtqueue pairs");
	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "req_vq_pairs",
	    CTLFLAG_RD, &sc->vtnet_req_vq_pairs, 0,
	    "Number of requested virtqueue pairs");
	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "act_vq_pairs",
	    CTLFLAG_RD, &sc->vtnet_act_vq_pairs, 0,
	    "Number of active virtqueue pairs");

	vtnet_setup_stat_sysctl(ctx, child, sc);
}

static void
vtnet_load_tunables(struct vtnet_softc *sc)
{

	sc->vtnet_lro_entry_count = vtnet_tunable_int(sc,
	    "lro_entry_count", vtnet_lro_entry_count);
	if (sc->vtnet_lro_entry_count < TCP_LRO_ENTRIES)
		sc->vtnet_lro_entry_count = TCP_LRO_ENTRIES;

	sc->vtnet_lro_mbufq_depth = vtnet_tunable_int(sc,
	    "lro_mbufq_depth", vtnet_lro_mbufq_depth);
}

static int
vtnet_rxq_enable_intr(struct vtnet_rxq *rxq)
{

	return (virtqueue_enable_intr(rxq->vtnrx_vq));
}

static void
vtnet_rxq_disable_intr(struct vtnet_rxq *rxq)
{

	virtqueue_disable_intr(rxq->vtnrx_vq);
}

static int
vtnet_txq_enable_intr(struct vtnet_txq *txq)
{
	struct virtqueue *vq;

	vq = txq->vtntx_vq;

	if (vtnet_txq_below_threshold(txq) != 0)
		return (virtqueue_postpone_intr(vq, VQ_POSTPONE_LONG));

	/*
	 * The free count is above our threshold. Keep the Tx interrupt
	 * disabled until the queue is fuller.
	 */
	return (0);
}

static void
vtnet_txq_disable_intr(struct vtnet_txq *txq)
{

	virtqueue_disable_intr(txq->vtntx_vq);
}

static void
vtnet_enable_rx_interrupts(struct vtnet_softc *sc)
{
	struct vtnet_rxq *rxq;
	int i;

	for (i = 0; i < sc->vtnet_act_vq_pairs; i++) {
		rxq = &sc->vtnet_rxqs[i];
		if (vtnet_rxq_enable_intr(rxq) != 0)
			taskqueue_enqueue(rxq->vtnrx_tq, &rxq->vtnrx_intrtask);
	}
}

static void
vtnet_enable_tx_interrupts(struct vtnet_softc *sc)
{
	int i;

	for (i = 0; i < sc->vtnet_act_vq_pairs; i++)
		vtnet_txq_enable_intr(&sc->vtnet_txqs[i]);
}

static void
vtnet_enable_interrupts(struct vtnet_softc *sc)
{

	vtnet_enable_rx_interrupts(sc);
	vtnet_enable_tx_interrupts(sc);
}

static void
vtnet_disable_rx_interrupts(struct vtnet_softc *sc)
{
	int i;

	for (i = 0; i < sc->vtnet_max_vq_pairs; i++)
		vtnet_rxq_disable_intr(&sc->vtnet_rxqs[i]);
}

static void
vtnet_disable_tx_interrupts(struct vtnet_softc *sc)
{
	int i;

	for (i = 0; i < sc->vtnet_max_vq_pairs; i++)
		vtnet_txq_disable_intr(&sc->vtnet_txqs[i]);
}

static void
vtnet_disable_interrupts(struct vtnet_softc *sc)
{

	vtnet_disable_rx_interrupts(sc);
	vtnet_disable_tx_interrupts(sc);
}

static int
vtnet_tunable_int(struct vtnet_softc *sc, const char *knob, int def)
{
	char path[64];

	snprintf(path, sizeof(path),
	    "hw.vtnet.%d.%s", device_get_unit(sc->vtnet_dev), knob);
	TUNABLE_INT_FETCH(path, &def);

	return (def);
}

#ifdef DEBUGNET
static void
vtnet_debugnet_init(if_t ifp, int *nrxr, int *ncl, int *clsize)
{
	struct vtnet_softc *sc;

	sc = if_getsoftc(ifp);

	VTNET_CORE_LOCK(sc);
	*nrxr = sc->vtnet_req_vq_pairs;
	*ncl = DEBUGNET_MAX_IN_FLIGHT;
	*clsize = sc->vtnet_rx_clustersz;
	VTNET_CORE_UNLOCK(sc);
}

static void
vtnet_debugnet_event(if_t ifp __unused, enum debugnet_ev event)
{
	struct vtnet_softc *sc;
	static bool sw_lro_enabled = false;

	/*
	 * Disable software LRO, since it would require entering the network
	 * epoch when calling vtnet_txq_eof() in vtnet_debugnet_poll().
	 */
	sc = if_getsoftc(ifp);
	switch (event) {
	case DEBUGNET_START:
		sw_lro_enabled = (sc->vtnet_flags & VTNET_FLAG_SW_LRO) != 0;
		if (sw_lro_enabled)
			sc->vtnet_flags &= ~VTNET_FLAG_SW_LRO;
		break;
	case DEBUGNET_END:
		if (sw_lro_enabled)
			sc->vtnet_flags |= VTNET_FLAG_SW_LRO;
		break;
	}
}

static int
vtnet_debugnet_transmit(if_t ifp, struct mbuf *m)
{
	struct vtnet_softc *sc;
	struct vtnet_txq *txq;
	int error;

	sc = if_getsoftc(ifp);
	if ((if_getdrvflags(ifp) & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return (EBUSY);

	txq = &sc->vtnet_txqs[0];
	error = vtnet_txq_encap(txq, &m, M_NOWAIT | M_USE_RESERVE);
	if (error == 0)
		(void)vtnet_txq_notify(txq);
	return (error);
}

static int
vtnet_debugnet_poll(if_t ifp, int count)
{
	struct vtnet_softc *sc;
	int i;

	sc = if_getsoftc(ifp);
	if ((if_getdrvflags(ifp) & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return (EBUSY);

	(void)vtnet_txq_eof(&sc->vtnet_txqs[0]);
	for (i = 0; i < sc->vtnet_act_vq_pairs; i++)
		(void)vtnet_rxq_eof(&sc->vtnet_rxqs[i]);
	return (0);
}
#endif /* DEBUGNET */
