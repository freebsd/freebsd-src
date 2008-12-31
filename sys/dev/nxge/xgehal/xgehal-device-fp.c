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
 * $FreeBSD: src/sys/dev/nxge/xgehal/xgehal-device-fp.c,v 1.1.2.1.4.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifdef XGE_DEBUG_FP
#include <dev/nxge/include/xgehal-device.h>
#endif

#include <dev/nxge/include/xgehal-ring.h>
#include <dev/nxge/include/xgehal-fifo.h>

/**
 * xge_hal_device_bar0 - Get BAR0 mapped address.
 * @hldev: HAL device handle.
 *
 * Returns: BAR0 address of the specified device.
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE char *
xge_hal_device_bar0(xge_hal_device_t *hldev)
{
	return hldev->bar0;
}

/**
 * xge_hal_device_isrbar0 - Get BAR0 mapped address.
 * @hldev: HAL device handle.
 *
 * Returns: BAR0 address of the specified device.
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE char *
xge_hal_device_isrbar0(xge_hal_device_t *hldev)
{
	return hldev->isrbar0;
}

/**
 * xge_hal_device_bar1 - Get BAR1 mapped address.
 * @hldev: HAL device handle.
 *
 * Returns: BAR1 address of the specified device.
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE char *
xge_hal_device_bar1(xge_hal_device_t *hldev)
{
	return hldev->bar1;
}

/**
 * xge_hal_device_bar0_set - Set BAR0 mapped address.
 * @hldev: HAL device handle.
 * @bar0: BAR0 mapped address.
 * * Set BAR0 address in the HAL device object.
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_bar0_set(xge_hal_device_t *hldev, char *bar0)
{
	xge_assert(bar0);
	hldev->bar0 = bar0;
}

/**
 * xge_hal_device_isrbar0_set - Set BAR0 mapped address.
 * @hldev: HAL device handle.
 * @isrbar0: BAR0 mapped address.
 * * Set BAR0 address in the HAL device object.
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_isrbar0_set(xge_hal_device_t *hldev, char *isrbar0)
{
	xge_assert(isrbar0);
	hldev->isrbar0 = isrbar0;
}

/**
 * xge_hal_device_bar1_set - Set BAR1 mapped address.
 * @hldev: HAL device handle.
 * @channelh: Channel handle.
 * @bar1: BAR1 mapped address.
 *
 * Set BAR1 address for the given channel.
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_bar1_set(xge_hal_device_t *hldev, xge_hal_channel_h channelh,
	           char *bar1)
{
	xge_hal_fifo_t *fifo = (xge_hal_fifo_t *)channelh;

	xge_assert(bar1);
	xge_assert(fifo);

	/* Initializing the BAR1 address as the start of
	 * the FIFO queue pointer and as a location of FIFO control
	 * word. */
	fifo->hw_pair =
	        (xge_hal_fifo_hw_pair_t *) (bar1 +
	            (fifo->channel.post_qid * XGE_HAL_FIFO_HW_PAIR_OFFSET));
	hldev->bar1 = bar1;
}


