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

#ifndef	VXGE_HAL_CHANNEL_H
#define	VXGE_HAL_CHANNEL_H

__EXTERN_BEGIN_DECLS

/*
 * __hal_dtr_h - Handle to the desriptor object used for nonoffload
 *		send or receive. Generic handle which can be with txd or rxd
 */
typedef void *__hal_dtr_h;

/*
 * enum __hal_channel_type_e - Enumerated channel types.
 * @VXGE_HAL_CHANNEL_TYPE_UNKNOWN: Unknown channel.
 * @VXGE_HAL_CHANNEL_TYPE_FIFO: fifo.
 * @VXGE_HAL_CHANNEL_TYPE_RING: ring.
 * @VXGE_HAL_CHANNEL_TYPE_SQ: Send Queue
 * @VXGE_HAL_CHANNEL_TYPE_SRQ: Receive Queue
 * @VXGE_HAL_CHANNEL_TYPE_CQRQ: Receive queue completion queue
 * @VXGE_HAL_CHANNEL_TYPE_UMQ: Up message queue
 * @VXGE_HAL_CHANNEL_TYPE_DMQ: Down message queue
 * @VXGE_HAL_CHANNEL_TYPE_MAX: Maximum number of HAL-supported
 * (and recognized) channel types. Currently: 7.
 *
 * Enumerated channel types. Currently there are only two link-layer
 * channels - X3100 fifo and X3100 ring. In the future the list will grow.
 */
typedef enum __hal_channel_type_e {
	VXGE_HAL_CHANNEL_TYPE_UNKNOWN			= 0,
	VXGE_HAL_CHANNEL_TYPE_FIFO			= 1,
	VXGE_HAL_CHANNEL_TYPE_RING			= 2,
	VXGE_HAL_CHANNEL_TYPE_SEND_QUEUE		= 3,
	VXGE_HAL_CHANNEL_TYPE_RECEIVE_QUEUE		= 4,
	VXGE_HAL_CHANNEL_TYPE_COMPLETION_QUEUE		= 5,
	VXGE_HAL_CHANNEL_TYPE_UP_MESSAGE_QUEUE		= 6,
	VXGE_HAL_CHANNEL_TYPE_DOWN_MESSAGE_QUEUE	= 7,
	VXGE_HAL_CHANNEL_TYPE_MAX			= 8
} __hal_channel_type_e;

/*
 * __hal_dtr_item_t
 * @dtr: Pointer to the descriptors that contains the dma data
 *		to/from the device.
 * @hal_priv: HAL Private data related to the dtr.
 * @uld_priv: ULD Private data related to the dtr.
 */
typedef struct __hal_dtr_item_t {
	void   *dtr;
	void   *hal_priv;
	void   *uld_priv;
	u32	state;
#define	VXGE_HAL_CHANNEL_DTR_FREE	0
#define	VXGE_HAL_CHANNEL_DTR_RESERVED	1
#define	VXGE_HAL_CHANNEL_DTR_POSTED	2
#define	VXGE_HAL_CHANNEL_DTR_COMPLETED	3
} __hal_dtr_item_t;

/*
 * __hal_channel_t
 * @item: List item; used to maintain a list of open channels.
 * @type: Channel type. See vxge_hal_channel_type_e {}.
 * @devh: Device handle. HAL device object that contains _this_ channel.
 * @pdev: PCI Device object
 * @vph: Virtual path handle. Virtual Path Object that contains _this_ channel.
 * @length: Channel length. Currently allocated number of descriptors.
 *	The channel length "grows" when more descriptors get allocated.
 *	See _hal_mempool_grow.
 * @dtr_arr: Dtr array. Contains descriptors posted to the channel and their
 *	private data.
 *	Note that at any point in time @dtr_arr contains 3 types of
 *	descriptors:
 *	1) posted but not yet consumed by X3100 device;
 *	2) consumed but not yet completed;
 *	3) completed.
 * @post_index: Post index. At any point in time points on the
 *	position in the channel, which'll contain next to-be-posted
 *	descriptor.
 * @compl_index: Completion index. At any point in time points on the
 *	position in the channel, which will contain next
 *	to-be-completed descriptor.
 * @reserve_index: Reserve index. At any point in time points on the
 *	position in the channel, which will contain next
 *	to-be-reserved descriptor.
 * @free_dtr_count: Number of dtrs free.
 * @posted_dtr_count: Number of dtrs posted
 * @post_lock: Lock to serialize multiple concurrent "posters" of descriptors
 *		on the given channel.
 * @poll_bytes: Poll bytes.
 * @per_dtr_space: Per-descriptor space (in bytes) that channel user can utilize
 *		to store per-operation control information.
 * @stats: Pointer to common statistics
 * @userdata: Per-channel opaque (void *) user-defined context, which may be
 *	upper-layer driver object, ULP connection, etc.
 *	Once channel is open, @userdata is passed back to user via
 *	vxge_hal_channel_callback_f.
 *
 * HAL channel object.
 *
 * See also: vxge_hal_channel_type_e {}, vxge_hal_channel_flag_e
 */
