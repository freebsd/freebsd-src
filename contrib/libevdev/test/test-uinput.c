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
#include <linux/input.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <libevdev/libevdev-uinput.h>

#include "test-common.h"
#define UINPUT_NODE "/dev/uinput"

START_TEST(test_uinput_create_device)
{
	struct libevdev *dev, *dev2;
	struct libevdev_uinput *uidev;
	int fd, uinput_fd;
	unsigned int type, code;
	int rc;
	const char *devnode;

	dev = libevdev_new();
	ck_assert(dev != NULL);
	libevdev_set_name(dev, TEST_DEVICE_NAME);
	libevdev_enable_event_type(dev, EV_SYN);
	libevdev_enable_event_type(dev, EV_REL);
	libevdev_enable_event_code(dev, EV_REL, REL_X, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_Y, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_MAX, NULL);

	rc = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
	ck_assert_int_eq(rc, 0);
	ck_assert(uidev != NULL);

	uinput_fd = libevdev_uinput_get_fd(uidev);
	ck_assert_int_gt(uinput_fd, -1);

	devnode = libevdev_uinput_get_devnode(uidev);
	ck_assert(devnode != NULL);

	fd = open(devnode, O_RDONLY);
	ck_assert_int_gt(fd, -1);
	rc = libevdev_new_from_fd(fd, &dev2);
	ck_assert_int_eq(rc, 0);

	for (type = 0; type < EV_CNT; type++) {
		int max = libevdev_event_type_get_max(type);
		if (max == -1)
			continue;

		for (code = 0; code < max; code++) {
			ck_assert_int_eq(libevdev_has_event_code(dev, type, code),
					 libevdev_has_event_code(dev2, type, code));
		}
	}

	libevdev_free(dev);
	libevdev_free(dev2);
	libevdev_uinput_destroy(uidev);
	close(fd);

	/* uinput fd is managed, so make sure it did get closed */
	ck_assert_int_eq(close(uinput_fd), -1);
	ck_assert_int_eq(errno, EBADF);

}
END_TEST

START_TEST(test_uinput_create_device_invalid)
{
	struct libevdev *dev;
	struct libevdev_uinput *uidev = NULL;
	int rc;

	dev = libevdev_new();
	ck_assert(dev != NULL);
	libevdev_set_name(dev, TEST_DEVICE_NAME);
	libevdev_enable_event_type(dev, EV_SYN);
	libevdev_enable_event_type(dev, EV_REL);
	libevdev_enable_event_code(dev, EV_REL, REL_X, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_Y, NULL);

	libevdev_set_log_function(test_logfunc_ignore_error, NULL);
	rc = libevdev_uinput_create_from_device(dev, -1, &uidev);
	ck_assert_int_eq(rc, -EBADF);
	ck_assert(uidev == NULL);
	libevdev_set_log_function(test_logfunc_abort_on_error, NULL);

	libevdev_free(dev);
}
END_TEST

START_TEST(test_uinput_create_device_from_fd)
{
	struct libevdev *dev, *dev2;
	struct libevdev_uinput *uidev;
	int fd, fd2;
	unsigned int type, code;
	int rc;
	const char *devnode;

	dev = libevdev_new();
	ck_assert(dev != NULL);
	libevdev_set_name(dev, TEST_DEVICE_NAME);
	libevdev_enable_event_type(dev, EV_SYN);
	libevdev_enable_event_type(dev, EV_REL);
	libevdev_enable_event_code(dev, EV_REL, REL_X, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_Y, NULL);

	fd = open(UINPUT_NODE, O_RDWR);
	ck_assert_int_gt(fd, -1);

	rc = libevdev_uinput_create_from_device(dev, fd, &uidev);
	ck_assert_int_eq(rc, 0);
	ck_assert(uidev != NULL);

	ck_assert_int_eq(libevdev_uinput_get_fd(uidev), fd);

	devnode = libevdev_uinput_get_devnode(uidev);
	ck_assert(devnode != NULL);

	fd2 = open(devnode, O_RDONLY);
	ck_assert_int_gt(fd2, -1);
	rc = libevdev_new_from_fd(fd2, &dev2);
	ck_assert_int_eq(rc, 0);

	for (type = 0; type < EV_CNT; type++) {
		int max = libevdev_event_type_get_max(type);
		if (max == -1)
			continue;

		for (code = 0; code < max; code++) {
			ck_assert_int_eq(libevdev_has_event_code(dev, type, code),
					 libevdev_has_event_code(dev2, type, code));
		}
	}

	libevdev_free(dev);
	libevdev_free(dev2);
	libevdev_uinput_destroy(uidev);
	close(fd);
	close(fd2);
}
END_TEST

