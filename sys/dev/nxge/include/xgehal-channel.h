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

/*
 *  FileName :    xgehal-channel.h
 *
 *  Description:  HAL channel object functionality
 *
 *  Created:      19 May 2004
 */

#ifndef XGE_HAL_CHANNEL_H
#define XGE_HAL_CHANNEL_H

#include <dev/nxge/include/xge-os-pal.h>
#include <dev/nxge/include/xge-list.h>
#include <dev/nxge/include/xgehal-types.h>
#include <dev/nxge/include/xgehal-stats.h>

__EXTERN_BEGIN_DECLS

/**
 * enum xge_hal_channel_type_e - Enumerated channel types.
 * @XGE_HAL_CHANNEL_TYPE_FIFO: fifo.
 * @XGE_HAL_CHANNEL_TYPE_RING: ring.
 * @XGE_HAL_CHANNEL_TYPE_SEND_QUEUE: Send Queue
 * @XGE_HAL_CHANNEL_TYPE_RECEIVE_QUEUE: Receive Queue
 * @XGE_HAL_CHANNEL_TYPE_COMPLETION_QUEUE: Receive queue completion queue
 * @XGE_HAL_CHANNEL_TYPE_UP_MESSAGE_QUEUE: Up message queue
 * @XGE_HAL_CHANNEL_TYPE_DOWN_MESSAGE_QUEUE: Down message queue
 * @XGE_HAL_CHANNEL_TYPE_MAX: Maximum number of HAL-supported
 * (and recognized) channel types. Currently: two.
 *
 * Enumerated channel types. Currently there are only two link-layer
 * channels - Xframe fifo and Xframe ring. In the future the list will grow.
 */
typedef enum xge_hal_channel_type_e {
	XGE_HAL_CHANNEL_TYPE_FIFO,
	XGE_HAL_CHANNEL_TYPE_RING,
	XGE_HAL_CHANNEL_TYPE_SEND_QUEUE,
	XGE_HAL_CHANNEL_TYPE_RECEIVE_QUEUE,
	XGE_HAL_CHANNEL_TYPE_COMPLETION_QUEUE,
	XGE_HAL_CHANNEL_TYPE_UP_MESSAGE_QUEUE,
	XGE_HAL_CHANNEL_TYPE_DOWN_MESSAGE_QUEUE,
	XGE_HAL_CHANNEL_TYPE_MAX
} xge_hal_channel_type_e;

/**
 * enum xge_hal_channel_flag_e - Channel flags.
 * @XGE_HAL_CHANNEL_FLAG_NONE: zero (nil) flag.
 * @XGE_HAL_CHANNEL_FLAG_USE_TX_LOCK: use lock when posting transmit
 * descriptor.
 * @XGE_HAL_CHANNEL_FLAG_FREE_RXD: to-be-defined.
 *
 * Channel opening flags. Reserved for future usage.
 */
typedef enum xge_hal_channel_flag_e {
	XGE_HAL_CHANNEL_FLAG_NONE		= 0x0,
	XGE_HAL_CHANNEL_FLAG_USE_TX_LOCK	= 0x1,
	XGE_HAL_CHANNEL_FLAG_FREE_RXD	        = 0x2
} xge_hal_channel_flag_e;

/**
 * enum xge_hal_dtr_state_e - Descriptor (DTR) state.
 * @XGE_HAL_DTR_STATE_NONE: Invalid state.
 * @XGE_HAL_DTR_STATE_AVAIL: Descriptor is available for reservation
 * (via xge_hal_fifo_dtr_reserve(), xge_hal_ring_dtr_reserve(), etc.).
 * @XGE_HAL_DTR_STATE_POSTED: Descriptor is posted for processing by the
 * device.
 * @XGE_HAL_DTR_STATE_FREED: Descriptor is free and can be reused for
 * filling-in and posting later.
 *
 * Xframe/HAL descriptor states. For more on descriptor states and transitions
 * please refer to ch_intern{}.
 *
 * See also: xge_hal_channel_dtr_term_f{}.
 */
typedef enum xge_hal_dtr_state_e {
	XGE_HAL_DTR_STATE_NONE		= 0,
	XGE_HAL_DTR_STATE_AVAIL		= 1,
	XGE_HAL_DTR_STATE_POSTED	= 2,
	XGE_HAL_DTR_STATE_FREED		= 3
} xge_hal_dtr_state_e;

