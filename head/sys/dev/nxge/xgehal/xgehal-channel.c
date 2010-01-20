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

#include <dev/nxge/include/xgehal-channel.h>
#include <dev/nxge/include/xgehal-fifo.h>
#include <dev/nxge/include/xgehal-ring.h>
#include <dev/nxge/include/xgehal-device.h>
#include <dev/nxge/include/xgehal-regs.h>

/*
 * __hal_channel_dtr_next_reservelist
 *
 * Walking through the all available DTRs.
 */
static xge_hal_status_e
__hal_channel_dtr_next_reservelist(xge_hal_channel_h channelh,
	    xge_hal_dtr_h *dtrh)
{
	xge_hal_channel_t *channel = (xge_hal_channel_t *)channelh;

	if (channel->reserve_top >= channel->reserve_length) {
	    return XGE_HAL_INF_NO_MORE_FREED_DESCRIPTORS;
	}

	*dtrh = channel->reserve_arr[channel->reserve_top++];

	return XGE_HAL_OK;
}

/*
 * __hal_channel_dtr_next_freelist
 *
 * Walking through the "freed" DTRs.
 */
static xge_hal_status_e
__hal_channel_dtr_next_freelist(xge_hal_channel_h channelh, xge_hal_dtr_h *dtrh)
{
	xge_hal_channel_t *channel = (xge_hal_channel_t *)channelh;

	if (channel->reserve_initial == channel->free_length) {
	    return XGE_HAL_INF_NO_MORE_FREED_DESCRIPTORS;
	}

	*dtrh = channel->free_arr[channel->free_length++];

	return XGE_HAL_OK;
}

/*
 * __hal_channel_dtr_next_not_completed - Get the _next_ posted but
 *                                     not completed descriptor.
 *
 * Walking through the "not completed" DTRs.
 */
static xge_hal_status_e
__hal_channel_dtr_next_not_completed(xge_hal_channel_h channelh,
	    xge_hal_dtr_h *dtrh)
{
	xge_hal_ring_rxd_1_t *rxdp; /* doesn't matter 1, 3 or 5... */
	__hal_channel_dtr_try_complete(channelh, dtrh);
	if (*dtrh == NULL) {
	    return XGE_HAL_INF_NO_MORE_COMPLETED_DESCRIPTORS;
	}

	rxdp = (xge_hal_ring_rxd_1_t *)*dtrh;
	xge_assert(rxdp->host_control!=0);

	__hal_channel_dtr_complete(channelh);

	return XGE_HAL_OK;
}

xge_hal_channel_t*
__hal_channel_allocate(xge_hal_device_h devh, int post_qid,
	    xge_hal_channel_type_e type)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_channel_t *channel;
	int size = 0;

	switch(type) {
	    case XGE_HAL_CHANNEL_TYPE_FIFO:
	        xge_assert(post_qid + 1 >= XGE_HAL_MIN_FIFO_NUM &&
	             post_qid + 1 <= XGE_HAL_MAX_FIFO_NUM);
	        size = sizeof(xge_hal_fifo_t);
	        break;
	    case XGE_HAL_CHANNEL_TYPE_RING:
	        xge_assert(post_qid + 1 >= XGE_HAL_MIN_RING_NUM &&
	            post_qid + 1 <= XGE_HAL_MAX_RING_NUM);
	        size = sizeof(xge_hal_ring_t);
	        break;
	    default :
	        xge_assert(size);
	        break;

	}


	/* allocate FIFO channel */
	channel = (xge_hal_channel_t *) xge_os_malloc(hldev->pdev, size);
	if (channel == NULL) {
	    return NULL;
	}
	xge_os_memzero(channel, size);

	channel->pdev       = hldev->pdev;
	channel->regh0      = hldev->regh0;
	channel->regh1      = hldev->regh1;
	channel->type       = type;
	channel->devh       = devh;
	channel->post_qid   = post_qid;
	channel->compl_qid  = 0;

	return channel;
}

