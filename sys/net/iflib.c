/*-
 * Copyright (c) 2014-2016, Matthew Macy <mmacy@nextbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *  2. Neither the name of Matthew Macy nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/eventhandler.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/kobj.h>
#include <sys/rman.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/taskqueue.h>


#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/mp_ring.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/tcp_lro.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>

#include <machine/bus.h>
#include <machine/in_cksum.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/led/led.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>

#include <net/iflib.h>

#include "ifdi_if.h"

#if defined(__i386__) || defined(__amd64__)
#include <sys/memdesc.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>
#include <x86/include/busdma_impl.h>
#include <x86/iommu/busdma_dmar.h>
#endif


/*
 * enable accounting of every mbuf as it comes in to and goes out of iflib's software descriptor references
 */
#define MEMORY_LOGGING 0
/*
 * Enable mbuf vectors for compressing long mbuf chains
 */


/*
 * NB:
 * - Prefetching in tx cleaning should perhaps be a tunable. The distance ahead
 *   we prefetch needs to be determined by the time spent in m_free vis a vis
 *   the cost of a prefetch. This will of course vary based on the workload:
 *      - NFLX's m_free path is dominated by vm-based M_EXT manipulation which
 *        is quite expensive, thus suggesting very little prefetch.
 *      - small packet forwarding which is just returning a single mbuf to
 *        UMA will typically be very fast vis a vis the cost of a memory
 *        access.
 */


/*
 * File organization:
 *  - private structures
 *  - iflib private utility functions
 *  - ifnet functions
 *  - vlan registry and other exported functions
 *  - iflib public core functions
 *
 *
 */
static MALLOC_DEFINE(M_IFLIB, "iflib", "ifnet library");

struct iflib_txq;
typedef struct iflib_txq *iflib_txq_t;
struct iflib_rxq;
typedef struct iflib_rxq *iflib_rxq_t;
struct iflib_fl;
typedef struct iflib_fl *iflib_fl_t;

typedef struct iflib_filter_info {
	driver_filter_t *ifi_filter;
	void *ifi_filter_arg;
	struct grouptask *ifi_task;
} *iflib_filter_info_t;

struct iflib_ctx {
	KOBJ_FIELDS;
   /*
   * Pointer to hardware driver's softc
   */
	void *ifc_softc;
	device_t ifc_dev;
	if_t ifc_ifp;

	cpuset_t ifc_cpus;
	if_shared_ctx_t ifc_sctx;
	struct if_softc_ctx ifc_softc_ctx;

	struct mtx ifc_mtx;

	uint16_t ifc_nhwtxqs;
	uint16_t ifc_nhwrxqs;

	iflib_txq_t ifc_txqs;
	iflib_rxq_t ifc_rxqs;
	uint32_t ifc_if_flags;
	uint32_t ifc_flags;
	uint32_t ifc_max_fl_buf_size;
	int ifc_in_detach;

	int ifc_link_state;
	int ifc_link_irq;
	int ifc_pause_frames;
	int ifc_watchdog_events;
	struct cdev *ifc_led_dev;
	struct resource *ifc_msix_mem;

	struct if_irq ifc_legacy_irq;
	struct grouptask ifc_admin_task;
	struct grouptask ifc_vflr_task;
	struct iflib_filter_info ifc_filter_info;
	struct ifmedia	ifc_media;

	struct sysctl_oid *ifc_sysctl_node;
	uint16_t ifc_sysctl_ntxqs;
	uint16_t ifc_sysctl_nrxqs;
	uint16_t ifc_sysctl_ntxds;
	uint16_t ifc_sysctl_nrxds;
	struct if_txrx ifc_txrx;
#define isc_txd_encap  ifc_txrx.ift_txd_encap
#define isc_txd_flush  ifc_txrx.ift_txd_flush
#define isc_txd_credits_update  ifc_txrx.ift_txd_credits_update
#define isc_rxd_available ifc_txrx.ift_rxd_available
#define isc_rxd_pkt_get ifc_txrx.ift_rxd_pkt_get
#define isc_rxd_refill ifc_txrx.ift_rxd_refill
#define isc_rxd_flush ifc_txrx.ift_rxd_flush
#define isc_rxd_refill ifc_txrx.ift_rxd_refill
#define isc_rxd_refill ifc_txrx.ift_rxd_refill
#define isc_legacy_intr ifc_txrx.ift_legacy_intr
	eventhandler_tag ifc_vlan_attach_event;
	eventhandler_tag ifc_vlan_detach_event;
	uint8_t ifc_mac[ETHER_ADDR_LEN];
	char ifc_mtx_name[16];
};


void *
iflib_get_softc(if_ctx_t ctx)
{

	return (ctx->ifc_softc);
}

device_t
iflib_get_dev(if_ctx_t ctx)
{

	return (ctx->ifc_dev);
}

if_t
iflib_get_ifp(if_ctx_t ctx)
{

	return (ctx->ifc_ifp);
}

struct ifmedia *
iflib_get_media(if_ctx_t ctx)
{

	return (&ctx->ifc_media);
}

void
iflib_set_mac(if_ctx_t ctx, uint8_t mac[ETHER_ADDR_LEN])
{

	bcopy(mac, ctx->ifc_mac, ETHER_ADDR_LEN);
}

if_softc_ctx_t
iflib_get_softc_ctx(if_ctx_t ctx)
{

	return (&ctx->ifc_softc_ctx);
}

if_shared_ctx_t
iflib_get_sctx(if_ctx_t ctx)
{

	return (ctx->ifc_sctx);
}

#define CACHE_PTR_INCREMENT (CACHE_LINE_SIZE/sizeof(void*))

#define LINK_ACTIVE(ctx) ((ctx)->ifc_link_state == LINK_STATE_UP)
#define CTX_IS_VF(ctx) ((ctx)->ifc_sctx->isc_flags & IFLIB_IS_VF)

#define RX_SW_DESC_MAP_CREATED	(1 << 0)
#define TX_SW_DESC_MAP_CREATED	(1 << 1)
#define RX_SW_DESC_INUSE        (1 << 3)
#define TX_SW_DESC_MAPPED       (1 << 4)

typedef struct iflib_sw_rx_desc {
	bus_dmamap_t    ifsd_map;         /* bus_dma map for packet */
	struct mbuf    *ifsd_m;           /* rx: uninitialized mbuf */
	caddr_t         ifsd_cl;          /* direct cluster pointer for rx */
	uint16_t	ifsd_flags;
} *iflib_rxsd_t;

typedef struct iflib_sw_tx_desc_val {
	bus_dmamap_t    ifsd_map;         /* bus_dma map for packet */
	struct mbuf    *ifsd_m;           /* pkthdr mbuf */
	uint8_t		ifsd_flags;
} *iflib_txsd_val_t;

typedef struct iflib_sw_tx_desc_array {
	bus_dmamap_t    *ifsd_map;         /* bus_dma maps for packet */
	struct mbuf    **ifsd_m;           /* pkthdr mbufs */
	uint8_t		*ifsd_flags;
} iflib_txsd_array_t;


/* magic number that should be high enough for any hardware */
#define IFLIB_MAX_TX_SEGS		128
#define IFLIB_MAX_RX_SEGS		32
#define IFLIB_RX_COPY_THRESH		128
#define IFLIB_MAX_RX_REFRESH		32
#define IFLIB_QUEUE_IDLE		0
#define IFLIB_QUEUE_HUNG		1
#define IFLIB_QUEUE_WORKING		2

/* this should really scale with ring size - 32 is a fairly arbitrary value for this */
#define TX_BATCH_SIZE			16

#define IFLIB_RESTART_BUDGET		8

#define	IFC_LEGACY		0x1
#define	IFC_QFLUSH		0x2
#define	IFC_MULTISEG		0x4
#define	IFC_DMAR		0x8

#define CSUM_OFFLOAD		(CSUM_IP_TSO|CSUM_IP6_TSO|CSUM_IP| \
				 CSUM_IP_UDP|CSUM_IP_TCP|CSUM_IP_SCTP| \
				 CSUM_IP6_UDP|CSUM_IP6_TCP|CSUM_IP6_SCTP)
struct iflib_txq {
	uint16_t	ift_in_use;
	uint16_t	ift_cidx;
	uint16_t	ift_cidx_processed;
	uint16_t	ift_pidx;
	uint8_t		ift_gen;
	uint8_t		ift_db_pending;
	uint8_t		ift_db_pending_queued;
	uint8_t		ift_npending;
	/* implicit pad */
	uint64_t	ift_processed;
	uint64_t	ift_cleaned;
#if MEMORY_LOGGING
	uint64_t	ift_enqueued;
	uint64_t	ift_dequeued;
#endif
	uint64_t	ift_no_tx_dma_setup;
	uint64_t	ift_no_desc_avail;
	uint64_t	ift_mbuf_defrag_failed;
	uint64_t	ift_mbuf_defrag;
	uint64_t	ift_map_failed;
	uint64_t	ift_txd_encap_efbig;
	uint64_t	ift_pullups;

	struct mtx	ift_mtx;
	struct mtx	ift_db_mtx;

	/* constant values */
	if_ctx_t	ift_ctx;
	struct ifmp_ring        **ift_br;
	struct grouptask	ift_task;
	uint16_t	ift_size;
	uint16_t	ift_id;
	struct callout	ift_timer;
	struct callout	ift_db_check;

	iflib_txsd_array_t	ift_sds;
	uint8_t			ift_nbr;
	uint8_t			ift_qstatus;
	uint8_t			ift_active;
	uint8_t			ift_closed;
	int			ift_watchdog_time;
	struct iflib_filter_info ift_filter_info;
	bus_dma_tag_t		ift_desc_tag;
	bus_dma_tag_t		ift_tso_desc_tag;
	iflib_dma_info_t	ift_ifdi;
#define MTX_NAME_LEN 16
	char                    ift_mtx_name[MTX_NAME_LEN];
	char                    ift_db_mtx_name[MTX_NAME_LEN];
	bus_dma_segment_t	ift_segs[IFLIB_MAX_TX_SEGS]  __aligned(CACHE_LINE_SIZE);
} __aligned(CACHE_LINE_SIZE);

struct iflib_fl {
	uint16_t	ifl_cidx;
	uint16_t	ifl_pidx;
	uint16_t	ifl_credits;
	uint8_t		ifl_gen;
#if MEMORY_LOGGING
	uint64_t	ifl_m_enqueued;
	uint64_t	ifl_m_dequeued;
	uint64_t	ifl_cl_enqueued;
	uint64_t	ifl_cl_dequeued;
#endif
	/* implicit pad */

	/* constant */
	uint16_t	ifl_size;
	uint16_t	ifl_buf_size;
	uint16_t	ifl_cltype;
	uma_zone_t	ifl_zone;
	iflib_rxsd_t	ifl_sds;
	iflib_rxq_t	ifl_rxq;
	uint8_t		ifl_id;
	bus_dma_tag_t           ifl_desc_tag;
	iflib_dma_info_t	ifl_ifdi;
	uint64_t	ifl_bus_addrs[IFLIB_MAX_RX_REFRESH] __aligned(CACHE_LINE_SIZE);
	caddr_t		ifl_vm_addrs[IFLIB_MAX_RX_REFRESH];
}  __aligned(CACHE_LINE_SIZE);

static inline int
get_inuse(int size, int cidx, int pidx, int gen)
{
	int used;

	if (pidx > cidx)
		used = pidx - cidx;
	else if (pidx < cidx)
		used = size - cidx + pidx;
	else if (gen == 0 && pidx == cidx)
		used = 0;
	else if (gen == 1 && pidx == cidx)
		used = size;
	else
		panic("bad state");

	return (used);
}

#define TXQ_AVAIL(txq) (txq->ift_size - get_inuse(txq->ift_size, txq->ift_cidx, txq->ift_pidx, txq->ift_gen))

#define IDXDIFF(head, tail, wrap) \
	((head) >= (tail) ? (head) - (tail) : (wrap) - (tail) + (head))

struct iflib_rxq {
	/* If there is a separate completion queue -
	 * these are the cq cidx and pidx. Otherwise
	 * these are unused.
	 */
	uint16_t	ifr_size;
	uint16_t	ifr_cq_cidx;
	uint16_t	ifr_cq_pidx;
	uint8_t		ifr_cq_gen;

	if_ctx_t	ifr_ctx;
	iflib_fl_t	ifr_fl;
	uint64_t	ifr_rx_irq;
	uint16_t	ifr_id;
	uint8_t		ifr_lro_enabled;
	uint8_t		ifr_nfl;
	struct lro_ctrl			ifr_lc;
	struct grouptask        ifr_task;
	struct iflib_filter_info ifr_filter_info;
	iflib_dma_info_t		ifr_ifdi;
	/* dynamically allocate if any drivers need a value substantially larger than this */
	struct if_rxd_frag	ifr_frags[IFLIB_MAX_RX_SEGS] __aligned(CACHE_LINE_SIZE);
}  __aligned(CACHE_LINE_SIZE);

/*
 * Only allow a single packet to take up most 1/nth of the tx ring
 */
#define MAX_SINGLE_PACKET_FRACTION 12
#define IF_BAD_DMA (bus_addr_t)-1

static int enable_msix = 1;

#define mtx_held(m)	(((m)->mtx_lock & ~MTX_FLAGMASK) != (uintptr_t)0)



#define CTX_ACTIVE(ctx) ((if_getdrvflags((ctx)->ifc_ifp) & IFF_DRV_RUNNING))

#define CTX_LOCK_INIT(_sc, _name)  mtx_init(&(_sc)->ifc_mtx, _name, "iflib ctx lock", MTX_DEF)

#define CTX_LOCK(ctx) mtx_lock(&(ctx)->ifc_mtx)
#define CTX_UNLOCK(ctx) mtx_unlock(&(ctx)->ifc_mtx)
#define CTX_LOCK_DESTROY(ctx) mtx_destroy(&(ctx)->ifc_mtx)


#define TXDB_LOCK_INIT(txq)  mtx_init(&(txq)->ift_db_mtx, (txq)->ift_db_mtx_name, NULL, MTX_DEF)
#define TXDB_TRYLOCK(txq) mtx_trylock(&(txq)->ift_db_mtx)
#define TXDB_LOCK(txq) mtx_lock(&(txq)->ift_db_mtx)
#define TXDB_UNLOCK(txq) mtx_unlock(&(txq)->ift_db_mtx)
#define TXDB_LOCK_DESTROY(txq) mtx_destroy(&(txq)->ift_db_mtx)

#define CALLOUT_LOCK(txq)	mtx_lock(&txq->ift_mtx)
#define CALLOUT_UNLOCK(txq) 	mtx_unlock(&txq->ift_mtx)


/* Our boot-time initialization hook */
static int	iflib_module_event_handler(module_t, int, void *);

static moduledata_t iflib_moduledata = {
	"iflib",
	iflib_module_event_handler,
	NULL
};

DECLARE_MODULE(iflib, iflib_moduledata, SI_SUB_INIT_IF, SI_ORDER_ANY);
MODULE_VERSION(iflib, 1);

MODULE_DEPEND(iflib, pci, 1, 1, 1);
MODULE_DEPEND(iflib, ether, 1, 1, 1);

TASKQGROUP_DEFINE(if_io_tqg, mp_ncpus, 1);
TASKQGROUP_DEFINE(if_config_tqg, 1, 1);

#ifndef IFLIB_DEBUG_COUNTERS
#ifdef INVARIANTS
#define IFLIB_DEBUG_COUNTERS 1
#else
#define IFLIB_DEBUG_COUNTERS 0
#endif /* !INVARIANTS */
#endif

static SYSCTL_NODE(_net, OID_AUTO, iflib, CTLFLAG_RD, 0,
                   "iflib driver parameters");

/*
 * XXX need to ensure that this can't accidentally cause the head to be moved backwards 
 */
static int iflib_min_tx_latency = 0;

SYSCTL_INT(_net_iflib, OID_AUTO, min_tx_latency, CTLFLAG_RW,
		   &iflib_min_tx_latency, 0, "minimize transmit latency at the possibel expense of throughput");


#if IFLIB_DEBUG_COUNTERS

static int iflib_tx_seen;
static int iflib_tx_sent;
static int iflib_tx_encap;
static int iflib_rx_allocs;
static int iflib_fl_refills;
static int iflib_fl_refills_large;
static int iflib_tx_frees;

SYSCTL_INT(_net_iflib, OID_AUTO, tx_seen, CTLFLAG_RD,
		   &iflib_tx_seen, 0, "# tx mbufs seen");
SYSCTL_INT(_net_iflib, OID_AUTO, tx_sent, CTLFLAG_RD,
		   &iflib_tx_sent, 0, "# tx mbufs sent");
SYSCTL_INT(_net_iflib, OID_AUTO, tx_encap, CTLFLAG_RD,
		   &iflib_tx_encap, 0, "# tx mbufs encapped");
SYSCTL_INT(_net_iflib, OID_AUTO, tx_frees, CTLFLAG_RD,
		   &iflib_tx_frees, 0, "# tx frees");
SYSCTL_INT(_net_iflib, OID_AUTO, rx_allocs, CTLFLAG_RD,
		   &iflib_rx_allocs, 0, "# rx allocations");
SYSCTL_INT(_net_iflib, OID_AUTO, fl_refills, CTLFLAG_RD,
		   &iflib_fl_refills, 0, "# refills");
SYSCTL_INT(_net_iflib, OID_AUTO, fl_refills_large, CTLFLAG_RD,
		   &iflib_fl_refills_large, 0, "# large refills");


static int iflib_txq_drain_flushing;
static int iflib_txq_drain_oactive;
static int iflib_txq_drain_notready;
static int iflib_txq_drain_encapfail;

SYSCTL_INT(_net_iflib, OID_AUTO, txq_drain_flushing, CTLFLAG_RD,
		   &iflib_txq_drain_flushing, 0, "# drain flushes");
SYSCTL_INT(_net_iflib, OID_AUTO, txq_drain_oactive, CTLFLAG_RD,
		   &iflib_txq_drain_oactive, 0, "# drain oactives");
SYSCTL_INT(_net_iflib, OID_AUTO, txq_drain_notready, CTLFLAG_RD,
		   &iflib_txq_drain_notready, 0, "# drain notready");
SYSCTL_INT(_net_iflib, OID_AUTO, txq_drain_encapfail, CTLFLAG_RD,
		   &iflib_txq_drain_encapfail, 0, "# drain encap fails");


static int iflib_encap_load_mbuf_fail;
static int iflib_encap_txq_avail_fail;
static int iflib_encap_txd_encap_fail;

SYSCTL_INT(_net_iflib, OID_AUTO, encap_load_mbuf_fail, CTLFLAG_RD,
		   &iflib_encap_load_mbuf_fail, 0, "# busdma load failures");
SYSCTL_INT(_net_iflib, OID_AUTO, encap_txq_avail_fail, CTLFLAG_RD,
		   &iflib_encap_txq_avail_fail, 0, "# txq avail failures");
SYSCTL_INT(_net_iflib, OID_AUTO, encap_txd_encap_fail, CTLFLAG_RD,
		   &iflib_encap_txd_encap_fail, 0, "# driver encap failures");

static int iflib_task_fn_rxs;
static int iflib_rx_intr_enables;
static int iflib_fast_intrs;
static int iflib_intr_link;
static int iflib_intr_msix; 
static int iflib_rx_unavail;
static int iflib_rx_ctx_inactive;
static int iflib_rx_zero_len;
static int iflib_rx_if_input;
static int iflib_rx_mbuf_null;
static int iflib_rxd_flush;

static int iflib_verbose_debug;

SYSCTL_INT(_net_iflib, OID_AUTO, intr_link, CTLFLAG_RD,
		   &iflib_intr_link, 0, "# intr link calls");
SYSCTL_INT(_net_iflib, OID_AUTO, intr_msix, CTLFLAG_RD,
		   &iflib_intr_msix, 0, "# intr msix calls");
SYSCTL_INT(_net_iflib, OID_AUTO, task_fn_rx, CTLFLAG_RD,
		   &iflib_task_fn_rxs, 0, "# task_fn_rx calls");
SYSCTL_INT(_net_iflib, OID_AUTO, rx_intr_enables, CTLFLAG_RD,
		   &iflib_rx_intr_enables, 0, "# rx intr enables");
SYSCTL_INT(_net_iflib, OID_AUTO, fast_intrs, CTLFLAG_RD,
		   &iflib_fast_intrs, 0, "# fast_intr calls");
SYSCTL_INT(_net_iflib, OID_AUTO, rx_unavail, CTLFLAG_RD,
		   &iflib_rx_unavail, 0, "# times rxeof called with no available data");
SYSCTL_INT(_net_iflib, OID_AUTO, rx_ctx_inactive, CTLFLAG_RD,
		   &iflib_rx_ctx_inactive, 0, "# times rxeof called with inactive context");
SYSCTL_INT(_net_iflib, OID_AUTO, rx_zero_len, CTLFLAG_RD,
		   &iflib_rx_zero_len, 0, "# times rxeof saw zero len mbuf");
SYSCTL_INT(_net_iflib, OID_AUTO, rx_if_input, CTLFLAG_RD,
		   &iflib_rx_if_input, 0, "# times rxeof called if_input");
SYSCTL_INT(_net_iflib, OID_AUTO, rx_mbuf_null, CTLFLAG_RD,
		   &iflib_rx_mbuf_null, 0, "# times rxeof got null mbuf");
SYSCTL_INT(_net_iflib, OID_AUTO, rxd_flush, CTLFLAG_RD,
	         &iflib_rxd_flush, 0, "# times rxd_flush called");
SYSCTL_INT(_net_iflib, OID_AUTO, verbose_debug, CTLFLAG_RW,
		   &iflib_verbose_debug, 0, "enable verbose debugging");

#define DBG_COUNTER_INC(name) atomic_add_int(&(iflib_ ## name), 1)

#else
#define DBG_COUNTER_INC(name)

#endif



#define IFLIB_DEBUG 0

static void iflib_tx_structures_free(if_ctx_t ctx);
static void iflib_rx_structures_free(if_ctx_t ctx);
static int iflib_queues_alloc(if_ctx_t ctx);
static int iflib_tx_credits_update(if_ctx_t ctx, iflib_txq_t txq);
static int iflib_rxd_avail(if_ctx_t ctx, iflib_rxq_t rxq, int cidx);
static int iflib_qset_structures_setup(if_ctx_t ctx);
static int iflib_msix_init(if_ctx_t ctx);
static int iflib_legacy_setup(if_ctx_t ctx, driver_filter_t filter, void *filterarg, int *rid, char *str);
static void iflib_txq_check_drain(iflib_txq_t txq, int budget);
static uint32_t iflib_txq_can_drain(struct ifmp_ring *);
static int iflib_register(if_ctx_t);
static void iflib_init_locked(if_ctx_t ctx);
static void iflib_add_device_sysctl_pre(if_ctx_t ctx);
static void iflib_add_device_sysctl_post(if_ctx_t ctx);


