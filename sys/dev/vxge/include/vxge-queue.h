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

#ifndef	VXGE_QUEUE_H
#define	VXGE_QUEUE_H

__EXTERN_BEGIN_DECLS

#define	VXGE_QUEUE_BUF_SIZE				0x1000
#define	VXGE_DEFAULT_EVENT_MAX_DATA_SIZE		16

/*
 * enum vxge_queue_status_e - Enumerates return codes of the vxge_queue
 * manipulation APIs.
 * @VXGE_QUEUE_IS_FULL: Queue is full, need to grow.
 * @VXGE_QUEUE_IS_EMPTY: Queue is empty.
 * @VXGE_QUEUE_OUT_OF_MEMORY: Out of memory.
 * @VXGE_QUEUE_NOT_ENOUGH_SPACE: Exceeded specified event size,
 * see vxge_queue_consume().
 * @VXGE_QUEUE_OK: Neither one of the codes listed above.
 *
 * Enumerates return codes of vxge_queue_consume()
 * and vxge_queue_produce() APIs.
 */
typedef enum vxge_queue_status_e {
	VXGE_QUEUE_OK			= 0,
	VXGE_QUEUE_IS_FULL		= 1,
	VXGE_QUEUE_IS_EMPTY		= 2,
	VXGE_QUEUE_OUT_OF_MEMORY	= 3,
	VXGE_QUEUE_NOT_ENOUGH_SPACE	= 4
} vxge_queue_status_e;

typedef void *vxge_queue_h;

/*
 * struct vxge_queue_item_t - Queue item.
 * @item: List item. Note that the queue is "built" on top of
 *	the bi-directional linked list.
 * @event_type: Event type. Includes (but is not restricted to)
 * one of the vxge_hal_event_e {} enumerated types.
 * @data_size: Size of the enqueued user data. Note that vxge_queue_t
 * items are allowed to have variable sizes.
 * @is_critical: For critical events, e.g. ECC.
 * @context: Opaque (void *) "context", for instance event producer object.
 *
 * Item of the vxge_queue_t {}. The queue is protected
 * in terms of multi-threaded concurrent access.
 * See also: vxge_queue_t {}.
 */
typedef struct vxge_queue_item_t {
	vxge_list_t		item;
	vxge_hal_event_e	event_type;
	u32			data_size;
	u32			is_critical;
	void			*context;
} vxge_queue_item_t;

/*
 * function vxge_queued_f - Item-enqueued callback.
 * @data: Per-queue context independent of the event. E.g., device handle.
 * @event_type: HAL or ULD-defined event type. Note that HAL own
 *	events are enumerated by vxge_hal_event_e {}.
 *
 * Per-queue optional callback. If not NULL, called by HAL each
 * time an event gets added to the queue.
 */
typedef void (*vxge_queued_f) (void *data, u32 event_type);

/*
 * struct vxge_queue_t - Protected dynamic queue of variable-size items.
 * @start_ptr: Points to the start of the queue.
 * @end_ptr: Points to the end of the queue.
 * @head_ptr: Points to the head of the queue. It gets changed during queue
 *	   produce/consume operations.
 * @tail_ptr: Points to the tail of the queue. It gets changed during queue
 *	   produce/consume operations.
 * @lock: Lock for queue operations(syncronization purpose).
 * @pages_initial:Number of pages to be initially allocated at the time
 *		 of queue creation.
 * @pages_max: Max number of pages that can be allocated in the queue.
 * @pages_current: Number of pages currently allocated
 * @list_head: Points to the list of queue elements that are produced, but yet
 *		to be consumed.
 * @hldev: HAL device handle
 * @pdev: PCI device handle
 * @irqh: PCI device IRQ handle.
 * @queued_func: Optional callback function to be called each time a new
 * item is added to the queue.
 * @queued_data: Arguments to the callback function.
 * @has_critical_event: Non-zero, if the queue contains a critical event,
 * see vxge_hal_event_e {}.
 * Protected dynamically growing queue. The queue is used to support multiple
 * producer/consumer type scenarios. The queue is a strict FIFO: first come
 * first served.
 * Queue users may "produce" (see vxge_queue_produce()) and "consume"
 * (see vxge_queue_consume()) items (a.k.a. events) variable sizes.
 * See also: vxge_queue_item_t {}.
 */
typedef struct vxge_queue_t {
	void			*start_ptr;
	void			*end_ptr;
	void			*head_ptr;
	void			*tail_ptr;
	spinlock_t		lock;
	u32			pages_initial;
	u32			pages_max;
	u32			pages_current;
	vxge_list_t		list_head;
	vxge_hal_device_h	hldev;
	pci_dev_h		pdev;
	pci_irq_h		irqh;
	vxge_queued_f		queued_func;
	void			*queued_data;
	u32			has_critical_event;
} vxge_queue_t;

/* ========================== PUBLIC API ================================= */

/*
 * vxge_queue_create - Create protected first-in-first-out queue.
 * @devh: HAL device handle.
 * @pages_initial: Number of pages to be initially allocated at the
 * time of queue creation.
 * @pages_max: Max number of pages that can be allocated in the queue.
 * @queued_func: Optional callback function to be called each time a new item is
 * added to the queue.
 * @queued_data: Argument to the callback function.
 *
 * Create protected (fifo) queue.
 *
 * Returns: Pointer to vxge_queue_t structure,
 * NULL - on failure.
 *
 * See also: vxge_queue_item_t {}, vxge_queue_destroy().
 */
