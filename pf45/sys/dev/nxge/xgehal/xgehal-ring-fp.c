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
 * $FreeBSD$
 */

#ifdef XGE_DEBUG_FP
#include <dev/nxge/include/xgehal-ring.h>
#endif

__HAL_STATIC_RING __HAL_INLINE_RING xge_hal_ring_rxd_priv_t*
__hal_ring_rxd_priv(xge_hal_ring_t *ring, xge_hal_dtr_h dtrh)
{

	xge_hal_ring_rxd_1_t *rxdp = (xge_hal_ring_rxd_1_t *)dtrh;
	xge_hal_ring_rxd_priv_t *rxd_priv;

	xge_assert(rxdp);

#if defined(XGE_HAL_USE_5B_MODE)
	xge_assert(ring);
	if (ring->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_5) {
	    xge_hal_ring_rxd_5_t *rxdp_5 = (xge_hal_ring_rxd_5_t *)dtrh;
#if defined (XGE_OS_PLATFORM_64BIT)
	    int memblock_idx = rxdp_5->host_control >> 16;
	    int i = rxdp_5->host_control & 0xFFFF;
	    rxd_priv = (xge_hal_ring_rxd_priv_t *)
	        ((char*)ring->mempool->memblocks_priv_arr[memblock_idx] + ring->rxd_priv_size * i);
#else
	    /* 32-bit case */
	    rxd_priv = (xge_hal_ring_rxd_priv_t *)rxdp_5->host_control;
#endif
	} else
#endif
	{
	    rxd_priv = (xge_hal_ring_rxd_priv_t *)
	            (ulong_t)rxdp->host_control;
	}

	xge_assert(rxd_priv);
	xge_assert(rxd_priv->dma_object);

	xge_assert(rxd_priv->dma_object->handle == rxd_priv->dma_handle);

	xge_assert(rxd_priv->dma_object->addr + rxd_priv->dma_offset ==
	                        rxd_priv->dma_addr);

	return rxd_priv;
}

__HAL_STATIC_RING __HAL_INLINE_RING int
__hal_ring_block_memblock_idx(xge_hal_ring_block_t *block)
{
	   return (int)*((u64 *)(void *)((char *)block +
	                           XGE_HAL_RING_MEMBLOCK_IDX_OFFSET));
}

__HAL_STATIC_RING __HAL_INLINE_RING void
__hal_ring_block_memblock_idx_set(xge_hal_ring_block_t*block, int memblock_idx)
{
	   *((u64 *)(void *)((char *)block +
	                   XGE_HAL_RING_MEMBLOCK_IDX_OFFSET)) =
	                   memblock_idx;
}


__HAL_STATIC_RING __HAL_INLINE_RING dma_addr_t
__hal_ring_block_next_pointer(xge_hal_ring_block_t *block)
{
	return (dma_addr_t)*((u64 *)(void *)((char *)block +
	        XGE_HAL_RING_NEXT_BLOCK_POINTER_OFFSET));
}

__HAL_STATIC_RING __HAL_INLINE_RING void
__hal_ring_block_next_pointer_set(xge_hal_ring_block_t *block,
	        dma_addr_t dma_next)
{
	*((u64 *)(void *)((char *)block +
	          XGE_HAL_RING_NEXT_BLOCK_POINTER_OFFSET)) = dma_next;
}

/**
 * xge_hal_ring_dtr_private - Get ULD private per-descriptor data.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 *
 * Returns: private ULD info associated with the descriptor.
 * ULD requests per-descriptor space via xge_hal_channel_open().
 *
 * See also: xge_hal_fifo_dtr_private().
 * Usage: See ex_rx_compl{}.
 */
__HAL_STATIC_RING __HAL_INLINE_RING void*
xge_hal_ring_dtr_private(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh)
{
	return (char *)__hal_ring_rxd_priv((xge_hal_ring_t *) channelh, dtrh) +
	                sizeof(xge_hal_ring_rxd_priv_t);
}

/**
 * xge_hal_ring_dtr_reserve - Reserve ring descriptor.
 * @channelh: Channel handle.
 * @dtrh: Reserved descriptor. On success HAL fills this "out" parameter
 *        with a valid handle.
 *
 * Reserve Rx descriptor for the subsequent filling-in (by upper layer
 * driver (ULD)) and posting on the corresponding channel (@channelh)
 * via xge_hal_ring_dtr_post().
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_INF_OUT_OF_DESCRIPTORS - Currently no descriptors available.
 *
 * See also: xge_hal_fifo_dtr_reserve(), xge_hal_ring_dtr_free(),
 * xge_hal_fifo_dtr_reserve_sp(), xge_hal_status_e{}.
 * Usage: See ex_post_all_rx{}.
 */