/**
 * xge_hal_device_rev - Get Device revision number.
 * @hldev: HAL device handle.
 *
 * Returns: Device revision number
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE int
xge_hal_device_rev(xge_hal_device_t *hldev)
{
	    return hldev->revision;
}


/**
 * xge_hal_device_begin_irq - Begin IRQ processing.
 * @hldev: HAL device handle.
 * @reason: "Reason" for the interrupt, the value of Xframe's
 *          general_int_status register.
 *
 * The function performs two actions, It first checks whether (shared IRQ) the
 * interrupt was raised by the device. Next, it masks the device interrupts.
 *
 * Note:
 * xge_hal_device_begin_irq() does not flush MMIO writes through the
 * bridge. Therefore, two back-to-back interrupts are potentially possible.
 * It is the responsibility of the ULD to make sure that only one
 * xge_hal_device_continue_irq() runs at a time.
 *
 * Returns: 0, if the interrupt is not "ours" (note that in this case the
 * device remain enabled).
 * Otherwise, xge_hal_device_begin_irq() returns 64bit general adapter
 * status.
 * See also: xge_hal_device_handle_irq()
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE xge_hal_status_e
xge_hal_device_begin_irq(xge_hal_device_t *hldev, u64 *reason)
{
	u64 val64;
	xge_hal_pci_bar0_t *isrbar0 = (xge_hal_pci_bar0_t *)hldev->isrbar0;

	hldev->stats.sw_dev_info_stats.total_intr_cnt++;

	val64 = xge_os_pio_mem_read64(hldev->pdev,
	              hldev->regh0, &isrbar0->general_int_status);
	if (xge_os_unlikely(!val64)) {
	    /* not Xframe interrupt */
	    hldev->stats.sw_dev_info_stats.not_xge_intr_cnt++;
	    *reason = 0;
	        return XGE_HAL_ERR_WRONG_IRQ;
	}

	if (xge_os_unlikely(val64 == XGE_HAL_ALL_FOXES)) {
	            u64 adapter_status =
	                    xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                      &isrbar0->adapter_status);
	            if (adapter_status == XGE_HAL_ALL_FOXES)  {
	                (void) xge_queue_produce(hldev->queueh,
	                     XGE_HAL_EVENT_SLOT_FREEZE,
	                     hldev,
	                     1,  /* critical: slot freeze */
	                     sizeof(u64),
	                     (void*)&adapter_status);
	        *reason = 0;
	        return XGE_HAL_ERR_CRITICAL;
	    }
	}

	*reason = val64;

	/* separate fast path, i.e. no errors */
	if (val64 & XGE_HAL_GEN_INTR_RXTRAFFIC) {
	    hldev->stats.sw_dev_info_stats.rx_traffic_intr_cnt++;
	    return XGE_HAL_OK;
	}
	if (val64 & XGE_HAL_GEN_INTR_TXTRAFFIC) {
	    hldev->stats.sw_dev_info_stats.tx_traffic_intr_cnt++;
	    return XGE_HAL_OK;
	}

	hldev->stats.sw_dev_info_stats.not_traffic_intr_cnt++;
	if (xge_os_unlikely(val64 & XGE_HAL_GEN_INTR_TXPIC)) {
	    xge_hal_status_e status;
	    hldev->stats.sw_dev_info_stats.txpic_intr_cnt++;
	    status = __hal_device_handle_txpic(hldev, val64);
	    if (status != XGE_HAL_OK) {
	        return status;
	    }
	}

	if (xge_os_unlikely(val64 & XGE_HAL_GEN_INTR_TXDMA)) {
	    xge_hal_status_e status;
	    hldev->stats.sw_dev_info_stats.txdma_intr_cnt++;
	    status = __hal_device_handle_txdma(hldev, val64);
	    if (status != XGE_HAL_OK) {
	        return status;
	    }
	}

	if (xge_os_unlikely(val64 & XGE_HAL_GEN_INTR_TXMAC)) {
	    xge_hal_status_e status;
	    hldev->stats.sw_dev_info_stats.txmac_intr_cnt++;
	    status = __hal_device_handle_txmac(hldev, val64);
	    if (status != XGE_HAL_OK) {
	        return status;
	    }
	}

	if (xge_os_unlikely(val64 & XGE_HAL_GEN_INTR_TXXGXS)) {
	    xge_hal_status_e status;
	    hldev->stats.sw_dev_info_stats.txxgxs_intr_cnt++;
	    status = __hal_device_handle_txxgxs(hldev, val64);
	    if (status != XGE_HAL_OK) {
	        return status;
	    }
	}

	if (xge_os_unlikely(val64 & XGE_HAL_GEN_INTR_RXPIC)) {
	    xge_hal_status_e status;
	    hldev->stats.sw_dev_info_stats.rxpic_intr_cnt++;
	    status = __hal_device_handle_rxpic(hldev, val64);
	    if (status != XGE_HAL_OK) {
	        return status;
	    }
	}

	if (xge_os_unlikely(val64 & XGE_HAL_GEN_INTR_RXDMA)) {
	    xge_hal_status_e status;
	    hldev->stats.sw_dev_info_stats.rxdma_intr_cnt++;
	    status = __hal_device_handle_rxdma(hldev, val64);
	    if (status != XGE_HAL_OK) {
	        return status;
	    }
	}

	if (xge_os_unlikely(val64 & XGE_HAL_GEN_INTR_RXMAC)) {
	    xge_hal_status_e status;
	    hldev->stats.sw_dev_info_stats.rxmac_intr_cnt++;
	    status = __hal_device_handle_rxmac(hldev, val64);
	    if (status != XGE_HAL_OK) {
	        return status;
	    }
	}

	if (xge_os_unlikely(val64 & XGE_HAL_GEN_INTR_RXXGXS)) {
	    xge_hal_status_e status;
	    hldev->stats.sw_dev_info_stats.rxxgxs_intr_cnt++;
	    status = __hal_device_handle_rxxgxs(hldev, val64);
	    if (status != XGE_HAL_OK) {
	        return status;
	    }
	}

	if (xge_os_unlikely(val64 & XGE_HAL_GEN_INTR_MC)) {
	    xge_hal_status_e status;
	    hldev->stats.sw_dev_info_stats.mc_intr_cnt++;
	    status = __hal_device_handle_mc(hldev, val64);
	    if (status != XGE_HAL_OK) {
	        return status;
	    }
	}

	return XGE_HAL_OK;
}

/**
 * xge_hal_device_clear_rx - Acknowledge (that is, clear) the
 * condition that has caused the RX interrupt.
 * @hldev: HAL device handle.
 *
 * Acknowledge (that is, clear) the condition that has caused
 * the Rx interrupt.
 * See also: xge_hal_device_begin_irq(), xge_hal_device_continue_irq(),
 * xge_hal_device_clear_tx(), xge_hal_device_mask_rx().
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_clear_rx(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *isrbar0 = (xge_hal_pci_bar0_t *)hldev->isrbar0;

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	             0xFFFFFFFFFFFFFFFFULL,
	             &isrbar0->rx_traffic_int);
}

/**
 * xge_hal_device_clear_tx - Acknowledge (that is, clear) the
 * condition that has caused the TX interrupt.
 * @hldev: HAL device handle.
 *
 * Acknowledge (that is, clear) the condition that has caused
 * the Tx interrupt.
 * See also: xge_hal_device_begin_irq(), xge_hal_device_continue_irq(),
 * xge_hal_device_clear_rx(), xge_hal_device_mask_tx().
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_clear_tx(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *isrbar0 = (xge_hal_pci_bar0_t *)hldev->isrbar0;

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	             0xFFFFFFFFFFFFFFFFULL,
	             &isrbar0->tx_traffic_int);
}

/**
 * xge_hal_device_poll_rx_channel - Poll Rx channel for completed
 * descriptors and process the same.
 * @channel: HAL channel.
 * @got_rx: Buffer to return the flag set if receive interrupt is occured
 *
 * The function polls the Rx channel for the completed  descriptors and calls
 * the upper-layer driver (ULD) via supplied completion callback.
 *
 * Returns: XGE_HAL_OK, if the polling is completed successful.
 * XGE_HAL_COMPLETIONS_REMAIN: There are still more completed
 * descriptors available which are yet to be processed.
 *
 * See also: xge_hal_device_poll_tx_channel()
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE xge_hal_status_e
xge_hal_device_poll_rx_channel(xge_hal_channel_t *channel, int *got_rx)
{
	xge_hal_status_e ret = XGE_HAL_OK;
	xge_hal_dtr_h first_dtrh;
	xge_hal_device_t *hldev = (xge_hal_device_t *)channel->devh;
	u8 t_code;
	int got_bytes;

	/* for each opened rx channel */
	got_bytes = *got_rx = 0;
	((xge_hal_ring_t *)channel)->cmpl_cnt = 0;
	channel->poll_bytes = 0;
	if ((ret = xge_hal_ring_dtr_next_completed (channel, &first_dtrh,
	    &t_code)) == XGE_HAL_OK) {
	    if (channel->callback(channel, first_dtrh,
	        t_code, channel->userdata) != XGE_HAL_OK) {
	        (*got_rx) += ((xge_hal_ring_t *)channel)->cmpl_cnt + 1;
	        got_bytes += channel->poll_bytes + 1;
	        ret = XGE_HAL_COMPLETIONS_REMAIN;
	    } else {
	        (*got_rx) += ((xge_hal_ring_t *)channel)->cmpl_cnt + 1;
	        got_bytes += channel->poll_bytes + 1;
	    }
	}

	if (*got_rx) {
	    hldev->irq_workload_rxd[channel->post_qid] += *got_rx;
	    hldev->irq_workload_rxcnt[channel->post_qid] ++;
	}
	hldev->irq_workload_rxlen[channel->post_qid] += got_bytes;

	return ret;
}

