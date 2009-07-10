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

static pthread_mutex_t default_context_lock = PTHREAD_MUTEX_INITIALIZER;
struct libusb_context *usbi_default_context = NULL;
pthread_mutex_t libusb20_lock = PTHREAD_MUTEX_INITIALIZER;

/*  Library initialisation / deinitialisation */

void
libusb_set_debug(libusb_context * ctx, int level)
{
	GET_CONTEXT(ctx);
	if (ctx)
		ctx->debug = level;
}

int
libusb_init(libusb_context ** context)
{
	struct libusb_context *ctx;
	char * debug;
	int ret;

	ctx = malloc(sizeof(*ctx));
	if (!ctx)
		return (LIBUSB_ERROR_INVALID_PARAM);

	memset(ctx, 0, sizeof(*ctx));

	debug = getenv("LIBUSB_DEBUG");
	if (debug != NULL) {
		ctx->debug = atoi(debug);
		if (ctx->debug != 0)
			ctx->debug_fixed = 1;
	}

	pthread_mutex_init(&ctx->usb_devs_lock, NULL);
	pthread_mutex_init(&ctx->open_devs_lock, NULL);
	TAILQ_INIT(&ctx->usb_devs);
	TAILQ_INIT(&ctx->open_devs);

	pthread_mutex_init(&ctx->flying_transfers_lock, NULL);
	pthread_mutex_init(&ctx->pollfds_lock, NULL);
	pthread_mutex_init(&ctx->pollfd_modify_lock, NULL);
	pthread_mutex_init(&ctx->events_lock, NULL);
	pthread_mutex_init(&ctx->event_waiters_lock, NULL);
	pthread_cond_init(&ctx->event_waiters_cond, NULL);

	TAILQ_INIT(&ctx->flying_transfers);
	TAILQ_INIT(&ctx->pollfds);

	ret = pipe(ctx->ctrl_pipe);
	if (ret < 0) {
		usb_remove_pollfd(ctx, ctx->ctrl_pipe[0]);
		close(ctx->ctrl_pipe[0]);
		close(ctx->ctrl_pipe[1]);
		free(ctx);
		return (LIBUSB_ERROR_OTHER);
	}

	ret = usb_add_pollfd(ctx, ctx->ctrl_pipe[0], POLLIN);
	if (ret < 0) {
		usb_remove_pollfd(ctx, ctx->ctrl_pipe[0]);
		close(ctx->ctrl_pipe[0]);
		close(ctx->ctrl_pipe[1]);
		free(ctx);
		return ret;
	}

	pthread_mutex_lock(&default_context_lock);
	if (usbi_default_context == NULL) {
		usbi_default_context = ctx;
	}
	pthread_mutex_unlock(&default_context_lock);

	if (context)
		*context = ctx;

	return (0);
}

