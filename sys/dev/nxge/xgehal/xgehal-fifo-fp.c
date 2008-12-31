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
 * $FreeBSD: src/sys/dev/nxge/xgehal/xgehal-fifo-fp.c,v 1.1.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifdef XGE_DEBUG_FP
#include <dev/nxge/include/xgehal-fifo.h>
#endif

__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_fifo_txdl_priv_t*
__hal_fifo_txdl_priv(xge_hal_dtr_h dtrh)
{
	xge_hal_fifo_txd_t *txdp = (xge_hal_fifo_txd_t*)dtrh;
	xge_hal_fifo_txdl_priv_t *txdl_priv;

	xge_assert(txdp);
	txdl_priv = (xge_hal_fifo_txdl_priv_t *)
	            (ulong_t)txdp->host_control;

	xge_assert(txdl_priv);
	xge_assert(txdl_priv->dma_object);
	xge_assert(txdl_priv->dma_addr);

	xge_assert(txdl_priv->dma_object->handle == txdl_priv->dma_handle);

	return txdl_priv;
}

__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
__hal_fifo_dtr_post_single(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	        u64 ctrl_1)
{
	xge_hal_fifo_t            *fifo    = (xge_hal_fifo_t *)channelh;
	xge_hal_fifo_hw_pair_t    *hw_pair = fifo->hw_pair;
	xge_hal_fifo_txd_t        *txdp    = (xge_hal_fifo_txd_t *)dtrh;
	xge_hal_fifo_txdl_priv_t  *txdl_priv;
	u64           ctrl;

	txdp->control_1 |= XGE_HAL_TXD_LIST_OWN_XENA;

#ifdef XGE_DEBUG_ASSERT
	    /* make sure Xena overwrites the (illegal) t_code value on completion */
	    XGE_HAL_SET_TXD_T_CODE(txdp->control_1, XGE_HAL_TXD_T_CODE_UNUSED_5);
#endif

	txdl_priv = __hal_fifo_txdl_priv(dtrh);

#if defined(XGE_OS_DMA_REQUIRES_SYNC) && defined(XGE_HAL_DMA_DTR_STREAMING)
	/* sync the TxDL to device */
	xge_os_dma_sync(fifo->channel.pdev,
	              txdl_priv->dma_handle,
	          txdl_priv->dma_addr,
	          txdl_priv->dma_offset,
	          txdl_priv->frags << 5 /* sizeof(xge_hal_fifo_txd_t) */,
	          XGE_OS_DMA_DIR_TODEVICE);
#endif
	/* write the pointer first */
	xge_os_pio_mem_write64(fifo->channel.pdev,
	             fifo->channel.regh1,
	                     txdl_priv->dma_addr,
	                     &hw_pair->txdl_pointer);

	/* spec: 0x00 = 1 TxD in the list */
	ctrl = XGE_HAL_TX_FIFO_LAST_TXD_NUM(txdl_priv->frags - 1);
	ctrl |= ctrl_1;
	ctrl |= fifo->no_snoop_bits;

	if (txdp->control_1 & XGE_HAL_TXD_LSO_COF_CTRL(XGE_HAL_TXD_TCP_LSO)) {
	    ctrl |= XGE_HAL_TX_FIFO_SPECIAL_FUNC;
	}

	/*
	 * according to the XENA spec:
	 *
	 * It is important to note that pointers and list control words are
	 * always written in pairs: in the first write, the host must write a
	 * pointer, and in the second write, it must write the list control
	 * word. Any other access will result in an error. Also, all 16 bytes
	 * of the pointer/control structure must be written, including any
	 * reserved bytes.
	 */
	xge_os_wmb();

	/*
	 * we want touch work_arr in order with ownership bit set to HW
	 */
	__hal_channel_dtr_post(channelh, dtrh);

	xge_os_pio_mem_write64(fifo->channel.pdev, fifo->channel.regh1,
	        ctrl, &hw_pair->list_control);

	xge_debug_fifo(XGE_TRACE, "posted txdl 0x"XGE_OS_LLXFMT" ctrl 0x"XGE_OS_LLXFMT" "
	    "into 0x"XGE_OS_LLXFMT"", (unsigned long long)txdl_priv->dma_addr,
	    (unsigned long long)ctrl,
	    (unsigned long long)(ulong_t)&hw_pair->txdl_pointer);

#ifdef XGE_HAL_FIFO_DUMP_TXD
	xge_os_printf(""XGE_OS_LLXFMT":"XGE_OS_LLXFMT":"XGE_OS_LLXFMT":"
	    XGE_OS_LLXFMT" dma "XGE_OS_LLXFMT,
	    txdp->control_1, txdp->control_2, txdp->buffer_pointer,
	    txdp->host_control, txdl_priv->dma_addr);
#endif

	fifo->channel.stats.total_posts++;
	fifo->channel.usage_cnt++;
	if (fifo->channel.stats.usage_max < fifo->channel.usage_cnt)
	    fifo->channel.stats.usage_max = fifo->channel.usage_cnt;
}

__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
__hal_fifo_txdl_free_many(xge_hal_channel_h channelh,
	          xge_hal_fifo_txd_t *txdp, int list_size, int frags)
{
	xge_hal_fifo_txdl_priv_t *current_txdl_priv;
	xge_hal_fifo_txdl_priv_t *next_txdl_priv;
	int invalid_frags = frags % list_size;
	if (invalid_frags){
	    xge_debug_fifo(XGE_ERR,
	        "freeing corrupt dtrh %p, fragments %d list size %d",
	        txdp, frags, list_size);
	    xge_assert(invalid_frags == 0);
	}
	while(txdp){
	    xge_debug_fifo(XGE_TRACE,
	        "freeing linked dtrh %p, fragments %d list size %d",
	        txdp, frags, list_size);
	    current_txdl_priv = __hal_fifo_txdl_priv(txdp);
#if defined(XGE_DEBUG_ASSERT) && defined(XGE_OS_MEMORY_CHECK)
	    current_txdl_priv->allocated = 0;
#endif
	    __hal_channel_dtr_free(channelh, txdp);
	    next_txdl_priv = current_txdl_priv->next_txdl_priv;
	    xge_assert(frags);
	    frags -= list_size;
	    if (next_txdl_priv) {
	        current_txdl_priv->next_txdl_priv = NULL;
	        txdp = next_txdl_priv->first_txdp;
	    }
	    else {
	        xge_debug_fifo(XGE_TRACE,
	        "freed linked dtrh fragments %d list size %d",
	        frags, list_size);
	        break;
	    }
	}
	xge_assert(frags == 0)
}

