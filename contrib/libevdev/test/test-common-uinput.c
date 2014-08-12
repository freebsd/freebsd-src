/*
 * Copyright © 2013 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#include <config.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#ifdef __FreeBSD__
#include <dev/evdev/uinput.h>
#else
#include <linux/uinput.h>
#endif
#include <dirent.h>

#include <libevdev/libevdev.h>
#include <libevdev/libevdev-int.h>
#include <libevdev/libevdev-util.h>
#include <libevdev/libevdev-uinput.h>

#include "test-common-uinput.h"

#define SYS_INPUT_DIR "/sys/class/input"
#define DEV_INPUT_DIR "/dev/input/"

struct uinput_device
{
	struct libevdev *d; /* lazy, it has all the accessors */
	struct libevdev_uinput *uidev;
	int dev_fd; /* open fd to the devnode */
	int uinput_fd;
};

struct uinput_device*
uinput_device_new(const char *name)
{
	struct uinput_device *dev;

	dev = calloc(1, sizeof(*dev));
	if (!dev)
		return NULL;

	dev->d = libevdev_new();
	dev->dev_fd = -1;
	dev->uinput_fd = -1;

	if (name)
		libevdev_set_name(dev->d, name);

	return dev;
}

int
uinput_device_new_with_events_v(struct uinput_device **d, const char *name, const struct input_id *id, va_list args)
{
	int rc;
	struct uinput_device *dev;

	dev = uinput_device_new(name);
	if (!dev)
		return -ENOMEM;
	if (id != DEFAULT_IDS)
		uinput_device_set_ids(dev, id);

	rc = uinput_device_set_event_bits_v(dev, args);

	if (rc == 0)
		rc = uinput_device_create(dev);

	if (rc != 0) {
		uinput_device_free(dev);
		dev = NULL;
	} else
		*d = dev;

	return rc;
}

int
uinput_device_new_with_events(struct uinput_device **d, const char *name, const struct input_id *id, ...)
{
	int rc;
	va_list args;

	va_start(args, id);
	rc = uinput_device_new_with_events_v(d, name, id, args);
	va_end(args);

	return rc;
}

void
uinput_device_free(struct uinput_device *dev)
{
	if (!dev)
		return;

	if (dev->uinput_fd != -1) {
		ioctl(dev->uinput_fd, UI_DEV_DESTROY, NULL);
		close(dev->uinput_fd);
	}
	if (dev->dev_fd != -1)
		close(dev->dev_fd);
	libevdev_free(dev->d);
	libevdev_uinput_destroy(dev->uidev);
	free(dev);
}

int
uinput_device_get_fd(const struct uinput_device *dev)
{
	return dev->dev_fd;
}

const char*
uinput_device_get_devnode(const struct uinput_device *dev)
{
	return libevdev_uinput_get_devnode(dev->uidev);
}

int
uinput_device_create(struct uinput_device* d)
{
	int rc;
	int fd;
	const char *devnode;

	fd = open("/dev/uinput", O_RDWR);
	if (fd < 0)
		goto error;

	d->uinput_fd = fd;

	rc = libevdev_uinput_create_from_device(d->d, fd, &d->uidev);
	if (rc != 0)
		goto error;

	devnode = libevdev_uinput_get_devnode(d->uidev);
	if (devnode == NULL)
		goto error;

	d->dev_fd = open(devnode, O_RDWR);
	if (d->dev_fd == -1)
		goto error;

	/* write abs resolution now */
	if (libevdev_has_event_type(d->d, EV_ABS)) {
		int  code;
		for (code = 0; code < ABS_CNT; code++) {
			const struct input_absinfo *abs;

			/* can't change slots */
			if (code == ABS_MT_SLOT)
				continue;

			abs = libevdev_get_abs_info(d->d, code);
			if (!abs)
				continue;

			rc = ioctl(d->dev_fd, EVIOCSABS(code), abs);
			if (rc < 0) {
				printf("error %s for code %d\n", strerror(-rc), code);
				goto error;
			}
		}
	}

	return 0;

error:
	if (d->dev_fd != -1)
		close(d->dev_fd);
	if (d->uinput_fd != -1)
		close(d->uinput_fd);
	return -errno;

}

int uinput_device_set_name(struct uinput_device *dev, const char *name)
{
	libevdev_set_name(dev->d, name);
	return 0;
}

int uinput_device_set_ids(struct uinput_device *dev, const struct input_id *ids)
{
	libevdev_set_id_product(dev->d, ids->product);
	libevdev_set_id_vendor(dev->d, ids->vendor);
	libevdev_set_id_bustype(dev->d, ids->bustype);
	libevdev_set_id_version(dev->d, ids->version);
	return 0;
}

int
uinput_device_set_bit(struct uinput_device* dev, unsigned int bit)
{
	return libevdev_enable_event_type(dev->d, bit);
}

int
uinput_device_set_prop(struct uinput_device *dev, unsigned int prop)
{
	return libevdev_enable_property(dev->d, prop);
}

int
uinput_device_set_event_bit(struct uinput_device* dev, unsigned int type, unsigned int code)
{
	return libevdev_enable_event_code(dev->d, type, code, NULL);
}

int
uinput_device_set_event_bits_v(struct uinput_device *dev, va_list args)
{
	int type, code;
	int rc = 0;

	do {
		type = va_arg(args, int);
		if (type == -1)
			break;
		code = va_arg(args, int);
		if (code == -1)
			break;
		rc = libevdev_enable_event_code(dev->d, type, code, NULL);
	} while (rc == 0);

	return rc;
}

int
uinput_device_set_event_bits(struct uinput_device *dev, ...)
{
	int rc;
	va_list args;
	va_start(args, dev);
	rc = uinput_device_set_event_bits_v(dev, args);
	va_end(args);

	return rc;
}

int
uinput_device_set_abs_bit(struct uinput_device* dev, unsigned int code, const struct input_absinfo *absinfo)
{
	return libevdev_enable_event_code(dev->d, EV_ABS, code, absinfo);
}

int
uinput_device_event(const struct uinput_device *dev, unsigned int type, unsigned int code, int value)
{
	return libevdev_uinput_write_event(dev->uidev, type, code, value);
}

int uinput_device_event_multiple_v(const struct uinput_device* dev, va_list args)
{
	int type, code, value;
	int rc = 0;

	do {
		type = va_arg(args, int);
		if (type == -1)
			break;
		code = va_arg(args, int);
		if (code == -1)
			break;
		value = va_arg(args, int);
		rc = uinput_device_event(dev, type, code, value);
	} while (rc == 0);

	return rc;
}

int uinput_device_event_multiple(const struct uinput_device* dev, ...)
{
	int rc;
	va_list args;
	va_start(args, dev);
	rc = uinput_device_event_multiple_v(dev, args);
	va_end(args);
	return rc;
}