/**
 * enum xge_hal_channel_reopen_e - Channel open, close, or reopen option.
 * @XGE_HAL_CHANNEL_RESET_ONLY: Do not (de)allocate channel; used with
 * xge_hal_channel_open(), xge_hal_channel_close().
 * @XGE_HAL_CHANNEL_OC_NORMAL: Do (de)allocate channel; used with
 * xge_hal_channel_open(), xge_hal_channel_close().
 *
 * Enumerates options used with channel open and close operations.
 * The @XGE_HAL_CHANNEL_RESET_ONLY can be used when resetting the device;
 * in this case there is actually no need to free and then again malloc
 * the memory (including DMA-able memory) used for channel operation.
 */
typedef enum xge_hal_channel_reopen_e {
	XGE_HAL_CHANNEL_RESET_ONLY	= 1,
	XGE_HAL_CHANNEL_OC_NORMAL	= 2
} xge_hal_channel_reopen_e;

/**
 * function xge_hal_channel_callback_f - Channel callback.
 * @channelh: Channel "containing" 1 or more completed descriptors.
 * @dtrh: First completed descriptor.
 * @t_code: Transfer code, as per Xframe User Guide.
 *          Returned by HAL.
 * @host_control: Opaque 64bit data stored by ULD inside the Xframe
 *            descriptor prior to posting the latter on the channel
 *            via xge_hal_fifo_dtr_post() or xge_hal_ring_dtr_post().
 *            The @host_control is returned as is to the ULD with each
 *            completed descriptor.
 * @userdata: Opaque per-channel data specified at channel open
 *            time, via xge_hal_channel_open().
 *
 * Channel completion callback (type declaration). A single per-channel
 * callback is specified at channel open time, via
 * xge_hal_channel_open().
 * Typically gets called as part of the processing of the Interrupt
 * Service Routine.
 *
 * Channel callback gets called by HAL if, and only if, there is at least
 * one new completion on a given ring or fifo channel. Upon processing the
 * first @dtrh ULD is _supposed_ to continue consuming completions
 * usingáone of the following HAL APIs:
 *    - xge_hal_fifo_dtr_next_completed()
 *      or
 *    - xge_hal_ring_dtr_next_completed().
 *
 * Note that failure to process new completions in a timely fashion
 * leads to XGE_HAL_INF_OUT_OF_DESCRIPTORS condition.
 *
 * Non-zero @t_code means failure to process (transmit or receive, depending
 * on the channel type) the descriptor.
 *
 * In the "transmit" case the failure could happen, for instance, when the
 * link is down, in which case Xframe completes the descriptor because it
 * is not able to send the data out.
 *
 * For details please refer to Xframe User Guide.
 *
 * See also: xge_hal_fifo_dtr_next_completed(),
 * xge_hal_ring_dtr_next_completed(), xge_hal_channel_dtr_term_f{}.
 */
typedef xge_hal_status_e (*xge_hal_channel_callback_f)
				(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
				 u8 t_code, void *userdata);

/**
 * function xge_hal_channel_dtr_init_f - Initialize descriptor callback.
 * @channelh: Channel "containing" the @dtrh descriptor.
 * @dtrh: Descriptor.
 * @index: Index of the descriptor in the channel's set of descriptors.
 * @userdata: Per-channel user data (a.k.a. context) specified at
 * channel open time, via xge_hal_channel_open().
 * @reopen: See  xge_hal_channel_reopen_e{}.
 *
 * Initialize descriptor callback. Unless NULL is specified in the
 * xge_hal_channel_attr_t{} structure passed to xge_hal_channel_open()),
 * HAL invokes the callback as part of the xge_hal_channel_open()
 * implementation.
 * For the ring type of channel the ULD is expected to fill in this descriptor
 * with buffer(s) and control information.
 * For the fifo type of channel the ULD could use the callback to
 * pre-set DMA mappings and/or alignment buffers.
 *
 * See also: xge_hal_channel_attr_t{}, xge_hal_channel_dtr_term_f{}.
 */
typedef xge_hal_status_e (*xge_hal_channel_dtr_init_f)
				(xge_hal_channel_h channelh,
				 xge_hal_dtr_h dtrh,
				 int index,
				 void *userdata,
				 xge_hal_channel_reopen_e reopen);

