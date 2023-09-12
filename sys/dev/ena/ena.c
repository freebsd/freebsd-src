/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015-2021 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
#include "opt_rss.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/time.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/in_cksum.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_vlan_var.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include "ena.h"
#include "ena_datapath.h"
#include "ena_rss.h"
#include "ena_sysctl.h"

#ifdef DEV_NETMAP
#include "ena_netmap.h"
#endif /* DEV_NETMAP */

/*********************************************************
 *  Function prototypes
 *********************************************************/
static int ena_probe(device_t);
static void ena_intr_msix_mgmnt(void *);
static void ena_free_pci_resources(struct ena_adapter *);
static int ena_change_mtu(if_t, int);
static inline void ena_alloc_counters(counter_u64_t *, int);
static inline void ena_free_counters(counter_u64_t *, int);
static inline void ena_reset_counters(counter_u64_t *, int);
static void ena_init_io_rings_common(struct ena_adapter *, struct ena_ring *,
    uint16_t);
static void ena_init_io_rings_basic(struct ena_adapter *);
static void ena_init_io_rings_advanced(struct ena_adapter *);
static void ena_init_io_rings(struct ena_adapter *);
static void ena_free_io_ring_resources(struct ena_adapter *, unsigned int);
static void ena_free_all_io_rings_resources(struct ena_adapter *);
static int ena_setup_tx_dma_tag(struct ena_adapter *);
static int ena_free_tx_dma_tag(struct ena_adapter *);
static int ena_setup_rx_dma_tag(struct ena_adapter *);
static int ena_free_rx_dma_tag(struct ena_adapter *);
static void ena_release_all_tx_dmamap(struct ena_ring *);
static int ena_setup_tx_resources(struct ena_adapter *, int);
static void ena_free_tx_resources(struct ena_adapter *, int);
static int ena_setup_all_tx_resources(struct ena_adapter *);
static void ena_free_all_tx_resources(struct ena_adapter *);
static int ena_setup_rx_resources(struct ena_adapter *, unsigned int);
static void ena_free_rx_resources(struct ena_adapter *, unsigned int);
static int ena_setup_all_rx_resources(struct ena_adapter *);
static void ena_free_all_rx_resources(struct ena_adapter *);
static inline int ena_alloc_rx_mbuf(struct ena_adapter *, struct ena_ring *,
    struct ena_rx_buffer *);
static void ena_free_rx_mbuf(struct ena_adapter *, struct ena_ring *,
    struct ena_rx_buffer *);
static void ena_free_rx_bufs(struct ena_adapter *, unsigned int);
static void ena_refill_all_rx_bufs(struct ena_adapter *);
static void ena_free_all_rx_bufs(struct ena_adapter *);
static void ena_free_tx_bufs(struct ena_adapter *, unsigned int);
static void ena_free_all_tx_bufs(struct ena_adapter *);
static void ena_destroy_all_tx_queues(struct ena_adapter *);
static void ena_destroy_all_rx_queues(struct ena_adapter *);
static void ena_destroy_all_io_queues(struct ena_adapter *);
static int ena_create_io_queues(struct ena_adapter *);
static int ena_handle_msix(void *);
static int ena_enable_msix(struct ena_adapter *);
static void ena_setup_mgmnt_intr(struct ena_adapter *);
static int ena_setup_io_intr(struct ena_adapter *);
static int ena_request_mgmnt_irq(struct ena_adapter *);
static int ena_request_io_irq(struct ena_adapter *);
static void ena_free_mgmnt_irq(struct ena_adapter *);
static void ena_free_io_irq(struct ena_adapter *);
static void ena_free_irqs(struct ena_adapter *);
static void ena_disable_msix(struct ena_adapter *);
static void ena_unmask_all_io_irqs(struct ena_adapter *);
static int ena_up_complete(struct ena_adapter *);
static uint64_t ena_get_counter(if_t, ift_counter);
static int ena_media_change(if_t);
static void ena_media_status(if_t, struct ifmediareq *);
static void ena_init(void *);
static int ena_ioctl(if_t, u_long, caddr_t);
static int ena_get_dev_offloads(struct ena_com_dev_get_features_ctx *);
static void ena_update_host_info(struct ena_admin_host_info *, if_t);
static void ena_update_hwassist(struct ena_adapter *);
static int ena_setup_ifnet(device_t, struct ena_adapter *,
    struct ena_com_dev_get_features_ctx *);
static int ena_enable_wc(device_t, struct resource *);
static int ena_set_queues_placement_policy(device_t, struct ena_com_dev *,
    struct ena_admin_feature_llq_desc *, struct ena_llq_configurations *);
static int ena_map_llq_mem_bar(device_t, struct ena_com_dev *);
static uint32_t ena_calc_max_io_queue_num(device_t, struct ena_com_dev *,
    struct ena_com_dev_get_features_ctx *);
static int ena_calc_io_queue_size(struct ena_calc_queue_size_ctx *);
static void ena_config_host_info(struct ena_com_dev *, device_t);
static int ena_attach(device_t);
static int ena_detach(device_t);
static int ena_device_init(struct ena_adapter *, device_t,
    struct ena_com_dev_get_features_ctx *, int *);
static int ena_enable_msix_and_set_admin_interrupts(struct ena_adapter *);
static void ena_update_on_link_change(void *, struct ena_admin_aenq_entry *);
static void unimplemented_aenq_handler(void *, struct ena_admin_aenq_entry *);
static int ena_copy_eni_metrics(struct ena_adapter *);
static int ena_copy_srd_metrics(struct ena_adapter *);
static int ena_copy_customer_metrics(struct ena_adapter *);
static void ena_timer_service(void *);

static char ena_version[] = ENA_DEVICE_NAME ENA_DRV_MODULE_NAME
    " v" ENA_DRV_MODULE_VERSION;

static ena_vendor_info_t ena_vendor_info_array[] = {
	{ PCI_VENDOR_ID_AMAZON, PCI_DEV_ID_ENA_PF, 0 },
	{ PCI_VENDOR_ID_AMAZON, PCI_DEV_ID_ENA_PF_RSERV0, 0 },
	{ PCI_VENDOR_ID_AMAZON, PCI_DEV_ID_ENA_VF, 0 },
	{ PCI_VENDOR_ID_AMAZON, PCI_DEV_ID_ENA_VF_RSERV0, 0 },
	/* Last entry */
	{ 0, 0, 0 }
};

struct sx ena_global_lock;

/*
 * Contains pointers to event handlers, e.g. link state chage.
 */
static struct ena_aenq_handlers aenq_handlers;

void
ena_dmamap_callback(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error != 0)
		return;
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

int
ena_dma_alloc(device_t dmadev, bus_size_t size, ena_mem_handle_t *dma,
    int mapflags, bus_size_t alignment, int domain)
{
	struct ena_adapter *adapter = device_get_softc(dmadev);
	device_t pdev = adapter->pdev;
	uint32_t maxsize;
	uint64_t dma_space_addr;
	int error;

	maxsize = ((size - 1) / PAGE_SIZE + 1) * PAGE_SIZE;

	dma_space_addr = ENA_DMA_BIT_MASK(adapter->dma_width);
	if (unlikely(dma_space_addr == 0))
		dma_space_addr = BUS_SPACE_MAXADDR;

	error = bus_dma_tag_create(bus_get_dma_tag(dmadev), /* parent */
	    alignment, 0,      /* alignment, bounds 		*/
	    dma_space_addr,    /* lowaddr of exclusion window	*/
	    BUS_SPACE_MAXADDR, /* highaddr of exclusion window	*/
	    NULL, NULL,	       /* filter, filterarg 		*/
	    maxsize,	       /* maxsize 			*/
	    1,		       /* nsegments 			*/
	    maxsize,	       /* maxsegsize 			*/
	    BUS_DMA_ALLOCNOW,  /* flags 			*/
	    NULL,	       /* lockfunc 			*/
	    NULL,	       /* lockarg 			*/
	    &dma->tag);
	if (unlikely(error != 0)) {
		ena_log(pdev, ERR, "bus_dma_tag_create failed: %d\n", error);
		goto fail_tag;
	}

	error = bus_dma_tag_set_domain(dma->tag, domain);
	if (unlikely(error != 0)) {
		ena_log(pdev, ERR, "bus_dma_tag_set_domain failed: %d\n",
		    error);
		goto fail_map_create;
	}

	error = bus_dmamem_alloc(dma->tag, (void **)&dma->vaddr,
	    BUS_DMA_COHERENT | BUS_DMA_ZERO, &dma->map);
	if (unlikely(error != 0)) {
		ena_log(pdev, ERR, "bus_dmamem_alloc(%ju) failed: %d\n",
		    (uintmax_t)size, error);
		goto fail_map_create;
	}

	dma->paddr = 0;
	error = bus_dmamap_load(dma->tag, dma->map, dma->vaddr, size,
	    ena_dmamap_callback, &dma->paddr, mapflags);
	if (unlikely((error != 0) || (dma->paddr == 0))) {
		ena_log(pdev, ERR, "bus_dmamap_load failed: %d\n", error);
		goto fail_map_load;
	}

	bus_dmamap_sync(dma->tag, dma->map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);

fail_map_load:
	bus_dmamem_free(dma->tag, dma->vaddr, dma->map);
fail_map_create:
	bus_dma_tag_destroy(dma->tag);
fail_tag:
	dma->tag = NULL;
	dma->vaddr = NULL;
	dma->paddr = 0;

	return (error);
}

static void
ena_free_pci_resources(struct ena_adapter *adapter)
{
	device_t pdev = adapter->pdev;

	if (adapter->memory != NULL) {
		bus_release_resource(pdev, SYS_RES_MEMORY,
		    PCIR_BAR(ENA_MEM_BAR), adapter->memory);
	}

	if (adapter->registers != NULL) {
		bus_release_resource(pdev, SYS_RES_MEMORY,
		    PCIR_BAR(ENA_REG_BAR), adapter->registers);
	}

	if (adapter->msix != NULL) {
		bus_release_resource(pdev, SYS_RES_MEMORY, adapter->msix_rid,
		    adapter->msix);
	}
}

static int
ena_probe(device_t dev)
{
	ena_vendor_info_t *ent;
	uint16_t pci_vendor_id = 0;
	uint16_t pci_device_id = 0;

	pci_vendor_id = pci_get_vendor(dev);
	pci_device_id = pci_get_device(dev);

	ent = ena_vendor_info_array;
	while (ent->vendor_id != 0) {
		if ((pci_vendor_id == ent->vendor_id) &&
		    (pci_device_id == ent->device_id)) {
			ena_log_raw(DBG, "vendor=%x device=%x\n", pci_vendor_id,
			    pci_device_id);

			device_set_desc(dev, ENA_DEVICE_DESC);
			return (BUS_PROBE_DEFAULT);
		}

		ent++;
	}

	return (ENXIO);
}

static int
ena_change_mtu(if_t ifp, int new_mtu)
{
	struct ena_adapter *adapter = if_getsoftc(ifp);
	device_t pdev = adapter->pdev;
	int rc;

	if ((new_mtu > adapter->max_mtu) || (new_mtu < ENA_MIN_MTU)) {
		ena_log(pdev, ERR, "Invalid MTU setting. new_mtu: %d max mtu: %d min mtu: %d\n",
		    new_mtu, adapter->max_mtu, ENA_MIN_MTU);
		return (EINVAL);
	}

	rc = ena_com_set_dev_mtu(adapter->ena_dev, new_mtu);
	if (likely(rc == 0)) {
		ena_log(pdev, DBG, "set MTU to %d\n", new_mtu);
		if_setmtu(ifp, new_mtu);
	} else {
		ena_log(pdev, ERR, "Failed to set MTU to %d\n", new_mtu);
	}

	return (rc);
}

static inline void
ena_alloc_counters(counter_u64_t *begin, int size)
{
	counter_u64_t *end = (counter_u64_t *)((char *)begin + size);

	for (; begin < end; ++begin)
		*begin = counter_u64_alloc(M_WAITOK);
}

static inline void
ena_free_counters(counter_u64_t *begin, int size)
{
	counter_u64_t *end = (counter_u64_t *)((char *)begin + size);

	for (; begin < end; ++begin)
		counter_u64_free(*begin);
}

static inline void
ena_reset_counters(counter_u64_t *begin, int size)
{
	counter_u64_t *end = (counter_u64_t *)((char *)begin + size);

	for (; begin < end; ++begin)
		counter_u64_zero(*begin);
}

static void
ena_init_io_rings_common(struct ena_adapter *adapter, struct ena_ring *ring,
    uint16_t qid)
{
	ring->qid = qid;
	ring->adapter = adapter;
	ring->ena_dev = adapter->ena_dev;
	atomic_store_8(&ring->first_interrupt, 0);
	ring->no_interrupt_event_cnt = 0;
}

static void
ena_init_io_rings_basic(struct ena_adapter *adapter)
{
	struct ena_com_dev *ena_dev;
	struct ena_ring *txr, *rxr;
	struct ena_que *que;
	int i;

	ena_dev = adapter->ena_dev;

	for (i = 0; i < adapter->num_io_queues; i++) {
		txr = &adapter->tx_ring[i];
		rxr = &adapter->rx_ring[i];

		/* TX/RX common ring state */
		ena_init_io_rings_common(adapter, txr, i);
		ena_init_io_rings_common(adapter, rxr, i);

		/* TX specific ring state */
		txr->tx_max_header_size = ena_dev->tx_max_header_size;
		txr->tx_mem_queue_type = ena_dev->tx_mem_queue_type;

		que = &adapter->que[i];
		que->adapter = adapter;
		que->id = i;
		que->tx_ring = txr;
		que->rx_ring = rxr;

		txr->que = que;
		rxr->que = que;

		rxr->empty_rx_queue = 0;
		rxr->rx_mbuf_sz = ena_mbuf_sz;
	}
}

static void
ena_init_io_rings_advanced(struct ena_adapter *adapter)
{
	struct ena_ring *txr, *rxr;
	int i;

	for (i = 0; i < adapter->num_io_queues; i++) {
		txr = &adapter->tx_ring[i];
		rxr = &adapter->rx_ring[i];

		/* Allocate a buf ring */
		txr->buf_ring_size = adapter->buf_ring_size;
		txr->br = buf_ring_alloc(txr->buf_ring_size, M_DEVBUF, M_WAITOK,
		    &txr->ring_mtx);

		/* Allocate Tx statistics. */
		ena_alloc_counters((counter_u64_t *)&txr->tx_stats,
		    sizeof(txr->tx_stats));
		txr->tx_last_cleanup_ticks = ticks;

		/* Allocate Rx statistics. */
		ena_alloc_counters((counter_u64_t *)&rxr->rx_stats,
		    sizeof(rxr->rx_stats));

		/* Initialize locks */
		snprintf(txr->mtx_name, nitems(txr->mtx_name), "%s:tx(%d)",
		    device_get_nameunit(adapter->pdev), i);
		snprintf(rxr->mtx_name, nitems(rxr->mtx_name), "%s:rx(%d)",
		    device_get_nameunit(adapter->pdev), i);

		mtx_init(&txr->ring_mtx, txr->mtx_name, NULL, MTX_DEF);
	}
}

static void
ena_init_io_rings(struct ena_adapter *adapter)
{
	/*
	 * IO rings initialization can be divided into the 2 steps:
	 *   1. Initialize variables and fields with initial values and copy
	 *      them from adapter/ena_dev (basic)
	 *   2. Allocate mutex, counters and buf_ring (advanced)
	 */
	ena_init_io_rings_basic(adapter);
	ena_init_io_rings_advanced(adapter);
}

static void
ena_free_io_ring_resources(struct ena_adapter *adapter, unsigned int qid)
{
	struct ena_ring *txr = &adapter->tx_ring[qid];
	struct ena_ring *rxr = &adapter->rx_ring[qid];

	ena_free_counters((counter_u64_t *)&txr->tx_stats,
	    sizeof(txr->tx_stats));
	ena_free_counters((counter_u64_t *)&rxr->rx_stats,
	    sizeof(rxr->rx_stats));

	ENA_RING_MTX_LOCK(txr);
	drbr_free(txr->br, M_DEVBUF);
	ENA_RING_MTX_UNLOCK(txr);

	mtx_destroy(&txr->ring_mtx);
}

static void
ena_free_all_io_rings_resources(struct ena_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_io_queues; i++)
		ena_free_io_ring_resources(adapter, i);
}

static int
ena_setup_tx_dma_tag(struct ena_adapter *adapter)
{
	int ret;

	/* Create DMA tag for Tx buffers */
	ret = bus_dma_tag_create(bus_get_dma_tag(adapter->pdev),
	    1, 0,				  /* alignment, bounds 	     */
	    ENA_DMA_BIT_MASK(adapter->dma_width), /* lowaddr of excl window  */
	    BUS_SPACE_MAXADDR,			  /* highaddr of excl window */
	    NULL, NULL,				  /* filter, filterarg 	     */
	    ENA_TSO_MAXSIZE,			  /* maxsize 		     */
	    adapter->max_tx_sgl_size - 1,	  /* nsegments 		     */
	    ENA_TSO_MAXSIZE,			  /* maxsegsize 	     */
	    0,					  /* flags 		     */
	    NULL,				  /* lockfunc 		     */
	    NULL,				  /* lockfuncarg 	     */
	    &adapter->tx_buf_tag);

	return (ret);
}

static int
ena_free_tx_dma_tag(struct ena_adapter *adapter)
{
	int ret;

	ret = bus_dma_tag_destroy(adapter->tx_buf_tag);

	if (likely(ret == 0))
		adapter->tx_buf_tag = NULL;

	return (ret);
}

