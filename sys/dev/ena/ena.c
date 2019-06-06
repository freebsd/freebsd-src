/*-
 * BSD LICENSE
 *
 * Copyright (c) 2015-2019 Amazon.com, Inc. or its affiliates.
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
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
#include <sys/eventhandler.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/in_cksum.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include "ena.h"
#include "ena_sysctl.h"

/*********************************************************
 *  Function prototypes
 *********************************************************/
static int	ena_probe(device_t);
static void	ena_intr_msix_mgmnt(void *);
static void	ena_free_pci_resources(struct ena_adapter *);
static int	ena_change_mtu(if_t, int);
static inline void ena_alloc_counters(counter_u64_t *, int);
static inline void ena_free_counters(counter_u64_t *, int);
static inline void ena_reset_counters(counter_u64_t *, int);
static void	ena_init_io_rings_common(struct ena_adapter *,
    struct ena_ring *, uint16_t);
static void	ena_init_io_rings(struct ena_adapter *);
static void	ena_free_io_ring_resources(struct ena_adapter *, unsigned int);
static void	ena_free_all_io_rings_resources(struct ena_adapter *);
static int	ena_setup_tx_dma_tag(struct ena_adapter *);
static int	ena_free_tx_dma_tag(struct ena_adapter *);
static int	ena_setup_rx_dma_tag(struct ena_adapter *);
static int	ena_free_rx_dma_tag(struct ena_adapter *);
static int	ena_setup_tx_resources(struct ena_adapter *, int);
static void	ena_free_tx_resources(struct ena_adapter *, int);
static int	ena_setup_all_tx_resources(struct ena_adapter *);
static void	ena_free_all_tx_resources(struct ena_adapter *);
static inline int validate_rx_req_id(struct ena_ring *, uint16_t);
static int	ena_setup_rx_resources(struct ena_adapter *, unsigned int);
static void	ena_free_rx_resources(struct ena_adapter *, unsigned int);
static int	ena_setup_all_rx_resources(struct ena_adapter *);
static void	ena_free_all_rx_resources(struct ena_adapter *);
static inline int ena_alloc_rx_mbuf(struct ena_adapter *, struct ena_ring *,
    struct ena_rx_buffer *);
static void	ena_free_rx_mbuf(struct ena_adapter *, struct ena_ring *,
    struct ena_rx_buffer *);
static int	ena_refill_rx_bufs(struct ena_ring *, uint32_t);
static void	ena_free_rx_bufs(struct ena_adapter *, unsigned int);
static void	ena_refill_all_rx_bufs(struct ena_adapter *);
static void	ena_free_all_rx_bufs(struct ena_adapter *);
static void	ena_free_tx_bufs(struct ena_adapter *, unsigned int);
static void	ena_free_all_tx_bufs(struct ena_adapter *);
static void	ena_destroy_all_tx_queues(struct ena_adapter *);
static void	ena_destroy_all_rx_queues(struct ena_adapter *);
static void	ena_destroy_all_io_queues(struct ena_adapter *);
static int	ena_create_io_queues(struct ena_adapter *);
static int	ena_tx_cleanup(struct ena_ring *);
static int	ena_rx_cleanup(struct ena_ring *);
static inline int validate_tx_req_id(struct ena_ring *, uint16_t);
static void	ena_rx_hash_mbuf(struct ena_ring *, struct ena_com_rx_ctx *,
    struct mbuf *);
static struct mbuf* ena_rx_mbuf(struct ena_ring *, struct ena_com_rx_buf_info *,
    struct ena_com_rx_ctx *, uint16_t *);
static inline void ena_rx_checksum(struct ena_ring *, struct ena_com_rx_ctx *,
    struct mbuf *);
static void	ena_cleanup(void *arg, int pending);
static int	ena_handle_msix(void *);
static int	ena_enable_msix(struct ena_adapter *);
static void	ena_setup_mgmnt_intr(struct ena_adapter *);
static int	ena_setup_io_intr(struct ena_adapter *);
static int	ena_request_mgmnt_irq(struct ena_adapter *);
static int	ena_request_io_irq(struct ena_adapter *);
static void	ena_free_mgmnt_irq(struct ena_adapter *);
static void	ena_free_io_irq(struct ena_adapter *);
static void	ena_free_irqs(struct ena_adapter*);
static void	ena_disable_msix(struct ena_adapter *);
static void	ena_unmask_all_io_irqs(struct ena_adapter *);
static int	ena_rss_configure(struct ena_adapter *);
static int	ena_up_complete(struct ena_adapter *);
static int	ena_up(struct ena_adapter *);
static void	ena_down(struct ena_adapter *);
static uint64_t	ena_get_counter(if_t, ift_counter);
static int	ena_media_change(if_t);
static void	ena_media_status(if_t, struct ifmediareq *);
static void	ena_init(void *);
static int	ena_ioctl(if_t, u_long, caddr_t);
static int	ena_get_dev_offloads(struct ena_com_dev_get_features_ctx *);
static void	ena_update_host_info(struct ena_admin_host_info *, if_t);
static void	ena_update_hwassist(struct ena_adapter *);
static int	ena_setup_ifnet(device_t, struct ena_adapter *,
    struct ena_com_dev_get_features_ctx *);
static void	ena_tx_csum(struct ena_com_tx_ctx *, struct mbuf *);
static int	ena_check_and_collapse_mbuf(struct ena_ring *tx_ring,
    struct mbuf **mbuf);
static void	ena_dmamap_llq(void *, bus_dma_segment_t *, int, int);
static int	ena_xmit_mbuf(struct ena_ring *, struct mbuf **);
static void	ena_start_xmit(struct ena_ring *);
static int	ena_mq_start(if_t, struct mbuf *);
static void	ena_deferred_mq_start(void *, int);
static void	ena_qflush(if_t);
static int	ena_enable_wc(struct resource *);
static int	ena_set_queues_placement_policy(device_t, struct ena_com_dev *,
    struct ena_admin_feature_llq_desc *, struct ena_llq_configurations *);
static int	ena_calc_io_queue_num(struct ena_adapter *,
    struct ena_com_dev_get_features_ctx *);
static int	ena_calc_queue_size(struct ena_adapter *,
    struct ena_calc_queue_size_ctx *);
static int	ena_handle_updated_queues(struct ena_adapter *,
    struct ena_com_dev_get_features_ctx *);
static int	ena_rss_init_default(struct ena_adapter *);
static void	ena_rss_init_default_deferred(void *);
static void	ena_config_host_info(struct ena_com_dev *, device_t);
static int	ena_attach(device_t);
static int	ena_detach(device_t);
static int	ena_device_init(struct ena_adapter *, device_t,
    struct ena_com_dev_get_features_ctx *, int *);
static int	ena_enable_msix_and_set_admin_interrupts(struct ena_adapter *,
    int);
static void ena_update_on_link_change(void *, struct ena_admin_aenq_entry *);
static void	unimplemented_aenq_handler(void *,
    struct ena_admin_aenq_entry *);
static void	ena_timer_service(void *);

static char ena_version[] = DEVICE_NAME DRV_MODULE_NAME " v" DRV_MODULE_VERSION;

static ena_vendor_info_t ena_vendor_info_array[] = {
    { PCI_VENDOR_ID_AMAZON, PCI_DEV_ID_ENA_PF, 0},
    { PCI_VENDOR_ID_AMAZON, PCI_DEV_ID_ENA_LLQ_PF, 0},
    { PCI_VENDOR_ID_AMAZON, PCI_DEV_ID_ENA_VF, 0},
    { PCI_VENDOR_ID_AMAZON, PCI_DEV_ID_ENA_LLQ_VF, 0},
    /* Last entry */
    { 0, 0, 0 }
};

/*
 * Contains pointers to event handlers, e.g. link state chage.
 */
static struct ena_aenq_handlers aenq_handlers;

void
ena_dmamap_callback(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error != 0)
		return;
	*(bus_addr_t *) arg = segs[0].ds_addr;
}

