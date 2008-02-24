/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
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
 *
 * $FreeBSD: src/sys/dev/nxge/xgehal/xgehal-ring.c,v 1.1.2.1 2007/11/02 00:52:33 rwatson Exp $
 */

#include <dev/nxge/include/xgehal-ring.h>
#include <dev/nxge/include/xgehal-device.h>

#if defined(XGE_OS_DMA_REQUIRES_SYNC) && defined(XGE_HAL_DMA_DTR_STREAMING)
static ptrdiff_t
__hal_ring_item_dma_offset(xge_hal_mempool_h mempoolh,
	           void *item)
{
	int memblock_idx;
	void *memblock;

	/* get owner memblock index */
	memblock_idx = __hal_ring_block_memblock_idx(item);

	/* get owner memblock by memblock index */
	memblock = __hal_mempool_memblock(mempoolh, memblock_idx);

	return (char*)item - (char*)memblock;
}
#endif

static dma_addr_t
__hal_ring_item_dma_addr(xge_hal_mempool_h mempoolh, void *item,
	    pci_dma_h *dma_handle)
{
	int memblock_idx;
	void *memblock;
	xge_hal_mempool_dma_t *memblock_dma_object;
	ptrdiff_t dma_item_offset;

	/* get owner memblock index */
	memblock_idx = __hal_ring_block_memblock_idx((xge_hal_ring_block_t *) item);

	/* get owner memblock by memblock index */
	memblock = __hal_mempool_memblock((xge_hal_mempool_t *) mempoolh,
	                                    memblock_idx);

	/* get memblock DMA object by memblock index */
	memblock_dma_object =
	    __hal_mempool_memblock_dma((xge_hal_mempool_t *) mempoolh,
	                                memblock_idx);

	/* calculate offset in the memblock of this item */
	dma_item_offset = (char*)item - (char*)memblock;

	*dma_handle = memblock_dma_object->handle;

	return memblock_dma_object->addr + dma_item_offset;
}

static void
__hal_ring_rxdblock_link(xge_hal_mempool_h mempoolh,
	         xge_hal_ring_t *ring, int from, int to)
{
	xge_hal_ring_block_t *to_item, *from_item;
	dma_addr_t to_dma, from_dma;
	pci_dma_h to_dma_handle, from_dma_handle;

	/* get "from" RxD block */
	from_item = (xge_hal_ring_block_t *)
	            __hal_mempool_item((xge_hal_mempool_t *) mempoolh, from);
	xge_assert(from_item);

	/* get "to" RxD block */
	to_item = (xge_hal_ring_block_t *) 
	          __hal_mempool_item((xge_hal_mempool_t *) mempoolh, to);
	xge_assert(to_item);

	/* return address of the beginning of previous RxD block */
	to_dma = __hal_ring_item_dma_addr(mempoolh, to_item, &to_dma_handle);

	/* set next pointer for this RxD block to point on
	 * previous item's DMA start address */
	__hal_ring_block_next_pointer_set(from_item, to_dma);

	/* return "from" RxD block's DMA start address */
	from_dma =
	    __hal_ring_item_dma_addr(mempoolh, from_item, &from_dma_handle);

#if defined(XGE_OS_DMA_REQUIRES_SYNC) && defined(XGE_HAL_DMA_DTR_STREAMING)
	/* we must sync "from" RxD block, so hardware will see it */
	xge_os_dma_sync(ring->channel.pdev,
	              from_dma_handle,
	          from_dma + XGE_HAL_RING_NEXT_BLOCK_POINTER_OFFSET,
	          __hal_ring_item_dma_offset(mempoolh, from_item) +
	                XGE_HAL_RING_NEXT_BLOCK_POINTER_OFFSET,
	          sizeof(u64),
	          XGE_OS_DMA_DIR_TODEVICE);
#endif

	xge_debug_ring(XGE_TRACE, "block%d:0x"XGE_OS_LLXFMT" => block%d:0x"XGE_OS_LLXFMT,
	    from, (unsigned long long)from_dma, to,
	    (unsigned long long)to_dma);
}