static int
ena_setup_rx_dma_tag(struct ena_adapter *adapter)
{
	int ret;

	/* Create DMA tag for Rx buffers*/
	ret = bus_dma_tag_create(bus_get_dma_tag(adapter->pdev), /* parent   */
	    1, 0,				  /* alignment, bounds 	     */
	    ENA_DMA_BIT_MASK(adapter->dma_width), /* lowaddr of excl window  */
	    BUS_SPACE_MAXADDR,			  /* highaddr of excl window */
	    NULL, NULL,				  /* filter, filterarg 	     */
	    ena_mbuf_sz,			  /* maxsize 		     */
	    adapter->max_rx_sgl_size,		  /* nsegments 		     */
	    ena_mbuf_sz,			  /* maxsegsize 	     */
	    0,					  /* flags 		     */
	    NULL,				  /* lockfunc 		     */
	    NULL,				  /* lockarg 		     */
	    &adapter->rx_buf_tag);

	return (ret);
}

static int
ena_free_rx_dma_tag(struct ena_adapter *adapter)
{
	int ret;

	ret = bus_dma_tag_destroy(adapter->rx_buf_tag);

	if (likely(ret == 0))
		adapter->rx_buf_tag = NULL;

	return (ret);
}

static void
ena_release_all_tx_dmamap(struct ena_ring *tx_ring)
{
	struct ena_adapter *adapter = tx_ring->adapter;
	struct ena_tx_buffer *tx_info;
	bus_dma_tag_t tx_tag = adapter->tx_buf_tag;
	int i;
#ifdef DEV_NETMAP
	struct ena_netmap_tx_info *nm_info;
	int j;
#endif /* DEV_NETMAP */

	for (i = 0; i < tx_ring->ring_size; ++i) {
		tx_info = &tx_ring->tx_buffer_info[i];
#ifdef DEV_NETMAP
		if (adapter->ifp->if_capenable & IFCAP_NETMAP) {
			nm_info = &tx_info->nm_info;
			for (j = 0; j < ENA_PKT_MAX_BUFS; ++j) {
				if (nm_info->map_seg[j] != NULL) {
					bus_dmamap_destroy(tx_tag,
					    nm_info->map_seg[j]);
					nm_info->map_seg[j] = NULL;
				}
			}
		}
#endif /* DEV_NETMAP */
		if (tx_info->dmamap != NULL) {
			bus_dmamap_destroy(tx_tag, tx_info->dmamap);
			tx_info->dmamap = NULL;
		}
	}
}

/**
 * ena_setup_tx_resources - allocate Tx resources (Descriptors)
 * @adapter: network interface device structure
 * @qid: queue index
 *
 * Returns 0 on success, otherwise on failure.
 **/
static int
ena_setup_tx_resources(struct ena_adapter *adapter, int qid)
{
	device_t pdev = adapter->pdev;
	char thread_name[MAXCOMLEN + 1];
	struct ena_que *que = &adapter->que[qid];
	struct ena_ring *tx_ring = que->tx_ring;
	cpuset_t *cpu_mask = NULL;
	int size, i, err;
#ifdef DEV_NETMAP
	bus_dmamap_t *map;
	int j;

	ena_netmap_reset_tx_ring(adapter, qid);
#endif /* DEV_NETMAP */

	size = sizeof(struct ena_tx_buffer) * tx_ring->ring_size;

	tx_ring->tx_buffer_info = malloc(size, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (unlikely(tx_ring->tx_buffer_info == NULL))
		return (ENOMEM);

	size = sizeof(uint16_t) * tx_ring->ring_size;
	tx_ring->free_tx_ids = malloc(size, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (unlikely(tx_ring->free_tx_ids == NULL))
		goto err_buf_info_free;

	size = tx_ring->tx_max_header_size;
	tx_ring->push_buf_intermediate_buf = malloc(size, M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (unlikely(tx_ring->push_buf_intermediate_buf == NULL))
		goto err_tx_ids_free;

	/* Req id stack for TX OOO completions */
	for (i = 0; i < tx_ring->ring_size; i++)
		tx_ring->free_tx_ids[i] = i;

	/* Reset TX statistics. */
	ena_reset_counters((counter_u64_t *)&tx_ring->tx_stats,
	    sizeof(tx_ring->tx_stats));

	tx_ring->next_to_use = 0;
	tx_ring->next_to_clean = 0;
	tx_ring->acum_pkts = 0;

	/* Make sure that drbr is empty */
	ENA_RING_MTX_LOCK(tx_ring);
	drbr_flush(adapter->ifp, tx_ring->br);
	ENA_RING_MTX_UNLOCK(tx_ring);

	/* ... and create the buffer DMA maps */
	for (i = 0; i < tx_ring->ring_size; i++) {
		err = bus_dmamap_create(adapter->tx_buf_tag, 0,
		    &tx_ring->tx_buffer_info[i].dmamap);
		if (unlikely(err != 0)) {
			ena_log(pdev, ERR,
			    "Unable to create Tx DMA map for buffer %d\n", i);
			goto err_map_release;
		}

#ifdef DEV_NETMAP
		if (adapter->ifp->if_capenable & IFCAP_NETMAP) {
			map = tx_ring->tx_buffer_info[i].nm_info.map_seg;
			for (j = 0; j < ENA_PKT_MAX_BUFS; j++) {
				err = bus_dmamap_create(adapter->tx_buf_tag, 0,
				    &map[j]);
				if (unlikely(err != 0)) {
					ena_log(pdev, ERR,
					    "Unable to create Tx DMA for buffer %d %d\n",
					    i, j);
					goto err_map_release;
				}
			}
		}
#endif /* DEV_NETMAP */
	}

	/* Allocate taskqueues */
	TASK_INIT(&tx_ring->enqueue_task, 0, ena_deferred_mq_start, tx_ring);
	tx_ring->enqueue_tq = taskqueue_create_fast("ena_tx_enque", M_NOWAIT,
	    taskqueue_thread_enqueue, &tx_ring->enqueue_tq);
	if (unlikely(tx_ring->enqueue_tq == NULL)) {
		ena_log(pdev, ERR,
		    "Unable to create taskqueue for enqueue task\n");
		i = tx_ring->ring_size;
		goto err_map_release;
	}

	tx_ring->running = true;

#ifdef RSS
	cpu_mask = &que->cpu_mask;
	snprintf(thread_name, sizeof(thread_name), "%s txeq %d",
	    device_get_nameunit(adapter->pdev), que->cpu);
#else
	snprintf(thread_name, sizeof(thread_name), "%s txeq %d",
	    device_get_nameunit(adapter->pdev), que->id);
#endif
	taskqueue_start_threads_cpuset(&tx_ring->enqueue_tq, 1, PI_NET,
	    cpu_mask, "%s", thread_name);

	return (0);

err_map_release:
	ena_release_all_tx_dmamap(tx_ring);
err_tx_ids_free:
	free(tx_ring->free_tx_ids, M_DEVBUF);
	tx_ring->free_tx_ids = NULL;
err_buf_info_free:
	free(tx_ring->tx_buffer_info, M_DEVBUF);
	tx_ring->tx_buffer_info = NULL;

	return (ENOMEM);
}

/**
 * ena_free_tx_resources - Free Tx Resources per Queue
 * @adapter: network interface device structure
 * @qid: queue index
 *
 * Free all transmit software resources
 **/
static void
ena_free_tx_resources(struct ena_adapter *adapter, int qid)
{
	struct ena_ring *tx_ring = &adapter->tx_ring[qid];
#ifdef DEV_NETMAP
	struct ena_netmap_tx_info *nm_info;
	int j;
#endif /* DEV_NETMAP */

	while (taskqueue_cancel(tx_ring->enqueue_tq, &tx_ring->enqueue_task, NULL))
		taskqueue_drain(tx_ring->enqueue_tq, &tx_ring->enqueue_task);

	taskqueue_free(tx_ring->enqueue_tq);

	ENA_RING_MTX_LOCK(tx_ring);
	/* Flush buffer ring, */
	drbr_flush(adapter->ifp, tx_ring->br);

	/* Free buffer DMA maps, */
	for (int i = 0; i < tx_ring->ring_size; i++) {
		bus_dmamap_sync(adapter->tx_buf_tag,
		    tx_ring->tx_buffer_info[i].dmamap, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(adapter->tx_buf_tag,
		    tx_ring->tx_buffer_info[i].dmamap);
		bus_dmamap_destroy(adapter->tx_buf_tag,
		    tx_ring->tx_buffer_info[i].dmamap);

#ifdef DEV_NETMAP
		if (adapter->ifp->if_capenable & IFCAP_NETMAP) {
			nm_info = &tx_ring->tx_buffer_info[i].nm_info;
			for (j = 0; j < ENA_PKT_MAX_BUFS; j++) {
				if (nm_info->socket_buf_idx[j] != 0) {
					bus_dmamap_sync(adapter->tx_buf_tag,
					    nm_info->map_seg[j],
					    BUS_DMASYNC_POSTWRITE);
					ena_netmap_unload(adapter,
					    nm_info->map_seg[j]);
				}
				bus_dmamap_destroy(adapter->tx_buf_tag,
				    nm_info->map_seg[j]);
				nm_info->socket_buf_idx[j] = 0;
			}
		}
#endif /* DEV_NETMAP */

		m_freem(tx_ring->tx_buffer_info[i].mbuf);
		tx_ring->tx_buffer_info[i].mbuf = NULL;
	}
	ENA_RING_MTX_UNLOCK(tx_ring);

	/* And free allocated memory. */
	free(tx_ring->tx_buffer_info, M_DEVBUF);
	tx_ring->tx_buffer_info = NULL;

	free(tx_ring->free_tx_ids, M_DEVBUF);
	tx_ring->free_tx_ids = NULL;

	free(tx_ring->push_buf_intermediate_buf, M_DEVBUF);
	tx_ring->push_buf_intermediate_buf = NULL;
}

/**
 * ena_setup_all_tx_resources - allocate all queues Tx resources
 * @adapter: network interface device structure
 *
 * Returns 0 on success, otherwise on failure.
 **/
static int
ena_setup_all_tx_resources(struct ena_adapter *adapter)
{
	int i, rc;

	for (i = 0; i < adapter->num_io_queues; i++) {
		rc = ena_setup_tx_resources(adapter, i);
		if (rc != 0) {
			ena_log(adapter->pdev, ERR,
			    "Allocation for Tx Queue %u failed\n", i);
			goto err_setup_tx;
		}
	}

	return (0);

err_setup_tx:
	/* Rewind the index freeing the rings as we go */
	while (i--)
		ena_free_tx_resources(adapter, i);
	return (rc);
}

/**
 * ena_free_all_tx_resources - Free Tx Resources for All Queues
 * @adapter: network interface device structure
 *
 * Free all transmit software resources
 **/
static void
ena_free_all_tx_resources(struct ena_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_io_queues; i++)
		ena_free_tx_resources(adapter, i);
}

/**
 * ena_setup_rx_resources - allocate Rx resources (Descriptors)
 * @adapter: network interface device structure
 * @qid: queue index
 *
 * Returns 0 on success, otherwise on failure.
 **/
static int
ena_setup_rx_resources(struct ena_adapter *adapter, unsigned int qid)
{
	device_t pdev = adapter->pdev;
	struct ena_que *que = &adapter->que[qid];
	struct ena_ring *rx_ring = que->rx_ring;
	int size, err, i;

	size = sizeof(struct ena_rx_buffer) * rx_ring->ring_size;

#ifdef DEV_NETMAP
	ena_netmap_reset_rx_ring(adapter, qid);
	rx_ring->initialized = false;
#endif /* DEV_NETMAP */

	/*
	 * Alloc extra element so in rx path
	 * we can always prefetch rx_info + 1
	 */
	size += sizeof(struct ena_rx_buffer);

	rx_ring->rx_buffer_info = malloc(size, M_DEVBUF, M_WAITOK | M_ZERO);

	size = sizeof(uint16_t) * rx_ring->ring_size;
	rx_ring->free_rx_ids = malloc(size, M_DEVBUF, M_WAITOK);

	for (i = 0; i < rx_ring->ring_size; i++)
		rx_ring->free_rx_ids[i] = i;

	/* Reset RX statistics. */
	ena_reset_counters((counter_u64_t *)&rx_ring->rx_stats,
	    sizeof(rx_ring->rx_stats));

	rx_ring->next_to_clean = 0;
	rx_ring->next_to_use = 0;

	/* ... and create the buffer DMA maps */
	for (i = 0; i < rx_ring->ring_size; i++) {
		err = bus_dmamap_create(adapter->rx_buf_tag, 0,
		    &(rx_ring->rx_buffer_info[i].map));
		if (err != 0) {
			ena_log(pdev, ERR,
			    "Unable to create Rx DMA map for buffer %d\n", i);
			goto err_buf_info_unmap;
		}
	}

	/* Create LRO for the ring */
	if ((adapter->ifp->if_capenable & IFCAP_LRO) != 0) {
		int err = tcp_lro_init(&rx_ring->lro);
		if (err != 0) {
			ena_log(pdev, ERR, "LRO[%d] Initialization failed!\n",
			    qid);
		} else {
			ena_log(pdev, DBG, "RX Soft LRO[%d] Initialized\n",
			    qid);
			rx_ring->lro.ifp = adapter->ifp;
		}
	}

	return (0);

err_buf_info_unmap:
	while (i--) {
		bus_dmamap_destroy(adapter->rx_buf_tag,
		    rx_ring->rx_buffer_info[i].map);
	}

	free(rx_ring->free_rx_ids, M_DEVBUF);
	rx_ring->free_rx_ids = NULL;
	free(rx_ring->rx_buffer_info, M_DEVBUF);
	rx_ring->rx_buffer_info = NULL;
	return (ENOMEM);
}

/**
 * ena_free_rx_resources - Free Rx Resources
 * @adapter: network interface device structure
 * @qid: queue index
 *
 * Free all receive software resources
 **/
static void
ena_free_rx_resources(struct ena_adapter *adapter, unsigned int qid)
{
	struct ena_ring *rx_ring = &adapter->rx_ring[qid];

	/* Free buffer DMA maps, */
	for (int i = 0; i < rx_ring->ring_size; i++) {
		bus_dmamap_sync(adapter->rx_buf_tag,
		    rx_ring->rx_buffer_info[i].map, BUS_DMASYNC_POSTREAD);
		m_freem(rx_ring->rx_buffer_info[i].mbuf);
		rx_ring->rx_buffer_info[i].mbuf = NULL;
		bus_dmamap_unload(adapter->rx_buf_tag,
		    rx_ring->rx_buffer_info[i].map);
		bus_dmamap_destroy(adapter->rx_buf_tag,
		    rx_ring->rx_buffer_info[i].map);
	}

	/* free LRO resources, */
	tcp_lro_free(&rx_ring->lro);

	/* free allocated memory */
	free(rx_ring->rx_buffer_info, M_DEVBUF);
	rx_ring->rx_buffer_info = NULL;

	free(rx_ring->free_rx_ids, M_DEVBUF);
	rx_ring->free_rx_ids = NULL;
}

/**
 * ena_setup_all_rx_resources - allocate all queues Rx resources
 * @adapter: network interface device structure
 *
 * Returns 0 on success, otherwise on failure.
 **/
static int
ena_setup_all_rx_resources(struct ena_adapter *adapter)
{
	int i, rc = 0;

	for (i = 0; i < adapter->num_io_queues; i++) {
		rc = ena_setup_rx_resources(adapter, i);
		if (rc != 0) {
			ena_log(adapter->pdev, ERR,
			    "Allocation for Rx Queue %u failed\n", i);
			goto err_setup_rx;
		}
	}
	return (0);

err_setup_rx:
	/* rewind the index freeing the rings as we go */
	while (i--)
		ena_free_rx_resources(adapter, i);
	return (rc);
}

/**
 * ena_free_all_rx_resources - Free Rx resources for all queues
 * @adapter: network interface device structure
 *
 * Free all receive software resources
 **/
static void
ena_free_all_rx_resources(struct ena_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_io_queues; i++)
		ena_free_rx_resources(adapter, i);
}

static inline int
ena_alloc_rx_mbuf(struct ena_adapter *adapter, struct ena_ring *rx_ring,
    struct ena_rx_buffer *rx_info)
{
	device_t pdev = adapter->pdev;
	struct ena_com_buf *ena_buf;
	bus_dma_segment_t segs[1];
	int nsegs, error;
	int mlen;

	/* if previous allocated frag is not used */
	if (unlikely(rx_info->mbuf != NULL))
		return (0);

	/* Get mbuf using UMA allocator */
	rx_info->mbuf = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR,
	    rx_ring->rx_mbuf_sz);

	if (unlikely(rx_info->mbuf == NULL)) {
		counter_u64_add(rx_ring->rx_stats.mjum_alloc_fail, 1);
		rx_info->mbuf = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if (unlikely(rx_info->mbuf == NULL)) {
			counter_u64_add(rx_ring->rx_stats.mbuf_alloc_fail, 1);
			return (ENOMEM);
		}
		mlen = MCLBYTES;
	} else {
		mlen = rx_ring->rx_mbuf_sz;
	}
	/* Set mbuf length*/
	rx_info->mbuf->m_pkthdr.len = rx_info->mbuf->m_len = mlen;

	/* Map packets for DMA */
	ena_log(pdev, DBG,
	    "Using tag %p for buffers' DMA mapping, mbuf %p len: %d\n",
	    adapter->rx_buf_tag, rx_info->mbuf, rx_info->mbuf->m_len);
	error = bus_dmamap_load_mbuf_sg(adapter->rx_buf_tag, rx_info->map,
	    rx_info->mbuf, segs, &nsegs, BUS_DMA_NOWAIT);
	if (unlikely((error != 0) || (nsegs != 1))) {
		ena_log(pdev, WARN,
		    "failed to map mbuf, error: %d, nsegs: %d\n", error, nsegs);
		counter_u64_add(rx_ring->rx_stats.dma_mapping_err, 1);
		goto exit;
	}

	bus_dmamap_sync(adapter->rx_buf_tag, rx_info->map, BUS_DMASYNC_PREREAD);

	ena_buf = &rx_info->ena_buf;
	ena_buf->paddr = segs[0].ds_addr;
	ena_buf->len = mlen;

	ena_log(pdev, DBG,
	    "ALLOC RX BUF: mbuf %p, rx_info %p, len %d, paddr %#jx\n",
	    rx_info->mbuf, rx_info, ena_buf->len, (uintmax_t)ena_buf->paddr);

	return (0);

exit:
	m_freem(rx_info->mbuf);
	rx_info->mbuf = NULL;
	return (EFAULT);
}

static void
ena_free_rx_mbuf(struct ena_adapter *adapter, struct ena_ring *rx_ring,
    struct ena_rx_buffer *rx_info)
{
	if (rx_info->mbuf == NULL) {
		ena_log(adapter->pdev, WARN,
		    "Trying to free unallocated buffer\n");
		return;
	}

	bus_dmamap_sync(adapter->rx_buf_tag, rx_info->map,
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(adapter->rx_buf_tag, rx_info->map);
	m_freem(rx_info->mbuf);
	rx_info->mbuf = NULL;
}

/**
 * ena_refill_rx_bufs - Refills ring with descriptors
 * @rx_ring: the ring which we want to feed with free descriptors
 * @num: number of descriptors to refill
 * Refills the ring with newly allocated DMA-mapped mbufs for receiving
 **/
int
ena_refill_rx_bufs(struct ena_ring *rx_ring, uint32_t num)
{
	struct ena_adapter *adapter = rx_ring->adapter;
	device_t pdev = adapter->pdev;
	uint16_t next_to_use, req_id;
	uint32_t i;
	int rc;

	ena_log_io(adapter->pdev, DBG, "refill qid: %d\n", rx_ring->qid);

	next_to_use = rx_ring->next_to_use;

	for (i = 0; i < num; i++) {
		struct ena_rx_buffer *rx_info;

		ena_log_io(pdev, DBG, "RX buffer - next to use: %d\n",
		    next_to_use);

		req_id = rx_ring->free_rx_ids[next_to_use];
		rx_info = &rx_ring->rx_buffer_info[req_id];
#ifdef DEV_NETMAP
		if (ena_rx_ring_in_netmap(adapter, rx_ring->qid))
			rc = ena_netmap_alloc_rx_slot(adapter, rx_ring,
			    rx_info);
		else
#endif /* DEV_NETMAP */
			rc = ena_alloc_rx_mbuf(adapter, rx_ring, rx_info);
		if (unlikely(rc != 0)) {
			ena_log_io(pdev, WARN,
			    "failed to alloc buffer for rx queue %d\n",
			    rx_ring->qid);
			break;
		}
		rc = ena_com_add_single_rx_desc(rx_ring->ena_com_io_sq,
		    &rx_info->ena_buf, req_id);
		if (unlikely(rc != 0)) {
			ena_log_io(pdev, WARN,
			    "failed to add buffer for rx queue %d\n",
			    rx_ring->qid);
			break;
		}
		next_to_use = ENA_RX_RING_IDX_NEXT(next_to_use,
		    rx_ring->ring_size);
	}

	if (unlikely(i < num)) {
		counter_u64_add(rx_ring->rx_stats.refil_partial, 1);
		ena_log_io(pdev, WARN,
		    "refilled rx qid %d with only %d mbufs (from %d)\n",
		    rx_ring->qid, i, num);
	}

	if (likely(i != 0))
		ena_com_write_sq_doorbell(rx_ring->ena_com_io_sq);

	rx_ring->next_to_use = next_to_use;
	return (i);
}

int
ena_update_buf_ring_size(struct ena_adapter *adapter,
    uint32_t new_buf_ring_size)
{
	uint32_t old_buf_ring_size;
	int rc = 0;
	bool dev_was_up;

	old_buf_ring_size = adapter->buf_ring_size;
	adapter->buf_ring_size = new_buf_ring_size;

	dev_was_up = ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter);
	ena_down(adapter);

	/* Reconfigure buf ring for all Tx rings. */
	ena_free_all_io_rings_resources(adapter);
	ena_init_io_rings_advanced(adapter);
	if (dev_was_up) {
		/*
		 * If ena_up() fails, it's not because of recent buf_ring size
		 * changes. Because of that, we just want to revert old drbr
		 * value and trigger the reset because something else had to
		 * go wrong.
		 */
		rc = ena_up(adapter);
		if (unlikely(rc != 0)) {
			ena_log(adapter->pdev, ERR,
			    "Failed to configure device after setting new drbr size: %u. Reverting old value: %u and triggering the reset\n",
			    new_buf_ring_size, old_buf_ring_size);

			/* Revert old size and trigger the reset */
			adapter->buf_ring_size = old_buf_ring_size;
			ena_free_all_io_rings_resources(adapter);
			ena_init_io_rings_advanced(adapter);

			ENA_FLAG_SET_ATOMIC(ENA_FLAG_DEV_UP_BEFORE_RESET,
			    adapter);
			ena_trigger_reset(adapter, ENA_REGS_RESET_OS_TRIGGER);
		}
	}

	return (rc);
}

int
ena_update_queue_size(struct ena_adapter *adapter, uint32_t new_tx_size,
    uint32_t new_rx_size)
{
	uint32_t old_tx_size, old_rx_size;
	int rc = 0;
	bool dev_was_up;

	old_tx_size = adapter->requested_tx_ring_size;
	old_rx_size = adapter->requested_rx_ring_size;
	adapter->requested_tx_ring_size = new_tx_size;
	adapter->requested_rx_ring_size = new_rx_size;

	dev_was_up = ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter);
	ena_down(adapter);

	/* Configure queues with new size. */
	ena_init_io_rings_basic(adapter);
	if (dev_was_up) {
		rc = ena_up(adapter);
		if (unlikely(rc != 0)) {
			ena_log(adapter->pdev, ERR,
			    "Failed to configure device with the new sizes - Tx: %u Rx: %u. Reverting old values - Tx: %u Rx: %u\n",
			    new_tx_size, new_rx_size, old_tx_size, old_rx_size);

			/* Revert old size. */
			adapter->requested_tx_ring_size = old_tx_size;
			adapter->requested_rx_ring_size = old_rx_size;
			ena_init_io_rings_basic(adapter);

			/* And try again. */
			rc = ena_up(adapter);
			if (unlikely(rc != 0)) {
				ena_log(adapter->pdev, ERR,
				    "Failed to revert old queue sizes. Triggering device reset.\n");
				/*
				 * If we've failed again, something had to go
				 * wrong. After reset, the device should try to
				 * go up
				 */
				ENA_FLAG_SET_ATOMIC(
				    ENA_FLAG_DEV_UP_BEFORE_RESET, adapter);
				ena_trigger_reset(adapter,
				    ENA_REGS_RESET_OS_TRIGGER);
			}
		}
	}

	return (rc);
}