vxge_queue_h
vxge_queue_create(vxge_hal_device_h devh,
    u32 pages_initial,
    u32 pages_max,
    vxge_queued_f queued_func,
    void *queued_data);

/*
 * vxge_queue_destroy - Destroy vxge_queue_t object.
 * @queueh: Queue handle.
 *
 * Destroy the specified vxge_queue_t object.
 *
 * See also: vxge_queue_item_t {}, vxge_queue_create().
 */
void
vxge_queue_destroy(vxge_queue_h queueh);

/*
 * vxge_queue_item_data - Get item's data.
 * @item: Queue item.
 *
 * Returns:  item data(variable size). Note that vxge_queue_t
 * contains items comprized of a fixed vxge_queue_item_t "header"
 * and a variable size data. This function returns the variable
 * user-defined portion of the queue item.
 */
void *
vxge_queue_item_data(vxge_queue_item_t *item);

/*
 * vxge_queue_produce - Enqueue an item (see vxge_queue_item_t {})
 *			 into the specified queue.
 * @queueh: Queue handle.
 * @event_type: Event type. One of the enumerated event types
 *		 that both consumer and producer "understand".
 *		 For an example, please refer to vxge_hal_event_e.
 * @context: Opaque (void *) "context", for instance event producer object.
 * @is_critical: For critical event, e.g. ECC.
 * @data_size: Size of the @data.
 * @data: User data of variable @data_size that is _copied_ into
 *	the new queue item (see vxge_queue_item_t {}). Upon return
 *	from the call the @data memory can be re-used or released.
 *
 * Enqueue a new item.
 *
 * Returns: VXGE_QUEUE_OK - success.
 * VXGE_QUEUE_IS_FULL - Queue is full.
 * VXGE_QUEUE_OUT_OF_MEMORY - Memory allocation failed.
 *
 * See also: vxge_queue_item_t {}, vxge_queue_consume().
 */
vxge_queue_status_e
vxge_queue_produce(vxge_queue_h queueh,
    u32 event_type,
    void *context,
    u32 is_critical,
    const u32 data_size,
    void *data);

/*
 * vxge_queue_produce_context - Enqueue context.
 * @queueh: Queue handle.
 * @event_type: Event type. One of the enumerated event types
 *		 that both consumer and producer "understand".
 *		 For an example, please refer to vxge_hal_event_e.
 * @context: Opaque (void *) "context", for instance event producer object.
 *
 * Enqueue Context.
 *
 * Returns: VXGE_QUEUE_OK - success.
 * VXGE_QUEUE_IS_EMPTY - Queue is empty.
 * VXGE_QUEUE_NOT_ENOUGH_SPACE - Requested item size(@data_max_size)
 * is too small to accomodate an item from the queue.
 *
 * See also: vxge_queue_item_t {}, vxge_queue_produce().
 */
static inline vxge_queue_status_e
/* LINTED */
vxge_queue_produce_context(vxge_queue_h queueh,
    u32 event_type,
    void *context)
{
	return (vxge_queue_produce(queueh, event_type, context, 0, 0, 0));
}

/*
 * vxge_queue_consume - Dequeue an item from the specified queue.
 * @queueh: Queue handle.
 * @data_max_size: Maximum expected size of the item.
 * @item: Memory area into which the item is _copied_ upon return
 *	from the function.
 *
 * Dequeue an item from the queue. The caller is required to provide
 * enough space for the item.
 *
 * Returns: VXGE_QUEUE_OK - success.
 * VXGE_QUEUE_IS_EMPTY - Queue is empty.
 * VXGE_QUEUE_NOT_ENOUGH_SPACE - Requested item size(@data_max_size)
 * is too small to accomodate an item from the queue.
 *
 * See also: vxge_queue_item_t {}, vxge_queue_produce().
 */
vxge_queue_status_e
vxge_queue_consume(vxge_queue_h queueh,
    u32 data_max_size,
    vxge_queue_item_t *item);

/*
 * vxge_queue_flush - Flush, or empty, the queue.
 * @queueh: Queue handle.
 *
 * Flush the queue, i.e. make it empty by consuming all events
 * without invoking the event processing logic (callbacks, etc.)
 */
void
vxge_queue_flush(vxge_queue_h queueh);

/*
 * vxge_io_queue_grow - Dynamically increases the size of the queue.
 * @queueh: Queue handle.
 *
 * This function is called in the case of no slot avaialble in the queue
 * to accomodate the newly received event.
 * Note that queue cannot grow beyond the max size specified for the
 * queue.
 *
 * Returns VXGE_QUEUE_OK: On success.
 * VXGE_QUEUE_OUT_OF_MEMORY : No memory is available.
 */
vxge_queue_status_e
vxge_io_queue_grow(vxge_queue_h qh);

/*
 * vxge_queue_get_reset_critical - Check for critical events in the queue,
 * @queueh: Queue handle.
 *
 * Check for critical event(s) in the queue, and reset the
 * "has-critical-event" flag upon return.
 * Returns: 1 - if the queue contains atleast one critical event.
 * 0 - If there are no critical events in the queue.
 */
u32
vxge_queue_get_reset_critical(vxge_queue_h queueh);

__EXTERN_END_DECLS

#endif	/* VXGE_QUEUE_H */
