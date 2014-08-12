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
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#ifdef __FreeBSD__
#include <dev/evdev/uinput.h>
#else
#include <linux/uinput.h>
#endif

#include "libevdev.h"
#include "libevdev-int.h"
#include "libevdev-uinput.h"
#include "libevdev-uinput-int.h"
#include "libevdev-util.h"

#define SYS_INPUT_DIR "/sys/devices/virtual/input/"

#ifndef UINPUT_IOCTL_BASE
#define UINPUT_IOCTL_BASE       'U'
#endif

#ifndef UI_SET_PROPBIT
#define UI_SET_PROPBIT _IOW(UINPUT_IOCTL_BASE, 110, int)
#endif

static struct libevdev_uinput *
alloc_uinput_device(const char *name)
{
	struct libevdev_uinput *uinput_dev;

	uinput_dev = calloc(1, sizeof(struct libevdev_uinput));
	if (uinput_dev) {
		uinput_dev->name = strdup(name);
		uinput_dev->fd = -1;
	}

	return uinput_dev;
}

static int
set_evbits(const struct libevdev *dev, int fd, struct uinput_user_dev *uidev)
{
	int rc = 0;
	unsigned int type;

	for (type = 0; type < EV_CNT; type++) {
		unsigned int code;
		int max;
		int uinput_bit;
		const unsigned long *mask;

		if (!libevdev_has_event_type(dev, type))
			continue;

		rc = ioctl(fd, UI_SET_EVBIT, type);
		if (rc == -1)
			break;

		/* uinput can't set EV_REP */
		if (type == EV_REP)
			continue;

		max = type_to_mask_const(dev, type, &mask);
		if (max == -1)
			continue;

		switch(type) {
			case EV_KEY: uinput_bit = UI_SET_KEYBIT; break;
			case EV_REL: uinput_bit = UI_SET_RELBIT; break;
			case EV_ABS: uinput_bit = UI_SET_ABSBIT; break;
			case EV_MSC: uinput_bit = UI_SET_MSCBIT; break;
			case EV_LED: uinput_bit = UI_SET_LEDBIT; break;
			case EV_SND: uinput_bit = UI_SET_SNDBIT; break;
			case EV_FF: uinput_bit = UI_SET_FFBIT; break;
			case EV_SW: uinput_bit = UI_SET_SWBIT; break;
			default:
				    rc = -1;
				    errno = EINVAL;
				    goto out;
		}

		for (code = 0; code <= (unsigned int)max; code++) {
			if (!libevdev_has_event_code(dev, type, code))
				continue;

			rc = ioctl(fd, uinput_bit, code);
			if (rc == -1)
				goto out;

			if (type == EV_ABS) {
				const struct input_absinfo *abs = libevdev_get_abs_info(dev, code);
				uidev->absmin[code] = abs->minimum;
				uidev->absmax[code] = abs->maximum;
				uidev->absfuzz[code] = abs->fuzz;
				uidev->absflat[code] = abs->flat;
				/* uinput has no resolution in the device struct, this needs
				 * to be fixed in the kernel */
			}
		}

	}

out:
	return rc;
}

static int
set_props(const struct libevdev *dev, int fd, struct uinput_user_dev *uidev)
{
	unsigned int prop;
	int rc = 0;

	for (prop = 0; prop <= INPUT_PROP_MAX; prop++) {
		if (!libevdev_has_property(dev, prop))
			continue;

		rc = ioctl(fd, UI_SET_PROPBIT, prop);
		if (rc == -1) {
			/* If UI_SET_PROPBIT is not supported, treat -EINVAL
			 * as success. The kernel only sends -EINVAL for an
			 * invalid ioctl, invalid INPUT_PROP_MAX or if the
			 * ioctl is called on an already created device. The
			 * last two can't happen here.
			 */
			if (errno == -EINVAL)
				rc = 0;
			break;
		}
	}
	return rc;
}

LIBEVDEV_EXPORT int
libevdev_uinput_get_fd(const struct libevdev_uinput *uinput_dev)
{
	return uinput_dev->fd;
}

static int is_event_device(const struct dirent *dent) {
	return strncmp("event", dent->d_name, 5) == 0;
}

static char *
fetch_device_node(const char *path)
{
	char *devnode = NULL;
	struct dirent **namelist;
	int ndev, i;

	ndev = scandir(path, &namelist, is_event_device, alphasort);
	if (ndev <= 0)
		return NULL;

	/* ndev should only ever be 1 */

	for (i = 0; i < ndev; i++) {
		if (!devnode && asprintf(&devnode, "/dev/input/%s", namelist[i]->d_name) == -1)
			devnode = NULL;
		free(namelist[i]);
	}

	free(namelist);

	return devnode;
}

static int is_input_device(const struct dirent *dent) {
	return strncmp("input", dent->d_name, 5) == 0;
}