static void
ena_update_io_rings(struct ena_adapter *adapter, uint32_t num)
{
	ena_free_all_io_rings_resources(adapter);
	/* Force indirection table to be reinitialized */
	ena_com_rss_destroy(adapter->ena_dev);

	adapter->num_io_queues = num;
	ena_init_io_rings(adapter);
}

int
ena_update_base_cpu(struct ena_adapter *adapter, int new_num)
{
	int old_num;
	int rc = 0;
	bool dev_was_up;

	dev_was_up = ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter);
	old_num = adapter->irq_cpu_base;

	ena_down(adapter);

	adapter->irq_cpu_base = new_num;

	if (dev_was_up) {
		rc = ena_up(adapter);
		if (unlikely(rc != 0)) {
			ena_log(adapter->pdev, ERR,
			    "Failed to configure device %d IRQ base CPU. "
			    "Reverting to previous value: %d\n",
			    new_num, old_num);

			adapter->irq_cpu_base = old_num;

			rc = ena_up(adapter);
			if (unlikely(rc != 0)) {
				ena_log(adapter->pdev, ERR,
				    "Failed to revert to previous setup."
				    "Triggering device reset.\n");
				ENA_FLAG_SET_ATOMIC(
				    ENA_FLAG_DEV_UP_BEFORE_RESET, adapter);
				ena_trigger_reset(adapter,
				    ENA_REGS_RESET_OS_TRIGGER);
			}
		}
	}
	return (rc);
}

int
ena_update_cpu_stride(struct ena_adapter *adapter, uint32_t new_num)
{
	uint32_t old_num;
	int rc = 0;
	bool dev_was_up;

	dev_was_up = ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter);
	old_num = adapter->irq_cpu_stride;

	ena_down(adapter);

	adapter->irq_cpu_stride = new_num;

	if (dev_was_up) {
		rc = ena_up(adapter);
		if (unlikely(rc != 0)) {
			ena_log(adapter->pdev, ERR,
			    "Failed to configure device %d IRQ CPU stride. "
			    "Reverting to previous value: %d\n",
			    new_num, old_num);

			adapter->irq_cpu_stride = old_num;

			rc = ena_up(adapter);
			if (unlikely(rc != 0)) {
				ena_log(adapter->pdev, ERR,
				    "Failed to revert to previous setup."
				    "Triggering device reset.\n");
				ENA_FLAG_SET_ATOMIC(
				    ENA_FLAG_DEV_UP_BEFORE_RESET, adapter);
				ena_trigger_reset(adapter,
				    ENA_REGS_RESET_OS_TRIGGER);
			}
		}
	}
	return (rc);
}

/* Caller should sanitize new_num */
int
ena_update_io_queue_nb(struct ena_adapter *adapter, uint32_t new_num)
{
	uint32_t old_num;
	int rc = 0;
	bool dev_was_up;

	dev_was_up = ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter);
	old_num = adapter->num_io_queues;
	ena_down(adapter);

	ena_update_io_rings(adapter, new_num);

	if (dev_was_up) {
		rc = ena_up(adapter);
		if (unlikely(rc != 0)) {
			ena_log(adapter->pdev, ERR,
			    "Failed to configure device with %u IO queues. "
			    "Reverting to previous value: %u\n",
			    new_num, old_num);

			ena_update_io_rings(adapter, old_num);

			rc = ena_up(adapter);
			if (unlikely(rc != 0)) {
				ena_log(adapter->pdev, ERR,
				    "Failed to revert to previous setup IO "
				    "queues. Triggering device reset.\n");
				ENA_FLAG_SET_ATOMIC(
				    ENA_FLAG_DEV_UP_BEFORE_RESET, adapter);
				ena_trigger_reset(adapter,
				    ENA_REGS_RESET_OS_TRIGGER);
			}
		}
	}

	return (rc);
}

static void
ena_free_rx_bufs(struct ena_adapter *adapter, unsigned int qid)
{
	struct ena_ring *rx_ring = &adapter->rx_ring[qid];
	unsigned int i;

	for (i = 0; i < rx_ring->ring_size; i++) {
		struct ena_rx_buffer *rx_info = &rx_ring->rx_buffer_info[i];

		if (rx_info->mbuf != NULL)
			ena_free_rx_mbuf(adapter, rx_ring, rx_info);
#ifdef DEV_NETMAP
		if (((if_getflags(adapter->ifp) & IFF_DYING) == 0) &&
		    (adapter->ifp->if_capenable & IFCAP_NETMAP)) {
			if (rx_info->netmap_buf_idx != 0)
				ena_netmap_free_rx_slot(adapter, rx_ring,
				    rx_info);
		}
#endif /* DEV_NETMAP */
	}
}

/**
 * ena_refill_all_rx_bufs - allocate all queues Rx buffers
 * @adapter: network interface device structure
 *
 */
static void
ena_refill_all_rx_bufs(struct ena_adapter *adapter)
{
	struct ena_ring *rx_ring;
	int i, rc, bufs_num;

	for (i = 0; i < adapter->num_io_queues; i++) {
		rx_ring = &adapter->rx_ring[i];
		bufs_num = rx_ring->ring_size - 1;
		rc = ena_refill_rx_bufs(rx_ring, bufs_num);
		if (unlikely(rc != bufs_num))
			ena_log_io(adapter->pdev, WARN,
			    "refilling Queue %d failed. "
			    "Allocated %d buffers from: %d\n",
			    i, rc, bufs_num);
#ifdef DEV_NETMAP
		rx_ring->initialized = true;
#endif /* DEV_NETMAP */
	}
}

static void
ena_free_all_rx_bufs(struct ena_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_io_queues; i++)
		ena_free_rx_bufs(adapter, i);
}

/**
 * ena_free_tx_bufs - Free Tx Buffers per Queue
 * @adapter: network interface device structure
 * @qid: queue index
 **/
