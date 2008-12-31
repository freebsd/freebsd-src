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
 * $FreeBSD: src/sys/dev/nxge/xgehal/xgehal-fifo.c,v 1.1.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $
 */

#include <dev/nxge/include/xgehal-fifo.h>
#include <dev/nxge/include/xgehal-device.h>

static xge_hal_status_e
__hal_fifo_mempool_item_alloc(xge_hal_mempool_h mempoolh,
	              void *memblock,
	              int memblock_index,
	              xge_hal_mempool_dma_t *dma_object,
	              void *item,
	              int index,
	              int is_last,
	              void *userdata)
{
	int memblock_item_idx;
	xge_hal_fifo_txdl_priv_t *txdl_priv;
	xge_hal_fifo_txd_t *txdp = (xge_hal_fifo_txd_t *)item;
	xge_hal_fifo_t *fifo = (xge_hal_fifo_t *)userdata;

	xge_assert(item);
	txdl_priv = (xge_hal_fifo_txdl_priv_t *) \
	            __hal_mempool_item_priv((xge_hal_mempool_t *) mempoolh,
	                                    memblock_index,
	                                    item,
	                                    &memblock_item_idx);
	xge_assert(txdl_priv);

	/* pre-format HAL's TxDL's private */
	txdl_priv->dma_offset = (char*)item - (char*)memblock;
	txdl_priv->dma_addr = dma_object->addr + txdl_priv->dma_offset;
	txdl_priv->dma_handle = dma_object->handle;
	txdl_priv->memblock   = memblock;
	txdl_priv->first_txdp = (xge_hal_fifo_txd_t *)item;
	txdl_priv->next_txdl_priv = NULL;
	txdl_priv->dang_txdl = NULL;
	txdl_priv->dang_frags = 0;
	txdl_priv->alloc_frags = 0;

#ifdef XGE_DEBUG_ASSERT
	txdl_priv->dma_object = dma_object;
#endif
	txdp->host_control = (u64)(ulong_t)txdl_priv;

#ifdef XGE_HAL_ALIGN_XMIT
	txdl_priv->align_vaddr = NULL;
	txdl_priv->align_dma_addr = (dma_addr_t)0;

#ifndef XGE_HAL_ALIGN_XMIT_ALLOC_RT
	{
	xge_hal_status_e status;
	if (fifo->config->alignment_size) {
	        status =__hal_fifo_dtr_align_alloc_map(fifo, txdp);
	    if (status != XGE_HAL_OK)  {
	            xge_debug_mm(XGE_ERR,
	                  "align buffer[%d] %d bytes, status %d",
	              index,
	              fifo->align_size,
	              status);
	            return status;
	    }
	}
	}
#endif
#endif

	if (fifo->channel.dtr_init) {
	    fifo->channel.dtr_init(fifo, (xge_hal_dtr_h)txdp, index,
	           fifo->channel.userdata, XGE_HAL_CHANNEL_OC_NORMAL);
	}

	return XGE_HAL_OK;
}


static xge_hal_status_e
__hal_fifo_mempool_item_free(xge_hal_mempool_h mempoolh,
	              void *memblock,
	              int memblock_index,
	              xge_hal_mempool_dma_t *dma_object,
	              void *item,
	              int index,
	              int is_last,
	              void *userdata)
{
	int memblock_item_idx;
	xge_hal_fifo_txdl_priv_t *txdl_priv;
#ifdef XGE_HAL_ALIGN_XMIT
	xge_hal_fifo_t *fifo = (xge_hal_fifo_t *)userdata;
#endif

	xge_assert(item);

	txdl_priv = (xge_hal_fifo_txdl_priv_t *) \
	            __hal_mempool_item_priv((xge_hal_mempool_t *) mempoolh,
	                                    memblock_index,
	                                    item,
	                                    &memblock_item_idx);
	xge_assert(txdl_priv);

#ifdef XGE_HAL_ALIGN_XMIT
	if (fifo->config->alignment_size) {
	    if (txdl_priv->align_dma_addr != 0) {
	        xge_os_dma_unmap(fifo->channel.pdev,
	               txdl_priv->align_dma_handle,
	               txdl_priv->align_dma_addr,
	               fifo->align_size,
	               XGE_OS_DMA_DIR_TODEVICE);

	        txdl_priv->align_dma_addr = 0;
	    }

	    if (txdl_priv->align_vaddr != NULL) {
	        xge_os_dma_free(fifo->channel.pdev,
	              txdl_priv->align_vaddr,
	              fifo->align_size,
	              &txdl_priv->align_dma_acch,
	              &txdl_priv->align_dma_handle);

	        txdl_priv->align_vaddr = NULL;
	    }
	}
#endif

	return XGE_HAL_OK;
}