void
libusb_exit(libusb_context * ctx)
{
	GET_CONTEXT(ctx);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_exit enter");
	usb_remove_pollfd(ctx, ctx->ctrl_pipe[0]);
	close(ctx->ctrl_pipe[0]);
	close(ctx->ctrl_pipe[1]);

	pthread_mutex_lock(&default_context_lock);
	if (ctx == usbi_default_context) {
		usbi_default_context = NULL;
	}
	pthread_mutex_unlock(&default_context_lock);

	free(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_exit leave");
}

/* Device handling and initialisation. */

ssize_t
libusb_get_device_list(libusb_context * ctx, libusb_device *** list)
{
	struct libusb20_device *pdev;
	struct LIBUSB20_DEVICE_DESC_DECODED *ddesc;
	struct libusb_device *dev;
	struct libusb20_backend *usb_backend;
	int i;

	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_device_list enter");

	usb_backend = libusb20_be_alloc_default();
	if (usb_backend == NULL)
		return (-1);

	pdev = NULL;
	i = 0;
	while ((pdev = libusb20_be_device_foreach(usb_backend, pdev)))
		i++;

	if (list == NULL) {
		libusb20_be_free(usb_backend);
		return (LIBUSB_ERROR_INVALID_PARAM);
	}
	*list = malloc((i + 1) * sizeof(void *));
	if (*list == NULL) {
		libusb20_be_free(usb_backend);
		return (LIBUSB_ERROR_NO_MEM);
	}
	i = 0;
	while ((pdev = libusb20_be_device_foreach(usb_backend, NULL))) {
		/* get device into libUSB v1.0 list */
		libusb20_be_dequeue_device(usb_backend, pdev);

		ddesc = libusb20_dev_get_device_desc(pdev);
		dev = malloc(sizeof(*dev));
		if (dev == NULL) {
			while (i != 0) {
				libusb_unref_device((*list)[i - 1]);
				i--;
			}
			free(*list);
			libusb20_be_free(usb_backend);
			return (LIBUSB_ERROR_NO_MEM);
		}
		memset(dev, 0, sizeof(*dev));

		pthread_mutex_init(&dev->lock, NULL);
		dev->ctx = ctx;
		dev->bus_number = pdev->bus_number;
		dev->device_address = pdev->device_address;
		dev->num_configurations = ddesc->bNumConfigurations;

		/* link together the two structures */
		dev->os_priv = pdev;

		pthread_mutex_lock(&ctx->usb_devs_lock);
		TAILQ_INSERT_HEAD(&ctx->usb_devs, dev, list);
		pthread_mutex_unlock(&ctx->usb_devs_lock);

		(*list)[i] = libusb_ref_device(dev);
		i++;
	}
	(*list)[i] = NULL;

	libusb20_be_free(usb_backend);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_device_list leave");
	return (i);
}

/*
 * In this function we cant free all the device contained into list because
 * open_with_pid_vid use some node of list after the free_device_list.
 */
void
libusb_free_device_list(libusb_device **list, int unref_devices)
{
	int i;
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_free_device_list enter");

	if (list == NULL)
		return ;

	if (unref_devices) {
		for (i = 0; list[i] != NULL; i++)
			libusb_unref_device(list[i]);
	}
	free(list);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_free_device_list leave");
}

uint8_t
libusb_get_bus_number(libusb_device * dev)
{
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_bus_number enter");

	if (dev == NULL)
		return (LIBUSB_ERROR_NO_DEVICE);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_bus_number leave");
	return (dev->bus_number);
}

uint8_t
libusb_get_device_address(libusb_device * dev)
{
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_device_address enter");

	if (dev == NULL)
		return (LIBUSB_ERROR_NO_DEVICE);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_device_address leave");
	return (dev->device_address);
}

int
libusb_get_max_packet_size(libusb_device *dev, unsigned char endpoint)
{
	struct libusb_config_descriptor *pdconf;
	struct libusb_interface *pinf;
	struct libusb_interface_descriptor *pdinf;
	struct libusb_endpoint_descriptor *pdend;
	libusb_context *ctx;
	int i, j, k, ret;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_max_packet_size enter");

	if (dev == NULL)
		return (LIBUSB_ERROR_NO_DEVICE);

	if (libusb_get_active_config_descriptor(dev, &pdconf) < 0) 
		return (LIBUSB_ERROR_OTHER);
 
	ret = LIBUSB_ERROR_NOT_FOUND;
	for (i = 0 ; i < pdconf->bNumInterfaces ; i++) {
		pinf = &pdconf->interface[i];
		for (j = 0 ; j < pinf->num_altsetting ; j++) {
			pdinf = &pinf->altsetting[j];
			for (k = 0 ; k < pdinf->bNumEndpoints ; k++) {
				pdend = &pdinf->endpoint[k];
				if (pdend->bEndpointAddress == endpoint) {
					ret = pdend->wMaxPacketSize;
					goto out;
				}
			}
		}
	}

out:
	libusb_free_config_descriptor(pdconf);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_max_packet_size leave");
	return (ret);
}

libusb_device *
libusb_ref_device(libusb_device * dev)
{
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_ref_device enter");

	if (dev == NULL)
		return (NULL);

	pthread_mutex_lock(&dev->lock);
	dev->refcnt++;
	pthread_mutex_unlock(&dev->lock);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_ref_device leave");
	return (dev);
}

void
libusb_unref_device(libusb_device * dev)
{
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_unref_device enter");

	if (dev == NULL)
		return;

	pthread_mutex_lock(&dev->lock);
	dev->refcnt--;
	pthread_mutex_unlock(&dev->lock);

	if (dev->refcnt == 0) {
		pthread_mutex_lock(&dev->ctx->usb_devs_lock);
		TAILQ_REMOVE(&ctx->usb_devs, dev, list);
		pthread_mutex_unlock(&dev->ctx->usb_devs_lock);

		libusb20_dev_free(dev->os_priv);
		free(dev);
	}
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_unref_device leave");
}

int
libusb_open(libusb_device * dev, libusb_device_handle **devh)
{
	libusb_context *ctx = dev->ctx;
	struct libusb20_device *pdev = dev->os_priv;
	libusb_device_handle *hdl;
	unsigned char dummy;
	int err;

	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_open enter");

	dummy = 1;
	if (devh == NULL)
		return (LIBUSB_ERROR_INVALID_PARAM);

	hdl = malloc(sizeof(*hdl));
	if (hdl == NULL)
		return (LIBUSB_ERROR_NO_MEM);

	err = libusb20_dev_open(pdev, 16 * 4 /* number of endpoints */ );
	if (err) {
		free(hdl);
		return (LIBUSB_ERROR_NO_MEM);
	}
	memset(hdl, 0, sizeof(*hdl));
	pthread_mutex_init(&hdl->lock, NULL);

	TAILQ_INIT(&hdl->ep_list);
	hdl->dev = libusb_ref_device(dev);
	hdl->claimed_interfaces = 0;
	hdl->os_priv = dev->os_priv;
	err = usb_add_pollfd(ctx, libusb20_dev_get_fd(pdev), POLLIN |
	    POLLOUT | POLLRDNORM | POLLWRNORM);
	if (err < 0) {
		libusb_unref_device(dev);
		free(hdl);
		return (err);
	}

	pthread_mutex_lock(&ctx->open_devs_lock);
	TAILQ_INSERT_HEAD(&ctx->open_devs, hdl, list);
	pthread_mutex_unlock(&ctx->open_devs_lock);

	*devh = hdl;

	pthread_mutex_lock(&ctx->pollfd_modify_lock);
	ctx->pollfd_modify++;
	pthread_mutex_unlock(&ctx->pollfd_modify_lock);	

	err = write(ctx->ctrl_pipe[1], &dummy, sizeof(dummy));
	if (err <= 0) {
		pthread_mutex_lock(&ctx->pollfd_modify_lock);
		ctx->pollfd_modify--;
		pthread_mutex_unlock(&ctx->pollfd_modify_lock);
		return 0;
	}

	libusb_lock_events(ctx);
	read(ctx->ctrl_pipe[0], &dummy, sizeof(dummy));
	pthread_mutex_lock(&ctx->pollfd_modify_lock);
	ctx->pollfd_modify--;
	pthread_mutex_unlock(&ctx->pollfd_modify_lock);
	libusb_unlock_events(ctx);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_open leave");
	return (0);
}

libusb_device_handle *
libusb_open_device_with_vid_pid(libusb_context * ctx, uint16_t vendor_id,
    uint16_t product_id)
{
	struct libusb_device **devs;
	struct libusb_device_handle *devh;
	struct libusb20_device *pdev;
	struct LIBUSB20_DEVICE_DESC_DECODED *pdesc;
	int i, j;

	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_open_device_width_vid_pid enter");

	devh = NULL;

	if ((i = libusb_get_device_list(ctx, &devs)) < 0)
		return (NULL);

	for (j = 0; j < i; j++) {
		pdev = (struct libusb20_device *)devs[j]->os_priv;
		pdesc = libusb20_dev_get_device_desc(pdev);
		if (pdesc->idVendor == vendor_id &&
		    pdesc->idProduct == product_id) {
			if (libusb_open(devs[j], &devh) < 0)
				devh = NULL;
			break ;
		}
	}

	libusb_free_device_list(devs, 1);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_open_device_width_vid_pid leave");
	return (devh);
}

void
libusb_close(libusb_device_handle * devh)
{
	libusb_context *ctx;
	struct libusb20_device *pdev;
	struct usb_ep_tr *eptr;
	unsigned char dummy = 1;
	int err;

	if (devh == NULL)
		return ;

	ctx = devh->dev->ctx;
	pdev = devh->os_priv;

	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_close enter");

	pthread_mutex_lock(&ctx->pollfd_modify_lock);
	ctx->pollfd_modify++;
	pthread_mutex_unlock(&ctx->pollfd_modify_lock);

	err = write(ctx->ctrl_pipe[1], &dummy, sizeof(dummy));
	
	if (err <= 0) {
		pthread_mutex_lock(&ctx->open_devs_lock);
		TAILQ_REMOVE(&ctx->open_devs, devh, list);
		pthread_mutex_unlock(&ctx->open_devs_lock);

		usb_remove_pollfd(ctx, libusb20_dev_get_fd(pdev));
		libusb20_dev_close(pdev);
		libusb_unref_device(devh->dev);
		TAILQ_FOREACH(eptr, &devh->ep_list, list) {
			TAILQ_REMOVE(&devh->ep_list, eptr, list);
			libusb20_tr_close(((struct libusb20_transfer **)
			    eptr->os_priv)[0]);
			if (eptr->flags)
				libusb20_tr_close(((struct libusb20_transfer **)
			            eptr->os_priv)[1]);
			free((struct libusb20_transfer **)eptr->os_priv);
		}
		free(devh);

		pthread_mutex_lock(&ctx->pollfd_modify_lock);
		ctx->pollfd_modify--;
		pthread_mutex_unlock(&ctx->pollfd_modify_lock);
		return ;
	}
	libusb_lock_events(ctx);

	read(ctx->ctrl_pipe[0], &dummy, sizeof(dummy));
	pthread_mutex_lock(&ctx->open_devs_lock);
	TAILQ_REMOVE(&ctx->open_devs, devh, list);
	pthread_mutex_unlock(&ctx->open_devs_lock);

	usb_remove_pollfd(ctx, libusb20_dev_get_fd(pdev));
	libusb20_dev_close(pdev);
	libusb_unref_device(devh->dev);
	TAILQ_FOREACH(eptr, &devh->ep_list, list) {
		TAILQ_REMOVE(&devh->ep_list, eptr, list);
		libusb20_tr_close(((struct libusb20_transfer **)
		    eptr->os_priv)[0]);
		if (eptr->flags)
			libusb20_tr_close(((struct libusb20_transfer **)
			    eptr->os_priv)[1]);
		free((struct libusb20_transfer **)eptr->os_priv);
	}
	free(devh);

	pthread_mutex_lock(&ctx->pollfd_modify_lock);
	ctx->pollfd_modify--;
	pthread_mutex_unlock(&ctx->pollfd_modify_lock);

	libusb_unlock_events(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_close leave");
}

libusb_device *
libusb_get_device(libusb_device_handle * devh)
{
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_device enter");

	if (devh == NULL)
		return (NULL);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_device leave");
	return (devh->dev);
}

int
libusb_get_configuration(libusb_device_handle * devh, int *config)
{
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_configuration enter");

	if (devh == NULL || config == NULL)
		return (LIBUSB_ERROR_INVALID_PARAM);

	*config = libusb20_dev_get_config_index((struct libusb20_device *)
	    devh->dev->os_priv);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_get_configuration leave");
	return (0);
}

int
libusb_set_configuration(libusb_device_handle * devh, int configuration)
{
	struct libusb20_device *pdev;
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_set_configuration enter");

	if (devh == NULL)
		return (LIBUSB_ERROR_INVALID_PARAM);

	pdev = (struct libusb20_device *)devh->dev->os_priv;

	libusb20_dev_set_config_index(pdev, configuration);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_set_configuration leave");
	return (0);
}

int
libusb_claim_interface(libusb_device_handle * dev, int interface_number)
{
	libusb_context *ctx;
	int ret = 0;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_claim_interface enter");

	if (dev == NULL)
		return (LIBUSB_ERROR_INVALID_PARAM);

	if (interface_number >= sizeof(dev->claimed_interfaces) * 8)
		return (LIBUSB_ERROR_INVALID_PARAM);

	pthread_mutex_lock(&(dev->lock));
	if (dev->claimed_interfaces & (1 << interface_number))
		ret = LIBUSB_ERROR_BUSY;

	if (!ret)
		dev->claimed_interfaces |= (1 << interface_number);
	pthread_mutex_unlock(&(dev->lock));

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_claim_interface leave");
	return (ret);
}

int
libusb_release_interface(libusb_device_handle * dev, int interface_number)
{
	libusb_context *ctx;
	int ret;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_release_interface enter");

	ret = 0;
	if (dev == NULL)
		return (LIBUSB_ERROR_INVALID_PARAM);

	if (interface_number >= sizeof(dev->claimed_interfaces) * 8)
		return (LIBUSB_ERROR_INVALID_PARAM);

	pthread_mutex_lock(&(dev->lock));
	if (!(dev->claimed_interfaces & (1 << interface_number)))
		ret = LIBUSB_ERROR_NOT_FOUND;

	if (!ret)
		dev->claimed_interfaces &= ~(1 << interface_number);
	pthread_mutex_unlock(&(dev->lock));

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_release_interface leave");
	return (ret);
}

int
libusb_set_interface_alt_setting(libusb_device_handle * dev,
    int interface_number, int alternate_setting)
{
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_set_interface_alt_setting enter");

	if (dev == NULL)
		return (LIBUSB_ERROR_INVALID_PARAM);

	if (interface_number >= sizeof(dev->claimed_interfaces) *8)
		return (LIBUSB_ERROR_INVALID_PARAM);

	pthread_mutex_lock(&dev->lock);
	if (!(dev->claimed_interfaces & (1 << interface_number))) {
		pthread_mutex_unlock(&dev->lock);
		return (LIBUSB_ERROR_NOT_FOUND);
	}
	pthread_mutex_unlock(&dev->lock);

	if (libusb20_dev_set_alt_index(dev->os_priv, interface_number,
	    alternate_setting) != 0)
		return (LIBUSB_ERROR_OTHER);
	
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_set_interface_alt_setting leave");
	return (0);
}

int
libusb_clear_halt(libusb_device_handle * devh, unsigned char endpoint)
{
	struct libusb20_transfer *xfer;
	struct libusb20_device *pdev;
	libusb_context *ctx;
	int ret; 

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_clear_halt enter");
	
	pdev = devh->os_priv;
	xfer = libusb20_tr_get_pointer(pdev, 
	    ((endpoint / 0x40) | (endpoint * 4)) % (16 * 4));
	if (xfer == NULL)
		return (LIBUSB_ERROR_NO_MEM);

	pthread_mutex_lock(&libusb20_lock);
	ret = libusb20_tr_open(xfer, 0, 0, endpoint);
	if (ret != 0 && ret != LIBUSB20_ERROR_BUSY) {
		pthread_mutex_unlock(&libusb20_lock);
		return (LIBUSB_ERROR_OTHER);
	}

	libusb20_tr_clear_stall_sync(xfer);
	if (ret == 0) /* check if we have open the device */
		libusb20_tr_close(xfer);
	pthread_mutex_unlock(&libusb20_lock);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_clear_halt leave");
	return (0);
}

int
libusb_reset_device(libusb_device_handle * dev)
{
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_reset_device enter");

	if (dev == NULL)
		return (LIBUSB20_ERROR_INVALID_PARAM);

	libusb20_dev_reset(dev->os_priv);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_reset_device leave");
	return (0);
}

int
libusb_kernel_driver_active(libusb_device_handle * devh, int interface)
{
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_kernel_driver_active enter");

	if (devh == NULL)
		return (LIBUSB_ERROR_INVALID_PARAM);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_kernel_driver_active leave");
	return (libusb20_dev_kernel_driver_active(devh->os_priv, interface));
}

int
libusb_detach_kernel_driver(libusb_device_handle * devh, int interface)
{
	struct libusb20_device *pdev;
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_detach_kernel_driver enter");

	if (devh == NULL)
		return (LIBUSB_ERROR_INVALID_PARAM);

	pdev = (struct libusb20_device *)devh->dev->os_priv;
	if (libusb20_dev_detach_kernel_driver(pdev, interface) == LIBUSB20_ERROR_OTHER)
		return (LIBUSB_ERROR_OTHER);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_detach_kernel_driver leave");
	return (0);
}

/* 
 * stub function.
 * libusb20 doesn't support this feature.
 */
int
libusb_attach_kernel_driver(libusb_device_handle * devh, int interface)
{
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_attach_kernel_driver enter");

	if (devh == NULL)
		return (LIBUSB_ERROR_INVALID_PARAM);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_attach_kernel_driver leave");
	return (0);
}

/* Asynchronous device I/O */

struct libusb_transfer *
libusb_alloc_transfer(int iso_packets)
{
	struct libusb_transfer *xfer;
	struct usb_transfer *bxfer;
	libusb_context *ctx;
	int len;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_alloc_transfer enter");

	len = sizeof(struct libusb_transfer) +
	    sizeof(struct usb_transfer) +
	    (iso_packets * sizeof(libusb_iso_packet_descriptor));

	bxfer = malloc(len);
	if (bxfer == NULL)
		return (NULL);

	memset(bxfer, 0, len);
	bxfer->num_iso_packets = iso_packets;

	xfer = (struct libusb_transfer *) ((uint8_t *)bxfer + 
	    sizeof(struct usb_transfer));

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_alloc_transfer leave");
	return (xfer);
}

void
libusb_free_transfer(struct libusb_transfer *xfer)
{
	struct usb_transfer *bxfer;
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_free_transfer enter");

	if (xfer == NULL)
		return ;

	bxfer = (struct usb_transfer *) ((uint8_t *)xfer - 
	    sizeof(struct usb_transfer));

	free(bxfer);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_free_transfer leave");
	return;
}

static int
libusb_get_maxframe(struct libusb20_device *pdev, libusb_transfer *xfer)
{
	int ret;
	int usb_speed;

	usb_speed = libusb20_dev_get_speed(pdev);

	switch (xfer->type) {
	case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
		switch (usb_speed) {
		case LIBUSB20_SPEED_LOW:
		case LIBUSB20_SPEED_FULL:
			ret = 60 * 1;
			break ;
		default :
			ret = 60 * 8;
			break ;
		}
		break ;
	case LIBUSB_TRANSFER_TYPE_CONTROL:
		ret = 2;
		break ;
	default:
		ret = 1;
		break ;
	}

	return ret;
}

static int
libusb_get_buffsize(struct libusb20_device *pdev, libusb_transfer *xfer)
{
	int ret;
	int usb_speed;

	usb_speed = libusb20_dev_get_speed(pdev);

	switch (xfer->type) {
	case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
		ret = 0;
		break ;
	case LIBUSB_TRANSFER_TYPE_CONTROL:
		switch (usb_speed) {
			case LIBUSB20_SPEED_LOW:
				ret = 8;
				break ;
			case LIBUSB20_SPEED_FULL:
				ret = 64;
				break ;
			default:
				ret = 64;
				break ;
		}
		ret += 8;
		break ;
	default :
		switch (usb_speed) {
			case LIBUSB20_SPEED_LOW:
				ret = 256;
				break ;
			case LIBUSB20_SPEED_FULL:
				ret = 4096;
				break ;
			default:
				ret = 16384;
				break ;
		}
		break ;
	}
	
	return ret;
}

static void
libusb10_proxy(struct libusb20_transfer *xfer)
{
	struct usb_transfer *usb_backend;
	struct libusb20_device *pdev;
	libusb_transfer *usb_xfer;
	libusb_context *ctx;
	uint32_t pos;
	uint32_t max;
	uint32_t size;
	uint8_t status;
	uint32_t iso_packets;
	int i;

	status = libusb20_tr_get_status(xfer);
	usb_xfer = libusb20_tr_get_priv_sc0(xfer);
	usb_backend = (struct usb_transfer *) ((uint8_t *)usb_xfer - 
	    sizeof(struct usb_transfer));
	pdev = usb_xfer->dev_handle->dev->os_priv;
	ctx = usb_xfer->dev_handle->dev->ctx;
	GET_CONTEXT(ctx);

	switch (status) {
	case LIBUSB20_TRANSFER_COMPLETED:
		usb_backend->transferred += libusb20_tr_get_actual_length(xfer);
		DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "LIBUSB20 TRANSFER %i bytes",
		    usb_backend->transferred);
		if (usb_backend->transferred != usb_xfer->length)
			goto tr_start;

		DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "LIBUSB20 TRANSFER COMPLETE");
		usb_handle_transfer_completion(usb_backend, LIBUSB_TRANSFER_COMPLETED);

		break ;
	case LIBUSB20_TRANSFER_START:
tr_start:
		DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "LIBUSB20 START");
		max = libusb_get_buffsize(pdev, usb_xfer);
		pos = usb_backend->transferred;
		size = (usb_xfer->length - pos);
		size = (size > max) ? max : size;
		usb_xfer->actual_length = 0;
		switch (usb_xfer->type) {
			case LIBUSB_TRANSFER_TYPE_CONTROL:
				DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "TYPE CTR");
				libusb20_tr_setup_control(xfer, usb_xfer->buffer,
				    (void *)(((uint8_t *) &usb_xfer->buffer[pos]) + 
			            sizeof(libusb_control_setup)), 
				    usb_xfer->timeout);
				break ;
			case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
				DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "TYPE ISO");
				iso_packets = libusb20_tr_get_max_frames(xfer);
				if (usb_xfer->num_iso_packets > iso_packets)
					usb_xfer->num_iso_packets = iso_packets;
				for (i = 0 ; i < usb_xfer->num_iso_packets ; i++) {
					libusb20_tr_setup_isoc(xfer, 
					    &usb_xfer->buffer[pos], size, i);
				}
				libusb20_tr_set_total_frames(xfer, i);
				break ;
			case LIBUSB_TRANSFER_TYPE_BULK:
				DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "TYPE BULK");
				libusb20_tr_setup_bulk(xfer, &usb_xfer->buffer[pos],
				    size, usb_xfer->timeout);
				break ;
			case LIBUSB_TRANSFER_TYPE_INTERRUPT:
				DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "TYPE INTR");
				libusb20_tr_setup_intr(xfer, &usb_xfer->buffer[pos],
				    size, usb_xfer->timeout);
				break ;
		}
		libusb20_tr_submit(xfer);
		DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "LIBUSB20 SUBMITED");
		break ;
	default:
		DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "TRANSFER DEFAULT 0x%x\n", 
		    status);
		usb_backend->transferred = 0;
		usb_handle_transfer_completion(usb_backend, LIBUSB_TRANSFER_CANCELLED);
		break ;
	}

	switch (status) {
	case LIBUSB20_TRANSFER_COMPLETED:
		DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "STATUS COMPLETED");
		usb_xfer->status = LIBUSB_TRANSFER_COMPLETED;
		break ;
	case LIBUSB20_TRANSFER_OVERFLOW:
		DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "STATUS TR OVERFLOW");
		usb_xfer->status = LIBUSB_TRANSFER_OVERFLOW;
		break ;
	case LIBUSB20_TRANSFER_NO_DEVICE:
		DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "STATUS TR NO DEVICE");
		usb_xfer->status = LIBUSB_TRANSFER_NO_DEVICE;
		break ;
	case LIBUSB20_TRANSFER_STALL:
		DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "STATUS TR STALL");
		usb_xfer->status = LIBUSB_TRANSFER_STALL;
		break ;
	case LIBUSB20_TRANSFER_CANCELLED:
		DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "STATUS TR CANCELLED");
		usb_xfer->status = LIBUSB_TRANSFER_CANCELLED;
		break ;
	case LIBUSB20_TRANSFER_TIMED_OUT:
		DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "STATUS TR TIMEOUT");
		usb_xfer->status = LIBUSB_TRANSFER_TIMED_OUT;
		break ;
	case LIBUSB20_TRANSFER_ERROR:
		DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "ERROR");
		usb_xfer->status = LIBUSB_TRANSFER_ERROR;
		break ;
	}
}