void __hal_channel_free(xge_hal_channel_t *channel)
{
	int size = 0;

	xge_assert(channel->pdev);

	switch(channel->type) {
	    case XGE_HAL_CHANNEL_TYPE_FIFO:
	        size = sizeof(xge_hal_fifo_t);
	        break;
	    case XGE_HAL_CHANNEL_TYPE_RING:
	        size = sizeof(xge_hal_ring_t);
	        break;
	    case XGE_HAL_CHANNEL_TYPE_SEND_QUEUE:
	    case XGE_HAL_CHANNEL_TYPE_RECEIVE_QUEUE:
	    case XGE_HAL_CHANNEL_TYPE_COMPLETION_QUEUE:
	    case XGE_HAL_CHANNEL_TYPE_UP_MESSAGE_QUEUE:
	    case XGE_HAL_CHANNEL_TYPE_DOWN_MESSAGE_QUEUE:
	        xge_assert(size);
	        break;
	    default:
	        break;
	}

	xge_os_free(channel->pdev, channel, size);
}

xge_hal_status_e
__hal_channel_initialize (xge_hal_channel_h channelh,
	    xge_hal_channel_attr_t *attr, void **reserve_arr,
	    int reserve_initial, int reserve_max, int reserve_threshold)
{
	xge_hal_channel_t *channel = (xge_hal_channel_t *)channelh;
	xge_hal_device_t *hldev;

	hldev = (xge_hal_device_t *)channel->devh;

	channel->dtr_term = attr->dtr_term;
	channel->dtr_init = attr->dtr_init;
	channel->callback = attr->callback;
	channel->userdata = attr->userdata;
	channel->flags = attr->flags;
	channel->per_dtr_space = attr->per_dtr_space;

	channel->reserve_arr = reserve_arr;
	channel->reserve_initial = reserve_initial;
	channel->reserve_max = reserve_max;
	channel->reserve_length = channel->reserve_initial;
	channel->reserve_threshold = reserve_threshold;
	channel->reserve_top = 0;
	channel->saved_arr = (void **) xge_os_malloc(hldev->pdev,
	                   sizeof(void*)*channel->reserve_max);
	if (channel->saved_arr == NULL) {
	    return XGE_HAL_ERR_OUT_OF_MEMORY;
	}
	xge_os_memzero(channel->saved_arr, sizeof(void*)*channel->reserve_max);
	channel->free_arr = channel->saved_arr;
	channel->free_length = channel->reserve_initial;
	channel->work_arr = (void **) xge_os_malloc(hldev->pdev,
	              sizeof(void*)*channel->reserve_max);
	if (channel->work_arr == NULL) {
	    return XGE_HAL_ERR_OUT_OF_MEMORY;
	}
	xge_os_memzero(channel->work_arr,
	                   sizeof(void*)*channel->reserve_max);
	channel->post_index = 0;
	channel->compl_index = 0;
	channel->length = channel->reserve_initial;

	channel->orig_arr = (void **) xge_os_malloc(hldev->pdev,
	                    sizeof(void*)*channel->reserve_max);
	if (channel->orig_arr == NULL)
	    return XGE_HAL_ERR_OUT_OF_MEMORY;

	xge_os_memzero(channel->orig_arr, sizeof(void*)*channel->reserve_max);

#if defined(XGE_HAL_RX_MULTI_FREE_IRQ) || defined(XGE_HAL_TX_MULTI_FREE_IRQ)
	xge_os_spin_lock_init_irq(&channel->free_lock, hldev->irqh);
#elif defined(XGE_HAL_RX_MULTI_FREE) || defined(XGE_HAL_TX_MULTI_FREE)
	xge_os_spin_lock_init(&channel->free_lock, hldev->pdev);
#endif

	return XGE_HAL_OK;
}

void __hal_channel_terminate(xge_hal_channel_h channelh)
{
	xge_hal_channel_t *channel = (xge_hal_channel_t *)channelh;
	xge_hal_device_t *hldev;

	hldev = (xge_hal_device_t *)channel->devh;

	xge_assert(channel->pdev);
	/* undo changes made at channel_initialize() */
	if (channel->work_arr) {
	    xge_os_free(channel->pdev, channel->work_arr,
	              sizeof(void*)*channel->reserve_max);
	    channel->work_arr = NULL;
	}

	if (channel->saved_arr) {
	    xge_os_free(channel->pdev, channel->saved_arr,
	              sizeof(void*)*channel->reserve_max);
	    channel->saved_arr = NULL;
	}

	if (channel->orig_arr) {
	    xge_os_free(channel->pdev, channel->orig_arr,
	              sizeof(void*)*channel->reserve_max);
	    channel->orig_arr = NULL;
	}

#if defined(XGE_HAL_RX_MULTI_FREE_IRQ) || defined(XGE_HAL_TX_MULTI_FREE_IRQ)
	xge_os_spin_lock_destroy_irq(&channel->free_lock, hldev->irqh);
#elif defined(XGE_HAL_RX_MULTI_FREE) || defined(XGE_HAL_TX_MULTI_FREE)
	xge_os_spin_lock_destroy(&channel->free_lock, hldev->pdev);
#endif
}

