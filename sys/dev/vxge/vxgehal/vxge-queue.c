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
 * vxge_queue_item_data - Get item's data.
 * @item: Queue item.
 *
 * Returns:  item data(variable size). Note that vxge_queue_t
 * contains items comprized of a fixed vxge_queue_item_t "header"
 * and a variable size data. This function returns the variable
 * user-defined portion of the queue item.
 */
void *
vxge_queue_item_data(vxge_queue_item_t *item)
{
	return (char *) item + sizeof(vxge_queue_item_t);
}

/*
 * __queue_consume - (Lockless) dequeue an item from the specified queue.
 *
 * @queue: Event queue.
 * @data_max_size: Maximum size of the data
 * @item: Queue item
 * See vxge_queue_consume().
 */
static vxge_queue_status_e
__queue_consume(vxge_queue_t *queue,
    u32 data_max_size,
    vxge_queue_item_t *item)
{
	int real_size;
	vxge_queue_item_t *elem;
	__hal_device_t *hldev;

	vxge_assert(queue != NULL);

	hldev = (__hal_device_t *) queue->hldev;

	vxge_hal_trace_log_queue("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_queue(
	    "queue = 0x"VXGE_OS_STXFMT", size = %d, item = 0x"VXGE_OS_STXFMT,
	    (ptr_t) queue, data_max_size, (ptr_t) item);

	if (vxge_list_is_empty(&queue->list_head)) {
		vxge_hal_trace_log_queue("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_QUEUE_IS_EMPTY);
		return (VXGE_QUEUE_IS_EMPTY);
	}

	elem = (vxge_queue_item_t *) queue->list_head.next;
	if (elem->data_size > data_max_size) {
		vxge_hal_trace_log_queue("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_QUEUE_NOT_ENOUGH_SPACE);
		return (VXGE_QUEUE_NOT_ENOUGH_SPACE);
	}

	vxge_list_remove(&elem->item);
	real_size = elem->data_size + sizeof(vxge_queue_item_t);
	if (queue->head_ptr == elem) {
		queue->head_ptr = (char *) queue->head_ptr + real_size;
		vxge_hal_info_log_queue("event_type: %d \
		    removing from the head: "
		    "0x"VXGE_OS_STXFMT":0x"VXGE_OS_STXFMT":0x"VXGE_OS_STXFMT
		    ":0x0x"VXGE_OS_STXFMT" elem 0x0x"VXGE_OS_STXFMT" length %d",
		    elem->event_type, (ptr_t) queue->start_ptr,
		    (ptr_t) queue->head_ptr, (ptr_t) queue->tail_ptr,
		    (ptr_t) queue->end_ptr, (ptr_t) elem, real_size);
	} else if ((char *) queue->tail_ptr - real_size == (char *) elem) {
		queue->tail_ptr = (char *) queue->tail_ptr - real_size;
		vxge_hal_info_log_queue("event_type: %d \
		    removing from the tail: "
		    "0x"VXGE_OS_STXFMT":0x"VXGE_OS_STXFMT":0x"VXGE_OS_STXFMT
		    ":0x"VXGE_OS_STXFMT" elem 0x"VXGE_OS_STXFMT" length %d",
		    elem->event_type, (ptr_t) queue->start_ptr,
		    (ptr_t) queue->head_ptr, (ptr_t) queue->tail_ptr,
		    (ptr_t) queue->end_ptr, (ptr_t) elem, real_size);
	} else {
		vxge_hal_info_log_queue("event_type: %d \
		    removing from the list: "
		    "0x"VXGE_OS_STXFMT":0x"VXGE_OS_STXFMT":0x"VXGE_OS_STXFMT
		    ":0x"VXGE_OS_STXFMT" elem 0x"VXGE_OS_STXFMT" length %d",
		    elem->event_type, (ptr_t) queue->start_ptr,
		    (ptr_t) queue->head_ptr, (ptr_t) queue->tail_ptr,
		    (ptr_t) queue->end_ptr, (ptr_t) elem, real_size);
	}
	vxge_assert(queue->tail_ptr >= queue->head_ptr);
	vxge_assert(queue->tail_ptr >= queue->start_ptr &&
	    queue->tail_ptr <= queue->end_ptr);
	vxge_assert(queue->head_ptr >= queue->start_ptr &&
	    queue->head_ptr < queue->end_ptr);
	vxge_os_memcpy(item, elem, sizeof(vxge_queue_item_t));
	vxge_os_memcpy(vxge_queue_item_data(item), vxge_queue_item_data(elem),
	    elem->data_size);

	if (vxge_list_is_empty(&queue->list_head)) {
		/* reset buffer pointers just to be clean */
		queue->head_ptr = queue->tail_ptr = queue->start_ptr;
	}

	vxge_hal_trace_log_queue("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_QUEUE_OK);
}

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
    void *data)
{
	vxge_queue_t *queue = (vxge_queue_t *) queueh;
	int real_size = data_size + sizeof(vxge_queue_item_t);
	__hal_device_t *hldev;
	vxge_queue_item_t *elem;
	unsigned long flags = 0;

	vxge_assert(queueh != NULL);

	hldev = (__hal_device_t *) queue->hldev;

	vxge_hal_trace_log_queue("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_queue(
	    "queueh = 0x"VXGE_OS_STXFMT", event_type = %d, "
	    "context = 0x"VXGE_OS_STXFMT", is_critical = %d, "
	    "data_size = %d, data = 0x"VXGE_OS_STXFMT,
	    (ptr_t) queueh, event_type, (ptr_t) context,
	    is_critical, data_size, (ptr_t) data);

	vxge_assert(real_size <= VXGE_QUEUE_BUF_SIZE);

	vxge_os_spin_lock_irq(&queue->lock, flags);

	if (is_critical && !queue->has_critical_event) {
		unsigned char item_buf[sizeof(vxge_queue_item_t) +
		    VXGE_DEFAULT_EVENT_MAX_DATA_SIZE];
		vxge_queue_item_t *item =
		    (vxge_queue_item_t *) (void *)item_buf;

		while (__queue_consume(queue, VXGE_DEFAULT_EVENT_MAX_DATA_SIZE,
		    item) != VXGE_QUEUE_IS_EMPTY) {
		}		/* do nothing */
	}

try_again:
	if ((char *) queue->tail_ptr + real_size <= (char *) queue->end_ptr) {
		elem = (vxge_queue_item_t *) queue->tail_ptr;
		queue->tail_ptr = (void *)((char *) queue->tail_ptr + real_size);
		vxge_hal_info_log_queue("event_type: %d adding to the tail: "
		    "0x"VXGE_OS_STXFMT":0x"VXGE_OS_STXFMT":0x"VXGE_OS_STXFMT
		    ":0x"VXGE_OS_STXFMT" elem 0x"VXGE_OS_STXFMT" length %d",
		    event_type, (ptr_t) queue->start_ptr,
		    (ptr_t) queue->head_ptr, (ptr_t) queue->tail_ptr,
		    (ptr_t) queue->end_ptr, (ptr_t) elem, real_size);
	} else if ((char *) queue->head_ptr - real_size >=
	    (char *) queue->start_ptr) {
		elem = (vxge_queue_item_t *)
		    ((void *)((char *) queue->head_ptr - real_size));
		queue->head_ptr = elem;
		vxge_hal_info_log_queue("event_type: %d adding to the head: "
		    "0x"VXGE_OS_STXFMT":0x"VXGE_OS_STXFMT":"
		    "0x"VXGE_OS_STXFMT":0x"VXGE_OS_STXFMT" length %d",
		    event_type, (ptr_t) queue->start_ptr,
		    (ptr_t) queue->head_ptr, (ptr_t) queue->tail_ptr,
		    (ptr_t) queue->end_ptr, real_size);
	} else {
		vxge_queue_status_e status;

		if (queue->pages_current >= queue->pages_max) {
			vxge_os_spin_unlock_irq(&queue->lock, flags);
			vxge_hal_trace_log_queue("<== %s:%s:%d Result = %d",
			    __FILE__, __func__, __LINE__, VXGE_QUEUE_IS_FULL);
			return (VXGE_QUEUE_IS_FULL);
		}

		if (queue->has_critical_event) {
			vxge_os_spin_unlock_irq(&queue->lock, flags);
			vxge_hal_trace_log_queue("<== %s:%s:%d Result = %d",
			    __FILE__, __func__, __LINE__, VXGE_QUEUE_IS_FULL);
			return (VXGE_QUEUE_IS_FULL);
		}

		/* grow */
		status = vxge_io_queue_grow(queueh);
		if (status != VXGE_QUEUE_OK) {
			vxge_os_spin_unlock_irq(&queue->lock, flags);
			vxge_hal_trace_log_queue("<== %s:%s:%d Result = %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}

		goto try_again;
	}
	vxge_assert(queue->tail_ptr >= queue->head_ptr);
	vxge_assert(queue->tail_ptr >= queue->start_ptr &&
	    queue->tail_ptr <= queue->end_ptr);
	vxge_assert(queue->head_ptr >= queue->start_ptr &&
	    queue->head_ptr < queue->end_ptr);
	elem->data_size = data_size;
	elem->event_type = (vxge_hal_event_e) event_type;
	elem->is_critical = is_critical;
	if (is_critical)
		queue->has_critical_event = 1;
	elem->context = context;
	vxge_os_memcpy(vxge_queue_item_data(elem), data, data_size);
	vxge_list_insert_before(&elem->item, &queue->list_head);
	vxge_os_spin_unlock_irq(&queue->lock, flags);

	/* no lock taken! */
	queue->queued_func(queue->queued_data, event_type);

	vxge_hal_trace_log_queue("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_QUEUE_OK);
}


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
    void *queued_data)
{
	vxge_queue_t *queue;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_queue("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_queue(
	    "devh = 0x"VXGE_OS_STXFMT", pages_initial = %d, "
	    "pages_max = %d, queued_func = 0x"VXGE_OS_STXFMT", "
	    "queued_data = 0x"VXGE_OS_STXFMT, (ptr_t) devh, pages_initial,
	    pages_max, (ptr_t) queued_func, (ptr_t) queued_data);

	if ((queue = (vxge_queue_t *) vxge_os_malloc(hldev->header.pdev,
	    sizeof(vxge_queue_t))) == NULL) {
		vxge_hal_trace_log_queue("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_QUEUE_OUT_OF_MEMORY);
		return (NULL);
	}

	queue->queued_func = queued_func;
	queue->queued_data = queued_data;
	queue->hldev = devh;
	queue->pdev = hldev->header.pdev;
	queue->irqh = hldev->header.irqh;
	queue->pages_current = pages_initial;
	queue->start_ptr = vxge_os_malloc(hldev->header.pdev,
	    queue->pages_current * VXGE_QUEUE_BUF_SIZE);
	if (queue->start_ptr == NULL) {
		vxge_os_free(hldev->header.pdev, queue, sizeof(vxge_queue_t));
		vxge_hal_trace_log_queue("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_QUEUE_OUT_OF_MEMORY);
		return (NULL);
	}
	queue->head_ptr = queue->tail_ptr = queue->start_ptr;
	queue->end_ptr = (char *) queue->start_ptr +
	    queue->pages_current * VXGE_QUEUE_BUF_SIZE;
	vxge_os_spin_lock_init_irq(&queue->lock, queue->irqh);
	queue->pages_initial = pages_initial;
	queue->pages_max = pages_max;
	vxge_list_init(&queue->list_head);

	vxge_hal_trace_log_queue("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (queue);
}

/*
 * vxge_queue_destroy - Destroy vxge_queue_t object.
 * @queueh: Queue handle.
 *
 * Destroy the specified vxge_queue_t object.
 *
 * See also: vxge_queue_item_t {}, vxge_queue_create().
 */
void
vxge_queue_destroy(vxge_queue_h queueh)
{
	vxge_queue_t *queue = (vxge_queue_t *) queueh;
	__hal_device_t *hldev;

	vxge_assert(queueh != NULL);

	hldev = (__hal_device_t *) queue->hldev;

	vxge_hal_trace_log_queue("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_queue("queueh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) queueh);

	vxge_os_spin_lock_destroy_irq(&queue->lock, queue->irqh);
	if (!vxge_list_is_empty(&queue->list_head)) {
		vxge_hal_trace_log_queue("destroying non-empty queue 0x"
		    VXGE_OS_STXFMT, (ptr_t) queue);
	}
	vxge_os_free(queue->pdev, queue->start_ptr, queue->pages_current *
	    VXGE_QUEUE_BUF_SIZE);

	vxge_os_free(queue->pdev, queue, sizeof(vxge_queue_t));

	vxge_hal_trace_log_queue("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_io_queue_grow - Dynamically increases the size of the queue.
 * @queueh: Queue handle.
 *
 * This function is called in the case of no slot avaialble in the queue
 * to accommodate the newly received event.
 * Note that queue cannot grow beyond the max size specified for the
 * queue.
 *
 * Returns VXGE_QUEUE_OK: On success.
 * VXGE_QUEUE_OUT_OF_MEMORY : No memory is available.
 */
vxge_queue_status_e
vxge_io_queue_grow(vxge_queue_h queueh)
{
	vxge_queue_t *queue = (vxge_queue_t *) queueh;
	__hal_device_t *hldev;
	void *newbuf, *oldbuf;
	vxge_list_t *item;
	vxge_queue_item_t *elem;

	vxge_assert(queueh != NULL);

	hldev = (__hal_device_t *) queue->hldev;

	vxge_hal_trace_log_queue("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_queue("queueh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) queueh);

	vxge_hal_info_log_queue("queue 0x"VXGE_OS_STXFMT":%d is growing",
	    (ptr_t) queue, queue->pages_current);

	newbuf = vxge_os_malloc(queue->pdev,
	    (queue->pages_current + 1) * VXGE_QUEUE_BUF_SIZE);
	if (newbuf == NULL) {
		vxge_hal_trace_log_queue("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_QUEUE_OUT_OF_MEMORY);
		return (VXGE_QUEUE_OUT_OF_MEMORY);
	}

	vxge_os_memcpy(newbuf, queue->start_ptr,
	    queue->pages_current * VXGE_QUEUE_BUF_SIZE);
	oldbuf = queue->start_ptr;

	/* adjust queue sizes */
	queue->start_ptr = newbuf;
	queue->end_ptr = (char *) newbuf +
	    (queue->pages_current + 1) * VXGE_QUEUE_BUF_SIZE;
	queue->tail_ptr = (char *) newbuf +
	/* LINTED */
	    ((char *) queue->tail_ptr - (char *) oldbuf);
	queue->head_ptr = (char *) newbuf +
	/* LINTED */
	    ((char *) queue->head_ptr - (char *) oldbuf);
	vxge_assert(!vxge_list_is_empty(&queue->list_head));
	queue->list_head.next = (vxge_list_t *) (void *)((char *) newbuf +
	/* LINTED */
	    ((char *) queue->list_head.next - (char *) oldbuf));
	queue->list_head.prev = (vxge_list_t *) (void *)((char *) newbuf +
	/* LINTED */
	    ((char *) queue->list_head.prev - (char *) oldbuf));
	/* adjust queue list */
	vxge_list_for_each(item, &queue->list_head) {
		elem = vxge_container_of(item, vxge_queue_item_t, item);
		if (elem->item.next != &queue->list_head) {
			elem->item.next =
			    (vxge_list_t *) (void *)((char *) newbuf +
			/* LINTED */
			    ((char *) elem->item.next - (char *) oldbuf));
		}
		if (elem->item.prev != &queue->list_head) {
			elem->item.prev =
			    (vxge_list_t *) (void *)((char *) newbuf +
			/* LINTED */
			    ((char *) elem->item.prev - (char *) oldbuf));
		}
	}
	vxge_os_free(queue->pdev, oldbuf,
	    queue->pages_current * VXGE_QUEUE_BUF_SIZE);
	queue->pages_current++;

	vxge_hal_trace_log_queue("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_QUEUE_OK);
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
 * is too small to accommodate an item from the queue.
 *
 * See also: vxge_queue_item_t {}, vxge_queue_produce().
 */
vxge_queue_status_e
vxge_queue_consume(vxge_queue_h queueh,
    u32 data_max_size,
    vxge_queue_item_t *item)
{
	vxge_queue_t *queue = (vxge_queue_t *) queueh;
	__hal_device_t *hldev;
	unsigned long flags = 0;
	vxge_queue_status_e status;

	vxge_assert(queueh != NULL);

	hldev = (__hal_device_t *) queue->hldev;

	vxge_hal_trace_log_queue("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_queue(
	    "queueh = 0x"VXGE_OS_STXFMT", data_max_size = %d, "
	    "item = 0x"VXGE_OS_STXFMT, (ptr_t) queueh,
	    data_max_size, (ptr_t) item);

	vxge_os_spin_lock_irq(&queue->lock, flags);
	status = __queue_consume(queue, data_max_size, item);
	vxge_os_spin_unlock_irq(&queue->lock, flags);

	vxge_hal_trace_log_queue("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}


/*
 * vxge_queue_flush - Flush, or empty, the queue.
 * @queueh: Queue handle.
 *
 * Flush the queue, i.e. make it empty by consuming all events
 * without invoking the event processing logic (callbacks, etc.)
 */
void
vxge_queue_flush(vxge_queue_h queueh)
{
	unsigned char item_buf[sizeof(vxge_queue_item_t) +
	    VXGE_DEFAULT_EVENT_MAX_DATA_SIZE];
	vxge_queue_item_t *item = (vxge_queue_item_t *) (void *)item_buf;
	vxge_queue_t *queue = (vxge_queue_t *) queueh;
	__hal_device_t *hldev;

	vxge_assert(queueh != NULL);

	hldev = (__hal_device_t *) queue->hldev;

	vxge_hal_trace_log_queue("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_queue("queueh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) queueh);

	/* flush queue by consuming all enqueued items */
	while (vxge_queue_consume(queueh, VXGE_DEFAULT_EVENT_MAX_DATA_SIZE,
	    item) != VXGE_QUEUE_IS_EMPTY) {
		/* do nothing */
		vxge_hal_trace_log_queue("item 0x"VXGE_OS_STXFMT"(%d) flushed",
		    (ptr_t) item, item->event_type);
	}

	(void) vxge_queue_get_reset_critical(queueh);

	vxge_hal_trace_log_queue("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

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
vxge_queue_get_reset_critical(vxge_queue_h queueh)
{
	vxge_queue_t *queue = (vxge_queue_t *) queueh;
	int c = queue->has_critical_event;
	__hal_device_t *hldev;

	vxge_assert(queueh != NULL);

	hldev = (__hal_device_t *) queue->hldev;

	vxge_hal_trace_log_queue("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_queue("queueh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) queueh);

	queue->has_critical_event = 0;

	vxge_hal_trace_log_queue("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
	return (c);
}