__HAL_STATIC_FIFO  __HAL_INLINE_FIFO void
__hal_fifo_txdl_restore_many(xge_hal_channel_h channelh,
	          xge_hal_fifo_txd_t *txdp, int txdl_count)
{
	xge_hal_fifo_txdl_priv_t *current_txdl_priv;
	xge_hal_fifo_txdl_priv_t *next_txdl_priv;
	int i = txdl_count;

	xge_assert(((xge_hal_channel_t *)channelh)->reserve_length +
	    txdl_count <= ((xge_hal_channel_t *)channelh)->reserve_initial);

	current_txdl_priv = __hal_fifo_txdl_priv(txdp);
	do{
	    xge_assert(i);
#if defined(XGE_DEBUG_ASSERT) && defined(XGE_OS_MEMORY_CHECK)
	    current_txdl_priv->allocated = 0;
#endif
	    next_txdl_priv = current_txdl_priv->next_txdl_priv;
	    txdp = current_txdl_priv->first_txdp;
	    current_txdl_priv->next_txdl_priv = NULL;
	    __hal_channel_dtr_restore(channelh, (xge_hal_dtr_h )txdp, --i);
	    xge_debug_fifo(XGE_TRACE,
	        "dtrh %p restored at offset %d", txdp, i);
	    current_txdl_priv = next_txdl_priv;
	} while(current_txdl_priv);
	__hal_channel_dtr_restore(channelh, NULL, txdl_count);
}
/**
 * xge_hal_fifo_dtr_private - Retrieve per-descriptor private data.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 *
 * Retrieve per-descriptor private data.
 * Note that ULD requests per-descriptor space via
 * xge_hal_channel_open().
 *
 * Returns: private ULD data associated with the descriptor.
 * Usage: See ex_xmit{} and ex_tx_compl{}.
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO void*
xge_hal_fifo_dtr_private(xge_hal_dtr_h dtrh)
{
	xge_hal_fifo_txd_t *txdp    = (xge_hal_fifo_txd_t *)dtrh;

	return ((char *)(ulong_t)txdp->host_control) +
	                sizeof(xge_hal_fifo_txdl_priv_t);
}

/**
 * xge_hal_fifo_dtr_buffer_cnt - Get number of buffers carried by the
 * descriptor.
 * @dtrh: Descriptor handle.
 *
 * Returns: Number of buffers stored in the given descriptor. Can be used
 * _after_ the descriptor is set up for posting (see
 * xge_hal_fifo_dtr_post()) and _before_ it is deallocated (see
 * xge_hal_fifo_dtr_free()).
 *
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO int
xge_hal_fifo_dtr_buffer_cnt(xge_hal_dtr_h dtrh)
{
	xge_hal_fifo_txdl_priv_t  *txdl_priv;

	txdl_priv = __hal_fifo_txdl_priv(dtrh);

	return txdl_priv->frags;
}
/**
 * xge_hal_fifo_dtr_reserve_many- Reserve fifo descriptors which span more
 *  than single txdl.
 * @channelh: Channel handle.
 * @dtrh: Reserved descriptor. On success HAL fills this "out" parameter
 *        with a valid handle.
 * @frags: minimum number of fragments to be reserved.
 *
 * Reserve TxDL(s) (that is, fifo descriptor)
 * for the subsequent filling-in by upper layerdriver (ULD))
 * and posting on the corresponding channel (@channelh)
 * via xge_hal_fifo_dtr_post().
 *
 * Returns: XGE_HAL_OK - success;
 * XGE_HAL_INF_OUT_OF_DESCRIPTORS - Currently no descriptors available
 *
 * See also: xge_hal_fifo_dtr_reserve_sp(), xge_hal_fifo_dtr_free(),
 * xge_hal_ring_dtr_reserve(), xge_hal_status_e{}.
 * Usage: See ex_xmit{}.
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_status_e
xge_hal_fifo_dtr_reserve_many(xge_hal_channel_h channelh,
	            xge_hal_dtr_h *dtrh, const int frags)
{
	xge_hal_status_e status = XGE_HAL_OK;
	int alloc_frags = 0, dang_frags = 0;
	xge_hal_fifo_txd_t *curr_txdp = NULL;
	xge_hal_fifo_txd_t *next_txdp;
	xge_hal_fifo_txdl_priv_t *next_txdl_priv, *curr_txdl_priv = NULL;
	xge_hal_fifo_t *fifo = (xge_hal_fifo_t *)channelh;
	int max_frags = fifo->config->max_frags;
	xge_hal_dtr_h dang_dtrh = NULL;
#if defined(XGE_HAL_TX_MULTI_RESERVE_IRQ)
	unsigned long flags=0;
#endif
	xge_debug_fifo(XGE_TRACE, "dtr_reserve_many called for frags %d",
	    frags);
	xge_assert(frags < (fifo->txdl_per_memblock * max_frags));
#if defined(XGE_HAL_TX_MULTI_RESERVE)
	xge_os_spin_lock(&fifo->channel.reserve_lock);
#elif defined(XGE_HAL_TX_MULTI_RESERVE_IRQ)
	xge_os_spin_lock_irq(&fifo->channel.reserve_lock, flags);
#endif
	while(alloc_frags < frags) {
	    status = __hal_channel_dtr_alloc(channelh,
	            (xge_hal_dtr_h *)(void*)&next_txdp);
	    if (status != XGE_HAL_OK){
	        xge_debug_fifo(XGE_ERR,
	            "failed to allocate linked fragments rc %d",
	             status);
	        xge_assert(status == XGE_HAL_INF_OUT_OF_DESCRIPTORS);
	        if (*dtrh) {
	            xge_assert(alloc_frags/max_frags);
	            __hal_fifo_txdl_restore_many(channelh,
	                (xge_hal_fifo_txd_t *) *dtrh, alloc_frags/max_frags);
	        }
	        if (dang_dtrh) {
	            xge_assert(dang_frags/max_frags);
	            __hal_fifo_txdl_restore_many(channelh,
	                (xge_hal_fifo_txd_t *) dang_dtrh, dang_frags/max_frags);
	        }
	        break;
	    }
	    xge_debug_fifo(XGE_TRACE, "allocated linked dtrh %p"
	        " for frags %d", next_txdp, frags);
	    next_txdl_priv = __hal_fifo_txdl_priv(next_txdp);
	    xge_assert(next_txdl_priv);
	    xge_assert(next_txdl_priv->first_txdp == next_txdp);
	    next_txdl_priv->dang_txdl = NULL;
	    next_txdl_priv->dang_frags = 0;
	    next_txdl_priv->next_txdl_priv = NULL;
#if defined(XGE_OS_MEMORY_CHECK)
	    next_txdl_priv->allocated = 1;
#endif
	    if (!curr_txdp || !curr_txdl_priv) {
	        curr_txdp = next_txdp;
	        curr_txdl_priv = next_txdl_priv;
	        *dtrh = (xge_hal_dtr_h)next_txdp;
	        alloc_frags = max_frags;
	        continue;
	    }
	    if (curr_txdl_priv->memblock ==
	        next_txdl_priv->memblock) {
	        xge_debug_fifo(XGE_TRACE,
	            "linking dtrh %p, with %p",
	            *dtrh, next_txdp);
	        xge_assert (next_txdp ==
	            curr_txdp + max_frags);
	        alloc_frags += max_frags;
	        curr_txdl_priv->next_txdl_priv = next_txdl_priv;
	    }
	    else {
	        xge_assert(*dtrh);
	        xge_assert(dang_dtrh == NULL);
	        dang_dtrh = *dtrh;
	        dang_frags = alloc_frags;
	        xge_debug_fifo(XGE_TRACE,
	            "dangling dtrh %p, linked with dtrh %p",
	            *dtrh, next_txdp);
	        next_txdl_priv->dang_txdl = (xge_hal_fifo_txd_t *) *dtrh;
	        next_txdl_priv->dang_frags = alloc_frags;
	        alloc_frags = max_frags;
	        *dtrh  = next_txdp;
	    }
	    curr_txdp = next_txdp;
	    curr_txdl_priv = next_txdl_priv;
	}

#if defined(XGE_HAL_TX_MULTI_RESERVE)
	xge_os_spin_unlock(&fifo->channel.reserve_lock);
#elif defined(XGE_HAL_TX_MULTI_RESERVE_IRQ)
	xge_os_spin_unlock_irq(&fifo->channel.reserve_lock, flags);
#endif

	if (status == XGE_HAL_OK) {
	    xge_hal_fifo_txdl_priv_t * txdl_priv;
	    xge_hal_fifo_txd_t *txdp = (xge_hal_fifo_txd_t *)*dtrh;
	    xge_hal_stats_channel_info_t *statsp = &fifo->channel.stats;
	    txdl_priv = __hal_fifo_txdl_priv(txdp);
	    /* reset the TxDL's private */
	    txdl_priv->align_dma_offset = 0;
	    txdl_priv->align_vaddr_start = txdl_priv->align_vaddr;
	    txdl_priv->align_used_frags = 0;
	    txdl_priv->frags = 0;
	    txdl_priv->bytes_sent = 0;
	    txdl_priv->alloc_frags = alloc_frags;
	    /* reset TxD0 */
	    txdp->control_1 = txdp->control_2 = 0;