int
ena_dma_alloc(device_t dmadev, bus_size_t size,
    ena_mem_handle_t *dma , int mapflags)
{
	struct ena_adapter* adapter = device_get_softc(dmadev);
	uint32_t maxsize;
	uint64_t dma_space_addr;
	int error;

	maxsize = ((size - 1) / PAGE_SIZE + 1) * PAGE_SIZE;

	dma_space_addr = ENA_DMA_BIT_MASK(adapter->dma_width);
	if (unlikely(dma_space_addr == 0))
		dma_space_addr = BUS_SPACE_MAXADDR;

	error = bus_dma_tag_create(bus_get_dma_tag(dmadev), /* parent */
	    8, 0,	      /* alignment, bounds 		*/
	    dma_space_addr,   /* lowaddr of exclusion window	*/
	    BUS_SPACE_MAXADDR,/* highaddr of exclusion window	*/
	    NULL, NULL,	      /* filter, filterarg 		*/
	    maxsize,	      /* maxsize 			*/
	    1,		      /* nsegments 			*/
	    maxsize,	      /* maxsegsize 			*/
	    BUS_DMA_ALLOCNOW, /* flags 				*/
	    NULL,	      /* lockfunc 			*/
	    NULL,	      /* lockarg 			*/
	    &dma->tag);
	if (unlikely(error != 0)) {
		ena_trace(ENA_ALERT, "bus_dma_tag_create failed: %d\n", error);
		goto fail_tag;
	}

	error = bus_dmamem_alloc(dma->tag, (void**) &dma->vaddr,
	    BUS_DMA_COHERENT | BUS_DMA_ZERO, &dma->map);
	if (unlikely(error != 0)) {
		ena_trace(ENA_ALERT, "bus_dmamem_alloc(%ju) failed: %d\n",
		    (uintmax_t)size, error);
		goto fail_map_create;
	}

	dma->paddr = 0;
	error = bus_dmamap_load(dma->tag, dma->map, dma->vaddr,
	    size, ena_dmamap_callback, &dma->paddr, mapflags);
	if (unlikely((error != 0) || (dma->paddr == 0))) {
		ena_trace(ENA_ALERT, ": bus_dmamap_load failed: %d\n", error);
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
}

static int
ena_probe(device_t dev)
{
	ena_vendor_info_t *ent;
	char		adapter_name[60];
	uint16_t	pci_vendor_id = 0;
	uint16_t	pci_device_id = 0;

	pci_vendor_id = pci_get_vendor(dev);
	pci_device_id = pci_get_device(dev);

	ent = ena_vendor_info_array;
	while (ent->vendor_id != 0) {
		if ((pci_vendor_id == ent->vendor_id) &&
		    (pci_device_id == ent->device_id)) {
			ena_trace(ENA_DBG, "vendor=%x device=%x\n",
			    pci_vendor_id, pci_device_id);

			sprintf(adapter_name, DEVICE_DESC);
			device_set_desc_copy(dev, adapter_name);
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
	int rc;

	if ((new_mtu > adapter->max_mtu) || (new_mtu < ENA_MIN_MTU)) {
		device_printf(adapter->pdev, "Invalid MTU setting. "
		    "new_mtu: %d max mtu: %d min mtu: %d\n",
		    new_mtu, adapter->max_mtu, ENA_MIN_MTU);
		return (EINVAL);
	}

	rc = ena_com_set_dev_mtu(adapter->ena_dev, new_mtu);
	if (likely(rc == 0)) {
		ena_trace(ENA_DBG, "set MTU to %d\n", new_mtu);
		if_setmtu(ifp, new_mtu);
	} else {
		device_printf(adapter->pdev, "Failed to set MTU to %d\n",
		    new_mtu);
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
	ring->first_interrupt = false;
	ring->no_interrupt_event_cnt = 0;
}

static void
ena_init_io_rings(struct ena_adapter *adapter)
{
	struct ena_com_dev *ena_dev;
	struct ena_ring *txr, *rxr;
	struct ena_que *que;
	int i;

	ena_dev = adapter->ena_dev;

	for (i = 0; i < adapter->num_queues; i++) {
		txr = &adapter->tx_ring[i];
		rxr = &adapter->rx_ring[i];

		/* TX/RX common ring state */
		ena_init_io_rings_common(adapter, txr, i);
		ena_init_io_rings_common(adapter, rxr, i);

		/* TX specific ring state */
		txr->ring_size = adapter->tx_ring_size;
		txr->tx_max_header_size = ena_dev->tx_max_header_size;
		txr->tx_mem_queue_type = ena_dev->tx_mem_queue_type;
		txr->smoothed_interval =
		    ena_com_get_nonadaptive_moderation_interval_tx(ena_dev);

		/* Allocate a buf ring */
		txr->buf_ring_size = adapter->buf_ring_size;
		txr->br = buf_ring_alloc(txr->buf_ring_size, M_DEVBUF,
		    M_WAITOK, &txr->ring_mtx);

		/* Alloc TX statistics. */
		ena_alloc_counters((counter_u64_t *)&txr->tx_stats,
		    sizeof(txr->tx_stats));

		/* RX specific ring state */
		rxr->ring_size = adapter->rx_ring_size;
		rxr->smoothed_interval =
		    ena_com_get_nonadaptive_moderation_interval_rx(ena_dev);

		/* Alloc RX statistics. */
		ena_alloc_counters((counter_u64_t *)&rxr->rx_stats,
		    sizeof(rxr->rx_stats));

		/* Initialize locks */
		snprintf(txr->mtx_name, nitems(txr->mtx_name), "%s:tx(%d)",
		    device_get_nameunit(adapter->pdev), i);
		snprintf(rxr->mtx_name, nitems(rxr->mtx_name), "%s:rx(%d)",
		    device_get_nameunit(adapter->pdev), i);

		mtx_init(&txr->ring_mtx, txr->mtx_name, NULL, MTX_DEF);

		que = &adapter->que[i];
		que->adapter = adapter;
		que->id = i;
		que->tx_ring = txr;
		que->rx_ring = rxr;

		txr->que = que;
		rxr->que = que;

		rxr->empty_rx_queue = 0;
	}
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

	for (i = 0; i < adapter->num_queues; i++)
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
	    BUS_SPACE_MAXADDR, 			  /* highaddr of excl window */
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
	    BUS_SPACE_MAXADDR, 			  /* highaddr of excl window */
	    NULL, NULL,				  /* filter, filterarg 	     */
	    MJUM16BYTES,			  /* maxsize 		     */
	    adapter->max_rx_sgl_size,		  /* nsegments 		     */
	    MJUM16BYTES,			  /* maxsegsize 	     */
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
	struct ena_que *que = &adapter->que[qid];
	struct ena_ring *tx_ring = que->tx_ring;
	int size, i, err;

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
		    &tx_ring->tx_buffer_info[i].map_head);
		if (unlikely(err != 0)) {
			ena_trace(ENA_ALERT,
			    "Unable to create Tx DMA map_head for buffer %d\n",
			    i);
			goto err_buf_info_unmap;
		}
		tx_ring->tx_buffer_info[i].seg_mapped = false;

		err = bus_dmamap_create(adapter->tx_buf_tag, 0,
		    &tx_ring->tx_buffer_info[i].map_seg);
		if (unlikely(err != 0)) {
			ena_trace(ENA_ALERT,
			    "Unable to create Tx DMA map_seg for buffer %d\n",
			    i);
			goto err_buf_info_head_unmap;
		}
		tx_ring->tx_buffer_info[i].head_mapped = false;
	}

	/* Allocate taskqueues */
	TASK_INIT(&tx_ring->enqueue_task, 0, ena_deferred_mq_start, tx_ring);
	tx_ring->enqueue_tq = taskqueue_create_fast("ena_tx_enque", M_NOWAIT,
	    taskqueue_thread_enqueue, &tx_ring->enqueue_tq);
	if (unlikely(tx_ring->enqueue_tq == NULL)) {
		ena_trace(ENA_ALERT,
		    "Unable to create taskqueue for enqueue task\n");
		i = tx_ring->ring_size;
		goto err_buf_info_unmap;
	}

	tx_ring->running = true;

	taskqueue_start_threads(&tx_ring->enqueue_tq, 1, PI_NET,
	    "%s txeq %d", device_get_nameunit(adapter->pdev), que->cpu);

	return (0);

err_buf_info_head_unmap:
	bus_dmamap_destroy(adapter->tx_buf_tag,
	    tx_ring->tx_buffer_info[i].map_head);
err_buf_info_unmap:
	while (i--) {
		bus_dmamap_destroy(adapter->tx_buf_tag,
		    tx_ring->tx_buffer_info[i].map_head);
		bus_dmamap_destroy(adapter->tx_buf_tag,
		    tx_ring->tx_buffer_info[i].map_seg);
	}
	free(tx_ring->push_buf_intermediate_buf, M_DEVBUF);
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

	while (taskqueue_cancel(tx_ring->enqueue_tq, &tx_ring->enqueue_task,
	    NULL))
		taskqueue_drain(tx_ring->enqueue_tq, &tx_ring->enqueue_task);

	taskqueue_free(tx_ring->enqueue_tq);

	ENA_RING_MTX_LOCK(tx_ring);
	/* Flush buffer ring, */
	drbr_flush(adapter->ifp, tx_ring->br);

	/* Free buffer DMA maps, */
	for (int i = 0; i < tx_ring->ring_size; i++) {
		if (tx_ring->tx_buffer_info[i].head_mapped == true) {
			bus_dmamap_sync(adapter->tx_buf_tag,
			    tx_ring->tx_buffer_info[i].map_head,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(adapter->tx_buf_tag,
			    tx_ring->tx_buffer_info[i].map_head);
			tx_ring->tx_buffer_info[i].head_mapped = false;
		}
		bus_dmamap_destroy(adapter->tx_buf_tag,
		    tx_ring->tx_buffer_info[i].map_head);

		if (tx_ring->tx_buffer_info[i].seg_mapped == true) {
			bus_dmamap_sync(adapter->tx_buf_tag,
			    tx_ring->tx_buffer_info[i].map_seg,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(adapter->tx_buf_tag,
			    tx_ring->tx_buffer_info[i].map_seg);
			tx_ring->tx_buffer_info[i].seg_mapped = false;
		}
		bus_dmamap_destroy(adapter->tx_buf_tag,
		    tx_ring->tx_buffer_info[i].map_seg);

		m_freem(tx_ring->tx_buffer_info[i].mbuf);
		tx_ring->tx_buffer_info[i].mbuf = NULL;
	}
	ENA_RING_MTX_UNLOCK(tx_ring);

	/* And free allocated memory. */
	free(tx_ring->tx_buffer_info, M_DEVBUF);
	tx_ring->tx_buffer_info = NULL;

	free(tx_ring->free_tx_ids, M_DEVBUF);
	tx_ring->free_tx_ids = NULL;

	ENA_MEM_FREE(adapter->ena_dev->dmadev,
	    tx_ring->push_buf_intermediate_buf);
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

	for (i = 0; i < adapter->num_queues; i++) {
		rc = ena_setup_tx_resources(adapter, i);
		if (rc != 0) {
			device_printf(adapter->pdev,
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

	for (i = 0; i < adapter->num_queues; i++)
		ena_free_tx_resources(adapter, i);
}

static inline int
validate_rx_req_id(struct ena_ring *rx_ring, uint16_t req_id)
{
	if (likely(req_id < rx_ring->ring_size))
		return (0);

	device_printf(rx_ring->adapter->pdev, "Invalid rx req_id: %hu\n",
	    req_id);
	counter_u64_add(rx_ring->rx_stats.bad_req_id, 1);

	/* Trigger device reset */
	if (likely(!ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, rx_ring->adapter))) {
		rx_ring->adapter->reset_reason = ENA_REGS_RESET_INV_RX_REQ_ID;
		ENA_FLAG_SET_ATOMIC(ENA_FLAG_TRIGGER_RESET, rx_ring->adapter);
	}

	return (EFAULT);
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
	struct ena_que *que = &adapter->que[qid];
	struct ena_ring *rx_ring = que->rx_ring;
	int size, err, i;

	size = sizeof(struct ena_rx_buffer) * rx_ring->ring_size;

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
			ena_trace(ENA_ALERT,
			    "Unable to create Rx DMA map for buffer %d\n", i);
			goto err_buf_info_unmap;
		}
	}

	/* Create LRO for the ring */
	if ((adapter->ifp->if_capenable & IFCAP_LRO) != 0) {
		int err = tcp_lro_init(&rx_ring->lro);
		if (err != 0) {
			device_printf(adapter->pdev,
			    "LRO[%d] Initialization failed!\n", qid);
		} else {
			ena_trace(ENA_INFO,
			    "RX Soft LRO[%d] Initialized\n", qid);
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

	for (i = 0; i < adapter->num_queues; i++) {
		rc = ena_setup_rx_resources(adapter, i);
		if (rc != 0) {
			device_printf(adapter->pdev,
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

	for (i = 0; i < adapter->num_queues; i++)
		ena_free_rx_resources(adapter, i);
}

static inline int
ena_alloc_rx_mbuf(struct ena_adapter *adapter,
    struct ena_ring *rx_ring, struct ena_rx_buffer *rx_info)
{
	struct ena_com_buf *ena_buf;
	bus_dma_segment_t segs[1];
	int nsegs, error;
	int mlen;

	/* if previous allocated frag is not used */
	if (unlikely(rx_info->mbuf != NULL))
		return (0);

	/* Get mbuf using UMA allocator */
	rx_info->mbuf = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, MJUM16BYTES);

	if (unlikely(rx_info->mbuf == NULL)) {
		counter_u64_add(rx_ring->rx_stats.mjum_alloc_fail, 1);
		rx_info->mbuf = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if (unlikely(rx_info->mbuf == NULL)) {
			counter_u64_add(rx_ring->rx_stats.mbuf_alloc_fail, 1);
			return (ENOMEM);
		}
		mlen = MCLBYTES;
	} else {
		mlen = MJUM16BYTES;
	}
	/* Set mbuf length*/
	rx_info->mbuf->m_pkthdr.len = rx_info->mbuf->m_len = mlen;

	/* Map packets for DMA */
	ena_trace(ENA_DBG | ENA_RSC | ENA_RXPTH,
	    "Using tag %p for buffers' DMA mapping, mbuf %p len: %d\n",
	    adapter->rx_buf_tag,rx_info->mbuf, rx_info->mbuf->m_len);
	error = bus_dmamap_load_mbuf_sg(adapter->rx_buf_tag, rx_info->map,
	    rx_info->mbuf, segs, &nsegs, BUS_DMA_NOWAIT);
	if (unlikely((error != 0) || (nsegs != 1))) {
		ena_trace(ENA_WARNING, "failed to map mbuf, error: %d, "
		    "nsegs: %d\n", error, nsegs);
		counter_u64_add(rx_ring->rx_stats.dma_mapping_err, 1);
		goto exit;

	}

	bus_dmamap_sync(adapter->rx_buf_tag, rx_info->map, BUS_DMASYNC_PREREAD);

	ena_buf = &rx_info->ena_buf;
	ena_buf->paddr = segs[0].ds_addr;
	ena_buf->len = mlen;

	ena_trace(ENA_DBG | ENA_RSC | ENA_RXPTH,
	    "ALLOC RX BUF: mbuf %p, rx_info %p, len %d, paddr %#jx\n",
	    rx_info->mbuf, rx_info,ena_buf->len, (uintmax_t)ena_buf->paddr);

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
		ena_trace(ENA_WARNING, "Trying to free unallocated buffer\n");
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
static int
ena_refill_rx_bufs(struct ena_ring *rx_ring, uint32_t num)
{
	struct ena_adapter *adapter = rx_ring->adapter;
	uint16_t next_to_use, req_id;
	uint32_t i;
	int rc;

	ena_trace(ENA_DBG | ENA_RXPTH | ENA_RSC, "refill qid: %d\n",
	    rx_ring->qid);

	next_to_use = rx_ring->next_to_use;

	for (i = 0; i < num; i++) {
		struct ena_rx_buffer *rx_info;

		ena_trace(ENA_DBG | ENA_RXPTH | ENA_RSC,
		    "RX buffer - next to use: %d\n", next_to_use);

		req_id = rx_ring->free_rx_ids[next_to_use];
		rx_info = &rx_ring->rx_buffer_info[req_id];

		rc = ena_alloc_rx_mbuf(adapter, rx_ring, rx_info);
		if (unlikely(rc != 0)) {
			ena_trace(ENA_WARNING,
			    "failed to alloc buffer for rx queue %d\n",
			    rx_ring->qid);
			break;
		}
		rc = ena_com_add_single_rx_desc(rx_ring->ena_com_io_sq,
		    &rx_info->ena_buf, req_id);
		if (unlikely(rc != 0)) {
			ena_trace(ENA_WARNING,
			    "failed to add buffer for rx queue %d\n",
			    rx_ring->qid);
			break;
		}
		next_to_use = ENA_RX_RING_IDX_NEXT(next_to_use,
		    rx_ring->ring_size);
	}

	if (unlikely(i < num)) {
		counter_u64_add(rx_ring->rx_stats.refil_partial, 1);
		ena_trace(ENA_WARNING,
		     "refilled rx qid %d with only %d mbufs (from %d)\n",
		     rx_ring->qid, i, num);
	}

	if (likely(i != 0)) {
		wmb();
		ena_com_write_sq_doorbell(rx_ring->ena_com_io_sq);
	}
	rx_ring->next_to_use = next_to_use;
	return (i);
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

	for (i = 0; i < adapter->num_queues; i++) {
		rx_ring = &adapter->rx_ring[i];
		bufs_num = rx_ring->ring_size - 1;
		rc = ena_refill_rx_bufs(rx_ring, bufs_num);

		if (unlikely(rc != bufs_num))
			ena_trace(ENA_WARNING, "refilling Queue %d failed. "
			    "Allocated %d buffers from: %d\n", i, rc, bufs_num);
	}
}

static void
ena_free_all_rx_bufs(struct ena_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_queues; i++)
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
			device_printf(adapter->pdev,
			    "free uncompleted tx mbuf qid %d idx 0x%x\n",
			    qid, i);
			print_once = false;
		} else {
			ena_trace(ENA_DBG,
			    "free uncompleted tx mbuf qid %d idx 0x%x\n",
			     qid, i);
		}

		if (tx_info->head_mapped == true) {
			bus_dmamap_sync(adapter->tx_buf_tag, tx_info->map_head,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(adapter->tx_buf_tag,
			    tx_info->map_head);
			tx_info->head_mapped = false;
		}

		if (tx_info->seg_mapped == true) {
			bus_dmamap_sync(adapter->tx_buf_tag, tx_info->map_seg,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(adapter->tx_buf_tag,
			    tx_info->map_seg);
			tx_info->seg_mapped = false;
		}

		m_free(tx_info->mbuf);
		tx_info->mbuf = NULL;
	}
	ENA_RING_MTX_UNLOCK(tx_ring);
}

static void
ena_free_all_tx_bufs(struct ena_adapter *adapter)
{

	for (int i = 0; i < adapter->num_queues; i++)
		ena_free_tx_bufs(adapter, i);
}

static void
ena_destroy_all_tx_queues(struct ena_adapter *adapter)
{
	uint16_t ena_qid;
	int i;

	for (i = 0; i < adapter->num_queues; i++) {
		ena_qid = ENA_IO_TXQ_IDX(i);
		ena_com_destroy_io_queue(adapter->ena_dev, ena_qid);
	}
}

static void
ena_destroy_all_rx_queues(struct ena_adapter *adapter)
{
	uint16_t ena_qid;
	int i;

	for (i = 0; i < adapter->num_queues; i++) {
		ena_qid = ENA_IO_RXQ_IDX(i);
		ena_com_destroy_io_queue(adapter->ena_dev, ena_qid);
	}
}

static void
ena_destroy_all_io_queues(struct ena_adapter *adapter)
{
	struct ena_que *queue;
	int i;

	for (i = 0; i < adapter->num_queues; i++) {
		queue = &adapter->que[i];
		while (taskqueue_cancel(queue->cleanup_tq,
		    &queue->cleanup_task, NULL))
			taskqueue_drain(queue->cleanup_tq,
			    &queue->cleanup_task);
		taskqueue_free(queue->cleanup_tq);
	}

	ena_destroy_all_tx_queues(adapter);
	ena_destroy_all_rx_queues(adapter);
}

static inline int
validate_tx_req_id(struct ena_ring *tx_ring, uint16_t req_id)
{
	struct ena_adapter *adapter = tx_ring->adapter;
	struct ena_tx_buffer *tx_info = NULL;

	if (likely(req_id < tx_ring->ring_size)) {
		tx_info = &tx_ring->tx_buffer_info[req_id];
		if (tx_info->mbuf != NULL)
			return (0);
		device_printf(adapter->pdev,
		    "tx_info doesn't have valid mbuf\n");
	}

	device_printf(adapter->pdev, "Invalid req_id: %hu\n", req_id);
	counter_u64_add(tx_ring->tx_stats.bad_req_id, 1);

	/* Trigger device reset */
	adapter->reset_reason = ENA_REGS_RESET_INV_TX_REQ_ID;
	ENA_FLAG_SET_ATOMIC(ENA_FLAG_TRIGGER_RESET, adapter);

	return (EFAULT);
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
	int rc, i;

	/* Create TX queues */
	for (i = 0; i < adapter->num_queues; i++) {
		msix_vector = ENA_IO_IRQ_IDX(i);
		ena_qid = ENA_IO_TXQ_IDX(i);
		ctx.mem_queue_type = ena_dev->tx_mem_queue_type;
		ctx.direction = ENA_COM_IO_QUEUE_DIRECTION_TX;
		ctx.queue_size = adapter->tx_ring_size;
		ctx.msix_vector = msix_vector;
		ctx.qid = ena_qid;
		rc = ena_com_create_io_queue(ena_dev, &ctx);
		if (rc != 0) {
			device_printf(adapter->pdev,
			    "Failed to create io TX queue #%d rc: %d\n", i, rc);
			goto err_tx;
		}
		ring = &adapter->tx_ring[i];
		rc = ena_com_get_io_handlers(ena_dev, ena_qid,
		    &ring->ena_com_io_sq,
		    &ring->ena_com_io_cq);
		if (rc != 0) {
			device_printf(adapter->pdev,
			    "Failed to get TX queue handlers. TX queue num"
			    " %d rc: %d\n", i, rc);
			ena_com_destroy_io_queue(ena_dev, ena_qid);
			goto err_tx;
		}
	}

	/* Create RX queues */
	for (i = 0; i < adapter->num_queues; i++) {
		msix_vector = ENA_IO_IRQ_IDX(i);
		ena_qid = ENA_IO_RXQ_IDX(i);
		ctx.mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
		ctx.direction = ENA_COM_IO_QUEUE_DIRECTION_RX;
		ctx.queue_size = adapter->rx_ring_size;
		ctx.msix_vector = msix_vector;
		ctx.qid = ena_qid;
		rc = ena_com_create_io_queue(ena_dev, &ctx);
		if (unlikely(rc != 0)) {
			device_printf(adapter->pdev,
			    "Failed to create io RX queue[%d] rc: %d\n", i, rc);
			goto err_rx;
		}

		ring = &adapter->rx_ring[i];
		rc = ena_com_get_io_handlers(ena_dev, ena_qid,
		    &ring->ena_com_io_sq,
		    &ring->ena_com_io_cq);
		if (unlikely(rc != 0)) {
			device_printf(adapter->pdev,
			    "Failed to get RX queue handlers. RX queue num"
			    " %d rc: %d\n", i, rc);
			ena_com_destroy_io_queue(ena_dev, ena_qid);
			goto err_rx;
		}
	}

	for (i = 0; i < adapter->num_queues; i++) {
		queue = &adapter->que[i];

		TASK_INIT(&queue->cleanup_task, 0, ena_cleanup, queue);
		queue->cleanup_tq = taskqueue_create_fast("ena cleanup",
		    M_WAITOK, taskqueue_thread_enqueue, &queue->cleanup_tq);

		taskqueue_start_threads(&queue->cleanup_tq, 1, PI_NET,
		    "%s queue %d cleanup",
		    device_get_nameunit(adapter->pdev), i);
	}

	return (0);

err_rx:
	while (i--)
		ena_com_destroy_io_queue(ena_dev, ENA_IO_RXQ_IDX(i));
	i = adapter->num_queues;
err_tx:
	while (i--)
		ena_com_destroy_io_queue(ena_dev, ENA_IO_TXQ_IDX(i));

	return (ENXIO);
}

/**
 * ena_tx_cleanup - clear sent packets and corresponding descriptors
 * @tx_ring: ring for which we want to clean packets
 *
 * Once packets are sent, we ask the device in a loop for no longer used
 * descriptors. We find the related mbuf chain in a map (index in an array)
 * and free it, then update ring state.
 * This is performed in "endless" loop, updating ring pointers every
 * TX_COMMIT. The first check of free descriptor is performed before the actual
 * loop, then repeated at the loop end.
 **/
static int
ena_tx_cleanup(struct ena_ring *tx_ring)
{
	struct ena_adapter *adapter;
	struct ena_com_io_cq* io_cq;
	uint16_t next_to_clean;
	uint16_t req_id;
	uint16_t ena_qid;
	unsigned int total_done = 0;
	int rc;
	int commit = TX_COMMIT;
	int budget = TX_BUDGET;
	int work_done;
	bool above_thresh;

	adapter = tx_ring->que->adapter;
	ena_qid = ENA_IO_TXQ_IDX(tx_ring->que->id);
	io_cq = &adapter->ena_dev->io_cq_queues[ena_qid];
	next_to_clean = tx_ring->next_to_clean;

	do {
		struct ena_tx_buffer *tx_info;
		struct mbuf *mbuf;

		rc = ena_com_tx_comp_req_id_get(io_cq, &req_id);
		if (unlikely(rc != 0))
			break;

		rc = validate_tx_req_id(tx_ring, req_id);
		if (unlikely(rc != 0))
			break;

		tx_info = &tx_ring->tx_buffer_info[req_id];

		mbuf = tx_info->mbuf;

		tx_info->mbuf = NULL;
		bintime_clear(&tx_info->timestamp);

		/* Map is no longer required */
		if (tx_info->head_mapped == true) {
			bus_dmamap_sync(adapter->tx_buf_tag, tx_info->map_head,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(adapter->tx_buf_tag,
			    tx_info->map_head);
			tx_info->head_mapped = false;
		}
		if (tx_info->seg_mapped == true) {
			bus_dmamap_sync(adapter->tx_buf_tag, tx_info->map_seg,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(adapter->tx_buf_tag,
			    tx_info->map_seg);
			tx_info->seg_mapped = false;
		}

		ena_trace(ENA_DBG | ENA_TXPTH, "tx: q %d mbuf %p completed\n",
		    tx_ring->qid, mbuf);

		m_freem(mbuf);

		total_done += tx_info->tx_descs;

		tx_ring->free_tx_ids[next_to_clean] = req_id;
		next_to_clean = ENA_TX_RING_IDX_NEXT(next_to_clean,
		    tx_ring->ring_size);

		if (unlikely(--commit == 0)) {
			commit = TX_COMMIT;
			/* update ring state every TX_COMMIT descriptor */
			tx_ring->next_to_clean = next_to_clean;
			ena_com_comp_ack(
			    &adapter->ena_dev->io_sq_queues[ena_qid],
			    total_done);
			ena_com_update_dev_comp_head(io_cq);
			total_done = 0;
		}
	} while (likely(--budget));

	work_done = TX_BUDGET - budget;

	ena_trace(ENA_DBG | ENA_TXPTH, "tx: q %d done. total pkts: %d\n",
	tx_ring->qid, work_done);

	/* If there is still something to commit update ring state */
	if (likely(commit != TX_COMMIT)) {
		tx_ring->next_to_clean = next_to_clean;
		ena_com_comp_ack(&adapter->ena_dev->io_sq_queues[ena_qid],
		    total_done);
		ena_com_update_dev_comp_head(io_cq);
	}

	/*
	 * Need to make the rings circular update visible to
	 * ena_xmit_mbuf() before checking for tx_ring->running.
	 */
	mb();

	above_thresh = ena_com_sq_have_enough_space(tx_ring->ena_com_io_sq,
	    ENA_TX_RESUME_THRESH);
	if (unlikely(!tx_ring->running && above_thresh)) {
		ENA_RING_MTX_LOCK(tx_ring);
		above_thresh =
		    ena_com_sq_have_enough_space(tx_ring->ena_com_io_sq,
		    ENA_TX_RESUME_THRESH);
		if (!tx_ring->running && above_thresh) {
			tx_ring->running = true;
			counter_u64_add(tx_ring->tx_stats.queue_wakeup, 1);
			taskqueue_enqueue(tx_ring->enqueue_tq,
			    &tx_ring->enqueue_task);
		}
		ENA_RING_MTX_UNLOCK(tx_ring);
	}

	return (work_done);
}

static void
ena_rx_hash_mbuf(struct ena_ring *rx_ring, struct ena_com_rx_ctx *ena_rx_ctx,
    struct mbuf *mbuf)
{
	struct ena_adapter *adapter = rx_ring->adapter;

	if (likely(ENA_FLAG_ISSET(ENA_FLAG_RSS_ACTIVE, adapter))) {
		mbuf->m_pkthdr.flowid = ena_rx_ctx->hash;

		if (ena_rx_ctx->frag &&
		    (ena_rx_ctx->l3_proto != ENA_ETH_IO_L3_PROTO_UNKNOWN)) {
			M_HASHTYPE_SET(mbuf, M_HASHTYPE_OPAQUE_HASH);
			return;
		}

		switch (ena_rx_ctx->l3_proto) {
		case ENA_ETH_IO_L3_PROTO_IPV4:
			switch (ena_rx_ctx->l4_proto) {
			case ENA_ETH_IO_L4_PROTO_TCP:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_TCP_IPV4);
				break;
			case ENA_ETH_IO_L4_PROTO_UDP:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_UDP_IPV4);
				break;
			default:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_IPV4);
			}
			break;
		case ENA_ETH_IO_L3_PROTO_IPV6:
			switch (ena_rx_ctx->l4_proto) {
			case ENA_ETH_IO_L4_PROTO_TCP:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_TCP_IPV6);
				break;
			case ENA_ETH_IO_L4_PROTO_UDP:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_UDP_IPV6);
				break;
			default:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_IPV6);
			}
			break;
		case ENA_ETH_IO_L3_PROTO_UNKNOWN:
			M_HASHTYPE_SET(mbuf, M_HASHTYPE_NONE);
			break;
		default:
			M_HASHTYPE_SET(mbuf, M_HASHTYPE_OPAQUE_HASH);
		}
	} else {
		mbuf->m_pkthdr.flowid = rx_ring->qid;
		M_HASHTYPE_SET(mbuf, M_HASHTYPE_NONE);
	}
}

/**
 * ena_rx_mbuf - assemble mbuf from descriptors
 * @rx_ring: ring for which we want to clean packets
 * @ena_bufs: buffer info
 * @ena_rx_ctx: metadata for this packet(s)
 * @next_to_clean: ring pointer, will be updated only upon success
 *
 **/
static struct mbuf*
ena_rx_mbuf(struct ena_ring *rx_ring, struct ena_com_rx_buf_info *ena_bufs,
    struct ena_com_rx_ctx *ena_rx_ctx, uint16_t *next_to_clean)
{
	struct mbuf *mbuf;
	struct ena_rx_buffer *rx_info;
	struct ena_adapter *adapter;
	unsigned int descs = ena_rx_ctx->descs;
	int rc;
	uint16_t ntc, len, req_id, buf = 0;

	ntc = *next_to_clean;
	adapter = rx_ring->adapter;

	len = ena_bufs[buf].len;
	req_id = ena_bufs[buf].req_id;
	rc = validate_rx_req_id(rx_ring, req_id);
	if (unlikely(rc != 0))
		return (NULL);

	rx_info = &rx_ring->rx_buffer_info[req_id];
	if (unlikely(rx_info->mbuf == NULL)) {
		device_printf(adapter->pdev, "NULL mbuf in rx_info");
		return (NULL);
	}

	ena_trace(ENA_DBG | ENA_RXPTH, "rx_info %p, mbuf %p, paddr %jx\n",
	    rx_info, rx_info->mbuf, (uintmax_t)rx_info->ena_buf.paddr);

	bus_dmamap_sync(adapter->rx_buf_tag, rx_info->map,
	    BUS_DMASYNC_POSTREAD);
	mbuf = rx_info->mbuf;
	mbuf->m_flags |= M_PKTHDR;
	mbuf->m_pkthdr.len = len;
	mbuf->m_len = len;
	mbuf->m_pkthdr.rcvif = rx_ring->que->adapter->ifp;

	/* Fill mbuf with hash key and it's interpretation for optimization */
	ena_rx_hash_mbuf(rx_ring, ena_rx_ctx, mbuf);

	ena_trace(ENA_DBG | ENA_RXPTH, "rx mbuf 0x%p, flags=0x%x, len: %d\n",
	    mbuf, mbuf->m_flags, mbuf->m_pkthdr.len);

	/* DMA address is not needed anymore, unmap it */
	bus_dmamap_unload(rx_ring->adapter->rx_buf_tag, rx_info->map);

	rx_info->mbuf = NULL;
	rx_ring->free_rx_ids[ntc] = req_id;
	ntc = ENA_RX_RING_IDX_NEXT(ntc, rx_ring->ring_size);

	/*
	 * While we have more than 1 descriptors for one rcvd packet, append
	 * other mbufs to the main one
	 */
	while (--descs) {
		++buf;
		len = ena_bufs[buf].len;
		req_id = ena_bufs[buf].req_id;
		rc = validate_rx_req_id(rx_ring, req_id);
		if (unlikely(rc != 0)) {
			/*
			 * If the req_id is invalid, then the device will be
			 * reset. In that case we must free all mbufs that
			 * were already gathered.
			 */
			m_freem(mbuf);
			return (NULL);
		}
		rx_info = &rx_ring->rx_buffer_info[req_id];

		if (unlikely(rx_info->mbuf == NULL)) {
			device_printf(adapter->pdev, "NULL mbuf in rx_info");
			/*
			 * If one of the required mbufs was not allocated yet,
			 * we can break there.
			 * All earlier used descriptors will be reallocated
			 * later and not used mbufs can be reused.
			 * The next_to_clean pointer will not be updated in case
			 * of an error, so caller should advance it manually
			 * in error handling routine to keep it up to date
			 * with hw ring.
			 */
			m_freem(mbuf);
			return (NULL);
		}

		bus_dmamap_sync(adapter->rx_buf_tag, rx_info->map,
		    BUS_DMASYNC_POSTREAD);
		if (unlikely(m_append(mbuf, len, rx_info->mbuf->m_data) == 0)) {
			counter_u64_add(rx_ring->rx_stats.mbuf_alloc_fail, 1);
			ena_trace(ENA_WARNING, "Failed to append Rx mbuf %p\n",
			    mbuf);
		}

		ena_trace(ENA_DBG | ENA_RXPTH,
		    "rx mbuf updated. len %d\n", mbuf->m_pkthdr.len);

		/* Free already appended mbuf, it won't be useful anymore */
		bus_dmamap_unload(rx_ring->adapter->rx_buf_tag, rx_info->map);
		m_freem(rx_info->mbuf);
		rx_info->mbuf = NULL;

		rx_ring->free_rx_ids[ntc] = req_id;
		ntc = ENA_RX_RING_IDX_NEXT(ntc, rx_ring->ring_size);
	}

	*next_to_clean = ntc;

	return (mbuf);
}

/**
 * ena_rx_checksum - indicate in mbuf if hw indicated a good cksum
 **/
static inline void
ena_rx_checksum(struct ena_ring *rx_ring, struct ena_com_rx_ctx *ena_rx_ctx,
    struct mbuf *mbuf)
{

	/* if IP and error */
	if (unlikely((ena_rx_ctx->l3_proto == ENA_ETH_IO_L3_PROTO_IPV4) &&
	    ena_rx_ctx->l3_csum_err)) {
		/* ipv4 checksum error */
		mbuf->m_pkthdr.csum_flags = 0;
		counter_u64_add(rx_ring->rx_stats.bad_csum, 1);
		ena_trace(ENA_DBG, "RX IPv4 header checksum error\n");
		return;
	}

	/* if TCP/UDP */
	if ((ena_rx_ctx->l4_proto == ENA_ETH_IO_L4_PROTO_TCP) ||
	    (ena_rx_ctx->l4_proto == ENA_ETH_IO_L4_PROTO_UDP)) {
		if (ena_rx_ctx->l4_csum_err) {
			/* TCP/UDP checksum error */
			mbuf->m_pkthdr.csum_flags = 0;
			counter_u64_add(rx_ring->rx_stats.bad_csum, 1);
			ena_trace(ENA_DBG, "RX L4 checksum error\n");
		} else {
			mbuf->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
			mbuf->m_pkthdr.csum_flags |= CSUM_IP_VALID;
		}
	}
}

/**
 * ena_rx_cleanup - handle rx irq
 * @arg: ring for which irq is being handled
 **/
static int
ena_rx_cleanup(struct ena_ring *rx_ring)
{
	struct ena_adapter *adapter;
	struct mbuf *mbuf;
	struct ena_com_rx_ctx ena_rx_ctx;
	struct ena_com_io_cq* io_cq;
	struct ena_com_io_sq* io_sq;
	if_t ifp;
	uint16_t ena_qid;
	uint16_t next_to_clean;
	uint32_t refill_required;
	uint32_t refill_threshold;
	uint32_t do_if_input = 0;
	unsigned int qid;
	int rc, i;
	int budget = RX_BUDGET;

	adapter = rx_ring->que->adapter;
	ifp = adapter->ifp;
	qid = rx_ring->que->id;
	ena_qid = ENA_IO_RXQ_IDX(qid);
	io_cq = &adapter->ena_dev->io_cq_queues[ena_qid];
	io_sq = &adapter->ena_dev->io_sq_queues[ena_qid];
	next_to_clean = rx_ring->next_to_clean;

	ena_trace(ENA_DBG, "rx: qid %d\n", qid);

	do {
		ena_rx_ctx.ena_bufs = rx_ring->ena_bufs;
		ena_rx_ctx.max_bufs = adapter->max_rx_sgl_size;
		ena_rx_ctx.descs = 0;
		bus_dmamap_sync(io_cq->cdesc_addr.mem_handle.tag,
		    io_cq->cdesc_addr.mem_handle.map, BUS_DMASYNC_POSTREAD);
		rc = ena_com_rx_pkt(io_cq, io_sq, &ena_rx_ctx);

		if (unlikely(rc != 0))
			goto error;

		if (unlikely(ena_rx_ctx.descs == 0))
			break;

		ena_trace(ENA_DBG | ENA_RXPTH, "rx: q %d got packet from ena. "
		    "descs #: %d l3 proto %d l4 proto %d hash: %x\n",
		    rx_ring->qid, ena_rx_ctx.descs, ena_rx_ctx.l3_proto,
		    ena_rx_ctx.l4_proto, ena_rx_ctx.hash);

		/* Receive mbuf from the ring */
		mbuf = ena_rx_mbuf(rx_ring, rx_ring->ena_bufs,
		    &ena_rx_ctx, &next_to_clean);
		bus_dmamap_sync(io_cq->cdesc_addr.mem_handle.tag,
		    io_cq->cdesc_addr.mem_handle.map, BUS_DMASYNC_PREREAD);
		/* Exit if we failed to retrieve a buffer */
		if (unlikely(mbuf == NULL)) {
			for (i = 0; i < ena_rx_ctx.descs; ++i) {
				rx_ring->free_rx_ids[next_to_clean] =
				    rx_ring->ena_bufs[i].req_id;
				next_to_clean =
				    ENA_RX_RING_IDX_NEXT(next_to_clean,
				    rx_ring->ring_size);

			}
			break;
		}

		if (((ifp->if_capenable & IFCAP_RXCSUM) != 0) ||
		    ((ifp->if_capenable & IFCAP_RXCSUM_IPV6) != 0)) {
			ena_rx_checksum(rx_ring, &ena_rx_ctx, mbuf);
		}

		counter_enter();
		counter_u64_add_protected(rx_ring->rx_stats.bytes,
		    mbuf->m_pkthdr.len);
		counter_u64_add_protected(adapter->hw_stats.rx_bytes,
		    mbuf->m_pkthdr.len);
		counter_exit();
		/*
		 * LRO is only for IP/TCP packets and TCP checksum of the packet
		 * should be computed by hardware.
		 */
		do_if_input = 1;
		if (((ifp->if_capenable & IFCAP_LRO) != 0)  &&
		    ((mbuf->m_pkthdr.csum_flags & CSUM_IP_VALID) != 0) &&
		    (ena_rx_ctx.l4_proto == ENA_ETH_IO_L4_PROTO_TCP)) {
			/*
			 * Send to the stack if:
			 *  - LRO not enabled, or
			 *  - no LRO resources, or
			 *  - lro enqueue fails
			 */
			if ((rx_ring->lro.lro_cnt != 0) &&
			    (tcp_lro_rx(&rx_ring->lro, mbuf, 0) == 0))
					do_if_input = 0;
		}
		if (do_if_input != 0) {
			ena_trace(ENA_DBG | ENA_RXPTH,
			    "calling if_input() with mbuf %p\n", mbuf);
			(*ifp->if_input)(ifp, mbuf);
		}

		counter_enter();
		counter_u64_add_protected(rx_ring->rx_stats.cnt, 1);
		counter_u64_add_protected(adapter->hw_stats.rx_packets, 1);
		counter_exit();
	} while (--budget);

	rx_ring->next_to_clean = next_to_clean;

	refill_required = ena_com_free_desc(io_sq);
	refill_threshold = min_t(int,
	    rx_ring->ring_size / ENA_RX_REFILL_THRESH_DIVIDER,
	    ENA_RX_REFILL_THRESH_PACKET);

	if (refill_required > refill_threshold) {
		ena_com_update_dev_comp_head(rx_ring->ena_com_io_cq);
		ena_refill_rx_bufs(rx_ring, refill_required);
	}

	tcp_lro_flush_all(&rx_ring->lro);

	return (RX_BUDGET - budget);

error:
	counter_u64_add(rx_ring->rx_stats.bad_desc_num, 1);

	/* Too many desc from the device. Trigger reset */
	if (likely(!ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, adapter))) {
		adapter->reset_reason = ENA_REGS_RESET_TOO_MANY_RX_DESCS;
		ENA_FLAG_SET_ATOMIC(ENA_FLAG_TRIGGER_RESET, adapter);
	}

	return (0);
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

static void
ena_cleanup(void *arg, int pending)
{
	struct ena_que	*que = arg;
	struct ena_adapter *adapter = que->adapter;
	if_t ifp = adapter->ifp;
	struct ena_ring *tx_ring;
	struct ena_ring *rx_ring;
	struct ena_com_io_cq* io_cq;
	struct ena_eth_io_intr_reg intr_reg;
	int qid, ena_qid;
	int txc, rxc, i;

	if (unlikely((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0))
		return;

	ena_trace(ENA_DBG, "MSI-X TX/RX routine\n");

	tx_ring = que->tx_ring;
	rx_ring = que->rx_ring;
	qid = que->id;
	ena_qid = ENA_IO_TXQ_IDX(qid);
	io_cq = &adapter->ena_dev->io_cq_queues[ena_qid];

	tx_ring->first_interrupt = true;
	rx_ring->first_interrupt = true;

	for (i = 0; i < CLEAN_BUDGET; ++i) {
		rxc = ena_rx_cleanup(rx_ring);
		txc = ena_tx_cleanup(tx_ring);

		if (unlikely((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0))
			return;

		if ((txc != TX_BUDGET) && (rxc != RX_BUDGET))
		       break;
	}

	/* Signal that work is done and unmask interrupt */
	ena_com_update_intr_reg(&intr_reg,
	    RX_IRQ_INTERVAL,
	    TX_IRQ_INTERVAL,
	    true);
	ena_com_unmask_intr(io_cq, &intr_reg);
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
		device_printf(dev, "Error, MSI-X is already enabled\n");
		return (EINVAL);
	}

	/* Reserved the max msix vectors we might need */
	msix_vecs = ENA_MAX_MSIX_VEC(adapter->num_queues);

	adapter->msix_entries = malloc(msix_vecs * sizeof(struct msix_entry),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	ena_trace(ENA_DBG, "trying to enable MSI-X, vectors: %d\n", msix_vecs);

	for (i = 0; i < msix_vecs; i++) {
		adapter->msix_entries[i].entry = i;
		/* Vectors must start from 1 */
		adapter->msix_entries[i].vector = i + 1;
	}

	msix_req = msix_vecs;
	rc = pci_alloc_msix(dev, &msix_vecs);
	if (unlikely(rc != 0)) {
		device_printf(dev,
		    "Failed to enable MSIX, vectors %d rc %d\n", msix_vecs, rc);

		rc = ENOSPC;
		goto err_msix_free;
	}

	if (msix_vecs != msix_req) {
		if (msix_vecs == ENA_ADMIN_MSIX_VEC) {
			device_printf(dev,
			    "Not enough number of MSI-x allocated: %d\n",
			    msix_vecs);
			pci_release_msi(dev);
			rc = ENOSPC;
			goto err_msix_free;
		}
		device_printf(dev, "Enable only %d MSI-x (out of %d), reduce "
		    "the number of queues\n", msix_vecs, msix_req);
		adapter->num_queues = msix_vecs - ENA_ADMIN_MSIX_VEC;
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

	snprintf(adapter->irq_tbl[ENA_MGMNT_IRQ_IDX].name,
	    ENA_IRQNAME_SIZE, "ena-mgmnt@pci:%s",
	    device_get_nameunit(adapter->pdev));
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
	static int last_bind_cpu = -1;
	int irq_idx;

	if (adapter->msix_entries == NULL)
		return (EINVAL);

	for (int i = 0; i < adapter->num_queues; i++) {
		irq_idx = ENA_IO_IRQ_IDX(i);

		snprintf(adapter->irq_tbl[irq_idx].name, ENA_IRQNAME_SIZE,
		    "%s-TxRx-%d", device_get_nameunit(adapter->pdev), i);
		adapter->irq_tbl[irq_idx].handler = ena_handle_msix;
		adapter->irq_tbl[irq_idx].data = &adapter->que[i];
		adapter->irq_tbl[irq_idx].vector =
		    adapter->msix_entries[irq_idx].vector;
		ena_trace(ENA_INFO | ENA_IOQ, "ena_setup_io_intr vector: %d\n",
		    adapter->msix_entries[irq_idx].vector);

		/*
		 * We want to bind rings to the corresponding cpu
		 * using something similar to the RSS round-robin technique.
		 */
		if (unlikely(last_bind_cpu < 0))
			last_bind_cpu = CPU_FIRST();
		adapter->que[i].cpu = adapter->irq_tbl[irq_idx].cpu =
		    last_bind_cpu;
		last_bind_cpu = CPU_NEXT(last_bind_cpu);
	}

	return (0);
}

static int
ena_request_mgmnt_irq(struct ena_adapter *adapter)
{
	struct ena_irq *irq;
	unsigned long flags;
	int rc, rcc;

	flags = RF_ACTIVE | RF_SHAREABLE;

	irq = &adapter->irq_tbl[ENA_MGMNT_IRQ_IDX];
	irq->res = bus_alloc_resource_any(adapter->pdev, SYS_RES_IRQ,
	    &irq->vector, flags);

	if (unlikely(irq->res == NULL)) {
		device_printf(adapter->pdev, "could not allocate "
		    "irq vector: %d\n", irq->vector);
		return (ENXIO);
	}

	rc = bus_setup_intr(adapter->pdev, irq->res,
	    INTR_TYPE_NET | INTR_MPSAFE, NULL, ena_intr_msix_mgmnt,
	    irq->data, &irq->cookie);
	if (unlikely(rc != 0)) {
		device_printf(adapter->pdev, "failed to register "
		    "interrupt handler for irq %ju: %d\n",
		    rman_get_start(irq->res), rc);
		goto err_res_free;
	}
	irq->requested = true;

	return (rc);

err_res_free:
	ena_trace(ENA_INFO | ENA_ADMQ, "releasing resource for irq %d\n",
	    irq->vector);
	rcc = bus_release_resource(adapter->pdev, SYS_RES_IRQ,
	    irq->vector, irq->res);
	if (unlikely(rcc != 0))
		device_printf(adapter->pdev, "dev has no parent while "
		    "releasing res for irq: %d\n", irq->vector);
	irq->res = NULL;

	return (rc);
}

static int
ena_request_io_irq(struct ena_adapter *adapter)
{
	struct ena_irq *irq;
	unsigned long flags = 0;
	int rc = 0, i, rcc;

	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_MSIX_ENABLED, adapter))) {
		device_printf(adapter->pdev,
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
			device_printf(adapter->pdev, "could not allocate "
			    "irq vector: %d\n", irq->vector);
			goto err;
		}

		rc = bus_setup_intr(adapter->pdev, irq->res,
		    INTR_TYPE_NET | INTR_MPSAFE, irq->handler, NULL,
		    irq->data, &irq->cookie);
		 if (unlikely(rc != 0)) {
			device_printf(adapter->pdev, "failed to register "
			    "interrupt handler for irq %ju: %d\n",
			    rman_get_start(irq->res), rc);
			goto err;
		}
		irq->requested = true;

		ena_trace(ENA_INFO, "queue %d - cpu %d\n",
		    i - ENA_IO_IRQ_FIRST_IDX, irq->cpu);
	}

	return (rc);

err:

	for (; i >= ENA_IO_IRQ_FIRST_IDX; i--) {
		irq = &adapter->irq_tbl[i];
		rcc = 0;

		/* Once we entered err: section and irq->requested is true we
		   free both intr and resources */
		if (irq->requested)
			rcc = bus_teardown_intr(adapter->pdev, irq->res, irq->cookie);
		if (unlikely(rcc != 0))
			device_printf(adapter->pdev, "could not release"
			    " irq: %d, error: %d\n", irq->vector, rcc);

		/* If we entred err: section without irq->requested set we know
		   it was bus_alloc_resource_any() that needs cleanup, provided
		   res is not NULL. In case res is NULL no work in needed in
		   this iteration */
		rcc = 0;
		if (irq->res != NULL) {
			rcc = bus_release_resource(adapter->pdev, SYS_RES_IRQ,
			    irq->vector, irq->res);
		}
		if (unlikely(rcc != 0))
			device_printf(adapter->pdev, "dev has no parent while "
			    "releasing res for irq: %d\n", irq->vector);
		irq->requested = false;
		irq->res = NULL;
	}

	return (rc);
}

static void
ena_free_mgmnt_irq(struct ena_adapter *adapter)
{
	struct ena_irq *irq;
	int rc;

	irq = &adapter->irq_tbl[ENA_MGMNT_IRQ_IDX];
	if (irq->requested) {
		ena_trace(ENA_INFO | ENA_ADMQ, "tear down irq: %d\n",
		    irq->vector);
		rc = bus_teardown_intr(adapter->pdev, irq->res, irq->cookie);
		if (unlikely(rc != 0))
			device_printf(adapter->pdev, "failed to tear "
			    "down irq: %d\n", irq->vector);
		irq->requested = 0;
	}

	if (irq->res != NULL) {
		ena_trace(ENA_INFO | ENA_ADMQ, "release resource irq: %d\n",
		    irq->vector);
		rc = bus_release_resource(adapter->pdev, SYS_RES_IRQ,
		    irq->vector, irq->res);
		irq->res = NULL;
		if (unlikely(rc != 0))
			device_printf(adapter->pdev, "dev has no parent while "
			    "releasing res for irq: %d\n", irq->vector);
	}
}

static void
ena_free_io_irq(struct ena_adapter *adapter)
{
	struct ena_irq *irq;
	int rc;

	for (int i = ENA_IO_IRQ_FIRST_IDX; i < adapter->msix_vecs; i++) {
		irq = &adapter->irq_tbl[i];
		if (irq->requested) {
			ena_trace(ENA_INFO | ENA_IOQ, "tear down irq: %d\n",
			    irq->vector);
			rc = bus_teardown_intr(adapter->pdev, irq->res,
			    irq->cookie);
			if (unlikely(rc != 0)) {
				device_printf(adapter->pdev, "failed to tear "
				    "down irq: %d\n", irq->vector);
			}
			irq->requested = 0;
		}

		if (irq->res != NULL) {
			ena_trace(ENA_INFO | ENA_IOQ, "release resource irq: %d\n",
			    irq->vector);
			rc = bus_release_resource(adapter->pdev, SYS_RES_IRQ,
			    irq->vector, irq->res);
			irq->res = NULL;
			if (unlikely(rc != 0)) {
				device_printf(adapter->pdev, "dev has no parent"
				    " while releasing res for irq: %d\n",
				    irq->vector);
			}
		}
	}
}

static void
ena_free_irqs(struct ena_adapter* adapter)
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
	if (adapter->msix_entries != NULL)
		free(adapter->msix_entries, M_DEVBUF);
	adapter->msix_entries = NULL;
}

static void
ena_unmask_all_io_irqs(struct ena_adapter *adapter)
{
	struct ena_com_io_cq* io_cq;
	struct ena_eth_io_intr_reg intr_reg;
	uint16_t ena_qid;
	int i;

	/* Unmask interrupts for all queues */
	for (i = 0; i < adapter->num_queues; i++) {
		ena_qid = ENA_IO_TXQ_IDX(i);
		io_cq = &adapter->ena_dev->io_cq_queues[ena_qid];
		ena_com_update_intr_reg(&intr_reg, 0, 0, true);
		ena_com_unmask_intr(io_cq, &intr_reg);
	}
}

/* Configure the Rx forwarding */
static int
ena_rss_configure(struct ena_adapter *adapter)
{
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	int rc;

	/* Set indirect table */
	rc = ena_com_indirect_table_set(ena_dev);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP)))
		return (rc);

	/* Configure hash function (if supported) */
	rc = ena_com_set_hash_function(ena_dev);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP)))
		return (rc);

	/* Configure hash inputs (if supported) */
	rc = ena_com_set_hash_ctrl(ena_dev);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP)))
		return (rc);

	return (0);
}