typedef struct __hal_channel_t {
	vxge_list_t		item;
	__hal_channel_type_e	type;
	vxge_hal_device_h	devh;
	pci_dev_h		pdev;
	vxge_hal_vpath_h	vph;
	u32			length;
	u32			is_initd;
	__hal_dtr_item_t	*dtr_arr;
	u32			compl_index __vxge_os_attr_cacheline_aligned;
	u32			reserve_index __vxge_os_attr_cacheline_aligned;
	spinlock_t		post_lock;
	u32			poll_bytes;
	u32			per_dtr_space;
	vxge_hal_vpath_stats_sw_common_info_t *stats;
	void			*userdata;
} __hal_channel_t __vxge_os_attr_cacheline_aligned;

#define	__hal_channel_is_posted_dtr(channel, index) \
	    ((channel)->dtr_arr[index].state == VXGE_HAL_CHANNEL_DTR_POSTED)

#define	__hal_channel_for_each_posted_dtr(channel, dtrh, index) \
	for (index = (channel)->compl_index,\
	    dtrh = (channel)->dtr_arr[index].dtr; \
	    (index < (channel)->reserve_index) && \
	    ((channel)->dtr_arr[index].state == VXGE_HAL_CHANNEL_DTR_POSTED); \
	    index = (++index == (channel)->length)? 0 : index, \
	    dtrh = (channel)->dtr_arr[index].dtr)

#define	__hal_channel_for_each_dtr(channel, dtrh, index) \
	for (index = 0, dtrh = (channel)->dtr_arr[index].dtr; \
	    index < (channel)->length; \
	    dtrh = ((++index == (channel)->length)? 0 : \
	    (channel)->dtr_arr[index].dtr))

#define	__hal_channel_free_dtr_count(channel)			\
	(((channel)->reserve_index < (channel)->compl_index) ?	\
	((channel)->compl_index - (channel)->reserve_index) :	\
	(((channel)->length - (channel)->reserve_index) + \
	(channel)->reserve_index))

/* ========================== CHANNEL PRIVATE API ========================= */

__hal_channel_t *
vxge_hal_channel_allocate(
    vxge_hal_device_h devh,
    vxge_hal_vpath_h vph,
    __hal_channel_type_e type,
    u32 length,
    u32 per_dtr_space,
    void *userdata);

void
vxge_hal_channel_free(
    __hal_channel_t *channel);

vxge_hal_status_e
vxge_hal_channel_initialize(
    __hal_channel_t *channel);

vxge_hal_status_e
__hal_channel_reset(
    __hal_channel_t *channel);

void
vxge_hal_channel_terminate(
    __hal_channel_t *channel);

void
__hal_channel_init_pending_list(
    vxge_hal_device_h devh);

void
__hal_channel_insert_pending_list(
    __hal_channel_t * channel);

void
__hal_channel_process_pending_list(
    vxge_hal_device_h devhv);

void
__hal_channel_destroy_pending_list(
    vxge_hal_device_h devh);

#if defined(VXGE_DEBUG_FP) && (VXGE_DEBUG_FP & VXGE_DEBUG_FP_CHANNEL)
#define	__HAL_STATIC_CHANNEL
#define	__HAL_INLINE_CHANNEL
#else	/* VXGE_FASTPATH_EXTERN */
#define	__HAL_STATIC_CHANNEL static
#define	__HAL_INLINE_CHANNEL inline
#endif	/* VXGE_FASTPATH_INLINE */

