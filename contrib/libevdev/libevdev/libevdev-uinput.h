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

#ifndef LIBEVDEV_UINPUT_H
#define LIBEVDEV_UINPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libevdev/libevdev.h>

struct libevdev_uinput;

/**
 * @defgroup uinput uinput device creation
 *
 * Creation of uinput devices based on existing libevdev devices. These functions
 * help to create uinput devices that emulate libevdev devices. In the simplest
 * form it serves to duplicate an existing device:
 *
 @code
 int err;
 int new_fd;
 struct libevdev *dev;
 struct libevdev_uinput *uidev;
 struct input_event ev[2];

 err = libevdev_new_from_fd(&dev, fd);
 if (err != 0)
     return err;

 uifd = open("/dev/uinput", O_RDWR);
 if (uidev < 0)
     return -errno;

 err = libevdev_uinput_create_from_device(dev, uifd, &uidev);
 if (err != 0)
     return err;

 // post a REL_X event
 err = libevdev_uinput_write_event(uidev, EV_REL, REL_X, -1);
 if (err != 0)
     return err;
 libevdev_uinput_write_event(uidev, EV_SYN, SYN_REPORT, 0);
 if (err != 0)
     return err;

 libevdev_uinput_destroy(uidev);
 close(uifd);

 @endcode
 *
 * Alternatively, a device can be constructed from scratch:
 *
 @code
 int err;
 struct libevdev *dev;
 struct libevdev_uinput *uidev;

 dev = libevdev_new();
 libevdev_set_name(dev, "test device");
 libevdev_enable_event_type(dev, EV_REL);
 libevdev_enable_event_code(dev, EV_REL, REL_X);
 libevdev_enable_event_code(dev, EV_REL, REL_Y);
 libevdev_enable_event_type(dev, EV_KEY);
 libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT);
 libevdev_enable_event_code(dev, EV_KEY, BTN_MIDDLE);
 libevdev_enable_event_code(dev, EV_KEY, BTN_RIGHT);

 err = libevdev_uinput_create_from_device(dev,
                                          LIBEVDEV_UINPUT_OPEN_MANAGED,
                                          &uidev);
 if (err != 0)
     return err;

 // ... do something ...

 libevdev_uinput_destroy(uidev);

 @endcode
 */

enum libevdev_uinput_open_mode {
	/* intentionally -2 to avoid to avoid code like the below from accidentally working:
		fd = open("/dev/uinput", O_RDWR); // fails, fd is -1
		libevdev_uinput_create_from_device(dev, fd, &uidev); // may hide the error */
	LIBEVDEV_UINPUT_OPEN_MANAGED = -2  /**< let libevdev open and close @c /dev/uinput */
};

/**
 * @ingroup uinput
 *
 * Create a uinput device based on the given libevdev device. The uinput device
 * will be an exact copy of the libevdev device, minus the bits that uinput doesn't
 * allow to be set.
 *
 * If uinput_fd is @ref LIBEVDEV_UINPUT_OPEN_MANAGED, libevdev_uinput_create_from_device()
 * will open @c /dev/uinput in read/write mode and manage the file descriptor.
 * Otherwise, uinput_fd must be opened by the caller and opened with the
 * appropriate permissions.
 *
 * The device's lifetime is tied to the uinput file descriptor, closing it will
 * destroy the uinput device. You should call libevdev_uinput_destroy() before
 * closing the file descriptor to free allocated resources.
 * A file descriptor can only create one uinput device at a time; the second device
 * will fail with -EINVAL.
 *
 * You don't need to keep the file descriptor variable around,
 * libevdev_uinput_get_fd() will return it when needed.
 *
 * @note Due to limitations in the uinput kernel module, REP_DELAY and
 * REP_PERIOD will default to the kernel defaults, not to the ones set in the
 * source device.
 *
 * @param dev The device to duplicate
 * @param uinput_fd @ref LIBEVDEV_UINPUT_OPEN_MANAGED or a file descriptor to @c /dev/uinput,
 * @param[out] uinput_dev The newly created libevdev device.
 *
 * @return 0 on success or a negative errno on failure. On failure, the value of
 * uinput_dev is unmodified.
 *
 * @see libevdev_uinput_destroy
 */
