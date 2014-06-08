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
#include <check.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include "test-common.h"

void test_logfunc_abort_on_error(enum libevdev_log_priority priority,
				 void *data,
				 const char *file, int line,
				 const char *func,
				 const char *format, va_list args)
{
	vprintf(format, args);
	ck_abort();
}

void test_logfunc_ignore_error(enum libevdev_log_priority priority,
			       void *data,
			       const char *file, int line,
			       const char *func,
			       const char *format, va_list args)
{
}

int test_create_device(struct uinput_device **uidev_return,
		       struct libevdev **dev_return,
		       ...)
{
	int rc, fd;
	struct uinput_device *uidev;
	struct libevdev *dev;
	va_list args;

	va_start(args, dev_return);

	rc = uinput_device_new_with_events_v(&uidev, TEST_DEVICE_NAME, DEFAULT_IDS, args);
	va_end(args);

	ck_assert_msg(rc == 0, "Failed to create uinput device: %s", strerror(-rc));

	fd = uinput_device_get_fd(uidev);

	rc = libevdev_new_from_fd(fd, &dev);
	ck_assert_msg(rc == 0, "Failed to init device device: %s", strerror(-rc));
	rc = fcntl(fd, F_SETFL, O_NONBLOCK);
	ck_assert_msg(rc == 0, "fcntl failed: %s", strerror(errno));

	*uidev_return = uidev;
	*dev_return = dev;

	return rc == 0 ? rc : -errno;
}

int test_create_abs_device(struct uinput_device **uidev_return,
			   struct libevdev **dev_return,
			   int nabs,
			   const struct input_absinfo *abs,
			   ...)
{
	int rc, fd;
	struct uinput_device *uidev;
	struct libevdev *dev;
	va_list args;

	uidev = uinput_device_new(TEST_DEVICE_NAME);
	ck_assert(uidev != NULL);

	va_start(args, abs);
	rc = uinput_device_set_event_bits_v(uidev, args);
	va_end(args);

	while (--nabs >= 0) {
		int code;
		struct input_absinfo a;

		code = abs[nabs].value;
		a = abs[nabs];
		a.value = 0;

		rc = uinput_device_set_abs_bit(uidev, code, &a);
		ck_assert_msg(rc == 0, "for abs field %d\n", nabs);
	}

	rc = uinput_device_create(uidev);
	ck_assert_msg(rc == 0, "Failed to create uinput device: %s", strerror(-rc));

	fd = uinput_device_get_fd(uidev);

	rc = libevdev_new_from_fd(fd, &dev);
	ck_assert_msg(rc == 0, "Failed to init device device: %s", strerror(-rc));
	rc = fcntl(fd, F_SETFL, O_NONBLOCK);
	ck_assert_msg(rc == 0, "fcntl failed: %s", strerror(errno));

	*uidev_return = uidev;
	*dev_return = dev;

	return rc == 0 ? rc : -errno;
}