xge_hal_status_e
__hal_fifo_open(xge_hal_channel_h channelh, xge_hal_channel_attr_t *attr)
{
	xge_hal_device_t *hldev;
	xge_hal_status_e status;
	xge_hal_fifo_t *fifo = (xge_hal_fifo_t *)channelh;
	xge_hal_fifo_queue_t *queue;
	int i, txdl_size, max_arr_index, mid_point;
	xge_hal_dtr_h  dtrh;

	hldev = (xge_hal_device_t *)fifo->channel.devh;
	fifo->config = &hldev->config.fifo;
	queue = &fifo->config->queue[attr->post_qid];

#if defined(XGE_HAL_TX_MULTI_RESERVE)
	xge_os_spin_lock_init(&fifo->channel.reserve_lock, hldev->pdev);
#elif defined(XGE_HAL_TX_MULTI_RESERVE_IRQ)
	xge_os_spin_lock_init_irq(&fifo->channel.reserve_lock, hldev->irqh);
#endif
#if defined(XGE_HAL_TX_MULTI_POST)
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA)  {
	            fifo->post_lock_ptr = &hldev->xena_post_lock;
	} else {
	        xge_os_spin_lock_init(&fifo->channel.post_lock, hldev->pdev);
	            fifo->post_lock_ptr = &fifo->channel.post_lock;
	}
#elif defined(XGE_HAL_TX_MULTI_POST_IRQ)
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA)  {
	            fifo->post_lock_ptr = &hldev->xena_post_lock;
	} else {
	        xge_os_spin_lock_init_irq(&fifo->channel.post_lock,
	                hldev->irqh);
	            fifo->post_lock_ptr = &fifo->channel.post_lock;
	}
#endif

	fifo->align_size =
	    fifo->config->alignment_size * fifo->config->max_aligned_frags;

	/* Initializing the BAR1 address as the start of
	 * the FIFO queue pointer and as a location of FIFO control
	 * word. */
	fifo->hw_pair =
	        (xge_hal_fifo_hw_pair_t *) (void *)(hldev->bar1 +
	            (attr->post_qid * XGE_HAL_FIFO_HW_PAIR_OFFSET));

	/* apply "interrupts per txdl" attribute */
	fifo->interrupt_type = XGE_HAL_TXD_INT_TYPE_UTILZ;
	if (queue->intr) {
	    fifo->interrupt_type = XGE_HAL_TXD_INT_TYPE_PER_LIST;
	}
	fifo->no_snoop_bits =
	    (int)(XGE_HAL_TX_FIFO_NO_SNOOP(queue->no_snoop_bits));

	/*
	 * FIFO memory management strategy:
	 *
	 * TxDL splitted into three independent parts:
	 *  - set of TxD's
	 *  - TxD HAL private part
	 *  - upper layer private part
	 *
	 * Adaptative memory allocation used. i.e. Memory allocated on
	 * demand with the size which will fit into one memory block.
	 * One memory block may contain more than one TxDL. In simple case
	 * memory block size can be equal to CPU page size. On more
	 * sophisticated OS's memory block can be contigious across
	 * several pages.
	 *
	 * During "reserve" operations more memory can be allocated on demand
	 * for example due to FIFO full condition.
	 *
	 * Pool of memory memblocks never shrinks except __hal_fifo_close
	 * routine which will essentially stop channel and free the resources.
	 */

	/* TxDL common private size == TxDL private + ULD private */
	fifo->priv_size = sizeof(xge_hal_fifo_txdl_priv_t) +
	attr->per_dtr_space;
	fifo->priv_size = ((fifo->priv_size + __xge_os_cacheline_size -1) /
	                           __xge_os_cacheline_size) *
	                           __xge_os_cacheline_size;

	/* recompute txdl size to be cacheline aligned */
	fifo->txdl_size = fifo->config->max_frags * sizeof(xge_hal_fifo_txd_t);
	txdl_size = ((fifo->txdl_size + __xge_os_cacheline_size - 1) /
	        __xge_os_cacheline_size) * __xge_os_cacheline_size;

	if (fifo->txdl_size != txdl_size)
	        xge_debug_fifo(XGE_ERR, "cacheline > 128 ( ?? ): %d, %d, %d, %d",
	    fifo->config->max_frags, fifo->txdl_size, txdl_size,
	    __xge_os_cacheline_size);

	fifo->txdl_size = txdl_size;

	/* since dtr_init() callback will be called from item_alloc(),
	 * the same way channels userdata might be used prior to
	 * channel_initialize() */
	fifo->channel.dtr_init = attr->dtr_init;
	fifo->channel.userdata = attr->userdata;
	fifo->txdl_per_memblock = fifo->config->memblock_size /
	    fifo->txdl_size;

	fifo->mempool = __hal_mempool_create(hldev->pdev,
	                     fifo->config->memblock_size,
	                     fifo->txdl_size,
	                     fifo->priv_size,
	                     queue->initial,
	                     queue->max,
	                     __hal_fifo_mempool_item_alloc,
	                     __hal_fifo_mempool_item_free,
	                     fifo);
	if (fifo->mempool == NULL) {
	    return XGE_HAL_ERR_OUT_OF_MEMORY;
	}

	status = __hal_channel_initialize(channelh, attr,
	                (void **) __hal_mempool_items_arr(fifo->mempool),
	                queue->initial, queue->max,
	                fifo->config->reserve_threshold);
	if (status != XGE_HAL_OK) {
	    __hal_fifo_close(channelh);
	    return status;
	}
	xge_debug_fifo(XGE_TRACE,
	    "DTR  reserve_length:%d reserve_top:%d\n"
	    "max_frags:%d reserve_threshold:%d\n"
	    "memblock_size:%d alignment_size:%d max_aligned_frags:%d",
	    fifo->channel.reserve_length, fifo->channel.reserve_top,
	    fifo->config->max_frags, fifo->config->reserve_threshold,
	    fifo->config->memblock_size, fifo->config->alignment_size,
	    fifo->config->max_aligned_frags);