static xge_hal_status_e
__hal_ring_mempool_item_alloc(xge_hal_mempool_h mempoolh,
	              void *memblock,
	              int memblock_index,
	              xge_hal_mempool_dma_t *dma_object,
	              void *item,
	              int index,
	              int is_last,
	              void *userdata)
{
	int i;
	xge_hal_ring_t *ring = (xge_hal_ring_t *)userdata;

	xge_assert(item);
	xge_assert(ring);


	/* format rxds array */
	for (i=ring->rxds_per_block-1; i>=0; i--) {
	    void *rxdblock_priv;
	    xge_hal_ring_rxd_priv_t *rxd_priv;
	    xge_hal_ring_rxd_1_t *rxdp;
	    int reserve_index = index * ring->rxds_per_block + i;
	    int memblock_item_idx;

	    ring->reserved_rxds_arr[reserve_index] = (char *)item +
	            (ring->rxds_per_block - 1 - i) * ring->rxd_size;

	    /* Note: memblock_item_idx is index of the item within
	     *       the memblock. For instance, in case of three RxD-blocks
	     *       per memblock this value can be 0,1 or 2. */
	    rxdblock_priv =
	        __hal_mempool_item_priv((xge_hal_mempool_t *) mempoolh,
	                                memblock_index, item,
	                                &memblock_item_idx);
	    rxdp = (xge_hal_ring_rxd_1_t *)
	        ring->reserved_rxds_arr[reserve_index];
	    rxd_priv = (xge_hal_ring_rxd_priv_t *) (void *)
	        ((char*)rxdblock_priv + ring->rxd_priv_size * i);

	    /* pre-format per-RxD Ring's private */
	    rxd_priv->dma_offset = (char*)rxdp - (char*)memblock;
	    rxd_priv->dma_addr = dma_object->addr +  rxd_priv->dma_offset;
	    rxd_priv->dma_handle = dma_object->handle;
#ifdef XGE_DEBUG_ASSERT
	    rxd_priv->dma_object = dma_object;
#endif

	    /* pre-format Host_Control */
#if defined(XGE_HAL_USE_5B_MODE)
	    if (ring->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_5) {
	        xge_hal_ring_rxd_5_t *rxdp_5 = (xge_hal_ring_rxd_5_t *)rxdp;
#if defined(XGE_OS_PLATFORM_64BIT)
	        xge_assert(memblock_index <= 0xFFFF);
	        xge_assert(i <= 0xFFFF);
	        /* store memblock's index */
	        rxdp_5->host_control = (u32)memblock_index << 16;
	        /* store index of memblock's private */
	        rxdp_5->host_control |= (u32)(memblock_item_idx *
	                        ring->rxds_per_block + i);
#else
	        /* 32-bit case */
	        rxdp_5->host_control = (u32)rxd_priv;
#endif
	    } else {
	        /* 1b and 3b modes */
	        rxdp->host_control = (u64)(ulong_t)rxd_priv;
	    }
#else
	    /* 1b and 3b modes */
	    rxdp->host_control = (u64)(ulong_t)rxd_priv;
#endif
	}

	__hal_ring_block_memblock_idx_set((xge_hal_ring_block_t *) item, memblock_index);

	if (is_last) {
	    /* link last one with first one */
	    __hal_ring_rxdblock_link(mempoolh, ring, 0, index);
	}

	if (index > 0 ) {
	     /* link this RxD block with previous one */
	    __hal_ring_rxdblock_link(mempoolh, ring, index, index-1);
	}

	return XGE_HAL_OK;
}

 xge_hal_status_e
__hal_ring_initial_replenish(xge_hal_channel_t *channel,
	             xge_hal_channel_reopen_e reopen)
{
	xge_hal_dtr_h dtr = NULL;

	while (xge_hal_channel_dtr_count(channel) > 0) {
	    xge_hal_status_e status;

	    status = xge_hal_ring_dtr_reserve(channel, &dtr);
	    xge_assert(status == XGE_HAL_OK);

	    if (channel->dtr_init) {
	        status = channel->dtr_init(channel,
	                                    dtr, channel->reserve_length,
	                                    channel->userdata,
	                reopen);
	        if (status != XGE_HAL_OK) {
	            xge_hal_ring_dtr_free(channel, dtr);
	            xge_hal_channel_abort(channel,
	                XGE_HAL_CHANNEL_OC_NORMAL);
	            return status;
	        }
	    }

	    xge_hal_ring_dtr_post(channel, dtr);
	}

	return XGE_HAL_OK;
}