#if defined(XGE_OS_MEMORY_CHECK)
	    txdl_priv->allocated = 1;
#endif
	    /* update statistics */
	    statsp->total_posts_dtrs_many++;
	    statsp->total_posts_frags_many += txdl_priv->alloc_frags;
	    if (txdl_priv->dang_frags){
	        statsp->total_posts_dang_dtrs++;
	        statsp->total_posts_dang_frags += txdl_priv->dang_frags;
	    }
	}

	return status;
}

/**
 * xge_hal_fifo_dtr_reserve - Reserve fifo descriptor.
 * @channelh: Channel handle.
 * @dtrh: Reserved descriptor. On success HAL fills this "out" parameter
 *        with a valid handle.
 *
 * Reserve a single TxDL (that is, fifo descriptor)
 * for the subsequent filling-in by upper layerdriver (ULD))
 * and posting on the corresponding channel (@channelh)
 * via xge_hal_fifo_dtr_post().
 *
 * Note: it is the responsibility of ULD to reserve multiple descriptors
 * for lengthy (e.g., LSO) transmit operation. A single fifo descriptor
 * carries up to configured number (fifo.max_frags) of contiguous buffers.
 *
 * Returns: XGE_HAL_OK - success;
 * XGE_HAL_INF_OUT_OF_DESCRIPTORS - Currently no descriptors available
 *
 * See also: xge_hal_fifo_dtr_reserve_sp(), xge_hal_fifo_dtr_free(),
 * xge_hal_ring_dtr_reserve(), xge_hal_status_e{}.
 * Usage: See ex_xmit{}.
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_status_e
xge_hal_fifo_dtr_reserve(xge_hal_channel_h channelh, xge_hal_dtr_h *dtrh)
{
	xge_hal_status_e status;
#if defined(XGE_HAL_TX_MULTI_RESERVE_IRQ)
	unsigned long flags=0;
#endif

#if defined(XGE_HAL_TX_MULTI_RESERVE)
	xge_os_spin_lock(&((xge_hal_channel_t*)channelh)->reserve_lock);
#elif defined(XGE_HAL_TX_MULTI_RESERVE_IRQ)
	xge_os_spin_lock_irq(&((xge_hal_channel_t*)channelh)->reserve_lock,
	                     flags);
#endif

	status = __hal_channel_dtr_alloc(channelh, dtrh);

#if defined(XGE_HAL_TX_MULTI_RESERVE)
	xge_os_spin_unlock(&((xge_hal_channel_t*)channelh)->reserve_lock);
#elif defined(XGE_HAL_TX_MULTI_RESERVE_IRQ)
	xge_os_spin_unlock_irq(&((xge_hal_channel_t*)channelh)->reserve_lock,
	                       flags);
#endif

	if (status == XGE_HAL_OK) {
	    xge_hal_fifo_txd_t *txdp = (xge_hal_fifo_txd_t *)*dtrh;
	    xge_hal_fifo_txdl_priv_t *txdl_priv;

	    txdl_priv = __hal_fifo_txdl_priv(txdp);

	    /* reset the TxDL's private */
	    txdl_priv->align_dma_offset = 0;
	    txdl_priv->align_vaddr_start = txdl_priv->align_vaddr;
	    txdl_priv->align_used_frags = 0;
	    txdl_priv->frags = 0;
	    txdl_priv->alloc_frags =
	        ((xge_hal_fifo_t *)channelh)->config->max_frags;
	    txdl_priv->dang_txdl = NULL;
	    txdl_priv->dang_frags = 0;
	    txdl_priv->next_txdl_priv = NULL;
	    txdl_priv->bytes_sent = 0;

	    /* reset TxD0 */
	    txdp->control_1 = txdp->control_2 = 0;

