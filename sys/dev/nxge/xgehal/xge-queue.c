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
 *  FileName :    xge-queue.c
 *
 *  Description:  serialized event queue
 *
 *  Created:      7 June 2004
 */

#include <dev/nxge/include/xge-queue.h>

/**
 * xge_queue_item_data - Get item's data.
 * @item: Queue item.
 *
 * Returns:  item data(variable size). Note that xge_queue_t
 * contains items comprized of a fixed xge_queue_item_t "header"
 * and a variable size data. This function returns the variable
 * user-defined portion of the queue item.
 */
void* xge_queue_item_data(xge_queue_item_t *item)
{
	return (char *)item + sizeof(xge_queue_item_t);
}

/*
 * __queue_consume - (Lockless) dequeue an item from the specified queue.
 *
 * @queue: Event queue.
 * See xge_queue_consume().
 */
static xge_queue_status_e
__queue_consume(xge_queue_t *queue, int data_max_size, xge_queue_item_t *item)
{
	int real_size;
	xge_queue_item_t *elem;

	if (xge_list_is_empty(&queue->list_head))
		return XGE_QUEUE_IS_EMPTY;

	elem = (xge_queue_item_t *)queue->list_head.next;
	if (elem->data_size > data_max_size)
		return XGE_QUEUE_NOT_ENOUGH_SPACE;

	xge_list_remove(&elem->item);
	real_size = elem->data_size + sizeof(xge_queue_item_t);
	if (queue->head_ptr == elem) {
		queue->head_ptr = (char *)queue->head_ptr + real_size;
		xge_debug_queue(XGE_TRACE,
			"event_type: %d removing from the head: "
			"0x"XGE_OS_LLXFMT":0x"XGE_OS_LLXFMT":0x"XGE_OS_LLXFMT
			":0x"XGE_OS_LLXFMT" elem 0x"XGE_OS_LLXFMT" length %d",
			elem->event_type,
			(u64)(ulong_t)queue->start_ptr,
			(u64)(ulong_t)queue->head_ptr,
			(u64)(ulong_t)queue->tail_ptr,
			(u64)(ulong_t)queue->end_ptr,
			(u64)(ulong_t)elem,
			real_size);
	} else if ((char *)queue->tail_ptr - real_size == (char*)elem) {
		queue->tail_ptr = (char *)queue->tail_ptr - real_size;
		xge_debug_queue(XGE_TRACE,
			"event_type: %d removing from the tail: "
			"0x"XGE_OS_LLXFMT":0x"XGE_OS_LLXFMT":0x"XGE_OS_LLXFMT
			":0x"XGE_OS_LLXFMT" elem 0x"XGE_OS_LLXFMT" length %d",
			elem->event_type,
			(u64)(ulong_t)queue->start_ptr,
			(u64)(ulong_t)queue->head_ptr,
			(u64)(ulong_t)queue->tail_ptr,
			(u64)(ulong_t)queue->end_ptr,
			(u64)(ulong_t)elem,
			real_size);
	} else {
		xge_debug_queue(XGE_TRACE,
			"event_type: %d removing from the list: "
			"0x"XGE_OS_LLXFMT":0x"XGE_OS_LLXFMT":0x"XGE_OS_LLXFMT
			":0x"XGE_OS_LLXFMT" elem 0x"XGE_OS_LLXFMT" length %d",
			elem->event_type,
			(u64)(ulong_t)queue->start_ptr,
			(u64)(ulong_t)queue->head_ptr,
			(u64)(ulong_t)queue->tail_ptr,
			(u64)(ulong_t)queue->end_ptr,
			(u64)(ulong_t)elem,
			real_size);
	}
	xge_assert(queue->tail_ptr >= queue->head_ptr);
	xge_assert(queue->tail_ptr >= queue->start_ptr &&
		    queue->tail_ptr <= queue->end_ptr);
	xge_assert(queue->head_ptr >= queue->start_ptr &&
		    queue->head_ptr < queue->end_ptr);
	xge_os_memcpy(item, elem, sizeof(xge_queue_item_t));
	xge_os_memcpy(xge_queue_item_data(item), xge_queue_item_data(elem),
		    elem->data_size);

	if (xge_list_is_empty(&queue->list_head)) {
		/* reset buffer pointers just to be clean */
		queue->head_ptr = queue->tail_ptr = queue->start_ptr;
	}
	return XGE_QUEUE_OK;
}