static int
ena_up_complete(struct ena_adapter *adapter)
{
	int rc;

	if (likely(ENA_FLAG_ISSET(ENA_FLAG_RSS_ACTIVE, adapter))) {
		rc = ena_rss_configure(adapter);
		if (rc != 0)
			return (rc);
	}

	rc = ena_change_mtu(adapter->ifp, adapter->ifp->if_mtu);
	if (unlikely(rc != 0))
		return (rc);

	ena_refill_all_rx_bufs(adapter);
	ena_reset_counters((counter_u64_t *)&adapter->hw_stats,
	    sizeof(adapter->hw_stats));

	return (0);
}

static int
ena_up(struct ena_adapter *adapter)
{
	int rc = 0;

	if (unlikely(device_is_attached(adapter->pdev) == 0)) {
		device_printf(adapter->pdev, "device is not attached!\n");
		return (ENXIO);
	}

	if (!ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter)) {
		device_printf(adapter->pdev, "device is going UP\n");

		/* setup interrupts for IO queues */
		rc = ena_setup_io_intr(adapter);
		if (unlikely(rc != 0)) {
			ena_trace(ENA_ALERT, "error setting up IO interrupt\n");
			goto error;
		}
		rc = ena_request_io_irq(adapter);
		if (unlikely(rc != 0)) {
			ena_trace(ENA_ALERT, "err_req_irq\n");
			goto error;
		}

		/* allocate transmit descriptors */
		rc = ena_setup_all_tx_resources(adapter);
		if (unlikely(rc != 0)) {
			ena_trace(ENA_ALERT, "err_setup_tx\n");
			goto err_setup_tx;
		}

		/* allocate receive descriptors */
		rc = ena_setup_all_rx_resources(adapter);
		if (unlikely(rc != 0)) {
			ena_trace(ENA_ALERT, "err_setup_rx\n");
			goto err_setup_rx;
		}

		/* create IO queues for Rx & Tx */
		rc = ena_create_io_queues(adapter);
		if (unlikely(rc != 0)) {
			ena_trace(ENA_ALERT,
			    "create IO queues failed\n");
			goto err_io_que;
		}

		if (ENA_FLAG_ISSET(ENA_FLAG_LINK_UP, adapter))
			if_link_state_change(adapter->ifp, LINK_STATE_UP);

		rc = ena_up_complete(adapter);
		if (unlikely(rc != 0))
			goto err_up_complete;

		counter_u64_add(adapter->dev_stats.interface_up, 1);

		ena_update_hwassist(adapter);

		if_setdrvflagbits(adapter->ifp, IFF_DRV_RUNNING,
		    IFF_DRV_OACTIVE);

		callout_reset_sbt(&adapter->timer_service, SBT_1S, SBT_1S,
		    ena_timer_service, (void *)adapter, 0);

		ENA_FLAG_SET_ATOMIC(ENA_FLAG_DEV_UP, adapter);

		ena_unmask_all_io_irqs(adapter);
	}

	return (0);

