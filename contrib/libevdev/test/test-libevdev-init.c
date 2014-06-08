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
#include <errno.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libevdev/libevdev-uinput.h>
#include "test-common.h"

START_TEST(test_new_device)
{
	struct libevdev *dev;

	dev = libevdev_new();
	ck_assert(dev != NULL);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_free_device)
{
	libevdev_free(NULL);
}
END_TEST

START_TEST(test_init_from_invalid_fd)
{
	int rc;
	struct libevdev *dev = NULL;

	rc = libevdev_new_from_fd(-1, &dev);

	ck_assert(dev == NULL);
	ck_assert_int_eq(rc, -EBADF);

	rc = libevdev_new_from_fd(STDIN_FILENO, &dev);
	ck_assert(dev == NULL);
	ck_assert_int_eq(rc, -ENOTTY);
}
END_TEST

START_TEST(test_init_and_change_fd)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;

	dev = libevdev_new();
	ck_assert(dev != NULL);
	ck_assert_int_eq(libevdev_set_fd(dev, -1), -EBADF);

	libevdev_set_log_function(test_logfunc_ignore_error, NULL);
	ck_assert_int_eq(libevdev_change_fd(dev, -1), -1);
	libevdev_set_log_function(test_logfunc_abort_on_error, NULL);

	rc = uinput_device_new_with_events(&uidev,
					   TEST_DEVICE_NAME, DEFAULT_IDS,
					   EV_SYN, SYN_REPORT,
					   EV_REL, REL_X,
					   EV_REL, REL_Y,
					   EV_REL, REL_WHEEL,
					   EV_KEY, BTN_LEFT,
					   EV_KEY, BTN_MIDDLE,
					   EV_KEY, BTN_RIGHT,
					   -1);
	ck_assert_msg(rc == 0, "Failed to create uinput device: %s", strerror(-rc));
	ck_assert_int_eq(libevdev_set_fd(dev, uinput_device_get_fd(uidev)), 0);

	libevdev_set_log_function(test_logfunc_ignore_error, NULL);
	ck_assert_int_eq(libevdev_set_fd(dev, 0), -EBADF);
	libevdev_set_log_function(test_logfunc_abort_on_error, NULL);

	ck_assert_int_eq(libevdev_get_fd(dev), uinput_device_get_fd(uidev));

	ck_assert_int_eq(libevdev_change_fd(dev, 0), 0);
	ck_assert_int_eq(libevdev_get_fd(dev), 0);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

static int log_fn_called = 0;
static char *logdata = "test";
static void logfunc(enum libevdev_log_priority priority,
		    void *data,
		    const char *file, int line, const char *func,
		    const char *f, va_list args) {
	ck_assert_int_eq(strcmp(logdata, data), 0);
	log_fn_called++;
}

START_TEST(test_log_init)
{
	struct libevdev *dev = NULL;
	enum libevdev_log_priority old;

	old = libevdev_get_log_priority();

	libevdev_set_log_priority(LIBEVDEV_LOG_DEBUG);

	libevdev_set_log_function(logfunc, NULL);
	libevdev_set_log_function(NULL, NULL);

	dev = libevdev_new();
	ck_assert(dev != NULL);

	libevdev_set_log_function(logfunc, logdata);
	libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, NULL);

	libevdev_set_log_function(NULL, NULL);
	libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, NULL);

	libevdev_set_log_function(logfunc, logdata);
	libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, NULL);

	/* libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, NULL) should
	   trigger a log message. We called it three times, but only twice
	   with the logfunc set, thus, ensure we only called the logfunc
	   twice */
	ck_assert_int_eq(log_fn_called, 2);

	libevdev_free(dev);

	libevdev_set_log_function(test_logfunc_abort_on_error, NULL);

	log_fn_called = 0;

	libevdev_set_log_priority(old);
}
END_TEST

START_TEST(test_log_default_priority)
{
	ck_assert_int_eq(libevdev_get_log_priority(), LIBEVDEV_LOG_INFO);
}
END_TEST