#ifdef DEV_NETMAP
#include <sys/selinfo.h>
#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>

MODULE_DEPEND(iflib, netmap, 1, 1, 1);

/*
 * device-specific sysctl variables:
 *
 * ixl_crcstrip: 0: keep CRC in rx frames (default), 1: strip it.
 *	During regular operations the CRC is stripped, but on some
 *	hardware reception of frames not multiple of 64 is slower,
 *	so using crcstrip=0 helps in benchmarks.
 *
 * ixl_rx_miss, ixl_rx_miss_bufs:
 *	count packets that might be missed due to lost interrupts.
 */
SYSCTL_DECL(_dev_netmap);
/*
 * The xl driver by default strips CRCs and we do not override it.
 */

int iflib_crcstrip = 1;
SYSCTL_INT(_dev_netmap, OID_AUTO, iflib_crcstrip,
    CTLFLAG_RW, &iflib_crcstrip, 1, "strip CRC on rx frames");

int iflib_rx_miss, iflib_rx_miss_bufs;
SYSCTL_INT(_dev_netmap, OID_AUTO, iflib_rx_miss,
    CTLFLAG_RW, &iflib_rx_miss, 0, "potentially missed rx intr");
SYSCTL_INT(_dev_netmap, OID_AUTO, ixl_rx_miss_bufs,
    CTLFLAG_RW, &iflib_rx_miss_bufs, 0, "potentially missed rx intr bufs");

/*
 * Register/unregister. We are already under netmap lock.
 * Only called on the first register or the last unregister.
 */
static int
iflib_netmap_register(struct netmap_adapter *na, int onoff)
{
	struct ifnet *ifp = na->ifp;
	if_ctx_t ctx = ifp->if_softc;

	CTX_LOCK(ctx);
	IFDI_INTR_DISABLE(ctx);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	if (!CTX_IS_VF(ctx))
		IFDI_CRCSTRIP_SET(ctx, onoff);

	/* enable or disable flags and callbacks in na and ifp */
	if (onoff) {
		nm_set_native_flags(na);
	} else {
		nm_clear_native_flags(na);
	}
	IFDI_INIT(ctx);
	IFDI_CRCSTRIP_SET(ctx, onoff); // XXX why twice ?
	CTX_UNLOCK(ctx);
	return (ifp->if_drv_flags & IFF_DRV_RUNNING ? 0 : 1);
}

/*
 * Reconcile kernel and user view of the transmit ring.
 *
 * All information is in the kring.
 * Userspace wants to send packets up to the one before kring->rhead,
 * kernel knows kring->nr_hwcur is the first unsent packet.
 *
 * Here we push packets out (as many as possible), and possibly
 * reclaim buffers from previously completed transmission.
 *
 * The caller (netmap) guarantees that there is only one instance
 * running at any time. Any interference with other driver
 * methods should be handled by the individual drivers.
 */
