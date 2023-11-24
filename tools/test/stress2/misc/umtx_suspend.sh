#!/bin/sh

# Test scenario from
# Bug 192918 - [patch] A thread will spin if a signal interrupts umtxq_sleep_pi.
# by eric@vangyzen.net

. ../default.cfg

here=`pwd`
cd /tmp
sed '1,/^EOF/d' < $here/$0 > umtx_suspend.c
mycc -o umtx_suspend -Wall -Wextra -O0 -g umtx_suspend.c \
    -lpthread || exit 1
rm -f umtx_suspend.c

/tmp/umtx_suspend

rm -f /tmp/umtx_suspend
exit 0
EOF
// cc -lpthread -o umtx_suspend umtx_suspend.c
//
// failure: a thread spins around "umtxpi" the kernel, ignoring signals
// success: the process exits

#include <sys/cdefs.h>

#include <err.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stdio.h>
#include <unistd.h>

pthread_mutex_t the_mutex;

void *
spinning_thread_func(void *arg __unused)
{
    int error;

    error = pthread_mutex_lock(&the_mutex);
    if (error)
        errc(1, error, "%s: pthread_mutex_lock", __func__);

    return (NULL);
}

int
main(int argc __unused, char *argv[] __unused)
{
    int error;
    pthread_t spinning_thread;
    pthread_mutexattr_t the_mutex_attr;

    error = pthread_mutexattr_init(&the_mutex_attr);
    if (error)
        errc(1, error, "pthread_mutexattr_init");

    error = pthread_mutexattr_setprotocol(&the_mutex_attr, PTHREAD_PRIO_INHERIT);
    if (error)
        errc(1, error, "pthread_mutexattr_setprotocol");

    error = pthread_mutex_init(&the_mutex, &the_mutex_attr);
    if (error)
        errc(1, error, "pthread_mutex_init");

    error = pthread_mutex_lock(&the_mutex);
    if (error)
        errc(1, error, "pthread_mutex_lock");

    error = pthread_create(&spinning_thread, NULL, spinning_thread_func, NULL);
    if (error)
        errc(1, error, "pthread_create");

    // Wait for the spinning_thread to call pthread_mutex_lock(3)
    // and enter the kernel.
    (void) sleep(1);

    error = pthread_suspend_np(spinning_thread);
    if (error)
        errc(1, error, "pthread_suspend_np");

    // The spinning_thread should be spinning in the kernel.
    // This thread should be blocked in pthread_suspend_np(3).
    fputs("This test failed to reproduce the bug.\n", stderr);

    return (0);
}