START_TEST(test_log_set_get_priority)
{
	enum libevdev_log_priority pri;
	enum libevdev_log_priority old;

	old = libevdev_get_log_priority();

	pri = LIBEVDEV_LOG_DEBUG;
	libevdev_set_log_priority(pri);
	ck_assert_int_eq(libevdev_get_log_priority(), pri);

	pri = LIBEVDEV_LOG_INFO;
	libevdev_set_log_priority(pri);
	ck_assert_int_eq(libevdev_get_log_priority(), pri);

	pri = LIBEVDEV_LOG_ERROR;
	libevdev_set_log_priority(pri);
	ck_assert_int_eq(libevdev_get_log_priority(), pri);

	/* debug and above is clamped */
	pri = LIBEVDEV_LOG_DEBUG + 1;
	libevdev_set_log_priority(pri);
	ck_assert_int_eq(libevdev_get_log_priority(), LIBEVDEV_LOG_DEBUG);

	/*  error and below is not clamped, we need this for another test */
	pri = LIBEVDEV_LOG_ERROR - 1;
	libevdev_set_log_priority(pri);
	ck_assert_int_eq(libevdev_get_log_priority(), pri);

	libevdev_set_log_priority(old);
}
END_TEST

START_TEST(test_log_priority)
{
	struct libevdev *dev = NULL;
	enum libevdev_log_priority old;

	old = libevdev_get_log_priority();

	libevdev_set_log_function(logfunc, logdata);

	dev = libevdev_new();
	ck_assert(dev != NULL);

	libevdev_set_log_priority(LIBEVDEV_LOG_DEBUG);
	libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, NULL);
	ck_assert_int_eq(log_fn_called, 1);

	libevdev_set_log_priority(LIBEVDEV_LOG_INFO);
	libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, NULL);
	ck_assert_int_eq(log_fn_called, 2);

	libevdev_set_log_priority(LIBEVDEV_LOG_ERROR);
	libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, NULL);
	ck_assert_int_eq(log_fn_called, 3);

	/* we don't have any log msgs > ERROR at the moment, so test it by
	   setting an invalid priority. */
	libevdev_set_log_priority(LIBEVDEV_LOG_ERROR - 1);
	libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, NULL);
	ck_assert_int_eq(log_fn_called, 3);

	libevdev_free(dev);

	libevdev_set_log_function(test_logfunc_abort_on_error, NULL);

	log_fn_called = 0;

	libevdev_set_log_priority(old);
}
END_TEST

static char *logdata_1 = "foo";
static char *logdata_2 = "bar";
static int log_data_fn_called = 0;
static void logfunc_data(enum libevdev_log_priority priority,
			 void *data,
			 const char *file, int line, const char *func,
			 const char *f, va_list args) {
	switch(log_data_fn_called) {
		case 0: ck_assert(data == logdata_1); break;
		case 1: ck_assert(data == logdata_2); break;
		case 2: ck_assert(data == NULL); break;
		default:
			ck_abort();
	}
	log_data_fn_called++;
}

START_TEST(test_log_data)
{
	struct libevdev *dev = NULL;

	dev = libevdev_new();
	ck_assert(dev != NULL);

	libevdev_set_log_function(logfunc_data, logdata_1);
	libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, NULL);

	libevdev_set_log_function(logfunc_data, logdata_2);
	libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, NULL);

	libevdev_set_log_function(logfunc_data, NULL);
	libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, NULL);

	libevdev_free(dev);
}
END_TEST

struct libevdev *devlogdata;
static int dev_log_fn_called = 0;
static void devlogfunc(const struct libevdev *dev,
		    enum libevdev_log_priority priority,
		    void *data,
		    const char *file, int line, const char *func,
		    const char *f, va_list args)
{
	ck_assert(dev == data);
	dev_log_fn_called++;
}

START_TEST(test_device_log_init)
{
	struct libevdev *dev = NULL;
	enum libevdev_log_priority old;

	old = libevdev_get_log_priority();
	libevdev_set_log_priority(LIBEVDEV_LOG_DEBUG);
	libevdev_set_log_function(logfunc, logdata);

	/* error for NULL device */
	libevdev_set_device_log_function(NULL, NULL,
					 LIBEVDEV_LOG_ERROR, NULL);
	ck_assert_int_eq(log_fn_called, 1);

	/* error for NULL device */
	libevdev_set_device_log_function(NULL, devlogfunc,
					 LIBEVDEV_LOG_ERROR, NULL);
	ck_assert_int_eq(log_fn_called, 2);

	log_fn_called = 0;

	dev = libevdev_new();
	ck_assert(dev != NULL);

	libevdev_set_device_log_function(dev, NULL,
					 LIBEVDEV_LOG_ERROR, NULL);

	/* libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, NULL) should
	   trigger a log message. */

	/* expect global handler triggered */
	libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, NULL);
	ck_assert_int_eq(log_fn_called, 1);
	ck_assert_int_eq(dev_log_fn_called, 0);

	/* expect device handler triggered */
	libevdev_set_device_log_function(dev, devlogfunc,
					 LIBEVDEV_LOG_ERROR, dev);
	libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, NULL);
	ck_assert_int_eq(log_fn_called, 1);
	ck_assert_int_eq(dev_log_fn_called, 1);

	/* device handler set, but priority filters. don't expect any log
	   handler to be called.
	   we don't have any log msgs > ERROR at the moment, so test it by
	   setting an invalid priority. */
	libevdev_set_device_log_function(dev, devlogfunc,
					 LIBEVDEV_LOG_ERROR - 1, dev);
	libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, NULL);
	ck_assert_int_eq(log_fn_called, 1);
	ck_assert_int_eq(dev_log_fn_called, 1);

	libevdev_free(dev);

	log_fn_called = 0;
	libevdev_set_log_priority(old);
	libevdev_set_log_function(test_logfunc_abort_on_error, NULL);

}
END_TEST