static void
ena_free_tx_bufs(struct ena_adapter *adapter, unsigned int qid)
{
	bool print_once = true;
	struct ena_ring *tx_ring = &adapter->tx_ring[qid];

	ENA_RING_MTX_LOCK(tx_ring);
	for (int i = 0; i < tx_ring->ring_size; i++) {
		struct ena_tx_buffer *tx_info = &tx_ring->tx_buffer_info[i];

		if (tx_info->mbuf == NULL)
			continue;

		if (print_once) {
			ena_log(adapter->pdev, WARN,
			    "free uncompleted tx mbuf qid %d idx 0x%x\n", qid,
			    i);
			print_once = false;
		} else {
			ena_log(adapter->pdev, DBG,
			    "free uncompleted tx mbuf qid %d idx 0x%x\n", qid,
			    i);
		}

		bus_dmamap_sync(adapter->tx_buf_tag, tx_info->dmamap,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(adapter->tx_buf_tag, tx_info->dmamap);

		m_free(tx_info->mbuf);
		tx_info->mbuf = NULL;
	}
	ENA_RING_MTX_UNLOCK(tx_ring);
}

static void
ena_free_all_tx_bufs(struct ena_adapter *adapter)
{
	for (int i = 0; i < adapter->num_io_queues; i++)
		ena_free_tx_bufs(adapter, i);
}

static void
ena_destroy_all_tx_queues(struct ena_adapter *adapter)
{
	uint16_t ena_qid;
	int i;

	for (i = 0; i < adapter->num_io_queues; i++) {
		ena_qid = ENA_IO_TXQ_IDX(i);
		ena_com_destroy_io_queue(adapter->ena_dev, ena_qid);
	}
}

static void
ena_destroy_all_rx_queues(struct ena_adapter *adapter)
{
	uint16_t ena_qid;
	int i;

	for (i = 0; i < adapter->num_io_queues; i++) {
		ena_qid = ENA_IO_RXQ_IDX(i);
		ena_com_destroy_io_queue(adapter->ena_dev, ena_qid);
	}
}

static void
ena_destroy_all_io_queues(struct ena_adapter *adapter)
{
	struct ena_que *queue;
	int i;

	for (i = 0; i < adapter->num_io_queues; i++) {
		queue = &adapter->que[i];
		while (taskqueue_cancel(queue->cleanup_tq, &queue->cleanup_task, NULL))
			taskqueue_drain(queue->cleanup_tq, &queue->cleanup_task);
		taskqueue_free(queue->cleanup_tq);
	}

	ena_destroy_all_tx_queues(adapter);
	ena_destroy_all_rx_queues(adapter);
}

static int
ena_create_io_queues(struct ena_adapter *adapter)
{
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	struct ena_com_create_io_ctx ctx;
	struct ena_ring *ring;
	struct ena_que *queue;
	uint16_t ena_qid;
	uint32_t msix_vector;
	cpuset_t *cpu_mask = NULL;
	int rc, i;

	/* Create TX queues */
	for (i = 0; i < adapter->num_io_queues; i++) {
		msix_vector = ENA_IO_IRQ_IDX(i);
		ena_qid = ENA_IO_TXQ_IDX(i);
		ctx.mem_queue_type = ena_dev->tx_mem_queue_type;
		ctx.direction = ENA_COM_IO_QUEUE_DIRECTION_TX;
		ctx.queue_size = adapter->requested_tx_ring_size;
		ctx.msix_vector = msix_vector;
		ctx.qid = ena_qid;
		ctx.numa_node = adapter->que[i].domain;

		rc = ena_com_create_io_queue(ena_dev, &ctx);
		if (rc != 0) {
			ena_log(adapter->pdev, ERR,
			    "Failed to create io TX queue #%d rc: %d\n", i, rc);
			goto err_tx;
		}
		ring = &adapter->tx_ring[i];
		rc = ena_com_get_io_handlers(ena_dev, ena_qid,
		    &ring->ena_com_io_sq, &ring->ena_com_io_cq);
		if (rc != 0) {
			ena_log(adapter->pdev, ERR,
			    "Failed to get TX queue handlers. TX queue num"
			    " %d rc: %d\n",
			    i, rc);
			ena_com_destroy_io_queue(ena_dev, ena_qid);
			goto err_tx;
		}

		if (ctx.numa_node >= 0) {
			ena_com_update_numa_node(ring->ena_com_io_cq,
			    ctx.numa_node);
		}
	}

	/* Create RX queues */
	for (i = 0; i < adapter->num_io_queues; i++) {
		msix_vector = ENA_IO_IRQ_IDX(i);
		ena_qid = ENA_IO_RXQ_IDX(i);
		ctx.mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
		ctx.direction = ENA_COM_IO_QUEUE_DIRECTION_RX;
		ctx.queue_size = adapter->requested_rx_ring_size;
		ctx.msix_vector = msix_vector;
		ctx.qid = ena_qid;
		ctx.numa_node = adapter->que[i].domain;

		rc = ena_com_create_io_queue(ena_dev, &ctx);
		if (unlikely(rc != 0)) {
			ena_log(adapter->pdev, ERR,
			    "Failed to create io RX queue[%d] rc: %d\n", i, rc);
			goto err_rx;
		}

		ring = &adapter->rx_ring[i];
		rc = ena_com_get_io_handlers(ena_dev, ena_qid,
		    &ring->ena_com_io_sq, &ring->ena_com_io_cq);
		if (unlikely(rc != 0)) {
			ena_log(adapter->pdev, ERR,
			    "Failed to get RX queue handlers. RX queue num"
			    " %d rc: %d\n",
			    i, rc);
			ena_com_destroy_io_queue(ena_dev, ena_qid);
			goto err_rx;
		}

		if (ctx.numa_node >= 0) {
			ena_com_update_numa_node(ring->ena_com_io_cq,
			    ctx.numa_node);
		}
	}

	for (i = 0; i < adapter->num_io_queues; i++) {
		queue = &adapter->que[i];

		NET_TASK_INIT(&queue->cleanup_task, 0, ena_cleanup, queue);
		queue->cleanup_tq = taskqueue_create_fast("ena cleanup",
		    M_WAITOK, taskqueue_thread_enqueue, &queue->cleanup_tq);

#ifdef RSS
		cpu_mask = &queue->cpu_mask;
#endif
		taskqueue_start_threads_cpuset(&queue->cleanup_tq, 1, PI_NET,
		    cpu_mask, "%s queue %d cleanup",
		    device_get_nameunit(adapter->pdev), i);
	}

	return (0);

err_rx:
	while (i--)
		ena_com_destroy_io_queue(ena_dev, ENA_IO_RXQ_IDX(i));
	i = adapter->num_io_queues;
err_tx:
	while (i--)
		ena_com_destroy_io_queue(ena_dev, ENA_IO_TXQ_IDX(i));

	return (ENXIO);
}

/*********************************************************************
 *
 *  MSIX & Interrupt Service routine
 *
 **********************************************************************/

/**
 * ena_handle_msix - MSIX Interrupt Handler for admin/async queue
 * @arg: interrupt number
 **/
static void
ena_intr_msix_mgmnt(void *arg)
{
	struct ena_adapter *adapter = (struct ena_adapter *)arg;

	ena_com_admin_q_comp_intr_handler(adapter->ena_dev);
	if (likely(ENA_FLAG_ISSET(ENA_FLAG_DEVICE_RUNNING, adapter)))
		ena_com_aenq_intr_handler(adapter->ena_dev, arg);
}

/**
 * ena_handle_msix - MSIX Interrupt Handler for Tx/Rx
 * @arg: queue
 **/
static int
ena_handle_msix(void *arg)
{
	struct ena_que *queue = arg;
	struct ena_adapter *adapter = queue->adapter;
	if_t ifp = adapter->ifp;

	if (unlikely((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0))
		return (FILTER_STRAY);

	taskqueue_enqueue(queue->cleanup_tq, &queue->cleanup_task);

	return (FILTER_HANDLED);
}

static int
ena_enable_msix(struct ena_adapter *adapter)
{
	device_t dev = adapter->pdev;
	int msix_vecs, msix_req;
	int i, rc = 0;

	if (ENA_FLAG_ISSET(ENA_FLAG_MSIX_ENABLED, adapter)) {
		ena_log(dev, ERR, "Error, MSI-X is already enabled\n");
		return (EINVAL);
	}

	/* Reserved the max msix vectors we might need */
	msix_vecs = ENA_MAX_MSIX_VEC(adapter->max_num_io_queues);

	adapter->msix_entries = malloc(msix_vecs * sizeof(struct msix_entry),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	ena_log(dev, DBG, "trying to enable MSI-X, vectors: %d\n", msix_vecs);

	for (i = 0; i < msix_vecs; i++) {
		adapter->msix_entries[i].entry = i;
		/* Vectors must start from 1 */
		adapter->msix_entries[i].vector = i + 1;
	}

	msix_req = msix_vecs;
	rc = pci_alloc_msix(dev, &msix_vecs);
	if (unlikely(rc != 0)) {
		ena_log(dev, ERR, "Failed to enable MSIX, vectors %d rc %d\n",
		    msix_vecs, rc);

		rc = ENOSPC;
		goto err_msix_free;
	}

	if (msix_vecs != msix_req) {
		if (msix_vecs == ENA_ADMIN_MSIX_VEC) {
			ena_log(dev, ERR,
			    "Not enough number of MSI-x allocated: %d\n",
			    msix_vecs);
			pci_release_msi(dev);
			rc = ENOSPC;
			goto err_msix_free;
		}
		ena_log(dev, ERR,
		    "Enable only %d MSI-x (out of %d), reduce "
		    "the number of queues\n",
		    msix_vecs, msix_req);
	}

	adapter->msix_vecs = msix_vecs;
	ENA_FLAG_SET_ATOMIC(ENA_FLAG_MSIX_ENABLED, adapter);

	return (0);

err_msix_free:
	free(adapter->msix_entries, M_DEVBUF);
	adapter->msix_entries = NULL;

	return (rc);
}

static void
ena_setup_mgmnt_intr(struct ena_adapter *adapter)
{
	snprintf(adapter->irq_tbl[ENA_MGMNT_IRQ_IDX].name, ENA_IRQNAME_SIZE,
	    "ena-mgmnt@pci:%s", device_get_nameunit(adapter->pdev));
	/*
	 * Handler is NULL on purpose, it will be set
	 * when mgmnt interrupt is acquired
	 */
	adapter->irq_tbl[ENA_MGMNT_IRQ_IDX].handler = NULL;
	adapter->irq_tbl[ENA_MGMNT_IRQ_IDX].data = adapter;
	adapter->irq_tbl[ENA_MGMNT_IRQ_IDX].vector =
	    adapter->msix_entries[ENA_MGMNT_IRQ_IDX].vector;
}

static int
ena_setup_io_intr(struct ena_adapter *adapter)
{
#ifdef RSS
	int num_buckets = rss_getnumbuckets();
	static int last_bind = 0;
	int cur_bind;
	int idx;
#endif
	int irq_idx;

	if (adapter->msix_entries == NULL)
		return (EINVAL);

#ifdef RSS
	if (adapter->first_bind < 0) {
		adapter->first_bind = last_bind;
		last_bind = (last_bind + adapter->num_io_queues) % num_buckets;
	}
	cur_bind = adapter->first_bind;
#endif

	for (int i = 0; i < adapter->num_io_queues; i++) {
		irq_idx = ENA_IO_IRQ_IDX(i);

		snprintf(adapter->irq_tbl[irq_idx].name, ENA_IRQNAME_SIZE,
		    "%s-TxRx-%d", device_get_nameunit(adapter->pdev), i);
		adapter->irq_tbl[irq_idx].handler = ena_handle_msix;
		adapter->irq_tbl[irq_idx].data = &adapter->que[i];
		adapter->irq_tbl[irq_idx].vector =
		    adapter->msix_entries[irq_idx].vector;
		ena_log(adapter->pdev, DBG, "ena_setup_io_intr vector: %d\n",
		    adapter->msix_entries[irq_idx].vector);

		if (adapter->irq_cpu_base > ENA_BASE_CPU_UNSPECIFIED) {
			adapter->que[i].cpu = adapter->irq_tbl[irq_idx].cpu =
			    (unsigned)(adapter->irq_cpu_base +
			    i * adapter->irq_cpu_stride) % (unsigned)mp_ncpus;
			CPU_SETOF(adapter->que[i].cpu, &adapter->que[i].cpu_mask);
		}

#ifdef RSS
		adapter->que[i].cpu = adapter->irq_tbl[irq_idx].cpu =
		    rss_getcpu(cur_bind);
		cur_bind = (cur_bind + 1) % num_buckets;
		CPU_SETOF(adapter->que[i].cpu, &adapter->que[i].cpu_mask);

		for (idx = 0; idx < MAXMEMDOM; ++idx) {
			if (CPU_ISSET(adapter->que[i].cpu, &cpuset_domain[idx]))
				break;
		}
		adapter->que[i].domain = idx;
#else
		adapter->que[i].domain = -1;
#endif
	}

	return (0);
}

static int
ena_request_mgmnt_irq(struct ena_adapter *adapter)
{
	device_t pdev = adapter->pdev;
	struct ena_irq *irq;
	unsigned long flags;
	int rc, rcc;

	flags = RF_ACTIVE | RF_SHAREABLE;

	irq = &adapter->irq_tbl[ENA_MGMNT_IRQ_IDX];
	irq->res = bus_alloc_resource_any(adapter->pdev, SYS_RES_IRQ,
	    &irq->vector, flags);

	if (unlikely(irq->res == NULL)) {
		ena_log(pdev, ERR, "could not allocate irq vector: %d\n",
		    irq->vector);
		return (ENXIO);
	}

	rc = bus_setup_intr(adapter->pdev, irq->res,
	    INTR_TYPE_NET | INTR_MPSAFE, NULL, ena_intr_msix_mgmnt, irq->data,
	    &irq->cookie);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR,
		    "failed to register interrupt handler for irq %ju: %d\n",
		    rman_get_start(irq->res), rc);
		goto err_res_free;
	}
	irq->requested = true;

	return (rc);

err_res_free:
	ena_log(pdev, INFO, "releasing resource for irq %d\n", irq->vector);
	rcc = bus_release_resource(adapter->pdev, SYS_RES_IRQ, irq->vector,
	    irq->res);
	if (unlikely(rcc != 0))
		ena_log(pdev, ERR,
		    "dev has no parent while releasing res for irq: %d\n",
		    irq->vector);
	irq->res = NULL;

	return (rc);
}

static int
ena_request_io_irq(struct ena_adapter *adapter)
{
	device_t pdev = adapter->pdev;
	struct ena_irq *irq;
	unsigned long flags = 0;
	int rc = 0, i, rcc;

	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_MSIX_ENABLED, adapter))) {
		ena_log(pdev, ERR,
		    "failed to request I/O IRQ: MSI-X is not enabled\n");
		return (EINVAL);
	} else {
		flags = RF_ACTIVE | RF_SHAREABLE;
	}

	for (i = ENA_IO_IRQ_FIRST_IDX; i < adapter->msix_vecs; i++) {
		irq = &adapter->irq_tbl[i];

		if (unlikely(irq->requested))
			continue;

		irq->res = bus_alloc_resource_any(adapter->pdev, SYS_RES_IRQ,
		    &irq->vector, flags);
		if (unlikely(irq->res == NULL)) {
			rc = ENOMEM;
			ena_log(pdev, ERR,
			    "could not allocate irq vector: %d\n", irq->vector);
			goto err;
		}

		rc = bus_setup_intr(adapter->pdev, irq->res,
		    INTR_TYPE_NET | INTR_MPSAFE, irq->handler, NULL, irq->data,
		    &irq->cookie);
		if (unlikely(rc != 0)) {
			ena_log(pdev, ERR,
			    "failed to register interrupt handler for irq %ju: %d\n",
			    rman_get_start(irq->res), rc);
			goto err;
		}
		irq->requested = true;

		if (adapter->rss_enabled || adapter->irq_cpu_base > ENA_BASE_CPU_UNSPECIFIED) {
			rc = bus_bind_intr(adapter->pdev, irq->res, irq->cpu);
			if (unlikely(rc != 0)) {
				ena_log(pdev, ERR,
				    "failed to bind interrupt handler for irq %ju to cpu %d: %d\n",
				    rman_get_start(irq->res), irq->cpu, rc);
				goto err;
			}

			ena_log(pdev, INFO, "queue %d - cpu %d\n",
			    i - ENA_IO_IRQ_FIRST_IDX, irq->cpu);
		}
	}
	return (rc);

err:

	for (; i >= ENA_IO_IRQ_FIRST_IDX; i--) {
		irq = &adapter->irq_tbl[i];
		rcc = 0;

		/* Once we entered err: section and irq->requested is true we
		   free both intr and resources */
		if (irq->requested) {
			rcc = bus_teardown_intr(adapter->pdev, irq->res,
			    irq->cookie);
			if (unlikely(rcc != 0))
				ena_log(pdev, ERR,
				    "could not release irq: %d, error: %d\n",
				    irq->vector, rcc);
		}

		/* If we entered err: section without irq->requested set we know
		   it was bus_alloc_resource_any() that needs cleanup, provided
		   res is not NULL. In case res is NULL no work in needed in
		   this iteration */
		rcc = 0;
		if (irq->res != NULL) {
			rcc = bus_release_resource(adapter->pdev, SYS_RES_IRQ,
			    irq->vector, irq->res);
		}
		if (unlikely(rcc != 0))
			ena_log(pdev, ERR,
			    "dev has no parent while releasing res for irq: %d\n",
			    irq->vector);
		irq->requested = false;
		irq->res = NULL;
	}

	return (rc);
}

static void
ena_free_mgmnt_irq(struct ena_adapter *adapter)
{
	device_t pdev = adapter->pdev;
	struct ena_irq *irq;
	int rc;

	irq = &adapter->irq_tbl[ENA_MGMNT_IRQ_IDX];
	if (irq->requested) {
		ena_log(pdev, DBG, "tear down irq: %d\n", irq->vector);
		rc = bus_teardown_intr(adapter->pdev, irq->res, irq->cookie);
		if (unlikely(rc != 0))
			ena_log(pdev, ERR, "failed to tear down irq: %d\n",
			    irq->vector);
		irq->requested = 0;
	}

	if (irq->res != NULL) {
		ena_log(pdev, DBG, "release resource irq: %d\n", irq->vector);
		rc = bus_release_resource(adapter->pdev, SYS_RES_IRQ,
		    irq->vector, irq->res);
		irq->res = NULL;
		if (unlikely(rc != 0))
			ena_log(pdev, ERR,
			    "dev has no parent while releasing res for irq: %d\n",
			    irq->vector);
	}
}

static void
ena_free_io_irq(struct ena_adapter *adapter)
{
	device_t pdev = adapter->pdev;
	struct ena_irq *irq;
	int rc;

	for (int i = ENA_IO_IRQ_FIRST_IDX; i < adapter->msix_vecs; i++) {
		irq = &adapter->irq_tbl[i];
		if (irq->requested) {
			ena_log(pdev, DBG, "tear down irq: %d\n", irq->vector);
			rc = bus_teardown_intr(adapter->pdev, irq->res,
			    irq->cookie);
			if (unlikely(rc != 0)) {
				ena_log(pdev, ERR,
				    "failed to tear down irq: %d\n",
				    irq->vector);
			}
			irq->requested = 0;
		}

		if (irq->res != NULL) {
			ena_log(pdev, DBG, "release resource irq: %d\n",
			    irq->vector);
			rc = bus_release_resource(adapter->pdev, SYS_RES_IRQ,
			    irq->vector, irq->res);
			irq->res = NULL;
			if (unlikely(rc != 0)) {
				ena_log(pdev, ERR,
				    "dev has no parent while releasing res for irq: %d\n",
				    irq->vector);
			}
		}
	}
}

static void
ena_free_irqs(struct ena_adapter *adapter)
{
	ena_free_io_irq(adapter);
	ena_free_mgmnt_irq(adapter);
	ena_disable_msix(adapter);
}

static void
ena_disable_msix(struct ena_adapter *adapter)
{
	if (ENA_FLAG_ISSET(ENA_FLAG_MSIX_ENABLED, adapter)) {
		ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_MSIX_ENABLED, adapter);
		pci_release_msi(adapter->pdev);
	}

	adapter->msix_vecs = 0;
	free(adapter->msix_entries, M_DEVBUF);
	adapter->msix_entries = NULL;
}

static void
ena_unmask_all_io_irqs(struct ena_adapter *adapter)
{
	struct ena_com_io_cq *io_cq;
	struct ena_eth_io_intr_reg intr_reg;
	struct ena_ring *tx_ring;
	uint16_t ena_qid;
	int i;

	/* Unmask interrupts for all queues */
	for (i = 0; i < adapter->num_io_queues; i++) {
		ena_qid = ENA_IO_TXQ_IDX(i);
		io_cq = &adapter->ena_dev->io_cq_queues[ena_qid];
		ena_com_update_intr_reg(&intr_reg, 0, 0, true, false);
		tx_ring = &adapter->tx_ring[i];
		counter_u64_add(tx_ring->tx_stats.unmask_interrupt_num, 1);
		ena_com_unmask_intr(io_cq, &intr_reg);
	}
}

static int
ena_up_complete(struct ena_adapter *adapter)
{
	int rc;

	if (likely(ENA_FLAG_ISSET(ENA_FLAG_RSS_ACTIVE, adapter))) {
		rc = ena_rss_configure(adapter);
		if (rc != 0) {
			ena_log(adapter->pdev, ERR,
			    "Failed to configure RSS\n");
			return (rc);
		}
	}

	rc = ena_change_mtu(adapter->ifp, adapter->ifp->if_mtu);
	if (unlikely(rc != 0))
		return (rc);

	ena_refill_all_rx_bufs(adapter);
	ena_reset_counters((counter_u64_t *)&adapter->hw_stats,
	    sizeof(adapter->hw_stats));

	return (0);
}