xge_hal_status_e
__hal_ring_open(xge_hal_channel_h channelh, xge_hal_channel_attr_t *attr)
{
	xge_hal_status_e status;
	xge_hal_device_t *hldev;
	xge_hal_ring_t *ring = (xge_hal_ring_t *)channelh;
	xge_hal_ring_queue_t *queue;


	/* Note: at this point we have channel.devh and channel.pdev
	 *       pre-set only! */

	hldev = (xge_hal_device_t *)ring->channel.devh;
	ring->config = &hldev->config.ring;
	queue = &ring->config->queue[attr->post_qid];
	ring->indicate_max_pkts = queue->indicate_max_pkts;
	ring->buffer_mode = queue->buffer_mode;

	xge_assert(queue->configured);

#if defined(XGE_HAL_RX_MULTI_RESERVE)
	xge_os_spin_lock_init(&ring->channel.reserve_lock, hldev->pdev);
#elif defined(XGE_HAL_RX_MULTI_RESERVE_IRQ)
	xge_os_spin_lock_init_irq(&ring->channel.reserve_lock, hldev->irqh);
#endif
#if defined(XGE_HAL_RX_MULTI_POST)
	xge_os_spin_lock_init(&ring->channel.post_lock, hldev->pdev);
#elif defined(XGE_HAL_RX_MULTI_POST_IRQ)
	xge_os_spin_lock_init_irq(&ring->channel.post_lock, hldev->irqh);
#endif

	ring->rxd_size = XGE_HAL_RING_RXD_SIZEOF(queue->buffer_mode);
	ring->rxd_priv_size =
	    sizeof(xge_hal_ring_rxd_priv_t) + attr->per_dtr_space;

	/* how many RxDs can fit into one block. Depends on configured
	 * buffer_mode. */
	ring->rxds_per_block = XGE_HAL_RING_RXDS_PER_BLOCK(queue->buffer_mode);

	/* calculate actual RxD block private size */
	ring->rxdblock_priv_size = ring->rxd_priv_size * ring->rxds_per_block;

	ring->reserved_rxds_arr = (void **) xge_os_malloc(ring->channel.pdev,
	          sizeof(void*) * queue->max * ring->rxds_per_block);

	if (ring->reserved_rxds_arr == NULL) {
	    __hal_ring_close(channelh);
	    return XGE_HAL_ERR_OUT_OF_MEMORY;
	}

	ring->mempool = __hal_mempool_create(
	                 hldev->pdev,
	                 ring->config->memblock_size,
	                 XGE_HAL_RING_RXDBLOCK_SIZE,
	                 ring->rxdblock_priv_size,
	                 queue->initial, queue->max,
	                 __hal_ring_mempool_item_alloc,
	                 NULL, /* nothing to free */
	                 ring);
	if (ring->mempool == NULL) {
	    __hal_ring_close(channelh);
	    return XGE_HAL_ERR_OUT_OF_MEMORY;
	}

	status = __hal_channel_initialize(channelh,
	                  attr,
	                  ring->reserved_rxds_arr,
	                  queue->initial * ring->rxds_per_block,
	                  queue->max * ring->rxds_per_block,
	                  0 /* no threshold for ring! */);
	if (status != XGE_HAL_OK) {
	    __hal_ring_close(channelh);
	    return status;
	}

	/* sanity check that everything formatted ok */
	xge_assert(ring->reserved_rxds_arr[0] ==
	        (char *)ring->mempool->items_arr[0] +
	          (ring->rxds_per_block * ring->rxd_size - ring->rxd_size));

	    /* Note:
	 * Specifying dtr_init callback means two things:
	 * 1) dtrs need to be initialized by ULD at channel-open time;
	 * 2) dtrs need to be posted at channel-open time
	 *    (that's what the initial_replenish() below does)
	 * Currently we don't have a case when the 1) is done without the 2).
	 */
	if (ring->channel.dtr_init) {
	    if ((status = __hal_ring_initial_replenish (
	                    (xge_hal_channel_t *) channelh,
	                    XGE_HAL_CHANNEL_OC_NORMAL) )
	                    != XGE_HAL_OK) {
	        __hal_ring_close(channelh);
	        return status;
	    }
	}

	/* initial replenish will increment the counter in its post() routine,
	 * we have to reset it */
	ring->channel.usage_cnt = 0;

	return XGE_HAL_OK;
}