/**
 * xge_hal_device_poll_tx_channel - Poll Tx channel for completed
 * descriptors and process the same.
 * @channel: HAL channel.
 * @got_tx: Buffer to return the flag set if transmit interrupt is occured
 *
 * The function polls the Tx channel for the completed  descriptors and calls
 * the upper-layer driver (ULD) via supplied completion callback.
 *
 * Returns: XGE_HAL_OK, if the polling is completed successful.
 * XGE_HAL_COMPLETIONS_REMAIN: There are still more completed
 * descriptors available which are yet to be processed.
 *
 * See also: xge_hal_device_poll_rx_channel().
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE xge_hal_status_e
xge_hal_device_poll_tx_channel(xge_hal_channel_t *channel, int *got_tx)
{
	xge_hal_dtr_h first_dtrh;
	xge_hal_device_t *hldev = (xge_hal_device_t *)channel->devh;
	u8 t_code;
	int got_bytes;

	/* for each opened tx channel */
	got_bytes = *got_tx = 0;
	channel->poll_bytes = 0;
	if (xge_hal_fifo_dtr_next_completed (channel, &first_dtrh,
	    &t_code) == XGE_HAL_OK) {
	    if (channel->callback(channel, first_dtrh,
	        t_code, channel->userdata) != XGE_HAL_OK) {
	        (*got_tx)++;
	        got_bytes += channel->poll_bytes + 1;
	        return XGE_HAL_COMPLETIONS_REMAIN;
	    }
	    (*got_tx)++;
	    got_bytes += channel->poll_bytes + 1;
	}

	if (*got_tx) {
	    hldev->irq_workload_txd[channel->post_qid] += *got_tx;
	    hldev->irq_workload_txcnt[channel->post_qid] ++;
	}
	hldev->irq_workload_txlen[channel->post_qid] += got_bytes;

	return XGE_HAL_OK;
}

/**
 * xge_hal_device_poll_rx_channels - Poll Rx channels for completed
 * descriptors and process the same.
 * @hldev: HAL device handle.
 * @got_rx: Buffer to return flag set if receive is ready
 *
 * The function polls the Rx channels for the completed descriptors and calls
 * the upper-layer driver (ULD) via supplied completion callback.
 *
 * Returns: XGE_HAL_OK, if the polling is completed successful.
 * XGE_HAL_COMPLETIONS_REMAIN: There are still more completed
 * descriptors available which are yet to be processed.
 *
 * See also: xge_hal_device_poll_tx_channels(), xge_hal_device_continue_irq().
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE xge_hal_status_e
xge_hal_device_poll_rx_channels(xge_hal_device_t *hldev, int *got_rx)
{
	xge_list_t *item;
	xge_hal_channel_t *channel;

	/* for each opened rx channel */
	xge_list_for_each(item, &hldev->ring_channels) {
	    if (hldev->terminating)
	        return XGE_HAL_OK;
	    channel = xge_container_of(item, xge_hal_channel_t, item);
	    (void) xge_hal_device_poll_rx_channel(channel, got_rx);
	}

	return XGE_HAL_OK;
}

/**
 * xge_hal_device_poll_tx_channels - Poll Tx channels for completed
 * descriptors and process the same.
 * @hldev: HAL device handle.
 * @got_tx: Buffer to return flag set if transmit is ready
 *
 * The function polls the Tx channels for the completed descriptors and calls
 * the upper-layer driver (ULD) via supplied completion callback.
 *
 * Returns: XGE_HAL_OK, if the polling is completed successful.
 * XGE_HAL_COMPLETIONS_REMAIN: There are still more completed
 * descriptors available which are yet to be processed.
 *
 * See also: xge_hal_device_poll_rx_channels(), xge_hal_device_continue_irq().
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE xge_hal_status_e
xge_hal_device_poll_tx_channels(xge_hal_device_t *hldev, int *got_tx)
{
	xge_list_t *item;
	xge_hal_channel_t *channel;

	/* for each opened tx channel */
	xge_list_for_each(item, &hldev->fifo_channels) {
	    if (hldev->terminating)
	        return XGE_HAL_OK;
	    channel = xge_container_of(item, xge_hal_channel_t, item);
	    (void) xge_hal_device_poll_tx_channel(channel, got_tx);
	}

	return XGE_HAL_OK;
}