__HAL_STATIC_RING __HAL_INLINE_RING xge_hal_status_e
xge_hal_ring_dtr_reserve(xge_hal_channel_h channelh, xge_hal_dtr_h *dtrh)
{
	xge_hal_status_e status;
#if defined(XGE_HAL_RX_MULTI_RESERVE_IRQ)
	unsigned long flags;
#endif

#if defined(XGE_HAL_RX_MULTI_RESERVE)
	xge_os_spin_lock(&((xge_hal_channel_t*)channelh)->reserve_lock);
#elif defined(XGE_HAL_RX_MULTI_RESERVE_IRQ)
	xge_os_spin_lock_irq(&((xge_hal_channel_t*)channelh)->reserve_lock,
	flags);
#endif

	status = __hal_channel_dtr_alloc(channelh, dtrh);

#if defined(XGE_HAL_RX_MULTI_RESERVE)
	xge_os_spin_unlock(&((xge_hal_channel_t*)channelh)->reserve_lock);
#elif defined(XGE_HAL_RX_MULTI_RESERVE_IRQ)
	xge_os_spin_unlock_irq(&((xge_hal_channel_t*)channelh)->reserve_lock,
	             flags);
#endif

	if (status == XGE_HAL_OK) {
	    xge_hal_ring_rxd_1_t *rxdp = (xge_hal_ring_rxd_1_t *)*dtrh;

	    /* instead of memset: reset this RxD */
	    rxdp->control_1 = rxdp->control_2 = 0;

#if defined(XGE_OS_MEMORY_CHECK)
	    __hal_ring_rxd_priv((xge_hal_ring_t *) channelh, rxdp)->allocated = 1;
#endif
	}

	return status;
}

/**
 * xge_hal_ring_dtr_info_get - Get extended information associated with
 * a completed receive descriptor for 1b mode.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 * @ext_info: See xge_hal_dtr_info_t{}. Returned by HAL.
 *
 * Retrieve extended information associated with a completed receive descriptor.
 *
 * See also: xge_hal_dtr_info_t{}, xge_hal_ring_dtr_1b_get(),
 * xge_hal_ring_dtr_5b_get().
 */
__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_info_get(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	        xge_hal_dtr_info_t *ext_info)
{
	/* cast to 1-buffer mode RxD: the code below relies on the fact
	 * that control_1 and control_2 are formatted the same way.. */
	xge_hal_ring_rxd_1_t *rxdp = (xge_hal_ring_rxd_1_t *)dtrh;

	ext_info->l3_cksum = XGE_HAL_RXD_GET_L3_CKSUM(rxdp->control_1);
	ext_info->l4_cksum = XGE_HAL_RXD_GET_L4_CKSUM(rxdp->control_1);
	    ext_info->frame = XGE_HAL_RXD_GET_FRAME_TYPE(rxdp->control_1);
	    ext_info->proto = XGE_HAL_RXD_GET_FRAME_PROTO(rxdp->control_1);
	ext_info->vlan = XGE_HAL_RXD_GET_VLAN_TAG(rxdp->control_2);

	/* Herc only, a few extra cycles imposed on Xena and/or
	 * when RTH is not enabled.
	 * Alternatively, could check
	 * xge_hal_device_check_id(), hldev->config.rth_en, queue->rth_en */
	ext_info->rth_it_hit = XGE_HAL_RXD_GET_RTH_IT_HIT(rxdp->control_1);
	ext_info->rth_spdm_hit =
	XGE_HAL_RXD_GET_RTH_SPDM_HIT(rxdp->control_1);
	ext_info->rth_hash_type =
	XGE_HAL_RXD_GET_RTH_HASH_TYPE(rxdp->control_1);
	ext_info->rth_value = XGE_HAL_RXD_1_GET_RTH_VALUE(rxdp->control_2);
}

/**
 * xge_hal_ring_dtr_info_nb_get - Get extended information associated
 * with a completed receive descriptor for 3b or 5b
 * modes.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 * @ext_info: See xge_hal_dtr_info_t{}. Returned by HAL.
 *
 * Retrieve extended information associated with a completed receive descriptor.
 *
 * See also: xge_hal_dtr_info_t{}, xge_hal_ring_dtr_1b_get(),
 *           xge_hal_ring_dtr_5b_get().
 */