#if defined(XGE_OS_MEMORY_CHECK)
	    txdl_priv->allocated = 1;
#endif
	}

	return status;
}

/**
 * xge_hal_fifo_dtr_reserve_sp - Reserve fifo descriptor and store it in
 * the ULD-provided "scratch" memory.
 * @channelh: Channel handle.
 * @dtr_sp_size: Size of the %dtr_sp "scratch pad" that HAL can use for TxDL.
 * @dtr_sp: "Scratch pad" supplied by upper-layer driver (ULD).
 *
 * Reserve TxDL and fill-in ULD supplied "scratch pad". The difference
 * between this API and xge_hal_fifo_dtr_reserve() is (possibly) -
 * performance.
 *
 * If upper-layer uses ULP-defined commands, and if those commands have enough
 * space for HAL/Xframe descriptors - tnan it is better (read: faster) to fit
 * all the per-command information into one command, which is typically
 * one contiguous block.
 *
 * Note: Unlike xge_hal_fifo_dtr_reserve(), this function can be used to
 * allocate a single descriptor for transmit operation.
 *
 * See also: xge_hal_fifo_dtr_reserve(), xge_hal_fifo_dtr_free(),
 * xge_hal_ring_dtr_reserve(), xge_hal_status_e{}.
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_status_e
xge_hal_fifo_dtr_reserve_sp(xge_hal_channel_h channelh, int dtr_sp_size,
	        xge_hal_dtr_h dtr_sp)
{
	/* FIXME: implement */
	return XGE_HAL_OK;
}

/**
 * xge_hal_fifo_dtr_post - Post descriptor on the fifo channel.
 * @channelh: Channel handle.
 * @dtrh: Descriptor obtained via xge_hal_fifo_dtr_reserve() or
 * xge_hal_fifo_dtr_reserve_sp()
 * @frags: Number of contiguous buffers that are part of a single
 *         transmit operation.
 *
 * Post descriptor on the 'fifo' type channel for transmission.
 * Prior to posting the descriptor should be filled in accordance with
 * Host/Xframe interface specification for a given service (LL, etc.).
 *
 * See also: xge_hal_fifo_dtr_post_many(), xge_hal_ring_dtr_post().
 * Usage: See ex_xmit{}.
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_post(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh)
{
	xge_hal_fifo_t *fifo = (xge_hal_fifo_t *)channelh;
	xge_hal_fifo_txdl_priv_t *txdl_priv;
	xge_hal_fifo_txd_t *txdp_last;
	xge_hal_fifo_txd_t *txdp_first;
#if defined(XGE_HAL_TX_MULTI_POST_IRQ)
	unsigned long flags = 0;
#endif

	txdl_priv = __hal_fifo_txdl_priv(dtrh);

	txdp_first = (xge_hal_fifo_txd_t *)dtrh;
	txdp_first->control_1 |= XGE_HAL_TXD_GATHER_CODE_FIRST;
	txdp_first->control_2 |= fifo->interrupt_type;

	txdp_last = (xge_hal_fifo_txd_t *)dtrh + (txdl_priv->frags - 1);
	txdp_last->control_1 |= XGE_HAL_TXD_GATHER_CODE_LAST;

#if defined(XGE_HAL_TX_MULTI_POST)
	xge_os_spin_lock(fifo->post_lock_ptr);
#elif defined(XGE_HAL_TX_MULTI_POST_IRQ)
	xge_os_spin_lock_irq(fifo->post_lock_ptr, flags);
#endif

	__hal_fifo_dtr_post_single(channelh, dtrh,
	     (u64)(XGE_HAL_TX_FIFO_FIRST_LIST | XGE_HAL_TX_FIFO_LAST_LIST));

#if defined(XGE_HAL_TX_MULTI_POST)
	xge_os_spin_unlock(fifo->post_lock_ptr);
#elif defined(XGE_HAL_TX_MULTI_POST_IRQ)
	xge_os_spin_unlock_irq(fifo->post_lock_ptr, flags);
#endif
}

/**
 * xge_hal_fifo_dtr_post_many - Post multiple descriptors on fifo
 * channel.
 * @channelh: Channel to post descriptor.
 * @num: Number of descriptors (i.e., fifo TxDLs) in the %dtrs[].
 * @dtrs: Descriptors obtained via xge_hal_fifo_dtr_reserve().
 * @frags_arr: Number of fragments carried @dtrs descriptors.
 * Note that frag_arr[i] corresponds to descriptor dtrs[i].
 *
 * Post multi-descriptor on the fifo channel. The operation is atomic:
 * all descriptrs are posted on the channel "back-to-back' without
 * letting other posts (possibly driven by multiple transmitting threads)
 * to interleave.
 *
 * See also: xge_hal_fifo_dtr_post(), xge_hal_ring_dtr_post().
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_post_many(xge_hal_channel_h channelh, int num,
	        xge_hal_dtr_h dtrs[])
{
	int i;
	xge_hal_fifo_t *fifo = (xge_hal_fifo_t *)channelh;
	xge_hal_fifo_txd_t *txdp_last;
	xge_hal_fifo_txd_t *txdp_first;
	xge_hal_fifo_txdl_priv_t *txdl_priv_last;
#if defined(XGE_HAL_TX_MULTI_POST_IRQ)
	unsigned long flags = 0;
#endif

	xge_assert(num > 1);

	txdp_first = (xge_hal_fifo_txd_t *)dtrs[0];
	txdp_first->control_1 |= XGE_HAL_TXD_GATHER_CODE_FIRST;
	txdp_first->control_2 |= fifo->interrupt_type;

	txdl_priv_last = __hal_fifo_txdl_priv(dtrs[num-1]);
	txdp_last = (xge_hal_fifo_txd_t *)dtrs[num-1] +
	                (txdl_priv_last->frags - 1);
	txdp_last->control_1 |= XGE_HAL_TXD_GATHER_CODE_LAST;

#if defined(XGE_HAL_TX_MULTI_POST)
	xge_os_spin_lock(&((xge_hal_channel_t*)channelh)->post_lock);
#elif defined(XGE_HAL_TX_MULTI_POST_IRQ)
	xge_os_spin_lock_irq(&((xge_hal_channel_t*)channelh)->post_lock,
	flags);
#endif

	for (i=0; i<num; i++) {
	    xge_hal_fifo_txdl_priv_t *txdl_priv;
	    u64 val64;
	    xge_hal_dtr_h dtrh = dtrs[i];

	    txdl_priv = __hal_fifo_txdl_priv(dtrh);
	    txdl_priv = txdl_priv; /* Cheat lint */

	    val64 = 0;
	    if (i == 0) {
	         val64 |= XGE_HAL_TX_FIFO_FIRST_LIST;
	    } else if (i == num -1) {
	         val64 |= XGE_HAL_TX_FIFO_LAST_LIST;
	    }

	    val64 |= XGE_HAL_TX_FIFO_SPECIAL_FUNC;
	    __hal_fifo_dtr_post_single(channelh, dtrh, val64);
	}