static void
set_io_rings_size(struct ena_adapter *adapter, int new_tx_size, int new_rx_size)
{
	int i;

	for (i = 0; i < adapter->num_io_queues; i++) {
		adapter->tx_ring[i].ring_size = new_tx_size;
		adapter->rx_ring[i].ring_size = new_rx_size;
	}
}

static int
create_queues_with_size_backoff(struct ena_adapter *adapter)
{
	device_t pdev = adapter->pdev;
	int rc;
	uint32_t cur_rx_ring_size, cur_tx_ring_size;
	uint32_t new_rx_ring_size, new_tx_ring_size;

	/*
	 * Current queue sizes might be set to smaller than the requested
	 * ones due to past queue allocation failures.
	 */
	set_io_rings_size(adapter, adapter->requested_tx_ring_size,
	    adapter->requested_rx_ring_size);

	while (1) {
		/* Allocate transmit descriptors */
		rc = ena_setup_all_tx_resources(adapter);
		if (unlikely(rc != 0)) {
			ena_log(pdev, ERR, "err_setup_tx\n");
			goto err_setup_tx;
		}

		/* Allocate receive descriptors */
		rc = ena_setup_all_rx_resources(adapter);
		if (unlikely(rc != 0)) {
			ena_log(pdev, ERR, "err_setup_rx\n");
			goto err_setup_rx;
		}

		/* Create IO queues for Rx & Tx */
		rc = ena_create_io_queues(adapter);
		if (unlikely(rc != 0)) {
			ena_log(pdev, ERR, "create IO queues failed\n");
			goto err_io_que;
		}

		return (0);

err_io_que:
		ena_free_all_rx_resources(adapter);
err_setup_rx:
		ena_free_all_tx_resources(adapter);
err_setup_tx:
		/*
		 * Lower the ring size if ENOMEM. Otherwise, return the
		 * error straightaway.
		 */
		if (unlikely(rc != ENOMEM)) {
			ena_log(pdev, ERR,
			    "Queue creation failed with error code: %d\n", rc);
			return (rc);
		}

		cur_tx_ring_size = adapter->tx_ring[0].ring_size;
		cur_rx_ring_size = adapter->rx_ring[0].ring_size;

		ena_log(pdev, ERR,
		    "Not enough memory to create queues with sizes TX=%d, RX=%d\n",
		    cur_tx_ring_size, cur_rx_ring_size);

		new_tx_ring_size = cur_tx_ring_size;
		new_rx_ring_size = cur_rx_ring_size;

		/*
		 * Decrease the size of a larger queue, or decrease both if they
		 * are the same size.
		 */
		if (cur_rx_ring_size <= cur_tx_ring_size)
			new_tx_ring_size = cur_tx_ring_size / 2;
		if (cur_rx_ring_size >= cur_tx_ring_size)
			new_rx_ring_size = cur_rx_ring_size / 2;

		if (new_tx_ring_size < ENA_MIN_RING_SIZE ||
		    new_rx_ring_size < ENA_MIN_RING_SIZE) {
			ena_log(pdev, ERR,
			    "Queue creation failed with the smallest possible queue size"
			    "of %d for both queues. Not retrying with smaller queues\n",
			    ENA_MIN_RING_SIZE);
			return (rc);
		}

		ena_log(pdev, INFO,
		    "Retrying queue creation with sizes TX=%d, RX=%d\n",
		    new_tx_ring_size, new_rx_ring_size);

		set_io_rings_size(adapter, new_tx_ring_size, new_rx_ring_size);
	}
}

int
ena_up(struct ena_adapter *adapter)
{
	int rc = 0;

	ENA_LOCK_ASSERT();

	if (unlikely(device_is_attached(adapter->pdev) == 0)) {
		ena_log(adapter->pdev, ERR, "device is not attached!\n");
		return (ENXIO);
	}

	if (ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter))
		return (0);

	ena_log(adapter->pdev, INFO, "device is going UP\n");

	/* setup interrupts for IO queues */
	rc = ena_setup_io_intr(adapter);
	if (unlikely(rc != 0)) {
		ena_log(adapter->pdev, ERR, "error setting up IO interrupt\n");
		goto error;
	}
	rc = ena_request_io_irq(adapter);
	if (unlikely(rc != 0)) {
		ena_log(adapter->pdev, ERR, "err_req_irq\n");
		goto error;
	}

	ena_log(adapter->pdev, INFO,
	    "Creating %u IO queues. Rx queue size: %d, Tx queue size: %d, LLQ is %s\n",
	    adapter->num_io_queues,
	    adapter->requested_rx_ring_size,
	    adapter->requested_tx_ring_size,
	    (adapter->ena_dev->tx_mem_queue_type ==
		ENA_ADMIN_PLACEMENT_POLICY_DEV) ? "ENABLED" : "DISABLED");

	rc = create_queues_with_size_backoff(adapter);
	if (unlikely(rc != 0)) {
		ena_log(adapter->pdev, ERR,
		    "error creating queues with size backoff\n");
		goto err_create_queues_with_backoff;
	}

	if (ENA_FLAG_ISSET(ENA_FLAG_LINK_UP, adapter))
		if_link_state_change(adapter->ifp, LINK_STATE_UP);

	rc = ena_up_complete(adapter);
	if (unlikely(rc != 0))
		goto err_up_complete;

	counter_u64_add(adapter->dev_stats.interface_up, 1);

	ena_update_hwassist(adapter);

	if_setdrvflagbits(adapter->ifp, IFF_DRV_RUNNING, IFF_DRV_OACTIVE);

	ENA_FLAG_SET_ATOMIC(ENA_FLAG_DEV_UP, adapter);

	ena_unmask_all_io_irqs(adapter);

	return (0);

err_up_complete:
	ena_destroy_all_io_queues(adapter);
	ena_free_all_rx_resources(adapter);
	ena_free_all_tx_resources(adapter);
err_create_queues_with_backoff:
	ena_free_io_irq(adapter);
error:
	return (rc);
}

static uint64_t
ena_get_counter(if_t ifp, ift_counter cnt)
{
	struct ena_adapter *adapter;
	struct ena_hw_stats *stats;

	adapter = if_getsoftc(ifp);
	stats = &adapter->hw_stats;

	switch (cnt) {
	case IFCOUNTER_IPACKETS:
		return (counter_u64_fetch(stats->rx_packets));
	case IFCOUNTER_OPACKETS:
		return (counter_u64_fetch(stats->tx_packets));
	case IFCOUNTER_IBYTES:
		return (counter_u64_fetch(stats->rx_bytes));
	case IFCOUNTER_OBYTES:
		return (counter_u64_fetch(stats->tx_bytes));
	case IFCOUNTER_IQDROPS:
		return (counter_u64_fetch(stats->rx_drops));
	case IFCOUNTER_OQDROPS:
		return (counter_u64_fetch(stats->tx_drops));
	default:
		return (if_get_counter_default(ifp, cnt));
	}
}

static int
ena_media_change(if_t ifp)
{
	/* Media Change is not supported by firmware */
	return (0);
}

static void
ena_media_status(if_t ifp, struct ifmediareq *ifmr)
{
	struct ena_adapter *adapter = if_getsoftc(ifp);
	ena_log(adapter->pdev, DBG, "Media status update\n");

	ENA_LOCK_LOCK();

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!ENA_FLAG_ISSET(ENA_FLAG_LINK_UP, adapter)) {
		ENA_LOCK_UNLOCK();
		ena_log(adapter->pdev, INFO, "Link is down\n");
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active |= IFM_UNKNOWN | IFM_FDX;

	ENA_LOCK_UNLOCK();
}

static void
ena_init(void *arg)
{
	struct ena_adapter *adapter = (struct ena_adapter *)arg;

	if (!ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter)) {
		ENA_LOCK_LOCK();
		ena_up(adapter);
		ENA_LOCK_UNLOCK();
	}
}

static int
ena_ioctl(if_t ifp, u_long command, caddr_t data)
{
	struct ena_adapter *adapter;
	struct ifreq *ifr;
	int rc;

	adapter = ifp->if_softc;
	ifr = (struct ifreq *)data;

	/*
	 * Acquiring lock to prevent from running up and down routines parallel.
	 */
	rc = 0;
	switch (command) {
	case SIOCSIFMTU:
		if (ifp->if_mtu == ifr->ifr_mtu)
			break;
		ENA_LOCK_LOCK();
		ena_down(adapter);

		ena_change_mtu(ifp, ifr->ifr_mtu);

		rc = ena_up(adapter);
		ENA_LOCK_UNLOCK();
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) != 0) {
			if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0) {
				if ((ifp->if_flags &
				    (IFF_PROMISC | IFF_ALLMULTI)) != 0) {
					ena_log(adapter->pdev, INFO,
					    "ioctl promisc/allmulti\n");
				}
			} else {
				ENA_LOCK_LOCK();
				rc = ena_up(adapter);
				ENA_LOCK_UNLOCK();
			}
		} else {
			if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0) {
				ENA_LOCK_LOCK();
				ena_down(adapter);
				ENA_LOCK_UNLOCK();
			}
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		rc = ifmedia_ioctl(ifp, ifr, &adapter->media, command);
		break;

	case SIOCSIFCAP:
		{
			int reinit = 0;

			if (ifr->ifr_reqcap != ifp->if_capenable) {
				ifp->if_capenable = ifr->ifr_reqcap;
				reinit = 1;
			}

			if ((reinit != 0) &&
			    ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0)) {
				ENA_LOCK_LOCK();
				ena_down(adapter);
				rc = ena_up(adapter);
				ENA_LOCK_UNLOCK();
			}
		}

		break;
	default:
		rc = ether_ioctl(ifp, command, data);
		break;
	}

	return (rc);
}

static int
ena_get_dev_offloads(struct ena_com_dev_get_features_ctx *feat)
{
	int caps = 0;

	if ((feat->offload.tx &
	    (ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_MASK |
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_MASK |
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L3_CSUM_IPV4_MASK)) != 0)
		caps |= IFCAP_TXCSUM;

	if ((feat->offload.tx &
	    (ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_FULL_MASK |
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV6_CSUM_PART_MASK)) != 0)
		caps |= IFCAP_TXCSUM_IPV6;

	if ((feat->offload.tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_MASK) != 0)
		caps |= IFCAP_TSO4;

	if ((feat->offload.tx & ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_MASK) != 0)
		caps |= IFCAP_TSO6;

	if ((feat->offload.rx_supported &
	    (ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV4_CSUM_MASK |
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L3_CSUM_IPV4_MASK)) != 0)
		caps |= IFCAP_RXCSUM;

	if ((feat->offload.rx_supported &
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_RX_L4_IPV6_CSUM_MASK) != 0)
		caps |= IFCAP_RXCSUM_IPV6;

	caps |= IFCAP_LRO | IFCAP_JUMBO_MTU;

	return (caps);
}

static void
ena_update_host_info(struct ena_admin_host_info *host_info, if_t ifp)
{
	host_info->supported_network_features[0] = (uint32_t)if_getcapabilities(ifp);
}

static void
ena_update_hwassist(struct ena_adapter *adapter)
{
	if_t ifp = adapter->ifp;
	uint32_t feat = adapter->tx_offload_cap;
	int cap = if_getcapenable(ifp);
	int flags = 0;

	if_clearhwassist(ifp);

	if ((cap & IFCAP_TXCSUM) != 0) {
		if ((feat &
		    ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L3_CSUM_IPV4_MASK) != 0)
			flags |= CSUM_IP;
		if ((feat &
		    (ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_FULL_MASK |
		    ENA_ADMIN_FEATURE_OFFLOAD_DESC_TX_L4_IPV4_CSUM_PART_MASK)) != 0)
			flags |= CSUM_IP_UDP | CSUM_IP_TCP;
	}

	if ((cap & IFCAP_TXCSUM_IPV6) != 0)
		flags |= CSUM_IP6_UDP | CSUM_IP6_TCP;

	if ((cap & IFCAP_TSO4) != 0)
		flags |= CSUM_IP_TSO;

	if ((cap & IFCAP_TSO6) != 0)
		flags |= CSUM_IP6_TSO;

	if_sethwassistbits(ifp, flags, 0);
}

static int
ena_setup_ifnet(device_t pdev, struct ena_adapter *adapter,
    struct ena_com_dev_get_features_ctx *feat)
{
	if_t ifp;
	int caps = 0;

	ifp = adapter->ifp = if_gethandle(IFT_ETHER);
	if (unlikely(ifp == NULL)) {
		ena_log(pdev, ERR, "can not allocate ifnet structure\n");
		return (ENXIO);
	}
	if_initname(ifp, device_get_name(pdev), device_get_unit(pdev));
	if_setdev(ifp, pdev);
	if_setsoftc(ifp, adapter);

	if_setflags(ifp,
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST | IFF_KNOWSEPOCH);
	if_setinitfn(ifp, ena_init);
	if_settransmitfn(ifp, ena_mq_start);
	if_setqflushfn(ifp, ena_qflush);
	if_setioctlfn(ifp, ena_ioctl);
	if_setgetcounterfn(ifp, ena_get_counter);

	if_setsendqlen(ifp, adapter->requested_tx_ring_size);
	if_setsendqready(ifp);
	if_setmtu(ifp, ETHERMTU);
	if_setbaudrate(ifp, 0);
	/* Zeroize capabilities... */
	if_setcapabilities(ifp, 0);
	if_setcapenable(ifp, 0);
	/* check hardware support */
	caps = ena_get_dev_offloads(feat);
	/* ... and set them */
	if_setcapabilitiesbit(ifp, caps, 0);

	/* TSO parameters */
	ifp->if_hw_tsomax = ENA_TSO_MAXSIZE -
	    (ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN);
	ifp->if_hw_tsomaxsegcount = adapter->max_tx_sgl_size - 1;
	ifp->if_hw_tsomaxsegsize = ENA_TSO_MAXSIZE;

	if_setifheaderlen(ifp, sizeof(struct ether_vlan_header));
	if_setcapenable(ifp, if_getcapabilities(ifp));

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&adapter->media, IFM_IMASK, ena_media_change,
	    ena_media_status);
	ifmedia_add(&adapter->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&adapter->media, IFM_ETHER | IFM_AUTO);

	ether_ifattach(ifp, adapter->mac_addr);

	return (0);
}

void
ena_down(struct ena_adapter *adapter)
{
	int rc;

	ENA_LOCK_ASSERT();

	if (!ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter))
		return;

	ena_log(adapter->pdev, INFO, "device is going DOWN\n");

	ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_DEV_UP, adapter);
	if_setdrvflagbits(adapter->ifp, IFF_DRV_OACTIVE, IFF_DRV_RUNNING);

	ena_free_io_irq(adapter);

	if (ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, adapter)) {
		rc = ena_com_dev_reset(adapter->ena_dev, adapter->reset_reason);
		if (unlikely(rc != 0))
			ena_log(adapter->pdev, ERR, "Device reset failed\n");
	}

	ena_destroy_all_io_queues(adapter);

	ena_free_all_tx_bufs(adapter);
	ena_free_all_rx_bufs(adapter);
	ena_free_all_tx_resources(adapter);
	ena_free_all_rx_resources(adapter);

	counter_u64_add(adapter->dev_stats.interface_down, 1);
}

static uint32_t
ena_calc_max_io_queue_num(device_t pdev, struct ena_com_dev *ena_dev,
    struct ena_com_dev_get_features_ctx *get_feat_ctx)
{
	uint32_t io_tx_sq_num, io_tx_cq_num, io_rx_num, max_num_io_queues;

	/* Regular queues capabilities */
	if (ena_dev->supported_features & BIT(ENA_ADMIN_MAX_QUEUES_EXT)) {
		struct ena_admin_queue_ext_feature_fields *max_queue_ext =
		    &get_feat_ctx->max_queue_ext.max_queue_ext;
		io_rx_num = min_t(int, max_queue_ext->max_rx_sq_num,
		    max_queue_ext->max_rx_cq_num);

		io_tx_sq_num = max_queue_ext->max_tx_sq_num;
		io_tx_cq_num = max_queue_ext->max_tx_cq_num;
	} else {
		struct ena_admin_queue_feature_desc *max_queues =
		    &get_feat_ctx->max_queues;
		io_tx_sq_num = max_queues->max_sq_num;
		io_tx_cq_num = max_queues->max_cq_num;
		io_rx_num = min_t(int, io_tx_sq_num, io_tx_cq_num);
	}

	/* In case of LLQ use the llq fields for the tx SQ/CQ */
	if (ena_dev->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_DEV)
		io_tx_sq_num = get_feat_ctx->llq.max_llq_num;

	max_num_io_queues = min_t(uint32_t, mp_ncpus, ENA_MAX_NUM_IO_QUEUES);
	max_num_io_queues = min_t(uint32_t, max_num_io_queues, io_rx_num);
	max_num_io_queues = min_t(uint32_t, max_num_io_queues, io_tx_sq_num);
	max_num_io_queues = min_t(uint32_t, max_num_io_queues, io_tx_cq_num);
	/* 1 IRQ for mgmnt and 1 IRQ for each TX/RX pair */
	max_num_io_queues = min_t(uint32_t, max_num_io_queues,
	    pci_msix_count(pdev) - 1);
#ifdef RSS
	max_num_io_queues = min_t(uint32_t, max_num_io_queues,
	    rss_getnumbuckets());
#endif

	return (max_num_io_queues);
}

static int
ena_enable_wc(device_t pdev, struct resource *res)
{
#if defined(__i386) || defined(__amd64) || defined(__aarch64__)
	vm_offset_t va;
	vm_size_t len;
	int rc;

	va = (vm_offset_t)rman_get_virtual(res);
	len = rman_get_size(res);
	/* Enable write combining */
	rc = pmap_change_attr(va, len, VM_MEMATTR_WRITE_COMBINING);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR, "pmap_change_attr failed, %d\n", rc);
		return (rc);
	}

	return (0);
