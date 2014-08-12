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
#ifdef __FreeBSD__
#include <dev/evdev/input.h>
#else
#include <linux/input.h>
#endif
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <libevdev/libevdev-util.h>

#include "test-common.h"

START_TEST(test_next_event)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;

	test_create_device(&uidev, &dev,
			   EV_REL, REL_X,
			   EV_REL, REL_Y,
			   EV_KEY, BTN_LEFT,
			   -1);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, -EAGAIN);

	uinput_device_event(uidev, EV_KEY, BTN_LEFT, 1);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(ev.type, EV_KEY);
	ck_assert_int_eq(ev.code, BTN_LEFT);
	ck_assert_int_eq(ev.value, 1);

	libevdev_free(dev);
	uinput_device_free(uidev);

}
END_TEST

START_TEST(test_syn_dropped_event)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;
	int pipefd[2];

	test_create_device(&uidev, &dev,
			   EV_SYN, SYN_REPORT,
			   EV_SYN, SYN_DROPPED,
			   EV_REL, REL_X,
			   EV_REL, REL_Y,
			   EV_KEY, BTN_LEFT,
			   -1);

	/* This is a bit complicated:
	   we can't get SYN_DROPPED through uinput, so we push two events down
	   uinput, and fetch one off libevdev (reading in the other one on the
	   way). Then write a SYN_DROPPED on a pipe, switch the fd and read
	   one event off the wire (but returning the second event from
	   before). Switch back, so that when we do read off the SYN_DROPPED
	   we have the fd back on the device and the ioctls work.
	 */
	uinput_device_event(uidev, EV_KEY, BTN_LEFT, 1);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(ev.type, EV_KEY);
	ck_assert_int_eq(ev.code, BTN_LEFT);
	rc = pipe2(pipefd, O_NONBLOCK);
	ck_assert_int_eq(rc, 0);

	libevdev_change_fd(dev, pipefd[0]);
	ev.type = EV_SYN;
	ev.code = SYN_DROPPED;
	ev.value = 0;
	rc = write(pipefd[1], &ev, sizeof(ev));
	ck_assert_int_eq(rc, sizeof(ev));
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);

	libevdev_change_fd(dev, uinput_device_get_fd(uidev));

	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_REPORT);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_DROPPED);

	/* only check for the rc, nothing actually changed on the device */

	libevdev_free(dev);
	uinput_device_free(uidev);

	close(pipefd[0]);
	close(pipefd[1]);

}
END_TEST

void double_syn_dropped_logfunc(enum libevdev_log_priority priority,
				void *data,
				const char *file, int line,
				const char *func,
				const char *format, va_list args)
{
	unsigned int *hit = data;
	*hit = 1;
}

START_TEST(test_double_syn_dropped_event)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;
	int pipefd[2];
	unsigned int logfunc_hit = 0;

	test_create_device(&uidev, &dev,
			   EV_SYN, SYN_REPORT,
			   EV_SYN, SYN_DROPPED,
			   EV_REL, REL_X,
			   EV_REL, REL_Y,
			   EV_KEY, BTN_LEFT,
			   -1);

	libevdev_set_log_function(double_syn_dropped_logfunc,  &logfunc_hit);

	/* This is a bit complicated:
	   we can't get SYN_DROPPED through uinput, so we push two events down
	   uinput, and fetch one off libevdev (reading in the other one on the
	   way). Then write a SYN_DROPPED on a pipe, switch the fd and read
	   one event off the wire (but returning the second event from
	   before). Switch back, so that when we do read off the SYN_DROPPED
	   we have the fd back on the device and the ioctls work.
	 */
	uinput_device_event(uidev, EV_KEY, BTN_LEFT, 1);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(ev.type, EV_KEY);
	ck_assert_int_eq(ev.code, BTN_LEFT);
	rc = pipe2(pipefd, O_NONBLOCK);
	ck_assert_int_eq(rc, 0);

	libevdev_change_fd(dev, pipefd[0]);
	ev.type = EV_SYN;
	ev.code = SYN_DROPPED;
	ev.value = 0;
	rc = write(pipefd[1], &ev, sizeof(ev));
	ck_assert_int_eq(rc, sizeof(ev));
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);

	/* sneak in a button change event while we're not looking, this way
	 * the sync queue contains 2 events: BTN_LEFT and SYN_REPORT. */
	uinput_device_event(uidev, EV_KEY, BTN_LEFT, 0);
	ck_assert_int_eq(read(pipefd[0], &ev, sizeof(ev)), -1);

	libevdev_change_fd(dev, uinput_device_get_fd(uidev));

	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_REPORT);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_DROPPED);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_KEY);
	ck_assert_int_eq(ev.code, BTN_LEFT);
	ck_assert_int_eq(ev.value, 0);

	/* now write the second SYN_DROPPED on the pipe so we pick it up
	 * before we finish syncing. */
	libevdev_change_fd(dev, pipefd[0]);
	ev.type = EV_SYN;
	ev.code = SYN_DROPPED;
	ev.value = 0;
	rc = write(pipefd[1], &ev, sizeof(ev));
	ck_assert_int_eq(rc, sizeof(ev));

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_REPORT);
	ck_assert_int_eq(ev.value, 0);

	/* back to enable the ioctls again */
	libevdev_change_fd(dev, uinput_device_get_fd(uidev));

	ck_assert_int_eq(logfunc_hit, 1);

	libevdev_free(dev);
	uinput_device_free(uidev);

	close(pipefd[0]);
	close(pipefd[1]);

	libevdev_set_log_function(test_logfunc_abort_on_error, NULL);
}
END_TEST

START_TEST(test_event_type_filtered)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;

	test_create_device(&uidev, &dev,
			   EV_REL, REL_X,
			   EV_REL, REL_Y,
			   EV_KEY, BTN_LEFT,
			   -1);

	libevdev_disable_event_type(dev, EV_REL);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, -EAGAIN);

	uinput_device_event(uidev, EV_REL, REL_X, 1);
	uinput_device_event(uidev, EV_KEY, REL_Y, 1);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_REPORT);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, -EAGAIN);

	libevdev_free(dev);
	uinput_device_free(uidev);

}
END_TEST

START_TEST(test_event_code_filtered)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;

	test_create_device(&uidev, &dev,
			   EV_REL, REL_X,
			   EV_REL, REL_Y,
			   EV_KEY, BTN_LEFT,
			   -1);

	libevdev_disable_event_code(dev, EV_REL, REL_X);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, -EAGAIN);

	uinput_device_event(uidev, EV_REL, REL_X, 1);
	uinput_device_event(uidev, EV_REL, REL_Y, 1);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(ev.type, EV_REL);
	ck_assert_int_eq(ev.code, REL_Y);
	ck_assert_int_eq(ev.value, 1);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_REPORT);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, -EAGAIN);

	libevdev_free(dev);
	uinput_device_free(uidev);

}
END_TEST

