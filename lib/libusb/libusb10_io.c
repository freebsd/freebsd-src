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

#include <sys/queue.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#include "libusb20.h"
#include "libusb20_desc.h"
#include "libusb20_int.h"
#include "libusb.h"
#include "libusb10.h"

UNEXPORTED int
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
	TAILQ_INSERT_TAIL(&ctx->pollfds, pollfd, list);
	pthread_mutex_unlock(&ctx->pollfds_lock);

	if (ctx->fd_added_cb)
		ctx->fd_added_cb(fd, events, ctx->fd_cb_user_data);
	return (0);
}

UNEXPORTED void
usb_remove_pollfd(libusb_context *ctx, int fd)
{
	struct usb_pollfd *pollfd;
	int found;

	found = 0;
	pthread_mutex_lock(&ctx->pollfds_lock);

	TAILQ_FOREACH(pollfd, &ctx->pollfds, list) {
		if (pollfd->pollfd.fd == fd) {
			found = 1;
			break ;
		}
	}

	if (found == 0) {
		pthread_mutex_unlock(&ctx->pollfds_lock);
		return ;
	}

	TAILQ_REMOVE(&ctx->pollfds, pollfd, list);
	pthread_mutex_unlock(&ctx->pollfds_lock);
	free(pollfd);

	if (ctx->fd_removed_cb)
		ctx->fd_removed_cb(fd, ctx->fd_cb_user_data);
}

UNEXPORTED void
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
	TAILQ_REMOVE(&ctx->flying_transfers, uxfer, list);
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

UNEXPORTED void
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
		TAILQ_FOREACH(cur, &ctx->flying_transfers, list) {
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

UNEXPORTED int 
get_next_timeout(libusb_context *ctx, struct timeval *tv, struct timeval *out)
{
	struct timeval timeout;

	if (libusb_get_next_timeout(ctx, &timeout)) {
		if (timerisset(&timeout) == 0)
			return 1;
		if (timercmp(&timeout, tv, <) != 0)
			*out = timeout;
		else
			*out = *tv;
	} else {
		*out = *tv;
	}

	return (0);
}	

UNEXPORTED int 
handle_timeouts(struct libusb_context *ctx)
{
	struct timespec sys_ts;
	struct timeval sys_tv;
	struct timeval *cur_tv;
	struct usb_transfer *xfer;
	struct libusb_transfer *uxfer;
	int ret;

	GET_CONTEXT(ctx);
	ret = 0;

	pthread_mutex_lock(&ctx->flying_transfers_lock);
	if (TAILQ_EMPTY(&ctx->flying_transfers))
		goto out;

	ret = clock_gettime(CLOCK_MONOTONIC, &sys_ts);
	TIMESPEC_TO_TIMEVAL(&sys_tv, &sys_ts);

	TAILQ_FOREACH(xfer, &ctx->flying_transfers, list) {
		cur_tv = &xfer->timeout;

		if (timerisset(cur_tv) == 0)
			goto out;
	
		if (xfer->flags & USB_TIMED_OUT)
			continue;
	
		if ((cur_tv->tv_sec > sys_tv.tv_sec) || (cur_tv->tv_sec == sys_tv.tv_sec &&
		    cur_tv->tv_usec > sys_tv.tv_usec))
			goto out;

		xfer->flags |= USB_TIMED_OUT;
		uxfer = (libusb_transfer *) ((uint8_t *)xfer +
		    sizeof(struct usb_transfer));
		ret = libusb_cancel_transfer(uxfer);
	}
out:
	pthread_mutex_unlock(&ctx->flying_transfers_lock);
	return (ret);
}

UNEXPORTED int
handle_events(struct libusb_context *ctx, struct timeval *tv)
{
	struct libusb_pollfd *tmppollfd;
	struct libusb_device_handle *devh;
	struct usb_pollfd *ipollfd;
	struct pollfd *fds;
	struct pollfd *tfds;
	nfds_t nfds;
	int tmpfd;
	int ret;
	int timeout;
	int i;
       
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "handle_events enter");

	nfds = 0;
	i = -1;

	pthread_mutex_lock(&ctx->pollfds_lock);
	TAILQ_FOREACH(ipollfd, &ctx->pollfds, list)
		nfds++;

	fds = alloca(sizeof(*fds) * nfds);
	if (fds == NULL)
		return (LIBUSB_ERROR_NO_MEM);

	TAILQ_FOREACH(ipollfd, &ctx->pollfds, list) {
		tmppollfd = &ipollfd->pollfd;
		tmpfd = tmppollfd->fd;
		i++;
		fds[i].fd = tmpfd;
		fds[i].events = tmppollfd->events;
		fds[i].revents = 0;
	}

	pthread_mutex_unlock(&ctx->pollfds_lock);

	timeout = (tv->tv_sec * 1000) + (tv->tv_usec / 1000);
	if (tv->tv_usec % 1000)
		timeout++;

	ret = poll(fds, nfds, timeout);
	if (ret == 0) 
		return (handle_timeouts(ctx));
	else if (ret == -1 && errno == EINTR)
		return (LIBUSB_ERROR_INTERRUPTED);
	else if (ret < 0) 
		return (LIBUSB_ERROR_IO);

	if (fds[0].revents) {
		if (ret == 1){
			ret = 0;
			goto handled;
		} else {
			fds[0].revents = 0;
			ret--;
		}
	}

	pthread_mutex_lock(&ctx->open_devs_lock);
	for (i = 0, devh = NULL ; i < nfds && ret > 0 ; i++) {

		tfds = &fds[i];
		if (!tfds->revents)
			continue;

		ret--;
		TAILQ_FOREACH(devh, &ctx->open_devs, list) {
			if (libusb20_dev_get_fd(devh->os_priv) == tfds->fd)
				break ;
		}

		if (tfds->revents & POLLERR) {
			usb_remove_pollfd(ctx, libusb20_dev_get_fd(devh->os_priv));
			if (devh != NULL)
				usb_handle_disconnect(devh);
			continue ;
		}


		pthread_mutex_lock(&libusb20_lock);
		DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "LIBUSB20_PROCESS");
		if (devh != NULL)
			ret = libusb20_dev_process(devh->os_priv);
		pthread_mutex_unlock(&libusb20_lock);


		if (ret == 0 || ret == LIBUSB20_ERROR_NO_DEVICE)
		       	continue;
		else if (ret < 0)
			goto out;
	}

	ret = 0;