/**
 * function xge_hal_channel_dtr_term_f - Terminate descriptor callback.
 * @channelh: Channel "containing" the @dtrh descriptor.
 * @dtrh: First completed descriptor.
 * @state: One of the xge_hal_dtr_state_e{} enumerated states.
 * @userdata: Per-channel user data (a.k.a. context) specified at
 * channel open time, via xge_hal_channel_open().
 * @reopen: See  xge_hal_channel_reopen_e{}.
 *
 * Terminate descriptor callback. Unless NULL is specified in the
 * xge_hal_channel_attr_t{} structure passed to xge_hal_channel_open()),
 * HAL invokes the callback as part of closing the corresponding
 * channel, prior to de-allocating the channel and associated data
 * structures (including descriptors).
 * ULD should utilize the callback to (for instance) unmap
 * and free DMA data buffers associated with the posted (state =
 * XGE_HAL_DTR_STATE_POSTED) descriptors,
 * as well as other relevant cleanup functions.
 *
 * See also: xge_hal_channel_attr_t{}, xge_hal_channel_dtr_init_f{}.
 */
typedef void (*xge_hal_channel_dtr_term_f) (xge_hal_channel_h channelh,
					    xge_hal_dtr_h dtrh,
					    xge_hal_dtr_state_e	state,
					    void *userdata,
					    xge_hal_channel_reopen_e reopen);


/**
 * struct xge_hal_channel_attr_t - Channel open "template".
 * @type: xge_hal_channel_type_e channel type.
 * @vp_id: Virtual path id
 * @post_qid: Queue ID to post descriptors. For the link layer this
 *            number should be in the 0..7 range.
 * @compl_qid: Completion queue ID. Must be set to zero for the link layer.
 * @callback: Channel completion callback. HAL invokes the callback when there
 *            are new completions on that channel. In many implementations
 *            the @callback executes in the hw interrupt context.
 * @dtr_init: Channel's descriptor-initialize callback.
 *            See xge_hal_channel_dtr_init_f{}.
 *            If not NULL, HAL invokes the callback when opening
 *            the channel via xge_hal_channel_open().
 * @dtr_term: Channel's descriptor-terminate callback. If not NULL,
 *          HAL invokes the callback when closing the corresponding channel.
 *          See also xge_hal_channel_dtr_term_f{}.
 * @userdata: User-defined "context" of _that_ channel. Passed back to the
 *            user as one of the @callback, @dtr_init, and @dtr_term arguments.
 * @per_dtr_space: If specified (i.e., greater than zero): extra space
 *              reserved by HAL per each transmit or receive (depending on the
 *              channel type) descriptor. Can be used to store,
 *              and retrieve on completion, information specific
 *              to the upper-layer.
 * @flags: xge_hal_channel_flag_e enumerated flags.
 *
 * Channel open "template". User fills the structure with channel
 * attributes and passes it to xge_hal_channel_open().
 * Usage: See ex_open{}.
 */
typedef struct xge_hal_channel_attr_t {
	xge_hal_channel_type_e		type;
#ifdef XGEHAL_RNIC
	u32				vp_id;
#endif
	int				post_qid;
	int				compl_qid;
	xge_hal_channel_callback_f	callback;
	xge_hal_channel_dtr_init_f	dtr_init;
	xge_hal_channel_dtr_term_f	dtr_term;
	void				*userdata;
	int				per_dtr_space;
	xge_hal_channel_flag_e		flags;
} xge_hal_channel_attr_t;

