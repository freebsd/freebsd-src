/*-
 * Copyright (c) 2016-2019 Hans Petter Selasky. All rights reserved.
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

#include <netlink/netlink_snl_generic.h>
#ifdef LIBUSB_GLOBAL_INCLUDE_FILE
#include LIBUSB_GLOBAL_INCLUDE_FILE
#else
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/endian.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/module.h>
#include <sys/linker.h>
#endif

#define	libusb_device_handle libusb20_device

#include "libusb20.h"
#include "libusb20_desc.h"
#include "libusb20_int.h"
#include "libusb.h"
#include "libusb10.h"

#define DEVDPIPE	"/var/run/devd.seqpacket.pipe"
#define DEVCTL_MAXBUF	1024

typedef enum {
	broken_event,
	invalid_event,
	valid_event,
} event_t;

static bool
netlink_init(libusb_context *ctx)
{
	uint32_t group;

	if (modfind("nlsysevent") < 0)
		kldload("nlsysevent");
	if (modfind("nlsysevent") < 0)
		return (false);
	if (!snl_init(&ctx->ss, NETLINK_GENERIC) || (group =
	    snl_get_genl_mcast_group(&ctx->ss, "nlsysevent", "USB", NULL)) == 0)
		return (false);

	if (setsockopt(ctx->ss.fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP, &group,
	    sizeof(group)) == -1)
		return (false);

	ctx->usb_event_mode = usb_event_netlink;
	return (true);
}

static bool
devd_init(libusb_context *ctx)
{
	struct sockaddr_un devd_addr;

	bzero(&devd_addr, sizeof(devd_addr));
	if ((ctx->devd_pipe = socket(PF_LOCAL, SOCK_SEQPACKET|SOCK_NONBLOCK, 0)) < 0)
		return (false);

	devd_addr.sun_family = PF_LOCAL;
	strlcpy(devd_addr.sun_path, DEVDPIPE, sizeof(devd_addr.sun_path));
	if (connect(ctx->devd_pipe, (struct sockaddr *)&devd_addr,
	    sizeof(devd_addr)) == -1) {
		close(ctx->devd_pipe);
		ctx->devd_pipe = -1;
		return (false);
	}

	ctx->usb_event_mode = usb_event_devd;
	return (true);
}

struct nlevent {
	const char *name;
	const char *subsystem;
	const char *type;
	const char *data;
};

#define	_OUT(_field)	offsetof(struct nlevent, _field)
static struct snl_attr_parser ap_nlevent_get[] = {
	{ .type = NLSE_ATTR_SYSTEM, .off = _OUT(name), .cb = snl_attr_get_string },
	{ .type = NLSE_ATTR_SUBSYSTEM, .off = _OUT(subsystem), .cb = snl_attr_get_string },
	{ .type = NLSE_ATTR_TYPE, .off = _OUT(type), .cb = snl_attr_get_string },
	{ .type = NLSE_ATTR_DATA, .off = _OUT(data), .cb = snl_attr_get_string },
};
#undef _OUT

SNL_DECLARE_GENL_PARSER(nlevent_get_parser, ap_nlevent_get);

static event_t
verify_event_validity(libusb_context *ctx)
{
	if (ctx->usb_event_mode == usb_event_netlink) {
		struct nlmsghdr *hdr;
		struct nlevent ne;

		hdr = snl_read_message(&ctx->ss);
		if (hdr != NULL && hdr->nlmsg_type != NLMSG_ERROR) {
			memset(&ne, 0, sizeof(ne));
			if (!snl_parse_nlmsg(&ctx->ss, hdr, &nlevent_get_parser, &ne))
				return (broken_event);
			if (strcmp(ne.subsystem, "DEVICE") == 0)
				return (valid_event);
			return (invalid_event);
		}
		if (errno == EBADF)
			return (broken_event);
		return (invalid_event);
	} else if (ctx->usb_event_mode == usb_event_devd) {
		char buf[DEVCTL_MAXBUF];
		ssize_t len;

		len = read(ctx->devd_pipe, buf, sizeof(buf));
		if (len == 0 || (len < 0 && errno != EWOULDBLOCK))
			return (broken_event);
		if (len > 0 && strstr(buf, "system=USB") != NULL &&
		    strstr(buf, "subsystem=DEVICE") != NULL)
			return (valid_event);
		return (invalid_event);
	}
	return (broken_event);
}

static int
libusb_hotplug_equal(libusb_device *_adev, libusb_device *_bdev)
{
	struct libusb20_device *adev = _adev->os_priv;
	struct libusb20_device *bdev = _bdev->os_priv;

	if (adev->bus_number != bdev->bus_number)
		return (0);
	if (adev->device_address != bdev->device_address)
		return (0);
	if (memcmp(&adev->ddesc, &bdev->ddesc, sizeof(adev->ddesc)))
		return (0);
	if (memcmp(&adev->session_data, &bdev->session_data, sizeof(adev->session_data)))
		return (0);
	return (1);
}

static int
libusb_hotplug_filter(libusb_context *ctx, libusb_hotplug_callback_handle pcbh,
    libusb_device *dev, libusb_hotplug_event event)
{
	if (!(pcbh->events & event))
		return (0);
	if (pcbh->vendor != LIBUSB_HOTPLUG_MATCH_ANY &&
	    pcbh->vendor != libusb20_dev_get_device_desc(dev->os_priv)->idVendor)
		return (0);
	if (pcbh->product != LIBUSB_HOTPLUG_MATCH_ANY &&
	    pcbh->product != libusb20_dev_get_device_desc(dev->os_priv)->idProduct)
		return (0);
	if (pcbh->devclass != LIBUSB_HOTPLUG_MATCH_ANY &&
	    pcbh->devclass != libusb20_dev_get_device_desc(dev->os_priv)->bDeviceClass)
		return (0);
	return (pcbh->fn(ctx, dev, event, pcbh->user_data));
}

static int
libusb_hotplug_enumerate(libusb_context *ctx, struct libusb_device_head *phead)
{
	libusb_device **ppdev;
	ssize_t count;
	ssize_t x;

	count = libusb_get_device_list(ctx, &ppdev);
	if (count < 0)
		return (-1);

	for (x = 0; x != count; x++)
		TAILQ_INSERT_TAIL(phead, ppdev[x], hotplug_entry);

	libusb_free_device_list(ppdev, 0);
	return (0);
}

static void *
libusb_hotplug_scan(void *arg)
{
	struct pollfd pfd;
	struct libusb_device_head hotplug_devs;
	libusb_hotplug_callback_handle acbh;
	libusb_hotplug_callback_handle bcbh;
	libusb_context *ctx = arg;
	libusb_device *temp;
	libusb_device *adev;
	libusb_device *bdev;
	int timeout = INFTIM;
	int nfds;

	memset(&pfd, 0, sizeof(pfd));
	if (ctx->usb_event_mode == usb_event_devd) {
		pfd.fd = ctx->devd_pipe;
		pfd.events = POLLIN | POLLERR;
		nfds = 1;
	} else if (ctx->usb_event_mode == usb_event_netlink) {
		pfd.fd = ctx->ss.fd;
		pfd.events = POLLIN | POLLERR;
		nfds = 1;
	} else {
		nfds = 0;
		timeout = 4000;
	}
	for (;;) {
		pfd.revents = 0;
		if (poll(&pfd, nfds, timeout) > 0)  {
			switch (verify_event_validity(ctx)) {
			case invalid_event:
				continue;
			case valid_event:
				break;
			case broken_event:
				/* There are 2 cases for broken events:
				 * - devd and netlink sockets are not available
				 *   anymore (devd restarted, nlsysevent unloaded)
				 * - libusb_exit has been called as it sets NO_THREAD
				 *   this will result in exiting this loop and this thread
				 *   immediately
				 */
				nfds = 0;
				if (ctx->usb_event_mode == usb_event_devd) {
					if (ctx->devd_pipe != -1)
						close(ctx->devd_pipe);
				} else if (ctx->usb_event_mode == usb_event_netlink) {
					if (ctx->ss.fd != -1)
						close(ctx->ss.fd);
				}
				ctx->usb_event_mode = usb_event_scan;
				timeout = 4000;
				break;
			}
		}

		HOTPLUG_LOCK(ctx);
		if (ctx->hotplug_handler == NO_THREAD) {
			while ((adev = TAILQ_FIRST(&ctx->hotplug_devs)) != NULL) {
				TAILQ_REMOVE(&ctx->hotplug_devs, adev, hotplug_entry);
				libusb_unref_device(adev);
			}
			if (ctx->usb_event_mode == usb_event_devd)
				close(ctx->devd_pipe);
			else if (ctx->usb_event_mode == usb_event_netlink)
				close(ctx->ss.fd);
			HOTPLUG_UNLOCK(ctx);
			break;
		}

		TAILQ_INIT(&hotplug_devs);

		if (libusb_hotplug_enumerate(ctx, &hotplug_devs) < 0) {
			HOTPLUG_UNLOCK(ctx);
			continue;
		}

		/* figure out which devices are gone */
		TAILQ_FOREACH_SAFE(adev, &ctx->hotplug_devs, hotplug_entry, temp) {
			TAILQ_FOREACH(bdev, &hotplug_devs, hotplug_entry) {
				if (libusb_hotplug_equal(adev, bdev))
					break;
			}
			if (bdev == NULL) {
				TAILQ_REMOVE(&ctx->hotplug_devs, adev, hotplug_entry);
				TAILQ_FOREACH_SAFE(acbh, &ctx->hotplug_cbh, entry, bcbh) {
					if (libusb_hotplug_filter(ctx, acbh, adev,
					    LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) == 0)
						continue;
					TAILQ_REMOVE(&ctx->hotplug_cbh, acbh, entry);
					free(acbh);
				}
				libusb_unref_device(adev);
			}
		}

		/* figure out which devices are new */
		TAILQ_FOREACH_SAFE(adev, &hotplug_devs, hotplug_entry, temp) {
			TAILQ_FOREACH(bdev, &ctx->hotplug_devs, hotplug_entry) {
				if (libusb_hotplug_equal(adev, bdev))
					break;
			}
			if (bdev == NULL) {
				TAILQ_REMOVE(&hotplug_devs, adev, hotplug_entry);
				TAILQ_INSERT_TAIL(&ctx->hotplug_devs, adev, hotplug_entry);
				TAILQ_FOREACH_SAFE(acbh, &ctx->hotplug_cbh, entry, bcbh) {
					if (libusb_hotplug_filter(ctx, acbh, adev,
					    LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) == 0)
						continue;
					TAILQ_REMOVE(&ctx->hotplug_cbh, acbh, entry);
					free(acbh);
				}
			}
		}
		HOTPLUG_UNLOCK(ctx);

		/* unref remaining devices */
		while ((adev = TAILQ_FIRST(&hotplug_devs)) != NULL) {
			TAILQ_REMOVE(&hotplug_devs, adev, hotplug_entry);
			libusb_unref_device(adev);
		}
	}
	return (NULL);
}