void
__hal_ring_close(xge_hal_channel_h channelh)
{
	xge_hal_ring_t *ring = (xge_hal_ring_t *)channelh;
	xge_hal_ring_queue_t *queue;
#if defined(XGE_HAL_RX_MULTI_RESERVE)||defined(XGE_HAL_RX_MULTI_RESERVE_IRQ)||\
	defined(XGE_HAL_RX_MULTI_POST) || defined(XGE_HAL_RX_MULTI_POST_IRQ)
	xge_hal_device_t *hldev = (xge_hal_device_t *)ring->channel.devh;
#endif

	xge_assert(ring->channel.pdev);

	queue = &ring->config->queue[ring->channel.post_qid];

	if (ring->mempool) {
	    __hal_mempool_destroy(ring->mempool);
	}

	if (ring->reserved_rxds_arr) {
	    xge_os_free(ring->channel.pdev,
	              ring->reserved_rxds_arr,
	          sizeof(void*) * queue->max * ring->rxds_per_block);
	}

	__hal_channel_terminate(channelh);

#if defined(XGE_HAL_RX_MULTI_RESERVE)
	xge_os_spin_lock_destroy(&ring->channel.reserve_lock, hldev->pdev);
#elif defined(XGE_HAL_RX_MULTI_RESERVE_IRQ)
	xge_os_spin_lock_destroy_irq(&ring->channel.reserve_lock, hldev->pdev);
#endif
#if defined(XGE_HAL_RX_MULTI_POST)
	xge_os_spin_lock_destroy(&ring->channel.post_lock, hldev->pdev);
#elif defined(XGE_HAL_RX_MULTI_POST_IRQ)
	xge_os_spin_lock_destroy_irq(&ring->channel.post_lock, hldev->pdev);
#endif
}

void
__hal_ring_prc_enable(xge_hal_channel_h channelh)
{
	xge_hal_ring_t *ring = (xge_hal_ring_t *)channelh;
	xge_hal_device_t *hldev = (xge_hal_device_t *)ring->channel.devh;
	xge_hal_pci_bar0_t *bar0;
	u64 val64;
	void *first_block;
	int block_num;
	xge_hal_ring_queue_t *queue;
	pci_dma_h dma_handle;

	xge_assert(ring);
	xge_assert(ring->channel.pdev);
	bar0 = (xge_hal_pci_bar0_t *) (void *)
	        ((xge_hal_device_t *)ring->channel.devh)->bar0;

	queue = &ring->config->queue[ring->channel.post_qid];
	xge_assert(queue->buffer_mode == 1 ||
	        queue->buffer_mode == 3 ||
	        queue->buffer_mode == 5);

	/* last block in fact becomes first. This is just the way it
	 * is filled up and linked by item_alloc() */

	block_num = queue->initial;
	first_block = __hal_mempool_item(ring->mempool, block_num - 1);
	val64 = __hal_ring_item_dma_addr(ring->mempool,
	                 first_block, &dma_handle);
	xge_os_pio_mem_write64(ring->channel.pdev, ring->channel.regh0,
	        val64, &bar0->prc_rxd0_n[ring->channel.post_qid]);

	xge_debug_ring(XGE_TRACE, "ring%d PRC DMA addr 0x"XGE_OS_LLXFMT" initialized",
	        ring->channel.post_qid, (unsigned long long)val64);

	val64 = xge_os_pio_mem_read64(ring->channel.pdev,
	    ring->channel.regh0, &bar0->prc_ctrl_n[ring->channel.post_qid]);
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC &&
	    !queue->rth_en) {
	    val64 |= XGE_HAL_PRC_CTRL_RTH_DISABLE;
	}
	val64 |= XGE_HAL_PRC_CTRL_RC_ENABLED;

	val64 |= vBIT((queue->buffer_mode >> 1),14,2);/* 1,3 or 5 => 0,1 or 2 */
	val64 &= ~XGE_HAL_PRC_CTRL_RXD_BACKOFF_INTERVAL(0xFFFFFF);
	val64 |= XGE_HAL_PRC_CTRL_RXD_BACKOFF_INTERVAL(
	    (hldev->config.pci_freq_mherz * queue->backoff_interval_us));

	/* Beware: no snoop by the bridge if (no_snoop_bits) */
	val64 |= XGE_HAL_PRC_CTRL_NO_SNOOP(queue->no_snoop_bits);

	    /* Herc: always use group_reads */
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC)
	        val64 |= XGE_HAL_PRC_CTRL_GROUP_READS;

	if (hldev->config.bimodal_interrupts)
	    if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC)
	        val64 |= XGE_HAL_PRC_CTRL_BIMODAL_INTERRUPT;
	
	xge_os_pio_mem_write64(ring->channel.pdev, ring->channel.regh0,
	        val64, &bar0->prc_ctrl_n[ring->channel.post_qid]);

	/* Configure Receive Protocol Assist */
	val64 = xge_os_pio_mem_read64(ring->channel.pdev,
	        ring->channel.regh0, &bar0->rx_pa_cfg);
	val64 |= XGE_HAL_RX_PA_CFG_SCATTER_MODE(ring->config->scatter_mode);
	val64 |= (XGE_HAL_RX_PA_CFG_IGNORE_SNAP_OUI | XGE_HAL_RX_PA_CFG_IGNORE_LLC_CTRL);
	/* Clean STRIP_VLAN_TAG bit and set as config from upper layer */
	val64 &= ~XGE_HAL_RX_PA_CFG_STRIP_VLAN_TAG_MODE(1);
	val64 |= XGE_HAL_RX_PA_CFG_STRIP_VLAN_TAG_MODE(ring->config->strip_vlan_tag);

	xge_os_pio_mem_write64(ring->channel.pdev, ring->channel.regh0,
	        val64, &bar0->rx_pa_cfg);

	xge_debug_ring(XGE_TRACE, "ring%d enabled in buffer_mode %d",
	        ring->channel.post_qid, queue->buffer_mode);
}