#endif
	return (EOPNOTSUPP);
}

static int
ena_set_queues_placement_policy(device_t pdev, struct ena_com_dev *ena_dev,
    struct ena_admin_feature_llq_desc *llq,
    struct ena_llq_configurations *llq_default_configurations)
{
	int rc;
	uint32_t llq_feature_mask;

	llq_feature_mask = 1 << ENA_ADMIN_LLQ;
	if (!(ena_dev->supported_features & llq_feature_mask)) {
		ena_log(pdev, WARN,
		    "LLQ is not supported. Fallback to host mode policy.\n");
		ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
		return (0);
	}

	if (ena_dev->mem_bar == NULL) {
		ena_log(pdev, WARN,
		    "LLQ is advertised as supported but device doesn't expose mem bar.\n");
		ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
		return (0);
	}

	rc = ena_com_config_dev_mode(ena_dev, llq, llq_default_configurations);
	if (unlikely(rc != 0)) {
		ena_log(pdev, WARN,
		    "Failed to configure the device mode. "
		    "Fallback to host mode policy.\n");
		ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
	}

	return (0);
}

static int
ena_map_llq_mem_bar(device_t pdev, struct ena_com_dev *ena_dev)
{
	struct ena_adapter *adapter = device_get_softc(pdev);
	int rc, rid;

	/* Try to allocate resources for LLQ bar */
	rid = PCIR_BAR(ENA_MEM_BAR);
	adapter->memory = bus_alloc_resource_any(pdev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (unlikely(adapter->memory == NULL)) {
		ena_log(pdev, WARN,
		    "Unable to allocate LLQ bar resource. LLQ mode won't be used.\n");
		return (0);
	}

	/* Enable write combining for better LLQ performance */
	rc = ena_enable_wc(adapter->pdev, adapter->memory);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR, "failed to enable write combining.\n");
		return (rc);
	}

	/*
	 * Save virtual address of the device's memory region
	 * for the ena_com layer.
	 */
	ena_dev->mem_bar = rman_get_virtual(adapter->memory);

	return (0);
}

static inline void
set_default_llq_configurations(struct ena_llq_configurations *llq_config,
    struct ena_admin_feature_llq_desc *llq)
{
	llq_config->llq_header_location = ENA_ADMIN_INLINE_HEADER;
	llq_config->llq_stride_ctrl = ENA_ADMIN_MULTIPLE_DESCS_PER_ENTRY;
	llq_config->llq_num_decs_before_header =
	    ENA_ADMIN_LLQ_NUM_DESCS_BEFORE_HEADER_2;
	if ((llq->entry_size_ctrl_supported & ENA_ADMIN_LIST_ENTRY_SIZE_256B) !=
	    0 && ena_force_large_llq_header) {
		llq_config->llq_ring_entry_size =
		    ENA_ADMIN_LIST_ENTRY_SIZE_256B;
		llq_config->llq_ring_entry_size_value = 256;
	} else {
		llq_config->llq_ring_entry_size =
		    ENA_ADMIN_LIST_ENTRY_SIZE_128B;
		llq_config->llq_ring_entry_size_value = 128;
	}
}

static int
ena_calc_io_queue_size(struct ena_calc_queue_size_ctx *ctx)
{
	struct ena_admin_feature_llq_desc *llq = &ctx->get_feat_ctx->llq;
	struct ena_com_dev *ena_dev = ctx->ena_dev;
	uint32_t tx_queue_size = ENA_DEFAULT_RING_SIZE;
	uint32_t rx_queue_size = ENA_DEFAULT_RING_SIZE;
	uint32_t max_tx_queue_size;
	uint32_t max_rx_queue_size;

	if (ena_dev->supported_features & BIT(ENA_ADMIN_MAX_QUEUES_EXT)) {
		struct ena_admin_queue_ext_feature_fields *max_queue_ext =
		    &ctx->get_feat_ctx->max_queue_ext.max_queue_ext;
		max_rx_queue_size = min_t(uint32_t,
		    max_queue_ext->max_rx_cq_depth,
		    max_queue_ext->max_rx_sq_depth);
		max_tx_queue_size = max_queue_ext->max_tx_cq_depth;

		if (ena_dev->tx_mem_queue_type ==
		    ENA_ADMIN_PLACEMENT_POLICY_DEV)
			max_tx_queue_size = min_t(uint32_t, max_tx_queue_size,
			    llq->max_llq_depth);
		else
			max_tx_queue_size = min_t(uint32_t, max_tx_queue_size,
			    max_queue_ext->max_tx_sq_depth);

		ctx->max_tx_sgl_size = min_t(uint16_t, ENA_PKT_MAX_BUFS,
		    max_queue_ext->max_per_packet_tx_descs);
		ctx->max_rx_sgl_size = min_t(uint16_t, ENA_PKT_MAX_BUFS,
		    max_queue_ext->max_per_packet_rx_descs);
	} else {
		struct ena_admin_queue_feature_desc *max_queues =
		    &ctx->get_feat_ctx->max_queues;
		max_rx_queue_size = min_t(uint32_t, max_queues->max_cq_depth,
		    max_queues->max_sq_depth);
		max_tx_queue_size = max_queues->max_cq_depth;

		if (ena_dev->tx_mem_queue_type ==
		    ENA_ADMIN_PLACEMENT_POLICY_DEV)
			max_tx_queue_size = min_t(uint32_t, max_tx_queue_size,
			    llq->max_llq_depth);
		else
			max_tx_queue_size = min_t(uint32_t, max_tx_queue_size,
			    max_queues->max_sq_depth);

		ctx->max_tx_sgl_size = min_t(uint16_t, ENA_PKT_MAX_BUFS,
		    max_queues->max_packet_tx_descs);
		ctx->max_rx_sgl_size = min_t(uint16_t, ENA_PKT_MAX_BUFS,
		    max_queues->max_packet_rx_descs);
	}

	/* round down to the nearest power of 2 */
	max_tx_queue_size = 1 << (flsl(max_tx_queue_size) - 1);
	max_rx_queue_size = 1 << (flsl(max_rx_queue_size) - 1);

	/*
	 * When forcing large headers, we multiply the entry size by 2,
	 * and therefore divide the queue size by 2, leaving the amount
	 * of memory used by the queues unchanged.
	 */
	if (ena_force_large_llq_header) {
		if ((llq->entry_size_ctrl_supported &
		    ENA_ADMIN_LIST_ENTRY_SIZE_256B) != 0 &&
		    ena_dev->tx_mem_queue_type ==
		    ENA_ADMIN_PLACEMENT_POLICY_DEV) {
			max_tx_queue_size /= 2;
			ena_log(ctx->pdev, INFO,
			    "Forcing large headers and decreasing maximum Tx queue size to %d\n",
			    max_tx_queue_size);
		} else {
			ena_log(ctx->pdev, WARN,
			    "Forcing large headers failed: LLQ is disabled or device does not support large headers\n");
		}
	}

	tx_queue_size = clamp_val(tx_queue_size, ENA_MIN_RING_SIZE,
	    max_tx_queue_size);
	rx_queue_size = clamp_val(rx_queue_size, ENA_MIN_RING_SIZE,
	    max_rx_queue_size);

	tx_queue_size = 1 << (flsl(tx_queue_size) - 1);
	rx_queue_size = 1 << (flsl(rx_queue_size) - 1);

	ctx->max_tx_queue_size = max_tx_queue_size;
	ctx->max_rx_queue_size = max_rx_queue_size;
	ctx->tx_queue_size = tx_queue_size;
	ctx->rx_queue_size = rx_queue_size;

	return (0);
}

static void
ena_config_host_info(struct ena_com_dev *ena_dev, device_t dev)
{
	struct ena_admin_host_info *host_info;
	uintptr_t rid;
	int rc;

	/* Allocate only the host info */
	rc = ena_com_allocate_host_info(ena_dev);
	if (unlikely(rc != 0)) {
		ena_log(dev, ERR, "Cannot allocate host info\n");
		return;
	}

	host_info = ena_dev->host_attr.host_info;

	if (pci_get_id(dev, PCI_ID_RID, &rid) == 0)
		host_info->bdf = rid;
	host_info->os_type = ENA_ADMIN_OS_FREEBSD;
	host_info->kernel_ver = osreldate;

	sprintf(host_info->kernel_ver_str, "%d", osreldate);
	host_info->os_dist = 0;
	strncpy(host_info->os_dist_str, osrelease,
	    sizeof(host_info->os_dist_str) - 1);

	host_info->driver_version = (ENA_DRV_MODULE_VER_MAJOR) |
	    (ENA_DRV_MODULE_VER_MINOR << ENA_ADMIN_HOST_INFO_MINOR_SHIFT) |
	    (ENA_DRV_MODULE_VER_SUBMINOR << ENA_ADMIN_HOST_INFO_SUB_MINOR_SHIFT);
	host_info->num_cpus = mp_ncpus;
	host_info->driver_supported_features =
	    ENA_ADMIN_HOST_INFO_RX_OFFSET_MASK |
	    ENA_ADMIN_HOST_INFO_RSS_CONFIGURABLE_FUNCTION_KEY_MASK;

	rc = ena_com_set_host_attributes(ena_dev);
	if (unlikely(rc != 0)) {
		if (rc == EOPNOTSUPP)
			ena_log(dev, WARN, "Cannot set host attributes\n");
		else
			ena_log(dev, ERR, "Cannot set host attributes\n");

		goto err;
	}

	return;

err:
	ena_com_delete_host_info(ena_dev);
}

static int
ena_device_init(struct ena_adapter *adapter, device_t pdev,
    struct ena_com_dev_get_features_ctx *get_feat_ctx, int *wd_active)
{
	struct ena_llq_configurations llq_config;
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	bool readless_supported;
	uint32_t aenq_groups;
	int dma_width;
	int rc;

	rc = ena_com_mmio_reg_read_request_init(ena_dev);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR, "failed to init mmio read less\n");
		return (rc);
	}

	/*
	 * The PCIe configuration space revision id indicate if mmio reg
	 * read is disabled
	 */
	readless_supported = !(pci_get_revid(pdev) & ENA_MMIO_DISABLE_REG_READ);
	ena_com_set_mmio_read_mode(ena_dev, readless_supported);

	rc = ena_com_dev_reset(ena_dev, ENA_REGS_RESET_NORMAL);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR, "Can not reset device\n");
		goto err_mmio_read_less;
	}

	rc = ena_com_validate_version(ena_dev);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR, "device version is too low\n");
		goto err_mmio_read_less;
	}

	dma_width = ena_com_get_dma_width(ena_dev);
	if (unlikely(dma_width < 0)) {
		ena_log(pdev, ERR, "Invalid dma width value %d", dma_width);
		rc = dma_width;
		goto err_mmio_read_less;
	}
	adapter->dma_width = dma_width;

	/* ENA admin level init */
	rc = ena_com_admin_init(ena_dev, &aenq_handlers);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR,
		    "Can not initialize ena admin queue with device\n");
		goto err_mmio_read_less;
	}

	/*
	 * To enable the msix interrupts the driver needs to know the number
	 * of queues. So the driver uses polling mode to retrieve this
	 * information
	 */
	ena_com_set_admin_polling_mode(ena_dev, true);

	ena_config_host_info(ena_dev, pdev);

	/* Get Device Attributes */
	rc = ena_com_get_dev_attr_feat(ena_dev, get_feat_ctx);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR,
		    "Cannot get attribute for ena device rc: %d\n", rc);
		goto err_admin_init;
	}

	aenq_groups = BIT(ENA_ADMIN_LINK_CHANGE) |
	    BIT(ENA_ADMIN_FATAL_ERROR) |
	    BIT(ENA_ADMIN_WARNING) |
	    BIT(ENA_ADMIN_NOTIFICATION) |
	    BIT(ENA_ADMIN_KEEP_ALIVE);

	aenq_groups &= get_feat_ctx->aenq.supported_groups;
	rc = ena_com_set_aenq_config(ena_dev, aenq_groups);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR, "Cannot configure aenq groups rc: %d\n", rc);
		goto err_admin_init;
	}

	*wd_active = !!(aenq_groups & BIT(ENA_ADMIN_KEEP_ALIVE));

	set_default_llq_configurations(&llq_config, &get_feat_ctx->llq);

	rc = ena_set_queues_placement_policy(pdev, ena_dev, &get_feat_ctx->llq,
	    &llq_config);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR, "Failed to set placement policy\n");
		goto err_admin_init;
	}

	return (0);

err_admin_init:
	ena_com_delete_host_info(ena_dev);
	ena_com_admin_destroy(ena_dev);
err_mmio_read_less:
	ena_com_mmio_reg_read_request_destroy(ena_dev);

	return (rc);
}

static int
ena_enable_msix_and_set_admin_interrupts(struct ena_adapter *adapter)
{
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	int rc;

	rc = ena_enable_msix(adapter);
	if (unlikely(rc != 0)) {
		ena_log(adapter->pdev, ERR, "Error with MSI-X enablement\n");
		return (rc);
	}

	ena_setup_mgmnt_intr(adapter);

	rc = ena_request_mgmnt_irq(adapter);
	if (unlikely(rc != 0)) {
		ena_log(adapter->pdev, ERR, "Cannot setup mgmnt queue intr\n");
		goto err_disable_msix;
	}

	ena_com_set_admin_polling_mode(ena_dev, false);

	ena_com_admin_aenq_enable(ena_dev);

	return (0);

err_disable_msix:
	ena_disable_msix(adapter);

	return (rc);
}

/* Function called on ENA_ADMIN_KEEP_ALIVE event */
static void
ena_keep_alive_wd(void *adapter_data, struct ena_admin_aenq_entry *aenq_e)
{
	struct ena_adapter *adapter = (struct ena_adapter *)adapter_data;
	struct ena_admin_aenq_keep_alive_desc *desc;
	sbintime_t stime;
	uint64_t rx_drops;
	uint64_t tx_drops;

	desc = (struct ena_admin_aenq_keep_alive_desc *)aenq_e;

	rx_drops = ((uint64_t)desc->rx_drops_high << 32) | desc->rx_drops_low;
	tx_drops = ((uint64_t)desc->tx_drops_high << 32) | desc->tx_drops_low;
	counter_u64_zero(adapter->hw_stats.rx_drops);
	counter_u64_add(adapter->hw_stats.rx_drops, rx_drops);
	counter_u64_zero(adapter->hw_stats.tx_drops);
	counter_u64_add(adapter->hw_stats.tx_drops, tx_drops);

	stime = getsbinuptime();
	atomic_store_rel_64(&adapter->keep_alive_timestamp, stime);
}

/* Check for keep alive expiration */
static void
check_for_missing_keep_alive(struct ena_adapter *adapter)
{
	sbintime_t timestamp, time;

	if (adapter->wd_active == 0)
		return;

	if (adapter->keep_alive_timeout == ENA_HW_HINTS_NO_TIMEOUT)
		return;

	timestamp = atomic_load_acq_64(&adapter->keep_alive_timestamp);
	time = getsbinuptime() - timestamp;
	if (unlikely(time > adapter->keep_alive_timeout)) {
		ena_log(adapter->pdev, ERR, "Keep alive watchdog timeout.\n");
		counter_u64_add(adapter->dev_stats.wd_expired, 1);
		ena_trigger_reset(adapter, ENA_REGS_RESET_KEEP_ALIVE_TO);
	}
}

/* Check if admin queue is enabled */
static void
check_for_admin_com_state(struct ena_adapter *adapter)
{
	if (unlikely(ena_com_get_admin_running_state(adapter->ena_dev) == false)) {
		ena_log(adapter->pdev, ERR,
		    "ENA admin queue is not in running state!\n");
		counter_u64_add(adapter->dev_stats.admin_q_pause, 1);
		ena_trigger_reset(adapter, ENA_REGS_RESET_ADMIN_TO);
	}
}

static int
check_for_rx_interrupt_queue(struct ena_adapter *adapter,
    struct ena_ring *rx_ring)
{
	if (likely(atomic_load_8(&rx_ring->first_interrupt)))
		return (0);

	if (ena_com_cq_empty(rx_ring->ena_com_io_cq))
		return (0);

	rx_ring->no_interrupt_event_cnt++;

	if (rx_ring->no_interrupt_event_cnt ==
	    ENA_MAX_NO_INTERRUPT_ITERATIONS) {
		ena_log(adapter->pdev, ERR,
		    "Potential MSIX issue on Rx side Queue = %d. Reset the device\n",
		    rx_ring->qid);
		ena_trigger_reset(adapter, ENA_REGS_RESET_MISS_INTERRUPT);
		return (EIO);
	}

	return (0);
}

static int
check_missing_comp_in_tx_queue(struct ena_adapter *adapter,
    struct ena_ring *tx_ring)
{
	device_t pdev = adapter->pdev;
	struct bintime curtime, time;
	struct ena_tx_buffer *tx_buf;
	int time_since_last_cleanup;
	int missing_tx_comp_to;
	sbintime_t time_offset;
	uint32_t missed_tx = 0;
	int i, rc = 0;

	getbinuptime(&curtime);