/* ========================== CHANNEL Fast Path API ========================= */
/*
 * __hal_channel_dtr_reserve- Reserve a dtr from the channel
 * @channelh: Channel
 * @dtrh: Buffer to return the DTR pointer
 *
 * Reserve a dtr from the reserve array.
 *
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL vxge_hal_status_e
/* LINTED */
__hal_channel_dtr_reserve(__hal_channel_t *channel, __hal_dtr_h *dtrh)
{
	vxge_hal_status_e status = VXGE_HAL_INF_OUT_OF_DESCRIPTORS;

	*dtrh = NULL;

	if (channel->dtr_arr[channel->reserve_index].state ==
	    VXGE_HAL_CHANNEL_DTR_FREE) {

		*dtrh = channel->dtr_arr[channel->reserve_index].dtr;

		channel->dtr_arr[channel->reserve_index].state =
		    VXGE_HAL_CHANNEL_DTR_RESERVED;

		if (++channel->reserve_index == channel->length)
			channel->reserve_index = 0;

		status = VXGE_HAL_OK;

	} else {

#if (VXGE_COMPONENT_HAL_CHANNEL & VXGE_DEBUG_MODULE_MASK)
		__hal_device_t *hldev = (__hal_device_t *) channel->devh;

		vxge_hal_info_log_channel("channel %d is full!", channel->type);
#endif

		channel->stats->full_cnt++;
	}

	return (status);
}

/*
 * __hal_channel_dtr_restore - Restores a dtr to the channel
 * @channelh: Channel
 * @dtr: DTR pointer
 *
 * Returns a dtr back to reserve array.
 *
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
/* LINTED */
__hal_channel_dtr_restore(__hal_channel_t *channel, __hal_dtr_h dtrh)
{
	u32 dtr_index;

	/*
	 * restore a previously allocated dtrh at current offset and update
	 * the available reserve length accordingly. If dtrh is null just
	 * update the reserve length, only
	 */

	if (channel->reserve_index == 0)
		dtr_index = channel->length;
	else
		dtr_index = channel->reserve_index - 1;

	if ((channel->dtr_arr[dtr_index].dtr = dtrh) != NULL) {

		channel->reserve_index = dtr_index;
		channel->dtr_arr[dtr_index].state = VXGE_HAL_CHANNEL_DTR_FREE;

#if (VXGE_COMPONENT_HAL_CHANNEL & VXGE_DEBUG_MODULE_MASK)

		__hal_device_t *hldev = (__hal_device_t *) channel->devh;
		vxge_hal_info_log_channel("dtrh 0x"VXGE_OS_STXFMT" \
		    restored for " "channel %d at reserve index %d, ",
		    (ptr_t) dtrh, channel->type,
		    channel->reserve_index);
#endif
	}
}

/*
 * __hal_channel_dtr_post - Post a dtr to the channel
 * @channelh: Channel
 * @dtr: DTR pointer
 *
 * Posts a dtr to work array.
 *
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
/* LINTED */
__hal_channel_dtr_post(__hal_channel_t *channel, u32 dtr_index)
{
	channel->dtr_arr[dtr_index].state =
	    VXGE_HAL_CHANNEL_DTR_POSTED;
}

/*
 * __hal_channel_dtr_try_complete - Returns next completed dtr
 * @channelh: Channel
 * @dtr: Buffer to return the next completed DTR pointer
 *
 * Returns the next completed dtr with out removing it from work array
 *
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
/* LINTED */
__hal_channel_dtr_try_complete(__hal_channel_t *channel, __hal_dtr_h *dtrh)
{
	vxge_assert(channel->dtr_arr);
	vxge_assert(channel->compl_index < channel->length);

	if (channel->dtr_arr[channel->compl_index].state ==
	    VXGE_HAL_CHANNEL_DTR_POSTED)
		*dtrh = channel->dtr_arr[channel->compl_index].dtr;
	else
		*dtrh = NULL;
}

/*
 * __hal_channel_dtr_complete - Removes next completed dtr from the work array
 * @channelh: Channel
 *
 * Removes the next completed dtr from work array
 *
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
/* LINTED */
__hal_channel_dtr_complete(__hal_channel_t *channel)
{
	channel->dtr_arr[channel->compl_index].state =
	VXGE_HAL_CHANNEL_DTR_COMPLETED;

	if (++channel->compl_index == channel->length)
		channel->compl_index = 0;

	channel->stats->total_compl_cnt++;
}

/*
 * __hal_channel_dtr_free - Frees a dtr
 * @channelh: Channel
 * @index:  Index of DTR
 *
 * Returns the dtr to free array
 *
 */
__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
/* LINTED */
__hal_channel_dtr_free(__hal_channel_t *channel, u32 dtr_index)
{
	channel->dtr_arr[dtr_index].state =
	    VXGE_HAL_CHANNEL_DTR_FREE;
}

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_CHANNEL_H */