void
__hal_ring_prc_disable(xge_hal_channel_h channelh)
{
	xge_hal_ring_t *ring = (xge_hal_ring_t *)channelh;
	xge_hal_pci_bar0_t *bar0;
	u64 val64;

	xge_assert(ring);
	xge_assert(ring->channel.pdev);
	bar0 = (xge_hal_pci_bar0_t *) (void *)
	        ((xge_hal_device_t *)ring->channel.devh)->bar0;

	val64 = xge_os_pio_mem_read64(ring->channel.pdev,
	ring->channel.regh0,
	              &bar0->prc_ctrl_n[ring->channel.post_qid]);
	val64 &= ~((u64) XGE_HAL_PRC_CTRL_RC_ENABLED);
	xge_os_pio_mem_write64(ring->channel.pdev, ring->channel.regh0,
	        val64, &bar0->prc_ctrl_n[ring->channel.post_qid]);
}

void
__hal_ring_hw_initialize(xge_hal_device_h devh)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64 val64;
	int i, j;

	/* Rx DMA intialization. */

	val64 = 0;
	for (i = 0; i < XGE_HAL_MAX_RING_NUM; i++) {
	    if (!hldev->config.ring.queue[i].configured)
	        continue;
	    val64 |= vBIT(hldev->config.ring.queue[i].priority,
	                        (5 + (i * 8)), 3);
	}
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->rx_queue_priority);
	xge_debug_ring(XGE_TRACE, "Rings priority configured to 0x"XGE_OS_LLXFMT,
	        (unsigned long long)val64);

	/* Configuring ring queues according to per-ring configuration */
	val64 = 0;
	for (i = 0; i < XGE_HAL_MAX_RING_NUM; i++) {
	    if (!hldev->config.ring.queue[i].configured)
	        continue;
	    val64 |= vBIT(hldev->config.ring.queue[i].dram_size_mb,(i*8),8);
	}
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                     &bar0->rx_queue_cfg);
	xge_debug_ring(XGE_TRACE, "DRAM configured to 0x"XGE_OS_LLXFMT,
	        (unsigned long long)val64);

	if (!hldev->config.rts_qos_en &&
	    !hldev->config.rts_port_en &&
	    !hldev->config.rts_mac_en) {

	    /*
	     * Activate default (QoS-based) Rx steering
	     */

	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                      &bar0->rts_qos_steering);
	    for (j = 0; j < 8 /* QoS max */; j++)
	    {
	        for (i = 0; i < XGE_HAL_MAX_RING_NUM; i++)
	        {
	            if (!hldev->config.ring.queue[i].configured)
	                continue;
	            if (!hldev->config.ring.queue[i].rth_en)
	                val64 |= (BIT(i) >> (j*8));
	        }
	    }
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                   &bar0->rts_qos_steering);
	    xge_debug_ring(XGE_TRACE, "QoS steering configured to 0x"XGE_OS_LLXFMT,
	               (unsigned long long)val64);

	}

	/* Note: If a queue does not exist, it should be assigned a maximum
	 *   length of zero. Otherwise, packet loss could occur.
	 *   P. 4-4 User guide.
	 *
	 * All configured rings will be properly set at device open time
	 * by utilizing device_mtu_set() API call. */
	for (i = 0; i < XGE_HAL_MAX_RING_NUM; i++) {
	    if (hldev->config.ring.queue[i].configured)
	        continue;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, 0ULL,
	                         &bar0->rts_frm_len_n[i]);
	}