#ifdef XGE_DEBUG_ASSERT
	for ( i = 0; i < fifo->channel.reserve_length; i++) {
	    xge_debug_fifo(XGE_TRACE, "DTR before reversing index:%d"
	    " handle:%p", i, fifo->channel.reserve_arr[i]);
	}
#endif

	xge_assert(fifo->channel.reserve_length);
	/* reverse the FIFO dtr array */
	max_arr_index   = fifo->channel.reserve_length - 1;
	max_arr_index   -=fifo->channel.reserve_top;
	xge_assert(max_arr_index);
	mid_point = (fifo->channel.reserve_length - fifo->channel.reserve_top)/2;
	for (i = 0; i < mid_point; i++) {
	    dtrh =  fifo->channel.reserve_arr[i];
	    fifo->channel.reserve_arr[i] =
	        fifo->channel.reserve_arr[max_arr_index - i];
	    fifo->channel.reserve_arr[max_arr_index  - i] = dtrh;
	}

#ifdef XGE_DEBUG_ASSERT
	for ( i = 0; i < fifo->channel.reserve_length; i++) {
	    xge_debug_fifo(XGE_TRACE, "DTR after reversing index:%d"
	    " handle:%p", i, fifo->channel.reserve_arr[i]);
	}
#endif

	return XGE_HAL_OK;
}

void
__hal_fifo_close(xge_hal_channel_h channelh)
{
	xge_hal_fifo_t *fifo = (xge_hal_fifo_t *)channelh;
	xge_hal_device_t *hldev = (xge_hal_device_t *)fifo->channel.devh;

	if (fifo->mempool) {
	    __hal_mempool_destroy(fifo->mempool);
	}

	__hal_channel_terminate(channelh);

#if defined(XGE_HAL_TX_MULTI_RESERVE)
	xge_os_spin_lock_destroy(&fifo->channel.reserve_lock, hldev->pdev);
#elif defined(XGE_HAL_TX_MULTI_RESERVE_IRQ)
	xge_os_spin_lock_destroy_irq(&fifo->channel.reserve_lock, hldev->pdev);
#endif
	if (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC)  {
#if defined(XGE_HAL_TX_MULTI_POST)
	    xge_os_spin_lock_destroy(&fifo->channel.post_lock, hldev->pdev);
#elif defined(XGE_HAL_TX_MULTI_POST_IRQ)
	    xge_os_spin_lock_destroy_irq(&fifo->channel.post_lock,
	                     hldev->pdev);
#endif
	}
}