START_TEST(test_device_init)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;

	rc = uinput_device_new_with_events(&uidev,
					   TEST_DEVICE_NAME, DEFAULT_IDS,
					   EV_SYN, SYN_REPORT,
					   EV_REL, REL_X,
					   EV_REL, REL_Y,
					   EV_REL, REL_WHEEL,
					   EV_KEY, BTN_LEFT,
					   EV_KEY, BTN_MIDDLE,
					   EV_KEY, BTN_RIGHT,
					   -1);
	ck_assert_msg(rc == 0, "Failed to create uinput device: %s", strerror(-rc));

	dev = libevdev_new();
	ck_assert(dev != NULL);
	rc = libevdev_set_fd(dev, uinput_device_get_fd(uidev));
	ck_assert_msg(rc == 0, "Failed to init device: %s", strerror(-rc));;

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_device_init_from_fd)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;

	rc = uinput_device_new_with_events(&uidev,
					   TEST_DEVICE_NAME, DEFAULT_IDS,
					   EV_SYN, SYN_REPORT,
					   EV_REL, REL_X,
					   EV_REL, REL_Y,
					   EV_REL, REL_WHEEL,
					   EV_KEY, BTN_LEFT,
					   EV_KEY, BTN_MIDDLE,
					   EV_KEY, BTN_RIGHT,
					   -1);
	ck_assert_msg(rc == 0, "Failed to create uinput device: %s", strerror(-rc));

	rc = libevdev_new_from_fd(uinput_device_get_fd(uidev), &dev);
	ck_assert_msg(rc == 0, "Failed to init device: %s", strerror(-rc));;

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_device_grab)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;

	test_create_device(&uidev, &dev,
			   EV_SYN, SYN_REPORT,
			   EV_REL, REL_X,
			   EV_REL, REL_Y,
			   EV_REL, REL_WHEEL,
			   EV_KEY, BTN_LEFT,
			   EV_KEY, BTN_MIDDLE,
			   EV_KEY, BTN_RIGHT,
			   -1);

	libevdev_set_log_function(test_logfunc_ignore_error, NULL);
	rc = libevdev_grab(dev, 0);
	ck_assert_int_eq(rc, -EINVAL);
	rc = libevdev_grab(dev, 1);
	ck_assert_int_eq(rc, -EINVAL);
	libevdev_set_log_function(test_logfunc_abort_on_error, NULL);

	rc = libevdev_grab(dev, LIBEVDEV_UNGRAB);
	ck_assert_int_eq(rc, 0);
	rc = libevdev_grab(dev, LIBEVDEV_GRAB);
	ck_assert_int_eq(rc, 0);
	rc = libevdev_grab(dev, LIBEVDEV_GRAB);
	ck_assert_int_eq(rc, 0);
	rc = libevdev_grab(dev, LIBEVDEV_UNGRAB);
	ck_assert_int_eq(rc, 0);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_set_clock_id)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;

	test_create_device(&uidev, &dev,
			   EV_SYN, SYN_REPORT,
			   EV_REL, REL_X,
			   EV_REL, REL_Y,
			   EV_REL, REL_WHEEL,
			   EV_KEY, BTN_LEFT,
			   EV_KEY, BTN_MIDDLE,
			   EV_KEY, BTN_RIGHT,
			   -1);

	rc = libevdev_set_clock_id(dev, CLOCK_REALTIME);
	ck_assert_int_eq(rc, 0);

	rc = libevdev_set_clock_id(dev, CLOCK_MONOTONIC);
	ck_assert_int_eq(rc, 0);

	rc = libevdev_set_clock_id(dev, CLOCK_MONOTONIC_RAW);
	ck_assert_int_eq(rc, -EINVAL);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_clock_id_events)
{
	struct uinput_device* uidev;
	struct libevdev *dev, *dev2;
	int rc, fd;
	struct input_event ev1, ev2;
	struct timespec t1_real, t2_real;
	struct timespec t1_mono, t2_mono;
	int64_t t1, t2;

	test_create_device(&uidev, &dev,
			   EV_SYN, SYN_REPORT,
			   EV_REL, REL_X,
			   EV_REL, REL_Y,
			   EV_REL, REL_WHEEL,
			   EV_KEY, BTN_LEFT,
			   EV_KEY, BTN_MIDDLE,
			   EV_KEY, BTN_RIGHT,
			   -1);

	fd = open(uinput_device_get_devnode(uidev), O_RDONLY);
	ck_assert_int_gt(fd, -1);

	rc = libevdev_new_from_fd(fd, &dev2);
	ck_assert_msg(rc == 0, "Failed to create second device: %s", strerror(-rc));

	rc = libevdev_set_clock_id(dev2, CLOCK_MONOTONIC);
	ck_assert_int_eq(rc, 0);

	clock_gettime(CLOCK_REALTIME, &t1_real);
	clock_gettime(CLOCK_MONOTONIC, &t1_mono);
	uinput_device_event(uidev, EV_REL, REL_X, 1);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	clock_gettime(CLOCK_REALTIME, &t2_real);
	clock_gettime(CLOCK_MONOTONIC, &t2_mono);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev1);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);

	rc = libevdev_next_event(dev2, LIBEVDEV_READ_FLAG_NORMAL, &ev2);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);

	ck_assert_int_eq(ev1.type, ev2.type);
	ck_assert_int_eq(ev1.code, ev2.code);
	ck_assert_int_eq(ev1.value, ev2.value);

	t1 = ev1.time.tv_sec * 1000000LL + ev1.time.tv_usec;
	t2 = ev2.time.tv_sec * 1000000LL + ev2.time.tv_usec;
	ck_assert_int_ne(t1, t2);

	ck_assert_int_ge(ev1.time.tv_sec, t1_real.tv_sec);
	ck_assert_int_ge(ev1.time.tv_usec, t1_real.tv_nsec/1000);
	ck_assert_int_le(ev1.time.tv_sec, t2_real.tv_sec);
	ck_assert_int_le(ev1.time.tv_usec, t2_real.tv_nsec/1000);

	ck_assert_int_ge(ev2.time.tv_sec, t1_mono.tv_sec);
	ck_assert_int_ge(ev2.time.tv_usec, t1_mono.tv_nsec/1000);
	ck_assert_int_le(ev2.time.tv_sec, t2_mono.tv_sec);
	ck_assert_int_le(ev2.time.tv_usec, t2_mono.tv_nsec/1000);

	uinput_device_free(uidev);
	libevdev_free(dev);
	libevdev_free(dev2);
	close(fd);
}
END_TEST