out:
	pthread_mutex_unlock(&ctx->open_devs_lock);

handled:
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "handle_events leave");
	return ret;
}

/* Polling and timing */

int
libusb_try_lock_events(libusb_context * ctx)
{
	int ret;

	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_try_lock_events enter");

	pthread_mutex_lock(&ctx->pollfd_modify_lock);
	ret = ctx->pollfd_modify;
	pthread_mutex_unlock(&ctx->pollfd_modify_lock);

	if (ret != 0)
		return (1);

	ret = pthread_mutex_trylock(&ctx->events_lock);
	
	if (ret != 0)
		return (1);
	
	ctx->event_handler_active = 1;

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_try_lock_events leave");
	return (0);
}

void
libusb_lock_events(libusb_context * ctx)
{
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_lock_events enter");

	pthread_mutex_lock(&ctx->events_lock);
	ctx->event_handler_active = 1;

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_lock_events leave");
}

void
libusb_unlock_events(libusb_context * ctx)
{
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_unlock_events enter");

	ctx->event_handler_active = 0;
	pthread_mutex_unlock(&ctx->events_lock);

	pthread_mutex_lock(&ctx->event_waiters_lock);
	pthread_cond_broadcast(&ctx->event_waiters_cond);
	pthread_mutex_unlock(&ctx->event_waiters_lock);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_unlock_events leave");
}

int
libusb_event_handling_ok(libusb_context * ctx)
{
	int ret;

	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_event_handling_ok enter");

	pthread_mutex_lock(&ctx->pollfd_modify_lock);
	ret = ctx->pollfd_modify;
	pthread_mutex_unlock(&ctx->pollfd_modify_lock);

	if (ret != 0)
		return (0);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_event_handling_ok leave");
	return (1);
}