err_up_complete:
	ena_destroy_all_io_queues(adapter);
err_io_que:
	ena_free_all_rx_resources(adapter);
err_setup_rx:
	ena_free_all_tx_resources(adapter);
err_setup_tx:
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
	ena_trace(ENA_DBG, "enter\n");

	mtx_lock(&adapter->global_mtx);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!ENA_FLAG_ISSET(ENA_FLAG_LINK_UP, adapter)) {
		mtx_unlock(&adapter->global_mtx);
		ena_trace(ENA_INFO, "Link is down\n");
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active |= IFM_UNKNOWN | IFM_FDX;

	mtx_unlock(&adapter->global_mtx);
}

static void
ena_init(void *arg)
{
	struct ena_adapter *adapter = (struct ena_adapter *)arg;

	if (!ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter)) {
		sx_xlock(&adapter->ioctl_sx);
		ena_up(adapter);
		sx_unlock(&adapter->ioctl_sx);
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
		sx_xlock(&adapter->ioctl_sx);
		ena_down(adapter);

		ena_change_mtu(ifp, ifr->ifr_mtu);

		rc = ena_up(adapter);
		sx_unlock(&adapter->ioctl_sx);
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) != 0) {
			if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0) {
				if ((ifp->if_flags & (IFF_PROMISC |
				    IFF_ALLMULTI)) != 0) {
					device_printf(adapter->pdev,
					    "ioctl promisc/allmulti\n");
				}
			} else {
				sx_xlock(&adapter->ioctl_sx);
				rc = ena_up(adapter);
				sx_unlock(&adapter->ioctl_sx);
			}
		} else {
			if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0) {
				sx_xlock(&adapter->ioctl_sx);
				ena_down(adapter);
				sx_unlock(&adapter->ioctl_sx);
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
				sx_xlock(&adapter->ioctl_sx);
				ena_down(adapter);
				rc = ena_up(adapter);
				sx_unlock(&adapter->ioctl_sx);
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

	if ((feat->offload.tx &
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV4_MASK) != 0)
		caps |= IFCAP_TSO4;

	if ((feat->offload.tx &
	    ENA_ADMIN_FEATURE_OFFLOAD_DESC_TSO_IPV6_MASK) != 0)
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

	host_info->supported_network_features[0] =
	    (uint32_t)if_getcapabilities(ifp);
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
		ena_trace(ENA_ALERT, "can not allocate ifnet structure\n");
		return (ENXIO);
	}
	if_initname(ifp, device_get_name(pdev), device_get_unit(pdev));
	if_setdev(ifp, pdev);
	if_setsoftc(ifp, adapter);

	if_setflags(ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
	if_setinitfn(ifp, ena_init);
	if_settransmitfn(ifp, ena_mq_start);
	if_setqflushfn(ifp, ena_qflush);
	if_setioctlfn(ifp, ena_ioctl);
	if_setgetcounterfn(ifp, ena_get_counter);

	if_setsendqlen(ifp, adapter->tx_ring_size);
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
	ifmedia_init(&adapter->media, IFM_IMASK,
	    ena_media_change, ena_media_status);
	ifmedia_add(&adapter->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&adapter->media, IFM_ETHER | IFM_AUTO);

	ether_ifattach(ifp, adapter->mac_addr);

	return (0);
}