__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_info_nb_get(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	        xge_hal_dtr_info_t *ext_info)
{
	/* cast to 1-buffer mode RxD: the code below relies on the fact
	 * that control_1 and control_2 are formatted the same way.. */
	xge_hal_ring_rxd_1_t *rxdp = (xge_hal_ring_rxd_1_t *)dtrh;

	ext_info->l3_cksum = XGE_HAL_RXD_GET_L3_CKSUM(rxdp->control_1);
	ext_info->l4_cksum = XGE_HAL_RXD_GET_L4_CKSUM(rxdp->control_1);
	    ext_info->frame = XGE_HAL_RXD_GET_FRAME_TYPE(rxdp->control_1);
	    ext_info->proto = XGE_HAL_RXD_GET_FRAME_PROTO(rxdp->control_1);
	    ext_info->vlan = XGE_HAL_RXD_GET_VLAN_TAG(rxdp->control_2);
	/* Herc only, a few extra cycles imposed on Xena and/or
	 * when RTH is not enabled. Same comment as above. */
	ext_info->rth_it_hit = XGE_HAL_RXD_GET_RTH_IT_HIT(rxdp->control_1);
	ext_info->rth_spdm_hit =
	XGE_HAL_RXD_GET_RTH_SPDM_HIT(rxdp->control_1);
	ext_info->rth_hash_type =
	XGE_HAL_RXD_GET_RTH_HASH_TYPE(rxdp->control_1);
	ext_info->rth_value = (u32)rxdp->buffer0_ptr;
}

/**
 * xge_hal_ring_dtr_1b_set - Prepare 1-buffer-mode descriptor.
 * @dtrh: Descriptor handle.
 * @dma_pointer: DMA address of a single receive buffer this descriptor
 *               should carry. Note that by the time
 *               xge_hal_ring_dtr_1b_set
 *               is called, the receive buffer should be already mapped
 *               to the corresponding Xframe device.
 * @size: Size of the receive @dma_pointer buffer.
 *
 * Prepare 1-buffer-mode Rx descriptor for posting
 * (via xge_hal_ring_dtr_post()).
 *
 * This inline helper-function does not return any parameters and always
 * succeeds.
 *
 * See also: xge_hal_ring_dtr_3b_set(), xge_hal_ring_dtr_5b_set().
 * Usage: See ex_post_all_rx{}.
 */
__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_1b_set(xge_hal_dtr_h dtrh, dma_addr_t dma_pointer, int size)
{
	xge_hal_ring_rxd_1_t *rxdp = (xge_hal_ring_rxd_1_t *)dtrh;
	rxdp->buffer0_ptr = dma_pointer;
	rxdp->control_2 &= (~XGE_HAL_RXD_1_MASK_BUFFER0_SIZE);
	rxdp->control_2 |= XGE_HAL_RXD_1_SET_BUFFER0_SIZE(size);

	xge_debug_ring(XGE_TRACE, "xge_hal_ring_dtr_1b_set: rxdp %p control_2 %p buffer0_ptr %p",
	            (xge_hal_ring_rxd_1_t *)dtrh,
	            rxdp->control_2,
	            rxdp->buffer0_ptr);
}

/**
 * xge_hal_ring_dtr_1b_get - Get data from the completed 1-buf
 * descriptor.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 * @dma_pointer: DMA address of a single receive buffer _this_ descriptor
 *               carries. Returned by HAL.
 * @pkt_length: Length (in bytes) of the data in the buffer pointed by
 *              @dma_pointer. Returned by HAL.
 *
 * Retrieve protocol data from the completed 1-buffer-mode Rx descriptor.
 * This inline helper-function uses completed descriptor to populate receive
 * buffer pointer and other "out" parameters. The function always succeeds.
 *
 * See also: xge_hal_ring_dtr_3b_get(), xge_hal_ring_dtr_5b_get().
 * Usage: See ex_rx_compl{}.
 */
__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_1b_get(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	    dma_addr_t *dma_pointer, int *pkt_length)
{
	xge_hal_ring_rxd_1_t *rxdp = (xge_hal_ring_rxd_1_t *)dtrh;

	*pkt_length = XGE_HAL_RXD_1_GET_BUFFER0_SIZE(rxdp->control_2);
	*dma_pointer = rxdp->buffer0_ptr;

	((xge_hal_channel_t *)channelh)->poll_bytes += *pkt_length;
}

/**
 * xge_hal_ring_dtr_3b_set - Prepare 3-buffer-mode descriptor.
 * @dtrh: Descriptor handle.
 * @dma_pointers: Array of DMA addresses. Contains exactly 3 receive buffers
 *               _this_ descriptor should carry.
 *               Note that by the time xge_hal_ring_dtr_3b_set
 *               is called, the receive buffers should be mapped
 *               to the corresponding Xframe device.
 * @sizes: Array of receive buffer sizes. Contains 3 sizes: one size per
 *         buffer from @dma_pointers.
 *
 * Prepare 3-buffer-mode Rx descriptor for posting (via
 * xge_hal_ring_dtr_post()).
 * This inline helper-function does not return any parameters and always
 * succeeds.
 *
 * See also: xge_hal_ring_dtr_1b_set(), xge_hal_ring_dtr_5b_set().
 */