#if defined(XGE_HAL_TX_MULTI_POST)
	xge_os_spin_unlock(&((xge_hal_channel_t*)channelh)->post_lock);
#elif defined(XGE_HAL_TX_MULTI_POST_IRQ)
	xge_os_spin_unlock_irq(&((xge_hal_channel_t*)channelh)->post_lock,
	flags);
#endif

	fifo->channel.stats.total_posts_many++;
}

/**
 * xge_hal_fifo_dtr_next_completed - Retrieve next completed descriptor.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle. Returned by HAL.
 * @t_code: Transfer code, as per Xframe User Guide,
 *          Transmit Descriptor Format.
 *          Returned by HAL.
 *
 * Retrieve the _next_ completed descriptor.
 * HAL uses channel callback (*xge_hal_channel_callback_f) to notifiy
 * upper-layer driver (ULD) of new completed descriptors. After that
 * the ULD can use xge_hal_fifo_dtr_next_completed to retrieve the rest
 * completions (the very first completion is passed by HAL via
 * xge_hal_channel_callback_f).
 *
 * Implementation-wise, the upper-layer driver is free to call
 * xge_hal_fifo_dtr_next_completed either immediately from inside the
 * channel callback, or in a deferred fashion and separate (from HAL)
 * context.
 *
 * Non-zero @t_code means failure to process the descriptor.
 * The failure could happen, for instance, when the link is
 * down, in which case Xframe completes the descriptor because it
 * is not able to send the data out.
 *
 * For details please refer to Xframe User Guide.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS - No completed descriptors
 * are currently available for processing.
 *
 * See also: xge_hal_channel_callback_f{},
 * xge_hal_ring_dtr_next_completed().
 * Usage: See ex_tx_compl{}.
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_status_e
xge_hal_fifo_dtr_next_completed(xge_hal_channel_h channelh,
	        xge_hal_dtr_h *dtrh, u8 *t_code)
{
	xge_hal_fifo_txd_t        *txdp;
	xge_hal_fifo_t            *fifo    = (xge_hal_fifo_t *)channelh;
#if defined(XGE_OS_DMA_REQUIRES_SYNC) && defined(XGE_HAL_DMA_DTR_STREAMING)
	xge_hal_fifo_txdl_priv_t  *txdl_priv;
#endif

	__hal_channel_dtr_try_complete(channelh, dtrh);
	txdp = (xge_hal_fifo_txd_t *)*dtrh;
	if (txdp == NULL) {
	    return XGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS;
	}

#if defined(XGE_OS_DMA_REQUIRES_SYNC) && defined(XGE_HAL_DMA_DTR_STREAMING)
	txdl_priv = __hal_fifo_txdl_priv(txdp);

	/* sync TxDL to read the ownership
	 *
	 * Note: 16bytes means Control_1 & Control_2 */
	xge_os_dma_sync(fifo->channel.pdev,
	              txdl_priv->dma_handle,
	          txdl_priv->dma_addr,
	          txdl_priv->dma_offset,
	          16,
	          XGE_OS_DMA_DIR_FROMDEVICE);
#endif

	/* check whether host owns it */
	if ( !(txdp->control_1 & XGE_HAL_TXD_LIST_OWN_XENA) ) {

	    xge_assert(txdp->host_control!=0);

	    __hal_channel_dtr_complete(channelh);

	    *t_code = (u8)XGE_HAL_GET_TXD_T_CODE(txdp->control_1);

	            /* see XGE_HAL_SET_TXD_T_CODE() above.. */
	            xge_assert(*t_code != XGE_HAL_TXD_T_CODE_UNUSED_5);

	    if (fifo->channel.usage_cnt > 0)
	        fifo->channel.usage_cnt--;

	    return XGE_HAL_OK;
	}

	/* no more completions */
	*dtrh = 0;
	return XGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS;
}