START_TEST(test_uinput_check_syspath_time)
{
	struct libevdev *dev;
	struct libevdev_uinput *uidev, *uidev2;
	const char *syspath, *syspath2;
	int fd, fd2;
	int rc;

	dev = libevdev_new();
	ck_assert(dev != NULL);
	libevdev_set_name(dev, TEST_DEVICE_NAME);
	libevdev_enable_event_type(dev, EV_SYN);
	libevdev_enable_event_type(dev, EV_REL);
	libevdev_enable_event_code(dev, EV_REL, REL_X, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_Y, NULL);

	fd = open(UINPUT_NODE, O_RDWR);
	ck_assert_int_gt(fd, -1);
	fd2 = open(UINPUT_NODE, O_RDWR);
	ck_assert_int_gt(fd2, -1);

	rc = libevdev_uinput_create_from_device(dev, fd, &uidev);
	ck_assert_int_eq(rc, 0);

	/* sleep for 1.5 seconds. sysfs resolution is 1 second, so
	   creating both devices without delay means
	   libevdev_uinput_get_syspath can't actually differ between
	   them. By waiting, we get different ctime for uidev and uidev2,
	   and exercise that part of the code.
	 */
	usleep(1500000);

	/* create a second one to test the syspath time filtering code */
	rc = libevdev_uinput_create_from_device(dev, fd2, &uidev2);
	ck_assert_int_eq(rc, 0);

	syspath = libevdev_uinput_get_syspath(uidev);
	ck_assert(syspath != NULL);

	/* get syspath twice returns same pointer */
	syspath2 = libevdev_uinput_get_syspath(uidev);
	ck_assert(syspath == syspath2);

	/* second dev has different syspath */
	syspath2 = libevdev_uinput_get_syspath(uidev2);
	ck_assert(strcmp(syspath, syspath2) != 0);

	libevdev_free(dev);
	libevdev_uinput_destroy(uidev);
	libevdev_uinput_destroy(uidev2);

	close(fd);
	close(fd2);
}
END_TEST

START_TEST(test_uinput_check_syspath_name)
{
	struct libevdev *dev;
	struct libevdev_uinput *uidev, *uidev2;
	const char *syspath, *syspath2;
	int fd, fd2;
	int rc;

	dev = libevdev_new();
	ck_assert(dev != NULL);
	libevdev_set_name(dev, TEST_DEVICE_NAME);
	libevdev_enable_event_type(dev, EV_SYN);
	libevdev_enable_event_type(dev, EV_REL);
	libevdev_enable_event_code(dev, EV_REL, REL_X, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_Y, NULL);

	fd = open(UINPUT_NODE, O_RDWR);
	ck_assert_int_gt(fd, -1);
	fd2 = open(UINPUT_NODE, O_RDWR);
	ck_assert_int_gt(fd2, -1);

	rc = libevdev_uinput_create_from_device(dev, fd, &uidev);
	ck_assert_int_eq(rc, 0);

	/* create a second one to stress the syspath filtering code */
	libevdev_set_name(dev, TEST_DEVICE_NAME " 2");
	rc = libevdev_uinput_create_from_device(dev, fd2, &uidev2);
	ck_assert_int_eq(rc, 0);

	syspath = libevdev_uinput_get_syspath(uidev);
	ck_assert(syspath != NULL);

	/* get syspath twice returns same pointer */
	syspath2 = libevdev_uinput_get_syspath(uidev);
	ck_assert(syspath == syspath2);

	/* second dev has different syspath */
	syspath2 = libevdev_uinput_get_syspath(uidev2);
	ck_assert(strcmp(syspath, syspath2) != 0);

	libevdev_free(dev);
	libevdev_uinput_destroy(uidev);
	libevdev_uinput_destroy(uidev2);

	close(fd);
	close(fd2);
}
END_TEST