__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_3b_set(xge_hal_dtr_h dtrh, dma_addr_t dma_pointers[],
	        int sizes[])
{
	xge_hal_ring_rxd_3_t *rxdp = (xge_hal_ring_rxd_3_t *)dtrh;
	rxdp->buffer0_ptr = dma_pointers[0];
	rxdp->control_2 &= (~XGE_HAL_RXD_3_MASK_BUFFER0_SIZE);
	rxdp->control_2 |= XGE_HAL_RXD_3_SET_BUFFER0_SIZE(sizes[0]);
	rxdp->buffer1_ptr = dma_pointers[1];
	rxdp->control_2 &= (~XGE_HAL_RXD_3_MASK_BUFFER1_SIZE);
	rxdp->control_2 |= XGE_HAL_RXD_3_SET_BUFFER1_SIZE(sizes[1]);
	rxdp->buffer2_ptr = dma_pointers[2];
	rxdp->control_2 &= (~XGE_HAL_RXD_3_MASK_BUFFER2_SIZE);
	rxdp->control_2 |= XGE_HAL_RXD_3_SET_BUFFER2_SIZE(sizes[2]);
}

/**
 * xge_hal_ring_dtr_3b_get - Get data from the completed 3-buf
 * descriptor.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 * @dma_pointers: DMA addresses of the 3 receive buffers _this_ descriptor
 *                carries. The first two buffers contain ethernet and
 *                (IP + transport) headers. The 3rd buffer contains packet
 *                data.
 *                Returned by HAL.
 * @sizes: Array of receive buffer sizes. Contains 3 sizes: one size per
 * buffer from @dma_pointers. Returned by HAL.
 *
 * Retrieve protocol data from the completed 3-buffer-mode Rx descriptor.
 * This inline helper-function uses completed descriptor to populate receive
 * buffer pointer and other "out" parameters. The function always succeeds.
 *
 * See also: xge_hal_ring_dtr_3b_get(), xge_hal_ring_dtr_5b_get().
 */
__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_3b_get(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	    dma_addr_t dma_pointers[], int sizes[])
{
	xge_hal_ring_rxd_3_t *rxdp = (xge_hal_ring_rxd_3_t *)dtrh;

	dma_pointers[0] = rxdp->buffer0_ptr;
	sizes[0] = XGE_HAL_RXD_3_GET_BUFFER0_SIZE(rxdp->control_2);

	dma_pointers[1] = rxdp->buffer1_ptr;
	sizes[1] = XGE_HAL_RXD_3_GET_BUFFER1_SIZE(rxdp->control_2);

	dma_pointers[2] = rxdp->buffer2_ptr;
	sizes[2] = XGE_HAL_RXD_3_GET_BUFFER2_SIZE(rxdp->control_2);

	((xge_hal_channel_t *)channelh)->poll_bytes += sizes[0] + sizes[1] +
	    sizes[2];
}

/**
 * xge_hal_ring_dtr_5b_set - Prepare 5-buffer-mode descriptor.
 * @dtrh: Descriptor handle.
 * @dma_pointers: Array of DMA addresses. Contains exactly 5 receive buffers
 *               _this_ descriptor should carry.
 *               Note that by the time xge_hal_ring_dtr_5b_set
 *               is called, the receive buffers should be mapped
 *               to the corresponding Xframe device.
 * @sizes: Array of receive buffer sizes. Contains 5 sizes: one size per
 *         buffer from @dma_pointers.
 *
 * Prepare 3-buffer-mode Rx descriptor for posting (via
 * xge_hal_ring_dtr_post()).
 * This inline helper-function does not return any parameters and always
 * succeeds.
 *
 * See also: xge_hal_ring_dtr_1b_set(), xge_hal_ring_dtr_3b_set().
 */