/**
 * xge_hal_channel_open - Open communication channel.
 * @devh: HAL device, pointer to xge_hal_device_t structure.
 * @attr: Contains attributes required to open
 *        the channel.
 * @channelh:  The channel handle. On success (XGE_HAL_OK) HAL fills
 * this "out" parameter with a valid channel handle.
 * @reopen: See  xge_hal_channel_reopen_e{}.
 *
 * Open communication channel with the device.
 *
 * HAL uses (persistent) channel configuration to allocate both channel
 * and Xframe Tx and Rx descriptors.
 * Notes:
 *     1) The channel config data is fed into HAL prior to
 *        xge_hal_channel_open().
 *
 *     2) The corresponding hardware queues must be already configured and
 *        enabled.
 *
 *     3) Either down or up queue may be omitted, in which case the channel
 *        is treated as _unidirectional_.
 *
 *     4) Post and completion queue may be the same, in which case the channel
 *        is said to have "in-band completions".
 *
 * Note that free_channels list is not protected. i.e. caller must provide
 * safe context.
 *
 * Returns: XGE_HAL_OK  - success.
 * XGE_HAL_ERR_CHANNEL_NOT_FOUND - Unable to locate the channel.
 * XGE_HAL_ERR_OUT_OF_MEMORY - Memory allocation failed.
 *
 * See also: xge_hal_channel_attr_t{}.
 * Usage: See ex_open{}.
 */
xge_hal_status_e
xge_hal_channel_open(xge_hal_device_h devh,
	         xge_hal_channel_attr_t *attr,
	         xge_hal_channel_h *channelh,
	         xge_hal_channel_reopen_e reopen)
{
	xge_list_t *item;
	int i;
	xge_hal_status_e status = XGE_HAL_OK;
	xge_hal_channel_t *channel = NULL;
	xge_hal_device_t *device = (xge_hal_device_t *)devh;

	xge_assert(device);
	xge_assert(attr);

	*channelh = NULL;

	/* find channel */
	xge_list_for_each(item, &device->free_channels) {
	    xge_hal_channel_t *tmp;

	    tmp = xge_container_of(item, xge_hal_channel_t, item);
	    if (tmp->type == attr->type &&
	    tmp->post_qid == attr->post_qid &&
	    tmp->compl_qid == attr->compl_qid) {
	        channel = tmp;
	        break;
	    }
	}

	if (channel == NULL) {
	    return XGE_HAL_ERR_CHANNEL_NOT_FOUND;
	}

	xge_assert((channel->type == XGE_HAL_CHANNEL_TYPE_FIFO) ||
	    (channel->type == XGE_HAL_CHANNEL_TYPE_RING));

	if (reopen == XGE_HAL_CHANNEL_OC_NORMAL) {
	    /* allocate memory, initialize pointers, etc */
	    switch(channel->type) {
	        case XGE_HAL_CHANNEL_TYPE_FIFO:
	            status = __hal_fifo_open(channel, attr);
	            break;
	        case XGE_HAL_CHANNEL_TYPE_RING:
	            status = __hal_ring_open(channel, attr);
	            break;
	        case XGE_HAL_CHANNEL_TYPE_SEND_QUEUE:
	        case XGE_HAL_CHANNEL_TYPE_RECEIVE_QUEUE:
	        case XGE_HAL_CHANNEL_TYPE_COMPLETION_QUEUE:
	        case XGE_HAL_CHANNEL_TYPE_UP_MESSAGE_QUEUE:
	        case XGE_HAL_CHANNEL_TYPE_DOWN_MESSAGE_QUEUE:
	            status = XGE_HAL_FAIL;
	            break;
	        default:
	            break;
	    }

	    if (status == XGE_HAL_OK) {
	        for (i = 0; i < channel->reserve_initial; i++) {
	            channel->orig_arr[i] =
	                channel->reserve_arr[i];
	        }
	    }
	    else
	        return status;
	} else {
	        xge_assert(reopen == XGE_HAL_CHANNEL_RESET_ONLY);

	    for (i = 0; i < channel->reserve_initial; i++) {
	        channel->reserve_arr[i] = channel->orig_arr[i];
	        channel->free_arr[i] = NULL;
	    }
	    channel->free_length = channel->reserve_initial;
	    channel->reserve_length = channel->reserve_initial;
	    channel->reserve_top = 0;
	    channel->post_index = 0;
	    channel->compl_index = 0;
	            if (channel->type == XGE_HAL_CHANNEL_TYPE_RING) {
	        status = __hal_ring_initial_replenish(channel,
	                              reopen);
	                    if (status != XGE_HAL_OK)
	                            return status;
	    }
	}

	/* move channel to the open state list */

	switch(channel->type) {
	    case XGE_HAL_CHANNEL_TYPE_FIFO:
	        xge_list_remove(&channel->item);
	        xge_list_insert(&channel->item, &device->fifo_channels);
	        break;
	    case XGE_HAL_CHANNEL_TYPE_RING:
	        xge_list_remove(&channel->item);
	        xge_list_insert(&channel->item, &device->ring_channels);
	        break;
	    case XGE_HAL_CHANNEL_TYPE_SEND_QUEUE:
	    case XGE_HAL_CHANNEL_TYPE_RECEIVE_QUEUE:
	    case XGE_HAL_CHANNEL_TYPE_COMPLETION_QUEUE:
	    case XGE_HAL_CHANNEL_TYPE_UP_MESSAGE_QUEUE:
	    case XGE_HAL_CHANNEL_TYPE_DOWN_MESSAGE_QUEUE:
	        xge_assert(channel->type == XGE_HAL_CHANNEL_TYPE_FIFO ||
	               channel->type == XGE_HAL_CHANNEL_TYPE_RING);
	        break;
	    default:
	        break;
	}
	channel->is_open = 1;
	channel->terminating = 0;
	/*
	 * The magic check the argument validity, has to be
	 * removed before 03/01/2005.
	 */
	channel->magic = XGE_HAL_MAGIC;

	*channelh = channel;

	return XGE_HAL_OK;
}