/**
 * xge_queue_produce - Enqueue an item (see xge_queue_item_t{})
 *                      into the specified queue.
 * @queueh: Queue handle.
 * @event_type: Event type. One of the enumerated event types
 *              that both consumer and producer "understand".
 *              For an example, please refer to xge_hal_event_e.
 * @context: Opaque (void*) "context", for instance event producer object.
 * @is_critical: For critical event, e.g. ECC.
 * @data_size: Size of the @data.
 * @data: User data of variable @data_size that is _copied_ into
 *        the new queue item (see xge_queue_item_t{}). Upon return
 *        from the call the @data memory can be re-used or released.
 *
 * Enqueue a new item.
 *
 * Returns: XGE_QUEUE_OK - success.
 * XGE_QUEUE_IS_FULL - Queue is full.
 * XGE_QUEUE_OUT_OF_MEMORY - Memory allocation failed.
 *
 * See also: xge_queue_item_t{}, xge_queue_consume().
 */
xge_queue_status_e
xge_queue_produce(xge_queue_h queueh, int event_type, void *context,
		int is_critical, const int data_size, void *data)
{
	xge_queue_t *queue = (xge_queue_t *)queueh;
	int real_size = data_size + sizeof(xge_queue_item_t);
	xge_queue_item_t *elem;
	unsigned long flags = 0;

	xge_assert(real_size <= XGE_QUEUE_BUF_SIZE);

	xge_os_spin_lock_irq(&queue->lock, flags);

	if (is_critical && !queue->has_critical_event)  {
		unsigned char item_buf[sizeof(xge_queue_item_t) +
				XGE_DEFAULT_EVENT_MAX_DATA_SIZE];
		xge_queue_item_t *item = (xge_queue_item_t *)(void *)item_buf;
    xge_os_memzero(item_buf, (sizeof(xge_queue_item_t) +
                             XGE_DEFAULT_EVENT_MAX_DATA_SIZE));  
	
	        while (__queue_consume(queue,
				       XGE_DEFAULT_EVENT_MAX_DATA_SIZE,
				       item) != XGE_QUEUE_IS_EMPTY)
		        ; /* do nothing */
	}

try_again:
	if ((char *)queue->tail_ptr + real_size <= (char *)queue->end_ptr) {
        elem = (xge_queue_item_t *) queue->tail_ptr;
		queue->tail_ptr = (void *)((char *)queue->tail_ptr + real_size);
		xge_debug_queue(XGE_TRACE,
			"event_type: %d adding to the tail: "
			"0x"XGE_OS_LLXFMT":0x"XGE_OS_LLXFMT":0x"XGE_OS_LLXFMT
			":0x"XGE_OS_LLXFMT" elem 0x"XGE_OS_LLXFMT" length %d",
			event_type,
			(u64)(ulong_t)queue->start_ptr,
			(u64)(ulong_t)queue->head_ptr,
			(u64)(ulong_t)queue->tail_ptr,
			(u64)(ulong_t)queue->end_ptr,
			(u64)(ulong_t)elem,
			real_size);
	} else if ((char *)queue->head_ptr - real_size >=
					(char *)queue->start_ptr) {
        elem = (xge_queue_item_t *) ((char *)queue->head_ptr - real_size);
		queue->head_ptr = elem;
		xge_debug_queue(XGE_TRACE,
			"event_type: %d adding to the head: "
			"0x"XGE_OS_LLXFMT":0x"XGE_OS_LLXFMT":0x"XGE_OS_LLXFMT
			":0x"XGE_OS_LLXFMT" length %d",
			event_type,
			(u64)(ulong_t)queue->start_ptr,
			(u64)(ulong_t)queue->head_ptr,
			(u64)(ulong_t)queue->tail_ptr,
			(u64)(ulong_t)queue->end_ptr,
			real_size);
	} else {
		xge_queue_status_e status;

		if (queue->pages_current >= queue->pages_max) {
			xge_os_spin_unlock_irq(&queue->lock, flags);
			return XGE_QUEUE_IS_FULL;
		}

		if (queue->has_critical_event) {
   		xge_os_spin_unlock_irq(&queue->lock, flags);
			return XGE_QUEUE_IS_FULL;
    }

		/* grow */
		status = __io_queue_grow(queueh);
		if (status != XGE_QUEUE_OK) {
			xge_os_spin_unlock_irq(&queue->lock, flags);
			return status;
		}

		goto try_again;
	}
	xge_assert(queue->tail_ptr >= queue->head_ptr);
	xge_assert(queue->tail_ptr >= queue->start_ptr &&
		    queue->tail_ptr <= queue->end_ptr);
	xge_assert(queue->head_ptr >= queue->start_ptr &&
		    queue->head_ptr < queue->end_ptr);
	elem->data_size = data_size;
    elem->event_type = (xge_hal_event_e) event_type;
	elem->is_critical = is_critical;
	if (is_critical)
	        queue->has_critical_event = 1;
	elem->context = context;
	xge_os_memcpy(xge_queue_item_data(elem), data, data_size);
	xge_list_insert_before(&elem->item, &queue->list_head);
	xge_os_spin_unlock_irq(&queue->lock, flags);

	/* no lock taken! */
	queue->queued_func(queue->queued_data, event_type);

	return XGE_QUEUE_OK;
}


