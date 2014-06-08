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
#include <limits.h>
#include <libevdev/libevdev-int.h>
#include "test-common.h"

START_TEST(test_queue_alloc)
{
	struct libevdev dev;
	int rc;

	rc = queue_alloc(&dev, 0);
	ck_assert_int_eq(rc, -ENOMEM);

	rc = queue_alloc(&dev, ULONG_MAX);
	ck_assert_int_eq(rc, -ENOMEM);

	rc = queue_alloc(&dev, 100);
	ck_assert_int_eq(rc, 0);

	ck_assert_int_eq(dev.queue_size, 100);
	ck_assert_int_eq(dev.queue_next, 0);

	queue_free(&dev);
	ck_assert_int_eq(dev.queue_size, 0);
	ck_assert_int_eq(dev.queue_next, 0);

}
END_TEST

START_TEST(test_queue_sizes)
{
	struct libevdev dev = {0};

	queue_alloc(&dev, 0);
	ck_assert_int_eq(queue_num_elements(&dev), 0);
	ck_assert_int_eq(queue_num_free_elements(&dev), 0);
	ck_assert_int_eq(queue_size(&dev), 0);

	queue_alloc(&dev, 100);
	ck_assert_int_eq(queue_num_elements(&dev), 0);
	ck_assert_int_eq(queue_num_free_elements(&dev), 100);
	ck_assert_int_eq(queue_size(&dev), 100);

	queue_free(&dev);

	ck_assert_int_eq(queue_num_elements(&dev), 0);
	ck_assert_int_eq(queue_num_free_elements(&dev), 0);
	ck_assert_int_eq(queue_size(&dev), 0);
}
END_TEST

START_TEST(test_queue_push)
{
	struct libevdev dev = {0};
	struct input_event *ev;

	queue_alloc(&dev, 0);
	ev = queue_push(&dev);
	ck_assert(ev == NULL);

	queue_alloc(&dev, 2);
	ev = queue_push(&dev);
	ck_assert(ev == dev.queue);
	ck_assert_int_eq(queue_num_elements(&dev), 1);
	ck_assert_int_eq(queue_num_free_elements(&dev), 1);

	ev = queue_push(&dev);
	ck_assert(ev == dev.queue + 1);

	ev = queue_push(&dev);
	ck_assert(ev == NULL);

	queue_free(&dev);
	ev = queue_push(&dev);
	ck_assert(ev == NULL);

}
END_TEST

START_TEST(test_queue_pop)
{
	struct libevdev dev = {0};
	struct input_event ev, *e, tmp;
	int rc;

	queue_alloc(&dev, 0);
	rc = queue_pop(&dev, &ev);
	ck_assert_int_eq(rc, 1);

	queue_alloc(&dev, 2);
	e = queue_push(&dev);
	memset(e, 0xab, sizeof(*e));
	ck_assert_int_eq(queue_num_elements(&dev), 1);
	ck_assert_int_eq(queue_num_free_elements(&dev), 1);

	rc = queue_pop(&dev, &ev);
	ck_assert_int_eq(rc, 0);
	memset(&tmp, 0xab, sizeof(tmp));
	rc = memcmp(&tmp, &ev, sizeof(tmp));
	ck_assert_int_eq(rc, 0);

	ck_assert_int_eq(queue_num_elements(&dev), 0);
	ck_assert_int_eq(queue_num_free_elements(&dev), 2);

	rc = queue_pop(&dev, &ev);
	ck_assert_int_eq(rc, 1);

	queue_free(&dev);
}
END_TEST

START_TEST(test_queue_peek)
{
	struct libevdev dev = {0};
	struct input_event ev, *e, tmp;
	int rc;

	queue_alloc(&dev, 0);
	rc = queue_peek(&dev, 0, &ev);
	ck_assert_int_eq(rc, 1);

	queue_alloc(&dev, 2);
	e = queue_push(&dev);
	memset(e, 0xab, sizeof(*e));

	rc = queue_peek(&dev, 0, &ev);
	ck_assert_int_eq(rc, 0);
	memset(&tmp, 0xab, sizeof(tmp));
	rc = memcmp(&tmp, &ev, sizeof(tmp));
	ck_assert_int_eq(rc, 0);

	ck_assert_int_eq(queue_num_elements(&dev), 1);
	e = queue_push(&dev);
	memset(e, 0xbc, sizeof(*e));

	rc = queue_peek(&dev, 1, &ev);
	ck_assert_int_eq(rc, 0);
	memset(&tmp, 0xbc, sizeof(tmp));
	rc = memcmp(&tmp, &ev, sizeof(tmp));
	ck_assert_int_eq(rc, 0);

	rc = queue_peek(&dev, 0, &ev);
	ck_assert_int_eq(rc, 0);
	memset(&tmp, 0xab, sizeof(tmp));
	rc = memcmp(&tmp, &ev, sizeof(tmp));
	ck_assert_int_eq(rc, 0);

	ck_assert_int_eq(queue_num_elements(&dev), 2);

	queue_free(&dev);
}
END_TEST