static void
ena_down(struct ena_adapter *adapter)
{
	int rc;

	if (ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter)) {
		device_printf(adapter->pdev, "device is going DOWN\n");

		callout_drain(&adapter->timer_service);

		ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_DEV_UP, adapter);
		if_setdrvflagbits(adapter->ifp, IFF_DRV_OACTIVE,
		    IFF_DRV_RUNNING);

		ena_free_io_irq(adapter);

		if (ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, adapter)) {
			rc = ena_com_dev_reset(adapter->ena_dev,
			    adapter->reset_reason);
			if (unlikely(rc != 0))
				device_printf(adapter->pdev,
				    "Device reset failed\n");
		}

		ena_destroy_all_io_queues(adapter);

		ena_free_all_tx_bufs(adapter);
		ena_free_all_rx_bufs(adapter);
		ena_free_all_tx_resources(adapter);
		ena_free_all_rx_resources(adapter);

		counter_u64_add(adapter->dev_stats.interface_down, 1);
	}
}

static void
ena_tx_csum(struct ena_com_tx_ctx *ena_tx_ctx, struct mbuf *mbuf)
{
	struct ena_com_tx_meta *ena_meta;
	struct ether_vlan_header *eh;
	struct mbuf *mbuf_next;
	u32 mss;
	bool offload;
	uint16_t etype;
	int ehdrlen;
	struct ip *ip;
	int iphlen;
	struct tcphdr *th;
	int offset;

	offload = false;
	ena_meta = &ena_tx_ctx->ena_meta;
	mss = mbuf->m_pkthdr.tso_segsz;

	if (mss != 0)
		offload = true;

	if ((mbuf->m_pkthdr.csum_flags & CSUM_TSO) != 0)
		offload = true;

	if ((mbuf->m_pkthdr.csum_flags & CSUM_OFFLOAD) != 0)
		offload = true;

	if (!offload) {
		ena_tx_ctx->meta_valid = 0;
		return;
	}

	/* Determine where frame payload starts. */
	eh = mtod(mbuf, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		etype = ntohs(eh->evl_proto);
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		etype = ntohs(eh->evl_encap_proto);
		ehdrlen = ETHER_HDR_LEN;
	}

	mbuf_next = m_getptr(mbuf, ehdrlen, &offset);
	ip = (struct ip *)(mtodo(mbuf_next, offset));
	iphlen = ip->ip_hl << 2;

	mbuf_next = m_getptr(mbuf, iphlen + ehdrlen, &offset);
	th = (struct tcphdr *)(mtodo(mbuf_next, offset));

	if ((mbuf->m_pkthdr.csum_flags & CSUM_IP) != 0) {
		ena_tx_ctx->l3_csum_enable = 1;
	}
	if ((mbuf->m_pkthdr.csum_flags & CSUM_TSO) != 0) {
		ena_tx_ctx->tso_enable = 1;
		ena_meta->l4_hdr_len = (th->th_off);
	}

	switch (etype) {
	case ETHERTYPE_IP:
		ena_tx_ctx->l3_proto = ENA_ETH_IO_L3_PROTO_IPV4;
		if ((ip->ip_off & htons(IP_DF)) != 0)
			ena_tx_ctx->df = 1;
		break;
	case ETHERTYPE_IPV6:
		ena_tx_ctx->l3_proto = ENA_ETH_IO_L3_PROTO_IPV6;

	default:
		break;
	}

	if (ip->ip_p == IPPROTO_TCP) {
		ena_tx_ctx->l4_proto = ENA_ETH_IO_L4_PROTO_TCP;
		if ((mbuf->m_pkthdr.csum_flags &
		    (CSUM_IP_TCP | CSUM_IP6_TCP)) != 0)
			ena_tx_ctx->l4_csum_enable = 1;
		else
			ena_tx_ctx->l4_csum_enable = 0;
	} else if (ip->ip_p == IPPROTO_UDP) {
		ena_tx_ctx->l4_proto = ENA_ETH_IO_L4_PROTO_UDP;
		if ((mbuf->m_pkthdr.csum_flags &
		    (CSUM_IP_UDP | CSUM_IP6_UDP)) != 0)
			ena_tx_ctx->l4_csum_enable = 1;
		else
			ena_tx_ctx->l4_csum_enable = 0;
	} else {
		ena_tx_ctx->l4_proto = ENA_ETH_IO_L4_PROTO_UNKNOWN;
		ena_tx_ctx->l4_csum_enable = 0;
	}

	ena_meta->mss = mss;
	ena_meta->l3_hdr_len = iphlen;
	ena_meta->l3_hdr_offset = ehdrlen;
	ena_tx_ctx->meta_valid = 1;
}

static int
ena_check_and_collapse_mbuf(struct ena_ring *tx_ring, struct mbuf **mbuf)
{
	struct ena_adapter *adapter;
	struct mbuf *collapsed_mbuf;
	int num_frags;

	adapter = tx_ring->adapter;
	num_frags = ena_mbuf_count(*mbuf);

	/* One segment must be reserved for configuration descriptor. */
	if (num_frags < adapter->max_tx_sgl_size)
		return (0);
	counter_u64_add(tx_ring->tx_stats.collapse, 1);

	collapsed_mbuf = m_collapse(*mbuf, M_NOWAIT,
	    adapter->max_tx_sgl_size - 1);
	if (unlikely(collapsed_mbuf == NULL)) {
		counter_u64_add(tx_ring->tx_stats.collapse_err, 1);
		return (ENOMEM);
	}

	/* If mbuf was collapsed succesfully, original mbuf is released. */
	*mbuf = collapsed_mbuf;

	return (0);
}

static void
ena_dmamap_llq(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct ena_com_buf *ena_buf = arg;

	if (unlikely(error != 0)) {
		ena_buf->paddr = 0;
		return;
	}

	KASSERT(nseg == 1, ("Invalid num of segments for LLQ dma"));

	ena_buf->paddr = segs->ds_addr;
	ena_buf->len = segs->ds_len;
}

static int
ena_tx_map_mbuf(struct ena_ring *tx_ring, struct ena_tx_buffer *tx_info,
    struct mbuf *mbuf, void **push_hdr, u16 *header_len)
{
	struct ena_adapter *adapter = tx_ring->adapter;
	struct ena_com_buf *ena_buf;
	bus_dma_segment_t segs[ENA_BUS_DMA_SEGS];
	uint32_t mbuf_head_len, frag_len;
	uint16_t push_len = 0;
	uint16_t delta = 0;
	int i, rc, nsegs;

	mbuf_head_len = mbuf->m_len;
	tx_info->mbuf = mbuf;
	ena_buf = tx_info->bufs;

	if (tx_ring->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_DEV) {
		/*
		 * When the device is LLQ mode, the driver will copy
		 * the header into the device memory space.
		 * the ena_com layer assumes the header is in a linear
		 * memory space.
		 * This assumption might be wrong since part of the header
		 * can be in the fragmented buffers.
		 * First check if header fits in the mbuf. If not, copy it to
		 * separate buffer that will be holding linearized data.
		 */
		push_len = min_t(uint32_t, mbuf->m_pkthdr.len,
		    tx_ring->tx_max_header_size);
		*header_len = push_len;
		/* If header is in linear space, just point into mbuf's data. */
		if (likely(push_len <= mbuf_head_len)) {
			*push_hdr = mbuf->m_data;
		/*
		 * Otherwise, copy whole portion of header from multiple mbufs
		 * to intermediate buffer.
		 */
		} else {
			m_copydata(mbuf, 0, push_len,
			    tx_ring->push_buf_intermediate_buf);
			*push_hdr = tx_ring->push_buf_intermediate_buf;

			counter_u64_add(tx_ring->tx_stats.llq_buffer_copy, 1);
			delta = push_len - mbuf_head_len;
		}

		ena_trace(ENA_DBG | ENA_TXPTH,
		    "mbuf: %p header_buf->vaddr: %p push_len: %d\n",
		    mbuf, *push_hdr, push_len);

		/*
		* If header was in linear memory space, map for the dma rest of the data
		* in the first mbuf of the mbuf chain.
		*/
		if (mbuf_head_len > push_len) {
			rc = bus_dmamap_load(adapter->tx_buf_tag,
			    tx_info->map_head,
			mbuf->m_data + push_len, mbuf_head_len - push_len,
			ena_dmamap_llq, ena_buf, BUS_DMA_NOWAIT);
			if (unlikely((rc != 0) || (ena_buf->paddr == 0)))
				goto single_dma_error;

			ena_buf++;
			tx_info->num_of_bufs++;

			tx_info->head_mapped = true;
		}
		mbuf = mbuf->m_next;
	} else {
		*push_hdr = NULL;
		/*
		* header_len is just a hint for the device. Because FreeBSD is not
		* giving us information about packet header length and it is not
		* guaranteed that all packet headers will be in the 1st mbuf, setting
		* header_len to 0 is making the device ignore this value and resolve
		* header on it's own.
		*/
		*header_len = 0;
	}

	/*
	 * If header is in non linear space (delta > 0), then skip mbufs
	 * containing header and map the last one containing both header and the
	 * packet data.
	 * The first segment is already counted in.
	 * If LLQ is not supported, the loop will be skipped.
	 */
	while (delta > 0) {
		frag_len = mbuf->m_len;

		/*
		 * If whole segment contains header just move to the
		 * next one and reduce delta.
		 */
		if (unlikely(delta >= frag_len)) {
			delta -= frag_len;
		} else {
			/*
			 * Map rest of the packet data that was contained in
			 * the mbuf.
			 */
			rc = bus_dmamap_load(adapter->tx_buf_tag,
			    tx_info->map_head, mbuf->m_data + delta,
			    frag_len - delta, ena_dmamap_llq, ena_buf,
			    BUS_DMA_NOWAIT);
			if (unlikely((rc != 0) || (ena_buf->paddr == 0)))
				goto single_dma_error;

			ena_buf++;
			tx_info->num_of_bufs++;
			tx_info->head_mapped = true;

			delta = 0;
		}

		mbuf = mbuf->m_next;
	}

	if (mbuf == NULL) {
		return (0);
	}

	/* Map rest of the mbufs */
	rc = bus_dmamap_load_mbuf_sg(adapter->tx_buf_tag, tx_info->map_seg, mbuf,
	    segs, &nsegs, BUS_DMA_NOWAIT);
	if (unlikely((rc != 0) || (nsegs == 0))) {
		ena_trace(ENA_WARNING,
		    "dmamap load failed! err: %d nsegs: %d\n", rc, nsegs);
		goto dma_error;
	}

	for (i = 0; i < nsegs; i++) {
		ena_buf->len = segs[i].ds_len;
		ena_buf->paddr = segs[i].ds_addr;
		ena_buf++;
	}
	tx_info->num_of_bufs += nsegs;
	tx_info->seg_mapped = true;

	return (0);

dma_error:
	if (tx_info->head_mapped == true)
		bus_dmamap_unload(adapter->tx_buf_tag, tx_info->map_head);
single_dma_error:
	counter_u64_add(tx_ring->tx_stats.dma_mapping_err, 1);
	tx_info->mbuf = NULL;
	return (rc);
}

