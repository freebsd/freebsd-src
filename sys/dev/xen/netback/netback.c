/*
 * Copyright (c) 2006, Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 * 3. Neither the name of Cisco Systems, Inc. nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
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
#include "opt_sctp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_types.h>
#include <net/ethernet.h>
#include <net/if_bridgevar.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#ifdef SCTP
#include <netinet/sctp.h>
#include <netinet/sctp_crc32.h>
#endif

#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#include <machine/in_cksum.h>
#include <machine/xen-os.h>
#include <machine/hypervisor.h>
#include <machine/hypervisor-ifs.h>
#include <machine/xen_intr.h>
#include <machine/evtchn.h>
#include <machine/xenbus.h>
#include <machine/gnttab.h>
#include <machine/xen-public/memory.h>
#include <dev/xen/xenbus/xenbus_comms.h>


#ifdef XEN_NETBACK_DEBUG
#define DPRINTF(fmt, args...) \
    printf("netback (%s:%d): " fmt, __FUNCTION__, __LINE__, ##args)
#else
#define DPRINTF(fmt, args...) ((void)0)
#endif

#ifdef XEN_NETBACK_DEBUG_LOTS
#define DDPRINTF(fmt, args...) \
    printf("netback (%s:%d): " fmt, __FUNCTION__, __LINE__, ##args)
#define DPRINTF_MBUF(_m) print_mbuf(_m, 0)
#define DPRINTF_MBUF_LEN(_m, _len) print_mbuf(_m, _len)
#else
#define DDPRINTF(fmt, args...) ((void)0)
#define DPRINTF_MBUF(_m) ((void)0)
#define DPRINTF_MBUF_LEN(_m, _len) ((void)0)
#endif

#define WPRINTF(fmt, args...) \
    printf("netback (%s:%d): " fmt, __FUNCTION__, __LINE__, ##args)

#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
#define BUG_ON PANIC_IF

#define IFNAME(_np) (_np)->ifp->if_xname

#define NET_TX_RING_SIZE __RING_SIZE((netif_tx_sring_t *)0, PAGE_SIZE)
#define NET_RX_RING_SIZE __RING_SIZE((netif_rx_sring_t *)0, PAGE_SIZE)

struct ring_ref {
	vm_offset_t va;
	grant_handle_t handle;
	uint64_t bus_addr;
};

typedef struct netback_info {

	/* Schedule lists */
	STAILQ_ENTRY(netback_info) next_tx;
	STAILQ_ENTRY(netback_info) next_rx;
	int on_tx_sched_list;
	int on_rx_sched_list;

	struct xenbus_device *xdev;
	XenbusState frontend_state;

	domid_t domid;
	int handle;
	char *bridge;

	int rings_connected;
	struct ring_ref tx_ring_ref;
	struct ring_ref rx_ring_ref;
	netif_tx_back_ring_t tx;
	netif_rx_back_ring_t rx;
	evtchn_port_t evtchn;
	int irq;
	void *irq_cookie;

	struct ifnet *ifp;
	int ref_cnt;

	device_t ndev;
	int attached;
} netif_t;


#define MAX_PENDING_REQS 256
#define PKT_PROT_LEN 64

static struct {
	netif_tx_request_t req;
	netif_t *netif;
} pending_tx_info[MAX_PENDING_REQS];
static uint16_t pending_ring[MAX_PENDING_REQS];
typedef unsigned int PEND_RING_IDX;
#define MASK_PEND_IDX(_i) ((_i)&(MAX_PENDING_REQS-1))
static PEND_RING_IDX pending_prod, pending_cons;
#define NR_PENDING_REQS (MAX_PENDING_REQS - pending_prod + pending_cons)

static unsigned long mmap_vstart;
#define MMAP_VADDR(_req) (mmap_vstart + ((_req) * PAGE_SIZE))

/* Freed TX mbufs get batched on this ring before return to pending_ring. */
static uint16_t dealloc_ring[MAX_PENDING_REQS];
static PEND_RING_IDX dealloc_prod, dealloc_cons;

static multicall_entry_t rx_mcl[NET_RX_RING_SIZE+1];
static mmu_update_t rx_mmu[NET_RX_RING_SIZE];
static gnttab_transfer_t grant_rx_op[NET_RX_RING_SIZE];

static grant_handle_t grant_tx_handle[MAX_PENDING_REQS];
static gnttab_unmap_grant_ref_t tx_unmap_ops[MAX_PENDING_REQS];
static gnttab_map_grant_ref_t tx_map_ops[MAX_PENDING_REQS];

static struct task net_tx_task, net_rx_task;
static struct callout rx_task_callout;

static STAILQ_HEAD(netback_tx_sched_list, netback_info) tx_sched_list =
	STAILQ_HEAD_INITIALIZER(tx_sched_list);
static STAILQ_HEAD(netback_rx_sched_list, netback_info) rx_sched_list =
	STAILQ_HEAD_INITIALIZER(rx_sched_list);
static struct mtx tx_sched_list_lock;
static struct mtx rx_sched_list_lock;

static int vif_unit_maker = 0;

/* Protos */
static void netback_start(struct ifnet *ifp);
static int netback_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static int vif_add_dev(struct xenbus_device *xdev);
static void disconnect_rings(netif_t *netif);

#ifdef XEN_NETBACK_DEBUG_LOTS
/* Debug code to display the contents of an mbuf */
static void
print_mbuf(struct mbuf *m, int max)
{
	int i, j=0;
	printf("mbuf %08x len = %d", (unsigned int)m, m->m_pkthdr.len);
	for (; m; m = m->m_next) {
		unsigned char *d = m->m_data;
		for (i=0; i < m->m_len; i++) {
			if (max && j == max)
				break;
			if ((j++ % 16) == 0)
				printf("\n%04x:", j);
			printf(" %02x", d[i]);
		}
	}
	printf("\n");
}
#endif


#define MAX_MFN_ALLOC 64
static unsigned long mfn_list[MAX_MFN_ALLOC];
static unsigned int alloc_index = 0;

static unsigned long
alloc_mfn(void)
{
	unsigned long mfn = 0;
	struct xen_memory_reservation reservation = {
		.extent_start = mfn_list,
		.nr_extents   = MAX_MFN_ALLOC,
		.extent_order = 0,
		.domid        = DOMID_SELF
	};
	if ( unlikely(alloc_index == 0) )
		alloc_index = HYPERVISOR_memory_op(
			XENMEM_increase_reservation, &reservation);
	if ( alloc_index != 0 )
		mfn = mfn_list[--alloc_index];
	return mfn;
}