START_TEST(test_queue_shift)
{
	struct libevdev dev = {0};
	struct input_event ev, *first, *second, e1, e2;
	int rc;

	ck_assert_int_eq(queue_shift(&dev, &ev), 1);

	queue_alloc(&dev, 10);
	ck_assert_int_eq(queue_shift(&dev, &ev), 1);

	first = queue_push(&dev);
	ck_assert(first != NULL);
	memset(first, 0xab, sizeof(*first));

	e1 = *first;

	second = queue_push(&dev);
	ck_assert(second != NULL);
	memset(second, 0x12, sizeof(*second));

	e2 = *second;

	rc = queue_shift(&dev, &ev);
	ck_assert_int_eq(rc, 0);
	rc = memcmp(&ev, &e1, sizeof(ev));
	ck_assert_int_eq(rc, 0);

	rc = queue_shift(&dev, &ev);
	ck_assert_int_eq(rc, 0);
	rc = memcmp(&ev, &e2, sizeof(ev));
	ck_assert_int_eq(rc, 0);

	ck_assert_int_eq(queue_shift(&dev, &ev), 1);

	queue_free(&dev);
}
END_TEST

START_TEST(test_queue_shift_multiple)
{
	struct libevdev dev = {0};
	struct input_event ev, *first, *second, e1, e2;
	struct input_event events[5];
	int rc;

	ck_assert_int_eq(queue_shift_multiple(&dev, 1, &ev), 0);
	ck_assert_int_eq(queue_shift_multiple(&dev, 0, &ev), 0);

	queue_alloc(&dev, 10);
	ck_assert_int_eq(queue_shift_multiple(&dev, 1, &ev), 0);
	ck_assert_int_eq(queue_shift_multiple(&dev, 0, &ev), 0);

	first = queue_push(&dev);
	ck_assert(first != NULL);
	memset(first, 0xab, sizeof(*first));
	e1 = *first;

	second = queue_push(&dev);
	ck_assert(second != NULL);
	memset(second, 0x12, sizeof(*second));
	e2 = *second;

	rc = queue_shift_multiple(&dev, 5, events);
	ck_assert_int_eq(rc, 2);
	rc = memcmp(&events[0], &e1, sizeof(ev));
	ck_assert_int_eq(rc, 0);
	rc = memcmp(&events[1], &e2, sizeof(ev));
	ck_assert_int_eq(rc, 0);

	first = queue_push(&dev);
	ck_assert(first != NULL);
	memset(first, 0xab, sizeof(*first));
	e1 = *first;

	second = queue_push(&dev);
	ck_assert(second != NULL);
	memset(second, 0x12, sizeof(*second));
	e2 = *second;

	rc = queue_shift_multiple(&dev, 1, events);
	ck_assert_int_eq(rc, 1);
	rc = memcmp(&events[0], &e1, sizeof(ev));
	ck_assert_int_eq(rc, 0);

	rc = queue_shift_multiple(&dev, 1, events);
	ck_assert_int_eq(rc, 1);
	rc = memcmp(&events[0], &e2, sizeof(ev));
	ck_assert_int_eq(rc, 0);

	ck_assert_int_eq(queue_shift_multiple(&dev, 1, events), 0);

	queue_free(&dev);
}
END_TEST

START_TEST(test_queue_next_element)
{
	struct libevdev dev = {0};
	struct input_event ev, *first, *second;
	int rc;

	queue_alloc(&dev, 0);
	first = queue_next_element(&dev);
	ck_assert(first == NULL);

	queue_alloc(&dev, 2);
	first = queue_next_element(&dev);
	ck_assert(first != NULL);
	memset(first, 0xab, sizeof(*first));

	second = queue_next_element(&dev);
	ck_assert(second != NULL);
	memset(second, 0xbc, sizeof(*second));

	/* queue_next_element does not advance, so we overwrite */
	memset(&ev, 0xbc, sizeof(ev));
	rc = memcmp(&ev, first, sizeof(ev));
	ck_assert_int_eq(rc, 0);

	ck_assert_int_eq(queue_num_elements(&dev), 0);

	first = queue_next_element(&dev);
	ck_assert(first != NULL);
	memset(first, 0xab, sizeof(*first));

	queue_set_num_elements(&dev, 1);
	ck_assert_int_eq(queue_num_elements(&dev), 1);

	second = queue_next_element(&dev);
	ck_assert(second != NULL);
	memset(second, 0xbc, sizeof(*second));

	memset(&ev, 0xab, sizeof(ev));
	rc = memcmp(&ev, first, sizeof(ev));
	ck_assert_int_eq(rc, 0);

	queue_free(&dev);
}
END_TEST

START_TEST(test_queue_set_num_elements)
{
	struct libevdev dev = {0};

	queue_alloc(&dev, 0);
	ck_assert_int_eq(queue_set_num_elements(&dev, 1), 1);

	queue_alloc(&dev, 2);
	ck_assert_int_eq(queue_set_num_elements(&dev, 3), 1);
	ck_assert_int_eq(queue_set_num_elements(&dev, 2), 0);

	queue_free(&dev);
}
END_TEST

Suite *
queue_suite(void)
{
	Suite *s = suite_create("Event queue");

	TCase *tc = tcase_create("Queue allocation");
	tcase_add_test(tc, test_queue_alloc);
	tcase_add_test(tc, test_queue_sizes);
	suite_add_tcase(s, tc);

	tc = tcase_create("Queue push/pop/peek");
	tcase_add_test(tc, test_queue_push);
	tcase_add_test(tc, test_queue_pop);
	tcase_add_test(tc, test_queue_peek);
	suite_add_tcase(s, tc);

	tc = tcase_create("Queue shift");
	tcase_add_test(tc, test_queue_shift);
	tcase_add_test(tc, test_queue_shift_multiple);
	suite_add_tcase(s, tc);

	tc = tcase_create("Queue next elem");
	tcase_add_test(tc, test_queue_next_element);
	tcase_add_test(tc, test_queue_set_num_elements);
	suite_add_tcase(s, tc);

	return s;
}