void
__hal_fifo_hw_initialize(xge_hal_device_h devh)
{
	xge_hal_device_t *hldev = (xge_hal_device_t *)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)(void *)hldev->bar0;
	u64* tx_fifo_partitions[4];
	u64* tx_fifo_wrr[5];
	u64  tx_fifo_wrr_value[5];
	u64 val64, part0;
	int i;

	/*  Tx DMA Initialization */

	tx_fifo_partitions[0] = &bar0->tx_fifo_partition_0;
	tx_fifo_partitions[1] = &bar0->tx_fifo_partition_1;
	tx_fifo_partitions[2] = &bar0->tx_fifo_partition_2;
	tx_fifo_partitions[3] = &bar0->tx_fifo_partition_3;

	tx_fifo_wrr[0] = &bar0->tx_w_round_robin_0;
	tx_fifo_wrr[1] = &bar0->tx_w_round_robin_1;
	tx_fifo_wrr[2] = &bar0->tx_w_round_robin_2;
	tx_fifo_wrr[3] = &bar0->tx_w_round_robin_3;
	tx_fifo_wrr[4] = &bar0->tx_w_round_robin_4;

	tx_fifo_wrr_value[0] = XGE_HAL_FIFO_WRR_0;
	tx_fifo_wrr_value[1] = XGE_HAL_FIFO_WRR_1;
	tx_fifo_wrr_value[2] = XGE_HAL_FIFO_WRR_2;
	tx_fifo_wrr_value[3] = XGE_HAL_FIFO_WRR_3;
	tx_fifo_wrr_value[4] = XGE_HAL_FIFO_WRR_4;

	/* Note: WRR calendar must be configured before the transmit
	 *       FIFOs are enabled! page 6-77 user guide */

	if (!hldev->config.rts_qos_en) {
	    /* all zeroes for Round-Robin */
	    for (i = 0; i < XGE_HAL_FIFO_MAX_WRR; i++) {
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, 0,
	                       tx_fifo_wrr[i]);
	    }

	    /* reset all of them but '0' */
	    for (i=1; i < XGE_HAL_FIFO_MAX_PARTITION; i++) {
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, 0ULL,
	                       tx_fifo_partitions[i]);
	    }
	} else { /* Change the default settings */

	    for (i = 0; i < XGE_HAL_FIFO_MAX_WRR; i++) {
	        xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                   tx_fifo_wrr_value[i], tx_fifo_wrr[i]);
	    }
	}

	/* configure only configured FIFOs */
	val64 = 0; part0 = 0;
	for (i = 0; i < XGE_HAL_MAX_FIFO_NUM; i++) {
	    int reg_half = i % 2;
	    int reg_num = i / 2;

	    if (hldev->config.fifo.queue[i].configured) {
	        int priority = hldev->config.fifo.queue[i].priority;
	        val64 |=
	            vBIT((hldev->config.fifo.queue[i].max-1),
	            (((reg_half) * 32) + 19),
	            13) | vBIT(priority, (((reg_half)*32) + 5), 3);
	    }

	    /* NOTE: do write operation for each second u64 half
	     *       or force for first one if configured number
	     *   is even */
	    if (reg_half) {
	        if (reg_num == 0) {
	            /* skip partition '0', must write it once at
	             * the end */
	            part0 = val64;
	        } else {
	            xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	                 val64, tx_fifo_partitions[reg_num]);
	            xge_debug_fifo(XGE_TRACE,
	                "fifo partition_%d at: "
	                "0x"XGE_OS_LLXFMT" is: 0x"XGE_OS_LLXFMT,
	                reg_num, (unsigned long long)(ulong_t)
	                tx_fifo_partitions[reg_num],
	                (unsigned long long)val64);
	        }
	        val64 = 0;
	    }
	}

	part0 |= BIT(0); /* to enable the FIFO partition. */
	__hal_pio_mem_write32_lower(hldev->pdev, hldev->regh0, (u32)part0,
	                     tx_fifo_partitions[0]);
	xge_os_wmb();
	__hal_pio_mem_write32_upper(hldev->pdev, hldev->regh0, (u32)(part0>>32),
	                     tx_fifo_partitions[0]);
	xge_debug_fifo(XGE_TRACE, "fifo partition_0 at: "
	        "0x"XGE_OS_LLXFMT" is: 0x"XGE_OS_LLXFMT,
	        (unsigned long long)(ulong_t)
	            tx_fifo_partitions[0],
	        (unsigned long long) part0);

	/*
	 * Initialization of Tx_PA_CONFIG register to ignore packet
	 * integrity checking.
	 */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                            &bar0->tx_pa_cfg);
	val64 |= XGE_HAL_TX_PA_CFG_IGNORE_FRM_ERR |
	     XGE_HAL_TX_PA_CFG_IGNORE_SNAP_OUI |
	     XGE_HAL_TX_PA_CFG_IGNORE_LLC_CTRL |
	     XGE_HAL_TX_PA_CFG_IGNORE_L2_ERR;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                     &bar0->tx_pa_cfg);

	/*
	 * Assign MSI-X vectors
	 */
	for (i = 0; i < XGE_HAL_MAX_FIFO_NUM; i++) {
	    xge_list_t *item;
	    xge_hal_channel_t *channel = NULL;

	    if (!hldev->config.fifo.queue[i].configured ||
	        !hldev->config.fifo.queue[i].intr_vector ||
	        !hldev->config.intr_mode != XGE_HAL_INTR_MODE_MSIX)
	        continue;

	    /* find channel */
	    xge_list_for_each(item, &hldev->free_channels) {
	        xge_hal_channel_t *tmp;
	        tmp = xge_container_of(item, xge_hal_channel_t,
	                       item);
	        if (tmp->type == XGE_HAL_CHANNEL_TYPE_FIFO &&
	            tmp->post_qid == i) {
	            channel = tmp;
	            break;
	        }
	    }

	    if (channel) {
	        xge_hal_channel_msix_set(channel,
	            hldev->config.fifo.queue[i].intr_vector);
	    }
	}

	xge_debug_fifo(XGE_TRACE, "%s", "fifo channels initialized");
}