int
libusb_event_handler_active(libusb_context * ctx)
{
	int ret;

	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_event_handler_active enter");

	pthread_mutex_lock(&ctx->pollfd_modify_lock);
	ret = ctx->pollfd_modify;
	pthread_mutex_unlock(&ctx->pollfd_modify_lock);

	if (ret != 0)
		return (1);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_event_handler_active leave");
	return (ctx->event_handler_active);
}

void
libusb_lock_event_waiters(libusb_context * ctx)
{
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_lock_event_waiters enter");

	pthread_mutex_lock(&ctx->event_waiters_lock);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_lock_event_waiters leave");
}

void
libusb_unlock_event_waiters(libusb_context * ctx)
{
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_unlock_event_waiters enter");

	pthread_mutex_unlock(&ctx->event_waiters_lock);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_unlock_event_waiters leave");
}

int
libusb_wait_for_event(libusb_context * ctx, struct timeval *tv)
{
	int ret;
	struct timespec ts;

	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_wait_for_event enter");

	if (tv == NULL) {
		pthread_cond_wait(&ctx->event_waiters_cond, 
		    &ctx->event_waiters_lock);
		return (0);
	}

	ret = clock_gettime(CLOCK_REALTIME, &ts);
	if (ret < 0)
		return (LIBUSB_ERROR_OTHER);

	ts.tv_sec = tv->tv_sec;
	ts.tv_nsec = tv->tv_usec * 1000;
	if (ts.tv_nsec > 1000000000) {
		ts.tv_nsec -= 1000000000;
		ts.tv_sec++;
	}

	ret = pthread_cond_timedwait(&ctx->event_waiters_cond,
	    &ctx->event_waiters_lock, &ts);

	if (ret == ETIMEDOUT)
		return (1);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_wait_for_event leave");
	return (0);
}

int
libusb_handle_events_timeout(libusb_context * ctx, struct timeval *tv)
{
	struct timeval poll_timeout;
	int ret;
	
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_handle_events_timeout enter");

	ret = get_next_timeout(ctx, tv, &poll_timeout);
	if (ret != 0) {
		return handle_timeouts(ctx);
	}
retry:
	if (libusb_try_lock_events(ctx) == 0) {
		ret = handle_events(ctx, &poll_timeout);
		libusb_unlock_events(ctx);
		return ret;
	}

	libusb_lock_event_waiters(ctx);
	if (libusb_event_handler_active(ctx) == 0) {
		libusb_unlock_event_waiters(ctx);
		goto retry;
	}
	
	ret = libusb_wait_for_event(ctx, &poll_timeout);
	libusb_unlock_event_waiters(ctx);

	if (ret < 0)
		return ret;
	else if (ret == 1)
		return (handle_timeouts(ctx));

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_handle_events_timeout leave");
	return (0);
}

int
libusb_handle_events(libusb_context * ctx)
{
	struct timeval tv;
	int ret;

	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_handle_events enter");

	tv.tv_sec = 2;
	tv.tv_usec = 0;
	ret = libusb_handle_events_timeout(ctx, &tv);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_handle_events leave");
	return (ret);
}

int
libusb_handle_events_locked(libusb_context * ctx, struct timeval *tv)
{
	int ret;
	struct timeval poll_tv;

	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_handle_events_locked enter");

	ret = get_next_timeout(ctx, tv, &poll_tv);
	if (ret != 0) {
		return handle_timeouts(ctx);
	}

	ret = handle_events(ctx, &poll_tv);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_handle_events_locked leave");
	return (ret);
}