__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_5b_set(xge_hal_dtr_h dtrh, dma_addr_t dma_pointers[],
	        int sizes[])
{
	xge_hal_ring_rxd_5_t *rxdp = (xge_hal_ring_rxd_5_t *)dtrh;
	rxdp->buffer0_ptr = dma_pointers[0];
	rxdp->control_2 &= (~XGE_HAL_RXD_5_MASK_BUFFER0_SIZE);
	rxdp->control_2 |= XGE_HAL_RXD_5_SET_BUFFER0_SIZE(sizes[0]);
	rxdp->buffer1_ptr = dma_pointers[1];
	rxdp->control_2 &= (~XGE_HAL_RXD_5_MASK_BUFFER1_SIZE);
	rxdp->control_2 |= XGE_HAL_RXD_5_SET_BUFFER1_SIZE(sizes[1]);
	rxdp->buffer2_ptr = dma_pointers[2];
	rxdp->control_2 &= (~XGE_HAL_RXD_5_MASK_BUFFER2_SIZE);
	rxdp->control_2 |= XGE_HAL_RXD_5_SET_BUFFER2_SIZE(sizes[2]);
	rxdp->buffer3_ptr = dma_pointers[3];
	rxdp->control_3 &= (~XGE_HAL_RXD_5_MASK_BUFFER3_SIZE);
	rxdp->control_3 |= XGE_HAL_RXD_5_SET_BUFFER3_SIZE(sizes[3]);
	rxdp->buffer4_ptr = dma_pointers[4];
	rxdp->control_3 &= (~XGE_HAL_RXD_5_MASK_BUFFER4_SIZE);
	rxdp->control_3 |= XGE_HAL_RXD_5_SET_BUFFER4_SIZE(sizes[4]);
}

/**
 * xge_hal_ring_dtr_5b_get - Get data from the completed 5-buf
 * descriptor.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 * @dma_pointers: DMA addresses of the 5 receive buffers _this_ descriptor
 *                carries. The first 4 buffers contains L2 (ethernet) through
 *                L5 headers. The 5th buffer contain received (applicaion)
 *                data. Returned by HAL.
 * @sizes: Array of receive buffer sizes. Contains 5 sizes: one size per
 * buffer from @dma_pointers. Returned by HAL.
 *
 * Retrieve protocol data from the completed 5-buffer-mode Rx descriptor.
 * This inline helper-function uses completed descriptor to populate receive
 * buffer pointer and other "out" parameters. The function always succeeds.
 *
 * See also: xge_hal_ring_dtr_3b_get(), xge_hal_ring_dtr_5b_get().
 */
__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_5b_get(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	    dma_addr_t dma_pointers[], int sizes[])
{
	xge_hal_ring_rxd_5_t *rxdp = (xge_hal_ring_rxd_5_t *)dtrh;

	dma_pointers[0] = rxdp->buffer0_ptr;
	sizes[0] = XGE_HAL_RXD_5_GET_BUFFER0_SIZE(rxdp->control_2);

	dma_pointers[1] = rxdp->buffer1_ptr;
	sizes[1] = XGE_HAL_RXD_5_GET_BUFFER1_SIZE(rxdp->control_2);

	dma_pointers[2] = rxdp->buffer2_ptr;
	sizes[2] = XGE_HAL_RXD_5_GET_BUFFER2_SIZE(rxdp->control_2);

	dma_pointers[3] = rxdp->buffer3_ptr;
	sizes[3] = XGE_HAL_RXD_5_GET_BUFFER3_SIZE(rxdp->control_3);

	dma_pointers[4] = rxdp->buffer4_ptr;
	sizes[4] = XGE_HAL_RXD_5_GET_BUFFER4_SIZE(rxdp->control_3);

	((xge_hal_channel_t *)channelh)->poll_bytes += sizes[0] + sizes[1] +
	    sizes[2] + sizes[3] + sizes[4];
}


/**
 * xge_hal_ring_dtr_pre_post - FIXME.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 *
 * TBD
 */