/**
 * xge_hal_device_mask_tx - Mask Tx interrupts.
 * @hldev: HAL device handle.
 *
 * Mask Tx device interrupts.
 *
 * See also: xge_hal_device_unmask_tx(), xge_hal_device_mask_rx(),
 * xge_hal_device_clear_tx().
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_mask_tx(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *isrbar0 = (xge_hal_pci_bar0_t *)hldev->isrbar0;

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	               0xFFFFFFFFFFFFFFFFULL,
	               &isrbar0->tx_traffic_mask);
}

/**
 * xge_hal_device_mask_rx - Mask Rx interrupts.
 * @hldev: HAL device handle.
 *
 * Mask Rx device interrupts.
 *
 * See also: xge_hal_device_unmask_rx(), xge_hal_device_mask_tx(),
 * xge_hal_device_clear_rx().
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_mask_rx(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *isrbar0 = (xge_hal_pci_bar0_t *)hldev->isrbar0;

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	               0xFFFFFFFFFFFFFFFFULL,
	               &isrbar0->rx_traffic_mask);
}

/**
 * xge_hal_device_mask_all - Mask all device interrupts.
 * @hldev: HAL device handle.
 *
 * Mask all device interrupts.
 *
 * See also: xge_hal_device_unmask_all()
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_mask_all(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *isrbar0 = (xge_hal_pci_bar0_t *)hldev->isrbar0;

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	               0xFFFFFFFFFFFFFFFFULL,
	               &isrbar0->general_int_mask);
}

/**
 * xge_hal_device_unmask_tx - Unmask Tx interrupts.
 * @hldev: HAL device handle.
 *
 * Unmask Tx device interrupts.
 *
 * See also: xge_hal_device_mask_tx(), xge_hal_device_clear_tx().
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_unmask_tx(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *isrbar0 = (xge_hal_pci_bar0_t *)hldev->isrbar0;

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	               0x0ULL,
	               &isrbar0->tx_traffic_mask);
}

/**
 * xge_hal_device_unmask_rx - Unmask Rx interrupts.
 * @hldev: HAL device handle.
 *
 * Unmask Rx device interrupts.
 *
 * See also: xge_hal_device_mask_rx(), xge_hal_device_clear_rx().
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_unmask_rx(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *isrbar0 = (xge_hal_pci_bar0_t *)hldev->isrbar0;

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	               0x0ULL,
	               &isrbar0->rx_traffic_mask);
}

/**
 * xge_hal_device_unmask_all - Unmask all device interrupts.
 * @hldev: HAL device handle.
 *
 * Unmask all device interrupts.
 *
 * See also: xge_hal_device_mask_all()
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE void
xge_hal_device_unmask_all(xge_hal_device_t *hldev)
{
	xge_hal_pci_bar0_t *isrbar0 = (xge_hal_pci_bar0_t *)hldev->isrbar0;

	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	               0x0ULL,
	               &isrbar0->general_int_mask);
}


/**
 * xge_hal_device_continue_irq - Continue handling IRQ: process all
 * completed descriptors.
 * @hldev: HAL device handle.
 *
 * Process completed descriptors and unmask the device interrupts.
 *
 * The xge_hal_device_continue_irq() walks all open channels
 * and calls upper-layer driver (ULD) via supplied completion
 * callback. Note that the completion callback is specified at channel open
 * time, see xge_hal_channel_open().
 *
 * Note that the xge_hal_device_continue_irq is part of the _fast_ path.
 * To optimize the processing, the function does _not_ check for
 * errors and alarms.
 *
 * The latter is done in a polling fashion, via xge_hal_device_poll().
 *
 * Returns: XGE_HAL_OK.
 *
 * See also: xge_hal_device_handle_irq(), xge_hal_device_poll(),
 * xge_hal_ring_dtr_next_completed(),
 * xge_hal_fifo_dtr_next_completed(), xge_hal_channel_callback_f{}.
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE xge_hal_status_e
xge_hal_device_continue_irq(xge_hal_device_t *hldev)
{
	int got_rx = 1, got_tx = 1;
	int isr_polling_cnt = hldev->config.isr_polling_cnt;
	int count = 0;

	do
	{
	    if (got_rx)
	        (void) xge_hal_device_poll_rx_channels(hldev, &got_rx);
	    if (got_tx && hldev->tti_enabled)
	        (void) xge_hal_device_poll_tx_channels(hldev, &got_tx);

	    if (!got_rx && !got_tx)
	        break;

	    count += (got_rx + got_tx);
	}while (isr_polling_cnt--);

	if (!count)
	    hldev->stats.sw_dev_info_stats.not_traffic_intr_cnt++;

	return XGE_HAL_OK;
}

/**
 * xge_hal_device_handle_irq - Handle device IRQ.
 * @hldev: HAL device handle.
 *
 * Perform the complete handling of the line interrupt. The function
 * performs two calls.
 * First it uses xge_hal_device_begin_irq() to  check the reason for
 * the interrupt and mask the device interrupts.
 * Second, it calls xge_hal_device_continue_irq() to process all
 * completed descriptors and re-enable the interrupts.
 *
 * Returns: XGE_HAL_OK - success;
 * XGE_HAL_ERR_WRONG_IRQ - (shared) IRQ produced by other device.
 *
 * See also: xge_hal_device_begin_irq(), xge_hal_device_continue_irq().
 */
__HAL_STATIC_DEVICE __HAL_INLINE_DEVICE xge_hal_status_e
xge_hal_device_handle_irq(xge_hal_device_t *hldev)
{
	u64 reason;
	xge_hal_status_e status;

	xge_hal_device_mask_all(hldev);

	status = xge_hal_device_begin_irq(hldev, &reason);
	if (status != XGE_HAL_OK) {
	    xge_hal_device_unmask_all(hldev);
	    return status;
	}

	if (reason & XGE_HAL_GEN_INTR_RXTRAFFIC) {
	    xge_hal_device_clear_rx(hldev);
	}

	status = xge_hal_device_continue_irq(hldev);

	xge_hal_device_clear_tx(hldev);

	xge_hal_device_unmask_all(hldev);

	return status;
}

#if defined(XGE_HAL_CONFIG_LRO)


__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL int
__hal_lro_check_for_session_match(lro_t *lro, tcplro_t *tcp, iplro_t *ip)
{

	/* Match Source address field */
	if ((lro->ip_hdr->saddr != ip->saddr))
	    return XGE_HAL_FAIL;

	/* Match Destination address field */
	if ((lro->ip_hdr->daddr != ip->daddr))
	    return XGE_HAL_FAIL;

	/* Match Source Port field */
	if ((lro->tcp_hdr->source != tcp->source))
	    return XGE_HAL_FAIL;

	/* Match Destination Port field */
	if ((lro->tcp_hdr->dest != tcp->dest))
	    return XGE_HAL_FAIL;
	    
	return XGE_HAL_OK;
}

