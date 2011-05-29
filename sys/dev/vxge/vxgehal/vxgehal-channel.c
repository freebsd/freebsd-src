/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
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
/*$FreeBSD$*/

#include <dev/vxge/vxgehal/vxgehal.h>


/*
 * vxge_hal_channel_allocate - Allocate memory for channel
 * @devh: Handle to the device object
 * @vph: Handle to Virtual Path
 * @type: Type of channel
 * @length: Lengths of arrays
 * @per_dtr_space: ULD requested per dtr space to be allocated in priv
 * @userdata: User data to be passed back in the callback
 *
 * This function allocates required memory for the channel and various arrays
 * in the channel
 */
__hal_channel_t *
vxge_hal_channel_allocate(
    vxge_hal_device_h devh,
    vxge_hal_vpath_h vph,
    __hal_channel_type_e type,
    u32 length,
    u32 per_dtr_space,
    void *userdata)
{
	vxge_hal_device_t *hldev = (vxge_hal_device_t *) devh;
	__hal_channel_t *channel;
	u32 i, size = 0;

	vxge_assert((devh != NULL) && (vph != NULL));

	vxge_hal_trace_log_channel("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_channel("devh = 0x"VXGE_OS_STXFMT", vph = "
	    "0x"VXGE_OS_STXFMT", type = %d, length = %d, "
	    "per_dtr_space = %d, userdata = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, (ptr_t) vph, type, length, per_dtr_space,
	    (ptr_t) userdata);

	switch (type) {
	case VXGE_HAL_CHANNEL_TYPE_FIFO:
		size = sizeof(__hal_fifo_t);
		break;
	case VXGE_HAL_CHANNEL_TYPE_RING:
		size = sizeof(__hal_ring_t);
		break;


	default:
		vxge_assert(size);
		break;

	}

	channel = (__hal_channel_t *) vxge_os_malloc(hldev->pdev, size);
	if (channel == NULL) {
		vxge_hal_trace_log_channel("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (NULL);
	}

	vxge_os_memzero(channel, size);
	vxge_list_init(&channel->item);

	channel->pdev = hldev->pdev;
	channel->type = type;
	channel->devh = devh;
	channel->vph = vph;

	channel->userdata = userdata;
	channel->per_dtr_space = per_dtr_space;

	channel->length = length;

	channel->dtr_arr = (__hal_dtr_item_t *) vxge_os_malloc(hldev->pdev,
	    sizeof(__hal_dtr_item_t)*length);
	if (channel->dtr_arr == NULL) {
		vxge_hal_channel_free(channel);
		vxge_hal_trace_log_channel("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (NULL);
	}

	vxge_os_memzero(channel->dtr_arr, sizeof(__hal_dtr_item_t)*length);

	channel->compl_index = 0;
	channel->reserve_index = 0;

	for (i = 0; i < length; i++)
		channel->dtr_arr[i].state = VXGE_HAL_CHANNEL_DTR_FREE;

	vxge_hal_trace_log_channel("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
	return (channel);
}

/*
 * __hal_channel_free - Free memory allocated for channel
 * @channel: channel to be freed
 *
 * This function deallocates memory from the channel and various arrays
 * in the channel
 */
void
vxge_hal_channel_free(
    __hal_channel_t *channel)
{
	int size = 0;
	vxge_hal_device_t *hldev;

	vxge_assert(channel != NULL);

	hldev = (vxge_hal_device_t *) channel->devh;

	vxge_hal_trace_log_channel("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_channel("channel = 0x"VXGE_OS_STXFMT,
	    (ptr_t) channel);

	vxge_assert(channel->pdev);

	if (channel->dtr_arr) {
		vxge_os_free(channel->pdev, channel->dtr_arr,
		    sizeof(__hal_dtr_item_t)*channel->length);
		channel->dtr_arr = NULL;
	}

	switch (channel->type) {
	case VXGE_HAL_CHANNEL_TYPE_FIFO:
		size = sizeof(__hal_fifo_t);
		break;
	case VXGE_HAL_CHANNEL_TYPE_RING:
		size = sizeof(__hal_ring_t);
		break;
	default:
		break;
	}

	vxge_os_free(channel->pdev, channel, size);

	vxge_hal_trace_log_channel("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * __hal_channel_initialize - Initialize a channel
 * @channel: channel to be initialized
 *
 * This function initializes a channel by properly
 *		setting the various references
 */
vxge_hal_status_e
vxge_hal_channel_initialize(
    __hal_channel_t *channel)
{
	vxge_hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(channel != NULL);

	hldev = (vxge_hal_device_t *) channel->devh;
	vpath = (__hal_virtualpath_t *)
	    ((__hal_vpath_handle_t *) channel->vph)->vpath;

	vxge_assert(vpath != NULL);

	vxge_hal_trace_log_channel("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_channel("channel = 0x"VXGE_OS_STXFMT,
	    (ptr_t) channel);

	switch (channel->type) {
	case VXGE_HAL_CHANNEL_TYPE_FIFO:
		vpath->fifoh = (vxge_hal_fifo_h) channel;
		channel->stats =
		    &((__hal_fifo_t *) channel)->stats->common_stats;
		break;
	case VXGE_HAL_CHANNEL_TYPE_RING:
		vpath->ringh = (vxge_hal_ring_h) channel;
		channel->stats =
		    &((__hal_ring_t *) channel)->stats->common_stats;
		break;


	default:
		break;
	}

	channel->is_initd = 1;
	vxge_hal_trace_log_channel("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * __hal_channel_reset - Resets a channel
 * @channel: channel to be reset
 *
 * This function resets a channel by properly setting the various references
 */
vxge_hal_status_e
__hal_channel_reset(
    __hal_channel_t *channel)
{
	u32 i;
	__hal_device_t *hldev;

	vxge_assert(channel != NULL);

	hldev = (__hal_device_t *) channel->devh;

	vxge_hal_trace_log_channel("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_channel("channel = 0x"VXGE_OS_STXFMT,
	    (ptr_t) channel);

	vxge_assert(channel->pdev);

	channel->compl_index = 0;
	channel->reserve_index = 0;

	for (i = 0; i < channel->length; i++) {
		channel->dtr_arr[i].state =
		    VXGE_HAL_CHANNEL_DTR_FREE;
	}

	vxge_hal_trace_log_channel("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_channel_terminate - Deinitializes a channel
 * @channel: channel to be deinitialized
 *
 * This function deinitializes a channel by properly
 *		setting the various references
 */
void
vxge_hal_channel_terminate(
    __hal_channel_t *channel)
{
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(channel != NULL);

	if (!channel || !channel->is_initd)
		return;

	hldev = (__hal_device_t *) channel->devh;
	vpath = (__hal_virtualpath_t *)
	    ((__hal_vpath_handle_t *) channel->vph)->vpath;

	vxge_assert(vpath != NULL);

	vxge_hal_trace_log_channel("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_channel("channel = 0x"VXGE_OS_STXFMT,
	    (ptr_t) channel);

	switch (channel->type) {
	case VXGE_HAL_CHANNEL_TYPE_FIFO:
		vpath->fifoh = 0;
		break;
	case VXGE_HAL_CHANNEL_TYPE_RING:
		vpath->ringh = 0;
		break;
	case VXGE_HAL_CHANNEL_TYPE_SEND_QUEUE:
		vxge_list_remove(&channel->item);
		vpath->sw_stats->obj_counts.no_sqs--;
		break;
	case VXGE_HAL_CHANNEL_TYPE_RECEIVE_QUEUE:
		vxge_list_remove(&channel->item);
		vpath->sw_stats->obj_counts.no_srqs--;
		break;
	case VXGE_HAL_CHANNEL_TYPE_COMPLETION_QUEUE:
		vxge_list_remove(&channel->item);
		vpath->sw_stats->obj_counts.no_cqrqs--;
		break;
	default:
		break;
	}

	vxge_hal_trace_log_channel("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

void
__hal_channel_init_pending_list(
    vxge_hal_device_h devh)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_channel("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_channel("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);
	vxge_list_init(&hldev->pending_channel_list);

#if defined(VXGE_HAL_VP_CHANNELS)
	vxge_os_spin_lock_init(&hldev->pending_channel_lock, hldev->pdev);
#elif defined(VXGE_HAL_VP_CHANNELS_IRQ)
	vxge_os_spin_lock_init_irq(&hldev->pending_channel_lock, hldev->irqh);
#endif
	vxge_hal_trace_log_channel("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

void
__hal_channel_insert_pending_list(
    __hal_channel_t * channel)
{
	__hal_device_t *hldev = (__hal_device_t *) channel->devh;

	vxge_assert(channel != NULL);

	vxge_hal_trace_log_channel("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_channel("channel = 0x"VXGE_OS_STXFMT,
	    (ptr_t) channel);

#if defined(VXGE_HAL_PENDING_CHANNELS)
	vxge_os_spin_lock(&hldev->pending_channel_lock);
#elif defined(VXGE_HAL_PENDING_CHANNELS_IRQ)
	vxge_os_spin_lock_irq(&hldev->pending_channel_lock, flags);
#endif

	vxge_list_insert_before(&channel->item, &hldev->pending_channel_list);

#if defined(VXGE_HAL_PENDING_CHANNELS)
	vxge_os_spin_unlock(&hldev->pending_channel_lock);
#elif defined(VXGE_HAL_PENDING_CHANNELS_IRQ)
	vxge_os_spin_unlock_irq(&hldev->pending_channel_lock, flags);
#endif

	__hal_channel_process_pending_list(channel->devh);

	vxge_hal_trace_log_channel("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}

void
__hal_channel_process_pending_list(
    vxge_hal_device_h devh)
{
	vxge_hal_status_e status;
	__hal_channel_t *channel;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_channel("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_channel("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	for (;;) {
#if defined(VXGE_HAL_PENDING_CHANNELS)
		vxge_os_spin_lock(&hldev->pending_channel_lock);
#elif defined(VXGE_HAL_PENDING_CHANNELS_IRQ)
		vxge_os_spin_lock_irq(&hldev->pending_channel_lock, flags);
#endif

		channel = (__hal_channel_t *)
		    vxge_list_first_get(&hldev->pending_channel_list);

		if (channel != NULL)
			vxge_list_remove(&channel->item);

#if defined(VXGE_HAL_PENDING_CHANNELS)
		vxge_os_spin_unlock(&hldev->pending_channel_lock);
#elif defined(VXGE_HAL_PENDING_CHANNELS_IRQ)
		vxge_os_spin_unlock_irq(&hldev->pending_channel_lock, flags);
#endif

		if (channel == NULL) {
			vxge_hal_trace_log_channel("<== %s:%s:%d  Result: 0",
			    __FILE__, __func__, __LINE__);
			return;
		}

		switch (channel->type) {
		default:
			status = VXGE_HAL_OK;
			break;
		}

		if (status == VXGE_HAL_ERR_OUT_OF_MEMORY) {
#if defined(VXGE_HAL_PENDING_CHANNELS)
			vxge_os_spin_lock(&hldev->pending_channel_lock);
#elif defined(VXGE_HAL_PENDING_CHANNELS_IRQ)
			vxge_os_spin_lock_irq(&hldev->pending_channel_lock,
			    flags);
#endif

			vxge_list_insert(&channel->item,
			    &hldev->pending_channel_list);

#if defined(VXGE_HAL_PENDING_CHANNELS)
			vxge_os_spin_unlock(&hldev->pending_channel_lock);
#elif defined(VXGE_HAL_PENDING_CHANNELS_IRQ)
			vxge_os_spin_unlock_irq(&hldev->pending_channel_lock,
			    flags);
#endif
			vxge_hal_trace_log_channel("<== %s:%s:%d  Result: 0",
			    __FILE__, __func__, __LINE__);

			return;
		}

	}
}

void
__hal_channel_destroy_pending_list(
    vxge_hal_device_h devh)
{
	vxge_list_t *p, *n;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_channel("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_channel("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	vxge_list_for_each_safe(p, n, &hldev->pending_channel_list) {

		vxge_list_remove(p);

		switch (((__hal_channel_t *) p)->type) {
		default:
			break;
		}

	}

#if defined(VXGE_HAL_PENDING_CHANNELS)
	vxge_os_spin_lock_destroy(&hldev->pending_channel_lock,
	    hldev->header.pdev);
#elif defined(VXGE_HAL_PENDING_CHANNELS_IRQ)
	vxge_os_spin_lock_destroy_irq(&hldev->pending_channel_lock,
	    hldev->header.pdev);
#endif
	vxge_hal_trace_log_channel("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}