__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_pre_post(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh)
{
	xge_hal_ring_rxd_1_t *rxdp = (xge_hal_ring_rxd_1_t *)dtrh;
#if defined(XGE_OS_DMA_REQUIRES_SYNC) && defined(XGE_HAL_DMA_DTR_STREAMING)
	xge_hal_ring_rxd_priv_t *priv;
	xge_hal_ring_t *ring = (xge_hal_ring_t *)channelh;
#endif
#if defined(XGE_HAL_RX_MULTI_POST_IRQ)
	unsigned long flags;
#endif

	rxdp->control_2 |= XGE_HAL_RXD_NOT_COMPLETED;

#ifdef XGE_DEBUG_ASSERT
	    /* make sure Xena overwrites the (illegal) t_code on completion */
	    XGE_HAL_RXD_SET_T_CODE(rxdp->control_1, XGE_HAL_RXD_T_CODE_UNUSED_C);
#endif

	xge_debug_ring(XGE_TRACE, "xge_hal_ring_dtr_pre_post: rxd 0x"XGE_OS_LLXFMT" posted %d  post_qid %d",
	        (unsigned long long)(ulong_t)dtrh,
	        ((xge_hal_ring_t *)channelh)->channel.post_index,
	        ((xge_hal_ring_t *)channelh)->channel.post_qid);

#if defined(XGE_HAL_RX_MULTI_POST)
	xge_os_spin_lock(&((xge_hal_channel_t*)channelh)->post_lock);
#elif defined(XGE_HAL_RX_MULTI_POST_IRQ)
	xge_os_spin_lock_irq(&((xge_hal_channel_t*)channelh)->post_lock,
	flags);
#endif

#if defined(XGE_DEBUG_ASSERT) && defined(XGE_HAL_RING_ENFORCE_ORDER)
	{
	    xge_hal_channel_t *channel = (xge_hal_channel_t *)channelh;

	    if (channel->post_index != 0) {
	        xge_hal_dtr_h prev_dtrh;
	        xge_hal_ring_rxd_priv_t *rxdp_priv;

	        rxdp_priv = __hal_ring_rxd_priv((xge_hal_ring_t*)channel, rxdp);
	        prev_dtrh = channel->work_arr[channel->post_index - 1];

	        if (prev_dtrh != NULL &&
	            (rxdp_priv->dma_offset & (~0xFFF)) !=
	                    rxdp_priv->dma_offset) {
	            xge_assert((char *)prev_dtrh +
	                ((xge_hal_ring_t*)channel)->rxd_size == dtrh);
	        }
	    }
	}
#endif

	__hal_channel_dtr_post(channelh, dtrh);

#if defined(XGE_HAL_RX_MULTI_POST)
	xge_os_spin_unlock(&((xge_hal_channel_t*)channelh)->post_lock);
#elif defined(XGE_HAL_RX_MULTI_POST_IRQ)
	xge_os_spin_unlock_irq(&((xge_hal_channel_t*)channelh)->post_lock,
	               flags);
#endif
}


/**
 * xge_hal_ring_dtr_post_post - FIXME.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 * 
 * TBD
 */
__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_post_post(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh)
{
	xge_hal_ring_rxd_1_t *rxdp = (xge_hal_ring_rxd_1_t *)dtrh;
	xge_hal_ring_t *ring = (xge_hal_ring_t *)channelh;
#if defined(XGE_OS_DMA_REQUIRES_SYNC) && defined(XGE_HAL_DMA_DTR_STREAMING)
	xge_hal_ring_rxd_priv_t *priv;
#endif
	/* do POST */
	rxdp->control_1 |= XGE_HAL_RXD_POSTED_4_XFRAME;

#if defined(XGE_OS_DMA_REQUIRES_SYNC) && defined(XGE_HAL_DMA_DTR_STREAMING)
	priv = __hal_ring_rxd_priv(ring, rxdp);
	xge_os_dma_sync(ring->channel.pdev,
	              priv->dma_handle, priv->dma_addr,
	          priv->dma_offset, ring->rxd_size,
	          XGE_OS_DMA_DIR_TODEVICE);
#endif

	xge_debug_ring(XGE_TRACE, "xge_hal_ring_dtr_post_post: rxdp %p control_1 %p",
	              (xge_hal_ring_rxd_1_t *)dtrh,
	              rxdp->control_1);

	if (ring->channel.usage_cnt > 0)
	    ring->channel.usage_cnt--;
}

/**
 * xge_hal_ring_dtr_post_post_wmb.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 * 
 * Similar as xge_hal_ring_dtr_post_post, but in addition it does memory barrier.
 */
__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_post_post_wmb(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh)
{
	xge_hal_ring_rxd_1_t *rxdp = (xge_hal_ring_rxd_1_t *)dtrh;
	xge_hal_ring_t *ring = (xge_hal_ring_t *)channelh;
#if defined(XGE_OS_DMA_REQUIRES_SYNC) && defined(XGE_HAL_DMA_DTR_STREAMING)
	xge_hal_ring_rxd_priv_t *priv;
#endif
	/* Do memory barrier before changing the ownership */
	xge_os_wmb();
	
	/* do POST */
	rxdp->control_1 |= XGE_HAL_RXD_POSTED_4_XFRAME;

#if defined(XGE_OS_DMA_REQUIRES_SYNC) && defined(XGE_HAL_DMA_DTR_STREAMING)
	priv = __hal_ring_rxd_priv(ring, rxdp);
	xge_os_dma_sync(ring->channel.pdev,
	              priv->dma_handle, priv->dma_addr,
	          priv->dma_offset, ring->rxd_size,
	          XGE_OS_DMA_DIR_TODEVICE);
#endif

	if (ring->channel.usage_cnt > 0)
	    ring->channel.usage_cnt--;

	xge_debug_ring(XGE_TRACE, "xge_hal_ring_dtr_post_post_wmb: rxdp %p control_1 %p rxds_with_host %d",
	              (xge_hal_ring_rxd_1_t *)dtrh,
	              rxdp->control_1, ring->channel.usage_cnt);

}