/*
 * __hal_tcp_seg_len: Find the tcp seg len.
 * @ip: ip header.
 * @tcp: tcp header.
 * returns: Tcp seg length.
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL u16
__hal_tcp_seg_len(iplro_t *ip, tcplro_t *tcp)
{
	u16 ret;

	ret =  (xge_os_ntohs(ip->tot_len) -
	       ((ip->version_ihl & 0x0F)<<2) -
	       ((tcp->doff_res)>>2));
	return (ret);
}

/*
 * __hal_ip_lro_capable: Finds whether ip is lro capable.
 * @ip: ip header.
 * @ext_info:  descriptor info.
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_ip_lro_capable(iplro_t *ip,
	         xge_hal_dtr_info_t *ext_info)
{

#ifdef XGE_LL_DEBUG_DUMP_PKT
	    {
	        u16 i;
	        u8 ch, *iph = (u8 *)ip;

	        xge_debug_ring(XGE_TRACE, "Dump Ip:" );
	        for (i =0; i < 40; i++) {
	            ch = ntohs(*((u8 *)(iph + i)) );
	            printf("i:%d %02x, ",i,ch);
	        }
	    }
#endif

	if (ip->version_ihl != IP_FAST_PATH_HDR_MASK) {
	    xge_debug_ring(XGE_ERR, "iphdr !=45 :%d",ip->version_ihl);
	    return XGE_HAL_FAIL;
	}

	if (ext_info->proto & XGE_HAL_FRAME_PROTO_IP_FRAGMENTED) {
	    xge_debug_ring(XGE_ERR, "IP fragmented");
	    return XGE_HAL_FAIL;
	}

	return XGE_HAL_OK;
}

/*
 * __hal_tcp_lro_capable: Finds whether tcp is lro capable.
 * @ip: ip header.
 * @tcp: tcp header.
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_tcp_lro_capable(iplro_t *ip, tcplro_t *tcp, lro_t *lro, int *ts_off)
{
#ifdef XGE_LL_DEBUG_DUMP_PKT
	    {
	        u8 ch;
	        u16 i;

	        xge_debug_ring(XGE_TRACE, "Dump Tcp:" );
	        for (i =0; i < 20; i++) {
	            ch = ntohs(*((u8 *)((u8 *)tcp + i)) );
	            xge_os_printf("i:%d %02x, ",i,ch);
	        }
	    }
#endif
	if ((TCP_FAST_PATH_HDR_MASK2 != tcp->ctrl) &&
	    (TCP_FAST_PATH_HDR_MASK3 != tcp->ctrl))
	    goto _exit_fail;

	*ts_off = -1;
	if (TCP_FAST_PATH_HDR_MASK1 != tcp->doff_res) {
	    u16 tcp_hdr_len = tcp->doff_res >> 2; /* TCP header len */
	    u16 off = 20; /* Start of tcp options */
	    int i, diff; 

	    /* Does Packet can contain time stamp */
	    if (tcp_hdr_len < 32) {
	        /*
	         * If the session is not opened, we can consider
	         * this packet for LRO
	         */
	        if (lro == NULL)
	            return XGE_HAL_OK;

	        goto _exit_fail;
	    }

	    /* Ignore No-operation 0x1 */
	    while (((u8 *)tcp)[off] == 0x1)
	        off++;

	    /* Next option == Timestamp */
	    if (((u8 *)tcp)[off] != 0x8) {
	        /*
	         * If the session ie not opened, we can consider
	         * this packet for LRO
	         */
	        if (lro == NULL)
	            return XGE_HAL_OK;

	        goto _exit_fail;
	    }

	    *ts_off = off;
	    if (lro == NULL)
	        return XGE_HAL_OK;

	    /*
	     * Now the session is opened. If the LRO frame doesn't
	     * have time stamp, we cannot consider current packet for
	     * LRO.
	     */
	    if (lro->ts_off == -1) {
	        xge_debug_ring(XGE_ERR, "Pkt received with time stamp after session opened with no time stamp : %02x %02x", tcp->doff_res, tcp->ctrl);
	        return XGE_HAL_FAIL;
	    }

	    /*
	     * If the difference is greater than three, then there are
	     * more options possible.
	     * else, there are two cases:
	     * case 1: remaining are padding bytes.
	     * case 2: remaining can contain options or padding
	     */
	    off += ((u8 *)tcp)[off+1];
	    diff = tcp_hdr_len - off;
	    if (diff > 3) {
	        /*
	         * Probably contains more options.
	         */
	        xge_debug_ring(XGE_ERR, "tcphdr not fastpth : pkt received with tcp options in addition to time stamp after the session is opened %02x %02x ", tcp->doff_res,   tcp->ctrl);
	        return XGE_HAL_FAIL;
	    }

	    for (i = 0; i < diff; i++) {
	        u8 byte = ((u8 *)tcp)[off+i];

	        /* Ignore No-operation 0x1 */
	        if ((byte == 0x0) || (byte == 0x1)) 
	            continue;
	        xge_debug_ring(XGE_ERR, "tcphdr not fastpth : pkt received with tcp options in addition to time stamp after the session is opened %02x %02x ", tcp->doff_res,   tcp->ctrl);
	        return XGE_HAL_FAIL;
	    }
	
	    /*
	     * Update the time stamp of LRO frame.
	     */
	    xge_os_memcpy(((char *)lro->tcp_hdr + lro->ts_off + 2),
	            (char *)((char *)tcp + (*ts_off) + 2), 8);
	}

	return XGE_HAL_OK;

_exit_fail:
	xge_debug_ring(XGE_TRACE,   "tcphdr not fastpth %02x %02x", tcp->doff_res, tcp->ctrl);
	return XGE_HAL_FAIL;

}

