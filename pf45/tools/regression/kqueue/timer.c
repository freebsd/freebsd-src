/*
 * Copyright (c) 2009 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $FreeBSD$
 */

#include "common.h"

int kqfd;

void
test_kevent_timer_add(void)
{
    const char *test_id = "kevent(EVFILT_TIMER, EV_ADD)";
    struct kevent kev;

    test_begin(test_id);

    EV_SET(&kev, 1, EVFILT_TIMER, EV_ADD, 0, 1000, NULL);
    if (kevent(kqfd, &kev, 1, NULL, 0, NULL) < 0)
        err(1, "%s", test_id);

    success();
}

void
test_kevent_timer_del(void)
{
    const char *test_id = "kevent(EVFILT_TIMER, EV_DELETE)";
    struct kevent kev;

    test_begin(test_id);

    EV_SET(&kev, 1, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
    if (kevent(kqfd, &kev, 1, NULL, 0, NULL) < 0)
        err(1, "%s", test_id);

    test_no_kevents();

    success();
}

void
test_kevent_timer_get(void)
{
    const char *test_id = "kevent(EVFILT_TIMER, wait)";
    struct kevent kev;

    test_begin(test_id);

    EV_SET(&kev, 1, EVFILT_TIMER, EV_ADD, 0, 1000, NULL);
    if (kevent(kqfd, &kev, 1, NULL, 0, NULL) < 0)
        err(1, "%s", test_id);

    kev.flags |= EV_CLEAR;
    kev.data = 1; 
    kevent_cmp(&kev, kevent_get(kqfd));

    EV_SET(&kev, 1, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
    if (kevent(kqfd, &kev, 1, NULL, 0, NULL) < 0)
        err(1, "%s", test_id);

    success();
}

static void
test_oneshot(void)
{
    const char *test_id = "kevent(EVFILT_TIMER, EV_ONESHOT)";
    struct kevent kev;

    test_begin(test_id);

    test_no_kevents();

    EV_SET(&kev, vnode_fd, EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0, 500,NULL);
    if (kevent(kqfd, &kev, 1, NULL, 0, NULL) < 0)
        err(1, "%s", test_id);

    /* Retrieve the event */
    kev.flags = EV_ADD | EV_CLEAR | EV_ONESHOT;
    kev.data = 1; 
    kevent_cmp(&kev, kevent_get(kqfd));

    /* Check if the event occurs again */
    sleep(3);
    test_no_kevents();


    success();
}

static void
test_periodic(void)
{
    const char *test_id = "kevent(EVFILT_TIMER, periodic)";
    struct kevent kev;

    test_begin(test_id);

    test_no_kevents();

    EV_SET(&kev, vnode_fd, EVFILT_TIMER, EV_ADD, 0, 1000,NULL);
    if (kevent(kqfd, &kev, 1, NULL, 0, NULL) < 0)
        err(1, "%s", test_id);

    /* Retrieve the event */
    kev.flags = EV_ADD | EV_CLEAR;
    kev.data = 1; 
    kevent_cmp(&kev, kevent_get(kqfd));

    /* Check if the event occurs again */
    sleep(1);
    kevent_cmp(&kev, kevent_get(kqfd));

    /* Delete the event */
    kev.flags = EV_DELETE;
    if (kevent(kqfd, &kev, 1, NULL, 0, NULL) < 0)
        err(1, "%s", test_id);

    success();
}

static void
disable_and_enable(void)
{
    const char *test_id = "kevent(EVFILT_TIMER, EV_DISABLE and EV_ENABLE)";
    struct kevent kev;

    test_begin(test_id);

    test_no_kevents();

    /* Add the watch and immediately disable it */
    EV_SET(&kev, vnode_fd, EVFILT_TIMER, EV_ADD | EV_ONESHOT, 0, 2000,NULL);
    if (kevent(kqfd, &kev, 1, NULL, 0, NULL) < 0)
        err(1, "%s", test_id);
    kev.flags = EV_DISABLE;
    if (kevent(kqfd, &kev, 1, NULL, 0, NULL) < 0)
        err(1, "%s", test_id);
    test_no_kevents();

    /* Re-enable and check again */
    kev.flags = EV_ENABLE;
    if (kevent(kqfd, &kev, 1, NULL, 0, NULL) < 0)
        err(1, "%s", test_id);

    kev.flags = EV_ADD | EV_CLEAR | EV_ONESHOT;
    kev.data = 1; 
    kevent_cmp(&kev, kevent_get(kqfd));

    success();
}

void
test_evfilt_timer()
{
	kqfd = kqueue();
    test_kevent_timer_add();
    test_kevent_timer_del();
    test_kevent_timer_get();
    test_oneshot();
    test_periodic();
    disable_and_enable();
	close(kqfd);
}