	for (i = 0; i < tx_ring->ring_size; i++) {
		tx_buf = &tx_ring->tx_buffer_info[i];

		if (bintime_isset(&tx_buf->timestamp) == 0)
			continue;

		time = curtime;
		bintime_sub(&time, &tx_buf->timestamp);
		time_offset = bttosbt(time);

		if (unlikely(!atomic_load_8(&tx_ring->first_interrupt) &&
		    time_offset > 2 * adapter->missing_tx_timeout)) {
			/*
			 * If after graceful period interrupt is still not
			 * received, we schedule a reset.
			 */
			ena_log(pdev, ERR,
			    "Potential MSIX issue on Tx side Queue = %d. "
			    "Reset the device\n",
			    tx_ring->qid);
			ena_trigger_reset(adapter,
			    ENA_REGS_RESET_MISS_INTERRUPT);
			return (EIO);
		}

		/* Check again if packet is still waiting */
		if (unlikely(time_offset > adapter->missing_tx_timeout)) {

			if (tx_buf->print_once) {
				time_since_last_cleanup = TICKS_2_MSEC(ticks -
				    tx_ring->tx_last_cleanup_ticks);
				missing_tx_comp_to = sbttoms(
				    adapter->missing_tx_timeout);
				ena_log(pdev, WARN,
				    "Found a Tx that wasn't completed on time, qid %d, index %d. "
				    "%d msecs have passed since last cleanup. Missing Tx timeout value %d msecs.\n",
				    tx_ring->qid, i, time_since_last_cleanup,
				    missing_tx_comp_to);
			}

			tx_buf->print_once = false;
			missed_tx++;
		}
	}

	if (unlikely(missed_tx > adapter->missing_tx_threshold)) {
		ena_log(pdev, ERR,
		    "The number of lost tx completion is above the threshold "
		    "(%d > %d). Reset the device\n",
		    missed_tx, adapter->missing_tx_threshold);
		ena_trigger_reset(adapter, ENA_REGS_RESET_MISS_TX_CMPL);
		rc = EIO;
	}

	counter_u64_add(tx_ring->tx_stats.missing_tx_comp, missed_tx);

	return (rc);
}

/*
 * Check for TX which were not completed on time.
 * Timeout is defined by "missing_tx_timeout".
 * Reset will be performed if number of incompleted
 * transactions exceeds "missing_tx_threshold".
 */
static void
check_for_missing_completions(struct ena_adapter *adapter)
{
	struct ena_ring *tx_ring;
	struct ena_ring *rx_ring;
	int i, budget, rc;

	/* Make sure the driver doesn't turn the device in other process */
	rmb();

	if (!ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter))
		return;

	if (ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, adapter))
		return;

	if (adapter->missing_tx_timeout == ENA_HW_HINTS_NO_TIMEOUT)
		return;

	budget = adapter->missing_tx_max_queues;

	for (i = adapter->next_monitored_tx_qid; i < adapter->num_io_queues; i++) {
		tx_ring = &adapter->tx_ring[i];
		rx_ring = &adapter->rx_ring[i];

		rc = check_missing_comp_in_tx_queue(adapter, tx_ring);
		if (unlikely(rc != 0))
			return;

		rc = check_for_rx_interrupt_queue(adapter, rx_ring);
		if (unlikely(rc != 0))
			return;

		budget--;
		if (budget == 0) {
			i++;
			break;
		}
	}

	adapter->next_monitored_tx_qid = i % adapter->num_io_queues;
}

/* trigger rx cleanup after 2 consecutive detections */
#define EMPTY_RX_REFILL 2
/* For the rare case where the device runs out of Rx descriptors and the
 * msix handler failed to refill new Rx descriptors (due to a lack of memory
 * for example).
 * This case will lead to a deadlock:
 * The device won't send interrupts since all the new Rx packets will be dropped
 * The msix handler won't allocate new Rx descriptors so the device won't be
 * able to send new packets.
 *
 * When such a situation is detected - execute rx cleanup task in another thread
 */
static void
check_for_empty_rx_ring(struct ena_adapter *adapter)
{
	struct ena_ring *rx_ring;
	int i, refill_required;

	if (!ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter))
		return;

	if (ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, adapter))
		return;

	for (i = 0; i < adapter->num_io_queues; i++) {
		rx_ring = &adapter->rx_ring[i];

		refill_required = ena_com_free_q_entries(
		    rx_ring->ena_com_io_sq);
		if (unlikely(refill_required == (rx_ring->ring_size - 1))) {
			rx_ring->empty_rx_queue++;

			if (rx_ring->empty_rx_queue >= EMPTY_RX_REFILL) {
				counter_u64_add(rx_ring->rx_stats.empty_rx_ring,
				    1);

				ena_log(adapter->pdev, WARN,
				    "Rx ring %d is stalled. Triggering the refill function\n",
				    i);

				taskqueue_enqueue(rx_ring->que->cleanup_tq,
				    &rx_ring->que->cleanup_task);
				rx_ring->empty_rx_queue = 0;
			}
		} else {
			rx_ring->empty_rx_queue = 0;
		}
	}
}

static void
ena_update_hints(struct ena_adapter *adapter,
    struct ena_admin_ena_hw_hints *hints)
{
	struct ena_com_dev *ena_dev = adapter->ena_dev;

	if (hints->admin_completion_tx_timeout)
		ena_dev->admin_queue.completion_timeout =
		    hints->admin_completion_tx_timeout * 1000;

	if (hints->mmio_read_timeout)
		/* convert to usec */
		ena_dev->mmio_read.reg_read_to = hints->mmio_read_timeout * 1000;

	if (hints->missed_tx_completion_count_threshold_to_reset)
		adapter->missing_tx_threshold =
		    hints->missed_tx_completion_count_threshold_to_reset;

	if (hints->missing_tx_completion_timeout) {
		if (hints->missing_tx_completion_timeout ==
		    ENA_HW_HINTS_NO_TIMEOUT)
			adapter->missing_tx_timeout = ENA_HW_HINTS_NO_TIMEOUT;
		else
			adapter->missing_tx_timeout = SBT_1MS *
			    hints->missing_tx_completion_timeout;
	}

	if (hints->driver_watchdog_timeout) {
		if (hints->driver_watchdog_timeout == ENA_HW_HINTS_NO_TIMEOUT)
			adapter->keep_alive_timeout = ENA_HW_HINTS_NO_TIMEOUT;
		else
			adapter->keep_alive_timeout = SBT_1MS *
			    hints->driver_watchdog_timeout;
	}
}

/**
 * ena_copy_eni_metrics - Get and copy ENI metrics from the HW.
 * @adapter: ENA device adapter
 *
 * Returns 0 on success, EOPNOTSUPP if current HW doesn't support those metrics
 * and other error codes on failure.
 *
 * This function can possibly cause a race with other calls to the admin queue.
 * Because of that, the caller should either lock this function or make sure
 * that there is no race in the current context.
 */
static int
ena_copy_eni_metrics(struct ena_adapter *adapter)
{
	static bool print_once = true;
	int rc;

	rc = ena_com_get_eni_stats(adapter->ena_dev, &adapter->eni_metrics);

	if (rc != 0) {
		if (rc == ENA_COM_UNSUPPORTED) {
			if (print_once) {
				ena_log(adapter->pdev, WARN,
				    "Retrieving ENI metrics is not supported.\n");
				print_once = false;
			} else {
				ena_log(adapter->pdev, DBG,
				    "Retrieving ENI metrics is not supported.\n");
			}
		} else {
			ena_log(adapter->pdev, ERR,
			    "Failed to get ENI metrics: %d\n", rc);
		}
	}

	return (rc);
}

static int
ena_copy_srd_metrics(struct ena_adapter *adapter)
{
	return ena_com_get_ena_srd_info(adapter->ena_dev, &adapter->ena_srd_info);
}

static int
ena_copy_customer_metrics(struct ena_adapter *adapter)
{
	struct ena_com_dev *dev;
	u32 supported_metrics_count;
	int rc, len;

	dev = adapter->ena_dev;

	supported_metrics_count = ena_com_get_customer_metric_count(dev);
	len = supported_metrics_count * sizeof(u64);

	/* Fill the data buffer */
	rc = ena_com_get_customer_metrics(adapter->ena_dev,
	    (char *)(adapter->customer_metrics_array), len);

	return (rc);
}

static void
ena_timer_service(void *data)
{
	struct ena_adapter *adapter = (struct ena_adapter *)data;
	struct ena_admin_host_info *host_info =
	    adapter->ena_dev->host_attr.host_info;

	check_for_missing_keep_alive(adapter);

	check_for_admin_com_state(adapter);

	check_for_missing_completions(adapter);

	check_for_empty_rx_ring(adapter);

	/*
	 * User controller update of the ENA metrics.
	 * If the delay was set to 0, then the stats shouldn't be updated at
	 * all.
	 * Otherwise, wait 'metrics_sample_interval' seconds, before
	 * updating stats.
	 * As timer service is executed every second, it's enough to increment
	 * appropriate counter each time the timer service is executed.
	 */
	if ((adapter->metrics_sample_interval != 0) &&
	    (++adapter->metrics_sample_interval_cnt >=
	    adapter->metrics_sample_interval)) {
		taskqueue_enqueue(adapter->metrics_tq, &adapter->metrics_task);
		adapter->metrics_sample_interval_cnt = 0;
	}


	if (host_info != NULL)
		ena_update_host_info(host_info, adapter->ifp);

	if (unlikely(ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, adapter))) {
		/*
		 * Timeout when validating version indicates that the device
		 * became unresponsive. If that happens skip the reset and
		 * reschedule timer service, so the reset can be retried later.
		 */
		if (ena_com_validate_version(adapter->ena_dev) ==
		    ENA_COM_TIMER_EXPIRED) {
			ena_log(adapter->pdev, WARN,
			    "FW unresponsive, skipping reset\n");
			ENA_TIMER_RESET(adapter);
			return;
		}
		ena_log(adapter->pdev, WARN, "Trigger reset is on\n");
		taskqueue_enqueue(adapter->reset_tq, &adapter->reset_task);
		return;
	}

	/*
	 * Schedule another timeout one second from now.
	 */
	ENA_TIMER_RESET(adapter);
}

void
ena_destroy_device(struct ena_adapter *adapter, bool graceful)
{
	if_t ifp = adapter->ifp;
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	bool dev_up;

	if (!ENA_FLAG_ISSET(ENA_FLAG_DEVICE_RUNNING, adapter))
		return;

	if (!graceful)
		if_link_state_change(ifp, LINK_STATE_DOWN);

	ENA_TIMER_DRAIN(adapter);

	dev_up = ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter);
	if (dev_up)
		ENA_FLAG_SET_ATOMIC(ENA_FLAG_DEV_UP_BEFORE_RESET, adapter);

	if (!graceful)
		ena_com_set_admin_running_state(ena_dev, false);

	if (ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter))
		ena_down(adapter);

	/*
	 * Stop the device from sending AENQ events (if the device was up, and
	 * the trigger reset was on, ena_down already performs device reset)
	 */
	if (!(ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, adapter) && dev_up))
		ena_com_dev_reset(adapter->ena_dev, adapter->reset_reason);

	ena_free_mgmnt_irq(adapter);

	ena_disable_msix(adapter);

	/*
	 * IO rings resources should be freed because `ena_restore_device()`
	 * calls (not directly) `ena_enable_msix()`, which re-allocates MSIX
	 * vectors. The amount of MSIX vectors after destroy-restore may be
	 * different than before. Therefore, IO rings resources should be
	 * established from scratch each time.
	 */
	ena_free_all_io_rings_resources(adapter);

	ena_com_abort_admin_commands(ena_dev);

	ena_com_wait_for_abort_completion(ena_dev);

	ena_com_admin_destroy(ena_dev);

	ena_com_mmio_reg_read_request_destroy(ena_dev);

	adapter->reset_reason = ENA_REGS_RESET_NORMAL;

	ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_TRIGGER_RESET, adapter);
	ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_DEVICE_RUNNING, adapter);
}

static int
ena_device_validate_params(struct ena_adapter *adapter,
    struct ena_com_dev_get_features_ctx *get_feat_ctx)
{
	if (memcmp(get_feat_ctx->dev_attr.mac_addr, adapter->mac_addr,
	    ETHER_ADDR_LEN) != 0) {
		ena_log(adapter->pdev, ERR, "Error, mac addresses differ\n");
		return (EINVAL);
	}

	if (get_feat_ctx->dev_attr.max_mtu < if_getmtu(adapter->ifp)) {
		ena_log(adapter->pdev, ERR,
		    "Error, device max mtu is smaller than ifp MTU\n");
		return (EINVAL);
	}

	return 0;
}

int
ena_restore_device(struct ena_adapter *adapter)
{
	struct ena_com_dev_get_features_ctx get_feat_ctx;
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	if_t ifp = adapter->ifp;
	device_t dev = adapter->pdev;
	int wd_active;
	int rc;

	ENA_FLAG_SET_ATOMIC(ENA_FLAG_ONGOING_RESET, adapter);

	rc = ena_device_init(adapter, dev, &get_feat_ctx, &wd_active);
	if (rc != 0) {
		ena_log(dev, ERR, "Cannot initialize device\n");
		goto err;
	}
	/*
	 * Only enable WD if it was enabled before reset, so it won't override
	 * value set by the user by the sysctl.
	 */
	if (adapter->wd_active != 0)
		adapter->wd_active = wd_active;

	rc = ena_device_validate_params(adapter, &get_feat_ctx);
	if (rc != 0) {
		ena_log(dev, ERR, "Validation of device parameters failed\n");
		goto err_device_destroy;
	}

	ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_ONGOING_RESET, adapter);
	/* Make sure we don't have a race with AENQ Links state handler */
	if (ENA_FLAG_ISSET(ENA_FLAG_LINK_UP, adapter))
		if_link_state_change(ifp, LINK_STATE_UP);

	rc = ena_enable_msix_and_set_admin_interrupts(adapter);
	if (rc != 0) {
		ena_log(dev, ERR, "Enable MSI-X failed\n");
		goto err_device_destroy;
	}

	/*
	 * Effective value of used MSIX vectors should be the same as before
	 * `ena_destroy_device()`, if possible, or closest to it if less vectors
	 * are available.
	 */
	if ((adapter->msix_vecs - ENA_ADMIN_MSIX_VEC) < adapter->num_io_queues)
		adapter->num_io_queues = adapter->msix_vecs - ENA_ADMIN_MSIX_VEC;

	/* Re-initialize rings basic information */
	ena_init_io_rings(adapter);

	/* If the interface was up before the reset bring it up */
	if (ENA_FLAG_ISSET(ENA_FLAG_DEV_UP_BEFORE_RESET, adapter)) {
		rc = ena_up(adapter);
		if (rc != 0) {
			ena_log(dev, ERR, "Failed to create I/O queues\n");
			goto err_disable_msix;
		}
	}

	/* Indicate that device is running again and ready to work */
	ENA_FLAG_SET_ATOMIC(ENA_FLAG_DEVICE_RUNNING, adapter);

	/*
	 * As the AENQ handlers weren't executed during reset because
	 * the flag ENA_FLAG_DEVICE_RUNNING was turned off, the
	 * timestamp must be updated again That will prevent next reset
	 * caused by missing keep alive.
	 */
	adapter->keep_alive_timestamp = getsbinuptime();
	ENA_TIMER_RESET(adapter);

	ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_DEV_UP_BEFORE_RESET, adapter);

	return (rc);

err_disable_msix:
	ena_free_mgmnt_irq(adapter);
	ena_disable_msix(adapter);
err_device_destroy:
	ena_com_abort_admin_commands(ena_dev);
	ena_com_wait_for_abort_completion(ena_dev);
	ena_com_admin_destroy(ena_dev);
	ena_com_dev_reset(ena_dev, ENA_REGS_RESET_DRIVER_INVALID_STATE);
	ena_com_mmio_reg_read_request_destroy(ena_dev);
err:
	ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_DEVICE_RUNNING, adapter);
	ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_ONGOING_RESET, adapter);
	ena_log(dev, ERR, "Reset attempt failed. Can not reset the device\n");

	return (rc);
}

static void
ena_metrics_task(void *arg, int pending)
{
	struct ena_adapter *adapter = (struct ena_adapter *)arg;

	ENA_LOCK_LOCK();

	if (ena_com_get_cap(adapter->ena_dev, ENA_ADMIN_CUSTOMER_METRICS))
		(void)ena_copy_customer_metrics(adapter);
	else if (ena_com_get_cap(adapter->ena_dev, ENA_ADMIN_ENI_STATS))
		(void)ena_copy_eni_metrics(adapter);

	if (ena_com_get_cap(adapter->ena_dev, ENA_ADMIN_ENA_SRD_INFO))
		(void)ena_copy_srd_metrics(adapter);

	ENA_LOCK_UNLOCK();
}

static void
ena_reset_task(void *arg, int pending)
{
	struct ena_adapter *adapter = (struct ena_adapter *)arg;

	ENA_LOCK_LOCK();
	if (likely(ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, adapter))) {
		ena_destroy_device(adapter, false);
		ena_restore_device(adapter);

		ena_log(adapter->pdev, INFO,
		    "Device reset completed successfully, Driver info: %s\n",
		    ena_version);
	}
	ENA_LOCK_UNLOCK();
}

static void
ena_free_stats(struct ena_adapter *adapter)
{
	ena_free_counters((counter_u64_t *)&adapter->hw_stats,
	    sizeof(struct ena_hw_stats));
	ena_free_counters((counter_u64_t *)&adapter->dev_stats,
	    sizeof(struct ena_stats_dev));

}
/**
 * ena_attach - Device Initialization Routine
 * @pdev: device information struct
 *
 * Returns 0 on success, otherwise on failure.
 *
 * ena_attach initializes an adapter identified by a device structure.
 * The OS initialization, configuring of the adapter private structure,
 * and a hardware reset occur.
 **/