/*
 * __hal_lro_capable: Finds whether frame is lro capable.
 * @buffer: Ethernet frame.
 * @ip: ip frame.
 * @tcp: tcp frame.
 * @ext_info: Descriptor info.
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_lro_capable( u8 *buffer,
	       iplro_t **ip,
	       tcplro_t **tcp,
	   xge_hal_dtr_info_t *ext_info)
{
	u8 ip_off, ip_length;

	if (!(ext_info->proto & XGE_HAL_FRAME_PROTO_TCP)) {
	    xge_debug_ring(XGE_ERR, "Cant do lro %d", ext_info->proto);
	    return XGE_HAL_FAIL;
	}

  if ( !*ip )
  {
#ifdef XGE_LL_DEBUG_DUMP_PKT
	    {
	        u8 ch;
	        u16 i;

	        xge_os_printf("Dump Eth:" );
	        for (i =0; i < 60; i++) {
	            ch = ntohs(*((u8 *)(buffer + i)) );
	            xge_os_printf("i:%d %02x, ",i,ch);
	        }
	    }
#endif

	switch (ext_info->frame) {
	case XGE_HAL_FRAME_TYPE_DIX:
	  ip_off = XGE_HAL_HEADER_ETHERNET_II_802_3_SIZE;
	  break;
	case XGE_HAL_FRAME_TYPE_LLC:
	  ip_off = (XGE_HAL_HEADER_ETHERNET_II_802_3_SIZE   +
	            XGE_HAL_HEADER_802_2_SIZE);
	  break;
	case XGE_HAL_FRAME_TYPE_SNAP:
	  ip_off = (XGE_HAL_HEADER_ETHERNET_II_802_3_SIZE   +
	            XGE_HAL_HEADER_SNAP_SIZE);
	  break;
	default: // XGE_HAL_FRAME_TYPE_IPX, etc.
	  return XGE_HAL_FAIL;
	}


	if (ext_info->proto & XGE_HAL_FRAME_PROTO_VLAN_TAGGED) {
	  ip_off += XGE_HAL_HEADER_VLAN_SIZE;
	}

	/* Grab ip, tcp headers */
	*ip = (iplro_t *)((char*)buffer + ip_off);
  } /* !*ip */

	ip_length = (u8)((*ip)->version_ihl & 0x0F);
	ip_length = ip_length <<2;
	*tcp = (tcplro_t *)((char *)*ip + ip_length);

	xge_debug_ring(XGE_TRACE, "ip_length:%d ip:"XGE_OS_LLXFMT
	       " tcp:"XGE_OS_LLXFMT"", (int)ip_length,
	    (unsigned long long)(ulong_t)*ip, (unsigned long long)(ulong_t)*tcp);

	return XGE_HAL_OK;

}


/*
 * __hal_open_lro_session: Open a new LRO session.
 * @buffer: Ethernet frame.
 * @ip: ip header.
 * @tcp: tcp header.
 * @lro: lro pointer
 * @ext_info: Descriptor info.
 * @hldev: Hal context.
 * @ring_lro: LRO descriptor per rx ring.
 * @slot: Bucket no.
 * @tcp_seg_len: Length of tcp segment.
 * @ts_off: time stamp offset in the packet.
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
__hal_open_lro_session (u8 *buffer, iplro_t *ip, tcplro_t *tcp, lro_t **lro,
	        xge_hal_device_t *hldev, xge_hal_lro_desc_t *ring_lro, int slot,
	  u32 tcp_seg_len, int  ts_off)
{

	lro_t *lro_new = &ring_lro->lro_pool[slot];

	lro_new->in_use         =   1;
	lro_new->ll_hdr         =   buffer;
	lro_new->ip_hdr         =   ip;
	lro_new->tcp_hdr        =   tcp;
	lro_new->tcp_next_seq_num   =   tcp_seg_len + xge_os_ntohl(
	                            tcp->seq);
	lro_new->tcp_seq_num        =   tcp->seq;
	lro_new->tcp_ack_num        =   tcp->ack_seq;
	lro_new->sg_num         =   1;
	lro_new->total_length       =   xge_os_ntohs(ip->tot_len);
	lro_new->frags_len      =   0;
	lro_new->ts_off         =   ts_off;

	hldev->stats.sw_dev_info_stats.tot_frms_lroised++;
	hldev->stats.sw_dev_info_stats.tot_lro_sessions++;

	*lro = ring_lro->lro_recent = lro_new;
	return;
}
/*
 * __hal_lro_get_free_slot: Get a free LRO bucket.
 * @ring_lro: LRO descriptor per ring.
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL int
__hal_lro_get_free_slot (xge_hal_lro_desc_t *ring_lro)
{
	int i;

	for (i = 0; i < XGE_HAL_LRO_MAX_BUCKETS; i++) {
	    lro_t *lro_temp = &ring_lro->lro_pool[i];

	    if (!lro_temp->in_use)
	        return i;
	}
	return -1;  
}

/*
 * __hal_get_lro_session: Gets matching LRO session or creates one.
 * @eth_hdr:    Ethernet header.
 * @ip: ip header.
 * @tcp: tcp header.
 * @lro: lro pointer
 * @ext_info: Descriptor info.
 * @hldev: Hal context.
 * @ring_lro: LRO descriptor per rx ring
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_get_lro_session (u8 *eth_hdr,
	           iplro_t *ip,
	           tcplro_t *tcp,
	           lro_t **lro,
	           xge_hal_dtr_info_t *ext_info,
	           xge_hal_device_t *hldev,
	           xge_hal_lro_desc_t   *ring_lro,
	           lro_t **lro_end3 /* Valid only when ret=END_3 */)
{
	lro_t *lro_match;
	int i, free_slot = -1;
	u32 tcp_seg_len;
	int ts_off = -1;

	*lro = lro_match = NULL;
	/*
	 * Compare the incoming frame with the lro session left from the 
	 * previous call.  There is a good chance that this incoming frame
	 * matches the lro session.
	 */
	if (ring_lro->lro_recent && ring_lro->lro_recent->in_use)   {
	    if (__hal_lro_check_for_session_match(ring_lro->lro_recent,
	                          tcp, ip)
	                        == XGE_HAL_OK)
	        lro_match = ring_lro->lro_recent;
	}
	
	if (!lro_match) {
	    /*
	     * Search in the pool of LROs for the session that matches 
	     * the incoming frame.
	     */
	    for (i = 0; i < XGE_HAL_LRO_MAX_BUCKETS; i++) {
	        lro_t *lro_temp = &ring_lro->lro_pool[i];

	        if (!lro_temp->in_use) {
	            if (free_slot == -1)
	                free_slot = i;
	            continue;
	        }   

	        if (__hal_lro_check_for_session_match(lro_temp, tcp,
	                          ip) == XGE_HAL_OK) {
	            lro_match = lro_temp;
	            break;
	        }
	    }
	}

	
	if (lro_match) {
	    /*
	     * Matching LRO Session found
	     */         
	    *lro = lro_match;

	    if (lro_match->tcp_next_seq_num != xge_os_ntohl(tcp->seq)) {
	 xge_debug_ring(XGE_ERR,    "**retransmit  **"
	                    "found***");
	        hldev->stats.sw_dev_info_stats.lro_out_of_seq_pkt_cnt++;
	        return XGE_HAL_INF_LRO_END_2;
	    }

	    if (XGE_HAL_OK != __hal_ip_lro_capable(ip, ext_info))
	{
	        return XGE_HAL_INF_LRO_END_2;
	}

	    if (XGE_HAL_OK != __hal_tcp_lro_capable(ip, tcp, lro_match,
	                        &ts_off)) {
	        /*
	         * Close the current session and open a new
	         * LRO session with this packet,
	         * provided it has tcp payload
	         */ 
	        tcp_seg_len = __hal_tcp_seg_len(ip, tcp);
	        if (tcp_seg_len == 0)
	  {
	            return XGE_HAL_INF_LRO_END_2;
	  }

	        /* Get a free bucket  */
	        free_slot = __hal_lro_get_free_slot(ring_lro);
	        if (free_slot == -1)
	  {
	            return XGE_HAL_INF_LRO_END_2;
	  }

	        /* 
	         * Open a new LRO session
	         */
	        __hal_open_lro_session (eth_hdr,    ip, tcp, lro_end3,
	                    hldev, ring_lro, free_slot, tcp_seg_len,
	                    ts_off);

	        return XGE_HAL_INF_LRO_END_3;
	    }

	            /*
	     * The frame is good, in-sequence, can be LRO-ed;
	     * take its (latest) ACK - unless it is a dupack.
	     * Note: to be exact need to check window size as well..
	    */
	    if (lro_match->tcp_ack_num == tcp->ack_seq &&
	        lro_match->tcp_seq_num == tcp->seq) {
	        hldev->stats.sw_dev_info_stats.lro_dup_pkt_cnt++;
	        return XGE_HAL_INF_LRO_END_2;
	    }

	    lro_match->tcp_seq_num = tcp->seq;
	    lro_match->tcp_ack_num = tcp->ack_seq;
	    lro_match->frags_len += __hal_tcp_seg_len(ip, tcp);

	    ring_lro->lro_recent =  lro_match;
	
	    return XGE_HAL_INF_LRO_CONT;
	}

	/* ********** New Session ***************/
	if (free_slot == -1)
	    return XGE_HAL_INF_LRO_UNCAPABLE;
	
	if (XGE_HAL_FAIL == __hal_ip_lro_capable(ip, ext_info))
	    return XGE_HAL_INF_LRO_UNCAPABLE;

	if (XGE_HAL_FAIL == __hal_tcp_lro_capable(ip, tcp, NULL, &ts_off))
	    return XGE_HAL_INF_LRO_UNCAPABLE;
	    
	xge_debug_ring(XGE_TRACE, "Creating lro session.");

	/*
	 * Open a LRO session, provided the packet contains payload.
	 */
	tcp_seg_len = __hal_tcp_seg_len(ip, tcp);
	if (tcp_seg_len == 0)
	    return XGE_HAL_INF_LRO_UNCAPABLE;

	__hal_open_lro_session (eth_hdr,    ip, tcp, lro, hldev, ring_lro, free_slot,
	            tcp_seg_len, ts_off);

	return XGE_HAL_INF_LRO_BEGIN;
}