/**
 * xge_hal_channel_abort - Abort the channel.
 * @channelh: Channel handle.
 * @reopen: See  xge_hal_channel_reopen_e{}.
 *
 * Terminate (via xge_hal_channel_dtr_term_f{}) all channel descriptors.
 * Currently used internally only by HAL, as part of its
 * xge_hal_channel_close() and xge_hal_channel_open() in case
 * of fatal error.
 *
 * See also: xge_hal_channel_dtr_term_f{}.
 */
void xge_hal_channel_abort(xge_hal_channel_h channelh,
	                       xge_hal_channel_reopen_e reopen)
{
	xge_hal_channel_t *channel = (xge_hal_channel_t *)channelh;
	xge_hal_dtr_h dtr;
#ifdef XGE_OS_MEMORY_CHECK
	int check_cnt = 0;
#endif
	int free_length_sav;
	int reserve_top_sav;

	if (channel->dtr_term == NULL) {
	    return;
	}

	free_length_sav = channel->free_length;
	while (__hal_channel_dtr_next_freelist(channelh, &dtr) == XGE_HAL_OK) {
#ifdef XGE_OS_MEMORY_CHECK
#ifdef XGE_DEBUG_ASSERT
	    if (channel->type == XGE_HAL_CHANNEL_TYPE_FIFO) {
	        xge_assert(!__hal_fifo_txdl_priv(dtr)->allocated);
	    } else {
	        if (channel->type == XGE_HAL_CHANNEL_TYPE_RING) {
	            xge_assert(!__hal_ring_rxd_priv((xge_hal_ring_t * ) channelh, dtr)->allocated);
	        }
	    }
#endif
	    check_cnt++;
#endif
	    channel->dtr_term(channel, dtr, XGE_HAL_DTR_STATE_FREED,
	              channel->userdata, reopen);
	}
	channel->free_length = free_length_sav;

	while (__hal_channel_dtr_next_not_completed(channelh, &dtr) ==
	       XGE_HAL_OK) {
#ifdef XGE_OS_MEMORY_CHECK
#ifdef XGE_DEBUG_ASSERT
	    if (channel->type == XGE_HAL_CHANNEL_TYPE_FIFO) {
	        xge_assert(__hal_fifo_txdl_priv(dtr)->allocated);
	    } else {
	        if (channel->type == XGE_HAL_CHANNEL_TYPE_RING) {
	        xge_assert(__hal_ring_rxd_priv((xge_hal_ring_t * ) channelh, dtr)
	               ->allocated);
	        }
	    }
#endif
	    check_cnt++;
#endif
	    channel->dtr_term(channel, dtr, XGE_HAL_DTR_STATE_POSTED,
	              channel->userdata, reopen);

	}

	reserve_top_sav = channel->reserve_top;
	while (__hal_channel_dtr_next_reservelist(channelh, &dtr) ==
	                        XGE_HAL_OK) {
#ifdef XGE_OS_MEMORY_CHECK
#ifdef XGE_DEBUG_ASSERT
	    if (channel->type == XGE_HAL_CHANNEL_TYPE_FIFO) {
	        xge_assert(!__hal_fifo_txdl_priv(dtr)->allocated);
	    } else {
	        if (channel->type == XGE_HAL_CHANNEL_TYPE_RING) {
	        xge_assert(!__hal_ring_rxd_priv((xge_hal_ring_t * ) channelh, dtr)->allocated);
	        }
	    }
#endif
	    check_cnt++;
#endif
	    channel->dtr_term(channel, dtr, XGE_HAL_DTR_STATE_AVAIL,
	              channel->userdata, reopen);
	}
	channel->reserve_top = reserve_top_sav;

	xge_assert(channel->reserve_length ==
	            (channel->free_length + channel->reserve_top));

#ifdef XGE_OS_MEMORY_CHECK
	xge_assert(check_cnt == channel->reserve_initial);
#endif

}

