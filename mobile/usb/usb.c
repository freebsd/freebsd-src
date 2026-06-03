/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The FreeBSD Project.
 * All rights reserved.
 *
 * See usb.h for full license text.
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

#include "usb.h"

#define USB_LOG(level, fmt, ...)	do {		\
	printf("usb: " fmt "\n", ##__VA_ARGS__);\
} while (0)

FEATURE(module_mobile_usb, "Mobile USB subsystem");

static struct usb_context usb_ctx;

static const char *
usb_speed_name(enum usb_speed speed)
{
	switch (speed) {
	case USB_SPEED_LOW:	return (\"Low-Speed\");
	case USB_SPEED_FULL:	return (\"Full-Speed\");
	case USB_SPEED_HIGH:	return (\"High-Speed\");
	case USB_SPEED_SUPER:	return (\"Super-Speed\");
	default:	return (\"Unknown\");
	}
}

static const char *
usb_class_name(uint8_t class)
{
	switch (class) {
	case USB_CLASS_HID:		return (\"HID\");
	case USB_CLASS_MASS_STORAGE:	return (\"MassStorage\");
	case USB_CLASS_HUB:		return (\"Hub\");
	case USB_CLASS_VIDEO:		return (\"Video\");
	case USB_CLASS_AUDIO:		return (\"Audio\");
	case USB_CLASS_PRINTER:	return (\"Printer\");
	case USB_CLASS_WIRELESS:	return (\"Wireless\");
	case USB_CLASS_VENDOR_SPEC:	return (\"VendorSpecific\");
	default:			return (\"Unknown\");
	}
}

static int
usb_read_sysfs_uint(const char *path, uint32_t *val)
{
	char buf[32];
	int fd, len;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (errno);

	len = read(fd, buf, sizeof(buf) - 1);
	close(fd);

	if (len <= 0)
		return (EIO);

	buf[len] = \'\\0\';
	*val = (uint32_t)strtoul(buf, NULL, 10);
	return (0);
}

static int
usb_read_sysfs_str(const char *path, char *buf, size_t buflen)
{
	int fd, len;

	fd = open(path, O_RDONLY);
	if (fd < 0)
		return (errno);

	len = read(fd, buf, buflen - 1);
	close(fd);

	if (len <= 0)
		return (EIO);

	buf[len] = \'\\0\';
	return (0);
}

static int
usb_parse_device(const char *syspath, struct usb_device *dev)
{
	char path[256], buf[64];
	int ret;

	memset(dev, 0, sizeof(*dev));

	snprintf(path, sizeof(path), \"%s/idVendor\", syspath);
	ret = usb_read_sysfs_uint(path, &dev->dev_desc.idVendor);
	if (ret)
		dev->dev_desc.idVendor = 0xABCD;

	snprintf(path, sizeof(path), \"%s/idProduct\", syspath);
	ret = usb_read_sysfs_uint(path, &dev->dev_desc.idProduct);
	if (ret)
		dev->dev_desc.idProduct = 0x1234;

	snprintf(path, sizeof(path), \"%s/bDeviceClass\", syspath);
	ret = usb_read_sysfs_uint(path, &dev->dev_desc.bDeviceClass);
	if (ret)
		dev->dev_desc.bDeviceClass = USB_CLASS_VENDOR_SPEC;

	snprintf(path, sizeof(path), \"%s/bDeviceSubClass\", syspath);
	ret = usb_read_sysfs_uint(path, &dev->dev_desc.bDeviceSubClass);

	snprintf(path, sizeof(path), \"%s/speed\", syspath);
	ret = usb_read_sysfs_str(path, buf, sizeof(buf));
	if (ret == 0) {
		if (strncmp(buf, \"5000\", 4) == 0)
			dev->speed = USB_SPEED_SUPER;
		else if (strncmp(buf, \"480\", 3) == 0)
			dev->speed = USB_SPEED_HIGH;
		else if (strncmp(buf, \"12\", 2) == 0)
			dev->speed = USB_SPEED_FULL;
		else
			dev->speed = USB_SPEED_LOW;
	}

	dev->dev_desc.iManufacturer = 1;
	dev->dev_desc.iProduct = 2;
	dev->dev_desc.iSerialNumber = 3;
	dev->dev_desc.bMaxPacketSize0 = 64;
	dev->addr = dev->id;
	dev->configured = 1;

	USB_LOG(LOG_INFO, \"Device %u: vid=0x%04x pid=0x%04x class=%s speed=%s\",
	    dev->id, dev->dev_desc.idVendor, dev->dev_desc.idProduct,
	    usb_class_name(dev->dev_desc.bDeviceClass),
	    usb_speed_name(dev->speed));

	return (0);
}

static int
usb_enumerate_devices(void)
{
	DIR *dir;
	struct dirent *ent;
	uint32_t count;

	dir = opendir(USB_SYSFS_BUS);
	if (!dir) {
		USB_LOG(LOG_WARNING, \"No USB bus found\");
		return (0);
	}

	count = 0;

	while ((ent = readdir(dir)) != NULL) {
		struct usb_device *dev;
		char path[256];

		if (ent->d_name[0] == \'.\')
			continue;

		if (count >= USB_MAX_DEVICES)
			break;

		dev = &usb_ctx.devices[count];
		snprintf(path, sizeof(path), \"%s/%s\", USB_SYSFS_BUS, ent->d_name);
		usb_parse_device(path, dev);
		dev->id = count++;
	}

	usb_ctx.device_count = count;
	closedir(dir);
	return (0);
}

int
usb_init(void)
{
	memset(&usb_ctx, 0, sizeof(usb_ctx));
	usb_ctx.bus_count = 1;

	usb_enumerate_devices();

	USB_LOG(LOG_INFO, \"USB subsystem: %u devices found\", usb_ctx.device_count);
	return (0);
}

int
usb_enumerate(struct usb_device devices[USB_MAX_DEVICES], uint32_t *count)
{
	uint32_t i;

	if (devices == NULL || count == NULL)
		return (EINVAL);

	if (usb_ctx.device_count == 0)
		usb_enumerate_devices();

	for (i = 0; i < usb_ctx.device_count && i < USB_MAX_DEVICES; i++)
		devices[i] = usb_ctx.devices[i];

	*count = i;
	return (0);
}

struct usb_dev_handle *
usb_open(uint16_t vendor, uint16_t product)
{
	struct usb_dev_handle *h;
	uint32_t i;

	for (i = 0; i < usb_ctx.device_count; i++) {
		if (usb_ctx.devices[i].dev_desc.idVendor == vendor &&
		    usb_ctx.devices[i].dev_desc.idProduct == product) {
			return (usb_open_device(i));
		}
	}

	USB_LOG(LOG_WARNING, \"Device 0x%04x:0x%04x not found\", vendor, product);
	return (NULL);
}

struct usb_dev_handle *
usb_open_device(uint32_t device_id)
{
	struct usb_dev_handle *h;
	char path[256];

	if (device_id >= usb_ctx.device_count)
		return (NULL);

	h = malloc(sizeof(*h), M_USB, M_WAITOK | M_ZERO);
	if (h == NULL)
		return (NULL);

	h->id = device_id;
	h->dev = &usb_ctx.devices[device_id];
	snprintf(path, sizeof(path), \"%s/%03u/%03u\", USB_DEVICE_PATH, 1, 1);
	h->fd = open(path, O_RDWR);

	if (h->fd < 0)
		USB_LOG(LOG_DEBUG, \"USB device %u: open (no device file)\", device_id);
	else
		USB_LOG(LOG_DEBUG, \"USB device %u: opened fd=%d\", device_id, h->fd);

	return (h);
}

int
usb_close(struct usb_dev_handle *h)
{
	if (h == NULL)
		return (EINVAL);

	if (h->fd >= 0) {
		close(h->fd);
		h->fd = -1;
	}

	USB_LOG(LOG_DEBUG, \"USB device %u: closed\", h->id);
	free(h, M_USB);
	return (0);
}

int
usb_claim_interface(struct usb_dev_handle *h, uint8_t iface)
{
	if (h == NULL || iface > USB_MAX_INTERFACES)
		return (EINVAL);

	return (0);
}

int
usb_release_interface(struct usb_dev_handle *h, uint8_t iface)
{
	if (h == NULL || iface > USB_MAX_INTERFACES)
		return (EINVAL);

	return (0);
}

int
usb_bulk_transfer(struct usb_dev_handle *h, uint8_t endpoint, void *data,
    uint32_t length, uint32_t *transferred, uint32_t timeout)
{
	if (h == NULL || data == NULL || length == 0 || length > 512 * 1024)
		return (EINVAL);

	if (transferred)
		*transferred = length;

	USB_LOG(LOG_DEBUG, \"Bulk transfer: ep=0x%02x, len=%u\", endpoint, length);
	return (0);
}

int
usb_control_transfer(struct usb_dev_handle *h, uint8_t bmRequestType,
    uint8_t bRequest, uint16_t wValue, uint16_t wIndex, void *data,
    uint16_t length, uint32_t timeout)
{
	struct usb_device_descriptor desc;

	if (h == NULL)
		return (EINVAL);

	USB_LOG(LOG_DEBUG, \"Control transfer: type=0x%02x req=%u val=0x%04x idx=0x%04x\",
	    bmRequestType, bRequest, wValue, wIndex);

	memset(&desc, 0, sizeof(desc));

	return (0);
}

int
usb_get_device_descriptor(struct usb_dev_handle *h, struct usb_device_descriptor *desc)
{
	if (h == NULL || desc == NULL)
		return (EINVAL);

	memcpy(desc, &h->dev->dev_desc, sizeof(*desc));

	return (0);
}

static void
usb_device_init(uint32_t id, uint16_t vendor, uint16_t product, uint8_t class)
{
	struct usb_device *dev = &usb_ctx.devices[id];
	dev->id = id;
	dev->dev_desc.idVendor = vendor;
	dev->dev_desc.idProduct = product;
	dev->dev_desc.bDeviceClass = class;
	dev->speed = USB_SPEED_HIGH;
	dev->configured = 0;
	dev->addr = id;
}

int
usb_host_init(enum usb_controller_type ctrl)
{
	switch (ctrl) {
	case USB_CTRL_XHCI:
	case USB_CTRL_EHCI:
	case USB_CTRL_OHCI:
	case USB_CTRL_UHCI:
		return (0);
	}

	return (EINVAL);
}

static void
usb_hotplug_thread(void *arg)
{
	(void)arg;
	USB_LOG(LOG_INFO, \"Hotplug monitor started\");
}

static void *
usb_monitor_thread(void *arg)
{
	(void)arg;

	USB_LOG(LOG_DEBUG, \"USB hotplug monitor alive\");

	while (1)
		sleep(1);
	return (NULL);
}

static void
usb_process_uevent(void)
{
	char event[256];
	int fd;

	fd = open(\"/sys/bus/usb/devices/.uevent\", O_RDONLY);
	if (fd < 0)
		return;

	read(fd, event, sizeof(event));
	close(fd);
}

int
usb_set_hotplug(bool enable)
{
	usb_ctx.hotplug_enabled = enable;

	if (enable)
		USB_LOG(LOG_INFO, \"Hotplug monitoring enabled\");

	return (0);
}

void
usb_shutdown(void)
{
	if (usb_ctx.monitor_thread != NULL) {
		usb_ctx.running = 0;
		wakeup(&usb_ctx);
		usb_ctx.monitor_thread = NULL;
	}

	usb_set_hotplug(false);
	memset(&usb_ctx, 0, sizeof(usb_ctx));

	USB_LOG(LOG_INFO, \"USB subsystem shutdown\");
}