/*
 * xge_hal_channel_t
 * ---------- complete/free section ---------------
 * @item: List item; used to maintain a list of open channels.
 * @callback: Channel completion callback. See
 * xge_hal_channel_callback_f.
 * @compl_index: Completion index. At any point in time points on the
 *               position in the channel, which will contain next
 *               to-be-completed descriptor.
 * @length: Channel length. Currently allocated number of descriptors.
 *          The channel length "grows" when more descriptors get allocated.
 *          See _hal_mempool_grow.
 * @free_arr: Free array. Contains completed descriptors that were freed
 *            (i.e., handed over back to HAL) by ULD.
 *            See xge_hal_fifo_dtr_free(), xge_hal_ring_dtr_free().
 * @free_lock: Lock to protect @free_arr.
 * ----------- reserve/post section ---------------
 * @post_index: Post index. At any point in time points on the
 *              position in the channel, which'll contain next to-be-posted
 *              descriptor.
 * @post_lock: Lock to serialize multiple concurrent "posters" of descriptors
 *             on the given channel.
 * @reserve_arr: Reserve array. Contains descriptors that can be reserved
 *               by ULD for the subsequent send or receive operation.
 *               See xge_hal_fifo_dtr_reserve(),
 *               xge_hal_ring_dtr_reserve().
 * @reserve_length: Length of the @reserve_arr. The length dynamically
 *                  changes: it decrements each time descriptor is reserved.
 * @reserve_lock: Lock to serialize multiple concurrent threads accessing
 *                @reserve_arr.
 * @reserve_threshold: Reserve threshold. Minimal number of free descriptors
 *                     that ought to be preserved in the channel at all times.
 *                     Note that @reserve_threshold >= 0 &&
 *                     @reserve_threshold < @reserve_max.
 * ------------ common section --------------------
 * @devh: Device handle. HAL device object that contains _this_ channel.
 * @dmah: Channel's DMA address. Used to synchronize (to/from device)
 *        descriptors.
 * @regh0: Base address of the device memory space handle. Copied from HAL device
 *         at channel open time.
 * @regh1: Base address of the device memory space handle. Copied from HAL device
 *         at channel open time.
 * @userdata: Per-channel opaque (void*) user-defined context, which may be
 *            upper-layer driver object, ULP connection, etc.
 *            Once channel is open, @userdata is passed back to user via
 *            xge_hal_channel_callback_f.
 * @work_arr: Work array. Contains descriptors posted to the channel.
 *            Note that at any point in time @work_arr contains 3 types of
 *            descriptors:
 *            1) posted but not yet consumed by Xframe device;
 *            2) consumed but not yet completed;
 *            3) completed but not yet freed
 *            (via xge_hal_fifo_dtr_free() or xge_hal_ring_dtr_free())
 * @saved_arr: Array used internally to optimize channel full-duplex
 *             operation.
 * @stats: Channel statistcis. Includes HAL internal counters, including
 *         for instance, number of times out-of-descriptors
 *         (see XGE_HAL_INF_OUT_OF_DESCRIPTORS) condition happened.
 * ------------- "slow" section  ------------------
 * @type: Channel type. See xge_hal_channel_type_e{}.
 * @vp_id: Virtual path id
 * @post_qid: Identifies Xframe queue used for posting descriptors.
 * @compl_qid: Identifies Xframe completion queue.
 * @flags: Channel flags. See xge_hal_channel_flag_e{}.
 * @reserve_initial: Initial number of descriptors allocated at channel open
 *                   time (see xge_hal_channel_open()). The number of
 *                   channel descriptors can grow at runtime
 *                   up to @reserve_max value.
 * @reserve_max: Maximum number of channel descriptors. See @reserve_initial.
 * @is_open: True, if channel is open; false - otherwise.
 * @per_dtr_space: Per-descriptor space (in bytes) that channel user can utilize
 *                 to store per-operation control information.
 * HAL channel object. HAL devices (see xge_hal_device_t{}) contains
 * zero or more channels. HAL channel contains zero or more descriptors. The
 * latter are used by ULD(s) to manage the device and/or send and receive data
 * to remote peer(s) via the channel.
 *
 * See also: xge_hal_channel_type_e{}, xge_hal_channel_flag_e,
 * xge_hal_channel_callback_f{}
 */