static int
ena_xmit_mbuf(struct ena_ring *tx_ring, struct mbuf **mbuf)
{
	struct ena_adapter *adapter;
	struct ena_tx_buffer *tx_info;
	struct ena_com_tx_ctx ena_tx_ctx;
	struct ena_com_dev *ena_dev;
	struct ena_com_io_sq* io_sq;
	void *push_hdr;
	uint16_t next_to_use;
	uint16_t req_id;
	uint16_t ena_qid;
	uint16_t header_len;
	int rc;
	int nb_hw_desc;

	ena_qid = ENA_IO_TXQ_IDX(tx_ring->que->id);
	adapter = tx_ring->que->adapter;
	ena_dev = adapter->ena_dev;
	io_sq = &ena_dev->io_sq_queues[ena_qid];

	rc = ena_check_and_collapse_mbuf(tx_ring, mbuf);
	if (unlikely(rc != 0)) {
		ena_trace(ENA_WARNING,
		    "Failed to collapse mbuf! err: %d\n", rc);
		return (rc);
	}

	ena_trace(ENA_DBG | ENA_TXPTH, "Tx: %d bytes\n", (*mbuf)->m_pkthdr.len);

	next_to_use = tx_ring->next_to_use;
	req_id = tx_ring->free_tx_ids[next_to_use];
	tx_info = &tx_ring->tx_buffer_info[req_id];
	tx_info->num_of_bufs = 0;

	rc = ena_tx_map_mbuf(tx_ring, tx_info, *mbuf, &push_hdr, &header_len);
	if (unlikely(rc != 0)) {
		ena_trace(ENA_WARNING, "Failed to map TX mbuf\n");
		return (rc);
	}
	memset(&ena_tx_ctx, 0x0, sizeof(struct ena_com_tx_ctx));
	ena_tx_ctx.ena_bufs = tx_info->bufs;
	ena_tx_ctx.push_header = push_hdr;
	ena_tx_ctx.num_bufs = tx_info->num_of_bufs;
	ena_tx_ctx.req_id = req_id;
	ena_tx_ctx.header_len = header_len;

	/* Set flags and meta data */
	ena_tx_csum(&ena_tx_ctx, *mbuf);

	if (tx_ring->acum_pkts == DB_THRESHOLD ||
	    ena_com_is_doorbell_needed(tx_ring->ena_com_io_sq, &ena_tx_ctx)) {
		ena_trace(ENA_DBG | ENA_TXPTH,
		    "llq tx max burst size of queue %d achieved, writing doorbell to send burst\n",
		    tx_ring->que->id);
		wmb();
		ena_com_write_sq_doorbell(tx_ring->ena_com_io_sq);
		counter_u64_add(tx_ring->tx_stats.doorbells, 1);
		tx_ring->acum_pkts = 0;
	}

	/* Prepare the packet's descriptors and send them to device */
	rc = ena_com_prepare_tx(io_sq, &ena_tx_ctx, &nb_hw_desc);
	if (unlikely(rc != 0)) {
		if (likely(rc == ENA_COM_NO_MEM)) {
			ena_trace(ENA_DBG | ENA_TXPTH,
			    "tx ring[%d] if out of space\n", tx_ring->que->id);
		} else {
			device_printf(adapter->pdev,
			    "failed to prepare tx bufs\n");
		}
		counter_u64_add(tx_ring->tx_stats.prepare_ctx_err, 1);
		goto dma_error;
	}

	counter_enter();
	counter_u64_add_protected(tx_ring->tx_stats.cnt, 1);
	counter_u64_add_protected(tx_ring->tx_stats.bytes,
	    (*mbuf)->m_pkthdr.len);

	counter_u64_add_protected(adapter->hw_stats.tx_packets, 1);
	counter_u64_add_protected(adapter->hw_stats.tx_bytes,
	    (*mbuf)->m_pkthdr.len);
	counter_exit();

	tx_info->tx_descs = nb_hw_desc;
	getbinuptime(&tx_info->timestamp);
	tx_info->print_once = true;

	tx_ring->next_to_use = ENA_TX_RING_IDX_NEXT(next_to_use,
	    tx_ring->ring_size);

	/* stop the queue when no more space available, the packet can have up
	 * to sgl_size + 2. one for the meta descriptor and one for header
	 * (if the header is larger than tx_max_header_size).
	 */
	if (unlikely(!ena_com_sq_have_enough_space(tx_ring->ena_com_io_sq,
	    adapter->max_tx_sgl_size + 2))) {
		ena_trace(ENA_DBG | ENA_TXPTH, "Stop queue %d\n",
		    tx_ring->que->id);

		tx_ring->running = false;
		counter_u64_add(tx_ring->tx_stats.queue_stop, 1);

		/* There is a rare condition where this function decides to
		 * stop the queue but meanwhile tx_cleanup() updates
		 * next_to_completion and terminates.
		 * The queue will remain stopped forever.
		 * To solve this issue this function performs mb(), checks
		 * the wakeup condition and wakes up the queue if needed.
		 */
		mb();

		if (ena_com_sq_have_enough_space(tx_ring->ena_com_io_sq,
		    ENA_TX_RESUME_THRESH)) {
			tx_ring->running = true;
			counter_u64_add(tx_ring->tx_stats.queue_wakeup, 1);
		}
	}

	if (tx_info->head_mapped == true)
		bus_dmamap_sync(adapter->tx_buf_tag, tx_info->map_head,
		    BUS_DMASYNC_PREWRITE);
	if (tx_info->seg_mapped == true)
		bus_dmamap_sync(adapter->tx_buf_tag, tx_info->map_seg,
		    BUS_DMASYNC_PREWRITE);

	return (0);

dma_error:
	tx_info->mbuf = NULL;
	if (tx_info->seg_mapped == true) {
		bus_dmamap_unload(adapter->tx_buf_tag, tx_info->map_seg);
		tx_info->seg_mapped = false;
	}
	if (tx_info->head_mapped == true) {
		bus_dmamap_unload(adapter->tx_buf_tag, tx_info->map_head);
		tx_info->head_mapped = false;
	}

	return (rc);
}

static void
ena_start_xmit(struct ena_ring *tx_ring)
{
	struct mbuf *mbuf;
	struct ena_adapter *adapter = tx_ring->adapter;
	struct ena_com_io_sq* io_sq;
	int ena_qid;
	int ret = 0;

	if (unlikely((if_getdrvflags(adapter->ifp) & IFF_DRV_RUNNING) == 0))
		return;

	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_LINK_UP, adapter)))
		return;

	ena_qid = ENA_IO_TXQ_IDX(tx_ring->que->id);
	io_sq = &adapter->ena_dev->io_sq_queues[ena_qid];

	while ((mbuf = drbr_peek(adapter->ifp, tx_ring->br)) != NULL) {
		ena_trace(ENA_DBG | ENA_TXPTH, "\ndequeued mbuf %p with flags %#x and"
		    " header csum flags %#jx\n",
		    mbuf, mbuf->m_flags, (uint64_t)mbuf->m_pkthdr.csum_flags);

		if (unlikely(!tx_ring->running)) {
			drbr_putback(adapter->ifp, tx_ring->br, mbuf);
			break;
		}

		if (unlikely((ret = ena_xmit_mbuf(tx_ring, &mbuf)) != 0)) {
			if (ret == ENA_COM_NO_MEM) {
				drbr_putback(adapter->ifp, tx_ring->br, mbuf);
			} else if (ret == ENA_COM_NO_SPACE) {
				drbr_putback(adapter->ifp, tx_ring->br, mbuf);
			} else {
				m_freem(mbuf);
				drbr_advance(adapter->ifp, tx_ring->br);
			}

			break;
		}

		drbr_advance(adapter->ifp, tx_ring->br);

		if (unlikely((if_getdrvflags(adapter->ifp) &
		    IFF_DRV_RUNNING) == 0))
			return;

		tx_ring->acum_pkts++;

		BPF_MTAP(adapter->ifp, mbuf);
	}

	if (likely(tx_ring->acum_pkts != 0)) {
		wmb();
		/* Trigger the dma engine */
		ena_com_write_sq_doorbell(io_sq);
		counter_u64_add(tx_ring->tx_stats.doorbells, 1);
		tx_ring->acum_pkts = 0;
	}

	if (unlikely(!tx_ring->running))
		taskqueue_enqueue(tx_ring->que->cleanup_tq,
		    &tx_ring->que->cleanup_task);
}

static void
ena_deferred_mq_start(void *arg, int pending)
{
	struct ena_ring *tx_ring = (struct ena_ring *)arg;
	struct ifnet *ifp = tx_ring->adapter->ifp;

	while (!drbr_empty(ifp, tx_ring->br) &&
	    tx_ring->running &&
	    (if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0) {
		ENA_RING_MTX_LOCK(tx_ring);
		ena_start_xmit(tx_ring);
		ENA_RING_MTX_UNLOCK(tx_ring);
	}
}

static int
ena_mq_start(if_t ifp, struct mbuf *m)
{
	struct ena_adapter *adapter = ifp->if_softc;
	struct ena_ring *tx_ring;
	int ret, is_drbr_empty;
	uint32_t i;

	if (unlikely((if_getdrvflags(adapter->ifp) & IFF_DRV_RUNNING) == 0))
		return (ENODEV);

	/* Which queue to use */
	/*
	 * If everything is setup correctly, it should be the
	 * same bucket that the current CPU we're on is.
	 * It should improve performance.
	 */
	if (M_HASHTYPE_GET(m) != M_HASHTYPE_NONE) {
		i = m->m_pkthdr.flowid % adapter->num_queues;
	} else {
		i = curcpu % adapter->num_queues;
	}
	tx_ring = &adapter->tx_ring[i];

	/* Check if drbr is empty before putting packet */
	is_drbr_empty = drbr_empty(ifp, tx_ring->br);
	ret = drbr_enqueue(ifp, tx_ring->br, m);
	if (unlikely(ret != 0)) {
		taskqueue_enqueue(tx_ring->enqueue_tq, &tx_ring->enqueue_task);
		return (ret);
	}

	if (is_drbr_empty && (ENA_RING_MTX_TRYLOCK(tx_ring) != 0)) {
		ena_start_xmit(tx_ring);
		ENA_RING_MTX_UNLOCK(tx_ring);
	} else {
		taskqueue_enqueue(tx_ring->enqueue_tq, &tx_ring->enqueue_task);
	}

	return (0);
}

static void
ena_qflush(if_t ifp)
{
	struct ena_adapter *adapter = ifp->if_softc;
	struct ena_ring *tx_ring = adapter->tx_ring;
	int i;

	for(i = 0; i < adapter->num_queues; ++i, ++tx_ring)
		if (!drbr_empty(ifp, tx_ring->br)) {
			ENA_RING_MTX_LOCK(tx_ring);
			drbr_flush(ifp, tx_ring->br);
			ENA_RING_MTX_UNLOCK(tx_ring);
		}

	if_qflush(ifp);
}

static int
ena_calc_io_queue_num(struct ena_adapter *adapter,
    struct ena_com_dev_get_features_ctx *get_feat_ctx)
{
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	int io_tx_sq_num, io_tx_cq_num, io_rx_num, io_queue_num;

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

	io_queue_num = min_t(int, mp_ncpus, ENA_MAX_NUM_IO_QUEUES);
	io_queue_num = min_t(int, io_queue_num, io_rx_num);
	io_queue_num = min_t(int, io_queue_num, io_tx_sq_num);
	io_queue_num = min_t(int, io_queue_num, io_tx_cq_num);
	/* 1 IRQ for for mgmnt and 1 IRQ for each TX/RX pair */
	io_queue_num = min_t(int, io_queue_num,
	    pci_msix_count(adapter->pdev) - 1);

	return (io_queue_num);
}

static int
ena_enable_wc(struct resource *res)
{
#if defined(__i386) || defined(__amd64)
	vm_offset_t va;
	vm_size_t len;
	int rc;

	va = (vm_offset_t)rman_get_virtual(res);
	len = rman_get_size(res);
	/* Enable write combining */
	rc = pmap_change_attr(va, len, PAT_WRITE_COMBINING);
	if (unlikely(rc != 0)) {
		ena_trace(ENA_ALERT, "pmap_change_attr failed, %d\n", rc);
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
	struct ena_adapter *adapter = device_get_softc(pdev);
	int rc, rid;
	uint32_t llq_feature_mask;

	llq_feature_mask = 1 << ENA_ADMIN_LLQ;
	if (!(ena_dev->supported_features & llq_feature_mask)) {
		device_printf(pdev,
		    "LLQ is not supported. Fallback to host mode policy.\n");
		ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
		return (0);
	}

	rc = ena_com_config_dev_mode(ena_dev, llq, llq_default_configurations);
	if (unlikely(rc != 0)) {
		device_printf(pdev, "Failed to configure the device mode. "
		    "Fallback to host mode policy.\n");
		ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
		return (0);
	}

	/* Nothing to config, exit */
	if (ena_dev->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_HOST)
		return (0);

	/* Try to allocate resources for LLQ bar */
	rid = PCIR_BAR(ENA_MEM_BAR);
	adapter->memory = bus_alloc_resource_any(pdev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (unlikely(adapter->memory == NULL)) {
		device_printf(pdev, "unable to allocate LLQ bar resource. "
		    "Fallback to host mode policy.\n");
		ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;
		return (0);
	}

	/* Enable write combining for better LLQ performance */
	rc = ena_enable_wc(adapter->memory);
	if (unlikely(rc != 0)) {
		device_printf(pdev, "failed to enable write combining.\n");
		return (rc);
	}

	/*
	 * Save virtual address of the device's memory region
	 * for the ena_com layer.
	 */
	ena_dev->mem_bar = rman_get_virtual(adapter->memory);

	return (0);
}

static inline
void set_default_llq_configurations(struct ena_llq_configurations *llq_config)
{
	llq_config->llq_header_location = ENA_ADMIN_INLINE_HEADER;
	llq_config->llq_ring_entry_size = ENA_ADMIN_LIST_ENTRY_SIZE_128B;
	llq_config->llq_stride_ctrl = ENA_ADMIN_MULTIPLE_DESCS_PER_ENTRY;
	llq_config->llq_num_decs_before_header =
	    ENA_ADMIN_LLQ_NUM_DESCS_BEFORE_HEADER_2;
	llq_config->llq_ring_entry_size_value = 128;
}

static int
ena_calc_queue_size(struct ena_adapter *adapter,
    struct ena_calc_queue_size_ctx *ctx)
{
	struct ena_admin_feature_llq_desc *llq = &ctx->get_feat_ctx->llq;
	struct ena_com_dev *ena_dev = ctx->ena_dev;
	uint32_t tx_queue_size = ENA_DEFAULT_RING_SIZE;
	uint32_t rx_queue_size = adapter->rx_ring_size;

	if (ena_dev->supported_features & BIT(ENA_ADMIN_MAX_QUEUES_EXT)) {
		struct ena_admin_queue_ext_feature_fields *max_queue_ext =
		    &ctx->get_feat_ctx->max_queue_ext.max_queue_ext;
		rx_queue_size = min_t(uint32_t, rx_queue_size,
		    max_queue_ext->max_rx_cq_depth);
		rx_queue_size = min_t(uint32_t, rx_queue_size,
		    max_queue_ext->max_rx_sq_depth);
		tx_queue_size = min_t(uint32_t, tx_queue_size,
		    max_queue_ext->max_tx_cq_depth);

		if (ena_dev->tx_mem_queue_type ==
		    ENA_ADMIN_PLACEMENT_POLICY_DEV)
			tx_queue_size = min_t(uint32_t, tx_queue_size,
			    llq->max_llq_depth);
		else
			tx_queue_size = min_t(uint32_t, tx_queue_size,
			    max_queue_ext->max_tx_sq_depth);

		ctx->max_rx_sgl_size = min_t(uint16_t, ENA_PKT_MAX_BUFS,
		    max_queue_ext->max_per_packet_rx_descs);
		ctx->max_tx_sgl_size = min_t(uint16_t, ENA_PKT_MAX_BUFS,
		    max_queue_ext->max_per_packet_tx_descs);
	} else {
		struct ena_admin_queue_feature_desc *max_queues =
		    &ctx->get_feat_ctx->max_queues;
		rx_queue_size = min_t(uint32_t, rx_queue_size,
		    max_queues->max_cq_depth);
		rx_queue_size = min_t(uint32_t, rx_queue_size,
		    max_queues->max_sq_depth);
		tx_queue_size = min_t(uint32_t, tx_queue_size,
		    max_queues->max_cq_depth);

		if (ena_dev->tx_mem_queue_type ==
		    ENA_ADMIN_PLACEMENT_POLICY_DEV)
			tx_queue_size = min_t(uint32_t, tx_queue_size,
			    llq->max_llq_depth);
		else
			tx_queue_size = min_t(uint32_t, tx_queue_size,
			    max_queues->max_sq_depth);

		ctx->max_rx_sgl_size = min_t(uint16_t, ENA_PKT_MAX_BUFS,
		    max_queues->max_packet_tx_descs);
		ctx->max_tx_sgl_size = min_t(uint16_t, ENA_PKT_MAX_BUFS,
		    max_queues->max_packet_rx_descs);
	}

	/* round down to the nearest power of 2 */
	rx_queue_size = 1 << (fls(rx_queue_size) - 1);
	tx_queue_size = 1 << (fls(tx_queue_size) - 1);

	if (unlikely(rx_queue_size == 0 || tx_queue_size == 0)) {
		device_printf(ctx->pdev, "Invalid queue size\n");
		return (EFAULT);
	}

	ctx->rx_queue_size = rx_queue_size;
	ctx->tx_queue_size = tx_queue_size;

	return (0);
}

static int
ena_handle_updated_queues(struct ena_adapter *adapter,
    struct ena_com_dev_get_features_ctx *get_feat_ctx)
{
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	struct ena_calc_queue_size_ctx calc_queue_ctx = { 0 };
	device_t pdev = adapter->pdev;
	bool are_queues_changed = false;
	int io_queue_num, rc;

	calc_queue_ctx.ena_dev = ena_dev;
	calc_queue_ctx.get_feat_ctx = get_feat_ctx;
	calc_queue_ctx.pdev = pdev;

	io_queue_num = ena_calc_io_queue_num(adapter, get_feat_ctx);
	rc = ena_calc_queue_size(adapter, &calc_queue_ctx);
	if (unlikely(rc != 0 || io_queue_num <= 0))
		return EFAULT;

	if (adapter->tx_ring->buf_ring_size != adapter->buf_ring_size)
		are_queues_changed = true;

	if (unlikely(adapter->tx_ring_size > calc_queue_ctx.tx_queue_size ||
	    adapter->rx_ring_size > calc_queue_ctx.rx_queue_size)) {
		device_printf(pdev,
		    "Not enough resources to allocate requested queue sizes "
		    "(TX,RX)=(%d,%d), falling back to queue sizes "
		    "(TX,RX)=(%d,%d)\n",
		    adapter->tx_ring_size,
		    adapter->rx_ring_size,
		    calc_queue_ctx.tx_queue_size,
		    calc_queue_ctx.rx_queue_size);
		adapter->tx_ring_size = calc_queue_ctx.tx_queue_size;
		adapter->rx_ring_size = calc_queue_ctx.rx_queue_size;
		adapter->max_tx_sgl_size = calc_queue_ctx.max_tx_sgl_size;
		adapter->max_rx_sgl_size = calc_queue_ctx.max_rx_sgl_size;
		are_queues_changed = true;
	}

	if (unlikely(adapter->num_queues > io_queue_num)) {
		device_printf(pdev,
		    "Not enough resources to allocate %d queues, "
		    "falling back to %d queues\n",
		    adapter->num_queues, io_queue_num);
		adapter->num_queues = io_queue_num;
		if (ENA_FLAG_ISSET(ENA_FLAG_RSS_ACTIVE, adapter)) {
			ena_com_rss_destroy(ena_dev);
			rc = ena_rss_init_default(adapter);
			if (unlikely(rc != 0) && (rc != EOPNOTSUPP)) {
				device_printf(pdev, "Cannot init RSS rc: %d\n",
				    rc);
				return (rc);
			}
		}
		are_queues_changed = true;
	}

	if (unlikely(are_queues_changed)) {
		ena_free_all_io_rings_resources(adapter);
		ena_init_io_rings(adapter);
	}

	return (0);
}

static int
ena_rss_init_default(struct ena_adapter *adapter)
{
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	device_t dev = adapter->pdev;
	int qid, rc, i;

	rc = ena_com_rss_init(ena_dev, ENA_RX_RSS_TABLE_LOG_SIZE);
	if (unlikely(rc != 0)) {
		device_printf(dev, "Cannot init indirect table\n");
		return (rc);
	}

	for (i = 0; i < ENA_RX_RSS_TABLE_SIZE; i++) {
		qid = i % adapter->num_queues;
		rc = ena_com_indirect_table_fill_entry(ena_dev, i,
		    ENA_IO_RXQ_IDX(qid));
		if (unlikely((rc != 0) && (rc != EOPNOTSUPP))) {
			device_printf(dev, "Cannot fill indirect table\n");
			goto err_rss_destroy;
		}
	}

	rc = ena_com_fill_hash_function(ena_dev, ENA_ADMIN_CRC32, NULL,
	    ENA_HASH_KEY_SIZE, 0xFFFFFFFF);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP))) {
		device_printf(dev, "Cannot fill hash function\n");
		goto err_rss_destroy;
	}

	rc = ena_com_set_default_hash_ctrl(ena_dev);
	if (unlikely((rc != 0) && (rc != EOPNOTSUPP))) {
		device_printf(dev, "Cannot fill hash control\n");
		goto err_rss_destroy;
	}

	return (0);

err_rss_destroy:
	ena_com_rss_destroy(ena_dev);
	return (rc);
}