START_TEST(test_has_event_pending)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;

	test_create_device(&uidev, &dev,
			   EV_REL, REL_X,
			   EV_REL, REL_Y,
			   EV_KEY, BTN_LEFT,
			   -1);

	ck_assert_int_eq(libevdev_has_event_pending(dev), 0);

	uinput_device_event(uidev, EV_REL, REL_X, 1);
	uinput_device_event(uidev, EV_REL, REL_Y, 1);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);

	ck_assert_int_eq(libevdev_has_event_pending(dev), 1);

	libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);

	ck_assert_int_eq(libevdev_has_event_pending(dev), 1);

	while ((rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)) != -EAGAIN)
			;

	ck_assert_int_eq(libevdev_has_event_pending(dev), 0);

	libevdev_change_fd(dev, -1);
	ck_assert_int_eq(libevdev_has_event_pending(dev), -EBADF);

	libevdev_free(dev);
	uinput_device_free(uidev);

}
END_TEST

START_TEST(test_syn_delta_button)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;

	test_create_device(&uidev, &dev,
			   EV_SYN, SYN_REPORT,
			   EV_SYN, SYN_DROPPED,
			   EV_REL, REL_X,
			   EV_REL, REL_Y,
			   EV_KEY, BTN_LEFT,
			   EV_KEY, BTN_MIDDLE,
			   EV_KEY, BTN_RIGHT,
			   EV_KEY, KEY_MAX,
			   -1);

	uinput_device_event(uidev, EV_KEY, BTN_LEFT, 1);
	uinput_device_event(uidev, EV_KEY, BTN_RIGHT, 1);
	uinput_device_event(uidev, EV_KEY, KEY_MAX, 1);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_FORCE_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_KEY);
	ck_assert_int_eq(ev.code, BTN_LEFT);
	ck_assert_int_eq(ev.value, 1);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_KEY);
	ck_assert_int_eq(ev.code, BTN_RIGHT);
	ck_assert_int_eq(ev.value, 1);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_KEY);
	ck_assert_int_eq(ev.code, KEY_MAX);
	ck_assert_int_eq(ev.value, 1);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_REPORT);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, -EAGAIN);

	ck_assert(libevdev_get_event_value(dev, EV_KEY, BTN_LEFT));
	ck_assert(libevdev_get_event_value(dev, EV_KEY, BTN_RIGHT));
	ck_assert(!libevdev_get_event_value(dev, EV_KEY, BTN_MIDDLE));
	ck_assert(libevdev_get_event_value(dev, EV_KEY, KEY_MAX));

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_syn_delta_abs)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;
	struct input_absinfo abs[3];

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;

	abs[1].value = ABS_Y;
	abs[1].maximum = 1000;

	abs[2].value = ABS_MAX;
	abs[2].maximum = 1000;

	test_create_abs_device(&uidev, &dev,
			       3, abs,
			       EV_SYN, SYN_REPORT,
			       EV_SYN, SYN_DROPPED,
			       EV_KEY, BTN_LEFT,
			       EV_KEY, BTN_MIDDLE,
			       EV_KEY, BTN_RIGHT,
			       -1);

	uinput_device_event(uidev, EV_ABS, ABS_X, 100);
	uinput_device_event(uidev, EV_ABS, ABS_Y, 500);
	uinput_device_event(uidev, EV_ABS, ABS_MAX, 700);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_FORCE_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_X);
	ck_assert_int_eq(ev.value, 100);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_Y);
	ck_assert_int_eq(ev.value, 500);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_MAX);
	ck_assert_int_eq(ev.value, 700);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_REPORT);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, -EAGAIN);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_syn_delta_mt)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;
	struct input_absinfo abs[6];

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;
	abs[1].value = ABS_MT_POSITION_X;
	abs[1].maximum = 1000;

	abs[2].value = ABS_Y;
	abs[2].maximum = 1000;
	abs[3].value = ABS_MT_POSITION_Y;
	abs[3].maximum = 1000;


	abs[4].value = ABS_MT_SLOT;
	abs[4].maximum = 1;
	abs[5].value = ABS_MT_TRACKING_ID;
	abs[5].minimum = -1;
	abs[5].maximum = 2;

	test_create_abs_device(&uidev, &dev,
			       6, abs,
			       EV_SYN, SYN_REPORT,
			       -1);

	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 0);
	uinput_device_event(uidev, EV_ABS, ABS_X, 100);
	uinput_device_event(uidev, EV_ABS, ABS_Y, 500);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 100);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 500);
	uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, 1);
	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 1);
	uinput_device_event(uidev, EV_ABS, ABS_X, 1);
	uinput_device_event(uidev, EV_ABS, ABS_Y, 5);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 1);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 5);
	uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, 2);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_FORCE_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);

	ck_assert_int_eq(libevdev_get_current_slot(dev), 0);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_X);
	ck_assert_int_eq(ev.value, 1);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_Y);
	ck_assert_int_eq(ev.value, 5);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_MT_SLOT);
	ck_assert_int_eq(ev.value, 0);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_MT_POSITION_X);
	ck_assert_int_eq(ev.value, 100);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_MT_POSITION_Y);
	ck_assert_int_eq(ev.value, 500);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_MT_TRACKING_ID);
	ck_assert_int_eq(ev.value, 1);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_MT_SLOT);
	ck_assert_int_eq(ev.value, 1);
	ck_assert_int_eq(libevdev_get_current_slot(dev), 1);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_MT_POSITION_X);
	ck_assert_int_eq(ev.value, 1);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_MT_POSITION_Y);
	ck_assert_int_eq(ev.value, 5);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_MT_TRACKING_ID);
	ck_assert_int_eq(ev.value, 2);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_REPORT);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, -EAGAIN);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_syn_delta_mt_reset_slot)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev,
			   last_slot_event = { .type = 0};
	struct input_absinfo abs[6];

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;
	abs[1].value = ABS_MT_POSITION_X;
	abs[1].maximum = 1000;

	abs[2].value = ABS_Y;
	abs[2].maximum = 1000;
	abs[3].value = ABS_MT_POSITION_Y;
	abs[3].maximum = 1000;


	abs[4].value = ABS_MT_SLOT;
	abs[4].maximum = 1;
	abs[5].value = ABS_MT_TRACKING_ID;
	abs[5].minimum = -1;
	abs[5].maximum = 2;

	test_create_abs_device(&uidev, &dev,
			       6, abs,
			       EV_SYN, SYN_REPORT,
			       -1);

	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 1);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 100);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 500);
	uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, 1);
	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 0);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 1);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 5);
	uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, 2);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_FORCE_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);

	do {
		rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
		if (libevdev_event_is_code(&ev, EV_ABS, ABS_MT_SLOT))
			last_slot_event = ev;
	} while (rc != -EAGAIN);

	ck_assert(libevdev_event_is_code(&last_slot_event, EV_ABS, ABS_MT_SLOT));
	ck_assert_int_eq(last_slot_event.value, 0);
	ck_assert_int_eq(libevdev_get_current_slot(dev), 0);

	last_slot_event.type = 0;

	/* same thing again, this time swap the numbers */
	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 0);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 100);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 500);
	uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, 1);
	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 1);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 1);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 5);
	uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, 2);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_FORCE_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);

	do {
		rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
		if (libevdev_event_is_code(&ev, EV_ABS, ABS_MT_SLOT))
			last_slot_event = ev;
	} while (rc != -EAGAIN);

	ck_assert(libevdev_event_is_code(&last_slot_event, EV_ABS, ABS_MT_SLOT));
	ck_assert_int_eq(last_slot_event.value, 1);
	ck_assert_int_eq(libevdev_get_current_slot(dev), 1);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_syn_delta_led)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;

	test_create_device(&uidev, &dev,
			   EV_SYN, SYN_REPORT,
			   EV_SYN, SYN_DROPPED,
			   EV_LED, LED_NUML,
			   EV_LED, LED_CAPSL,
			   EV_LED, LED_MAX,
			   -1);

	uinput_device_event(uidev, EV_LED, LED_NUML, 1);
	uinput_device_event(uidev, EV_LED, LED_CAPSL, 1);
	uinput_device_event(uidev, EV_LED, LED_MAX, 1);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_FORCE_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_LED);
	ck_assert_int_eq(ev.code, LED_NUML);
	ck_assert_int_eq(ev.value, 1);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_LED);
	ck_assert_int_eq(ev.code, LED_CAPSL);
	ck_assert_int_eq(ev.value, 1);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_LED);
	ck_assert_int_eq(ev.code, LED_MAX);
	ck_assert_int_eq(ev.value, 1);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_REPORT);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, -EAGAIN);

	ck_assert_int_eq(libevdev_get_event_value(dev, EV_LED, LED_NUML), 1);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_LED, LED_CAPSL), 1);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_LED, LED_MAX), 1);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_syn_delta_sw)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;

	test_create_device(&uidev, &dev,
			   EV_SYN, SYN_REPORT,
			   EV_SYN, SYN_DROPPED,
			   EV_SW, SW_LID,
			   EV_SW, SW_MICROPHONE_INSERT,
			   EV_SW, SW_MAX,
			   -1);

	uinput_device_event(uidev, EV_SW, SW_LID, 1);
	uinput_device_event(uidev, EV_SW, SW_MICROPHONE_INSERT, 1);
	uinput_device_event(uidev, EV_SW, SW_MAX, 1);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_FORCE_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_SW);
	ck_assert_int_eq(ev.code, SW_LID);
	ck_assert_int_eq(ev.value, 1);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_SW);
	ck_assert_int_eq(ev.code, SW_MICROPHONE_INSERT);
	ck_assert_int_eq(ev.value, 1);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_SW);
	ck_assert_int_eq(ev.code, SW_MAX);
	ck_assert_int_eq(ev.value, 1);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_REPORT);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, -EAGAIN);

	ck_assert_int_eq(libevdev_get_event_value(dev, EV_SW, SW_LID), 1);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_SW, SW_MICROPHONE_INSERT), 1);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_SW, SW_MAX), 1);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_syn_delta_tracking_ids)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;
	struct input_absinfo abs[6];
	int i;
	const int num_slots = 15;
	int slot = -1;
	unsigned long terminated[NLONGS(num_slots)];
	unsigned long restarted[NLONGS(num_slots)];

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;
	abs[1].value = ABS_MT_POSITION_X;
	abs[1].maximum = 1000;

	abs[2].value = ABS_Y;
	abs[2].maximum = 1000;
	abs[3].value = ABS_MT_POSITION_Y;
	abs[3].maximum = 1000;

	abs[4].value = ABS_MT_SLOT;
	abs[4].maximum = num_slots;

	abs[5].minimum = -1;
	abs[5].maximum = 255;
	abs[5].value = ABS_MT_TRACKING_ID;

	test_create_abs_device(&uidev, &dev,
			       6, abs,
			       EV_SYN, SYN_REPORT,
			       -1);

	/* Test the sync process to make sure we get touches terminated when
	 * the tracking id changes:
	 * 1) start a bunch of touch points
	 * 2) read data into libevdev, make sure state is up-to-date
	 * 3) change touchpoints
	 * 3.1) change the tracking ID on some (indicating terminated and
	 * re-started touchpoint)
	 * 3.2) change the tracking ID to -1 on some (indicating termianted
	 * touchpoint)
	 * 3.3) just update the data on others
	 * 4) force a sync on the device
	 * 5) make sure we get the right tracking ID changes in the caller
	 */

	/* Start a bunch of touch points  */
	for (i = num_slots; i >= 0; i--) {
		uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, i);
		uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, i);
		uinput_device_event(uidev, EV_ABS, ABS_X, 100 + i);
		uinput_device_event(uidev, EV_ABS, ABS_Y, 500 + i);
		uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 100 + i);
		uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 500 + i);
		uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
		do {
			rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
			ck_assert_int_ne(rc, LIBEVDEV_READ_STATUS_SYNC);
		} while (rc >= 0);
	}

	/* we have a bunch of touches now, and libevdev knows it. Change all
	 * touches */
	for (i = num_slots; i >= 0; i--) {
		uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, i);
		if (i % 3 == 0) {
			/* change some slots with a new tracking id */
			uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, num_slots + i);
			uinput_device_event(uidev, EV_ABS, ABS_X, 200 + i);
			uinput_device_event(uidev, EV_ABS, ABS_Y, 700 + i);
			uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 200 + i);
			uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 700 + i);
		} else if (i % 3 == 1) {
			/* stop others */
			uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, -1);
		} else {
			/* just update */
			uinput_device_event(uidev, EV_ABS, ABS_X, 200 + i);
			uinput_device_event(uidev, EV_ABS, ABS_Y, 700 + i);
			uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 200 + i);
			uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 700 + i);
		}
		uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	}

	/* Force sync */
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_FORCE_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);

	/* now check for the right tracking IDs */
	memset(terminated, 0, sizeof(terminated));
	memset(restarted, 0, sizeof(restarted));
	slot = -1;
	while ((rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev)) != -EAGAIN) {
		if (libevdev_event_is_code(&ev, EV_SYN, SYN_REPORT))
			continue;

		if (libevdev_event_is_code(&ev, EV_ABS, ABS_MT_SLOT)) {
			slot = ev.value;
			continue;
		}

		if (libevdev_event_is_code(&ev, EV_ABS, ABS_X) ||
		    libevdev_event_is_code(&ev, EV_ABS, ABS_Y))
			continue;

		ck_assert_int_ne(slot, -1);

		if (libevdev_event_is_code(&ev, EV_ABS, ABS_MT_TRACKING_ID)) {
			if (slot % 3 == 0) {
				if (!bit_is_set(terminated, slot)) {
					ck_assert_int_eq(ev.value, -1);
					set_bit(terminated, slot);
				} else {
					ck_assert_int_eq(ev.value, num_slots + slot);
					set_bit(restarted, slot);
				}
			} else if (slot % 3 == 1) {
				ck_assert(!bit_is_set(terminated, slot));
				ck_assert_int_eq(ev.value, -1);
				set_bit(terminated, slot);
			} else
				ck_abort();

			continue;
		}

		switch(ev.code) {
			case ABS_MT_POSITION_X:
				ck_assert_int_eq(ev.value, 200 + slot);
				break;
			case ABS_MT_POSITION_Y:
				ck_assert_int_eq(ev.value, 700 + slot);
				break;
			default:
				ck_abort();
		}
	}

	for (i = 0; i < num_slots; i++) {
		if (i % 3 == 0) {
			ck_assert(bit_is_set(terminated, i));
			ck_assert(bit_is_set(restarted, i));
		} else if (i % 3 == 1) {
			ck_assert(bit_is_set(terminated, i));
			ck_assert(!bit_is_set(restarted, i));
		} else {
			ck_assert(!bit_is_set(terminated, i));
			ck_assert(!bit_is_set(restarted, i));
		}
	}

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_syn_delta_late_sync)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;
	struct input_absinfo abs[6];
	int i, slot;

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;
	abs[1].value = ABS_MT_POSITION_X;
	abs[1].maximum = 1000;

	abs[2].value = ABS_Y;
	abs[2].maximum = 1000;
	abs[3].value = ABS_MT_POSITION_Y;
	abs[3].maximum = 1000;

	abs[4].value = ABS_MT_SLOT;
	abs[4].maximum = 1;

	abs[5].minimum = -1;
	abs[5].maximum = 255;
	abs[5].value = ABS_MT_TRACKING_ID;

	test_create_abs_device(&uidev, &dev,
			       6, abs,
			       EV_SYN, SYN_REPORT,
			       -1);

	/* emulate a touch down, make sure libevdev sees it */
	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 0);
	uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, 1);
	uinput_device_event(uidev, EV_ABS, ABS_X, 100);
	uinput_device_event(uidev, EV_ABS, ABS_Y, 500);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 100);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 500);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	do {
		rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		ck_assert_int_ne(rc, LIBEVDEV_READ_STATUS_SYNC);
	} while (rc >= 0);

	/* force enough events to trigger a SYN_DROPPED */
	for (i = 0; i < 100; i++) {
		uinput_device_event(uidev, EV_ABS, ABS_X, 100 + i);
		uinput_device_event(uidev, EV_ABS, ABS_Y, 500 + i);
		uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 100 + i);
		uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 500 + i);
		uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	}

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);

	/* trigger the tracking ID change after getting the SYN_DROPPED */
	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 0);
	uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, -1);
	uinput_device_event(uidev, EV_ABS, ABS_X, 200);
	uinput_device_event(uidev, EV_ABS, ABS_Y, 600);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 200);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 600);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);

	slot = 0;

	/* Now sync the device, expect the data to be equal to the last event*/
	while ((rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev)) != -EAGAIN) {
		if (ev.type == EV_SYN)
			continue;

		ck_assert_int_eq(ev.type, EV_ABS);
		switch(ev.code) {
			case ABS_MT_SLOT:
				slot = ev.value;
				break;
			case ABS_MT_TRACKING_ID:
				if (slot == 0)
					ck_assert_int_eq(ev.value, -1);
				break;
			case ABS_X:
			case ABS_MT_POSITION_X:
				ck_assert_int_eq(ev.value, 200);
				break;
			case ABS_Y:
			case ABS_MT_POSITION_Y:
				ck_assert_int_eq(ev.value, 600);
				break;
		}
	}

	/* And a new tracking ID */
	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 0);
	uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, 2);
	uinput_device_event(uidev, EV_ABS, ABS_X, 201);
	uinput_device_event(uidev, EV_ABS, ABS_Y, 601);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 201);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 601);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);

	while ((rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)) != -EAGAIN) {
		ck_assert_int_ne(rc, LIBEVDEV_READ_STATUS_SYNC);

		if (ev.type == EV_SYN)
			continue;

		ck_assert_int_eq(ev.type, EV_ABS);

		switch(ev.code) {
			case ABS_MT_SLOT:
				ck_assert_int_eq(ev.value, 0);
				break;
			case ABS_MT_TRACKING_ID:
				ck_assert_int_eq(ev.value, 2);
				break;
			case ABS_X:
			case ABS_MT_POSITION_X:
				ck_assert_int_eq(ev.value, 201);
				break;
			case ABS_Y:
			case ABS_MT_POSITION_Y:
				ck_assert_int_eq(ev.value, 601);
				break;
		}
	}


	/* Now we basically re-do the exact same test, just with the
	   tracking ID order inverted */

	/* drop the tracking ID, make sure libevdev sees it */
	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 0);
	uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, -1);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	do {
		rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
		ck_assert_int_ne(rc, LIBEVDEV_READ_STATUS_SYNC);
	} while (rc >= 0);

	/* force enough events to trigger a SYN_DROPPED */
	for (i = 0; i < 100; i++) {
		uinput_device_event(uidev, EV_ABS, ABS_X, 100 + i);
		uinput_device_event(uidev, EV_ABS, ABS_Y, 500 + i);
		uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 100 + i);
		uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 500 + i);
		uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	}

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);

	/* trigger the new tracking ID after getting the SYN_DROPPED */
	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 0);
	uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, 5);
	uinput_device_event(uidev, EV_ABS, ABS_X, 200);
	uinput_device_event(uidev, EV_ABS, ABS_Y, 600);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 200);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 600);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);

	slot = 0;

	/* Now sync the device, expect the data to be equal to the last event*/
	while ((rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev)) != -EAGAIN) {
		if (ev.type == EV_SYN)
			continue;

		ck_assert_int_eq(ev.type, EV_ABS);
		switch(ev.code) {
			case ABS_MT_SLOT:
				slot = ev.value;
				break;
			case ABS_MT_TRACKING_ID:
				if (slot == 0)
					ck_assert_int_eq(ev.value, 5);
				break;
			case ABS_X:
			case ABS_MT_POSITION_X:
				ck_assert_int_eq(ev.value, 200);
				break;
			case ABS_Y:
			case ABS_MT_POSITION_Y:
				ck_assert_int_eq(ev.value, 600);
				break;
		}
	}

	/* Drop the tracking ID */
	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 0);
	uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, -1);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);

	while ((rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev)) != -EAGAIN) {
		ck_assert_int_ne(rc, LIBEVDEV_READ_STATUS_SYNC);

		if (ev.type == EV_SYN)
			continue;

		ck_assert_int_eq(ev.type, EV_ABS);

		switch(ev.code) {
			case ABS_MT_SLOT:
				ck_assert_int_eq(ev.value, 0);
				break;
			case ABS_MT_TRACKING_ID:
				ck_assert_int_eq(ev.value, -1);
				break;
		}
	}


	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_syn_delta_fake_mt)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;
	struct input_absinfo abs[] = {  { ABS_X, 0, 1000 },
		{ ABS_Y, 0, 1000 },
		{ ABS_MT_POSITION_X, 0, 1000 },
		{ ABS_MT_POSITION_Y, 0, 1000 },
		{ ABS_MT_SLOT - 1, 0, 2 }};
		/* don't set ABS_MT_SLOT here, otherwise uinput will init
		 * slots and the behavior is different to real devices with
		 * such events */
	unsigned long received[NLONGS(ABS_CNT)] = {0};

	test_create_abs_device(&uidev, &dev, 5, abs,
			       -1);
	/* first set of events */
	uinput_device_event(uidev, EV_ABS, ABS_X, 200);
	uinput_device_event(uidev, EV_ABS, ABS_Y, 400);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 100);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 500);
	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT - 1, 1);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);

	/* second set of events */
	uinput_device_event(uidev, EV_ABS, ABS_X, 201);
	uinput_device_event(uidev, EV_ABS, ABS_Y, 401);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 101);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 501);
	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT - 1, 2);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_FORCE_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);

	while ((rc = libevdev_next_event(dev, LIBEVDEV_READ_STATUS_SYNC, &ev)) != -EAGAIN) {
		if (ev.type != EV_ABS)
			continue;

		ck_assert(!bit_is_set(received, ev.code));

		switch(ev.code) {
			/* see comment below for ABS_MT_POSITION_X
			 * and ABS_MT_POSITION_Y */
			case ABS_MT_POSITION_X:
			case ABS_MT_POSITION_Y:
				ck_abort();
				break;

			case ABS_MT_SLOT - 1: ck_assert_int_eq(ev.value, 2); break;
			case ABS_X: ck_assert_int_eq(ev.value, 201); break;
			case ABS_Y: ck_assert_int_eq(ev.value, 401); break;
			default:
				ck_abort();
		}

		set_bit(received, ev.code);
	}

	/* Dont' expect ABS_MT values, they are ignored during the sync
	 * process */
	ck_assert(!bit_is_set(received, ABS_MT_POSITION_X));
	ck_assert(!bit_is_set(received, ABS_MT_POSITION_Y));
	ck_assert(bit_is_set(received, ABS_MT_SLOT - 1));
	ck_assert(bit_is_set(received, ABS_X));
	ck_assert(bit_is_set(received, ABS_Y));

	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_X), 201);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_Y), 401);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_MT_SLOT - 1), 2);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_skipped_sync)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;
	struct input_absinfo abs[2];

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;

	abs[1].value = ABS_Y;
	abs[1].maximum = 1000;

	test_create_abs_device(&uidev, &dev,
			       2, abs,
			       EV_SYN, SYN_REPORT,
			       EV_SYN, SYN_DROPPED,
			       EV_KEY, BTN_LEFT,
			       EV_KEY, BTN_MIDDLE,
			       EV_KEY, BTN_RIGHT,
			       -1);

	uinput_device_event(uidev, EV_KEY, BTN_LEFT, 1);
	uinput_device_event(uidev, EV_ABS, ABS_X, 100);
	uinput_device_event(uidev, EV_ABS, ABS_Y, 500);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_FORCE_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, -EAGAIN);

	ck_assert_int_eq(libevdev_get_event_value(dev, EV_KEY, BTN_LEFT), 1);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_X), 100);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_Y), 500);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_incomplete_sync)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;
	struct input_absinfo abs[2];

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;

	abs[1].value = ABS_Y;
	abs[1].maximum = 1000;

	test_create_abs_device(&uidev, &dev,
			       2, abs,
			       EV_SYN, SYN_REPORT,
			       EV_SYN, SYN_DROPPED,
			       EV_KEY, BTN_LEFT,
			       EV_KEY, BTN_MIDDLE,
			       EV_KEY, BTN_RIGHT,
			       -1);

	uinput_device_event(uidev, EV_KEY, BTN_LEFT, 1);
	uinput_device_event(uidev, EV_ABS, ABS_X, 100);
	uinput_device_event(uidev, EV_ABS, ABS_Y, 500);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_FORCE_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);
	ck_assert_int_eq(ev.type, EV_KEY);
	ck_assert_int_eq(ev.code, BTN_LEFT);
	ck_assert_int_eq(ev.value, 1);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, -EAGAIN);

	ck_assert_int_eq(libevdev_get_event_value(dev, EV_KEY, BTN_LEFT), 1);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_X), 100);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_Y), 500);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_empty_sync)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;

	test_create_device(&uidev, &dev,
			   EV_SYN, SYN_REPORT,
			   EV_SYN, SYN_DROPPED,
			   EV_KEY, BTN_LEFT,
			   EV_KEY, BTN_MIDDLE,
			   EV_KEY, BTN_RIGHT,
			   -1);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_FORCE_SYNC, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SYNC);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_SYNC, &ev);
	ck_assert_int_eq(rc, -EAGAIN);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_event_values)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;
	struct input_absinfo abs[2];
	int value;

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;

	abs[1].value = ABS_Y;
	abs[1].maximum = 1000;

	test_create_abs_device(&uidev, &dev,
			       2, abs,
			       EV_SYN, SYN_REPORT,
			       EV_SYN, SYN_DROPPED,
			       EV_REL, REL_X,
			       EV_REL, REL_Y,
			       EV_KEY, BTN_LEFT,
			       EV_KEY, BTN_MIDDLE,
			       EV_KEY, BTN_RIGHT,
			       -1);

	uinput_device_event(uidev, EV_KEY, BTN_LEFT, 1);
	uinput_device_event(uidev, EV_ABS, ABS_X, 100);
	uinput_device_event(uidev, EV_ABS, ABS_Y, 500);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);

	/* must still be on old values */
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_KEY, BTN_LEFT), 0);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_X), 0);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_Y), 0);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_REL, REL_X), 0);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_REL, REL_Y), 0);

	ck_assert_int_eq(libevdev_fetch_event_value(dev, EV_KEY, BTN_LEFT, &value), 1);
	ck_assert_int_eq(value, 0);

	do {
		rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	} while (rc == 0);
	ck_assert_int_eq(rc, -EAGAIN);

	ck_assert_int_eq(libevdev_get_event_value(dev, EV_KEY, BTN_LEFT), 1);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_X), 100);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_Y), 500);

	/* always 0 */
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_REL, REL_X), 0);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_REL, REL_Y), 0);

	ck_assert_int_eq(libevdev_fetch_event_value(dev, EV_KEY, BTN_LEFT, &value), 1);
	ck_assert_int_eq(value, 1);
	ck_assert_int_eq(libevdev_fetch_event_value(dev, EV_ABS, ABS_X, &value), 1);
	ck_assert_int_eq(value, 100);
	ck_assert_int_eq(libevdev_fetch_event_value(dev, EV_ABS, ABS_Y, &value), 1);
	ck_assert_int_eq(value, 500);

	uinput_device_free(uidev);
	libevdev_free(dev);

}
END_TEST