/**
 * xge_hal_channel_close - Close communication channel.
 * @channelh: The channel handle.
 * @reopen: See  xge_hal_channel_reopen_e{}.
 *
 * Will close previously opened channel and deallocate associated resources.
 * Channel must be opened otherwise assert will be generated.
 * Note that free_channels list is not protected. i.e. caller must provide
 * safe context.
 */
void xge_hal_channel_close(xge_hal_channel_h channelh,
	                       xge_hal_channel_reopen_e reopen)
{
	xge_hal_channel_t *channel = (xge_hal_channel_t *)channelh;
	xge_hal_device_t *hldev;
	xge_list_t *item;
	xge_assert(channel);
	xge_assert(channel->type < XGE_HAL_CHANNEL_TYPE_MAX);

	hldev = (xge_hal_device_t *)channel->devh;
	channel->is_open = 0;
	channel->magic = XGE_HAL_DEAD;

	/* sanity check: make sure channel is not in free list */
	xge_list_for_each(item, &hldev->free_channels) {
	    xge_hal_channel_t *tmp;

	    tmp = xge_container_of(item, xge_hal_channel_t, item);
	    xge_assert(!tmp->is_open);
	    if (channel == tmp) {
	        return;
	    }
	}

	xge_hal_channel_abort(channel, reopen);

	xge_assert((channel->type == XGE_HAL_CHANNEL_TYPE_FIFO) ||
	       (channel->type == XGE_HAL_CHANNEL_TYPE_RING));

	if (reopen == XGE_HAL_CHANNEL_OC_NORMAL) {
	    /* de-allocate */
	    switch(channel->type) {
	        case XGE_HAL_CHANNEL_TYPE_FIFO:
	            __hal_fifo_close(channelh);
	            break;
	        case XGE_HAL_CHANNEL_TYPE_RING:
	            __hal_ring_close(channelh);
	            break;
	        case XGE_HAL_CHANNEL_TYPE_SEND_QUEUE:
	        case XGE_HAL_CHANNEL_TYPE_RECEIVE_QUEUE:
	        case XGE_HAL_CHANNEL_TYPE_COMPLETION_QUEUE:
	        case XGE_HAL_CHANNEL_TYPE_UP_MESSAGE_QUEUE:
	        case XGE_HAL_CHANNEL_TYPE_DOWN_MESSAGE_QUEUE:
	            xge_assert(channel->type == XGE_HAL_CHANNEL_TYPE_FIFO ||
	                   channel->type == XGE_HAL_CHANNEL_TYPE_RING);
	            break;
	        default:
	            break;
	    }
	}
	else
	        xge_assert(reopen == XGE_HAL_CHANNEL_RESET_ONLY);

	/* move channel back to free state list */
	xge_list_remove(&channel->item);
	xge_list_insert(&channel->item, &hldev->free_channels);

	if (xge_list_is_empty(&hldev->fifo_channels) &&
	    xge_list_is_empty(&hldev->ring_channels)) {
	    /* clear msix_idx in case of following HW reset */
	    hldev->reset_needed_after_close = 1;
	}
}