/**
 * xge_hal_ring_dtr_post - Post descriptor on the ring channel.
 * @channelh: Channel handle.
 * @dtrh: Descriptor obtained via xge_hal_ring_dtr_reserve().
 *
 * Post descriptor on the 'ring' type channel.
 * Prior to posting the descriptor should be filled in accordance with
 * Host/Xframe interface specification for a given service (LL, etc.).
 *
 * See also: xge_hal_fifo_dtr_post_many(), xge_hal_fifo_dtr_post().
 * Usage: See ex_post_all_rx{}.
 */
__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_post(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh)
{
	xge_hal_ring_dtr_pre_post(channelh, dtrh);
	xge_hal_ring_dtr_post_post(channelh, dtrh);
}

/**
 * xge_hal_ring_dtr_next_completed - Get the _next_ completed
 * descriptor.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle. Returned by HAL.
 * @t_code: Transfer code, as per Xframe User Guide,
 *          Receive Descriptor Format. Returned by HAL.
 *
 * Retrieve the _next_ completed descriptor.
 * HAL uses channel callback (*xge_hal_channel_callback_f) to notifiy
 * upper-layer driver (ULD) of new completed descriptors. After that
 * the ULD can use xge_hal_ring_dtr_next_completed to retrieve the rest
 * completions (the very first completion is passed by HAL via
 * xge_hal_channel_callback_f).
 *
 * Implementation-wise, the upper-layer driver is free to call
 * xge_hal_ring_dtr_next_completed either immediately from inside the
 * channel callback, or in a deferred fashion and separate (from HAL)
 * context.
 *
 * Non-zero @t_code means failure to fill-in receive buffer(s)
 * of the descriptor.
 * For instance, parity error detected during the data transfer.
 * In this case Xframe will complete the descriptor and indicate
 * for the host that the received data is not to be used.
 * For details please refer to Xframe User Guide.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS - No completed descriptors
 * are currently available for processing.
 *
 * See also: xge_hal_channel_callback_f{},
 * xge_hal_fifo_dtr_next_completed(), xge_hal_status_e{}.
 * Usage: See ex_rx_compl{}.
 */
__HAL_STATIC_RING __HAL_INLINE_RING xge_hal_status_e
xge_hal_ring_dtr_next_completed(xge_hal_channel_h channelh, xge_hal_dtr_h *dtrh,
	            u8 *t_code)
{
	xge_hal_ring_rxd_1_t *rxdp; /* doesn't matter 1, 3 or 5... */
	xge_hal_ring_t *ring = (xge_hal_ring_t *)channelh;
#if defined(XGE_OS_DMA_REQUIRES_SYNC) && defined(XGE_HAL_DMA_DTR_STREAMING)
	xge_hal_ring_rxd_priv_t *priv;
#endif

	__hal_channel_dtr_try_complete(ring, dtrh);
	rxdp = (xge_hal_ring_rxd_1_t *)*dtrh;
	if (rxdp == NULL) {
	    return XGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS;
	}

#if defined(XGE_OS_DMA_REQUIRES_SYNC) && defined(XGE_HAL_DMA_DTR_STREAMING)
	/* Note: 24 bytes at most means:
	 *  - Control_3 in case of 5-buffer mode
	 *  - Control_1 and Control_2
	 *
	 * This is the only length needs to be invalidated
	 * type of channels.*/
	priv = __hal_ring_rxd_priv(ring, rxdp);
	xge_os_dma_sync(ring->channel.pdev,
	              priv->dma_handle, priv->dma_addr,
	          priv->dma_offset, 24,
	          XGE_OS_DMA_DIR_FROMDEVICE);
#endif

	/* check whether it is not the end */
	if (!(rxdp->control_2 & XGE_HAL_RXD_NOT_COMPLETED) &&
	    !(rxdp->control_1 & XGE_HAL_RXD_POSTED_4_XFRAME)) {
#ifndef XGE_HAL_IRQ_POLLING
	    if (++ring->cmpl_cnt > ring->indicate_max_pkts) {
	        /* reset it. since we don't want to return
	         * garbage to the ULD */
	        *dtrh = 0;
	        return XGE_HAL_COMPLETIONS_REMAIN;
	    }
#endif

#ifdef XGE_DEBUG_ASSERT
#if defined(XGE_HAL_USE_5B_MODE)
#if !defined(XGE_OS_PLATFORM_64BIT)
	    if (ring->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_5) {
	        xge_assert(((xge_hal_ring_rxd_5_t *)
	                rxdp)->host_control!=0);
	    }
#endif

#else
	    xge_assert(rxdp->host_control!=0);
#endif
#endif

	    __hal_channel_dtr_complete(ring);

	    *t_code = (u8)XGE_HAL_RXD_GET_T_CODE(rxdp->control_1);

	            /* see XGE_HAL_SET_RXD_T_CODE() above.. */
	    xge_assert(*t_code != XGE_HAL_RXD_T_CODE_UNUSED_C);

	    xge_debug_ring(XGE_TRACE,
	        "compl_index %d post_qid %d t_code %d rxd 0x"XGE_OS_LLXFMT,
	        ((xge_hal_channel_t*)ring)->compl_index,
	        ((xge_hal_channel_t*)ring)->post_qid, *t_code,
	        (unsigned long long)(ulong_t)rxdp);

	    ring->channel.usage_cnt++;
	    if (ring->channel.stats.usage_max < ring->channel.usage_cnt)
	        ring->channel.stats.usage_max = ring->channel.usage_cnt;

	    return XGE_HAL_OK;
	}

	/* reset it. since we don't want to return
	 * garbage to the ULD */
	*dtrh = 0;
	return XGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS;
}