static void
ena_rss_init_default_deferred(void *arg)
{
	struct ena_adapter *adapter;
	devclass_t dc;
	int max;
	int rc;

	dc = devclass_find("ena");
	if (unlikely(dc == NULL)) {
		ena_trace(ENA_ALERT, "No devclass ena\n");
		return;
	}

	max = devclass_get_maxunit(dc);
	while (max-- >= 0) {
		adapter = devclass_get_softc(dc, max);
		if (adapter != NULL) {
			rc = ena_rss_init_default(adapter);
			ENA_FLAG_SET_ATOMIC(ENA_FLAG_RSS_ACTIVE, adapter);
			if (unlikely(rc != 0)) {
				device_printf(adapter->pdev,
				    "WARNING: RSS was not properly initialized,"
				    " it will affect bandwidth\n");
				ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_RSS_ACTIVE, adapter);
			}
		}
	}
}
SYSINIT(ena_rss_init, SI_SUB_KICK_SCHEDULER, SI_ORDER_SECOND, ena_rss_init_default_deferred, NULL);

static void
ena_config_host_info(struct ena_com_dev *ena_dev, device_t dev)
{
	struct ena_admin_host_info *host_info;
	uintptr_t rid;
	int rc;

	/* Allocate only the host info */
	rc = ena_com_allocate_host_info(ena_dev);
	if (unlikely(rc != 0)) {
		ena_trace(ENA_ALERT, "Cannot allocate host info\n");
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

	host_info->driver_version =
		(DRV_MODULE_VER_MAJOR) |
		(DRV_MODULE_VER_MINOR << ENA_ADMIN_HOST_INFO_MINOR_SHIFT) |
		(DRV_MODULE_VER_SUBMINOR << ENA_ADMIN_HOST_INFO_SUB_MINOR_SHIFT);
	host_info->num_cpus = mp_ncpus;

	rc = ena_com_set_host_attributes(ena_dev);
	if (unlikely(rc != 0)) {
		if (rc == EOPNOTSUPP)
			ena_trace(ENA_WARNING, "Cannot set host attributes\n");
		else
			ena_trace(ENA_ALERT, "Cannot set host attributes\n");

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
	struct ena_com_dev* ena_dev = adapter->ena_dev;
	bool readless_supported;
	uint32_t aenq_groups;
	int dma_width;
	int rc;

	rc = ena_com_mmio_reg_read_request_init(ena_dev);
	if (unlikely(rc != 0)) {
		device_printf(pdev, "failed to init mmio read less\n");
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
		device_printf(pdev, "Can not reset device\n");
		goto err_mmio_read_less;
	}

	rc = ena_com_validate_version(ena_dev);
	if (unlikely(rc != 0)) {
		device_printf(pdev, "device version is too low\n");
		goto err_mmio_read_less;
	}

	dma_width = ena_com_get_dma_width(ena_dev);
	if (unlikely(dma_width < 0)) {
		device_printf(pdev, "Invalid dma width value %d", dma_width);
		rc = dma_width;
		goto err_mmio_read_less;
	}
	adapter->dma_width = dma_width;

	/* ENA admin level init */
	rc = ena_com_admin_init(ena_dev, &aenq_handlers);
	if (unlikely(rc != 0)) {
		device_printf(pdev,
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
		device_printf(pdev,
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
		device_printf(pdev, "Cannot configure aenq groups rc: %d\n", rc);
		goto err_admin_init;
	}

	*wd_active = !!(aenq_groups & BIT(ENA_ADMIN_KEEP_ALIVE));

	return (0);

err_admin_init:
	ena_com_delete_host_info(ena_dev);
	ena_com_admin_destroy(ena_dev);
err_mmio_read_less:
	ena_com_mmio_reg_read_request_destroy(ena_dev);

	return (rc);
}

static int ena_enable_msix_and_set_admin_interrupts(struct ena_adapter *adapter,
    int io_vectors)
{
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	int rc;

	rc = ena_enable_msix(adapter);
	if (unlikely(rc != 0)) {
		device_printf(adapter->pdev, "Error with MSI-X enablement\n");
		return (rc);
	}

	ena_setup_mgmnt_intr(adapter);

	rc = ena_request_mgmnt_irq(adapter);
	if (unlikely(rc != 0)) {
		device_printf(adapter->pdev, "Cannot setup mgmnt queue intr\n");
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
static void ena_keep_alive_wd(void *adapter_data,
    struct ena_admin_aenq_entry *aenq_e)
{
	struct ena_adapter *adapter = (struct ena_adapter *)adapter_data;
	struct ena_admin_aenq_keep_alive_desc *desc;
	sbintime_t stime;
	uint64_t rx_drops;

	desc = (struct ena_admin_aenq_keep_alive_desc *)aenq_e;

	rx_drops = ((uint64_t)desc->rx_drops_high << 32) | desc->rx_drops_low;
	counter_u64_zero(adapter->hw_stats.rx_drops);
	counter_u64_add(adapter->hw_stats.rx_drops, rx_drops);

	stime = getsbinuptime();
	atomic_store_rel_64(&adapter->keep_alive_timestamp, stime);
}

/* Check for keep alive expiration */
static void check_for_missing_keep_alive(struct ena_adapter *adapter)
{
	sbintime_t timestamp, time;

	if (adapter->wd_active == 0)
		return;

	if (adapter->keep_alive_timeout == ENA_HW_HINTS_NO_TIMEOUT)
		return;

	timestamp = atomic_load_acq_64(&adapter->keep_alive_timestamp);
	time = getsbinuptime() - timestamp;
	if (unlikely(time > adapter->keep_alive_timeout)) {
		device_printf(adapter->pdev,
		    "Keep alive watchdog timeout.\n");
		counter_u64_add(adapter->dev_stats.wd_expired, 1);
		if (likely(!ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, adapter))) {
			adapter->reset_reason = ENA_REGS_RESET_KEEP_ALIVE_TO;
			ENA_FLAG_SET_ATOMIC(ENA_FLAG_TRIGGER_RESET, adapter);
		}
	}
}

/* Check if admin queue is enabled */
static void check_for_admin_com_state(struct ena_adapter *adapter)
{
	if (unlikely(ena_com_get_admin_running_state(adapter->ena_dev) ==
	    false)) {
		device_printf(adapter->pdev,
		    "ENA admin queue is not in running state!\n");
		counter_u64_add(adapter->dev_stats.admin_q_pause, 1);
		if (likely(!ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, adapter))) {
			adapter->reset_reason = ENA_REGS_RESET_ADMIN_TO;
			ENA_FLAG_SET_ATOMIC(ENA_FLAG_TRIGGER_RESET, adapter);
		}
	}
}

static int
check_for_rx_interrupt_queue(struct ena_adapter *adapter,
    struct ena_ring *rx_ring)
{
	if (likely(rx_ring->first_interrupt))
		return (0);

	if (ena_com_cq_empty(rx_ring->ena_com_io_cq))
		return (0);

	rx_ring->no_interrupt_event_cnt++;

	if (rx_ring->no_interrupt_event_cnt == ENA_MAX_NO_INTERRUPT_ITERATIONS) {
		device_printf(adapter->pdev, "Potential MSIX issue on Rx side "
		    "Queue = %d. Reset the device\n", rx_ring->qid);
		if (likely(!ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, adapter))) {
			adapter->reset_reason = ENA_REGS_RESET_MISS_INTERRUPT;
			ENA_FLAG_SET_ATOMIC(ENA_FLAG_TRIGGER_RESET, adapter);
		}
		return (EIO);
	}

	return (0);
}

static int
check_missing_comp_in_tx_queue(struct ena_adapter *adapter,
    struct ena_ring *tx_ring)
{
	struct bintime curtime, time;
	struct ena_tx_buffer *tx_buf;
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

		if (unlikely(!tx_ring->first_interrupt &&
		    time_offset > 2 * adapter->missing_tx_timeout)) {
			/*
			 * If after graceful period interrupt is still not
			 * received, we schedule a reset.
			 */
			device_printf(adapter->pdev,
			    "Potential MSIX issue on Tx side Queue = %d. "
			    "Reset the device\n", tx_ring->qid);
			if (likely(!ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET,
			    adapter))) {
				adapter->reset_reason =
				    ENA_REGS_RESET_MISS_INTERRUPT;
				ENA_FLAG_SET_ATOMIC(ENA_FLAG_TRIGGER_RESET,
				    adapter);
			}
			return (EIO);
		}

		/* Check again if packet is still waiting */
		if (unlikely(time_offset > adapter->missing_tx_timeout)) {

			if (!tx_buf->print_once)
				ena_trace(ENA_WARNING, "Found a Tx that wasn't "
				    "completed on time, qid %d, index %d.\n",
				    tx_ring->qid, i);

			tx_buf->print_once = true;
			missed_tx++;
		}
	}

	if (unlikely(missed_tx > adapter->missing_tx_threshold)) {
		device_printf(adapter->pdev,
		    "The number of lost tx completion is above the threshold "
		    "(%d > %d). Reset the device\n",
		    missed_tx, adapter->missing_tx_threshold);
		if (likely(!ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, adapter))) {
			adapter->reset_reason = ENA_REGS_RESET_MISS_TX_CMPL;
			ENA_FLAG_SET_ATOMIC(ENA_FLAG_TRIGGER_RESET, adapter);
		}
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

	for (i = adapter->next_monitored_tx_qid; i < adapter->num_queues; i++) {
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

	adapter->next_monitored_tx_qid = i % adapter->num_queues;
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

	for (i = 0; i < adapter->num_queues; i++) {
		rx_ring = &adapter->rx_ring[i];

		refill_required = ena_com_free_desc(rx_ring->ena_com_io_sq);
		if (unlikely(refill_required == (rx_ring->ring_size - 1))) {
			rx_ring->empty_rx_queue++;

			if (rx_ring->empty_rx_queue >= EMPTY_RX_REFILL)	{
				counter_u64_add(rx_ring->rx_stats.empty_rx_ring,
				    1);

				device_printf(adapter->pdev,
				    "trigger refill for ring %d\n", i);

				taskqueue_enqueue(rx_ring->que->cleanup_tq,
				    &rx_ring->que->cleanup_task);
				rx_ring->empty_rx_queue = 0;
			}
		} else {
			rx_ring->empty_rx_queue = 0;
		}
	}
}

static void ena_update_hints(struct ena_adapter *adapter,
			     struct ena_admin_ena_hw_hints *hints)
{
	struct ena_com_dev *ena_dev = adapter->ena_dev;

	if (hints->admin_completion_tx_timeout)
		ena_dev->admin_queue.completion_timeout =
		    hints->admin_completion_tx_timeout * 1000;

	if (hints->mmio_read_timeout)
		/* convert to usec */
		ena_dev->mmio_read.reg_read_to =
		    hints->mmio_read_timeout * 1000;

	if (hints->missed_tx_completion_count_threshold_to_reset)
		adapter->missing_tx_threshold =
		    hints->missed_tx_completion_count_threshold_to_reset;

	if (hints->missing_tx_completion_timeout) {
		if (hints->missing_tx_completion_timeout ==
		     ENA_HW_HINTS_NO_TIMEOUT)
			adapter->missing_tx_timeout = ENA_HW_HINTS_NO_TIMEOUT;
		else
			adapter->missing_tx_timeout =
			    SBT_1MS * hints->missing_tx_completion_timeout;
	}

	if (hints->driver_watchdog_timeout) {
		if (hints->driver_watchdog_timeout == ENA_HW_HINTS_NO_TIMEOUT)
			adapter->keep_alive_timeout = ENA_HW_HINTS_NO_TIMEOUT;
		else
			adapter->keep_alive_timeout =
			    SBT_1MS * hints->driver_watchdog_timeout;
	}
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

	if (host_info != NULL)
		ena_update_host_info(host_info, adapter->ifp);

	if (unlikely(ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, adapter))) {
		device_printf(adapter->pdev, "Trigger reset is on\n");
		taskqueue_enqueue(adapter->reset_tq, &adapter->reset_task);
		return;
	}

	/*
	 * Schedule another timeout one second from now.
	 */
	callout_schedule_sbt(&adapter->timer_service, SBT_1S, SBT_1S, 0);
}

static void
ena_destroy_device(struct ena_adapter *adapter, bool graceful)
{
	if_t ifp = adapter->ifp;
	struct ena_com_dev *ena_dev = adapter->ena_dev;
	bool dev_up;

	if (!ENA_FLAG_ISSET(ENA_FLAG_DEVICE_RUNNING, adapter))
		return;

	if_link_state_change(ifp, LINK_STATE_DOWN);

	callout_drain(&adapter->timer_service);

	dev_up = ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, adapter);
	if (dev_up)
		ENA_FLAG_SET_ATOMIC(ENA_FLAG_DEV_UP_BEFORE_RESET, adapter);
	else
		ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_DEV_UP_BEFORE_RESET, adapter);

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
		device_printf(adapter->pdev,
		    "Error, mac address are different\n");
		return (EINVAL);
	}

	if (get_feat_ctx->dev_attr.max_mtu < if_getmtu(adapter->ifp)) {
		device_printf(adapter->pdev,
		    "Error, device max mtu is smaller than ifp MTU\n");
		return (EINVAL);
	}

	return 0;
}