int libusb_hotplug_register_callback(libusb_context *ctx,
    libusb_hotplug_event events, libusb_hotplug_flag flags,
    int vendor_id, int product_id, int dev_class,
    libusb_hotplug_callback_fn cb_fn, void *user_data,
    libusb_hotplug_callback_handle *phandle)
{
	libusb_hotplug_callback_handle handle;
	struct libusb_device *adev;

	ctx = GET_CONTEXT(ctx);

	if (ctx->usb_event_mode == usb_event_none) {
		HOTPLUG_LOCK(ctx);
		if (!netlink_init(ctx) && !devd_init(ctx))
			ctx->usb_event_mode = usb_event_scan;
		HOTPLUG_UNLOCK(ctx);
	}

	if (ctx == NULL || cb_fn == NULL || events == 0 ||
	    vendor_id < -1 || vendor_id > 0xffff ||
	    product_id < -1 || product_id > 0xffff ||
	    dev_class < -1 || dev_class > 0xff)
		return (LIBUSB_ERROR_INVALID_PARAM);

	handle = malloc(sizeof(*handle));
	if (handle == NULL)
		return (LIBUSB_ERROR_NO_MEM);

	HOTPLUG_LOCK(ctx);
	if (ctx->hotplug_handler == NO_THREAD) {
	  	libusb_hotplug_enumerate(ctx, &ctx->hotplug_devs);

		if (pthread_create(&ctx->hotplug_handler, NULL,
		    &libusb_hotplug_scan, ctx) != 0)
			ctx->hotplug_handler = NO_THREAD;
	}
	handle->events = events;
	handle->vendor = vendor_id;
	handle->product = product_id;
	handle->devclass = dev_class;
	handle->fn = cb_fn;
	handle->user_data = user_data;

	if (flags & LIBUSB_HOTPLUG_ENUMERATE) {
		TAILQ_FOREACH(adev, &ctx->hotplug_devs, hotplug_entry) {
			if (libusb_hotplug_filter(ctx, handle, adev,
			    LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) == 0)
				continue;
			free(handle);
			handle = NULL;
			break;
		}
	}
	if (handle != NULL)
		TAILQ_INSERT_TAIL(&ctx->hotplug_cbh, handle, entry);
	HOTPLUG_UNLOCK(ctx);

	if (phandle != NULL)
		*phandle = handle;
	return (LIBUSB_SUCCESS);
}

void libusb_hotplug_deregister_callback(libusb_context *ctx,
    libusb_hotplug_callback_handle handle)
{
  	ctx = GET_CONTEXT(ctx);

	if (ctx == NULL || handle == NULL)
		return;

	HOTPLUG_LOCK(ctx);
	TAILQ_REMOVE(&ctx->hotplug_cbh, handle, entry);
	libusb_interrupt_event_handler(ctx);
	HOTPLUG_UNLOCK(ctx);

	free(handle);
}

void *
libusb_hotplug_get_user_data(struct libusb_context *ctx,
    libusb_hotplug_callback_handle callback_handle)
{
	libusb_hotplug_callback_handle handle;

	ctx = GET_CONTEXT(ctx);

	HOTPLUG_LOCK(ctx);
	TAILQ_FOREACH(handle, &ctx->hotplug_cbh, entry) {
		if (handle == callback_handle)
			break;
	}
	HOTPLUG_UNLOCK(ctx);

	return (handle);
}
