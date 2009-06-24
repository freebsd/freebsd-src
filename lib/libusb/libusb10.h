/* $FreeBSD$ */
/*-
 * Copyright (c) 2009 Sylvestre Gallon. All rights reserved.
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
 */

#ifndef __LIBUSB10_H__
#define __LIBUSB10_H__

/*
 * The two following macros were taken from the original LibUSB v1.0
 * for sake of compatibility:
 */
#define	USB_LIST_INIT(entry) \
	(entry)->prev = (entry)->next = entry;
#define USB_LIST_EMPTY(entry) \
	((entry)->next = (entry))

#define	LIST_ADD(entry, head) \
	(entry)->next = (head)->next; \
	(entry)->prev = (head); \
	(head)->next->prev = (entry); \
	(head)->next = (entry);
#define	LIST_ADD_TAIL(entry, head) \
	(entry)->next = (head); \
	(entry)->prev = (head)->prev; \
	(head)->prev->next = (entry); \
	(head)->prev = (entry);
#define	LIST_DEL(entry) \
	(entry)->next->prev = (entry)->prev; \
	(entry)->prev->next = (entry)->next;

#define LIST_ENT(ptr, type, member) \
	((type *)((char *)(ptr) - (unsigned long) (&((type*)0L)->member)))
#define LIST_FOREACH_ENTRY(pos, head, member) \
	for (pos = LIST_ENT((head)->next, typeof(*pos), member) ; \
	    &pos->member != head ; \
	    pos = LIST_ENT(pos->member.next, typeof(*pos), member))
#define LIST_FOREACH_ENTRY_SAFE(pos, n, head, member) \
	for (pos = LIST_ENT((head)->next, typeof(*pos), member), \
	    n = LIST_ENT(pos->member.next, typeof(*pos), member); \
	    &pos->member != (head); \
	    pos = n, n = LIST_ENT(n->member.next, typeof(*n), member))	

/* fetch libusb20_transfer from libusb20_device */
#define GET_XFER(xfer, endpoint, pdev)\
	xfer = libusb20_tr_get_pointer(pdev, \
	    (2 *endpoint)|(endpoint/0x80)); \
	if (xfer == NULL) \
		return (LIBUSB_ERROR_OTHER);


static int get_next_timeout(libusb_context *ctx, struct timeval *tv, struct timeval *out);
static int handle_timeouts(struct libusb_context *ctx);
static int handle_events(struct libusb_context *ctx, struct timeval *tv);
extern struct libusb_context *usbi_default_context;
extern pthread_mutex_t libusb20_lock; 

/* if ctx is NULL use default context*/

#define GET_CONTEXT(ctx) \
	if (ctx == NULL) ctx = usbi_default_context;

#define MAX(a,b) (((a)>(b))?(a):(b))
#define USB_TIMED_OUT (1<<0)

static inline void 
dprintf(libusb_context *ctx, int debug, char *str)
{
	if (ctx->debug != debug)
		return ;

	switch (ctx->debug) {
	case LIBUSB_DEBUG_NO:
		break ;
	case LIBUSB_DEBUG_FUNCTION:
		printf("LIBUSB FUNCTION : %s\n", str); 
		break ;
	case LIBUSB_DEBUG_TRANSFER:
		printf("LIBUSB TRANSFER : %s\n", str);
		break ;
	default:
		printf("LIBUSB UNKNOW DEBUG\n");
		break ;
	}
	return ;
}

struct usb_pollfd {
	struct libusb_pollfd pollfd;
	struct list_head list;
};

struct usb_transfer {
	int num_iso_packets;
	struct list_head list;
	struct timeval timeout;
	int transferred;
	uint8_t flags;
};