/**
 * xge_queue_create - Create protected first-in-first-out queue.
 * @pdev: PCI device handle.
 * @irqh: PCI device IRQ handle.
 * @pages_initial: Number of pages to be initially allocated at the
 * time of queue creation.
 * @pages_max: Max number of pages that can be allocated in the queue.
 * @queued: Optional callback function to be called each time a new item is
 * added to the queue.
 * @queued_data: Argument to the callback function.
 *
 * Create protected (fifo) queue.
 *
 * Returns: Pointer to xge_queue_t structure,
 * NULL - on failure.
 *
 * See also: xge_queue_item_t{}, xge_queue_destroy().
 */
xge_queue_h
xge_queue_create(pci_dev_h pdev, pci_irq_h irqh, int pages_initial,
		int pages_max, xge_queued_f queued, void *queued_data)
{
	xge_queue_t *queue;

    if ((queue = (xge_queue_t *) xge_os_malloc(pdev, sizeof(xge_queue_t))) == NULL)
		return NULL;

	queue->queued_func = queued;
	queue->queued_data = queued_data;
	queue->pdev = pdev;
	queue->irqh = irqh;
	queue->pages_current = pages_initial;
	queue->start_ptr = xge_os_malloc(pdev, queue->pages_current *
	                               XGE_QUEUE_BUF_SIZE);
	if (queue->start_ptr == NULL) {
		xge_os_free(pdev, queue, sizeof(xge_queue_t));
		return NULL;
	}
	queue->head_ptr = queue->tail_ptr = queue->start_ptr;
	queue->end_ptr = (char *)queue->start_ptr +
		queue->pages_current * XGE_QUEUE_BUF_SIZE;
	xge_os_spin_lock_init_irq(&queue->lock, irqh);
	queue->pages_initial = pages_initial;
	queue->pages_max = pages_max;
	xge_list_init(&queue->list_head);

	return queue;
}

/**
 * xge_queue_destroy - Destroy xge_queue_t object.
 * @queueh: Queue handle.
 *
 * Destroy the specified xge_queue_t object.
 *
 * See also: xge_queue_item_t{}, xge_queue_create().
 */
void xge_queue_destroy(xge_queue_h queueh)
{
	xge_queue_t *queue = (xge_queue_t *)queueh;
	xge_os_spin_lock_destroy_irq(&queue->lock, queue->irqh);
	if (!xge_list_is_empty(&queue->list_head)) {
		xge_debug_queue(XGE_ERR, "destroying non-empty queue 0x"
				XGE_OS_LLXFMT, (u64)(ulong_t)queue);
	}
	xge_os_free(queue->pdev, queue->start_ptr, queue->pages_current *
	          XGE_QUEUE_BUF_SIZE);

	xge_os_free(queue->pdev, queue, sizeof(xge_queue_t));
}

/*
 * __io_queue_grow - Dynamically increases the size of the queue.
 * @queueh: Queue handle.
 *
 * This function is called in the case of no slot avaialble in the queue
 * to accomodate the newly received event.
 * Note that queue cannot grow beyond the max size specified for the
 * queue.
 *
 * Returns XGE_QUEUE_OK: On success.
 * XGE_QUEUE_OUT_OF_MEMORY : No memory is available.
 */