static unsigned long
alloc_empty_page_range(unsigned long nr_pages)
{
	void *pages;
	int i = 0, j = 0;
	multicall_entry_t mcl[17];
	unsigned long mfn_list[16];
	struct xen_memory_reservation reservation = {
		.extent_start = mfn_list,
		.nr_extents   = 0,
		.address_bits = 0,
		.extent_order = 0,
		.domid        = DOMID_SELF
	};

	pages = malloc(nr_pages*PAGE_SIZE, M_DEVBUF, M_NOWAIT);
	if (pages == NULL)
		return 0;

	memset(mcl, 0, sizeof(mcl));

	while (i < nr_pages) {
		unsigned long va = (unsigned long)pages + (i++ * PAGE_SIZE);

		mcl[j].op = __HYPERVISOR_update_va_mapping;
		mcl[j].args[0] = va;

		mfn_list[j++] = vtomach(va) >> PAGE_SHIFT;

		xen_phys_machine[(vtophys(va) >> PAGE_SHIFT)] = INVALID_P2M_ENTRY;

		if (j == 16 || i == nr_pages) {
			mcl[j-1].args[MULTI_UVMFLAGS_INDEX] = UVMF_TLB_FLUSH|UVMF_LOCAL;

			reservation.nr_extents = j;

			mcl[j].op = __HYPERVISOR_memory_op;
			mcl[j].args[0] = XENMEM_decrease_reservation;
			mcl[j].args[1] =  (unsigned long)&reservation;
			
			(void)HYPERVISOR_multicall(mcl, j+1);

			mcl[j-1].args[MULTI_UVMFLAGS_INDEX] = 0;
			j = 0;
		}
	}

	return (unsigned long)pages;
}

#ifdef XEN_NETBACK_FIXUP_CSUM
static void
fixup_checksum(struct mbuf *m)
{
	struct ether_header *eh = mtod(m, struct ether_header *);
	struct ip *ip = (struct ip *)(eh + 1);
	int iphlen = ip->ip_hl << 2;
	int iplen = ntohs(ip->ip_len);

	if ((m->m_pkthdr.csum_flags & CSUM_TCP)) {
		struct tcphdr *th = (struct tcphdr *)((caddr_t)ip + iphlen);
		th->th_sum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
			htons(IPPROTO_TCP + (iplen - iphlen)));
		th->th_sum = in_cksum_skip(m, iplen + sizeof(*eh), sizeof(*eh) + iphlen);
		m->m_pkthdr.csum_flags &= ~CSUM_TCP;
#ifdef SCTP
	} else if (sw_csum & CSUM_SCTP) {
		sctp_delayed_cksum(m, iphlen);
		sw_csum &= ~CSUM_SCTP;
#endif
	} else {
		u_short csum;
		struct udphdr *uh = (struct udphdr *)((caddr_t)ip + iphlen);
		uh->uh_sum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
			htons(IPPROTO_UDP + (iplen - iphlen)));
		if ((csum = in_cksum_skip(m, iplen + sizeof(*eh), sizeof(*eh) + iphlen)) == 0)
			csum = 0xffff;
		uh->uh_sum = csum;
		m->m_pkthdr.csum_flags &= ~CSUM_UDP;
	}
}
#endif

/* Add the interface to the specified bridge */
static int
add_to_bridge(struct ifnet *ifp, char *bridge)
{
	struct ifdrv ifd;
	struct ifbreq ifb;
	struct ifnet *ifp_bridge = ifunit(bridge);

	if (!ifp_bridge)
		return ENOENT;

	bzero(&ifd, sizeof(ifd));
	bzero(&ifb, sizeof(ifb));

	strcpy(ifb.ifbr_ifsname, ifp->if_xname);
	strcpy(ifd.ifd_name, ifp->if_xname);
	ifd.ifd_cmd = BRDGADD;
	ifd.ifd_len = sizeof(ifb);
	ifd.ifd_data = &ifb;

	return bridge_ioctl_kern(ifp_bridge, SIOCSDRVSPEC, &ifd);
	
}

static int
netif_create(int handle, struct xenbus_device *xdev, char *bridge)
{
	netif_t *netif;
	struct ifnet *ifp;

	netif = (netif_t *)malloc(sizeof(*netif), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!netif)
		return ENOMEM;

	netif->ref_cnt = 1;
	netif->handle = handle;
	netif->domid = xdev->otherend_id;
	netif->xdev = xdev;
	netif->bridge = bridge;
	xdev->data = netif;

	/* Set up ifnet structure */
	ifp = netif->ifp = if_alloc(IFT_ETHER);
	if (!ifp) {
		if (bridge)
			free(bridge, M_DEVBUF);
		free(netif, M_DEVBUF);
		return ENOMEM;
	}

	ifp->if_softc = netif;
	if_initname(ifp, "vif",
		atomic_fetchadd_int(&vif_unit_maker, 1) /* ifno */ );
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX;
	ifp->if_output = ether_output;
	ifp->if_start = netback_start;
	ifp->if_ioctl = netback_ioctl;
	ifp->if_mtu = ETHERMTU;
	ifp->if_snd.ifq_maxlen = NET_TX_RING_SIZE - 1;
	
	DPRINTF("Created %s for domid=%d handle=%d\n", IFNAME(netif), netif->domid, netif->handle);

	return 0;
}

static void
netif_get(netif_t *netif)
{
	atomic_add_int(&netif->ref_cnt, 1);
}

static void
netif_put(netif_t *netif)
{
	if (atomic_fetchadd_int(&netif->ref_cnt, -1) == 1) {
		DPRINTF("%s\n", IFNAME(netif));
		disconnect_rings(netif);
		if (netif->ifp) {
			if_free(netif->ifp);
			netif->ifp = NULL;
		}
		if (netif->bridge)
			free(netif->bridge, M_DEVBUF);
		free(netif, M_DEVBUF);
	}
}

static int
netback_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	switch (cmd) {
	case SIOCSIFFLAGS:
	DDPRINTF("%s cmd=SIOCSIFFLAGS flags=%x\n",
			IFNAME((struct netback_info *)ifp->if_softc), ((struct ifreq *)data)->ifr_flags);
		return 0;
	}

	DDPRINTF("%s cmd=%lx\n", IFNAME((struct netback_info *)ifp->if_softc), cmd);

	return ether_ioctl(ifp, cmd, data);
}