START_TEST(test_event_values_invalid)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	struct input_absinfo abs[2];
	int value;

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;

	abs[1].value = ABS_Y;
	abs[1].maximum = 1000;

	test_create_abs_device(&uidev, &dev,
			       2, abs,
			       EV_SYN, SYN_REPORT,
			       EV_SYN, SYN_DROPPED,
			       EV_REL, REL_X,
			       EV_REL, REL_Y,
			       EV_KEY, BTN_LEFT,
			       EV_KEY, BTN_MIDDLE,
			       EV_KEY, BTN_RIGHT,
			       -1);

	ck_assert_int_eq(libevdev_get_event_value(dev, EV_KEY, BTN_EXTRA), 0);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_Z), 0);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_REL, REL_Z), 0);

	value = 0xab;
	ck_assert_int_eq(libevdev_fetch_event_value(dev, EV_KEY, BTN_EXTRA, &value), 0);
	ck_assert_int_eq(value, 0xab);
	ck_assert_int_eq(libevdev_fetch_event_value(dev, EV_ABS, ABS_Z, &value), 0);
	ck_assert_int_eq(value, 0xab);
	ck_assert_int_eq(libevdev_fetch_event_value(dev, EV_REL, REL_Z, &value), 0);
	ck_assert_int_eq(value, 0xab);


	uinput_device_free(uidev);
	libevdev_free(dev);

}
END_TEST