static inline int
usb_add_pollfd(libusb_context *ctx, int fd, short events)
{
	struct usb_pollfd *pollfd;

	if (ctx == NULL)
		return (LIBUSB_ERROR_INVALID_PARAM);
	
	pollfd = malloc(sizeof(*pollfd));
	if (pollfd == NULL)
		return (LIBUSB_ERROR_NO_MEM);

	pollfd->pollfd.fd = fd;
	pollfd->pollfd.events = events;

	pthread_mutex_lock(&ctx->pollfds_lock);
	LIST_ADD_TAIL(&pollfd->list, &ctx->pollfds);
	pthread_mutex_unlock(&ctx->pollfds_lock);

	if (ctx->fd_added_cb)
		ctx->fd_added_cb(fd, events, ctx->fd_cb_user_data);
	return (0);
}

static inline void
usb_remove_pollfd(libusb_context *ctx, int fd)
{
	struct usb_pollfd *pollfd;
	int found;

	found = 0;
	pthread_mutex_lock(&ctx->pollfds_lock);

	LIST_FOREACH_ENTRY(pollfd, &ctx->pollfds, list) {
		if (pollfd->pollfd.fd == fd) {
			found = 1;
			break ;
		}
	}

	if (found == 0) {
		pthread_mutex_unlock(&ctx->pollfds_lock);
		return ;
	}

	LIST_DEL(&pollfd->list);
	pthread_mutex_unlock(&ctx->pollfds_lock);
	free(pollfd);

	if (ctx->fd_removed_cb)
		ctx->fd_removed_cb(fd, ctx->fd_cb_user_data);
}

static inline void
usb_handle_transfer_completion(struct usb_transfer *uxfer, 
    enum libusb_transfer_status status)
{
	libusb_transfer *xfer;
	libusb_context *ctx;
	int len;

	xfer = (struct libusb_transfer *) ((uint8_t *)uxfer + 
	    sizeof(struct usb_transfer));
	ctx = xfer->dev_handle->dev->ctx;

	pthread_mutex_lock(&ctx->flying_transfers_lock);
	LIST_DEL(&uxfer->list);
	pthread_mutex_unlock(&ctx->flying_transfers_lock);

	if (status == LIBUSB_TRANSFER_COMPLETED && xfer->flags &
	    LIBUSB_TRANSFER_SHORT_NOT_OK) {
		len = xfer->length;
		if (xfer->type == LIBUSB_TRANSFER_TYPE_CONTROL)
			len -= sizeof(libusb_control_setup);
		if (len != uxfer->transferred) {
			status = LIBUSB_TRANSFER_ERROR;
		}
	}

	xfer->status = status;
	xfer->actual_length = uxfer->transferred;

	if (xfer->callback)
		xfer->callback(xfer);
	if (xfer->flags & LIBUSB_TRANSFER_FREE_TRANSFER)
		libusb_free_transfer(xfer);

	pthread_mutex_lock(&ctx->event_waiters_lock);
	pthread_cond_broadcast(&ctx->event_waiters_cond);
	pthread_mutex_unlock(&ctx->event_waiters_lock);
}

static inline void
usb_handle_disconnect(struct libusb_device_handle *devh)
{
	struct libusb_context *ctx;
	struct libusb_transfer *xfer;
	struct usb_transfer *cur;
	struct usb_transfer *to_cancel;

	ctx = devh->dev->ctx;

	while (1) {
		pthread_mutex_lock(&ctx->flying_transfers_lock);
		to_cancel = NULL;
		LIST_FOREACH_ENTRY(cur, &ctx->flying_transfers, list) {
			xfer = (struct libusb_transfer *) ((uint8_t *)cur + 
	    		    sizeof(struct usb_transfer));
			if (xfer->dev_handle == devh) {
				to_cancel = cur;
				break ;
			}
		}
		pthread_mutex_unlock(&ctx->flying_transfers_lock);

		if (to_cancel == NULL)
			break ;
		
		usb_handle_transfer_completion(to_cancel, LIBUSB_TRANSFER_NO_DEVICE);
	}
	return ;
}

#endif /*__LIBUSB10_H__*/