#ifdef XGE_HAL_HERC_EMULATION
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	    ((u8 *)bar0 + 0x2e60)); /* mc_rldram_mrs_herc */
	val64 |= 0x0000000000010000;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	    ((u8 *)bar0 + 0x2e60));

	val64 |= 0x003a000000000000;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	    ((u8 *)bar0 + 0x2e40)); /* mc_rldram_ref_herc */
	xge_os_mdelay(2000);
#endif

	/* now enabling MC-RLDRAM after setting MC_QUEUE sizes */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                            &bar0->mc_rldram_mrs);
	val64 |= XGE_HAL_MC_RLDRAM_QUEUE_SIZE_ENABLE |
	     XGE_HAL_MC_RLDRAM_MRS_ENABLE;
	__hal_pio_mem_write32_upper(hldev->pdev, hldev->regh0, (u32)(val64>>32),
	                     &bar0->mc_rldram_mrs);
	xge_os_wmb();
	__hal_pio_mem_write32_lower(hldev->pdev, hldev->regh0, (u32)val64,
	                     &bar0->mc_rldram_mrs);

	/* RLDRAM initialization procedure require 500us to complete */
	xge_os_mdelay(1);

	/* Temporary fixes for Herc RLDRAM */
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC) {
	    val64 = XGE_HAL_MC_RLDRAM_SET_REF_PERIOD(0x0279);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                         &bar0->mc_rldram_ref_per_herc);

	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                            &bar0->mc_rldram_mrs_herc);
	    xge_debug_ring(XGE_TRACE, "default mc_rldram_mrs_herc 0x"XGE_OS_LLXFMT,
	               (unsigned long long)val64);

	    val64 = 0x0003570003010300ULL;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                           &bar0->mc_rldram_mrs_herc);

	    xge_os_mdelay(1);
	}

	/*
	 * Assign MSI-X vectors
	 */
	for (i = 0; i < XGE_HAL_MAX_RING_NUM; i++) {
	    xge_list_t *item;
	    xge_hal_channel_t *channel = NULL;

	    if (!hldev->config.ring.queue[i].configured ||
	        !hldev->config.ring.queue[i].intr_vector ||
	        !hldev->config.intr_mode != XGE_HAL_INTR_MODE_MSIX)
	        continue;

	    /* find channel */
	    xge_list_for_each(item, &hldev->free_channels) {
	        xge_hal_channel_t *tmp;
	        tmp = xge_container_of(item, xge_hal_channel_t,
	                       item);
	        if (tmp->type == XGE_HAL_CHANNEL_TYPE_RING &&
	            tmp->post_qid == i) {
	            channel = tmp;
	            break;
	        }
	    }

	    if (channel) {
	        xge_hal_channel_msix_set(channel,
	            hldev->config.ring.queue[i].intr_vector);
	    }
	}

	xge_debug_ring(XGE_TRACE, "%s", "ring channels initialized");
}

void
__hal_ring_mtu_set(xge_hal_device_h devh, int new_frmlen)
{
	int i;
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;

	for (i = 0; i < XGE_HAL_MAX_RING_NUM; i++) {
	    if (!hldev->config.ring.queue[i].configured)
	        continue;
	    if (hldev->config.ring.queue[i].max_frm_len !=
	                    XGE_HAL_RING_USE_MTU) {
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                XGE_HAL_MAC_RTS_FRM_LEN_SET(
	            hldev->config.ring.queue[i].max_frm_len),
	            &bar0->rts_frm_len_n[i]);
	    } else {
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                   XGE_HAL_MAC_RTS_FRM_LEN_SET(new_frmlen),
	                   &bar0->rts_frm_len_n[i]);
	    }
	}
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	               XGE_HAL_RMAC_MAX_PYLD_LEN(new_frmlen),
	                   &bar0->rmac_max_pyld_len);
}