START_TEST(test_mt_event_values)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;
	struct input_absinfo abs[5];
	int value;

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;
	abs[1].value = ABS_MT_POSITION_X;
	abs[1].maximum = 1000;

	abs[2].value = ABS_Y;
	abs[2].maximum = 1000;
	abs[3].value = ABS_MT_POSITION_Y;
	abs[3].maximum = 1000;

	abs[4].value = ABS_MT_SLOT;
	abs[4].maximum = 2;

	test_create_abs_device(&uidev, &dev,
			       5, abs,
			       EV_SYN, SYN_REPORT,
			       -1);

	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 0);
	uinput_device_event(uidev, EV_ABS, ABS_X, 100);
	uinput_device_event(uidev, EV_ABS, ABS_Y, 500);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 100);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 500);
	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 1);
	uinput_device_event(uidev, EV_ABS, ABS_X, 1);
	uinput_device_event(uidev, EV_ABS, ABS_Y, 5);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_X, 1);
	uinput_device_event(uidev, EV_ABS, ABS_MT_POSITION_Y, 5);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);

	/* must still be on old values */
	ck_assert_int_eq(libevdev_get_current_slot(dev), 0);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 0, ABS_MT_POSITION_X), 0);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 0, ABS_MT_POSITION_Y), 0);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 1, ABS_MT_POSITION_X), 0);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 1, ABS_MT_POSITION_Y), 0);

	do {
		rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	} while (rc == LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(rc, -EAGAIN);

	ck_assert_int_eq(libevdev_get_current_slot(dev), 1);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 0, ABS_MT_POSITION_X), 100);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 0, ABS_MT_POSITION_Y), 500);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 1, ABS_MT_POSITION_X), 1);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 1, ABS_MT_POSITION_Y), 5);

	ck_assert_int_eq(libevdev_fetch_slot_value(dev, 0, ABS_MT_POSITION_X, &value), 1);
	ck_assert_int_eq(value, 100);
	ck_assert_int_eq(libevdev_fetch_slot_value(dev, 0, ABS_MT_POSITION_Y, &value), 1);
	ck_assert_int_eq(value, 500);
	ck_assert_int_eq(libevdev_fetch_slot_value(dev, 1, ABS_MT_POSITION_X, &value), 1);
	ck_assert_int_eq(value, 1);
	ck_assert_int_eq(libevdev_fetch_slot_value(dev, 1, ABS_MT_POSITION_Y, &value), 1);
	ck_assert_int_eq(value, 5);

	uinput_device_free(uidev);
	libevdev_free(dev);

}
END_TEST