/**
 * xge_hal_fifo_dtr_free - Free descriptor.
 * @channelh: Channel handle.
 * @dtr: Descriptor handle.
 *
 * Free the reserved descriptor. This operation is "symmetrical" to
 * xge_hal_fifo_dtr_reserve or xge_hal_fifo_dtr_reserve_sp.
 * The "free-ing" completes the descriptor's lifecycle.
 *
 * After free-ing (see xge_hal_fifo_dtr_free()) the descriptor again can
 * be:
 *
 * - reserved (xge_hal_fifo_dtr_reserve);
 *
 * - posted (xge_hal_fifo_dtr_post);
 *
 * - completed (xge_hal_fifo_dtr_next_completed);
 *
 * - and recycled again (xge_hal_fifo_dtr_free).
 *
 * For alternative state transitions and more details please refer to
 * the design doc.
 *
 * See also: xge_hal_ring_dtr_free(), xge_hal_fifo_dtr_reserve().
 * Usage: See ex_tx_compl{}.
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_free(xge_hal_channel_h channelh, xge_hal_dtr_h dtr)
{
#if defined(XGE_HAL_TX_MULTI_FREE_IRQ)
	unsigned long flags = 0;
#endif
	xge_hal_fifo_txdl_priv_t *txdl_priv = __hal_fifo_txdl_priv(
	                (xge_hal_fifo_txd_t *)dtr);
	int max_frags = ((xge_hal_fifo_t *)channelh)->config->max_frags;
#if defined(XGE_HAL_TX_MULTI_FREE)
	xge_os_spin_lock(&((xge_hal_channel_t*)channelh)->free_lock);
#elif defined(XGE_HAL_TX_MULTI_FREE_IRQ)
	xge_os_spin_lock_irq(&((xge_hal_channel_t*)channelh)->free_lock,
	flags);
#endif

	if (txdl_priv->alloc_frags > max_frags) {
	    xge_hal_fifo_txd_t *dang_txdp = (xge_hal_fifo_txd_t *)
	                    txdl_priv->dang_txdl;
	    int dang_frags = txdl_priv->dang_frags;
	    int alloc_frags = txdl_priv->alloc_frags;
	    txdl_priv->dang_txdl = NULL;
	    txdl_priv->dang_frags = 0;
	    txdl_priv->alloc_frags = 0;
	    /* dtrh must have a linked list of dtrh */
	    xge_assert(txdl_priv->next_txdl_priv);

	    /* free any dangling dtrh first */
	    if (dang_txdp) {
	        xge_debug_fifo(XGE_TRACE,
	            "freeing dangled dtrh %p for %d fragments",
	            dang_txdp, dang_frags);
	        __hal_fifo_txdl_free_many(channelh, dang_txdp,
	            max_frags, dang_frags);
	    }

	    /* now free the reserved dtrh list */
	    xge_debug_fifo(XGE_TRACE,
	            "freeing dtrh %p list of %d fragments", dtr,
	            alloc_frags);
	    __hal_fifo_txdl_free_many(channelh,
	            (xge_hal_fifo_txd_t *)dtr, max_frags,
	            alloc_frags);
	}
	else
	    __hal_channel_dtr_free(channelh, dtr);

	((xge_hal_channel_t *)channelh)->poll_bytes += txdl_priv->bytes_sent;

#if defined(XGE_DEBUG_ASSERT) && defined(XGE_OS_MEMORY_CHECK)
	__hal_fifo_txdl_priv(dtr)->allocated = 0;
#endif

#if defined(XGE_HAL_TX_MULTI_FREE)
	xge_os_spin_unlock(&((xge_hal_channel_t*)channelh)->free_lock);
#elif defined(XGE_HAL_TX_MULTI_FREE_IRQ)
	xge_os_spin_unlock_irq(&((xge_hal_channel_t*)channelh)->free_lock,
	flags);
#endif
}


/**
 * xge_hal_fifo_dtr_buffer_set_aligned - Align transmit buffer and fill
 * in fifo descriptor.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 * @frag_idx: Index of the data buffer in the caller's scatter-gather listá
 *            (of buffers).
 * @vaddr: Virtual address of the data buffer.
 * @dma_pointer: DMA address of the data buffer referenced by @frag_idx.
 * @size: Size of the data buffer (in bytes).
 * @misaligned_size: Size (in bytes) of the misaligned portion of the
 * data buffer. Calculated by the caller, based on the platform/OS/other
 * specific criteria, which is outside of HAL's domain. See notes below.
 *
 * This API is part of the transmit descriptor preparation for posting
 * (via xge_hal_fifo_dtr_post()). The related "preparation" APIs include
 * xge_hal_fifo_dtr_mss_set() and xge_hal_fifo_dtr_cksum_set_bits().
 * All three APIs fill in the fields of the fifo descriptor,
 * in accordance with the Xframe specification.
 * On the PCI-X based systems aligning transmit data typically provides better
 * transmit performance. The typical alignment granularity: L2 cacheline size.
 * However, HAL does not make assumptions in terms of the alignment granularity;
 * this is specified via additional @misaligned_size parameter described above.
 * Prior to calling xge_hal_fifo_dtr_buffer_set_aligned(),
 * ULD is supposed to check alignment of a given fragment/buffer. For this HAL
 * provides a separate xge_hal_check_alignment() API sufficient to cover
 * most (but not all) possible alignment criteria.
 * If the buffer appears to be aligned, the ULD calls
 * xge_hal_fifo_dtr_buffer_set().
 * Otherwise, ULD calls xge_hal_fifo_dtr_buffer_set_aligned().
 *
 * Note; This API is a "superset" of xge_hal_fifo_dtr_buffer_set(). In
 * addition to filling in the specified descriptor it aligns transmit data on
 * the specified boundary.
 * Note: Decision on whether to align or not to align a given contiguous
 * transmit buffer is outside of HAL's domain. To this end ULD can use any
 * programmable criteria, which can help to 1) boost transmit performance,
 * and/or 2) provide a workaround for PCI bridge bugs, if any.
 *
 * See also: xge_hal_fifo_dtr_buffer_set(),
 * xge_hal_check_alignment().
 *
 * See also: xge_hal_fifo_dtr_reserve(), xge_hal_fifo_dtr_post(),
 * xge_hal_fifo_dtr_mss_set(), xge_hal_fifo_dtr_cksum_set_bits()
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_status_e
xge_hal_fifo_dtr_buffer_set_aligned(xge_hal_channel_h channelh,
	        xge_hal_dtr_h dtrh, int frag_idx, void *vaddr,
	        dma_addr_t dma_pointer, int size, int misaligned_size)
{
	xge_hal_fifo_t *fifo = (xge_hal_fifo_t *)channelh;
	xge_hal_fifo_txdl_priv_t *txdl_priv;
	xge_hal_fifo_txd_t *txdp;
	int remaining_size;
	ptrdiff_t prev_boff;

	txdl_priv = __hal_fifo_txdl_priv(dtrh);
	txdp = (xge_hal_fifo_txd_t *)dtrh + txdl_priv->frags;

	if (frag_idx != 0) {
	    txdp->control_1 = txdp->control_2 = 0;
	}

	/* On some systems buffer size could be zero.
	 * It is the responsibility of ULD and *not HAL* to
	 * detect it and skip it. */
	xge_assert(size > 0);
	xge_assert(frag_idx < txdl_priv->alloc_frags);
	xge_assert(misaligned_size != 0 &&
	        misaligned_size <= fifo->config->alignment_size);

	remaining_size = size - misaligned_size;
	xge_assert(remaining_size >= 0);

	xge_os_memcpy((char*)txdl_priv->align_vaddr_start,
	                  vaddr, misaligned_size);

	    if (txdl_priv->align_used_frags >= fifo->config->max_aligned_frags) {
	        return XGE_HAL_ERR_OUT_ALIGNED_FRAGS;
	    }

	/* setup new buffer */
	prev_boff = txdl_priv->align_vaddr_start - txdl_priv->align_vaddr;
	txdp->buffer_pointer = (u64)txdl_priv->align_dma_addr + prev_boff;
	txdp->control_1 |= XGE_HAL_TXD_BUFFER0_SIZE(misaligned_size);
	txdl_priv->bytes_sent += misaligned_size;
	fifo->channel.stats.total_buffers++;
	txdl_priv->frags++;
	txdl_priv->align_used_frags++;
	txdl_priv->align_vaddr_start += fifo->config->alignment_size;
	    txdl_priv->align_dma_offset = 0;