static inline void
maybe_schedule_tx_action(void)
{
	smp_mb();
	if ((NR_PENDING_REQS < (MAX_PENDING_REQS/2)) && !STAILQ_EMPTY(&tx_sched_list))
		taskqueue_enqueue(taskqueue_swi, &net_tx_task); 
}

/* Removes netif from front of list and does not call netif_put() (caller must) */
static netif_t *
remove_from_tx_schedule_list(void)
{
	netif_t *netif;

	mtx_lock(&tx_sched_list_lock);

	if ((netif = STAILQ_FIRST(&tx_sched_list))) {
		STAILQ_REMOVE(&tx_sched_list, netif, netback_info, next_tx);
		STAILQ_NEXT(netif, next_tx) = NULL;
		netif->on_tx_sched_list = 0;
	}

	mtx_unlock(&tx_sched_list_lock);

	return netif;
}

/* Adds netif to end of list and calls netif_get() */
static void
add_to_tx_schedule_list_tail(netif_t *netif)
{
	if (netif->on_tx_sched_list)
		return;

	mtx_lock(&tx_sched_list_lock);
	if (!netif->on_tx_sched_list && (netif->ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		netif_get(netif);
		STAILQ_INSERT_TAIL(&tx_sched_list, netif, next_tx);
		netif->on_tx_sched_list = 1;
	}
	mtx_unlock(&tx_sched_list_lock);
}

/*
 * Note on CONFIG_XEN_NETDEV_PIPELINED_TRANSMITTER:
 * If this driver is pipelining transmit requests then we can be very
 * aggressive in avoiding new-packet notifications -- frontend only needs to
 * send a notification if there are no outstanding unreceived responses.
 * If we may be buffer transmit buffers for any reason then we must be rather
 * more conservative and treat this as the final check for pending work.
 */
static void
netif_schedule_tx_work(netif_t *netif)
{
	int more_to_do;

#ifdef CONFIG_XEN_NETDEV_PIPELINED_TRANSMITTER
	more_to_do = RING_HAS_UNCONSUMED_REQUESTS(&netif->tx);
#else
	RING_FINAL_CHECK_FOR_REQUESTS(&netif->tx, more_to_do);
#endif

	if (more_to_do) {
		DDPRINTF("Adding %s to tx sched list\n", IFNAME(netif));
		add_to_tx_schedule_list_tail(netif);
		maybe_schedule_tx_action();
	}
}

static struct mtx dealloc_lock;
MTX_SYSINIT(netback_dealloc, &dealloc_lock, "DEALLOC LOCK", MTX_SPIN | MTX_NOWITNESS);

static void
netif_idx_release(uint16_t pending_idx)
{
	mtx_lock_spin(&dealloc_lock);
	dealloc_ring[MASK_PEND_IDX(dealloc_prod++)] = pending_idx;
	mtx_unlock_spin(&dealloc_lock);

	taskqueue_enqueue(taskqueue_swi, &net_tx_task); 
}

static void
make_tx_response(netif_t *netif, 
				 uint16_t    id,
				 int8_t      st)
{
	RING_IDX i = netif->tx.rsp_prod_pvt;
	netif_tx_response_t *resp;
	int notify;

	resp = RING_GET_RESPONSE(&netif->tx, i);
	resp->id     = id;
	resp->status = st;

	netif->tx.rsp_prod_pvt = ++i;
	RING_PUSH_RESPONSES_AND_CHECK_NOTIFY(&netif->tx, notify);
	if (notify)
		notify_remote_via_irq(netif->irq);

#ifdef CONFIG_XEN_NETDEV_PIPELINED_TRANSMITTER
	if (i == netif->tx.req_cons) {
		int more_to_do;
		RING_FINAL_CHECK_FOR_REQUESTS(&netif->tx, more_to_do);
		if (more_to_do)
			add_to_tx_schedule_list_tail(netif);
	}
#endif
}

inline static void
net_tx_action_dealloc(void)
{
	gnttab_unmap_grant_ref_t *gop;
	uint16_t pending_idx;
	PEND_RING_IDX dc, dp;
	netif_t *netif;
	int ret;

	dc = dealloc_cons;
	dp = dealloc_prod;

	/*
	 * Free up any grants we have finished using
	 */
	gop = tx_unmap_ops;
	while (dc != dp) {
		pending_idx = dealloc_ring[MASK_PEND_IDX(dc++)];
		gop->host_addr    = MMAP_VADDR(pending_idx);
		gop->dev_bus_addr = 0;
		gop->handle       = grant_tx_handle[pending_idx];
		gop++;
	}
	ret = HYPERVISOR_grant_table_op(
		GNTTABOP_unmap_grant_ref, tx_unmap_ops, gop - tx_unmap_ops);
	BUG_ON(ret);

	while (dealloc_cons != dp) {
		pending_idx = dealloc_ring[MASK_PEND_IDX(dealloc_cons++)];

		netif = pending_tx_info[pending_idx].netif;

		make_tx_response(netif, pending_tx_info[pending_idx].req.id, 
				 NETIF_RSP_OKAY);
        
		pending_ring[MASK_PEND_IDX(pending_prod++)] = pending_idx;

		netif_put(netif);
	}
}

static void
netif_page_release(void *buf, void *args)
{
	uint16_t pending_idx = (unsigned int)args;
	
	DDPRINTF("pending_idx=%u\n", pending_idx);

	KASSERT(pending_idx < MAX_PENDING_REQS, ("%s: bad index %u", __func__, pending_idx));

	netif_idx_release(pending_idx);
}

static void
net_tx_action(void *context, int pending)
{
	struct mbuf *m;
	netif_t *netif;
	netif_tx_request_t txreq;
	uint16_t pending_idx;
	RING_IDX i;
	gnttab_map_grant_ref_t *mop;
	int ret, work_to_do;
	struct mbuf *txq = NULL, *txq_last = NULL;

	if (dealloc_cons != dealloc_prod)
		net_tx_action_dealloc();

	mop = tx_map_ops;
	while ((NR_PENDING_REQS < MAX_PENDING_REQS) && !STAILQ_EMPTY(&tx_sched_list)) {

		/* Get a netif from the list with work to do. */
		netif = remove_from_tx_schedule_list();

		DDPRINTF("Processing %s (prod=%u, cons=%u)\n",
				IFNAME(netif), netif->tx.sring->req_prod, netif->tx.req_cons);

		RING_FINAL_CHECK_FOR_REQUESTS(&netif->tx, work_to_do);
		if (!work_to_do) {
			netif_put(netif);
			continue;
		}

		i = netif->tx.req_cons;
		rmb(); /* Ensure that we see the request before we copy it. */
		memcpy(&txreq, RING_GET_REQUEST(&netif->tx, i), sizeof(txreq));

		/* If we want credit-based scheduling, coud add it here - WORK */

		netif->tx.req_cons++;

		netif_schedule_tx_work(netif);

		if (unlikely(txreq.size < ETHER_HDR_LEN) || 
		    unlikely(txreq.size > (ETHER_MAX_LEN-ETHER_CRC_LEN))) {
			WPRINTF("Bad packet size: %d\n", txreq.size);
			make_tx_response(netif, txreq.id, NETIF_RSP_ERROR);
			netif_put(netif);
			continue; 
		}

		/* No crossing a page as the payload mustn't fragment. */
		if (unlikely((txreq.offset + txreq.size) >= PAGE_SIZE)) {
			WPRINTF("txreq.offset: %x, size: %u, end: %u\n", 
				txreq.offset, txreq.size, 
				(txreq.offset & PAGE_MASK) + txreq.size);
			make_tx_response(netif, txreq.id, NETIF_RSP_ERROR);
			netif_put(netif);
			continue;
		}

		pending_idx = pending_ring[MASK_PEND_IDX(pending_cons)];

		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (!m) {
			WPRINTF("Failed to allocate mbuf\n");
			make_tx_response(netif, txreq.id, NETIF_RSP_ERROR);
			netif_put(netif);
			break;
		}
		m->m_pkthdr.rcvif = netif->ifp;

		if ((m->m_pkthdr.len = txreq.size) > PKT_PROT_LEN) {
			struct mbuf *n;
			MGET(n, M_DONTWAIT, MT_DATA);
			if (!(m->m_next = n)) {
				m_freem(m);
				WPRINTF("Failed to allocate second mbuf\n");
				make_tx_response(netif, txreq.id, NETIF_RSP_ERROR);
				netif_put(netif);
				break;
			}
			n->m_len = txreq.size - PKT_PROT_LEN;
			m->m_len = PKT_PROT_LEN;
		} else
			m->m_len = txreq.size;

		mop->host_addr = MMAP_VADDR(pending_idx);
		mop->dom       = netif->domid;
		mop->ref       = txreq.gref;
		mop->flags     = GNTMAP_host_map | GNTMAP_readonly;
		mop++;

		memcpy(&pending_tx_info[pending_idx].req,
		       &txreq, sizeof(txreq));
		pending_tx_info[pending_idx].netif = netif;
		*((uint16_t *)m->m_data) = pending_idx;

		if (txq_last)
			txq_last->m_nextpkt = m;
		else
			txq = m;
		txq_last = m;

		pending_cons++;

		if ((mop - tx_map_ops) >= ARRAY_SIZE(tx_map_ops))
			break;
	}

	if (!txq)
		return;

	ret = HYPERVISOR_grant_table_op(
		GNTTABOP_map_grant_ref, tx_map_ops, mop - tx_map_ops);
	BUG_ON(ret);

	mop = tx_map_ops;
	while ((m = txq) != NULL) {
		caddr_t data;

		txq = m->m_nextpkt;
		m->m_nextpkt = NULL;

		pending_idx = *((uint16_t *)m->m_data);
		netif       = pending_tx_info[pending_idx].netif;
		memcpy(&txreq, &pending_tx_info[pending_idx].req, sizeof(txreq));

		/* Check the remap error code. */
		if (unlikely(mop->status)) {
			WPRINTF("#### netback grant fails\n");
			make_tx_response(netif, txreq.id, NETIF_RSP_ERROR);
			netif_put(netif);
			m_freem(m);
			mop++;
			pending_ring[MASK_PEND_IDX(pending_prod++)] = pending_idx;
			continue;
		}

#if 0
		/* Can't do this in FreeBSD since vtophys() returns the pfn */
		/* of the remote domain who loaned us the machine page - DPT */
		xen_phys_machine[(vtophys(MMAP_VADDR(pending_idx)) >> PAGE_SHIFT)] =
			mop->dev_bus_addr >> PAGE_SHIFT;
#endif
		grant_tx_handle[pending_idx] = mop->handle;

		/* Setup data in mbuf (lengths are already set) */
		data = (caddr_t)(MMAP_VADDR(pending_idx)|txreq.offset);
		bcopy(data, m->m_data, m->m_len);
		if (m->m_next) {
			struct mbuf *n = m->m_next;
			MEXTADD(n, MMAP_VADDR(pending_idx), PAGE_SIZE, netif_page_release,
				(void *)(unsigned int)pending_idx, M_RDONLY, EXT_NET_DRV);
			n->m_data = &data[PKT_PROT_LEN];
		} else {
			/* Schedule a response immediately. */
			netif_idx_release(pending_idx);
		}

		if ((txreq.flags & NETTXF_data_validated)) {
			/* Tell the stack the checksums are okay */
			m->m_pkthdr.csum_flags |=
				(CSUM_IP_CHECKED | CSUM_IP_VALID | CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
			m->m_pkthdr.csum_data = 0xffff;
		}

		/* If necessary, inform stack to compute the checksums if it forwards the packet */
		if ((txreq.flags & NETTXF_csum_blank)) {
			struct ether_header *eh = mtod(m, struct ether_header *);
			if (ntohs(eh->ether_type) == ETHERTYPE_IP) {
				struct ip *ip = (struct ip *)&m->m_data[14];
				if (ip->ip_p == IPPROTO_TCP)
					m->m_pkthdr.csum_flags |= CSUM_TCP;
				else if (ip->ip_p == IPPROTO_UDP)
					m->m_pkthdr.csum_flags |= CSUM_UDP;
			}
		}

		netif->ifp->if_ibytes += m->m_pkthdr.len;
		netif->ifp->if_ipackets++;

		DDPRINTF("RECV %d bytes from %s (cflags=%x)\n",
			m->m_pkthdr.len, IFNAME(netif), m->m_pkthdr.csum_flags);
		DPRINTF_MBUF_LEN(m, 128);

		(*netif->ifp->if_input)(netif->ifp, m);

		mop++;
	}
}

/* Handle interrupt from a frontend */
static void
netback_intr(void *arg)
{
	netif_t *netif = arg;
	DDPRINTF("%s\n", IFNAME(netif));
	add_to_tx_schedule_list_tail(netif);
	maybe_schedule_tx_action();
}

/* Removes netif from front of list and does not call netif_put() (caller must) */
static netif_t *
remove_from_rx_schedule_list(void)
{
	netif_t *netif;

	mtx_lock(&rx_sched_list_lock);

	if ((netif = STAILQ_FIRST(&rx_sched_list))) {
		STAILQ_REMOVE(&rx_sched_list, netif, netback_info, next_rx);
		STAILQ_NEXT(netif, next_rx) = NULL;
		netif->on_rx_sched_list = 0;
	}

	mtx_unlock(&rx_sched_list_lock);

	return netif;
}

/* Adds netif to end of list and calls netif_get() */
static void
add_to_rx_schedule_list_tail(netif_t *netif)
{
	if (netif->on_rx_sched_list)
		return;

	mtx_lock(&rx_sched_list_lock);
	if (!netif->on_rx_sched_list && (netif->ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		netif_get(netif);
		STAILQ_INSERT_TAIL(&rx_sched_list, netif, next_rx);
		netif->on_rx_sched_list = 1;
	}
	mtx_unlock(&rx_sched_list_lock);
}

static int
make_rx_response(netif_t *netif, uint16_t id, int8_t st,
				 uint16_t offset, uint16_t size, uint16_t flags)
{
	RING_IDX i = netif->rx.rsp_prod_pvt;
	netif_rx_response_t *resp;
	int notify;

	resp = RING_GET_RESPONSE(&netif->rx, i);
	resp->offset     = offset;
	resp->flags      = flags;
	resp->id         = id;
	resp->status     = (int16_t)size;
	if (st < 0)
		resp->status = (int16_t)st;

	DDPRINTF("rx resp(%d): off=%x fl=%x id=%x stat=%d\n",
		i, resp->offset, resp->flags, resp->id, resp->status);

	netif->rx.rsp_prod_pvt = ++i;
	RING_PUSH_RESPONSES_AND_CHECK_NOTIFY(&netif->rx, notify);

	return notify;
}

static int
netif_rx(netif_t *netif)
{
	struct ifnet *ifp = netif->ifp;
	struct mbuf *m;
	multicall_entry_t *mcl;
	mmu_update_t *mmu;
	gnttab_transfer_t *gop;
	unsigned long vdata, old_mfn, new_mfn;
	struct mbuf *rxq = NULL, *rxq_last = NULL;
	int ret, notify = 0, pkts_dequeued = 0;

	DDPRINTF("%s\n", IFNAME(netif));

	mcl = rx_mcl;
	mmu = rx_mmu;
	gop = grant_rx_op;

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		
		/* Quit if the target domain has no receive buffers */
		if (netif->rx.req_cons == netif->rx.sring->req_prod)
			break;

		IFQ_DRV_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		pkts_dequeued++;

		/* Check if we need to copy the data */
		if (((m->m_flags & (M_RDONLY|M_EXT)) != M_EXT) ||
			(*m->m_ext.ref_cnt > 1) || m->m_next != NULL) {
			struct mbuf *n;
				
			DDPRINTF("copying mbuf (fl=%x ext=%x rc=%d n=%x)\n",
				m->m_flags,
				(m->m_flags & M_EXT) ? m->m_ext.ext_type : 0,
				(m->m_flags & M_EXT) ? *m->m_ext.ref_cnt : 0,
				(unsigned int)m->m_next);

			/* Make copy */
			MGETHDR(n, M_DONTWAIT, MT_DATA);
			if (!n)
				goto drop;

			MCLGET(n, M_DONTWAIT);
			if (!(n->m_flags & M_EXT)) {
				m_freem(n);
				goto drop;
			}

			/* Leave space at front and keep current alignment */
			n->m_data += 16 + ((unsigned int)m->m_data & 0x3);

			if (m->m_pkthdr.len > M_TRAILINGSPACE(n)) {
				WPRINTF("pkt to big %d\n", m->m_pkthdr.len);
				m_freem(n);
				goto drop;
			}
			m_copydata(m, 0, m->m_pkthdr.len, n->m_data);
			n->m_pkthdr.len = n->m_len = m->m_pkthdr.len;
			n->m_pkthdr.csum_flags = (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA);
			m_freem(m);
			m = n;
		}

		vdata = (unsigned long)m->m_data;
		old_mfn = vtomach(vdata) >> PAGE_SHIFT;

		if ((new_mfn = alloc_mfn()) == 0)
			goto drop;

#ifdef XEN_NETBACK_FIXUP_CSUM
		/* Check if we need to compute a checksum.  This happens */
		/* when bridging from one domain to another. */
		if ((m->m_pkthdr.csum_flags & CSUM_DELAY_DATA) ||
			(m->m_pkthdr.csum_flags & CSUM_SCTP))
			fixup_checksum(m);
#endif

		xen_phys_machine[(vtophys(vdata) >> PAGE_SHIFT)] = new_mfn;

		mcl->op = __HYPERVISOR_update_va_mapping;
		mcl->args[0] = vdata;
		mcl->args[1] = (new_mfn << PAGE_SHIFT) | PG_V | PG_RW | PG_M | PG_A;
		mcl->args[2] = 0;
		mcl->args[3] = 0;
		mcl++;

		gop->mfn = old_mfn;
		gop->domid = netif->domid;
		gop->ref = RING_GET_REQUEST(&netif->rx, netif->rx.req_cons)->gref;
		netif->rx.req_cons++;
		gop++;

		mmu->ptr = (new_mfn << PAGE_SHIFT) | MMU_MACHPHYS_UPDATE;
		mmu->val = vtophys(vdata) >> PAGE_SHIFT;  
		mmu++;

		if (rxq_last)
			rxq_last->m_nextpkt = m;
		else
			rxq = m;
		rxq_last = m;

		DDPRINTF("XMIT %d bytes to %s\n", m->m_pkthdr.len, IFNAME(netif));
		DPRINTF_MBUF_LEN(m, 128);

		/* Filled the batch queue? */
		if ((gop - grant_rx_op) == ARRAY_SIZE(grant_rx_op))
			break;		

		continue;
	drop:
		DDPRINTF("dropping pkt\n");
		ifp->if_oerrors++;
		m_freem(m);
	}

	if (mcl == rx_mcl)
		return pkts_dequeued;

	mcl->op = __HYPERVISOR_mmu_update;
	mcl->args[0] = (unsigned long)rx_mmu;
	mcl->args[1] = mmu - rx_mmu;
	mcl->args[2] = 0;
	mcl->args[3] = DOMID_SELF;
	mcl++;

	mcl[-2].args[MULTI_UVMFLAGS_INDEX] = UVMF_TLB_FLUSH|UVMF_ALL;
	ret = HYPERVISOR_multicall(rx_mcl, mcl - rx_mcl);
	BUG_ON(ret != 0);

	ret = HYPERVISOR_grant_table_op(GNTTABOP_transfer, grant_rx_op, gop - grant_rx_op);
	BUG_ON(ret != 0);

	mcl = rx_mcl;
	gop = grant_rx_op;

	while ((m = rxq) != NULL) {
		int8_t status;
		uint16_t id, flags = 0;

		rxq = m->m_nextpkt;
		m->m_nextpkt = NULL;

		/* Rederive the machine addresses. */
		new_mfn = mcl->args[1] >> PAGE_SHIFT;
		old_mfn = gop->mfn;

		ifp->if_obytes += m->m_pkthdr.len;
		ifp->if_opackets++;

		/* The update_va_mapping() must not fail. */
		BUG_ON(mcl->result != 0);

		/* Setup flags */
		if ((m->m_pkthdr.csum_flags & CSUM_DELAY_DATA))
			flags |= NETRXF_csum_blank | NETRXF_data_validated;
		else if ((m->m_pkthdr.csum_flags & CSUM_DATA_VALID))
			flags |= NETRXF_data_validated;

		/* Check the reassignment error code. */
		status = NETIF_RSP_OKAY;
		if (gop->status != 0) { 
			DPRINTF("Bad status %d from grant transfer to DOM%u\n",
				gop->status, netif->domid);
			/*
			 * Page no longer belongs to us unless GNTST_bad_page,
			 * but that should be a fatal error anyway.
			 */
			BUG_ON(gop->status == GNTST_bad_page);
			status = NETIF_RSP_ERROR; 
		}
		id = RING_GET_REQUEST(&netif->rx, netif->rx.rsp_prod_pvt)->id;
		notify |= make_rx_response(netif, id, status,
					(unsigned long)m->m_data & PAGE_MASK,
					m->m_pkthdr.len, flags);

		m_freem(m);
		mcl++;
		gop++;
	}

	if (notify)
		notify_remote_via_irq(netif->irq);

	return pkts_dequeued;
}

static void
rx_task_timer(void *arg)
{
	DDPRINTF("\n");
	taskqueue_enqueue(taskqueue_swi, &net_rx_task); 
}

static void
net_rx_action(void *context, int pending)
{
	netif_t *netif, *last_zero_work = NULL;

	DDPRINTF("\n");

	while ((netif = remove_from_rx_schedule_list())) {
		struct ifnet *ifp = netif->ifp;

		if (netif == last_zero_work) {
			if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
				add_to_rx_schedule_list_tail(netif);
			netif_put(netif);
			if (!STAILQ_EMPTY(&rx_sched_list))
				callout_reset(&rx_task_callout, 1, rx_task_timer, NULL);
			break;
		}

		if ((ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			if (netif_rx(netif))
				last_zero_work = NULL;
			else if (!last_zero_work)
				last_zero_work = netif;
			if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
				add_to_rx_schedule_list_tail(netif);
		}

		netif_put(netif);
	}
}

static void
netback_start(struct ifnet *ifp)
{
	netif_t *netif = (netif_t *)ifp->if_softc;

	DDPRINTF("%s\n", IFNAME(netif));

	add_to_rx_schedule_list_tail(netif);
	taskqueue_enqueue(taskqueue_swi, &net_rx_task); 
}

/* Map a grant ref to a ring */
static int
map_ring(grant_ref_t ref, domid_t dom, struct ring_ref *ring)
{
	struct gnttab_map_grant_ref op;

	ring->va = kmem_alloc_nofault(kernel_map, PAGE_SIZE);
	if (ring->va == 0)
		return ENOMEM;

	op.host_addr = ring->va;
	op.flags = GNTMAP_host_map;
	op.ref = ref;
	op.dom = dom;
	HYPERVISOR_grant_table_op(GNTTABOP_map_grant_ref, &op, 1);
	if (op.status) {
		WPRINTF("grant table op err=%d\n", op.status);
		kmem_free(kernel_map, ring->va, PAGE_SIZE);
		ring->va = 0;
		return EACCES;
	}

	ring->handle = op.handle;
	ring->bus_addr = op.dev_bus_addr;

	return 0;
}

/* Unmap grant ref for a ring */
static void
unmap_ring(struct ring_ref *ring)
{
	struct gnttab_unmap_grant_ref op;

	op.host_addr = ring->va;
	op.dev_bus_addr = ring->bus_addr;
	op.handle = ring->handle;
	HYPERVISOR_grant_table_op(GNTTABOP_unmap_grant_ref, &op, 1);
	if (op.status)
		WPRINTF("grant table op err=%d\n", op.status);

	kmem_free(kernel_map, ring->va, PAGE_SIZE);
	ring->va = 0;
}

static int
connect_rings(netif_t *netif)
{
	struct xenbus_device *xdev = netif->xdev;
	netif_tx_sring_t *txs;
	netif_rx_sring_t *rxs;
	unsigned long tx_ring_ref, rx_ring_ref;
	evtchn_port_t evtchn;
	evtchn_op_t op = { .cmd = EVTCHNOP_bind_interdomain };
	int err;

	// Grab FE data and map his memory
	err = xenbus_gather(NULL, xdev->otherend,
			"tx-ring-ref", "%lu", &tx_ring_ref,
		    "rx-ring-ref", "%lu", &rx_ring_ref,
		    "event-channel", "%u", &evtchn, NULL);
	if (err) {
		xenbus_dev_fatal(xdev, err,
			"reading %s/ring-ref and event-channel",
			xdev->otherend);
		return err;
	}

	err = map_ring(tx_ring_ref, netif->domid, &netif->tx_ring_ref);
	if (err) {
		xenbus_dev_fatal(xdev, err, "mapping tx ring");
		return err;
	}
	txs = (netif_tx_sring_t *)netif->tx_ring_ref.va;
	BACK_RING_INIT(&netif->tx, txs, PAGE_SIZE);

	err = map_ring(rx_ring_ref, netif->domid, &netif->rx_ring_ref);
	if (err) {
		unmap_ring(&netif->tx_ring_ref);
		xenbus_dev_fatal(xdev, err, "mapping rx ring");
		return err;
	}
	rxs = (netif_rx_sring_t *)netif->rx_ring_ref.va;
	BACK_RING_INIT(&netif->rx, rxs, PAGE_SIZE);

	op.u.bind_interdomain.remote_dom = netif->domid;
	op.u.bind_interdomain.remote_port = evtchn;
	err = HYPERVISOR_event_channel_op(&op);
	if (err) {
		unmap_ring(&netif->tx_ring_ref);
		unmap_ring(&netif->rx_ring_ref);
		xenbus_dev_fatal(xdev, err, "binding event channel");
		return err;
	}
	netif->evtchn = op.u.bind_interdomain.local_port;

	/* bind evtchn to irq handler */
	netif->irq =
		bind_evtchn_to_irqhandler(netif->evtchn, "netback",
			netback_intr, netif, INTR_TYPE_NET|INTR_MPSAFE, &netif->irq_cookie);

	netif->rings_connected = 1;

	DPRINTF("%s connected! evtchn=%d irq=%d\n",
		IFNAME(netif), netif->evtchn, netif->irq);

	return 0;
}

static void
disconnect_rings(netif_t *netif)
{
	DPRINTF("\n");

	if (netif->rings_connected) {
		unbind_from_irqhandler(netif->irq, netif->irq_cookie);
		netif->irq = 0;
		unmap_ring(&netif->tx_ring_ref);
		unmap_ring(&netif->rx_ring_ref);
		netif->rings_connected = 0;
	}
}

static void
connect(netif_t *netif)
{
	if (!netif->xdev ||
		!netif->attached ||
		netif->frontend_state != XenbusStateConnected) {
		return;
	}

	if (!connect_rings(netif)) {
		xenbus_switch_state(netif->xdev, NULL, XenbusStateConnected);

		/* Turn on interface */
		netif->ifp->if_drv_flags |= IFF_DRV_RUNNING;
		netif->ifp->if_flags |= IFF_UP;
	}
}

static int
netback_remove(struct xenbus_device *xdev)
{
	netif_t *netif = xdev->data;
	device_t ndev;

	DPRINTF("remove %s\n", xdev->nodename);

	if ((ndev = netif->ndev)) {
		netif->ndev = NULL;
		mtx_lock(&Giant);
		device_detach(ndev);
		mtx_unlock(&Giant);
	}

	xdev->data = NULL;
	netif->xdev = NULL;
	netif_put(netif);

	return 0;
}

/**
 * Entry point to this code when a new device is created.  Allocate the basic
 * structures and the ring buffers for communication with the frontend.
 * Switch to Connected state.
 */
static int
netback_probe(struct xenbus_device *xdev, const struct xenbus_device_id *id)
{
	int err;
	long handle;
	char *bridge;
	
	DPRINTF("node=%s\n", xdev->nodename);

	/* Grab the handle */
	err = xenbus_scanf(NULL, xdev->nodename, "handle", "%li", &handle);
	if (err != 1) {
		xenbus_dev_fatal(xdev, err, "reading handle");
		return err;
	}

	/* Check for bridge */
	bridge = xenbus_read(NULL, xdev->nodename, "bridge", NULL);
	if (IS_ERR(bridge))
		bridge = NULL;

	err = xenbus_switch_state(xdev, NULL, XenbusStateInitWait);
	if (err) {
		xenbus_dev_fatal(xdev, err, "writing switch state");
		return err;
	}

	err = netif_create(handle, xdev, bridge);
	if (err) {
		xenbus_dev_fatal(xdev, err, "creating netif");
		return err;
	}

	err = vif_add_dev(xdev);
	if (err) {
		netif_put((netif_t *)xdev->data);
		xenbus_dev_fatal(xdev, err, "adding vif device");
		return err;
	}

	return 0;
}

/**
 * We are reconnecting to the backend, due to a suspend/resume, or a backend
 * driver restart.  We tear down our netif structure and recreate it, but
 * leave the device-layer structures intact so that this is transparent to the
 * rest of the kernel.
 */
static int netback_resume(struct xenbus_device *xdev)
{
	DPRINTF("node=%s\n", xdev->nodename);
	return 0;
}


/**
 * Callback received when the frontend's state changes.
 */
static void frontend_changed(struct xenbus_device *xdev,
							 XenbusState frontend_state)
{
	netif_t *netif = xdev->data;

	DPRINTF("state=%d\n", frontend_state);
	
	netif->frontend_state = frontend_state;

	switch (frontend_state) {
	case XenbusStateInitialising:
	case XenbusStateInitialised:
		break;
	case XenbusStateConnected:
		connect(netif);
		break;
	case XenbusStateClosing:
		xenbus_switch_state(xdev, NULL, XenbusStateClosing);
		break;
	case XenbusStateClosed:
		xenbus_remove_device(xdev);
		break;
	case XenbusStateUnknown:
	case XenbusStateInitWait:
		xenbus_dev_fatal(xdev, EINVAL, "saw state %d at frontend",
						 frontend_state);
		break;
	}
}

/* ** Driver registration ** */

static struct xenbus_device_id netback_ids[] = {
	{ "vif" },
	{ "" }
};

static struct xenbus_driver netback = {
	.name = "netback",
	.ids = netback_ids,
	.probe = netback_probe,
	.remove = netback_remove,
	.resume= netback_resume,
	.otherend_changed = frontend_changed,
};

static void
netback_init(void *unused)
{
	callout_init(&rx_task_callout, CALLOUT_MPSAFE);

	mmap_vstart = alloc_empty_page_range(MAX_PENDING_REQS);
	BUG_ON(!mmap_vstart);

	pending_cons = 0;
	for (pending_prod = 0; pending_prod < MAX_PENDING_REQS; pending_prod++)
		pending_ring[pending_prod] = pending_prod;

	TASK_INIT(&net_tx_task, 0, net_tx_action, NULL);
	TASK_INIT(&net_rx_task, 0, net_rx_action, NULL);
	mtx_init(&tx_sched_list_lock, "nb_tx_sched_lock", "netback tx sched lock", MTX_DEF);
	mtx_init(&rx_sched_list_lock, "nb_rx_sched_lock", "netback rx sched lock", MTX_DEF);

	DPRINTF("registering %s\n", netback.name);

	xenbus_register_backend(&netback);
}

SYSINIT(xnbedev, SI_SUB_PSEUDO, SI_ORDER_ANY, netback_init, NULL)

static int
vif_add_dev(struct xenbus_device *xdev)
{
	netif_t *netif = xdev->data;
	device_t nexus, ndev;
	devclass_t dc;
	int err = 0;

	mtx_lock(&Giant);

	/* We will add a vif device as a child of nexus0 (for now) */
	if (!(dc = devclass_find("nexus")) ||
		!(nexus = devclass_get_device(dc, 0))) {
		WPRINTF("could not find nexus0!\n");
		err = ENOENT;
		goto done;
	}


	/* Create a newbus device representing the vif */
	ndev = BUS_ADD_CHILD(nexus, 0, "vif", netif->ifp->if_dunit);
	if (!ndev) {
		WPRINTF("could not create newbus device %s!\n", IFNAME(netif));
		err = EFAULT;
		goto done;
	}
	
	netif_get(netif);
	device_set_ivars(ndev, netif);
	netif->ndev = ndev;

	device_probe_and_attach(ndev);

 done:

	mtx_unlock(&Giant);

	return err;
}

enum {
	VIF_SYSCTL_DOMID,
	VIF_SYSCTL_HANDLE,
	VIF_SYSCTL_TXRING,
	VIF_SYSCTL_RXRING,
};

static char *
vif_sysctl_ring_info(netif_t *netif, int cmd)
{
	char *buf = malloc(256, M_DEVBUF, M_WAITOK);
	if (buf) {
		if (!netif->rings_connected)
			sprintf(buf, "rings not connected\n");
		else if (cmd == VIF_SYSCTL_TXRING) {
			netif_tx_back_ring_t *tx = &netif->tx;
			sprintf(buf, "nr_ents=%x req_cons=%x"
					" req_prod=%x req_event=%x"
					" rsp_prod=%x rsp_event=%x",
					tx->nr_ents, tx->req_cons,
					tx->sring->req_prod, tx->sring->req_event,
					tx->sring->rsp_prod, tx->sring->rsp_event);
		} else {
			netif_rx_back_ring_t *rx = &netif->rx;
			sprintf(buf, "nr_ents=%x req_cons=%x"
					" req_prod=%x req_event=%x"
					" rsp_prod=%x rsp_event=%x",
					rx->nr_ents, rx->req_cons,
					rx->sring->req_prod, rx->sring->req_event,
					rx->sring->rsp_prod, rx->sring->rsp_event);
		}
	}
	return buf;
}

static int
vif_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	device_t dev = (device_t)arg1;
	netif_t *netif = (netif_t *)device_get_ivars(dev);
	const char *value;
	char *buf = NULL;
	int err;

	switch (arg2) {
	case VIF_SYSCTL_DOMID:
		return sysctl_handle_int(oidp, NULL, netif->domid, req);
	case VIF_SYSCTL_HANDLE:
		return sysctl_handle_int(oidp, NULL, netif->handle, req);
	case VIF_SYSCTL_TXRING:
	case VIF_SYSCTL_RXRING:
		value = buf = vif_sysctl_ring_info(netif, arg2);
		break;
	default:
		return (EINVAL);
	}

	err = SYSCTL_OUT(req, value, strlen(value));
	if (buf != NULL)
		free(buf, M_DEVBUF);

	return err;
}

/* Newbus vif device driver probe */
static int
vif_probe(device_t dev)
{
	DDPRINTF("vif%d\n", device_get_unit(dev));
	return 0;
}

/* Newbus vif device driver attach */
static int
vif_attach(device_t dev) 
{
	netif_t *netif = (netif_t *)device_get_ivars(dev);
	uint8_t mac[ETHER_ADDR_LEN];

	DDPRINTF("%s\n", IFNAME(netif));

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev), SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "domid", CTLTYPE_INT|CTLFLAG_RD,
	    dev, VIF_SYSCTL_DOMID, vif_sysctl_handler, "I",
	    "domid of frontend");
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev), SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "handle", CTLTYPE_INT|CTLFLAG_RD,
	    dev, VIF_SYSCTL_HANDLE, vif_sysctl_handler, "I",
	    "handle of frontend");