START_TEST(test_mt_event_values_invalid)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	struct input_absinfo abs[5];
	int value;

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;
	abs[1].value = ABS_MT_POSITION_X;
	abs[1].maximum = 1000;

	abs[2].value = ABS_Y;
	abs[2].maximum = 1000;
	abs[3].value = ABS_MT_POSITION_Y;
	abs[3].maximum = 1000;

	abs[4].value = ABS_MT_SLOT;
	abs[4].maximum = 2;

	test_create_abs_device(&uidev, &dev,
			       5, abs,
			       EV_SYN, SYN_REPORT,
			       -1);

	ck_assert_int_eq(libevdev_get_current_slot(dev), 0);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 0, ABS_MT_TOUCH_MINOR), 0);
	value = 0xab;
	ck_assert_int_eq(libevdev_fetch_slot_value(dev, 0, ABS_MT_TOUCH_MINOR, &value), 0);
	ck_assert_int_eq(value, 0xab);

	ck_assert_int_eq(libevdev_get_slot_value(dev, 10, ABS_MT_POSITION_X), 0);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 0, ABS_X), 0);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_mt_slot_ranges_invalid)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	struct input_event ev[2];
	int rc;
	struct input_absinfo abs[5];
	int num_slots = 2;
	int pipefd[2];

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;
	abs[1].value = ABS_MT_POSITION_X;
	abs[1].maximum = 1000;

	abs[2].value = ABS_Y;
	abs[2].maximum = 1000;
	abs[3].value = ABS_MT_POSITION_Y;
	abs[3].maximum = 1000;

	abs[4].value = ABS_MT_SLOT;
	abs[4].maximum = num_slots - 1;

	test_create_abs_device(&uidev, &dev,
			       5, abs,
			       EV_SYN, SYN_REPORT,
			       -1);

	rc = pipe2(pipefd, O_NONBLOCK);
	ck_assert_int_eq(rc, 0);
	libevdev_change_fd(dev, pipefd[0]);

	memset(ev, 0, sizeof(ev));
	ev[0].type = EV_ABS;
	ev[0].code = ABS_MT_SLOT;
	ev[0].value = num_slots;
	ev[1].type = EV_SYN;
	ev[1].code = SYN_REPORT;
	ev[1].value = 0;
	rc = write(pipefd[1], ev, sizeof(ev));
	ck_assert_int_eq(rc, sizeof(ev));

	libevdev_set_log_function(test_logfunc_ignore_error, NULL);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, ev);
	ck_assert(libevdev_event_is_code(ev, EV_ABS, ABS_MT_SLOT));
	ck_assert_int_eq(ev[0].value, num_slots - 1);

	/* drain the EV_SYN */
	libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, ev);

	ev[0].type = EV_ABS;
	ev[0].code = ABS_MT_SLOT;
	ev[0].value = -1;
	ev[1].type = EV_SYN;
	ev[1].code = SYN_REPORT;
	ev[1].value = 0;
	rc = write(pipefd[1], ev, sizeof(ev));
	ck_assert_int_eq(rc, sizeof(ev));

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, ev);
	ck_assert(libevdev_event_is_code(ev, EV_ABS, ABS_MT_SLOT));
	ck_assert_int_eq(ev[0].value, num_slots - 1);

	ck_assert_int_eq(libevdev_get_current_slot(dev), num_slots - 1);

	ck_assert_int_eq(libevdev_set_event_value(dev, EV_ABS, ABS_MT_SLOT, num_slots), -1);
	ck_assert_int_eq(libevdev_set_event_value(dev, EV_ABS, ABS_MT_SLOT, -1), -1);

	libevdev_set_log_function(test_logfunc_abort_on_error, NULL);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_mt_tracking_id_discard)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;
	struct input_absinfo abs[6];

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;
	abs[1].value = ABS_MT_POSITION_X;
	abs[1].maximum = 1000;

	abs[2].value = ABS_Y;
	abs[2].maximum = 1000;
	abs[3].value = ABS_MT_POSITION_Y;
	abs[3].maximum = 1000;

	abs[4].value = ABS_MT_SLOT;
	abs[4].maximum = 10;
	abs[5].value = ABS_MT_TRACKING_ID;
	abs[5].maximum = 500;

	rc = test_create_abs_device(&uidev, &dev,
				    6, abs,
				    EV_SYN, SYN_REPORT,
				    -1);

	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 1);
	uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, 1);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);

	/* second tracking ID on same slot */
	uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, 2);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);

	libevdev_set_log_function(test_logfunc_ignore_error, NULL);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_MT_SLOT);
	ck_assert_int_eq(ev.value, 1);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_MT_TRACKING_ID);
	ck_assert_int_eq(ev.value, 1);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_REPORT);
	ck_assert_int_eq(ev.value, 0);

	/* expect tracking ID discarded */

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_REPORT);
	ck_assert_int_eq(ev.value, 0);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, -EAGAIN);

	libevdev_set_log_function(test_logfunc_abort_on_error, NULL);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_mt_tracking_id_discard_neg_1)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int rc;
	struct input_event ev;
	struct input_absinfo abs[6];
	int pipefd[2];
	struct input_event events[] = {
		{ .type = EV_ABS, .code = ABS_MT_TRACKING_ID, .value = -1 },
		{ .type = EV_SYN, .code = SYN_REPORT, .value = 0 },
	};

	rc = pipe2(pipefd, O_NONBLOCK);
	ck_assert_int_eq(rc, 0);

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;
	abs[1].value = ABS_MT_POSITION_X;
	abs[1].maximum = 1000;

	abs[2].value = ABS_Y;
	abs[2].maximum = 1000;
	abs[3].value = ABS_MT_POSITION_Y;
	abs[3].maximum = 1000;

	abs[4].value = ABS_MT_SLOT;
	abs[4].maximum = 10;
	abs[5].value = ABS_MT_TRACKING_ID;
	abs[5].maximum = 500;

	rc = test_create_abs_device(&uidev, &dev,
				    6, abs,
				    EV_SYN, SYN_REPORT,
				    -1);
	uinput_device_event(uidev, EV_ABS, ABS_MT_SLOT, 1);
	uinput_device_event(uidev, EV_ABS, ABS_MT_TRACKING_ID, 1);
	uinput_device_event(uidev, EV_SYN, SYN_REPORT, 0);

	while (libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev) != -EAGAIN)
		;

	libevdev_set_log_function(test_logfunc_ignore_error, NULL);

	/* two -1 tracking ids, need to use the pipe here, the kernel will
	   filter it otherwise */
	libevdev_change_fd(dev, pipefd[0]);

	rc = write(pipefd[1], events, sizeof(events));
	ck_assert_int_eq(rc, sizeof(events));
	rc = write(pipefd[1], events, sizeof(events));
	ck_assert_int_eq(rc, sizeof(events));

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(ev.type, EV_ABS);
	ck_assert_int_eq(ev.code, ABS_MT_TRACKING_ID);
	ck_assert_int_eq(ev.value, -1);
	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_REPORT);
	ck_assert_int_eq(ev.value, 0);

	/* expect second tracking ID discarded */

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, LIBEVDEV_READ_STATUS_SUCCESS);
	ck_assert_int_eq(ev.type, EV_SYN);
	ck_assert_int_eq(ev.code, SYN_REPORT);
	ck_assert_int_eq(ev.value, 0);

	rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
	ck_assert_int_eq(rc, -EAGAIN);

	libevdev_set_log_function(test_logfunc_abort_on_error, NULL);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_ev_rep_values)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	int delay = 500, period = 200;
	test_create_device(&uidev, &dev,
			   EV_KEY, BTN_LEFT,
			   EV_REL, REL_X,
			   EV_REL, REL_Y,
			   EV_SYN, SYN_REPORT,
			   -1);

	libevdev_enable_event_code(dev, EV_REP, REP_DELAY, &delay);
	libevdev_enable_event_code(dev, EV_REP, REP_PERIOD, &period);

	ck_assert_int_eq(libevdev_has_event_type(dev, EV_REP), 1);
	ck_assert_int_eq(libevdev_has_event_code(dev, EV_REP, REP_DELAY), 1);
	ck_assert_int_eq(libevdev_has_event_code(dev, EV_REP, REP_PERIOD), 1);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_REP, REP_DELAY), 500);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_REP, REP_PERIOD), 200);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_event_value_setters)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	struct input_absinfo abs[2];

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;

	abs[1].value = ABS_Y;
	abs[1].maximum = 1000;

	test_create_abs_device(&uidev, &dev,
			       2, abs,
			       EV_SYN, SYN_REPORT,
			       EV_REL, REL_X,
			       EV_REL, REL_Y,
			       EV_KEY, BTN_LEFT,
			       EV_KEY, BTN_MIDDLE,
			       EV_KEY, BTN_RIGHT,
			       EV_LED, LED_NUML,
			       EV_LED, LED_CAPSL,
			       EV_SW, SW_LID,
			       EV_SW, SW_TABLET_MODE,
			       -1);

	ck_assert_int_eq(libevdev_get_event_value(dev, EV_KEY, BTN_LEFT), 0);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_X), 0);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_Y), 0);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_REL, REL_X), 0);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_REL, REL_Y), 0);

	ck_assert_int_eq(libevdev_set_event_value(dev, EV_KEY, BTN_LEFT, 1), 0);
	ck_assert_int_eq(libevdev_set_event_value(dev, EV_KEY, BTN_RIGHT, 1), 0);

	ck_assert_int_eq(libevdev_get_event_value(dev, EV_KEY, BTN_LEFT), 1);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_KEY, BTN_RIGHT), 1);

	ck_assert_int_eq(libevdev_set_event_value(dev, EV_ABS, ABS_X, 10), 0);
	ck_assert_int_eq(libevdev_set_event_value(dev, EV_ABS, ABS_Y, 20), 0);

	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_X), 10);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_Y), 20);

	ck_assert_int_eq(libevdev_set_event_value(dev, EV_LED, LED_NUML, 1), 0);
	ck_assert_int_eq(libevdev_set_event_value(dev, EV_LED, LED_CAPSL, 1), 0);

	ck_assert_int_eq(libevdev_get_event_value(dev, EV_LED, LED_NUML), 1);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_LED, LED_CAPSL), 1);

	ck_assert_int_eq(libevdev_set_event_value(dev, EV_SW, SW_LID, 1), 0);
	ck_assert_int_eq(libevdev_set_event_value(dev, EV_SW, SW_TABLET_MODE, 1), 0);

	ck_assert_int_eq(libevdev_get_event_value(dev, EV_SW, SW_LID), 1);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_SW, SW_TABLET_MODE), 1);

	uinput_device_free(uidev);
	libevdev_free(dev);

}
END_TEST