#if defined(XGE_OS_DMA_REQUIRES_SYNC)
	/* sync new buffer */
	xge_os_dma_sync(fifo->channel.pdev,
	          txdl_priv->align_dma_handle,
	          txdp->buffer_pointer,
	          0,
	          misaligned_size,
	          XGE_OS_DMA_DIR_TODEVICE);
#endif

	if (remaining_size) {
	    xge_assert(frag_idx < txdl_priv->alloc_frags);
	    txdp++;
	    txdp->buffer_pointer = (u64)dma_pointer +
	                misaligned_size;
	    txdp->control_1 =
	        XGE_HAL_TXD_BUFFER0_SIZE(remaining_size);
	    txdl_priv->bytes_sent += remaining_size;
	    txdp->control_2 = 0;
	    fifo->channel.stats.total_buffers++;
	    txdl_priv->frags++;
	}

	return XGE_HAL_OK;
}

/**
 * xge_hal_fifo_dtr_buffer_append - Append the contents of virtually
 * contiguous data buffer to a single physically contiguous buffer.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 * @vaddr: Virtual address of the data buffer.
 * @size: Size of the data buffer (in bytes).
 *
 * This API is part of the transmit descriptor preparation for posting
 * (via xge_hal_fifo_dtr_post()).
 * The main difference of this API wrt to the APIs
 * xge_hal_fifo_dtr_buffer_set_aligned() is that this API appends the
 * contents of virtually contiguous data buffers received from
 * upper layer into a single physically contiguous data buffer and the
 * device will do a DMA from this buffer.
 *
 * See Also: xge_hal_fifo_dtr_buffer_finalize(), xge_hal_fifo_dtr_buffer_set(),
 * xge_hal_fifo_dtr_buffer_set_aligned().
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_status_e
xge_hal_fifo_dtr_buffer_append(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	    void *vaddr, int size)
{
	xge_hal_fifo_t *fifo = (xge_hal_fifo_t *)channelh;
	xge_hal_fifo_txdl_priv_t *txdl_priv;
	ptrdiff_t used;

	xge_assert(size > 0);

	txdl_priv = __hal_fifo_txdl_priv(dtrh);

	used = txdl_priv->align_vaddr_start - txdl_priv->align_vaddr;
	used += txdl_priv->align_dma_offset;
	if (used + (unsigned int)size > (unsigned int)fifo->align_size)
	        return XGE_HAL_ERR_OUT_ALIGNED_FRAGS;

	xge_os_memcpy((char*)txdl_priv->align_vaddr_start +
	    txdl_priv->align_dma_offset, vaddr, size);

	fifo->channel.stats.copied_frags++;

	txdl_priv->align_dma_offset += size;
	return XGE_HAL_OK;
}

/**
 * xge_hal_fifo_dtr_buffer_finalize - Prepares a descriptor that contains the
 * single physically contiguous buffer.
 *
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 * @frag_idx: Index of the data buffer in the Txdl list.
 *
 * This API in conjuction with xge_hal_fifo_dtr_buffer_append() prepares
 * a descriptor that consists of a single physically contiguous buffer
 * which inturn contains the contents of one or more virtually contiguous
 * buffers received from the upper layer.
 *
 * See Also: xge_hal_fifo_dtr_buffer_append().
*/
__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_buffer_finalize(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	    int frag_idx)
{
	xge_hal_fifo_t *fifo = (xge_hal_fifo_t *)channelh;
	xge_hal_fifo_txdl_priv_t *txdl_priv;
	xge_hal_fifo_txd_t *txdp;
	ptrdiff_t prev_boff;

	xge_assert(frag_idx < fifo->config->max_frags);

	txdl_priv = __hal_fifo_txdl_priv(dtrh);
	txdp = (xge_hal_fifo_txd_t *)dtrh + txdl_priv->frags;

	if (frag_idx != 0) {
	    txdp->control_1 = txdp->control_2 = 0;
	}

	prev_boff = txdl_priv->align_vaddr_start - txdl_priv->align_vaddr;
	txdp->buffer_pointer = (u64)txdl_priv->align_dma_addr + prev_boff;
	txdp->control_1 |=
	            XGE_HAL_TXD_BUFFER0_SIZE(txdl_priv->align_dma_offset);
	txdl_priv->bytes_sent += (unsigned int)txdl_priv->align_dma_offset;
	fifo->channel.stats.total_buffers++;
	fifo->channel.stats.copied_buffers++;
	txdl_priv->frags++;
	txdl_priv->align_used_frags++;

#if defined(XGE_OS_DMA_REQUIRES_SYNC)
	/* sync pre-mapped buffer */
	xge_os_dma_sync(fifo->channel.pdev,
	          txdl_priv->align_dma_handle,
	          txdp->buffer_pointer,
	          0,
	          txdl_priv->align_dma_offset,
	          XGE_OS_DMA_DIR_TODEVICE);
#endif

	/* increment vaddr_start for the next buffer_append() iteration */
	txdl_priv->align_vaddr_start += txdl_priv->align_dma_offset;
	    txdl_priv->align_dma_offset = 0;
}