static int
ena_attach(device_t pdev)
{
	struct ena_com_dev_get_features_ctx get_feat_ctx;
	struct ena_calc_queue_size_ctx calc_queue_ctx = { 0 };
	static int version_printed;
	struct ena_adapter *adapter;
	struct ena_com_dev *ena_dev = NULL;
	uint32_t max_num_io_queues;
	int msix_rid;
	int rid, rc;

	adapter = device_get_softc(pdev);
	adapter->pdev = pdev;
	adapter->first_bind = -1;

	/*
	 * Set up the timer service - driver is responsible for avoiding
	 * concurrency, as the callout won't be using any locking inside.
	 */
	ENA_TIMER_INIT(adapter);
	adapter->keep_alive_timeout = ENA_DEFAULT_KEEP_ALIVE_TO;
	adapter->missing_tx_timeout = ENA_DEFAULT_TX_CMP_TO;
	adapter->missing_tx_max_queues = ENA_DEFAULT_TX_MONITORED_QUEUES;
	adapter->missing_tx_threshold = ENA_DEFAULT_TX_CMP_THRESHOLD;

	adapter->irq_cpu_base = ENA_BASE_CPU_UNSPECIFIED;
	adapter->irq_cpu_stride = 0;

#ifdef RSS
	adapter->rss_enabled = 1;
#endif

	if (version_printed++ == 0)
		ena_log(pdev, INFO, "%s\n", ena_version);

	/* Allocate memory for ena_dev structure */
	ena_dev = malloc(sizeof(struct ena_com_dev), M_DEVBUF,
	    M_WAITOK | M_ZERO);

	adapter->ena_dev = ena_dev;
	ena_dev->dmadev = pdev;

	rid = PCIR_BAR(ENA_REG_BAR);
	adapter->memory = NULL;
	adapter->registers = bus_alloc_resource_any(pdev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (unlikely(adapter->registers == NULL)) {
		ena_log(pdev, ERR,
		    "unable to allocate bus resource: registers!\n");
		rc = ENOMEM;
		goto err_dev_free;
	}

	/* MSIx vector table may reside on BAR0 with registers or on BAR1. */
	msix_rid = pci_msix_table_bar(pdev);
	if (msix_rid != rid) {
		adapter->msix = bus_alloc_resource_any(pdev, SYS_RES_MEMORY,
		    &msix_rid, RF_ACTIVE);
		if (unlikely(adapter->msix == NULL)) {
			ena_log(pdev, ERR,
			    "unable to allocate bus resource: msix!\n");
			rc = ENOMEM;
			goto err_pci_free;
		}
		adapter->msix_rid = msix_rid;
	}

	ena_dev->bus = malloc(sizeof(struct ena_bus), M_DEVBUF,
	    M_WAITOK | M_ZERO);

	/* Store register resources */
	((struct ena_bus *)(ena_dev->bus))->reg_bar_t = rman_get_bustag(
	    adapter->registers);
	((struct ena_bus *)(ena_dev->bus))->reg_bar_h = rman_get_bushandle(
	    adapter->registers);

	if (unlikely(((struct ena_bus *)(ena_dev->bus))->reg_bar_h == 0)) {
		ena_log(pdev, ERR, "failed to pmap registers bar\n");
		rc = ENXIO;
		goto err_bus_free;
	}

	rc = ena_map_llq_mem_bar(pdev, ena_dev);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR, "Failed to map ENA mem bar");
		goto err_bus_free;
	}

	/* Initially clear all the flags */
	ENA_FLAG_ZERO(adapter);

	/* Device initialization */
	rc = ena_device_init(adapter, pdev, &get_feat_ctx, &adapter->wd_active);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR, "ENA device init failed! (err: %d)\n", rc);
		rc = ENXIO;
		goto err_bus_free;
	}

	if (ena_dev->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_DEV)
		adapter->disable_meta_caching = !!(
		    get_feat_ctx.llq.accel_mode.u.get.supported_flags &
		    BIT(ENA_ADMIN_DISABLE_META_CACHING));

	adapter->keep_alive_timestamp = getsbinuptime();

	adapter->tx_offload_cap = get_feat_ctx.offload.tx;

	memcpy(adapter->mac_addr, get_feat_ctx.dev_attr.mac_addr,
	    ETHER_ADDR_LEN);

	calc_queue_ctx.pdev = pdev;
	calc_queue_ctx.ena_dev = ena_dev;
	calc_queue_ctx.get_feat_ctx = &get_feat_ctx;

	/* Calculate initial and maximum IO queue number and size */
	max_num_io_queues = ena_calc_max_io_queue_num(pdev, ena_dev,
	    &get_feat_ctx);
	rc = ena_calc_io_queue_size(&calc_queue_ctx);
	if (unlikely((rc != 0) || (max_num_io_queues <= 0))) {
		rc = EFAULT;
		goto err_com_free;
	}

	adapter->requested_tx_ring_size = calc_queue_ctx.tx_queue_size;
	adapter->requested_rx_ring_size = calc_queue_ctx.rx_queue_size;
	adapter->max_tx_ring_size = calc_queue_ctx.max_tx_queue_size;
	adapter->max_rx_ring_size = calc_queue_ctx.max_rx_queue_size;
	adapter->max_tx_sgl_size = calc_queue_ctx.max_tx_sgl_size;
	adapter->max_rx_sgl_size = calc_queue_ctx.max_rx_sgl_size;

	adapter->max_num_io_queues = max_num_io_queues;

	adapter->buf_ring_size = ENA_DEFAULT_BUF_RING_SIZE;

	adapter->max_mtu = get_feat_ctx.dev_attr.max_mtu;

	adapter->reset_reason = ENA_REGS_RESET_NORMAL;

	/* set up dma tags for rx and tx buffers */
	rc = ena_setup_tx_dma_tag(adapter);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR, "Failed to create TX DMA tag\n");
		goto err_com_free;
	}

	rc = ena_setup_rx_dma_tag(adapter);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR, "Failed to create RX DMA tag\n");
		goto err_tx_tag_free;
	}

	/*
	 * The amount of requested MSIX vectors is equal to
	 * adapter::max_num_io_queues (see `ena_enable_msix()`), plus a constant
	 * number of admin queue interrupts. The former is initially determined
	 * by HW capabilities (see `ena_calc_max_io_queue_num())` but may not be
	 * achieved if there are not enough system resources. By default, the
	 * number of effectively used IO queues is the same but later on it can
	 * be limited by the user using sysctl interface.
	 */
	rc = ena_enable_msix_and_set_admin_interrupts(adapter);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR,
		    "Failed to enable and set the admin interrupts\n");
		goto err_io_free;
	}
	/* By default all of allocated MSIX vectors are actively used */
	adapter->num_io_queues = adapter->msix_vecs - ENA_ADMIN_MSIX_VEC;

	/* initialize rings basic information */
	ena_init_io_rings(adapter);

	rc = ena_com_allocate_customer_metrics_buffer(ena_dev);
	if (rc) {
		ena_log(pdev, ERR, "Failed to allocate customer metrics buffer.\n");
		goto err_msix_free;
	}

	rc = ena_sysctl_allocate_customer_metrics_buffer(adapter);
	if (unlikely(rc)){
		ena_log(pdev, ERR, "Failed to allocate sysctl customer metrics buffer.\n");
		goto err_metrics_buffer_destroy;
	}

	/* Initialize statistics */
	ena_alloc_counters((counter_u64_t *)&adapter->dev_stats,
	    sizeof(struct ena_stats_dev));
	ena_alloc_counters((counter_u64_t *)&adapter->hw_stats,
	    sizeof(struct ena_hw_stats));
	ena_sysctl_add_nodes(adapter);

	/* setup network interface */
	rc = ena_setup_ifnet(pdev, adapter, &get_feat_ctx);
	if (unlikely(rc != 0)) {
		ena_log(pdev, ERR, "Error with network interface setup\n");
		goto err_customer_metrics_alloc;
	}

	/* Initialize reset task queue */
	TASK_INIT(&adapter->reset_task, 0, ena_reset_task, adapter);
	adapter->reset_tq = taskqueue_create("ena_reset_enqueue",
	    M_WAITOK | M_ZERO, taskqueue_thread_enqueue, &adapter->reset_tq);
	taskqueue_start_threads(&adapter->reset_tq, 1, PI_NET, "%s rstq",
	    device_get_nameunit(adapter->pdev));

	/* Initialize metrics task queue */
	TASK_INIT(&adapter->metrics_task, 0, ena_metrics_task, adapter);
	adapter->metrics_tq = taskqueue_create("ena_metrics_enqueue",
	    M_WAITOK | M_ZERO, taskqueue_thread_enqueue, &adapter->metrics_tq);
	taskqueue_start_threads(&adapter->metrics_tq, 1, PI_NET, "%s metricsq",
	    device_get_nameunit(adapter->pdev));

#ifdef DEV_NETMAP
	rc = ena_netmap_attach(adapter);
	if (rc != 0) {
		ena_log(pdev, ERR, "netmap attach failed: %d\n", rc);
		goto err_detach;
	}
#endif /* DEV_NETMAP */

	/* Tell the stack that the interface is not active */
	if_setdrvflagbits(adapter->ifp, IFF_DRV_OACTIVE, IFF_DRV_RUNNING);
	ENA_FLAG_SET_ATOMIC(ENA_FLAG_DEVICE_RUNNING, adapter);

	/* Run the timer service */
	ENA_TIMER_RESET(adapter);

	return (0);

#ifdef DEV_NETMAP
err_detach:
	ether_ifdetach(adapter->ifp);
#endif /* DEV_NETMAP */
err_customer_metrics_alloc:
	free(adapter->customer_metrics_array, M_DEVBUF);
err_metrics_buffer_destroy:
	ena_com_delete_customer_metrics_buffer(ena_dev);
err_msix_free:
	ena_free_stats(adapter);
	ena_com_dev_reset(adapter->ena_dev, ENA_REGS_RESET_INIT_ERR);
	ena_free_mgmnt_irq(adapter);
	ena_disable_msix(adapter);
err_io_free:
	ena_free_all_io_rings_resources(adapter);
	ena_free_rx_dma_tag(adapter);
err_tx_tag_free:
	ena_free_tx_dma_tag(adapter);
err_com_free:
	ena_com_admin_destroy(ena_dev);
	ena_com_delete_host_info(ena_dev);
	ena_com_mmio_reg_read_request_destroy(ena_dev);
err_bus_free:
	free(ena_dev->bus, M_DEVBUF);
err_pci_free:
	ena_free_pci_resources(adapter);
err_dev_free:
	free(ena_dev, M_DEVBUF);

	return (rc);
}

/**
 * ena_detach - Device Removal Routine
 * @pdev: device information struct
 *
 * ena_detach is called by the device subsystem to alert the driver
 * that it should release a PCI device.
 **/
static int
ena_detach(device_t pdev)
{
	struct ena_adapter *adapter = device_get_softc(pdev);
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	int rc;

	/* Make sure VLANS are not using driver */
	if (adapter->ifp->if_vlantrunk != NULL) {
		ena_log(adapter->pdev, ERR, "VLAN is in use, detach first\n");
		return (EBUSY);
	}

	ether_ifdetach(adapter->ifp);

	/* Stop timer service */
	ENA_LOCK_LOCK();
	ENA_TIMER_DRAIN(adapter);
	ENA_LOCK_UNLOCK();

	/* Release metrics task */
	while (taskqueue_cancel(adapter->metrics_tq, &adapter->metrics_task, NULL))
		taskqueue_drain(adapter->metrics_tq, &adapter->metrics_task);
	taskqueue_free(adapter->metrics_tq);

	/* Release reset task */
	while (taskqueue_cancel(adapter->reset_tq, &adapter->reset_task, NULL))
		taskqueue_drain(adapter->reset_tq, &adapter->reset_task);
	taskqueue_free(adapter->reset_tq);

	ENA_LOCK_LOCK();
	ena_down(adapter);
	ena_destroy_device(adapter, true);
	ENA_LOCK_UNLOCK();

	/* Restore unregistered sysctl queue nodes. */
	ena_sysctl_update_queue_node_nb(adapter, adapter->num_io_queues,
	    adapter->max_num_io_queues);

#ifdef DEV_NETMAP
	netmap_detach(adapter->ifp);
#endif /* DEV_NETMAP */

	ena_free_stats(adapter);

	rc = ena_free_rx_dma_tag(adapter);
	if (unlikely(rc != 0))
		ena_log(adapter->pdev, WARN,
		    "Unmapped RX DMA tag associations\n");

	rc = ena_free_tx_dma_tag(adapter);
	if (unlikely(rc != 0))
		ena_log(adapter->pdev, WARN,
		    "Unmapped TX DMA tag associations\n");

	ena_free_irqs(adapter);

	ena_free_pci_resources(adapter);

	if (adapter->rss_indir != NULL)
		free(adapter->rss_indir, M_DEVBUF);

	if (likely(ENA_FLAG_ISSET(ENA_FLAG_RSS_ACTIVE, adapter)))
		ena_com_rss_destroy(ena_dev);

	ena_com_delete_host_info(ena_dev);

	free(adapter->customer_metrics_array, M_DEVBUF);

	ena_com_delete_customer_metrics_buffer(ena_dev);

	if_free(adapter->ifp);

	free(ena_dev->bus, M_DEVBUF);

	free(ena_dev, M_DEVBUF);

	return (bus_generic_detach(pdev));
}

/******************************************************************************
 ******************************** AENQ Handlers *******************************
 *****************************************************************************/
/**
 * ena_update_on_link_change:
 * Notify the network interface about the change in link status
 **/
static void
ena_update_on_link_change(void *adapter_data,
    struct ena_admin_aenq_entry *aenq_e)
{
	struct ena_adapter *adapter = (struct ena_adapter *)adapter_data;
	struct ena_admin_aenq_link_change_desc *aenq_desc;
	int status;
	if_t ifp;

	aenq_desc = (struct ena_admin_aenq_link_change_desc *)aenq_e;
	ifp = adapter->ifp;
	status = aenq_desc->flags &
	    ENA_ADMIN_AENQ_LINK_CHANGE_DESC_LINK_STATUS_MASK;

	if (status != 0) {
		ena_log(adapter->pdev, INFO, "link is UP\n");
		ENA_FLAG_SET_ATOMIC(ENA_FLAG_LINK_UP, adapter);
		if (!ENA_FLAG_ISSET(ENA_FLAG_ONGOING_RESET, adapter))
			if_link_state_change(ifp, LINK_STATE_UP);
	} else {
		ena_log(adapter->pdev, INFO, "link is DOWN\n");
		if_link_state_change(ifp, LINK_STATE_DOWN);
		ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_LINK_UP, adapter);
	}
}

static void
ena_notification(void *adapter_data, struct ena_admin_aenq_entry *aenq_e)
{
	struct ena_adapter *adapter = (struct ena_adapter *)adapter_data;
	struct ena_admin_ena_hw_hints *hints;

	ENA_WARN(aenq_e->aenq_common_desc.group != ENA_ADMIN_NOTIFICATION,
	    adapter->ena_dev, "Invalid group(%x) expected %x\n",
	    aenq_e->aenq_common_desc.group, ENA_ADMIN_NOTIFICATION);

	switch (aenq_e->aenq_common_desc.syndrome) {
	case ENA_ADMIN_UPDATE_HINTS:
		hints =
		    (struct ena_admin_ena_hw_hints *)(&aenq_e->inline_data_w4);
		ena_update_hints(adapter, hints);
		break;
	default:
		ena_log(adapter->pdev, ERR,
		    "Invalid aenq notification link state %d\n",
		    aenq_e->aenq_common_desc.syndrome);
	}
}

static void
ena_lock_init(void *arg)
{
	ENA_LOCK_INIT();
}
SYSINIT(ena_lock_init, SI_SUB_LOCK, SI_ORDER_FIRST, ena_lock_init, NULL);

static void
ena_lock_uninit(void *arg)
{
	ENA_LOCK_DESTROY();
}
SYSUNINIT(ena_lock_uninit, SI_SUB_LOCK, SI_ORDER_FIRST, ena_lock_uninit, NULL);

/**
 * This handler will called for unknown event group or unimplemented handlers
 **/
static void
unimplemented_aenq_handler(void *adapter_data,
    struct ena_admin_aenq_entry *aenq_e)
{
	struct ena_adapter *adapter = (struct ena_adapter *)adapter_data;

	ena_log(adapter->pdev, ERR,
	    "Unknown event was received or event with unimplemented handler\n");
}

static struct ena_aenq_handlers aenq_handlers = {
    .handlers = {
	    [ENA_ADMIN_LINK_CHANGE] = ena_update_on_link_change,
	    [ENA_ADMIN_NOTIFICATION] = ena_notification,
	    [ENA_ADMIN_KEEP_ALIVE] = ena_keep_alive_wd,
    },
    .unimplemented_handler = unimplemented_aenq_handler
};

/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/

static device_method_t ena_methods[] = { /* Device interface */
	DEVMETHOD(device_probe, ena_probe),
	DEVMETHOD(device_attach, ena_attach),
	DEVMETHOD(device_detach, ena_detach), DEVMETHOD_END
};

static driver_t ena_driver = {
	"ena",
	ena_methods,
	sizeof(struct ena_adapter),
};

DRIVER_MODULE(ena, pci, ena_driver, 0, 0);
MODULE_PNP_INFO("U16:vendor;U16:device", pci, ena, ena_vendor_info_array,
    nitems(ena_vendor_info_array) - 1);
MODULE_DEPEND(ena, pci, 1, 1, 1);
MODULE_DEPEND(ena, ether, 1, 1, 1);
#ifdef DEV_NETMAP
MODULE_DEPEND(ena, netmap, 1, 1, 1);
#endif /* DEV_NETMAP */

/*********************************************************************/