static int
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
		device_printf(dev, "Cannot initialize device\n");
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
		device_printf(dev, "Validation of device parameters failed\n");
		goto err_device_destroy;
	}

	rc = ena_handle_updated_queues(adapter, &get_feat_ctx);
	if (rc != 0)
		goto err_device_destroy;

	ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_ONGOING_RESET, adapter);
	/* Make sure we don't have a race with AENQ Links state handler */
	if (ENA_FLAG_ISSET(ENA_FLAG_LINK_UP, adapter))
		if_link_state_change(ifp, LINK_STATE_UP);

	rc = ena_enable_msix_and_set_admin_interrupts(adapter,
	    adapter->num_queues);
	if (rc != 0) {
		device_printf(dev, "Enable MSI-X failed\n");
		goto err_device_destroy;
	}

	/* If the interface was up before the reset bring it up */
	if (ENA_FLAG_ISSET(ENA_FLAG_DEV_UP_BEFORE_RESET, adapter)) {
		rc = ena_up(adapter);
		if (rc != 0) {
			device_printf(dev, "Failed to create I/O queues\n");
			goto err_disable_msix;
		}
	}

	ENA_FLAG_SET_ATOMIC(ENA_FLAG_DEVICE_RUNNING, adapter);
	callout_reset_sbt(&adapter->timer_service, SBT_1S, SBT_1S,
	    ena_timer_service, (void *)adapter, 0);

	device_printf(dev,
	    "Device reset completed successfully, Driver info: %s\n", ena_version);

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
	device_printf(dev, "Reset attempt failed. Can not reset the device\n");

	return (rc);
}

static void
ena_reset_task(void *arg, int pending)
{
	struct ena_adapter *adapter = (struct ena_adapter *)arg;

	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_TRIGGER_RESET, adapter))) {
		device_printf(adapter->pdev,
		    "device reset scheduled but trigger_reset is off\n");
		return;
	}

	sx_xlock(&adapter->ioctl_sx);
	ena_destroy_device(adapter, false);
	ena_restore_device(adapter);
	sx_unlock(&adapter->ioctl_sx);
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
	struct ena_llq_configurations llq_config;
	struct ena_calc_queue_size_ctx calc_queue_ctx = { 0 };
	static int version_printed;
	struct ena_adapter *adapter;
	struct ena_com_dev *ena_dev = NULL;
	const char *queue_type_str;
	int io_queue_num;
	int rid, rc;

	adapter = device_get_softc(pdev);
	adapter->pdev = pdev;

	mtx_init(&adapter->global_mtx, "ENA global mtx", NULL, MTX_DEF);
	sx_init(&adapter->ioctl_sx, "ENA ioctl sx");

	/* Set up the timer service */
	callout_init_mtx(&adapter->timer_service, &adapter->global_mtx, 0);
	adapter->keep_alive_timeout = DEFAULT_KEEP_ALIVE_TO;
	adapter->missing_tx_timeout = DEFAULT_TX_CMP_TO;
	adapter->missing_tx_max_queues = DEFAULT_TX_MONITORED_QUEUES;
	adapter->missing_tx_threshold = DEFAULT_TX_CMP_THRESHOLD;

	if (version_printed++ == 0)
		device_printf(pdev, "%s\n", ena_version);

	/* Allocate memory for ena_dev structure */
	ena_dev = malloc(sizeof(struct ena_com_dev), M_DEVBUF,
	    M_WAITOK | M_ZERO);

	adapter->ena_dev = ena_dev;
	ena_dev->dmadev = pdev;

	rid = PCIR_BAR(ENA_REG_BAR);
	adapter->memory = NULL;
	adapter->registers = bus_alloc_resource_any(pdev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (unlikely(adapter->registers == NULL)) {
		device_printf(pdev,
		    "unable to allocate bus resource: registers!\n");
		rc = ENOMEM;
		goto err_dev_free;
	}

	ena_dev->bus = malloc(sizeof(struct ena_bus), M_DEVBUF,
	    M_WAITOK | M_ZERO);

	/* Store register resources */
	((struct ena_bus*)(ena_dev->bus))->reg_bar_t =
	    rman_get_bustag(adapter->registers);
	((struct ena_bus*)(ena_dev->bus))->reg_bar_h =
	    rman_get_bushandle(adapter->registers);

	if (unlikely(((struct ena_bus*)(ena_dev->bus))->reg_bar_h == 0)) {
		device_printf(pdev, "failed to pmap registers bar\n");
		rc = ENXIO;
		goto err_bus_free;
	}

	ena_dev->tx_mem_queue_type = ENA_ADMIN_PLACEMENT_POLICY_HOST;

	/* Initially clear all the flags */
	ENA_FLAG_ZERO(adapter);

	/* Device initialization */
	rc = ena_device_init(adapter, pdev, &get_feat_ctx, &adapter->wd_active);
	if (unlikely(rc != 0)) {
		device_printf(pdev, "ENA device init failed! (err: %d)\n", rc);
		rc = ENXIO;
		goto err_bus_free;
	}

	set_default_llq_configurations(&llq_config);

#if defined(__arm__) || defined(__aarch64__)
	/*
	 * Force LLQ disable, as the driver is not supporting WC enablement
	 * on the ARM architecture. Using LLQ without WC would affect
	 * performance in a negative way.
	 */
	ena_dev->supported_features &= ~(1 << ENA_ADMIN_LLQ);
#endif
	rc = ena_set_queues_placement_policy(pdev, ena_dev, &get_feat_ctx.llq,
	     &llq_config);
	if (unlikely(rc != 0)) {
		device_printf(pdev, "failed to set placement policy\n");
		goto err_com_free;
	}

	if (ena_dev->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_HOST)
		queue_type_str = "Regular";
	else
		queue_type_str = "Low Latency";
	device_printf(pdev, "Placement policy: %s\n", queue_type_str);

	adapter->keep_alive_timestamp = getsbinuptime();

	adapter->tx_offload_cap = get_feat_ctx.offload.tx;

	memcpy(adapter->mac_addr, get_feat_ctx.dev_attr.mac_addr,
	    ETHER_ADDR_LEN);

	calc_queue_ctx.ena_dev = ena_dev;
	calc_queue_ctx.get_feat_ctx = &get_feat_ctx;
	calc_queue_ctx.pdev = pdev;

	/* calculate IO queue number to create */
	io_queue_num = ena_calc_io_queue_num(adapter, &get_feat_ctx);

	ENA_ASSERT(io_queue_num > 0, "Invalid queue number: %d\n",
	    io_queue_num);
	adapter->num_queues = io_queue_num;

	adapter->max_mtu = get_feat_ctx.dev_attr.max_mtu;
	// Set the requested Rx ring size
	adapter->rx_ring_size = ENA_DEFAULT_RING_SIZE;
	/* calculatre ring sizes */
	rc = ena_calc_queue_size(adapter, &calc_queue_ctx);
	if (unlikely((rc != 0) || (io_queue_num <= 0))) {
		rc = EFAULT;
		goto err_com_free;
	}

	adapter->reset_reason = ENA_REGS_RESET_NORMAL;

	adapter->tx_ring_size = calc_queue_ctx.tx_queue_size;
	adapter->rx_ring_size = calc_queue_ctx.rx_queue_size;

	adapter->max_tx_sgl_size = calc_queue_ctx.max_tx_sgl_size;
	adapter->max_rx_sgl_size = calc_queue_ctx.max_rx_sgl_size;

	adapter->buf_ring_size = ENA_DEFAULT_BUF_RING_SIZE;

	/* set up dma tags for rx and tx buffers */
	rc = ena_setup_tx_dma_tag(adapter);
	if (unlikely(rc != 0)) {
		device_printf(pdev, "Failed to create TX DMA tag\n");
		goto err_com_free;
	}

	rc = ena_setup_rx_dma_tag(adapter);
	if (unlikely(rc != 0)) {
		device_printf(pdev, "Failed to create RX DMA tag\n");
		goto err_tx_tag_free;
	}

	/* initialize rings basic information */
	device_printf(pdev,
	    "Creating %d io queues. Rx queue size: %d, Tx queue size: %d\n",
	    io_queue_num,
	    calc_queue_ctx.rx_queue_size,
	    calc_queue_ctx.tx_queue_size);
	ena_init_io_rings(adapter);

	rc = ena_enable_msix_and_set_admin_interrupts(adapter, io_queue_num);
	if (unlikely(rc != 0)) {
		device_printf(pdev,
		    "Failed to enable and set the admin interrupts\n");
		goto err_io_free;
	}

	/* setup network interface */
	rc = ena_setup_ifnet(pdev, adapter, &get_feat_ctx);
	if (unlikely(rc != 0)) {
		device_printf(pdev, "Error with network interface setup\n");
		goto err_msix_free;
	}

	/* Initialize reset task queue */
	TASK_INIT(&adapter->reset_task, 0, ena_reset_task, adapter);
	adapter->reset_tq = taskqueue_create("ena_reset_enqueue",
	    M_WAITOK | M_ZERO, taskqueue_thread_enqueue, &adapter->reset_tq);
	taskqueue_start_threads(&adapter->reset_tq, 1, PI_NET,
	    "%s rstq", device_get_nameunit(adapter->pdev));

	/* Initialize statistics */
	ena_alloc_counters((counter_u64_t *)&adapter->dev_stats,
	    sizeof(struct ena_stats_dev));
	ena_alloc_counters((counter_u64_t *)&adapter->hw_stats,
	    sizeof(struct ena_hw_stats));
	ena_sysctl_add_nodes(adapter);

	/* Tell the stack that the interface is not active */
	if_setdrvflagbits(adapter->ifp, IFF_DRV_OACTIVE, IFF_DRV_RUNNING);
	ENA_FLAG_SET_ATOMIC(ENA_FLAG_DEVICE_RUNNING, adapter);

	return (0);

err_msix_free:
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
		device_printf(adapter->pdev ,"VLAN is in use, detach first\n");
		return (EBUSY);
	}

	ether_ifdetach(adapter->ifp);

	/* Free reset task and callout */
	callout_drain(&adapter->timer_service);
	while (taskqueue_cancel(adapter->reset_tq, &adapter->reset_task, NULL))
		taskqueue_drain(adapter->reset_tq, &adapter->reset_task);
	taskqueue_free(adapter->reset_tq);

	sx_xlock(&adapter->ioctl_sx);
	ena_down(adapter);
	ena_destroy_device(adapter, true);
	sx_unlock(&adapter->ioctl_sx);

	ena_free_all_io_rings_resources(adapter);

	ena_free_counters((counter_u64_t *)&adapter->hw_stats,
	    sizeof(struct ena_hw_stats));
	ena_free_counters((counter_u64_t *)&adapter->dev_stats,
	    sizeof(struct ena_stats_dev));

	rc = ena_free_rx_dma_tag(adapter);
	if (unlikely(rc != 0))
		device_printf(adapter->pdev,
		    "Unmapped RX DMA tag associations\n");

	rc = ena_free_tx_dma_tag(adapter);
	if (unlikely(rc != 0))
		device_printf(adapter->pdev,
		    "Unmapped TX DMA tag associations\n");

	ena_free_irqs(adapter);

	ena_free_pci_resources(adapter);

	if (likely(ENA_FLAG_ISSET(ENA_FLAG_RSS_ACTIVE, adapter)))
		ena_com_rss_destroy(ena_dev);

	ena_com_delete_host_info(ena_dev);

	mtx_destroy(&adapter->global_mtx);
	sx_destroy(&adapter->ioctl_sx);

	if_free(adapter->ifp);

	if (ena_dev->bus != NULL)
		free(ena_dev->bus, M_DEVBUF);

	if (ena_dev != NULL)
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
		device_printf(adapter->pdev, "link is UP\n");
		ENA_FLAG_SET_ATOMIC(ENA_FLAG_LINK_UP, adapter);
		if (!ENA_FLAG_ISSET(ENA_FLAG_ONGOING_RESET, adapter))
			if_link_state_change(ifp, LINK_STATE_UP);
	} else {
		device_printf(adapter->pdev, "link is DOWN\n");
		if_link_state_change(ifp, LINK_STATE_DOWN);
		ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_LINK_UP, adapter);
	}
}

static void ena_notification(void *adapter_data,
    struct ena_admin_aenq_entry *aenq_e)
{
	struct ena_adapter *adapter = (struct ena_adapter *)adapter_data;
	struct ena_admin_ena_hw_hints *hints;

	ENA_WARN(aenq_e->aenq_common_desc.group != ENA_ADMIN_NOTIFICATION,
	    "Invalid group(%x) expected %x\n",	aenq_e->aenq_common_desc.group,
	    ENA_ADMIN_NOTIFICATION);

	switch (aenq_e->aenq_common_desc.syndrom) {
	case ENA_ADMIN_UPDATE_HINTS:
		hints =
		    (struct ena_admin_ena_hw_hints *)(&aenq_e->inline_data_w4);
		ena_update_hints(adapter, hints);
		break;
	default:
		device_printf(adapter->pdev,
		    "Invalid aenq notification link state %d\n",
		    aenq_e->aenq_common_desc.syndrom);
	}
}

/**
 * This handler will called for unknown event group or unimplemented handlers
 **/
static void
unimplemented_aenq_handler(void *adapter_data,
    struct ena_admin_aenq_entry *aenq_e)
{
	struct ena_adapter *adapter = (struct ena_adapter *)adapter_data;

	device_printf(adapter->pdev,
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

static device_method_t ena_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe, ena_probe),
    DEVMETHOD(device_attach, ena_attach),
    DEVMETHOD(device_detach, ena_detach),
    DEVMETHOD_END
};

static driver_t ena_driver = {
    "ena", ena_methods, sizeof(struct ena_adapter),
};

devclass_t ena_devclass;
DRIVER_MODULE(ena, pci, ena_driver, ena_devclass, 0, 0);
MODULE_PNP_INFO("U16:vendor;U16:device", pci, ena, ena_vendor_info_array,
    nitems(ena_vendor_info_array) - 1);
MODULE_DEPEND(ena, pci, 1, 1, 1);
MODULE_DEPEND(ena, ether, 1, 1, 1);

/*********************************************************************/