START_TEST(test_event_value_setters_invalid)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	struct input_absinfo abs[2];

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;

	abs[1].value = ABS_Y;
	abs[1].maximum = 1000;

	test_create_abs_device(&uidev, &dev,
			       2, abs,
			       EV_SYN, SYN_REPORT,
			       EV_REL, REL_X,
			       EV_REL, REL_Y,
			       EV_KEY, BTN_LEFT,
			       EV_KEY, BTN_MIDDLE,
			       EV_KEY, BTN_RIGHT,
			       -1);

	ck_assert_int_eq(libevdev_set_event_value(dev, EV_REL, REL_X, 1), -1);
	ck_assert_int_eq(libevdev_set_event_value(dev, EV_SW, SW_DOCK, 1), -1);
	ck_assert_int_eq(libevdev_set_event_value(dev, EV_ABS, ABS_Z, 1), -1);
	ck_assert_int_eq(libevdev_set_event_value(dev, EV_MAX + 1, 0, 1), -1);
	ck_assert_int_eq(libevdev_set_event_value(dev, EV_SYN, SYN_REPORT, 0), -1);

	uinput_device_free(uidev);
	libevdev_free(dev);

}
END_TEST

START_TEST(test_event_mt_value_setters)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	struct input_absinfo abs[5];

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;
	abs[1].value = ABS_MT_POSITION_X;
	abs[1].maximum = 1000;

	abs[2].value = ABS_Y;
	abs[2].maximum = 1000;
	abs[3].value = ABS_MT_POSITION_Y;
	abs[3].maximum = 1000;

	abs[4].value = ABS_MT_SLOT;
	abs[4].maximum = 2;

	test_create_abs_device(&uidev, &dev,
			       5, abs,
			       EV_SYN, SYN_REPORT,
			       -1);

	ck_assert_int_eq(libevdev_set_slot_value(dev, 1, ABS_MT_POSITION_X, 1), 0);
	ck_assert_int_eq(libevdev_set_slot_value(dev, 1, ABS_MT_POSITION_Y, 2), 0);
	ck_assert_int_eq(libevdev_set_slot_value(dev, 0, ABS_MT_POSITION_X, 3), 0);
	ck_assert_int_eq(libevdev_set_slot_value(dev, 0, ABS_MT_POSITION_Y, 4), 0);

	ck_assert_int_eq(libevdev_get_slot_value(dev, 1, ABS_MT_POSITION_X), 1);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 1, ABS_MT_POSITION_Y), 2);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 0, ABS_MT_POSITION_X), 3);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 0, ABS_MT_POSITION_Y), 4);

	ck_assert_int_eq(libevdev_set_slot_value(dev, 1, ABS_MT_SLOT, 1), 0);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 1, ABS_MT_SLOT), 1);
	ck_assert_int_eq(libevdev_get_current_slot(dev), 1);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_event_mt_value_setters_invalid)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	struct input_absinfo abs[5];

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;
	abs[1].value = ABS_MT_POSITION_X;
	abs[1].maximum = 1000;

	abs[2].value = ABS_Y;
	abs[2].maximum = 1000;
	abs[3].value = ABS_MT_POSITION_Y;
	abs[3].maximum = 1000;

	abs[4].value = ABS_MT_SLOT;
	abs[4].maximum = 2;

	test_create_abs_device(&uidev, &dev,
			       5, abs,
			       EV_SYN, SYN_REPORT,
			       -1);

	/* invalid axis */
	ck_assert_int_eq(libevdev_set_slot_value(dev, 1, ABS_Z, 1), -1);
	/* valid, but non-mt axis */
	ck_assert_int_eq(libevdev_set_slot_value(dev, 1, ABS_X, 1), -1);
	/* invalid mt axis */
	ck_assert_int_eq(libevdev_set_slot_value(dev, 1, ABS_MT_PRESSURE, 1), -1);
	/* invalid slot no */
	ck_assert_int_eq(libevdev_set_slot_value(dev, 4, ABS_X, 1), -1);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