/**
 * xge_hal_fifo_dtr_buffer_set - Set transmit buffer pointer in the
 * descriptor.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 * @frag_idx: Index of the data buffer in the caller's scatter-gather listá
 *            (of buffers).
 * @dma_pointer: DMA address of the data buffer referenced by @frag_idx.
 * @size: Size of the data buffer (in bytes).
 *
 * This API is part of the preparation of the transmit descriptor for posting
 * (via xge_hal_fifo_dtr_post()). The related "preparation" APIs include
 * xge_hal_fifo_dtr_mss_set() and xge_hal_fifo_dtr_cksum_set_bits().
 * All three APIs fill in the fields of the fifo descriptor,
 * in accordance with the Xframe specification.
 *
 * See also: xge_hal_fifo_dtr_buffer_set_aligned(),
 * xge_hal_check_alignment().
 *
 * See also: xge_hal_fifo_dtr_reserve(), xge_hal_fifo_dtr_post(),
 * xge_hal_fifo_dtr_mss_set(), xge_hal_fifo_dtr_cksum_set_bits()
 * Prepare transmit descriptor for transmission (via
 * xge_hal_fifo_dtr_post()).
 * See also: xge_hal_fifo_dtr_vlan_set().
 * Note: Compare with xge_hal_fifo_dtr_buffer_set_aligned().
 *
 * Usage: See ex_xmit{}.
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_buffer_set(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	    int frag_idx, dma_addr_t dma_pointer, int size)
{
	xge_hal_fifo_t *fifo = (xge_hal_fifo_t *)channelh;
	xge_hal_fifo_txdl_priv_t *txdl_priv;
	xge_hal_fifo_txd_t *txdp;

	txdl_priv = __hal_fifo_txdl_priv(dtrh);
	txdp = (xge_hal_fifo_txd_t *)dtrh + txdl_priv->frags;

	if (frag_idx != 0) {
	    txdp->control_1 = txdp->control_2 = 0;
	}

	/* Note:
	 * it is the responsibility of upper layers and not HAL
	 * detect it and skip zero-size fragment
	 */
	xge_assert(size > 0);
	xge_assert(frag_idx < txdl_priv->alloc_frags);

	txdp->buffer_pointer = (u64)dma_pointer;
	txdp->control_1 |= XGE_HAL_TXD_BUFFER0_SIZE(size);
	txdl_priv->bytes_sent += size;
	fifo->channel.stats.total_buffers++;
	txdl_priv->frags++;
}

/**
 * xge_hal_fifo_dtr_mss_set - Set MSS.
 * @dtrh: Descriptor handle.
 * @mss: MSS size for _this_ TCP connection. Passed by TCP stack down to the
 *       ULD, which in turn inserts the MSS into the @dtrh.
 *
 * This API is part of the preparation of the transmit descriptor for posting
 * (via xge_hal_fifo_dtr_post()). The related "preparation" APIs include
 * xge_hal_fifo_dtr_buffer_set(), xge_hal_fifo_dtr_buffer_set_aligned(),
 * and xge_hal_fifo_dtr_cksum_set_bits().
 * All these APIs fill in the fields of the fifo descriptor,
 * in accordance with the Xframe specification.
 *
 * See also: xge_hal_fifo_dtr_reserve(),
 * xge_hal_fifo_dtr_post(), xge_hal_fifo_dtr_vlan_set().
 * Usage: See ex_xmit{}.
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_mss_set(xge_hal_dtr_h dtrh, int mss)
{
	xge_hal_fifo_txd_t *txdp = (xge_hal_fifo_txd_t *)dtrh;

	txdp->control_1 |= XGE_HAL_TXD_LSO_COF_CTRL(XGE_HAL_TXD_TCP_LSO);
	txdp->control_1 |= XGE_HAL_TXD_TCP_LSO_MSS(mss);
}

/**
 * xge_hal_fifo_dtr_cksum_set_bits - Offload checksum.
 * @dtrh: Descriptor handle.
 * @cksum_bits: Specifies which checksums are to be offloaded: IPv4,
 *              and/or TCP and/or UDP.
 *
 * Ask Xframe to calculate IPv4 & transport checksums for _this_ transmit
 * descriptor.
 * This API is part of the preparation of the transmit descriptor for posting
 * (via xge_hal_fifo_dtr_post()). The related "preparation" APIs include
 * xge_hal_fifo_dtr_mss_set(), xge_hal_fifo_dtr_buffer_set_aligned(),
 * and xge_hal_fifo_dtr_buffer_set().
 * All these APIs fill in the fields of the fifo descriptor,
 * in accordance with the Xframe specification.
 *
 * See also: xge_hal_fifo_dtr_reserve(),
 * xge_hal_fifo_dtr_post(), XGE_HAL_TXD_TX_CKO_IPV4_EN,
 * XGE_HAL_TXD_TX_CKO_TCP_EN.
 * Usage: See ex_xmit{}.
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_cksum_set_bits(xge_hal_dtr_h dtrh, u64 cksum_bits)
{
	xge_hal_fifo_txd_t *txdp = (xge_hal_fifo_txd_t *)dtrh;

	txdp->control_2 |= cksum_bits;
}


/**
 * xge_hal_fifo_dtr_vlan_set - Set VLAN tag.
 * @dtrh: Descriptor handle.
 * @vlan_tag: 16bit VLAN tag.
 *
 * Insert VLAN tag into specified transmit descriptor.
 * The actual insertion of the tag into outgoing frame is done by the hardware.
 * See also: xge_hal_fifo_dtr_buffer_set(), xge_hal_fifo_dtr_mss_set().
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO void
xge_hal_fifo_dtr_vlan_set(xge_hal_dtr_h dtrh, u16 vlan_tag)
{
	xge_hal_fifo_txd_t *txdp = (xge_hal_fifo_txd_t *)dtrh;

	txdp->control_2 |= XGE_HAL_TXD_VLAN_ENABLE;
	txdp->control_2 |= XGE_HAL_TXD_VLAN_TAG(vlan_tag);
}

/**
 * xge_hal_fifo_is_next_dtr_completed - Checks if the next dtr is completed
 * @channelh: Channel handle.
 */
__HAL_STATIC_FIFO __HAL_INLINE_FIFO xge_hal_status_e
xge_hal_fifo_is_next_dtr_completed(xge_hal_channel_h channelh)
{
	xge_hal_fifo_txd_t *txdp;
	xge_hal_dtr_h dtrh;

	__hal_channel_dtr_try_complete(channelh, &dtrh);
	txdp = (xge_hal_fifo_txd_t *)dtrh;
	if (txdp == NULL) {
	    return XGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS;
	}

	/* check whether host owns it */
	if ( !(txdp->control_1 & XGE_HAL_TXD_LIST_OWN_XENA) ) {
	    xge_assert(txdp->host_control!=0);
	    return XGE_HAL_OK;
	}

	/* no more completions */
	return XGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS;
}
