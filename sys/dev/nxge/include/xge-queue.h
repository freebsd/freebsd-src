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
 *  FileName :    xge-queue.h
 *
 *  Description:  serialized event queue
 *
 *  Created:      7 June 2004
 */

#ifndef XGE_QUEUE_H
#define XGE_QUEUE_H

#include <dev/nxge/include/xge-os-pal.h>
#include <dev/nxge/include/xge-defs.h>
#include <dev/nxge/include/xge-list.h>
#include <dev/nxge/include/xgehal-event.h>

__EXTERN_BEGIN_DECLS

#define XGE_QUEUE_BUF_SIZE		0x1000
#define XGE_DEFAULT_EVENT_MAX_DATA_SIZE	16

/**
 * enum xge_queue_status_e - Enumerates return codes of the xge_queue
 * manipulation APIs.
 * @XGE_QUEUE_IS_FULL: Queue is full, need to grow.
 * @XGE_QUEUE_IS_EMPTY: Queue is empty.
 * @XGE_QUEUE_OUT_OF_MEMORY: Out of memory.
 * @XGE_QUEUE_NOT_ENOUGH_SPACE: Exceeded specified event size,
 * see xge_queue_consume().
 * @XGE_QUEUE_OK: Neither one of the codes listed above.
 *
 * Enumerates return codes of xge_queue_consume()
 * and xge_queue_produce() APIs.
 */
typedef enum xge_queue_status_e {
	XGE_QUEUE_OK			= 0,
	XGE_QUEUE_IS_FULL		= 1,
	XGE_QUEUE_IS_EMPTY		= 2,
	XGE_QUEUE_OUT_OF_MEMORY	        = 3,
	XGE_QUEUE_NOT_ENOUGH_SPACE	= 4
} xge_queue_status_e;

typedef void* xge_queue_h;

/**
 * struct xge_queue_item_t - Queue item.
 * @item: List item. Note that the queue is "built" on top of
 *        the bi-directional linked list.
 * @event_type: Event type. Includes (but is not restricted to)
 * one of the xge_hal_event_e{} enumerated types.
 * @data_size: Size of the enqueued user data. Note that xge_queue_t
 * items are allowed to have variable sizes.
 * @is_critical: For critical events, e.g. ECC.
 * @context: Opaque (void*) "context", for instance event producer object.
 *
 * Item of the xge_queue_t{}. The queue is protected
 * in terms of multi-threaded concurrent access.
 * See also: xge_queue_t{}.
 */
typedef struct xge_queue_item_t {
	xge_list_t			item;
	xge_hal_event_e		event_type;
	int					data_size;
	int					is_critical;
	void				*context;
} xge_queue_item_t;

/**
 * function xge_queued_f - Item-enqueued callback.
 * @data: Per-queue context independent of the event. E.g., device handle.
 * @event_type: HAL or ULD-defined event type. Note that HAL own
 *        events are enumerated by xge_hal_event_e{}.
 *
 * Per-queue optional callback. If not NULL, called by HAL each
 * time an event gets added to the queue.
 */
typedef void (*xge_queued_f) (void *data, int event_type);

/**
 * struct xge_queue_t - Protected dynamic queue of variable-size items.
 * @start_ptr: Points to the start of the queue.
 * @end_ptr: Points to the end of the queue.
 * @head_ptr: Points to the head of the queue. It gets changed during queue
 *            produce/consume operations.
 * @tail_ptr: Points to the tail of the queue. It gets changed during queue
 *            produce/consume operations.
 * @lock: Lock for queue operations(syncronization purpose).
 * @pages_initial:Number of pages to be initially allocated at the time
 *		  of queue creation.
 * @pages_max: Max number of pages that can be allocated in the queue.
 * @pages_current: Number of pages currently allocated
 * @list_head: Points to the list of queue elements that are produced, but yet
 *             to be consumed.
 * @signal_callback: (TODO)
 * @pdev: PCI device handle
 * @irqh: PCI device IRQ handle.
 * @queued_func: Optional callback function to be called each time a new
 * item is added to the queue.
 * @queued_data: Arguments to the callback function.
 * @has_critical_event: Non-zero, if the queue contains a critical event,
 * see xge_hal_event_e{}.
 * Protected dynamically growing queue. The queue is used to support multiple
 * producer/consumer type scenarios. The queue is a strict FIFO: first come
 * first served.
 * Queue users may "produce" (see xge_queue_produce()) and "consume"
 * (see xge_queue_consume()) items (a.k.a. events) variable sizes.
 * See also: xge_queue_item_t{}.
 */
typedef struct xge_queue_t {
	void				*start_ptr;
	void				*end_ptr;
	void				*head_ptr;
	void				*tail_ptr;
	spinlock_t			lock;
	unsigned int			pages_initial;
	unsigned int			pages_max;
	unsigned int			pages_current;
	xge_list_t			list_head;
	pci_dev_h                       pdev;
	pci_irq_h                       irqh;
	xge_queued_f			queued_func;
	void				*queued_data;
	int				has_critical_event;
} xge_queue_t;

/* ========================== PUBLIC API ================================= */

xge_queue_h xge_queue_create(pci_dev_h pdev, pci_irq_h irqh, int pages_initial,
		int pages_max, xge_queued_f queued_func, void *queued_data);

void xge_queue_destroy(xge_queue_h queueh);

void* xge_queue_item_data(xge_queue_item_t *item);

xge_queue_status_e
xge_queue_produce(xge_queue_h queueh, int event_type, void *context,
		int is_critical, const int data_size, void *data);

static inline xge_queue_status_e
xge_queue_produce_context(xge_queue_h queueh, int event_type, void *context) {
	return xge_queue_produce(queueh, event_type, context, 0, 0, 0);
}

xge_queue_status_e xge_queue_consume(xge_queue_h queueh, int data_max_size,
		xge_queue_item_t *item);

void xge_queue_flush(xge_queue_h queueh);

/* ========================== PRIVATE API ================================= */

xge_queue_status_e __io_queue_grow(xge_queue_h qh);

int __queue_get_reset_critical (xge_queue_h qh);

__EXTERN_END_DECLS

#endif /* XGE_QUEUE_H */