int
libusb_get_next_timeout(libusb_context * ctx, struct timeval *tv)
{
	struct usb_transfer *xfer;
	struct timeval *next_tv;
	struct timeval cur_tv;
	struct timespec cur_ts;
	int found;
	int ret;

	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_next_timeout enter");

	found = 0;
	pthread_mutex_lock(&ctx->flying_transfers_lock);
	if (TAILQ_EMPTY(&ctx->flying_transfers)) {
		pthread_mutex_unlock(&ctx->flying_transfers_lock);
		return (0);
	}

	TAILQ_FOREACH(xfer, &ctx->flying_transfers, list) {
		if (!(xfer->flags & USB_TIMED_OUT)) {
			found = 1;
			break ;
		}
	}
	pthread_mutex_unlock(&ctx->flying_transfers_lock);

	if (found == 0) {
		return 0;
	}

	next_tv = &xfer->timeout;
	if (timerisset(next_tv) == 0)
		return (0);

	ret = clock_gettime(CLOCK_MONOTONIC, &cur_ts);
       	if (ret < 0)
		return (LIBUSB_ERROR_OTHER);
	TIMESPEC_TO_TIMEVAL(&cur_tv, &cur_ts);	

	if (timercmp(&cur_tv, next_tv, >=) != 0)
		timerclear(tv);
	else
		timersub(next_tv, &cur_tv, tv);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_next_timeout leave");
	return (1);
}

void
libusb_set_pollfd_notifiers(libusb_context * ctx,
    libusb_pollfd_added_cb added_cb, libusb_pollfd_removed_cb removed_cb,
    void *user_data)
{
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_set_pollfd_notifiers enter");

	ctx->fd_added_cb = added_cb;
	ctx->fd_removed_cb = removed_cb;
	ctx->fd_cb_user_data = user_data;

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_set_pollfd_notifiers leave");
}

struct libusb_pollfd **
libusb_get_pollfds(libusb_context * ctx)
{
	struct usb_pollfd *pollfd;
	libusb_pollfd **ret;
	int i;

	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_pollfds enter");

	i = 0;
	pthread_mutex_lock(&ctx->pollfds_lock);
	TAILQ_FOREACH(pollfd, &ctx->pollfds, list)
		i++;

	ret = calloc(i + 1 , sizeof(struct libusb_pollfd *));
	if (ret == NULL) {
		pthread_mutex_unlock(&ctx->pollfds_lock);
		return (ret);
	}

	i = 0;
	TAILQ_FOREACH(pollfd, &ctx->pollfds, list)
		ret[i++] = (struct libusb_pollfd *) pollfd;
	ret[i] = NULL;

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_pollfds leave");
	return (ret);
}


/* Synchronous device I/O */

static void ctrl_tr_cb(struct libusb_transfer *transfer)
{
	libusb_context *ctx;
	int *complet;
       
	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "CALLBACK ENTER");

	complet = transfer->user_data;
	*complet = 1;
}