static int
fetch_syspath_and_devnode(struct libevdev_uinput *uinput_dev)
{
	struct dirent **namelist;
	int ndev, i;

	/* FIXME: use new ioctl() here once kernel supports it */

	ndev = scandir(SYS_INPUT_DIR, &namelist, is_input_device, alphasort);
	if (ndev <= 0)
		return -1;

	for (i = 0; i < ndev; i++) {
		int fd, len;
		char buf[sizeof(SYS_INPUT_DIR) + 64];
		struct stat st;

		strcpy(buf, SYS_INPUT_DIR);
		strcat(buf, namelist[i]->d_name);

		if (stat(buf, &st) == -1)
			continue;

		/* created before UI_DEV_CREATE, or after it finished */
		if (st.st_ctime < uinput_dev->ctime[0] ||
		    st.st_ctime > uinput_dev->ctime[1])
			continue;

		/* created within time frame */
		strcat(buf, "/name");
		fd = open(buf, O_RDONLY);
		if (fd < 0)
			continue;

		len = read(fd, buf, sizeof(buf));
		close(fd);
		if (len <= 0)
			continue;

		buf[len - 1] = '\0'; /* file contains \n */
		if (strcmp(buf, uinput_dev->name) == 0) {
			if (uinput_dev->syspath) {
				/* FIXME: could descend into bit comparison here */
				log_info(NULL, "multiple identical devices found. syspath is unreliable\n");
				break;
			} else {
				strcpy(buf, SYS_INPUT_DIR);
				strcat(buf, namelist[i]->d_name);
				uinput_dev->syspath = strdup(buf);
				uinput_dev->devnode = fetch_device_node(buf);
			}
		}
	}

	for (i = 0; i < ndev; i++)
		free(namelist[i]);
	free(namelist);

	return uinput_dev->devnode ? 0 : -1;
}



LIBEVDEV_EXPORT int
libevdev_uinput_create_from_device(const struct libevdev *dev, int fd, struct libevdev_uinput** uinput_dev)
{
	int rc;
	struct uinput_user_dev uidev;
	struct libevdev_uinput *new_device;

	new_device = alloc_uinput_device(libevdev_get_name(dev));
	if (!new_device)
		return -ENOMEM;

	if (fd == LIBEVDEV_UINPUT_OPEN_MANAGED) {
		fd = open("/dev/uinput", O_RDWR|O_CLOEXEC);
		if (fd < 0)
			goto error;

		new_device->fd_is_managed = 1;
	} else if (fd < 0) {
		log_bug(NULL, "Invalid fd %d\n", fd);
		errno = EBADF;
		goto error;
	}

	memset(&uidev, 0, sizeof(uidev));

	strncpy(uidev.name, libevdev_get_name(dev), UINPUT_MAX_NAME_SIZE - 1);
	uidev.id.vendor = libevdev_get_id_vendor(dev);
	uidev.id.product = libevdev_get_id_product(dev);
	uidev.id.bustype = libevdev_get_id_bustype(dev);
	uidev.id.version = libevdev_get_id_version(dev);

	if (set_evbits(dev, fd, &uidev) != 0)
		goto error;
	if (set_props(dev, fd, &uidev) != 0)
		goto error;

	rc = write(fd, &uidev, sizeof(uidev));
	if (rc < 0)
		goto error;
	else if ((size_t)rc < sizeof(uidev)) {
		errno = EINVAL;
		goto error;
	}

	/* ctime notes time before/after ioctl to help us filter out devices
	   when traversing /sys/devices/virtual/input to find the device
	   node.

	   this is in seconds, so ctime[0]/[1] will almost always be
	   identical but /sys doesn't give us sub-second ctime so...
	 */
	new_device->ctime[0] = time(NULL);

	rc = ioctl(fd, UI_DEV_CREATE, NULL);
	if (rc == -1)
		goto error;

	new_device->ctime[1] = time(NULL);
	new_device->fd = fd;

	if (fetch_syspath_and_devnode(new_device) == -1) {
		log_error(NULL, "unable to fetch syspath or device node.\n");
		errno = ENODEV;
		goto error;
	}

	*uinput_dev = new_device;

	return 0;

error:
	libevdev_uinput_destroy(new_device);
	return -errno;
}

LIBEVDEV_EXPORT void
libevdev_uinput_destroy(struct libevdev_uinput *uinput_dev)
{
	if (!uinput_dev)
		return;

	ioctl(uinput_dev->fd, UI_DEV_DESTROY, NULL);
	if (uinput_dev->fd_is_managed)
		close(uinput_dev->fd);
	free(uinput_dev->syspath);
	free(uinput_dev->devnode);
	free(uinput_dev->name);
	free(uinput_dev);
}

LIBEVDEV_EXPORT const char*
libevdev_uinput_get_syspath(struct libevdev_uinput *uinput_dev)
{
	return uinput_dev->syspath;
}

LIBEVDEV_EXPORT const char*
libevdev_uinput_get_devnode(struct libevdev_uinput *uinput_dev)
{
	return uinput_dev->devnode;
}

LIBEVDEV_EXPORT int
libevdev_uinput_write_event(const struct libevdev_uinput *uinput_dev,
			    unsigned int type,
			    unsigned int code,
			    int value)
{
	struct input_event ev = { {0,0}, type, code, value };
	int fd = libevdev_uinput_get_fd(uinput_dev);
	int rc, max;

	if (type > EV_MAX)
		return -EINVAL;

	max = libevdev_event_type_get_max(type);
	if (max == -1 || code > (unsigned int)max)
		return -EINVAL;

	rc = write(fd, &ev, sizeof(ev));

	return rc < 0 ? -errno : 0;
}