typedef struct {
	/* complete/free section */
	xge_list_t			item;
	xge_hal_channel_callback_f	callback;
	void				**free_arr;
	int				length;
	int				free_length;
#if defined(XGE_HAL_RX_MULTI_FREE_IRQ) || defined(XGE_HAL_TX_MULTI_FREE_IRQ) || \
    defined(XGE_HAL_RX_MULTI_FREE) || defined(XGE_HAL_TX_MULTI_FREE)
	spinlock_t			free_lock;
#endif
	int				compl_index;
	unsigned int			usage_cnt;
	unsigned int			poll_bytes;
	int				unused0;

	/* reserve/post data path section */
#ifdef __XGE_WIN__
	int				__xge_os_attr_cacheline_aligned
					post_index;
#else
	int				post_index
					__xge_os_attr_cacheline_aligned;
#endif
	spinlock_t			reserve_lock;
	spinlock_t			post_lock;

	void				**reserve_arr;
	int				reserve_length;
	int				reserve_threshold;
	int				reserve_top;
	int                             unused1;

	/* common section */
	xge_hal_device_h		devh;
	pci_dev_h                       pdev;
	pci_reg_h			regh0;
	pci_reg_h			regh1;
	void				*userdata;
	void				**work_arr;
	void				**saved_arr;
	void				**orig_arr;
	xge_hal_stats_channel_info_t	stats;

	/* slow section */
	xge_hal_channel_type_e		type;
#ifdef XGEHAL_RNIC
	u32				vp_id;
#endif
	int				post_qid;
	int				compl_qid;
	xge_hal_channel_flag_e		flags;
	int				reserve_initial;
	int				reserve_max;
	int				is_open;
	int				per_dtr_space;
	xge_hal_channel_dtr_term_f	dtr_term;
	xge_hal_channel_dtr_init_f	dtr_init;
	/* MSI stuff */
	u32				msi_msg;
	u8				rti;
	u8				tti;
	u16                             unused2;
	/* MSI-X stuff */
	u64				msix_address;
	u32				msix_data;
	int				msix_idx;
	volatile int			in_interrupt;
        unsigned int			magic;
#ifdef __XGE_WIN__
} __xge_os_attr_cacheline_aligned xge_hal_channel_t ;
#else
} xge_hal_channel_t __xge_os_attr_cacheline_aligned;
#endif

/* ========================== CHANNEL PRIVATE API ========================= */

xge_hal_status_e
__hal_channel_initialize(xge_hal_channel_h channelh,
		xge_hal_channel_attr_t *attr, void **reserve_arr,
		int reserve_initial, int reserve_max, int reserve_threshold);

void __hal_channel_terminate(xge_hal_channel_h channelh);

xge_hal_channel_t*
__hal_channel_allocate(xge_hal_device_h devh, int post_qid,
#ifdef XGEHAL_RNIC
		u32 vp_id,
#endif
		xge_hal_channel_type_e	type);

void __hal_channel_free(xge_hal_channel_t *channel);

#if defined(XGE_DEBUG_FP) && (XGE_DEBUG_FP & XGE_DEBUG_FP_CHANNEL)
#define __HAL_STATIC_CHANNEL
#define __HAL_INLINE_CHANNEL

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL xge_hal_status_e
__hal_channel_dtr_alloc(xge_hal_channel_h channelh, xge_hal_dtr_h *dtrh);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
__hal_channel_dtr_post(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
__hal_channel_dtr_try_complete(xge_hal_channel_h channelh, xge_hal_dtr_h *dtrh);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
__hal_channel_dtr_complete(xge_hal_channel_h channelh);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
__hal_channel_dtr_free(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
__hal_channel_dtr_dealloc(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void
__hal_channel_dtr_restore(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
			  int offset);

/* ========================== CHANNEL PUBLIC API ========================= */

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL int
xge_hal_channel_dtr_count(xge_hal_channel_h channelh);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL void*
xge_hal_channel_userdata(xge_hal_channel_h channelh);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL int
xge_hal_channel_id(xge_hal_channel_h channelh);

__HAL_STATIC_CHANNEL __HAL_INLINE_CHANNEL int
xge_hal_check_alignment(dma_addr_t dma_pointer, int size, int alignment,
		int copy_size);

#else /* XGE_FASTPATH_EXTERN */
#define __HAL_STATIC_CHANNEL static
#define __HAL_INLINE_CHANNEL inline
#include <dev/nxge/xgehal/xgehal-channel-fp.c>
#endif /* XGE_FASTPATH_INLINE */

xge_hal_status_e
xge_hal_channel_open(xge_hal_device_h hldev, xge_hal_channel_attr_t *attr,
		     xge_hal_channel_h *channel,
		     xge_hal_channel_reopen_e reopen);

void xge_hal_channel_close(xge_hal_channel_h channelh,
                           xge_hal_channel_reopen_e reopen);

void xge_hal_channel_abort(xge_hal_channel_h channelh,
                           xge_hal_channel_reopen_e reopen);

__EXTERN_END_DECLS

#endif /* XGE_HAL_CHANNEL_H */