xge_queue_status_e
__io_queue_grow(xge_queue_h queueh)
{
	xge_queue_t *queue = (xge_queue_t *)queueh;
	void *newbuf, *oldbuf;
	xge_list_t *item;
	xge_queue_item_t *elem;

	xge_debug_queue(XGE_TRACE, "queue 0x"XGE_OS_LLXFMT":%d is growing",
			 (u64)(ulong_t)queue, queue->pages_current);

	newbuf = xge_os_malloc(queue->pdev,
	        (queue->pages_current + 1) * XGE_QUEUE_BUF_SIZE);
	if (newbuf == NULL)
		return XGE_QUEUE_OUT_OF_MEMORY;

	xge_os_memcpy(newbuf, queue->start_ptr,
	       queue->pages_current * XGE_QUEUE_BUF_SIZE);
	oldbuf = queue->start_ptr;

	/* adjust queue sizes */
	queue->start_ptr = newbuf;
	queue->end_ptr = (char *)newbuf +
			(queue->pages_current + 1) * XGE_QUEUE_BUF_SIZE;
	queue->tail_ptr = (char *)newbuf + ((char *)queue->tail_ptr -
					    (char *)oldbuf);
	queue->head_ptr = (char *)newbuf + ((char *)queue->head_ptr -
					    (char *)oldbuf);
	xge_assert(!xge_list_is_empty(&queue->list_head));
	queue->list_head.next = (xge_list_t *) (void *)((char *)newbuf +
			((char *)queue->list_head.next - (char *)oldbuf));
	queue->list_head.prev = (xge_list_t *) (void *)((char *)newbuf +
			((char *)queue->list_head.prev - (char *)oldbuf));
	/* adjust queue list */
	xge_list_for_each(item, &queue->list_head) {
		elem = xge_container_of(item, xge_queue_item_t, item);
		if (elem->item.next != &queue->list_head) {
			elem->item.next =
				(xge_list_t*)(void *)((char *)newbuf +
				 ((char *)elem->item.next - (char *)oldbuf));
		}
		if (elem->item.prev != &queue->list_head) {
			elem->item.prev =
				(xge_list_t*) (void *)((char *)newbuf +
				 ((char *)elem->item.prev - (char *)oldbuf));
		}
	}
	xge_os_free(queue->pdev, oldbuf,
		  queue->pages_current * XGE_QUEUE_BUF_SIZE);
	queue->pages_current++;

	return XGE_QUEUE_OK;
}

/**
 * xge_queue_consume - Dequeue an item from the specified queue.
 * @queueh: Queue handle.
 * @data_max_size: Maximum expected size of the item.
 * @item: Memory area into which the item is _copied_ upon return
 *        from the function.
 *
 * Dequeue an item from the queue. The caller is required to provide
 * enough space for the item.
 *
 * Returns: XGE_QUEUE_OK - success.
 * XGE_QUEUE_IS_EMPTY - Queue is empty.
 * XGE_QUEUE_NOT_ENOUGH_SPACE - Requested item size(@data_max_size)
 * is too small to accomodate an item from the queue.
 *
 * See also: xge_queue_item_t{}, xge_queue_produce().
 */
xge_queue_status_e
xge_queue_consume(xge_queue_h queueh, int data_max_size, xge_queue_item_t *item)
{
	xge_queue_t *queue = (xge_queue_t *)queueh;
	unsigned long flags = 0;
	xge_queue_status_e status;

	xge_os_spin_lock_irq(&queue->lock, flags);
	status = __queue_consume(queue, data_max_size, item);
	xge_os_spin_unlock_irq(&queue->lock, flags);

	return status;
}


/**
 * xge_queue_flush - Flush, or empty, the queue.
 * @queueh: Queue handle.
 *
 * Flush the queue, i.e. make it empty by consuming all events
 * without invoking the event processing logic (callbacks, etc.)
 */
void xge_queue_flush(xge_queue_h queueh)
{
	unsigned char item_buf[sizeof(xge_queue_item_t) +
				XGE_DEFAULT_EVENT_MAX_DATA_SIZE];
	xge_queue_item_t *item = (xge_queue_item_t *)(void *)item_buf;
  xge_os_memzero(item_buf, (sizeof(xge_queue_item_t) +
                             XGE_DEFAULT_EVENT_MAX_DATA_SIZE));  
	  
	/* flush queue by consuming all enqueued items */
	while (xge_queue_consume(queueh,
				    XGE_DEFAULT_EVENT_MAX_DATA_SIZE,
				    item) != XGE_QUEUE_IS_EMPTY) {
		/* do nothing */
		xge_debug_queue(XGE_TRACE, "item "XGE_OS_LLXFMT"(%d) flushed",
				 item, item->event_type);
	}
	(void) __queue_get_reset_critical (queueh);
}

/*
 * __queue_get_reset_critical - Check for critical events in the queue,
 * @qh: Queue handle.
 *
 * Check for critical event(s) in the queue, and reset the
 * "has-critical-event" flag upon return.
 * Returns: 1 - if the queue contains atleast one critical event.
 * 0 - If there are no critical events in the queue.
 */
int __queue_get_reset_critical (xge_queue_h qh) {
	xge_queue_t* queue = (xge_queue_t*)qh;
	int c = queue->has_critical_event;

	queue->has_critical_event = 0;
        return c;
}