int
libusb_submit_transfer(struct libusb_transfer *xfer)
{
	struct libusb20_transfer **usb20_xfer;
	struct usb_transfer *usb_backend;
	struct usb_transfer *usb_node;
	struct libusb20_device *pdev;
	struct usb_ep_tr *eptr;
	struct timespec cur_ts;
	struct timeval *cur_tv;
	libusb_device_handle *devh;
	libusb_context *ctx;
	int maxframe;
	int buffsize;
	int ep_idx;
	int ret;

	if (xfer == NULL)
		return (LIBUSB_ERROR_NO_MEM);

	usb20_xfer = malloc(2 * sizeof(struct libusb20_transfer *));
	if (usb20_xfer == NULL)
		return (LIBUSB_ERROR_NO_MEM);

	ctx = xfer->dev_handle->dev->ctx;
	pdev = xfer->dev_handle->os_priv;
	devh = xfer->dev_handle;

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_submit_transfer enter");

	usb_backend = (struct usb_transfer *) ((uint8_t *)xfer - 
	    sizeof(struct usb_transfer));
	usb_backend->transferred = 0;
	usb_backend->flags = 0;

	if (xfer->timeout != 0) {
		clock_gettime(CLOCK_MONOTONIC, &cur_ts);
		cur_ts.tv_sec += xfer->timeout / 1000;
		cur_ts.tv_nsec += (xfer->timeout % 1000) * 1000000;
		
		if (cur_ts.tv_nsec > 1000000000) {
			cur_ts.tv_nsec -= 1000000000;
			cur_ts.tv_sec++;
		}
		
		TIMESPEC_TO_TIMEVAL(&usb_backend->timeout, &cur_ts);
	} 

	/*Add to flying list*/
	pthread_mutex_lock(&ctx->flying_transfers_lock);
	if (TAILQ_EMPTY(&ctx->flying_transfers)) {
		TAILQ_INSERT_HEAD(&ctx->flying_transfers, usb_backend, list);
		goto out;
	}
	if (timerisset(&usb_backend->timeout) == 0) {
		TAILQ_INSERT_HEAD(&ctx->flying_transfers, usb_backend, list);
		goto out;
	}
	TAILQ_FOREACH(usb_node, &ctx->flying_transfers, list) {
		cur_tv = &usb_node->timeout;
		if (timerisset(cur_tv) == 0 || 
		    (cur_tv->tv_sec > usb_backend->timeout.tv_sec) ||
		    (cur_tv->tv_sec == usb_backend->timeout.tv_sec &&
		    cur_tv->tv_usec > usb_backend->timeout.tv_usec)) {
			TAILQ_INSERT_TAIL(&ctx->flying_transfers, usb_backend, list);
			goto out;
		}
	}	
	TAILQ_INSERT_TAIL(&ctx->flying_transfers, usb_backend, list);

out:
	pthread_mutex_unlock(&ctx->flying_transfers_lock);

	ep_idx = (xfer->endpoint / 0x40) | (xfer->endpoint * 4) % (16 * 4);
	usb20_xfer[0] = libusb20_tr_get_pointer(pdev, ep_idx);
	usb20_xfer[1] = libusb20_tr_get_pointer(pdev, ep_idx + 1);
	
	if (usb20_xfer[0] == NULL)
		return (LIBUSB_ERROR_OTHER);

	xfer->os_priv = usb20_xfer;

	pthread_mutex_lock(&libusb20_lock);

	buffsize = libusb_get_buffsize(pdev, xfer);
	maxframe = libusb_get_maxframe(pdev, xfer);
	
	ret = 0;
	TAILQ_FOREACH(eptr, &devh->ep_list, list) {
		if (xfer->endpoint == eptr->addr)
			ret++;
	}
	if (ret == 0) {
		eptr = malloc(sizeof(struct usb_ep_tr));
		eptr->addr = xfer->endpoint;
		eptr->idx = ep_idx;
		eptr->os_priv = usb20_xfer;
		eptr->flags = (xfer->type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS)?1:0; 
		TAILQ_INSERT_HEAD(&devh->ep_list, eptr, list);
		ret = libusb20_tr_open(usb20_xfer[0], buffsize, 
	    		maxframe, xfer->endpoint);
		if (xfer->type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS)
			ret |= libusb20_tr_open(usb20_xfer[1], buffsize,
		    	maxframe, xfer->endpoint);
       
		if (ret != 0) {
			pthread_mutex_unlock(&libusb20_lock);
			pthread_mutex_lock(&ctx->flying_transfers_lock);
			TAILQ_REMOVE(&ctx->flying_transfers, usb_backend, list);
			pthread_mutex_unlock(&ctx->flying_transfers_lock);
			return (LIBUSB_ERROR_OTHER);
		}
	}

	libusb20_tr_set_priv_sc0(usb20_xfer[0], xfer);
	libusb20_tr_set_callback(usb20_xfer[0], libusb10_proxy);
	if (xfer->type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS) {
		libusb20_tr_set_priv_sc0(usb20_xfer[1], xfer);
		libusb20_tr_set_callback(usb20_xfer[1], libusb10_proxy);
	}	

	DPRINTF(ctx, LIBUSB_DEBUG_TRANSFER, "LIBUSB20_TR_START");
	libusb20_tr_start(usb20_xfer[0]);
	if (xfer->type == LIBUSB_TRANSFER_TYPE_ISOCHRONOUS) 
		libusb20_tr_start(usb20_xfer[1]);

	pthread_mutex_unlock(&libusb20_lock);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_submit_transfer leave");
	return (0);
}

int
libusb_cancel_transfer(struct libusb_transfer *xfer)
{
	libusb_context *ctx;

	ctx = NULL;
	GET_CONTEXT(ctx);
	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_cancel_transfer enter");

	if (xfer == NULL)
		return (LIBUSB_ERROR_NO_MEM);

	pthread_mutex_lock(&libusb20_lock);
	libusb20_tr_stop(xfer->os_priv);
	pthread_mutex_unlock(&libusb20_lock);

	DPRINTF(ctx, LIBUSB_DEBUG_FUNCTION, "libusb_cancel_transfer leave");
	return (0);
}