START_TEST(test_event_mt_value_setters_current_slot)
{
	struct uinput_device* uidev;
	struct libevdev *dev;
	struct input_absinfo abs[5];

	memset(abs, 0, sizeof(abs));
	abs[0].value = ABS_X;
	abs[0].maximum = 1000;
	abs[1].value = ABS_MT_POSITION_X;
	abs[1].maximum = 1000;

	abs[2].value = ABS_Y;
	abs[2].maximum = 1000;
	abs[3].value = ABS_MT_POSITION_Y;
	abs[3].maximum = 1000;

	abs[4].value = ABS_MT_SLOT;
	abs[4].maximum = 2;

	test_create_abs_device(&uidev, &dev,
			       5, abs,
			       EV_SYN, SYN_REPORT,
			       -1);

	/* set_event_value/get_event_value works on the current slot */

	ck_assert_int_eq(libevdev_get_current_slot(dev), 0);
	ck_assert_int_eq(libevdev_set_event_value(dev, EV_ABS, ABS_MT_POSITION_X, 1), 0);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_MT_POSITION_X), 1);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 0, ABS_MT_POSITION_X), 1);

	ck_assert_int_eq(libevdev_set_event_value(dev, EV_ABS, ABS_MT_SLOT, 1), 0);
	ck_assert_int_eq(libevdev_get_current_slot(dev), 1);
	ck_assert_int_eq(libevdev_set_event_value(dev, EV_ABS, ABS_MT_POSITION_X, 2), 0);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_MT_POSITION_X), 2);
	ck_assert_int_eq(libevdev_get_slot_value(dev, 1, ABS_MT_POSITION_X), 2);

	/* set slot 0, but current is still slot 1 */
	ck_assert_int_eq(libevdev_set_slot_value(dev, 0, ABS_MT_POSITION_X, 3), 0);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_MT_POSITION_X), 2);

	ck_assert_int_eq(libevdev_set_event_value(dev, EV_ABS, ABS_MT_SLOT, 0), 0);
	ck_assert_int_eq(libevdev_get_current_slot(dev), 0);
	ck_assert_int_eq(libevdev_get_event_value(dev, EV_ABS, ABS_MT_POSITION_X), 3);

	uinput_device_free(uidev);
	libevdev_free(dev);
}
END_TEST