Suite *
libevdev_init_test(void)
{
	Suite *s = suite_create("libevdev init tests");

	TCase *tc = tcase_create("device init");
	tcase_add_test(tc, test_new_device);
	tcase_add_test(tc, test_free_device);
	tcase_add_test(tc, test_init_from_invalid_fd);
	tcase_add_test(tc, test_init_and_change_fd);
	suite_add_tcase(s, tc);

	tc = tcase_create("log init");
	tcase_add_test(tc, test_log_init);
	tcase_add_test(tc, test_log_priority);
	tcase_add_test(tc, test_log_set_get_priority);
	tcase_add_test(tc, test_log_default_priority);
	tcase_add_test(tc, test_log_data);
	tcase_add_test(tc, test_device_log_init);
	suite_add_tcase(s, tc);

	tc = tcase_create("device fd init");
	tcase_add_test(tc, test_device_init);
	tcase_add_test(tc, test_device_init_from_fd);
	suite_add_tcase(s, tc);

	tc = tcase_create("device grab");
	tcase_add_test(tc, test_device_grab);
	suite_add_tcase(s, tc);

	tc = tcase_create("clock id");
	tcase_add_test(tc, test_set_clock_id);
	tcase_add_test(tc, test_clock_id_events);
	suite_add_tcase(s, tc);

	return s;
}