/*
 * __hal_lro_under_optimal_thresh: Finds whether combined session is optimal.
 * @ip: ip header.
 * @tcp: tcp header.
 * @lro: lro pointer
 * @hldev: Hal context.
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_lro_under_optimal_thresh (iplro_t *ip,
	                tcplro_t *tcp,
	            lro_t *lro,
	            xge_hal_device_t *hldev)
{
	if (!lro) return XGE_HAL_FAIL;

	if ((lro->total_length + __hal_tcp_seg_len(ip, tcp) ) > 
	                    hldev->config.lro_frm_len) {
	    xge_debug_ring(XGE_TRACE, "Max LRO frame len exceeded:"
	     "max length %d ", hldev->config.lro_frm_len);
	    hldev->stats.sw_dev_info_stats.lro_frm_len_exceed_cnt++;
	    return XGE_HAL_FAIL;
	}

	if (lro->sg_num == hldev->config.lro_sg_size) {
	    xge_debug_ring(XGE_TRACE, "Max sg count exceeded:"
	             "max sg %d ", hldev->config.lro_sg_size);
	    hldev->stats.sw_dev_info_stats.lro_sg_exceed_cnt++;
	    return XGE_HAL_FAIL;
	}

	return XGE_HAL_OK;
}

/*
 * __hal_collapse_ip_hdr: Collapses ip header.
 * @ip: ip header.
 * @tcp: tcp header.
 * @lro: lro pointer
 * @hldev: Hal context.
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_collapse_ip_hdr ( iplro_t *ip,
	        tcplro_t *tcp,
	        lro_t *lro,
	        xge_hal_device_t *hldev)
{

	lro->total_length += __hal_tcp_seg_len(ip, tcp);

	/* May be we have to handle time stamps or more options */

	return XGE_HAL_OK;

}