#ifdef XGE_HAL_ALIGN_XMIT
void
__hal_fifo_dtr_align_free_unmap(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh)
{
	    xge_hal_fifo_txdl_priv_t *txdl_priv;
	xge_hal_fifo_txd_t *txdp = (xge_hal_fifo_txd_t *)dtrh;
	xge_hal_fifo_t *fifo = (xge_hal_fifo_t *)channelh;

	txdl_priv = __hal_fifo_txdl_priv(txdp);

	if (txdl_priv->align_dma_addr != 0) {
	    xge_os_dma_unmap(fifo->channel.pdev,
	           txdl_priv->align_dma_handle,
	           txdl_priv->align_dma_addr,
	           fifo->align_size,
	           XGE_OS_DMA_DIR_TODEVICE);

	            txdl_priv->align_dma_addr = 0;
	}

	    if (txdl_priv->align_vaddr != NULL) {
	        xge_os_dma_free(fifo->channel.pdev,
	              txdl_priv->align_vaddr,
	              fifo->align_size,
	              &txdl_priv->align_dma_acch,
	              &txdl_priv->align_dma_handle);


	        txdl_priv->align_vaddr = NULL;
	    }
 }

xge_hal_status_e
__hal_fifo_dtr_align_alloc_map(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh)
{
	    xge_hal_fifo_txdl_priv_t *txdl_priv;
	xge_hal_fifo_txd_t *txdp = (xge_hal_fifo_txd_t *)dtrh;
	xge_hal_fifo_t *fifo = (xge_hal_fifo_t *)channelh;

	xge_assert(txdp);

	txdl_priv = __hal_fifo_txdl_priv(txdp);

	/* allocate alignment DMA-buffer */
	txdl_priv->align_vaddr = (char *)xge_os_dma_malloc(fifo->channel.pdev,
	            fifo->align_size,
	            XGE_OS_DMA_CACHELINE_ALIGNED |
	            XGE_OS_DMA_STREAMING,
	            &txdl_priv->align_dma_handle,
	            &txdl_priv->align_dma_acch);
	if (txdl_priv->align_vaddr == NULL) {
	    return XGE_HAL_ERR_OUT_OF_MEMORY;
	}

	/* map it */
	txdl_priv->align_dma_addr = xge_os_dma_map(fifo->channel.pdev,
	    txdl_priv->align_dma_handle, txdl_priv->align_vaddr,
	    fifo->align_size,
	    XGE_OS_DMA_DIR_TODEVICE, XGE_OS_DMA_STREAMING);

	if (txdl_priv->align_dma_addr == XGE_OS_INVALID_DMA_ADDR) {
	            __hal_fifo_dtr_align_free_unmap(channelh, dtrh);
	    return XGE_HAL_ERR_OUT_OF_MAPPING;
	}

	return XGE_HAL_OK;
}
#endif