int libevdev_uinput_create_from_device(const struct libevdev *dev,
				       int uinput_fd,
				       struct libevdev_uinput **uinput_dev);

/**
 * @ingroup uinput
 *
 * Destroy a previously created uinput device and free associated memory.
 *
 * If the device was opened with @ref LIBEVDEV_UINPUT_OPEN_MANAGED,
 * libevdev_uinput_destroy() also closes the file descriptor. Otherwise, the
 * fd is left as-is and must be closed by the caller.
 *
 * @param uinput_dev A previously created uinput device.
 */
void libevdev_uinput_destroy(struct libevdev_uinput *uinput_dev);

/**
 * @ingroup uinput
 *
 * Return the file descriptor used to create this uinput device. This is the
 * fd pointing to <strong>/dev/uinput</strong>. This file descriptor may be used to write
 * events that are emitted by the uinput device.
 * Closing this file descriptor will destroy the uinput device, you should
 * call libevdev_uinput_destroy() first to free allocated resources.
 *
 * @param uinput_dev A previously created uinput device.
 *
 * @return The file descriptor used to create this device
 */
int libevdev_uinput_get_fd(const struct libevdev_uinput *uinput_dev);

/**
 * @ingroup uinput
 *
 * Return the syspath representing this uinput device.
 * At the time of writing, the uinput kernel device does not
 * provide a way to get the syspath directly through uinput so libevdev must guess.
 * In some cases libevdev is unable to derive the syspath. If the running kernel
 * supports the UI_GET_SYSNAME ioctl, the syspath is retrieved through that and will
 * be reliable and not be NULL. The UI_GET_SYSNAME ioctl is currently
 * scheduled for 3.15.
 *
 * @note This function may return NULL. libevdev currently uses ctime and
 * the device name to guess devices. To avoid false positives, wait at least
 * wait at least 1.5s between creating devices that have the same name.
 * @param uinput_dev A previously created uinput device.
 * @return The syspath for this device, including the preceding /sys
 *
 * @see libevdev_uinput_get_devnode
 */
const char*libevdev_uinput_get_syspath(struct libevdev_uinput *uinput_dev);

/**
 * @ingroup uinput
 *
 * Return the device node representing this uinput device.
 *
 * This relies on libevdev_uinput_get_syspath() to provide a valid syspath.
 * See libevdev_uinput_get_syspath() for more details.
 *
 * @note This function may return NULL. libevdev currently has to guess the
 * syspath and the device node. See libevdev_uinput_get_syspath() for details.
 * @param uinput_dev A previously created uinput device.
 * @return The device node for this device, in the form of /dev/input/eventN
 *
 * @see libevdev_uinput_get_syspath
 */
const char* libevdev_uinput_get_devnode(struct libevdev_uinput *uinput_dev);

/**
 * @ingroup uinput
 *
 * Post an event through the uinput device. It is the caller's responsibility
 * that any event sequence is terminated with an EV_SYN/SYN_REPORT/0 event.
 * Otherwise, listeners on the device node will not see the events until the
 * next EV_SYN event is posted.
 *
 * @param uinput_dev A previously created uinput device.
 * @param type Event type (EV_ABS, EV_REL, etc.)
 * @param code Event code (ABS_X, REL_Y, etc.)
 * @param value The event value
 * @return 0 on success or a negative errno on error
 */
int libevdev_uinput_write_event(const struct libevdev_uinput *uinput_dev,
				unsigned int type,
				unsigned int code,
				int value);
#ifdef __cplusplus
}
#endif

#endif /* LIBEVDEV_UINPUT_H */