static int
iflib_netmap_txsync(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
	struct ifnet *ifp = na->ifp;
	struct netmap_ring *ring = kring->ring;
	u_int nm_i;	/* index into the netmap ring */
	u_int nic_i;	/* index into the NIC ring */
	u_int n;
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = kring->rhead;
	struct if_pkt_info pi;

	/*
	 * interrupts on every tx packet are expensive so request
	 * them every half ring, or where NS_REPORT is set
	 */
	u_int report_frequency = kring->nkr_num_slots >> 1;
	/* device-specific */
	if_ctx_t ctx = ifp->if_softc;
	iflib_txq_t txq = &ctx->ifc_txqs[kring->ring_id];

	pi.ipi_segs = txq->ift_segs;
	pi.ipi_qsidx = kring->ring_id;
	pi.ipi_ndescs = 0;

	bus_dmamap_sync(txq->ift_desc_tag, txq->ift_ifdi->idi_map,
					BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);


	/*
	 * First part: process new packets to send.
	 * nm_i is the current index in the netmap ring,
	 * nic_i is the corresponding index in the NIC ring.
	 *
	 * If we have packets to send (nm_i != head)
	 * iterate over the netmap ring, fetch length and update
	 * the corresponding slot in the NIC ring. Some drivers also
	 * need to update the buffer's physical address in the NIC slot
	 * even NS_BUF_CHANGED is not set (PNMB computes the addresses).
	 *
	 * The netmap_reload_map() calls is especially expensive,
	 * even when (as in this case) the tag is 0, so do only
	 * when the buffer has actually changed.
	 *
	 * If possible do not set the report/intr bit on all slots,
	 * but only a few times per ring or when NS_REPORT is set.
	 *
	 * Finally, on 10G and faster drivers, it might be useful
	 * to prefetch the next slot and txr entry.
	 */

	nm_i = kring->nr_hwcur;
	if (nm_i != head) {	/* we have new packets to send */
		nic_i = netmap_idx_k2n(kring, nm_i);

		__builtin_prefetch(&ring->slot[nm_i]);
		__builtin_prefetch(&txq->ift_sds.ifsd_m[nic_i]);
		__builtin_prefetch(&txq->ift_sds.ifsd_map[nic_i]);

		for (n = 0; nm_i != head; n++) {
			struct netmap_slot *slot = &ring->slot[nm_i];
			u_int len = slot->len;
			uint64_t paddr;
			void *addr = PNMB(na, slot, &paddr);
			int flags = (slot->flags & NS_REPORT ||
				nic_i == 0 || nic_i == report_frequency) ?
				IPI_TX_INTR : 0;

			/* device-specific */
			pi.ipi_pidx = nic_i;
			pi.ipi_flags = flags;

			/* Fill the slot in the NIC ring. */
			ctx->isc_txd_encap(ctx->ifc_softc, &pi);

			/* prefetch for next round */
			__builtin_prefetch(&ring->slot[nm_i + 1]);
			__builtin_prefetch(&txq->ift_sds.ifsd_m[nic_i + 1]);
			__builtin_prefetch(&txq->ift_sds.ifsd_map[nic_i + 1]);

			NM_CHECK_ADDR_LEN(na, addr, len);

			if (slot->flags & NS_BUF_CHANGED) {
				/* buffer has changed, reload map */
				netmap_reload_map(na, txq->ift_desc_tag, txq->ift_sds.ifsd_map[nic_i], addr);
			}
			slot->flags &= ~(NS_REPORT | NS_BUF_CHANGED);

			/* make sure changes to the buffer are synced */
			bus_dmamap_sync(txq->ift_ifdi->idi_tag, txq->ift_sds.ifsd_map[nic_i],
							BUS_DMASYNC_PREWRITE);

			nm_i = nm_next(nm_i, lim);
			nic_i = nm_next(nic_i, lim);
		}
		kring->nr_hwcur = head;

		/* synchronize the NIC ring */
		bus_dmamap_sync(txq->ift_desc_tag, txq->ift_ifdi->idi_map,
						BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* (re)start the tx unit up to slot nic_i (excluded) */
		ctx->isc_txd_flush(ctx->ifc_softc, txq->ift_id, nic_i);
	}

	/*
	 * Second part: reclaim buffers for completed transmissions.
	 */
	if (iflib_tx_credits_update(ctx, txq)) {
		/* some tx completed, increment avail */
		nic_i = txq->ift_cidx_processed;
		kring->nr_hwtail = nm_prev(netmap_idx_n2k(kring, nic_i), lim);
	}
	return (0);
}

/*
 * Reconcile kernel and user view of the receive ring.
 * Same as for the txsync, this routine must be efficient.
 * The caller guarantees a single invocations, but races against
 * the rest of the driver should be handled here.
 *
 * On call, kring->rhead is the first packet that userspace wants
 * to keep, and kring->rcur is the wakeup point.
 * The kernel has previously reported packets up to kring->rtail.
 *
 * If (flags & NAF_FORCE_READ) also check for incoming packets irrespective
 * of whether or not we received an interrupt.
 */
static int
iflib_netmap_rxsync(struct netmap_kring *kring, int flags)
{
	struct netmap_adapter *na = kring->na;
	struct ifnet *ifp = na->ifp;
	struct netmap_ring *ring = kring->ring;
	u_int nm_i;	/* index into the netmap ring */
	u_int nic_i;	/* index into the NIC ring */
	u_int i, n;
	u_int const lim = kring->nkr_num_slots - 1;
	u_int const head = kring->rhead;
	int force_update = (flags & NAF_FORCE_READ) || kring->nr_kflags & NKR_PENDINTR;
	struct if_rxd_info ri;
	/* device-specific */
	if_ctx_t ctx = ifp->if_softc;
	iflib_rxq_t rxq = &ctx->ifc_rxqs[kring->ring_id];
	iflib_fl_t fl = rxq->ifr_fl;
	if (head > lim)
		return netmap_ring_reinit(kring);

	bzero(&ri, sizeof(ri));
	ri.iri_qsidx = kring->ring_id;
	ri.iri_ifp = ctx->ifc_ifp;
	/* XXX check sync modes */
	for (i = 0, fl = rxq->ifr_fl; i < rxq->ifr_nfl; i++, fl++)
		bus_dmamap_sync(rxq->ifr_fl[i].ifl_desc_tag, fl->ifl_ifdi->idi_map,
				BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/*
	 * First part: import newly received packets.
	 *
	 * nm_i is the index of the next free slot in the netmap ring,
	 * nic_i is the index of the next received packet in the NIC ring,
	 * and they may differ in case if_init() has been called while
	 * in netmap mode. For the receive ring we have
	 *
	 *	nic_i = rxr->next_check;
	 *	nm_i = kring->nr_hwtail (previous)
	 * and
	 *	nm_i == (nic_i + kring->nkr_hwofs) % ring_size
	 *
	 * rxr->next_check is set to 0 on a ring reinit
	 */
	if (netmap_no_pendintr || force_update) {
		int crclen = iflib_crcstrip ? 0 : 4;
		int error, avail;
		uint16_t slot_flags = kring->nkr_slot_flags;

		for (fl = rxq->ifr_fl, i = 0; i < rxq->ifr_nfl; i++, fl++) {
			nic_i = fl->ifl_cidx;
			nm_i = netmap_idx_n2k(kring, nic_i);
			avail = ctx->isc_rxd_available(ctx->ifc_softc, kring->ring_id, nic_i);
			for (n = 0; avail > 0; n++, avail--) {
				error = ctx->isc_rxd_pkt_get(ctx->ifc_softc, &ri);
				if (error)
					ring->slot[nm_i].len = 0;
				else
					ring->slot[nm_i].len = ri.iri_len - crclen;
				ring->slot[nm_i].flags = slot_flags;
				bus_dmamap_sync(fl->ifl_ifdi->idi_tag,
								fl->ifl_sds[nic_i].ifsd_map, BUS_DMASYNC_POSTREAD);
				nm_i = nm_next(nm_i, lim);
				nic_i = nm_next(nic_i, lim);
			}
			if (n) { /* update the state variables */
				if (netmap_no_pendintr && !force_update) {
					/* diagnostics */
					iflib_rx_miss ++;
					iflib_rx_miss_bufs += n;
				}
				fl->ifl_cidx = nic_i;
				kring->nr_hwtail = nm_i;
			}
			kring->nr_kflags &= ~NKR_PENDINTR;
		}
	}
	/*
	 * Second part: skip past packets that userspace has released.
	 * (kring->nr_hwcur to head excluded),
	 * and make the buffers available for reception.
	 * As usual nm_i is the index in the netmap ring,
	 * nic_i is the index in the NIC ring, and
	 * nm_i == (nic_i + kring->nkr_hwofs) % ring_size
	 */
	/* XXX not sure how this will work with multiple free lists */
	nm_i = kring->nr_hwcur;
	if (nm_i != head) {
		nic_i = netmap_idx_k2n(kring, nm_i);
		for (n = 0; nm_i != head; n++) {
			struct netmap_slot *slot = &ring->slot[nm_i];
			uint64_t paddr;
			caddr_t vaddr;
			void *addr = PNMB(na, slot, &paddr);

			if (addr == NETMAP_BUF_BASE(na)) /* bad buf */
				goto ring_reset;

			vaddr = addr;
			if (slot->flags & NS_BUF_CHANGED) {
				/* buffer has changed, reload map */
				netmap_reload_map(na, fl->ifl_ifdi->idi_tag, fl->ifl_sds[nic_i].ifsd_map, addr);
				slot->flags &= ~NS_BUF_CHANGED;
			}
			/*
			 * XXX we should be batching this operation - TODO
			 */
			ctx->isc_rxd_refill(ctx->ifc_softc, rxq->ifr_id, fl->ifl_id, nic_i, &paddr, &vaddr, 1);
			bus_dmamap_sync(fl->ifl_ifdi->idi_tag, fl->ifl_sds[nic_i].ifsd_map,
			    BUS_DMASYNC_PREREAD);
			nm_i = nm_next(nm_i, lim);
			nic_i = nm_next(nic_i, lim);
		}
		kring->nr_hwcur = head;

		bus_dmamap_sync(fl->ifl_ifdi->idi_tag, fl->ifl_ifdi->idi_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		/*
		 * IMPORTANT: we must leave one free slot in the ring,
		 * so move nic_i back by one unit
		 */
		nic_i = nm_prev(nic_i, lim);
		ctx->isc_rxd_flush(ctx->ifc_softc, rxq->ifr_id, fl->ifl_id, nic_i);
	}

	return 0;

ring_reset:
	return netmap_ring_reinit(kring);
}

static int
iflib_netmap_attach(if_ctx_t ctx)
{
	struct netmap_adapter na;

	bzero(&na, sizeof(na));

	na.ifp = ctx->ifc_ifp;
	na.na_flags = NAF_BDG_MAYSLEEP;
	MPASS(ctx->ifc_softc_ctx.isc_ntxqsets);
	MPASS(ctx->ifc_softc_ctx.isc_nrxqsets);

	na.num_tx_desc = ctx->ifc_sctx->isc_ntxd;
	na.num_rx_desc = ctx->ifc_sctx->isc_ntxd;
	na.nm_txsync = iflib_netmap_txsync;
	na.nm_rxsync = iflib_netmap_rxsync;
	na.nm_register = iflib_netmap_register;
	na.num_tx_rings = ctx->ifc_softc_ctx.isc_ntxqsets;
	na.num_rx_rings = ctx->ifc_softc_ctx.isc_nrxqsets;
	return (netmap_attach(&na));
}

static void
iflib_netmap_txq_init(if_ctx_t ctx, iflib_txq_t txq)
{
	struct netmap_adapter *na = NA(ctx->ifc_ifp);
	struct netmap_slot *slot;

	slot = netmap_reset(na, NR_TX, txq->ift_id, 0);
	if (slot == 0)
		return;

	for (int i = 0; i < ctx->ifc_sctx->isc_ntxd; i++) {

		/*
		 * In netmap mode, set the map for the packet buffer.
		 * NOTE: Some drivers (not this one) also need to set
		 * the physical buffer address in the NIC ring.
		 * netmap_idx_n2k() maps a nic index, i, into the corresponding
		 * netmap slot index, si
		 */
		int si = netmap_idx_n2k(&na->tx_rings[txq->ift_id], i);
		netmap_load_map(na, txq->ift_desc_tag, txq->ift_sds.ifsd_map[i], NMB(na, slot + si));
	}
}
static void
iflib_netmap_rxq_init(if_ctx_t ctx, iflib_rxq_t rxq)
{
	struct netmap_adapter *na = NA(ctx->ifc_ifp);
	struct netmap_slot *slot;
	iflib_rxsd_t sd;
	int nrxd;

	slot = netmap_reset(na, NR_RX, rxq->ifr_id, 0);
	if (slot == 0)
		return;
	sd = rxq->ifr_fl[0].ifl_sds;
	nrxd = ctx->ifc_sctx->isc_nrxd;
	for (int i = 0; i < nrxd; i++, sd++) {
			int sj = netmap_idx_n2k(&na->rx_rings[rxq->ifr_id], i);
			uint64_t paddr;
			void *addr;
			caddr_t vaddr;

			vaddr = addr = PNMB(na, slot + sj, &paddr);
			netmap_load_map(na, rxq->ifr_fl[0].ifl_ifdi->idi_tag, sd->ifsd_map, addr);
			/* Update descriptor and the cached value */
			ctx->isc_rxd_refill(ctx->ifc_softc, rxq->ifr_id, 0 /* fl_id */, i, &paddr, &vaddr, 1);
	}
	/* preserve queue */
	if (ctx->ifc_ifp->if_capenable & IFCAP_NETMAP) {
		struct netmap_kring *kring = &na->rx_rings[rxq->ifr_id];
		int t = na->num_rx_desc - 1 - nm_kr_rxspace(kring);
		ctx->isc_rxd_flush(ctx->ifc_softc, rxq->ifr_id, 0 /* fl_id */, t);
	} else
		ctx->isc_rxd_flush(ctx->ifc_softc, rxq->ifr_id, 0 /* fl_id */, nrxd-1);
}

#define iflib_netmap_detach(ifp) netmap_detach(ifp)

#else
#define iflib_netmap_txq_init(ctx, txq)
#define iflib_netmap_rxq_init(ctx, rxq)
#define iflib_netmap_detach(ifp)

#define iflib_netmap_attach(ctx) (0)
#define netmap_rx_irq(ifp, qid, budget) (0)

#endif

#if defined(__i386__) || defined(__amd64__)
static __inline void
prefetch(void *x)
{
	__asm volatile("prefetcht0 %0" :: "m" (*(unsigned long *)x));
}
#else
#define prefetch(x)
#endif

static void
_iflib_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int err)
{
	if (err)
		return;
	*(bus_addr_t *) arg = segs[0].ds_addr;
}

int
iflib_dma_alloc(if_ctx_t ctx, int size, iflib_dma_info_t dma, int mapflags)
{
	int err;
	if_shared_ctx_t sctx = ctx->ifc_sctx;
	device_t dev = ctx->ifc_dev;

	KASSERT(sctx->isc_q_align != 0, ("alignment value not initialized"));

	err = bus_dma_tag_create(bus_get_dma_tag(dev), /* parent */
				sctx->isc_q_align, 0,	/* alignment, bounds */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				size,			/* maxsize */
				1,			/* nsegments */
				size,			/* maxsegsize */
				BUS_DMA_ALLOCNOW,	/* flags */
				NULL,			/* lockfunc */
				NULL,			/* lockarg */
				&dma->idi_tag);
	if (err) {
		device_printf(dev,
		    "%s: bus_dma_tag_create failed: %d\n",
		    __func__, err);
		goto fail_0;
	}

	err = bus_dmamem_alloc(dma->idi_tag, (void**) &dma->idi_vaddr,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT | BUS_DMA_ZERO, &dma->idi_map);
	if (err) {
		device_printf(dev,
		    "%s: bus_dmamem_alloc(%ju) failed: %d\n",
		    __func__, (uintmax_t)size, err);
		goto fail_1;
	}

	dma->idi_paddr = IF_BAD_DMA;
	err = bus_dmamap_load(dma->idi_tag, dma->idi_map, dma->idi_vaddr,
	    size, _iflib_dmamap_cb, &dma->idi_paddr, mapflags | BUS_DMA_NOWAIT);
	if (err || dma->idi_paddr == IF_BAD_DMA) {
		device_printf(dev,
		    "%s: bus_dmamap_load failed: %d\n",
		    __func__, err);
		goto fail_2;
	}

	dma->idi_size = size;
	return (0);

fail_2:
	bus_dmamem_free(dma->idi_tag, dma->idi_vaddr, dma->idi_map);
fail_1:
	bus_dma_tag_destroy(dma->idi_tag);
fail_0:
	dma->idi_tag = NULL;

	return (err);
}

int
iflib_dma_alloc_multi(if_ctx_t ctx, int *sizes, iflib_dma_info_t *dmalist, int mapflags, int count)
{
	int i, err;
	iflib_dma_info_t *dmaiter;

	dmaiter = dmalist;
	for (i = 0; i < count; i++, dmaiter++) {
		if ((err = iflib_dma_alloc(ctx, sizes[i], *dmaiter, mapflags)) != 0)
			break;
	}
	if (err)
		iflib_dma_free_multi(dmalist, i);
	return (err);
}

void
iflib_dma_free(iflib_dma_info_t dma)
{
	if (dma->idi_tag == NULL)
		return;
	if (dma->idi_paddr != IF_BAD_DMA) {
		bus_dmamap_sync(dma->idi_tag, dma->idi_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dma->idi_tag, dma->idi_map);
		dma->idi_paddr = IF_BAD_DMA;
	}
	if (dma->idi_vaddr != NULL) {
		bus_dmamem_free(dma->idi_tag, dma->idi_vaddr, dma->idi_map);
		dma->idi_vaddr = NULL;
	}
	bus_dma_tag_destroy(dma->idi_tag);
	dma->idi_tag = NULL;
}

void
iflib_dma_free_multi(iflib_dma_info_t *dmalist, int count)
{
	int i;
	iflib_dma_info_t *dmaiter = dmalist;

	for (i = 0; i < count; i++, dmaiter++)
		iflib_dma_free(*dmaiter);
}

static int
iflib_fast_intr(void *arg)
{
	iflib_filter_info_t info = arg;
	struct grouptask *gtask = info->ifi_task;

	DBG_COUNTER_INC(fast_intrs);
	if (info->ifi_filter != NULL && info->ifi_filter(info->ifi_filter_arg) == FILTER_HANDLED)
		return (FILTER_HANDLED);

	GROUPTASK_ENQUEUE(gtask);
	return (FILTER_HANDLED);
}

static int
_iflib_irq_alloc(if_ctx_t ctx, if_irq_t irq, int rid,
	driver_filter_t filter, driver_intr_t handler, void *arg,
				 char *name)
{
	int rc;
	struct resource *res;
	void *tag;
	device_t dev = ctx->ifc_dev;

	MPASS(rid < 512);
	irq->ii_rid = rid;
	res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &irq->ii_rid,
				     RF_SHAREABLE | RF_ACTIVE);
	if (res == NULL) {
		device_printf(dev,
		    "failed to allocate IRQ for rid %d, name %s.\n", rid, name);
		return (ENOMEM);
	}
	irq->ii_res = res;
	KASSERT(filter == NULL || handler == NULL, ("filter and handler can't both be non-NULL"));
	rc = bus_setup_intr(dev, res, INTR_MPSAFE | INTR_TYPE_NET,
						filter, handler, arg, &tag);
	if (rc != 0) {
		device_printf(dev,
		    "failed to setup interrupt for rid %d, name %s: %d\n",
					  rid, name ? name : "unknown", rc);
		return (rc);
	} else if (name)
		bus_describe_intr(dev, res, tag, name);

	irq->ii_tag = tag;
	return (0);
}


/*********************************************************************
 *
 *  Allocate memory for tx_buffer structures. The tx_buffer stores all
 *  the information needed to transmit a packet on the wire. This is
 *  called only once at attach, setup is done every reset.
 *
 **********************************************************************/

static int
iflib_txsd_alloc(iflib_txq_t txq)
{
	if_ctx_t ctx = txq->ift_ctx;
	if_shared_ctx_t sctx = ctx->ifc_sctx;
	if_softc_ctx_t scctx = &ctx->ifc_softc_ctx;
	device_t dev = ctx->ifc_dev;
	int err, nsegments, ntsosegments;

	nsegments = scctx->isc_tx_nsegments;
	ntsosegments = scctx->isc_tx_tso_segments_max;
	MPASS(sctx->isc_ntxd > 0);
	MPASS(nsegments > 0);
	MPASS(ntsosegments > 0);
	/*
	 * Setup DMA descriptor areas.
	 */
	if ((err = bus_dma_tag_create(bus_get_dma_tag(dev),
			       1, 0,			/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,		/* filter, filterarg */
			       sctx->isc_tx_maxsize,		/* maxsize */
			       nsegments,	/* nsegments */
			       sctx->isc_tx_maxsegsize,	/* maxsegsize */
			       0,			/* flags */
			       NULL,			/* lockfunc */
			       NULL,			/* lockfuncarg */
			       &txq->ift_desc_tag))) {
		device_printf(dev,"Unable to allocate TX DMA tag: %d\n", err);
		device_printf(dev,"maxsize: %zd nsegments: %d maxsegsize: %zd\n",
					  sctx->isc_tx_maxsize, nsegments, sctx->isc_tx_maxsegsize);
		goto fail;
	}
#ifdef INVARIANTS
	device_printf(dev,"maxsize: %zd nsegments: %d maxsegsize: %zd\n",
		      sctx->isc_tx_maxsize, nsegments, sctx->isc_tx_maxsegsize);
#endif
	device_printf(dev,"TSO maxsize: %d ntsosegments: %d maxsegsize: %d\n",
		      scctx->isc_tx_tso_size_max, ntsosegments,
		      scctx->isc_tx_tso_segsize_max);
	if ((err = bus_dma_tag_create(bus_get_dma_tag(dev),
			       1, 0,			/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,		/* filter, filterarg */
			       scctx->isc_tx_tso_size_max,		/* maxsize */
			       ntsosegments,	/* nsegments */
			       scctx->isc_tx_tso_segsize_max,	/* maxsegsize */
			       0,			/* flags */
			       NULL,			/* lockfunc */
			       NULL,			/* lockfuncarg */
			       &txq->ift_tso_desc_tag))) {
		device_printf(dev,"Unable to allocate TX TSO DMA tag: %d\n", err);

		goto fail;
	}
#ifdef INVARIANTS
	device_printf(dev,"TSO maxsize: %d ntsosegments: %d maxsegsize: %d\n",
		      scctx->isc_tx_tso_size_max, ntsosegments,
		      scctx->isc_tx_tso_segsize_max);
#endif
	if (!(txq->ift_sds.ifsd_flags =
	    (uint8_t *) malloc(sizeof(uint8_t) *
	    sctx->isc_ntxd, M_IFLIB, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate tx_buffer memory\n");
		err = ENOMEM;
		goto fail;
	}
	if (!(txq->ift_sds.ifsd_m =
	    (struct mbuf **) malloc(sizeof(struct mbuf *) *
	    sctx->isc_ntxd, M_IFLIB, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate tx_buffer memory\n");
		err = ENOMEM;
		goto fail;
	}

        /* Create the descriptor buffer dma maps */
#if defined(ACPI_DMAR) || (!(defined(__i386__) && !defined(__amd64__)))
	if ((ctx->ifc_flags & IFC_DMAR) == 0)
		return (0);

	if (!(txq->ift_sds.ifsd_map =
	    (bus_dmamap_t *) malloc(sizeof(bus_dmamap_t) * sctx->isc_ntxd, M_IFLIB, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate tx_buffer map memory\n");
		err = ENOMEM;
		goto fail;
	}

	for (int i = 0; i < sctx->isc_ntxd; i++) {
		err = bus_dmamap_create(txq->ift_desc_tag, 0, &txq->ift_sds.ifsd_map[i]);
		if (err != 0) {
			device_printf(dev, "Unable to create TX DMA map\n");
			goto fail;
		}
	}
#endif
	return (0);
fail:
	/* We free all, it handles case where we are in the middle */
	iflib_tx_structures_free(ctx);
	return (err);
}

static void
iflib_txsd_destroy(if_ctx_t ctx, iflib_txq_t txq, int i)
{
	bus_dmamap_t map;

	map = NULL;
	if (txq->ift_sds.ifsd_map != NULL)
		map = txq->ift_sds.ifsd_map[i];
	if (map != NULL) {
		bus_dmamap_unload(txq->ift_desc_tag, map);
		bus_dmamap_destroy(txq->ift_desc_tag, map);
		txq->ift_sds.ifsd_map[i] = NULL;
	}
}

static void
iflib_txq_destroy(iflib_txq_t txq)
{
	if_ctx_t ctx = txq->ift_ctx;
	if_shared_ctx_t sctx = ctx->ifc_sctx;

	for (int i = 0; i < sctx->isc_ntxd; i++)
		iflib_txsd_destroy(ctx, txq, i);
	if (txq->ift_sds.ifsd_map != NULL) {
		free(txq->ift_sds.ifsd_map, M_IFLIB);
		txq->ift_sds.ifsd_map = NULL;
	}
	if (txq->ift_sds.ifsd_m != NULL) {
		free(txq->ift_sds.ifsd_m, M_IFLIB);
		txq->ift_sds.ifsd_m = NULL;
	}
	if (txq->ift_sds.ifsd_flags != NULL) {
		free(txq->ift_sds.ifsd_flags, M_IFLIB);
		txq->ift_sds.ifsd_flags = NULL;
	}
	if (txq->ift_desc_tag != NULL) {
		bus_dma_tag_destroy(txq->ift_desc_tag);
		txq->ift_desc_tag = NULL;
	}
	if (txq->ift_tso_desc_tag != NULL) {
		bus_dma_tag_destroy(txq->ift_tso_desc_tag);
		txq->ift_tso_desc_tag = NULL;
	}
}

static void
iflib_txsd_free(if_ctx_t ctx, iflib_txq_t txq, int i)
{
	struct mbuf **mp;

	mp = &txq->ift_sds.ifsd_m[i];
	if (*mp == NULL)
		return;

	if (txq->ift_sds.ifsd_map != NULL) {
		bus_dmamap_sync(txq->ift_desc_tag,
				txq->ift_sds.ifsd_map[i],
				BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(txq->ift_desc_tag,
				  txq->ift_sds.ifsd_map[i]);
	}
	m_freem(*mp);
	DBG_COUNTER_INC(tx_frees);
	*mp = NULL;
}

static int
iflib_txq_setup(iflib_txq_t txq)
{
	if_ctx_t ctx = txq->ift_ctx;
	if_shared_ctx_t sctx = ctx->ifc_sctx;
	iflib_dma_info_t di;
	int i;

    /* Set number of descriptors available */
	txq->ift_qstatus = IFLIB_QUEUE_IDLE;

	/* Reset indices */
	txq->ift_cidx_processed = txq->ift_pidx = txq->ift_cidx = txq->ift_npending = 0;
	txq->ift_size = sctx->isc_ntxd;

	for (i = 0, di = txq->ift_ifdi; i < ctx->ifc_nhwtxqs; i++, di++)
		bzero((void *)di->idi_vaddr, di->idi_size);

	IFDI_TXQ_SETUP(ctx, txq->ift_id);
	for (i = 0, di = txq->ift_ifdi; i < ctx->ifc_nhwtxqs; i++, di++)
		bus_dmamap_sync(di->idi_tag, di->idi_map,
						BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	return (0);
}

/*********************************************************************
 *
 *  Allocate memory for rx_buffer structures. Since we use one
 *  rx_buffer per received packet, the maximum number of rx_buffer's
 *  that we'll need is equal to the number of receive descriptors
 *  that we've allocated.
 *
 **********************************************************************/
static int
iflib_rxsd_alloc(iflib_rxq_t rxq)
{
	if_ctx_t ctx = rxq->ifr_ctx;
	if_shared_ctx_t sctx = ctx->ifc_sctx;
	device_t dev = ctx->ifc_dev;
	iflib_fl_t fl;
	iflib_rxsd_t	rxsd;
	int			err;

	MPASS(sctx->isc_nrxd > 0);

	fl = rxq->ifr_fl;
	for (int i = 0; i <  rxq->ifr_nfl; i++, fl++) {
		fl->ifl_sds = malloc(sizeof(struct iflib_sw_rx_desc) *
							 sctx->isc_nrxd, M_IFLIB, M_WAITOK | M_ZERO);
		if (fl->ifl_sds == NULL) {
			device_printf(dev, "Unable to allocate rx sw desc memory\n");
			return (ENOMEM);
		}
		fl->ifl_size = sctx->isc_nrxd; /* this isn't necessarily the same */
		err = bus_dma_tag_create(bus_get_dma_tag(dev), /* parent */
					 1, 0,			/* alignment, bounds */
					 BUS_SPACE_MAXADDR,	/* lowaddr */
					 BUS_SPACE_MAXADDR,	/* highaddr */
					 NULL, NULL,		/* filter, filterarg */
					 sctx->isc_rx_maxsize,	/* maxsize */
					 sctx->isc_rx_nsegments,	/* nsegments */
					 sctx->isc_rx_maxsegsize,	/* maxsegsize */
					 0,			/* flags */
					 NULL,			/* lockfunc */
					 NULL,			/* lockarg */
					 &fl->ifl_desc_tag);
		if (err) {
			device_printf(dev, "%s: bus_dma_tag_create failed %d\n",
				__func__, err);
			goto fail;
		}

		rxsd = fl->ifl_sds;
		for (int i = 0; i < sctx->isc_nrxd; i++, rxsd++) {
			err = bus_dmamap_create(fl->ifl_desc_tag, 0, &rxsd->ifsd_map);
			if (err) {
				device_printf(dev, "%s: bus_dmamap_create failed: %d\n",
					__func__, err);
				goto fail;
			}
		}
	}
	return (0);

fail:
	iflib_rx_structures_free(ctx);
	return (err);
}


/*
 * Internal service routines
 */

struct rxq_refill_cb_arg {
	int               error;
	bus_dma_segment_t seg;
	int               nseg;
};

static void
_rxq_refill_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct rxq_refill_cb_arg *cb_arg = arg;

	cb_arg->error = error;
	cb_arg->seg = segs[0];
	cb_arg->nseg = nseg;
}


#ifdef ACPI_DMAR
#define IS_DMAR(ctx) (ctx->ifc_flags & IFC_DMAR)
#else
#define IS_DMAR(ctx) (0)
#endif

/**
 *	rxq_refill - refill an rxq  free-buffer list
 *	@ctx: the iflib context
 *	@rxq: the free-list to refill
 *	@n: the number of new buffers to allocate
 *
 *	(Re)populate an rxq free-buffer list with up to @n new packet buffers.
 *	The caller must assure that @n does not exceed the queue's capacity.
 */
static void
_iflib_fl_refill(if_ctx_t ctx, iflib_fl_t fl, int count)
{
	struct mbuf *m;
	int pidx = fl->ifl_pidx;
	iflib_rxsd_t rxsd = &fl->ifl_sds[pidx];
	caddr_t cl;
	int n, i = 0;
	uint64_t bus_addr;
	int err;

	n  = count;
	MPASS(n > 0);
	MPASS(fl->ifl_credits + n <= fl->ifl_size);

	if (pidx < fl->ifl_cidx)
		MPASS(pidx + n <= fl->ifl_cidx);
	if (pidx == fl->ifl_cidx && (fl->ifl_credits < fl->ifl_size))
		MPASS(fl->ifl_gen == 0);
	if (pidx > fl->ifl_cidx)
		MPASS(n <= fl->ifl_size - pidx + fl->ifl_cidx);

	DBG_COUNTER_INC(fl_refills);
	if (n > 8)
		DBG_COUNTER_INC(fl_refills_large);

	while (n--) {
		/*
		 * We allocate an uninitialized mbuf + cluster, mbuf is
		 * initialized after rx.
		 *
		 * If the cluster is still set then we know a minimum sized packet was received
		 */
		if ((cl = rxsd->ifsd_cl) == NULL) {
			if ((cl = rxsd->ifsd_cl = m_cljget(NULL, M_NOWAIT, fl->ifl_buf_size)) == NULL)
				break;
#if MEMORY_LOGGING
			fl->ifl_cl_enqueued++;
#endif
		}
		if ((m = m_gethdr(M_NOWAIT, MT_NOINIT)) == NULL) {
			break;
		}
#if MEMORY_LOGGING
		fl->ifl_m_enqueued++;
#endif

		DBG_COUNTER_INC(rx_allocs);
#ifdef notyet
		if ((rxsd->ifsd_flags & RX_SW_DESC_MAP_CREATED) == 0) {
			int err;

			if ((err = bus_dmamap_create(fl->ifl_ifdi->idi_tag, 0, &rxsd->ifsd_map))) {
				log(LOG_WARNING, "bus_dmamap_create failed %d\n", err);
				uma_zfree(fl->ifl_zone, cl);
				n = 0;
				goto done;
			}
			rxsd->ifsd_flags |= RX_SW_DESC_MAP_CREATED;
		}
#endif
#if defined(__i386__) || defined(__amd64__)
		if (!IS_DMAR(ctx)) {
			bus_addr = pmap_kextract((vm_offset_t)cl);
		} else
#endif
		{
			struct rxq_refill_cb_arg cb_arg;
			iflib_rxq_t q;

			cb_arg.error = 0;
			q = fl->ifl_rxq;
			err = bus_dmamap_load(fl->ifl_desc_tag, rxsd->ifsd_map,
		         cl, fl->ifl_buf_size, _rxq_refill_cb, &cb_arg, 0);

			if (err != 0 || cb_arg.error) {
				/*
				 * !zone_pack ?
				 */
				if (fl->ifl_zone == zone_pack)
					uma_zfree(fl->ifl_zone, cl);
				m_free(m);
				n = 0;
				goto done;
			}
			bus_addr = cb_arg.seg.ds_addr;
		}
		rxsd->ifsd_flags |= RX_SW_DESC_INUSE;

		MPASS(rxsd->ifsd_m == NULL);
		rxsd->ifsd_cl = cl;
		rxsd->ifsd_m = m;
		fl->ifl_bus_addrs[i] = bus_addr;
		fl->ifl_vm_addrs[i] = cl;
		rxsd++;
		fl->ifl_credits++;
		i++;
		MPASS(fl->ifl_credits <= fl->ifl_size);
		if (++fl->ifl_pidx == fl->ifl_size) {
			fl->ifl_pidx = 0;
			fl->ifl_gen = 1;
			rxsd = fl->ifl_sds;
		}
		if (n == 0 || i == IFLIB_MAX_RX_REFRESH) {
			ctx->isc_rxd_refill(ctx->ifc_softc, fl->ifl_rxq->ifr_id, fl->ifl_id, pidx,
								 fl->ifl_bus_addrs, fl->ifl_vm_addrs, i);
			i = 0;
			pidx = fl->ifl_pidx;
		}
	}
done:
	DBG_COUNTER_INC(rxd_flush);
	if (fl->ifl_pidx == 0)
		pidx = fl->ifl_size - 1;
	else
		pidx = fl->ifl_pidx - 1;
	ctx->isc_rxd_flush(ctx->ifc_softc, fl->ifl_rxq->ifr_id, fl->ifl_id, pidx);
}

static __inline void
__iflib_fl_refill_lt(if_ctx_t ctx, iflib_fl_t fl, int max)
{
	/* we avoid allowing pidx to catch up with cidx as it confuses ixl */
	int32_t reclaimable = fl->ifl_size - fl->ifl_credits - 1;
#ifdef INVARIANTS
	int32_t delta = fl->ifl_size - get_inuse(fl->ifl_size, fl->ifl_cidx, fl->ifl_pidx, fl->ifl_gen) - 1;
#endif

	MPASS(fl->ifl_credits <= fl->ifl_size);
	MPASS(reclaimable == delta);

	if (reclaimable > 0)
		_iflib_fl_refill(ctx, fl, min(max, reclaimable));
}

static void
iflib_fl_bufs_free(iflib_fl_t fl)
{
	iflib_dma_info_t idi = fl->ifl_ifdi;
	uint32_t i;

	for (i = 0; i < fl->ifl_size; i++) {
		iflib_rxsd_t d = &fl->ifl_sds[i];

		if (d->ifsd_flags & RX_SW_DESC_INUSE) {
			bus_dmamap_unload(fl->ifl_desc_tag, d->ifsd_map);
			bus_dmamap_destroy(fl->ifl_desc_tag, d->ifsd_map);
			if (d->ifsd_m != NULL) {
				m_init(d->ifsd_m, M_NOWAIT, MT_DATA, 0);
				uma_zfree(zone_mbuf, d->ifsd_m);
			}
			if (d->ifsd_cl != NULL)
				uma_zfree(fl->ifl_zone, d->ifsd_cl);
			d->ifsd_flags = 0;
		} else {
			MPASS(d->ifsd_cl == NULL);
			MPASS(d->ifsd_m == NULL);
		}
#if MEMORY_LOGGING
		fl->ifl_m_dequeued++;
		fl->ifl_cl_dequeued++;
#endif
		d->ifsd_cl = NULL;
		d->ifsd_m = NULL;
	}
	/*
	 * Reset free list values
	 */
	fl->ifl_credits = fl->ifl_cidx = fl->ifl_pidx = fl->ifl_gen = 0;;
	bzero(idi->idi_vaddr, idi->idi_size);
}

/*********************************************************************
 *
 *  Initialize a receive ring and its buffers.
 *
 **********************************************************************/
static int
iflib_fl_setup(iflib_fl_t fl)
{
	iflib_rxq_t rxq = fl->ifl_rxq;
	if_ctx_t ctx = rxq->ifr_ctx;
	if_softc_ctx_t sctx = &ctx->ifc_softc_ctx;

	/*
	** Free current RX buffer structs and their mbufs
	*/
	iflib_fl_bufs_free(fl);
	/* Now replenish the mbufs */
	MPASS(fl->ifl_credits == 0);
	/*
	 * XXX don't set the max_frame_size to larger
	 * than the hardware can handle
	 */
	if (sctx->isc_max_frame_size <= 2048)
		fl->ifl_buf_size = MCLBYTES;
	else if (sctx->isc_max_frame_size <= 4096)
		fl->ifl_buf_size = MJUMPAGESIZE;
	else if (sctx->isc_max_frame_size <= 9216)
		fl->ifl_buf_size = MJUM9BYTES;
	else
		fl->ifl_buf_size = MJUM16BYTES;
	if (fl->ifl_buf_size > ctx->ifc_max_fl_buf_size)
		ctx->ifc_max_fl_buf_size = fl->ifl_buf_size;
	fl->ifl_cltype = m_gettype(fl->ifl_buf_size);
	fl->ifl_zone = m_getzone(fl->ifl_buf_size);


	/* avoid pre-allocating zillions of clusters to an idle card
	 * potentially speeding up attach
	 */
	_iflib_fl_refill(ctx, fl, min(128, fl->ifl_size));
	MPASS(min(128, fl->ifl_size) == fl->ifl_credits);
	if (min(128, fl->ifl_size) != fl->ifl_credits)
		return (ENOBUFS);
	/*
	 * handle failure
	 */
	MPASS(rxq != NULL);
	MPASS(fl->ifl_ifdi != NULL);
	bus_dmamap_sync(fl->ifl_ifdi->idi_tag, fl->ifl_ifdi->idi_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	return (0);
}

/*********************************************************************
 *
 *  Free receive ring data structures
 *
 **********************************************************************/
static void
iflib_rx_sds_free(iflib_rxq_t rxq)
{
	iflib_fl_t fl;
	int i;

	if (rxq->ifr_fl != NULL) {
		for (i = 0; i < rxq->ifr_nfl; i++) {
			fl = &rxq->ifr_fl[i];
			if (fl->ifl_desc_tag != NULL) {
				bus_dma_tag_destroy(fl->ifl_desc_tag);
				fl->ifl_desc_tag = NULL;
			}
		}
		if (rxq->ifr_fl->ifl_sds != NULL)
			free(rxq->ifr_fl->ifl_sds, M_IFLIB);

		free(rxq->ifr_fl, M_IFLIB);
		rxq->ifr_fl = NULL;
		rxq->ifr_cq_gen = rxq->ifr_cq_cidx = rxq->ifr_cq_pidx = 0;
	}
}

/*
 * MI independent logic
 *
 */
static void
iflib_timer(void *arg)
{
	iflib_txq_t txq = arg;
	if_ctx_t ctx = txq->ift_ctx;
	if_softc_ctx_t scctx = &ctx->ifc_softc_ctx;

	if (!(if_getdrvflags(ctx->ifc_ifp) & IFF_DRV_RUNNING))
		return;
	/*
	** Check on the state of the TX queue(s), this
	** can be done without the lock because its RO
	** and the HUNG state will be static if set.
	*/
	IFDI_TIMER(ctx, txq->ift_id);
	if ((txq->ift_qstatus == IFLIB_QUEUE_HUNG) &&
		(ctx->ifc_pause_frames == 0))
		goto hung;

	if (TXQ_AVAIL(txq) <= 2*scctx->isc_tx_nsegments ||
	    ifmp_ring_is_stalled(txq->ift_br[0]))
		GROUPTASK_ENQUEUE(&txq->ift_task);

	ctx->ifc_pause_frames = 0;
	if (if_getdrvflags(ctx->ifc_ifp) & IFF_DRV_RUNNING) 
		callout_reset_on(&txq->ift_timer, hz/2, iflib_timer, txq, txq->ift_timer.c_cpu);
	return;
hung:
	CTX_LOCK(ctx);
	if_setdrvflagbits(ctx->ifc_ifp, 0, IFF_DRV_RUNNING);
	device_printf(ctx->ifc_dev,  "TX(%d) desc avail = %d, pidx = %d\n",
				  txq->ift_id, TXQ_AVAIL(txq), txq->ift_pidx);

	IFDI_WATCHDOG_RESET(ctx);
	ctx->ifc_watchdog_events++;
	ctx->ifc_pause_frames = 0;

	iflib_init_locked(ctx);
	CTX_UNLOCK(ctx);
}

static void
iflib_init_locked(if_ctx_t ctx)
{
	if_softc_ctx_t sctx = &ctx->ifc_softc_ctx;
	if_t ifp = ctx->ifc_ifp;
	iflib_fl_t fl;
	iflib_txq_t txq;
	iflib_rxq_t rxq;
	int i, j;


	if_setdrvflagbits(ifp, IFF_DRV_OACTIVE, IFF_DRV_RUNNING);
	IFDI_INTR_DISABLE(ctx);

	/* Set hardware offload abilities */
	if_clearhwassist(ifp);
	if (if_getcapenable(ifp) & IFCAP_TXCSUM)
		if_sethwassistbits(ifp, CSUM_IP | CSUM_TCP | CSUM_UDP, 0);
	if (if_getcapenable(ifp) & IFCAP_TXCSUM_IPV6)
		if_sethwassistbits(ifp,  (CSUM_TCP_IPV6 | CSUM_UDP_IPV6), 0);
	if (if_getcapenable(ifp) & IFCAP_TSO4)
		if_sethwassistbits(ifp, CSUM_IP_TSO, 0);
	if (if_getcapenable(ifp) & IFCAP_TSO6)
		if_sethwassistbits(ifp, CSUM_IP6_TSO, 0);

	for (i = 0, txq = ctx->ifc_txqs; i < sctx->isc_ntxqsets; i++, txq++) {
		CALLOUT_LOCK(txq);
		callout_stop(&txq->ift_timer);
		callout_stop(&txq->ift_db_check);
		CALLOUT_UNLOCK(txq);
		iflib_netmap_txq_init(ctx, txq);
	}
	for (i = 0, rxq = ctx->ifc_rxqs; i < sctx->isc_nrxqsets; i++, rxq++) {
		iflib_netmap_rxq_init(ctx, rxq);
	}
	IFDI_INIT(ctx);
	for (i = 0, rxq = ctx->ifc_rxqs; i < sctx->isc_nrxqsets; i++, rxq++) {
		for (j = 0, fl = rxq->ifr_fl; j < rxq->ifr_nfl; j++, fl++) {
			if (iflib_fl_setup(fl)) {
				device_printf(ctx->ifc_dev, "freelist setup failed - check cluster settings\n");
				goto done;
			}
		}
	}
	done:
	if_setdrvflagbits(ctx->ifc_ifp, IFF_DRV_RUNNING, IFF_DRV_OACTIVE);
	IFDI_INTR_ENABLE(ctx);
	txq = ctx->ifc_txqs;
	for (i = 0; i < sctx->isc_ntxqsets; i++, txq++)
		callout_reset_on(&txq->ift_timer, hz/2, iflib_timer, txq,
			txq->ift_timer.c_cpu);
}

static int
iflib_media_change(if_t ifp)
{
	if_ctx_t ctx = if_getsoftc(ifp);
	int err;

	CTX_LOCK(ctx);
	if ((err = IFDI_MEDIA_CHANGE(ctx)) == 0)
		iflib_init_locked(ctx);
	CTX_UNLOCK(ctx);
	return (err);
}

static void
iflib_media_status(if_t ifp, struct ifmediareq *ifmr)
{
	if_ctx_t ctx = if_getsoftc(ifp);

	CTX_LOCK(ctx);
	IFDI_UPDATE_ADMIN_STATUS(ctx);
	IFDI_MEDIA_STATUS(ctx, ifmr);
	CTX_UNLOCK(ctx);
}

static void
iflib_stop(if_ctx_t ctx)
{
	iflib_txq_t txq = ctx->ifc_txqs;
	iflib_rxq_t rxq = ctx->ifc_rxqs;
	if_softc_ctx_t scctx = &ctx->ifc_softc_ctx;
	if_shared_ctx_t sctx = ctx->ifc_sctx;
	iflib_dma_info_t di;
	iflib_fl_t fl;
	int i, j;

	/* Tell the stack that the interface is no longer active */
	if_setdrvflagbits(ctx->ifc_ifp, IFF_DRV_OACTIVE, IFF_DRV_RUNNING);

	IFDI_INTR_DISABLE(ctx);
	msleep(ctx, &ctx->ifc_mtx, PUSER, "iflib_init", hz);

	/* Wait for current tx queue users to exit to disarm watchdog timer. */
	for (i = 0; i < scctx->isc_ntxqsets; i++, txq++) {
		/* make sure all transmitters have completed before proceeding XXX */

		/* clean any enqueued buffers */
		iflib_txq_check_drain(txq, 0);
		/* Free any existing tx buffers. */
		for (j = 0; j < sctx->isc_ntxd; j++) {
			iflib_txsd_free(ctx, txq, j);
		}
		txq->ift_processed = txq->ift_cleaned = txq->ift_cidx_processed = 0;
		txq->ift_in_use = txq->ift_cidx = txq->ift_pidx = txq->ift_no_desc_avail = 0;
		txq->ift_closed = txq->ift_mbuf_defrag = txq->ift_mbuf_defrag_failed = 0;
		txq->ift_no_tx_dma_setup = txq->ift_txd_encap_efbig = txq->ift_map_failed = 0;
		txq->ift_pullups = 0;
		ifmp_ring_reset_stats(txq->ift_br[0]);
		for (j = 0, di = txq->ift_ifdi; j < ctx->ifc_nhwtxqs; j++, di++)
			bzero((void *)di->idi_vaddr, di->idi_size);
	}
	for (i = 0; i < scctx->isc_nrxqsets; i++, rxq++) {
		/* make sure all transmitters have completed before proceeding XXX */

		for (j = 0, di = txq->ift_ifdi; j < ctx->ifc_nhwrxqs; j++, di++)
			bzero((void *)di->idi_vaddr, di->idi_size);
		/* also resets the free lists pidx/cidx */
		for (j = 0, fl = rxq->ifr_fl; j < rxq->ifr_nfl; j++, fl++)
			iflib_fl_bufs_free(fl);
	}
	IFDI_STOP(ctx);
}

static iflib_rxsd_t
rxd_frag_to_sd(iflib_rxq_t rxq, if_rxd_frag_t irf, int *cltype, int unload)
{
	int flid, cidx;
	iflib_rxsd_t sd;
	iflib_fl_t fl;
	iflib_dma_info_t di;

	flid = irf->irf_flid;
	cidx = irf->irf_idx;
	fl = &rxq->ifr_fl[flid];
	fl->ifl_credits--;
#if MEMORY_LOGGING
	fl->ifl_m_dequeued++;
	if (cltype)
		fl->ifl_cl_dequeued++;
#endif
	sd = &fl->ifl_sds[cidx];
	di = fl->ifl_ifdi;
	bus_dmamap_sync(di->idi_tag, di->idi_map,
			BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/* not valid assert if bxe really does SGE from non-contiguous elements */
	MPASS(fl->ifl_cidx == cidx);
	if (unload)
		bus_dmamap_unload(fl->ifl_desc_tag, sd->ifsd_map);

	if (__predict_false(++fl->ifl_cidx == fl->ifl_size)) {
		fl->ifl_cidx = 0;
		fl->ifl_gen = 0;
	}
	/* YES ick */
	if (cltype)
		*cltype = fl->ifl_cltype;
	return (sd);
}

static struct mbuf *
assemble_segments(iflib_rxq_t rxq, if_rxd_info_t ri)
{
	int i, padlen , flags, cltype;
	struct mbuf *m, *mh, *mt;
	iflib_rxsd_t sd;
	caddr_t cl;

	i = 0;
	do {
		sd = rxd_frag_to_sd(rxq, &ri->iri_frags[i], &cltype, TRUE);

		MPASS(sd->ifsd_cl != NULL);
		MPASS(sd->ifsd_m != NULL);
		m = sd->ifsd_m;
		if (i == 0) {
			flags = M_PKTHDR|M_EXT;
			mh = mt = m;
			padlen = ri->iri_pad;
		} else {
			flags = M_EXT;
			mt->m_next = m;
			mt = m;
			/* assuming padding is only on the first fragment */
			padlen = 0;
		}
		sd->ifsd_m = NULL;
		cl = sd->ifsd_cl;
		sd->ifsd_cl = NULL;

		/* Can these two be made one ? */
		m_init(m, M_NOWAIT, MT_DATA, flags);
		m_cljset(m, cl, cltype);
		/*
		 * These must follow m_init and m_cljset
		 */
		m->m_data += padlen;
		ri->iri_len -= padlen;
		m->m_len = ri->iri_len;
	} while (++i < ri->iri_nfrags);

	return (mh);
}



/*
 * Process one software descriptor
 */
static struct mbuf *
iflib_rxd_pkt_get(iflib_rxq_t rxq, if_rxd_info_t ri)
{
	struct mbuf *m;
	iflib_rxsd_t sd;

	/* should I merge this back in now that the two paths are basically duplicated? */
	if (ri->iri_len <= IFLIB_RX_COPY_THRESH) {
		sd = rxd_frag_to_sd(rxq, &ri->iri_frags[0], NULL, FALSE);
		m = sd->ifsd_m;
		sd->ifsd_m = NULL;
		m_init(m, M_NOWAIT, MT_DATA, M_PKTHDR);
		memcpy(m->m_data, sd->ifsd_cl, ri->iri_len);
		m->m_len = ri->iri_len;
       } else {
		m = assemble_segments(rxq, ri);
	}
	m->m_pkthdr.len = ri->iri_len;
	m->m_pkthdr.rcvif = ri->iri_ifp;
	m->m_flags |= ri->iri_flags;
	m->m_pkthdr.ether_vtag = ri->iri_vtag;
	m->m_pkthdr.flowid = ri->iri_flowid;
	M_HASHTYPE_SET(m, ri->iri_rsstype);
	m->m_pkthdr.csum_flags = ri->iri_csum_flags;
	m->m_pkthdr.csum_data = ri->iri_csum_data;
	return (m);
}

static bool
iflib_rxeof(iflib_rxq_t rxq, int budget)
{
	if_ctx_t ctx = rxq->ifr_ctx;
	if_shared_ctx_t sctx = ctx->ifc_sctx;
	int avail, i;
	uint16_t *cidxp;
	struct if_rxd_info ri;
	int err, budget_left, rx_bytes, rx_pkts;
	iflib_fl_t fl;
	struct ifnet *ifp;
	struct lro_entry *queued;
	int lro_enabled;
	/*
	 * XXX early demux data packets so that if_input processing only handles
	 * acks in interrupt context
	 */
	struct mbuf *m, *mh, *mt;

	if (netmap_rx_irq(ctx->ifc_ifp, rxq->ifr_id, &budget)) {
		return (FALSE);
	}

	mh = mt = NULL;
	MPASS(budget > 0);
	rx_pkts	= rx_bytes = 0;
	if (sctx->isc_flags & IFLIB_HAS_CQ)
		cidxp = &rxq->ifr_cq_cidx;
	else
		cidxp = &rxq->ifr_fl[0].ifl_cidx;
	if ((avail = iflib_rxd_avail(ctx, rxq, *cidxp)) == 0) {
		for (i = 0, fl = &rxq->ifr_fl[0]; i < sctx->isc_nfl; i++, fl++)
			__iflib_fl_refill_lt(ctx, fl, budget + 8);
		DBG_COUNTER_INC(rx_unavail);
		return (false);
	}

	for (budget_left = budget; (budget_left > 0) && (avail > 0); budget_left--, avail--) {
		if (__predict_false(!CTX_ACTIVE(ctx))) {
			DBG_COUNTER_INC(rx_ctx_inactive);
			break;
		}
		/*
		 * Reset client set fields to their default values
		 */
		bzero(&ri, sizeof(ri));
		ri.iri_qsidx = rxq->ifr_id;
		ri.iri_cidx = *cidxp;
		ri.iri_ifp = ctx->ifc_ifp;
		ri.iri_frags = rxq->ifr_frags;
		err = ctx->isc_rxd_pkt_get(ctx->ifc_softc, &ri);

		/* in lieu of handling correctly - make sure it isn't being unhandled */
		MPASS(err == 0);
		if (sctx->isc_flags & IFLIB_HAS_CQ) {
			/* we know we consumed _one_ CQ entry */
			if (++rxq->ifr_cq_cidx == sctx->isc_nrxd) {
				rxq->ifr_cq_cidx = 0;
				rxq->ifr_cq_gen = 0;
			}
			/* was this only a completion queue message? */
			if (__predict_false(ri.iri_nfrags == 0))
				continue;
		}
		MPASS(ri.iri_nfrags != 0);
		MPASS(ri.iri_len != 0);

		/* will advance the cidx on the corresponding free lists */
		m = iflib_rxd_pkt_get(rxq, &ri);
		if (avail == 0 && budget_left)
			avail = iflib_rxd_avail(ctx, rxq, *cidxp);

		if (__predict_false(m == NULL)) {
			DBG_COUNTER_INC(rx_mbuf_null);
			continue;
		}
		/* imm_pkt: -- cxgb */
		if (mh == NULL)
			mh = mt = m;
		else {
			mt->m_nextpkt = m;
			mt = m;
		}
	}
	/* make sure that we can refill faster than drain */
	for (i = 0, fl = &rxq->ifr_fl[0]; i < sctx->isc_nfl; i++, fl++)
		__iflib_fl_refill_lt(ctx, fl, budget + 8);

	ifp = ctx->ifc_ifp;
	lro_enabled = (if_getcapenable(ifp) & IFCAP_LRO);

	while (mh != NULL) {
		m = mh;
		mh = mh->m_nextpkt;
		m->m_nextpkt = NULL;
		rx_bytes += m->m_pkthdr.len;
		rx_pkts++;
#if defined(INET6) || defined(INET)
		if (lro_enabled && tcp_lro_rx(&rxq->ifr_lc, m, 0) == 0)
			continue;
#endif
		DBG_COUNTER_INC(rx_if_input);
		ifp->if_input(ifp, m);
	}
	if_inc_counter(ifp, IFCOUNTER_IBYTES, rx_bytes);
	if_inc_counter(ifp, IFCOUNTER_IPACKETS, rx_pkts);

	/*
	 * Flush any outstanding LRO work
	 */
	while ((queued = LIST_FIRST(&rxq->ifr_lc.lro_active)) != NULL) {
		LIST_REMOVE(queued, next);
#if defined(INET6) || defined(INET)
		tcp_lro_flush(&rxq->ifr_lc, queued);
#endif
	}
	return (iflib_rxd_avail(ctx, rxq, *cidxp));
}

#define M_CSUM_FLAGS(m) ((m)->m_pkthdr.csum_flags)
#define M_HAS_VLANTAG(m) (m->m_flags & M_VLANTAG)
#define TXQ_MAX_DB_DEFERRED(ctx) (ctx->ifc_sctx->isc_ntxd >> 5)
#define TXQ_MAX_DB_CONSUMED(ctx) (ctx->ifc_sctx->isc_ntxd >> 4)

static __inline void
iflib_txd_db_check(if_ctx_t ctx, iflib_txq_t txq, int ring)
{
	uint32_t dbval;

	if (ring || txq->ift_db_pending >= TXQ_MAX_DB_DEFERRED(ctx)) {

		/* the lock will only ever be contended in the !min_latency case */
		if (!TXDB_TRYLOCK(txq))
			return;
		dbval = txq->ift_npending ? txq->ift_npending : txq->ift_pidx;
		ctx->isc_txd_flush(ctx->ifc_softc, txq->ift_id, dbval);
		txq->ift_db_pending = txq->ift_npending = 0;
		TXDB_UNLOCK(txq);
	}
}

static void
iflib_txd_deferred_db_check(void * arg)
{
	iflib_txq_t txq = arg;

	/* simple non-zero boolean so use bitwise OR */
	if ((txq->ift_db_pending | txq->ift_npending) &&
	    txq->ift_db_pending >= txq->ift_db_pending_queued)
		iflib_txd_db_check(txq->ift_ctx, txq, TRUE);
	txq->ift_db_pending_queued = 0;
	if (ifmp_ring_is_stalled(txq->ift_br[0]))
		iflib_txq_check_drain(txq, 4);
}

#ifdef PKT_DEBUG
static void
print_pkt(if_pkt_info_t pi)
{
	printf("pi len:  %d qsidx: %d nsegs: %d ndescs: %d flags: %x pidx: %d\n",
	       pi->ipi_len, pi->ipi_qsidx, pi->ipi_nsegs, pi->ipi_ndescs, pi->ipi_flags, pi->ipi_pidx);
	printf("pi new_pidx: %d csum_flags: %lx tso_segsz: %d mflags: %x vtag: %d\n",
	       pi->ipi_new_pidx, pi->ipi_csum_flags, pi->ipi_tso_segsz, pi->ipi_mflags, pi->ipi_vtag);
	printf("pi etype: %d ehdrlen: %d ip_hlen: %d ipproto: %d\n",
	       pi->ipi_etype, pi->ipi_ehdrlen, pi->ipi_ip_hlen, pi->ipi_ipproto);
}
#endif

#define IS_TSO4(pi) ((pi)->ipi_csum_flags & CSUM_IP_TSO)
#define IS_TSO6(pi) ((pi)->ipi_csum_flags & CSUM_IP6_TSO)

static int
iflib_parse_header(iflib_txq_t txq, if_pkt_info_t pi, struct mbuf **mp)
{
	struct ether_vlan_header *eh;
	struct mbuf *m;

	m = *mp;
	/*
	 * Determine where frame payload starts.
	 * Jump over vlan headers if already present,
	 * helpful for QinQ too.
	 */
	if (__predict_false(m->m_len < sizeof(*eh))) {
		txq->ift_pullups++;
		if (__predict_false((m = m_pullup(m, sizeof(*eh))) == NULL))
			return (ENOMEM);
	}
	eh = mtod(m, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		pi->ipi_etype = ntohs(eh->evl_proto);
		pi->ipi_ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		pi->ipi_etype = ntohs(eh->evl_encap_proto);
		pi->ipi_ehdrlen = ETHER_HDR_LEN;
	}

	switch (pi->ipi_etype) {
#ifdef INET
	case ETHERTYPE_IP:
	{
		struct ip *ip = NULL;
		struct tcphdr *th = NULL;
		struct mbuf *n;
		int minthlen;

		minthlen = min(m->m_pkthdr.len, pi->ipi_ehdrlen + sizeof(*ip) + sizeof(*th));
		if (__predict_false(m->m_len < minthlen)) {
			/*
			 * if this code bloat is causing too much of a hit
			 * move it to a separate function and mark it noinline
			 */
			if (m->m_len == pi->ipi_ehdrlen) {
				n = m->m_next;
				MPASS(n);
				if (n->m_len >= sizeof(*ip))  {
					ip = (struct ip *)n->m_data;
					if (n->m_len >= (ip->ip_hl << 2) + sizeof(*th))
						th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));
				} else {
					txq->ift_pullups++;
					if (__predict_false((m = m_pullup(m, minthlen)) == NULL))
						return (ENOMEM);
					ip = (struct ip *)(m->m_data + pi->ipi_ehdrlen);
				}
			} else {
				txq->ift_pullups++;
				if (__predict_false((m = m_pullup(m, minthlen)) == NULL))
					return (ENOMEM);
				ip = (struct ip *)(m->m_data + pi->ipi_ehdrlen);
				if (m->m_len >= (ip->ip_hl << 2) + sizeof(*th))
					th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));
			}
		} else {
			ip = (struct ip *)(m->m_data + pi->ipi_ehdrlen);
			if (m->m_len >= (ip->ip_hl << 2) + sizeof(*th))
				th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));
		}
		pi->ipi_ip_hlen = ip->ip_hl << 2;
		pi->ipi_ipproto = ip->ip_p;
		pi->ipi_flags |= IPI_TX_IPV4;

		if (pi->ipi_csum_flags & CSUM_IP)
                       ip->ip_sum = 0;

		if (pi->ipi_ipproto == IPPROTO_TCP) {
			if (__predict_false(th == NULL)) {
				txq->ift_pullups++;
				if (__predict_false((m = m_pullup(m, (ip->ip_hl << 2) + sizeof(*th))) == NULL))
					return (ENOMEM);
				th = (struct tcphdr *)((caddr_t)ip + pi->ipi_ip_hlen);
			}
			pi->ipi_tcp_hflags = th->th_flags;
			pi->ipi_tcp_hlen = th->th_off << 2;
			pi->ipi_tcp_seq = th->th_seq;
		}
		if (IS_TSO4(pi)) {
			if (__predict_false(ip->ip_p != IPPROTO_TCP))
				return (ENXIO);
			th->th_sum = in_pseudo(ip->ip_src.s_addr,
					       ip->ip_dst.s_addr, htons(IPPROTO_TCP));
			pi->ipi_tso_segsz = m->m_pkthdr.tso_segsz;
		}
		break;
	}
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
	{
		struct ip6_hdr *ip6 = (struct ip6_hdr *)(m->m_data + pi->ipi_ehdrlen);
		struct tcphdr *th;
		pi->ipi_ip_hlen = sizeof(struct ip6_hdr);

		if (__predict_false(m->m_len < pi->ipi_ehdrlen + sizeof(struct ip6_hdr))) {
			if (__predict_false((m = m_pullup(m, pi->ipi_ehdrlen + sizeof(struct ip6_hdr))) == NULL))
				return (ENOMEM);
		}
		th = (struct tcphdr *)((caddr_t)ip6 + pi->ipi_ip_hlen);

		/* XXX-BZ this will go badly in case of ext hdrs. */
		pi->ipi_ipproto = ip6->ip6_nxt;
		pi->ipi_flags |= IPI_TX_IPV6;

		if (pi->ipi_ipproto == IPPROTO_TCP) {
			if (__predict_false(m->m_len < pi->ipi_ehdrlen + sizeof(struct ip6_hdr) + sizeof(struct tcphdr))) {
				if (__predict_false((m = m_pullup(m, pi->ipi_ehdrlen + sizeof(struct ip6_hdr) + sizeof(struct tcphdr))) == NULL))
					return (ENOMEM);
			}
			pi->ipi_tcp_hflags = th->th_flags;
			pi->ipi_tcp_hlen = th->th_off << 2;
		}
		if (IS_TSO6(pi)) {

			if (__predict_false(ip6->ip6_nxt != IPPROTO_TCP))
				return (ENXIO);
			/*
			 * The corresponding flag is set by the stack in the IPv4
			 * TSO case, but not in IPv6 (at least in FreeBSD 10.2).
			 * So, set it here because the rest of the flow requires it.
			 */
			pi->ipi_csum_flags |= CSUM_TCP_IPV6;
			th->th_sum = in6_cksum_pseudo(ip6, 0, IPPROTO_TCP, 0);
			pi->ipi_tso_segsz = m->m_pkthdr.tso_segsz;
		}
		break;
	}
#endif
	default:
		pi->ipi_csum_flags &= ~CSUM_OFFLOAD;
		pi->ipi_ip_hlen = 0;
		break;
	}
	*mp = m;
	return (0);
}


static  __noinline  struct mbuf *
collapse_pkthdr(struct mbuf *m0)
{
	struct mbuf *m, *m_next, *tmp;

	m = m0;
	m_next = m->m_next;
	while (m_next != NULL && m_next->m_len == 0) {
		m = m_next;
		m->m_next = NULL;
		m_free(m);
		m_next = m_next->m_next;
	}
	m = m0;
	m->m_next = m_next;
	if ((m_next->m_flags & M_EXT) == 0) {
		m = m_defrag(m, M_NOWAIT);
	} else {
		tmp = m_next->m_next;
		memcpy(m_next, m, MPKTHSIZE);
		m = m_next;
		m->m_next = tmp;
	}
	return (m);
}

/*
 * If dodgy hardware rejects the scatter gather chain we've handed it
 * we'll need to rebuild the mbuf chain before we can call m_defrag
 */
static __noinline struct mbuf *
iflib_rebuild_mbuf(iflib_txq_t txq)
{

	int ntxd, mhlen, len, i, pidx;
	struct mbuf *m, *mh, **ifsd_m;
	if_shared_ctx_t		sctx;

	pidx = txq->ift_pidx;
	ifsd_m = txq->ift_sds.ifsd_m;
	sctx = txq->ift_ctx->ifc_sctx;
	ntxd = sctx->isc_ntxd;
	mh = m = ifsd_m[pidx];
	ifsd_m[pidx] = NULL;
#if MEMORY_LOGGING
	txq->ift_dequeued++;
#endif
	len = m->m_len;
	mhlen = m->m_pkthdr.len;
	i = 1;

	while (len < mhlen && (m->m_next == NULL)) {
		m->m_next = ifsd_m[(pidx + i) & (ntxd-1)];
		ifsd_m[(pidx + i) & (ntxd -1)] = NULL;
#if MEMORY_LOGGING
		txq->ift_dequeued++;
#endif
		m = m->m_next;
		len += m->m_len;
		i++;
	}
	return (mh);
}

static int
iflib_busdma_load_mbuf_sg(iflib_txq_t txq, bus_dma_tag_t tag, bus_dmamap_t map,
			  struct mbuf **m0, bus_dma_segment_t *segs, int *nsegs,
			  int max_segs, int flags)
{
	if_ctx_t ctx;
	if_shared_ctx_t		sctx;
	int i, next, pidx, mask, err, maxsegsz, ntxd, count;
	struct mbuf *m, *tmp, **ifsd_m, **mp;

	m = *m0;

	/*
	 * Please don't ever do this
	 */
	if (__predict_false(m->m_len == 0))
		*m0 = m = collapse_pkthdr(m);

	ctx = txq->ift_ctx;
	sctx = ctx->ifc_sctx;
	ifsd_m = txq->ift_sds.ifsd_m;
	ntxd = sctx->isc_ntxd;
	pidx = txq->ift_pidx;
	if (map != NULL) {
		uint8_t *ifsd_flags = txq->ift_sds.ifsd_flags;

		err = bus_dmamap_load_mbuf_sg(tag, map,
					      *m0, segs, nsegs, BUS_DMA_NOWAIT);
		if (err)
			return (err);
		ifsd_flags[pidx] |= TX_SW_DESC_MAPPED;
		i = 0;
		next = pidx;
		mask = (sctx->isc_ntxd-1);
		m = *m0;
		do {
			mp = &ifsd_m[next];
			*mp = m;
			m = m->m_next;
			(*mp)->m_next = NULL;
			if (__predict_false((*mp)->m_len == 0)) {
				m_free(*mp);
				*mp = NULL;
			} else
				next = (pidx + i) & (ntxd-1);
		} while (m != NULL);
	} else {
		int buflen, sgsize, max_sgsize;
		vm_offset_t vaddr;
		vm_paddr_t curaddr;

		count = i = 0;
		maxsegsz = sctx->isc_tx_maxsize;
		m = *m0;
		do {
			if (__predict_false(m->m_len <= 0)) {
				tmp = m;
				m = m->m_next;
				tmp->m_next = NULL;
				m_free(tmp);
				continue;
			}
			buflen = m->m_len;
			vaddr = (vm_offset_t)m->m_data;
			/*
			 * see if we can't be smarter about physically
			 * contiguous mappings
			 */
			next = (pidx + count) & (ntxd-1);
			MPASS(ifsd_m[next] == NULL);
#if MEMORY_LOGGING
			txq->ift_enqueued++;
#endif
			ifsd_m[next] = m;
			while (buflen > 0) {
				max_sgsize = MIN(buflen, maxsegsz);
				curaddr = pmap_kextract(vaddr);
				sgsize = PAGE_SIZE - (curaddr & PAGE_MASK);
				sgsize = MIN(sgsize, max_sgsize);
				segs[i].ds_addr = curaddr;
				segs[i].ds_len = sgsize;
				vaddr += sgsize;
				buflen -= sgsize;
				i++;
				if (i >= max_segs)
					goto err;
			}
			count++;
			tmp = m;
			m = m->m_next;
			tmp->m_next = NULL;
		} while (m != NULL);
		*nsegs = i;
	}
	return (0);
err:
	*m0 = iflib_rebuild_mbuf(txq);
	return (EFBIG);
}

static int
iflib_encap(iflib_txq_t txq, struct mbuf **m_headp)
{
	if_ctx_t		ctx;
	if_shared_ctx_t		sctx;
	if_softc_ctx_t		scctx;
	bus_dma_segment_t	*segs;
	struct mbuf		*m_head;
	bus_dmamap_t		map;
	struct if_pkt_info	pi;
	int remap = 0;
	int err, nsegs, ndesc, max_segs, pidx, cidx, next, ntxd;
	bus_dma_tag_t desc_tag;

	segs = txq->ift_segs;
	ctx = txq->ift_ctx;
	sctx = ctx->ifc_sctx;
	scctx = &ctx->ifc_softc_ctx;
	segs = txq->ift_segs;
	ntxd = sctx->isc_ntxd;
	m_head = *m_headp;
	map = NULL;

	/*
	 * If we're doing TSO the next descriptor to clean may be quite far ahead
	 */
	cidx = txq->ift_cidx;
	pidx = txq->ift_pidx;
	next = (cidx + CACHE_PTR_INCREMENT) & (ntxd-1);

	/* prefetch the next cache line of mbuf pointers and flags */
	prefetch(&txq->ift_sds.ifsd_m[next]);
	if (txq->ift_sds.ifsd_map != NULL) {
		prefetch(&txq->ift_sds.ifsd_map[next]);
		map = txq->ift_sds.ifsd_map[pidx];
		next = (cidx + CACHE_LINE_SIZE) & (ntxd-1);
		prefetch(&txq->ift_sds.ifsd_flags[next]);
	}


	if (m_head->m_pkthdr.csum_flags & CSUM_TSO) {
		desc_tag = txq->ift_tso_desc_tag;
		max_segs = scctx->isc_tx_tso_segments_max;
	} else {
		desc_tag = txq->ift_desc_tag;
		max_segs = scctx->isc_tx_nsegments;
	}
	m_head = *m_headp;
	bzero(&pi, sizeof(pi));
	pi.ipi_len = m_head->m_pkthdr.len;
	pi.ipi_mflags = (m_head->m_flags & (M_VLANTAG|M_BCAST|M_MCAST));
	pi.ipi_csum_flags = m_head->m_pkthdr.csum_flags;
	pi.ipi_vtag = (m_head->m_flags & M_VLANTAG) ? m_head->m_pkthdr.ether_vtag : 0;
	pi.ipi_pidx = pidx;
	pi.ipi_qsidx = txq->ift_id;

	/* deliberate bitwise OR to make one condition */
	if (__predict_true((pi.ipi_csum_flags | pi.ipi_vtag))) {
		if (__predict_false((err = iflib_parse_header(txq, &pi, m_headp)) != 0))
			return (err);
		m_head = *m_headp;
	}

retry:
	err = iflib_busdma_load_mbuf_sg(txq, desc_tag, map, m_headp, segs, &nsegs, max_segs, BUS_DMA_NOWAIT);
defrag:
	if (__predict_false(err)) {
		switch (err) {
		case EFBIG:
			/* try collapse once and defrag once */
			if (remap == 0)
				m_head = m_collapse(*m_headp, M_NOWAIT, max_segs);
			if (remap == 1)
				m_head = m_defrag(*m_headp, M_NOWAIT);
			remap++;
			if (__predict_false(m_head == NULL))
				goto defrag_failed;
			txq->ift_mbuf_defrag++;
			*m_headp = m_head;
			goto retry;
			break;
		case ENOMEM:
			txq->ift_no_tx_dma_setup++;
			break;
		default:
			txq->ift_no_tx_dma_setup++;
			m_freem(*m_headp);
			DBG_COUNTER_INC(tx_frees);
			*m_headp = NULL;
			break;
		}
		txq->ift_map_failed++;
		DBG_COUNTER_INC(encap_load_mbuf_fail);
		return (err);
	}

	/*
	 * XXX assumes a 1 to 1 relationship between segments and
	 *        descriptors - this does not hold true on all drivers, e.g.
	 *        cxgb
	 */
	if (__predict_false(nsegs + 2 > TXQ_AVAIL(txq))) {
		txq->ift_no_desc_avail++;
		if (map != NULL)
			bus_dmamap_unload(desc_tag, map);
		DBG_COUNTER_INC(encap_txq_avail_fail);
		if (txq->ift_task.gt_task.ta_pending == 0)
			GROUPTASK_ENQUEUE(&txq->ift_task);
		return (ENOBUFS);
	}
	pi.ipi_segs = segs;
	pi.ipi_nsegs = nsegs;

	MPASS(pidx >= 0 && pidx < sctx->isc_ntxd);
#ifdef PKT_DEBUG
	print_pkt(&pi);
#endif
	if ((err = ctx->isc_txd_encap(ctx->ifc_softc, &pi)) == 0) {
		bus_dmamap_sync(txq->ift_ifdi->idi_tag, txq->ift_ifdi->idi_map,
						BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		DBG_COUNTER_INC(tx_encap);
		MPASS(pi.ipi_new_pidx >= 0 && pi.ipi_new_pidx < sctx->isc_ntxd);

		ndesc = pi.ipi_new_pidx - pi.ipi_pidx;
		if (pi.ipi_new_pidx < pi.ipi_pidx) {
			ndesc += sctx->isc_ntxd;
			txq->ift_gen = 1;
		}
		MPASS(pi.ipi_new_pidx != pidx);
		MPASS(ndesc > 0);
		txq->ift_in_use += ndesc;
		/*
		 * We update the last software descriptor again here because there may
		 * be a sentinel and/or there may be more mbufs than segments
		 */
		txq->ift_pidx = pi.ipi_new_pidx;
		txq->ift_npending += pi.ipi_ndescs;
	} else if (__predict_false(err == EFBIG && remap < 2)) {
		*m_headp = m_head = iflib_rebuild_mbuf(txq);
		remap = 1;
		txq->ift_txd_encap_efbig++;
		goto defrag;
	} else
		DBG_COUNTER_INC(encap_txd_encap_fail);
	return (err);

defrag_failed:
	txq->ift_mbuf_defrag_failed++;
	txq->ift_map_failed++;
	m_freem(*m_headp);
	DBG_COUNTER_INC(tx_frees);
	*m_headp = NULL;
	return (ENOMEM);
}

/* forward compatibility for cxgb */
#define FIRST_QSET(ctx) 0

#define NTXQSETS(ctx) ((ctx)->ifc_softc_ctx.isc_ntxqsets)
#define NRXQSETS(ctx) ((ctx)->ifc_softc_ctx.isc_nrxqsets)
#define QIDX(ctx, m) ((((m)->m_pkthdr.flowid & ctx->ifc_softc_ctx.isc_rss_table_mask) % NRXQSETS(ctx)) + FIRST_QSET(ctx))
#define DESC_RECLAIMABLE(q) ((int)((q)->ift_processed - (q)->ift_cleaned - (q)->ift_ctx->ifc_softc_ctx.isc_tx_nsegments))
#define RECLAIM_THRESH(ctx) ((ctx)->ifc_sctx->isc_tx_reclaim_thresh)
#define MAX_TX_DESC(ctx) ((ctx)->ifc_softc_ctx.isc_tx_tso_segments_max)



/* if there are more than TXQ_MIN_OCCUPANCY packets pending we consider deferring
 * doorbell writes
 *
 * ORing with 2 assures that min occupancy is never less than 2 without any conditional logic
 */
#define TXQ_MIN_OCCUPANCY(ctx) ((ctx->ifc_sctx->isc_ntxd >> 6)| 0x2)

static inline int
iflib_txq_min_occupancy(iflib_txq_t txq)
{
	if_ctx_t ctx;

	ctx = txq->ift_ctx;
	return (get_inuse(txq->ift_size, txq->ift_cidx, txq->ift_pidx, txq->ift_gen) < TXQ_MIN_OCCUPANCY(ctx) + MAX_TX_DESC(ctx));
}

static void
iflib_tx_desc_free(iflib_txq_t txq, int n)
{
	int hasmap;
	uint32_t qsize, cidx, mask, gen;
	struct mbuf *m, **ifsd_m;
	uint8_t *ifsd_flags;
	bus_dmamap_t *ifsd_map;

	cidx = txq->ift_cidx;
	gen = txq->ift_gen;
	qsize = txq->ift_ctx->ifc_sctx->isc_ntxd;
	mask = qsize-1;
	hasmap = txq->ift_sds.ifsd_map != NULL;
	ifsd_flags = txq->ift_sds.ifsd_flags;
	ifsd_m = txq->ift_sds.ifsd_m;
	ifsd_map = txq->ift_sds.ifsd_map;

	while (n--) {
		prefetch(ifsd_m[(cidx + 3) & mask]);
		prefetch(ifsd_m[(cidx + 4) & mask]);

		if (ifsd_m[cidx] != NULL) {
			prefetch(&ifsd_m[(cidx + CACHE_PTR_INCREMENT) & mask]);
			prefetch(&ifsd_flags[(cidx + CACHE_PTR_INCREMENT) & mask]);
			if (hasmap && (ifsd_flags[cidx] & TX_SW_DESC_MAPPED)) {
				/*
				 * does it matter if it's not the TSO tag? If so we'll
				 * have to add the type to flags
				 */
				bus_dmamap_unload(txq->ift_desc_tag, ifsd_map[cidx]);
				ifsd_flags[cidx] &= ~TX_SW_DESC_MAPPED;
			}
			if ((m = ifsd_m[cidx]) != NULL) {
				/* XXX we don't support any drivers that batch packets yet */
				MPASS(m->m_nextpkt == NULL);

				m_freem(m);
				ifsd_m[cidx] = NULL;
#if MEMORY_LOGGING
				txq->ift_dequeued++;
#endif
				DBG_COUNTER_INC(tx_frees);
			}
		}
		if (__predict_false(++cidx == qsize)) {
			cidx = 0;
			gen = 0;
		}
	}
	txq->ift_cidx = cidx;
	txq->ift_gen = gen;
}

static __inline int
iflib_completed_tx_reclaim(iflib_txq_t txq, int thresh)
{
	int reclaim;
	if_ctx_t ctx = txq->ift_ctx;

	KASSERT(thresh >= 0, ("invalid threshold to reclaim"));
	MPASS(thresh /*+ MAX_TX_DESC(txq->ift_ctx) */ < txq->ift_size);

	/*
	 * Need a rate-limiting check so that this isn't called every time
	 */
	iflib_tx_credits_update(ctx, txq);
	reclaim = DESC_RECLAIMABLE(txq);

	if (reclaim <= thresh /* + MAX_TX_DESC(txq->ift_ctx) */) {
#ifdef INVARIANTS
		if (iflib_verbose_debug) {
			printf("%s processed=%ju cleaned=%ju tx_nsegments=%d reclaim=%d thresh=%d\n", __FUNCTION__,
			       txq->ift_processed, txq->ift_cleaned, txq->ift_ctx->ifc_softc_ctx.isc_tx_nsegments,
			       reclaim, thresh);

		}
#endif
		return (0);
	}
	iflib_tx_desc_free(txq, reclaim);
	txq->ift_cleaned += reclaim;
	txq->ift_in_use -= reclaim;

	if (txq->ift_active == FALSE)
		txq->ift_active = TRUE;

	return (reclaim);
}

static struct mbuf **
_ring_peek_one(struct ifmp_ring *r, int cidx, int offset)
{

	return (__DEVOLATILE(struct mbuf **, &r->items[(cidx + offset) & (r->size-1)]));
}

static void
iflib_txq_check_drain(iflib_txq_t txq, int budget)
{

	ifmp_ring_check_drainage(txq->ift_br[0], budget);
}

static uint32_t
iflib_txq_can_drain(struct ifmp_ring *r)
{
	iflib_txq_t txq = r->cookie;
	if_ctx_t ctx = txq->ift_ctx;

	return ((TXQ_AVAIL(txq) >= MAX_TX_DESC(ctx)) ||
		ctx->isc_txd_credits_update(ctx->ifc_softc, txq->ift_id, txq->ift_cidx_processed, false));
}

static uint32_t
iflib_txq_drain(struct ifmp_ring *r, uint32_t cidx, uint32_t pidx)
{
	iflib_txq_t txq = r->cookie;
	if_ctx_t ctx = txq->ift_ctx;
	if_t ifp = ctx->ifc_ifp;
	struct mbuf **mp, *m;
	int i, count, consumed, pkt_sent, bytes_sent, mcast_sent, avail, err, in_use_prev, desc_used;

	if (__predict_false(!(if_getdrvflags(ifp) & IFF_DRV_RUNNING) ||
			    !LINK_ACTIVE(ctx))) {
		DBG_COUNTER_INC(txq_drain_notready);
		return (0);
	}

	avail = IDXDIFF(pidx, cidx, r->size);
	if (__predict_false(ctx->ifc_flags & IFC_QFLUSH)) {
		DBG_COUNTER_INC(txq_drain_flushing);
		for (i = 0; i < avail; i++) {
			m_freem(r->items[(cidx + i) & (r->size-1)]);
			r->items[(cidx + i) & (r->size-1)] = NULL;
		}
		return (avail);
	}
	iflib_completed_tx_reclaim(txq, RECLAIM_THRESH(ctx));
	if (__predict_false(if_getdrvflags(ctx->ifc_ifp) & IFF_DRV_OACTIVE)) {
		txq->ift_qstatus = IFLIB_QUEUE_IDLE;
		CALLOUT_LOCK(txq);
		callout_stop(&txq->ift_timer);
		callout_stop(&txq->ift_db_check);
		CALLOUT_UNLOCK(txq);
		DBG_COUNTER_INC(txq_drain_oactive);
		return (0);
	}
	consumed = mcast_sent = bytes_sent = pkt_sent = 0;
	count = MIN(avail, TX_BATCH_SIZE);

	for (desc_used = i = 0; i < count && TXQ_AVAIL(txq) > MAX_TX_DESC(ctx) + 2; i++) {
		mp = _ring_peek_one(r, cidx, i);
		in_use_prev = txq->ift_in_use;
		err = iflib_encap(txq, mp);
		/*
		 * What other errors should we bail out for?
		 */
		if (err == ENOBUFS) {
			DBG_COUNTER_INC(txq_drain_encapfail);
			break;
		}
		consumed++;
		if (err)
			continue;

		pkt_sent++;
		m = *mp;
		DBG_COUNTER_INC(tx_sent);
		bytes_sent += m->m_pkthdr.len;
		if (m->m_flags & M_MCAST)
			mcast_sent++;

		txq->ift_db_pending += (txq->ift_in_use - in_use_prev);
		desc_used += (txq->ift_in_use - in_use_prev);
		iflib_txd_db_check(ctx, txq, FALSE);
		ETHER_BPF_MTAP(ifp, m);
		if (__predict_false(!(if_getdrvflags(ctx->ifc_ifp) & IFF_DRV_RUNNING)))
			break;

		if (desc_used > TXQ_MAX_DB_CONSUMED(ctx))
			break;
	}

	if ((iflib_min_tx_latency || iflib_txq_min_occupancy(txq)) && txq->ift_db_pending)
		iflib_txd_db_check(ctx, txq, TRUE);
	else if ((txq->ift_db_pending || TXQ_AVAIL(txq) < MAX_TX_DESC(ctx)) &&
		 (callout_pending(&txq->ift_db_check) == 0)) {
		txq->ift_db_pending_queued = txq->ift_db_pending;
		callout_reset_on(&txq->ift_db_check, 1, iflib_txd_deferred_db_check,
				 txq, txq->ift_db_check.c_cpu);
	}
	if_inc_counter(ifp, IFCOUNTER_OBYTES, bytes_sent);
	if_inc_counter(ifp, IFCOUNTER_OPACKETS, pkt_sent);
	if (mcast_sent)
		if_inc_counter(ifp, IFCOUNTER_OMCASTS, mcast_sent);

	return (consumed);
}

static void
_task_fn_tx(void *context, int pending)
{
	iflib_txq_t txq = context;
	if_ctx_t ctx = txq->ift_ctx;

	if (!(if_getdrvflags(ctx->ifc_ifp) & IFF_DRV_RUNNING))
		return;
	ifmp_ring_check_drainage(txq->ift_br[0], TX_BATCH_SIZE);
}

static void
_task_fn_rx(void *context, int pending)
{
	iflib_rxq_t rxq = context;
	if_ctx_t ctx = rxq->ifr_ctx;
	bool more;

	DBG_COUNTER_INC(task_fn_rxs);
	if (__predict_false(!(if_getdrvflags(ctx->ifc_ifp) & IFF_DRV_RUNNING)))
		return;

	if ((more = iflib_rxeof(rxq, 16 /* XXX */)) == false) {
		if (ctx->ifc_flags & IFC_LEGACY)
			IFDI_INTR_ENABLE(ctx);
		else {
			DBG_COUNTER_INC(rx_intr_enables);
			IFDI_QUEUE_INTR_ENABLE(ctx, rxq->ifr_id);
		}
	}
	if (__predict_false(!(if_getdrvflags(ctx->ifc_ifp) & IFF_DRV_RUNNING)))
		return;
	if (more)
		GROUPTASK_ENQUEUE(&rxq->ifr_task);
}

static void
_task_fn_admin(void *context, int pending)
{
	if_ctx_t ctx = context;
	if_softc_ctx_t sctx = &ctx->ifc_softc_ctx;
	iflib_txq_t txq;
	int i;

	if (!(if_getdrvflags(ctx->ifc_ifp) & IFF_DRV_RUNNING))
		return;

	CTX_LOCK(ctx);
	for (txq = ctx->ifc_txqs, i = 0; i < sctx->isc_ntxqsets; i++, txq++) {
		CALLOUT_LOCK(txq);
		callout_stop(&txq->ift_timer);
		CALLOUT_UNLOCK(txq);
	}
	IFDI_UPDATE_ADMIN_STATUS(ctx);
	for (txq = ctx->ifc_txqs, i = 0; i < sctx->isc_ntxqsets; i++, txq++)
		callout_reset_on(&txq->ift_timer, hz/2, iflib_timer, txq, txq->ift_timer.c_cpu);
	IFDI_LINK_INTR_ENABLE(ctx);
	CTX_UNLOCK(ctx);

	if (LINK_ACTIVE(ctx) == 0)
		return;
	for (txq = ctx->ifc_txqs, i = 0; i < sctx->isc_ntxqsets; i++, txq++)
		iflib_txq_check_drain(txq, IFLIB_RESTART_BUDGET);
}


static void
_task_fn_iov(void *context, int pending)
{
	if_ctx_t ctx = context;

	if (!(if_getdrvflags(ctx->ifc_ifp) & IFF_DRV_RUNNING))
		return;

	CTX_LOCK(ctx);
	IFDI_VFLR_HANDLE(ctx);
	CTX_UNLOCK(ctx);
}

static int
iflib_sysctl_int_delay(SYSCTL_HANDLER_ARGS)
{
	int err;
	if_int_delay_info_t info;
	if_ctx_t ctx;

	info = (if_int_delay_info_t)arg1;
	ctx = info->iidi_ctx;
	info->iidi_req = req;
	info->iidi_oidp = oidp;
	CTX_LOCK(ctx);
	err = IFDI_SYSCTL_INT_DELAY(ctx, info);
	CTX_UNLOCK(ctx);
	return (err);
}

/*********************************************************************
 *
 *  IFNET FUNCTIONS
 *
 **********************************************************************/

static void
iflib_if_init_locked(if_ctx_t ctx)
{
	iflib_stop(ctx);
	iflib_init_locked(ctx);
}


static void
iflib_if_init(void *arg)
{
	if_ctx_t ctx = arg;

	CTX_LOCK(ctx);
	iflib_if_init_locked(ctx);
	CTX_UNLOCK(ctx);
}

static int
iflib_if_transmit(if_t ifp, struct mbuf *m)
{
	if_ctx_t	ctx = if_getsoftc(ifp);

	iflib_txq_t txq;
	struct mbuf *marr[8], **mp, *next;
	int err, i, count, qidx;

	if (__predict_false((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0 || !LINK_ACTIVE(ctx))) {
		DBG_COUNTER_INC(tx_frees);
		m_freem(m);
		return (0);
	}

	qidx = 0;
	if ((NTXQSETS(ctx) > 1) && M_HASHTYPE_GET(m))
		qidx = QIDX(ctx, m);
	/*
	 * XXX calculate buf_ring based on flowid (divvy up bits?)
	 */
	txq = &ctx->ifc_txqs[qidx];

#ifdef DRIVER_BACKPRESSURE
	if (txq->ift_closed) {
		while (m != NULL) {
			next = m->m_nextpkt;
			m->m_nextpkt = NULL;
			m_freem(m);
			m = next;
		}
		return (ENOBUFS);
	}
#endif
	qidx = count = 0;
	mp = marr;
	next = m;
	do {
		count++;
		next = next->m_nextpkt;
	} while (next != NULL);

	if (count > nitems(marr))
		if ((mp = malloc(count*sizeof(struct mbuf *), M_IFLIB, M_NOWAIT)) == NULL) {
			/* XXX check nextpkt */
			m_freem(m);
			/* XXX simplify for now */
			DBG_COUNTER_INC(tx_frees);
			return (ENOBUFS);
		}
	for (next = m, i = 0; next != NULL; i++) {
		mp[i] = next;
		next = next->m_nextpkt;
		mp[i]->m_nextpkt = NULL;
	}
	DBG_COUNTER_INC(tx_seen);
	err = ifmp_ring_enqueue(txq->ift_br[0], (void **)mp, count, TX_BATCH_SIZE);

	if (iflib_txq_can_drain(txq->ift_br[0]))
		GROUPTASK_ENQUEUE(&txq->ift_task);
	if (err) {
		/* support forthcoming later */
#ifdef DRIVER_BACKPRESSURE
		txq->ift_closed = TRUE;
#endif
		for (i = 0; i < count; i++)
			m_freem(mp[i]);
		ifmp_ring_check_drainage(txq->ift_br[0], TX_BATCH_SIZE);
	}
	if (count > nitems(marr))
		free(mp, M_IFLIB);

	return (err);
}

static void
iflib_if_qflush(if_t ifp)
{
	if_ctx_t ctx = if_getsoftc(ifp);
	iflib_txq_t txq = ctx->ifc_txqs;
	int i;

	CTX_LOCK(ctx);
	ctx->ifc_flags |= IFC_QFLUSH;
	CTX_UNLOCK(ctx);
	for (i = 0; i < NTXQSETS(ctx); i++, txq++)
		while (!(ifmp_ring_is_idle(txq->ift_br[0]) || ifmp_ring_is_stalled(txq->ift_br[0])))
			iflib_txq_check_drain(txq, 0);
	CTX_LOCK(ctx);
	ctx->ifc_flags &= ~IFC_QFLUSH;
	CTX_UNLOCK(ctx);

	if_qflush(ifp);
}

#define IFCAP_REINIT (IFCAP_HWCSUM|IFCAP_TSO4|IFCAP_TSO6|IFCAP_VLAN_HWTAGGING|IFCAP_VLAN_MTU | \
		      IFCAP_VLAN_HWFILTER | IFCAP_VLAN_HWTSO)

#define IFCAP_FLAGS (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6 | IFCAP_HWCSUM | IFCAP_LRO | \
		     IFCAP_TSO4 | IFCAP_TSO6 | IFCAP_VLAN_HWTAGGING |	\
		     IFCAP_VLAN_MTU | IFCAP_VLAN_HWFILTER | IFCAP_VLAN_HWTSO)

static int
iflib_if_ioctl(if_t ifp, u_long command, caddr_t data)
{
	if_ctx_t ctx = if_getsoftc(ifp);
	struct ifreq	*ifr = (struct ifreq *)data;
#if defined(INET) || defined(INET6)
	struct ifaddr	*ifa = (struct ifaddr *)data;
#endif
	bool		avoid_reset = FALSE;
	int		err = 0, reinit = 0, bits;

	switch (command) {
	case SIOCSIFADDR:
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			avoid_reset = TRUE;
#endif
#ifdef INET6
		if (ifa->ifa_addr->sa_family == AF_INET6)
			avoid_reset = TRUE;
#endif
		/*
		** Calling init results in link renegotiation,
		** so we avoid doing it when possible.
		*/
		if (avoid_reset) {
			if_setflagbits(ifp, IFF_UP,0);
			if (!(if_getdrvflags(ifp)& IFF_DRV_RUNNING))
				reinit = 1;
#ifdef INET
			if (!(if_getflags(ifp) & IFF_NOARP))
				arp_ifinit(ifp, ifa);
#endif
		} else
			err = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFMTU:
		CTX_LOCK(ctx);
		if (ifr->ifr_mtu == if_getmtu(ifp)) {
			CTX_UNLOCK(ctx);
			break;
		}
		bits = if_getdrvflags(ifp);
		/* stop the driver and free any clusters before proceeding */
		iflib_stop(ctx);

		if ((err = IFDI_MTU_SET(ctx, ifr->ifr_mtu)) == 0) {
			if (ifr->ifr_mtu > ctx->ifc_max_fl_buf_size)
				ctx->ifc_flags |= IFC_MULTISEG;
			else
				ctx->ifc_flags &= ~IFC_MULTISEG;
			err = if_setmtu(ifp, ifr->ifr_mtu);
		}
		iflib_init_locked(ctx);
		if_setdrvflags(ifp, bits);
		CTX_UNLOCK(ctx);
		break;
	case SIOCSIFFLAGS:
		CTX_LOCK(ctx);
		if (if_getflags(ifp) & IFF_UP) {
			if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
				if ((if_getflags(ifp) ^ ctx->ifc_if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) {
					err = IFDI_PROMISC_SET(ctx, if_getflags(ifp));
				}
			} else
				reinit = 1;
		} else if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
			iflib_stop(ctx);
		}
		ctx->ifc_if_flags = if_getflags(ifp);
		CTX_UNLOCK(ctx);
		break;

		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
			CTX_LOCK(ctx);
			IFDI_INTR_DISABLE(ctx);
			IFDI_MULTI_SET(ctx);
			IFDI_INTR_ENABLE(ctx);
			CTX_UNLOCK(ctx);
		}
		break;
	case SIOCSIFMEDIA:
		CTX_LOCK(ctx);
		IFDI_MEDIA_SET(ctx);
		CTX_UNLOCK(ctx);
		/* falls thru */
	case SIOCGIFMEDIA:
		err = ifmedia_ioctl(ifp, ifr, &ctx->ifc_media, command);
		break;
	case SIOCGI2C:
	{
		struct ifi2creq i2c;

		err = copyin(ifr->ifr_data, &i2c, sizeof(i2c));
		if (err != 0)
			break;
		if (i2c.dev_addr != 0xA0 && i2c.dev_addr != 0xA2) {
			err = EINVAL;
			break;
		}
		if (i2c.len > sizeof(i2c.data)) {
			err = EINVAL;
			break;
		}

		if ((err = IFDI_I2C_REQ(ctx, &i2c)) == 0)
			err = copyout(&i2c, ifr->ifr_data, sizeof(i2c));
		break;
	}
	case SIOCSIFCAP:
	{
		int mask, setmask;

		mask = ifr->ifr_reqcap ^ if_getcapenable(ifp);
		setmask = 0;
#ifdef TCP_OFFLOAD
		setmask |= mask & (IFCAP_TOE4|IFCAP_TOE6);
#endif
		setmask |= (mask & IFCAP_FLAGS);

		if ((mask & IFCAP_WOL) &&
		    (if_getcapabilities(ifp) & IFCAP_WOL) != 0)
			setmask |= (mask & (IFCAP_WOL_MCAST|IFCAP_WOL_MAGIC));
		if_vlancap(ifp);
		/*
		 * want to ensure that traffic has stopped before we change any of the flags
		 */
		if (setmask) {
			CTX_LOCK(ctx);
			bits = if_getdrvflags(ifp);
			if (setmask & IFCAP_REINIT)
				iflib_stop(ctx);
			if_togglecapenable(ifp, setmask);
			if (setmask & IFCAP_REINIT)
				iflib_init_locked(ctx);
			if_setdrvflags(ifp, bits);
			CTX_UNLOCK(ctx);
		}
		break;
	    }
	case SIOCGPRIVATE_0:
	case SIOCSDRVSPEC:
	case SIOCGDRVSPEC:
		CTX_LOCK(ctx);
		err = IFDI_PRIV_IOCTL(ctx, command, data);
		CTX_UNLOCK(ctx);
		break;
	default:
		err = ether_ioctl(ifp, command, data);
		break;
	}
	if (reinit)
		iflib_if_init(ctx);
	return (err);
}

static uint64_t
iflib_if_get_counter(if_t ifp, ift_counter cnt)
{
	if_ctx_t ctx = if_getsoftc(ifp);

	return (IFDI_GET_COUNTER(ctx, cnt));
}

/*********************************************************************
 *
 *  OTHER FUNCTIONS EXPORTED TO THE STACK
 *
 **********************************************************************/

static void
iflib_vlan_register(void *arg, if_t ifp, uint16_t vtag)
{
	if_ctx_t ctx = if_getsoftc(ifp);

	if ((void *)ctx != arg)
		return;

	if ((vtag == 0) || (vtag > 4095))
		return;

	CTX_LOCK(ctx);
	IFDI_VLAN_REGISTER(ctx, vtag);
	/* Re-init to load the changes */
	if (if_getcapenable(ifp) & IFCAP_VLAN_HWFILTER)
		iflib_init_locked(ctx);
	CTX_UNLOCK(ctx);
}

static void
iflib_vlan_unregister(void *arg, if_t ifp, uint16_t vtag)
{
	if_ctx_t ctx = if_getsoftc(ifp);

	if ((void *)ctx != arg)
		return;

	if ((vtag == 0) || (vtag > 4095))
		return;

	CTX_LOCK(ctx);
	IFDI_VLAN_UNREGISTER(ctx, vtag);
	/* Re-init to load the changes */
	if (if_getcapenable(ifp) & IFCAP_VLAN_HWFILTER)
		iflib_init_locked(ctx);
	CTX_UNLOCK(ctx);
}

static void
iflib_led_func(void *arg, int onoff)
{
	if_ctx_t ctx = arg;

	CTX_LOCK(ctx);
	IFDI_LED_FUNC(ctx, onoff);
	CTX_UNLOCK(ctx);
}

/*********************************************************************
 *
 *  BUS FUNCTION DEFINITIONS
 *
 **********************************************************************/

int
iflib_device_probe(device_t dev)
{
	pci_vendor_info_t *ent;

	uint16_t	pci_vendor_id, pci_device_id;
	uint16_t	pci_subvendor_id, pci_subdevice_id;
	uint16_t	pci_rev_id;
	if_shared_ctx_t sctx;

	if ((sctx = DEVICE_REGISTER(dev)) == NULL || sctx->isc_magic != IFLIB_MAGIC)
		return (ENOTSUP);

	pci_vendor_id = pci_get_vendor(dev);
	pci_device_id = pci_get_device(dev);
	pci_subvendor_id = pci_get_subvendor(dev);
	pci_subdevice_id = pci_get_subdevice(dev);
	pci_rev_id = pci_get_revid(dev);
	if (sctx->isc_parse_devinfo != NULL)
		sctx->isc_parse_devinfo(&pci_device_id, &pci_subvendor_id, &pci_subdevice_id, &pci_rev_id);

	ent = sctx->isc_vendor_info;
	while (ent->pvi_vendor_id != 0) {
		if (pci_vendor_id != ent->pvi_vendor_id) {
			ent++;
			continue;
		}
		if ((pci_device_id == ent->pvi_device_id) &&
		    ((pci_subvendor_id == ent->pvi_subvendor_id) ||
		     (ent->pvi_subvendor_id == 0)) &&
		    ((pci_subdevice_id == ent->pvi_subdevice_id) ||
		     (ent->pvi_subdevice_id == 0)) &&
		    ((pci_rev_id == ent->pvi_rev_id) ||
		     (ent->pvi_rev_id == 0))) {

			device_set_desc_copy(dev, ent->pvi_name);
			/* this needs to be changed to zero if the bus probing code
			 * ever stops re-probing on best match because the sctx
			 * may have its values over written by register calls
			 * in subsequent probes
			 */
			return (BUS_PROBE_DEFAULT);
		}
		ent++;
	}
	return (ENXIO);
}

int
iflib_device_register(device_t dev, void *sc, if_shared_ctx_t sctx, if_ctx_t *ctxp)
{
	int err, rid, msix, msix_bar;
	if_ctx_t ctx;
	if_t ifp;
	if_softc_ctx_t scctx;


	ctx = malloc(sizeof(* ctx), M_IFLIB, M_WAITOK|M_ZERO);

	if (sc == NULL) {
		sc = malloc(sctx->isc_driver->size, M_IFLIB, M_WAITOK|M_ZERO);
		device_set_softc(dev, ctx);
	}

	ctx->ifc_sctx = sctx;
	ctx->ifc_dev = dev;
	ctx->ifc_txrx = *sctx->isc_txrx;
	ctx->ifc_softc = sc;

	if ((err = iflib_register(ctx)) != 0) {
		device_printf(dev, "iflib_register failed %d\n", err);
		return (err);
	}
	iflib_add_device_sysctl_pre(ctx);
	if ((err = IFDI_ATTACH_PRE(ctx)) != 0) {
		device_printf(dev, "IFDI_ATTACH_PRE failed %d\n", err);
		return (err);
	}
#ifdef ACPI_DMAR
	if (dmar_get_dma_tag(device_get_parent(dev), dev) != NULL)
		ctx->ifc_flags |= IFC_DMAR;
#endif

	scctx = &ctx->ifc_softc_ctx;
	msix_bar = scctx->isc_msix_bar;

	if (scctx->isc_tx_nsegments > sctx->isc_ntxd / MAX_SINGLE_PACKET_FRACTION)
		scctx->isc_tx_nsegments = max(1, sctx->isc_ntxd / MAX_SINGLE_PACKET_FRACTION);
	if (scctx->isc_tx_tso_segments_max > sctx->isc_ntxd / MAX_SINGLE_PACKET_FRACTION)
		scctx->isc_tx_tso_segments_max = max(1, sctx->isc_ntxd / MAX_SINGLE_PACKET_FRACTION);

	ifp = ctx->ifc_ifp;

	/*
	 * XXX sanity check that ntxd & nrxd are a power of 2
	 */

	/*
	 * Protect the stack against modern hardware
	 */
	if (scctx->isc_tx_tso_size_max > FREEBSD_TSO_SIZE_MAX)
		scctx->isc_tx_tso_size_max = FREEBSD_TSO_SIZE_MAX;

	/* TSO parameters - dig these out of the data sheet - simply correspond to tag setup */
	ifp->if_hw_tsomaxsegcount = scctx->isc_tx_tso_segments_max;
	ifp->if_hw_tsomax = scctx->isc_tx_tso_size_max;
	ifp->if_hw_tsomaxsegsize = scctx->isc_tx_tso_segsize_max;
	if (scctx->isc_rss_table_size == 0)
		scctx->isc_rss_table_size = 64;
	scctx->isc_rss_table_mask = scctx->isc_rss_table_size-1;;
	/*
	** Now setup MSI or MSI/X, should
	** return us the number of supported
	** vectors. (Will be 1 for MSI)
	*/
	if (sctx->isc_flags & IFLIB_SKIP_MSIX) {
		msix = scctx->isc_vectors;
	} else if (scctx->isc_msix_bar != 0)
		msix = iflib_msix_init(ctx);
	else {
		scctx->isc_vectors = 1;
		scctx->isc_ntxqsets = 1;
		scctx->isc_nrxqsets = 1;
		scctx->isc_intr = IFLIB_INTR_LEGACY;
		msix = 0;
	}
	/* Get memory for the station queues */
	if ((err = iflib_queues_alloc(ctx))) {
		device_printf(dev, "Unable to allocate queue memory\n");
		goto fail;
	}

	if ((err = iflib_qset_structures_setup(ctx))) {
		device_printf(dev, "qset structure setup failed %d\n", err);
		goto fail_queues;
	}

	if (msix > 1 && (err = IFDI_MSIX_INTR_ASSIGN(ctx, msix)) != 0) {
		device_printf(dev, "IFDI_MSIX_INTR_ASSIGN failed %d\n", err);
		goto fail_intr_free;
	}
	if (msix <= 1) {
		rid = 0;
		if (scctx->isc_intr == IFLIB_INTR_MSI) {
			MPASS(msix == 1);
			rid = 1;
		}
		if ((err = iflib_legacy_setup(ctx, ctx->isc_legacy_intr, ctx, &rid, "irq0")) != 0) {
			device_printf(dev, "iflib_legacy_setup failed %d\n", err);
			goto fail_intr_free;
		}
	}
	ether_ifattach(ctx->ifc_ifp, ctx->ifc_mac);
	if ((err = IFDI_ATTACH_POST(ctx)) != 0) {
		device_printf(dev, "IFDI_ATTACH_POST failed %d\n", err);
		goto fail_detach;
	}
	if ((err = iflib_netmap_attach(ctx))) {
		device_printf(ctx->ifc_dev, "netmap attach failed: %d\n", err);
		goto fail_detach;
	}
	*ctxp = ctx;

	iflib_add_device_sysctl_post(ctx);
	return (0);
fail_detach:
	ether_ifdetach(ctx->ifc_ifp);
fail_intr_free:
	if (scctx->isc_intr == IFLIB_INTR_MSIX || scctx->isc_intr == IFLIB_INTR_MSI)
		pci_release_msi(ctx->ifc_dev);
fail_queues:
	/* XXX free queues */
fail:
	IFDI_DETACH(ctx);
	return (err);
}

int
iflib_device_attach(device_t dev)
{
	if_ctx_t ctx;
	if_shared_ctx_t sctx;

	if ((sctx = DEVICE_REGISTER(dev)) == NULL || sctx->isc_magic != IFLIB_MAGIC)
		return (ENOTSUP);

	pci_enable_busmaster(dev);

	return (iflib_device_register(dev, NULL, sctx, &ctx));
}

int
iflib_device_deregister(if_ctx_t ctx)
{
	if_t ifp = ctx->ifc_ifp;
	iflib_txq_t txq;
	iflib_rxq_t rxq;
	device_t dev = ctx->ifc_dev;
	int i;
	struct taskqgroup *tqg;

	/* Make sure VLANS are not using driver */
	if (if_vlantrunkinuse(ifp)) {
		device_printf(dev,"Vlan in use, detach first\n");
		return (EBUSY);
	}

	CTX_LOCK(ctx);
	ctx->ifc_in_detach = 1;
	iflib_stop(ctx);
	CTX_UNLOCK(ctx);

	/* Unregister VLAN events */
	if (ctx->ifc_vlan_attach_event != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, ctx->ifc_vlan_attach_event);
	if (ctx->ifc_vlan_detach_event != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, ctx->ifc_vlan_detach_event);

	iflib_netmap_detach(ifp);
	ether_ifdetach(ifp);
	/* ether_ifdetach calls if_qflush - lock must be destroy afterwards*/
	CTX_LOCK_DESTROY(ctx);
	if (ctx->ifc_led_dev != NULL)
		led_destroy(ctx->ifc_led_dev);
	/* XXX drain any dependent tasks */
	tqg = qgroup_if_io_tqg;
	for (txq = ctx->ifc_txqs, i = 0, rxq = ctx->ifc_rxqs; i < NTXQSETS(ctx); i++, txq++) {
		callout_drain(&txq->ift_timer);
		callout_drain(&txq->ift_db_check);
		if (txq->ift_task.gt_uniq != NULL)
			taskqgroup_detach(tqg, &txq->ift_task);
	}
	for (i = 0, rxq = ctx->ifc_rxqs; i < NRXQSETS(ctx); i++, rxq++) {
		if (rxq->ifr_task.gt_uniq != NULL)
			taskqgroup_detach(tqg, &rxq->ifr_task);
	}
	tqg = qgroup_if_config_tqg;
	if (ctx->ifc_admin_task.gt_uniq != NULL)
		taskqgroup_detach(tqg, &ctx->ifc_admin_task);
	if (ctx->ifc_vflr_task.gt_uniq != NULL)
		taskqgroup_detach(tqg, &ctx->ifc_vflr_task);

	IFDI_DETACH(ctx);
	if (ctx->ifc_softc_ctx.isc_intr != IFLIB_INTR_LEGACY) {
		pci_release_msi(dev);
	}
	if (ctx->ifc_softc_ctx.isc_intr != IFLIB_INTR_MSIX) {
		iflib_irq_free(ctx, &ctx->ifc_legacy_irq);
	}
	if (ctx->ifc_msix_mem != NULL) {
		bus_release_resource(ctx->ifc_dev, SYS_RES_MEMORY,
			ctx->ifc_softc_ctx.isc_msix_bar, ctx->ifc_msix_mem);
		ctx->ifc_msix_mem = NULL;
	}

	bus_generic_detach(dev);
	if_free(ifp);

	iflib_tx_structures_free(ctx);
	iflib_rx_structures_free(ctx);
	return (0);
}


int
iflib_device_detach(device_t dev)
{
	if_ctx_t ctx = device_get_softc(dev);

	return (iflib_device_deregister(ctx));
}

int
iflib_device_suspend(device_t dev)
{
	if_ctx_t ctx = device_get_softc(dev);

	CTX_LOCK(ctx);
	IFDI_SUSPEND(ctx);
	CTX_UNLOCK(ctx);

	return bus_generic_suspend(dev);
}
int
iflib_device_shutdown(device_t dev)
{
	if_ctx_t ctx = device_get_softc(dev);

	CTX_LOCK(ctx);
	IFDI_SHUTDOWN(ctx);
	CTX_UNLOCK(ctx);

	return bus_generic_suspend(dev);
}


int
iflib_device_resume(device_t dev)
{
	if_ctx_t ctx = device_get_softc(dev);
	iflib_txq_t txq = ctx->ifc_txqs;

	CTX_LOCK(ctx);
	IFDI_RESUME(ctx);
	iflib_init_locked(ctx);
	CTX_UNLOCK(ctx);
	for (int i = 0; i < NTXQSETS(ctx); i++, txq++)
		iflib_txq_check_drain(txq, IFLIB_RESTART_BUDGET);

	return (bus_generic_resume(dev));
}

int
iflib_device_iov_init(device_t dev, uint16_t num_vfs, const nvlist_t *params)
{
	int error;
	if_ctx_t ctx = device_get_softc(dev);

	CTX_LOCK(ctx);
	error = IFDI_IOV_INIT(ctx, num_vfs, params);
	CTX_UNLOCK(ctx);

	return (error);
}

void
iflib_device_iov_uninit(device_t dev)
{
	if_ctx_t ctx = device_get_softc(dev);

	CTX_LOCK(ctx);
	IFDI_IOV_UNINIT(ctx);
	CTX_UNLOCK(ctx);
}

int
iflib_device_iov_add_vf(device_t dev, uint16_t vfnum, const nvlist_t *params)
{
	int error;
	if_ctx_t ctx = device_get_softc(dev);

	CTX_LOCK(ctx);
	error = IFDI_IOV_VF_ADD(ctx, vfnum, params);
	CTX_UNLOCK(ctx);

	return (error);
}

/*********************************************************************
 *
 *  MODULE FUNCTION DEFINITIONS
 *
 **********************************************************************/

/*
 * - Start a fast taskqueue thread for each core
 * - Start a taskqueue for control operations
 */
static int
iflib_module_init(void)
{
	return (0);
}

static int
iflib_module_event_handler(module_t mod, int what, void *arg)
{
	int err;

	switch (what) {
	case MOD_LOAD:
		if ((err = iflib_module_init()) != 0)
			return (err);
		break;
	case MOD_UNLOAD:
		return (EBUSY);
	default:
		return (EOPNOTSUPP);
	}

	return (0);
}

/*********************************************************************
 *
 *  PUBLIC FUNCTION DEFINITIONS
 *     ordered as in iflib.h
 *
 **********************************************************************/


static void
_iflib_assert(if_shared_ctx_t sctx)
{
	MPASS(sctx->isc_tx_maxsize);
	MPASS(sctx->isc_tx_maxsegsize);

	MPASS(sctx->isc_rx_maxsize);
	MPASS(sctx->isc_rx_nsegments);
	MPASS(sctx->isc_rx_maxsegsize);


	MPASS(sctx->isc_txrx->ift_txd_encap);
	MPASS(sctx->isc_txrx->ift_txd_flush);
	MPASS(sctx->isc_txrx->ift_txd_credits_update);
	MPASS(sctx->isc_txrx->ift_rxd_available);
	MPASS(sctx->isc_txrx->ift_rxd_pkt_get);
	MPASS(sctx->isc_txrx->ift_rxd_refill);
	MPASS(sctx->isc_txrx->ift_rxd_flush);
	MPASS(sctx->isc_nrxd);
}

static int
iflib_register(if_ctx_t ctx)
{
	if_shared_ctx_t sctx = ctx->ifc_sctx;
	driver_t *driver = sctx->isc_driver;
	device_t dev = ctx->ifc_dev;
	if_t ifp;

	_iflib_assert(sctx);

	CTX_LOCK_INIT(ctx, device_get_nameunit(ctx->ifc_dev));
	MPASS(ctx->ifc_flags == 0);

	ifp = ctx->ifc_ifp = if_gethandle(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not allocate ifnet structure\n");
		return (ENOMEM);
	}

	/*
	 * Initialize our context's device specific methods
	 */
	kobj_init((kobj_t) ctx, (kobj_class_t) driver);
	kobj_class_compile((kobj_class_t) driver);
	driver->refs++;

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	if_setsoftc(ifp, ctx);
	if_setdev(ifp, dev);
	if_setinitfn(ifp, iflib_if_init);
	if_setioctlfn(ifp, iflib_if_ioctl);
	if_settransmitfn(ifp, iflib_if_transmit);
	if_setqflushfn(ifp, iflib_if_qflush);
	if_setgetcounterfn(ifp, iflib_if_get_counter);
	if_setflags(ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);

	if_setcapabilities(ifp, 0);
	if_setcapenable(ifp, 0);

	ctx->ifc_vlan_attach_event =
		EVENTHANDLER_REGISTER(vlan_config, iflib_vlan_register, ctx,
							  EVENTHANDLER_PRI_FIRST);
	ctx->ifc_vlan_detach_event =
		EVENTHANDLER_REGISTER(vlan_unconfig, iflib_vlan_unregister, ctx,
							  EVENTHANDLER_PRI_FIRST);

	ifmedia_init(&ctx->ifc_media, IFM_IMASK,
					 iflib_media_change, iflib_media_status);

	return (0);
}


static int
iflib_queues_alloc(if_ctx_t ctx)
{
	if_shared_ctx_t sctx = ctx->ifc_sctx;
	device_t dev = ctx->ifc_dev;
	int nrxqsets = ctx->ifc_softc_ctx.isc_nrxqsets;
	int ntxqsets = ctx->ifc_softc_ctx.isc_ntxqsets;
	iflib_txq_t txq;
	iflib_rxq_t rxq;
	iflib_fl_t fl = NULL;
	int i, j, cpu, err, txconf, rxconf, fl_ifdi_offset;
	iflib_dma_info_t ifdip;
	uint32_t *rxqsizes = sctx->isc_rxqsizes;
	uint32_t *txqsizes = sctx->isc_txqsizes;
	uint8_t nrxqs = sctx->isc_nrxqs;
	uint8_t ntxqs = sctx->isc_ntxqs;
	int nfree_lists = sctx->isc_nfl ? sctx->isc_nfl : 1;
	caddr_t *vaddrs;
	uint64_t *paddrs;
	struct ifmp_ring **brscp;
	int nbuf_rings = 1; /* XXX determine dynamically */

	KASSERT(ntxqs > 0, ("number of queues must be at least 1"));
	KASSERT(nrxqs > 0, ("number of queues must be at least 1"));

	brscp = NULL;
	rxq = NULL;

/* Allocate the TX ring struct memory */
	if (!(txq =
	    (iflib_txq_t) malloc(sizeof(struct iflib_txq) *
	    ntxqsets, M_IFLIB, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate TX ring memory\n");
		err = ENOMEM;
		goto fail;
	}

	/* Now allocate the RX */
	if (!(rxq =
	    (iflib_rxq_t) malloc(sizeof(struct iflib_rxq) *
	    nrxqsets, M_IFLIB, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate RX ring memory\n");
		err = ENOMEM;
		goto rx_fail;
	}
	if (!(brscp = malloc(sizeof(void *) * nbuf_rings * nrxqsets, M_IFLIB, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to buf_ring_sc * memory\n");
		err = ENOMEM;
		goto rx_fail;
	}

	ctx->ifc_txqs = txq;
	ctx->ifc_rxqs = rxq;
	txq = NULL;
	rxq = NULL;

	/*
	 * XXX handle allocation failure
	 */
	for (txconf = i = 0, cpu = CPU_FIRST(); i < ntxqsets; i++, txconf++, txq++, cpu = CPU_NEXT(cpu)) {
		/* Set up some basics */

		if ((ifdip = malloc(sizeof(struct iflib_dma_info) * ntxqs, M_IFLIB, M_WAITOK|M_ZERO)) == NULL) {
			device_printf(dev, "failed to allocate iflib_dma_info\n");
			err = ENOMEM;
			goto err_tx_desc;
		}
		txq->ift_ifdi = ifdip;
		for (j = 0; j < ntxqs; j++, ifdip++) {
			if (iflib_dma_alloc(ctx, txqsizes[j], ifdip, BUS_DMA_NOWAIT)) {
				device_printf(dev, "Unable to allocate Descriptor memory\n");
				err = ENOMEM;
				goto err_tx_desc;
			}
			bzero((void *)ifdip->idi_vaddr, txqsizes[j]);
		}
		txq->ift_ctx = ctx;
		txq->ift_id = i;
		/* XXX fix this */
		txq->ift_timer.c_cpu = cpu;
		txq->ift_db_check.c_cpu = cpu;
		txq->ift_nbr = nbuf_rings;

		if (iflib_txsd_alloc(txq)) {
			device_printf(dev, "Critical Failure setting up TX buffers\n");
			err = ENOMEM;
			goto err_tx_desc;
		}

		/* Initialize the TX lock */
		snprintf(txq->ift_mtx_name, MTX_NAME_LEN, "%s:tx(%d):callout",
		    device_get_nameunit(dev), txq->ift_id);
		mtx_init(&txq->ift_mtx, txq->ift_mtx_name, NULL, MTX_DEF);
		callout_init_mtx(&txq->ift_timer, &txq->ift_mtx, 0);
		callout_init_mtx(&txq->ift_db_check, &txq->ift_mtx, 0);

		snprintf(txq->ift_db_mtx_name, MTX_NAME_LEN, "%s:tx(%d):db",
			 device_get_nameunit(dev), txq->ift_id);
		TXDB_LOCK_INIT(txq);

		txq->ift_br = brscp + i*nbuf_rings;
		for (j = 0; j < nbuf_rings; j++) {
			err = ifmp_ring_alloc(&txq->ift_br[j], 2048, txq, iflib_txq_drain,
					      iflib_txq_can_drain, M_IFLIB, M_WAITOK);
			if (err) {
				/* XXX free any allocated rings */
				device_printf(dev, "Unable to allocate buf_ring\n");
				goto err_tx_desc;
			}
		}
	}

	for (rxconf = i = 0; i < nrxqsets; i++, rxconf++, rxq++) {
		/* Set up some basics */

		if ((ifdip = malloc(sizeof(struct iflib_dma_info) * nrxqs, M_IFLIB, M_WAITOK|M_ZERO)) == NULL) {
			device_printf(dev, "failed to allocate iflib_dma_info\n");
			err = ENOMEM;
			goto err_tx_desc;
		}

		rxq->ifr_ifdi = ifdip;
		for (j = 0; j < nrxqs; j++, ifdip++) {
			if (iflib_dma_alloc(ctx, rxqsizes[j], ifdip, BUS_DMA_NOWAIT)) {
				device_printf(dev, "Unable to allocate Descriptor memory\n");
				err = ENOMEM;
				goto err_tx_desc;
			}
			bzero((void *)ifdip->idi_vaddr, rxqsizes[j]);
		}
		rxq->ifr_ctx = ctx;
		rxq->ifr_id = i;
		if (sctx->isc_flags & IFLIB_HAS_CQ) {
			fl_ifdi_offset = 1;
		} else {
			fl_ifdi_offset = 0;
		}
		rxq->ifr_nfl = nfree_lists;
		if (!(fl =
			  (iflib_fl_t) malloc(sizeof(struct iflib_fl) * nfree_lists, M_IFLIB, M_NOWAIT | M_ZERO))) {
			device_printf(dev, "Unable to allocate free list memory\n");
			err = ENOMEM;
			goto err_tx_desc;
		}
		rxq->ifr_fl = fl;
		for (j = 0; j < nfree_lists; j++) {
			rxq->ifr_fl[j].ifl_rxq = rxq;
			rxq->ifr_fl[j].ifl_id = j;
			rxq->ifr_fl[j].ifl_ifdi = &rxq->ifr_ifdi[j + fl_ifdi_offset];
		}
        /* Allocate receive buffers for the ring*/
		if (iflib_rxsd_alloc(rxq)) {
			device_printf(dev,
			    "Critical Failure setting up receive buffers\n");
			err = ENOMEM;
			goto err_rx_desc;
		}
	}

	/* TXQs */
	vaddrs = malloc(sizeof(caddr_t)*ntxqsets*ntxqs, M_IFLIB, M_WAITOK);
	paddrs = malloc(sizeof(uint64_t)*ntxqsets*ntxqs, M_IFLIB, M_WAITOK);
	for (i = 0; i < ntxqsets; i++) {
		iflib_dma_info_t di = ctx->ifc_txqs[i].ift_ifdi;

		for (j = 0; j < ntxqs; j++, di++) {
			vaddrs[i*ntxqs + j] = di->idi_vaddr;
			paddrs[i*ntxqs + j] = di->idi_paddr;
		}
	}
	if ((err = IFDI_TX_QUEUES_ALLOC(ctx, vaddrs, paddrs, ntxqs, ntxqsets)) != 0) {
		device_printf(ctx->ifc_dev, "device queue allocation failed\n");
		iflib_tx_structures_free(ctx);
		free(vaddrs, M_IFLIB);
		free(paddrs, M_IFLIB);
		goto err_rx_desc;
	}
	free(vaddrs, M_IFLIB);
	free(paddrs, M_IFLIB);

	/* RXQs */
	vaddrs = malloc(sizeof(caddr_t)*nrxqsets*nrxqs, M_IFLIB, M_WAITOK);
	paddrs = malloc(sizeof(uint64_t)*nrxqsets*nrxqs, M_IFLIB, M_WAITOK);
	for (i = 0; i < nrxqsets; i++) {
		iflib_dma_info_t di = ctx->ifc_rxqs[i].ifr_ifdi;

		for (j = 0; j < nrxqs; j++, di++) {
			vaddrs[i*nrxqs + j] = di->idi_vaddr;
			paddrs[i*nrxqs + j] = di->idi_paddr;
		}
	}
	if ((err = IFDI_RX_QUEUES_ALLOC(ctx, vaddrs, paddrs, nrxqs, nrxqsets)) != 0) {
		device_printf(ctx->ifc_dev, "device queue allocation failed\n");
		iflib_tx_structures_free(ctx);
		free(vaddrs, M_IFLIB);
		free(paddrs, M_IFLIB);
		goto err_rx_desc;
	}
	free(vaddrs, M_IFLIB);
	free(paddrs, M_IFLIB);

	return (0);

/* XXX handle allocation failure changes */
err_rx_desc:
err_tx_desc:
	if (ctx->ifc_rxqs != NULL)
		free(ctx->ifc_rxqs, M_IFLIB);
	ctx->ifc_rxqs = NULL;
	if (ctx->ifc_txqs != NULL)
		free(ctx->ifc_txqs, M_IFLIB);
	ctx->ifc_txqs = NULL;
rx_fail:
	if (brscp != NULL)
		free(brscp, M_IFLIB);
	if (rxq != NULL)
		free(rxq, M_IFLIB);
	if (txq != NULL)
		free(txq, M_IFLIB);
fail:
	return (err);
}

static int
iflib_tx_structures_setup(if_ctx_t ctx)
{
	iflib_txq_t txq = ctx->ifc_txqs;
	int i;

	for (i = 0; i < NTXQSETS(ctx); i++, txq++)
		iflib_txq_setup(txq);

	return (0);
}

static void
iflib_tx_structures_free(if_ctx_t ctx)
{
	iflib_txq_t txq = ctx->ifc_txqs;
	int i, j;

	for (i = 0; i < NTXQSETS(ctx); i++, txq++) {
		iflib_txq_destroy(txq);
		for (j = 0; j < ctx->ifc_nhwtxqs; j++)
			iflib_dma_free(&txq->ift_ifdi[j]);
	}
	free(ctx->ifc_txqs, M_IFLIB);
	ctx->ifc_txqs = NULL;
	IFDI_QUEUES_FREE(ctx);
}

/*********************************************************************
 *
 *  Initialize all receive rings.
 *
 **********************************************************************/
static int
iflib_rx_structures_setup(if_ctx_t ctx)
{
	iflib_rxq_t rxq = ctx->ifc_rxqs;
	int q;
#if defined(INET6) || defined(INET)
	int i, err;
#endif

	for (q = 0; q < ctx->ifc_softc_ctx.isc_nrxqsets; q++, rxq++) {
#if defined(INET6) || defined(INET)
		tcp_lro_free(&rxq->ifr_lc);
		if ((err = tcp_lro_init(&rxq->ifr_lc)) != 0) {
			device_printf(ctx->ifc_dev, "LRO Initialization failed!\n");
			goto fail;
		}
		rxq->ifr_lro_enabled = TRUE;
		rxq->ifr_lc.ifp = ctx->ifc_ifp;
#endif
		IFDI_RXQ_SETUP(ctx, rxq->ifr_id);
	}
	return (0);
#if defined(INET6) || defined(INET)
fail:
	/*
	 * Free RX software descriptors allocated so far, we will only handle
	 * the rings that completed, the failing case will have
	 * cleaned up for itself. 'q' failed, so its the terminus.
	 */
	rxq = ctx->ifc_rxqs;
	for (i = 0; i < q; ++i, rxq++) {
		iflib_rx_sds_free(rxq);
		rxq->ifr_cq_gen = rxq->ifr_cq_cidx = rxq->ifr_cq_pidx = 0;
	}
	return (err);
#endif
}

/*********************************************************************
 *
 *  Free all receive rings.
 *
 **********************************************************************/
static void
iflib_rx_structures_free(if_ctx_t ctx)
{
	iflib_rxq_t rxq = ctx->ifc_rxqs;

	for (int i = 0; i < ctx->ifc_softc_ctx.isc_ntxqsets; i++, rxq++) {
		iflib_rx_sds_free(rxq);
	}
}

static int
iflib_qset_structures_setup(if_ctx_t ctx)
{
	int err;

	if ((err = iflib_tx_structures_setup(ctx)) != 0)
		return (err);

	if ((err = iflib_rx_structures_setup(ctx)) != 0) {
		device_printf(ctx->ifc_dev, "iflib_rx_structures_setup failed: %d\n", err);
		iflib_tx_structures_free(ctx);
		iflib_rx_structures_free(ctx);
	}
	return (err);
}

int
iflib_irq_alloc(if_ctx_t ctx, if_irq_t irq, int rid,
				driver_filter_t filter, void *filter_arg, driver_intr_t handler, void *arg, char *name)
{

	return (_iflib_irq_alloc(ctx, irq, rid, filter, handler, arg, name));
}

static void
find_nth(if_ctx_t ctx, cpuset_t *cpus, int qid)
{
	int i, cpuid;

	CPU_COPY(&ctx->ifc_cpus, cpus);
	/* clear up to the qid'th bit */
	for (i = 0; i < qid; i++) {
		cpuid = CPU_FFS(cpus);
		CPU_CLR(cpuid, cpus);
	}
}

int
iflib_irq_alloc_generic(if_ctx_t ctx, if_irq_t irq, int rid,
						iflib_intr_type_t type, driver_filter_t *filter,
						void *filter_arg, int qid, char *name)
{
	struct grouptask *gtask;
	struct taskqgroup *tqg;
	iflib_filter_info_t info;
	cpuset_t cpus;
	task_fn_t *fn;
	int tqrid, err;
	void *q;

	info = &ctx->ifc_filter_info;

	switch (type) {
	/* XXX merge tx/rx for netmap? */
	case IFLIB_INTR_TX:
		q = &ctx->ifc_txqs[qid];
		info = &ctx->ifc_txqs[qid].ift_filter_info;
		gtask = &ctx->ifc_txqs[qid].ift_task;
		tqg = qgroup_if_io_tqg;
		tqrid = irq->ii_rid;
		fn = _task_fn_tx;
		break;
	case IFLIB_INTR_RX:
		q = &ctx->ifc_rxqs[qid];
		info = &ctx->ifc_rxqs[qid].ifr_filter_info;
		gtask = &ctx->ifc_rxqs[qid].ifr_task;
		tqg = qgroup_if_io_tqg;
		tqrid = irq->ii_rid;
		fn = _task_fn_rx;
		break;
	case IFLIB_INTR_ADMIN:
		q = ctx;
		info = &ctx->ifc_filter_info;
		gtask = &ctx->ifc_admin_task;
		tqg = qgroup_if_config_tqg;
		tqrid = -1;
		fn = _task_fn_admin;
		break;
	default:
		panic("unknown net intr type");
	}
	GROUPTASK_INIT(gtask, 0, fn, q);

	info->ifi_filter = filter;
	info->ifi_filter_arg = filter_arg;
	info->ifi_task = gtask;

	/* XXX query cpu that rid belongs to */

	err = _iflib_irq_alloc(ctx, irq, rid, iflib_fast_intr, NULL, info,  name);
	if (err != 0)
		return (err);
	if (tqrid != -1) {
		find_nth(ctx, &cpus, qid);
		taskqgroup_attach_cpu(tqg, gtask, q, CPU_FFS(&cpus), irq->ii_rid, name);
	} else
		taskqgroup_attach(tqg, gtask, q, tqrid, name);


	return (0);
}

void
iflib_softirq_alloc_generic(if_ctx_t ctx, int rid, iflib_intr_type_t type,  void *arg, int qid, char *name)
{
	struct grouptask *gtask;
	struct taskqgroup *tqg;
	task_fn_t *fn;
	void *q;

	switch (type) {
	case IFLIB_INTR_TX:
		q = &ctx->ifc_txqs[qid];
		gtask = &ctx->ifc_txqs[qid].ift_task;
		tqg = qgroup_if_io_tqg;
		fn = _task_fn_tx;
		break;
	case IFLIB_INTR_RX:
		q = &ctx->ifc_rxqs[qid];
		gtask = &ctx->ifc_rxqs[qid].ifr_task;
		tqg = qgroup_if_io_tqg;
		fn = _task_fn_rx;
		break;
	case IFLIB_INTR_ADMIN:
		q = ctx;
		gtask = &ctx->ifc_admin_task;
		tqg = qgroup_if_config_tqg;
		rid = -1;
		fn = _task_fn_admin;
		break;
	case IFLIB_INTR_IOV:
		q = ctx;
		gtask = &ctx->ifc_vflr_task;
		tqg = qgroup_if_config_tqg;
		rid = -1;
		fn = _task_fn_iov;
		break;
	default:
		panic("unknown net intr type");
	}
	GROUPTASK_INIT(gtask, 0, fn, q);
	taskqgroup_attach(tqg, gtask, q, rid, name);
}

void
iflib_irq_free(if_ctx_t ctx, if_irq_t irq)
{
	if (irq->ii_tag)
		bus_teardown_intr(ctx->ifc_dev, irq->ii_res, irq->ii_tag);

	if (irq->ii_res)
		bus_release_resource(ctx->ifc_dev, SYS_RES_IRQ, irq->ii_rid, irq->ii_res);
}

static int
iflib_legacy_setup(if_ctx_t ctx, driver_filter_t filter, void *filter_arg, int *rid, char *name)
{
	iflib_txq_t txq = ctx->ifc_txqs;
	iflib_rxq_t rxq = ctx->ifc_rxqs;
	if_irq_t irq = &ctx->ifc_legacy_irq;
	iflib_filter_info_t info;
	struct grouptask *gtask;
	struct taskqgroup *tqg;
	task_fn_t *fn;
	int tqrid;
	void *q;
	int err;

	q = &ctx->ifc_rxqs[0];
	info = &rxq[0].ifr_filter_info;
	gtask = &rxq[0].ifr_task;
	tqg = qgroup_if_io_tqg;
	tqrid = irq->ii_rid = *rid;
	fn = _task_fn_rx;

	ctx->ifc_flags |= IFC_LEGACY;
	info->ifi_filter = filter;
	info->ifi_filter_arg = filter_arg;
	info->ifi_task = gtask;

	/* We allocate a single interrupt resource */
	if ((err = _iflib_irq_alloc(ctx, irq, tqrid, iflib_fast_intr, NULL, info, name)) != 0)
		return (err);
	GROUPTASK_INIT(gtask, 0, fn, q);
	taskqgroup_attach(tqg, gtask, q, tqrid, name);

	GROUPTASK_INIT(&txq->ift_task, 0, _task_fn_tx, txq);
	taskqgroup_attach(qgroup_if_io_tqg, &txq->ift_task, txq, tqrid, "tx");
	GROUPTASK_INIT(&ctx->ifc_admin_task, 0, _task_fn_admin, ctx);
	taskqgroup_attach(qgroup_if_config_tqg, &ctx->ifc_admin_task, ctx, -1, "admin/link");

	return (0);
}

void
iflib_led_create(if_ctx_t ctx)
{

	ctx->ifc_led_dev = led_create(iflib_led_func, ctx,
								  device_get_nameunit(ctx->ifc_dev));
}

void
iflib_tx_intr_deferred(if_ctx_t ctx, int txqid)
{

	GROUPTASK_ENQUEUE(&ctx->ifc_txqs[txqid].ift_task);
}

void
iflib_rx_intr_deferred(if_ctx_t ctx, int rxqid)
{

	GROUPTASK_ENQUEUE(&ctx->ifc_rxqs[rxqid].ifr_task);
}

void
iflib_admin_intr_deferred(if_ctx_t ctx)
{

	GROUPTASK_ENQUEUE(&ctx->ifc_admin_task);
}

void
iflib_iov_intr_deferred(if_ctx_t ctx)
{

	GROUPTASK_ENQUEUE(&ctx->ifc_vflr_task);
}

void
iflib_io_tqg_attach(struct grouptask *gt, void *uniq, int cpu, char *name)
{

	taskqgroup_attach_cpu(qgroup_if_io_tqg, gt, uniq, cpu, -1, name);
}

void
iflib_config_gtask_init(if_ctx_t ctx, struct grouptask *gtask, task_fn_t *fn,
	char *name)
{

	GROUPTASK_INIT(gtask, 0, fn, ctx);
	taskqgroup_attach(qgroup_if_config_tqg, gtask, gtask, -1, name);
}

void
iflib_link_state_change(if_ctx_t ctx, int link_state)
{
	if_t ifp = ctx->ifc_ifp;
	iflib_txq_t txq = ctx->ifc_txqs;

#if 0
	if_setbaudrate(ifp, baudrate);
#endif
	/* If link down, disable watchdog */
	if ((ctx->ifc_link_state == LINK_STATE_UP) && (link_state == LINK_STATE_DOWN)) {
		for (int i = 0; i < ctx->ifc_softc_ctx.isc_ntxqsets; i++, txq++)
			txq->ift_qstatus = IFLIB_QUEUE_IDLE;
	}
	ctx->ifc_link_state = link_state;
	if_link_state_change(ifp, link_state);
}

static int
iflib_tx_credits_update(if_ctx_t ctx, iflib_txq_t txq)
{
	int credits;

	if (ctx->isc_txd_credits_update == NULL)
		return (0);

	if ((credits = ctx->isc_txd_credits_update(ctx->ifc_softc, txq->ift_id, txq->ift_cidx_processed, true)) == 0)
		return (0);

	txq->ift_processed += credits;
	txq->ift_cidx_processed += credits;

	if (txq->ift_cidx_processed >= txq->ift_size)
		txq->ift_cidx_processed -= txq->ift_size;
	return (credits);
}

static int
iflib_rxd_avail(if_ctx_t ctx, iflib_rxq_t rxq, int cidx)
{

	return (ctx->isc_rxd_available(ctx->ifc_softc, rxq->ifr_id, cidx));
}

void
iflib_add_int_delay_sysctl(if_ctx_t ctx, const char *name,
	const char *description, if_int_delay_info_t info,
	int offset, int value)
{
	info->iidi_ctx = ctx;
	info->iidi_offset = offset;
	info->iidi_value = value;
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(ctx->ifc_dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(ctx->ifc_dev)),
	    OID_AUTO, name, CTLTYPE_INT|CTLFLAG_RW,
	    info, 0, iflib_sysctl_int_delay, "I", description);
}

struct mtx *
iflib_ctx_lock_get(if_ctx_t ctx)
{

	return (&ctx->ifc_mtx);
}

static int
iflib_msix_init(if_ctx_t ctx)
{
	device_t dev = ctx->ifc_dev;
	if_shared_ctx_t sctx = ctx->ifc_sctx;
	if_softc_ctx_t scctx = &ctx->ifc_softc_ctx;
	int vectors, queues, rx_queues, tx_queues, queuemsgs, msgs;
	int iflib_num_tx_queues, iflib_num_rx_queues;
	int err, admincnt, bar;

	iflib_num_tx_queues = ctx->ifc_sysctl_ntxqs;
	iflib_num_rx_queues = ctx->ifc_sysctl_nrxqs;
	bar = ctx->ifc_softc_ctx.isc_msix_bar;
	admincnt = sctx->isc_admin_intrcnt;
	/* Override by tuneable */
	if (enable_msix == 0)
		goto msi;

	/*
	** When used in a virtualized environment
	** PCI BUSMASTER capability may not be set
	** so explicity set it here and rewrite
	** the ENABLE in the MSIX control register
	** at this point to cause the host to
	** successfully initialize us.
	*/
	{
		uint16_t pci_cmd_word;
		int msix_ctrl, rid;

		rid = 0;
		pci_cmd_word = pci_read_config(dev, PCIR_COMMAND, 2);
		pci_cmd_word |= PCIM_CMD_BUSMASTEREN;
		pci_write_config(dev, PCIR_COMMAND, pci_cmd_word, 2);
		pci_find_cap(dev, PCIY_MSIX, &rid);
		rid += PCIR_MSIX_CTRL;
		msix_ctrl = pci_read_config(dev, rid, 2);
		msix_ctrl |= PCIM_MSIXCTRL_MSIX_ENABLE;
		pci_write_config(dev, rid, msix_ctrl, 2);
	}

	/*
	 * bar == -1 => "trust me I know what I'm doing"
	 * https://www.youtube.com/watch?v=nnwWKkNau4I
	 * Some drivers are for hardware that is so shoddily
	 * documented that no one knows which bars are which
	 * so the developer has to map all bars. This hack
	 * allows shoddy garbage to use msix in this framework.
	 */
	if (bar != -1) {
		ctx->ifc_msix_mem = bus_alloc_resource_any(dev,
	            SYS_RES_MEMORY, &bar, RF_ACTIVE);
		if (ctx->ifc_msix_mem == NULL) {
			/* May not be enabled */
			device_printf(dev, "Unable to map MSIX table \n");
			goto msi;
		}
	}
	/* First try MSI/X */
	if ((msgs = pci_msix_count(dev)) == 0) { /* system has msix disabled */
		device_printf(dev, "System has MSIX disabled \n");
		bus_release_resource(dev, SYS_RES_MEMORY,
		    bar, ctx->ifc_msix_mem);
		ctx->ifc_msix_mem = NULL;
		goto msi;
	}
#if IFLIB_DEBUG
	/* use only 1 qset in debug mode */
	queuemsgs = min(msgs - admincnt, 1);
#else
	queuemsgs = msgs - admincnt;
#endif
	if (bus_get_cpus(dev, INTR_CPUS, sizeof(ctx->ifc_cpus), &ctx->ifc_cpus) == 0) {
#ifdef RSS
		queues = imin(queuemsgs, rss_getnumbuckets());
#else
		queues = queuemsgs;
#endif
		queues = imin(CPU_COUNT(&ctx->ifc_cpus), queues);
		device_printf(dev, "pxm cpus: %d queue msgs: %d admincnt: %d\n",
					  CPU_COUNT(&ctx->ifc_cpus), queuemsgs, admincnt);
	} else {
		device_printf(dev, "Unable to fetch CPU list\n");
		/* Figure out a reasonable auto config value */
		queues = min(queuemsgs, mp_ncpus);
	}
#ifdef  RSS
	/* If we're doing RSS, clamp at the number of RSS buckets */
	if (queues > rss_getnumbuckets())
		queues = rss_getnumbuckets();
#endif
	if (iflib_num_rx_queues > 0 && iflib_num_rx_queues < queues)
		queues = rx_queues = iflib_num_rx_queues;
	else
		rx_queues = queues;
	if (iflib_num_tx_queues > 0 && iflib_num_tx_queues < queues)
		tx_queues = iflib_num_tx_queues;
	else
		tx_queues = queues;

	device_printf(dev, "using %d rx queues %d tx queues \n", rx_queues, tx_queues);

	vectors = queues + admincnt;
	if ((err = pci_alloc_msix(dev, &vectors)) == 0) {
		device_printf(dev,
					  "Using MSIX interrupts with %d vectors\n", vectors);
		scctx->isc_vectors = vectors;
		scctx->isc_nrxqsets = rx_queues;
		scctx->isc_ntxqsets = tx_queues;
		scctx->isc_intr = IFLIB_INTR_MSIX;
		return (vectors);
	} else {
		device_printf(dev, "failed to allocate %d msix vectors, err: %d - using MSI\n", vectors, err);
	}
msi:
	vectors = pci_msi_count(dev);
	scctx->isc_nrxqsets = 1;
	scctx->isc_ntxqsets = 1;
	scctx->isc_vectors = vectors;
	if (vectors == 1 && pci_alloc_msi(dev, &vectors) == 0) {
		device_printf(dev,"Using an MSI interrupt\n");
		scctx->isc_intr = IFLIB_INTR_MSI;
	} else {
		device_printf(dev,"Using a Legacy interrupt\n");
		scctx->isc_intr = IFLIB_INTR_LEGACY;
	}

	return (vectors);
}

char * ring_states[] = { "IDLE", "BUSY", "STALLED", "ABDICATED" };

static int
mp_ring_state_handler(SYSCTL_HANDLER_ARGS)
{
	int rc;
	uint16_t *state = ((uint16_t *)oidp->oid_arg1);
	struct sbuf *sb;
	char *ring_state = "UNKNOWN";

	/* XXX needed ? */
	rc = sysctl_wire_old_buffer(req, 0);
	MPASS(rc == 0);
	if (rc != 0)
		return (rc);
	sb = sbuf_new_for_sysctl(NULL, NULL, 80, req);
	MPASS(sb != NULL);
	if (sb == NULL)
		return (ENOMEM);
	if (state[3] <= 3)
		ring_state = ring_states[state[3]];

	sbuf_printf(sb, "pidx_head: %04hd pidx_tail: %04hd cidx: %04hd state: %s",
		    state[0], state[1], state[2], ring_state);
	rc = sbuf_finish(sb);
	sbuf_delete(sb);
        return(rc);
}



#define NAME_BUFLEN 32
static void
iflib_add_device_sysctl_pre(if_ctx_t ctx)
{
        device_t dev = iflib_get_dev(ctx);
	struct sysctl_oid_list *child, *oid_list;
	struct sysctl_ctx_list *ctx_list;
	struct sysctl_oid *node;

	ctx_list = device_get_sysctl_ctx(dev);
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));
	ctx->ifc_sysctl_node = node = SYSCTL_ADD_NODE(ctx_list, child, OID_AUTO, "iflib",
						      CTLFLAG_RD, NULL, "IFLIB fields");
	oid_list = SYSCTL_CHILDREN(node);

	SYSCTL_ADD_U16(ctx_list, oid_list, OID_AUTO, "override_ntxqs",
		       CTLFLAG_RWTUN, &ctx->ifc_sysctl_ntxqs, 0,
			"# of txqs to use, 0 => use default #");
	SYSCTL_ADD_U16(ctx_list, oid_list, OID_AUTO, "override_nrxqs",
		       CTLFLAG_RWTUN, &ctx->ifc_sysctl_ntxqs, 0,
			"# of txqs to use, 0 => use default #");
	SYSCTL_ADD_U16(ctx_list, oid_list, OID_AUTO, "override_ntxds",
		       CTLFLAG_RWTUN, &ctx->ifc_sysctl_ntxds, 0,
			"# of tx descriptors to use, 0 => use default #");
	SYSCTL_ADD_U16(ctx_list, oid_list, OID_AUTO, "override_nrxds",
		       CTLFLAG_RWTUN, &ctx->ifc_sysctl_nrxds, 0,
			"# of rx descriptors to use, 0 => use default #");

}

static void
iflib_add_device_sysctl_post(if_ctx_t ctx)
{
	if_shared_ctx_t sctx = ctx->ifc_sctx;
	if_softc_ctx_t scctx = &ctx->ifc_softc_ctx;
        device_t dev = iflib_get_dev(ctx);
	struct sysctl_oid_list *child;
	struct sysctl_ctx_list *ctx_list;
	iflib_fl_t fl;
	iflib_txq_t txq;
	iflib_rxq_t rxq;
	int i, j;
	char namebuf[NAME_BUFLEN];
	char *qfmt;
	struct sysctl_oid *queue_node, *fl_node, *node;
	struct sysctl_oid_list *queue_list, *fl_list;
	ctx_list = device_get_sysctl_ctx(dev);

	node = ctx->ifc_sysctl_node;
	child = SYSCTL_CHILDREN(node);

	if (scctx->isc_ntxqsets > 100)
		qfmt = "txq%03d";
	else if (scctx->isc_ntxqsets > 10)
		qfmt = "txq%02d";
	else
		qfmt = "txq%d";
	for (i = 0, txq = ctx->ifc_txqs; i < scctx->isc_ntxqsets; i++, txq++) {
		snprintf(namebuf, NAME_BUFLEN, qfmt, i);
		queue_node = SYSCTL_ADD_NODE(ctx_list, child, OID_AUTO, namebuf,
					     CTLFLAG_RD, NULL, "Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);
#if MEMORY_LOGGING
		SYSCTL_ADD_QUAD(ctx_list, queue_list, OID_AUTO, "txq_dequeued",
				CTLFLAG_RD,
				&txq->ift_dequeued, "total mbufs freed");
		SYSCTL_ADD_QUAD(ctx_list, queue_list, OID_AUTO, "txq_enqueued",
				CTLFLAG_RD,
				&txq->ift_enqueued, "total mbufs enqueued");
#endif
		SYSCTL_ADD_QUAD(ctx_list, queue_list, OID_AUTO, "mbuf_defrag",
				   CTLFLAG_RD,
				   &txq->ift_mbuf_defrag, "# of times m_defrag was called");
		SYSCTL_ADD_QUAD(ctx_list, queue_list, OID_AUTO, "m_pullups",
				   CTLFLAG_RD,
				   &txq->ift_pullups, "# of times m_pullup was called");
		SYSCTL_ADD_QUAD(ctx_list, queue_list, OID_AUTO, "mbuf_defrag_failed",
				   CTLFLAG_RD,
				   &txq->ift_mbuf_defrag_failed, "# of times m_defrag failed");
		SYSCTL_ADD_QUAD(ctx_list, queue_list, OID_AUTO, "no_desc_avail",
				   CTLFLAG_RD,
				   &txq->ift_mbuf_defrag_failed, "# of times no descriptors were available");
		SYSCTL_ADD_QUAD(ctx_list, queue_list, OID_AUTO, "tx_map_failed",
				   CTLFLAG_RD,
				   &txq->ift_map_failed, "# of times dma map failed");
		SYSCTL_ADD_QUAD(ctx_list, queue_list, OID_AUTO, "txd_encap_efbig",
				   CTLFLAG_RD,
				   &txq->ift_txd_encap_efbig, "# of times txd_encap returned EFBIG");
		SYSCTL_ADD_QUAD(ctx_list, queue_list, OID_AUTO, "no_tx_dma_setup",
				   CTLFLAG_RD,
				   &txq->ift_no_tx_dma_setup, "# of times map failed for other than EFBIG");
		SYSCTL_ADD_U16(ctx_list, queue_list, OID_AUTO, "txq_pidx",
				   CTLFLAG_RD,
				   &txq->ift_pidx, 1, "Producer Index");
		SYSCTL_ADD_U16(ctx_list, queue_list, OID_AUTO, "txq_cidx",
				   CTLFLAG_RD,
				   &txq->ift_cidx, 1, "Consumer Index");
		SYSCTL_ADD_U16(ctx_list, queue_list, OID_AUTO, "txq_cidx_processed",
				   CTLFLAG_RD,
				   &txq->ift_cidx_processed, 1, "Consumer Index seen by credit update");
		SYSCTL_ADD_U16(ctx_list, queue_list, OID_AUTO, "txq_in_use",
				   CTLFLAG_RD,
				   &txq->ift_in_use, 1, "descriptors in use");
		SYSCTL_ADD_QUAD(ctx_list, queue_list, OID_AUTO, "txq_processed",
				   CTLFLAG_RD,
				   &txq->ift_processed, "descriptors procesed for clean");
		SYSCTL_ADD_QUAD(ctx_list, queue_list, OID_AUTO, "txq_cleaned",
				   CTLFLAG_RD,
				   &txq->ift_cleaned, "total cleaned");
		SYSCTL_ADD_PROC(ctx_list, queue_list, OID_AUTO, "ring_state",
				CTLTYPE_STRING | CTLFLAG_RD, __DEVOLATILE(uint64_t *, &txq->ift_br[0]->state),
				0, mp_ring_state_handler, "A", "soft ring state");
		SYSCTL_ADD_COUNTER_U64(ctx_list, queue_list, OID_AUTO, "r_enqueues",
				       CTLFLAG_RD, &txq->ift_br[0]->enqueues,
				       "# of enqueues to the mp_ring for this queue");
		SYSCTL_ADD_COUNTER_U64(ctx_list, queue_list, OID_AUTO, "r_drops",
				       CTLFLAG_RD, &txq->ift_br[0]->drops,
				       "# of drops in the mp_ring for this queue");
		SYSCTL_ADD_COUNTER_U64(ctx_list, queue_list, OID_AUTO, "r_starts",
				       CTLFLAG_RD, &txq->ift_br[0]->starts,
				       "# of normal consumer starts in the mp_ring for this queue");
		SYSCTL_ADD_COUNTER_U64(ctx_list, queue_list, OID_AUTO, "r_stalls",
				       CTLFLAG_RD, &txq->ift_br[0]->stalls,
					       "# of consumer stalls in the mp_ring for this queue");
		SYSCTL_ADD_COUNTER_U64(ctx_list, queue_list, OID_AUTO, "r_restarts",
			       CTLFLAG_RD, &txq->ift_br[0]->restarts,
				       "# of consumer restarts in the mp_ring for this queue");
		SYSCTL_ADD_COUNTER_U64(ctx_list, queue_list, OID_AUTO, "r_abdications",
				       CTLFLAG_RD, &txq->ift_br[0]->abdications,
				       "# of consumer abdications in the mp_ring for this queue");

	}

	if (scctx->isc_nrxqsets > 100)
		qfmt = "rxq%03d";
	else if (scctx->isc_nrxqsets > 10)
		qfmt = "rxq%02d";
	else
		qfmt = "rxq%d";
	for (i = 0, rxq = ctx->ifc_rxqs; i < scctx->isc_nrxqsets; i++, rxq++) {
		snprintf(namebuf, NAME_BUFLEN, qfmt, i);
		queue_node = SYSCTL_ADD_NODE(ctx_list, child, OID_AUTO, namebuf,
					     CTLFLAG_RD, NULL, "Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);
		if (sctx->isc_flags & IFLIB_HAS_CQ) {
			SYSCTL_ADD_U16(ctx_list, queue_list, OID_AUTO, "rxq_cq_pidx",
				       CTLFLAG_RD,
				       &rxq->ifr_cq_pidx, 1, "Producer Index");
			SYSCTL_ADD_U16(ctx_list, queue_list, OID_AUTO, "rxq_cq_cidx",
				       CTLFLAG_RD,
				       &rxq->ifr_cq_cidx, 1, "Consumer Index");
		}
		for (j = 0, fl = rxq->ifr_fl; j < rxq->ifr_nfl; j++, fl++) {
			snprintf(namebuf, NAME_BUFLEN, "rxq_fl%d", j);
			fl_node = SYSCTL_ADD_NODE(ctx_list, queue_list, OID_AUTO, namebuf,
						     CTLFLAG_RD, NULL, "freelist Name");
			fl_list = SYSCTL_CHILDREN(fl_node);
			SYSCTL_ADD_U16(ctx_list, fl_list, OID_AUTO, "pidx",
				       CTLFLAG_RD,
				       &fl->ifl_pidx, 1, "Producer Index");
			SYSCTL_ADD_U16(ctx_list, fl_list, OID_AUTO, "cidx",
				       CTLFLAG_RD,
				       &fl->ifl_cidx, 1, "Consumer Index");
			SYSCTL_ADD_U16(ctx_list, fl_list, OID_AUTO, "credits",
				       CTLFLAG_RD,
				       &fl->ifl_credits, 1, "credits available");
#if MEMORY_LOGGING
			SYSCTL_ADD_QUAD(ctx_list, fl_list, OID_AUTO, "fl_m_enqueued",
					CTLFLAG_RD,
					&fl->ifl_m_enqueued, "mbufs allocated");
			SYSCTL_ADD_QUAD(ctx_list, fl_list, OID_AUTO, "fl_m_dequeued",
					CTLFLAG_RD,
					&fl->ifl_m_dequeued, "mbufs freed");
			SYSCTL_ADD_QUAD(ctx_list, fl_list, OID_AUTO, "fl_cl_enqueued",
					CTLFLAG_RD,
					&fl->ifl_cl_enqueued, "clusters allocated");
			SYSCTL_ADD_QUAD(ctx_list, fl_list, OID_AUTO, "fl_cl_dequeued",
					CTLFLAG_RD,
					&fl->ifl_cl_dequeued, "clusters freed");
#endif

		}
	}

}