/*
 * __hal_collapse_tcp_hdr: Collapses tcp header.
 * @ip: ip header.
 * @tcp: tcp header.
 * @lro: lro pointer
 * @hldev: Hal context.
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_collapse_tcp_hdr ( iplro_t *ip,
	         tcplro_t *tcp,
	         lro_t *lro,
	         xge_hal_device_t *hldev)
{
	lro->tcp_next_seq_num += __hal_tcp_seg_len(ip, tcp);
	return XGE_HAL_OK;

}

/*
 * __hal_append_lro: Appends new frame to existing LRO session.
 * @ip: ip header.
 * @tcp: IN tcp header, OUT tcp payload.
 * @seg_len: tcp payload length.
 * @lro: lro pointer
 * @hldev: Hal context.
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_append_lro(iplro_t *ip,
	     tcplro_t **tcp,
	     u32 *seg_len,
	     lro_t *lro,
	     xge_hal_device_t *hldev)
{
	(void) __hal_collapse_ip_hdr(ip, *tcp,  lro, hldev);
	(void) __hal_collapse_tcp_hdr(ip, *tcp, lro, hldev);
	// Update mbuf chain will be done in ll driver.
	// xge_hal_accumulate_large_rx on success of appending new frame to
	// lro will return to ll driver tcpdata pointer, and tcp payload length.
	// along with return code lro frame appended.

	lro->sg_num++;
	*seg_len = __hal_tcp_seg_len(ip, *tcp);
	*tcp = (tcplro_t *)((char *)*tcp    + (((*tcp)->doff_res)>>2));

	return XGE_HAL_OK;

}

/**
 * __xge_hal_accumulate_large_rx:   LRO a given frame
 * frames
 * @ring: rx ring number
 * @eth_hdr: ethernet header.
 * @ip_hdr: ip header (optional)
 * @tcp: tcp header.
 * @seglen: packet length.
 * @p_lro: lro pointer.
 * @ext_info: descriptor info, see xge_hal_dtr_info_t{}.
 * @hldev: HAL device.
 * @lro_end3: for lro_end3 output
 *
 * LRO the newly received frame, i.e. attach it (if possible) to the
 * already accumulated (i.e., already LRO-ed) received frames (if any),
 * to form one super-sized frame for the subsequent processing
 * by the stack.
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
xge_hal_lro_process_rx(int ring, u8 *eth_hdr, u8 *ip_hdr, tcplro_t **tcp,
	                   u32 *seglen, lro_t **p_lro,
	                   xge_hal_dtr_info_t *ext_info, xge_hal_device_t *hldev,
	                   lro_t **lro_end3)
{
	iplro_t *ip = (iplro_t *)ip_hdr;
	xge_hal_status_e ret;
	lro_t *lro;

	xge_debug_ring(XGE_TRACE, "Entered accumu lro. ");
	if (XGE_HAL_OK != __hal_lro_capable(eth_hdr, &ip, (tcplro_t **)tcp,
	                                  ext_info))
	    return XGE_HAL_INF_LRO_UNCAPABLE;

	/*
	 * This function shall get matching LRO or else
	 * create one and return it
	 */
	ret = __hal_get_lro_session(eth_hdr, ip, (tcplro_t *)*tcp,
	                          p_lro, ext_info, hldev,   &hldev->lro_desc[ring],
	                          lro_end3);
	xge_debug_ring(XGE_TRACE, "ret from get_lro:%d ",ret);
	lro = *p_lro;
	if (XGE_HAL_INF_LRO_CONT == ret) {
	    if (XGE_HAL_OK == __hal_lro_under_optimal_thresh(ip,
	                    (tcplro_t *)*tcp, lro, hldev)) {
	        (void) __hal_append_lro(ip,(tcplro_t **) tcp, seglen,
	                         lro, hldev);
	        hldev->stats.sw_dev_info_stats.tot_frms_lroised++;

	        if (lro->sg_num >= hldev->config.lro_sg_size) {
	            hldev->stats.sw_dev_info_stats.lro_sg_exceed_cnt++;
	            ret = XGE_HAL_INF_LRO_END_1;
	        }

	    } else ret = XGE_HAL_INF_LRO_END_2;
	}

	/*
	 * Since its time to flush,
	 * update ip header so that it can be sent up
	 */
	if ((ret == XGE_HAL_INF_LRO_END_1) ||
	    (ret == XGE_HAL_INF_LRO_END_2) ||
	    (ret == XGE_HAL_INF_LRO_END_3)) {
	    lro->ip_hdr->tot_len = xge_os_htons((*p_lro)->total_length);
	    lro->ip_hdr->check = xge_os_htons(0);
	    lro->ip_hdr->check = XGE_LL_IP_FAST_CSUM(((u8 *)(lro->ip_hdr)),
	                (lro->ip_hdr->version_ihl & 0x0F));
	    lro->tcp_hdr->ack_seq = lro->tcp_ack_num;
	}

	return (ret);
}

/**
 * xge_hal_accumulate_large_rx: LRO a given frame
 * frames
 * @buffer: Ethernet frame.
 * @tcp: tcp header.
 * @seglen: packet length.
 * @p_lro: lro pointer.
 * @ext_info: descriptor info, see xge_hal_dtr_info_t{}.
 * @hldev: HAL device.
 * @lro_end3: for lro_end3 output
 *
 * LRO the newly received frame, i.e. attach it (if possible) to the
 * already accumulated (i.e., already LRO-ed) received frames (if any),
 * to form one super-sized frame for the subsequent processing
 * by the stack.
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
xge_hal_accumulate_large_rx(u8 *buffer, tcplro_t **tcp, u32 *seglen,
lro_t **p_lro, xge_hal_dtr_info_t *ext_info, xge_hal_device_t *hldev,
lro_t **lro_end3)
{
  int ring = 0;
  return xge_hal_lro_process_rx(ring, buffer, NULL, tcp, seglen, p_lro,
	                            ext_info, hldev, lro_end3);
}

/**
 * xge_hal_lro_close_session: Close LRO session
 * @lro: LRO Session.
 * @hldev: HAL Context.
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
xge_hal_lro_close_session (lro_t *lro)
{
	lro->in_use = 0;
}

/**
 * xge_hal_lro_next_session: Returns next LRO session in the list or NULL
 *                  if none exists.
 * @hldev: HAL Context.
 * @ring: rx ring number.
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL lro_t *
xge_hal_lro_next_session (xge_hal_device_t *hldev, int ring)
{
xge_hal_lro_desc_t *ring_lro = &hldev->lro_desc[ring];
	int i;
	int start_idx = ring_lro->lro_next_idx;

	for(i = start_idx; i < XGE_HAL_LRO_MAX_BUCKETS; i++) {
	    lro_t *lro = &ring_lro->lro_pool[i];

	    if (!lro->in_use)
	        continue;

	    lro->ip_hdr->tot_len = xge_os_htons(lro->total_length);
	    lro->ip_hdr->check = xge_os_htons(0);
	    lro->ip_hdr->check = XGE_LL_IP_FAST_CSUM(((u8 *)(lro->ip_hdr)),
	                            (lro->ip_hdr->version_ihl & 0x0F));
	    ring_lro->lro_next_idx  = i + 1;
	    return lro;
	}

	ring_lro->lro_next_idx  = 0;
	return NULL;

}

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL lro_t *
xge_hal_lro_get_next_session(xge_hal_device_t *hldev)
{
  int ring = 0; /* assume default ring=0 */
  return xge_hal_lro_next_session(hldev, ring);
}
#endif