/**
 * xge_hal_ring_dtr_free - Free descriptor.
 * @channelh: Channel handle.
 * @dtrh: Descriptor handle.
 *
 * Free the reserved descriptor. This operation is "symmetrical" to
 * xge_hal_ring_dtr_reserve. The "free-ing" completes the descriptor's
 * lifecycle.
 *
 * After free-ing (see xge_hal_ring_dtr_free()) the descriptor again can
 * be:
 *
 * - reserved (xge_hal_ring_dtr_reserve);
 *
 * - posted (xge_hal_ring_dtr_post);
 *
 * - completed (xge_hal_ring_dtr_next_completed);
 *
 * - and recycled again (xge_hal_ring_dtr_free).
 *
 * For alternative state transitions and more details please refer to
 * the design doc.
 *
 * See also: xge_hal_ring_dtr_reserve(), xge_hal_fifo_dtr_free().
 * Usage: See ex_rx_compl{}.
 */
__HAL_STATIC_RING __HAL_INLINE_RING void
xge_hal_ring_dtr_free(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh)
{
#if defined(XGE_HAL_RX_MULTI_FREE_IRQ)
	unsigned long flags;
#endif

#if defined(XGE_HAL_RX_MULTI_FREE)
	xge_os_spin_lock(&((xge_hal_channel_t*)channelh)->free_lock);
#elif defined(XGE_HAL_RX_MULTI_FREE_IRQ)
	xge_os_spin_lock_irq(&((xge_hal_channel_t*)channelh)->free_lock,
	flags);
#endif

	__hal_channel_dtr_free(channelh, dtrh);
#if defined(XGE_OS_MEMORY_CHECK)
	__hal_ring_rxd_priv((xge_hal_ring_t * ) channelh, dtrh)->allocated = 0;
#endif

#if defined(XGE_HAL_RX_MULTI_FREE)
	xge_os_spin_unlock(&((xge_hal_channel_t*)channelh)->free_lock);
#elif defined(XGE_HAL_RX_MULTI_FREE_IRQ)
	xge_os_spin_unlock_irq(&((xge_hal_channel_t*)channelh)->free_lock,
	flags);
#endif
}

/**
 * xge_hal_ring_is_next_dtr_completed - Check if the next dtr is completed
 * @channelh: Channel handle.
 *
 * Checks if the the _next_ completed descriptor is in host memory
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS - No completed descriptors
 * are currently available for processing.
 */
__HAL_STATIC_RING __HAL_INLINE_RING xge_hal_status_e
xge_hal_ring_is_next_dtr_completed(xge_hal_channel_h channelh)
{
	xge_hal_ring_rxd_1_t *rxdp; /* doesn't matter 1, 3 or 5... */
	xge_hal_ring_t *ring = (xge_hal_ring_t *)channelh;
	xge_hal_dtr_h dtrh;

	__hal_channel_dtr_try_complete(ring, &dtrh);
	rxdp = (xge_hal_ring_rxd_1_t *)dtrh;
	if (rxdp == NULL) {
	    return XGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS;
	}

	/* check whether it is not the end */
	if (!(rxdp->control_2 & XGE_HAL_RXD_NOT_COMPLETED) &&
	    !(rxdp->control_1 & XGE_HAL_RXD_POSTED_4_XFRAME)) {

#ifdef XGE_DEBUG_ASSERT
#if defined(XGE_HAL_USE_5B_MODE)
#if !defined(XGE_OS_PLATFORM_64BIT)
	    if (ring->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_5) {
	        xge_assert(((xge_hal_ring_rxd_5_t *)
	                rxdp)->host_control!=0);
	    }
#endif

#else
	    xge_assert(rxdp->host_control!=0);
#endif
#endif
	    return XGE_HAL_OK;
	}

	return XGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS;
}