#ifdef XEN_NETBACK_DEBUG
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev), SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "txring", CTLTYPE_STRING | CTLFLAG_RD,
	    dev, VIF_SYSCTL_TXRING, vif_sysctl_handler, "A",
	    "tx ring info");
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev), SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "rxring", CTLTYPE_STRING | CTLFLAG_RD,
	    dev, VIF_SYSCTL_RXRING, vif_sysctl_handler, "A",
	    "rx ring info");
#endif

	memset(mac, 0xff, sizeof(mac));
	mac[0] &= ~0x01;
	
	ether_ifattach(netif->ifp, mac);
	netif->attached = 1;

	connect(netif);

	if (netif->bridge) {
		DPRINTF("Adding %s to bridge %s\n", IFNAME(netif), netif->bridge);
		int err = add_to_bridge(netif->ifp, netif->bridge);
		if (err) {
			WPRINTF("Error adding %s to %s; err=%d\n",
				IFNAME(netif), netif->bridge, err);
		}
	}

	return bus_generic_attach(dev);
}

/* Newbus vif device driver detach */
static int
vif_detach(device_t dev)
{
	netif_t *netif = (netif_t *)device_get_ivars(dev);
	struct ifnet *ifp = netif->ifp;

	DDPRINTF("%s\n", IFNAME(netif));

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	ether_ifdetach(ifp);

	bus_generic_detach(dev);

	netif->attached = 0;

	netif_put(netif);

	return 0;
}

static device_method_t vif_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		vif_probe),
	DEVMETHOD(device_attach, 	vif_attach),
	DEVMETHOD(device_detach,	vif_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	{0, 0}
};

static devclass_t vif_devclass;

static driver_t vif_driver = {
	"vif",
	vif_methods,
	0,
};

DRIVER_MODULE(vif, nexus, vif_driver, vif_devclass, 0, 0);


/*
 * Local variables:
 * mode: C
 * c-set-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: t
 * End:
 */