Suite *
libevdev_events(void)
{
	Suite *s = suite_create("libevdev event tests");

	TCase *tc = tcase_create("event polling");
	tcase_add_test(tc, test_next_event);
	tcase_add_test(tc, test_syn_dropped_event);
	tcase_add_test(tc, test_double_syn_dropped_event);
	tcase_add_test(tc, test_event_type_filtered);
	tcase_add_test(tc, test_event_code_filtered);
	tcase_add_test(tc, test_has_event_pending);
	suite_add_tcase(s, tc);

	tc = tcase_create("SYN_DROPPED deltas");
	tcase_add_test(tc, test_syn_delta_button);
	tcase_add_test(tc, test_syn_delta_abs);
	tcase_add_test(tc, test_syn_delta_mt);
	tcase_add_test(tc, test_syn_delta_mt_reset_slot);
	tcase_add_test(tc, test_syn_delta_led);
	tcase_add_test(tc, test_syn_delta_sw);
	tcase_add_test(tc, test_syn_delta_fake_mt);
	tcase_add_test(tc, test_syn_delta_tracking_ids);
	tcase_add_test(tc, test_syn_delta_late_sync);
	suite_add_tcase(s, tc);

	tc = tcase_create("skipped syncs");
	tcase_add_test(tc, test_skipped_sync);
	tcase_add_test(tc, test_incomplete_sync);
	tcase_add_test(tc, test_empty_sync);
	suite_add_tcase(s, tc);

	tc = tcase_create("event values");
	tcase_add_test(tc, test_event_values);
	tcase_add_test(tc, test_event_values_invalid);
	tcase_add_test(tc, test_mt_event_values);
	tcase_add_test(tc, test_mt_event_values_invalid);
	tcase_add_test(tc, test_mt_slot_ranges_invalid);
	tcase_add_test(tc, test_mt_tracking_id_discard);
	tcase_add_test(tc, test_mt_tracking_id_discard_neg_1);
	tcase_add_test(tc, test_ev_rep_values);
	suite_add_tcase(s, tc);

	tc = tcase_create("event value setters");
	tcase_add_test(tc, test_event_value_setters);
	tcase_add_test(tc, test_event_value_setters_invalid);
	tcase_add_test(tc, test_event_mt_value_setters);
	tcase_add_test(tc, test_event_mt_value_setters_invalid);
	tcase_add_test(tc, test_event_mt_value_setters_current_slot);
	suite_add_tcase(s, tc);

	return s;
}