START_TEST(test_uinput_events)
{
	struct libevdev *dev;
	struct libevdev_uinput *uidev;
	int fd, fd2;
	int rc;
	const char *devnode;
	int i;
	const int nevents = 5;
	struct input_event events[] = { {{0, 0}, EV_REL, REL_X, 1},
					{{0, 0}, EV_REL, REL_Y, -1},
					{{0, 0}, EV_SYN, SYN_REPORT, 0},
					{{0, 0}, EV_KEY, BTN_LEFT, 1},
					{{0, 0}, EV_SYN, SYN_REPORT, 0}};
	struct input_event events_read[nevents];

	dev = libevdev_new();
	ck_assert(dev != NULL);
	libevdev_set_name(dev, TEST_DEVICE_NAME);
	libevdev_enable_event_type(dev, EV_SYN);
	libevdev_enable_event_type(dev, EV_REL);
	libevdev_enable_event_type(dev, EV_KEY);
	libevdev_enable_event_code(dev, EV_REL, REL_X, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_Y, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, NULL);

	fd = open(UINPUT_NODE, O_RDWR);
	ck_assert_int_gt(fd, -1);

	rc = libevdev_uinput_create_from_device(dev, fd, &uidev);
	ck_assert_int_eq(rc, 0);
	ck_assert(uidev != NULL);

	devnode = libevdev_uinput_get_devnode(uidev);
	ck_assert(devnode != NULL);

	fd2 = open(devnode, O_RDONLY);

	for (i = 0; i < nevents; i++)
		libevdev_uinput_write_event(uidev, events[i].type, events[i].code, events[i].value);

	rc = read(fd2, events_read, sizeof(events_read));
	ck_assert_int_eq(rc, sizeof(events_read));

	for (i = 0; i < nevents; i++) {
		ck_assert_int_eq(events[i].type, events_read[i].type);
		ck_assert_int_eq(events[i].code, events_read[i].code);
		ck_assert_int_eq(events[i].value, events_read[i].value);
	}

	libevdev_free(dev);
	libevdev_uinput_destroy(uidev);
	close(fd);
	close(fd2);
}
END_TEST

START_TEST(test_uinput_properties)
{
	struct libevdev *dev, *dev2;
	struct libevdev_uinput *uidev;
	int fd;
	int rc;
	const char *devnode;

	dev = libevdev_new();
	ck_assert(dev != NULL);
	libevdev_set_name(dev, TEST_DEVICE_NAME);
	libevdev_enable_event_type(dev, EV_SYN);
	libevdev_enable_event_type(dev, EV_REL);
	libevdev_enable_event_type(dev, EV_KEY);
	libevdev_enable_event_code(dev, EV_REL, REL_X, NULL);
	libevdev_enable_event_code(dev, EV_REL, REL_Y, NULL);
	libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, NULL);
	libevdev_enable_property(dev, INPUT_PROP_BUTTONPAD);
	libevdev_enable_property(dev, INPUT_PROP_MAX);

	rc = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev);
	ck_assert_int_eq(rc, 0);
	ck_assert(uidev != NULL);

	devnode = libevdev_uinput_get_devnode(uidev);
	ck_assert(devnode != NULL);

	fd = open(devnode, O_RDONLY);
	ck_assert_int_gt(fd, -1);
	rc = libevdev_new_from_fd(fd, &dev2);
	ck_assert_int_eq(rc, 0);

	ck_assert(libevdev_has_property(dev2, INPUT_PROP_BUTTONPAD));
	ck_assert(libevdev_has_property(dev2, INPUT_PROP_MAX));

	libevdev_free(dev);
	libevdev_free(dev2);
	libevdev_uinput_destroy(uidev);
	close(fd);
}
END_TEST

Suite *
uinput_suite(void)
{
	Suite *s = suite_create("libevdev uinput device tests");

	TCase *tc = tcase_create("device creation");
	tcase_add_test(tc, test_uinput_create_device);
	tcase_add_test(tc, test_uinput_create_device_invalid);
	tcase_add_test(tc, test_uinput_create_device_from_fd);
	tcase_add_test(tc, test_uinput_check_syspath_time);
	tcase_add_test(tc, test_uinput_check_syspath_name);
	suite_add_tcase(s, tc);

	tc = tcase_create("device events");
	tcase_add_test(tc, test_uinput_events);
	suite_add_tcase(s, tc);

	tc = tcase_create("device properties");
	tcase_add_test(tc, test_uinput_properties);
	suite_add_tcase(s, tc);

	return s;
}