int
libusb_control_transfer(libusb_device_handle * devh,
    uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
    unsigned char *data, uint16_t wLength, unsigned int timeout)
{
	struct libusb_transfer *xfer;
	struct libusb_control_setup *ctr;
	libusb_context *ctx;
	unsigned char *buff;
	int complet;
	int ret;

	ctx = devh->dev->ctx;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_control_transfer enter");

	if (devh == NULL || data == NULL)
		return (LIBUSB_ERROR_NO_MEM);

	xfer = libusb_alloc_transfer(0);
	if (xfer == NULL)
		return (LIBUSB_ERROR_NO_MEM);

	buff = malloc(sizeof(libusb_control_setup) + wLength);
	if (buff == NULL) {
		libusb_free_transfer(xfer);
		return (LIBUSB_ERROR_NO_MEM);
	}

	ctr = (libusb_control_setup *)buff;
	ctr->bmRequestType = bmRequestType;
	ctr->bRequest = bRequest;
	ctr->wValue = wValue;
	ctr->wIndex = wIndex;
	ctr->wLength = wLength;
	if ((bmRequestType & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_OUT)
		memcpy(buff + sizeof(libusb_control_setup), data, wLength);

	xfer->dev_handle = devh;
	xfer->endpoint = 0;
	xfer->type = LIBUSB_TRANSFER_TYPE_CONTROL;
	xfer->timeout = timeout;
	xfer->buffer = buff;
	xfer->length = sizeof(libusb_control_setup) + wLength;
	xfer->user_data = &complet;
	xfer->callback = ctrl_tr_cb;
	xfer->flags = LIBUSB_TRANSFER_FREE_TRANSFER;
	complet = 0;

	if ((ret = libusb_submit_transfer(xfer)) < 0) {
		libusb_free_transfer(xfer);
		return (ret);
	}

	while (complet == 0)
		if ((ret = libusb_handle_events(ctx)) < 0) {
			libusb_cancel_transfer(xfer);
			while (complet == 0)
				if (libusb_handle_events(ctx) < 0) {
					break;
				}
			libusb_free_transfer(xfer);
			return (ret);
		}


	if ((bmRequestType & LIBUSB_ENDPOINT_DIR_MASK) == LIBUSB_ENDPOINT_IN)
		memcpy(data, buff + sizeof(libusb_control_setup), wLength);

	switch (xfer->status) {
	case LIBUSB_TRANSFER_COMPLETED:
		ret = xfer->actual_length;
		break;
	case LIBUSB_TRANSFER_TIMED_OUT:
	case LIBUSB_TRANSFER_STALL:
	case LIBUSB_TRANSFER_NO_DEVICE:
		ret = xfer->status;
		break;
	default:
		ret = LIBUSB_ERROR_OTHER;
	}
	libusb_free_transfer(xfer);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_control_transfer leave");
	return (ret);
}

static int
do_transfer(struct libusb_device_handle *devh, 
    unsigned char endpoint, unsigned char *data, int length,
    int *transferred, unsigned int timeout, int type)
{
	struct libusb_transfer *xfer;
	libusb_context *ctx;
	int complet;
	int ret;

	if (devh == NULL || data == NULL)
		return (LIBUSB_ERROR_NO_MEM);

	xfer = libusb_alloc_transfer(0);
	if (xfer == NULL)
		return (LIBUSB_ERROR_NO_MEM);

	ctx = devh->dev->ctx;
	GET_CONTEXT(ctx);

	xfer->dev_handle = devh;
	xfer->endpoint = endpoint;
	xfer->type = type;
	xfer->timeout = timeout;
	xfer->buffer = data;
	xfer->length = length;
	xfer->user_data = &complet;
	xfer->callback = ctrl_tr_cb;
	complet = 0;

	DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "SUBMIT_TRANSFER");
	if ((ret = libusb_submit_transfer(xfer)) < 0) {
		libusb_free_transfer(xfer);
		DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "SUBMIT_TRANSFER FAILED %i", ret);
		return (ret);
	}

	while (complet == 0) {
		if ((ret = libusb_handle_events(ctx)) < 0) {
			libusb_cancel_transfer(xfer);
			libusb_free_transfer(xfer);
			while (complet == 0) {
				if (libusb_handle_events(ctx) < 0)
					break ;
			}
			return (ret);
		}
	}

	*transferred = xfer->actual_length;
	DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "xfer->status %i", xfer->status);
	switch (xfer->status) {
	case LIBUSB_TRANSFER_COMPLETED:
		ret = xfer->actual_length;
		break;
	case LIBUSB_TRANSFER_TIMED_OUT:
	case LIBUSB_TRANSFER_OVERFLOW:
	case LIBUSB_TRANSFER_STALL:
	case LIBUSB_TRANSFER_NO_DEVICE:
		ret = xfer->status;
		break;
	default:
		ret = LIBUSB_ERROR_OTHER;
	}

	libusb_free_transfer(xfer);
	return (ret);
}

int
libusb_bulk_transfer(struct libusb_device_handle *devh,
    unsigned char endpoint, unsigned char *data, int length,
    int *transferred, unsigned int timeout)
{
	libusb_context *ctx;
	int ret;
	
	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_bulk_transfer enter");

	ret = do_transfer(devh, endpoint, data, length, transferred,
	    timeout, LIBUSB_TRANSFER_TYPE_BULK);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_bulk_transfer leave");
	return (ret);
}

/*
 * Need to fix xfer->type
 */
int
libusb_interrupt_transfer(struct libusb_device_handle *devh,
    unsigned char endpoint, unsigned char *data, int length, 
    int *transferred, unsigned int timeout)
{
	libusb_context *ctx;
	int ret;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_interrupt_transfer enter");

	ret = do_transfer(devh, endpoint, data, length, transferred,
	    timeout, LIBUSB_TRANSFER_TYPE_INTERRUPT);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_interrupt_transfer leave");
	return (ret);
}
